// Measurements behind the sound-quality and reliability claims (included by SuiteTests.cpp).
//   --oversampling  halfband responses (IIR 2x/4x, FIR 4x), synth alias floor at high notes
//   --limit         LIMIT output true peak (independent 8x sinc), BS.1770 meter reference
//   --reverb        REVERB RT60 vs DECAY, two-band decay with DARK
//   --calibration   loudness of every instrument model against its role target
//   --presets       every factory preset: ids exist, plays clean
//   --motion        MOTION shapes, determinism, KICK source
//   --panic         SF CONTROL PANIC silences tails and held notes
//   --rates         44.1 / 96 kHz, odd and huge block sizes, block-size independence
//   --cpu           real-time factor per module (README table)
#include "core/BandLimited.h"
#include "core/Loudness.h"
#include "core/Oversample.h"
#include "core/Resample.h"

struct Rig2
{
    std::unique_ptr<SFProcessor> p;
    juce::AudioBuffer<float> buf;
    juce::MidiBuffer midi;
    FakeHead head;
    double sr, ppq = 0.0;
    int block;
    Rig2 (const std::string& id, double rate = kSr, int blk = kBlock) : sr (rate), block (blk)
    {
        p = std::make_unique<SFProcessor> (makeModule (id));
        p->setPlayHead (&head);
        p->enableAllBuses();
        p->setRateAndBufferSizeDetails (rate, blk);
        p->prepareToPlay (rate, blk);
        buf.setSize (std::max (p->getTotalNumInputChannels(), p->getTotalNumOutputChannels()), blk);
    }
    void set (const char* pid, float v)
    {
        auto* prm = p->apvts.getParameter (pid);
        if (prm != nullptr) prm->setValueNotifyingHost (prm->convertTo0to1 (v));
        else { ++failures; std::printf ("  FAIL: no param %s\n", pid); }
    }
    double bps() const { return head.bpm / 60.0 / sr; }
    // One block of n samples; fill gets (buffer, midi, n).
    void step (int n, const std::function<void (juce::AudioBuffer<float>&, juce::MidiBuffer&, int)>& fill)
    {
        buf.setSize (buf.getNumChannels(), n, false, false, true);
        buf.clear();
        midi.clear();
        head.ppq = ppq;
        if (fill) fill (buf, midi, n);
        p->processBlock (buf, midi);
        ppq += n * bps();
    }
};

// Notes on a grid (every `every` beats, length `len` beats) for the block starting at ppq.
static void gridNotes (juce::MidiBuffer& m, double ppq, double bps, int n, double every, double len, int note, int vel)
{
    for (int i = 0; i < n; ++i)
    {
        const double q = ppq + i * bps, prev = q - bps;
        if (std::floor (q / every) != std::floor (prev / every) || (i == 0 && std::abs (q - std::round (q / every) * every) < 1e-9))
            m.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) vel), i);
        const double qo = q - len, po = prev - len;
        if (qo >= 0.0 && std::floor (qo / every) != std::floor (po / every)) m.addEvent (juce::MidiMessage::noteOff (1, note), i);
    }
}

static std::vector<float> magnitudeSpectrum (const std::vector<float>& x, int order)
{
    const int N = 1 << order;
    juce::dsp::FFT fft (order);
    std::vector<float> d ((size_t) N * 2, 0.0f);
    for (int i = 0; i < N && i < (int) x.size(); ++i)
    {
        // 4-term Blackman-Harris (sidelobes -92 dB)
        const double t = 2.0 * 3.14159265358979 * i / (N - 1);
        const double w = 0.35875 - 0.48829 * std::cos (t) + 0.14128 * std::cos (2 * t) - 0.01168 * std::cos (3 * t);
        d[(size_t) i] = (float) (x[(size_t) i] * w);
    }
    fft.performFrequencyOnlyForwardTransform (d.data());
    d.resize ((size_t) N / 2);
    return d;
}

