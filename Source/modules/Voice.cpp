// VOICE: spoken-word / vocal instrument. A loaded phrase is sliced (C3 = slice 1, C#3 = slice 2 ...,
// keys below C3 play the whole phrase). Models process it: SAMPLE, RADIO, VOCODER, ROBOT, CHOIR,
// STUTTER. Built-in dub echo send. With no phrase loaded it plays a formant choir from the notes.
#include "Kit.h"

namespace sf
{
class VoiceModule : public Module
{
public:
    enum { SIGNAL, TONE, GATE, DENS, FILT, ECHO, DRIVE, LEVEL, MODEL, SLICEMODE, TUNE, ECHOTIME, ECHOFB, PDEPTH, PSHAPE, ESPACE, ALLOCON };
    std::string id() const override { return "VOICE"; }
    bool instrument() const override { return true; }
    int latency() const override { return Pocket::kLatency; }
    double tailSeconds() const override { return 10.0; }
    std::vector<ParamSpec> params() const override
    {
        return { pct ("signal", "SIGNAL", 0.6f).prim().dir (1),
                 pct ("tone", "TONE", 0.5f).prim(),
                 pct ("gate", "GATE", 0.6f).prim(),
                 pct ("density", "DENSITY", 0.0f).prim().dir (1),
                 bip ("filter", "FILTER").prim(),
                 pct ("echo", "ECHO", 0.2f).prim().dir (1),
                 pct ("drive", "DRIVE", 0.1f).prim(),
                 levelParam (-6.0f).prim(),
                 choice ("model", "MODEL", { "SAMPLE", "RADIO", "VOCODER", "ROBOT", "CHOIR", "STUTTER" }, 1),
                 choice ("slice_mode", "SLICE MODE", { "AUTO", "EQUAL 8", "EQUAL 16", "WHOLE" }, 0),
                 knob ("tune", "TUNE", -12.0f, 12.0f, 0.0f, "st"),
                 choice ("echo_time", "ECHO TIME", divisionNames (divIndex ("1 BAR"), divIndex ("1/16")), divIndex ("1/4D") - divIndex ("1 BAR")),
                 pct ("echo_fb", "ECHO FDBK", 0.55f),
                 pct ("pocket_depth", "POCKET DEPTH", 0.2f),
                 pct ("pocket_shape", "POCKET SHAPE", 0.3f),
                 choice ("echo_space", "ECHO SPACE", echoSpaceNames(), 1),
                 toggle ("alloc", "ALLOC", true) };
    }
    bool acceptsFiles() const override { return true; }
    juce::String fileHint() const override { return "DROP PHRASE (C3 = SLICE 1)"; }
    void loadFiles (const juce::StringArray& f) override { if (f.size() > 0) sample.set (loadSampleFile (juce::File (f[0]), sr)); }
    void save (juce::ValueTree& t) const override { sample.save (t, "SAMPLE"); }
    void load (const juce::ValueTree& t) override { sample.load (t, "SAMPLE", sr); }

    void prepare (double rate, int mb) override
    {
        if (auto s = sample.get(); s != nullptr && std::abs (s->sr - rate) > 1 && s->flac.getSize() > 0) sample.set (loadSampleFlac (s->flac, s->name, rate));
        sr = rate;
        out.prepare (rate, mb, ROLE_VOICE, "VOICE", true);
        echoes.reset();
        for (auto& d : echoLine) d.prepare ((int) (rate * 9));
        for (int b = 0; b < kVb; ++b)
        {
            const float f = expMap ((float) b / (float) (kVb - 1), 120.0f, 7000.0f);
            ana[b].set (f, 6.0f, rate); car[b].set (f, 6.0f, rate);
            fol[b].set (0.002f, 0.03f, rate);
        }
        play = {};
        for (auto& v : choir) v.active = false;
        for (auto& f : echoLp) f.reset();
    }

