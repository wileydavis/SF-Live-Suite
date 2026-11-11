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

// The 909's and 808's bass drums as circuits [Nov 10]. SFTests --kicks.
static void testKicks()
{
    using namespace emu;
    std::printf ("-- KICK 909-TYPE and 808-TYPE: the circuits\n");
    auto hit = [] (int model, int vel, float decay = 0.5f, float click = 0.5f)
    {
        resetBus();
        Rig2 r ("KICK");
        r.set ("model", (float) model); r.set ("decay", decay); r.set ("click", click); r.set ("drive", 0.0f); r.set ("filter", 0.5f);
        std::vector<float> out;
        bool on = false;
        for (int s = 0; s < (int) (kSr * 2.0); s += kBlock)
        {
            r.step (kBlock, [&] (juce::AudioBuffer<float>&, juce::MidiBuffer& m, int) { if (! on) { m.addEvent (juce::MidiMessage::noteOn (1, 36, (juce::uint8) vel), 0); on = true; } });
            for (int i = 0; i < kBlock; ++i) out.push_back (r.buf.getSample (0, i));
        }
        return out;
    };
    // Instantaneous frequency from successive zero crossings (either direction), around t seconds.
    auto freqAt = [] (const std::vector<float>& x, double t)
    {
        std::vector<double> z;
        for (size_t i = 1; i < x.size(); ++i)
            if ((x[i - 1] <= 0.0f) != (x[i] <= 0.0f)) z.push_back ((double) i - 1 + x[i - 1] / (x[i - 1] - x[i]));
        const double at = t * kSr;
        for (size_t k = 1; k < z.size(); ++k)
            if (z[k] >= at) return kSr / (2.0 * (z[k] - z[k - 1]));
        return 0.0;
    };
    auto peakIn = [] (const std::vector<float>& x, double t0, double t1) { float p = 0; for (size_t i = (size_t) (t0 * kSr); i < (size_t) (t1 * kSr) && i < x.size(); ++i) p = std::max (p, std::fabs (x[i])); return p; };
    // A full period's frequency (two half periods: the 909's clipped underside skews each half).
    auto periodAt = [&] (const std::vector<float>& x, double t) { const double a = freqAt (x, t), b = freqAt (x, t + 0.5 / std::max (20.0, a)); return 2.0 / (1.0 / a + 1.0 / b); };
    auto meanOf = [] (const std::vector<float>& x) { double m = 0.0; for (float v : x) m += v; return m / (double) x.size(); };
    auto bandDb = [] (const std::vector<float>& x, double t0, double t1, double lo)   // energy above lo Hz (one-pole high-pass, twice)
    {
        const float a = std::exp (-6.2831853f * (float) lo / (float) kSr);
        float h1 = 0, p1 = 0, h2 = 0, p2 = 0; double e = 0.0;
        for (size_t i = 0; i < x.size() && i < (size_t) (t1 * kSr); ++i)
        {
            h1 = a * (h1 + x[i] - p1); p1 = x[i];
            h2 = a * (h2 + h1 - p2); p2 = h1;
            if (i >= (size_t) (t0 * kSr)) e += (double) h2 * h2;
        }
        return 10.0 * std::log10 (e + 1e-30);
    };
    // 808: the resting pitch, the attack's jump, the sigh (deeper with the accent), DECAY.
    {
        const auto hard = hit (1, 127), soft = hit (1, 40), shortD = hit (1, 127, 0.25f), longD = hit (1, 127, 0.75f);
        const double rest = periodAt (hard, 0.9), jump = freqAt (hard, 0.0015), s30 = periodAt (hard, 0.03), s300 = periodAt (hard, 0.3), soft30 = periodAt (soft, 0.03);
        const double dS = 20.0 * std::log10 (peakIn (shortD, 0.5, 0.52) / peakIn (shortD, 0.05, 0.07)), dL = 20.0 * std::log10 (peakIn (longD, 0.5, 0.52) / peakIn (longD, 0.05, 0.07));
        std::printf ("   808: resting %.1f Hz; the attack's first half period %.0f Hz; the sigh %.1f Hz at 30 ms (soft hit %.1f), %.1f at 300 ms; 50 ms to 500 ms: DECAY 25 %% %.1f dB, 75 %% %.1f dB; peak %.2f, mean %.5f\n",
                     rest, jump, s30, soft30, s300, dS, dL, peakIn (hard, 0.0, 0.2), meanOf (hard));
        CHECK (std::abs (rest - 49.5) < 1.0 && jump > rest * 1.3 && s30 > rest * 1.1 && s300 < rest * 1.03 && s30 > soft30 + 0.5 && dS < dL - 8.0
               && peakIn (hard, 0.0, 0.2) < 1.2f && std::abs (meanOf (hard)) < 1e-3,
               "808: rest %.1f, jump %.0f, sigh %.1f / %.1f (soft %.1f), decay %.1f / %.1f dB", rest, jump, s30, s300, soft30, dS, dL);
    }
    // 909: the sweep, the waveshaper's clipped underside, ATTACK's click.
    {
        const auto x = hit (0, 127), dry = hit (0, 127, 0.5f, 0.0f), loud = hit (0, 127, 0.5f, 1.0f);
        const double f2 = freqAt (x, 0.002), f100 = periodAt (x, 0.1);
        // The DC bias into the diode limiter: even harmonics a symmetric shaper wouldn't make.
        const double h1 = toneAmp (x, (int) (0.06 * kSr), (int) (0.16 * kSr), f100), h2 = toneAmp (x, (int) (0.06 * kSr), (int) (0.16 * kSr), 2.0 * f100);
        const double h3 = toneAmp (x, (int) (0.06 * kSr), (int) (0.16 * kSr), 3.0 * f100);
        const double pos = 20.0 * std::log10 (h2 / h1), neg = 20.0 * std::log10 (h3 / h1);
        const double clickDb = bandDb (loud, 0.0, 0.006, 2000.0) - bandDb (dry, 0.0, 0.006, 2000.0), lateDb = bandDb (loud, 0.05, 0.2, 2000.0) - bandDb (dry, 0.05, 0.2, 2000.0);
        std::printf ("   909: %.0f Hz at 2 ms, %.1f Hz at 100 ms; the body's second and third harmonics %.1f and %.1f dB; ATTACK full against off, above 2 kHz: first 6 ms %+.1f dB, 50-200 ms %+.1f dB; mean %.5f\n",
                     f2, f100, pos, neg, clickDb, lateDb, meanOf (x));
        CHECK (f2 > 3.0 * f100 && std::abs (f100 - 50.0) < 5.0 && pos > -35.0 && pos < -10.0 && neg > -45.0 && neg < -10.0 && clickDb > 12.0 && std::abs (lateDb) < 1.0
               && peakIn (x, 0.0, 0.2) < 1.2f && std::abs (meanOf (x)) < 1e-3,
               "909: %.0f / %.1f Hz, harmonics %.1f / %.1f dB, click %+.1f / %+.1f dB", f2, f100, pos, neg, clickDb, lateDb);
    }
}

