#pragma once
// Visual standard: black, white and greys. Colour only carries information:
//   RED = clip / fault, AMBER = conflict / warning, GREEN = active / locked to grid,
//   CYAN = the value of the control being touched.
#include <juce_gui_basics/juce_gui_basics.h>
#include <atomic>

namespace sf::ui
{
const juce::Colour kBg       { 0xff000000 };
const juce::Colour kPanel    { 0xff0b0b0b };
const juce::Colour kLine     { 0xff2a2a2a };
const juce::Colour kGrid     { 0xff171717 };
const juce::Colour kText     { 0xffe8e8e8 };
const juce::Colour kDim      { 0xff8a8a8a };
const juce::Colour kFaint    { 0xff4a4a4a };
const juce::Colour kTrack    { 0xff2e2e2e };
const juce::Colour kAccent   { 0xff00c8ff };
const juce::Colour kRed      { 0xffff3b30 };
const juce::Colour kAmber    { 0xffffb000 };
const juce::Colour kGreen    { 0xff30d158 };

inline juce::Font mono (float h, bool bold = false)
{
    return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), h, bold ? juce::Font::bold : juce::Font::plain));
}

inline void label (juce::Graphics& g, const juce::String& s, juce::Rectangle<float> r, juce::Colour c = kDim,
                   float h = 11.0f, juce::Justification j = juce::Justification::centredLeft)
{
    g.setColour (c);
    g.setFont (mono (h));
    g.drawText (s, r, j, false);
}

inline void frame (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& title = {})
{
    g.setColour (kLine);
    g.drawRect (r, 1.0f);
    if (title.isNotEmpty()) label (g, title, r.reduced (6, 2).removeFromTop (14), kFaint, 10.0f);
}

// Horizontal level bar, dB scale -60..+6. Amber above -6 dBFS, red above -0.3.
inline void levelBar (juce::Graphics& g, juce::Rectangle<float> r, float linear)
{
    const float db = 20.0f * std::log10 (std::max (linear, 1.0e-6f));
    const float t = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 66.0f);
    g.setColour (kTrack);
    g.fillRect (r);
    g.setColour (db > -0.3f ? kRed : db > -6.0f ? kAmber : kText);
    g.fillRect (r.withWidth (r.getWidth() * t));
}

// Plain 0..1 value bar (no level colours).
inline void valueBar (juce::Graphics& g, juce::Rectangle<float> r, float v)
{
    g.setColour (kTrack);
    g.fillRect (r);
    g.setColour (kText);
    g.fillRect (r.withWidth (r.getWidth() * juce::jlimit (0.0f, 1.0f, v)));
}

// Ring-buffer trace (values 0..1), newest at the right.
template <int N>
inline void trace (juce::Graphics& g, juce::Rectangle<float> r, const std::atomic<float> (&v)[N], int writePos, juce::Colour c)
{
    juce::Path p;
    for (int i = 0; i < N; ++i)
    {
        const float val = juce::jlimit (0.0f, 1.0f, v[(writePos + i) % N].load());
        const float x = r.getX() + r.getWidth() * (float) i / (float) (N - 1);
        const float y = r.getBottom() - val * r.getHeight();
        if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
    }
    g.setColour (c);
    g.strokePath (p, juce::PathStrokeType (1.2f));
}
} // namespace sf::ui
