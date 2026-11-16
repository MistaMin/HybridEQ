//! The reduced "Circuit" engine: a fixed topology (nonlinear transformer -> bounded stage -> coupling
//! -> bounded stage -> nonlinear transformer -> fitted linear response) whose implicit maps are built
//! when prepared, so the audio path has no Newton solve. Coefficients come from
//! `tools/fit_reduced_physical.py` (offline circuit captures).

use crate::adaa::{CurveAdaa, IntegratedCurve};
use std::f64::consts::PI;

struct Model {
    p: [f64; 16],
    weights: [f64; 10],
}

#[rustfmt::skip]
const MODELS: [Model; 4] = [
    Model{p:[3.4635620060464252, -4.8283137373023015, 0.10749537422501478, 2.2199327312632549, 1.1397880378873568, -0.029999999999999995, 1.1909050925211386, 1.7211884396124519, -0.017126380861760354, 0.10000000006635328, -2.3025850929940455, 0.0, -6.9077552689821369, 0.80000000000000004, 1.5, -3.912023005428146], weights:[-0.051027014700247141, 1.0147285960960135, 0.04071772676604607, -0.0059189324142149774, 0.0024807350872779132, -0.0020380267175612505, 0.0043601357573329084, -0.017739953837939821, 0.084253147706033066, -0.31347742342023738]},
    Model{p:[9.2962668098380448, -2.5257286443082561, 1.6936183079101188, 3.5103374495383433, 1.0000000000000002, -0.10261805476027283, 1.3157129886567915, 1.0000000000000002, 0.11389376635083946, 2.5952738000312099, -3.8164150872894944, -0.48093753649140608, 7.8186794213314679, 0.68742639828395058, 2.0131208456335976, -2.3025850929940459], weights:[-1.8843530577138612, 1.2927099998502976, -0.40943183105429837, 0.0027759455253449764, -0.010824865052649636, 0.023944680997115157, -0.089464355446908464, 0.47042890169751689, 2.5367318143239634, -2.7109996031392112]},
    Model{p:[1.9614899944826065, -4.953569656149865, 1.8561247328110924, 1.0696904621666117, 7.9999999999874962, 0.0036411182125808822, 7.9997518321322403, 1.6059162184053761, -0.029999999999962529, 0.20391573737158439, -4.4171902703519503, 0.049999999999999378, -6.9077552689821369, 0.80000000000000004, 1.5, -3.912023005428146], weights:[1.3627500197009728, -0.35611127079856253, -0.0069700486636494145, 0.0010904593692582162, 0.0019987915469679572, -0.0052338951191835568, 0.023123884508892554, -0.44461342716640428, -0.27037431958792046, -0.047303148536259297]},
    Model{p:[9.2501948715265812, -4.4755449796119198, 3.1945180386784315, 1.5875287426440445, 7.9918439699616304, -0.001676205733051292, 1.6035332212564208, 7.999890999403342, 0.016908987970914814, 0.10000000000124405, -5.2983172994097094, -0.049999980609684198, -6.9077552689821369, 0.80000000000000004, 1.5, -3.912023005428146], weights:[0.27975041596629024, 0.64799912308206076, 0.070625193653605672, 0.0019381303978547751, 0.00014124292583202525, -0.0010931062654800352, 0.0041586752169487625, -0.019728494692957451, 0.098263306261785532, -0.37213236771910446]},
];

const RESPONSE_POLES: [f64; 10] = [0.3, 2.0, 15.0, 60.0, 250.0, 1000.0, 4000.0, 12000.0, 30000.0, 80000.0];

#[derive(Clone, Copy)]
struct Spec {
    input_hz: f64,
    input_flux: f64,
    input_ratio: f64,
    coupling_hz: f64,
    coupling_charge_hz: f64,
    coupling_threshold: f64,
    coupling_gate: f64,
    coupling_release: f64,
    rail1: f64,
    order1: f64,
    bias1: f64,
    rail2: f64,
    order2: f64,
    bias2: f64,
    output_hz: f64,
    output_flux: f64,
    output_ratio: f64,
    output_bias: f64,
    input_core: bool,
    output_core: bool,
    weights: [f64; 10],
}

