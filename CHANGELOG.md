# Changelog

## 2.0.1

Built on the current OpenPluginSDK. The sound, the plug-in and parameter IDs and saved projects are
unchanged: renders are bit-identical to 2.0.0.

- VST3: when a host starts processing from its main thread while audio already runs (REAPER does),
  HybridEQ no longer passes one block through unprocessed and resets mid-stream. That could be heard
  now and then as a short knock.
- Linux VST3: in a host that embeds the editor in its own area (Ardour), the editor takes that
  area's width and keeps its proportions, and follows a host window that is replaced while open.
- Audio Unit: the component version is 2.0.1, so hosts take the build as an update.

## 2.0.0

HybridEQ rewritten in Rust on OpenPluginSDK, with the GoodLookin DRI editor. A drop-in replacement
for 1.6.16: the same plug-in IDs in every format, the same parameter IDs and ranges, and 1.6
projects open with every setting.

- New: AUTO GAIN, a BS.1770 loudness match of output to input (3 s window, -70 LUFS gate, ±24 dB,
  50 ms glide). Initially off.
- New: DELTA, the processed signal minus the latency-aligned input. Initially off.
- New: the editor resizes in proportion from 826 x 616 to 1770 x 1320 points; the size is saved with
  the project. Its text is Source Sans Pro Semibold, drawn at the window's own size on every screen.
- New: Windows ARM64 and Linux aarch64 builds; Windows ships as an installer.
- Automation lands on the exact sample (1.6 applied it at the start of each block).
- The FSF high shelf no longer inverts the signal's polarity near the top of the spectrum.
- Own oversampling filters: IIR keeps 1.6's delay and level; FIR adds 11 samples of latency at 4x and
  9 at 8x.
- VST3: the host bypass is a latency-aligned, sample-accurate crossfade with a new parameter ID;
  automation of the DAW's bypass switch in 1.6 VST3 projects does not carry over.
- No standalone application (none has shipped since 1.6.13).

See "What changed from 1.6" in the user manual for how and why.

## 1.6.16

- Editor drawing returns to the 1.6.13 path: the cached artwork from 1.6.14 is removed, and knobs use the 1.6.13 spring animation.
- The full-window analyser declares itself opaque, so the controls beneath it are no longer redrawn every frame. Overlay-open CPU on the development Mac fell from about 61% to about 35%.
- The small analyser behind the EQ curve pauses while the full-window view covers it.
- Audio DSP, host parameter IDs, FFT resolution and saved settings are unchanged.
- Includes a current user manual and plugin-manager bundle archives.

## 1.6.14

- Knob indicators follow their current values immediately, removing timer-dependent visual lag.
- Unchanged knobs, section panels and chassis artwork are cached at the current display scale.
- EQ automation notifications are coalesced; hidden and idle display work is reduced.
- Spectrograms colour newly arrived rows instead of repainting the complete history.
- Audio DSP, host parameter IDs, FFT resolution and saved settings are unchanged.
- Includes a current user manual and plugin-manager bundle archives.
- Local rendering and processor tests pass. Performance on the reported Windows / Iris Xe machine still needs user confirmation; local microbenchmarks are not Windows frame-rate measurements.

## 1.6.13

- **Avid AAX format**: Official signed and notarized AAX plugin included in the freeware macOS installer and DMG, digitally signed with PACE Eden credentials for Avid Pro Tools.
- **Toggle controls**: Slope, M/S, Band Type, and Preamp Pad keys now cycle on left-click as immediate toggles rather than opening dropdowns (right-click still opens the full choice menu).
- **Preamp Type menu**: Retains dropdown menu navigation with `Off` as the first item on the list and `Brit` as the initial default.
- **2x Oversampling**: Added `2x` as a direct user-selectable oversampling rate alongside `1x`, `4x`, and `8x`.
- **Selectable anti-aliasing filter**: Added `FILTER` key in Global section allowing selection between `IIR (Min)` (minimum-phase polyphase IIR, zero/low latency, no pre-ringing) and `FIR (Lin)` (linear-phase equiripple FIR) across all oversampled rates.

## 1.6.12

- **Three preamp modes on one key.** The CIRCUIT key now steps Off -> Circuit -> Circuit HQ.
  - Off: the light static models (with ADAA, as in 1.6.10).
  - Circuit: the reduced circuit model from OpenPreamp, light enough for 1x with no
    latency, with its own ADAA at 1x. Its coefficient tables cover 1x, 2x, 4x and 8x.
  - Circuit HQ: the full component-level solver, which is what Circuit on always meant.
    Sessions saved before 1.6.12 open as Circuit HQ.
  - Stored as the existing `preampCircuit` plus a new appended `preampHQ` parameter
    (default on), so saved sessions and host automation keep their meaning.
