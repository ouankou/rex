#!/usr/bin/env bash

set -euo pipefail

if [[ $# -lt 6 ]]; then
  echo "usage: $0 MODE TRANSLATOR CXX INPUT OUTPUT EXECUTABLE [translator flags...]" >&2
  exit 2
fi

mode=$1
translator=$2
cxx=$3
input=$4
output=$5
executable=$6
shift 6

env REX_LOOP_CORE_MODE="$mode" "$translator" "$@" -std=c++20 -c "$input" \
  -rose:output "$output"
"$cxx" -std=c++20 "$output" -o "$executable"
"$executable"
