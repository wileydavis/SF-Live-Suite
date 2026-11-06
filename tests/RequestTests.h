// Regressions for the Nov 6 audit of 93d84a4 (included by SuiteTests.cpp, after QualityTests.h).
//   --requests   B01 B03: a take's request ends with PANIC, a reset or prepare at every stage
//   --authority2 B02:     a state loaded is newer than a recall, its report, a song waiting, a STORE
//   --tempo      B04:     TERROIR's first input at the host's tempo and meter, not the state's
//   --reach      B05:     every strip's controls within reach at any window height

// A take's request is one thing from the ask to the file (B01, B03). CONTROL's PANIC (held, or a
// tap between two of SF CAPTURE's blocks), the module reset (PANIC's own, a fault's) and prepare
// (the same rate, a new one) end a request made before them at every stage: waiting for a
// recorder, being made, made but not started, playing, being written, written but not yet Done.
// Such a take plays nothing (playing: nothing after PANIC's fade), leaves no file, is never Done
// (so never taken in), and a request asked afterwards plays.
static void testCaptureRequests()
{
    std::printf ("-- A take's request ends with PANIC, a reset or prepare, from the ask to the file (Nov 6 audit B01, B03)\n");
    const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("sf_capture_requests");
    auto& d = Bus::get().d();
    auto cs = [&] { return capture::status (d.capStatus); };
    auto files = [&] { int n = 0; for (const auto& e : juce::RangedDirectoryIterator (dir, false, "*.wav")) { juce::ignoreUnused (e); ++n; } return n; };
    const juce::Point<float> captureX (73.0f, 47.0f);
    auto slotX = [] (int i) { const float w = (280.0f - 16.0f - 21.0f) / 8.0f; return juce::Point<float> (8.0f + (float) i * (w + 3.0f) + w * 0.5f, 25.0f); };
    struct World
    {
        std::unique_ptr<Rig2> ctl, cap, terr;
        float peak = 0.0f;                      // SF CAPTURE's output since it was last zeroed
        bool sawDone = false;
        World() : ctl (std::make_unique<Rig2> ("CONTROL")), cap (std::make_unique<Rig2> ("CAPTURE")), terr (std::make_unique<Rig2> ("TERROIR"))
        {
            for (auto* r : { ctl.get(), cap.get(), terr.get() }) r->head.playing = false;
        }
        void click (juce::Point<float> at)
        {
            const juce::Rectangle<float> r (0.0f, 0.0f, 280.0f, 184.0f);
            terr->p->mod().displayMouse (at, r, true); terr->p->mod().displayMouseUp();
        }
        void step()
        {
            ctl->step (kBlock, nullptr);
            cap->step (kBlock, nullptr);
            peak = std::max (peak, cap->buf.getMagnitude (0, 0, kBlock));
            terr->step (kBlock, nullptr);
            if (capture::status (Bus::get().d().capStatus).state == capture::Done) sawDone = true;
        }
    };
    auto until = [&] (World& w, const std::function<bool()>& f, double sec, bool audio = true)
    {
        for (const double end = juce::Time::getMillisecondCounterHiRes() + sec * 1000.0; juce::Time::getMillisecondCounterHiRes() < end;)
        {
            if (f()) return true;
            if (audio) w.step();
            juce::Thread::sleep (1);
        }
        return f();
    };
    auto settle = [&] (World& w, int blocks) { for (int b = 0; b < blocks; ++b) { w.step(); if (b % 8 == 0) juce::Thread::sleep (2); } };
    auto ask = [&] (World& w, int slot) { w.click (captureX); w.click (slotX (slot)); return d.capSeq.load(); };

    enum Cmd { PanicHeld, PanicTap, Reset, PrepareSame, PrepareNew, kCmds };
    const char* cmdName[] = { "PANIC held, released", "PANIC tapped", "reset", "prepare 48k", "prepare 96k" };
    auto command = [&] (World& w, int c)
    {
        switch (c)
        {
            case PanicHeld:
                w.ctl->set ("panic", 1.0f);
                for (int k = 0; k < 10; ++k) w.step();
                w.ctl->set ("panic", 0.0f);
                for (int k = 0; k < 10; ++k) w.step();
                break;
            case PanicTap:                       // pressed and let go between two of SF CAPTURE's blocks
                w.ctl->set ("panic", 1.0f); w.ctl->step (kBlock, nullptr);
                w.ctl->set ("panic", 0.0f); w.ctl->step (kBlock, nullptr);
                break;
            case Reset: w.cap->p->mod().reset(); break;
            case PrepareSame: w.cap->p->prepareToPlay (kSr, kBlock); break;
            default: w.cap->p->prepareToPlay (96000.0, kBlock); break;
        }
    };
    enum Stage { Waiting, Making, Made, Playing, Writing, Written, kStages };
    const char* stageName[] = { "waiting for a recorder", "being made", "made, not started", "playing", "being written", "written, not Done" };
    for (int st = 0; st < kStages; ++st)
    {
        juce::String row;
        for (int c = 0; c < kCmds; ++c)
        {
            resetBus();
            dir.deleteRecursively(); dir.createDirectory();
            capture::testFolder = dir.getFullPathName().toStdString();
            capture::testShortTake = true;
            World w;
            settle (w, 4);
            capture::testHold = st == Waiting ? capture::kHoldClaim : st == Making ? capture::kHoldPublish
                              : st == Writing ? capture::kHoldWrite : st == Written ? capture::kHoldDone : 0;
            const uint32_t seq = ask (w, st);
            w.peak = 0.0f;
            bool reached = true;
            auto is = [&] (int state) { const auto s = cs(); return s.seq == seq && s.state == state; };
            if (st == Waiting) { juce::Thread::sleep (50); reached = is (capture::Idle); }   // the worker waits before its claim
            else if (st == Making) reached = until (w, [&] { return is (capture::Preparing); }, 3.0, false);
            else if (st == Made) { reached = until (w, [&] { return is (capture::Preparing); }, 3.0, false); juce::Thread::sleep (300); }   // handed over; no audio since
            else if (st == Playing) reached = until (w, [&] { return is (capture::Playing); }, 5.0);
            else reached = until (w, [&] { return is (capture::Writing) && (st != Written || files() == 1); }, 5.0);
            if (st >= Writing) w.peak = 0.0f;                                        // (it played, as it should, before)
            command (w, c);
            if (st == Playing) { settle (w, 3); w.peak = 0.0f; }                       // PANIC's 5 ms fade, then nothing
            capture::testHold = 0;
            settle (w, 150);
            const auto e = cs();
            const bool ended = e.seq == seq && e.state == capture::Failed && e.error == capture::Cancelled;
            const float out = w.peak;
            const int left = files();
            const bool done = w.sawDone;
            // A new request, asked now, plays (then STOP).
            const uint32_t seq2 = ask (w, (st + 1) % 8);
            const bool again = seq2 > seq && until (w, [&] { const auto s = cs(); return s.seq == seq2 && s.state == capture::Playing; }, 5.0);
            w.click (captureX);
            settle (w, 20);
            row << "  " << cmdName[c] << (reached && ended && out == 0.0f && left == 0 && ! done && again ? ": ended" : ": NOT");
            CHECK (reached && ended && out == 0.0f && left == 0 && ! done && again,
                   "request %s, %s: %s; state %d error %d, out %.2g after, files %d, Done %d, a new request plays %d",
                   stageName[st], cmdName[c], reached ? "reached" : "NOT REACHED", e.state, e.error, out, left, done ? 1 : 0, again ? 1 : 0);
        }
        std::printf ("   %-24s%s\n", stageName[st], row.toRawUTF8());
    }
    capture::testHold = 0;
    capture::testShortTake = false;
    capture::testFolder.clear();
    dir.deleteRecursively();
    resetBus();
}

