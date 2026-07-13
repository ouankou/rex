add_test(
  NAME compiler_options_gnu_options
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${BASH_EXECUTABLE} -c
    "set -e; \
     ${ROSE_TEST_TRANSLATOR} -rose:verbose 0 -include ${CMAKE_CURRENT_SOURCE_DIR}/testOptions.h -c ${CMAKE_CURRENT_SOURCE_DIR}/test-includeOption.C; \
     ${ROSE_TEST_TRANSLATOR} -rose:verbose 0 -isystem ${CMAKE_CURRENT_SOURCE_DIR} -c ${CMAKE_CURRENT_SOURCE_DIR}/test-isystemOption.C; \
     cp ${CMAKE_CURRENT_SOURCE_DIR}/test-isystemOption.C local_test-isystemOption.C; \
     ${ROSE_TEST_TRANSLATOR} -rose:verbose 0 -isystem ${CMAKE_CURRENT_SOURCE_DIR} -c local_test-isystemOption.C"
)

add_executable(
  rex_frontend_forced_include_variable_owner_translator
  rex_frontend_forced_include_variable_owner_translator.cpp
)
target_link_libraries(
  rex_frontend_forced_include_variable_owner_translator
  ROSE_DLL
  ${link_with_libraries}
)

add_test(
  NAME compiler_options_gnu_rex_frontend_forced_include_variable_owner
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND
    $<TARGET_FILE:rex_frontend_forced_include_variable_owner_translator>
    -rose:verbose 0
    -include
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_forced_include_variable_owner.hpp
    -rose:skipfinalCompileStep
    -c
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_forced_include_variable_owner.cpp
)
set_tests_properties(
  compiler_options_gnu_rex_frontend_forced_include_variable_owner
  PROPERTIES LABELS "rex;Cxx_tests"
)

add_test(
  NAME compiler_options_gnu_minus_x
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${BASH_EXECUTABLE} -c
    "set -e; \
     rm -f compiler_options_gnu_minus_x.log; \
     ROSE_PHASE_TRACE=1 ${ROSE_TEST_TRANSLATOR} -c -x c /dev/null -rose:verbose 0 -rose:skipfinalCompileStep > compiler_options_gnu_minus_x.log 2>&1; \
     grep -F 'ROSE_PHASE clang_main.parse.begin' compiler_options_gnu_minus_x.log; \
     grep -F 'ROSE_PHASE clang_main.parse.end' compiler_options_gnu_minus_x.log; \
     grep -F 'ROSE_PHASE clang_main.end' compiler_options_gnu_minus_x.log"
)

add_test(
  NAME compiler_options_gnu_nonexistent_system_include_candidate
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${BASH_EXECUTABLE} -c
    "set -eu; \
     missing='${CMAKE_CURRENT_BINARY_DIR}/rex_nonexistent_system_include_candidate'; \
     rm -rf -- \"$missing\"; \
     test ! -e \"$missing\"; \
     LLVM_DIR='${LLVM_DIR}' $<TARGET_FILE:testTranslator> \
       -c -x c ${CMAKE_CURRENT_SOURCE_DIR}/rex_cmdline_split_x_c_input.c \
       -isystem \"$missing\" -rose:verbose 0 -rose:skipfinalCompileStep"
)

add_test(
  NAME compiler_options_gnu_joined_x_c
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${ROSE_TEST_TRANSLATOR} -c -xc
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_cmdline_joined_x_c_input.cpp
    -rose:verbose 0 -rose:skipfinalCompileStep
)

add_test(
  NAME compiler_options_gnu_x_none_after_source
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${ROSE_TEST_TRANSLATOR} -c -x c
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_cmdline_joined_x_c_input.cpp
    -x none -rose:verbose 0 -rose:skipfinalCompileStep
)

configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/rex_cmdline_split_x_c_input.c
  ${CMAKE_CURRENT_BINARY_DIR}/rex_cmdline_split_x_c_input
  COPYONLY
)
configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/rex_cmdline_split_x_cxx_input.cpp
  ${CMAKE_CURRENT_BINARY_DIR}/rex_cmdline_split_x_cxx_input
  COPYONLY
)
configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/rex_cmdline_split_x_c_input.c
  ${CMAKE_CURRENT_BINARY_DIR}/rex_cmdline_suffix_c_input.i
  COPYONLY
)
configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/rex_cmdline_split_x_cxx_input.cpp
  ${CMAKE_CURRENT_BINARY_DIR}/rex_cmdline_suffix_cxx_input.ii
  COPYONLY
)
configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/rex_cmdline_split_x_cxx_input.cpp
  ${CMAKE_CURRENT_BINARY_DIR}/rex_cmdline_suffix_cxx_module_input.cppm
  COPYONLY
)

