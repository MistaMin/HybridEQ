#include "PluginEditor.h"
#include "EmbeddedDesign.h"
#include "EmbeddedLooks.h"
#include "EmbeddedSettings.h"

static juce::String choiceName(juce::AudioProcessorValueTreeState& apvts,const char* id)
{
    if(auto* p=dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter(id))) return p->getCurrentChoiceName();
    return {};
}

// Developer builds start from the live Designs/*.csv (the files the editor autosaves into), so a relaunch shows
// the saved design without a rebuild. Release builds always use the baked text.
// Folder the developer build loads from and autosaves into: the project's Designs/ folder, or the folder named
// by the HYBRIDEQ_DESIGNS_DIR environment variable (used by the autosave test so it never touches real files).
#if GOODLOOKINUI_ENABLE_EDITOR && defined(HYBRIDEQ_DESIGNS_DIR)
static juce::File designsDir()
{
    const auto env = juce::SystemStats::getEnvironmentVariable("HYBRIDEQ_DESIGNS_DIR", {});
    return env.isNotEmpty() ? juce::File(env) : juce::File(HYBRIDEQ_DESIGNS_DIR);
}
#endif

static std::string designText(const char* baked, const char* file)
{
#if GOODLOOKINUI_ENABLE_EDITOR && defined(HYBRIDEQ_DESIGNS_DIR)
    const auto f = designsDir().getChildFile(file);
    if (f.existsAsFile()) return f.loadFileAsString().toStdString();
#else
    (void) file;
#endif
    return baked;
}

const std::vector<juce::String>& HybridEQEditor::settingKeys()
{
    static const std::vector<juce::String> keys{"lookTrigger","lookFaceplate","lookPlate","knobStyle","meterFace","faderStyle","glow","spectrum"};
    return keys;
}

// Project-level design choices. Releases: the project's saved state wins, Designs/Settings.csv supplies defaults.
// Developer builds: the file is the source of truth (it is what gets committed), so it overrides stale state.
void HybridEQEditor::loadDesignSettings()
{
    SettingsTable table;
    table.fromCsv(designText(hybridEQSettings, "Settings.csv"));
    for (const auto& key : settingKeys()) {
        if (!table.has(key.toStdString())) continue;
#if GOODLOOKINUI_ENABLE_EDITOR
        proc.apvts.state.setProperty(key, juce::String(table.get(key.toStdString())), nullptr);
#else
        if (!proc.apvts.state.hasProperty(key)) proc.apvts.state.setProperty(key, juce::String(table.get(key.toStdString())), nullptr);
#endif
    }
}

void HybridEQEditor::valueTreePropertyChanged(juce::ValueTree& tree, const juce::Identifier& property)
{
#if GOODLOOKINUI_ENABLE_EDITOR
    if (tree != proc.apvts.state) return;
    if (property.toString() == "lookTable") { saveLooksFile(); return; }
    for (const auto& key : settingKeys()) if (property.toString() == key) { saveSettingsFile(); return; }
#else
    (void) tree; (void) property;
#endif
}

#if GOODLOOKINUI_ENABLE_EDITOR
void HybridEQEditor::saveSettingsFile()
{
    SettingsTable table;
    for (const auto& key : settingKeys())
        if (proc.apvts.state.hasProperty(key)) table.set(key.toStdString(), proc.apvts.state.getProperty(key).toString().toStdString());
    autosave.request("Settings.csv", table.toCsv());
}
void HybridEQEditor::saveLooksFile() { autosave.request("LookTriggers.csv", lookTable.toCsv()); }
void HybridEQEditor::saveKnobsFile() { autosave.request("HybridEQ.csv", designStudio.toCsv()); }
#endif

