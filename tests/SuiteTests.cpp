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
#include "core/Capture.h"
#include "core/Dsp.h"
#include "core/Editor.h"
#include "core/Pocket.h"
#include "core/Processor.h"
#include "core/Registry.h"
#include <chrono>
#include <complex>
#include <limits>
#include <cstdio>
#include <map>
#include <random>
#include <thread>

using namespace sf;

struct FakeHead : juce::AudioPlayHead
{
    double bpm = 124.0, ppq = 0.0;
    bool playing = true;
    int beatsPerBar = 4;                    // the time signature: beatsPerBar / 4
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p;
        p.setBpm (bpm);
        p.setPpqPosition (ppq);
        p.setIsPlaying (playing);
        p.setTimeSignature (TimeSignature { beatsPerBar, 4 });
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
    for (auto& s : d.src) { s.owner = 0; s.seenMs = 0; s.uid = 0; }
    for (auto& m : d.macro) m = 0.0f;
    d.keyRoot = -1;
    d.songAtPpq = -1.0;
    d.panicHeld = 0;
    d.dropPpq = -1.0; d.dropDb = 0.0f; d.storeSong = -1;
    d.slowAll = 0; d.slowAllPrev = 0; d.slowAllFromPpq = -1.0;
    for (auto& g : d.glides) { g.owner = 0; g.count = 0; g.seenMs = 0; }
    d.capStatus = 0; d.capCancel = 0; d.captureSeenMs = 0; d.capOwnerSeenMs = 0; d.capProgress = 0.0f; d.capPeak = 0.0f;
    capture::testHold = 0; capture::testShortTake = false;
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
                if (specs[k].id == "slow" || specs[k].id == "slow_all" || specs[k].id == "level") continue;
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
    if (! inst && id != "ALLOC" && id != "CAPTURE") CHECK (peakDefault > 0.01f, "%s silent at default settings", id.c_str());
    // SF CAPTURE never passes its input, whatever its parameters: it plays only a sweep TERROIR asked for.
    if (id == "CAPTURE") CHECK (peakAll == 0.0f, "CAPTURE passed its input (%.1f dB)", gainToDb (peakAll));

