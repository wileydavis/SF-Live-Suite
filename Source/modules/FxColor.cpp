// DRIVE, RADIO, DUCK, ENERGY
#include "Kit.h"
#include "../core/Oversample.h"

namespace sf
{
// =====================================================================================  DRIVE
// One CHARACTER knob from smooth to fuzz: TAPE -> TUBE -> FUZZ -> INDUSTRIAL. 4x oversampled.
class DriveModule : public Module
{
public:
    enum { DRIVE, CHAR, TONE, LOWKEEP, MIX, LEVEL, AUTO };
    std::string id() const override { return "DRIVE"; }
    std::vector<ParamSpec> params() const override
    {
        return { pct ("drive", "DRIVE", 0.2f).prim().dir (1),
                 pct ("character", "CHARACTER", 0.0f).prim().dir (1),
                 knob ("tone", "TONE", -1.0f, 1.0f, 0.0f, "%").prim(),
                 knob ("low_keep", "LOW KEEP", 0.0f, 300.0f, 0.0f, "Hz").prim(),
                 pct ("mix", "MIX", 1.0f).prim(),
                 levelParam().prim(),
                 toggle ("auto_gain", "AUTO GAIN", true).prim() };
    }
    int latency() const override { return os ? (int) std::lround (os->getLatencyInSamples()) : 0; }
    void prepare (double rate, int mb) override
    {
        sr = rate;
        os = std::make_unique<juce::dsp::Oversampling<float>> (2, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false);
        os->initProcessing ((size_t) mb);
        dryDelay.setSize (2, mb);
        for (auto& d : dryLine) d.prepare (64);
        inF.set (0.05f, 0.4f, rate); outF.set (0.05f, 0.4f, rate);
        reset();
    }
    void reset() override
    {
        if (os) os->reset();
        for (auto& d : dryLine) d.clear();
        for (auto& x : xo) x.reset();
        for (auto& x : xoDry) x.reset();
        for (auto& t : tilt) for (auto& b : t) b.reset();
        inF.z = outF.z = 0.0f;
        autoG = autoGPrev = 1.0f;
    }
    static float shape (float x, float ch, float bias)
    {
        // Four stages blended by CHARACTER.
        const float tape = fastTanh (x);
        const float tube = x >= 0 ? fastTanh (x * 1.2f) : fastTanh (x * 0.7f) * 1.1f;
        const float fuzz = hardClip ((x + bias) * 2.5f, 0.8f) - hardClip (bias * 2.5f, 0.8f);
        const float fold = std::sin (x * 1.7f);
        const float pos = ch * 3.0f;
        if (pos < 1.0f) return lerp (tape, tube, pos);
        if (pos < 2.0f) return lerp (tube, fuzz, pos - 1.0f);
        return lerp (fuzz, fold * 0.9f, pos - 2.0f);
    }
    void process (Ctx& c, juce::AudioBuffer<float>& io, juce::AudioBuffer<float>*, juce::AudioBuffer<float>*, juce::MidiBuffer&) override
    {
        const int n = c.n;
        const float drive = c.v (DRIVE), ch = c.v (CHAR), mix = c.v (MIX);
        const float pre = dbToGain (drive * 36.0f);
        const float lowKeep = c.v (LOWKEEP);
        // Keep a dry copy (delayed by the oversampler latency) for MIX and LOW KEEP.
        dryDelay.setSize (2, n, false, false, true);
        const int lat = latency();
        float inLevel = 0.0f;
        for (int k = 0; k < 2; ++k)
            for (int i = 0; i < n; ++i)
            {
                const float x = io.getSample (k, i);
                inLevel = std::max (inLevel, inF.tick (x));
                dryLine[k].push (x);
                dryDelay.setSample (k, i, dryLine[k].tap (lat));
            }
        juce::dsp::AudioBlock<float> block (io);
        auto up = os->processSamplesUp (block);
        const float bias = ch > 0.5f ? 0.15f * (ch - 0.5f) * 2.0f : 0.0f;
        for (size_t k = 0; k < up.getNumChannels(); ++k)
        {
            auto* x = up.getChannelPointer (k);
            for (size_t i = 0; i < up.getNumSamples(); ++i) x[i] = shape (x[i] * pre, ch, bias);
        }
        os->processSamplesDown (block);
        // Tone tilt around 800 Hz.
        const float t = c.v (TONE);
        for (int k = 0; k < 2; ++k)
        {
            tilt[k][0].lowShelf (800.0f, 0.6f, -t * 6.0f, sr);
            tilt[k][1].highShelf (800.0f, 0.6f, t * 6.0f, sr);
        }
        float outLevel = 0.0f;
        if (lowKeep > 20.0f) { for (auto& x : xo) x.set (lowKeep, sr); for (auto& x : xoDry) x.set (lowKeep, sr); }
        for (int k = 0; k < 2; ++k)
        {
            auto* x = io.getWritePointer (k);
            const auto* d = dryDelay.getReadPointer (k);
            for (int i = 0; i < n; ++i)
            {
                float y = tilt[k][1].tick (tilt[k][0].tick (x[i]));
                if (lowKeep > 20.0f)
                {
                    float lo, hi, dlo, dhi;
                    xo[k].split (y, lo, hi);
                    xoDry[k].split (d[i], dlo, dhi);
                    y = hi + dlo;
                }
                outLevel = std::max (outLevel, outF.tick (y));
                x[i] = d[i] * (1.0f - mix) + y * mix;
            }
        }
        // Auto gain: hold output loudness near input loudness.
        float target = 1.0f;
        if (c.b (AUTO) && outLevel > 1e-4f && inLevel > 1e-4f) target = juce::jlimit (0.1f, 2.0f, inLevel / outLevel);
        autoG += 0.05f * (target - autoG);
        float g0; const float g1 = levelDb (c, LEVEL, g0);
        applyGain (io, g0 * autoGPrev, g1 * autoG);
        autoGPrev = autoG;
        dispCh = ch;
    }
    int displayHeight() const override { return 60; }
    void paint (juce::Graphics& g, juce::Rectangle<float> r) override
    {
        ui::frame (g, r, "CHARACTER");
        drawPosition (g, r.reduced (12, 20), dispCh.load(), "TAPE", "INDUSTRIAL", ui::kText);
        ui::label (g, "TUBE", r.reduced (12, 20).withTrimmedLeft (r.getWidth() * 0.30f).withHeight (14), ui::kFaint, 10.0f);
        ui::label (g, "FUZZ", r.reduced (12, 20).withTrimmedLeft (r.getWidth() * 0.62f).withHeight (14), ui::kFaint, 10.0f);
    }
private:
    double sr = 48000.0;
    std::unique_ptr<juce::dsp::Oversampling<float>> os;
    juce::AudioBuffer<float> dryDelay;
    DelayLine dryLine[2];
    Biquad tilt[2][2];
    Crossover xo[2], xoDry[2];
    Follower inF, outF;
    float autoG = 1.0f, autoGPrev = 1.0f;
    std::atomic<float> dispCh { 0 };
};
ModulePtr makeDRIVE() { return std::make_unique<DriveModule>(); }

// =====================================================================================  RADIO
// Transmission processor. SIGNAL 0 = barely there, 1 = clear.
class RadioModule : public Module
{
public:
    enum { SIGNAL, BAND, WIDTH, NOISE, DROPOUT, SQUELCH, RING, MIX, CRUSH, RATE };
    std::string id() const override { return "RADIO"; }
    std::vector<ParamSpec> params() const override
    {
        return { pct ("signal", "SIGNAL", 0.5f).prim().dir (1),
                 knob ("band", "BAND", 300.0f, 3000.0f, 1200.0f, "Hz", 1000.0f).prim(),
                 pct ("width", "WIDTH", 0.5f).prim(),
                 pct ("noise", "NOISE", 0.3f).prim(),
                 pct ("dropout", "DROPOUT", 0.2f).prim(),
                 pct ("squelch", "SQUELCH", 0.4f).prim(),
                 pct ("ring", "RING", 0.0f).prim(),
                 pct ("mix", "MIX", 1.0f).prim(),
                 knob ("crush", "CRUSH", 4.0f, 16.0f, 16.0f, "bit"),
                 knob ("rate", "RATE", 2000.0f, 48000.0f, 48000.0f, "Hz", 12000.0f) };
    }
    void prepare (double rate, int) override { sr = rate; reset(); }
    void reset() override { for (auto& b : bp) for (auto& f : b) f.reset(); sqf.reset(); dropLeft = 0; burst = 0; gz = 1.0f; held[0] = held[1] = 0.0f; }
    void process (Ctx& c, juce::AudioBuffer<float>& io, juce::AudioBuffer<float>*, juce::AudioBuffer<float>*, juce::MidiBuffer&) override
    {
        const float sig = c.v (SIGNAL), mix = c.v (MIX);
        const float widthOct = 0.6f + c.v (WIDTH) * 3.0f * (0.4f + 0.6f * sig);
        const float lo = c.v (BAND) / std::pow (2.0f, widthOct * 0.5f), hi = c.v (BAND) * std::pow (2.0f, widthOct * 0.5f);
        for (int k = 0; k < 2; ++k) { bp[k][0].set (lo, 0.8f, sr); bp[k][1].set (lo, 0.8f, sr); bp[k][2].set (std::min (hi, 18000.0f), 0.8f, sr); bp[k][3].set (std::min (hi, 18000.0f), 0.8f, sr); }
        sqf.set (2200.0f, 3.0f, sr);
        const float noiseAmt = c.v (NOISE) * (1.0f - sig * 0.75f) * 0.12f;
        const float dropProb = c.v (DROPOUT) * (1.0f - sig) * 6.0f / (float) sr;   // events per sample
        const float ring = c.v (RING), sq = c.v (SQUELCH);
        const float bits = std::pow (2.0f, c.v (CRUSH) - 1.0f);
        const float hold = (float) (sr / c.v (RATE));
        for (int i = 0; i < c.n; ++i)
        {
            if (dropLeft <= 0 && rng.uni() < dropProb) { dropLeft = (int) ((0.02 + rng.uni() * 0.13) * sr); }
            float gate = 1.0f;
            if (dropLeft > 0) { gate = 0.0f; if (--dropLeft == 0) burst = (int) (0.04 * sr); }
            gz += 0.01f * (gate - gz);
            ringPh += 47.0f / (float) sr; if (ringPh > 1.0f) ringPh -= 1.0f;
            const float rm = lerp (1.0f, std::sin (kTwoPi * ringPh), ring);
            float sqBurst = 0.0f;
            if (burst > 0) { sqBurst = sqf.tickBP (rng.next()) * sq * 0.6f * (float) burst / (float) (0.04 * sr); --burst; }
            if (++holdCount >= hold) { holdCount -= hold; for (int k = 0; k < 2; ++k) held[k] = io.getSample (k, i); }
            for (int k = 0; k < 2; ++k)
            {
                const float dry = io.getSample (k, i);
                float x = held[k];
                x = bp[k][1].tickHP (bp[k][0].tickHP (x));
                x = bp[k][3].tickLP (bp[k][2].tickLP (x));
                x = fastTanh (x * (2.0f - sig)) * rm;
                if (bits < 32000.0f) x = std::round (x * bits) / bits;
                x = x * gz + rng.next() * noiseAmt + sqBurst;
                io.setSample (k, i, dry * (1.0f - mix) + x * mix);
            }
        }
        dispDrop = gz;
    }
    int displayHeight() const override { return 50; }
    void paint (juce::Graphics& g, juce::Rectangle<float> r) override
    {
        ui::frame (g, r, "CARRIER");
        ui::label (g, dispDrop.load() > 0.5f ? "LOCK" : "DROP", r.reduced (10, 14), dispDrop.load() > 0.5f ? ui::kGreen : ui::kAmber, 14.0f, juce::Justification::centred);
    }
private:
    double sr = 48000.0;
    Svf bp[2][4], sqf;
    Noise rng;
    int dropLeft = 0, burst = 0;
    float gz = 1.0f, ringPh = 0.0f, holdCount = 0.0f, held[2] {};
    std::atomic<float> dispDrop { 1.0f };
};
ModulePtr makeRADIO() { return std::make_unique<RadioModule>(); }

// =====================================================================================  DUCK
class DuckModule : public Module
{
public:
    enum { DEPTH, SHAPE, SOURCE, RATE, CURVE, MIX };
    std::string id() const override { return "DUCK"; }
    bool sidechain() const override { return true; }
    std::vector<ParamSpec> params() const override
    {
        return { pct ("depth", "DEPTH", 0.6f).prim().dir (1),
                 pct ("shape", "SHAPE", 0.4f).prim().dir (1),
                 choice ("source", "SOURCE", { "KICK BUS", "SIDECHAIN", "GRID" }, 0).prim(),
                 choice ("rate", "RATE", { "1/2", "1/4", "1/8" }, 1).prim(),
                 pct ("curve", "CURVE", 0.5f).prim(),
                 pct ("mix", "MIX", 1.0f).prim() };
    }
    int latency() const override { return Pocket::kLatency; }
    void prepare (double rate, int mb) override { sr = rate; pocket.prepare (rate, mb); gz = 1.0f; }
    void reset() override { pocket.reset(); gz = 1.0f; }
    void process (Ctx& c, juce::AudioBuffer<float>& io, juce::AudioBuffer<float>* side, juce::AudioBuffer<float>*, juce::MidiBuffer&) override
    {
        const int src = c.ci (SOURCE);
        const float depth = c.v (DEPTH) * c.v (MIX);
        pocket.process (c, io.getArrayOfWritePointers(), 2, c.n, depth, c.v (SHAPE), side, src != 2, src == 1);
        if (src == 2)
        {
            static const double rB[] = { 2.0, 1.0, 0.5 };
            const double per = rB[juce::jlimit (0, 2, c.ci (RATE))];
            const float pw = 0.3f + c.v (CURVE) * 3.0f;
            const double look = Pocket::kLatency * c.beatsPerSample();
            for (int i = 0; i < c.n; ++i)
            {
                double ph = std::fmod (c.ppqAt (i) - look, per) / per;
                if (ph < 0) ph += 1.0;
                const float shapeV = std::pow ((float) ph, pw);
                const float g = 1.0f - depth * (1.0f - shapeV);
                gz = g + 0.9f * (gz - g);
                io.getWritePointer (0)[i] *= gz;
                io.getWritePointer (1)[i] *= gz;
            }
        }
    }
    int displayHeight() const override { return 80; }
    void paint (juce::Graphics& g, juce::Rectangle<float> r) override { pocket.paint (g, r); }
private:
    double sr = 48000.0;
    Pocket pocket;
    float gz = 1.0f;
};
ModulePtr makeDUCK() { return std::make_unique<DuckModule>(); }

// =====================================================================================  ENERGY
// Group energy: one bipolar knob, centre = neutral. Down: low-pass + space + softer.
// Up: high-pass with resonant edge + drive + 1/16 echo + chop. PROFILE sets how far each side goes.
class EnergyModule : public Module
{
public:
    enum { ENERGY, PROFILE, RES, DRIVEAMT, SPACEAMT, ECHOAMT, GATEAMT, LEVEL };
    std::string id() const override { return "ENERGY"; }
    std::vector<ParamSpec> params() const override
    {
        return { bip ("energy", "ENERGY").prim().dir (1),
                 choice ("profile", "PROFILE", { "DRUMS", "BASS", "MUSIC", "VOICE", "MASTER" }, 2).prim(),
                 pct ("res", "RES", 0.3f).prim(),
                 pct ("drive_amt", "DRIVE AMT", 0.4f).prim(),
                 pct ("space_amt", "SPACE AMT", 0.4f).prim(),
                 pct ("echo_amt", "ECHO AMT", 0.3f).prim(),
                 pct ("gate_amt", "GATE AMT", 0.0f).prim(),
                 levelParam().prim() };
    }
    void prepare (double rate, int mb) override
    {
        sr = rate; filt.prepare (rate); fdn.prepare (rate);
        for (auto& d : echo) d.prepare ((int) (rate * 2));
        os.prepare (rate, 2);
        juce::ignoreUnused (mb);
        reset();
    }
    void reset() override
    {
        filt.reset(); fdn.clear(); os.reset();
        for (auto& d : echo) d.clear();
        for (auto& f : echoLp) f.reset();
        echoFb[0] = echoFb[1] = 0.0f; chopZ = 1.0f; fdnActive = false;
    }
    void process (Ctx& c, juce::AudioBuffer<float>& io, juce::AudioBuffer<float>*, juce::AudioBuffer<float>*, juce::MidiBuffer&) override
    {
        static const float lpTravel[] = { 0.85f, 1.0f, 1.0f, 0.8f, 0.7f };
        static const float hpTravel[] = { 0.7f, 0.5f, 1.0f, 0.75f, 0.6f };
        const int prof = juce::jlimit (0, 4, c.ci (PROFILE));
        auto mapKnob = [&] (float k)
        {
            const float e = k - 0.5f;
            return 0.5f + (e < 0 ? e * lpTravel[prof] : e * hpTravel[prof]);
        };
        const float e = c.v (ENERGY) - 0.5f;
        const float dn = clamp01 (-e * 2.0f), up = clamp01 (e * 2.0f);
        const int n = c.n;
        // Drive (up side) before the filter.
        const float drv = up * c.v (DRIVEAMT);
        if (drv > 0.001f)
        {
            const float gIn = 1.0f + drv * 6.0f, comp = 1.4f / (1.0f + drv * 2.0f);
            for (int k = 0; k < 2; ++k) os.process (k, io.getWritePointer (k), n, [gIn, comp] (float x) { return fastTanh (x * gIn) * comp; });
        }
        filt.process (io.getArrayOfWritePointers(), 2, n, mapKnob (c.v0 (ENERGY)), mapKnob (c.v (ENERGY)), c.v (RES) * (0.4f + 0.6f * up));
        // Space (down side) and echo (up side).
        const float space = dn * c.v (SPACEAMT) * 0.5f, ech = up * c.v (ECHOAMT) * 0.5f;
        fdn.set (0.45f, 2.5f, 0.6f, 0.2f, false);
        const float echoD = (float) (0.25 * c.samplesPerBeat());
        for (auto& f : echoLp) f.set (3500.0f, 0.7071f, sr);
        const float chop = up * c.v (GATEAMT);
        for (int i = 0; i < n; ++i)
        {
            const float l = io.getSample (0, i), r = io.getSample (1, i);
            float rl = 0, rr = 0;
            if (space > 0.001f || fdnActive) fdn.tick (l * space, r * space, rl, rr, false);
            echo[0].push (l + echoFb[0] * 0.35f); echo[1].push (r + echoFb[1] * 0.35f);
            echoFb[0] = echoLp[0].tickLP (echo[0].read (echoD)); echoFb[1] = echoLp[1].tickLP (echo[1].read (echoD));
            float g = 1.0f;
            if (chop > 0.001f)
            {
                const double ph = std::fmod (c.ppqAt (i) * 4.0, 1.0);
                const float tgt = ph < 0.5 ? 1.0f : 1.0f - chop;
                chopZ = tgt + 0.995f * (chopZ - tgt);
                g = chopZ;
            }
            io.setSample (0, i, (l + rl + echoFb[0] * ech) * g);
            io.setSample (1, i, (r + rr + echoFb[1] * ech) * g);
        }
        fdnActive = space > 0.001f;
        float g0; const float g1 = levelDb (c, LEVEL, g0);
        applyGain (io, g0, g1);
        disp = c.v (ENERGY);
    }
    int displayHeight() const override { return 60; }
    void paint (juce::Graphics& g, juce::Rectangle<float> r) override
    {
        ui::frame (g, r, "ENERGY");
        drawPosition (g, r.reduced (12, 20), disp.load(), "DOWN", "UP", std::abs (disp.load() - 0.5f) < 0.02f ? ui::kDim : ui::kText);
    }
private:
    double sr = 48000.0;
    DJFilter filt;
    Oversampler os;
    Fdn fdn;
    bool fdnActive = false;
    DelayLine echo[2];
    Svf echoLp[2];
    float echoFb[2] {}, chopZ = 1.0f;
    std::atomic<float> disp { 0.5f };
};
ModulePtr makeENERGY() { return std::make_unique<EnergyModule>(); }
} // namespace sf
