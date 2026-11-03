// CONTROL, ALLOC, TAP
#include "Kit.h"
#include <map>

namespace sf
{
// =====================================================================================  CONTROL
// Suite brain: six macros, song select (Program Change on the chosen channel, default 16),
// STORE ALL. Put it on its own MIDI track receiving the OXI One.
class ControlModule : public Module, private juce::Timer
{
public:
    enum { M1, M2, M3, M4, M5, M6, SONG, STOREALL, PCCH, PCON, POFFSET };
    ControlModule() { startTimerHz (20); }
    ~ControlModule() override { stopTimer(); }
    std::string id() const override { return "CONTROL"; }
    bool instrument() const override { return true; }
    std::vector<ParamSpec> params() const override
    {
        std::vector<ParamSpec> p;
        for (int i = 1; i <= kMacros; ++i)
        {
            auto s = pct (("macro" + std::to_string (i)).c_str(), ("MACRO " + std::to_string (i)).c_str(), 0.0f);
            s.primary = true; s.song = true;
            p.push_back (s);
        }
        std::vector<std::string> songs; for (int i = 1; i <= kSongs; ++i) songs.push_back ("SONG " + std::string (i < 10 ? "0" : "") + std::to_string (i));
        auto sg = choice ("song", "SONG", songs, 0); sg.primary = true; sg.song = false; p.push_back (sg);
        p.push_back (momentary ("store_all", "STORE ALL").prim());
        std::vector<std::string> ch; for (int i = 1; i <= 16; ++i) ch.push_back ("CH " + std::to_string (i));
        auto pc = choice ("pc_channel", "PC CHANNEL", ch, 15); pc.song = false; p.push_back (pc);
        auto on = toggle ("pc_enable", "PC ENABLE", true); on.song = false; p.push_back (on);
        // Calibration for Pocket timing in Live (+ = every pocket later). Set once, by ear.
        auto off = knob ("pocket_offset", "POCKET OFFSET", -20.0f, 20.0f, 0.0f, "ms"); off.song = false; off.glide = false; p.push_back (off);
        return p;
    }
    // VST3 delivers Program Change as a program change: 16 programs = 16 songs.
    int numPrograms() const override { return kSongs; }
    int currentProgram() const override { return std::max (0, (int) Bus::get().d().song.load()); }
    void setProgram (int p) override
    {
        auto& d = Bus::get().d();
        const int s = juce::jlimit (0, kSongs - 1, p);
        if (d.song.load() != s) { d.song = s; d.songSeq.fetch_add (1); }
        pcSong = s;
    }
    void prepare (double, int) override {}
    void process (Ctx& c, juce::AudioBuffer<float>&, juce::AudioBuffer<float>*, juce::AudioBuffer<float>*, juce::MidiBuffer& midi) override
    {
        auto& d = c.bus->d();
        d.controlSeenMs = Bus::nowMs();
        d.pocketOffsetMs = c.v (POFFSET);
        for (int i = 0; i < kMacros; ++i) d.macro[i] = c.v (M1 + i);
        // Program Change selects the song.
        if (c.b (PCON))
            for (const auto meta : midi)
            {
                const auto m = meta.getMessage();
                if (m.isProgramChange() && m.getChannel() == c.ci (PCCH) + 1)
                {
                    const int s = juce::jlimit (0, kSongs - 1, m.getProgramChangeNumber());
                    d.song = s; d.songSeq.fetch_add (1);
                    pcSong = s;
                }
            }
        // A change of the SONG parameter (knob, menu, automation) selects that song. The
        // Program Change mirror arriving later matches the bus already and does nothing.
        const int sp = c.ci (SONG);
        if (sp != lastSongParam)
        {
            lastSongParam = sp;
            if (sp != d.song.load()) { d.song = sp; d.songSeq.fetch_add (1); }
        }
        const bool st = c.b (STOREALL);
        if (st && ! storeWas)
        {
            if (d.song.load() < 0) { d.song = sp; d.songSeq.fetch_add (1); }
            d.storeSeq.fetch_add (1);
        }
        storeWas = st;
    }
    int displayHeight() const override { return 150; }
    void paint (juce::Graphics& g, juce::Rectangle<float> r) override
    {
        auto& bus = Bus::get();
        auto& d = bus.d();
        auto left = r.removeFromLeft (r.getWidth() * 0.45f);
        ui::frame (g, left, "MACROS");
        auto in = left.reduced (10, 18);
        for (int i = 0; i < kMacros; ++i)
        {
            auto row = in.removeFromTop (in.getHeight() / (float) (kMacros - i));
            ui::label (g, "M" + juce::String (i + 1), row.removeFromLeft (28), ui::kDim, 10.0f);
            const float v = d.macro[i].load();
            g.setColour (ui::kTrack); g.fillRect (row.withSizeKeepingCentre (row.getWidth(), 6));
            g.setColour (ui::kText); g.fillRect (row.withSizeKeepingCentre (row.getWidth(), 6).withWidth (row.getWidth() * v));
        }
        r.removeFromLeft (8);
        ui::frame (g, r, "SUITE");
        auto s = r.reduced (10, 18);
        auto line = [&] (const juce::String& k, const juce::String& v, juce::Colour col)
        {
            auto row = s.removeFromTop (16);
            ui::label (g, k, row.removeFromLeft (110), ui::kFaint, 10.0f);
            ui::label (g, v, row, col, 11.0f);
        };
        const int song = d.song.load();
        line ("SONG", song < 0 ? "--" : juce::String (song + 1).paddedLeft ('0', 2), ui::kText);
        line ("BUS", bus.shared() ? "SHARED" : "LOCAL ONLY", bus.shared() ? ui::kGreen : ui::kAmber);
        line ("KICK", bus.kickAlive (2000) ? "REPORTING" : "SILENT", bus.kickAlive (2000) ? ui::kGreen : ui::kFaint);
        line ("ALLOC", bus.allocAlive() ? "RUNNING" : "NOT LOADED", bus.allocAlive() ? ui::kGreen : ui::kFaint);
        int alive = 0; juce::String names;
        for (int i = 0; i < kSources; ++i)
            if (bus.sourceAlive (i))
            {
                ++alive;
                char nm[24]; Bus::unpackName (d.src[i].name, nm);
                if (names.length() < 60) names << nm << " ";
            }
        line ("SOURCES", juce::String (alive), ui::kText);
        ui::label (g, names, s.removeFromTop (16), ui::kDim, 10.0f);
    }
private:
    void timerCallback() override
    {
        // Mirror a Program Change into the SONG parameter so Live (and the E16) follow.
        const int pc = pcSong.exchange (-1);
        if (pc >= 0 && setParam) setParam (SONG, (float) pc);
    }
    std::atomic<int> pcSong { -1 };
    int lastSongParam = -1;
    bool storeWas = false;
};
ModulePtr makeCONTROL() { return std::make_unique<ControlModule>(); }

// =====================================================================================  ALLOC
// Mix assigner. Automatic where sources don't overlap. Where two or more sources sit in the same
// band(s), the conflict gets one of three SLOT knobs: centre = automatic (role priority),
// turning sweeps ownership through the sources. Pass-through audio: put it anywhere (master).
class AllocModule : public Module, private juce::Timer
{
public:
    enum { SLOT1, SLOT2, SLOT3, DEPTH, SENS, HOLD, ENABLE };
    AllocModule() { startTimerHz (20); }
    ~AllocModule() override { stopTimer(); }
    std::string id() const override { return "ALLOC"; }
    std::vector<ParamSpec> params() const override
    {
        auto s1 = pct ("slot1", "SLOT 1", 0.5f); s1.unit = "SLOT"; s1.primary = true; s1.glide = false; s1.song = false;
        auto s2 = s1; s2.id = "slot2"; s2.name = "SLOT 2";
        auto s3 = s1; s3.id = "slot3"; s3.name = "SLOT 3";
        return { s1, s2, s3,
                 knob ("depth", "DEPTH", 0.0f, 18.0f, 9.0f, "dB").prim(),
                 pct ("sensitivity", "SENSITIVITY", 0.5f).prim(),
                 choice ("hold", "HOLD", { "1 BEAT", "2 BEAT", "4 BEAT" }, 0).prim(),
                 toggle ("enable", "ENABLE", true).prim() };
    }
    void prepare (double rate, int) override { sr = rate; }

