//! The four preamp netlists, solved per sample by [`Solver`]. Provenance tags on every value:
//! [NETLIST] values and topology from the transcription, [DATASHEET] published figures,
//! [ESTIMATE] fitted or assumed, [PLACEHOLDER] known-weak until better data exists.

use crate::solver::{NpnModel, Solver};

const PI_CAL: f64 = 3.14159265358979;

/// Common surface of the four circuits, as the preamp engine drives them.
pub trait Circuit {
    const MAX_GAIN: f64;
    fn prepare(&mut self, sample_rate: f64) -> bool;
    fn restore_dc(&mut self);
    fn restore_dc_at(&mut self, sample_rate: f64);
    fn set_full_scale_volts(&mut self, v: f64);
    fn set_gain(&mut self, g: f64);
    fn process(&mut self, x: f64) -> f64;
}

/// Small-signal 1 kHz peak gain over the second half of `seconds` of a sine of `amp`, driving a
/// balanced pair (`n` is the negative leg) or a single-ended node.
fn measure_peak(s: &mut Solver, p: usize, n: Option<usize>, out_p: usize, out_n: Option<usize>, amp: f64, seconds: f64, balanced_half: bool) -> f64 {
    let sr = s.sample_rate();
    let w = 2.0 * PI_CAL * 1000.0 / sr;
    let count = (sr * seconds) as usize;
    let mut pk = 0.0f64;
    for i in 0..count {
        let e = if balanced_half { 0.5 * amp } else { amp } * (w * i as f64).sin();
        s.set_fixed(p, e);
        if let Some(n) = n {
            s.set_fixed(n, -e);
        }
        s.step();
        if i > count / 2 {
            let v = s.voltage(out_p) - out_n.map_or(0.0, |o| s.voltage(o));
            pk = pk.max(v.abs());
        }
    }
    pk
}

// ---------------------------------------------------------------------------------------------
// N-Type: transformer-coupled line path. VTB9046 input transformer -> 3-transistor feedback preamp
// -> class-A output stage -> gapped VTB9049 output transformer -> 600 ohm load with Zobel.

pub struct NType {
    s: Solver,
    n_in: usize,
    n_out: usize,
    r_gain: usize,
    pub out_scale: f64,
    pub v_full: f64,
}

impl Default for NType {
    fn default() -> Self {
        Self { s: Solver::new(), n_in: 0, n_out: 0, r_gain: 0, out_scale: 1.0, v_full: 1.0 }
    }
}

impl NType {
    const IN_SOURCE: f64 = 10000.0; // [DATASHEET]
    const IN_DCR_PRI: f64 = 350.0; // [DATASHEET] 175 + 175
    const IN_DCR_SEC: f64 = 28.0; // [DATASHEET] 56 || 56
    const IN_TURNS: f64 = 0.25; // [DATASHEET]
    const IN_L0: f64 = 100.0; // [ESTIMATE] the key number to fit (~15 Hz corner)
    const IN_LSAT: f64 = 5.0; // [ESTIMATE]
    const IN_ISAT: f64 = 0.0005; // [ESTIMATE]
    const IN_LEAK: f64 = 0.02; // [ESTIMATE]
    const IN_CAP: f64 = 200e-12; // [ESTIMATE]
    const OUT_DCR_PRI: f64 = 12.0; // [DATASHEET]
    const OUT_DCR_SEC: f64 = 40.0; // [DATASHEET]
    const OUT_TURNS: f64 = 1.7; // [DATASHEET]
    const OUT_L0: f64 = 4.0; // [ESTIMATE]
    const OUT_LSAT: f64 = 0.3; // [ESTIMATE]
    const OUT_ISAT: f64 = 0.06; // [ESTIMATE]
    const OUT_LEAK: f64 = 0.0005; // [ESTIMATE]
    const VCEO: f64 = 59.4; // [DATASHEET-ish]
    const RAIL: f64 = 24.0; // [NETLIST]
    const LOAD: f64 = 600.0; // [DATASHEET]
    const BIAS_POT: f64 = 0.5; // [ESTIMATE]
    const R11_NOMINAL: f64 = 1800.0;

