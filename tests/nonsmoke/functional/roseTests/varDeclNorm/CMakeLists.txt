include_directories(${ROSE_INCLUDES})

set(_rth ${CMAKE_SOURCE_DIR}/scripts/rth_run.pl)
set(_exit_status ${CMAKE_SOURCE_DIR}/scripts/test_exit_status)

set(_rose_include_flags)
foreach(entry IN LISTS ROSE_INCLUDES)
  if(entry MATCHES "^-I" OR entry MATCHES "^-isystem")
    list(APPEND _rose_include_flags ${entry})
  else()
    list(APPEND _rose_include_flags -I${entry})
  endif()
endforeach()

set(_var_decl_specimens
  int_global_scope.C
  int_function_scope.C
  int_struct_scope.C
  mix_global_scope.C
  nonreal_type_template_class_scope.C
)

set(_var_decl_flags
  -rose:suppress_variable_declaration_normalization
  -rose:skipfinalCompileStep
  -w
  -rose:verbose 0)

foreach(specimen IN LISTS _var_decl_specimens)
  set(_target ${specimen}.passed)
  add_test(
    NAME varDeclNorm_${specimen}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${_rth}
      TITLE="varDeclNorm [${specimen}]"
      CMD="$<TARGET_FILE:testTranslator> ${_var_decl_flags} ${_rose_include_flags} -I${CMAKE_CURRENT_SOURCE_DIR} -c ${CMAKE_CURRENT_SOURCE_DIR}/${specimen}"
      ${_exit_status}
      ${_target}
  )
endforeach()

# Makefile check targets.
add_test(
  NAME nonsmoke_functional_roseTests_varDeclNorm_check-local
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=varDeclNorm_int_function_scope.C\;varDeclNorm_int_global_scope.C\;varDeclNorm_int_struct_scope.C\;varDeclNorm_mix_global_scope.C\;varDeclNorm_nonreal_type_template_class_scope.C
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_roseTests_varDeclNorm_check-local PROPERTIES DEPENDS "varDeclNorm_int_function_scope.C;varDeclNorm_int_global_scope.C;varDeclNorm_int_struct_scope.C;varDeclNorm_mix_global_scope.C;varDeclNorm_nonreal_type_template_class_scope.C" )
