// SFTests: offline checks for every module.
//  1. Every module: 4 bars of audio/MIDI with every parameter swept to random values each beat,
//     including extremes. Fails on NaN/Inf, on hitting the +12 dBFS safety clamp, or (instruments)
//     on silence at default settings.
//  2. State round trip (save -> load -> identical parameter values).
//  3. Bus: KICK reports reach DUCK (pocket), RUMBLE triggers from KICK.
//  4. ALLOC: two TAPs fighting at 180 Hz -> conflict slot bound, lower priority source cut.
//  5. Songs: CONTROL STORE ALL + Program Change recalls FILTER on the bar.
//  6. CPU: real-time factor per module.
#include <juce_audio_formats/juce_audio_formats.h>
#include "core/Bus.h"
#include "core/Dsp.h"
#include "core/Pocket.h"
#include "core/Processor.h"
#include "core/Registry.h"
#include <chrono>
#include <cstdio>
#include <random>

using namespace sf;

struct FakeHead : juce::AudioPlayHead
{
    double bpm = 124.0, ppq = 0.0;
    bool playing = true;
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p;
        p.setBpm (bpm);
        p.setPpqPosition (ppq);
        p.setIsPlaying (playing);
        p.setTimeSignature (TimeSignature { 4, 4 });
        return p;
    }
};

static constexpr double kSr = 48000.0;
static constexpr int kBlock = 256;
static int failures = 0;
#define CHECK(cond, ...) do { if (! (cond)) { ++failures; std::printf ("  FAIL: "); std::printf (__VA_ARGS__); std::printf ("\n"); } } while (0)

static void resetBus()
{
    auto& d = Bus::get().d();
    d.kickWrite = 0; d.kickSeenMs = 0; d.allocSeenMs = 0; d.controlSeenMs = 0; d.song = -1;
    for (auto& s : d.src) { s.owner = 0; s.seenMs = 0; }
    for (auto& m : d.macro) m = 0.0f;
}

struct Rig
{
    std::unique_ptr<SFProcessor> p;
    juce::AudioBuffer<float> buf;
    juce::MidiBuffer midi;
    FakeHead* head = nullptr;
    explicit Rig (const std::string& id, FakeHead& h)
    {
        p = std::make_unique<SFProcessor> (makeModule (id));
        p->setPlayHead (&h);
        head = &h;
        p->enableAllBuses();
        p->prepareToPlay (kSr, kBlock);
        buf.setSize (std::max (p->getTotalNumInputChannels(), p->getTotalNumOutputChannels()), kBlock);
    }
    void set (const char* pid, float v)
    {
        auto* prm = p->apvts.getParameter (pid);
        if (prm != nullptr) prm->setValueNotifyingHost (prm->convertTo0to1 (v));
        else { ++failures; std::printf ("  FAIL: no param %s\n", pid); }
    }
    void run (const std::function<void (juce::AudioBuffer<float>&, juce::MidiBuffer&, int)>& fill, int block)
    {
        buf.clear();
        midi.clear();
        fill (buf, midi, block);
        p->processBlock (buf, midi);
    }
};

// Test input: kick-ish pulses + chord + noise, stereo.
static void fillAudio (juce::AudioBuffer<float>& b, double ppq0, double bpm, int nch)
{
    const double bps = bpm / 60.0 / kSr;
    for (int i = 0; i < b.getNumSamples(); ++i)
    {
        const double q = ppq0 + i * bps;
        const double beatPh = q - std::floor (q);
        const float t = (float) (beatPh * 60.0 / bpm);
        const float kick = std::sin (6.2831853f * 55.0f * t) * std::exp (-t / 0.15f) * 0.6f;
        const float chord = 0.08f * (std::sin (6.2831853f * 220.0f * (float) (q * 60.0 / bpm)) + std::sin (6.2831853f * 277.18f * (float) (q * 60.0 / bpm)));
        const float hat = ((int) std::floor (q * 2.0) % 2 == 1 && beatPh - 0.5 < 0.05) ? 0.1f * ((float) std::rand() / RAND_MAX - 0.5f) : 0.0f;
        for (int c = 0; c < std::min (2, nch); ++c) b.setSample (c, i, kick + chord + hat);
    }
}

static void fillMidi (juce::MidiBuffer& m, double ppq0, double bpm, int n)
{
    // 1/8 notes, alternating notes from a minor chord, held 1/16.
    const double bps = bpm / 60.0 / kSr;
    static const int notes[] = { 36, 48, 51, 55, 60, 63, 67, 43 };
    for (int i = 0; i < n; ++i)
    {
        const double q = ppq0 + i * bps, prev = q - bps;
        const double e = std::floor (q * 2.0), ep = std::floor (prev * 2.0);
        if (e != ep) m.addEvent (juce::MidiMessage::noteOn (1, notes[(int) e % 8], (juce::uint8) (90 + ((int) e % 3) * 15)), i);
        const double off = std::floor ((q - 0.25) * 2.0), offp = std::floor ((prev - 0.25) * 2.0);
        if (off != offp && q > 0.25) m.addEvent (juce::MidiMessage::noteOff (1, notes[(int) off % 8]), i);
    }
}