    fn build(&mut self) {
        self.s = Solver::new();
        let bc184c = NpnModel::npn(1.8e-14, 450.0, 5.0, 80.0, 9e-12, 5e-12, 1.06e-9);
        let bc184c_hi = NpnModel::npn(1.8e-14, 650.0, 5.0, 80.0, 9e-12, 5e-12, 1.06e-9);
        let n2n3055 = NpnModel::npn(1e-12, 37.0, 5.0, 100.0, 500e-12, 200e-12, 64e-9);
        let s = &mut self.s;
        let n = s.add_fixed_node(Self::RAIL);
        self.n_in = s.add_fixed_node(0.0);

        let (in_a, in_b, in_p, in_s) = (s.add_node(), s.add_node(), s.add_node(), s.add_node());
        let u = s.add_node();
        s.add_r(self.n_in, in_a, Self::IN_SOURCE, false);
        s.add_r(in_a, in_b, Self::IN_DCR_PRI, false);
        s.add_l(in_b, in_p, Self::IN_LEAK, 0.0, 0.0);
        s.add_l(in_p, 0, Self::IN_L0, Self::IN_LSAT, Self::IN_ISAT);
        s.add_ideal(in_p, 0, in_s, 0, Self::IN_TURNS);
        s.add_r(in_s, u, Self::IN_DCR_SEC, false);
        s.add_c(u, 0, Self::IN_CAP);

        let (b1, fb, c1, e1) = (s.add_node(), s.add_node(), s.add_node(), s.add_node());
        let (dec, e2, c2, out) = (s.add_node(), s.add_node(), s.add_node(), s.add_node());
        let (t, tt, ss, p) = (s.add_node(), s.add_node(), s.add_node(), s.add_node());
        s.add_r(u, 0, 120e3, false);
        s.add_c(b1, u, 10e-6);
        s.add_r(b1, fb, 68e3, false);
        s.add_c(c1, b1, 100e-12);
        s.add_c(b1, e1, 1500e-12);
        s.add_npn(c1, b1, e1, bc184c_hi);
        s.add_r(n, dec, 33e3, false);
        s.add_r(dec, c1, 47e3, false);
        s.add_c(dec, e2, 22e-6);
        s.add_c(c1, fb, 680e-12);
        s.add_npn(c2, c1, e2, bc184c);
        s.add_r(n, c2, 5.1e3, false);
        s.add_r(e2, fb, 470.0, false);
        s.add_r(fb, 0, 1.5e3, false);
        s.add_c(fb, 0, 125e-6);
        s.add_c(e2, ss, 22e-6);
        s.add_npn(n, c2, out, bc184c);
        s.add_c(out, p, 22e-6);
        s.add_r(out, t, 2.2e3, false);
        s.add_r(t, 0, 390.0, false);
        s.add_r(e1, t, 10e3, false);
        s.add_c(ss, 0, 1000e-12);
        s.add_c(t, tt, 400e-6);
        self.r_gain = s.add_r(tt, 0, Self::R11_NOMINAL, true);
        s.add_r(p, tt, 51e3, false);

        let (n1, ob1, oc1, k) = (s.add_node(), s.add_node(), s.add_node(), s.add_node());
        let (wip, b, ofb, d) = (s.add_node(), s.add_node(), s.add_node(), s.add_node());
        let (c, f) = (s.add_node(), s.add_node());
        s.add_c(n1, p, 10e-6);
        s.add_r(n1, ob1, 2.2e3, false);
        s.add_npn(oc1, ob1, k, bc184c_hi);
        s.add_c(oc1, ob1, 220e-12);
        s.add_c(ob1, k, 4.7e-9);
        s.add_r(ob1, wip, 56e3, false);
        s.add_r(n, oc1, 68e3, false);
        s.add_r(k, 0, 1.2e3, false);
        s.add_c(b, ofb, 80e-6);
        s.add_r(ofb, k, 3.3e3, false);
        s.add_c(ofb, k, 330e-12);
        s.add_npn(b, oc1, d, bc184c);
        s.add_r(d, 0, 18e3, false);
        s.add_npn(b, d, c, n2n3055);
        s.add_r(c, wip, 4.7e3 * (1.0 - Self::BIAS_POT), false);
        s.add_r(wip, 0, 4.7e3 * Self::BIAS_POT, false);
        s.add_r(c, 0, 47.0, false);
        s.add_c(b, f, 80e-6);
        s.add_r(f, 0, 33e3, false);

        let (pa, pb, sa) = (s.add_node(), s.add_node(), s.add_node());
        self.n_out = s.add_node();
        let zn = s.add_node();
        s.add_r(n, pa, Self::OUT_DCR_PRI, false);
        s.add_l(pa, pb, Self::OUT_LEAK, 0.0, 0.0);
        s.add_l(pb, b, Self::OUT_L0, Self::OUT_LSAT, Self::OUT_ISAT);
        s.add_ideal(pb, b, sa, 0, Self::OUT_TURNS);
        let vceo = s.add_fixed_node(Self::VCEO);
        s.add_diode(b, vceo, 1e-14, 1.0);
        s.add_r(sa, self.n_out, Self::OUT_DCR_SEC, false);
        s.add_r(self.n_out, 0, Self::LOAD, false);
        s.add_r(self.n_out, zn, 1.5e3, false);
        s.add_c(zn, 0, 10e-9);
    }

    fn calibrate(&mut self) {
        self.out_scale = 1.0;
        self.set_gain(1.0);
        let amp = 1e-3;
        let pk = measure_peak(&mut self.s, self.n_in, None, self.n_out, None, amp, 0.1, false);
        self.out_scale = amp / pk;
        self.s.restore_dc();
    }
}

impl Circuit for NType {
    const MAX_GAIN: f64 = 20.0;

