# HybridEQ on OpenPluginSDK main, 2026-11-26

Change under test (uncommitted on rust-port at e431ff5): OpenPluginSDK c5446e7 -> 19fac89 (main; GoodLookin DRI
now in the SDK tree, shell ABI 10), CI `SDK_REF` moved and the separate GoodLookin DRI checkout dropped; four
audio-path `unwrap`/`expect` in the engine replaced by fallbacks (RT-3); clippy fixes in plugin/; new
`tests/soak.rs`, `examples/bench.rs`, `bench/baseline.json`.

Build: macOS arm64, release, `build_plugin.sh` with `--editor --tail --aax-pages`, all five formats, VST3 SDK
from clap-wrapper's cpm checkout, AudioUnitSDK, AAX SDK 2.8.1. Bundles unsigned (ad hoc), not installed.

## Scope

| # | Item | Evidence | Status |
|---|---|---|---|
| 1 | Builds on SDK main, every format | cargo build; build_plugin.sh exit 0, codesign and arch checks | verified |
| 2 | Audio unchanged by the SDK move and the code edits | null/: 17 renders at e431ff5 + c5446e7 and at the working tree + 19fac89 are bit-identical (every preamp type, full and reduced models, HQ, 2/4/8x with both filters, M/S EQ, harmonics, Autogain, Delta, mono, block 333); all non-silent, no NaN, 17 distinct outputs | verified |
| 3 | Editor unchanged | preview renders 1x defaults, 2x, overlay open, 0.75 window: pixel-identical; with audio: identical once both use the same test signal (the preview's tone constant 6.283 -> TAU is the only difference); gui/editor-2x-audio.png checked by eye | verified |
| 4 | Engine fallbacks only act when unprepared | code review; item 2 | verified |
| 5 | Soak, strict fuzz, bench exist and pass | soak.txt (24 seeds, 865 s); fuzz_strict 65 s; bench within 1 % of the baseline (median gate) | verified |
| 6 | CI pinned to the new SDK | .github/workflows/windows-linux.yml | unverified: needs a push to run |

## Checks

| Check | Result | Evidence |
|---|---|---|
| Full tier: rt-lint, clippy -D warnings, cargo test --release | PASS | |
| T-SOAK 24 seeds / T-FUZZ strict / T-BENCH | PASS | soak.txt, bench/baseline.json |
| clap-validator | PASS: 34 passed, 0 failed, 10 skipped (as 2.0.0) | validators/clap-validator.txt |
| VST3 SDK validator | PASS: 47/47 | validators/vst3-validator.txt |
| pluginval strictness 10, VST3, in process | PASS (GUI tests skipped, as 2.0.0 on macOS) | validators/pluginval-vst3.txt |
| AAX Validator, the six tests 2.0.0 ran | PASS: describe_validation, parameters, parameter_traversal.random.fast, page_table.load, page_table.automation_list, load_unload | aax/summary-new.txt |
| AAX Validator, tests 2.0.0 did not run | PASS: parameter_traversal.linear, parameter_traversal.random, info.support.audiosuite, info.support.s6_feature. SKIPPED: cycle_counts (needs HDX) | aax/summary-new.txt |
| AAX info.productids, info.support.general, test.data_model | INCONCLUSIVE: aborted by the validator's own harness before the plugin loads (SysInfo.rb:249, a volume with no free-space field in its disk survey); the shipped 2.0.0 AAX aborts identically | aax/sysinfo-crash.txt, aax/summary-2.0.0-aborted-tests.txt |
| auval, REAPER host suite, OpenGrid9 | SKIPPED: 2.0.0 is installed in /Library (VST3, AU) and would shadow a test build; moving it needs the owner and admin rights | |
| Windows, Linux | SKIPPED: CI runs on push | |
| Pro Tools, listening | SKIPPED: owner | |

Exceptions: NEW, crate-level clippy allows in crates/engine/src/lib.rs, each with its reason there:
`approx_constant` and `excessive_precision` (the 1.6.x constants and fitted coefficients as written; exact
constants would break the null against 1.6.16), `neg_cmp_op_on_partial_ord` (`!(x > 0.0)` also rejects NaN),
`needless_range_loop` and `too_many_arguments` (loops over several arrays in step mirror the 1.6.x C++).
No code generation changes. Validator notes: the AAX Validator cannot run from a path with a space (dsh passes its
own path unquoted; a symlink does not help, dsh resolves it).
