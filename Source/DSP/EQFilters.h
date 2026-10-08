#pragma once
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <array>
#include <vector>
#include <algorithm>
#include <complex>
#include "BiquadFit.h"

namespace dsp {

struct BiquadCoeffs {
    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;

    std::complex<double> response(double normFreq) const
    {
        double w = 2.0 * juce::MathConstants<double>::pi * normFreq;
        std::complex<double> z = std::exp(std::complex<double>(0.0, w));
        std::complex<double> z2 = z * z;
        auto num = b0 + b1 / z + b2 / z2;
        auto den = 1.0 + a1 / z + a2 / z2;
        return num / den;
    }
};

class BiquadProcessor {
public:
    void setCoeffs(const BiquadCoeffs& c) noexcept { coeffs = c; }
    void reset() noexcept { z1 = z2 = 0.0; }

    float process(float x) noexcept
    {
        double in = static_cast<double>(x);
        double out = coeffs.b0 * in + z1;
        z1 = coeffs.b1 * in - coeffs.a1 * out + z2;
        z2 = coeffs.b2 * in - coeffs.a2 * out;
        return static_cast<float>(out);
    }

    void processBlock(float* data, int n) noexcept
    {
        for (int i = 0; i < n; ++i)
            data[i] = process(data[i]);
    }

