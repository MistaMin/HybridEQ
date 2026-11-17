//! Renders the editor to an image: `hybrideq-preview <out.ppm> [scale] [overlay]`. The settings are
//! a musical example so the curve, knobs and meters show something.
use hybrideq::editor::HybridEqEditor;
use hybrideq_engine::params::idx;
use openplugin::core::{Editor, EditorHost};

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
        e.set_value(i, v);
    }
    e.set_meters(0.8, [-14.0, -15.0, -11.0, -12.5], -6.4);
    let (w, h) = (1180, 880);
    e.resize(w, h);
    if a.get(3).map(String::as_str) == Some("overlay") {
        e.mouse_down(756.0, 38.0, 1);
    }
    let frame = e.render(scale);
    let mut ppm = format!("P6\n{} {}\n255\n", frame.width, frame.height).into_bytes();
    for px in frame.pixels {
        ppm.extend_from_slice(&[(px >> 16) as u8, (px >> 8) as u8, *px as u8]);
    }
    std::fs::write(&out, ppm).expect("write");
}
