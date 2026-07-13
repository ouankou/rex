#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 5 ]]; then
  echo "usage: $0 <translator> <source> <output> <maximum-columns> <sentinel> [translator-flags...]" >&2
  exit 2
fi

translator=$1
source_file=$2
output_file=$3
maximum_columns=$4
sentinel=$5
shift 5

if [[ ! $maximum_columns =~ ^[1-9][0-9]*$ ]]; then
  echo "maximum columns must be a positive integer: $maximum_columns" >&2
  exit 2
fi
if [[ -z $sentinel ]]; then
  echo "continuation sentinel must not be empty" >&2
  exit 2
fi

"$translator" "$@" -rose:output "$output_file" -c "$source_file"

if ! grep -Fq "$sentinel" "$output_file"; then
  echo "generated directive has no continuation sentinel: $sentinel" >&2
  exit 1
fi

if ! awk -v maximum="$maximum_columns" 'length($0) > maximum { exit 1 }' \
    "$output_file"; then
  echo "generated directive exceeds the ${maximum_columns}-column limit" >&2
  exit 1
fi

"$translator" "$@" -rose:output "${output_file}.roundtrip" -c "$output_file"
