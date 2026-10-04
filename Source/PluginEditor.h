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
#include <memory>

class HybridEQEditor : public juce::AudioProcessorEditor,
                       private juce::AudioProcessorValueTreeState::Listener {
public:
    explicit HybridEQEditor(HybridEQProcessor&);
    ~HybridEQEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;

    void parameterChanged(const juce::String&, float) override;
    void refreshBypassStates();

    HybridEQProcessor& proc;
    juce::Component panelSurface;
#if GOODLOOKINUI_ENABLE_EDITOR
    goodlookinui::juce_adapter::Studio designStudio;
    juce::TextButton designButton{"Design"};
#endif

    std::unique_ptr<EQDisplay> eqDisplay;

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

    CycleButton oversampleBtn;
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
