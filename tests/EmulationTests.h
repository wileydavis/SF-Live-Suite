#pragma once
// The drum machines and the 303 at the circuit level [Nov 10]: SFTests --emulation (--acid: the 303).
// Included after QualityTests.h (Rig2, CHECK, kSr, kBlock).
#include <complex>

namespace emu
{
struct NoteEv { int at; int note; int vel; bool on; };

// A module's left output for the notes given (sample times), total samples long.
inline std::vector<float> renderNotes (Rig2& r, const std::vector<NoteEv>& evs, int total)
{
    std::vector<float> out;
    out.reserve ((size_t) total);
    for (int s = 0; s < total; s += kBlock)
    {
        const int n = std::min (kBlock, total - s);
        r.step (n, [&] (juce::AudioBuffer<float>&, juce::MidiBuffer& m, int)
        {
            for (const auto& e : evs)
                if (e.at >= s && e.at < s + n)
                    m.addEvent (e.on ? juce::MidiMessage::noteOn (1, e.note, (juce::uint8) e.vel) : juce::MidiMessage::noteOff (1, e.note), e.at - s);
        });
        for (int i = 0; i < n; ++i) out.push_back (r.buf.getSample (0, i));
    }
    return out;
}

// The RMS frequency of a stretch (Hz): from the first difference's RMS against the signal's,
// a measure of brightness that needs no FFT (a sine gives its own frequency).
inline double rmsHz (const std::vector<float>& x, int from, int to, double sr = kSr)
{
    double e = 0.0, d = 0.0;
    for (int i = std::max (1, from); i < std::min ((int) x.size(), to); ++i)
    {
        e += (double) x[(size_t) i] * x[(size_t) i];
        const double df = (double) x[(size_t) i] - x[(size_t) i - 1];
        d += df * df;
    }
    if (e <= 0.0) return 0.0;
    const double r = std::min (1.0, std::sqrt (d / e) * 0.5);
    return sr / juce::MathConstants<double>::pi * std::asin (r);
}

inline double rms (const std::vector<float>& x, int from, int to)
{
    double e = 0.0; int n = 0;
    for (int i = std::max (0, from); i < std::min ((int) x.size(), to); ++i) { e += (double) x[(size_t) i] * x[(size_t) i]; ++n; }
    return n > 0 ? std::sqrt (e / n) : 0.0;
}

// The amplitude of one frequency in a stretch (a single-bin DFT, Hann window).
inline double toneAmp (const std::vector<float>& x, int from, int to, double hz, double sr = kSr)
{
    std::complex<double> acc = 0.0; double w = 0.0;
    const int n = to - from;
    for (int i = 0; i < n; ++i)
    {
        const double h = 0.5 - 0.5 * std::cos (2.0 * juce::MathConstants<double>::pi * i / (n - 1));
        acc += (double) x[(size_t) (from + i)] * h * std::polar (1.0, -2.0 * juce::MathConstants<double>::pi * hz * (from + i) / sr);
        w += h;
    }
    return 2.0 * std::abs (acc) / w;
}

// The 303's open loop, 1 / D(s), s in units of wc (DiodeLadder303's comment).
inline std::complex<double> diodeD (std::complex<double> s)
{
    const auto p = s + 2.0;
    return (0.5 * s + 1.0) * p * (p * p - 2.0) - (p * p - 1.0);
}
} // namespace emu

