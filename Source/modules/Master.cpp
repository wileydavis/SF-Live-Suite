// ISOLATOR, BUILD, GLUE, TONE, LOWEND, LIMIT, ROOMEQ
#include "Kit.h"
#include "../core/Loudness.h"
#include "../core/Oversample.h"
#include <complex>

namespace sf
{
// =====================================================================================  ISOLATOR
// 3-band DJ kill EQ. Knob: 0 = kill, 75% = 0 dB, 100% = +6 dB.
class IsolatorModule : public Module
{
public:
    enum { LOW, MID, HIGH, LOWKILL, MIDKILL, HIGHKILL, XLO, XHI };
    std::string id() const override { return "ISOLATOR"; }
    std::vector<ParamSpec> params() const override
    {
        return { pct ("low", "LOW", 0.75f).prim(), pct ("mid", "MID", 0.75f).prim(), pct ("high", "HIGH", 0.75f).prim(),
                 toggle ("low_kill", "LOW KILL").prim(), toggle ("mid_kill", "MID KILL").prim(), toggle ("high_kill", "HIGH KILL").prim(),
                 knob ("xover_lo", "XOVER LO", 60.0f, 500.0f, 200.0f, "Hz", 180.0f).prim(),
                 knob ("xover_hi", "XOVER HI", 1000.0f, 8000.0f, 2500.0f, "Hz", 2500.0f).prim() };
    }
    static float gainFor (float v) { return v <= 0.75f ? std::pow (v / 0.75f, 2.0f) : dbToGain ((v - 0.75f) / 0.25f * 6.0f); }
    void prepare (double rate, int) override { sr = rate; reset(); }
    void reset() override { for (auto& x : a) x.reset(); for (auto& x : b) x.reset(); for (auto& x : ap) x.reset(); }
    void process (Ctx& c, juce::AudioBuffer<float>& io, juce::AudioBuffer<float>*, juce::AudioBuffer<float>*, juce::MidiBuffer&) override
    {
        for (int k = 0; k < 2; ++k) { a[k].set (c.v (XLO), sr); b[k].set (c.v (XHI), sr); ap[k].set (c.v (XHI), sr); }
        const float tg[3] = { c.b (LOWKILL) ? 0.0f : gainFor (c.v (LOW)), c.b (MIDKILL) ? 0.0f : gainFor (c.v (MID)), c.b (HIGHKILL) ? 0.0f : gainFor (c.v (HIGH)) };
        const float s = std::exp (-1.0f / (float) (0.004 * sr));
        for (int i = 0; i < c.n; ++i)
        {
            for (int j = 0; j < 3; ++j) gz[j] = tg[j] + s * (gz[j] - tg[j]);
            for (int k = 0; k < 2; ++k)
            {
                float lo, rest, mid, hi, l1, l2;
                a[k].split (io.getSample (k, i), lo, rest);
                b[k].split (rest, mid, hi);
                ap[k].split (lo, l1, l2);           // keeps the low band phase-aligned with mid/high
                io.setSample (k, i, (l1 + l2) * gz[0] + mid * gz[1] + hi * gz[2]);
            }
        }
        for (int j = 0; j < 3; ++j) disp[j] = gz[j];
    }
    int displayHeight() const override { return 60; }
    void paint (juce::Graphics& g, juce::Rectangle<float> r) override
    {
        ui::frame (g, r, "BANDS");
        auto in = r.reduced (12, 20);
        const char* names[] = { "LOW", "MID", "HIGH" };
        for (int j = 0; j < 3; ++j)
        {
            auto cell = in.removeFromLeft (in.getWidth() / (float) (3 - j)).reduced (6, 0);
            const float v = disp[j].load();
            ui::label (g, names[j], cell.removeFromLeft (40), v < 0.01f ? ui::kRed : ui::kDim, 10.0f);
            ui::valueBar (g, cell.withSizeKeepingCentre (cell.getWidth(), 6), std::min (v, 2.0f) * 0.5f);
        }
    }
private:
    double sr = 48000.0;
    Crossover a[2], b[2], ap[2];
    float gz[3] { 1, 1, 1 };
    std::atomic<float> disp[3] { 1, 1, 1 };
};
ModulePtr makeISOLATOR() { return std::make_unique<IsolatorModule>(); }

// =====================================================================================  BUILD
// One knob for the whole mix: high-pass sweep, noise riser, reverb wash, width.
// DROP arms a reset that lands on the next bar line; the build stays off until BUILD is
// turned back below 5 %, so the knob can be returned without re-building.
class BuildModule : public Module
{
public:
    enum { BUILD, DROP, NOISE, SPACE, WIDTH, HPMAX, LEVEL };
    std::string id() const override { return "BUILD"; }
    std::vector<ParamSpec> params() const override
    {
        return { pct ("build", "BUILD", 0.0f).prim().dir (1),
                 momentary ("drop", "DROP").prim(),
                 pct ("noise", "NOISE", 0.4f).prim(),
                 pct ("space", "SPACE", 0.4f).prim(),
                 pct ("width", "WIDTH", 0.4f).prim(),
                 knob ("hp_max", "HP MAX", 200.0f, 4000.0f, 1200.0f, "Hz", 900.0f).prim(),
                 levelParam().prim() };
    }
    double tailSeconds() const override { return 8.0; }
    void prepare (double rate, int) override { sr = rate; fdn.prepare (rate); armed = released = false; reset(); }
    void reset() override { fdn.clear(); for (auto& h : hp) for (auto& f : h) f.reset(); nbp.reset(); }
    void process (Ctx& c, juce::AudioBuffer<float>& io, juce::AudioBuffer<float>*, juce::AudioBuffer<float>*, juce::MidiBuffer&) override
    {
        const float knobV = c.v (BUILD);
        if (c.b (DROP) && ! released) armed = true;
        if (released && knobV < 0.05f) released = false;
        int dropAt = -1;
        if (armed) { dropAt = c.playing ? c.nextGrid (c.beatsPerBar) : 0; if (dropAt >= 0) { armed = false; } }
        const float bTarget = released ? 0.0f : knobV;
        const float noise = c.v (NOISE), space = c.v (SPACE), width = c.v (WIDTH);
        fdn.set (0.9f, 6.0f, 0.3f, 0.4f, false);
        for (int i = 0; i < c.n; ++i)
        {
            if (i == dropAt) { released = true; bz = 0.0f; fdn.clear(); }
            const float t = released ? 0.0f : bTarget;
            bz = t + 0.999f * (bz - t);
            if (((c.samplePos + i) & 15) == 0)
            {
                const float hz = expMap (std::pow (bz, 1.5f), 20.0f, c.v (HPMAX));
                for (int k = 0; k < 2; ++k) { hp[k][0].set (hz, 0.7071f, sr); hp[k][1].set (hz, 0.9f + bz * 1.5f, sr); }
                nbp.set (expMap (bz, 400.0f, 9000.0f), 2.0f, sr);
            }
            const float nz = nbp.tickBP (rng.next()) * noise * bz * bz * 0.35f;
            float l = hp[0][1].tickHP (hp[0][0].tickHP (io.getSample (0, i)));
            float r = hp[1][1].tickHP (hp[1][0].tickHP (io.getSample (1, i)));
            float rl, rr;
            fdn.tick (l * bz * space, r * bz * space, rl, rr, false);
            l += rl + nz; r += rr + nz * 0.9f;
            const float m = 0.5f * (l + r), s = 0.5f * (l - r) * (1.0f + bz * width * 1.5f);
            const float lift = 1.0f + bz * 0.25f;
            io.setSample (0, i, (m + s) * lift);
            io.setSample (1, i, (m - s) * lift);
        }
        float g0; const float g1 = levelDb (c, LEVEL, g0);
        applyGain (io, g0, g1);
        disp = bz; dispArmed = armed; dispReleased = released;
    }
    int displayHeight() const override { return 60; }
    void paint (juce::Graphics& g, juce::Rectangle<float> r) override
    {
        ui::frame (g, r, "BUILD");
        auto in = r.reduced (12, 20);
        ui::valueBar (g, in.removeFromLeft (in.getWidth() * 0.6f).withSizeKeepingCentre (in.getWidth() * 0.6f, 8), disp.load());
        const juce::String st = dispArmed.load() ? "DROP ON BAR" : dispReleased.load() ? "RETURN KNOB" : "LIVE";
        ui::label (g, st, in, dispArmed.load() ? ui::kGreen : dispReleased.load() ? ui::kAmber : ui::kDim, 12.0f, juce::Justification::centredRight);
    }
private:
    double sr = 48000.0;
    Svf hp[2][2], nbp;
    Fdn fdn;
    Noise rng;
    float bz = 0.0f;
    bool armed = false, released = false;
    std::atomic<float> disp { 0 };
    std::atomic<bool> dispArmed { false }, dispReleased { false };
};
ModulePtr makeBUILD() { return std::make_unique<BuildModule>(); }

// =====================================================================================  GLUE
class GlueModule : public Module
{
public:
    enum { PUSH, ATTACK, RELEASE, RATIO, SCHPF, MIX, LEVEL };
    std::string id() const override { return "GLUE"; }
    std::vector<ParamSpec> params() const override
    {
        return { pct ("push", "PUSH", 0.3f).prim().dir (1),
                 choice ("attack", "ATTACK", { "0.1 ms", "0.3 ms", "1 ms", "3 ms", "10 ms", "30 ms" }, 3).prim(),
                 choice ("release", "RELEASE", { "0.1 s", "0.3 s", "0.6 s", "1.2 s", "AUTO" }, 4).prim(),
                 choice ("ratio", "RATIO", { "2:1", "4:1", "10:1" }, 0).prim(),
                 knob ("sc_hpf", "SC HPF", 20.0f, 300.0f, 90.0f, "Hz", 90.0f).prim(),
                 pct ("mix", "MIX", 1.0f).prim(),
                 levelParam().prim() };
    }
    void prepare (double rate, int) override { sr = rate; reset(); }
    void reset() override { for (auto& f : sc) f.reset(); env = 0; slow = 0; }
    void process (Ctx& c, juce::AudioBuffer<float>& io, juce::AudioBuffer<float>*, juce::AudioBuffer<float>*, juce::MidiBuffer&) override
    {
        static const float atk[] = { 0.0001f, 0.0003f, 0.001f, 0.003f, 0.01f, 0.03f };
        static const float rel[] = { 0.1f, 0.3f, 0.6f, 1.2f };
        static const float ratios[] = { 2.0f, 4.0f, 10.0f };
        const float push = c.v (PUSH);
        const float thr = -4.0f - push * 26.0f;
        const float ratio = ratios[juce::jlimit (0, 2, c.ci (RATIO))];
        const float makeup = -thr * (1.0f - 1.0f / ratio) * 0.45f;
        const float aC = std::exp (-1.0f / (atk[juce::jlimit (0, 5, c.ci (ATTACK))] * (float) sr));
        const bool autoRel = c.ci (RELEASE) == 4;
        const float rC = std::exp (-1.0f / (rel[std::min (3, c.ci (RELEASE))] * (float) sr));
        const float rFast = std::exp (-1.0f / (0.1f * (float) sr)), rSlow = std::exp (-1.0f / (1.5f * (float) sr));
        for (auto& f : sc) f.set (c.v (SCHPF), 0.7071f, sr);
        const float mix = c.v (MIX);
        float maxGr = 0.0f;
        for (int i = 0; i < c.n; ++i)
        {
            const float l = io.getSample (0, i), r = io.getSample (1, i);
            const float d = std::max (std::fabs (sc[0].tickHP (l)), std::fabs (sc[1].tickHP (r)));
            const float db = gainToDb (d);
            // 6 dB soft knee: the ratio fades in over threshold +-3 dB.
            const float over = db - thr;
            const float slope = 1.0f - 1.0f / ratio;
            const float gr = over <= -kKnee * 0.5f ? 0.0f
                           : over >= kKnee * 0.5f ? over * slope
                           : slope * (over + kKnee * 0.5f) * (over + kKnee * 0.5f) / (2.0f * kKnee);
            if (autoRel)
            {
                env = gr > env ? gr + aC * (env - gr) : gr + rFast * (env - gr);
                slow = gr > slow ? gr + aC * (slow - gr) : gr + rSlow * (slow - gr);
            }
            else env = gr > env ? gr + aC * (env - gr) : gr + rC * (env - gr);
            const float total = autoRel ? 0.6f * env + 0.4f * slow : env;
            maxGr = std::max (maxGr, total);
            const float g = dbToGain (makeup - total);
            io.setSample (0, i, l * (1.0f - mix) + l * g * mix);
            io.setSample (1, i, r * (1.0f - mix) + r * g * mix);
        }
        float g0; const float g1 = levelDb (c, LEVEL, g0);
        applyGain (io, g0, g1);
        grHist.push (clamp01 (maxGr / 12.0f));
        grNow = maxGr;
    }
    int displayHeight() const override { return 80; }
    void paint (juce::Graphics& g, juce::Rectangle<float> r) override
    {
        ui::frame (g, r, "GAIN REDUCTION");
        auto in = r.reduced (8, 18);
        juce::Path p;
        for (int i = 0; i < 128; ++i)
        {
            const float v = grHist.v[(grHist.w + i) % 128].load();
            const float x = in.getX() + in.getWidth() * (float) i / 127.0f, y = in.getY() + v * in.getHeight();
            if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        g.setColour (ui::kText); g.strokePath (p, juce::PathStrokeType (1.2f));
        ui::label (g, juce::String (-grNow.load(), 1) + " dB", r.reduced (8, 2).removeFromBottom (14), ui::kText, 11.0f, juce::Justification::centredRight);
    }
private:
    static constexpr float kKnee = 6.0f;
    double sr = 48000.0;
    Svf sc[2];
    float env = 0, slow = 0;
    History<128> grHist;
    std::atomic<float> grNow { 0 };
};
ModulePtr makeGLUE() { return std::make_unique<GlueModule>(); }

// =====================================================================================  TONE
// Master voicing EQ for techno: five moves that matter, at fixed, tuned frequencies, run 2x
// oversampled so the AIR shelf keeps its shape up to 20 kHz (no cramping near Nyquist).
//   LOW       shelf 70 Hz, +-6 dB       weight of kick and sub
//   MUD       bell 280 Hz, cut 0..6 dB  the boxy build-up of stacked parts
//   PRESENCE  bell 3.2 kHz, +-4 dB      hats, clap edges, voice
//   AIR       shelf 12 kHz, +-6 dB      top sheen
//   TILT      +-3 dB around 900 Hz      darker / brighter overall
class ToneModule : public Module
{
public:
    enum { LOW, MUD, PRES, AIR, TILT, LEVEL };
    std::string id() const override { return "TONE"; }
    std::vector<ParamSpec> params() const override
    {
        return { knob ("low", "LOW", -6.0f, 6.0f, 0.0f, "dB").prim(),
                 knob ("mud", "MUD", 0.0f, 6.0f, 0.0f, "dB").prim(),
                 knob ("presence", "PRESENCE", -4.0f, 4.0f, 0.0f, "dB").prim(),
                 knob ("air", "AIR", -6.0f, 6.0f, 0.0f, "dB").prim(),
                 knob ("tilt", "TILT", -3.0f, 3.0f, 0.0f, "dB").prim(),
                 levelParam().prim() };
    }
    void prepare (double rate, int) override { sr = rate; os.prepare (rate, 2); designed = false; reset(); }   // new rate: new coefficients
    void reset() override { os.reset(); for (auto& c : eq) for (auto& b : c) b.reset(); }
    void process (Ctx& c, juce::AudioBuffer<float>& io, juce::AudioBuffer<float>*, juce::AudioBuffer<float>*, juce::MidiBuffer&) override
    {
        const double osr = 2.0 * sr;
        float v[5] = { c.v (LOW), -c.v (MUD), c.v (PRES), c.v (AIR), c.v (TILT) };
        bool changed = false;
        for (int k = 0; k < 5; ++k) if (std::abs (v[k] - last[k]) > 1e-4f) { changed = true; last[k] = v[k]; }
        if (changed || ! designed)
        {
            designed = true;
            for (int ch = 0; ch < 2; ++ch)
            {
                eq[ch][0].lowShelf (70.0f, 0.7071f, v[0], osr);
                eq[ch][1].peak (280.0f, 1.0f, v[1], osr);
                eq[ch][2].peak (3200.0f, 0.8f, v[2], osr);
                eq[ch][3].highShelf (12000.0f, 0.7071f, v[3], osr);
                eq[ch][4].lowShelf (900.0f, 0.5f, -v[4], osr);
                eq[ch][5].highShelf (900.0f, 0.5f, v[4], osr);
            }
        }
        // Always through the oversampler, flat or not: switching the 2x path in and out would
        // move the phase of the highs (and click). It is flat to 20 kHz within 0.0001 dB.
        for (int ch = 0; ch < 2; ++ch)
        {
            auto& e = eq[ch];
            os.process (ch, io.getWritePointer (ch), c.n, [&e] (float x)
            {
                for (auto& b : e) x = b.tick (x);
                return x;
            });
        }
        float g0; const float g1 = levelDb (c, LEVEL, g0);
        applyGain (io, g0, g1);
        for (int k = 0; k < 5; ++k) disp[k] = v[k];
    }
    int displayHeight() const override { return 90; }
    void paint (juce::Graphics& g, juce::Rectangle<float> r) override
    {
        ui::frame (g, r, "RESPONSE");
        auto in = r.reduced (10, 18);
        g.setColour (ui::kGrid);
        g.drawHorizontalLine ((int) in.getCentreY(), in.getX(), in.getRight());
        // Magnitude of the six sections (analytic, at 2x rate).
        Biquad s[6];
        const double osr = 96000.0;
        s[0].lowShelf (70.0f, 0.7071f, disp[0].load(), osr);
        s[1].peak (280.0f, 1.0f, disp[1].load(), osr);
        s[2].peak (3200.0f, 0.8f, disp[2].load(), osr);
        s[3].highShelf (12000.0f, 0.7071f, disp[3].load(), osr);
        s[4].lowShelf (900.0f, 0.5f, -disp[4].load(), osr);
        s[5].highShelf (900.0f, 0.5f, disp[4].load(), osr);
        juce::Path p;
        for (int i = 0; i <= 200; ++i)
        {
            const float f = expMap ((float) i / 200.0f, 20.0f, 20000.0f);
            const double w = 2.0 * 3.14159265358979 * f / osr;
            const std::complex<double> z1 = std::polar (1.0, -w), z2 = z1 * z1;
            double db = 0.0;
            for (auto& b : s)
            {
                const auto num = (double) b.b0 + (double) b.b1 * z1 + (double) b.b2 * z2;
                const auto den = 1.0 + (double) b.a1 * z1 + (double) b.a2 * z2;
                db += 20.0 * std::log10 (std::abs (num / den) + 1e-12);
            }
            const float x = in.getX() + in.getWidth() * (float) i / 200.0f;
            const float y = in.getCentreY() - (float) juce::jlimit (-9.0, 9.0, db) / 9.0f * in.getHeight() * 0.5f;
            if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        g.setColour (ui::kText); g.strokePath (p, juce::PathStrokeType (1.5f));
        ui::label (g, "+9", in.withHeight (12), ui::kFaint, 9.0f);
        ui::label (g, "-9", in.withTrimmedTop (in.getHeight() - 12), ui::kFaint, 9.0f);
    }
private:
    double sr = 48000.0;
    Oversampler os;
    Biquad eq[2][6];
    float last[5] {};
    bool designed = false;
    std::atomic<float> disp[5] {};
};
ModulePtr makeTONE() { return std::make_unique<ToneModule>(); }

// =====================================================================================  LOWEND
class LowEndModule : public Module
{
public:
    enum { MONOF, SUBCLIP, RUMBLE, WIDTH, LOWTRIM, LEVEL };
    std::string id() const override { return "LOWEND"; }
    std::vector<ParamSpec> params() const override
    {
        return { knob ("mono_freq", "MONO FREQ", 40.0f, 300.0f, 120.0f, "Hz", 120.0f).prim(),
                 pct ("sub_clip", "SUB CLIP", 0.2f).prim(),
                 knob ("rumble_cut", "RUMBLE CUT", 10.0f, 45.0f, 25.0f, "Hz").prim(),
                 knob ("width", "WIDTH", 0.0f, 2.0f, 1.0f, "x").prim(),
                 knob ("low_trim", "LOW TRIM", -12.0f, 6.0f, 0.0f, "dB").prim(),
                 levelParam().prim() };
    }
    void prepare (double rate, int) override { sr = rate; up.set (HalfbandDesign::forStage (rate, 0)); dn.set (HalfbandDesign::forStage (rate, 0)); reset(); }
    void reset() override { for (auto& x : xo) x.reset(); for (auto& h : hp) for (auto& f : h) f.reset(); up.reset(); dn.reset(); }
    void process (Ctx& c, juce::AudioBuffer<float>& io, juce::AudioBuffer<float>*, juce::AudioBuffer<float>*, juce::MidiBuffer&) override
    {
        for (int k = 0; k < 2; ++k) { xo[k].set (c.v (MONOF), sr); hp[k][0].set (c.v (RUMBLE), 0.5412f, sr); hp[k][1].set (c.v (RUMBLE), 1.3066f, sr); }
        const float clip = c.v (SUBCLIP), drv = 1.0f + clip * 4.0f, lowG = dbToGain (c.v (LOWTRIM)), w = c.v (WIDTH);
        const float clipNorm = 1.1f / std::min (drv, 2.0f);
        auto sub = [=] (float m) { return lerp (m, fastTanh (m * drv) * clipNorm, clip); };
        float peakLow = 0.0f;
        for (int i = 0; i < c.n; ++i)
        {
            float lo[2], hi[2];
            for (int k = 0; k < 2; ++k)
            {
                const float x = hp[k][1].tickHP (hp[k][0].tickHP (io.getSample (k, i)));
                xo[k].split (x, lo[k], hi[k]);
            }
            float m = 0.5f * (lo[0] + lo[1]) * lowG;
            // SUB CLIP, 2x oversampled (always run, so the phase never changes with the knob).
            float a0, a1;
            up.up (m, a0, a1);
            m = dn.down (sub (a0), sub (a1));
            peakLow = std::max (peakLow, std::fabs (m));
            const float hm = 0.5f * (hi[0] + hi[1]), hs = 0.5f * (hi[0] - hi[1]) * w;
            io.setSample (0, i, m + hm + hs);
            io.setSample (1, i, m + hm - hs);
        }
        float g0; const float g1 = levelDb (c, LEVEL, g0);
        applyGain (io, g0, g1);
        lowPk = peakLow;
    }
    int displayHeight() const override { return 50; }
    void paint (juce::Graphics& g, juce::Rectangle<float> r) override
    {
        ui::frame (g, r, "MONO LOW");
        ui::levelBar (g, r.reduced (12, 20).withHeight (8), lowPk.load());
    }
private:
    double sr = 48000.0;
    Crossover xo[2];
    Svf hp[2][2];
    Halfband up, dn;
    std::atomic<float> lowPk { 0 };
};
ModulePtr makeLOWEND() { return std::make_unique<LowEndModule>(); }

// =====================================================================================  LIMIT
// Master stage: INPUT (+ AUTO trim) -> soft-knee CLIP at 4x, linear-phase FIR oversampling ->
// true-peak lookahead limiter -> CEILING.
//   Detection: 8x interpolated true peak (32-tap windowed sinc, stricter than BS.1770's 4x)
//   of the clipped signal.
//   Gain: min-hold over the 2 ms lookahead, program-dependent release (fast envelope at
//   RELEASE, held down by a slow envelope during sustained limiting: no pumping on dense
//   material, quick recovery after a single peak), then a 2 ms moving average, which is
//   guaranteed to have reached every peak's gain when that peak leaves the delay line.
//   Meters (output, BS.1770): momentary, short-term, integrated (gated), true peak.
// AUTO trims the input slowly (<= 1 dB/s) so the loudest short-term loudness of the last minute
// sits at TARGET; breakdowns are held, not lifted.
class LimitModule : public Module
{
public:
    enum { TARGET, CEILING, CLIP, INPUT, RELEASE, AUTO, RESET, AUTOMODE };
    std::string id() const override { return "LIMIT"; }
    std::vector<ParamSpec> params() const override
    {
        return { knob ("target", "TARGET", -16.0f, -5.0f, -9.0f, "LUFS").prim().noGlide(),
                 knob ("ceiling", "CEILING", -3.0f, -0.1f, -1.0f, "dB").prim().noGlide(),
                 pct ("clip", "CLIP", 0.3f).prim(),
                 knob ("input", "INPUT", -12.0f, 24.0f, 0.0f, "dB").prim(),
                 knob ("release", "RELEASE", 20.0f, 500.0f, 120.0f, "ms", 120.0f).prim(),
                 toggle ("auto", "AUTO").prim(),
                 momentary ("reset", "RESET").prim(),
                 choice ("auto_mode", "AUTO MODE", { "DOWN ONLY", "UP AND DOWN" }, 0) };
    }
    int latency() const override { return (int) std::lround (os.latency() + TruePeak::kDelay + look - 1); }
    double tailSeconds() const override { return 0.1; }
    void prepare (double rate, int) override
    {
        sr = rate;
        look = juce::jlimit (16, kMaxLook - 8, (int) std::lround (0.002 * rate));
        os.prepare (rate);
        for (auto& d : dl) d.prepare (TruePeak::kDelay + look + 8);
        outMeter.prepare (rate);
        preMeter.prepare (rate);
        autoGainDb = 0.0f; tpMax = 0.0f;
        for (auto& h : hist) h = -100.0f;
        histPos = 0; secMax = -100.0f; secLeft = (int) rate; liftWait = (int) (3.0 * rate);
        reset();
    }
    void reset() override
    {
        os.reset();
        for (auto& d : dl) d.clear();
        tpIn.reset(); tpOut.reset();
        for (int i = 0; i < kMaxLook; ++i) { minIdx[i] = 0; minVal[i] = 1.0f; boxRing[i] = 1.0f; }
        minHead = minTail = 0; t = 0;
        boxSum = (double) look; boxPos = 0;
        gFast = gSlow = 1.0f;
    }
    void process (Ctx& c, juce::AudioBuffer<float>& io, juce::AudioBuffer<float>*, juce::AudioBuffer<float>*, juce::MidiBuffer&) override
    {
        const bool rst = c.b (RESET);
        if (rst && ! resetWas) { outMeter.resetIntegrated(); tpMax = 0.0f; }
        resetWas = rst;
        const float ceilDb = c.v (CEILING), ceil = dbToGain (ceilDb), tpCeil = ceil * 0.985f;
        const float clip = c.v (CLIP);
        const float knee = ceil * dbToGain (-6.0f * clip), span = std::max (1e-6f, ceil - knee);
        const bool clipOn = clip > 0.01f;
        auto clipper = [=] (float x)
        {
            if (! clipOn) return x;
            const float a = std::fabs (x);
            return a <= knee ? x : std::copysign (knee + span * fastTanh ((a - knee) / span), x);
        };
        const float relT = c.v (RELEASE) * 0.001f;
        const float relFast = std::exp (-1.0f / (relT * (float) sr));
        const float relSlow = std::exp (-1.0f / (relT * 4.0f * (float) sr));
        const float attSlow = std::exp (-1.0f / (0.08f * (float) sr));
        // Auto input. preMeter measures the raw input (before INPUT and AUTO), so the gain AUTO
        // needs is known directly; there is no loop around the 3 s meter that could ring. AUTO
        // aims the LOUDEST short-term loudness of the last minute at TARGET, so a breakdown isn't
        // taken for the set getting quieter: the trim holds through it and the drop comes back
        // where it was. The trim moves down at <= 1 dB/s and up at <= 0.5 dB/s (proportional
        // over the last dB, so it settles without overshoot), and up only after 3 s of music within
        // 6 dB of TARGET (a breakdown longer than the minute is held too, and the meter has seen
        // only the new level when a drop comes back), and only while there is programme (> -40 LUFS). DOWN ONLY (default) never lifts above unity: AUTO only
        // stops the set running above TARGET. UP AND DOWN also lifts a quieter set.
        const float rawSt = preMeter.shortTerm;
        secMax = std::max (secMax, rawSt);
        if ((secLeft -= c.n) <= 0) { hist[histPos] = secMax; histPos = (histPos + 1) % kHist; secMax = -100.0f; secLeft += (int) sr; }
        float loud = secMax;
        for (const float h : hist) loud = std::max (loud, h);
        const float inLufs = rawSt + c.v (INPUT), loudLufs = loud + c.v (INPUT);
        if (c.b (AUTO) && inLufs > -40.0f)
        {
            float want = juce::jlimit (-12.0f, 12.0f, c.v (TARGET) - loudLufs);
            if (c.ci (AUTOMODE) == 0) want = std::min (0.0f, want);
            float step = want - autoGainDb;
            liftWait = inLufs + autoGainDb < c.v (TARGET) - 6.0f ? (int) (3.0 * sr) : std::max (0, liftWait - c.n);
            if (step > 0.0f && liftWait > 0) step = 0.0f;
            autoGainDb += juce::jlimit (-1.0f, 0.5f, step) * (float) c.n / (float) sr;
        }
        const float g0 = gIn, g1 = dbToGain (c.v (INPUT) + (c.b (AUTO) ? autoGainDb : 0.0f));
        gIn = g1;
        auto* L = io.getWritePointer (0); auto* R = io.getWritePointer (1);
        float grMax = 0.0f;
        const int delayC = TruePeak::kDelay + look - 1;
        for (int i = 0; i < c.n; ++i)
        {
            const float ig = g0 + (g1 - g0) * (float) i / (float) c.n;
            preMeter.push (L[i], R[i]);
            const float cl = os.process (0, L[i] * ig, clipper);
            const float cr = os.process (1, R[i] * ig, clipper);
            // Gain needed for the true peak of the clipped signal (TruePeak::kDelay samples ago),
            // aimed 0.13 dB under CEILING for the detector's residual error.
            const float tp = tpIn.push (cl, cr);
            const float need = tp > tpCeil ? tpCeil / tp : 1.0f;
            // Min-hold over the lookahead window (monotonic queue).
            while (minTail != minHead && minVal[(minTail - 1 + kMaxLook) % kMaxLook] >= need) minTail = (minTail - 1 + kMaxLook) % kMaxLook;
            minVal[minTail] = need; minIdx[minTail] = t; minTail = (minTail + 1) % kMaxLook;
            while (t - minIdx[minHead] >= look) minHead = (minHead + 1) % kMaxLook;
            const float hold = minVal[minHead];
            ++t;
            // Program-dependent release.
            gFast = hold < gFast ? hold : hold + relFast * (gFast - hold);
            gSlow = hold < gSlow ? hold + attSlow * (gSlow - hold) : hold + relSlow * (gSlow - hold);
            const float g = std::min (gFast, gSlow);
            // Moving average over the lookahead.
            boxSum += (double) g - (double) boxRing[boxPos];
            boxRing[boxPos] = g; boxPos = (boxPos + 1) % look;
            const float ga = std::min (1.0f, (float) (boxSum / (double) look));
            dl[0].push (cl); dl[1].push (cr);
            const float yl = hardClip (dl[0].tap (delayC) * ga, ceil), yr = hardClip (dl[1].tap (delayC) * ga, ceil);
            L[i] = yl; R[i] = yr;
            outMeter.push (yl, yr);
            tpMax = std::max (tpMax, tpOut.push (yl, yr));
            grMax = std::max (grMax, -gainToDb (ga));
        }
        dGr = grMax; dTp = gainToDb (tpMax); dIn = c.b (AUTO) ? autoGainDb : 0.0f;
        dMom = outMeter.momentary; dShort = outMeter.shortTerm; dInt = outMeter.integrated;
    }
    int displayHeight() const override { return 90; }
    void paint (juce::Graphics& g, juce::Rectangle<float> r) override
    {
        ui::frame (g, r, "LOUDNESS (OUTPUT, BS.1770)");
        auto in = r.reduced (12, 18);
        const float cw = in.getWidth() / 6.0f;
        auto cell = [&] (const char* name, const juce::String& v, juce::Colour col)
        {
            auto c2 = in.removeFromLeft (cw);
            ui::label (g, name, c2.removeFromTop (14), ui::kFaint, 10.0f);
            ui::label (g, v, c2, col, 20.0f);
        };
        auto fmt = [] (float v) { return v < -69.0f ? juce::String ("--") : juce::String (v, 1); };
        cell ("MOMENTARY", fmt (dMom.load()), ui::kText);
        cell ("SHORT-TERM", fmt (dShort.load()), ui::kText);
        cell ("INTEGRATED", fmt (dInt.load()), ui::kText);
        cell ("TRUE PEAK", fmt (dTp.load()), dTp.load() > -0.1f ? ui::kRed : ui::kText);
        cell ("GR", dGr.load() < 0.05f ? juce::String ("0.0") : juce::String (-dGr.load(), 1), dGr.load() > 6.0f ? ui::kAmber : ui::kText);
        cell ("AUTO IN", juce::String (dIn.load(), 1), ui::kDim);
    }
    // Tests
    float integratedLufs() const { return dInt.load(); }
    float truePeakDb() const { return dTp.load(); }

private:
    static constexpr int kMaxLook = 1024;
    double sr = 48000.0;
    int look = 96;
    FirOversampler4x os;
    DelayLine dl[2];
    TruePeak tpIn, tpOut;
    LoudnessMeter outMeter, preMeter;
    long long minIdx[kMaxLook] {}, t = 0;
    int minHead = 0, minTail = 0, boxPos = 0;
    float minVal[kMaxLook] {}, boxRing[kMaxLook] {};
    double boxSum = 96.0;
    float gFast = 1.0f, gSlow = 1.0f, gIn = 1.0f, autoGainDb = 0, tpMax = 0;
    static constexpr int kHist = 60;                 // AUTO's memory: loudest raw short-term per second
    float hist[kHist] {}, secMax = -100.0f;
    int histPos = 0, secLeft = 48000, liftWait = 144000;
    bool resetWas = false;
    std::atomic<float> dShort { -70 }, dInt { -70 }, dMom { -70 }, dTp { -70 }, dGr { 0 }, dIn { 0 };
};
ModulePtr makeLIMIT() { return std::make_unique<LimitModule>(); }

// =====================================================================================  ROOMEQ
// Cut-only correction for the room / PA, recalled per venue (16 stored venues).
class RoomEqModule : public Module, private juce::Timer
{
public:
    static constexpr int kB = 6;
    enum { CUT1 = 0, Q = 6, VENUE = 7, FREQ1 = 8, STOREV = 14 };
    RoomEqModule() { for (auto& v : venueHas) v = false; startTimerHz (10); }
    ~RoomEqModule() override { stopTimer(); }
    std::string id() const override { return "ROOMEQ"; }
    std::vector<ParamSpec> params() const override
    {
        static const float f[kB] = { 60, 120, 250, 500, 2000, 6000 };
        std::vector<ParamSpec> p;
        for (int b = 0; b < kB; ++b)
        {
            auto s = knob (("cut" + std::to_string (b + 1)).c_str(), ("CUT " + std::to_string (b + 1)).c_str(), 0.0f, 18.0f, 0.0f, "dB");
            s.primary = true; s.glide = false; s.song = false;
            p.push_back (s);
        }
        auto q = knob ("q", "Q", 0.5f, 8.0f, 2.0f, "", 2.0f); q.primary = true; q.song = false; p.push_back (q);
        std::vector<std::string> venues; for (int i = 1; i <= 16; ++i) venues.push_back ("VENUE " + std::to_string (i));
        auto v = choice ("venue", "VENUE", venues, 0); v.primary = true; v.song = false; p.push_back (v);
        for (int b = 0; b < kB; ++b)
        {
            auto s = knob (("freq" + std::to_string (b + 1)).c_str(), ("FREQ " + std::to_string (b + 1)).c_str(), 30.0f, 16000.0f, f[b], "Hz", 800.0f);
            s.glide = false; s.song = false;
            p.push_back (s);
        }
        p.push_back (momentary ("store_venue", "STORE VENUE"));
        return p;
    }
    void prepare (double rate, int) override { sr = rate; reset(); }
    void reset() override { for (auto& c : eq) for (auto& b : c) b.reset(); }
    void process (Ctx& c, juce::AudioBuffer<float>& io, juce::AudioBuffer<float>*, juce::AudioBuffer<float>*, juce::MidiBuffer&) override
    {
        for (int b = 0; b < kB; ++b)
        {
            const float cut = c.v (CUT1 + b);
            active[b] = cut > 0.05f;
            if (! active[b]) continue;
            for (int k = 0; k < 2; ++k)
            {
                eq[k][b].peak (c.v (FREQ1 + b), c.v (Q), -cut, sr);
                auto* x = io.getWritePointer (k);
                for (int i = 0; i < c.n; ++i) x[i] = eq[k][b].tick (x[i]);
            }
        }
        for (int b = 0; b < kB; ++b) { dF[b] = c.v (FREQ1 + b); dC[b] = c.v (CUT1 + b); }
        dQ = c.v (Q);
    }
    void save (juce::ValueTree& t) const override
    {
        for (int v = 0; v < 16; ++v)
        {
            if (! venueHas[(size_t) v]) continue;
            juce::ValueTree vt ("VENUE");
            vt.setProperty ("index", v, nullptr);
            juce::String s; for (float x : venues[(size_t) v]) s << x << ",";
            vt.setProperty ("values", s, nullptr);
            t.appendChild (vt, nullptr);
        }
    }
    void load (const juce::ValueTree& t) override
    {
        for (auto vt : t)
        {
            if (vt.getType().toString() != "VENUE") continue;
            const int v = (int) vt.getProperty ("index", -1);
            if (v < 0 || v >= 16) continue;
            auto toks = juce::StringArray::fromTokens (vt.getProperty ("values").toString(), ",", "");
            for (int i = 0; i < 13 && i < toks.size(); ++i) venues[(size_t) v][(size_t) i] = toks[i].getFloatValue();
            venueHas[(size_t) v] = true;
        }
    }
    int displayHeight() const override { return 100; }
    void paint (juce::Graphics& g, juce::Rectangle<float> r) override
    {
        ui::frame (g, r, "RESPONSE (CUT ONLY)");
        auto in = r.reduced (10, 18);
        g.setColour (ui::kGrid);
        g.drawHorizontalLine ((int) in.getY() + 2, in.getX(), in.getRight());
        juce::Path p;
        for (int i = 0; i <= 200; ++i)
        {
            const float f = expMap ((float) i / 200.0f, 20.0f, 20000.0f);
            float db = 0.0f;
            for (int b = 0; b < kB; ++b)
            {
                const float c = dC[b].load(); if (c <= 0.05f) continue;
                const float oct = std::log2 (f / dF[b].load());
                const float bw = 1.0f / std::max (0.3f, dQ.load());
                db -= c / (1.0f + std::pow (oct / (bw * 0.5f), 2.0f));
            }
            const float x = in.getX() + in.getWidth() * (float) i / 200.0f, y = in.getY() + 2 + (-db / 18.0f) * (in.getHeight() - 4);
            if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        g.setColour (ui::kText); g.strokePath (p, juce::PathStrokeType (1.5f));
        ui::label (g, venueHas[(size_t) lastVenue] ? "STORED" : "NOT STORED", r.reduced (8, 2).removeFromBottom (14),
                   venueHas[(size_t) lastVenue] ? ui::kGreen : ui::kFaint, 10.0f, juce::Justification::centredRight);
    }
private:
    void timerCallback() override
    {
        if (! getParam) return;
        const int v = (int) getParam (VENUE);
        const bool store = getParam (STOREV) > 0.5f;
        if (store && ! storeWas)
        {
            for (int b = 0; b < kB; ++b) { venues[(size_t) v][(size_t) b] = getParam (CUT1 + b); venues[(size_t) v][(size_t) (6 + b)] = getParam (FREQ1 + b); }
            venues[(size_t) v][12] = getParam (Q);
            venueHas[(size_t) v] = true;
        }
        storeWas = store;
        if (v != lastVenue)
        {
            lastVenue = v;
            if (venueHas[(size_t) v] && setParam)
            {
                for (int b = 0; b < kB; ++b) { setParam (CUT1 + b, venues[(size_t) v][(size_t) b]); setParam (FREQ1 + b, venues[(size_t) v][(size_t) (6 + b)]); }
                setParam (Q, venues[(size_t) v][12]);
            }
        }
    }
    double sr = 48000.0;
    Biquad eq[2][kB];
    bool active[kB] {};
    std::array<std::array<float, 13>, 16> venues {};
    std::array<bool, 16> venueHas {};
    int lastVenue = 0;
    bool storeWas = false;
    std::atomic<float> dF[kB] {}, dC[kB] {}, dQ { 2.0f };
};
ModulePtr makeROOMEQ() { return std::make_unique<RoomEqModule>(); }
} // namespace sf
