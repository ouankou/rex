#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 2 ]]; then
  echo "usage: $0 <case_name> <workdir>" >&2
  exit 2
fi

case_name="$1"
workdir="$2"

die() {
  echo "ERROR(${case_name}): $*" >&2
  exit 1
}

require_file() {
  local file="$1"
  [[ -f "${file}" ]] || die "missing output file: ${file}"
}

count_matches() {
  local file="$1"
  local regex="$2"
  grep -Ec "${regex}" "${file}" || true
}

expect_count() {
  local file="$1"
  local regex="$2"
  local expected="$3"
  local label="$4"
  local actual
  actual="$(count_matches "${file}" "${regex}")"
  if [[ "${actual}" != "${expected}" ]]; then
    die "${label}: expected ${expected}, got ${actual} (${file})"
  fi
}

first_line() {
  local file="$1"
  local regex="$2"
  local line
  line="$(grep -nE "${regex}" "${file}" | head -n 1 | cut -d: -f1 || true)"
  echo "${line}"
}

case "${case_name}" in
  rodinia_bfs_like)
    rose_file="${workdir}/rose_rodinia_bfs_like.c"
    cu_file="${workdir}/rex_lib_rodinia_bfs_like.cu"
    require_file "${rose_file}"
    require_file "${cu_file}"

    expect_count "${rose_file}" '#include "rex_kmp.h"' 1 "host runtime include count"
    expect_count "${rose_file}" 'struct[[:space:]]+__tgt_offload_entry[[:space:]]+OUT__' 1 "host offload entry count"
    expect_count "${rose_file}" 'char[[:space:]]+OUT__.*__id__[[:space:]]*=' 1 "host kernel id count"

    expect_count "${cu_file}" '#include "rex_nvidia.h"' 1 "device runtime include count"
    expect_count "${cu_file}" 'extern "C"' 1 "device extern C count"
    expect_count "${cu_file}" '__global__[[:space:]]+void[[:space:]]+OUT__' 1 "device kernel count"
    ;;

  rodinia_srad_comments_like)
    rose_file="${workdir}/rose_rodinia_srad_comments_like.c"
    cu_file="${workdir}/rex_lib_rodinia_srad_comments_like.cu"
    require_file "${rose_file}"
    require_file "${cu_file}"

    expect_count "${rose_file}" '//[[:space:]]*#pragma omp parallel' 4 "commented pragma count"

    rows_line="$(first_line "${rose_file}" 'RODINIA_SRAD_ROWS')"
    cols_line="$(first_line "${rose_file}" 'RODINIA_SRAD_COLS')"
    down_line="$(first_line "${rose_file}" 'RODINIA_SRAD_SCALE_DOWN')"
    up_line="$(first_line "${rose_file}" 'RODINIA_SRAD_SCALE_UP')"
    target_data_line="$(first_line "${rose_file}" 'Translated from #pragma omp target data|__tgt_target_data_begin')"

    [[ -n "${rows_line}" ]] || die "missing RODINIA_SRAD_ROWS marker"
    [[ -n "${cols_line}" ]] || die "missing RODINIA_SRAD_COLS marker"
    [[ -n "${down_line}" ]] || die "missing RODINIA_SRAD_SCALE_DOWN marker"
    [[ -n "${up_line}" ]] || die "missing RODINIA_SRAD_SCALE_UP marker"
    [[ -n "${target_data_line}" ]] || die "missing target data marker"

    mapfile -t pragma_lines < <(
      grep -nE '//[[:space:]]*#pragma omp parallel' "${rose_file}" | cut -d: -f1
    )
    [[ "${#pragma_lines[@]}" -eq 4 ]] || die "expected 4 pragma lines after extraction"

    p1="${pragma_lines[0]}"
    p2="${pragma_lines[1]}"
    p3="${pragma_lines[2]}"
    p4="${pragma_lines[3]}"

    (( rows_line < p1 && p1 < cols_line )) || die "rows pragma ordering mismatch"
    (( cols_line < p2 && p2 < down_line )) || die "cols pragma ordering mismatch"
    (( down_line < p3 && p3 < target_data_line )) || die "scale-down pragma moved near/after target data"
    (( up_line < p4 )) || die "scale-up pragma ordering mismatch"

    (( p1 - rows_line <= 30 )) || die "rows pragma too far from rows marker"
    (( p2 - cols_line <= 30 )) || die "cols pragma too far from cols marker"
    (( p3 - down_line <= 30 )) || die "scale-down pragma too far from marker"
    (( p4 - up_line <= 30 )) || die "scale-up pragma too far from marker"
    ;;

  rodinia_btree_kernel_like)
    rose_file="${workdir}/rose_rodinia_btree_kernel_like.c"
    cu_file="${workdir}/rex_lib_rodinia_btree_kernel_like.cu"
    require_file "${rose_file}"
    require_file "${cu_file}"

    expect_count "${rose_file}" '#include "rex_kmp.h"' 1 "host runtime include count"
    expect_count "${rose_file}" 'struct[[:space:]]+__tgt_offload_entry[[:space:]]+OUT__' 1 "host offload entry count"
    expect_count "${rose_file}" 'char[[:space:]]+OUT__.*__id__[[:space:]]*=' 1 "host kernel id count"
    expect_count "${rose_file}" '//[[:space:]]*main' 1 "host trailing main comment count"

    expect_count "${cu_file}" '#include "rex_nvidia.h"' 1 "device runtime include count"
    expect_count "${cu_file}" '__global__[[:space:]]+void[[:space:]]+OUT__' 1 "device kernel count"
    ;;

  *)
    die "unknown case: ${case_name}"
    ;;
esac
