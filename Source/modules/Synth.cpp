// BASS, LEAD, PAD: one synth engine, role-specific model lists and defaults.
#include "Kit.h"

namespace sf
{
enum Engine { E_SUB, E_ORGAN, E_FM, E_HOOVER, E_SYNC, E_NOISE, E_WAVETABLE, E_GRAIN, E_SAMPLE };
enum FiltType { F_LADDER, F_SVF, F_MS20, F_NONE };

struct SynthModel
{
    const char* name;
    Engine engine;
    int wave = 0, wave2 = 0;        // 0 saw 1 square 2 tri 3 sine
    float osc2 = 0.0f, detune = 0.0f, sub = 0.0f, noise = 0.0f;
    int unison = 1;
    FiltType filt = F_LADDER;
    float resMul = 1.0f, envMul = 1.0f;
    float att = 0.002f, dec = 0.3f, sus = 0.6f, rel = 0.1f;       // amp
    float fAtt = 0.001f, fDec = 0.3f, fSus = 0.0f, fRel = 0.2f;   // filter
    bool mono = false, acid = false;
    int chord = 0;                  // 0 none, else index into chord table
    float ensemble = 0.0f, drive = 0.0f, wow = 0.0f, ratio = 1.0f, pitchDive = 0.0f;
    float gain = 1.0f;
};

// Chords (semitones). Index 0 = single note.
static const std::vector<std::vector<int>>& chordTable()
{
    static const std::vector<std::vector<int>> t { { 0 }, { 0, 3, 7, 10 }, { 0, 3, 10, 14 }, { 0, 5, 7, 12 }, { 0, 4, 7, 11 }, { 0, 3, 7 }, { 0, 7 }, { 0, 12 } };
    return t;
}
static std::vector<std::string> chordNames() { return { "MODEL", "NONE", "MIN7", "MIN9", "SUS4", "MAJ7", "MIN", "5TH", "OCT" }; }

static std::vector<SynthModel> bassModels()
{
    std::vector<SynthModel> m;
    { SynthModel x { "BASS LADDER", E_SUB }; x.gain = 1.6f; x.wave = 0; x.wave2 = 0; x.osc2 = 0.8f; x.detune = 0.08f; x.sub = 0.4f; x.mono = true; x.dec = 0.5f; x.sus = 0.8f; x.fDec = 0.35f; m.push_back (x); }
    { SynthModel x { "MONO 101-TYPE", E_SUB }; x.wave = 0; x.wave2 = 1; x.osc2 = 0.5f; x.sub = 0.5f; x.mono = true; x.dec = 0.4f; x.sus = 0.7f; x.fDec = 0.25f; m.push_back (x); }
    { SynthModel x { "SUB OSC", E_SUB }; x.wave = 1; x.sub = 0.9f; x.filt = F_SVF; x.mono = true; x.dec = 0.6f; x.sus = 0.9f; x.envMul = 0.4f; m.push_back (x); }
    { SynthModel x { "ACID 303-TYPE", E_SUB }; x.wave = 0; x.mono = true; x.acid = true; x.dec = 2.0f; x.sus = 1.0f; x.rel = 0.02f; x.fDec = 0.3f; x.resMul = 1.1f; x.envMul = 1.2f; m.push_back (x); }
    { SynthModel x { "FM", E_FM }; x.ratio = 1.0f; x.mono = true; x.filt = F_SVF; x.dec = 0.5f; x.sus = 0.7f; x.fDec = 0.2f; m.push_back (x); }
    { SynthModel x { "SINE SUB", E_SUB }; x.wave = 3; x.filt = F_NONE; x.mono = true; x.dec = 1.0f; x.sus = 1.0f; x.drive = 0.15f; m.push_back (x); }
    { SynthModel x { "OFFBEAT", E_SUB }; x.wave = 1; x.wave2 = 0; x.osc2 = 0.6f; x.detune = 0.05f; x.mono = true; x.dec = 0.18f; x.sus = 0.0f; x.fDec = 0.12f; x.resMul = 1.3f; x.envMul = 1.3f; m.push_back (x); }
    { SynthModel x { "SAMPLE", E_SAMPLE }; x.filt = F_SVF; x.mono = true; x.dec = 1.0f; x.sus = 1.0f; x.envMul = 0.0f; m.push_back (x); }
    return m;
}
static std::vector<SynthModel> leadModels()
{
    std::vector<SynthModel> m;
    { SynthModel x { "ACID 303-TYPE", E_SUB }; x.wave = 0; x.mono = true; x.acid = true; x.dec = 2.0f; x.sus = 1.0f; x.rel = 0.02f; x.fDec = 0.3f; x.resMul = 1.1f; x.envMul = 1.2f; m.push_back (x); }
    { SynthModel x { "MONO 101-TYPE", E_SUB }; x.wave = 0; x.wave2 = 1; x.osc2 = 0.5f; x.sub = 0.3f; x.mono = true; x.dec = 0.4f; x.sus = 0.6f; x.fDec = 0.25f; m.push_back (x); }
    { SynthModel x { "MS20-TYPE", E_SUB }; x.wave = 0; x.wave2 = 1; x.osc2 = 0.6f; x.detune = 0.1f; x.filt = F_MS20; x.mono = true; x.drive = 0.4f; x.dec = 0.5f; x.sus = 0.7f; x.resMul = 1.2f; m.push_back (x); }
    { SynthModel x { "STAB DUB", E_SUB }; x.wave = 0; x.wave2 = 1; x.osc2 = 0.5f; x.detune = 0.06f; x.filt = F_SVF; x.chord = 1; x.dec = 0.25f; x.sus = 0.0f; x.rel = 0.15f; x.fDec = 0.12f; x.envMul = 0.8f; x.ensemble = 0.3f; m.push_back (x); }
    { SynthModel x { "STAB DETROIT", E_SUB }; x.gain = 2.0f; x.wave = 0; x.unison = 3; x.detune = 0.15f; x.wave2 = 1; x.osc2 = 0.3f; x.filt = F_SVF; x.chord = 2; x.dec = 0.35f; x.sus = 0.2f; x.rel = 0.25f; x.fDec = 0.2f; x.ensemble = 0.6f; m.push_back (x); }
    { SynthModel x { "ORGAN M1-TYPE", E_ORGAN }; x.filt = F_NONE; x.chord = 5; x.dec = 0.3f; x.sus = 0.3f; x.rel = 0.08f; x.ensemble = 0.2f; m.push_back (x); }
    { SynthModel x { "HOOVER", E_HOOVER }; x.unison = 3; x.detune = 0.25f; x.filt = F_SVF; x.dec = 1.0f; x.sus = 0.9f; x.rel = 0.3f; x.ensemble = 0.8f; x.pitchDive = 7.0f; x.mono = true; m.push_back (x); }
    { SynthModel x { "FM BELL", E_FM }; x.ratio = 3.5f; x.filt = F_NONE; x.dec = 1.5f; x.sus = 0.0f; x.rel = 1.2f; x.fDec = 0.6f; m.push_back (x); }
    { SynthModel x { "SYNC", E_SYNC }; x.mono = true; x.dec = 0.5f; x.sus = 0.7f; x.fDec = 0.4f; x.filt = F_SVF; m.push_back (x); }
    { SynthModel x { "SAMPLE", E_SAMPLE }; x.filt = F_SVF; x.dec = 1.0f; x.sus = 1.0f; x.envMul = 0.0f; m.push_back (x); }
    return m;
}
static std::vector<SynthModel> padModels()
{
    std::vector<SynthModel> m;
    { SynthModel x { "STRING MACHINE", E_SUB }; x.gain = 2.0f; x.wave = 0; x.unison = 4; x.detune = 0.2f; x.wave2 = 0; x.osc2 = 0.5f; x.ratio = 2.0f; x.filt = F_SVF; x.att = 0.3f; x.dec = 1.0f; x.sus = 1.0f; x.rel = 0.8f; x.envMul = 0.1f; x.ensemble = 1.0f; m.push_back (x); }
    { SynthModel x { "POLY PAD", E_SUB }; x.gain = 1.6f; x.wave = 0; x.wave2 = 1; x.osc2 = 0.6f; x.detune = 0.1f; x.sub = 0.3f; x.filt = F_SVF; x.att = 0.5f; x.dec = 1.0f; x.sus = 1.0f; x.rel = 1.2f; x.fAtt = 0.8f; x.fDec = 2.0f; x.fSus = 0.4f; x.envMul = 0.4f; x.ensemble = 0.7f; m.push_back (x); }
    { SynthModel x { "WAVETABLE", E_WAVETABLE }; x.gain = 1.4f; x.filt = F_SVF; x.att = 0.4f; x.dec = 1.0f; x.sus = 1.0f; x.rel = 1.0f; x.envMul = 0.2f; x.ensemble = 0.3f; m.push_back (x); }
    { SynthModel x { "GRANULAR", E_GRAIN }; x.gain = 1.5f; x.filt = F_SVF; x.att = 0.6f; x.dec = 1.0f; x.sus = 1.0f; x.rel = 1.5f; x.envMul = 0.0f; m.push_back (x); }
    { SynthModel x { "DUB WASH", E_SUB }; x.wave = 0; x.wave2 = 1; x.osc2 = 0.4f; x.detune = 0.05f; x.filt = F_SVF; x.chord = 1; x.att = 0.05f; x.dec = 1.5f; x.sus = 0.6f; x.rel = 1.5f; x.fDec = 1.5f; x.envMul = 0.5f; x.ensemble = 0.5f; m.push_back (x); }
    { SynthModel x { "TAPE LOOP", E_SUB }; x.wave = 2; x.wave2 = 0; x.osc2 = 0.3f; x.detune = 0.03f; x.filt = F_SVF; x.att = 0.4f; x.dec = 1.0f; x.sus = 1.0f; x.rel = 1.5f; x.envMul = 0.0f; x.wow = 1.0f; x.drive = 0.3f; x.noise = 0.02f; m.push_back (x); }
    { SynthModel x { "NOISE BED", E_NOISE }; x.filt = F_SVF; x.att = 0.8f; x.dec = 1.0f; x.sus = 1.0f; x.rel = 2.0f; x.envMul = 0.0f; m.push_back (x); }
    { SynthModel x { "SAMPLE", E_SAMPLE }; x.filt = F_SVF; x.att = 0.05f; x.dec = 1.0f; x.sus = 1.0f; x.rel = 0.8f; x.envMul = 0.0f; m.push_back (x); }
    return m;
}

// Exponential ADSR.
struct Adsr
{
    enum { Idle, Att, Dec, Sus, Rel } st = Idle;
    float v = 0.0f, a = 0, d = 0, s = 0, r = 0;
    void set (float at, float dt, float sl, float rt, double sr)
    {
        a = 1.0f / (float) (std::max (0.0005f, at) * sr);
        d = std::exp (-1.0f / (float) (std::max (0.001f, dt) * sr * 0.3f));
        s = sl;
        r = std::exp (-1.0f / (float) (std::max (0.001f, rt) * sr * 0.3f));
    }
    void on (bool retrigger = true) { if (retrigger || st == Idle || st == Rel) st = Att; }
    void off() { if (st != Idle) st = Rel; }
    float tick()
    {
        switch (st)
        {
            case Att: v += a; if (v >= 1.0f) { v = 1.0f; st = Dec; } break;
            case Dec: v = s + d * (v - s); if (std::abs (v - s) < 1e-4f) st = Sus; break;
            case Sus: v = s; break;
            case Rel: v *= r; if (v < 1e-5f) { v = 0; st = Idle; } break;
            case Idle: break;
        }
        return v;
    }
};

// Stereo ensemble (3 modulated delays, BBD-like).
struct Ensemble
{
    DelayLine d[2];
    float ph = 0.0f;
    double sr = 48000.0;
    void prepare (double rate) { sr = rate; for (auto& x : d) x.prepare ((int) (rate * 0.05)); }
    void process (float* L, float* R, int n, float amt)
    {
        if (amt <= 0.001f) { for (int i = 0; i < n; ++i) { d[0].push (L[i]); d[1].push (R[i]); } return; }
        for (int i = 0; i < n; ++i)
        {
            ph += 0.6f / (float) sr; if (ph > 1.0f) ph -= 1.0f;
            const float m = 0.5f * (L[i] + R[i]);
            d[0].push (m); d[1].push (m);
            float wl = 0, wr = 0;
            for (int k = 0; k < 3; ++k)
            {
                const float lfo = std::sin (kTwoPi * (ph + k / 3.0f)) + 0.15f * std::sin (kTwoPi * (ph * 9.0f + k / 3.0f));
                const float ms = (7.0f + 4.0f * lfo) * 0.001f * (float) sr;
                const float y = d[0].read (ms);
                if (k == 0) wl += y; else if (k == 1) wr += y; else { wl += 0.5f * y; wr += 0.5f * y; }
            }
            L[i] = L[i] * (1.0f - amt * 0.5f) + wl * amt * 0.7f;
            R[i] = R[i] * (1.0f - amt * 0.5f) + wr * amt * 0.7f;
        }
    }
};

class SynthModule : public Module
{
public:
    enum { CUTOFF, RES, ENVAMT, GATE, DENS, FILT, DRIVE, LEVEL, MODEL, DECAY, ATTACK, ACCENT, GLIDE, TUNE, DETUNE, COLOR,
           LAYERMODEL, LAYER, CHORD, SPACE, PDEPTH, PSHAPE, ESPACE, ESWING, ALLOCON };

