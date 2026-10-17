// The three preamp modes on the real processor: Circuit off (static models), Circuit on with HQ off (the reduced
// Circuit model) and Circuit on with HQ on (the full solver). For every preamp type, oversampling choice and a few
// sample rates: finite and bounded output, silence stays silent, the effective oversampling factor and reported
// latency follow the mode, switching modes while audio runs is safe, and a session saved before the HQ parameter
// existed restores as HQ.
#include "PluginProcessor.h"
#include <cstdio>

static int failures = 0;
static void check(bool ok, const juce::String& what) { std::printf("  %s  %s\n", ok ? "PASS" : "FAIL", what.toRawUTF8()); if (!ok) ++failures; }

static void setParam(HybridEQProcessor& p, const char* id, float plain)
{
    if (auto* prm = p.apvts.getParameter(id)) prm->setValueNotifyingHost(prm->convertTo0to1(plain));
}

enum class Mode { Off, Circuit, HQ };
static const char* modeName[] = {"Circuit off", "Circuit", "HQ"};
static const char* typeName[] = {"Off", "Brit", "N-Type", "FSF", "A-Type"};

static void configure(HybridEQProcessor& p, int type, Mode m, int osIndex)
{
    setParam(p, "preampBypass", 0.0f);
    setParam(p, "preampType", float(type));
    setParam(p, "preampCircuit", m == Mode::Off ? 0.0f : 1.0f);
    setParam(p, "preampHQ", m == Mode::HQ ? 1.0f : 0.0f);
    setParam(p, "oversampleMode", float(osIndex));
    for (const char* id : {"lcBypass", "hcBypass", "lowBypass", "mid1Bypass", "mid2Bypass", "highBypass"}) setParam(p, id, 1.0f);
    for (const char* id : {"harmonic2nd", "harmonic3rd", "harmonic4th", "harmonic5th"}) setParam(p, id, 0.0f);
}

struct Result { bool finite = true; float peak = 0; double rms = 0; };

