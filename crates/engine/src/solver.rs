//! Small real-time nonlinear circuit solver: nodal analysis, trapezoidal integration, Newton iteration
//! per sample, double precision.
//!
//! Elements: resistor, capacitor, inductor (linear or flux-saturating), ideal transformer, BJT
//! (Ebers-Moll with Early effect), diode, behavioural op-amp parts, and fixed-voltage nodes. Node 0 is
//! ground. Everything is allocated in `prepare`; `step`, `set_r`, `set_fixed` and `restore_dc` do not
//! allocate.

#[derive(Debug, Clone, Copy)]
pub struct NpnModel {
    pub is: f64,
    pub bf: f64,
    pub br: f64,
    pub va: f64,
    pub cje: f64,
    pub cjc: f64,
    /// Forward transit time: diffusion charge tf * If.
    pub tf: f64,
    /// PNP device (all junction polarities and currents mirrored).
    pub pnp: bool,
}

impl NpnModel {
    pub const fn npn(is: f64, bf: f64, br: f64, va: f64, cje: f64, cjc: f64, tf: f64) -> Self {
        Self { is, bf, br, va, cje, cjc, tf, pnp: false }
    }
}

#[derive(Clone, Copy)]
struct Res {
    a: usize,
    b: usize,
    g: f64,
    var: bool,
}
#[derive(Clone, Copy)]
struct Cap {
    a: usize,
    b: usize,
    c: f64,
    v_prev: f64,
    i_prev: f64,
    geq: f64,
}
#[derive(Clone, Copy)]
struct Ind {
    a: usize,
    b: usize,
    l0: f64,
    l_sat: f64,
    isat: f64,
    ex: usize,
    phi_prev: f64,
    v_prev: f64,
}
#[derive(Clone, Copy)]
struct Idl {
    pa: usize,
    pb: usize,
    sa: usize,
    sb: usize,
    n: f64,
    ex: usize,
}
#[derive(Clone, Copy)]
struct Dio {
    a: usize,
    c: usize,
    is: f64,
    n: f64,
}
#[derive(Clone, Copy)]
struct Gm {
    p: usize,
    n: usize,
    node: usize,
    gm: f64,
    imax: f64,
}
#[derive(Clone, Copy)]
struct Leak {
    node: usize,
    vcl: f64,
    k: f64,
}
#[derive(Clone, Copy)]
struct Buf {
    p: usize,
    n: usize,
    out: usize,
    gain: f64,
    vmax: f64,
    g: f64,
}
#[derive(Clone, Copy)]
struct Bjt {
    c: usize,
    b: usize,
    e: usize,
    m: NpnModel,
    q_prev: f64,
    iq_prev: f64,
}
#[derive(Clone, Copy)]
struct LinEntry {
    row: usize,
    var: usize,
    val: f64,
}

const VT: f64 = 0.02585;
const V_LIM: f64 = 0.95;

#[derive(Clone, Default)]
pub struct Solver {
    node_fixed: Vec<bool>,
    node_val: Vec<f64>,
    idx: Vec<i32>,
    res: Vec<Res>,
    caps: Vec<Cap>,
    inds: Vec<Ind>,
    idls: Vec<Idl>,
    bjts: Vec<Bjt>,
    dios: Vec<Dio>,
    gms: Vec<Gm>,
    leaks: Vec<Leak>,
    bufs: Vec<Buf>,
    lin_entries: Vec<LinEntry>,
    fix_entries: Vec<LinEntry>,
    inv_pos: Vec<usize>,
    x_save: Vec<f64>,
    node_prev: Vec<f64>,
    node_target: Vec<f64>,
    x: Vec<f64>,
    x_dc: Vec<f64>,
    f: Vec<f64>,
    dx: Vec<f64>,
    jac: Vec<f64>,
    rhs: Vec<f64>,
    sol: Vec<f64>,
    jac_lin: Vec<f64>,
    bconst: Vec<f64>,
    col_pos: Vec<usize>,
    rp: Vec<usize>,
    nz_idx: Vec<usize>,
    rp_s: Vec<usize>,
    s_nz_idx: Vec<usize>,
    s_elim_rows: Vec<usize>,
    s_nz_start: Vec<usize>,
    s_nz_end: Vec<usize>,
    s_elim_start: Vec<usize>,
    pattern: Vec<bool>,
    cap_hist: Vec<(f64, f64)>,
    ind_hist: Vec<(f64, f64)>,
    static_ok: bool,
    fast_ok: bool,
    nz_start: Vec<usize>,
    nz_end: Vec<usize>,
    sr: f64,
    n: usize,
    node_unknowns: usize,
    h: f64,
    gmin: f64,
    dc: bool,
    pub iter_count: u64,
    pub fallbacks: u64,
    pub fail_count: u64,
    pub hard_fails: u64,
    consecutive_fails: u32,
}

#[inline]
fn exp_lim(u: f64) -> (f64, f64) {
    if u < V_LIM {
        let e = (u / VT).exp();
        (e, e / VT)
    } else {
        let e0 = (V_LIM / VT).exp();
        (e0 * (1.0 + (u - V_LIM) / VT), e0 / VT)
    }
}