    fn prepare(&mut self, sample_rate: f64) -> bool {
        self.build();
        let ok = self.s.prepare(sample_rate);
        self.calibrate();
        ok
    }
    fn restore_dc(&mut self) {
        self.s.restore_dc();
    }
    fn restore_dc_at(&mut self, sample_rate: f64) {
        self.s.restore_dc_at(sample_rate);
    }
    fn set_full_scale_volts(&mut self, v: f64) {
        self.v_full = v;
    }
    /// Closed-loop gain relative to the stock 1k8 network, set by R11 like the gain switch.
    fn set_gain(&mut self, g: f64) {
        const DELTA: f64 = 0.00627;
        let g = g.clamp(1.0, 20.0);
        let g_out = 1.0 / 2200.0 + 1.0 / 51000.0;
        let g_gnd0 = 1.0 / 390.0 + 1.0 / Self::R11_NOMINAL;
        let beta0 = g_out / (g_out + g_gnd0);
        let beta = ((beta0 + DELTA) / g - DELTA).max(0.002);
        let g_gnd = g_out * (1.0 / beta - 1.0);
        let g11 = g_gnd - 1.0 / 390.0;
        self.s.set_r(self.r_gain, if g11 > 1.0 / 20000.0 { 1.0 / g11 } else { 20000.0 });
    }
    fn process(&mut self, x: f64) -> f64 {
        self.s.set_fixed(self.n_in, x * self.v_full);
        self.s.step();
        self.s.voltage(self.n_out) * self.out_scale / self.v_full
    }
}

// ---------------------------------------------------------------------------------------------
// Brit: balanced 500-series console mic preamp. Matched NPN pair with op-amp current feedback ->
// instrumentation pair -> difference amp -> DC servos -> balanced line driver. Gain is the dual-gang
// pot RV1.

pub struct Brit {
    s: Solver,
    n_sp: usize,
    n_sn: usize,
    n_out_p: usize,
    n_out_n: usize,
    rv_a: usize,
    rv_b: usize,
    last_gain: f64,
    pub out_scale: f64,
    pub v_full: f64,
}

impl Default for Brit {
    fn default() -> Self {
        Self { s: Solver::new(), n_sp: 0, n_sn: 0, n_out_p: 0, n_out_n: 0, rv_a: 0, rv_b: 0, last_gain: 0.0, out_scale: 1.0, v_full: 1.0 }
    }
}

impl Brit {
    const RAIL: f64 = 15.0; // [NETLIST]
    const RV_MAX: f64 = 2200.0; // [NETLIST] gain pot, 0 = max gain
    const RV_REF: f64 = 2200.0;

    fn gain_at(r: f64) -> f64 {
        (1.0 + 2000.0 / (27.0 + r)) * (1.02 + 2000.0 / (39.0 + r))
    }

