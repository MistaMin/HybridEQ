// Reference renderer for the Rust port: runs the 1.6.16 C++ processor offline so the Rust engine can be
// null-tested against it.
//
//   HybridEQRender gen <kind> <seconds> <sr> <out.f32>
//       kind: chirp | noise | mix | impulse | burst. Writes interleaved stereo float32.
//   HybridEQRender run <in.f32> <out.f32> <sr> <block> [id=plain ...]
//       Renders the input through the real processor after prepareToPlay + reset, with the given parameters
//       (plain units). Writes interleaved stereo float32 and <out.f32>.params with every parameter's stored plain
//       value and the reported latency, so the Rust side can use exactly the same values.
//   HybridEQRender core <in.f32> <out.f32> <sr> <block> [id=plain ...]
//       As run, but the input is taken as samples at the effective (oversampled) rate and goes straight through
//       the preamp, EQ and harmonics, with no oversampling filters and no output gain. Isolates the engine core
//       (including Circuit HQ, which always runs at 2x or more) from JUCE's resampler.
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
// test tool only: reach the processor's engine members
#define private public
#include "PluginProcessor.h"
#undef private
#include <cstdio>
#include <fstream>
#include <vector>

static std::vector<float> readF32(const char* path)
{
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    const auto size = static_cast<size_t>(f.tellg());
    std::vector<float> v(size / sizeof(float));
    f.seekg(0);
    f.read(reinterpret_cast<char*>(v.data()), static_cast<std::streamsize>(v.size() * sizeof(float)));
    return v;
}

static void writeF32(const char* path, const std::vector<float>& v)
{
    std::ofstream f(path, std::ios::binary);
    f.write(reinterpret_cast<const char*>(v.data()), static_cast<std::streamsize>(v.size() * sizeof(float)));
}

static int gen(const juce::String& kind, double seconds, double sr, const char* out)
{
    const int n = int(seconds * sr);
    std::vector<float> v(size_t(n) * 2);
    uint32_t seed = 0x12345678u;
    auto rnd = [&] { seed = seed * 1664525u + 1013904223u; return (double(seed >> 8) / double(1 << 24)) * 2.0 - 1.0; };
    const double pi = juce::MathConstants<double>::pi;
    for (int i = 0; i < n; ++i) {
        const double t = i / sr;
        double l = 0, r = 0;
        if (kind == "chirp") {          // 20 Hz .. 20 kHz log sweep, -6 dBFS, right channel inverted half amplitude
            const double f0 = 20, f1 = 20000, k = std::log(f1 / f0) / seconds;
            const double ph = 2 * pi * f0 * (std::exp(k * t) - 1) / k;
            l = 0.5 * std::sin(ph); r = -0.25 * std::sin(ph);
        } else if (kind == "noise") {   // decorrelated white noise, about -10 dBFS RMS
            l = 0.55 * rnd(); r = 0.55 * rnd();
        } else if (kind == "mix") {     // 110 Hz + 2.3 kHz tones, hot (0.9 peak), different per side
            l = 0.6 * std::sin(2 * pi * 110 * t) + 0.3 * std::sin(2 * pi * 2300 * t);
            r = 0.45 * std::sin(2 * pi * 110 * t + 0.7) + 0.35 * rnd();
        } else if (kind == "impulse") { // one impulse every 0.25 s
            l = (i % int(0.25 * sr)) == 0 ? 0.8 : 0.0; r = l * -0.5;
        } else if (kind == "burst") {   // 1 kHz bursts with silence, level rising
            const double env = (std::fmod(t, 0.5) < 0.25) ? (0.1 + 0.8 * t / seconds) : 0.0;
            l = env * std::sin(2 * pi * 1000 * t); r = env * std::sin(2 * pi * 1500 * t);
        }
        v[size_t(i) * 2] = float(l);
        v[size_t(i) * 2 + 1] = float(r);
    }
    writeF32(out, v);
    return 0;
}

static int run(const char* in, const char* out, double sr, int block, int argc, char** argv, bool core)
{
    HybridEQProcessor p;
    p.setPlayConfigDetails(2, 2, sr, block);
    for (int a = 0; a < argc; ++a) {
        const juce::String kv(argv[a]);
        const auto id = kv.upToFirstOccurrenceOf("=", false, false);
        const float plain = kv.fromFirstOccurrenceOf("=", false, false).getFloatValue();
        auto* prm = p.apvts.getParameter(id);
        if (prm == nullptr) { std::fprintf(stderr, "unknown parameter %s\n", id.toRawUTF8()); return 2; }
        prm->setValueNotifyingHost(prm->convertTo0to1(plain));
    }
    p.prepareToPlay(sr, block);
    p.reset();
    const auto input = readF32(in);
    const int frames = int(input.size() / 2);
    std::vector<float> output(input.size());
    juce::AudioBuffer<float> buf(2, block);
    juce::MidiBuffer midi;
    for (int pos = 0; pos < frames; pos += block) {
        const int n = std::min(block, frames - pos);
        buf.setSize(2, n, false, false, true);
        for (int i = 0; i < n; ++i) {
            buf.setSample(0, i, input[size_t(pos + i) * 2]);
            buf.setSample(1, i, input[size_t(pos + i) * 2 + 1]);
        }
        if (core) {
            p.updateParameters();
            float* ch[2] = {buf.getWritePointer(0), buf.getWritePointer(1)};
            if (p.reducedActive) {
                for (int i = 0; i < n; ++i) {
                    const float g = p.reducedDrive.getNextValue();
                    for (int c = 0; c < 2; ++c)
                        ch[c][i] = static_cast<float>(p.reducedKernels[size_t(c)].process(double(ch[c][i]) * g));
                }
            } else
                p.preamp.processBlock(ch, 2, n);
            p.eq.processBlock(ch, 2, n);
            for (int c = 0; c < 2; ++c) p.saturation.processBlock(ch[c], n);
        } else
            p.processBlock(buf, midi);
        for (int i = 0; i < n; ++i) {
            output[size_t(pos + i) * 2] = buf.getSample(0, i);
            output[size_t(pos + i) * 2 + 1] = buf.getSample(1, i);
        }
    }
    writeF32(out, output);
    std::ofstream params((juce::String(out) + ".params").toStdString());
    for (auto* prm : p.getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*>(prm)) {
            char text[64];
            std::snprintf(text, sizeof text, "%.9g", double(p.apvts.getRawParameterValue(r->getParameterID())->load()));
            params << r->getParameterID() << "=" << text << "\n";
        }
    params << "latency=" << p.getLatencySamples() << "\n";
    return 0;
}

int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    if (argc >= 6 && juce::String(argv[1]) == "gen")
        return gen(argv[2], std::atof(argv[3]), std::atof(argv[4]), argv[5]);
    if (argc >= 6 && (juce::String(argv[1]) == "run" || juce::String(argv[1]) == "core"))
        return run(argv[2], argv[3], std::atof(argv[4]), std::atoi(argv[5]), argc - 6, argv + 6, juce::String(argv[1]) == "core");
    std::fprintf(stderr, "usage: HybridEQRender gen <kind> <seconds> <sr> <out> | run <in> <out> <sr> <block> [id=value ...]\n");
    return 1;
}
