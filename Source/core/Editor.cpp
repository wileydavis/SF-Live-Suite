#include "Editor.h"

using namespace sf;

// ---------------------------------------------------------------- look and feel
SFLookAndFeel::SFLookAndFeel()
{
    setColour (juce::ComboBox::backgroundColourId, ui::kBg);
    setColour (juce::ComboBox::textColourId, ui::kText);
    setColour (juce::ComboBox::outlineColourId, ui::kLine);
    setColour (juce::ComboBox::arrowColourId, ui::kDim);
    setColour (juce::PopupMenu::backgroundColourId, ui::kPanel);
    setColour (juce::PopupMenu::textColourId, ui::kText);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, ui::kText);
    setColour (juce::PopupMenu::highlightedTextColourId, ui::kBg);
    setColour (juce::TextButton::buttonColourId, ui::kBg);
    setColour (juce::TextButton::buttonOnColourId, ui::kText);
    setColour (juce::TextButton::textColourOffId, ui::kText);
    setColour (juce::TextButton::textColourOnId, ui::kBg);
}

void SFLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end, juce::Slider& s)
{
    const auto r = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.0f);
    const float radius = std::min (r.getWidth(), r.getHeight()) * 0.5f;
    const auto c = r.getCentre();
    const float thick = std::max (2.0f, radius * 0.12f);
    const bool touched = s.isMouseButtonDown() || (bool) s.getProperties().getWithDefault ("hot", false);
    const bool bipolar = (bool) s.getProperties().getWithDefault ("bipolar", false);

    juce::Path track;
    track.addCentredArc (c.x, c.y, radius - thick, radius - thick, 0.0f, start, end, true);
    g.setColour (ui::kTrack);
    g.strokePath (track, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::butt));

    const float from = bipolar ? (start + end) * 0.5f : start;
    const float to = start + pos * (end - start);
    juce::Path val;
    val.addCentredArc (c.x, c.y, radius - thick, radius - thick, 0.0f, std::min (from, to), std::max (from, to), true);
    g.setColour (touched ? ui::kAccent : ui::kText);
    g.strokePath (val, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::butt));

    // Pointer line.
    const float inner = radius * 0.25f, outer = radius - thick * 2.2f;
    const juce::Point<float> a (c.x + inner * std::sin (to), c.y - inner * std::cos (to));
    const juce::Point<float> b (c.x + outer * std::sin (to), c.y - outer * std::cos (to));
    g.setColour (touched ? ui::kAccent : ui::kDim);
    g.drawLine ({ a, b }, 1.5f);
}

void SFLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState() || down;
    const auto onColour = b.getProperties().contains ("onColour") ? juce::Colour ((juce::uint32) (juce::int64) b.getProperties()["onColour"]) : ui::kText;
    g.setColour (on ? onColour : ui::kBg);
    g.fillRect (r);
    g.setColour (over && ! on ? ui::kDim : ui::kLine);
    g.drawRect (r, 1.0f);
}

void SFLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool down)
{
    const bool on = b.getToggleState() || down;
    g.setColour (on ? ui::kBg : ui::kText);
    g.setFont (ui::mono (11.0f, true));
    g.drawText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred, false);
}

void SFLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    g.setColour (ui::kBg);
    g.fillRect (r);
    g.setColour (box.hasKeyboardFocus (false) ? ui::kDim : ui::kLine);
    g.drawRect (r, 1.0f);
    juce::Path p;
    const float ax = (float) w - 12.0f, ay = (float) h * 0.5f;
    p.addTriangle (ax - 4, ay - 2, ax + 4, ay - 2, ax, ay + 3);
    g.setColour (ui::kDim);
    g.fillPath (p);
}

void SFLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& l)
{
    l.setBounds (4, 0, box.getWidth() - 20, box.getHeight());
    l.setFont (getComboBoxFont (box));
    l.setColour (juce::Label::textColourId, ui::kText);
}