// A state loaded is newer than everything asked for before it, whatever its values (B02): the
// recall in force and its report to Live, a song still waiting for its bar line, a STORE not yet
// made. SF FILTER's LEVEL: song 1 holds -6 dB; the state is saved at LEVEL t, then LEVEL goes to
// 0 and song 1 is recalled (-6 in force, Live not told yet). t is the value Live held before the
// recall (0), the recall's (-6) or another (+3); Live is told before the load, between the load
// and the next block, or after it; prepared again or not. What the module hears at once and
// settled, Live's value and the next save are t. Then a recall after the load, a recall gliding
// (SLOW) when the state comes in, a move after the load, songs and STOREs asked before and after.
static void testStateAuthority()
{
    std::printf ("-- A state loaded is newer than a recall, its report, a song waiting and a STORE not yet made (Nov 6 audit B02)\n");
    auto& d = Bus::get().d();
    auto level = [] (Rig2& r) { return r.p->currentValue (r.p->paramIndex ("level")); };
    auto raw = [] (Rig2& r) { return r.p->apvts.getRawParameterValue ("level")->load(); };
    auto saved = [] (Rig2& r, int song = -1)
    {
        juce::MemoryBlock m;
        r.p->getStateInformation (m);
        const auto t = juce::ValueTree::readFromData (m.getData(), m.getSize());
        if (song < 0)
        {
            for (auto row : t.getChildWithName ("PARAMS")) if (row.getProperty ("id").toString() == "level") return (float) row.getProperty ("value");
            return (float) NAN;
        }
        for (auto s : t.getChildWithName ("SONGS")) if ((int) s.getProperty ("index") == song) return (float) s.getProperty ("level", (double) NAN);
        return (float) NAN;
    };
    auto blocks = [] (Rig2& r, int n)
    {
        for (int k = 0; k < n; ++k)
            r.step (kBlock, [] (juce::AudioBuffer<float>& b, juce::MidiBuffer&, int m) { for (int c = 0; c < b.getNumChannels(); ++c) for (int i = 0; i < m; ++i) b.setSample (c, i, 0.1f); });
    };
    auto near = [] (float a, float b) { return std::abs (a - b) < 0.01f; };
    auto recall = [&] (int s) { d.song = s; d.songAtPpq = -1.0; d.songFromPpq = -1.0; d.songKey = -2; d.songSeq.fetch_add (1); };
    auto load = [] (Rig2& r, const juce::MemoryBlock& m) { r.p->setStateInformation (m.getData(), (int) m.getSize()); };
    // Song 1 at -6, the state saved at t, then LEVEL 0 and song 1 recalled: -6 in force, unreported.
    auto setup = [&] (Rig2& r, float t, juce::MemoryBlock& st)
    {
        r.head.playing = false;
        d.song = 1;
        r.set ("level", -6.0f); blocks (r, 2); r.p->storeSong(); blocks (r, 2);
        r.set ("level", t); blocks (r, 120);
        r.p->getStateInformation (st);
        r.set ("level", 0.0f); blocks (r, 120);
        recall (1); blocks (r, 48);
    };

    const float targets[] = { 0.0f, -6.0f, 3.0f };
    const char* when[] = { "told before the load", "told between the load and the next block", "told after the next block" };
    for (int ti = 0; ti < 3; ++ti)
    {
        const float t = targets[ti];
        int ok = 0, n = 0;
        for (int report = 0; report < 3; ++report)
            for (const bool again : { false, true })
            {
                resetBus();
                Rig2 r ("FILTER");
                juce::MemoryBlock st;
                setup (r, t, st);
                const float before = level (r);
                if (report == 0) r.p->flushHostNotifications();
                load (r, st);
                if (again) r.p->prepareToPlay (kSr, kBlock);
                if (report == 1) r.p->flushHostNotifications();
                blocks (r, 1);
                const float first = level (r);
                r.p->flushHostNotifications();
                blocks (r, 120);
                const float settled = level (r), host = raw (r), next = saved (r);
                const bool good = near (before, -6.0f) && near (first, t) && near (settled, t) && near (host, t) && near (next, t);
                ok += good ? 1 : 0; ++n;
                CHECK (good, "state at LEVEL %+.0f over a recall of -6, %s%s: before %.2f, first %.2f, settled %.2f, Live %.2f, next save %.2f",
                       t, when[report], again ? ", prepared again" : "", before, first, settled, host, next);
            }
        std::printf ("   state at LEVEL %+.0f dB loaded over a recall of -6 (from 0) not yet told to Live: %d of %d heard, held by Live and saved as the state's\n", t, ok, n);
    }

    // A recall after the load: in force, told, saved. With SLOW (1 BAR): a recall gliding when the
    // state comes in gives way at once (the first block after a load glides nothing); one after it
    // glides, and a save meanwhile keeps its target (A01).
    {
        resetBus();
        Rig2 r ("FILTER");
        juce::MemoryBlock st;
        setup (r, 0.0f, st);
        load (r, st); blocks (r, 2);
        recall (1); blocks (r, 120);
        const float inForce = level (r);
        r.p->flushHostNotifications(); blocks (r, 4);
        std::printf ("   a recall after the load: heard %.2f, Live %.2f, saved %.2f\n", inForce, raw (r), saved (r));
        CHECK (near (inForce, -6.0f) && near (raw (r), -6.0f) && near (saved (r), -6.0f), "a recall after a state load: heard %.2f, Live %.2f, saved %.2f", inForce, raw (r), saved (r));
    }
    {
        resetBus();
        Rig2 r ("FILTER");
        r.head.bpm = 120.0;
        r.set ("slow", 1.0f);                                        // 1 BAR: 2 s
        juce::MemoryBlock st;
        setup (r, 3.0f, st);
        const float gliding = level (r);
        load (r, st);
        blocks (r, 1);
        const float first = level (r);
        r.p->flushHostNotifications(); blocks (r, 60);
        const float settled = level (r), host = raw (r), next = saved (r);
        recall (1); blocks (r, 90);                                  // 0.48 s of 2
        const float midway = level (r), target = saved (r);
        std::printf ("   SLOW 1 BAR: a recall gliding (%.2f) when a state at +3 comes in: first %.2f, settled %.2f, Live %.2f, saved %.2f; a recall after it glides (%.2f), saved %.2f\n",
                     gliding, first, settled, host, next, midway, target);
        CHECK (gliding < -0.1f && gliding > -5.9f && near (first, 3.0f) && near (settled, 3.0f) && near (host, 3.0f) && near (next, 3.0f)
               && midway < 2.9f && midway > -5.9f && near (target, -6.0f),
               "SLOW and a state load: gliding %.2f, first %.2f, settled %.2f, Live %.2f, saved %.2f; then %.2f saved %.2f", gliding, first, settled, host, next, midway, target);
    }
    // A move after the load: the old report never undoes it.
    {
        resetBus();
        Rig2 r ("FILTER");
        juce::MemoryBlock st;
        setup (r, 0.0f, st);
        load (r, st);
        r.set ("level", 1.5f);
        r.p->flushHostNotifications();
        blocks (r, 120);
        std::printf ("   LEVEL moved to +1.5 after the load, the old report still due: heard %.2f, Live %.2f, saved %.2f\n", level (r), raw (r), saved (r));
        CHECK (near (level (r), 1.5f) && near (raw (r), 1.5f) && near (saved (r), 1.5f), "a move after a state load: heard %.2f, Live %.2f, saved %.2f", level (r), raw (r), saved (r));
    }
    // A song waiting for its bar line (the transport running) when the state comes in: over. One
    // asked after it lands on its bar line.
    {
        resetBus();
        Rig2 r ("FILTER");
        r.head.bpm = 120.0; r.head.playing = false;
        d.song = 1;
        r.set ("level", -6.0f); blocks (r, 2); r.p->storeSong(); blocks (r, 2);
        r.set ("level", 0.0f); blocks (r, 60);
        juce::MemoryBlock st;
        r.p->getStateInformation (st);
        r.head.playing = true; r.ppq = 0.5;
        recall (1); blocks (r, 2);
        const bool waited = r.p->recallWaiting();
        load (r, st);
        blocks (r, (int) (2.5 * 2.0 * kSr / kBlock));                // past two bar lines
        const float after = level (r);
        const bool over = ! r.p->recallWaiting();
        recall (1); blocks (r, (int) (2.5 * 2.0 * kSr / kBlock));
        const float later = level (r);
        std::printf ("   a song waiting for its bar line when a state comes in: %s; two bars on, heard %.2f; a song asked after it: heard %.2f\n",
                     waited && over ? "over" : "STILL WAITING", after, later);
        CHECK (waited && over && near (after, 0.0f) && near (later, -6.0f), "a song waiting across a state load: waited %d, over %d, heard %.2f, then %.2f", waited ? 1 : 0, over ? 1 : 0, after, later);
    }
    // A STORE pressed (this plugin's, or SF CONTROL's STORE ALL) and the state loaded before the
    // next block: not made (the song keeps the state's value). One after the load stores the state's sound.
    {
        resetBus();
        Rig2 r ("FILTER");
        r.head.playing = false;
        d.song = 2;
        r.set ("level", -3.0f); blocks (r, 2); r.p->storeSong(); blocks (r, 2);
        r.set ("level", 0.0f); blocks (r, 4);
        juce::MemoryBlock st;
        r.p->getStateInformation (st);                               // LEVEL 0, song 2 at -3
        r.set ("level", 2.0f); blocks (r, 4);
        r.p->storeSong();
        load (r, st); blocks (r, 4);
        const float keep = saved (r, 2);
        r.p->storeSong(); blocks (r, 4);
        const float after = saved (r, 2);
        load (r, st); blocks (r, 2);
        r.set ("level", 2.0f); blocks (r, 4);
        d.storeSong = -1; d.storeSeq.fetch_add (1);
        load (r, st); blocks (r, 4);
        const float keepBus = saved (r, 2);
        d.storeSeq.fetch_add (1); blocks (r, 4);
        const float afterBus = saved (r, 2);
        std::printf ("   STORE then a state load before the next block: song 2 keeps %.2f (STORE ALL: %.2f); a STORE after it: %.2f (STORE ALL: %.2f)\n", keep, keepBus, after, afterBus);
        CHECK (near (keep, -3.0f) && near (after, 0.0f) && near (keepBus, -3.0f) && near (afterBus, 0.0f), "STOREs across a state load: %.2f %.2f %.2f %.2f", keep, after, keepBus, afterBus);
    }
    // Songs written by three at once: STOREs on the audio thread, states loaded and saved on two
    // others. A song is only ever whole: a state's or a STORE's, never part of each.
    {
        resetBus();
        Rig2 r ("FILTER");
        r.head.playing = false;
        d.song = 3;
        auto stateWith = [&] (float v)
        {
            r.set ("level", v); r.set ("res", 0.5f + 0.05f * v); r.set ("drive", 0.5f + 0.04f * v); blocks (r, 2);
            r.p->storeSong(); blocks (r, 2);
            juce::MemoryBlock m; r.p->getStateInformation (m); return m;
        };
        const auto A = stateWith (-4.0f), B = stateWith (4.0f);
        std::atomic<bool> stop { false };
        std::atomic<int> torn { 0 }, saves { 0 };
        std::thread loader ([&] { for (int i = 0; ! stop.load(); ++i) { load (r, i % 2 ? A : B); juce::Thread::sleep (1); } });
        std::thread saver ([&]
        {
            while (! stop.load())
            {
                juce::MemoryBlock m;
                r.p->getStateInformation (m);
                const auto t = juce::ValueTree::readFromData (m.getData(), m.getSize());
                for (auto s : t.getChildWithName ("SONGS"))
                    if ((int) s.getProperty ("index") == 3)
                    {
                        const float lv = (float) s.getProperty ("level"), rs = (float) s.getProperty ("res"), dr = (float) s.getProperty ("drive");
                        if (std::abs (rs - (0.5f + 0.05f * lv)) > 1e-4f || std::abs (dr - (0.5f + 0.04f * lv)) > 1e-4f) ++torn;
                    }
                ++saves;
            }
        });
        for (int k = 0; k < 600; ++k) { r.p->storeSong(); blocks (r, 1); }
        stop = true;
        loader.join(); saver.join();
        std::printf ("   STOREs every block, states loaded and saved meanwhile on two threads: %d saves, %d songs part one and part another\n", saves.load(), torn.load());
        CHECK (torn.load() == 0, "songs torn between writers: %d", torn.load());
    }
    resetBus();
}

