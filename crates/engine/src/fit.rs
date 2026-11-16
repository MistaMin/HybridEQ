//! Cramping-free EQ filters by magnitude fitting.
//!
//! A bilinear biquad squeezes the analog frequency axis into 0..Nyquist, so shelves, cuts and bells
//! whose corner sits near or above Nyquist bend. Instead of raising the sample rate, the five biquad
//! coefficients are fitted by least squares (Levenberg-Marquardt) so the digital magnitude follows the
//! ANALOG prototype up to Nyquist. The result is an ordinary IIR biquad (no latency, same per-sample
//! cost); the fit runs only when a control changes. Poles are parameterised by reflection
//! coefficients, so the filter is always stable. Magnitude is matched, phase is not.

use crate::biquad::Coeffs;
use std::f64::consts::PI;

#[derive(Debug, Clone, Copy, PartialEq)]
pub enum Shape {
    LowPass,
    HighPass,
    Peak,
    LowShelf,
    HighShelf,
}

#[derive(Debug, Clone, Copy, PartialEq)]
pub struct Spec {
    pub shape: Shape,
    /// Corner or centre frequency (may be at or above Nyquist).
    pub f0: f64,
    pub q: f64,
    pub gain_db: f64,
}

#[derive(Clone, Copy)]
struct C64 {
    re: f64,
    im: f64,
}

impl C64 {
    const fn new(re: f64, im: f64) -> Self {
        Self { re, im }
    }
    fn add(self, o: C64) -> C64 {
        C64::new(self.re + o.re, self.im + o.im)
    }
    fn sub(self, o: C64) -> C64 {
        C64::new(self.re - o.re, self.im - o.im)
    }
    fn mul(self, o: C64) -> C64 {
        C64::new(self.re * o.re - self.im * o.im, self.re * o.im + self.im * o.re)
    }
    fn scale(self, k: f64) -> C64 {
        C64::new(self.re * k, self.im * k)
    }
    fn div(self, o: C64) -> C64 {
        let d = o.re * o.re + o.im * o.im;
        C64::new((self.re * o.re + self.im * o.im) / d, (self.im * o.re - self.re * o.im) / d)
    }
    fn norm(self) -> f64 {
        self.re * self.re + self.im * self.im
    }
    fn abs(self) -> f64 {
        self.re.hypot(self.im)
    }
    fn exp(self) -> C64 {
        let e = self.re.exp();
        C64::new(e * self.im.cos(), e * self.im.sin())
    }
    fn conj(self) -> C64 {
        C64::new(self.re, -self.im)
    }
    fn recip(self) -> C64 {
        C64::new(1.0, 0.0).div(self)
    }
    /// Principal square root of a real number as a complex value.
    fn sqrt_real(v: f64) -> C64 {
        if v >= 0.0 {
            C64::new(v.sqrt(), 0.0)
        } else {
            C64::new(0.0, (-v).sqrt())
        }
    }
}

/// Analog prototype, x = s / w0. Same shapes as the cookbook biquads.
pub fn analog_response_mag(s: &Spec, f: f64) -> f64 {
    analog_response(s, f).abs()
}

fn analog_response(s: &Spec, f: f64) -> C64 {
    let x = C64::new(0.0, f / s.f0);
    let a = 10f64.powf(s.gain_db / 40.0);
    let sa = a.sqrt();
    let one = C64::new(1.0, 0.0);
    let x2 = x.mul(x);
    match s.shape {
        Shape::LowPass => one.div(x2.add(x.scale(1.0 / s.q)).add(one)),
        Shape::HighPass => x2.div(x2.add(x.scale(1.0 / s.q)).add(one)),
        Shape::Peak => x2.add(x.scale(a / s.q)).add(one).div(x2.add(x.scale(1.0 / (a * s.q))).add(one)),
        Shape::LowShelf => x2
            .add(x.scale(sa / s.q))
            .add(C64::new(a, 0.0))
            .scale(a)
            .div(x2.scale(a).add(x.scale(sa / s.q)).add(one)),
        Shape::HighShelf => x2
            .scale(a)
            .add(x.scale(sa / s.q))
            .add(one)
            .scale(a)
            .div(x2.add(x.scale(sa / s.q)).add(C64::new(a, 0.0))),
    }
}

