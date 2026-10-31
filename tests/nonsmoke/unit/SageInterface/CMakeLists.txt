include_directories(${ROSE_INCLUDES} ${CMAKE_CURRENT_BINARY_DIR})

add_executable(rex_defuse_multiple_predecessors
  rex_defuse_multiple_predecessors.cpp)
target_link_libraries(rex_defuse_multiple_predecessors PRIVATE ROSE_DLL)
add_test(NAME rex_defuse_multiple_predecessors
  COMMAND rex_defuse_multiple_predecessors
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_defuse_multiple_predecessors_input.cpp)
set_tests_properties(rex_defuse_multiple_predecessors
  PROPERTIES LABELS "rex;analysis;sanitizer")

add_executable(rex_liveness_loop_fixed_point rex_liveness_loop_fixed_point.cpp)
target_link_libraries(rex_liveness_loop_fixed_point PRIVATE ROSE_DLL)
add_test(NAME rex_liveness_loop_fixed_point
  COMMAND rex_liveness_loop_fixed_point
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_liveness_loop_fixed_point_input.cpp)
set_tests_properties(rex_liveness_loop_fixed_point
  PROPERTIES LABELS "rex;analysis;sanitizer")

add_executable(rex_graph_directed_edge_removal
  rex_graph_directed_edge_removal.cpp)
target_link_libraries(rex_graph_directed_edge_removal PRIVATE ROSE_DLL)
add_test(NAME rex_graph_directed_edge_removal
  COMMAND rex_graph_directed_edge_removal)
set_tests_properties(rex_graph_directed_edge_removal
  PROPERTIES LABELS "rex;graph;sanitizer")

find_program(BASH_EXECUTABLE bash)
if(NOT BASH_EXECUTABLE)
  message(FATAL_ERROR "bash not found; SageInterface death tests require bash")
endif()
find_program(REX_TEST_PYTHON_EXECUTABLE NAMES python3 REQUIRED)

add_executable(rex_literal_typed_mutation rex_literal_typed_mutation.cpp)
target_link_libraries(rex_literal_typed_mutation PRIVATE ROSE_DLL)
add_test(NAME rex_literal_typed_mutation COMMAND rex_literal_typed_mutation)
set_tests_properties(rex_literal_typed_mutation
  PROPERTIES LABELS "rex;AST;frontend;sanitizer")

