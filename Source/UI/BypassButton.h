#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "Theme.h"
#include <memory>
#include <GoodLookinUI.h>
#include <Meters.h>

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
        auto b=getLocalBounds().toFloat().reduced(2);
        goodlookinui::juce_adapter::meters::drawLitKey(g,b,"B",getToggleState(),accentColour,shouldDrawButtonAsDown);
        if(shouldDrawButtonAsHighlighted){g.setColour(juce::Colour(0x12ffffff));g.fillRoundedRectangle(b,2);}
    }

    void setAccent(juce::Colour c) { accentColour = c; repaint(); }
    juce::Colour getAccent() const { return accentColour; }

private:
    juce::Colour accentColour;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
};
