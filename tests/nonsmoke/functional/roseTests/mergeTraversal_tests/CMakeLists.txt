
include_directories(${ROSE_INCLUDES})


########### next target ###############

add_executable(traversalArray traversalArray.C)
target_link_libraries(traversalArray ROSE_DLL ${link_with_libraries})

install(TARGETS traversalArray DESTINATION bin)

set(_rose_include_flags)
foreach(entry IN LISTS ROSE_INCLUDES)
  if(entry MATCHES "^-I" OR entry MATCHES "^-isystem")
    list(APPEND _rose_include_flags ${entry})
  else()
    list(APPEND _rose_include_flags -I${entry})
  endif()
endforeach()

add_test(
  NAME merge_traversal_input
  COMMAND traversalArray -rose:verbose 2 -I${CMAKE_BINARY_DIR}
    ${_rose_include_flags}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/input_traversalArray.C
)

add_test(
  NAME merge_traversal_cxx_grammar
  COMMAND traversalArray -rose:verbose 2 -I${CMAKE_BINARY_DIR}
    ${_rose_include_flags}
    -c ${CMAKE_BINARY_DIR}/src/frontend/SageIII/Cxx_Grammar.C
)


########### install files ###############

# Makefile check targets.
add_test(
  NAME nonsmoke_functional_roseTests_mergeTraversal_tests_check-local
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=merge_traversal_cxx_grammar\;merge_traversal_input
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_roseTests_mergeTraversal_tests_check-local PROPERTIES
  DEPENDS "merge_traversal_cxx_grammar;merge_traversal_input"
)