// ------------------------------------------------------------------------------------------
static void testOversampling()
{
    std::printf ("-- oversampling: halfband responses\n");
    auto measure = [] (const std::function<void (float, float&, float&)>& up, double osr, double pass, double stopFrom, double& ripple, double& reject)
    {
        constexpr int order = 15, N = 1 << order;
        std::vector<float> h ((size_t) N, 0.0f);
        for (int i = 0; i < N / 2; ++i) { float a, b; up (i == 0 ? 1.0f : 0.0f, a, b); h[(size_t) (2 * i)] = a; h[(size_t) (2 * i + 1)] = b; }
        juce::dsp::FFT fft (order);
        std::vector<float> d ((size_t) N * 2, 0.0f);
        std::copy (h.begin(), h.end(), d.begin());
        fft.performFrequencyOnlyForwardTransform (d.data());
        double pmin = 1e9, pmax = 0, smax = 0;
        for (int k = 1; k < N / 2; ++k)
        {
            const double f = k * osr / N;
            const double mag = d[(size_t) k] * 0.5;          // interpolator gain 2
            if (f <= pass) { pmin = std::min (pmin, mag); pmax = std::max (pmax, mag); }
            if (f >= stopFrom) smax = std::max (smax, mag);
        }
        ripple = 20.0 * std::log10 (pmax / pmin);
        reject = -20.0 * std::log10 (smax + 1e-30);
    };
    for (double sr : { 44100.0, 48000.0, 96000.0 })
    {
        const auto d = HalfbandDesign::forStage (sr, 0);
        Halfband hb; hb.set (d);
        double ripple, reject;
        const double pass = std::min (20000.0, 0.45 * sr);
        measure ([&] (float x, float& a, float& b) { hb.up (x, a, b); }, 2.0 * sr, pass, sr - pass, ripple, reject);
        std::printf ("   IIR 2x @ %5.1f kHz: %2d coefs, passband ripple %.1e dB (0-20 kHz), images of 0-20 kHz down %.1f dB\n", sr / 1000.0, d.n, ripple, reject);
        CHECK (reject >= 100.0, "IIR halfband rejection only %.1f dB at %.0f Hz", reject, sr);
        CHECK (ripple < 0.01, "IIR halfband passband ripple %.3f dB", ripple);
    }
    {
        FirHalfband f; f.design (79, 0.0);
        double ripple, reject;
        measure ([&] (float x, float& a, float& b) { f.up (x, a, b); }, 96000.0, 20000.0, 28000.0, ripple, reject);
        std::printf ("   FIR 2x (LIMIT) @ 48 kHz: 79 taps, passband ripple %.3f dB, stopband %.1f dB\n", ripple, reject);
        CHECK (reject >= 85.0, "FIR halfband rejection only %.1f dB", reject);
        CHECK (ripple < 0.05, "FIR halfband ripple %.3f dB", ripple);
    }
    // The whole LIMIT path (4x up, identity, 4x down): flat to 20 kHz at every rate.
    for (double sr : { 44100.0, 48000.0, 96000.0 })
    {
        FirOversampler4x os; os.prepare (sr);
        double worst = 0.0;
        for (double f : { 100.0, 1000.0, 10000.0, 16000.0, 18000.0, 19000.0, 20000.0 })
        {
            os.reset();
            // Amplitude by Hann-windowed quadrature correlation (a sampled peak under-reads near
            // fs/3, fs/4 ...; a rectangular window leaks at a non-integer number of cycles).
            const int lat = (int) std::lround (os.latency()), from = lat + 2000, total = (int) sr / 2, len = total - from;
            double si = 0.0, co = 0.0, wsum = 0.0;
            for (int i = 0; i < total; ++i)
            {
                const float y = os.process (0, (float) std::sin (6.283185307 * f * i / sr), [] (float x) { return x; });
                if (i >= from)
                {
                    const double win = 0.5 - 0.5 * std::cos (6.283185307 * (i - from) / (len - 1)), w = 6.283185307 * f * i / sr;
                    si += win * y * std::sin (w); co += win * y * std::cos (w); wsum += win;
                }
            }
            const double amp = 2.0 * std::sqrt (si * si + co * co) / wsum;
            worst = std::max (worst, std::abs (20.0 * std::log10 (amp)));
        }
        std::printf ("   FIR 4x path (LIMIT) @ %5.1f kHz: worst deviation 0-20 kHz %.3f dB, latency %.1f samples\n", sr / 1000.0, worst, os.latency());
        CHECK (worst < 0.015, "LIMIT oversampling not flat to 20 kHz at %.0f Hz (%.3f dB)", sr, worst);
    }

    std::printf ("-- oversampling: synth alias floor (sustained note, filter open)\n");
    struct Case { const char* id; int model; const char* name; int note; float color; float detune = 0.0f; };
    const Case cases[] = { { "LEAD", 0, "ACID SAW", 72, 0.0f }, { "LEAD", 0, "ACID SAW", 84, 0.0f }, { "LEAD", 0, "ACID SAW", 96, 0.0f },
                           { "LEAD", 0, "ACID SQUARE", 84, 1.0f }, { "BASS", 1, "MONO 101", 72, 0.5f }, { "LEAD", 2, "MS20", 84, 0.5f },
                           { "LEAD", 8, "SYNC", 84, 0.6f }, { "LEAD", 7, "FM BELL", 84, 0.5f, 0.5f }, { "PAD", 0, "STRING MACHINE", 84, 0.5f },
                           { "BASS", 2, "SUB OSC", 72, 0.5f } };
    for (const auto& cs : cases)
    {
        resetBus();
        Rig2 r (cs.id);
        r.set ("model", (float) cs.model);
        r.set ("cutoff", 1.0f); r.set ("res", 0.0f); r.set ("env_amt", 0.0f); r.set ("drive", 0.0f);
        r.set ("color", cs.color); r.set ("glide", 0.0f); r.set ("pocket_depth", 0.0f); r.set ("decay", 1.0f);
        r.set ("detune", cs.detune); r.set ("space", 0.0f);
        std::vector<float> out;
        const int total = (int) (kSr * 1.6);
        for (int s = 0; s < total; s += kBlock)
        {
            r.step (kBlock, [&] (juce::AudioBuffer<float>&, juce::MidiBuffer& m, int) { if (s == 0) m.addEvent (juce::MidiMessage::noteOn (1, cs.note, (juce::uint8) 100), 0); });
            for (int i = 0; i < kBlock; ++i) out.push_back (r.buf.getSample (0, i));
        }
        out.erase (out.begin(), out.begin() + (int) (kSr * 0.3));
        const auto mag = magnitudeSpectrum (out, 16);
        const int N = 1 << 16;
        const double f0 = 440.0 * std::pow (2.0, (cs.note - 69) / 12.0);
        // Partials can be any multiple of f0 / 2 (sub oscillator) or detuned (unison): mark bins
        // near every multiple of f0/2 as wanted; everything else below 20 kHz is alias.
        double ref = 0.0;
        for (int k = 1; k < N / 2; ++k) ref = std::max (ref, (double) mag[(size_t) k]);
        double worst = 0.0;
        const double binHz = kSr / N;
        for (int k = 4; k < N / 2 && k * binHz < 20000.0; ++k)
        {
            const double f = k * binHz;
            const double h = f / (f0 * 0.5);
            const double dist = std::abs (h - std::round (h)) * f0 * 0.5;
            if (dist < 30.0 + (cs.model == 0 && std::string (cs.id) == "PAD" ? 60.0 : 0.0)) continue;
            worst = std::max (worst, (double) mag[(size_t) k]);
        }
        const double db = 20.0 * std::log10 (worst / ref + 1e-30);
        std::printf ("   %-6s %-15s %5.0f Hz: worst alias %.1f dB below the loudest partial\n", cs.id, cs.name, f0, -db);
        CHECK (db < -70.0, "%s %s aliases at %.1f dB", cs.id, cs.name, db);
    }
}

// ------------------------------------------------------------------------------------------
static double truePeak8x (const std::vector<float>& x)
{
    std::vector<float> up (x.size() * 8);
    SincResampler::process (x.data(), (int) x.size(), up.data(), (int) up.size(), 48000.0, 384000.0);
    float pk = 0.0f;
    for (size_t i = 512; i + 512 < up.size(); ++i) pk = std::max (pk, std::fabs (up[i]));
    return gainToDb (pk);
}

