#pragma once
// SF TERROIR's offline tools: the test sweep, turning a recording into an impulse response, and the
// transforms SIZE, DECAY, DARK and the mono-to-stereo spread apply to one. Message / background
// thread only (they allocate and take time).
//
// A capture is either
//   a sweep: the sweep file played from a speaker and recorded. Deconvolved against the sweep;
//            every sweep in the recording found, the playback clock's drift measured from their
//            spacing and corrected, the responses averaged (3 sweeps: 4.8 dB less noise). The
//            speaker's distortion lands before each response and is left out.
//   an impulse: a clap, a balloon, a click from the speaker, or an IR file. The loudest event, from
//            just before its onset to where its tail meets the noise floor (cut before a second
//            event).
// Either way the result starts 1 ms before the onset, its tail fades out where it meets the noise,
// it is high-passed at 20 Hz and scaled to a fixed energy, so every place plays at the same level.
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>

namespace sf
{
namespace ir
{
constexpr double kSweepSec = 10.0, kGapSec = 4.0, kLeadSec = 1.0;
constexpr int kRepeats = 3;
constexpr double kMaxSeconds = 12.0;           // a captured IR is at most this long
constexpr double kMaxRecording = 90.0;         // longer recordings are refused (trim them)
constexpr float kEnergy = 0.85f;               // mean per-channel energy of every IR (sum of squares): SF REVERB's level

// The exponential sine sweep, 20 Hz to 20 kHz (or 0.45 of the rate) over kSweepSec, with fades.
std::vector<float> sweep (double sr);
// The file to play from the speaker: kLeadSec of silence, then kRepeats x (sweep, kGapSec silence), -6 dBFS.
juce::AudioBuffer<float> sweepFile (double sr);

struct Capture
{
    bool ok = false;
    juce::String error;                        // why not (ok == false)
    juce::String kind;                         // "SWEEP x3", "IMPULSE"
    juce::AudioBuffer<float> ir;               // 1 or 2 channels, at the recording's rate
    double sr = 48000.0;
    float snrDb = 0.0f;                        // decay range above the noise floor
    float rt60 = 0.0f;                         // seconds
    double driftPpm = 0.0;                     // sweep: playback clock against the recorder's
};
// A sweep recording if it holds the sweep, else an impulse. rec: 1 or 2 channels.
Capture analyse (const juce::AudioBuffer<float>& rec, double sr);
Capture fromSweep (const juce::AudioBuffer<float>& rec, double sr);
Capture fromImpulse (const juce::AudioBuffer<float>& rec, double sr);

// Reverberation time from the energy decay (Schroeder): T20, or T10 when the decay is too short.
float rt60 (const juce::AudioBuffer<float>& ir, double sr);
// A second channel for a mono IR: the same decay and colour, its tail decorrelated (velvet noise).
void decorrelate (const float* in, float* out, int n, double sr);
// Windowed-sinc resampling by `ratio` (output samples per input sample), 16 zero crossings.
std::vector<float> stretch (const float* in, int n, double ratio);
// The tail darkens with time: a low-pass falling octPerSec octaves a second from 20 kHz (to 150 Hz).
void darken (float* x, int n, double sr, float octPerSec);
// A synthetic room for empty slots: early reflections and a 1.3 s tail, stereo, scaled like a capture.
juce::AudioBuffer<float> builtInRoom (double sr);
// Scales the IR to kEnergy per channel, on average: the balance between the channels stays (a
// spaced pair keeps the speaker's side).
void normalise (juce::AudioBuffer<float>& ir);

// Which tracks of a recording to use. A field recorder's poly file can put its L/R mix ahead of the
// microphones (a MixPre writes MixL, MixR, then the inputs): tracks named MIX... are skipped when
// there are others, then the first two are taken (one: a mono place). names: one per channel in
// file order, or empty when the file names none (then the first two).
std::vector<int> pickTracks (const juce::StringArray& names, int numChannels);
// A WAV file's track names in file order: its iXML TRACK_LIST, else the "sTRK1=..." lines of its
// bext description (Sound Devices, Zoom). Empty when it names none.
juce::StringArray trackNames (const juce::File& wav, const juce::String& bextDescription, int numChannels);
// Two channels carrying the same signal (a mono take saved as stereo, a centred mic in a mix): the
// difference is 60 dB or more under the signal.
bool sameChannels (const juce::AudioBuffer<float>& b);
} // namespace ir
} // namespace sf