add_executable(rex_complex_literal_semantics rex_complex_literal_semantics.cpp)
target_link_libraries(rex_complex_literal_semantics PRIVATE ROSE_DLL)
add_test(NAME rex_complex_literal_runtime
  COMMAND ${REX_TEST_PYTHON_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_complex_literal_runtime.py
    --translator $<TARGET_FILE:rex_complex_literal_semantics>
    --cc ${CMAKE_C_COMPILER} --cxx ${CMAKE_CXX_COMPILER})
set_tests_properties(rex_complex_literal_runtime
  PROPERTIES LABELS "rex;AST;frontend;sanitizer")

add_executable(rex_extended_floating_semantics rex_extended_floating_semantics.cpp)
target_link_libraries(rex_extended_floating_semantics PRIVATE ROSE_DLL)
# The runtime specimen uses __float128, a target extension that AArch64 Clang
# rejects.  Run it only where the configured C compiler provides the type.
include(CheckCSourceCompiles)
check_c_source_compiles(
  "__float128 rex_quad = 1.25Q; int main(void) { return rex_quad > 0 ? 0 : 1; }"
  REX_UNIT_TESTS_HAS_FLOAT128)
if(REX_UNIT_TESTS_HAS_FLOAT128)
  add_test(NAME rex_extended_floating_runtime
    COMMAND ${REX_TEST_PYTHON_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/rex_extended_floating_runtime.py
      --translator $<TARGET_FILE:rex_extended_floating_semantics>
      --cc ${CMAKE_C_COMPILER} --cxx ${CMAKE_CXX_COMPILER})
  set_tests_properties(rex_extended_floating_runtime
    PROPERTIES LABELS "rex;AST;frontend;sanitizer")
endif()

add_executable(rex_integer_wide_semantics rex_integer_wide_semantics.cpp)
target_link_libraries(rex_integer_wide_semantics PRIVATE ROSE_DLL)
add_test(NAME rex_integer_wide_runtime
  COMMAND ${REX_TEST_PYTHON_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_integer_wide_runtime.py
    $<TARGET_FILE:rex_integer_wide_semantics> ${CMAKE_CXX_COMPILER})
set_tests_properties(rex_integer_wide_runtime
  PROPERTIES LABELS "rex;AST;frontend;sanitizer")

add_executable(rex_atomic_type_semantics rex_atomic_type_semantics.cpp)
target_link_libraries(rex_atomic_type_semantics PRIVATE ROSE_DLL)
add_test(NAME rex_atomic_type_runtime
  COMMAND ${REX_TEST_PYTHON_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_atomic_type_runtime.py
    $<TARGET_FILE:rex_atomic_type_semantics>
    ${ROSE_LLVM_CLANG_C_DRIVER} ${ROSE_LLVM_CLANG_CXX_DRIVER})
set_tests_properties(rex_atomic_type_runtime
  PROPERTIES LABELS "rex;AST;frontend;sanitizer")

add_executable(rex_vector_type_semantics rex_vector_type_semantics.cpp)
target_link_libraries(rex_vector_type_semantics PRIVATE ROSE_DLL)
add_executable(rex_type_modifier_identity rex_type_modifier_identity.cpp)
target_link_libraries(rex_type_modifier_identity PRIVATE ROSE_DLL gtest_main)
add_test(NAME rex_type_modifier_identity COMMAND rex_type_modifier_identity)
set_tests_properties(rex_type_modifier_identity PROPERTIES LABELS "rex;typed")
add_test(NAME rex_vector_type_runtime
  COMMAND ${REX_TEST_PYTHON_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_vector_type_runtime.py
    $<TARGET_FILE:rex_vector_type_semantics>
    ${ROSE_LLVM_CLANG_C_DRIVER} ${ROSE_LLVM_CLANG_CXX_DRIVER})
set_tests_properties(rex_vector_type_runtime
  PROPERTIES LABELS "rex;AST;frontend;sanitizer")

add_executable(rex_array_extent_semantics rex_array_extent_semantics.cpp)
target_link_libraries(rex_array_extent_semantics PRIVATE ROSE_DLL)
add_test(NAME rex_array_extent_runtime
  COMMAND ${REX_TEST_PYTHON_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_array_extent_runtime.py
    $<TARGET_FILE:rex_array_extent_semantics>
    ${CMAKE_C_COMPILER} ${CMAKE_CXX_COMPILER})
set_tests_properties(rex_array_extent_runtime
  PROPERTIES LABELS "rex;AST;frontend;sanitizer")

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(_rex_linker_fortran)
  if(ENABLE-FORTRAN)
    list(APPEND _rex_linker_fortran --fc ${CMAKE_Fortran_COMPILER})
  endif()
  add_test(NAME rex_cmake_linker_policy
    COMMAND ${REX_TEST_PYTHON_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/rex_cmake_linker_policy.py
      --cmake ${CMAKE_COMMAND}
      --module ${ROSE_TOP_SRC_DIR}/cmake/rex_linker.cmake
      --cc ${CMAKE_C_COMPILER} --cxx ${CMAKE_CXX_COMPILER}
      ${_rex_linker_fortran})
  set_tests_properties(rex_cmake_linker_policy
    PROPERTIES LABELS "rex;build-system;CI")
  unset(_rex_linker_fortran)
endif()

add_test(NAME rex_cmake_pch_languages
  COMMAND ${REX_TEST_PYTHON_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_cmake_pch_languages.py
    ${ROSE_TOP_SRC_DIR}/cmake/rex_pch.cmake
    ${CMAKE_COMMAND} ${CMAKE_C_COMPILER} ${CMAKE_CXX_COMPILER})
set_tests_properties(rex_cmake_pch_languages
  PROPERTIES LABELS "rex;build-system")

list(LENGTH BACKEND_C_COMPILER_COMMAND _rex_c_compiler_argc)
add_test(
  NAME rex_cmake_backend_metadata_contract
  COMMAND ${REX_TEST_PYTHON_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_cmake_backend_metadata_contract.py
    ${ROSE_TOP_SRC_DIR}/cmake/modules/roseChooseBackendCompiler.cmake
    ${CMAKE_COMMAND} ${_rex_c_compiler_argc}
    ${BACKEND_C_COMPILER_COMMAND} ${BACKEND_CXX_COMPILER_COMMAND})
unset(_rex_c_compiler_argc)
set_tests_properties(rex_cmake_backend_metadata_contract
  PROPERTIES LABELS "rex;build-system")

add_test(NAME rex_cmake_bison_prefix_contract
  COMMAND ${REX_TEST_PYTHON_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_cmake_bison_prefix_contract.py
    ${ROSE_TOP_SRC_DIR}/cmake/rex_bison.cmake
    ${CMAKE_COMMAND} ${BISON_EXECUTABLE})
set_tests_properties(rex_cmake_bison_prefix_contract
  PROPERTIES LABELS "rex;build-system")

add_test(NAME rex_cmake_zlib_prefix_contract
  COMMAND ${REX_TEST_PYTHON_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_cmake_zlib_prefix_contract.py
    ${ROSE_TOP_SRC_DIR}/cmake/rex_zlib.cmake
    ${CMAKE_COMMAND} ${CMAKE_C_COMPILER} ${ZLIB_INCLUDE_DIR}
    $<TARGET_FILE:ZLIB::ZLIB>
    ${CMAKE_SHARED_LIBRARY_PREFIX}z${CMAKE_SHARED_LIBRARY_SUFFIX})
set_tests_properties(rex_cmake_zlib_prefix_contract
  PROPERTIES LABELS "rex;build-system")

add_test(NAME rex_cmake_llvm_package_version_contract
  COMMAND ${REX_TEST_PYTHON_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_cmake_llvm_package_version_contract.py
    ${ROSE_TOP_SRC_DIR}/cmake/modules/roseReadLlvmPackageVersion.cmake
    ${CMAKE_COMMAND})
set_tests_properties(rex_cmake_llvm_package_version_contract
  PROPERTIES LABELS "rex;build-system")

add_test(NAME rex_cmake_llvm_driver_contract
  COMMAND ${REX_TEST_PYTHON_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_cmake_llvm_driver_contract.py
    ${ROSE_TOP_SRC_DIR}/cmake/rex_llvm_driver.cmake
    ${CMAKE_COMMAND})
set_tests_properties(rex_cmake_llvm_driver_contract
  PROPERTIES LABELS "rex;build-system")

add_test(NAME rex_cmake_imported_shared_library
  COMMAND ${REX_TEST_PYTHON_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_cmake_imported_shared_library.py
    ${ROSE_TOP_SRC_DIR}/cmake/rex_imported_shared_library.cmake
    ${CMAKE_COMMAND} ${CMAKE_CXX_COMPILER}
    $<TARGET_FILE:rose-config> $<TARGET_FILE:ROSE_DLL>
    $<TARGET_SONAME_FILE_NAME:ROSE_DLL>)
set_tests_properties(rex_cmake_imported_shared_library
  PROPERTIES LABELS "rex;build-system")

add_test(NAME rex_clang_graph_headers
  COMMAND ${REX_TEST_PYTHON_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_clang_graph_headers.py
    $<TARGET_FILE:identityTranslator> ${CMAKE_CURRENT_SOURCE_DIR})
set_tests_properties(rex_clang_graph_headers
  PROPERTIES LABELS "rex;AST;sanitizer")

if(ENABLE-C)
  set(_rex_backend_runtime_flags)
  if(ENABLE-SANITIZER)
    list(JOIN ROSE_SANITIZERS "," _rex_backend_runtime_sanitizers)
    list(APPEND _rex_backend_runtime_flags
      -fsanitize=${_rex_backend_runtime_sanitizers} -fno-omit-frame-pointer)
  endif()
  add_test(NAME rex_backend_output_directory
    COMMAND ${REX_TEST_PYTHON_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/rex_backend_output_directory.py
      $<TARGET_FILE:identityTranslator> ${_rex_backend_runtime_flags})
  set_tests_properties(rex_backend_output_directory
    PROPERTIES LABELS "rex;backend;runtime;sanitizer")
  add_test(NAME rex_backend_literal_arguments
    COMMAND ${REX_TEST_PYTHON_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/rex_backend_literal_arguments.py
      $<TARGET_FILE:identityTranslator>
      ${CMAKE_C_COMPILER} ${CMAKE_CXX_COMPILER})
  set_tests_properties(rex_backend_literal_arguments
    PROPERTIES LABELS "rex;backend;sanitizer")
endif()

add_test(NAME rex_fortran_rejection_status
  COMMAND ${REX_TEST_PYTHON_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_fortran_rejection_status.py
    ${ROSE_TOP_SRC_DIR}/tests/nonsmoke/functional/CompileTests/Fortran_tests/experimental_frontend_tests/rex_fortran_rejection_contract.py
    ${ROSE_TOP_SRC_DIR}/tests/nonsmoke/functional/CompileTests/Fortran_tests/experimental_frontend_tests/R1164-F2018.f90
    ${CMAKE_C_COMPILER})
set_tests_properties(rex_fortran_rejection_status
  PROPERTIES LABELS "rex;Fortran;CI")

if(ENABLE-FORTRAN)
  add_test(NAME rex_fortran_interface_checker
    COMMAND ${REX_TEST_PYTHON_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/rex_fortran_interface_checker.py
      $<TARGET_FILE:tools_checkFortranInterfaces_exe>)
  set_tests_properties(rex_fortran_interface_checker
    PROPERTIES LABELS "rex;Fortran;AST;tools")
endif()

if(ENABLE-TUTORIAL-DIRECTORY)
  add_test(NAME rex_tutorial_change_function
    COMMAND ${REX_TEST_PYTHON_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/rex_tutorial_change_function.py
      $<TARGET_FILE:changeFunctionTutorial> ${_rex_backend_runtime_flags})
  set_tests_properties(rex_tutorial_change_function
    PROPERTIES LABELS "rex;AST;tutorial;runtime;sanitizer")
  add_test(NAME rex_tutorial_inline_runtime
    COMMAND ${REX_TEST_PYTHON_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/rex_tutorial_inline_runtime.py
      $<TARGET_FILE:inlineTransformationsTutorial> ${CMAKE_CXX_COMPILER})
  set_tests_properties(rex_tutorial_inline_runtime
    PROPERTIES LABELS "rex;AST;tutorial;runtime")
  add_test(NAME rex_tutorial_access_runtime
    COMMAND ${REX_TEST_PYTHON_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/rex_tutorial_access_runtime.py
      $<TARGET_FILE:interveneAtVariablesTutorial>
      ${CMAKE_C_COMPILER} ${CMAKE_CXX_COMPILER})
  set_tests_properties(rex_tutorial_access_runtime
    PROPERTIES LABELS "rex;AST;tutorial;runtime")
  add_test(NAME rex_tutorial_function_construction
    COMMAND ${REX_TEST_PYTHON_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/rex_tutorial_function_construction.py
      ${CMAKE_CXX_COMPILER}
      $<TARGET_FILE:addFunctionDeclarationTutorial>
      $<TARGET_FILE:addFunctionDeclaration2Tutorial>
      $<TARGET_FILE:addFunctionDeclaration3Tutorial>)
  set_tests_properties(rex_tutorial_function_construction
    PROPERTIES LABELS "rex;AST;tutorial;runtime")
endif()

add_executable(rex_type_interning_lifetime rex_type_interning_lifetime.cpp)
target_link_libraries(rex_type_interning_lifetime ROSE_DLL)
add_test(NAME rex_type_interning_lifetime
  COMMAND rex_type_interning_lifetime
    ${ROSE_TOP_SRC_DIR}/tests/nonsmoke/functional/CompileTests/Cxx_tests/rex_frontend_dependent_alias_projection.cpp)
set_tests_properties(rex_type_interning_lifetime
  PROPERTIES LABELS "rex;AST;lifetime;sanitizer")

add_executable(rex_function_type_identity_contract rex_function_type_identity_contract.cpp)
add_executable(rex_ast_deletion_ownership rex_ast_deletion_ownership.cpp)
target_link_libraries(rex_ast_deletion_ownership ROSE_DLL)
add_test(NAME rex_ast_deletion_ownership
  COMMAND rex_ast_deletion_ownership
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_ast_deletion_ownership_input.c)
set_tests_properties(rex_ast_deletion_ownership
  PROPERTIES LABELS "rex;AST;lifetime;sanitizer")
add_test(NAME rex_ast_deletion_orphan
  COMMAND ${REX_TEST_PYTHON_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_ast_deletion_orphan.py
    $<TARGET_FILE:rex_ast_deletion_ownership>
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_ast_deletion_ownership_input.c)
set_tests_properties(rex_ast_deletion_orphan
  PROPERTIES LABELS "rex;AST;lifetime;death")
add_executable(rex_preprocessing_destruction rex_preprocessing_destruction.cpp)
target_link_libraries(rex_preprocessing_destruction ROSE_DLL)
add_test(NAME rex_preprocessing_destruction COMMAND rex_preprocessing_destruction)
set_tests_properties(rex_preprocessing_destruction
  PROPERTIES LABELS "rex;AST;copy;lifetime;sanitizer")
target_link_libraries(rex_function_type_identity_contract ROSE_DLL)
add_test(NAME rex_function_type_identity_contract COMMAND rex_function_type_identity_contract)
set_tests_properties(rex_function_type_identity_contract
  PROPERTIES LABELS "rex;AST;sanitizer")

add_executable(rex_outliner_symbol_identity rex_outliner_symbol_identity.cpp)
target_link_libraries(rex_outliner_symbol_identity ROSE_DLL)
add_test(NAME rex_outliner_symbol_identity
  COMMAND rex_outliner_symbol_identity -std=c++17 -fms-extensions
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_outliner_symbol_identity_input.cpp)
set_tests_properties(rex_outliner_symbol_identity
  PROPERTIES LABELS "rex;AST;outliner;sanitizer")
add_test(NAME rex_outliner_missing_symbol
  COMMAND ${REX_TEST_PYTHON_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_outliner_missing_symbol.py
    $<TARGET_FILE:rex_outliner_symbol_identity>)
set_tests_properties(rex_outliner_missing_symbol
  PROPERTIES LABELS "rex;AST;outliner;death")

add_executable(rex_filesystem_contract rex_filesystem_contract.cpp)
target_link_libraries(rex_filesystem_contract ROSE_DLL)
add_test(NAME rex_filesystem_contract COMMAND rex_filesystem_contract)
set_tests_properties(rex_filesystem_contract
  PROPERTIES LABELS "rex;filesystem;sanitizer")

add_executable(rex_process_exit_status rex_process_exit_status.cpp)
target_link_libraries(rex_process_exit_status ROSE_DLL)
add_test(NAME rex_process_exit_status COMMAND rex_process_exit_status)
set_tests_properties(rex_process_exit_status
  PROPERTIES LABELS "rex;process;sanitizer")

add_executable(rex_backend_frontend_failure rex_backend_frontend_failure.cpp)
target_link_libraries(rex_backend_frontend_failure ROSE_DLL)
add_test(NAME rex_backend_frontend_failure COMMAND rex_backend_frontend_failure)
set_tests_properties(rex_backend_frontend_failure
  PROPERTIES LABELS "rex;backend;sanitizer")

add_test(NAME rex_rosetta_incremental_output
  COMMAND ${REX_TEST_PYTHON_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_rosetta_incremental_output.py
    $<TARGET_FILE:CxxGrammarMetaProgram> ${ROSE_TOP_SRC_DIR}/src/ROSETTA/Grammar)
set_tests_properties(rex_rosetta_incremental_output
  PROPERTIES LABELS "rex;ROSETTA;build-system;sanitizer")
add_test(NAME rex_rosetta_fragment_contract
  COMMAND ${REX_TEST_PYTHON_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_rosetta_fragment_contract.py
    $<TARGET_FILE:CxxGrammarMetaProgram> ${ROSE_TOP_SRC_DIR}/src/ROSETTA/Grammar)
set_tests_properties(rex_rosetta_fragment_contract
  PROPERTIES LABELS "rex;ROSETTA;build-system;sanitizer")

set(_rex_pool_contract_flags)
if(ENABLE-SANITIZER)
  list(JOIN ROSE_SANITIZERS "," _rex_pool_contract_sanitizers)
  list(APPEND _rex_pool_contract_flags
    "-fsanitize=${_rex_pool_contract_sanitizers}" -fno-omit-frame-pointer)
endif()
add_test(NAME rex_rosetta_pool_fill
  COMMAND ${REX_TEST_PYTHON_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_rosetta_pool_fill.py
    ${CMAKE_CXX_COMPILER}
    ${ROSE_TOP_SRC_DIR}/src/ROSETTA/Grammar/grammarNewDeleteOperatorMacros.macro
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_rosetta_pool_fill.cpp.in
    ${_rex_pool_contract_flags})
set_tests_properties(rex_rosetta_pool_fill
  PROPERTIES LABELS "rex;ROSETTA;sanitizer")
unset(_rex_pool_contract_flags)
unset(_rex_pool_contract_sanitizers)

if(ENABLE-FORTRAN)
  add_executable(rex_flang_imported_components rex_flang_imported_components.cpp)
  target_link_libraries(rex_flang_imported_components ROSE_DLL)
  add_test(NAME rex_flang_include_paths
    COMMAND ${REX_TEST_PYTHON_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/rex_flang_include_paths.py
      $<TARGET_FILE:rex_flang_imported_components> ${CMAKE_Fortran_COMPILER}
      ${CMAKE_CURRENT_SOURCE_DIR})
  set_tests_properties(rex_flang_include_paths
    PROPERTIES LABELS "rex;AST;Fortran;sanitizer")
  add_test(NAME rex_ast_json_shared_modules
    COMMAND ${REX_TEST_PYTHON_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/rex_ast_json_shared_modules.py
      $<TARGET_FILE:identityTranslator> ${CMAKE_Fortran_COMPILER}
      ${CMAKE_CURRENT_SOURCE_DIR})
  set_tests_properties(rex_ast_json_shared_modules
    PROPERTIES LABELS "rex;AST;ASTJSON;Fortran;sanitizer")
  add_test(NAME rex_backend_mixed_languages
    COMMAND ${REX_TEST_PYTHON_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/rex_backend_mixed_languages.py
      $<TARGET_FILE:identityTranslator> ${CMAKE_COMMAND}
      ${CMAKE_C_COMPILER} ${CMAKE_CXX_COMPILER} ${CMAKE_Fortran_COMPILER})
  set_tests_properties(rex_backend_mixed_languages
    PROPERTIES LABELS "rex;ASTJSON;backend;Fortran;sanitizer")
  add_test(NAME rex_flang_prescan_source
    COMMAND ${REX_TEST_PYTHON_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/rex_flang_prescan_source.py
      $<TARGET_FILE:identityTranslator> ${CMAKE_Fortran_COMPILER}
      ${CMAKE_CURRENT_SOURCE_DIR})
  set_tests_properties(rex_flang_prescan_source
    PROPERTIES LABELS "rex;AST;Fortran;sanitizer")
endif()

add_executable(rex_ast_statement_numbering rex_ast_statement_numbering.cpp)
target_link_libraries(rex_ast_statement_numbering ROSE_DLL)
add_test(
  NAME rex_ast_statement_numbering
  COMMAND rex_ast_statement_numbering
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_ast_statement_numbering_input.cpp)
set_tests_properties(rex_ast_statement_numbering
  PROPERTIES LABELS "rex;AST;sanitizer")

add_executable(rex_template_parameter_spelling
  rex_template_parameter_spelling.cpp)
target_link_libraries(rex_template_parameter_spelling ROSE_DLL)
add_test(
  NAME rex_template_parameter_spelling
  COMMAND ${REX_TEST_PYTHON_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_template_parameter_spelling_runtime.py
    $<TARGET_FILE:rex_template_parameter_spelling>
    ${CMAKE_CXX_COMPILER}
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_template_parameter_spelling_input.cpp)
set_tests_properties(rex_template_parameter_spelling
  PROPERTIES LABELS "rex;AST;sanitizer")

add_executable(rex_member_address_type rex_member_address_type.cpp)
target_link_libraries(rex_member_address_type ROSE_DLL)
add_test(NAME rex_member_address_type
  COMMAND ${REX_TEST_PYTHON_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_template_parameter_spelling_runtime.py
    $<TARGET_FILE:rex_member_address_type> ${CMAKE_CXX_COMPILER}
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_member_address_type_input.cpp c++17)
set_tests_properties(rex_member_address_type
  PROPERTIES LABELS "rex;AST;sanitizer")

add_executable(rex_constructor_semantic_name rex_constructor_semantic_name.cpp)
target_link_libraries(rex_constructor_semantic_name ROSE_DLL)
add_test(NAME rex_constructor_semantic_name
  COMMAND ${REX_TEST_PYTHON_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_template_parameter_spelling_runtime.py
    $<TARGET_FILE:rex_constructor_semantic_name> ${CMAKE_CXX_COMPILER}
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_constructor_semantic_name_input.cpp c++17)
set_tests_properties(rex_constructor_semantic_name
  PROPERTIES LABELS "rex;AST;sanitizer")
add_test(NAME rex_constructor_semantic_name_cxx14
  COMMAND ${REX_TEST_PYTHON_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_template_parameter_spelling_runtime.py
    $<TARGET_FILE:rex_constructor_semantic_name> ${CMAKE_CXX_COMPILER}
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_constructor_semantic_name_input.cpp c++14)
set_tests_properties(rex_constructor_semantic_name_cxx14
  PROPERTIES LABELS "rex;AST;sanitizer")
add_test(NAME rex_constructor_semantic_name_cxx20
  COMMAND ${REX_TEST_PYTHON_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_template_parameter_spelling_runtime.py
    $<TARGET_FILE:rex_constructor_semantic_name> ${CMAKE_CXX_COMPILER}
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_constructor_semantic_name_input.cpp c++20)
set_tests_properties(rex_constructor_semantic_name_cxx20
  PROPERTIES LABELS "rex;AST;sanitizer")
# A nothrow query about an implicit special member whose exception
# specification the program never needed has no answer in the AST.
set(_unneeded_exception_test
  rex_constructor_unneeded_exception_specification_is_rejected)
add_test(NAME ${_unneeded_exception_test}
  COMMAND ${BASH_EXECUTABLE} -c [=[
ulimit -c 0 || exit
log=$1
diagnostic=$2
shift 2
"$@" >"$log" 2>&1
status=$?
test "$status" -eq 134 &&
  test "$(grep -Fxc -- "$diagnostic" "$log")" -eq 1
]=]
    dummy ${_unneeded_exception_test}.log
    "REX_AST_INVARIANT[unevaluated-exception-specification]: ::RexUnneededNoThrowSpecialMembers::RexUnneededNoThrowSpecialMembers has an exception specification the translated program never needed, so its nothrow guarantee is unknown"
    $<TARGET_FILE:rex_constructor_semantic_name>
    --query-unneeded-exception-specification -std=c++17 -c
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_constructor_semantic_name_input.cpp)
set_tests_properties(${_unneeded_exception_test}
  PROPERTIES LABELS "rex;AST;sanitizer;death")
unset(_unneeded_exception_test)

foreach(_array_rejection IN ITEMS anonymous_array_source unnamed_array_reference
    unnamed_array_source unnamed_array_loop)
  if(_array_rejection STREQUAL "anonymous_array_source")
    set(_array_standard c++14)
    set(_array_diagnostic
      "REX_UNPARSE_INVARIANT[anonymous-class-type]: source-anonymous class type has no standalone source spelling")
  else()
    set(_array_standard c++17)
    set(_array_diagnostic
      "REX_UNPARSE_INVARIANT[unnamed-variable-reference]: variable reference has no source identifier")
  endif()
  set(_array_test rex_constructor_${_array_rejection}_is_rejected)
  string(REPLACE "_" "-" _array_mode ${_array_rejection})
  add_test(NAME ${_array_test}
    COMMAND ${BASH_EXECUTABLE} -c [=[
ulimit -c 0 || exit
log=$1
diagnostic=$2
shift 2
"$@" >"$log" 2>&1
status=$?
test "$status" -eq 134 &&
  test "$(grep -Fxc -- "$diagnostic" "$log")" -eq 1
]=]
      dummy ${_array_test}.log "${_array_diagnostic}"
      $<TARGET_FILE:rex_constructor_semantic_name>
      --reject-${_array_mode} -std=${_array_standard} -c
      ${CMAKE_CURRENT_SOURCE_DIR}/rex_constructor_semantic_name_input.cpp)
  set_tests_properties(${_array_test}
    PROPERTIES LABELS "rex;AST;UNPARSER;sanitizer;death")
endforeach()
unset(_array_rejection)
unset(_array_standard)
unset(_array_diagnostic)
unset(_array_test)
unset(_array_mode)

add_executable(rex_template_enum_identity rex_template_enum_identity.cpp)
target_include_directories(rex_template_enum_identity PRIVATE
  ${ROSE_TOP_SRC_DIR}/src/midend/astUtil)
target_link_libraries(rex_template_enum_identity ROSE_DLL)
add_test(NAME rex_template_enum_identity
  COMMAND ${REX_TEST_PYTHON_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_template_parameter_spelling_runtime.py
    $<TARGET_FILE:rex_template_enum_identity> ${CMAKE_CXX_COMPILER}
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_template_enum_identity_input.cpp c++17)
set_tests_properties(rex_template_enum_identity
  PROPERTIES LABELS "rex;AST;sanitizer")

add_executable(rex_template_abbreviated_constraints
  rex_template_abbreviated_constraints.cpp)
target_link_libraries(rex_template_abbreviated_constraints ROSE_DLL)
add_test(NAME rex_template_abbreviated_constraints
  COMMAND ${REX_TEST_PYTHON_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_template_parameter_spelling_runtime.py
    $<TARGET_FILE:rex_template_abbreviated_constraints> ${CMAKE_CXX_COMPILER}
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_template_abbreviated_constraints_input.cpp
    c++20)
set_tests_properties(rex_template_abbreviated_constraints
  PROPERTIES LABELS "rex;AST;sanitizer")

add_executable(rex_ast_file_unparse rex_ast_file_unparse.cpp)
target_link_libraries(rex_ast_file_unparse ROSE_DLL)
add_test(NAME rex_ast_file_unparse
  COMMAND ${REX_TEST_PYTHON_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_template_parameter_spelling_runtime.py
    $<TARGET_FILE:rex_ast_file_unparse> ${CMAKE_CXX_COMPILER}
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_template_parameter_spelling_input.cpp)
set_tests_properties(rex_ast_file_unparse
  PROPERTIES LABELS "rex;AST;sanitizer")

add_executable(rex_slicing_cleanup rex_slicing_cleanup.cpp)
target_link_libraries(rex_slicing_cleanup ROSE_DLL)
add_test(NAME rex_slicing_cleanup COMMAND rex_slicing_cleanup
  -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_slicing_cleanup_input.cpp)
set_tests_properties(rex_slicing_cleanup
  PROPERTIES LABELS "rex;analysis;sanitizer")

add_executable(rex_macro_expansion_lifetime rex_macro_expansion_lifetime.cpp)
target_link_libraries(rex_macro_expansion_lifetime ROSE_DLL)
add_test(NAME rex_macro_expansion_lifetime COMMAND rex_macro_expansion_lifetime
  -rose:unparse_tokens -c
  ${CMAKE_CURRENT_SOURCE_DIR}/rex_macro_expansion_lifetime_input.cpp)
set_tests_properties(rex_macro_expansion_lifetime
  PROPERTIES LABELS "rex;AST;sanitizer")

file(GENERATE OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/test_output_path.h CONTENT [=[
#pragma once

#include <string>

#include "rose_test_output_path.h"

inline std::string resolveTestOutputPath(const std::string &filename) {
  return Rose::TestOutput::resolvePath(filename);
}
]=])

add_executable(getDataSharingAttribute getDataSharingAttribute.C)
target_link_libraries(getDataSharingAttribute ROSE_DLL ${link_with_libraries})

add_executable(get_C_array_dimensions get_C_array_dimensions.C)
target_link_libraries(get_C_array_dimensions ROSE_DLL ${link_with_libraries})

add_executable(genericCoverage genericCoverage.C)
target_link_libraries(genericCoverage ROSE_DLL ${link_with_libraries})

add_executable(collectFunctionSideEffect collectFunctionSideEffect.C)
target_link_libraries(collectFunctionSideEffect ROSE_DLL ${link_with_libraries})

add_executable(performAliasAnalysis performAliasAnalysis.C)
target_link_libraries(performAliasAnalysis ROSE_DLL ${link_with_libraries})

add_executable(rex_global_initialization_dependence
  rex_global_initialization_dependence.cpp)
target_include_directories(rex_global_initialization_dependence PRIVATE
  ${ROSE_TOP_SRC_DIR}/src/midend/astUtil)
target_link_libraries(rex_global_initialization_dependence
  ROSE_DLL ${link_with_libraries})
add_test(
  NAME rex_global_initialization_dependence
  COMMAND rex_global_initialization_dependence
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_global_initialization_dependence_input.cpp)
set_tests_properties(rex_global_initialization_dependence
  PROPERTIES LABELS "rex;analysis;dependence")

add_executable(rex_sageinterface_delete_ast_attribute_ownership
  rex_sageinterface_delete_ast_attribute_ownership.cpp)
target_link_libraries(rex_sageinterface_delete_ast_attribute_ownership
  ROSE_DLL ${link_with_libraries})
add_test(
  NAME rex_sageinterface_delete_ast_attribute_ownership
  COMMAND rex_sageinterface_delete_ast_attribute_ownership)
set_tests_properties(rex_sageinterface_delete_ast_attribute_ownership
  PROPERTIES LABELS "rex;SageInterface;sanitizer")

add_executable(rex_integer_width_contract rex_integer_width_contract.cpp)
target_include_directories(rex_integer_width_contract PRIVATE
  ${ROSE_TOP_SRC_DIR}/src/frontend/SageIII/sageInterface)
add_test(NAME rex_integer_width_contract COMMAND rex_integer_width_contract)
set_tests_properties(rex_integer_width_contract
  PROPERTIES LABELS "rex;integer-width;WASM")

set(_openmp_modifier_validation_header_test
  rex_openmp_modifier_validation_header_self_contained.cpp)
add_executable(rex_openmp_modifier_validation_header_self_contained
  ${_openmp_modifier_validation_header_test})
set_source_files_properties(${_openmp_modifier_validation_header_test}
  PROPERTIES SKIP_PRECOMPILE_HEADERS TRUE)
target_link_libraries(rex_openmp_modifier_validation_header_self_contained
  ROSE_DLL ${link_with_libraries})
add_test(
  NAME rex_openmp_modifier_validation_header_self_contained
  COMMAND rex_openmp_modifier_validation_header_self_contained)
set_tests_properties(rex_openmp_modifier_validation_header_self_contained
  PROPERTIES LABELS "rex;headers;OpenMP;WASM")

add_test(
  NAME rex_omp_support_header_self_contained
  COMMAND ${CMAKE_CXX_COMPILER}
    -std=c++17
    -fsyntax-only
    -I${ROSE_TOP_SRC_DIR}/src/frontend/SageIII
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_omp_support_header_self_contained.cpp)
set_tests_properties(rex_omp_support_header_self_contained
  PROPERTIES LABELS "rex;headers;CFE")

add_test(
  NAME rex_build_script_preserves_search_paths
  COMMAND ${BASH_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_build_script_preserves_search_paths.sh
    ${ROSE_TOP_SRC_DIR}/build-rex.sh)
set_tests_properties(rex_build_script_preserves_search_paths
  PROPERTIES LABELS "rex;build-system")

add_test(NAME rex_build_script_submodules
  COMMAND ${REX_TEST_PYTHON_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_build_script_submodules.py
    ${ROSE_TOP_SRC_DIR}/build-rex.sh)
set_tests_properties(rex_build_script_submodules
  PROPERTIES LABELS "rex;build-system" TIMEOUT 60)

add_test(NAME rex_build_script_toolchain_prefix
  COMMAND ${REX_TEST_PYTHON_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_build_script_toolchain_prefix.py
    ${ROSE_TOP_SRC_DIR}/build-rex.sh
    ${ROSE_TOP_SRC_DIR}/cmake/rex_flang_frontend_manifest.cmake
    ${ROSE_TOP_SRC_DIR}/cmake/rex_check_flang_installation.cmake)
set_tests_properties(rex_build_script_toolchain_prefix
  PROPERTIES LABELS "rex;build-system")

add_test(
  NAME rex_ci_llvm_env_preserves_search_paths
  COMMAND ${BASH_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_ci_llvm_env_preserves_search_paths.sh
    ${ROSE_TOP_SRC_DIR}/scripts/ci-llvm-env)
set_tests_properties(rex_ci_llvm_env_preserves_search_paths
  PROPERTIES LABELS "rex;build-system;CI")

add_test(
  NAME rex_frontend_toolchain_ownership_contract
  COMMAND ${BASH_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_toolchain_ownership_contract.sh
    ${ROSE_TOP_SRC_DIR}/CMakeLists.txt
    ${ROSE_TOP_SRC_DIR}/tests/nonsmoke/functional/CompileTests/Cxx20_tests/CMakeLists.txt
    ${ROSE_TOP_SRC_DIR}/tests/nonsmoke/functional/CompileTests/uninitializedField_tests/CMakeLists.txt
    ${ROSE_TOP_SRC_DIR}/tests/nonsmoke/functional/roseTests/ompLoweringTests/CMakeLists.txt
    ${ROSE_TOP_SRC_DIR}/tests/nonsmoke/functional/roseTests/ompLoweringTests/scripts/run_semantic_compare.sh)
set_tests_properties(rex_frontend_toolchain_ownership_contract
  PROPERTIES LABELS "rex;build-system;CFE;OMPLOWERING")

if(ENABLE-FORTRAN)
  set(_rex_fortran_registration_enabled 1)
else()
  set(_rex_fortran_registration_enabled 0)
endif()
add_test(
  NAME rex_fortran_test_registration_contract
  COMMAND ${BASH_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_fortran_test_registration_contract.sh
    ${CMAKE_CTEST_COMMAND}
    ${CMAKE_BINARY_DIR}
    ${_rex_fortran_registration_enabled})
set_tests_properties(rex_fortran_test_registration_contract
  PROPERTIES LABELS "rex;build-system;Fortran")

add_test(
  NAME rex_frontend_include_graph_authority_contract
  COMMAND ${BASH_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_include_graph_authority_contract.sh
    ${ROSE_TOP_SRC_DIR})
set_tests_properties(rex_frontend_include_graph_authority_contract
  PROPERTIES LABELS "rex;headers;CFE")

add_test(
  NAME rex_policy_submodule_snapshot_contract
  COMMAND ${BASH_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_policy_submodule_snapshot_contract.sh
    ${ROSE_TOP_SRC_DIR}/scripts/policies/NoTabCharacters.pl
    ${CMAKE_CURRENT_BINARY_DIR}/policy_submodule_snapshot_contract)
set_tests_properties(rex_policy_submodule_snapshot_contract
  PROPERTIES LABELS "rex;build-system;CI;policy")

add_test(NAME rex_policy_current_source_inventory
  COMMAND ${REX_TEST_PYTHON_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_policy_current_source_inventory.py
    ${ROSE_TOP_SRC_DIR}/scripts/policies/NoTabCharacters.pl)
set_tests_properties(rex_policy_current_source_inventory
  PROPERTIES LABELS "rex;build-system;CI;policy")

add_test(NAME rex_policy_filename_inventory
  COMMAND ${REX_TEST_PYTHON_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_policy_filename_inventory.py
    ${ROSE_TOP_SRC_DIR}/scripts/policies/CaseInsensitiveFileNames.pl)
set_tests_properties(rex_policy_filename_inventory
  PROPERTIES LABELS "rex;build-system;CI;policy")

add_test(
  NAME rex_ctest_name_set_fixture_contract
  COMMAND ${BASH_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_ctest_name_set_fixture_contract.sh
    ${ROSE_TOP_SRC_DIR}/scripts/run_ctest_name_set.py
    ${CMAKE_COMMAND}
    ${CMAKE_CTEST_COMMAND}
    ${CMAKE_CURRENT_SOURCE_DIR}/ctest_name_set_fixture_project
    ${CMAKE_CURRENT_BINARY_DIR}/ctest_name_set_fixture_contract)
set_tests_properties(rex_ctest_name_set_fixture_contract
  PROPERTIES LABELS "rex;build-system;CI")

add_test(
  NAME rex_docs_cpp_compilation_db_contract
  COMMAND ${BASH_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_docs_cpp_compilation_db_contract.sh
    ${ROSE_TOP_SRC_DIR}/scripts/build-docs-compilation-db.py
    ${ROSE_TOP_SRC_DIR}/scripts/generate-api-documentation
    ${REX_TEST_PYTHON_EXECUTABLE}
    ${CMAKE_CURRENT_BINARY_DIR}/docs_header_compilation_db_contract)
set_tests_properties(rex_docs_cpp_compilation_db_contract
  PROPERTIES LABELS "rex;build-system;CI;docs")

if(UNIX)
  # Deliberately terminate CTest while a child is running. Memory runs exclude
  # intentional process-termination controls through the existing death policy.
  add_test(
    NAME rex_ctest_interruption
    COMMAND ${REX_TEST_PYTHON_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/rex_ctest_interruption.py
      ${ROSE_TOP_SRC_DIR}/scripts/run_ctest_name_set.py
      ${CMAKE_COMMAND} ${CMAKE_CTEST_COMMAND}
      ${CMAKE_CURRENT_BINARY_DIR}/ctest_interruption)
  set_tests_properties(rex_ctest_interruption
    PROPERTIES LABELS "rex;build-system;CI;death" TIMEOUT 60)
endif()

if(ROSE_CTEST_MEMCHECK_WRAPPER)
  separate_arguments(_rex_memcheck_contract_options UNIX_COMMAND
    "${ROSE_CTEST_MEMCHECK_OPTIONS}")
endif()
if(ROSE_CTEST_MEMCHECK_WRAPPER AND CMAKE_Fortran_COMPILER)
  add_test(
    NAME rex_ctest_valgrind_wrapper_contract
    COMMAND ${BASH_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/rex_ctest_valgrind_wrapper_contract.sh
      ${ROSE_CTEST_MEMCHECK_WRAPPER}
      ${BASH_EXECUTABLE}
      ${CMAKE_C_COMPILER}
      ${CMAKE_CXX_COMPILER}
      ${CMAKE_Fortran_COMPILER}
      ${CMAKE_CURRENT_BINARY_DIR}/ctest_valgrind_wrapper_contract
      ${_rex_memcheck_contract_options}
      --suppressions=${ROSE_CTEST_MEMCHECK_SUPPRESSIONS})
  set_tests_properties(rex_ctest_valgrind_wrapper_contract
    PROPERTIES LABELS "rex;build-system;CI;memcheck")
endif()

if(ROSE_CTEST_MEMCHECK_WRAPPER)
  add_test(
    NAME rex_ctest_memcheck_result_contract
    COMMAND ${REX_TEST_PYTHON_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/rex_ctest_memcheck_result_contract.py
      ${CMAKE_COMMAND} ${CMAKE_CTEST_COMMAND} ${CMAKE_C_COMPILER}
      ${ROSE_CTEST_MEMCHECK_WRAPPER} ${ROSE_TOP_SRC_DIR}
      ${CMAKE_CURRENT_BINARY_DIR}/ctest_memcheck_result_contract)
  set_tests_properties(rex_ctest_memcheck_result_contract
    PROPERTIES LABELS "rex;build-system;CI;memcheck")

  if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_test(
      NAME rex_memcheck_external_boundary
      COMMAND ${REX_TEST_PYTHON_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/rex_memcheck_external_boundary.py
        ${CMAKE_C_COMPILER} ${ROSE_CTEST_MEMCHECK_WRAPPER}
        ${ROSE_CTEST_MEMCHECK_SUPPRESSIONS}
        ${CMAKE_CURRENT_BINARY_DIR}/memcheck_external_boundary
        ${_rex_memcheck_contract_options})
    set_tests_properties(rex_memcheck_external_boundary
      PROPERTIES LABELS "rex;build-system;CI;memcheck")
    if(ROSE_LLVM_CLANG_C_DRIVER AND ROSE_LLVM_CLANG_CXX_DRIVER)
      add_test(
        NAME rex_memcheck_compiler_boundaries
        COMMAND ${REX_TEST_PYTHON_EXECUTABLE}
          ${CMAKE_CURRENT_SOURCE_DIR}/rex_memcheck_compiler_boundaries.py
          ${ROSE_CTEST_MEMCHECK_WRAPPER}
          ${ROSE_LLVM_CLANG_C_DRIVER} ${ROSE_LLVM_CLANG_CXX_DRIVER}
          ${ROSE_CTEST_MEMCHECK_SUPPRESSIONS}
          ${CMAKE_CURRENT_BINARY_DIR}/memcheck_compiler_boundaries
          ${_rex_memcheck_contract_options})
      set_tests_properties(rex_memcheck_compiler_boundaries
        PROPERTIES LABELS "rex;build-system;CI;memcheck")
    endif()
  endif()
endif()

set(_openmp_specimens_path
  ${CMAKE_SOURCE_DIR}/tests/nonsmoke/functional/CompileTests/OpenMP_tests)
set(_dataracebench_path ${_openmp_specimens_path}/dataRaceBench)
set(_c_specimens_path
  ${CMAKE_SOURCE_DIR}/tests/nonsmoke/functional/CompileTests/C_tests)
set(_cxx_specimens_path
  ${CMAKE_SOURCE_DIR}/tests/nonsmoke/functional/CompileTests/Cxx_tests)
set(_alias_specimens_path ${CMAKE_CURRENT_SOURCE_DIR}/tests)
set(_reference_dir ${CMAKE_CURRENT_SOURCE_DIR}/referenceOutput)
set(_sage_output_dir ${CMAKE_CURRENT_BINARY_DIR}/test-output)
file(MAKE_DIRECTORY ${_sage_output_dir})
set(_test_flags)
#Most C DataRaceBench specimens rely on GNU89 implicit - declaration semantics
#during parsing, but a few use C99 - only keywords such as `restrict`.
set(_datarace_c_gnu89_test_flags -std=gnu89)
set(_datarace_c_gnu99_test_flags -std=gnu99)

set(SAGE_OPENMP_SPECIMENS
  3loops.c
  alignment.c
  array_init_2.c
  array_init.c
  atomic.c
  atoms-2.c
  rex_openmp_axpy_multi_device.c
  axpy_ompacc3.c
  axpy_ompacc.c
  barrier.c
  bonds-2.c
  collapse_2.c
  collapse.c
  copyin.c
  copyprivate2.c
  copyprivate3.c
  copyprivate.c
  critical.c
  critical_dead.c
  critical_orphaned.c
  dijkstra_open_mp.c
  dynamicChunk.c
  empty.c
  endif2.c
  endif3.c
  endif.c
  expressions.c
  falsesharing.c
  firstlastprivate.c
  firstPrivateArray_2.c
  firstPrivateArray.c
  firstprivate.c
  flush.c
  flush_exampleA_21_1c.c
  full_verify.c
  get_max_threads.c
  hello-1.c
  hello-2.c
  hello.c
  hello-ordered.c
  include_header_only.c
  init.c
  jacobi.c
  jacobi-collapse.c
  jacobi-ompacc.c
  jacobi-ompacc-multiGPU.c
  jacobi-ompacc-opt1.c
  jacobi-ompacc-opt2.c
  jacobi-ompacc-v2.c
  lastprivate0.c
  lastprivate.c
  lastprivateOrphaned.c
  limits_threads.c
  linebreak.c
  lockarray.c
  loop1.c
  lu_factorization.c
  macroIds.c
  master.c
  masterSingle.c
  matrixmultiply.c
  matrixmultiply-ompacc2.c
  matrixmultiply-ompacc.c
  matrix_vector.c
  md_open_mp.c
  multiple_return.c
  nestedpar1.c
  nestedpar.c
  nowait.c
  omp1.c
  ompfor2.c
  ompfor3.c
  ompfor4.c
  ompfor5.c
  ompfor6.c
  ompfor7.c
  ompfor.c
  ompfor_c99.c
  ompfor-decremental.c
  ompfor-default.c
  ompfor-static.c
  ompGetNumThreads.c
  omp_sections.c
  ordered2.c
  ordered.c
  orphaned-directives.c
  parallel.c
  parallelfor2.c
  parallelfor.c
  parallel-if.c
  parallel-if-numthreads.c
  parallel-numthreads.c
  parallel-reduction2.c
  parallel-reduction.c
  parallelsections.c
  preprocessingInfo.c
  prettyfunc.c
  private.c
  private-duplicate.c
  privatej.c
  recursive.c
  reduction2.c
  reduction.c
  reduction-classic.c
  reduction_min_max.c
  rice1.c
  section1.c
  section.c
  set_num_threads.c
  shared.c
  single2.c
  single.c
  single_copyprivate.c
  sizeof.c
  spmd1.c
  staticChunk.c
  subteam2.c
  subteam.c
  task_array2.c
  task_array.c
  task_largenumber.c
  task_orphaned.c
  task_outlining.c
  task_underIf.c
  task_untied2.c
  task_untied3.c
  task_untied4.c
  task_untied.c
  task_wait2.c
  task_wait.c
  threadprivate2.c
  threadprivate3.c
  threadprivate.c
  threadProcessor.c
  twoRegions.c
  upperCase.c
  variables.c
)

set(SAGE_OPENMP_VERIFY
  3loops.c
  alignment.c
  array_init_2.c
  array_init.c
  atomic.c
  atoms-2.c
  rex_openmp_axpy_multi_device.c
  axpy_ompacc3.c
  axpy_ompacc.c
  barrier.c
  bonds-2.c
  collapse_2.c
  collapse.c
  copyin.c
  copyprivate2.c
  copyprivate3.c
  copyprivate.c
  critical.c
  critical_dead.c
  critical_orphaned.c
  dijkstra_open_mp.c
  dynamicChunk.c
  empty.c
  endif2.c
  endif3.c
  endif.c
  reduction_min_max.c
)

set(SAGE_DATARACE_C
  3mm-tile-no.c
  adi-tile-no.c
  3mm-parallel-no.c
  adi-parallel-no.c
  antidep1-orig-yes.c
  antidep1-var-yes.c
  antidep2-orig-yes.c
  antidep2-var-yes.c
  doall1-orig-no.c
  doall2-orig-no.c
  doall2-orig-yes.c
  doallchar-orig-no.c
  firstprivate-orig-no.c
  flush-orig-no.c
  flush-orig-yes.c
  fprintf-orig-no.c
  functionparameter-orig-no.c
  getthreadnum-orig-no.c
  getthreadnum-orig-yes.c
  indirectaccess1-orig-yes.c
  indirectaccess2-orig-yes.c
  indirectaccess3-orig-yes.c
  indirectaccess4-orig-yes.c
  indirectaccesssharebase-orig-no.c
  inneronly1-orig-no.c
  inneronly2-orig-no.c
  jacobi2d-parallel-no.c
  jacobi2d-tile-no.c
  jacobiinitialize-orig-no.c
  jacobikernel-orig-no.c
  lastprivatemissing-orig-yes.c
  lastprivatemissing-var-yes.c
  lastprivate-orig-no.c
  matrixmultiply-orig-no.c
  matrixvector1-orig-no.c
  matrixvector2-orig-no.c
  minusminus-orig-yes.c
  minusminus-var-yes.c
  nowait-orig-yes.c
  outeronly1-orig-no.c
  outeronly2-orig-no.c
  outofbounds-orig-yes.c
  outofbounds-var-yes.c
  outputdep-orig-yes.c
  outputdep-var-yes.c
  pireduction-orig-no.c
  plusplus-orig-yes.c
  plusplus-var-yes.c
  pointernoaliasing-orig-no.c
  privatemissing-orig-yes.c
  privatemissing-var-yes.c
  reductionmissing-orig-yes.c
  reductionmissing-var-yes.c
  restrictpointer1-orig-no.c
  restrictpointer2-orig-no.c
  sections1-orig-yes.c
  sectionslock1-orig-no.c
  simd1-orig-no.c
  simdtruedep-orig-yes.c
  simdtruedep-var-yes.c
  single-orig-no.c
  targetparallelfor-orig-no.c
  targetparallelfor-orig-yes.c
  taskdep1-orig-no.c
  taskdep2-orig-no.c
  taskdep3-orig-no.c
  taskdependmissing-orig-yes.c
  truedep1-orig-yes.c
  truedep1-var-yes.c
  truedepfirstdimension-orig-yes.c
  truedepfirstdimension-var-yes.c
  truedeplinear-orig-yes.c
  truedeplinear-var-yes.c
  truedepscalar-orig-yes.c
  truedepscalar-var-yes.c
  truedepseconddimension-orig-yes.c
  truedepseconddimension-var-yes.c
  truedepsingleelement-orig-yes.c
  truedepsingleelement-var-yes.c
  2018-simd2-orig-no.c
  2018-parallel-for-simd-orig-no.c
  2018-threadprivate-orig-no.c
  2018-private-origin-no.c
  2018-static-origin-yes.c
  2018-collapse-orig-no.c
  2018-collapsemissing-orig-yes.c
  2018-ordered-orig-no.c
  2018-ordered-orig-yes.c
)

set(SAGE_DATARACE_CPP
  2018-static-data-member-orig-yes.cpp
)

set(SAGE_DATARACE_C_GNU99
  restrictpointer1-orig-no.c
  restrictpointer2-orig-no.c
)

set(SAGE_C_ARRAY_DIMENSIONS
  test2017_01_array.c
)

set(SAGE_GENERIC_COVERAGE
  helloworld.cpp
)

set(SAGE_COLLECT_FUNCTION_SIDE_EFFECT
  rex_readwriteset_address_of_cast.cpp
  rex_readwriteset_nested_memory_refs.cpp
  test2017_14_Liao.C
  test2017_15_Liao.C
)

set(SAGE_ALIAS_ANALYSIS
  alias-001.cpp
  alias-002.cpp
  alias-003.cpp
)

function(_sage_sanitize_name input output_var)
  set(_name "${input}")
  string(REPLACE "/" "_" _name "${_name}")
  string(REPLACE "." "_" _name "${_name}")
  string(REPLACE "-" "_" _name "${_name}")
  string(REPLACE "+" "_" _name "${_name}")
  set(${output_var} "${_name}" PARENT_SCOPE)
endfunction()

function(_sage_add_diff_test test_name output_file)
  if(ARGC GREATER 2)
    set(_reference_output ${ARGV2})
  else()
    set(_reference_output ${output_file})
  endif()
  add_test(
    NAME ${test_name}_diff
    COMMAND ${CMAKE_COMMAND} -E compare_files
      ${_sage_output_dir}/${output_file}
      ${_reference_dir}/${_reference_output}
  )
  set_tests_properties(${test_name}_diff PROPERTIES DEPENDS "${test_name}")
endfunction()

function(_sage_set_output_env test_name)
  set_property(TEST ${test_name} APPEND PROPERTY ENVIRONMENT
               "ROSE_TEST_OUTPUT_DIR=${_sage_output_dir}")
endfunction()

foreach(specimen IN LISTS SAGE_OPENMP_SPECIMENS)
  _sage_sanitize_name(${specimen} _name)
  set(_test_name sage_getDataSharingAttribute_${_name})
  set(_output ${specimen}.getDataSharingAttribute.output)
  add_test(
    NAME ${_test_name}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND $<TARGET_FILE:getDataSharingAttribute>
      ${_test_flags}
      -c -rose:openmp:ast_only
      ${_openmp_specimens_path}/${specimen}
  )
  _sage_set_output_env(${_test_name})
  if(specimen IN_LIST SAGE_OPENMP_VERIFY)
    if(specimen STREQUAL "rex_openmp_axpy_multi_device.c")
      _sage_add_diff_test(${_test_name} ${_output}
        axpy_ompacc2.c.getDataSharingAttribute.output)
    else()
      _sage_add_diff_test(${_test_name} ${_output})
    endif()
  endif()
endforeach()

foreach(specimen IN LISTS SAGE_DATARACE_C)
  _sage_sanitize_name(${specimen} _name)
  set(_test_name sage_getDataSharingAttribute_datarace_${_name})
  set(_output ${specimen}.getDataSharingAttribute.output)
  set(_specimen_flags ${_datarace_c_gnu89_test_flags})
  if(specimen IN_LIST SAGE_DATARACE_C_GNU99)
    set(_specimen_flags ${_datarace_c_gnu99_test_flags})
  endif()
  add_test(
    NAME ${_test_name}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND $<TARGET_FILE:getDataSharingAttribute>
      ${_test_flags}
      ${_specimen_flags}
      -I${_dataracebench_path}
      -c -rose:openmp:ast_only
      ${_dataracebench_path}/${specimen}
  )
  _sage_set_output_env(${_test_name})
  _sage_add_diff_test(${_test_name} ${_output})
endforeach()

foreach(specimen IN LISTS SAGE_DATARACE_CPP)
  _sage_sanitize_name(${specimen} _name)
  set(_test_name sage_getDataSharingAttribute_datarace_${_name})
  set(_output ${specimen}.getDataSharingAttribute.output)
  add_test(
    NAME ${_test_name}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND $<TARGET_FILE:getDataSharingAttribute>
      ${_test_flags}
      -I${_dataracebench_path}
      -c -rose:openmp:ast_only
      ${_dataracebench_path}/${specimen}
  )
  _sage_set_output_env(${_test_name})
  _sage_add_diff_test(${_test_name} ${_output})
endforeach()

foreach(specimen IN LISTS SAGE_C_ARRAY_DIMENSIONS)
  _sage_sanitize_name(${specimen} _name)
  set(_test_name sage_get_C_array_dimensions_${_name})
  set(_output ${specimen}.get_C_array_dimensions.output)
  add_test(
    NAME ${_test_name}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND $<TARGET_FILE:get_C_array_dimensions>
      ${_test_flags}
      -c ${_c_specimens_path}/${specimen}
  )
  _sage_set_output_env(${_test_name})
  _sage_add_diff_test(${_test_name} ${_output})
endforeach()

foreach(specimen IN LISTS SAGE_GENERIC_COVERAGE)
  _sage_sanitize_name(${specimen} _name)
  set(_test_name sage_genericCoverage_${_name})
  set(_output ${specimen}.genericCoverage.output)
  add_test(
    NAME ${_test_name}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND $<TARGET_FILE:genericCoverage>
      ${_test_flags}
      -c ${_cxx_specimens_path}/${specimen}
  )
  _sage_set_output_env(${_test_name})
  _sage_add_diff_test(${_test_name} ${_output})
endforeach()

foreach(specimen IN LISTS SAGE_COLLECT_FUNCTION_SIDE_EFFECT)
  _sage_sanitize_name(${specimen} _name)
  set(_test_name sage_collectFunctionSideEffect_${_name})
  set(_output ${specimen}.collectFunctionSideEffect.output)
  add_test(
    NAME ${_test_name}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND $<TARGET_FILE:collectFunctionSideEffect>
      -sort ${_test_flags}
      -annot ${CMAKE_CURRENT_SOURCE_DIR}/vectorXY.annot
      -c ${_cxx_specimens_path}/${specimen}
  )
  _sage_set_output_env(${_test_name})
  _sage_add_diff_test(${_test_name} ${_output})
endforeach()

foreach(specimen IN LISTS SAGE_ALIAS_ANALYSIS)
  _sage_sanitize_name(${specimen} _name)
  set(_test_name sage_performAliasAnalysis_${_name})
  set(_output ${specimen}.performAliasAnalysis.output)
  add_test(
    NAME ${_test_name}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND $<TARGET_FILE:performAliasAnalysis>
      ${_test_flags}
      -c ${_alias_specimens_path}/${specimen}
  )
  _sage_set_output_env(${_test_name})
  _sage_add_diff_test(${_test_name} ${_output})
endforeach()

if(TARGET astCopyReplTest)
  set(_rex_copy_example_options)
  if(ENABLE-FORTRAN)
    list(APPEND _rex_copy_example_options --fortran)
  endif()
  add_test(NAME rex_copy_replacement_example
    COMMAND ${REX_TEST_PYTHON_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/rex_copy_replacement_example.py
      --translator $<TARGET_FILE:astCopyReplTest> ${_rex_copy_example_options})
  set_tests_properties(rex_copy_replacement_example
    PROPERTIES LABELS "examples;frontend;ownership")
  add_test(NAME rex_copy_replacement_lambda
    COMMAND astCopyReplTest -std=c++17 -c
      ${CMAKE_SOURCE_DIR}/tests/nonsmoke/functional/CompileTests/copyAST_tests/rex_copy_structural_ownership_contract_input.cpp)
  set_tests_properties(rex_copy_replacement_lambda
    PROPERTIES LABELS "examples;frontend;ownership")
  unset(_rex_copy_example_options)
endif()

if(TARGET astTermGenerator AND TARGET pdfGenerator)
  add_test(NAME rex_example_output_contract
    COMMAND ${REX_TEST_PYTHON_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/rex_example_output_contract.py
      --term-generator $<TARGET_FILE:astTermGenerator>
      --pdf-generator $<TARGET_FILE:pdfGenerator>
      --work "${CMAKE_CURRENT_BINARY_DIR}/example output contract")
  set_tests_properties(rex_example_output_contract PROPERTIES LABELS "examples;frontend")
endif()


add_executable(rex_ast_enclosing_node_contract rex_ast_enclosing_node_contract.cpp)
target_link_libraries(rex_ast_enclosing_node_contract PRIVATE ROSE_DLL)
foreach(mode IN ITEMS deep declarations)
  add_test(NAME rex_ast_enclosing_node_${mode}
    COMMAND rex_ast_enclosing_node_contract ${mode})
  set_tests_properties(rex_ast_enclosing_node_${mode}
    PROPERTIES LABELS "rex;AST;SageInterface")
endforeach()
add_test(NAME rex_ast_statement_scope_deep
  COMMAND rex_ast_enclosing_node_contract scope_deep)
set_tests_properties(rex_ast_statement_scope_deep
  PROPERTIES LABELS "rex;AST;SageInterface")

add_executable(rex_ast_traversal_attributes rex_ast_traversal_attributes.cpp)
target_link_libraries(rex_ast_traversal_attributes PRIVATE ROSE_DLL)
add_test(NAME rex_ast_traversal_attributes COMMAND rex_ast_traversal_attributes)
set_tests_properties(rex_ast_traversal_attributes
  PROPERTIES LABELS "rex;AST;SageInterface")

add_executable(rex_ast_traversal_diagnostics rex_ast_traversal_diagnostics.cpp)
target_link_libraries(rex_ast_traversal_diagnostics PRIVATE ROSE_DLL)
add_test(NAME rex_ast_traversal_diagnostics_valid
  COMMAND rex_ast_traversal_diagnostics valid)
set_tests_properties(rex_ast_traversal_diagnostics_valid
  PROPERTIES LABELS "rex;AST;SageInterface")

function(_rex_add_ast_diagnostic_death_test name target mode diagnostic)
  add_test(NAME ${name}
    COMMAND ${BASH_EXECUTABLE} -c
      [=[ulimit -c 0 || exit
log="$0.log"
"$1" "$2" >"$log" 2>&1
status=$?
test "$status" -eq 134 && test "$(grep -Fc -- "$3" "$log")" -eq 1]=]
      ${name} $<TARGET_FILE:${target}> ${mode} "${diagnostic}")
  set_tests_properties(${name} PROPERTIES LABELS "rex;AST;SageInterface;death")
endfunction()

if(ENABLE-C)
  _rex_add_ast_diagnostic_death_test(rex_callgraph_missing_member_symbol_is_rejected
    rex_callgraph_implicit_conversion missing_member_symbol
    "memberFunctionSymbol != nullptr")
  _rex_add_ast_diagnostic_death_test(rex_callgraph_missing_member_declaration_is_rejected
    rex_callgraph_implicit_conversion missing_member_declaration
    "declaration != nullptr")
endif()

foreach(mode IN ITEMS cycle semantic_cycle)
  _rex_add_ast_diagnostic_death_test(rex_ast_enclosing_node_${mode}_is_rejected
    rex_ast_enclosing_node_contract ${mode}
    "next == nullptr || next != checkpoint")
endforeach()
_rex_add_ast_diagnostic_death_test(rex_ast_statement_scope_cycle_is_rejected
  rex_ast_enclosing_node_contract scope_cycle
  "REX_AST_INVARIANT[statement-scope-cycle]")
_rex_add_ast_diagnostic_death_test(rex_ast_ancestor_cycle_is_rejected
  rex_ast_enclosing_node_contract ancestor_cycle
  "REX_AST_INVARIANT[ancestor-parent-cycle]")
foreach(mode IN ITEMS count names)
  _rex_add_ast_diagnostic_death_test(rex_ast_traversal_${mode}_is_rejected
    rex_ast_traversal_diagnostics ${mode}
    "REX_AST_INVARIANT[traversal-cardinality]")
endforeach()
foreach(mode IN ITEMS null duplicate)
  _rex_add_ast_diagnostic_death_test(rex_ast_traversal_${mode}_is_rejected
    rex_ast_traversal_diagnostics ${mode}
    "REX_AST_INVARIANT[traversal-identity]")
endforeach()

add_executable(rex_ast_parent_ownership rex_ast_parent_ownership.cpp)
target_link_libraries(rex_ast_parent_ownership PRIVATE ROSE_DLL)
add_test(NAME rex_ast_parent_ownership_valid
  COMMAND rex_ast_parent_ownership valid)
set_tests_properties(rex_ast_parent_ownership_valid
  PROPERTIES LABELS "rex;AST;SageInterface")
foreach(mode IN ITEMS wrong missing)
  _rex_add_ast_diagnostic_death_test(rex_ast_parent_${mode}_is_rejected
    rex_ast_parent_ownership ${mode}
    "REX_AST_INVARIANT[parent-ownership]")
endforeach()

add_test(NAME rex_ast_parent_ownership_reuse
  COMMAND rex_ast_parent_ownership reuse)
set_tests_properties(rex_ast_parent_ownership_reuse
  PROPERTIES LABELS "rex;AST;SageInterface")
_rex_add_ast_diagnostic_death_test(rex_ast_parent_duplicate_structural_node_is_rejected
  rex_ast_parent_ownership duplicate
  "REX_AST_INVARIANT[unique-structural-node]")

add_executable(rex_ast_nonreal_scope rex_ast_nonreal_scope.cpp)
target_link_libraries(rex_ast_nonreal_scope PRIVATE ROSE_DLL)
add_test(NAME rex_ast_nonreal_scope_valid COMMAND rex_ast_nonreal_scope valid)
set_tests_properties(rex_ast_nonreal_scope_valid
  PROPERTIES LABELS "rex;AST;SageInterface")
_rex_add_ast_diagnostic_death_test(rex_ast_nonreal_missing_scope_is_rejected
  rex_ast_nonreal_scope missing "REX_AST_INVARIANT[nonreal-declaration-scope]")
_rex_add_ast_diagnostic_death_test(rex_ast_nonreal_owner_cycle_is_rejected
  rex_ast_nonreal_scope cycle "nonreal declaration ownership contains a cycle")
_rex_add_ast_diagnostic_death_test(rex_ast_nonreal_duplicate_argument_is_rejected
  rex_ast_nonreal_scope duplicate_argument "duplicate nonreal template argument")
_rex_add_ast_diagnostic_death_test(rex_ast_nonreal_pack_marker_is_rejected
  rex_ast_nonreal_scope pack_marker "nonreal-pack-marker")
_rex_add_ast_diagnostic_death_test(rex_ast_template_expression_cycle_is_rejected
  rex_ast_nonreal_scope template_expression_cycle
  "REX_AST_INVARIANT[template-identity]: template expression contains a cycle")

add_executable(rex_ast_nonreal_symbol_scope rex_ast_nonreal_symbol_scope.cpp)
target_link_libraries(rex_ast_nonreal_symbol_scope PRIVATE ROSE_DLL)
add_test(NAME rex_ast_nonreal_symbol_scope_valid COMMAND rex_ast_nonreal_symbol_scope valid)
set_tests_properties(rex_ast_nonreal_symbol_scope_valid
  PROPERTIES LABELS "rex;AST;SageInterface")
_rex_add_ast_diagnostic_death_test(rex_ast_nonreal_wrong_symbol_scope_is_rejected
  rex_ast_nonreal_symbol_scope wrong "REX_AST_INVARIANT[nonreal-symbol-scope]")

add_executable(rex_sagebuilder_frontend_failure rex_sagebuilder_frontend_failure.cpp)
target_link_libraries(rex_sagebuilder_frontend_failure PRIVATE ROSE_DLL)
add_test(NAME rex_sagebuilder_missing_input_is_rejected
  COMMAND ${REX_TEST_PYTHON_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_sagebuilder_missing_input.py
    $<TARGET_FILE:rex_sagebuilder_frontend_failure>)
set_tests_properties(rex_sagebuilder_missing_input_is_rejected
  PROPERTIES LABELS "rex;AST;SageInterface;death")
if(ENABLE-FORTRAN)
  _rex_add_ast_diagnostic_death_test(rex_sagebuilder_failed_frontend_is_rejected
    rex_sagebuilder_frontend_failure
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_sagebuilder_frontend_failure.f90
    "REX_SAGEBUILDER_INVARIANT[file-frontend-failure]")
endif()

add_executable(rex_sagebuilder_copied_preprocessing rex_sagebuilder_copied_preprocessing.cpp)
target_link_libraries(rex_sagebuilder_copied_preprocessing PRIVATE ROSE_DLL)
set(_rex_copied_preprocessing_languages c cpp)
if(ENABLE-FORTRAN)
  list(APPEND _rex_copied_preprocessing_languages f90)
endif()
foreach(language IN LISTS _rex_copied_preprocessing_languages)
  add_test(NAME rex_sagebuilder_copied_preprocessing_${language}
    COMMAND ${REX_TEST_PYTHON_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/rex_sagebuilder_copied_preprocessing.py
      $<TARGET_FILE:rex_sagebuilder_copied_preprocessing> ${language})
  set_tests_properties(rex_sagebuilder_copied_preprocessing_${language}
    PROPERTIES LABELS "rex;AST;SageInterface;preprocessing")
endforeach()
unset(_rex_copied_preprocessing_languages)

add_test(NAME rex_cmake_valgrind_selection
  COMMAND ${REX_TEST_PYTHON_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_cmake_valgrind_selection.py
    ${ROSE_TOP_SRC_DIR}/CMakeLists.txt ${CMAKE_COMMAND})
set_tests_properties(rex_cmake_valgrind_selection
  PROPERTIES LABELS "rex;build-system;CI")

add_test(NAME rex_cmake_flang_header_selection
  COMMAND ${REX_TEST_PYTHON_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_cmake_flang_header_selection.py
    ${ROSE_TOP_SRC_DIR}/CMakeLists.txt ${CMAKE_COMMAND})
set_tests_properties(rex_cmake_flang_header_selection
  PROPERTIES LABELS "rex;build-system")

add_executable(rex_walrus_graph_output rex_walrus_graph_output.cpp)
target_link_libraries(rex_walrus_graph_output PRIVATE ROSE_DLL)
add_test(NAME rex_walrus_graph_output
  COMMAND ${REX_TEST_PYTHON_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_walrus_graph_output.py
    $<TARGET_FILE:rex_walrus_graph_output>)
set_tests_properties(rex_walrus_graph_output
  PROPERTIES LABELS "rex;AST;SageInterface;graph")
