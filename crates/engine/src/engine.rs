//! The HybridEQ signal path at the effective (possibly oversampled) rate: preamp (static, reduced
//! Circuit or HQ netlist) -> six-band EQ -> harmonics. Oversampling, output gain, Autogain and Delta
//! are applied around it by the plugin.

use crate::eq::{AnalogEq, BellType, MsMode, ShelfType};
use crate::params::{idx, COUNT, PARAMS};
use crate::preamp::{Preamp, PreampType};
use crate::reduced::{Bank, Kernel};

/// Harmonic saturation 2nd..5th: zero-at-origin polynomials (silence stays silent), then tanh.
#[inline]
fn harmonics(x: f32, h: [f32; 4]) -> f32 {
    let xc = x.clamp(-1.0, 1.0);
    let ax = xc.abs();
    let t2 = xc * ax;
    let t3 = xc * xc * xc;
    let t4 = xc * xc * xc * ax;
    let t5 = xc * xc * xc * xc * xc;
    (x + 6.0 * (h[0] * t2 + h[1] * t3 + h[2] * t4 + h[3] * t5)).tanh()
}

const PAD_GAINS: [f32; 3] = [-20.0, 0.0, 10.0];

pub struct Engine {
    pub eq: AnalogEq,
    preamp: Preamp,
    bank: Option<Bank>,
    kernels: [Kernel; 2],
    reduced_active: bool,
    reduced_kind: usize,
    reduced_factor: usize,
    reduced_drive: Smoothed,
    harmonic: [f32; 4],
    base_rate: f64,
}

/// Multiplicative smoothing over a fixed time, as 1.6.x's reduced-engine drive used.
#[derive(Default, Clone, Copy)]
struct Smoothed {
    current: f32,
    target: f32,
    step: f32,
    countdown: u32,
    steps: u32,
}

impl Smoothed {
    fn reset(&mut self, rate: f64, seconds: f64) {
        self.steps = (rate * seconds).floor() as u32;
        self.current = self.target;
        self.countdown = 0;
    }
    fn set_current_and_target(&mut self, v: f32) {
        self.current = v;
        self.target = v;
        self.countdown = 0;
    }
    fn set_target(&mut self, v: f32) {
        if v == self.target {
            return;
        }
        if self.steps == 0 {
            self.set_current_and_target(v);
            return;
        }
        self.target = v;
        self.countdown = self.steps;
        self.step = ((v.abs().ln() - self.current.abs().ln()) / self.countdown as f32).exp();
    }
    #[inline]
    fn next(&mut self) -> f32 {
        if self.countdown == 0 {
            return self.target;
        }
        self.countdown -= 1;
        if self.countdown > 0 {
            self.current *= self.step;
        } else {
            self.current = self.target;
        }
        self.current
    }
}

impl Default for Engine {
    fn default() -> Self {
        Self {
            eq: AnalogEq::new(),
            preamp: Preamp::default(),
            bank: None,
            kernels: [Kernel::default(); 2],
            reduced_active: false,
            reduced_kind: usize::MAX,
            reduced_factor: 0,
            reduced_drive: Smoothed::default(),
            harmonic: [0.0; 4],
            base_rate: 44100.0,
        }
    }
}

/// Oversampling factor the engine runs at for these parameters: the user's choice, raised to 2x
/// when the HQ circuit is on.
pub fn effective_factor(p: &[f32; COUNT]) -> usize {
    let user = [1, 2, 4, 8][(p[idx::OVERSAMPLE_MODE] as usize).min(3)];
    if p[idx::PREAMP_CIRCUIT] > 0.5 && p[idx::PREAMP_HQ] > 0.5 {
        user.max(2)
    } else {
        user
    }
}

impl Engine {
    /// Allocates everything (netlists, reduced bank, scratch) for `max_block` base-rate samples at up
    /// to 8x. Call `set_params` afterwards.
    pub fn prepare(&mut self, sample_rate: f64, max_block: usize, p: &[f32; COUNT]) {
        // 1.6.x applied the parameters once at the previous base rate (44.1 kHz on first load) before
        // preparing, and the fitted EQ bands warm-start from that design; kept so sessions null.
        self.set_params(p);
        let max_os = max_block * 8;
        self.base_rate = sample_rate;
        self.preamp.prepare(sample_rate, max_os);
        self.bank = Some(Bank::new(sample_rate));
        self.reduced_kind = usize::MAX;
        self.reduced_factor = 0;
        self.reduced_active = false;
        self.eq.prepare(sample_rate, max_os);
    }

