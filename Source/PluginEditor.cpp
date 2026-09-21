#include "PluginEditor.h"

HybridEQEditor::HybridEQEditor(HybridEQProcessor& p)
    : AudioProcessorEditor(&p), proc(p),
      lcSlopeBtn(p.apvts, "lcSlope", "SLOPE"),
      lcModeBtn(p.apvts, "lcMode", "M/S"),
      lcBypassBtn(p.apvts, "lcBypass", Theme::lowCutCol),
      hcSlopeBtn(p.apvts, "hcSlope", "SLOPE"),
      hcModeBtn(p.apvts, "hcMode", "M/S"),
      hcBypassBtn(p.apvts, "hcBypass", Theme::highCutCol),
      lowTypeBtn(p.apvts, "lowType", "TYPE"),
      lowModeBtn(p.apvts, "lowMode", "M/S"),
      lowBypassBtn(p.apvts, "lowBypass", Theme::lowBandCol),
      mid1TypeBtn(p.apvts, "mid1Type", "TYPE"),
      mid1ModeBtn(p.apvts, "mid1Mode", "M/S"),
      mid1BypassBtn(p.apvts, "mid1Bypass", Theme::mid1Col),
      mid2TypeBtn(p.apvts, "mid2Type", "TYPE"),
      mid2ModeBtn(p.apvts, "mid2Mode", "M/S"),
      mid2BypassBtn(p.apvts, "mid2Bypass", Theme::mid2Col),
      highTypeBtn(p.apvts, "highType", "TYPE"),
      highModeBtn(p.apvts, "highMode", "M/S"),
      highBypassBtn(p.apvts, "highBypass", Theme::highBandCol),
      preampPadBtn(p.apvts, "preampPad", "PAD"),
      preampTypeBtn(p.apvts, "preampType", "TYPE"),
      preampBypassBtn(p.apvts, "preampBypass", Theme::preampCol),
      preampCircuitToggle(p.apvts, "preampCircuit", "CIRCUIT", Theme::preampCol),
      harmonicsPanel(p.apvts),
      oversampleBtn(p.apvts, "oversampleMode", "OVERSAMPLE")
{
    eqDisplay = std::make_unique<EQDisplay>(proc.apvts, proc.getEQ());
    addAndMakeVisible(*eqDisplay);

    // Narrow vertical channel-strip bands (SSL/vintage console style): every
    // band is a slim column with knobs stacked top-to-bottom rather than a
    // wide horizontal row, which keeps the whole plugin far more compact.
    constexpr int panelW  = 132;
    constexpr int knobH   = 76;
    constexpr int buttonH = 46;

    lowCutPanel.addItem(lcFreqDial, knobH);
    lowCutPanel.addItem(lcQDial, knobH);
    lowCutPanel.addItem(lcSlopeBtn, buttonH);
    lowCutPanel.addItem(lcModeBtn, buttonH);
    lowCutPanel.setBypassButton(lcBypassBtn);
    lowCutPanel.setVerticalLayout(true, panelW);
    addAndMakeVisible(lowCutPanel);

    highCutPanel.addItem(hcFreqDial, knobH);
    highCutPanel.addItem(hcQDial, knobH);
    highCutPanel.addItem(hcSlopeBtn, buttonH);
    highCutPanel.addItem(hcModeBtn, buttonH);
    highCutPanel.setBypassButton(hcBypassBtn);
    highCutPanel.setVerticalLayout(true, panelW);
    addAndMakeVisible(highCutPanel);

    lowBandPanel.addItem(lowGainDial, knobH);
    lowBandPanel.addItem(lowFreqDial, knobH);
    lowBandPanel.addItem(lowTypeBtn, buttonH);
    lowBandPanel.addItem(lowModeBtn, buttonH);
    lowBandPanel.setBypassButton(lowBypassBtn);
    lowBandPanel.setVerticalLayout(true, panelW);
    addAndMakeVisible(lowBandPanel);

    mid1Panel.addItem(mid1GainDial, knobH);
    mid1Panel.addItem(mid1FreqDial, knobH);
    mid1Panel.addItem(mid1QDial, knobH);
    mid1Panel.addItem(mid1TypeBtn, buttonH);
    mid1Panel.addItem(mid1ModeBtn, buttonH);
    mid1Panel.setBypassButton(mid1BypassBtn);
    mid1Panel.setVerticalLayout(true, panelW);
    addAndMakeVisible(mid1Panel);

    mid2Panel.addItem(mid2GainDial, knobH);
    mid2Panel.addItem(mid2FreqDial, knobH);
    mid2Panel.addItem(mid2QDial, knobH);
    mid2Panel.addItem(mid2TypeBtn, buttonH);
    mid2Panel.addItem(mid2ModeBtn, buttonH);
    mid2Panel.setBypassButton(mid2BypassBtn);
    mid2Panel.setVerticalLayout(true, panelW);
    addAndMakeVisible(mid2Panel);

    highBandPanel.addItem(highGainDial, knobH);
    highBandPanel.addItem(highFreqDial, knobH);
    highBandPanel.addItem(highTypeBtn, buttonH);
    highBandPanel.addItem(highModeBtn, buttonH);
    highBandPanel.setBypassButton(highBypassBtn);
    highBandPanel.setVerticalLayout(true, panelW);
    addAndMakeVisible(highBandPanel);

    constexpr int hKnobW = 66;
    constexpr int hButtonW = 84;

    preampTypeCircuitPair.setChildren(preampTypeBtn, preampCircuitToggle);

    preampPanel.addItem(preampPadBtn, hButtonW);
    preampPanel.addItem(preampGainDial, hKnobW);
    preampPanel.addItem(preampTypeCircuitPair, hButtonW);
    preampPanel.setBypassButton(preampBypassBtn);
    addAndMakeVisible(preampPanel);

    globalPanel.addItem(oversampleBtn, 120);
    globalPanel.addItem(outputGainDial, hKnobW);
    addAndMakeVisible(globalPanel);

    addAndMakeVisible(harmonicsPanel);

    auto& vts = proc.apvts;

    lcFreqAtt  = std::make_unique<SliderAttachment>(vts, "lcFreq",  lcFreqDial);
    lcQAtt     = std::make_unique<SliderAttachment>(vts, "lcQ",     lcQDial);
    hcFreqAtt  = std::make_unique<SliderAttachment>(vts, "hcFreq",  hcFreqDial);
    hcQAtt     = std::make_unique<SliderAttachment>(vts, "hcQ",     hcQDial);

    lowFreqAtt = std::make_unique<SliderAttachment>(vts, "lowFreq", lowFreqDial);
    lowGainAtt = std::make_unique<SliderAttachment>(vts, "lowGain", lowGainDial);

    mid1FreqAtt = std::make_unique<SliderAttachment>(vts, "mid1Freq", mid1FreqDial);
    mid1GainAtt = std::make_unique<SliderAttachment>(vts, "mid1Gain", mid1GainDial);
    mid1QAtt    = std::make_unique<SliderAttachment>(vts, "mid1Q",    mid1QDial);

    mid2FreqAtt = std::make_unique<SliderAttachment>(vts, "mid2Freq", mid2FreqDial);
    mid2GainAtt = std::make_unique<SliderAttachment>(vts, "mid2Gain", mid2GainDial);
    mid2QAtt    = std::make_unique<SliderAttachment>(vts, "mid2Q",    mid2QDial);

    highFreqAtt = std::make_unique<SliderAttachment>(vts, "highFreq", highFreqDial);
    highGainAtt = std::make_unique<SliderAttachment>(vts, "highGain", highGainDial);

    preampGainAtt = std::make_unique<SliderAttachment>(vts, "preampGain", preampGainDial);
    outputGainAtt = std::make_unique<SliderAttachment>(vts, "outputGain", outputGainDial);

    for (const auto& id : {"lcBypass", "hcBypass", "lowBypass", "mid1Bypass", "mid2Bypass",
                           "highBypass", "preampBypass", "mid1Type", "mid2Type"})
        proc.apvts.addParameterListener(id, this);

    setResizable(true, true);
    setResizeLimits(860, 620, 1600, 1100);

    int savedW = proc.apvts.state.getProperty("editorWidth", 980);
    int savedH = proc.apvts.state.getProperty("editorHeight", 760);
    setSize(savedW, savedH);

    refreshBypassStates();
}

