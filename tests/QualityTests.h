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
//   --radio         RADIO: CRUSH/RATE follow SIGNAL, click-free holes and squeal, RATE counter
//   --poly          MOTION's odd rates on the song position, DELAY 5/16 and 7/16
//   --terroir       TERROIR: the engine against direct convolution, captures, knobs, FREEZE, drop / store
#include "core/BandLimited.h"
#include "core/Convolver.h"
#include "core/IrTools.h"
#include "core/E16Glide.h"
#include "core/E16Map.h"
#include "core/HeardBeat.h"
#include "core/Loudness.h"
#include "core/Oversample.h"
#include "core/Resample.h"
#include "core/Sample.h"

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
        // EBU Tech 3341 minimum-requirement cases 1-5 and 9, synthesised from the specification
        // (1 kHz sines, both channels, 48 kHz; the published files are these signals): level,
        // the absolute and relative gates, and a short-term reading that must stay constant.
        struct Res { float m, s, i, sMin, sMax; };
        auto run = [] (const std::vector<std::pair<double, double>>& segs)    // (seconds, dBFS)
        {
            LoudnessMeter m; m.prepare (48000.0);
            Res r { 0, 0, 0, 100.0f, -100.0f };
            int64_t i = 0;
            for (const auto& sg : segs)
            {
                const float a = (float) std::pow (10.0, sg.second / 20.0);
                const int64_t n = (int64_t) std::llround (sg.first * 48000.0);
                for (int64_t k = 0; k < n; ++k, ++i)
                {
                    const float v = a * (float) std::sin (2.0 * 3.14159265358979 * 1000.0 * (double) (i % 48000) / 48000.0);
                    m.push (v, v);
                    if (i >= 3 * 48000 && i % 4800 == 4799) { r.sMin = std::min (r.sMin, m.shortTerm); r.sMax = std::max (r.sMax, m.shortTerm); }
                }
            }
            r.m = m.momentary; r.s = m.shortTerm; r.i = m.integrated;
            return r;
        };
        std::vector<std::pair<double, double>> c9;
        for (int k = 0; k < 20; ++k) { c9.push_back ({ 1.34, -20.0 }); c9.push_back ({ 1.66, -30.0 }); }
        const Res c1 = run ({ { 20.0, -23.0 } }), c2 = run ({ { 20.0, -33.0 } }),
                  c3 = run ({ { 10.0, -36.0 }, { 60.0, -23.0 }, { 10.0, -36.0 } }),
                  c4 = run ({ { 10.0, -72.0 }, { 10.0, -36.0 }, { 60.0, -23.0 }, { 10.0, -36.0 }, { 10.0, -72.0 } }),
                  c5 = run ({ { 20.0, -26.0 }, { 20.1, -20.0 }, { 20.0, -26.0 } }),
                  r9 = run (c9);
        std::printf ("   EBU Tech 3341: case 1 M/S/I %.2f %.2f %.2f (-23); case 2 %.2f %.2f %.2f (-33); cases 3/4/5 I %.2f %.2f %.2f (-23); case 9 S %.2f .. %.2f (-23)\n",
                     c1.m, c1.s, c1.i, c2.m, c2.s, c2.i, c3.i, c4.i, c5.i, r9.sMin, r9.sMax);
        auto near = [] (float v, float want) { return std::abs (v - want) <= 0.1f; };
        CHECK (near (c1.m, -23) && near (c1.s, -23) && near (c1.i, -23), "EBU 3341 case 1 (%.2f %.2f %.2f)", c1.m, c1.s, c1.i);
        CHECK (near (c2.m, -33) && near (c2.s, -33) && near (c2.i, -33), "EBU 3341 case 2 (%.2f %.2f %.2f)", c2.m, c2.s, c2.i);
        CHECK (near (c3.i, -23) && near (c4.i, -23) && near (c5.i, -23), "EBU 3341 gating cases 3/4/5 (%.2f %.2f %.2f)", c3.i, c4.i, c5.i);
        CHECK (near (r9.sMin, -23) && near (r9.sMax, -23), "EBU 3341 case 9 short-term (%.2f .. %.2f)", r9.sMin, r9.sMax);
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

    // 1. Song recall: SF CONTROL decides the bar line, and every plugin lands on it whatever
    //    order they are processed in and whatever the block size or song position (followers
    //    used to slip a bar from ppq rounding, or when processed before CONTROL).
    {
        int split = 0, late = 0, runs = 0;
        double worstErr = 0.0;
        for (int blk : { 333, 17, 64, 1000, 2048 })
            for (double startPpq : { 0.37, 500.37, 7777.13, 30001.9 })
            {
                resetBus();
                Rig2 before ("FILTER", kSr, blk), ctl ("CONTROL", kSr, blk), after ("FILTER", kSr, blk);
                for (auto* r : { &before, &ctl, &after }) { r->head.bpm = 128.0; r->ppq = startPpq; }
                auto dc = [&] (juce::AudioBuffer<float>& b, juce::MidiBuffer&, int n) { for (int c2 = 0; c2 < 2; ++c2) for (int i = 0; i < n; ++i) b.setSample (c2, i, 0.5f); };
                auto quiet = [] (juce::AudioBuffer<float>&, juce::MidiBuffer&, int) {};
                auto cycle = [&] { before.step (blk, dc); ctl.step (blk, quiet); after.step (blk, dc); };
                // Song 2 (index 1): LEVEL -6 dB in both followers, stored with STORE ALL.
                ctl.set ("song", 1.0f); cycle();
                before.set ("level", -6.0f); after.set ("level", -6.0f); cycle();
                ctl.set ("store_all", 1.0f); cycle(); ctl.set ("store_all", 0.0f); cycle();
                ctl.set ("song", 0.0f); cycle();
                before.set ("level", 0.0f); after.set ("level", 0.0f);
                for (int k = 0; k < (int) (0.3 * kSr) / blk + 2; ++k) cycle();
                // Select song 2 mid-bar; find the first moving sample in each follower.
                const double reqPpq = ctl.ppq;
                ctl.set ("song", 1.0f);
                const double barPpq = std::ceil (reqPpq / 4.0) * 4.0;
                const double expect = std::ceil ((barPpq - reqPpq) / ctl.bps() - 1e-3);
                double fb = -1.0, fa = -1.0;
                int64_t pos = 0;
                for (int k = 0; k < (int) (kSr * 6.0) / blk && (fb < 0.0 || fa < 0.0); ++k)
                {
                    cycle();
                    for (int i = 0; i < blk; ++i, ++pos)
                    {
                        if (fb < 0.0 && before.buf.getSample (0, i) < 0.5f * (1.0f - 1e-4f)) fb = (double) pos;
                        if (fa < 0.0 && after.buf.getSample (0, i) < 0.5f * (1.0f - 1e-4f)) fa = (double) pos;
                    }
                }
                ++runs;
                const double errA = fa - expect, errB = fb - expect;
                worstErr = std::max ({ worstErr, std::abs (errA), std::abs (errB) });
                // The plugin processed after CONTROL lands on the exact sample; the one before it
                // hears the change a block later at most (a mid-bar request is ahead of the bar
                // line, so it lands exactly too).
                if (fa < 0.0 || std::abs (errA) > 1.0 || fb < 0.0 || std::abs (errB) > 1.0) ++late;
                if (std::abs (fa - fb) > 1.0) ++split;
            }
        std::printf ("   song recall: %d of %d runs off the bar line, %d split between plugins (worst %.0f samples)\n", late, runs, split, worstErr);
        CHECK (late == 0 && split == 0, "song recall missed the bar line in %d runs, split in %d (worst %.0f samples)", late, split, worstErr);
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
    //    UP AND DOWN lifts -17 LUFS to -14; DOWN ONLY (the default) brings -8 down to -14 and
    //    leaves a quiet -20 alone. Breakdowns are held: DOWN ONLY, a 30 s breakdown only 4 dB down
    //    (the trim used to release, so the drop came back hot); UP AND DOWN, a 90 s breakdown
    //    12 dB down (it used to be lifted 12 dB, and the drop came back 12 dB over TARGET).
    {
        struct Seg { double sec; float amp; };
        auto run = [&] (std::vector<Seg> segs, int mode, std::vector<float>& st)
        {
            resetBus();
            Rig2 r ("LIMIT");
            r.set ("target", -14.0f); r.set ("ceiling", -0.1f); r.set ("clip", 0.0f); r.set ("auto", 1.0f); r.set ("input", 0.0f);
            r.set ("auto_mode", (float) mode);
            LoudnessMeter m; m.prepare (kSr);
            st.clear();
            int64_t t = 0;
            double segEnd = 0.0;
            for (const auto& sg : segs)
            {
                segEnd += sg.sec;
                while (t < (int64_t) (segEnd * kSr))
                {
                    r.step (kBlock, [&] (juce::AudioBuffer<float>& bb, juce::MidiBuffer&, int n)
                    {
                        for (int i = 0; i < n; ++i) { const float v = sg.amp * (float) std::sin (6.283185307 * 997.0 * (double) (t + i) / kSr); bb.setSample (0, i, v); bb.setSample (1, i, v); }
                    });
                    for (int i = 0; i < kBlock; ++i) m.push (r.buf.getSample (0, i), r.buf.getSample (1, i));
                    t += kBlock;
                    if (t % (int64_t) (kSr / 10) < kBlock) st.push_back (m.shortTerm);   // every 100 ms
                }
            }
        };
        auto at = [] (const std::vector<float>& st, double sec) { return st[std::min (st.size() - 1, (size_t) std::lround (sec * 10.0) - 1)]; };
        auto peakFrom = [] (const std::vector<float>& st, double sec) { float p = -100.0f; for (size_t i = (size_t) (sec * 10.0); i < st.size(); ++i) p = std::max (p, st[i]); return p; };
        const float a17 = 0.1413f, a8 = 0.398f, a12 = 0.251f, a20 = 0.1f, a14 = 0.2f, a26 = 0.05f;
        std::vector<float> up, down, quiet, shallow, deep;
        run ({ { 40.0, a17 } }, 1, up);                                 // -17 LUFS, UP AND DOWN
        run ({ { 20.0, a8 } }, 0, down);                                // -8 LUFS, DOWN ONLY
        run ({ { 40.0, a20 } }, 0, quiet);                              // -20 LUFS, DOWN ONLY
        run ({ { 20.0, a8 }, { 30.0, a12 }, { 6.0, a8 } }, 0, shallow); // DOWN ONLY, 30 s breakdown 4 dB down
        run ({ { 20.0, a14 }, { 90.0, a26 }, { 6.0, a14 } }, 1, deep);  // UP AND DOWN, 90 s breakdown 12 dB down
        const float pk = peakFrom (up, 4.0), u20 = at (up, 20.0), d20 = at (down, 20.0), q40 = at (quiet, 40.0);
        const float sRet = at (shallow, 54.0), dBrk = at (deep, 109.0), dRet = at (deep, 114.0);
        std::printf ("   LIMIT AUTO UP AND DOWN, -17 in, TARGET -14: highest %.2f, at 20 s %.2f; DOWN ONLY, -8 in: at 20 s %.2f; -20 in: at 40 s %.2f\n", pk, u20, d20, q40);
        std::printf ("   LIMIT AUTO breakdowns: DOWN ONLY, drop back after 30 s 4 dB down: %.2f; UP AND DOWN, 90 s 12 dB down: breakdown %.2f, drop back %.2f\n", sRet, dBrk, dRet);
        CHECK (pk < -14.0f + 0.3f, "LIMIT AUTO overshoots TARGET (%.2f LUFS)", pk);
        CHECK (std::abs (u20 + 14.0f) < 0.3f, "LIMIT AUTO has not settled after 20 s (%.2f LUFS)", u20);
        CHECK (std::abs (d20 + 14.0f) < 0.3f, "LIMIT AUTO DOWN ONLY did not bring a loud input to TARGET (%.2f LUFS)", d20);
        CHECK (std::abs (q40 + 20.0f) < 0.3f, "LIMIT AUTO DOWN ONLY lifted a quiet input (%.2f LUFS)", q40);
        CHECK (std::abs (sRet + 14.0f) < 0.5f, "LIMIT AUTO let go during a short breakdown: the drop came back at %.2f LUFS", sRet);
        CHECK (std::abs (dBrk + 26.0f) < 0.5f, "LIMIT AUTO lifted a breakdown (%.2f LUFS)", dBrk);
        CHECK (std::abs (dRet + 14.0f) < 0.5f, "LIMIT AUTO: the drop came back at %.2f LUFS after a long breakdown", dRet);
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
// Regressions for the second (ChatGPT) audit. Each test fails on the code before its fix.
static double hannAmp (const std::vector<float>& y, size_t from, size_t to, double f, double sr)
{
    double si = 0.0, co = 0.0, ws = 0.0;
    const size_t len = to - from;
    for (size_t i = from; i < to; ++i)
    {
        const double w = 0.5 - 0.5 * std::cos (6.283185307 * (double) (i - from) / (double) (len - 1)), ph = 6.283185307 * f * (double) i / sr;
        si += w * y[i] * std::sin (ph); co += w * y[i] * std::cos (ph); ws += w;
    }
    return 2.0 * std::sqrt (si * si + co * co) / ws;
}
static juce::File writeWav (const char* name, const juce::AudioBuffer<float>& b, double sr)
{
    juce::File f = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile (name);
    f.deleteFile();
    juce::WavAudioFormat fmt;
    std::unique_ptr<juce::AudioFormatWriter> w (fmt.createWriterFor (new juce::FileOutputStream (f), sr, (unsigned) b.getNumChannels(), 24, {}, 0));
    w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
    return f;
}

static void testAuditFixes()
{
    std::printf ("-- audit 2 regressions\n");
    auto& d = Bus::get().d();
    using Fill = std::function<void (juce::AudioBuffer<float>&, juce::MidiBuffer&, int)>;
    auto render = [] (Rig2& r, double secs, const Fill& fill, std::vector<float>* outL = nullptr, std::vector<float>* outR = nullptr)
    {
        for (int b = 0; b < (int) (r.sr * secs) / r.block; ++b)
        {
            r.step (r.block, fill);
            for (int i = 0; i < r.block; ++i) { if (outL) outL->push_back (r.buf.getSample (0, i)); if (outR) outR->push_back (r.buf.getSample (1, i)); }
        }
    };

    // PAD: the top of the legal range (TUNE +24, note 127) used to step a wavetable phase past the
    // table's end. Phases now wrap for any increment and partials above Nyquist go silent.
    {
        resetBus();
        Rig2 r ("PAD");
        r.set ("model", 0.0f); r.set ("tune", 24.0f);
        float pk = 0.0f;
        int64_t t = 0;
        std::vector<float> o;
        render (r, 1.0, [&] (juce::AudioBuffer<float>&, juce::MidiBuffer& m, int n)
        {
            if (t == 0) m.addEvent (juce::MidiMessage::noteOn (1, 127, (juce::uint8) 127), 0);
            t += n;
        }, &o);
        for (const float v : o) pk = std::max (pk, std::fabs (v));
        // The safety stage zeroes NaN / Inf and resets the module, so the output alone can't show a
        // bad read: no FAULT may have been raised anywhere in the render.
        const int faults = r.p->faultCount.load();
        Mipmap mm; std::vector<float> a (8, 0.5f), ph (8, 0.0f); mm.build (a, ph, 96000.0);
        const float wrapped = mm.read (3, 1.7) - mm.read (3, 0.7), silent = mm.read (mm.octaveFor (50000.0f), 0.3);
        std::printf ("   PAD TUNE +24, note 127: faults %d, peak %.3f; table read at phase 1.7 = 0.7 (%.1e), above Nyquist %.1e\n", faults, pk, wrapped, silent);
        CHECK (faults == 0 && std::fabs (wrapped) < 1e-6f && silent == 0.0f, "extreme PAD note: unsafe wavetable read (%d faults)", faults);
    }

    // Notes that must end: a mono ACID line layered with a poly ORGAN (B released while A is held),
    // and VOICE choir DENSITY ghosts that start after their note was released.
    {
        resetBus();
        Rig2 r ("LEAD");
        r.set ("model", 0.0f); r.set ("layer_model", 5.0f); r.set ("layer", 1.0f);
        std::vector<float> o;
        int64_t t = 0;
        const int64_t A = 0, B = (int64_t) (0.2 * kSr), Boff = (int64_t) (0.5 * kSr), Aoff = (int64_t) (0.8 * kSr);
        render (r, 4.0, [&] (juce::AudioBuffer<float>&, juce::MidiBuffer& m, int n)
        {
            for (int i = 0; i < n; ++i)
            {
                const int64_t s2 = t + i;
                if (s2 == A) m.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), i);
                if (s2 == B) m.addEvent (juce::MidiMessage::noteOn (1, 62, (juce::uint8) 100), i);
                if (s2 == Boff) m.addEvent (juce::MidiMessage::noteOff (1, 62), i);
                if (s2 == Aoff) m.addEvent (juce::MidiMessage::noteOff (1, 60), i);
            }
            t += n;
        }, &o);
        float tail = 0.0f;
        for (size_t i = (size_t) (3.5 * kSr); i < o.size(); ++i) tail = std::max (tail, std::fabs (o[i]));
        resetBus();
        Rig2 v ("VOICE");
        v.set ("density", 0.5f); v.set ("echo", 0.0f);
        std::vector<float> ov;
        t = 0;
        render (v, 4.0, [&] (juce::AudioBuffer<float>&, juce::MidiBuffer& m, int n)
        {
            for (int i = 0; i < n; ++i)
            {
                if (t + i == 0) m.addEvent (juce::MidiMessage::noteOn (1, 57, (juce::uint8) 100), i);
                if (t + i == (int64_t) (0.05 * kSr)) m.addEvent (juce::MidiMessage::noteOff (1, 57), i);
            }
            t += n;
        }, &ov);
        float vtail = 0.0f;
        for (size_t i = (size_t) (3.5 * kSr); i < ov.size(); ++i) vtail = std::max (vtail, std::fabs (ov[i]));
        std::printf ("   ACID + ORGAN layer, overlapping notes: %.1f dB after release; VOICE choir ghosts: %.1f dB\n", gainToDb (tail), gainToDb (vtail));
        CHECK (tail < 1e-4f, "LEAD layer left a voice sounding (%.1f dB)", gainToDb (tail));
        CHECK (vtail < 1e-4f, "VOICE choir DENSITY ghost kept sounding (%.1f dB)", gainToDb (vtail));
    }

    // After a recall the performer gets the knob back at once (it used to be held for 3 s).
    {
        resetBus();
        Rig2 r ("FILTER");
        r.head.playing = false;
        auto dc = [] (juce::AudioBuffer<float>& b, juce::MidiBuffer&, int n) { for (int c2 = 0; c2 < 2; ++c2) for (int i = 0; i < n; ++i) b.setSample (c2, i, 0.5f); };
        d.song = 1;
        r.set ("level", -6.0f); render (r, 0.1, dc);
        r.p->storeSong(); render (r, 0.05, dc);
        r.set ("level", 0.0f); render (r, 0.3, dc);
        d.song = 1; d.songAtPpq = -1.0; d.songSeq.fetch_add (1);       // recall (stopped: at once)
        render (r, 0.2, dc);
        const float recalled = r.buf.getSample (0, r.block - 1);
        r.set ("level", 3.0f);                                          // the performer turns the knob
        render (r, 0.15, dc);
        const float after = r.buf.getSample (0, r.block - 1);
        std::printf ("   recall -6 dB: %.3f; knob to +3 dB 150 ms later: %.3f (want 0.706)\n", recalled, after);
        CHECK (std::fabs (recalled - 0.25f) < 0.01f, "recall did not apply (%.3f)", recalled);
        CHECK (std::fabs (after - 0.706f) < 0.02f, "a knob move after a recall was ignored (%.3f)", after);
    }

    // PANIC held: silent for as long as it is held, sound back on release.
    {
        resetBus();
        Rig2 ctl ("CONTROL"), f ("FILTER");
        float during = 0.0f, after = 0.0f;
        int64_t t = 0;
        auto sine = [&] (juce::AudioBuffer<float>& b, juce::MidiBuffer&, int n) { for (int i = 0; i < n; ++i) { const float v = 0.3f * (float) std::sin (6.283185307 * 440.0 * (double) (t + i) / kSr); b.setSample (0, i, v); b.setSample (1, i, v); } };
        for (int b = 0; b < (int) (2.0 * kSr) / kBlock; ++b)
        {
            const double sec = (double) t / kSr;
            ctl.set ("panic", sec >= 0.5 && sec < 1.5 ? 1.0f : 0.0f);
            ctl.step (kBlock, nullptr);
            f.step (kBlock, sine);
            const float pk = f.buf.getMagnitude (0, 0, kBlock);
            if (sec > 0.52 && sec < 1.48) during = std::max (during, pk);
            if (sec > 1.6) after = std::max (after, pk);
            t += kBlock;
        }
        std::printf ("   PANIC held 1 s on a FILTER with input: peak while held %.1f dB, after release %.1f dB\n", gainToDb (during), gainToDb (after));
        CHECK (during < 1e-4f && after > 0.25f, "PANIC hold (during %.1f dB, after %.1f dB)", gainToDb (during), gainToDb (after));
    }

    // DECK reopens with its stored loudness analysis (the restore line was inside a comment).
    {
        resetBus();
        Rig2 r ("DECK");
        juce::ValueTree t ("MODULE"), tr ("TRACK");
        tr.setProperty ("path", "/nonexistent/sf_track.wav", nullptr); tr.setProperty ("bpm", 124.0, nullptr);
        tr.setProperty ("firstSec", 0.1, nullptr); tr.setProperty ("lufs", -8.0, nullptr); tr.setProperty ("cues", "0,32,64,96,", nullptr);
        t.appendChild (tr, nullptr);
        r.p->mod().load (t);
        juce::ValueTree out ("MODULE");
        r.p->mod().save (out);
        const double lufs = out.getNumChildren() > 0 ? (double) out.getChild (0).getProperty ("lufs", 0.0) : 0.0;
        std::printf ("   DECK stored -8.0 LUFS, reopened %.1f LUFS\n", lufs);
        CHECK (std::abs (lufs + 8.0) < 0.01, "DECK lost its stored loudness (%.1f)", lufs);
    }

    // DRY off: DELAY, REVERB, DENSITY on a return pass no dry signal.
    for (const char* id : { "DELAY", "REVERB", "DENSITY" })
    {
        float dryOn = 0.0f, dryOff = 0.0f;
        for (int pass = 0; pass < 2; ++pass)
        {
            resetBus();
            Rig2 r (id);
            r.set ("mix", 1.0f); r.set ("dry", pass == 0 ? 1.0f : 0.0f);
            std::vector<float> o;
            int64_t t = 0;
            render (r, 0.1, [&] (juce::AudioBuffer<float>& b, juce::MidiBuffer&, int n)
            {
                for (int i = 0; i < n; ++i) { const float v = (t + i >= 1000 && t + i < 1400) ? 0.5f : 0.0f; b.setSample (0, i, v); b.setSample (1, i, v); }
                t += n;
            }, &o);
            const int lat = r.p->getLatencySamples();
            const float at = o[(size_t) (1200 + lat)];
            (pass == 0 ? dryOn : dryOff) = at;
        }
        std::printf ("   %-7s MIX 100 %%: dry part %.3f with DRY on, %.4f with DRY off\n", id, dryOn, dryOff);
        CHECK (dryOn > 0.4f && std::fabs (dryOff) < 1e-3f, "%s DRY off still passes dry (%.4f)", id, dryOff);
    }

    // DRIVE: MIX 0 is the clean input (auto gain used to pull it down ~20 dB), and a 50 % blend
    // is flat to 18 kHz (the dry path is phase matched; a plain delay combed -17 dB at 18 kHz).
    {
        auto level = [&] (float mix, float drive, double f, float amp)
        {
            resetBus();
            Rig2 r ("DRIVE");
            r.set ("mix", mix); r.set ("drive", drive); r.set ("auto_gain", 1.0f);
            std::vector<float> o;
            int64_t t = 0;
            render (r, 0.5, [&] (juce::AudioBuffer<float>& b, juce::MidiBuffer&, int n)
            {
                for (int i = 0; i < n; ++i) { const float v = amp * (float) std::sin (6.283185307 * f * (double) (t + i) / kSr); b.setSample (0, i, v); b.setSample (1, i, v); }
                t += n;
            }, &o);
            return 20.0 * std::log10 (hannAmp (o, (size_t) (0.2 * kSr), o.size(), f, kSr) / amp);
        };
        const double clean = level (0.0f, 0.8f, 1000.0, 0.25f);
        double worst = 0.0;
        const double ref = level (0.5f, 0.0f, 1000.0, 0.02f);
        for (double f : { 15000.0, 17000.0, 18000.0 }) worst = std::max (worst, std::abs (level (0.5f, 0.0f, f, 0.02f) - ref));
        std::printf ("   DRIVE MIX 0 / DRIVE 80 %% / AUTO: %.2f dB; 50 %% blend 15-18 kHz vs 1 kHz: worst %.2f dB\n", clean, worst);
        CHECK (std::abs (clean) < 0.1, "DRIVE MIX 0 is not clean (%.2f dB)", clean);
        CHECK (worst < 0.5, "DRIVE dry/wet blend combs (%.2f dB)", worst);
    }

    // DENSITY's quiet end: the first ghost rises from silence (-30 dB at 1/64 of the knob's
    // first half), instead of arriving almost at full level.
    for (const char* id : { "PERC", "BASS" })
    {
        auto ghost = [&] (float dens)
        {
            resetBus();
            Rig2 r (id);
            if (std::string (id) == "PERC") { r.set ("model", 6.0f); r.set ("accent", 0.0f); }
            else r.set ("model", 5.0f);
            r.set ("density", dens); r.set ("echo_space", 0.0f);
            std::vector<float> o;
            int64_t t = 0;
            render (r, 0.6, [&] (juce::AudioBuffer<float>&, juce::MidiBuffer& m, int n)
            {
                for (int i = 0; i < n; ++i)
                {
                    if (t + i == 0) m.addEvent (juce::MidiMessage::noteOn (1, 48, (juce::uint8) 100), i);
                    if (t + i == (int64_t) (0.08 * kSr)) m.addEvent (juce::MidiMessage::noteOff (1, 48), i);
                }
                t += n;
            }, &o);
            const int lat = r.p->getLatencySamples();
            const size_t at = (size_t) (lat + 0.5 * 60.0 / r.head.bpm * kSr);
            float pk = 0.0f;
            for (size_t i = at; i < std::min (o.size(), at + (size_t) (0.06 * kSr)); ++i) pk = std::max (pk, std::fabs (o[i]));
            return pk;
        };
        const float full = ghost (0.5f), quiet = ghost (2.0f / 127.0f);
        const double rel = gainToDb (quiet) - gainToDb (full);
        std::printf ("   %-5s first ghost at DENSITY CC2 vs 50 %%: %.1f dB (expect about -30)\n", id, rel);
        CHECK (rel < -25.0 && rel > -36.0, "%s DENSITY quiet end (%.1f dB)", id, rel);
    }

    // REPEAT: the captured loop survives a long hold (it used to be overwritten after 4 s), and
    // release crossfades the loop out instead of cutting it.
    {
        resetBus();
        Rig2 r ("REPEAT");
        r.set ("quant", 0.0f); r.set ("decay", 0.0f); r.set ("length", 2.0f); r.set ("mix", 1.0f);
        std::vector<float> o;
        int64_t t = 0;
        for (int b = 0; b < (int) (6.5 * kSr) / kBlock; ++b)
        {
            const double sec = (double) t / kSr;
            r.set ("roll", sec >= 0.25 && sec < 6.0 ? 1.0f : 0.0f);
            r.step (kBlock, [&] (juce::AudioBuffer<float>& bb, juce::MidiBuffer&, int n)
            {
                for (int i = 0; i < n; ++i) { const float v = (t + i) < (int64_t) (0.5 * kSr) ? 0.3f * (float) std::sin (6.283185307 * 220.0 * (double) (t + i) / kSr) : 0.0f; bb.setSample (0, i, v); bb.setSample (1, i, v); }
            });
            for (int i = 0; i < kBlock; ++i) o.push_back (r.buf.getSample (0, i));
            t += kBlock;
        }
        auto rms = [&] (double a, double b2) { double e = 0; size_t n = 0; for (size_t i = (size_t) (a * kSr); i < (size_t) (b2 * kSr); ++i, ++n) e += o[i] * o[i]; return std::sqrt (e / std::max<size_t> (1, n)); };
        const double early = rms (0.6, 1.0), late = rms (5.5, 5.95);
        float jump = 0.0f;
        for (size_t i = (size_t) (6.0 * kSr); i < (size_t) (6.03 * kSr); ++i) jump = std::max (jump, std::fabs (o[i] - o[i - 1]));
        std::printf ("   REPEAT held 5.75 s: loop level %.1f dB late vs early; largest step at release %.4f (a sine step is %.4f)\n",
                     20.0 * std::log10 (late / std::max (1e-9, early)), jump, 0.3f * 6.2832f * 220.0f / 48000.0f);
        CHECK (std::abs (20.0 * std::log10 (late / std::max (1e-9, early))) < 1.0, "REPEAT loop did not survive the hold");
        CHECK (jump < 0.03f, "REPEAT release cuts the loop (step %.4f)", jump);
    }

    // VOICE: a 17 kHz sample played an octave up has nothing to play below Nyquist; it used to
    // fold back to 14 kHz at full level.
    {
        juce::AudioBuffer<float> b (2, 48000);
        for (int i = 0; i < 48000; ++i) { const float v = 0.5f * (float) std::sin (6.283185307 * 17000.0 * i / 48000.0); b.setSample (0, i, v); b.setSample (1, i, v); }
        const auto wav = writeWav ("sf_test_17k.wav", b, 48000.0);
        resetBus();
        Rig2 r ("VOICE");
        r.p->mod().loadFiles (juce::StringArray (wav.getFullPathName()));
        r.set ("model", 0.0f); r.set ("tune", 12.0f); r.set ("echo", 0.0f); r.set ("drive", 0.0f); r.set ("gate", 1.0f);
        std::vector<float> o;
        int64_t t = 0;
        render (r, 0.6, [&] (juce::AudioBuffer<float>&, juce::MidiBuffer& m, int n) { if (t == 0) m.addEvent (juce::MidiMessage::noteOn (1, 48, (juce::uint8) 127), 0); t += n; }, &o);
        const double alias = hannAmp (o, (size_t) (0.1 * kSr), (size_t) (0.4 * kSr), 14000.0, kSr);
        std::printf ("   VOICE 17 kHz sample, TUNE +12: alias at 14 kHz %.1f dB (re the sample)\n", 20.0 * std::log10 (alias / 0.5 + 1e-12));
        CHECK (alias < 0.5 * 0.001, "VOICE sample aliases when transposed up (%.1f dB)", 20.0 * std::log10 (alias / 0.5 + 1e-12));
    }

    // FILTER: loud resonant peaks are ridden down by a gain, not a waveshaper (which made
    // harmonics at the base rate). A 1 kHz sine at +9.5 dBFS, resonance up, high-pass at 66 Hz.
    {
        resetBus();
        Rig2 r ("FILTER");
        r.set ("filter", 0.6f); r.set ("res", 1.0f);
        std::vector<float> o;
        int64_t t = 0;
        render (r, 0.5, [&] (juce::AudioBuffer<float>& b, juce::MidiBuffer&, int n)
        {
            for (int i = 0; i < n; ++i) { const float v = 3.0f * (float) std::sin (6.283185307 * 1000.0 * (double) (t + i) / kSr); b.setSample (0, i, v); b.setSample (1, i, v); }
            t += n;
        }, &o);
        const double f1 = hannAmp (o, (size_t) (0.2 * kSr), o.size(), 1000.0, kSr), f3 = hannAmp (o, (size_t) (0.2 * kSr), o.size(), 3000.0, kSr);
        std::printf ("   FILTER guard, 1 kHz at +9.5 dBFS, resonant: output %.2f, 3rd harmonic %.1f dB\n", f1, 20.0 * std::log10 (f3 / f1 + 1e-12));
        CHECK (f1 < 0.95 && f3 / f1 < 0.001, "FILTER guard distorts (3rd harmonic %.1f dB)", 20.0 * std::log10 (f3 / f1 + 1e-12));
    }

    // TONE after a sample-rate change: PRESENCE stays at 3.2 kHz (it moved an octave down).
    {
        resetBus();
        Rig2 r ("TONE");
        r.set ("presence", 4.0f);
        auto gainAt = [&] (double f)
        {
            std::vector<float> o;
            int64_t t = 0;
            render (r, 0.3, [&] (juce::AudioBuffer<float>& b, juce::MidiBuffer&, int n)
            {
                for (int i = 0; i < n; ++i) { const float v = 0.1f * (float) std::sin (6.283185307 * f * (double) (t + i) / r.sr); b.setSample (0, i, v); b.setSample (1, i, v); }
                t += n;
            }, &o);
            return 20.0 * std::log10 (hannAmp (o, o.size() / 2, o.size(), f, r.sr) / 0.1);
        };
        const double at48 = gainAt (3200.0);
        r.p->prepareToPlay (96000.0, r.block); r.sr = 96000.0;
        const double at96 = gainAt (3200.0);
        std::printf ("   TONE PRESENCE +4 at 3.2 kHz: %.2f dB at 48 kHz, %.2f dB after re-preparing at 96 kHz\n", at48, at96);
        CHECK (std::abs (at48 - at96) < 0.3, "TONE kept the old sample rate's coefficients (%.2f vs %.2f dB)", at48, at96);
    }

    // ALLOC sees wide stereo: L = x, R = -x used to cancel in its mono analysis.
    {
        resetBus();
        Rig2 r ("TAP");
        r.set ("alloc", 1.0f);
        int64_t t = 0;
        render (r, 0.5, [&] (juce::AudioBuffer<float>& b, juce::MidiBuffer&, int n)
        {
            for (int i = 0; i < n; ++i) { const float v = 0.3f * (float) std::sin (6.283185307 * 1000.0 * (double) (t + i) / kSr); b.setSample (0, i, v); b.setSample (1, i, -v); }
            t += n;
        });
        float e = -200.0f;
        for (int k = 0; k < kSources; ++k) if (Bus::get().sourceAlive (k)) e = std::max (e, d.src[k].energy[7].load());
        std::printf ("   ALLOC, anti-phase 1 kHz at -10 dBFS: band 750-1500 Hz reads %.1f dB\n", e);
        CHECK (e > -30.0f, "ALLOC misses out-of-phase stereo (%.1f dB)", e);
    }

    // TUNE reaches a held synth note.
    {
        resetBus();
        Rig2 r ("BASS");
        r.set ("model", 5.0f);
        std::vector<float> o;
        int64_t t = 0;
        for (int b = 0; b < (int) (2.0 * kSr) / kBlock; ++b)
        {
            if (t >= (int64_t) kSr) r.set ("tune", 12.0f);
            r.step (kBlock, [&] (juce::AudioBuffer<float>&, juce::MidiBuffer& m, int) { if (t == 0) m.addEvent (juce::MidiMessage::noteOn (1, 48, (juce::uint8) 100), 0); });
            for (int i = 0; i < kBlock; ++i) o.push_back (r.buf.getSample (0, i));
            t += kBlock;
        }
        auto crossings = [&] (double a, double b2) { int n = 0; for (size_t i = (size_t) (a * kSr) + 1; i < (size_t) (b2 * kSr); ++i) n += (o[i - 1] < 0.0f) != (o[i] < 0.0f); return n; };
        const double ratio = (double) crossings (1.3, 1.9) / std::max (1, crossings (0.3, 0.9));
        std::printf ("   held BASS note, TUNE +12: pitch ratio %.3f (want 2)\n", ratio);
        CHECK (std::abs (ratio - 2.0) < 0.1, "TUNE did not reach a held note (ratio %.3f)", ratio);
    }

    // A floating-point sample peaking above full scale reopens unchanged (FLAC used to clip it).
    {
        juce::AudioBuffer<float> b (2, 4800);
        for (int i = 0; i < 4800; ++i) { const float v = 2.0f * (float) std::sin (6.283185307 * 100.0 * i / 48000.0); b.setSample (0, i, v); b.setSample (1, i, v); }
        auto s1 = makeSample (std::move (b), 48000.0, 48000.0, "hot", true);
        auto s2 = loadSampleFlac (s1->flac, "hot", 48000.0, s1->flacGain);
        const float p1 = s1->buf.getMagnitude (0, 0, s1->length()), p2 = s2 != nullptr ? s2->buf.getMagnitude (0, 0, s2->length()) : 0.0f;
        std::printf ("   sample peaking at %.2f reopens at %.3f\n", p1, p2);
        CHECK (std::fabs (p2 - p1) < 0.01f, "hot sample changed on reload (%.3f vs %.3f)", p2, p1);
    }

    // FREEZE holds: REVERB and DELAY keep their level and brightness for a minute's worth of loop.
    for (const char* id : { "REVERB", "DELAY" })
    {
        resetBus();
        Rig2 r (id);
        r.set ("mix", 1.0f); r.set ("dry", 0.0f);
        if (std::string (id) == "DELAY") r.set ("feedback", 0.6f);
        Noise nz;
        std::vector<float> o;
        int64_t t = 0;
        for (int b = 0; b < (int) (20.0 * kSr) / kBlock; ++b)
        {
            r.set ("freeze", t >= (int64_t) (1.5 * kSr) ? 1.0f : 0.0f);
            r.step (kBlock, [&] (juce::AudioBuffer<float>& bb, juce::MidiBuffer&, int n)
            {
                for (int i = 0; i < n; ++i) { const float v = (t + i) < (int64_t) kSr ? 0.2f * nz.next() : 0.0f; bb.setSample (0, i, v); bb.setSample (1, i, v); }
            });
            for (int i = 0; i < kBlock; ++i) o.push_back (r.buf.getSample (0, i));
            t += kBlock;
        }
        auto stats = [&] (double a, double b2, double& lvl, double& hf)
        {
            double e = 0, h = 0;
            for (size_t i = (size_t) (a * kSr) + 2; i < (size_t) (b2 * kSr); ++i) { e += o[i] * o[i]; const double d2 = o[i] - 2.0 * o[i - 1] + o[i - 2]; h += d2 * d2; }
            lvl = 10.0 * std::log10 (e + 1e-30); hf = 10.0 * std::log10 (h / (e + 1e-30) + 1e-30);
        };
        double l1, h1, l2, h2;
        stats (3.0, 5.0, l1, h1); stats (17.0, 19.0, l2, h2);
        std::printf ("   %-6s FREEZE over 14 s: level %+.2f dB, brightness %+.2f dB\n", id, l2 - l1, h2 - h1);
        CHECK (std::abs (l2 - l1) < 1.0 && std::abs (h2 - h1) < 1.5, "%s FREEZE fades or darkens (level %+.2f, brightness %+.2f dB)", id, l2 - l1, h2 - h1);
    }

    // ALLOC keeps each source's place by plugin instance, not by label. Two PERC taps carry the
    // same name (the second is listed as "TAP PERC 2" by bus slot, and the slots are claimed in
    // whatever order the plugins first run). Order: B above A. The set is reopened with the taps
    // starting in the other order and in the same order; B's track is renamed; A's track is
    // duplicated: the copy takes a new id and A keeps its place.
    {
        auto stateOf = [] (SFProcessor& p) { juce::MemoryBlock mb; p.getStateInformation (mb); return mb; };
        auto uidOf = [&] (SFProcessor& p)
        {
            const auto mb = stateOf (p);
            return (uint64_t) juce::ValueTree::readFromData (mb.getData(), mb.getSize()).getProperty ("uid").toString().getHexValue64();
        };
        auto keyOf = [&] (SFProcessor& p) { char k[24]; std::snprintf (k, sizeof (k), "#%016llx", (unsigned long long) uidOf (p)); return std::string (k); };
        auto makeTap = [] (const juce::MemoryBlock* st)
        {
            auto r = std::make_unique<Rig2> ("TAP");
            if (st != nullptr) r->p->setStateInformation (st->getData(), (int) st->getSize());
            else r->set ("role", (float) ROLE_PERC);
            return r;
        };
        int64_t t = 0;
        auto runAll = [&] (const std::vector<Rig2*>& rigs, double secs)
        {
            for (int k = 0; k < (int) (kSr * secs / kBlock); ++k, t += kBlock)
                for (auto* r : rigs)
                    r->step (kBlock, [&] (juce::AudioBuffer<float>& bb, juce::MidiBuffer&, int n)
                    {
                        for (int i = 0; i < n; ++i) { const float v = 0.3f * (float) std::sin (6.283185307 * 180.0 * (double) (t + i) / kSr); bb.setSample (0, i, v); bb.setSample (1, i, v); }
                    });
        };
        auto cutOf = [&] (SFProcessor& p)
        {
            const uint64_t u = uidOf (p);
            auto& d = Bus::get().d();
            for (int s = 0; s < kSources; ++s) if (Bus::get().sourceAlive (s) && d.src[s].uid.load() == u) return d.src[s].cut[3].load();
            return -1.0f;
        };
        auto setAlloc = [] (Rig2& al, const juce::MemoryBlock* st, float pri)
        {
            if (st != nullptr) al.p->setStateInformation (st->getData(), (int) st->getSize());   // ALLOC reads its order
            al.set ("depth4", 1.0f); al.set ("q4", 1.0f); al.set ("pri4", pri);
        };

        resetBus();
        auto A = makeTap (nullptr), B = makeTap (nullptr);
        runAll ({ A.get(), B.get() }, 0.3);
        auto AL = std::make_unique<Rig2> ("ALLOC");
        {
            juce::MemoryBlock mb = stateOf (*AL->p);
            auto root = juce::ValueTree::readFromData (mb.getData(), mb.getSize());
            root.getChildWithName ("MODULE").setProperty ("order", juce::String (keyOf (*B->p) + "\n" + keyOf (*A->p) + "\n"), nullptr);
            juce::MemoryBlock out; { juce::MemoryOutputStream os (out, false); root.writeToStream (os); }
            setAlloc (*AL, &out, 0.0f);
        }
        runAll ({ A.get(), B.get(), AL.get() }, 1.0);
        const float a0 = cutOf (*A->p), b0 = cutOf (*B->p);
        const auto stA = stateOf (*A->p), stB = stateOf (*B->p), stAL = stateOf (*AL->p);
        const uint64_t uidA = uidOf (*A->p);
        A.reset(); B.reset(); AL.reset();

        float a1, b1, a2, b2, a3, b3;
        {
            resetBus();                                   // reopened: B starts first this time
            auto B2 = makeTap (&stB), A2 = makeTap (&stA);
            runAll ({ B2.get(), A2.get() }, 0.3);
            auto AL2 = std::make_unique<Rig2> ("ALLOC");
            setAlloc (*AL2, &stAL, 0.0f);
            runAll ({ B2.get(), A2.get(), AL2.get() }, 1.0);
            a1 = cutOf (*A2->p); b1 = cutOf (*B2->p);
        }
        resetBus();                                       // reopened: A starts first, as when stored
        auto A3 = makeTap (&stA), B3 = makeTap (&stB);
        runAll ({ A3.get(), B3.get() }, 0.3);
        auto AL3 = std::make_unique<Rig2> ("ALLOC");
        setAlloc (*AL3, &stAL, 0.0f);
        runAll ({ A3.get(), B3.get(), AL3.get() }, 1.0);
        a2 = cutOf (*A3->p); b2 = cutOf (*B3->p);
        juce::AudioProcessor::TrackProperties tp; tp.name = "HATS";
        B3->p->updateTrackProperties (tp);                // B's track renamed
        runAll ({ A3.get(), B3.get(), AL3.get() }, 0.2);
        setAlloc (*AL3, &stAL, 0.0f);
        runAll ({ A3.get(), B3.get(), AL3.get() }, 1.0);
        a3 = cutOf (*A3->p); b3 = cutOf (*B3->p);
        auto C = makeTap (&stA);                          // A's track duplicated
        runAll ({ A3.get(), B3.get(), C.get(), AL3.get() }, 1.5);
        setAlloc (*AL3, &stAL, 0.5f);                     // PRIORITY on the middle of three: A's place
        runAll ({ A3.get(), B3.get(), C.get(), AL3.get() }, 1.0);
        const float a4 = cutOf (*A3->p), b4 = cutOf (*B3->p), c4 = cutOf (*C->p);
        const bool newId = uidOf (*C->p) != uidOf (*A3->p) && uidOf (*A3->p) == uidA;
        std::printf ("   ALLOC order B > A, cut A / B in dB: as stored %.1f / %.1f; reopened, B first %.1f / %.1f; A first %.1f / %.1f; B renamed %.1f / %.1f\n", a0, b0, a1, b1, a2, b2, a3, b3);
        std::printf ("   A duplicated: copy has its own id: %s; PRIORITY on place 2: cut A %.1f, B %.1f, copy %.1f dB\n", newId ? "yes" : "NO", a4, b4, c4);
        CHECK (b0 < 0.5f && a0 > 9.0f, "ALLOC order did not apply (A %.1f, B %.1f)", a0, b0);
        CHECK (b1 < 0.5f && a1 > 9.0f, "ALLOC order lost when the set reopened with B starting first (A %.1f, B %.1f)", a1, b1);
        CHECK (b2 < 0.5f && a2 > 9.0f, "ALLOC order lost when the set reopened (A %.1f, B %.1f)", a2, b2);
        CHECK (b3 < 0.5f && a3 > 9.0f, "ALLOC order lost when a track was renamed (A %.1f, B %.1f)", a3, b3);
        CHECK (newId, "a duplicated track kept its original's ALLOC id");
        CHECK (a4 < 0.5f && b4 > 9.0f && c4 > 9.0f, "ALLOC: the original lost its place to its copy (A %.1f, B %.1f, copy %.1f)", a4, b4, c4);
    }
    resetBus();
}

// ------------------------------------------------------------------------------------------
// Regressions for the second independent review (Nov 3).
static void testSecondReview()
{
    std::printf ("-- second review regressions\n");
    auto& d = Bus::get().d();
    auto paramIndex = [] (SFProcessor& p, const char* id) { const auto& sp = p.allSpecs(); for (size_t k = 0; k < sp.size(); ++k) if (sp[k].id == id) return (int) k; return -1; };
    auto brightness = [] (const std::vector<float>& o, double a, double b)
    {
        double e = 0, h = 0;
        for (size_t i = (size_t) (a * kSr) + 2; i < (size_t) (b * kSr) && i < o.size(); ++i) { e += o[i] * o[i]; const double d2 = o[i] - 2.0 * o[i - 1] + o[i - 2]; h += d2 * d2; }
        return 10.0 * std::log10 (h / (e + 1e-30) + 1e-30);
    };

    // 1. Releasing PANIC never steps. The input comes back over 5 ms too, so an effect with latency
    //    (DELAY's dry path runs 512 samples late) doesn't play a hard edge after the output ramp.
    {
        resetBus();
        Rig2 ctl ("CONTROL"), dl ("DELAY");
        dl.set ("mix", 0.0f);
        int64_t t = 0;
        std::vector<float> o;
        for (int b = 0; b < (int) (1.6 * kSr) / kBlock; ++b)
        {
            const double sec = (double) (b * kBlock) / kSr;
            ctl.set ("panic", sec >= 0.4 && sec < 0.9 ? 1.0f : 0.0f);
            ctl.step (kBlock, nullptr);
            dl.step (kBlock, [&] (juce::AudioBuffer<float>& bb, juce::MidiBuffer&, int n)
            {
                for (int i = 0; i < n; ++i) { const float v = 0.3f * (float) std::sin (6.283185307 * 200.0 * (double) (t + i) / kSr); bb.setSample (0, i, v); bb.setSample (1, i, v); }
                t += n;
            });
            for (int i = 0; i < kBlock; ++i) o.push_back (dl.buf.getSample (0, i));
        }
        float stepMax = 0.0f, back = 0.0f;
        for (size_t i = (size_t) (0.9 * kSr); i < (size_t) (1.2 * kSr); ++i) stepMax = std::max (stepMax, std::fabs (o[i] - o[i - 1]));
        for (size_t i = (size_t) (1.3 * kSr); i < o.size(); ++i) back = std::max (back, std::fabs (o[i]));
        const float natural = 0.3f * 6.2831853f * 200.0f / (float) kSr;
        std::printf ("   PANIC released on DELAY (dry path 512 samples late): largest step %.4f (the sine's own %.4f), level back %.2f\n", stepMax, natural, back);
        CHECK (stepMax < 1.5f * natural && back > 0.25f, "PANIC release steps on a latent effect (%.4f, back %.2f)", stepMax, back);
    }

    // 2. REVERB FREEZE pressed while music plays: the input fades out into the frozen loop instead
    //    of being cut (a cut circulates for as long as FREEZE is held). 300 Hz sine, wet only.
    {
        resetBus();
        Rig2 r ("REVERB");
        r.set ("mix", 1.0f); r.set ("dry", 0.0f); r.set ("decay", 3.0f); r.set ("duck", 0.0f);
        int64_t t = 0;
        std::vector<float> o;
        for (int b = 0; b < (int) (6.0 * kSr) / kBlock; ++b)
        {
            r.set ("freeze", (double) (b * kBlock) / kSr >= 2.0 ? 1.0f : 0.0f);
            r.step (kBlock, [&] (juce::AudioBuffer<float>& bb, juce::MidiBuffer&, int n)
            {
                for (int i = 0; i < n; ++i) { const float v = 0.3f * (float) std::sin (6.283185307 * 300.0 * (double) (t + i) / kSr); bb.setSample (0, i, v); bb.setSample (1, i, v); }
                t += n;
            });
            for (int i = 0; i < kBlock; ++i) o.push_back (r.buf.getSample (0, i));
        }
        const double before = brightness (o, 1.4, 2.0), frozen = brightness (o, 4.0, 6.0);
        std::printf ("   REVERB FREEZE on a playing 300 Hz sine: brightness %.1f dB before, %.1f dB frozen\n", before, frozen);
        CHECK (frozen - before < 6.0, "REVERB FREEZE cut the input into the loop (brightness %+.1f dB)", frozen - before);
    }

    // 3. DELAY FREEZE while TIME moves: the head reads between samples until it settles (whole-
    //    sample reads while moving zipper). 1 kHz repeats frozen, then TIME 1/4 -> 1/4D, SLEW 2 s.
    {
        resetBus();
        Rig2 r ("DELAY");
        r.head.bpm = 120.0;
        r.set ("mix", 1.0f); r.set ("dry", 0.0f); r.set ("feedback", 0.6f); r.set ("wow", 0.0f); r.set ("duck", 0.0f);
        r.set ("time", 2.0f); r.set ("slew", 2000.0f);
        int64_t t = 0;
        std::vector<float> o;
        for (int b = 0; b < (int) (3.5 * kSr) / kBlock; ++b)
        {
            const double sec = (double) (b * kBlock) / kSr;
            r.set ("freeze", sec >= 1.2 ? 1.0f : 0.0f);
            if (sec >= 1.6) r.set ("time", 3.0f);
            r.step (kBlock, [&] (juce::AudioBuffer<float>& bb, juce::MidiBuffer&, int n)
            {
                for (int i = 0; i < n; ++i) { const float v = (t + i) < (int64_t) kSr ? 0.3f * (float) std::sin (6.283185307 * 1000.0 * (double) (t + i) / kSr) : 0.0f; bb.setSample (0, i, v); bb.setSample (1, i, v); }
                t += n;
            });
            for (int i = 0; i < kBlock; ++i) o.push_back (r.buf.getSample (0, i));
        }
        const double settled = brightness (o, 1.4, 1.6), moving = brightness (o, 2.2, 3.2);
        std::printf ("   DELAY FREEZE, TIME moving: brightness %.1f dB settled, %.1f dB while the head moves\n", settled, moving);
        CHECK (moving - settled < 3.0, "DELAY FREEZE zippers while TIME moves (%+.1f dB)", moving - settled);
    }

    // 4. DRY on / off ramps over 5 ms counted in samples: the same at every buffer size, no step.
    {
        auto run = [&] (int blk)
        {
            resetBus();
            Rig2 r ("DELAY", kSr, blk);
            r.set ("mix", 0.0f);
            std::vector<float> o;
            int64_t t = 0;
            while (t < (int64_t) (0.5 * kSr))
            {
                r.set ("dry", t < 12288 ? 1.0f : 0.0f);
                r.step (blk, [&] (juce::AudioBuffer<float>& bb, juce::MidiBuffer&, int n)
                {
                    for (int i = 0; i < n; ++i) { const float v = 0.3f * (float) std::sin (6.283185307 * 200.0 * (double) (t + i) / kSr); bb.setSample (0, i, v); bb.setSample (1, i, v); }
                });
                for (int i = 0; i < blk; ++i) o.push_back (r.buf.getSample (0, i));
                t += blk;
            }
            return o;
        };
        const auto a = run (16), b = run (64), c2 = run (256);
        float diff = 0.0f, stepMax = 0.0f;
        for (size_t i = 0; i < std::min ({ a.size(), b.size(), c2.size() }); ++i) diff = std::max ({ diff, std::fabs (a[i] - b[i]), std::fabs (a[i] - c2[i]) });
        for (size_t i = 12288; i < 12288 + 2000 && i < a.size(); ++i) stepMax = std::max (stepMax, std::fabs (a[i] - a[i - 1]));
        const float natural = 0.3f * 6.2831853f * 200.0f / (float) kSr;
        std::printf ("   DRY off on DELAY at buffer sizes 16 / 64 / 256: largest difference %.1e; largest step %.4f (the sine's own %.4f)\n", diff, stepMax, natural);
        CHECK (diff == 0.0f && stepMax < 1.5f * natural, "DRY switch depends on the buffer size or steps (%.1e, %.4f)", diff, stepMax);
    }

    // 5. STORE ALL right after a recall stores the recalled values (Live hasn't reported them back
    //    yet; it used to store the values from before the recall into the new song).
    {
        resetBus();
        Rig2 r ("FILTER");
        r.head.playing = false;
        auto dc = [] (juce::AudioBuffer<float>& b, juce::MidiBuffer&, int n) { for (int c2 = 0; c2 < 2; ++c2) for (int i = 0; i < n; ++i) b.setSample (c2, i, 0.5f); };
        auto blocks = [&] (int k) { for (int i = 0; i < k; ++i) r.step (kBlock, dc); };
        const int lvl = paramIndex (*r.p, "level");
        d.song = 1; r.set ("level", -6.0f); blocks (4);
        r.p->storeSong(); blocks (2);
        r.set ("level", 0.0f); blocks (4);
        d.song = 1; d.songAtPpq = -1.0; d.songSeq.fetch_add (1);        // recall song 2 (stopped: at once)
        blocks (1);
        d.storeSeq.fetch_add (1);                                       // STORE ALL straight away
        blocks (2);
        const float stored = r.p->mod().getSongValue (1, lvl);
        std::printf ("   STORE ALL 5 ms after recalling a song with LEVEL -6 dB: stores %.1f dB\n", stored);
        CHECK (std::abs (stored + 6.0f) < 0.01f, "STORE ALL right after a recall stored the old values (%.1f dB)", stored);
    }

    // 6. A recall requested inside an Arrangement loop whose bar line the loop never reaches
    //    (loop beats 2-6, request at beat 5: CONTROL's bar is beat 8). Every plugin lands on the
    //    next bar line after the jump back (beat 4), together.
    {
        resetBus();
        Rig2 before ("FILTER"), ctl ("CONTROL"), after ("FILTER");
        for (auto* r : { &before, &ctl, &after }) { r->head.bpm = 120.0; r->ppq = 0.0; }
        auto dc = [] (juce::AudioBuffer<float>& b, juce::MidiBuffer&, int n) { for (int c2 = 0; c2 < 2; ++c2) for (int i = 0; i < n; ++i) b.setSample (c2, i, 0.5f); };
        auto quiet = [] (juce::AudioBuffer<float>&, juce::MidiBuffer&, int) {};
        bool loop = false;
        double changeB = -1.0, changeA = -1.0;
        auto cycle = [&]
        {
            const double p0 = before.ppq, bps = before.bps();
            before.step (kBlock, dc); ctl.step (kBlock, quiet); after.step (kBlock, dc);
            for (int i = 0; i < kBlock; ++i)
            {
                if (changeB < 0.0 && before.buf.getSample (0, i) < 0.5f * (1.0f - 1e-4f)) changeB = p0 + i * bps;
                if (changeA < 0.0 && after.buf.getSample (0, i) < 0.5f * (1.0f - 1e-4f)) changeA = p0 + i * bps;
            }
            if (loop && ctl.ppq >= 6.0) for (auto* r : { &before, &ctl, &after }) r->ppq -= 4.0;
        };
        ctl.set ("song", 1.0f); cycle();
        before.set ("level", -6.0f); after.set ("level", -6.0f); cycle();
        ctl.set ("store_all", 1.0f); cycle(); ctl.set ("store_all", 0.0f); cycle();
        ctl.set ("song", 0.0f); cycle();
        before.set ("level", 0.0f); after.set ("level", 0.0f);
        for (int k = 0; k < (int) (0.5 * kSr) / kBlock; ++k) cycle();
        while (ctl.ppq < 2.0 || ctl.ppq >= 6.0) cycle();       // inside the loop range
        loop = true;
        changeB = changeA = -1.0;
        while (ctl.ppq < 5.0) cycle();
        ctl.set ("song", 1.0f);
        for (int k = 0; k < (int) (6.0 * kSr) / kBlock && (changeB < 0.0 || changeA < 0.0); ++k) cycle();
        std::printf ("   recall inside a loop that never reaches CONTROL's bar: lands at beat %.3f / %.3f (want 4.000)\n", changeB, changeA);
        CHECK (std::abs (changeB - 4.0) < 1e-3 && std::abs (changeA - 4.0) < 1e-3, "recall in a loop waited or split (%.3f / %.3f)", changeB, changeA);
    }

    // 7. KEY from a song recall switches on the bar in every plugin, whichever side of CONTROL Live
    //    processes it on (it used to switch a block early after CONTROL and a block late before it).
    {
        resetBus();
        const int blk = 1000;
        Rig2 before ("BASS", kSr, blk), ctl ("CONTROL", kSr, blk), after ("BASS", kSr, blk);
        for (auto* r : { &before, &ctl, &after }) { r->head.bpm = 123.0; r->ppq = 0.0; }      // bar 12 mid-block
        before.set ("model", 5.0f); after.set ("model", 5.0f);
        before.set ("glide", 0.0f); after.set ("glide", 0.0f);         // the pitch jumps (no glide to blur when)
        bool started = false;
        std::vector<float> ob, oa;
        auto note = [&] (juce::AudioBuffer<float>&, juce::MidiBuffer& m, int) { if (! started) m.addEvent (juce::MidiMessage::noteOn (1, 72, (juce::uint8) 100), 0); };
        auto quiet = [] (juce::AudioBuffer<float>&, juce::MidiBuffer&, int) {};
        auto cycle = [&]
        {
            before.step (blk, note); ctl.step (blk, quiet); after.step (blk, note); started = true;
            for (int i = 0; i < blk; ++i) { ob.push_back (before.buf.getSample (0, i)); oa.push_back (after.buf.getSample (0, i)); }
        };
        ctl.set ("key", 1.0f); cycle();                                // song 1: C
        ctl.set ("store_all", 1.0f); cycle(); ctl.set ("store_all", 0.0f); cycle();
        ctl.set ("song", 1.0f); cycle();                               // song 2: D
        ctl.set ("key", 3.0f); cycle();
        ctl.set ("store_all", 1.0f); cycle(); ctl.set ("store_all", 0.0f); cycle();
        ctl.set ("song", 0.0f);                                        // back to song 1 (C) on its bar
        while (ctl.ppq < 5.0) cycle();
        ctl.set ("key", 1.0f);                                         // Live reports the recalled KEY back
        while (ctl.ppq < 9.5) cycle();
        ctl.set ("song", 1.0f);                                        // song 2 mid-bar: its bar is beat 12
        while (ctl.ppq < 14.0) cycle();
        const double barSample = 12.0 / ctl.bps() + before.p->getLatencySamples();     // BASS reports Pocket's lookahead
        // The first zero crossing from which the half periods are the new key's (D5 against C5).
        auto change = [&] (const std::vector<float>& o)
        {
            std::vector<size_t> zc;
            for (size_t i = (size_t) barSample - 6000; i < (size_t) barSample + 6000; ++i) if ((o[i - 1] < 0.0f) != (o[i] < 0.0f)) zc.push_back (i);
            const double hC = kSr / (2.0 * 523.25), hD = kSr / (2.0 * 587.33), mid = 0.5 * (hC + hD);
            for (size_t k = 0; k + 4 < zc.size(); ++k)
            {
                bool allD = true;
                for (size_t j = k; j < k + 4; ++j) allD = allD && (double) (zc[j + 1] - zc[j]) < mid;
                if (allD) return (double) zc[k] - barSample;
            }
            return 1e9;
        };
        const double eB = change (ob), eA = change (oa);
        std::printf ("   KEY C -> D with a song recall, BASS held note, buffer 1000: changes %.0f / %.0f samples from the bar (before / after CONTROL)\n", eB, eA);
        CHECK (std::abs (eB) < 100.0 && std::abs (eA) < 100.0 && std::abs (eA - eB) < 60.0, "KEY changed off the bar (%.0f / %.0f samples)", eB, eA);
    }

    // 8. The band-limited sample reader costs the same however far up a sample is played: it reads
    //    the octave-down copy where the rate is between 1 and 2 (reading the original at rate r
    //    costs about 18 r taps: a sample played eight octaves up used to cost 4,600).
    {
        juce::AudioBuffer<float> b (2, 48000);
        for (int i = 0; i < 48000; ++i) { const float v = 0.5f * (float) std::sin (6.283185307 * 100.0 * i / kSr); b.setSample (0, i, v); b.setSample (1, i, v); }
        Mips m; m.build (b);
        auto timeRead = [&] (double rate, bool mips)
        {
            const auto t0 = std::chrono::high_resolution_clock::now();
            volatile float sink = 0.0f;
            double pos = 100.0;
            for (int k = 0; k < 20000; ++k)
            {
                sink = sink + (mips ? readAt (b, m, 0, pos, rate) : sampleAt (b.getReadPointer (0), 48000, pos, rate));
                pos += 1.37; if (pos > 40000.0) pos = 100.0;
            }
            return std::chrono::duration<double> (std::chrono::high_resolution_clock::now() - t0).count();
        };
        const double base = std::min (timeRead (1.5, true), timeRead (1.5, true));
        const double high = std::min (timeRead (256.0, true), timeRead (256.0, true));
        const double old = timeRead (256.0, false);
        // Accuracy: 100 Hz read at rate 16 is a 1.6 kHz tone at the same level.
        std::vector<float> o;
        for (int i = 0; i < 2400; ++i) o.push_back (readAt (b, m, 0, 200.0 + i * 16.0, 16.0));
        const double a16 = hannAmp (o, 0, o.size(), 1600.0, kSr);
        std::printf ("   sample reader: rate 256 costs %.1f x rate 1.5 (reading the original: %.0f x); 100 Hz at rate 16: %.2f dB\n", high / base, old / base, 20.0 * std::log10 (a16 / 0.5));
        CHECK (high < 3.0 * base, "sample reader cost grows with the rate (%.1f x)", high / base);
        CHECK (std::abs (20.0 * std::log10 (a16 / 0.5)) < 0.1, "octave-down read is off level (%.2f dB)", 20.0 * std::log10 (a16 / 0.5));
    }
    resetBus();
}

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


// =====================================================================================
// Third review (Nov 3): a recall's report to Live against a newer knob move, a recall asked for
// across a loop wrap, DRIVE's LOW KEEP at partial MIX, STORE TO.
static void testThirdReview()
{
    std::printf ("-- third review regressions\n");
    auto& d = Bus::get().d();
    auto dc = [] (juce::AudioBuffer<float>& b, juce::MidiBuffer&, int n) { for (int c2 = 0; c2 < 2; ++c2) for (int i = 0; i < n; ++i) b.setSample (c2, i, 0.5f); };
    auto recall = [&] (int s) { d.song = s; d.songAtPpq = -1.0; d.songKey = -2; d.songSeq.fetch_add (1); };

    // 1. A recall is reported to Live by a timer; a value Live gets after the recall (a knob, automation)
    //    is newer and stays, whether the timer or the next audio block comes first.
    {
        auto run = [&] (bool reportFirst, const char* what, const std::function<void (Rig2&)>& between, float want)
        {
            resetBus();
            Rig2 r ("FILTER");
            r.head.playing = false;
            auto blocks = [&] (int k) { for (int i = 0; i < k; ++i) r.step (kBlock, dc); };
            d.song = 1; r.set ("level", -6.0f); blocks (2); r.p->storeSong(); blocks (2);    // song 2: -6 dB
            d.song = 2; r.set ("level", 2.0f); blocks (2); r.p->storeSong(); blocks (2);     // song 3: +2 dB
            r.set ("level", 0.0f); blocks (130); r.p->flushHostNotifications();
            between (r);
            if (reportFirst) { r.p->flushHostNotifications(); blocks (1); }
            else { blocks (1); r.p->flushHostNotifications(); }
            blocks (130); r.p->flushHostNotifications(); blocks (2);
            const float live = r.p->apvts.getRawParameterValue ("level")->load(), heard = r.p->currentValue (r.p->paramIndex ("level"));
            std::printf ("   %s (%s): Live %.2f dB, heard %.2f dB, want %.2f\n", what, reportFirst ? "report first" : "audio first", live, heard, want);
            CHECK (std::abs (live - want) < 0.01f && std::abs (heard - want) < 0.01f, "%s (%s): Live %.2f, heard %.2f dB, want %.2f",
                   what, reportFirst ? "report first" : "audio first", live, heard, want);
        };
        for (const bool reportFirst : { true, false })
        {
            run (reportFirst, "recall -6, knob to +3", [&] (Rig2& r) { recall (1); r.step (kBlock, dc); r.set ("level", 3.0f); }, 3.0f);
            run (reportFirst, "recall -6, knob to +3 then +1", [&] (Rig2& r) { recall (1); r.step (kBlock, dc); r.set ("level", 3.0f); r.set ("level", 1.0f); }, 1.0f);
            run (reportFirst, "recall -6, recall +2", [&] (Rig2& r) { recall (1); r.step (kBlock, dc); recall (2); r.step (kBlock, dc); }, 2.0f);
            run (reportFirst, "recall -6 reported, recall +2, Live echoes -6",
                 [&] (Rig2& r) { recall (1); r.step (kBlock, dc); r.p->flushHostNotifications(); r.step (kBlock, dc); recall (2); r.step (kBlock, dc); r.set ("level", -6.0f); }, 2.0f);
        }
    }

    // 2. A song asked for in the last block before an Arrangement loop wraps lands at the wrap in SF
    //    CONTROL and in every plugin, before or after it in Live's order ([0, 4) loop, 120 BPM). It
    //    used to leave CONTROL and the plugins before it waiting for beat 4 for ever. In a loop with
    //    no bar line in it ([1, 4)) the song lands at the next wrap, everywhere at once.
    for (const double loopStart : { 0.0, 1.0 })
    {
        resetBus();
        const int blk = 250;                                   // a whole number of blocks per loop
        Rig2 before ("FILTER", kSr, blk), ctl ("CONTROL", kSr, blk), after ("FILTER", kSr, blk);
        for (auto* r : { &before, &ctl, &after }) { r->head.bpm = 120.0; r->head.playing = false; }
        auto quiet = [] (juce::AudioBuffer<float>&, juce::MidiBuffer&, int) {};
        auto cycle = [&] { before.step (blk, dc); ctl.step (blk, quiet); after.step (blk, dc); };
        ctl.set ("song", 1.0f); for (int k = 0; k < 3; ++k) cycle();
        before.set ("level", -6.0f); after.set ("level", -6.0f); cycle();
        ctl.set ("store_all", 1.0f); cycle(); ctl.set ("store_all", 0.0f); cycle();
        ctl.set ("song", 0.0f); for (int k = 0; k < 3; ++k) cycle();
        before.set ("level", 0.0f); after.set ("level", 0.0f);
        for (int k = 0; k < 130; ++k) cycle();
        for (auto* r : { &before, &ctl, &after }) r->p->flushHostNotifications();
        const double bps = before.bps(), blockBeats = blk * bps;
        for (auto* r : { &before, &ctl, &after }) { r->head.playing = true; r->ppq = 4.0 - blockBeats; }
        ctl.set ("song", 1.0f);
        cycle();                                               // the last block before the wrap
        const int perLoop = (int) std::lround ((4.0 - loopStart) / blockBeats);
        int firstB = -1, firstA = -1;
        for (int k = 0; k < 3 * perLoop; ++k)
        {
            for (auto* r : { &before, &ctl, &after }) r->ppq = loopStart + (k % perLoop) * blockBeats;
            cycle();
            if (firstB < 0 && before.p->currentValue (before.p->paramIndex ("level")) < -0.001f) firstB = k;
            if (firstA < 0 && after.p->currentValue (after.p->paramIndex ("level")) < -0.001f) firstA = k;
            if (k % 6 == 5) for (auto* r : { &before, &ctl, &after }) r->p->flushHostNotifications();
        }
        const int want = loopStart == 0.0 ? 0 : perLoop;
        std::printf ("   song asked for in the last block of a [%.0f, 4) loop: lands %d / %d blocks after the wrap (want %d), CONTROL still waiting %d\n",
                     loopStart, firstB, firstA, want, (int) ctl.p->recallWaiting());
        CHECK (firstB == want && firstA == want && ! before.p->recallWaiting() && ! after.p->recallWaiting() && ! ctl.p->recallWaiting(),
               "song across a loop wrap ([%.0f, 4)): blocks %d / %d, CONTROL waiting %d", loopStart, firstB, firstA, (int) ctl.p->recallWaiting());
    }

    // 3. DRIVE, LOW KEEP at partial MIX, linear range (-60 dBFS, DRIVE 0, TAPE, TONE 0, AUTO off): flat
    //    at every MIX and LOW KEEP. (Blending the crossover's sum with the unsplit dry cancelled
    //    -104 dB at the crossover frequency at MIX 50 %.)
    {
        float worst = 0.0f;
        double wHz = 0.0, wKeep = 0.0, wMix = 0.0;
        for (const float keep : { 60.0f, 120.0f, 250.0f })
            for (const float mix : { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f })
                for (const double hz : { 30.0, 60.0, 90.0, 120.0, 180.0, 250.0, 400.0, 1000.0, 6000.0 })
                {
                    resetBus();
                    Rig2 r ("DRIVE");
                    r.set ("drive", 0.0f); r.set ("character", 0.0f); r.set ("tone", 0.0f); r.set ("low_keep", keep);
                    r.set ("mix", mix); r.set ("auto_gain", 0.0f); r.set ("level", 0.0f);
                    int64_t t = 0;
                    double ein = 0.0, eout = 0.0;
                    while (t < (int64_t) (0.8 * kSr))
                    {
                        r.step (kBlock, [&] (juce::AudioBuffer<float>& b, juce::MidiBuffer&, int n)
                        { for (int i = 0; i < n; ++i) { const float v = 0.001f * (float) std::sin (6.283185307 * hz * (double) (t + i) / kSr); b.setSample (0, i, v); b.setSample (1, i, v); } });
                        for (int i = 0; i < kBlock; ++i)
                            if (t + i >= (int64_t) (0.4 * kSr))
                            {
                                const double x = 0.001 * std::sin (6.283185307 * hz * (double) (t + i) / kSr), y = r.buf.getSample (0, i);
                                ein += x * x; eout += y * y;
                            }
                        t += kBlock;
                    }
                    const float g = (float) (10.0 * std::log10 (eout / ein));
                    if (std::abs (g) > std::abs (worst)) { worst = g; wHz = hz; wKeep = keep; wMix = mix; }
                }
        std::printf ("   DRIVE LOW KEEP 60/120/250 Hz x MIX 0-100 %%, linear range: largest deviation %.3f dB (%.0f Hz, LOW KEEP %.0f, MIX %.0f %%)\n",
                     worst, wHz, wKeep, wMix * 100.0);
        CHECK (std::abs (worst) < 0.1f, "DRIVE LOW KEEP not flat at partial MIX (%.3f dB at %.0f Hz, LOW KEEP %.0f, MIX %.0f %%)", worst, wHz, wKeep, wMix * 100.0);

        // LOW KEEP switched on and off under a 100 Hz tone at MIX 50 %: no step.
        resetBus();
        Rig2 r ("DRIVE");
        r.set ("drive", 0.0f); r.set ("auto_gain", 0.0f); r.set ("mix", 0.5f);
        std::vector<float> o;
        int64_t t = 0;
        for (int b = 0; b < (int) (1.5 * kSr) / kBlock; ++b)
        {
            const double sec = (double) t / kSr;
            r.set ("low_keep", sec > 0.5 && sec < 1.0 ? 80.0f : 0.0f);
            r.step (kBlock, [&] (juce::AudioBuffer<float>& bb, juce::MidiBuffer&, int n)
            { for (int i = 0; i < n; ++i) { const float v = 0.3f * (float) std::sin (6.283185307 * 100.0 * (double) (t + i) / kSr); bb.setSample (0, i, v); bb.setSample (1, i, v); } });
            for (int i = 0; i < kBlock; ++i) o.push_back (r.buf.getSample (0, i));
            t += kBlock;
        }
        float stepMax = 0.0f;
        for (size_t i = (size_t) (0.2 * kSr); i < o.size(); ++i) stepMax = std::max (stepMax, std::fabs (o[i] - o[i - 1]));
        const float natural = 0.3f * 6.2831853f * 100.0f / (float) kSr;
        std::printf ("   LOW KEEP on / off under a 100 Hz tone, MIX 50 %%: largest step %.5f (the tone's own %.5f)\n", stepMax, natural);
        CHECK (stepMax < 1.5f * natural, "LOW KEEP switching steps (%.5f against %.5f)", stepMax, natural);
    }

    // 4. STORE TO a song that already holds a sound stores what plays now; the song isn't recalled
    //    first (choosing SONG recalls it: the edits were gone before STORE ALL could take them). It
    //    becomes the song in force; SONG follows and STORE TO goes back to ACTIVE SONG.
    {
        resetBus();
        Rig2 ctl ("CONTROL"), f ("FILTER");
        for (auto* r : { &ctl, &f }) r->head.playing = false;
        auto quiet = [] (juce::AudioBuffer<float>&, juce::MidiBuffer&, int) {};
        float lowest = 1.0f;
        auto cycle = [&] { ctl.step (kBlock, quiet); f.step (kBlock, dc); lowest = std::min (lowest, f.buf.getMagnitude (0, 0, kBlock) / 0.5f); };
        const int lvl = f.p->paramIndex ("level");
        for (int k = 0; k < 4; ++k) cycle();                                  // song 1 in force
        f.set ("level", -12.0f); for (int k = 0; k < 130; ++k) cycle();
        ctl.set ("store_to", 3.0f); ctl.set ("store_all", 1.0f); cycle(); ctl.set ("store_all", 0.0f); cycle(); cycle();
        const float first = f.p->mod().getSongValue (2, lvl);
        ctl.set ("store_to", 0.0f); ctl.set ("song", 0.0f); for (int k = 0; k < 4; ++k) cycle();      // back on song 1
        f.set ("level", 0.0f); for (int k = 0; k < 130; ++k) cycle();
        f.p->flushHostNotifications();
        lowest = 1.0f;
        const uint32_t seq = d.songSeq.load();
        ctl.set ("store_to", 3.0f); ctl.set ("store_all", 1.0f); cycle(); ctl.set ("store_all", 0.0f);
        for (int k = 0; k < 130; ++k) cycle();
        const float second = f.p->mod().getSongValue (2, lvl);
        const bool recalled = d.songSeq.load() != seq;
        juce::MessageManager::getInstance()->runDispatchLoopUntil (150);    // CONTROL's timer: SONG, STORE TO
        for (int k = 0; k < 130; ++k) cycle();
        const bool recalledLater = d.songSeq.load() != seq;                  // SONG following the store is no request
        const int songParam = (int) ctl.p->apvts.getRawParameterValue ("song")->load(), storeTo = (int) ctl.p->apvts.getRawParameterValue ("store_to")->load();
        std::printf ("   STORE TO song 3 (held -12 dB) while playing 0 dB: song 3 now %.1f dB (first store %.1f), quietest %.2f dB, a recall sent %d,"
                     " song in force %d, SONG %d, STORE TO %d\n", second, first, gainToDb (lowest), (int) recalled, d.song.load() + 1, songParam + 1, storeTo);
        CHECK (std::abs (first + 12.0f) < 0.01f && std::abs (second) < 0.01f, "STORE TO stored %.1f / %.1f dB", first, second);
        CHECK (lowest > 0.999f && ! recalled && ! recalledLater, "STORE TO recalled the song (quietest %.2f dB, recall %d / %d)", gainToDb (lowest), (int) recalled, (int) recalledLater);
        CHECK (d.song.load() == 2 && f.p->activeSong() == 2 && songParam == 2 && storeTo == 0,
               "after STORE TO: song in force %d, FILTER %d, SONG %d, STORE TO %d", d.song.load(), f.p->activeSong(), songParam, storeTo);
    }

    // 5. STORE TO while a song change waits for its bar (a Program Change from the OXI at a
    //    pattern change): the sound playing now goes into the STORE TO song, the change still
    //    lands on the bar, and the song in force stays the one it brings. (It used to become the
    //    STORE TO song: SF CONTROL showed it while every plugin played the other, and the next
    //    STORE ALL wrote that one's sound over it.) A plugin's own STORE afterwards goes into the
    //    song in force, not the STORE TO song.
    {
        resetBus();
        Rig2 ctl ("CONTROL"), f ("FILTER");
        for (auto* r : { &ctl, &f }) { r->head.playing = false; r->head.bpm = 120.0; }
        auto quiet = [] (juce::AudioBuffer<float>&, juce::MidiBuffer&, int) {};
        auto cycle = [&] { ctl.step (kBlock, quiet); f.step (kBlock, dc); };
        const int lvl = f.p->paramIndex ("level");
        auto song = [&] (int s) { ctl.set ("song", (float) s); for (int k = 0; k < 4; ++k) cycle(); };
        auto storeAll = [&] { ctl.set ("store_all", 1.0f); cycle(); ctl.set ("store_all", 0.0f); for (int k = 0; k < 3; ++k) cycle(); };
        auto settle = [&] { for (int k = 0; k < 130; ++k) cycle(); f.p->flushHostNotifications(); ctl.p->flushHostNotifications(); };
        for (int k = 0; k < 4; ++k) cycle();
        f.set ("level", -3.0f); settle(); storeAll();                     // song 1: -3 dB
        song (1); f.set ("level", 2.0f); settle(); storeAll();          // song 2: +2 dB
        song (2); f.set ("level", -6.0f); settle(); storeAll();         // song 3: -6 dB
        song (0); settle();                                             // song 1 in force: -3 dB
        for (auto* r : { &ctl, &f }) { r->head.playing = true; r->ppq = 1.0; }
        ctl.set ("song", 1.0f);                                         // song 2, on the bar at beat 4
        for (int k = 0; k < 8; ++k) cycle();
        const bool waiting = f.p->recallWaiting() && ctl.p->recallWaiting();
        ctl.set ("store_to", 3.0f); storeAll();                         // STORE TO song 3 meanwhile
        while (f.ppq < 4.3) cycle();
        juce::MessageManager::getInstance()->runDispatchLoopUntil (150);
        for (int k = 0; k < 130; ++k) cycle();                          // the level settles
        const float song3 = f.p->mod().getSongValue (2, lvl), heard = f.p->currentValue (lvl);
        const int songParam = (int) ctl.p->apvts.getRawParameterValue ("song")->load(), storeTo = (int) ctl.p->apvts.getRawParameterValue ("store_to")->load();
        std::printf ("   STORE TO song 3 while song 2 waits for the bar: song 3 now %.2f dB (want -3), heard after the bar %.2f dB (want +2),"
                     " song in force %d / FILTER %d / SONG %d (want 2), STORE TO %d\n", song3, heard, d.song.load() + 1, f.p->activeSong() + 1, songParam + 1, storeTo);
        CHECK (waiting && std::abs (song3 + 3.0f) < 0.01f && std::abs (heard - 2.0f) < 0.01f, "STORE TO during a waiting song change (waiting %d, song 3 %.1f, heard %.1f dB)",
               (int) waiting, song3, heard);
        CHECK (d.song.load() == 1 && f.p->activeSong() == 1 && ctl.p->activeSong() == 1 && songParam == 1 && storeTo == 0,
               "STORE TO during a waiting song change: song in force %d, FILTER %d, CONTROL %d, SONG %d, STORE TO %d",
               d.song.load(), f.p->activeSong(), ctl.p->activeSong(), songParam, storeTo);
        f.set ("level", 1.0f); settle();
        f.p->storeSong(); for (int k = 0; k < 3; ++k) cycle();          // FILTER's own STORE
        const float s2 = f.p->mod().getSongValue (1, lvl), s3 = f.p->mod().getSongValue (2, lvl);
        std::printf ("   then FILTER's own STORE at +1 dB: song 2 %.1f dB (want +1), song 3 %.1f dB (want -3)\n", s2, s3);
        CHECK (std::abs (s2 - 1.0f) < 0.01f && std::abs (s3 + 3.0f) < 0.01f, "a plugin's STORE after STORE TO (song 2 %.1f, song 3 %.1f dB)", s2, s3);
    }

    // Found by a review of the above. Song positions on a fixed grid of host blocks, as Live plays
    // an Arrangement loop: the last block of a pass is cut at the loop end, the next starts at the
    // loop start exactly.
    auto songSetup = [&] (Rig2& before, Rig2& ctl, Rig2& after, int blk, float s1, float s2)
    {
        auto quiet = [] (juce::AudioBuffer<float>&, juce::MidiBuffer&, int) {};
        auto cycle = [&] { before.step (blk, dc); ctl.step (blk, quiet); after.step (blk, dc); };
        for (auto* r : { &before, &ctl, &after }) r->head.playing = false;
        ctl.set ("song", 1.0f); for (int k = 0; k < 3; ++k) cycle();
        before.set ("level", s2); after.set ("level", s2); for (int k = 0; k < 3; ++k) cycle();
        ctl.set ("store_all", 1.0f); cycle(); ctl.set ("store_all", 0.0f); for (int k = 0; k < 3; ++k) cycle();
        ctl.set ("song", 0.0f); for (int k = 0; k < 3; ++k) cycle();
        before.set ("level", s1); after.set ("level", s1); for (int k = 0; k < 130; ++k) cycle();
        ctl.set ("store_all", 1.0f); cycle(); ctl.set ("store_all", 0.0f); for (int k = 0; k < 130; ++k) cycle();
        for (auto* r : { &before, &ctl, &after }) r->p->flushHostNotifications();
    };

    // 6. 127 BPM: bar lines fall between two samples, so the last block of a loop can end a fraction
    //    of a sample past the loop end. A song asked for in that block lands at the wrap everywhere
    //    (SF CONTROL and the plugins before it used to wait for good). Asked for in the first block
    //    after a wrap of a loop with no bar line in it, everywhere on the same later wrap (the plugins
    //    after CONTROL took it for a request from before the wrap and landed a loop early).
    for (const double loopStart : { 0.0, 1.0 })
        for (const bool first : { false, true })
        {
            resetBus();
            const int blk = 256;
            const double bpm = 127.0, bps = bpm / 60.0 / kSr;
            Rig2 before ("FILTER", kSr, blk), ctl ("CONTROL", kSr, blk), after ("FILTER", kSr, blk);
            for (auto* r : { &before, &ctl, &after }) r->head.bpm = bpm;
            songSetup (before, ctl, after, blk, 0.0f, -6.0f);
            auto quiet = [] (juce::AudioBuffer<float>&, juce::MidiBuffer&, int) {};
            const int lvl = before.p->paramIndex ("level");
            for (auto* r : { &before, &ctl, &after }) r->head.playing = true;
            double q = loopStart;
            int wraps = 0, landB = -1, landC = -1, landA = -1, cycles = 0;
            bool asked = false;
            while (wraps < 8)
            {
                const int toEnd = std::max (1, (int) std::ceil ((4.0 - q) / bps - 1e-9));     // the sample straddling the end is in
                const int n = std::min (blk, toEnd);
                const bool lastOfPass = n == toEnd;
                if (! asked && wraps == 2 && (first ? q == loopStart : lastOfPass)) { ctl.set ("song", 1.0f); asked = true; }
                for (auto* r : { &before, &ctl, &after }) r->ppq = q;
                before.step (n, dc); ctl.step (n, quiet); after.step (n, dc);
                if (asked)
                {
                    if (landB < 0 && before.p->currentValue (lvl) < -0.001f) landB = wraps;
                    if (landC < 0 && ctl.p->activeSong() == 1) landC = wraps;
                    if (landA < 0 && after.p->currentValue (lvl) < -0.001f) landA = wraps;
                }
                if (++cycles % 6 == 0) for (auto* r : { &before, &ctl, &after }) r->p->flushHostNotifications();
                q += n * bps;
                if (lastOfPass) { q = loopStart; ++wraps; }
            }
            std::printf ("   127 BPM, [%.0f, 4) loop, song asked for in the %s block %s wrap 2: lands after wrap %d / %d / %d (FILTER before CONTROL, CONTROL, FILTER after)\n",
                         loopStart, first ? "first" : "last", first ? "after" : "before", landB, landC, landA);
            CHECK (landB >= 0 && landB == landC && landC == landA && ! ctl.p->recallWaiting() && ! before.p->recallWaiting(),
                   "a [%.0f, 4) loop at 127 BPM, asked in the %s block: wraps %d / %d / %d", loopStart, first ? "first" : "last", landB, landC, landA);
        }

    // 7. STORE TO, then a Program Change from the OXI before SF CONTROL's timer has moved SONG, or
    //    in the same block: the Program Change's song lands and stays, and SONG shows it (SONG
    //    following STORE TO used to be taken for a request and won). The store has what played.
    for (const bool sameBlock : { false, true })
    {
        resetBus();
        Rig2 before ("FILTER"), ctl ("CONTROL"), after ("FILTER");
        songSetup (before, ctl, after, kBlock, -1.0f, 2.0f);
        bool pcNow = false;
        auto cycle = [&]
        {
            before.step (kBlock, dc);
            ctl.step (kBlock, [&] (juce::AudioBuffer<float>&, juce::MidiBuffer& m, int) { if (pcNow) m.addEvent (juce::MidiMessage::programChange (16, 1), 0); });
            pcNow = false;
            after.step (kBlock, dc);
        };
        ctl.set ("store_to", 3.0f); ctl.set ("store_all", 1.0f); pcNow = sameBlock;
        cycle(); ctl.set ("store_all", 0.0f);
        if (! sameBlock) { pcNow = true; cycle(); }
        juce::MessageManager::getInstance()->runDispatchLoopUntil (150);
        for (int k = 0; k < 130; ++k) cycle();
        juce::MessageManager::getInstance()->runDispatchLoopUntil (150);
        for (int k = 0; k < 130; ++k) cycle();
        const int lvl = before.p->paramIndex ("level");
        const int songParam = (int) ctl.p->apvts.getRawParameterValue ("song")->load(), storeTo = (int) ctl.p->apvts.getRawParameterValue ("store_to")->load();
        const float hb = before.p->currentValue (lvl), ha = after.p->currentValue (lvl);
        const float s3b = before.p->mod().getSongValue (2, lvl), s3a = after.p->mod().getSongValue (2, lvl);
        std::printf ("   STORE TO song 3, Program Change to song 2 %s: song in force %d, SONG %d, STORE TO %d, heard %.1f / %.1f dB (want +2), song 3 %.1f / %.1f dB (want -1)\n",
                     sameBlock ? "in the same block" : "a block later", d.song.load() + 1, songParam + 1, storeTo, hb, ha, s3b, s3a);
        CHECK (d.song.load() == 1 && songParam == 1 && storeTo == 0 && std::abs (hb - 2.0f) < 0.01f && std::abs (ha - 2.0f) < 0.01f
                 && std::abs (s3b + 1.0f) < 0.01f && std::abs (s3a + 1.0f) < 0.01f,
               "STORE TO then a Program Change (%s): song %d, SONG %d, heard %.1f / %.1f, song 3 %.1f / %.1f", sameBlock ? "same block" : "later",
               d.song.load() + 1, songParam + 1, hb, ha, s3b, s3a);
    }

    // 8. STORE ALL in the very block a song change lands in (the bar line inside it): every plugin
    //    stores the same sound and agrees on the song. (The plugins after SF CONTROL stored before
    //    the recall, those before it after; with STORE ALL into the arriving song, those after
    //    CONTROL wrote the old sound over it and kept playing it.) It goes out after the bar.
    for (const bool storeTo : { false, true })
    {
        resetBus();
        const int blk = 300;
        Rig2 before ("FILTER", kSr, blk), ctl ("CONTROL", kSr, blk), after ("FILTER", kSr, blk);
        songSetup (before, ctl, after, blk, -3.0f, 2.0f);
        auto quiet = [] (juce::AudioBuffer<float>&, juce::MidiBuffer&, int) {};
        auto cycle = [&] { before.step (blk, dc); ctl.step (blk, quiet); after.step (blk, dc); };
        for (auto* r : { &before, &ctl, &after }) { r->head.playing = true; r->ppq = 1.0; }
        ctl.set ("song", 1.0f);                                          // song 2, on the bar at beat 4
        while (before.ppq + blk * before.bps() <= 4.0) cycle();
        if (storeTo) ctl.set ("store_to", 3.0f);
        ctl.set ("store_all", 1.0f); cycle(); ctl.set ("store_all", 0.0f);   // the block with beat 4 in it
        for (int k = 0; k < 130; ++k) cycle();
        juce::MessageManager::getInstance()->runDispatchLoopUntil (150);
        for (int k = 0; k < 130; ++k) cycle();
        const int lvl = before.p->paramIndex ("level"), target = storeTo ? 2 : 1;
        const float vb = before.p->mod().getSongValue (target, lvl), va = after.p->mod().getSongValue (target, lvl);
        const float hb = before.p->currentValue (lvl), ha = after.p->currentValue (lvl);
        std::printf ("   STORE ALL%s in the block song 2 lands in: song %d holds %.1f / %.1f dB, heard %.1f / %.1f dB, song in force %d / %d / %d / bus %d\n",
                     storeTo ? " (STORE TO song 3)" : "", target + 1, vb, va, hb, ha, before.p->activeSong() + 1, ctl.p->activeSong() + 1, after.p->activeSong() + 1, d.song.load() + 1);
        CHECK (std::abs (vb - va) < 0.01f && std::abs (hb - 2.0f) < 0.01f && std::abs (ha - 2.0f) < 0.01f
                 && before.p->activeSong() == 1 && ctl.p->activeSong() == 1 && after.p->activeSong() == 1 && d.song.load() == 1,
               "STORE ALL in the bar-line block (%s): song %d %.1f / %.1f, heard %.1f / %.1f", storeTo ? "STORE TO" : "active", target + 1, vb, va, hb, ha);
    }

    // 9. A recall Live hasn't been told about yet holds, however long Live's message thread is busy
    //    (saving the set): it used to give up after 3 s, and the old song came back.
    {
        resetBus();
        Rig2 f ("FILTER");
        f.head.playing = false;
        const int lvl = f.p->paramIndex ("level");
        d.song = 1; f.set ("level", -6.0f); for (int k = 0; k < 3; ++k) f.step (kBlock, dc);
        f.p->storeSong(); f.step (kBlock, dc);
        d.song = 0; f.set ("level", 0.0f); for (int k = 0; k < 130; ++k) f.step (kBlock, dc);
        f.p->flushHostNotifications();
        recall (1);
        for (int k = 0; k < (int) (3.5 * kSr) / kBlock; ++k) f.step (kBlock, dc);       // 3.5 s without a report
        const float during = f.p->currentValue (lvl);
        f.p->flushHostNotifications();
        for (int k = 0; k < 130; ++k) f.step (kBlock, dc);
        const float after = f.p->currentValue (lvl), live = f.p->apvts.getRawParameterValue ("level")->load();
        std::printf ("   recall with Live's message thread busy for 3.5 s: heard %.2f dB then, %.2f dB after the report, Live %.2f dB (want -6)\n", during, after, live);
        CHECK (std::abs (during + 6.0f) < 0.01f && std::abs (after + 6.0f) < 0.01f && std::abs (live + 6.0f) < 0.01f,
               "a recall during a busy message thread (%.2f, %.2f, Live %.2f dB)", during, after, live);
    }
    resetBus();
}

// SF BUILD's drop: GAP (silence before the bar), TAIL (into the gap, or into the drop), DROP DUCK
// (every Pocket stage ducks to the kick after the drop).
static void testDrop()
{
    std::printf ("-- BUILD drop: gap, tail, drop duck\n");
    const double bpm = 120.0, spb = kSr * 60.0 / bpm;
    // BUILD from beat 0, DROP pressed at `press`; input a 200 Hz sine at 0.3 until `inputUntil`.
    auto render = [&] (const std::function<void (Rig2&)>& setup, double press, double inputUntil, double beats)
    {
        resetBus();
        Rig2 r ("BUILD");
        r.head.bpm = bpm;
        setup (r);
        std::vector<float> o;
        int64_t t = 0;
        bool pressed = false;
        while (r.ppq < beats)
        {
            const double q0 = r.ppq;
            if (! pressed && q0 >= press) { r.set ("drop", 1.0f); pressed = true; }
            else r.set ("drop", 0.0f);
            r.step (kBlock, [&] (juce::AudioBuffer<float>& b, juce::MidiBuffer&, int n)
            {
                for (int i = 0; i < n; ++i)
                {
                    const float v = q0 + i * r.bps() < inputUntil ? 0.3f * (float) std::sin (6.283185307 * 200.0 * (double) (t + i) / kSr) : 0.0f;
                    b.setSample (0, i, v); b.setSample (1, i, v);
                }
            });
            for (int i = 0; i < kBlock; ++i) o.push_back (r.buf.getSample (0, i));
            t += kBlock;
        }
        return o;
    };
    auto db = [&] (const std::vector<float>& o, double b0, double b1)
    {
        double e = 0.0; int n = 0;
        for (size_t i = (size_t) (b0 * spb); i < (size_t) (b1 * spb) && i < o.size(); ++i) { e += (double) o[i] * o[i]; ++n; }
        return 10.0 * std::log10 (e / std::max (1, n) + 1e-30);
    };
    const double sineDb = 20.0 * std::log10 (0.3 / std::sqrt (2.0));

    // 0. One press, one drop. DROP held the way the E16 holds it (127 from press to release), from
    //    beat 1 to beat 7, GAP 1/4: one plan, one gap before beat 4 and none before beat 8 (the held
    //    button armed a second drop once the RETURN KNOB latch let go: BUILD at 0, or BUILD at 80 %
    //    turned back to 0 while still held). Two separate presses are two drops.
    {
        const char* what[] = { "held, BUILD at 0", "held, BUILD 80 % turned back to 0 at beat 4.25", "two presses, beats 1 and 5" };
        for (int variant = 0; variant < 3; ++variant)
        {
            resetBus();
            Rig2 r ("BUILD");
            r.head.bpm = bpm;
            r.set ("build", variant == 1 ? 0.8f : 0.0f); r.set ("gap", 3.0f); r.set ("tail", 0.0f);
            r.set ("noise", 0.0f); r.set ("space", 0.0f); r.set ("width", 0.0f); r.set ("drop_duck", 0.0f);
            std::vector<float> o;
            int64_t t = 0;
            int plans = 0;
            double lastPub = Bus::get().d().dropPpq.load();
            bool returned = false;
            const double blockBeats = kBlock * r.bps();
            while (r.ppq < 12.0)
            {
                const double q0 = r.ppq;
                const bool down = variant < 2 ? (q0 >= 1.0 && q0 < 7.0)
                                              : ((q0 >= 1.0 && q0 < 1.0 + blockBeats) || (q0 >= 5.0 && q0 < 5.0 + blockBeats));
                r.set ("drop", down ? 1.0f : 0.0f);
                if (variant == 1 && ! returned && q0 >= 4.25) { r.set ("build", 0.0f); returned = true; }
                r.step (kBlock, [&] (juce::AudioBuffer<float>& b, juce::MidiBuffer&, int n)
                {
                    for (int i = 0; i < n; ++i) { const float v = 0.3f * (float) std::sin (6.283185307 * 200.0 * (double) (t + i) / kSr); b.setSample (0, i, v); b.setSample (1, i, v); }
                });
                if (const double pub = Bus::get().d().dropPpq.load(); pub != lastPub) { ++plans; lastPub = pub; }
                for (int i = 0; i < kBlock; ++i) o.push_back (r.buf.getSample (0, i));
                t += kBlock;
            }
            const double gap1 = db (o, 3.05, 3.95), gap2 = db (o, 7.05, 7.95);
            std::printf ("   DROP %s, GAP 1/4: %d plan(s); before beat 4 %.1f dB, before beat 8 %.1f dB (input %.2f)\n", what[variant], plans, gap1, gap2, sineDb);
            if (variant < 2)
                CHECK (plans == 1 && gap1 < -120.0 && std::abs (gap2 - sineDb) < 0.3, "one held DROP (%s): %d plans, gaps %.1f / %.1f dB", what[variant], plans, gap1, gap2);
            else
                CHECK (plans == 2 && gap1 < -120.0 && gap2 < -120.0, "two DROP presses: %d plans, gaps %.1f / %.1f dB", plans, gap1, gap2);
        }
    }

    // 1. GAP 1/4: silent for the beat before the bar line, the mix back in full on it.
    {
        const auto o = render ([] (Rig2& r) { r.set ("build", 0.5f); r.set ("gap", 3.0f); r.set ("tail", 0.0f); }, 1.3, 99.0, 8.0);
        const double pre = db (o, 2.0, 2.95), gap = db (o, 3.02, 3.97), post = db (o, 4.05, 7.9);
        std::printf ("   GAP 1/4: before %.1f dB, in the gap %.1f dB, after the drop %.2f dB (input %.2f)\n", pre, gap, post, sineDb);
        CHECK (pre > -30.0 && gap < -120.0 && std::abs (post - sineDb) < 0.3, "GAP 1/4 (before %.1f, gap %.1f, after %.2f dB)", pre, gap, post);
        CHECK (std::fabs (o[(size_t) std::ceil (4.0 * spb) + 2]) > 1e-4f || std::fabs (o[(size_t) std::ceil (4.0 * spb) + 30]) > 1e-4f, "nothing on the drop's bar line");
    }
    // 2. DROP pressed inside the gap's beat: the gap starts on the next sixteenth; the drop stays on the bar.
    {
        const auto o = render ([] (Rig2& r) { r.set ("build", 0.5f); r.set ("gap", 3.0f); r.set ("tail", 0.0f); }, 3.6, 99.0, 6.0);
        const double before = db (o, 3.62, 3.74), gap = db (o, 3.77, 3.97), post = db (o, 4.05, 5.9);
        std::printf ("   GAP 1/4 pressed at beat 3.6: until 3.75 %.1f dB, 3.75-4 %.1f dB, after the drop %.2f dB\n", before, gap, post);
        CHECK (before > -30.0 && gap < -120.0 && std::abs (post - sineDb) < 0.3, "late GAP (%.1f / %.1f / %.2f dB)", before, gap, post);
    }
    // 3. TAIL 1/4 in a GAP 1/2: the noise and wash ring into the silence for a beat and stop; nothing
    //    of the build comes back on the drop (input stops at the gap).
    {
        const auto o = render ([] (Rig2& r) { r.set ("build", 1.0f); r.set ("noise", 1.0f); r.set ("space", 1.0f); r.set ("gap", 4.0f); r.set ("tail", 3.0f); }, 0.5, 2.0, 6.0);
        const double ring = db (o, 2.05, 2.5), after = db (o, 3.05, 3.97), drop = db (o, 4.0, 5.5);
        std::printf ("   TAIL 1/4 in GAP 1/2: first half beat of the gap %.1f dB, its second beat %.1f dB, after the drop %.1f dB\n", ring, after, drop);
        CHECK (ring > -60.0 && after < -120.0 && drop < -120.0, "TAIL into the gap (%.1f / %.1f / %.1f dB)", ring, after, drop);
    }
    // 4. TAIL 1 BAR past a GAP 1/16: the tail carries into the drop, DROP DUCK (6 dB) lower, and ends.
    {
        const auto o = render ([] (Rig2& r) { r.set ("build", 1.0f); r.set ("noise", 1.0f); r.set ("space", 0.5f); r.set ("gap", 1.0f); r.set ("tail", 5.0f); r.set ("drop_duck", 6.0f); },
                               0.5, 3.75, 9.0);
        const double before = db (o, 3.80, 3.97), after = db (o, 4.01, 4.20), late = db (o, 5.0, 6.0), gone = db (o, 7.8, 8.9);
        std::printf ("   TAIL 1 BAR past a GAP 1/16: tail before the bar %.1f dB, after %.1f dB (%.1f dB lower), a beat on %.1f dB, after its bar %.1f dB\n",
                     before, after, before - after, late, gone);
        CHECK (std::abs (before - after - 6.0) < 1.5 && late > -90.0 && gone < -120.0, "TAIL into the drop (%.1f / %.1f / %.1f / %.1f dB)", before, after, late, gone);
    }
    // 5. TAIL CUT without a gap: the riser stops within 5 ms of the bar line (no click: a fade).
    {
        const auto o = render ([] (Rig2& r) { r.set ("build", 1.0f); r.set ("noise", 1.0f); r.set ("tail", 0.0f); }, 0.5, 0.0, 6.0);
        const double riser = db (o, 3.5, 3.97), after = db (o, 4.0, 5.0);
        std::printf ("   TAIL CUT, no gap: riser %.1f dB, from 5 ms after the bar %.1f dB\n", riser, after);
        CHECK (riser > -40.0 && after < -120.0, "TAIL CUT (%.1f / %.1f dB)", riser, after);
    }
    // 5a. A kick that reaches the bus early keeps its attack [Nov 7]: the build lets go 10 ms before
    //     the bar line. GAP 1/4, BUILD 100 %: a 40 ms burst (80 Hz, 0.5) starting 8 ms before the bar
    //     comes out whole and unfiltered (it lost its first 8 ms to the gap's silence, and was
    //     high-passed with no gap); 12 ms before the bar the gap still holds.
    for (const int gap : { 3, 0 })
    {
        const double burstAt = 4.0 - 0.008 * bpm / 60.0, burstEnd = burstAt + 0.040 * bpm / 60.0;
        resetBus();
        Rig2 r ("BUILD");
        r.head.bpm = bpm;
        r.set ("build", 1.0f); r.set ("noise", 0.0f); r.set ("space", 0.0f); r.set ("width", 0.0f); r.set ("gap", (float) gap); r.set ("tail", 0.0f);
        std::vector<float> out, in;
        int64_t t = 0;
        bool pressed = false;
        while (r.ppq < 5.0)
        {
            const double q0 = r.ppq;
            if (! pressed && q0 >= 1.3) { r.set ("drop", 1.0f); pressed = true; } else r.set ("drop", 0.0f);
            r.step (kBlock, [&] (juce::AudioBuffer<float>& b, juce::MidiBuffer&, int n)
            {
                for (int i = 0; i < n; ++i)
                {
                    const double q = q0 + i * r.bps();
                    const float v = q >= burstAt && q < burstEnd ? 0.5f * (float) std::sin (6.283185307 * 80.0 * (double) (t + i) / kSr) : 0.0f;
                    b.setSample (0, i, v); b.setSample (1, i, v); in.push_back (v);
                }
            });
            for (int i = 0; i < kBlock; ++i) out.push_back (r.buf.getSample (0, i));
            t += kBlock;
        }
        auto e = [&] (const std::vector<float>& x, double b0, double b1)
        {
            double sum = 0.0; int n = 0;
            for (size_t i = (size_t) (b0 * spb); i < (size_t) (b1 * spb) && i < x.size(); ++i) { sum += (double) x[i] * x[i]; ++n; }
            return 10.0 * std::log10 (sum / std::max (1, n) + 1e-30);
        };
        float firstPeak = 0.0f;
        for (size_t i = (size_t) (burstAt * spb); i < (size_t) ((burstAt + 0.004 * bpm / 60.0) * spb); ++i) firstPeak = std::max (firstPeak, std::fabs (out[i]));
        const double whole = e (out, burstAt, burstEnd) - e (in, burstAt, burstEnd);
        std::printf ("   GAP %s, a burst 8 ms before the bar: its first 4 ms peak %.3f (input 0.5 sine), the whole burst %+.2f dB\n",
                     gap ? "1/4" : "OFF", firstPeak, whole);
        CHECK (firstPeak > 0.2f && std::abs (whole) < 0.5, "an early kick through the drop (GAP %s): first 4 ms peak %.3f, whole %+.2f dB", gap ? "1/4" : "OFF", firstPeak, whole);
    }
    // 5b. An Arrangement loop that wraps while a drop waits (GAP 1/4, DROP at 1.3): the drop lands
    //     on the wrap and the mix is back. In a loop with no bar line in it ([1, 4)) the bar the
    //     drop was planned on never comes: the gap used to stay on, the mix silent for good.
    for (const double loopStart : { 0.0, 1.0 })
    {
        resetBus();
        Rig2 r ("BUILD");
        r.head.bpm = bpm;
        r.set ("build", 0.5f); r.set ("gap", 3.0f); r.set ("tail", 0.0f);
        std::vector<float> o; std::vector<double> at;
        int64_t t = 0;
        bool pressed = false;
        int wraps = 0;
        while (wraps < 2 || r.ppq < loopStart + 1.0)
        {
            const double q0 = r.ppq;
            if (! pressed && q0 >= 1.3) { r.set ("drop", 1.0f); pressed = true; } else r.set ("drop", 0.0f);
            r.step (kBlock, [&] (juce::AudioBuffer<float>& b, juce::MidiBuffer&, int n)
            { for (int i = 0; i < n; ++i) { const float v = 0.3f * (float) std::sin (6.283185307 * 200.0 * (double) (t + i) / kSr); b.setSample (0, i, v); b.setSample (1, i, v); } });
            for (int i = 0; i < kBlock; ++i) { o.push_back (r.buf.getSample (0, i)); at.push_back (q0 + i * r.bps() + wraps * 100.0); }
            t += kBlock;
            if (r.ppq >= 4.0 - 1e-9) { r.ppq = loopStart; ++wraps; }
        }
        auto lvl = [&] (double b0, double b1)
        {
            double e = 0.0; int n = 0;
            for (size_t i = 0; i < o.size(); ++i) if (at[i] >= b0 && at[i] < b1) { e += (double) o[i] * o[i]; ++n; }
            return 10.0 * std::log10 (e / std::max (1, n) + 1e-30);
        };
        const double gap = lvl (3.02, 3.97), back = lvl (100.0 + loopStart + 0.05, 100.0 + 2.9), next = lvl (200.0 + loopStart + 0.05, 200.0 + loopStart + 0.9);
        std::printf ("   DROP waiting as a [%.0f, 4) loop wraps: gap %.1f dB, after the wrap %.2f dB, a loop later %.2f dB (input %.2f)\n", loopStart, gap, back, next, sineDb);
        CHECK (gap < -120.0 && std::abs (back - sineDb) < 0.3 && std::abs (next - sineDb) < 0.3, "drop across a loop wrap ([%.0f, 4): gap %.1f, after %.2f, later %.2f dB)", loopStart, gap, back, next);
    }
    // 5c. The drop duck starts without a step, also after a 1/16 roll into the bar (it started at
    //     the depth the roll's last kick still had: a click on pads and bass). 73 Hz tone, DROP
    //     DUCK 12 dB: the largest sample step around the bar line against the tone's own.
    {
        static double simSec2 = 9000.0;
        Bus::testClock = [] { return simSec2; };
        resetBus();
        Rig2 kick ("KICK"), duck ("DUCK"), build ("BUILD");
        for (auto* r : { &kick, &duck, &build }) r->head.bpm = bpm;
        duck.set ("depth", 0.0f);
        build.set ("drop_duck", 12.0f); build.set ("duck_time", 2.0f);
        const int lat = duck.p->getLatencySamples();
        float stepMax = 0.0f, prevY = 0.0f;
        int64_t t = 0;
        bool pressed = false, havePrev = false;
        while (kick.ppq < 8.6)
        {
            const double q0 = kick.ppq, bps = kick.bps();
            if (! pressed && q0 >= 5.0) { build.set ("drop", 1.0f); pressed = true; } else build.set ("drop", 0.0f);
            kick.step (kBlock, [&] (juce::AudioBuffer<float>&, juce::MidiBuffer& m, int n)
            {
                for (int i = 0; i < n; ++i)
                {
                    const double q = q0 + i * bps, grid = q >= 7.0 && q < 8.0 ? 0.25 : 1.0;
                    if (std::floor (q / grid) != std::floor ((q - bps) / grid)) m.addEvent (juce::MidiMessage::noteOn (1, 36, (juce::uint8) 127), i);
                }
            });
            build.step (kBlock, nullptr);
            duck.step (kBlock, [&] (juce::AudioBuffer<float>& b, juce::MidiBuffer&, int n)
            { for (int i = 0; i < n; ++i) { const float v = 0.3f * (float) std::sin (6.283185307 * 73.0 * (double) (t + i) / kSr); b.setSample (0, i, v); b.setSample (1, i, v); } });
            for (int i = 0; i < kBlock; ++i)
            {
                const float y = duck.buf.getSample (0, i);
                const double qOut = q0 + (i - lat) * bps;
                if (havePrev && qOut > 7.99 && qOut < 8.02) stepMax = std::max (stepMax, std::fabs (y - prevY));
                prevY = y; havePrev = true;
            }
            t += kBlock;
            simSec2 += kBlock / kSr;
        }
        const float natural = 0.3f * 6.2831853f * 73.0f / (float) kSr;
        std::printf ("   DROP DUCK 12 dB after a 1/16 roll into the bar: largest step at the bar %.5f (the tone's own %.5f)\n", stepMax, natural);
        CHECK (stepMax < 1.5f * natural, "the drop duck steps at the bar line (%.5f against %.5f)", stepMax, natural);
        Bus::testClock = nullptr;
    }
    // 6. DROP DUCK: an SF part with POCKET DEPTH 0 ducks 6 dB to each kick for the first half of
    //    DUCK TIME (1 bar) after the drop, less in the second, not at all before or after. A jump back
    //    into the window later (an Arrangement loop) doesn't duck again. Simulated clock.
    {
        static double simSec = 7000.0;
        Bus::testClock = [] { return simSec; };
        resetBus();
        Rig2 kick ("KICK"), duck ("DUCK"), build ("BUILD"), tapOther ("TAP"), tapKick ("TAP");
        for (auto* r : { &kick, &duck, &build, &tapOther, &tapKick }) r->head.bpm = bpm;
        duck.set ("depth", 0.0f);
        tapKick.set ("role", (float) ROLE_KICK);      // a kick layer on a TAP never ducks under SF KICK
        build.set ("drop_duck", 6.0f); build.set ("duck_time", 2.0f);
        const int lat = duck.p->getLatencySamples();
        std::vector<float> out, outOther, outKick, beatAt;     // DUCK, TAP OTHER, TAP KICK; the song position of each output sample
        bool pressed = false, jumped = false;
        int64_t t = 0;
        while (! jumped || kick.ppq < 12.0)
        {
            if (! jumped && kick.ppq >= 14.0) { for (auto* r : { &kick, &duck, &build, &tapOther, &tapKick }) r->ppq = 8.0; jumped = true; }
            const double q0 = kick.ppq, bps = kick.bps();
            if (! pressed && q0 >= 5.0) { build.set ("drop", 1.0f); pressed = true; } else build.set ("drop", 0.0f);
            kick.step (kBlock, [&] (juce::AudioBuffer<float>&, juce::MidiBuffer& m, int n)
            { for (int i = 0; i < n; ++i) if (std::floor (q0 + i * bps) != std::floor (q0 + (i - 1) * bps)) m.addEvent (juce::MidiMessage::noteOn (1, 36, (juce::uint8) 127), i); });
            build.step (kBlock, nullptr);
            auto tone = [&] (juce::AudioBuffer<float>& b, juce::MidiBuffer&, int n)
            { for (int i = 0; i < n; ++i) { const float v = 0.3f * (float) std::sin (6.283185307 * 1000.0 * (double) (t + i) / kSr); b.setSample (0, i, v); b.setSample (1, i, v); } };
            duck.step (kBlock, tone); tapOther.step (kBlock, tone); tapKick.step (kBlock, tone);
            for (int i = 0; i < kBlock; ++i)
            {
                out.push_back (duck.buf.getSample (0, i)); outOther.push_back (tapOther.buf.getSample (0, i)); outKick.push_back (tapKick.buf.getSample (0, i));
                beatAt.push_back ((float) (q0 + (i - lat) * bps) + (jumped ? 100.0f : 0.0f));
            }
            t += kBlock;
            simSec += kBlock / kSr;
        }
        // Lowest gain (2 ms RMS against the input's) in the first quarter beat after kick k (second pass: k + 100).
        auto lowest = [&] (double k, const std::vector<float>& o = {})
        {
            const auto& out2 = o.empty() ? out : o;
            float lo = 9.0f;
            const int w = (int) (0.002 * kSr);
            for (size_t i = (size_t) w; i < out.size(); i += (size_t) w)
                if (beatAt[i - (size_t) w] >= (float) k && beatAt[i] < (float) (k + 0.25))
                {
                    double e = 0.0;
                    for (size_t j = i - (size_t) w; j < i; ++j) e += (double) out2[j] * out2[j];
                    lo = std::min (lo, (float) (std::sqrt (e / w) / (0.3 / std::sqrt (2.0))));
                }
            return gainToDb (lo);
        };
        const float b7 = lowest (7.0), b8 = lowest (8.0), b9 = lowest (9.0), b11 = lowest (11.0), b12 = lowest (12.0), again = lowest (108.0);
        std::printf ("   DROP DUCK 6 dB, 1 bar, on a POCKET at DEPTH 0: kick 7 %.1f, kick 8 (the drop) %.1f, kick 9 %.1f, kick 11 %.1f, kick 12 %.1f dB; looped back to 8: %.1f dB\n",
                     b7, b8, b9, b11, b12, again);
        CHECK (b7 > -0.5f && b8 < -4.5f && b8 > -7.0f && b9 < -4.5f && b11 > b9 + 1.0f && b12 > -0.5f && again > -0.5f,
               "DROP DUCK (kick 7 %.1f, 8 %.1f, 9 %.1f, 11 %.1f, 12 %.1f, loop %.1f dB)", b7, b8, b9, b11, b12, again);
        const float other8 = lowest (8.0, outOther), kick8 = lowest (8.0, outKick);
        std::printf ("   the drop's first kick on a TAP at POCKET DEPTH 0: ROLE OTHER %.1f dB, ROLE KICK %.1f dB (never ducks)\n", other8, kick8);
        CHECK (other8 < -4.5f && other8 > -7.0f && kick8 > -0.5f, "DROP DUCK on TAP (OTHER %.1f, KICK %.1f dB)", other8, kick8);
        Bus::testClock = nullptr;
    }
    // 7. LAND ON KICK [Nov 7]: the gap and the drop follow the beat as BUILD hears it. On that day's
    //    recordings the kick reached BUILD 80 ms after BUILD's beat (Live's position ahead of the
    //    LIVE BUS's audio, the ONE after Live's beat): the gap ended, and the previous kick's tail,
    //    the bass and the rest came back, 80 ms before the kick, and the drop hit like any other beat.
    //    A groove arriving `late` ms after the beat (a kick, sweeping 160 > 50 Hz, on every beat; a
    //    55 Hz bass under it; a hat on the off-beats), BUILD from beat 8, DROP at 13.3, GAP 1/4: the
    //    gap ends 10 ms before the kick (Live's bar line on LAND ON BAR LINE), whatever `late` is;
    //    silent until then; the kick whole. Held through a build with no kick; Live's bar line when
    //    no beat was ever heard, or onsets that never keep to it (the input SFSnapshot plays: it
    //    read KICK +138 MS before beats had to agree).
    {
        const double bps = bpm / 60.0 / kSr;
        enum Case { HEARD, BAR_LINE, NO_KICK_IN_BUILD, NEVER_A_KICK, OFF_THE_BEAT };
        auto run = [&] (double lateMs, Case cs, double& openMs, double& gapDb, double& wholeDb)
        {
            resetBus();
            Rig2 r ("BUILD");
            r.head.bpm = bpm;
            r.set ("gap", 3.0f); r.set ("tail", 0.0f); r.set ("noise", 0.0f); r.set ("space", 0.0f); r.set ("width", 0.0f);
            r.set ("land", cs == BAR_LINE ? 1.0f : 0.0f);
            const double end = 18.0;
            const size_t N = (size_t) (end * spb) + 1;
            std::vector<float> in (N, 0.0f), out;
            uint32_t seed = 1;
            for (size_t i = 0; i < N; ++i)
            {
                double x = 0.1 * std::sin (6.283185307 * 55.0 * (double) i / kSr);
                if (cs == OFF_THE_BEAT)     // bursts every 5888 samples, 3.9 to a beat: never in the same place
                {
                    in[i] = (float) (0.3 * std::sin (0.01 * (double) i) * ((i / 256) % 23 < 4 ? 1.0 : 0.2));
                    continue;
                }
                for (int k = 0; k < 18; ++k)
                {
                    const bool kickHere = cs != NEVER_A_KICK && ! (cs == NO_KICK_IN_BUILD && k >= 9 && k < 16);
                    const double tk = ((double) i - (k * spb + lateMs * 0.001 * kSr)) / kSr;
                    if (kickHere && tk >= 0.0 && tk < 0.45)
                        x += 0.5 * std::exp (-tk / 0.15) * std::sin (6.283185307 * (50.0 * tk + 110.0 * 0.03 * (1.0 - std::exp (-tk / 0.03))));
                    const double th = ((double) i - ((k + 0.5) * spb + lateMs * 0.001 * kSr)) / kSr;
                    if (th >= 0.0 && th < 0.06)
                    {
                        seed = seed * 1664525u + 1013904223u;
                        x += 0.08 * std::exp (-th / 0.015) * ((double) (seed >> 8) / 8388608.0 - 1.0);
                    }
                }
                in[i] = (float) x;
            }
            size_t at = 0;
            bool pressed = false;
            while (r.ppq < end - 0.1)
            {
                const double q0 = r.ppq;
                r.set ("build", (float) juce::jlimit (0.0, 0.6, (q0 - 8.0) * 0.2));
                if (! pressed && q0 >= 13.3) { r.set ("drop", 1.0f); pressed = true; } else r.set ("drop", 0.0f);
                r.step (kBlock, [&] (juce::AudioBuffer<float>& b, juce::MidiBuffer&, int n)
                { for (int i = 0; i < n; ++i) { const float v = at + (size_t) i < N ? in[at + (size_t) i] : 0.0f; b.setSample (0, i, v); b.setSample (1, i, v); } });
                for (int i = 0; i < kBlock; ++i) out.push_back (r.buf.getSample (0, i));
                at += kBlock;
            }
            const double heard = cs == BAR_LINE || cs == NEVER_A_KICK || cs == OFF_THE_BEAT ? 0.0 : lateMs;
            const size_t gap0 = (size_t) ((15.0 * spb) + (heard + 6.0) * 0.001 * kSr);
            size_t open = gap0;
            while (open < out.size() && std::fabs (out[open]) < 1e-5f) ++open;
            const double kickAt = 16.0 * spb + lateMs * 0.001 * kSr;
            openMs = ((double) open - kickAt) / kSr * 1000.0;
            double e = 0.0; int n = 0;
            for (size_t i = gap0; i + 1 < open; ++i) { e += (double) out[i] * out[i]; ++n; }
            gapDb = n > 0 ? 10.0 * std::log10 (e / n + 1e-30) : -999.0;
            double eo = 0.0, ei = 0.0;
            for (size_t i = (size_t) kickAt; i < (size_t) (kickAt + 0.040 * kSr) && i < out.size(); ++i) { eo += (double) out[i] * out[i]; ei += (double) in[i] * in[i]; }
            wholeDb = 10.0 * std::log10 ((eo + 1e-30) / (ei + 1e-30));
        };
        for (const double late : { -20.0, 0.0, 40.0, 80.0, 120.0 })
        {
            double openMs, gapDb, wholeDb;
            run (late, HEARD, openMs, gapDb, wholeDb);
            std::printf ("   LAND ON KICK, the groove %+4.0f ms after the beat: the gap ends %+5.1f ms from the drop's kick, silent before (%.0f dB), the kick's first 40 ms %+.2f dB\n",
                         late, openMs, gapDb, wholeDb);
            CHECK (openMs > -12.5 && openMs < -7.5 && gapDb < -120.0 && std::abs (wholeDb) < 0.5,
                   "LAND ON KICK, %+.0f ms: opens %+.1f ms from the kick, gap %.0f dB, kick %+.2f dB", late, openMs, gapDb, wholeDb);
        }
        {
            double openMs, gapDb, wholeDb;
            run (80.0, BAR_LINE, openMs, gapDb, wholeDb);
            std::printf ("   LAND ON BAR LINE, the groove 80 ms late: the gap ends %+5.1f ms from the kick (Live's bar line less 10 ms)\n", openMs);
            CHECK (std::abs (openMs - (-90.0)) < 1.0, "LAND ON BAR LINE: opens %+.1f ms from an 80 ms late kick (want -90)", openMs);
            run (80.0, NO_KICK_IN_BUILD, openMs, gapDb, wholeDb);
            std::printf ("   no kick from beat 9 (the build) to the drop, 80 ms late before: the gap ends %+5.1f ms from the kick back on the drop, the kick %+.2f dB\n", openMs, wholeDb);
            CHECK (openMs > -12.5 && openMs < -7.5 && gapDb < -120.0 && std::abs (wholeDb) < 0.5, "no kick in the build: opens %+.1f ms, gap %.0f dB, kick %+.2f dB", openMs, gapDb, wholeDb);
            run (80.0, NEVER_A_KICK, openMs, gapDb, wholeDb);
            std::printf ("   never a kick (bass and hats only): the gap ends %+5.1f ms from the bar line\n", openMs + 80.0);
            CHECK (std::abs (openMs + 80.0 - (-10.0)) < 1.0, "never a kick: opens %+.1f ms from the bar line (want -10)", openMs + 80.0);
            run (80.0, OFF_THE_BEAT, openMs, gapDb, wholeDb);
            std::printf ("   bursts that don't keep to the beat (3.9 to a beat): the gap ends %+5.1f ms from the bar line\n", openMs + 80.0);
            CHECK (std::abs (openMs + 80.0 - (-10.0)) < 1.0, "bursts off the beat: opens %+.1f ms from the bar line (want -10)", openMs + 80.0);
        }
    }
    // 8. The same with SF KICK, as on Nov 7: its notes 40 ms after the beat (the ONE), its audio
    //    42 ms later again at BUILD (the LIVE BUS's position ahead of its audio), BUILD building from
    //    beat 0 (only the beats SF KICK reports a kick on count), DROP at 9.3, GAP 1/4: the gap ends
    //    10 ms before the kick on the drop's bar, 82 ms after Live's bar line, and the kick is whole.
    {
        static double simSec3 = 11000.0;
        Bus::testClock = [] { return simSec3; };
        resetBus();
        Rig2 kick ("KICK"), build ("BUILD");
        kick.head.bpm = build.head.bpm = bpm;
        build.set ("gap", 3.0f); build.set ("tail", 0.0f); build.set ("noise", 0.0f); build.set ("space", 0.0f); build.set ("build", 0.6f);
        const int pdc = (int) std::lround (0.042 * kSr);
        const double lateBeats = 0.040 * bpm / 60.0;
        std::vector<float> kickOut, out;
        bool pressed = false;
        while (kick.ppq < 13.0)
        {
            const double q0 = kick.ppq, bps = kick.bps();
            if (! pressed && q0 >= 9.3) { build.set ("drop", 1.0f); pressed = true; } else build.set ("drop", 0.0f);
            kick.step (kBlock, [&] (juce::AudioBuffer<float>&, juce::MidiBuffer& m, int n) { gridNotes (m, q0 - lateBeats, bps, n, 1.0, 0.25, 36, 127); });
            for (int i = 0; i < kBlock; ++i) kickOut.push_back (kick.buf.getSample (0, i));
            build.step (kBlock, [&] (juce::AudioBuffer<float>& b, juce::MidiBuffer&, int n)
            {
                const size_t base = kickOut.size() - (size_t) n;
                for (int i = 0; i < n; ++i)
                {
                    const long src = (long) (base + (size_t) i) - pdc;
                    const float v = src >= 0 ? kickOut[(size_t) src] : 0.0f;
                    b.setSample (0, i, v); b.setSample (1, i, v);
                }
            });
            for (int i = 0; i < kBlock; ++i) out.push_back (build.buf.getSample (0, i));
            simSec3 += kBlock / kSr;
        }
        const double kickAt = 12.0 * spb + 0.040 * kSr + pdc;     // the drop's kick at BUILD: its note, plus the way there
        size_t open = (size_t) (11.0 * spb + 0.090 * kSr);
        while (open < out.size() && std::fabs (out[open]) < 1e-5f) ++open;
        double eo = 0.0, ei = 0.0;
        for (size_t i = (size_t) kickAt; i < (size_t) (kickAt + 0.040 * kSr) && i < out.size(); ++i) { eo += (double) out[i] * out[i]; ei += (double) kickOut[i - (size_t) pdc] * kickOut[i - (size_t) pdc]; }
        const double openMs = ((double) open - kickAt) / kSr * 1000.0, whole = 10.0 * std::log10 ((eo + 1e-30) / (ei + 1e-30));
        const double fromBar = ((double) open - 12.0 * spb) / kSr * 1000.0;
        std::printf ("   SF KICK 40 ms after the beat, 42 ms more to BUILD, building throughout: the gap ends %+5.1f ms from the kick (%+.1f ms from Live's bar line), the kick's first 40 ms %+.2f dB\n",
                     openMs, fromBar, whole);
        CHECK (openMs > -12.5 && openMs < -7.5 && std::abs (whole) < 0.5, "SF KICK late: opens %+.1f ms from the kick, kick %+.2f dB", openMs, whole);
        Bus::testClock = nullptr;
    }
    resetBus();
}

// The bar line a song change lands on [Nov 7]. A Program Change from MIDI comes from a sequencer's
// downbeat, and one clocked by Live plays after Live's: the OXI One's came 40 ms after it, past the
// 30 ms SF CONTROL took as "on the bar", so every song change from the ONE landed a bar late. Now a
// Program Change within a sixteenth after a bar line lands on that bar; the SONG knob keeps 30 ms
// (a hand a little late on the one waits for the next bar). 120 BPM, bar lines every 4 beats.
static void testSongWindow()
{
    std::printf ("-- song changes: the bar line a request lands on\n");
    const double bpm = 120.0;
    struct Case { const char* what; bool pc; double afterMs, want; };
    const Case cases[] = {
        { "Program Change 40 ms after bar line 8 (the ONE, Nov 7)", true, 40.0, 8.0 },
        { "Program Change 110 ms after it", true, 110.0, 8.0 },
        { "Program Change 140 ms after it (past a sixteenth)", true, 140.0, 12.0 },
        { "Program Change 20 ms before it", true, -20.0, 8.0 },
        { "SONG turned 20 ms after it", false, 20.0, 8.0 },
        { "SONG turned 40 ms after it", false, 40.0, 12.0 },
    };
    for (const auto& k : cases)
    {
        resetBus();
        Rig2 ctl ("CONTROL");
        ctl.head.bpm = bpm;
        auto& d = Bus::get().d();
        const double at = 8.0 + k.afterMs * 0.001 * bpm / 60.0;
        auto quiet = [] (juce::AudioBuffer<float>&, juce::MidiBuffer&, int) {};
        // Up to the request, a block that starts on it (the knob: read at a block's start), on to beat 10.
        while (ctl.ppq + kBlock * ctl.bps() <= at) ctl.step (kBlock, quiet);
        const int lead = (int) std::floor ((at - ctl.ppq) / ctl.bps());
        if (! k.pc && lead > 0) ctl.step (lead, quiet);
        const uint32_t seq = d.songSeq.load();
        if (k.pc) ctl.step (kBlock, [&] (juce::AudioBuffer<float>&, juce::MidiBuffer& m, int) { m.addEvent (juce::MidiMessage::programChange (16, 2), lead); });
        else { ctl.set ("song", 2.0f); ctl.step (kBlock, quiet); }
        const bool published = d.songSeq.load() != seq && d.song.load() == 2;
        const double lands = d.songAtPpq.load();
        std::printf ("   %-58s lands on beat %4.1f (want %4.1f)\n", k.what, lands, k.want);
        CHECK (published && std::abs (lands - k.want) < 1e-6, "%s: published %d, lands on %.3f, want %.1f", k.what, (int) published, lands, k.want);
    }
    resetBus();
}

// The drop's weight. A groove of SF plugins at their defaults (KICK; RUMBLE on the kick bus; a rolling
// BASS; closed hats on the eighths, open hats on the off-beats, a clap on 2 and 4; a held PAD chord),
// summed as Live sums them (delay-compensated), through the LIVE BUS chain (BUILD > GLUE > TONE >
// LOWEND) and the Main's LIMIT. A 16-beat build (BUILD 0 to 100 %) and DROP on the bar (beat 32),
// against the same groove with no build: each beat's level around the drop, and how much of the
// kick's low end (under 150 Hz) and peak the drop's first bars keep after each stage. Simulated clock.
static void testDropWeight()
{
    std::printf ("-- the drop's weight: a groove > BUILD > GLUE > TONE > LOWEND > LIMIT\n");
    const double bpm = 120.0;
    static double clock = 40000.0;
    Bus::testClock = [] { return clock; };
    enum { STEADY, DROP, DUCK6, TAIL_CUT, GAP16, BY_HAND, KICK_FADER, HIS, HIS_NO_DUCK, VARIANTS };
    const char* names[] = { "no build", "DROP, defaults (DROP DUCK off)", "DROP, DROP DUCK 6 dB (the default before Nov 7)", "DROP, TAIL CUT",
                            "DROP, GAP 1/16", "no DROP: BUILD back to 0 by hand on the bar", "no build: the kick's fader down 2 bars, up on the bar",
                            "DROP, Nov 7 set: GAP 1 BAR, NOISE 0, SPACE 83 %, DUCK 6 dB", "DROP, Nov 7 set, DROP DUCK 0" };
    const char* taps[] = { "sources", "BUILD", "GLUE", "LOWEND", "LIMIT" };
    constexpr int T = 5;
    struct Run { std::vector<float> o[T]; std::vector<double> at[T]; };
    auto render = [&] (int v, double earlyMs = 0.0, bool busInTime = false, int landOn = 0)
    {
        resetBus();
        Rig2 kick ("KICK"), rumble ("RUMBLE"), bass ("BASS"), closed ("PERC"), open ("PERC"), clap ("PERC"), pad ("PAD");
        Rig2 build ("BUILD"), glue ("GLUE"), tone ("TONE"), low ("LOWEND"), lim ("LIMIT");
        Rig2* src[] = { &kick, &rumble, &bass, &closed, &open, &clap, &pad };
        constexpr int S = 7;
        for (auto* r : src) r->head.bpm = bpm;
        for (auto* r : { &build, &glue, &tone, &low, &lim }) r->head.bpm = bpm;
        open.set ("decay", 0.8f); clap.set ("model", 5.0f);
        if (v == DUCK6 || v == HIS) build.set ("drop_duck", 6.0f);
        if (v == TAIL_CUT) build.set ("tail", 0.0f);
        if (v == GAP16) build.set ("gap", 1.0f);
        if (v == HIS || v == HIS_NO_DUCK) { build.set ("gap", 5.0f); build.set ("noise", 0.0f); build.set ("space", 0.8255f); }
        build.set ("land", (float) landOn);
        int ls[S], lmax = 0;
        for (int k = 0; k < S; ++k) { ls[k] = src[k]->p->getLatencySamples(); lmax = std::max (lmax, ls[k]); }
        int lat[T] = { lmax, 0, 0, 0, 0 };
        lat[1] = lat[0] + build.p->getLatencySamples(); lat[2] = lat[1] + glue.p->getLatencySamples();
        lat[3] = lat[2] + tone.p->getLatencySamples() + low.p->getLatencySamples(); lat[4] = lat[3] + lim.p->getLatencySamples();
        const size_t L = (size_t) lmax + 1;
        std::vector<std::vector<float>> dl ((size_t) S, std::vector<float> (L * 2, 0.0f));   // per source, L and R interleaved
        size_t w = 0;
        juce::AudioBuffer<float> sum (2, kBlock);
        Run run;
        bool pressed = false;
        while (kick.ppq < 44.0)
        {
            const double q0 = kick.ppq, bps = kick.bps();
            float knob = 0.0f;
            if (v != STEADY && v != KICK_FADER) knob = (float) juce::jlimit (0.0, 1.0, (q0 - 16.0) / 16.0);
            if (v == BY_HAND && q0 >= 32.0) knob = 0.0f;
            build.set ("build", knob);
            if (v != STEADY && v != BY_HAND && v != KICK_FADER && ! pressed && q0 >= 30.0) { build.set ("drop", 1.0f); pressed = true; }
            else build.set ("drop", 0.0f);
            // Every part's notes `earlyMs` ahead of the grid (a ONE that plays early; negative: late); 0: on it.
            const double shift = earlyMs * 0.001 * bpm / 60.0;
            kick.step (kBlock, [&] (juce::AudioBuffer<float>&, juce::MidiBuffer& m, int n) { gridNotes (m, q0 + shift, bps, n, 1.0, 0.25, 36, 127); });
            rumble.step (kBlock, nullptr);
            auto sixteenths = [&] (juce::MidiBuffer& m, int n, int note, const std::function<int (int)>& velAt)   // velAt(sixteenth 0-15 in the bar): 0 = none
            {
                for (int i = 0; i < n; ++i)
                {
                    const double q = q0 + shift + i * bps, prev = q - bps;
                    if (q < 0.0) continue;
                    if (std::floor (q * 4.0 + 1e-9) == std::floor (prev * 4.0 + 1e-9) && ! (i == 0 && q0 + shift == 0.0)) continue;
                    const int pos = (int) std::floor (q * 4.0 + 1e-9) % 16;
                    m.addEvent (juce::MidiMessage::noteOff (1, note), i);
                    if (const int vel = velAt (pos)) m.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) vel), i);
                }
            };
            bass.step (kBlock, [&] (juce::AudioBuffer<float>&, juce::MidiBuffer& m, int n) { sixteenths (m, n, 36, [] (int p) { return p % 4 == 0 ? 0 : p % 4 == 1 ? 20 : p % 4 == 2 ? 35 : 55; }); });
            closed.step (kBlock, [&] (juce::AudioBuffer<float>&, juce::MidiBuffer& m, int n) { sixteenths (m, n, 60, [] (int p) { return p % 2 == 0 ? 100 : 0; }); });
            open.step (kBlock, [&] (juce::AudioBuffer<float>&, juce::MidiBuffer& m, int n) { sixteenths (m, n, 60, [] (int p) { return p % 4 == 2 ? 100 : 0; }); });
            clap.step (kBlock, [&] (juce::AudioBuffer<float>&, juce::MidiBuffer& m, int n) { sixteenths (m, n, 60, [] (int p) { return p == 4 || p == 12 ? 110 : 0; }); });
            pad.step (kBlock, [&] (juce::AudioBuffer<float>&, juce::MidiBuffer& m, int n)
            { if (q0 == 0.0 && n > 0) for (int note : { 60, 63, 67 }) m.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 90), 0); });
            // Live's delay compensation: each source delayed to the slowest one, then summed.
            for (int i = 0; i < kBlock; ++i)
            {
                const size_t at = (w + (size_t) i) % L;
                float sl = 0.0f, sr2 = 0.0f;
                for (int k = 0; k < S; ++k)
                {
                    const bool faderDown = k == 0 && v == KICK_FADER && q0 + i * bps >= 24.0 && q0 + i * bps < 32.0;
                    dl[(size_t) k][at * 2] = faderDown ? 0.0f : src[k]->buf.getSample (0, i);
                    dl[(size_t) k][at * 2 + 1] = faderDown ? 0.0f : src[k]->buf.getSample (std::min (1, src[k]->buf.getNumChannels() - 1), i);
                    const size_t rd = (at + L - (size_t) (lmax - ls[k])) % L;
                    sl += dl[(size_t) k][rd * 2]; sr2 += dl[(size_t) k][rd * 2 + 1];
                }
                sum.setSample (0, i, sl); sum.setSample (1, i, sr2);
            }
            w = (w + kBlock) % L;
            auto feed = [] (const juce::AudioBuffer<float>& from) { return [&from] (juce::AudioBuffer<float>& b, juce::MidiBuffer&, int n)
            { for (int ch = 0; ch < 2; ++ch) b.copyFrom (ch, 0, from, ch, 0, n); }; };
            // busInTime: the LIVE BUS's plugins get the song position of the audio reaching them (the
            // sources' delay compensation taken off), as a host that compensates the position does.
            if (busInTime) { build.ppq = q0 - lmax * bps; }
            build.step (kBlock, feed (sum)); glue.step (kBlock, feed (build.buf)); tone.step (kBlock, feed (glue.buf));
            low.step (kBlock, feed (tone.buf)); lim.step (kBlock, feed (low.buf));
            const juce::AudioBuffer<float>* outs[T] = { &sum, &build.buf, &glue.buf, &low.buf, &lim.buf };
            for (int t = 0; t < T; ++t)
                for (int i = 0; i < kBlock; ++i) { run.o[t].push_back (0.5f * (outs[t]->getSample (0, i) + outs[t]->getSample (1, i))); run.at[t].push_back (q0 + (i - lat[t]) * bps); }
            clock += kBlock / kSr;
        }
        return run;
    };
    // The low band: two 2-pole low-passes at 150 Hz (Q 0.707); energy over the half beat after a kick.
    auto lowDb = [&] (const Run& r, int t, double b0)
    {
        double s1 = 0, s2 = 0, s3 = 0, s4 = 0;
        const double f = 2.0 * std::sin (3.14159265 * 150.0 / kSr), q = 1.0 / 0.7071;
        double e = 0.0; int n = 0;
        for (size_t i = 0; i < r.o[t].size(); ++i)
        {
            double x = r.o[t][i];
            s2 += f * s1; const double h1 = x - s2 - q * s1; s1 += f * h1; x = s2;
            s4 += f * s3; const double h2 = x - s4 - q * s3; s3 += f * h2; x = s4;
            if (r.at[t][i] >= b0 && r.at[t][i] < b0 + 0.5) { e += x * x; ++n; }
        }
        return 10.0 * std::log10 (e / std::max (1, n) + 1e-30);
    };
    auto peakDb = [&] (const Run& r, int t, double b0)
    {
        float p = 0.0f;
        for (size_t i = 0; i < r.o[t].size(); ++i) if (r.at[t][i] >= b0 && r.at[t][i] < b0 + 0.08) p = std::max (p, std::fabs (r.o[t][i]));
        return gainToDb (p);
    };
    auto beatDb = [&] (const Run& r, int t, double b0)
    {
        double e = 0.0; int n = 0;
        for (size_t i = 0; i < r.o[t].size(); ++i) if (r.at[t][i] >= b0 && r.at[t][i] < b0 + 1.0) { e += (double) r.o[t][i] * r.o[t][i]; ++n; }
        return 10.0 * std::log10 (e / std::max (1, n) + 1e-30);
    };
    std::vector<Run> runs;
    for (int v = 0; v < VARIANTS; ++v) runs.push_back (render (v));
    // The kick's first 20 ms on the drop (GAP 1 BAR, DROP DUCK off), against the same kick with no build:
    // a kick that reaches the LIVE BUS ahead of the bar line, with the bus's position in time with
    // its audio and ahead of it by the sources' compensation.
    for (const bool inTime : { true, false })
        for (const double early : { 0.0, 2.0, 5.0, 10.0 })
        {
            const auto st = render (STEADY, early, inTime), dr = render (HIS_NO_DUCK, early, inTime);
            auto first = [&] (const Run& r, int t)
            {
                double e = 0.0; float pk = 0.0f;
                const double from = 32.0 - early * 0.001 * bpm / 60.0, to = from + 0.020 * bpm / 60.0;
                for (size_t i = 0; i < r.o[t].size(); ++i)
                    if (r.at[t][i] >= from - 0.002 && r.at[t][i] < to) { e += (double) r.o[t][i] * r.o[t][i]; pk = std::max (pk, std::fabs (r.o[t][i])); }
                return std::make_pair (10.0 * std::log10 (e + 1e-30), (double) gainToDb (pk));
            };
            const auto a = first (st, 1), b = first (dr, 1), c = first (st, 4), dd = first (dr, 4);
            std::printf ("   kick %4.1f ms early, bus position %s: the drop's first 20 ms after BUILD %+5.1f dB (peak %+5.1f), after LIMIT %+5.1f dB (peak %+5.1f)\n",
                         early, inTime ? "in time with its audio" : "ahead of its audio   ", b.first - a.first, b.second - a.second, dd.first - c.first, dd.second - c.second);
        }
    // The whole groove late (the ONE after Live's beat, Nov 7: 40 ms), the LIVE BUS's position ahead
    // of its audio (as in Live): what comes out of LIMIT in the 70 ms before the drop's kick, and its
    // first 20 ms, against the same moment with no build; LAND ON KICK against LAND ON BAR LINE.
    for (const double late : { 0.0, 40.0, 80.0 })
    {
        const auto st = render (STEADY, -late, false);
        for (const int landOn : { 0, 1 })
        {
            const auto dr = render (HIS_NO_DUCK, -late, false, landOn);
            const double kickAt = 32.0 + late * 0.001 * bpm / 60.0, ms = 0.001 * bpm / 60.0;
            auto win = [&] (const Run& r, double a, double b, float& pk)
            {
                double e = 0.0; int n = 0; pk = 0.0f;
                for (size_t i = 0; i < r.o[4].size(); ++i)
                    if (r.at[4][i] >= a && r.at[4][i] < b) { e += (double) r.o[4][i] * r.o[4][i]; ++n; pk = std::max (pk, std::fabs (r.o[4][i])); }
                return 10.0 * std::log10 (e / std::max (1, n) + 1e-30);
            };
            float p0, p1, p2, p3;
            const double preS = win (st, kickAt - 70 * ms, kickAt - 12 * ms, p0), preD = win (dr, kickAt - 70 * ms, kickAt - 12 * ms, p1);
            const double hitS = win (st, kickAt - 2 * ms, kickAt + 20 * ms, p2), hitD = win (dr, kickAt - 2 * ms, kickAt + 20 * ms, p3);
            // Where BUILD's gap ends, against the kick (BUILD's own output: no latency after it).
            size_t i = 0;
            while (i < dr.o[1].size() && dr.at[1][i] < kickAt - 200 * ms) ++i;
            while (i < dr.o[1].size() && std::fabs (dr.o[1][i]) < 1e-5f) ++i;
            const double openMs = i < dr.o[1].size() ? (dr.at[1][i] - kickAt) / ms : 999.0;
            std::printf ("   the groove %2.0f ms late, LAND ON %-8s: the gap ends %+6.1f ms from the drop's kick; 70-12 ms before it %6.1f dB (no build %6.1f), its first 20 ms %+5.1f dB, peak %+5.1f dB against no build\n",
                         late, landOn ? "BAR LINE" : "KICK", openMs, preD, preS, hitD - hitS, gainToDb (p3) - gainToDb (p2));
        }
    }
    std::printf ("   each beat's level (RMS) at the LIMIT's output, beats 28-35, the drop on 32:\n");
    for (int v = 0; v < VARIANTS; ++v)
    {
        std::printf ("     %-54s", names[v]);
        for (int b = 28; b < 36; ++b) std::printf (" %6.1f", beatDb (runs[(size_t) v], 4, (double) b));
        std::printf ("  | the drop against the beat before %+5.1f dB\n", beatDb (runs[(size_t) v], 4, 32.0) - beatDb (runs[(size_t) v], 4, 31.0));
    }
    auto avg = [&] (const std::function<double (const Run&, int, double)>& f, int v, int t, double from, double to)
    {
        double s = 0.0; int n = 0;
        for (double b = from; b < to - 1e-9; b += 1.0) { s += f (runs[(size_t) v], t, b) - f (runs[STEADY], t, b); ++n; }
        return s / n;
    };
    std::printf ("   against no build (dB): the drop's first kick, the rest of its bar, the next bar: level, low band, kick peak\n");
    for (int v = 1; v < VARIANTS; ++v)
    {
        std::printf ("     %s\n", names[v]);
        for (int t = 0; t < T; ++t)
            std::printf ("       after %-7s  %+5.1f %+5.1f %+5.1f | %+5.1f %+5.1f %+5.1f | %+5.1f %+5.1f %+5.1f\n", taps[t],
                         avg (beatDb, v, t, 32.0, 33.0), avg (lowDb, v, t, 32.0, 33.0), avg (peakDb, v, t, 32.0, 33.0),
                         avg (beatDb, v, t, 33.0, 36.0), avg (lowDb, v, t, 33.0, 36.0), avg (peakDb, v, t, 33.0, 36.0),
                         avg (beatDb, v, t, 36.0, 40.0), avg (lowDb, v, t, 36.0, 40.0), avg (peakDb, v, t, 36.0, 40.0));
    }
    Bus::testClock = nullptr;
    resetBus();
}

// SLOW ALL: SF CONTROL sets how long the next knob moves take in every plugin whose own SLOW is
// CONTROL. A move keeps the length it was made with: turning SLOW ALL (or SLOW) never touches a
// knob already travelling; the plugin's own SLOW wins; both go to 128 BAR; a length left on the bus
// by a CONTROL that is gone slows nothing; song recalls follow it. SLOW ALL changed in the same host block as a move
// leaves that move on the previous length, whatever order the host runs the tracks in (testControlAuthority);
// here the gestures are apart, as a hand makes them.
static void testSlowAll()
{
    std::printf ("-- SLOW ALL\n");
    resetBus();
    auto& d = Bus::get().d();
    Rig2 ctl ("CONTROL"), f ("FILTER");
    auto quiet = [] (juce::AudioBuffer<float>&, juce::MidiBuffer&, int) {};
    bool controlRuns = true;
    auto cycle = [&] { if (controlRuns) ctl.step (kBlock, quiet); else ctl.ppq += kBlock * ctl.bps(); f.step (kBlock, quiet); };
    const double barBlocks = 4.0 / (kBlock * f.bps());
    auto bars = [&] (double b) { for (int k = 0, n = (int) std::lround (b * barBlocks); k < n; ++k) cycle(); };
    const int fi = f.p->paramIndex ("filter");
    auto heard = [&] { return f.p->baseValue (fi); };
    auto expect = [&] (const char* what, float want, float tol)
    {
        const float h = heard();
        std::printf ("   %-62s %.3f (want %.3f)\n", what, h, want);
        CHECK (std::abs (h - want) <= tol, "%s: %.3f, want %.3f", what, h, want);
    };
    CHECK (f.p->apvts.getParameter ("slow")->getCurrentValueAsText() == "CONTROL", "a new plugin's SLOW should be CONTROL");

    f.set ("filter", 0.2f); bars (0.25);
    f.set ("filter", 0.8f); bars (0.15);
    expect ("SLOW ALL OFF: a move lands at once", 0.8f, 0.002f);

    ctl.set ("slow_all", 1.0f); bars (0.25);
    expect ("SLOW ALL to 1 BAR: nothing moves", 0.8f, 0.001f);
    f.set ("filter", 0.0f); bars (0.5);
    expect ("the next move (80 > 0 %) half a bar in", 0.4f, 0.01f);
    ctl.set ("slow_all", 0.0f); bars (0.25);
    expect ("SLOW ALL to OFF while it travels: three quarters in, no jump", 0.2f, 0.01f);
    ctl.set ("slow_all", 3.0f); bars (0.3);
    expect ("SLOW ALL to 4 BAR while it travels: still there after its bar", 0.0f, 0.001f);

    f.set ("filter", 1.0f); bars (1.0);
    expect ("the next move takes the new length (4 BAR): a quarter after a bar", 0.25f, 0.01f);
    ctl.set ("slow_all", 0.0f); bars (0.05); f.set ("filter", 0.6f); bars (0.15);
    expect ("moved again with SLOW ALL OFF: straight there from where it got to", 0.6f, 0.002f);

    f.set ("slow", 1.0f); f.set ("filter", 0.0f); bars (0.5);
    expect ("own SLOW 1 BAR over SLOW ALL OFF: half a bar in", 0.3f, 0.01f);
    f.set ("slow", 8.0f); bars (0.25);
    expect ("own SLOW to OFF while it travels: no jump", 0.15f, 0.01f);
    bars (0.3);
    expect ("... and it arrives on time", 0.0f, 0.001f);
    ctl.set ("slow_all", 3.0f); f.set ("filter", 1.0f); bars (0.15);
    expect ("own SLOW OFF over SLOW ALL 4 BAR: at once", 1.0f, 0.002f);

    // 128 BAR [Nov 5]: SLOW ALL's top (index 8) and SLOW's, after OFF (index 9: appended, so a saved
    // OFF is still OFF and a saved 64 BAR still 64). At 400 BPM, to keep it short: a quarter of the
    // way after 32 bars, there after 128; the plugin's own, half way after 64.
    {
        auto named = [] (Rig2& r, const char* id) { return r.p->apvts.getParameter (id)->getCurrentValueAsText(); };
        const juce::String off = named (f, "slow");
        f.set ("slow", 7.0f);
        const juce::String l64 = named (f, "slow");
        f.set ("slow", 8.0f);
        const double bpm0 = f.head.bpm;
        ctl.head.bpm = f.head.bpm = 400.0;
        const double fast = 4.0 / (kBlock * f.bps());
        auto longBars = [&] (double b) { for (int k = 0, n = (int) std::lround (b * fast); k < n; ++k) cycle(); };
        ctl.set ("slow_all", 8.0f); f.set ("slow", 0.0f); longBars (0.05);
        const juce::String all128 = named (ctl, "slow_all");
        f.set ("filter", 0.0f); longBars (32.0);
        expect ("SLOW ALL 128 BAR: a quarter of the way after 32 bars", 0.75f, 0.01f);
        longBars (96.1);
        expect ("... there after 128", 0.0f, 0.001f);
        ctl.set ("slow_all", 0.0f); f.set ("slow", 9.0f); longBars (0.05);
        const juce::String own128 = named (f, "slow");
        f.set ("filter", 1.0f); longBars (64.0);
        expect ("own SLOW 128 BAR over SLOW ALL OFF: half way after 64 bars", 0.5f, 0.01f);
        f.set ("slow", 8.0f); f.set ("filter", 1.0f); longBars (0.05);
        std::printf ("   SLOW 8 / 7 / 9: %s / %s / %s; SLOW ALL 8: %s\n", off.toRawUTF8(), l64.toRawUTF8(), own128.toRawUTF8(), all128.toRawUTF8());
        CHECK (off == "OFF" && l64 == "64 BAR" && own128 == "128 BAR" && all128 == "128 BAR", "SLOW's names: %s / %s / %s, SLOW ALL's %s",
               off.toRawUTF8(), l64.toRawUTF8(), own128.toRawUTF8(), all128.toRawUTF8());
        ctl.head.bpm = f.head.bpm = bpm0;
        ctl.set ("slow_all", 3.0f); bars (0.05);
    }

    f.set ("slow", 0.0f); bars (0.05);
    controlRuns = false; d.controlSeenMs = 0;            // CONTROL deleted: 4 BAR stays on the bus
    f.set ("filter", 0.0f); bars (0.15);
    expect ("SLOW ALL left on the bus by a CONTROL that is gone: at once", 0.0f, 0.002f);
    controlRuns = true;

    // Recalls with the transport stopped land at once (playing, they wait for the bar line).
    auto recall = [&] (int s) { d.song = s; d.songAtPpq = -1.0; d.songKey = -2; d.songSeq.fetch_add (1); };
    ctl.head.playing = f.head.playing = false;
    ctl.set ("slow_all", 0.0f);
    d.song = 1; f.set ("filter", 0.2f); bars (0.15); f.p->storeSong(); bars (0.05);       // song 2: 20 %
    d.song = 2; f.set ("filter", 0.9f); bars (0.15); f.p->storeSong(); bars (0.05);       // song 3: 90 %
    ctl.set ("slow_all", 1.0f); bars (0.05);
    recall (1); bars (0.5); f.p->flushHostNotifications();
    expect ("song 2 recalled from 90 % with SLOW ALL 1 BAR: half a bar in", 0.55f, 0.01f);
    bars (0.55); f.p->flushHostNotifications(); bars (0.05);
    expect ("... after its bar", 0.2f, 0.001f);
    ctl.set ("slow_all", 0.0f); bars (0.05);
    recall (2); bars (0.15); f.p->flushHostNotifications();
    expect ("song 3 recalled with SLOW ALL OFF: at once", 0.9f, 0.002f);
    resetBus();
}

// What SLOW moves (Nov 5 audit, F9). Every SF knob on the E16 scene (kE16Knobs, generated from
// layout.py): a continuous knob travels the whole bar at SLOW 1 BAR, a menu changes at once.
// TEXTURE COLOR over 4 bars changes the sound bar by bar. A MODEL change while a chord is held goes
// on in the new sound (the keys play again); a mono model keeps its stack.
static void testSlowScope()
{
    std::printf ("-- SLOW scope: every E16 knob; TEXTURE COLOR over 4 bars; MODEL with keys held\n");
    resetBus();
    std::map<std::string, std::unique_ptr<Rig2>> rigs;
    int glided = 0, menus = 0;
    juce::StringArray wrong, glideNames;
    for (const auto& k : kE16Knobs)
    {
        auto& rp = rigs[k.module];
        if (rp == nullptr) { rp = std::make_unique<Rig2> (k.module); rp->head.bpm = 120.0; }
        auto& r = *rp;
        auto* prm = r.p->param (k.param);
        const auto& s = r.p->allSpecs()[(size_t) k.param];
        const juce::String name = juce::String (k.module) + " " + s.name;
        auto blocks = [&] (double bars) { for (int b = 0, n = (int) std::lround (bars * 4.0 / (kBlock * r.bps())); b < n; ++b) r.step (kBlock, nullptr); };
        if (s.type == PType::Float)
        {
            r.set ("slow", 8.0f);                                             // OFF: the start lands at once
            prm->setValueNotifyingHost (0.25f); blocks (0.1);
            r.set ("slow", 1.0f);                                             // 1 BAR
            prm->setValueNotifyingHost (0.75f); blocks (0.5);
            const float a = prm->convertFrom0to1 (0.25f), b = prm->convertFrom0to1 (0.75f), h = r.p->baseValue (k.param);
            const float range = s.max - s.min;
            const bool half = std::abs (h - 0.5f * (a + b)) < 0.03f * range;
            blocks (0.6);
            const bool there = std::abs (r.p->baseValue (k.param) - b) < 1e-3f * range;
            if (half && there && s.glidesWithSlow()) { ++glided; if (! s.primary) glideNames.add (name); }
            else wrong.add (name + juce::String::formatted (" (half a bar: %.3f of %.3f..%.3f)", h, a, b));
        }
        else
        {
            r.set ("slow", 1.0f);
            const int now = (int) std::lround (r.p->baseValue (k.param));
            const int next = now + 1 > (int) s.max ? now - 1 : now + 1;
            prm->setValueNotifyingHost (prm->convertTo0to1 ((float) next)); r.step (kBlock, nullptr);
            if ((int) std::lround (r.p->baseValue (k.param)) == next) ++menus;
            else wrong.add (name + " (menu not at once)");
        }
        r.set ("slow", 0.0f);
    }
    std::printf ("   %d E16 knobs travel the bar at SLOW 1 BAR (detail knobs among them: %s); %d menus change at once\n",
                 glided, glideNames.joinIntoString (", ").toRawUTF8(), menus);
    CHECK (wrong.isEmpty(), "SLOW scope: %s", wrong.joinIntoString ("; ").toRawUTF8());
    rigs.clear();

    // TEXTURE COLOR 0 -> 100 % with its own SLOW at 4 BAR, a chord held, on WAVETABLE (COLOR is the
    // table's position): each bar's spectrum (third octaves, 100 Hz - 8 kHz) moves away from the 0 %
    // sound and towards the 100 % one, bar by bar.
    auto bandsOf = [] (const std::vector<float>& x, size_t a, size_t n, double rate)
    {
        constexpr int order = 14, N = 1 << order;
        juce::dsp::FFT fft (order);
        std::vector<double> e (19, 1e-20);
        for (size_t f0 = a; f0 + N <= a + n; f0 += N / 2)
        {
            std::vector<float> w (2 * (size_t) N, 0.0f);
            for (int i = 0; i < N; ++i) w[(size_t) i] = x[f0 + (size_t) i] * (float) (0.5 - 0.5 * std::cos (2.0 * 3.141592653589793 * i / (N - 1)));
            fft.performFrequencyOnlyForwardTransform (w.data());
            for (int i = 1; i <= N / 2; ++i)
            {
                const double f = i * rate / N, b = 3.0 * std::log2 (f / 100.0);
                if (b >= 0.0 && b < 19.0) e[(size_t) b] += (double) w[(size_t) i] * w[(size_t) i];
            }
        }
        double tot = 0.0;
        for (auto v : e) tot += v;
        for (auto& v : e) v = 10.0 * std::log10 (v / tot);                         // the shape, not the level
        return e;
    };
    auto dist = [] (const std::vector<double>& p, const std::vector<double>& q) { double d = 0.0; for (size_t i = 0; i < p.size(); ++i) d += (p[i] - q[i]) * (p[i] - q[i]); return std::sqrt (d / (double) p.size()); };
    auto pad = [&] (int slow, float colorFrom, float colorTo, double seconds, std::vector<float>& out)
    {
        resetBus();
        Rig2 r ("PAD");
        r.head.bpm = 120.0;
        r.set ("model", 2.0f); r.set ("color", colorFrom); r.set ("slow", 8.0f); r.set ("density", 0.0f);
        for (int b = 0; b < 8; ++b) r.step (kBlock, nullptr);
        r.set ("slow", (float) slow); r.set ("color", colorTo);
        bool on = false;
        out.clear();
        for (int64_t t = 0; t < (int64_t) (seconds * kSr); t += kBlock)
        {
            r.step (kBlock, [&] (juce::AudioBuffer<float>&, juce::MidiBuffer& m, int) { if (! on) { for (int nt : { 48, 51, 55 }) m.addEvent (juce::MidiMessage::noteOn (1, nt, (juce::uint8) 100), 0); on = true; } });
            for (int i = 0; i < kBlock; ++i) out.push_back (r.buf.getSample (0, i));
        }
    };
    {
        // At a quarter, half and three quarters of the way (2, 4, 6 s), the sound is the sound of COLOR
        // held at 25, 50, 75 % (the same time into the note: WAVETABLE's position drifts slowly as the
        // note goes on), nearer to it than to either end.
        std::vector<float> o, st;
        pad (3, 0.0f, 1.0f, 9.0, o);                                                  // SLOW 4 BAR: 8 s at 120 BPM
        std::vector<std::vector<float>> held;
        for (float c : { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f }) { pad (8, c, c, 7.0, st); held.push_back (st); }
        auto window = [&] (const std::vector<float>& x, int q) { return bandsOf (x, (size_t) ((2.0 * q - 0.25) * kSr), (size_t) (0.5 * kSr), kSr); };
        bool ok = dist (window (held[0], 2), window (held[4], 2)) > 3.0;
        std::printf ("   TEXTURE COLOR 0 -> 100 %% over 4 bars (WAVETABLE; the ends %.1f dB apart): against COLOR held there,", dist (window (held[0], 2), window (held[4], 2)));
        for (int q = 1; q <= 3; ++q)
        {
            const auto g = window (o, q);
            const double here = dist (g, window (held[(size_t) q], q)), from0 = dist (g, window (held[0], q)), from1 = dist (g, window (held[4], q));
            std::printf (" at %d %%: %.1f dB (0 %%: %.1f, 100 %%: %.1f);", 25 * q, here, from0, from1);
            ok = ok && here < 3.0 && here < from0 && here < from1;
        }
        std::printf ("\n");
        CHECK (ok, "TEXTURE COLOR does not evolve over its 4 bars");
    }

    // MODEL change with a chord held on TEXTURE: STRING MACHINE -> POLY PAD at 2 s; the chord goes on
    // (the keys play again in the new model) as if POLY PAD had played it all along, and ends when the
    // keys go up at 5 s.
    {
        auto chord = [&] (bool change)
        {
            resetBus();
            Rig2 r ("PAD");
            r.head.bpm = 120.0;
            r.set ("model", change ? 0.0f : 1.0f); r.set ("density", 0.0f);
            std::vector<float> o;
            for (int64_t t = 0; t < (int64_t) (8.0 * kSr); t += kBlock)
            {
                if (change && t >= (int64_t) (2.0 * kSr) && t < (int64_t) (2.0 * kSr) + kBlock) r.set ("model", 1.0f);
                r.step (kBlock, [&] (juce::AudioBuffer<float>&, juce::MidiBuffer& m, int)
                {
                    if (t == 0) for (int nt : { 48, 51, 55 }) m.addEvent (juce::MidiMessage::noteOn (1, nt, (juce::uint8) 100), 0);
                    if (t <= (int64_t) (5.0 * kSr) && t + kBlock > (int64_t) (5.0 * kSr)) for (int nt : { 48, 51, 55 }) m.addEvent (juce::MidiMessage::noteOff (1, nt), 0);
                });
                for (int i = 0; i < kBlock; ++i) o.push_back (r.buf.getSample (0, i));
            }
            return o;
        };
        const auto o = chord (true), ref = chord (false);
        auto rms = [&] (const std::vector<float>& x, double a, double b) { double e = 0.0; int n = 0; for (int i = (int) (a * kSr); i < (int) (b * kSr); ++i) { e += (double) x[(size_t) i] * x[(size_t) i]; ++n; } return gainToDb ((float) std::sqrt (e / std::max (1, n))); };
        const float before = rms (o, 1.0, 2.0), after = rms (o, 3.0, 4.5), afterRef = rms (ref, 3.0, 4.5), gone = rms (o, 7.5, 8.0), goneRef = rms (ref, 7.5, 8.0);
        std::printf ("   TEXTURE MODEL changed with a chord held: %.1f dB before, %.1f dB after (POLY PAD from the start: %.1f), %.1f dB 2.5 s after the keys went up (%.1f)\n",
                     before, after, afterRef, gone, goneRef);
        CHECK (std::abs (after - afterRef) < 3.0f && std::abs (gone - goneRef) < 3.0f, "TEXTURE MODEL change: %.1f dB after (want %.1f), %.1f once released (want %.1f)", after, afterRef, gone, goneRef);
    }
    // BASS (mono): keys 36 then 43 held, MODEL BASS LADDER -> MONO 101-TYPE: 43 plays on; 43 up: back to 36.
    {
        resetBus();
        Rig2 r ("BASS");
        r.head.bpm = 120.0;
        r.set ("model", 0.0f); r.set ("density", 0.0f); r.set ("gate", 0.6f);              // held as long as the key
        std::vector<float> o;
        for (int64_t t = 0; t < (int64_t) (4.0 * kSr); t += kBlock)
        {
            if (t >= (int64_t) (1.5 * kSr) && t < (int64_t) (1.5 * kSr) + kBlock) r.set ("model", 1.0f);
            r.step (kBlock, [&] (juce::AudioBuffer<float>&, juce::MidiBuffer& m, int)
            {
                if (t == 0) m.addEvent (juce::MidiMessage::noteOn (1, 36, (juce::uint8) 100), 0);
                if (t == (int64_t) kBlock * 100) m.addEvent (juce::MidiMessage::noteOn (1, 43, (juce::uint8) 100), 0);
                if (t <= (int64_t) (2.5 * kSr) && t + kBlock > (int64_t) (2.5 * kSr)) m.addEvent (juce::MidiMessage::noteOff (1, 43), 0);
            });
            for (int i = 0; i < kBlock; ++i) o.push_back (r.buf.getSample (0, i));
        }
        // Which note, over half a second:
        auto at = [&] (double t0, double hz)
        {
            std::complex<double> z {};
            const int n = (int) (0.5 * kSr);
            for (int i = 0; i < n; ++i)
                z += (double) o[(size_t) ((int) (t0 * kSr) + i)] * (0.5 - 0.5 * std::cos (2.0 * 3.141592653589793 * i / (n - 1))) * std::polar (1.0, -2.0 * 3.141592653589793 * hz * i / kSr);
            return 20.0 * std::log10 (std::abs (z) + 1e-12);
        };
        auto rms = [&] (double a) { double e = 0.0; for (int i = (int) (a * kSr); i < (int) ((a + 0.5) * kSr); ++i) e += (double) o[(size_t) i] * o[(size_t) i]; return gainToDb ((float) std::sqrt (e / (0.5 * kSr))); };
        // (Each note's own: 43 has its sub at 49 Hz, 36 its fundamental at 65.4 Hz; 36's sub, 32.7 Hz,
        // has a harmonic on 43's 98 Hz. The old model's voice releases on its own, under them.)
        const double held43 = at (1.8, 49.0) - at (1.8, 65.4), back36 = at (3.0, 65.4) - at (3.0, 49.0);
        // Both held, not a release fading: the level the same half a second on.
        const float after = rms (1.6), after2 = rms (2.0), back = rms (3.0), back2 = rms (3.5);
        std::printf ("   BASS (mono) MODEL changed with two keys held: 43 plays on (over 36 by %.1f dB; %.1f then %.1f dB); 43 up: 36 (over 43 by %.1f dB; %.1f then %.1f dB)\n",
                     held43, after, after2, back36, back, back2);
        CHECK (held43 > 12.0 && back36 > 12.0 && std::abs (after2 - after) < 3.0f && std::abs (back2 - back) < 3.0f && back2 > -40.0f,
               "BASS MODEL change: 43 by %.1f dB, then 36 by %.1f dB; %.1f / %.1f, %.1f / %.1f dB", held43, back36, after, after2, back, back2);
    }
    resetBus();
}

// GLIDE RINGS: a plugin finds its knobs on the E16 scene (by track name, or by module when the scene
// has it on one track), reports the travelling ones with Live's mapping of where the sound is, and
// SF CONTROL turns that into the scene script's SysEx: a start, moves of 32 or more, a keep-alive
// every 400 ms, an arrival; at most 14 knobs a message.
// The newest intent wins (Nov 5 audit, F2 F3 F7 F8): a return to the value from before a recall;
// STORE after a newer move; SLOW ALL changed in the same host block as a move; a host program
// request cancelled by a newer one. The ordinary cases stay as they were.
static void testControlAuthority()
{
    std::printf ("-- Control authority: returns after a recall, STORE after a move, SLOW ALL order, program requests\n");
    const int lvl = [] { Rig2 r ("FILTER"); return r.p->paramIndex ("level"); }();
    // FILTER's song 2 holds LEVEL -6 dB, Live's value is 0 dB; song 2 recalled, transport stopped.
    auto recallMinusSix = [] (Rig2& r)
    {
        auto& d = Bus::get().d();
        r.head.playing = false;
        d.song = 1;
        r.set ("level", -6.0f); r.step (kBlock, nullptr);
        r.p->storeSong(); r.step (kBlock, nullptr);
        r.set ("level", 0.0f); for (int k = 0; k < 120; ++k) r.step (kBlock, nullptr);
        d.songAtPpq = -1.0; d.songFromPpq = -1.0; d.songKey = -2;
        d.songSeq.fetch_add (1);
        r.step (kBlock, nullptr);
    };
    auto settle = [] (Rig2& r, int blocks) { for (int k = 0; k < blocks; ++k) r.step (kBlock, nullptr); };
    const int quarter = (int) std::ceil (0.25 * kSr / kBlock);

    // F2. Recall reported to Live, then a newer move before the next audio block: back to the
    //     value from before the recall (0 dB: a reset, a knob turned back) wins as surely as +3 does.
    for (const float manual : { 0.0f, 3.0f })
    {
        resetBus();
        Rig2 r ("FILTER");
        recallMinusSix (r);
        r.p->flushHostNotifications();
        const float reported = r.p->apvts.getRawParameterValue ("level")->load();
        r.set ("level", manual);
        settle (r, quarter);
        const float heard = r.p->currentValue (lvl);
        std::printf ("   recall -6 dB reported (Live %.1f), then %+.0f dB in Live: heard %.2f dB after 0.25 s\n", reported, manual, heard);
        CHECK (std::abs (reported + 6.0f) < 0.01f && std::abs (heard - manual) < 0.01f, "a move to %+.0f dB after a reported recall: heard %.2f dB", manual, heard);
    }
    // F3. STORE in the host block of a newer move: the move is stored, as it is heard (a local STORE
    //     and SF CONTROL's STORE TO); with no move the recall is stored; and the return to 0 dB.
    for (int variant = 0; variant < 4; ++variant)
    {
        resetBus();
        Rig2 r ("FILTER");
        recallMinusSix (r);
        float want = 3.0f;
        int target = 1;
        if (variant == 0) { r.set ("level", 3.0f); r.p->storeSong(); }
        else if (variant == 1) { r.set ("level", 3.0f); target = 2; auto& d = Bus::get().d(); d.storeSong = target; d.storeSeq.fetch_add (1); }
        else if (variant == 2) { want = -6.0f; r.p->storeSong(); }
        else { r.p->flushHostNotifications(); r.set ("level", 0.0f); want = 0.0f; r.p->storeSong(); }
        r.step (kBlock, nullptr);
        const float stored = r.p->mod().getSongValue (target, lvl);
        settle (r, 120);
        const float heard = r.p->currentValue (lvl);
        const char* what[] = { "+3 dB then STORE", "+3 dB then STORE TO song 3", "STORE, no move", "reported, 0 dB, STORE" };
        std::printf ("   recall -6 dB, %s in one block: stored %.2f dB, heard %.2f dB (want %+.0f)\n", what[variant], stored, heard, want);
        CHECK (std::abs (stored - want) < 0.01f && std::abs (heard - want) < 0.01f, "STORE after a recall (%s): stored %.2f, heard %.2f dB", what[variant], stored, heard);
    }
    // F7. SLOW ALL changed in the host block in which two FILTERs move (0.2 -> 0.8, 120 BPM):
    //     the same length for both, whether Live processes them before or after SF CONTROL, and
    //     with SF CONTROL first. A change made a block earlier gives both the new length (1 BAR).
    for (int variant = 0; variant < 4; ++variant)
    {
        resetBus();
        Rig2 a ("FILTER"), ctl ("CONTROL"), b ("FILTER");
        for (auto* r : { &a, &ctl, &b }) { r->head.playing = true; r->head.bpm = 120.0; }
        const bool ctlFirst = variant == 2, lengthFirst = variant == 1, toOff = variant == 3;
        a.set ("filter", 0.2f); b.set ("filter", 0.2f);
        ctl.set ("e16_rings", 0.0f);
        ctl.set ("slow_all", toOff ? 1.0f : 0.0f);
        auto cycle = [&]
        {
            if (ctlFirst) ctl.step (kBlock, nullptr);
            a.step (kBlock, nullptr);
            if (! ctlFirst) ctl.step (kBlock, nullptr);
            b.step (kBlock, nullptr);
        };
        for (int k = 0; k < 1200; ++k) cycle();               // long enough for a 1 BAR move to finish
        ctl.set ("slow_all", toOff ? 0.0f : 1.0f);
        if (lengthFirst) cycle();
        a.set ("filter", 0.8f); b.set ("filter", 0.8f);
        for (int k = 0; k < 94; ++k) cycle();                 // 0.5 s: a quarter of a 1 BAR move
        const float va = a.p->currentValue (a.p->paramIndex ("filter")), vb = b.p->currentValue (b.p->paramIndex ("filter"));
        const char* what[] = { "SLOW ALL OFF -> 1 BAR in the block of the move", "1 BAR a block before the move", "the same, SF CONTROL processed first", "1 BAR -> OFF in the block of the move" };
        const float want = variant == 1 ? 0.35f : variant == 3 ? 0.35f : 0.8f;
        std::printf ("   %s: after 0.5 s %.4f / %.4f (FILTER before / after SF CONTROL)\n", what[variant], va, vb);
        CHECK (std::abs (va - vb) < 0.005f && std::abs (va - want) < 0.01f, "SLOW ALL order (%s): %.4f / %.4f, want %.2f", what[variant], va, vb, want);
    }
    // F8. Host program requests before the next block: SONG 02 then SONG 01 from SONG 01 leaves
    //     SONG 01, with no recall; SONG 02 then SONG 03 lands SONG 03.
    {
        resetBus();
        Rig2 ctl ("CONTROL");
        ctl.set ("e16_rings", 0.0f);
        settle (ctl, 3);
        auto& d = Bus::get().d();
        const int start = d.song.load();
        const uint32_t seq0 = d.songSeq.load();
        ctl.p->setCurrentProgram (1); ctl.p->setCurrentProgram (0);
        ctl.step (kBlock, nullptr);
        const int back = d.song.load();
        const bool recalled = d.songSeq.load() != seq0;
        ctl.p->setCurrentProgram (1); ctl.p->setCurrentProgram (2);
        ctl.step (kBlock, nullptr);
        const int last = d.song.load();
        std::printf ("   program requests: from %d, 1 then 0 -> %d (%s), 1 then 2 -> %d\n", start, back, recalled ? "recalled" : "no recall", last);
        CHECK (start == 0 && back == 0 && ! recalled && last == 2, "program requests: %d, %d (%s), %d", start, back, recalled ? "recalled" : "no recall", last);
    }
}

// The newest intent, kept through its history (Nov 5 audit 2: A01, A05, A06).
//   A06: before the recall is reported to Live, a move away and back (+3 then 0 dB, no audio block
//        between) is still the performer's: 0 dB stays, in the sound, a STORE, STORE TO and the
//        set's saved state. An echo of the old value with no move away is not a move.
//   A01: the state Live saves holds the setting in force: a recall not yet reported to Live, a
//        newer move, a second recall; targets, not a glide's or MOTION's position; and the first
//        block after reopening plays it.
//   A05: a SLOW ALL change's one-block fence is spent once passed: a loop or relocate back to that
//        song position takes the new length (both ways), CONTROL running the next block or not.
static void testRecallHistory()
{
    std::printf ("-- Recall history: a move away and back, the saved state, SLOW ALL after a loop back\n");
    const int lvl = [] { Rig2 r ("FILTER"); return r.p->paramIndex ("level"); }();
    auto recallSong = [] (Rig2& r, int song)
    {
        auto& d = Bus::get().d();
        d.song = song; d.songAtPpq = -1.0; d.songFromPpq = -1.0; d.songKey = -2;
        d.songSeq.fetch_add (1);
        r.step (kBlock, nullptr);
    };
    // FILTER: song 2 holds LEVEL -6 dB (song 3: -12 dB); Live's value is 0 dB; transport stopped.
    auto setup = [] (Rig2& r)
    {
        auto& d = Bus::get().d();
        r.head.playing = false;
        d.song = 1; r.set ("level", -6.0f); r.step (kBlock, nullptr); r.p->storeSong(); r.step (kBlock, nullptr);
        d.song = 2; r.set ("level", -12.0f); r.step (kBlock, nullptr); r.p->storeSong(); r.step (kBlock, nullptr);
        r.set ("level", 0.0f); for (int k = 0; k < 120; ++k) r.step (kBlock, nullptr);
    };
    auto settle = [] (Rig2& r, int blocks) { for (int k = 0; k < blocks; ++k) r.step (kBlock, nullptr); };
    // reopened*: the setting the reopened plugin holds (after SLOW, before MACRO / MOTION).
    auto saved = [&] (Rig2& r, float& encoded, float& reopenedFirst, float& reopenedLater)
    {
        juce::MemoryBlock st;
        r.p->getStateInformation (st);
        const auto tree = juce::ValueTree::readFromData (st.getData(), st.getSize());
        encoded = NAN;
        for (auto row : tree.getChildWithName ("PARAMS"))
            if (row.getProperty ("id").toString() == "level") encoded = (float) row.getProperty ("value");
        Rig2 q ("FILTER");
        q.head.playing = false;
        q.p->setStateInformation (st.getData(), (int) st.getSize());
        q.step (kBlock, nullptr);
        reopenedFirst = q.p->baseValue (lvl);
        settle (q, 120);
        reopenedLater = q.p->baseValue (lvl);
    };

    // A06: a move away and back before the report.
    for (int variant = 0; variant < 6; ++variant)
    {
        resetBus();
        Rig2 r ("FILTER");
        setup (r);
        recallSong (r, 1);
        if (variant == 4) r.p->flushHostNotifications();          // control: reported first
        if (variant == 5) r.set ("level", 0.0f);                  // control: an echo of the old value, no move away
        else { r.set ("level", 3.0f); r.set ("level", 0.0f); }
        float stored = NAN, enc = NAN, first = NAN, later = NAN;
        if (variant == 1) { r.p->storeSong(); r.step (kBlock, nullptr); stored = r.p->mod().getSongValue (1, lvl); }
        if (variant == 2) { auto& d = Bus::get().d(); d.storeSong = 3; d.storeSeq.fetch_add (1); r.step (kBlock, nullptr); stored = r.p->mod().getSongValue (3, lvl); }
        if (variant == 3) saved (r, enc, first, later);
        if (variant != 4) r.p->flushHostNotifications();
        settle (r, 120);
        const float heard = r.p->currentValue (lvl), live = r.p->apvts.getRawParameterValue ("level")->load();
        const float want = variant == 5 ? -6.0f : 0.0f;
        const char* what[] = { "+3 then 0 dB, then the report", "... then STORE", "... then STORE TO song 4", "... then the set saved",
                               "reported first, then +3 then 0 dB", "Live sends 0 dB again (no move)" };
        std::printf ("   recall -6 dB from 0, %-36s heard %6.2f, Live %6.2f", what[variant], heard, live);
        if (variant == 1 || variant == 2) std::printf (", stored %.2f", stored);
        if (variant == 3) std::printf (", saved %.2f, reopened %.2f / %.2f", enc, first, later);
        std::printf (" dB (want %+.0f)\n", want);
        bool ok = std::abs (heard - want) < 0.01f && std::abs (live - want) < 0.01f;
        if (variant == 1 || variant == 2) ok = ok && std::abs (stored - want) < 0.01f;
        if (variant == 3) ok = ok && std::abs (enc - want) < 0.01f && std::abs (first - want) < 0.01f && std::abs (later - want) < 0.01f;
        CHECK (ok, "recall history (%s): heard %.2f, Live %.2f", what[variant], heard, live);
    }

    // A01: the saved state.
    for (int variant = 0; variant < 6; ++variant)
    {
        resetBus();
        Rig2 r ("FILTER");
        float want = -6.0f;
        if (variant == 4) { r.set ("slow", 1.0f); }                               // own SLOW 1 BAR (2 s at 120 BPM)
        if (variant == 5)
        {
            auto* tgt = dynamic_cast<juce::AudioParameterChoice*> (r.p->apvts.getParameter ("m1_target"));
            r.set ("m1_src", 1.0f); r.set ("m1_target", (float) tgt->choices.indexOf ("LEVEL")); r.set ("m1_depth", 0.5f); r.set ("m1_rate", 0.0f);
        }
        setup (r);
        recallSong (r, 1);
        if (variant == 1) r.p->flushHostNotifications();
        if (variant == 2) { r.set ("level", 3.0f); want = 3.0f; }
        if (variant == 3) { recallSong (r, 2); want = -12.0f; }
        settle (r, variant == 4 ? 94 : 2);                                         // SLOW: 0.5 s into the 2 s glide
        const float heardBefore = r.p->currentValue (lvl);
        float enc = NAN, first = NAN, later = NAN;
        saved (r, enc, first, later);
        const char* what[] = { "recall not yet reported", "recall reported (control)", "recall, then +3 dB (control)", "a second recall, neither reported",
                               "recall gliding (own SLOW 1 BAR)", "recall under MOTION on LEVEL" };
        std::printf ("   saved with %-36s heard %6.2f; saved %6.2f, reopened %6.2f / %6.2f dB (want %+.0f)\n", what[variant], heardBefore, enc, first, later, want);
        bool ok = std::abs (enc - want) < 0.01f && std::abs (first - want) < 0.01f && std::abs (later - want) < 0.01f;
        if (variant == 4 || variant == 5) ok = ok && std::abs (heardBefore - want) > 1.0f;   // saved mid-glide / while MOTION moved the sound
        CHECK (ok, "saved state (%s): saved %.2f, reopened %.2f / %.2f, want %.2f", what[variant], enc, first, later, want);
    }

    // A05: SLOW ALL changed at beat 2; much later the transport comes back to beat 2 (a loop, a
    // relocate) and two FILTERs, one each side of CONTROL, move 0.2 -> 0.8: the length in force now.
    for (int variant = 0; variant < 6; ++variant)
    {
        resetBus();
        Rig2 a ("FILTER"), ctl ("CONTROL"), b ("FILTER");
        auto& d = Bus::get().d();
        for (auto* r : { &a, &ctl, &b }) { r->head.playing = true; r->head.bpm = 120.0; }
        const bool toOff = variant % 2 == 1, aside = variant >= 2 && variant < 4, controlSkips = variant >= 4;
        a.set ("filter", 0.2f); b.set ("filter", 0.2f);
        ctl.set ("e16_rings", 0.0f);
        ctl.set ("slow_all", toOff ? 1.0f : 0.0f);
        bool ctlRuns = true;
        auto cycle = [&]
        {
            a.step (kBlock, nullptr);
            if (ctlRuns) ctl.step (kBlock, nullptr); else { ctl.ppq += kBlock * ctl.bps(); d.controlSeenMs = Bus::nowMs(); }
            b.step (kBlock, nullptr);
        };
        for (int k = 0; k < 1200; ++k) cycle();
        for (auto* r : { &a, &ctl, &b }) r->ppq = 2.0;
        ctl.set ("slow_all", toOff ? 0.0f : 1.0f);
        cycle();                                                   // the change, at beat 2
        if (controlSkips) ctlRuns = false;                         // CONTROL's next blocks don't run (still alive)
        for (int k = 0; k < 480; ++k) cycle();                     // 2.56 s on
        const double back = aside ? 2.25 : 2.0;
        for (auto* r : { &a, &ctl, &b }) r->ppq = back;
        a.set ("filter", 0.8f); b.set ("filter", 0.8f);
        for (int k = 0; k < 94; ++k) cycle();                      // 0.5 s: a quarter of a 1 BAR move
        const float va = a.p->currentValue (a.p->paramIndex ("filter")), vb = b.p->currentValue (b.p->paramIndex ("filter"));
        const float want = toOff ? 0.8f : 0.3504f;
        std::printf ("   SLOW ALL %s at beat 2, back to beat %.2f%s: after 0.5 s %.4f / %.4f (want %.4f)\n", toOff ? "1 BAR -> OFF" : "OFF -> 1 BAR",
                     back, controlSkips ? " (CONTROL not run since)" : aside ? " (control)" : "", va, vb, want);
        CHECK (std::abs (va - want) < 0.005f && std::abs (vb - want) < 0.005f, "SLOW ALL after a loop back (%s, beat %.2f): %.4f / %.4f, want %.4f",
               toOff ? "1 BAR -> OFF" : "OFF -> 1 BAR", back, va, vb, want);
    }
    resetBus();
}

// MODEL change and voice allocation (Nov 5 audit 2: A02, A04). SF LEAD at 48 kHz / 64.
//   A02: a chord's tones never take each other's voice (a tone started at the same instant hasn't
//        sounded yet, its level 0 the quietest of all): a full pool's MODEL change plays the whole
//        chord again (also with LAYER, DENSITY, changes in a row), and so does a new chord over a
//        full pool. The keys go up after the change: silence, no voice left on.
//   A04: the keys down are kept in the order they went down (not the voices' order in the pool):
//        poly to mono plays the newest key, mono -> poly -> mono keeps the stack, each key its velocity.
static void testModelChange()
{
    std::printf ("-- MODEL change with keys held: the whole chord, the newest key, the keys' own velocities\n");
    constexpr int blk = 64;
    using Fn = std::function<void (Rig2&, juce::MidiBuffer&, int)>;
    auto render = [&] (double secs, const std::function<void (Rig2&)>& setup, const Fn& fn)
    {
        resetBus();
        Rig2 r ("LEAD", kSr, blk);
        r.head.playing = true;
        for (const auto& pv : std::vector<std::pair<const char*, float>> { { "slow", 8.0f }, { "density", 0.0f }, { "layer", 0.0f }, { "space", 0.0f },
                                                                             { "pocket_depth", 0.0f }, { "alloc", 0.0f }, { "cutoff", 1.0f }, { "res", 0.0f },
                                                                             { "env_amt", 0.0f }, { "drive", 0.0f }, { "glide", 0.0f }, { "attack", 0.0f },
                                                                             { "gate", 0.5f }, { "accent", 0.0f } })
            r.set (pv.first, pv.second);
        setup (r);
        std::vector<float> y;
        const int nb = (int) std::lround (secs * kSr / blk);
        for (int k = 0; k < nb; ++k)
        {
            r.step (blk, [&] (juce::AudioBuffer<float>&, juce::MidiBuffer& m, int) { fn (r, m, k); });
            for (int i = 0; i < blk; ++i) y.push_back (r.buf.getSample (0, i));
        }
        return y;
    };
    auto blockAt = [] (double sec) { return (int) std::lround (sec * kSr / blk); };
    auto at = [] (const std::vector<float>& y, int note, double from, double len = 0.5)
    {
        return gainToDb ((float) hannAmp (y, (size_t) (from * kSr), (size_t) ((from + len) * kSr), 440.0 * std::exp2 ((note - 69) / 12.0), kSr));
    };
    auto rms = [] (const std::vector<float>& y, double from, double len = 0.5)
    {
        double e = 0.0; const size_t a = (size_t) (from * kSr), n = (size_t) (len * kSr);
        for (size_t i = a; i < a + n; ++i) e += (double) y[i] * y[i];
        return 10.0 * std::log10 (e / (double) n + 1e-30);
    };
    auto on = [] (juce::MidiBuffer& m, int note, int vel = 100) { m.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) vel), 0); };
    auto off = [] (juce::MidiBuffer& m, int note) { m.addEvent (juce::MidiMessage::noteOff (1, note), 0); };
    constexpr int kDetroit = 4, kOrgan = 5, kMono101 = 1, kStabDub = 3;

    // A02: four keys x MIN7 fill the 16 voices; STAB DETROIT -> ORGAN at 1 s; against ORGAN from the start.
    // Each key is measured at a tone of its own (no other tone, nor its octave, at that pitch: two
    // tones at one pitch add with random phases): 48, 53, 61 (58's third), 66 (63's third).
    const int keys[] = { 48, 53, 58, 63 }, own[] = { 48, 53, 61, 66 };
    struct Case { const char* what; std::function<void (Rig2&)> setup; int changes; };
    const std::vector<Case> cases {
        { "4 keys x MIN7 (16 voices)", [] (Rig2& r) { r.set ("chord", 2.0f); }, 1 },
        { "2 keys x MIN7 with LAYER (16 voices)", [] (Rig2& r) { r.set ("chord", 2.0f); r.set ("layer", 1.0f); r.set ("layer_model", (float) kStabDub); }, 1 },
        { "4 keys x MIN7, DENSITY 60 %", [] (Rig2& r) { r.set ("chord", 2.0f); r.set ("density", 0.6f); }, 1 },
        { "4 keys x MIN7, 10 changes in 0.5 s", [] (Rig2& r) { r.set ("chord", 2.0f); }, 10 } };
    for (const auto& cs : cases)
    {
        const int nk = std::string (cs.what).find ("2 keys") == 0 ? 2 : 4;
        auto play = [&] (bool change)
        {
            return render (5.5, [&] (Rig2& r) { cs.setup (r); r.set ("model", (float) (change ? kDetroit : kOrgan)); }, [&] (Rig2& r, juce::MidiBuffer& m, int k)
            {
                if (k == 0) for (int i = 0; i < nk; ++i) on (m, keys[i]);
                if (change)
                    for (int c = 0; c < cs.changes; ++c)
                        if (k == blockAt (1.0 + 0.05 * c)) r.set ("model", (float) ((cs.changes - 1 - c) % 2 == 0 ? kOrgan : kDetroit));
                if (k == blockAt (3.6)) for (int i = 0; i < nk; ++i) off (m, keys[i]);
            });
        };
        const auto y = play (true), ref = play (false);
        float worst = 0.0f, refLow = 0.0f;
        for (int i = 0; i < nk; ++i) { worst = std::min (worst, at (y, own[i], 3.0) - at (ref, own[i], 3.0)); refLow = std::min (refLow, at (ref, own[i], 3.0)); }
        const double dr = rms (y, 3.0) - rms (ref, 3.0), tail = rms (y, 5.0);
        std::printf ("   %-40s every key within %+.1f dB of ORGAN from the start; level %+.1f dB; keys up: %.0f dB\n", cs.what, worst, dr, tail);
        CHECK (refLow > -60.0f && worst > -6.0f && std::abs (dr) < 1.5 && tail < -90.0, "MODEL change, %s: keys %+.1f dB (reference down to %.1f), level %+.1f dB, after the keys %.0f dB", cs.what, worst, refLow, dr, tail);
    }

    // A02: a new chord over a full pool (4 keys x MIN7 held on ORGAN, then 71 x MIN7): its tones take
    //      held voices (the oldest key's, 48), each its own: its root (71) and seventh (81), tones
    //      of its own, sound as with 48 never played.
    {
        auto play = [&] (bool first)
        {
            return render (3.0, [] (Rig2& r) { r.set ("chord", 2.0f); r.set ("model", (float) kOrgan); }, [&] (Rig2&, juce::MidiBuffer& m, int k)
            {
                if (k == 0) for (int i = first ? 0 : 1; i < 4; ++i) on (m, keys[i]);
                if (k == blockAt (1.5)) on (m, 71);
            });
        };
        const auto y = play (true), ref = play (false);
        float worst = 0.0f;
        for (int t : { 71, 81 }) worst = std::min (worst, at (y, t, 2.2) - at (ref, t, 2.2));
        const float oldest = at (y, 48, 2.2), oldestBefore = at (y, 48, 1.0);
        std::printf ("   a fifth chord over 16 held voices: its root and seventh within %+.1f dB of the reference; the oldest key %.1f -> %.1f dB\n", worst, oldestBefore, oldest);
        CHECK (worst > -6.0f && oldest < oldestBefore - 30.0f, "a chord over a full pool: tones %+.1f dB, oldest key %.1f -> %.1f dB", worst, oldestBefore, oldest);
    }

    // A04: ORGAN, no chord: 48, then 55; 48 up (its voice free); 60, which takes 48's voice; MONO 101.
    for (const bool reuse : { false, true })
    {
        const auto y = render (3.5, [] (Rig2& r) { r.set ("chord", 1.0f); r.set ("model", (float) kOrgan); }, [&] (Rig2& r, juce::MidiBuffer& m, int k)
        {
            if (k == 0) on (m, reuse ? 48 : 55);
            if (reuse && k == blockAt (0.4)) on (m, 55);
            if (reuse && k == blockAt (0.8)) off (m, 48);
            if (k == blockAt (1.3)) on (m, 60);
            if (k == blockAt (1.8)) r.set ("model", (float) kMono101);
        });
        const float newest = at (y, 60, 2.5), older = at (y, 55, 2.5);
        std::printf ("   poly -> mono, %-30s the newest key %.1f dB, the older %.1f dB\n", reuse ? "the newest key in a freed voice:" : "keys in the pool's order:", newest, older);
        CHECK (newest > older + 30.0f, "poly -> mono (%s): newest %.1f, older %.1f dB", reuse ? "freed voice" : "in order", newest, older);
    }
    // A04: MONO 101 with 48, 55, 60 held; ORGAN; MONO 101 again: 60; 60 up: 55; 55 up: 48.
    {
        const auto y = render (4.0, [] (Rig2& r) { r.set ("chord", 1.0f); r.set ("model", (float) kMono101); }, [&] (Rig2& r, juce::MidiBuffer& m, int k)
        {
            if (k == 0) on (m, 48);
            if (k == blockAt (0.3)) on (m, 55);
            if (k == blockAt (0.6)) on (m, 60);
            if (k == blockAt (1.0)) r.set ("model", (float) kOrgan);
            if (k == blockAt (1.5)) r.set ("model", (float) kMono101);
            if (k == blockAt (2.2)) off (m, 60);
            if (k == blockAt (2.9)) off (m, 55);
        });
        auto lead = [&] (double from) { const float a = at (y, 48, from, 0.3), b = at (y, 55, from, 0.3), c = at (y, 60, from, 0.3);
                                        return a > b && a > c ? 48 : b > c ? 55 : 60; };
        const int s1 = lead (1.8), s2 = lead (2.5), s3 = lead (3.3);
        const float poly48 = at (y, 48, 1.15, 0.3), poly60 = at (y, 60, 1.15, 0.3);
        std::printf ("   mono -> poly -> mono: poly plays all three (48 %.1f, 60 %.1f dB); mono plays %d; 60 up: %d; 55 up: %d\n", poly48, poly60, s1, s2, s3);
        CHECK (s1 == 60 && s2 == 55 && s3 == 48 && poly48 > -60.0f && poly60 > -60.0f, "mono -> poly -> mono: %d, %d, %d", s1, s2, s3);
    }
    // A04: each key plays again at its own velocity (48 at 127, 54 at 30): DETROIT -> ORGAN.
    {
        auto play = [&] (bool change)
        {
            return render (3.0, [&] (Rig2& r) { r.set ("chord", 1.0f); r.set ("model", (float) (change ? kDetroit : kOrgan)); }, [&] (Rig2& r, juce::MidiBuffer& m, int k)
            {
                if (k == 0) { on (m, 48, 127); on (m, 54, 30); }
                if (change && k == blockAt (1.0)) r.set ("model", (float) kOrgan);
            });
        };
        const auto y = play (true), ref = play (false);
        const float d = at (y, 48, 2.0) - at (y, 54, 2.0), dRef = at (ref, 48, 2.0) - at (ref, 54, 2.0);
        std::printf ("   two keys at velocities 127 and 30 through a MODEL change: %+.1f dB apart (from the start: %+.1f)\n", d, dRef);
        CHECK (std::abs (d - dRef) < 1.5f, "velocities through a MODEL change: %+.1f vs %+.1f dB", d, dRef);
    }
    resetBus();
}

static void testGlideRings()
{
    std::printf ("-- GLIDE RINGS\n");
    resetBus();
    auto& bus = Bus::get();
    auto& d = bus.d();
    auto quiet = [] (juce::AudioBuffer<float>&, juce::MidiBuffer&, int) {};
    Rig2 ctl ("CONTROL"), kick ("KICK"), stab ("LEAD"), lead ("LEAD"), dub ("DELAY"), low ("ENERGY");
    auto name = [] (Rig2& r, const char* n) { juce::AudioProcessor::TrackProperties t; t.name = n; r.p->updateTrackProperties (t); };
    name (stab, "32 Stab");          // Live's name, any case
    name (dub, "A DUB");
    // Live reports a track's context in three calls: an empty name, the name, then the colour with
    // an empty name. ENERGY sits on three tracks in the scene: it needs the name to survive.
    name (low, ""); name (low, "10 LOW");
    { juce::AudioProcessor::TrackProperties t; t.colour = juce::Colours::white; low.p->updateTrackProperties (t); }
    std::vector<Rig2*> all { &ctl, &kick, &stab, &lead, &dub, &low };
    auto cycle = [&] { for (auto* r : all) r->step (kBlock, quiet); };
    const double barBlocks = 4.0 / (kBlock * kick.bps());
    auto bars = [&] (double b) { for (int k = 0, n = (int) std::lround (b * barBlocks); k < n; ++k) cycle(); };
    auto entries = [&] {
        std::map<int, int> m;      // channel * 128 + number -> ring
        for (auto& g : d.glides)
            if (g.owner.load() != 0)
                for (int k = 0; k < g.count.load(); ++k) { const uint32_t e = g.entry[k]; m[(int) (e >> 24) * 128 + (int) ((e >> 16) & 0xff)] = (int) (e & 0xffff); }
        return m;
    };
    bars (0.1);
    int claimed = 0;
    for (auto& g : d.glides) claimed += g.owner.load() != 0 ? 1 : 0;
    std::printf ("   slots claimed: %d (CONTROL, KICK by its module, LEAD on 32 STAB, DELAY on A DUB, ENERGY on 10 LOW after Live's\n"
                 "   three calls; not the unnamed LEAD)\n", claimed);
    CHECK (claimed == 5, "%d glide slots claimed, want 5", claimed);

    ctl.set ("slow_all", 1.0f); bars (0.05);
    kick.set ("tune", 12.0f); stab.set ("cutoff", 1.0f); lead.set ("cutoff", 1.0f); dub.set ("feedback", 0.70f);
    bars (0.5);
    auto e = entries();
    CHECK (e.empty() || ! e.count (14 * 128 + 20), "LOW ENERGY reported before it moved");
    low.set ("energy", 1.0f); bars (0.25);
    e = entries();
    std::printf ("   LOW ENERGY (ch 14 CC 20) travelling: reported %s\n", e.count (14 * 128 + 20) ? "yes" : "NO");
    CHECK (e.count (14 * 128 + 20) == 1, "LOW ENERGY not reported: its track name was lost");
    low.set ("energy", low.p->param (low.p->paramIndex ("energy"))->convertFrom0to1 (low.p->param (low.p->paramIndex ("energy"))->getValue()));
    bars (1.1);
    kick.set ("tune", 0.0f); stab.set ("cutoff", 0.4f); dub.set ("feedback", 0.45f); bars (1.1);
    kick.set ("tune", 12.0f); stab.set ("cutoff", 1.0f); dub.set ("feedback", 0.70f);
    bars (0.5);
    e = entries();
    const int kt = kick.p->paramIndex ("tune"), sc = stab.p->paramIndex ("cutoff"), fb = dub.p->paramIndex ("feedback");
    auto ringOf = [] (Rig2& r, int i, float top) { auto* p = r.p->param (i); return (int) std::lround (juce::jlimit (0.0f, 1.0f, p->convertTo0to1 (r.p->baseValue (i)) / top) * 16383.0f); };
    const float fbTop = dub.p->param (fb)->convertTo0to1 (0.70f);
    std::printf ("   half a bar in: KICK TUNE ch 1 CC 20 ring %d (want %d), STAB CUTOFF ch 9 CC 20 ring %d (want %d), DUB FEEDBACK ch 12 CC 21 ring %d (want %d), %d knobs reported\n",
                 e.count (1 * 128 + 20) ? e[1 * 128 + 20] : -1, ringOf (kick, kt, 1.0f), e.count (9 * 128 + 20) ? e[9 * 128 + 20] : -1, ringOf (stab, sc, 1.0f),
                 e.count (12 * 128 + 21) ? e[12 * 128 + 21] : -1, ringOf (dub, fb, fbTop), (int) e.size());
    CHECK (e.size() == 3, "%d knobs reported, want 3 (the unnamed LEAD has no knob on the scene)", (int) e.size());
    CHECK (e.count (1 * 128 + 20) && std::abs (e[1 * 128 + 20] - ringOf (kick, kt, 1.0f)) <= 1, "KICK TUNE ring wrong");
    CHECK (e.count (9 * 128 + 20) && std::abs (e[9 * 128 + 20] - ringOf (stab, sc, 1.0f)) <= 1, "STAB CUTOFF ring wrong");
    CHECK (e.count (12 * 128 + 21) && std::abs (e[12 * 128 + 21] - ringOf (dub, fb, fbTop)) <= 1, "DUB FEEDBACK ring wrong (mapped 0-70 %%)");

    E16Glide sender;
    auto msgs = sender.collect (bus, Bus::nowMs());
    bool framed = ! msgs.empty();
    int knobs = 0;
    for (auto& m : msgs)
    {
        framed = framed && m.size() >= 7 && m[0] == 0xF0 && m[1] == 0x7D && m[2] == 0x53 && m[3] == 0x46 && m.back() == 0xF7;
        for (size_t i = 5; i + 1 < m.size(); ++i) framed = framed && m[i] < 0x80;
        if (m[4] == 1) knobs += (int) (m.size() - 6) / 4;
    }
    std::printf ("   first collect: %d message(s), %d knobs, framed %s\n", (int) msgs.size(), knobs, framed ? "yes" : "NO");
    CHECK (framed && knobs == 3, "first collect: %d knobs, framed %d", knobs, (int) framed);
    const auto again = sender.collect (bus, Bus::nowMs());
    std::printf ("   at once again: %d message(s) (nothing moved 32)\n", (int) again.size());
    CHECK (again.empty(), "nothing moved, yet %d messages", (int) again.size());
    const auto keep = sender.collect (bus, Bus::nowMs() + 450);
    int kept = 0;
    for (auto& m : keep) if (m[4] == 1) kept += (int) (m.size() - 6) / 4;
    std::printf ("   450 ms later: %d knobs again (keep-alive)\n", kept);
    CHECK (kept == 3, "keep-alive: %d knobs, want 3", kept);

    bars (0.6);
    e = entries();
    const auto done = sender.collect (bus, Bus::nowMs());
    int arrived = 0;
    for (auto& m : done) if (m[4] == 2) arrived += (int) (m.size() - 6) / 2;
    std::printf ("   after the bar: %d knobs travelling, %d arrival(s) sent\n", (int) e.size(), arrived);
    CHECK (e.empty() && arrived == 3, "after the glide: %d travelling, %d arrivals", (int) e.size(), arrived);

    // Fifteen knobs at once: two messages.
    for (int i = 0; i < 6; ++i) ctl.set (("macro" + std::to_string (i + 1)).c_str(), 1.0f);
    auto flip = [] (Rig2& r, const char* id) { auto* p = r.p->param (r.p->paramIndex (id)); p->setValueNotifyingHost (p->getValue() > 0.5f ? 0.0f : 1.0f); };
    for (const char* id : { "tune", "decay", "punch", "click", "density", "filter", "drive", "level" }) flip (kick, id);
    flip (stab, "cutoff");
    bars (0.25);
    e = entries();
    const auto many = sender.collect (bus, Bus::nowMs());
    int moves = 0, biggest = 0;
    for (auto& m : many) if (m[4] == 1) { moves += (int) (m.size() - 6) / 4; biggest = std::max (biggest, (int) (m.size() - 6) / 4); }
    std::printf ("   %d knobs travelling: %d message(s), %d knobs, at most %d in one\n", (int) e.size(), (int) many.size(), moves, biggest);
    CHECK ((int) e.size() >= 15 && moves == (int) e.size() && biggest <= E16Glide::kPerMove && many.size() >= 2, "batching wrong");

    // A plugin removed: its slot goes, and its knobs are reported as arrived.
    all.erase (std::remove (all.begin(), all.end(), &kick), all.end());
    kick.p.reset();
    cycle();
    const auto gone = sender.collect (bus, Bus::nowMs());
    int left = 0;
    for (auto& m : gone) if (m[4] == 2) left += (int) (m.size() - 6) / 2;
    std::printf ("   KICK removed: %d arrival(s) sent for its knobs\n", left);
    CHECK (left == 8, "removed plugin: %d arrivals, want 8", left);
    resetBus();
}

// ------------------------------------------------------------------------------------------
// SF RADIO: CRUSH and RATE follow SIGNAL (clean at 100 %), the holes fade on an S-curve, SQUELCH is
// a squeal that never starts on a step and slides down, and the RATE counter can't run away.
static float goertzelDb (const std::vector<float>& x, size_t from, size_t n, double hz, double sr)
{
    const double w = 2.0 * 3.14159265358979 * hz / sr, cw = 2.0 * std::cos (w);
    double s1 = 0.0, s2 = 0.0;
    for (size_t i = from; i < from + n && i < x.size(); ++i) { const double s0 = x[i] + cw * s1 - s2; s2 = s1; s1 = s0; }
    const double p = s1 * s1 + s2 * s2 - cw * s1 * s2;
    return gainToDb ((float) (2.0 * std::sqrt (std::max (0.0, p)) / (double) n));
}

static void testRadio()
{
    std::printf ("-- RADIO\n");
    // A sine through RADIO; returns the left channel.
    auto run = [] (double sr, double seconds, double hz, const std::map<std::string, float>& set,
                   const std::function<void (Rig2&, double)>& during = nullptr)
    {
        resetBus();
        Rig2 r ("RADIO", sr);
        r.set ("dropout", 0.0f); r.set ("noise", 0.0f); r.set ("squelch", 0.0f); r.set ("ring", 0.0f);
        for (auto& [k, v] : set) r.set (k.c_str(), v);
        std::vector<float> o;
        int64_t t = 0;
        while (t < (int64_t) (seconds * sr))
        {
            if (during) during (r, (double) t / sr);
            r.step (r.block, [&] (juce::AudioBuffer<float>& bb, juce::MidiBuffer&, int n)
            {
                for (int i = 0; i < n; ++i)
                {
                    const float v = hz > 0.0 ? 0.4f * (float) std::sin (6.283185307 * hz * (double) (t + i) / sr) : 0.0f;
                    bb.setSample (0, i, v); bb.setSample (1, i, v);
                }
            });
            for (int i = 0; i < r.block; ++i) o.push_back (r.buf.getSample (0, i));
            t += r.block;
        }
        return o;
    };
    auto residual = [] (const std::vector<float>& a, const std::vector<float>& b)
    {
        double e = 0.0, s = 0.0;
        for (size_t i = 4800; i < std::min (a.size(), b.size()); ++i) { e += (double) (a[i] - b[i]) * (a[i] - b[i]); s += (double) b[i] * b[i]; }
        return 10.0 * std::log10 (std::max (e, 1e-30) / std::max (s, 1e-30));
    };
    // 1. CRUSH 4 bit / RATE 4 kHz: the full damage at SIGNAL 0, less at 50 %, none at 100 %.
    {
        double res[3];
        const float sigs[3] = { 0.0f, 0.5f, 1.0f };
        for (int k = 0; k < 3; ++k)
        {
            const auto crushed = run (kSr, 0.5, 1100.0, { { "signal", sigs[k] }, { "crush", 4.0f }, { "rate", 4000.0f } });
            const auto clean = run (kSr, 0.5, 1100.0, { { "signal", sigs[k] } });
            res[k] = residual (crushed, clean);
        }
        std::printf ("   CRUSH 4 bit, RATE 4 kHz against clean: SIGNAL 0 %.1f dB, 50 %% %.1f dB, 100 %% %.1f dB\n", res[0], res[1], res[2]);
        CHECK (res[0] > -15.0 && res[1] < res[0] - 3.0 && res[2] < -200.0, "CRUSH / RATE don't follow SIGNAL (%.1f, %.1f, %.1f dB)", res[0], res[1], res[2]);
    }
    // 2. Holes: the dropout fades add no kink. Largest second difference with dropouts against without.
    {
        auto d2 = [] (const std::vector<float>& x)
        {
            float m = 0.0f;
            for (size_t i = 4800; i < x.size(); ++i) m = std::max (m, std::fabs (x[i] - 2.0f * x[i - 1] + x[i - 2]));
            return m;
        };
        const auto holes = run (kSr, 8.0, 1100.0, { { "signal", 0.0f }, { "dropout", 1.0f } });
        const auto none = run (kSr, 8.0, 1100.0, { { "signal", 0.0f } });
        int dips = 0;
        for (size_t i = 4800, in = 0; i < holes.size(); i += 480)
        {
            float pk = 0.0f;
            for (size_t j = i; j < i + 480 && j < holes.size(); ++j) pk = std::max (pk, std::fabs (holes[j]));
            const bool low = pk < 0.01f;
            if (low && ! in) ++dips;
            in = low;
        }
        std::printf ("   DROPOUT 100 %% at SIGNAL 0: %d holes in 8 s; largest 2nd difference %.4f (%.4f without holes)\n", dips, d2 (holes), d2 (none));
        CHECK (dips >= 10, "RADIO made only %d holes", dips);
        CHECK (d2 (holes) < 1.1f * d2 (none), "RADIO holes kink the signal (%.4f against %.4f)", d2 (holes), d2 (none));
    }
    // 3. SQUELCH alone (no input): every squeal rises from silence without a step and slides down.
    {
        const auto o = run (kSr, 6.0, 0.0, { { "signal", 0.0f }, { "dropout", 1.0f }, { "squelch", 1.0f } });
        float pk = 0.0f, step = 0.0f;
        for (size_t i = 1; i < o.size(); ++i) { pk = std::max (pk, std::fabs (o[i])); step = std::max (step, std::fabs (o[i] - o[i - 1])); }
        // A sine at 4.2 kHz (the highest a squeal starts) of the loudest level moves at most this far per sample.
        const float sineStep = pk * 2.0f * 3.14159265f * 4200.0f / (float) kSr;
        size_t first = 0;
        while (first < o.size() && std::fabs (o[first]) < 1e-6f) ++first;
        auto pitch = [&] (size_t from, size_t n)
        {
            double best = -300.0, bestHz = 0.0;
            for (double hz = 200.0; hz < 5000.0; hz += 10.0) { const double v = goertzelDb (o, from, n, hz, kSr); if (v > best) { best = v; bestHz = hz; } }
            return bestHz;
        };
        const double early = pitch (first + 288, 1920), late = pitch (first + 4800, 1920);   // 6-46 ms, 100-140 ms
        int squeals = 0;
        for (size_t i = 1, quiet = 1; i < o.size(); ++i)
        {
            if (std::fabs (o[i]) > 1e-6f && quiet > 480) ++squeals;
            quiet = std::fabs (o[i]) > 1e-6f ? 0 : quiet + 1;
        }
        std::printf ("   SQUELCH 100 %%: %d squeals in 6 s, peak %.1f dB, largest step %.4f (a 4.2 kHz sine at that level: %.4f);\n"
                     "   the first slides %.0f Hz > %.0f Hz\n", squeals, gainToDb (pk), step, sineStep, early, late);
        CHECK (squeals >= 5, "RADIO SQUELCH: %d squeals", squeals);
        CHECK (step < sineStep, "RADIO squeal starts on a step (%.4f, a sine moves %.4f)", step, sineStep);
        CHECK (early > 1.3 * late && late >= 200.0 && early <= 4300.0, "RADIO squeal doesn't slide down (%.0f > %.0f Hz)", early, late);
        CHECK (pk > 0.05f && pk < 0.4f, "RADIO squeal level %.1f dB", gainToDb (pk));
    }
    // 4. RATE at 44.1 kHz: five minutes at 48 kHz (above the host rate) must not wind up the
    //    sample-hold counter (it grew 0.08 a sample and took over a second to drain), so RATE 2 kHz
    //    bites as soon as the knob gets there (its 1100 Hz image of a 900 Hz sine, 0.6 s later).
    {
        const double sr = 44100.0;
        const auto late = run (sr, 301.0, 900.0, { { "signal", 0.0f } }, [] (Rig2& r, double sec) { r.set ("rate", sec < 300.0 ? 48000.0f : 2000.0f); });
        const auto fresh = run (sr, 1.0, 900.0, { { "signal", 0.0f }, { "rate", 2000.0f } });
        const double a = goertzelDb (late, (size_t) (300.6 * sr), 2205, 1100.0, sr), b = goertzelDb (fresh, (size_t) (0.6 * sr), 2205, 1100.0, sr);
        std::printf ("   RATE 2 kHz at 44.1 kHz after five minutes at 48 kHz: image %.1f dB 0.6 s later (from the start: %.1f dB)\n", a, b);
        CHECK (std::abs (a - b) < 3.0, "RADIO RATE late after a long run above the host rate (%.1f against %.1f dB)", a, b);
    }
}

// Polyrhythm: MOTION's odd rates hold to the song position and meet the bar line where they should;
// DELAY's 5/16 and 7/16 put the first echo 1.25 and 1.75 beats after the sound.
static void testPolyrhythm()
{
    std::printf ("-- POLYRHYTHM\n");
    {
        const auto names = motionRateNames();
        CHECK ((int) names.size() == kMotionRates && names[10] == "64 BAR" && names[12] == "5/16" && names[20] == "7 BAR", "MOTION rate list changed its order");
    }
    // SQUARE on FILTER 50 %, depth 25 %: 75 % in the first half of each cycle, 25 % in the second.
    struct Case { int rate; const char* name; double cycleBeats; };
    const Case cases[] = { { 11, "3/16", 0.75 }, { 12, "5/16", 1.25 }, { 13, "7/16", 1.75 }, { 14, "1/4D", 1.5 }, { 15, "1/2D", 3.0 },
                           { 16, "1/4T", 2.0 / 3.0 }, { 17, "1/2T", 4.0 / 3.0 }, { 18, "3 BAR", 12.0 }, { 19, "5 BAR", 20.0 }, { 20, "7 BAR", 28.0 } };
    for (const auto& cs : cases)
    {
        resetBus();
        Rig2 r ("FILTER");
        const int fi = r.p->paramIndex ("filter");
        r.set ("filter", 0.5f); r.set ("m1_src", 3.0f); r.set ("m1_rate", (float) cs.rate); r.set ("m1_target", 1.0f); r.set ("m1_depth", 0.25f);
        const double span = std::max (cs.cycleBeats * 4.0, 20.0) + 1.0;
        const double blockBeats = r.bps() * kBlock;
        int checked = 0, wrong = 0;
        while (r.ppq < span)
        {
            r.step (kBlock, nullptr);
            const double ph = std::fmod (r.ppq, cs.cycleBeats) / cs.cycleBeats;
            const double edge = std::min ({ ph, std::abs (ph - 0.5), 1.0 - ph }) * cs.cycleBeats;
            if (edge < 4.0 * blockBeats) continue;             // the 3 ms slew sits on the edges
            const float want = ph < 0.5 ? 0.75f : 0.25f;
            ++checked;
            if (std::abs (r.p->currentValue (fi) - want) > 0.02f) ++wrong;
        }
        // Where it meets the bar line again: the least whole number of bars holding whole cycles.
        int meet = 1;
        while (std::abs (std::fmod (meet * 4.0, cs.cycleBeats)) > 1e-6 && std::abs (std::fmod (meet * 4.0, cs.cycleBeats) - cs.cycleBeats) > 1e-6) ++meet;
        std::printf ("   %-5s cycle %.3f beats, meets the bar line every %d bar%s: %d points, %d off\n", cs.name, cs.cycleBeats, meet, meet == 1 ? "" : "s", checked, wrong);
        CHECK (checked > 50 && wrong == 0, "MOTION %s off the song position (%d of %d)", cs.name, wrong, checked);
    }
    for (const char* t : { "5/16", "7/16" })
    {
        resetBus();
        Rig2 r ("DELAY");
        r.head.bpm = 120.0;
        const auto& sp = r.p->allSpecs();
        const auto& ch = sp[(size_t) r.p->paramIndex ("time")].choices;
        const int idx = (int) (std::find (ch.begin(), ch.end(), std::string (t)) - ch.begin());
        r.set ("time", (float) idx); r.set ("mix", 1.0f); r.set ("dry", 1.0f); r.set ("feedback", 0.3f); r.set ("wow", 0.0f); r.set ("duck", 0.0f);
        std::vector<float> o;
        int64_t n0 = 0;
        for (int b = 0; b < (int) (2.0 * kSr) / kBlock; ++b)
        {
            r.step (kBlock, [&] (juce::AudioBuffer<float>& bb, juce::MidiBuffer&, int n)
            {
                for (int i = 0; i < n; ++i) { const float v = (n0 + i) == 2400 ? 0.8f : 0.0f; bb.setSample (0, i, v); bb.setSample (1, i, v); }
                n0 += n;
            });
            for (int i = 0; i < kBlock; ++i) o.push_back (0.5f * (r.buf.getSample (0, i) + r.buf.getSample (1, i)));
        }
        // The dry click, then the loudest point after it (the first echo).
        size_t dry = 0;
        for (size_t i = 0; i < o.size(); ++i) if (std::fabs (o[i]) > std::fabs (o[dry])) dry = i;
        size_t echo = dry + 2000;
        for (size_t i = dry + 2000; i < o.size(); ++i) if (std::fabs (o[i]) > std::fabs (o[echo])) echo = i;
        const double beats = (double) (echo - dry) / (kSr * 60.0 / 120.0);
        const double want = std::string (t) == "5/16" ? 1.25 : 1.75;
        std::printf ("   DELAY TIME %s (menu item %d): first echo %.4f beats after the sound\n", t, idx + 1, beats);
        CHECK (idx < (int) ch.size() && std::abs (beats - want) < 0.002, "DELAY %s echo at %.4f beats", t, beats);
    }
}

// ------------------------------------------------------------------------------------------
// SF TERROIR's convolution engine against direct convolution: every tap where it belongs, 512
// samples late, at any chunk size; a new IR takes over with the whole input history, through a
// crossfade that is always a mix of the two (never a jump).
static void testConvolverEngine()
{
    std::printf ("-- TERROIR engine\n");
    std::mt19937 rng (7);
    std::normal_distribution<float> gauss (0.0f, 1.0f);
    auto makeIr = [&] (int taps, float rt)
    {
        std::vector<float> l ((size_t) taps), r ((size_t) taps);
        for (int i = 0; i < taps; ++i) { const float e = std::exp (-6.9f * (float) i / (rt * (float) kSr)); l[(size_t) i] = gauss (rng) * e * 0.05f; r[(size_t) i] = gauss (rng) * e * 0.05f; }
        l[0] = 0.8f; r[3] = -0.6f;
        return std::make_pair (l, r);
    };
    auto direct = [] (const std::vector<float>& h, const std::vector<float>& x, int t)
    {
        double acc = 0.0;
        for (size_t k = 0; k < h.size(); ++k) { const long j = (long) t - Convolver::kLatency - (long) k; if (j < 0) break; acc += (double) h[k] * x[(size_t) j]; }
        return acc;
    };
    // 1. Exactness: a 1.3 s IR (head and 15 tail partitions), chunks of odd sizes.
    {
        const int taps = (int) (1.3 * kSr), n = 70000;
        auto ir = makeIr (taps, 1.0f);
        std::vector<float> x ((size_t) n);
        for (auto& v : x) v = gauss (rng) * 0.3f;
        auto f = makeIrFilter (ir.first.data(), ir.second.data(), taps);
        Convolver cv;
        cv.prepare (taps);
        cv.offer (f.get());
        std::vector<float> yl ((size_t) n), yr ((size_t) n);
        const int sizes[] = { 1, 63, 64, 100, 7, 512, 333 };
        for (int pos = 0, k = 0; pos < n; ++k) { const int m = std::min (sizes[k % 7], n - pos); cv.process (x.data() + pos, yl.data() + pos, yr.data() + pos, m); pos += m; }
        double err = 0.0, peak = 0.0;
        for (int t : { 600, 4000, 4700, 9000, 30001, 62000, 69999 })
            for (int d = 0; d < 300 && t + d < n; ++d)
            {
                const double el = direct (ir.first, x, t + d), er = direct (ir.second, x, t + d);
                err = std::max ({ err, std::abs (el - yl[(size_t) (t + d)]), std::abs (er - yr[(size_t) (t + d)]) });
                peak = std::max ({ peak, std::abs (el), std::abs (er) });
            }
        std::printf ("   1.3 s IR, chunks 1-512: largest error %.2e of a %.2f peak (%.0f dB)\n", err, peak, 20.0 * std::log10 (err / peak + 1e-30));
        CHECK (err < 2e-5 * peak + 1e-6, "TERROIR engine differs from direct convolution (%.2e)", err);
    }
    // 2. Swap: IR a, then b offered mid-stream. Before: a. After: b, with the input history. In
    //    between: a mix (1-g) a + g b with g rising from 0 to 1.
    {
        const int taps = 14000, n = 40000, at = 15000;
        auto a = makeIr (taps, 0.2f), b = makeIr (taps, 0.15f);
        std::vector<float> x ((size_t) n);
        for (auto& v : x) v = gauss (rng) * 0.3f;
        auto fa = makeIrFilter (a.first.data(), a.second.data(), taps), fb = makeIrFilter (b.first.data(), b.second.data(), taps);
        Convolver cv;
        cv.prepare (taps);
        cv.offer (fa.get());
        std::vector<float> yl ((size_t) n), yr ((size_t) n);
        bool offered = false, retiredA = false;
        for (int pos = 0; pos < n; pos += 64)
        {
            if (pos >= at && ! offered) offered = cv.offer (fb.get());
            cv.process (x.data() + pos, yl.data() + pos, yr.data() + pos, 64);
            if (auto* r = cv.takeRetired()) retiredA = r == fa.get();
        }
        std::vector<double> ya ((size_t) n), yb ((size_t) n);
        for (int t = 10000; t < n; ++t) { ya[(size_t) t] = direct (a.first, x, t); yb[(size_t) t] = direct (b.first, x, t); }
        bool before = true;
        for (int t = 10000; t < at; ++t) before = before && std::abs (yl[(size_t) t] - ya[(size_t) t]) < 1e-4;
        int firstB = at;                                     // from here on: b alone
        for (int t = at; t < n; ++t) if (std::abs (yl[(size_t) t] - yb[(size_t) t]) > 1e-4) firstB = t + 1;
        float gPrev = -1.0f;
        bool mix = true, rising = true;
        for (int t = at; t < firstB; ++t)
        {
            const double y = yl[(size_t) t], da = ya[(size_t) t], db = yb[(size_t) t];
            if (std::abs (db - da) < 0.05) continue;
            const float g = (float) ((y - da) / (db - da));
            mix = mix && g > -0.002f && g < 1.002f;
            if (gPrev >= 0.0f && g < gPrev - 0.002f) rising = false;
            gPrev = g;
        }
        const bool after = firstB < n - 1000;
        std::printf ("   swap offered at %d: IR b alone from %d (%.0f ms later), a mix of a and b in between: %s, rising: %s; old IR returned: %s\n",
                     at, firstB, (firstB - at) * 1000.0 / kSr, mix ? "yes" : "NO", rising ? "yes" : "NO", retiredA ? "yes" : "NO");
        CHECK (before && after && mix && rising && firstB > at && firstB - at < 4 * IrFilter::kB2 && retiredA, "TERROIR engine swap is not a clean crossfade");
    }
}

// SF TERROIR: captures (a sweep played through a room with a drifting clock, a clap with a second
// clap after it), what the knobs do to the response, FREEZE, a place dropped on the plugin and kept
// in the set, offline renders that repeat exactly, and the level against SF REVERB.
static std::vector<float> roomIr (double sr, float rt, uint32_t seed)
{
    std::mt19937 rng (seed);
    std::normal_distribution<float> g (0.0f, 1.0f);
    std::vector<float> h ((size_t) (2.0 * sr), 0.0f);
    h[(size_t) (0.003 * sr)] = 1.0f;
    const float refl[][2] = { { 0.011f, 0.5f }, { 0.017f, -0.4f }, { 0.026f, 0.3f }, { 0.041f, -0.25f } };
    for (auto& r : refl) h[(size_t) (r[0] * sr)] += r[1];
    for (size_t i = (size_t) (0.015 * sr); i < h.size(); ++i) h[i] += g (rng) * 0.05f * std::exp (-6.9078f * (float) (i / sr) / rt);
    return h;
}
static std::vector<float> convolveWith (const std::vector<float>& x, const std::vector<float>& h)
{
    auto f = makeIrFilter (h.data(), h.data(), (int) h.size());
    Convolver cv;
    cv.prepare ((int) h.size());
    cv.offer (f.get());
    std::vector<float> y (x.size() + h.size() + 1024, 0.0f), l (y.size()), r (y.size());
    std::vector<float> in (y.size(), 0.0f);
    std::copy (x.begin(), x.end(), in.begin());
    for (size_t p = 0; p < y.size(); p += 512) { const int m = (int) std::min<size_t> (512, y.size() - p); cv.process (in.data() + p, l.data() + p, r.data() + p, m); }
    return l;
}
static double bandCorrelation (const float* a, const float* b, int n, double sr)
{
    // Both band-limited to 80 Hz - 8 kHz, then the normalised correlation (best fractional lag).
    auto band = [&] (const float* x)
    {
        Svf hp1, hp2, lp1, lp2;
        hp1.set (80.0f, 0.7071f, sr); hp2.set (80.0f, 0.7071f, sr); lp1.set (8000.0f, 0.7071f, sr); lp2.set (8000.0f, 0.7071f, sr);
        std::vector<double> y ((size_t) n);
        for (int i = 0; i < n; ++i) y[(size_t) i] = lp2.tickLP (lp1.tickLP (hp2.tickHP (hp1.tickHP (x[i]))));
        return y;
    };
    const auto x = band (a), y = band (b);
    // At lags -1, 0, +1 and the parabola's top: the two may sit a fraction of a sample apart.
    double c[3];
    for (int lag = -1; lag <= 1; ++lag)
    {
        double sxy = 0, sxx = 0, syy = 0;
        for (int i = 2; i < n - 2; ++i) { const double u = x[(size_t) i], v = y[(size_t) (i + lag)]; sxy += u * v; sxx += u * u; syy += v * v; }
        c[lag + 1] = sxy / std::sqrt (sxx * syy + 1e-30);
    }
    const double d = c[0] - 2.0 * c[1] + c[2];
    return d < 0.0 ? c[1] - 0.125 * (c[0] - c[2]) * (c[0] - c[2]) / d : c[1];
}
static int peakIndex (const float* x, int n) { int k = 0; for (int i = 1; i < n; ++i) if (std::fabs (x[i]) > std::fabs (x[k])) k = i; return k; }

static void testTerroir()
{
    std::printf ("-- TERROIR\n");
    const double sr = kSr;
    std::mt19937 rng (11);
    std::normal_distribution<float> gauss (0.0f, 1.0f);
    const auto room = roomIr (sr, 1.2f, 5);
    // 1. The sweep file, played on a speaker whose clock runs 40 ppm slow, through the room, recorded
    //    from 0.7 s before it starts, with the noise of a quiet place.
    {
        const auto file = ir::sweepFile (sr);
        float pk = file.getMagnitude (0, 0, file.getNumSamples());
        const double secs = file.getNumSamples() / sr;
        auto played = ir::stretch (file.getReadPointer (0), file.getNumSamples(), 1.0 + 40.0e-6);
        std::vector<float> pre ((size_t) (0.7 * sr), 0.0f);
        played.insert (played.begin(), pre.begin(), pre.end());
        auto rec = convolveWith (played, room);
        for (auto& v : rec) v += gauss (rng) * 3.0e-4f;                         // about -70 dBFS
        juce::AudioBuffer<float> b (1, (int) rec.size());
        b.copyFrom (0, 0, rec.data(), (int) rec.size());
        const auto t0 = std::chrono::steady_clock::now();
        const auto cap = ir::analyse (b, sr);
        const double took = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        bool ok = cap.ok;
        double corr = 0.0;
        if (ok)
        {
            const float* x = cap.ir.getReadPointer (0);
            const int px = peakIndex (x, cap.ir.getNumSamples()), ph = peakIndex (room.data(), (int) room.size());
            const int n = std::min ({ (int) (0.8 * sr), cap.ir.getNumSamples() - px + (int) (0.002 * sr), (int) room.size() - ph + (int) (0.002 * sr) });
            corr = bandCorrelation (x + px - (int) (0.002 * sr), room.data() + ph - (int) (0.002 * sr), n, sr);
        }
        std::printf ("   sweep file: %.1f s, peak %.1f dBFS; recorded through a 1.2 s room at -70 dB noise, clock 40 ppm slow:\n"
                     "   %s, clock %.1f ppm, RT %.2f s, %.0f dB over the noise, %.2f s long; shape against the room %.4f (%.1f s to analyse)\n",
                     secs, gainToDb (pk), cap.ok ? cap.kind.toRawUTF8() : cap.error.toRawUTF8(), cap.driftPpm, cap.rt60, cap.snrDb,
                     cap.ok ? cap.ir.getNumSamples() / sr : 0.0, corr, took);
        CHECK (std::abs (secs - 43.0) < 0.01 && std::abs (gainToDb (pk) + 6.0f) < 0.1f, "TERROIR sweep file wrong (%.1f s, %.1f dB)", secs, gainToDb (pk));
        CHECK (ok && cap.kind == "SWEEP x3" && std::abs (cap.driftPpm - 40.0) < 3.0 && std::abs (cap.rt60 - 1.2f) < 0.12f && corr > 0.99,
               "TERROIR sweep capture wrong");
    }
    // 1b. The sweep comes in softer under 60 Hz (a small speaker isn't driven where it can't play), and
    //     the analysis takes that out again: the file straight into the recorder gives one click, flat
    //     from 40 Hz to 16 kHz in third-octave bands.
    {
        const auto file = ir::sweepFile (sr);
        const int lead = (int) std::lround (ir::kLeadSec * sr);
        const float pk = file.getMagnitude (0, 0, file.getNumSamples()), pkStart = file.getMagnitude (0, lead, (int) (0.5 * sr));
        juce::AudioBuffer<float> rec (1, file.getNumSamples() + (int) (2.0 * sr));
        rec.clear();
        rec.copyFrom (0, (int) (0.3 * sr), file, 0, 0, file.getNumSamples());
        for (int i = 0; i < rec.getNumSamples(); ++i) rec.addSample (0, i, gauss (rng) * 1.0e-5f);
        const auto cap = ir::analyse (rec, sr);
        double worst = 99.0, worstF = 0.0;
        if (cap.ok)
        {
            constexpr int order = 16, N = 1 << order;
            juce::dsp::FFT fft (order);
            std::vector<float> d ((size_t) N * 2, 0.0f);
            for (int i = 0; i < std::min (N, cap.ir.getNumSamples()); ++i) d[(size_t) i] = cap.ir.getSample (0, i);
            fft.performFrequencyOnlyForwardTransform (d.data());
            auto bandDb = [&] (double fc)
            {
                const int k0 = std::max (1, (int) std::floor (fc / std::pow (2.0, 1.0 / 6.0) * N / sr)), k1 = (int) std::ceil (fc * std::pow (2.0, 1.0 / 6.0) * N / sr);
                double e = 0.0;
                for (int k = k0; k <= k1; ++k) e += (double) d[(size_t) k] * d[(size_t) k];
                return 10.0 * std::log10 (e / (k1 - k0 + 1) + 1e-30);
            };
            const double ref = bandDb (1000.0);
            worst = 0.0;
            for (double fc = 40.0; fc <= 16000.0 * 1.01; fc *= std::pow (2.0, 1.0 / 3.0))
                if (std::abs (bandDb (fc) - ref) > std::abs (worst)) { worst = bandDb (fc) - ref; worstF = fc; }
        }
        std::printf ("   sweep's first 0.5 s %.1f dB under its peak; straight into the recorder: %s, flat 40 Hz-16 kHz within %.2f dB (at %.0f Hz)\n",
                     gainToDb (pkStart / pk), cap.ok ? cap.kind.toRawUTF8() : cap.error.toRawUTF8(), worst, worstF);
        CHECK (gainToDb (pkStart / pk) < -12.0f, "TERROIR sweep's low start not softened (%.1f dB)", gainToDb (pkStart / pk));
        CHECK (cap.ok && std::abs (worst) < 1.0, "TERROIR loopback capture not flat (%.2f dB at %.0f Hz)", worst, worstF);
    }
    // 2. A clap (8 ms of band-passed noise) in the room, 4 s of -65 dB noise, a second clap at 2.6 s.
    {
        std::vector<float> clap ((size_t) (0.008 * sr));
        Svf bp; bp.set (2000.0f, 1.0f, sr);
        for (size_t i = 0; i < clap.size(); ++i) clap[i] = bp.tickBP (gauss (rng)) * std::exp (-(float) i / (float) (0.002 * sr));
        std::vector<float> src ((size_t) (4.0 * sr), 0.0f);
        const int at = (int) (0.5 * sr), second = (int) (2.6 * sr);
        for (size_t i = 0; i < clap.size(); ++i) { src[(size_t) at + i] += clap[i]; src[(size_t) second + i] += clap[i]; }
        auto rec = convolveWith (src, room);
        rec.resize ((size_t) (4.0 * sr));
        for (auto& v : rec) v += gauss (rng) * 5.6e-4f;
        juce::AudioBuffer<float> b (1, (int) rec.size());
        b.copyFrom (0, 0, rec.data(), (int) rec.size());
        const auto cap = ir::analyse (b, sr);
        const double len = cap.ok ? cap.ir.getNumSamples() / sr : 0.0;
        std::printf ("   clap in the same room, a second clap 2.1 s later: %s, RT %.2f s, %.0f dB over the noise, %.2f s long\n",
                     cap.ok ? cap.kind.toRawUTF8() : cap.error.toRawUTF8(), cap.rt60, cap.snrDb, len);
        CHECK (cap.ok && cap.kind == "CLAP" && len > 0.6 && len < 2.1 && std::abs (cap.rt60 - 1.2f) < 0.25f, "TERROIR clap capture wrong");
    }
    // The module's response to one impulse (MIX 100 %, DRY off), offline.
    auto response = [&] (const std::map<std::string, float>& set, double seconds, juce::MemoryBlock* state = nullptr)
    {
        resetBus();
        Rig2 r ("TERROIR");
        r.head.bpm = 120.0;
        r.p->setNonRealtime (true);
        if (state != nullptr) r.p->setStateInformation (state->getData(), (int) state->getSize());
        r.set ("mix", 1.0f); r.set ("dry", 0.0f); r.set ("duck", 0.0f); r.set ("lo_cut", 20.0f);
        for (auto& [k, v] : set) r.set (k.c_str(), v);
        for (int b = 0; b < (int) kSr / kBlock; ++b) r.step (kBlock, nullptr);    // the knobs settle, every swap is done
        std::vector<float> o;
        int64_t t = 0;
        while (t < (int64_t) (seconds * kSr))
        {
            r.step (kBlock, [&] (juce::AudioBuffer<float>& bb, juce::MidiBuffer&, int nn) { for (int i = 0; i < nn; ++i) { const float v = t + i == 0 ? 1.0f : 0.0f; bb.setSample (0, i, v); bb.setSample (1, i, v); } });
            for (int i = 0; i < kBlock; ++i) o.push_back (0.5f * (r.buf.getSample (0, i) + r.buf.getSample (1, i)));
            t += kBlock;
        }
        return o;
    };
    auto rtOf = [&] (const std::vector<float>& o) { juce::AudioBuffer<float> b (1, (int) o.size()); b.copyFrom (0, 0, o.data(), (int) o.size()); return ir::rt60 (b, kSr); };
    auto lastAbove = [&] (const std::vector<float>& o, float db) { float pk = 0; for (auto v : o) pk = std::max (pk, std::fabs (v)); int k = 0; for (int i = 0; i < (int) o.size(); ++i) if (std::fabs (o[(size_t) i]) > pk * dbToGain (db)) k = i; return (k - Convolver::kLatency) / kSr; };
    // 3. The knobs on the built-in room.
    {
        const auto base = response ({}, 3.0), half = response ({ { "decay", 0.5f } }, 3.0), big = response ({ { "size", 2.0f } }, 4.5);
        const auto quarter = response ({ { "length", 5.0f } }, 2.0), rev = response ({ { "length", 5.0f }, { "reverse", 1.0f } }, 2.0);
        const float rt0 = rtOf (base), rtH = rtOf (half), rtB = rtOf (big);
        const double endQ = lastAbove (quarter, -80.0f);
        const int pkRev = peakIndex (rev.data(), (int) rev.size()) - Convolver::kLatency;
        std::printf ("   built-in room RT %.2f s; DECAY 50 %% %.2f s; SIZE 200 %% %.2f s; LENGTH 1/4 at 120 BPM ends %.3f s; REVERSE peaks at %.3f s\n",
                     rt0, rtH, rtB, endQ, pkRev / kSr);
        CHECK (std::abs (rtH / rt0 - 0.5f) < 0.08f, "TERROIR DECAY 50 %% gives RT %.2f of %.2f", rtH, rt0);
        CHECK (std::abs (rtB / rt0 - 2.0f) < 0.25f, "TERROIR SIZE 200 %% gives RT %.2f of %.2f", rtB, rt0);
        CHECK (endQ <= 0.501 && endQ > 0.4, "TERROIR LENGTH 1/4 ends at %.3f s", endQ);
        CHECK (pkRev / kSr > 0.4 && pkRev / kSr <= 0.501, "TERROIR REVERSE peaks at %.3f s", pkRev / kSr);
    }
    // 4. FREEZE: noise for 2 s, FREEZE, 6 s of silence: the held sound stays near the level it had; off, it goes.
    {
        resetBus();
        Rig2 r ("TERROIR");
        r.p->setNonRealtime (true);
        r.set ("mix", 1.0f); r.set ("dry", 0.0f); r.set ("duck", 0.0f);
        std::vector<float> o;
        int64_t t = 0;
        for (int b = 0; b < (int) (10.0 * kSr) / kBlock; ++b)
        {
            const double sec = (double) t / kSr;
            r.set ("freeze", sec >= 2.0 && sec < 8.0 ? 1.0f : 0.0f);
            r.step (kBlock, [&] (juce::AudioBuffer<float>& bb, juce::MidiBuffer&, int nn) { for (int i = 0; i < nn; ++i) { const float v = sec < 2.0 ? gauss (rng) * 0.1f : 0.0f; bb.setSample (0, i, v); bb.setSample (1, i, v); } });
            for (int i = 0; i < kBlock; ++i) o.push_back (r.buf.getSample (0, i));
            t += kBlock;
        }
        auto rms = [&] (double a, double b) { double e = 0; int c = 0; for (int i = (int) (a * kSr); i < (int) (b * kSr); ++i) { e += o[(size_t) i] * o[(size_t) i]; ++c; } return gainToDb ((float) std::sqrt (e / std::max (1, c))); };
        const float before = rms (1.5, 2.0), held = rms (6.0, 8.0), after = rms (9.5, 10.0);
        std::printf ("   FREEZE: %.1f dB before, %.1f dB 4-6 s into the hold (no input), %.1f dB 1.5 s after release\n", before, held, after);
        CHECK (std::abs (held - before) < 3.0f && after < before - 40.0f, "TERROIR FREEZE does not hold (%.1f / %.1f / %.1f dB)", before, held, after);
    }
    // 5. A place dropped on the plugin (a clap recording, on the worker thread), kept in the set, and
    //    renders that repeat exactly, with a knob moving mid-way.
    {
        const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("sf_terroir_test");
        dir.createDirectory();
        const auto wavFile = dir.getChildFile ("Culvert Clap.wav");
        {
            std::vector<float> src ((size_t) (2.0 * sr), 0.0f);
            src[(size_t) (0.3 * sr)] = 0.7f;
            const auto shortRoom = roomIr (sr, 0.4f, 9);
            auto rec = convolveWith (src, std::vector<float> (shortRoom.begin(), shortRoom.begin() + (int) (0.8 * sr)));
            rec.resize ((size_t) (2.0 * sr));
            for (auto& v : rec) v += gauss (rng) * 2.0e-4f;
            juce::AudioBuffer<float> b (1, (int) rec.size());
            b.copyFrom (0, 0, rec.data(), (int) rec.size());
            wavFile.deleteFile();
            juce::WavAudioFormat wav;
            std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (wavFile), sr, 1, 24, {}, 0));
            w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
        }
        resetBus();
        Rig2 r ("TERROIR");
        r.set ("place", 2.0f);                                               // slot 3
        for (int b = 0; b < 4; ++b) r.step (kBlock, nullptr);
        r.p->mod().loadFiles (juce::StringArray (wavFile.getFullPathName()));
        juce::MemoryBlock state;
        bool stored = false;
        for (const double until = juce::Time::getMillisecondCounterHiRes() + 120000.0; ! stored && juce::Time::getMillisecondCounterHiRes() < until;)
        {
            juce::Thread::sleep (25);
            r.step (kBlock, nullptr);
            state.reset();
            r.p->getStateInformation (state);
            auto root = juce::ValueTree::readFromData (state.getData(), state.getSize());
            const auto m = root.getChildWithName ("MODULE");
            for (int i = 0; i < m.getNumChildren(); ++i)
                if (m.getChild (i).hasType ("PLACE") && (int) m.getChild (i).getProperty ("slot") == 2 && m.getChild (i).getProperty ("name").toString() == "CULVERT CLAP") stored = true;
        }
        const auto a = response ({ { "place", 2.0f } }, 1.5, &state), base = response ({}, 1.5);
        const float rtA = rtOf (a), rtBase = rtOf (base);
        std::printf ("   dropped \"Culvert Clap.wav\" on slot 3: %s in the set; it plays (RT %.2f s, built-in %.2f s)\n", stored ? "stored" : "NOT STORED", rtA, rtBase);
        CHECK (stored && std::abs (rtA - 0.4f) < 0.1f, "TERROIR drop / store / reload failed (RT %.2f)", rtA);
        auto offline = [&] ()
        {
            resetBus();
            Rig2 q ("TERROIR");
            q.p->setNonRealtime (true);
            q.p->setStateInformation (state.getData(), (int) state.getSize());
            q.set ("place", 2.0f); q.set ("mix", 0.5f);
            std::vector<float> o;
            int64_t t = 0;
            for (int b = 0; b < (int) (3.0 * kSr) / kBlock; ++b)
            {
                if (t >= (int64_t) kSr) q.set ("decay", 0.4f);
                if (t >= (int64_t) (2 * kSr)) q.set ("size", 1.5f);
                q.step (kBlock, [&] (juce::AudioBuffer<float>& bb, juce::MidiBuffer&, int nn) { for (int i = 0; i < nn; ++i) { uint32_t h = (uint32_t) (t + i) * 2654435761u; h ^= h >> 15; const float v = 0.2f * ((float) (h & 0xffff) / 32768.0f - 1.0f) * ((t + i) % 12000 < 2000 ? 1.0f : 0.0f); bb.setSample (0, i, v); bb.setSample (1, i, v); } });
                for (int i = 0; i < kBlock; ++i) o.push_back (q.buf.getSample (0, i));
                t += kBlock;
            }
            return o;
        };
        const auto o1 = offline(), o2 = offline();
        double diff = 0.0;
        for (size_t i = 0; i < std::min (o1.size(), o2.size()); ++i) diff = std::max (diff, (double) std::fabs (o1[i] - o2[i]));
        std::printf ("   two offline renders with DECAY and SIZE moving: largest difference %.1e\n", diff);
        CHECK (diff == 0.0, "TERROIR offline render not repeatable (%.1e)", diff);
        dir.deleteRecursively();
    }
    // 6. Level: a held chord through SF REVERB and SF TERROIR (built-in room) at their defaults, wet only.
    {
        auto wetRms = [&] (const char* id)
        {
            resetBus();
            Rig2 r (id);
            r.p->setNonRealtime (true);
            r.set ("mix", 1.0f); r.set ("dry", 0.0f); r.set ("duck", 0.0f);
            double e = 0; int c = 0;
            int64_t t = 0;
            for (int b = 0; b < (int) (8.0 * kSr) / kBlock; ++b)
            {
                r.step (kBlock, [&] (juce::AudioBuffer<float>& bb, juce::MidiBuffer&, int nn)
                {
                    for (int i = 0; i < nn; ++i)
                    {
                        const double q = (double) (t + i) / kSr;
                        const float v = 0.1f * (float) (std::sin (6.283185307 * 220.0 * q) + std::sin (6.283185307 * 277.18 * q) + std::sin (6.283185307 * 329.63 * q));
                        bb.setSample (0, i, v); bb.setSample (1, i, v);
                    }
                });
                if (t > (int64_t) (3 * kSr)) for (int i = 0; i < kBlock; ++i) { e += r.buf.getSample (0, i) * r.buf.getSample (0, i); ++c; }
                t += kBlock;
            }
            return gainToDb ((float) std::sqrt (e / c));
        };
        const float rv = wetRms ("REVERB"), tr = wetRms ("TERROIR");
        std::printf ("   held chord, wet only: SF REVERB %.1f dB, SF TERROIR %.1f dB\n", rv, tr);
        CHECK (std::abs (tr - rv) < 2.0f, "TERROIR level %.1f dB against REVERB %.1f dB", tr, rv);
    }
    // 7. A field recorder's poly file, as a MixPre writes it: MixL and MixR (the two mics summed in
    //    the middle) ahead of a spaced pair, the right mic 1.5 ms later and 3 dB down. Named in the
    //    bext description, or in iXML. The pair is used and keeps its time and level difference; a
    //    mono take saved as stereo becomes a mono place (its tail spread).
    {
        const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("sf_terroir_poly");
        dir.deleteRecursively();
        dir.createDirectory();
        const auto roomL = roomIr (sr, 0.5f, 21), roomR = roomIr (sr, 0.5f, 22);
        const int n = (int) (2.0 * sr), at = (int) (0.3 * sr), lag = (int) std::lround (0.0015 * sr);
        juce::AudioBuffer<float> poly (4, n);
        poly.clear();
        for (int i = 0; i < (int) (0.9 * sr); ++i)
        {
            poly.setSample (2, at + i, 0.6f * roomL[(size_t) i]);
            poly.setSample (3, at + lag + i, 0.42f * roomR[(size_t) i]);
        }
        for (int c = 2; c < 4; ++c) for (int i = 0; i < n; ++i) poly.addSample (c, i, gauss (rng) * 1.0e-4f);
        for (int i = 0; i < n; ++i) { const float m = 0.5f * (poly.getSample (2, i) + poly.getSample (3, i)); poly.setSample (0, i, m); poly.setSample (1, i, m); }
        auto write = [&] (const juce::File& f, const juce::AudioBuffer<float>& b, const juce::StringPairArray& meta, int bits = 24)
        {
            f.deleteFile();
            juce::WavAudioFormat wav;
            std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (f), sr, (unsigned) b.getNumChannels(), bits, meta, 0));
            w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
        };
        const auto bextFile = dir.getChildFile ("Factory Poly.wav"), ixmlFile = dir.getChildFile ("Factory Ixml.wav"), monoFile = dir.getChildFile ("Shed Mono.wav");
        write (bextFile, poly, juce::WavAudioFormat::createBWAVMetadata ("sSPEED=048.000-ND\r\nsTRK1=MixL\r\nsTRK2=MixR\r\nsTRK3=Room L\r\nsTRK4=Room R\r\n",
                                                                         "SOUND DEVICES", {}, juce::Time::getCurrentTime(), 0, {}));
        write (ixmlFile, poly, {});
        {
            // An iXML chunk after the audio (as some recorders write it), the RIFF size patched.
            const juce::String x = "<?xml version=\"1.0\" encoding=\"UTF-8\"?><BWFXML><TRACK_LIST><TRACK_COUNT>4</TRACK_COUNT>"
                                   "<TRACK><CHANNEL_INDEX>3</CHANNEL_INDEX><INTERLEAVE_INDEX>3</INTERLEAVE_INDEX><NAME>Factory L</NAME></TRACK>"
                                   "<TRACK><CHANNEL_INDEX>4</CHANNEL_INDEX><INTERLEAVE_INDEX>4</INTERLEAVE_INDEX><NAME>Factory R</NAME></TRACK>"
                                   "<TRACK><CHANNEL_INDEX>1</CHANNEL_INDEX><INTERLEAVE_INDEX>1</INTERLEAVE_INDEX><NAME>MixL</NAME></TRACK>"
                                   "<TRACK><CHANNEL_INDEX>2</CHANNEL_INDEX><INTERLEAVE_INDEX>2</INTERLEAVE_INDEX><NAME>MixR</NAME></TRACK>"
                                   "</TRACK_LIST></BWFXML>";
            juce::MemoryBlock mb;
            ixmlFile.loadFileAsData (mb);
            {
                juce::MemoryOutputStream out (mb, true);
                const auto bytes = x.getNumBytesAsUTF8();
                out.write ("iXML", 4);
                out.writeInt ((int) bytes);
                out.write (x.toRawUTF8(), bytes);
                if ((bytes & 1) != 0) out.writeByte (0);
            }
            const auto riff = (juce::uint32) (mb.getSize() - 8);
            auto* d = static_cast<juce::uint8*> (mb.getData());
            for (int k = 0; k < 4; ++k) d[4 + k] = (juce::uint8) ((riff >> (8 * k)) & 0xffu);
            ixmlFile.replaceWithData (mb.getData(), mb.getSize());
        }
        {
            juce::AudioBuffer<float> mono (2, n);
            mono.copyFrom (0, 0, poly, 2, 0, n);
            mono.copyFrom (1, 0, poly, 2, 0, n);
            write (monoFile, mono, {});
        }
        auto namesOf = [&] (const juce::File& f)
        {
            juce::WavAudioFormat wav;
            std::unique_ptr<juce::AudioFormatReader> r (wav.createReaderFor (new juce::FileInputStream (f), true));
            return r == nullptr ? juce::StringArray() : ir::trackNames (f, r->metadataValues.getValue (juce::WavAudioFormat::bwavDescription, {}), (int) r->numChannels);
        };
        const auto nb = namesOf (bextFile), nx = namesOf (ixmlFile), nm = namesOf (monoFile);
        const auto pb = ir::pickTracks (nb, 4), px = ir::pickTracks (nx, 4);
        std::printf ("   poly file names: bext %s, iXML %s, none %d; tracks used %d + %d, %d + %d\n",
                     nb.joinIntoString ("|").toRawUTF8(), nx.joinIntoString ("|").toRawUTF8(), nm.size(),
                     pb.size() > 0 ? pb[0] + 1 : 0, pb.size() > 1 ? pb[1] + 1 : 0, px.size() > 0 ? px[0] + 1 : 0, px.size() > 1 ? px[1] + 1 : 0);
        CHECK (nb.joinIntoString ("|") == "MixL|MixR|Room L|Room R" && nx.joinIntoString ("|") == "MixL|MixR|Factory L|Factory R" && nm.isEmpty(),
               "TERROIR track names wrong (%s / %s)", nb.joinIntoString ("|").toRawUTF8(), nx.joinIntoString ("|").toRawUTF8());
        CHECK (pb == (std::vector<int> { 2, 3 }) && px == (std::vector<int> { 2, 3 }) && ir::pickTracks ({}, 4) == (std::vector<int> { 0, 1 })
                 && ir::pickTracks ({ "MixL", "MixR", "Ch1" }, 3) == (std::vector<int> { 2 }) && ir::pickTracks ({ "MixL", "MixR" }, 2) == (std::vector<int> { 0, 1 })
                 && ir::pickTracks ({ "L", "R" }, 2) == (std::vector<int> { 0, 1 }) && ir::pickTracks ({}, 1) == (std::vector<int> { 0 }),
               "TERROIR picks the wrong tracks");
        // Dropped on the plugin: the place as stored in the set.
        auto dropped = [&] (const juce::File& f, int slot, const juce::String& name)
        {
            resetBus();
            Rig2 r ("TERROIR");
            r.set ("place", (float) slot);
            for (int b = 0; b < 4; ++b) r.step (kBlock, nullptr);
            r.p->mod().loadFiles (juce::StringArray (f.getFullPathName()));
            juce::AudioBuffer<float> a;
            for (const double until = juce::Time::getMillisecondCounterHiRes() + 120000.0; a.getNumChannels() == 0 && juce::Time::getMillisecondCounterHiRes() < until;)
            {
                juce::Thread::sleep (25);
                r.step (kBlock, nullptr);
                juce::MemoryBlock state;
                r.p->getStateInformation (state);
                const auto m = juce::ValueTree::readFromData (state.getData(), state.getSize()).getChildWithName ("MODULE");
                for (int i = 0; i < m.getNumChildren(); ++i)
                {
                    const auto c = m.getChild (i);
                    if (! c.hasType ("PLACE") || (int) c.getProperty ("slot") != slot || c.getProperty ("name").toString() != name) continue;
                    if (const auto* mb = c.getProperty ("flac").getBinaryData())
                    {
                        juce::FlacAudioFormat fmt;
                        std::unique_ptr<juce::AudioFormatReader> rd (fmt.createReaderFor (new juce::MemoryInputStream (*mb, true), true));
                        if (rd != nullptr)
                        {
                            a.setSize ((int) rd->numChannels, (int) rd->lengthInSamples);
                            rd->read (a.getArrayOfWritePointers(), a.getNumChannels(), 0, a.getNumSamples());
                        }
                    }
                }
            }
            return a;
        };
        const auto pair = dropped (bextFile, 3, "FACTORY POLY"), monoPlace = dropped (monoFile, 4, "SHED MONO");
        int dt = 0;
        float ild = 0.0f;
        if (pair.getNumChannels() == 2)
        {
            dt = peakIndex (pair.getReadPointer (1), pair.getNumSamples()) - peakIndex (pair.getReadPointer (0), pair.getNumSamples());
            double el = 0, er = 0;
            for (int i = 0; i < pair.getNumSamples(); ++i) { el += (double) pair.getSample (0, i) * pair.getSample (0, i); er += (double) pair.getSample (1, i) * pair.getSample (1, i); }
            ild = (float) (10.0 * std::log10 (er / std::max (el, 1e-30)));
        }
        std::printf ("   dropped as a place: the pair %d channels, right %d samples later (want %d), %.2f dB (want %.2f); the mono take %d channel(s)\n",
                     pair.getNumChannels(), dt, lag, ild, gainToDb (0.42f / 0.6f), monoPlace.getNumChannels());
        CHECK (pair.getNumChannels() == 2 && std::abs (dt - lag) <= 1 && std::abs (ild - gainToDb (0.42f / 0.6f)) < 0.5f,
               "TERROIR poly file: %d channels, %d samples apart, %.2f dB", pair.getNumChannels(), dt, ild);
        CHECK (monoPlace.getNumChannels() == 1, "TERROIR: a mono take saved as stereo stays stereo (%d channels)", monoPlace.getNumChannels());

        // 8. Stereo from one mic: the sweep recorded twice, from two spots (two rooms here), each take
        //    started by hand (0.6 s and 1.7 s before the sweep), one speaker clock 30 ppm off, files
        //    "Hall L" and "Hall R" dropped together. Each side is its own take, from its own onset.
        //    And a stereo IR split into two files of the same length ("Plate L" / "Plate R": the poly
        //    file's pair) stays one recording: its 1.5 ms between the sides is kept.
        {
            const auto file = ir::sweepFile (sr);
            const auto roomA = roomIr (sr, 0.9f, 31), roomB = roomIr (sr, 0.9f, 32);
            auto take = [&] (const std::vector<float>& rm, double lead, double ppm, double tailSec)
            {
                auto played = ir::stretch (file.getReadPointer (0), file.getNumSamples(), 1.0 + ppm * 1.0e-6);
                played.insert (played.begin(), (size_t) (lead * sr), 0.0f);
                auto rec = convolveWith (played, rm);
                rec.resize (played.size() + (size_t) (tailSec * sr));
                for (auto& v : rec) v += gauss (rng) * 3.0e-4f;
                juce::AudioBuffer<float> b (1, (int) rec.size());
                b.copyFrom (0, 0, rec.data(), (int) rec.size());
                return b;
            };
            // 32-bit float files, as a MixPre writes them: these rooms take the sweep over full scale.
            const auto hallL = dir.getChildFile ("Hall L.wav"), hallR = dir.getChildFile ("Hall R.wav");
            write (hallL, take (roomA, 0.6, 0.0, 2.0), {}, 32);
            write (hallR, take (roomB, 1.7, 30.0, 3.1), {}, 32);
            resetBus();
            Rig2 r ("TERROIR");
            r.set ("place", 5.0f);
            for (int b = 0; b < 4; ++b) r.step (kBlock, nullptr);
            r.p->mod().loadFiles (juce::StringArray (hallL.getFullPathName(), hallR.getFullPathName()));
            auto placeIn = [&] (Rig2& rr, int slot, const juce::String& name)
            {
                juce::AudioBuffer<float> a;
                for (const double until = juce::Time::getMillisecondCounterHiRes() + 120000.0; a.getNumChannels() == 0 && juce::Time::getMillisecondCounterHiRes() < until;)
                {
                    juce::Thread::sleep (25);
                    rr.step (kBlock, nullptr);
                    juce::MemoryBlock state;
                    rr.p->getStateInformation (state);
                    const auto m = juce::ValueTree::readFromData (state.getData(), state.getSize()).getChildWithName ("MODULE");
                    for (int i = 0; i < m.getNumChildren(); ++i)
                    {
                        const auto c = m.getChild (i);
                        if (! c.hasType ("PLACE") || (int) c.getProperty ("slot") != slot || c.getProperty ("name").toString() != name) continue;
                        if (const auto* mb = c.getProperty ("flac").getBinaryData())
                        {
                            juce::FlacAudioFormat fmt;
                            std::unique_ptr<juce::AudioFormatReader> rd (fmt.createReaderFor (new juce::MemoryInputStream (*mb, true), true));
                            if (rd != nullptr)
                            {
                                a.setSize ((int) rd->numChannels, (int) rd->lengthInSamples);
                                rd->read (a.getArrayOfWritePointers(), a.getNumChannels(), 0, a.getNumSamples());
                            }
                        }
                    }
                }
                return a;
            };
            const auto hall = placeIn (r, 5, "HALL");
            double cA = 0.0, cB = 0.0;
            int dPk = -1;
            if (hall.getNumChannels() == 2)
            {
                auto shape = [&] (const float* x, int nx, const std::vector<float>& rm)
                {
                    const int px = peakIndex (x, nx), ph = peakIndex (rm.data(), (int) rm.size()), pre = (int) (0.002 * sr);
                    const int len = std::min ({ (int) (0.8 * sr), nx - px + pre, (int) rm.size() - ph + pre });
                    return px < pre ? 0.0 : bandCorrelation (x + px - pre, rm.data() + ph - pre, len, sr);
                };
                cA = shape (hall.getReadPointer (0), hall.getNumSamples(), roomA);
                cB = shape (hall.getReadPointer (1), hall.getNumSamples(), roomB);
                dPk = std::abs (peakIndex (hall.getReadPointer (1), hall.getNumSamples()) - peakIndex (hall.getReadPointer (0), hall.getNumSamples()));
            }
            std::printf ("   two takes from two spots (\"Hall L\" / \"Hall R\"): %d channels, left against its room %.4f, right %.4f, onsets %d samples apart\n",
                         hall.getNumChannels(), cA, cB, dPk);
            CHECK (hall.getNumChannels() == 2 && cA > 0.99 && cB > 0.99 && dPk >= 0 && dPk <= 2,
                   "TERROIR two takes: %d channels, %.4f / %.4f, %d samples apart", hall.getNumChannels(), cA, cB, dPk);
            // The split stereo IR.
            const auto plateL = dir.getChildFile ("Plate L.wav"), plateR = dir.getChildFile ("Plate R.wav");
            {
                juce::AudioBuffer<float> one (1, n);
                one.copyFrom (0, 0, poly, 2, 0, n); write (plateL, one, {});
                one.copyFrom (0, 0, poly, 3, 0, n); write (plateR, one, {});
            }
            resetBus();
            Rig2 q ("TERROIR");
            q.set ("place", 6.0f);
            for (int b = 0; b < 4; ++b) q.step (kBlock, nullptr);
            q.p->mod().loadFiles (juce::StringArray (plateL.getFullPathName(), plateR.getFullPathName()));
            const auto plate = placeIn (q, 6, "PLATE");
            const int dt2 = plate.getNumChannels() == 2 ? peakIndex (plate.getReadPointer (1), plate.getNumSamples()) - peakIndex (plate.getReadPointer (0), plate.getNumSamples()) : -1;
            std::printf ("   a stereo IR as two files of one length (\"Plate L\" / \"Plate R\"): %d channels, right %d samples later (want %d)\n", plate.getNumChannels(), dt2, lag);
            CHECK (plate.getNumChannels() == 2 && std::abs (dt2 - lag) <= 1, "TERROIR split stereo IR: %d channels, %d samples apart", plate.getNumChannels(), dt2);
        }
        dir.deleteRecursively();
    }
}

// A finished IR comes in as it is, and the sweep detector holds for odd files (Nov 5 audit, F6): a
// click at the first sample, a 5 ms response, a direct sound before a stronger reflection (and one
// before a weaker), stereo timing and polarity, a click in 20 s of silence (no sweep), a sweep with
// one channel inverted (a sweep), rooms recorded in noise. Timing and transfer, not just energy.
static void testIrImport()
{
    std::printf ("-- IR import: IR files as they are; claps cut and said; sweep or not, and SF CAPTURE's take a sweep or a failure\n");
    const double sr = kSr;
    const int lead = (int) (0.001 * sr), w = (int) (0.0005 * sr);
    auto energy = [] (const juce::AudioBuffer<float>& b) { double e = 0.0; for (int c = 0; c < b.getNumChannels(); ++c) for (int i = 0; i < b.getNumSamples(); ++i) e += (double) b.getSample (c, i) * b.getSample (c, i); return e / std::max (1, b.getNumChannels()); };
    auto dtft = [&] (const juce::AudioBuffer<float>& b, int ch, double hz)
    {
        std::complex<double> z {};
        for (int i = 0; i < b.getNumSamples(); ++i) z += (double) b.getSample (ch, i) * std::polar (1.0, -2.0 * 3.141592653589793 * hz * i / sr);
        return 20.0 * std::log10 (std::abs (z) + 1e-30);
    };
    // Arrivals: samples over 5 % of the largest, the first of each run 1 ms apart.
    auto arrivals = [] (const juce::AudioBuffer<float>& b, int ch)
    {
        std::vector<int> a;
        const float pk = b.getMagnitude (ch, 0, b.getNumSamples());
        for (int i = 0; i < b.getNumSamples(); ++i)
            if (std::fabs (b.getSample (ch, i)) > 0.05f * pk && (a.empty() || i - a.back() > 48)) a.push_back (i);
        return a;
    };
    // 1. One click, at sample 0, 1, 24, 48, 240 of a 1 s file: the click 1 ms (up to 1.5) in; flat
    //    but for the 20 Hz high-pass every import gets (on a 30 ms IR, 0.1 dB at 100 Hz).
    {
        bool ok = true;
        std::printf ("   a click at sample");
        for (int at : { 0, 1, 24, 48, 240 })
        {
            juce::AudioBuffer<float> b (1, (int) sr); b.clear(); b.setSample (0, at, 1.0f);
            const auto c = ir::analyse (b, sr);
            const int pos = c.ok ? peakIndex (c.ir.getReadPointer (0), c.ir.getNumSamples()) : -1;
            const double lo = c.ok ? dtft (c.ir, 0, 100.0) : 0.0, mid = c.ok ? dtft (c.ir, 0, 1000.0) : 0.0, hi = c.ok ? dtft (c.ir, 0, 10000.0) : 0.0;
            const double spread = std::max ({ lo, mid, hi }) - std::min ({ lo, mid, hi });
            std::printf (" %d: %s at %d, energy %.3f, 100 Hz-10 kHz within %.3f dB;", at, c.ok ? c.kind.toRawUTF8() : c.error.toRawUTF8(), pos, c.ok ? energy (c.ir) : 0.0, spread);
            ok = ok && c.ok && c.kind == "IR FILE" && pos >= lead && pos <= lead + w && std::abs (energy (c.ir) - ir::kEnergy) < 1e-3 && spread < 0.15;
        }
        std::printf ("\n");
        CHECK (ok, "IR import: a click near the file start is not kept as it is");
    }
    // 2. A 5 ms response in a 10 ms file.
    {
        juce::AudioBuffer<float> b (1, 480); b.clear();
        for (int i = 0; i < 240; ++i) b.setSample (0, i, (float) (std::exp (-i / 60.0) * std::sin (2.0 * 3.141592653589793 * 3000.0 * i / 48000.0 + 0.3)));
        const auto c = ir::analyse (b, sr);
        double corr = 0.0, ea = 0.0, eb = 0.0;
        if (c.ok)
            for (int i = 0; i < 240 && lead + i < c.ir.getNumSamples(); ++i)
            { const double x = c.ir.getSample (0, lead + i), y = b.getSample (0, i); corr += x * y; ea += x * x; eb += y * y; }
        corr /= std::sqrt (ea * eb + 1e-30);
        std::printf ("   a 5 ms response in a 10 ms file: %s, %.1f ms, shape %.4f\n", c.ok ? c.kind.toRawUTF8() : c.error.toRawUTF8(), c.ok ? 1000.0 * c.ir.getNumSamples() / sr : 0.0, corr);
        CHECK (c.ok && corr > 0.999, "IR import: a 5 ms response (%s, shape %.4f)", c.ok ? "ok" : c.error.toRawUTF8(), corr);
    }
    // 3, 4. A direct sound at 5 ms and a reflection at 85 ms, the reflection 4 times stronger, then 4
    //       times weaker: both kept, 3840 samples apart, in their ratio.
    for (int strongerLater = 1; strongerLater >= 0; --strongerLater)
    {
        juce::AudioBuffer<float> b (1, (int) sr); b.clear();
        b.setSample (0, 240, strongerLater ? 0.25f : 1.0f); b.setSample (0, 4080, strongerLater ? 1.0f : 0.25f);
        const auto c = ir::analyse (b, sr);
        const auto a = c.ok ? arrivals (c.ir, 0) : std::vector<int> {};
        const float ratio = a.size() >= 2 ? std::fabs (c.ir.getSample (0, a[1]) / c.ir.getSample (0, a[0])) : 0.0f;
        std::printf ("   a direct sound and a reflection %s 80 ms later: %s, arrivals", strongerLater ? "4x stronger" : "4x weaker", c.ok ? c.kind.toRawUTF8() : c.error.toRawUTF8());
        for (int i : a) std::printf (" %d", i);
        std::printf (", second against first %.3f (want %.3f)\n", ratio, strongerLater ? 4.0 : 0.25);
        CHECK (c.ok && a.size() == 2 && a[1] - a[0] == 3840 && a[0] >= lead && a[0] <= lead + w && std::abs (ratio / (strongerLater ? 4.0f : 0.25f) - 1.0f) < 0.02f,
               "IR import: direct sound and reflection (%s) not kept", strongerLater ? "stronger later" : "weaker later");
    }
    // 5. Stereo: the right arrival 72 samples after the left, inverted.
    {
        juce::AudioBuffer<float> b (2, (int) (0.5 * sr)); b.clear();
        b.setSample (0, 96, 1.0f); b.setSample (0, 1440, 0.3f);
        b.setSample (1, 96 + 72, -0.8f); b.setSample (1, 1920, 0.2f);
        const auto c = ir::analyse (b, sr);
        const int pl = c.ok ? peakIndex (c.ir.getReadPointer (0), c.ir.getNumSamples()) : 0;
        const int pr = c.ok && c.ir.getNumChannels() > 1 ? peakIndex (c.ir.getReadPointer (1), c.ir.getNumSamples()) : 0;
        const float sl = c.ok ? c.ir.getSample (0, pl) : 0.0f, srt = c.ok && c.ir.getNumChannels() > 1 ? c.ir.getSample (1, pr) : 0.0f;
        std::printf ("   stereo, the right 72 samples later and inverted: %s, %d channels, right %d samples later, signs %+.0f %+.0f, right/left %.3f (want -0.800)\n",
                     c.ok ? c.kind.toRawUTF8() : c.error.toRawUTF8(), c.ok ? c.ir.getNumChannels() : 0, pr - pl, (double) (sl > 0 ? 1 : -1), (double) (srt > 0 ? 1 : -1), sl != 0.0f ? srt / sl : 0.0f);
        CHECK (c.ok && c.ir.getNumChannels() == 2 && pr - pl == 72 && std::abs (srt / sl + 0.8f) < 0.01f, "IR import: stereo timing or polarity lost");
    }
    // 6. A click in 20 s of silence, at 0.5 s and at 12.5 s: an IR file (silence before it), not a sweep.
    for (double t : { 0.5, 12.5 })
    {
        juce::AudioBuffer<float> b (1, (int) (20.0 * sr)); b.clear(); b.setSample (0, (int) (t * sr), 1.0f);
        const auto c = ir::analyse (b, sr);
        std::printf ("   a click at %.1f s in 20 s of silence: %s, %.3f s long\n", t, c.ok ? c.kind.toRawUTF8() : c.error.toRawUTF8(), c.ok ? c.ir.getNumSamples() / sr : 0.0);
        CHECK (c.ok && c.kind == "IR FILE" && c.ir.getNumSamples() < (int) (0.1 * sr), "IR import: a click in silence taken for %s", c.kind.toRawUTF8());
    }
    // 7. The sweep in the left channel and inverted in the right: a sweep; the two IRs opposite.
    {
        const auto s = ir::sweep (sr);
        juce::AudioBuffer<float> b (2, 12 * (int) sr); b.clear();
        for (int i = 0; i < (int) s.size(); ++i) { b.setSample (0, (int) sr + i, 0.5f * s[(size_t) i]); b.setSample (1, (int) sr + i, -0.5f * s[(size_t) i]); }
        const auto c = ir::analyse (b, sr);
        double xy = 0.0, xx = 0.0, yy = 0.0;
        if (c.ok && c.ir.getNumChannels() == 2)
            for (int i = 0; i < c.ir.getNumSamples(); ++i) { const double x = c.ir.getSample (0, i), y = c.ir.getSample (1, i); xy += x * y; xx += x * x; yy += y * y; }
        const double corr = xy / std::sqrt (xx * yy + 1e-30);
        std::printf ("   the sweep, the right channel inverted: %s, left against right %.4f\n", c.ok ? c.kind.toRawUTF8() : c.error.toRawUTF8(), corr);
        CHECK (c.ok && c.kind == "SWEEP x1" && corr < -0.999, "IR import: inverted stereo sweep (%s, %.4f)", c.ok ? c.kind.toRawUTF8() : c.error.toRawUTF8(), corr);
    }
    // 8. The built-in room recorded in 20 s of noise at -90, -70, -50 dBFS: a clap recording. Over
    //    the noise, it is kept from its first arrival (10 % of the peak: as long before the peak as in
    //    the room), the room's shape from there; at -50 dB that arrival is in the noise (2 dB over
    //    it), so the shape is compared from the peak.
    {
        const auto room = ir::builtInRoom (sr);
        auto firstOver = [] (const float* x, int n, float frac) { float pk = 0.0f; for (int i = 0; i < n; ++i) pk = std::max (pk, std::fabs (x[i])); int i = 0; while (i < n && std::fabs (x[i]) < frac * pk) ++i; return i; };
        const int roomPeak = peakIndex (room.getReadPointer (0), room.getNumSamples()), roomFirst = firstOver (room.getReadPointer (0), room.getNumSamples(), 0.1f);
        std::printf ("   the built-in room (first arrival %.1f ms before its peak) recorded in noise:", (roomPeak - roomFirst) * 1000.0 / sr);
        bool ok = true;
        for (float noiseDb : { -90.0f, -70.0f, -50.0f })
        {
            std::mt19937 rng (1234);
            std::normal_distribution<float> g (0.0f, 1.0f);
            juce::AudioBuffer<float> b (1, (int) (20.0 * sr));
            for (int i = 0; i < b.getNumSamples(); ++i) b.setSample (0, i, g (rng) * std::pow (10.0f, noiseDb / 20.0f));
            for (int i = 0; i < room.getNumSamples(); ++i) b.addSample (0, (int) (0.5 * sr) + i, 0.5f * room.getSample (0, i));
            const auto c = ir::analyse (b, sr);
            const int n = c.ok ? c.ir.getNumSamples() : 0;
            const int pk = n > 0 ? peakIndex (c.ir.getReadPointer (0), n) : 0, fst = n > 0 ? firstOver (c.ir.getReadPointer (0), n, 0.1f) : 0;
            const bool clean = noiseDb < -60.0f;
            const int from = clean ? fst : pk, roomFrom = clean ? roomFirst : roomPeak;
            const int m = std::min ({ n - from, room.getNumSamples() - roomFrom, (int) (0.1 * sr) });
            const double corr = m > 0 ? bandCorrelation (c.ir.getReadPointer (0) + from, room.getReadPointer (0) + roomFrom, m, sr) : 0.0;
            std::printf (" %.0f dB: %s, first arrival at %.2f ms, %.1f ms before the peak, from %s against the room %.4f;", noiseDb,
                         c.ok ? c.kind.toRawUTF8() : c.error.toRawUTF8(), fst * 1000.0 / sr, (pk - fst) * 1000.0 / sr, clean ? "there" : "the peak", corr);
            ok = ok && c.ok && c.kind == "CLAP" && corr > (clean ? 0.99 : 0.85)             // -50 dB: what the noise allows (0.89)
                 && (! clean || (std::abs ((pk - fst) - (roomPeak - roomFirst)) <= 1 && fst >= lead));
        }
        std::printf ("\n");
        CHECK (ok, "IR import: a room recorded in noise is not kept from its first arrival");
    }
    // 9. Samples that are not numbers: refused.
    {
        juce::AudioBuffer<float> b (1, (int) sr); b.clear(); b.setSample (0, 100, 1.0f); b.setSample (0, 2000, std::numeric_limits<float>::quiet_NaN());
        const auto c = ir::analyse (b, sr);
        std::printf ("   a file with a NaN: %s\n", c.ok ? "taken" : c.error.toRawUTF8());
        CHECK (! c.ok, "IR import: a NaN taken in");
    }
    // 10. Sparse IR files (352df3c audit, A09): the direct sound at sample 0 and one reflection 0.25,
    //     0.8 or 1.6 times as strong, 20, 80, 280 or 400 ms later, silence between; AUTO and IMPORT IR
    //     FILE. Both arrivals in their spacing and ratio, nothing named, and the comb they make (the
    //     response at 1 kHz, a peak, against the notch next to it) within 0.3 dB of the file's.
    {
        int bad = 0, cases = 0;
        double worst = 0.0;
        for (auto src : { ir::Source::Auto, ir::Source::IrFile })
            for (float r : { 0.25f, 0.8f, 1.6f })
                for (double ms : { 20.0, 80.0, 280.0, 400.0 })
                {
                    const int d = (int) std::lround (ms * 0.001 * sr);
                    juce::AudioBuffer<float> b (1, d + (int) (0.1 * sr)); b.clear();
                    b.setSample (0, 0, 1.0f); b.setSample (0, d, r);
                    const auto c = ir::analyse (b, sr, src);
                    const auto a = c.ok ? arrivals (c.ir, 0) : std::vector<int> {};
                    const float ratio = a.size() == 2 ? c.ir.getSample (0, a[1]) / c.ir.getSample (0, a[0]) : 0.0f;
                    const double notch = 1000.0 + 500.0 / ms;
                    const double inC = dtft (b, 0, 1000.0) - dtft (b, 0, notch), outC = c.ok ? dtft (c.ir, 0, 1000.0) - dtft (c.ir, 0, notch) : 0.0;
                    const bool good = c.ok && c.kind == "IR FILE" && c.note.isEmpty() && a.size() == 2 && a[1] - a[0] == d
                                   && std::abs (ratio / r - 1.0f) < 0.02f && std::abs (outC - inC) < 0.3;
                    worst = std::max (worst, std::abs (outC - inC));
                    ++cases;
                    if (! good)
                    {
                        ++bad;
                        std::printf ("      %s, a reflection %.2f at %.0f ms: %s, %d arrivals, ratio %.3f, comb %.2f dB (the file's %.2f)\n", src == ir::Source::Auto ? "AUTO" : "IR FILE",
                                     r, ms, c.ok ? c.kind.toRawUTF8() : c.error.toRawUTF8(), (int) a.size(), ratio, outC, inC);
                    }
                }
        std::printf ("   sparse IR files (a reflection 0.25 / 0.8 / 1.6 times the direct sound, 20-400 ms on; AUTO and IR FILE): %d of %d whole, the comb within %.3f dB\n", cases - bad, cases, worst);
        CHECK (bad == 0, "IR import: %d of %d sparse IR files not kept as they are", bad, cases);
    }
    // 11. A sparse stereo IR file: the left 1 at 0 and 0.8 at 80 ms, the right -0.6 at 1.5 ms and 0.9
    //     at 150 ms, silence between. Each arrival where it was against the other channel's, its sign,
    //     its level against the left's first.
    {
        juce::AudioBuffer<float> b (2, (int) (0.3 * sr)); b.clear();
        b.setSample (0, 0, 1.0f); b.setSample (0, 3840, 0.8f);
        b.setSample (1, 72, -0.6f); b.setSample (1, 7200, 0.9f);
        const auto c = ir::analyse (b, sr);
        const auto l = c.ok ? arrivals (c.ir, 0) : std::vector<int> {};
        const auto rgt = c.ok && c.ir.getNumChannels() == 2 ? arrivals (c.ir, 1) : std::vector<int> {};
        bool ok = c.ok && c.kind == "IR FILE" && l.size() == 2 && rgt.size() == 2;
        float g1 = 0.0f, g2 = 0.0f, g3 = 0.0f;
        if (ok)
        {
            const float l0 = c.ir.getSample (0, l[0]);
            g1 = c.ir.getSample (0, l[1]) / l0; g2 = c.ir.getSample (1, rgt[0]) / l0; g3 = c.ir.getSample (1, rgt[1]) / l0;
            ok = l[1] - l[0] == 3840 && rgt[0] - l[0] == 72 && rgt[1] - l[0] == 7200
              && std::abs (g1 / 0.8f - 1.0f) < 0.02f && std::abs (g2 / -0.6f - 1.0f) < 0.02f && std::abs (g3 / 0.9f - 1.0f) < 0.02f;
        }
        std::printf ("   a sparse stereo IR file: %s, the left's arrivals %d apart, the right's %d and %d after the left's first; levels %.3f %.3f %.3f (want 0.800 -0.600 0.900)\n",
                     c.ok ? c.kind.toRawUTF8() : c.error.toRawUTF8(), l.size() == 2 ? l[1] - l[0] : 0, rgt.size() == 2 && ! l.empty() ? rgt[0] - l[0] : 0, rgt.size() == 2 && ! l.empty() ? rgt[1] - l[0] : 0, g1, g2, g3);
        CHECK (ok, "IR import: a sparse stereo IR file not kept as it is");
    }
    // 12. A clap recording in -65 dBFS noise, a second clap 0.25 s after the first (its tail still over
    //     the noise) or 1.2 s after (the tail down in it): AUTO and IMPORT CLAP end before the second
    //     clap and name the cut only where it cut the tail; IMPORT IR FILE keeps both claps.
    for (double gap : { 0.25, 1.2 })
    {
        std::mt19937 rng (5);
        std::normal_distribution<float> g (0.0f, 1.0f);
        juce::AudioBuffer<float> b (1, (int) (3.0 * sr));
        for (int i = 0; i < b.getNumSamples(); ++i) b.setSample (0, i, 5.6e-4f * g (rng));
        for (double t : { 0.5, 0.5 + gap })
            for (int i = 0; i < (int) (1.2 * sr) && (int) (t * sr) + i < b.getNumSamples(); ++i)
                b.addSample (0, (int) (t * sr) + i, 0.1f * g (rng) * std::exp (-6.9078f * (float) i / (float) sr / 0.5f));
        std::printf ("   two claps %.2f s apart in noise:", gap);
        for (auto src : { ir::Source::Auto, ir::Source::Clap, ir::Source::IrFile })
        {
            const auto c = ir::analyse (b, sr, src);
            const double len = c.ok ? c.ir.getNumSamples() / sr : 0.0;
            const bool asClap = src != ir::Source::IrFile;
            // The second clap in the IR: its 10 ms against the first's.
            double second = -300.0;
            if (c.ok && (int) ((gap + 0.011) * sr) < c.ir.getNumSamples())
            {
                const int p = peakIndex (c.ir.getReadPointer (0), c.ir.getNumSamples());
                auto e10 = [&] (int at) { double e = 0.0; for (int i = at; i < std::min (at + (int) (0.01 * sr), c.ir.getNumSamples()); ++i) e += (double) c.ir.getSample (0, i) * c.ir.getSample (0, i); return 10.0 * std::log10 (e + 1e-30); };
                second = e10 (p + (int) (gap * sr)) - e10 (p);
            }
            std::printf (" %s %s, %.2f s%s%s;", src == ir::Source::Auto ? "AUTO:" : src == ir::Source::Clap ? "CLAP:" : "IR FILE:", c.ok ? c.kind.toRawUTF8() : c.error.toRawUTF8(), len,
                         c.note.isNotEmpty() ? ", " : "", c.note.toRawUTF8());
            const bool ok = asClap ? (c.ok && c.kind == "CLAP" && len < gap + 0.01 && (gap < 0.5 ? c.note.startsWith ("CUT AT 0.7") : c.note.isEmpty()))
                                   : (c.ok && c.kind == "IR FILE" && c.note.isEmpty() && len > gap + 0.2 && second > -3.0);
            CHECK (ok, "IR import: two claps %.2f s apart, %s: %s %.2f s, note '%s', the second clap %.1f dB", gap, src == ir::Source::Auto ? "AUTO" : src == ir::Source::Clap ? "CLAP" : "IR FILE",
                   c.ok ? c.kind.toRawUTF8() : c.error.toRawUTF8(), len, c.note.toRawUTF8(), second);
        }
        std::printf ("\n");
    }
    // 13. SF CAPTURE's take (352df3c audit, A10): the sweep file and 3 s more, through a known transfer
    //     (four one-pole low-passes), analysed as the in-set import does (KnownSweep). Broad (5 kHz) and
    //     narrow (500 Hz): a sweep, its response the transfer's (at 200 Hz-10 kHz, where it is within
    //     40 dB of its top, to 1 dB: the sweep's flatness, as in testTerroir); stereo with one channel
    //     narrow (each its own, and the right's delay against the left's), inverted, or silent (left
    //     out, said, mono); noise 44 dB under the sweep; clipped (the sweep still, its shape squashed:
    //     SF CAPTURE says when the mics clip, testCapture). No sweep (silence, noise, a click): the
    //     take fails, and says so; never an IR of something else.
    {
        const auto file = ir::sweepFile (sr);
        const int total = file.getNumSamples() + (int) (3.0 * sr);
        auto lowpass = [&] (std::vector<float> x, double fc) { const float k = 1.0f - (float) std::exp (-2.0 * 3.141592653589793 * fc / sr); for (int st = 0; st < 4; ++st) { float y = 0.0f; for (auto& v : x) { y += k * (v - y); v = y; } } return x; };
        auto through = [&] (double fc, float sign = 1.0f) { std::vector<float> x ((size_t) total, 0.0f); std::copy (file.getReadPointer (0), file.getReadPointer (0) + file.getNumSamples(), x.begin()); x = lowpass (x, fc); for (auto& v : x) v *= sign; return x; };
        // The transfer's IR, and as every import treats it (high-passed at 20 Hz: ir::finish).
        auto truth = [&] (double fc)
        {
            std::vector<float> x ((size_t) (0.5 * sr), 0.0f); x[0] = 1.0f;
            x = lowpass (x, fc);
            const double wc = 2.0 * 3.141592653589793 * 20.0 / sr, cs = std::cos (wc), al = std::sin (wc) / (2.0 * 0.70710678);
            const double a0 = 1.0 + al, b0 = (1.0 + cs) / 2.0 / a0, b1 = -(1.0 + cs) / a0, b2 = b0, a1 = -2.0 * cs / a0, a2 = (1.0 - al) / a0;
            double z1 = 0.0, z2 = 0.0;
            for (auto& v : x) { const double in = v, y = b0 * in + z1; z1 = b1 * in - a1 * y + z2; z2 = b2 * in - a2 * y; v = (float) y; }
            return x;
        };
        auto recording = [&] (const std::vector<float>& l, const std::vector<float>* r) { juce::AudioBuffer<float> b (r != nullptr ? 2 : 1, total); b.copyFrom (0, 0, l.data(), total); if (r != nullptr) b.copyFrom (1, 0, r->data(), total); return b; };
        // The response's shape against the transfer's: dB at f less dB at 200 Hz, both; the largest difference.
        auto shapeOff = [&] (const juce::AudioBuffer<float>& got, int ch, double fc)
        {
            const auto t = truth (fc);
            juce::AudioBuffer<float> tb (1, (int) t.size()); tb.copyFrom (0, 0, t.data(), (int) t.size());
            const double t0 = dtft (tb, 0, 200.0), g0 = dtft (got, ch, 200.0);
            double worst = 0.0;
            for (double f : { 500.0, 1000.0, 2000.0, 5000.0, 10000.0 })
            {
                const double tf = dtft (tb, 0, f) - t0;
                if (tf < -40.0) continue;
                worst = std::max (worst, std::abs ((dtft (got, ch, f) - g0) - tf));
            }
            return worst;
        };
        const auto broad = through (5000.0), narrow = through (500.0), inverted = through (5000.0, -1.0f);
        const std::vector<float> silent ((size_t) total, 0.0f);
        struct Take { const char* name; juce::AudioBuffer<float> rec; std::vector<double> fcs; double tol; };
        std::vector<Take> takes;
        takes.push_back ({ "broad (5 kHz)", recording (broad, nullptr), { 5000.0 }, 1.0 });
        takes.push_back ({ "narrow (500 Hz)", recording (narrow, nullptr), { 500.0 }, 1.0 });
        takes.push_back ({ "the left broad, the right narrow", recording (broad, &narrow), { 5000.0, 500.0 }, 1.0 });
        takes.push_back ({ "the right inverted", recording (broad, &inverted), { 5000.0, 5000.0 }, 1.0 });
        takes.push_back ({ "the right silent", recording (broad, &silent), { 5000.0 }, 1.0 });
        {
            std::mt19937 rng (9);
            std::normal_distribution<float> g (0.0f, 1.0f);
            auto noisy = recording (broad, nullptr);
            for (int i = 0; i < total; ++i) noisy.addSample (0, i, 3.2e-3f * g (rng));   // -50 dBFS
            takes.push_back ({ "noise 44 dB under the sweep", std::move (noisy), { 5000.0 }, 1.0 });
            auto clipped = recording (broad, nullptr);
            for (int i = 0; i < total; ++i) clipped.setSample (0, i, juce::jlimit (-0.2f, 0.2f, clipped.getSample (0, i)));
            takes.push_back ({ "clipped at -14 dBFS", std::move (clipped), { 5000.0 }, 100.0 });   // still the sweep; its shape squashed (SF CAPTURE says the mics clipped: testCapture)
        }
        for (const auto& t : takes)
        {
            const auto c = ir::analyse (t.rec, sr, ir::Source::KnownSweep);
            const bool mono = t.fcs.size() == 1;
            double off = 0.0;
            for (int ch = 0; c.ok && ch < (int) t.fcs.size() && ch < c.ir.getNumChannels(); ++ch) off = std::max (off, shapeOff (c.ir, ch, t.fcs[(size_t) ch]));
            juce::String extra;
            bool ok = c.ok && c.kind == "SWEEP x3" && c.ir.getNumChannels() == (int) t.fcs.size() && off < t.tol;
            if (std::string (t.name) == "the right silent") ok = ok && c.note == "NO SWEEP IN THE RIGHT CHANNEL: MONO, FROM THE LEFT";
            else ok = ok && c.note.isEmpty();
            if (ok && ! mono && t.fcs[1] != t.fcs[0])
            {
                // The narrow right channel peaks later than the broad left, as its transfer does.
                const auto tl = truth (t.fcs[0]), tr = truth (t.fcs[1]);
                const int want = peakIndex (tr.data(), (int) tr.size()) - peakIndex (tl.data(), (int) tl.size());
                const int got = peakIndex (c.ir.getReadPointer (1), c.ir.getNumSamples()) - peakIndex (c.ir.getReadPointer (0), c.ir.getNumSamples());
                extra << ", the right peaks " << got << " samples after the left (the transfers: " << want << ")";
                ok = ok && std::abs (got - want) <= 1;
            }
            if (ok && ! mono && t.fcs[1] == t.fcs[0])
            {
                double xy = 0.0, xx = 0.0, yy = 0.0;
                for (int i = 0; i < c.ir.getNumSamples(); ++i) { const double x = c.ir.getSample (0, i), y = c.ir.getSample (1, i); xy += x * y; xx += x * x; yy += y * y; }
                const double corr = xy / std::sqrt (xx * yy + 1e-30);
                extra << ", left against right " << juce::String (corr, 4);
                ok = ok && corr < -0.999;
            }
            std::printf ("   SF CAPTURE's take, %s: %s, %d channel(s), the response %.2f dB off the transfer's%s%s%s\n", t.name, c.ok ? c.kind.toRawUTF8() : c.error.toRawUTF8(),
                         c.ok ? c.ir.getNumChannels() : 0, off, extra.toRawUTF8(), c.note.isNotEmpty() ? "; " : "", c.note.toRawUTF8());
            CHECK (ok, "IR import: SF CAPTURE's take, %s: %s, %.2f dB off, note '%s'", t.name, c.ok ? c.kind.toRawUTF8() : c.error.toRawUTF8(), off, c.note.toRawUTF8());
        }
        // The narrow take read as a file: AUTO finds it too (the repeats); one sweep of it is a sweep
        // only when IMPORT says SWEEP (AUTO can't tell it from a clap: the limit, said in the manual).
        {
            const auto a = ir::analyse (recording (narrow, nullptr), sr, ir::Source::Auto);
            juce::AudioBuffer<float> one (1, (int) ((ir::kLeadSec + ir::kSweepSec + 3.0) * sr));
            one.copyFrom (0, 0, narrow.data(), one.getNumSamples());
            const auto s1 = ir::analyse (one, sr, ir::Source::Sweep), a1 = ir::analyse (one, sr, ir::Source::Auto);
            std::printf ("   the narrow take as a file: AUTO %s; one sweep of it: IMPORT SWEEP %s, AUTO %s\n", a.ok ? a.kind.toRawUTF8() : a.error.toRawUTF8(),
                         s1.ok ? s1.kind.toRawUTF8() : s1.error.toRawUTF8(), a1.ok ? a1.kind.toRawUTF8() : a1.error.toRawUTF8());
            CHECK (a.ok && a.kind == "SWEEP x3" && s1.ok && s1.kind == "SWEEP x1" && shapeOff (s1.ir, 0, 500.0) < 1.0, "IR import: the narrow sweep as a file (AUTO %s, IMPORT SWEEP %s)",
                   a.ok ? a.kind.toRawUTF8() : a.error.toRawUTF8(), s1.ok ? s1.kind.toRawUTF8() : s1.error.toRawUTF8());
        }
        // No sweep in the take.
        {
            std::mt19937 rng (13);
            std::normal_distribution<float> g (0.0f, 1.0f);
            juce::AudioBuffer<float> quiet (2, total), noise (2, total), click (2, total);
            quiet.clear(); click.clear();
            for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < total; ++i) noise.setSample (ch, i, 3.2e-3f * g (rng));
            click.setSample (0, (int) (12.5 * sr), 0.5f); click.setSample (1, (int) (12.5 * sr) + 30, 0.4f);
            for (auto* b : { &quiet, &noise, &click })
            {
                const auto c = ir::analyse (*b, sr, ir::Source::KnownSweep);
                const char* what = b == &quiet ? "silence" : b == &noise ? "noise" : "a click";
                std::printf ("   SF CAPTURE's take of %s: %s\n", what, c.ok ? c.kind.toRawUTF8() : c.error.toRawUTF8());
                CHECK (! c.ok && c.error == "THE SWEEP DIDN'T COME BACK (THE SPEAKER, THE MICS, THE ROUTING?)", "IR import: SF CAPTURE's take of %s: %s", what, c.ok ? c.kind.toRawUTF8() : c.error.toRawUTF8());
            }
            const auto c = ir::analyse (click, sr);
            std::printf ("   the click as a file (AUTO): %s\n", c.ok ? c.kind.toRawUTF8() : c.error.toRawUTF8());
            CHECK (c.ok && c.kind == "IR FILE", "IR import: a click in 46 s of silence read as %s", c.ok ? c.kind.toRawUTF8() : c.error.toRawUTF8());
        }
    }
}

// In-set capture (SF CAPTURE on the room mics' track, SF TERROIR's CAPTURE): the sweep out of SF
// CAPTURE, through a room heard by two mics (the right 72 samples later, 3 dB down), -70 dB noise and
// the round trip, back into its input; the take written, then analysed into TERROIR's slot 4 with its
// shape and the mics' timing. And what must not happen: the mics passed through, a take while the
// transport plays or with no SF CAPTURE, a stop that clicks, a take that runs on when the transport starts.
static void testCapture()
{
    std::printf ("-- In-set capture: SF CAPTURE plays the sweep and records, SF TERROIR analyses the take\n");
    const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("sf_capture_test");
    dir.deleteRecursively();
    dir.createDirectory();
    capture::testFolder = dir.getFullPathName().toStdString();
    resetBus();
    auto& d = Bus::get().d();
    // The room, as the two mics hear it.
    const auto room = roomIr (kSr, 0.6f, 21);
    std::vector<float> irL (room.begin(), room.begin() + (int) (0.9 * kSr)), irR (irL.size(), 0.0f);
    for (size_t i = 72; i < irR.size(); ++i) irR[i] = 0.708f * irL[i - 72];
    Convolver air;
    air.prepare ((int) irL.size());
    air.offer (makeIrFilter (irL.data(), irR.data(), (int) irL.size()).release());

    Rig2 terr ("TERROIR");
    auto capOwner = std::make_unique<Rig2> ("CAPTURE");
    Rig2* cap = capOwner.get();
    auto cs = [&] { return capture::status (d.capStatus); };
    terr.head.playing = cap->head.playing = false;
    // TERROIR's display as the editor's strip lays it out (280 wide): the slots, CAPTURE under them.
    const juce::Rectangle<float> disp (0.0f, 0.0f, 280.0f, 184.0f);
    auto click = [&] (juce::Point<float> at) { terr.p->mod().displayMouse (at, disp, true); terr.p->mod().displayMouseUp(); };
    auto slotX = [] (int i) { const float w = (280.0f - 16.0f - 21.0f) / 8.0f; return juce::Point<float> (8.0f + (float) i * (w + 3.0f) + w * 0.5f, 25.0f); };
    const juce::Point<float> captureX (73.0f, 47.0f);
    std::mt19937 rng (5);
    std::normal_distribution<float> g (0.0f, 1.0f);
    std::vector<float> backL ((size_t) kBlock, 0.0f), backR ((size_t) kBlock, 0.0f), mono ((size_t) kBlock, 0.0f);
    float outPeak = 0.0f, micGain = 1.0f;          // micGain over 1: the mics hotter, clipped at the converter's full scale
    std::vector<float> lastOut;
    // One block: the mics hear what the speaker played (the engine's 512 samples and a block late).
    auto step = [&] (float extra = 0.0f)
    {
        cap->step (kBlock, [&] (juce::AudioBuffer<float>& b, juce::MidiBuffer&, int n)
        {
            for (int i = 0; i < n; ++i)
            {
                b.setSample (0, i, backL[(size_t) i] + g (rng) * 3.0e-4f + extra);
                b.setSample (1, i, backR[(size_t) i] + g (rng) * 3.0e-4f + extra);
                if (micGain != 1.0f)
                    for (int ch = 0; ch < 2; ++ch) b.setSample (ch, i, juce::jlimit (-1.0f, 1.0f, micGain * b.getSample (ch, i)));
            }
        });
        lastOut.assign (cap->buf.getReadPointer (0), cap->buf.getReadPointer (0) + kBlock);
        for (int i = 0; i < kBlock; ++i) { mono[(size_t) i] = cap->buf.getSample (0, i); outPeak = std::max (outPeak, std::fabs (mono[(size_t) i])); }
        air.process (mono.data(), backL.data(), backR.data(), kBlock);
        terr.step (kBlock, nullptr);
    };
    auto waitState = [&] (int want, double sec)
    {
        for (const double until = juce::Time::getMillisecondCounterHiRes() + sec * 1000.0; juce::Time::getMillisecondCounterHiRes() < until;)
        {
            if (cs().state == want) return true;
            step();
            juce::Thread::sleep (1);
        }
        return cs().state == want;
    };
    auto placeIn = [&] (int slot, juce::String& kind, juce::String& name, juce::MemoryBlock& flac, double& srHz)
    {
        juce::MemoryBlock st;
        terr.p->getStateInformation (st);
        const auto m = juce::ValueTree::readFromData (st.getData(), st.getSize()).getChildWithName ("MODULE");
        for (int i = 0; i < m.getNumChildren(); ++i)
            if (m.getChild (i).hasType ("PLACE") && (int) m.getChild (i).getProperty ("slot") == slot)
            {
                kind = m.getChild (i).getProperty ("kind").toString();
                name = m.getChild (i).getProperty ("name").toString();
                srHz = (double) m.getChild (i).getProperty ("sr");
                if (auto* mb = m.getChild (i).getProperty ("flac").getBinaryData()) flac = *mb;
                return true;
            }
        return false;
    };
    for (int b = 0; b < 40; ++b) step();

    // 1. It never passes the mics: a loud input, nothing out.
    outPeak = 0.0f;
    for (int b = 0; b < 20; ++b) step (0.5f);
    std::printf ("   idle, the mics at -6 dBFS: out %.1f dBFS\n", gainToDb (outPeak));
    CHECK (outPeak == 0.0f, "SF CAPTURE passes its input (%.1f dBFS)", gainToDb (outPeak));

    // 2. Refused: the transport playing; no SF CAPTURE.
    const uint32_t seq0 = d.capSeq.load();
    terr.head.playing = true; step();
    click (captureX); click (slotX (3));
    terr.head.playing = false; step();
    const bool refusedPlaying = d.capSeq.load() == seq0;
    d.captureSeenMs = 0;
    click (captureX); click (slotX (3));
    const bool refusedAbsent = d.capSeq.load() == seq0;
    step();
    std::printf ("   asked while the transport plays: %s; with no SF CAPTURE: %s\n", refusedPlaying ? "refused" : "TAKEN", refusedAbsent ? "refused" : "TAKEN");
    CHECK (refusedPlaying && refusedAbsent, "TERROIR asks for a capture it must not");

    // 3. A take into slot 4: armed, slot clicked; the whole sweep and the tail; written; analysed.
    outPeak = 0.0f;
    click (captureX); click (slotX (3));
    const bool asked = d.capSeq.load() == seq0 + 1;
    const bool playing = waitState (capture::Playing, 5.0);
    const int blocks = (int) ((ir::kLeadSec + ir::kRepeats * (ir::kSweepSec + ir::kGapSec) + capture::kTailSec) * kSr / kBlock) + 2;
    for (int b = 0; b < blocks && cs().state == capture::Playing; ++b) step();
    const bool done = waitState (capture::Done, 60.0);
    // Written (Done), then SF CAPTURE removed before or while SF TERROIR takes the take in: it stays
    // Done, and the place still comes in (a recorder's removal never loses a finished take).
    capOwner.reset(); cap = nullptr;
    const bool doneStays = cs().state == capture::Done;
    capOwner = std::make_unique<Rig2> ("CAPTURE");
    cap = capOwner.get();
    cap->head.playing = false;
    const auto take = capture::fileFor (d.capStamp.load());
    int takeCh = 0; double takeSec = 0.0;
    {
        juce::AudioFormatManager fm;
        fm.registerBasicFormats();
        if (std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (take)); r != nullptr) { takeCh = (int) r->numChannels; takeSec = (double) r->lengthInSamples / r->sampleRate; }
    }
    juce::String kind, name;
    juce::MemoryBlock flac;
    double placeSr = 0.0;
    bool stored = false;
    for (const double until = juce::Time::getMillisecondCounterHiRes() + 180000.0; ! stored && juce::Time::getMillisecondCounterHiRes() < until;)
    {
        juce::Thread::sleep (50);
        step();
        stored = placeIn (3, kind, name, flac, placeSr);
    }
    double corrL = 0.0, corrR = 0.0;
    int lag = 0, chans = 0;
    if (stored)
    {
        juce::FlacAudioFormat fmt;
        std::unique_ptr<juce::AudioFormatReader> r (fmt.createReaderFor (new juce::MemoryInputStream (flac, false), true));
        if (r != nullptr)
        {
            chans = (int) r->numChannels;
            juce::AudioBuffer<float> b ((int) r->numChannels, (int) r->lengthInSamples);
            r->read (&b, 0, (int) r->lengthInSamples, 0, true, true);
            if (chans == 2)
            {
                const int pl = peakIndex (b.getReadPointer (0), b.getNumSamples()), pr = peakIndex (b.getReadPointer (1), b.getNumSamples());
                const int ph = peakIndex (irL.data(), (int) irL.size());
                lag = pr - pl;
                const int pre = (int) (0.002 * kSr), n = std::min ({ (int) (0.5 * kSr), b.getNumSamples() - pl - 200, (int) irL.size() - ph - 200 });
                corrL = bandCorrelation (b.getReadPointer (0) + pl - pre, irL.data() + ph - pre, n, kSr);
                corrR = bandCorrelation (b.getReadPointer (1) + pr - pre, irR.data() + ph + 72 - pre, n, kSr);
            }
        }
    }
    std::printf ("   CAPTURE armed, slot 4 clicked: asked %s, played %s (peak %.2f dBFS at SWEEP LEVEL -6), written %s (%d channels, %.1f s)\n",
                 asked ? "yes" : "NO", playing ? "yes" : "NO", gainToDb (outPeak), done ? "yes" : "NO", takeCh, takeSec);
    std::printf ("   slot 4: %s \"%s\", %.0f Hz, %d channels; against the room: left %.4f, right %.4f; right %d samples later (want 72)\n",
                 stored ? kind.toRawUTF8() : "NOT STORED", name.toRawUTF8(), placeSr, chans, corrL, corrR, lag);
    std::printf ("   SF CAPTURE removed once the take was written: %s\n", doneStays ? "still Done, taken in" : "NOT DONE");
    CHECK (asked && playing && done && doneStays && std::abs (gainToDb (outPeak) + 6.0f) < 0.1f && takeCh == 2 && std::abs (takeSec - 46.0) < 0.01,
           "SF CAPTURE take wrong");
    CHECK (stored && kind == "SWEEP x3" && name.startsWith ("ROOM ") && chans == 2 && corrL > 0.99 && corrR > 0.99 && lag == 72,
           "capture into TERROIR's slot 4 wrong");

    // 3b. A take with the mics clipping (18 dB hotter, at the converter's full scale; 352df3c audit,
    //     A10): still the sweep, into slot 3, and the status line says the mics clipped. (Its file is
    //     then removed: the takes counted below are the cases'.)
    {
        juce::Thread::sleep (100);
        step();
        micGain = 8.0f;
        click (captureX); click (slotX (2));
        const bool playing5 = waitState (capture::Playing, 5.0);
        for (int b = 0; b < blocks && cs().state == capture::Playing; ++b) step();
        const float peak5 = d.capPeak.load();
        const bool done5 = waitState (capture::Done, 60.0);
        micGain = 1.0f;
        juce::String kind5, name5, status;
        juce::MemoryBlock flac5;
        double sr5 = 0.0;
        bool stored5 = false;
        for (const double until = juce::Time::getMillisecondCounterHiRes() + 180000.0; ! stored5 && juce::Time::getMillisecondCounterHiRes() < until;)
        {
            juce::Thread::sleep (50);
            step();
            stored5 = placeIn (2, kind5, name5, flac5, sr5);
        }
        for (int k = 0; k < 200 && ! status.startsWith ("SLOT 3"); ++k) { juce::Thread::sleep (10); step(); status = terr.p->mod().statusLine(); }
        capture::fileFor (d.capStamp.load()).deleteFile();
        std::printf ("   a take with the mics clipping (their peak %.2f): slot 3 %s; the status line: %s\n", peak5, stored5 ? kind5.toRawUTF8() : "NOT STORED", status.toRawUTF8());
        CHECK (playing5 && done5 && peak5 >= 0.99f && stored5 && kind5 == "SWEEP x3" && status.contains ("THE MICS CLIPPED: TAKE IT AGAIN, SPEAKER OR GAIN DOWN"),
               "a clipped capture: slot 3 %s, status '%s'", stored5 ? kind5.toRawUTF8() : "not stored", status.toRawUTF8());
    }

    // 4. STOP (CAPTURE again) two seconds in: the sweep fades out over 10 ms, nothing is stored.
    auto stopTest = [&] (bool byTransport, int slot)
    {
        juce::Thread::sleep (100);                       // TERROIR's worker has seen the last take end (a hand is slower)
        step();
        click (captureX); click (slotX (slot));
        if (! waitState (capture::Playing, 5.0)) return std::make_pair (false, 0);
        for (int b = 0; b < (int) (2.5 * kSr / kBlock); ++b) step();
        if (byTransport) cap->head.playing = true; else click (captureX);
        std::vector<float> o;
        for (int b = 0; b < 8; ++b) { step(); o.insert (o.end(), lastOut.begin(), lastOut.end()); }
        cap->head.playing = false;
        int last = 0;
        for (int i = 0; i < (int) o.size(); ++i) if (o[(size_t) i] != 0.0f) last = i;
        const bool stopped = cs().state == capture::Failed && cs().error == (byTransport ? capture::TransportStarted : capture::Cancelled);
        return std::make_pair (stopped, last);
    };
    const auto [stopOk, stopLen] = stopTest (false, 4);
    const auto [transportOk, transportLen] = stopTest (true, 5);
    juce::Thread::sleep (300);
    for (int b = 0; b < 4; ++b) step();
    juce::String k2, n2; juce::MemoryBlock f2; double s2 = 0.0;
    const bool nothing = ! placeIn (4, k2, n2, f2, s2) && ! placeIn (5, k2, n2, f2, s2);
    std::printf ("   STOP 2.5 s in: %s, silent after %d samples; the transport starting: %s, silent after %d; neither stored: %s\n",
                 stopOk ? "stopped" : "NOT STOPPED", stopLen, transportOk ? "stopped" : "NOT STOPPED", transportLen, nothing ? "yes" : "NO");
    CHECK (stopOk && transportOk && stopLen < 2 * kBlock && transportLen < 2 * kBlock && nothing, "capture stop wrong");

    // 5. Live prepares SF CAPTURE again (a new rate or buffer size) after it claimed a take and
    //    before the take starts: the take ends (Cancelled) instead of the bus saying Preparing for
    //    good, which would refuse every later capture; the next take starts.
    auto failedWith = [&] (int err) { return cs().state == capture::Failed && cs().error == err; };
    juce::Thread::sleep (100);
    step();
    click (captureX); click (slotX (6));
    bool preparing = false;
    for (int k = 0; k < 500 && ! preparing; ++k) { juce::Thread::sleep (2); preparing = cs().state == capture::Preparing; }
    juce::Thread::sleep (500);                           // the take built and waiting for a block (no block runs meanwhile)
    cap->p->prepareToPlay (kSr, kBlock);
    const bool preparedEnds = preparing && failedWith (capture::Cancelled);
    juce::Thread::sleep (100);
    step();
    click (captureX); click (slotX (7));
    const bool restarted = waitState (capture::Playing, 5.0);
    click (captureX);
    for (int b = 0; b < 4; ++b) step();
    const bool restartStops = failedWith (capture::Cancelled);

    // 6. PANIC held on SF CONTROL: a take asked for meanwhile never starts (Cancelled); a take
    //    playing when PANIC comes stops (Cancelled). Nothing is written or stored.
    auto panic = [&] (bool on) { d.controlSeenMs = Bus::nowMs(); d.panicHeld = on ? 1 : 0; };
    juce::Thread::sleep (100);
    panic (true); step();
    click (captureX); click (slotX (6));
    bool panicRefused = false;
    for (const double until = juce::Time::getMillisecondCounterHiRes() + 3000.0; ! panicRefused && juce::Time::getMillisecondCounterHiRes() < until;)
    {
        panic (true); step(); juce::Thread::sleep (2);
        panicRefused = failedWith (capture::Cancelled) && cs().seq == d.capSeq.load();
    }
    panic (false);
    for (int b = 0; b < 4; ++b) step();
    juce::Thread::sleep (100);
    step();
    click (captureX); click (slotX (7));
    const bool panicPlays = waitState (capture::Playing, 5.0);
    for (int b = 0; b < (int) (1.0 * kSr / kBlock); ++b) step();
    bool panicStops = false;
    for (int b = 0; b < 8 && ! panicStops; ++b) { panic (true); step(); panicStops = failedWith (capture::Cancelled); }
    panic (false);
    juce::Thread::sleep (300);
    for (int b = 0; b < 4; ++b) step();
    const bool none67 = ! placeIn (6, k2, n2, f2, s2) && ! placeIn (7, k2, n2, f2, s2);
    int files = 0;
    for (const auto& e : juce::RangedDirectoryIterator (dir, false, "*.wav")) { juce::ignoreUnused (e); ++files; }
    std::printf ("   prepared again after the claim: %s; the next take %s and stops %s; PANIC held: a take asked %s, a take playing %s; stored %s, takes written %d (want 1)\n",
                 preparedEnds ? "ended (Cancelled)" : "STUCK", restarted ? "plays" : "DOESN'T PLAY", restartStops ? "yes" : "NO",
                 panicRefused ? "never starts" : "STARTS", panicPlays && panicStops ? "stops" : "DOESN'T STOP", none67 ? "nothing" : "SOMETHING", files);
    CHECK (preparedEnds && restarted && restartStops && panicRefused && panicPlays && panicStops && none67 && files == 1, "capture lifecycle wrong");

    for (auto* f : air.releaseAll()) delete f;
    capture::testFolder.clear();
    dir.deleteRecursively();
    resetBus();
}

// A take's lifecycle (Nov 5 audit 2: A03, A07). The request is one thing from the ask to the
// import: STOP holds whatever stage it has reached (before SF CAPTURE claims it, while the take is
// being made, made but not yet started, playing, being written); PANIC or a new stream (prepare)
// while it is being made ends it; a take STOPped before it started plays nothing, writes nothing
// and fills no slot. The recorder removed while the take is being made or playing ends it (and a
// new SF CAPTURE and SF TERROIR capture again); a recorder whose audio stands still loses it after
// capture::kLeaseMs, and its take never sounds again. Held at fixed points by capture::testHold;
// a 0.5 s tone for a take (capture::testShortTake).
static void testCaptureLifecycle()
{
    std::printf ("-- A take's lifecycle: STOP at every stage, PANIC and prepare while it is made, the recorder removed or standing still\n");
    const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("sf_capture_lifecycle");
    const juce::Rectangle<float> disp (0.0f, 0.0f, 280.0f, 184.0f);
    const juce::Point<float> captureX (73.0f, 47.0f);           // as the strip lays out the display (280 wide)
    auto slotX = [] (int i) { const float w = (280.0f - 16.0f - 21.0f) / 8.0f; return juce::Point<float> (8.0f + (float) i * (w + 3.0f) + w * 0.5f, 25.0f); };
    auto& d = Bus::get().d();
    auto cs = [&] { return capture::status (d.capStatus); };
    auto files = [&] { int n = 0; for (const auto& e : juce::RangedDirectoryIterator (dir, false, "*.wav")) { juce::ignoreUnused (e); ++n; } return n; };
    auto slotFilled = [] (Rig2& terr, int slot)
    {
        juce::MemoryBlock st;
        terr.p->getStateInformation (st);
        const auto m = juce::ValueTree::readFromData (st.getData(), st.getSize()).getChildWithName ("MODULE");
        for (int i = 0; i < m.getNumChildren(); ++i)
            if (m.getChild (i).hasType ("PLACE") && (int) m.getChild (i).getProperty ("slot") == slot) return true;
        return false;
    };
    struct World
    {
        std::unique_ptr<Rig2> terr, cap;
        float peak = 0.0f;
        bool capRuns = true;
        World() : terr (std::make_unique<Rig2> ("TERROIR")), cap (std::make_unique<Rig2> ("CAPTURE")) { terr->head.playing = cap->head.playing = false; }
        void click (juce::Point<float> at)
        {
            const juce::Rectangle<float> r (0.0f, 0.0f, 280.0f, 184.0f);
            terr->p->mod().displayMouse (at, r, true); terr->p->mod().displayMouseUp();
        }
        void step()
        {
            if (cap != nullptr && capRuns)
            {
                cap->step (kBlock, nullptr);
                peak = std::max (peak, cap->buf.getMagnitude (0, 0, kBlock));
            }
            if (terr != nullptr) terr->step (kBlock, nullptr);
        }
    };
    juce::ignoreUnused (disp);
    auto until = [&] (World& w, const std::function<bool()>& f, double sec, bool audio = true)
    {
        for (const double end = juce::Time::getMillisecondCounterHiRes() + sec * 1000.0; juce::Time::getMillisecondCounterHiRes() < end;)
        {
            if (f()) return true;
            if (audio) w.step();
            juce::Thread::sleep (1);
        }
        return f();
    };
    auto ask = [&] (World& w, int slot) { w.click (captureX); w.click (slotX (slot)); return d.capSeq.load(); };
    auto failed = [&] (uint32_t seq, int err) { const auto c = cs(); return c.seq == seq && c.state == capture::Failed && c.error == err; };
    auto settle = [&] (World& w, int blocks) { for (int b = 0; b < blocks; ++b) { w.step(); if (b % 8 == 0) juce::Thread::sleep (2); } };

    // STOP, PANIC and prepare at each stage before the take starts: no sweep, no file, no slot.
    const char* what[] = { "STOP before SF CAPTURE claims it", "STOP while the take is being made", "STOP once made, before it starts",
                           "PANIC while the take is being made", "prepare while the take is being made" };
    for (int variant = 0; variant < 5; ++variant)
    {
        resetBus();
        dir.deleteRecursively(); dir.createDirectory();
        capture::testFolder = dir.getFullPathName().toStdString();
        capture::testShortTake = true;
        World w;
        settle (w, 4);
        capture::testHold = variant == 0 ? capture::kHoldClaim : variant == 2 ? 0 : capture::kHoldPublish;
        const uint32_t seq = ask (w, variant);
        bool reached = true;
        if (variant == 0) juce::Thread::sleep (50);                                  // the worker waits before the claim
        else reached = until (w, [&] { return cs().seq == seq && cs().state == capture::Preparing; }, 3.0, false);
        if (variant == 2) juce::Thread::sleep (300);                                 // made and handed over; no audio has run since
        if (variant <= 2) w.click (captureX);                                        // STOP
        if (variant == 3) w.cap->p->mod().reset();                                   // PANIC's reset
        if (variant == 4) w.cap->p->prepareToPlay (kSr, kBlock);
        capture::testHold = 0;
        settle (w, 120);
        const auto c = cs();
        const bool ended = failed (seq, capture::Cancelled);
        std::printf ("   %-38s %s; state %d error %d; sweep out %.1f dBFS, takes written %d, slot %s\n", what[variant], reached ? "reached" : "NOT REACHED",
                     c.state, c.error, gainToDb (w.peak), files(), slotFilled (*w.terr, variant) ? "FILLED" : "empty");
        CHECK (reached && ended && w.peak == 0.0f && files() == 0 && ! slotFilled (*w.terr, variant), "capture lifecycle (%s): state %d error %d, out %.1f dBFS", what[variant], c.state, c.error, gainToDb (w.peak));
    }

    // STOP while the take is being written: not written, not taken in.
    {
        resetBus();
        dir.deleteRecursively(); dir.createDirectory();
        capture::testFolder = dir.getFullPathName().toStdString();
        capture::testShortTake = true;
        World w;
        settle (w, 4);
        capture::testHold = capture::kHoldWrite;
        const uint32_t seq = ask (w, 0);
        const bool writing = until (w, [&] { return cs().seq == seq && cs().state == capture::Writing; }, 5.0);
        w.click (captureX);
        capture::testHold = 0;
        settle (w, 60);
        const bool ended = failed (seq, capture::Cancelled);
        std::printf ("   STOP while it is being written:        %s; %s, takes written %d, slot %s\n", writing ? "reached" : "NOT REACHED", ended ? "Cancelled" : "NOT CANCELLED", files(), slotFilled (*w.terr, 0) ? "FILLED" : "empty");
        CHECK (writing && ended && files() == 0 && ! slotFilled (*w.terr, 0), "STOP while writing");
    }

    // SF CAPTURE removed while the take is being made, or playing: the take ends (Removed); a new SF
    // CAPTURE and a new SF TERROIR capture again, and the old take never sounds.
    for (const bool playing : { false, true })
    {
        resetBus();
        dir.deleteRecursively(); dir.createDirectory();
        capture::testFolder = dir.getFullPathName().toStdString();
        capture::testShortTake = true;
        World w;
        settle (w, 4);
        capture::testHold = playing ? 0 : capture::kHoldPublish;
        const uint32_t seq = ask (w, 0);
        const bool reached = until (w, [&] { return cs().seq == seq && cs().state == (playing ? capture::Playing : capture::Preparing); }, 5.0, playing);
        w.cap.reset();
        const bool removed = failed (seq, capture::Removed);
        capture::testHold = 0;
        World w2;                                                              // a new SF CAPTURE and SF TERROIR
        settle (w2, 4);
        const uint32_t seq2 = ask (w2, 1);
        const bool again = until (w2, [&] { return cs().seq == seq2 && cs().state == capture::Playing; }, 5.0);
        w2.click (captureX);                                                   // STOP it
        settle (w2, 20);
        w.terr.reset();
        std::printf ("   SF CAPTURE removed while the take %-8s %s; a new one asked: %s, then STOPped: %s\n", playing ? "plays:" : "is made:", removed ? "Removed" : "NOT ENDED",
                     again ? "plays" : "DOESN'T PLAY", failed (seq2, capture::Cancelled) ? "yes" : "NO");
        CHECK (reached && removed && seq2 > seq && again && failed (seq2, capture::Cancelled), "recorder removed while %s", playing ? "playing" : "preparing");
    }

    // The recorder's audio stands still while its take plays (its track off, Live's engine stopped):
    // after capture::kLeaseMs SF TERROIR ends the take (Lost); when that audio runs again the take
    // never sounds, and the same SF CAPTURE takes a new request. Or the SF TERROIR that asked is gone
    // too, and a new one asks while a second SF CAPTURE runs: the stalled take is over (its recorder's
    // lease ran out; the other SF CAPTURE's audio doesn't keep it), the second plays the new one, and
    // the first, its audio back, never sounds the old take.
    for (const bool newTerroir : { false, true })
    {
        resetBus();
        dir.deleteRecursively(); dir.createDirectory();
        capture::testFolder = dir.getFullPathName().toStdString();
        capture::testShortTake = true;
        World w;
        settle (w, 4);
        const uint32_t seq = ask (w, 0);
        const bool started = until (w, [&] { return cs().seq == seq && cs().state == capture::Playing; }, 5.0);
        w.capRuns = false;                                                     // its audio stands still
        std::unique_ptr<World> other;
        bool lost = false, again = false;
        uint32_t seq2 = 0;
        if (! newTerroir)
            lost = until (w, [&] { return failed (seq, capture::Lost); }, 4.0);
        else
        {
            w.terr.reset();                                                    // the SF TERROIR that asked: gone
            other = std::make_unique<World>();                                 // a new SF TERROIR, a second SF CAPTURE
            const double t0 = juce::Time::getMillisecondCounterHiRes();
            while (juce::Time::getMillisecondCounterHiRes() - t0 < (double) capture::kLeaseMs + 300.0) { other->step(); juce::Thread::sleep (5); }
            seq2 = ask (*other, 1);
            lost = seq2 > seq;
            again = until (*other, [&] { return cs().seq == seq2 && cs().state == capture::Playing; }, 5.0);
        }
        w.peak = 0.0f;
        w.capRuns = true;                                                      // its audio runs again
        settle (w, 30);
        if (other != nullptr) settle (*other, 10);
        const float after = w.peak;
        if (! newTerroir)
        {
            seq2 = ask (w, 1);
            again = until (w, [&] { return cs().seq == seq2 && cs().state == capture::Playing; }, 5.0);
            w.click (captureX);
            settle (w, 20);
        }
        else { other->click (captureX); settle (*other, 20); }
        std::printf ("   its audio stood still while playing%s: %s; then out %.1f dBFS; a new take plays: %s\n", newTerroir ? ", a new SF TERROIR and SF CAPTURE" : "",
                     lost ? (newTerroir ? "taken over" : "Lost") : "STILL BUSY", gainToDb (after), again ? "yes" : "NO");
        CHECK (started && lost && after == 0.0f && again && failed (seq2, capture::Cancelled), "recorder standing still (%s)", newTerroir ? "new SF TERROIR" : "same SF TERROIR");
    }

    // ... and with no SF TERROIR left at all to end it: the recorder, its audio back after the lease,
    // ends the take itself (Lost) and never sounds it.
    {
        resetBus();
        dir.deleteRecursively(); dir.createDirectory();
        capture::testFolder = dir.getFullPathName().toStdString();
        capture::testShortTake = true;
        World w;
        settle (w, 4);
        const uint32_t seq = ask (w, 0);
        const bool started = until (w, [&] { return cs().seq == seq && cs().state == capture::Playing; }, 5.0);
        w.capRuns = false;
        w.terr.reset();
        juce::Thread::sleep ((int) capture::kLeaseMs + 300);
        w.peak = 0.0f;
        w.capRuns = true;
        settle (w, 30);
        std::printf ("   its audio stood still, no SF TERROIR left: %s; then out %.1f dBFS\n", failed (seq, capture::Lost) ? "Lost" : "STILL BUSY", gainToDb (w.peak));
        CHECK (started && failed (seq, capture::Lost) && w.peak == 0.0f, "recorder standing still with no SF TERROIR");
    }

    capture::testHold = 0;
    capture::testShortTake = false;
    capture::testFolder.clear();
    dir.deleteRecursively();
    resetBus();
}

// TERROIR's IR follows its knobs through every change of stream (Nov 5 audit, F4 F5): real time to
// rendering and back with an old answer from the worker waiting or still being built, prepare
// again, a fresh instance's first note, the state loaded before or after prepare, a change of rate;
// measured from the first sample, not after a warm-up. And the level is the same at every host rate,
// for a place stored at 48 or 96 kHz.
static void testTerroirLifecycle()
{
    std::printf ("-- TERROIR lifecycle: the IR in use is the knobs' (render, real time, prepare, state); one level at every rate\n");
    auto setup = [] (Rig2& r, bool offline)
    {
        r.head.bpm = 120.0;
        r.p->setNonRealtime (offline);
        r.set ("mix", 1.0f); r.set ("dry", 0.0f); r.set ("duck", 0.0f); r.set ("lo_cut", 20.0f);
    };
    // sec seconds of input (0 silence, 1 an impulse at the first sample, 2 a 0.05 sine at hz); the left output.
    auto run = [] (Rig2& r, double sec, int input, double hz = 1000.0)
    {
        std::vector<float> y;
        const int64_t n = (int64_t) (sec * r.sr);
        for (int64_t t = 0; t < n;)
        {
            const int nn = (int) std::min<int64_t> (r.block, n - t);
            r.step (nn, [&] (juce::AudioBuffer<float>& b, juce::MidiBuffer&, int m)
            {
                for (int i = 0; i < m; ++i)
                {
                    float x = 0.0f;
                    if (input == 1 && t + i == 0) x = 0.2f;
                    if (input == 2) x = 0.05f * (float) std::sin (6.283185307179586 * hz * (double) (t + i) / r.sr);
                    b.setSample (0, i, x); b.setSample (1, i, x);
                }
            });
            for (int i = 0; i < nn; ++i) y.push_back (r.buf.getSample (0, i));
            t += nn;
        }
        return y;
    };
    auto rtOf = [] (const std::vector<float>& o, double rate) { juce::AudioBuffer<float> b (1, (int) o.size()); b.copyFrom (0, 0, o.data(), (int) o.size()); return ir::rt60 (b, rate); };
    auto impulse = [&] (Rig2& r) { r.p->mod().reset(); return rtOf (run (r, 1.5, 1), r.sr); };
    // Real time: the worker's answer, while nothing is going in.
    auto settle = [&] (Rig2& r) { run (r, 0.05, 0); juce::Thread::sleep (250); run (r, 0.05, 0); };
    auto near = [] (float a, float b) { return std::abs (a / b - 1.0f) < 0.05f; };

    float want = 0.0f, wantLong = 0.0f;
    {
        resetBus();
        Rig2 r ("TERROIR"); setup (r, true);
        r.set ("decay", 0.2f); run (r, 1.0, 0); want = impulse (r);
        r.set ("decay", 0.5f); run (r, 1.0, 0); wantLong = impulse (r);
    }
    std::printf ("   DECAY 20 %%: RT %.3f s (50 %%: %.3f s), the reference\n", want, wantLong);

    // 1, 2. Real time -> rendering -> real time; DECAY 50 % asked for in real time, its answer waiting
    //       (1) or still being built (2) when the render ends with DECAY at 20 %.
    for (int inFlight = 0; inFlight < 2; ++inFlight)
    {
        resetBus();
        Rig2 r ("TERROIR"); setup (r, false);
        settle (r);
        if (inFlight) ir::testBuildDelayMs = 400;
        r.set ("decay", 0.5f); run (r, 0.01, 0);
        juce::Thread::sleep (inFlight ? 60 : 300);
        r.p->setNonRealtime (true);
        r.set ("decay", 0.2f); run (r, 1.0, 0);
        const float off = impulse (r);
        r.p->setNonRealtime (false);
        run (r, 0.3, 0); juce::Thread::sleep (inFlight ? 700 : 250); run (r, 0.3, 0);
        const float on = impulse (r);
        run (r, 0.5, 0); juce::Thread::sleep (250); run (r, 0.5, 0);
        const float later = impulse (r);
        ir::testBuildDelayMs = 0;
        std::printf ("   real time > render > real time, DECAY 50 %% %s: RT rendering %.3f, back in real time %.3f, later %.3f s (want %.3f)\n",
                     inFlight ? "still being built when the render ends" : "built and waiting", off, on, later, want);
        CHECK (near (off, want) && near (on, want) && near (later, want), "TERROIR after a render with an old answer %s: RT %.3f / %.3f / %.3f, want %.3f",
               inFlight ? "in flight" : "waiting", off, on, later, want);
    }
    // 3. Prepare again (the stream starts over at 0) after a longer pass: DECAY 20 % from the first note.
    {
        resetBus();
        Rig2 r ("TERROIR"); setup (r, true);
        r.set ("decay", 0.2f); run (r, 1.0, 0);
        const float a = impulse (r);
        r.set ("decay", 1.0f); run (r, 2.0, 0);
        r.p->prepareToPlay (r.sr, r.block); r.ppq = 0.0;
        r.set ("decay", 0.2f);
        const float b = impulse (r);
        std::printf ("   prepared again after DECAY 100 %%, set to 20 %%: first note RT %.3f s (before %.3f)\n", b, a);
        CHECK (near (a, want) && near (b, want), "TERROIR prepared again: first note RT %.3f, want %.3f", b, want);
    }
    // 4. A fresh instance at DECAY 10 %, rendering: the first note, and after it settles.
    {
        resetBus();
        Rig2 r ("TERROIR"); setup (r, true);
        r.set ("decay", 0.1f);
        const float first = impulse (r);
        run (r, 1.0, 0);
        const float settled = impulse (r);
        std::printf ("   fresh instance at DECAY 10 %%: first note RT %.3f s, settled %.3f s\n", first, settled);
        CHECK (near (first, settled), "TERROIR fresh instance: first note RT %.3f, settled %.3f", first, settled);
    }
    // 5. The state (DECAY 20 %) loaded before prepare and after it; rendering and real time.
    {
        juce::MemoryBlock state;
        {
            resetBus();
            Rig2 r ("TERROIR"); setup (r, true);
            r.set ("decay", 0.2f); run (r, 0.1, 0);
            r.p->getStateInformation (state);
        }
        for (int rt = 0; rt < 2; ++rt)
            for (int before = 0; before < 2; ++before)
            {
                resetBus();
                Rig2 r ("TERROIR");
                r.head.bpm = 120.0; r.p->setNonRealtime (! rt);
                if (before) { r.p->setStateInformation (state.getData(), (int) state.getSize()); r.p->prepareToPlay (r.sr, r.block); }
                else r.p->setStateInformation (state.getData(), (int) state.getSize());
                if (rt) settle (r);
                const float first = impulse (r);
                std::printf ("   state loaded %s prepare, %s: first note RT %.3f s\n", before ? "before" : "after", rt ? "real time" : "rendering", first);
                CHECK (near (first, want), "TERROIR state loaded %s prepare (%s): first note RT %.3f, want %.3f", before ? "before" : "after", rt ? "real time" : "rendering", first, want);
            }
    }
    // 6. Real time at 48 kHz, then prepared at 96 kHz: DECAY 20 % at once.
    {
        resetBus();
        Rig2 r ("TERROIR"); setup (r, false);
        r.set ("decay", 0.2f); settle (r);
        const float at48 = impulse (r);
        r.sr = 96000.0; r.p->setRateAndBufferSizeDetails (r.sr, r.block); r.p->prepareToPlay (r.sr, r.block); r.ppq = 0.0;
        const float at96 = impulse (r);
        settle (r);
        const float later = impulse (r);
        std::printf ("   real time, 48 kHz then 96 kHz: RT %.3f s, then %.3f s at once, %.3f s later\n", at48, at96, later);
        CHECK (near (at48, want) && near (at96, want) && near (later, want), "TERROIR rate change: RT %.3f / %.3f / %.3f, want %.3f", at48, at96, later, want);
    }
    // 7. Level: steady sines at 100 Hz, 1 kHz, 5 kHz through the built-in room at each host rate, wet,
    //    the last second of 4: within 0.05 dB of 48 kHz. SIZE 150 % too.
    auto steady = [&] (double rate, double hz, float size, int place = 0, juce::MemoryBlock* st = nullptr)
    {
        resetBus();
        Rig2 r ("TERROIR", rate, 256);
        if (st != nullptr) r.p->setStateInformation (st->getData(), (int) st->getSize());
        setup (r, true);                                     // after the state: wet only
        r.set ("size", size); r.set ("place", (float) place);
        const auto y = run (r, 4.0, 2, hz);
        double e = 0.0; int c = 0;
        for (size_t i = (size_t) (3.0 * rate); i < y.size(); ++i) { e += (double) y[i] * y[i]; ++c; }
        return gainToDb ((float) std::sqrt (e / std::max (1, c)));
    };
    {
        float worst = 0.0f;
        for (float size : { 1.0f, 1.5f })
        {
            std::printf ("   SIZE %3.0f %%:", size * 100.0f);
            for (double hz : { 100.0, 1000.0, 5000.0 })
            {
                const float ref = steady (48000.0, hz, size);
                std::printf ("  %4.0f Hz %.2f dB", hz, ref);
                for (double rate : { 44100.0, 88200.0, 96000.0 })
                {
                    const float d = steady (rate, hz, size) - ref;
                    std::printf (" %+.3f", d);
                    worst = std::max (worst, std::abs (d));
                }
            }
            std::printf (" (at 48 kHz; 44.1, 88.2, 96 kHz against it)\n");
        }
        CHECK (worst < 0.05f, "TERROIR level changes with the host rate by %.3f dB", worst);
    }
    // 8. One room stored as a place twice: recorded at 48 kHz and at 96 kHz (mono, band-limited under
    //    18 kHz, each scaled to kEnergy at its own rate, as an import does): the same level at 48 and
    //    at 96 kHz.
    {
        const double r96 = 96000.0;
        std::mt19937 rng (31);
        std::normal_distribution<float> g (0.0f, 1.0f);
        std::vector<float> raw ((size_t) (1.2 * r96), 0.0f);
        for (size_t i = (size_t) (0.004 * r96); i < raw.size(); ++i) raw[i] = g (rng) * std::exp (-6.9078f * (float) (i / r96) / 0.6f);
        // Low-pass at 18 kHz (windowed sinc, 255 taps).
        std::vector<float> h96 (raw.size(), 0.0f);
        const double fc = 18000.0 / r96;
        for (size_t i = 0; i < raw.size(); ++i)
        {
            double acc = 0.0;
            for (int k = -127; k <= 127; ++k)
            {
                const int64_t j = (int64_t) i - k;
                if (j < 0 || j >= (int64_t) raw.size()) continue;
                const double w = 0.42 + 0.5 * std::cos (3.141592653589793 * k / 128.0) + 0.08 * std::cos (2.0 * 3.141592653589793 * k / 128.0);
                const double sn = k == 0 ? 2.0 * fc : std::sin (2.0 * 3.141592653589793 * fc * k) / (3.141592653589793 * k);
                acc += sn * w * raw[(size_t) j];
            }
            h96[i] = (float) acc;
        }
        const auto h48 = ir::stretch (h96.data(), (int) h96.size(), 0.5);
        auto flacOf = [] (const std::vector<float>& h, double rate)
        {
            juce::AudioBuffer<float> b (1, (int) h.size());
            b.copyFrom (0, 0, h.data(), (int) h.size());
            ir::normalise (b);
            juce::MemoryBlock mb;
            juce::FlacAudioFormat fmt;
            auto* mos = new juce::MemoryOutputStream (mb, false);
            std::unique_ptr<juce::AudioFormatWriter> w (fmt.createWriterFor (mos, rate, 1, 24, {}, 0));
            w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
            w.reset();
            return std::make_pair (mb, b.getMagnitude (0, 0, b.getNumSamples()));
        };
        const auto [f48, pk48] = flacOf (h48, 48000.0);
        const auto [f96, pk96] = flacOf (h96, r96);
        juce::MemoryBlock state;
        {
            resetBus();
            Rig2 r ("TERROIR");
            juce::MemoryBlock s0;
            r.p->getStateInformation (s0);
            auto root = juce::ValueTree::readFromData (s0.getData(), s0.getSize());
            auto m = root.getChildWithName ("MODULE");
            int slot = 0;
            for (auto* f : { &f48, &f96 })
            {
                juce::ValueTree c ("PLACE");
                c.setProperty ("slot", slot, nullptr);
                c.setProperty ("name", slot == 0 ? "ROOM AT 48" : "ROOM AT 96", nullptr);
                c.setProperty ("kind", "IMPULSE", nullptr);
                c.setProperty ("sr", slot == 0 ? 48000.0 : r96, nullptr);
                c.setProperty ("rt60", 0.6, nullptr);
                c.setProperty ("flac", *f, nullptr);
                m.appendChild (c, nullptr);
                ++slot;
            }
            juce::MemoryOutputStream os (state, false);
            root.writeToStream (os);
        }
        const float a48 = steady (48000.0, 1000.0, 1.0f, 0, &state), b48 = steady (48000.0, 1000.0, 1.0f, 1, &state);
        const float a96 = steady (r96, 1000.0, 1.0f, 0, &state), b96 = steady (r96, 1000.0, 1.0f, 1, &state);
        std::printf ("   one room stored at 48 and at 96 kHz (peaks %.2f, %.2f): at a 48 kHz host %.2f / %.2f dB, at 96 kHz %.2f / %.2f dB\n",
                     pk48, pk96, a48, b48, a96, b96);
        CHECK (pk48 < 0.99f && pk96 < 0.99f, "TERROIR test places clip");
        CHECK (std::abs (a48 - b48) < 0.05f && std::abs (a96 - b96) < 0.05f && std::abs (a48 - a96) < 0.05f,
               "TERROIR place level depends on rate: %.2f / %.2f / %.2f / %.2f dB", a48, b48, a96, b96);
    }
    resetBus();
}

// TERROIR's first sound is the room its knobs ask for (352df3c audit, A08). Real time, the first
// input in the first block, nothing run before it: the state loaded before prepare or after it, or
// prepared again after the knobs moved while it was off; a short room, a long one, a place of its
// own, LENGTH at the state's tempo; the worker as it is and slowed to 400 ms an IR (so nothing it
// builds can be what is heard). The output must be the same knobs' rendered, settled output, to
// -60 dB. And a state loaded while the room rings, as defined in Terroir.cpp (restored): the knobs
// are the state's from the next block, and its room, built in the state load and not by the worker,
// goes into the engine at its next tail block and crossfades in over the one after (4096 samples
// each) with the whole input history kept: the old room's output untouched for 4096 samples at
// least, no jump, and from 3 x 4096 samples on exactly the output of the state's room all along.
// States loaded from another thread while the audio runs end in the last one's room.
static void testTerroirColdStart()
{
    std::printf ("-- TERROIR cold start: the first input in real time meets the knobs' room; a state loaded while it rings\n");
    // sec seconds of input: 0 silence, 1 an impulse (0.2) at the first sample, 2 noise (0.05, seeded); the left output.
    auto run = [] (Rig2& r, double sec, int input, uint32_t seed = 1)
    {
        std::vector<float> y;
        std::mt19937 rng (seed);
        std::uniform_real_distribution<float> u (-0.05f, 0.05f);
        const int64_t n = (int64_t) std::llround (sec * r.sr);
        for (int64_t t = 0; t < n;)
        {
            const int nn = (int) std::min<int64_t> (r.block, n - t);
            r.step (nn, [&] (juce::AudioBuffer<float>& b, juce::MidiBuffer&, int m)
            {
                for (int i = 0; i < m; ++i)
                {
                    const float x = input == 1 ? (t + i == 0 ? 0.2f : 0.0f) : input == 2 ? u (rng) : 0.0f;
                    b.setSample (0, i, x); b.setSample (1, i, x);
                }
            });
            for (int i = 0; i < nn; ++i) y.push_back (r.buf.getSample (0, i));
            t += nn;
        }
        return y;
    };
    // What a leaves of ref over [from, to), in dB of ref's energy there.
    auto residualDb = [] (const std::vector<float>& a, const std::vector<float>& ref, size_t from = 0, size_t to = SIZE_MAX)
    {
        double e = 0.0, r = 0.0;
        for (size_t i = from; i < std::min ({ to, a.size(), ref.size() }); ++i) { const double d = (double) a[i] - ref[i]; e += d * d; r += (double) ref[i] * ref[i]; }
        return (float) (10.0 * std::log10 ((e + 1e-30) / (r + 1e-30)));
    };
    auto stateOf = [] (Rig2& r) { juce::MemoryBlock m; r.p->getStateInformation (m); return m; };
    auto load = [] (Rig2& r, const juce::MemoryBlock& m) { r.p->setStateInformation (m.getData(), (int) m.getSize()); };

    // The starting state: wet only, the knobs at their defaults, and a place of its own in slot 3 (a
    // stereo room, RT 0.9 s, stored as an import stores it).
    juce::MemoryBlock base;
    {
        std::mt19937 rng (77);
        std::normal_distribution<float> g (0.0f, 1.0f);
        juce::AudioBuffer<float> h (2, 48000);
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < h.getNumSamples(); ++i)
                h.setSample (c, i, i < 96 ? 0.0f : g (rng) * std::exp (-6.9078f * (float) i / 48000.0f / 0.9f));
        ir::normalise (h);
        CHECK (h.getMagnitude (0, h.getNumSamples()) < 0.99f, "TERROIR cold start: the test place clips");
        juce::MemoryBlock flac;
        {
            juce::FlacAudioFormat fmt;
            std::unique_ptr<juce::AudioFormatWriter> w (fmt.createWriterFor (new juce::MemoryOutputStream (flac, false), 48000.0, 2, 24, {}, 0));
            w->writeFromAudioSampleBuffer (h, 0, h.getNumSamples());
        }
        resetBus();
        Rig2 r ("TERROIR");
        r.set ("mix", 1.0f); r.set ("dry", 0.0f); r.set ("duck", 0.0f); r.set ("lo_cut", 20.0f);
        const auto s0 = stateOf (r);
        auto root = juce::ValueTree::readFromData (s0.getData(), s0.getSize());
        juce::ValueTree c ("PLACE");
        c.setProperty ("slot", 2, nullptr); c.setProperty ("name", "TEST ROOM", nullptr); c.setProperty ("kind", "IR FILE", nullptr);
        c.setProperty ("sr", 48000.0, nullptr); c.setProperty ("rt60", 0.9, nullptr); c.setProperty ("flac", flac, nullptr);
        root.getChildWithName ("MODULE").appendChild (c, nullptr);
        juce::MemoryOutputStream os (base, false);
        root.writeToStream (os);
    }
    // The knobs' room: rendered (built in the block), settled, then the first input after a reset.
    auto rendered = [&] (const std::vector<std::pair<const char*, float>>& knobs, double bpm, juce::MemoryBlock& state)
    {
        resetBus();
        Rig2 r ("TERROIR");
        r.head.bpm = bpm; r.p->setNonRealtime (true);
        load (r, base);
        for (const auto& [id, v] : knobs) r.set (id, v);
        run (r, 0.1, 0);
        state = stateOf (r);
        r.p->mod().reset();
        return run (r, 1.5, 1);
    };

    struct Cfg { const char* name; std::vector<std::pair<const char*, float>> knobs; double bpm; };
    const std::vector<Cfg> cfgs {
        { "DECAY 20 %", { { "decay", 0.2f } }, 120.0 },
        { "SIZE 180 %", { { "size", 1.8f } }, 120.0 },
        { "PLACE 3 (its own room), DECAY 60 %, DARK 30 %", { { "place", 2.0f }, { "decay", 0.6f }, { "dark", 0.3f } }, 120.0 },
        { "LENGTH 1/8 at 132 BPM", { { "length", 6.0f } }, 132.0 },
    };
    static const char* hows[] = { "the state loaded before prepare", "the state loaded after prepare", "prepared again after the knobs moved while off" };
    for (const auto& cfg : cfgs)
    {
        juce::MemoryBlock state;
        const auto want = rendered (cfg.knobs, cfg.bpm, state);
        juce::AudioBuffer<float> wb (1, (int) want.size());
        wb.copyFrom (0, 0, want.data(), (int) want.size());
        float worst = -400.0f;
        for (int slow : { 0, 400 })
            for (int how = 0; how < 3; ++how)
            {
                resetBus();
                Rig2 r ("TERROIR");
                r.head.bpm = cfg.bpm;
                ir::testBuildDelayMs = slow;
                if (how == 0) { load (r, state); r.p->prepareToPlay (r.sr, r.block); r.ppq = 0.0; }
                else if (how == 1) load (r, state);
                else
                {
                    load (r, base);
                    run (r, 0.2, 0);                                     // playing, at the defaults
                    for (const auto& [id, v] : cfg.knobs) r.set (id, v);   // turned while off
                    r.p->prepareToPlay (r.sr, r.block); r.ppq = 0.0;
                }
                const auto first = run (r, 1.5, 1);
                ir::testBuildDelayMs = 0;
                const float e = residualDb (first, want);
                worst = std::max (worst, e);
                CHECK (e < -60.0f && r.p->faultCount.load() == 0, "TERROIR cold start, %s, %s, worker %s: the first input's output is %.1f dB off the knobs' room (faults %d)",
                       cfg.name, hows[how], slow ? "at 400 ms an IR" : "as it is", e, r.p->faultCount.load());
            }
        std::printf ("   %s (RT %.3f s): the first input against the settled room, worst of 6 (state before / after prepare, prepared again; worker as is / slow): %.1f dB\n",
                     cfg.name, ir::rt60 (wb, kSr), worst);
    }

    // A state loaded while the room rings: A (SIZE 150 %, DECAY 100 %) to B (DECAY 20 %) at 0.3 s, in
    // real time, the worker at 1 s an IR. The same noise into three: A all along and B all along,
    // rendered (each built in the block), and A then B.
    juce::MemoryBlock A, B;
    std::vector<float> wantB;
    rendered ({ { "size", 1.5f }, { "decay", 1.0f } }, 120.0, A);
    wantB = rendered ({ { "decay", 0.2f } }, 120.0, B);
    {
        auto feed = [&] (const juce::MemoryBlock& first, const juce::MemoryBlock* then, int& faults)
        {
            resetBus();
            Rig2 r ("TERROIR");
            r.p->setNonRealtime (then == nullptr);
            load (r, first);
            auto y = run (r, 0.3, 2, 11);
            if (then != nullptr) load (r, *then);
            const auto z = run (r, 0.7, 2, 12);
            y.insert (y.end(), z.begin(), z.end());
            faults = r.p->faultCount.load();
            return y;
        };
        int fa = 0, fb = 0, fab = 0;
        const auto a = feed (A, nullptr, fa), b = feed (B, nullptr, fb);
        ir::testBuildDelayMs = 1000;
        const auto ab = feed (A, &B, fab);
        ir::testBuildDelayMs = 0;
        const size_t at = (size_t) std::llround (0.3 * kSr), settled = at + 3 * (size_t) IrFilter::kB2;
        size_t leaves = at, joins = at;
        for (size_t i = at; i < ab.size(); ++i) if (std::abs (ab[i] - a[i]) > 1e-6f) { leaves = i; break; }
        for (size_t i = ab.size(); i-- > at;) if (std::abs (ab[i] - b[i]) > 1e-6f) { joins = i + 1; break; }
        float jumpAB = 0.0f, jumpRef = 0.0f;
        for (size_t i = at - 2400; i < settled; ++i)
        {
            jumpAB = std::max (jumpAB, std::abs (ab[i] - ab[i - 1]));
            jumpRef = std::max ({ jumpRef, std::abs (a[i] - a[i - 1]), std::abs (b[i] - b[i - 1]) });
        }
        const float before = residualDb (ab, a, 0, at), after = residualDb (ab, b, settled);
        std::printf ("   state B loaded into A ringing (worker at 1 s an IR): A's output for %d samples after, B's exactly from %d (before: %.1f dB off A; from %d: %.1f dB off B); largest step %.4f (A's or B's own %.4f)\n",
                     (int) (leaves - at), (int) (joins - at), before, (int) (settled - at), after, jumpAB, jumpRef);
        CHECK (before < -60.0f && after < -60.0f && leaves - at >= (size_t) IrFilter::kB2 && joins <= settled && fa + fb + fab == 0,
               "TERROIR state loaded while ringing: %.1f dB off A before, %.1f dB off B after; A's for %d samples, B's from %d (faults %d)",
               before, after, (int) (leaves - at), (int) (joins - at), fa + fb + fab);
        CHECK (jumpAB <= 1.5f * jumpRef, "TERROIR state loaded while ringing: a step of %.4f in the change (A's and B's own at most %.4f)", jumpAB, jumpRef);
    }
    // States loaded from another thread while the audio runs (A, B, A ... then B): no fault, and then B's room.
    {
        resetBus();
        Rig2 r ("TERROIR");
        load (r, A);
        std::atomic<bool> done { false };
        std::thread loader ([&]
        {
            for (int i = 0; i < 40; ++i) { load (r, i % 2 ? A : B); juce::Thread::sleep (2); }
            load (r, B);
            done = true;
        });
        uint32_t blocks = 0;
        while (! done.load()) run (r, r.block / r.sr, 2, ++blocks);
        loader.join();
        run (r, 0.3, 0);
        r.p->mod().reset();
        const auto y = run (r, 1.5, 1);
        const float e = residualDb (y, wantB);
        std::printf ("   41 states loaded from another thread over %u blocks of audio, B last: then %.1f dB off B's room, faults %d\n", blocks, e, r.p->faultCount.load());
        CHECK (e < -60.0f && r.p->faultCount.load() == 0, "TERROIR states loaded while the audio runs: then %.1f dB off the last one's room (faults %d)", e, r.p->faultCount.load());
    }
    resetBus();
}