// TERROIR's first input meets the room at the host's tempo and meter, not the one a state was
// saved at (B04): LENGTH's notes are another length then. Loaded before prepare or after it, or
// into a plugin that has played (silence) at the host's tempo; the worker as it is or at 400 ms an
// IR. Against the room rendered at the host's tempo (built in the block), the first hit and a
// settled second one.
static void testTerroirTempo()
{
    std::printf ("-- TERROIR's first input at the host's tempo and meter, whatever a state saved (Nov 6 audit B04)\n");
    auto run = [] (Rig2& r, double sec, bool hit)
    {
        std::vector<float> y;
        const int64_t n = (int64_t) std::llround (sec * r.sr);
        for (int64_t t = 0; t < n;)
        {
            const int nn = (int) std::min<int64_t> (r.block, n - t);
            r.step (nn, [&] (juce::AudioBuffer<float>& b, juce::MidiBuffer&, int m)
            {
                for (int i = 0; i < m; ++i) { const float x = hit && t + i == 0 ? 0.2f : 0.0f; b.setSample (0, i, x); b.setSample (1, i, x); }
            });
            for (int i = 0; i < nn; ++i) y.push_back (r.buf.getSample (0, i));
            t += nn;
        }
        return y;
    };
    auto residualDb = [] (const std::vector<float>& a, const std::vector<float>& ref)
    {
        double e = 0.0, r = 0.0;
        for (size_t i = 0; i < std::min (a.size(), ref.size()); ++i) { const double dd = (double) a[i] - ref[i]; e += dd * dd; r += (double) ref[i] * ref[i]; }
        return (float) (10.0 * std::log10 ((e + 1e-30) / (r + 1e-30)));
    };
    auto load = [] (Rig2& r, const juce::MemoryBlock& m) { r.p->setStateInformation (m.getData(), (int) m.getSize()); };
    struct Cfg { const char* name; float length; bool rev; double savedBpm; int savedBar; double hostBpm; int hostBar; };
    const std::vector<Cfg> cfgs {
        { "LENGTH 1/16, saved at 120, played at 240", 7.0f, false, 120.0, 4, 240.0, 4 },
        { "LENGTH 1/16, saved at 240, played at 120", 7.0f, false, 240.0, 4, 120.0, 4 },
        { "LENGTH 1/4 REVERSE, saved at 120, played at 90", 5.0f, true, 120.0, 4, 90.0, 4 },
        { "LENGTH 1 BAR at 240, saved in 4/4, played in 3/4", 3.0f, false, 240.0, 4, 240.0, 3 },   // 1 s, 0.75 s (the room is 1.8)
        { "LENGTH 1/8 at 132, as saved", 6.0f, false, 132.0, 4, 132.0, 4 },
    };
    const char* hows[] = { "loaded, prepared again", "prepared, then loaded", "loaded into one that has played", "loaded into one that has played, prepared again" };
    for (const auto& cfg : cfgs)
    {
        // The state: wet only, DECAY 100 %, LENGTH (and REVERSE), saved where it last played.
        juce::MemoryBlock state;
        {
            resetBus();
            Rig2 r ("TERROIR");
            r.head.bpm = cfg.savedBpm; r.head.beatsPerBar = cfg.savedBar;
            r.p->setNonRealtime (true);
            r.set ("mix", 1.0f); r.set ("dry", 0.0f); r.set ("duck", 0.0f); r.set ("lo_cut", 20.0f);
            r.set ("decay", 1.0f); r.set ("length", cfg.length); r.set ("reverse", cfg.rev ? 1.0f : 0.0f);
            run (r, 0.1, false);
            r.p->getStateInformation (state);
        }
        std::vector<float> want;
        {
            resetBus();
            Rig2 r ("TERROIR");
            r.head.bpm = cfg.hostBpm; r.head.beatsPerBar = cfg.hostBar;
            r.p->setNonRealtime (true);
            load (r, state);
            want = run (r, 1.0, true);
        }
        float worst = -400.0f, worstSettled = -400.0f;
        for (const int slow : { 0, 400 })
            for (int how = 0; how < 4; ++how)
            {
                resetBus();
                Rig2 r ("TERROIR");
                r.head.bpm = cfg.hostBpm; r.head.beatsPerBar = cfg.hostBar;
                ir::testBuildDelayMs = slow;
                if (how >= 2)                                        // played (silence) at the host's tempo, the room
                {                                                     // at its defaults, the rest as the state has it
                    r.set ("mix", 1.0f); r.set ("dry", 0.0f); r.set ("duck", 0.0f); r.set ("lo_cut", 20.0f);
                    run (r, 0.2, false);
                }
                load (r, state);
                if (how == 0 || how == 3) { r.p->prepareToPlay (r.sr, r.block); r.ppq = 0.0; }
                const auto first = run (r, 1.0, true);
                r.p->mod().reset();
                const auto settled = run (r, 1.0, true);
                ir::testBuildDelayMs = 0;
                const float e = residualDb (first, want), s = residualDb (settled, want);
                worst = std::max (worst, e); worstSettled = std::max (worstSettled, s);
                CHECK (e < -60.0f && s < -60.0f && r.p->faultCount.load() == 0, "TERROIR %s, %s, worker %s: first input %.1f dB off the host's room, settled %.1f dB (faults %d)",
                       cfg.name, hows[how], slow ? "at 400 ms an IR" : "as it is", e, s, r.p->faultCount.load());
            }
        std::printf ("   %-46s first input against the host's room, worst of 8: %.1f dB (settled %.1f dB)\n", cfg.name, worst, worstSettled);
    }
    resetBus();
}

