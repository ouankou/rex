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
script_dir="$(cd "$(dirname "$0")" && pwd)"

baseline_dir="${workdir}/baseline"
checkpoint_dir="${workdir}/checkpoint"
json_dir="${checkpoint_dir}/json"

rm -rf "${workdir}"
mkdir -p "${baseline_dir}" "${checkpoint_dir}" "${json_dir}"

bash "${script_dir}/run_and_check.sh" "${parse_omp}" "${input_file}" \
  "${baseline_dir}" "${case_name}"

(
  cd "${checkpoint_dir}"
  "${parse_omp}" --rex-omp-lowering \
    -rex:ast-json-checkpoint=post-omp-lowering \
    "-rex:ast-json-dir=${json_dir}" \
    -rose:verbose 0 -c "${input_file}" > run.log 2>&1
)

bash "${script_dir}/verify_outputs.sh" "${case_name}" "${checkpoint_dir}"

if [[ -z "$(find "${json_dir}" -type f -name '*.json' -print -quit)" ]]; then
  echo "ERROR(${case_name}): post-lowering checkpoint JSON was not written under ${json_dir}" >&2
  exit 1
fi

mapfile -t baseline_outputs < <(
  cd "${baseline_dir}"
  find . -maxdepth 1 -type f \( -name 'rose_*.c' -o -name 'rex_lib_*.cu' \) | sort
)

if [[ "${#baseline_outputs[@]}" -eq 0 ]]; then
  echo "ERROR(${case_name}): no baseline lowering outputs found" >&2
  exit 1
fi

for rel in "${baseline_outputs[@]}"; do
  diff -u "${baseline_dir}/${rel}" "${checkpoint_dir}/${rel}"
done
