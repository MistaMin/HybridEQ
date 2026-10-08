// End-to-end check of the cramping-free EQ on the real processor.
// The same impulse goes through the plug-in at 1x (the EQ biquads are fitted to the analog curve, see
// Source/DSP/BiquadFit.h) and at 8x oversampling (the standard design at 8x the rate, which is how the plug-in
// used to avoid cramping). The two magnitude responses are compared (pre-echo and phase are ignored).
// It also checks the reported latency: at 1x with Circuit off it must be zero, whatever the High Cut / shelf setting.
#include "PluginProcessor.h"
#include <cstdio>

static int failures = 0;
static void check(bool ok, const juce::String& what) { std::printf("  %s  %s\n", ok ? "PASS" : "FAIL", what.toRawUTF8()); if (!ok) ++failures; }

static void setParam(HybridEQProcessor& p, const char* id, float plain)
{
    if (auto* prm = p.apvts.getParameter(id)) prm->setValueNotifyingHost(prm->convertTo0to1(plain));
}

struct Setup { const char* name; std::vector<std::pair<const char*, float>> params; };

// impulse response of the plug-in (left channel), `n` samples
static std::vector<double> impulseResponse(double sr, int oversampleIndex, const Setup& s, int* latency)
{
    HybridEQProcessor p;
    // Settings first, then prepareToPlay: the oversampling factor and the reported latency are settled there,
    // exactly as when a host restores a saved session.
    setParam(p, "preampBypass", 1.0f);
    setParam(p, "lcBypass", 1.0f);
    setParam(p, "preampCircuit", 0.0f);
    for (const char* id : {"harmonic2nd", "harmonic3rd", "harmonic4th", "harmonic5th"}) setParam(p, id, 0.0f);
    setParam(p, "oversampleMode", float(oversampleIndex));
    for (auto& kv : s.params) setParam(p, kv.first, kv.second);
    p.prepareToPlay(sr, 512);
    juce::AudioBuffer<float> buf(2, 512);
    juce::MidiBuffer midi;
    for (int i = 0; i < 8; ++i) { buf.clear(); p.processBlock(buf, midi); }          // let every setting take effect
    if (latency) *latency = p.getLatencySamples();
    std::vector<double> ir;
    for (int blk = 0; blk < 24; ++blk) {
        buf.clear();
        if (blk == 0) { buf.setSample(0, 0, 1.0f); buf.setSample(1, 0, 1.0f); }
        p.processBlock(buf, midi);
        for (int i = 0; i < 512; ++i) ir.push_back(buf.getSample(0, i));
    }
    return ir;
}

static double magAt(const std::vector<double>& ir, double f, double sr)
{
    std::complex<double> acc = 0.0;
    for (size_t n = 0; n < ir.size(); ++n) acc += ir[n] * std::exp(std::complex<double>(0.0, -2.0 * juce::MathConstants<double>::pi * f * double(n) / sr));
    return std::abs(acc);
}

int main()
{
    // choice indices: slopes -6/-12/-18/-24 dB/oct = 0..3 ; types: 0 = first entry
    const std::vector<Setup> setups = {
        {"High Cut 30 kHz, 12 dB/oct", {{"hcFreq", 30000.0f}, {"hcSlope", 1.0f}, {"hcQ", 0.71f}}},
        {"High Cut 24 kHz, 24 dB/oct", {{"hcFreq", 24000.0f}, {"hcSlope", 3.0f}, {"hcQ", 0.71f}}},
        {"High Cut 16 kHz, 18 dB/oct", {{"hcFreq", 16000.0f}, {"hcSlope", 2.0f}, {"hcQ", 0.71f}}},
        {"High shelf 20 kHz +12 dB, Brit", {{"highFreq", 20000.0f}, {"highGain", 12.0f}, {"highType", 1.0f}}},
        {"High shelf 30 kHz -12 dB, FSF", {{"highFreq", 30000.0f}, {"highGain", -12.0f}, {"highType", 2.0f}}},
        {"Mid 1 bell 6 kHz +12 dB, Q 4, Brit", {{"mid1Freq", 6000.0f}, {"mid1Gain", 12.0f}, {"mid1Q", 4.0f}, {"mid1Type", 1.0f}}},
        {"Mid 2 bell 5 kHz -12 dB, Q 1, N-EQ", {{"mid2Freq", 5000.0f}, {"mid2Gain", -12.0f}, {"mid2Q", 1.0f}, {"mid2Type", 0.0f}}},
    };
    double worstOverall = 0.0;
    for (double sr : {44100.0, 48000.0}) {
        std::printf("sample rate %.0f Hz: base-rate (fitted) versus 8x oversampled, worst difference 20 Hz - 18 kHz\n", sr);
        for (const Setup& s : setups) {
            int lat1 = -1, lat8 = -1;
            const auto a = impulseResponse(sr, 0, s, &lat1), r = impulseResponse(sr, 2, s, &lat8);
            double worst = 0.0, worstF = 0.0;
            for (int k = 0; k < 160; ++k) {
                const double f = 20.0 * std::pow(18000.0 / 20.0, k / 159.0);
                const double e = std::abs(20.0 * std::log10((magAt(a, f, sr) + 1e-3) / (magAt(r, f, sr) + 1e-3)));
                if (e > worst) { worst = e; worstF = f; }
            }
            worstOverall = std::max(worstOverall, worst);
            std::printf("  %-38s %5.2f dB (at %5.0f Hz)   latency 1x: %d samples, 8x: %d samples\n", s.name, worst, worstF, lat1, lat8);
            check(lat1 == 0, juce::String(s.name) + ": no latency at 1x");
        }
    }
    check(worstOverall < 1.5, "the fitted 1x response stays within 1.5 dB of the 8x-oversampled plug-in in every setting");
    std::printf("\n%s (%d failed)\n", failures ? "EQ INTEGRATION TEST FAILED" : "EQ integration test passed", failures);
    return failures ? 1 : 0;
}
