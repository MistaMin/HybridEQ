//! HybridEQ engine: the 1.6.x signal path in Rust, framework-free.
#![forbid(unsafe_code)]
// Fitted coefficients and constants are the 1.6.x values as written (0.7071, PI_CAL, 6.2831853):
// the exact constants would break the null against 1.6.16.
#![allow(clippy::approx_constant, clippy::excessive_precision)]
// `!(x > 0.0)` is deliberate: it also rejects NaN.
#![allow(clippy::neg_cmp_op_on_partial_ord)]
// Index loops over several arrays in step mirror the 1.6.x C++ they are checked against.
#![allow(clippy::needless_range_loop, clippy::too_many_arguments)]

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
