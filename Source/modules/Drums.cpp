// KICK, PERC, RUMBLE
#include "Kit.h"

namespace sf
{
// =====================================================================================  KICK
struct KickModel { const char* name; float f1, ratio, tau, decay, click, clickHz, drive, shape, lp, fm, gain; };
static const KickModel kKickModels[] = {
    { "KICK 909-TYPE",    50.0f, 5.5f, 0.007f, 0.35f, 0.60f, 3500.0f, 0.25f, 0.25f,  8000.0f, 0.00f, 1.0f },
    { "KICK 808-TYPE",    46.0f, 2.4f, 0.020f, 0.90f, 0.08f, 2000.0f, 0.05f, 0.00f,  5000.0f, 0.00f, 0.85f },
    { "KICK SINE",        48.0f, 3.0f, 0.012f, 0.50f, 0.05f, 3000.0f, 0.00f, 0.00f, 12000.0f, 0.00f, 0.62f },
    { "KICK INDUSTRIAL",  52.0f, 7.0f, 0.006f, 0.45f, 0.70f, 4500.0f, 0.85f, 0.30f,  9000.0f, 0.00f, 0.6f },
    { "KICK MODULAR",     55.0f, 4.0f, 0.015f, 0.30f, 0.30f, 2500.0f, 0.30f, 0.60f,  3000.0f, 0.35f, 0.72f },
    { "KICK DUB",         44.0f, 2.0f, 0.025f, 0.60f, 0.00f, 2000.0f, 0.10f, 0.00f,   600.0f, 0.00f, 0.8f },
    { "SAMPLE",           50.0f, 1.0f, 0.010f, 0.50f, 0.00f, 2000.0f, 0.00f, 0.00f, 20000.0f, 0.00f, 1.0f } };
static constexpr int kNumKickModels = 7;
static std::vector<std::string> kickModelNames() { std::vector<std::string> v; for (auto& m : kKickModels) v.push_back (m.name); return v; }

class KickModule : public Module
{
public:
    enum { TUNE, DECAY, PUNCH, CLICK, DRIVE, FILT, DENS, LEVEL, MODEL, LAYERMODEL, LAYER, PTIME, VELSENS, ESPACE, ESWING, KEYTRACK, RES, ALLOCON };
    std::string id() const override { return "KICK"; }
    bool instrument() const override { return true; }
    std::vector<ParamSpec> params() const override
    {
        return { knob ("tune", "TUNE", -12.0f, 12.0f, 0.0f, "st").prim(),
                 pct ("decay", "DECAY", 0.5f).prim().dir (1),
                 pct ("punch", "PUNCH", 0.5f).prim().dir (1),
                 pct ("click", "CLICK", 0.5f).prim(),
                 pct ("drive", "DRIVE", 0.2f).prim().dir (1),
                 bip ("filter", "FILTER").prim(),
                 pct ("density", "DENSITY", 0.0f).prim(),
                 levelParam (-3.0f).prim(),
                 choice ("model", "MODEL", kickModelNames(), 0),
                 choice ("layer_model", "LAYER MODEL", kickModelNames(), 1),
                 pct ("layer", "LAYER", 0.0f),
                 pct ("pitch_time", "PITCH TIME", 0.5f),
                 pct ("vel_sens", "VEL SENS", 0.5f),
                 choice ("echo_space", "ECHO SPACE", echoSpaceNames(), 1),
                 pct ("echo_swing", "ECHO SWING", 0.0f),
                 toggle ("key_track", "KEY TRACK"),
                 pct ("res", "FILTER RES", 0.1f),
                 toggle ("alloc", "ALLOC", true) };
    }
    bool acceptsFiles() const override { return true; }
    juce::String fileHint() const override { return "DROP ONE-SHOT (MODEL: SAMPLE)"; }
    void loadFiles (const juce::StringArray& f) override { if (f.size() > 0) sample.set (loadSampleFile (juce::File (f[0]), sr)); }
    void save (juce::ValueTree& t) const override { sample.save (t, "SAMPLE"); }
    void load (const juce::ValueTree& t) override { sample.load (t, "SAMPLE", sr); }

    void prepare (double rate, int mb) override
    {
        if (auto s = sample.get(); s != nullptr && std::abs (s->sr - rate) > 1 && s->flac.getSize() > 0) sample.set (loadSampleFlac (s->flac, s->name, rate));
        sr = rate;
        out.prepare (rate, mb, ROLE_KICK, "KICK", false);
        echoes.reset();
        for (auto& v : voices) v.active = false;
    }

    struct Voice
    {
        bool active = false;
        int model = 0;
        double t = 0.0, phase = 0.0;
        float vel = 1, f0 = 200, f1 = 50, tau = 0.01f, decay = 0.3f, click = 0, clickHz = 3000, drive = 0, shape = 0, lp = 8000, fm = 0;
        float fade = 1.0f, fadeStep = 0.0f;
        float lpZ = 0, nHp = 0;
        Svf res;
        size_t spos = 0; float srate = 1.0f;
        float level() const { return active ? vel * std::exp (-(float) t / std::max (0.01f, decay)) : 0.0f; }
    };

