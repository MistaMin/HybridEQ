//! The editor: GoodLookin DRI 0.2 parts on a `SceneCanvas`, laid out on the 1.6.x design canvas
//! (1180 x 880 units, scaled to the window). The plates, rings and legends are a cached layer; knobs
//! (lit sprites), keys, readouts, the response curve, the analyser and the meters are drawn on top.
//!
//! Every control is a parameter gesture through `EditorHost`; values come back through
//! `param_changed`. Section looks (plate, knob style and colour) follow each band's type or slope,
//! as 1.6.x's Designs/LookTriggers.csv set them.

use std::f32::consts::PI;
use std::sync::Arc;

use hybrideq_engine::engine::apply_eq;
use hybrideq_engine::eq::AnalogEq;
use hybrideq_engine::params::{idx, COUNT, TABLE};
use openplugin::core::editor::{DamageRect, DamageTracker, Modifiers, SizeRange};
use openplugin::core::{Editor, EditorHost, EditorState, Frame};
use openplugin::dri::goodlookindri::color::Color;
use openplugin::dri::goodlookindri::design::{find_faceplate, Faceplate};
use openplugin::dri::goodlookindri::draw::{self, KnobMode};
use openplugin::dri::goodlookindri::hardware::*;
use openplugin::dri::goodlookindri::meters::{self, LampBezel, VuFace};
use openplugin::dri::goodlookindri::scene::{Point, Rect, Scene};
use openplugin::dri::goodlookindri::spectrum_draw::{SpectrumSettings, SpectrumStyle, SpectrumView};
use openplugin::dri::SceneCanvas;

use crate::Shared;

pub const W: f32 = 1180.0;
pub const H: f32 = 880.0;
const F_MIN: f32 = 20.0;
const F_MAX: f32 = 30000.0;
const DB_RANGE: f32 = 24.0;

/// One section's look for a choice: knob cap colour, knob style, plate.
#[derive(Clone, Copy)]
struct Look {
    colour: u32,
    style: &'static str,
    plate: &'static str,
}

const fn look(colour: u32, style: &'static str, plate: &'static str) -> Look {
    Look { colour, style, plate }
}

/// Designs/LookTriggers.csv of 1.6.x, by choice index.
fn section_look(param: usize, choice: usize) -> Look {
    let shelf = |brit: u32| [look(0xffffffff, "Slv", "Black"), look(brit, "Brit", "Granite"), look(0xfff2d21e, "FS", "Royal")];
    let bell = |brit: u32| [look(0xffa32323, "Mic", "Marine"), look(brit, "Brit", "Granite"), look(0xff3f8fd8, "A", "Cream")];
    let pick = |a: &[Look]| a[choice.min(a.len() - 1)];
    match param {
        idx::LOW_TYPE => pick(&shelf(0xffb58c59)),
        idx::HIGH_TYPE => pick(&shelf(0xffb96057)),
        idx::MID1_TYPE => pick(&bell(0xff5793b8)),
        idx::MID2_TYPE => pick(&bell(0xff6ea68b)),
        idx::LC_SLOPE => look(0xffcbcbbc, "Snk", "Graphite"),
        idx::HC_SLOPE => look(if choice < 2 { 0xffaaafa6 } else { 0xffcbcbbc }, "Snk", "Olive"),
        _ => pick(&[
            look(0xff7a7f82, "Snk", "Graphite"),
            look(0xffc76abc, "Brit", "Granite"),
            look(0xffa32323, "Mic", "Marine"),
            look(0xfff2d21e, "FS", "Royal"),
            look(0xff3f8fd8, "A", "Black"),
        ]),
    }
}

fn plate(code: &str) -> &'static Faceplate {
    find_faceplate(code).unwrap_or_else(|| find_faceplate("Graphite").expect("Graphite plate"))
}

