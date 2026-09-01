#!/usr/bin/env bash

set -euo pipefail

if [ "$#" -ne 3 ] || [ ! -d "$1" ] || [ ! -d "$2" ] || [ ! -f "$3" ]; then
  echo "usage: $0 .github/workflows tests/fragile C-tests-CMakeLists" >&2
  exit 2
fi

workflow_dir=$1
fragile_dir=$2
c_tests_cmake=$3
image_workflow="${workflow_dir}/nightly-rex-images.yml"
ci_workflow="${workflow_dir}/ci-nightly.yml"
publish_script="${workflow_dir}/../../scripts/ci-publish-nightly-images"
test_script="${workflow_dir}/../../scripts/ci-test-nightly-image"
non_x86_absent_manifest="${fragile_dir}/unparser_hardening_fast_non_x86_absent.txt"
cross_manifest="${fragile_dir}/rex_nightly_cross_architecture.txt"
image_matrix=$(sed -n '/^      matrix:$/,/^    env:$/p' "$image_workflow")
ci_matrix=$(sed -n '/^      matrix:$/,/^    env:$/p' "$ci_workflow")

for input in "$image_workflow" "$ci_workflow" "$publish_script" "$test_script"; do
  if [ ! -f "$input" ]; then
    echo "required nightly CI input does not exist: $input" >&2
    exit 2
  fi
done

if [ ! -f "$non_x86_absent_manifest" ] || [ ! -f "$cross_manifest" ]; then
  echo "non-x86 architecture manifests do not exist" >&2
  exit 1
fi
expected_non_x86_absences=$'C_tests_test2015_141_c\nC_tests_test2015_142_c'
actual_non_x86_absences=$(sed -e 's/#.*//' -e '/^[[:space:]]*$/d' \
  "$non_x86_absent_manifest" | sort)
if [ "$actual_non_x86_absences" != "$expected_non_x86_absences" ]; then
  echo "non-x86 expected-absence manifest changed its exact reviewed set" >&2
  exit 1
fi
expected_cross_tests=$'C_tests_test2015_141_c\nC_tests_test2015_142_c\nCxx20_tests_rex_frontend_module_external_ownership_structure\nCxx20_tests_rex_frontend_target_builtin_type_aarch64\nCxx20_tests_rex_frontend_target_builtin_type_riscv64\nCxx20_tests_rex_frontend_unowned_imported_namespace\nCxx20_tests_rex_frontend_variable_template_semantic_cross_file_constraint_cpp\nCxx_tests_rex_frontend_support_namespace_semantic_root\nCxx_tests_rex_test2026_stl_map_cpp\nastInterface_rex_test2026_stl_regex\nomp_lowering_rex_simd_duplicate_length_clause_is_rejected\nomp_lowering_rex_simd_induction_scalar_is_rejected\nomp_lowering_rex_simd_intel_target_mismatch_is_rejected\nomp_lowering_rex_simd_length_absent\nomp_lowering_rex_simd_length_both\nomp_lowering_rex_simd_length_order_is_rejected\nomp_lowering_rex_simd_length_safelen\nomp_lowering_rex_simd_length_simdlen\nomp_lowering_rex_simd_length_unsupported_is_rejected\nomp_lowering_rex_simd_mixed_lane_width_is_rejected\nomp_lowering_rex_simd_mixed_type_is_rejected\nomp_lowering_rex_simd_runtime_index_is_rejected\nomp_lowering_rex_simd_safelen_too_small_is_rejected\nomp_lowering_rex_simd_strided_is_rejected\nomp_lowering_rex_simd_two_region_reduction_ownership\nomp_lowering_rex_simd_value_cast_is_rejected'
actual_cross_tests=$(sed -e 's/#.*//' -e '/^[[:space:]]*$/d' \
  "$cross_manifest" | sort)
if [ "$actual_cross_tests" != "$expected_cross_tests" ]; then
  echo "cross-architecture manifest changed its exact reviewed set" >&2
  exit 1
fi
if [ "$(grep -Fxc 'rex_callgraph_test04_answer_manifest_contract' \
     "${fragile_dir}/unparser_hardening_fast.txt")" -ne 1 ]; then
  echo "fast retained manifest lacks the callgraph answer registry contract" >&2
  exit 1
fi
if [ "$(grep -Fxc 'check_policies' \
     "${fragile_dir}/unparser_hardening_full.txt")" -ne 1 ]; then
  echo "native full manifest must retain the architecture-independent policy suite" >&2
  exit 1
fi
x86_only_sources=$(sed -n \
  '/^set(C_TESTCODES_X86_ONLY$/,/^)$/p' "$c_tests_cmake")
