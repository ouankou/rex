#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 2 ]]; then
  echo "usage: $0 <workflow-directory> <source-directory>" >&2
  exit 2
fi

workflow="$1/weekly-memory.yml"
source_directory="$2"
ci_install_deps="${source_directory}/scripts/ci-install-deps"
root_cmake="${source_directory}/CMakeLists.txt"
uninitialized_field_cmake="${source_directory}/tests/nonsmoke/functional/CompileTests/uninitializedField_tests/CMakeLists.txt"
if [[ ! -f "$workflow" ]]; then
  echo "missing weekly memory workflow: $workflow" >&2
  exit 1
fi
if [[ ! -f "$ci_install_deps" || ! -f "$root_cmake" ||
      ! -f "$uninitialized_field_cmake" ]]; then
  echo "weekly memory test-registration contract is incomplete" >&2
  exit 1
fi

native_dependencies="$(sed -n '/^  native)$/,/^    ;;$/p' "$ci_install_deps")"
if [[ "$(grep -Fxc '      valgrind' <<<"$native_dependencies")" -ne 1 ]]; then
  echo "native CI dependencies must register the complete Valgrind-aware CTest tree" >&2
  exit 1
fi
if grep -Fq 'find_path(' "$uninitialized_field_cmake" ||
   ! grep -Fq 'if(NOT ROSE_USE_VALGRIND)' \
     "$uninitialized_field_cmake" ||
   ! grep -Fq '${VALGRIND_INCLUDE_PATH}/valgrind/valgrind.h' \
     "$uninitialized_field_cmake" ||
   grep -Eq '_uninit_skip_all|Not registering uninitializedField_tests|EXCLUDE_FROM_ALL' \
     "$uninitialized_field_cmake"; then
  echo "registered uninitialized-field tests must require root-validated Valgrind support" >&2
  exit 1
fi

memcheck_discovery="$(sed -n \
  '/^set(_rose_memcheck_valgrind "")$/,/^set(ROSE_CTEST_EXPLICIT_TIMEOUT_SCALE 1)$/p' \
  "$root_cmake")"
if ! grep -Fq 'if(ROSE_VALGRIND_REQUESTED)' <<<"$memcheck_discovery" ||
   ! grep -Fq 'set(_rose_memcheck_valgrind "${VALGRIND_BINARY}")' \
     <<<"$memcheck_discovery" ||
   grep -Fq 'find_program(_rose_memcheck_valgrind' <<<"$memcheck_discovery" ||
   grep -Fq 'command -v valgrind' <<<"$memcheck_discovery"; then
  echo "Valgrind client-header discovery must not implicitly enable CTest MemCheck" >&2
  exit 1
fi

memcheck_timeout_setup="$(sed -n \
  '/^set(ROSE_CTEST_EXPLICIT_TIMEOUT_SCALE 1)$/,/^set(ROSE_CTEST_MEMCHECK_COMMAND "")$/p' \
  "$root_cmake")"
if ! grep -Fq 'if(ROSE_VALGRIND_REQUESTED)' <<<"$memcheck_timeout_setup" ||
   grep -Fq 'if(ROSE_USE_VALGRIND)' <<<"$memcheck_timeout_setup"; then
  echo "normal builds with Valgrind client headers must retain normal CTest timeouts" >&2
  exit 1
fi

if grep -R -n --include='CMakeLists.txt' --include='*.cmake' \
     -E '(^|[^[:alnum:]_])DEATH([^[:alnum:]_]|$)' \
     "$source_directory/tests"; then
  echo "CTest death labels must use the canonical lowercase spelling" >&2
  exit 1
fi

full_test_step="$(sed -n \
  '/^    - name: Run full tests$/,/^    - name: Upload full test log$/p' \
  "$workflow")"
if [[ -z "$full_test_step" ]]; then
  echo "weekly memory workflow has no exact full test step" >&2
  exit 1
fi
if ! grep -Fq 'ROSE_TEST_TIMEOUT_SCALE: "2"' <<<"$full_test_step" ||
   ! grep -Fq -- '--timeout 3000' <<<"$full_test_step"; then
  echo "weekly full tests must scale both Debug timeout layers by two" >&2
  exit 1
fi

sanitizer_step="$({
  sed -n '/^  sanitizer:/,$p' "$workflow"
} | sed -n '/^    - name: Run sanitizer on CI selection$/,/^    - name: Upload sanitizer log$/p')"

if [[ -z "$sanitizer_step" ]]; then
  echo "weekly memory workflow has no exact sanitizer test step" >&2
  exit 1
fi

if ! grep -Fq 'ROSE_TEST_TIMEOUT_SCALE: "4"' <<<"$sanitizer_step"; then
  echo "sanitizer tests must scale the harness timeout for instrumented Debug execution" >&2
  exit 1
fi