// Readable names for the developer tools (inspector list, tooltips, section looks).
static const char* knobName(const std::string& id)
{
    static const std::pair<const char*,const char*> names[]={
        {"lcFreq","Low Cut: Frequency"},{"lcQ","Low Cut: Q (resonance)"},
        {"hcFreq","High Cut: Frequency"},{"hcQ","High Cut: Q (resonance)"},
        {"lowGain","Low Band: Gain"},{"lowFreq","Low Band: Frequency"},
        {"mid1Gain","Mid 1: Gain"},{"mid1Freq","Mid 1: Frequency"},{"mid1Q","Mid 1: Q (width)"},
        {"mid2Gain","Mid 2: Gain"},{"mid2Freq","Mid 2: Frequency"},{"mid2Q","Mid 2: Q (width)"},
        {"highGain","High Band: Gain"},{"highFreq","High Band: Frequency"},
        {"preampGain","Preamp: Gain"},{"outputGain","Global: Output gain"}};
    for(auto& n:names) if(id==n.first) return n.second;
    return "";
}

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
      meterPanel(p),
      oversampleBtn(p.apvts, "oversampleMode", "OVERSAMPLE")
{
    // The Oversample key shows the factor the engine is actually running at, which the headroom floor
    // (High Cut / High shelf / Preamp Circuit) can raise above the user's choice. The choice itself is
    // never rewritten, so the menu still shows what the user picked.
    oversampleBtn.setDisplayTextOverride(oversampleDisplayText());
    oversampleBtn.setButtonTooltip("Internal processing rate. The headroom floor can raise this above "
                                   "your choice when High Cut, the High shelf or Preamp Circuit need it.");

#if GOODLOOKINUI_ENABLE_EDITOR && defined(HYBRIDEQ_DESIGNS_DIR)
    autosave.setFolder(designsDir());
#endif
    loadDesignSettings();
    spectrumRenderer = std::make_unique<SpectrumRenderer>(proc.getInputRing(), proc.getOutputRing(), [this] { return proc.getCurrentSampleRate(); });
    spectrumRenderer->setSettings(SpectrumSettings::fromString(proc.apvts.state.getProperty("spectrum", "").toString()));
    eqDisplay = std::make_unique<EQDisplay>(proc.apvts, proc.getEQ());
    eqDisplay->setSpectrum(spectrumRenderer.get());
    spectrumOverlay = std::make_unique<SpectrumOverlay>(proc.getInputRing(), proc.getOutputRing(), [this] { return proc.getCurrentSampleRate(); });
    spectrumOverlay->setBase(spectrumRenderer->getSettings());
    overlayEq = std::make_unique<EQDisplay>(proc.apvts, proc.getEQ());
    overlayEq->setOverlayMode(true);
    spectrumOverlay->setNodeLayer(overlayEq.get());
    addAndMakeVisible(*eqDisplay);

    // Narrow vertical channel-strip bands (vintage console style): every
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

    constexpr int hKnobW = 78;
    constexpr int hButtonW = 84;

    preampTypeCircuitPair.setChildren(preampTypeBtn, preampCircuitToggle);

    preampPanel.addItem(preampPadBtn, hButtonW);
    preampPanel.addItem(preampGainDial, hKnobW);
    preampPanel.addItem(preampTypeCircuitPair, hButtonW);
    preampPanel.setBypassButton(preampBypassBtn);
    addAndMakeVisible(preampPanel);

    globalPanel.addItem(oversampleBtn, 104);
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
                           "highBypass", "preampBypass", "mid1Type", "mid2Type", "lowType", "highType", "preampType", "lcSlope", "hcSlope"})
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
        &mid1Panel, &mid2Panel, &highBandPanel, &preampPanel, &globalPanel, &harmonicsPanel, &meterPanel
    };
    for (auto* child : panelChildren) panelSurface.addAndMakeVisible(child);
    addAndMakeVisible(panelSurface);
    // Full-window analyser: a button in the header opens it over the whole editor.
    panelSurface.addAndMakeVisible(spectrumBtn);
    spectrumBtn.setColour(juce::TextButton::buttonColourId, Theme::buttonTop);
    spectrumBtn.setColour(juce::TextButton::textColourOffId, Theme::buttonText);
    spectrumBtn.setTooltip("Full-window spectrum / spectrogram");
    spectrumBtn.onClick = [this] { spectrumOverlay->setVisible(!spectrumOverlay->isVisible()); if (spectrumOverlay->isVisible()) spectrumOverlay->toFront(true); };
    panelSurface.addChildComponent(*spectrumOverlay);
    eqDisplay->setSize(0,0);
    resized();
    std::vector<goodlookinui::Item> bakedItems;
    {
        std::istringstream live(designText(hybridEQDesign,"HybridEQ.csv"));
        try { bakedItems=goodlookinui::readDesign(live); }
        catch(const std::exception&) { std::istringstream baked(hybridEQDesign); bakedItems=goodlookinui::readDesign(baked); }   // unreadable live file: use the baked one
    }
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
        knobById[id]=&knob;
        designStudio.add(item,knob,[this,&knob](const goodlookinui::Item& value){onKnobDesignEdited(knob,value);},knobName(id));
        knob.setTooltip(juce::String(knobName(id))+"   ["+id+"]");
        knob.onSelect=[this,&knob]{if(designStudio.isVisible()) designStudio.select(&knob);};