    pub fn reset(&mut self) {
        self.preamp.reset();
        self.eq.reset();
        for k in &mut self.kernels {
            k.reset();
        }
    }

    /// Applies parameter values (plain units, table order). Call once per block before `process`.
    pub fn set_params(&mut self, p: &[f32; COUNT]) {
        let on = |i: usize| p[i] > 0.5;
        let choice = |i: usize| p[i] as usize;
        let factor = effective_factor(p);
        let kind = choice(idx::PREAMP_TYPE);
        let preamp_on = !on(idx::PREAMP_BYPASS) && kind != 0;
        let circuit_on = on(idx::PREAMP_CIRCUIT);
        let reduced_on = circuit_on && !on(idx::PREAMP_HQ) && preamp_on && self.bank.is_some();
        let drive_db = p[idx::PREAMP_GAIN] + PAD_GAINS[choice(idx::PREAMP_PAD).min(2)];

        self.preamp.set_type(PreampType::from_index(kind));
        self.preamp.set_drive_db(drive_db);
        self.preamp.set_bypassed(on(idx::PREAMP_BYPASS));
        self.preamp.set_circuit_enabled(circuit_on && !reduced_on);

        // Reduced engine: ADAA is built into its stages at the base rate only. Re-pointing resets it.
        if reduced_on {
            let drive = 10f32.powf(drive_db * 0.05);
            if !self.reduced_active || self.reduced_kind != kind || self.reduced_factor != factor {
                let bank = self.bank.as_ref().unwrap();
                for k in &mut self.kernels {
                    k.configure(bank, kind, factor, factor == 1);
                }
                self.reduced_kind = kind;
                self.reduced_factor = factor;
                self.reduced_drive.reset(self.base_rate * factor as f64, 0.02);
                self.reduced_drive.set_current_and_target(drive);
            }
            self.reduced_drive.set_target(drive);
        }
        self.reduced_active = reduced_on;

        self.harmonic = [p[idx::H2], p[idx::H3], p[idx::H4], p[idx::H5]];

        let sr = self.base_rate * factor as f64;
        self.eq.set_sample_rate(sr);
        self.preamp.set_sample_rate(sr);

        apply_eq(&mut self.eq, p);
    }

    /// Processes up to two channels in place at the effective rate (`factor()` times the base rate).
    pub fn process(&mut self, channels: &mut [&mut [f32]]) {
        let used = channels.len().min(2);
        let channels = &mut channels[..used];
        if channels.is_empty() {
            return;
        }
        // No bank (never prepared for the reduced model): fall back to the full preamp.
        if let (true, Some(bank)) = (self.reduced_active, self.bank.as_ref()) {
            let n = channels[0].len();
            for i in 0..n {
                let g = self.reduced_drive.next();
                for (ch, data) in channels.iter_mut().enumerate() {
                    data[i] = self.kernels[ch].process(bank, data[i] as f64 * g as f64) as f32;
                }
            }
        } else {
            self.preamp.process(channels);
        }
        self.eq.process(channels);
        let h = self.harmonic;
        if h != [0.0; 4] {
            for data in channels.iter_mut() {
                for x in data.iter_mut() {
                    *x = harmonics(*x, h);
                }
            }
        }
    }
}

