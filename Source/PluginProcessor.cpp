#include "PluginProcessor.h"
#include "PluginEditor.h"

static juce::NormalisableRange<float> makeLogRange(float min, float max, float mid)
{
    return juce::NormalisableRange<float>(
        min, max,
        [=](float rangeStart, float rangeEnd, float normalised) {
            return rangeStart * std::pow(rangeEnd / rangeStart, normalised);
        },
        [=](float rangeStart, float rangeEnd, float value) {
            return std::log(value / rangeStart) / std::log(rangeEnd / rangeStart);
        },
        [=](float rangeStart, float rangeEnd, float value) {
            (void)rangeStart; (void)rangeEnd;
            return value;
        });
}

juce::AudioProcessorValueTreeState::ParameterLayout HybridEQProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"oversampleMode", 1}, "Oversample",
        juce::StringArray{"1x", "4x", "8x"}, 0));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"harmonic2nd", 1}, "2nd Harmonic",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"harmonic3rd", 1}, "3rd Harmonic",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"harmonic4th", 1}, "4th Harmonic",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"harmonic5th", 1}, "5th Harmonic",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"lcFreq", 1}, "Low Cut Freq",
        makeLogRange(20.0f, 1000.0f, 200.0f), 20.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"lcQ", 1}, "Low Cut Q",
        juce::NormalisableRange<float>(0.1f, 10.0f, 0.01f, 0.5f), 0.707f));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"lcSlope", 1}, "Low Cut Slope",
        juce::StringArray{"-6 dB/oct", "-12 dB/oct", "-18 dB/oct", "-24 dB/oct"}, 1));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"lcMode", 1}, "Low Cut Mode",
        juce::StringArray{"Stereo", "Mid", "Side"}, 0));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"lcBypass", 1}, "Low Cut Bypass", false));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"hcFreq", 1}, "High Cut Freq",
        makeLogRange(1000.0f, 30000.0f, 8000.0f), 20000.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"hcQ", 1}, "High Cut Q",
        juce::NormalisableRange<float>(0.1f, 10.0f, 0.01f, 0.5f), 0.707f));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"hcSlope", 1}, "High Cut Slope",
        juce::StringArray{"-6 dB/oct", "-12 dB/oct", "-18 dB/oct", "-24 dB/oct"}, 1));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"hcMode", 1}, "High Cut Mode",
        juce::StringArray{"Stereo", "Mid", "Side"}, 0));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"hcBypass", 1}, "High Cut Bypass", false));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"lowFreq", 1}, "Low Freq",
        makeLogRange(20.0f, 400.0f, 100.0f), 100.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"lowGain", 1}, "Low Gain",
        juce::NormalisableRange<float>(-18.0f, 18.0f, 0.1f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"lowType", 1}, "Low Type",
        juce::StringArray{"Bax-EQ", "Brit", "FSF"}, 0));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"lowMode", 1}, "Low Mode",
        juce::StringArray{"Stereo", "Mid", "Side"}, 0));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"lowBypass", 1}, "Low Band Bypass", false));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"mid1Freq", 1}, "Mid 1 Freq",
        makeLogRange(200.0f, 6000.0f, 1000.0f), 800.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"mid1Gain", 1}, "Mid 1 Gain",
        juce::NormalisableRange<float>(-18.0f, 18.0f, 0.1f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"mid1Q", 1}, "Mid 1 Q",
        juce::NormalisableRange<float>(0.1f, 10.0f, 0.01f, 0.5f), 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"mid1Type", 1}, "Mid 1 Type",
        juce::StringArray{"N-EQ", "Brit", "A-Type"}, 0));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"mid1Mode", 1}, "Mid 1 Mode",
        juce::StringArray{"Stereo", "Mid", "Side"}, 0));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"mid1Bypass", 1}, "Mid 1 Bypass", false));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"mid2Freq", 1}, "Mid 2 Freq",
        makeLogRange(200.0f, 6000.0f, 1000.0f), 2500.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"mid2Gain", 1}, "Mid 2 Gain",
        juce::NormalisableRange<float>(-18.0f, 18.0f, 0.1f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"mid2Q", 1}, "Mid 2 Q",
        juce::NormalisableRange<float>(0.1f, 10.0f, 0.01f, 0.5f), 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"mid2Type", 1}, "Mid 2 Type",
        juce::StringArray{"N-EQ", "Brit", "A-Type"}, 0));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"mid2Mode", 1}, "Mid 2 Mode",
        juce::StringArray{"Stereo", "Mid", "Side"}, 0));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"mid2Bypass", 1}, "Mid 2 Bypass", false));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"highFreq", 1}, "High Freq",
        makeLogRange(800.0f, 30000.0f, 5000.0f), 8000.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"highGain", 1}, "High Gain",
        juce::NormalisableRange<float>(-18.0f, 18.0f, 0.1f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"highType", 1}, "High Type",
        juce::StringArray{"Bax-EQ", "Brit", "FSF"}, 0));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"highMode", 1}, "High Mode",
        juce::StringArray{"Stereo", "Mid", "Side"}, 0));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"highBypass", 1}, "High Band Bypass", false));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"preampType", 1}, "Preamp Type",
        juce::StringArray{"Brit", "N-Type", "FSF", "Off", "A-Type"}, 0));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"preampPad", 1}, "Preamp Pad",
        juce::StringArray{"-20 dB", "Unity", "+10 dB"}, 1));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"preampGain", 1}, "Preamp Gain",
        juce::NormalisableRange<float>(-12.0f, 24.0f, 0.1f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"preampBypass", 1}, "Preamp Bypass", false));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"preampCircuit", 1}, "Preamp Circuit", false));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"outputGain", 1}, "Output Gain",
        juce::NormalisableRange<float>(-24.0f, 24.0f, 0.1f), 0.0f));

    return {params.begin(), params.end()};
}

