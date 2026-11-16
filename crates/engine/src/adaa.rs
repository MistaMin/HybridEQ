//! First-order antiderivative anti-aliasing (ADAA) for the static preamp models.
//!
//! A memoryless nonlinearity y = f(x) creates harmonics that fold back as aliasing. First-order ADAA
//! replaces f(x_n) with the mean of f over the straight line from x_{n-1} to x_n,
//! (F(x_n) - F(x_{n-1})) / (x_n - x_{n-1}). See [`Residual`] for how it stays transparent on clean
//! signals.

use std::sync::OnceLock;

/// ln(cosh(x)), the antiderivative of tanh, without overflow.
#[inline]
pub fn log_cosh(x: f64) -> f64 {
    let a = x.abs();
    a + (-2.0 * a).exp().ln_1p() - std::f64::consts::LN_2
}

/// Antiderivative of the core saturator f(v) = v / sqrt(1 + (v/sat)^2).
#[inline]
pub fn core_saturate_f(v: f64, sat: f64) -> f64 {
    let r = v / sat;
    sat * sat * (1.0 + r * r).sqrt()
}

const RAIL_SIZE: usize = 4096;
const RAIL_MAX: f64 = 8.0;
const RAIL_STEP: f64 = RAIL_MAX / RAIL_SIZE as f64;

/// G(t) = integral of t / (1 + t^8)^(1/8) from 0 to t, tabulated and read back with cubic Hermite
/// interpolation (the slope is known exactly), accurate to about 1e-11.
pub struct RailTable {
    g: Box<[f64; RAIL_SIZE + 1]>,
    f: Box<[f64; RAIL_SIZE + 1]>,
}

impl RailTable {
    fn new() -> Self {
        let mut g = Box::new([0.0; RAIL_SIZE + 1]);
        let mut f = Box::new([0.0; RAIL_SIZE + 1]);
        for (i, v) in f.iter_mut().enumerate() {
            *v = Self::value(i as f64 * RAIL_STEP);
        }
        for i in 0..RAIL_SIZE {
            let mut acc = 0.0;
            let t0 = i as f64 * RAIL_STEP;
            let h = RAIL_STEP / 16.0;
            for k in 0..16 {
                let a = t0 + k as f64 * h;
                let m = a + 0.5 * h;
                let b = a + h;
                acc += h / 6.0 * (Self::value(a) + 4.0 * Self::value(m) + Self::value(b));
            }
            g[i + 1] = g[i] + acc;
        }
        Self { g, f }
    }

    #[inline]
    pub fn value(t: f64) -> f64 {
        t / (1.0 + t.powf(8.0)).powf(0.125)
    }

    /// Integral of [`Self::value`] from 0 to t, t >= 0.
    #[inline]
    pub fn integral(&self, t: f64) -> f64 {
        if t >= RAIL_MAX {
            return self.g[RAIL_SIZE] + (t - RAIL_MAX);
        }
        let u = t / RAIL_STEP;
        let i = (u as usize).min(RAIL_SIZE - 1);
        let s = u - i as f64;
        let s2 = s * s;
        let s3 = s2 * s;
        let (g0, g1, f0, f1) = (self.g[i], self.g[i + 1], self.f[i], self.f[i + 1]);
        (2.0 * s3 - 3.0 * s2 + 1.0) * g0 + (s3 - 2.0 * s2 + s) * RAIL_STEP * f0 + (-2.0 * s3 + 3.0 * s2) * g1 + (s3 - s2) * RAIL_STEP * f1
    }
}

/// The shared table, built on first use (`prepare` touches it so the audio thread never builds it).
pub fn rail_table() -> &'static RailTable {
    static TABLE: OnceLock<RailTable> = OnceLock::new();
    TABLE.get_or_init(RailTable::new)
}

/// Antiderivative of `rail_limit(v, rail)`.
#[inline]
pub fn rail_limit_f(v: f64, rail: f64) -> f64 {
    rail * rail * rail_table().integral(v.abs() / rail)
}

/// Antiderivative of `asym_limit(v, pos, neg)`.
#[inline]
pub fn asym_limit_f(v: f64, pos_rail: f64, neg_rail: f64) -> f64 {
    rail_limit_f(v, if v >= 0.0 { pos_rail } else { neg_rail })
}

/// Transistor leg: v = w + kappa (e^w - 1), I = e^w; the integral of I dv is I + kappa I^2 / 2.
#[inline]
pub fn leg_f(i: f64, kappa: f64) -> f64 {
    i + 0.5 * kappa * i * i
}