/// Plain cookbook (bilinear) design, corner clamped just below Nyquist.
pub fn cookbook(s: &Spec, fs: f64) -> Coeffs {
    let f0 = s.f0.min(0.49 * fs);
    let a = 10f64.powf(s.gain_db / 40.0);
    let sa = a.sqrt();
    let w0 = 2.0 * PI * f0 / fs;
    let al = w0.sin() / (2.0 * s.q);
    let c = w0.cos();
    let (b0, b1, b2, a0, a1, a2);
    match s.shape {
        Shape::LowPass => {
            b0 = (1.0 - c) / 2.0;
            b1 = 1.0 - c;
            b2 = (1.0 - c) / 2.0;
            a0 = 1.0 + al;
            a1 = -2.0 * c;
            a2 = 1.0 - al;
        }
        Shape::HighPass => {
            b0 = (1.0 + c) / 2.0;
            b1 = -(1.0 + c);
            b2 = (1.0 + c) / 2.0;
            a0 = 1.0 + al;
            a1 = -2.0 * c;
            a2 = 1.0 - al;
        }
        Shape::Peak => {
            b0 = 1.0 + al * a;
            b1 = -2.0 * c;
            b2 = 1.0 - al * a;
            a0 = 1.0 + al / a;
            a1 = -2.0 * c;
            a2 = 1.0 - al / a;
        }
        Shape::LowShelf => {
            b0 = a * ((a + 1.0) - (a - 1.0) * c + 2.0 * sa * al);
            b1 = 2.0 * a * ((a - 1.0) - (a + 1.0) * c);
            b2 = a * ((a + 1.0) - (a - 1.0) * c - 2.0 * sa * al);
            a0 = (a + 1.0) + (a - 1.0) * c + 2.0 * sa * al;
            a1 = -2.0 * ((a - 1.0) + (a + 1.0) * c);
            a2 = (a + 1.0) + (a - 1.0) * c - 2.0 * sa * al;
        }
        Shape::HighShelf => {
            b0 = a * ((a + 1.0) + (a - 1.0) * c + 2.0 * sa * al);
            b1 = -2.0 * a * ((a - 1.0) + (a + 1.0) * c);
            b2 = a * ((a + 1.0) + (a - 1.0) * c - 2.0 * sa * al);
            a0 = (a + 1.0) - (a - 1.0) * c + 2.0 * sa * al;
            a1 = 2.0 * ((a - 1.0) - (a + 1.0) * c);
            a2 = (a + 1.0) - (a - 1.0) * c - 2.0 * sa * al;
        }
    }
    Coeffs { b0: b0 / a0, b1: b1 / a0, b2: b2 / a0, a1: a1 / a0, a2: a2 / a0 }
}

pub fn is_stable(c: &Coeffs) -> bool {
    c.a1.is_finite() && c.a2.is_finite() && c.a2.abs() < 1.0 && c.a1.abs() < 1.0 + c.a2
}

/// Analog-matched starting point: poles by matched-z, numerator magnitude-matched at DC, a middle
/// frequency and Nyquist.
fn matched_start(s: &Spec, fs: f64) -> Coeffs {
    let a = 10f64.powf(s.gain_db / 40.0);
    let sa = a.sqrt();
    let (mut d2, mut d1, mut d0) = (1.0, 1.0 / s.q, 1.0);
    match s.shape {
        Shape::Peak => d1 = 1.0 / (a * s.q),
        Shape::LowShelf => {
            d2 = a;
            d1 = sa / s.q;
        }
        Shape::HighShelf => {
            d1 = sa / s.q;
            d0 = a;
        }
        _ => {}
    }
    let disc = C64::sqrt_real(d1 * d1 - 4.0 * d2 * d0);
    let x1 = C64::new(-d1, 0.0).add(disc).scale(1.0 / (2.0 * d2));
    let x2 = C64::new(-d1, 0.0).sub(disc).scale(1.0 / (2.0 * d2));
    let k = 2.0 * PI * s.f0.min(0.49 * fs) / fs;
    let z1 = x1.scale(k).exp();
    let z2 = x2.scale(k).exp();
    let a1 = -(z1.add(z2)).re;
    let a2 = z1.mul(z2).re;
    let fm = if s.f0 <= 0.46 * fs { s.f0 } else { 0.3 * fs };
    let wm = 2.0 * PI * fm / fs;
    let d = |w: f64| {
        let e = C64::new(0.0, -w).exp();
        C64::new(1.0, 0.0).add(e.scale(a1)).add(e.mul(e).scale(a2)).norm()
    };
    let t0 = analog_response(s, 1.0e-3).norm() * d(0.0);
    let t1 = analog_response(s, 0.5 * fs).norm() * d(PI);
    let tm = analog_response(s, fm).norm() * d(wm);
    let ph0 = (wm / 2.0).cos() * (wm / 2.0).cos();
    let ph1 = (wm / 2.0).sin() * (wm / 2.0).sin();
    let ph2 = wm.sin() * wm.sin();
    let a2n = (tm - t0 * ph0 - t1 * ph1) / ph2;
    let s0 = t0.max(0.0).sqrt();
    let s1 = t1.max(0.0).sqrt();
    let w = (s0 + s1) / 2.0;
    let r = (w * w + a2n).max(0.0).sqrt();
    Coeffs { b0: (w + r) / 2.0, b1: (s0 - s1) / 2.0, b2: (w - r) / 2.0, a1, a2 }
}

