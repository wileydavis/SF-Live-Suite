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
// PANIC, a new sample rate or a fault end a take too; an ended take is never written.
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
        delete pending.exchange (nullptr);
        delete finished.exchange (nullptr);
        delete aborted.exchange (nullptr);
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

    void prepare (double r, int) override
    {
        rate.store (r);
        fadeLen = std::max (64, (int) std::lround (0.01 * r));
        if (job != nullptr) { stop (capture::Cancelled); }   // built for the old rate
        if (auto* t = pending.exchange (nullptr))            // claimed, not started: ended too, or the bus would say Preparing for good
        {
            delete t;
            auto& d = Bus::get().d();
            d.capError = capture::Cancelled;
            d.capState = capture::Failed;
        }
        meter[0] = meter[1] = 0.0f;
    }
    // PANIC or a fault: silent now (no fade).
    void reset() override
    {
        if (job != nullptr) stop (capture::Cancelled);
    }

    void process (Ctx& c, juce::AudioBuffer<float>& io, juce::AudioBuffer<float>*, juce::AudioBuffer<float>*, juce::MidiBuffer&) override
    {
        auto& d = Bus::get().d();
        const int64_t now = Bus::nowMs();
        d.captureSeenMs = now;
        lastBlockMs = now;
        const int n = c.n;
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
        // PANIC held (SF CONTROL): this plugin's input and output are muted; a take would be silence.
        const bool panic = d.panicHeld.load() != 0 && Bus::get().controlAlive();
        if (job == nullptr)
            if (auto* t = pending.exchange (nullptr, std::memory_order_acq_rel))
            {
                if (c.playing || c.offline) { d.capError = capture::TransportStarted; d.capState = capture::Failed; retire (t); }
                else if (panic || t->rate != c.sr) { d.capError = capture::Cancelled; d.capState = capture::Failed; retire (t); }   // or built for another rate
                else
                {
                    job = t; pos = 0; fade = -1; takePeak = 0.0f;
                    cancelAt = d.capCancel.load();
                    d.capState = capture::Playing;
                }
            }
        if (job == nullptr) { io.clear(); return; }                   // never the input
        if (fade < 0 && (c.playing || c.offline || panic || d.capCancel.load() != cancelAt))
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
        if (fade == 0) stop (stopReason);
        else if (pos >= total)
        {
            d.capState = capture::Writing;
            delete finished.exchange (job, std::memory_order_acq_rel);    // (none waiting: one take at a time)
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
        const bool mine = d.capTaken.load() == tookSeq.load() && tookSeq.load() != 0;
        const int st = mine ? d.capState.load() : capture::Idle;
        juce::String text = "READY. SF TERROIR'S CAPTURE STARTS A TAKE; THE TRANSPORT MUST BE STOPPED";
        juce::Colour col = ui::kDim;
        const double total = takeSeconds();
        if (st == capture::Preparing) { text = "STARTING A TAKE"; col = ui::kAmber; }
        else if (st == capture::Playing)
        {
            text = "PLAYING THE SWEEP: " + juce::String ((int) (d.capProgress.load() * total)) + " OF " + juce::String ((int) total) + " S. QUIET PLEASE";
            col = ui::kGreen;
        }
        else if (st == capture::Writing) { text = "WRITING THE TAKE"; col = ui::kGreen; }
        else if (st == capture::Done) { text = "TAKE WRITTEN: " + lastName(); col = ui::kText; }
        else if (st == capture::Failed)
        {
            const int e = d.capError.load();
            text = e == capture::TransportStarted ? "STOPPED: THE TRANSPORT STARTED" : e == capture::CantWrite ? "STOPPED: CAN'T WRITE IN " + capture::folder().getFullPathName().toUpperCase()
                 : e == capture::NoMemory ? "STOPPED: NOT ENOUGH MEMORY" : "STOPPED";
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
        const float pk = mine && st != capture::Idle ? d.capPeak.load() : 0.0f;
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
        uint32_t seq = 0;
    };
    // Audio thread: the take ends here, unfinished.
    void stop (int32_t reason)
    {
        auto& d = Bus::get().d();
        d.capError = reason;
        d.capState = capture::Failed;
        retire (job);
        job = nullptr;
    }
    void retire (Take* t) { delete aborted.exchange (t, std::memory_order_acq_rel); }   // the worker frees it

    static double takeSeconds() { return ir::kLeadSec + ir::kRepeats * (ir::kSweepSec + ir::kGapSec) + capture::kTailSec; }
    juce::String lastName() const { const juce::SpinLock::ScopedLockType sl (nameLock); return lastFile; }

    // ------------------------------------------------------------------------------------- worker
    void run() override
    {
        auto& d = Bus::get().d();
        while (! threadShouldExit())
        {
            wait (10);
            delete aborted.exchange (nullptr, std::memory_order_acq_rel);
            if (auto* t = finished.exchange (nullptr, std::memory_order_acq_rel)) { write (*t); delete t; }
            const uint32_t seq = d.capSeq.load (std::memory_order_acquire);
            if (seq == seenSeq) continue;
            seenSeq = seq;
            // Only an instance whose audio runs (its track on, Live's engine going) takes a request,
            // and only one: the first to claim it.
            // A request older than 2 s (from before this instance ran) is never taken: no sweep out of the blue.
            // TERROIR waits 3 s for the claim, so a claim always lands while it still waits.
            if (Bus::nowMs() - lastBlockMs.load() > 500 || capture::busy (d.capState.load())
                || std::abs (juce::Time::currentTimeMillis() - d.capStamp.load()) > 2000) continue;
            uint32_t taken = d.capTaken.load();
            if (taken == seq || ! d.capTaken.compare_exchange_strong (taken, seq)) continue;
            tookSeq = seq;
            d.capError = capture::None; d.capProgress = 0.0f; d.capPeak = 0.0f;
            d.capState = capture::Preparing;
            try
            {
                auto t = std::make_unique<Take>();
                const double r = rate.load();
                t->rate = r;
                t->sweep = ir::sweepFile (r);
                t->rec.setSize (2, t->sweep.getNumSamples() + (int) std::lround (capture::kTailSec * r));
                t->rec.clear();
                t->stamp = d.capStamp.load();
                t->seq = seq;
                delete pending.exchange (t.release(), std::memory_order_acq_rel);
            }
            catch (const std::bad_alloc&) { d.capError = capture::NoMemory; d.capState = capture::Failed; }
        }
    }
    void write (const Take& t)
    {
        auto& d = Bus::get().d();
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
        if (! ok) { f.deleteFile(); d.capError = capture::CantWrite; d.capState = capture::Failed; return; }
        { const juce::SpinLock::ScopedLockType sl (nameLock); lastFile = f.getFileNameWithoutExtension().toUpperCase(); }
        d.capState = capture::Done;
    }

    std::atomic<double> rate { 48000.0 };
    std::atomic<Take*> pending { nullptr }, finished { nullptr }, aborted { nullptr };
    Take* job = nullptr;                             // audio thread: the take playing
    int pos = 0, fade = -1, fadeLen = 480;            // the stop's fade: 10 ms (a stop never clicks)
    int32_t stopReason = capture::Cancelled;
    uint32_t cancelAt = 0, seenSeq = 0;
    float takePeak = 0.0f;
    std::atomic<uint32_t> tookSeq { 0 };             // the last request this instance took
    std::atomic<int64_t> lastBlockMs { 0 };
    std::atomic<float> meter[2] { { 0.0f }, { 0.0f } };
    mutable juce::SpinLock nameLock;
    juce::String lastFile;
};

ModulePtr makeCAPTURE() { return std::make_unique<CaptureModule>(); }
} // namespace sf