static void testModule (const std::string& id)
{
    resetBus();
    FakeHead head;
    Rig r (id, head);
    const bool inst = r.p->mod().instrument();
    std::mt19937 rng (1234);
    const int blocksPerBeat = (int) (kSr * 60.0 / head.bpm / kBlock);
    const int beats = 16;
    float peakDefault = 0.0f, peakAll = 0.0f, peakSweep = 0.0f;
    bool clamp = false;
    double ppq = 0.0;
    const auto& specs = r.p->allSpecs();
    auto t0 = std::chrono::high_resolution_clock::now();
    int totalSamples = 0;
    for (int beat = 0; beat < beats; ++beat)
    {
        if (beat >= 8)
        {
            // Sweep: every parameter random (extremes 20 % of the time).
            for (size_t k = 0; k < specs.size(); ++k)
            {
                auto* prm = r.p->param ((int) k);
                if (specs[k].type == PType::Momentary) { prm->setValueNotifyingHost ((rng() % 4 == 0) ? 1.0f : 0.0f); continue; }
                if (specs[k].id == "slow" || specs[k].id == "level") continue;
                const int mode = (int) (rng() % 10);
                const float v = mode == 0 ? 0.0f : mode == 1 ? 1.0f : (float) (rng() % 1000) / 999.0f;
                prm->setValueNotifyingHost (v);
            }
        }
        for (int b = 0; b < blocksPerBeat; ++b)
        {
            head.ppq = ppq;
            r.run ([&] (juce::AudioBuffer<float>& buf, juce::MidiBuffer& m, int)
                   {
                       if (! inst) fillAudio (buf, ppq, head.bpm, r.p->getTotalNumInputChannels());
                       else fillMidi (m, ppq, head.bpm, kBlock);
                   }, b);
            ppq += kBlock * head.bpm / 60.0 / kSr;
            totalSamples += kBlock;
            for (int c = 0; c < 2; ++c)
            {
                const float pk = r.buf.getMagnitude (c, 0, kBlock);
                if (beat < 8 && beat >= 1) peakDefault = std::max (peakDefault, pk);
                peakAll = std::max (peakAll, pk);
                if (beat >= 8) peakSweep = std::max (peakSweep, pk);
                if (pk >= 3.999f) clamp = true;
                for (int i = 0; i < kBlock; ++i) if (! std::isfinite (r.buf.getSample (c, i))) clamp = true;
            }
        }
    }
    const double secs = std::chrono::duration<double> (std::chrono::high_resolution_clock::now() - t0).count();
    const double rtf = (totalSamples / kSr) / secs;
    std::printf ("%-9s default peak %6.1f dB  sweep peak %6.1f dB  latency %4d  speed %6.0fx realtime\n", id.c_str(),
                 gainToDb (peakDefault), gainToDb (peakSweep), r.p->getLatencySamples(), rtf);
    CHECK (! clamp, "%s hit the safety clamp / produced NaN", id.c_str());
    CHECK (peakDefault <= 1.5f, "%s default output too hot (%.1f dB)", id.c_str(), gainToDb (peakDefault));
    const bool silentOk = id == "CONTROL" || id == "DECK" || id == "RUMBLE" || id == "VOICE";
    if (inst && ! silentOk) CHECK (peakDefault > 0.01f, "%s silent at default settings", id.c_str());
    if (! inst && id != "ALLOC") CHECK (peakDefault > 0.01f, "%s silent at default settings", id.c_str());

    // State round trip.
    juce::MemoryBlock mb;
    r.p->getStateInformation (mb);
    SFProcessor q (makeModule (id));
    q.setStateInformation (mb.getData(), (int) mb.getSize());
    for (size_t k = 0; k < specs.size(); ++k)
    {
        const float a = q.param ((int) k)->convertFrom0to1 (q.param ((int) k)->getValue());
        const float b = r.p->param ((int) k)->convertFrom0to1 (r.p->param ((int) k)->getValue());
        const bool same = specs[k].type == PType::Float ? std::abs (a - b) < 1e-4f * (1.0f + std::abs (b)) : (a > 0.5f) == (b > 0.5f) || std::lround (a) == std::lround (b);
        CHECK (same, "%s state round trip lost %s", id.c_str(), specs[k].id.c_str());
    }
    CHECK (r.p->getNumParameters() <= 64, "%s has %d parameters (Live auto-lists 64)", id.c_str(), r.p->getNumParameters());
}

static void testPocket()
{
    std::printf ("-- pocket: KICK -> DUCK / RUMBLE\n");
    resetBus();
    FakeHead head;
    Rig kick ("KICK", head), duck ("DUCK", head), rumble ("RUMBLE", head);
    duck.set ("depth", 1.0f); duck.set ("shape", 0.7f);
    double ppq = 0.0;
    float minG = 1.0f, maxG = 0.0f, rumblePeak = 0.0f;
    const int blocks = (int) (kSr * 8.0 / kBlock);
    for (int b = 0; b < blocks; ++b)
    {
        head.ppq = ppq;
        kick.run ([&] (juce::AudioBuffer<float>&, juce::MidiBuffer& m, int)
        {
            const double bps = head.bpm / 60.0 / kSr;
            for (int i = 0; i < kBlock; ++i) if (std::floor (ppq + i * bps) != std::floor (ppq + (i - 1) * bps)) m.addEvent (juce::MidiMessage::noteOn (1, 36, (juce::uint8) 120), i);
        }, b);
        duck.run ([&] (juce::AudioBuffer<float>& buf, juce::MidiBuffer&, int)
        {
            for (int i = 0; i < kBlock; ++i) for (int c = 0; c < 2; ++c) buf.setSample (c, i, 0.5f * std::sin (6.2831853f * 70.0f * (float) ((b * kBlock + i) / kSr)));
        }, b);
        rumble.run ([] (juce::AudioBuffer<float>&, juce::MidiBuffer&, int) {}, b);
        if (b > blocks / 2)
        {
            const float g = duck.buf.getRMSLevel (0, 0, kBlock) / (0.5f / std::sqrt (2.0f));
            minG = std::min (minG, g); maxG = std::max (maxG, g);
            rumblePeak = std::max (rumblePeak, rumble.buf.getMagnitude (0, 0, kBlock));
        }
        ppq += kBlock * head.bpm / 60.0 / kSr;
    }
    std::printf ("   duck gain range %.2f .. %.2f, rumble peak %.1f dB\n", minG, maxG, gainToDb (rumblePeak));
    CHECK (minG < 0.4f && maxG > 0.75f, "DUCK did not follow KICK reports (%.2f..%.2f)", minG, maxG);
    CHECK (rumblePeak > 0.01f, "RUMBLE did not trigger from KICK");
}

