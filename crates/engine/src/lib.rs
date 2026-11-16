//! HybridEQ engine: the 1.6.x signal path in Rust, framework-free.
#![forbid(unsafe_code)]

pub mod adaa;
pub mod biquad;
pub mod circuits;
pub mod engine;
pub mod eq;
pub mod fit;
pub mod params;
pub mod preamp;
pub mod reduced;
pub mod solver;

pub use engine::{defaults, effective_factor, tail_seconds, Engine};
