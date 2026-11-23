#!/bin/sh
# Null test of the Rust engine against the 1.6.16 C++ processor.
#   tools/null_ref.sh [out-dir]
# Needs build-ref/HybridEQRender_artefacts/Release/HybridEQRender (cmake --build build-ref --target HybridEQRender).
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${1:-$ROOT/dsp/evidence/null}"
REF="${HYBRIDEQ_REF:-$ROOT/build-ref/HybridEQRender_artefacts/Release/HybridEQRender}"
mkdir -p "$OUT"
cargo build -q --release --example render --manifest-path "$ROOT/crates/engine/Cargo.toml"
RS="$ROOT/target/release/examples/render"
SR=48000
for k in chirp noise mix burst impulse; do "$REF" gen $k 3 $SR "$OUT/$k.f32"; done

# name | mode | input | block | parameters
cat > "$OUT/scenarios.txt" <<'EOF'
default|run|mix|512|
flat-off|run|chirp|512|preampType=0
eq-all|run|noise|256|lowGain=6 lowFreq=80 mid1Gain=-9 mid1Freq=600 mid1Q=2.5 mid2Gain=7 mid2Freq=3500 mid2Q=0.6 highGain=-5 highFreq=16000 lcFreq=60 lcSlope=3 hcFreq=15000 hcSlope=3 hcQ=1.2 preampType=0
eq-types|run|noise|512|lowType=1 lowGain=-8 mid1Type=1 mid1Gain=9 mid2Type=2 mid2Gain=-6 highType=2 highGain=10 highFreq=20000 preampType=0
eq-ms|run|noise|333|lcMode=1 lcFreq=150 lowMode=2 lowGain=6 mid1Mode=1 mid1Gain=8 mid2Mode=2 mid2Gain=-8 highMode=1 highGain=4 hcMode=2 hcFreq=8000 preampType=0
bypass|run|noise|512|lcBypass=1 hcBypass=1 lowBypass=1 mid1Gain=12 mid1Bypass=1 preampBypass=1
brit-hot|run|mix|512|preampType=1 preampGain=18
ntype-hot|run|mix|512|preampType=2 preampGain=20 preampPad=2
fsf-hot|run|burst|512|preampType=3 preampGain=15
atype-hot|run|mix|512|preampType=4 preampGain=24
off-pad|run|chirp|512|preampType=0 preampPad=0 outputGain=-6
harmonics|run|mix|512|harmonic2nd=0.4 harmonic3rd=0.3 harmonic4th=0.2 harmonic5th=0.5
reduced-brit|run|mix|512|preampCircuit=1 preampHQ=0 preampType=1 preampGain=12
reduced-ntype|run|mix|512|preampCircuit=1 preampHQ=0 preampType=2 preampGain=18
reduced-fsf|run|burst|512|preampCircuit=1 preampHQ=0 preampType=3 preampGain=12
reduced-atype|run|mix|512|preampCircuit=1 preampHQ=0 preampType=4 preampGain=20
circuit-off-type|run|mix|512|preampCircuit=1 preampHQ=0 preampType=0
hq-brit|core|mix|1024|preampCircuit=1 preampType=1 preampGain=12
hq-ntype|core|mix|1024|preampCircuit=1 preampType=2 preampGain=12
hq-fsf|core|mix|1024|preampCircuit=1 preampType=3 preampGain=12
hq-atype|core|mix|1024|preampCircuit=1 preampType=4 preampGain=12
hq-off|core|chirp|1024|preampCircuit=1 preampType=0
EOF

while IFS='|' read -r name mode input block params; do
  # shellcheck disable=SC2086
  "$REF" $mode "$OUT/$input.f32" "$OUT/$name.cpp.f32" $SR "$block" $params
  "$RS" $mode "$OUT/$input.f32" "$OUT/$name.rs.f32" $SR "$block" "$OUT/$name.cpp.f32.params"
  printf '%-18s %s\n' "$name" "$("$RS" compare "$OUT/$name.cpp.f32" "$OUT/$name.rs.f32")"
done < "$OUT/scenarios.txt"
