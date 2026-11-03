#include "Processor.h"
#include "Editor.h"
#include <chrono>
#include <cstring>

using namespace sf;

juce::AudioProcessor::BusesProperties SFProcessor::busesFor (const Module& m)
{
    BusesProperties b;
    if (m.instrument())
        b = b.withOutput ("Output", juce::AudioChannelSet::stereo(), true);
    else
        b = b.withInput ("Input", juce::AudioChannelSet::stereo(), true)
             .withOutput ("Output", juce::AudioChannelSet::stereo(), true);
    if (m.sidechain())
        b = b.withInput ("Sidechain", juce::AudioChannelSet::stereo(), false);
    for (auto& name : m.extraOutputs())
        b = b.withOutput (name, juce::AudioChannelSet::stereo(), false);
    return b;
}

static std::vector<ParamSpec> withSystem (std::vector<ParamSpec> s)
{
    auto g = choice ("slow", "SLOW", SFProcessor::glideNames(), 0); g.song = false;
    auto ms = choice ("macro_src", "MACRO SRC", { "OFF", "MACRO 1", "MACRO 2", "MACRO 3", "MACRO 4", "MACRO 5", "MACRO 6" }, 0); ms.song = false;
    auto ma = knob ("macro_amt", "MACRO AMT", -1.0f, 1.0f, 0.5f, "%"); ma.song = false; ma.glide = false;
    s.push_back (g); s.push_back (ms); s.push_back (ma);
    return s;
}

SFProcessor::SFProcessor (ModulePtr m)
    : juce::AudioProcessor (busesFor (*m)),
      module (std::move (m)),
      specs (withSystem (module->params())),
      numModParams ((int) module->params().size()),
      apvts (*this, nullptr, "PARAMS", makeLayout())
{
    jassert (specs.size() <= 96);
    for (auto& s : specs)
    {
        params.push_back (apvts.getParameter (s.id));
        raw.push_back (apvts.getRawParameterValue (s.id));
    }
    cur.resize (specs.size());
    prev.resize (specs.size());
    base.resize (specs.size());
    macroOff.assign (specs.size(), 0.0f);
    ovr.assign (specs.size(), NAN);
    ovrAge.assign (specs.size(), 0);
    for (size_t i = 0; i < specs.size(); ++i) cur[i] = prev[i] = base[i] = specs[i].def;
    for (auto& h : songHas) h = false;

    module->setParam = [this] (int i, float v)
    {
        if (i < 0 || i >= (int) params.size()) return;
        auto* p = params[(size_t) i];
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (v));
        p->endChangeGesture();
    };
    module->getParam = [this] (int i) { return i >= 0 && i < (int) raw.size() ? raw[(size_t) i]->load() : 0.0f; };
    glideFrom.assign (specs.size(), 0.0f); glideTo.assign (specs.size(), NAN); glideElapsed.assign (specs.size(), 0.0);
    sf::Bus::get();     // map the shared bus here, not on the audio thread
    startTimerHz (30);
}

SFProcessor::~SFProcessor() { stopTimer(); }

juce::AudioProcessorValueTreeState::ParameterLayout SFProcessor::makeLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (auto& s : specs)
    {
        const juce::ParameterID pid (s.id, 1);
        switch (s.type)
        {
            case PType::Float:
            {
                juce::NormalisableRange<float> r (s.min, s.max);
                if (! std::isnan (s.centre) && s.centre > s.min && s.centre < s.max) r.setSkewForCentre (s.centre);
                const auto unit = s.unit;
                auto toText = [unit] (float v, int)
                {
                    if (unit == "%")   return juce::String (juce::roundToInt (v * 100.0f)) + " %";
                    if (unit == "SLOT") return std::abs (v - 0.5f) < 0.04f ? juce::String ("AUTO")
                                             : v < 0.5f ? "SHARE " + juce::String (juce::roundToInt (juce::jlimit (0.0f, 1.0f, (0.46f - v) / 0.46f) * 100.0f)) + " %"
                                                        : "HAND " + juce::String (juce::roundToInt (juce::jlimit (0.0f, 1.0f, (v - 0.54f) / 0.46f) * 100.0f)) + " %";
                    if (unit == "BIP") return std::abs (v - 0.5f) < 0.02f ? juce::String ("OFF") : (v < 0.5f ? "LP " : "HP ") + juce::String (juce::roundToInt (std::abs (v - 0.5f) * 200.0f));
                    if (unit == "Hz")  return v >= 1000.0f ? juce::String (v / 1000.0f, 2) + " kHz" : juce::String (juce::roundToInt (v)) + " Hz";
                    if (unit == "ms")  return v >= 1000.0f ? juce::String (v / 1000.0f, 2) + " s" : juce::String (v, v < 10.0f ? 1 : 0) + " ms";
                    if (unit == "dB")  return juce::String (v, 1) + " dB";
                    if (unit == "st")  return juce::String (v, 1) + " st";
                    if (unit == "s")   return juce::String (v, 2) + " s";
                    if (unit == "LUFS")return juce::String (v, 1) + " LUFS";
                    return juce::String (v, 2) + (unit.empty() ? "" : " " + juce::String (unit));
                };
                layout.add (std::make_unique<juce::AudioParameterFloat> (pid, s.name, r, s.def,
                    juce::AudioParameterFloatAttributes().withStringFromValueFunction (toText)));
                break;
            }
            case PType::Choice:
            {
                juce::StringArray c;
                for (auto& x : s.choices) c.add (x);
                layout.add (std::make_unique<juce::AudioParameterChoice> (pid, s.name, c, (int) s.def));
                break;
            }
            case PType::Toggle:
            case PType::Momentary:
                layout.add (std::make_unique<juce::AudioParameterBool> (pid, s.name, s.def > 0.5f));
                break;
        }
    }
    return layout;
}

