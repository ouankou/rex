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
amd64_workflow="${workflow_dir}/ci-amd64-nightly.yml"
non_x86_absent_manifest="${fragile_dir}/unparser_hardening_fast_non_x86_absent.txt"
image_amd64_job=$(sed -n '/^  build-amd64:/,/^  build-arm64:/p' \
  "$image_workflow")
image_non_amd64_jobs=$(sed -n '/^  build-arm64:/,/^  publish-manifests:/p' \
  "$image_workflow")

for workflow in "$image_workflow" "$amd64_workflow" \
  "${workflow_dir}/ci-arm64-nightly.yml" \
  "${workflow_dir}/ci-loongarch64-nightly.yml" \
  "${workflow_dir}/ci-riscv64-nightly.yml"; do
  if [ ! -f "$workflow" ]; then
    echo "required nightly workflow does not exist: $workflow" >&2
    exit 2
  fi
done

if [ ! -f "$non_x86_absent_manifest" ]; then
  echo "non-x86 expected-absence manifest does not exist" >&2
  exit 1
fi
expected_non_x86_absences=$'C_tests_test2015_141_c\nC_tests_test2015_142_c'
actual_non_x86_absences=$(sed -e 's/#.*//' -e '/^[[:space:]]*$/d' \
  "$non_x86_absent_manifest" | sort)
if [ "$actual_non_x86_absences" != "$expected_non_x86_absences" ]; then
  echo "non-x86 expected-absence manifest changed its exact reviewed set" >&2
  exit 1
fi
if [ "$(grep -Fxc 'rex_callgraph_test04_answer_manifest_contract' \
     "${fragile_dir}/unparser_hardening_fast.txt")" -ne 1 ]; then
  echo "fast retained manifest lacks the callgraph answer registry contract" >&2
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
       "$image_workflow")" -ne 16 ] ||
   [ "$(grep -Fc '${REX_RUNTIME_TAG}-${GITHUB_SHA}' "$image_workflow")" -ne 1 ] ||
   [ "$(grep -Fc '${REX_DEV_TAG}-${GITHUB_SHA}' "$image_workflow")" -ne 1 ]; then
  echo "nightly images lack one immutable revision label/tag contract" >&2
  exit 1
fi

if [ "$(grep -Fc 'REX_ENABLE_X86_SIMD_TESTS=ON' "$image_workflow")" -ne 8 ]; then
  echo "nightly development images must explicitly select x86 SIMD tests" >&2
  exit 1
fi
if [ "$(grep -Fc 'REX_ENABLE_UNINITIALIZED_FIELD_TESTS=OFF' \
     <<<"$image_amd64_job")" -ne 2 ] ||
   [ "$(grep -Fc 'REX_ENABLE_UNINITIALIZED_FIELD_TESTS=OFF' \
     <<<"$image_non_amd64_jobs")" -ne 12 ]; then
  echo "nightly images lack their exact explicit uninitialized-field availability contract" >&2
  exit 1
fi

if [ "$(grep -Fc 'unparser_hardening_full.txt' "$amd64_workflow")" -ne 2 ] ||
   [ "$(grep -Fc 'rex|astInterface|OMPLOWERING_RODINIA_' \
       "$amd64_workflow")" -ne 2 ]; then
  echo "native x86_64 nightly must retain the complete manifest and core regex" >&2
  exit 1
fi

for arch in arm64 loongarch64 riscv64; do
  workflow="${workflow_dir}/ci-${arch}-nightly.yml"
  if [ "$(grep -Fc 'unparser_hardening_fast.txt' "$workflow")" -ne 2 ] ||
     [ "$(grep -Fc 'unparser_hardening_fast_non_x86_absent.txt' \
         "$workflow")" -ne 2 ] ||
     [ "$(grep -Fc 'variable_template_semantic_cross_file_constraint_cpp' \
         "$workflow")" -ne 2 ] ||
     [ "$(grep -Fc 'omp_lowering_rex_simd_.*' "$workflow")" -ne 2 ] ||
     grep -Fq 'rex|astInterface|OMPLOWERING_RODINIA_' "$workflow"; then
    echo "${arch} nightly lacks the exact fast-plus-architecture contract" >&2
    exit 1
  fi
done

for arch in amd64 arm64 loongarch64 riscv64; do
  workflow="${workflow_dir}/ci-${arch}-nightly.yml"
  if [ "$(grep -Fc 'image_ref="${IMAGE_REPO}:dev-${source_sha}"' \
       "$workflow")" -ne 1 ] ||
     [ "$(grep -Fc 'org.opencontainers.image.revision' "$workflow")" -ne 1 ] ||
     ! grep -Fq '^[0-9a-f]{40}$' "$workflow"; then
    echo "${arch} nightly does not hard-bind tests to one labeled image revision" >&2
    exit 1
  fi
done
