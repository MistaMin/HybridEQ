#include "PluginEditor.h"
#include "EmbeddedDesign.h"

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
    constexpr int panelW  = 150;
    constexpr int knobH   = 98;
    constexpr int buttonH = 32;

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

    constexpr int hKnobW = 88;
    constexpr int hButtonW = 96;

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

    lowCutPanel.setAccent(Theme::lowCutCol,"20 Hz - 1 kHz");
    lowBandPanel.setAccent(Theme::lowBandCol,"20 - 400 Hz");
    mid1Panel.setAccent(Theme::mid1Col,"200 Hz - 6 kHz");
    mid2Panel.setAccent(Theme::mid2Col,"200 Hz - 6 kHz");
    highBandPanel.setAccent(Theme::highBandCol,"800 Hz - 30 kHz");
    highCutPanel.setAccent(Theme::highCutCol,"1 - 30 kHz");
    preampPanel.setAccent(Theme::preampCol,"INPUT / CIRCUIT COLOUR");
    globalPanel.setAccent(Theme::outputCol,"OVERSAMPLING / OUTPUT TRIM");

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
    setResizeLimits(826, 616, 1770, 1320);
    getConstrainer()->setFixedAspectRatio(1180.0 / 880.0);

    int savedW = proc.apvts.state.getProperty("consoleEditorWidth", 1062);
    int savedH = proc.apvts.state.getProperty("consoleEditorHeight", 792);
    setSize(savedW, savedH);

    // Put the existing panel on a fixed design canvas; resize scales it uniformly.
    std::vector<juce::Component*> panelChildren {
        eqDisplay.get(), &lowCutPanel, &highCutPanel, &lowBandPanel,
        &mid1Panel, &mid2Panel, &highBandPanel, &preampPanel, &globalPanel, &harmonicsPanel
    };
    for (auto* child : panelChildren) panelSurface.addAndMakeVisible(child);
    addAndMakeVisible(panelSurface);
    eqDisplay->setSize(0,0);
    resized();
    std::istringstream designInput(hybridEQDesign);
    const auto bakedItems=goodlookinui::readDesign(designInput);
    auto registerKnob = [this, &bakedItems](const char* id, RotaryKnob& knob) {
        auto item=knob.getDesign(); item.id=id; item.parameter=id;
        const auto b=knob.getBounds(); item.x=float(b.getX()); item.y=float(b.getY());
        item.width=float(b.getWidth()); item.height=float(b.getHeight());
        if(auto found=std::find_if(bakedItems.begin(),bakedItems.end(),[&](const auto& v){return v.id==id;}); found!=bakedItems.end()) {
            if(found->parameter!=id) throw std::runtime_error("Embedded design binding mismatch");
            item=*found; knob.applyDesign(item);
            knob.setBounds(juce::roundToInt(item.x),juce::roundToInt(item.y),juce::roundToInt(item.width),juce::roundToInt(item.height));
        }
#if GOODLOOKINUI_ENABLE_EDITOR
        designStudio.add(item,knob,[&knob](const goodlookinui::Item& value){knob.applyDesign(value);});
#endif
    };
    registerKnob("lcFreq",lcFreqDial); registerKnob("lcQ",lcQDial);
    registerKnob("hcFreq",hcFreqDial); registerKnob("hcQ",hcQDial);
    registerKnob("lowFreq",lowFreqDial); registerKnob("lowGain",lowGainDial);
    registerKnob("mid1Freq",mid1FreqDial); registerKnob("mid1Gain",mid1GainDial); registerKnob("mid1Q",mid1QDial);
    registerKnob("mid2Freq",mid2FreqDial); registerKnob("mid2Gain",mid2GainDial); registerKnob("mid2Q",mid2QDial);
    registerKnob("highFreq",highFreqDial); registerKnob("highGain",highGainDial);
    registerKnob("preampGain",preampGainDial); registerKnob("outputGain",outputGainDial);

#if GOODLOOKINUI_ENABLE_EDITOR
    addChildComponent(designStudio); panelSurface.addAndMakeVisible(designButton);
    designButton.onClick=[this]{designStudio.setVisible(!designStudio.isVisible());designStudio.toFront(false);};
    resized();
#endif
    refreshBypassStates();
}