    SynthModule (const char* idName, int role, std::vector<SynthModel> m, float cutDef, float resDef, float envDef, float gateDef, float pDepth)
        : name (idName), roleId (role), models (std::move (m)), dCut (cutDef), dRes (resDef), dEnv (envDef), dGate (gateDef), dPocket (pDepth) {}

    std::string id() const override { return name; }
    bool instrument() const override { return true; }
    int latency() const override { return Pocket::kLatency; }
    double tailSeconds() const override { return 6.0; }
    std::vector<ParamSpec> params() const override
    {
        std::vector<std::string> mn; for (auto& m : models) mn.push_back (m.name);
        return { pct ("cutoff", "CUTOFF", dCut).prim().dir (1),
                 pct ("res", "RES", dRes).prim(),
                 pct ("env_amt", "ENV AMT", dEnv).prim().dir (1),
                 pct ("gate", "GATE", dGate).prim(),
                 pct ("density", "DENSITY", 0.0f).prim().dir (1),
                 bip ("filter", "FILTER").prim(),
                 pct ("drive", "DRIVE", 0.0f).prim().dir (1),
                 levelParam (-6.0f).prim(),
                 choice ("model", "MODEL", mn, 0),
                 pct ("decay", "DECAY", 0.5f),
                 pct ("attack", "ATTACK", 0.0f),
                 pct ("accent", "ACCENT", 0.5f),
                 pct ("glide", "GLIDE", 0.2f),
                 knob ("tune", "TUNE", -24.0f, 24.0f, 0.0f, "st"),
                 pct ("detune", "DETUNE", 0.5f),
                 pct ("color", "COLOR", 0.5f),
                 choice ("layer_model", "LAYER MODEL", mn, std::min<int> (1, (int) mn.size() - 1)),
                 pct ("layer", "LAYER", 0.0f),
                 choice ("chord", "CHORD", chordNames(), 0),
                 pct ("space", "SPACE", 0.5f),
                 pct ("pocket_depth", "POCKET DEPTH", dPocket),
                 pct ("pocket_shape", "POCKET SHAPE", 0.25f),
                 choice ("echo_space", "ECHO SPACE", echoSpaceNames(), 1),
                 pct ("echo_swing", "ECHO SWING", 0.0f),
                 toggle ("alloc", "ALLOC", true) };
    }
    bool acceptsFiles() const override { return true; }
    juce::String fileHint() const override { return "DROP SAMPLE (MODELS: SAMPLE, GRANULAR)"; }
    void loadFiles (const juce::StringArray& f) override { if (f.size() > 0) sample.set (loadSampleFile (juce::File (f[0]), sr)); }
    void save (juce::ValueTree& t) const override { sample.save (t, "SAMPLE"); }
    void load (const juce::ValueTree& t) override { sample.load (t, "SAMPLE", sr); }

