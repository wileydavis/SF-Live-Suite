#pragma once
// ALLOC participant: measures this module's energy in the 10 shared bands, publishes it on the
// SF BUS, and applies the per-band cuts SF ALLOC decides. Inactive (no cuts) when no SF ALLOC
// instance is running.
#include "Bus.h"
#include "Dsp.h"
#include "Module.h"
#include <cstring>
#include <random>

namespace sf
{
class AllocTap
{
public:
    AllocTap()
    {
        std::random_device rd;
        owner = ((uint64_t) rd() << 32) ^ (uint64_t) rd() ^ (uint64_t) (uintptr_t) this;
        if (owner == 0) owner = 1;
    }
    ~AllocTap() { Bus::get().releaseSource (slot, owner); }

    void setIdentity (int r, const char* n)
    {
        role = r; std::strncpy (base, n, 23);
        std::strncpy (name, liveName[0] ? liveName : base, 23);
        if (slot >= 0 && Bus::get().d().src[slot].owner.load() == owner) Bus::packName (Bus::get().d().src[slot].name, name);
    }

    void prepare (double rate)
    {
        sr = rate;
        for (int b = 0; b < kBands; ++b)
        {
            const float bw = kBandEdges[b + 1] - kBandEdges[b];
            analysis[b].set (kBandCentres[b], std::max (0.5f, kBandCentres[b] / bw), sr);
            analysis[b].reset();
            ms[b] = 0.0f;
            applied[b] = 0.0f;
            for (auto& c : eq) c[b].reset();
        }
        msCoef = std::exp (-1.0f / (float) (0.15 * sr));
    }

    // Analyse and apply. Call on the module's final output.
    void process (float* const* ch, int numCh, int n, bool enabled, const Ctx* c = nullptr)
    {
        auto& bus = Bus::get();
        // Live's track name (VST3) replaces the module name in ALLOC's list.
        if (c != nullptr && c->trackNameSeq != seenNameSeq)
        {
            seenNameSeq = c->trackNameSeq;
            std::strncpy (liveName, c->trackName, 23);
            setIdentity (role, base);
        }
        if (slot < 0 || ! bus.sourceAlive (slot, 6000) || bus.d().src[slot].owner.load() != owner)
            slot = bus.claimSource (owner, role, name);
        if (slot < 0) return;
        bus.touchSource (slot, owner, role, enabled);
        auto& s = bus.d().src[slot];

        // Analysis on the mono sum (pre-cut, so ALLOC sees what the source wants to play).
        float acc[kBands] {};
        for (int i = 0; i < n; ++i)
        {
            float x = ch[0][i];
            if (numCh > 1) x = 0.5f * (x + ch[1][i]);
            for (int b = 0; b < kBands; ++b)
            {
                const float y = analysis[b].tickBP (x);
                acc[b] += y * y;
            }
        }
        // Fast attack (40 ms), quick release (80 ms): a band frees up soon after a source stops.
        const float upCoef = std::pow (std::exp (-1.0f / (float) (0.04 * sr)), (float) n);
        const float downCoef = std::pow (std::exp (-1.0f / (float) (0.08 * sr)), (float) n);
        for (int b = 0; b < kBands; ++b)
        {
            const float m = acc[b] / (float) std::max (1, n);
            ms[b] = m + (m > ms[b] ? upCoef : downCoef) * (ms[b] - m);
            s.energy[b] = 10.0f * std::log10 (ms[b] + 1.0e-12f) + 3.0f;
        }

        // Cuts: glide toward ALLOC's request at ~60 dB/s.
        const bool live = enabled && bus.allocAlive();
        const float step = 60.0f * (float) n / (float) sr;
        for (int b = 0; b < kBands; ++b)
        {
            const float target = live ? std::min (24.0f, std::max (0.0f, s.cut[b].load())) : 0.0f;
            const float before = applied[b];
            applied[b] += juce_clamp (target - applied[b], -step, step);
            if (applied[b] < 0.05f && before < 0.05f) continue;
            const float q = std::max (0.7f, kBandCentres[b] / (kBandEdges[b + 1] - kBandEdges[b]));
            for (int c = 0; c < std::min (numCh, 2); ++c)
            {
                eq[c][b].peak (kBandCentres[b], q, -applied[b], sr);
                for (int i = 0; i < n; ++i) ch[c][i] = eq[c][b].tick (ch[c][i]);
            }
        }
    }

    float appliedCut (int b) const { return applied[b]; }
    int busSlot() const { return slot; }

private:
    static float juce_clamp (float v, float lo, float hi) { return std::max (lo, std::min (hi, v)); }
    uint64_t owner = 1;
    int slot = -1, role = ROLE_OTHER;
    char name[24] { "SOURCE" }, base[24] { "SOURCE" }, liveName[24] {};
    uint32_t seenNameSeq = 0;
    double sr = 48000.0;
    Svf analysis[kBands];
    Biquad eq[2][kBands];
    float ms[kBands] {}, applied[kBands] {};
    float msCoef = 0.0f;
};
} // namespace sf