static void testLimit()
{
    std::printf ("-- LIMIT: true peak (independent 8x sinc) and BS.1770 meter\n");
    {
        LoudnessMeter m; m.prepare (48000.0);
        for (int i = 0; i < 48000 * 10; ++i) { const float v = 0.1f * (float) std::sin (2.0 * 3.14159265358979 * 997.0 * i / 48000.0); m.push (v, v); }
        std::printf ("   meter: 997 Hz sine at -20 dBFS, both channels: integrated %.2f LUFS (reference -20.00)\n", m.integrated);
        CHECK (std::abs (m.integrated + 20.0f) < 0.1f, "BS.1770 meter off (%.2f)", m.integrated);
    }
    {
        TruePeak tp;
        float pk = 0.0f;
        for (int i = 0; i < 48000; ++i) { const float v = (float) std::sin (2.0 * 3.14159265358979 * (12000.0 * i / 48000.0) + 0.785398); pk = std::max (pk, tp.push (v, v)); }
        std::printf ("   true-peak detector: fs/4 sine, sample peaks -3.0 dB, reads %.2f dBTP (reference 0.00)\n", gainToDb (pk));
        CHECK (std::abs (gainToDb (pk)) < 0.3f, "true-peak detector off (%.2f dB)", gainToDb (pk));
    }
    const char* names[] = { "PROGRAMME +12 dB", "FS/4 SINE +6 dB", "NOISE +12 dB" };
    for (int sig = 0; sig < 3; ++sig)
        for (float ceilDb : { -1.0f, -0.3f })
        {
            resetBus();
            Rig2 r ("LIMIT");
            r.set ("ceiling", ceilDb); r.set ("clip", 0.3f); r.set ("input", sig == 1 ? 6.0f : 12.0f); r.set ("auto", 0.0f);
            std::vector<float> outL;
            Noise nz;
            const int total = 48000 * 6;
            for (int s = 0; s < total; s += kBlock)
            {
                r.step (kBlock, [&] (juce::AudioBuffer<float>& b, juce::MidiBuffer&, int n)
                {
                    if (sig == 0) { fillAudio (b, r.ppq, r.head.bpm, 2); return; }
                    for (int i = 0; i < n; ++i)
                    {
                        const float v = sig == 1 ? (float) std::sin (2.0 * 3.14159265358979 * (12000.0 * (s + i) / 48000.0) + 0.785398) * 0.9f : nz.next() * 0.5f;
                        b.setSample (0, i, v); b.setSample (1, i, v);
                    }
                });
                for (int i = 0; i < kBlock; ++i) outL.push_back (r.buf.getSample (0, i));
            }
            outL.erase (outL.begin(), outL.begin() + 4800);
            const double tp = truePeak8x (outL);
            std::printf ("   %-16s ceiling %.1f dB: output true peak %.2f dBTP\n", names[sig], ceilDb, tp);
            CHECK (tp <= ceilDb + 0.15, "LIMIT true peak %.2f dBTP over ceiling %.1f", tp, ceilDb);
        }
}

// ------------------------------------------------------------------------------------------
// Schroeder backward integration, T30 extrapolated to 60 dB.
static double rt60 (const std::vector<float>& x, double sr)
{
    std::vector<double> e (x.size());
    double acc = 0.0;
    for (size_t i = x.size(); i-- > 0;) { acc += (double) x[i] * x[i]; e[i] = acc; }
    const double e0 = e[0];
    double t5 = -1, t35 = -1;
    for (size_t i = 0; i < e.size(); ++i)
    {
        const double db = 10.0 * std::log10 (e[i] / e0 + 1e-30);
        if (t5 < 0 && db <= -5.0) t5 = (double) i / sr;
        if (t35 < 0 && db <= -35.0) { t35 = (double) i / sr; break; }
    }
    return t35 > 0 && t5 >= 0 ? (t35 - t5) * 2.0 : -1.0;
}

static void testReverb()
{
    std::printf ("-- REVERB: RT60 (T30) vs DECAY, DARK = two-band decay\n");
    for (float decay : { 1.0f, 3.0f, 8.0f })
        for (float dark : { 0.0f, 1.0f })
        {
            resetBus();
            Rig2 r ("REVERB");
            r.set ("decay", decay); r.set ("dark", dark); r.set ("mix", 1.0f); r.set ("duck", 0.0f);
            r.set ("mod", 0.0f); r.set ("lo_cut", 20.0f); r.set ("predelay", 0.0f); r.set ("size", 0.6f);
            std::vector<float> wet;
            const int lat = r.p->getLatencySamples();
            const int total = (int) (48000 * (decay * 1.6 + 1.0));
            Noise nz;
            for (int s = 0; s < total; s += kBlock)
            {
                r.step (kBlock, [&] (juce::AudioBuffer<float>& b, juce::MidiBuffer&, int n)
                {
                    for (int i = 0; i < n; ++i) { const float v = (s + i) < 480 ? nz.next() * 0.5f : 0.0f; b.setSample (0, i, v); b.setSample (1, i, v); }
                });
                for (int i = 0; i < kBlock; ++i) wet.push_back (r.buf.getSample (0, i) + r.buf.getSample (1, i));
            }
            // Drop the dry burst (latency + 10 ms) and measure the tail; also its low and high bands.
            wet.erase (wet.begin(), wet.begin() + lat + 600);
            std::vector<float> lo (wet.size()), hi (wet.size());
            Svf a, b; a.set (500.0f, 0.7071f, 48000.0); b.set (5000.0f, 0.7071f, 48000.0);
            for (size_t i = 0; i < wet.size(); ++i) { lo[i] = a.tickLP (wet[i]); hi[i] = b.tickHP (wet[i]); }
            const double tAll = rt60 (wet, 48000.0), tLo = rt60 (lo, 48000.0), tHi = rt60 (hi, 48000.0);
            std::printf ("   DECAY %.1f s DARK %3.0f %%: RT60 %.2f s (lows %.2f s, highs %.2f s)\n", decay, dark * 100.0f, tAll, tLo, tHi);
            CHECK (tLo > decay * 0.75 && tLo < decay * 1.3, "REVERB low RT60 %.2f s for DECAY %.1f", tLo, decay);
            if (dark > 0.5f) CHECK (tHi < tLo * 0.5, "DARK did not shorten the high decay (%.2f vs %.2f)", tHi, tLo);
        }
}

// ------------------------------------------------------------------------------------------
// Loudness of every instrument model at default settings, playing a role pattern.
struct RoleTarget { const char* id; float lufs; double every, len; int note, vel; };
static const RoleTarget kTargets[] = {
    { "KICK", -15.0f, 1.0, 0.25, 36, 110 }, { "PERC", -22.0f, 0.5, 0.125, 42, 100 },
    { "RUMBLE", -20.0f, 1.0, 0.25, 36, 110 }, { "BASS", -18.0f, 0.25, 0.125, 36, 100 },
    { "LEAD", -20.0f, 0.5, 0.25, 48, 100 }, { "PAD", -22.0f, 4.0, 3.9, 48, 90 },
    { "VOICE", -20.0f, 4.0, 3.9, 48, 100 }, { "STRIKE", -20.0f, 1.0, 0.5, 60, 100 } };

static float measureModel (const RoleTarget& t, int model)
{
    resetBus();
    Rig2 r (t.id);
    if (auto* m = r.p->apvts.getParameter ("model")) m->setValueNotifyingHost (m->convertTo0to1 ((float) model));
    if (std::string (t.id) == "RUMBLE") r.set ("source", 1.0f);
    LoudnessMeter lm; lm.prepare (kSr);
    const int total = (int) (kSr * 10.0);
    for (int s = 0; s < total; s += kBlock)
    {
        r.step (kBlock, [&] (juce::AudioBuffer<float>&, juce::MidiBuffer& m, int n) { gridNotes (m, r.ppq, r.bps(), n, t.every, t.len, t.note, t.vel); });
        if (s > kSr * 1.0) for (int i = 0; i < kBlock; ++i) lm.push (r.buf.getSample (0, i), r.buf.getSample (1, i));
    }
    return lm.integrated;
}

