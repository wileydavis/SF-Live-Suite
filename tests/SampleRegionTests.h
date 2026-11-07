// START, END, FADE IN, FADE OUT: the part of a sample that plays, on every sampler (KICK, PERC,
// RUMBLE, BASS, LEAD, PAD, VOICE). Included by SuiteTests.cpp (--region).
//   SampleRegion: the defaults are the whole sample with no fades (gain exactly 1 everywhere); a hair
//     off a default (as knob smoothing leaves it) is the default; a point inside the sample fades
//     1 ms at least; END is never before START; fades longer than the part are scaled to fit it.
//   VOICE's slices (Slices): the whole phrase as before regions; AUTO from START (or the onset just
//     after it), up to END; EQUAL 8 / 16 in START..END; WHOLE from START.
//   Every sampler: START 20 % .. END 40 % of a 1.5 s sample plays that part (its loud middle) and no
//     more; FADE IN and FADE OUT shape it, in the sample's own time (KICK an octave up: twice as
//     fast); a cut at a waveform peak doesn't click (the sample's own start at a peak does, on KICK
//     and PERC: the check sees a click when there is one).
//   KICK: a cut sample tells Pocket its shorter length.
//   PAD GRANULAR: the grains come from START..END, and follow the knobs while a note holds (a grain
//     playing finishes in the part it started in: none is cut); a part shorter than a grain still sounds.
//   Presets leave the region alone; a hair off the defaults plays the defaults, to the bit.
//   Editor: the SAMPLE group (folded at first, the four knobs); the display grows by the strip when
//     a sample loads; START and END dragged on it (past the display's edge too, and never across
//     each other); on VOICE's phrase.
#include "modules/Kit.h"

namespace regiontest
{
constexpr int kLen = 72000;                                  // the test samples: 1.5 s at 48 kHz
constexpr double kOn = 0.25, kOff = 2.25;                    // the note (s)
const char* const kSamplers[] = { "KICK", "PERC", "RUMBLE", "BASS", "LEAD", "PAD", "VOICE" };

inline juce::File writeWav (const juce::String& name, const std::function<float (int)>& f, int n = kLen)
{
    juce::AudioBuffer<float> b (2, n);
    for (int i = 0; i < n; ++i) { const float x = f (i); b.setSample (0, i, x); b.setSample (1, i, x); }
    auto file = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile (name);
    file.deleteFile();
    juce::WavAudioFormat fmt;
    std::unique_ptr<juce::AudioFormatWriter> w (fmt.createWriterFor (new juce::FileOutputStream (file), 48000.0, 2, 24, {}, 0));
    w->writeFromAudioSampleBuffer (b, 0, n);
    return file;
}
inline float sine (int i, float amp) { return amp * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 110.0 * i / 48000.0); }

inline float choiceIndex (SFProcessor& p, const char* param, const char* name)
{
    const auto& sp = p.allSpecs()[(size_t) p.paramIndex (param)];
    for (size_t k = 0; k < sp.choices.size(); ++k) if (sp.choices[k] == name) return (float) k;
    ++failures; std::printf ("  FAIL: %s has no %s\n", param, name);
    return 0.0f;
}

using Settings = std::vector<std::pair<const char*, float>>;
// A note (A2, 110 Hz: the test samples' own pitch) at 0.25 s, held 2 s, on the module's SAMPLE model
// with its own sound out of the way (DECAY open, no GRIT, no echo, notes as played); then `extra`.
// during (optional): called before each block with the rig and the block's first sample. The left output.
inline std::vector<float> render (const char* id, const juce::File& wav, const Settings& extra, double secs = 2.6,
                                  const std::function<void (Rig2&, int)>& during = {})
{
    resetBus();
    Rig2 r (id);
    r.head.playing = true; r.head.bpm = 120.0;
    const std::string m (id);
    r.set ("model", choiceIndex (*r.p, "model", "SAMPLE"));
    if (m == "KICK" || m == "PERC") r.set ("decay", 1.0f);
    if (m == "RUMBLE") { r.set ("source", 1.0f); r.set ("grit", 0.0f); }
    if (m == "BASS" || m == "LEAD" || m == "PAD") r.set ("gate", 0.5f);
    if (m == "VOICE") { r.set ("slice_mode", 3.0f); r.set ("echo", 0.0f); r.set ("drive", 0.0f); }
    for (auto [k, v] : extra) r.set (k, v);
    r.p->mod().loadFiles (juce::StringArray (wav.getFullPathName()));
    const int on = (int) (kOn * kSr), off = (int) (kOff * kSr), total = (int) (secs * kSr);
    std::vector<float> out;
    out.reserve ((size_t) total);
    for (int t = 0; t < total; t += kBlock)
    {
        const int n = std::min (kBlock, total - t);
        if (during) during (r, t);
        r.step (n, [&] (juce::AudioBuffer<float>&, juce::MidiBuffer& mb, int nn)
        {
            if (on >= t && on < t + nn) mb.addEvent (juce::MidiMessage::noteOn (1, 45, (juce::uint8) 100), on - t);
            if (off >= t && off < t + nn) mb.addEvent (juce::MidiMessage::noteOff (1, 45), off - t);
        });
        for (int i = 0; i < n; ++i) out.push_back (r.buf.getSample (0, i));
    }
    return out;
}

