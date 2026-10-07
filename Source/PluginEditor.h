#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "UI/EQDisplay.h"
#include "UI/RotaryKnob.h"
#include "UI/CycleButton.h"
#include "UI/BypassButton.h"
#include "UI/HarmonicsPanel.h"
#include "UI/SectionPanel.h"
#include "UI/SmallToggle.h"
#include "UI/VerticalPair.h"
#include "UI/Theme.h"
#include "Toolkit.h"
#include "UI/LookTriggers.h"
#include "UI/MeterPanel.h"
#include "UI/SpectrumOverlay.h"
#if GOODLOOKINUI_ENABLE_EDITOR
#include <Studio.h>
#endif
#include <memory>

class HybridEQEditor : public juce::AudioProcessorEditor,
                       private juce::AudioProcessorValueTreeState::Listener,
                       private juce::ValueTree::Listener,
                       private juce::Timer {
public:
    explicit HybridEQEditor(HybridEQProcessor&);
    ~HybridEQEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;

    void parameterChanged(const juce::String&, float) override;
    LookTable lookTable{LookTriggers::defaultLooks()};
    // Project-level design choices (colour trigger, meter face, spectrum ...): baked from Designs/Settings.csv,
    // overridden by the project's state in releases. Developer builds autosave them to that file.
    static const std::vector<juce::String>& settingKeys();
    void loadDesignSettings();
    void valueTreePropertyChanged(juce::ValueTree&, const juce::Identifier&) override;
    std::map<std::string, RotaryKnob*> knobById;
#if GOODLOOKINUI_ENABLE_EDITOR
    DesignAutosave autosave;
    void saveSettingsFile();
    void saveLooksFile();
    void saveKnobsFile();
#endif
    void onKnobDesignEdited(RotaryKnob&, const goodlookinui::Item&);
    void persistLooks();
    void refreshBypassStates();
    void applyKnobStyle(const juce::String& code);
    void refreshKnobStyles();
    void showKnobStyleMenu();
    // Colour triggers / faceplates (see UI/LookTriggers.h).
    TriggerMode getTriggerMode() const;
    void applyLook(bool animate);
    void stepLook(float t);
    void showLookMenu();
    void timerCallback() override;
    juce::String oversampleDisplayText() const;

    HybridEQProcessor& proc;
    juce::Component panelSurface;
#if GOODLOOKINUI_ENABLE_EDITOR
    goodlookinui::juce_adapter::Studio designStudio;
    juce::TextButton designButton{"Design"};
    struct SelectionRing : juce::Component {
        SelectionRing() { setInterceptsMouseClicks(false, false); }
        void paint(juce::Graphics& g) override { g.setColour(juce::Colour(0xffce9435)); g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1.5f), 5.0f, 2.5f); }
    } ring;
    std::unique_ptr<LookStudio> lookStudio;
    juce::TooltipWindow tooltips{this, 500};
    void highlightKnob(juce::Component*);
#endif

    std::unique_ptr<SpectrumRenderer> spectrumRenderer;
    std::unique_ptr<SpectrumOverlay> spectrumOverlay;
    juce::TextButton spectrumBtn{"SPECTRUM"};
    void applySpectrum(const SpectrumSettings&, bool save);
#if GOODLOOKINUI_ENABLE_EDITOR
    std::unique_ptr<SpectrumStudio> spectrumStudio;
