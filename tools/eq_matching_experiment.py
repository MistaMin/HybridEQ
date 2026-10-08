#!/usr/bin/env python3
"""Experiment: can EQ filters avoid cramping near Nyquist without oversampling?

Compares, for the plugin's own filter shapes (cookbook cut / shelf / bell biquads), four ways of designing the
digital biquad at the base sample rate against the REFERENCE: the same cookbook biquad designed at 8x the
sample rate (what the oversampled signal path effectively applies; the linear-phase oversampler is flat in
the pass-band, and its pre-echo is ignored, so only magnitude is compared):

  A   current design      cookbook (bilinear) biquad at the base rate, frequency clamped to 0.49 * fs
  B   analog-matched      poles by matched-z, numerator magnitude-matched at DC, Nyquist and a middle frequency
  B2  matched, capped     as B but the pole frequency is capped below Nyquist (for corner frequencies >= Nyquist)
  C   magnitude fit       5 biquad coefficients least-squares fitted to the analog magnitude (stable by design)

Run:  python3 tools/eq_matching_experiment.py   (needs numpy and scipy)
"""
import time
import numpy as np
from scipy.optimize import least_squares

# ---------------------------------------------------------------- analog prototypes and cookbook designs
def analog(kind, f, f0, Q, gdb=0.0):
    x = 1j * np.asarray(f, float) / f0
    A = 10 ** (gdb / 40)
    if kind == 'lp':   return 1 / (x * x + x / Q + 1)
    if kind == 'hp':   return x * x / (x * x + x / Q + 1)
    if kind == 'peak': return (x * x + x * A / Q + 1) / (x * x + x / (A * Q) + 1)
    sA = np.sqrt(A)
    if kind == 'ls':   return A * (x * x + sA / Q * x + A) / (A * x * x + sA / Q * x + 1)
    if kind == 'hs':   return A * (A * x * x + sA / Q * x + 1) / (x * x + sA / Q * x + A)
    raise ValueError(kind)

def denominator(kind, Q, gdb):
    """Analog denominator d2*x^2 + d1*x + d0 (x = s / w0)."""
    A = 10 ** (gdb / 40); sA = np.sqrt(A)
    return {'lp': (1, 1 / Q, 1), 'hp': (1, 1 / Q, 1), 'peak': (1, 1 / (A * Q), 1),
            'ls': (A, sA / Q, 1), 'hs': (1, sA / Q, A)}[kind]

def rbj(kind, fs, f0, Q, gdb=0.0):
    A = 10 ** (gdb / 40); w0 = 2 * np.pi * f0 / fs; al = np.sin(w0) / (2 * Q); c = np.cos(w0); sA = np.sqrt(A)
    if kind == 'lp':   b = [(1 - c) / 2, 1 - c, (1 - c) / 2]; a = [1 + al, -2 * c, 1 - al]
    elif kind == 'hp': b = [(1 + c) / 2, -(1 + c), (1 + c) / 2]; a = [1 + al, -2 * c, 1 - al]
    elif kind == 'peak': b = [1 + al * A, -2 * c, 1 - al * A]; a = [1 + al / A, -2 * c, 1 - al / A]
    elif kind == 'ls':
        b = [A * ((A + 1) - (A - 1) * c + 2 * sA * al), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - 2 * sA * al)]
        a = [(A + 1) + (A - 1) * c + 2 * sA * al, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - 2 * sA * al]
    else:
        b = [A * ((A + 1) + (A - 1) * c + 2 * sA * al), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - 2 * sA * al)]
        a = [(A + 1) - (A - 1) * c + 2 * sA * al, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - 2 * sA * al]
    b = np.array(b) / a[0]; a = np.array(a) / a[0]
    return b, a

def dmag(b, a, f, fs):
    z1 = np.exp(-1j * 2 * np.pi * np.asarray(f, float) / fs)
    return np.abs((b[0] + b[1] * z1 + b[2] * z1 ** 2) / (1 + a[1] * z1 + a[2] * z1 ** 2))

def stable(a):
    return bool(np.all(np.abs(np.roots([1, a[1], a[2]])) < 1.0))