    fn build(&mut self) {
        self.s = Solver::new();
        let mat02 = NpnModel::npn(1.2e-13, 605.0, 5.0, 200.0, 40e-12, 23e-12, 0.8e-9);
        let s = &mut self.s;
        let vp = s.add_fixed_node(Self::RAIL);
        let _vn = s.add_fixed_node(-Self::RAIL);
        self.n_sp = s.add_fixed_node(0.0);
        self.n_sn = s.add_fixed_node(0.0);
        let ne3 = |s: &mut Solver, p, n, o| s.add_opamp(p, n, o, 48e6, 13.0, 1e5, 13.0, 10.0, 1e6);
        let ne10 = |s: &mut Solver, p, n, o| s.add_opamp(p, n, o, 34e6, 13.0, 1e5, 13.0, 10.0, 1e6);
        let tl = |s: &mut Solver, p, n, o| s.add_opamp(p, n, o, 5.25e6, 20.0, 2e5, 13.5, 125.0, 1e6);

        let (inp, inn) = (s.add_node(), s.add_node());
        s.add_r(self.n_sp, inp, 75.0, false);
        s.add_r(self.n_sn, inn, 75.0, false);

        let nph = s.add_node();
        s.add_r(inn, nph, 6.8e3, false);
        s.add_r(inp, nph, 6.8e3, false);
        s.add_c(nph, 0, 100e-6);
        s.add_r(nph, 0, 1e3, false);

        let (n_l2, n_l1, n_ba, n_bb) = (s.add_node(), s.add_node(), s.add_node(), s.add_node());
        s.add_c(inp, n_l2, 2.2e-6);
        s.add_l(n_l2, n_ba, 18e-6, 0.0, 0.0);
        s.add_c(inn, n_l1, 2.2e-6);
        s.add_l(n_l1, n_bb, 18e-6, 0.0, 0.0);
        let (n_z1, n_z2) = (s.add_node(), s.add_node());
        s.add_c(n_ba, 0, 100e-12);
        s.add_c(n_ba, n_z1, 330e-12);
        s.add_r(n_z1, 0, 300.0, false);
        s.add_r(n_ba, 0, 100e3, false);
        s.add_c(n_bb, 0, 100e-12);
        s.add_c(n_bb, n_z2, 330e-12);
        s.add_r(n_z2, 0, 300.0, false);
        s.add_r(n_bb, 0, 100e3, false);

        let (vccf, n_ca, n_cb, n_ref) = (s.add_node(), s.add_node(), s.add_node(), s.add_node());
        let (n_ea, n_eb, n_rva) = (s.add_node(), s.add_node(), s.add_node());
        s.add_r(vp, vccf, 100.0, false);
        s.add_c(vccf, 0, 220e-6);
        s.add_r(vccf, n_ca, 3.9e3, false);
        s.add_r(vccf, n_cb, 3.9e3, false);
        s.add_r(vccf, n_ref, 4.22e3, false);
        s.add_r(n_ref, 0, 10e3, false);
        s.add_c(n_ref, 0, 10e-6);
        s.add_npn(n_ca, n_ba, n_ea, mat02);
        s.add_npn(n_cb, n_bb, n_eb, mat02);
        s.add_r(n_ea, n_rva, 27.0, false);
        self.rv_a = s.add_r(n_rva, n_eb, Self::RV_MAX, true);
        let (n_z3, n_z4) = (s.add_node(), s.add_node());
        s.add_c(n_ca, n_z3, 330e-12);
        s.add_r(n_z3, 0, 200.0, false);
        s.add_c(n_cb, n_z4, 330e-12);
        s.add_r(n_z4, 0, 200.0, false);
        let (n_o29, n_o22, n_x1, n_x2) = (s.add_node(), s.add_node(), s.add_node(), s.add_node());
        ne3(s, n_ref, n_ca, n_o29);
        s.add_r(n_o29, n_ea, 1e3, false);
        s.add_r(n_ea, n_x1, 100.0, false);
        s.add_c(n_x1, n_o29, 470e-12);
        ne3(s, n_ref, n_cb, n_o22);
        s.add_r(n_o22, n_eb, 1e3, false);
        s.add_r(n_eb, n_x2, 100.0, false);
        s.add_c(n_x2, n_o22, 470e-12);

        let (tp3, tp4, n_n30, n_o30) = (s.add_node(), s.add_node(), s.add_node(), s.add_node());
        let (n_rvb, n_n23, n_o23, n_sv1) = (s.add_node(), s.add_node(), s.add_node(), s.add_node());
        s.add_c(n_o29, tp3, 2.2e-6);
        s.add_r(tp3, 0, 100e3, false);
        s.add_c(n_o22, tp4, 2.2e-6);
        s.add_r(tp4, 0, 100e3, false);
        ne3(s, tp3, n_n30, n_o30);
        s.add_r(n_n30, n_o30, 1e3, false);
        s.add_c(n_n30, n_o30, 100e-12);
        s.add_r(n_n30, 0, 49.9e3, false);
        s.add_r(n_n30, n_rvb, 39.0, false);
        self.rv_b = s.add_r(n_rvb, n_n23, Self::RV_MAX, true);
        ne3(s, tp4, n_n23, n_o23);
        s.add_r(n_n23, n_o23, 1e3, false);
        s.add_c(n_n23, n_o23, 100e-12);
        s.add_r(n_n23, n_sv1, 49.9e3, false);

        let (n_sp, n_sm) = (s.add_node(), s.add_node());
        tl(s, n_sp, n_sm, n_sv1);
        s.add_r(n_o23, n_sp, 470e3, false);
        s.add_c(n_sp, 0, 940e-9);
        s.add_r(n_o30, n_sm, 470e3, false);
        s.add_c(n_sm, n_sv1, 940e-9);

        let (n_p24, n_n24, n_o24, n_sv2) = (s.add_node(), s.add_node(), s.add_node(), s.add_node());
        s.add_r(n_o30, n_p24, 1.0 / (1.0 / 6.8e3 + 1.0 / 100e3), false);
        s.add_r(n_p24, 0, 6.8e3, false);
        s.add_r(n_p24, n_sv2, 100e3, false);
        s.add_r(n_o23, n_n24, 6.8e3, false);
        s.add_r(n_n24, n_o24, 6.8e3, false);
        ne10(s, n_p24, n_n24, n_o24);

        let n_sm2 = s.add_node();
        tl(s, 0, n_sm2, n_sv2);
        s.add_r(n_o24, n_sm2, 470e3, false);
        s.add_c(n_sm2, n_sv2, 940e-9);

        let (n_spo, n_sn) = (s.add_node(), s.add_node());
        s.add_r(n_o24, 0, 10e3, false);
        s.add_buffer(n_o24, 0, n_spo, 0.995, 7.4, 50.0);
        s.add_buffer(n_o24, 0, n_sn, -0.995, 7.4, 50.0);
        self.n_out_p = s.add_node();
        self.n_out_n = s.add_node();
        s.add_c(self.n_out_p, n_spo, 100e-6);
        s.add_c(self.n_out_n, n_sn, 100e-6);
        s.add_r(self.n_out_p, 0, 10e3, false);
        s.add_r(self.n_out_n, 0, 10e3, false);
        s.add_r(self.n_out_p, self.n_out_n, 600.0, false);
    }

    fn calibrate(&mut self) {
        self.out_scale = 1.0;
        self.v_full = 1.0;
        self.last_gain = 0.0;
        self.set_gain(1.0);
        let amp = 1e-3;
        let pk = measure_peak(&mut self.s, self.n_sp, Some(self.n_sn), self.n_out_p, Some(self.n_out_n), amp, 0.25, true);
        self.out_scale = amp / pk;
        self.restore_dc();
    }
}

impl Circuit for Brit {
    const MAX_GAIN: f64 = 1000.0;

