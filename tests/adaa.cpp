// Tests the ADAA building blocks (Source/DSP/Adaa.h) without JUCE.
// Build: clang++ -std=c++20 -O2 -I Source/DSP tests/adaa.cpp -o /tmp/adaa && /tmp/adaa
#include "Adaa.h"
#include <cstdio>
#include <vector>
#include <complex>

using namespace dsp::adaa;
static int failures = 0;
static void check(bool ok, const char* what) { std::printf("  %s  %s\n", ok ? "PASS" : "FAIL", what); if (!ok) ++failures; }

// copies of the preamp's solvers (Source/DSP/Preamp.h), to verify the antiderivative formulas against them
static double solveLeg(double v, double kappa)
{
    double w = v >= 0.0 ? (kappa > 1e-9 ? std::min(v, std::log1p(v / kappa)) : v) : 0.0;
    for (int n = 0; n < 40; ++n) { if (w > 20.0) w = 20.0; const double e = std::exp(w); w -= (w + kappa * (e - 1.0) - v) / (1.0 + kappa * e); }
    return std::exp(std::min(w, 20.0));
}
static double railLimit(double v, double rail) { const double t = std::abs(v) / rail, t2 = t * t, t4 = t2 * t2; return v / std::pow(1.0 + t4 * t4, 0.125); }
static double coreSaturate(double v, double sat) { const double r = v / sat; return v / std::sqrt(1.0 + r * r); }

// derivative of F by a central difference compared with f
template <class F, class Fn> static double maxDerivativeError(F F_, Fn f, double lo, double hi, int n = 4000)
{
    double worst = 0.0;
    for (int i = 0; i < n; ++i) {
        const double x = lo + (hi - lo) * (i + 0.5) / n, h = 1e-5 * std::max(1.0, std::abs(x));
        worst = std::max(worst, std::abs((F_(x + h) - F_(x - h)) / (2 * h) - f(x)) / std::max(1.0, std::abs(f(x))));
    }
    return worst;
}

