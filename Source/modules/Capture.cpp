#include "../core/Module.h"
#include "../core/Bus.h"
#include "../core/Capture.h"
#include "../core/IrTools.h"
#include "../core/Ui.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <new>

namespace sf
{
// =====================================================================================  CAPTURE
// The room mics' track (Source/core/Capture.h). It never passes its input: its output is silence,
// or the sweep while SF TERROIR has asked for a place. A take: the sweep file, generated at this
// track's rate and played at SWEEP LEVEL, then 3 s more, the input recorded all the while on the
// interface's own clock; written as a 32-bit float WAV, then TERROIR analyses it. Live's transport
// must be stopped: a take refuses to start while it plays and stops (10 ms fade) if it starts.
// PANIC, a new sample rate, a fault or TERROIR's STOP end a take at any stage, before it has started
// too; an ended take is never played on, written or imported. Removing the plugin ends its take.
class CaptureModule : public Module, private juce::Thread
{
public:
    enum { LEVEL };
    CaptureModule() : juce::Thread ("SF CAPTURE")
    {
        seenSeq = Bus::get().d().capSeq.load();             // a request from before it was here is not its own
        startThread (juce::Thread::Priority::low);
    }
    ~CaptureModule() override
    {
        stopThread (10000);                                  // a take being written is finished first
        // A take still this plugin's ends with it: the bus outlives the plugin, and a take left in
        // progress there would refuse every capture after it. (A take written stays Done: TERROIR
        // takes it in.)
        if (const uint32_t s = tookSeq.load(); s != 0)
            capture::advance (Bus::get().d().capStatus, s, capture::kBusyMask, capture::Failed, capture::Removed);
        delete pending.exchange (nullptr);
        delete finished.exchange (nullptr);
        for (auto& r : retired) delete r.exchange (nullptr);
        delete job;
    }

    std::string id() const override { return "CAPTURE"; }
    std::vector<ParamSpec> params() const override
    {
        // The sweep's peak, in dBFS (the file's own: -6). Turn the speaker for loudness; this for headroom.
        auto l = knob ("level", "SWEEP LEVEL", -30.0f, 0.0f, -6.0f, "dB").prim().noGlide();
        l.song = false; l.preset = false;
        return { l };
    }
    double tailSeconds() const override { return 0.0; }

    // A new stream: a take built before it (or being built) is for another one and never starts.
    void prepare (double r, int) override
    {
        rate.store (r);
        fadeLen = std::max (64, (int) std::lround (0.01 * r));
        epoch.fetch_add (1, std::memory_order_acq_rel);
        if (job != nullptr) endJob (capture::Cancelled);
        if (auto* t = pending.exchange (nullptr)) { end (t->seq, capture::Cancelled); delete t; }
        meter[0] = meter[1] = 0.0f;
    }
    // PANIC or a fault (audio thread): silent now (no fade), and a take being made never starts.
    void reset() override
    {
        epoch.fetch_add (1, std::memory_order_acq_rel);
        if (job != nullptr) endJob (capture::Cancelled);
    }