    struct Conflict { uint32_t mask = 0; int lo = 0, hi = 0; float severity = 0; };

    void process (Ctx& c, juce::AudioBuffer<float>&, juce::AudioBuffer<float>*, juce::AudioBuffer<float>*, juce::MidiBuffer&) override
    {
        auto& bus = *c.bus;
        auto& d = bus.d();
        d.allocSeenMs = Bus::nowMs();
        const bool en = c.b (ENABLE);
        const float floorDb = -58.0f + (1.0f - c.v (SENS)) * 22.0f;
        static const double holdB[] = { 1.0, 2.0, 4.0 };
        const double holdSec = holdB[juce::jlimit (0, 2, c.ci (HOLD))] * 60.0 / c.bpm;
        const double dt = c.n / sr;
        clock += dt;

        // Gather sources.
        float E[kSources][kBands];
        bool alive[kSources];
        int role[kSources];
        float bandMax[kBands];
        for (int b = 0; b < kBands; ++b) bandMax[b] = -120.0f;
        for (int s = 0; s < kSources; ++s)
        {
            alive[s] = bus.sourceAlive (s) && d.src[s].enabled.load() != 0;
            role[s] = d.src[s].role.load();
            for (int b = 0; b < kBands; ++b)
            {
                E[s][b] = alive[s] ? d.src[s].energy[b].load() : -120.0f;
                bandMax[b] = std::max (bandMax[b], E[s][b]);
            }
        }
        // Active sets per band, stabilised: a band's set must hold for HOLD before it counts,
        // and a counted set survives 2x HOLD of absence (no flicker).
        uint32_t mask[kBands];
        for (int b = 0; b < kBands; ++b)
        {
            uint32_t m = 0;
            for (int s = 0; s < kSources; ++s)
                if (alive[s] && E[s][b] > floorDb && E[s][b] > bandMax[b] - 15.0f) m |= 1u << s;
            if (m != rawMask[b]) { rawMask[b] = m; rawSince[b] = clock; }
            if (clock - rawSince[b] >= holdSec) { stableMask[b] = rawMask[b]; lastStable[b] = clock; }
            else if (clock - lastStable[b] > 2.0 * holdSec) stableMask[b] = rawMask[b] & stableMask[b];
            mask[b] = __builtin_popcount (stableMask[b]) >= 2 ? stableMask[b] : 0u;
        }
        // Group contiguous bands with the same source set.
        Conflict found[kBands];
        int nf = 0;
        for (int b = 0; b < kBands;)
        {
            if (mask[b] == 0) { ++b; continue; }
            Conflict cf; cf.mask = mask[b]; cf.lo = b;
            int e = b;
            while (e + 1 < kBands && mask[e + 1] == mask[b]) ++e;
            cf.hi = e;
            for (int k = cf.lo; k <= cf.hi; ++k)
            {
                float a = -120, bb = -120;
                for (int s = 0; s < kSources; ++s) if (cf.mask & (1u << s)) { if (E[s][k] > a) { bb = a; a = E[s][k]; } else if (E[s][k] > bb) bb = E[s][k]; }
                cf.severity += bb - floorDb;
            }
            found[nf++] = cf;
            b = e + 1;
        }
        std::sort (found, found + nf, [] (const Conflict& a, const Conflict& b) { return a.severity > b.severity; });

        // Slot binding. A slot keeps its conflict while touched in the last 3 s or while the
        // conflict exists; free / stale slots take the worst unbound conflicts.
        const int64_t now = Bus::nowMs();
        for (int k = 0; k < kConflictSlots; ++k)
        {
            const float v = getParam ? getParam (SLOT1 + k) : c.v (SLOT1 + k);
            if (std::abs (v - lastSlotVal[k].load()) > 0.002f) { lastSlotVal[k] = v; touchedMs[k] = now; }
            slotNow[k] = v;
        }
        for (int k = 0; k < kConflictSlots; ++k)
        {
            if (bound[k].mask == 0) continue;
            bool exists = false;
            for (int q = 0; q < nf; ++q) if (found[q].mask == bound[k].mask) { bound[k] = found[q]; exists = true; }
            if (! exists && now - touchedMs[k] > 3000 && clock - boundSeen[k] > 2.0 * holdSec) bound[k] = {};
            if (exists) boundSeen[k] = clock;
        }
        for (int q = 0; q < nf; ++q)
        {
            const auto& f = found[q];
            bool isBound = false;
            for (auto& bnd : bound) if (bnd.mask == f.mask) isBound = true;
            if (isBound) continue;
            for (int k = 0; k < kConflictSlots; ++k)
                if (bound[k].mask == 0 && now - touchedMs[k] > 3000)
                {
                    bound[k] = f; boundSeen[k] = clock;
                    bindSeq[k].fetch_add (1);          // the timer restores this conflict's remembered value
                    break;
                }
        }

        // Cuts.
        const float depth = en ? c.v (DEPTH) : 0.0f;
        float cut[kSources][kBands] {};
        for (int q = 0; q < nf; ++q)
        {
            const auto& f = found[q];
            int parts[kSources]; int np = 0;
            for (int s = 0; s < kSources; ++s) if (f.mask & (1u << s)) parts[np++] = s;
            std::sort (parts, parts + np, [&] (int a, int b) { return role[a] != role[b] ? role[a] < role[b] : a < b; });
            float knobV = 0.5f;
            for (int k = 0; k < kConflictSlots; ++k) if (bound[k].mask == f.mask) knobV = c.v (SLOT1 + k);
            // Centre (AUTO): the highest-priority source owns the band. Right half: ownership
            // moves continuously along the priority order to the last source. Left half: SHARE,
            // cuts fade out until nobody is cut.
            float w[kSources] {};
            const float right = clamp01 ((knobV - 0.54f) / 0.46f), share = clamp01 ((0.46f - knobV) / 0.46f);
            const float x = right * (float) (np - 1);
            for (int i = 0; i < np; ++i) w[i] = lerp (std::max (0.0f, 1.0f - std::abs (x - (float) i)), 1.0f, share);
            for (int b = f.lo; b <= f.hi; ++b)
            {
                float ownerE = -120.0f;
                float wMax = 0.0f; for (int i = 0; i < np; ++i) wMax = std::max (wMax, w[i]);
                for (int i = 0; i < np; ++i) if (w[i] >= wMax - 1e-4f) ownerE = std::max (ownerE, E[parts[i]][b]);
                const float strength = clamp01 ((ownerE - floorDb) / 18.0f);
                for (int i = 0; i < np; ++i) cut[parts[i]][b] = depth * (1.0f - w[i]) * strength;
            }
        }
        for (int s = 0; s < kSources; ++s)
            for (int b = 0; b < kBands; ++b) d.src[s].cut[b] = cut[s][b];
        for (int k = 0; k < kConflictSlots; ++k)
        {
            d.conflicts[k].sources = bound[k].mask;
            d.conflicts[k].bandLo = bound[k].lo;
            d.conflicts[k].bandHi = bound[k].hi;
            d.conflicts[k].active = bound[k].mask != 0;
        }
        // Display copies.
        for (int s = 0; s < kSources; ++s) for (int b = 0; b < kBands; ++b) { dispE[s][b] = E[s][b]; dispCut[s][b] = cut[s][b]; }
        for (int b = 0; b < kBands; ++b) dispMask[b] = mask[b];
        dispFloor = floorDb;
    }

