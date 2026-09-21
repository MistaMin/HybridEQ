#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "Theme.h"
#include <memory>

// Compact look for cycle buttons: small bold text, subtle chrome.
class CycleButtonLNF final : public juce::LookAndFeel_V4 {
public:
    juce::Font getTextButtonFont(juce::TextButton&, int) override
    {
        return juce::FontOptions(10.5f, juce::Font::bold);
    }
};

// A single button that advances to the next choice each time it is clicked,
// bound directly to an AudioParameterChoice. Replaces juce::ComboBox, whose
// popup never got populated with items in the previous implementation.
class CycleButton : public juce::Component, private juce::AudioProcessorValueTreeState::Listener {
public:
    CycleButton(juce::AudioProcessorValueTreeState& state, const juce::String& paramIDToUse,
                const juce::String& labelText, bool horizontalLayout = false)
        : apvts(state), paramID(paramIDToUse), isHorizontal(horizontalLayout)
    {
        param = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter(paramID));
        jassert(param != nullptr);

        title.setText(labelText, juce::dontSendNotification);
        title.setJustificationType(isHorizontal ? juce::Justification::centredRight
                                                : juce::Justification::centred);
        title.setFont(juce::FontOptions(isHorizontal ? 9.5f : 10.5f, juce::Font::bold));
        title.setColour(juce::Label::textColourId,
                        isHorizontal ? Theme::textMid : Theme::textDark);
        addAndMakeVisible(title);

        lnf = std::make_unique<CycleButtonLNF>();
        button.setLookAndFeel(lnf.get());
        button.setColour(juce::TextButton::buttonColourId, Theme::buttonTop);
        button.setColour(juce::TextButton::buttonOnColourId, Theme::buttonBottom);
        button.setColour(juce::TextButton::textColourOffId, Theme::buttonText);
        button.setColour(juce::TextButton::textColourOnId, Theme::buttonText);
        button.onClick = [this] { cycle(); };
        addAndMakeVisible(button);

        updateText();
        apvts.addParameterListener(paramID, this);
    }

    ~CycleButton() override
    {
        apvts.removeParameterListener(paramID, this);
    }

    void resized() override
    {
        auto b = getLocalBounds();
        if (isHorizontal) {
            title.setBounds(b.removeFromLeft(42));
            button.setBounds(b.reduced(0, 1));
        } else {
            title.setBounds(b.removeFromTop(14));
            b.removeFromTop(3);
            int buttonH = juce::jmin(26, b.getHeight());
            button.setBounds(2, b.getY(), getWidth() - 4, buttonH);
        }
    }

    // Grey-out styling used when the owning band is bypassed.
    void setActive(bool active)
    {
        if (isActive == active)
            return;
        isActive = active;
        if (isActive) {
            button.setColour(juce::TextButton::buttonColourId, Theme::buttonTop);
            button.setColour(juce::TextButton::buttonOnColourId, Theme::buttonBottom);
            button.setColour(juce::TextButton::textColourOffId, Theme::buttonText);
            button.setColour(juce::TextButton::textColourOnId, Theme::buttonText);
        } else {
            button.setColour(juce::TextButton::buttonColourId, Theme::buttonInactiveTop);
            button.setColour(juce::TextButton::buttonOnColourId, Theme::buttonInactiveBottom);
            button.setColour(juce::TextButton::textColourOffId, Theme::buttonTextInactive);
            button.setColour(juce::TextButton::textColourOnId, Theme::buttonTextInactive);
        }
        title.setColour(juce::Label::textColourId, isActive
                             ? (isHorizontal ? Theme::textMid : Theme::textDark)
                             : Theme::buttonTextInactive);
    }

    bool getActive() const { return isActive; }

    void setButtonTooltip(const juce::String& tip) { button.setTooltip(tip); }

private:
    void cycle()
    {
        if (param == nullptr) return;
        int numChoices = param->choices.size();
        int next = (param->getIndex() + 1) % numChoices;
        param->beginChangeGesture();
        param->setValueNotifyingHost(param->getNormalisableRange().convertTo0to1(static_cast<float>(next)));
        param->endChangeGesture();
        updateText();
    }

    void parameterChanged(const juce::String&, float) override
    {
        juce::MessageManager::callAsync([safeThis = juce::Component::SafePointer<CycleButton>(this)] {
            if (safeThis != nullptr)
                safeThis->updateText();
        });
    }

    void updateText()
    {
        if (param != nullptr)
            button.setButtonText(param->getCurrentChoiceName());
    }

    juce::AudioProcessorValueTreeState& apvts;
    juce::String paramID;
    juce::AudioParameterChoice* param = nullptr;
    bool isActive = true;
    bool isHorizontal = false;
    std::unique_ptr<juce::LookAndFeel> lnf;
    juce::Label title;
    juce::TextButton button;
};
