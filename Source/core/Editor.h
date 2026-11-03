#pragma once
// SFEditor: one generic editor for every module.
//   HEADER   SF <MODULE> | MODEL | PRESET | SONG | STORE | LOAD | OUT meter (FAULT)
//   PRIMARY  8 large controls (the E16 row / Live's device view)
//   DISPLAY  module-drawn panel
//   SECOND   remaining controls, 8 per row
//   MOTION   M1 / M2 SOURCE, RATE, TARGET, DEPTH
//   SYSTEM   SLOW, MACRO SRC, MACRO AMT
#include "Processor.h"
#include "Ui.h"
#include "Sample.h"

class SFLookAndFeel : public juce::LookAndFeel_V4
{
public:
    SFLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool over, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override { return sf::ui::mono (12.0f); }
    juce::Font getPopupMenuFont() override { return sf::ui::mono (12.0f); }
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
};

class SFCell;

class SFEditor : public juce::AudioProcessorEditor,
                 public juce::FileDragAndDropTarget,
                 private juce::Timer
{
public:
    explicit SFEditor (SFProcessor&);
    ~SFEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

    bool isInterestedInFileDrag (const juce::StringArray&) override;
    void filesDropped (const juce::StringArray&, int, int) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { dragHover = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override { dragHover = false; repaint(); }

    static constexpr int kW = 928, kHeader = 40, kPrimaryH = 128, kSecondH = 92, kCols = 8, kMargin = 16, kSystemTitle = 16;

private:
    void timerCallback() override;
    juce::Rectangle<int> displayArea() const;
    int systemTop() const;
    void showPresetMenu();
    void updatePresetLabel();

    SFProcessor& proc;
    SFLookAndFeel lnf;
    std::vector<std::unique_ptr<SFCell>> primary, secondary, motionCells, systemCells;
    juce::ComboBox modelBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modelAttach;
    juce::TextButton storeButton { "STORE" }, loadButton { "LOAD" }, presetButton { "PRESET" };
    juce::Array<juce::File> userFiles;
    std::unique_ptr<juce::FileChooser> chooser;
    bool dragHover = false;
    int numSecondRows = 0;
};
