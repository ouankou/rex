#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 4 ]]; then
  echo "usage: $0 <translator> <input> <workdir> <case_name>" >&2
  exit 2
fi

script_dir="$(cd "$(dirname "$0")" && pwd)"
translator="$1"
input_file="$2"
workdir="$3"
case_name="$4"
source_name="$(basename "${input_file}")"
rose_file="${workdir}/rose_${source_name}"

fail() {
  echo "ERROR(${case_name}): $*" >&2
  exit 1
}

bash "${script_dir}/run_translate_only.sh" "${translator}" "${input_file}" \
  "${workdir}" "${case_name}"

[[ -f "${rose_file}" ]] || fail "missing lowered host file '${rose_file}'"

grep -Fq '&v[0:1].len' "${rose_file}" || \
  fail "missing lowered mapper member address for v[0:1].len"
grep -Fq 'v[0:1].data + 0' "${rose_file}" || \
  fail "missing lowered mapper array section for v[0:1].data"
grep -Fq 'sizeof(float ) * v[0:1].len' "${rose_file}" || \
  fail "missing lowered mapper-derived array size expression"
grep -Fq 'void *__args_base[] = {&v[0:1].len, v[0:1].data};' "${rose_file}" || \
  fail "missing lowered mapper base-argument expansion"

if grep -Fq 'void *__args[] = {v};' "${rose_file}"; then
  fail "raw pointer fallback remained in lowered output"
fi
if grep -Fq 'void *__args_base[] = {v};' "${rose_file}"; then
  fail "raw pointer base fallback remained in lowered output"
fi

exit 0
