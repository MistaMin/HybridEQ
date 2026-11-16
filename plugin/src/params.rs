//! The host-facing parameter table, made from the engine's (`hybrideq_engine::params`), with the
//! identity and value space of HybridEQ 1.6.x so its sessions and automation keep working:
//!
//! - ids: the string ids in AAX (as before); in CLAP, VST3 and Audio Unit the 1.6.x hash of the
//!   string id (CLAP the full 32 bits, VST3 and Audio Unit the low 31), read from the 1.6.16 bundles
//!   with `openplugin-port scan` (qa/2026-11-14-rust-port/scan);
//! - values: 1.6.x showed hosts 0..1 travel for every continuous parameter (CLAP also for choices and
//!   switches), so those are shown normalized there;
//! - travel and text: the 1.6.x curves (log frequency, Q with skew 0.5) and value text.

use hybrideq_engine::params::{self as ep, Mapping, Param, COUNT, TABLE};
use openplugin::prelude::*;

/// 1.6.x's hash of a parameter's string id: h = 31 h + c over the characters.
pub const fn host_hash(id: &str) -> u32 {
    let b = id.as_bytes();
    let mut h: u32 = 0;
    let mut i = 0;
    while i < b.len() {
        h = h.wrapping_mul(31).wrapping_add(b[i] as u32);
        i += 1;
    }
    h
}

fn hz_text(v: f32) -> String {
    ep::format_value(ep::Unit::Hz, v)
}
fn hz_parse(t: &str) -> Option<f32> {
    ep::parse_value(ep::Unit::Hz, t)
}
fn db_text(v: f32) -> String {
    ep::format_value(ep::Unit::Db, v)
}
fn db_parse(t: &str) -> Option<f32> {
    ep::parse_value(ep::Unit::Db, t)
}
fn q_text(v: f32) -> String {
    ep::format_value(ep::Unit::Q, v)
}
fn q_parse(t: &str) -> Option<f32> {
    ep::parse_value(ep::Unit::Q, t)
}
fn pct_text(v: f32) -> String {
    ep::format_value(ep::Unit::Percent, v)
}
fn pct_parse(t: &str) -> Option<f32> {
    ep::parse_value(ep::Unit::Percent, t)
}

const fn sdk(p: &Param) -> ParamInfo {
    let hash = host_hash(p.id);
    let base = match p.unit {
        ep::Unit::Hz => ParamInfo::new(p.id, p.name, p.min, p.max, p.default, Unit::Hz).with_text(hz_text, hz_parse),
        ep::Unit::Db => ParamInfo::new(p.id, p.name, p.min, p.max, p.default, Unit::Db).with_text(db_text, db_parse),
        ep::Unit::Q => ParamInfo::new(p.id, p.name, p.min, p.max, p.default, Unit::None).with_text(q_text, q_parse),
        ep::Unit::Percent => ParamInfo::new(p.id, p.name, p.min, p.max, p.default, Unit::Percent).with_text(pct_text, pct_parse),
        ep::Unit::Choice => ParamInfo::new(p.id, p.name, p.min, p.max, p.default, Unit::Choice).with_choices(p.choices),
        ep::Unit::Toggle => ParamInfo::new(p.id, p.name, p.min, p.max, p.default, Unit::Toggle),
    };
    let base = match p.mapping {
        Mapping::Linear => base,
        Mapping::Log => base.with_taper(Taper::Log),
        Mapping::Skew(s) => base.with_taper(Taper::Skew(Skew::new(s))),
    };
    let space = match p.unit {
        // 1.6.x switches were stepped 0..1 in every format: their plain value already is that.
        ep::Unit::Toggle => HostSpace::PLAIN,
        ep::Unit::Choice => HostSpace::CLAP,
        _ => HostSpace::CLAP.union(HostSpace::SHELLS),
    };
    base.with_normalized_in(space).with_clap_id(hash).with_vst3_id(hash & 0x7fff_ffff).with_au_id(hash & 0x7fff_ffff)
}

macro_rules! table {
    ($($i:literal)*) => { [$(sdk(&TABLE[$i]),)*] };
}

pub static PARAMS: [ParamInfo; COUNT] = table!(0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31 32 33 34 35 36 37 38 39 40 41 42 43 44 45 46);

#[cfg(test)]
mod tests {
    use super::*;

    /// The ids the 1.6.16 bundles report (VST3 and Audio Unit masked, CLAP full), spot-checked.
    #[test]
    fn ids_are_those_of_1_6_16() {
        assert_eq!(host_hash("oversampleMode") & 0x7fff_ffff, 1_057_129_025);
        assert_eq!(host_hash("oversampleFilter") & 0x7fff_ffff, 2_083_021_974);
        assert_eq!(host_hash("harmonic2nd") & 0x7fff_ffff, 2_140_420_123);
        assert_eq!(openplugin::core::identity::check_pins(&crate::HybridEq::default()), []);
    }

    #[test]
    fn host_travel_matches_the_engine_table() {
        for (sdk, eng) in PARAMS.iter().zip(TABLE.iter()) {
            for k in 0..=10 {
                let v = eng.from_normalized(k as f32 / 10.0);
                assert!((sdk.to_normalized(v) - eng.to_normalized(v)).abs() < 1e-5, "{}", eng.id);
            }
            assert_eq!(sdk.format(eng.default), eng.format(eng.default), "{}", eng.id);
        }
    }
}