    void process (Ctx& c, juce::AudioBuffer<float>& io, juce::AudioBuffer<float>*, juce::AudioBuffer<float>*, juce::MidiBuffer& midi) override
    {
        echoes.run (c, midi, events, c.v (DENS), echoSpaceBeats (c.ci (ESPACE)), 0.0f, 1.0f, true);
        const auto smp = sample.get();
        const int model = c.ci (MODEL);
        int autoOff; float relMul;
        gateShape (c.v (GATE), c.samplesPerBeat(), autoOff, relMul);
        const float tone = c.v (TONE), sig = c.v (SIGNAL), drive = c.v (DRIVE);
        const float rate = semis (c.v (TUNE));
        // Carrier pitch for VOCODER / ROBOT / CHOIR: C1..C4 from TONE.
        const float carrierHz = noteHz (36.0f + std::round (tone * 36.0f));
        for (int k = 0; k < 2; ++k) { radio[k].set (300.0f * (1.0f - sig * 0.6f), 0.7f, sr); radioHi[k].set (2200.0f + sig * 9000.0f, 0.7f, sr); }
        const double echoBeats = divisionBeats (c.ci (ECHOTIME), divIndex ("1 BAR"));
        const float echoD = (float) std::min (echoBeats * c.samplesPerBeat(), sr * 8.5);
        const float echoAmt = c.v (ECHO), echoFb = c.v (ECHOFB) * 0.95f;
        for (auto& f : echoLp) f.set (2800.0f, 0.7f, sr);
        size_t e = 0;
        auto* L = io.getWritePointer (0); auto* R = io.getWritePointer (1);
        for (int i = 0; i < c.n; ++i)
        {
            while (e < events.size() && events[e].offset <= i)
            {
                const auto& ev = events[e++];
                if (ev.on) noteOn (ev, smp.get(), c.ci (SLICEMODE), autoOff, relMul);
                else if (ev.gen == 0 || ev.note < 0) noteOff (ev.note);
            }
            float x = 0.0f;
            if (smp != nullptr)
            {
                if (play.active)
                {
                    const int ip = (int) play.pos;
                    if (ip + 1 >= play.end || ip + 1 >= smp->length()) play.active = false;
                    else
                    {
                        const float fr = (float) (play.pos - ip);
                        const float a = 0.5f * (smp->buf.getSample (0, ip) + smp->buf.getSample (1, ip));
                        const float b = 0.5f * (smp->buf.getSample (0, ip + 1) + smp->buf.getSample (1, ip + 1));
                        float env = std::min (1.0f, (float) (play.pos - play.start) / (0.003f * (float) sr) + 0.001f);
                        if (play.releasing) { play.rel *= play.relCoef; env *= play.rel; if (play.rel < 1e-4f) play.active = false; }
                        if (play.autoOff > 0 && --play.autoOff == 0) startRelease (relMul);
                        x = (a + fr * (b - a)) * env * play.vel;
                        play.pos += rate;
                        if (model == 5)
                        {
                            // STUTTER: loop the first part of the slice; TONE sets the loop length.
                            const double loop = (0.5 - tone * 0.45) * c.samplesPerBeat() * 0.5;
                            if (play.pos - play.start >= loop) play.pos = play.start;
                        }
                    }
                }
                switch (model)
                {
                    case 1: x = radioChain (x, sig); break;
                    case 2: case 3: case 4: x = vocode (x, model, carrierHz, sig); break;
                    default: break;
                }
            }
            else
                x = choirSynth (tone);
            if (drive > 0.001f) { const float g = 1.0f + drive * 8.0f; x = fastTanh (x * g) / std::sqrt (g); }
            // Dub echo send.
            const float yl = echoLine[0].read (echoD), yr = echoLine[1].read (echoD * 1.5f > sr * 8.5f ? echoD : echoD * 1.5f);
            echoLine[0].push (x * echoAmt + echoLp[0].tickLP (fastTanh (yr * echoFb)));
            echoLine[1].push (echoLp[1].tickLP (fastTanh (yl * echoFb)));
            L[i] = x + yl * 0.9f;
            R[i] = x + yr * 0.9f;
        }
        out.process (c, io, FILT, -1, c.v (PDEPTH), c.v (PSHAPE), c.b (ALLOCON), LEVEL);
        dispSlice = play.active ? play.slice : -1;
    }