static void testAlloc()
{
    std::printf ("-- alloc: two sources at 180 Hz\n");
    resetBus();
    FakeHead head;
    Rig a ("TAP", head), b ("TAP", head), alloc ("ALLOC", head);
    a.set ("role", (float) ROLE_BASS); b.set ("role", (float) ROLE_PAD);
    double ppq = 0.0;
    const int blocks = (int) (kSr * 4.0 / kBlock);
    for (int k = 0; k < blocks; ++k)
    {
        head.ppq = ppq;
        auto sine = [&] (juce::AudioBuffer<float>& buf, juce::MidiBuffer&, int)
        {
            for (int i = 0; i < kBlock; ++i) for (int c = 0; c < 2; ++c) buf.setSample (c, i, 0.3f * std::sin (6.2831853f * 180.0f * (float) ((k * kBlock + i) / kSr)));
        };
        a.run (sine, k); b.run (sine, k);
        alloc.run ([] (juce::AudioBuffer<float>&, juce::MidiBuffer&, int) {}, k);
        ppq += kBlock * head.bpm / 60.0 / kSr;
    }
    auto& d = Bus::get().d();
    float cutA = 0, cutB = 0;
    for (int s = 0; s < kSources; ++s)
    {
        if (! Bus::get().sourceAlive (s)) continue;
        const float c3 = d.src[s].cut[3].load();
        if (d.src[s].role.load() == ROLE_BASS) cutA = c3; else cutB = c3;
    }
    std::printf ("   slot1 active %d, cut BASS %.1f dB, cut PAD %.1f dB\n", (int) d.conflicts[0].active.load(), cutA, cutB);
    CHECK (d.conflicts[0].active.load() != 0, "ALLOC did not bind the conflict to SLOT 1");
    CHECK (cutB > 3.0f && cutA < 0.5f, "ALLOC priority wrong (BASS %.1f, PAD %.1f)", cutA, cutB);
    // Turn SLOT 1 fully right: ownership moves to the PAD.
    alloc.set ("slot1", 1.0f);
    for (int k = 0; k < (int) (kSr / kBlock); ++k)
    {
        head.ppq = ppq;
        auto sine = [&] (juce::AudioBuffer<float>& buf, juce::MidiBuffer&, int)
        { for (int i = 0; i < kBlock; ++i) for (int c = 0; c < 2; ++c) buf.setSample (c, i, 0.3f * std::sin (6.2831853f * 180.0f * (float) ((k * kBlock + i) / kSr))); };
        a.run (sine, k); b.run (sine, k);
        alloc.run ([] (juce::AudioBuffer<float>&, juce::MidiBuffer&, int) {}, k);
        ppq += kBlock * head.bpm / 60.0 / kSr;
    }
    for (int s = 0; s < kSources; ++s)
    {
        if (! Bus::get().sourceAlive (s)) continue;
        const float c3 = d.src[s].cut[3].load();
        if (d.src[s].role.load() == ROLE_BASS) cutA = c3; else cutB = c3;
    }
    std::printf ("   slot1 -> 100%%: cut BASS %.1f dB, cut PAD %.1f dB\n", cutA, cutB);
    CHECK (cutA > 3.0f && cutB < 0.5f, "SLOT knob did not hand the band to the other source");
    alloc.set ("slot1", 0.0f);
    for (int k = 0; k < (int) (kSr / kBlock); ++k)
    {
        head.ppq = ppq;
        auto sine = [&] (juce::AudioBuffer<float>& buf, juce::MidiBuffer&, int)
        { for (int i = 0; i < kBlock; ++i) for (int c = 0; c < 2; ++c) buf.setSample (c, i, 0.3f * std::sin (6.2831853f * 180.0f * (float) ((k * kBlock + i) / kSr))); };
        a.run (sine, k); b.run (sine, k);
        alloc.run ([] (juce::AudioBuffer<float>&, juce::MidiBuffer&, int) {}, k);
        ppq += kBlock * head.bpm / 60.0 / kSr;
    }
    float maxCut = 0.0f;
    for (int s = 0; s < kSources; ++s) if (Bus::get().sourceAlive (s)) maxCut = std::max (maxCut, d.src[s].cut[3].load());
    std::printf ("   slot1 -> 0%% (SHARE): largest cut %.1f dB\n", maxCut);
    CHECK (maxCut < 0.5f, "SHARE still cuts (%.1f dB)", maxCut);
}

