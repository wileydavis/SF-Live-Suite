#pragma once
// Small DSP toolkit shared by every module. Header-only, allocation-free after prepare().
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace sf
{
constexpr float kPi = 3.14159265358979f;
constexpr float kTwoPi = 6.28318530717959f;

inline float dbToGain (float db)       { return std::pow (10.0f, db * 0.05f); }
inline float gainToDb (float g)        { return 20.0f * std::log10 (std::max (g, 1.0e-7f)); }
inline float clamp01 (float x)         { return std::min (1.0f, std::max (0.0f, x)); }
inline float lerp (float a, float b, float t) { return a + (b - a) * t; }
inline float smoothstep (float e0, float e1, float x) { const float t = clamp01 ((x - e0) / (e1 - e0)); return t * t * (3.0f - 2.0f * t); }
inline float expMap (float t, float lo, float hi) { return lo * std::pow (hi / lo, clamp01 (t)); }
inline float noteHz (float note)       { return 440.0f * std::pow (2.0f, (note - 69.0f) / 12.0f); }
inline float semis (float st)          { return std::pow (2.0f, st / 12.0f); }

// Soft saturators
inline float fastTanh (float x)
{
    x = std::max (-3.0f, std::min (3.0f, x));
    return x * (27.0f + x * x) / (27.0f + 9.0f * x * x);
}
inline float softClip (float x)        { return fastTanh (x); }
inline float hardClip (float x, float c = 1.0f) { return std::max (-c, std::min (c, x)); }

// Tempo divisions (beats). Index lists are shared by every "TIME"/"RATE" menu.
struct Division { const char* name; double beats; };
inline const std::vector<Division>& divisions()
{
    static const std::vector<Division> d {
        { "4 BAR", 16.0 }, { "2 BAR", 8.0 }, { "1 BAR", 4.0 }, { "1/2", 2.0 }, { "1/4", 1.0 }, { "1/4D", 1.5 }, { "1/4T", 2.0 / 3.0 },
        { "1/8", 0.5 }, { "1/8D", 0.75 }, { "1/8T", 1.0 / 3.0 }, { "1/16", 0.25 }, { "1/16D", 0.375 }, { "1/16T", 1.0 / 6.0 }, { "1/32", 0.125 } };
    return d;
}
inline std::vector<std::string> divisionNames (int from = 0, int to = -1)
{
    std::vector<std::string> v;
    const auto& d = divisions();
    if (to < 0) to = (int) d.size() - 1;
    for (int i = from; i <= to; ++i) v.push_back (d[(size_t) i].name);
    return v;
}
inline double divisionBeats (int idx, int from = 0)
{
    const auto& d = divisions();
    return d[(size_t) std::max (0, std::min ((int) d.size() - 1, idx + from))].beats;
}
inline int divIndex (const char* name)
{
    const auto& d = divisions();
    for (size_t i = 0; i < d.size(); ++i) if (std::string (d[i].name) == name) return (int) i;
    return 0;
}

struct Noise
{
    uint32_t s = 0x9E3779B9u;
    float next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (float) (int32_t) s * (1.0f / 2147483648.0f); }
    float uni()  { return 0.5f + 0.5f * next(); }
};

struct OnePole
{
    float a = 0.0f, z = 0.0f;
    void setTime (float seconds, double sr) { a = seconds <= 0.0f ? 0.0f : std::exp (-1.0f / (float) (seconds * sr)); }
    void setHz (float hz, double sr)        { a = std::exp (-kTwoPi * hz / (float) sr); }
    float lp (float x)                      { z = x + a * (z - x); return z; }
    float hp (float x)                      { return x - lp (x); }
    void reset (float v = 0.0f)             { z = v; }
};

// Attack/release envelope follower (linear magnitude).
struct Follower
{
    float att = 0.0f, rel = 0.0f, z = 0.0f;
    void set (float attS, float relS, double sr)
    {
        att = std::exp (-1.0f / (float) (std::max (1.0e-5f, attS) * sr));
        rel = std::exp (-1.0f / (float) (std::max (1.0e-5f, relS) * sr));
    }
    float tick (float x)
    {
        x = std::fabs (x);
        z = x > z ? x + att * (z - x) : x + rel * (z - x);
        return z;
    }
};

