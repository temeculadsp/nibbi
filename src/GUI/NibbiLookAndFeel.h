#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace nibbi::gui
{
// Vector controls and solid faceplates, with optional supplied PNG faceplates.
namespace ink
{
inline const juce::Colour paper { 0xffeeeede }, gold { 0xffd9c883 };
inline const juce::Colour purple { 0xffb08bdd }, mint { 0xff81edba };
inline const juce::Colour yellow { 0xfff2df79 }, coral { 0xffe9808c };
inline const juce::Colour cyan { 0xff7ad9e7 }, blue { 0xff8787e8 };
}
// Nibbi's control finishes; independent of the firmware's functional LED colours.
namespace palette
{
inline const juce::Colour wheel { 0xff9dcfe5 }, knobCap { 0xffbcd9cd };
inline const juce::Colour sampleButton { 0xffa9b7ed }, playButton { 0xffc9dda1 };
inline const juce::Colour recordButton { 0xffdfafcc };
inline const juce::Colour noteKey { 0xffd2e1ed }, modeKey { 0xffa29abd };
inline const juce::Colour glyph { 0xff303044 };
}

class NibbiLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics&, int, int, int, int, float, float, float,
                           juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&,
                               bool, bool) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool, bool) override;

    static void led (juce::Graphics&, juce::Point<float>, float, juce::Colour, bool = true);
    static void mascot (juce::Graphics&, juce::Rectangle<float>, juce::Colour, bool sparkles);
    static void screw (juce::Graphics&, float, float);
};
}
