// ISOLATOR, BUILD, GLUE, LOWEND, LIMIT, ROOMEQ
#include "Kit.h"
#include <deque>

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
    void prepare (double rate, int) override { sr = rate; for (auto& x : a) x.reset(); for (auto& x : b) x.reset(); for (auto& x : ap) x.reset(); for (auto& g : gz) g = 1.0f; }
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
            ui::levelBar (g, cell.withSizeKeepingCentre (cell.getWidth(), 6), std::min (v, 2.0f) * 0.5f);
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
    void prepare (double rate, int) override { sr = rate; fdn.prepare (rate); for (auto& h : hp) for (auto& f : h) f.reset(); armed = released = false; }
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
            if ((i & 15) == 0)
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
        ui::levelBar (g, in.removeFromLeft (in.getWidth() * 0.6f).withSizeKeepingCentre (in.getWidth() * 0.6f, 8), std::pow (10.0f, (disp.load() * 66.0f - 60.0f) / 20.0f));
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
    void prepare (double rate, int) override { sr = rate; for (auto& f : sc) f.reset(); env = 0; slow = 0; }
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
            const float over = std::max (0.0f, db - thr);
            const float gr = over * (1.0f - 1.0f / ratio);
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
    double sr = 48000.0;
    Svf sc[2];
    float env = 0, slow = 0;
    History<128> grHist;
    std::atomic<float> grNow { 0 };
};
ModulePtr makeGLUE() { return std::make_unique<GlueModule>(); }

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
    void prepare (double rate, int) override { sr = rate; for (auto& x : xo) x.reset(); for (auto& h : hp) for (auto& f : h) f.reset(); }
    void process (Ctx& c, juce::AudioBuffer<float>& io, juce::AudioBuffer<float>*, juce::AudioBuffer<float>*, juce::MidiBuffer&) override
    {
        for (int k = 0; k < 2; ++k) { xo[k].set (c.v (MONOF), sr); hp[k][0].set (c.v (RUMBLE), 0.5412f, sr); hp[k][1].set (c.v (RUMBLE), 1.3066f, sr); }
        const float clip = c.v (SUBCLIP), drv = 1.0f + clip * 4.0f, lowG = dbToGain (c.v (LOWTRIM)), w = c.v (WIDTH);
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
            if (clip > 0.001f) m = lerp (m, fastTanh (m * drv) / std::min (drv, 2.0f) * 1.1f, clip);
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
    std::atomic<float> lowPk { 0 };
};
ModulePtr makeLOWEND() { return std::make_unique<LowEndModule>(); }