# ---------------------------------------------------------------- designs B, B2, C
def design_matched(kind, fs, f0, Q, gdb, cap_poles=False):
    d2, d1, d0 = denominator(kind, Q, gdb)
    xs = np.roots([d2, d1, d0])
    fp = min(f0, 0.49 * fs) if cap_poles else f0
    zp = np.exp(xs * 2 * np.pi * fp / fs)                   # matched-z poles
    a = np.real(np.poly(zp)); a1, a2 = a[1], a[2]
    fm = f0 if f0 <= 0.46 * fs else 0.3 * fs
    wm = 2 * np.pi * fm / fs
    def D(w):  # |1 + a1 e^-jw + a2 e^-2jw|^2
        z1 = np.exp(-1j * w); return abs(1 + a1 * z1 + a2 * z1 ** 2) ** 2
    t0 = abs(analog(kind, 1e-6 * f0, f0, Q, gdb)) ** 2 * D(0.0)
    t1 = abs(analog(kind, 0.5 * fs, f0, Q, gdb)) ** 2 * D(np.pi)
    tm = abs(analog(kind, fm, f0, Q, gdb)) ** 2 * D(wm)
    A0, A1 = t0, t1
    ph0, ph1, ph2 = np.cos(wm / 2) ** 2, np.sin(wm / 2) ** 2, np.sin(wm) ** 2
    A2 = (tm - A0 * ph0 - A1 * ph1) / ph2
    s0, s1 = np.sqrt(max(A0, 0)), np.sqrt(max(A1, 0))
    b1 = (s0 - s1) / 2; W = (s0 + s1) / 2
    r = np.sqrt(max(W * W + A2, 0.0))
    b = np.array([(W + r) / 2, b1, (W - r) / 2])
    return b, np.array([1.0, a1, a2])

def _from_params(p):
    k1, k2 = np.tanh(p[3]), np.tanh(p[4])
    return p[:3], np.array([1.0, k1 * (1 + k2), k2])

def _to_params(b, a):
    k2 = np.clip(a[2], -0.999, 0.999); k1 = np.clip(a[1] / (1 + k2), -0.999, 0.999)
    return np.array([b[0], b[1], b[2], np.arctanh(k1), np.arctanh(k2)])

def design_fit(kind, fs, f0, Q, gdb, starts):
    f = np.unique(np.concatenate([np.geomspace(20, 0.499 * fs, 140), np.linspace(0.1 * fs, 0.499 * fs, 70)]))
    target = 20 * np.log10(np.abs(analog(kind, f, f0, Q, gdb)))
    def resid(p):
        b, a = _from_params(p)
        return 20 * np.log10(np.maximum(dmag(b, a, f, fs), 1e-12)) - target
    best = None
    for b, a in starts:
        try:
            res = least_squares(resid, _to_params(b, a), method='lm', max_nfev=400)
        except Exception:
            continue
        if best is None or res.cost < best.cost: best = res
    return _from_params(best.x)

# ---------------------------------------------------------------- cases (the plugin's real filter shapes)
def butter_q(order, i, q):
    qs = q if order == 1 else 1 / (2 * np.sin(np.pi * (2 * i + 1) / (2 * order)))
    return max(qs * q / 0.7071, 0.1)

def stages(case):
    """List of (kind, f0, Q, gdb) biquads the plugin would build for this case."""
    kind, f0, q, g, order = case
    if kind in ('lp', 'hp'): return [(kind, f0, butter_q(order, i, q), 0.0) for i in range(order)]
    return [(kind, f0, q, g)]

def evaluate(case, fs, evalf):
    ref = np.ones_like(evalf); ideal = np.ones_like(evalf)
    mags = {m: np.ones_like(evalf) for m in ('A', 'B', 'B2', 'C')}
    ok = {m: True for m in mags}; tfit = 0.0
    for kind, f0, q, g in stages(case):
        fos = 8 * fs
        b, a = rbj(kind, fos, min(f0, 0.49 * fos), q, g); ref *= dmag(b, a, evalf, fos)
        ideal *= np.abs(analog(kind, evalf, f0, q, g))
        bA, aA = rbj(kind, fs, min(f0, 0.49 * fs), q, g)
        bB, aB = design_matched(kind, fs, f0, q, g)
        bB2, aB2 = design_matched(kind, fs, f0, q, g, cap_poles=True)
        t = time.perf_counter()
        bC, aC = design_fit(kind, fs, f0, q, g, [(bA, aA), (bB2, aB2)])
        tfit += time.perf_counter() - t
        for m, (bb, aa) in {'A': (bA, aA), 'B': (bB, aB), 'B2': (bB2, aB2), 'C': (bC, aC)}.items():
            mags[m] *= dmag(bb, aa, evalf, fs); ok[m] = ok[m] and stable(aa)
    return ref, ideal, mags, ok, tfit

def build_cases():
    cases = []
    for order in (1, 2, 3, 4):
        for f0 in (8e3, 12e3, 16e3, 20e3, 25e3, 30e3):
            for q in (0.7071, 1.5):
                cases.append(('lp', f0, q, 0.0, order))
    for q in (0.5, 0.71, 0.85):
        for g in (12.0, -12.0):
            for f0 in (8e3, 12e3, 16e3, 20e3, 30e3):
                cases.append(('hs', f0, q, g, 1))
    for q in (0.5, 1.0, 4.0):
        for g in (12.0, -12.0):
            for f0 in (2e3, 6e3, 10e3, 15e3):
                cases.append(('peak', f0, q, g, 1))
    return cases