HybridEQEditor::~HybridEQEditor()
{
    for (const auto& id : {"lcBypass", "hcBypass", "lowBypass", "mid1Bypass", "mid2Bypass",
                           "highBypass", "preampBypass", "mid1Type", "mid2Type"})
        proc.apvts.removeParameterListener(id, this);

    proc.apvts.state.setProperty("editorWidth", getWidth(), nullptr);
    proc.apvts.state.setProperty("editorHeight", getHeight(), nullptr);
}

void HybridEQEditor::parameterChanged(const juce::String&, float)
{
    juce::MessageManager::callAsync([safeThis = juce::Component::SafePointer<HybridEQEditor>(this)] {
        if (safeThis != nullptr)
            safeThis->refreshBypassStates();
    });
}

void HybridEQEditor::refreshBypassStates()
{
    auto raw = [&](const juce::String& id) {
        return proc.apvts.getRawParameterValue(id)->load();
    };
    auto isBypassed = [&](const juce::String& id) {
        return raw(id) > 0.5f;
    };
    auto isAType = [&](const juce::String& id) {
        return raw(id) >= 1.5f;
    };

    const bool lcB = isBypassed("lcBypass");
    lcFreqDial.setActive(!lcB);
    lcQDial.setActive(!lcB);
    lcSlopeBtn.setActive(!lcB);
    lcModeBtn.setActive(!lcB);

    const bool hcB = isBypassed("hcBypass");
    hcFreqDial.setActive(!hcB);
    hcQDial.setActive(!hcB);
    hcSlopeBtn.setActive(!hcB);
    hcModeBtn.setActive(!hcB);

    const bool lowB = isBypassed("lowBypass");
    lowFreqDial.setActive(!lowB);
    lowGainDial.setActive(!lowB);
    lowTypeBtn.setActive(!lowB);
    lowModeBtn.setActive(!lowB);

    const bool highB = isBypassed("highBypass");
    highFreqDial.setActive(!highB);
    highGainDial.setActive(!highB);
    highTypeBtn.setActive(!highB);
    highModeBtn.setActive(!highB);

    const bool m1B = isBypassed("mid1Bypass");
    const bool m1A = isAType("mid1Type");
    mid1FreqDial.setActive(!m1B);
    mid1GainDial.setActive(!m1B);
    mid1TypeBtn.setActive(!m1B);
    mid1ModeBtn.setActive(!m1B);
    mid1QDial.setActive(!m1B && !m1A);
    mid1QDial.setEnabled(!m1A);

    const bool m2B = isBypassed("mid2Bypass");
    const bool m2A = isAType("mid2Type");
    mid2FreqDial.setActive(!m2B);
    mid2GainDial.setActive(!m2B);
    mid2TypeBtn.setActive(!m2B);
    mid2ModeBtn.setActive(!m2B);
    mid2QDial.setActive(!m2B && !m2A);
    mid2QDial.setEnabled(!m2A);

    const bool pb = isBypassed("preampBypass");
    preampPadBtn.setActive(!pb);
    preampTypeBtn.setActive(!pb);
    preampGainDial.setActive(!pb);
    preampCircuitToggle.setActive(!pb);
}