- **New interface rendering** (GoodLookinUI 0.2.3-4): shaded, fluted 3D knobs, recessed
  rocker switches and an upgraded VU meter. Layout, colours and designs are unchanged;
  the toolkit's `flat_` styles remain available.
- **Universal binary.** The plug-ins and standalone app contain Apple silicon and Intel
  code and require macOS 11 or later; the installer accepts both architectures.
- Includes 1.6.11: Oversample key shows the real factor and keeps updating, the KNOBS
  style picker is developer-only again, and the Spectrum key takes the title bar's colour.
- Tests: `tests/engine_modes.cpp` (`HybridEQEngineTest`): every type x mode x oversampling
  x rate for finite and bounded output, effective factor and latency per mode, live mode
  switching, and old-session restore.

## 1.6.11

- The Oversample key now shows the factor that is really running and no longer shows a stray `*` when your own choice is in use. It keeps updating after the interface has settled (with Circuit on at 1x it reads `2x*`).
- The KNOBS style picker is developer-only again and no longer appears in release builds; releases always use each knob's own design.
- The Spectrum key in the title bar takes the bar's colour instead of the cream control-key colour.

## 1.6.10

- **ADAA on the preamp with Circuit off.** The static N-Type, Brit, A-Type and FSF models now use first-order
  antiderivative anti-aliasing (`Source/DSP/Adaa.h`) on every nonlinear stage, so the harmonics they generate fold back
  less, even at 1x (measured 5 to 14 dB lower aliasing on hard-driven tones, 4.4 and 10 kHz). It works on the
  nonlinear part only: quiet signals keep their level at every frequency (the linear path is a unity-magnitude
  all-pass, about half a sample of delay, no reported latency). ADAA holds off while the Drive control is moving.
  N-Type's feedback-loop stages are not ADAA'd (their antiderivative proved too noisy); its limiters and cores are.
- **Circuit needs only 2x.** Turning Circuit on now raises Oversample to a minimum of 2x (it was 4x). The choice list
  is still 1x/4x/8x; 4x and 8x remain available.
- Tests: `tests/adaa.cpp` (antiderivatives against their functions, transparency for linear stages, alias reduction).

## 1.6.9

- **Cramping-free EQ without oversampling.** The High Cut, High shelf and bells (corner above 10 % of the sample
  rate) are now biquads fitted by least squares to the analog curve (`Source/DSP/BiquadFit.h`), so a corner near or
  above Nyquist follows the analog response in-band instead of bending. At the base rate they stay within about
  0.4 dB of the same plug-in running 8x oversampled; the steepest, highest-Q High Cut slopes within about 1.2 dB.
  The filters are ordinary IIR biquads (no latency); the fit runs only when a control changes.
- The EQ no longer forces extra oversampling for the High Cut and High shelf, so with Oversample at 1x and
  Circuit off the plug-in has no latency (Circuit forces a 2x minimum since 1.6.10).
- The reported latency is now set from `prepareToPlay` (it was never reported after the audio-thread call was removed).
- Tests: `tests/eq_fit.cpp` (fit accuracy, stability, speed, continuity), `tests/eq_integration.cpp` (`HybridEQEqTest`:
  1x versus 8x on the real processor, latency); `tools/eq_matching_experiment.py` is the Python experiment behind it.
- Magnitude is matched, phase is not (it cannot follow the analog phase near Nyquist).

## 1.6.8

- Built with **JUCE 9.0.3** (JUCE 9 End User Licence Agreement; was JUCE 8.0.15). The bundled-library license
  texts are regenerated for JUCE 9 (adds Opus, libwebp and LunaSVG).

## 1.6.7

- Built with **JUCE 8.0.15** (was 8.0.4). Its bundled VST3 SDK is MIT licensed, so VST3 builds no longer depend on
  the older dual-license (Steinberg license / GPLv3) SDK. Third-party license texts in `Licenses/third-party` are
  regenerated from the new JUCE tree (`tools/collect_licenses.py`).

## 1.6.6