bool SFProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    if (l.getMainOutputChannelSet() != juce::AudioChannelSet::stereo()) return false;
    if (! module->instrument() && l.getMainInputChannelSet() != juce::AudioChannelSet::stereo()) return false;
    for (int i = 1; i < l.outputBuses.size(); ++i)
        if (! l.outputBuses[i].isDisabled() && l.outputBuses[i] != juce::AudioChannelSet::stereo()) return false;
    for (int i = module->instrument() ? 0 : 1; i < l.inputBuses.size(); ++i)
    {
        const auto& s = l.inputBuses[i];
        if (! s.isDisabled() && s != juce::AudioChannelSet::stereo() && s != juce::AudioChannelSet::mono()) return false;
    }
    return true;
}

void SFProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    module->prepare (sampleRate, samplesPerBlock);
    setLatencySamples (module->latency());
    extraScratch.setSize (2 * std::max<int> (1, (int) module->extraOutputs().size()), samplesPerBlock);
    sideScratch.setSize (2, samplesPerBlock);
    firstBlock = true;
}

void SFProcessor::storeSong()
{
    storeRequested = true;
}

void SFProcessor::applySong (int s)
{
    // Audio thread. The recalled values take effect now (ovr); the timer then tells Live, and
    // the override drops once Live's value matches.
    if (s < 0 || s >= kSongs || ! songHas[(size_t) s].load()) { songIndex = s; return; }
    for (size_t i = 0; i < specs.size(); ++i)
    {
        if (! specs[i].song) continue;
        ovr[i] = songVals[(size_t) s][i];
        ovrAge[i] = 0;
    }
    songIndex = s;
    notifyHost = true;
}

void SFProcessor::flushHostNotifications()
{
    if (! notifyHost.exchange (false)) return;
    for (size_t i = 0; i < specs.size(); ++i)
    {
        const float v = ovr[i];
        if (std::isnan (v)) continue;
        auto* p = params[i];
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (v));
        p->endChangeGesture();
    }
}

void SFProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    auto& bus = sf::Bus::get();

    // Transport. When stopped, a wall clock stands in so tempo-synced motion keeps running
    // and every SF plugin agrees on the beat.
    Ctx c;
    c.sr = sr; c.n = n; c.bus = &bus;
    c.trackName = trackName; c.trackNameSeq = trackNameSeq.load();
    c.bpm = lastBpm;
    bool havePos = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) { c.bpm = juce::jlimit (20.0, 400.0, *b); lastBpm = c.bpm; }
            if (auto ts = pos->getTimeSignature()) c.beatsPerBar = ts->numerator * 4.0 / std::max (1, ts->denominator);
            c.playing = pos->getIsPlaying();
            if (c.playing)
                if (auto q = pos->getPpqPosition()) { c.ppq = *q; havePos = true; }
        }
    if (! havePos)
    {
        // Stopped: continue from where Live stopped (or, at first use, from a wall-clock
        // anchor shared by every SF plugin) and advance by whole blocks: never jumps.
        if (! clockAnchored)
        {
            using namespace std::chrono;
            const double secs = duration<double> (steady_clock::now().time_since_epoch()).count();
            internalPpq = std::fmod (secs, 3600.0) * c.bpm / 60.0;
            clockAnchored = true;
        }
        c.ppq = internalPpq;
    }
    internalPpq = c.ppq + n * c.beatsPerSample();
    clockAnchored = true;

    // Song recall + store requests from SF CONTROL (or this plugin's STORE button).
    auto& d = bus.d();
    if (! seqInit) { lastSongSeq = d.songSeq.load(); lastStoreSeq = d.storeSeq.load(); seqInit = true; if (songIndex.load() < 0) songIndex = d.song.load(); }
    if (d.songSeq.load() != lastSongSeq)
    {
        lastSongSeq = d.songSeq.load();
        pendingSong = d.song.load();
    }
    if (d.storeSeq.load() != lastStoreSeq) { lastStoreSeq = d.storeSeq.load(); storeRequested = true; }
    if (storeRequested.exchange (false))
    {
        // Store into the selected song (the bus song), even if its recall is still waiting for the bar.
        int s = d.song.load();
        if (s < 0) s = std::max (0, songIndex.load());
        songIndex = s;
        for (size_t i = 0; i < specs.size(); ++i) songVals[(size_t) s][i] = raw[i]->load();
        songHas[(size_t) s] = true;
    }
    if (const int ps = pendingSong.load(); ps >= 0)
    {
        // On the next bar line while playing, at once when stopped.
        const double bar = c.beatsPerBar;
        const double next = std::ceil (c.ppq / bar - 1e-9) * bar;
        if (! c.playing || (next - c.ppq) * c.samplesPerBeat() < n)
        {
            applySong (ps);
            pendingSong = -1;
        }
    }

    // Parameter values: target -> macro -> glide or smoothing.
    const int glideIdx = numModParams + SYS_GLIDE;
    const double gBars = glideBars ((int) raw[(size_t) glideIdx]->load());
    const int mSrc = (int) raw[(size_t) (numModParams + SYS_MACRO_SRC)]->load();
    const float mAmt = raw[(size_t) (numModParams + SYS_MACRO_AMT)]->load();
    const float macro = mSrc > 0 ? d.macro[mSrc - 1].load() : 0.0f;
    const float smooth = std::exp (-(float) n / (float) (0.03 * sr));
    const double blockBeats = n * c.beatsPerSample();
    for (size_t i = 0; i < specs.size(); ++i)
    {
        const auto& s = specs[i];
        prev[i] = cur[i];
        float t = raw[i]->load();
        if (! std::isnan (ovr[i]))
        {
            // Live has caught up, or 3 s passed without it (automation wins): drop the override.
            if (std::abs (t - ovr[i]) < 1e-4f * (1.0f + std::abs (t)) || (ovrAge[i] += n) > (int) (3.0 * sr)) ovr[i] = NAN;
            else t = ovr[i];
        }
        if (s.type != PType::Float) { cur[i] = t; continue; }
        const float range = s.max - s.min;
        // SLOW: performance-row knobs travel to a new setting over exactly N bars, however far.
        if (gBars > 0.0 && s.glide && s.primary && (int) i < numModParams && ! firstBlock)
        {
            if (std::isnan (glideTo[i]) || std::abs (t - glideTo[i]) > 1e-5f * (1.0f + std::abs (range)))
            {
                glideFrom[i] = base[i];
                glideTo[i] = t;
                glideElapsed[i] = 0.0;
            }
            glideElapsed[i] += blockBeats;
            const double total = gBars * c.beatsPerBar;
            const float k = (float) std::min (1.0, glideElapsed[i] / total);
            base[i] = glideFrom[i] + (glideTo[i] - glideFrom[i]) * k;
        }
        else
        {
            glideTo[i] = NAN;
            base[i] = firstBlock ? t : t + smooth * (base[i] - t);
        }
        // MACRO is added after the glide, so macro moves stay immediate.
        float v = base[i];
        if (s.macroDir != 0 && (int) i < numModParams)
        {
            macroOff[i] = (macro * mAmt * (float) s.macroDir * range) + smooth * (macroOff[i] - macro * mAmt * (float) s.macroDir * range);
            v = juce::jlimit (s.min, s.max, v + macroOff[i]);
        }
        cur[i] = v;
    }
    firstBlock = false;
    c.p = cur.data();
    c.p0 = prev.data();

    // Buses
    auto mainOut = getBusBuffer (buffer, false, 0);
    juce::AudioBuffer<float>* side = nullptr;
    juce::AudioBuffer<float> sideView;
    const int sideBus = module->instrument() ? 0 : 1;
    if (module->sidechain() && getBusCount (true) > sideBus && getBus (true, sideBus)->isEnabled())
    {
        sideView = getBusBuffer (buffer, true, sideBus);
        if (sideView.getNumChannels() > 0) side = &sideView;
    }
    juce::AudioBuffer<float>* extra = nullptr;
    juce::AudioBuffer<float> extraView;
    const int numExtra = (int) module->extraOutputs().size();
    if (numExtra > 0)
    {
        // Gather enabled extra outputs into one buffer view (2 channels each); disabled ones use scratch.
        extraScratch.setSize (2 * numExtra, n, false, false, true);
        extraScratch.clear();
        extra = &extraScratch;
    }
    if (module->instrument()) mainOut.clear();

    module->process (c, mainOut, side, extra, midi);

    if (extra != nullptr)
    {
        // Same safety stage as the main output.
        for (int ch = 0; ch < extraScratch.getNumChannels(); ++ch)
        {
            auto* x = extraScratch.getWritePointer (ch);
            for (int i = 0; i < n; ++i) x[i] = std::isfinite (x[i]) ? juce::jlimit (-4.0f, 4.0f, x[i]) : 0.0f;
        }
    }
    if (extra != nullptr)
        for (int e = 0; e < numExtra; ++e)
            if (getBusCount (false) > e + 1 && getBus (false, e + 1)->isEnabled())
            {
                auto out = getBusBuffer (buffer, false, e + 1);
                for (int ch = 0; ch < std::min (2, out.getNumChannels()); ++ch)
                    out.copyFrom (ch, 0, extraScratch, e * 2 + ch, 0, n);
            }

    // Safety: never pass NaN/Inf, never exceed +12 dBFS.
    bool bad = false;
    for (int ch = 0; ch < mainOut.getNumChannels(); ++ch)
    {
        auto* x = mainOut.getWritePointer (ch);
        for (int i = 0; i < n; ++i)
        {
            if (! std::isfinite (x[i])) { bad = true; x[i] = 0.0f; }
            else x[i] = juce::jlimit (-4.0f, 4.0f, x[i]);
        }
    }
    if (bad) { mainOut.clear(); module->reset(); }
    outMeter.push (mainOut.getArrayOfReadPointers(), mainOut.getNumChannels(), n);
    if (outMeter.peak > 0.999f) clipped = true;
    // Clear any remaining output channels (unused buses).
    for (int ch = mainOut.getNumChannels(); ch < getTotalNumOutputChannels(); ++ch)
        if (numExtra == 0) buffer.clear (ch, 0, n);
}

