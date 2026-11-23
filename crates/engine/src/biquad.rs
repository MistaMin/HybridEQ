//! Second-order sections and the cookbook designs the EQ bands use.

use std::f64::consts::PI;

#[derive(Debug, Clone, Copy, PartialEq)]
pub struct Coeffs {
    pub b0: f64,
    pub b1: f64,
    pub b2: f64,
    pub a1: f64,
    pub a2: f64,
}

impl Default for Coeffs {
    fn default() -> Self {
        Self::BYPASS
    }
}

impl Coeffs {
    pub const BYPASS: Coeffs = Coeffs { b0: 1.0, b1: 0.0, b2: 0.0, a1: 0.0, a2: 0.0 };

    /// Squared magnitude at `norm_freq`, cheaper than [`Self::response`].
    pub fn magnitude_sq(&self, norm_freq: f64) -> f64 {
        let w = 2.0 * PI * norm_freq;
        let (s1, c1) = w.sin_cos();
        let (s2, c2) = (2.0 * w).sin_cos();
        let nr = self.b0 + self.b1 * c1 + self.b2 * c2;
        let ni = self.b1 * s1 + self.b2 * s2;
        let dr = 1.0 + self.a1 * c1 + self.a2 * c2;
        let di = self.a1 * s1 + self.a2 * s2;
        (nr * nr + ni * ni) / (dr * dr + di * di)
    }
}

/// Transposed direct form II, double-precision state.
#[derive(Debug, Clone, Copy, Default)]
pub struct Biquad {
    pub c: Coeffs,
    z1: f64,
    z2: f64,
}

impl Biquad {
    #[inline]
    pub fn reset(&mut self) {
        self.z1 = 0.0;
        self.z2 = 0.0;
    }

    #[inline(always)]
    pub fn tick(&mut self, x: f64) -> f64 {
        let c = &self.c;
        let y = c.b0 * x + self.z1;
        self.z1 = c.b1 * x - c.a1 * y + self.z2;
        self.z2 = c.b2 * x - c.a2 * y;
        y
    }

    /// One sample through, rounded to `f32` as the 1.6.x section did.
    #[inline(always)]
    pub fn process(&mut self, x: f32) -> f32 {
        self.tick(x as f64) as f32
    }
}

fn cookbook_lp(w0: f64, q: f64) -> Coeffs {
    let alpha = w0.sin() / (2.0 * q);
    let c = w0.cos();
    let a0 = 1.0 + alpha;
    Coeffs { b0: (1.0 - c) / 2.0 / a0, b1: (1.0 - c) / a0, b2: (1.0 - c) / 2.0 / a0, a1: -2.0 * c / a0, a2: (1.0 - alpha) / a0 }
}

fn cookbook_hp(w0: f64, q: f64) -> Coeffs {
    let alpha = w0.sin() / (2.0 * q);
    let c = w0.cos();
    let a0 = 1.0 + alpha;
    Coeffs { b0: (1.0 + c) / 2.0 / a0, b1: -(1.0 + c) / a0, b2: (1.0 + c) / 2.0 / a0, a1: -2.0 * c / a0, a2: (1.0 - alpha) / a0 }
}

pub fn low_pass(sr: f64, freq: f64, q: f64) -> Coeffs {
    cookbook_lp(2.0 * PI * freq / sr, q)
}

pub fn high_pass(sr: f64, freq: f64, q: f64) -> Coeffs {
    cookbook_hp(2.0 * PI * freq / sr, q)
}

pub fn peak(sr: f64, freq: f64, q: f64, gain_db: f64) -> Coeffs {
    let a = 10f64.powf(gain_db / 40.0);
    let w0 = 2.0 * PI * freq / sr;
    let alpha = w0.sin() / (2.0 * q);
    let c = w0.cos();
    let a0 = 1.0 + alpha / a;
    Coeffs { b0: (1.0 + alpha * a) / a0, b1: -2.0 * c / a0, b2: (1.0 - alpha * a) / a0, a1: -2.0 * c / a0, a2: (1.0 - alpha / a) / a0 }
}

pub fn low_shelf(sr: f64, freq: f64, q: f64, gain_db: f64) -> Coeffs {
    let a = 10f64.powf(gain_db / 40.0);
    let w0 = 2.0 * PI * freq / sr;
    let alpha = w0.sin() / (2.0 * q);
    let c = w0.cos();
    let sa = a.sqrt();
    let a0 = (a + 1.0) + (a - 1.0) * c + 2.0 * sa * alpha;
    Coeffs {
        b0: (a * ((a + 1.0) - (a - 1.0) * c + 2.0 * sa * alpha)) / a0,
        b1: (2.0 * a * ((a - 1.0) - (a + 1.0) * c)) / a0,
        b2: (a * ((a + 1.0) - (a - 1.0) * c - 2.0 * sa * alpha)) / a0,
        a1: (-2.0 * ((a - 1.0) + (a + 1.0) * c)) / a0,
        a2: ((a + 1.0) + (a - 1.0) * c - 2.0 * sa * alpha) / a0,
    }
}

pub fn high_shelf(sr: f64, freq: f64, q: f64, gain_db: f64) -> Coeffs {
    let a = 10f64.powf(gain_db / 40.0);
    let w0 = 2.0 * PI * freq / sr;
    let alpha = w0.sin() / (2.0 * q);
    let c = w0.cos();
    let sa = a.sqrt();
    let a0 = (a + 1.0) - (a - 1.0) * c + 2.0 * sa * alpha;
    Coeffs {
        b0: (a * ((a + 1.0) + (a - 1.0) * c + 2.0 * sa * alpha)) / a0,
        b1: (-2.0 * a * ((a - 1.0) + (a + 1.0) * c)) / a0,
        b2: (a * ((a + 1.0) + (a - 1.0) * c - 2.0 * sa * alpha)) / a0,
        a1: (2.0 * ((a - 1.0) - (a + 1.0) * c)) / a0,
        a2: ((a + 1.0) - (a - 1.0) * c - 2.0 * sa * alpha) / a0,
    }
}

/// Numerical safety net only (a bilinear design at or above Nyquist is unstable): corners above
/// about 10 % of the rate are fitted to the analog curve instead (see `fit`).
pub fn safe_freq(sr: f64, freq: f64) -> f64 {
    freq.min(sr * 0.49)
}

/// N-EQ: proportional Q, widening with gain.
pub fn n_type_q(q: f64, gain_db: f64) -> f64 {
    (q + gain_db.abs() * 0.15).clamp(0.3, 12.0)
}

pub const BAX_SHELF_Q: f64 = 0.5;
pub const BRIT_SHELF_Q: f64 = 0.71;
pub const FSF_SHELF_Q: f64 = 0.85;
pub const BRIT_BELL_Q_SCALE: f64 = 1.2;
pub const A_TYPE_BELL_Q: f64 = 0.9;

/// Butterworth-style stage Q for stage `i` of an `order`-section cut, scaled by the user's Q
/// (a single section takes the user's Q directly), floored at 0.1.
pub fn cut_stage_q(order: usize, i: usize, q: f64) -> f64 {
    let mut stage_q = q;
    if order > 1 {
        stage_q = 1.0 / (2.0 * (PI * (2.0 * i as f64 + 1.0) / (2.0 * order as f64)).sin());
    }
    (stage_q * q / 0.7071).max(0.1)
}
