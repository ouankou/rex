#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 4 ]]; then
  echo "usage: $0 <parseOmp> <input.c> <workdir> <case_name>" >&2
  exit 2
fi

parse_omp="$1"
input_file="$2"
workdir="$3"
case_name="$4"

mkdir -p "${workdir}"
rm -f "${workdir}"/rose_*.c "${workdir}"/rex_lib_*.cu "${workdir}"/run.log

(
  cd "${workdir}"
  "${parse_omp}" --rex-omp-lowering -rose:verbose 0 -c "${input_file}" > run.log 2>&1
)

bash "$(dirname "$0")/verify_outputs.sh" "${case_name}" "${workdir}"
