#!/usr/bin/env bash
set -euo pipefail

if (($# < 6)); then
  echo "usage: $0 <translator> <input> <output> <width> <continuation-prefix> <translator-args...>" >&2
  exit 2
fi

translator=$1
input=$2
output=$3
width=$4
continuation_prefix=$5
shift 5

if [[ ! $width =~ ^[1-9][0-9]*$ ]]; then
  echo "line width must be a positive integer: $width" >&2
  exit 2
fi
if [[ -z $continuation_prefix ]]; then
  echo "continuation prefix must not be empty" >&2
  exit 2
fi

rm -f "$output" "$output.roundtrip"
"$translator" "$@" -rose:output "$output" -c "$input"

test -s "$output"
awk -v maximum="$width" '
  length($0) > maximum {
    printf "line %d has %d columns, expected at most %d\n", NR, length($0), maximum > "/dev/stderr"
    failed = 1
  }
  END { exit failed }
' "$output"
grep -Fq "$continuation_prefix" "$output"

"$translator" "$@" -rose:output "$output.roundtrip" -c "$output"
test -s "$output.roundtrip"
