include_directories(${ROSE_INCLUDES})

set(_slice_tools
  multiPassTest
  generateSDG
  generateSlicedSDG
  generateCDG
  generateDDG
  testSlicing
  generateSFBDT
  generateSFCFG
  generateSFDF
  generateSFDT
  slice
)

set(_slice_chain_tools
  multiPassTest
  generateSDG
  generateCDG
  generateDDG
  generateSFBDT
  generateSFCFG
  generateSFDF
  generateSFDT
)

add_executable(multiPassTest multiPassTest.C)
target_link_libraries(multiPassTest ROSE_DLL ${link_with_libraries})

add_executable(generateSDG generateSDG.C)
target_link_libraries(generateSDG ROSE_DLL ${link_with_libraries})

add_executable(generateSlicedSDG generateSlicedSDG.C)
target_link_libraries(generateSlicedSDG ROSE_DLL ${link_with_libraries})

add_executable(generateCDG generateCDG.C)
target_link_libraries(generateCDG ROSE_DLL ${link_with_libraries})

add_executable(generateDDG generateDDG.C)
target_link_libraries(generateDDG ROSE_DLL ${link_with_libraries})

add_executable(testSlicing testSlicing.C)
target_link_libraries(testSlicing ROSE_DLL ${link_with_libraries})

add_executable(generateSFBDT generateSFBDT.C)
target_link_libraries(generateSFBDT ROSE_DLL ${link_with_libraries})

add_executable(generateSFCFG generateSFCFG.C)
target_link_libraries(generateSFCFG ROSE_DLL ${link_with_libraries})

add_executable(generateSFDF generateSFDF.C)
target_link_libraries(generateSFDF ROSE_DLL ${link_with_libraries})

add_executable(generateSFDT generateSFDT.C)
target_link_libraries(generateSFDT ROSE_DLL ${link_with_libraries})

add_executable(slice slice.C)
target_link_libraries(slice ROSE_DLL ${link_with_libraries})

set(_slice_specimens
  test1.C
  test2.C
  test3.C
  test4.C
)

foreach(tool IN LISTS _slice_tools)
  foreach(specimen IN LISTS _slice_specimens)
    string(REPLACE "." "_" _name ${specimen})
    add_test(
      NAME slicing_${tool}_${_name}
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      COMMAND ${tool} -c ${CMAKE_CURRENT_SOURCE_DIR}/${specimen}
        -o ${tool}_${specimen}.o
    )
  endforeach()
endforeach()

# Makefile TEST_Objects targets.
set(_slice_chain_tests)
foreach(specimen IN LISTS _slice_specimens)
  get_filename_component(_specimen_base ${specimen} NAME_WE)
  set(_chain_commands)
  foreach(tool IN LISTS _slice_chain_tools)
    list(APPEND _chain_commands
      "$<TARGET_FILE:${tool}> -c ${CMAKE_CURRENT_SOURCE_DIR}/${specimen} -o ${_specimen_base}"
    )
  endforeach()
  string(JOIN " && " _chain_cmd ${_chain_commands})
  set(_chain_test "nonsmoke_functional_roseTests_programAnalysisTests_staticInterproceduralSlicingTests_${specimen}")
  add_test(
    NAME ${_chain_test}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND sh -c "${_chain_cmd}"
  )
  list(APPEND _slice_chain_tests ${_chain_test})
endforeach()

# Makefile check targets.
add_test(
  NAME nonsmoke_functional_roseTests_programAnalysisTests_staticInterproceduralSlicingTests_check-local
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=${_slice_chain_tests}
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_roseTests_programAnalysisTests_staticInterproceduralSlicingTests_check-local PROPERTIES DEPENDS "${_slice_chain_tests}" )