/// Mirror any numerator zero outside the unit circle back inside (magnitude unchanged), so
/// coefficients vary continuously while a control is swept.
fn make_minimum_phase(mut c: Coeffs) -> Coeffs {
    if c.b0.abs() < 1.0e-12 {
        return c;
    }
    let p = c.b1 / c.b0;
    let q = c.b2 / c.b0;
    let disc = C64::sqrt_real(p * p - 4.0 * q);
    let mut r1 = C64::new(-p, 0.0).add(disc).scale(0.5);
    let mut r2 = C64::new(-p, 0.0).sub(disc).scale(0.5);
    let mut gain = c.b0;
    for r in [&mut r1, &mut r2] {
        if r.abs() > 1.0 {
            gain *= r.abs();
            *r = r.conj().recip();
        }
    }
    // A magnitude fit cannot see polarity either, and the b0/b2 direction is ill-conditioned: 1.6.x could
    // land on a negated numerator and invert the signal. Every prototype here has positive gain.
    let gain = gain.abs();
    c.b0 = gain;
    c.b1 = -gain * r1.add(r2).re;
    c.b2 = gain * r1.mul(r2).re;
    c
}

const LOG_POINTS: usize = 64;
const LIN_POINTS: usize = 32;
const NEAR_POINTS: usize = 24;
const GRID: usize = LOG_POINTS + LIN_POINTS + NEAR_POINTS;
/// -60 dB: stopband detail below this is not chased.
const FLOOR: f64 = 1.0e-3;
/// 20 / ln(10)
const DB_PER_NEPER: f64 = 8.685889638065035;

#[derive(Clone)]
struct Grid {
    f: [f64; GRID],
    z1: [C64; GRID],
    target: [f64; GRID],
}

fn make_grid(s: &Spec, fs: f64) -> Grid {
    let mut g = Grid { f: [0.0; GRID], z1: [C64::new(0.0, 0.0); GRID], target: [0.0; GRID] };
    let lo = 20f64.ln();
    let hi = (0.499 * fs).ln();
    for i in 0..GRID {
        let f = if i < LOG_POINTS {
            (lo + (hi - lo) * i as f64 / (LOG_POINTS - 1) as f64).exp()
        } else if i < LOG_POINTS + LIN_POINTS {
            0.1 * fs + (0.499 - 0.1) * fs * (i - LOG_POINTS) as f64 / (LIN_POINTS - 1) as f64
        } else {
            let span = (2.0 / s.q.max(0.5)).min(1.2);
            let u = (i - LOG_POINTS - LIN_POINTS) as f64 / (NEAR_POINTS - 1) as f64 * 2.0 - 1.0;
            (s.f0 * (u * span).exp()).clamp(20.0, 0.499 * fs)
        };
        g.f[i] = f;
        g.z1[i] = C64::new(0.0, -2.0 * PI * f / fs).exp();
        g.target[i] = analog_response(s, f).abs();
    }
    g
}

/// b0, b1, b2, u1, u2 (k = tanh(u), a1 = k1 (1 + k2), a2 = k2)
#[derive(Clone, Copy)]
struct Params {
    v: [f64; 5],
}

fn to_coeffs(p: &Params) -> Coeffs {
    let k1 = p.v[3].tanh();
    let k2 = p.v[4].tanh();
    Coeffs { b0: p.v[0], b1: p.v[1], b2: p.v[2], a1: k1 * (1.0 + k2), a2: k2 }
}

