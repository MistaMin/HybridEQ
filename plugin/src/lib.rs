//! HybridEQ on OpenPluginSDK: the engine (`hybrideq_engine`) inside the multirate wrapper, output
//! gain, Autogain and Delta, with the identity of 1.6.x so its sessions open in this version.
//!
//! Signal path: oversampled core (preamp, EQ, harmonics) -> output gain -> Autogain (loudness of the
//! input, BS.1770, when on) -> Delta (the result minus the input delayed by the reported latency, when
//! on). Delta therefore hears what the processing changes in tone, after any level matching.

pub mod editor;
pub mod multirate;
pub mod params;
pub mod state;

use std::sync::atomic::{AtomicU32, AtomicU64, Ordering};
use std::sync::Arc;

use hybrideq_engine::params::{idx, COUNT, TABLE};
use hybrideq_engine::{defaults, effective_factor, tail_seconds, Engine};
use multirate::Multirate;
use openplugin::core::{AaxIds, AuIds, Vst3Ids};
use openplugin::dri::goodlookindri::spectrum::SampleRing;
use openplugin::dsp::{Delta, LoudnessMatch};
use openplugin::prelude::*;

pub use params::PARAMS;

pub static INFO: PluginInfo = PluginInfo::new("com.opengrid.hybrideq", "HybridEQ", "OpenGrid", "2.0.0", Category::Eq, &PARAMS)
    .with_url("https://github.com/MistaMin/HybridEQ")
    .with_editor(true);

/// What the engine shows its editor: levels and the effective oversampling factor.
#[derive(Default)]
pub struct Shared {
    /// Peak since the editor last looked, linear, as f32 bits: input L, R, output L, R.
    peaks: [AtomicU32; 4],
    /// Output sum of squares (L, R) as f32 bits, and the frames it covers, since the editor last looked.
    energy: [AtomicU32; 2],
    frames: AtomicU32,
    factor: AtomicU32,
    /// Editor size in points (width << 32 | height), saved with the project; 0 until set.
    size: AtomicU64,
    sample_rate: AtomicU32,
    /// The output for the editor's analyser (wait-free; the FFT runs in the editor).
    pub ring: SampleRing,
}

impl Shared {
    fn raise(slot: &AtomicU32, v: f32) {
        if v > f32::from_bits(slot.load(Ordering::Relaxed)) {
            slot.store(v.to_bits(), Ordering::Relaxed);
        }
    }

    /// Input L, R and output L, R peaks since the last call.
    pub fn take_peaks(&self) -> [f32; 4] {
        std::array::from_fn(|i| f32::from_bits(self.peaks[i].swap(0, Ordering::Relaxed)))
    }

    fn add(slot: &AtomicU32, v: f32) {
        let _ = slot.fetch_update(Ordering::Relaxed, Ordering::Relaxed, |b| Some((f32::from_bits(b) + v).to_bits()));
    }

    /// Output L, R mean square since the last call (zero when no audio ran).
    pub fn take_power(&self) -> [f32; 2] {
        let frames = self.frames.swap(0, Ordering::Relaxed).max(1) as f32;
        std::array::from_fn(|i| f32::from_bits(self.energy[i].swap(0, Ordering::Relaxed)) / frames)
    }

    /// The editor size saved with the project, if any.
    pub fn editor_size(&self) -> Option<(u32, u32)> {
        match self.size.load(Ordering::Relaxed) {
            0 => None,
            v => Some(((v >> 32) as u32, v as u32)),
        }
    }

    pub fn set_editor_size(&self, width: u32, height: u32) {
        self.size.store((u64::from(width) << 32) | u64::from(height), Ordering::Relaxed);
    }

    /// The oversampling factor the engine runs at (the user's choice, or 2x for HQ).
    pub fn factor(&self) -> u32 {
        self.factor.load(Ordering::Relaxed).max(1)
    }

    /// The session sample rate (48 kHz before the first prepare).
    pub fn sample_rate(&self) -> f64 {
        match f32::from_bits(self.sample_rate.load(Ordering::Relaxed)) {
            r if r > 0.0 => r as f64,
            _ => 48000.0,
        }
    }
}

