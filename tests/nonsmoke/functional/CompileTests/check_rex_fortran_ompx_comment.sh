#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 3 ]]; then
  echo "usage: $0 <translator> <source> <output> [translator-flags...]" >&2
  exit 2
fi

translator=$1
source_file=$2
output_file=$3
shift 3

check_ompx_line() {
  local file=$1
  local count
  count=$(awk '
    /^[[:space:]]*!\$ompx[[:space:]]+test_nonexistent[[:space:]]*$/ {
      count++
    }
    END {
      print count + 0
    }
  ' "$file")
  if [[ $count -ne 1 ]]; then
    echo "expected exactly one preserved unknown OMPX source line in $file" >&2
    exit 1
  fi
}

"$translator" "$@" -rose:output "$output_file" -c "$source_file"
check_ompx_line "$output_file"

roundtrip_output="${output_file}.roundtrip"
"$translator" "$@" -rose:output "$roundtrip_output" -c "$output_file"
check_ompx_line "$roundtrip_output"
