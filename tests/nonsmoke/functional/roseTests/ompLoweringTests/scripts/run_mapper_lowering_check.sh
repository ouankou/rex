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

grep -Fq '__arg_num = 0;' "${rose_file}" || \
  fail "missing dynamic mapper argument counting"
grep -Fq '__rex_mapper_section_index_0 < (int64_t )n' "${rose_file}" || \
  fail "missing mapper element-expansion loop over runtime section length"
grep -Fq '&v[0 + (long long )__rex_mapper_section_index_0].len' "${rose_file}" || \
  fail "missing lowered mapper member address for per-element expansion"
grep -Fq 'v[0 + (long long )__rex_mapper_section_index_0].data + 0' "${rose_file}" || \
  fail "missing lowered mapper data expansion for per-element section mapping"
grep -Fq 'sizeof(float ) * v[0 + (long long )__rex_mapper_section_index_0].len' "${rose_file}" || \
  fail "missing lowered mapper-derived per-element array size expression"
grep -Fq 'malloc(sizeof(void *) * __arg_num)' "${rose_file}" || \
  fail "missing dynamic mapper runtime argument allocation"

if grep -Fq 'void *__args[] = {v};' "${rose_file}"; then
  fail "raw pointer fallback remained in lowered output"
fi
if grep -Fq 'void *__args_base[] = {v};' "${rose_file}"; then
  fail "raw pointer base fallback remained in lowered output"
fi
if grep -Fq 'v[0:n].len' "${rose_file}"; then
  fail "mapper expansion still uses the whole array section instead of elements"
fi

exit 0