    void trigger (Ctx& c, int model, float vel, int gen, int note, float layerGain, bool publish, int offset)
    {
        // Fade any ringing voice over 3 ms.
        for (auto& v : voices) if (v.active && v.fadeStep == 0.0f) v.fadeStep = 1.0f / (0.003f * (float) sr);
        int idx = 0;
        for (int i = 0; i < 4; ++i) if (! voices[(size_t) i].active) { idx = i; break; }
        auto& v = voices[(size_t) idx];
        const auto& m = kKickModels[model];
        const float tune = c.v (TUNE) + (c.b (KEYTRACK) ? (float) (note - 36) : 0.0f);
        const float decScale = std::pow (2.0f, (c.v (DECAY) - 0.5f) * 4.0f) * std::pow (0.6f, (float) gen);
        v = Voice();
        v.active = true; v.model = model;
        v.vel = vel * layerGain * m.gain;
        v.f1 = m.f1 * semis (tune);
        v.f0 = v.f1 * (1.0f + (m.ratio - 1.0f) * std::pow (2.0f, (c.v (PUNCH) - 0.5f) * 2.0f));
        v.tau = m.tau * std::pow (2.0f, (c.v (PTIME) - 0.5f) * 3.0f);
        v.decay = m.decay * decScale;
        v.click = m.click * c.v (CLICK) * 2.0f;
        v.clickHz = m.clickHz;
        v.drive = clamp01 (m.drive + c.v (DRIVE) * 0.8f);
        v.shape = m.shape; v.fm = m.fm;
        v.lp = m.lp * std::pow (0.6f, (float) gen);
        v.srate = semis (tune);
        if (publish)
        {
            float hp, lp;
            DJFilter::corners (c.v (FILT), hp, lp);
            float f1r = v.f1, f0r = v.f0, decr = v.decay;
            if (model == kNumKickModels - 1)
                if (auto s = sample.get()) { f1r = s->rootHz > 20 && s->rootHz < 200 ? s->rootHz * v.srate : 50.0f; f0r = f1r * 2.0f; decr = std::max (0.05f, s->decaySec * 0.4f * decScale); }
            c.bus->publishKick ({ c.ppqAt (offset), v.vel, f0r, f1r, v.tau, std::min (decr, lp < 200.0f ? 0.05f : decr), hp, lp });
        }
    }

    float renderVoice (Voice& v, const SampleData* s)
    {
        const float t = (float) v.t;
        float y = 0.0f;
        if (v.model == kNumKickModels - 1)
        {
            if (s == nullptr || v.spos + 1 >= (size_t) s->length()) { v.active = false; return 0.0f; }
            const size_t i0 = v.spos;
            y = 0.5f * (s->buf.getSample (0, (int) i0) + s->buf.getSample (1, (int) i0)) * v.vel;
            y *= std::exp (-t / std::max (0.05f, v.decay * 2.0f));
            v.t += 1.0 / sr;
            v.phase += v.srate;
            v.spos = (size_t) v.phase;
        }
        else
        {
            const float f = v.f1 + (v.f0 - v.f1) * std::exp (-t / v.tau);
            float ph = (float) v.phase;
            if (v.fm > 0.0f) ph += v.fm * rng.next() * 0.02f * std::exp (-t / 0.02f);
            const float sine = std::sin (kTwoPi * ph);
            const float tri = 4.0f * std::abs (ph - std::floor (ph + 0.5f)) - 1.0f;
            const float body = lerp (sine, -tri, v.shape * 0.6f);
            const float amp = std::min (1.0f, t / 0.0005f) * std::exp (-t / v.decay);
            float clickSig = 0.0f;
            if (v.click > 0.0f && t < 0.02f)
            {
                const float n = rng.next();
                v.nHp = n - v.nHp * 0.0f;
                const float a = std::exp (-kTwoPi * v.clickHz / (float) sr);
                v.lpZ = n + a * (v.lpZ - n);
                clickSig = (n - v.lpZ) * std::exp (-t / 0.004f) * v.click * 0.6f + (t < 0.0003f ? v.click * 0.8f : 0.0f);
            }
            y = (body * amp + clickSig) * v.vel;
            if (v.drive > 0.0f) { const float g = 1.0f + v.drive * 6.0f; y = fastTanh (y * g) / std::sqrt (g) * 1.3f; }
            if (v.lp < 15000.0f)
            {
                const float lpf = v.model == 4 ? v.lp + 4000.0f * std::exp (-t / 0.05f) : v.lp;
                if (((int) (v.t * sr) & 15) == 0) v.res.set (lpf, v.model == 4 ? 2.5f : 0.7071f, sr);
                y = v.res.tickLP (y);
            }
            v.phase += f / sr;
            if (v.phase > 1.0) v.phase -= 1.0;
            v.t += 1.0 / sr;
            if (t > v.decay * 9.0f + 0.05f) v.active = false;
        }
        if (v.fadeStep > 0.0f) { v.fade -= v.fadeStep; if (v.fade <= 0.0f) { v.active = false; return 0.0f; } y *= v.fade; }
        return y;
    }

