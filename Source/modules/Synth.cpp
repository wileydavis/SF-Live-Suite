// BASS, LEAD, PAD: one synth engine, role-specific model lists and defaults.
// Voices render oversampled from band-limited mipmapped tables (saw, square, triangle, PWM as the
// difference of two saws, BLEP-corrected hard sync): 2x for the linear-filter models, 4x for the
// models with a driven ladder / MS20 filter and for hard sync. DRIVE and the MS20 saturation use
// antiderivative anti-aliasing on top.
// Halfband decimators bring the sums back down: no audible aliasing at any pitch.
#include "Kit.h"
#include "../core/BandLimited.h"
#include "../core/Oversample.h"
#include "Calibration.h"

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

// Hard-sync saw (master resets slave), polyBLEP at every edge. One sample of delay so the
// sample before an edge can be corrected too.
struct SyncSaw
{
    double pm = 0.0, ps = 0.0;
    float held = 0.0f;
    float tick (double im, double is)
    {
        pm += im; ps += is;
        float now = 0.0f, before = 0.0f;
        auto step = [&] (double jump, double d)            // d: samples since the edge (0..1)
        {
            now += (float) (0.5 * jump * (2.0 * d - d * d - 1.0));
            before += (float) (0.5 * jump * d * d);
        };
        if (pm >= 1.0)
        {
            pm -= 1.0;
            const double d = pm / im;
            double pr = ps - d * is;                        // slave phase at the reset
            if (pr >= 1.0) { step (-2.0, (ps - 1.0) / is); pr -= 1.0; }
            step (-2.0 * pr, d);
            ps = d * is;
        }
        else if (ps >= 1.0)
        {
            ps -= 1.0;
            step (-2.0, ps / is);
        }
        const float out = held + before;
        held = (float) (2.0 * ps - 1.0) + now;
        return out;
    }
};

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
    void reset() { for (auto& x : d) x.clear(); }
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
           LAYERMODEL, LAYER, CHORD, SPACE, PDEPTH, PSHAPE, ESPACE, ESWING, ALLOCON, KEYFOLLOW };

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
                 toggle ("alloc", "ALLOC", true).noPreset(),
                 toggle ("key_follow", "KEY FOLLOW", true).noPreset() };
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
        osr = 2.0 * rate;
        waves = WaveSet::get (osr);
        dec.prepare (rate);
        for (auto& d : dec4) d.set (HalfbandDesign::forStage (rate, 1));
        out.prepare (rate, mb, roleId, name.c_str(), true);
        ens.prepare (rate);
        events.reserve (2048);
        held.reserve (128);
        buildGrainSource();
        reset();
    }
    void reset() override
    {
        echoes.reset();
        for (auto& v : voices) v.active = false;
        held.clear();
        dec.reset();
        for (auto& d : dec4) d.reset();
        ens.reset();
        out.reset();
        activeVoices = 0;
    }

    // Control values of a voice, computed on the stream's 32-sample grid (and at a note start),
    // so the voice sounds the same at any host buffer size.
    struct VoiceCtl
    {
        float res = 0, driveG = 1, driveNorm = 1, color = 0.5f, det = 0.5f, cutT = 0.5f, gainAll = 0, subMul = 1, pw2 = 0.5f, osc2Amt = 0, subLvl = 0, sideK = 1;
        float detCents = 0, uniR[8] {}, pw[8] {}, r2[8] {};
        int uni = 1, shape1 = 0, oct1[8] {}, oct2[8] {}, octSub = 0;
        bool doSub = false;
    };
    struct Voice
    {
        bool active = false, releasing = false;
        int note = 60, gen = 0, model = 0, layer = 0, autoOff = -1;
        float vel = 1.0f, gain = 1.0f, freq = 110.0f, target = 110.0f, glideCoef = 0.0f, accent = 0.0f;
        double rate = 96000.0;            // render rate: 2x, or 4x for ladder / MS20 models
        int os = 2;
        double ph[8] {}, ph2[8] {}, subPh = 0.0, modPh = 0.0, spos = 0.0;
        float pan[8] {};
        Adsr amp, fenv;
        SyncSaw sync;
        AdaaTanh driveSat, ms20Sat;
        VoiceCtl ctl;
        bool ctlStale = true;
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
        const SampleData* smp = sample.audio();
        pitchOffset = c.v (TUNE) + (c.b (KEYFOLLOW) ? (float) keyOffset (c.keyRoot) : 0.0f);
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
        for (int i0 = 0; i0 < c.n;)
        {
            // Sub-blocks end at the next event: every note starts on its own sample.
            while (e < events.size() && events[e].offset <= i0)
            {
                const auto& ev = events[e++];
                if (ev.on)
                {
                    noteOn (c, modelA, ev, 1.0f / (1.0f + layer * 0.5f), 0, autoOff, relMul);
                    if (layer > 0.01f) noteOn (c, modelB, ev, layer / (1.0f + layer * 0.5f), 1, autoOff, relMul);
                }
                else noteOff (ev.note, ev.gen, relMul);
            }
            // Sub-blocks also sit on a fixed grid of the stream (block-size independent sound).
            int n = std::min (kSub - (int) ((c.samplePos + i0) % kSub), c.n - i0);
            if (e < events.size()) n = std::min (n, std::max (1, events[e].offset - i0));
            std::fill (L2, L2 + 2 * n, 0.0f);
            std::fill (R2, R2 + 2 * n, 0.0f);
            bool any4 = false;
            const bool onGrid = (c.samplePos + i0) % kSub == 0;
            for (auto& v : voices)
                if (v.active)
                {
                    if (v.os == 4)
                    {
                        if (! any4) { std::fill (L4, L4 + 4 * n, 0.0f); std::fill (R4, R4 + 4 * n, 0.0f); any4 = true; }
                        render (c, v, L4, R4, 4 * n, smp, onGrid);
                    }
                    else render (c, v, L2, R2, 2 * n, smp, onGrid);
                }
            // 4x voices: down to 2x (always run, so the filters' state stays continuous).
            if (! any4) { std::fill (L4, L4 + 4 * n, 0.0f); std::fill (R4, R4 + 4 * n, 0.0f); }
            for (int i = 0; i < 2 * n; ++i)
            {
                L2[i] += dec4[0].down (L4[2 * i], L4[2 * i + 1]);
                R2[i] += dec4[1].down (R4[2 * i], R4[2 * i + 1]);
            }
            dec.process (0, L2, L + i0, n);
            dec.process (1, R2, R + i0, n);
            i0 += n;
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
        const float tune = pitchOffset;
        const bool accent = ev.vel > 0.85f && ev.gen == 0;
        if (m.mono)
        {
            // Mono: legato slides (acid), else retrigger. Note stack for releases.
            // DENSITY echoes never interrupt a note that is still held.
            if (ev.gen > 0 && ! held.empty()) return;
            if (layerId == 0 && ev.gen == 0 && held.size() < 120) held.push_back (ev.note);
            Voice* v = nullptr;
            for (auto& x : voices) if (x.active && x.layer == layerId && x.model == modelIdx && ! x.releasing) v = &x;
            const bool legato = v != nullptr && ev.gen == 0 && held.size() > 1;
            if (v == nullptr) v = &voices[(size_t) pickVoice (voices)];
            setupVoice (c, *v, m, modelIdx, ev, (float) ev.note + tune, gain, layerId, autoOff, relMul, ! legato, accent);
            if (legato) v->glideCoef = std::exp (-1.0f / (0.06f * (float) v->rate * (m.acid ? 1.0f : 0.5f + c.v (GLIDE) * 3.0f)));
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
            v.os = (m.filt == F_LADDER || m.filt == F_MS20 || m.engine == E_SYNC) ? 4 : 2;
            v.rate = sr * v.os;
            v.freq = wasActive ? keepFreq : noteHz (note);
            for (int k = 0; k < 8; ++k) { v.ph[k] = rng.uni(); v.ph2[k] = rng.uni(); v.pan[k] = 0.5f + (k == 0 ? 0.0f : ((k & 1) ? -0.35f : 0.35f) * (0.5f + 0.5f * rng.uni())); }
        }
        v.active = true; v.releasing = false;
        v.note = ev.note; v.gen = ev.gen; v.model = modelIdx; v.layer = layerId;
        v.vel = 0.55f + 0.45f * lerp (1.0f, ev.vel, 0.6f);
        v.gain = gain;
        v.target = noteHz (note);
        v.glideCoef = c.v (GLIDE) > 0.02f && ! m.acid ? std::exp (-1.0f / ((0.005f + c.v (GLIDE) * 0.4f) * (float) v.rate)) : 0.0f;
        if (! wasActive) v.freq = v.target;
        v.accent = accent ? c.v (ACCENT) : 0.0f;
        const float decK = std::pow (2.0f, (c.v (DECAY) - 0.5f) * 4.0f) * std::pow (0.6f, (float) ev.gen) * (1.0f - v.accent * 0.4f);
        const float attK = c.v (ATTACK) * 2.0f;
        v.amp.set (m.att + attK, m.dec * (m.acid ? 1.0f : decK), m.sus, m.rel * relMul, v.rate);
        v.fenv.set (m.fAtt + attK * 0.5f, m.fDec * decK, m.fSus, m.fRel * relMul, v.rate);
        v.amp.on (retrigger || ! wasActive);
        v.fenv.on (retrigger || ! wasActive || m.acid);
        v.autoOff = autoOff > 0 ? autoOff * v.os : autoOff;
        v.t = 0.0f;
        v.ctlStale = true;
    }

    void noteOff (int note, int gen, float relMul)
    {
        juce::ignoreUnused (relMul);
        if (note < 0) { for (auto& v : voices) { v.amp.off(); v.fenv.off(); v.releasing = true; } held.clear(); return; }
        if (gen == 0)
        {
            held.erase (std::remove (held.begin(), held.end(), note), held.end());
            // Mono: return to the previous held note (slide), every layer.
            bool slid = false;
            for (auto& v : voices)
                if (v.active && ! v.releasing && v.note == note && models[(size_t) v.model].mono && ! held.empty())
                {
                    v.note = held.back();
                    v.target = noteHz ((float) held.back() + pitchOffset);
                    v.glideCoef = std::exp (-1.0f / (0.06f * (float) v.rate));
                    slid = true;
                }
            if (slid) return;
            // A played note's release ends its voice even if an echo retriggered it.
            for (auto& v : voices)
                if (v.active && ! v.releasing && v.note == note && (v.gen == 0 || models[(size_t) v.model].mono)) { v.amp.off(); v.fenv.off(); v.releasing = true; }
            return;
        }
        // Echo releases only end echo voices, never a voice a held note owns.
        for (auto& v : voices)
            if (v.active && ! v.releasing && v.note == note && v.gen == gen && ! (models[(size_t) v.model].mono && ! held.empty()))
            { v.amp.off(); v.fenv.off(); v.releasing = true; }
    }

    // Renders n samples at the voice's oversampled rate (v.rate) into L/R (accumulating).
    void updateControls (Ctx& c, Voice& v, int n)
    {
        const auto& m = models[(size_t) v.model];
        const auto& W = *waves;
        auto& k = v.ctl;
        const double vr = v.rate;
        const float invOsr = (float) (1.0 / vr);
        const float cutK = c.v (CUTOFF), envA = c.v (ENVAMT) * m.envMul * (1.0f + v.accent);
        k.res = clamp01 (c.v (RES) * m.resMul);
        const float drive = clamp01 (c.v (DRIVE) + m.drive);
        k.driveG = drive > 0.001f ? 1.0f + drive * 6.0f : 0.0f;
        k.driveNorm = 1.2f / std::sqrt (std::max (1.0f, k.driveG));
        k.color = c.v (COLOR); k.det = c.v (DETUNE);
        const float fe = v.fenv.v;
        const float keyTrack = std::log2 (std::max (20.0f, v.freq) / 130.0f) * 0.3f;
        k.cutT = clamp01 (cutK + envA * fe * 0.6f + keyTrack * 0.1f) * std::pow (0.75f, (float) v.gen);
        const float cutHz = expMap (k.cutT, 35.0f, 18000.0f);
        switch (m.filt)
        {
            case F_LADDER: v.ladder.set (cutHz, k.res * 0.98f, vr); break;
            case F_SVF:    v.svf.set (cutHz, 0.6f + k.res * 8.0f, vr); break;
            case F_MS20:   v.svf.set (cutHz, 0.6f + k.res * 12.0f, vr); v.svf2.set (std::max (20.0f, cutHz * 0.08f), 0.7f + k.res * 3.0f, vr); break;
            case F_NONE: break;
        }
        if (m.engine == E_NOISE) v.nbp.set (v.freq, 4.0f + k.color * 40.0f, vr);
        k.uni = std::max (1, std::min (8, m.unison));
        k.detCents = m.detune * 40.0f * (0.2f + k.det * 1.6f);
        k.gainAll = v.gain * v.vel * (1.0f + v.accent * 0.5f) * 0.32f * m.gain;
        // Pitch multipliers that move slowly.
        k.subMul = 1.0f;
        if (m.pitchDive > 0.0f) k.subMul *= semis (-m.pitchDive * std::exp (-v.t / 0.08f));
        if (m.wow > 0.0f)
        {
            v.drift += 0.3f * (float) n * invOsr; if (v.drift > 1.0f) v.drift -= 1.0f;
            k.subMul *= semis (m.wow * (0.12f * std::sin (kTwoPi * v.drift) + 0.03f * std::sin (kTwoPi * v.drift * 17.0f)));
        }
        const float fTop = std::max (v.freq, v.target) * k.subMul;
        k.shape1 = m.engine == E_HOOVER ? 1 : (m.acid ? (k.color < 0.5f ? 0 : 1) : m.wave);
        for (int u = 0; u < k.uni; ++u)
        {
            const float cents = k.uni == 1 ? 0.0f : k.detCents * ((float) u / (float) (k.uni - 1) - 0.5f) * 2.0f;
            k.uniR[u] = semis (cents / 100.0f);
            k.pw[u] = m.engine == E_HOOVER ? 0.5f + 0.35f * std::sin (kTwoPi * (v.t * 0.7f + (float) u * 0.3f)) : 0.5f;
            k.r2[u] = m.ratio * semis ((k.detCents * 0.5f + k.det * 7.0f * (m.unison > 1 ? 0.0f : 1.0f)) / 100.0f);
            k.oct1[u] = W.saw.octaveFor (fTop * k.uniR[u]);
            k.oct2[u] = W.saw.octaveFor (fTop * k.uniR[u] * k.r2[u]);
        }
        k.octSub = W.square.octaveFor (fTop * 0.5f);
        k.pw2 = 0.3f + k.color * 0.4f;
        k.osc2Amt = m.osc2 * (m.acid ? 0.0f : 1.0f);
        k.doSub = m.sub > 0.0f || (m.engine == E_SUB && ! m.acid && k.color > 0.0f && m.mono);
        k.subLvl = m.sub * (m.mono ? (0.5f + k.color) : 1.0f) * 0.7f;
        k.sideK = clamp01 (k.cutT * 1.5f);
    }

    void render (Ctx& c, Voice& v, float* L, float* R, int n, const SampleData* smp, bool onGrid)
    {
        const auto& m = models[(size_t) v.model];
        const auto& W = *waves;
        const double vr = v.rate;
        const float invOsr = (float) (1.0 / vr);
        if (onGrid || v.ctlStale) { updateControls (c, v, kSub * v.os); v.ctlStale = false; }
        const auto& k = v.ctl;
        const float res = k.res; juce::ignoreUnused (res);
        const float driveG = k.driveG, driveNorm = k.driveNorm, color = k.color, det = k.det, gainAll = k.gainAll, subMul = k.subMul;
        const int uni = k.uni, shape1 = k.shape1, octSub = k.octSub;
        const float* uniR = k.uniR; const float* pw = k.pw; const float* r2 = k.r2;
        const int* oct1 = k.oct1; const int* oct2 = k.oct2;
        const float pw2 = k.pw2, osc2Amt = k.osc2Amt, subLvl = k.subLvl, sideK = k.sideK;
        const bool doSub = k.doSub;
        const float drive = driveG > 0.0f ? 1.0f : 0.0f;
        auto sinT = [&W] (double ph) { return W.frame[0].read (0, ph); };
        // Band-limited oscillator read: 0 saw, 1 pulse (pw), 2 triangle, 3 sine.
        auto osc = [&W, &sinT] (int shape, int oct, double ph, float pwv) -> float
        {
            switch (shape)
            {
                case 0: return W.saw.read (oct, ph);
                case 1:
                {
                    if (std::abs (pwv - 0.5f) < 1e-3f) return W.square.read (oct, ph);
                    double p2 = ph - pwv; if (p2 < 0.0) p2 += 1.0;
                    return (2.0f * pwv - 1.0f) - (W.saw.read (oct, ph) - W.saw.read (oct, p2));
                }
                case 2: return W.tri.read (oct, ph);
                default: return sinT (ph);
            }
        };

        for (int i = 0; i < n; ++i)
        {
            if (v.glideCoef > 0.0f) v.freq = v.target + v.glideCoef * (v.freq - v.target); else v.freq = v.target;
            const float f = v.freq * subMul;
            const double inc = (double) f * invOsr;
            const float a = v.amp.tick();
            v.fenv.tick();
            float x = 0.0f, xl = 0.0f, xr = 0.0f;
            bool stereo = false;
            switch (m.engine)
            {
                case E_SUB:
                case E_HOOVER:
                {
                    for (int u = 0; u < uni; ++u)
                    {
                        float y = osc (shape1, oct1[u], v.ph[u], pw[u]);
                        v.ph[u] += inc * uniR[u]; if (v.ph[u] >= 1.0) v.ph[u] -= 1.0;
                        if (osc2Amt > 0.0f && m.engine != E_HOOVER)
                        {
                            y += osc (m.wave2, oct2[u], v.ph2[u], pw2) * osc2Amt;
                            v.ph2[u] += inc * uniR[u] * r2[u]; if (v.ph2[u] >= 1.0) v.ph2[u] -= 1.0;
                        }
                        if (uni > 1) { xl += y * (1.0f - v.pan[u]); xr += y * v.pan[u]; stereo = true; }
                        else x += y;
                    }
                    if (uni > 1) { const float k = 1.0f / std::sqrt ((float) uni); xl *= k; xr *= k; }
                    if (doSub)
                    {
                        const float sq = W.square.read (octSub, v.subPh) * subLvl;
                        v.subPh += inc * 0.5; if (v.subPh >= 1.0) v.subPh -= 1.0;
                        x += sq; xl += sq; xr += sq;
                    }
                    if (m.noise > 0.0f) { const float nz = rng.next() * m.noise; x += nz; xl += nz; xr += nz; }
                    break;
                }
                case E_ORGAN:
                {
                    static const double h[6] = { 1, 2, 3, 4, 6, 8 };
                    const float w[6] = { 1.0f, 0.8f * color + 0.2f, 0.9f, 0.5f * color, 0.3f, 0.25f * color };
                    v.ph[0] += inc; if (v.ph[0] >= 1.0) v.ph[0] -= 1.0;
                    for (int k = 0; k < 6; ++k) { const double p = v.ph[0] * h[k]; x += sinT (p - std::floor (p)) * w[k]; }
                    x *= 0.35f;
                    if (v.t < 0.004f) x += rng.next() * 0.3f * (1.0f - v.t / 0.004f);       // key click
                    { const double p = v.ph[0] * 3.0; x += sinT (p - std::floor (p)) * 0.4f * std::exp (-v.t / 0.2f); }   // percussion
                    break;
                }
                case E_FM:
                {
                    const float idx = (0.5f + color * 6.0f) * (0.3f + 0.7f * v.fenv.v) * (1.0f + v.accent);
                    v.modPh += inc * m.ratio * (0.98 + det * 0.04); if (v.modPh >= 1.0) v.modPh -= 1.0;
                    double mp = v.modPh + v.fb * (0.3f / kTwoPi); mp -= std::floor (mp);
                    const float mod = sinT (mp);
                    v.fb = mod;
                    v.ph[0] += inc; if (v.ph[0] >= 1.0) v.ph[0] -= 1.0;
                    double cp = v.ph[0] + idx * mod / kTwoPi; cp -= std::floor (cp);
                    x = sinT (cp);
                    break;
                }
                case E_SYNC:
                {
                    // Hard sync: slave saw restarts with the master; every edge BLEP-corrected.
                    const float ratio = 1.0f + (color * 4.0f + v.fenv.v * c.v (ENVAMT) * 6.0f);
                    x = v.sync.tick (inc, std::min (0.45, inc * ratio)) * 0.8f;
                    break;
                }
                case E_NOISE:
                    x = v.nbp.tickBP (rng.next()) * (2.0f + color * 6.0f) + rng.next() * 0.05f * (1.0f - color);
                    break;
                case E_WAVETABLE:
                {
                    const float pos = clamp01 (color + 0.15f * std::sin (kTwoPi * v.t * 0.13f)) * 3.0f;
                    const int t0 = std::min (2, (int) pos);
                    const float fr = pos - (float) t0;
                    x = lerp (W.frame[t0].read (oct1[0], v.ph[0]), W.frame[t0 + 1].read (oct1[0], v.ph[0]), fr);
                    v.ph[0] += inc; if (v.ph[0] >= 1.0) v.ph[0] -= 1.0;
                    break;
                }
                case E_GRAIN:
                {
                    const auto& src = smp != nullptr ? smp->buf : grainSrc;
                    const int len = src.getNumSamples();
                    const float root = smp != nullptr && smp->rootHz > 0 ? smp->rootHz : 130.81f;
                    const int glen = (int) ((0.03f + color * 0.25f) * sr);       // source samples
                    if (len > glen + 4 && --v.grainClock <= 0)
                    {
                        v.grainClock = std::max (1, glen * v.os / 4);             // voice-rate samples (= glen / 4 at 1x)
                        for (auto& gr : v.grains)
                            if (! gr.on)
                            {
                                gr.on = true; gr.age = 0; gr.len = glen * v.os;
                                const float scatter = det * 0.3f;
                                gr.pos = std::fmod (v.t * 0.05f * sr + rng.uni() * scatter * len, (double) (len - glen - 4));
                                if (gr.pos < 0) gr.pos += len - glen - 4;
                                gr.rate = (f / root) / v.os * (1.0 + (rng.uni() - 0.5f) * det * 0.02f);
                                gr.pan = 0.5f + (rng.uni() - 0.5f) * 0.8f;
                                break;
                            }
                    }
                    stereo = true;
                    const float* s0 = src.getReadPointer (0);
                    const float* s1 = src.getReadPointer (src.getNumChannels() > 1 ? 1 : 0);
                    for (auto& gr : v.grains)
                    {
                        if (! gr.on) continue;
                        if ((int) gr.pos + 2 >= len || gr.age >= gr.len) { gr.on = false; continue; }
                        const float w = 0.5f - 0.5f * std::cos (kTwoPi * (float) gr.age / (float) gr.len);
                        const float y = (hermiteAt (s0, len, gr.pos) + hermiteAt (s1, len, gr.pos)) * 0.5f * w;
                        xl += y * (1.0f - gr.pan); xr += y * gr.pan;
                        gr.pos += gr.rate; ++gr.age;
                    }
                    break;
                }
                case E_SAMPLE:
                {
                    if (smp == nullptr) { v.active = false; return; }
                    const int len = smp->length();
                    if ((int) v.spos + 2 >= len) { v.active = false; return; }
                    xl = hermiteAt (smp->buf.getReadPointer (0), len, v.spos);
                    xr = hermiteAt (smp->buf.getReadPointer (smp->buf.getNumChannels() > 1 ? 1 : 0), len, v.spos);
                    stereo = true;
                    v.spos += (double) f / (double) noteHz (smp->rootNote) / v.os;
                    break;
                }
            }
            if (! stereo) { xl = x; xr = x; }
            // Filter (mono filters run on the mid; stereo engines get mid/side filtered as one).
            auto filt = [&] (float in) -> float
            {
                if (drive > 0.0f) in = v.driveSat.tick (in * driveG) * driveNorm;
                switch (m.filt)
                {
                    case F_LADDER: return v.ladder.tick (in);
                    case F_SVF: return v.svf.tickLP (in);
                    case F_MS20: return v.ms20Sat.tick (v.svf2.tickHP (v.svf.tickLP (in)) * 1.5f);
                    default: return in;
                }
            };
            float yl, yr;
            if (stereo && m.filt != F_NONE)
            {
                const float mid = filt (0.5f * (xl + xr));
                const float side = 0.5f * (xl - xr);
                yl = mid + side * sideK; yr = mid - side * sideK;
            }
            else if (stereo) { yl = xl; yr = xr; }
            else { yl = yr = filt (xl); }
            L[i] += yl * a * gainAll;
            R[i] += yr * a * gainAll;
            v.t += invOsr;
            if (v.autoOff > 0 && --v.autoOff == 0 && ! v.releasing) { v.amp.off(); v.fenv.off(); v.releasing = true; }
            if (v.amp.st == Adsr::Idle && v.t > 0.001f) { v.active = false; return; }
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
    float ensembleAmt = 0.0f, pitchOffset = 0.0f;
    int lastModel = -1;
    double osr = 96000.0;
    std::shared_ptr<const WaveSet> waves;
    Decimator2x dec;
    Halfband dec4[2];
    static constexpr int kSub = 32;
    float L2[2 * kSub] {}, R2[2 * kSub] {}, L4[4 * kSub] {}, R4[4 * kSub] {};
    juce::AudioBuffer<float> grainSrc;
    std::atomic<int> activeVoices { 0 };
};

// Gain staging (Calibration.h): every model at its role's target loudness.
template <size_t N>
static std::vector<SynthModel> calibrated (std::vector<SynthModel> m, const float (&db)[N])
{
    for (size_t i = 0; i < m.size() && i < N; ++i) m[i].gain *= cal::gain (db[i]);
    return m;
}
ModulePtr makeBASS() { return std::make_unique<SynthModule> ("BASS", ROLE_BASS, calibrated (bassModels(), cal::bass), 0.35f, 0.3f, 0.4f, 0.5f, 0.6f); }
ModulePtr makeLEAD() { return std::make_unique<SynthModule> ("LEAD", ROLE_LEAD, calibrated (leadModels(), cal::lead), 0.4f, 0.5f, 0.5f, 0.5f, 0.2f); }
ModulePtr makePAD()  { return std::make_unique<SynthModule> ("PAD", ROLE_PAD, calibrated (padModels(), cal::pad), 0.55f, 0.1f, 0.1f, 0.75f, 0.3f); }
} // namespace sf