// Topology-preserving state-variable filter (Simper). Safe under fast modulation.
struct Svf
{
    float g = 0, k = 1, a1 = 0, a2 = 0, a3 = 0, ic1 = 0, ic2 = 0;
    float lp = 0, bp = 0, hp = 0;
    void set (float hz, float q, double sr)
    {
        hz = std::min (std::max (hz, 10.0f), 0.49f * (float) sr);
        g = std::tan (kPi * hz / (float) sr);
        k = 1.0f / std::max (q, 0.05f);
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }
    void tick (float x)
    {
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        lp = v2; bp = v1; hp = x - k * v1 - v2;
    }
    float tickLP (float x) { tick (x); return lp; }
    float tickHP (float x) { tick (x); return hp; }
    float tickBP (float x) { tick (x); return bp; }
    void reset() { ic1 = ic2 = 0; }
};

// RBJ biquad, transposed direct form II.
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    float tick (float x)
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
    void reset() { z1 = z2 = 0; }
    void norm (float a0) { b0 /= a0; b1 /= a0; b2 /= a0; a1 /= a0; a2 /= a0; }
    void peak (float hz, float q, float db, double sr)
    {
        const float A = std::pow (10.0f, db / 40.0f), w = kTwoPi * std::min (hz, 0.49f * (float) sr) / (float) sr;
        const float al = std::sin (w) / (2.0f * q), c = std::cos (w);
        b0 = 1 + al * A; b1 = -2 * c; b2 = 1 - al * A; a1 = -2 * c; a2 = 1 - al / A; norm (1 + al / A);
    }
    void lowShelf (float hz, float q, float db, double sr)
    {
        const float A = std::pow (10.0f, db / 40.0f), w = kTwoPi * hz / (float) sr, c = std::cos (w), s = std::sin (w);
        const float al = s / (2.0f * q), sq = 2.0f * std::sqrt (A) * al;
        b0 = A * ((A + 1) - (A - 1) * c + sq); b1 = 2 * A * ((A - 1) - (A + 1) * c); b2 = A * ((A + 1) - (A - 1) * c - sq);
        a1 = -2 * ((A - 1) + (A + 1) * c); a2 = (A + 1) + (A - 1) * c - sq; norm ((A + 1) + (A - 1) * c + sq);
    }
    void highShelf (float hz, float q, float db, double sr)
    {
        const float A = std::pow (10.0f, db / 40.0f), w = kTwoPi * hz / (float) sr, c = std::cos (w), s = std::sin (w);
        const float al = s / (2.0f * q), sq = 2.0f * std::sqrt (A) * al;
        b0 = A * ((A + 1) + (A - 1) * c + sq); b1 = -2 * A * ((A - 1) + (A + 1) * c); b2 = A * ((A + 1) + (A - 1) * c - sq);
        a1 = 2 * ((A - 1) - (A + 1) * c); a2 = (A + 1) - (A - 1) * c - sq; norm ((A + 1) - (A - 1) * c + sq);
    }
    void lowPass (float hz, float q, double sr)
    {
        const float w = kTwoPi * std::min (hz, 0.49f * (float) sr) / (float) sr, al = std::sin (w) / (2 * q), c = std::cos (w);
        b0 = (1 - c) / 2; b1 = 1 - c; b2 = (1 - c) / 2; a1 = -2 * c; a2 = 1 - al; norm (1 + al);
    }
    void highPass (float hz, float q, double sr)
    {
        const float w = kTwoPi * std::min (hz, 0.49f * (float) sr) / (float) sr, al = std::sin (w) / (2 * q), c = std::cos (w);
        b0 = (1 + c) / 2; b1 = -(1 + c); b2 = (1 + c) / 2; a1 = -2 * c; a2 = 1 - al; norm (1 + al);
    }
};

// 4th-order Linkwitz-Riley split (two cascaded Butterworth SVF sections per side).
struct Crossover
{
    Svf l1, l2, h1, h2;
    void set (float hz, double sr) { l1.set (hz, 0.7071f, sr); l2.set (hz, 0.7071f, sr); h1.set (hz, 0.7071f, sr); h2.set (hz, 0.7071f, sr); }
    void split (float x, float& lo, float& hi) { lo = l2.tickLP (l1.tickLP (x)); hi = h2.tickHP (h1.tickHP (x)); }
    void reset() { l1.reset(); l2.reset(); h1.reset(); h2.reset(); }
};