static void testSongs()
{
    std::printf ("-- songs: STORE ALL + program change\n");
    resetBus();
    FakeHead head;
    Rig ctl ("CONTROL", head), filt ("FILTER", head);
    double ppq = 0.0;
    auto step = [&] (int blocks, std::function<void (juce::MidiBuffer&)> midiFn = {})
    {
        for (int k = 0; k < blocks; ++k)
        {
            head.ppq = ppq;
            ctl.run ([&] (juce::AudioBuffer<float>&, juce::MidiBuffer& m, int) { if (midiFn && k == 0) midiFn (m); }, k);
            filt.run ([] (juce::AudioBuffer<float>&, juce::MidiBuffer&, int) {}, k);
            filt.p->flushHostNotifications();
            ppq += kBlock * head.bpm / 60.0 / kSr;
        }
    };
    step (4);
    filt.set ("filter", 0.2f);
    ctl.set ("song", 2.0f); step (40);              // select song 3 (lands on a bar)
    ctl.set ("store_all", 1.0f); step (2); ctl.set ("store_all", 0.0f); step (2);
    filt.set ("filter", 0.8f);
    ctl.set ("song", 0.0f); step (100);              // song 1 (empty: no change)
    step (2, [] (juce::MidiBuffer& m) { m.addEvent (juce::MidiMessage::programChange (16, 2), 0); });
    step (500);                                       // wait for the bar
    const float v = filt.p->apvts.getRawParameterValue ("filter")->load();
    std::printf ("   FILTER after recall: %.2f (stored 0.20)\n", v);
    CHECK (std::abs (v - 0.2f) < 0.01f, "song recall failed (%.2f)", v);
}


// Every model of every instrument at default settings, with and without a sample loaded.
static void testModels()
{
    std::printf ("-- models\n");
    // Synthetic sample: 1 s pitched pluck (A2) with three onsets.
    juce::AudioBuffer<float> smp (2, 48000);
    for (int i = 0; i < 48000; ++i)
    {
        const float t = (float) (i % 16000) / 48000.0f;
        const float v = std::sin (6.2831853f * 110.0f * (float) i / 48000.0f) * std::exp (-t * 6.0f) * 0.8f;
        smp.setSample (0, i, v); smp.setSample (1, i, v);
    }
    juce::File wav = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("sf_test_pluck.wav");
    {
        wav.deleteFile();
        juce::WavAudioFormat fmt;
        std::unique_ptr<juce::AudioFormatWriter> w (fmt.createWriterFor (new juce::FileOutputStream (wav), 48000.0, 2, 24, {}, 0));
        w->writeFromAudioSampleBuffer (smp, 0, smp.getNumSamples());
    }
    for (auto id : { "KICK", "PERC", "RUMBLE", "BASS", "LEAD", "PAD", "VOICE" })
    {
        for (int withSample = 0; withSample < 2; ++withSample)
        {
            resetBus();
            FakeHead head;
            Rig r (id, head);
            if (withSample) r.p->mod().loadFiles (juce::StringArray (wav.getFullPathName()));
            auto* model = r.p->apvts.getParameter ("model");
            const int numModels = model ? (int) dynamic_cast<juce::AudioParameterChoice*> (model)->choices.size() : 1;
            juce::String line;
            for (int m = 0; m < numModels; ++m)
            {
                if (model) model->setValueNotifyingHost (model->convertTo0to1 ((float) m));
                if (std::string (id) == "RUMBLE") r.set ("source", 1.0f);
                double ppq = 0.0; float pk = 0.0f; bool bad = false;
                const int blocks = (int) (kSr * 2.0 / kBlock);
                for (int b = 0; b < blocks; ++b)
                {
                    head.ppq = ppq;
                    r.run ([&] (juce::AudioBuffer<float>&, juce::MidiBuffer& mb, int) { fillMidi (mb, ppq, head.bpm, kBlock); }, b);
                    ppq += kBlock * head.bpm / 60.0 / kSr;
                    for (int c = 0; c < 2; ++c) { pk = std::max (pk, r.buf.getMagnitude (c, 0, kBlock)); for (int i = 0; i < kBlock; ++i) if (! std::isfinite (r.buf.getSample (c, i))) bad = true; }
                }
                const juce::String nm = model ? dynamic_cast<juce::AudioParameterChoice*> (model)->choices[m] : juce::String (id);
                const bool sampleModel = nm == "SAMPLE" || nm == "STUTTER";
                line << nm << " " << juce::String (gainToDb (pk), 0) << "  ";
                CHECK (! bad && pk < 3.99f, "%s model %s clamp/NaN", id, nm.toRawUTF8());
                CHECK (pk < 1.4f, "%s model %s too hot (%.1f dB)", id, nm.toRawUTF8(), gainToDb (pk));
                if (! sampleModel || withSample) CHECK (pk > 0.003f, "%s model %s silent (sample %d)", id, nm.toRawUTF8(), withSample);
            }
            std::printf ("   %-7s %s %s\n", id, withSample ? "+smp" : "    ", line.toRawUTF8());
        }
    }
}

