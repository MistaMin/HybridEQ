#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "Theme.h"
#include <memory>
#include <GoodLookinUI.h>

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
        goodlookinui::juce_adapter::drawKey(g,b,juce::Colour(0xffaeb6a7),shouldDrawButtonAsDown);
        auto lamp=juce::Rectangle<float>(4,4).withCentre({b.getX()+5,b.getCentreY()});
        if(getToggleState()){g.setColour(accentColour.withAlpha(0.2f));g.fillEllipse(lamp.expanded(2));}
        g.setColour(getToggleState()?accentColour:juce::Colour(0xff414c44));g.fillEllipse(lamp);
        g.setColour(juce::Colour(0xff26312e));g.setFont(juce::FontOptions(8.0f,juce::Font::bold));
        g.drawText("B",b.withTrimmedLeft(7),juce::Justification::centred);
        if(shouldDrawButtonAsHighlighted){g.setColour(juce::Colour(0x12ffffff));g.fillRoundedRectangle(b,2);}

    }

private:
    juce::Colour accentColour;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
};