// Every control of every strip within reach at any window height (B05). All groups open, the
// window held to 480, 640 and 800 pixels: it is no taller, and every control, the module's display
// and every group's title line can be scrolled whole into view. The wheel over a knob scrolls the
// strip (and turns nothing) while it scrolls, and turns the knob when it doesn't. A set saved with
// every group open reopens held to the screen, at the top. A group opened at the bottom scrolls
// into view. PANIC and THROW held while the window closes are let go.
static void testStripReach()
{
    std::printf ("-- Every strip's controls within reach at any window height (Nov 6 audit B05)\n");
    const int allOpen = SFEditor::kFoldMotion | SFEditor::kFoldSystem | SFEditor::kFoldSetup;
    std::function<void (juce::Component&, std::vector<juce::Component*>&)> deep = [&] (juce::Component& c, std::vector<juce::Component*>& out)
    {
        for (auto* k : c.getChildren()) { out.push_back (k); deep (*k, out); }
    };
    auto wheel = [] (juce::Component& c, float dy)
    {
        juce::MouseWheelDetails w;
        w.deltaX = 0.0f; w.deltaY = dy; w.isReversed = false; w.isSmooth = false; w.isInertial = false;
        const auto now = juce::Time::getCurrentTime();
        const juce::MouseEvent e (juce::Desktop::getInstance().getMainMouseSource(), { 5.0f, 5.0f }, {}, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, &c, &c, now, { 5.0f, 5.0f }, now, 0, false);
        c.mouseWheelMove (e, w);
    };
    int worstUnreached = 0, tallest = 0, modules = 0, wheels = 0;
    std::string tallestId;
    for (const int limit : { 480, 640, 800 })
        for (auto& id : allModuleIds())
        {
            SFProcessor p (makeModule (id));
            p.uiFold = allOpen;
            SFEditor::testHeightLimit = limit;
            std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
            auto& e = dynamic_cast<SFEditor&> (*ed);
            auto& v = e.scroller();
            if (limit == 800) { ++modules; if (SFEditor::kHeader + e.contentHeight() > tallest) { tallest = SFEditor::kHeader + e.contentHeight(); tallestId = id; } }
            CHECK (e.getHeight() <= limit && e.getWidth() == SFEditor::kW, "%s held to %d: %dx%d", id.c_str(), limit, e.getWidth(), e.getHeight());
            // Scrolled to, each lies whole in the view.
            int unreached = 0, items = 0;
            // (Up to the scroll bar: a title line runs the strip's width, its fold mark 12 px from the end.
            // Taller than the window, as ALLOC's display held to 480: its top, then its bottom.)
            auto reach = [&] (juce::Rectangle<int> r)
            {
                ++items;
                r = r.withRight (std::min (r.getRight(), SFEditor::kW - 6));
                const int vh = v.getViewHeight();
                v.setViewPosition (0, std::max (0, r.getY() - 2));
                bool whole = v.getViewArea().contains (r.withHeight (std::min (r.getHeight(), vh - 2)));
                v.setViewPosition (0, std::max (0, r.getBottom() - vh));
                whole = whole && v.getViewArea().contains (r.withTop (std::max (r.getY(), r.getBottom() - vh)));
                if (! whole) ++unreached;
            };
            for (auto* c : e.strip().getChildren()) if (c->isVisible()) reach (c->getBounds());
            for (auto& t : e.titleRows()) reach (t);
            if (! e.displayBounds().isEmpty()) reach (e.strip().getLocalArea (&e, e.displayBounds()));
            v.setViewPosition (0, 1 << 20);
            const bool bottom = v.getViewArea().getBottom() == e.contentHeight();
            worstUnreached = std::max (worstUnreached, unreached);
            CHECK (unreached == 0 && bottom, "%s held to %d: %d of %d out of reach, the bottom %s", id.c_str(), limit, unreached, items, bottom ? "reached" : "NOT reached");
            // The wheel: scrolls while the strip scrolls (no knob turned).
            if (SFEditor::kHeader + e.contentHeight() > e.getHeight())
            {
                std::vector<juce::Component*> all;
                deep (e.strip(), all);
                juce::Slider* knob = nullptr;
                for (auto* c : all)
                    if (auto* s = dynamic_cast<juce::Slider*> (c); s != nullptr && knob == nullptr && s->getParentComponent()->isVisible()) knob = s;
                CHECK (knob != nullptr, "%s held to %d: no knob for the wheel", id.c_str(), limit);
                if (knob != nullptr)
                {
                    ++wheels;
                    v.setViewPosition (0, 0);
                    const double was = knob->getValue();
                    wheel (*knob, -0.5f);
                    CHECK (v.getViewPositionY() > 0 && knob->getValue() == was, "%s held to %d: the wheel over a knob scrolls %d, turns %.3f to %.3f", id.c_str(), limit, v.getViewPositionY(), was, knob->getValue());
                }
            }
        }
    // ... and turns the knob when the strip fits.
    {
        SFProcessor p (makeModule ("LEAD"));
        SFEditor::testHeightLimit = 100000;
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        auto& e = dynamic_cast<SFEditor&> (*ed);
        std::vector<juce::Component*> all;
        deep (e.strip(), all);
        juce::Slider* knob = nullptr;
        for (auto* c : all) if (auto* s = dynamic_cast<juce::Slider*> (c); s != nullptr && knob == nullptr) knob = s;
        const double was = knob != nullptr ? knob->getValue() : 0.0;
        if (knob != nullptr) wheel (*knob, 0.5f);
        CHECK (knob != nullptr && knob->getValue() != was, "the wheel turns a knob when the strip fits");
    }
    std::printf ("   %d strips, every group open, held to 480 / 640 / 800 px: worst %d controls out of reach; the tallest, %s, %d px; the wheel scrolled %d of them, turning no knob\n",
                 modules, worstUnreached, tallestId.c_str(), tallest, wheels);

    // A set saved with every group open reopens held to the screen, at the top; a group opened at
    // the bottom scrolls into view.
    {
        SFEditor::testHeightLimit = 100000;
        SFProcessor p (makeModule ("PAD"));
        p.uiFold = allOpen;
        juce::MemoryBlock mb;
        p.getStateInformation (mb);
        SFProcessor q (makeModule ("PAD"));
        q.setStateInformation (mb.getData(), (int) mb.getSize());
        SFEditor::testHeightLimit = 640;
        std::unique_ptr<juce::AudioProcessorEditor> ed (q.createEditor());
        auto& e = dynamic_cast<SFEditor&> (*ed);
        const bool open = ! e.folded (SFEditor::kFoldMotion) && ! e.folded (SFEditor::kFoldSetup);
        const bool top = e.scroller().getViewPositionY() == 0 && e.getHeight() <= 640 && SFEditor::kHeader + e.contentHeight() > 640;
        std::printf ("   PAD saved with every group open, reopened on a 640 px screen: %s, %d px, at the top %s (of %d)\n", open ? "open" : "FOLDED", e.getHeight(), top ? "yes" : "NO", SFEditor::kHeader + e.contentHeight());
        CHECK (open && top, "PAD reopened open on a small screen: open %d, height %d, view at %d", open ? 1 : 0, e.getHeight(), e.scroller().getViewPositionY());

        SFProcessor r (makeModule ("PAD"));
        std::unique_ptr<juce::AudioProcessorEditor> ed2 (r.createEditor());
        auto& e2 = dynamic_cast<SFEditor&> (*ed2);
        e2.toggleFold (SFEditor::kFoldSetup);
        const auto rows = e2.titleRows();
        const bool seen = ! rows.empty() && e2.scroller().getViewArea().contains (rows.back().withRight (SFEditor::kW - 6));
        std::printf ("   PAD on a 640 px screen, SETUP opened: its title line %s\n", seen ? "scrolled into view" : "OUT OF VIEW");
        CHECK (seen, "a group opened at the bottom scrolls into view");
    }
    // PANIC (SF CONTROL) and THROW (SF DELAY) held as the window closes: let go, scrolled or not.
    for (const auto& [id, text, pid] : { std::tuple<std::string, juce::String, const char*> { "CONTROL", "PANIC", "panic" }, { "DELAY", "THROW", "throw" } })
        for (const int limit : { 480, 100000 })
        {
            SFEditor::testHeightLimit = limit;
            SFProcessor p (makeModule (id));
            float down = 0.0f;
            {
                std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
                auto& e = dynamic_cast<SFEditor&> (*ed);
                std::vector<juce::Component*> all;
                deep (e.strip(), all);
                for (auto* c : all)
                    if (auto* b = dynamic_cast<juce::TextButton*> (c); b != nullptr && b->getButtonText() == text) { b->setState (juce::Button::buttonDown); break; }
                down = p.apvts.getRawParameterValue (pid)->load();
            }
            const float after = p.apvts.getRawParameterValue (pid)->load();
            CHECK (down > 0.5f && after < 0.5f, "%s %s held as the window (held to %d) closes: down %.0f, after %.0f", id.c_str(), text.toRawUTF8(), limit, down, after);
        }
    SFEditor::testHeightLimit = 0;
}