// DECK: a 128 BPM click track played in a 124 BPM set must land its clicks on the set's beats.
static void testDeck()
{
    std::printf ("-- deck: tempo sync 128 -> 124 BPM\n");
    const double trackBpm = 128.0;
    const int len = (int) (kSr * 30.0);
    juce::AudioBuffer<float> trk (2, len);
    trk.clear();
    const double spb = kSr * 60.0 / trackBpm;
    for (double p = kSr * 0.25; p < len - 4000; p += spb)
        for (int i = 0; i < 3000; ++i) { const float v = std::sin (6.2831853f * 60.0f * i / (float) kSr) * std::exp (-i / 600.0f) * 0.8f; trk.setSample (0, (int) p + i, v); trk.setSample (1, (int) p + i, v); }
    juce::File wav = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("sf_test_track.wav");
    {
        wav.deleteFile();
        juce::WavAudioFormat fmt;
        std::unique_ptr<juce::AudioFormatWriter> w (fmt.createWriterFor (new juce::FileOutputStream (wav), kSr, 2, 24, {}, 0));
        w->writeFromAudioSampleBuffer (trk, 0, len);
    }
    resetBus();
    FakeHead head;
    Rig r ("DECK", head);
    juce::ValueTree root ("SFSTATE"), mod ("MODULE");
    root.setProperty ("module", "DECK", nullptr);
    root.appendChild (r.p->apvts.copyState(), nullptr);
    mod.setProperty ("deckA", wav.getFullPathName(), nullptr);
    root.appendChild (mod, nullptr);
    juce::MemoryBlock mb; { juce::MemoryOutputStream os (mb, false); root.writeToStream (os); }
    r.p->setStateInformation (mb.getData(), (int) mb.getSize());
    juce::Thread::sleep (6000);   // background load + analysis
    r.set ("play_a", 1.0f);
    if (getenv ("SF_NOLOCK")) r.set ("key_lock", 0.0f);
    double ppq = 0.0;
    std::vector<double> onsets;
    float prevEnv = 0.0f, env = 0.0f, pk = 0.0f;
    const int blocks = (int) (kSr * 16.0 / kBlock);
    for (int b = 0; b < blocks; ++b)
    {
        head.ppq = ppq;
        r.run ([] (juce::AudioBuffer<float>&, juce::MidiBuffer&, int) {}, b);
        for (int i = 0; i < kBlock; ++i)
        {
            const float x = std::fabs (r.buf.getSample (0, i));
            pk = std::max (pk, x);
            env = std::max (x, env * 0.999f);
            if (env > 0.08f && prevEnv <= 0.08f) onsets.push_back (ppq + i * head.bpm / 60.0 / kSr);
            prevEnv = env;
        }
        ppq += kBlock * head.bpm / 60.0 / kSr;
    }
    double maxErr = 0.0, meanErr = 0.0; int counted = 0;
    for (size_t k = 2; k < onsets.size(); ++k) { const double s = onsets[k] - std::round (onsets[k]); maxErr = std::max (maxErr, std::abs (s)); meanErr += s; ++counted; }
    if (counted > 0) std::printf ("   mean signed offset %.4f beat (%.1f ms)\n", meanErr / counted, meanErr / counted * 60000.0 / head.bpm);
    std::printf ("   peak %.1f dB, %d onsets, worst distance from the beat %.3f beat (%.1f ms)\n", gainToDb (pk), (int) onsets.size(), maxErr, maxErr * 60000.0 / head.bpm);
    CHECK (pk > 0.05f, "DECK silent after PLAY A");
    CHECK (counted >= 20, "DECK produced too few beats (%d)", counted);
    CHECK (maxErr < 0.01, "DECK drifts off the grid (%.3f beat)", maxErr);
}

// SLOW: a full-range move with SLOW = 1 BAR takes a bar. MACRO: CONTROL macro drives FILTER.
static void testSlowAndMacro()
{
    std::printf ("-- slow mode + macros\n");
    resetBus();
    FakeHead head;
    Rig ctl ("CONTROL", head), f ("FILTER", head);
    double ppq = 0.0;
    auto step = [&] (int blocks) { for (int k = 0; k < blocks; ++k) { head.ppq = ppq; ctl.run ([] (juce::AudioBuffer<float>&, juce::MidiBuffer&, int) {}, k); f.run ([] (juce::AudioBuffer<float>&, juce::MidiBuffer&, int) {}, k); ppq += kBlock * head.bpm / 60.0 / kSr; } };
    f.set ("filter", 0.0f); step (20);
    f.set ("slow", 1.0f);                       // 1 BAR
    f.set ("filter", 1.0f);
    const int barBlocks = (int) (4.0 * 60.0 / head.bpm * kSr / kBlock);
    step (barBlocks / 2);
    const float half = f.p->currentValue (0);
    step (barBlocks / 2 + 4);
    const float full = f.p->currentValue (0);
    std::printf ("   SLOW 1 BAR: %.2f after half a bar, %.2f after a bar\n", half, full);
    CHECK (half > 0.4f && half < 0.6f && full > 0.99f, "SLOW glide wrong (%.2f, %.2f)", half, full);
    f.set ("slow", 0.0f); f.set ("filter", 0.5f); step (10);
    f.set ("macro_src", 1.0f); f.set ("macro_amt", 0.5f); ctl.set ("macro1", 0.6f); step (80);
    const float m = f.p->currentValue (0);
    std::printf ("   MACRO 1 = 60 %%, AMT +50 %%: FILTER %.2f (expect 0.80)\n", m);
    CHECK (std::abs (m - 0.8f) < 0.02f, "MACRO response wrong (%.2f)", m);
}

