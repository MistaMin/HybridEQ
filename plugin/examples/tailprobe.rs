use hybrideq::HybridEq;
use openplugin::prelude::*;
fn main() {
    let sr = 48000.0;
    let cases: Vec<(&str, Vec<(&str, f32)>)> = vec![
        ("default", vec![]),
        ("lc 20Hz Q10 24dB", vec![("lcQ", 10.0), ("lcSlope", 3.0)]),
        ("static N +20", vec![("preampType", 2.0), ("preampGain", 20.0)]),
        ("reduced Brit +20", vec![("preampCircuit", 1.0), ("preampHQ", 0.0), ("preampGain", 20.0)]),
        ("reduced N +20", vec![("preampCircuit", 1.0), ("preampHQ", 0.0), ("preampType", 2.0), ("preampGain", 20.0)]),
        ("HQ Brit +20", vec![("preampCircuit", 1.0), ("preampGain", 20.0), ("lcBypass", 1.0)]),
        ("HQ N +20", vec![("preampCircuit", 1.0), ("preampType", 2.0), ("preampGain", 20.0), ("lcBypass", 1.0)]),
        ("HQ FSF +20", vec![("preampCircuit", 1.0), ("preampType", 3.0), ("preampGain", 20.0), ("lcBypass", 1.0)]),
        ("HQ A +20", vec![("preampCircuit", 1.0), ("preampType", 4.0), ("preampGain", 20.0), ("lcBypass", 1.0)]),
    ];
    for (label, setup) in cases {
        let mut p = HybridEq::default();
        for (id, v) in &setup { p.set_param(id, *v); }
        p.prepare(sr, 512, 2);
        let mut seed = 1u32;
        let mut rnd = || { seed = seed.wrapping_mul(1664525).wrapping_add(1013904223); (seed >> 8) as f32 / (1u32 << 24) as f32 * 2.0 - 1.0 };
        let mut last_loud = 0usize;
        let n_noise = sr as usize / 2;
        for b in 0..(sr as usize * 12 / 512) {
            let noisy = b * 512 < n_noise;
            let mut io = vec![(0..512).map(|_| if noisy { rnd() * 0.7 } else { 0.0 }).collect::<Vec<f32>>(), vec![0.0; 512]];
            if noisy { io[1] = io[0].clone(); }
            p.process(&mut io, 512);
            let pk = io[0].iter().fold(0.0f32, |m, x| m.max(x.abs()));
            if !noisy && pk > 10f32.powf(-90.0 / 20.0) { last_loud = (b + 1) * 512 - n_noise; }
        }
        println!("{label:20} reaches -90 dBFS after {:.3} s", last_loud as f64 / sr as f64);
    }
}