    void process (Ctx& c, juce::AudioBuffer<float>& io, juce::AudioBuffer<float>*, juce::AudioBuffer<float>*, juce::MidiBuffer& midi) override
    {
        echoes.run (c, midi, events, c.v (DENS), echoSpaceBeats (c.ci (ESPACE)), c.v (ESWING), 1.0f, false);
        const auto smp = sample.get();
        const int model = c.ci (MODEL), layerModel = c.ci (LAYERMODEL);
        const float layer = c.v (LAYER);
        const float velSens = c.v (VELSENS);
        size_t e = 0;
        auto* L = io.getWritePointer (0); auto* R = io.getWritePointer (1);
        for (int i = 0; i < c.n; ++i)
        {
            while (e < events.size() && events[e].offset <= i)
            {
                const auto& ev = events[e++];
                if (! ev.on) continue;
                const float vel = ev.gen == 0 ? lerp (1.0f, ev.vel, velSens) : ev.vel;
                trigger (c, model, vel, ev.gen, ev.note, 1.0f / (1.0f + layer * 0.5f), true, i);
                if (layer > 0.01f) trigger (c, layerModel, vel, ev.gen, ev.note, layer / (1.0f + layer * 0.5f), false, i);
                lastHit = 1.0f;
            }
            float y = 0.0f;
            for (auto& v : voices) if (v.active) y += renderVoice (v, smp.get());
            L[i] = y; R[i] = y;
            hist.push (std::fabs (y));
        }
        out.process (c, io, FILT, RES, 0.0f, 0.0f, c.b (ALLOCON), LEVEL);
    }
    int displayHeight() const override { return 70; }
    void paint (juce::Graphics& g, juce::Rectangle<float> r) override
    {
        ui::frame (g, r, "OUTPUT");
        auto in = r.reduced (8, 18);
        juce::Path p;
        constexpr int N = 512;
        for (int i = 0; i < N; ++i)
        {
            const float v = hist.v[(hist.w + i) % N].load();
            const float x = in.getX() + in.getWidth() * (float) i / (float) (N - 1);
            if (i == 0) p.startNewSubPath (x, in.getBottom() - v * in.getHeight()); else p.lineTo (x, in.getBottom() - v * in.getHeight());
        }
        g.setColour (ui::kText); g.strokePath (p, juce::PathStrokeType (1.0f));
        auto s = sample.get();
        ui::label (g, s ? "SAMPLE " + s->name.toUpperCase() : "NO SAMPLE", r.reduced (8, 2).removeFromBottom (14), ui::kFaint, 10.0f, juce::Justification::centredRight);
    }
private:
    struct DecimHist { std::atomic<float> v[512] {}; int w = 0, k = 0; float acc = 0; void push (float x) { acc = std::max (acc, x); if (++k >= 512) { v[w].store (std::min (1.0f, acc)); w = (w + 1) % 512; k = 0; acc = 0; } } };
    double sr = 48000.0;
    std::array<Voice, 4> voices;
    EchoScheduler echoes;
    std::vector<NoteEvent> events;
    RoleOutput out;
    SampleSlot sample;
    Noise rng;
    float lastHit = 0;
    DecimHist hist;
};
ModulePtr makeKICK() { return std::make_unique<KickModule>(); }

// =====================================================================================  PERC
enum PercType { P_METAL, P_CLAP, P_RIM, P_BELL, P_CLAVE, P_SHAKER, P_FM, P_MODAL, P_SAMPLE };
struct PercModel { const char* name; PercType type; float freqMul, bpHz, hpHz, noise, decay; };
static const PercModel kPercModels[] = {
    { "HAT 909-TYPE",  P_METAL,  1.30f, 10000.0f, 7000.0f, 0.50f, 0.09f },
    { "HAT 808-TYPE",  P_METAL,  1.00f,  8000.0f, 6000.0f, 0.15f, 0.07f },
    { "HAT 606-TYPE",  P_METAL,  1.60f, 11000.0f, 9000.0f, 0.60f, 0.05f },
    { "RIDE 909-TYPE", P_METAL,  1.10f,  6000.0f, 3000.0f, 0.20f, 1.20f },
    { "CRASH",         P_METAL,  1.20f,  7000.0f, 4000.0f, 0.70f, 1.80f },
    { "CLAP",          P_CLAP,   1.00f,  1200.0f,  400.0f, 1.00f, 0.15f },
    { "RIM",           P_RIM,    1.00f,  1700.0f,  400.0f, 0.00f, 0.02f },
    { "COWBELL",       P_BELL,   1.00f,  1000.0f,  300.0f, 0.00f, 0.25f },
    { "CLAVE",         P_CLAVE,  1.00f,  2500.0f,  200.0f, 0.00f, 0.04f },
    { "SHAKER",        P_SHAKER, 1.00f,  6000.0f, 2000.0f, 1.00f, 0.08f },
    { "METAL FM",      P_FM,     1.00f,   400.0f,  200.0f, 0.00f, 0.30f },
    { "MODAL",         P_MODAL,  1.00f,   300.0f,   60.0f, 0.00f, 0.60f },
    { "SAMPLE",        P_SAMPLE, 1.00f,  1000.0f,   20.0f, 0.00f, 1.00f } };
static constexpr int kNumPercModels = 13;
static std::vector<std::string> percModelNames() { std::vector<std::string> v; for (auto& m : kPercModels) v.push_back (m.name); return v; }

class PercModule : public Module
{
public:
    enum { TUNE, DECAY, TONE, DENS, FILT, ACCENT, DRIVE, LEVEL, MODEL, ESPACE, ESWING, EPROB, CHOKE, SPREAD, RES, ALLOCON };
    std::string id() const override { return "PERC"; }
    bool instrument() const override { return true; }
    std::vector<ParamSpec> params() const override
    {
        return { knob ("tune", "TUNE", -24.0f, 24.0f, 0.0f, "st").prim(),
                 pct ("decay", "DECAY", 0.5f).prim().dir (1),
                 pct ("tone", "TONE", 0.5f).prim(),
                 pct ("density", "DENSITY", 0.0f).prim().dir (1),
                 bip ("filter", "FILTER").prim(),
                 pct ("accent", "ACCENT", 0.5f).prim(),
                 pct ("drive", "DRIVE", 0.0f).prim(),
                 levelParam (-6.0f).prim(),
                 choice ("model", "MODEL", percModelNames(), 0),
                 choice ("echo_space", "ECHO SPACE", echoSpaceNames(), 1),
                 pct ("echo_swing", "ECHO SWING", 0.0f),
                 pct ("echo_prob", "ECHO PROB", 1.0f),
                 toggle ("choke", "CHOKE", true),
                 pct ("spread", "SPREAD", 0.3f),
                 pct ("res", "FILTER RES", 0.1f),
                 toggle ("alloc", "ALLOC", true) };
    }
    bool acceptsFiles() const override { return true; }
    juce::String fileHint() const override { return "DROP ONE-SHOT (MODEL: SAMPLE)"; }
    void loadFiles (const juce::StringArray& f) override { if (f.size() > 0) sample.set (loadSampleFile (juce::File (f[0]), sr)); }
    void save (juce::ValueTree& t) const override { sample.save (t, "SAMPLE"); }
    void load (const juce::ValueTree& t) override { sample.load (t, "SAMPLE", sr); }
    void prepare (double rate, int mb) override
    {
        if (auto s = sample.get(); s != nullptr && std::abs (s->sr - rate) > 1 && s->flac.getSize() > 0) sample.set (loadSampleFlac (s->flac, s->name, rate));
        sr = rate; out.prepare (rate, mb, ROLE_PERC, "PERC", false); echoes.reset();
        for (auto& v : voices) v.active = false;
    }