static void testAcid()
{
    using namespace emu;
    std::printf ("-- TB-303 (ACID 303-TYPE): diode ladder, accent sweep, envelope, slide, square\n");
    // The filter alone against its transfer function, small signal, at 4x 48 kHz.
    {
        const double fs = 4.0 * 48000.0, fc = 1000.0;
        double worst = 0.0;
        for (float k : { 0.0f, 12.0f })
            for (double f : { 100.0, 500.0, 1000.0, 3000.0 })
            {
                DiodeLadder303 d; d.set ((float) fc, fs); d.setHighPass (DiodeLadder303::kHighPass, fs);
                const int n = (int) (fs * 0.3);
                std::vector<float> y ((size_t) n);
                for (int i = 0; i < n; ++i) y[(size_t) i] = d.tick (1.0e-3f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * f * i / fs), k);
                const double got = toneAmp (y, n / 2, n, f, fs) / 1.0e-3;
                const double wc = 2.0 * juce::MathConstants<double>::pi * fc / DiodeLadder303::kPeak;
                const std::complex<double> s (0.0, 2.0 * juce::MathConstants<double>::pi * f / wc);
                const std::complex<double> shp (0.0, f / DiodeLadder303::kHighPass);
                const double want = std::abs (1.0 / (diodeD (s) + (double) k * shp / (1.0 + shp)));
                worst = std::max (worst, std::abs (20.0 * std::log10 (got / want)));
            }
        std::printf ("   diode ladder against its transfer function (k 0 and 12, 100 Hz-3 kHz): worst %.2f dB\n", worst);
        CHECK (worst < 0.3, "the diode ladder is off its transfer function by %.2f dB", worst);
    }
    // Hot noise at full resonance anywhere from 60 Hz to 16 kHz: finite and bounded.
    {
        float peak = 0.0f; bool finite = true;
        juce::Random rnd (7);
        for (float hz : { 60.0f, 300.0f, 1000.0f, 5000.0f, 16000.0f })
        {
            const double fs = 4.0 * 44100.0;
            DiodeLadder303 d; d.set (hz, fs); d.setHighPass (DiodeLadder303::kHighPass, fs);
            const float k = DiodeLadder303::feedback (1.0f, hz);
            for (int i = 0; i < (int) fs; ++i)
            {
                const float y = d.tick (4.0f * (rnd.nextFloat() * 2.0f - 1.0f), k);
                finite = finite && std::isfinite (y);
                peak = std::max (peak, std::abs (y));
            }
        }
        std::printf ("   hot noise (+12 dB) at full resonance, 60 Hz-16 kHz: peak %.2f\n", peak);
        CHECK (finite && peak < 4.0f, "the diode ladder ran away: peak %.2f", peak);
    }
    // The resonance peaks where the cutoff says, and grows with RESONANCE.
    {
        const double fs = 4.0 * 48000.0;
        double peakHz[2] {}, peakDb[2] {};
        int j = 0;
        for (float res : { 0.5f, 1.0f })
        {
            double best = 0.0, bestHz = 0.0;
            for (double f = 300.0; f < 3000.0; f *= 1.02)
            {
                DiodeLadder303 d; d.set (1000.0f, fs); d.setHighPass (DiodeLadder303::kHighPass, fs);
                const float k = DiodeLadder303::feedback (res, 1000.0f);
                const int n = (int) (fs * 0.15);
                std::vector<float> y ((size_t) n);
                for (int i = 0; i < n; ++i) y[(size_t) i] = d.tick (1.0e-3f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * f * i / fs), k);
                const double a = toneAmp (y, n / 2, n, f, fs);
                if (a > best) { best = a; bestHz = f; }
            }
            DiodeLadder303 d; d.set (1000.0f, fs); d.setHighPass (DiodeLadder303::kHighPass, fs);
            const float k = DiodeLadder303::feedback (res, 1000.0f);
            const int n = (int) (fs * 0.3);
            std::vector<float> y ((size_t) n);
            for (int i = 0; i < n; ++i) y[(size_t) i] = d.tick (1.0e-3f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 100.0 * i / fs), k);
            peakHz[j] = bestHz; peakDb[j] = 20.0 * std::log10 (best / toneAmp (y, n / 2, n, 100.0, fs));
            ++j;
        }
        std::printf ("   CUTOFF 1 kHz: RESONANCE 0.5 peaks at %.0f Hz, %+.1f dB over 100 Hz; 1.0 at %.0f Hz, %+.1f dB\n", peakHz[0], peakDb[0], peakHz[1], peakDb[1]);
        CHECK (peakHz[1] > 850.0 && peakHz[1] < 1250.0 && peakDb[1] > 15.0 && peakDb[1] > peakDb[0] + 8.0,
               "the resonance peaks at %.0f Hz, %+.1f dB (0.5: %+.1f dB)", peakHz[1], peakDb[1], peakDb[0]);
    }

    const int step = (int) (kSr * 0.125);           // 16ths at 120 BPM
    auto acidRig = [] (float res, float decay, float accent)
    {
        auto r = std::make_unique<Rig2> ("LEAD");
        r->set ("model", 0.0f);
        r->set ("cutoff", 0.25f); r->set ("res", res); r->set ("env_amt", 0.25f); r->set ("decay", decay);
        r->set ("accent", accent); r->set ("glide", 0.0f); r->set ("pocket_depth", 0.0f); r->set ("space", 0.0f);
        r->set ("drive", 0.0f); r->set ("color", 0.0f); r->set ("density", 0.0f); r->set ("gate", 0.5f);
        return r;
    };
    // Accents close together climb on each other (the sweep capacitor); after a rest they start low
    // again; plain notes don't climb.
    {
        resetBus();
        auto r = acidRig (0.6f, 0.5f, 1.0f);
        std::vector<NoteEv> ev;
        const int restAt = 4 * step + (int) (kSr * 1.5);
        for (int k = 0; k < 4; ++k) { ev.push_back ({ k * step, 45, 127, true }); ev.push_back ({ k * step + step / 2, 45, 0, false }); }
        ev.push_back ({ restAt, 45, 127, true }); ev.push_back ({ restAt + step / 2, 45, 0, false });
        const auto y = renderNotes (*r, ev, restAt + step);
        double b[5];
        for (int k = 0; k < 4; ++k) b[k] = rmsHz (y, k * step + 240, k * step + 2160);
        b[4] = rmsHz (y, restAt + 240, restAt + 2160);
        resetBus();
        auto r2 = acidRig (0.6f, 0.5f, 1.0f);
        std::vector<NoteEv> ev2;
        for (int k = 0; k < 4; ++k) { ev2.push_back ({ k * step, 45, 90, true }); ev2.push_back ({ k * step + step / 2, 45, 0, false }); }
        const auto y2 = renderNotes (*r2, ev2, 4 * step);
        double p[4];
        for (int k = 0; k < 4; ++k) p[k] = rmsHz (y2, k * step + 240, k * step + 2160);
        std::printf ("   accents in a row: %.0f %.0f %.0f %.0f Hz (RMS frequency), after 1.5 s rest %.0f; plain notes %.0f %.0f %.0f %.0f\n",
                     b[0], b[1], b[2], b[3], b[4], p[0], p[1], p[2], p[3]);
        // (The first note of all starts its filter from rest, a little brighter for it; the sweep
        // levels off after three or four.)
        CHECK (b[1] > b[0] * 1.04 && b[2] > b[1] * 1.01 && b[3] > b[1], "accents don't climb: %.0f %.0f %.0f %.0f", b[0], b[1], b[2], b[3]);
        CHECK (b[4] < b[3] * 0.95 && std::abs (b[4] - b[0]) < 0.08 * b[0], "after a rest the accent starts at %.0f Hz (first %.0f, last %.0f)", b[4], b[0], b[3]);
        CHECK (p[1] < b[1] * 0.9 && std::abs (p[3] - p[1]) < 0.03 * p[1], "plain notes: %.0f .. %.0f (accent %.0f)", p[1], p[3], b[1]);
    }
    // A slide: the note slid into doesn't restart the envelope (darker than the same note retriggered).
    {
        double slid = 0.0, hit = 0.0;
        for (int overlap = 0; overlap < 2; ++overlap)
        {
            resetBus();
            auto r = acidRig (0.5f, 0.5f, 0.5f);
            // The glide is over (60 ms) well before the stretch measured, 150-200 ms into the note.
            const int second = 2 * step, len = (int) (kSr * 0.3);
            std::vector<NoteEv> ev { { 0, 45, 100, true }, { second, 52, 100, true } };
            ev.push_back ({ overlap ? second + 200 : second - 200, 45, 0, false });
            ev.push_back ({ second + len, 52, 0, false });
            const auto y = renderNotes (*r, ev, second + len);
            (overlap ? slid : hit) = rmsHz (y, second + (int) (kSr * 0.15), second + (int) (kSr * 0.2));
        }
        std::printf ("   the second note slid into: %.0f Hz (RMS frequency); played again: %.0f\n", slid, hit);
        CHECK (slid < hit * 0.92, "a slide restarted the envelope: %.0f against %.0f", slid, hit);
    }
    // An accent takes the shortest decay, whatever DECAY says.
    {
        double acc = 0.0, plain = 0.0;
        for (int a = 0; a < 2; ++a)
        {
            resetBus();
            auto r = acidRig (0.0f, 1.0f, 1.0f);
            r->set ("cutoff", 0.5f); r->set ("env_amt", 0.6f);
            std::vector<NoteEv> ev { { 0, 45, a ? 127 : 90, true }, { (int) (kSr * 0.5), 45, 0, false } };
            const auto y = renderNotes (*r, ev, (int) (kSr * 0.6));
            (a ? acc : plain) = rmsHz (y, (int) (kSr * 0.30), (int) (kSr * 0.36));
        }
        std::printf ("   DECAY at full, 0.3 s in: accented %.0f Hz, plain %.0f Hz (RMS frequency)\n", acc, plain);
        CHECK (acc < plain * 0.95, "an accent kept DECAY's long envelope: %.0f against %.0f", acc, plain);
    }
    // ENV MOD widens the sweep both ways, as the 303's does: a brighter peak and a darker rest.
    {
        double peak[2] {}, rest[2] {};
        for (int e = 0; e < 2; ++e)
        {
            resetBus();
            auto r = acidRig (0.6f, 0.0f, 0.0f);
            r->set ("cutoff", 0.5f); r->set ("env_amt", e ? 1.0f : 0.0f);
            std::vector<NoteEv> ev { { 0, 33, 100, true }, { (int) (kSr * 1.4), 33, 0, false } };
            const auto y = renderNotes (*r, ev, (int) (kSr * 1.4));
            peak[e] = rmsHz (y, (int) (kSr * 0.004), (int) (kSr * 0.02));
            rest[e] = rmsHz (y, (int) (kSr * 1.0), (int) (kSr * 1.3));
        }
        std::printf ("   ENV MOD 0 and full: peak %.0f and %.0f Hz, rest %.0f and %.0f Hz (RMS frequency)\n", peak[0], peak[1], rest[0], rest[1]);
        CHECK (peak[1] > peak[0] * 1.3 && rest[1] < rest[0] * 0.92, "ENV MOD: peak %.0f against %.0f, rest %.0f against %.0f", peak[1], peak[0], rest[1], rest[0]);
    }
    // RESONANCE's second gang: a played line keeps its loudness from no RESONANCE to full.
    {
        float lu[3] {};
        int j = 0;
        for (float res : { 0.0f, 0.5f, 1.0f })
        {
            resetBus();
            Rig2 r ("LEAD");
            r.set ("model", 0.0f); r.set ("res", res);
            LoudnessMeter lm; lm.prepare (kSr);
            for (int s = 0; s < (int) (kSr * 6.0); s += kBlock)
            {
                r.step (kBlock, [&] (juce::AudioBuffer<float>&, juce::MidiBuffer& m, int n) { gridNotes (m, r.ppq, r.bps(), n, 0.5, 0.25, 48, 100); });
                if (s > kSr * 1.0) for (int i = 0; i < kBlock; ++i) lm.push (r.buf.getSample (0, i), r.buf.getSample (1, i));
            }
            lu[j++] = lm.integrated;
        }
        const float spread = std::max ({ lu[0], lu[1], lu[2] }) - std::min ({ lu[0], lu[1], lu[2] });
        std::printf ("   a line at RESONANCE 0, 0.5, full: %.1f %.1f %.1f LUFS\n", lu[0], lu[1], lu[2]);
        CHECK (spread < 2.0f, "RESONANCE moves the level by %.1f dB", spread);
    }
    // The square is the 303's waveshaped one: much less second harmonic than the saw.
    {
        double h2[2] {};
        for (int sq = 0; sq < 2; ++sq)
        {
            resetBus();
            auto r = acidRig (0.0f, 1.0f, 0.0f);
            r->set ("cutoff", 1.0f); r->set ("env_amt", 0.0f); r->set ("color", sq ? 1.0f : 0.0f);
            std::vector<NoteEv> ev { { 0, 33, 100, true }, { (int) (kSr * 0.8), 33, 0, false } };
            const auto y = renderNotes (*r, ev, (int) (kSr * 0.8));
            const double f0 = 55.0;
            h2[sq] = 20.0 * std::log10 (toneAmp (y, (int) (kSr * 0.2), (int) (kSr * 0.7), 2.0 * f0) / toneAmp (y, (int) (kSr * 0.2), (int) (kSr * 0.7), f0));
        }
        std::printf ("   second harmonic against the first: saw %.1f dB, square %.1f dB\n", h2[0], h2[1]);
        CHECK (h2[1] < h2[0] - 6.0, "the square has as much second harmonic as the saw (%.1f against %.1f dB)", h2[1], h2[0]);
    }
}

