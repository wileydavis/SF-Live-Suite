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
// Mix assigner. Sources are put in order of importance (drag in the list, top = most important):
// one global order, and any band can have its own order instead (click the band, drag; LINK
// returns it to the global order). Each of the 10 bands has a priority curve over its order:
//   PRIORITY - where the curve's peak sits (top .. bottom of the order)
//   DEPTH    - how hard sources away from the peak are cut (0 = nothing cut)
//   Q        - width of the peak (wide = neighbours share, narrow = one source owns the band)
// MODE DYNAMIC cuts a source only while a more favoured source is playing in that band;
// STATIC cuts all the time (a fixed EQ). Pass-through audio: put it on the master.
class AllocModule : public Module, private juce::Timer
{
public:
    enum { ENABLE, MODE, MAXCUT, SENS, PRI0 = 4, DEPTH0 = PRI0 + kBands, Q0 = DEPTH0 + kBands };
    AllocModule()
    {
        for (auto& band : rank) for (auto& r : band) r = -1;
        for (auto& l : linked) l = true;
        startTimerHz (20);
    }
    ~AllocModule() override { stopTimer(); }
    std::string id() const override { return "ALLOC"; }
    std::vector<ParamSpec> params() const override
    {
        std::vector<ParamSpec> p {
            toggle ("enable", "ENABLE", true).prim(),
            choice ("mode", "MODE", { "DYNAMIC", "STATIC" }, 0).prim(),
            knob ("max_cut", "MAX CUT", 0.0f, 24.0f, 12.0f, "dB").prim(),
            pct ("sensitivity", "SENSITIVITY", 0.5f).prim() };
        for (int b = 0; b < kBands; ++b) p.push_back (pct (("pri" + std::to_string (b + 1)).c_str(), ("PRIORITY " + std::to_string (b + 1)).c_str(), 0.0f).hide());
        for (int b = 0; b < kBands; ++b) p.push_back (pct (("depth" + std::to_string (b + 1)).c_str(), ("DEPTH " + std::to_string (b + 1)).c_str(), 0.0f).hide());
        for (int b = 0; b < kBands; ++b) p.push_back (pct (("q" + std::to_string (b + 1)).c_str(), ("Q " + std::to_string (b + 1)).c_str(), 0.5f).hide());
        return p;
    }
    void prepare (double rate, int) override { sr = rate; }

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
        bool alive[kSources];
        for (int s = 0; s < kSources; ++s) alive[s] = Bus::get().sourceAlive (s) && d.src[s].enabled.load() != 0;
        for (int s = 0; s < kSources; ++s) for (int b = 0; b < kBands; ++b) cutOut[s][b] = 0.0f;
        for (int b = 0; b < kBands; ++b)
        {
            // This band's order (ranks come from the message thread); unplaced sources after it, by role.
            int order[kSources], n = 0, slotAt[kSources];
            for (auto& x : slotAt) x = -1;
            for (int s = 0; s < kSources; ++s) { const int rk = rank[b][s].load(); if (alive[s] && rk >= 0 && rk < kSources) slotAt[rk] = s; }
            for (int r = 0; r < kSources; ++r) if (slotAt[r] >= 0) order[n++] = slotAt[r];
            for (int role = 0; role < kNumRoles; ++role)
                for (int s = 0; s < kSources; ++s)
                    if (alive[s] && rank[b][s].load() < 0 && d.src[s].role.load() == role && n < kSources) order[n++] = s;
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

    // ---- state: global order + per-band orders (by name)
    void save (juce::ValueTree& t) const override
    {
        auto join = [] (const std::vector<std::string>& v) { juce::String s; for (auto& nm : v) s << nm << "\n"; return s; };
        t.setProperty ("order", join (globalOrder), nullptr);
        for (int b = 0; b < kBands; ++b)
        {
            t.setProperty ("linked" + juce::String (b), linked[b], nullptr);
            t.setProperty ("order" + juce::String (b), join (bandOrder[b]), nullptr);
        }
    }
    void load (const juce::ValueTree& t) override
    {
        auto split = [] (const juce::var& v) { std::vector<std::string> out; for (auto& l : juce::StringArray::fromLines (v.toString())) if (l.isNotEmpty()) out.push_back (l.toStdString()); return out; };
        globalOrder = split (t.getProperty ("order"));
        for (int b = 0; b < kBands; ++b)
        {
            linked[b] = (bool) t.getProperty ("linked" + juce::String (b), true);
            bandOrder[b] = split (t.getProperty ("order" + juce::String (b)));
        }
        updateRanks();
    }

    // ---- display
    int displayHeight() const override { return 470; }
    void paint (juce::Graphics& g, juce::Rectangle<float> r) override
    {
        area = r;
        const float rh = rowH();
        // List: global order, or the selected band's order.
        const auto rows = visibleRows (selBand);
        auto list = listArea();
        const juce::String title = selBand < 0 ? "ORDER: ALL BANDS" : "ORDER: " + juce::String (bandName (selBand)) + " HZ" + (linked[selBand] ? " (ALL)" : " (OWN)");
        ui::frame (g, r.withWidth (list.getWidth() + 8), title);
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
        // Buttons under the list.
        auto allB = allButton(), linkB = linkButton();
        g.setColour (selBand < 0 ? ui::kText : ui::kLine); g.drawRect (allB, 1.0f);
        ui::label (g, "ALL BANDS", allB, selBand < 0 ? ui::kText : ui::kDim, 9.5f, juce::Justification::centred);
        if (selBand >= 0)
        {
            g.setColour (ui::kLine); g.drawRect (linkB, 1.0f);
            ui::label (g, linked[selBand] ? "LINKED" : "LINK TO ALL", linkB, linked[selBand] ? ui::kFaint : ui::kText, 9.5f, juce::Justification::centred);
        }
        ui::label (g, selBand < 0 ? "CLICK A BAND: OWN ORDER" : "DRAG: OWN ORDER",
                   juce::Rectangle<float> (list.getX(), allB.getBottom() + 4.0f, list.getWidth(), 24.0f), ui::kFaint, 8.0f);

        for (int b = 0; b < kBands; ++b)
        {
            auto col = bandArea (b);
            const auto bandRows = visibleRows (b);
            const int n = (int) bandRows.size();
            // Header: click to select. White frame = selected; "OWN" = own order.
            g.setColour (b == selBand ? ui::kText : ui::kLine);
            g.drawRect (col, b == selBand ? 1.5f : 1.0f);
            ui::label (g, bandName (b), col.reduced (5.0f, 2.0f).removeFromTop (14.0f), b == selBand ? ui::kText : ui::kFaint, 10.0f);
            if (! linked[b]) ui::label (g, "OWN", col.reduced (5.0f, 2.0f).removeFromTop (14.0f), ui::kText, 8.5f, juce::Justification::centredRight);
            auto lanes = col.reduced (4.0f, 0.0f).withTop (list.getY()).withHeight (rh * (float) std::max (1, n));
            float peak, sigma;
            curve (getParam ? getParam (PRI0 + b) : 0.0f, getParam ? getParam (Q0 + b) : 0.5f, n, peak, sigma);
            const float depth = getParam ? getParam (DEPTH0 + b) : 0.0f;
            for (int i = 0; i < n; ++i)
            {
                const int s = bandRows[(size_t) i].slot;
                auto lane = juce::Rectangle<float> (lanes.getX(), lanes.getY() + rh * (float) i, lanes.getWidth(), rh).reduced (0.0f, 3.0f);
                g.setColour (ui::kGrid); g.fillRect (lane);
                const float pre = clamp01 ((dispE[s][b].load() - dispFloor.load()) / 50.0f);
                const float post = clamp01 ((dispE[s][b].load() - dispCut[s][b].load() - dispFloor.load()) / 50.0f);
                g.setColour (ui::kAmber); g.fillRect (lane.withWidth (lane.getWidth() * pre));
                g.setColour (ui::kTrack.brighter (0.9f)); g.fillRect (lane.withWidth (lane.getWidth() * post));
                ui::label (g, shortName (bandRows[(size_t) i].label), lane.withTrimmedLeft (3.0f), post > 0.35f ? ui::kBg : ui::kDim, 8.0f);
            }
            if (n > 0)
            {
                juce::Path p;
                const int steps = std::max (2, n * 8);
                for (int k = 0; k <= steps; ++k)
                {
                    const float rowPos = (float) k / (float) steps * (float) n - 0.5f;
                    const float dd = (rowPos - peak) / sigma;
                    const float wgt = 1.0f - depth * (1.0f - std::exp (-0.5f * dd * dd));
                    const float x = lanes.getX() + lanes.getWidth() * (0.15f + 0.85f * wgt);
                    const float y = lanes.getY() + rh * (rowPos + 0.5f);
                    if (k == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
                }
                const bool touched = dragParam == PRI0 + b || dragParam == DEPTH0 + b || dragParam == Q0 + b;
                g.setColour (touched ? ui::kAccent : depth > 0.001f ? ui::kText : ui::kFaint);
                g.strokePath (p, juce::PathStrokeType (touched ? 2.0f : 1.4f));
                if (depth > 0.001f)
                {
                    const int owner = juce::jlimit (0, n - 1, (int) std::lround (peak));
                    ui::label (g, shortName (bandRows[(size_t) owner].label), juce::Rectangle<float> (col.getX() + 4.0f, col.getBottom() - 134.0f, col.getWidth() - 8.0f, 14.0f), ui::kText, 9.0f);
                }
            }
            for (int k = 0; k < 3; ++k)
            {
                const int idx = (k == 0 ? PRI0 : k == 1 ? DEPTH0 : Q0) + b;
                const float v = getParam ? getParam (idx) : 0.0f;
                drawKnob (g, knobRect (b, k), v, k == 0 ? "PRI" : k == 1 ? "DEPTH" : "Q", dragParam == idx);
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
            {
                for (int k = 0; k < 3; ++k)
                    if (knobRect (b, k).contains (p)) { dragParam = (k == 0 ? PRI0 : k == 1 ? DEPTH0 : Q0) + b; startVal = getParam ? getParam (dragParam) : 0.0f; startY = p.y; return; }
                if (bandArea (b).withHeight (18.0f).contains (p) || (bandArea (b).contains (p) && p.y < knobRect (b, 0).getY() - 20.0f))
                { selBand = selBand == b ? -1 : b; return; }
            }
            if (allButton().contains (p)) { selBand = -1; return; }
            if (selBand >= 0 && linkButton().contains (p)) { linked[selBand] = true; bandOrder[selBand].clear(); updateRanks(); return; }
            const auto rows = visibleRows (selBand);
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
            const auto rows = visibleRows (selBand);
            const int target = juce::jlimit (0, std::max (0, (int) rows.size() - 1), (int) ((p.y - listArea().getY()) / rowH()));
            if (target != dragRow)
            {
                auto& ord = orderFor (selBand, true);
                // Make sure every visible row is in the stored order, then move the dragged name.
                for (auto& row : rows) if (std::find (ord.begin(), ord.end(), row.label) == ord.end()) ord.push_back (row.label);
                const std::string moving = rows[(size_t) dragRow].label, anchor = rows[(size_t) target].label;
                ord.erase (std::remove (ord.begin(), ord.end(), moving), ord.end());
                auto it = std::find (ord.begin(), ord.end(), anchor);
                if (target > dragRow && it != ord.end()) ++it;
                ord.insert (it, moving);
                dragRow = target;
                updateRanks();
            }
        }
    }
    void displayMouseUp() override { dragParam = -1; dragRow = -1; }

private:
    struct Row { std::string label; int slot; };

    static const char* bandName (int b) { static const char* n[kBands] = { "40", "80", "120", "180", "270", "390", "600", "1K", "2.5K", "9K" }; return n[b]; }
    static juce::String shortName (const std::string& s) { juce::String n (s); if (n.startsWith ("TAP ")) n = n.substring (4); if (n.startsWith ("STRIP ")) n = n.substring (6); return n.substring (0, 9); }

    // Editing a band's order unlinks it, starting from the global order.
    std::vector<std::string>& orderFor (int band, bool forEdit)
    {
        if (band < 0) return globalOrder;
        if (forEdit && linked[band]) { linked[band] = false; bandOrder[band] = globalOrder; }
        return linked[band] ? globalOrder : bandOrder[band];
    }
    const std::vector<std::string>& orderView (int band) const { return band < 0 || linked[band] ? globalOrder : bandOrder[band]; }

    juce::Rectangle<float> listArea() const { return { area.getX() + 6.0f, area.getY() + 20.0f, 150.0f, area.getHeight() - 200.0f }; }
    juce::Rectangle<float> allButton() const { auto l = listArea(); return { l.getX(), l.getBottom() + 8.0f, l.getWidth() * 0.48f, 22.0f }; }
    juce::Rectangle<float> linkButton() const { auto l = listArea(); return { l.getX() + l.getWidth() * 0.52f, l.getBottom() + 8.0f, l.getWidth() * 0.48f, 22.0f }; }
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

    // Alive sources with unique labels (duplicates get " 2", " 3"), by role.
    std::vector<Row> aliveRows() const
    {
        std::vector<Row> out;
        auto& d = Bus::get().d();
        std::vector<std::pair<int, int>> byRole;
        for (int s = 0; s < kSources; ++s) if (Bus::get().sourceAlive (s)) byRole.push_back ({ d.src[s].role.load(), s });
        std::sort (byRole.begin(), byRole.end());
        std::map<std::string, int> seen;
        for (auto& rs : byRole)
        {
            char nm[24]; Bus::unpackName (d.src[rs.second].name, nm);
            std::string label = nm;
            const int k = ++seen[label];
            if (k > 1) label += " " + std::to_string (k);
            out.push_back ({ label, rs.second });
        }
        return out;
    }
    // Rows in a band's order (-1 = global); sources the order doesn't name yet come last.
    std::vector<Row> visibleRows (int band) const
    {
        const auto alive = aliveRows();
        const auto& ord = orderView (band);
        std::vector<Row> rows;
        for (auto& nm : ord) for (auto& a : alive) if (a.label == nm) rows.push_back (a);
        for (auto& a : alive) if (std::find (ord.begin(), ord.end(), a.label) == ord.end()) rows.push_back (a);
        return rows;
    }
    void updateRanks()
    {
        for (int b = 0; b < kBands; ++b)
        {
            const auto rows = visibleRows (b);
            int r[kSources]; for (auto& x : r) x = -1;
            for (size_t i = 0; i < rows.size(); ++i) r[rows[i].slot] = (int) i;
            for (int s = 0; s < kSources; ++s) rank[b][s] = r[s];
        }
    }
    void timerCallback() override
    {
        // Newcomers join the end of every order, so lists only change when you drag them.
        for (auto& row : aliveRows())
        {
            if (std::find (globalOrder.begin(), globalOrder.end(), row.label) == globalOrder.end()) globalOrder.push_back (row.label);
            for (int b = 0; b < kBands; ++b)
                if (! linked[b] && std::find (bandOrder[b].begin(), bandOrder[b].end(), row.label) == bandOrder[b].end()) bandOrder[b].push_back (row.label);
        }
        updateRanks();
    }

    double sr = 48000.0;
    std::vector<std::string> globalOrder, bandOrder[kBands];
    bool linked[kBands];
    std::atomic<int> rank[kBands][kSources];
    float cutOut[kSources][kBands] {};
    juce::Rectangle<float> area;
    int dragParam = -1, dragRow = -1, selBand = -1;
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
