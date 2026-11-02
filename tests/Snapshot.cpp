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
    for (auto& id : sf::allModuleIds())
    {
        SFProcessor p (sf::makeModule (id));
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