    fn prepare(&mut self, sample_rate: f64) -> bool {
        self.build();
        let ok = self.s.prepare(sample_rate);
        self.calibrate();
        ok
    }
    fn restore_dc(&mut self) {
        self.s.restore_dc();
        self.last_gain = 0.0;
    }
    fn restore_dc_at(&mut self, sample_rate: f64) {
        self.s.restore_dc_at(sample_rate);
        self.last_gain = 0.0;
    }
    fn set_full_scale_volts(&mut self, v: f64) {
        self.v_full = v;
    }
    /// Pot value giving the gain ratio, from G1 = 1 + 2000/(27+R), G2 = 1.02 + 2000/(39+R).
    fn set_gain(&mut self, g: f64) {
        if (g - self.last_gain).abs() <= 1e-4 * self.last_gain {
            return;
        }
        self.last_gain = g;
        let target = Self::gain_at(Self::RV_REF) * g.max(1.0);
        let (mut lo, mut hi) = (0.0, Self::RV_MAX);
        for _ in 0..40 {
            let mid = 0.5 * (lo + hi);
            if Self::gain_at(mid) > target {
                lo = mid;
            } else {
                hi = mid;
            }
        }
        let r = 0.5 * (lo + hi) + 1e-3;
        self.s.set_r(self.rv_a, r);
        self.s.set_r(self.rv_b, r);
    }
    fn process(&mut self, x: f64) -> f64 {
        let e = 0.5 * x * self.v_full;
        self.s.set_fixed(self.n_sp, e);
        self.s.set_fixed(self.n_sn, -e);
        self.s.step();
        (self.s.voltage(self.n_out_p) - self.s.voltage(self.n_out_n)) * self.out_scale / self.v_full
    }
}

// ---------------------------------------------------------------------------------------------
// FSF: transformer-coupled console mic channel. Mic transformer (1:5) -> attenuator ladder ->
// 12-position gain switch -> op-amp gain stage -> op-amp + class-AB pair into the output transformer
// whose tertiary closes the loop -> 600 ohm. Stepped gain with a +/-3 dB trim between steps.

pub const FSF_POSITIONS: usize = 12;

pub struct Fsf {
    s: Solver,
    n_sp: usize,
    n_sn: usize,
    n_out_p: usize,
    n_out_n: usize,
    r_m: [usize; 6],
    r_z: [usize; 7],
    pos: usize,
    trim: f64,
    pub gain_db: [f64; FSF_POSITIONS],
    pub out_scale: f64,
    pub v_full: f64,
}

impl Default for Fsf {
    fn default() -> Self {
        Self {
            s: Solver::new(),
            n_sp: 0,
            n_sn: 0,
            n_out_p: 0,
            n_out_n: 0,
            r_m: [0; 6],
            r_z: [0; 7],
            pos: Self::REF_POS,
            trim: 1.0,
            gain_db: [0.0; FSF_POSITIONS],
            out_scale: 1.0,
            v_full: 1.0,
        }
    }
}

impl Fsf {
    const RAIL: f64 = 15.0; // [NETLIST]
    const REF_POS: usize = 6;
    const MIC_L: f64 = 12.0; // [DATASHEET, derived]
    const MIC_LSAT: f64 = 0.6; // [ESTIMATE]
    const MIC_ISAT: f64 = 0.0004; // [ESTIMATE]
    const OUT_L: f64 = 2.0; // [PLACEHOLDER]
    const OUT_LSAT: f64 = 0.2; // [PLACEHOLDER]
    const OUT_ISAT: f64 = 0.15; // [PLACEHOLDER]

    /// Second channel: share the measured gain table instead of re-measuring it.
    pub fn prepare_like(&mut self, other: &Fsf, sample_rate: f64) -> bool {
        self.build();
        let ok = self.s.prepare(sample_rate);
        self.gain_db = other.gain_db;
        self.out_scale = other.out_scale;
        self.v_full = other.v_full;
        self.set_pos(Self::REF_POS);
        ok
    }

    fn reset_pos(&mut self) {
        let p = self.pos;
        self.pos = 0;
        self.set_pos(p);
    }

    /// RS1: deck M picks the input tap, deck Z the feedback tap. Closed 1 mOhm, open 1 GOhm.
    fn set_pos(&mut self, p: usize) {
        self.pos = p;
        let s = &mut self.s;
        let sw = |s: &mut Solver, idx: usize, closed: bool| s.set_r(idx, if closed { 1e-3 } else { 1e9 });
        for i in 0..5 {
            sw(s, self.r_m[i], p == i + 1);
        }
        sw(s, self.r_m[5], p >= 6);
        sw(s, self.r_z[0], p <= 6);
        for i in 1..7 {
            sw(s, self.r_z[i], p == 6 + i);
        }
    }

