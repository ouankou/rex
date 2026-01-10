
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

add_subdirectory(AstDeleteTests)
add_subdirectory(FortranTests)


########### install files ###############

# Makefile check targets.
add_test(
  NAME nonsmoke_functional_RunTests_check-local
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=RunTests_traverseCommonBlock
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_RunTests_check-local PROPERTIES DEPENDS RunTests_traverseCommonBlock )
