#pragma once
// Sample loading, analysis and embedding (FLAC inside the Live set), shared by every sampler.
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_data_structures/juce_data_structures.h>
#include "BandLimited.h"
#include <atomic>
#include <memory>
#include <mutex>
#include <vector>

namespace sf
{
// Octave-down copies of a stereo buffer (each a half-band low-pass and every second sample), built
// off the audio thread. A read far above the original pitch uses the copy where the rate is
// between 1 and 2, so the band-limited read costs at most about 36 taps however high a sample is
// played (reading the original at rate r costs about 18 r).
struct Mips
{
    static constexpr int kMax = 12;                 // rates up to 8192
    std::vector<juce::AudioBuffer<float>> lv;       // lv[k]: decimated by 2^(k+1)
    void build (const juce::AudioBuffer<float>& b);
};

// Band-limited read of channel ch (clamped to what the buffer has) at a fractional position,
// for any playback rate (input samples per output sample). Audio thread; allocation-free.
inline float readAt (const juce::AudioBuffer<float>& b, const Mips& m, int ch, double pos, double rate)
{
    const juce::AudioBuffer<float>* src = &b;
    for (size_t k = 0; rate > 2.0 && k < m.lv.size(); ++k) { src = &m.lv[k]; rate *= 0.5; pos *= 0.5; }
    return sampleAt (src->getReadPointer (std::min (ch, src->getNumChannels() - 1)), src->getNumSamples(), pos, rate);
}

struct SampleData
{
    juce::AudioBuffer<float> buf;   // stereo, at the module's sample rate
    double sr = 48000.0;
    juce::String name;
    float rootHz = 0.0f;            // 0 = no clear pitch
    float rootNote = 60.0f;         // MIDI note (from rootHz), 60 if unpitched
    float decaySec = 0.5f;          // time to fall 40 dB from the peak
    std::vector<int> slices;        // onset positions (samples), first = first onset
    juce::MemoryBlock flac;         // embedded copy for the Live set
    float flacGain = 1.0f;          // the copy is scaled to fit FLAC's range: multiply back by this
    float peak = 1.0f;
    Mips mips;                      // octave-down copies of buf (readAt)
    int length() const { return buf.getNumSamples(); }
    float read (int ch, double pos, double rate) const { return readAt (buf, mips, ch, pos, rate); }
};
using SamplePtr = std::shared_ptr<const SampleData>;

// Decode, resample to `targetSr` (windowed sinc), analyse. Returns null on failure.
// Message/background thread. Files longer than maxSeconds are cut there.
SamplePtr loadSampleFile (const juce::File&, double targetSr, double maxSeconds = 60.0);
SamplePtr loadSampleFlac (const juce::MemoryBlock&, const juce::String& name, double targetSr, float gain = 1.0f);
SamplePtr makeSample (juce::AudioBuffer<float>&& audio, double fileSr, double targetSr, const juce::String& name, bool encode);

// Sample holder shared by the message thread (set, save, paint) and the audio thread (audio()).
// The audio thread only ever loads a raw pointer (lock-free, wait-free); a replaced sample is
// kept alive on the message thread for 2 s, long after any audio callback that saw it ended.
class SampleSlot
{
public:
    const SampleData* audio() const noexcept { return live.load (std::memory_order_acquire); }   // audio thread
    // Everything below may be called from the message thread, the host's prepare / state threads
    // or a loader: guarded by a mutex the audio thread never touches.
    SamplePtr get() const { const std::lock_guard<std::mutex> g (lock); return owned; }
    void set (SamplePtr p);
    void save (juce::ValueTree& t, const juce::Identifier& key) const;
    void load (const juce::ValueTree& t, const juce::Identifier& key, double sr);
private:
    std::atomic<const SampleData*> live { nullptr };
    mutable std::mutex lock;
    SamplePtr owned;
    struct Retired { SamplePtr p; juce::uint32 at; };
    std::vector<Retired> retired;
};

// Audio file extensions every sampler accepts.
inline bool isAudioFile (const juce::String& path)
{
    return path.endsWithIgnoreCase (".wav") || path.endsWithIgnoreCase (".aif") || path.endsWithIgnoreCase (".aiff")
        || path.endsWithIgnoreCase (".flac") || path.endsWithIgnoreCase (".mp3") || path.endsWithIgnoreCase (".ogg")
        || path.endsWithIgnoreCase (".m4a");
}
} // namespace sf
