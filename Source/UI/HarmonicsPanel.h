#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "Theme.h"
#include <array>
#include <memory>
#include <GoodLookinUI.h>

class ConsoleFaderLook final : public juce::LookAndFeel_V4 {
public:
    void drawLinearSlider(juce::Graphics& g,int x,int y,int width,int height,float pos,float,float,
                          const juce::Slider::SliderStyle,juce::Slider&) override {
        float centre=float(y)+float(height)*0.5f;
        g.setColour(juce::Colour(0xff0d1518));g.fillRoundedRectangle(float(x),centre-2,float(width),4,2);
        g.setColour(juce::Colour(0xff52605e));g.drawHorizontalLine(int(centre+3),float(x),float(x+width));
        for(int n=0;n<9;++n){float tick=float(x)+float(width)*float(n)/8;
            g.setColour(juce::Colour(0xff77867f));g.drawLine(tick,centre+7,tick,centre+10,0.8f);}
        goodlookinui::juce_adapter::drawKey(g,{pos-7,centre-6,14,12},Theme::buttonTop,false);
        g.setColour(juce::Colour(0xff4b564c));g.drawLine(pos,centre-4,pos,centre+4,1);
    }
};

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

            row.slider.setLookAndFeel(&faderLook);
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

    ~HarmonicsPanel() override { for(auto& row:rows) row.slider.setLookAndFeel(nullptr); }

    void paint(juce::Graphics& g) override
    {
        goodlookinui::juce_adapter::drawPanel(g,getLocalBounds().toFloat().reduced(1));

    }

    void resized() override
    {
        auto b = getLocalBounds();
        title.setBounds(b.removeFromTop(30).reduced(14, 0));

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

    ConsoleFaderLook faderLook;
    juce::Label title;
    std::array<Row, 4> rows;
};