    void prepare (double rate, int mb) override
    {
        if (auto s = sample.get(); s != nullptr && std::abs (s->sr - rate) > 1 && s->flac.getSize() > 0) sample.set (loadSampleFlac (s->flac, s->name, rate));
        sr = rate;
        out.prepare (rate, mb, roleId, name.c_str(), true);
        ens.prepare (rate);
        echoes.reset();
        for (auto& v : voices) v.active = false;
        held.clear();
        buildTables();
        buildGrainSource();
    }

    struct Voice
    {
        bool active = false, releasing = false;
        int note = 60, gen = 0, model = 0, layer = 0, autoOff = -1;
        float vel = 1.0f, gain = 1.0f, freq = 110.0f, target = 110.0f, glideCoef = 0.0f, accent = 0.0f;
        double ph[8] {}, ph2[8] {}, subPh = 0.0, modPh = 0.0, spos = 0.0;
        float pan[8] {};
        Adsr amp, fenv;
        Ladder ladder;
        Svf svf, svf2, nbp;
        float t = 0.0f, fb = 0.0f, drift = 0.0f;
        struct Grain { bool on = false; double pos = 0, rate = 1; int len = 0, age = 0; float pan = 0.5f; } grains[8];
        int grainClock = 0;
        float level() const { return active ? amp.v * vel : 0.0f; }
    };