// VOICE phrase models against plain SAMPLE playback, with a synthetic spoken phrase.
static void calibrateVoicePhrase (bool printCorrections)
{
    juce::File wav = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("sf_test_phrase.wav");
    {
        const int n = (int) (kSr * 4.0);
        juce::AudioBuffer<float> b (2, n);
        Noise nz; Svf f1, f2, f3; f1.set (700.0f, 6.0f, kSr); f2.set (1200.0f, 6.0f, kSr); f3.set (2600.0f, 6.0f, kSr);
        double ph = 0.0;
        for (int i = 0; i < n; ++i)
        {
            const double t = i / kSr;
            const float syl = (float) std::pow (std::max (0.0, std::sin (2.0 * 3.14159265 * 4.0 * t)), 2.0) * (t < 3.6 ? 1.0f : 0.0f);
            ph += (120.0 + 20.0 * std::sin (t * 3.0)) / kSr; if (ph > 1.0) ph -= 1.0;
            const float src = (float) (2.0 * ph - 1.0) * 0.6f + nz.next() * 0.2f;
            const float v = (f1.tickBP (src) + 0.7f * f2.tickBP (src) + 0.4f * f3.tickBP (src)) * syl * 0.5f;
            b.setSample (0, i, v); b.setSample (1, i, v);
        }
        wav.deleteFile();
        juce::WavAudioFormat fmt;
        std::unique_ptr<juce::AudioFormatWriter> w (fmt.createWriterFor (new juce::FileOutputStream (wav), kSr, 2, 24, {}, 0));
        w->writeFromAudioSampleBuffer (b, 0, n);
    }
    const char* names[6] = { "SAMPLE", "RADIO", "VOCODER", "ROBOT", "CHOIR", "STUTTER" };
    float ref = 0.0f;
    juce::String line;
    for (int m = 0; m < 6; ++m)
    {
        resetBus();
        Rig2 r ("VOICE");
        r.p->mod().loadFiles (juce::StringArray (wav.getFullPathName()));
        r.set ("model", (float) m); r.set ("slice_mode", 3.0f); r.set ("gate", 1.0f); r.set ("echo", 0.0f);
        LoudnessMeter lm; lm.prepare (kSr);
        for (int s = 0; s < (int) (kSr * 9.0); s += kBlock)
        {
            r.step (kBlock, [&] (juce::AudioBuffer<float>&, juce::MidiBuffer& mb, int n) { gridNotes (mb, r.ppq, r.bps(), n, 8.0, 7.5, 48, 100); });
            if (s > kSr * 0.5) for (int i = 0; i < kBlock; ++i) lm.push (r.buf.getSample (0, i), r.buf.getSample (1, i));
        }
        if (m == 0) ref = lm.integrated;
        line << names[m] << " " << juce::String (lm.integrated - ref, 1) << "  ";
        if (printCorrections) std::printf ("   CORR VOICE phrase %s %.2f dB\n", names[m], ref - lm.integrated);
        if (m > 0) CHECK (std::abs (lm.integrated - ref) <= 1.5f, "VOICE %s phrase level %.1f dB off SAMPLE", names[m], lm.integrated - ref);
    }
    std::printf ("   VOICE phrase models vs SAMPLE (dB): %s\n", line.toRawUTF8());
}

static void testCalibration (bool printCorrections)
{
    std::printf ("-- calibration: integrated loudness per model, default settings (target per role, +-1.5 dB)\n");
    for (const auto& t : kTargets)
    {
        resetBus();
        SFProcessor probe (makeModule (t.id));
        auto* mp = dynamic_cast<juce::AudioParameterChoice*> (probe.apvts.getParameter ("model"));
        const int numModels = mp ? mp->choices.size() : 1;
        juce::String line;
        for (int m = 0; m < numModels; ++m)
        {
            const juce::String nm = mp ? mp->choices[m] : juce::String (t.id);
            if (nm == "SAMPLE" || nm == "STUTTER") continue;           // depend on the user's sample
            const float l = measureModel (t, m);
            const float err = l - t.lufs;
            line << nm << " " << juce::String (l, 1) << (std::abs (err) > 1.5f ? "*" : "") << "  ";
            if (printCorrections) std::printf ("   CORR %s %d %s %.2f dB\n", t.id, m, nm.toRawUTF8(), -err);
            CHECK (std::abs (err) <= 1.5f, "%s %s at %.1f LUFS (target %.1f)", t.id, nm.toRawUTF8(), l, t.lufs);
        }
        std::printf ("   %-7s target %5.1f: %s\n", t.id, t.lufs, line.toRawUTF8());
    }
    calibrateVoicePhrase (printCorrections);
}

// ------------------------------------------------------------------------------------------
static void testPresets()
{
    std::printf ("-- factory presets\n");
    int total = 0;
    for (auto& id : allModuleIds())
    {
        const auto presets = factoryPresets (id);
        if (presets.empty()) continue;
        resetBus();
        Rig2 r (id);
        const bool inst = r.p->mod().instrument();
        int bad = 0;
        float minPk = 1e9f, maxPk = 0.0f;
        for (const auto& pr : presets)
        {
            for (auto& kv : pr.values)
                if (r.p->paramIndex (kv.first) < 0) { ++failures; std::printf ("  FAIL: %s preset %s: unknown parameter %s\n", id.c_str(), pr.name.c_str(), kv.first.c_str()); }
            r.p->applyPreset (pr);
            if (id == "RUMBLE") r.set ("source", 1.0f);           // MIDI triggers (no SF KICK here)
            r.p->panic();
            float pk = 0.0f;
            bool nan = false;
            const int blocks = (int) (kSr * 3.0 / kBlock);
            for (int b = 0; b < blocks; ++b)
            {
                r.step (kBlock, [&] (juce::AudioBuffer<float>& buf, juce::MidiBuffer& m, int n)
                {
                    if (inst) { gridNotes (m, r.ppq, r.bps(), n, 0.5, 0.25, id == "KICK" || id == "RUMBLE" ? 36 : 48, 100); }
                    else fillAudio (buf, r.ppq, r.head.bpm, 2);
                });
                if (b > blocks / 3)
                    for (int c = 0; c < 2; ++c)
                    {
                        pk = std::max (pk, r.buf.getMagnitude (c, 0, kBlock));
                        for (int i = 0; i < kBlock; ++i) if (! std::isfinite (r.buf.getSample (c, i))) nan = true;
                    }
            }
            const bool silentOk = id == "DECK" || id == "CONTROL";
            if (nan || pk >= 3.99f || (! silentOk && pk < 0.001f))
            {
                ++bad; ++failures;
                std::printf ("  FAIL: %s preset %s: peak %.1f dB%s\n", id.c_str(), pr.name.c_str(), gainToDb (pk), nan ? " NaN" : "");
            }
            minPk = std::min (minPk, pk); maxPk = std::max (maxPk, pk);
            if (getenv ("SF_PRESET_PEAKS")) std::printf ("      %-24s %6.1f dB\n", pr.name.c_str(), gainToDb (pk));
            ++total;
        }
        std::printf ("   %-8s %2d presets, peaks %.1f .. %.1f dB%s\n", id.c_str(), (int) presets.size(), gainToDb (minPk), gainToDb (maxPk), bad ? "  (FAILURES)" : "");
    }
    std::printf ("   %d factory presets\n", total);
}

