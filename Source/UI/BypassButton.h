#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "Theme.h"
#include <memory>

// Tiny "B" toggle sitting in the corner of each band panel. When bypass is
// engaged the button lights up with the band's accent colour and the rest of
// the panel greys out. Bound to an AudioParameterBool via ButtonAttachment.
class BypassButton : public juce::Button {
public:
    BypassButton(juce::AudioProcessorValueTreeState& state, const juce::String& paramID, juce::Colour accent)
        : juce::Button(paramID), accentColour(accent)
    {
        setClickingTogglesState(true);
        setTooltip("Bypass band");
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(state, paramID, *this);
    }

    void paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        auto b = getLocalBounds().toFloat().reduced(0.5f);

        juce::Colour bg;
        if (getToggleState())
            bg = shouldDrawButtonAsHighlighted ? accentColour.brighter(0.18f) : accentColour;
        else
            bg = shouldDrawButtonAsHighlighted ? Theme::buttonTop.brighter(0.06f) : Theme::buttonTop;

        // Pick black or white text based on the background's brightness so
        // the "B" stays legible against any accent colour.
        juce::Colour textCol = bg.getPerceivedBrightness() > 0.55f ? juce::Colours::black
                                                                   : juce::Colours::white;

        juce::ColourGradient fill(bg.brighter(0.10f), 0.0f, b.getY(),
                                  bg.darker(0.08f), 0.0f, b.getBottom(), false);
        g.setGradientFill(fill);
        g.fillRoundedRectangle(b, 3.0f);

        g.setColour(Theme::buttonOutline.withAlpha(0.55f));
        g.drawRoundedRectangle(b, 3.0f, 1.0f);

        g.setColour(textCol);
        g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
        g.drawText("B", b, juce::Justification::centred);

        if (shouldDrawButtonAsDown)
            g.setColour(juce::Colour(0x22000000));
        if (shouldDrawButtonAsDown)
            g.fillRoundedRectangle(b, 3.0f);
    }

private:
    juce::Colour accentColour;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
};