add_test(
  NAME compiler_options_gnu_per_input_x_state
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${BASH_EXECUTABLE} -c
    "set -e; \
     rm -f compiler_options_gnu_per_input_x_state.log; \
     ROSE_PHASE_TRACE=1 ${ROSE_TEST_TRANSLATOR} -c \
       -x c ${CMAKE_CURRENT_BINARY_DIR}/rex_cmdline_split_x_c_input \
       -x c++ ${CMAKE_CURRENT_BINARY_DIR}/rex_cmdline_split_x_cxx_input \
       -rose:verbose 0 -rose:skipfinalCompileStep \
       > compiler_options_gnu_per_input_x_state.log 2>&1; \
     test \"$(grep -Fc 'ROSE_PHASE clang_main.parse.begin' compiler_options_gnu_per_input_x_state.log)\" -eq 2; \
     test \"$(grep -Fc 'ROSE_PHASE clang_main.parse.end' compiler_options_gnu_per_input_x_state.log)\" -eq 2; \
     test \"$(grep -Fc 'ROSE_PHASE clang_main.end' compiler_options_gnu_per_input_x_state.log)\" -eq 2"
)

add_test(
  NAME compiler_options_gnu_extensionless_x_backend_validation
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${ROSE_TEST_TRANSLATOR} -c -x c
    ${CMAKE_CURRENT_BINARY_DIR}/rex_cmdline_split_x_c_input
    -rose:verbose 0
)

add_test(
  NAME compiler_options_gnu_x_cxx_module
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${ROSE_TEST_TRANSLATOR} -c -x c++-module /dev/null
    -rose:verbose 0 -rose:skipfinalCompileStep
)

function(add_rex_cmdline_death_test test_name expected_diagnostic)
  add_test(
    NAME ${test_name}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${BASH_EXECUTABLE} -c
      "ulimit -c 0; log=\"$1\"; expected=\"$2\"; shift 2; set +e; \"$@\" >\"$log\" 2>&1; status=$?; set -e; test \"$status\" -eq 134; test \"$(grep -Fxc -- \"$expected\" \"$log\")\" -eq 1"
      dummy
      ${test_name}.log
      "${expected_diagnostic}"
      ${ROSE_TEST_TRANSLATOR}
      ${ARGN}
  )
  set_tests_properties(${test_name} PROPERTIES LABELS "rex;death")
endfunction()

add_rex_cmdline_death_test(
  compiler_options_gnu_x_missing_operand
  "REX_FRONTEND_INVARIANT[clang-option-operand]: option '-x' requires an argument"
  -x
)

add_rex_cmdline_death_test(
  compiler_options_gnu_compilation_performance_duplicate
  "REX_FRONTEND_INVARIANT[compilation-performance-option]: option '-rose:compilationPerformanceFile' appears more than once in one translation-unit command"
  -rose:compilationPerformanceFile first.csv
  -rose:compilationPerformanceFile second.csv
  ${CMAKE_CURRENT_SOURCE_DIR}/rex_cmdline_joined_x_c_input.cpp
)

set(_rex_unsupported_x_diagnostic
  "REX_FRONTEND_INVARIANT[clang-language]: explicit -x language 'objective-c' is unsupported; expected a supported C, C++, CUDA, or OpenCL Clang 22 input type, or none"
)
add_rex_cmdline_death_test(
  compiler_options_gnu_x_unsupported_split
  "${_rex_unsupported_x_diagnostic}"
  -x objective-c
  ${CMAKE_CURRENT_SOURCE_DIR}/rex_cmdline_joined_x_c_input.cpp
)
add_rex_cmdline_death_test(
  compiler_options_gnu_x_unsupported_joined
  "${_rex_unsupported_x_diagnostic}"
  -xobjective-c
  ${CMAKE_CURRENT_SOURCE_DIR}/rex_cmdline_joined_x_c_input.cpp
)
unset(_rex_unsupported_x_diagnostic)

add_rex_cmdline_death_test(
  compiler_options_gnu_stdin_unsupported
  "REX_FRONTEND_INVARIANT[clang-source-path]: stdin input '-' has no stable source identity and is unsupported"
  -x c -
)