for source in test2015_141.c test2015_142.c; do
  if [ "$(grep -Fxc "  $source" <<<"$x86_only_sources")" -ne 1 ]; then
    echo "expected-absent test source is not exactly x86-only: $source" >&2
    exit 1
  fi
done

if [ "$(grep -Fc 'org.opencontainers.image.revision=${{ github.sha }}' \
       "$image_workflow")" -ne 4 ] ||
   [ "$(grep -Fc '${image_repo}:${tag}-${source_sha}' \
       "$publish_script")" -ne 2 ] ||
   [ "$(grep -Fxc 'publish_manifest "${digest_directory}/runtime" latest' \
       "$publish_script")" -ne 1 ] ||
   [ "$(grep -Fxc 'publish_manifest "${digest_directory}/dev" dev' \
       "$publish_script")" -ne 1 ]; then
  echo "nightly images lack one immutable revision label/tag contract" >&2
  exit 1
fi

if [ "$(grep -Fc 'REX_ENABLE_X86_SIMD_TESTS=ON' "$image_workflow")" -ne 2 ]; then
  echo "nightly development images must explicitly select x86 SIMD tests" >&2
  exit 1
fi
if [ "$(grep -Fc 'REX_ENABLE_UNINITIALIZED_FIELD_TESTS=OFF' \
     "$image_workflow")" -ne 2 ] ||
   [ "$(grep -Fc 'REX_ENABLE_UNINITIALIZED_FIELD_TESTS=${{ matrix.uninitialized_fields }}' \
     "$image_workflow")" -ne 2 ] ||
   [ "$(grep -Fxc '          uninitialized_fields: "ON"' \
     <<<"$image_matrix")" -ne 1 ] ||
   [ "$(grep -Fxc '          uninitialized_fields: "OFF"' \
     <<<"$image_matrix")" -ne 3 ]; then
  echo "nightly images lack their exact explicit uninitialized-field availability contract" >&2
  exit 1
fi

if [ "$(grep -Fc 'unparser_hardening_full.txt' "$test_script")" -ne 1 ] ||
   [ "$(grep -Fc 'rex|astInterface|OMPLOWERING_RODINIA_' \
       "$test_script")" -ne 1 ] ||
   [ "$(grep -Fxc '          suite: full' <<<"$ci_matrix")" -ne 1 ]; then
  echo "native x86_64 nightly must retain the complete manifest and core regex" >&2
  exit 1
fi

if [ "$(grep -Fc 'rex_nightly_cross_architecture.txt' "$test_script")" -ne 1 ] ||
   [ "$(grep -Fc 'unparser_hardening_fast_non_x86_absent.txt' \
       "$test_script")" -ne 1 ] ||
   [ "$(grep -Fxc '          suite: cross' <<<"$ci_matrix")" -ne 3 ]; then
  echo "non-x86 nightly matrix lacks the exact architecture manifest contract" >&2
  exit 1
fi

if [ "$(grep -Fc 'image_ref="${IMAGE_REPO:?}:dev-${source_sha}"' \
     "$test_script")" -ne 1 ] ||
   [ "$(grep -Fc 'org.opencontainers.image.revision' "$test_script")" -ne 1 ] ||
   ! grep -Fq '^[0-9a-f]{40}$' "$test_script" ||
   ! grep -Fq "workflows:" "$ci_workflow" ||
   ! grep -Fq -- "- Nightly REX Images" "$ci_workflow" ||
   ! grep -Fq "github.event.workflow_run.conclusion == 'success'" \
     "$ci_workflow"; then
  echo "nightly CI does not hard-bind the matrix to one successfully published image revision" >&2
  exit 1
fi

if [ "$(grep -Fxc '    test_jobs=${ACT_CTEST_JOBS:-8}' "$test_script")" -ne 1 ] ||
   [ "$(grep -Fxc '    test_jobs=$(nproc)' "$test_script")" -ne 1 ] ||
   [ "$(grep -Fc 'tests/fragile:/opt/rex/src/tests/fragile:ro' \
     "$test_script")" -ne 1 ] ||
   [ "$(grep -Fxc '    -e REX_NIGHTLY_TEST_JOBS="$test_jobs"' \
     "$test_script")" -ne 1 ]; then
  echo "local tests lack current manifests or bounded parallelism without changing hosted CI" >&2
  exit 1
fi

for arch in amd64 arm64 riscv64 loong64; do
  if [ "$(grep -Fxc "        - arch: $arch" <<<"$image_matrix")" -ne 1 ] ||
     [ "$(grep -Fxc "        - arch: $arch" <<<"$ci_matrix")" -ne 1 ]; then
    echo "$arch is not represented exactly once in both nightly matrices" >&2
    exit 1
  fi
done
