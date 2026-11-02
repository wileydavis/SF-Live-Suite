#pragma once
// Sample loading, analysis and embedding (FLAC inside the Live set), shared by every sampler.
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_data_structures/juce_data_structures.h>
#include <atomic>
#include <memory>
#include <vector>

namespace sf
{
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
    float peak = 1.0f;
    int length() const { return buf.getNumSamples(); }
};
using SamplePtr = std::shared_ptr<const SampleData>;

// Decode, resample to `targetSr`, analyse. Returns null on failure. Message/background thread.
SamplePtr loadSampleFile (const juce::File&, double targetSr);
SamplePtr loadSampleFlac (const juce::MemoryBlock&, const juce::String& name, double targetSr);
SamplePtr makeSample (juce::AudioBuffer<float>&& audio, double fileSr, double targetSr, const juce::String& name, bool encode);

// Lock-free holder: message thread sets, audio thread reads a snapshot per block.
class SampleSlot
{
public:
    SamplePtr get() const { return std::atomic_load (&ptr); }
    void set (SamplePtr p)
    {
        auto old = std::atomic_exchange (&ptr, std::move (p));
        retired.push_back (old);            // freed later on the message thread, never on audio
        if (retired.size() > 6) retired.erase (retired.begin());
    }
    void save (juce::ValueTree& t, const juce::Identifier& key) const;
    void load (const juce::ValueTree& t, const juce::Identifier& key, double sr);
private:
    SamplePtr ptr;
    std::vector<SamplePtr> retired;
};

// Audio file extensions every sampler accepts.
inline bool isAudioFile (const juce::String& path)
{
    return path.endsWithIgnoreCase (".wav") || path.endsWithIgnoreCase (".aif") || path.endsWithIgnoreCase (".aiff")
        || path.endsWithIgnoreCase (".flac") || path.endsWithIgnoreCase (".mp3") || path.endsWithIgnoreCase (".ogg")
        || path.endsWithIgnoreCase (".m4a");
}
} // namespace sf