#[inline]
fn flux(l: &Ind, i: f64) -> f64 {
    if l.isat <= 0.0 {
        return l.l0 * i;
    }
    l.l_sat * i + (l.l0 - l.l_sat) * l.isat * (i / l.isat).tanh()
}

#[inline]
fn flux_d(l: &Ind, i: f64) -> f64 {
    if l.isat <= 0.0 {
        return l.l0;
    }
    let t = (i / l.isat).tanh();
    l.l_sat + (l.l0 - l.l_sat) * (1.0 - t * t)
}

impl Solver {
    pub fn new() -> Self {
        Self { node_fixed: vec![true], node_val: vec![0.0], sr: 48000.0, h: 1e-5, gmin: 1e-12, ..Default::default() }
    }

    pub fn add_node(&mut self) -> usize {
        self.node_fixed.push(false);
        self.node_val.push(0.0);
        self.node_fixed.len() - 1
    }

    pub fn add_fixed_node(&mut self, v: f64) -> usize {
        self.node_fixed.push(true);
        self.node_val.push(v);
        self.node_fixed.len() - 1
    }

    #[inline]
    pub fn set_fixed(&mut self, node: usize, v: f64) {
        self.node_val[node] = v;
    }

    /// Returns the resistor's index so `set_r` can change it while running.
    pub fn add_r(&mut self, a: usize, b: usize, ohm: f64, variable: bool) -> usize {
        self.res.push(Res { a, b, g: 1.0 / ohm, var: variable });
        self.res.len() - 1
    }

    #[inline]
    pub fn set_r(&mut self, index: usize, ohm: f64) {
        self.res[index].g = 1.0 / ohm;
    }

    pub fn add_c(&mut self, a: usize, b: usize, farad: f64) {
        self.caps.push(Cap { a, b, c: farad, v_prev: 0.0, i_prev: 0.0, geq: 0.0 });
    }

    /// Inductor a->b. If isat > 0 the flux is lSat*i + (l0-lSat)*isat*tanh(i/isat).
    pub fn add_l(&mut self, a: usize, b: usize, l0: f64, l_sat: f64, isat: f64) {
        self.inds.push(Ind { a, b, l0, l_sat, isat, ex: 0, phi_prev: 0.0, v_prev: 0.0 });
    }

    /// Ideal transformer: v(sa)-v(sb) = n * (v(pa)-v(pb)).
    pub fn add_ideal(&mut self, pa: usize, pb: usize, sa: usize, sb: usize, ratio: f64) {
        self.idls.push(Idl { pa, pb, sa, sb, n: ratio, ex: 0 });
    }

    pub fn add_diode(&mut self, anode: usize, cathode: usize, is: f64, n: f64) {
        self.dios.push(Dio { a: anode, c: cathode, is, n });
    }

    /// Current imax*tanh(gm*(v(p)-v(n))/imax) injected into `node`.
    pub fn add_transconductor(&mut self, p: usize, n: usize, node: usize, gm: f64, imax: f64) {
        self.gms.push(Gm { p, n, node, gm, imax });
    }

    /// Steep leak pinning `node` near +/-vcl: imax at |v| = vcl, rising as (v/vcl)^16.
    pub fn add_clamp_leak(&mut self, node: usize, vcl: f64, imax: f64) {
        self.leaks.push(Leak { node, vcl, k: imax / vcl });
    }

    /// `out` driven through rout from a sharp-knee limited gain*(v(p)-v(n)).
    pub fn add_buffer(&mut self, p: usize, n: usize, out: usize, gain: f64, vmax: f64, rout: f64) {
        self.bufs.push(Buf { p, n, out, gain, vmax, g: 1.0 / rout });
    }

    /// Op-amp: input resistance, single dominant pole (gbw, aol), slew limit sr (V/us), output clip
    /// +/-vsw and output resistance.
    #[allow(clippy::too_many_arguments)]
    pub fn add_opamp(&mut self, inp: usize, inn: usize, out: usize, gbw: f64, sr: f64, aol: f64, vsw: f64, rout: f64, rin: f64) {
        let ci = 1e-12;
        let gm = 6.2831853 * gbw * ci;
        let imax = sr * 1e6 * ci;
        let n1 = self.add_node();
        self.add_r(inp, inn, rin, false);
        self.add_transconductor(inp, inn, n1, gm, imax);
        self.add_c(n1, 0, ci);
        self.add_r(n1, 0, aol / gm, false);
        self.add_clamp_leak(n1, 1.1 * vsw, imax);
        self.add_buffer(n1, 0, out, 1.0, vsw, rout);
    }

    pub fn add_npn(&mut self, c: usize, b: usize, e: usize, m: NpnModel) {
        self.bjts.push(Bjt { c, b, e, m, q_prev: 0.0, iq_prev: 0.0 });
        if m.cje > 0.0 {
            self.add_c(b, e, m.cje);
        }
        if m.cjc > 0.0 {
            self.add_c(b, c, m.cjc);
        }
    }

