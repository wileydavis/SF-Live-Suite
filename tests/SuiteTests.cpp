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

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    std::vector<std::string> ids = allModuleIds();
    if (argc > 1) { ids.clear(); for (int i = 1; i < argc; ++i) if (argv[i][0] != '-') ids.push_back (argv[i]); }
    std::printf ("SF LIVE SUITE offline tests (bus %s)\n", Bus::get().shared() ? "shared" : "local");
    for (auto& id : ids) testModule (id);
    if (argc <= 1) { testPocket(); testAlloc(); testSongs(); testModels(); testDeck(); }
    else if (std::string (argv[1]) == "--models") { testModels(); }
    else if (std::string (argv[1]) == "--deck") { testDeck(); }
    std::printf ("%s: %d failure(s)\n", failures == 0 ? "PASS" : "FAIL", failures);
    return failures == 0 ? 0 : 1;
}
