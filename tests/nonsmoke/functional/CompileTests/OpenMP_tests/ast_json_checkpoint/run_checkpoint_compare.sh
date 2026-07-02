#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 6 ]]; then
  echo "usage: $0 <parseOmp> <checkpoint> <input.c> <workdir> <output-name> <rose-flags...>" >&2
  exit 2
fi

parse_omp="$1"
checkpoint="$2"
input_file="$3"
workdir="$4"
output_name="$5"
shift 5
rose_flags=("$@")

baseline_dir="${workdir}/baseline"
checkpoint_dir="${workdir}/checkpoint"
json_dir="${checkpoint_dir}/json"
baseline_output="${baseline_dir}/rose_${output_name}"
checkpoint_output="${checkpoint_dir}/rose_${output_name}"

rm -rf "${workdir}"
mkdir -p "${baseline_dir}" "${checkpoint_dir}" "${json_dir}"

(
  cd "${baseline_dir}"
  "${parse_omp}" "${rose_flags[@]}" \
    -rose:output "${baseline_output}" \
    -c "${input_file}" > run.log 2>&1
)

(
  cd "${checkpoint_dir}"
  "${parse_omp}" "${rose_flags[@]}" \
    "-rex:ast-json-checkpoint=${checkpoint}" \
    "-rex:ast-json-dir=${json_dir}" \
    -rose:output "${checkpoint_output}" \
    -c "${input_file}" > run.log 2>&1
)

if [[ -z "$(find "${json_dir}" -type f -name '*.json' -print -quit)" ]]; then
  echo "ERROR(${checkpoint}): checkpoint JSON was not written under ${json_dir}" >&2
  exit 1
fi

diff -u "${baseline_output}" "${checkpoint_output}"