// The 808's hat and the 909's ROM cymbals, the 808 and 909 snares and toms [Nov 10]. SFTests --perc-circuits.
static void testPercCircuits()
{
    using namespace emu;
    std::printf ("-- PERC: the 808 hat, the 909's ROM hats and cymbals, snares and toms\n");
    auto hit = [] (int model, float decay = 0.5f, float tone = 0.5f, float tune = 0.0f, double secs = 1.5)
    {
        resetBus();
        Rig2 r ("PERC");
        r.set ("model", (float) model); r.set ("decay", decay); r.set ("tone", tone); r.set ("tune", tune); r.set ("drive", 0.0f); r.set ("filter", 0.5f); r.set ("density", 0.0f);
        std::vector<float> out;
        bool on = false;
        for (int s = 0; s < (int) (kSr * secs); s += kBlock)
        {
            r.step (kBlock, [&] (juce::AudioBuffer<float>&, juce::MidiBuffer& m, int) { if (! on) { m.addEvent (juce::MidiMessage::noteOn (1, 42, (juce::uint8) 110), 0); on = true; } });
            for (int i = 0; i < kBlock; ++i) out.push_back (r.buf.getSample (0, i));
        }
        return out;
    };
    auto lvl = [] (const std::vector<float>& x, double t0, double t1) { return 20.0 * std::log10 (rms (x, (int) (t0 * kSr), (int) (t1 * kSr)) + 1e-15); };
    auto period = [] (const std::vector<float>& x, double t0, double t1)    // mean frequency from rising zero crossings
    {
        std::vector<double> u;
        for (size_t i = (size_t) (t0 * kSr); i + 1 < (size_t) (t1 * kSr) && i + 1 < x.size(); ++i)
            if (x[i] <= 0.0f && x[i + 1] > 0.0f) u.push_back ((double) i + x[i] / (x[i] - x[i + 1]));
        return u.size() > 2 ? kSr * (double) (u.size() - 1) / (u.back() - u.front()) : 0.0;
    };
    // 808 hat: bright, short, and fully off once its envelope is down (the swing VCA's diode).
    {
        const auto x = hit (1);
        const double early = lvl (x, 0.0, 0.03), hz = rmsHz (x, 0, (int) (0.03 * kSr)), late = lvl (x, 0.5, 1.0);
        std::printf ("   HAT 808: first 30 ms %.1f dB, RMS frequency %.0f Hz; 0.5-1 s %.1f dB\n", early, hz, late);
        CHECK (early > -40.0 && hz > 6000.0 && late < -120.0, "HAT 808: %.1f dB, %.0f Hz, tail %.1f dB", early, hz, late);
    }
    // 909 hat: DECAY full (the VCA open), the ROM's own decay quantised to 6 bits still ends it: its
    // tail stops dead about 36 dB under its peak instead of fading on.
    {
        const auto x = hit (0, 1.0f);
        const double a = lvl (x, 0.0, 0.03), b = lvl (x, 0.18, 0.22), c = lvl (x, 0.7, 1.0);
        std::printf ("   HAT 909 (6-bit ROM), DECAY full: first 30 ms %.1f dB, 180-220 ms %.1f dB, 0.7-1 s %.1f dB\n", a, b, c);
        CHECK (a > -40.0 && b > a - 50.0 && c < -100.0, "HAT 909 ROM: %.1f / %.1f / %.1f dB", a, b, c);
        const auto ride = hit (3, 0.5f, 0.5f, 0.0f, 3.0);
        const double r1 = lvl (ride, 0.8, 1.0);
        std::printf ("   RIDE 909: 0.8-1 s %.1f dB against the hat's %.1f\n", r1, lvl (x, 0.8, 1.0));
        CHECK (r1 > -70.0, "RIDE 909 too short: %.1f dB at 0.8 s", r1);
    }
    // Snares: the 808's body at 238 Hz, more snappy noise with TONE; the 909's noise longer with TONE.
    {
        const auto s0 = hit (13, 0.5f, 0.0f), s1 = hit (13, 0.5f, 1.0f);
        const double body = toneAmp (s0, (int) (0.02 * kSr), (int) (0.12 * kSr), 238.0), off = toneAmp (s0, (int) (0.02 * kSr), (int) (0.12 * kSr), 330.0);
        const double hi0 = 20.0 * std::log10 (rmsHz (s0, 0, (int) (0.05 * kSr))), hi1 = 20.0 * std::log10 (rmsHz (s1, 0, (int) (0.05 * kSr)));
        const auto n0 = hit (14, 0.5f, 0.0f), n1 = hit (14, 0.5f, 1.0f);
        const double t0 = lvl (n0, 0.35, 0.45), t1 = lvl (n1, 0.35, 0.45);
        const double b909 = 20.0 * std::log10 (toneAmp (n0, (int) (0.03 * kSr), (int) (0.13 * kSr), 185.0) / toneAmp (n0, (int) (0.03 * kSr), (int) (0.13 * kSr), 260.0));
        std::printf ("   SNARE 808: 238 Hz %.1f dB over 330 Hz; RMS frequency TONE 0 against full %+.1f dB. SNARE 909: 185 Hz %.1f dB over 260 Hz; 350-450 ms TONE 0 %.1f dB, full %.1f dB\n",
                     20.0 * std::log10 (body / off), hi1 - hi0, b909, t0, t1);
        CHECK (body > off * 4.0 && hi1 > hi0 + 6.0 && t1 > t0 + 10.0 && b909 > 12.0, "snares: body %.1f dB, snappy %+.1f dB, 909 body %.1f dB, tail %.1f / %.1f", 20.0 * std::log10 (body / off), hi1 - hi0, b909, t0, t1);
    }
    // Toms: settle at 100 and 110 Hz; TUNE +12 doubles them.
    {
        const double a = period (hit (15), 0.1, 0.3), b = period (hit (16), 0.15, 0.4), c = period (hit (15, 0.5f, 0.5f, 12.0f), 0.1, 0.3);
        std::printf ("   TOM 808 %.1f Hz (TUNE +12: %.1f), TOM 909 %.1f Hz\n", a, c, b);
        CHECK (std::abs (a - 100.0) < 4.0 && std::abs (c - 200.0) < 8.0 && std::abs (b - 110.0) < 6.0, "toms: %.1f / %.1f / %.1f Hz", a, c, b);
    }
}

