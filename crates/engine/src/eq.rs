//! The six-band console EQ: Low Cut, Low shelf, Mid 1, Mid 2, High shelf, High Cut, each with
//! Stereo / Mid / Side routing and a bypass. Corners above 10 % of the rate are fitted to the analog
//! curve (`fit`), so the EQ does not cramp near Nyquist without oversampling.

use crate::biquad::{self, Biquad, Coeffs};
use crate::fit::{self, Shape, Spec};

pub const MAX_STAGES: usize = 4;
pub const MAX_CHANNELS: usize = 2;

#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
pub enum MsMode {
    #[default]
    Stereo,
    Mid,
    Side,
}

impl MsMode {
    pub fn from_index(i: usize) -> Self {
        match i {
            1 => MsMode::Mid,
            2 => MsMode::Side,
            _ => MsMode::Stereo,
        }
    }
    /// Whether a band in this mode runs on the mid path (`mid == true`) or the side path.
    #[inline]
    fn on_path(self, mid: bool) -> bool {
        if mid {
            self != MsMode::Side
        } else {
            self != MsMode::Mid
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ShelfType {
    Baxandall,
    Brit,
    Fsf,
}

impl ShelfType {
    pub fn from_index(i: usize) -> Self {
        match i {
            1 => ShelfType::Brit,
            2 => ShelfType::Fsf,
            _ => ShelfType::Baxandall,
        }
    }
    fn q(self) -> f64 {
        match self {
            ShelfType::Baxandall => biquad::BAX_SHELF_Q,
            ShelfType::Brit => biquad::BRIT_SHELF_Q,
            ShelfType::Fsf => biquad::FSF_SHELF_Q,
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum BellType {
    NEq,
    Brit,
    AType,
}

impl BellType {
    pub fn from_index(i: usize) -> Self {
        match i {
            1 => BellType::Brit,
            2 => BellType::AType,
            _ => BellType::NEq,
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Band {
    LowCut,
    HighCut,
    Low,
    Mid1,
    Mid2,
    High,
}

/// A cascade of up to four sections, per channel.
#[derive(Clone, Default)]
pub struct Cascade {
    stages: [[Biquad; MAX_STAGES]; MAX_CHANNELS],
    active: usize,
}

impl Cascade {
    fn set_order(&mut self, order: usize) {
        self.active = order.clamp(1, MAX_STAGES);
    }
    fn set_coeffs(&mut self, stage: usize, c: Coeffs) {
        for ch in &mut self.stages {
            ch[stage].c = c;
        }
    }
    pub fn reset(&mut self) {
        for ch in &mut self.stages {
            for s in ch {
                s.reset();
            }
        }
    }
    pub fn active(&self) -> usize {
        self.active
    }
    pub fn coeffs(&self, stage: usize) -> Coeffs {
        self.stages[0][stage].c
    }
    #[inline]
    fn process(&mut self, data: &mut [f32], channel: usize) {
        for s in &mut self.stages[channel][..self.active] {
            for x in data.iter_mut() {
                *x = s.process(*x);
            }
        }
    }
}

#[derive(Clone, Copy, Default, PartialEq)]
struct Key {
    v: [f64; 5],
    sr: f64,
    valid: bool,
}

/// Fit used from this fraction of the sample rate upward.
const FIT_FRACTION: f64 = 0.1;

#[derive(Clone, Copy, Default)]
struct BandState {
    mode: MsMode,
    bypass: bool,
}

pub struct AnalogEq {
    sr: f64,
    pub low_cut: Cascade,
    pub high_cut: Cascade,
    pub low: Cascade,
    pub mid1: Cascade,
    pub mid2: Cascade,
    pub high: Cascade,
    bands: [BandState; 6],
    ms_active: bool,
    hc_key: Key,
    mid1_key: Key,
    mid2_key: Key,
    high_key: Key,
    hc_fitted: bool,
    hc_fitted_order: usize,
    mid1_fitted: bool,
    mid2_fitted: bool,
    high_fitted: bool,
    scratch: [Vec<f32>; 2],
}

impl Default for AnalogEq {
    fn default() -> Self {
        Self::new()
    }
}

impl AnalogEq {
    pub fn new() -> Self {
        Self {
            sr: 44100.0,
            low_cut: Cascade::default(),
            high_cut: Cascade::default(),
            low: Cascade::default(),
            mid1: Cascade::default(),
            mid2: Cascade::default(),
            high: Cascade::default(),
            bands: [BandState::default(); 6],
            ms_active: false,
            hc_key: Key::default(),
            mid1_key: Key::default(),
            mid2_key: Key::default(),
            high_key: Key::default(),
            hc_fitted: false,
            hc_fitted_order: 0,
            mid1_fitted: false,
            mid2_fitted: false,
            high_fitted: false,
            scratch: [Vec::new(), Vec::new()],
        }
    }

    /// Allocates the mid/side scratch for `max_samples` (oversampled) samples.
    pub fn prepare(&mut self, sample_rate: f64, max_samples: usize) {
        self.sr = sample_rate;
        for s in &mut self.scratch {
            s.clear();
            s.resize(max_samples, 0.0);
        }
        self.reset();
    }

    pub fn reset(&mut self) {
        self.low_cut.reset();
        self.high_cut.reset();
        self.low.reset();
        self.mid1.reset();
        self.mid2.reset();
        self.high.reset();
    }

    pub fn sample_rate(&self) -> f64 {
        self.sr
    }

    pub fn set_sample_rate(&mut self, sr: f64) {
        self.sr = sr;
    }

    fn slot(band: Band) -> usize {
        match band {
            Band::LowCut => 0,
            Band::HighCut => 1,
            Band::Low => 2,
            Band::Mid1 => 3,
            Band::Mid2 => 4,
            Band::High => 5,
        }
    }

    pub fn set_bypass(&mut self, band: Band, bypassed: bool) {
        self.bands[Self::slot(band)].bypass = bypassed;
    }

    pub fn is_bypassed(&self, band: Band) -> bool {
        self.bands[Self::slot(band)].bypass
    }

    fn set_mode(&mut self, band: Band, mode: MsMode) {
        self.bands[Self::slot(band)].mode = mode;
        self.ms_active = self.bands.iter().any(|b| b.mode != MsMode::Stereo);
    }

    fn unchanged(key: &mut Key, sr: f64, v: [f64; 5]) -> bool {
        if key.valid && key.sr == sr && key.v == v {
            return true;
        }
        *key = Key { v, sr, valid: true };
        false
    }

    pub fn update_low_cut(&mut self, freq: f64, q: f64, slope: usize, mode: MsMode) {
        self.set_mode(Band::LowCut, mode);
        let safe = biquad::safe_freq(self.sr, freq);
        let order = slope + 1;
        self.low_cut.set_order(order);
        for i in 0..order {
            let c = biquad::high_pass(self.sr, safe, biquad::cut_stage_q(order, i, q));
            self.low_cut.set_coeffs(i, c);
        }
    }

    pub fn update_high_cut(&mut self, freq: f64, q: f64, slope: usize, mode: MsMode) {
        self.set_mode(Band::HighCut, mode);
        if Self::unchanged(&mut self.hc_key, self.sr, [freq, q, slope as f64, 0.0, 0.0]) {
            return;
        }
        let sr = self.sr;
        let safe = biquad::safe_freq(sr, freq);
        let order = slope + 1;
        self.high_cut.set_order(order);
        let mut specs = [Spec { shape: Shape::LowPass, f0: freq, q: 0.7071, gain_db: 0.0 }; MAX_STAGES];
        let mut standard = [Coeffs::BYPASS; MAX_STAGES];
        for i in 0..order {
            let stage_q = biquad::cut_stage_q(order, i, q);
            standard[i] = biquad::low_pass(sr, safe, stage_q);
            specs[i] = Spec { shape: Shape::LowPass, f0: freq, q: stage_q, gain_db: 0.0 };
        }
        let mut done = false;
        if freq >= FIT_FRACTION * sr {
            let have_warm = self.hc_fitted && self.hc_fitted_order == order;
            let mut warm = [Coeffs::BYPASS; MAX_STAGES];
            for (i, w) in warm.iter_mut().enumerate().take(order) {
                *w = self.high_cut.coeffs(i);
            }
            let mut out = [Coeffs::BYPASS; MAX_STAGES];
            fit::design_cascade(&specs[..order], sr, if have_warm { Some(&warm[..order]) } else { None }, &mut out[..order]);
            if out[..order].iter().all(fit::is_stable) {
                for (i, c) in out.iter().enumerate().take(order) {
                    self.high_cut.set_coeffs(i, *c);
                }
                done = true;
            }
        }
        if !done {
            for (i, c) in standard.iter().enumerate().take(order) {
                self.high_cut.set_coeffs(i, *c);
            }
        }
        self.hc_fitted = done;
        self.hc_fitted_order = order;
    }

    pub fn update_low(&mut self, freq: f64, gain: f64, kind: ShelfType, mode: MsMode) {
        self.set_mode(Band::Low, mode);
        self.low.set_order(1);
        self.low.set_coeffs(0, biquad::low_shelf(self.sr, freq, kind.q(), gain));
    }

    pub fn update_mid1(&mut self, freq: f64, gain: f64, q: f64, kind: BellType, mode: MsMode) {
        self.set_mode(Band::Mid1, mode);
        if Self::unchanged(&mut self.mid1_key, self.sr, [freq, gain, q, kind as usize as f64, 0.0]) {
            return;
        }
        self.mid1.set_order(1);
        let previous = self.mid1.coeffs(0);
        let mut fitted = self.mid1_fitted;
        let c = Self::design_bell(self.sr, freq, gain, q, kind, &previous, &mut fitted);
        self.mid1_fitted = fitted;
        self.mid1.set_coeffs(0, c);
    }

    pub fn update_mid2(&mut self, freq: f64, gain: f64, q: f64, kind: BellType, mode: MsMode) {
        self.set_mode(Band::Mid2, mode);
        if Self::unchanged(&mut self.mid2_key, self.sr, [freq, gain, q, kind as usize as f64, 0.0]) {
            return;
        }
        self.mid2.set_order(1);
        let previous = self.mid2.coeffs(0);
        let mut fitted = self.mid2_fitted;
        let c = Self::design_bell(self.sr, freq, gain, q, kind, &previous, &mut fitted);
        self.mid2_fitted = fitted;
        self.mid2.set_coeffs(0, c);
    }

    pub fn update_high(&mut self, freq: f64, gain: f64, kind: ShelfType, mode: MsMode) {
        self.set_mode(Band::High, mode);
        if Self::unchanged(&mut self.high_key, self.sr, [freq, gain, kind as usize as f64, 0.0, 0.0]) {
            return;
        }
        self.high.set_order(1);
        let safe = biquad::safe_freq(self.sr, freq);
        let shelf_q = kind.q();
        let standard = biquad::high_shelf(self.sr, safe, shelf_q, gain);
        let previous = self.high.coeffs(0);
        let mut fitted = self.high_fitted;
        let spec = Spec { shape: Shape::HighShelf, f0: freq, q: shelf_q, gain_db: gain };
        let c = Self::fitted(self.sr, &spec, standard, &previous, &mut fitted);
        self.high_fitted = fitted;
        self.high.set_coeffs(0, c);
    }

    /// Standard design below the fit threshold, magnitude fit above it.
    fn fitted(sr: f64, spec: &Spec, standard: Coeffs, previous: &Coeffs, was_fitted: &mut bool) -> Coeffs {
        if spec.f0 < FIT_FRACTION * sr {
            *was_fitted = false;
            return standard;
        }
        let r = fit::design(spec, sr, if *was_fitted { Some(previous) } else { None });
        *was_fitted = r.fitted;
        if r.fitted {
            r.c
        } else {
            standard
        }
    }

    fn design_bell(sr: f64, freq: f64, gain: f64, q: f64, kind: BellType, previous: &Coeffs, was_fitted: &mut bool) -> Coeffs {
        let (bell_q, standard) = match kind {
            BellType::Brit => (q * biquad::BRIT_BELL_Q_SCALE, biquad::peak(sr, freq, q * biquad::BRIT_BELL_Q_SCALE, gain)),
            BellType::AType => (biquad::A_TYPE_BELL_Q, biquad::peak(sr, freq, biquad::A_TYPE_BELL_Q, gain)),
            BellType::NEq => {
                let nq = biquad::n_type_q(q, gain);
                (nq, biquad::peak(sr, freq, nq, gain))
            }
        };
        let spec = Spec { shape: Shape::Peak, f0: freq, q: bell_q, gain_db: gain };
        Self::fitted(sr, &spec, standard, previous, was_fitted)
    }

    /// Processes up to two channels in place (the slices are the same length).
    pub fn process(&mut self, channels: &mut [&mut [f32]]) {
        if channels.len() == 2 && self.ms_active {
            let n = channels[0].len();
            let (left, right) = channels.split_at_mut(1);
            let (left, right) = (&mut *left[0], &mut *right[0]);
            let [mid, side] = &mut self.scratch;
            let (mid, side) = (&mut mid[..n], &mut side[..n]);
            for i in 0..n {
                mid[i] = (left[i] + right[i]) * 0.5;
                side[i] = (left[i] - right[i]) * 0.5;
            }
            Self::chain(&self.bands, [&mut self.low_cut, &mut self.low, &mut self.mid1, &mut self.mid2, &mut self.high, &mut self.high_cut], mid, 0, true);
            Self::chain(&self.bands, [&mut self.low_cut, &mut self.low, &mut self.mid1, &mut self.mid2, &mut self.high, &mut self.high_cut], side, 1, false);
            for i in 0..n {
                left[i] = mid[i] + side[i];
                right[i] = mid[i] - side[i];
            }
            return;
        }
        for (ch, data) in channels.iter_mut().enumerate().take(MAX_CHANNELS) {
            Self::chain(&self.bands, [&mut self.low_cut, &mut self.low, &mut self.mid1, &mut self.mid2, &mut self.high, &mut self.high_cut], data, ch, true);
        }
    }

    /// Serial band chain in signal order: Low Cut, Low, Mid 1, Mid 2, High, High Cut.
    fn chain(bands: &[BandState; 6], filters: [&mut Cascade; 6], data: &mut [f32], ch: usize, mid_path: bool) {
        // band slots in signal order
        const ORDER: [usize; 6] = [0, 2, 3, 4, 5, 1];
        for (f, slot) in filters.into_iter().zip(ORDER) {
            let b = bands[slot];
            if !b.bypass && b.mode.on_path(mid_path) {
                f.process(data, ch);
            }
        }
    }

    /// Complex product of the active (not bypassed) sections at `freq`, as |H|^2.
    pub fn magnitude_sq(&self, freq: f64) -> f64 {
        let nf = freq / self.sr;
        let mut total = 1.0;
        let all = [(&self.low_cut, 0), (&self.high_cut, 1), (&self.low, 2), (&self.mid1, 3), (&self.mid2, 4), (&self.high, 5)];
        for (f, slot) in all {
            if self.bands[slot].bypass {
                continue;
            }
            for s in 0..f.active() {
                total *= f.coeffs(s).magnitude_sq(nf);
            }
        }
        total
    }

    pub fn magnitude_db(&self, freq: f64) -> f64 {
        10.0 * self.magnitude_sq(freq).max(1e-20).log10()
    }
}