    void process (Ctx& c, juce::AudioBuffer<float>& io, juce::AudioBuffer<float>*, juce::AudioBuffer<float>*, juce::MidiBuffer&) override
    {
        auto& d = Bus::get().d();
        const int64_t now = Bus::nowMs(), gap = now - lastBlockMs.load();
        d.captureSeenMs = now;
        lastBlockMs = now;
        const int n = c.n;
        // This plugin's audio stood still longer than the lease (its track off, Live's engine
        // stopped): a take it had is over and never goes on (SF TERROIR may have ended it already).
        if (gap > capture::kLeaseMs)
        {
            if (job != nullptr) endJob (capture::Lost);
            if (auto* t = pending.exchange (nullptr, std::memory_order_acq_rel)) { end (t->seq, capture::Lost); retire (t); }
        }
        float* L = io.getWritePointer (0);
        float* R = io.getNumChannels() > 1 ? io.getWritePointer (1) : nullptr;
        // The mics, for the meters.
        for (int ch = 0; ch < 2; ++ch)
        {
            const float* x = ch == 0 ? L : (R != nullptr ? R : L);
            float pk = 0.0f;
            for (int i = 0; i < n; ++i) pk = std::max (pk, std::fabs (x[i]));
            const float m = meter[ch].load() * std::pow (0.5f, (float) n / (float) (0.3 * c.sr));   // falls by half in 0.3 s
            meter[ch] = std::max (m, pk);
        }
        // A take of this plugin's in progress: its lease (TERROIR ends a take whose recorder stops answering).
        const auto st = capture::status (d.capStatus);
        if (const uint32_t mine = tookSeq.load(); mine != 0 && st.seq == mine && capture::busy (st.state)) d.capOwnerSeenMs = now;
        // PANIC held (SF CONTROL): this plugin's input and output are muted; a take would be silence.
        const bool panic = d.panicHeld.load() != 0 && Bus::get().controlAlive();
        if (job == nullptr)
            if (auto* t = pending.exchange (nullptr, std::memory_order_acq_rel))
            {
                // Started only if it is still the request in force, not stopped, and made for this stream.
                const int32_t why = c.playing || c.offline ? capture::TransportStarted
                                  : panic || t->rate != c.sr || t->epoch != epoch.load (std::memory_order_acquire)
                                      || d.capCancel.load() == t->seq ? capture::Cancelled : capture::None;
                if (why != capture::None) { end (t->seq, why); retire (t); }
                else if (! capture::advance (d.capStatus, t->seq, capture::bit (capture::Preparing), capture::Playing, capture::None)) retire (t);
                else { job = t; pos = 0; fade = -1; takePeak = 0.0f; }
            }
        if (job == nullptr) { io.clear(); return; }                   // never the input
        // Given up on while this plugin's audio stood still (the lease ran out; maybe a newer take
        // since): over, at once and silently. Its sound had stopped with the audio already.
        const auto js = capture::status (d.capStatus);
        if (js.seq != job->seq || js.state != capture::Playing) { retire (job); job = nullptr; io.clear(); return; }
        if (fade < 0 && (c.playing || c.offline || panic || d.capCancel.load() == job->seq))
        {
            fade = fadeLen;
            stopReason = c.playing || c.offline ? capture::TransportStarted : capture::Cancelled;
        }
        const float g = juce::Decibels::decibelsToGain (c.v (LEVEL) + 6.0f);   // the file's sweep peaks at -6 dBFS
        const float* sw = job->sweep.getReadPointer (0);
        float* rl = job->rec.getWritePointer (0);
        float* rr = job->rec.getWritePointer (1);
        const int sweepN = job->sweep.getNumSamples(), total = job->rec.getNumSamples();
        for (int i = 0; i < n; ++i)
        {
            if (pos < total)
            {
                const float a = L[i], b = R != nullptr ? R[i] : L[i];
                rl[pos] = a; rr[pos] = b;
                takePeak = std::max ({ takePeak, std::fabs (a), std::fabs (b) });
            }
            float s = pos < sweepN ? sw[pos] * g : 0.0f;
            if (fade >= 0) { s *= (float) fade / (float) fadeLen; if (fade > 0) --fade; }
            L[i] = s;
            if (R != nullptr) R[i] = s;
            ++pos;
        }
        d.capProgress = (float) std::min (1.0, (double) pos / (double) total);
        d.capPeak = takePeak;
        if (fade == 0) endJob (stopReason);
        else if (pos >= total)
        {
            if (capture::advance (d.capStatus, job->seq, capture::bit (capture::Playing), capture::Writing, capture::None))
            {
                if (auto* old = finished.exchange (job, std::memory_order_acq_rel)) retire (old);   // (none: one take at a time)
            }
            else retire (job);
            job = nullptr;
        }
    }