// The NRG page's knobs [Nov 10]: KCK NRG at the bottom takes the kick out (ENERGY, DRUMS profile);
// ACID pushes the 303; WARP warbles the pitch, each voice its own way. SFTests --nrg.
static void testNrg()
{
    using namespace emu;
    std::printf ("-- NRG: ENERGY's DRUMS profile at the bottom, ACID, WARP\n");
    // ENERGY on a 60 Hz tone (a kick's body): the level against the input at each knob position.
    auto energyDb = [] (int profile, float knob)
    {
        resetBus();
        Rig2 r ("ENERGY");
        r.set ("profile", (float) profile); r.set ("energy", knob);
        int64_t t = 0;
        double ei = 0.0, eo = 0.0;
        for (int s = 0; s < (int) (kSr * 1.5); s += kBlock)
        {
            r.step (kBlock, [&] (juce::AudioBuffer<float>& b, juce::MidiBuffer&, int n)
            { for (int i = 0; i < n; ++i) { const float v = 0.3f * (float) std::sin (6.283185307 * 60.0 * (double) (t + i) / kSr); b.setSample (0, i, v); b.setSample (1, i, v); } });
            if (s > kSr * 0.7)
                for (int i = 0; i < kBlock; ++i) { const double x = 0.3 * std::sin (6.283185307 * 60.0 * (double) (t + i) / kSr); ei += x * x; eo += (double) r.buf.getSample (0, i) * r.buf.getSample (0, i); }
            t += kBlock;
        }
        return 10.0 * std::log10 ((eo + 1e-30) / ei);
    };
    const double d50 = energyDb (0, 0.5f), d13 = energyDb (0, 0.13f), d6 = energyDb (0, 0.0625f), d0 = energyDb (0, 0.0f), b0 = energyDb (1, 0.0f), m0 = energyDb (2, 0.0f);
    std::printf ("   a 60 Hz tone through ENERGY: DRUMS at the centre %+.1f dB, 13 %% %+.1f dB, 6 %% %+.1f dB, 0 %% %.1f dB; BASS at 0 %+.1f dB, MUSIC at 0 %+.1f dB\n", d50, d13, d6, d0, b0, m0);
    CHECK (std::abs (d50) < 0.5 && d13 > -12.0 && d6 < d13 - 3.0 && d6 > -60.0 && d0 < -120.0 && b0 > -40.0 && m0 > -40.0,
           "ENERGY DRUMS at the bottom: centre %+.1f, 13 %% %+.1f, 6 %% %+.1f, 0 %% %.1f dB; BASS %+.1f, MUSIC %+.1f dB", d50, d13, d6, d0, b0, m0);

    // ACID on SF LEAD's 303 (its defaults, a line of 16ths): brighter and more resonant; at 0 nothing changes.
    auto acidLine = [] (float acid)
    {
        resetBus();
        Rig2 r ("LEAD");
        r.set ("model", 0.0f); r.set ("acid", acid); r.set ("pocket_depth", 0.0f); r.set ("space", 0.0f);
        std::vector<float> out;
        for (int s = 0; s < (int) (kSr * 3.0); s += kBlock)
        {
            r.step (kBlock, [&] (juce::AudioBuffer<float>&, juce::MidiBuffer& m, int n) { gridNotes (m, r.ppq, r.bps(), n, 0.25, 0.125, 45, 100); });
            for (int i = 0; i < kBlock; ++i) out.push_back (r.buf.getSample (0, i));
        }
        return out;
    };
    const auto a0 = acidLine (0.0f), a1 = acidLine (1.0f);
    const double h0 = rmsHz (a0, (int) kSr, (int) (kSr * 3.0)), h1 = rmsHz (a1, (int) kSr, (int) (kSr * 3.0));
    const double l0 = 20.0 * std::log10 (rms (a0, (int) kSr, (int) (kSr * 3.0))), l1 = 20.0 * std::log10 (rms (a1, (int) kSr, (int) (kSr * 3.0)));
    std::printf ("   ACID on the 303: RMS frequency %.0f Hz at 0, %.0f Hz at full; level %+.1f dB against 0\n", h0, h1, l1 - l0);
    CHECK (h1 > h0 * 1.4 && std::abs (l1 - l0) < 9.0, "ACID: %.0f -> %.0f Hz, %+.1f dB", h0, h1, l1 - l0);

    // WARP on a pure sine (BASS SINE SUB, one held note): the pitch's swing over 3 s, from the zero
    // crossings, at 0 and at full.
    auto swingCents = [] (float warp)
    {
        resetBus();
        Rig2 r ("BASS");
        r.set ("model", 5.0f); r.set ("warp", warp); r.set ("pocket_depth", 0.0f); r.set ("glide", 0.0f); r.set ("drive", 0.0f);
        std::vector<float> out;
        bool on = false;
        for (int s = 0; s < (int) (kSr * 4.0); s += kBlock)
        {
            r.step (kBlock, [&] (juce::AudioBuffer<float>&, juce::MidiBuffer& m, int) { if (! on) { m.addEvent (juce::MidiMessage::noteOn (1, 45, (juce::uint8) 100), 0); on = true; } });
            for (int i = 0; i < kBlock; ++i) out.push_back (r.buf.getSample (0, i));
        }
        // Periods between rising zero crossings (interpolated), from 1 s on: highest against lowest frequency.
        std::vector<double> ups;
        for (size_t i = (size_t) kSr; i + 1 < out.size(); ++i)
            if (out[i] <= 0.0f && out[i + 1] > 0.0f) ups.push_back ((double) i + out[i] / (out[i] - out[i + 1]));
        double lo = 1e9, hi = 0.0;
        for (size_t k = 8; k < ups.size(); k += 8) { const double f = 8.0 * kSr / (ups[k] - ups[k - 8]); lo = std::min (lo, f); hi = std::max (hi, f); }
        return 1200.0 * std::log2 (hi / lo);
    };
    const double w0 = swingCents (0.0f), w1 = swingCents (1.0f);
    std::printf ("   WARP on a held sine: the pitch swings %.1f cents at 0, %.1f cents at full\n", w0, w1);
    CHECK (w0 < 1.0 && w1 > 30.0 && w1 < 90.0, "WARP: %.1f cents at 0, %.1f at full", w0, w1);
}

static void testEmulation()
{
    testAcid();
    testNrg();
}
