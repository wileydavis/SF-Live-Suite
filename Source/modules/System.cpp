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
// Mix assigner. You put the sources in order (drag in the list, top = most important). Each of
// the 10 bands has a priority curve over that list:
//   PRIORITY - where the curve's peak sits (top .. bottom of the list)
//   DEPTH    - how hard sources away from the peak are cut (0 = nothing cut)
//   Q        - width of the peak (wide = neighbours share, narrow = one source owns the band)
// MODE DYNAMIC cuts a source only while a more favoured source is playing in that band;
// STATIC cuts all the time (a fixed EQ). Pass-through audio: put it on the master.
class AllocModule : public Module, private juce::Timer
{
public:
    enum { ENABLE, MODE, MAXCUT, SENS, PRI0 = 4, DEPTH0 = PRI0 + kBands, Q0 = DEPTH0 + kBands };
    AllocModule() { for (auto& r : rank) r = -1; startTimerHz (20); }
    ~AllocModule() override { stopTimer(); }
    std::string id() const override { return "ALLOC"; }
    std::vector<ParamSpec> params() const override
    {
        std::vector<ParamSpec> p {
            toggle ("enable", "ENABLE", true).prim(),
            choice ("mode", "MODE", { "DYNAMIC", "STATIC" }, 0).prim(),
            knob ("max_cut", "MAX CUT", 0.0f, 24.0f, 12.0f, "dB").prim(),
            pct ("sensitivity", "SENSITIVITY", 0.5f).prim() };
        for (int b = 0; b < kBands; ++b) { auto k = pct (("pri" + std::to_string (b + 1)).c_str(), ("PRIORITY " + std::to_string (b + 1)).c_str(), 0.0f); k.glide = true; p.push_back (k.hide()); }
        for (int b = 0; b < kBands; ++b) p.push_back (pct (("depth" + std::to_string (b + 1)).c_str(), ("DEPTH " + std::to_string (b + 1)).c_str(), 0.0f).hide());
        for (int b = 0; b < kBands; ++b) p.push_back (pct (("q" + std::to_string (b + 1)).c_str(), ("Q " + std::to_string (b + 1)).c_str(), 0.5f).hide());
        return p;
    }
    void prepare (double rate, int) override { sr = rate; }

    // Peak position in list rows and width (rows) for a band.
    static void curve (float pri, float q, int n, float& peak, float& sigma)
    {
        peak = pri * (float) std::max (0, n - 1);
        sigma = lerp (3.0f, 0.3f, q);
    }
    static float weight (int i, float peak, float sigma) { const float d = ((float) i - peak) / sigma; return std::exp (-0.5f * d * d); }

    void process (Ctx& c, juce::AudioBuffer<float>&, juce::AudioBuffer<float>*, juce::AudioBuffer<float>*, juce::MidiBuffer&) override
    {
        auto& d = c.bus->d();
        d.allocSeenMs = Bus::nowMs();
        const bool en = c.b (ENABLE), dynamic = c.ci (MODE) == 0;
        const float maxCut = c.v (MAXCUT);
        const float floorDb = -58.0f + (1.0f - c.v (SENS)) * 22.0f;
        // Sources in list order (ranks come from the message thread).
        int order[kSources]; int n = 0;
        for (int r = 0; r < kSources; ++r) slotAt[r] = -1;
        for (int s = 0; s < kSources; ++s)
        {
            const int rk = rank[s].load();
            if (rk >= 0 && rk < kSources && Bus::get().sourceAlive (s) && d.src[s].enabled.load() != 0) slotAt[rk] = s;
        }
        for (int r = 0; r < kSources; ++r) if (slotAt[r] >= 0) order[n++] = slotAt[r];
        // Sources the list hasn't placed yet (just appeared): after the list, in role order.
        for (int role = 0; role < kNumRoles; ++role)
            for (int s = 0; s < kSources; ++s)
                if (rank[s].load() < 0 && Bus::get().sourceAlive (s) && d.src[s].enabled.load() != 0 && d.src[s].role.load() == role && n < kSources)
                    order[n++] = s;
        for (int s = 0; s < kSources; ++s) for (int b = 0; b < kBands; ++b) cutOut[s][b] = 0.0f;
        for (int b = 0; b < kBands; ++b)
        {
            float peak, sigma;
            curve (c.v (PRI0 + b), c.v (Q0 + b), n, peak, sigma);
            const float depth = c.v (DEPTH0 + b);
            float w[kSources], e[kSources];
            for (int i = 0; i < n; ++i) { w[i] = weight (i, peak, sigma); e[i] = d.src[order[i]].energy[b].load(); }
            for (int i = 0; i < n; ++i)
            {
                float cut = en ? maxCut * depth * (1.0f - w[i]) : 0.0f;
                if (cut > 0.0f && dynamic)
                {
                    // Only while someone more favoured is actually playing in this band.
                    float winner = -120.0f;
                    for (int j = 0; j < n; ++j) if (w[j] > w[i] + 0.05f) winner = std::max (winner, e[j]);
                    cut *= clamp01 ((winner - floorDb) / 18.0f);
                }
                cutOut[order[i]][b] = cut;
            }
        }
        for (int s = 0; s < kSources; ++s)
            for (int b = 0; b < kBands; ++b)
            {
                d.src[s].cut[b] = cutOut[s][b];
                dispE[s][b] = d.src[s].energy[b].load();
                dispCut[s][b] = cutOut[s][b];
            }
        for (int k = 0; k < kConflictSlots; ++k) d.conflicts[k].active = 0;
        dispFloor = floorDb;
    }

