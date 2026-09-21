#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "Theme.h"
#include <array>
#include <memory>

// Four stacked horizontal sliders (2nd..5th harmonic amount), grouped as a
// single unit. Deliberately shows no numeric readout - the user dials in
// harmonic saturation by ear rather than by a number.
class HarmonicsPanel : public juce::Component {
public:
    explicit HarmonicsPanel(juce::AudioProcessorValueTreeState& apvts)
    {
        static const char* ids[4]    = {"harmonic2nd", "harmonic3rd", "harmonic4th", "harmonic5th"};
        static const char* labels[4] = {"2nd", "3rd", "4th", "5th"};

        for (int i = 0; i < 4; ++i) {
            auto& row = rows[static_cast<size_t>(i)];

            row.label.setText(labels[i], juce::dontSendNotification);
            row.label.setJustificationType(juce::Justification::centredRight);
            row.label.setFont(juce::FontOptions(11.0f, juce::Font::bold));
            row.label.setColour(juce::Label::textColourId, Theme::textMid);
            addAndMakeVisible(row.label);

            row.slider.setSliderStyle(juce::Slider::LinearHorizontal);
            row.slider.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
            row.slider.setPopupDisplayEnabled(false, false, nullptr);
            row.slider.setColour(juce::Slider::backgroundColourId, Theme::harmonicsTrack);
            row.slider.setColour(juce::Slider::trackColourId, Theme::harmonicsAmber);
            row.slider.setColour(juce::Slider::thumbColourId, Theme::textDark);
            addAndMakeVisible(row.slider);

            row.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                apvts, ids[i], row.slider);
        }

        title.setText("HARMONICS", juce::dontSendNotification);
        title.setJustificationType(juce::Justification::centredLeft);
        title.setFont(juce::FontOptions(11.5f, juce::Font::bold));
        title.setColour(juce::Label::textColourId, Theme::textDark);
        addAndMakeVisible(title);
    }

    void paint(juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced(2.0f);

        juce::ColourGradient fill(Theme::panelTop, 0.0f, b.getY(),
                                  Theme::panelBottom, 0.0f, b.getBottom(), false);
        g.setGradientFill(fill);
        g.fillRoundedRectangle(b, 7.0f);

        g.setColour(Theme::panelHighlight);
        g.drawHorizontalLine(static_cast<int>(b.getY()) + 1,
                             static_cast<int>(b.getX()) + 8,
                             static_cast<int>(b.getRight()) - 8);
        g.setColour(Theme::panelOutline);
        g.drawRoundedRectangle(b, 7.0f, 1.0f);
    }

    void resized() override
    {
        auto b = getLocalBounds();
        title.setBounds(b.removeFromTop(20).reduced(8, 0));

        b.reduce(6, 2);
        int rowH = b.getHeight() / 4;
        for (auto& row : rows) {
            auto r = b.removeFromTop(rowH);
            row.label.setBounds(r.removeFromLeft(30));
            row.slider.setBounds(r.reduced(4, 4));
        }
    }

private:
    struct Row {
        juce::Label label;
        juce::Slider slider;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    juce::Label title;
    std::array<Row, 4> rows;
};