if ! grep -Fq 'LSAN_OPTIONS: suppressions=${{ github.workspace }}/scripts/rex-suppressions-for-lsan' <<<"$sanitizer_step"; then
  echo "sanitizer tests lost the checked-in LeakSanitizer configuration" >&2
  exit 1
fi

if ! grep -Fq 'ctest --test-dir build --no-tests=error -R "astInterface|testQuery|rex" -LE death' <<<"$sanitizer_step"; then
  echo "sanitizer tests no longer run the exact scheduled CI selection" >&2
  exit 1
fi

if ! grep -Fq -- '--timeout 6000' <<<"$sanitizer_step"; then
  echo "sanitizer CTest must apply the fourfold instrumented-test timeout to direct executables" >&2
  exit 1
fi

memcheck_job="$({
  sed -n '/^  memcheck:/,/^  sanitizer:/p' "$workflow"
})"
if [[ -z "$memcheck_job" ]]; then
  echo "weekly memory workflow has no exact MemCheck job" >&2
  exit 1
fi

if ! grep -Fq 'shard: [1, 2, 3, 4, 5, 6, 7, 8]' <<<"$memcheck_job" ||
   ! grep -Fq 'python3 scripts/run_ctest_name_set.py' <<<"$memcheck_job" ||
   ! grep -Fq -- '--shard-index "${{ matrix.shard }}"' <<<"$memcheck_job" ||
   ! grep -Fq -- '--shard-count 8' <<<"$memcheck_job"; then
  echo "MemCheck must partition the complete filtered CTest registry into eight dependency-closed shards" >&2
  exit 1
fi
if ! grep -Fq -- '--include-regex "astInterface|testQuery|rex|Cxx_tests_test_multiple_files_2"' <<<"$memcheck_job" ||
   ! grep -Fq -- '--exclude-label death' <<<"$memcheck_job"; then
  echo "MemCheck shards no longer retain the complete scheduled selection" >&2
  exit 1
fi
if grep -Eq -- '-I[[:space:]]+"?\$\{\{[[:space:]]*matrix\.shard' <<<"$memcheck_job"; then
  echo "MemCheck must not use registration-order-dependent CTest index shards" >&2
  exit 1
fi
if ! grep -Fq -- '--memcheck' <<<"$memcheck_job"; then
  echo "MemCheck shards no longer use CTest's MemCheck dashboard step" >&2
  exit 1
fi
if ! grep -Fq -- '-DREX_EXTENDED_TEST_TIMEOUTS=ON' <<<"$memcheck_job" ||
   ! grep -Fq 'ROSE_TEST_TIMEOUT_SCALE: "24"' <<<"$memcheck_job"; then
  echo "MemCheck lost its instrumented-test scheduling and timeout contract" >&2
  exit 1
fi
if ! grep -Fq 'REX_MEMCHECK_CTEST_MAX_JOBS: 8' <<<"$memcheck_job" ||
   ! grep -Fq 'if (( BUILD_JOBS > REX_MEMCHECK_CTEST_MAX_JOBS )); then' <<<"$memcheck_job" ||
   ! grep -Fq 'cmake --build build -j"${BUILD_JOBS}"' <<<"$memcheck_job" ||
   ! grep -Fq 'if (( CTEST_JOBS > REX_MEMCHECK_CTEST_MAX_JOBS )); then' <<<"$memcheck_job" ||
   ! grep -Fq -- '--jobs "${CTEST_JOBS}"' <<<"$memcheck_job"; then
  echo "MemCheck must cap each matrix shard to eight instrumented workers" >&2
  exit 1
fi
if ! grep -Fq -- '--timeout 19800' <<<"$memcheck_job"; then
  echo "MemCheck lost the measured local long-test timeout boundary" >&2
  exit 1
fi
if [[ "$(grep -Fc 'coreutils-from-gnu' <<<"$memcheck_job")" -ne 1 ]] ||
   [[ "$(grep -Fc 'coreutils-from-uutils-' <<<"$memcheck_job")" -ne 1 ]] ||
   ! grep -Fq -- '--allow-remove-essential' <<<"$memcheck_job" ||
   ! grep -Fq '/usr/bin/env --version' <<<"$memcheck_job" ||
   ! grep -Fq 'GNU coreutils' <<<"$memcheck_job"; then
  echo "MemCheck must select and verify the standalone GNU env interpreter" >&2
  exit 1
fi
if ! grep -Fq 'name: memcheck-logs-${{ matrix.shard }}-of-8' <<<"$memcheck_job"; then
  echo "MemCheck shard artifacts do not have collision-free names" >&2
  exit 1
fi

