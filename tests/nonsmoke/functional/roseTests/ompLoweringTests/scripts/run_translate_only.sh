#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 4 ]]; then
  echo "usage: $0 <translator> <input> <workdir> <case_name> [translator_flags...]" >&2
  exit 2
fi

translator="$1"
input_file="$2"
workdir="$3"
case_name="$4"
shift 4
extra_translator_flags=("$@")

mkdir -p "${workdir}"
rm -f "${workdir}"/rose_* "${workdir}"/rex_lib_* "${workdir}"/lower.log

(
  cd "${workdir}"
  "${translator}" -rose:openmp:lowering -rose:skipfinalCompileStep -w -rose:verbose 0 \
    "${extra_translator_flags[@]}" \
    -c "${input_file}" > lower.log 2>&1
)

source_name="$(basename "${input_file}")"
rose_file="${workdir}/rose_${source_name}"
if [[ ! -f "${rose_file}" ]]; then
  echo "ERROR(${case_name}): missing lowered host file '${rose_file}'" >&2
  if [[ -f "${workdir}/lower.log" ]]; then
    echo "---- lower.log (${case_name}) ----" >&2
    cat "${workdir}/lower.log" >&2
    echo "---- end lower.log ----" >&2
  fi
  exit 1
fi

exit 0