    const BiquadCoeffs& getCoeffs() const noexcept { return coeffs; }

private:
    BiquadCoeffs coeffs;
    double z1 = 0.0, z2 = 0.0;
};

namespace FilterDesign {

inline BiquadCoeffs makeBypass()
{
    return {1.0, 0.0, 0.0, 0.0, 0.0};
}

inline BiquadCoeffs makeLowPass1st(double sampleRate, double freq)
{
    double w0 = 2.0 * juce::MathConstants<double>::pi * freq / sampleRate;
    double alpha = std::sin(w0) / (2.0 * 0.7071);
    double cosw0 = std::cos(w0);
    double a0 = 1.0 + alpha;
    return {
        (1.0 - cosw0) / 2.0 / a0,
        (1.0 - cosw0) / a0,
        (1.0 - cosw0) / 2.0 / a0,
        -2.0 * cosw0 / a0,
        (1.0 - alpha) / a0
    };
}

inline BiquadCoeffs makeHighPass1st(double sampleRate, double freq)
{
    double w0 = 2.0 * juce::MathConstants<double>::pi * freq / sampleRate;
    double alpha = std::sin(w0) / (2.0 * 0.7071);
    double cosw0 = std::cos(w0);
    double a0 = 1.0 + alpha;
    return {
        (1.0 + cosw0) / 2.0 / a0,
        -(1.0 + cosw0) / a0,
        (1.0 + cosw0) / 2.0 / a0,
        -2.0 * cosw0 / a0,
        (1.0 - alpha) / a0
    };
}

inline BiquadCoeffs makeLowPass(double sampleRate, double freq, double Q)
{
    double w0 = 2.0 * juce::MathConstants<double>::pi * freq / sampleRate;
    double alpha = std::sin(w0) / (2.0 * Q);
    double cosw0 = std::cos(w0);
    double a0 = 1.0 + alpha;
    return {
        (1.0 - cosw0) / 2.0 / a0,
        (1.0 - cosw0) / a0,
        (1.0 - cosw0) / 2.0 / a0,
        -2.0 * cosw0 / a0,
        (1.0 - alpha) / a0
    };
}

inline BiquadCoeffs makeHighPass(double sampleRate, double freq, double Q)
{
    double w0 = 2.0 * juce::MathConstants<double>::pi * freq / sampleRate;
    double alpha = std::sin(w0) / (2.0 * Q);
    double cosw0 = std::cos(w0);
    double a0 = 1.0 + alpha;
    return {
        (1.0 + cosw0) / 2.0 / a0,
        -(1.0 + cosw0) / a0,
        (1.0 + cosw0) / 2.0 / a0,
        -2.0 * cosw0 / a0,
        (1.0 - alpha) / a0
    };
}

inline BiquadCoeffs makePeakEQ(double sampleRate, double freq, double Q, double gainDB)
{
    double A = std::pow(10.0, gainDB / 40.0);
    double w0 = 2.0 * juce::MathConstants<double>::pi * freq / sampleRate;
    double alpha = std::sin(w0) / (2.0 * Q);
    double cosw0 = std::cos(w0);
    double a0 = 1.0 + alpha / A;
    return {
        (1.0 + alpha * A) / a0,
        -2.0 * cosw0 / a0,
        (1.0 - alpha * A) / a0,
        -2.0 * cosw0 / a0,
        (1.0 - alpha / A) / a0
    };
}

inline BiquadCoeffs makeLowShelf(double sampleRate, double freq, double Q, double gainDB)
{
    double A = std::pow(10.0, gainDB / 40.0);
    double w0 = 2.0 * juce::MathConstants<double>::pi * freq / sampleRate;
    double alpha = std::sin(w0) / (2.0 * Q);
    double cosw0 = std::cos(w0);
    double sqrtA = std::sqrt(A);
    double a0 = (A + 1.0) + (A - 1.0) * cosw0 + 2.0 * sqrtA * alpha;
    return {
        (A * ((A + 1.0) - (A - 1.0) * cosw0 + 2.0 * sqrtA * alpha)) / a0,
        (2.0 * A * ((A - 1.0) - (A + 1.0) * cosw0)) / a0,
        (A * ((A + 1.0) - (A - 1.0) * cosw0 - 2.0 * sqrtA * alpha)) / a0,
        (-2.0 * ((A - 1.0) + (A + 1.0) * cosw0)) / a0,
        ((A + 1.0) + (A - 1.0) * cosw0 - 2.0 * sqrtA * alpha) / a0
    };
}

// Pure numerical safety net - NOT a curve-shaping tool. A bilinear-transform
// biquad becomes unstable if asked to design for a frequency at/above
// Nyquist, so this only clamps the pathological case (freq >= ~Nyquist) to
// stop NaNs/blowups. It is the fallback/standard design only: corners above
// ~10 % of the sample rate are fitted to the analog curve instead
// (AnalogEQ, BiquadFit.h), which avoids cramping without oversampling.
inline double safeFilterFreq(double sampleRate, double freq)
{
    return std::min(freq, sampleRate * 0.49);
}

inline double safeShelfFreq(double sampleRate, double freq)
{
    return safeFilterFreq(sampleRate, freq);
}

// Legacy: the minimum sample rate at which `freq` sits at or below 75 % of
// Nyquist, i.e. where a bilinear design still follows its analog prototype.
// The plug-in used to raise its oversampling factor to satisfy this; it no
// longer does (the EQ fits the analog curve directly) and nothing calls it.
inline double minSampleRateFor(double freq)
{
    constexpr double kSafeFractionOfNyquist = 0.75;
    return freq / (0.5 * kSafeFractionOfNyquist);
}

inline BiquadCoeffs makeHighShelf(double sampleRate, double freq, double Q, double gainDB)
{
    double A = std::pow(10.0, gainDB / 40.0);
    double w0 = 2.0 * juce::MathConstants<double>::pi * freq / sampleRate;
    double alpha = std::sin(w0) / (2.0 * Q);
    double cosw0 = std::cos(w0);
    double sqrtA = std::sqrt(A);
    double a0 = (A + 1.0) - (A - 1.0) * cosw0 + 2.0 * sqrtA * alpha;
    return {
        (A * ((A + 1.0) + (A - 1.0) * cosw0 + 2.0 * sqrtA * alpha)) / a0,
        (-2.0 * A * ((A - 1.0) + (A + 1.0) * cosw0)) / a0,
        (A * ((A + 1.0) + (A - 1.0) * cosw0 - 2.0 * sqrtA * alpha)) / a0,
        (2.0 * ((A - 1.0) - (A + 1.0) * cosw0)) / a0,
        ((A + 1.0) - (A - 1.0) * cosw0 - 2.0 * sqrtA * alpha) / a0
    };
}

inline BiquadCoeffs makeBaxandallLowShelf(double sampleRate, double freq, double gainDB)
{
    return makeLowShelf(sampleRate, freq, 0.5, gainDB);
}

inline BiquadCoeffs makeBaxandallHighShelf(double sampleRate, double freq, double gainDB)
{
    return makeHighShelf(sampleRate, freq, 0.5, gainDB);
}

inline BiquadCoeffs makeBritBell(double sampleRate, double freq, double Q, double gainDB)
{
    return makePeakEQ(sampleRate, freq, Q * 1.2, gainDB);
}

inline BiquadCoeffs makeBritLowShelf(double sampleRate, double freq, double gainDB)
{
    return makeLowShelf(sampleRate, freq, 0.71, gainDB);
}

inline BiquadCoeffs makeBritHighShelf(double sampleRate, double freq, double gainDB)
{
    return makeHighShelf(sampleRate, freq, 0.71, gainDB);
}

inline BiquadCoeffs makeNTypeProportionalQ(double sampleRate, double freq, double baseQ, double gainDB)
{
    double absGain = std::abs(gainDB);
    double effectiveQ = baseQ + absGain * 0.15;
    effectiveQ = std::clamp(effectiveQ, 0.3, 12.0);
    return makePeakEQ(sampleRate, freq, effectiveQ, gainDB);
}

inline BiquadCoeffs makeFSFHighShelf(double sampleRate, double freq, double gainDB)
{
    return makeHighShelf(sampleRate, freq, 0.85, gainDB);
}

inline BiquadCoeffs makeFSFLowShelf(double sampleRate, double freq, double gainDB)
{
    return makeLowShelf(sampleRate, freq, 0.85, gainDB);
}

inline BiquadCoeffs makeATypeBell(double sampleRate, double freq, double gainDB)
{
    // A-Type fixed-shape bell: the console's Q is not adjustable, so a
    // constant, fairly wide Q is baked into the curve.
    return makePeakEQ(sampleRate, freq, 0.9, gainDB);
}

} // namespace FilterDesign

enum class CutFilterSlope { dB6 = 0, dB12, dB18, dB24 };
enum class LowBandType { Baxandall = 0, Brit, FSF };
enum class HighBandType { Baxandall = 0, Brit, FSF };
enum class MidBandType { N_EQ = 0, Brit, A_Type };
enum class MidSideMode { Stereo = 0, Mid, Side };
enum class BandId { LowCut, HighCut, LowBand, Mid1, Mid2, HighBand };

static constexpr int kMaxBiquadStages = 4;
static constexpr int kMaxChannels = 2;

class CascadedFilter {
public:
    void setOrder(int order)
    {
        activeStages = std::clamp(order, 1, kMaxBiquadStages);
    }

