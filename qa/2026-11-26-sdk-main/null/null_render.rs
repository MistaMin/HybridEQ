//! Scratch null render (QA 2026-11-26): `null_render <out-dir>` writes one raw f32 stereo file per scenario.
use hybrideq::HybridEq;
use hybrideq_engine::params::{idx::*, PARAMS};
use openplugin::prelude::*;
use std::io::Write;

fn main() {
    let out = std::env::args().nth(1).expect("out dir");
    let s: &[(&str, usize, usize, &[(usize, f32)])] = &[
        ("default", 2, 512, &[]),
        ("mono", 1, 512, &[]),
        ("block-odd", 2, 333, &[(PREAMP_TYPE, 2.0), (PREAMP_GAIN, 15.0)]),
        ("brit-hot", 2, 512, &[(PREAMP_TYPE, 1.0), (PREAMP_GAIN, 18.0)]),
        ("ntype-hot", 2, 512, &[(PREAMP_TYPE, 2.0), (PREAMP_GAIN, 20.0), (PREAMP_PAD, 2.0)]),
        ("fsf-hot", 2, 512, &[(PREAMP_TYPE, 3.0), (PREAMP_GAIN, 15.0)]),
        ("atype-hot", 2, 512, &[(PREAMP_TYPE, 4.0), (PREAMP_GAIN, 24.0)]),
        ("circuit-hq", 2, 512, &[(PREAMP_TYPE, 1.0), (PREAMP_GAIN, 12.0), (PREAMP_CIRCUIT, 1.0), (PREAMP_HQ, 1.0)]),
        ("reduced-brit", 2, 512, &[(PREAMP_TYPE, 1.0), (PREAMP_GAIN, 12.0), (PREAMP_CIRCUIT, 1.0), (PREAMP_HQ, 0.0)]),
        ("reduced-ntype", 2, 512, &[(PREAMP_TYPE, 2.0), (PREAMP_GAIN, 18.0), (PREAMP_CIRCUIT, 1.0), (PREAMP_HQ, 0.0)]),
        ("os2-iir", 2, 512, &[(OVERSAMPLE_MODE, 1.0), (PREAMP_TYPE, 4.0), (PREAMP_GAIN, 18.0)]),
        ("os4-fir", 2, 512, &[(OVERSAMPLE_MODE, 2.0), (OVERSAMPLE_FILTER, 1.0), (PREAMP_TYPE, 1.0), (PREAMP_GAIN, 18.0)]),
        ("os8-iir-reduced", 2, 256, &[(OVERSAMPLE_MODE, 3.0), (PREAMP_TYPE, 2.0), (PREAMP_GAIN, 18.0), (PREAMP_CIRCUIT, 1.0), (PREAMP_HQ, 0.0)]),
        ("eq-ms", 2, 512, &[(LC_MODE, 1.0), (LC_FREQ, 150.0), (LOW_MODE, 2.0), (LOW_GAIN, 6.0), (MID1_MODE, 1.0), (MID1_GAIN, 8.0), (MID2_MODE, 2.0), (MID2_GAIN, -8.0), (HIGH_MODE, 1.0), (HIGH_GAIN, 4.0), (HC_MODE, 2.0), (HC_FREQ, 8000.0)]),
        ("harmonics", 2, 512, &[(H2, 0.4), (H3, 0.3), (H4, 0.2), (H5, 0.5)]),
        ("autogain", 2, 512, &[(PREAMP_GAIN, 12.0), (AUTO_GAIN, 1.0)]),
        ("delta", 2, 512, &[(PREAMP_TYPE, 1.0), (PREAMP_GAIN, 12.0), (DELTA, 1.0)]),
    ];
    for &(name, ch, block, set) in s {
        let mut p = HybridEq::default();
        for &(i, v) in set {
            p.set_param(PARAMS[i].id, v);
        }
        p.prepare(48000.0, block, ch);
        let mut seed = 7u32;
        let mut f = std::fs::File::create(format!("{out}/{name}.f32")).unwrap();
        for b in 0..(48000 * 3 / block) {
            let mut io: Vec<Vec<f32>> = vec![vec![0.0; block]; ch];
            for i in 0..block {
                let t = (b * block + i) as f32 / 48000.0;
                seed = seed.wrapping_mul(1664525).wrapping_add(1013904223);
                let n = ((seed >> 8) as f32 / (1 << 24) as f32 - 0.5) * 0.1;
                let x = 0.3 * (std::f32::consts::TAU * 110.0 * t).sin() + 0.15 * (std::f32::consts::TAU * 2500.0 * t).sin() + n;
                for (c, d) in io.iter_mut().enumerate() {
                    d[i] = x * (1.0 - 0.2 * c as f32);
                }
            }
            p.process(&mut io.iter_mut().map(Vec::as_mut_slice).collect::<Vec<_>>(), block);
            for i in 0..block {
                for d in &io {
                    f.write_all(&d[i].to_le_bytes()).unwrap();
                }
            }
        }
    }
}