// DENSITY: 1/8 hats with DENSITY 50 % (ECHO SPACE 1/16) become 1/16 hats. Same for the effect.
static int countHits (const std::vector<float>& x)
{
    int hits = 0; float env = 0.0f; bool above = false; float pk = 0.0f;
    for (float v : x) pk = std::max (pk, std::fabs (v));
    for (float v : x)
    {
        env = std::max (std::fabs (v), env * 0.995f);
        if (! above && env > pk * 0.15f) { ++hits; above = true; }
        if (above && env < pk * 0.05f) above = false;
    }
    return hits;
}
static void testDensity()
{
    std::printf ("-- density: 1/8 -> 1/16\n");
    for (int pass = 0; pass < 3; ++pass)
    {
        int counts[2] {};
        for (int dset = 0; dset < 2; ++dset)
        {
            resetBus();
            FakeHead head;
            Rig r (pass == 0 ? "PERC" : pass == 1 ? "DENSITY" : "STRIP", head);
            if (pass == 0) { r.set ("density", dset ? 0.5f : 0.0f); r.set ("decay", 0.2f); }
            else if (pass == 1) { r.set ("density", dset ? 0.5f : 0.0f); r.set ("space", 1.0f); }
            else { r.set ("density", dset ? 0.5f : 0.0f); r.set ("echo_space", 1.0f); }
            std::vector<float> out;
            double ppq = 0.0;
            const int blocks = (int) (4.0 * 60.0 / head.bpm * kSr / kBlock) * 2;   // 2 bars
            for (int b = 0; b < blocks; ++b)
            {
                head.ppq = ppq;
                r.run ([&] (juce::AudioBuffer<float>& buf, juce::MidiBuffer& m, int)
                {
                    const double bps = head.bpm / 60.0 / kSr;
                    for (int i = 0; i < kBlock; ++i)
                    {
                        const double q = ppq + i * bps;
                        const bool on = std::floor (q * 2.0) != std::floor ((q - bps) * 2.0);
                        if (pass == 0 && on) m.addEvent (juce::MidiMessage::noteOn (1, 42, (juce::uint8) 110), i);
                        if (pass >= 1)
                        {
                            const double ph = q * 2.0 - std::floor (q * 2.0);
                            const float t = (float) (ph * 30.0 / head.bpm);
                            const float v = t < 0.02f ? (float) std::sin (6.2831853 * 3000.0 * t) * (1.0f - t / 0.02f) * 0.5f : 0.0f;
                            buf.setSample (0, i, v); buf.setSample (1, i, v);
                        }
                    }
                }, b);
                for (int i = 0; i < kBlock; ++i) out.push_back (r.buf.getSample (0, i));
                ppq += kBlock * head.bpm / 60.0 / kSr;
            }
            counts[dset] = countHits (out);
        }
        std::printf ("   %s: %d hits at 0 %%, %d hits at 50 %%\n", pass == 0 ? "PERC   " : pass == 1 ? "DENSITY" : "STRIP  ", counts[0], counts[1]);
        CHECK (counts[1] >= counts[0] * 2 - 2 && counts[0] >= 14, "%s DENSITY did not double the hits", pass == 0 ? "PERC" : pass == 1 ? "DENSITY" : "STRIP");
    }
}

// Regressions from review: held mono bass survives DENSITY echoes; SLOW is fixed-time; ALLOC SHARE;
// CONTROL program change selects the song.
static void testRegressions()
{
    std::printf ("-- regressions\n");
    {
        resetBus();
        FakeHead head;
        Rig r ("BASS", head);
        r.set ("density", 0.3f); r.set ("pocket_depth", 0.0f);
        double ppq = 0.0;
        float early = 0, late = 0; int ne = 0, nl = 0;
        const int blocks = (int) (3.0 * 60.0 / head.bpm * kSr / kBlock);
        for (int b = 0; b < blocks; ++b)
        {
            head.ppq = ppq;
            r.run ([&] (juce::AudioBuffer<float>&, juce::MidiBuffer& m, int)
            {
                if (b == 0) m.addEvent (juce::MidiMessage::noteOn (1, 36, (juce::uint8) 100), 0);
                const double next = ppq + kBlock * head.bpm / 60.0 / kSr;
                if (ppq < 2.0 && next >= 2.0) m.addEvent (juce::MidiMessage::noteOff (1, 36), 0);
            }, b);
            const float rms = r.buf.getRMSLevel (0, 0, kBlock);
            if (ppq > 0.3 && ppq < 0.6) { early += rms; ++ne; }
            if (ppq > 1.2 && ppq < 1.9) { late += rms; ++nl; }
            ppq += kBlock * head.bpm / 60.0 / kSr;
        }
        early /= std::max (1, ne); late /= std::max (1, nl);
        std::printf ("   held BASS note with DENSITY: level %.3f early, %.3f late\n", early, late);
        CHECK (late > early * 0.3f, "held mono note was cut by its own echoes");
    }
    {
        resetBus();
        FakeHead head;
        Rig f ("FILTER", head);
        double ppq = 0.0;
        auto step = [&] (int blocks) { for (int k = 0; k < blocks; ++k) { head.ppq = ppq; f.run ([] (juce::AudioBuffer<float>&, juce::MidiBuffer&, int) {}, k); ppq += kBlock * head.bpm / 60.0 / kSr; } };
        f.set ("filter", 0.5f); step (20);
        f.set ("slow", 1.0f); f.set ("filter", 0.75f);
        const int barBlocks = (int) (4.0 * 60.0 / head.bpm * kSr / kBlock);
        step (barBlocks / 2);
        const float half = f.p->currentValue (0);
        std::printf ("   SLOW 1 BAR, quarter turn: %.3f after half a bar (expect 0.625)\n", half);
        CHECK (std::abs (half - 0.625f) < 0.02f, "SLOW is not fixed-time (%.3f)", half);
    }
    {
        resetBus();
        FakeHead head;
        Rig ctl ("CONTROL", head);
        ctl.p->setCurrentProgram (5);
        std::printf ("   CONTROL program 6 -> bus song %d\n", (int) Bus::get().d().song.load() + 1);
        CHECK (Bus::get().d().song.load() == 5, "program change did not select the song");
        CHECK (ctl.p->getNumPrograms() == 16, "CONTROL should expose 16 programs");
    }
}