    void setCoeffs(int stage, const BiquadCoeffs& c)
    {
        if (stage >= 0 && stage < kMaxBiquadStages) {
            for (int ch = 0; ch < kMaxChannels; ++ch)
                stages[static_cast<size_t>(ch)][static_cast<size_t>(stage)].setCoeffs(c);
        }
    }

    void reset()
    {
        for (auto& ch : stages)
            for (auto& s : ch)
                s.reset();
    }

    void processBlock(float* data, int numSamples, int channel) noexcept
    {
        for (int s = 0; s < activeStages; ++s)
            stages[static_cast<size_t>(channel)][static_cast<size_t>(s)].processBlock(data, numSamples);
    }

    int getActiveStages() const { return activeStages; }

    BiquadCoeffs getStageCoeffs(int stage) const
    {
        if (stage >= 0 && stage < kMaxBiquadStages)
            return stages[0][static_cast<size_t>(stage)].getCoeffs();
        return {};
    }

private:
    std::array<std::array<BiquadProcessor, kMaxBiquadStages>, kMaxChannels> stages;
    int activeStages = 1;
};

class AnalogEQ {
public:
    void prepare(double sampleRate, int /*samplesPerBlock*/)
    {
        sr = sampleRate;
        lowCut.reset();
        highCut.reset();
        lowBand.reset();
        mid1Band.reset();
        mid2Band.reset();
        highBand.reset();
    }