#endif
    };
    auto trackDefault=[this](RotaryKnob& k){knobDefaults.emplace_back(&k,k.getDesign().style);};
    registerKnob("lcFreq",lcFreqDial); registerKnob("lcQ",lcQDial);
    registerKnob("hcFreq",hcFreqDial); registerKnob("hcQ",hcQDial);
    registerKnob("lowGain",lowGainDial); registerKnob("lowFreq",lowFreqDial);
    registerKnob("mid1Gain",mid1GainDial); registerKnob("mid1Freq",mid1FreqDial); registerKnob("mid1Q",mid1QDial);
    registerKnob("mid2Gain",mid2GainDial); registerKnob("mid2Freq",mid2FreqDial); registerKnob("mid2Q",mid2QDial);
    registerKnob("highGain",highGainDial); registerKnob("highFreq",highFreqDial);
    registerKnob("preampGain",preampGainDial); registerKnob("outputGain",outputGainDial);

#if GOODLOOKINUI_ENABLE_EDITOR
    addChildComponent(designStudio); panelSurface.addAndMakeVisible(designButton);
    panelSurface.addChildComponent(ring);
    designButton.onClick=[this]{
        const bool show=!designStudio.isVisible();
        designStudio.setVisible(show);designStudio.toFront(false);
        if(lookStudio){lookStudio->setVisible(show);lookStudio->toFront(false);}
        if(spectrumStudio){spectrumStudio->setVisible(show);spectrumStudio->toFront(false);}
        highlightKnob(designStudio.selected());
    };
    designStudio.onSelect=[this](juce::Component* c){
        highlightKnob(c);
        if(lookStudio) for(auto& r:rules) for(auto* k:r.knobs) if(k==c) lookStudio->follow(r.param,choiceName(proc.apvts,r.param));
    };
    resized();
#endif
    for(auto* k:std::initializer_list<RotaryKnob*>{&lcFreqDial,&lcQDial,&hcFreqDial,&hcQDial,&lowFreqDial,&lowGainDial,
        &mid1FreqDial,&mid1GainDial,&mid1QDial,&mid2FreqDial,&mid2GainDial,&mid2QDial,&highFreqDial,&highGainDial,
        &preampGainDial,&outputGainDial}) trackDefault(*k);

    globalPanel.addAndMakeVisible(knobStyleBtn);
    knobStyleBtn.setBounds(18,100,117,24);
    cycleButtons = {&lcSlopeBtn,&lcModeBtn,&hcSlopeBtn,&hcModeBtn,&lowTypeBtn,&lowModeBtn,&mid1TypeBtn,&mid1ModeBtn,
                    &mid2TypeBtn,&mid2ModeBtn,&highTypeBtn,&highModeBtn,&preampPadBtn,&preampTypeBtn,&oversampleBtn};
    // Which controls re-tint when a parameter's choice changes (Control mode).
    rules = {
        {"lcSlope",    {&lcFreqDial,&lcQDial},                   &lowCutPanel,   &lcBypassBtn,   {&lcSlopeBtn,&lcModeBtn},       {}, {}},
        {"hcSlope",    {&hcFreqDial,&hcQDial},                   &highCutPanel,  &hcBypassBtn,   {&hcSlopeBtn,&hcModeBtn},       {}, {}},
        {"lowType",    {&lowFreqDial,&lowGainDial},              &lowBandPanel,  &lowBypassBtn,  {&lowTypeBtn,&lowModeBtn},      {}, {}},
        {"mid1Type",   {&mid1GainDial,&mid1FreqDial,&mid1QDial}, &mid1Panel,     &mid1BypassBtn, {&mid1TypeBtn,&mid1ModeBtn},    {}, {}},
        {"mid2Type",   {&mid2GainDial,&mid2FreqDial,&mid2QDial}, &mid2Panel,     &mid2BypassBtn, {&mid2TypeBtn,&mid2ModeBtn},    {}, {}},
        {"highType",   {&highFreqDial,&highGainDial},            &highBandPanel, &highBypassBtn, {&highTypeBtn,&highModeBtn},    {}, {}},
        {"preampType", {&preampGainDial},                        &preampPanel,   &preampBypassBtn,{&preampPadBtn,&preampTypeBtn},{}, {}},
    };
    for(auto& r:rules) for(auto* k:r.knobs) r.knobBase.push_back(k->getAccent());
    lookFrom=lookTo=Theme::current();
    // Looks: built-in defaults, then the baked Designs/LookTriggers.csv, then this project's saved edits.
    lookTable.fromCsv(designText(hybridEQLooks,"LookTriggers.csv"));
#if !GOODLOOKINUI_ENABLE_EDITOR
    if(auto saved=proc.apvts.state.getProperty("lookTable","").toString(); saved.isNotEmpty()) lookTable.fromCsv(saved.toStdString());
