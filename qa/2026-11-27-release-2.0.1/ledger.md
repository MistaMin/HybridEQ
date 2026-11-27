# HybridEQ 2.0.1 release, 2026-11-27

Owner decisions (2026-11-27): version 2.0.1; move the installed 2.0.0 aside for the host tests and put it
back; tag and publish. Standing decisions from 2.0.0 apply: Windows and Linux built in GitHub Actions,
Windows installers unsigned, no standalone.

Source: HybridEQ 02dddcd (version commit; later commits change only documents and QA evidence), OpenPluginSDK
19fac89 (main, GoodLookin DRI in its tree). macOS: this Mac, universal, VST3 SDK v3.8.1 (Water Channel's
checkout), AudioUnitSDK, AAX SDK 2.8.1. Windows and Linux: CI run 45914878619 at 02dddcd (windows-latest,
windows-11-arm, ubuntu-22.04, ubuntu-22.04-arm), VST3 SDK v3.8.0_build_66.

Audio against 2.0.0: bit-identical on 17 scenarios (qa/2026-11-26-sdk-main/ledger.md); the version string is
the only source change since.

| Check | Result | Evidence |
|---|---|---|
| Full tier on 02dddcd: rt-lint, clippy, tests | PASS (commit tester) | |
| Soak, 200 seeds (RL-1), on 02dddcd with SDK 19fac89 | PASS (soak_many_seeds ok, 11263 s on a busy machine) | soak.txt |
| Identity: VST3 class ids, AU aufx/HyEq/Hybr, CLAP id, AAX bundle id, LV2 URI and port symbols, parameter ids | equal to 2.0.0 | |
| macOS bundles | universal, minos 11.0 both slices, Developer ID with timestamp and hardened runtime (AAX: Developer ID, PACE signing-only wrap, no runtime flag, as 2.0.0), no build path in any binary | dist-release/macos-2.0.1-build.log |
| macOS installer | productsigned, notarized (e75161a7-660d-44da-ae77-ec22da9148b1), stapled, spctl: Notarized Developer ID | |
| clap-validator | 34 pass, 0 fail, 10 skipped | validators/clap-validator.txt |
| VST3 SDK validator | 47/47 | validators/vst3-validator.txt |
| pluginval strictness 10, VST3 | SUCCESS (GUI tests skipped, as 2.0.0 on macOS) | validators/pluginval-vst3.txt |
| auval | AU VALIDATION SUCCEEDED, 2.0.1 (0x20001), user folder with 2.0.0 moved out of /Library | validators/auval.txt |
| AAX Validator, PACE-signed AAX | PASS: describe_validation, parameters, parameter_traversal (linear, random, random.fast), page_table.load, page_table.automation_list, load_unload, info.support.audiosuite, info.support.s6_feature. Aborted before the plugin loads: info.productids, info.support.general, test.data_model (the validator's disk survey, SysInfo.rb:249; 2.0.0 aborts identically). Skipped: cycle_counts (HDX) | aax/summary.txt, ../2026-11-26-sdk-main/aax/ |
| REAPER suite, VST3 / AU / CLAP, each in a fresh hidden instance | PASS on all three: loads, finite, not silent, deterministic (-240 dB), editor opens, every parameter changes the sound (band bypasses CONDITIONAL: they act once the band has gain), every parameter shown, state survives save and reopen, editor reopens 5 times. lsof: the 2.0.1 binaries in ~/Library | reaper-vst3/, reaper-au/, reaper-clap/ |
| Windows x64 / ARM64, Linux x86_64 / aarch64 (CI) | tests 24 pass on each; VST3 validator 47/47 on each; clap-validator 0 fail (33-34 pass); pluginval SUCCESS on Windows x64 and Linux x86_64 (with the editor; Linux under Xvfb); not available on ARM | dist-release/ci-2.0.1/*/qa (not committed) |

## Finding: one VST3 render pair differed (not reproduced)

The first REAPER run (reaper-run1-shared-instance/) tested the three formats in one REAPER instance. That run is
invalid for AU and CLAP: the bridge adds each plug-in on a new track, so the AU renders were VST3 + AU and the
CLAP renders all three, and REAPER wrote the repeat renders as base-001/-002 while the suite compared the earlier
VST3 files. The AU and CLAP repeat pairs themselves were bit-identical.

Its VST3 pair did differ: against a clean render, base.wav deviates from block 100 to 139 (1.07-1.49 s) and
base2.wav from block 106 to 137, then both match again exactly. The machine was heavily loaded at the time
(this run's soak, the AAX Validator, other sessions' builds; load average above 100). It did not come back in
43 later renders in three fresh instances (probe-vst3-201*, one of them with eight extra busy processes) nor in
the full rerun (reaper-vst3/, -240 dB).

Hypothesis, not proven: a setProcessing(false)/(true) pair that REAPER makes on its main thread lands between
two blocks of a running render, so the SDK resets the engine before the next block (OpenPluginVST3.cpp,
setProcessing and process); the filters then settle back onto the input over about 0.4 s, which is the shape
seen. 2.0.0 reset in that case too (on the main thread, with a dry block), so this is not a regression. Follow-up
for the SDK: reset only for a stream that really restarts (for example, the host's continuous-time position),
and log setProcessing calls in a debug build to confirm the cause.

## Not done

Pro Tools (AAX in a host), Logic, Windows and Linux in a DAW by hand, listening tests.