    void setSampleRate(double sampleRate) noexcept
    {
        sr = sampleRate;
    }

    void setBypass(BandId band, bool bypassed) noexcept
    {
        switch (band) {
            case BandId::LowCut:   lcBypass = bypassed; break;
            case BandId::HighCut:  hcBypass = bypassed; break;
            case BandId::LowBand:  lowBypass = bypassed; break;
            case BandId::Mid1:     mid1Bypass = bypassed; break;
            case BandId::Mid2:     mid2Bypass = bypassed; break;
            case BandId::HighBand: highBypass = bypassed; break;
        }
    }

    bool isBypassed(BandId band) const noexcept
    {
        switch (band) {
            case BandId::LowCut:   return lcBypass;
            case BandId::HighCut:  return hcBypass;
            case BandId::LowBand:  return lowBypass;
            case BandId::Mid1:     return mid1Bypass;
            case BandId::Mid2:     return mid2Bypass;
            case BandId::HighBand: return highBypass;
        }
        return false;
    }

    void updateLowCut(double freq, double Q, CutFilterSlope slope, MidSideMode mode = MidSideMode::Stereo)
    {
        lcMode = mode;
        msActive = usesMidSide();
        double safeFreq = FilterDesign::safeFilterFreq(sr, freq);
        int order = static_cast<int>(slope) + 1;
        lowCut.setOrder(order);
        for (int i = 0; i < order; ++i) {
            double stageQ = Q;
            if (order > 1)
                stageQ = 1.0 / (2.0 * std::sin(juce::MathConstants<double>::pi * (2.0 * i + 1) / (2.0 * order)));
            stageQ = std::max(stageQ * Q / 0.7071, 0.1);
            lowCut.setCoeffs(i, FilterDesign::makeHighPass(sr, safeFreq, stageQ));
        }
    }

    void updateHighCut(double freq, double Q, CutFilterSlope slope, MidSideMode mode = MidSideMode::Stereo)
    {
        hcMode = mode;
        msActive = usesMidSide();
        if (unchanged(hcKey, freq, Q, static_cast<double>(slope), 0.0, 0.0)) return;
        const double safeFreq = FilterDesign::safeFilterFreq(sr, freq);
        const int order = static_cast<int>(slope) + 1;
        highCut.setOrder(order);
        std::array<fit::Spec, kMaxBiquadStages> specs;
        std::array<BiquadCoeffs, kMaxBiquadStages> standard;
        for (int i = 0; i < order; ++i) {
            double stageQ = Q;
            if (order > 1)
                stageQ = 1.0 / (2.0 * std::sin(juce::MathConstants<double>::pi * (2.0 * i + 1) / (2.0 * order)));
            stageQ = std::max(stageQ * Q / 0.7071, 0.1);
            standard[static_cast<size_t>(i)] = FilterDesign::makeLowPass(sr, safeFreq, stageQ);
            specs[static_cast<size_t>(i)] = {fit::Shape::LowPass, freq, stageQ, 0.0};
        }
        // Corners above ~10 % of the sample rate would cramp with the standard design: fit the cascade to the
        // analog curve instead (see BiquadFit.h). Below that the standard design is accurate.
        bool done = false;
        if (freq >= kFitFraction * sr) {
            std::array<fit::Coeffs, kMaxBiquadStages> warm, out;
            const bool haveWarm = hcFitted && hcFittedOrder == order;
            for (int i = 0; i < order && haveWarm; ++i) warm[static_cast<size_t>(i)] = toFit(highCut.getStageCoeffs(i));
            fit::designCascade(specs.data(), order, sr, haveWarm ? warm.data() : nullptr, out.data());
            bool ok = true;
            for (int i = 0; i < order; ++i) ok = ok && fit::isStable(out[static_cast<size_t>(i)]);
            if (ok) {
                for (int i = 0; i < order; ++i) highCut.setCoeffs(i, fromFit(out[static_cast<size_t>(i)]));
                done = true;
            }
        }
        if (!done) for (int i = 0; i < order; ++i) highCut.setCoeffs(i, standard[static_cast<size_t>(i)]);
        hcFitted = done; hcFittedOrder = order;
    }