#endif
#if GOODLOOKINUI_ENABLE_EDITOR
    lookStudio=std::make_unique<LookStudio>(lookTable,
        std::vector<LookStudio::Section>{{"lcSlope","Low Cut: Slope"},{"hcSlope","High Cut: Slope"},{"lowType","Low Band: Type"},
            {"mid1Type","Mid 1: Type"},{"mid2Type","Mid 2: Type"},{"highType","High Band: Type"},{"preampType","Preamp: Type"}},
        [this](const std::string& id){
            juce::StringArray out;
            if(auto* p=dynamic_cast<juce::AudioParameterChoice*>(proc.apvts.getParameter(id))) out=p->choices;
            return out;});
    lookStudio->onChanged=[this]{
        persistLooks();
        proc.apvts.state.setProperty("lookTrigger","control",nullptr);   // so the edit is visible straight away
        applyLook(false);
    };
    lookStudio->note=[this]{return getTriggerMode()==TriggerMode::Control?juce::String("(Per section is on)"):juce::String("(turn on LOOK > Per section to see it)");};
    addChildComponent(*lookStudio);
    spectrumStudio=std::make_unique<SpectrumStudio>();
    spectrumStudio->show(spectrumRenderer->getSettings());
    spectrumStudio->onChanged=[this](const SpectrumSettings& s){applySpectrum(s,true);};
    addChildComponent(*spectrumStudio);
    resized();
#endif

    knobStyleBtn.setLookAndFeel(&knobStyleLnf);
    knobStyleBtn.setColour(juce::TextButton::buttonColourId, Theme::buttonTop);
    knobStyleBtn.setColour(juce::TextButton::textColourOffId, Theme::buttonText);
    knobStyleBtn.onClick=[this]{showKnobStyleMenu();};
#if GOODLOOKINUI_ENABLE_EDITOR
    // Developer-only: how a project's look and colour trigger are chosen.
    globalPanel.addAndMakeVisible(lookBtn);
    lookBtn.setBounds(141,100,70,24);
    lookBtn.setButtonText("LOOK");
    lookBtn.setLookAndFeel(&knobStyleLnf);
    lookBtn.setColour(juce::TextButton::buttonColourId, Theme::buttonTop);
    lookBtn.setColour(juce::TextButton::textColourOffId, Theme::buttonText);
    lookBtn.onClick=[this]{showLookMenu();};
#endif
    applyKnobStyle(proc.apvts.state.getProperty("knobStyle","").toString());
    refreshBypassStates();
    applyLook(false);
    applyMeterStyles();

#if GOODLOOKINUI_ENABLE_EDITOR
    // Developer mode: every design change is written straight into Designs/ (see GOODLOOKINUI_CHANGES.md).
    // Knobs save their BASE style and colour (what Designs/HybridEQ.csv means); per-section overrides go to LookTriggers.csv.
    designStudio.saveTransform=[this](const goodlookinui::Item& it){
        auto out=it;
        if(auto found=knobById.find(it.id);found!=knobById.end()) {
            auto* knob=found->second;
            for(auto& k:knobDefaults) if(k.first==knob) out.style=k.second;
            for(auto& r:rules) for(size_t n=0;n<r.knobs.size();++n)
                if(r.knobs[n]==knob) out.colour="#"+r.knobBase[n].toDisplayString(false).toStdString();
        }
        return out;
    };
    designStudio.onChanged=[this]{saveKnobsFile();};
    autosave.onSaved=[this](const DesignAutosave::Event& e){
        juce::String msg;
        if(!e.ok) msg="NOT SAVED  "+e.name+"  -  "+e.error;
        else if(e.wrote) msg="Saved  "+e.name+"  ->  "+e.file.getFullPathName()+"   ("+e.time.formatted("%H:%M:%S")+")   commit it with git";
        else return;
        designStudio.setStatus(msg);
        if(lookStudio) lookStudio->setStatus(msg);
    };
#endif
    proc.apvts.state.addListener(this);   // last, so start-up housekeeping never triggers a save
}

// Meter face and fader cap: set by the developer (LOOK menu), saved with the project.
void HybridEQEditor::applyMeterStyles()
{
    goodlookinui::juce_adapter::retro::glowEnabled=proc.apvts.state.getProperty("glow","on").toString()!="off";
    meterPanel.setFace(MeterPanel::faceFromCode(proc.apvts.state.getProperty("meterFace","cream").toString()));
    harmonicsPanel.setFaderStyle(HarmonicsPanel::faderIndex(proc.apvts.state.getProperty("faderStyle","oval").toString()));
}

void HybridEQEditor::applySpectrum(const SpectrumSettings& s, bool save)
{
    spectrumRenderer->setSettings(s);
    if (spectrumOverlay) spectrumOverlay->setBase(s);
    if (save) proc.apvts.state.setProperty("spectrum", s.toString(), nullptr);
    eqDisplay->refreshRate();
    eqDisplay->repaint();
}

void HybridEQEditor::persistLooks()
{
    proc.apvts.state.setProperty("lookTable",juce::String(lookTable.toCsv()),nullptr);
}

