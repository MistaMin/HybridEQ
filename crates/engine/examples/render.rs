//! Offline renderer for the null test against 1.6.16 (tests/reference_render.cpp).
//!
//!   render run  <in.f32> <out.f32> <sr> <block> <params-file>   full path at 1x (output gain applied)
//!   render core <in.f32> <out.f32> <sr> <block> <params-file>   engine core at the effective rate
//!   render compare <a.f32> <b.f32>                               max and RMS difference in dBFS
//!
//! Files are interleaved stereo float32. The params file is the `.params` sidecar the C++ renderer
//! writes (`id=value` per line, plain units).

use hybrideq_engine::params::{idx, index_of};
use hybrideq_engine::{defaults, effective_factor, Engine};
use std::fs;

fn read_f32(path: &str) -> Vec<f32> {
    fs::read(path).unwrap().chunks_exact(4).map(|b| f32::from_le_bytes([b[0], b[1], b[2], b[3]])).collect()
}

fn write_f32(path: &str, v: &[f32]) {
    fs::write(path, v.iter().flat_map(|x| x.to_le_bytes()).collect::<Vec<u8>>()).unwrap();
}

fn main() {
    let a: Vec<String> = std::env::args().collect();
    if a.get(1).map(String::as_str) == Some("compare") {
        let (x, y) = (read_f32(&a[2]), read_f32(&a[3]));
        assert_eq!(x.len(), y.len(), "length differs");
        let (mut max, mut sum, mut at) = (0.0f64, 0.0f64, 0usize);
        for (i, (p, q)) in x.iter().zip(&y).enumerate() {
            let d = (*p as f64 - *q as f64).abs();
            sum += d * d;
            if d > max {
                max = d;
                at = i / 2;
            }
        }
        let db = |v: f64| if v > 0.0 { 20.0 * v.log10() } else { f64::NEG_INFINITY };
        println!("max {:.1} dBFS at frame {at}, rms {:.1} dBFS", db(max), db((sum / x.len() as f64).sqrt()));
        return;
    }
    let (mode, input, output) = (a[1].as_str(), &a[2], &a[3]);
    let sr: f64 = a[4].parse().unwrap();
    let block: usize = a[5].parse().unwrap();
    let mut p = defaults();
    for line in fs::read_to_string(&a[6]).unwrap().lines() {
        if let Some((id, v)) = line.split_once('=') {
            if let Some(i) = index_of(id) {
                p[i] = v.parse().unwrap();
            }
        }
    }
    let factor = effective_factor(&p);
    let core = mode == "core";
    assert!(core || factor == 1, "run mode is 1x only (the oversampler lives in the plugin)");
    // core mode: like the C++ tool, the engine is prepared at `sr` and the input is taken as samples at
    // the effective rate (`sr` times the factor).
    let mut e = Engine::default();
    e.prepare(sr, block, &p);
    e.set_params(&p);
    e.reset();
    let inp = read_f32(input);
    let frames = inp.len() / 2;
    let mut out = vec![0.0f32; inp.len()];
    let (mut l, mut r) = (vec![0.0f32; block], vec![0.0f32; block]);
    let gain = 10f32.powf(p[idx::OUTPUT_GAIN] * 0.05);
    let mut pos = 0;
    while pos < frames {
        let n = block.min(frames - pos);
        for i in 0..n {
            l[i] = inp[(pos + i) * 2];
            r[i] = inp[(pos + i) * 2 + 1];
        }
        e.set_params(&p);
        e.process(&mut [&mut l[..n], &mut r[..n]]);
        for i in 0..n {
            let (x, y) = if core { (l[i], r[i]) } else { (l[i] * gain, r[i] * gain) };
            out[(pos + i) * 2] = x;
            out[(pos + i) * 2 + 1] = y;
        }
        pos += n;
    }
    write_f32(output, &out);
}