pub struct HybridEq {
    p: [f32; COUNT],
    engine: Engine,
    multirate: Option<Multirate>,
    autogain: LoudnessMatch,
    delta: Option<Delta>,
    dry: Vec<Vec<f32>>,
    latency: usize,
    sample_rate: f32,
    shared: Arc<Shared>,
}

impl Default for HybridEq {
    fn default() -> Self {
        Self {
            p: defaults(),
            engine: Engine::default(),
            multirate: None,
            autogain: LoudnessMatch::new(48000.0),
            delta: None,
            dry: Vec::new(),
            latency: 0,
            sample_rate: 48000.0,
            shared: Arc::new(Shared::default()),
        }
    }
}

impl HybridEq {
    /// Replaces every value at once (a state load).
    pub fn set_values(&mut self, values: [f32; COUNT]) {
        for (i, v) in values.into_iter().enumerate() {
            self.p[i] = TABLE[i].clamp(v);
        }
    }

    fn update_latency(&mut self) {
        let factor = effective_factor(&self.p);
        let linear = self.p[idx::OVERSAMPLE_FILTER] > 0.5;
        if let Some(m) = self.multirate.as_mut() {
            m.set(factor, linear);
            self.latency = m.latency_for(factor, linear);
        }
        if let Some(d) = self.delta.as_mut() {
            d.set_latency(self.latency);
        }
        self.shared.factor.store(factor as u32, Ordering::Relaxed);
    }
}

