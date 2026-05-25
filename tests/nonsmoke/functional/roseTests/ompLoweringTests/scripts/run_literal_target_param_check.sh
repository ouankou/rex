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
rose_host_file="${workdir}/rose_${source_name}"
rose_device_file="${workdir}/rose_${source_stem}.cu"

fail() {
  echo "ERROR(${case_name}): $*" >&2
  if [[ -f "${workdir}/lower.log" ]]; then
    echo "---- lower.log (${case_name}) ----" >&2
    cat "${workdir}/lower.log" >&2
    echo "---- end lower.log ----" >&2
  fi
  exit 1
}

mkdir -p "${workdir}"
rm -f "${workdir}"/rose_* "${workdir}"/rex_lib_* "${workdir}"/lower.log

(
  cd "${workdir}"
  "${translator}" -rose:openmp:lowering -rose:skipfinalCompileStep -w -rose:verbose 0 \
    -c "${input_file}" > lower.log 2>&1
)

rose_file="${rose_host_file}"
if [[ ! -f "${rose_file}" ]]; then
  rose_file="${rose_device_file}"
fi
[[ -f "${rose_file}" ]] || fail "missing lowered output for '${source_name}'"

grep -Eq 'rex_pack_literal_arg_bytes\(&n,[[:space:]]*sizeof\(int[[:space:]]*\)\)' "${rose_file}" || \
  fail "missing packed literal target parameter for n"
grep -Fq '__arg_types[] = {288' "${rose_file}" || \
  fail "missing literal map flag for explicit map(to:n)"

if grep -Eq '__args_base\[\] = \{[^}]*&n' "${rose_file}"; then
  fail "scalar n still lowered by reference in __args_base"
fi
if grep -Eq '__args\[\] = \{[^}]*&n' "${rose_file}"; then
  fail "scalar n still lowered by reference in __args"
fi

exit 0