    void save (juce::ValueTree& t) const override
    {
        for (auto& kv : slotValues)
        {
            juce::ValueTree v ("SLOTVALUE");
            v.setProperty ("key", juce::String (kv.first), nullptr);
            v.setProperty ("value", kv.second, nullptr);
            t.appendChild (v, nullptr);
        }
    }
    void load (const juce::ValueTree& t) override
    {
        slotValues.clear();
        for (auto v : t) if (v.getType().toString() == "SLOTVALUE") slotValues[v.getProperty ("key").toString().toStdString()] = (float) v.getProperty ("value");
    }

    int displayHeight() const override { return 210; }
    void paint (juce::Graphics& g, juce::Rectangle<float> r) override
    {
        auto& bus = Bus::get();
        auto& d = bus.d();
        auto map = r.removeFromLeft (r.getWidth() * 0.62f);
        ui::frame (g, map, "SPECTRUM ALLOCATION");
        auto in = map.reduced (10, 18).withTrimmedBottom (14);
        const float cw = in.getWidth() / (float) kBands;
        static const char* bandNames[kBands] = { "40", "80", "120", "180", "270", "390", "600", "1K", "2.5K", "9K" };
        for (int b = 0; b < kBands; ++b)
        {
            auto col = juce::Rectangle<float> (in.getX() + b * cw, in.getY(), cw, in.getHeight()).reduced (3, 0);
            const bool conflict = dispMask[b].load() != 0;
            g.setColour (ui::kGrid); g.fillRect (col);
            // Stack per source: height = energy above floor.
            float y = col.getBottom();
            int shade = 0;
            for (int s = 0; s < kSources; ++s)
            {
                if (! bus.sourceAlive (s)) continue;
                const float e = dispE[s][b].load() - dispCut[s][b].load();
                const float h = clamp01 ((e - dispFloor.load()) / 50.0f) * col.getHeight() * 0.5f;
                if (h < 0.5f) { ++shade; continue; }
                g.setColour (juce::Colour::greyLevel (0.9f - 0.15f * (float) (shade++ % 5)));
                g.fillRect (col.getX(), y - h, col.getWidth(), h - 1.0f);
                y -= h;
            }
            if (y < col.getY()) { g.setColour (ui::kRed); g.drawRect (col, 1.5f); }
            else if (conflict) { g.setColour (ui::kAmber); g.drawRect (col, 1.0f); }
            ui::label (g, bandNames[b], juce::Rectangle<float> (col.getX(), in.getBottom() + 1, col.getWidth(), 12), ui::kFaint, 9.0f, juce::Justification::centred);
        }
        r.removeFromLeft (8);
        ui::frame (g, r, "CONFLICT SLOTS");
        auto s = r.reduced (10, 18);
        for (int k = 0; k < kConflictSlots; ++k)
        {
            auto row = s.removeFromTop (s.getHeight() / (float) (kConflictSlots - k));
            const uint32_t m = d.conflicts[k].sources.load();
            ui::label (g, "SLOT " + juce::String (k + 1), row.removeFromTop (14), m ? ui::kAmber : ui::kFaint, 10.0f);
            if (m == 0) { ui::label (g, "AUTO / NO CONFLICT", row.removeFromTop (14), ui::kFaint, 10.0f); continue; }
            juce::String names;
            for (int i = 0; i < kSources; ++i) if (m & (1u << i)) { char nm[24]; Bus::unpackName (d.src[i].name, nm); names << nm << " / "; }
            names = names.dropLastCharacters (3);
            ui::label (g, names, row.removeFromTop (14), ui::kText, 10.0f);
            ui::label (g, juce::String ((int) kBandEdges[d.conflicts[k].bandLo.load()]) + "-" + juce::String ((int) kBandEdges[d.conflicts[k].bandHi.load() + 1]) + " HZ",
                       row.removeFromTop (14), ui::kDim, 10.0f);
        }
    }
private:
    static std::string keyFor (uint32_t mask)
    {
        auto& d = Bus::get().d();
        std::vector<std::string> parts;
        // Names only (bus slot numbers change between sessions), so choices survive reloading the set.
        for (int i = 0; i < kSources; ++i) if (mask & (1u << i)) { char nm[24]; Bus::unpackName (d.src[i].name, nm); parts.push_back (nm); }
        std::sort (parts.begin(), parts.end());
        std::string k; for (auto& p : parts) k += p + "|";
        return k;
    }
    // Message thread: remembers slot values per conflict and restores them when a conflict
    // binds again (string keys and the map never touch the audio thread).
    void timerCallback() override
    {
        auto& d = Bus::get().d();
        const int64_t now = Bus::nowMs();
        for (int k = 0; k < kConflictSlots; ++k)
        {
            const uint32_t mask = d.conflicts[k].sources.load();
            if (mask != 0 && touchedMs[k].load() > 0 && now - touchedMs[k].load() < 3000)
                slotValues[keyFor (mask)] = slotNow[k].load();
            const uint32_t seq = bindSeq[k].load();
            if (seq != seenBindSeq[k] && mask != 0)
            {
                seenBindSeq[k] = seq;
                const auto it = slotValues.find (keyFor (mask));
                const float v = it != slotValues.end() ? it->second : 0.5f;
                lastSlotVal[k] = v;                     // not a touch
                if (setParam) setParam (SLOT1 + k, v);
            }
        }
    }
    double sr = 48000.0, clock = 0.0;
    uint32_t rawMask[kBands] {}, stableMask[kBands] {};
    double rawSince[kBands] {}, lastStable[kBands] {};
    Conflict bound[kConflictSlots];
    double boundSeen[kConflictSlots] {};
    std::atomic<float> lastSlotVal[kConflictSlots] { 0.5f, 0.5f, 0.5f }, slotNow[kConflictSlots] { 0.5f, 0.5f, 0.5f };
    std::atomic<int64_t> touchedMs[kConflictSlots] {};
    std::atomic<uint32_t> bindSeq[kConflictSlots] {};
    uint32_t seenBindSeq[kConflictSlots] {};
    std::map<std::string, float> slotValues;
    std::atomic<float> dispE[kSources][kBands] {}, dispCut[kSources][kBands] {}, dispFloor { -50.0f };
    std::atomic<uint32_t> dispMask[kBands] {};
};
ModulePtr makeALLOC() { return std::make_unique<AllocModule>(); }

// =====================================================================================  TAP
// Puts any Live track (stems, hardware, other plugins) into Pocket and ALLOC. Makes no sound.
class TapModule : public Module
{
public:
    enum { ROLE, ALLOCON, PDEPTH, PSHAPE, LEVEL };
    std::string id() const override { return "TAP"; }
    std::vector<ParamSpec> params() const override
    {
        std::vector<std::string> roles; for (int i = 0; i < kNumRoles; ++i) roles.push_back (kRoleNames[i]);
        auto r = choice ("role", "ROLE", roles, ROLE_OTHER); r.primary = true; r.song = false;
        return { r, toggle ("alloc", "ALLOC", true).prim(), pct ("pocket_depth", "POCKET DEPTH", 0.0f).prim(),
                 pct ("pocket_shape", "POCKET SHAPE", 0.3f).prim(), levelParam().prim() };
    }
    int latency() const override { return Pocket::kLatency; }
    void prepare (double rate, int mb) override { pocket.prepare (rate, mb); tap.prepare (rate); }
    void process (Ctx& c, juce::AudioBuffer<float>& io, juce::AudioBuffer<float>*, juce::AudioBuffer<float>*, juce::MidiBuffer&) override
    {
        const int role = c.ci (ROLE);
        if (role != lastRole) { lastRole = role; tap.setIdentity (role, (std::string ("TAP ") + kRoleNames[role]).c_str()); }
        pocket.process (c, io.getArrayOfWritePointers(), 2, c.n, c.v (PDEPTH), c.v (PSHAPE), nullptr, c.v (PDEPTH) > 0.001f);
        tap.process (io.getArrayOfWritePointers(), 2, c.n, c.b (ALLOCON));
        float g0; const float g1 = levelDb (c, LEVEL, g0);
        applyGain (io, g0, g1);
    }
    int displayHeight() const override { return 80; }
    void paint (juce::Graphics& g, juce::Rectangle<float> r) override { pocket.paint (g, r); }
private:
    Pocket pocket;
    AllocTap tap;
    int lastRole = -1;
};
ModulePtr makeTAP() { return std::make_unique<TapModule>(); }
} // namespace sf
