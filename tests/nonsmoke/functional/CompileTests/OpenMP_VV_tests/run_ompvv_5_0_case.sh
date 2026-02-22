#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 10 ]]; then
  echo "usage: $0 <parseOmp> <extract_script> <source_file> <reference_file> <rose_file> <output_file> <diff_file> <ompvv_source_root> <ompvv_support_root> <source_extension> [rose_flags...]" >&2
  exit 2
fi

parse_omp=$1
extract_script=$2
source_file=$3
reference_file=$4
rose_file=$5
output_file=$6
diff_file=$7
ompvv_source_root=$8
ompvv_support_root=$9
source_extension=${10}
shift 10

rose_flags=("$@")

if [[ ! -x "$parse_omp" ]]; then
  echo "parseOmp executable is not runnable: $parse_omp" >&2
  exit 1
fi

if [[ ! -x "$extract_script" ]]; then
  echo "extract script is not executable: $extract_script" >&2
  exit 1
fi

if [[ ! -f "$source_file" ]]; then
  echo "source file not found: $source_file" >&2
  exit 1
fi

if [[ ! -f "$reference_file" ]]; then
  echo "reference file not found: $reference_file" >&2
  exit 1
fi

source_dir=$(dirname "$source_file")
parse_log="${rose_file}.parse.log"

include_flags=(
  -I"$ompvv_source_root"
  -I"$ompvv_support_root"
  -I"$source_dir"
)

"$parse_omp" "${rose_flags[@]}" -rose:output "$rose_file" "${include_flags[@]}" -c "$source_file" >"$parse_log" 2>&1

if grep -q "Errors in Processing Input File" "$parse_log"; then
  cat "$parse_log"
  exit 1
fi

if [[ ! -f "$rose_file" ]]; then
  alt_rose_file="$rose_file"
  case "$source_extension" in
    .F90) alt_rose_file=${rose_file%.F90}.f90 ;;
    .F) alt_rose_file=${rose_file%.F}.f ;;
  esac
  if [[ -f "$alt_rose_file" ]]; then
    rose_file="$alt_rose_file"
  else
    cat "$parse_log"
    echo "expected ROSE output file not found: $rose_file" >&2
    exit 1
  fi
fi

"$extract_script" "$rose_file" "$output_file"

if diff -u "$output_file" "$reference_file" >"$diff_file"; then
  rm -f "$diff_file" "$parse_log"
  exit 0
fi

cat "$diff_file"
rm -f "$diff_file"
exit 1