add_rex_cmdline_death_test(
  compiler_options_gnu_objc_suffix_unsupported
  "REX_FRONTEND_INVARIANT[clang-language]: Objective-C source '${CMAKE_CURRENT_SOURCE_DIR}/rex_cmdline_unsupported_objc.m' is not supported by REX"
  ${CMAKE_CURRENT_SOURCE_DIR}/rex_cmdline_unsupported_objc.m
)

add_rex_cmdline_death_test(
  compiler_options_removed_ast_merge
  "REX_FRONTEND_INVARIANT[removed-legacy-option]: option '-rose:astMerge' selected the removed AST-merge subsystem and was silently ignored and is not supported"
  -rose:astMerge
)
add_rex_cmdline_death_test(
  compiler_options_removed_ast_colon_merge
  "REX_FRONTEND_INVARIANT[removed-legacy-option]: option '-rose:ast:merge' selected the removed AST-merge subsystem and was silently ignored and is not supported"
  -rose:ast:merge
)
add_rex_cmdline_death_test(
  compiler_options_removed_ast_merge_command_file
  "REX_FRONTEND_INVARIANT[removed-legacy-option]: option '-rose:astMergeCommandFile' configured the removed AST-merge subsystem and was silently ignored and is not supported"
  -rose:astMergeCommandFile removed-ast-merge-command-file.txt
)
add_rex_cmdline_death_test(
  compiler_options_removed_skip_unparse_asm_commands
  "REX_FRONTEND_INVARIANT[removed-legacy-option]: option '-rose:skip_unparse_asm_commands' silently dropped source-written inline assembly and is not supported"
  -rose:skip_unparse_asm_commands
)
add_rex_cmdline_death_test(
  compiler_options_removed_skip_unparse_cc_commands
  "REX_FRONTEND_INVARIANT[removed-legacy-option]: option '-rose:skip_unparse_cc_commands' was a no-op compatibility option and is not supported"
  -rose:skip_unparse_cc_commands
)
add_rex_cmdline_death_test(
  compiler_options_removed_mangled_noshortname
  "REX_FRONTEND_INVARIANT[removed-legacy-option]: option '-rose:mangled:noshortname' was an unsupported no-op mangling option and is not supported"
  -rose:mangled:noshortname
)
add_rex_cmdline_death_test(
  compiler_options_removed_suppress_variable_declaration_normalization
  "REX_FRONTEND_INVARIANT[removed-legacy-option]: option '-rose:suppress_variable_declaration_normalization' was an unparser-formatting compatibility switch; exact source declarator groups are represented by typed AST and is not supported"
  -rose:suppress_variable_declaration_normalization
)

add_test(
  NAME compiler_options_mixed_c_cxx_original_backend
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${BASH_EXECUTABLE} -c
    "set -e; \
     rm -f rex_frontend_mixed_driver rex_frontend_mixed_driver_link_input.o rex_frontend_mixed_driver_link_input.a; \
     \"${CMAKE_C_COMPILER}\" -c \
       ${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_mixed_driver_link_input.c \
       -o rex_frontend_mixed_driver_link_input.o; \
     \"${CMAKE_AR}\" qc rex_frontend_mixed_driver_link_input.a \
       rex_frontend_mixed_driver_link_input.o; \
     $<TARGET_FILE:testFrontEnd> \
       ${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_mixed_driver_c.c \
       rex_frontend_mixed_driver_link_input.o \
       ${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_mixed_driver_cpp.cpp \
       rex_frontend_mixed_driver_link_input.a \
       -o rex_frontend_mixed_driver; \
     ./rex_frontend_mixed_driver"
)

add_test(
  NAME compiler_options_mixed_exact_preprocessed_x_backend
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${BASH_EXECUTABLE} -c
    "set -e; \
     rm -f compiler_options_mixed_exact_preprocessed_x_backend.log; \
     $<TARGET_FILE:testFrontEnd> \
       -x cpp-output ${CMAKE_CURRENT_BINARY_DIR}/rex_cmdline_split_x_c_input \
       -x c++-cpp-output ${CMAKE_CURRENT_BINARY_DIR}/rex_cmdline_split_x_cxx_input \
       -rose:verbose 1 -rose:skipfinalCompileStep \
       > compiler_options_mixed_exact_preprocessed_x_backend.log 2>&1; \
     test \"$(grep -F 'Compile Line:' compiler_options_mixed_exact_preprocessed_x_backend.log | grep -Fo -- '-x cpp-output' | wc -l)\" -eq 3; \
     test \"$(grep -F 'Compile Line:' compiler_options_mixed_exact_preprocessed_x_backend.log | grep -Fo -- '-x c++-cpp-output' | wc -l)\" -eq 3"
)