    struct Voice
    {
        bool active = false;
        PercType type = P_METAL;
        double t = 0;
        float vel = 1, decay = 0.1f, pan = 0.5f, fade = 1.0f, fadeStep = 0.0f, tune = 1.0f, tone = 0.5f, noise = 0;
        double ph[6] {};
        float freqs[6] {};
        Svf bp, hp, res[6];
        float fmPh = 0, fmMod = 0;
        double spos = 0;
        float level() const { return active ? vel * std::exp (-(float) t / std::max (0.005f, decay)) : 0.0f; }
    };

    void trigger (Ctx& c, int model, const NoteEvent& ev)
    {
        if (c.b (CHOKE)) for (auto& v : voices) if (v.active && v.fadeStep == 0.0f) v.fadeStep = 1.0f / (0.004f * (float) sr);
        int idx = pickVoice (voices);
        auto& v = voices[(size_t) idx];
        const auto& m = kPercModels[model];
        v = Voice();
        v.active = true; v.type = m.type;
        const float acc = c.v (ACCENT);
        v.vel = lerp (0.7f, ev.vel, acc) * (ev.gen > 0 ? 1.0f : 1.0f);
        v.tune = semis (c.v (TUNE));
        v.tone = c.v (TONE);
        v.decay = m.decay * std::pow (2.0f, (c.v (DECAY) - 0.5f) * 4.0f) * std::pow (0.65f, (float) ev.gen);
        v.noise = m.noise;
        v.pan = 0.5f + (ev.gen == 0 ? 0.0f : ((ev.gen & 1) ? -1.0f : 1.0f) * c.v (SPREAD) * 0.5f * (rng.uni() * 0.5f + 0.5f));
        static const float metal[6] = { 205.3f, 304.4f, 369.6f, 522.7f, 540.0f, 800.0f };
        static const float modal[6] = { 1.0f, 1.59f, 2.14f, 2.30f, 2.65f, 2.92f };
        const float dark = std::pow (0.7f, (float) ev.gen);
        switch (m.type)
        {
            case P_METAL:
                for (int k = 0; k < 6; ++k) v.freqs[k] = metal[k] * m.freqMul * v.tune * (0.8f + v.tone * 0.4f);
                v.bp.set (m.bpHz * v.tune * (0.6f + v.tone * 0.8f) * dark, 1.2f, sr);
                v.hp.set (m.hpHz * (0.6f + v.tone * 0.8f) * dark, 0.7071f, sr);
                break;
            case P_CLAP: v.bp.set (m.bpHz * v.tune * (0.7f + v.tone * 0.6f) * dark, 1.5f, sr); v.hp.set (m.hpHz, 0.7071f, sr); break;
            case P_RIM: v.freqs[0] = 1700.0f * v.tune; v.freqs[1] = 500.0f * v.tune; v.hp.set (400.0f, 0.7071f, sr); break;
            case P_BELL: v.freqs[0] = 540.0f * v.tune; v.freqs[1] = 800.0f * v.tune; v.bp.set (1000.0f * v.tune * (0.6f + v.tone * 0.8f), 1.0f, sr); break;
            case P_CLAVE: v.freqs[0] = 2500.0f * v.tune * (0.7f + v.tone * 0.6f); break;
            case P_SHAKER: v.bp.set (6000.0f * (0.5f + v.tone) * dark, 0.8f, sr); v.hp.set (2000.0f, 0.7071f, sr); break;
            case P_FM: v.freqs[0] = 400.0f * v.tune; v.freqs[1] = v.freqs[0] * (1.2f + v.tone * 1.2f); break;
            case P_MODAL:
                for (int k = 0; k < 6; ++k)
                {
                    v.freqs[k] = 300.0f * v.tune * std::pow (modal[k], 0.7f + v.tone * 0.6f);
                    v.res[k].set (std::min (v.freqs[k], 0.45f * (float) sr), 60.0f + v.decay * 400.0f, sr);
                }
                break;
            case P_SAMPLE: break;
        }
    }