fn to_params(c: &Coeffs) -> Params {
    let k2 = c.a2.clamp(-0.999, 0.999);
    let k1 = (c.a1 / (1.0 + k2)).clamp(-0.999, 0.999);
    Params { v: [c.b0, c.b1, c.b2, k1.atanh(), k2.atanh()] }
}

/// Residual (dB, floored) and optionally its Jacobian. Returns the sum of squares.
fn evaluate(g: &Grid, p: &Params, r: &mut [f64; GRID], jac: Option<&mut [[f64; 5]; GRID]>) -> f64 {
    let c = to_coeffs(p);
    let k1 = p.v[3].tanh();
    let k2 = p.v[4].tanh();
    let da1du1 = (1.0 - k1 * k1) * (1.0 + k2);
    let da1du2 = k1 * (1.0 - k2 * k2);
    let da2du2 = 1.0 - k2 * k2;
    let mut cost = 0.0;
    let mut jac = jac;
    for i in 0..GRID {
        let z = g.z1[i];
        let z2 = z.mul(z);
        let b = C64::new(c.b0, 0.0).add(z.scale(c.b1)).add(z2.scale(c.b2));
        let a = C64::new(1.0, 0.0).add(z.scale(c.a1)).add(z2.scale(c.a2));
        let h = b.abs() / a.abs();
        let ri = DB_PER_NEPER * ((h + FLOOR) / (g.target[i] + FLOOR)).ln();
        r[i] = ri;
        cost += ri * ri;
        if let Some(j) = jac.as_deref_mut() {
            let wgt = DB_PER_NEPER * h / (h + FLOOR);
            let ib = b.recip();
            let ia = a.recip();
            let db0 = ib.re;
            let db1 = z.mul(ib).re;
            let db2 = z2.mul(ib).re;
            let da1 = -z.mul(ia).re;
            let da2 = -z2.mul(ia).re;
            j[i][0] = wgt * db0;
            j[i][1] = wgt * db1;
            j[i][2] = wgt * db2;
            j[i][3] = wgt * da1 * da1du1;
            j[i][4] = wgt * (da1 * da1du2 + da2 * da2du2);
        }
    }
    cost
}

/// Levenberg-Marquardt, 5 parameters. Returns the final cost.
fn levenberg_marquardt(g: &Grid, p: &mut Params, max_iter: usize) -> f64 {
    let mut r = [0.0; GRID];
    let mut r_try = [0.0; GRID];
    let mut jac = [[0.0; 5]; GRID];
    let mut cost = evaluate(g, p, &mut r, Some(&mut jac));
    let mut lambda = 1.0e-3;
    for _ in 0..max_iter {
        let mut jtj = [[0.0; 5]; 5];
        let mut grad = [0.0; 5];
        for i in 0..GRID {
            for a in 0..5 {
                grad[a] += jac[i][a] * r[i];
                for b in 0..5 {
                    jtj[a][b] += jac[i][a] * jac[i][b];
                }
            }
        }
        let mut improved = false;
        let mut attempt = 0;
        while attempt < 8 && !improved {
            attempt += 1;
            let mut m = [[0.0; 6]; 5];
            for a in 0..5 {
                for b in 0..5 {
                    m[a][b] = jtj[a][b] + if a == b { lambda * (jtj[a][a] + 1.0e-9) } else { 0.0 };
                }
                m[a][5] = -grad[a];
            }
            let mut singular = false;
            for c in 0..5 {
                let mut pv = c;
                for rr in c + 1..5 {
                    if m[rr][c].abs() > m[pv][c].abs() {
                        pv = rr;
                    }
                }
                if m[pv][c].abs() < 1.0e-300 {
                    singular = true;
                    break;
                }
                if pv != c {
                    m.swap(c, pv);
                }
                for rr in c + 1..5 {
                    let f = m[rr][c] / m[c][c];
                    for k in c..6 {
                        m[rr][k] -= f * m[c][k];
                    }
                }
            }
            if singular {
                lambda *= 10.0;
                continue;
            }
            let mut delta = [0.0; 5];
            for a in (0..5).rev() {
                let mut s = m[a][5];
                for b in a + 1..5 {
                    s -= m[a][b] * delta[b];
                }
                delta[a] = s / m[a][a];
            }
            let mut pn = *p;
            for a in 0..5 {
                pn.v[a] += delta[a];
            }
            pn.v[3] = pn.v[3].clamp(-6.0, 6.0);
            pn.v[4] = pn.v[4].clamp(-6.0, 6.0);
            let cost_try = evaluate(g, &pn, &mut r_try, None);
            if cost_try.is_finite() && cost_try < cost {
                let rel = (cost - cost_try) / (cost + 1.0e-30);
                *p = pn;
                cost = cost_try;
                lambda = (lambda * 0.3).max(1.0e-9);
                evaluate(g, p, &mut r, Some(&mut jac));
                improved = true;
                if rel < 1.0e-7 {
                    return cost;
                }
            } else {
                lambda *= 5.0;
            }
        }
        if !improved {
            break;
        }
    }
    cost
}

