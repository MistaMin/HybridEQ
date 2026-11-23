//! The preamp: light static models with ADAA (Circuit off) or the full netlist simulations
//! (Circuit HQ), for Brit, N-Type, FSF and A-Type, plus a clean Off. The reduced Circuit engine lives
//! in `reduced` and is driven by `Engine`.

use crate::adaa::{self, Mode, Residual};
use crate::biquad::{self, Biquad};
use crate::circuits::{AType, Brit, Circuit, Fsf, NType};
use std::f64::consts::PI;

/// Off is 0, Brit 1 (default), then N-Type, FSF, A-Type: the stored choice index.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum PreampType {
    Off,
    Brit,
    NType,
    Fsf,
    AType,
}

impl PreampType {
    pub fn from_index(i: usize) -> Self {
        match i {
            0 => PreampType::Off,
            2 => PreampType::NType,
            3 => PreampType::Fsf,
            4 => PreampType::AType,
            _ => PreampType::Brit,
        }
    }
}

const MAX_CH: usize = 2;
/// Internal rate of the netlist simulation.
const CIRCUIT_RATE: f64 = 96000.0;
/// Line EMF at 0 dBFS (0 dBFS at 0 dB drive just reaches the stage's clip point).
const N_FULL_SCALE: f64 = 2.0;
const BRIT_FULL_SCALE: f64 = 1.7;
const FSF_FULL_SCALE: f64 = 0.8;
const A_FULL_SCALE: f64 = 0.6;

/// Anti-alias / anti-image low pass for the decimated circuit: 8th-order Butterworth as four
/// sections, minimum phase, no latency to report.
#[derive(Default, Clone, Copy)]
struct Lp8 {
    b0: [f64; 4],
    b1: [f64; 4],
    b2: [f64; 4],
    a1: [f64; 4],
    a2: [f64; 4],
    z1: [f64; 4],
    z2: [f64; 4],
}

impl Lp8 {
    fn design(&mut self, fs: f64, fc: f64) {
        const Q: [f64; 4] = [0.50979558, 0.60134489, 0.89997622, 2.56291545];
        let w0 = 2.0 * PI * fc.min(0.49 * fs) / fs;
        let (cw, sw) = (w0.cos(), w0.sin());
        for i in 0..4 {
            let al = sw / (2.0 * Q[i]);
            let a0 = 1.0 + al;
            self.b0[i] = (1.0 - cw) * 0.5 / a0;
            self.b1[i] = (1.0 - cw) / a0;
            self.b2[i] = self.b0[i];
            self.a1[i] = -2.0 * cw / a0;
            self.a2[i] = (1.0 - al) / a0;
            self.z1[i] = 0.0;
            self.z2[i] = 0.0;
        }
    }

    #[inline]
    fn process(&mut self, mut x: f64) -> f64 {
        for i in 0..4 {
            let y = self.b0[i] * x + self.z1[i];
            self.z1[i] = self.b1[i] * x - self.a1[i] * y + self.z2[i];
            self.z2[i] = self.b2[i] * x - self.a2[i] * y;
            x = y;
        }
        x
    }
}

/// Decimation state shared by whichever circuit is running.
#[derive(Default, Clone, Copy)]
struct Decim {
    aa_down: Lp8,
    aa_up: Lp8,
    held: f64,
    last_gain: f64,
    phase: usize,
}

pub struct Preamp {
    sr: f64,
    slew_coef: f32,
    drive_state: f32,
    drive_ramp: Vec<f32>,
    kind: PreampType,
    drive_target_db: f32,
    bypassed: bool,
    circuit_on: bool,
    configured: Option<(PreampType, bool)>,
    n_adaa: [[Residual; 4]; MAX_CH],
    b_adaa: [[Residual; 3]; MAX_CH],
    f_adaa: [[Residual; 2]; MAX_CH],
    a_adaa: [Residual; MAX_CH],
    adaa_prev_drive: [f64; MAX_CH],
    hp: [Biquad; MAX_CH],
    lp: [Biquad; MAX_CH],
    n_y1: [f64; MAX_CH],
    n_y2: [f64; MAX_CH],
    n_dc1: [f64; MAX_CH],
    n_dc2: [f64; MAX_CH],
    dc_a: f64,
    n_circ: [NType; MAX_CH],
    b_circ: [Brit; MAX_CH],
    f_circ: [Fsf; MAX_CH],
    a_circ: [AType; MAX_CH],
    n_ok: bool,
    b_ok: bool,
    f_ok: bool,
    a_ok: bool,
    decim: [Decim; MAX_CH],
    n_decim: usize,
    brit: [[f64; 3]; MAX_CH],
    brit_a: [f64; 3],
}

