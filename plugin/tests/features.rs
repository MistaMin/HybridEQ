//! Autogain and Delta, the two features 2.0.0 adds (ledger S19, S20).
use hybrideq::HybridEq;
use hybrideq_engine::params::{idx, PARAMS};
use openplugin::prelude::*;

const SR: f32 = 48000.0;

fn plugin(settings: &[(usize, f32)]) -> HybridEq {
    let mut p = HybridEq::default();
    for &(i, v) in settings {
        p.set_param(PARAMS[i].id, v);
    }
    p.prepare(SR, 512, 2);
    p
}

/// Runs `seconds` of `signal` and returns (input, output) of the last `tail` seconds, left channel.
fn run(p: &mut HybridEq, seconds: f32, tail: f32, signal: impl Fn(usize) -> f32) -> (Vec<f32>, Vec<f32>) {
    let n = (seconds * SR) as usize;
    let keep = n - (tail * SR) as usize;
    let (mut input, mut output) = (Vec::new(), Vec::new());
    for start in (0..n).step_by(512) {
        let block: Vec<f32> = (start..start + 512).map(&signal).collect();
        let mut io = vec![block.clone(), block.clone()];
        p.process(&mut io.iter_mut().map(Vec::as_mut_slice).collect::<Vec<_>>(), 512);
        if start >= keep {
            input.extend_from_slice(&block);
            output.extend_from_slice(&io[0]);
        }
    }
    (input, output)
}

fn rms_db(x: &[f32]) -> f32 {
    10.0 * (x.iter().map(|v| v * v).sum::<f32>() / x.len() as f32).max(1e-30).log10()
}

fn noise() -> impl Fn(usize) -> f32 {
    |i| {
        let mut s = (i as u32).wrapping_mul(2654435761).wrapping_add(12345);
        s ^= s >> 13;
        s = s.wrapping_mul(1274126177);
        ((s >> 8) as f32 / (1u32 << 24) as f32 * 2.0 - 1.0) * 0.25
    }
}

/// A clean path: preamp off, every band bypassed, so only the oversampling (if any) touches the audio.
const CLEAN: [(usize, f32); 7] = [
    (idx::PREAMP_TYPE, 0.0),
    (idx::LC_BYPASS, 1.0),
    (idx::HC_BYPASS, 1.0),
    (idx::LOW_BYPASS, 1.0),
    (idx::MID1_BYPASS, 1.0),
    (idx::MID2_BYPASS, 1.0),
    (idx::HIGH_BYPASS, 1.0),
];

#[test]
fn autogain_brings_the_output_back_to_the_input_loudness() {
    let mut settings = CLEAN.to_vec();
    settings.extend([(idx::OUTPUT_GAIN, 6.0), (idx::AUTO_GAIN, 1.0)]);
    let mut p = plugin(&settings);
    let (input, output) = run(&mut p, 8.0, 2.0, noise());
    // white noise through a flat path: the K-weighted difference equals the RMS difference
    let diff = rms_db(&output) - rms_db(&input);
    assert!(diff.abs() < 0.5, "output {diff:+.2} dB from the input with +6 dB output gain");

    let mut off = plugin(&settings[..settings.len() - 1]);
    let (input, output) = run(&mut off, 2.0, 1.0, noise());
    assert!((rms_db(&output) - rms_db(&input) - 6.0).abs() < 0.1, "without Autogain the +6 dB stays");
}

#[test]
fn delta_is_silent_when_nothing_is_processed() {
    let tone = |i: usize| (2.0 * std::f32::consts::PI * 1000.0 * i as f32 / SR).sin() * 0.5;
    // 1x: no latency, bit-exact silence
    let mut settings = CLEAN.to_vec();
    settings.push((idx::DELTA, 1.0));
    let (_, out) = run(&mut plugin(&settings), 1.0, 0.5, tone);
    assert!(rms_db(&out) < -140.0, "1x: {:.1} dB", rms_db(&out));
    // 4x linear phase: the dry signal is delayed by the reported latency; only the filter's ripple remains
    settings.extend([(idx::OVERSAMPLE_MODE, 2.0), (idx::OVERSAMPLE_FILTER, 1.0)]);
    let mut p = plugin(&settings);
    assert!(p.latency() > 0);
    let (input, out) = run(&mut p, 1.0, 0.5, tone);
    assert!(rms_db(&out) - rms_db(&input) < -60.0, "4x FIR: delta {:.1} dB below the input", rms_db(&input) - rms_db(&out));
}