// Developer inspector edit of a knob. Label, font and geometry always go to the knob.
// Style and colour go to the section look of the section's current choice when the
// "Per section" trigger is on (so they survive switching the type); otherwise they
// are the knob's own saved design.
void HybridEQEditor::onKnobDesignEdited(RotaryKnob& knob,const goodlookinui::Item& item)
{
    if(rules.empty()) {knob.applyDesign(item);return;}   // still constructing
    Rule* rule=nullptr;size_t idx=0;
    for(auto& r:rules) for(size_t n=0;n<r.knobs.size();++n) if(r.knobs[n]==&knob){rule=&r;idx=n;}
    if(getTriggerMode()==TriggerMode::Control && rule) {
        const auto choice=choiceName(proc.apvts,rule->param).toStdString();
        Look l;
        if(auto* e=lookTable.find(rule->param,choice)) l=*e; else {l.parameter=rule->param;l.choice=choice;}
        l.style=item.style;l.colour=item.colour;
        lookTable.set(l);persistLooks();
        auto keep=item;keep.style=knob.getDesign().style;keep.colour=knob.getDesign().colour;
        knob.applyDesign(keep);
#if GOODLOOKINUI_ENABLE_EDITOR
        if(lookStudio) lookStudio->refresh();
#endif
    } else {
        for(auto& k:knobDefaults) if(k.first==&knob) k.second=item.style;
        if(rule) rule->knobBase[idx]=juce::Colour::fromString("ff"+juce::String(item.colour.substr(1)));
        knob.applyDesign(item);
    }
    applyLook(false);
}

#if GOODLOOKINUI_ENABLE_EDITOR
void HybridEQEditor::highlightKnob(juce::Component* c)
{
    if(!c) {ring.setVisible(false);return;}
    ring.setBounds(panelSurface.getLocalArea(c,c->getLocalBounds()).expanded(4));
    ring.setVisible(designStudio.isVisible());
    ring.toFront(false);
}
#endif

TriggerMode HybridEQEditor::getTriggerMode() const
{
    const auto m=proc.apvts.state.getProperty("lookTrigger","").toString();
    if(m=="control") return TriggerMode::Control;
    if(m=="faceplate") return TriggerMode::Faceplate;
    if(m=="off") return TriggerMode::Off;
    return LookTriggers::defaultMode;
}

static void quadFor(const goodlookinui::Faceplate& f,juce::Colour q[4])
{
    q[0]=juce::Colour(f.panelTop);q[1]=juce::Colour(f.panelBottom);q[2]=juce::Colour(f.text);q[3]=juce::Colour(f.textMid);
}

void HybridEQEditor::applyLook(bool animate)
{
    const auto mode=getTriggerMode();
    juce::String code=proc.apvts.state.getProperty("lookFaceplate",LookTriggers::defaultFaceplate).toString();
    if(mode==TriggerMode::Faceplate)
        if(auto* f=lookTable.find(LookTriggers::faceplateDriver,choiceName(proc.apvts,LookTriggers::faceplateDriver).toStdString()); f&&!f->faceplate.empty())
            code=f->faceplate;
    const auto* fp=goodlookinui::findFaceplate(code.toStdString());
    if(!fp) fp=&goodlookinui::faceplates[0];
    lookFrom=Theme::current();
    lookTo=Theme::Look::from(*fp);
    knobFades.clear();panelFades.clear();
    for(auto& r:rules) {
        const Look* e=nullptr;
        if(mode==TriggerMode::Control)
            e=lookTable.find(r.param,choiceName(proc.apvts,r.param).toStdString());
        const bool hasColour=e&&!e->colour.empty();
        const juce::Colour flavour=hasColour?juce::Colour::fromString("ff"+juce::String(e->colour.substr(1))):juce::Colour();
        r.styleNow=e?e->style:"";
        for(size_t n=0;n<r.knobs.size();++n)
            knobFades.push_back({r.knobs[n],r.knobs[n]->getAccent(),hasColour?flavour:r.knobBase[n]});
        PanelFade pf;
        pf.rule=&r;
        pf.from=r.panel->getAccentColour();
        pf.to=hasColour?flavour:(r.knobBase.empty()?pf.from:r.knobBase.front());
        pf.hadPlate=r.panel->getPlate(pf.plateFrom,&pf.finishFrom);
        if(!pf.hadPlate) {quadFor(*fp,pf.plateFrom);pf.finishFrom=fp->finish;}   // fade in from the current look
        // A trigger entry's plate wins; otherwise the developer's "plate for all sections" choice applies.
        const juce::String allPlate=proc.apvts.state.getProperty("lookPlate","").toString();
        const auto* plate=(e&&!e->plate.empty())?goodlookinui::findFaceplate(e->plate)
                                       :(mode!=TriggerMode::Faceplate&&allPlate.isNotEmpty()?goodlookinui::findFaceplate(allPlate.toStdString()):nullptr);
        pf.wantsPlate=plate!=nullptr;
        quadFor(plate?*plate:*fp,pf.plateTo);
        pf.finishTo=(plate?*plate:*fp).finish;
        panelFades.push_back(pf);
    }
    refreshKnobStyles();
    fadeT=animate?0.0f:1.0f;
    if(animate) startTimerHz(60); else {stopTimer();stepLook(1.0f);}
}