    /// Finish construction: allocate, solve the DC operating point, initialise reactive state.
    pub fn prepare(&mut self, sample_rate: f64) -> bool {
        self.h = 0.5 / sample_rate;
        let nn = self.node_fixed.len();
        self.idx = vec![-1; nn];
        let mut k = 0usize;
        for i in 0..nn {
            if !self.node_fixed[i] {
                self.idx[i] = k as i32;
                k += 1;
            }
        }
        self.node_unknowns = k;
        for l in &mut self.inds {
            l.ex = k;
            k += 1;
        }
        for t in &mut self.idls {
            t.ex = k;
            k += 1;
        }
        self.n = k;
        let n = k;
        self.x = vec![0.0; n];
        self.f = vec![0.0; n];
        self.dx = vec![0.0; n];
        self.rhs = vec![0.0; n];
        self.sol = vec![0.0; n];
        self.rp = vec![0; n];
        self.nz_start = vec![0; n];
        self.nz_end = vec![0; n];
        self.nz_idx = vec![0; n * n];
        self.jac = vec![0.0; n * n];
        self.x_save = vec![0.0; n];
        self.node_target = vec![0.0; nn];
        for c in &mut self.caps {
            c.geq = 2.0 * c.c * sample_rate;
        }
        self.order_columns();
        self.x.fill(0.0);

        self.static_ok = false;
        self.fast_ok = false;
        let mut ok = true;
        self.dc = true;
        for gm in [1e-3, 1e-5, 1e-7, 1e-9, 1e-12] {
            self.gmin = gm;
            ok = self.newton(200, 1e-9, 5.0) && ok;
        }
        if !ok {
            // Plain DC Newton failed: ramp the supplies and integrate to the operating point instead.
            self.dc = false;
            if self.settle(sample_rate) {
                self.dc = true;
                self.gmin = 1e-12;
                ok = self.newton(200, 1e-8, 5.0);
                if !ok {
                    ok = true; // the settled point is already a valid operating point
                }
            }
        }
        self.dc = false;
        self.gmin = 1e-12;
        self.x_dc = self.x.clone();
        self.node_prev = self.node_val.clone();
        self.sr = sample_rate;
        self.build_linear();
        self.fast_ok = true;
        self.init_history();
        self.build_symbolic();
        ok
    }

    pub fn sample_rate(&self) -> f64 {
        self.sr
    }

    pub fn size(&self) -> usize {
        self.n
    }

    /// Back to the stored DC operating point.
    pub fn restore_dc(&mut self) {
        self.x.copy_from_slice(&self.x_dc);
        self.node_prev.copy_from_slice(&self.node_val);
        self.init_history();
    }

    /// Change the sample rate and return to the DC operating point. The constant linear Jacobian and
    /// the frozen pivot order are rebuilt for the new rate (in place, no allocation).
    pub fn restore_dc_at(&mut self, sample_rate: f64) {
        self.sr = sample_rate;
        self.h = 0.5 / sample_rate;
        for c in &mut self.caps {
            c.geq = 2.0 * c.c * sample_rate;
        }
        self.build_linear();
        self.restore_dc();
    }

    #[inline]
    pub fn voltage(&self, node: usize) -> f64 {
        self.v(node)
    }

    /// Advance one sample (inputs already set with `set_fixed`).
    pub fn step(&mut self) {
        self.x_save.copy_from_slice(&self.x);
        if !self.newton(8, 1e-7, 5.0) {
            self.fail_count += 1;
            // Recovery: source-step the fixed nodes from last sample's values with a tight step limit.
            self.x.copy_from_slice(&self.x_save);
            self.node_target.copy_from_slice(&self.node_val);
            let mut ok = true;
            const STEPS: usize = 8;
            let mut k = 1;
            while k <= STEPS && ok {
                for i in 0..self.node_val.len() {
                    self.node_val[i] = self.node_prev[i] + (self.node_target[i] - self.node_prev[i]) * k as f64 / STEPS as f64;
                }
                ok = self.newton(40, 1e-7, 1.0);
                k += 1;
            }
            self.node_val.copy_from_slice(&self.node_target);
            if !ok || !self.x[0].is_finite() {
                self.hard_fails += 1;
                self.x.copy_from_slice(&self.x_save);
                self.consecutive_fails += 1;
                if self.consecutive_fails > 200 {
                    self.restore_dc();
                    self.consecutive_fails = 0;
                }
            } else {
                self.consecutive_fails = 0;
            }
        }
        self.node_prev.copy_from_slice(&self.node_val);
        for i in 0..self.caps.len() {
            let c = self.caps[i];
            let vn = self.v(c.a) - self.v(c.b);
            let c = &mut self.caps[i];
            c.i_prev = c.geq * (vn - c.v_prev) - c.i_prev;
            c.v_prev = vn;
        }
        for i in 0..self.inds.len() {
            let l = self.inds[i];
            let phi = flux(&l, self.x[l.ex]);
            let v = self.v(l.a) - self.v(l.b);
            let l = &mut self.inds[i];
            l.phi_prev = phi;
            l.v_prev = v;
        }
        for i in 0..self.bjts.len() {
            let q = self.bjts[i];
            if q.m.tf <= 0.0 {
                continue;
            }
            let vbe = (if q.m.pnp { -1.0 } else { 1.0 }) * (self.v(q.b) - self.v(q.e));
            let (e, _) = exp_lim(vbe);
            let qd = q.m.tf * q.m.is * (e - 1.0);
            let sr = self.sr;
            let q = &mut self.bjts[i];
            q.iq_prev = 2.0 * sr * (qd - q.q_prev) - q.iq_prev;
            q.q_prev = qd;
        }
        self.update_const();
    }