    fn build(&mut self) {
        self.s = Solver::new();
        let bc441 = NpnModel { is: 1e-13, bf: 100.0, br: 5.0, va: 100.0, cje: 60e-12, cjc: 30e-12, tf: 3.2e-9, pnp: false };
        let bc461 = NpnModel { pnp: true, ..bc441 };
        let (d4148_is, d4148_n) = (2.52e-9, 1.752);
        let s = &mut self.s;
        let vp = s.add_fixed_node(Self::RAIL);
        let vn = s.add_fixed_node(-Self::RAIL);
        self.n_sp = s.add_fixed_node(0.0);
        self.n_sn = s.add_fixed_node(0.0);
        let ne = |s: &mut Solver, p, n, o| s.add_opamp(p, n, o, 10e6, 6.0, 1e5, 13.0, 10.0, 100e3);

        let (inp, inn) = (s.add_node(), s.add_node());
        s.add_r(self.n_sp, inp, 75.0, false);
        s.add_r(self.n_sn, inn, 75.0, false);

        let nph = s.add_node();
        s.add_r(inp, nph, 3.6e3, false);
        s.add_r(inn, nph, 3.6e3, false);
        s.add_r(nph, 0, 2.4e3, false);

        let (n_tp, n_ts, s5) = (s.add_node(), s.add_node(), s.add_node());
        s.add_r(inp, n_tp, 22.0, false);
        s.add_l(n_tp, inn, Self::MIC_L, Self::MIC_LSAT, Self::MIC_ISAT);
        s.add_ideal(n_tp, inn, n_ts, 0, 5.0);
        s.add_r(n_ts, s5, 1760.0, false);

        let n_zb = s.add_node();
        s.add_c(s5, n_zb, 220e-12);
        s.add_r(n_zb, 0, 5.6e3, false);
        let (t_e, t_d, t_c, t_b, t_a) = (s.add_node(), s.add_node(), s.add_node(), s.add_node(), s.add_node());
        s.add_r(s5, t_e, 15e3, false);
        s.add_r(t_e, t_d, 9.1e3, false);
        s.add_r(t_d, t_c, 4.7e3, false);
        s.add_r(t_c, t_b, 2.2e3, false);
        s.add_r(t_b, t_a, 1e3, false);
        s.add_r(t_a, 0, 1e3, false);

        let wm = s.add_node();
        let taps = [t_a, t_b, t_c, t_d, t_e, s5];
        for (i, &t) in taps.iter().enumerate() {
            self.r_m[i] = s.add_r(t, wm, 1e9, true);
        }

        let (p1, n1, o1, n_dc) = (s.add_node(), s.add_node(), s.add_node(), s.add_node());
        s.add_c(wm, p1, 100e-6 + 100e-9);
        s.add_r(p1, 0, 220e3, false);
        ne(s, p1, n1, o1);
        s.add_c(n1, o1, 220e-12);
        s.add_r(o1, n_dc, 6.2e3, false);
        s.add_r(n_dc, n1, 6.2e3, false);
        s.add_c(n_dc, 0, 100e-6);
        let (n_y, t_x, t20, t21, t22) = (s.add_node(), s.add_node(), s.add_node(), s.add_node(), s.add_node());
        let (t23, t24, t25, n_g) = (s.add_node(), s.add_node(), s.add_node(), s.add_node());
        s.add_c(o1, n_y, 100e-6);
        s.add_r(n_y, t_x, 1.8e3, false);
        s.add_r(t_x, t20, 2.4e3, false);
        s.add_r(t20, t21, 1.2e3, false);
        s.add_r(t21, t22, 560.0, false);
        s.add_r(t22, t23, 270.0, false);
        s.add_r(t23, t24, 120.0, false);
        s.add_r(t24, t25, 56.0, false);
        s.add_r(t25, n_g, 56.0, false);
        s.add_c(n_g, 0, 100e-6);
        let ztaps = [t_x, t20, t21, t22, t23, t24, t25];
        for (i, &t) in ztaps.iter().enumerate() {
            self.r_z[i] = s.add_r(t, n1, 1e9, true);
        }

        let (p3, mtr) = (s.add_node(), s.add_node());
        s.add_c(o1, p3, 100e-6);
        s.add_r(p3, 0, 4.7e3, false);
        s.add_c(p3, mtr, 100e-6);
        s.add_r(mtr, 0, 10e3, false);

        let (n3, o3, q, q2, out) = (s.add_node(), s.add_node(), s.add_node(), s.add_node(), s.add_node());
        let (bl, bn, bp, en, ep) = (s.add_node(), s.add_node(), s.add_node(), s.add_node(), s.add_node());
        ne(s, p3, n3, o3);
        s.add_r(n3, q, 1.2e3, false);
        s.add_r(q, q2, 240.0, false);
        s.add_r(q2, out, 1.2e3, false);
        s.add_c(q, out, 22e-9);
        s.add_c(bl, q2, 100e-6);
        s.add_r(vp, bn, 1.8e3, false);
        s.add_diode(bn, o3, d4148_is, d4148_n);
        s.add_diode(o3, bp, d4148_is, d4148_n);
        s.add_r(bp, vn, 1.8e3, false);
        s.add_c(bn, bp, 47e-9);
        s.add_npn(vp, bn, en, bc441);
        s.add_r(en, out, 22.0, false);
        s.add_npn(vn, bp, ep, bc461);
        s.add_r(ep, out, 22.0, false);

        let (n_po, n_so, y, o, n_to) = (s.add_node(), s.add_node(), s.add_node(), s.add_node(), s.add_node());
        s.add_r(out, n_po, 10.0, false);
        s.add_l(n_po, 0, Self::OUT_L, Self::OUT_LSAT, Self::OUT_ISAT);
        s.add_ideal(n_po, 0, n_so, o, 1.0);
        s.add_r(y, n_so, 20.0, false);
        s.add_ideal(n_po, 0, n_to, 0, 0.5);
        s.add_r(bl, n_to, 10.0, false);

        self.n_out_p = y;
        self.n_out_n = o;
        s.add_r(y, 0, 1e6, false);
        s.add_r(o, 0, 1e6, false);
        s.add_r(y, o, 600.0, false);
    }