void HybridEQEditor::stepLook(float t)
{
    Theme::apply(lookFrom.mix(lookTo,t));
    for(auto& f:knobFades) f.knob->setAccent(f.from.interpolatedWith(f.to,t));
    for(auto& f:panelFades) {
        auto& r=*f.rule;
        const auto c=f.from.interpolatedWith(f.to,t);
        r.panel->setAccentColour(c);r.bypass->setAccent(c);
        if(f.wantsPlate || (f.hadPlate && t<1.0f)) {
            juce::Colour q[4];
            for(int i=0;i<4;++i) q[i]=f.plateFrom[i].interpolatedWith(f.plateTo[i],t);
            r.panel->setPlate(q[0],q[1],q[2],q[3],t<0.5f?f.finishFrom:f.finishTo);
            for(auto* k:r.knobs) k->setPlateText(&q[2],&q[3]);
            for(auto* b:r.buttons) b->setPlateText(&q[2],&q[3]);
        } else {
            r.panel->clearPlate();
            for(auto* k:r.knobs) k->setPlateText(nullptr,nullptr);
            for(auto* b:r.buttons) b->setPlateText(nullptr,nullptr);
        }
    }
    for(auto& k:knobDefaults) k.first->refreshTheme();
    for(auto* b:cycleButtons) b->refreshTheme();
    harmonicsPanel.refreshTheme();
    repaint();
}

void HybridEQEditor::timerCallback()
{
    fadeT=juce::jmin(1.0f,fadeT+1.0f/60.0f/0.25f);
    const float eased=fadeT*fadeT*(3.0f-2.0f*fadeT);
    stepLook(eased);
    if(fadeT>=1.0f) stopTimer();
    oversampleBtn.setDisplayTextOverride(oversampleDisplayText());
}

// Text for the Oversample key: the factor the engine is really running at. The headroom floor can push
// this above the user's choice, so show the effective value and mark it when it differs.
juce::String HybridEQEditor::oversampleDisplayText() const
{
    const int effective = proc.getEffectiveOversampleFactor();
    const auto* param = dynamic_cast<juce::AudioParameterChoice*>(proc.apvts.getParameter("oversampleMode"));
    const int chosen = param != nullptr ? param->getIndex() : 0;
    const juce::String text = juce::String(effective) + "x";
    return effective == chosen ? text : text + "*";
}