inline int at (double sec) { return (int) std::lround (sec * kSr); }
inline float peak (const std::vector<float>& x, int a, int b)
{
    float p = 0.0f;
    for (int i = std::max (0, a); i < std::min ((int) x.size(), b); ++i) p = std::max (p, std::fabs (x[(size_t) i]));
    return p;
}
inline float rms (const std::vector<float>& x, int a, int b)
{
    double e = 0.0; int n = 0;
    for (int i = std::max (0, a); i < std::min ((int) x.size(), b); ++i, ++n) e += (double) x[(size_t) i] * x[(size_t) i];
    return n > 0 ? (float) std::sqrt (e / n) : 0.0f;
}
inline float maxStep (const std::vector<float>& x, int a, int b)
{
    float d = 0.0f;
    for (int i = std::max (1, a); i < std::min ((int) x.size(), b); ++i) d = std::max (d, std::fabs (x[(size_t) i] - x[(size_t) i - 1]));
    return d;
}
// First and last sample above frac of the whole peak.
inline std::pair<int, int> span (const std::vector<float>& x, float frac)
{
    const float th = peak (x, 0, (int) x.size()) * frac;
    int a = -1, b = -1;
    for (int i = 0; i < (int) x.size(); ++i) if (std::fabs (x[(size_t) i]) > th) { if (a < 0) a = i; b = i; }
    return { a, b };
}

inline juce::MouseEvent mouseAt (juce::Component& c, juce::Point<float> p, bool dragged)
{
    const auto now = juce::Time::getCurrentTime();
    return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), p, juce::ModifierKeys::leftButtonModifier,
                             0.0f, 0.0f, 0.0f, 0.0f, 0.0f, &c, &c, now, p, now, 1, dragged);
}
} // namespace regiontest