// ---------------------------------------------------------------- cells
class SFCell : public juce::Component, private juce::Timer
{
public:
    SFCell (SFProcessor& p, int index, bool big) : proc (p), idx (index), large (big)
    {
        const auto& s = p.allSpecs()[(size_t) idx];
        name = s.name;
        auto& ap = p.apvts;
        if (s.type == PType::Float)
        {
            slider = std::make_unique<juce::Slider> (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox);
            slider->setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
            slider->setMouseDragSensitivity (large ? 260 : 200);
            slider->setVelocityModeParameters (1.0, 1, 0.0, false, juce::ModifierKeys::shiftModifier);
            slider->getProperties().set ("bipolar", s.unit == "BIP" || s.unit == "SLOT" || (s.min < 0 && s.max > 0));
            sAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (ap, s.id, *slider);
            slider->setDoubleClickReturnValue (true, s.def);
            addAndMakeVisible (*slider);
            slider->onValueChange = [this] { hotUntil = juce::Time::getMillisecondCounter() + 700; };
        }
        else if (s.type == PType::Choice)
        {
            box = std::make_unique<juce::ComboBox>();
            for (size_t i = 0; i < s.choices.size(); ++i) box->addItem (s.choices[i], (int) i + 1);
            cAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (ap, s.id, *box);
            addAndMakeVisible (*box);
        }
        else
        {
            button = std::make_unique<juce::TextButton> (s.name);
            button->setClickingTogglesState (s.type == PType::Toggle);
            if (s.type == PType::Toggle)
                bAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (ap, s.id, *button);
            else
            {
                auto* prm = p.param (idx);
                button->onStateChange = [this, prm]
                {
                    const bool down = button->isDown();
                    if (down != wasDown)
                    {
                        wasDown = down;
                        prm->beginChangeGesture();
                        prm->setValueNotifyingHost (down ? 1.0f : 0.0f);
                        prm->endChangeGesture();
                    }
                };
            }
            addAndMakeVisible (*button);
        }
        startTimerHz (20);
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        ui::label (g, name, r.removeFromTop (16.0f), ui::kDim, large ? 11.0f : 10.0f, juce::Justification::centred);
        if (slider != nullptr)
        {
            auto* prm = proc.param (idx);
            const bool hot = slider->isMouseButtonDown() || juce::Time::getMillisecondCounter() < hotUntil;
            ui::label (g, prm->getCurrentValueAsText(), r.removeFromBottom (16.0f), hot ? ui::kAccent : ui::kText,
                       large ? 12.0f : 10.5f, juce::Justification::centred);
            // Glide target marker: shows where a gliding value is heading.
            const float target = prm->getValue();
            const float now = prm->convertTo0to1 (proc.currentValue (idx));
            if (std::abs (target - now) > 0.01f)
                ui::label (g, "GLIDE " + juce::String (juce::roundToInt (now * 100)) + ">" + juce::String (juce::roundToInt (target * 100)),
                           getLocalBounds().toFloat().removeFromBottom (30.0f).removeFromTop (14.0f), ui::kGreen, 9.0f, juce::Justification::centred);
        }
    }

    void resized() override
    {
        auto r = getLocalBounds().withTrimmedTop (16);
        if (slider != nullptr) { r.removeFromBottom (16); slider->setBounds (r.withSizeKeepingCentre (std::min (r.getWidth(), r.getHeight()), std::min (r.getWidth(), r.getHeight()))); }
        if (box != nullptr) box->setBounds (r.reduced (4).withSizeKeepingCentre (r.getWidth() - 8, 24));
        if (button != nullptr) button->setBounds (r.reduced (4).withSizeKeepingCentre (r.getWidth() - 8, 26));
    }

private:
    void timerCallback() override
    {
        if (slider != nullptr)
        {
            slider->getProperties().set ("hot", juce::Time::getMillisecondCounter() < hotUntil);
            repaint();
        }
    }

    SFProcessor& proc;
    int idx;
    bool large;
    juce::String name;
    bool wasDown = false;
    juce::uint32 hotUntil = 0;
    std::unique_ptr<juce::Slider> slider;
    std::unique_ptr<juce::ComboBox> box;
    std::unique_ptr<juce::TextButton> button;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> cAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bAttach;
};

// ---------------------------------------------------------------- editor
SFEditor::SFEditor (SFProcessor& p) : juce::AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&lnf);
    const auto& specs = p.allSpecs();
    int modelIdx = -1;
    for (int i = 0; i < (int) specs.size(); ++i)
    {
        if (specs[(size_t) i].id == "model") { modelIdx = i; continue; }
        auto cell = std::make_unique<SFCell> (p, i, specs[(size_t) i].primary);
        addAndMakeVisible (*cell);
        (specs[(size_t) i].primary ? primary : secondary).push_back (std::move (cell));
    }
    if (modelIdx >= 0)
    {
        for (size_t i = 0; i < specs[(size_t) modelIdx].choices.size(); ++i) modelBox.addItem (specs[(size_t) modelIdx].choices[i], (int) i + 1);
        modelAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, "model", modelBox);
        addAndMakeVisible (modelBox);
    }
    storeButton.onClick = [this] { proc.storeSong(); };
    addAndMakeVisible (storeButton);
    if (p.mod().acceptsFiles())
    {
        loadButton.onClick = [this]
        {
            chooser = std::make_unique<juce::FileChooser> ("LOAD", juce::File(), "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg;*.m4a");
            chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::canSelectMultipleItems,
                                  [this] (const juce::FileChooser& fc)
                                  {
                                      juce::StringArray paths;
                                      for (auto& f : fc.getResults()) paths.add (f.getFullPathName());
                                      if (! paths.isEmpty()) proc.mod().loadFiles (paths);
                                  });
        };
        addAndMakeVisible (loadButton);
    }
    numSecondRows = (int) (secondary.size() + kCols - 1) / kCols;
    const int dh = p.mod().displayHeight();
    setSize (kW, kHeader + kPrimaryH + (dh > 0 ? dh + 12 : 0) + numSecondRows * kSecondH + kMargin);
    startTimerHz (30);
}

