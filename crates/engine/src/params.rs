//! The parameter table: the single source of truth for ids, names, ranges,
//! defaults, choice lists and the normalised mapping.
//!
//! Ids, ranges, defaults, choices and mappings are those of HybridEQ 1.6.x, in
//! the same order, so saved sessions and host automation keep their meaning.
//! New parameters are appended at the end, never inserted.

/// How normalised 0..1 travel maps to the plain value. The same three laws
/// the 1.6.x parameters used.
#[derive(Debug, Clone, Copy, PartialEq)]
pub enum Mapping {
    /// `min + (max - min) * t`
    Linear,
    /// `min * (max / min)^t`
    Log,
    /// `min + (max - min) * t^(1 / skew)` (normalised = proportion^skew)
    Skew(f32),
}

#[derive(Debug, Clone, Copy, PartialEq)]
pub enum Unit {
    Hz,
    Db,
    Q,
    Percent,
    Choice,
    Toggle,
}

#[derive(Debug, Clone, Copy, PartialEq)]
pub struct Param {
    pub id: &'static str,
    pub name: &'static str,
    pub min: f32,
    pub max: f32,
    pub default: f32,
    pub unit: Unit,
    pub mapping: Mapping,
    pub choices: &'static [&'static str],
}

const fn float(id: &'static str, name: &'static str, min: f32, max: f32, default: f32, unit: Unit, mapping: Mapping) -> Param {
    Param { id, name, min, max, default, unit, mapping, choices: &[] }
}

const fn choice(id: &'static str, name: &'static str, choices: &'static [&'static str], default: usize) -> Param {
    Param {
        id,
        name,
        min: 0.0,
        max: (choices.len() - 1) as f32,
        default: default as f32,
        unit: Unit::Choice,
        mapping: Mapping::Linear,
        choices,
    }
}

const fn toggle(id: &'static str, name: &'static str, default: bool) -> Param {
    Param {
        id,
        name,
        min: 0.0,
        max: 1.0,
        default: if default { 1.0 } else { 0.0 },
        unit: Unit::Toggle,
        mapping: Mapping::Linear,
        choices: &[],
    }
}

const fn hz(id: &'static str, name: &'static str, min: f32, max: f32, default: f32) -> Param {
    float(id, name, min, max, default, Unit::Hz, Mapping::Log)
}

const fn db(id: &'static str, name: &'static str, min: f32, max: f32, default: f32) -> Param {
    float(id, name, min, max, default, Unit::Db, Mapping::Linear)
}

const fn q(id: &'static str, name: &'static str, default: f32) -> Param {
    float(id, name, 0.1, 10.0, default, Unit::Q, Mapping::Skew(0.5))
}

const fn pct(id: &'static str, name: &'static str) -> Param {
    float(id, name, 0.0, 1.0, 0.0, Unit::Percent, Mapping::Linear)
}

pub const SLOPES: &[&str] = &["-6 dB/oct", "-12 dB/oct", "-18 dB/oct", "-24 dB/oct"];
pub const MS_MODES: &[&str] = &["Stereo", "Mid", "Side"];
pub const SHELF_TYPES: &[&str] = &["Bax-EQ", "Brit", "FSF"];
pub const BELL_TYPES: &[&str] = &["N-EQ", "Brit", "A-Type"];
pub const PREAMP_TYPES: &[&str] = &["Off", "Brit", "N-Type", "FSF", "A-Type"];
pub const PADS: &[&str] = &["-20 dB", "Unity", "+10 dB"];
pub const OVERSAMPLE: &[&str] = &["1x", "2x", "4x", "8x"];
pub const OS_FILTERS: &[&str] = &["IIR (Min)", "FIR (Lin)"];