/// Off returns f(x) untouched; Hold keeps timing without the antiderivative (gain moving); On is the
/// full method.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Mode {
    Off,
    Hold,
    On,
}

/// One memoryless stage y = f(x) with antiderivative F and small-signal slope `slope`.
///
/// y = slope * x + r(x). ADAA is applied to the residual r only; the linear part goes through a
/// first-order all-pass with the same half-sample delay at low frequencies (unity magnitude). As the
/// stage compresses, a weight fades the linear part to the plain two-sample average, so deep
/// saturation gives exactly the plain ADAA mean.
#[derive(Debug, Clone, Copy, Default)]
pub struct Residual {
    x1: f64,
    f1: f64,
    r1: f64,
    apx: f64,
    apy: f64,
    primed: bool,
}

const KNEE: f64 = 0.3;

impl Residual {
    #[inline]
    pub fn reset(&mut self) {
        self.primed = false;
    }

    #[inline]
    pub fn step(&mut self, x: f64, f: f64, big_f: f64, slope: f64, mode: Mode) -> f64 {
        if mode == Mode::Off {
            self.primed = false;
            return f;
        }
        let lin_in = slope * x;
        if !self.primed {
            self.apx = lin_in;
            self.apy = lin_in;
        }
        let lin = (1.0 / 3.0) * (lin_in - self.apy) + self.apx;
        self.apx = lin_in;
        self.apy = lin;

        let r = f - lin_in;
        let fr = big_f - 0.5 * slope * x * x;
        let mut result = lin + r;
        if mode == Mode::On && self.primed {
            let dx = x - self.x1;
            if dx.abs() > 1.0e-5 {
                let mut mean_r = (fr - self.f1) / dx;
                let lo = r.min(self.r1);
                let hi = r.max(self.r1);
                let tol = (hi - lo) + 1.0e-6 * (1.0 + r.abs());
                if !(mean_r >= lo - tol && mean_r <= hi + tol) {
                    mean_r = r;
                }
                let c = if x.abs() > 1.0e-9 { 1.0 - f / lin_in } else { 0.0 };
                let c2 = c * c;
                let c4 = c2 * c2;
                let c8 = c4 * c4;
                let k2 = KNEE * KNEE;
                let k4 = k2 * k2;
                let w = c8 / (c8 + k4 * k4);
                result = lin + mean_r + w * (0.5 * slope * (x + self.x1) - lin);
            }
        }
        self.x1 = x;
        self.f1 = fr;
        self.r1 = r;
        self.primed = true;
        result
    }
}

/// A warped knot grid with interpolation linear in input volts; its primitive integrates that exact
/// interpolant. Immutable after `prepare`.
pub struct IntegratedCurve {
    knots: Box<[f64; CURVE_SIZE]>,
    values: Box<[f64; CURVE_SIZE]>,
    integrals: Box<[f64; CURVE_SIZE]>,
    slopes: Box<[f64; CURVE_SIZE - 1]>,
    limit: f64,
    warp_limit: f64,
    scale: f64,
}

pub const CURVE_SIZE: usize = 2049;
const CURVE_CENTER: usize = (CURVE_SIZE - 1) / 2;

#[derive(Debug, Clone, Copy, Default)]
pub struct Evaluation {
    pub value: f64,
    pub integral: f64,
    pub segment: i32,
}

impl IntegratedCurve {
    pub fn new(extent: f64, function: impl Fn(f64) -> f64) -> Self {
        let mut c = Self {
            knots: Box::new([0.0; CURVE_SIZE]),
            values: Box::new([0.0; CURVE_SIZE]),
            integrals: Box::new([0.0; CURVE_SIZE]),
            slopes: Box::new([0.0; CURVE_SIZE - 1]),
            limit: extent,
            warp_limit: extent / (1.0 + extent),
            scale: 1.0,
        };
        c.scale = (CURVE_SIZE - 1) as f64 / (2.0 * c.warp_limit);
        for i in 0..CURVE_SIZE {
            let u = (i as f64 - CURVE_CENTER as f64) / c.scale;
            c.knots[i] = u / (1.0 - u.abs());
            c.values[i] = function(c.knots[i]);
        }
        c.knots[0] = -extent;
        c.knots[CURVE_SIZE - 1] = extent;
        for i in 0..CURVE_SIZE - 1 {
            c.slopes[i] = (c.values[i + 1] - c.values[i]) / (c.knots[i + 1] - c.knots[i]);
        }
        c.integrals[CURVE_CENTER] = 0.0;
        for i in CURVE_CENTER + 1..CURVE_SIZE {
            c.integrals[i] = c.integrals[i - 1] + 0.5 * (c.values[i - 1] + c.values[i]) * (c.knots[i] - c.knots[i - 1]);
        }
        for i in (0..CURVE_CENTER).rev() {
            c.integrals[i] = c.integrals[i + 1] - 0.5 * (c.values[i] + c.values[i + 1]) * (c.knots[i + 1] - c.knots[i]);
        }
        c
    }