impl Plugin for HybridEq {
    fn info(&self) -> &'static PluginInfo {
        &INFO
    }

    fn prepare(&mut self, sample_rate: f32, max_block: usize, _channels: usize) {
        let sr = sample_rate as f64;
        self.sample_rate = sample_rate;
        self.shared.sample_rate.store(sample_rate.to_bits(), Ordering::Relaxed);
        self.engine.prepare(sr, max_block, &self.p);
        self.engine.set_params(&self.p);
        let multirate = Multirate::new(sr, max_block);
        let max_latency = [2, 4, 8].iter().flat_map(|&f| [multirate.latency_for(f, false), multirate.latency_for(f, true)]).max().unwrap_or(0);
        self.multirate = Some(multirate);
        self.delta = Some(Delta::prepare(2, max_latency, sample_rate, 20.0));
        self.autogain = LoudnessMatch::new(sample_rate);
        self.dry = vec![vec![0.0; max_block]; 2];
        self.update_latency();
        self.reset();
    }

    fn reset(&mut self) {
        self.engine.reset();
        if let Some(m) = self.multirate.as_mut() {
            m.reset();
        }
        if let Some(d) = self.delta.as_mut() {
            d.reset();
        }
        self.autogain.reset();
    }

    fn set_param(&mut self, id: &str, value: f32) -> bool {
        match hybrideq_engine::params::index_of(id) {
            Some(i) => self.set_param_index(i, value),
            None => false,
        }
    }

    fn set_param_index(&mut self, index: usize, value: f32) -> bool {
        let Some(info) = TABLE.get(index) else { return false };
        self.p[index] = info.clamp(value);
        if matches!(index, idx::OVERSAMPLE_MODE | idx::OVERSAMPLE_FILTER | idx::PREAMP_CIRCUIT | idx::PREAMP_HQ) {
            self.update_latency();
        }
        true
    }

    fn param(&self, id: &str) -> Option<f32> {
        hybrideq_engine::params::index_of(id).map(|i| self.p[i])
    }

    fn latency(&self) -> usize {
        self.latency
    }

    fn tail_samples(&self) -> usize {
        (tail_seconds(&self.p) * f64::from(self.sample_rate)).ceil() as usize
    }

    fn process(&mut self, io: &mut [&mut [f32]], frames: usize) {
        let channels = io.len().min(2);
        if channels == 0 || frames == 0 {
            return;
        }
        let peak = |s: &[f32]| s.iter().fold(0.0f32, |m, x| m.max(x.abs()));
        for (ch, x) in io[..channels].iter().enumerate() {
            self.dry[ch][..frames].copy_from_slice(&x[..frames]);
            Shared::raise(&self.shared.peaks[ch], peak(&x[..frames]));
        }
        self.engine.set_params(&self.p);
        let engine = &mut self.engine;
        if let Some(m) = self.multirate.as_mut() {
            m.process(io, frames, |ch| engine.process(ch));
        }

        let gain = 10f32.powf(self.p[idx::OUTPUT_GAIN] * 0.05);
        if gain != 1.0 {
            for ch in io.iter_mut().take(channels) {
                for x in &mut ch[..frames] {
                    *x *= gain;
                }
            }
        }

        let dry: [&[f32]; 2] = [&self.dry[0][..frames], &self.dry[1][..frames]];
        let auto_on = self.p[idx::AUTO_GAIN] > 0.5;
        if auto_on != self.autogain.is_enabled() {
            self.autogain.set_enabled(auto_on);
        }
        let delta_on = self.p[idx::DELTA] > 0.5;
        {
            let (a, b) = io.split_at_mut(1);
            let mut pair: [&mut [f32]; 2] = [&mut a[0][..frames], if channels == 2 { &mut b[0][..frames] } else { &mut [] }];
            let wet = &mut pair[..channels];
            self.autogain.process(&dry[..channels], wet);
            if let Some(d) = self.delta.as_mut() {
                if delta_on != d.is_enabled() {
                    d.set_enabled(delta_on);
                }
                d.process(&dry[..channels], wet);
            }
        }

        match channels {
            2 => self.shared.ring.push(&[&io[0][..frames], &io[1][..frames]]),
            _ => self.shared.ring.push(&[&io[0][..frames]]),
        }
        for (ch, data) in io[..channels].iter_mut().enumerate() {
            // Filter state decays below f32 range on silence; nothing under -600 dBFS reaches the host.
            for x in &mut data[..frames] {
                if x.abs() < 1e-30 {
                    *x = 0.0;
                }
            }
            let out = &data[..frames];
            Shared::raise(&self.shared.peaks[2 + ch], peak(out));
            Shared::add(&self.shared.energy[ch], out.iter().map(|x| x * x).sum::<f32>());
        }
        self.shared.frames.fetch_add(frames as u32, Ordering::Relaxed);
    }

    fn note_ports(&self) -> NotePorts {
        NotePorts { input: false, output: false }
    }

    fn editor_state(&self) -> Option<EditorState> {
        Some(self.shared.clone())
    }

    fn save_state(&mut self) -> Option<Vec<u8>> {
        Some(state::encode(&self.p, self.shared.editor_size()))
    }

    fn load_state(&mut self, data: &[u8]) -> bool {
        match state::decode(data) {
            Some((values, size)) => {
                self.set_values(values);
                if let Some((w, h)) = size {
                    self.shared.set_editor_size(w, h);
                }
                self.update_latency();
                true
            }
            None => false,
        }
    }

    // ---- identity of 1.6.x ----
    fn vst3_ids(&self) -> Option<Vst3Ids> {
        Vst3Ids::from_hex("ABCDEF019182FAEB4879627248794571", "ABCDEF011234ABCD4879627248794571")
    }

    fn au_ids(&self) -> Option<AuIds> {
        Some(AuIds { subtype: *b"HyEq", manufacturer: *b"Hybr" })
    }

    fn aax_ids(&self) -> Option<AaxIds> {
        // 1.6.x's AAX plug-in types: 'jcaa' plus the mono (1) or stereo (2) layout in and out.
        Some(AaxIds { manufacturer: *b"Hybr", product: *b"HyEq", mono: *b"jcbb", stereo: *b"jccc" })
    }

    fn lv2_uri(&self) -> Option<&'static str> {
        Some("https://github.com/MistaMin/HybridEQ")
    }
}

openplugin::targets::export_clap!(HybridEq, editor = editor::make_editor);
openplugin::targets::export_lv2!(HybridEq, editor = editor::make_editor);
openplugin::targets::export_shell!(HybridEq, editor = editor::make_editor);