pub const TABLE: [Param; COUNT] = [
    choice("oversampleMode", "Oversample", OVERSAMPLE, 0),
    choice("oversampleFilter", "Oversample Filter", OS_FILTERS, 0),
    pct("harmonic2nd", "2nd Harmonic"),
    pct("harmonic3rd", "3rd Harmonic"),
    pct("harmonic4th", "4th Harmonic"),
    pct("harmonic5th", "5th Harmonic"),
    hz("lcFreq", "Low Cut Freq", 20.0, 1000.0, 20.0),
    q("lcQ", "Low Cut Q", 0.707),
    choice("lcSlope", "Low Cut Slope", SLOPES, 1),
    choice("lcMode", "Low Cut Mode", MS_MODES, 0),
    toggle("lcBypass", "Low Cut Bypass", false),
    hz("hcFreq", "High Cut Freq", 1000.0, 30000.0, 20000.0),
    q("hcQ", "High Cut Q", 0.707),
    choice("hcSlope", "High Cut Slope", SLOPES, 1),
    choice("hcMode", "High Cut Mode", MS_MODES, 0),
    toggle("hcBypass", "High Cut Bypass", false),
    hz("lowFreq", "Low Freq", 20.0, 400.0, 100.0),
    db("lowGain", "Low Gain", -18.0, 18.0, 0.0),
    choice("lowType", "Low Type", SHELF_TYPES, 0),
    choice("lowMode", "Low Mode", MS_MODES, 0),
    toggle("lowBypass", "Low Band Bypass", false),
    hz("mid1Freq", "Mid 1 Freq", 200.0, 6000.0, 800.0),
    db("mid1Gain", "Mid 1 Gain", -18.0, 18.0, 0.0),
    q("mid1Q", "Mid 1 Q", 1.0),
    choice("mid1Type", "Mid 1 Type", BELL_TYPES, 0),
    choice("mid1Mode", "Mid 1 Mode", MS_MODES, 0),
    toggle("mid1Bypass", "Mid 1 Bypass", false),
    hz("mid2Freq", "Mid 2 Freq", 200.0, 6000.0, 2500.0),
    db("mid2Gain", "Mid 2 Gain", -18.0, 18.0, 0.0),
    q("mid2Q", "Mid 2 Q", 1.0),
    choice("mid2Type", "Mid 2 Type", BELL_TYPES, 0),
    choice("mid2Mode", "Mid 2 Mode", MS_MODES, 0),
    toggle("mid2Bypass", "Mid 2 Bypass", false),
    hz("highFreq", "High Freq", 800.0, 30000.0, 8000.0),
    db("highGain", "High Gain", -18.0, 18.0, 0.0),
    choice("highType", "High Type", SHELF_TYPES, 0),
    choice("highMode", "High Mode", MS_MODES, 0),
    toggle("highBypass", "High Band Bypass", false),
    choice("preampType", "Preamp Type", PREAMP_TYPES, 1),
    choice("preampPad", "Preamp Pad", PADS, 1),
    db("preampGain", "Preamp Gain", -12.0, 24.0, 0.0),
    toggle("preampBypass", "Preamp Bypass", false),
    toggle("preampCircuit", "Preamp Circuit", false),
    // 1.6.12: with Circuit on, HQ selects the full solver (on) or the reduced model (off). Sessions saved
    // before it existed restore as HQ, which is what their Circuit setting always meant.
    toggle("preampHQ", "Preamp HQ", true),
    db("outputGain", "Output Gain", -24.0, 24.0, 0.0),
    // 1.7.0
    toggle("autoGain", "Auto Gain", false),
    toggle("delta", "Delta", false),
];

pub static PARAMS: [Param; COUNT] = TABLE;

pub const COUNT: usize = 47;