HybridEQProcessor::HybridEQProcessor()
    : AudioProcessor(BusesProperties()
          .withInput("Input", juce::AudioChannelSet::stereo(), true)
          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", createParameterLayout())
{
}

void HybridEQProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    sampleRateAtomic.store(sampleRate, std::memory_order_relaxed);
    currentSampleRate = sampleRate;
    multirateEngine.prepare(sampleRate, samplesPerBlock, getTotalNumOutputChannels());
    preamp.prepare(sampleRate);
    eq.prepare(sampleRate, samplesPerBlock);
    // Hosts read the latency right after prepareToPlay, so settle the oversampling factor (and with it
    // the reported latency) now instead of on the first audio block. This is the only place the
    // reported latency is ever changed.
    updateParameters();
    setLatencySamples(static_cast<int>(std::round(multirateEngine.getLatencySamples())));
}

void HybridEQProcessor::releaseResources() {}

void HybridEQProcessor::updateParameters()
{
    auto getFloat = [&](const juce::String& id) {
        return apvts.getRawParameterValue(id)->load();
    };
    auto getChoice = [&](const juce::String& id) {
        return static_cast<int>(apvts.getRawParameterValue(id)->load());
    };
    auto getBool = [&](const juce::String& id) {
        return apvts.getRawParameterValue(id)->load() > 0.5f;
    };

    // The "Oversample" control only offers 1x/4x/8x - 2x is never a user
    // choice - but the engine itself also supports an internal 2x step
    // (dsp::OversampleMode::TwoX) purely so headroom protection (below) can
    // stop there instead of jumping straight to 4x when 2x is already
    // enough. That internal step never touches the parameter or the visible
    // control; it is applied to the engine only.
    static constexpr dsp::OversampleMode allModes[] = {
        dsp::OversampleMode::Native, dsp::OversampleMode::TwoX,
        dsp::OversampleMode::FourX, dsp::OversampleMode::EightX
    };
    static constexpr int allFactors[] = {1, 2, 4, 8};
    static constexpr int userFactors[] = {1, 4, 8};

    // Turning the Preamp Circuit model on adds a nonlinear stage that needs
    // more oversampling headroom to stay clean, so it raises the floor to 4x.
    // This is applied to the engine only (below): the audio thread must never
    // write a host-visible parameter, and the "Oversample" choice stays the
    // user's own. The editor shows the effective factor instead.
    //
    // The High Cut, High shelf and bells no longer need extra oversampling to avoid "cramping" near Nyquist:
    // their biquads are fitted to the analog curve at the base rate (see dsp/BiquadFit.h), so even a corner at or
    // above Nyquist follows the analog response in-band. Oversampling is now only the user's choice (Oversample)
    // and what the nonlinear stages need (Circuit forces 4x). With 1x and Circuit off the plug-in has no latency.
    int userFactor = userFactors[getChoice("oversampleMode")];
    if (getBool("preampCircuit")) userFactor = std::max(userFactor, 4);   // circuit needs the headroom
    const int effectiveFactor = userFactor;
    dsp::OversampleMode effectiveMode = dsp::OversampleMode::Native;
    for (int i = 0; i < 4; ++i)
        if (allFactors[i] == effectiveFactor)
            effectiveMode = allModes[i];

    multirateEngine.setMode(effectiveMode);
    // Publish the factor the engine is really running at so the editor can show it (the "Oversample"
    // choice itself may be lower when the headroom floor is doing the work).
    effectiveFactorAtomic.store(effectiveFactor, std::memory_order_relaxed);

    static constexpr float padGains[] = {-20.0f, 0.0f, 10.0f};
    preamp.setType(static_cast<dsp::PreampType>(getChoice("preampType")));
    preamp.setDriveDB(getFloat("preampGain") + padGains[getChoice("preampPad")]);
    preamp.setBypassed(getBool("preampBypass"));
    preamp.setCircuitEnabled(getBool("preampCircuit"));

    saturation.setHarmonicLevels(
        getFloat("harmonic2nd"), getFloat("harmonic3rd"),
        getFloat("harmonic4th"), getFloat("harmonic5th"));

    double effectiveSR = multirateEngine.getEffectiveSampleRate();

    eq.setSampleRate(effectiveSR);
    preamp.setSampleRate(effectiveSR);

    auto ms = [&](const juce::String& id) {
        return static_cast<dsp::MidSideMode>(getChoice(id));
    };
    auto midType = [&](const juce::String& id) {
        return static_cast<dsp::MidBandType>(getChoice(id));
    };

    eq.updateLowCut(
        getFloat("lcFreq"), getFloat("lcQ"),
        static_cast<dsp::CutFilterSlope>(getChoice("lcSlope")), ms("lcMode"));

    eq.updateHighCut(
        getFloat("hcFreq"), getFloat("hcQ"),
        static_cast<dsp::CutFilterSlope>(getChoice("hcSlope")), ms("hcMode"));

    eq.updateLowBand(
        getFloat("lowFreq"), getFloat("lowGain"),
        static_cast<dsp::LowBandType>(getChoice("lowType")), ms("lowMode"));

    eq.updateMid1(getFloat("mid1Freq"), getFloat("mid1Gain"), getFloat("mid1Q"),
                  midType("mid1Type"), ms("mid1Mode"));
    eq.updateMid2(getFloat("mid2Freq"), getFloat("mid2Gain"), getFloat("mid2Q"),
                  midType("mid2Type"), ms("mid2Mode"));

    eq.updateHighBand(
        getFloat("highFreq"), getFloat("highGain"),
        static_cast<dsp::HighBandType>(getChoice("highType")), ms("highMode"));

    eq.setBypass(dsp::BandId::LowCut, getBool("lcBypass"));
    eq.setBypass(dsp::BandId::HighCut, getBool("hcBypass"));
    eq.setBypass(dsp::BandId::LowBand, getBool("lowBypass"));
    eq.setBypass(dsp::BandId::Mid1, getBool("mid1Bypass"));
    eq.setBypass(dsp::BandId::Mid2, getBool("mid2Bypass"));
    eq.setBypass(dsp::BandId::HighBand, getBool("highBypass"));
}

void HybridEQProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    updateParameters();

    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());
    inLevel.push(buffer.getArrayOfReadPointers(), buffer.getNumChannels(), buffer.getNumSamples());
    inRing.push(buffer.getArrayOfReadPointers(), std::min(buffer.getNumChannels(), 2), buffer.getNumSamples());

    // ONE oversampled region wrapping every nonlinear stage (preamp, EQ,
    // harmonics). JUCE's Oversampling requires exactly one up/down cycle per
    // audio block - calling it twice per block corrupts its internal history
    // and is what caused the crackling/dropouts. The bitcrusher is gone, so
    // the whole chain fits in a single pass: upsample -> process -> downsample.
    auto osBlock = multirateEngine.upsample(buffer);
    int numCh = std::min(static_cast<int>(osBlock.getNumChannels()), 2);
    int numSamp = static_cast<int>(osBlock.getNumSamples());
    float* chPtrs[2] = {};
    for (int ch = 0; ch < numCh; ++ch)
        chPtrs[ch] = osBlock.getChannelPointer(static_cast<size_t>(ch));

    preamp.processBlock(chPtrs, numCh, numSamp);
    eq.processBlock(chPtrs, numCh, numSamp);
    for (int ch = 0; ch < numCh; ++ch)
        saturation.processBlock(chPtrs[ch], numSamp);

    multirateEngine.downsample(buffer);

    float outGainDB = apvts.getRawParameterValue("outputGain")->load();
    float outGain = juce::Decibels::decibelsToGain(outGainDB);
    buffer.applyGain(outGain);
    outLevel.push(buffer.getArrayOfReadPointers(), buffer.getNumChannels(), buffer.getNumSamples());
    outRing.push(buffer.getArrayOfReadPointers(), std::min(buffer.getNumChannels(), 2), buffer.getNumSamples());
}

juce::AudioProcessorEditor* HybridEQProcessor::createEditor()
{
    return new HybridEQEditor(*this);
}

void HybridEQProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void HybridEQProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml && xml->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new HybridEQProcessor();
}