    fn calibrate(&mut self) {
        self.out_scale = 1.0;
        self.v_full = 1.0;
        self.trim = 1.0;
        for p in 1..=FSF_POSITIONS {
            self.s.restore_dc();
            self.set_pos(p);
            let amp = 1e-3 * 10f64.powf(-((p - 1) as f64 * 6.0) / 20.0);
            let pk = measure_peak(&mut self.s, self.n_sp, Some(self.n_sn), self.n_out_p, Some(self.n_out_n), amp, 0.15, true);
            self.gain_db[p - 1] = 20.0 * (pk / amp).log10();
        }
        self.out_scale = 10f64.powf(-self.gain_db[Self::REF_POS - 1] / 20.0);
        self.s.restore_dc();
        self.pos = 0;
        self.set_pos(Self::REF_POS);
    }
}

impl Circuit for Fsf {
    const MAX_GAIN: f64 = 75.0;

    fn prepare(&mut self, sample_rate: f64) -> bool {
        self.build();
        let ok = self.s.prepare(sample_rate);
        self.calibrate();
        ok
    }
    fn restore_dc(&mut self) {
        self.s.restore_dc();
        self.reset_pos();
    }
    fn restore_dc_at(&mut self, sample_rate: f64) {
        self.s.restore_dc_at(sample_rate);
        self.reset_pos();
    }
    fn set_full_scale_volts(&mut self, v: f64) {
        self.v_full = v;
    }
    /// Nearest switch position (with 0.5 dB hysteresis), the remainder as an input trim.
    fn set_gain(&mut self, g: f64) {
        let target = self.gain_db[Self::REF_POS - 1] + 20.0 * g.max(1e-3).log10();
        let mut best = self.pos;
        let mut best_err = (target - self.gain_db[self.pos - 1]).abs() - 0.5;
        for p in 1..=FSF_POSITIONS {
            let e = (target - self.gain_db[p - 1]).abs();
            if e < best_err {
                best_err = e;
                best = p;
            }
        }
        if best != self.pos {
            self.set_pos(best);
        }
        self.trim = 10f64.powf((target - self.gain_db[self.pos - 1]) / 20.0);
    }
    fn process(&mut self, x: f64) -> f64 {
        let e = 0.5 * x * self.trim * self.v_full;
        self.s.set_fixed(self.n_sp, e);
        self.s.set_fixed(self.n_sn, -e);
        self.s.step();
        (self.s.voltage(self.n_out_p) - self.s.voltage(self.n_out_n)) * self.out_scale / self.v_full
    }
}

// ---------------------------------------------------------------------------------------------
// A-Type: transformer-coupled 500-series mic preamp. 1:8 mic transformer -> discrete op-amp
// ([PLACEHOLDER] behavioural) with a T feedback network -> 1:2 output transformer -> output pad.

pub struct AType {
    s: Solver,
    n_sp: usize,
    n_sn: usize,
    n_out_p: usize,
    n_out_n: usize,
    r_pa: usize,
    r_pb: usize,
    r_high: usize,
    last_gain: f64,
    pub out_scale: f64,
    pub v_full: f64,
}

impl Default for AType {
    fn default() -> Self {
        Self { s: Solver::new(), n_sp: 0, n_sn: 0, n_out_p: 0, n_out_n: 0, r_pa: 0, r_pb: 0, r_high: 0, last_gain: 0.0, out_scale: 1.0, v_full: 1.0 }
    }
}

impl AType {
    const MIC_LSAT: f64 = 0.1; // [ESTIMATE] of 2 H
    const MIC_ISAT: f64 = 0.0024; // [ESTIMATE]
    const OUT_LSAT: f64 = 0.1; // [ESTIMATE] of 1 H
    const OUT_ISAT: f64 = 0.08; // [ESTIMATE]
    const POT: f64 = 10e3;

    fn stage_gain(high: bool, ra: f64) -> f64 {
        let rg = if high { 1.0 / (1.0 / 1e3 + 1.0 / 100.0) } else { 1e3 };
        let vw = 1.0 + 1e3 / rg;
        vw + ra * ((vw - 1.0) / 1e3 + vw / ((Self::POT - ra) + 1e3))
    }