    // ---- state: the list order (by name)
    void save (juce::ValueTree& t) const override
    {
        juce::String s; for (auto& nm : listOrder) s << nm << "\n";
        t.setProperty ("order", s, nullptr);
    }
    void load (const juce::ValueTree& t) override
    {
        listOrder.clear();
        for (auto& line : juce::StringArray::fromLines (t.getProperty ("order").toString())) if (line.isNotEmpty()) listOrder.push_back (line.toStdString());
    }

    // ---- display
    int displayHeight() const override { return 470; }
    void paint (juce::Graphics& g, juce::Rectangle<float> r) override
    {
        area = r;
        const auto rows = visibleRows();
        auto list = listArea();
        ui::frame (g, r.withWidth (list.getWidth() + 8), "SOURCES  DRAG TO ORDER");
        const float rh = rowH();
        for (size_t i = 0; i < rows.size(); ++i)
        {
            auto row = juce::Rectangle<float> (list.getX(), list.getY() + rh * (float) i, list.getWidth(), rh);
            const bool dragging = dragRow == (int) i;
            g.setColour (dragging ? ui::kText : ui::kLine);
            g.drawRect (row.reduced (1.0f), 1.0f);
            ui::label (g, juce::String (i + 1), row.removeFromLeft (18).withTrimmedLeft (4), ui::kFaint, 9.0f);
            ui::label (g, juce::String (rows[i].label), row, dragging ? ui::kAccent : ui::kText, 10.5f);
        }
        if (rows.empty()) ui::label (g, "NO SOURCES", list.withHeight (20), ui::kFaint, 10.0f);
        static const char* bandNames[kBands] = { "40", "80", "120", "180", "270", "390", "600", "1K", "2.5K", "9K" };
        for (int b = 0; b < kBands; ++b)
        {
            auto col = bandArea (b);
            ui::frame (g, col, bandNames[b]);
            auto lanes = col.reduced (4.0f, 0.0f).withTop (list.getY()).withHeight (rh * (float) std::max<size_t> (1, rows.size()));
            const int n = (int) rows.size();
            float peak, sigma;
            curve (getParam ? getParam (PRI0 + b) : 0.0f, getParam ? getParam (Q0 + b) : 0.5f, n, peak, sigma);
            const float depth = getParam ? getParam (DEPTH0 + b) : 0.0f;
            // Per source: what plays (white) and what ALLOC takes away (amber).
            for (int i = 0; i < n; ++i)
            {
                const int s = rows[(size_t) i].slot;
                auto lane = juce::Rectangle<float> (lanes.getX(), lanes.getY() + rh * (float) i, lanes.getWidth(), rh).reduced (0.0f, 4.0f);
                g.setColour (ui::kGrid); g.fillRect (lane);
                const float pre = clamp01 ((dispE[s][b].load() - dispFloor.load()) / 50.0f);
                const float post = clamp01 ((dispE[s][b].load() - dispCut[s][b].load() - dispFloor.load()) / 50.0f);
                g.setColour (ui::kAmber); g.fillRect (lane.withWidth (lane.getWidth() * pre));
                g.setColour (ui::kText);  g.fillRect (lane.withWidth (lane.getWidth() * post));
            }
            // The priority curve: x = how protected each row is (right = fully protected).
            if (n > 0)
            {
                juce::Path p;
                const int steps = std::max (2, n * 8);
                for (int k = 0; k <= steps; ++k)
                {
                    const float rowPos = (float) k / (float) steps * (float) n - 0.5f;
                    const float d = (rowPos - peak) / sigma;
                    const float wgt = 1.0f - depth * (1.0f - std::exp (-0.5f * d * d));
                    const float x = lanes.getX() + lanes.getWidth() * (0.15f + 0.85f * wgt);
                    const float y = lanes.getY() + rh * (rowPos + 0.5f);
                    if (k == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
                }
                const bool touched = dragParam == PRI0 + b || dragParam == DEPTH0 + b || dragParam == Q0 + b;
                g.setColour (touched ? ui::kAccent : depth > 0.001f ? ui::kDim : ui::kFaint);
                g.strokePath (p, juce::PathStrokeType (touched ? 2.0f : 1.4f));
                // Who owns this band: the source nearest the peak (only when the curve does something).
                if (depth > 0.001f)
                {
                    const int owner = juce::jlimit (0, n - 1, (int) std::lround (peak));
                    juce::String nm (rows[(size_t) owner].label);
                    nm = nm.fromFirstOccurrenceOf ("TAP ", false, false).isNotEmpty() && nm.startsWith ("TAP ") ? nm.substring (4) : nm;
                    ui::label (g, nm.substring (0, 9), juce::Rectangle<float> (col.getX() + 4.0f, col.getBottom() - 134.0f, col.getWidth() - 8.0f, 14.0f), ui::kText, 9.0f);
                }
            }
            // Knobs.
            for (int k = 0; k < 3; ++k)
            {
                const int idx = (k == 0 ? PRI0 : k == 1 ? DEPTH0 : Q0) + b;
                auto kr = knobRect (b, k);
                const float v = getParam ? getParam (idx) : 0.0f;
                drawKnob (g, kr, v, k == 0 ? "PRI" : k == 1 ? "DEPTH" : "Q", dragParam == idx);
            }
        }
    }
    void displayMouse (juce::Point<float> p, juce::Rectangle<float> r, bool down) override
    {
        area = r;
        if (down)
        {
            dragParam = -1; dragRow = -1;
            for (int b = 0; b < kBands; ++b)
                for (int k = 0; k < 3; ++k)
                    if (knobRect (b, k).contains (p)) { dragParam = (k == 0 ? PRI0 : k == 1 ? DEPTH0 : Q0) + b; startVal = getParam ? getParam (dragParam) : 0.0f; startY = p.y; }
            const auto rows = visibleRows();
            if (listArea().contains (p))
            {
                const int i = (int) ((p.y - listArea().getY()) / rowH());
                if (i >= 0 && i < (int) rows.size()) dragRow = i;
            }
            return;
        }
        if (dragParam >= 0 && setParam) setParam (dragParam, clamp01 (startVal + (startY - p.y) / 160.0f));
        if (dragRow >= 0)
        {
            const auto rows = visibleRows();
            const int target = juce::jlimit (0, std::max (0, (int) rows.size() - 1), (int) ((p.y - listArea().getY()) / rowH()));
            if (target != dragRow)
            {
                // Move in the stored order: take the dragged name out, put it before/after the target's name.
                const std::string moving = rows[(size_t) dragRow].label, anchor = rows[(size_t) target].label;
                listOrder.erase (std::remove (listOrder.begin(), listOrder.end(), moving), listOrder.end());
                auto it = std::find (listOrder.begin(), listOrder.end(), anchor);
                if (target > dragRow && it != listOrder.end()) ++it;
                listOrder.insert (it, moving);
                dragRow = target;
                updateRanks();
            }
        }
    }
    void displayMouseUp() override { dragParam = -1; dragRow = -1; }

private:
    struct Row { std::string label; int slot; };

    juce::Rectangle<float> listArea() const { return { area.getX() + 6.0f, area.getY() + 20.0f, 150.0f, area.getHeight() - 140.0f }; }
    float rowH() const { return 22.0f; }
    juce::Rectangle<float> bandArea (int b) const
    {
        const float x0 = area.getX() + 164.0f, w = (area.getRight() - x0) / (float) kBands;
        return { x0 + w * (float) b, area.getY(), w - 3.0f, area.getHeight() };
    }
    juce::Rectangle<float> knobRect (int b, int k) const
    {
        auto col = bandArea (b);
        return { col.getX() + 4.0f, col.getBottom() - 116.0f + (float) k * 38.0f, col.getWidth() - 8.0f, 36.0f };
    }
    static void drawKnob (juce::Graphics& g, juce::Rectangle<float> r, float v, const char* name, bool hot)
    {
        auto knob = r.removeFromLeft (r.getHeight()).reduced (4.0f);
        const float a0 = juce::degreesToRadians (225.0f), a1 = juce::degreesToRadians (495.0f);
        juce::Path track; track.addCentredArc (knob.getCentreX(), knob.getCentreY(), knob.getWidth() * 0.45f, knob.getHeight() * 0.45f, 0.0f, a0, a1, true);
        g.setColour (ui::kTrack); g.strokePath (track, juce::PathStrokeType (2.5f));
        juce::Path val; val.addCentredArc (knob.getCentreX(), knob.getCentreY(), knob.getWidth() * 0.45f, knob.getHeight() * 0.45f, 0.0f, a0, a0 + v * (a1 - a0), true);
        g.setColour (hot ? ui::kAccent : ui::kText); g.strokePath (val, juce::PathStrokeType (2.5f));
        ui::label (g, name, r.removeFromTop (r.getHeight() * 0.5f), ui::kFaint, 8.5f);
        ui::label (g, juce::String (juce::roundToInt (v * 100.0f)), r, hot ? ui::kAccent : ui::kText, 9.5f);
    }

    // Alive sources as list rows, in stored order (new names are appended by role priority).
    std::vector<Row> visibleRows() const
    {
        std::vector<Row> rows;
        auto& d = Bus::get().d();
        std::vector<std::pair<std::string, int>> alive;
        std::map<std::string, int> seen;
        std::vector<std::pair<int, int>> byRole;
        for (int s = 0; s < kSources; ++s) if (Bus::get().sourceAlive (s)) byRole.push_back ({ d.src[s].role.load(), s });
        std::sort (byRole.begin(), byRole.end());
        for (auto& rs : byRole)
        {
            char nm[24]; Bus::unpackName (d.src[rs.second].name, nm);
            std::string label = nm;
            const int k = ++seen[label];
            if (k > 1) label += " " + std::to_string (k);       // two tracks with the same name
            alive.push_back ({ label, rs.second });
        }
        for (auto& nm : listOrder) for (auto& a : alive) if (a.first == nm) rows.push_back ({ a.first, a.second });
        for (auto& a : alive) if (std::find (listOrder.begin(), listOrder.end(), a.first) == listOrder.end()) rows.push_back ({ a.first, a.second });
        return rows;
    }
    void updateRanks()
    {
        const auto rows = visibleRows();
        int r[kSources]; for (auto& x : r) x = -1;
        for (size_t i = 0; i < rows.size(); ++i) r[rows[i].slot] = (int) i;
        for (int s = 0; s < kSources; ++s) rank[s] = r[s];
    }
    void timerCallback() override
    {
        // Remember newcomers in the order, so the list only changes when you drag it.
        for (auto& row : visibleRows())
            if (std::find (listOrder.begin(), listOrder.end(), row.label) == listOrder.end()) listOrder.push_back (row.label);
        updateRanks();
    }

    double sr = 48000.0;
    std::vector<std::string> listOrder;
    std::atomic<int> rank[kSources];
    int slotAt[kSources] {};
    float cutOut[kSources][kBands] {};
    juce::Rectangle<float> area;
    int dragParam = -1, dragRow = -1;
    float startVal = 0.0f, startY = 0.0f;
    std::atomic<float> dispE[kSources][kBands] {}, dispCut[kSources][kBands] {}, dispFloor { -50.0f };
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
        tap.process (io.getArrayOfWritePointers(), 2, c.n, c.b (ALLOCON), &c);
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