// ------------------------------------------------------------------------------------------
static void testMotion()
{
    std::printf ("-- MOTION\n");
    {
        resetBus();
        Rig2 r ("FILTER");
        const int fi = r.p->paramIndex ("filter");
        r.set ("filter", 0.5f);
        r.set ("m1_src", 1.0f);                       // SINE
        r.set ("m1_rate", 4.0f);                      // 1 BAR
        r.set ("m1_target", 1.0f);                    // first target: FILTER
        r.set ("m1_depth", 0.25f);
        float at[4] {};
        const double blockBeats = r.bps() * kBlock;
        for (int b = 0; b < (int) (8.0 / blockBeats) + 4; ++b)
        {
            r.step (kBlock, nullptr);          // currentValue = value at the end of the block (r.ppq)
            for (int q = 0; q < 4; ++q) if (std::abs (r.ppq - (double) q - 4.0) <= blockBeats * 0.5) at[q] = r.p->currentValue (fi);
        }
        std::printf ("   SINE 1 BAR, depth 25 %% on FILTER 50 %%: beat 1 %.3f, 2 %.3f, 3 %.3f, 4 %.3f (expect .50 .75 .50 .25)\n", at[0], at[1], at[2], at[3]);
        CHECK (std::abs (at[1] - 0.75f) < 0.03f && std::abs (at[3] - 0.25f) < 0.03f && std::abs (at[2] - 0.5f) < 0.03f, "MOTION sine wrong");
    }
    {
        // RANDOM: the same instance state plays the same moves; another instance differs.
        auto run = [] (const juce::MemoryBlock* state)
        {
            resetBus();
            Rig2 r ("FILTER");
            if (state) r.p->setStateInformation (state->getData(), (int) state->getSize());
            r.set ("m1_src", 6.0f); r.set ("m1_rate", 1.0f); r.set ("m1_target", 1.0f); r.set ("m1_depth", 0.5f);
            std::vector<float> v;
            for (int b = 0; b < 200; ++b) { r.step (kBlock, nullptr); v.push_back (r.p->currentValue (0)); }
            juce::MemoryBlock mb; r.p->getStateInformation (mb);
            return std::make_pair (v, mb);
        };
        auto a = run (nullptr);
        auto b = run (&a.second);
        auto c = run (nullptr);
        float dAB = 0, dAC = 0;
        for (size_t i = 0; i < a.first.size(); ++i) { dAB = std::max (dAB, std::abs (a.first[i] - b.first[i])); dAC = std::max (dAC, std::abs (a.first[i] - c.first[i])); }
        std::printf ("   RANDOM: same set reloaded differs by %.4f, another instance by %.3f\n", dAB, dAC);
        CHECK (dAB < 1e-4f, "MOTION RANDOM not reproducible after reload");
        CHECK (dAC > 0.05f, "MOTION RANDOM identical across instances");
    }
    {
        resetBus();
        Rig2 kick ("KICK"), f ("FILTER");
        f.set ("m1_src", 8.0f); f.set ("m1_rate", 2.0f); f.set ("m1_target", 1.0f); f.set ("m1_depth", 0.4f);
        float lo = 1.0f, hi = 0.0f;
        for (int b = 0; b < (int) (kSr * 4.0 / kBlock); ++b)
        {
            kick.step (kBlock, [&] (juce::AudioBuffer<float>&, juce::MidiBuffer& m, int n) { gridNotes (m, kick.ppq, kick.bps(), n, 1.0, 0.25, 36, 120); });
            f.step (kBlock, [&] (juce::AudioBuffer<float>& bb, juce::MidiBuffer&, int) { fillAudio (bb, f.ppq, f.head.bpm, 2); });
            if (b > 100) { lo = std::min (lo, f.p->currentValue (0)); hi = std::max (hi, f.p->currentValue (0)); }
        }
        std::printf ("   KICK source, depth 40 %%: FILTER moves %.2f .. %.2f with the kicks\n", lo, hi);
        CHECK (hi - lo > 0.2f, "MOTION KICK source did not follow SF KICK");
    }
}

// ------------------------------------------------------------------------------------------
static void testPanic()
{
    std::printf ("-- PANIC (SF CONTROL)\n");
    resetBus();
    Rig2 ctl ("CONTROL"), dly ("DELAY"), pad ("PAD"), rev ("REVERB");
    dly.set ("feedback", 1.1f); dly.set ("mix", 1.0f);
    rev.set ("decay", 20.0f); rev.set ("mix", 1.0f);
    float before[3] {}, after[3] {};
    const int blocks = (int) (kSr * 3.0 / kBlock), panicAt = (int) (kSr * 2.0 / kBlock), burstEnd = (int) (kSr * 1.2 / kBlock);
    for (int b = 0; b < blocks; ++b)
    {
        if (b == panicAt) ctl.set ("panic", 1.0f);
        if (b == panicAt + 2) ctl.set ("panic", 0.0f);
        ctl.step (kBlock, nullptr);
        auto burst = [&] (juce::AudioBuffer<float>& bb, juce::MidiBuffer&, int) { if (b < burstEnd) fillAudio (bb, dly.ppq, 124.0, 2); };
        dly.step (kBlock, burst);
        rev.step (kBlock, burst);
        pad.step (kBlock, [&] (juce::AudioBuffer<float>&, juce::MidiBuffer& m, int) { if (b == 0) m.addEvent (juce::MidiMessage::noteOn (1, 48, (juce::uint8) 100), 0); });
        Rig2* rr[3] = { &dly, &rev, &pad };
        for (int k = 0; k < 3; ++k)
        {
            const float rms = std::max (rr[k]->buf.getRMSLevel (0, 0, kBlock), rr[k]->buf.getRMSLevel (1, 0, kBlock));
            if (b > burstEnd + 10 && b < panicAt) before[k] = std::max (before[k], rms);
            if (b > panicAt + 2) after[k] = std::max (after[k], rms);
        }
    }
    const char* nm[3] = { "DELAY (feedback 110 %)", "REVERB (20 s)", "PAD (held note)" };
    for (int k = 0; k < 3; ++k)
    {
        std::printf ("   %-22s before %.1f dB, after PANIC %.1f dB\n", nm[k], gainToDb (before[k]), gainToDb (after[k]));
        CHECK (before[k] > 0.003f && after[k] < 1e-4f, "PANIC did not silence %s", nm[k]);
    }
}

