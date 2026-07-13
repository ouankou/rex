#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 4 ]]; then
  echo "usage: $0 <extract-script> <input> <expected> <output>" >&2
  exit 2
fi

extract_script=$1
input=$2
expected=$3
output=$4

"$extract_script" "$input" "$output"
diff -u "$output" "$expected"
