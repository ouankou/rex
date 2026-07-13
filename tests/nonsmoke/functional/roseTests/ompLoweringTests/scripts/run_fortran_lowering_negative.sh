#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 5 ]]; then
  echo "usage: $0 <translator> <input> <workdir> <omp_fortran_inc> <expected_diagnostic>" >&2
  exit 2
fi

translator="$1"
input_file="$2"
workdir="$3"
omp_fortran_inc="$4"
expected_diagnostic="$5"

mkdir -p "${workdir}"
log_file="${workdir}/lowering-negative.log"
source_name="$(basename "${input_file}")"

set +e
(
  cd "${workdir}"
  "${translator}" -rose:openmp:lowering -rose:skipfinalCompileStep \
    -w -rose:verbose 0 "-I${omp_fortran_inc}" -c "${input_file}"
) >"${log_file}" 2>&1
status=$?
set -e

if [[ ${status} -eq 0 ]]; then
  echo "ERROR(${source_name}): lowering unexpectedly accepted an unsupported declarative directive" >&2
  cat "${log_file}" >&2
  exit 1
fi

if ! grep -Fq "${expected_diagnostic}" "${log_file}"; then
  echo "ERROR(${source_name}): lowering failed without the exact hard diagnostic '${expected_diagnostic}'" >&2
  cat "${log_file}" >&2
  exit 1
fi

if [[ -e "${workdir}/rose_${source_name}" ]]; then
  echo "ERROR(${source_name}): failed lowering emitted a partial translated source" >&2
  exit 1
fi