    void process (Ctx& c, juce::AudioBuffer<float>& io, juce::AudioBuffer<float>*, juce::AudioBuffer<float>*, juce::MidiBuffer& midi) override
    {
        echoes.run (c, midi, events, c.v (DENS), echoSpaceBeats (c.ci (ESPACE)), c.v (ESWING), 1.0f, true);
        const auto smp = sample.get();
        const int modelA = c.ci (MODEL), modelB = c.ci (LAYERMODEL);
        ensembleAmt = models[(size_t) juce::jlimit (0, (int) models.size() - 1, modelA)].ensemble;
        if (modelA != lastModel)
        {
            // Model change: release everything so a mono/poly switch can't leave a voice hanging.
            lastModel = modelA;
            for (auto& v : voices) if (v.active) { v.amp.off(); v.fenv.off(); v.releasing = true; }
            held.clear();
        }
        const float layer = c.v (LAYER);
        int autoOff; float relMul;
        gateShape (c.v (GATE), c.samplesPerBeat(), autoOff, relMul);
        size_t e = 0;
        auto* L = io.getWritePointer (0); auto* R = io.getWritePointer (1);
        const int block = 32;
        for (int i0 = 0; i0 < c.n; i0 += block)
        {
            const int n = std::min (block, c.n - i0);
            // Events landing in this sub-block are applied at its start (<= 0.7 ms early).
            while (e < events.size() && events[e].offset < i0 + n)
            {
                const auto& ev = events[e++];
                if (ev.on)
                {
                    noteOn (c, modelA, ev, 1.0f / (1.0f + layer * 0.5f), 0, autoOff, relMul);
                    if (layer > 0.01f) noteOn (c, modelB, ev, layer / (1.0f + layer * 0.5f), 1, autoOff, relMul);
                }
                else noteOff (ev.note, ev.gen, relMul);
            }
            for (int i = i0; i < i0 + n; ++i) { L[i] = 0; R[i] = 0; }
            for (auto& v : voices) if (v.active) render (c, v, L + i0, R + i0, n, smp.get());
        }
        ens.process (L, R, c.n, ensembleAmt * c.v (SPACE) * 2.0f);
        out.process (c, io, FILT, -1, c.v (PDEPTH), c.v (PSHAPE), c.b (ALLOCON), LEVEL);
        int act = 0; for (auto& v : voices) act += v.active ? 1 : 0;
        activeVoices = act;
    }