impl Default for Preamp {
    fn default() -> Self {
        Self {
            sr: 44100.0,
            slew_coef: 0.001,
            drive_state: 1.0,
            drive_ramp: Vec::new(),
            kind: PreampType::Brit,
            drive_target_db: 0.0,
            bypassed: false,
            circuit_on: false,
            configured: None,
            n_adaa: Default::default(),
            b_adaa: Default::default(),
            f_adaa: Default::default(),
            a_adaa: Default::default(),
            adaa_prev_drive: [-1.0; MAX_CH],
            hp: Default::default(),
            lp: Default::default(),
            n_y1: [0.0; MAX_CH],
            n_y2: [0.0; MAX_CH],
            n_dc1: [0.0; MAX_CH],
            n_dc2: [0.0; MAX_CH],
            dc_a: 0.0,
            n_circ: Default::default(),
            b_circ: Default::default(),
            f_circ: Default::default(),
            a_circ: Default::default(),
            n_ok: false,
            b_ok: false,
            f_ok: false,
            a_ok: false,
            decim: Default::default(),
            n_decim: 1,
            brit: [[0.0; 3]; MAX_CH],
            brit_a: [0.0; 3],
        }
    }
}

impl Preamp {
    /// Copies in the solved netlists (allocates; off the audio thread) and sizes the drive ramp.
    pub fn prepare(&mut self, sample_rate: f64, max_samples: usize) {
        self.sr = sample_rate;
        self.slew_coef = 1.0 - (-1.0 / (sample_rate * 0.04)).exp() as f32;
        self.update_env_coeffs(sample_rate);
        self.drive_state = 1.0;
        self.configured = None;
        self.drive_ramp = vec![0.0; max_samples];
        let n = netlists();
        (self.n_circ, self.b_circ, self.f_circ, self.a_circ) = (n.n.clone(), n.b.clone(), n.f.clone(), n.a.clone());
        [self.n_ok, self.b_ok, self.f_ok, self.a_ok] = n.ok;
        adaa::rail_table();
    }

    pub fn set_type(&mut self, t: PreampType) {
        self.kind = t;
    }
    pub fn set_drive_db(&mut self, db: f32) {
        self.drive_target_db = db;
    }
    pub fn set_bypassed(&mut self, b: bool) {
        self.bypassed = b;
    }
    pub fn set_circuit_enabled(&mut self, on: bool) {
        self.circuit_on = on;
    }

    /// Transient state only: the drive slew and (via a forced re-configure) the ADAA history and the
    /// coupling filters. No netlist rebuild.
    pub fn reset(&mut self) {
        self.drive_state = 1.0;
        self.configured = None;
    }

    /// The circuit poles follow the rate, so a reconfigure is forced only when it really changes.
    pub fn set_sample_rate(&mut self, sample_rate: f64) {
        if (sample_rate - self.sr).abs() < 1e-6 {
            return;
        }
        self.sr = sample_rate;
        self.slew_coef = 1.0 - (-1.0 / (sample_rate * 0.04)).exp() as f32;
        self.update_env_coeffs(sample_rate);
        self.configured = None;
    }

    fn update_env_coeffs(&mut self, sr: f64) {
        self.dc_a = 1.0 - (-2.0 * PI * 1.5 / sr).exp();
    }

    pub fn process(&mut self, channels: &mut [&mut [f32]]) {
        if self.bypassed || channels.is_empty() {
            return;
        }
        let n = channels[0].len();
        self.prepare_drive_ramp(n);
        let (t, on) = (self.kind, self.circuit_on);
        self.ensure_config(t, on);
        for (ch, data) in channels.iter_mut().enumerate().take(MAX_CH) {
            self.process_channel(data, ch, t, on);
        }
    }

    fn prepare_drive_ramp(&mut self, n: usize) {
        let target = 10f64.powf(self.drive_target_db as f64 * 0.05) as f32;
        for r in &mut self.drive_ramp[..n] {
            self.drive_state += (target - self.drive_state) * self.slew_coef;
            *r = self.drive_state;
        }
    }

