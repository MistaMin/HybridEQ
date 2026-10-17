#pragma once
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <memory>

namespace dsp {

// TwoX exists purely as an internal safety step: it is never a user-selectable
// choice (the "Oversample" parameter only offers 1x/4x/8x), but the engine can
// still be driven to it silently to keep a cut/shelf frequency clear of
// Nyquist without jumping straight to 4x when 2x headroom is already enough.
enum class OversampleMode { Native, TwoX, FourX, EightX };
enum class ResamplerFilter { IIR = 0, FIR = 1 };

class MultirateEngine {
public:
    void prepare(double sampleRate, int samplesPerBlock, int numChannels)
    {
        baseSampleRate = sampleRate;
        baseBlockSize = samplesPerBlock;
        channels = numChannels;

        for (int f = 0; f < 2; ++f) {
            const auto filterType = (f == 0)
                ? juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR
                : juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple;

            oversamplers2x[f] = std::make_unique<juce::dsp::Oversampling<float>>(
                numChannels, 1, filterType, true);
            oversamplers4x[f] = std::make_unique<juce::dsp::Oversampling<float>>(
                numChannels, 2, filterType, true);
            oversamplers8x[f] = std::make_unique<juce::dsp::Oversampling<float>>(
                numChannels, 3, filterType, true);

            oversamplers2x[f]->initProcessing(static_cast<size_t>(samplesPerBlock));
            oversamplers4x[f]->initProcessing(static_cast<size_t>(samplesPerBlock));
            oversamplers8x[f]->initProcessing(static_cast<size_t>(samplesPerBlock));
        }
    }

    void setMode(OversampleMode m)
    {
        if (m == mode)
            return;
        mode = m;
        reset();
    }
    OversampleMode getMode() const { return mode; }

    void setFilter(ResamplerFilter f)
    {
        if (f == filter)
            return;
        filter = f;
        reset();
    }
    ResamplerFilter getFilter() const { return filter; }

    void reset()
    {
        for (int f = 0; f < 2; ++f) {
            if (oversamplers2x[f]) oversamplers2x[f]->reset();
            if (oversamplers4x[f]) oversamplers4x[f]->reset();
            if (oversamplers8x[f]) oversamplers8x[f]->reset();
        }
    }

    double getEffectiveSampleRate() const
    {
        switch (mode) {
            case OversampleMode::Native: return baseSampleRate;
            case OversampleMode::TwoX:   return baseSampleRate * 2.0;
            case OversampleMode::FourX:  return baseSampleRate * 4.0;
            case OversampleMode::EightX: return baseSampleRate * 8.0;
        }
        return baseSampleRate;
    }

    juce::dsp::AudioBlock<float> upsample(juce::AudioBuffer<float>& buffer)
    {
        juce::dsp::AudioBlock<float> block(buffer);
        const int f = static_cast<int>(filter);
        switch (mode) {
            case OversampleMode::TwoX:
                return oversamplers2x[f]->processSamplesUp(block);
            case OversampleMode::FourX:
                return oversamplers4x[f]->processSamplesUp(block);
            case OversampleMode::EightX:
                return oversamplers8x[f]->processSamplesUp(block);
            case OversampleMode::Native:
            default:
                return block;
        }
    }

    void downsample(juce::AudioBuffer<float>& buffer)
    {
        juce::dsp::AudioBlock<float> block(buffer);
        const int f = static_cast<int>(filter);
        switch (mode) {
            case OversampleMode::TwoX:
                oversamplers2x[f]->processSamplesDown(block);
                break;
            case OversampleMode::FourX:
                oversamplers4x[f]->processSamplesDown(block);
                break;
            case OversampleMode::EightX:
                oversamplers8x[f]->processSamplesDown(block);
                break;
            case OversampleMode::Native:
            default:
                break;
        }
    }

    float getLatencySamples() const
    {
        const int f = static_cast<int>(filter);
        switch (mode) {
            case OversampleMode::TwoX:   return oversamplers2x[f] ? oversamplers2x[f]->getLatencyInSamples() : 0.0f;
            case OversampleMode::FourX:  return oversamplers4x[f] ? oversamplers4x[f]->getLatencyInSamples() : 0.0f;
            case OversampleMode::EightX: return oversamplers8x[f] ? oversamplers8x[f]->getLatencyInSamples() : 0.0f;
            default: return 0.0f;
        }
    }

private:
    double baseSampleRate = 44100.0;
    int baseBlockSize = 512;
    int channels = 2;
    OversampleMode mode = OversampleMode::Native;
    ResamplerFilter filter = ResamplerFilter::IIR;

    std::unique_ptr<juce::dsp::Oversampling<float>> oversamplers2x[2];
    std::unique_ptr<juce::dsp::Oversampling<float>> oversamplers4x[2];
    std::unique_ptr<juce::dsp::Oversampling<float>> oversamplers8x[2];
};

} // namespace dsp