    // ------------------------------------------------------------------------------------- display
    int displayHeight() const override { return 110; }
    void paint (juce::Graphics& g, juce::Rectangle<float> r) override
    {
        ui::frame (g, r, "ROOM MICS");
        auto& d = Bus::get().d();
        auto in = r.reduced (10, 18);
        const auto st = capture::status (d.capStatus);
        const bool mine = tookSeq.load() != 0 && st.seq == tookSeq.load();
        const int state = mine ? st.state : capture::Idle;
        juce::String text = "READY. SF TERROIR'S CAPTURE STARTS A TAKE; THE TRANSPORT MUST BE STOPPED";
        juce::Colour col = ui::kDim;
        const double total = takeSeconds();
        if (state == capture::Preparing) { text = "STARTING A TAKE"; col = ui::kAmber; }
        else if (state == capture::Playing)
        {
            text = "PLAYING THE SWEEP: " + juce::String ((int) (d.capProgress.load() * total)) + " OF " + juce::String ((int) total) + " S. QUIET PLEASE";
            col = ui::kGreen;
        }
        else if (state == capture::Writing) { text = "WRITING THE TAKE"; col = ui::kGreen; }
        else if (state == capture::Done) { text = "TAKE WRITTEN: " + lastName(); col = ui::kText; }
        else if (state == capture::Failed)
        {
            const int e = st.error;
            text = e == capture::TransportStarted ? "STOPPED: THE TRANSPORT STARTED" : e == capture::CantWrite ? "STOPPED: CAN'T WRITE IN " + capture::folder().getFullPathName().toUpperCase()
                 : e == capture::NoMemory ? "STOPPED: NOT ENOUGH MEMORY" : e == capture::Lost ? "STOPPED: THIS TRACK'S AUDIO STOOD STILL. CAPTURE AGAIN" : "STOPPED";
            col = ui::kRed;
        }
        ui::label (g, text, in.removeFromTop (16), col, 11.0f);
        in.removeFromTop (6);
        for (int ch = 0; ch < 2; ++ch)
        {
            auto row = in.removeFromTop (12);
            ui::label (g, ch == 0 ? "IN L" : "IN R", row.removeFromLeft (40), ui::kFaint, 10.0f);
            ui::levelBar (g, row.reduced (0, 2), meter[ch].load());
            in.removeFromTop (4);
        }
        const float pk = mine && state != capture::Idle ? d.capPeak.load() : 0.0f;
        const juce::String foot = pk >= 0.99f ? "THE MICS CLIPPED IN THIS TAKE: SPEAKER OR GAIN DOWN, AND AGAIN"
                                : "OUT: THE SWEEP ONLY, NEVER THE MICS. TAKES GO TO MUSIC/SF LIVE SUITE/TERROIR/CAPTURES";
        ui::label (g, foot, in.removeFromBottom (14), pk >= 0.99f ? ui::kAmber : ui::kFaint, 10.0f);
    }

private:
    struct Take
    {
        juce::AudioBuffer<float> sweep, rec;       // mono sweep at this rate; the stereo take
        double rate = 0.0;                         // the rate both were made for
        int64_t stamp = 0;
        uint32_t seq = 0, epoch = 0;               // the request; the stream it was made for (prepare / reset)
    };
    // Request seq, if still in progress, ends (Failed, why).
    static void end (uint32_t seq, int32_t why)
    {
        capture::advance (Bus::get().d().capStatus, seq, capture::kBusyMask, capture::Failed, why);
    }
    // Audio thread: the take playing ends here, unfinished.
    void endJob (int32_t why)
    {
        end (job->seq, why);
        retire (job);
        job = nullptr;
    }
    // Audio thread: a take for the worker to free (never freed here, but with every slot full).
    void retire (Take* t)
    {
        for (auto& r : retired)
        {
            Take* e = nullptr;
            if (r.compare_exchange_strong (e, t, std::memory_order_acq_rel)) return;
        }
        delete t;                                    // eight waiting: the worker has stood still for a while
    }

    static double takeSeconds() { return ir::kLeadSec + ir::kRepeats * (ir::kSweepSec + ir::kGapSec) + capture::kTailSec; }
    juce::String lastName() const { const juce::SpinLock::ScopedLockType sl (nameLock); return lastFile; }
    // Tests: wait here while the bit is set (capture::testHold).
    void hold (int bit) { while ((capture::testHold.load() & bit) != 0 && ! threadShouldExit()) wait (1); }