// --dump: every module's parameters as JSON (for the manual).
static void dumpSpecs()
{
    std::printf ("{\n");
    auto ids = allModuleIds();
    for (size_t m = 0; m < ids.size(); ++m)
    {
        SFProcessor p (makeModule (ids[m]));
        std::printf ("\"%s\": {\"instrument\": %s, \"latency\": %d, \"params\": [\n", ids[m].c_str(), p.mod().instrument() ? "true" : "false", p.mod().latency());
        const auto& sp = p.allSpecs();
        for (size_t k = 0; k < sp.size(); ++k)
        {
            const auto& s = sp[k];
            juce::String choices;
            for (size_t c = 0; c < s.choices.size(); ++c) choices << (c ? "|" : "") << s.choices[c];
            auto* prm = p.param ((int) k);
            const juce::String defText = s.type == PType::Float ? prm->getText (prm->convertTo0to1 (s.def), 64)
                                       : s.type == PType::Choice ? juce::String (s.choices[(size_t) s.def]) : juce::String (s.def > 0.5f ? "ON" : "OFF");
            const juce::String minText = s.type == PType::Float ? prm->getText (0.0f, 64) : juce::String();
            const juce::String maxText = s.type == PType::Float ? prm->getText (1.0f, 64) : juce::String();
            const char* type = s.type == PType::Float ? "knob" : s.type == PType::Choice ? "menu" : s.type == PType::Toggle ? "toggle" : "momentary";
            std::printf ("  {\"id\": \"%s\", \"name\": \"%s\", \"type\": \"%s\", \"primary\": %s, \"min\": \"%s\", \"max\": \"%s\", \"def\": \"%s\", \"choices\": \"%s\", \"macro\": %d}%s\n",
                         s.id.c_str(), s.name.c_str(), type, s.primary ? "true" : "false", minText.toRawUTF8(), maxText.toRawUTF8(), defText.toRawUTF8(), choices.toRawUTF8(), s.macroDir, k + 1 < sp.size() ? "," : "");
        }
        std::printf ("]}%s\n", m + 1 < ids.size() ? "," : "");
    }
    std::printf ("}\n");
}

// FILTER crossing the centre (LP <-> open <-> HP) must not click: sweep a sustained chord slowly
// through the centre and compare the worst high-frequency burst with a steady reference.
static void testFilterCrossing()
{
    std::printf ("-- filter: centre crossing\n");
    resetBus();
    FakeHead head;
    Rig r ("FILTER", head);
    r.set ("res", 0.5f);
    double ppq = 0.0;
    Svf hp; hp.set (9000.0f, 0.7f, kSr);
    float steady = 0.0f, crossing = 0.0f;
    const int blocks = (int) (kSr * 4.0 / kBlock);
    double ph = 0.0;
    for (int b = 0; b < blocks; ++b)
    {
        head.ppq = ppq;
        // Hold at 0.40 for 1 s, then sweep 0.40 -> 0.60 -> 0.40 over 3 s.
        const double t = b * (double) kBlock / kSr;
        float k = 0.40f;
        if (t > 1.0) { const double u = (t - 1.0) / 3.0; k = 0.40f + 0.20f * (float) (u < 0.5 ? u * 2.0 : 2.0 - u * 2.0); }
        r.set ("filter", k);
        r.run ([&] (juce::AudioBuffer<float>& buf, juce::MidiBuffer&, int)
        {
            for (int i = 0; i < kBlock; ++i)
            {
                ph += 1.0 / kSr;
                const float v = 0.25f * (float) (std::sin (6.2831853 * 110.0 * ph) + std::sin (6.2831853 * 164.8 * ph) + std::sin (6.2831853 * 220.0 * ph));
                buf.setSample (0, i, v); buf.setSample (1, i, v);
            }
        }, b);
        for (int i = 0; i < kBlock; ++i)
        {
            const float h = std::fabs (hp.tickHP (r.buf.getSample (0, i)));
            if (t > 0.3 && t < 1.0) steady = std::max (steady, h);
            if (t > 1.0) crossing = std::max (crossing, h);
        }
        ppq += kBlock * head.bpm / 60.0 / kSr;
    }
    std::printf ("   high-frequency peak: steady %.5f, while crossing %.5f\n", steady, crossing);
    CHECK (crossing < std::max (0.001f, steady * 3.0f), "FILTER clicks crossing the centre (%.5f vs %.5f)", crossing, steady);
}