    fn init_history(&mut self) {
        for i in 0..self.caps.len() {
            let c = self.caps[i];
            let v = self.v(c.a) - self.v(c.b);
            self.caps[i].v_prev = v;
            self.caps[i].i_prev = 0.0;
        }
        for i in 0..self.inds.len() {
            let l = self.inds[i];
            self.inds[i].phi_prev = flux(&l, self.x[l.ex]);
            self.inds[i].v_prev = 0.0;
        }
        for i in 0..self.bjts.len() {
            let q = self.bjts[i];
            let u = ((if q.m.pnp { -1.0 } else { 1.0 }) * (self.v(q.b) - self.v(q.e))).min(V_LIM);
            self.bjts[i].q_prev = q.m.tf * q.m.is * ((u / VT).exp() - 1.0);
            self.bjts[i].iq_prev = 0.0;
        }
        if !self.bconst.is_empty() {
            self.update_const();
        }
    }

    #[inline(always)]
    fn v(&self, node: usize) -> f64 {
        let k = self.idx[node];
        if k >= 0 {
            self.x[k as usize]
        } else {
            self.node_val[node]
        }
    }

    #[inline(always)]
    fn add_f(&mut self, node: usize, val: f64) {
        let k = self.idx[node];
        if k >= 0 {
            self.f[k as usize] += val;
        }
    }

    #[inline(always)]
    fn add_j(&mut self, row: usize, col: usize, val: f64) {
        let r = self.idx[row];
        let c = self.idx[col];
        if r >= 0 && c >= 0 {
            let p = r as usize * self.n + self.col_pos[c as usize];
            self.jac[p] += val;
        }
    }

    #[inline(always)]
    fn add_jrc(&mut self, r: usize, col: usize, val: f64) {
        let c = self.idx[col];
        if c >= 0 {
            let p = r * self.n + self.col_pos[c as usize];
            self.jac[p] += val;
        }
    }

    #[inline(always)]
    fn add_jcr(&mut self, row: usize, c: usize, val: f64) {
        let r = self.idx[row];
        if r >= 0 {
            let p = r as usize * self.n + self.col_pos[c];
            self.jac[p] += val;
        }
    }

    #[inline(always)]
    fn add_jrr(&mut self, r: usize, c: usize, val: f64) {
        let p = r * self.n + self.col_pos[c];
        self.jac[p] += val;
    }

    #[inline(always)]
    fn branch(&mut self, a: usize, b: usize, i: f64, g: f64) {
        self.add_f(a, i);
        self.add_f(b, -i);
        self.add_j(a, a, g);
        self.add_j(a, b, -g);
        self.add_j(b, b, g);
        self.add_j(b, a, -g);
    }

    /// Linear elements (fixed resistors, capacitors, linear inductors, ideal transformers, gmin).
    fn stamp_linear(&mut self) {
        for k in 0..self.node_unknowns {
            self.f[k] += self.gmin * self.x[k];
            let p = k * self.n + self.col_pos[k];
            self.jac[p] += self.gmin;
        }
        for i in 0..self.res.len() {
            let r = self.res[i];
            if !r.var {
                let i_ = r.g * (self.v(r.a) - self.v(r.b));
                self.branch(r.a, r.b, i_, r.g);
            }
        }
        if !self.dc {
            for i in 0..self.caps.len() {
                let c = self.caps[i];
                let i_ = c.geq * (self.v(c.a) - self.v(c.b) - c.v_prev) - c.i_prev;
                self.branch(c.a, c.b, i_, c.geq);
            }
        }
        for i in 0..self.inds.len() {
            let l = self.inds[i];
            if l.isat <= 0.0 {
                self.stamp_ind(&l);
            }
        }
        for i in 0..self.idls.len() {
            let t = self.idls[i];
            let j = self.x[t.ex];
            self.add_f(t.sa, j);
            self.add_f(t.sb, -j);
            self.add_f(t.pa, -t.n * j);
            self.add_f(t.pb, t.n * j);
            self.add_jcr(t.sa, t.ex, 1.0);
            self.add_jcr(t.sb, t.ex, -1.0);
            self.add_jcr(t.pa, t.ex, -t.n);
            self.add_jcr(t.pb, t.ex, t.n);
            self.f[t.ex] += (self.v(t.sa) - self.v(t.sb)) - t.n * (self.v(t.pa) - self.v(t.pb));
            self.add_jrc(t.ex, t.sa, 1.0);
            self.add_jrc(t.ex, t.sb, -1.0);
            self.add_jrc(t.ex, t.pa, -t.n);
            self.add_jrc(t.ex, t.pb, t.n);
        }
    }

