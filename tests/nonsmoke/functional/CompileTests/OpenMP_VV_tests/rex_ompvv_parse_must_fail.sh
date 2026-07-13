#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 4 ]]; then
  echo "usage: $0 <parseOmp> <source-file> <expected-contract> <expected-detail> [rose-flags...]" >&2
  exit 2
fi

parse_omp=$1
source_file=$2
expected_contract=$3
expected_detail=$4
shift 4
source_base=$(basename "$source_file")
temp_dir=$(mktemp -d "${TMPDIR:-.}/rex_ompvv_parse_must_fail.XXXXXX")
parse_log="$temp_dir/${source_base}.parse.log"
rose_file="$temp_dir/rose_${source_base}"
trap 'rm -rf "$temp_dir"' EXIT

if [[ ! -x "$parse_omp" ]]; then
  echo "parseOmp executable is not runnable: $parse_omp" >&2
  exit 1
fi

if [[ ! -f "$source_file" ]]; then
  echo "source file not found: $source_file" >&2
  exit 1
fi

rose_flags=(-rose:openmp:ast_only -rose:skipfinalCompileStep -w
            -rose:verbose 0 -rose:output "$rose_file" "$@" -c "$source_file")
case "$source_file" in
  *.f|*.F|*.f90|*.F90|*.f95|*.F95) rose_flags=(-cpp "${rose_flags[@]}") ;;
esac

set +e
"$parse_omp" "${rose_flags[@]}" >"$parse_log" 2>&1
status=$?
set -e

if [[ $status -ne 134 ]] ||
   [[ $(grep -Fxc -- "$expected_contract" "$parse_log") -ne 1 ]] ||
   [[ $(grep -Fxc -- "$expected_detail" "$parse_log") -ne 1 ]]; then
  cat "$parse_log"
  echo "expected one exact hard OpenMP parser rejection for: $source_file" >&2
  exit 1
fi