    int displayHeight() const override { return 80; }
    void paint (juce::Graphics& g, juce::Rectangle<float> r) override
    {
        auto left = r.removeFromLeft (r.getWidth() * 0.4f);
        ui::frame (g, left, "VOICES");
        ui::label (g, juce::String (activeVoices.load()) + " / " + juce::String ((int) voices.size()), left.reduced (10, 18), ui::kText, 18.0f, juce::Justification::centred);
        auto s = sample.get();
        ui::label (g, s ? s->name.toUpperCase() + (s->rootHz > 0 ? " ROOT " + juce::MidiMessage::getMidiNoteName ((int) std::lround (s->rootNote), true, true, 3) : "") : "NO SAMPLE",
                   left.reduced (8, 2).removeFromBottom (14), ui::kFaint, 10.0f);
        out.pocket.paint (g, r.withTrimmedLeft (8));
    }

private:
    void noteOn (Ctx& c, int modelIdx, const NoteEvent& ev, float gain, int layerId, int autoOff, float relMul)
    {
        const auto& m = models[(size_t) juce::jlimit (0, (int) models.size() - 1, modelIdx)];
        int chordIdx = c.ci (CHORD);
        const auto& chord = chordIdx == 0 ? chordTable()[(size_t) m.chord] : chordTable()[(size_t) std::max (0, chordIdx - 1)];
        const float tune = c.v (TUNE);
        const bool accent = ev.vel > 0.85f && ev.gen == 0;
        if (m.mono)
        {
            // Mono: legato slides (acid), else retrigger. Note stack for releases.
            if (layerId == 0 && ev.gen == 0) held.push_back (ev.note);
            Voice* v = nullptr;
            for (auto& x : voices) if (x.active && x.layer == layerId && x.model == modelIdx && ! x.releasing) v = &x;
            const bool legato = v != nullptr && ev.gen == 0 && held.size() > 1;
            if (v == nullptr) v = &voices[(size_t) pickVoice (voices)];
            setupVoice (c, *v, m, modelIdx, ev, (float) ev.note + tune, gain, layerId, autoOff, relMul, ! legato, accent);
            if (legato) v->glideCoef = std::exp (-1.0f / (0.06f * (float) sr * (m.acid ? 1.0f : 0.5f + c.v (GLIDE) * 3.0f)));
            return;
        }
        for (size_t k = 0; k < chord.size(); ++k)
        {
            auto& v = voices[(size_t) pickVoice (voices)];
            setupVoice (c, v, m, modelIdx, ev, (float) (ev.note + chord[k]) + tune, gain / std::sqrt ((float) chord.size()), layerId, autoOff, relMul, true, accent);
        }
    }

    void setupVoice (Ctx& c, Voice& v, const SynthModel& m, int modelIdx, const NoteEvent& ev, float note, float gain, int layerId,
                     int autoOff, float relMul, bool retrigger, bool accent)
    {
        const bool wasActive = v.active && ! v.releasing;
        if (retrigger || ! wasActive)
        {
            const float keepFreq = v.freq;
            v = Voice();
            v.freq = wasActive ? keepFreq : noteHz (note);
            for (int k = 0; k < 8; ++k) { v.ph[k] = rng.uni(); v.ph2[k] = rng.uni(); v.pan[k] = 0.5f + (k == 0 ? 0.0f : ((k & 1) ? -0.35f : 0.35f) * (0.5f + 0.5f * rng.uni())); }
        }
        v.active = true; v.releasing = false;
        v.note = ev.note; v.gen = ev.gen; v.model = modelIdx; v.layer = layerId;
        v.vel = 0.55f + 0.45f * lerp (1.0f, ev.vel, 0.6f);
        v.gain = gain;
        v.target = noteHz (note);
        v.glideCoef = c.v (GLIDE) > 0.02f && ! m.acid ? std::exp (-1.0f / ((0.005f + c.v (GLIDE) * 0.4f) * (float) sr)) : 0.0f;
        if (! wasActive) v.freq = v.target;
        v.accent = accent ? c.v (ACCENT) : 0.0f;
        const float decK = std::pow (2.0f, (c.v (DECAY) - 0.5f) * 4.0f) * std::pow (0.6f, (float) ev.gen) * (1.0f - v.accent * 0.4f);
        const float attK = c.v (ATTACK) * 2.0f;
        v.amp.set (m.att + attK, m.dec * (m.acid ? 1.0f : decK), m.sus, m.rel * relMul, sr);
        v.fenv.set (m.fAtt + attK * 0.5f, m.fDec * decK, m.fSus, m.fRel * relMul, sr);
        v.amp.on (retrigger || ! wasActive);
        v.fenv.on (retrigger || ! wasActive || m.acid);
        v.autoOff = autoOff;
        v.t = 0.0f;
    }

    void noteOff (int note, int gen, float relMul)
    {
        juce::ignoreUnused (relMul);
        if (note < 0) { for (auto& v : voices) { v.amp.off(); v.fenv.off(); v.releasing = true; } held.clear(); return; }
        if (gen == 0)
        {
            held.erase (std::remove (held.begin(), held.end(), note), held.end());
            // Mono: return to the previous held note (slide).
            for (auto& v : voices)
                if (v.active && ! v.releasing && v.note == note && models[(size_t) v.model].mono && ! held.empty())
                {
                    v.note = held.back();
                    v.target = noteHz ((float) held.back() + tuneCache);
                    v.glideCoef = std::exp (-1.0f / (0.06f * (float) sr));
                    return;
                }
        }
        for (auto& v : voices)
            if (v.active && ! v.releasing && v.note == note && v.gen == gen) { v.amp.off(); v.fenv.off(); v.releasing = true; }
    }

