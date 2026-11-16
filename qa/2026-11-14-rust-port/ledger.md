# Scope ledger: HybridEQ Rust port (OpenPluginSDK + GoodLookin DRI)

Project: HybridEQ   Branch / commit: rust-port (from bundle-release d45d08b)   Started: 2026-11-14   Agent: coding agent

Sources of scope:
- Request (2026-11-14): "port this project over to the openpluginsdk and have it be pure rust. I also want to upgrade the UI to the latest version of the GoodLookinDRI ... more efficient on dsp, not lose any features and look better and be more beautiful."
- Intake answers (2026-11-14): macOS first (JUCE 1.6.16 stays the Windows/Linux release); drop-in replacement, version bump "0.+1.0" (read as 1.7.0); add Autogain and Delta.
- Previous product: HybridEQ 1.6.16 (Source/, README.md, HANDOFF.md, Docs/UserManual.md, Designs/*.csv), installed bundles in /Library/Audio/Plug-Ins.

Identity of 1.6.16 (from the installed bundles):
- VST3 processor CID `ABCDEF019182FAEB4879627248794571`, controller `ABCDEF011234ABCD4879627248794571`
- AU `aufx` / `HyEq` / `Hybr`, name "OpenGrid: HybridEQ", bundle id `com.OpenGrid.HybridEQ`
- CLAP id `com.opengrid.hybrideq`; LV2 URI `https://github.com/MistaMin/HybridEQ`
- AAX: JUCE codes from manufacturer `Hybr`, product `HyEq` (exact plug-in type ids: to read from the JUCE AAX wrapper)

## Items

| ID | Item | Source | Code | GUI | Test | Evidence | Status |
|---|---|---|---|---|---|---|---|
| **DSP** | | | | | | | |
| S1 | Six-band EQ: Low Cut (6/12/18/24 dB/oct, Q), Low shelf, Mid 1, Mid 2 bells, High shelf, High Cut (6..24, Q) | PluginProcessor.cpp:516-609 | crates/engine/src/eq.rs | band strips | null vs 1.6.16 (tools/null_ref.sh; bundles: openplugin-port null) | dsp/evidence/null-vs-1.6.16-*.txt; qa/.../null-bundles (2026-11-16): -114..-145 dBFS max core; bundles RMS -126 dB | verified |
| S2 | Band types: Low/High Bax-EQ, Brit, FSF; Mid N-EQ (proportional Q), Brit, A-Type (fixed Q) | EQFilters.h:196-244 | crates/engine/src/eq.rs, biquad.rs | TYPE keys | null_ref eq-types | one case differs by design: 1.6.16 inverted polarity (FSF high shelf near Nyquist, fit landed on negated numerator); fixed, see Open questions | verified (with the polarity fix) |
| S3 | Mid/Side per band (Stereo / Mid / Side), M/S path only when any band uses it | EQFilters.h:508-567 | crates/engine/src/eq.rs | M/S keys | null_ref eq-ms | -118 dBFS max (FMA build), -144 (no-FMA build) | verified |
| S4 | Per-band bypass | EQFilters.h:323 | crates/engine/src/eq.rs | B keys | null_ref bypass | -inf (bit-identical) | verified |
| S5 | Cramping-free EQ: LM magnitude fit above 0.1 fs, warm start, cascade refine, cookbook fallback | BiquadFit.h | crates/engine/src/fit.rs | none: DSP | fit tests (4) + null_ref | cargo test -p hybrideq-engine 10/10 | verified |
| S6 | Coefficients redesigned only when inputs change | EQFilters.h:575 | | none | unit test | | todo |
| S7 | Preamp types Off, Brit, N-Type, FSF, A-Type; Pad -20/Unity/+10; Gain -12..+24; bypass | PluginProcessor.cpp:611-628 | crates/engine/src/preamp.rs, engine.rs | preamp card | null_ref *-hot, off-pad | bit-identical vs no-FMA C++; -82..-98 dBFS vs shipped (FMA rounding through drive) | verified |
| S8 | Static preamp models with residual ADAA (Hold while drive moves) | Preamp.h, Adaa.h | crates/engine/src/adaa.rs, preamp.rs | CIRCUIT unlit | null_ref static models | bit-identical vs no-FMA C++ | verified |
| S9 | Reduced Circuit engine (4 fitted models, ADAA at 1x via IntegratedCurve) | ReducedCircuit*.h, CurveADAA.h | crates/engine/src/reduced.rs | CIRCUIT lit | null_ref reduced-* | bit-identical vs no-FMA C++; bundles -110 dB RMS | verified |
| S10 | Circuit HQ: nodal Newton solver, 4 netlists, ~96 kHz internal rate, Lp8 decimation, calibration | CircuitSolver.h, *Circuit.h | crates/engine/src/solver.rs, circuits.rs | CIRCUIT HQ | null_ref core hq-* | -119..-128 dBFS max vs shipped build; through 2x oversampling the top-octave phase of the new anti-alias filter differs (Open questions) | verified (core); unverified (bundle, filter phase) |
| S11 | HQ forces 2x minimum oversampling; key shows effective factor with `*` | PluginProcessor.cpp:734; PluginEditor.cpp oversampleDisplayText | | RATE key | unit + screenshot | | todo |
| S12 | Oversampling 1x/2x/4x/8x, IIR (min-phase) or FIR (linear-phase) half-band filters | Resampler.h | plugin/src/multirate.rs | RATE, FILTER keys | multirate tests; impulse comparison vs 1.6.16 | IIR: same delay, mag within 0.017 dB 20 Hz-20 kHz; FIR: mag within 0.3 dB, latency 72 vs 61 (4x), 75 vs 66 (8x) | verified (behaviour); phase differs (Open questions) |
| S13 | Latency reported for oversampling filter; never changed from the audio thread except a deferred prepare that really changed it | PluginProcessor.cpp:645-684 | plugin/src/lib.rs latency() | none | conformance latency; multirate impulse test | conformance 2026-11-17 pass | verified |
| S14 | Harmonic saturation 2nd..5th, zero-at-origin polynomials, tanh | HarmonicSaturation.h | crates/engine/src/engine.rs harmonics | HARMONICS faders | null_ref harmonics; bundles | bit-identical no-FMA; bundles -114 dB RMS | verified |
| S15 | Output gain -24..+24 dB | PluginProcessor.cpp:630 | plugin/src/lib.rs | OUTPUT knob | null_ref off-pad | -121 dBFS | verified |
| S16 | Deterministic reset() for offline renders | PluginProcessor.cpp:688 | Engine::reset, HybridEq::reset | none | conformance reset | pass | verified |
| S17 | Realtime safety: no allocation, locks or panics in process (incl. circuit failure recovery, rate change) | HANDOFF 5.3; CircuitSolver.h:189 | engine, plugin | none | SDK conformance kit no_alloc/no_panic (222 sessions, 22.05-384 kHz) | plugin/tests/conformance.rs pass 2026-11-17 | verified |
| S18 | DSP efficiency: lower CPU than 1.6.16 in every mode, sound unchanged (null gate) | request | | none | CPU table before/after | | todo |
| S19 | Autogain: BS.1770 K-weighted loudness match, ~3 s, ±24 dB clamp, gate -70 LUFS, smoothed; default off | intake 2026-11-14 | | AUTO GAIN key | loudness within 0.5 LU on steady material | | todo |
| S20 | Delta: wet minus latency-aligned dry; default off; documented how output gain/autogain apply | intake 2026-11-14 | | DELTA key | null at unity | | todo |
| **Parameters, state, identity** | | | | | | | |
| S21 | All 45 parameters kept with the same ids, names, ranges, defaults, choice lists | PluginProcessor.cpp:491-635 | crates/engine/src/params.rs, plugin/src/params.rs | | scan diff 1.6.16 vs 1.7.0 (CLAP, VST3, AU) | qa/.../scan, scan-1.7: all 45 ids present, ranges equal in AU and CLAP | verified |
| S22 | Same normalised mapping as JUCE (log freq ranges, Q skew 0.5, linear gains) so automation keeps its meaning | PluginProcessor.cpp:430, 520 | plugin/src/params.rs (HostSpace, Taper::Skew) | | SDK tests + scan diff | CLAP/AU 0..1 like 1.6.16; VST3 normalized equal | verified |
| S23 | Display text and typed-value parsing round-trip (Hz/kHz, dB, Q, %) | PluginProcessor.cpp:448-488 | plugin/src/params.rs with_text | value readouts | param text round trip tests | engine params text_round_trip | verified |
| S24 | 1.6.x sessions load: JUCE XML state chunk import (all formats) incl. missing preampHQ restores as HQ | PluginProcessor.cpp:876; README 1.6.12 | plugin/src/state.rs | | state tests on captured 1.6.16 blobs | CLAP: works; VST3/AAX shells refuse foreign state (SDK gap); AU restores base-class parameter data | in progress |
| S25 | VST3 class ids identical to 1.6.16; VST3 parameter ids identical (JUCE hash) | moduleinfo.json | plugin/src/lib.rs vst3_ids, params pins | | scan of dist-rust VST3 | class ids and parameter ids equal; JUCE bypass param id differs | verified (ids); bypass id open |
| S26 | AU identity aufx/HyEq/Hybr; AU parameter ids identical | Info.plist | au_ids, pins; SDK AU version fix | | scan of dist-rust AU | aufx/HyEq/Hybr, ids equal, version 0x010700 | verified (scan); auval todo |
| S27 | AAX identity and parameter ids identical; page table | JUCE AAX wrapper | | | AAX Validator; Pro Tools session reopen | | todo |
| S28 | CLAP id com.opengrid.hybrideq; CLAP param ids identical | CMakeLists.txt:81 | PluginInfo.id, pins | | scan of dist-rust CLAP | ids equal; switches stepped as before | verified (scan); clap-validator todo |
| S29 | LV2 URI unchanged; port symbols | CMakeLists.txt:65 | | | LV2 load in REAPER | | todo |
| S30 | New state format saves and restores every parameter and the editor settings (size, spectrum, looks) | PluginProcessor.cpp:869 | | | save/reload round trip | | todo |
| S31 | Version 1.7.0 shown by hosts; AU version 0x010700 | intake | | footer | bundle plists | | todo |
| **Formats and build** | | | | | | | |
| S32 | macOS universal (arm64 + x86_64), macOS 11+: VST3, AU, AAX (PACE-signed), CLAP, LV2 | README | | | lipo, validators | | todo |
| S33 | Plugin code is pure Rust (format shells are the SDK's, AAX links Avid's SDK) | request | | | dependency audit | | todo |
| S34 | Windows / Linux | intake | | | | | out of scope (owner, 2026-11-14: 1.6.16 stays the Win/Linux release) |
| S35 | Standalone app | README: "not included in the current downloads" | | | | | out of scope (not shipped since 1.6.13; SDK standalone may be built for dev) |
| **GUI (GoodLookin DRI 0.2, lit tier)** | | | | | | | |
| S36 | One editor in every format; proportional resize 826x616..1770x1320 at 1180:880; size saved with the project | PluginEditor.cpp:296-302 | | whole editor | screenshot at 3 sizes | | todo |
| S37 | Header: title, OpenGrid, SPECTRUM key, tagline; footer with version | screenshot 1.6.14 | | header | screenshot | | todo |
| S38 | Frequency response display: grid, curve with fill, small analyser behind it, coloured draggable nodes (freq + gain), wheel changes Q with speed boost | EQDisplay.h | | response display | screenshot + click test | | todo |
| S39 | Full-window spectrum / spectrogram overlay with draggable EQ nodes, mode and close keys; small analyser paused while covered | SpectrumOverlay.h | | overlay | screenshot | | todo |
| S40 | Band strips: knobs with value readouts and range labels, drag, double-click reset, fine drag; choice keys cycle on click, menu on right click | RotaryKnob.h, CycleButton.h | | 6 strips | click test per control | | todo |
| S41 | Preamp card: PAD, GAIN, TYPE (menu on click), CIRCUIT key Off -> Circuit -> HQ, bypass | PreampModeKey.h | | preamp card | click test | | todo |
| S42 | Harmonics: 4 faders 2nd..5th | HarmonicsPanel.h | | harmonics | drag test | | todo |
| S43 | Global: RATE (effective factor), FILTER (dimmed at 1x), OUTPUT knob | PluginEditor.cpp | | global | screenshot | | todo |
| S44 | Level: VU (0 VU = -18 dBFS), peak dBFS readout with hold, IN/OUT LED ladders, latching clip lamp (click resets) | MeterPanel.h | | level | ballistics test | | todo |
| S45 | Section looks: knob style, colour and plate follow each section's type/slope (LookTriggers.csv), animated fade | LookTriggers.h, Designs/LookTriggers.csv | | all strips | screenshot per type | | todo |
| S46 | Bypassed sections dim their controls | PluginEditor.cpp refreshBypassStates | | strips | screenshot | | todo |
| S47 | Design data from Designs/*.csv embedded (knob styles, colours, spectrum settings) | Designs/ | | | unit test | | todo |
| S48 | Autogain and Delta keys on the panel | intake | | global/level | coverage + screenshot | | todo |
| S49 | Visual upgrade: lit knobs, shaded plates, better typography (Core Text), refined meters; owner judges "more beautiful" | request | | whole editor | before/after screenshots | | todo |
| S50 | Editor CPU: cached static layer; idle editor cheaper than 1.6.16 | Docs/UI_PERFORMANCE.md | | | frame timing | | todo |
| S51 | Developer tools (Design studio, Look studio, Gallery, LOOK/KNOBS menus) | PluginEditor.cpp (GOODLOOKINUI_ENABLE_EDITOR) | | | | | out of scope for 1.7.0 release builds (debug-only in 1.6.x; owner to confirm whether a Rust dev inspector is wanted) |
| **Docs and release** | | | | | | | |
| S52 | README, user manual, release notes, third-party notices updated; CMake/JUCE removed after parity | | | | review | | todo |

## Parameters (host list vs ledger)

Host list dumped from: <format, build>   Count: 45 own (+ host bypass)   All in a row above: S21 lists them by id

## Exceptions (each needs a reason)

| Parameter / check | Exception | Reason | Decided by |
|---|---|---|---|

## Open questions

- Version: "0.+1.0" read as 1.7.0; confirm if 2.0.0 was meant.
- Polarity fix (S2): 1.6.16 could invert the signal with a fitted shelf near Nyquist (FSF high shelf 20 kHz +10 dB at 48 kHz). 1.7.0 keeps positive polarity. Sound change vs 1.6.16 in that case only; owner to accept.
- Oversampling filters (S10, S12): the 1.6.x filters were JUCE's; 1.7.0 uses its own (SDK FIR designs). IIR (Min): same delay, magnitude within 0.02 dB, phase differs in the top octave, which through the HQ circuit changes high-frequency harmonics (null RMS -14 dB on a 1-20 kHz sweep, levels identical). FIR (Lin): latency 72 vs 61 samples at 4x. Owner: accept, or ask for a polyphase all-pass half-band (closer phase).
- VST3 host bypass parameter: 1.6.16's JUCE bypass id (1652125811) is not the SDK shell's; host bypass automation in old VST3 sessions does not carry over.
- Automation timing: 1.7.0 applies automation on the exact sample (1.6.16 at block start). Improvement; differs only around automation events.
- S51: is a developer design inspector wanted in the Rust build?

## Out of scope

| Item | Decided by | Where |
|---|---|---|
| Windows and Linux builds (S34) | owner, 2026-11-14 | intake question |
| Standalone app (S35) | not shipped since 1.6.13 | README |