// =====================================================================================  LIMIT
// Clipper + lookahead true-peak limiter + LUFS meter (BS.1770 K-weighting).
// AUTO trims the input slowly so short-term loudness sits at TARGET.
class LimitModule : public Module
{
public:
    enum { TARGET, CEILING, CLIP, INPUT, RELEASE, AUTO, RESET };
    std::string id() const override { return "LIMIT"; }
    std::vector<ParamSpec> params() const override
    {
        return { knob ("target", "TARGET", -16.0f, -5.0f, -9.0f, "LUFS").prim().noGlide(),
                 knob ("ceiling", "CEILING", -3.0f, -0.1f, -1.0f, "dB").prim().noGlide(),
                 pct ("clip", "CLIP", 0.3f).prim(),
                 knob ("input", "INPUT", -12.0f, 24.0f, 0.0f, "dB").prim(),
                 knob ("release", "RELEASE", 20.0f, 500.0f, 120.0f, "ms", 120.0f).prim(),
                 toggle ("auto", "AUTO").prim(),
                 momentary ("reset", "RESET").prim() };
    }
    int latency() const override { return look; }
    void prepare (double rate, int) override
    {
        sr = rate;
        look = (int) std::lround (0.0015 * rate);
        for (auto& d : dl) d.prepare (look + 8);
        win.assign ((size_t) look + 1, 1.0f);
        kShelf[0].highShelf (1681.97f, 0.7072f, 3.9998f, rate); kShelf[1] = kShelf[0];
        kHp[0].highPass (38.1355f, 0.5003f, rate); kHp[1] = kHp[0];
        blockLen = (int) (0.1 * rate); blockAcc = 0; blockCount = 0;
        blocks.clear(); gated.clear();
        gz = 1.0f; autoGainDb = 0.0f; tpMax = 0.0f;
        for (auto& p : prevS) p = 0;
    }
    void process (Ctx& c, juce::AudioBuffer<float>& io, juce::AudioBuffer<float>*, juce::AudioBuffer<float>*, juce::MidiBuffer&) override
    {
        if (c.b (RESET)) { gated.clear(); tpMax = 0.0f; }
        const float ceil = dbToGain (c.v (CEILING));
        const float clip = c.v (CLIP);
        const float rel = std::exp (-1.0f / (c.v (RELEASE) * 0.001f * (float) sr));
        const float attC = std::exp (-4.0f / (float) std::max (1, look));
        // Auto input: move toward TARGET at <= 1 dB/s while there is programme (> -40 LUFS).
        if (c.b (AUTO) && shortTerm > -40.0f)
        {
            const float err = c.v (TARGET) - shortTerm;
            autoGainDb += juce::jlimit (-1.0f, 1.0f, err) * (float) c.n / (float) sr;
            autoGainDb = juce::jlimit (-12.0f, 12.0f, autoGainDb);
        }
        const float inG = dbToGain (c.v (INPUT) + (c.b (AUTO) ? autoGainDb : 0.0f));
        float grMax = 0.0f;
        for (int i = 0; i < c.n; ++i)
        {
            float s[2];
            for (int k = 0; k < 2; ++k)
            {
                float x = io.getSample (k, i) * inG;
                if (clip > 0.001f) { const float cl = fastTanh (x / ceil) * ceil; x = lerp (x, cl, clip); }
                s[k] = x;
                // K-weighted loudness (on the post-input signal).
                const float kw = kHp[k].tick (kShelf[k].tick (x));
                blockAcc += kw * kw;
            }
            // Inter-sample peak estimate (Hermite at 3 points between samples).
            float pk = std::max (std::fabs (s[0]), std::fabs (s[1]));
            for (int k = 0; k < 2; ++k)
            {
                const float xm1 = prevS[k * 3], x0 = prevS[k * 3 + 1], x1 = prevS[k * 3 + 2], x2 = s[k];
                for (float f : { 0.25f, 0.5f, 0.75f })
                {
                    const float c1 = 0.5f * (x1 - xm1), c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2, c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
                    pk = std::max (pk, std::fabs (((c3 * f + c2) * f + c1) * f + x0));
                }
                prevS[k * 3] = x0; prevS[k * 3 + 1] = x1; prevS[k * 3 + 2] = x2;
            }
            const float need = pk > ceil ? ceil / pk : 1.0f;
            win[(size_t) wPos] = need; wPos = (wPos + 1) % (int) win.size();
            float mn = 1.0f;
            for (float v : win) mn = std::min (mn, v);
            gz = mn < gz ? mn + attC * (gz - mn) : mn + rel * (gz - mn);
            gz = std::min (gz, 1.0f);
            for (int k = 0; k < 2; ++k)
            {
                dl[k].push (s[k]);
                const float y = hardClip (dl[k].tap (look) * gz, ceil);
                io.setSample (k, i, y);
                tpMax = std::max (tpMax, std::fabs (y));
            }
            grMax = std::max (grMax, -gainToDb (gz));
            if (++blockCount >= blockLen) closeBlock();
        }
        dGr = grMax; dTp = gainToDb (tpMax); dIn = c.b (AUTO) ? autoGainDb : 0.0f;
    }
    int displayHeight() const override { return 90; }
    void paint (juce::Graphics& g, juce::Rectangle<float> r) override
    {
        ui::frame (g, r, "LOUDNESS");
        auto in = r.reduced (12, 18);
        const float cw = in.getWidth() / 5.0f;
        auto cell = [&] (const char* name, const juce::String& v, juce::Colour col)
        {
            auto c2 = in.removeFromLeft (cw);
            ui::label (g, name, c2.removeFromTop (14), ui::kFaint, 10.0f);
            ui::label (g, v, c2, col, 20.0f);
        };
        auto fmt = [] (float v) { return v < -69.0f ? juce::String ("--") : juce::String (v, 1); };
        cell ("SHORT-TERM", fmt (dShort.load()), ui::kText);
        cell ("INTEGRATED", fmt (dInt.load()), ui::kText);
        cell ("TRUE PEAK", fmt (dTp.load()), dTp.load() > -0.1f ? ui::kRed : ui::kText);
        cell ("GR", dGr.load() < 0.05f ? juce::String ("0.0") : juce::String (-dGr.load(), 1), dGr.load() > 6.0f ? ui::kAmber : ui::kText);
        cell ("AUTO IN", juce::String (dIn.load(), 1), ui::kDim);
    }
private:
    void closeBlock()
    {
        const float ms = blockAcc / (float) (blockCount * 2) * 2.0f;   // sum of both channels' mean squares
        blockAcc = 0; blockCount = 0;
        blocks.push_back (ms);
        if (blocks.size() > 30) blocks.pop_front();
        // Short-term: 3 s.
        float sum = 0; for (float b : blocks) sum += b;
        shortTerm = -0.691f + 10.0f * std::log10 (sum / (float) blocks.size() + 1e-12f);
        dShort = shortTerm;
        // Integrated: 400 ms blocks (4 x 100 ms, 75 % overlap), absolute -70 and relative -10 gates.
        if (blocks.size() >= 4)
        {
            float m = 0; for (size_t i = blocks.size() - 4; i < blocks.size(); ++i) m += blocks[i];
            m /= 4.0f;
            const float l = -0.691f + 10.0f * std::log10 (m + 1e-12f);
            if (l > -70.0f && gated.size() < 200000) gated.push_back (m);
            if (! gated.empty())
            {
                double s1 = 0; for (float v : gated) s1 += v;
                const double rel = -0.691 + 10.0 * std::log10 (s1 / (double) gated.size() + 1e-12) - 10.0;
                double s2 = 0; size_t n2 = 0;
                for (float v : gated) if (-0.691 + 10.0 * std::log10 (v + 1e-12) > rel) { s2 += v; ++n2; }
                dInt = n2 > 0 ? (float) (-0.691 + 10.0 * std::log10 (s2 / (double) n2 + 1e-12)) : -70.0f;
            }
        }
    }
    double sr = 48000.0;
    int look = 72, wPos = 0, blockLen = 4800, blockCount = 0;
    DelayLine dl[2];
    std::vector<float> win;
    Biquad kShelf[2], kHp[2];
    float blockAcc = 0, gz = 1.0f, autoGainDb = 0, tpMax = 0, shortTerm = -70.0f, prevS[6] {};
    std::deque<float> blocks;
    std::vector<float> gated;
    std::atomic<float> dShort { -70 }, dInt { -70 }, dTp { -70 }, dGr { 0 }, dIn { 0 };
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
    void prepare (double rate, int) override { sr = rate; for (auto& c : eq) for (auto& b : c) b.reset(); }
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
