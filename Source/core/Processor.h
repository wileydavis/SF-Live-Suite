#pragma once
// SFProcessor: hosts one sf::Module. Owns parameters (Live-visible), smoothing, SLOW (glide),
// MACRO response, MOTION (two song-locked modulators), song slots (recalled exactly on the bar),
// presets, state save/load, PANIC and the output safety stage.
//
// Every host block is split into chunks of at most kChunk samples: parameters, MOTION, song
// recall and every module's control-rate work run per chunk, so the sound is identical at any
// Live buffer size (64 .. 2048) and no module ever sees a block larger than it prepared for.
#include "Bus.h"
#include "Dsp.h"
#include "Module.h"
#include "Motion.h"
#include <array>
#include <atomic>

class SFProcessor : public juce::AudioProcessor, private juce::Timer, private juce::AudioProcessorParameter::Listener
{
    // Declared first: the parameter layout is built from these.
    sf::ModulePtr module;
    std::vector<int> motionTargets;         // module parameter index for MOTION TARGET choice k+1
    std::vector<sf::ParamSpec> specs;
    int numModParams = 0;
    int numExtraOut = 0;                    // extra stereo outputs (DECK CUE); read once, never on the audio thread

public:
    static constexpr int kChunk = 64;
    static constexpr int kMaxParams = 128;
    static constexpr int kMotions = 2;

    explicit SFProcessor (sf::ModulePtr);
    ~SFProcessor() override;

    // System parameters appended after the module's own (order fixed: saved sets depend on ids).
    enum { SYS_GLIDE = 0, SYS_MACRO_SRC, SYS_MACRO_AMT,
           SYS_M1_SRC, SYS_M1_RATE, SYS_M1_TARGET, SYS_M1_DEPTH,
           SYS_M2_SRC, SYS_M2_RATE, SYS_M2_TARGET, SYS_M2_DEPTH, kNumSys };
    // SLOW: CONTROL (follow SF CONTROL's SLOW ALL; it was OFF before SLOW ALL, and sets saved then
    // keep their old sound while SLOW ALL is OFF), this plugin's own length, or OFF (appended, so
    // saved sets keep their lengths).
    enum { SLOW_CONTROL = 0, SLOW_OFF = 8 };
    static std::vector<std::string> glideNames() { return { "CONTROL", "1 BAR", "2 BAR", "4 BAR", "8 BAR", "16 BAR", "32 BAR", "64 BAR", "OFF" }; }
    // Bars for a SLOW ALL index (0 = OFF) or an own SLOW length (1..7).
    static double glideBars (int i) { static const double b[] = { 0, 1, 2, 4, 8, 16, 32, 64 }; return b[juce::jlimit (0, 7, i)]; }

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "SF " + juce::String (module->id()); }
    bool acceptsMidi() const override { return module->instrument(); }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return module->tailSeconds(); }

    // Programs: SF CONTROL exposes 16 (VST3 delivers Program Change only as a program change).
    int getNumPrograms() override { return std::max (1, module->numPrograms()); }
    int getCurrentProgram() override { return module->currentProgram(); }
    void setCurrentProgram (int i) override { module->setProgram (i); }
    const juce::String getProgramName (int i) override { return module->numPrograms() > 1 ? juce::String ("SONG ") + juce::String (i + 1).paddedLeft ('0', 2) : juce::String(); }
    void changeProgramName (int, const juce::String&) override {}

    void updateTrackProperties (const TrackProperties&) override;
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // Editor / tests
    sf::Module& mod() { return *module; }
    const std::vector<sf::ParamSpec>& allSpecs() const { return specs; }
    int numModuleParams() const { return numModParams; }
    juce::RangedAudioParameter* param (int i) { return params[(size_t) i]; }
    int paramIndex (const std::string& id) const;
    float currentValue (int i) const { return cur[(size_t) i]; }     // what the module hears (glide, macro, motion)
    float baseValue (int i) const { return base[(size_t) i]; }       // after SLOW, before MACRO / MOTION
    bool isModulated (int i) const { return std::abs (cur[(size_t) i] - base[(size_t) i]) > 1e-4f * (1.0f + std::abs (base[(size_t) i])); }
    float motionValue (int k) const { return motionOut[(size_t) k].load(); }
    void storeSong();                       // store current values into the active song slot
    int activeSong() const { return songIndex.load(); }
    bool songStored (int s) const { return s >= 0 && s < sf::kSongs && songHas[(size_t) s].load(); }
    bool recallWaiting() const { return pendingSong.load() >= 0; }
    void flushHostNotifications();          // message thread (timer); public for tests

    // Presets (message thread).
    const std::vector<sf::Preset>& factoryPresets() const { return factory; }
    void applyPreset (const sf::Preset&);
    sf::Preset capturePreset (const juce::String& name) const;
    juce::File userPresetFolder() const;
    juce::Array<juce::File> userPresets() const;
    bool saveUserPreset (const juce::String& name);
    bool loadUserPreset (const juce::File&);
    juce::String presetName() const { return currentPreset; }

    // Panic: silence this plugin now (voices, tails, delay lines). SF CONTROL PANIC does it to all.
    void panic() { localPanic = true; }

    sf::MeterState outMeter;
    std::atomic<bool> clipped { false };
    std::atomic<bool> fault { false };      // NaN / runaway: the module was reset (header shows FAULT)
    std::atomic<int> faultCount { 0 };

    juce::AudioProcessorValueTreeState apvts;

