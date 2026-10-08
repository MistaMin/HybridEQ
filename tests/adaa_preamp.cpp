// ADAA on the static preamp models (Circuit off): aliasing, transparency for quiet signals, silence, stability.
#include <juce_audio_basics/juce_audio_basics.h>
#include "DSP/Preamp.h"
#include <chrono>
#include <complex>
#include <cstdio>
#include <vector>

using dsp::PreampEngine;
using dsp::PreampType;

static int failures = 0;
static void check(bool ok, const char* what) { std::printf("  %s  %s\n", ok ? "PASS" : "FAIL", what); if (!ok) ++failures; }

static const char* names[] = {"Brit", "N-Type", "FSF", "Off", "A-Type"};
static constexpr double kPi = 3.14159265358979323846;

// Steady-state output of the engine for a bin-aligned sine at `rate` = M x sr (so M x N samples per period).
static std::vector<float> run(PreampType t, bool adaa, double driveDb, double amp, int bin, int N, double sr, int M = 1, int block = 512)
{
    PreampEngine pe; pe.prepare(sr * M); pe.setType(t); pe.setCircuitEnabled(false); pe.setAdaaEnabled(adaa); pe.setDriveDB(float(driveDb));
    const size_t period = static_cast<size_t>(N) * static_cast<size_t>(M), total = period * 5;     // 4 periods to settle, 1 measured
    std::vector<float> all(total);
    for (size_t s = 0; s < total; s += static_cast<size_t>(block)) {
        const int len = static_cast<int>(std::min<size_t>(static_cast<size_t>(block), total - s));
        juce::AudioBuffer<float> b(2, len);
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < len; ++i) b.setSample(c, i, float(amp * std::sin(2 * kPi * bin * double(s + size_t(i)) / double(period))));
        pe.process(b);
        for (int i = 0; i < len; ++i) all[s + size_t(i)] = b.getSample(0, i);
    }
    return std::vector<float>(all.begin() + static_cast<long>(period * 4), all.end());
}

static std::vector<std::complex<double>> fft(const std::vector<float>& y)
{
    const size_t N = y.size();
    std::vector<std::complex<double>> a(N);
    for (size_t i = 0; i < N; ++i) a[i] = double(y[i]);
    for (size_t i = 1, j = 0; i < N; ++i) { size_t bit = N >> 1; for (; j & bit; bit >>= 1) j ^= bit; j ^= bit; if (i < j) std::swap(a[i], a[j]); }
    for (size_t len = 2; len <= N; len <<= 1) {
        const std::complex<double> w(std::cos(-2 * kPi / double(len)), std::sin(-2 * kPi / double(len)));
        for (size_t i = 0; i < N; i += len) { std::complex<double> wn = 1; for (size_t j = 0; j < len / 2; ++j) { auto u = a[i + j], v = a[i + j + len / 2] * wn; a[i + j] = u + v; a[i + j + len / 2] = u - v; wn *= w; } }
    }
    return a;
}

// Power in every bin that is not a multiple of the fundamental's bin (folded harmonics, i.e. aliasing, plus whatever
// else is not a harmonic), relative to the fundamental, in dB; and the fundamental's level in dB.
struct Spectrum { double aliasDb, fundDb; };
static Spectrum analyse(const std::vector<float>& y, int N, int k0)
{
    const auto A = fft(y);
    const double scale = double(N) / double(y.size());
    double alias = 0;
    for (int b = 1; b < N / 2; ++b) if (b % k0 != 0) alias += std::norm(A[size_t(b)] * scale);
    const double fund = std::norm(A[size_t(k0)] * scale);
    return {10 * std::log10((alias + 1e-30) / (fund + 1e-30)), 10 * std::log10(fund + 1e-30)};
}

