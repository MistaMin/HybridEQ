//! The SDK's conformance kit: rates 22.05 to 384 kHz, hostile block sizes and input, parameter fuzz,
//! state round trips, and no allocation on the audio path.
use openplugin::testing::{conformance, Options};

openplugin::testing::install_allocator!();

#[test]
fn hybrideq_conforms() {
    let options = Options {
        latency_depends_on_params: true,
        // HybridEQ's designed gain range, as in 1.6.x: preamp +34 dB (gain +24, pad +10) with the clean
        // Off model, three overlapping +18 dB EQ boosts and +24 dB output, about +112 dB. Instability is
        // caught by finite_output and the tail check, not by this bound.
        max_output: 4.5e5,
        ..Options::default()
    };
    conformance(hybrideq::HybridEq::default, options).assert_pass();
}
