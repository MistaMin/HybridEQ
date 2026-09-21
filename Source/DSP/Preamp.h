#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>
#include <array>
#include <vector>
#include <cmath>
#include "EQFilters.h"

namespace dsp {

enum class PreampType { Brit = 0, N, FSF, Off };

// Input-stage preamp emulation. The drive gain (pad + trim) saturates a
// two-term soft-clip model per flavour:
//   - Brit: SSL-style op-amp preamp  -> mostly odd harmonics, clean up to a
//     fairly hard clip, tiny output-stage even content.
//   - N:    Neve-style transformer/class-A preamp -> softer clip plus a
//     stronger even-harmonic (transformer) term.
//   - FSF:  Focusrite Forte console-channel transformer emulation -> a
//     soft, mildly asymmetric input-transformer curve followed by a
//     second, gentler output-transformer stage, giving a blend of even
//     and odd harmonics at both ends of the gain stage.
//   - Off:  clean drive/trim only, no waveshaping and no harmonics added.
// All non-linear terms vanish at x = 0 so true silence stays silent, and the
// low-level gain is normalised to unity so the trim knob behaves like a
// clean preamp gain until drive actually pushes into the clip.
//
// Optional active-circuit model (setCircuitEnabled): real preamps are not
// flat - coupling capacitors and the transformer/output stage create RC
// poles that tilt both magnitude and PHASE. When enabled, the shaped signal
// passes through a minimum-phase network of a 1st-order high-pass
// (coupling/transformer LF pole) and a 1st-order low-pass (output stage HF
// pole), with pole frequencies per flavour (N ~10 Hz + ~15 kHz, Brit ~22 Hz
// + ~22 kHz). When disabled the stage is magnitude- and phase-flat apart
// from the waveshaper itself.
class PreampEngine {
public:
    void prepare(double sampleRate)
    {
        sr = sampleRate;
        slewCoef = 1.0f - static_cast<float>(std::exp(-1.0 / (sampleRate * 0.04)));
        driveState = 1.0f;
        configuredType = static_cast<PreampType>(-1); // force (re)configure
        configuredOn = false;
        driveRamp.clear();
    }

    void setType(PreampType t) noexcept { type.store(t, std::memory_order_relaxed); }
    void setDriveDB(float db) noexcept { driveTargetDB.store(db, std::memory_order_relaxed); }
    void setBypassed(bool b) noexcept { bypassed.store(b, std::memory_order_relaxed); }
    void setCircuitEnabled(bool enabled) noexcept { circuitOn.store(enabled, std::memory_order_relaxed); }

    // Called every block with the current effective sample rate. The circuit
    // poles are computed from sr, so only force a reconfigure (which also
    // resets the circuit filters' state) when the rate has actually changed
    // - e.g. when the oversampling factor changes. Forcing it unconditionally
    // on every block would reset the filters' history constantly, which
    // shows up as a small gain/response discontinuity at every block
    // boundary and was a source of the level differing between oversampling
    // settings (more blocks per second at higher oversampling meant more
    // resets).
    void setSampleRate(double sampleRate) noexcept
    {
        if (std::abs(sampleRate - sr) < 1e-6)
            return;
        sr = sampleRate;
        slewCoef = 1.0f - static_cast<float>(std::exp(-1.0 / (sampleRate * 0.04)));
        configuredType = static_cast<PreampType>(-1);
    }

    void process(juce::AudioBuffer<float>& buffer) noexcept
    {
        if (bypassed.load(std::memory_order_relaxed))
            return;

        const int numSamples = buffer.getNumSamples();
        const int numChannels = buffer.getNumChannels();

        prepareDriveRamp(numSamples);
        const auto t = type.load(std::memory_order_relaxed);
        const bool on = circuitOn.load(std::memory_order_relaxed);
        ensureCircuitConfig(t, on);

        for (int ch = 0; ch < numChannels; ++ch) {
            auto* data = buffer.getWritePointer(ch);
            if (ch >= kMaxChannels) {
                for (int i = 0; i < numSamples; ++i)
                    data[i] = 0.0f;
                continue;
            }
            processChannel(data, numSamples, ch, t, on);
        }
    }

    // Channel-pointer variant for processing inside an oversampled
    // AudioBlock (the plugin's oversampled input stage).
    void processBlock(float* const* channelData, int numChannels, int numSamples) noexcept
    {
        if (bypassed.load(std::memory_order_relaxed))
            return;

        prepareDriveRamp(numSamples);
        const auto t = type.load(std::memory_order_relaxed);
        const bool on = circuitOn.load(std::memory_order_relaxed);
        ensureCircuitConfig(t, on);

        int numCh = std::min(numChannels, kMaxChannels);
        for (int ch = 0; ch < numCh; ++ch)
            processChannel(channelData[ch], numSamples, ch, t, on);
    }

private:
    static constexpr int kMaxChannels = 2;

    void prepareDriveRamp(int numSamples)
    {
        if (static_cast<int>(driveRamp.size()) < numSamples)
            driveRamp.resize(static_cast<size_t>(numSamples));

        const float target = static_cast<float>(
            std::pow(10.0, driveTargetDB.load(std::memory_order_relaxed) * 0.05));
        for (int i = 0; i < numSamples; ++i) {
            driveState += (target - driveState) * slewCoef;
            driveRamp[static_cast<size_t>(i)] = driveState;
        }
    }