    fn stamp_ind(&mut self, l: &Ind) {
        let i = self.x[l.ex];
        let vab = self.v(l.a) - self.v(l.b);
        self.add_f(l.a, i);
        self.add_f(l.b, -i);
        self.add_jcr(l.a, l.ex, 1.0);
        self.add_jcr(l.b, l.ex, -1.0);
        if self.dc {
            self.f[l.ex] += vab;
            self.add_jrc(l.ex, l.a, 1.0);
            self.add_jrc(l.ex, l.b, -1.0);
        } else {
            self.f[l.ex] += flux(l, i) - l.phi_prev - self.h * (vab + l.v_prev);
            self.add_jrr(l.ex, l.ex, flux_d(l, i));
            self.add_jrc(l.ex, l.a, -self.h);
            self.add_jrc(l.ex, l.b, self.h);
        }
    }

    fn stamp_nonlinear(&mut self) {
        for i in 0..self.res.len() {
            let r = self.res[i];
            if r.var {
                let i_ = r.g * (self.v(r.a) - self.v(r.b));
                self.branch(r.a, r.b, i_, r.g);
            }
        }
        for i in 0..self.inds.len() {
            let l = self.inds[i];
            if l.isat > 0.0 {
                self.stamp_ind(&l);
            }
        }
        for i in 0..self.dios.len() {
            let d = self.dios[i];
            let (e, de) = exp_lim((self.v(d.a) - self.v(d.c)) / d.n);
            self.branch(d.a, d.c, d.is * (e - 1.0), d.is * de / d.n);
        }
        for i in 0..self.gms.len() {
            let t = self.gms[i];
            let vd = self.v(t.p) - self.v(t.n);
            let th = (t.gm * vd / t.imax).tanh();
            let cur = t.imax * th;
            let g = t.gm * (1.0 - th * th);
            self.add_f(t.node, -cur);
            self.add_j(t.node, t.p, -g);
            self.add_j(t.node, t.n, g);
        }
        for i in 0..self.leaks.len() {
            let l = self.leaks[i];
            let vn = self.v(l.node);
            let xx = vn / l.vcl;
            let x2 = xx * xx;
            let x4 = x2 * x2;
            let x8 = x4 * x4;
            let x16 = x8 * x8;
            self.add_f(l.node, l.k * vn * x16);
            self.add_j(l.node, l.node, 17.0 * l.k * x16);
        }
        for i in 0..self.bufs.len() {
            // Sharp-knee limiter x / (1 + x^8)^(1/8) with a continuous derivative.
            let b = self.bufs[i];
            let vd = self.v(b.p) - self.v(b.n);
            let xx = b.gain * vd / b.vmax;
            let x2 = xx * xx;
            let x4 = x2 * x2;
            let u = 1.0 + x4 * x4;
            let ui = u.powf(-0.125);
            let vt_ = b.vmax * xx * ui;
            let dv = b.gain * ui / u;
            let cur = (vt_ - self.v(b.out)) * b.g;
            self.add_f(b.out, -cur);
            self.add_j(b.out, b.out, b.g);
            self.add_j(b.out, b.p, -dv * b.g);
            self.add_j(b.out, b.n, dv * b.g);
        }
        for i in 0..self.bjts.len() {
            let q = self.bjts[i];
            let pol = if q.m.pnp { -1.0 } else { 1.0 };
            let (vb, vc, ve) = (self.v(q.b), self.v(q.c), self.v(q.e));
            let vbe = pol * (vb - ve);
            let vbc = pol * (vb - vc);
            let (ef, def) = exp_lim(vbe);
            let (er, der) = exp_lim(vbc);
            let i_f = q.m.is * (ef - 1.0);
            let i_r = q.m.is * (er - 1.0);
            let gf = q.m.is * def;
            let gr = q.m.is * der;
            let mut e = 1.0 - vbc / q.m.va;
            let mut d_e = -1.0 / q.m.va;
            if e < 0.2 {
                e = 0.2;
                d_e = 0.0;
            }
            let ict = (i_f - i_r) * e;
            let dict_be = gf * e;
            let dict_bc = -gr * e + (i_f - i_r) * d_e;
            let ic = ict - i_r / q.m.br;
            let ib = i_f / q.m.bf + i_r / q.m.br;
            let dic_be = dict_be;
            let dic_bc = dict_bc - gr / q.m.br;
            let dib_be = gf / q.m.bf;
            let dib_bc = gr / q.m.br;
            self.add_f(q.c, pol * ic);
            self.add_f(q.b, pol * ib);
            self.add_f(q.e, -pol * (ic + ib));
            let (dic_b, dic_c, dic_e) = (dic_be + dic_bc, -dic_bc, -dic_be);
            let (dib_b, dib_c, dib_e) = (dib_be + dib_bc, -dib_bc, -dib_be);
            self.add_j(q.c, q.b, dic_b);
            self.add_j(q.c, q.c, dic_c);
            self.add_j(q.c, q.e, dic_e);
            self.add_j(q.b, q.b, dib_b);
            self.add_j(q.b, q.c, dib_c);
            self.add_j(q.b, q.e, dib_e);
            self.add_j(q.e, q.b, -(dic_b + dib_b));
            self.add_j(q.e, q.c, -(dic_c + dib_c));
            self.add_j(q.e, q.e, -(dic_e + dib_e));
            if q.m.tf > 0.0 && !self.dc {
                // diffusion charge qd = tf * If(vbe), trapezoidal, base -> emitter
                let k = 2.0 * self.sr;
                let qd = q.m.tf * i_f;
                let iq = k * (qd - q.q_prev) - q.iq_prev;
                let g = k * q.m.tf * gf;
                self.add_f(q.b, pol * iq);
                self.add_f(q.e, -pol * iq);
                self.add_j(q.b, q.b, g);
                self.add_j(q.b, q.e, -g);
                self.add_j(q.e, q.b, -g);
                self.add_j(q.e, q.e, g);
            }
        }
    }