// ------------------------------------------------------------------------------------------
static void testRates()
{
    std::printf ("-- sample rates and block sizes\n");
    for (double sr : { 44100.0, 96000.0 })
    {
        int bad = 0;
        for (auto& id : allModuleIds())
        {
            resetBus();
            Rig2 r (id, sr, 512);
            const bool inst = r.p->mod().instrument();
            bool nan = false; float pk = 0.0f;
            for (int s = 0; s < (int) (sr * 2.0); s += 512)
            {
                r.step (512, [&] (juce::AudioBuffer<float>& b, juce::MidiBuffer& m, int n)
                {
                    if (inst) gridNotes (m, r.ppq, r.bps(), n, 0.5, 0.25, 48, 100);
                    else for (int i = 0; i < n; ++i) { const float v = 0.3f * (float) std::sin (2.0 * 3.14159265 * 110.0 * (s + i) / sr) + ((s + i) % 24000 < 600 ? 0.5f : 0.0f); b.setSample (0, i, v); b.setSample (1, i, v); }
                });
                for (int c = 0; c < 2; ++c) { pk = std::max (pk, r.buf.getMagnitude (c, 0, 512)); for (int i = 0; i < 512; ++i) if (! std::isfinite (r.buf.getSample (c, i))) nan = true; }
            }
            if (nan || pk >= 3.99f) { ++bad; ++failures; std::printf ("  FAIL: %s at %.0f Hz: peak %.1f dB%s\n", id.c_str(), sr, gainToDb (pk), nan ? " NaN" : ""); }
        }
        std::printf ("   %.1f kHz: %d modules clean\n", sr / 1000.0, (int) allModuleIds().size() - bad);
    }
    // Block-size independence: the same performance at block 64 and odd / huge blocks.
    int identical = 0, checked = 0;
    for (const auto& idS : allModuleIds())
    {
        const char* id = idS.c_str();
        auto render = [&] (const std::vector<int>& sizes)
        {
            resetBus();
            Rig2 r (id, kSr, 2048);
            const bool inst = r.p->mod().instrument();
            std::vector<float> out;
            size_t k = 0;
            for (int s = 0; s < (int) (kSr * 1.0);)
            {
                const int n = sizes[k++ % sizes.size()];
                r.step (n, [&] (juce::AudioBuffer<float>& b, juce::MidiBuffer& m, int nn)
                {
                    if (inst) gridNotes (m, r.ppq, r.bps(), nn, 0.5, 0.25, 36, 100);
                    else
                        for (int i = 0; i < nn; ++i)
                        {
                            // Deterministic programme: kick pulses, two tones, hashed noise.
                            const int64_t idx = s + i;
                            const double q = r.ppq + i * r.bps(), ph = q - std::floor (q);
                            const float t = (float) (ph * 60.0 / r.head.bpm);
                            uint32_t h = (uint32_t) idx * 2654435761u; h ^= h >> 15; h *= 2246822519u; h ^= h >> 13;
                            const float v = std::sin (6.2831853f * 55.0f * t) * std::exp (-t / 0.15f) * 0.6f
                                          + 0.08f * (float) std::sin (6.2831853 * 220.0 * idx / kSr) + 0.05f * ((float) (h & 0xffff) / 32768.0f - 1.0f);
                            b.setSample (0, i, v); b.setSample (1, i, v);
                        }
                });
                for (int i = 0; i < n; ++i) out.push_back (r.buf.getSample (0, i));
                s += n;
            }
            return out;
        };
        const auto ref = render ({ 64 });
        double worst = -300.0;
        for (const auto& sizes : std::vector<std::vector<int>> { { 1, 7, 333 }, { 2048 }, { 17, 1000, 3 } })
        {
            const auto o = render (sizes);
            double e = 0, s = 0;
            for (size_t i = 0; i < std::min (o.size(), ref.size()); ++i) { e += (o[i] - ref[i]) * (o[i] - ref[i]); s += ref[i] * ref[i]; }
            worst = std::max (worst, 10.0 * std::log10 (e / (s + 1e-30) + 1e-30));
        }
        ++checked;
        if (worst <= -299.0) ++identical;
        else std::printf ("   %-8s differs across block sizes by %.1f dB\n", id, worst);
        CHECK (worst < -90.0, "%s depends on the block size (%.1f dB)", id, worst);
    }
    std::printf ("   block sizes 1, 3, 7, 17, 64, 333, 1000, 2048: %d of %d modules bit-identical\n", identical, checked);
}