void HybridEQEditor::paint(juce::Graphics& g)
{
    juce::ColourGradient bg(Theme::editorTop, 0.0f, 0.0f,
                            Theme::editorBottom, 0.0f, static_cast<float>(getHeight()), false);
    g.setGradientFill(bg);
    g.fillAll();
}

void HybridEQEditor::resized()
{
    auto bounds = getLocalBounds();

    int displayH = juce::jlimit(140, bounds.getHeight() * 45 / 100, bounds.getHeight() - 530);
    auto displayArea = bounds.removeFromTop(displayH).reduced(8, 6);
    if (eqDisplay)
        eqDisplay->setBounds(displayArea);

    auto controlArea = bounds.reduced(8, 4);
    auto bottomArea = controlArea.removeFromBottom(132);
    controlArea.removeFromBottom(6);

    // Slim vertical channel-strip bands, one row, no wrapping - this is the
    // whole point of the compact hardware-inspired redesign.
    juce::FlexBox bandFlex;
    bandFlex.flexDirection = juce::FlexBox::Direction::row;
    bandFlex.flexWrap = juce::FlexBox::Wrap::noWrap;
    bandFlex.alignItems = juce::FlexBox::AlignItems::flexStart;
    bandFlex.justifyContent = juce::FlexBox::JustifyContent::center;

    auto addBand = [&](SectionPanel& panel) {
        bandFlex.items.add(juce::FlexItem(panel)
                               .withWidth(static_cast<float>(panel.getPreferredWidth()))
                               .withHeight(static_cast<float>(panel.getPreferredHeight()))
                               .withMargin(juce::FlexItem::Margin(4.0f)));
    };

    addBand(lowCutPanel);
    addBand(lowBandPanel);
    addBand(mid1Panel);
    addBand(mid2Panel);
    addBand(highBandPanel);
    addBand(highCutPanel);

    bandFlex.performLayout(controlArea.toFloat());

    const int unitW = 300;

    juce::FlexBox bottomFlex;
    bottomFlex.flexDirection = juce::FlexBox::Direction::row;
    bottomFlex.alignItems = juce::FlexBox::AlignItems::center;
    bottomFlex.justifyContent = juce::FlexBox::JustifyContent::center;
    bottomFlex.items.add(juce::FlexItem(preampPanel)
                             .withWidth(static_cast<float>(unitW))
                             .withHeight(124.0f)
                             .withMargin(juce::FlexItem::Margin(4.0f)));
    bottomFlex.items.add(juce::FlexItem(harmonicsPanel)
                             .withWidth(static_cast<float>(unitW))
                             .withHeight(124.0f)
                             .withMargin(juce::FlexItem::Margin(4.0f)));
    bottomFlex.items.add(juce::FlexItem(globalPanel)
                             .withWidth(static_cast<float>(unitW))
                             .withHeight(124.0f)
                             .withMargin(juce::FlexItem::Margin(4.0f)));
    bottomFlex.performLayout(bottomArea.toFloat());
}
