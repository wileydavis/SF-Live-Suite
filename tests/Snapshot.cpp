// SFSnapshot: renders every module's editor to docs/snapshots/<ID>.png (after a short run of
// audio so meters and displays show something).
#include "core/Processor.h"
#include "core/Registry.h"
#include <juce_gui_basics/juce_gui_basics.h>

struct Head : juce::AudioPlayHead
{
    double ppq = 0;
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p; p.setBpm (124.0); p.setPpqPosition (ppq); p.setIsPlaying (true); p.setTimeSignature (TimeSignature { 4, 4 }); return p;
    }
};

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    juce::File dir = juce::File::getCurrentWorkingDirectory().getChildFile (argc > 1 ? argv[1] : "docs/snapshots");
    dir.createDirectory();
    // Sources for the ALLOC picture: four taps with different content.
    std::vector<std::unique_ptr<SFProcessor>> taps;
    const int roles[] = { sf::ROLE_KICK, sf::ROLE_BASS, sf::ROLE_LEAD, sf::ROLE_PAD };
    const float freqs[] = { 55.0f, 110.0f, 330.0f, 180.0f };
    Head th;
    for (int k = 0; k < 4; ++k)
    {
        taps.push_back (std::make_unique<SFProcessor> (sf::makeModule ("TAP")));
        taps.back()->setPlayHead (&th);
        taps.back()->prepareToPlay (48000.0, 256);
        auto* r = taps.back()->apvts.getParameter ("role"); r->setValueNotifyingHost (r->convertTo0to1 ((float) roles[k]));
    }
    auto runTaps = [&] (int block)
    {
        for (int k = 0; k < 4; ++k)
        {
            juce::AudioBuffer<float> b (2, 256); juce::MidiBuffer m;
            for (int i = 0; i < 256; ++i) { const float v = 0.3f * std::sin (6.2831853f * freqs[k] * (float) (block * 256 + i) / 48000.0f) + 0.1f * std::sin (6.2831853f * freqs[k] * 2.1f * (float) (block * 256 + i) / 48000.0f); b.setSample (0, i, v); b.setSample (1, i, v); }
            taps[(size_t) k]->processBlock (b, m);
        }
    };
    for (auto& id : sf::allModuleIds())
    {
        SFProcessor p (sf::makeModule (id));
        if (id == "ALLOC")
            for (auto pr : { std::pair<const char*, float> { "depth2", 0.7f }, { "q2", 0.8f }, { "pri2", 0.3f }, { "depth4", 1.0f }, { "q4", 0.7f }, { "pri4", 0.0f }, { "depth6", 0.5f }, { "pri6", 0.8f } })
                { auto* prm = p.apvts.getParameter (pr.first); prm->setValueNotifyingHost (prm->convertTo0to1 (pr.second)); }
        Head h; p.setPlayHead (&h);
        p.enableAllBuses();
        p.prepareToPlay (48000.0, 256);
        juce::AudioBuffer<float> b (std::max (p.getTotalNumInputChannels(), p.getTotalNumOutputChannels()), 256);
        for (int k = 0; k < 400; ++k)
        {
            juce::MidiBuffer m;
            if (k % 23 == 0) m.addEvent (juce::MidiMessage::noteOn (1, 36 + (k / 23) % 12, (juce::uint8) 110), 0);
            if (k % 23 == 10) m.addEvent (juce::MidiMessage::noteOff (1, 36 + (k / 23) % 12), 0);
            for (int c = 0; c < b.getNumChannels(); ++c) for (int i = 0; i < 256; ++i) b.setSample (c, i, 0.3f * std::sin (0.01f * (float) (k * 256 + i)) * ((k % 23) < 4 ? 1.0f : 0.2f));
            if (id == "ALLOC") runTaps (k);
            p.processBlock (b, m);
            h.ppq += 256 * 124.0 / 60.0 / 48000.0;
        }
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        ed->setVisible (true);
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        juce::File f = dir.getChildFile (juce::String (id) + ".png");
        f.deleteFile();
        juce::FileOutputStream os (f);
        juce::PNGImageFormat().writeImageToStream (img, os);
        std::printf ("%s %dx%d\n", id.c_str(), img.getWidth(), img.getHeight());
    }
    return 0;
}