    // State round trip.
    juce::MemoryBlock mb;
    r.p->getStateInformation (mb);
    SFProcessor q (makeModule (id));
    q.setStateInformation (mb.getData(), (int) mb.getSize());
    for (size_t k = 0; k < specs.size(); ++k)
    {
        const float a = q.param ((int) k)->convertFrom0to1 (q.param ((int) k)->getValue());
        const float b = r.p->param ((int) k)->convertFrom0to1 (r.p->param ((int) k)->getValue());
        // Momentary buttons (PANIC, STORE ALL, RESET ...) always come back released.
        if (specs[k].type == PType::Momentary) { CHECK (a < 0.5f, "%s reopened with %s pressed", id.c_str(), specs[k].id.c_str()); continue; }
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
    std::printf ("-- alloc: priority curve, BASS above PAD in the list, band 4 (150-220 Hz)\n");
    resetBus();
    FakeHead head;
    Rig a ("TAP", head), b ("TAP", head), alloc ("ALLOC", head);
    a.set ("role", (float) ROLE_BASS); b.set ("role", (float) ROLE_PAD);
    double ppq = 0.0;
    auto& d = Bus::get().d();
    auto run = [&] (float secs, bool bassPlays)
    {
        for (int k = 0; k < (int) (kSr * secs / kBlock); ++k)
        {
            head.ppq = ppq;
            auto sine = [&] (juce::AudioBuffer<float>& buf, juce::MidiBuffer&, int)
            { for (int i = 0; i < kBlock; ++i) for (int c = 0; c < 2; ++c) buf.setSample (c, i, 0.3f * std::sin (6.2831853f * 180.0f * (float) ((k * kBlock + i) / kSr))); };
            if (bassPlays) a.run (sine, k); else a.run ([] (juce::AudioBuffer<float>&, juce::MidiBuffer&, int) {}, k);
            b.run (sine, k);
            alloc.run ([] (juce::AudioBuffer<float>&, juce::MidiBuffer&, int) {}, k);
            ppq += kBlock * head.bpm / 60.0 / kSr;
        }
    };
    auto cuts = [&] (float& cb, float& cp)
    {
        cb = cp = 0.0f;
        for (int s = 0; s < kSources; ++s)
        {
            if (! Bus::get().sourceAlive (s)) continue;
            if (d.src[s].role.load() == ROLE_BASS) cb = d.src[s].cut[3].load(); else cp = d.src[s].cut[3].load();
        }
    };
    float cb, cp;
    run (1.0f, true); cuts (cb, cp);
    std::printf ("   DEPTH 0: cut BASS %.1f, PAD %.1f dB\n", cb, cp);
    CHECK (cb < 0.1f && cp < 0.1f, "DEPTH 0 should cut nothing");
    alloc.set ("depth4", 1.0f); alloc.set ("q4", 1.0f); alloc.set ("pri4", 0.0f);
    run (1.0f, true); cuts (cb, cp);
    std::printf ("   PRIORITY top: cut BASS %.1f, PAD %.1f dB\n", cb, cp);
    CHECK (cb < 0.5f && cp > 9.0f, "PRIORITY at the top should protect BASS");
    alloc.set ("pri4", 1.0f);
    run (1.0f, true); cuts (cb, cp);
    std::printf ("   PRIORITY bottom: cut BASS %.1f, PAD %.1f dB\n", cb, cp);
    CHECK (cb > 9.0f && cp < 0.5f, "PRIORITY at the bottom should protect PAD");
    alloc.set ("pri4", 0.0f); alloc.set ("q4", 0.0f);
    run (1.0f, true); cuts (cb, cp);
    std::printf ("   Q 0 (wide): cut BASS %.1f, PAD %.1f dB\n", cb, cp);
    CHECK (cp < 3.0f, "wide Q should share the band");
    alloc.set ("q4", 1.0f);
    run (1.5f, false); cuts (cb, cp);
    for (int s = 0; s < kSources; ++s) if (Bus::get().sourceAlive (s) && d.src[s].role.load() == ROLE_BASS) std::printf ("   (BASS band-4 energy now %.1f dB)\n", d.src[s].energy[3].load());
    std::printf ("   DYNAMIC, BASS silent: cut PAD %.1f dB\n", cp);
    CHECK (cp < 0.5f, "DYNAMIC should not cut when the favoured source is silent");
    alloc.set ("mode", 1.0f);
    run (1.0f, false); cuts (cb, cp);
    std::printf ("   STATIC, BASS silent: cut PAD %.1f dB\n", cp);
    CHECK (cp > 9.0f, "STATIC should always cut");
    // Band 4 gets its own order (PAD above BASS); the global order is unchanged.
    alloc.set ("mode", 0.0f);
    {
        juce::MemoryBlock mb; alloc.p->getStateInformation (mb);
        auto root = juce::ValueTree::readFromData (mb.getData(), mb.getSize());
        auto mod = root.getChildWithName ("MODULE");
        mod.setProperty ("order", "TAP BASS\nTAP PAD\n", nullptr);
        mod.setProperty ("linked3", false, nullptr);
        mod.setProperty ("order3", "TAP PAD\nTAP BASS\n", nullptr);
        juce::MemoryBlock out; { juce::MemoryOutputStream os (out, false); root.writeToStream (os); }
        alloc.p->setStateInformation (out.getData(), (int) out.getSize());
    }
    alloc.set ("pri4", 0.0f); alloc.set ("depth4", 1.0f); alloc.set ("q4", 1.0f);
    alloc.set ("pri5", 0.0f); alloc.set ("depth5", 1.0f); alloc.set ("q5", 1.0f);
    run (1.0f, true); cuts (cb, cp);
    float cb5 = 0, cp5 = 0;
    for (int s = 0; s < kSources; ++s) if (Bus::get().sourceAlive (s)) { if (d.src[s].role.load() == ROLE_BASS) cb5 = d.src[s].cut[4].load(); else cp5 = d.src[s].cut[4].load(); }
    std::printf ("   band 4 own order (PAD first): cut BASS %.1f, PAD %.1f dB; band 5 global: cut BASS %.1f, PAD %.1f dB\n", cb, cp, cb5, cp5);
    CHECK (cb > 9.0f && cp < 0.5f, "band order did not apply");
    CHECK (cp5 >= cb5, "global order changed with the band order");
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
    const int blocks = (int) (kSr * 24.0 / kBlock);
    for (int b = 0; b < blocks; ++b)
    {
        if (b == (int) (kSr * 12.0 / kBlock)) { ppq -= 6.5; std::printf ("   (transport jumps back 6.5 beats)\n"); }
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
    CHECK (counted >= 34, "DECK produced too few beats (%d)", counted);
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
        ctl.run ([] (juce::AudioBuffer<float>&, juce::MidiBuffer&, int) {}, 0);   // published by the next audio block
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
        p.prepareToPlay (48000.0, 512);                 // latency depends on the rate (reported at 48 kHz)
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
            const bool slows = s.glidesWithSlow() && (int) k < p.numModuleParams();
            std::printf ("  {\"id\": \"%s\", \"name\": \"%s\", \"type\": \"%s\", \"primary\": %s, \"perform\": %s, \"setup\": %s, \"sample\": %s, \"slow\": %s, \"min\": \"%s\", \"max\": \"%s\", \"def\": \"%s\", \"choices\": \"%s\", \"macro\": %d}%s\n",
                         s.id.c_str(), s.name.c_str(), type, s.primary ? "true" : "false", s.perform ? "true" : "false", s.setup ? "true" : "false", s.inSample ? "true" : "false", slows ? "true" : "false",
                         minText.toRawUTF8(), maxText.toRawUTF8(), defText.toRawUTF8(), choices.toRawUTF8(), s.macroDir, k + 1 < sp.size() ? "," : "");
        }
        std::printf ("]}%s\n", m + 1 < ids.size() ? "," : "");
    }
    std::printf ("}\n");
}

