#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>

// Cramping-free EQ filters by magnitude fitting.
//
// A normal (bilinear) biquad squeezes the analog frequency axis into 0..Nyquist, so shelves, cuts and bells whose
// corner sits near or above Nyquist visibly bend. Instead of raising the sample rate, the five biquad coefficients
// are fitted by least squares so the digital magnitude follows the ANALOG prototype across the whole audible
// range up to Nyquist. A corner at or above Nyquist is fine: the fit reproduces what that analog filter does
// in-band. The result is an ordinary IIR biquad (no added latency, same per-sample cost); the fit runs only
// when a control changes. Magnitude is matched, phase is whatever the biquad gives (it cannot follow the analog
// phase near Nyquist). Poles are parameterised by reflection coefficients, so the filter is always stable.
//
// Measured against the same filters designed at 8x the sample rate: see tools/eq_matching_experiment.py.
namespace dsp::fit {

struct Coeffs {
    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
};

enum class Shape { LowPass, HighPass, Peak, LowShelf, HighShelf };

struct Spec {
    Shape shape = Shape::Peak;
    double f0 = 1000.0;     // corner / centre frequency (may be at or above Nyquist)
    double Q = 0.7071;
    double gainDB = 0.0;    // peak and shelf gain
};

// Analog prototype, x = s / w0. Same shapes as the cookbook biquads the plugin has always used.
inline std::complex<double> analogResponse(const Spec& s, double f)
{
    using C = std::complex<double>;
    const C x(0.0, f / s.f0);
    const double A = std::pow(10.0, s.gainDB / 40.0), sA = std::sqrt(A);
    switch (s.shape) {
        case Shape::LowPass:   return 1.0 / (x * x + x / s.Q + 1.0);
        case Shape::HighPass:  return x * x / (x * x + x / s.Q + 1.0);
        case Shape::Peak:      return (x * x + x * (A / s.Q) + 1.0) / (x * x + x / (A * s.Q) + 1.0);
        case Shape::LowShelf:  return A * (x * x + x * (sA / s.Q) + A) / (A * x * x + x * (sA / s.Q) + 1.0);
        case Shape::HighShelf: return A * (A * x * x + x * (sA / s.Q) + 1.0) / (x * x + x * (sA / s.Q) + A);
    }
    return 1.0;
}

// Plain cookbook (bilinear) design, corner clamped just below Nyquist. Used as the starting point of the fit and
// as the fallback.
inline Coeffs cookbook(const Spec& s, double fs)
{
    const double pi = 3.14159265358979323846;
    const double f0 = std::min(s.f0, 0.49 * fs);
    const double A = std::pow(10.0, s.gainDB / 40.0), sA = std::sqrt(A);
    const double w0 = 2.0 * pi * f0 / fs, al = std::sin(w0) / (2.0 * s.Q), c = std::cos(w0);
    double b0, b1, b2, a0, a1, a2;
    switch (s.shape) {
        case Shape::LowPass:
            b0 = (1 - c) / 2; b1 = 1 - c; b2 = (1 - c) / 2; a0 = 1 + al; a1 = -2 * c; a2 = 1 - al; break;
        case Shape::HighPass:
            b0 = (1 + c) / 2; b1 = -(1 + c); b2 = (1 + c) / 2; a0 = 1 + al; a1 = -2 * c; a2 = 1 - al; break;
        case Shape::Peak:
            b0 = 1 + al * A; b1 = -2 * c; b2 = 1 - al * A; a0 = 1 + al / A; a1 = -2 * c; a2 = 1 - al / A; break;
        case Shape::LowShelf:
            b0 = A * ((A + 1) - (A - 1) * c + 2 * sA * al); b1 = 2 * A * ((A - 1) - (A + 1) * c);
            b2 = A * ((A + 1) - (A - 1) * c - 2 * sA * al); a0 = (A + 1) + (A - 1) * c + 2 * sA * al;
            a1 = -2 * ((A - 1) + (A + 1) * c); a2 = (A + 1) + (A - 1) * c - 2 * sA * al; break;
        case Shape::HighShelf:
        default:
            b0 = A * ((A + 1) + (A - 1) * c + 2 * sA * al); b1 = -2 * A * ((A - 1) + (A + 1) * c);
            b2 = A * ((A + 1) + (A - 1) * c - 2 * sA * al); a0 = (A + 1) - (A - 1) * c + 2 * sA * al;
            a1 = 2 * ((A - 1) - (A + 1) * c); a2 = (A + 1) - (A - 1) * c - 2 * sA * al; break;
    }
    return {b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0};
}

inline bool isStable(const Coeffs& c)
{
    return std::isfinite(c.a1) && std::isfinite(c.a2) && std::abs(c.a2) < 1.0 && std::abs(c.a1) < 1.0 + c.a2;
}

// Analog-matched starting point: poles by matched-z (corner capped below Nyquist), numerator magnitude-matched at
// DC, a middle frequency and Nyquist. Close to the final answer for most shapes; the fit then refines it.
inline Coeffs matchedStart(const Spec& s, double fs)
{
    using C = std::complex<double>;
    const double pi = 3.14159265358979323846;
    const double A = std::pow(10.0, s.gainDB / 40.0), sA = std::sqrt(A);
    double d2 = 1.0, d1 = 1.0 / s.Q, d0 = 1.0;                       // analog denominator in x = s / w0
    if (s.shape == Shape::Peak) d1 = 1.0 / (A * s.Q);
    else if (s.shape == Shape::LowShelf) { d2 = A; d1 = sA / s.Q; }
    else if (s.shape == Shape::HighShelf) { d1 = sA / s.Q; d0 = A; }
    const C disc = std::sqrt(C(d1 * d1 - 4.0 * d2 * d0, 0.0));
    const C x1 = (-d1 + disc) / (2.0 * d2), x2 = (-d1 - disc) / (2.0 * d2);
    const double k = 2.0 * pi * std::min(s.f0, 0.49 * fs) / fs;
    const C z1 = std::exp(x1 * k), z2 = std::exp(x2 * k);
    const double a1 = -std::real(z1 + z2), a2 = std::real(z1 * z2);
    const double fm = s.f0 <= 0.46 * fs ? s.f0 : 0.3 * fs, wm = 2.0 * pi * fm / fs;
    auto D = [&](double w) { const C e = std::exp(C(0, -w)); return std::norm(1.0 + a1 * e + a2 * e * e); };
    const double t0 = std::norm(analogResponse(s, 1.0e-3)) * D(0.0);
    const double t1 = std::norm(analogResponse(s, 0.5 * fs)) * D(pi);
    const double tm = std::norm(analogResponse(s, fm)) * D(wm);
    const double ph0 = std::cos(wm / 2) * std::cos(wm / 2), ph1 = std::sin(wm / 2) * std::sin(wm / 2), ph2 = std::sin(wm) * std::sin(wm);
    const double A2 = (tm - t0 * ph0 - t1 * ph1) / ph2;
    const double s0 = std::sqrt(std::max(t0, 0.0)), s1 = std::sqrt(std::max(t1, 0.0));
    const double W = (s0 + s1) / 2.0, r = std::sqrt(std::max(W * W + A2, 0.0));
    return {(W + r) / 2.0, (s0 - s1) / 2.0, (W - r) / 2.0, a1, a2};
}

// A magnitude fit cannot tell a zero from its mirror image outside the unit circle (same magnitude, different
// phase). Mirror any zero outside back in (rescaling the gain so the magnitude is unchanged) so the numerator is
// always minimum-phase: coefficients then vary continuously while a control is swept, with no phase flips.
inline Coeffs makeMinimumPhase(Coeffs c)
{
    using C = std::complex<double>;
    if (std::abs(c.b0) < 1.0e-12) return c;
    const double p = c.b1 / c.b0, q = c.b2 / c.b0;
    const C disc = std::sqrt(C(p * p - 4.0 * q, 0.0));
    C r1 = (-p + disc) / 2.0, r2 = (-p - disc) / 2.0;
    double gain = c.b0;
    for (C* r : {&r1, &r2})
        if (std::abs(*r) > 1.0) { gain *= std::abs(*r); *r = 1.0 / std::conj(*r); }
    c.b0 = gain; c.b1 = -gain * std::real(r1 + r2); c.b2 = gain * std::real(r1 * r2);
    return c;
}

namespace detail {

constexpr int kLogPoints = 64, kLinPoints = 32, kNearPoints = 24, kGrid = kLogPoints + kLinPoints + kNearPoints;
constexpr double kFloor = 1.0e-3;                               // -60 dB: stopband detail below this is not chased
constexpr double kDbPerNeper = 8.685889638065035;               // 20 / ln(10)

struct Grid {
    std::array<double, kGrid> f;                                // Hz
    std::array<std::complex<double>, kGrid> z1;                 // e^{-jw}
    std::array<double, kGrid> target;                           // analog magnitude
};

inline Grid makeGrid(const Spec& s, double fs)
{
    const double pi = 3.14159265358979323846;
    Grid g;
    const double lo = std::log(20.0), hi = std::log(0.499 * fs);
    for (int i = 0; i < kGrid; ++i) {
        double f;
        if (i < kLogPoints) f = std::exp(lo + (hi - lo) * i / (kLogPoints - 1));
        else if (i < kLogPoints + kLinPoints) f = 0.1 * fs + (0.499 - 0.1) * fs * (i - kLogPoints) / (kLinPoints - 1);
        else {
            // extra points around the corner so a narrow bell or a sharp resonance is not stepped over; the
            // span scales with the width of the feature (1 / Q, at most +-1.2 in ln frequency)
            const double span = std::min(1.2, 2.0 / std::max(s.Q, 0.5));
            const double u = (i - kLogPoints - kLinPoints) / double(kNearPoints - 1) * 2.0 - 1.0;
            f = std::clamp(s.f0 * std::exp(u * span), 20.0, 0.499 * fs);
        }
        g.f[i] = f;
        g.z1[i] = std::exp(std::complex<double>(0.0, -2.0 * pi * f / fs));
        g.target[i] = std::abs(analogResponse(s, f));
    }
    return g;
}

struct Params { double v[5]; };   // b0, b1, b2, u1, u2   (k = tanh(u), a1 = k1 (1 + k2), a2 = k2)

inline Coeffs toCoeffs(const Params& p)
{
    const double k1 = std::tanh(p.v[3]), k2 = std::tanh(p.v[4]);
    return {p.v[0], p.v[1], p.v[2], k1 * (1.0 + k2), k2};
}

inline Params toParams(const Coeffs& c)
{
    const double k2 = std::clamp(c.a2, -0.999, 0.999);
    const double k1 = std::clamp(c.a1 / (1.0 + k2), -0.999, 0.999);
    return {{c.b0, c.b1, c.b2, std::atanh(k1), std::atanh(k2)}};
}

// Residual (dB, with the -60 dB floor) and, optionally, its Jacobian. Returns the sum of squares.
inline double evaluate(const Grid& g, const Params& p, double* r, double (*J)[5])
{
    const Coeffs c = toCoeffs(p);
    const double k1 = std::tanh(p.v[3]), k2 = std::tanh(p.v[4]);
    const double da1du1 = (1 - k1 * k1) * (1 + k2), da1du2 = k1 * (1 - k2 * k2), da2du2 = 1 - k2 * k2;
    double cost = 0.0;
    for (int i = 0; i < kGrid; ++i) {
        const std::complex<double> z = g.z1[i], z2 = z * z;
        const std::complex<double> B = c.b0 + c.b1 * z + c.b2 * z2, A = 1.0 + c.a1 * z + c.a2 * z2;
        const double H = std::abs(B) / std::abs(A);
        const double ri = kDbPerNeper * std::log((H + kFloor) / (g.target[i] + kFloor));
        r[i] = ri; cost += ri * ri;
        if (J) {
            const double wgt = kDbPerNeper * H / (H + kFloor);           // d/dlogH of the floored dB error
            const std::complex<double> iB = 1.0 / B, iA = 1.0 / A;
            const double dB0 = std::real(iB), dB1 = std::real(z * iB), dB2 = std::real(z2 * iB);
            const double dA1 = -std::real(z * iA), dA2 = -std::real(z2 * iA);   // d log|H| / d a
            J[i][0] = wgt * dB0; J[i][1] = wgt * dB1; J[i][2] = wgt * dB2;
            J[i][3] = wgt * dA1 * da1du1;
            J[i][4] = wgt * (dA1 * da1du2 + dA2 * da2du2);
        }
    }
    return cost;
}

// Levenberg-Marquardt, 5 parameters. Returns the final cost.
inline double levenbergMarquardt(const Grid& g, Params& p, int maxIter)
{
    double r[kGrid], rTry[kGrid], J[kGrid][5];
    double cost = evaluate(g, p, r, J), lambda = 1.0e-3;
    for (int it = 0; it < maxIter; ++it) {
        double JtJ[5][5] = {}, grad[5] = {};
        for (int i = 0; i < kGrid; ++i)
            for (int a = 0; a < 5; ++a) {
                grad[a] += J[i][a] * r[i];
                for (int b = 0; b < 5; ++b) JtJ[a][b] += J[i][a] * J[i][b];
            }
        bool improved = false;
        for (int attempt = 0; attempt < 8 && !improved; ++attempt) {
            double M[5][6];
            for (int a = 0; a < 5; ++a) {
                for (int b = 0; b < 5; ++b) M[a][b] = JtJ[a][b] + (a == b ? lambda * (JtJ[a][a] + 1.0e-9) : 0.0);
                M[a][5] = -grad[a];
            }
            bool singular = false;
            for (int c = 0; c < 5 && !singular; ++c) {                 // Gaussian elimination with partial pivoting
                int pv = c;
                for (int rr = c + 1; rr < 5; ++rr) if (std::abs(M[rr][c]) > std::abs(M[pv][c])) pv = rr;
                if (std::abs(M[pv][c]) < 1.0e-300) { singular = true; break; }
                if (pv != c) for (int k = 0; k < 6; ++k) std::swap(M[c][k], M[pv][k]);
                for (int rr = c + 1; rr < 5; ++rr) {
                    const double m = M[rr][c] / M[c][c];
                    for (int k = c; k < 6; ++k) M[rr][k] -= m * M[c][k];
                }
            }
            if (singular) { lambda *= 10.0; continue; }
            double delta[5];
            for (int a = 4; a >= 0; --a) {
                double s = M[a][5];
                for (int b = a + 1; b < 5; ++b) s -= M[a][b] * delta[b];
                delta[a] = s / M[a][a];
            }
            Params pn = p;
            for (int a = 0; a < 5; ++a) pn.v[a] += delta[a];
            pn.v[3] = std::clamp(pn.v[3], -6.0, 6.0); pn.v[4] = std::clamp(pn.v[4], -6.0, 6.0);
            const double costTry = evaluate(g, pn, rTry, nullptr);
            if (std::isfinite(costTry) && costTry < cost) {
                const double rel = (cost - costTry) / (cost + 1.0e-30);
                p = pn; cost = costTry; lambda = std::max(lambda * 0.3, 1.0e-9);
                evaluate(g, p, r, J);
                improved = true;
                if (rel < 1.0e-7) return cost;
            } else {
                lambda *= 5.0;
            }
        }
        if (!improved) break;
    }
    return cost;
}

inline double maxAbsResidual(const Grid& g, const Coeffs& c)
{
    double worst = 0.0;
    for (int i = 0; i < kGrid; ++i) {
        const std::complex<double> z = g.z1[i], z2 = z * z;
        const double H = std::abs((c.b0 + c.b1 * z + c.b2 * z2) / (1.0 + c.a1 * z + c.a2 * z2));
        worst = std::max(worst, std::abs(kDbPerNeper * std::log((H + kFloor) / (g.target[i] + kFloor))));
    }
    return worst;
}

} // namespace detail

// Result of a design. `fitted` is false when the cookbook fallback was used.
struct Result { Coeffs c; bool fitted = false; double maxErrorDB = 0.0; };

// Fits the biquad to the analog prototype. `warm` (e.g. the previous coefficients while a control is being swept)
// makes the solve converge in a few iterations and keeps coefficients continuous.
inline Result design(const Spec& spec, double fs, const Coeffs* warm = nullptr)
{
    const bool gainShape = spec.shape == Shape::Peak || spec.shape == Shape::LowShelf || spec.shape == Shape::HighShelf;
    if (gainShape && std::abs(spec.gainDB) < 0.005) return {Coeffs{}, true, 0.0};       // flat

    const Coeffs base = cookbook(spec, fs);
    if (!(fs > 0.0) || !(spec.f0 > 0.0) || !(spec.Q > 0.0) || !std::isfinite(spec.gainDB)) return {base, false, 0.0};

    const detail::Grid g = detail::makeGrid(spec, fs);
    Coeffs best = base; double bestErr = 1.0e30;
    auto consider = [&](const Coeffs& start, int iterations) {
        detail::Params p = detail::toParams(start);
        detail::levenbergMarquardt(g, p, iterations);
        const Coeffs out = makeMinimumPhase(detail::toCoeffs(p));
        const double e = detail::maxAbsResidual(g, out);
        if (std::isfinite(e) && isStable(out) && e < bestErr) { bestErr = e; best = out; }
    };
    constexpr double kGood = 0.3;                                  // dB: good enough, stop trying other starts
    if (warm && isStable(*warm)) {
        // A control being swept stays on its current solution (continuous poles and zeros, no click) unless
        // that solution has become poor.
        consider(*warm, 40);
        if (bestErr <= 0.8) return {best, true, bestErr};
    }
    if (bestErr > kGood) consider(matchedStart(spec, fs), 80);
    if (bestErr > kGood) consider(base, 80);
    if (bestErr > kGood) { Spec s2 = spec; s2.f0 = std::min(spec.f0, 0.25 * fs); consider(cookbook(s2, fs), 80); }
    if (bestErr > 1.5 || !std::isfinite(bestErr)) return {base, false, detail::maxAbsResidual(g, base)};
    return {best, true, bestErr};
}

// Designs a cascade of biquads (the cut filters) whose COMBINED magnitude follows the product of the analog
// stages. Each stage is fitted on its own first, then refined against what the other stages leave behind,
// which removes most of the error that would otherwise add up across stages. out[i] is stage i.
inline double designCascade(const Spec* specs, int n, double fs, const Coeffs* warm, Coeffs* out)
{
    for (int i = 0; i < n; ++i) out[i] = design(specs[i], fs, warm ? &warm[i] : nullptr).c;
    detail::Grid g = detail::makeGrid(specs[0], fs);
    std::array<double, detail::kGrid> total;
    for (int k = 0; k < detail::kGrid; ++k) {
        double t = 1.0;
        for (int i = 0; i < n; ++i) t *= std::abs(analogResponse(specs[i], g.f[k]));
        total[k] = t;
    }
    auto stageMag = [&](const Coeffs& c, int k) {
        const std::complex<double> z = g.z1[k], z2 = z * z;
        return std::abs((c.b0 + c.b1 * z + c.b2 * z2) / (1.0 + c.a1 * z + c.a2 * z2));
    };
    auto combinedError = [&] {
        double worst = 0.0;
        for (int k = 0; k < detail::kGrid; ++k) {
            double m = 1.0;
            for (int i = 0; i < n; ++i) m *= stageMag(out[i], k);
            worst = std::max(worst, std::abs(detail::kDbPerNeper * std::log((m + detail::kFloor) / (total[k] + detail::kFloor))));
        }
        return worst;
    };
    double best = combinedError();
    if (n < 2) return best;
    for (int pass = 0; pass < 4 && best > 0.05; ++pass)
        for (int i = 0; i < n; ++i) {
            detail::Grid gi = g;
            for (int k = 0; k < detail::kGrid; ++k) {
                double others = 1.0;
                for (int j = 0; j < n; ++j) if (j != i) others *= stageMag(out[j], k);
                gi.target[k] = std::min(total[k] / std::max(others, 1.0e-9), 1.0e3);
            }
            detail::Params p = detail::toParams(out[i]);
            detail::levenbergMarquardt(gi, p, 30);
            const Coeffs c = makeMinimumPhase(detail::toCoeffs(p));
            if (!isStable(c)) continue;
            const Coeffs old = out[i];
            out[i] = c;
            const double e = combinedError();
            if (e < best) best = e; else out[i] = old;
        }
    return best;
}

} // namespace dsp::fit