    int displayHeight() const override { return 110; }
    void paint (juce::Graphics& g, juce::Rectangle<float> r) override
    {
        ui::frame (g, r, "PHRASE");
        auto in = r.reduced (8, 18);
        auto s = sample.get();
        if (s == nullptr) { ui::label (g, "NO PHRASE: FORMANT CHOIR PLAYS THE NOTES", in, ui::kFaint, 11.0f, juce::Justification::centred); return; }
        const int n = s->length();
        g.setColour (ui::kDim);
        const int w = (int) in.getWidth();
        for (int x = 0; x < w; ++x)
        {
            const int a = (int) ((double) x / w * n), b = std::min (n, (int) ((double) (x + 1) / w * n));
            float pk = 0; for (int k = a; k < b; k += 8) pk = std::max (pk, std::fabs (s->buf.getSample (0, k)));
            const float h = pk / s->peak * in.getHeight() * 0.5f;
            g.drawVerticalLine ((int) in.getX() + x, in.getCentreY() - h, in.getCentreY() + h);
        }
        const auto sl = slicesFor (*s, sliceModeDisp);
        for (size_t k = 0; k < sl.size(); ++k)
        {
            const float x = in.getX() + (float) sl[k] / (float) n * in.getWidth();
            g.setColour ((int) k == dispSlice.load() ? ui::kGreen : ui::kText);
            g.drawVerticalLine ((int) x, in.getY(), in.getBottom());
            ui::label (g, juce::MidiMessage::getMidiNoteName (60 + (int) k, true, true, 3), { x + 2, in.getY(), 30, 12 }, ui::kFaint, 9.0f);
        }
        ui::label (g, s->name.toUpperCase(), r.reduced (8, 2).removeFromBottom (14), ui::kFaint, 10.0f, juce::Justification::centredRight);
    }

private:
    static constexpr int kVb = 16;
    struct Play { bool active = false, releasing = false; double pos = 0, start = 0; int end = 0, slice = -1, autoOff = -1; float vel = 1, rel = 1, relCoef = 0.999f; };
    struct ChoirVoice { bool active = false; int note = 0; float f = 220, a = 0, target = 0; double ph = 0; Svf fm[3]; };

    std::vector<int> slicesFor (const SampleData& s, int mode) const
    {
        std::vector<int> sl;
        if (mode == 0) sl = s.slices;
        else if (mode == 1 || mode == 2)
        {
            const int k = mode == 1 ? 8 : 16;
            for (int i = 0; i < k; ++i) sl.push_back ((int) ((long long) s.length() * i / k));
        }
        if (sl.empty()) sl.push_back (0);
        return sl;
    }

    void noteOn (const NoteEvent& ev, const SampleData* s, int sliceMode, int autoOff, float relMul)
    {
        sliceModeDisp = sliceMode;
        if (s == nullptr)
        {
            ChoirVoice* v = nullptr;
            for (auto& x : choir) if (! x.active) { v = &x; break; }
            if (v == nullptr) v = &choir[0];
            v->active = true; v->note = ev.note; v->f = noteHz ((float) ev.note); v->target = ev.vel; v->ph = 0;
            return;
        }
        const auto sl = slicesFor (*s, sliceMode);
        const int idx = ev.note - 60;
        play.slice = (idx >= 0 && sliceMode != 3) ? std::min (idx, (int) sl.size() - 1) : -1;
        play.start = play.slice >= 0 ? sl[(size_t) play.slice] : 0;
        play.end = play.slice >= 0 && play.slice + 1 < (int) sl.size() ? sl[(size_t) play.slice + 1] : s->length();
        play.pos = play.start;
        play.active = true; play.releasing = false; play.rel = 1.0f;
        play.vel = 0.5f + 0.5f * ev.vel;
        play.autoOff = autoOff;
        play.relCoef = std::exp (-1.0f / (0.02f * relMul * (float) sr));
    }
    void startRelease (float relMul) { play.releasing = true; play.relCoef = std::exp (-1.0f / (0.02f * relMul * (float) sr)); }
    void noteOff (int note)
    {
        for (auto& v : choir) if (v.active && (note < 0 || v.note == note)) v.target = 0.0f;
        juce::ignoreUnused (note);
    }