HybridEQEditor::~HybridEQEditor()
{
    for (const auto& id : {"lcBypass", "hcBypass", "lowBypass", "mid1Bypass", "mid2Bypass",
                           "highBypass", "preampBypass", "mid1Type", "mid2Type"})
        proc.apvts.removeParameterListener(id, this);

    proc.apvts.state.setProperty("consoleEditorWidth", getWidth(), nullptr);
    proc.apvts.state.setProperty("consoleEditorHeight", getHeight(), nullptr);
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
    g.fillAll(juce::Colour(0xff10171b));
    constexpr float width=1180, height=880;
    const float scale=juce::jmin(float(getWidth())/width,float(getHeight())/height);
    g.addTransform(juce::AffineTransform::scale(scale).translated(
        (getWidth()-width*scale)*0.5f,(getHeight()-height*scale)*0.5f));
    auto body=juce::Rectangle<float>(4,4,width-8,height-8);
    g.setGradientFill(juce::ColourGradient(Theme::editorTop,0,0,Theme::editorBottom,width,height,false));
    g.fillRoundedRectangle(body,5);
    g.setColour(juce::Colour(0xff485156));g.drawRoundedRectangle(body,5,1);
    // Fine machined rack rails, deliberately quieter than the controls.
    for(int x:{16,1164}) {
        g.setColour(juce::Colour(0xff0c1114));g.fillRect(x-5,12,10,856);
        goodlookinui::juce_adapter::drawScrew(g,float(x),24);
        goodlookinui::juce_adapter::drawScrew(g,float(x),856);
    }
    g.setColour(juce::Colour(0xff121b20));g.fillRect(30,12,1120,50);
    g.setColour(Theme::textDark);g.setFont(juce::FontOptions(28.0f,juce::Font::bold));
    g.drawText("Hybrid EQ",44,16,260,40,juce::Justification::centredLeft);
    g.setFont(juce::FontOptions(10.0f,juce::Font::bold));g.setColour(Theme::textMid);
    g.drawText("H Y B R I D A U D I O",310,21,200,18,juce::Justification::centredLeft);
    g.setFont(juce::FontOptions(9.0f));
    g.drawText("SIX-BAND CONSOLE EQUALISER",310,39,260,14,juce::Justification::centredLeft);
    g.setColour(Theme::preampCol);g.setFont(juce::FontOptions(10.0f,juce::Font::bold));
    g.drawText("ANALOGUE CHARACTER / DIGITAL PRECISION",780,24,350,22,juce::Justification::centredRight);
    g.setColour(Theme::textMid);g.setFont(juce::FontOptions(9.0f,juce::Font::bold));
    g.drawText("FREQUENCY RESPONSE",40,68,200,15,juce::Justification::centredLeft);
    g.drawText("20 Hz - 30 kHz   /   +/-24 dB",900,68,240,15,juce::Justification::centredRight);
    g.setColour(Theme::preampCol.withAlpha(0.7f));g.fillRect(38.0f,268.0f,28.0f,2.0f);
    g.setColour(Theme::textMid);g.setFont(juce::FontOptions(9.0f,juce::Font::bold));
    g.drawText("EQUALISER",76,260,180,18,juce::Justification::centredLeft);
    g.drawText("FILTERS / FOUR COLOUR BANDS / MID-SIDE",740,260,400,18,juce::Justification::centredRight);
    g.setColour(juce::Colour(0xff526065));g.setFont(juce::FontOptions(8.0f));
    g.drawText("HYBRIDAUDIO   /   HYBRID EQ",38,862,600,12,juce::Justification::centredLeft);
    g.drawText("CONSOLE EDITION / v" + juce::String(JucePlugin_VersionString),900,862,240,12,juce::Justification::centredRight);
}

void HybridEQEditor::resized()
{
    constexpr int canvasWidth=1180, canvasHeight=880;
    const float scale=juce::jmin(float(getWidth())/canvasWidth,float(getHeight())/canvasHeight);
    panelSurface.setBounds(0,0,canvasWidth,canvasHeight);
    panelSurface.setTransform(juce::AffineTransform::scale(scale).translated(
        (getWidth()-canvasWidth*scale)*0.5f,(getHeight()-canvasHeight*scale)*0.5f));
#if GOODLOOKINUI_ENABLE_EDITOR
    designButton.setBounds(600,22,76,24);
    designStudio.setBounds(8,32,juce::jmin(810,getWidth()-16),92);
#endif
    // Layout once at design resolution. Saved development edits survive resizing.
    if (eqDisplay && eqDisplay->getWidth()>0) return;
    if(eqDisplay) eqDisplay->setBounds(38,88,1104,166);
    SectionPanel* strips[]{&lowCutPanel,&lowBandPanel,&mid1Panel,&mid2Panel,&highBandPanel,&highCutPanel};
    for(int n=0;n<6;++n) strips[n]->setBounds(38+n*186,282,174,430);
    preampPanel.setBounds(38,726,360,132);
    harmonicsPanel.setBounds(410,726,360,132);
    globalPanel.setBounds(782,726,360,132);
}