    // Applies per-channel waveshape; the circuit network follows when on.
    void processChannel(float* data, int numSamples, int channel, PreampType t, bool circuit) noexcept
    {
        const float* ramp = driveRamp.data();

        if (t == PreampType::Off) {
            // Clean drive/trim only - no waveshaping, no added harmonics.
            // The circuit network (if enabled) still models the transformer
            // frequency response, just without any nonlinearity.
            if (circuit) {
                for (int i = 0; i < numSamples; ++i) {
                    const float s = data[i] * ramp[i];
                    data[i] = lpFilters[channel].process(hpFilters[channel].process(s));
                }
            } else {
                for (int i = 0; i < numSamples; ++i)
                    data[i] = data[i] * ramp[i];
            }
        } else if (t == PreampType::FSF) {
            // Focusrite Forte-style input + output transformer stage: a
            // soft, slightly asymmetric input curve (odd + a touch of even)
            // followed by a gentler output-transformer curve, so harmonics
            // are generated at both ends of the gain stage like the real
            // console's transformer-coupled input/output.
            constexpr float inW1 = 0.62f, inK1 = 1.15f;
            constexpr float inW2 = 0.38f, inK2 = 2.2f;
            constexpr float inNorm = inW1 * inK1 + inW2 * inK2;
            constexpr float inAsym = 0.06f;
            constexpr float outDrive = 1.35f;
            constexpr float outAsym = 0.08f;
            if (circuit) {
                for (int i = 0; i < numSamples; ++i) {
                    const float s = data[i] * ramp[i];
                    float in = (inW1 * std::tanh(inK1 * s) + inW2 * std::tanh(inK2 * s)) / inNorm
                               + inAsym * s * s;
                    float y = std::tanh(outDrive * in) / outDrive + outAsym * in * std::abs(in);
                    data[i] = lpFilters[channel].process(hpFilters[channel].process(y));
                }
            } else {
                for (int i = 0; i < numSamples; ++i) {
                    const float s = data[i] * ramp[i];
                    float in = (inW1 * std::tanh(inK1 * s) + inW2 * std::tanh(inK2 * s)) / inNorm
                               + inAsym * s * s;
                    data[i] = std::tanh(outDrive * in) / outDrive + outAsym * in * std::abs(in);
                }
            }
        } else if (t == PreampType::N) {
            constexpr float w1 = 0.55f, k1 = 1.05f;
            constexpr float w2 = 0.45f, k2 = 2.7f;
            constexpr float norm = w1 * k1 + w2 * k2;
            if (circuit) {
                for (int i = 0; i < numSamples; ++i) {
                    const float s = data[i] * ramp[i];
                    const float u = s / (1.0f + std::abs(s));
                    float y = (w1 * std::tanh(k1 * s) + w2 * std::tanh(k2 * s)) / norm + 0.10f * u * std::abs(u);
                    data[i] = lpFilters[channel].process(hpFilters[channel].process(y));
                }
            } else {
                for (int i = 0; i < numSamples; ++i) {
                    const float s = data[i] * ramp[i];
                    const float u = s / (1.0f + std::abs(s));
                    data[i] = (w1 * std::tanh(k1 * s) + w2 * std::tanh(k2 * s)) / norm + 0.10f * u * std::abs(u);
                }
            }
        } else {
            constexpr float w1 = 0.72f, k1 = 1.0f;
            constexpr float w2 = 0.28f, k2 = 2.9f;
            constexpr float norm = w1 * k1 + w2 * k2;
            if (circuit) {
                for (int i = 0; i < numSamples; ++i) {
                    const float s = data[i] * ramp[i];
                    const float u = s / (1.0f + std::abs(s));
                    float y = (w1 * std::tanh(k1 * s) + w2 * std::tanh(k2 * s)) / norm + 0.04f * u * std::abs(u);
                    data[i] = lpFilters[channel].process(hpFilters[channel].process(y));
                }
            } else {
                for (int i = 0; i < numSamples; ++i) {
                    const float s = data[i] * ramp[i];
                    const float u = s / (1.0f + std::abs(s));
                    data[i] = (w1 * std::tanh(k1 * s) + w2 * std::tanh(k2 * s)) / norm + 0.04f * u * std::abs(u);
                }
            }
        }
    }

    void ensureCircuitConfig(PreampType t, bool on)
    {
        if (on == configuredOn && t == configuredType)
            return;

        configuredOn = on;
        configuredType = t;

        double hpF = 22.0;
        double lpF = 22000.0;
        if (t == PreampType::N) { hpF = 10.0; lpF = 15000.0; }
        else if (t == PreampType::FSF) { hpF = 15.0; lpF = 24000.0; }
        const auto hpCoeffs = FilterDesign::makeHighPass(sr, hpF, 0.7071);
        const auto lpCoeffs = FilterDesign::makeLowPass(sr, FilterDesign::safeFilterFreq(sr, lpF), 0.7071);

        for (int ch = 0; ch < kMaxChannels; ++ch) {
            hpFilters[static_cast<size_t>(ch)].setCoeffs(hpCoeffs);
            hpFilters[static_cast<size_t>(ch)].reset();
            lpFilters[static_cast<size_t>(ch)].setCoeffs(lpCoeffs);
            lpFilters[static_cast<size_t>(ch)].reset();
        }
    }

    double sr = 44100.0;
    float slewCoef = 0.001f;
    float driveState = 1.0f;
    std::vector<float> driveRamp;

    std::atomic<PreampType> type{PreampType::Brit};
    std::atomic<float> driveTargetDB{0.0f};
    std::atomic<bool> bypassed{false};
    std::atomic<bool> circuitOn{false};

    PreampType configuredType = PreampType::Brit;
    bool configuredOn = false;

    std::array<BiquadProcessor, kMaxChannels> hpFilters;
    std::array<BiquadProcessor, kMaxChannels> lpFilters;
};

} // namespace dsp
