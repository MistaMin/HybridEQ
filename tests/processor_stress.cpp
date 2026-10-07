// Headless stress test of the real processor: many sample rates, block sizes, oversampling modes,
// signals and parameter automation, plus a multi-thread run like a host UI would produce.
// Exit code 0 = every check passed.
#include "PluginProcessor.h"
#include <chrono>
#include <cstdio>
#include <thread>

static int failures = 0, checks = 0;
static void check(bool ok, const juce::String& what) { ++checks; if (!ok) { ++failures; std::printf("FAIL %s\n", what.toRawUTF8()); } }

enum class Sig { Silence, Sine, Noise, Dc, Impulses, Square, Tiny, Burst };
static const char* sigName[] = {"silence", "sine -18 dBFS", "noise -6 dBFS", "DC 0.1", "impulses", "full-scale square", "denormal-level noise", "bursts"};

struct Gen {
    juce::Random rng{42}; double ph = 0; long n = 0;
    void fill(juce::AudioBuffer<float>& b, Sig s, double sr) {
        for (int i = 0; i < b.getNumSamples(); ++i, ++n) {
            float v = 0;
            switch (s) {
                case Sig::Silence: break;
                case Sig::Sine: v = 0.126f * std::sin(float(ph)); ph += juce::MathConstants<double>::twoPi * 1000.0 / sr; break;
                case Sig::Noise: v = 0.5f * (rng.nextFloat() * 2 - 1); break;
                case Sig::Dc: v = 0.1f; break;
                case Sig::Impulses: v = (n % 4800 == 0) ? 1.0f : 0.0f; break;
                case Sig::Square: v = ((n / 48) % 2) ? 1.0f : -1.0f; break;
                case Sig::Tiny: v = 1.0e-30f * (rng.nextFloat() * 2 - 1); break;
                case Sig::Burst: v = ((n / 24000) % 2) ? 0.8f * (rng.nextFloat() * 2 - 1) : 0.0f; break;
            }
            for (int c = 0; c < b.getNumChannels(); ++c) b.setSample(c, i, c == 0 ? v : v * 0.7f);
        }
    }
};

static bool finiteAndBounded(const juce::AudioBuffer<float>& b, float& maxAbs) {
    for (int c = 0; c < b.getNumChannels(); ++c) for (int i = 0; i < b.getNumSamples(); ++i) { const float v = b.getSample(c, i); if (!std::isfinite(v)) return false; maxAbs = std::max(maxAbs, std::fabs(v)); }
    return true;
}
// Parameters that make the plugin change its own oversampling factor (and so its latency) while running.
// None of them may do so any more: the effective factor is settled in prepareToPlay and the reported
// latency never moves from the audio thread. Kept as the list the test asserts must stay stable.
static bool affectsLatency(const juce::String& id) { return id == "oversampleMode" || id == "hcFreq" || id == "highFreq" || id == "preampCircuit"; }
// Sets random values the way a host would: toggles get 0 or 1, choices get a valid index, floats anything.
static void randomiseParameters(HybridEQProcessor& p, juce::Random& rng, int howMany, bool leaveLatencyAlone = false) {
    auto& params = p.getParameters();
    for (int k = 0; k < howMany; ++k) {
        auto* prm = params[rng.nextInt(params.size())];
        if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*>(prm)) if (leaveLatencyAlone && affectsLatency(withId->paramID)) continue;
        if (dynamic_cast<juce::AudioParameterBool*>(prm)) prm->setValueNotifyingHost(rng.nextBool() ? 1.0f : 0.0f);
        else if (auto* c = dynamic_cast<juce::AudioParameterChoice*>(prm)) prm->setValueNotifyingHost(c->convertTo0to1(float(rng.nextInt(c->choices.size()))));
        else prm->setValueNotifyingHost(rng.nextFloat());
    }
}
static void setAll(HybridEQProcessor& p, float normalised) { for (auto* prm : p.getParameters()) prm->setValueNotifyingHost(normalised); }