// ------------------------------------------------------------------------------------------
// Regressions for the Nov 3 review: each test fails on the code before its fix.
static void testReviewFixes()
{
    std::printf ("-- review regressions\n");
    auto& d = Bus::get().d();

    // 1. Song recall lands on the bar line at any block size and song position (it used to slip a
    //    whole bar when ppq rounding put the chunk cut one sample past the bar).
    {
        int late = 0, runs = 0;
        double worstErr = 0.0;
        for (int blk : { 333, 17, 64, 1000, 2048 })
            for (double startPpq : { 0.37, 500.37, 7777.13, 30001.9 })
            {
                resetBus();
                Rig2 r ("FILTER", kSr, blk);
                r.head.bpm = 128.0;
                r.ppq = startPpq;
                d.song = 1;
                auto dc = [&] (juce::AudioBuffer<float>& b, juce::MidiBuffer&, int n) { for (int c = 0; c < 2; ++c) for (int i = 0; i < n; ++i) b.setSample (c, i, 0.5f); };
                r.set ("level", -6.0f);
                r.step (blk, dc);
                r.p->storeSong();
                r.step (blk, dc);
                r.set ("level", 0.0f);
                for (int k = 0; k < (int) (0.3 * kSr) / blk + 2; ++k) r.step (blk, dc);       // settle
                // Request the recall mid-bar, then find the first sample that moves.
                const double reqPpq = r.ppq;
                d.songSeq.fetch_add (1);
                const double barPpq = std::ceil (reqPpq / 4.0) * 4.0;
                const double expect = (barPpq - reqPpq) / r.bps();      // samples from the request
                double found = -1.0;
                int64_t pos = 0;
                for (int k = 0; k < (int) (kSr * 12.0) / blk && found < 0.0; ++k)
                {
                    r.step (blk, dc);
                    for (int i = 0; i < blk; ++i, ++pos)
                        if (found < 0.0 && r.buf.getSample (0, i) < 0.5f * (1.0f - 1e-4f)) found = (double) pos;
                }
                ++runs;
                const double err = found - std::ceil (expect - 1e-3);
                worstErr = std::max (worstErr, std::abs (err));
                if (found < 0.0 || std::abs (err) > 1.0) ++late;
            }
        std::printf ("   song recall on the bar: %d of %d runs late (worst %.0f samples)\n", late, runs, worstErr);
        CHECK (late == 0, "song recall missed the bar line in %d runs (worst %.0f samples)", late, worstErr);
    }

    // 2. KEY is ignored when SF CONTROL is not running (the shared bus keeps the last key forever).
    {
        auto render = [&] (int key, bool alive)
        {
            resetBus();
            d.keyRoot = key;
            Rig2 r ("BASS");
            r.set ("model", 5.0f);          // SINE SUB
            std::vector<float> out;
            for (int b = 0; b < 40; ++b)
            {
                if (alive) d.controlSeenMs = Bus::nowMs();
                r.step (kBlock, [&] (juce::AudioBuffer<float>&, juce::MidiBuffer& m, int n) { gridNotes (m, r.ppq, r.bps(), n, 1.0, 0.75, 36, 100); });
                for (int i = 0; i < kBlock; ++i) out.push_back (r.buf.getSample (0, i));
            }
            d.keyRoot = -1;
            return out;
        };
        const auto ref = render (-1, false), dead = render (5, false), alive = render (5, true);
        double eD = 0, eA = 0, s = 0;
        for (size_t i = 0; i < ref.size(); ++i) { eD += (dead[i] - ref[i]) * (dead[i] - ref[i]); eA += (alive[i] - ref[i]) * (alive[i] - ref[i]); s += ref[i] * ref[i]; }
        std::printf ("   KEY F, CONTROL gone: difference %.1f dB (want none); CONTROL running: %.1f dB (want transposed)\n",
                     10.0 * std::log10 (eD / s + 1e-30), 10.0 * std::log10 (eA / s + 1e-30));
        CHECK (eD == 0.0, "a stale KEY on the bus transposed BASS without SF CONTROL");
        CHECK (eA > 0.1 * s, "KEY did not transpose BASS with SF CONTROL running");
    }

    // 3. KICK LAYER: the main voice keeps playing (909 layered with itself = the same kick).
    {
        auto bodyRms = [&] (float layer)
        {
            resetBus();
            Rig2 r ("KICK");
            r.set ("model", 0.0f); r.set ("layer_model", 0.0f); r.set ("layer", layer); r.set ("click", 0.0f);
            double e = 0.0; int cnt = 0;
            for (int b = 0; b < (int) (kSr * 0.4) / kBlock; ++b)
            {
                r.step (kBlock, [&] (juce::AudioBuffer<float>&, juce::MidiBuffer& m, int) { if (b == 0) m.addEvent (juce::MidiMessage::noteOn (1, 36, (juce::uint8) 127), 0); });
                for (int i = 0; i < kBlock; ++i) { const int t = b * kBlock + i; if (t > (int) (0.01 * kSr)) { e += r.buf.getSample (0, i) * r.buf.getSample (0, i); ++cnt; } }
            }
            return 10.0 * std::log10 (e / cnt + 1e-30);
        };
        const double single = bodyRms (0.0), layered = bodyRms (1.0);
        std::printf ("   KICK 909 layered with 909: body %.2f dB vs single %.2f dB\n", layered, single);
        CHECK (std::abs (layered - single) < 0.5, "KICK LAYER loses the main voice (%.1f dB vs %.1f dB)", layered, single);
    }

    // 4. Drive stages: no delay or level step when the drive leaves 0 (ENERGY crossing centre,
    //    FILTER / STRIP drive turned up from 0). A 1 kHz sine; the third difference of the output
    //    stays near that of a clean sine.
    for (const char* id : { "ENERGY", "FILTER", "STRIP" })
    {
        resetBus();
        Rig2 r (id);
        if (std::string (id) == "ENERGY") { r.set ("drive_amt", 1.0f); r.set ("energy", 0.47f); }
        else r.set ("drive", 0.0f);
        float worst = 0.0f, ref = 0.0f;
        float y1 = 0, y2 = 0, y3 = 0;
        int64_t t = 0;
        for (int b = 0; b < (int) (kSr * 1.5) / 64; ++b)
        {
            if (b == (int) (kSr * 0.5) / 64)
            {
                if (std::string (id) == "ENERGY") r.set ("energy", 0.53f);
                else r.set ("drive", 0.15f);
            }
            r.step (64, [&] (juce::AudioBuffer<float>& bb, juce::MidiBuffer&, int n)
            {
                for (int i = 0; i < n; ++i) { const float v = 0.25f * (float) std::sin (6.283185307 * 1000.0 * (double) (t + i) / kSr); bb.setSample (0, i, v); bb.setSample (1, i, v); }
            });
            for (int i = 0; i < 64; ++i, ++t)
            {
                const float y = r.buf.getSample (0, i), d3 = std::fabs (y - 3.0f * y1 + 3.0f * y2 - y3);
                y3 = y2; y2 = y1; y1 = y;
                if (t < (int) (kSr * 0.45) && t > (int) (kSr * 0.1)) ref = std::max (ref, d3);
                if (t > (int) (kSr * 0.45)) worst = std::max (worst, d3);
            }
        }
        std::printf ("   %-7s drive leaving 0: worst 3rd difference %.4f (clean sine %.4f)\n", id, worst, ref);
        CHECK (worst < 6.0f * ref + 0.002f, "%s steps when its drive leaves 0 (%.4f vs %.4f)", id, worst, ref);
    }

    // 5. REVERB SIZE under MOTION: no zipper. A 200 Hz sine into a 100 % wet reverb; SIZE swept
    //    by a 1-bar sine at 50 % (the read heads move up to ~0.3 samples per sample: Doppler, but
    //    nothing above a few hundred Hz). HF content (4th difference: > 5 kHz) stays near the
    //    unmodulated case; per-chunk length steps would put broadband clicks there.
    {
        auto hfRatio = [&] (float depth)
        {
            resetBus();
            Rig2 r ("REVERB");
            r.set ("mix", 1.0f); r.set ("mod", 0.0f);
            r.set ("m1_src", 1.0f); r.set ("m1_rate", 4.0f); r.set ("m1_depth", depth);
            int sizeIdx = 0;
            {
                auto* prm = r.p->apvts.getParameter ("m1_target");
                const auto& names = prm->getAllValueStrings();
                for (int k = 0; k < names.size(); ++k) if (names[k] == "SIZE") sizeIdx = k;
                r.set ("m1_target", (float) sizeIdx);
            }
            double e2 = 0, e = 0;
            float y1 = 0, y2 = 0, y3 = 0, y4 = 0;
            int64_t t = 0;
            for (int b = 0; b < (int) (kSr * 6.0) / kBlock; ++b)
            {
                r.step (kBlock, [&] (juce::AudioBuffer<float>& bb, juce::MidiBuffer&, int n)
                {
                    for (int i = 0; i < n; ++i) { const float v = 0.2f * (float) std::sin (6.283185307 * 200.0 * (double) (t + i) / kSr); bb.setSample (0, i, v); bb.setSample (1, i, v); }
                });
                for (int i = 0; i < kBlock; ++i, ++t)
                {
                    const float y = r.buf.getSample (0, i), d4 = y - 4.0f * y1 + 6.0f * y2 - 4.0f * y3 + y4;
                    y4 = y3; y3 = y2; y2 = y1; y1 = y;
                    if (t > (int) kSr) { e2 += d4 * d4; e += y * y; }
                }
            }
            return 10.0 * std::log10 (e2 / (e + 1e-30) + 1e-30);
        };
        const double still = hfRatio (0.0), moving = hfRatio (0.5);
        std::printf ("   REVERB SIZE modulated: HF %.1f dB (unmodulated %.1f dB)\n", moving, still);
        // Per-chunk length steps measured -73.6 dB here; ramped + glided lengths and gains -95 dB
        // (what is left is the Doppler of the moving lines, far below audibility).
        CHECK (moving < -88.0, "REVERB SIZE modulation zippers (HF %.1f dB vs %.1f dB)", moving, still);
    }

    // 6. LIMIT AUTO settles on TARGET without overshoot (no feedback loop around the 3 s meter).
    {
        resetBus();
        Rig2 r ("LIMIT");
        r.set ("target", -14.0f); r.set ("ceiling", -0.1f); r.set ("clip", 0.0f); r.set ("auto", 1.0f); r.set ("input", 0.0f);
        LoudnessMeter m; m.prepare (kSr);
        float peakST = -100.0f, at20 = 0.0f;
        int64_t t = 0;
        for (int b = 0; b < (int) (kSr * 40.0) / kBlock; ++b)
        {
            r.step (kBlock, [&] (juce::AudioBuffer<float>& bb, juce::MidiBuffer&, int n)
            {
                for (int i = 0; i < n; ++i) { const float v = 0.1f * (float) std::sin (6.283185307 * 997.0 * (double) (t + i) / kSr); bb.setSample (0, i, v); bb.setSample (1, i, v); }
            });
            for (int i = 0; i < kBlock; ++i) m.push (r.buf.getSample (0, i), r.buf.getSample (1, i));
            t += kBlock;
            if (t > (int) (kSr * 4.0)) peakST = std::max (peakST, m.shortTerm);
            if (t <= (int) (kSr * 20.0)) at20 = m.shortTerm;
        }
        std::printf ("   LIMIT AUTO, -20 LUFS in, TARGET -14: highest short-term %.2f LUFS, at 20 s %.2f, at 40 s %.2f\n", peakST, at20, m.shortTerm);
        CHECK (peakST < -14.0f + 0.3f, "LIMIT AUTO overshoots TARGET (%.2f LUFS)", peakST);
        CHECK (std::abs (at20 + 14.0f) < 0.3f, "LIMIT AUTO has not settled after 20 s (%.2f LUFS)", at20);
    }

    // 7. STRIKE: stolen voices fade (no click). BELL, long decay, quarter notes; DISTANCE far so
    //    the blast itself is smooth. Third difference at steal hits vs the first four hits.
    {
        resetBus();
        Rig2 r ("STRIKE");
        r.set ("model", 0.0f); r.set ("decay", 0.85f); r.set ("distance", 0.95f); r.set ("power", 0.4f);
        float early = 0.0f, steal = 0.0f, y1 = 0, y2 = 0, y3 = 0;
        int hits = 0;
        int64_t t = 0, lastHit = -100000;
        const int every = (int) std::round (kSr * 60.0 / r.head.bpm);
        for (int b = 0; b < (int) (kSr * 5.0) / 64; ++b)
        {
            r.step (64, [&] (juce::AudioBuffer<float>&, juce::MidiBuffer& m, int n)
            {
                for (int i = 0; i < n; ++i) if ((t + i) % every == 0) { m.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), i); ++hits; lastHit = t + i; }
            });
            for (int i = 0; i < 64; ++i, ++t)
            {
                const float y = r.buf.getSample (0, i), d3 = std::fabs (y - 3.0f * y1 + 3.0f * y2 - y3);
                y3 = y2; y2 = y1; y1 = y;
                if (t - lastHit < (int) (0.003 * kSr)) (hits <= 4 ? early : steal) = std::max (hits <= 4 ? early : steal, d3);
            }
        }
        std::printf ("   STRIKE steals: worst 3rd difference at a steal %.5f, at a free voice %.5f\n", steal, early);
        CHECK (steal < 3.0f * early + 1e-4f, "STRIKE voice steal clicks (%.5f vs %.5f)", steal, early);
    }

    // 8. A second PANIC during the fade doesn't bring the sound back. The same held pad rendered
    //    with and without PANIC: after the first PANIC the ratio between them only ever falls.
    {
        auto render = [&] (bool withPanic)
        {
            resetBus();
            Rig2 r ("PAD", kSr, 64);
            std::vector<float> out;
            for (int b = 0; b < 640; ++b)
            {
                if (withPanic && (b == 600 || b == 601)) r.p->panic();
                r.step (64, [&] (juce::AudioBuffer<float>&, juce::MidiBuffer& m, int) { if (b == 0) m.addEvent (juce::MidiMessage::noteOn (1, 48, (juce::uint8) 100), 0); });
                for (int i = 0; i < 64; ++i) out.push_back (r.buf.getSample (0, i));
            }
            return out;
        };
        const auto a = render (false), b = render (true);
        float prevG = 1.0f, worstRise = 0.0f;
        for (size_t i = 600 * 64; i < a.size(); ++i)
        {
            if (std::fabs (a[i]) < 1e-3f) continue;
            const float g = b[i] / a[i];
            worstRise = std::max (worstRise, g - prevG);
            prevG = std::min (prevG, g);
        }
        std::printf ("   PANIC twice in 2 ms: largest rise in gain during the fade %.4f (want 0)\n", worstRise);
        CHECK (worstRise < 0.01f, "a second PANIC restarted the fade (gain rose by %.3f)", worstRise);
    }
    resetBus();
}

