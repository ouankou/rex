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

rm -rf "$binary_dir"
"$cmake" -S "$source_dir" -B "$binary_dir"
output=$(python3 "$runner" \
  --test-dir "$binary_dir" \
  --manifest "$source_dir/manifest.txt" \
  --ctest "$ctest" \
  --jobs 1 \
  --dry-run)

expected="Validated exact CTest selection: manifest=1, regex=0, fixture_support=1, union=2, source=$source_dir/manifest.txt"
if [ "$output" != "$expected" ]; then
  echo "unexpected exact-selection result: $output" >&2
  exit 1
fi