    float render (Voice& v, const SampleData* s)
    {
        const float t = (float) v.t;
        float y = 0.0f;
        switch (v.type)
        {
            case P_METAL:
            {
                float sq = 0.0f;
                for (int k = 0; k < 6; ++k) { v.ph[k] += v.freqs[k] / sr; if (v.ph[k] > 1.0) v.ph[k] -= 1.0; sq += v.ph[k] < 0.5 ? 1.0f : -1.0f; }
                const float src = sq / 6.0f * (1.0f - v.noise) + rng.next() * v.noise;
                y = v.hp.tickHP (v.bp.tickBP (src) * 2.0f) * std::exp (-t / v.decay) * std::min (1.0f, t / 0.0003f);
                break;
            }
            case P_CLAP:
            {
                float env = 0.0f;
                for (int b = 0; b < 3; ++b) { const float tb = t - b * 0.011f; if (tb >= 0) env = std::max (env, std::exp (-tb / 0.007f)); }
                env = std::max (env, t > 0.022f ? 0.6f * std::exp (-(t - 0.022f) / v.decay) : 0.0f);
                y = v.hp.tickHP (v.bp.tickBP (rng.next()) * 3.0f) * env;
                break;
            }
            case P_RIM:
                v.ph[0] += v.freqs[0] / sr; v.ph[1] += v.freqs[1] / sr;
                y = v.hp.tickHP (std::sin (kTwoPi * (float) v.ph[0]) * 0.6f + std::sin (kTwoPi * (float) v.ph[1]) * 0.5f) * std::exp (-t / v.decay);
                break;
            case P_BELL:
            {
                for (int k = 0; k < 2; ++k) { v.ph[k] += v.freqs[k] / sr; if (v.ph[k] > 1.0) v.ph[k] -= 1.0; }
                const float sq = (v.ph[0] < 0.5 ? 1.0f : -1.0f) + (v.ph[1] < 0.5 ? 1.0f : -1.0f);
                const float env = 0.6f * std::exp (-t / 0.012f) + 0.4f * std::exp (-t / v.decay);
                y = v.bp.tickBP (sq) * env * 0.6f;
                break;
            }
            case P_CLAVE: v.ph[0] += v.freqs[0] / sr; y = std::sin (kTwoPi * (float) v.ph[0]) * std::exp (-t / v.decay); break;
            case P_SHAKER: y = v.hp.tickHP (v.bp.tickBP (rng.next()) * 2.0f) * std::min (1.0f, t / 0.008f) * std::exp (-t / v.decay); break;
            case P_FM:
            {
                const float idx = 6.0f * std::exp (-t / (v.decay * 0.5f));
                v.ph[1] += v.freqs[1] / sr; if (v.ph[1] > 1.0) v.ph[1] -= 1.0;
                v.ph[0] += v.freqs[0] / sr; if (v.ph[0] > 1.0) v.ph[0] -= 1.0;
                y = std::sin (kTwoPi * (float) v.ph[0] + idx * std::sin (kTwoPi * (float) v.ph[1])) * std::exp (-t / v.decay);
                break;
            }
            case P_MODAL:
            {
                const float ex = t < 0.002f ? rng.next() * (1.0f - t / 0.002f) : 0.0f;
                for (int k = 0; k < 6; ++k) y += v.res[k].tickBP (ex) / (1.0f + k * 0.4f);
                y *= 0.6f;
                break;
            }
            case P_SAMPLE:
            {
                if (s == nullptr || v.spos + 1 >= s->length()) { v.active = false; return 0.0f; }
                const int i0 = (int) v.spos;
                const float fr = (float) (v.spos - i0);
                const float a = 0.5f * (s->buf.getSample (0, i0) + s->buf.getSample (1, i0));
                const float b = 0.5f * (s->buf.getSample (0, i0 + 1) + s->buf.getSample (1, i0 + 1));
                y = (a + fr * (b - a)) * std::exp (-t / std::max (0.01f, v.decay * 2.0f));
                v.spos += v.tune;
                break;
            }
        }
        v.t += 1.0 / sr;
        if (v.type != P_SAMPLE && t > v.decay * 8.0f + 0.03f) v.active = false;
        if (v.fadeStep > 0.0f) { v.fade -= v.fadeStep; if (v.fade <= 0.0f) { v.active = false; return 0.0f; } y *= v.fade; }
        return y * v.vel;
    }

