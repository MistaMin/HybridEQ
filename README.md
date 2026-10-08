# HybridEQ

HybridEQ is a channel-strip plug-in combining a multi-flavor analog preamp
emulation with a 6-band EQ, harmonic saturation, and mid/side processing.
Each stage can be switched between the N-Type, Brit, A-Type, Baxandall and
FSF circuit models. Includes adaptive oversampling
(1x/4x/8x) that automatically raises the internal processing rate for
headroom when the Circuit model is engaged (2x minimum; 4x and 8x stay available). Built with
[JUCE](https://juce.com); ships as VST3, AU, AAX, and Standalone for macOS.

Current version: see [`VERSION`](VERSION).

## Building

Requires CMake 3.22+ and a C++20 compiler. GoodLookinUI is a git submodule
in `third_party/GoodLookinUI` (pinned to v0.2.2): after cloning run
`git submodule update --init`, or set `GOODLOOKINUI_ROOT` to use a separate
checkout instead. AAX is added only when Avid's SDK exists at
`../aax-sdk-2-8-1`, or at the path specified by `HYBRIDEQ_AAX_SDK`. JUCE itself is fetched automatically via CMake's
`FetchContent` — no manual setup needed for VST3/AU/Standalone.

```bash
cmake -B build -G Xcode
cmake --build build --config Release --target HybridEQ_VST3
```

Swap `HybridEQ_VST3` for `HybridEQ_AU`, `HybridEQ_AAX`, `HybridEQ_LV2`,
`HybridEQ_CLAP` or `HybridEQ_Standalone` for the other formats. LV2 and CLAP
(the open formats for Linux) are on by default; turn them off with
`-DHYBRIDEQ_LV2=OFF` / `-DHYBRIDEQ_CLAP=OFF` (CLAP fetches clap-juce-extensions
at configure time). Linux needs the usual JUCE dependencies (ALSA/JACK, X11,
freetype, fontconfig, OpenGL); the `Linux build` GitHub Actions workflow shows
the exact package list and builds them.

## License

Official compiled HybridEQ plug-ins, standalone applications, and installers
are **proprietary freeware**, free to use for personal and commercial audio
production. They are distributed under the [binary freeware license](BINARY_LICENSE.txt),
not an open-source license. The publisher builds them under a JUCE commercial
license; end users do not need a JUCE developer license to use these downloads.

HybridEQ's own source code is separately [MIT licensed](LICENSE). The bundled
GoodLookinUI toolkit, including its core and JUCE adapter, has its own
[MIT license](third_party/GoodLookinUI/LICENSE). Those component permissions
remain intact when included in the freeware binaries, and their copyright and
permission notices accompany binary distributions.

JUCE and optional Avid SDK code retain their own licenses. Anyone building or
distributing their own version must comply with the applicable dependency
licenses; the publisher's JUCE commercial license is not transferred with the
source. See [component notices](THIRD_PARTY_NOTICES.txt) and the
[JUCE 9 terms](https://juce.com/legal/juce-9-licence/). Choosing JUCE's
open-source licensing route requires satisfying its applicable terms.
AAX additionally requires Avid's SDK/developer permissions and applicable
signing requirements; it is excluded from public packaging by default.

The license texts of every bundled third-party component (JUCE-bundled libraries and the VST3, LV2,
CLAP and Audio Unit SDKs) are in [`Licenses/third-party`](Licenses/third-party). Binary bundles
(VST3, AU, CLAP, LV2, standalone) carry them in their Licenses folders. The macOS installer also displays the
binary freeware terms and installs the notices in
`/Library/Application Support/OpenGrid/HybridEQ/Licenses`.

## GoodLookinUI development editor

Debug builds include a **Design** inspector for generated knobs. Edit style,
label, hex colour, font size and panel-relative geometry; Apply, Undo, Save CSV
and Load CSV are available. Save the accepted design to `Designs/HybridEQ.csv`
and rebuild to embed it. Release builds exclude the editor entirely. The
finished panel scales proportionally with the window.

## Console appearance

The console edition uses GoodLookinUI's generated fluted black grips and coloured
knob caps, fixed calibration marks, shaded mechanical keys, recessed numeric
readouts and a charcoal rack faceplate. Choice keys open dropdown menus using
the existing DSP choice values. Six strips share fixed control slots and switch
positions. Resizing preserves the panel proportions. The existing DSP and
parameter IDs are retained.

## Circuit history

Each flavour is inspired by a studio classic:

- **N-Type** is inspired by Rupert Neve's 1073 preamp module, which first appeared in a console built in 1970 and became one of the most celebrated mic preamps ever made ([history](https://www.ams-neve.com/consoles/history-of-1073/)).
- **FSF** is inspired by Focusrite's console channel, which began with a 1985 commission from Sir George Martin and grew into one of the most influential names in recorded sound ([story](https://eu.focusrite.com/our-story)).
- **Brit** is inspired by Solid State Logic, founded in 1969 by Colin Sanders, whose 1979 SL 4000 E Series ranks among the most groundbreaking mixing consoles ever built ([history](https://solidstatelogic.com/our-history)).
- **A-Type** is inspired by API's 512 mic preamp, one of the most iconic 500 Series modules, from a company founded in 1969 around the 2520 discrete op-amp ([about](https://apiaudio.com/about/)).

**Trademark notice.** Neve, 1073, Focusrite, Solid State Logic (SSL), API and 512 are trademarks of their respective owners. HybridEQ is an independent project: those companies are not affiliated with, and have not endorsed or sponsored, Marcos Deida, the OpenGrid Project or this plug-in. The names appear only to describe the classic designs that inspired each model; every model here is an independent circuit simulation, and no code or text from those companies is included.

## Version 1.6.10

- **ADAA on the preamp with Circuit off.** The static N-Type, Brit, A-Type and FSF models now use first-order
  antiderivative anti-aliasing (`Source/DSP/Adaa.h`) on every nonlinear stage, so the harmonics they generate fold back
  less, even at 1x (measured 5 to 14 dB lower aliasing on hard-driven tones, 4.4 and 10 kHz). It works on the
  nonlinear part only: quiet signals keep their level at every frequency (the linear path is a unity-magnitude
  all-pass, about half a sample of delay, no reported latency). ADAA holds off while the Drive control is moving.
  N-Type's feedback-loop stages are not ADAA'd (their antiderivative proved too noisy); its limiters and cores are.
- **Circuit needs only 2x.** Turning Circuit on now raises Oversample to a minimum of 2x (it was 4x). The choice list
  is still 1x/4x/8x; 4x and 8x remain available.
- Tests: `tests/adaa.cpp` (antiderivatives against their functions, transparency for linear stages, alias reduction).

## Version 1.6.9

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

## Version 1.6.8

- Built with **JUCE 9.0.3** (JUCE 9 End User Licence Agreement; was JUCE 8.0.15). The bundled-library license
  texts are regenerated for JUCE 9 (adds Opus, libwebp and LunaSVG).

## Version 1.6.7

- Built with **JUCE 8.0.15** (was 8.0.4). Its bundled VST3 SDK is MIT licensed, so VST3 builds no longer depend on
  the older dual-license (Steinberg license / GPLv3) SDK. Third-party license texts in `Licenses/third-party` are
  regenerated from the new JUCE tree (`tools/collect_licenses.py`).

## Version 1.6.6

- **LV2 and CLAP** are built alongside VST3, AU and the standalone, and are installed by the macOS installer
  (`/Library/Audio/Plug-Ins/CLAP` and `/Library/Audio/Plug-Ins/LV2`). A GitHub Actions workflow builds the Linux
  versions (VST3, LV2, CLAP, standalone).
- **Licenses:** the license texts of all bundled third-party components are in `Licenses/third-party` and ship
  with the source, the installer, the disk image and inside every plug-in bundle.

## Version 1.6.5

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

## Version 1.6.4 (superseded by 1.6.5)

- N preamp: input and output transformers are now flux-state circuit models instead of memoryless saturators plus an envelope-driven filter (with "Preamp Circuit" on). Low-frequency corner shift and low-frequency harmonics now come from the core's inductance falling as it saturates; the output transformer carries a standing DC bias, giving even-order harmonics.
- Datasheet values (transformer design guide, preliminary, Issue 1e): VTB 9046 input at 2:1 turns, 2k4 source into 2k4 load (0 dB), DCR 175 || 175 primary and 56 + 56 secondary. VTB 9049 output at 1:1.7 turns (series windings), 200 ohm source into 600 ohm load, DCR 12 primary and 40 secondary.
- ESTIMATES (not published by the manufacturer): magnetising inductance (4 H each, placeholder), post-saturation inductance, saturation flux, output standing bias current, leakage/capacitance high-frequency corner and Q, and the volts-per-full-scale calibration (13.8 V peak). Treat the sound as an approximation, not a measured unit.
- Limitations: no hysteresis; no manufacturer inductance, frequency-response or THD data was available. Overall small-signal gain remains unity; "Preamp Circuit" off keeps the static saturation curves.

## Version 1.6.3

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

## Version 1.6.2

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

## Version 1.6.1

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

## Version 1.6.0

Introduces the console interface: generated hardware knobs, aligned channel
strips, mechanical keys with dropdown choices, proportional resizing, and a
development-only visual inspector. Accepted designs are embedded from CSV.
The UI toolkit is included as source so fresh checkouts are self-contained.

The bundled [GoodLookinUI toolkit](third_party/GoodLookinUI) is independently [MIT licensed](third_party/GoodLookinUI/LICENSE). Its license file travels with the toolkit when reused in other projects.
