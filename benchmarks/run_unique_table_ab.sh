#!/usr/bin/env bash
# Manual ordered/hash unique-table ABBA. Never invoked by CI or CTest.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
hash_dir="${HASH_BUILD:-$root/build-release-unique-hash}"
map_dir="${ORDERED_BUILD:-$root/build-release-unique-ordered}"
keys="${KEYS:-256,1024,4096}"
bits="${BITS:-8,10,12}"
concurrency="${CONCURRENCY:-0,1,2,4,8}"
samples="${SAMPLES:-7}"
profile_samples="${PROFILE_SAMPLES:-0}"
results="${RESULT_DIR:-$root/build-unique-ab-results/$(date -u +%Y%m%dT%H%M%SZ)}"
mkdir -p "$results"

{
  printf 'git_commit=%s\n' "$(git -C "$root" rev-parse HEAD)"
  printf 'git_dirty_files=%s\n' "$(git -C "$root" status --porcelain | wc -l | tr -d ' ')"
  printf 'keys=%s\nbits=%s\nconcurrency=%s\nsamples=%s\n' \
      "$keys" "$bits" "$concurrency" "$samples"
  printf 'profile_samples=%s\n' "$profile_samples"
  printf 'cmake=%s\n' "$(cmake --version | head -n 1)"
  printf 'host=%s\n' "$(uname -a)"
  printf 'CC=%s\nCXX=%s\n' "${CC:-default}" "${CXX:-default}"
} > "$results/manifest.txt"

for variant in hash ordered; do
  if [[ "$variant" == hash ]]; then
    build="$hash_dir"
    ordered=OFF
  else
    build="$map_dir"
    ordered=ON
  fi
  echo "Configuring $variant in $build"
  cmake -S "$root" -B "$build" \
      -DCMAKE_BUILD_TYPE=Release -DBUILD_PYTHON_BINDINGS=OFF \
      -DARIADNE_BDD_UNIQUE_TABLE_ORDERED="$ordered"
  if ! grep -q "^ARIADNE_BDD_UNIQUE_TABLE_ORDERED:BOOL=${ordered}$" \
      "$build/CMakeCache.txt"; then
    echo "Variant configuration did not stick: $build" >&2
    exit 1
  fi
  cmake --build "$build" --config Release --target \
      test_robdd benchmark_robdd_unique_table benchmark_robdd_parallel_ite
  (cd "$build" && ctest -C Release --output-on-failure -R '^test_robdd$')
done

binary() {
  local dir="$1" name="$2"
  if [[ -x "$dir/benchmarks/$name" ]]; then
    printf '%s\n' "$dir/benchmarks/$name"
  elif [[ -x "$dir/benchmarks/Release/$name" ]]; then
    printf '%s\n' "$dir/benchmarks/Release/$name"
  else
    echo "Missing built benchmark $name under $dir" >&2
    return 1
  fi
}

# Identical programs and inputs, separate complete builds, ABBA order.
n=0
for variant in hash ordered ordered hash; do
  n=$((n+1))
  if [[ "$variant" == hash ]]; then build="$hash_dir"; else build="$map_dir"; fi
  prefix="$(printf '%02d_%s' "$n" "$variant")"
  echo "Benchmark sequence $n/4: $variant"
  "$(binary "$build" benchmark_robdd_unique_table)" \
      --keys "$keys" --concurrency "$concurrency" --samples "$samples" \
      > "$results/${prefix}_unique.log" 2>&1
  "$(binary "$build" benchmark_robdd_parallel_ite)" \
      --bits "$bits" --concurrency "$concurrency" --samples "$samples" \
      --profile-samples "$profile_samples" \
      > "$results/${prefix}_ite.log" 2>&1
done
echo "Manual ABBA completed; manifest and eight logs: $results"
echo "Compare absolute serial medians across table variants before speedups."
