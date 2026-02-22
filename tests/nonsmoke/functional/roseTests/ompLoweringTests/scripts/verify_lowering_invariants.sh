#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 3 ]]; then
  echo "usage: $0 <input> <workdir> <case_name>" >&2
  exit 2
fi

input_file="$1"
workdir="$2"
case_name="$3"
source_name="$(basename "${input_file}")"
rose_file="${workdir}/rose_${source_name}"

fail() {
  echo "ERROR(${case_name}): $*" >&2
  exit 1
}

[[ -f "${rose_file}" ]] || fail "missing lowered host file '${rose_file}'"

# Lowered sources must not retain active OpenMP pragmas.
if grep -Eiq '^[[:space:]]*(#[[:space:]]*pragma[[:space:]]+omp\b|!\$omp\b)' "${rose_file}"; then
  fail "active OpenMP pragma remained in lowered host output"
fi

# Host runtime include should not be duplicated.
include_count="$(grep -Ec '#include[[:space:]]+"rex_kmp.h"' "${rose_file}" || true)"
if [[ "${include_count}" -gt 1 ]]; then
  fail "duplicate rex_kmp.h includes in lowered host output"
fi

# Lowered code should contain at least one OpenMP runtime call for sources
# that contain active OpenMP pragmas.
runtime_count="$(grep -Ec '__kmpc_|\bXOMP_' "${rose_file}" || true)"
input_pragma_count="$(grep -Eic '^[[:space:]]*(#[[:space:]]*pragma[[:space:]]+omp\b|!\$omp\b)' "${input_file}" || true)"
if [[ "${input_pragma_count}" -gt 0 && "${runtime_count}" -eq 0 ]]; then
  fail "no OpenMP runtime calls found in lowered host output"
fi

# If a companion runtime file exists, it must define at least one outlined helper.
for ext in c cu cpp cxx; do
  rex_file="${workdir}/rex_lib_${source_name%.*}.${ext}"
  if [[ -f "${rex_file}" ]]; then
    helper_count="$(grep -Ec 'OUT__' "${rex_file}" || true)"
    if [[ "${helper_count}" -eq 0 ]]; then
      fail "runtime companion file '${rex_file}' has no outlined helpers"
    fi
  fi
done

exit 0
