#pragma once
// Everything a module file needs.
#include "../core/AllocTap.h"
#include "../core/Bus.h"
#include "../core/Dsp.h"
#include "../core/Instrument.h"
#include "../core/Module.h"
#include "../core/Pocket.h"
#include "../core/Registry.h"
#include "../core/Sample.h"
#include "../core/Ui.h"

namespace sf
{
inline void applyGain (juce::AudioBuffer<float>& b, float g0, float g1)
{
    for (int c = 0; c < b.getNumChannels(); ++c) b.applyGainRamp (c, 0, b.getNumSamples(), g0, g1);
}
inline float levelDb (const Ctx& c, int i, float& g0) { g0 = dbToGain (c.v0 (i)); return dbToGain (c.v (i)); }
inline ParamSpec levelParam (float def = 0.0f) { return knob ("level", "LEVEL", -36.0f, 6.0f, def, "dB", -9.0f); }

// Atomic float history for displays.
template <int N>
struct History
{
    std::atomic<float> v[N] {};
    int w = 0;
    void push (float x) { v[w].store (x); w = (w + 1) % N; }
};

// A 0..1 position marker on a line, for filter-type displays.
inline void drawPosition (juce::Graphics& g, juce::Rectangle<float> r, float pos, const juce::String& left, const juce::String& right, juce::Colour c)
{
    const float y = r.getCentreY();
    g.setColour (ui::kLine);
    g.drawHorizontalLine ((int) y, r.getX(), r.getRight());
    g.drawVerticalLine ((int) r.getCentreX(), y - 6, y + 6);
    const float x = r.getX() + pos * r.getWidth();
    g.setColour (c);
    g.fillRect (x - 2.0f, y - 8.0f, 4.0f, 16.0f);
    ui::label (g, left, r.withHeight (14), ui::kFaint, 10.0f);
    ui::label (g, right, r.withHeight (14), ui::kFaint, 10.0f, juce::Justification::centredRight);
}
} // namespace sf

namespace sf
{
// Standard instrument output stage: DJ FILTER -> (POCKET) -> ALLOC tap -> LEVEL.
struct RoleOutput
{
    DJFilter filter;
    Pocket pocket;
    AllocTap tap;
    bool usePocket = false;
    void prepare (double sr, int mb, int role, const char* name, bool pocketOn)
    {
        filter.prepare (sr);
        usePocket = pocketOn;
        pocket.prepare (sr, mb);
        tap.setIdentity (role, name);
        tap.prepare (sr);
    }
    int latency() const { return usePocket ? Pocket::kLatency : 0; }
    void reset() { filter.reset(); pocket.reset(); tap.reset(); }
    void process (const Ctx& c, juce::AudioBuffer<float>& b, int filtIdx, int resIdx, float pocketDepth, float pocketShape,
                  bool allocOn, int levelIdx, bool delayAudio = true)
    {
        auto ch = b.getArrayOfWritePointers();
        filter.process (ch, 2, c.n, c.v0 (filtIdx), c.v (filtIdx), resIdx >= 0 ? c.v (resIdx) : 0.15f);
        if (usePocket) pocket.process (c, ch, 2, c.n, pocketDepth, pocketShape, nullptr, pocketDepth > 0.001f, false, delayAudio);
        tap.process (ch, 2, c.n, allocOn, &c);
        float g0; const float g1 = levelDb (c, levelIdx, g0);
        applyGain (b, g0, g1);
    }
};

// KEY: semitones that move a pitch of `hz` to the nearest note of pitch class keyRoot (C = 0).
inline float keySnap (float hz, int keyRoot)
{
    if (keyRoot < 0 || hz <= 0.0f) return 0.0f;
    const float note = 69.0f + 12.0f * std::log2 (hz / 440.0f);
    float d = std::fmod ((float) keyRoot - note, 12.0f);
    if (d < -6.0f) d += 12.0f;
    if (d >= 6.0f) d -= 12.0f;
    return d;
}

// Voice-steal helper: index of the quietest / oldest voice.
template <typename V, size_t N>
int pickVoice (std::array<V, N>& v)
{
    for (size_t i = 0; i < N; ++i) if (! v[i].active) return (int) i;
    int best = 0; float lvl = 1e9f;
    for (size_t i = 0; i < N; ++i) if (v[i].level() < lvl) { lvl = v[i].level(); best = (int) i; }
    return best;
}
} // namespace sf
