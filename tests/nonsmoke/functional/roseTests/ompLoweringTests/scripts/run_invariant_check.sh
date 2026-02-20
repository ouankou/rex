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

bash "${script_dir}/run_translate_only.sh" "${translator}" "${input_file}" "${workdir}" "${case_name}"
bash "${script_dir}/verify_lowering_invariants.sh" "${input_file}" "${workdir}" "${case_name}"
