#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "Theme.h"
#include <array>
#include <memory>
#include <GoodLookinUI.h>
#include <Meters.h>
#include <Retro.h>

class ConsoleFaderLook final : public juce::LookAndFeel_V4 {
public:
    // 0 oval, 1 console cap, 2..5 retro slider styles (neon, bar, chamfer, wedge)
    int style = 0;
    float clock = 0.0f;
    juce::Colour glow = juce::Colour(0xff39f2ff);
    void drawLinearSlider(juce::Graphics& g,int x,int y,int width,int height,float pos,float,float,
                          const juce::Slider::SliderStyle,juce::Slider&) override {
        const float usable=float(width)-14.0f;
        const float prop=usable>0?juce::jlimit(0.0f,1.0f,(pos-float(x)-7.0f)/usable):0.0f;
        const juce::Rectangle<float> r{float(x)+7.0f,float(y),usable,float(height)};
        namespace gm=goodlookinui::juce_adapter::meters;
        if(style==0) gm::drawSlotFader(g,r,prop,gm::FaderCap::Oval,Theme::textDark);
        else if(style==1) gm::drawSlotFader(g,r,prop,gm::FaderCap::Console,Theme::textDark);
        else goodlookinui::juce_adapter::retro::drawSliderH(g,r,prop,static_cast<goodlookinui::juce_adapter::retro::SliderStyle>(style-2),glow,clock);
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

    static constexpr const char* faderCodes[7] = {"oval","console","neon","bar","chamfer","wedge","flat"};
    static int faderIndex(const juce::String& code) { for (int i = 0; i < 7; ++i) if (code == faderCodes[i]) return i; return 0; }
    void setFaderStyle(int styleIndex)
    {
        faderLook.style = styleIndex;
        faderLook.glow = styleIndex == 3 ? juce::Colour(0xffd6ff2e) : styleIndex == 6 ? juce::Colour(0xff3ccf5a) : styleIndex == 5 ? juce::Colour(0xffffa31a) : juce::Colour(0xff39f2ff);
        for (auto& row : rows) row.slider.repaint();
    }
    int getFaderStyle() const { return faderLook.style; }

    void refreshTheme()
    {
        for (auto& row : rows) {
            row.label.setColour(juce::Label::textColourId, Theme::textMid);
            row.slider.setColour(juce::Slider::thumbColourId, Theme::textDark);
        }
        title.setColour(juce::Label::textColourId, Theme::textDark);
        repaint();
    }

    ~HarmonicsPanel() override { for(auto& row:rows) row.slider.setLookAndFeel(nullptr); }

    void paint(juce::Graphics& g) override
    {
        goodlookinui::juce_adapter::drawPanel(g,getLocalBounds().toFloat().reduced(1),Theme::panelTop,Theme::panelBottom,Theme::panelFinish);

    }

    void resized() override
    {
        auto b = getLocalBounds();
        title.setBounds(b.removeFromTop(30).reduced(14, 0));

        // Compact rows: the four sliders sit in a tighter block, centred under the title.
        b.reduce(8, 2);
        constexpr int rowH = 19;
        b.removeFromTop((b.getHeight() - rowH * 4) / 2);
        for (auto& row : rows) {
            auto r = b.removeFromTop(rowH);
            row.label.setBounds(r.removeFromLeft(26));
            row.slider.setBounds(r.reduced(6, 2));
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