impl Spec {
    fn for_model(model: usize) -> Self {
        let m = &MODELS[model];
        let p = &m.p;
        let mut s = Spec {
            input_hz: p[0],
            input_flux: p[1].exp(),
            input_ratio: 0.05,
            coupling_hz: p[2],
            coupling_charge_hz: 0.0,
            coupling_threshold: 1.0,
            coupling_gate: 1.5,
            coupling_release: 0.02,
            rail1: p[3],
            order1: p[4],
            bias1: p[5],
            rail2: p[6],
            order2: p[7],
            bias2: p[8],
            output_hz: p[9],
            output_flux: p[10].exp(),
            output_ratio: if model == 1 { 0.075 } else { 0.1 },
            output_bias: p[11],
            input_core: model != 0,
            output_core: model != 0,
            weights: m.weights,
        };
        if model == 1 {
            s.coupling_charge_hz = p[12].exp();
            s.coupling_threshold = p[13];
            s.coupling_gate = p[14];
            s.coupling_release = p[15].exp();
        }
        s
    }
}

const CURVE: usize = 2049;

/// Warped table with Catmull-Rom interpolation.
struct Curve {
    values: Box<[f64; CURVE]>,
    limit: f64,
    warped_limit: f64,
    scale: f64,
}

impl Curve {
    fn build(extent: f64, f: impl Fn(f64) -> f64) -> Self {
        let warped_limit = extent / (1.0 + extent);
        let scale = (CURVE - 1) as f64 / (2.0 * warped_limit);
        let mut values = Box::new([0.0; CURVE]);
        for (i, v) in values.iter_mut().enumerate() {
            let u = -warped_limit + i as f64 / scale;
            *v = f(u / (1.0 - u.abs()));
        }
        Self { values, limit: extent, warped_limit, scale }
    }

    #[inline]
    fn lookup(&self, x: f64) -> f64 {
        let u = x / (1.0 + x.abs());
        let position = ((u + self.warped_limit) * self.scale).clamp(0.0, (CURVE - 1) as f64);
        let i = (position as usize).min(CURVE - 2);
        let t = position - i as f64;
        let v = &self.values;
        if i == 0 || i >= CURVE - 2 {
            return v[i] + t * (v[i + 1] - v[i]);
        }
        let (a, b, c, d) = (v[i - 1], v[i], v[i + 1], v[i + 2]);
        b + 0.5 * t * (c - a + t * (2.0 * a - 5.0 * b + 4.0 * c - d + t * (3.0 * (b - c) + d - a)))
    }
}

struct Clip {
    curve: Curve,
    integrated: Option<IntegratedCurve>,
    rail: f64,
    order: f64,
    bias: f64,
    offset: f64,
    slope: f64,
}

impl Clip {
    fn exact_with(rail: f64, order: f64, bias: f64, offset: f64, slope: f64, x: f64) -> f64 {
        let v = x + rail * bias;
        let t = (v / rail).abs();
        let clip = if t > 1e6 { rail.copysign(v) } else { v / (1.0 + t.powf(2.0 * order)).powf(1.0 / (2.0 * order)) };
        (clip - offset) / slope
    }

    fn new(rail: f64, order: f64, bias: f64, antialias: bool) -> Self {
        let offset = rail * bias / (1.0 + bias.abs().powf(2.0 * order)).powf(1.0 / (2.0 * order));
        let slope = (1.0 + bias.abs().powf(2.0 * order)).powf(-1.0 - 1.0 / (2.0 * order));
        let f = |x: f64| Self::exact_with(rail, order, bias, offset, slope, x);
        let curve = Curve::build(64.0, f);
        let integrated = antialias.then(|| IntegratedCurve::new(64.0, f));
        Self { curve, integrated, rail, order, bias, offset, slope }
    }

    #[inline]
    fn process(&self, x: f64) -> f64 {
        if x.abs() < self.curve.limit {
            self.curve.lookup(x)
        } else {
            Self::exact_with(self.rail, self.order, self.bias, self.offset, self.slope, x)
        }
    }
}

