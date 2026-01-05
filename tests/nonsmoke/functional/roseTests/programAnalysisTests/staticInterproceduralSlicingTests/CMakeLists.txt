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