    void process (Ctx& c, juce::AudioBuffer<float>& io, juce::AudioBuffer<float>*, juce::AudioBuffer<float>*, juce::MidiBuffer& midi) override
    {
        echoes.run (c, midi, events, c.v (DENS), echoSpaceBeats (c.ci (ESPACE)), c.v (ESWING), c.v (EPROB), false);
        const auto smp = sample.get();
        const int model = c.ci (MODEL);
        const float drv = c.v (DRIVE);
        size_t e = 0;
        auto* L = io.getWritePointer (0); auto* R = io.getWritePointer (1);
        for (int i = 0; i < c.n; ++i)
        {
            while (e < events.size() && events[e].offset <= i)
            {
                const auto& ev = events[e++];
                if (ev.on) trigger (c, model, ev);
            }
            float l = 0, r = 0;
            for (auto& v : voices)
                if (v.active)
                {
                    float y = render (v, smp.get());
                    if (drv > 0.001f) y = fastTanh (y * (1.0f + drv * 8.0f)) / (1.0f + drv * 2.0f);
                    l += y * std::sqrt (1.0f - v.pan); r += y * std::sqrt (v.pan);
                }
            L[i] = l * 1.41f; R[i] = r * 1.41f;
        }
        out.process (c, io, FILT, RES, 0.0f, 0.0f, c.b (ALLOCON), LEVEL);
    }
    int displayHeight() const override { return 0; }
private:
    double sr = 48000.0;
    std::array<Voice, 8> voices;
    EchoScheduler echoes;
    std::vector<NoteEvent> events;
    RoleOutput out;
    SampleSlot sample;
    Noise rng;
};
ModulePtr makePERC() { return std::make_unique<PercModule>(); }

// =====================================================================================  RUMBLE
// Kick-driven low-end bed. Listens to SF KICK on the bus (or MIDI), excites a thump through a
// model (reverb / delay / sub drone / distorted / dub / sample), then darkens, dirties and
// ducks it around the kick.
class RumbleModule : public Module
{
public:
    enum { SIZE, DARK, GRIT, PUMP, GATE, DENS, FILT, LEVEL, MODEL, TUNE, SOURCE, PREDELAY, WIDTH, PSHAPE, ESPACE, ALLOCON };
    std::string id() const override { return "RUMBLE"; }
    bool instrument() const override { return true; }
    int latency() const override { return Pocket::kLatency; }
    double tailSeconds() const override { return 10.0; }
    std::vector<ParamSpec> params() const override
    {
        return { pct ("size", "SIZE", 0.6f).prim().dir (1),
                 pct ("dark", "DARK", 0.6f).prim(),
                 pct ("grit", "GRIT", 0.3f).prim().dir (1),
                 pct ("pump", "PUMP", 0.7f).prim(),
                 pct ("gate", "GATE", 1.0f).prim(),
                 pct ("density", "DENSITY", 0.0f).prim().dir (1),
                 bip ("filter", "FILTER").prim(),
                 levelParam (-6.0f).prim(),
                 choice ("model", "MODEL", { "REVERB", "DELAY", "SUB DRONE", "DISTORTED", "DUB BOOM", "SAMPLE" }, 0),
                 knob ("tune", "TUNE", -12.0f, 12.0f, 0.0f, "st"),
                 choice ("source", "SOURCE", { "KICK BUS", "MIDI", "BOTH" }, 0),
                 choice ("predelay", "PREDELAY", { "0", "1/32", "1/16", "1/8" }, 1),
                 pct ("width", "WIDTH", 0.3f),
                 pct ("pocket_shape", "POCKET SHAPE", 0.5f),
                 choice ("echo_space", "ECHO SPACE", echoSpaceNames(), 0),
                 toggle ("alloc", "ALLOC", true) };
    }
    bool acceptsFiles() const override { return true; }
    juce::String fileHint() const override { return "DROP RUMBLE ONE-SHOT (MODEL: SAMPLE)"; }
    void loadFiles (const juce::StringArray& f) override { if (f.size() > 0) sample.set (loadSampleFile (juce::File (f[0]), sr)); }
    void save (juce::ValueTree& t) const override { sample.save (t, "SAMPLE"); }
    void load (const juce::ValueTree& t) override { sample.load (t, "SAMPLE", sr); }
    void prepare (double rate, int mb) override
    {
        if (auto s = sample.get(); s != nullptr && std::abs (s->sr - rate) > 1 && s->flac.getSize() > 0) sample.set (loadSampleFlac (s->flac, s->name, rate));
        sr = rate;
        out.prepare (rate, mb, ROLE_RUMBLE, "RUMBLE", true);
        fdn.prepare (rate);
        for (auto& d : dly) d.prepare ((int) (rate * 3));
        for (auto& d : pre) d.prepare ((int) (rate * 1));
        midiDelay.clear();
        lastTrigPpq = -1e9;
        for (auto& f : lp) for (auto& x : f) x.reset();
        for (auto& f : hp) f.reset();
        thumpT = 1e9; extras.clear();
    }
    void process (Ctx& c, juce::AudioBuffer<float>& io, juce::AudioBuffer<float>*, juce::AudioBuffer<float>*, juce::MidiBuffer& midi) override
    {
        const int model = c.ci (MODEL), src = c.ci (SOURCE);
        const double bps = c.beatsPerSample();
        const double look = Pocket::kLatency * bps;
        // Gather triggers (as musical times on the delayed timeline).
        KickHit hits[kKickRing];
        const int nh = (src != 1 && c.bus->kickAlive (4000)) ? c.bus->recentKicks (hits, kKickRing) : 0;
        // MIDI notes are rendered one pocket-latency later, so everything shares the delayed timeline.
        if (src != 0)
            for (const auto meta : midi)
                if (meta.getMessage().isNoteOn())
                    midiDelay.push_back ({ c.ppqAt (meta.samplePosition), meta.getMessage().getFloatVelocity() });
        fdn.set (c.v (SIZE), 0.6f + c.v (SIZE) * 5.0f, c.v (DARK), 0.15f, false);
        const float dlyTime = (float) (0.75 * c.samplesPerBeat());
        static const double preB[] = { 0.0, 0.125, 0.25, 0.5 };
        const float preS = (float) (preB[juce::jlimit (0, 3, c.ci (PREDELAY))] * c.samplesPerBeat());
        const float darkHz = expMap (1.0f - c.v (DARK), 70.0f, 600.0f);
        for (int k = 0; k < 2; ++k) { lp[k][0].set (darkHz, 0.5412f, sr); lp[k][1].set (darkHz, 1.3066f, sr); hp[k].set (26.0f, 0.7071f, sr); }
        const float grit = c.v (GRIT) + (model == 3 ? 0.6f : 0.0f);
        const float gateLen = c.v (GATE) >= 0.99f ? 1e9f : (float) ((0.1 + c.v (GATE) * 0.9) * c.samplesPerBeat());
        const float tune = semis (c.v (TUNE));
        const float width = c.v (WIDTH);
        const float dens = c.v (DENS);
        const double space = echoSpaceBeats (c.ci (ESPACE));
        const auto smp = sample.get();
        auto* L = io.getWritePointer (0); auto* R = io.getWritePointer (1);
        for (int i = 0; i < c.n; ++i)
        {
            const double tq = c.ppqAt (i) - look;
            // Kick-bus triggers whose time has come on the delayed timeline.
            for (int h = 0; h < nh; ++h)
                if (hits[h].ppq > lastTrigPpq + 1e-6 && hits[h].ppq <= tq && tq - hits[h].ppq < 0.25)
                {
                    lastTrigPpq = hits[h].ppq;
                    fire (hits[h].f0, hits[h].f1, hits[h].tau, hits[h].vel, tune, tq, dens, space);
                }
            for (size_t k = 0; k < midiDelay.size();)
                if (midiDelay[k].first <= tq) { fire (200.0f, 50.0f, 0.012f, midiDelay[k].second, tune, tq, dens, space); midiDelay.erase (midiDelay.begin() + (long) k); }
                else ++k;
            for (size_t k = 0; k < extras.size();)
                if (extras[k].first <= tq) { fire (lastF0, lastF1, lastTau, extras[k].second, tune, -1.0, 0.0f, space); extras.erase (extras.begin() + (long) k); }
                else ++k;

            // Exciter.
            float x = 0.0f;
            if (thumpT < 0.4f)
            {
                if (model == 5 && smp != nullptr)
                {
                    const int idx = (int) sPos;
                    if (idx + 1 < smp->length()) x = 0.5f * (smp->buf.getSample (0, idx) + smp->buf.getSample (1, idx)) * thumpVel;
                    sPos += tune;
                }
                else
                {
                    const float f = thF1 + (thF0 - thF1) * std::exp (-thumpT / thTau);
                    thPh += f / (float) sr; if (thPh > 1.0f) thPh -= 1.0f;
                    x = std::sin (kTwoPi * thPh) * std::exp (-thumpT / 0.12f) * thumpVel;
                }
                thumpT += 1.0f / (float) sr;
            }
            ++sinceTrig;
            float wl = 0, wr = 0;
            pre[0].push (x);
            const float xp = preS > 1 ? pre[0].read (preS) : x;
            switch (model)
            {
                case 0: case 3: fdn.tick (xp, xp, wl, wr, false); wl *= 2.0f; wr *= 2.0f; break;
                case 1:
                {
                    const float yl = dly[0].read (dlyTime), yr = dly[1].read (dlyTime * 1.02f);
                    dly[0].push (xp + yr * (0.3f + c.v (SIZE) * 0.55f)); dly[1].push (yl * (0.3f + c.v (SIZE) * 0.55f));
                    wl = yl + xp * 0.5f; wr = yr + xp * 0.5f;
                    break;
                }
                case 2:
                {
                    dronePh += thF1 / (float) sr; if (dronePh > 1.0f) dronePh -= 1.0f;
                    const float d = std::sin (kTwoPi * dronePh) * 0.5f * (0.6f + 0.4f * std::sin (kTwoPi * (float) std::fmod (c.ppqAt (i) * 0.25, 1.0)));
                    wl = wr = d * droneLevel;
                    droneLevel = std::min (1.0f, droneLevel + 1.0f / (float) sr);
                    break;
                }
                case 4:
                {
                    float rl, rr; fdn.tick (xp * 0.5f, xp * 0.5f, rl, rr, false);
                    const float yl = dly[0].read (dlyTime), yr = dly[1].read (dlyTime);
                    dly[0].push (xp + yl * 0.5f); dly[1].push (xp + yr * 0.5f);
                    wl = rl + yl; wr = rr + yr;
                    break;
                }
                default: wl = wr = xp; break;
            }
            if (grit > 0.001f) { const float g = 1.0f + grit * 8.0f; wl = fastTanh (wl * g) / std::sqrt (g); wr = fastTanh (wr * g) / std::sqrt (g); }
            wl = hp[0].tickHP (lp[0][1].tickLP (lp[0][0].tickLP (wl)));
            wr = hp[1].tickHP (lp[1][1].tickLP (lp[1][0].tickLP (wr)));
            const float gt = sinceTrig < gateLen ? 1.0f : 0.0f;
            gateZ = gt + 0.998f * (gateZ - gt);
            const float m = 0.5f * (wl + wr), s = 0.5f * (wl - wr) * width;
            L[i] = (m + s) * gateZ;
            R[i] = (m - s) * gateZ;
        }
        out.process (c, io, FILT, -1, c.v (PUMP), c.v (PSHAPE), c.b (ALLOCON), LEVEL, false);
    }
    int displayHeight() const override { return 80; }
    void paint (juce::Graphics& g, juce::Rectangle<float> r) override { out.pocket.paint (g, r); }
private:
    void fire (float f0, float f1, float tau, float vel, float tune, double tq, float dens, double space)
    {
        thF0 = f0 * tune; thF1 = std::max (25.0f, f1 * tune); thTau = std::max (0.002f, tau);
        thumpT = 0.0f; thumpVel = vel; thPh = 0.0f; sPos = 0.0; sinceTrig = 0;
        lastF0 = f0; lastF1 = f1; lastTau = tau;
        if (tq > 0.0 && dens > 0.001f)
        {
            const float t1 = clamp01 (dens * 2.0f), t2 = clamp01 ((dens - 0.5f) * 2.0f);
            if (t1 > 0.01f) extras.push_back ({ tq + space, vel * 0.6f * t1 });
            if (t2 > 0.01f) { extras.push_back ({ tq + space * 0.5, vel * 0.4f * t2 }); extras.push_back ({ tq + space * 1.5, vel * 0.35f * t2 }); }
        }
    }
    double sr = 48000.0;
    RoleOutput out;
    Fdn fdn;
    DelayLine dly[2], pre[1];
    Svf lp[2][2], hp[2];
    SampleSlot sample;
    std::vector<std::pair<double, float>> midiDelay, extras;
    double lastTrigPpq = -1e9, sPos = 0;
    float thumpT = 1e9f, thumpVel = 1, thF0 = 200, thF1 = 50, thTau = 0.01f, thPh = 0, dronePh = 0, droneLevel = 0, gateZ = 1, lastF0 = 200, lastF1 = 50, lastTau = 0.01f;
    float sinceTrig = 1e9f;
};
ModulePtr makeRUMBLE() { return std::make_unique<RumbleModule>(); }
} // namespace sf
