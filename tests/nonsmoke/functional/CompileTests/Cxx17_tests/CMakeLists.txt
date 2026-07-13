set(CXX17_TESTCODES
  test2018_01.C
  test2018_04.C
  test2018_19.C
  test2018_02.C
  test2018_05.C
  test2018_06.C
  test2018_07.C
  test2018_08.C
  test2018_09.C
  test2018_10.C
  test2018_11.C
  test2018_13.C
  test2018_14.C
  test2018_15.C
  test2018_16.C
  test2018_17.C
  test2018_18.C
  test2018_20.C
  test2018_21.C
  test2018_22.C
  test2018_23.C
  test2018_24.C
  test2018_25.C
  test2018_26.C
  test2018_27.C
  test2018_28.C
  test2018_29.C
  test2018_30.C
  test2018_31.C
  test2018_32.C
  test2018_33.C
  test2018_34.C
  test2018_35.C
  test2018_36.C
  test2018_37.C
  test2018_38.C
  test2018_39.C
  test2018_40.C
  test2018_41.C
  test2018_42.C
  test2018_43.C
  test2018_44.C
  test2018_45.C
  test2018_46.C
  test2018_47.C
  test2018_48.C
  test2018_49.C
  test2018_50.C
  test2018_51.C
  test2018_52.C
  test2018_53.C
  test2018_54.C
  test2018_55.C
  test2018_56.C
  test2018_57.C
  test2018_58.C
  test2018_59.C
  test2018_60.C
  test2018_61.C
  test2018_62.C
  test2018_63.C
  test2018_64.C
  test2018_65.C
  test2018_66.C
  test2018_67.C
  test2018_68.C
  test2018_69.C
  test2018_70.C
  test2018_71.C
  test2018_72.C
  test2018_73.C
  rex_exact_special_expression_result_types.cpp
  rex_frontend_recursive_default_argument_publication.cpp
  test2018_03.C
  test2018_12.C
)

set(ROSE_FLAGS -rose:verbose 0 -std=c++17)

rex_register_compile_source_set(REX_CXX17_TESTS_SOURCES ${CXX17_TESTCODES})

set(CXX17_NEGATIVE_TESTCODES
  # Clang 22 rejects the original because the inherited constructor use is
  # ambiguous. Keep it registered as negative C++17 frontend coverage.
  test2018_67.C
)
rex_register_compile_source_set(REX_CXX17_TESTS_NEGATIVE_SOURCES
  ${CXX17_NEGATIVE_TESTCODES}
)
set(CXX17_POSITIVE_TESTCODES ${CXX17_TESTCODES})
list(REMOVE_ITEM CXX17_POSITIVE_TESTCODES ${CXX17_NEGATIVE_TESTCODES})

foreach(file_to_test ${CXX17_POSITIVE_TESTCODES})
  compile_test(${file_to_test} "Cxx17_tests")
endforeach()

foreach(file_to_test ${CXX17_NEGATIVE_TESTCODES})
  compile_negative_test(${file_to_test} "Cxx17_tests")
endforeach()

add_executable(
  rex_exact_special_expression_result_types_translator
  rex_exact_special_expression_result_types_translator.cpp
)
target_link_libraries(
  rex_exact_special_expression_result_types_translator
  ROSE_DLL
  ${link_with_libraries}
)

add_test(
  NAME Cxx17_tests_rex_exact_special_expression_result_types_ast_json
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND
    $<TARGET_FILE:rex_exact_special_expression_result_types_translator>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep
    -rex:ast-json-checkpoint=pre-omp-construction
    -rex:ast-json-dir=rex_exact_special_expression_result_types_json
    ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_exact_special_expression_result_types.cpp
)
set_tests_properties(
  Cxx17_tests_rex_exact_special_expression_result_types_ast_json
  PROPERTIES LABELS "Cxx17_tests;rex;CFE;ASTJSON"
)
rex_lock_test_for_input(
  Cxx17_tests_rex_exact_special_expression_result_types_ast_json
  rex_exact_special_expression_result_types.cpp
)

add_test(
  NAME Cxx17_tests_rex_frontend_recursive_default_argument_publication_structure
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND
    $<TARGET_FILE:rex_frontend_function_publication_contract_translator>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep -rose:skip_unparse
    ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_recursive_default_argument_publication.cpp
)
set_tests_properties(
  Cxx17_tests_rex_frontend_recursive_default_argument_publication_structure
  PROPERTIES LABELS "Cxx17_tests;rex;CFE"
)
rex_lock_test_for_input(
  Cxx17_tests_rex_frontend_recursive_default_argument_publication_structure
  rex_frontend_recursive_default_argument_publication.cpp
)