// LAYER SPLIT [Nov 10] (Kit.h's LayerSplit): SFTests --split.
static void testLayerSplit()
{
    using namespace emu;
    std::printf ("-- LAYER SPLIT: which layer plays the lows, which the mids and highs\n");
    struct St { std::vector<float> l, r; };
    // One note on a fresh module, held, from its fourth block (the knobs settled, as in a set);
    // SPLIT to moveTo at moveAt and back to the centre at backAt (samples, on block starts).
    auto play = [] (const char* id, const std::vector<std::pair<const char*, float>>& set, int note, double secs,
                    int moveAt = -1, float moveTo = 0.5f, int backAt = -1)
    {
        resetBus();
        Rig2 r (id);
        for (const auto& [k, v] : set) r.set (k, v);
        St o;
        bool on = false;
        for (int s = 0; s < (int) (kSr * secs); s += kBlock)
        {
            if (s == moveAt) r.set ("split", moveTo);
            if (s == backAt) r.set ("split", 0.5f);
            r.step (kBlock, [&] (juce::AudioBuffer<float>&, juce::MidiBuffer& m, int) { if (! on && s >= 4 * kBlock) { m.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), 0); on = true; } });
            for (int i = 0; i < kBlock; ++i) { o.l.push_back (r.buf.getSample (0, i)); o.r.push_back (r.buf.getSample (1, i)); }
        }
        return o;
    };
    // Energy (dB) above or below hz (two Butterworth sections), t0 to t1 seconds.
    auto band = [] (const std::vector<float>& x, double t0, double t1, float hz, bool high)
    {
        sf::Svf a, b; a.set (hz, 0.7071f, kSr); b.set (hz, 0.7071f, kSr);
        double e = 0.0;
        for (size_t i = 0; i < x.size() && i < (size_t) (t1 * kSr); ++i)
        {
            const float y = high ? b.tickHP (a.tickHP (x[i])) : b.tickLP (a.tickLP (x[i]));
            if (i >= (size_t) (t0 * kSr)) e += (double) y * y;
        }
        return 10.0 * std::log10 (e + 1e-30);
    };
    auto db = [] (double a) { return 20.0 * std::log10 (a + 1e-12); };

    // BASS: SINE SUB (MODEL) and ACID 303-TYPE's saw (LAYER MODEL: one oscillator, rendered at 4x)
    // at LAYER 100 % on A1 (55 Hz). Left: the sine's 55 Hz and the 303's harmonics, each as loud as
    // alone; right: the 303's 55 Hz as alone and no harmonics (the sine has none). The centre is the
    // plain sum (the 303 at LAYER's 2/3) and anywhere inside the OFF zone is the same sound to the
    // sample. SPLIT HZ moves the crossover.
    {
        const std::vector<std::pair<const char*, float>> base { { "cutoff", 0.9f }, { "env_amt", 0.0f }, { "filter", 0.5f }, { "density", 0.0f }, { "drive", 0.0f }, { "color", 0.0f } };
        auto with = [&] (std::vector<std::pair<const char*, float>> extra) { auto v = base; v.insert (v.end(), extra.begin(), extra.end()); return v; };
        auto both = [&] (float split, float hz = 150.0f) { return play ("BASS", with ({ { "model", 5.0f }, { "layer_model", 3.0f }, { "layer", 1.0f }, { "split", split }, { "split_hz", hz } }), 33, 1.6); };
        const auto sine = play ("BASS", with ({ { "model", 5.0f }, { "layer", 0.0f } }), 33, 1.6);
        const auto mono = play ("BASS", with ({ { "model", 3.0f }, { "layer", 0.0f } }), 33, 1.6);
        const auto left = both (0.0f), right = both (1.0f), centre = both (0.5f), nearC = both (0.515f), right400 = both (1.0f, 400.0f);
        const int a0 = (int) (0.6 * kSr), a1 = (int) (1.5 * kSr);
        auto f = [&] (const St& x, double hz) { return db (toneAmp (x.l, a0, a1, hz)); };
        auto hi = [&] (const St& x) { return band (x.l, 0.6, 1.5, 500.0f, true); };
        std::printf ("   BASS 55 Hz: sine alone %.1f dB, 303 alone %.1f; left %.1f, right %.1f. Over 500 Hz: 303 alone %.1f dB, sine alone %.1f; left %.1f, right %.1f, centre %.1f\n",
                     f (sine, 55.0), f (mono, 55.0), f (left, 55.0), f (right, 55.0), hi (mono), hi (sine), hi (left), hi (right), hi (centre));
        CHECK (std::abs (f (left, 55.0) - f (sine, 55.0)) < 1.0 && std::abs (hi (left) - hi (mono)) < 1.0, "left: 55 Hz %.1f against %.1f, highs %.1f against %.1f",
               f (left, 55.0), f (sine, 55.0), hi (left), hi (mono));
        // Right, over 500 Hz: only the sine's own (its model's drive), as loud as alone.
        CHECK (std::abs (f (right, 55.0) - f (mono, 55.0)) < 1.0 && hi (right) < hi (mono) - 15.0 && std::abs (hi (right) - hi (sine)) < 1.5,
               "right: 55 Hz %.1f against %.1f, highs %.1f against the 303's %.1f and the sine's %.1f", f (right, 55.0), f (mono, 55.0), hi (right), hi (mono), hi (sine));
        CHECK (std::abs (hi (centre) - (hi (mono) + db (2.0 / 3.0))) < 0.5, "centre: highs %.1f, not the plain sum's %.1f", hi (centre), hi (mono) + db (2.0 / 3.0));
        bool same = centre.l.size() == nearC.l.size();
        for (size_t i = 0; same && i < centre.l.size(); ++i) same = centre.l[i] == nearC.l[i] && centre.r[i] == nearC.r[i];
        CHECK (same, "SPLIT inside the OFF zone changed the sound");
        const double h150 = f (right, 220.0), h400 = f (right400, 220.0);
        std::printf ("   the 303's 220 Hz under the right split: SPLIT HZ 150 %.1f dB, 400 %.1f dB\n", h150, h400);
        CHECK (h400 > h150 + 8.0, "SPLIT HZ: 220 Hz %.1f dB at 150, %.1f at 400", h150, h400);
    }
    // KICK: the 909 (MODEL) and the 808 (LAYER MODEL) at LAYER 100 %. Right: the 808's lows and the
    // 909's click and punch, each as on its own; left: the 909's lows, the 808's top.
    {
        auto kick = [&] (std::vector<std::pair<const char*, float>> s) { s.push_back ({ "drive", 0.0f }); s.push_back ({ "filter", 0.5f }); return play ("KICK", s, 36, 1.0); };
        const auto k909 = kick ({ { "model", 0.0f }, { "layer", 0.0f } }), k808 = kick ({ { "model", 1.0f }, { "layer", 0.0f } });
        const auto kr = kick ({ { "model", 0.0f }, { "layer_model", 1.0f }, { "layer", 1.0f }, { "split", 1.0f } });
        const auto kl = kick ({ { "model", 0.0f }, { "layer_model", 1.0f }, { "layer", 1.0f }, { "split", 0.0f } });
        auto lo = [&] (const St& x) { return band (x.l, 0.0, 0.8, 80.0f, false); };
        auto top = [&] (const St& x) { return band (x.l, 0.0, 0.8, 1000.0f, true); };
        std::printf ("   KICK under 80 Hz: 909 %.1f dB, 808 %.1f; right %.1f, left %.1f. Over 1 kHz: 909 %.1f, 808 %.1f; right %.1f, left %.1f\n",
                     lo (k909), lo (k808), lo (kr), lo (kl), top (k909), top (k808), top (kr), top (kl));
        CHECK (std::abs (lo (kr) - lo (k808)) < 1.5 && std::abs (top (kr) - top (k909)) < 1.5, "kick right: lows %.1f against %.1f, top %.1f against %.1f", lo (kr), lo (k808), top (kr), top (k909));
        CHECK (std::abs (lo (kl) - lo (k909)) < 1.5 && std::abs (top (kl) - top (k808)) < 2.0, "kick left: lows %.1f against %.1f, top %.1f against %.1f", lo (kl), lo (k909), top (kl), top (k808));
        CHECK (kr.l == kr.r, "the kick is mono");
    }
    // PAD: STRING MACHINE in both layers (unison and ensemble: wide), C2. Split, its lows are mono.
    {
        auto pad = [&] (float split) { return play ("PAD", { { "model", 0.0f }, { "layer_model", 0.0f }, { "layer", 1.0f }, { "split", split }, { "split_hz", 400.0f }, { "filter", 0.5f }, { "density", 0.0f } }, 36, 2.4); };
        auto sideOver = [&] (const St& x)
        {
            std::vector<float> m (x.l.size()), d (x.l.size());
            for (size_t i = 0; i < x.l.size(); ++i) { m[i] = 0.5f * (x.l[i] + x.r[i]); d[i] = 0.5f * (x.l[i] - x.r[i]); }
            const int a0 = (int) (1.2 * kSr), a1 = (int) (2.2 * kSr);
            return db (toneAmp (d, a0, a1, 65.41)) - db (toneAmp (m, a0, a1, 65.41));
        };
        const double off = sideOver (pad (0.5f)), on = sideOver (pad (0.0f));
        std::printf ("   PAD: the side at 65 Hz against the mid, off %.1f dB, split %.1f dB\n", off, on);
        CHECK (on < off - 25.0, "mono lows: side %.1f dB split, %.1f off", on, off);
    }
    // Into the split and out of it mid-note: a fade, no click. SINE SUB in both layers (a click
    // would be many times the sines' steepest step).
    {
        const int moveAt = 75 * kBlock, backAt = 150 * kBlock;
        const auto x = play ("BASS", { { "model", 5.0f }, { "layer_model", 5.0f }, { "layer", 1.0f }, { "filter", 0.5f }, { "density", 0.0f } }, 33, 1.2, moveAt, 0.0f, backAt);
        auto steep = [&] (int a, int b) { float m = 0.0f; for (int i = std::max (1, a); i < b; ++i) m = std::max (m, std::fabs (x.l[(size_t) i] - x.l[(size_t) i - 1])); return m; };
        const float calm = std::max ({ steep ((int) (0.3 * kSr), moveAt), steep (moveAt + 2400, backAt), steep (backAt + 2400, (int) (1.15 * kSr)) });
        const float move = std::max (steep (moveAt, moveAt + 2400), steep (backAt, backAt + 2400));
        std::printf ("   SPLIT 0 to full and back mid-note: steepest step %.5f against %.5f held\n", move, calm);
        CHECK (move < calm * 1.3f, "SPLIT clicks: step %.5f against %.5f", move, calm);
    }
    // The value as Live shows it.
    {
        Rig2 r ("BASS");
        auto* prm = r.p->apvts.getParameter ("split");
        const auto a = prm->getText (prm->convertTo0to1 (0.0f), 64), b = prm->getText (prm->convertTo0to1 (0.5f), 64), c = prm->getText (prm->convertTo0to1 (1.0f), 64);
        std::printf ("   SPLIT reads %s / %s / %s\n", a.toRawUTF8(), b.toRawUTF8(), c.toRawUTF8());
        CHECK (a == "MODEL LOWS 100 %" && b == "OFF" && c == "LAYER LOWS 100 %", "SPLIT's text: %s / %s / %s", a.toRawUTF8(), b.toRawUTF8(), c.toRawUTF8());
    }
}

