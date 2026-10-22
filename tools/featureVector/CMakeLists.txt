add_executable(tools_featureVector_exe featureVector.cpp)

target_link_libraries(tools_featureVector_exe PRIVATE ROSE_DLL)

target_include_directories(
  tools_featureVector_exe
  PRIVATE ${ROSE_INCLUDES})

set_target_properties(tools_featureVector_exe PROPERTIES OUTPUT_NAME
                                                         "featureVector")

install(TARGETS tools_featureVector_exe DESTINATION bin)

add_dependencies(tools tools_featureVector_exe)

if(ENABLE-TESTS-DIRECTORY)
  add_test(NAME rex_tool_feature_vector
    COMMAND tools_featureVector_exe -std=c++11
      ${ROSE_TOP_SRC_DIR}/tests/nonsmoke/functional/CompileTests/Cxx11_tests/globalVariablesInLambdas_test_01.C)
  set_tests_properties(rex_tool_feature_vector
    PROPERTIES LABELS "rex;tools;AST")
endif()