    void render (Ctx& c, Voice& v, float* L, float* R, int n, const SampleData* smp)
    {
        const auto& m = models[(size_t) v.model];
        tuneCache = c.v (TUNE);
        // Filter settings for this sub-block.
        const float cutK = c.v (CUTOFF), res = clamp01 (c.v (RES) * m.resMul), envA = c.v (ENVAMT) * m.envMul * (1.0f + v.accent);
        const float drive = clamp01 (c.v (DRIVE) + m.drive);
        const float color = c.v (COLOR), det = c.v (DETUNE);
        const float fe = v.fenv.v;
        const float keyTrack = std::log2 (std::max (20.0f, v.freq) / 130.0f) * 0.3f;
        const float cutT = clamp01 (cutK + envA * fe * 0.6f + keyTrack * 0.1f) * std::pow (0.75f, (float) v.gen);
        const float cutHz = expMap (cutT, 35.0f, 18000.0f);
        switch (m.filt)
        {
            case F_LADDER: v.ladder.set (cutHz, res * 0.98f, sr); break;
            case F_SVF:    v.svf.set (cutHz, 0.6f + res * 8.0f, sr); break;
            case F_MS20:   v.svf.set (cutHz, 0.6f + res * 12.0f, sr); v.svf2.set (std::max (20.0f, cutHz * 0.08f), 0.7f + res * 3.0f, sr); break;
            case F_NONE: break;
        }
        if (m.engine == E_NOISE) v.nbp.set (v.freq, 4.0f + color * 40.0f, sr);
        const int uni = std::max (1, m.unison);
        const float detCents = m.detune * 40.0f * (0.2f + det * 1.6f);
        const float gainAll = v.gain * v.vel * (1.0f + v.accent * 0.5f) * 0.32f * m.gain;
        for (int i = 0; i < n; ++i)
        {
            if (v.glideCoef > 0.0f) v.freq = v.target + v.glideCoef * (v.freq - v.target); else v.freq = v.target;
            float f = v.freq;
            if (m.pitchDive > 0.0f) f *= semis (-m.pitchDive * std::exp (-v.t / 0.08f));
            if (m.wow > 0.0f)
            {
                v.drift += 0.3f / (float) sr; if (v.drift > 1.0f) v.drift -= 1.0f;
                f *= semis (m.wow * (0.12f * std::sin (kTwoPi * v.drift) + 0.03f * std::sin (kTwoPi * v.drift * 17.0f)));
            }
            const float a = v.amp.tick();
            v.fenv.tick();
            float x = 0.0f, xl = 0.0f, xr = 0.0f;
            bool stereo = false;
            switch (m.engine)
            {
                case E_SUB:
                case E_HOOVER:
                {
                    const int shape1 = m.engine == E_HOOVER ? 1 : (m.acid ? (color < 0.5f ? 0 : 1) : m.wave);
                    for (int u = 0; u < uni; ++u)
                    {
                        const float cents = uni == 1 ? 0.0f : detCents * ((float) u / (float) (uni - 1) - 0.5f) * 2.0f;
                        const float pw = m.engine == E_HOOVER ? 0.5f + 0.35f * std::sin (kTwoPi * (v.t * 0.7f + u * 0.3f)) : 0.5f;
                        Osc o; o.phase = v.ph[u];
                        float y = o.tick (f * semis (cents / 100.0f), sr, shape1, pw);
                        v.ph[u] = o.phase;
                        if (m.osc2 > 0.0f && m.engine != E_HOOVER)
                        {
                            Osc o2; o2.phase = v.ph2[u];
                            const float r2 = m.ratio * semis ((detCents * 0.5f + det * 7.0f * (m.unison > 1 ? 0.0f : 1.0f)) / 100.0f);
                            y += o2.tick (f * r2, sr, m.wave2, 0.3f + color * 0.4f) * m.osc2 * (m.acid ? 0.0f : 1.0f);
                            v.ph2[u] = o2.phase;
                        }
                        if (uni > 1) { xl += y * (1.0f - v.pan[u]); xr += y * v.pan[u]; stereo = true; }
                        else x += y;
                    }
                    if (uni > 1) { xl /= std::sqrt ((float) uni); xr /= std::sqrt ((float) uni); }
                    if (m.sub > 0.0f || (m.engine == E_SUB && ! m.acid && color > 0.0f && m.mono))
                    {
                        v.subPh += f * 0.5 / sr; if (v.subPh > 1.0) v.subPh -= 1.0;
                        const float subLvl = m.sub * (m.mono ? (0.5f + color) : 1.0f);
                        const float s = (v.subPh < 0.5 ? 1.0f : -1.0f) * subLvl * 0.7f;
                        x += s; xl += s; xr += s;
                    }
                    if (m.noise > 0.0f) { const float nz = rng.next() * m.noise; x += nz; xl += nz; xr += nz; }
                    break;
                }
                case E_ORGAN:
                {
                    static const float h[6] = { 1, 2, 3, 4, 6, 8 };
                    const float w[6] = { 1.0f, 0.8f * color + 0.2f, 0.9f, 0.5f * color, 0.3f, 0.25f * color };
                    v.ph[0] += f / sr; if (v.ph[0] > 1.0) v.ph[0] -= 1.0;
                    for (int k = 0; k < 6; ++k) x += std::sin (kTwoPi * (float) std::fmod (v.ph[0] * h[k], 1.0)) * w[k];
                    x *= 0.35f;
                    if (v.t < 0.004f) x += rng.next() * 0.3f * (1.0f - v.t / 0.004f);       // key click
                    x += std::sin (kTwoPi * (float) std::fmod (v.ph[0] * 3.0, 1.0)) * 0.4f * std::exp (-v.t / 0.2f);   // percussion
                    break;
                }
                case E_FM:
                {
                    const float idx = (0.5f + color * 6.0f) * (0.3f + 0.7f * v.fenv.v) * (1.0f + v.accent);
                    v.modPh += f * m.ratio * (0.98 + det * 0.04) / sr; if (v.modPh > 1.0) v.modPh -= 1.0;
                    const float mod = std::sin (kTwoPi * (float) v.modPh + v.fb * 0.3f);
                    v.fb = mod;
                    v.ph[0] += f / sr; if (v.ph[0] > 1.0) v.ph[0] -= 1.0;
                    x = std::sin (kTwoPi * (float) v.ph[0] + idx * mod);
                    break;
                }
                case E_SYNC:
                {
                    v.ph[0] += f / sr;
                    const float ratio = 1.0f + (color * 4.0f + v.fenv.v * c.v (ENVAMT) * 6.0f);
                    if (v.ph[0] >= 1.0) { v.ph[0] -= 1.0; v.ph2[0] = 0.0; }
                    v.ph2[0] += f * ratio / sr; if (v.ph2[0] > 1.0) v.ph2[0] -= 1.0;
                    x = (float) (2.0 * v.ph2[0] - 1.0) * 0.8f;
                    break;
                }
                case E_NOISE:
                    x = v.nbp.tickBP (rng.next()) * (2.0f + color * 6.0f) + rng.next() * 0.05f * (1.0f - color);
                    break;
                case E_WAVETABLE:
                {
                    v.ph[0] += f / sr; if (v.ph[0] > 1.0) v.ph[0] -= 1.0;
                    const float pos = clamp01 (color + 0.15f * std::sin (kTwoPi * v.t * 0.13f)) * 3.0f;
                    const int t0 = std::min (2, (int) pos);
                    const float fr = pos - (float) t0;
                    const float idxF = (float) v.ph[0] * kTable;
                    const int i0 = (int) idxF & (kTable - 1);
                    x = lerp (tables[t0][(size_t) i0], tables[t0 + 1][(size_t) i0], fr);
                    break;
                }
                case E_GRAIN:
                {
                    const auto& src = smp != nullptr ? smp->buf : grainSrc;
                    const int len = src.getNumSamples();
                    const float root = smp != nullptr && smp->rootHz > 0 ? smp->rootHz : 130.81f;
                    const int glen = (int) ((0.03f + color * 0.25f) * sr);
                    if (len > glen + 2 && --v.grainClock <= 0)
                    {
                        v.grainClock = std::max (1, glen / 4);
                        for (auto& gr : v.grains)
                            if (! gr.on)
                            {
                                gr.on = true; gr.age = 0; gr.len = glen;
                                const float scatter = det * 0.3f;
                                gr.pos = std::fmod (v.t * 0.05f * sr + rng.uni() * scatter * len, (double) (len - glen - 2));
                                if (gr.pos < 0) gr.pos += len - glen - 2;
                                gr.rate = (f / root) * (1.0 + (rng.uni() - 0.5f) * det * 0.02f);
                                gr.pan = 0.5f + (rng.uni() - 0.5f) * 0.8f;
                                break;
                            }
                    }
                    stereo = true;
                    for (auto& gr : v.grains)
                    {
                        if (! gr.on) continue;
                        const int ip = (int) gr.pos;
                        if (ip + 1 >= len || gr.age >= gr.len) { gr.on = false; continue; }
                        const float w = 0.5f - 0.5f * std::cos (kTwoPi * (float) gr.age / (float) gr.len);
                        const float y = (src.getSample (0, ip) + src.getSample (src.getNumChannels() > 1 ? 1 : 0, ip)) * 0.5f * w;
                        xl += y * (1.0f - gr.pan); xr += y * gr.pan;
                        gr.pos += gr.rate; ++gr.age;
                    }
                    break;
                }
                case E_SAMPLE:
                {
                    if (smp == nullptr) { v.active = false; return; }
                    const int ip = (int) v.spos;
                    if (ip + 1 >= smp->length()) { v.active = false; return; }
                    const float fr = (float) (v.spos - ip);
                    xl = smp->buf.getSample (0, ip) + fr * (smp->buf.getSample (0, ip + 1) - smp->buf.getSample (0, ip));
                    xr = smp->buf.getSample (1, ip) + fr * (smp->buf.getSample (1, ip + 1) - smp->buf.getSample (1, ip));
                    stereo = true;
                    v.spos += (double) f / (double) noteHz (smp->rootNote);
                    break;
                }
            }
            if (! stereo) { xl = x; xr = x; }
            // Filter (mono filters run on the mid; stereo engines get mid/side filtered as one).
            auto filt = [&] (float in) -> float
            {
                if (drive > 0.001f) { const float g = 1.0f + drive * 6.0f; in = fastTanh (in * g) / std::sqrt (g) * 1.2f; }
                switch (m.filt)
                {
                    case F_LADDER: return v.ladder.tick (in);
                    case F_SVF: return v.svf.tickLP (in);
                    case F_MS20: return fastTanh (v.svf2.tickHP (v.svf.tickLP (in)) * 1.5f);
                    default: return in;
                }
            };
            float yl, yr;
            if (stereo && m.filt != F_NONE)
            {
                const float mid = filt (0.5f * (xl + xr));
                const float side = 0.5f * (xl - xr);
                yl = mid + side * (m.filt == F_NONE ? 1.0f : clamp01 (cutT * 1.5f)); yr = mid - side * clamp01 (cutT * 1.5f);
            }
            else if (stereo) { yl = xl; yr = xr; }
            else { yl = yr = filt (xl); }
            L[i] += yl * a * gainAll;
            R[i] += yr * a * gainAll;
            v.t += 1.0f / (float) sr;
            if (v.autoOff > 0 && --v.autoOff == 0 && ! v.releasing) { v.amp.off(); v.fenv.off(); v.releasing = true; }
            if (v.amp.st == Adsr::Idle && v.t > 0.001f) { v.active = false; return; }
        }
    }