    fn process_channel(&mut self, data: &mut [f32], ch: usize, t: PreampType, circuit: bool) {
        if circuit {
            match t {
                PreampType::NType if self.n_ok => return Self::process_circuit(&mut self.n_circ[ch], &mut self.decim[ch], self.n_decim, &self.drive_ramp, data),
                PreampType::Brit if self.b_ok => return Self::process_circuit(&mut self.b_circ[ch], &mut self.decim[ch], self.n_decim, &self.drive_ramp, data),
                PreampType::Fsf if self.f_ok => return Self::process_circuit(&mut self.f_circ[ch], &mut self.decim[ch], self.n_decim, &self.drive_ramp, data),
                PreampType::AType if self.a_ok => return Self::process_circuit(&mut self.a_circ[ch], &mut self.decim[ch], self.n_decim, &self.drive_ramp, data),
                _ => {}
            }
        }
        let n = data.len();
        match t {
            PreampType::Off => {
                // Clean drive only; with Circuit on the transformer band limits still apply.
                if circuit {
                    for i in 0..n {
                        let s = data[i] * self.drive_ramp[i];
                        data[i] = self.lp[ch].process(self.hp[ch].process(s));
                    }
                } else {
                    for i in 0..n {
                        data[i] *= self.drive_ramp[i];
                    }
                }
            }
            PreampType::Fsf => {
                // Transformer-coupled input + output curves, both of unity small-signal slope.
                const IN_W1: f64 = 0.62;
                const IN_K1: f64 = 1.15;
                const IN_W2: f64 = 0.38;
                const IN_K2: f64 = 2.2;
                const IN_NORM: f64 = IN_W1 * IN_K1 + IN_W2 * IN_K2;
                const IN_ASYM: f64 = 0.06;
                const OUT_DRIVE: f64 = 1.35;
                const OUT_ASYM: f64 = 0.08;
                for i in 0..n {
                    let g = self.drive_ramp[i] as f64;
                    let mode = self.adaa_mode(ch, g);
                    let s = data[i] as f64 * g;
                    let st = &mut self.f_adaa[ch];
                    let inp = st[0].step(
                        s,
                        (IN_W1 * (IN_K1 * s).tanh() + IN_W2 * (IN_K2 * s).tanh()) / IN_NORM + IN_ASYM * s * s,
                        (IN_W1 * adaa::log_cosh(IN_K1 * s) / IN_K1 + IN_W2 * adaa::log_cosh(IN_K2 * s) / IN_K2) / IN_NORM + IN_ASYM * s * s * s / 3.0,
                        1.0,
                        mode,
                    );
                    let y = st[1].step(
                        inp,
                        (OUT_DRIVE * inp).tanh() / OUT_DRIVE + OUT_ASYM * inp * inp.abs(),
                        adaa::log_cosh(OUT_DRIVE * inp) / (OUT_DRIVE * OUT_DRIVE) + OUT_ASYM * inp * inp * inp.abs() / 3.0,
                        1.0,
                        mode,
                    );
                    data[i] = if circuit { self.lp[ch].process(self.hp[ch].process(y as f32)) } else { y as f32 };
                }
            }
            PreampType::AType => {
                for i in 0..n {
                    let g = self.drive_ramp[i] as f64;
                    let mode = self.adaa_mode(ch, g);
                    let x = data[i] as f64 * g;
                    data[i] = self.a_adaa[ch].step(
                        x,
                        (1.4 * x).tanh() / 1.4 + 0.03 * x * x.abs(),
                        adaa::log_cosh(1.4 * x) / (1.4 * 1.4) + 0.01 * x * x * x.abs(),
                        1.0,
                        mode,
                    ) as f32;
                }
            }
            PreampType::NType => self.process_n(data, ch),
            PreampType::Brit => self.process_brit(data, ch, circuit),
        }
    }