    void updateLowBand(double freq, double gain, LowBandType type, MidSideMode mode = MidSideMode::Stereo)
    {
        lowMode = mode;
        msActive = usesMidSide();
        lowBand.setOrder(1);
        switch (type) {
            case LowBandType::Baxandall:
                lowBand.setCoeffs(0, FilterDesign::makeBaxandallLowShelf(sr, freq, gain));
                break;
            case LowBandType::Brit:
                lowBand.setCoeffs(0, FilterDesign::makeBritLowShelf(sr, freq, gain));
                break;
            case LowBandType::FSF:
                lowBand.setCoeffs(0, FilterDesign::makeFSFLowShelf(sr, freq, gain));
                break;
        }
    }

    void updateMid1(double freq, double gain, double Q, MidBandType type = MidBandType::N_EQ,
                    MidSideMode mode = MidSideMode::Stereo)
    {
        mid1Mode = mode;
        msActive = usesMidSide();
        if (unchanged(mid1Key, freq, gain, Q, static_cast<double>(type), 0.0)) return;
        mid1Band.setOrder(1);
        mid1Band.setCoeffs(0, designBell(freq, gain, Q, type, mid1Band.getStageCoeffs(0), mid1Fitted));
    }

    void updateMid2(double freq, double gain, double Q, MidBandType type = MidBandType::N_EQ,
                    MidSideMode mode = MidSideMode::Stereo)
    {
        mid2Mode = mode;
        msActive = usesMidSide();
        if (unchanged(mid2Key, freq, gain, Q, static_cast<double>(type), 0.0)) return;
        mid2Band.setOrder(1);
        mid2Band.setCoeffs(0, designBell(freq, gain, Q, type, mid2Band.getStageCoeffs(0), mid2Fitted));
    }

    void updateHighBand(double freq, double gain, HighBandType type, MidSideMode mode = MidSideMode::Stereo)
    {
        highMode = mode;
        msActive = usesMidSide();
        if (unchanged(highKey, freq, gain, static_cast<double>(type), 0.0, 0.0)) return;
        highBand.setOrder(1);
        const double safeFreq = FilterDesign::safeShelfFreq(sr, freq);
        BiquadCoeffs standard;
        double shelfQ = 0.5;
        switch (type) {
            case HighBandType::Baxandall: shelfQ = 0.5;  standard = FilterDesign::makeBaxandallHighShelf(sr, safeFreq, gain); break;
            case HighBandType::Brit:      shelfQ = 0.71; standard = FilterDesign::makeBritHighShelf(sr, safeFreq, gain); break;
            case HighBandType::FSF:       shelfQ = 0.85; standard = FilterDesign::makeFSFHighShelf(sr, safeFreq, gain); break;
        }
        highBand.setCoeffs(0, fitted({fit::Shape::HighShelf, freq, shelfQ, gain}, standard, highBand.getStageCoeffs(0), highFitted));
    }

    void processBlock(juce::AudioBuffer<float>& buffer) noexcept
    {
        int numChannels = buffer.getNumChannels();
        int numSamples = buffer.getNumSamples();

        if (numChannels == 2 && msActive) {
            processStereoMidSide(buffer.getWritePointer(0), buffer.getWritePointer(1), numSamples);
            return;
        }

        for (int ch = 0; ch < numChannels; ++ch)
            applyChain(buffer.getWritePointer(ch), numSamples, ch, true);
    }