void HybridEQEditor::showLookMenu()
{
    juce::PopupMenu menu;
    const auto mode=getTriggerMode();
    menu.addSectionHeader("Colour trigger");
    menu.addItem(1,"Off",true,mode==TriggerMode::Off);
    menu.addItem(2,"Per section (knob style, colour and plate follow each parameter)",true,mode==TriggerMode::Control);
    menu.addItem(3,"Whole UI (faceplate follows Preamp type)",true,mode==TriggerMode::Faceplate);
    menu.addSeparator();
    const auto code=proc.apvts.state.getProperty("lookFaceplate",LookTriggers::defaultFaceplate).toString();
    const auto plateCode=proc.apvts.state.getProperty("lookPlate","").toString();
    juce::PopupMenu faces,plates;
    plates.addItem(500,"None (follow trigger / faceplate)",true,plateCode.isEmpty());
    plates.addSeparator();
    int n=0;
    for(const auto& f:goodlookinui::faceplates) {
        faces.addItem(1000+n,juce::String(f.code)+"  -  "+f.name,mode!=TriggerMode::Faceplate,code==f.code);
        plates.addItem(2000+n,juce::String(f.code)+"  -  "+f.name,mode!=TriggerMode::Faceplate,plateCode==f.code);
        ++n;
    }
    juce::PopupMenu meters,pro,retro;
    const auto face=juce::String(MeterPanel::faceCode(meterPanel.getFace()));
    for(int i=0;i<int(std::size(MeterPanel::faces));++i)
        (MeterPanel::faces[i].retro?retro:pro).addItem(3000+i,MeterPanel::faces[i].name,true,face==MeterPanel::faces[i].code);
    meters.addSubMenu("Meter face: professional hardware",pro);
    meters.addSubMenu("Meter face: retro / 80s / cyberpunk",retro);
    meters.addSeparator();
    const auto fader=proc.apvts.state.getProperty("faderStyle","oval").toString();
    const char* faderNames[]={"Fader: oval cap (pro)","Fader: console cap (pro)","Fader: neon (retro)","Fader: bar graph (retro)","Fader: chamfer (cyberpunk)","Fader: wedge ramp (80s dash)","Fader: flat bar (clean)"};
    for(int i=0;i<7;++i) meters.addItem(3100+i,faderNames[i],true,fader==HarmonicsPanel::faderCodes[i]);
    meters.addSeparator();
    meters.addItem(3200,"Glow, scanlines and flicker (neon look)",true,goodlookinui::juce_adapter::retro::glowEnabled);
    menu.addSubMenu("Meter face and fader style",meters);
    menu.addSubMenu("Faceplate (whole UI)",faces);
    menu.addSubMenu("Plate for all sections",plates);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&lookBtn),
        [safe=juce::Component::SafePointer<HybridEQEditor>(this)](int r){
            if(!safe||r<=0)return;
            auto& st=safe->proc.apvts.state;
            if(r<=3) st.setProperty("lookTrigger",r==1?"off":r==2?"control":"faceplate",nullptr);
            else if(r>=3000&&r<3000+int(std::size(MeterPanel::faces))){st.setProperty("meterFace",MeterPanel::faces[r-3000].code,nullptr);safe->applyMeterStyles();return;}
            else if(r==3200){st.setProperty("glow",goodlookinui::juce_adapter::retro::glowEnabled?"off":"on",nullptr);safe->applyMeterStyles();return;}
            else if(r>=3100&&r<3107){st.setProperty("faderStyle",HarmonicsPanel::faderCodes[r-3100],nullptr);safe->applyMeterStyles();return;}
            else if(r==500) st.setProperty("lookPlate","",nullptr);
            else if(r>=2000) st.setProperty("lookPlate",juce::String(goodlookinui::faceplates[r-2000].code),nullptr);
            else st.setProperty("lookFaceplate",juce::String(goodlookinui::faceplates[r-1000].code),nullptr);
            safe->applyLook(true);
        });
}

void HybridEQEditor::applyKnobStyle(const juce::String& code)
{
    // Empty code = no global override: saved design styles (or a section's trigger style) apply.
    proc.apvts.state.setProperty("knobStyle", code, nullptr);
    knobStyleBtn.setButtonText("KNOBS: " + (code.isEmpty() ? juce::String("DESIGN") : code));
    refreshKnobStyles();
}

void HybridEQEditor::refreshKnobStyles()
{
    const auto global=proc.apvts.state.getProperty("knobStyle","").toString().toStdString();
    for(auto& [knob,baked]:knobDefaults) {
        std::string style=baked;
        for(auto& r:rules)
            if(!r.styleNow.empty() && std::find(r.knobs.begin(),r.knobs.end(),knob)!=r.knobs.end()) style=r.styleNow;
        if(!global.empty()) style=global;
        auto item=knob->getDesign();
        if(item.style==style) continue;
        item.style=style;
        knob->applyDesign(item);
    }
}

void HybridEQEditor::showKnobStyleMenu()
{
    juce::PopupMenu menu;
    const auto current=proc.apvts.state.getProperty("knobStyle","").toString();
    menu.addItem(1,"DESIGN (as saved)",true,current.isEmpty());
    menu.addSeparator();
    int id=2;
    for(const auto& s:goodlookinui::knobStyles)
        menu.addItem(id++,juce::String(s.code)+"  -  "+s.description,true,current==s.code);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&knobStyleBtn),
        [safe=juce::Component::SafePointer<HybridEQEditor>(this)](int r){
            if(!safe||r<=0)return;
            safe->applyKnobStyle(r==1?juce::String():juce::String(goodlookinui::knobStyles[r-2].code));
        });
}

HybridEQEditor::~HybridEQEditor()
{
    proc.apvts.state.removeListener(this);
#if GOODLOOKINUI_ENABLE_EDITOR
    autosave.onSaved=nullptr;
    autosave.flush();     // a design edit made just before closing is not lost
#endif
    stopTimer();
    knobStyleBtn.setLookAndFeel(nullptr);
    lookBtn.setLookAndFeel(nullptr);
    for (const auto& id : {"lcBypass", "hcBypass", "lowBypass", "mid1Bypass", "mid2Bypass",
                           "highBypass", "preampBypass", "mid1Type", "mid2Type", "lowType", "highType", "preampType", "lcSlope", "hcSlope"})
        proc.apvts.removeParameterListener(id, this);

    proc.apvts.state.setProperty("consoleEditorWidth", getWidth(), nullptr);
    proc.apvts.state.setProperty("consoleEditorHeight", getHeight(), nullptr);
}

