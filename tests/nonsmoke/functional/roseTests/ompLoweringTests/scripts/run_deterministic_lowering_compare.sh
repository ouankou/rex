#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 5 ]]; then
  echo "usage: $0 <translator> <input> <workdir> <case_name> <checkpoint|none>" >&2
  exit 2
fi

script_dir="$(cd "$(dirname "$0")" && pwd)"
translator="$1"
input_file="$2"
workdir="$3"
case_name="$4"
checkpoint="$5"
source_name="$(basename "${input_file}")"

rm -rf "${workdir}"
mkdir -p "${workdir}/run1" "${workdir}/run2"

run_once() {
  local run_dir="$1"
  shift
  bash "${script_dir}/run_translate_only.sh" "${translator}" "${input_file}" \
    "${run_dir}" "${case_name}" "$@"
}

if [[ "${checkpoint}" == "none" ]]; then
  run_once "${workdir}/run1"
  run_once "${workdir}/run2"
else
  mkdir -p "${workdir}/run1/json" "${workdir}/run2/json"
  run_once "${workdir}/run1" \
    "-rex:ast-json-checkpoint=${checkpoint}" \
    "-rex:ast-json-dir=${workdir}/run1/json"
  run_once "${workdir}/run2" \
    "-rex:ast-json-checkpoint=${checkpoint}" \
    "-rex:ast-json-dir=${workdir}/run2/json"
fi

diff -u "${workdir}/run1/rose_${source_name}" \
        "${workdir}/run2/rose_${source_name}"
