#!/usr/bin/env bash
set -euo pipefail
cmake -S . -B build-fuzz -DAETHON_BUILD_TESTS=OFF -DAETHON_BUILD_BENCHMARKS=OFF -DAETHON_BUILD_FUZZERS=ON -DCMAKE_CXX_COMPILER="${CXX:-clang++}"
cmake --build build-fuzz --parallel 2
mkdir -p "$OUT"
cp build-fuzz/*_fuzzer "$OUT"/
cp fuzz/*.dict "$OUT"/ 2>/dev/null || true
if [ -d fuzz/corpus ]; then
  cp -R fuzz/corpus "$OUT"/
fi