    // ------------------------------------------------------------------------------------- worker
    void run() override
    {
        auto& d = Bus::get().d();
        while (! threadShouldExit())
        {
            wait (10);
            for (auto& r : retired) delete r.exchange (nullptr, std::memory_order_acq_rel);
            if (auto* t = finished.exchange (nullptr, std::memory_order_acq_rel)) { write (*t); delete t; }
            const uint32_t seq = d.capSeq.load (std::memory_order_acquire);
            if (seq == seenSeq) continue;
            hold (capture::kHoldClaim);
            if (threadShouldExit()) return;
            seenSeq = seq;
            // Only an instance whose audio runs (its track on, Live's engine going) takes a request.
            // A request older than 2 s (from before this instance ran) is never taken: no sweep out
            // of the blue. TERROIR waits 3 s for the claim, then ends the request itself.
            if (Bus::nowMs() - lastBlockMs.load() > 500 || std::abs (juce::Time::currentTimeMillis() - d.capStamp.load()) > 2000) continue;
            // The claim: one plugin moves the request from Idle to Preparing (one STOPped before it
            // was claimed, or given up on, is no longer Idle). Its lease starts here.
            d.capOwnerSeenMs = Bus::nowMs();
            if (! capture::advance (d.capStatus, seq, capture::bit (capture::Idle), capture::Preparing, capture::None)) continue;
            tookSeq = seq;
            d.capProgress = 0.0f; d.capPeak = 0.0f;
            const uint32_t ep = epoch.load (std::memory_order_acquire);
            std::unique_ptr<Take> t;
            try
            {
                t = std::make_unique<Take>();
                const double r = rate.load();
                t->rate = r;
                if (capture::testShortTake.load())
                {
                    t->sweep.setSize (1, (int) std::lround (0.5 * r));
                    for (int i = 0; i < t->sweep.getNumSamples(); ++i) t->sweep.setSample (0, i, 0.5f * (float) std::sin (6.283185307 * 1000.0 * i / r));
                    t->rec.setSize (2, t->sweep.getNumSamples() + (int) std::lround (0.2 * r));
                }
                else
                {
                    t->sweep = ir::sweepFile (r);
                    t->rec.setSize (2, t->sweep.getNumSamples() + (int) std::lround (capture::kTailSec * r));
                }
                t->rec.clear();
                t->stamp = d.capStamp.load();
                t->seq = seq;
                t->epoch = ep;
            }
            catch (const std::bad_alloc&) { end (seq, capture::NoMemory); continue; }
            hold (capture::kHoldPublish);
            if (threadShouldExit()) return;
            // STOPped, or the stream changed (prepare, PANIC), while it was being made: never played.
            if (d.capCancel.load() == seq || ep != epoch.load (std::memory_order_acquire)) { end (seq, capture::Cancelled); continue; }
            delete pending.exchange (t.release(), std::memory_order_acq_rel);    // (an older one, never started: its request is over)
        }
    }
    void write (const Take& t)
    {
        auto& d = Bus::get().d();
        hold (capture::kHoldWrite);
        // STOPped as it was being finished, or given up on: not written, not taken in.
        auto stillOn = [&] { const auto s = capture::status (d.capStatus); return s.seq == t.seq && s.state == capture::Writing && d.capCancel.load() != t.seq; };
        if (! stillOn()) { end (t.seq, capture::Cancelled); return; }
        const auto f = capture::fileFor (t.stamp);
        f.getParentDirectory().createDirectory();
        f.deleteFile();
        bool ok = false;
        if (auto os = std::unique_ptr<juce::FileOutputStream> (f.createOutputStream()))
        {
            juce::WavAudioFormat wav;
            std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (os.get(), t.rate, 2, 32, {}, 0));
            if (w != nullptr)
            {
                os.release();                                  // the writer owns it now
                ok = w->writeFromAudioSampleBuffer (t.rec, 0, t.rec.getNumSamples());
            }
        }
        if (! ok) { f.deleteFile(); end (t.seq, capture::CantWrite); return; }
        if (! stillOn()) { f.deleteFile(); end (t.seq, capture::Cancelled); return; }
        { const juce::SpinLock::ScopedLockType sl (nameLock); lastFile = f.getFileNameWithoutExtension().toUpperCase(); }
        capture::advance (d.capStatus, t.seq, capture::bit (capture::Writing), capture::Done, capture::None);
    }

    std::atomic<double> rate { 48000.0 };
    std::atomic<Take*> pending { nullptr }, finished { nullptr };
    std::array<std::atomic<Take*>, 8> retired {};
    Take* job = nullptr;                             // audio thread: the take playing
    int pos = 0, fade = -1, fadeLen = 480;            // the stop's fade: 10 ms (a stop never clicks)
    int32_t stopReason = capture::Cancelled;
    uint32_t seenSeq = 0;
    float takePeak = 0.0f;
    std::atomic<uint32_t> tookSeq { 0 };             // the last request this instance claimed
    std::atomic<uint32_t> epoch { 0 };               // prepare and reset: a take made before is for another stream
    std::atomic<int64_t> lastBlockMs { 0 };
    std::atomic<float> meter[2] { { 0.0f }, { 0.0f } };
    mutable juce::SpinLock nameLock;
    juce::String lastFile;
};

ModulePtr makeCAPTURE() { return std::make_unique<CaptureModule>(); }
} // namespace sf
