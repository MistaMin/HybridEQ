# HybridEQ 2.0.0 release, 2026-11-23

Owner decisions (2026-11-23): version 2.0.0; Windows (x64, ARM64) and Linux (x86_64, aarch64) built in
GitHub Actions (explicit exception to native machines); Windows as an unsigned Inno Setup installer; no
standalone; the three differences from 1.6.16 accepted and documented (manual, "What changed from 1.6");
the SDK VST3 bug (edits while the host bypasses the effect were not saved) fixed before release.

Source: HybridEQ f9e6307, OpenPluginSDK b01d07a (branch MistaMin/hybrideq-host-values, PR #37),
GoodLookin DRI 333a952 (branch MistaMin/semibold-text). macOS: this Mac, VST3 SDK v3.8.1, universal.
Windows and Linux: CI run 45163752697 (windows-latest, windows-11-arm, ubuntu-22.04, ubuntu-22.04-arm),
VST3 SDK v3.8.0_build_66.

| Check | Result | Evidence |
|---|---|---|
| Engine and plugin tests | pass on all five builds | ci-*/tests-*.txt; local run at f9e6307 |
| VST3 SDK validator | 47/47 on all five | validators/vst3-validator.txt, ci-*/vst3-validator.txt |
| clap-validator 0.4.1 | 0 failed on all five (33-34 pass, 10-11 skipped: no presets, no note ports, no 64-bit; Windows also skips process-audio-denormals) | validators/clap-validator.txt, ci-*/clap-validator.txt |
| pluginval strictness 10, VST3 | macOS pass without GUI tests; Windows x64 and Linux x86_64 pass with the Editor and "open editor whilst processing" tests (Linux under Xvfb, through the host's run loop); not run on ARM (no build) | validators/pluginval-vst3.txt, ci-*/pluginval-vst3.txt |
| auval | AU VALIDATION SUCCEEDED, 2.0.0 (0x20000), from the user folder, then removed | validators/auval.txt |
| AAX Validator, PACE-signed AAX | 6/6 pass; warning: algorithm context gaps (as 1.6.16) | aax/ |
| macOS signing | Developer ID with hardened runtime, timestamp; universal, minos 11.0 both slices; AAX PACE-signed (signing-only wrap, __Pace_Eden as 1.6.16); pkg productsigned, notarized (bab46f90-bb89-4510-a1b3-27b618479254), stapled, spctl: Notarized Developer ID | |
| Linux binaries | glibc 2.35; NEEDED only glibc, libgcc_s and the loader; the VST3 exports only its three entry points | ci-linux-*/build.txt |
| Windows binaries | no Visual C++ runtime imports; version resources; installers 3.4 and 3.6 MB | ci-windows-*/build.txt |

Not done: Windows and Linux in a DAW by hand; ARM pluginval; Windows code signing (owner: unsigned);
REAPER rerun on the final macOS build (the 1.7.0-day build passed; the final build passed the validators).