    #[inline]
    fn interval(&self, x: f64) -> usize {
        let u = x / (1.0 + x.abs());
        (((u + self.warp_limit) * self.scale) as i64).clamp(0, CURVE_SIZE as i64 - 2) as usize
    }

    #[inline]
    pub fn value(&self, x: f64) -> f64 {
        if x <= -self.limit {
            return self.values[0];
        }
        if x >= self.limit {
            return self.values[CURVE_SIZE - 1];
        }
        let i = self.interval(x);
        self.values[i] + (x - self.knots[i]) * self.slopes[i]
    }

    #[inline]
    pub fn evaluate(&self, x: f64) -> Evaluation {
        if x <= -self.limit {
            return Evaluation { value: self.values[0], integral: self.integrals[0] + (x + self.limit) * self.values[0], segment: -1 };
        }
        if x >= self.limit {
            let last = CURVE_SIZE - 1;
            return Evaluation { value: self.values[last], integral: self.integrals[last] + (x - self.limit) * self.values[last], segment: last as i32 };
        }
        let i = self.interval(x);
        let dx = x - self.knots[i];
        Evaluation { value: self.values[i] + dx * self.slopes[i], integral: self.integrals[i] + dx * (self.values[i] + 0.5 * dx * self.slopes[i]), segment: i as i32 }
    }

    pub fn lower_safety_bound(&self) -> f64 {
        4.0 * self.values[0]
    }

    pub fn upper_safety_bound(&self) -> f64 {
        4.0 * self.values[CURVE_SIZE - 1]
    }
}

/// Residual ADAA over an [`IntegratedCurve`], with the identity path left unaveraged.
#[derive(Debug, Clone, Copy, Default)]
pub struct CurveAdaa {
    initialized: bool,
    previous: f64,
    cached: Evaluation,
}

impl CurveAdaa {
    #[inline]
    pub fn reset(&mut self) {
        self.initialized = false;
        self.previous = 0.0;
    }

    #[inline]
    pub fn process(&mut self, input: f64, curve: &IntegratedCurve) -> f64 {
        let current = curve.evaluate(input);
        if !self.initialized {
            self.previous = input;
            self.cached = current;
            self.initialized = true;
            return current.value;
        }
        let old = self.previous;
        let prior = self.cached;
        self.previous = input;
        self.cached = current;
        let delta = input - old;
        let average = if current.segment == prior.segment {
            0.5 * current.value + 0.5 * prior.value
        } else if delta.abs() < 1e-5 * 1f64.max(input.abs()).max(old.abs()) {
            curve.value(0.5 * input + 0.5 * old)
        } else {
            (current.integral - prior.integral) / delta
        };
        (average + 0.5 * delta).clamp(curve.lower_safety_bound(), curve.upper_safety_bound())
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn rail_integral_matches_quadrature() {
        let t = rail_table();
        for k in 0..80 {
            let x = k as f64 * 0.1;
            let n = 20000;
            let h = x / n as f64;
            let mut s = 0.0;
            for i in 0..n {
                let a = i as f64 * h;
                s += h / 6.0 * (RailTable::value(a) + 4.0 * RailTable::value(a + 0.5 * h) + RailTable::value(a + h));
            }
            assert!((t.integral(x) - s).abs() < 1e-9, "{x}");
        }
    }

    #[test]
    fn residual_is_transparent_for_linear_stages() {
        let mut r = Residual::default();
        let mut out = Vec::new();
        for i in 0..2000 {
            let x = 0.01 * (i as f64 * 0.3).sin();
            out.push(r.step(x, x, 0.5 * x * x, 1.0, Mode::On));
        }
        // all-pass: energy kept
        let ein: f64 = (1000..2000).map(|i| (0.01 * (i as f64 * 0.3).sin()).powi(2)).sum();
        let eout: f64 = out[1000..].iter().map(|v| v * v).sum();
        assert!((eout / ein - 1.0).abs() < 0.01);
    }
}