    /// Brit static model: balanced degenerated-transistor input stage + op-amp stages.
    fn process_brit(&mut self, data: &mut [f32], ch: usize, circuit: bool) {
        const OPEN_GAIN: f64 = 20.0;
        const SWING: f64 = 1.0;
        const V_SCALE: f64 = OPEN_GAIN / SWING;
        const GAIN_SPLIT: f64 = 0.7;
        const LEG_MISMATCH: f64 = 0.0015;
        const RAIL_CLIP: f64 = 1.2;
        for i in 0..data.len() {
            let g = self.drive_ramp[i] as f64;
            let mode = self.adaa_mode(ch, g);
            let pad = if g < 1.0 { g } else { 1.0 };
            let gain = if g < 1.0 { 1.0 } else { g };
            let g1 = gain.powf(GAIN_SPLIT).min(OPEN_GAIN);
            let g2 = gain / g1;
            let kappa = OPEN_GAIN / g1 - 1.0;
            let v = data[i] as f64 * pad * V_SCALE;
            let leg_a = solve_leg(v, kappa);
            let leg_b = solve_leg(-v, kappa);
            let st = &mut self.b_adaa[ch];
            let mut o1 = st[0].step(
                v,
                0.5 * SWING * ((1.0 + LEG_MISMATCH) * leg_a - leg_b - LEG_MISMATCH),
                0.5 * SWING * ((1.0 + LEG_MISMATCH) * adaa::leg_f(leg_a, kappa) + adaa::leg_f(leg_b, kappa) - LEG_MISMATCH * v),
                0.5 * SWING * (2.0 + LEG_MISMATCH) / (1.0 + kappa),
                mode,
            );
            o1 = st[1].step(o1, rail_limit(o1, RAIL_CLIP), adaa::rail_limit_f(o1, RAIL_CLIP), 1.0, mode);
            let b = &mut self.brit[ch];
            if circuit {
                b[0] += self.brit_a[0] * (o1 - b[0]);
                o1 -= b[0];
            }
            let in2 = g2 * o1;
            let mut o2 = st[2].step(in2, rail_limit(in2, RAIL_CLIP), adaa::rail_limit_f(in2, RAIL_CLIP), 1.0, mode);
            if circuit {
                b[1] += self.brit_a[1] * (o2 - b[1]);
                o2 -= b[1];
                b[2] += self.brit_a[2] * (o2 - b[2]);
                o2 -= b[2];
            }
            data[i] = o2 as f32;
        }
    }

    /// N-Type static model (the netlist simulation is `process_circuit`).
    fn process_n(&mut self, data: &mut [f32], ch: usize) {
        const V_S: f64 = 20.0;
        const A0_GAIN: f64 = 300.0;
        const K_GAIN: f64 = 0.5;
        const A0_DRV: f64 = 60.0;
        const K_DRV: f64 = 2.0;
        const AEFF_GAIN: f64 = A0_GAIN / (1.0 + K_GAIN);
        const AEFF_DRV: f64 = A0_DRV / (1.0 + K_DRV);
        const BETA_DRV: f64 = 1.0 - 1.0 / AEFF_DRV;
        for i in 0..data.len() {
            let g = self.drive_ramp[i] as f64;
            let mode = self.adaa_mode(ch, g);
            let pad = if g < 1.0 { g } else { 1.0 };
            let gain = (if g < 1.0 { 1.0 } else { g }).min(0.9 * AEFF_GAIN);
            let beta_gain = 1.0 / gain - 1.0 / AEFF_GAIN;
            let x = data[i] as f64 * pad;
            let st = &mut self.n_adaa[ch];
            let v = st[0].step(x, core_saturate(x, 2.0), adaa::core_saturate_f(x, 2.0), 1.0, mode);
            let mut y1 = solve_loop(v, beta_gain, A0_GAIN, K_GAIN, V_S, &mut self.n_y1[ch]);
            y1 = st[1].step(y1, asym_limit(y1, 1.5, 1.1), adaa::asym_limit_f(y1, 1.5, 1.1), 1.0, mode);
            self.n_dc1[ch] += self.dc_a * (y1 - self.n_dc1[ch]);
            y1 -= self.n_dc1[ch];
            let mut y2 = solve_loop(y1, BETA_DRV, A0_DRV, K_DRV, V_S, &mut self.n_y2[ch]);
            y2 = st[2].step(y2, asym_limit(y2, 1.6, 1.3), adaa::asym_limit_f(y2, 1.6, 1.3), 1.0, mode);
            self.n_dc2[ch] += self.dc_a * (y2 - self.n_dc2[ch]);
            y2 -= self.n_dc2[ch];
            data[i] = st[3].step(y2, core_saturate(y2, 2.5), adaa::core_saturate_f(y2, 2.5), 1.0, mode) as f32;
        }
    }

