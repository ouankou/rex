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

verify_common_cuda_lowering() {
  local rose_file="$1"
  local cu_file="$2"
  local kernel_count="$3"
  local include_line
  local offload_entry_line

  require_file "${rose_file}"
  require_file "${cu_file}"

  expect_count "${rose_file}" '#include "rex_kmp.h"' 1 "host runtime include count"
  expect_count "${rose_file}" 'rex_offload_init[[:space:]]*\(' 1 "host offload init count"
  expect_count "${rose_file}" 'rex_offload_fini[[:space:]]*\(' 1 "host offload fini count"
  expect_count "${rose_file}" 'struct[[:space:]]+__tgt_offload_entry[[:space:]]+OUT__' "${kernel_count}" "host offload entry count"
  expect_count "${rose_file}" 'char[[:space:]]+OUT__.*__id__[[:space:]]*=' "${kernel_count}" "host kernel id count"
  expect_count "${rose_file}" '__tgt_target_teams[[:space:]]*\(' "${kernel_count}" "host target teams call count"

  include_line="$(first_line "${rose_file}" '#include "rex_kmp.h"')"
  offload_entry_line="$(first_line "${rose_file}" 'struct[[:space:]]+__tgt_offload_entry[[:space:]]+OUT__')"
  [[ -n "${include_line}" ]] || die "missing rex_kmp.h include"
  [[ -n "${offload_entry_line}" ]] || die "missing host offload entry"
  (( include_line < offload_entry_line )) || die "host include moved after offload entries"

  expect_count "${cu_file}" '#include "rex_nvidia.h"' 1 "device runtime include count"
  expect_count "${cu_file}" 'extern "C"' 1 "device extern C count"
  expect_count "${cu_file}" '__global__[[:space:]]+void[[:space:]]+OUT__' "${kernel_count}" "device kernel count"
}

case "${case_name}" in
  rodinia_axpy_multi_like)
    rose_file="${workdir}/rose_rodinia_axpy_multi_like.c"
    cu_file="${workdir}/rex_lib_rodinia_axpy_multi_like.cu"
    verify_common_cuda_lowering "${rose_file}" "${cu_file}" 3
    expect_count "${rose_file}" 'axpy_like[[:space:]]*\([[:space:]]*x,[[:space:]]*y,[[:space:]]*a,[[:space:]]*n[[:space:]]*\)[[:space:]]*;' 2 "repeated host call count"
    expect_count "${rose_file}" 'scale_like[[:space:]]*\([[:space:]]*x,[[:space:]]*scale,[[:space:]]*n[[:space:]]*\)[[:space:]]*;' 1 "scale host call count"
    expect_count "${rose_file}" 'bias_like[[:space:]]*\([[:space:]]*y,[[:space:]]*bias,[[:space:]]*n[[:space:]]*\)[[:space:]]*;' 1 "bias host call count"
    ;;

  rodinia_bfs_like)
    rose_file="${workdir}/rose_rodinia_bfs_like.c"
    cu_file="${workdir}/rex_lib_rodinia_bfs_like.cu"
    verify_common_cuda_lowering "${rose_file}" "${cu_file}" 1
    ;;

  rodinia_gaussian_like)
    rose_file="${workdir}/rose_rodinia_gaussian_like.c"
    cu_file="${workdir}/rex_lib_rodinia_gaussian_like.cu"
    verify_common_cuda_lowering "${rose_file}" "${cu_file}" 3
    ;;

  rodinia_hotspot_like)
    rose_file="${workdir}/rose_rodinia_hotspot_like.c"
    cu_file="${workdir}/rex_lib_rodinia_hotspot_like.cu"
    verify_common_cuda_lowering "${rose_file}" "${cu_file}" 2
    ;;

  rodinia_nn_like)
    rose_file="${workdir}/rose_rodinia_nn_like.c"
    cu_file="${workdir}/rex_lib_rodinia_nn_like.cu"
    verify_common_cuda_lowering "${rose_file}" "${cu_file}" 1
    time0_line="$(first_line "${rose_file}" 'long long[[:space:]]+time0[[:space:]]*=[[:space:]]*clock[[:space:]]*\(')"
    init_line="$(first_line "${rose_file}" 'rex_offload_init[[:space:]]*\(')"
    [[ -n "${time0_line}" ]] || die "missing timer declaration marker"
    [[ -n "${init_line}" ]] || die "missing rex_offload_init call"
    (( init_line < time0_line )) || die "rex_offload_init moved after timed declaration"
    ;;

  rodinia_pathfinder_like)
    rose_file="${workdir}/rose_rodinia_pathfinder_like.c"
    cu_file="${workdir}/rex_lib_rodinia_pathfinder_like.cu"
    verify_common_cuda_lowering "${rose_file}" "${cu_file}" 1
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

  rodinia_srad_v2_like)
    rose_file="${workdir}/rose_rodinia_srad_v2_like.c"
    cu_file="${workdir}/rex_lib_rodinia_srad_v2_like.cu"
    verify_common_cuda_lowering "${rose_file}" "${cu_file}" 2
    expect_count "${rose_file}" '//[[:space:]]*target data region ends' 1 "target-data trailing comment count"
    ;;

  rodinia_btree_kernel_like)
    rose_file="${workdir}/rose_rodinia_btree_kernel_like.c"
    cu_file="${workdir}/rex_lib_rodinia_btree_kernel_like.cu"
    verify_common_cuda_lowering "${rose_file}" "${cu_file}" 2
    expect_count "${rose_file}" 'kernel_cpu_like[[:space:]]*\([[:space:]]*records,[[:space:]]*nodes,[[:space:]]*count,[[:space:]]*curr_nodes,[[:space:]]*offsets,[[:space:]]*lookup_keys,[[:space:]]*answers[[:space:]]*\)[[:space:]]*;' 2 "first kernel repeated host call count"
    expect_count "${rose_file}" 'kernel_cpu_2_like[[:space:]]*\([[:space:]]*nodes,[[:space:]]*count,[[:space:]]*curr_nodes,[[:space:]]*offsets,[[:space:]]*last_nodes,[[:space:]]*out_begin,[[:space:]]*out_len[[:space:]]*\)[[:space:]]*;' 2 "second kernel repeated host call count"
    expect_count "${rose_file}" 'int64_t __arg_sizes\[\][[:space:]]*=.*\{[[:space:]]*\(int64_t[[:space:]]*\)0,' 2 "implicit pointer zero-size count"
    expect_count "${rose_file}" 'int64_t __arg_types\[\][[:space:]]*=.*\{[[:space:]]*544,' 2 "implicit pointer arg-type count"
    expect_count "${rose_file}" 'int64_t __arg_types\[\].*35' 2 "tofrom arg-type count"
    expect_count "${rose_file}" 'int64_t __arg_types\[\].*32' 0 "unexpected target-param-only arg types"
    expect_count "${rose_file}" '//[[:space:]]*main' 1 "host trailing main comment count"
    ;;

  *)
    die "unknown case: ${case_name}"
    ;;
esac
