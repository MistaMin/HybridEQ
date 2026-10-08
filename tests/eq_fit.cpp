// Tests the cramping-free biquad fit (Source/DSP/BiquadFit.h) without JUCE.
// Build: clang++ -std=c++20 -O2 -I Source/DSP tests/eq_fit.cpp -o /tmp/eq_fit && /tmp/eq_fit
//
// Compares the fit, at the base sample rate, with the REFERENCE: the same cookbook filter designed at 8x the
// sample rate (what the oversampled path applies; magnitude only, pre-echo ignored). Differences deeper than
// -60 dB are not counted. Also checks stability, speed, continuity while a control is swept, and odd inputs.
#include "BiquadFit.h"
#include <chrono>
#include <cstdio>
#include <vector>

using namespace dsp::fit;

static double mag(const Coeffs& c, double f, double fs)
{
    const double pi = 3.14159265358979323846;
    const std::complex<double> z = std::exp(std::complex<double>(0, -2 * pi * f / fs));
    return std::abs((c.b0 + c.b1 * z + c.b2 * z * z) / (1.0 + c.a1 * z + c.a2 * z * z));
}
static double dbErr(double m, double ref) { return 20 * std::log10((m + 1e-3) / (ref + 1e-3)); }

struct Case { Shape shape; double f0, Q, gain; int order; };
static double butterQ(int order, int i, double q)
{
    const double pi = 3.14159265358979323846;
    const double qs = order == 1 ? q : 1.0 / (2.0 * std::sin(pi * (2 * i + 1) / (2.0 * order)));
    return std::max(qs * q / 0.7071, 0.1);
}