- **LV2 and CLAP** are built alongside VST3, AU and the standalone, and are installed by the macOS installer
  (`/Library/Audio/Plug-Ins/CLAP` and `/Library/Audio/Plug-Ins/LV2`). A GitHub Actions workflow builds the Linux
  versions (VST3, LV2, CLAP, standalone).
- **Licenses:** the license texts of all bundled third-party components are in `Licenses/third-party` and ship
  with the source, the installer, the disk image and inside every plug-in bundle.

## 1.6.5

### A-Type

Adds **A-Type**, a fourth preamp flavour with its own circuit simulation
(`ATypeCircuit.h`): 1:8 mic
transformer, discrete op-amp gain stage with a T feedback network (gain pot,
and a MID/HIGH ground-leg switch), 1:2 output transformer and output pad.

- At 0 dB drive the gain is unity. Drive first turns the gain pot up, then
  moves to the HIGH position and continues with the pot (up to about 40 dB).
- The netlist's discrete op-amp is a behavioural placeholder (single-pole,
  slew-limited, clipped) until its schematic is available, so A-Type is very
  clean until it clips (about +25 dBu); both transformers' inductances and
  resistances are placeholders too, and their saturation knees are estimates.
- The input pad position is not modelled (the Pad control does that job).
- A-Type is the last entry of the Type list so saved sessions keep their stored choices.
- Light static model when **Preamp Circuit** is off, as for the others.

### Circuit simulations (Brit, N-Type, FSF)

With **Preamp Circuit** on, all three preamp flavours are now real circuit
simulations of transcribed netlists (kept privately and not distributed) instead of
behavioural approximations. Every resistor, capacitor, transistor, op-amp and
saturating transformer core is solved together every sample
(`Source/DSP/CircuitSolver.h`):

- **Brit** (`BritCircuit.h`): a
  balanced 500-series console mic preamp: matched-pair transistor input stage
  with op-amp current feedback, instrumentation-amp second stage, difference
  amp, DC servos and a balanced line driver. The gain control is the dual-gang
  pot. The balanced pair cancels even harmonics, so the colour is odd-order
  and very clean until the line driver clips (about +21 dBu).
- **N-Type** (`NTypeCircuit.h`):
  transformer-coupled line path: input transformer, three-transistor feedback
  preamp, class-A output stage and a gapped output transformer. Drive changes
  the gain-network resistor like the real gain switch. Asymmetric even-order
  colour from the class-A stage and the DC-biased output transformer.
- **FSF** (`FsfCircuit.h`):
  transformer-coupled console mic channel: mic transformer, 12-position gain
  network, two op-amp stages and a class-AB output stage inside a transformer
  feedback loop. Simulated gain is -7.9 to +59.1 dB across the 12 positions
  (the manual states -6 to +60 dB). The hardware gain is stepped, so drive picks
  the nearest position and covers the remainder with a trim of up to 3 dB.

Common behaviour:

- At 0 dB drive the small-signal gain is unity. Drive raises the circuit's
  gain; whatever a circuit cannot supply is applied as an input trim.
- CPU: this is a heavy mode. Each circuit runs at about 96 kHz internally
  (decimated from the oversampled rate with zero-latency IIR filters), roughly
  3x realtime per channel on a loaded Apple-silicon Mac.
- Op-amps are single-pole, slew-limited, clipped behavioural models (as in the
  netlists); they capture gain, bandwidth, slew and clipping, not noise.
  Transistor series resistances are not modelled.
- **Estimates, not published:** transformer inductance (N-Type input 100 H,
  output 4 H), saturation knees, leakage and winding capacitance; the FSF output
  transformer's turns, inductance and resistance (placeholders in the netlist).
  Measured values or response plots would let these be fitted.
- With **Preamp Circuit** off, each flavour uses a light static model.
- Rename: the preamp, EQ-shape and file names now use only N-Type, Brit,
  A-Type, FSF and Baxandall.
- Replaces the 1.6.4 transformer model and the 1.6.2 Brit behavioural model.

## 1.6.4 (superseded by 1.6.5)

