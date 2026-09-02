#!/usr/bin/env bash

set -euo pipefail

runner=$1
cmake=$2
ctest=$3
source_dir=$4
binary_dir=$5

source_dir=$(realpath "$source_dir")
binary_dir=$(realpath -m "$binary_dir")

rm -rf "$binary_dir"
"$cmake" -S "$source_dir" -B "$binary_dir"
output=$(python3 "$runner" \
  --test-dir "$binary_dir" \
  --manifest "$source_dir/manifest.txt" \
  --include-regex '^rex_regex_only$' \
  --ctest "$ctest" \
  --jobs 1 \
  --dry-run)

expected="Selected 5 CTest tests"
if [ "$output" != "$expected" ]; then
  echo "unexpected manifest/regex selection result: $output" >&2
  exit 1
fi

"$cmake" -E echo rex_missing >"$binary_dir/missing-manifest.txt"
if python3 "$runner" \
  --test-dir "$binary_dir" \
  --manifest "$binary_dir/missing-manifest.txt" \
  --ctest "$ctest" \
  --jobs 1 \
  --dry-run >"$binary_dir/missing-manifest.log" 2>&1; then
  echo "missing manifest test was accepted" >&2
  exit 1
fi
grep -Fq 'Manifest tests are not registered:' "$binary_dir/missing-manifest.log"

output=$(python3 "$runner" \
  --test-dir "$binary_dir" \
  --include-regex '^rex_memcheck_(ignored|selected)$' \
  --shard-index 1 \
  --shard-count 1 \
  --ctest "$ctest" \
  --jobs 1 \
  --memcheck \
  --dry-run)

expected="Selected 1 CTest tests"
if [ "$output" != "$expected" ]; then
  echo "unexpected MemCheck-registry selection result: $output" >&2
  exit 1
fi

output=$(python3 "$runner" \
  --test-dir "$binary_dir" \
  --include-regex '^rex_dependency_(producer|consumer)$' \
  --shard-index 2 \
  --shard-count 3 \
  --ctest "$ctest" \
  --jobs 1 \
  --dry-run)

expected="Selected 2 CTest tests"
if [ "$output" != "$expected" ]; then
  echo "unexpected dependency-closed shard result: $output" >&2
  exit 1
fi

# The stable name hash assigns the producer to shard 3.  A raw CTest index
# residue would leave this shard empty because the filtered registry has only
# two direct tests, so this also guards against registration-order sharding.
output=$(python3 "$runner" \
  --test-dir "$binary_dir" \
  --include-regex '^rex_dependency_(producer|consumer)$' \
  --shard-index 3 \
  --shard-count 3 \
  --ctest "$ctest" \
  --jobs 1 \
  --dry-run)

expected="Selected 1 CTest tests"
if [ "$output" != "$expected" ]; then
  echo "unexpected stable name-hash shard result: $output" >&2
  exit 1
fi

python3 "$runner" \
  --test-dir "$binary_dir" \
  --include-regex '^rex_dependency_(producer|consumer)$' \
  --shard-index 2 \
  --shard-count 3 \
  --ctest "$ctest" \
  --jobs 1