memcheck_ignore="${source_directory}/cmake/CTestCustom.cmake.in"
cxx_tests_cmake="${source_directory}/tests/nonsmoke/functional/CompileTests/Cxx_tests/CMakeLists.txt"
cxx_transformation_specimens="${source_directory}/tests/nonsmoke/functional/CompileTests/Cxx_tests/Cxx_transformation_specimens.cmake"
single_statement_cmake="${source_directory}/tests/nonsmoke/functional/roseTests/programTransformationTests/singleStatementToBlockNormalization/CMakeLists.txt"
bounded_lazy_source="${source_directory}/tests/nonsmoke/functional/CompileTests/Cxx_tests/rex_frontend_lazy_bounded_system_header_fields.cpp"
bounded_lazy_header="${source_directory}/tests/nonsmoke/functional/CompileTests/Cxx_tests/rex_frontend_lazy_bounded_system_header_fields.hpp"
template_argument_source="${source_directory}/tests/nonsmoke/functional/CompileTests/Cxx14_tests/rex_test2026_template_argument_marking_valgrind_defined_reads.cpp"
ast_interface_cmake="${source_directory}/tests/nonsmoke/functional/roseTests/astInterfaceTests/CMakeLists.txt"
ast_interface_regex_input="${source_directory}/tests/nonsmoke/functional/roseTests/astInterfaceTests/inputrex_test2026_stl_regex.C"
ast_interface_bounded_input="${source_directory}/tests/nonsmoke/functional/roseTests/astInterfaceTests/rex_ast_interface_template_instantiation_memcheck.cpp"
interface_coverage_bounded_input="${source_directory}/tests/nonsmoke/functional/roseTests/astInterfaceTests/rex_ast_interface_function_coverage_memcheck.cpp"
move_decl_cmake="${source_directory}/tests/nonsmoke/functional/moveDeclarationTool/CMakeLists.txt"
move_decl_bounded_input="${source_directory}/tests/nonsmoke/functional/moveDeclarationTool/rex_move_declaration_boundary_ownership.C"
move_decl_bounded_runner="${source_directory}/tests/nonsmoke/functional/moveDeclarationTool/run_rex_move_declaration_boundary_ownership.sh"

for path in "$memcheck_ignore" "$cxx_tests_cmake" \
  "$cxx_transformation_specimens" "$single_statement_cmake" \
  "$bounded_lazy_source" \
  "$bounded_lazy_header" "$template_argument_source" \
  "$ast_interface_cmake" "$ast_interface_regex_input" \
  "$ast_interface_bounded_input" "$interface_coverage_bounded_input" \
  "$move_decl_cmake" "$move_decl_bounded_input" \
  "$move_decl_bounded_runner"; do
  if [[ ! -f "$path" ]]; then
    echo "weekly MemCheck bounded-input contract is missing: $path" >&2
    exit 1
  fi
done

for test_name in \
  Cxx_tests_rex_frontend_lazy_system_header_fields_cpp \
  Cxx_tests_rex_frontend_lazy_system_header_fields_unparse \
  uninit_fields_cxx_Cxx_tests_rex_frontend_lazy_system_header_fields_cpp \
  compiler_options_collect_comments_Cxx_tests_rex_frontend_lazy_system_header_fields_cpp \
  Cxx_tests_rex_test2026_stl_map_cpp \
  uninit_fields_cxx_Cxx_tests_rex_test2026_stl_map_cpp \
  normalizationTranslator_rex_test2026_stl_map.cpp \
  singleStatementToBlockNormalization_rex_test2026_stl_map.cpp \
  compiler_options_collect_comments_Cxx_tests_rex_test2026_stl_map_cpp \
  Cxx_tests_rex_test2026_stl_regex_cpp \
  uninit_fields_cxx_Cxx_tests_rex_test2026_stl_regex_cpp \
  normalizationTranslator_rex_test2026_stl_regex.cpp \
  singleStatementToBlockNormalization_rex_test2026_stl_regex.cpp \
  compiler_options_collect_comments_Cxx_tests_rex_test2026_stl_regex_cpp \
  astInterface_rex_test2026_stl_regex \
  astInterface_interfaceFunctionCoverage \
  moveDecl_rex_anonymous_tag_identity_inputmoveDeclarationToInnermostScope_test2014_18_C \
  moveDecl_rex_anonymous_tag_identity_inputmoveDeclarationToInnermostScope_test2014_22_C; do
  if [[ "$(grep -Fxc "  ${test_name}" "$memcheck_ignore")" -ne 1 ]]; then
    echo "MemCheck heavy-input boundary is not exact: $test_name" >&2
    exit 1
  fi
done

