#!/bin/sh
# Full host-side verification: Unity tests + 30-day soak sim.
set -e
cd "$(dirname "$(readlink -f "$0")")"
echo "== Unity tests =="
pio test -e native 2>&1 | tail -11
echo "== Soak sim =="
OUT="${TMPDIR:-/tmp}/ha_soak"
g++ -std=c++17 -Wall -Wextra -Isrc/logic -Isrc/common \
  tools/soak_sim.cpp src/logic/relay.cpp src/logic/debounce.cpp \
  src/logic/logstore.cpp src/logic/auth.cpp src/common/sha256.cpp \
  -o "$OUT"
"$OUT"