// Bipolar performance filter: knob 0..1, 0.5 = open. Left = low-pass, right = high-pass.
// 24 dB/oct. Both chains run all the time and the output crossfades low-pass <-> dry <-> high-pass
// around the centre, with resonance and its level compensation fading in as the knob leaves the
// centre, so crossing the middle is seamless (no switch, no click).
struct DJFilter
{
    Svf lp[2][2], hp[2][2];
    float wLp = 0.0f, wHp = 0.0f, kz = 0.5f;
    int counter = 0;
    double sr = 48000.0;
    float wCoef = 0.999f;
    void prepare (double rate)
    {
        sr = rate;
        wCoef = std::exp (-1.0f / (float) (0.004 * rate));
        reset();
    }
    void reset() { for (auto& c : lp) for (auto& f : c) f.reset(); for (auto& c : hp) for (auto& f : c) f.reset(); }
    static float lpHz (float k) { return expMap (k / 0.5f, 60.0f, 22000.0f); }           // k in [0, .5]
    static float hpHz (float k) { return expMap ((k - 0.5f) / 0.5f, 18.0f, 12000.0f); }  // k in [.5, 1]
    static float lpWeight (float k) { return smoothstep (0.5f, 0.465f, k); }
    static float hpWeight (float k) { return smoothstep (0.5f, 0.535f, k); }
    // Equivalent corner frequencies (for kick reports): highpass corner / lowpass corner.
    static void corners (float k, float& hpc, float& lpc)
    {
        hpc = k > 0.52f ? hpHz (k) : 0.0f;
        lpc = k < 0.48f ? lpHz (k) : 22000.0f;
    }
    void process (float* const* ch, int numCh, int n, float k0, float k1, float res)
    {
        res = clamp01 (res);
        for (int i = 0; i < n; ++i)
        {
            const float k = k0 + (k1 - k0) * (float) i / (float) std::max (1, n);
            float l = ch[0][i], r = numCh > 1 ? ch[1][i] : l;
            step (k, res, l, r);
            ch[0][i] = l;
            if (numCh > 1) ch[1][i] = r;
        }
    }
    // One stereo sample. knee = false inside feedback loops (the loop has its own saturation).
    void step (float k, float res, float& l, float& r, bool knee = true)
    {
        // Weights glide (4 ms) so block-wise knob values never step the mix.
        wLp = lpWeight (k) + wCoef * (wLp - lpWeight (k));
        wHp = hpWeight (k) + wCoef * (wHp - hpWeight (k));
        if ((counter++ & 7) == 0)
        {
            kz = k;
            const float lpf = lpHz (std::min (k, 0.5f)), hpf = hpHz (std::max (k, 0.5f));
            const float qL = 0.7071f + res * 4.5f * wLp, qH = 0.7071f + res * 4.5f * wHp;
            for (int c = 0; c < 2; ++c)
            {
                lp[c][0].set (lpf, 0.5412f, sr); lp[c][1].set (lpf, qL, sr);
                hp[c][0].set (hpf, 0.5412f, sr); hp[c][1].set (hpf, qH, sr);
            }
        }
        const float gL = 1.0f / (1.0f + res * 0.9f * wLp), gH = 1.0f / (1.0f + res * 0.9f * wHp);
        const float wDry = std::max (0.0f, 1.0f - wLp - wHp);
        float* xs[2] = { &l, &r };
        for (int c = 0; c < 2; ++c)
        {
            const float x = *xs[c];
            const float yl = lp[c][1].tickLP (lp[c][0].tickLP (x)) * gL;
            const float yh = hp[c][1].tickHP (hp[c][0].tickHP (x)) * gH;
            // Transparent below 0.9; a soft knee above catches resonant peaks.
            const float y = x * wDry + yl * wLp + yh * wHp;
            const float a = std::fabs (y);
            *xs[c] = a <= 0.9f || ! knee ? y : std::copysign (0.9f + 0.1f * fastTanh ((a - 0.9f) * 10.0f), y);
        }
    }
};

// Fractional delay line with 4-point Hermite interpolation.
struct DelayLine
{
    std::vector<float> buf;
    int mask = 0, w = 0;
    void prepare (int maxSamples)
    {
        int size = 1;
        while (size < maxSamples + 4) size <<= 1;
        buf.assign ((size_t) size, 0.0f);
        mask = size - 1; w = 0;
    }
    void clear() { std::fill (buf.begin(), buf.end(), 0.0f); }
    void push (float x) { buf[(size_t) w] = x; w = (w + 1) & mask; }
    float tap (int d) const { return buf[(size_t) ((w - 1 - d) & mask)]; }
    float read (float d) const
    {
        d = std::max (1.0f, std::min (d, (float) (mask - 3)));
        const int di = (int) d;
        const float f = d - (float) di;
        const float xm1 = tap (di - 1), x0 = tap (di), x1 = tap (di + 1), x2 = tap (di + 2);
        const float c1 = 0.5f * (x1 - xm1), c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2, c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * f + c2) * f + c1) * f + x0;
    }
};