// ------------------------------------------------------------------------------------------
static void testCpu()
{
    std::printf ("-- CPU: real-time factor, 48 kHz, block 256, default settings (best of 3)\n");
    std::printf ("| MODULE | x REAL TIME | %% OF ONE CORE |\n|---|---:|---:|\n");
    for (auto& id : allModuleIds())
    {
        double best = 0.0;
        for (int run = 0; run < 3; ++run)
        {
            resetBus();
            Rig2 r (id);
            const bool inst = r.p->mod().instrument();
            const int total = (int) (kSr * 4.0);
            const auto t0 = std::chrono::high_resolution_clock::now();
            for (int s = 0; s < total; s += kBlock)
                r.step (kBlock, [&] (juce::AudioBuffer<float>& b, juce::MidiBuffer& m, int n)
                {
                    if (inst) gridNotes (m, r.ppq, r.bps(), n, 0.25, 0.2, id == "KICK" ? 36 : 48, 100);
                    else fillAudio (b, r.ppq, r.head.bpm, 2);
                });
            const double secs = std::chrono::duration<double> (std::chrono::high_resolution_clock::now() - t0).count();
            best = std::max (best, 4.0 / secs);
        }
        std::printf ("| %s | %.0f | %.2f |\n", id.c_str(), best, 100.0 / best);
    }
}

