# HybridEQ 2.0.0 release, 2026-11-23

Owner decisions (2026-11-23): version 2.0.0; Windows (x64, ARM64) and Linux (x86_64, aarch64) built in
GitHub Actions (explicit exception to native machines); Windows as an unsigned Inno Setup installer; no
standalone; the three differences from 1.6.16 accepted and documented (manual, "What changed from 1.6");
the SDK VST3 bug (edits while the host bypasses the effect were not saved) fixed before release.

Source: HybridEQ f9e6307, OpenPluginSDK b01d07a for macOS (branch MistaMin/hybrideq-host-values, PR #37),
GoodLookin DRI 333a952 (branch MistaMin/semibold-text). macOS: this Mac, VST3 SDK v3.8.1, universal.
Windows and Linux: CI run 45202748784 at HybridEQ 09c7124 with OpenPluginSDK c5446e7 (PR #42, the X11 frame
pacing; see the finding below) on windows-latest, windows-11-arm, ubuntu-22.04 and ubuntu-22.04-arm,
VST3 SDK v3.8.0_build_66. The first Windows and Linux builds (run 45163752697, SDK b01d07a) were replaced.
OpenPluginSDK PRs #37 and #42 and GoodLookin DRI PR #1 are merged into their main branches.

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

## Finding: Linux editor stalled the host (fixed before release)

After merging main into rust-port (QA files only), CI run 45166466133 failed Linux x86_64 pluginval
"Editor Automation: Timeout after 30 secs", and its rerun failed the same way; the run before (45163752697,
same source) had passed. A diagnostic workflow (branch diag/linux-editor-hang, deleted after use) stalled
in 7 of 8 runs over seeds 0x727d986, 1, 2 and 3.

- Cause (backtraces in linux-editor-stall/before/*-bt.txt, four snapshots 5 s apart): the main thread was
  always in LinuxEditorView::onTimer -> EditorView::pump -> HybridEqEditor::render. The test changes every
  parameter every 10 ms, so every frame repaints everything: 300-650 ms on the Mac, worst 1.15 s on the
  runner. JUCE's timer thread re-posts its message every 300 ms while one is running
  (juce_Timer.cpp, callbackArrived.wait (300)), each drew another frame, and pluginval's callAsync that
  closes the editor never ran.
- Fix: OpenPluginSDK 6eed477 (PR #42): after a frame the X11 view stays idle as long as the frame took;
  pending damage is drawn at the next pump.
- After: 6 of 6 pluginval runs SUCCESS, 0 failed (seeds 0x727d986 x2, 0x6e07e7c, 1, 2, 3):
  linux-editor-stall/after/.
- Not fixed in 2.0.0: any parameter change repaints the whole editor (85 ms per frame at the defaults on
  the Mac, more with new knob angles or a section type change). Follow-up: damage only the changed control.

Not done: Windows and Linux in a DAW by hand; ARM pluginval; Windows code signing (owner: unsigned);
REAPER rerun on the final macOS build (the 1.7.0-day build passed; the final build passed the validators).
