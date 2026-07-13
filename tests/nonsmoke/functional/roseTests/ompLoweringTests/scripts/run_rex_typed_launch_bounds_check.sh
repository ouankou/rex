#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 4 ]]; then
  echo "usage: $0 <translator> <input> <workdir> <case_name>" >&2
  exit 2
fi

translator="$1"
input_file="$2"
workdir="$3"
case_name="$4"
source_name="$(basename "${input_file}")"
source_stem="${source_name%.*}"
device_file="${workdir}/rex_lib_${source_stem}.cu"

fail() {
  echo "ERROR(${case_name}): $*" >&2
  if [[ -f "${workdir}/lower.log" ]]; then
    cat "${workdir}/lower.log" >&2
  fi
  exit 1
}

mkdir -p "${workdir}"
rm -f "${workdir}"/rose_* "${workdir}"/rex_lib_* "${workdir}"/lower.log

(
  cd "${workdir}"
  "${translator}" -rose:openmp:lowering -rose:skipfinalCompileStep \
    -rose:verbose 0 -c "${input_file}" >lower.log 2>&1
)

[[ -f "${device_file}" ]] || \
  fail "missing lowered device output '${device_file}'"

launch_bounds_count="$(grep -Fc '__launch_bounds__(' "${device_file}")"
[[ "${launch_bounds_count}" -eq 1 ]] || \
  fail "expected exactly one typed launch-bounds attribute, found ${launch_bounds_count}"
grep -Eq '__launch_bounds__\([[:space:]]*\(?32[[:space:]]*\+[[:space:]]*16\)?[[:space:]]*\)' \
  "${device_file}" || fail "launch-bounds expression was not preserved"
