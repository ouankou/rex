#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 14 ]]; then
  echo "usage: $0 <semantic-script> <translator> <compiler> <input> <oracle_input> <workdir> <mode> <omp_fortran_inc> <omp_runtime_dir> <lowering_inc> <kmpc_fortran_abi_lib> <case_name> <timeout_s> <checkpoint>" >&2
  exit 2
fi

semantic_script="$1"
translator="$2"
compiler="$3"
input_file="$4"
oracle_input_file="$5"
workdir="$6"
mode="$7"
omp_fortran_inc="$8"
omp_runtime_dir="$9"
lowering_inc="${10}"
kmpc_fortran_abi_lib="${11}"
case_name="${12}"
timeout_s="${13}"
checkpoint="${14}"

source_name="$(basename "${input_file}")"
baseline_dir="${workdir}/baseline"
checkpoint_dir="${workdir}/checkpoint"
json_dir="${checkpoint_dir}/json"
baseline_output="${baseline_dir}/rose_${source_name}"
checkpoint_output="${checkpoint_dir}/rose_${source_name}"

rm -rf "${workdir}"
mkdir -p "${baseline_dir}" "${checkpoint_dir}" "${json_dir}"

env -u REX_AST_JSON_CHECKPOINT -u REX_AST_JSON_DIR \
  "${semantic_script}" \
    "${translator}" \
    "${compiler}" \
    "${input_file}" \
    "${oracle_input_file}" \
    "${baseline_dir}" \
    "${mode}" \
    "${omp_fortran_inc}" \
    "${omp_runtime_dir}" \
    "${lowering_inc}" \
    "${kmpc_fortran_abi_lib}" \
    "${case_name}" \
    "${timeout_s}"

env REX_AST_JSON_CHECKPOINT="${checkpoint}" REX_AST_JSON_DIR="${json_dir}" \
  "${semantic_script}" \
    "${translator}" \
    "${compiler}" \
    "${input_file}" \
    "${oracle_input_file}" \
    "${checkpoint_dir}" \
    "${mode}" \
    "${omp_fortran_inc}" \
    "${omp_runtime_dir}" \
    "${lowering_inc}" \
    "${kmpc_fortran_abi_lib}" \
    "${case_name}" \
    "${timeout_s}"

if [[ -z "$(find "${json_dir}" -type f -name '*.json' -print -quit)" ]]; then
  echo "ERROR(${case_name}): checkpoint JSON was not written under ${json_dir}" >&2
  exit 1
fi

if [[ ! -f "${baseline_output}" ]]; then
  echo "ERROR(${case_name}): missing baseline lowered source '${baseline_output}'" >&2
  exit 1
fi

if [[ ! -f "${checkpoint_output}" ]]; then
  echo "ERROR(${case_name}): missing checkpoint lowered source '${checkpoint_output}'" >&2
  exit 1
fi

diff -u "${baseline_output}" "${checkpoint_output}"
