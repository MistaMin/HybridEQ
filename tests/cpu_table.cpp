// Per-configuration CPU table for the preamp circuit path.
// Measures the real HybridEQProcessor across oversampling x sample rate x block size,
// with the Preamp Circuit on and off, so solver work can be attributed and compared.
#include "PluginProcessor.h"
#include <chrono>
#include <cstdio>
#include <vector>

namespace {

struct Row { double sr; int bs; int osIndex; bool circuit; double cpu; };

double measure(double sr, int bs, int osIndex, bool circuit, double seconds = 4.0)
{
    HybridEQProcessor p;
    p.setPlayConfigDetails(2, 2, sr, bs);
    p.prepareToPlay(sr, bs);
    if (auto* prm = p.apvts.getParameter("oversampleMode"))
        prm->setValueNotifyingHost(prm->convertTo0to1(static_cast<float>(osIndex)));
    if (auto* prm = p.apvts.getParameter("preampCircuit"))
        prm->setValueNotifyingHost(circuit ? 1.0f : 0.0f);
    if (auto* prm = p.apvts.getParameter("preampType"))
        prm->setValueNotifyingHost(prm->convertTo0to1(1.0f));   // N-Type (the netlist circuit)
    p.prepareToPlay(sr, bs);   // settle latency/oversampling the way a host would

    juce::AudioBuffer<float> buf(2, bs);
    juce::MidiBuffer midi;
    juce::Random rng(7);
    const int blocks = static_cast<int>(seconds * sr / bs);
    const auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < blocks; ++i) {
        for (int c = 0; c < 2; ++c)
            for (int s = 0; s < bs; ++s)
                buf.setSample(c, s, 0.5f * (rng.nextFloat() * 2.0f - 1.0f));
        p.processBlock(buf, midi);
    }
    const double wall = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    return 100.0 * wall / seconds;   // % of one core
}

} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI init;

    const double rates[] = {44100.0, 48000.0, 96000.0, 192000.0};
    const int blocks[] = {64, 128, 512, 1024};
    const int osIdx[] = {0, 1, 2};              // 1x, 4x, 8x
    const char* osName[] = {"1x", "4x", "8x"};

    std::printf("HybridEQ preamp circuit CPU table (%% of one core, stereo, N-Type)\n");
    std::printf("%-9s %-6s %-4s %-9s %-9s %s\n", "rate", "block", "os", "circuit", "cpu%", "xRT");
    std::printf("%s\n", std::string(60, '-').c_str());

    for (double sr : rates)
        for (int bs : blocks)
            for (int o = 0; o < 3; ++o)
                for (bool circuit : {false, true}) {
                    const double cpu = measure(sr, bs, osIdx[o], circuit);
                    std::printf("%-9.0f %-6d %-4s %-9s %8.1f%% %6.1fx\n",
                                sr, bs, osName[o], circuit ? "ON" : "off", cpu, 100.0 / cpu);
                    std::fflush(stdout);
                }
    return 0;
}
