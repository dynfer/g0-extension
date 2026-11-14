#!/usr/bin/env bash
set -euo pipefail
source_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
test_build_dir=${1:-${TMPDIR:-/tmp}/g0-fome-host-tests}
cmake -S "$source_dir/modules/tests" -B "$test_build_dir" -DG0_SANITIZE=ON
cmake --build "$test_build_dir" -j12
ASAN_OPTIONS=${ASAN_OPTIONS:-detect_leaks=0} ctest --test-dir "$test_build_dir" --output-on-failure
cmake -S "$source_dir/../tests" -B "$test_build_dir/slave" -DG0_SANITIZE=ON
cmake --build "$test_build_dir/slave" -j12
ASAN_OPTIONS=${ASAN_OPTIONS:-detect_leaks=0} ctest --test-dir "$test_build_dir/slave" --output-on-failure
