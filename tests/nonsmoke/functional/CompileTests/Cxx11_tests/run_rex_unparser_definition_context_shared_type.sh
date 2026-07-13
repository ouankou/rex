#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 6 ]]; then
  echo "usage: $0 <translator> <compiler> <workdir> <source-a> <source-b> <translator-flags...>" >&2
  exit 2
fi

translator=$1
compiler=$2
workdir=$3
source_a=$4
source_b=$5
shift 5

mkdir -p "$workdir"
output_a="$workdir/rose_$(basename "$source_a")"
output_b="$workdir/rose_$(basename "$source_b")"
rm -f "$output_a" "$output_b"

(
  cd "$workdir"
  "$translator" "$@" -c "$source_a" "$source_b"
)

test -f "$output_a"
test -f "$output_b"
grep -Fq 'rex_definition_context_member_a' "$output_a"
grep -Fq 'rex_definition_context_enumerator_a' "$output_a"
! grep -Fq 'rex_definition_context_member_b' "$output_a"
! grep -Fq 'rex_definition_context_enumerator_b' "$output_a"
grep -Fq 'rex_definition_context_member_b' "$output_b"
grep -Fq 'rex_definition_context_enumerator_b' "$output_b"
! grep -Fq 'rex_definition_context_member_a' "$output_b"
! grep -Fq 'rex_definition_context_enumerator_a' "$output_b"
"$compiler" -std=c++11 -fsyntax-only "$output_a" "$output_b"
