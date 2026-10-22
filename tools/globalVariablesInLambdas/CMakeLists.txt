add_executable(tools_globalVariablesInLambdas_exe
               globalVariablesInLambdas.C)

target_link_libraries(tools_globalVariablesInLambdas_exe PRIVATE ROSE_DLL)

target_include_directories(
  tools_globalVariablesInLambdas_exe
  PRIVATE ${ROSE_INCLUDES})

set_target_properties(tools_globalVariablesInLambdas_exe
                      PROPERTIES OUTPUT_NAME "globalVariablesInLambdas")

install(TARGETS tools_globalVariablesInLambdas_exe DESTINATION bin)

add_dependencies(tools tools_globalVariablesInLambdas_exe)

if(ENABLE-TESTS-DIRECTORY)
  add_test(NAME rex_tool_global_variable_lambda
    COMMAND tools_globalVariablesInLambdas_exe
      -rose:skipfinalCompileStep -std=c++11 -c
      ${ROSE_TOP_SRC_DIR}/tests/nonsmoke/functional/CompileTests/Cxx11_tests/globalVariablesInLambdas_test_01.C)
  set_tests_properties(rex_tool_global_variable_lambda
    PROPERTIES LABELS "rex;tools;AST")
endif()