// --state MODULE [seed=N] [id=value ...]: a fresh plugin's state with the given parameters set, as the
// hex bytes Live stores for a VST3 (ProcessorState), and every parameter's normalised value, as JSON.
// Used by controllers/live/make_set.py. A value is a number in the parameter's own unit, a choice name,
// or on/off. The state carries uid 0, so every instance keeps the random id it is born with.
static int dumpState (int argc, char** argv)
{
    if (argc < 3) { std::fprintf (stderr, "usage: SFTests --state MODULE [seed=N] [id=value ...]\n"); return 2; }
    const std::string mid = argv[2];
    const auto ids = allModuleIds();
    if (std::find (ids.begin(), ids.end(), mid) == ids.end()) { std::fprintf (stderr, "no module %s\n", mid.c_str()); return 2; }
    SFProcessor p (makeModule (mid));
    const auto& sp = p.allSpecs();
    int seed = 1;
    for (int a = 3; a < argc; ++a)
    {
        const std::string arg = argv[a];
        const auto eq = arg.find ('=');
        if (eq == std::string::npos) { std::fprintf (stderr, "not id=value: %s\n", arg.c_str()); return 2; }
        const auto key = arg.substr (0, eq), val = arg.substr (eq + 1);
        if (key == "seed") { seed = std::stoi (val); continue; }
        const int k = p.paramIndex (key);
        if (k < 0) { std::fprintf (stderr, "%s has no parameter %s\n", mid.c_str(), key.c_str()); return 2; }
        const auto& s = sp[(size_t) k];
        float v = 0.0f;
        if (s.type == PType::Choice)
        {
            const auto it = std::find (s.choices.begin(), s.choices.end(), val);
            if (it == s.choices.end()) { std::fprintf (stderr, "%s %s has no choice %s\n", mid.c_str(), key.c_str(), val.c_str()); return 2; }
            v = (float) (it - s.choices.begin());
        }
        else if (s.type != PType::Float)
        {
            if (val != "on" && val != "off") { std::fprintf (stderr, "%s %s takes on or off\n", mid.c_str(), key.c_str()); return 2; }
            v = val == "on" ? 1.0f : 0.0f;
        }
        else
        {
            v = std::stof (val);
            if (v < s.min || v > s.max) { std::fprintf (stderr, "%s %s: %s is outside %g..%g\n", mid.c_str(), key.c_str(), val.c_str(), s.min, s.max); return 2; }
        }
        auto* prm = p.param (k);
        prm->setValueNotifyingHost (prm->convertTo0to1 (v));
    }
    juce::MemoryBlock mb;
    p.getStateInformation (mb);
    auto root = juce::ValueTree::readFromData (mb.getData(), mb.getSize());
    root.setProperty ("uid", "0", nullptr);
    root.setProperty ("motionSeed", seed, nullptr);
    juce::MemoryOutputStream os;
    root.writeToStream (os);
    // The state must load back into a fresh plugin with every value intact.
    SFProcessor q (makeModule (mid));
    q.setStateInformation (os.getData(), (int) os.getDataSize());
    bool same = true;
    for (size_t k = 0; k < sp.size(); ++k)
        if (sp[k].type != PType::Momentary && std::abs (q.param ((int) k)->getValue() - p.param ((int) k)->getValue()) > 1e-6f) same = false;
    // programs > 1: JUCE's VST3 wrapper adds a Program parameter after these (CONTROL's songs).
    std::printf ("{\"module\": \"%s\", \"roundtrip\": %s, \"programs\": %d, \"program\": %d, \"state\": \"%s\", \"params\": [\n", mid.c_str(),
                 same ? "true" : "false", p.getNumPrograms(), p.getCurrentProgram(),
                 juce::String::toHexString (os.getData(), (int) os.getDataSize(), 0).toUpperCase().toRawUTF8());
    for (size_t k = 0; k < sp.size(); ++k)
    {
        const auto& s = sp[k];
        auto* prm = p.param ((int) k);
        const char* type = s.type == PType::Float ? "knob" : s.type == PType::Choice ? "menu" : s.type == PType::Toggle ? "toggle" : "momentary";
        const int steps = s.type == PType::Choice ? (int) s.choices.size() : s.type == PType::Float ? 0 : 2;
        // The id a VST3 host sees: JUCE hashes the parameter id and clears the top bit (Studio One rule).
        const auto vst3Id = (juce::uint32) juce::String (s.id).hashCode() & 0x7fffffffu;
        std::printf ("  {\"id\": \"%s\", \"name\": \"%s\", \"type\": \"%s\", \"vst3_id\": %u, \"norm\": %.9g, \"def_norm\": %.9g, \"min\": %.9g, \"max\": %.9g, \"centre\": %s, \"steps\": %d}%s\n",
                     s.id.c_str(), s.name.c_str(), type, (unsigned) vst3Id, (double) prm->getValue(), (double) prm->convertTo0to1 (s.def), (double) s.min, (double) s.max,
                     std::isnan (s.centre) ? "null" : juce::String (s.centre, 9).toRawUTF8(), steps, k + 1 < sp.size() ? "," : "");
    }
    std::printf ("]}\n");
    return 0;
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

// Transport jumps (loop braces, stop/restart, relocate) must not break timing.
static void testTransportJumps()
{
    std::printf ("-- transport jumps\n");
    const double bpsH = 124.0 / 60.0 / kSr;
    auto kickNotes = [&] (double ppq, juce::MidiBuffer& m)
    {
        for (int i = 0; i < kBlock; ++i) if (std::floor (ppq + i * bpsH) != std::floor (ppq + (i - 1) * bpsH)) m.addEvent (juce::MidiMessage::noteOn (1, 36, (juce::uint8) 120), i);
    };
    // 1. Stale kick reports: play 1 bar of kick, jump back to 0 with the kick muted: DUCK must not duck.
    // (Runs on a simulated clock: offline processing is faster than real time.)
    static double simSec = 1000.0;
    Bus::testClock = [] { return simSec; };
    {
        resetBus();
        FakeHead head;
        Rig kick ("KICK", head), duck ("DUCK", head);
        duck.set ("depth", 1.0f); duck.set ("shape", 0.7f);
        double ppq = 0.0; float minG = 1.0f;
        const int bar = (int) (4.0 / bpsH / kBlock);
        for (int b = 0; b < bar * 2; ++b)
        {
            if (b == bar) ppq = 0.0;     // jump back, kick muted from here
            head.ppq = ppq;
            const bool kickOn = b < bar;
            kick.run ([&] (juce::AudioBuffer<float>&, juce::MidiBuffer& m, int) { if (kickOn) kickNotes (ppq, m); }, b);
            duck.run ([&] (juce::AudioBuffer<float>& buf, juce::MidiBuffer&, int)
            { for (int i = 0; i < kBlock; ++i) for (int c = 0; c < 2; ++c) buf.setSample (c, i, 0.5f * std::sin (6.2831853f * 70.0f * (float) ((b * kBlock + i) / kSr))); }, b);
            if (b > bar + 8) minG = std::min (minG, duck.buf.getRMSLevel (0, 0, kBlock) / 0.3536f);
            ppq += kBlock * bpsH;
            simSec += kBlock / kSr;
        }
        std::printf ("   after jump back with kick muted: lowest duck gain %.2f (expect ~1)\n", minG);
        CHECK (minG > 0.8f, "stale kick reports duck after a transport jump (%.2f)", minG);
    }
    // 2. RUMBLE keeps triggering after a jump back.
    {
        resetBus();
        FakeHead head;
        Rig kick ("KICK", head), rum ("RUMBLE", head);
        double ppq = 0.0; float pk = 0.0f;
        const int bar = (int) (4.0 / bpsH / kBlock);
        for (int b = 0; b < bar * 3; ++b)
        {
            if (b == bar * 2) ppq = 0.0;
            head.ppq = ppq;
            kick.run ([&] (juce::AudioBuffer<float>&, juce::MidiBuffer& m, int) { kickNotes (ppq, m); }, b);
            rum.run ([] (juce::AudioBuffer<float>&, juce::MidiBuffer&, int) {}, b);
            if (b > bar * 2 + bar / 2) pk = std::max (pk, rum.buf.getMagnitude (0, 0, kBlock));
            ppq += kBlock * bpsH;
            simSec += kBlock / kSr;
        }
        std::printf ("   RUMBLE after jump back: peak %.1f dB\n", gainToDb (pk));
        CHECK (pk > 0.05f, "RUMBLE stopped triggering after a transport jump");
    }
    Bus::testClock = nullptr;
}

// --render MODULE SECONDS OUT.wav [source=chord|beat|silence] [sweep=id:from:to] [id=value ...]: one
// module at 48 kHz and 124 BPM into a 24-bit stereo WAV, for listening. An effect gets a test signal
// (chord: a held D minor 9 chord of band-limited saws; beat: the suite's kick, chord and hats;
// silence: nothing, to hear what the effect adds); an instrument gets the test MIDI. Values as --state (number in the parameter's unit, choice name, on/off). sweep
// ramps one parameter in a straight line across the render.
static int renderModule (int argc, char** argv)
{
    if (argc < 5) { std::fprintf (stderr, "usage: SFTests --render MODULE SECONDS OUT.wav [source=chord|beat|silence] [sweep=id:from:to] [id=value ...]\n"); return 2; }
    const std::string mid = argv[2];
    const double seconds = std::atof (argv[3]);
    const juce::File outFile (juce::File::getCurrentWorkingDirectory().getChildFile (argv[4]));
    const auto ids = allModuleIds();
    if (std::find (ids.begin(), ids.end(), mid) == ids.end()) { std::fprintf (stderr, "no module %s\n", mid.c_str()); return 2; }
    resetBus();
    FakeHead head;
    Rig r (mid, head);
    const auto& sp = r.p->allSpecs();
    std::string source = "chord", sweepId;
    float sweepFrom = 0.0f, sweepTo = 0.0f;
    auto setText = [&] (const std::string& key, const std::string& val) -> bool
    {
        const int k = r.p->paramIndex (key);
        if (k < 0) { std::fprintf (stderr, "%s has no parameter %s\n", mid.c_str(), key.c_str()); return false; }
        const auto& s = sp[(size_t) k];
        float v = 0.0f;
        if (s.type == PType::Choice)
        {
            const auto it = std::find (s.choices.begin(), s.choices.end(), val);
            if (it == s.choices.end()) { std::fprintf (stderr, "%s %s has no choice %s\n", mid.c_str(), key.c_str(), val.c_str()); return false; }
            v = (float) (it - s.choices.begin());
        }
        else if (s.type != PType::Float) v = val == "on" ? 1.0f : 0.0f;
        else v = juce::jlimit (s.min, s.max, std::stof (val));
        r.p->param (k)->setValueNotifyingHost (r.p->param (k)->convertTo0to1 (v));
        return true;
    };
    for (int a = 5; a < argc; ++a)
    {
        const std::string arg = argv[a];
        const auto eq = arg.find ('=');
        if (eq == std::string::npos) { std::fprintf (stderr, "not id=value: %s\n", arg.c_str()); return 2; }
        const auto key = arg.substr (0, eq), val = arg.substr (eq + 1);
        if (key == "source") { source = val; continue; }
        if (key == "sweep")
        {
            const auto c1 = val.find (':'), c2 = val.rfind (':');
            if (c1 == std::string::npos || c2 == c1) { std::fprintf (stderr, "sweep=id:from:to\n"); return 2; }
            sweepId = val.substr (0, c1); sweepFrom = std::stof (val.substr (c1 + 1, c2 - c1 - 1)); sweepTo = std::stof (val.substr (c2 + 1));
            if (r.p->paramIndex (sweepId) < 0) { std::fprintf (stderr, "%s has no parameter %s\n", mid.c_str(), sweepId.c_str()); return 2; }
            continue;
        }
        if (! setText (key, val)) return 2;
    }
    const bool inst = r.p->mod().instrument();
    const int total = (int) (seconds * kSr);
    juce::AudioBuffer<float> out (2, total);
    // D minor 9 (D3 F3 A3 C4 E4), each note two saws 7 cents apart, harmonics below 9 kHz.
    static const double chordHz[] = { 146.83, 174.61, 220.0, 261.63, 329.63 };
    double ph[5][2] {};
    auto chordAt = [&] (double t)
    {
        double y = 0.0;
        for (int nIdx = 0; nIdx < 5; ++nIdx)
            for (int d = 0; d < 2; ++d)
            {
                const double f = chordHz[nIdx] * (d == 0 ? 0.99796 : 1.00204);
                double s = 0.0;
                for (int h = 1; h * f < 9000.0; ++h) s += std::sin (ph[nIdx][d] * h) / h;
                y += s;
                ph[nIdx][d] += 2.0 * juce::MathConstants<double>::pi * f / kSr;
            }
        return (float) (y * 0.035 * (0.85 + 0.15 * std::sin (2.0 * juce::MathConstants<double>::pi * 0.25 * t)));
    };
    double ppq = 0.0;
    for (int pos = 0; pos < total; pos += kBlock)
    {
        const int n = std::min (kBlock, total - pos);
        if (! sweepId.empty())
        {
            const int k = r.p->paramIndex (sweepId);
            const float v = sweepFrom + (sweepTo - sweepFrom) * (float) pos / (float) total;
            r.p->param (k)->setValueNotifyingHost (r.p->param (k)->convertTo0to1 (v));
        }
        head.ppq = ppq;
        r.buf.setSize (r.buf.getNumChannels(), n, false, false, true);
        r.buf.clear();
        r.midi.clear();
        if (inst) fillMidi (r.midi, ppq, head.bpm, n);
        else if (source == "beat") fillAudio (r.buf, ppq, head.bpm, r.p->getTotalNumInputChannels());
        else if (source != "silence")
            for (int i = 0; i < n; ++i)
            {
                const float x = chordAt ((pos + i) / kSr);
                for (int c = 0; c < std::min (2, r.p->getTotalNumInputChannels()); ++c) r.buf.setSample (c, i, x);
            }
        r.p->processBlock (r.buf, r.midi);
        for (int c = 0; c < 2; ++c) out.copyFrom (c, pos, r.buf, std::min (c, r.buf.getNumChannels() - 1), 0, n);
        ppq += n * head.bpm / 60.0 / kSr;
    }
    outFile.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (outFile), kSr, 2, 24, {}, 0));
    if (w == nullptr) { std::fprintf (stderr, "can't write %s\n", outFile.getFullPathName().toRawUTF8()); return 1; }
    w->writeFromAudioSampleBuffer (out, 0, total);
    std::printf ("%s: %s, %.1f s, peak %.1f dB\n", outFile.getFullPathName().toRawUTF8(), mid.c_str(), seconds,
                 gainToDb (std::max (out.getMagnitude (0, 0, total), out.getMagnitude (1, 0, total))));
    return 0;
}