int main()
{
    std::printf("1) closed-form antiderivatives: F'(x) must equal f(x)\n");
    check(maxDerivativeError([](double x) { return logCosh(x); }, [](double x) { return std::tanh(x); }, -30, 30) < 1e-7, "ln cosh -> tanh (and finite for x = 1e4: " );
    check(std::isfinite(logCosh(1.0e4)) && std::abs(logCosh(1.0e4) - (1.0e4 - 0.6931471805599453)) < 1e-9, "ln cosh does not overflow");
    check(maxDerivativeError([](double x) { return coreSaturateF(x, 2.0); }, [](double x) { return coreSaturate(x, 2.0); }, -20, 20) < 1e-7, "core saturator, sat 2.0");
    for (double rail : {1.1, 1.2, 1.3, 1.5, 1.6})
        check(maxDerivativeError([&](double x) { return railLimitF(x, rail); }, [&](double x) { return railLimit(x, rail); }, -12, 12) < 1e-6, "rail limiter (tabulated), several rails");
    check(maxDerivativeError([](double x) { return asymLimitF(x, 1.5, 1.1); }, [](double x) { return x >= 0 ? railLimit(x, 1.5) : railLimit(x, 1.1); }, -12, 12) < 1e-6, "asymmetric limiter");
    {
        // table accuracy against a brute-force integral
        double worst = 0.0;
        for (double t : {0.1, 0.5, 0.9, 1.0, 1.1, 2.0, 5.0, 7.99, 8.0, 8.5, 20.0}) {
            double acc = 0.0; const int n = 200000; const double h = t / n;
            for (int i = 0; i < n; ++i) { const double a = i * h; acc += h / 6.0 * (RailTable::value(a) + 4 * RailTable::value(a + 0.5 * h) + RailTable::value(a + h)); }
            worst = std::max(worst, std::abs(railTable().integral(t) - acc));
        }
        std::printf("     rail table versus brute-force integration: worst difference %.2e\n", worst);
        check(worst < 1e-7, "rail table accurate to 1e-7 or better");
    }
    {
        double worstLeg = 0;
        for (double kappa : {0.0, 0.5, 2.0, 12.0}) {
            auto Fv = [&](double v) { return legF(solveLeg(v, kappa), kappa); };
            auto fv = [&](double v) { return solveLeg(v, kappa); };
            worstLeg = std::max(worstLeg, maxDerivativeError(Fv, fv, -15, 8, 600));
        }
        std::printf("     transistor leg: worst |F'-f| %.2e\n", worstLeg);
        check(worstLeg < 1e-4, "transistor leg antiderivative");
    }

    std::printf("2) a linear stage keeps its level at every frequency (all-pass linear path, half-sample delay)\n");
    {
        double worstDb = 0, worstDelayErr = 0;
        for (double f : {100.0, 1000.0, 5000.0, 10000.0, 16000.0, 20000.0}) {
            Residual st; const double w = 2 * 3.14159265358979323846 * f / 48000.0; const int n = 4800;
            double num = 0, den = 0, sumErr = 0; int cnt = 0;
            for (int i = 0; i < n; ++i) {
                const double x = std::sin(w * i), y = st.step(x, 1.7 * x, 0.85 * x * x, 1.7, Mode::On);
                if (i > 200) { num += y * y; den += 1.7 * 1.7 * x * x; const double ideal = 1.7 * std::sin(w * (i - 0.5)); sumErr += std::abs(y - ideal); ++cnt; }
            }
            worstDb = std::max(worstDb, std::abs(10 * std::log10(num / den)));
            if (f <= 5000.0) worstDelayErr = std::max(worstDelayErr, sumErr / cnt / 1.7);
        }
        std::printf("     worst level change %.4f dB; mean deviation from a half-sample delay up to 5 kHz %.4f\n", worstDb, worstDelayErr);
        check(worstDb < 0.001, "unity magnitude from 100 Hz to 20 kHz");
        check(worstDelayErr < 0.02, "about half a sample of delay up to 5 kHz");
        Residual off; check(off.step(0.3, 0.5, 0.1, 1.0, Mode::Off) == 0.5, "Off returns f unchanged");
    }

    std::printf("3) aliasing of a tanh saturator at 48 kHz (sine at a whole FFT bin, drive x2, x4, x10)\n");
    {
        const int N = 1 << 12; const int k0 = 375;            // 4.39 kHz: harmonics 1..5 below Nyquist
        auto run = [&](bool adaa, double drive) {
            Residual st; std::vector<double> y(N);
            for (int n = 0; n < N; ++n) { const double x = drive * std::sin(2 * 3.14159265358979323846 * k0 * n / N);
                y[n] = st.step(x, std::tanh(x), logCosh(x), 1.0, adaa ? Mode::On : Mode::Off); }
            // alias power: everything not on a harmonic bin
            double alias = 0, fund = 0;
            for (int b = 1; b < N / 2; ++b) {
                std::complex<double> acc = 0; for (int n = 0; n < N; ++n) acc += y[n] * std::exp(std::complex<double>(0, -2 * 3.14159265358979323846 * b * n / N));
                const double p = std::norm(acc) / double(N) / double(N);
                const bool harmonic = (b % k0) == 0;
                if (b == k0) fund = p; else if (!harmonic) alias += p;
            }
            return 10 * std::log10((alias + 1e-30) / fund);
        };
        for (double drive : {2.0, 4.0, 10.0}) { const double off = run(false, drive), on = run(true, drive);
        std::printf("     drive %.0f: plain %.1f dB, ADAA %.1f dB (improvement %.1f dB)\n", drive, off, on, off - on);
        if (drive >= 4.0) check(on < off - 4.0, "ADAA lowers the aliasing by at least 4 dB"); else check(on < off + 3.0, "no worse by more than 3 dB at a level where aliasing is already below -35 dB"); }
    }
    std::printf("\n%s (%d failed)\n", failures ? "ADAA TEST FAILED" : "adaa test passed", failures);
    return failures ? 1 : 0;
}
