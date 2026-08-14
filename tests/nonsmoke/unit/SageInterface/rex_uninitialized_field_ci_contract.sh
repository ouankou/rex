#!/usr/bin/env bash

set -euo pipefail

if [ "$#" -ne 9 ]; then
  echo "usage: $0 cmake root-CMakeLists option-module CompileTests-CMakeLists uninitialized-CMakeLists build-rex nightly-Dockerfile workflows-dir ci-install-deps" >&2
  exit 2
fi

cmake_command=$1
root_cmake=$2
option_module=$3
compile_tests_cmake=$4
uninitialized_cmake=$5
build_rex=$6
nightly_dockerfile=$7
workflow_dir=$8
ci_install_deps=$9
if [ ! -x "$cmake_command" ]; then
  echo "configured CMake command is not executable: $cmake_command" >&2
  exit 2
fi
for input in "$root_cmake" "$option_module" "$compile_tests_cmake" \
  "$uninitialized_cmake" "$build_rex" \
  "$nightly_dockerfile" "$ci_install_deps"; do
  if [ ! -f "$input" ]; then
    echo "required uninitialized-field contract input is missing: $input" >&2
    exit 2
  fi
done
if [ ! -d "$workflow_dir" ]; then
  echo "required workflow directory is missing: $workflow_dir" >&2
  exit 2
fi

if [ "$(grep -Fxc 'include(cmake/rex_uninitialized_field_tests.cmake)' \
     "$root_cmake")" -ne 1 ]; then
  echo "root CMake must configure the uninitialized-field option after Valgrind discovery" >&2
  exit 1
fi
valgrind_validation_line=$(grep -Fn \
  'if(VALGRIND_INCLUDE_PATH AND NOT EXISTS' "$root_cmake" | cut -d: -f1)
option_include_line=$(grep -Fn \
  'include(cmake/rex_uninitialized_field_tests.cmake)' "$root_cmake" | \
  cut -d: -f1)
if [ -z "$valgrind_validation_line" ] || [ -z "$option_include_line" ] ||
   [ "$option_include_line" -le "$valgrind_validation_line" ]; then
  echo "uninitialized-field option default was evaluated before Valgrind validation" >&2
  exit 1
fi
if ! default_output=$("$cmake_command" -DROSE_USE_VALGRIND=FALSE \
     -P "$option_module" 2>&1); then
  echo "Valgrind-unavailable default configuration must disable the uninitialized-field suite" >&2
  echo "$default_output" >&2
  exit 1
fi
if explicit_output=$("$cmake_command" -DROSE_USE_VALGRIND=FALSE \
     -DREX_ENABLE_UNINITIALIZED_FIELD_TESTS=TRUE \
     -P "$option_module" 2>&1); then
  echo "explicit uninitialized-field enablement without Valgrind must fail" >&2
  exit 1
fi
if ! grep -Fq 'REX_ENABLE_UNINITIALIZED_FIELD_TESTS=ON requires executable Valgrind' \
     <<<"$explicit_output"; then
  echo "explicit uninitialized-field enablement lacks its hard dependency diagnostic" >&2
  exit 1
fi
if ! "$cmake_command" -DROSE_USE_VALGRIND=TRUE \
     -DREX_ENABLE_UNINITIALIZED_FIELD_TESTS=TRUE \
     -P "$option_module" >/dev/null 2>&1; then
  echo "complete Valgrind support must admit explicit uninitialized-field enablement" >&2
  exit 1
fi

if grep -Fq 'find_path(' "$uninitialized_cmake" ||
   ! grep -Fq '${VALGRIND_INCLUDE_PATH}/valgrind/valgrind.h' \
     "$uninitialized_cmake" ||
   ! grep -Fq 'PRIVATE ${VALGRIND_INCLUDE_PATH}' "$uninitialized_cmake"; then
  echo "uninitialized-field tests must consume the root-validated Valgrind headers" >&2
  exit 1
fi

registration_block=$(sed -n \
  '/if(REX_ENABLE_UNINITIALIZED_FIELD_TESTS)/,/endif()/p' \
  "$compile_tests_cmake")
if [ "$(grep -Fxc '    add_subdirectory(uninitializedField_tests)' \
     <<<"$registration_block")" -ne 1 ]; then
  echo "CompileTests must register uninitialized-field tests only through the explicit option" >&2
  exit 1
fi

if [ "$(grep -Fc 'REX_ENABLE_UNINITIALIZED_FIELD_TESTS' "$build_rex")" -ne 4 ] ||
   ! grep -Fq -- '-DREX_ENABLE_UNINITIALIZED_FIELD_TESTS="$REX_ENABLE_UNINITIALIZED_FIELD_TESTS"' \
     "$build_rex"; then
  echo "build-rex does not hard-validate and forward the uninitialized-field option" >&2
  exit 1
fi

if [ "$(grep -Fxc 'ARG REX_ENABLE_UNINITIALIZED_FIELD_TESTS' \
     "$nightly_dockerfile")" -ne 1 ] ||
   [ "$(grep -Fxc '      ""|OFF|ON) ;; \' \
     "$nightly_dockerfile")" -ne 1 ] ||
   [ "$(grep -Fc 'REX_ENABLE_UNINITIALIZED_FIELD_TESTS="${REX_ENABLE_UNINITIALIZED_FIELD_TESTS}"' \
     "$nightly_dockerfile")" -ne 1 ] ||
   [ "$(grep -Fc 'REX_ENABLE_UNINITIALIZED_FIELD_TESTS must be ON or OFF' \
     "$nightly_dockerfile")" -ne 1 ]; then
  echo "nightly image builder does not hard-validate and forward the uninitialized-field option" >&2
  exit 1
fi

if [ "$(grep -Fxc '      valgrind' "$ci_install_deps")" -ne 1 ]; then
  echo "native CI dependencies must install Valgrind for availability-derived field coverage" >&2
  exit 1
fi

image_workflow="$workflow_dir/nightly-rex-images.yml"
ci_workflow="$workflow_dir/ci.yml"
memory_workflow="$workflow_dir/weekly-memory.yml"
for workflow in "$image_workflow" "$ci_workflow" "$memory_workflow"; do
  if [ ! -f "$workflow" ]; then
    echo "required workflow is missing: $workflow" >&2
    exit 2
  fi
done

amd64_job=$(sed -n '/^  build-amd64:/,/^  build-arm64:/p' "$image_workflow")
emulated_jobs=$(sed -n '/^  build-arm64:/,/^  publish-manifests:/p' \
  "$image_workflow")
if [ "$(grep -Fc 'REX_ENABLE_UNINITIALIZED_FIELD_TESTS=OFF' \
     <<<"$amd64_job")" -ne 2 ] ||
   [ "$(grep -Fc 'REX_ENABLE_VALGRIND=1' <<<"$amd64_job")" -ne 2 ]; then
  echo "amd64 runtime images must opt out, while both amd64 dev images retain Valgrind coverage" >&2
  exit 1
fi
if [ "$(grep -Fc 'REX_ENABLE_UNINITIALIZED_FIELD_TESTS=OFF' \
     <<<"$emulated_jobs")" -ne 12 ]; then
  echo "each Valgrind-unavailable emulated runtime and dev image must explicitly opt out" >&2
  exit 1
fi
if grep -Fq 'REX_ENABLE_UNINITIALIZED_FIELD_TESTS=OFF' "$ci_workflow" ||
   grep -Fq 'REX_ENABLE_UNINITIALIZED_FIELD_TESTS=OFF' "$memory_workflow"; then
  echo "native PR and weekly CI must never disable uninitialized-field coverage" >&2
  exit 1
fi