    /// Full netlist simulation: anti-alias, decimate to about 96 kHz, circuit, zero-stuff, anti-image.
    /// Drive above 0 dB raises the circuit's gain; what it cannot supply is an input trim.
    fn process_circuit<C: Circuit>(c: &mut C, d: &mut Decim, k: usize, ramp: &[f32], data: &mut [f32]) {
        let kd = k as f64;
        for i in 0..data.len() {
            let g = ramp[i] as f64;
            let x = data[i] as f64 * if g < 1.0 { g } else { 1.0 };
            if k == 1 {
                data[i] = step_circuit(c, d, x, g) as f32;
                continue;
            }
            let xf = d.aa_down.process(x);
            let mut up = 0.0;
            if d.phase == 0 {
                d.held = step_circuit(c, d, xf, g);
                up = d.held * kd;
            }
            d.phase += 1;
            if d.phase >= k {
                d.phase = 0;
            }
            data[i] = d.aa_up.process(up) as f32;
        }
    }

    fn config_circuits<C: Circuit>(circuits: &mut [C; MAX_CH], decim: &mut [Decim; MAX_CH], n_decim: &mut usize, sr: f64, ok: bool, full_scale: f64) {
        *n_decim = ((sr / CIRCUIT_RATE).round() as usize).max(1);
        let internal = sr / *n_decim as f64;
        for ch in 0..MAX_CH {
            if ok {
                circuits[ch].restore_dc_at(internal);
                circuits[ch].set_full_scale_volts(full_scale);
            }
            let d = &mut decim[ch];
            d.last_gain = 0.0;
            d.phase = 0;
            d.held = 0.0;
            d.aa_down.design(sr, 0.4 * internal);
            d.aa_up.design(sr, 0.4 * internal);
        }
    }

    /// The static stages' antiderivatives are valid for fixed gains, so ADAA holds while drive moves.
    #[inline]
    fn adaa_mode(&mut self, ch: usize, g: f64) -> Mode {
        let prev = &mut self.adaa_prev_drive[ch];
        let settled = (g - *prev).abs() <= 1.0e-6 * g;
        *prev = g;
        if settled {
            Mode::On
        } else {
            Mode::Hold
        }
    }

    fn ensure_config(&mut self, t: PreampType, on: bool) {
        if self.configured == Some((t, on)) {
            return;
        }
        self.configured = Some((t, on));
        for ch in 0..MAX_CH {
            self.n_adaa[ch].iter_mut().for_each(Residual::reset);
            self.b_adaa[ch].iter_mut().for_each(Residual::reset);
            self.f_adaa[ch].iter_mut().for_each(Residual::reset);
            self.a_adaa[ch].reset();
            self.adaa_prev_drive[ch] = -1.0;
        }
        let sr = self.sr;
        match t {
            PreampType::NType => {
                self.n_y1 = [0.0; MAX_CH];
                self.n_y2 = [0.0; MAX_CH];
                self.n_dc1 = [0.0; MAX_CH];
                self.n_dc2 = [0.0; MAX_CH];
                Self::config_circuits(&mut self.n_circ, &mut self.decim, &mut self.n_decim, sr, self.n_ok, N_FULL_SCALE);
            }
            PreampType::AType => Self::config_circuits(&mut self.a_circ, &mut self.decim, &mut self.n_decim, sr, self.a_ok, A_FULL_SCALE),
            PreampType::Brit => {
                // Coupling/servo high-pass poles: 2.2 uF into 100 k, 470 k into 2 x 470 nF, 100 uF into 10 k.
                let coef = |f: f64| 1.0 - (-2.0 * PI * f / sr).exp();
                self.brit_a = [coef(0.72), coef(0.36), coef(0.16)];
                self.brit = [[0.0; 3]; MAX_CH];
                Self::config_circuits(&mut self.b_circ, &mut self.decim, &mut self.n_decim, sr, self.b_ok, BRIT_FULL_SCALE);
            }
            PreampType::Off | PreampType::Fsf => {
                let (hp_f, lp_f) = if t == PreampType::Fsf { (15.0, 24000.0) } else { (22.0, 22000.0) };
                let hp = biquad::high_pass(sr, hp_f, 0.7071);
                let lp = biquad::low_pass(sr, biquad::safe_freq(sr, lp_f), 0.7071);
                for ch in 0..MAX_CH {
                    self.hp[ch].c = hp;
                    self.hp[ch].reset();
                    self.lp[ch].c = lp;
                    self.lp[ch].reset();
                }
                if t == PreampType::Fsf {
                    Self::config_circuits(&mut self.f_circ, &mut self.decim, &mut self.n_decim, sr, self.f_ok, FSF_FULL_SCALE);
                }
            }
        }
    }
}