add_test(
  NAME compiler_options_mixed_suffix_preprocessed_backend
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${BASH_EXECUTABLE} -c
    "set -e; \
     rm -f compiler_options_mixed_suffix_preprocessed_backend.log; \
     $<TARGET_FILE:testFrontEnd> \
       ${CMAKE_CURRENT_BINARY_DIR}/rex_cmdline_suffix_c_input.i \
       ${CMAKE_CURRENT_BINARY_DIR}/rex_cmdline_suffix_cxx_input.ii \
       -rose:verbose 1 -rose:skipfinalCompileStep \
       > compiler_options_mixed_suffix_preprocessed_backend.log 2>&1; \
     test \"$(grep -F 'Compile Line:' compiler_options_mixed_suffix_preprocessed_backend.log | grep -Fo -- '-x cpp-output' | wc -l)\" -eq 1; \
     test \"$(grep -F 'Compile Line:' compiler_options_mixed_suffix_preprocessed_backend.log | grep -Fo -- '-x c++-cpp-output' | wc -l)\" -eq 1; \
     test \"$(grep -F 'Compile Line:' compiler_options_mixed_suffix_preprocessed_backend.log | grep -Fo -- '-x none' | wc -l)\" -eq 2"
)

add_test(
  NAME compiler_options_mixed_suffix_module_backend
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${BASH_EXECUTABLE} -c
    "set -e; \
     rm -f compiler_options_mixed_suffix_module_backend.log; \
     $<TARGET_FILE:testFrontEnd> \
       ${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_mixed_driver_c.c \
       ${CMAKE_CURRENT_BINARY_DIR}/rex_cmdline_suffix_cxx_module_input.cppm \
       -rose:verbose 1 -rose:skipfinalCompileStep \
       > compiler_options_mixed_suffix_module_backend.log 2>&1; \
     test \"$(grep -F 'Compile Line:' compiler_options_mixed_suffix_module_backend.log | grep -Fo -- '-x c++ ' | wc -l)\" -eq 1; \
     test \"$(grep -F 'Compile Line:' compiler_options_mixed_suffix_module_backend.log | grep -Fo -- '-x c++-module' | wc -l)\" -eq 0; \
     test \"$(grep -F 'Compile Line:' compiler_options_mixed_suffix_module_backend.log | grep -Fo -- '-x none' | wc -l)\" -eq 2"
)

if(NOT LLVM_TOOLS_BINARY_DIR OR NOT IS_ABSOLUTE "${LLVM_TOOLS_BINARY_DIR}")
  message(FATAL_ERROR
    "The GNU option tests require the absolute LLVM 22 tool directory from LLVMConfig.cmake")
endif()
set(_compiler_options_clang_driver "")
foreach(_compiler_options_clang_driver_name
    clang-${LLVM_REQUIRED_MAJOR} clang)
  if(EXISTS
      "${LLVM_TOOLS_BINARY_DIR}/${_compiler_options_clang_driver_name}" AND
     NOT IS_DIRECTORY
      "${LLVM_TOOLS_BINARY_DIR}/${_compiler_options_clang_driver_name}")
    set(_compiler_options_clang_driver
      "${LLVM_TOOLS_BINARY_DIR}/${_compiler_options_clang_driver_name}")
    break()
  endif()
endforeach()
if(NOT _compiler_options_clang_driver)
  message(FATAL_ERROR
    "The loaded LLVM ${LLVM_REQUIRED_MAJOR} installation has no Clang driver in ${LLVM_TOOLS_BINARY_DIR}")
endif()
get_filename_component(_compiler_options_clang_driver
  "${_compiler_options_clang_driver}" REALPATH)

execute_process(
  COMMAND "${_compiler_options_clang_driver}" -dumpversion
  OUTPUT_VARIABLE _compiler_options_clang_driver_version
  OUTPUT_STRIP_TRAILING_WHITESPACE
  ERROR_VARIABLE _compiler_options_clang_driver_version_error
  RESULT_VARIABLE _compiler_options_clang_driver_version_result
)
string(REGEX MATCH "^[0-9]+" _compiler_options_clang_driver_major
  "${_compiler_options_clang_driver_version}")
if(NOT _compiler_options_clang_driver_version_result EQUAL 0 OR
   NOT "${_compiler_options_clang_driver_major}" STREQUAL
       "${LLVM_REQUIRED_MAJOR}")
  message(FATAL_ERROR
    "The selected Clang driver is not exact LLVM ${LLVM_REQUIRED_MAJOR}: "
    "${_compiler_options_clang_driver}; version='${_compiler_options_clang_driver_version}'; "
    "error='${_compiler_options_clang_driver_version_error}'")
