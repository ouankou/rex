include_directories(${ROSE_INCLUDES})

set(ROSE_INCLUDE_FLAGS "")
foreach(include ${ROSE_INCLUDES})
  list(APPEND ROSE_INCLUDE_FLAGS "-I${include}")
endforeach()

set(ROSE_TEST_TRANSLATOR $<TARGET_FILE:testTranslator>)

find_program(BASH_EXECUTABLE bash)
if(NOT BASH_EXECUTABLE)
  message(FATAL_ERROR "bash not found; CompilerOptionsTests require bash")
endif()

add_subdirectory(A++Code)
add_subdirectory(collectAllCommentsAndDirectives_tests)
add_subdirectory(preinclude_tests)
add_subdirectory(testAnsiOption)
add_subdirectory(testCpreprocessorOption)
add_subdirectory(testFileNamesAndExtensions)
add_subdirectory(testForSpuriousOutput)
add_subdirectory(testGenerateSourceFileNames)
add_subdirectory(testGnuOptions)
add_subdirectory(testHeaderFileOutput)
add_subdirectory(testIncludeOptions)
add_subdirectory(testNostdincOption)
add_subdirectory(testOutputFileOption)

# Makefile check targets.
add_test(
  NAME nonsmoke_functional_CompilerOptionsTests_check-local
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=compiler_options_output_file
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_CompilerOptionsTests_check-local PROPERTIES
  DEPENDS "compiler_options_output_file"
)
