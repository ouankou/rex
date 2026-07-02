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

baseline_dir="${workdir}/baseline"
checkpoint_dir="${workdir}/checkpoint"
json_dir="${checkpoint_dir}/json"
baseline_output="${baseline_dir}/rose_${source_name}"
checkpoint_output="${checkpoint_dir}/rose_${source_name}"

rm -rf "${workdir}"
mkdir -p "${baseline_dir}" "${checkpoint_dir}" "${json_dir}"

bash "${script_dir}/run_mapper_lowering_check.sh" "${translator}" \
  "${input_file}" "${baseline_dir}" "${case_name}"

bash "${script_dir}/run_mapper_lowering_check.sh" "${translator}" \
  "${input_file}" "${checkpoint_dir}" "${case_name}" \
  -rex:ast-json-checkpoint=post-omp-lowering \
  "-rex:ast-json-dir=${json_dir}"

if [[ -z "$(find "${json_dir}" -type f -name '*.json' -print -quit)" ]]; then
  echo "ERROR(${case_name}): post-lowering checkpoint JSON was not written under ${json_dir}" >&2
  exit 1
fi

diff -u "${baseline_output}" "${checkpoint_output}"