struct Strip {
    title: &'static str,
    range: &'static str,
    caption: &'static str,
    look: usize,
    bypass: usize,
    knobs: &'static [(usize, &'static str)],
    keys: [(usize, &'static str); 2],
}

const STRIPS: [Strip; 6] = [
    Strip { title: "LOW CUT", range: "20 Hz - 1 kHz", caption: "FILTER / RESONANCE", look: idx::LC_SLOPE, bypass: idx::LC_BYPASS, knobs: &[(idx::LC_FREQ, "FREQ"), (idx::LC_Q, "Q")], keys: [(idx::LC_SLOPE, "SLOPE"), (idx::LC_MODE, "M/S")] },
    Strip { title: "LOW BAND", range: "20 - 400 Hz", caption: "GAIN / FREQUENCY", look: idx::LOW_TYPE, bypass: idx::LOW_BYPASS, knobs: &[(idx::LOW_GAIN, "GAIN"), (idx::LOW_FREQ, "FREQ")], keys: [(idx::LOW_TYPE, "TYPE"), (idx::LOW_MODE, "M/S")] },
    Strip { title: "MID 1", range: "200 Hz - 6 kHz", caption: "", look: idx::MID1_TYPE, bypass: idx::MID1_BYPASS, knobs: &[(idx::MID1_GAIN, "GAIN"), (idx::MID1_FREQ, "FREQ"), (idx::MID1_Q, "Q")], keys: [(idx::MID1_TYPE, "TYPE"), (idx::MID1_MODE, "M/S")] },
    Strip { title: "MID 2", range: "200 Hz - 6 kHz", caption: "", look: idx::MID2_TYPE, bypass: idx::MID2_BYPASS, knobs: &[(idx::MID2_GAIN, "GAIN"), (idx::MID2_FREQ, "FREQ"), (idx::MID2_Q, "Q")], keys: [(idx::MID2_TYPE, "TYPE"), (idx::MID2_MODE, "M/S")] },
    Strip { title: "HIGH BAND", range: "800 Hz - 30 kHz", caption: "GAIN / FREQUENCY", look: idx::HIGH_TYPE, bypass: idx::HIGH_BYPASS, knobs: &[(idx::HIGH_GAIN, "GAIN"), (idx::HIGH_FREQ, "FREQ")], keys: [(idx::HIGH_TYPE, "TYPE"), (idx::HIGH_MODE, "M/S")] },
    Strip { title: "HIGH CUT", range: "1 - 30 kHz", caption: "FILTER / RESONANCE", look: idx::HC_SLOPE, bypass: idx::HC_BYPASS, knobs: &[(idx::HC_FREQ, "FREQ"), (idx::HC_Q, "Q")], keys: [(idx::HC_SLOPE, "SLOPE"), (idx::HC_MODE, "M/S")] },
];

/// Response-display nodes: label, frequency, gain (or none: the node sits on the curve), Q, strip.
#[allow(clippy::type_complexity)] // a const table read by position, not worth a struct
const NODES: [(&str, usize, Option<usize>, Option<usize>, usize); 6] = [
    ("LC", idx::LC_FREQ, None, Some(idx::LC_Q), 0),
    ("LOW", idx::LOW_FREQ, Some(idx::LOW_GAIN), None, 1),
    ("M1", idx::MID1_FREQ, Some(idx::MID1_GAIN), Some(idx::MID1_Q), 2),
    ("M2", idx::MID2_FREQ, Some(idx::MID2_GAIN), Some(idx::MID2_Q), 3),
    ("HI", idx::HIGH_FREQ, Some(idx::HIGH_GAIN), None, 4),
    ("HC", idx::HC_FREQ, None, Some(idx::HC_Q), 5),
];

// ---- layout (design units) ----
const SCREEN: Rect = Rect { x: 38.0, y: 92.0, w: 1104.0, h: 158.0 };
const STRIP_Y: f32 = 282.0;
const STRIP_H: f32 = 430.0;
const STRIP_W: f32 = 172.0;
const ROW_Y: f32 = 726.0;
const ROW_H: f32 = 128.0;
const PREAMP: Rect = Rect { x: 38.0, y: ROW_Y, w: 280.0, h: ROW_H };
const HARM: Rect = Rect { x: 332.0, y: ROW_Y, w: 212.0, h: ROW_H };
const GLOBAL: Rect = Rect { x: 558.0, y: ROW_Y, w: 300.0, h: ROW_H };
const LEVEL: Rect = Rect { x: 872.0, y: ROW_Y, w: 270.0, h: ROW_H };
const SPECTRUM_KEY: Rect = Rect { x: 700.0, y: 24.0, w: 112.0, h: 28.0 };
const OVERLAY_PLOT: Rect = Rect { x: 34.0, y: 76.0, w: 1112.0, h: 770.0 };
const OVERLAY_MODE: Rect = Rect { x: 902.0, y: 26.0, w: 130.0, h: 28.0 };
const OVERLAY_CLOSE: Rect = Rect { x: 1044.0, y: 26.0, w: 96.0, h: 28.0 };

fn strip_x(i: usize) -> f32 {
    38.0 + i as f32 * (1104.0 - STRIP_W) / 5.0
}

fn knob_centre(strip: usize, slot: usize) -> Point {
    Point::new(strip_x(strip) + STRIP_W * 0.5, STRIP_Y + 100.0 + slot as f32 * 98.0)
}

fn strip_key(strip: usize, row: usize) -> Rect {
    Rect::new(strip_x(strip) + 64.0, STRIP_Y + 356.0 + row as f32 * 36.0, 94.0, 26.0)
}

fn strip_bypass(strip: usize) -> Rect {
    Rect::new(strip_x(strip) + STRIP_W - 40.0, STRIP_Y + 12.0, 28.0, 22.0)
}

const PAD_KEY: Rect = Rect { x: 52.0, y: 792.0, w: 76.0, h: 26.0 };
const PREAMP_KNOB: (f32, f32) = (176.0, 794.0);
const TYPE_KEY: Rect = Rect { x: 222.0, y: 766.0, w: 84.0, h: 24.0 };
const CIRCUIT_KEY: Rect = Rect { x: 222.0, y: 800.0, w: 84.0, h: 30.0 };
const PREAMP_BYPASS: Rect = Rect { x: 278.0, y: 738.0, w: 28.0, h: 22.0 };
const RATE_KEY: Rect = Rect { x: 614.0, y: 768.0, w: 70.0, h: 24.0 };
const FILTER_KEY: Rect = Rect { x: 614.0, y: 804.0, w: 70.0, h: 24.0 };
const AUTO_KEY: Rect = Rect { x: 696.0, y: 768.0, w: 72.0, h: 24.0 };
const DELTA_KEY: Rect = Rect { x: 696.0, y: 804.0, w: 72.0, h: 24.0 };
const OUTPUT_KNOB: (f32, f32) = (812.0, 794.0);
const VU: Rect = Rect { x: 884.0, y: 756.0, w: 132.0, h: 88.0 };
const PEAK: Rect = Rect { x: 1028.0, y: 758.0, w: 78.0, h: 24.0 };
const CLIP: (f32, f32) = (1124.0, 770.0);

fn harm_fader(i: usize) -> Rect {
    Rect::new(HARM.x + 46.0, HARM.y + 40.0 + i as f32 * 21.0, HARM.w - 62.0, 16.0)
}

#[derive(Clone, Copy, PartialEq)]
enum Hit {
    Knob(usize),
    Key(usize),
    Bypass(usize),
    Fader(usize),
    Node(usize),
    Circuit,
    Spectrum,
    OverlayMode,
    OverlayClose,
    Clip,
}

#[derive(Clone, Copy)]
enum Drag {
    Knob { param: usize, y: f32, start: f32 },
    Fader { param: usize, rect: Rect },
    Node { node: usize },
}

/// Meter ballistics: VU (about 300 ms), falling ladders, a held peak and a latching clip lamp.
#[derive(Default)]
struct Meters {
    vu: f32,
    ladder: [f32; 4],
    hold: f32,
    hold_time: f32,
    clip: bool,
}

pub struct HybridEqEditor {
    host: EditorHost,
    shared: Option<Arc<Shared>>,
    v: [f32; COUNT],
    size: (u32, u32),
    canvas: SceneCanvas,
    eq: AnalogEq,
    curve: Vec<f32>,
    curve_dirty: bool,
    spectrum: SpectrumView,
    overlay: Option<SpectrumView>,
    gram: bool,
    meters: Meters,
    drag: Option<Drag>,
    mods: Modifiers,
    zoom: f32,
    /// What to repaint, in design units.
    damage: DamageTracker,
}

pub fn make_editor(host: EditorHost) -> Option<Box<dyn Editor>> {
    Some(Box::new(HybridEqEditor::new(host)))
}

fn analyser_settings(style: SpectrumStyle) -> SpectrumSettings {
    // Designs/Settings.csv of 1.6.x: multicolour, fall and smoothing, the seven range colours.
    let mut s = SpectrumSettings::from_string("1,3,multi,1,3,24,0.8,1,ff8fe0c0,ffff3b4a,ffff9a1f,ffffe23a,ff5be85a,ff35e0d0,ff3f8ff0,ffc05cff,0,0");
    s.on = true;
    s.style = style;
    s.frame = false;
    s.grid = false;
    s.labels = false;
    s
}

impl HybridEqEditor {
    pub fn new(host: EditorHost) -> Self {
        let mut v = [0.0; COUNT];
        for (x, p) in v.iter_mut().zip(TABLE.iter()) {
            *x = p.default;
        }
        let mut s = Self {
            host,
            shared: None,
            v,
            size: (1062, 792),
            canvas: SceneCanvas::new(W, H),
            eq: AnalogEq::new(),
            curve: vec![0.0; 1100],
            curve_dirty: true,
            spectrum: SpectrumView::new(analyser_settings(SpectrumStyle::Multicolour)),
            overlay: None,
            gram: true,
            meters: Meters { hold: -99.0, ladder: [-99.0; 4], ..Meters::default() },
            drag: None,
            mods: Modifiers::NONE,
            zoom: 1062.0 / W,
            damage: DamageTracker::new(),
        };
        s.eq.set_sample_rate(48000.0);
        s
    }

    /// Plain values the editor shows (for tests and the preview tool).
    pub fn set_value(&mut self, index: usize, plain: f32) {
        self.param_changed(index, plain);
    }

    /// Meter state for the preview tool: VU position, ladders (dB) and the held peak.
    pub fn set_meters(&mut self, vu: f32, ladders: [f32; 4], hold: f32) {
        self.meters.vu = vu;
        self.meters.ladder = ladders;
        self.meters.hold = hold;
    }

    fn on(&self, i: usize) -> bool {
        self.v[i] > 0.5
    }

    fn choice(&self, i: usize) -> usize {
        self.v[i] as usize
    }

    fn sample_rate(&self) -> f64 {
        self.shared.as_ref().map_or(48000.0, |s| s.sample_rate())
    }

    fn effective_factor(&self) -> u32 {
        match &self.shared {
            Some(s) => s.factor(),
            None => hybrideq_engine::effective_factor(&self.v) as u32,
        }
    }

    fn set(&mut self, i: usize, plain: f32) {
        let plain = TABLE[i].clamp(plain);
        self.v[i] = plain;
        self.curve_dirty = true;
        self.damage.add_all();
        self.host.set(i, plain);
    }

    fn perform(&mut self, i: usize, plain: f32) {
        let plain = TABLE[i].clamp(plain);
        if plain != self.v[i] {
            self.v[i] = plain;
            self.curve_dirty = true;
        self.damage.add_all();
            self.host.perform(i, plain);
        }
    }

    fn step_choice(&mut self, i: usize, dir: i32) {
        let p = &TABLE[i];
        let n = (p.max - p.min) as i32 + 1;
        let next = ((self.v[i] as i32 - p.min as i32 + dir).rem_euclid(n)) as f32 + p.min;
        self.set(i, next);
    }

    // ---- geometry of the response display ----

    fn x_of(plot: Rect, f: f32) -> f32 {
        plot.x + (f / F_MIN).ln() / (F_MAX / F_MIN).ln() * plot.w
    }

    fn f_of(plot: Rect, x: f32) -> f32 {
        F_MIN * (F_MAX / F_MIN).powf(((x - plot.x) / plot.w).clamp(0.0, 1.0))
    }

    fn y_of(plot: Rect, db: f32) -> f32 {
        plot.y + plot.h * 0.5 - db / DB_RANGE * plot.h * 0.5
    }

    fn db_of(plot: Rect, y: f32) -> f32 {
        (plot.y + plot.h * 0.5 - y) / (plot.h * 0.5) * DB_RANGE
    }

    fn plot(&self) -> Rect {
        if self.overlay.is_some() {
            OVERLAY_PLOT
        } else {
            SCREEN.reduced(6.0, 6.0)
        }
    }

    fn refresh_curve(&mut self) {
        if !self.curve_dirty {
            return;
        }
        self.curve_dirty = false;
        self.eq.set_sample_rate(self.sample_rate());
        apply_eq(&mut self.eq, &self.v);
        let n = self.curve.len();
        for (k, db) in self.curve.iter_mut().enumerate() {
            let f = F_MIN * (F_MAX / F_MIN).powf(k as f32 / (n - 1) as f32);
            *db = self.eq.magnitude_db(f as f64) as f32;
        }
    }

    fn curve_db_at(&self, f: f32) -> f32 {
        let n = self.curve.len();
        let k = ((f / F_MIN).ln() / (F_MAX / F_MIN).ln() * (n - 1) as f32).round().clamp(0.0, (n - 1) as f32) as usize;
        self.curve[k]
    }

    fn node_point(&self, plot: Rect, node: usize) -> Point {
        let (_, f, g, _, _) = NODES[node];
        let freq = self.v[f];
        let db = g.map_or_else(|| self.curve_db_at(freq), |g| self.v[g]);
        Point::new(Self::x_of(plot, freq), Self::y_of(plot, db.clamp(-DB_RANGE, DB_RANGE)))
    }

    // ---- hit testing ----

    fn hit(&self, p: Point) -> Option<Hit> {
        if self.overlay.is_some() {
            if OVERLAY_CLOSE.contains(p) {
                return Some(Hit::OverlayClose);
            }
            if OVERLAY_MODE.contains(p) {
                return Some(Hit::OverlayMode);
            }
            return self.node_hit(p, 16.0);
        }
        if SPECTRUM_KEY.contains(p) {
            return Some(Hit::Spectrum);
        }
        if SCREEN.contains(p) {
            return self.node_hit(p, 22.0);
        }
        for (s, strip) in STRIPS.iter().enumerate() {
            if strip_bypass(s).contains(p) {
                return Some(Hit::Bypass(strip.bypass));
            }
            for (slot, (param, _)) in strip.knobs.iter().enumerate() {
                let c = knob_centre(s, slot);
                if (p.x - c.x).hypot(p.y - c.y) < 34.0 {
                    return Some(Hit::Knob(*param));
                }
            }
            for (row, (param, _)) in strip.keys.iter().enumerate() {
                if strip_key(s, row).contains(p) {
                    return Some(Hit::Key(*param));
                }
            }
        }
        let near = |c: (f32, f32), r: f32| (p.x - c.0).hypot(p.y - c.1) < r;
        if near(PREAMP_KNOB, 28.0) {
            return Some(Hit::Knob(idx::PREAMP_GAIN));
        }
        if near(OUTPUT_KNOB, 28.0) {
            return Some(Hit::Knob(idx::OUTPUT_GAIN));
        }
        if near(CLIP, 10.0) {
            return Some(Hit::Clip);
        }
        let keys = [
            (PAD_KEY, Hit::Key(idx::PREAMP_PAD)),
            (TYPE_KEY, Hit::Key(idx::PREAMP_TYPE)),
            (CIRCUIT_KEY, Hit::Circuit),
            (PREAMP_BYPASS, Hit::Bypass(idx::PREAMP_BYPASS)),
            (RATE_KEY, Hit::Key(idx::OVERSAMPLE_MODE)),
            (FILTER_KEY, Hit::Key(idx::OVERSAMPLE_FILTER)),
            (AUTO_KEY, Hit::Bypass(idx::AUTO_GAIN)),
            (DELTA_KEY, Hit::Bypass(idx::DELTA)),
        ];
        if let Some((_, h)) = keys.iter().find(|(r, _)| r.contains(p)) {
            return Some(*h);
        }
        (0..4).find(|&i| harm_fader(i).expanded(3.0).contains(p)).map(|i| Hit::Fader(idx::H2 + i))
    }

    fn node_hit(&self, p: Point, radius: f32) -> Option<Hit> {
        let plot = self.plot();
        (0..NODES.len())
            .map(|n| (n, self.node_point(plot, n)))
            .map(|(n, c)| (n, (c.x - p.x).hypot(c.y - p.y)))
            .filter(|(_, d)| *d < radius)
            .min_by(|a, b| a.1.total_cmp(&b.1))
            .map(|(n, _)| Hit::Node(n))
    }

    fn to_design(&self, x: f32, y: f32) -> Point {
        Point::new(x / self.zoom, y / self.zoom)
    }

    // ---- painting ----

    fn layer_key(&self) -> u64 {
        let mut k = u64::from(self.overlay.is_some());
        for i in [idx::LC_SLOPE, idx::HC_SLOPE, idx::LOW_TYPE, idx::MID1_TYPE, idx::MID2_TYPE, idx::HIGH_TYPE, idx::PREAMP_TYPE] {
            k = k * 8 + self.choice(i) as u64;
        }
        k
    }

    fn paint_static(v: &[f32; COUNT], overlay: bool, gram: bool, sc: &mut Scene) {
        let choice = |i: usize| v[i] as usize;
        let chassis = plate("Graphite");
        let ink = Color(chassis.text);
        let mid = Color(chassis.text_mid);
        let rig = openplugin::dri::goodlookindri::light::LightRig::studio();
        // chassis: full width, no ears or screws
        draw_panel(sc, Rect::new(0.0, 0.0, W, H), Color(chassis.bg_top), Color(chassis.bg_bottom), FINISH_POWDER);
        if overlay {
            let body = Rect::new(26.0, 14.0, W - 52.0, H - 28.0);
            sc.fill_round(body, 8.0, Color(0xff0c1114), Color(0xff080b0d), true);
            legend_text(sc, Legend::Silk(ink), Point::new(40.0, 40.0), "SPECTRUM", 20.0, false);
            let caption = if gram { "SPECTROGRAM  /  TIME DOWN  /  NEWEST AT TOP" } else { "FREQUENCY ANALYSER  /  0 TO -90 dBFS" };
            legend_text(sc, Legend::Silk(mid), Point::new(160.0, 41.0), caption, 9.0, false);
            Self::paint_scale(sc, OVERLAY_PLOT, mid, true);
            return;
        }

        // header bar
        let header = Rect::new(30.0, 14.0, W - 60.0, 52.0);
        sc.fill_round(header, 6.0, Color(chassis.title_bar).brighter(0.06), Color(chassis.title_bar), true);
        sc.stroke_round(header, 6.0, Color(0x30ffffff), 1.0);
        legend_text_on(sc, Legend::Engraved(Color(0xffe9e3d0)), Color(chassis.title_bar), Point::new(48.0, 42.0), "Hybrid EQ", 27.0, false);
        legend_text(sc, Legend::Silk(mid), Point::new(300.0, 32.0), "O P E N G R I D", 9.0, false);
        legend_text(sc, Legend::Silk(mid), Point::new(300.0, 49.0), "SIX-BAND CONSOLE EQUALISER", 8.5, false);
        legend_text(sc, Legend::Silk(Color(chassis.accent)), Point::new(900.0, 42.0), "ANALOGUE CHARACTER  /  DIGITAL PRECISION", 8.5, false);

        // response display
        legend_text(sc, Legend::Silk(mid), Point::new(40.0, 82.0), "FREQUENCY RESPONSE", 8.5, false);
        legend_text(sc, Legend::Silk(mid), Point::new(1004.0, 82.0), "20 Hz - 30 kHz   /   +/-24 dB", 8.5, false);
        let screen = SpectrumSettings { frame: true, grid: false, ..Default::default() };
        SpectrumView::paint_screen_back(sc, SCREEN, &screen, F_MIN, F_MAX);
        Self::paint_scale(sc, SCREEN.reduced(6.0, 6.0), mid, false);

        draw_legend_line(sc, mid, 40.0, W - 40.0, 268.0, "EQUALISER", 8.5);

        for (s, strip) in STRIPS.iter().enumerate() {
            let lk = section_look(strip.look, choice(strip.look));
            let fp = plate(lk.plate);
            let r = Rect::new(strip_x(s), STRIP_Y, STRIP_W, STRIP_H);
            sc.shadow(openplugin::dri::goodlookindri::scene::Shape::Round(r, 7.0), 6.0, Color(0x90000000));
            draw_panel_with(sc, r, 7.0, Color(fp.panel_top), Color(fp.panel_bottom), fp.finish, &rig);
            let ink = Color(fp.text);
            let silk = Legend::Silk(ink);
            sc.fill_rect(Rect::new(r.x + 10.0, r.y + 13.0, 3.0, 18.0), Color(lk.colour), Color(lk.colour).darker(0.3), true);
            legend_text(sc, silk, Point::new(r.x + 20.0, r.y + 25.0), strip.title, 12.5, false);
            legend_text(sc, Legend::Silk(Color(fp.text_mid)), Point::new(r.x + 20.0, r.y + 41.0), strip.range, 8.5, false);
            legend_line(sc, Legend::Silk(Color(fp.text_mid).with_alpha(0.5)), Point::new(r.x + 12.0, r.y + 54.0), Point::new(r.right() - 12.0, r.y + 54.0), 1.0);
            for (slot, (param, name)) in strip.knobs.iter().enumerate() {
                let c = knob_centre(s, slot);
                Self::paint_knob_ring(sc, c, 27.0, *param, name, ink, Color(fp.text_mid));
            }
            if !strip.caption.is_empty() {
                legend_text(sc, Legend::Silk(Color(fp.text_mid)), Point::new(r.x + STRIP_W * 0.5, STRIP_Y + 292.0), strip.caption, 8.5, true);
            }
            for (row, (_, label)) in strip.keys.iter().enumerate() {
                let k = strip_key(s, row);
                legend_text(sc, silk, Point::new(r.x + 16.0, k.y + k.h * 0.5), label, 9.5, false);
            }
        }

        // bottom row
        let pl = section_look(idx::PREAMP_TYPE, choice(idx::PREAMP_TYPE));
        let sections = [(PREAMP, "PREAMP", "INPUT / CIRCUIT COLOUR", plate(pl.plate)), (HARM, "HARMONICS", "2ND TO 5TH ORDER", plate("Graphite")), (GLOBAL, "GLOBAL", "OVERSAMPLING / OUTPUT", plate("Graphite")), (LEVEL, "LEVEL", "OUTPUT RMS / PEAK", plate("Graphite"))];
        for (r, title, sub, fp) in sections {
            sc.shadow(openplugin::dri::goodlookindri::scene::Shape::Round(r, 7.0), 6.0, Color(0x90000000));
            draw_panel_with(sc, r, 7.0, Color(fp.panel_top), Color(fp.panel_bottom), fp.finish, &rig);
            legend_text(sc, Legend::Silk(Color(fp.text)), Point::new(r.x + 14.0, r.y + 20.0), title, 11.5, false);
            legend_text(sc, Legend::Silk(Color(fp.text_mid)), Point::new(r.x + 14.0, r.y + 34.0), sub, 8.5, false);
        }
        let pfp = plate(pl.plate);
        let pink = Legend::Silk(Color(pfp.text));
        legend_text(sc, pink, Point::new(PAD_KEY.x + PAD_KEY.w * 0.5, PAD_KEY.y - 10.0), "PAD", 9.0, true);
        Self::paint_knob_ring(sc, Point::new(PREAMP_KNOB.0, PREAMP_KNOB.1), 20.0, idx::PREAMP_GAIN, "GAIN", Color(pfp.text), Color(pfp.text_mid));
        let g = Legend::Silk(ink);
        for (i, name) in ["2nd", "3rd", "4th", "5th"].iter().enumerate() {
            let f = harm_fader(i);
            legend_text(sc, g, Point::new(HARM.x + 16.0, f.y + f.h * 0.5), name, 9.0, false);
        }
        legend_text(sc, g, Point::new(GLOBAL.x + 14.0, RATE_KEY.y + 12.0), "RATE", 9.0, false);
        legend_text(sc, g, Point::new(GLOBAL.x + 14.0, FILTER_KEY.y + 12.0), "FILTER", 9.0, false);
        Self::paint_knob_ring(sc, Point::new(OUTPUT_KNOB.0, OUTPUT_KNOB.1), 20.0, idx::OUTPUT_GAIN, "OUTPUT", ink, mid);
        legend_text(sc, Legend::Silk(mid), Point::new(PEAK.x + PEAK.w * 0.5, PEAK.bottom() + 9.0), "PEAK dBFS", 8.5, true);
        legend_text(sc, Legend::Silk(mid), Point::new(CLIP.0, CLIP.1 + 15.0), "CLIP", 8.5, true);
        legend_text(sc, Legend::Silk(mid), Point::new(1028.0, 806.0), "IN", 8.5, false);
        legend_text(sc, Legend::Silk(mid), Point::new(1028.0, 829.0), "OUT", 8.5, false);

        legend_text(sc, Legend::Silk(Color(chassis.footer)), Point::new(40.0, 868.0), "OPENGRID   /   HYBRID EQ", 8.5, false);
        legend_text(sc, Legend::Silk(Color(chassis.footer)), Point::new(1010.0, 868.0), &format!("CONSOLE EDITION   /   v{}", crate::INFO.version), 8.5, false);
    }

    /// The frequency and gain graticule of a response plot.
    fn paint_scale(sc: &mut Scene, plot: Rect, ink: Color, overlay: bool) {
        let grid = Color(0x1cffffff);
        for f in [50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0, 20000.0] {
            let x = Self::x_of(plot, f);
            sc.line(Point::new(x, plot.y), Point::new(x, plot.bottom()), grid, 1.0);
            let label = if f >= 1000.0 { format!("{}k", f / 1000.0) } else { format!("{f}") };
            sc.text(Point::new(x, plot.bottom() - 8.0), label, ink.with_alpha(0.7), 8.5, true);
        }
        for db in [-18.0, -12.0, -6.0, 0.0, 6.0, 12.0, 18.0] {
            let y = Self::y_of(plot, db);
            let c = if db == 0.0 { Color(0x40ffffff) } else { grid };
            sc.line(Point::new(plot.x, y), Point::new(plot.right(), y), c, 1.0);
            if overlay || db % 12.0 == 0.0 {
                sc.text(Point::new(plot.right() - 22.0, y - 6.0), format!("{db:+}"), ink.with_alpha(0.6), 8.5, true);
            }
        }
    }

    /// Printed scale round a knob: dots, the end values, and the name above.
    fn paint_knob_ring(sc: &mut Scene, c: Point, r: f32, param: usize, name: &str, ink: Color, mid: Color) {
        let p = &TABLE[param];
        let mut ring = ScaleRing::new(c, r + 9.0, 11, 0, &[]);
        ring.legend = Legend::Silk(mid);
        ring.marks = ScaleMarks::Dots;
        ring.tick = 2.2;
        draw_scale_ring_with(sc, &ring);
        if r <= 24.0 {
            legend_text(sc, Legend::Silk(ink), Point::new(c.x, c.y - r - 17.5), name, 8.5, true);
        } else {
            // band knobs stack: the name sits in the ring's top-left corner so it never meets the readout above
            legend_text(sc, Legend::Silk(ink), Point::new(c.x - 70.0, c.y - r - 7.0), name, 10.0, false);
            let short = |v: f32| match p.unit {
                hybrideq_engine::params::Unit::Hz if v >= 1000.0 => format!("{}k", v / 1000.0),
                hybrideq_engine::params::Unit::Db => format!("{v:+.1}"),
                hybrideq_engine::params::Unit::Q => hybrideq_engine::params::format_value(p.unit, v),
                _ => format!("{v}"),
            };
            legend_text(sc, Legend::Silk(mid), Point::new(c.x - 46.0, c.y + r + 15.0), &short(p.min), 8.5, true);
            legend_text(sc, Legend::Silk(mid), Point::new(c.x + 46.0, c.y + r + 15.0), &short(p.max), 8.5, true);
        }
    }

    #[allow(clippy::too_many_arguments)] // each is a distinct drawing input
    fn paint_knob(&self, sc: &mut Scene, c: Point, r: f32, param: usize, look: Look, active: bool, readout: bool) {
        let t = TABLE[param].to_normalized(self.v[param]);
        let angle = (t * 1.6 - 0.8) * PI;
        let colour = if active { Color(look.colour) } else { Color(look.colour).mix(Color(0xff4a4f52), 0.6) };
        // The A-Type body is small for its radius (its `visual_frame` reach is the pointer tip, so
        // that cannot normalise it): draw it larger, lowered by its lift so it stays centred in the ring.
        let (kr, ky) = match look.style {
            "A" => (r * 1.18, r * 0.083),
            // N-EQ (Mic) reads slightly small beside the other styles too
            "Mic" => (r * 1.06, 0.0),
            _ => (r, 0.0),
        };
        draw::knob_named_mode(sc, look.style, Point::new(c.x, c.y + ky), kr, angle, colour, false, KnobMode::Lit);
        if readout {
            let box_ = Rect::new(c.x - 28.0, c.y + r + 7.0, 56.0, 16.0);
            sc.fill_round(box_, 3.0, Color(0xff080b0d), Color(0xff11161a), true);
            sc.stroke_round(box_, 3.0, Color(0x22ffffff), 1.0);
            let text = TABLE[param].format(self.v[param]);
            sc.text(Point::new(box_.center().x, box_.center().y), text, if active { Color(0xffe8e2cf) } else { Color(0xff6c7276) }, 9.5, true);
        }
    }

    fn paint_key(&self, sc: &mut Scene, r: Rect, text: &str, active: bool) {
        let base = if active { Color(0xffd9d2ba) } else { Color(0xff8e8c84) };
        draw_key(sc, r, base, false);
        sc.text(r.center(), text, Color(0xff1d2124), 9.5, true);
    }

    fn paint_dynamic(&mut self, sc: &mut Scene) {
        self.refresh_curve();
        if let Some(view) = self.overlay.as_mut() {
            view.paint(sc, OVERLAY_PLOT, F_MIN, F_MAX);
            let mode = if self.gram { "SPECTRUM" } else { "SPECTROGRAM" };
            self.paint_key(sc, OVERLAY_MODE, mode, true);
            self.paint_key(sc, OVERLAY_CLOSE, "CLOSE", true);
            self.paint_curve(sc, OVERLAY_PLOT, false);
            return;
        }
        let plot = SCREEN.reduced(6.0, 6.0);
        self.spectrum.paint(sc, plot, F_MIN, F_MAX);
        self.paint_curve(sc, plot, true);
        let glass = SpectrumSettings { frame: true, ..Default::default() };
        SpectrumView::paint_screen_front(sc, SCREEN, &glass);
        meters::draw_lit_key(sc, SPECTRUM_KEY, "SPECTRUM", false, Color(0xff35e0d0), false);

        for (s, strip) in STRIPS.iter().enumerate() {
            let lk = section_look(strip.look, self.choice(strip.look));
            let active = !self.on(strip.bypass);
            for (slot, (param, _)) in strip.knobs.iter().enumerate() {
                self.paint_knob(sc, knob_centre(s, slot), 27.0, *param, lk, active, true);
            }
            for (row, (param, _)) in strip.keys.iter().enumerate() {
                self.paint_key(sc, strip_key(s, row), &TABLE[*param].format(self.v[*param]), active);
            }
            draw_led_key(sc, strip_bypass(s), Color(lk.colour).brighter(0.2), active, false);
        }

        // preamp
        let pl = section_look(idx::PREAMP_TYPE, self.choice(idx::PREAMP_TYPE));
        let preamp_on = !self.on(idx::PREAMP_BYPASS);
        self.paint_key(sc, PAD_KEY, &TABLE[idx::PREAMP_PAD].format(self.v[idx::PREAMP_PAD]), preamp_on);
        self.paint_knob(sc, Point::new(PREAMP_KNOB.0, PREAMP_KNOB.1), 20.0, idx::PREAMP_GAIN, pl, preamp_on, false);
        let gain = TABLE[idx::PREAMP_GAIN].format(self.v[idx::PREAMP_GAIN]);
        sc.text(Point::new(PREAMP_KNOB.0, PREAMP_KNOB.1 + 38.0), gain, Color(plate(pl.plate).text), 8.5, true);
        self.paint_key(sc, TYPE_KEY, &TABLE[idx::PREAMP_TYPE].format(self.v[idx::PREAMP_TYPE]), preamp_on);
        let mode = match (self.on(idx::PREAMP_CIRCUIT), self.on(idx::PREAMP_HQ)) {
            (false, _) => 0,
            (true, false) => 1,
            (true, true) => 2,
        };
        meters::draw_lit_key(sc, CIRCUIT_KEY, if mode == 2 { "CIRCUIT HQ" } else { "CIRCUIT" }, mode > 0 && preamp_on, Color(pl.colour), false);
        draw_led_key(sc, PREAMP_BYPASS, Color(pl.colour).brighter(0.2), preamp_on, false);

        // harmonics
        for i in 0..4 {
            let p = idx::H2 + i;
            meters::draw_fader(sc, harm_fader(i), self.v[p], meters::FaderCap::Console, Color(0xffd6ff2e), false, false);
        }

        // global
        let factor = self.effective_factor();
        let chosen = [1, 2, 4, 8][self.choice(idx::OVERSAMPLE_MODE).min(3)];
        let rate = if factor == chosen { format!("{factor}x") } else { format!("{factor}x*") };
        self.paint_key(sc, RATE_KEY, &rate, true);
        let filter = if self.choice(idx::OVERSAMPLE_FILTER) == 1 { "FIR (Lin)" } else { "IIR (Min)" };
        self.paint_key(sc, FILTER_KEY, filter, factor > 1);
        meters::draw_lit_key(sc, AUTO_KEY, "AUTO GAIN", self.on(idx::AUTO_GAIN), Color(0xff5be85a), false);
        meters::draw_lit_key(sc, DELTA_KEY, "DELTA", self.on(idx::DELTA), Color(0xffffa52e), false);
        self.paint_knob(sc, Point::new(OUTPUT_KNOB.0, OUTPUT_KNOB.1), 20.0, idx::OUTPUT_GAIN, look(0xffb9bdb3, "Brit", "Graphite"), true, false);
        let out = TABLE[idx::OUTPUT_GAIN].format(self.v[idx::OUTPUT_GAIN]);
        sc.text(Point::new(OUTPUT_KNOB.0, OUTPUT_KNOB.1 + 38.0), out, Color(0xffdde1d5), 8.5, true);

        // level
        let m = &self.meters;
        meters::draw_vu_meter_lit(sc, VU, meters::vu_position(m.vu), VuFace::Cream, 1.0);
        let hold = if m.hold <= -90.0 { "--".to_string() } else { format!("{:.1}", m.hold) };
        meters::draw_seven_segment(sc, PEAK, &hold, Color(0xff5be85a), true);
        meters::draw_jewel(sc, Point::new(CLIP.0, CLIP.1), 6.0, Color(0xffff3b30), if m.clip { 1.0 } else { 0.0 }, LampBezel::Chrome);
        for ch in 0..2 {
            meters::draw_led_ladder(sc, Rect::new(1052.0, 798.0 + ch as f32 * 8.0, 82.0, 6.0), true, m.ladder[ch], m.ladder[ch], 18);
            meters::draw_led_ladder(sc, Rect::new(1052.0, 821.0 + ch as f32 * 8.0, 82.0, 6.0), true, m.ladder[2 + ch], m.hold, 18);
        }
    }

    fn paint_curve(&self, sc: &mut Scene, plot: Rect, fill: bool) {
        let n = self.curve.len();
        let pts: Vec<Point> = (0..n)
            .map(|k| Point::new(plot.x + plot.w * k as f32 / (n - 1) as f32, Self::y_of(plot, self.curve[k].clamp(-DB_RANGE - 2.0, DB_RANGE + 2.0))))
            .collect();
        if fill {
            let mut poly = pts.clone();
            let y0 = Self::y_of(plot, 0.0);
            poly.push(Point::new(plot.right(), y0));
            poly.push(Point::new(plot.x, y0));
            sc.fill_poly(poly, Color(0x30b8f0d8), Color(0x08b8f0d8));
        }
        sc.stroke_poly(pts.clone(), Color(0x70000000), 3.5, false);
        sc.stroke_poly(pts, Color(0xffc8f3e0), 1.6, false);
        for (node, &(label, _, _, _, strip)) in NODES.iter().enumerate() {
            let lk = section_look(STRIPS[strip].look, self.choice(STRIPS[strip].look));
            let c = self.node_point(plot, node);
            let active = !self.on(STRIPS[strip].bypass);
            let col = if active { Color(lk.colour) } else { Color(0xff666b6e) };
            sc.glow_ellipse(Scene::ellipse_at(c, 9.0), col.with_alpha(0.35), 5.0, openplugin::dri::goodlookindri::scene::Blend::Add);
            sc.fill_ellipse(Scene::ellipse_at(c, 6.0), col.brighter(0.25), col.darker(0.2));
            sc.stroke_ellipse(Scene::ellipse_at(c, 6.0), Color(0xccffffff), 1.0);
            let pill = Rect::new(c.x - 15.0, (c.y - 25.0).max(plot.y + 1.0), 30.0, 13.0);
            sc.fill_round(pill, 4.0, col.with_alpha(0.92), col.darker(0.15).with_alpha(0.92), true);
            let ink = if col.perceived_brightness() > 0.55 { Color(0xff101216) } else { Color(0xfff2f2f2) };
            sc.text(pill.center(), label, ink, 8.5, true);
        }
    }
}

impl Editor for HybridEqEditor {
    fn size(&self) -> (u32, u32) {
        self.size
    }

    fn size_range(&self) -> SizeRange {
        SizeRange::between((826, 616), (1770, 1320)).with_aspect(1180, 880)
    }

    fn resize(&mut self, width: u32, height: u32) {
        self.size = (width, height);
        self.zoom = width as f32 / W;
        self.canvas.invalidate_layer();
        self.damage.add_all();
        if let Some(s) = &self.shared {
            s.set_editor_size(width, height);
        }
    }

    fn bind(&mut self, state: Option<EditorState>) {
        self.shared = state.and_then(|s| s.downcast::<Shared>().ok());
        // The size saved with the project wins over the default.
        if let Some((w, h)) = self.shared.as_ref().and_then(|s| s.editor_size()) {
            if (w, h) != self.size && self.host.request_resize(&self.size_range(), w, h) {
                self.resize(w, h);
            }
        }
        self.curve_dirty = true;
        self.damage.add_all();
    }

    fn param_changed(&mut self, index: usize, plain: f32) {
        if index < COUNT {
            self.v[index] = TABLE[index].clamp(plain);
            self.curve_dirty = true;
        self.damage.add_all();
        }
    }

    fn tick(&mut self, dt: f32) {
        let Some(shared) = self.shared.clone() else { return };
        let peaks = shared.take_peaks();
        let power = shared.take_power();
        let sr = shared.sample_rate();
        if let Some(view) = self.overlay.as_mut() {
            view.update(&shared.ring, dt, sr);
        } else {
            self.spectrum.update(&shared.ring, dt, sr);
        }
        let m = &mut self.meters;
        let db = |a: f32| if a > 1e-5 { 20.0 * a.log10() } else { -99.0 };
        for (i, p) in peaks.iter().enumerate() {
            m.ladder[i] = db(*p).max(m.ladder[i] - 20.0 * dt);
        }
        // VU: 0 VU = -18 dBFS, about 300 ms to 99 %
        let rms = ((power[0] + power[1]) * 0.5).sqrt() / 0.12589;
        m.vu += (rms - m.vu) * (1.0 - (-dt / 0.065).exp());
        let out = db(peaks[2].max(peaks[3]));
        if out >= m.hold {
            m.hold = out;
            m.hold_time = 1.5;
        } else {
            m.hold_time -= dt;
            if m.hold_time <= 0.0 {
                m.hold = (m.hold - 12.0 * dt).max(-99.0);
            }
        }
        if peaks[2].max(peaks[3]) >= 0.999 {
            m.clip = true;
        }
        if self.overlay.is_some() {
            self.damage.add_all();
        } else {
            for r in [SCREEN.expanded(8.0), LEVEL.expanded(4.0)] {
                self.damage.add(DamageRect::new(r.x, r.y, r.w, r.h));
            }
        }
    }

    fn damage(&mut self) -> Option<Vec<DamageRect>> {
        let z = self.zoom;
        self.damage.pending().map(|rects| rects.iter().map(|r| DamageRect::new(r.x * z, r.y * z, r.w * z, r.h * z)).collect())
    }

    fn render(&mut self, scale: f32) -> Frame<'_> {
        let key = self.layer_key();
        let pending = self.damage.pending().map(<[DamageRect]>::to_vec);
        self.damage.clear();
        let mut top = Scene::new();
        if pending.as_ref().is_none_or(|d| !d.is_empty()) {
            self.paint_dynamic(&mut top);
        }
        draw::set_knob_floor(Color(0xff2c3236));
        // The static layer is built only when the canvas needs it (look, size or scale changed).
        let (v, overlay, gram) = (self.v, self.overlay.is_some(), self.gram);
        let layer = move |dl: &mut openplugin::dri::DrawList| {
            let mut sc = Scene::new();
            Self::paint_static(&v, overlay, gram, &mut sc);
            dl.append(sc);
        };
        self.canvas.frame_damaged(scale * self.zoom, key, layer, |dl| dl.append(top), pending.as_deref())
    }

    fn modifiers_changed(&mut self, modifiers: Modifiers) {
        self.mods = modifiers;
    }

    fn mouse_down(&mut self, x: f32, y: f32, clicks: u32) {
        let p = self.to_design(x, y);
        self.damage.add_all();
        match self.hit(p) {
            Some(Hit::Knob(i)) if clicks >= 2 => self.set(i, TABLE[i].default),
            Some(Hit::Knob(i)) => {
                self.host.begin(i);
                self.drag = Some(Drag::Knob { param: i, y: p.y, start: TABLE[i].to_normalized(self.v[i]) });
            }
            Some(Hit::Fader(i)) => {
                self.host.begin(i);
                let rect = harm_fader(i - idx::H2);
                self.drag = Some(Drag::Fader { param: i, rect });
                self.mouse_move(x, y);
            }
            Some(Hit::Node(n)) => {
                let (_, f, g, _, _) = NODES[n];
                self.host.begin(f);
                if let Some(g) = g {
                    self.host.begin(g);
                }
                self.drag = Some(Drag::Node { node: n });
            }
            Some(Hit::Key(i)) => self.step_choice(i, 1),
            Some(Hit::Bypass(i)) => self.set(i, if self.on(i) { 0.0 } else { 1.0 }),
            Some(Hit::Circuit) => {
                // Off -> Circuit -> Circuit HQ -> Off; the engine is chosen before the circuit is switched on.
                let next = match (self.on(idx::PREAMP_CIRCUIT), self.on(idx::PREAMP_HQ)) {
                    (false, _) => 1,
                    (true, false) => 2,
                    (true, true) => 0,
                };
                if next != 0 {
                    self.set(idx::PREAMP_HQ, if next == 2 { 1.0 } else { 0.0 });
                }
                self.set(idx::PREAMP_CIRCUIT, if next != 0 { 1.0 } else { 0.0 });
            }
            Some(Hit::Spectrum) => {
                self.overlay = Some(SpectrumView::new(analyser_settings(if self.gram { SpectrumStyle::Spectrogram } else { SpectrumStyle::Multicolour })));
            }
            Some(Hit::OverlayMode) => {
                self.gram = !self.gram;
                self.overlay = Some(SpectrumView::new(analyser_settings(if self.gram { SpectrumStyle::Spectrogram } else { SpectrumStyle::Multicolour })));
            }
            Some(Hit::OverlayClose) => self.overlay = None,
            Some(Hit::Clip) => {
                self.meters.clip = false;
                self.meters.hold = -99.0;
            }
            None => {}
        }
    }

    fn mouse_move(&mut self, x: f32, y: f32) {
        let p = self.to_design(x, y);
        match self.drag {
            Some(Drag::Knob { param, y: y0, start }) => {
                let travel = if self.mods.shift { 900.0 } else { 180.0 };
                let t = (start + (y0 - p.y) / travel).clamp(0.0, 1.0);
                self.perform(param, TABLE[param].from_normalized(t));
            }
            Some(Drag::Fader { param, rect }) => {
                let t = ((p.x - rect.x) / rect.w).clamp(0.0, 1.0);
                self.perform(param, t);
            }
            Some(Drag::Node { node }) => {
                let plot = self.plot();
                let (_, f, g, _, _) = NODES[node];
                self.perform(f, Self::f_of(plot, p.x));
                if let Some(g) = g {
                    self.perform(g, Self::db_of(plot, p.y));
                }
            }
            None => {}
        }
    }

    fn mouse_up(&mut self, _x: f32, _y: f32) {
        match self.drag.take() {
            Some(Drag::Knob { param, .. }) | Some(Drag::Fader { param, .. }) => self.host.end(param),
            Some(Drag::Node { node }) => {
                let (_, f, g, _, _) = NODES[node];
                self.host.end(f);
                if let Some(g) = g {
                    self.host.end(g);
                }
            }
            None => {}
        }
    }

    fn right_mouse_down(&mut self, x: f32, y: f32) -> bool {
        // Choice keys step back on a right click (left steps forward); other controls open the host's
        // parameter menu (automation, MIDI learn).
        // shortcut: the SDK's fallback menu is not wired, so a host without a menu shows nothing; add it if one matters.
        let param = match self.hit(self.to_design(x, y)) {
            Some(Hit::Key(i)) => {
                self.step_choice(i, -1);
                return true;
            }
            Some(Hit::Knob(i) | Hit::Fader(i) | Hit::Bypass(i)) => i,
            Some(Hit::Node(n)) => NODES[n].1,
            Some(Hit::Circuit) => idx::PREAMP_CIRCUIT,
            _ => return false,
        };
        self.host.context_menu(param, x, y);
        true
    }

    fn wheel(&mut self, x: f32, y: f32, dy: f32) {
        let p = self.to_design(x, y);
        let q = match self.hit(p) {
            Some(Hit::Node(n)) => NODES[n].3,
            Some(Hit::Knob(i)) => Some(i),
            _ => None,
        };
        if let Some(i) = q {
            let t = (TABLE[i].to_normalized(self.v[i]) + dy * 0.02).clamp(0.0, 1.0);
            self.set(i, TABLE[i].from_normalized(t));
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use openplugin::core::Gesture;
    use std::cell::RefCell;
    use std::rc::Rc;

    /// An editor at design size (one point per design unit) that records its gestures.
    fn editor() -> (HybridEqEditor, Rc<RefCell<Vec<Gesture>>>) {
        let log = Rc::new(RefCell::new(Vec::new()));
        let sink = log.clone();
        let mut e = HybridEqEditor::new(EditorHost::new(move |g| sink.borrow_mut().push(g)));
        e.resize(W as u32, H as u32);
        (e, log)
    }

    fn params_touched(log: &Rc<RefCell<Vec<Gesture>>>) -> Vec<u32> {
        let mut ids: Vec<u32> = log.borrow_mut().drain(..).filter_map(|g| match g {
            Gesture::Begin(i) | Gesture::Perform(i, _) | Gesture::End(i) => Some(i),
            _ => None,
        }).collect();
        ids.dedup();
        ids
    }

    #[test]
    fn every_control_reaches_its_own_parameter() {
        let (mut e, log) = editor();
        let mut reached = std::collections::BTreeSet::new();
        let mut click = |p: Point, expect: usize| {
            e.mouse_down(p.x, p.y, 1);
            e.mouse_up(p.x, p.y);
            assert_eq!(params_touched(&log), [expect as u32], "click at {:?}", (p.x, p.y));
            reached.insert(expect);
        };
        for (s, strip) in STRIPS.iter().enumerate() {
            for (slot, (param, _)) in strip.knobs.iter().enumerate() {
                click(knob_centre(s, slot), *param);
            }
            for (row, (param, _)) in strip.keys.iter().enumerate() {
                click(strip_key(s, row).center(), *param);
            }
            click(strip_bypass(s).center(), strip.bypass);
        }
        click(Point::new(PREAMP_KNOB.0, PREAMP_KNOB.1), idx::PREAMP_GAIN);
        click(Point::new(OUTPUT_KNOB.0, OUTPUT_KNOB.1), idx::OUTPUT_GAIN);
        for (r, p) in [(PAD_KEY, idx::PREAMP_PAD), (TYPE_KEY, idx::PREAMP_TYPE), (PREAMP_BYPASS, idx::PREAMP_BYPASS), (RATE_KEY, idx::OVERSAMPLE_MODE), (FILTER_KEY, idx::OVERSAMPLE_FILTER), (AUTO_KEY, idx::AUTO_GAIN), (DELTA_KEY, idx::DELTA)] {
            click(r.center(), p);
        }
        for i in 0..4 {
            click(harm_fader(i).center(), idx::H2 + i);
        }
        // The CIRCUIT key drives two parameters (circuit_key_steps_off_circuit_hq checks the order).
        let c = CIRCUIT_KEY.center();
        e.mouse_down(c.x, c.y, 1);
        reached.extend(params_touched(&log).into_iter().map(|i| i as usize));
        // GUI coverage: every parameter in the host's list has a control.
        let missing: Vec<&str> = (0..TABLE.len()).filter(|i| !reached.contains(i)).map(|i| TABLE[i].id).collect();
        assert!(missing.is_empty(), "parameters with no control: {missing:?}");
    }

    #[test]
    fn nodes_drag_frequency_and_gain() {
        let (mut e, log) = editor();
        e.refresh_curve();
        let plot = e.plot();
        let m1 = e.node_point(plot, 2);
        e.mouse_down(m1.x, m1.y, 1);
        e.mouse_move(m1.x + 40.0, m1.y - 20.0);
        e.mouse_up(m1.x + 40.0, m1.y - 20.0);
        let touched = params_touched(&log);
        assert!(touched.contains(&(idx::MID1_FREQ as u32)) && touched.contains(&(idx::MID1_GAIN as u32)), "{touched:?}");
        assert!(e.v[idx::MID1_FREQ] > 800.0 && e.v[idx::MID1_GAIN] > 0.0);
    }

    #[test]
    fn right_click_asks_the_host_menu_for_the_control() {
        let seen = Rc::new(std::cell::Cell::new(None));
        let sink = seen.clone();
        let host = EditorHost::new(|_| {}).with_context_menu(move |p, _, _| {
            sink.set(Some(p));
            true
        });
        let mut e = HybridEqEditor::new(host);
        e.resize(W as u32, H as u32);
        let c = strip_bypass(2).center();
        assert!(e.right_mouse_down(c.x, c.y));
        assert_eq!(seen.get(), Some(STRIPS[2].bypass as u32));
        assert!(!e.right_mouse_down(5.0, 5.0), "the bare plate has no menu");
    }

    #[test]
    fn circuit_key_steps_off_circuit_hq() {
        let (mut e, _log) = editor();
        let c = CIRCUIT_KEY.center();
        let mode = |e: &HybridEqEditor| (e.on(idx::PREAMP_CIRCUIT), e.on(idx::PREAMP_HQ));
        e.mouse_down(c.x, c.y, 1);
        assert_eq!(mode(&e), (true, false), "Off -> Circuit");
        e.mouse_down(c.x, c.y, 1);
        assert_eq!(mode(&e), (true, true), "Circuit -> HQ");
        e.mouse_down(c.x, c.y, 1);
        assert!(!mode(&e).0, "HQ -> Off");
    }
}