static void testSampleRegion()
{
    using namespace regiontest;
    std::printf ("-- Sample START, END, FADE IN, FADE OUT on every sampler\n");

    // ---- SampleRegion
    {
        SampleData s;
        s.buf.setSize (2, kLen);
        s.sr = 48000.0;
        const auto d = SampleRegion::of (s, 0.0f, 1.0f, 0.0f, 0.0f);
        bool unity = true;
        for (double pos = -2.0; pos < kLen + 2.0; pos += 0.37) unity = unity && d.gain (pos) == 1.0f;
        CHECK (d.start == 0.0 && d.end == kLen && d.fadeIn == 0.0 && d.fadeOut == 0.0 && unity, "the defaults: the whole sample, gain 1 (%g..%g, fades %g %g)", d.start, d.end, d.fadeIn, d.fadeOut);
        const auto h = SampleRegion::of (s, 1.0e-6f, 1.0f - 1.0e-6f, 0.001f, 0.001f);
        CHECK (h.start == 0.0 && h.end == kLen && h.fadeIn == 0.0 && h.fadeOut == 0.0, "a hair off the defaults is the defaults (%g..%g, fades %g %g)", h.start, h.end, h.fadeIn, h.fadeOut);
        const auto c = SampleRegion::of (s, 0.5f, 0.75f, 0.0f, 0.0f);
        CHECK (c.start == 36000.0 && c.end == 54000.0 && c.fadeIn == 48.0 && c.fadeOut == 48.0, "inner points fade 1 ms (%g..%g, fades %g %g)", c.start, c.end, c.fadeIn, c.fadeOut);
        CHECK (c.gain (c.start) == 0.0f && std::abs (c.gain (c.start + 24.0) - 0.5f) < 1e-5f && c.gain (c.start + 48.0) == 1.0f && c.gain (c.end) == 0.0f
               && std::abs (c.gain (c.end - 24.0) - 0.5f) < 1e-5f && c.gain (45000.0) == 1.0f, "the fades: 0 at the points, 0.5 halfway, 1 between");
        const auto x = SampleRegion::of (s, 0.6f, 0.4f, 0.0f, 0.0f);
        CHECK (std::abs (x.start - 43200.0) < 0.01 && std::abs (x.end - x.start - 48.0) < 1e-9, "END before START: 1 ms after it (%g..%g)", x.start, x.end);
        const auto f = SampleRegion::of (s, 0.5f, 0.6f, 200.0f, 100.0f);    // 150 ms of fades in 150 ms... 300 ms asked
        CHECK (std::abs (f.fadeIn + f.fadeOut - f.length()) < 1e-6 && std::abs (f.fadeIn - 2.0 * f.fadeOut) < 1e-6, "fades longer than the part fit it (%g + %g of %g)", f.fadeIn, f.fadeOut, f.length());
        const auto e = SampleRegion::of (s, 0.0f, 1.0f, 10.0f, 20.0f);
        CHECK (e.fadeIn == 480.0 && e.fadeOut == 960.0 && e.gain (0.0) == 0.0f && e.gain ((double) kLen) == 0.0f, "fades at the sample's own ends (%g %g)", e.fadeIn, e.fadeOut);
    }

    // ---- VOICE's slices in a region
    {
        SampleData s;
        s.buf.setSize (2, 96000);
        s.sr = 48000.0;
        s.slices = { 0, 9600, 24000, 43200, 72000 };            // 0, 0.2, 0.5, 0.9, 1.5 s
        auto layout = [&] (int mode, const SampleRegion& r)
        {
            std::vector<double> v;
            for (int k = 0; k < Slices::count (s, mode, r); ++k) v.push_back (Slices::start (s, mode, k, r));
            return v;
        };
        auto reg = [&] (double a, double b) { SampleRegion r; r.start = a; r.end = b; return r; };
        const auto whole = SampleRegion::whole (s);
        bool equalOld = true;
        for (int k = 0; k < 16; ++k) equalOld = equalOld && Slices::start (s, 2, k, whole) == (double) (int) ((long long) 96000 * k / 16);
        CHECK (layout (0, whole) == std::vector<double> ({ 0, 9600, 24000, 43200, 72000 }) && layout (3, whole) == std::vector<double> ({ 0 }) && equalOld,
               "the whole phrase: the slices as before");
        CHECK (layout (0, reg (28800, 96000)) == std::vector<double> ({ 28800, 43200, 72000 }), "AUTO: START first, then the onsets after it");
        CHECK (layout (0, reg (42000, 96000)) == std::vector<double> ({ 43200, 72000 }), "AUTO: an onset within 40 ms after START starts the first slice");
        CHECK (layout (0, reg (0, 50000)) == std::vector<double> ({ 0, 9600, 24000, 43200 }), "AUTO: up to END");
        CHECK (layout (0, reg (50000, 60000)) == std::vector<double> ({ 50000 }), "AUTO: no onset inside: one slice from START");
        CHECK (layout (1, reg (24000, 72000)) == std::vector<double> ({ 24000, 30000, 36000, 42000, 48000, 54000, 60000, 66000 }), "EQUAL 8: START..END in 8");
        CHECK (layout (3, reg (24000, 72000)) == std::vector<double> ({ 24000 }), "WHOLE: from START");
    }

    // The test samples: steps (110 Hz, 0.1 with a 0.5 middle from 0.3 to 0.6 s: START 20 % .. END
    // 40 %), flat (0.5 throughout, starting at 0), peak (a cosine: starting at its peak), burst
    // (silence but the 0.5 middle).
    const auto steps = writeWav ("sf_region_steps.wav", [] (int i) { return sine (i, i >= 14400 && i < 28800 ? 0.5f : 0.1f); });
    const auto flat = writeWav ("sf_region_flat.wav", [] (int i) { return sine (i, 0.5f); });
    const auto cosine = writeWav ("sf_region_peak.wav", [] (int i) { return 0.5f * (float) std::cos (2.0 * juce::MathConstants<double>::pi * 110.0 * i / 48000.0); });
    const auto burst = writeWav ("sf_region_burst.wav", [] (int i) { return i >= 14400 && i < 28800 ? sine (i, 0.5f) : 0.0f; });
    const Settings part { { "smp_start", 0.2f }, { "smp_end", 0.4f } };

    // ---- Every sampler: the part plays, and no more; the fades. Times in the sample's own seconds:
    // the synths play it at the pitch of the note against the root found in it (rate: the sample's
    // seconds per second), the drums and VOICE as recorded (TUNE 0).
    for (const char* id : kSamplers)
    {
        const std::string m (id);
        const bool rumble = m == "RUMBLE", synth = m == "BASS" || m == "LEAD" || m == "PAD";
        const auto a = render (id, steps, {});
        const auto b = render (id, steps, part);
        const auto [a0, a1] = span (a, 0.05f);
        const auto [b0, b1] = span (b, 0.05f);
        const double durA = (a1 - a0) / kSr, durB = (b1 - b0) / kSr, rate = synth ? 1.5 / durA : 1.0;
        auto in = [&] (double sampleSec) { return at (sampleSec / rate); };
        // The first 20-60 ms (of the sample) of each, from the same moment (the part's onset: the note
        // starts both alike): the whole's quiet start; the part's, from the loud middle, 5 times
        // louder (the module's own attack shapes both alike).
        const float lvA = peak (a, b0 + in (0.02), b0 + in (0.06)) / peak (a, 0, (int) a.size());
        const float louder = peak (b, b0 + in (0.02), b0 + in (0.06)) / std::max (1.0e-9f, peak (a, b0 + in (0.02), b0 + in (0.06)));
        const double wantA = rumble ? 0.4 : 1.5;               // RUMBLE: its exciter's 0.4 s of the sample
        std::printf ("   %-6s whole: %.3f s (rate %.2f), its start %.2f of its peak; START 20 %% END 40 %%: %.3f s, its start %.1f x louder\n", id, durA, rate, lvA, durB, louder);
        CHECK ((synth ? rate > 0.24 && rate < 4.1 : std::abs (durA - wantA) < 0.03) && lvA < 0.35, "%s at the defaults plays the whole sample from its start (%.3f s, %.2f)", id, durA, lvA);
        CHECK (std::abs (durB * rate - 0.3) < 0.02 && louder > 3.5f, "%s plays START..END (%.3f s of the sample's 0.3, its start %.1f x louder)", id, durB * rate, louder);

        const auto c = render (id, flat, part);
        const auto d = render (id, flat, { { "smp_start", 0.2f }, { "smp_end", 0.4f }, { "smp_fade_in", 100.0f }, { "smp_fade_out", 100.0f } });
        const auto [c0, c1] = span (c, 0.05f);
        auto ratio = [&] (int x0, int x1) { return peak (d, x0, x1) / std::max (1.0e-9f, peak (c, x0, x1)); };
        const float rIn = ratio (c0 + in (0.045), c0 + in (0.055)), rMid = ratio (c0 + in (0.14), c0 + in (0.16)), rOut = ratio (c1 - in (0.055), c1 - in (0.045));
        std::printf ("          FADE IN / OUT 100 ms: %.2f at 50 ms in, %.2f at 150 ms, %.2f 50 ms before END\n", rIn, rMid, rOut);
        CHECK (rIn > 0.3f && rIn < 0.75f && rMid > 0.9f && rMid < 1.1f && rOut > 0.3f && rOut < 0.75f, "%s FADE IN / OUT (%.2f %.2f %.2f)", id, rIn, rMid, rOut);
    }

    // ---- The fades are in the sample's own time: KICK an octave up plays the part in half the time.
    {
        const auto c = render ("KICK", flat, { { "smp_start", 0.2f }, { "smp_end", 0.4f }, { "tune", 12.0f } });
        const auto d = render ("KICK", flat, { { "smp_start", 0.2f }, { "smp_end", 0.4f }, { "tune", 12.0f }, { "smp_fade_in", 100.0f } });
        const auto [c0, c1] = span (c, 0.05f);
        const float rIn = peak (d, c0 + at (0.022), c0 + at (0.028)) / peak (c, c0 + at (0.022), c0 + at (0.028));
        const float rLate = peak (d, c0 + at (0.07), c0 + at (0.08)) / peak (c, c0 + at (0.07), c0 + at (0.08));
        std::printf ("   KICK TUNE +12: the part %.3f s; FADE IN 100 ms: %.2f at 25 ms, %.2f at 75 ms\n", (c1 - c0) / kSr, rIn, rLate);
        CHECK (std::abs ((c1 - c0) / kSr - 0.15) < 0.01 && rIn > 0.3f && rIn < 0.75f && rLate > 0.9f, "the region in the sample's own time");
    }

    // ---- No click where START and END cut into a waveform (at its peaks); the sample's own start at
    // a peak clicks (KICK, PERC), so the check would see one.
    for (const char* id : { "KICK", "PERC", "BASS", "VOICE" })
    {
        const bool bass = std::string (id) == "BASS";
        Settings cut { { "smp_start", 14509.0f / kLen }, { "smp_end", 28909.0f / kLen } };
        if (bass) { cut.push_back ({ "cutoff", 1.0f }); cut.push_back ({ "res", 0.0f }); }
        const auto g = render (id, flat, cut);
        const auto [g0, g1] = span (g, 0.02f);
        const float steady = maxStep (g, g0 + at (0.05), g1 - at (0.05));
        const float eOn = maxStep (g, g0 - at (0.001), g0 + at (0.004)) / steady, eOff = maxStep (g, g1 - at (0.004), g1 + at (0.002)) / steady;
        float own = 0.0f;
        if (! bass && std::string (id) != "VOICE")
        {
            const auto h = render (id, cosine, {});
            const auto [h0, h1] = span (h, 0.02f);
            own = maxStep (h, h0 - at (0.001), h0 + at (0.004)) / maxStep (h, h0 + at (0.05), h0 + at (0.25));
        }
        std::printf ("   %-6s the steepest step where START / END cut a peak: %.1f / %.1f x the waveform's own%s\n", id, eOn, eOff,
                     own > 0.0f ? (" (the sample's own start at a peak: " + juce::String (own, 1) + " x)").toRawUTF8() : "");
        CHECK (eOn < 4.0f && eOff < 4.0f, "%s clicks where START or END cut (%.1f, %.1f)", id, eOn, eOff);
        if (own > 0.0f) CHECK (own > 10.0f, "%s: the click check sees the sample's own click (%.1f)", id, own);
    }

    // ---- KICK tells Pocket a cut sample's length.
    {
        float dec[2] {};
        for (int k = 0; k < 2; ++k)
        {
            render ("KICK", flat, k == 0 ? Settings {} : Settings { { "smp_start", 0.2f }, { "smp_end", 0.25f } }, 0.5);
            KickHit hits[kKickRing];
            dec[k] = Bus::get().recentKicks (hits, kKickRing) > 0 ? hits[0].decay : -1.0f;
        }
        std::printf ("   KICK's decay for Pocket: the whole sample %.3f s, 75 ms of it %.3f s\n", dec[0], dec[1]);
        CHECK (dec[0] > 0.3f && dec[1] > 0.0f && dec[1] <= 0.076f, "KICK's Pocket decay of a cut sample (%.3f, %.3f)", dec[0], dec[1]);
    }

    // ---- PAD GRANULAR: the grains come from START..END, and follow them while a note holds.
    {
        const float granular = [] { SFProcessor p (makeModule ("PAD")); return choiceIndex (p, "model", "GRANULAR"); }();
        auto gr = [&] (float a, float b, const std::function<void (Rig2&, int)>& during = {})
        { return render ("PAD", burst, { { "model", granular }, { "smp_start", a }, { "smp_end", b }, { "cutoff", 1.0f }, { "res", 0.0f }, { "space", 0.0f } }, 2.6, during); };
        const auto loud = gr (0.2f, 0.4f), quiet = gr (0.5f, 0.8f), tiny = gr (0.25f, 0.2525f);
        const float l = rms (loud, at (1.15), at (2.25)), q = rms (quiet, at (1.15), at (2.25)), t = rms (tiny, at (1.15), at (2.25));
        auto moveAt1s = [] (float a, float b) { return [a, b] (Rig2& r, int s) { if (s <= at (1.0) && s + kBlock > at (1.0)) { r.set ("smp_start", a); r.set ("smp_end", b); } }; };
        const auto moved = gr (0.5f, 0.8f, moveAt1s (0.2f, 0.4f));
        const float before = rms (moved, at (0.8), at (1.0)), after = rms (moved, at (1.5), at (2.25));
        std::printf ("   PAD GRANULAR: the loud part %.4f, a silent part %.5f, 3.75 ms of the loud part %.4f; moved there while held: %.5f -> %.4f\n", l, q, t, before, after);
        CHECK (l > 0.02f && q < 0.01f * l && t > 0.1f * l && before < 0.01f * l && after > 0.3f * l, "GRANULAR's grains from START..END (%.4f %.5f %.4f %.5f %.4f)", l, q, t, before, after);
        // Moved away while held: the grains playing finish in the part they started in (none cut).
        const auto away = gr (0.2f, 0.4f, moveAt1s (0.5f, 0.8f));
        const float stepBefore = maxStep (away, at (0.88), at (1.0)), stepAfter = maxStep (away, at (1.0), at (1.3)), gone = rms (away, at (1.5), at (2.25));
        std::printf ("   PAD GRANULAR moved off the loud part while held: steepest step %.5f before, %.5f after; then %.5f\n", stepBefore, stepAfter, gone);
        CHECK (stepAfter <= 1.2f * stepBefore && gone < 0.01f * l, "GRANULAR: grains cut when the part moves (%.5f -> %.5f, then %.5f)", stepBefore, stepAfter, gone);
    }

    // ---- A hair off the defaults plays the defaults (to the bit); presets leave the region alone.
    {
        const auto a = render ("KICK", steps, {}, 1.0);
        const auto b = render ("KICK", steps, { { "smp_start", 1.0e-6f }, { "smp_end", 0.999999f }, { "smp_fade_in", 0.001f }, { "smp_fade_out", 0.001f } }, 1.0);
        CHECK (a == b, "a hair off the defaults plays the defaults");
        int moved = 0, presets = 0;
        for (const char* id : kSamplers)
            for (const auto& pr : factoryPresets (id))
            {
                SFProcessor p (makeModule (id));
                auto set = [&] (const char* pid, float v) { auto* prm = p.apvts.getParameter (pid); prm->setValueNotifyingHost (prm->convertTo0to1 (v)); };
                set ("smp_start", 0.3f); set ("smp_end", 0.6f); set ("smp_fade_in", 50.0f); set ("smp_fade_out", 70.0f);
                p.applyPreset (pr);
                auto val = [&] (const char* pid) { return p.apvts.getRawParameterValue (pid)->load(); };
                if (std::abs (val ("smp_start") - 0.3f) > 1e-5f || std::abs (val ("smp_end") - 0.6f) > 1e-5f
                    || std::abs (val ("smp_fade_in") - 50.0f) > 0.01f || std::abs (val ("smp_fade_out") - 70.0f) > 0.01f) ++moved;
                ++presets;
            }
        std::printf ("   %d factory presets of the samplers: %d moved the region\n", presets, moved);
        CHECK (presets > 0 && moved == 0, "presets leave START, END and the fades alone (%d of %d moved them)", moved, presets);
    }

    // ---- The editor: the SAMPLE group; the strip appears with a sample; START and END dragged.
    {
        SFEditor::testHeightLimit = 100000;
        for (const char* id : { "KICK", "PERC" })
        {
            SFProcessor p (makeModule (id));
            std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
            auto& e = dynamic_cast<SFEditor&> (*ed);
            std::map<std::string, juce::String> place;
            for (auto& x : e.places()) place[p.allSpecs()[(size_t) x.first].id] = x.second;
            const bool grouped = place["smp_start"] == "SAMPLE" && place["smp_end"] == "SAMPLE" && place["smp_fade_in"] == "SAMPLE" && place["smp_fade_out"] == "SAMPLE";
            const int own = std::string (id) == "KICK" ? 70 : 0;
            const int h0 = e.getHeight(), d0 = e.displayBounds().getHeight();
            p.mod().loadFiles (juce::StringArray (flat.getFullPathName()));
            juce::MessageManager::getInstance()->runDispatchLoopUntil (150);      // the editor's timer
            const int h1 = e.getHeight(), d1 = e.displayBounds().getHeight();
            std::printf ("   %s: SAMPLE group %s, folded %s; display %d -> %d px with a sample, window %d -> %d\n", id, grouped ? "yes" : "NO",
                         e.folded (SFEditor::kFoldSample) ? "yes" : "NO", d0, d1, h0, h1);
            CHECK (grouped && e.folded (SFEditor::kFoldSample) && d0 == own && d1 == own + SampleStrip::kHeight && h1 == h0 + SampleStrip::kHeight + (own == 0 ? 10 : 0),
                   "%s: the SAMPLE group and the strip (%d -> %d, %d -> %d)", id, d0, d1, h0, h1);
            const int hf = e.getHeight();
            e.toggleFold (SFEditor::kFoldSample);
            CHECK (! e.folded (SFEditor::kFoldSample) && e.getHeight() > hf, "%s: SAMPLE opens", id);

            // Drags on the body (its own coordinates), as the mouse sends them.
            auto& body = e.strip();
            const auto disp = body.getLocalArea (&e, e.displayBounds()).toFloat();
            const auto w = SampleStrip::wave (SampleStrip::frameIn (disp, own));
            auto val = [&] (const char* pid) { return p.apvts.getRawParameterValue (pid)->load(); };
            auto drag = [&] (float fromFrac, juce::Point<float> to)
            {
                const juce::Point<float> from (w.getX() + fromFrac * w.getWidth(), w.getCentreY());
                body.mouseDown (mouseAt (body, from, false));
                body.mouseDrag (mouseAt (body, (from + to) * 0.5f, true));
                body.mouseDrag (mouseAt (body, to, true));
                body.mouseUp (mouseAt (body, to, true));
            };
            auto x = [&] (float f) { return juce::Point<float> (w.getX() + f * w.getWidth(), w.getCentreY()); };
            drag (0.0f, x (0.25f));
            const float s1 = val ("smp_start");
            drag (1.0f, x (0.75f));
            const float e1 = val ("smp_end");
            drag (0.25f, x (0.9f));
            const float s2 = val ("smp_start");
            drag (0.74f, { disp.getX() - 60.0f, disp.getY() - 40.0f });         // out past the left edge, above the display
            const float s3 = val ("smp_start");
            std::printf ("   %s drags: START -> %.3f, END -> %.3f, START across END -> %.3f, START out past the edge -> %.3f\n", id, s1, e1, s2, s3);
            CHECK (std::abs (s1 - 0.25f) < 0.01f && std::abs (e1 - 0.75f) < 0.01f && s2 < e1 && s2 > e1 - 0.01f && s3 == 0.0f,
                   "%s: START and END dragged (%.3f %.3f %.3f %.3f)", id, s1, e1, s2, s3);
        }
        // VOICE: on the phrase itself; no phrase: a press changes nothing.
        {
            SFProcessor p (makeModule ("VOICE"));
            auto& m = p.mod();
            const juce::Rectangle<float> r (0.0f, 0.0f, 280.0f, 110.0f);
            const auto w = r.reduced (8.0f, 18.0f);
            m.displayMouse ({ w.getX(), w.getCentreY() }, r, true); m.displayMouse ({ w.getCentreX(), w.getCentreY() }, r, false); m.displayMouseUp();
            const float none = p.apvts.getRawParameterValue ("smp_start")->load();
            m.loadFiles (juce::StringArray (flat.getFullPathName()));
            m.displayMouse ({ w.getX(), w.getCentreY() }, r, true); m.displayMouse ({ w.getCentreX(), w.getCentreY() }, r, false); m.displayMouseUp();
            const float half = p.apvts.getRawParameterValue ("smp_start")->load();
            std::printf ("   VOICE: START dragged to the phrase's middle: %.3f (with no phrase: %.3f)\n", half, none);
            CHECK (none == 0.0f && std::abs (half - 0.5f) < 0.01f, "VOICE: START on the phrase (%.3f, %.3f)", none, half);
        }
        SFEditor::testHeightLimit = 0;
    }
    for (const auto& f : { steps, flat, cosine, burst }) f.deleteFile();
}
