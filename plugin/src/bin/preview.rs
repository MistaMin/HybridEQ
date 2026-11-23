//! Renders the editor to an image: `hybrideq-preview <out.ppm> [scale] [overlay] [audio] [window] [defaults]`.
//! The settings are a musical example (`defaults`: none). `window`: the scale sizes the window and
//! pixels are 1:1, as on a 1x screen; otherwise the scale is the pixel density. With `audio`, two seconds of tones and noise run through a real instance
//! bound to the editor, so the analyser, spectrogram and meters show live data.
use hybrideq::editor::HybridEqEditor;
use hybrideq_engine::params::idx;
use openplugin::core::{Editor, EditorHost};
use openplugin::prelude::Plugin;

fn main() {
    let a: Vec<String> = std::env::args().collect();
    let out = a.get(1).cloned().unwrap_or_else(|| "preview.ppm".into());
    let scale: f32 = a.get(2).and_then(|s| s.parse().ok()).unwrap_or(1.0);
    let mut e = HybridEqEditor::new(EditorHost::detached());
    for (i, v) in [
        (idx::LC_FREQ, 40.0), (idx::LOW_GAIN, 3.5), (idx::LOW_FREQ, 90.0), (idx::LOW_TYPE, 1.0),
        (idx::MID1_GAIN, -4.0), (idx::MID1_FREQ, 420.0), (idx::MID1_Q, 1.4),
        (idx::MID2_GAIN, 2.5), (idx::MID2_FREQ, 3200.0), (idx::MID2_TYPE, 2.0),
        (idx::HIGH_GAIN, 4.0), (idx::HIGH_FREQ, 12000.0), (idx::HIGH_TYPE, 2.0),
        (idx::HC_FREQ, 18000.0), (idx::PREAMP_GAIN, 6.0), (idx::PREAMP_CIRCUIT, 1.0), (idx::PREAMP_HQ, 0.0),
        (idx::H2, 0.25), (idx::H3, 0.1), (idx::AUTO_GAIN, 1.0),
    ] {
        if !a.iter().any(|x| x == "defaults") {
            e.set_value(i, v);
        }
    }
    let window = a.iter().any(|x| x == "window");
    let fit = if window { scale } else { 1.0 };
    e.resize((1180.0 * fit).round() as u32, (880.0 * fit).round() as u32);
    if a.get(3).map(String::as_str) == Some("overlay") {
        e.mouse_down(756.0, 38.0, 1);
    }
    if a.iter().any(|x| x == "audio") {
        let mut plugin = hybrideq::HybridEq::default();
        plugin.prepare(48000.0, 1600, 2);
        e.bind(plugin.editor_state());
        let mut seed = 1u32;
        let mut n = 0usize;
        for _ in 0..60 {
            let mut io = vec![vec![0.0f32; 1600], vec![0.0f32; 1600]];
            for i in 0..1600 {
                seed = seed.wrapping_mul(1664525).wrapping_add(1013904223);
                let t = (n + i) as f32 / 48000.0;
                let noise = ((seed >> 8) as f32 / (1 << 24) as f32 - 0.5) * 0.08;
                let x = 0.25 * (6.283 * 110.0 * t).sin() + 0.12 * (6.283 * 880.0 * t).sin() + 0.06 * (6.283 * 5200.0 * t).sin() + noise;
                io[0][i] = x;
                io[1][i] = x * 0.9;
            }
            n += 1600;
            plugin.process(&mut io, 1600);
            e.tick(1.0 / 30.0);
        }
    } else {
        e.set_meters(0.8, [-14.0, -15.0, -11.0, -12.5], -6.4);
    }
    let frame = e.render(if window { 1.0 } else { scale });
    let mut ppm = format!("P6\n{} {}\n255\n", frame.width, frame.height).into_bytes();
    for px in frame.pixels {
        ppm.extend_from_slice(&[(px >> 16) as u8, (px >> 8) as u8, *px as u8]);
    }
    std::fs::write(&out, ppm).expect("write");
}