#include "QualityTests.h"
#include "RequestTests.h"
#include "SampleRegionTests.h"

// --capture IN.wav [IN2.wav] OUT.wav: SF TERROIR's import on real recordings, through the plugin's own
// path (the worker thread, the place as stored in the set). Prints what the status line would and a
// few measures of the place (balance, width, band levels); writes the place to OUT.wav (32-bit float).
static int captureFiles (int argc, char** argv)
{
    if (argc < 4) { std::fprintf (stderr, "usage: SFTests --capture IN.wav [IN2.wav] OUT.wav\n"); return 2; }
    const auto cwd = juce::File::getCurrentWorkingDirectory();
    juce::StringArray ins;
    for (int i = 2; i < argc - 1; ++i) ins.add (cwd.getChildFile (argv[i]).getFullPathName());
    const auto out = cwd.getChildFile (argv[argc - 1]);
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    for (auto& in : ins)
    {
        std::unique_ptr<juce::AudioFormatReader> rd (fm.createReaderFor (juce::File (in)));
        if (rd == nullptr) { std::fprintf (stderr, "%s: can't read it\n", in.toRawUTF8()); return 1; }
        const int nch = (int) rd->numChannels;
        const auto names = ir::trackNames (juce::File (in), rd->metadataValues.getValue (juce::WavAudioFormat::bwavDescription, {}), nch);
        const auto picks = ir::pickTracks (names, nch);
        std::printf ("%s: %d ch, %.0f Hz, %d bit%s, %.1f s; tracks %s; used",
                     juce::File (in).getFileName().toRawUTF8(), nch, rd->sampleRate, (int) rd->bitsPerSample, rd->usesFloatingPointData ? " float" : "",
                     (double) rd->lengthInSamples / rd->sampleRate, names.isEmpty() ? "(unnamed)" : names.joinIntoString (", ").toRawUTF8());
        for (int c : picks) std::printf (" %d", c + 1);
        std::printf ("\n");
        // The capture alone, for the numbers the stored place doesn't keep (the speaker's clock).
        if (ins.size() == 1)
        {
            const int len = (int) std::min<juce::int64> (rd->lengthInSamples, (juce::int64) ((ir::kMaxRecording + 1.0) * rd->sampleRate));
            juce::AudioBuffer<float> all (nch, len), b ((int) picks.size(), len);
            rd->read (all.getArrayOfWritePointers(), nch, 0, len);
            for (size_t c = 0; c < picks.size(); ++c) b.copyFrom ((int) c, 0, all, picks[c], 0, len);
            if (b.getNumChannels() == 2 && ir::sameChannels (b)) b.setSize (1, len, true);
            const auto cap = ir::analyse (b, rd->sampleRate);
            std::printf ("analysis: %s, clock %.1f ppm\n", cap.ok ? cap.kind.toRawUTF8() : cap.error.toRawUTF8(), cap.driftPpm);
        }
    }
    resetBus();
    Rig2 r ("TERROIR");
    r.set ("place", 0.0f);
    for (int b = 0; b < 4; ++b) r.step (kBlock, nullptr);
    r.p->mod().loadFiles (ins);
    juce::ValueTree place;
    const double until = juce::Time::getMillisecondCounterHiRes() + 300000.0;
    while (! place.isValid() && juce::Time::getMillisecondCounterHiRes() < until)
    {
        juce::Thread::sleep (50);
        r.step (kBlock, nullptr);
        juce::MemoryBlock state;
        r.p->getStateInformation (state);
        const auto m = juce::ValueTree::readFromData (state.getData(), state.getSize()).getChildWithName ("MODULE");
        for (int i = 0; i < m.getNumChildren(); ++i)
            if (m.getChild (i).hasType ("PLACE") && (int) m.getChild (i).getProperty ("slot") == 0) place = m.getChild (i).createCopy();
    }
    if (! place.isValid()) { std::printf ("no place stored (the import failed: an unreadable file, or no sweep or impulse found)\n"); return 1; }
    juce::AudioBuffer<float> a;
    double psr = (double) place.getProperty ("sr", 48000.0);
    if (const auto* mb = place.getProperty ("flac").getBinaryData())
    {
        juce::FlacAudioFormat flac;
        std::unique_ptr<juce::AudioFormatReader> rd (flac.createReaderFor (new juce::MemoryInputStream (*mb, true), true));
        if (rd != nullptr) { a.setSize ((int) rd->numChannels, (int) rd->lengthInSamples); rd->read (a.getArrayOfWritePointers(), a.getNumChannels(), 0, a.getNumSamples()); }
        a.applyGain ((float) (double) place.getProperty ("gain", 1.0));
    }
    const int n = a.getNumSamples(), chs = a.getNumChannels();
    std::printf ("place: %s, %s, %.2f s, RT %.2f s, %.0f dB over the noise\n", place.getProperty ("kind").toString().toRawUTF8(),
                 chs > 1 ? "STEREO" : "MONO > STEREO", n / psr, (double) place.getProperty ("rt60"), (double) place.getProperty ("snr"));
    auto energy = [&] (int c, double t0, double t1)
    {
        double e = 0.0;
        for (int i = (int) (t0 * psr); i < std::min (n, (int) (t1 * psr)); ++i) e += (double) a.getSample (c, i) * a.getSample (c, i);
        return e;
    };
    if (chs == 2)
    {
        auto db = [] (double x, double y) { return 10.0 * std::log10 ((x + 1e-30) / (y + 1e-30)); };
        double sxy = 0, sxx = 0, syy = 0;
        for (int i = (int) (0.08 * psr); i < n; ++i) { const double x = a.getSample (0, i), y = a.getSample (1, i); sxy += x * y; sxx += x * x; syy += y * y; }
        std::printf ("right against left: whole %+.1f dB, first 20 ms %+.1f dB, after 80 ms %+.1f dB; tail correlation %.2f\n",
                     db (energy (1, 0, 99), energy (0, 0, 99)), db (energy (1, 0, 0.02), energy (0, 0, 0.02)), db (energy (1, 0.08, 99), energy (0, 0.08, 99)),
                     sxy / std::sqrt (sxx * syy + 1e-30));
    }
    // Octave-band levels of the place (both sides summed), relative to the loudest band.
    {
        constexpr int order = 18, N = 1 << order;
        juce::dsp::FFT fft (order);
        std::vector<float> d ((size_t) N * 2, 0.0f);
        for (int i = 0; i < std::min (N, n); ++i) { float s = 0.0f; for (int c = 0; c < chs; ++c) s += a.getSample (c, i); d[(size_t) i] = s; }
        fft.performFrequencyOnlyForwardTransform (d.data());
        std::vector<double> lv;
        const double fcs[] = { 31.5, 63, 125, 250, 500, 1000, 2000, 4000, 8000, 16000 };
        for (double fc : fcs)
        {
            const int k0 = std::max (1, (int) (fc / std::sqrt (2.0) * N / psr)), k1 = std::min (N / 2 - 1, (int) (fc * std::sqrt (2.0) * N / psr));
            double e = 0.0;
            for (int k = k0; k <= k1; ++k) e += (double) d[(size_t) k] * d[(size_t) k];
            lv.push_back (10.0 * std::log10 (e + 1e-30));
        }
        const double top = *std::max_element (lv.begin(), lv.end());
        std::printf ("octave bands (dB under the loudest):");
        for (size_t k = 0; k < lv.size(); ++k) std::printf ("  %g Hz %.0f", fcs[k], lv[k] - top);
        std::printf ("\n");
    }
    out.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (out), psr, (unsigned) chs, 32, {}, 0));
    if (w != nullptr) w->writeFromAudioSampleBuffer (a, 0, n);
    std::printf ("wrote %s\n", out.getFullPathName().toRawUTF8());
    return 0;
}