    float radioChain (float x, float sig)
    {
        x = radioHi[0].tickLP (radio[0].tickHP (x));
        x = fastTanh (x * (2.5f - sig * 1.5f)) * 0.8f;
        return x + rng.next() * 0.01f * (1.0f - sig);
    }

    float vocode (float x, int model, float carrierHz, float sig)
    {
        // Carrier
        float c = 0.0f;
        cph += carrierHz / (float) sr; if (cph > 1.0f) cph -= 1.0f;
        if (model == 3) c = cph < 0.04f ? 1.0f : -0.04f;                                          // ROBOT: pulse train
        else if (model == 4)
        {
            cph2 += carrierHz * 1.1892f / (float) sr; if (cph2 > 1.0f) cph2 -= 1.0f;               // CHOIR: minor third + fifth
            cph3 += carrierHz * 1.4983f / (float) sr; if (cph3 > 1.0f) cph3 -= 1.0f;
            c = (2.0f * cph - 1.0f) + (2.0f * cph2 - 1.0f) * 0.8f + (2.0f * cph3 - 1.0f) * 0.7f;
            c *= 0.5f;
        }
        else c = 2.0f * cph - 1.0f;                                                                 // VOCODER: saw
        c += rng.next() * 0.15f * sig;
        float y = 0.0f;
        for (int b = 0; b < kVb; ++b)
        {
            const float env = fol[b].tick (ana[b].tickBP (x));
            y += car[b].tickBP (c) * env * (1.0f + (float) b / kVb * sig * 2.0f);
        }
        y *= 2.5f;
        if (model == 3)
        {
            // ROBOT: 8 kHz / 8-bit
            if (++holdN >= std::max (1, (int) (sr / 8000.0))) { holdN = 0; heldY = std::round (y * 128.0f) / 128.0f; }
            y = heldY;
        }
        return y;
    }

    float choirSynth (float tone)
    {
        // Formants for A E I O U, morphed by TONE.
        static const float F[5][3] = { { 800, 1150, 2900 }, { 400, 1600, 2700 }, { 270, 2140, 2950 }, { 450, 800, 2830 }, { 325, 700, 2530 } };
        const float pos = tone * 4.0f;
        const int a = std::min (3, (int) pos);
        const float fr = pos - (float) a;
        float y = 0.0f;
        for (auto& v : choir)
        {
            if (! v.active) continue;
            v.a += (v.target - v.a) * (v.target > v.a ? 0.0005f : 0.0002f);
            if (v.target <= 0.0f && v.a < 1e-4f) { v.active = false; continue; }
            v.ph += v.f * (1.0 + 0.003 * std::sin (v.ph * 0.0001)) / sr; if (v.ph > 1.0) v.ph -= 1.0;
            const float src = (float) (2.0 * v.ph - 1.0) + rng.next() * 0.05f;
            float out = 0.0f;
            for (int k = 0; k < 3; ++k)
            {
                v.fm[k].set (lerp (F[a][k], F[a + 1][k], fr), 8.0f, sr);
                out += v.fm[k].tickBP (src) * (k == 0 ? 1.0f : 0.6f);
            }
            y += out * v.a * 0.25f;
        }
        return y;
    }

    double sr = 48000.0;
    RoleOutput out;
    EchoScheduler echoes;
    std::vector<NoteEvent> events;
    SampleSlot sample;
    Play play;
    std::array<ChoirVoice, 6> choir;
    Svf ana[kVb], car[kVb], radio[2], radioHi[2], echoLp[2];
    Follower fol[kVb];
    DelayLine echoLine[2];
    Noise rng;
    float cph = 0, cph2 = 0, cph3 = 0, heldY = 0;
    int holdN = 0, sliceModeDisp = 0;
    std::atomic<int> dispSlice { -1 };
};
ModulePtr makeVOICE() { return std::make_unique<VoiceModule>(); }
} // namespace sf