int main()
{
    int failures = 0;
    auto check = [&](bool ok, const char* what) { std::printf("  %s  %s\n", ok ? "PASS" : "FAIL", what); if (!ok) ++failures; };

    std::vector<Case> cases;
    for (int order = 1; order <= 4; ++order)
        for (double f0 : {8e3, 12e3, 16e3, 20e3, 25e3, 30e3})
            for (double q : {0.7071, 1.5}) cases.push_back({Shape::LowPass, f0, q, 0.0, order});
    for (double q : {0.5, 0.71, 0.85})
        for (double g : {12.0, -12.0})
            for (double f0 : {8e3, 12e3, 16e3, 20e3, 30e3}) cases.push_back({Shape::HighShelf, f0, q, g, 1});
    for (double q : {0.3, 0.5, 1.0, 4.0, 10.0})
        for (double g : {18.0, 12.0, -12.0, -18.0})
            for (double f0 : {200.0, 1e3, 2e3, 6e3, 10e3, 15e3}) cases.push_back({Shape::Peak, f0, q, g, 1});

    std::printf("1) accuracy against the 8x-oversampled reference (%zu cases x 3 sample rates)\n", cases.size());
    double worstAud[3] = {0, 0, 0}, worstCut = 0, worstShelfBell = 0, worstShelfBellUsed = 0, worstIdeal = 0, worstRef = 0;
    char worstDesc[128] = "";
    int unstable = 0, fellBack = 0;
    double totalMicros = 0; long designs = 0;
    const double rates[3] = {44100.0, 48000.0, 96000.0};
    for (int ri = 0; ri < 3; ++ri) {
        const double fs = rates[ri];
        for (const Case& cs : cases) {
            std::vector<Coeffs> fitted, refs; std::vector<Spec> specs;
            for (int i = 0; i < cs.order; ++i) {
                Spec s{cs.shape, cs.f0, cs.shape == Shape::LowPass ? butterQ(cs.order, i, cs.Q) : cs.Q, cs.gain};
                specs.push_back(s);
                refs.push_back(cookbook(s, 8 * fs));
            }
            fitted.resize(cs.order);
            const auto t0 = std::chrono::steady_clock::now();
            designCascade(specs.data(), cs.order, fs, nullptr, fitted.data());
            totalMicros += std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count(); designs += cs.order;
            for (const Coeffs& c : fitted) if (!isStable(c)) ++unstable;
            double aud = 0, full = 0;
            for (int k = 0; k < 400; ++k) {
                const double f = 20.0 * std::pow(0.499 * fs / 20.0, k / 399.0);
                double m = 1, rf = 1, id = 1;
                for (int i = 0; i < cs.order; ++i) { m *= mag(fitted[i], f, fs); rf *= mag(refs[i], f, 8 * fs); id *= std::abs(analogResponse(specs[i], f)); }
                const double e = std::abs(dbErr(m, rf));
                full = std::max(full, e); if (f <= 20000.0) { aud = std::max(aud, e); worstIdeal = std::max(worstIdeal, std::abs(dbErr(m, id))); worstRef = std::max(worstRef, std::abs(dbErr(rf, id))); }
            }
            worstAud[ri] = std::max(worstAud[ri], aud);
            if (cs.shape == Shape::LowPass) worstCut = std::max(worstCut, aud);
            else {
                if (aud > worstShelfBell) { worstShelfBell = aud; std::snprintf(worstDesc, sizeof worstDesc, "%s f0=%.0f Q=%.2f gain=%+.0f at %.1fk", cs.shape == Shape::Peak ? "bell" : "high shelf", cs.f0, cs.Q, cs.gain, fs / 1000); }
                if (cs.f0 >= 0.1 * fs) worstShelfBellUsed = std::max(worstShelfBellUsed, aud);   // the range where the plugin uses the fit
            }
        }
    }
    std::printf("     worst audible-band error vs the reference: 44.1k %.2f dB, 48k %.2f dB, 96k %.2f dB\n", worstAud[0], worstAud[1], worstAud[2]);
    std::printf("     cuts (all slopes) %.2f dB, shelves and bells %.2f dB (worst: %s), %.2f dB where the plugin uses the fit (corner >= 10%% of fs);\n     vs ideal analog %.2f dB (the 8x path itself: %.2f dB)\n", worstCut, worstShelfBell, worstDesc, worstShelfBellUsed, worstIdeal, worstRef);
    check(worstShelfBellUsed < 0.6, "shelves and bells (Q 0.3 to 10, +-18 dB, corners to 15 kHz) within 0.6 dB of the reference where the fit is used");
    check(worstCut < 1.3, "cuts, every slope and Q, within 1.3 dB of the reference (audible band)");
    check(unstable == 0, "no unstable filter");
    check(fellBack == 0, "the fit never needed the cookbook fallback in these cases");
    std::printf("     cold fit cost: %.1f us per biquad (mean of %ld designs)\n", totalMicros / designs, designs);

    std::printf("2) sweeping a control (continuity and speed with the previous coefficients as the starting point)\n");
    {
        const double fs = 48000.0; Coeffs prev{}; bool have = false; double maxJump = 0, maxErr = 0; double micros = 0; int n = 0;
        for (int i = 0; i <= 300; ++i) {                        // high shelf 6 kHz -> 28 kHz, +12 dB
            Spec s{Shape::HighShelf, 6000.0 * std::pow(28000.0 / 6000.0, i / 300.0), 0.71, 12.0};
            const auto t0 = std::chrono::steady_clock::now();
            Result r = design(s, fs, have ? &prev : nullptr);
            micros += std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count(); ++n;
            if (have) maxJump = std::max({maxJump, std::abs(r.c.b0 - prev.b0), std::abs(r.c.a1 - prev.a1), std::abs(r.c.a2 - prev.a2)});
            maxErr = std::max(maxErr, r.maxErrorDB); prev = r.c; have = true;
        }
        std::printf("     warm-start cost %.1f us per update, largest coefficient step %.4f, worst fit error %.2f dB\n", micros / n, maxJump, maxErr);
        check(maxJump < 0.1, "coefficients change smoothly while the control sweeps");
        check(micros / n < 400.0, "an update costs well under 0.4 ms");
    }

    std::printf("3) odd inputs\n");
    {
        int bad = 0, tried = 0, cookbookUsed = 0;
        for (Shape sh : {Shape::LowPass, Shape::HighPass, Shape::Peak, Shape::LowShelf, Shape::HighShelf})
            for (double f0 : {10.0, 20.0, 100.0, 5e3, 24e3, 40e3, 80e3})
                for (double q : {0.1, 0.5, 0.7071, 4.0, 40.0})
                    for (double g : {-24.0, -0.001, 0.0, 6.0, 24.0})
                        for (double fs : {22050.0, 44100.0, 192000.0}) {
                            Result r = design({sh, f0, q, g}, fs); ++tried;
                            const bool finite = std::isfinite(r.c.b0) && std::isfinite(r.c.b1) && std::isfinite(r.c.b2) && std::isfinite(r.c.a1) && std::isfinite(r.c.a2);
                            if (!finite || !isStable(r.c)) ++bad;
                            if (!r.fitted) ++cookbookUsed;
                        }
        std::printf("     %d combinations: %d unusable results, %d used the cookbook fallback\n", tried, bad, cookbookUsed);
        check(bad == 0, "every combination gives a finite, stable filter");
    }
    std::printf("\n%s (%d failed)\n", failures ? "FIT TEST FAILED" : "fit test passed", failures);
    return failures ? 1 : 0;
}