#endif
    std::unique_ptr<EQDisplay> eqDisplay;
    std::unique_ptr<EQDisplay> overlayEq;   // draggable EQ nodes on the full-window spectrum view

    RotaryKnob lcFreqDial{"FREQ", KnobValueType::Frequency, Theme::lowCutCol};
    RotaryKnob lcQDial{"Q", KnobValueType::Q, Theme::lowCutCol};
    CycleButton lcSlopeBtn, lcModeBtn;
    BypassButton lcBypassBtn;

    RotaryKnob hcFreqDial{"FREQ", KnobValueType::Frequency, Theme::highCutCol};
    RotaryKnob hcQDial{"Q", KnobValueType::Q, Theme::highCutCol};
    CycleButton hcSlopeBtn, hcModeBtn;
    BypassButton hcBypassBtn;

    RotaryKnob lowFreqDial{"FREQ", KnobValueType::Frequency, Theme::lowBandCol};
    RotaryKnob lowGainDial{"GAIN", KnobValueType::Gain, Theme::lowBandCol};
    CycleButton lowTypeBtn, lowModeBtn;
    BypassButton lowBypassBtn;

    RotaryKnob mid1GainDial{"GAIN", KnobValueType::Gain, Theme::mid1Col};
    RotaryKnob mid1FreqDial{"FREQ", KnobValueType::Frequency, Theme::mid1Col};
    RotaryKnob mid1QDial{"Q", KnobValueType::Q, Theme::mid1Col};
    CycleButton mid1TypeBtn, mid1ModeBtn;
    BypassButton mid1BypassBtn;

    RotaryKnob mid2GainDial{"GAIN", KnobValueType::Gain, Theme::mid2Col};
    RotaryKnob mid2FreqDial{"FREQ", KnobValueType::Frequency, Theme::mid2Col};
    RotaryKnob mid2QDial{"Q", KnobValueType::Q, Theme::mid2Col};
    CycleButton mid2TypeBtn, mid2ModeBtn;
    BypassButton mid2BypassBtn;

    RotaryKnob highFreqDial{"FREQ", KnobValueType::Frequency, Theme::highBandCol};
    RotaryKnob highGainDial{"GAIN", KnobValueType::Gain, Theme::highBandCol};
    CycleButton highTypeBtn, highModeBtn;
    BypassButton highBypassBtn;

    // PREAMP input-stage card
    CycleButton preampPadBtn, preampTypeBtn;
    RotaryKnob preampGainDial{"GAIN", KnobValueType::Gain, Theme::preampCol};
    BypassButton preampBypassBtn;
    SmallToggle preampCircuitToggle;
    VerticalPair preampTypeCircuitPair;

    HarmonicsPanel harmonicsPanel;
    MeterPanel meterPanel;
    void applyMeterStyles();

    CycleButton oversampleBtn;
    CycleButtonLNF knobStyleLnf;
    juce::TextButton knobStyleBtn;
    juce::TextButton lookBtn;
    std::vector<std::pair<RotaryKnob*, std::string>> knobDefaults;
    std::vector<CycleButton*> cycleButtons;
    struct Rule {
        const char* param;
        std::vector<RotaryKnob*> knobs;
        SectionPanel* panel;
        BypassButton* bypass;
        std::vector<CycleButton*> buttons;
        std::vector<juce::Colour> knobBase;
        std::string styleNow;   // knob class chosen by the trigger; empty = keep the saved design style
    };
    std::vector<Rule> rules;
    struct KnobFade { RotaryKnob* knob; juce::Colour from, to; };
    struct PanelFade {
        Rule* rule;
        juce::Colour from, to;
        bool hadPlate = false, wantsPlate = false;
        juce::Colour plateFrom[4], plateTo[4];
        int finishFrom = 0, finishTo = 0;
    };
    std::vector<KnobFade> knobFades;
    std::vector<PanelFade> panelFades;
    Theme::Look lookFrom, lookTo;
    float fadeT = 1.0f;
    RotaryKnob outputGainDial{"OUTPUT", KnobValueType::Gain, Theme::outputCol};

    SectionPanel lowCutPanel{"LOW CUT"}, highCutPanel{"HIGH CUT"}, lowBandPanel{"LOW BAND"};
    SectionPanel mid1Panel{"MID 1"}, mid2Panel{"MID 2"}, highBandPanel{"HIGH BAND"};
    SectionPanel preampPanel{"PREAMP"};
    SectionPanel globalPanel{"GLOBAL"};

    std::unique_ptr<SliderAttachment> lcFreqAtt, lcQAtt, hcFreqAtt, hcQAtt;
    std::unique_ptr<SliderAttachment> lowFreqAtt, lowGainAtt;
    std::unique_ptr<SliderAttachment> mid1FreqAtt, mid1GainAtt, mid1QAtt;
    std::unique_ptr<SliderAttachment> mid2FreqAtt, mid2GainAtt, mid2QAtt;
    std::unique_ptr<SliderAttachment> highFreqAtt, highGainAtt;
    std::unique_ptr<SliderAttachment> preampGainAtt;
    std::unique_ptr<SliderAttachment> outputGainAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HybridEQEditor)
};