    fn build(&mut self) {
        self.s = Solver::new();
        let s = &mut self.s;
        self.n_sp = s.add_fixed_node(0.0);
        self.n_sn = s.add_fixed_node(0.0);

        let (inp, inn) = (s.add_node(), s.add_node());
        s.add_r(self.n_sp, inp, 75.0, false);
        s.add_r(self.n_sn, inn, 75.0, false);

        let (n_c3, nph) = (s.add_node(), s.add_node());
        s.add_c(inp, n_c3, 470e-12);
        s.add_c(inn, n_c3, 470e-12);
        s.add_c(n_c3, 0, 100e-12);
        s.add_r(inp, nph, 6.8e3, false);
        s.add_r(inn, nph, 6.8e3, false);
        s.add_c(nph, 0, 100e-6);
        s.add_r(nph, 0, 150.0, false);

        let (n_t1p, n_t1s, tp1) = (s.add_node(), s.add_node(), s.add_node());
        s.add_r(inp, n_t1p, 30.0, false);
        s.add_l(n_t1p, inn, 2.0, 2.0 * Self::MIC_LSAT, Self::MIC_ISAT);
        s.add_ideal(n_t1p, inn, n_t1s, 0, 8.0);
        s.add_r(tp1, n_t1s, 1000.0, false);
        let n_z = s.add_node();
        s.add_c(tp1, n_z, 22e-12);
        s.add_r(n_z, 0, 10e3, false);
        s.add_r(tp1, 0, 100e3, false);

        let (n, a, w, n_p1, g, n_r11) = (s.add_node(), s.add_node(), s.add_node(), s.add_node(), s.add_node(), s.add_node());
        s.add_opamp(tp1, n, a, 20e6, 15.0, 3e5, 14.0, 5.0, 1e6);
        s.add_c(a, n, 100e-12);
        self.r_pa = s.add_r(a, w, Self::POT / 2.0, true);
        self.r_pb = s.add_r(w, n_p1, Self::POT / 2.0, true);
        s.add_r(n, w, 1e3, false);
        s.add_r(n_p1, g, 1e3, false);
        s.add_r(n, g, 1e3, false);
        s.add_r(n, n_r11, 100.0, false);
        self.r_high = s.add_r(n_r11, g, 1e9, true);
        s.add_c(g, 0, 1000e-6);
        s.add_c(g, 0, 470e-9);

        let (n_l, b, n_t2p, n_t2s) = (s.add_node(), s.add_node(), s.add_node(), s.add_node());
        self.n_out_p = s.add_node();
        self.n_out_n = s.add_node();
        s.add_c(a, n_l, 1000e-6);
        s.add_r(a, n_l, 39.0, false);
        s.add_l(n_l, b, 6e-6, 0.0, 0.0);
        s.add_r(b, 0, 10e3, false);
        s.add_r(b, n_t2p, 5.0, false);
        s.add_l(n_t2p, 0, 1.0, Self::OUT_LSAT, Self::OUT_ISAT);
        s.add_ideal(n_t2p, 0, n_t2s, self.n_out_n, 2.0);
        s.add_r(self.n_out_p, n_t2s, 20.0, false);
        s.add_r(self.n_out_p, self.n_out_n, 2.2e3, false);
        s.add_r(self.n_out_p, 0, 1e6, false);
        s.add_r(self.n_out_n, 0, 1e6, false);

        let (n_pa, n_pb) = (s.add_node(), s.add_node());
        s.add_r(self.n_out_p, n_pa, 1e3, false);
        s.add_r(n_pa, n_pb, 220.0, false);
        s.add_r(n_pb, self.n_out_n, 1e3, false);

        s.add_r(self.n_out_p, self.n_out_n, 600.0, false);
    }

    fn calibrate(&mut self) {
        self.out_scale = 1.0;
        self.v_full = 1.0;
        self.last_gain = 0.0;
        self.set_gain(1.0);
        let amp = 1e-3;
        let pk = measure_peak(&mut self.s, self.n_sp, Some(self.n_sn), self.n_out_p, Some(self.n_out_n), amp, 0.25, true);
        self.out_scale = amp / pk;
        self.restore_dc();
    }
}

impl Circuit for AType {
    const MAX_GAIN: f64 = 100.0;

    fn prepare(&mut self, sample_rate: f64) -> bool {
        self.build();
        let ok = self.s.prepare(sample_rate);
        self.calibrate();
        ok
    }
    fn restore_dc(&mut self) {
        self.s.restore_dc();
        self.last_gain = 0.0;
    }
    fn restore_dc_at(&mut self, sample_rate: f64) {
        self.s.restore_dc_at(sample_rate);
        self.last_gain = 0.0;
    }
    fn set_full_scale_volts(&mut self, v: f64) {
        self.v_full = v;
    }
    /// G = Vw + RA*((Vw - 1)/R13 + Vw/(RB + R15)), Vw = 1 + R13/Rg; MID first, then HIGH.
    fn set_gain(&mut self, g: f64) {
        if (g - self.last_gain).abs() <= 1e-4 * self.last_gain {
            return;
        }
        self.last_gain = g;
        let target = 2.0 * g.max(1.0);
        let high = target > Self::stage_gain(false, Self::POT) * 0.999;
        let (mut lo, mut hi) = (0.0, Self::POT);
        for _ in 0..40 {
            let mid = 0.5 * (lo + hi);
            if Self::stage_gain(high, mid) < target {
                lo = mid;
            } else {
                hi = mid;
            }
        }
        let ra = 0.5 * (lo + hi);
        self.s.set_r(self.r_pa, ra + 1e-3);
        self.s.set_r(self.r_pb, Self::POT - ra + 1e-3);
        self.s.set_r(self.r_high, if high { 1e-2 } else { 1e9 });
    }
    fn process(&mut self, x: f64) -> f64 {
        let e = 0.5 * x * self.v_full;
        self.s.set_fixed(self.n_sp, e);
        self.s.set_fixed(self.n_sn, -e);
        self.s.step();
        (self.s.voltage(self.n_out_p) - self.s.voltage(self.n_out_n)) * self.out_scale / self.v_full
    }
}