// The editor's strip (Editor.h): every parameter it shows sits in one place; the E16 grid lights the
// scene's knobs for the plugin's track, at their encoders; folds are saved with the set.
static void testEditor()
{
    std::printf ("-- EDITOR (the strip)\n");
    SFEditor::testHeightLimit = 100000;           // the whole strip: no screen holds it here (testStripReach does)
    // The track rule GLIDE RINGS and the grid share.
    CHECK (juce::String (e16::trackFor ("LEAD", "32 STAB")) == "32 STAB", "LEAD on 32 STAB");
    CHECK (juce::String (e16::trackFor ("LEAD", "")) == "", "LEAD unnamed: on two tracks");
    CHECK (juce::String (e16::trackFor ("KICK", "ANY NAME")) == "01 KICK", "KICK: one track, no name needed");
    CHECK (juce::String (e16::trackFor ("TERROIR", "")) == "B SPACE", "TERROIR stands in on B SPACE");
    CHECK (e16::trackFor ("GLUE", "") == nullptr, "GLUE isn't on the E16");
    CHECK (e16::listHas ("REVERB|TERROIR", "TERROIR") && e16::listHas ("REVERB|TERROIR", "REVERB") && ! e16::listHas ("REVERB|TERROIR", "REV")
           && ! e16::listHas ("", "KICK"), "listHas");

    int worst = 0, grids = 0;
    std::string worstId;
    for (auto& id : allModuleIds())
    {
        // No name, a name the scene doesn't know, and each track the scene has it on.
        std::vector<std::string> names { "", "SOME TRACK" };
        for (const auto& k : kE16Knobs)
            if (id == k.module && std::find (names.begin(), names.end(), std::string (k.track)) == names.end()) names.push_back (k.track);
        for (auto& nm : names)
        {
            SFProcessor p (makeModule (id));
            if (! nm.empty()) { juce::AudioProcessor::TrackProperties tp; tp.name = nm; p.updateTrackProperties (tp); }
            std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
            auto& e = dynamic_cast<SFEditor&> (*ed);
            CHECK (e.getWidth() == SFEditor::kW, "%s: width %d", id.c_str(), e.getWidth());
            if (e.getHeight() > worst) { worst = e.getHeight(); worstId = id + " on '" + nm + "'"; }
            CHECK (e.getHeight() == SFEditor::kHeader + e.contentHeight(), "%s: the window is the whole strip when the screen holds it", id.c_str());
            // Every parameter it shows, once; nothing hidden.
            const auto pl = e.places();
            std::map<int, int> seen;
            std::map<int, juce::String> where;
            for (auto& x : pl) { ++seen[x.first]; where[x.first] = x.second; }
            const auto& sp = p.allSpecs();
            for (int i = 0; i < (int) sp.size(); ++i)
                CHECK (seen[i] == (sp[(size_t) i].hidden ? 0 : 1), "%s on '%s': %s shown %d times", id.c_str(), nm.c_str(), sp[(size_t) i].id.c_str(), seen[i]);
            // The grid: the scene's knobs for this track, on its page, at their encoders.
            const char* t = e16::trackFor (id.c_str(), nm.c_str());
            if (t == nullptr || t[0] == 0)
            {
                CHECK (e.e16Page() < 0, "%s on '%s': no grid", id.c_str(), nm.c_str());
                for (auto& x : pl) CHECK (! x.second.startsWith ("E16"), "%s on '%s': nothing on the E16", id.c_str(), nm.c_str());
                continue;
            }
            ++grids;
            CHECK (e.e16Page() == e16::homePage (id.c_str(), t) && e.e16Track() == t, "%s on '%s': page and track", id.c_str(), nm.c_str());
            const auto& pg = kE16Pages[e.e16Page()];
            int lit = 0;
            for (int enc = 0; enc < 16; ++enc)
            {
                const auto& c = pg.cells[enc];
                const juce::String at = "E16 " + juce::String (enc + 1);
                for (const auto* turn : { &c.turn, &c.second })
                    if (e16::turnIs (*turn, id.c_str(), t))
                    {
                        ++lit;
                        CHECK (where[turn->param] == at, "%s on %s: %s at %s, not %s", id.c_str(), t, sp[(size_t) turn->param].id.c_str(), at.toRawUTF8(), where[turn->param].toRawUTF8());
                    }
                if (e16::pressIs (c.press, id.c_str(), t))
                    CHECK (where[c.press.param] == at + " PRESS", "%s on %s: press %s", id.c_str(), t, sp[(size_t) c.press.param].id.c_str());
            }
            CHECK (lit > 0, "%s on %s: lit", id.c_str(), t);
        }
    }
    std::printf ("   %d grids; tallest strip at first: %s, %d px\n", grids, worstId.c_str(), worst);
    CHECK (grids >= 24, "grids: %d", grids);
    CHECK (worst <= 900, "every strip fits a laptop's screen at first: %d", worst);

    // DELAY on A DUB: TIME, FEEDBACK... on SPACE 9-12, THROW and FREEZE their presses.
    {
        SFProcessor p (makeModule ("DELAY"));
        juce::AudioProcessor::TrackProperties tp; tp.name = "A DUB"; p.updateTrackProperties (tp);
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        std::map<std::string, juce::String> at;
        for (auto& x : dynamic_cast<SFEditor&> (*ed).places()) at[p.allSpecs()[(size_t) x.first].id] = x.second;
        CHECK (at["time"] == "E16 9" && at["feedback"] == "E16 10" && at["throw"] == "E16 9 PRESS" && at["freeze"] == "E16 10 PRESS" && at["mix"] == "PERFORM"
               && at["dry"] == "SETUP" && at["m1_src"] == "MOTION" && at["slow"] == "SYSTEM" && at["slew"] == "DETAIL",
               "DELAY's places: %s %s %s %s", at["time"].toRawUTF8(), at["throw"].toRawUTF8(), at["mix"].toRawUTF8(), at["dry"].toRawUTF8());
    }

    // Folds: MOTION opens; saved with the set; a new window opens as it was left.
    {
        SFProcessor p (makeModule ("LEAD"));
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        auto& e = dynamic_cast<SFEditor&> (*ed);
        const int h0 = e.getHeight();
        CHECK (e.folded (SFEditor::kFoldMotion) && e.folded (SFEditor::kFoldSystem) && e.folded (SFEditor::kFoldSetup) && ! e.folded (SFEditor::kFoldDetail), "the folds at first");
        e.toggleFold (SFEditor::kFoldMotion);
        CHECK (! e.folded (SFEditor::kFoldMotion) && e.getHeight() > h0, "MOTION opens: %d -> %d", h0, e.getHeight());
        juce::MemoryBlock mb;
        p.getStateInformation (mb);
        SFProcessor q (makeModule ("LEAD"));
        q.setStateInformation (mb.getData(), (int) mb.getSize());
        std::unique_ptr<juce::AudioProcessorEditor> ed2 (q.createEditor());
        auto& e2 = dynamic_cast<SFEditor&> (*ed2);
        CHECK (! e2.folded (SFEditor::kFoldMotion) && e2.getHeight() == e.getHeight(), "the fold comes back with the set");
        e.toggleFold (SFEditor::kFoldMotion);
        CHECK (e.getHeight() == h0, "and folds again");
        juce::MemoryBlock mb2;
        p.getStateInformation (mb2);
        CHECK (! juce::ValueTree::readFromData (mb2.getData(), mb2.getSize()).hasProperty ("ui"), "the defaults save nothing");
    }
    SFEditor::testHeightLimit = 0;
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    if (argc > 1 && std::string (argv[1]) == "--dump") { dumpSpecs(); return 0; }
    if (argc > 1 && std::string (argv[1]) == "--state") return dumpState (argc, argv);
    if (argc > 1 && std::string (argv[1]) == "--render") return renderModule (argc, argv);
    if (argc > 1 && std::string (argv[1]) == "--capture") return captureFiles (argc, argv);
    if (argc > 2 && std::string (argv[1]) == "--sweep")
    {
        // SF TERROIR's sweep file (the same as its SWEEP FILE button writes).
        const auto b = ir::sweepFile (48000.0);
        const juce::File f (juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]));
        f.deleteFile();
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (f), 48000.0, 1, 24, {}, 0));
        if (w == nullptr) return 1;
        w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
        std::printf ("%s: %.1f s\n", f.getFullPathName().toRawUTF8(), b.getNumSamples() / 48000.0);
        return 0;
    }
    std::vector<std::string> ids = allModuleIds();
    if (argc > 1) { ids.clear(); for (int i = 1; i < argc; ++i) if (argv[i][0] != '-' && std::string (argv[i]).find_first_of ("abcdefghijklmnopqrstuvwxyz") == std::string::npos) ids.push_back (argv[i]); }
    std::printf ("SF LIVE SUITE offline tests (bus %s)\n", Bus::get().shared() ? "shared" : "local");
    for (auto& id : ids) testModule (id);
    if (argc <= 1) { testPocket(); testAlloc(); testSongs(); testSlowAndMacro(); testDensity(); testRegressions(); testFilterCrossing(); testSampleGate(); testStrip(); testTransportJumps(); testModels(); testDeck();
                     testOversampling(); testLimit(); testReverb(); testCalibration (false); testPresets(); testMotion(); testPanic(); testRates(); testReviewFixes(); testAuditFixes(); testSecondReview();
                     testThirdReview(); testDrop(); testSongWindow(); testGrooveFollow(); testSlowAll(); testSlowScope(); testControlAuthority(); testRecallHistory(); testModelChange(); testGlideRings(); testRadio(); testPolyrhythm(); testConvolverEngine(); testTerroir(); testIrImport(); testTerroirLifecycle(); testTerroirColdStart(); testCapture(); testCaptureLifecycle(); testEditor();
                     testCaptureRequests(); testStateAuthority(); testTerroirTempo(); testStripReach(); testSampleRegion(); }
    else if (std::string (argv[1]) == "--editor") { testEditor(); testStripReach(); testSampleRegion(); }
    else if (std::string (argv[1]) == "--requests") { testCaptureRequests(); }
    else if (std::string (argv[1]) == "--authority2") { testStateAuthority(); }
    else if (std::string (argv[1]) == "--tempo") { testTerroirTempo(); }
    else if (std::string (argv[1]) == "--reach") { testStripReach(); }
    else if (std::string (argv[1]) == "--region") { testSampleRegion(); }
    else if (std::string (argv[1]) == "--nov6") { testCaptureRequests(); testStateAuthority(); testTerroirTempo(); testStripReach(); }
    else if (std::string (argv[1]) == "--perf") { testSlowAndMacro(); testDensity(); testRegressions(); }
    else if (std::string (argv[1]) == "--models") { testModels(); }
    else if (std::string (argv[1]) == "--deck") { testDeck(); }
    else if (std::string (argv[1]) == "--filter") { testFilterCrossing(); }
    else if (std::string (argv[1]) == "--gate") { testSampleGate(); }
    else if (std::string (argv[1]) == "--strip") { testStrip(); }
    else if (std::string (argv[1]) == "--alloc") { testAlloc(); }
    else if (std::string (argv[1]) == "--jumps") { testTransportJumps(); testDeck(); }
    else if (std::string (argv[1]) == "--oversampling") { testOversampling(); }
    else if (std::string (argv[1]) == "--limit") { testLimit(); }
    else if (std::string (argv[1]) == "--reverb") { testReverb(); }
    else if (std::string (argv[1]) == "--calibration") { testCalibration (argc > 2 && std::string (argv[2]) == "--print"); }
    else if (std::string (argv[1]) == "--presets") { testPresets(); }
    else if (std::string (argv[1]) == "--motion") { testMotion(); }
    else if (std::string (argv[1]) == "--panic") { testPanic(); }
    else if (std::string (argv[1]) == "--rates") { testRates(); }
    else if (std::string (argv[1]) == "--cpu") { testCpu(); }
    else if (std::string (argv[1]) == "--review") { testReviewFixes(); testAuditFixes(); testSecondReview(); }
    else if (std::string (argv[1]) == "--audit") { testAuditFixes(); testSecondReview(); }
    else if (std::string (argv[1]) == "--second") { testSecondReview(); }
    else if (std::string (argv[1]) == "--third") { testThirdReview(); }
    else if (std::string (argv[1]) == "--drop") { testDrop(); }
    else if (std::string (argv[1]) == "--songwindow") { testSongWindow(); }
    else if (std::string (argv[1]) == "--groove") { testGrooveFollow(); }
    else if (std::string (argv[1]) == "--dropweight") { testDropWeight(); }
    else if (std::string (argv[1]) == "--authority") { testControlAuthority(); testRecallHistory(); }
    else if (std::string (argv[1]) == "--history") { testRecallHistory(); }
    else if (std::string (argv[1]) == "--modelchange") { testModelChange(); }
    else if (std::string (argv[1]) == "--slow") { testSlowAll(); testSlowScope(); testSlowAndMacro(); }
    else if (std::string (argv[1]) == "--scope") { testSlowScope(); }
    else if (std::string (argv[1]) == "--rings") { testGlideRings(); }
    else if (std::string (argv[1]) == "--radio") { testRadio(); }
    else if (std::string (argv[1]) == "--poly") { testPolyrhythm(); }
    else if (std::string (argv[1]) == "--terroir") { testConvolverEngine(); testTerroir(); testIrImport(); testTerroirLifecycle(); testTerroirColdStart(); testCapture(); testCaptureLifecycle(); }
    else if (std::string (argv[1]) == "--lifecycle") { testTerroirLifecycle(); testTerroirColdStart(); }
    else if (std::string (argv[1]) == "--coldstart") { testTerroirColdStart(); }
    else if (std::string (argv[1]) == "--import") { testIrImport(); }
    else if (std::string (argv[1]) == "--room") { testCapture(); testCaptureLifecycle(); }
    else if (std::string (argv[1]) == "--takes") { testCaptureLifecycle(); }
    else if (std::string (argv[1]) == "--quality") { testOversampling(); testLimit(); testReverb(); testCalibration (false); testPresets(); testMotion(); testPanic(); testRates(); }
    std::printf ("%s: %d failure(s)\n", failures == 0 ? "PASS" : "FAIL", failures);
    return failures == 0 ? 0 : 1;
}
