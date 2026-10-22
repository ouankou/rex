add_executable(tools_classMemberVariablesInLambdas_exe
               classMemberVariablesInLambdas.C)

target_link_libraries(tools_classMemberVariablesInLambdas_exe PRIVATE ROSE_DLL)

target_include_directories(
  tools_classMemberVariablesInLambdas_exe PRIVATE ${ROSE_INCLUDES})

set_target_properties(tools_classMemberVariablesInLambdas_exe PROPERTIES OUTPUT_NAME "classMemberVariablesInLambdas")
install(TARGETS tools_classMemberVariablesInLambdas_exe DESTINATION bin)

add_dependencies(tools tools_classMemberVariablesInLambdas_exe)

if(ENABLE-TESTS-DIRECTORY)
  add_test(NAME rex_tool_class_member_lambda
    COMMAND tools_classMemberVariablesInLambdas_exe
      -rose:skipfinalCompileStep -std=c++11 -c
      ${ROSE_TOP_SRC_DIR}/tests/nonsmoke/functional/CompileTests/Cxx11_tests/classMemberVariablesInLambdas_test_01.C)
  set_tests_properties(rex_tool_class_member_lambda
    PROPERTIES LABELS "rex;tools;AST")
endif()