for replacement_name in \
  Cxx_tests_rex_frontend_lazy_bounded_system_header_fields_cpp \
  uninit_fields_cxx_Cxx_tests_rex_frontend_lazy_bounded_system_header_fields_cpp \
  compiler_options_collect_comments_Cxx_tests_rex_frontend_lazy_bounded_system_header_fields_cpp \
  normalizationTranslator_rex_test2026_for_init_scope.cpp \
  rex_single_statement_normalization_memcheck \
  astInterface_rex_interface_function_coverage_memcheck \
  moveDecl_rex_move_declaration_boundary_ownership; do
  if grep -Fqx "  ${replacement_name}" "$memcheck_ignore"; then
    echo "bounded MemCheck replacement was excluded: $replacement_name" >&2
    exit 1
  fi
done

if [[ "$(grep -Fxc '  rex_frontend_lazy_bounded_system_header_fields.cpp' \
     "$cxx_tests_cmake")" -ne 1 ]] ||
   [[ "$(grep -Fxc '  rex_frontend_lazy_system_header_fields.cpp' \
     "$cxx_tests_cmake")" -ne 1 ]] ||
   [[ "$(grep -Fxc '  rex_test2026_stl_regex.cpp' \
     "$cxx_tests_cmake")" -ne 1 ]] ||
   [[ "$(grep -Fxc '  rex_test2026_stl_map.cpp' \
     "$cxx_tests_cmake")" -ne 1 ]] ||
   [[ "$(grep -Fc '"rex_frontend_lazy_bounded_system_header_fields"' \
     "$cxx_tests_cmake")" -ne 1 ]] ||
   ! grep -Fq '#pragma GCC system_header' "$bounded_lazy_header"; then
  echo "bounded lazy-system-header MemCheck coverage is not registered exactly" >&2
  exit 1
fi

if grep -Eq '^[[:space:]]*#[[:space:]]*include[[:space:]]*<' \
     "$interface_coverage_bounded_input" ||
   [[ "$(grep -Fc 'NAME astInterface_rex_interface_function_coverage_memcheck' \
     "$ast_interface_cmake")" -ne 1 ]] ||
   [[ "$(grep -Fc 'rex_ast_interface_function_coverage_memcheck.cpp' \
     "$ast_interface_cmake")" -ne 1 ]] ||
   ! grep -Fq '$<TARGET_FILE:interfaceFunctionCoverage>' "$ast_interface_cmake"; then
  echo "bounded SageInterface coverage executable is not registered exactly" >&2
  exit 1
fi

if [[ "$(grep -Fc 'NAME moveDecl_rex_move_declaration_boundary_ownership' \
     "$move_decl_cmake")" -ne 1 ]] ||
   ! grep -Fq 'union {' "$move_decl_bounded_input" ||
   ! grep -Fq "REX_MOVE_DECLARATION_INVARIANT[anonymous-tag]" \
     "$move_decl_bounded_runner"; then
  echo "bounded move-declaration anonymous-tag contract is not registered exactly" >&2
  exit 1
fi

if [[ "$(grep -Fxc '  rex_test2026_for_init_scope.cpp' \
     "$cxx_transformation_specimens")" -ne 1 ]] ||
   [[ "$(grep -Fc 'NAME rex_single_statement_normalization_memcheck' \
     "$single_statement_cmake")" -ne 1 ]]; then
  echo "bounded normalization replacements are not registered exactly" >&2
  exit 1
fi

for required_source_line in \
  '#include <algorithm>' \
  '#include <iostream>' \
  '#include <string>' \
  '#include <unordered_map>' \
  '#include <vector>' \
  'std::vector<int> values;' \
  'std::unordered_map<std::wstring, std::vector<std::string>>'; do
  if ! grep -Fq "$required_source_line" "$template_argument_source"; then
    echo "Valgrind template-argument contract lost real standard-library coverage: $required_source_line" >&2
    exit 1
  fi
done
if grep -Fq 'Cxx14_tests_rex_test2026_template_argument_marking_valgrind_defined_reads_cpp' \
     "$memcheck_ignore"; then
  echo "standard-library frontend template-argument contract must remain instrumented" >&2
  exit 1
fi
if ! grep -Fq '#include <regex>' "$ast_interface_regex_input" ||
   [[ "$(grep -Fc 'NAME astInterface_rex_test2026_stl_regex' \
     "$ast_interface_cmake")" -ne 1 ]]; then
  echo "full AST-interface regex integration coverage was removed" >&2
  exit 1
fi
if grep -Eq '^[[:space:]]*#[[:space:]]*include[[:space:]]*<' \
     "$ast_interface_bounded_input" ||
   ! grep -Fq 'struct basic_regex' "$ast_interface_bounded_input" ||
   [[ "$(grep -Fc 'NAME astInterface_rex_template_instantiation_memcheck' \
     "$ast_interface_cmake")" -ne 1 ]] ||
   [[ "$(grep -Fc 'rex_ast_interface_template_instantiation_memcheck.cpp' \
     "$ast_interface_cmake")" -ne 1 ]]; then
  echo "bounded AST-interface template query is not registered exactly" >&2
  exit 1
fi