    void processBlock(float* const* channelData, int numChannels, int numSamples) noexcept
    {
        if (numChannels == 2 && msActive) {
            processStereoMidSide(channelData[0], channelData[1], numSamples);
            return;
        }

        for (int ch = 0; ch < numChannels; ++ch)
            applyChain(channelData[ch], numSamples, ch, true);
    }

    std::complex<double> getMagnitudeResponse(double freq) const
    {
        double normFreq = freq / sr;
        std::complex<double> total(1.0, 0.0);

        auto accum = [&](const CascadedFilter& f) {
            for (int s = 0; s < f.getActiveStages(); ++s)
                total *= f.getStageCoeffs(s).response(normFreq);
        };

        if (!lcBypass)   accum(lowCut);
        if (!hcBypass)   accum(highCut);
        if (!lowBypass)  accum(lowBand);
        if (!mid1Bypass) accum(mid1Band);
        if (!mid2Bypass) accum(mid2Band);
        if (!highBypass) accum(highBand);

        return total;
    }

    double getMagnitudeDB(double freq) const
    {
        double mag = std::abs(getMagnitudeResponse(freq));
        return 20.0 * std::log10(std::max(mag, 1e-10));
    }

private:
    bool usesMidSide() const noexcept
    {
        return lcMode != MidSideMode::Stereo || hcMode != MidSideMode::Stereo ||
               lowMode != MidSideMode::Stereo || mid1Mode != MidSideMode::Stereo ||
               mid2Mode != MidSideMode::Stereo || highMode != MidSideMode::Stereo;
    }

    // Serial band chain. isMidPath == true  -> mid path  (bands set to Side are skipped)
    //                     isMidPath == false -> side path (bands set to Mid  are skipped)
    void applyChain(float* data, int numSamples, int pathChannel, bool isMidPath) noexcept
    {
        bool lcOn = !lcBypass && (isMidPath ? lcMode != MidSideMode::Side : lcMode != MidSideMode::Mid);
        if (lcOn) lowCut.processBlock(data, numSamples, pathChannel);

        bool lowOn = !lowBypass && (isMidPath ? lowMode != MidSideMode::Side : lowMode != MidSideMode::Mid);
        if (lowOn) lowBand.processBlock(data, numSamples, pathChannel);

        bool m1On = !mid1Bypass && (isMidPath ? mid1Mode != MidSideMode::Side : mid1Mode != MidSideMode::Mid);
        if (m1On) mid1Band.processBlock(data, numSamples, pathChannel);

        bool m2On = !mid2Bypass && (isMidPath ? mid2Mode != MidSideMode::Side : mid2Mode != MidSideMode::Mid);
        if (m2On) mid2Band.processBlock(data, numSamples, pathChannel);

        bool highOn = !highBypass && (isMidPath ? highMode != MidSideMode::Side : highMode != MidSideMode::Mid);
        if (highOn) highBand.processBlock(data, numSamples, pathChannel);

        bool hcOn = !hcBypass && (isMidPath ? hcMode != MidSideMode::Side : hcMode != MidSideMode::Mid);
        if (hcOn) highCut.processBlock(data, numSamples, pathChannel);
    }

    void ensureScratch(int numSamples)
    {
        if (msScratchCap < numSamples) {
            msScratch.setSize(2, numSamples, false, false, true);
            msScratchCap = numSamples;
        }
    }

    void processStereoMidSide(float* left, float* right, int numSamples) noexcept
    {
        ensureScratch(numSamples);

        float* mid = msScratch.getWritePointer(0);
        float* side = msScratch.getWritePointer(1);

        for (int i = 0; i < numSamples; ++i) {
            float m = (left[i] + right[i]) * 0.5f;
            float s = (left[i] - right[i]) * 0.5f;
            mid[i] = m;
            side[i] = s;
        }

        applyChain(mid, numSamples, 0, true);    // mid path  (Stereo + Mid bands)
        applyChain(side, numSamples, 1, false);  // side path (Stereo + Side bands)

        for (int i = 0; i < numSamples; ++i) {
            left[i] = mid[i] + side[i];
            right[i] = mid[i] - side[i];
        }
    }