fn stage_mag(c: &Coeffs, z: C64) -> f64 {
    let z2 = z.mul(z);
    let num = C64::new(c.b0, 0.0).add(z.scale(c.b1)).add(z2.scale(c.b2));
    let den = C64::new(1.0, 0.0).add(z.scale(c.a1)).add(z2.scale(c.a2));
    num.abs() / den.abs()
}

fn max_abs_residual(g: &Grid, c: &Coeffs) -> f64 {
    let mut worst = 0.0f64;
    for i in 0..GRID {
        let h = stage_mag(c, g.z1[i]);
        worst = worst.max((DB_PER_NEPER * ((h + FLOOR) / (g.target[i] + FLOOR)).ln()).abs());
    }
    worst
}

/// Result of a design. `fitted` is false when the cookbook fallback was used.
#[derive(Debug, Clone, Copy)]
pub struct Fit {
    pub c: Coeffs,
    pub fitted: bool,
    pub max_error_db: f64,
}

/// Fits the biquad to the analog prototype. `warm` (the previous coefficients while a control is being
/// swept) makes the solve converge in a few iterations and keeps coefficients continuous.
pub fn design(spec: &Spec, fs: f64, warm: Option<&Coeffs>) -> Fit {
    let gain_shape = matches!(spec.shape, Shape::Peak | Shape::LowShelf | Shape::HighShelf);
    if gain_shape && spec.gain_db.abs() < 0.005 {
        return Fit { c: Coeffs::BYPASS, fitted: true, max_error_db: 0.0 };
    }
    let base = cookbook(spec, fs);
    if !(fs > 0.0) || !(spec.f0 > 0.0) || !(spec.q > 0.0) || !spec.gain_db.is_finite() {
        return Fit { c: base, fitted: false, max_error_db: 0.0 };
    }
    let g = make_grid(spec, fs);
    let mut best = base;
    let mut best_err = 1.0e30;
    let consider = |start: &Coeffs, iterations: usize, best: &mut Coeffs, best_err: &mut f64| {
        let mut p = to_params(start);
        levenberg_marquardt(&g, &mut p, iterations);
        let out = make_minimum_phase(to_coeffs(&p));
        let e = max_abs_residual(&g, &out);
        if e.is_finite() && is_stable(&out) && e < *best_err {
            *best_err = e;
            *best = out;
        }
    };
    const GOOD: f64 = 0.3;
    if let Some(w) = warm {
        if is_stable(w) {
            consider(w, 40, &mut best, &mut best_err);
            if best_err <= 0.8 {
                return Fit { c: best, fitted: true, max_error_db: best_err };
            }
        }
    }
    if best_err > GOOD {
        consider(&matched_start(spec, fs), 80, &mut best, &mut best_err);
    }
    if best_err > GOOD {
        consider(&base, 80, &mut best, &mut best_err);
    }
    if best_err > GOOD {
        let mut s2 = *spec;
        s2.f0 = spec.f0.min(0.25 * fs);
        consider(&cookbook(&s2, fs), 80, &mut best, &mut best_err);
    }
    if best_err > 1.5 || !best_err.is_finite() {
        return Fit { c: base, fitted: false, max_error_db: max_abs_residual(&g, &base) };
    }
    Fit { c: best, fitted: true, max_error_db: best_err }
}