// PolyBLEP oscillator.
struct Osc
{
    double phase = 0.0;
    float last = 0.0f;
    static float blep (double t, double dt)
    {
        if (t < dt) { t /= dt; return (float) (t + t - t * t - 1.0); }
        if (t > 1.0 - dt) { t = (t - 1.0) / dt; return (float) (t * t + t + t + 1.0); }
        return 0.0f;
    }
    // shape: 0 saw, 1 square/pulse, 2 triangle, 3 sine
    float tick (double hz, double sr, int shape, float pw = 0.5f)
    {
        const double dt = std::min (0.45, hz / sr);
        double t = phase;
        float y = 0.0f;
        switch (shape)
        {
            case 0: y = (float) (2.0 * t - 1.0) - blep (t, dt); break;
            case 1:
            {
                y = t < pw ? 1.0f : -1.0f;
                y += blep (t, dt);
                double t2 = t - pw; if (t2 < 0) t2 += 1.0;
                y -= blep (t2, dt);
                break;
            }
            case 2:
            {
                float sq = t < 0.5 ? 1.0f : -1.0f;
                sq += blep (t, dt);
                double t2 = t + 0.5; if (t2 >= 1.0) t2 -= 1.0;
                sq -= blep (t2, dt);
                last = (float) (dt * 4.0) * sq + (1.0f - (float) dt * 0.05f) * last;
                y = last;
                break;
            }
            default: y = std::sin (kTwoPi * (float) t); break;
        }
        phase += dt;
        if (phase >= 1.0) phase -= 1.0;
        return y;
    }
};

// Zero-delay-feedback 4-pole ladder (Moog/303-ish), tanh in the feedback path.
struct Ladder
{
    float s[4] {}, g = 0, G = 0, k = 0;
    void set (float hz, float res, double sr)
    {
        hz = std::min (std::max (hz, 20.0f), 0.45f * (float) sr);
        g = std::tan (kPi * hz / (float) sr);
        G = g / (1.0f + g);
        k = res * 3.9f;
    }
    float tick (float x)
    {
        const float G4 = G * G * G * G;
        const float S = (s[0] * G * G * G + s[1] * G * G + s[2] * G + s[3]) / (1.0f + g);
        float u = (x - k * S) / (1.0f + k * G4);
        u = fastTanh (u);
        float y = u;
        for (int i = 0; i < 4; ++i)
        {
            const float v = (y - s[i]) * G;
            const float out = v + s[i];
            s[i] = out + v;
            y = out;
        }
        return y * (1.0f + k * 0.3f);
    }
    void reset() { for (auto& v : s) v = 0; }
};

// Schroeder all-pass (input diffusion).
struct Allpass
{
    DelayLine d;
    int len = 1;
    float g = 0.6f;
    void prepare (int maxLen) { d.prepare (maxLen + 2); }
    float tick (float x)
    {
        const float vm = d.tap (len - 1);
        const float v = x + g * vm;
        d.push (v);
        return vm - g * v;
    }
    void clear() { d.clear(); }
};