/// The four netlists, solved and calibrated at the fixed internal rate. That takes about a second and
/// gives the same result every time, so it is done once per process and copied into each instance.
struct Netlists {
    n: [NType; MAX_CH],
    b: [Brit; MAX_CH],
    f: [Fsf; MAX_CH],
    a: [AType; MAX_CH],
    ok: [bool; 4],
}

fn netlists() -> &'static Netlists {
    static NETLISTS: std::sync::OnceLock<Netlists> = std::sync::OnceLock::new();
    NETLISTS.get_or_init(|| {
        let mut x = Netlists { n: Default::default(), b: Default::default(), f: Default::default(), a: Default::default(), ok: [false; 4] };
        x.ok[0] = x.n[0].prepare(CIRCUIT_RATE) & x.n[1].prepare(CIRCUIT_RATE);
        x.ok[1] = x.b[0].prepare(CIRCUIT_RATE) & x.b[1].prepare(CIRCUIT_RATE);
        let (first, rest) = x.f.split_at_mut(1);
        x.ok[2] = first[0].prepare(CIRCUIT_RATE) && rest[0].prepare_like(&first[0], CIRCUIT_RATE);
        x.ok[3] = x.a[0].prepare(CIRCUIT_RATE) & x.a[1].prepare(CIRCUIT_RATE);
        x
    })
}

fn step_circuit<C: Circuit>(c: &mut C, d: &mut Decim, x: f64, g: f64) -> f64 {
    let gain = if g < 1.0 { 1.0 } else { g };
    let cg = gain.min(C::MAX_GAIN);
    if (cg - d.last_gain).abs() > 1e-4 * d.last_gain {
        c.set_gain(cg);
        d.last_gain = cg;
    }
    let mut y = c.process(x * (gain / cg));
    if !y.is_finite() {
        c.restore_dc();
        d.last_gain = 0.0;
        y = 0.0;
    }
    y
}

/// Solves y = h(x - beta*y) for one transistor gain stage inside its feedback loop (warm-started,
/// step-limited Newton; the loop function is monotonic).
fn solve_loop(x: f64, beta: f64, a0: f64, kappa: f64, v_s: f64, y: &mut f64) -> f64 {
    for _ in 0..5 {
        let v = x - beta * *y;
        let cur = solve_leg(v * v_s, kappa);
        let h = a0 * (cur - 1.0) / v_s;
        let dh = a0 * cur / (1.0 + kappa * cur);
        let step = ((*y - h) / (1.0 + beta * dh)).clamp(-2.0, 2.0);
        *y -= step;
    }
    *y = y.clamp(-8.0, 8.0);
    *y
}

/// Transformer core: unity small-signal slope, saturating smoothly.
#[inline]
fn core_saturate(v: f64, sat: f64) -> f64 {
    let r = v / sat;
    v / (1.0 + r * r).sqrt()
}

#[inline]
fn asym_limit(v: f64, pos: f64, neg: f64) -> f64 {
    rail_limit(v, if v >= 0.0 { pos } else { neg })
}

/// One transistor leg: solves w + kappa (e^w - 1) = v and returns e^w (relative collector current).
#[inline]
fn solve_leg(v: f64, kappa: f64) -> f64 {
    let mut w = if v >= 0.0 { if kappa > 1e-9 { v.min((v / kappa).ln_1p()) } else { v } } else { 0.0 };
    for _ in 0..6 {
        if w > 20.0 {
            w = 20.0;
        }
        let e = w.exp();
        w -= (w + kappa * (e - 1.0) - v) / (1.0 + kappa * e);
    }
    w.min(20.0).exp()
}

/// Op-amp output limit: unity slope below the rails, hard knee at them.
#[inline]
fn rail_limit(v: f64, rail: f64) -> f64 {
    let t = v.abs() / rail;
    let t2 = t * t;
    let t4 = t2 * t2;
    v / (1.0 + t4 * t4).powf(0.125)
}