void HybridEQEditor::parameterChanged(const juce::String& id, float)
{
    juce::MessageManager::callAsync([safeThis = juce::Component::SafePointer<HybridEQEditor>(this), id] {
        if (safeThis != nullptr)
        {
            safeThis->refreshBypassStates();
            safeThis->applyLook(true);
#if GOODLOOKINUI_ENABLE_EDITOR
            if (safeThis->lookStudio)
                if (auto* p = dynamic_cast<juce::AudioParameterChoice*>(safeThis->proc.apvts.getParameter(id)))
                    safeThis->lookStudio->follow(id.toStdString(), p->getCurrentChoiceName());
#endif
        }
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
    g.fillAll(Theme::bgFill);
    constexpr float width=1180, height=880;
    const float scale=juce::jmin(float(getWidth())/width,float(getHeight())/height);
    g.addTransform(juce::AffineTransform::scale(scale).translated(
        (getWidth()-width*scale)*0.5f,(getHeight()-height*scale)*0.5f));
    auto body=juce::Rectangle<float>(4,4,width-8,height-8);
    g.setGradientFill(juce::ColourGradient(Theme::editorTop,0,0,Theme::editorBottom,width,height,false));
    g.fillRoundedRectangle(body,5);
    g.setColour(Theme::border);g.drawRoundedRectangle(body,5,1);
    // Fine machined rack rails, deliberately quieter than the controls.
    for(int x:{16,1164}) {
        g.setColour(Theme::rail);g.fillRect(x-5,12,10,856);
        goodlookinui::juce_adapter::drawScrew(g,float(x),24);
        goodlookinui::juce_adapter::drawScrew(g,float(x),856);
    }
    g.setColour(Theme::titleBar);g.fillRect(30,12,1120,50);
    g.setColour(Theme::textDark);g.setFont(juce::FontOptions(28.0f,juce::Font::bold));
    g.drawText("Hybrid EQ",44,16,260,40,juce::Justification::centredLeft);
    g.setFont(juce::FontOptions(10.0f,juce::Font::bold));g.setColour(Theme::textMid);
    g.drawText("O P E N G R I D",310,21,200,18,juce::Justification::centredLeft);
    g.setFont(juce::FontOptions(9.0f));
    g.drawText("SIX-BAND CONSOLE EQUALISER",310,39,260,14,juce::Justification::centredLeft);
    g.setColour(Theme::faceAccent);g.setFont(juce::FontOptions(10.0f,juce::Font::bold));
    g.drawText("ANALOGUE CHARACTER / DIGITAL PRECISION",780,24,350,22,juce::Justification::centredRight);
    g.setColour(Theme::textMid);g.setFont(juce::FontOptions(9.0f,juce::Font::bold));
    g.drawText("FREQUENCY RESPONSE",40,68,200,15,juce::Justification::centredLeft);
    g.drawText("20 Hz - 30 kHz   /   +/-24 dB",900,68,240,15,juce::Justification::centredRight);
    g.setColour(Theme::faceAccent.withAlpha(0.7f));g.fillRect(38.0f,268.0f,28.0f,2.0f);
    g.setColour(Theme::textMid);g.setFont(juce::FontOptions(9.0f,juce::Font::bold));
    g.drawText("EQUALISER",76,260,180,18,juce::Justification::centredLeft);
    g.drawText("FILTERS / FOUR COLOUR BANDS / MID-SIDE",740,260,400,18,juce::Justification::centredRight);
    g.setColour(Theme::footerText);g.setFont(juce::FontOptions(8.0f));
    g.drawText("OPENGRID   /   HYBRID EQ",38,862,600,12,juce::Justification::centredLeft);
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
    designStudio.setBounds(8,32,getWidth()-16,114);
    if(lookStudio) lookStudio->setBounds(8,150,getWidth()-16,74);
    if(spectrumStudio) spectrumStudio->setBounds(8,228,getWidth()-16,112);
#endif
    // Layout once at design resolution. Saved development edits survive resizing.
    if (eqDisplay && eqDisplay->getWidth()>0) return;
    if(eqDisplay) eqDisplay->setBounds(38,88,1104,166);
    spectrumBtn.setBounds(690,22,90,24);
    spectrumOverlay->setBounds(0,0,canvasWidth,canvasHeight);
    SectionPanel* strips[]{&lowCutPanel,&lowBandPanel,&mid1Panel,&mid2Panel,&highBandPanel,&highCutPanel};
    for(int n=0;n<6;++n) strips[n]->setBounds(38+n*186,282,174,430);
    // Bottom row: input, harmonics, output, level meter (bottom right).
    preampPanel.setBounds(38,726,292,132);
    harmonicsPanel.setBounds(342,726,236,132);
    globalPanel.setBounds(590,726,250,132);
    meterPanel.setBounds(852,726,290,132);
}
