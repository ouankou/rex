include_directories(${ROSE_INCLUDES})

add_executable(traversalArray traversalArray.C)
target_link_libraries(traversalArray ROSE_DLL ${link_with_libraries})

install(TARGETS traversalArray DESTINATION bin)

add_test(
  NAME merge_traversal_input
  COMMAND traversalArray -rose:verbose 2 -I${CMAKE_BINARY_DIR}
    ${_functional_include_flags}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/input_traversalArray.C
)

add_test(
  NAME merge_traversal_cxx_grammar
  COMMAND traversalArray -rose:verbose 2 -I${CMAKE_BINARY_DIR}
    ${_functional_include_flags}
    -std=c++${CMAKE_CXX_STANDARD}
    -c ${CMAKE_BINARY_DIR}/src/frontend/SageIII/Cxx_Grammar.C
)

add_test(
  NAME rex_merge_traversal_memcheck
  COMMAND traversalArray -I${CMAKE_BINARY_DIR}
    ${_functional_include_flags}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_merge_traversal_memcheck.cpp
)