endif()

execute_process(
  COMMAND "${_compiler_options_clang_driver}" --print-resource-dir
  OUTPUT_VARIABLE _compiler_options_clang_resource_dir
  OUTPUT_STRIP_TRAILING_WHITESPACE
  ERROR_VARIABLE _compiler_options_clang_resource_dir_error
  RESULT_VARIABLE _compiler_options_clang_resource_dir_result
)
if(NOT _compiler_options_clang_resource_dir_result EQUAL 0 OR
   NOT IS_ABSOLUTE "${_compiler_options_clang_resource_dir}" OR
   NOT IS_DIRECTORY "${_compiler_options_clang_resource_dir}" OR
   NOT EXISTS "${_compiler_options_clang_resource_dir}/include/stddef.h")
  message(FATAL_ERROR
    "The exact LLVM ${LLVM_REQUIRED_MAJOR} Clang driver has no complete resource directory: "
    "driver='${_compiler_options_clang_driver}'; resource='${_compiler_options_clang_resource_dir}'; "
    "error='${_compiler_options_clang_resource_dir_error}'")
endif()
get_filename_component(_compiler_options_clang_resource_dir
  "${_compiler_options_clang_resource_dir}" REALPATH)

add_test(
  NAME compiler_options_gnu_split_resource_dir
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${CMAKE_COMMAND} -E env "LLVM_DIR=${LLVM_DIR}"
    ${ROSE_TEST_TRANSLATOR} -rose:verbose 0
    -include ${CMAKE_CURRENT_SOURCE_DIR}/testOptions.h
    -resource-dir ${_compiler_options_clang_resource_dir}
    -rose:skipfinalCompileStep
    -c ${CMAKE_CURRENT_SOURCE_DIR}/test-includeOption.C
)

add_test(
  NAME compiler_options_gnu_joined_resource_dir
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${CMAKE_COMMAND} -E env "LLVM_DIR=${LLVM_DIR}"
    ${ROSE_TEST_TRANSLATOR} -rose:verbose 0
    -include ${CMAKE_CURRENT_SOURCE_DIR}/testOptions.h
    -resource-dir=${_compiler_options_clang_resource_dir}
    -rose:skipfinalCompileStep
    -c ${CMAKE_CURRENT_SOURCE_DIR}/test-includeOption.C
)

add_rex_cmdline_death_test(
  compiler_options_gnu_duplicate_resource_dir
  "REX_FRONTEND_INVARIANT[clang-resource-dir]: command contains more than one explicit resource directory"
  -resource-dir ${_compiler_options_clang_resource_dir}
  -resource-dir=${_compiler_options_clang_resource_dir}
  -c ${CMAKE_CURRENT_SOURCE_DIR}/test-includeOption.C
)
set_tests_properties(compiler_options_gnu_duplicate_resource_dir
  PROPERTIES ENVIRONMENT "LLVM_DIR=${LLVM_DIR}")
unset(_compiler_options_clang_driver)
unset(_compiler_options_clang_driver_major)
unset(_compiler_options_clang_driver_name)
unset(_compiler_options_clang_driver_version)
unset(_compiler_options_clang_driver_version_error)
unset(_compiler_options_clang_driver_version_result)
unset(_compiler_options_clang_resource_dir)
unset(_compiler_options_clang_resource_dir_error)
unset(_compiler_options_clang_resource_dir_result)

add_test(
  NAME rex_wasm_ci_cache_and_mandatory_test_contracts
  COMMAND ${BASH_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_wasm_cache_contract.sh
    ${CMAKE_SOURCE_DIR}
)
set_tests_properties(rex_wasm_ci_cache_and_mandatory_test_contracts
  PROPERTIES LABELS "rex;WASM;CI;death")

add_test(
  NAME compiler_options_gnu_skip_rose
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${BASH_EXECUTABLE} -c
    "set -e; \
     ${ROSE_TEST_TRANSLATOR} -rose:verbose 0 -isystem ${CMAKE_CURRENT_SOURCE_DIR} -c ${CMAKE_CURRENT_SOURCE_DIR}/test-isystemOption.C -rose:skip_rose; \
     ${ROSE_TEST_TRANSLATOR} -w -c ${CMAKE_SOURCE_DIR}/tests/nonsmoke/functional/CompileTests/C_tests/test2006_135.c -rose:skip_rose"
)