// DECAY gates samples on KICK and PERC: a 1 s sustained sample, one hit, DECAY 0 vs 100 %.
static void testSampleGate()
{
    std::printf ("-- sample gate (DECAY)\n");
    juce::AudioBuffer<float> smp (2, 48000);
    for (int i = 0; i < 48000; ++i) { const float v = 0.5f * std::sin (6.2831853f * 220.0f * (float) i / 48000.0f); smp.setSample (0, i, v); smp.setSample (1, i, v); }
    juce::File wav = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("sf_test_sustain.wav");
    {
        wav.deleteFile();
        juce::WavAudioFormat fmt;
        std::unique_ptr<juce::AudioFormatWriter> w (fmt.createWriterFor (new juce::FileOutputStream (wav), 48000.0, 2, 24, {}, 0));
        w->writeFromAudioSampleBuffer (smp, 0, smp.getNumSamples());
    }
    for (auto id : { "KICK", "PERC" })
    {
        float tail[2] {};
        for (int pass = 0; pass < 2; ++pass)
        {
            resetBus();
            FakeHead head;
            Rig r (id, head);
            r.p->mod().loadFiles (juce::StringArray (wav.getFullPathName()));
            r.set ("model", (float) (std::string (id) == "KICK" ? 6 : 12));
            r.set ("decay", pass == 0 ? 0.0f : 1.0f);
            double ppq = 0.0;
            const int blocks = (int) (kSr * 0.6 / kBlock);
            for (int b = 0; b < blocks; ++b)
            {
                head.ppq = ppq;
                r.run ([&] (juce::AudioBuffer<float>&, juce::MidiBuffer& m, int) { if (b == 4) m.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 120), 0); }, b);
                const double t = (b - 4) * (double) kBlock / kSr;
                if (t > 0.3) tail[pass] = std::max (tail[pass], r.buf.getMagnitude (0, 0, kBlock));
                ppq += kBlock * head.bpm / 60.0 / kSr;
            }
        }
        std::printf ("   %s SAMPLE: level 300 ms after the hit: DECAY 0 %% %.4f, DECAY 100 %% %.4f\n", id, tail[0], tail[1]);
        CHECK (tail[0] < 0.001f && tail[1] > 0.05f, "%s DECAY does not gate the sample", id);
    }
}

// STRIP: GATE (HITS) closes each hit; DENSITY adds hits to any audio.
static void testStrip()
{
    std::printf ("-- strip\n");
    float tail[2] {};
    for (int pass = 0; pass < 2; ++pass)
    {
        resetBus();
        FakeHead head;
        Rig r ("STRIP", head);
        r.set ("gate", pass == 0 ? 1.0f : 0.1f);
        double ppq = 0.0, ph = 0.0;
        const int blocks = (int) (kSr * 3.0 / kBlock);
        for (int b = 0; b < blocks; ++b)
        {
            head.ppq = ppq;
            r.run ([&] (juce::AudioBuffer<float>& buf, juce::MidiBuffer&, int)
            {
                const double bps = head.bpm / 60.0 / kSr;
                for (int i = 0; i < kBlock; ++i)
                {
                    const double q = ppq + i * bps;
                    const double t = (q - std::floor (q)) * 60.0 / head.bpm;     // seconds since the beat
                    ph += 1.0 / kSr;
                    const float v = 0.5f * (float) std::sin (6.2831853 * 220.0 * ph) * (float) std::exp (-t / 0.4);   // long ringing hit each beat
                    buf.setSample (0, i, v); buf.setSample (1, i, v);
                }
            }, b);
            // level at 0.6 beat after each beat (past a short gate)
            const double bps = head.bpm / 60.0 / kSr;
            for (int i = 0; i < kBlock; ++i)
            {
                const double q = ppq + i * bps - Pocket::kLatency * bps;   // output timeline
                const double fr = q - std::floor (q);
                if (q > 2.0 && fr > 0.55 && fr < 0.65) tail[pass] = std::max (tail[pass], std::fabs (r.buf.getSample (0, i)));
            }
            ppq += kBlock * bps;
        }
    }
    std::printf ("   GATE 100 %%: tail %.3f   GATE 10 %%: tail %.4f\n", tail[0], tail[1]);
    CHECK (tail[0] > 0.1f && tail[1] < 0.005f, "STRIP GATE does not close hits");
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    if (argc > 1 && std::string (argv[1]) == "--dump") { dumpSpecs(); return 0; }
    std::vector<std::string> ids = allModuleIds();
    if (argc > 1) { ids.clear(); for (int i = 1; i < argc; ++i) if (argv[i][0] != '-') ids.push_back (argv[i]); }
    std::printf ("SF LIVE SUITE offline tests (bus %s)\n", Bus::get().shared() ? "shared" : "local");
    for (auto& id : ids) testModule (id);
    if (argc <= 1) { testPocket(); testAlloc(); testSongs(); testSlowAndMacro(); testDensity(); testRegressions(); testFilterCrossing(); testSampleGate(); testStrip(); testModels(); testDeck(); }
    else if (std::string (argv[1]) == "--perf") { testSlowAndMacro(); testDensity(); testRegressions(); }
    else if (std::string (argv[1]) == "--models") { testModels(); }
    else if (std::string (argv[1]) == "--deck") { testDeck(); }
    else if (std::string (argv[1]) == "--filter") { testFilterCrossing(); }
    else if (std::string (argv[1]) == "--gate") { testSampleGate(); }
    else if (std::string (argv[1]) == "--strip") { testStrip(); }
    std::printf ("%s: %d failure(s)\n", failures == 0 ? "PASS" : "FAIL", failures);
    return failures == 0 ? 0 : 1;
}
