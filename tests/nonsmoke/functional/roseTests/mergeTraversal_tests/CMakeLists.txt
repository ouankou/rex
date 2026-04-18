
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
    -std=c++${CMAKE_CXX_STANDARD}
    -c ${CMAKE_BINARY_DIR}/src/frontend/SageIII/Cxx_Grammar.C
)

########### install files ###############