#[derive(Default)]
struct Transformer {
    inverse: Option<Curve>,
    a: f64,
    b: f64,
    positive: f64,
    negative: f64,
    tail_slope: f64,
}

impl Transformer {
    fn new(rate: f64, hz: f64, flux: f64, ratio: f64, dc_bias: f64) -> Self {
        let a = PI * hz / rate;
        let b = 0.5 / (rate * flux);
        let r = ratio;
        let origin = dc_bias.tanh();
        let slope = r + (1.0 - r) * (1.0 - origin * origin);
        let tail_slope = r / slope + a;
        let positive = (1.0 - r) * (1.0 - origin) / slope;
        let negative = (1.0 - r) * (-1.0 - origin) / slope;
        let phi = |q: f64| (r * q + (1.0 - r) * ((q + dc_bias).tanh() - origin)) / slope;
        let inverse = Curve::build(8.0, |w| {
            if w == 0.0 {
                return 0.0;
            }
            let mut lo = (w - positive.max(negative)) / tail_slope - 1.0;
            let mut hi = (w - positive.min(negative)) / tail_slope + 1.0;
            for _ in 0..55 {
                let mid = 0.5 * (lo + hi);
                if phi(mid) + a * mid < w {
                    lo = mid;
                } else {
                    hi = mid;
                }
            }
            0.5 * (lo + hi)
        });
        Self { inverse: Some(inverse), a, b, positive, negative, tail_slope }
    }

    #[inline]
    fn process(&self, input: f64, state: &mut f64) -> f64 {
        let inv = self.inverse.as_ref().expect("prepared");
        let w = *state + self.b * input;
        let q = if w.abs() < inv.limit { inv.lookup(w) } else { (w - if w > 0.0 { self.positive } else { self.negative }) / self.tail_slope };
        let output = input - (self.a / self.b) * q;
        *state += 2.0 * self.b * output;
        output
    }
}

/// Everything one model needs at one rate. Built in `prepare`; immutable afterwards.
pub struct Coefficients {
    spec: Spec,
    first: Clip,
    second: Clip,
    input: Transformer,
    output: Transformer,
    input_g: f64,
    coupling_g: f64,
    output_g: f64,
    coupling_a: f64,
    coupling_d: f64,
    envelope_release: f64,
    response_g: [f64; 10],
}

impl Coefficients {
    fn new(rate: f64, spec: Spec, antialias: bool) -> Self {
        let g = |hz: f64| {
            let t = PI * hz / rate;
            t / (1.0 + t)
        };
        let mut response_g = [0.0; 10];
        for (r, p) in response_g.iter_mut().zip(RESPONSE_POLES) {
            *r = g(p);
        }
        Self {
            first: Clip::new(spec.rail1, spec.order1, spec.bias1, antialias),
            second: Clip::new(spec.rail2, spec.order2, spec.bias2, antialias),
            input: if spec.input_core { Transformer::new(rate, spec.input_hz, spec.input_flux, spec.input_ratio, 0.0) } else { Transformer::default() },
            output: if spec.output_core { Transformer::new(rate, spec.output_hz, spec.output_flux, spec.output_ratio, spec.output_bias) } else { Transformer::default() },
            input_g: g(spec.input_hz),
            coupling_g: g(spec.coupling_hz),
            output_g: g(spec.output_hz),
            coupling_a: PI * spec.coupling_hz / rate,
            coupling_d: PI * spec.coupling_charge_hz / rate,
            envelope_release: (-1.0 / (rate * spec.coupling_release)).exp(),
            response_g,
            spec,
        }
    }
}

/// The four models at the session rate times 1, 2, 4 and 8 (ADAA tables at 1x only).
pub struct Bank {
    coefficients: Vec<Coefficients>,
}

impl Bank {
    pub fn new(session_rate: f64) -> Self {
        let mut coefficients = Vec::with_capacity(16);
        for rate_index in 0..4 {
            for model in 0..4 {
                coefficients.push(Coefficients::new(session_rate * (1 << rate_index) as f64, Spec::for_model(model), rate_index == 0));
            }
        }
        Self { coefficients }
    }