/// Designs the EQ bands from the parameters at the EQ's current sample rate (the engine per block, the
/// editor for its response curve).
pub fn apply_eq(eq: &mut AnalogEq, p: &[f32; COUNT]) {
    let on = |i: usize| p[i] > 0.5;
    let choice = |i: usize| p[i] as usize;
        let f = |i: usize| p[i] as f64;
        let ms = |i: usize| MsMode::from_index(choice(i));
        eq.update_low_cut(f(idx::LC_FREQ), f(idx::LC_Q), choice(idx::LC_SLOPE).min(3), ms(idx::LC_MODE));
        eq.update_high_cut(f(idx::HC_FREQ), f(idx::HC_Q), choice(idx::HC_SLOPE).min(3), ms(idx::HC_MODE));
        eq.update_low(f(idx::LOW_FREQ), f(idx::LOW_GAIN), ShelfType::from_index(choice(idx::LOW_TYPE)), ms(idx::LOW_MODE));
        eq.update_mid1(f(idx::MID1_FREQ), f(idx::MID1_GAIN), f(idx::MID1_Q), BellType::from_index(choice(idx::MID1_TYPE)), ms(idx::MID1_MODE));
        eq.update_mid2(f(idx::MID2_FREQ), f(idx::MID2_GAIN), f(idx::MID2_Q), BellType::from_index(choice(idx::MID2_TYPE)), ms(idx::MID2_MODE));
        eq.update_high(f(idx::HIGH_FREQ), f(idx::HIGH_GAIN), ShelfType::from_index(choice(idx::HIGH_TYPE)), ms(idx::HIGH_MODE));
        use crate::eq::Band;
        eq.set_bypass(Band::LowCut, on(idx::LC_BYPASS));
        eq.set_bypass(Band::HighCut, on(idx::HC_BYPASS));
        eq.set_bypass(Band::Low, on(idx::LOW_BYPASS));
        eq.set_bypass(Band::Mid1, on(idx::MID1_BYPASS));
        eq.set_bypass(Band::Mid2, on(idx::MID2_BYPASS));
        eq.set_bypass(Band::High, on(idx::HIGH_BYPASS));
}

/// Default parameter values, in table order.
pub fn defaults() -> [f32; COUNT] {
    let mut p = [0.0; COUNT];
    for (v, info) in p.iter_mut().zip(PARAMS.iter()) {
        *v = info.default;
    }
    p
}

/// How long the output keeps ringing after the input stops, in seconds, for the host's tail: twice the
/// time the slowest active EQ section takes to fall 90 dB (10.4 time constants of Q / (pi f)), plus the
/// preamp's coupling and servo poles (measured: under 0.12 s for the static and reduced models, under
/// 2.2 s for the HQ netlists).
pub fn tail_seconds(p: &[f32; COUNT]) -> f64 {
    use crate::biquad::{cut_stage_q, n_type_q, A_TYPE_BELL_Q, BAX_SHELF_Q, BRIT_BELL_Q_SCALE, BRIT_SHELF_Q, FSF_SHELF_Q};
    let on = |i: usize| p[i] < 0.5;
    let shelf_q = |t: usize| [BAX_SHELF_Q, BRIT_SHELF_Q, FSF_SHELF_Q][(p[t] as usize).min(2)];
    let bell_q = |t: usize, q: usize, g: usize| match p[t] as usize {
        1 => p[q] as f64 * BRIT_BELL_Q_SCALE,
        2 => A_TYPE_BELL_Q,
        _ => n_type_q(p[q] as f64, p[g] as f64),
    };
    let mut tau = 0.0f64;
    let mut section = |q: f64, f: f32| tau = tau.max(q / (std::f64::consts::PI * f as f64));
    if on(idx::LC_BYPASS) {
        let order = p[idx::LC_SLOPE] as usize + 1;
        (0..order).for_each(|i| section(cut_stage_q(order, i, p[idx::LC_Q] as f64), p[idx::LC_FREQ]));
    }
    if on(idx::HC_BYPASS) {
        let order = p[idx::HC_SLOPE] as usize + 1;
        (0..order).for_each(|i| section(cut_stage_q(order, i, p[idx::HC_Q] as f64), p[idx::HC_FREQ]));
    }
    if on(idx::LOW_BYPASS) {
        section(shelf_q(idx::LOW_TYPE), p[idx::LOW_FREQ]);
    }
    if on(idx::MID1_BYPASS) {
        section(bell_q(idx::MID1_TYPE, idx::MID1_Q, idx::MID1_GAIN), p[idx::MID1_FREQ]);
    }
    if on(idx::MID2_BYPASS) {
        section(bell_q(idx::MID2_TYPE, idx::MID2_Q, idx::MID2_GAIN), p[idx::MID2_FREQ]);
    }
    if on(idx::HIGH_BYPASS) {
        section(shelf_q(idx::HIGH_TYPE), p[idx::HIGH_FREQ]);
    }
    let preamp = if p[idx::PREAMP_BYPASS] > 0.5 {
        0.0
    } else if p[idx::PREAMP_CIRCUIT] > 0.5 && p[idx::PREAMP_HQ] > 0.5 {
        3.0
    } else {
        0.25
    };
    2.0 * 10.4 * tau + preamp
}
