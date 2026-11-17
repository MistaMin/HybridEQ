//! Plugin state: this version's own chunk, and the state 1.6.x saved.
//!
//! Own chunk: `HEQ7`, a version byte, then `id=value` lines (plain units), so a later table can add,
//! drop or reorder parameters, and `@size=WxH`, the editor size. 1.6.x kept the editor size in the
//! `consoleEditorWidth` / `consoleEditorHeight` attributes of its document. 1.6.x saved `VC2!`, a 32-bit size and an XML document,
//! `<Parameters ...><PARAM id=".." value=".."/>...</Parameters>`, with plain values; parameters it did
//! not have (`preampHQ` before 1.6.12, Autogain and Delta) keep their defaults, so an old Circuit
//! session opens as Circuit HQ, which is what it always meant.

use hybrideq_engine::defaults;
use hybrideq_engine::params::{index_of, COUNT, TABLE};
use openplugin::core::foreign_state::xml;

const MAGIC: &[u8; 4] = b"HEQ7";
const VERSION: u8 = 1;

pub type Size = Option<(u32, u32)>;

pub fn encode(values: &[f32; COUNT], size: Size) -> Vec<u8> {
    let mut out = MAGIC.to_vec();
    out.push(VERSION);
    for (p, v) in TABLE.iter().zip(values) {
        out.extend_from_slice(format!("{}={}\n", p.id, v).as_bytes());
    }
    if let Some((w, h)) = size {
        out.extend_from_slice(format!("@size={w}x{h}\n").as_bytes());
    }
    out
}

fn size_of(w: Option<&str>, h: Option<&str>) -> Size {
    let (w, h) = (w?.trim().parse::<u32>().ok()?, h?.trim().parse::<u32>().ok()?);
    (w > 0 && h > 0).then_some((w, h))
}

/// Every value and the editor size, from either format, or `None` when the bytes are neither
/// (nothing is applied then).
pub fn decode(data: &[u8]) -> Option<([f32; COUNT], Size)> {
    if let Some(body) = data.strip_prefix(MAGIC) {
        return decode_own(body);
    }
    decode_1_6(data)
}

fn decode_own(body: &[u8]) -> Option<([f32; COUNT], Size)> {
    let (&version, text) = body.split_first()?;
    if version != VERSION {
        return None;
    }
    let mut values = defaults();
    let mut size = None;
    for line in std::str::from_utf8(text).ok()?.lines() {
        let (id, v) = line.split_once('=')?;
        if id == "@size" {
            let (w, h) = v.split_once('x')?;
            size = size_of(Some(w), Some(h));
            continue;
        }
        let v: f32 = v.parse().ok()?;
        if let Some(i) = index_of(id) {
            values[i] = TABLE[i].clamp(v);
        }
    }
    Some((values, size))
}

/// 1.6.x: `VC2!`, size, XML. The XML is taken from the first `<` near the front to the last `>`.
fn decode_1_6(data: &[u8]) -> Option<([f32; COUNT], Size)> {
    let body = data.strip_prefix(b"VC2!")?;
    let start = body.iter().position(|b| *b == b'<')?;
    let end = body.iter().rposition(|b| *b == b'>')?;
    if start > 16 || end <= start {
        return None;
    }
    let root = xml::parse(&body[start..=end]).ok()?;
    if root.name != "Parameters" {
        return None;
    }
    let mut values = defaults();
    let mut found = 0;
    for param in root.children_named("PARAM") {
        let (Some(id), Some(v)) = (param.attr("id"), param.attr("value").and_then(|v| v.parse::<f32>().ok())) else {
            return None;
        };
        if let Some(i) = index_of(id) {
            values[i] = TABLE[i].clamp(v);
            found += 1;
        }
    }
    let size = size_of(root.attr("consoleEditorWidth"), root.attr("consoleEditorHeight"));
    (found > 0).then_some((values, size))
}

#[cfg(test)]
mod tests {
    use super::*;
    use hybrideq_engine::params::idx;

    #[test]
    fn own_state_round_trips() {
        let mut v = defaults();
        v[idx::LC_FREQ] = 123.5;
        v[idx::PREAMP_TYPE] = 3.0;
        v[idx::DELTA] = 1.0;
        // values come back clamped and snapped to their step, as 1.6.x stored them (Q 0.707 -> 0.71)
        let snapped: [f32; COUNT] = std::array::from_fn(|i| TABLE[i].clamp(v[i]));
        assert_eq!(decode(&encode(&v, None)), Some((snapped, None)));
        assert_eq!(decode(&encode(&v, Some((1500, 1119)))), Some((snapped, Some((1500, 1119)))));
        assert_eq!(decode(b"junk"), None);
        assert_eq!(decode(b"HEQ7\x09"), None, "an unknown version is refused");
    }

    /// Every state 1.6.16 saved in the capture (qa/2026-11-14-rust-port/capture) loads with its values.
    #[test]
    fn states_of_1_6_16_load() {
        let dir = concat!(env!("CARGO_MANIFEST_DIR"), "/../qa/2026-11-14-rust-port/capture/hybrideq.clap.blobs");
        let set = std::fs::read(format!("{dir}/001-set.clap-state.bin")).expect("captured 1.6.16 state");
        let (v, size) = decode(&set).expect("1.6.16 state is read");
        assert_eq!(size, None, "the capture never opened an editor");
        // the capture set the 0..1 host values of Low Cut Freq to 1 (1 kHz), Low Cut Q to 1 (10) and Mid 1 Gain
        assert_eq!(v[idx::LC_FREQ], 1000.0);
        assert_eq!(v[idx::LC_Q], 10.0);
        assert_eq!(v[idx::PREAMP_HQ], 1.0, "absent in the XML of an older session: HQ, as it always meant");
        let (def, _) = decode(&std::fs::read(format!("{dir}/000-default.clap-state.bin")).unwrap()).unwrap();
        assert_eq!(def[idx::HC_FREQ], 20000.0);
        // the same document in the VST3 component state
        let vdir = concat!(env!("CARGO_MANIFEST_DIR"), "/../qa/2026-11-14-rust-port/capture/hybrideq.vst3.blobs");
        assert_eq!(decode(&std::fs::read(format!("{vdir}/001-set.vst3-component.bin")).unwrap()), Some((v, None)));
        // 1.6.x stored the editor size on the document
        let sized = String::from_utf8_lossy(&set).replace("<Parameters>", "<Parameters consoleEditorWidth=\"1300\" consoleEditorHeight=\"969\">");
        assert_eq!(decode(sized.as_bytes()).map(|d| d.1), Some(Some((1300, 969))));
        // damaged documents change nothing
        assert_eq!(decode(&set[..set.len() / 2]), None);
    }
}