    /// `kind` is the preamp type (1 = Brit .. 4 = A-Type), `factor` the oversampling factor.
    pub fn get(&self, kind: usize, factor: usize) -> &Coefficients {
        let model = kind.saturating_sub(1).min(3);
        let rate_index = if factor >= 8 { 3 } else if factor >= 4 { 2 } else if factor >= 2 { 1 } else { 0 };
        &self.coefficients[rate_index * 4 + model]
    }
}

#[inline(always)]
fn highpass(x: f64, state: &mut f64, g: f64) -> f64 {
    let v = (x - *state) * g;
    let low = v + *state;
    *state = low + v;
    x - low
}

/// Per-channel state of the reduced engine. Holds a model index and factor, not a reference, so it
/// can live beside the bank.
#[derive(Default, Clone, Copy)]
pub struct Kernel {
    kind: usize,
    factor: usize,
    use_adaa: bool,
    first_adaa: CurveAdaa,
    second_adaa: CurveAdaa,
    input_state: f64,
    coupling_state: f64,
    output_state: f64,
    envelope: f64,
    response: [f64; 10],
}

impl Kernel {
    pub fn configure(&mut self, bank: &Bank, kind: usize, factor: usize, antialias: bool) {
        let c = bank.get(kind, factor);
        self.kind = kind;
        self.factor = factor;
        self.use_adaa = antialias && c.first.integrated.is_some() && c.second.integrated.is_some();
        self.reset();
    }

    pub fn reset(&mut self) {
        self.input_state = 0.0;
        self.coupling_state = 0.0;
        self.output_state = 0.0;
        self.envelope = 0.0;
        self.response = [0.0; 10];
        self.first_adaa.reset();
        self.second_adaa.reset();
    }

    #[inline]
    pub fn process(&mut self, bank: &Bank, x: f64) -> f64 {
        if !x.is_finite() {
            self.reset();
            return 0.0;
        }
        let c = bank.get(self.kind, self.factor);
        let mut v = if c.spec.input_core { c.input.process(x, &mut self.input_state) } else { highpass(x, &mut self.input_state, c.input_g) };
        v = if self.use_adaa { self.first_adaa.process(v, c.first.integrated.as_ref().unwrap()) } else { c.first.process(v) };
        if c.spec.coupling_charge_hz > 0.0 {
            // Reduced transistor input loading: a passive quadratic port with an explicit
            // trapezoidal solution, no runtime Newton solve.
            self.envelope = x.abs().max(self.envelope * c.envelope_release);
            let gate = ((self.envelope - c.spec.coupling_gate) / c.spec.coupling_gate).clamp(0.0, 1.0);
            let d = c.coupling_d * gate * gate * (3.0 - 2.0 * gate);
            let rhs = v - self.coupling_state;
            let a = 1.0 + c.coupling_a;
            let mut coupled = rhs / a;
            if coupled > c.spec.coupling_threshold {
                let delta = rhs - a * c.spec.coupling_threshold;
                coupled = c.spec.coupling_threshold + 2.0 * delta / (a + (a * a + 4.0 * d * delta).sqrt());
            }
            self.coupling_state = 2.0 * (v - coupled) - self.coupling_state;
            v = coupled;
        } else {
            v = highpass(v, &mut self.coupling_state, c.coupling_g);
        }
        v = if self.use_adaa { self.second_adaa.process(v, c.second.integrated.as_ref().unwrap()) } else { c.second.process(v) };
        v = if c.spec.output_core { c.output.process(v, &mut self.output_state) } else { highpass(v, &mut self.output_state, c.output_g) };
        // fitted linear response: a weighted sum of one-pole high passes
        // summed in two lanes (even, odd) as the 1.6.x NEON path did, so the result is bit-identical
        let (mut even, mut odd) = (0.0, 0.0);
        for i in (0..10).step_by(2) {
            even += c.spec.weights[i] * highpass(v, &mut self.response[i], c.response_g[i]);
            odd += c.spec.weights[i + 1] * highpass(v, &mut self.response[i + 1], c.response_g[i + 1]);
        }
        even + odd
    }
}