FLOOR = 1e-3   # -60 dB: levels below this are treated as inaudible when comparing responses

def db_err(m, ref):
    return 20 * np.log10((m + FLOOR) / (ref + FLOOR))

def family(case):
    kind = case[0]
    return {'lp': f'High Cut (order {case[4]})', 'hs': 'High shelf', 'peak': 'Bell'}[kind]

def main():
    t_all = time.perf_counter()
    rows = {}   # (family, fs) -> list of (case, metrics)
    for fs in (44100.0, 48000.0, 96000.0):
        top = 0.499 * fs
        evalf = np.geomspace(20, top, 500)
        aud = evalf <= min(20000.0, top)
        for case in build_cases():
            ref, ideal, mags, ok, tfit = evaluate(case, fs, evalf)
            entry = {'case': case, 'tfit': tfit, 'ok': ok}
            for m, mg in mags.items():
                err = db_err(mg, ref)
                entry[m] = (np.max(np.abs(err[aud])), np.max(np.abs(err)), np.sqrt(np.mean(err[aud] ** 2)))
            erri = db_err(mags['A'], ideal)
            entry['A_vs_ideal'] = np.max(np.abs(erri[aud]))
            entry['C_vs_ideal'] = np.max(np.abs(db_err(mags['C'], ideal)[aud]))
            entry['B_vs_ideal'] = np.max(np.abs(db_err(mags['B'], ideal)[aud]))
            entry['R_vs_ideal'] = np.max(np.abs(db_err(ref, ideal)[aud]))
            rows.setdefault((family(case), fs), []).append(entry)

    meths = ('A', 'B', 'B2', 'C')
    print(f'(Errors are measured with a -60 dB floor: differences deep in the stopband, where the level is below -60 dB, are not counted.)')
    print('Max |dB error| versus the 8x-oversampled reference, worst case over all cases in each group.')
    print('Columns: audible band 20 Hz-20 kHz  |  full band up to Nyquist.   A = current, B = matched, B2 = matched+capped poles, C = fit\n')
    print(f"{'group':<22}{'fs':>7} |" + ''.join(f'{m:>8}' for m in meths) + ' |' + ''.join(f'{m:>8}' for m in meths) + '  (cases)')
    for (fam, fs), lst in sorted(rows.items(), key=lambda kv: (kv[0][1], kv[0][0])):
        aud = [max(e[m][0] for e in lst) for m in meths]; full = [max(e[m][1] for e in lst) for m in meths]
        print(f'{fam:<22}{fs/1000:>6.1f}k |' + ''.join(f'{v:8.2f}' for v in aud) + ' |' + ''.join(f'{v:8.2f}' for v in full) + f'  ({len(lst)})')

    print('\nCorner frequency at or above Nyquist (44.1k / 48k, 30 kHz): worst audible-band error per group')
    for fam in sorted({k[0] for k in rows}):
        for fs in (44100.0, 48000.0):
            sel = [e for e in rows[(fam, fs)] if e['case'][1] >= 0.5 * fs]
            if not sel: continue
            print(f'  {fam:<20}{fs/1000:>5.1f}k ' + '  '.join(f'{m}={max(e[m][0] for e in sel):6.2f}' for m in meths) + f'   ({len(sel)} cases)')

    print('\nStability (all cases, all rates):  ' + '  '.join(
        f"{m}: {'all stable' if all(e['ok'][m] for l in rows.values() for e in l) else 'UNSTABLE in some cases'}" for m in meths))
    allc = [e for l in rows.values() for e in l]
    wins = {m: 0 for m in meths}
    for e in allc: wins[min(meths, key=lambda m: e[m][0])] += 1
    print('Best match to the reference (lowest audible-band max error), number of cases won: ' + ', '.join(f'{m}={wins[m]}' for m in meths) + f' of {len(allc)}')
    print(f"Fit cost: mean {1e3 * np.mean([e['tfit'] for e in allc]):.1f} ms per filter design in Python (runs only when a knob moves)")
    print(f'Current design (A) vs the ideal analog curve, worst audible error: {max(e["A_vs_ideal"] for e in allc):.2f} dB')
    print('\nVersus the IDEAL analog curve (audible band, worst case over all cases): '
          f"8x-oversampled reference {max(e['R_vs_ideal'] for e in allc):.2f} dB, "
          f"fit C {max(e['C_vs_ideal'] for e in allc):.2f} dB, matched B {max(e['B_vs_ideal'] for e in allc):.2f} dB")
    mid = [e for e in allc if e['case'][1] <= 12e3]
    print('For corner frequencies up to 12 kHz only (the normal range): ' +
          ', '.join(f"{m} {max(e[m][0] for e in mid):.2f} dB" for m in meths) + ' versus the 8x reference')
    print(f'Total run time {time.perf_counter() - t_all:.0f} s')

if __name__ == '__main__':
    main()