private:
    static BusesProperties busesFor (const sf::Module&);
    juce::AudioProcessorValueTreeState::ParameterLayout makeLayout();
    void timerCallback() override;
    void parameterValueChanged (int index, float) override;
    void parameterGestureChanged (int, bool) override {}
    bool movedSinceRecall (size_t i) const;
    void applySong (int s);
    void updateParams (const sf::Ctx&, int n, float motionInput);
    void processChunk (juce::AudioBuffer<float>& buffer, juce::AudioBuffer<float>& mainOut, juce::AudioBuffer<float>* sideAll,
                       int start, int n, sf::Ctx& c, const juce::MidiBuffer& midi, bool panicNow, bool panicHold, bool lastChunk);

    std::vector<juce::RangedAudioParameter*> params;
    std::vector<std::atomic<float>*> raw;
    std::vector<float> cur, prev, base, macroOff, glideFrom, glideTo, modAmt;
    std::unique_ptr<std::atomic<float>[]> ovr;     // read by the message thread (host notification)
    // A recall's value holds until Live reports it back, unless Live's value is moved after the
    // recall (the performer, automation): extGen counts every change that isn't this plugin's own
    // notification; ovrGen / ovrFrom are the count and Live's value at the recall.
    // recallGen counts recalls per parameter, reportedGen is the last one reported to Live: the
    // 3 s give-up runs only once Live has been told (a message thread busy saving the set mustn't
    // undo a recall).
    std::unique_ptr<std::atomic<float>[]> ovrFrom;
    std::unique_ptr<std::atomic<uint32_t>[]> extGen, ovrGen, recallGen, reportedGen;
    std::atomic<int> notifying { -1 };            // the parameter flushHostNotifications is writing
    // SLOW: a move's length (beats) is fixed when it is made; 0 = not travelling.
    std::vector<double> glideElapsed, glideBeats;
    int slowAllNow = 0;                           // SF CONTROL's SLOW ALL this block (0 while CONTROL isn't running)
    std::vector<int> ovrAge;
    double internalPpq = 0.0, barPhase = 0.0;
    char trackName[24] {};
    std::atomic<uint32_t> trackNameSeq { 0 };
    std::atomic<uint64_t> instanceUid { 1 };   // saved with the set; a duplicated track's copy takes a new one (AllocTap)
    bool clockAnchored = false;    // ovr: recalled value in force until Live reports it back (NaN = none)
    bool firstBlock = true;
    double sr = 48000.0;

    // MOTION
    std::array<sf::Motion, kMotions> motion;
    std::array<std::atomic<float>, kMotions> motionOut {};
    uint32_t motionSeed = 1;
    float noteVel = 0.0f;

    // Songs
    std::array<std::array<float, kMaxParams>, sf::kSongs> songVals {};
    std::array<std::atomic<bool>, sf::kSongs> songHas {};
    std::atomic<int> songIndex { -1 }, pendingSong { -1 };
    uint32_t lastSongSeq = 0, lastStoreSeq = 0, lastPanicSeq = 0;
    bool seqInit = false;
    std::atomic<bool> notifyHost { false }, storeRequested { false }, localPanic { false };

    // Safety
    int overSamples = 0;
    // PANIC: output gain (fades 5 ms), a press still to complete (fade out, then reset).
    int panicPos = 240, panicInPos = 240;   // PANIC output / input gain, in samples of the 5 ms ramp
    bool panicDown = false;
    int panicLen = 240;
    double pendingAt = -1.0;               // ppq of the bar line the pending song lands on (-1 = own)
    bool pendingJumped = false;            // the pending song already lost its bar line to a jump back
    bool recalledInBlock = false;          // a recall landed in this host block (SF CONTROL's STORE ALL waits)
    bool lastPlaying = false;              // the transport was playing in the last block
    int pendingKey = -2;                   // the KEY the pending song brings (-2 = no change)
    int keyUsed = -1, keyHold = 0;         // the key in force here; samples it holds against the bus
    uint64_t streamPos = 0;

    // Presets
    std::vector<sf::Preset> factory;
    juce::String currentPreset;

    double lastBpm = 120.0;
    juce::AudioBuffer<float> extraScratch, mainView, sideView, extraView;   // views: never allocate
    juce::MidiBuffer chunkMidi;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SFProcessor)
};
