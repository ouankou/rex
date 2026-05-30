#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 2 ]]; then
  echo "usage: $0 <parseOmp> <source_file>" >&2
  exit 2
fi

parse_omp=$1
source_file=$2
parse_log="bad_openmp_directive.parse.log"
rose_file="rose_bad_openmp_directive.F90"

if [[ ! -x "$parse_omp" ]]; then
  echo "parseOmp executable is not runnable: $parse_omp" >&2
  exit 1
fi

if [[ ! -f "$source_file" ]]; then
  echo "source file not found: $source_file" >&2
  exit 1
fi

if "$parse_omp" -rose:openmp:ast_only -rose:skipfinalCompileStep -w -rose:verbose 0 -cpp -rose:output "$rose_file" -c "$source_file" >"$parse_log" 2>&1; then
  cat "$parse_log"
  echo "expected parseOmp to reject invalid OpenMP directive: $source_file" >&2
  exit 1
fi

if ! grep -Eq '(^error:|^Error: failed to parse OpenMP directive|Errors in Processing Input File)' "$parse_log"; then
  cat "$parse_log"
  echo "expected an OpenMP parser failure diagnostic for: $source_file" >&2
  exit 1
fi

rm -f "$parse_log" "$rose_file"