int main()
{
    const double sr = 48000.0; const int N = 1 << 14, k0 = 1500;      // 4.39 kHz
    const PreampType types[] = {PreampType::Brit, PreampType::N, PreampType::FSF, PreampType::AType};

    std::printf("1) aliasing (energy off the harmonic bins, dB re the fundamental, lower is better) and fundamental level change\n");
    // needDb: required alias improvement (negative: may be that much worse, but see the floor below)
    struct Level { const char* name; double drive, amp; int bin; double needDb; double maxChange; };
    const Level levels[] = {{"loud 4.4 kHz (0.9, +6 dB drive)", 6.0, 0.9, k0, 3.0, 0.3},
                            {"moderate 4.4 kHz (0.3, 0 dB drive)", 0.0, 0.3, k0, -1.0, 0.25},
                            {"moderate 10 kHz (0.3, 0 dB drive)", 0.0, 0.3, 3413, 0.0, 0.35}};
    for (const Level& lv : levels) {
        std::printf("   %s\n", lv.name);
        for (PreampType t : types) {
            const Spectrum off = analyse(run(t, false, lv.drive, lv.amp, lv.bin, N, sr), N, lv.bin);
            const Spectrum on = analyse(run(t, true, lv.drive, lv.amp, lv.bin, N, sr), N, lv.bin);
            const Spectrum off2 = analyse(run(t, false, lv.drive, lv.amp, lv.bin, N, sr, 2), N, lv.bin);
            const double droop = on.fundDb - off.fundDb;
            std::printf("     %-7s 1x alias %.1f -> %.1f dB with ADAA (2x without: %.1f); fundamental %+.2f dB\n", names[int(t)], off.aliasDb, on.aliasDb, off2.aliasDb, droop);
            check((on.aliasDb < off.aliasDb - lv.needDb || on.aliasDb < -100.0) && std::abs(droop) <= lv.maxChange, names[int(t)]);
        }
    }

    std::printf("2) quiet signals keep their level (-60 dBFS, 1 kHz-ish): ADAA on versus off\n");
    for (PreampType t : types) {
        const Spectrum a = analyse(run(t, false, 0.0, 0.001, 340, N, sr), N, 340), b = analyse(run(t, true, 0.0, 0.001, 340, N, sr), N, 340);
        std::printf("     %-7s level change %+.4f dB\n", names[int(t)], b.fundDb - a.fundDb);
        check(std::abs(b.fundDb - a.fundDb) < 0.01, names[int(t)]);
    }

    std::printf("3) silence stays silent; hard overdrive stays finite and close to the unprocessed peak\n");
    for (PreampType t : types) {
        auto z = run(t, true, 0.0, 0.0, 1, 4096, sr);
        double m = 0; for (float v : z) m = std::max(m, double(std::abs(v)));
        auto big = run(t, true, 20.0, 1.0, 77, 4096, sr, 1, 64), bigOff = run(t, false, 20.0, 1.0, 77, 4096, sr, 1, 64);
        auto huge = run(t, true, 24.0, 8.0, 77, 4096, sr, 1, 64), hugeOff = run(t, false, 24.0, 8.0, 77, 4096, sr, 1, 64);
        double hm = 0, hmOff = 0; bool hugeFinite = true;
        for (size_t i = 0; i < huge.size(); ++i) { hugeFinite &= std::isfinite(huge[i]); hm = std::max(hm, double(std::abs(huge[i]))); hmOff = std::max(hmOff, double(std::abs(hugeOff[i]))); }
        bool finite = true; double bm = 0, bmOff = 0; for (size_t i = 0; i < big.size(); ++i) { finite &= std::isfinite(big[i]); bm = std::max(bm, double(std::abs(big[i]))); bmOff = std::max(bmOff, double(std::abs(bigOff[i]))); }
        std::printf("     %-7s silence peak %.2e; +20 dB drive at full scale: peak %.2f (ADAA off %.2f); +24 dB at +18 dBFS: %.2f (%.2f)\n", names[int(t)], m, bm, bmOff, hm, hmOff);
        check(m < 1e-9 && finite && hugeFinite && bm < 1.25 * bmOff + 0.05 && hm < 3.0 * hmOff + 0.1, names[int(t)]);
    }

    std::printf("4) drive sweep while processing (ADAA steps aside): finite, and no bigger jumps than without ADAA\n");
    for (PreampType t : types) {
        double stepOf[2] = {0, 0}; bool finite = true;
        for (int pass = 0; pass < 2; ++pass) {
            PreampEngine pe; pe.prepare(sr); pe.setType(t); pe.setCircuitEnabled(false); pe.setAdaaEnabled(pass == 1);
            float last = 0;
            for (int blk = 0; blk < 400; ++blk) {
                pe.setDriveDB(float(-20 + 30.0 * (0.5 + 0.5 * std::sin(blk * 0.05))));
                juce::AudioBuffer<float> b(2, 128);
                for (int i = 0; i < 128; ++i) { const float s = float(0.3 * std::sin(2 * kPi * 1000.0 * double(blk * 128 + i) / sr)); b.setSample(0, i, s); b.setSample(1, i, s); }
                pe.process(b);
                for (int i = 0; i < 128; ++i) { const float v = b.getSample(0, i); finite &= std::isfinite(v); if (blk > 5) stepOf[pass] = std::max(stepOf[pass], double(std::abs(v - last))); last = v; }
            }
        }
        std::printf("     %-7s largest sample-to-sample step %.2f (ADAA off %.2f)\n", names[int(t)], stepOf[1], stepOf[0]);
        check(finite && stepOf[1] <= 1.1 * stepOf[0] + 0.05, names[int(t)]);
    }

    std::printf("5) cost (stereo, 48 kHz, 512-sample blocks), ns per sample per channel\n");
    for (PreampType t : types) {
        double ns[2];
        for (int pass = 0; pass < 2; ++pass) {
            PreampEngine pe; pe.prepare(sr); pe.setType(t); pe.setCircuitEnabled(false); pe.setAdaaEnabled(pass == 1); pe.setDriveDB(6.0f);
            juce::AudioBuffer<float> b(2, 512);
            for (int i = 0; i < 512; ++i) { const float v = float(0.5 * std::sin(0.09 * i)); b.setSample(0, i, v); b.setSample(1, i, v); }
            const auto t0 = std::chrono::steady_clock::now();
            const int blocks = 4000;
            for (int k = 0; k < blocks; ++k) { for (int i = 0; i < 512; ++i) { const float v = float(0.5 * std::sin(0.09 * i)); b.setSample(0, i, v); b.setSample(1, i, v); } pe.process(b); }
            ns[pass] = std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - t0).count() / (double(blocks) * 512 * 2);
        }
        std::printf("     %-7s %.1f ns without ADAA, %.1f ns with\n", names[int(t)], ns[0], ns[1]);
    }

    std::printf("\n%s (%d failed)\n", failures ? "ADAA PREAMP TEST FAILED" : "adaa preamp test passed", failures);
    return failures ? 1 : 0;
}