/// Index constants, in table order.
pub mod idx {
    pub const OVERSAMPLE_MODE: usize = 0;
    pub const OVERSAMPLE_FILTER: usize = 1;
    pub const H2: usize = 2;
    pub const H3: usize = 3;
    pub const H4: usize = 4;
    pub const H5: usize = 5;
    pub const LC_FREQ: usize = 6;
    pub const LC_Q: usize = 7;
    pub const LC_SLOPE: usize = 8;
    pub const LC_MODE: usize = 9;
    pub const LC_BYPASS: usize = 10;
    pub const HC_FREQ: usize = 11;
    pub const HC_Q: usize = 12;
    pub const HC_SLOPE: usize = 13;
    pub const HC_MODE: usize = 14;
    pub const HC_BYPASS: usize = 15;
    pub const LOW_FREQ: usize = 16;
    pub const LOW_GAIN: usize = 17;
    pub const LOW_TYPE: usize = 18;
    pub const LOW_MODE: usize = 19;
    pub const LOW_BYPASS: usize = 20;
    pub const MID1_FREQ: usize = 21;
    pub const MID1_GAIN: usize = 22;
    pub const MID1_Q: usize = 23;
    pub const MID1_TYPE: usize = 24;
    pub const MID1_MODE: usize = 25;
    pub const MID1_BYPASS: usize = 26;
    pub const MID2_FREQ: usize = 27;
    pub const MID2_GAIN: usize = 28;
    pub const MID2_Q: usize = 29;
    pub const MID2_TYPE: usize = 30;
    pub const MID2_MODE: usize = 31;
    pub const MID2_BYPASS: usize = 32;
    pub const HIGH_FREQ: usize = 33;
    pub const HIGH_GAIN: usize = 34;
    pub const HIGH_TYPE: usize = 35;
    pub const HIGH_MODE: usize = 36;
    pub const HIGH_BYPASS: usize = 37;
    pub const PREAMP_TYPE: usize = 38;
    pub const PREAMP_PAD: usize = 39;
    pub const PREAMP_GAIN: usize = 40;
    pub const PREAMP_BYPASS: usize = 41;
    pub const PREAMP_CIRCUIT: usize = 42;
    pub const PREAMP_HQ: usize = 43;
    pub const OUTPUT_GAIN: usize = 44;
    pub const AUTO_GAIN: usize = 45;
    pub const DELTA: usize = 46;
}

/// Index of a parameter id.
pub fn index_of(id: &str) -> Option<usize> {
    PARAMS.iter().position(|p| p.id == id)
}

impl Param {
    /// Clamp to the range; choices and toggles to whole numbers.
    pub fn clamp(&self, value: f32) -> f32 {
        let v = if value.is_nan() { self.default } else { value.clamp(self.min, self.max) };
        // Floats are continuous: the host's value is the one used and saved, so they never disagree.
        match self.unit {
            Unit::Choice | Unit::Toggle => v.round(),
            _ => v,
        }
    }

    pub fn to_normalized(&self, plain: f32) -> f32 {
        let v = if plain.is_nan() { self.default } else { plain.clamp(self.min, self.max) };
        let span = self.max - self.min;
        if span <= 0.0 {
            return 0.0;
        }
        let t = match self.mapping {
            Mapping::Linear => (v - self.min) / span,
            Mapping::Log => (v / self.min).ln() / (self.max / self.min).ln(),
            Mapping::Skew(skew) => {
                let p = (v - self.min) / span;
                if p > 0.0 { (p.ln() * skew).exp() } else { 0.0 }
            }
        };
        t.clamp(0.0, 1.0)
    }

    pub fn from_normalized(&self, t: f32) -> f32 {
        let t = if t.is_nan() { 0.0 } else { t.clamp(0.0, 1.0) };
        let v = match self.mapping {
            Mapping::Linear => self.min + (self.max - self.min) * t,
            Mapping::Log => self.min * (self.max / self.min).powf(t),
            Mapping::Skew(skew) => {
                let p = if t > 0.0 { (t.ln() / skew).exp() } else { 0.0 };
                self.min + (self.max - self.min) * p
            }
        };
        self.clamp(v)
    }

    /// Display text. A displayed value always parses back to the same value.
    pub fn format(&self, value: f32) -> String {
        let v = self.clamp(value);
        match self.unit {
            Unit::Choice => self.choices.get(v as usize).copied().unwrap_or("").to_string(),
            Unit::Toggle => (if v >= 0.5 { "On" } else { "Off" }).to_string(),
            u => format_value(u, v),
        }
    }

    /// Reads a typed value (or one [`Self::format`] printed) back into plain units.
    pub fn parse(&self, text: &str) -> Option<f32> {
        let t = text.trim();
        if t.is_empty() {
            return None;
        }
        match self.unit {
            Unit::Choice => {
                if let Some(i) = self.choices.iter().position(|c| c.eq_ignore_ascii_case(t)) {
                    return Some(i as f32);
                }
                leading_number(t).map(|n| self.clamp(n))
            }
            Unit::Toggle => match t.to_ascii_lowercase().as_str() {
                "on" | "true" | "yes" => Some(1.0),
                "off" | "false" | "no" => Some(0.0),
                _ => leading_number(t).map(|n| self.clamp(n)),
            },
            u => parse_value(u, t).map(|v| self.clamp(v)),
        }
    }
}