    void buildTables()
    {
        for (int t = 0; t < 4; ++t) tables[t].assign ((size_t) kTable, 0.0f);
        for (int i = 0; i < kTable; ++i)
        {
            const float ph = (float) i / (float) kTable;
            tables[0][(size_t) i] = std::sin (kTwoPi * ph);
            float saw = 0, sq = 0, vox = 0;
            for (int h = 1; h <= 32; ++h)
            {
                saw += std::sin (kTwoPi * ph * (float) h) / (float) h;
                if (h & 1) sq += std::sin (kTwoPi * ph * (float) h) / (float) h;
                const float formant = std::exp (-std::pow (((float) h * 130.0f - 700.0f) / 300.0f, 2.0f)) + 0.6f * std::exp (-std::pow (((float) h * 130.0f - 1200.0f) / 300.0f, 2.0f));
                vox += std::sin (kTwoPi * ph * (float) h) * (formant + 0.05f / (float) h);
            }
            tables[1][(size_t) i] = saw * 0.55f; tables[2][(size_t) i] = sq * 0.9f; tables[3][(size_t) i] = vox * 0.5f;
        }
    }
    void buildGrainSource()
    {
        // Default grain source: 2 s of a slowly beating minor chord through a soft low-pass.
        const int n = (int) (sr * 2.0);
        grainSrc.setSize (2, n);
        Svf lp; lp.set (2500.0f, 0.7f, sr);
        double p[4] {};
        const float fr[4] = { 130.81f, 155.56f, 196.0f, 261.63f };
        for (int i = 0; i < n; ++i)
        {
            float x = 0;
            for (int k = 0; k < 4; ++k) { p[k] += fr[k] * (1.0 + 0.002 * std::sin (i * 0.0001 * (k + 1))) / sr; if (p[k] > 1) p[k] -= 1; x += (float) (2.0 * p[k] - 1.0); }
            x = lp.tickLP (x * 0.2f);
            grainSrc.setSample (0, i, x); grainSrc.setSample (1, i, x);
        }
    }