int main(int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI init;
    const bool quick = argc > 1 && juce::String(argv[1]) == "--quick";      // fewer combinations, for sanitizer builds
    std::printf("HybridEQ processor stress test\n");

    // 1) rates x block sizes x oversampling x signals, with parameter automation on every block
    {
        const std::vector<double> rates = quick ? std::vector<double>{48000.0, 96000.0} : std::vector<double>{44100.0, 48000.0, 96000.0, 192000.0};
        const std::vector<int> blocks = quick ? std::vector<int>{1, 64, 512, 8192} : std::vector<int>{1, 16, 64, 127, 512, 2048, 8192};
        float worst = 0; int combos = 0;
        for (double sr : rates) for (int bs : blocks) {
            HybridEQProcessor p; p.setPlayConfigDetails(2, 2, sr, bs); p.prepareToPlay(sr, bs);
            if (auto* os = p.getParameters()[0]) (void)os;
            juce::AudioBuffer<float> buf(2, bs); juce::MidiBuffer midi; Gen g; juce::Random rng(int(sr) + bs);
            for (int mode = 0; mode < 3; ++mode) {
                if (auto* prm = p.apvts.getParameter("oversampleMode")) prm->setValueNotifyingHost(prm->convertTo0to1(float(mode)));
                p.prepareToPlay(sr, bs);
                for (int s = 0; s < 8; ++s) {
                    const int latency = p.getLatencySamples();
                    const int total = int(sr * (quick ? 0.05 : 0.25)); bool ok = true; float maxAbs = 0;
                    for (int done = 0; done < total; done += bs) {
                        g.fill(buf, Sig(s), sr);
                        if (bs >= 16 || done % 16 == 0) randomiseParameters(p, rng, 3, true);
                        p.processBlock(buf, midi); ok &= finiteAndBounded(buf, maxAbs);
                    }
                    worst = std::max(worst, maxAbs);
                    check(ok, juce::String(sr, 0) + " Hz, block " + juce::String(bs) + ", oversample " + juce::String(mode) + ", " + sigName[s] + ": output stays finite");
                    check(p.getLatencySamples() == latency, juce::String(sr, 0) + " Hz, block " + juce::String(bs) + ", oversample " + juce::String(mode) + ", " + sigName[s] + ": latency stays constant (other parameters automating)");
                    check(maxAbs < 1000.0f, juce::String(sr, 0) + " Hz, block " + juce::String(bs) + ", " + sigName[s] + ": output stays bounded (max " + juce::String(maxAbs, 2) + ")");
                    ++combos;
                }
            }
        }
        std::printf("1) %d rate/block/oversampling/signal combinations with live automation, worst peak %.2f\n", combos, worst);
    }

    // 1b) The reported latency must not move from the audio thread. Hosts (and AAX validation) require
    // it to be settled in prepareToPlay; a change mid-stream forces the host to re-align and clicks.
    {
        HybridEQProcessor p; p.setPlayConfigDetails(2, 2, 48000.0, 512); p.prepareToPlay(48000.0, 512);
        juce::AudioBuffer<float> buf(2, 512); juce::MidiBuffer midi; Gen g; juce::Random rng(77); int changes = 0, last = p.getLatencySamples(), lo = last, hi = last;
        for (int i = 0; i < 400; ++i) { g.fill(buf, Sig::Noise, 48000.0); randomiseParameters(p, rng, 4); p.processBlock(buf, midi);
            const int now = p.getLatencySamples(); if (now != last) { ++changes; last = now; lo = std::min(lo, now); hi = std::max(hi, now); } }
        std::printf("1b) with oversampling / High Cut / High shelf / Circuit automated, reported latency changed %d times in 400 blocks (range %d..%d samples)\n", changes, lo, hi);
        check(changes == 0, "reported latency never changes from the audio thread (" + juce::String(changes) + " changes)");
    }

    // 2) extreme settings: every parameter at 0, at 1, and at the middle
    {
        HybridEQProcessor p; p.setPlayConfigDetails(2, 2, 48000.0, 512); p.prepareToPlay(48000.0, 512);
        juce::AudioBuffer<float> buf(2, 512); juce::MidiBuffer midi; Gen g; float worst = 0;
        for (float v : {0.0f, 1.0f, 0.5f, 1.0f, 0.0f}) for (int s = 1; s < 8; ++s) {
            setAll(p, v); bool ok = true; float m = 0;
            for (int i = 0; i < 60; ++i) { g.fill(buf, Sig(s), 48000.0); p.processBlock(buf, midi); ok &= finiteAndBounded(buf, m); }
            worst = std::max(worst, m); check(ok && m < 1000.0f, "all parameters at " + juce::String(v) + ", " + sigName[s] + ": finite and bounded (max " + juce::String(m, 1) + ")");
        }
        std::printf("2) extreme parameter sets, worst peak %.1f\n", worst);
    }

    // 3) state: save, change everything, restore, compare
    {
        HybridEQProcessor a, b; a.setPlayConfigDetails(2, 2, 48000.0, 512); a.prepareToPlay(48000.0, 512); b.setPlayConfigDetails(2, 2, 48000.0, 512); b.prepareToPlay(48000.0, 512);
        juce::Random rng(9); randomiseParameters(a, rng, 200); a.apvts.state.setProperty("spectrum", "1,4,multi,1,3,24,0.85,1,ff8fe0c0,ffff3b4a,ffff9a1f,ffffe23a,ff5be85a,ff35e0d0,ff3f8ff0,ffc05cff,3,1", nullptr);
        juce::MemoryBlock mb; a.getStateInformation(mb); b.setStateInformation(mb.getData(), int(mb.getSize()));
        bool same = true; auto& pa = a.getParameters(); auto& pb = b.getParameters();
        for (int i = 0; i < pa.size(); ++i) { const bool eq = std::fabs(pa[i]->getValue() - pb[i]->getValue()) < 1.0e-4f; if (!eq) std::printf("   differs: %s  %.4f -> %.4f\n", pa[i]->getName(64).toRawUTF8(), pa[i]->getValue(), pb[i]->getValue()); same &= eq; }
        check(same, "state save / restore keeps every parameter");
        check(b.apvts.state.getProperty("spectrum").toString().contains("multi"), "state save / restore keeps the editor settings (spectrum)");
        b.setStateInformation("garbage", 7); check(true, "garbage state data is ignored without crashing");
        b.setStateInformation(nullptr, 0); check(true, "empty state data is ignored without crashing");
        std::printf("3) state round trip\n");
    }

    // 4) lifecycle: repeated prepare / release with changing rates, then processing
    {
        HybridEQProcessor p; juce::AudioBuffer<float> buf(2, 512); juce::MidiBuffer midi; Gen g; bool ok = true; float m = 0;
        for (int i = 0; i < 24; ++i) { const double sr = (i % 3 == 0) ? 44100.0 : (i % 3 == 1) ? 96000.0 : 48000.0; p.setPlayConfigDetails(2, 2, sr, 512); p.prepareToPlay(sr, 512);
            for (int k = 0; k < 6; ++k) { g.fill(buf, Sig::Noise, sr); p.processBlock(buf, midi); ok &= finiteAndBounded(buf, m); } if (i % 5 == 0) p.releaseResources(); }
        check(ok, "24 prepare / release cycles with changing sample rates stay finite"); std::printf("4) lifecycle\n");
    }

    // 5) threads: audio thread processing while "UI" threads read the meters / analyser and set parameters
    {
        HybridEQProcessor p; p.setPlayConfigDetails(2, 2, 48000.0, 256); p.prepareToPlay(48000.0, 256);
        std::atomic<bool> go{true}; std::atomic<long> reads{0}; std::atomic<bool> bad{false};
        std::thread ui([&] { std::vector<float> frame(8192); while (go) { auto r = p.getOutputLevel().read(); if (!(r.peak[0] >= 0.0f)) bad = true; auto q = p.getInputLevel().read(); (void)q;
            if (p.getOutputRing().copyLatest(frame.data(), 8192)) for (float v : frame) if (!std::isfinite(v)) bad = true; ++reads; } });
        std::thread host([&] { juce::Random rng(3); while (go) { auto& params = p.getParameters(); params[rng.nextInt(params.size())]->setValueNotifyingHost(rng.nextFloat()); std::this_thread::sleep_for(std::chrono::microseconds(300)); } });
        juce::AudioBuffer<float> buf(2, 256); juce::MidiBuffer midi; Gen g; bool ok = true; float m = 0;
        const auto t0 = std::chrono::steady_clock::now();
        while (std::chrono::steady_clock::now() - t0 < std::chrono::seconds(3)) { g.fill(buf, Sig::Noise, 48000.0); p.processBlock(buf, midi); ok &= finiteAndBounded(buf, m); }
        go = false; ui.join(); host.join();
        check(ok && !bad, "3 s of processing with a UI thread and a host thread running alongside: finite, no torn data");
        check(reads > 100, "UI thread kept reading the taps while audio ran (" + juce::String(long(reads)) + " reads)");
        std::printf("5) threads\n");
    }

    // 6) CPU: real-time factor for the heaviest configuration (8x oversampling, 48 kHz, 512-sample blocks, noise)
    {
        HybridEQProcessor p; p.setPlayConfigDetails(2, 2, 48000.0, 512); p.prepareToPlay(48000.0, 512);
        if (auto* prm = p.apvts.getParameter("oversampleMode")) prm->setValueNotifyingHost(prm->convertTo0to1(2.0f)); p.prepareToPlay(48000.0, 512);
        juce::AudioBuffer<float> buf(2, 512); juce::MidiBuffer midi; Gen g; const int seconds = 10, blocks = seconds * 48000 / 512;
        const auto t0 = std::chrono::steady_clock::now();
        for (int i = 0; i < blocks; ++i) { g.fill(buf, Sig::Noise, 48000.0); p.processBlock(buf, midi); }
        const double wall = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        const double cpu = 100.0 * wall / double(seconds);
        std::printf("6) CPU: %.1f%% of one core per instance at 48 kHz / 512 / 8x oversampling (%.1fx real time)\n", cpu, double(seconds) / wall);
        check(cpu < 50.0, "one instance uses under 50% of a core in the heaviest mode (" + juce::String(cpu, 1) + "%)");
    }

    std::printf("\n%d checks, %d failed\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