static Result run(HybridEQProcessor& p, double sr, int block, double seconds, double amp, double hz)
{
    Result r; juce::AudioBuffer<float> buf(2, block); juce::MidiBuffer midi;
    const int total = int(sr * seconds); double sum = 0; long n = 0; long idx = 0;
    for (int done = 0; done < total; done += block) {
        for (int i = 0; i < block; ++i, ++idx) { const float v = float(amp * std::sin(2 * juce::MathConstants<double>::pi * hz * double(idx) / sr)); buf.setSample(0, i, v); buf.setSample(1, i, v); }
        p.processBlock(buf, midi);
        for (int i = 0; i < block; ++i) {
            const float v = buf.getSample(0, i);
            if (!std::isfinite(v)) r.finite = false;
            r.peak = std::max(r.peak, std::abs(v));
            if (done > total / 2) { sum += double(v) * v; ++n; }
        }
    }
    r.rms = n ? std::sqrt(sum / double(n)) : 0;
    return r;
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    const int types[] = {1, 2, 3, 4};
    std::printf("1) every type x mode x oversampling x rate: finite, bounded, silence stays silent\n");
    for (double sr : {44100.0, 48000.0, 96000.0})
        for (int type : types)
            for (Mode m : {Mode::Off, Mode::Circuit, Mode::HQ})
                for (int os = 0; os < 4; ++os) {
                    if (m == Mode::HQ && sr > 50000.0 && os == 3) continue;                 // 8x of 96 kHz is a very slow full solve
                    HybridEQProcessor p; configure(p, type, m, os); p.prepareToPlay(sr, 256);
                    const auto quiet = run(p, sr, 256, 0.15, 0.0, 1000.0);
                    const auto loud = run(p, sr, 256, 0.30, 0.5, 1000.0);
                    const int factor = os == 0 ? 1 : os == 1 ? 2 : os == 2 ? 4 : 8;
                    const juce::String tag = juce::String(typeName[type]) + ", " + modeName[int(m)] + ", " + juce::String(factor) + "x, " + juce::String(sr, 0) + " Hz";
                    check(quiet.finite && loud.finite && loud.peak < 20.0f && quiet.peak < 1.0e-3f && loud.rms > 0.01, tag + ": peak " + juce::String(loud.peak, 2) + ", silence " + juce::String(quiet.peak, 6));
                }

    std::printf("2) effective oversampling and latency follow the mode (Oversample = 1x)\n");
    for (Mode m : {Mode::Off, Mode::Circuit, Mode::HQ}) {
        HybridEQProcessor p; configure(p, 1, m, 0); p.prepareToPlay(48000.0, 256);
        const int factor = p.getEffectiveOversampleFactor(), latency = p.getLatencySamples();
        std::printf("     %-11s effective %dx, latency %d samples\n", modeName[int(m)], factor, latency);
        if (m == Mode::HQ) check(factor == 2 && latency > 0, "HQ raises the factor to 2x");
        else check(factor == 1 && latency == 0, juce::String(modeName[int(m)]) + " stays at 1x with no latency");
    }
    {
        HybridEQProcessor p; configure(p, 1, Mode::Circuit, 0); setParam(p, "preampBypass", 1.0f); p.prepareToPlay(48000.0, 256);
        check(p.getEffectiveOversampleFactor() == 1, "Circuit with the preamp bypassed adds no oversampling");
    }

    std::printf("3) switching modes, types and rates while audio runs\n");
    {
        HybridEQProcessor p; configure(p, 1, Mode::HQ, 0); p.prepareToPlay(48000.0, 256);
        juce::AudioBuffer<float> buf(2, 256); juce::MidiBuffer midi; juce::Random rng(5); bool ok = true; float peak = 0; long idx = 0;
        for (int blk = 0; blk < 600; ++blk) {
            if (blk % 3 == 0) { configure(p, types[rng.nextInt(4)], Mode(rng.nextInt(3)), rng.nextInt(4)); }
            for (int i = 0; i < 256; ++i, ++idx) { const float v = float(0.4 * std::sin(0.0654 * double(idx))); buf.setSample(0, i, v); buf.setSample(1, i, v); }
            p.processBlock(buf, midi);
            for (int i = 0; i < 256; ++i) { const float v = buf.getSample(0, i); ok &= std::isfinite(v); peak = std::max(peak, std::abs(v)); }
        }
        check(ok && peak < 20.0f, "600 blocks of random mode/type/oversampling changes: finite, peak " + juce::String(peak, 2));
    }

    std::printf("4) sessions saved before the HQ parameter existed restore as HQ\n");
    {
        HybridEQProcessor src; setParam(src, "preampCircuit", 1.0f);
        juce::MemoryBlock mb; src.getStateInformation(mb);
        std::unique_ptr<juce::XmlElement> xml(juce::AudioProcessor::getXmlFromBinary(mb.getData(), int(mb.getSize())));
        for (auto* child = xml->getFirstChildElement(); child != nullptr;) {          // strip the new parameter, as an old session would lack it
            auto* next = child->getNextElement();
            if (child->getStringAttribute("id") == "preampHQ") xml->removeChildElement(child, true);
            child = next;
        }
        juce::MemoryBlock old; juce::AudioProcessor::copyXmlToBinary(*xml, old);
        HybridEQProcessor dst; setParam(dst, "preampHQ", 0.0f);                       // start from the opposite setting
        dst.setStateInformation(old.getData(), int(old.getSize()));
        const bool hq = dst.apvts.getRawParameterValue("preampHQ")->load() > 0.5f;
        const bool circuit = dst.apvts.getRawParameterValue("preampCircuit")->load() > 0.5f;
        check(circuit && hq, "old session with Circuit on loads as Circuit on + HQ");
    }
    std::printf("\n%s (%d failed)\n", failures ? "ENGINE MODE TEST FAILED" : "engine mode test passed", failures);
    return failures ? 1 : 0;
}
