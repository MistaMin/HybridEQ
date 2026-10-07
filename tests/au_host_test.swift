// Loads the installed AU like a host does (Apple's audio engine) and renders offline.
// Build: swiftc -O tests/au_host_test.swift -o /tmp/au_host_test     Run: /tmp/au_host_test
import AVFoundation
import AudioToolbox

func fourCC(_ s: String) -> FourCharCode { var r: FourCharCode = 0; for u in s.utf8 { r = (r << 8) | FourCharCode(u) }; return r }
var failures = 0, checks = 0
func check(_ ok: Bool, _ what: String) { checks += 1; if !ok { failures += 1; print("FAIL \(what)") } }

let desc = AudioComponentDescription(componentType: kAudioUnitType_Effect, componentSubType: fourCC("HyEq"), componentManufacturer: fourCC("Hybr"), componentFlags: 0, componentFlagsMask: 0)

// Values a host would send: toggles and choices are whole numbers, everything else anywhere in range.
func hostValue(_ p: AUParameter) -> Float {
    let r = Float.random(in: 0...1)
    if p.unit == .boolean || p.unit == .indexed { return p.minValue + (p.maxValue - p.minValue) * (r > 0.5 ? 1 : 0) == p.minValue ? p.minValue : (p.unit == .boolean ? p.maxValue : (p.minValue + (p.maxValue - p.minValue) * r).rounded()) }
    return p.minValue + (p.maxValue - p.minValue) * r
}

func instantiate() -> AVAudioUnit? {
    var result: AVAudioUnit?; let sem = DispatchSemaphore(value: 0)
    AVAudioUnit.instantiate(with: desc, options: []) { au, error in result = au; if let e = error { print("instantiate error: \(e)") }; sem.signal() }
    sem.wait(); return result
}

func render(rate: Double, frames: AVAudioFrameCount, automate: Bool, label: String) {
    guard let unit = instantiate() else { check(false, "\(label): AU instantiates"); return }
    let engine = AVAudioEngine(); let player = AVAudioPlayerNode()
    guard let format = AVAudioFormat(commonFormat: .pcmFormatFloat32, sampleRate: rate, channels: 2, interleaved: false) else { return }
    do {
        try engine.enableManualRenderingMode(.offline, format: format, maximumFrameCount: 4096)
        engine.attach(player); engine.attach(unit)
        engine.connect(player, to: unit, format: format); engine.connect(unit, to: engine.mainMixerNode, format: format)
        try engine.start()
    } catch { check(false, "\(label): engine starts (\(error))"); return }
    // 2 seconds of noise + 1 kHz sine, looped
    let n = AVAudioFrameCount(rate * 2)
    let src = AVAudioPCMBuffer(pcmFormat: format, frameCapacity: n)!; src.frameLength = n
    for c in 0..<2 { let p = src.floatChannelData![c]; for i in 0..<Int(n) { let t = Double(i) / rate
        p[i] = Float(0.2 * sin(2 * Double.pi * 1000 * t)) + 0.1 * Float.random(in: -1...1) } }
    player.scheduleBuffer(src, at: nil, options: .loops, completionHandler: nil); player.play()
    let out = AVAudioPCMBuffer(pcmFormat: engine.manualRenderingFormat, frameCapacity: engine.manualRenderingMaximumFrameCount)!
    var rendered: AVAudioFrameCount = 0, finite = true, peak: Float = 0, energy: Double = 0, block = 0
    let params = unit.auAudioUnit.parameterTree?.allParameters ?? []
    while rendered < frames {
        if automate && block % 3 == 0 { for p in params.shuffled().prefix(3) { p.value = hostValue(p) } }
        let want = min(engine.manualRenderingMaximumFrameCount, frames - rendered)
        guard let status = try? engine.renderOffline(want, to: out) else { check(false, "\(label): render call"); break }
        if status == .success {
            for c in 0..<2 { let p = out.floatChannelData![c]; for i in 0..<Int(out.frameLength) { let v = p[i]; if !v.isFinite { finite = false }; peak = max(peak, abs(v)); energy += Double(v * v) } }
            rendered += out.frameLength
        } else if status == .error { check(false, "\(label): render status error"); break }
        block += 1
    }
    check(rendered == frames, "\(label): rendered all \(frames) frames (\(rendered))")
    check(finite, "\(label): every output sample is finite")
    check(peak < 1000, "\(label): output bounded (peak \(peak))")
    check(energy > 1e-3, "\(label): signal comes out (energy \(energy))")
    print("  \(label): latency \(Int(unit.auAudioUnit.latency * rate)) samples, tail \(unit.auAudioUnit.tailTime) s, \(params.count) parameters, peak \(String(format: "%.3f", peak))")
    engine.stop()
}

print("AU host test (HybridEQ, aufx HyEq Hybr)")
guard instantiate() != nil else { print("FAIL: HybridEQ AU is not installed / cannot be instantiated"); exit(2) }
for rate in [44100.0, 48000.0, 96000.0] { render(rate: rate, frames: AVAudioFrameCount(rate * 3), automate: false, label: "\(Int(rate)) Hz default settings") }
for rate in [48000.0, 96000.0] { render(rate: rate, frames: AVAudioFrameCount(rate * 3), automate: true, label: "\(Int(rate)) Hz with automation") }

// state round trip through the host's fullState, like saving and reloading a session
if let a = instantiate(), let b = instantiate() {
    let pa = a.auAudioUnit.parameterTree!.allParameters
    for p in pa { p.value = hostValue(p) }
    let state = a.auAudioUnit.fullState; check(state != nil, "fullState can be read")
    b.auAudioUnit.fullState = state
    var same = true; let pb = b.auAudioUnit.parameterTree!.allParameters
    for p in pa {
        guard let q = pb.first(where: { $0.address == p.address }) else { continue }
        let span: Float = abs(p.maxValue - p.minValue)
        let tolerance: Float = 0.001 * max(Float(1), span)
        let delta: Float = abs(q.value - p.value)
        if delta > tolerance { same = false; print("  differs: \(p.displayName) \(p.value) -> \(q.value)") }
    }
    check(same, "fullState round trip restores every parameter")
}
print("\(checks) checks, \(failures) failed"); exit(failures == 0 ? 0 : 1)
