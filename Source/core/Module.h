#pragma once
// The contract every SF module implements. One SFProcessor + SFEditor hosts any module.
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <functional>
#include <string>
#include <vector>

namespace sf
{
class Bus;

enum class PType { Float, Choice, Toggle, Momentary };

struct ParamSpec
{
    std::string id, name;
    PType type = PType::Float;
    float min = 0.0f, max = 1.0f, def = 0.0f;
    float centre = NAN;             // value at the knob's midpoint (skew); NAN = linear
    std::string unit;               // "%", "dB", "Hz", "ms", "st", "BIP" (bipolar 0..1, centre = off)
    std::vector<std::string> choices;
    bool primary = false;           // shown in the top row; first 8 params = Live's device view
    bool glide = true;              // follows GLIDE (slow mode) when on
    bool song = true;               // stored in song slots
    int macroDir = 0;               // response to the plugin's MACRO: +1 / -1 / 0
    bool hidden = false;            // not in the generic grid (the module draws its own control)

    ParamSpec& prim()           { primary = true; return *this; }
    ParamSpec& dir (int d)      { macroDir = d; return *this; }
    ParamSpec& noGlide()        { glide = false; return *this; }
    ParamSpec& noSong()         { song = false; return *this; }
    ParamSpec& u (const char* s){ unit = s; return *this; }
    ParamSpec& hide()           { hidden = true; return *this; }
};

inline ParamSpec knob (const char* id, const char* name, float mn, float mx, float def, const char* unit = "", float centre = NAN)
{
    ParamSpec p; p.id = id; p.name = name; p.min = mn; p.max = mx; p.def = def; p.unit = unit; p.centre = centre; return p;
}
inline ParamSpec pct (const char* id, const char* name, float def) { return knob (id, name, 0.0f, 1.0f, def, "%"); }
inline ParamSpec bip (const char* id, const char* name)             { return knob (id, name, 0.0f, 1.0f, 0.5f, "BIP"); }
inline ParamSpec choice (const char* id, const char* name, std::vector<std::string> c, int def = 0)
{
    ParamSpec p; p.id = id; p.name = name; p.type = PType::Choice; p.choices = std::move (c);
    p.min = 0; p.max = (float) p.choices.size() - 1; p.def = (float) def; p.glide = false; return p;
}
inline ParamSpec toggle (const char* id, const char* name, bool def = false)
{
    ParamSpec p; p.id = id; p.name = name; p.type = PType::Toggle; p.def = def ? 1.0f : 0.0f; p.glide = false; return p;
}
inline ParamSpec momentary (const char* id, const char* name)
{
    ParamSpec p; p.id = id; p.name = name; p.type = PType::Momentary; p.glide = false; p.song = false; return p;
}

// Per-block context handed to Module::process.
struct Ctx
{
    double sr = 48000.0;
    int n = 0;                    // samples in this block
    double bpm = 120.0;
    double ppq = 0.0;             // quarter-note position at block start (internal clock when stopped)
    bool playing = false;
    double beatsPerBar = 4.0;
    const float* p = nullptr;     // parameter values at block end (glided, macro applied)
    const float* p0 = nullptr;    // values at block start (for per-sample ramps)
    Bus* bus = nullptr;
    const char* trackName = "";  // Live track name (VST3 only; empty if unknown)
    uint32_t trackNameSeq = 0;

    double beatsPerSample() const   { return bpm / 60.0 / sr; }
    double samplesPerBeat() const   { return sr * 60.0 / bpm; }
    float v (int i) const           { return p[i]; }
    float v0 (int i) const          { return p0[i]; }
    int ci (int i) const            { return (int) std::lround (p[i]); }
    bool b (int i) const            { return p[i] > 0.5f; }
    double ppqAt (int s) const      { return ppq + s * beatsPerSample(); }
    // Sample offset in this block of the next multiple of `beats`, or -1.
    int nextGrid (double beats) const
    {
        const double first = std::ceil (ppq / beats - 1e-9) * beats;
        const double off = (first - ppq) * samplesPerBeat();
        return off < n ? (int) off : -1;
    }
};

class Module
{
public:
    virtual ~Module() = default;

    virtual std::string id() const = 0;                          // "KICK"
    virtual bool instrument() const                 { return false; }
    virtual bool sidechain() const                  { return false; }
    virtual std::vector<std::string> extraOutputs() const { return {}; }   // extra stereo outputs (instruments)
    virtual std::vector<ParamSpec> params() const = 0;           // module params; system params are appended

    virtual void prepare (double sr, int maxBlock) = 0;
    virtual void reset() {}
    // main: in/out stereo. side: sidechain (may be null). extra: extra outputs (may be null).
    virtual void process (Ctx& c, juce::AudioBuffer<float>& main, juce::AudioBuffer<float>* side,
                          juce::AudioBuffer<float>* extra, juce::MidiBuffer& midi) = 0;
    virtual int latency() const                     { return 0; }
    virtual double tailSeconds() const              { return 2.0; }

    // Files (samplers, DECK). Message thread.
    virtual bool acceptsFiles() const               { return false; }
    virtual void loadFiles (const juce::StringArray&) {}
    virtual juce::String fileHint() const           { return "DROP AUDIO FILE"; }

    // State beyond parameters (samples, track lists, stored tables). Message thread.
    virtual void save (juce::ValueTree&) const {}
    virtual void load (const juce::ValueTree&) {}

    // Display panel (message thread). Read only atomics / benign values set by the audio thread.
    virtual int displayHeight() const               { return 140; }
    virtual void paint (juce::Graphics&, juce::Rectangle<float>) {}
    virtual void displayMouse (juce::Point<float>, juce::Rectangle<float>, bool down) { juce::ignoreUnused (down); }
    virtual void displayMouseUp() {}

    // Program list (only SF CONTROL uses it: songs).
    virtual int numPrograms() const                 { return 1; }
    virtual int currentProgram() const              { return 0; }
    virtual void setProgram (int)                   {}

    // Message-thread service from the host processor: set a parameter and tell Live.
    std::function<void (int index, float value)> setParam;
    std::function<float (int index)> getParam;
};

using ModulePtr = std::unique_ptr<Module>;
} // namespace sf