    fn assemble(&mut self) {
        self.f.fill(0.0);
        self.jac.fill(0.0);
        self.stamp_linear();
        self.stamp_nonlinear();
    }

    /// Transient assembly: constant linear part copied in, only the nonlinear devices stamped.
    fn assemble_fast(&mut self) {
        self.jac.copy_from_slice(&self.jac_lin);
        self.f.copy_from_slice(&self.bconst);
        for e in &self.lin_entries {
            self.f[e.row] += e.val * self.x[e.var];
        }
        for e in &self.fix_entries {
            self.f[e.row] += e.val * self.node_val[e.var];
        }
        self.stamp_nonlinear();
    }

    /// Constant Jacobian of the linear elements for the current sample rate. Allocates only the
    /// first time (from `prepare`); later calls reuse the buffers.
    fn build_linear(&mut self) {
        let n = self.n;
        self.dc = false;
        self.gmin = 1e-12;
        self.f.fill(0.0);
        self.jac.fill(0.0);
        self.stamp_linear();
        if self.jac_lin.len() != self.jac.len() {
            self.jac_lin = self.jac.clone();
        } else {
            self.jac_lin.copy_from_slice(&self.jac);
        }
        if self.inv_pos.len() != n {
            self.inv_pos = vec![0; n];
        }
        for v in 0..n {
            self.inv_pos[self.col_pos[v]] = v;
        }
        self.lin_entries.clear();
        for r in 0..n {
            for c in 0..n {
                let val = self.jac_lin[r * n + c];
                if val != 0.0 {
                    self.lin_entries.push(LinEntry { row: r, var: self.inv_pos[c], val });
                }
            }
        }
        if self.bconst.len() != n {
            self.bconst = vec![0.0; n];
        }

        // Couplings from the linear elements to fixed-voltage nodes: residual per volt on each.
        // The state is parked in x_save / node_target (already sized) instead of fresh copies.
        self.fix_entries.clear();
        self.x_save.copy_from_slice(&self.x);
        self.node_target.copy_from_slice(&self.node_val);
        // Reactive history is zeroed for the probe and restored afterwards (buffers sized once).
        self.cap_hist.resize(self.caps.len(), (0.0, 0.0));
        self.ind_hist.resize(self.inds.len(), (0.0, 0.0));
        for (c, h) in self.caps.iter_mut().zip(self.cap_hist.iter_mut()) {
            *h = (c.v_prev, c.i_prev);
            c.v_prev = 0.0;
            c.i_prev = 0.0;
        }
        for (l, h) in self.inds.iter_mut().zip(self.ind_hist.iter_mut()) {
            *h = (l.phi_prev, l.v_prev);
            l.phi_prev = 0.0;
            l.v_prev = 0.0;
        }
        self.x.fill(0.0);
        for k in 1..self.node_fixed.len() {
            if !self.node_fixed[k] {
                continue;
            }
            self.node_val.fill(0.0);
            self.node_val[k] = 1.0;
            self.f.fill(0.0);
            self.stamp_linear();
            for r in 0..n {
                if self.f[r] != 0.0 {
                    self.fix_entries.push(LinEntry { row: r, var: k, val: self.f[r] });
                }
            }
        }
        self.x.copy_from_slice(&self.x_save);
        self.node_val.copy_from_slice(&self.node_target);
        for (c, h) in self.caps.iter_mut().zip(&self.cap_hist) {
            (c.v_prev, c.i_prev) = *h;
        }
        for (l, h) in self.inds.iter_mut().zip(&self.ind_hist) {
            (l.phi_prev, l.v_prev) = *h;
        }
        self.update_const();
    }

    fn update_const(&mut self) {
        self.bconst.fill(0.0);
        for c in &self.caps {
            let cst = -c.geq * c.v_prev - c.i_prev;
            let ra = self.idx[c.a];
            let rb = self.idx[c.b];
            if ra >= 0 {
                self.bconst[ra as usize] += cst;
            }
            if rb >= 0 {
                self.bconst[rb as usize] -= cst;
            }
        }
        for l in &self.inds {
            if l.isat <= 0.0 {
                self.bconst[l.ex] += -l.phi_prev - self.h * l.v_prev;
            }
        }
    }