// DRIFT [Nov 10]: SFTests --drift. SINE SUB (mono: its one oscillator wanders, no slot offset) held
// 12 s on A1, its pitch in half-second windows.
static void testDrift()
{
    using namespace emu;
    std::printf ("-- DRIFT: analog voices off tune and wandering\n");
    auto cents = [] (float drift)
    {
        resetBus();
        Rig2 r ("BASS");
        r.set ("model", 5.0f); r.set ("drift", drift); r.set ("filter", 0.5f); r.set ("density", 0.0f);
        std::vector<float> x;
        bool on = false;
        for (int s = 0; s < (int) (kSr * 12.0); s += kBlock)
        {
            r.step (kBlock, [&] (juce::AudioBuffer<float>&, juce::MidiBuffer& m, int) { if (! on) { m.addEvent (juce::MidiMessage::noteOn (1, 33, (juce::uint8) 100), 0); on = true; } });
            for (int i = 0; i < kBlock; ++i) x.push_back (r.buf.getSample (0, i));
        }
        std::vector<double> c;
        for (double t = 0.5; t + 0.5 <= 12.0; t += 0.5)
        {
            std::vector<double> u;                         // rising zero crossings
            for (size_t i = (size_t) (t * kSr); i + 1 < (size_t) ((t + 0.5) * kSr); ++i)
                if (x[i] <= 0.0f && x[i + 1] > 0.0f) u.push_back ((double) i + x[i] / (x[i] - x[i + 1]));
            c.push_back (u.size() > 2 ? 1200.0 * std::log2 (kSr * (double) (u.size() - 1) / (u.back() - u.front()) / 55.0) : 1e9);
        }
        return c;
    };
    const auto c0 = cents (0.0f), c1 = cents (1.0f);
    const auto [lo0, hi0] = std::minmax_element (c0.begin(), c0.end());
    const auto [lo1, hi1] = std::minmax_element (c1.begin(), c1.end());
    std::printf ("   SINE SUB on A1 over 12 s: DRIFT 0 %+.2f .. %+.2f cents, DRIFT full %+.2f .. %+.2f cents\n", *lo0, *hi0, *lo1, *hi1);
    CHECK (*hi0 - *lo0 < 0.05 && std::abs (*lo0) < 0.2, "DRIFT 0 moved: %.2f .. %.2f cents", *lo0, *hi0);
    CHECK (*hi1 - *lo1 > 1.0 && std::max (std::abs (*lo1), std::abs (*hi1)) < 4.5, "DRIFT full: %.2f .. %.2f cents", *lo1, *hi1);
}

