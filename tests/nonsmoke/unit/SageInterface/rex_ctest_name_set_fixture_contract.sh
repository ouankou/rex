#!/usr/bin/env bash

set -euo pipefail

if [ "$#" -ne 5 ]; then
  echo "usage: $0 runner cmake ctest fixture-project binary-dir" >&2
  exit 2
fi

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
  --ctest "$ctest" \
  --jobs 1 \
  --dry-run)

expected="Validated exact CTest selection: manifest=1, expected_absent=0, regex=0, dependency_support=2, fixture_support=1, union=4, source=$source_dir/manifest.txt, expected_absent_source=<none>"
if [ "$output" != "$expected" ]; then
  echo "unexpected exact-selection result: $output" >&2
  exit 1
fi

output=$(python3 "$runner" \
  --test-dir "$binary_dir" \
  --manifest "$source_dir/manifest.txt" \
  --include-regex '^rex_regex_only$' \
  --ctest "$ctest" \
  --jobs 1 \
  --dry-run)

expected="Validated exact CTest selection: manifest=1, expected_absent=0, regex=1, dependency_support=2, fixture_support=1, union=5, source=$source_dir/manifest.txt, expected_absent_source=<none>"
if [ "$output" != "$expected" ]; then
  echo "unexpected manifest/regex union result: $output" >&2
  exit 1
fi

output=$(python3 "$runner" \
  --test-dir "$binary_dir" \
  --include-regex '^rex_memcheck_(ignored|selected)$' \
  --ctest "$ctest" \
  --jobs 1 \
  --memcheck \
  --dry-run)

expected="Validated exact CTest selection: manifest=0, expected_absent=0, regex=1, dependency_support=0, fixture_support=0, union=1, source=<none>, expected_absent_source=<none>"
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

expected="Validated exact CTest selection: manifest=0, expected_absent=0, regex=1, dependency_support=1, fixture_support=0, union=2, source=<none>, expected_absent_source=<none>"
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

expected="Validated exact CTest selection: manifest=0, expected_absent=0, regex=1, dependency_support=0, fixture_support=0, union=1, source=<none>, expected_absent_source=<none>"
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

output=$(python3 "$runner" \
  --test-dir "$binary_dir" \
  --manifest "$source_dir/rex_manifest_with_expected_absent.txt" \
  --expected-absent-manifest \
    "$source_dir/rex_expected_absent_manifest.txt" \
  --ctest "$ctest" \
  --jobs 1 \
  --dry-run)

expected="Validated exact CTest selection: manifest=1, expected_absent=1, regex=0, dependency_support=2, fixture_support=1, union=4, source=$source_dir/rex_manifest_with_expected_absent.txt, expected_absent_source=$source_dir/rex_expected_absent_manifest.txt"
if [ "$output" != "$expected" ]; then
  echo "unexpected expected-absent manifest result: $output" >&2
  exit 1
fi

if python3 "$runner" \
  --test-dir "$binary_dir" \
  --manifest "$source_dir/manifest.txt" \
  --expected-absent-manifest \
    "$source_dir/rex_expected_absent_manifest.txt" \
  --ctest "$ctest" \
  --jobs 1 \
  --dry-run >"$binary_dir/undeclared-absence.log" 2>&1; then
  echo "undeclared expected-absent test was accepted" >&2
  exit 1
fi
grep -Fq \
  'expected-absent tests are not present in the primary manifest:' \
  "$binary_dir/undeclared-absence.log"

if python3 "$runner" \
  --test-dir "$binary_dir" \
  --manifest "$source_dir/manifest.txt" \
  --expected-absent-manifest "$source_dir/manifest.txt" \
  --ctest "$ctest" \
  --jobs 1 \
  --dry-run >"$binary_dir/unexpected-presence.log" 2>&1; then
  echo "unexpectedly registered expected-absent test was accepted" >&2
  exit 1
fi
grep -Fq \
  'tests declared absent are present in the configured CTest registry:' \
  "$binary_dir/unexpected-presence.log"
