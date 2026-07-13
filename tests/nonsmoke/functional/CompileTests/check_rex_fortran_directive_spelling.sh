#!/usr/bin/env bash
set -euo pipefail

if (($# < 5)); then
  echo "usage: $0 <translator> <source> <output> <expected-file> <translator-args...>" >&2
  exit 2
fi

translator=$1
source_file=$2
output_file=$3
expected_file=$4
shift 4

if [[ ! -f $expected_file ]]; then
  echo "expected-spelling file does not exist: $expected_file" >&2
  exit 2
fi
if ! grep -q '[^[:space:]]' "$expected_file"; then
  echo "expected-spelling file has no nonempty contracts: $expected_file" >&2
  exit 2
fi

rm -f "$output_file" "${output_file}.roundtrip"
"$translator" "$@" -rose:output "$output_file" -c "$source_file"
test -s "$output_file"

check_expected_spelling() {
  local generated_file=$1
  while IFS= read -r expected || [[ -n "$expected" ]]; do
    [[ -z "$expected" ]] && continue
    if ! grep -Fq -- "$expected" "$generated_file"; then
      echo "generated directive does not preserve exact spelling in ${generated_file}: ${expected}" >&2
      exit 1
    fi
  done < "$expected_file"
}

check_expected_spelling "$output_file"

"$translator" "$@" -rose:output "${output_file}.roundtrip" -c "$output_file"
test -s "${output_file}.roundtrip"
check_expected_spelling "${output_file}.roundtrip"
