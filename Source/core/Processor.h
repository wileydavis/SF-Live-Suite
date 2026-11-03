#pragma once
// SFProcessor: hosts one sf::Module. Owns parameters (Live-visible), smoothing, GLIDE (slow mode),
// MACRO response, song slots (recalled on the bar), state save/load and the output safety stage.
#include "Bus.h"
#include "Dsp.h"
#include "Module.h"
#include <array>
#include <atomic>

class SFProcessor : public juce::AudioProcessor, private juce::Timer
{
    // Declared first: the parameter layout is built from these.
    sf::ModulePtr module;
    std::vector<sf::ParamSpec> specs;
    int numModParams = 0;

public:
    explicit SFProcessor (sf::ModulePtr);
    ~SFProcessor() override;

    // System parameters appended after the module's own.
    enum { SYS_GLIDE = 0, SYS_MACRO_SRC, SYS_MACRO_AMT, kNumSys };
    static std::vector<std::string> glideNames() { return { "OFF", "1 BAR", "2 BAR", "4 BAR", "8 BAR", "16 BAR", "32 BAR", "64 BAR" }; }
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

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // Editor / tests
    sf::Module& mod() { return *module; }
    const std::vector<sf::ParamSpec>& allSpecs() const { return specs; }
    int numModuleParams() const { return numModParams; }
    juce::RangedAudioParameter* param (int i) { return params[(size_t) i]; }
    float currentValue (int i) const { return cur[(size_t) i]; }
    void storeSong();                       // store current values into the active song slot
    int activeSong() const { return songIndex.load(); }
    bool songStored (int s) const { return s >= 0 && s < sf::kSongs && songHas[(size_t) s].load(); }
    bool recallWaiting() const { return pendingSong.load() >= 0; }
    void flushHostNotifications();          // message thread (timer); public for tests

    sf::MeterState outMeter;
    std::atomic<bool> clipped { false };

    juce::AudioProcessorValueTreeState apvts;

private:
    static BusesProperties busesFor (const sf::Module&);
    juce::AudioProcessorValueTreeState::ParameterLayout makeLayout();
    void timerCallback() override;
    void applySong (int s);

    std::vector<juce::RangedAudioParameter*> params;
    std::vector<std::atomic<float>*> raw;
    std::vector<float> cur, prev, ovr, base, macroOff, glideFrom, glideTo;
    std::vector<double> glideElapsed;
    double internalPpq = 0.0;
    bool clockAnchored = false;    // ovr: recalled value in force until Live reports it back (NaN = none)
    std::vector<int> ovrAge;
    bool firstBlock = true;
    double sr = 48000.0;

    // Songs
    std::array<std::array<float, 96>, sf::kSongs> songVals {};
    std::array<std::atomic<bool>, sf::kSongs> songHas {};
    std::atomic<int> songIndex { -1 }, pendingSong { -1 };
    uint32_t lastSongSeq = 0, lastStoreSeq = 0;
    bool seqInit = false;
    std::atomic<bool> notifyHost { false }, storeRequested { false };

    // Internal clock when Live is stopped (shared wall clock, so all SF plugins agree).
    double lastBpm = 120.0;
    juce::AudioBuffer<float> extraScratch, sideScratch;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SFProcessor)
};