    // Fit used from this fraction of the sample rate upward (cramping is negligible below it).
    static constexpr double kFitFraction = 0.1;

    static fit::Coeffs toFit(const BiquadCoeffs& c) { return {c.b0, c.b1, c.b2, c.a1, c.a2}; }
    static BiquadCoeffs fromFit(const fit::Coeffs& c) { return {c.b0, c.b1, c.b2, c.a1, c.a2}; }

    // A band is only redesigned when one of its inputs (or the sample rate) actually changed.
    struct Key { double v[5] = {}; double sr = 0.0; bool valid = false; };
    bool unchanged(Key& k, double a, double b, double c, double d, double e) const
    {
        if (k.valid && k.sr == sr && k.v[0] == a && k.v[1] == b && k.v[2] == c && k.v[3] == d && k.v[4] == e) return true;
        k.v[0] = a; k.v[1] = b; k.v[2] = c; k.v[3] = d; k.v[4] = e; k.sr = sr; k.valid = true;
        return false;
    }

    // Standard design below the fit threshold, magnitude fit above it. `wasFitted` says whether the previous
    // coefficients came from the fit (then they are the warm start, so sweeps stay continuous).
    BiquadCoeffs fitted(const fit::Spec& spec, const BiquadCoeffs& standard, const BiquadCoeffs& previous, bool& wasFitted) const
    {
        if (spec.f0 < kFitFraction * sr) { wasFitted = false; return standard; }
        const fit::Coeffs warm = toFit(previous);
        const fit::Result r = fit::design(spec, sr, wasFitted ? &warm : nullptr);
        wasFitted = r.fitted;
        return r.fitted ? fromFit(r.c) : standard;
    }

    BiquadCoeffs designBell(double freq, double gain, double Q, MidBandType type, const BiquadCoeffs& previous, bool& wasFitted) const
    {
        BiquadCoeffs standard;
        double bellQ = Q;
        switch (type) {
            case MidBandType::Brit:   bellQ = Q * 1.2; standard = FilterDesign::makeBritBell(sr, freq, Q, gain); break;
            case MidBandType::A_Type: bellQ = 0.9;     standard = FilterDesign::makeATypeBell(sr, freq, gain); break;
            case MidBandType::N_EQ:
            default:
                bellQ = std::clamp(Q + std::abs(gain) * 0.15, 0.3, 12.0);
                standard = FilterDesign::makeNTypeProportionalQ(sr, freq, Q, gain);
                break;
        }
        return fitted({fit::Shape::Peak, freq, bellQ, gain}, standard, previous, wasFitted);
    }

    Key hcKey, mid1Key, mid2Key, highKey;
    bool hcFitted = false, mid1Fitted = false, mid2Fitted = false, highFitted = false;
    int hcFittedOrder = 0;

    double sr = 44100.0;
    CascadedFilter lowCut, highCut, lowBand, mid1Band, mid2Band, highBand;

    MidSideMode lcMode = MidSideMode::Stereo;
    MidSideMode hcMode = MidSideMode::Stereo;
    MidSideMode lowMode = MidSideMode::Stereo;
    MidSideMode mid1Mode = MidSideMode::Stereo;
    MidSideMode mid2Mode = MidSideMode::Stereo;
    MidSideMode highMode = MidSideMode::Stereo;
    bool msActive = false;

    bool lcBypass = false;
    bool hcBypass = false;
    bool lowBypass = false;
    bool mid1Bypass = false;
    bool mid2Bypass = false;
    bool highBypass = false;

    juce::AudioBuffer<float> msScratch;
    int msScratchCap = 0;
};

} // namespace dsp
