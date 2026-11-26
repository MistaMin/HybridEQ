//! TS-2 and TS-3. Ignored in a plain `cargo test`; the commit tester runs them with `--ignored`.
//! OPENPLUGIN_SOAK_SEEDS sets the number of seeds (24 per commit, 200 for a release).
use hybrideq::HybridEq;
use openplugin::testing::{conformance, Options};

openplugin::testing::install_allocator!();

fn seeds() -> u64 {
    std::env::var("OPENPLUGIN_SOAK_SEEDS").ok().and_then(|s| s.parse().ok()).unwrap_or(24)
}

/// The same latency and output bound as tests/conformance.rs (the +112 dB designed gain range).
fn base() -> Options {
    Options { latency_depends_on_params: true, max_output: 4.5e5, ..Options::default() }
}

#[test]
#[ignore]
fn soak_many_seeds() {
    for seed in 1..=seeds() {
        let report = conformance(HybridEq::default, Options { fuzz_sessions: 12, ..base() }.with_seed(seed));
        assert!(report.passed(), "seed {seed}:\n{report}");
    }
}

#[test]
#[ignore]
fn fuzz_strict() {
    let options = Options { fuzz_sessions: 32, strict_hostile_output: true, strict_denormals: true, ..base() };
    conformance(HybridEq::default, options).assert_pass();
}