// SF RESONATOR [Nov 10]: SFTests --resonator. White noise in; the resonators' fundamentals where
// the chord puts them (cents), the key from SF CONTROL, DECAY's ring time, FREEZE holding.
static void testResonator()
{
    using namespace emu;
    std::printf ("-- SF RESONATOR: six resonators on a chord in the key\n");
    auto& d = sf::Bus::get().d();
    // Noise for `noiseSecs` (or one click at the start), then silence; total secs. key: SF CONTROL's KEY (-1 none).
    auto run = [&] (const std::vector<std::pair<const char*, float>>& set, double secs, double noiseSecs, int key = -1, double freezeAt = -1.0, bool click = false)
    {
        resetBus();
        Rig2 r ("RESONATOR");
        for (const auto& [k, v] : set) r.set (k, v);
        Noise nz { 0x1234567u };
        std::vector<float> out;
        for (int s = 0; s < (int) (kSr * secs); s += kBlock)
        {
            d.keyRoot = key;
            if (key >= 0) d.controlSeenMs = Bus::nowMs();
            if (freezeAt >= 0.0 && s >= (int) (freezeAt * kSr) && s < (int) (freezeAt * kSr) + kBlock) r.set ("freeze", 1.0f);
            r.step (kBlock, [&] (juce::AudioBuffer<float>& b, juce::MidiBuffer&, int n)
            {
                for (int i = 0; i < n; ++i)
                {
                    const double t = (double) (s + i) / kSr;
                    const float x = click ? (s + i == 0 ? 1.0f : 0.0f) : (t < noiseSecs ? 0.1f * nz.next() : 0.0f);
                    b.setSample (0, i, x); b.setSample (1, i, x);
                }
            });
            for (int i = 0; i < kBlock; ++i) out.push_back (0.5f * (r.buf.getSample (0, i) + r.buf.getSample (1, i)));
        }
        d.keyRoot = -1;
        return out;
    };
    // The strongest frequency within +-60 cents of hz (FFT from 0.1 s, 2.7 s long, parabolic peak), in
    // cents off hz. A click rings each resonator at its own frequency (noise would scatter the peaks).
    auto peakCents = [] (const std::vector<float>& x, double hz)
    {
        constexpr int order = 17, N = 1 << order;
        std::vector<float> f (2 * (size_t) N, 0.0f);
        const int from = (int) (0.1 * kSr);
        for (int i = 0; i < N && from + i < (int) x.size(); ++i) f[(size_t) i] = x[(size_t) (from + i)] * (0.5f - 0.5f * std::cos (2.0f * juce::MathConstants<float>::pi * (float) i / (float) (N - 1)));
        juce::dsp::FFT fft (order);
        fft.performFrequencyOnlyForwardTransform (f.data());
        const double bin = kSr / N;
        const int k0 = (int) (hz * std::pow (2.0, -0.05) / bin), k1 = (int) (hz * std::pow (2.0, 0.05) / bin) + 1;
        int best = k0;
        for (int k = k0; k <= k1; ++k) if (f[(size_t) k] > f[(size_t) best]) best = k;
        const double a = std::log (f[(size_t) best - 1] + 1e-20), b = std::log (f[(size_t) best] + 1e-20), c = std::log (f[(size_t) best + 1] + 1e-20);
        const double p = best + 0.5 * (a - c) / (a - 2.0 * b + c);
        return 1200.0 * std::log2 (p * bin / hz);
    };
    auto hzOf = [] (double midi) { return 440.0 * std::pow (2.0, (midi - 69.0) / 12.0); };
    // MIN7 on C3 (ROOT C, OCTAVE C3), DAMP 0.6 (the low-pass's phase corrected): C3 Eb3 G3 Bb3 C4 Eb4 in tune.
    {
        const auto x = run ({ { "chord", 3.0f }, { "root", 0.0f }, { "octave", 2.0f }, { "damp", 0.6f }, { "decay", 6.0f }, { "mix", 1.0f }, { "spread", 0.0f } }, 3.0, 0.0, -1, -1.0, true);
        const int notes[] = { 48, 51, 55, 58, 60, 63 };
        double worst = 0.0;
        std::printf ("   MIN7 on C3, DAMP 60 %%:");
        for (int n : notes) { const double cts = peakCents (x, hzOf (n)); worst = std::max (worst, std::abs (cts)); std::printf (" %+.1f", cts); }
        std::printf (" cents\n");
        CHECK (worst < 3.0, "RESONATOR out of tune: %.1f cents", worst);
    }
    // SF CONTROL's KEY D: the same chord on D3.
    {
        const auto x = run ({ { "chord", 3.0f }, { "root", 0.0f }, { "octave", 2.0f }, { "damp", 0.3f }, { "decay", 6.0f }, { "mix", 1.0f } }, 3.0, 0.0, 2, -1.0, true);
        const double c0 = peakCents (x, hzOf (50)), c1 = peakCents (x, hzOf (57));
        // C3's own resonance is gone: its band is quieter than D3's.
        std::printf ("   KEY D from SF CONTROL: D3 %+.1f cents, A3 %+.1f cents\n", c0, c1);
        CHECK (std::abs (c0) < 3.0 && std::abs (c1) < 3.0, "RESONATOR KEY: %.1f / %.1f cents", c0, c1);
    }
    // DECAY 2 s: a click rings down 60 dB in about 2 s (measured from 0.2 s on, by the slope).
    {
        const auto x = run ({ { "chord", 0.0f }, { "octave", 2.0f }, { "damp", 0.0f }, { "decay", 2.0f }, { "mix", 1.0f } }, 3.0, 0.0, -1, -1.0, true);
        const double a = rms (x, (int) (0.2 * kSr), (int) (0.4 * kSr)), b = rms (x, (int) (1.2 * kSr), (int) (1.4 * kSr));
        const double t60 = 60.0 / (20.0 * std::log10 (a / b));
        std::printf ("   DECAY 2 s, DAMP 0: rings to -60 dB in %.2f s\n", t60);
        CHECK (t60 > 1.6 && t60 < 2.4, "RESONATOR DECAY: %.2f s", t60);
    }
    // FREEZE: noise 1 s, FREEZE at 1 s (the noise goes on, shut out): the ring holds, from 1-1.5 s on to
    // 3.5-4 s within 1.5 dB, and within 8 dB of the level before (the interpolation takes the top
    // harmonics down over the first second, as damping would).
    {
        const auto x = run ({ { "chord", 3.0f }, { "octave", 2.0f }, { "damp", 0.3f }, { "decay", 1.0f }, { "mix", 1.0f } }, 4.0, 4.0, -1, 1.0);
        auto lvl = [&] (double t0, double t1) { return 20.0 * std::log10 (rms (x, (int) (t0 * kSr), (int) (t1 * kSr)) + 1e-12); };
        const double a = lvl (0.8, 1.0), b = lvl (2.0, 2.5), c = lvl (3.5, 4.0);
        std::printf ("   FREEZE: before %.1f dB; 1-1.5 s on %.1f dB, 2.5-3 s on %.1f dB\n", a, b, c);
        CHECK (std::abs (c - b) < 1.5 && b > a - 8.0 && b < a + 3.0, "RESONATOR FREEZE: %.1f dB, then %.1f and %.1f dB", a, b, c);
    }
    // Level: white noise at -25 dBFS RMS through MIX 100 % comes out within 3 dB of it, from DECAY 0.5 s
    // to 8 s and DAMP 0 to 90 % (SPREAD 0: the six summed in the middle).
    {
        const double in = 20.0 * std::log10 (0.1 / std::sqrt (3.0));
        double lo = 1e9, hi = -1e9;
        for (const float dec : { 0.5f, 8.0f })
            for (const float dmp : { 0.0f, 0.4f, 0.9f })
            {
                const auto x = run ({ { "chord", 3.0f }, { "octave", 2.0f }, { "damp", dmp }, { "decay", dec }, { "mix", 1.0f }, { "spread", 0.0f } }, 6.0, 6.0);
                const double out = 20.0 * std::log10 (rms (x, (int) (4.0 * kSr), (int) (6.0 * kSr)) + 1e-12);
                lo = std::min (lo, out - in); hi = std::max (hi, out - in);
                std::printf ("   noise in at %.1f dBFS, DECAY %.1f s, DAMP %2.0f %%: out %.1f dBFS\n", in, dec, dmp * 100.0f, out);
            }
        CHECK (lo > -3.0 && hi < 3.0, "RESONATOR level: %.1f to %+.1f dB against the input", lo, hi);
    }
}

static void testEmulation()
{
    testAcid();
    testNrg();
    testKicks();
    testPercCircuits();
}