/// Designs a cascade of biquads whose COMBINED magnitude follows the product of the analog stages.
/// Each stage is fitted alone first, then refined against what the others leave behind. Returns the
/// combined worst error in dB.
pub fn design_cascade(specs: &[Spec], fs: f64, warm: Option<&[Coeffs]>, out: &mut [Coeffs]) -> f64 {
    let n = specs.len();
    for i in 0..n {
        out[i] = design(&specs[i], fs, warm.map(|w| &w[i])).c;
    }
    let g = make_grid(&specs[0], fs);
    let mut total = [0.0; GRID];
    for k in 0..GRID {
        let mut t = 1.0;
        for s in specs {
            t *= analog_response(s, g.f[k]).abs();
        }
        total[k] = t;
    }
    let combined_error = |out: &[Coeffs]| {
        let mut worst = 0.0f64;
        for k in 0..GRID {
            let mut m = 1.0;
            for c in out.iter().take(n) {
                m *= stage_mag(c, g.z1[k]);
            }
            worst = worst.max((DB_PER_NEPER * ((m + FLOOR) / (total[k] + FLOOR)).ln()).abs());
        }
        worst
    };
    let mut best = combined_error(out);
    if n < 2 {
        return best;
    }
    let mut pass = 0;
    while pass < 4 && best > 0.05 {
        pass += 1;
        for i in 0..n {
            let mut gi = g.clone();
            for k in 0..GRID {
                let mut others = 1.0;
                for (j, c) in out.iter().enumerate().take(n) {
                    if j != i {
                        others *= stage_mag(c, g.z1[k]);
                    }
                }
                gi.target[k] = (total[k] / others.max(1.0e-9)).min(1.0e3);
            }
            let mut p = to_params(&out[i]);
            levenberg_marquardt(&gi, &mut p, 30);
            let c = make_minimum_phase(to_coeffs(&p));
            if !is_stable(&c) {
                continue;
            }
            let old = out[i];
            out[i] = c;
            let e = combined_error(out);
            if e < best {
                best = e;
            } else {
                out[i] = old;
            }
        }
    }
    best
}

#[cfg(test)]
mod tests {
    use super::*;

    fn worst_vs_analog(spec: &Spec, fs: f64, c: &Coeffs) -> f64 {
        let mut worst = 0.0f64;
        let mut f = 20.0;
        while f < 0.45 * fs {
            let h = c.magnitude_sq(f / fs).sqrt();
            let t = analog_response_mag(spec, f);
            if t > 1e-3 {
                worst = worst.max((20.0 * (h / t).log10()).abs());
            }
            f *= 1.05;
        }
        worst
    }

    #[test]
    fn high_shelf_near_nyquist_follows_the_analog_curve() {
        let fs = 48000.0;
        let spec = Spec { shape: Shape::HighShelf, f0: 16000.0, q: 0.71, gain_db: 9.0 };
        let fit = design(&spec, fs, None);
        assert!(fit.fitted && is_stable(&fit.c));
        assert!(worst_vs_analog(&spec, fs, &fit.c) < 0.6, "{}", worst_vs_analog(&spec, fs, &fit.c));
        let cook = cookbook(&spec, fs);
        assert!(worst_vs_analog(&spec, fs, &cook) > 1.0);
    }

    #[test]
    fn bell_above_nyquist_is_stable() {
        for f0 in [5000.0, 12000.0, 20000.0, 30000.0] {
            for g in [-18.0, -6.0, 6.0, 18.0] {
                let spec = Spec { shape: Shape::Peak, f0, q: 1.0, gain_db: g };
                let fit = design(&spec, 44100.0, None);
                assert!(is_stable(&fit.c), "{f0} {g}");
            }
        }
    }

    #[test]
    fn fitted_shelf_keeps_positive_polarity() {
        // 1.6.16 inverted the signal here: FSF high shelf at 20 kHz, designed at 44.1 kHz, then warm-started at 48 kHz.
        let spec = Spec { shape: Shape::HighShelf, f0: 20000.0, q: 0.85, gain_db: 10.0 };
        let first = design(&spec, 44100.0, None);
        let second = design(&spec, 48000.0, Some(&first.c));
        assert!(first.c.b0 > 0.0 && second.c.b0 > 0.0);
        assert!(second.max_error_db < 0.5);
    }

    #[test]
    fn flat_gain_is_bypass() {
        let spec = Spec { shape: Shape::Peak, f0: 15000.0, q: 1.0, gain_db: 0.0 };
        assert_eq!(design(&spec, 48000.0, None).c, Coeffs::BYPASS);
    }
}
