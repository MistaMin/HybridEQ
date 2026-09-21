#pragma once
#include <cmath>
#include <juce_audio_basics/juce_audio_basics.h>

namespace dsp {

class HarmonicSaturationEngine {
public:
    void setHarmonicLevels(float h2, float h3, float h4, float h5)
    {
        harm2.store(h2, std::memory_order_relaxed);
        harm3.store(h3, std::memory_order_relaxed);
        harm4.store(h4, std::memory_order_relaxed);
        harm5.store(h5, std::memory_order_relaxed);
    }

    void process(juce::AudioBuffer<float>& buffer) noexcept
    {
        const float h2 = harm2.load(std::memory_order_relaxed);
        const float h3 = harm3.load(std::memory_order_relaxed);
        const float h4 = harm4.load(std::memory_order_relaxed);
        const float h5 = harm5.load(std::memory_order_relaxed);

        if (h2 == 0.0f && h3 == 0.0f && h4 == 0.0f && h5 == 0.0f)
            return;

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch) {
            auto* data = buffer.getWritePointer(ch);
            for (int i = 0; i < buffer.getNumSamples(); ++i) {
                data[i] = processSample(data[i], h2, h3, h4, h5);
            }
        }
    }

    void processBlock(float* data, int numSamples) noexcept
    {
        const float h2 = harm2.load(std::memory_order_relaxed);
        const float h3 = harm3.load(std::memory_order_relaxed);
        const float h4 = harm4.load(std::memory_order_relaxed);
        const float h5 = harm5.load(std::memory_order_relaxed);

        if (h2 == 0.0f && h3 == 0.0f && h4 == 0.0f && h5 == 0.0f)
            return;

        for (int i = 0; i < numSamples; ++i)
            data[i] = processSample(data[i], h2, h3, h4, h5);
    }

private:
    static float processSample(float x, float h2, float h3, float h4, float h5) noexcept
    {
        // NOTE: raw Chebyshev polynomials (T2, T4, ...) are nonzero at x = 0
        // (T2(0) = -1, T4(0) = 1), which injects a DC offset into silence and
        // is heard as a thump/noise whenever the even-order sliders move.
        // These zero-at-origin variants generate the same harmonic content
        // but vanish identically when x = 0, so silence stays silent.
        float xc = std::clamp(x, -1.0f, 1.0f);
        float ax = std::abs(xc);

        float t2 = xc * ax;                       // 0 at x = 0, even-order character
        float t3 = xc * xc * xc;                  // odd, 0 at x = 0
        float t4 = xc * xc * xc * ax;              // 0 at x = 0, even-order character
        float t5 = xc * xc * xc * xc * xc;        // odd, 0 at x = 0

        float result = x + 6.0f * (h2 * t2 + h3 * t3 + h4 * t4 + h5 * t5);

        return std::tanh(result);
    }

    std::atomic<float> harm2{0.0f};
    std::atomic<float> harm3{0.0f};
    std::atomic<float> harm4{0.0f};
    std::atomic<float> harm5{0.0f};
};

} // namespace dsp