    std::string name;
    int roleId;
    std::vector<SynthModel> models;
    float dCut, dRes, dEnv, dGate, dPocket;
    double sr = 48000.0;
    std::array<Voice, 16> voices;
    std::vector<int> held;
    EchoScheduler echoes;
    std::vector<NoteEvent> events;
    RoleOutput out;
    Ensemble ens;
    SampleSlot sample;
    Noise rng;
    float ensembleAmt = 0.0f, tuneCache = 0.0f;
    int lastModel = -1;
    static constexpr int kTable = 2048;
    std::vector<float> tables[4];
    juce::AudioBuffer<float> grainSrc;
    std::atomic<int> activeVoices { 0 };
};

ModulePtr makeBASS() { return std::make_unique<SynthModule> ("BASS", ROLE_BASS, bassModels(), 0.35f, 0.3f, 0.4f, 0.5f, 0.6f); }
ModulePtr makeLEAD() { return std::make_unique<SynthModule> ("LEAD", ROLE_LEAD, leadModels(), 0.4f, 0.5f, 0.5f, 0.5f, 0.2f); }
ModulePtr makePAD()  { return std::make_unique<SynthModule> ("PAD", ROLE_PAD, padModels(), 0.55f, 0.1f, 0.1f, 0.75f, 0.3f); }
} // namespace sf
