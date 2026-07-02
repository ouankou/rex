#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 11 ]]; then
  echo "usage: $0 <translator> <compiler> <input> <workdir> <mode> <omp_header_dir> <omp_runtime_dir> <lowering_inc> <case_name> <language> <openmp_version>" >&2
  exit 2
fi

translator="$1"
compiler="$2"
input_file="$3"
workdir="$4"
mode="$5"
omp_header_dir="$6"
omp_runtime_dir="$7"
lowering_inc="$8"
case_name="$9"
language="${10}"
openmp_version="${11}"

if [[ "${mode}" != "exact" && "${mode}" != "sort" ]]; then
  echo "ERROR(${case_name}): invalid mode '${mode}'" >&2
  exit 2
fi

if [[ "${language}" != "c" && "${language}" != "cxx" ]]; then
  echo "ERROR(${case_name}): invalid language '${language}'" >&2
  exit 2
fi

mkdir -p "${workdir}"
rm -f "${workdir}"/orig.exe "${workdir}"/lowered.exe
rm -f "${workdir}"/rose_* "${workdir}"/rex_lib_* "${workdir}"/lower.log

source_name="${input_file##*/}"
rose_file="${workdir}/rose_${source_name}"

compile_flags=(
  -fopenmp=libiomp5
  -O0
  -g
  -U_OPENMP
  "-D_OPENMP=${openmp_version}"
  "-I${omp_header_dir}"
  "-L${omp_runtime_dir}"
  "-Wl,-rpath,${omp_runtime_dir}"
)

if [[ -n "${ROSE_TEST_SANITIZER_FLAGS:-}" ]]; then
  read -r -a sanitizer_flags <<< "${ROSE_TEST_SANITIZER_FLAGS}"
  compile_flags+=("${sanitizer_flags[@]}")
fi

canonicalize() {
  local in_file="$1"
  local out_file="$2"
  if [[ "${mode}" == "sort" ]]; then
    sort "${in_file}" > "${out_file}"
  else
    cp "${in_file}" "${out_file}"
  fi
}

fail_with_diff() {
  local what="$1"
  local lhs="$2"
  local rhs="$3"
  echo "ERROR(${case_name}): ${what} mismatch" >&2
  diff -u "${lhs}" "${rhs}" >&2 || true
  exit 1
}

"${compiler}" "${compile_flags[@]}" "${input_file}" -o "${workdir}/orig.exe"

saved_pwd="${PWD}"
cd "${workdir}"
"${translator}" -rose:openmp:lowering -rose:skipfinalCompileStep -w -rose:verbose 0 \
  -c "${input_file}" > lower.log 2>&1
cd "${saved_pwd}"

if [[ ! -f "${rose_file}" ]]; then
  echo "ERROR(${case_name}): missing lowered host file '${rose_file}'" >&2
  if [[ -f "${workdir}/lower.log" ]]; then
    echo "---- lower.log (${case_name}) ----" >&2
    cat "${workdir}/lower.log" >&2
    echo "---- end lower.log ----" >&2
  fi
  exit 1
fi

lowered_sources=("${rose_file}")
for ext in c cu cpp cxx; do
  rex_file="${workdir}/rex_lib_${source_name%.*}.${ext}"
  if [[ -f "${rex_file}" ]]; then
    lowered_sources+=("${rex_file}")
  fi
done

"${compiler}" "${compile_flags[@]}" -I"${lowering_inc}" "${lowered_sources[@]}" -o "${workdir}/lowered.exe"

export LD_LIBRARY_PATH="${omp_runtime_dir}:${LD_LIBRARY_PATH:-}"
thread_counts=(2 4)
repeats=4
for threads in "${thread_counts[@]}"; do
  for ((i = 1; i <= repeats; ++i)); do
    timeout 30s env OMP_NUM_THREADS="${threads}" "${workdir}/orig.exe" > "${workdir}/orig_${threads}_${i}.out" 2> "${workdir}/orig_${threads}_${i}.err"
    timeout 30s env OMP_NUM_THREADS="${threads}" "${workdir}/lowered.exe" > "${workdir}/low_${threads}_${i}.out" 2> "${workdir}/low_${threads}_${i}.err"

    canonicalize "${workdir}/orig_${threads}_${i}.out" "${workdir}/orig_${threads}_${i}.canon"
    canonicalize "${workdir}/low_${threads}_${i}.out" "${workdir}/low_${threads}_${i}.canon"

    if ! cmp -s "${workdir}/orig_${threads}_${i}.canon" "${workdir}/low_${threads}_${i}.canon"; then
      fail_with_diff "stdout" "${workdir}/orig_${threads}_${i}.canon" "${workdir}/low_${threads}_${i}.canon"
    fi

    if ! cmp -s "${workdir}/orig_${threads}_${i}.err" "${workdir}/low_${threads}_${i}.err"; then
      fail_with_diff "stderr" "${workdir}/orig_${threads}_${i}.err" "${workdir}/low_${threads}_${i}.err"
    fi
  done
done

exit 0