void SFProcessor::timerCallback() { flushHostNotifications(); }

void SFProcessor::updateTrackProperties (const TrackProperties& p)
{
    // Live (VST3) tells the plugin its track name; ALLOC lists sources by it.
    char buf[24] {};
    p.name.toUpperCase().copyToUTF8 (buf, 23);
    std::memcpy (trackName, buf, sizeof (buf));
    trackNameSeq.fetch_add (1);
}

void SFProcessor::getStateInformation (juce::MemoryBlock& mb)
{
    juce::ValueTree root ("SFSTATE");
    root.setProperty ("module", juce::String (module->id()), nullptr);
    root.setProperty ("version", 1, nullptr);
    root.appendChild (apvts.copyState(), nullptr);
    juce::ValueTree songs ("SONGS");
    songs.setProperty ("active", songIndex.load(), nullptr);
    for (int s = 0; s < kSongs; ++s)
    {
        if (! songHas[(size_t) s].load()) continue;
        juce::ValueTree t ("SONG");
        t.setProperty ("index", s, nullptr);
        for (size_t i = 0; i < specs.size(); ++i)
            if (specs[i].song) t.setProperty (juce::Identifier (specs[i].id), songVals[(size_t) s][i], nullptr);
        songs.appendChild (t, nullptr);
    }
    root.appendChild (songs, nullptr);
    juce::ValueTree m ("MODULE");
    module->save (m);
    root.appendChild (m, nullptr);
    juce::MemoryOutputStream os (mb, false);
    root.writeToStream (os);
}

void SFProcessor::setStateInformation (const void* data, int size)
{
    auto root = juce::ValueTree::readFromData (data, (size_t) size);
    if (! root.isValid() || root.getType().toString() != "SFSTATE") return;
    auto p = root.getChildWithName ("PARAMS");
    if (p.isValid()) apvts.replaceState (p);
    auto songs = root.getChildWithName ("SONGS");
    for (auto& h : songHas) h = false;
    if (songs.isValid())
    {
        songIndex = (int) songs.getProperty ("active", -1);
        for (auto t : songs)
        {
            const int s = (int) t.getProperty ("index", -1);
            if (s < 0 || s >= kSongs) continue;
            for (size_t i = 0; i < specs.size(); ++i)
                songVals[(size_t) s][i] = t.hasProperty (juce::Identifier (specs[i].id)) ? (float) t.getProperty (juce::Identifier (specs[i].id)) : specs[i].def;
            songHas[(size_t) s] = true;
        }
    }
    auto m = root.getChildWithName ("MODULE");
    if (m.isValid()) module->load (m);
    firstBlock = true;
}

juce::AudioProcessorEditor* SFProcessor::createEditor() { return new SFEditor (*this); }