/// Text for a value of a numeric unit (Hz, dB, Q, percent), as 1.6.x printed it. Not clamped.
pub fn format_value(unit: Unit, v: f32) -> String {
    match unit {
        Unit::Hz => hz_text(v as f64),
        Unit::Db => {
            let r = round_to(v as f64, 1);
            format!("{}{:.1} dB", if r > 0.0 { "+" } else { "" }, r)
        }
        // at most three digits: 0.71, 2.57, 10.0
        Unit::Q if v >= 9.995 => format!("{:.1}", v),
        Unit::Q => format!("{:.2}", v),
        Unit::Percent => format!("{} %", (v * 100.0).round() as i32),
        Unit::Choice | Unit::Toggle => format!("{v}"),
    }
}

/// Reads text of a numeric unit back (`kHz` scales, percent divides by 100). Not clamped.
pub fn parse_value(unit: Unit, text: &str) -> Option<f32> {
    let t = text.trim();
    let n = leading_number(t)?;
    match unit {
        Unit::Hz if t.to_ascii_lowercase().ends_with("khz") => Some(n * 1000.0),
        Unit::Percent => Some(n / 100.0),
        _ => Some(n),
    }
}

fn round_to(v: f64, digits: i32) -> f64 {
    let m = 10f64.powi(digits);
    (v * m).round() / m
}

/// Frequency text as 1.6.x printed it: "12.5 kHz", "2.50 kHz", "800 Hz", "20.0 Hz".
pub fn hz_text(hz: f64) -> String {
    if hz >= 10000.0 {
        format!("{:.1} kHz", hz / 1000.0)
    } else if hz >= 1000.0 {
        format!("{:.2} kHz", hz / 1000.0)
    } else if hz >= 100.0 {
        format!("{} Hz", hz.round() as i64)
    } else {
        format!("{:.1} Hz", hz)
    }
}

fn leading_number(t: &str) -> Option<f32> {
    let end = t
        .char_indices()
        .find(|&(i, c)| !(c.is_ascii_digit() || c == '.' || ((c == '-' || c == '+') && i == 0)))
        .map(|(i, _)| i)
        .unwrap_or(t.len());
    t[..end].parse::<f32>().ok().filter(|v| v.is_finite())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn table_is_consistent() {
        assert_eq!(PARAMS.len(), COUNT);
        for (i, p) in PARAMS.iter().enumerate() {
            assert_eq!(index_of(p.id), Some(i), "duplicate id {}", p.id);
            assert!(p.min <= p.default && p.default <= p.max, "{}", p.id);
        }
        assert_eq!(PARAMS[idx::OUTPUT_GAIN].id, "outputGain");
        assert_eq!(PARAMS[idx::PREAMP_HQ].id, "preampHQ");
        assert_eq!(PARAMS[idx::DELTA].id, "delta");
    }

    #[test]
    fn normalised_round_trip() {
        for p in PARAMS.iter() {
            for k in 0..=20 {
                let t = k as f32 / 20.0;
                let v = p.from_normalized(t);
                let back = p.from_normalized(p.to_normalized(v));
                assert!((back - v).abs() <= 1e-3 * (1.0 + v.abs()), "{} {} {} {}", p.id, t, v, back);
            }
        }
    }

    #[test]
    fn q_uses_square_root_skew() {
        let p = &PARAMS[idx::LC_Q];
        // JUCE NormalisableRange(0.1, 10, 0.01, 0.5): normalised = sqrt(proportion)
        let v = 2.5f32;
        let expect = ((v - 0.1) / 9.9).sqrt();
        assert!((p.to_normalized(v) - expect).abs() < 1e-6);
    }

    #[test]
    fn text_round_trip() {
        for p in PARAMS.iter() {
            for k in 0..=40 {
                let v = p.from_normalized(k as f32 / 40.0);
                let shown = p.format(v);
                let back = p.parse(&shown).unwrap_or_else(|| panic!("{} cannot parse {shown}", p.id));
                assert_eq!(p.format(back), shown, "{}", p.id);
                let digits = shown.chars().filter(char::is_ascii_digit).count();
                assert!(digits <= 3, "{} shows {shown}: more than three digits", p.id);
            }
        }
    }
}
