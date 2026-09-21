#pragma once
#include <juce_core/juce_core.h>
#include <cmath>

namespace ValueFormat {

// 220 -> "220"   2200 -> "2.2k"   16800 -> "16.8k"  (never shows "Hz", implied by context)
inline juce::String frequency(float hz)
{
    if (hz < 1000.0f)
        return juce::String(juce::roundToInt(hz));

    float k = hz / 1000.0f;
    return juce::String(k, 1) + "k";
}

inline juce::String gainDB(float db)
{
    juce::String sign = db > 0.01f ? "+" : "";
    return sign + juce::String(db, 1);
}

inline juce::String qFactor(float q)
{
    return juce::String(q, 2);
}

} // namespace ValueFormat