    /// Solve jac * dx = -f with numeric row pivoting over a fixed column order.
    fn solve_dynamic(&mut self) -> bool {
        let n = self.n;
        for i in 0..n {
            self.rhs[i] = -self.f[i];
            self.rp[i] = i;
        }
        let mut nz_top = 0;
        for c in 0..n {
            let mut p = c;
            let mut best = self.jac[self.rp[c] * n + c].abs();
            for r in c + 1..n {
                let a = self.jac[self.rp[r] * n + c].abs();
                if a > best {
                    best = a;
                    p = r;
                }
            }
            if best < 1e-300 {
                return false;
            }
            self.rp.swap(c, p);
            let prow = self.rp[c] * n;
            let inv = 1.0 / self.jac[prow + c];
            self.nz_start[c] = nz_top;
            for k in c + 1..n {
                if self.jac[prow + k] != 0.0 {
                    self.nz_idx[nz_top] = k;
                    nz_top += 1;
                }
            }
            self.nz_end[c] = nz_top;
            let bp = self.rhs[self.rp[c]];
            for r in c + 1..n {
                let row = self.rp[r] * n;
                if self.jac[row + c] == 0.0 {
                    continue;
                }
                let m = self.jac[row + c] * inv;
                for q in self.nz_start[c]..self.nz_end[c] {
                    let k = self.nz_idx[q];
                    self.jac[row + k] -= m * self.jac[prow + k];
                }
                self.rhs[self.rp[r]] -= m * bp;
            }
        }
        for ii in (0..n).rev() {
            let prow = self.rp[ii] * n;
            let mut sum = self.rhs[self.rp[ii]];
            for q in self.nz_start[ii]..self.nz_end[ii] {
                let k = self.nz_idx[q];
                sum -= self.jac[prow + k] * self.sol[k];
            }
            self.sol[ii] = sum / self.jac[prow + ii];
        }
        for v in 0..n {
            self.dx[v] = self.sol[self.col_pos[v]];
        }
        true
    }

    /// Fast path: pivot sequence and fill pattern frozen by `build_symbolic`. Returns false when a
    /// pivot has become unacceptably small.
    fn solve_static(&mut self) -> bool {
        let n = self.n;
        for i in 0..n {
            self.rhs[i] = -self.f[i];
        }
        for c in 0..n {
            let prow = self.rp_s[c];
            let pr = prow * n;
            let mut mx = self.jac[pr + c].abs();
            for q in self.s_nz_start[c]..self.s_nz_end[c] {
                mx = mx.max(self.jac[pr + self.s_nz_idx[q]].abs());
            }
            if !(self.jac[pr + c].abs() > 1e-9 * mx) || !(mx > 1e-290) {
                return false;
            }
            let inv = 1.0 / self.jac[pr + c];
            let bp = self.rhs[prow];
            for e in self.s_elim_start[c]..self.s_elim_start[c + 1] {
                let rrow = self.s_elim_rows[e];
                let row = rrow * n;
                let m = self.jac[row + c] * inv;
                for q in self.s_nz_start[c]..self.s_nz_end[c] {
                    let k = self.s_nz_idx[q];
                    self.jac[row + k] -= m * self.jac[pr + k];
                }
                self.rhs[rrow] -= m * bp;
            }
        }
        for ii in (0..n).rev() {
            let prow = self.rp_s[ii];
            let pr = prow * n;
            let mut sum = self.rhs[prow];
            for q in self.s_nz_start[ii]..self.s_nz_end[ii] {
                let k = self.s_nz_idx[q];
                sum -= self.jac[pr + k] * self.sol[k];
            }
            self.sol[ii] = sum / self.jac[pr + ii];
        }
        for v in 0..n {
            self.dx[v] = self.sol[self.col_pos[v]];
        }
        true
    }

    fn solve_linear(&mut self) -> bool {
        if self.static_ok {
            if self.solve_static() {
                return true;
            }
            self.fallbacks += 1;
            self.assemble_current();
        }
        self.solve_dynamic()
    }

    fn assemble_current(&mut self) {
        if self.dc || !self.fast_ok {
            self.assemble();
        } else {
            self.assemble_fast();
        }
    }

    /// Freeze the pivot order from one factorisation at the operating point and derive the
    /// elimination lists (including fill-in).
    fn build_symbolic(&mut self) {
        let n = self.n;
        self.static_ok = false;
        self.dc = false;
        self.assemble_fast();
        if !self.solve_dynamic() {
            return;
        }
        self.rp_s = self.rp.clone();
        let mut pt = self.pattern.clone();
        self.s_nz_start = vec![0; n];
        self.s_nz_end = vec![0; n];
        self.s_nz_idx.clear();
        self.s_elim_start = vec![0; n + 1];
        self.s_elim_rows.clear();
        for c in 0..n {
            let prow = self.rp_s[c];
            self.s_nz_start[c] = self.s_nz_idx.len();
            for k in c + 1..n {
                if pt[prow * n + k] {
                    self.s_nz_idx.push(k);
                }
            }
            self.s_nz_end[c] = self.s_nz_idx.len();
            self.s_elim_start[c] = self.s_elim_rows.len();
            for r in c + 1..n {
                let rrow = self.rp_s[r];
                if !pt[rrow * n + c] {
                    continue;
                }
                self.s_elim_rows.push(rrow);
                for q in self.s_nz_start[c]..self.s_nz_end[c] {
                    pt[rrow * n + self.s_nz_idx[q]] = true;
                }
            }
        }
        self.s_elim_start[n] = self.s_elim_rows.len();
        self.static_ok = true;
    }