SFEditor::~SFEditor() { setLookAndFeel (nullptr); }

juce::Rectangle<int> SFEditor::displayArea() const
{
    const int dh = proc.mod().displayHeight();
    if (dh <= 0) return {};
    return { kMargin, kHeader + kPrimaryH + 4, getWidth() - 2 * kMargin, dh };
}

void SFEditor::paint (juce::Graphics& g)
{
    g.fillAll (ui::kBg);
    auto hdr = getLocalBounds().removeFromTop (kHeader).toFloat();
    g.setColour (ui::kLine);
    g.drawHorizontalLine (kHeader - 1, 0.0f, (float) getWidth());
    ui::label (g, "SF " + juce::String (proc.mod().id()), hdr.withTrimmedLeft ((float) kMargin).withWidth (200), ui::kText, 16.0f);

    // Song indicator.
    const int s = proc.activeSong();
    const juce::String song = s < 0 ? "SONG --" : "SONG " + juce::String (s + 1).paddedLeft ('0', 2) + (proc.songStored (s) ? "" : " EMPTY");
    ui::label (g, song + (proc.recallWaiting() ? " WAIT BAR" : ""), { 470.0f, 0.0f, 170.0f, (float) kHeader },
               proc.recallWaiting() ? ui::kGreen : ui::kDim, 11.0f);

    // Output meter.
    juce::Rectangle<float> m ((float) getWidth() - 150.0f, 15.0f, 110.0f, 8.0f);
    ui::levelBar (g, m, proc.outMeter.peak);
    ui::label (g, "OUT", m.withX (m.getRight() + 6).withWidth (30).withHeight (8), proc.clipped.load() ? ui::kRed : ui::kFaint, 9.0f);

    // Primary row frame.
    g.setColour (ui::kLine);
    g.drawHorizontalLine (kHeader + kPrimaryH, 0.0f, (float) getWidth());

    if (auto d = displayArea(); ! d.isEmpty())
    {
        g.saveState();
        g.reduceClipRegion (d);
        proc.mod().paint (g, d.toFloat());
        g.restoreState();
        g.setColour (ui::kLine);
        g.drawHorizontalLine (d.getBottom() + 6, 0.0f, (float) getWidth());
    }
    if (dragHover)
    {
        g.setColour (ui::kText);
        g.drawRect (getLocalBounds().reduced (2), 2);
        ui::label (g, proc.mod().fileHint(), getLocalBounds().toFloat().removeFromBottom (24), ui::kText, 12.0f, juce::Justification::centred);
    }
}

void SFEditor::resized()
{
    int x = kMargin + 220;
    if (modelBox.isVisible()) { modelBox.setBounds (x, 8, 220, 24); }
    storeButton.setBounds (640, 8, 64, 24);
    loadButton.setBounds (710, 8, 56, 24);
    const int cw = (getWidth() - 2 * kMargin) / kCols;
    for (size_t i = 0; i < primary.size(); ++i)
        primary[i]->setBounds (kMargin + (int) i * cw, kHeader + 6, cw, kPrimaryH - 10);
    const int dh = proc.mod().displayHeight();
    const int y0 = kHeader + kPrimaryH + (dh > 0 ? dh + 12 : 0);
    for (size_t i = 0; i < secondary.size(); ++i)
        secondary[i]->setBounds (kMargin + (int) (i % kCols) * cw, y0 + (int) (i / kCols) * kSecondH + 4, cw, kSecondH - 6);
}

void SFEditor::mouseDown (const juce::MouseEvent& e)
{
    if (auto d = displayArea(); d.contains (e.getPosition()))
        proc.mod().displayMouse (e.position, d.toFloat(), true);
    if (e.y < kHeader && e.x > getWidth() - 160) proc.clipped = false;
}

void SFEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (auto d = displayArea(); d.contains (e.getPosition()))
        proc.mod().displayMouse (e.position, d.toFloat(), false);
}

bool SFEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    if (! proc.mod().acceptsFiles()) return false;
    for (auto& f : files) if (isAudioFile (f)) return true;
    return false;
}

void SFEditor::filesDropped (const juce::StringArray& files, int, int)
{
    dragHover = false;
    juce::StringArray audio;
    for (auto& f : files) if (isAudioFile (f)) audio.add (f);
    proc.mod().loadFiles (audio);
    repaint();
}

void SFEditor::timerCallback() { repaint (0, 0, getWidth(), kHeader); if (auto d = displayArea(); ! d.isEmpty()) repaint (d); }
