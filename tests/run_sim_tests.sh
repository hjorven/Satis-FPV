#!/usr/bin/env bash
# Builds and runs the standalone flight simulation tests (no Unreal Engine required).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$ROOT/Mods/FPVDrone/Source/FPVDrone"
OUT="${TMPDIR:-/tmp}/fpvdrone_sim_tests"
"${CXX:-g++}" -std=c++17 -O2 -Wall -Wextra -Wshadow -Werror \
  -I"$SRC/Public" \
  "$ROOT/tests/sim_tests.cpp" "$SRC/Private/Sim/FPVRates.cpp" "$SRC/Private/Sim/FPVFlightSim.cpp" \
  -o "$OUT"
"$OUT"
