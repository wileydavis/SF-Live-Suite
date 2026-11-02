#include "Sample.h"
#include <cmath>

namespace sf
{
static juce::AudioFormatManager& formats()
{
    static juce::AudioFormatManager fm;
    static bool init = false;
    if (! init) { fm.registerBasicFormats(); init = true; }
    return fm;
}

static void analyse (SampleData& s)
{
    const int n = s.buf.getNumSamples();
    if (n == 0) return;
    std::vector<float> mono ((size_t) n);
    for (int i = 0; i < n; ++i)
        mono[(size_t) i] = 0.5f * (s.buf.getSample (0, i) + s.buf.getSample (s.buf.getNumChannels() > 1 ? 1 : 0, i));

    // Peak and decay (-40 dB after peak).
    int peakIdx = 0; float pk = 0.0f;
    for (int i = 0; i < n; ++i) if (std::fabs (mono[(size_t) i]) > pk) { pk = std::fabs (mono[(size_t) i]); peakIdx = i; }
    s.peak = std::max (pk, 1.0e-6f);
    int last = peakIdx;
    for (int i = peakIdx; i < n; ++i) if (std::fabs (mono[(size_t) i]) > pk * 0.01f) last = i;
    s.decaySec = (float) ((last - peakIdx) / s.sr);

    // Onsets: energy flux in 10 ms hops.
    const int hop = std::max (64, (int) (s.sr * 0.01));
    std::vector<float> e;
    for (int i = 0; i + hop <= n; i += hop)
    {
        float sum = 0.0f;
        for (int k = 0; k < hop; ++k) sum += mono[(size_t) (i + k)] * mono[(size_t) (i + k)];
        e.push_back (std::sqrt (sum / (float) hop));
    }
    float mean = 0.0f;
    for (auto v : e) mean += v;
    mean /= (float) std::max<size_t> (1, e.size());
    s.slices.clear();
    int lastOnset = -1000;
    for (size_t h = 1; h < e.size(); ++h)
    {
        const float flux = e[h] - e[h - 1];
        if (flux > mean * 0.6f && e[h] > mean * 0.5f && (int) h - lastOnset > 8)
        {
            s.slices.push_back ((int) h * hop);
            lastOnset = (int) h;
        }
    }
    if (s.slices.empty() || s.slices.front() > hop * 4) s.slices.insert (s.slices.begin(), 0);
    if (s.slices.size() > 32) s.slices.resize (32);

    // Pitch: normalised autocorrelation on a window after the attack (YIN-lite).
    const int start = std::min (n - 1, peakIdx + (int) (s.sr * 0.02));
    const int win = std::min ((int) (s.sr * 0.08), n - start);
    s.rootHz = 0.0f; s.rootNote = 60.0f;
    if (win > (int) (s.sr * 0.03))
    {
        const int minLag = (int) (s.sr / 1200.0), maxLag = std::min (win / 2, (int) (s.sr / 30.0));
        float best = 0.0f; int bestLag = 0;
        for (int lag = minLag; lag < maxLag; ++lag)
        {
            double num = 0, d1 = 0, d2 = 0;
            for (int i = 0; i < win - lag; ++i)
            {
                const float a = mono[(size_t) (start + i)], b = mono[(size_t) (start + i + lag)];
                num += a * b; d1 += a * a; d2 += b * b;
            }
            const float r = (float) (num / std::sqrt (d1 * d2 + 1e-12));
            if (r > best) { best = r; bestLag = lag; }
        }
        if (best > 0.8f && bestLag > 0)
        {
            s.rootHz = (float) (s.sr / bestLag);
            s.rootNote = 69.0f + 12.0f * std::log2 (s.rootHz / 440.0f);
        }
    }
}

SamplePtr makeSample (juce::AudioBuffer<float>&& audio, double fileSr, double targetSr, const juce::String& name, bool encode)
{
    auto s = std::make_shared<SampleData>();
    s->name = name;
    s->sr = targetSr;
    const int inCh = audio.getNumChannels();
    const int inN = audio.getNumSamples();
    if (inCh == 0 || inN == 0) return nullptr;

    if (encode)
    {
        juce::FlacAudioFormat flacFmt;
        auto* mos = new juce::MemoryOutputStream (s->flac, false);
        std::unique_ptr<juce::AudioFormatWriter> w (flacFmt.createWriterFor (mos, fileSr, (unsigned int) std::min (inCh, 2), 24, {}, 0));
        if (w != nullptr) w->writeFromAudioSampleBuffer (audio, 0, inN);
        else delete mos;
    }

    const double ratio = fileSr / targetSr;
    const int outN = (int) std::ceil (inN / ratio);
    s->buf.setSize (2, outN);
    for (int c = 0; c < 2; ++c)
    {
        const int src = std::min (c, inCh - 1);
        if (std::abs (ratio - 1.0) < 1e-9)
            s->buf.copyFrom (c, 0, audio, src, 0, inN);
        else
        {
            juce::LagrangeInterpolator interp;
            interp.process (ratio, audio.getReadPointer (src), s->buf.getWritePointer (c), outN, inN, 0);
        }
    }
    analyse (*s);
    return s;
}

static SamplePtr readFrom (juce::AudioFormatReader* r, double targetSr, const juce::String& name, bool encode)
{
    std::unique_ptr<juce::AudioFormatReader> reader (r);
    if (reader == nullptr || reader->lengthInSamples <= 0) return nullptr;
    const int len = (int) std::min<juce::int64> (reader->lengthInSamples, (juce::int64) (reader->sampleRate * 60 * 20));
    juce::AudioBuffer<float> b ((int) std::max (1u, std::min (2u, reader->numChannels)), len);
    reader->read (&b, 0, len, 0, true, true);
    return makeSample (std::move (b), reader->sampleRate, targetSr, name, encode);
}

SamplePtr loadSampleFile (const juce::File& f, double targetSr)
{
    return readFrom (formats().createReaderFor (f), targetSr, f.getFileNameWithoutExtension(), true);
}

SamplePtr loadSampleFlac (const juce::MemoryBlock& mb, const juce::String& name, double targetSr)
{
    juce::FlacAudioFormat flacFmt;
    auto* r = flacFmt.createReaderFor (new juce::MemoryInputStream (mb, true), true);
    if (r == nullptr) return nullptr;
    auto s = readFrom (r, targetSr, name, false);
    if (s != nullptr) const_cast<SampleData&> (*s).flac = mb;
    return s;
}

void SampleSlot::save (juce::ValueTree& t, const juce::Identifier& key) const
{
    if (auto s = get())
        if (s->flac.getSize() > 0)
        {
            juce::ValueTree c (key);
            c.setProperty ("name", s->name, nullptr);
            c.setProperty ("flac", s->flac, nullptr);
            t.appendChild (c, nullptr);
        }
}

void SampleSlot::load (const juce::ValueTree& t, const juce::Identifier& key, double sr)
{
    auto c = t.getChildWithName (key);
    if (! c.isValid()) { set (nullptr); return; }
    if (auto* mb = c.getProperty ("flac").getBinaryData())
        set (loadSampleFlac (*mb, c.getProperty ("name").toString(), sr));
}
} // namespace sf