// 8-line feedback delay network reverb (Hadamard mixing, modulated).
//  - Input diffusion: 4 series all-passes per channel smear the attack into a dense onset.
//  - Two-band decay (Jot): every line has its own first-order absorption filter whose DC gain
//    gives DECAY (RT60) and whose Nyquist gain gives the shorter high-frequency RT60 set by DARK,
//    each computed for that line's length, so every mode dies at the same rate.
//  - FREEZE: lossless loop, input closed.
struct Fdn
{
    static constexpr int N = 8;
    DelayLine line[N];
    Allpass diff[2][4];
    float len[N] {}, a0[N] {}, b1[N] {}, z[N] {}, lfo = 0.0f;
    OnePole lowCut[2];
    double sr = 48000.0;
    float modDepth = 0.0f;
    bool frozen = false;
    void prepare (double rate)
    {
        sr = rate;
        for (auto& l : line) l.prepare ((int) (rate * 0.6));
        static const float dl[4] = { 0.00473f, 0.00359f, 0.01271f, 0.00930f };
        for (int c = 0; c < 2; ++c)
            for (int k = 0; k < 4; ++k)
            {
                auto& ap = diff[c][k];
                ap.prepare ((int) (rate * 0.02));
                ap.len = std::max (1, (int) (dl[k] * (c == 0 ? 1.0f : 1.087f) * (float) rate));
                ap.g = k < 2 ? 0.70f : 0.62f;
            }
        for (auto& c : lowCut) c.setHz (60.0f, rate);
        clear();
    }
    void clear()
    {
        for (auto& l : line) l.clear();
        for (auto& c : diff) for (auto& ap : c) ap.clear();
        for (auto& v : z) v = 0.0f;
        for (auto& c : lowCut) c.reset();
    }
    // size 0..1, decay = RT60 seconds (lows/mids), dark 0..1 (highs decay faster), freeze -> infinite
    void set (float size, float decaySeconds, float dark, float mod, bool freeze)
    {
        static const float base[N] = { 0.0297f, 0.0371f, 0.0411f, 0.0437f, 0.0533f, 0.0617f, 0.0683f, 0.0751f };
        const float scale = 0.35f + size * 2.4f;
        const float rtLow = std::max (0.1f, decaySeconds);
        const float rtHigh = rtLow * lerp (0.85f, 0.07f, clamp01 (dark));
        frozen = freeze;
        for (int i = 0; i < N; ++i)
        {
            len[i] = base[i] * scale * (float) sr;
            if (freeze) { a0[i] = 1.0f; b1[i] = 0.0f; continue; }
            const float secs = len[i] / (float) sr;
            const float gDc = std::min (0.9995f, std::pow (10.0f, -3.0f * secs / rtLow));
            const float gNy = std::min (gDc, std::pow (10.0f, -3.0f * secs / rtHigh));
            const float b = (gDc - gNy) / (gDc + gNy);
            b1[i] = b;
            a0[i] = gDc * (1.0f - b);
        }
        modDepth = mod * 0.0025f * (float) sr;
    }
    void tick (float inL, float inR, float& outL, float& outR, bool freeze)
    {
        float x[N];
        lfo += 0.7f / (float) sr; if (lfo > 1.0f) lfo -= 1.0f;
        for (int i = 0; i < N; ++i)
        {
            const float m = modDepth * std::sin (kTwoPi * (lfo + (float) i * 0.125f));
            const float y = line[i].read (len[i] + m);
            z[i] = a0[i] * y + b1[i] * z[i];          // absorption: per-line RT60, two bands
            x[i] = z[i];
        }
        outL = lowCut[0].hp (x[0] + x[2] + x[4] + x[6]) * 0.5f;
        outR = lowCut[1].hp (x[1] + x[3] + x[5] + x[7]) * 0.5f;
        // Hadamard 8 (unitary with the 1/sqrt(8) below).
        for (int h = 1; h < N; h <<= 1)
            for (int i = 0; i < N; i += h << 1)
                for (int j = i; j < i + h; ++j) { const float u = x[j], v = x[j + h]; x[j] = u + v; x[j + h] = u - v; }
        float dl = 0.0f, dr = 0.0f;
        if (! freeze)
        {
            dl = inL; dr = inR;
            for (int k = 0; k < 4; ++k) { dl = diff[0][k].tick (dl); dr = diff[1][k].tick (dr); }
        }
        for (int i = 0; i < N; ++i)
        {
            const float in = (i & 1) ? dr : dl;
            // Soft limit in the loop: never runs away, transparent at normal levels.
            line[i].push (fastTanh ((x[i] * 0.35355339f + in) * 0.5f) * 2.0f);
        }
    }
};

// Peak / RMS meter readings for editors (written on the audio thread).
struct MeterState
{
    float peak = 0.0f, rms = 0.0f;
    void push (const float* const* ch, int numCh, int n)
    {
        float pk = 0.0f, sum = 0.0f;
        for (int c = 0; c < numCh; ++c)
            for (int i = 0; i < n; ++i) { const float v = std::fabs (ch[c][i]); pk = std::max (pk, v); sum += v * v; }
        peak = std::max (pk, peak * 0.92f);
        const float r = std::sqrt (sum / (float) std::max (1, n * numCh));
        rms = r + 0.8f * (rms - r);
    }
};
} // namespace sf
