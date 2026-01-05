
include_directories(${KDE4_INCLUDES} ${KDE4_INCLUDE_DIR} ${QT_INCLUDES})
include_directories(${ROSE_INCLUDES})

add_executable(traverseCommonBlock traverseCommonBlock.C)
target_link_libraries(traverseCommonBlock ROSE_DLL ${link_with_libraries})

add_test(
  NAME RunTests_traverseCommonBlock
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND $<TARGET_FILE:traverseCommonBlock> -c -rose:verbose 0 -g3
    ${CMAKE_CURRENT_SOURCE_DIR}/inputtraverseCommonBlock.f
)
set_tests_properties(RunTests_traverseCommonBlock PROPERTIES LABELS RunTests)
if(NOT ENABLE-FORTRAN)
  set_tests_properties(RunTests_traverseCommonBlock PROPERTIES DISABLED TRUE)
endif()

add_subdirectory(A++Tests)
add_subdirectory(AstDeleteTests)
add_subdirectory(FortranTests)
add_subdirectory(P++Tests)


########### install files ###############
