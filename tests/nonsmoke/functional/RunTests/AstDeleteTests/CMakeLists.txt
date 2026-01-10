
include_directories(${ROSE_INCLUDES})


########### next target ###############

add_executable(memoryTranslator test.C)
target_link_libraries(memoryTranslator ROSE_DLL ${link_with_libraries})

install(TARGETS memoryTranslator DESTINATION bin)

add_test(
  NAME ast_delete_memory_translator
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND memoryTranslator -rose:verbose 0 -c
    ${CMAKE_CURRENT_SOURCE_DIR}/test1.C
)

add_test(
  NAME nonsmoke_functional_RunTests_AstDeleteTests_test1.C
  COMMAND testTranslator -c ${CMAKE_CURRENT_SOURCE_DIR}/test1.C
)
set_tests_properties(nonsmoke_functional_RunTests_AstDeleteTests_test1.C
  PROPERTIES DEPENDS ast_delete_memory_translator)


########### install files ###############

# Makefile check targets.
add_test(
  NAME nonsmoke_functional_RunTests_AstDeleteTests_check-local
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=ast_delete_memory_translator
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_RunTests_AstDeleteTests_check-local PROPERTIES
  DEPENDS "ast_delete_memory_translator"
)