- N preamp: input and output transformers are now flux-state circuit models instead of memoryless saturators plus an envelope-driven filter (with "Preamp Circuit" on). Low-frequency corner shift and low-frequency harmonics now come from the core's inductance falling as it saturates; the output transformer carries a standing DC bias, giving even-order harmonics.
- Datasheet values (transformer design guide, preliminary, Issue 1e): VTB 9046 input at 2:1 turns, 2k4 source into 2k4 load (0 dB), DCR 175 || 175 primary and 56 + 56 secondary. VTB 9049 output at 1:1.7 turns (series windings), 200 ohm source into 600 ohm load, DCR 12 primary and 40 secondary.
- ESTIMATES (not published by the manufacturer): magnetising inductance (4 H each, placeholder), post-saturation inductance, saturation flux, output standing bias current, leakage/capacitance high-frequency corner and Q, and the volts-per-full-scale calibration (13.8 V peak). Treat the sound as an approximation, not a measured unit.
- Limitations: no hysteresis; no manufacturer inductance, frequency-response or THD data was available. Overall small-signal gain remains unity; "Preamp Circuit" off keeps the static saturation curves.

## 1.6.3

Rebuilt the "N-Type" preamp as a circuit-behaviour model, the same
way "Brit" was rebuilt in 1.6.2. Signal path: input transformer, a
three-transistor direct-coupled feedback gain stage, a class-A line-driver
stage (degenerated transistor into a complementary emitter-follower pair),
and an output transformer. Each gain stage is a single-ended transistor
solved from the real exponential junction equation with emitter
degeneration, placed inside its feedback loop and solved every sample.
Resulting behaviour:

- Asymmetric distortion, rich in 2nd harmonic at moderate drive, with 3rd
  and higher orders growing as the stage is pushed.
- Distortion depends on the gain setting: less gain means more feedback and
  a cleaner stage; more gain means less feedback and more colour.
- The two stages clip against different positive and negative limits.
- Both transformer cores saturate. With "Preamp Circuit" on, the input
  transformer's low-frequency corner rises and the output transformer's
  high-frequency corner falls as the level increases.
- Coupling capacitors are modelled as DC blockers between stages.

This is a behavioural circuit model, not a SPICE netlist simulation, and
the transformers are saturating level-dependent filters, not full
magnetic-core (hysteresis) models. Transistor high-frequency poles are not
modelled. Parameter IDs are unchanged, so existing sessions load as before,
but "N-Type" will sound different from 1.6.1.

## 1.6.2

Rebuilt the "Brit" preamp as a circuit-behaviour model instead
of a generic saturation curve. It now follows the structure of a balanced,
transformerless console mic preamp: a matched pair of degenerated bipolar
transistors (one per leg) solved from the real exponential junction
equation, feeding low-noise op-amp stages that clip with a hard knee at
their supply rails. Resulting behaviour:

- Distortion depends on the gain setting: more gain means less emitter
  degeneration, so the input stage gets less linear; low gain stays very
  clean.
- The legs are subtracted, so even harmonics cancel and the colour is almost
  purely odd-order, with only a trace of even content from leg mismatch.
- Overdriving it gives a hard, console-style clip rather than a soft curve.
- With "Preamp Circuit" on, the sub-audio coupling and servo high-pass poles
  are modelled in their real positions in the signal chain, so DC from
  asymmetric clipping settles the way it does in hardware. The circuit's
  high-frequency poles sit far above the audio band and are not modelled.

This is a behavioural circuit model, not a SPICE netlist simulation. The
parameter names and IDs are unchanged, so existing sessions load as before,
but "Brit" will sound different (cleaner at low gain, harder-edged when
driven).

## 1.6.1

Reworked the "N-Type" preamp's circuit model to be closer to real
transformer-coupled hardware, after a two-transformer mic
amp topology (input transformer, discrete class-A gain stage,
output transformer). Previously "N-Type" used a single lumped
saturation curve plus a static, level-independent pair of filter poles; it
now explicitly models the input and output transformer stages separately,
and - when "Preamp Circuit" is on - a fast level follower makes both
stages' saturation grow more even-harmonic and the transformer poles shift
(input stage's LF corner rises, output stage's HF corner falls) as drive
increases, mimicking how a real transformer core saturates and narrows its
bandwidth
under heavier signal. This is a tuned approximation, not a component-level
SPICE model of the original schematics. See the comment above
`dsp::PreampEngine` in `Source/DSP/Preamp.h` for the full explanation.

## 1.6.0

Introduces the console interface: generated hardware knobs, aligned channel
strips, mechanical keys with dropdown choices, proportional resizing, and a
development-only visual inspector. Accepted designs are embedded from CSV.
The UI toolkit is included as source so fresh checkouts are self-contained.

The bundled [GoodLookinUI toolkit](third_party/GoodLookinUI) is independently [MIT licensed](third_party/GoodLookinUI/LICENSE). Its license file travels with the toolkit when reused in other projects.