    /// Pseudo-transient continuation to the DC operating point: supplies ramped from zero, backward
    /// Euler with growing time steps until the circuit stops moving.
    fn settle(&mut self, sample_rate: f64) -> bool {
        let target = self.node_val.clone();
        self.x.fill(0.0);
        self.node_val.fill(0.0);
        for c in &mut self.caps {
            c.v_prev = 0.0;
            c.i_prev = 0.0;
        }
        for l in &mut self.inds {
            l.phi_prev = 0.0;
            l.v_prev = 0.0;
        }
        for q in &mut self.bjts {
            q.q_prev = 0.0;
            q.iq_prev = 0.0;
        }
        self.dc = false;
        self.gmin = 1e-12;
        let ramp: f64 = 0.05;
        let t_end = 6.0;
        let mut t = 0.0;
        let mut dt = 1e-6;
        let mut guard = 0;
        let mut good = true;
        while t < t_end && guard < 40000 {
            guard += 1;
            let x_prev = self.x.clone();
            let tn = t + dt;
            let sc = (tn / ramp).min(1.0);
            for k in 0..self.node_val.len() {
                self.node_val[k] = target[k] * sc;
            }
            self.sr = 0.5 / dt;
            self.h = dt;
            for c in &mut self.caps {
                c.geq = c.c / dt;
            }
            if !self.newton(60, 1e-8, 1.0) {
                self.x = x_prev;
                dt *= 0.5;
                if dt < 1e-13 {
                    good = false;
                    break;
                }
                continue;
            }
            for i in 0..self.caps.len() {
                let c = self.caps[i];
                self.caps[i].v_prev = self.v(c.a) - self.v(c.b);
                self.caps[i].i_prev = 0.0;
            }
            for i in 0..self.inds.len() {
                let l = self.inds[i];
                self.inds[i].phi_prev = flux(&l, self.x[l.ex]);
                self.inds[i].v_prev = 0.0;
            }
            for i in 0..self.bjts.len() {
                let q = self.bjts[i];
                let (e, _) = exp_lim((if q.m.pnp { -1.0 } else { 1.0 }) * (self.v(q.b) - self.v(q.e)));
                self.bjts[i].q_prev = q.m.tf * q.m.is * (e - 1.0);
                self.bjts[i].iq_prev = 0.0;
            }
            t = tn;
            dt = (dt * 1.4).min(0.02);
        }
        self.node_val = target;
        self.sr = sample_rate;
        self.h = 0.5 / sample_rate;
        for c in &mut self.caps {
            c.geq = 2.0 * c.c * sample_rate;
        }
        good
    }

    /// Column elimination order: greedy minimum degree on the structural pattern of both the DC and
    /// transient Jacobians.
    fn order_columns(&mut self) {
        let n = self.n;
        self.col_pos = (0..n).collect();
        for i in 0..n {
            self.x[i] = 0.1 * (1.7 * i as f64 + 0.3).sin() + 0.4;
        }
        let mut pat = vec![false; n * n];
        for mode in 0..2 {
            self.dc = mode == 0;
            self.assemble();
            for k in 0..n * n {
                if self.jac[k] != 0.0 {
                    pat[k] = true;
                }
            }
        }
        self.dc = false;
        let mut adj: Vec<Vec<usize>> = vec![Vec::new(); n];
        for r in 0..n {
            for c in 0..n {
                if r != c && (pat[r * n + c] || pat[c * n + r]) {
                    adj[r].push(c);
                }
            }
        }
        let mut done = vec![false; n];
        let mut order = Vec::with_capacity(n);
        for _ in 0..n {
            let mut best = usize::MAX;
            let mut bd = n + 1;
            for v in 0..n {
                if done[v] {
                    continue;
                }
                let d = adj[v].iter().filter(|&&w| !done[w]).count();
                if d < bd {
                    bd = d;
                    best = v;
                }
            }
            done[best] = true;
            order.push(best);
            let nb: Vec<usize> = adj[best].iter().copied().filter(|&w| !done[w]).collect();
            for &a in &nb {
                for &b in &nb {
                    if a != b && !adj[a].contains(&b) {
                        adj[a].push(b);
                    }
                }
            }
        }
        for (pos, &v) in order.iter().enumerate() {
            self.col_pos[v] = pos;
        }
        let mut permuted = vec![false; n * n];
        for r in 0..n {
            for v in 0..n {
                if pat[r * n + v] {
                    permuted[r * n + self.col_pos[v]] = true;
                }
            }
        }
        self.pattern = permuted;
    }

    fn newton(&mut self, max_it: usize, tol: f64, step_lim: f64) -> bool {
        for _ in 0..max_it {
            self.iter_count += 1;
            self.assemble_current();
            if !self.solve_linear() {
                return false;
            }
            let mut mx = 0.0f64;
            for k in 0..self.node_unknowns {
                mx = mx.max(self.dx[k].abs());
            }
            let lim = if self.dc { 2.0 } else { step_lim };
            let sc = if mx > lim { lim / mx } else { 1.0 };
            let mut mi = 0.0f64;
            for k in 0..self.n {
                self.x[k] += sc * self.dx[k];
                if k >= self.node_unknowns {
                    mi = mi.max(self.dx[k].abs());
                }
            }
            if sc == 1.0 && mx < tol && mi < tol * 1e-2 {
                return true;
            }
        }
        false
    }
}
