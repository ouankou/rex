include_directories(${ROSE_INCLUDES})

set(minimal_input_code "")
if(ENABLE-C)
  set(minimal_input_code ${ROSE_TOP_SRC_DIR}/tests/nonsmoke/functional/input_codes/minimal/minimal.cpp)
elseif(ENABLE-FORTRAN)
  set(minimal_input_code ${ROSE_TOP_SRC_DIR}/tests/nonsmoke/functional/input_codes/minimal/minimal.f90)
endif()

set(UTILITY_TESTS
    testCopyFile
    testFileNameClassifier
    filenameSupport
    directorySupport)

foreach(test_target IN LISTS UTILITY_TESTS)
  add_executable(${test_target} ${test_target}.C)
  target_link_libraries(${test_target} ROSE_DLL ${link_with_libraries})
endforeach()

set(_rth ${CMAKE_SOURCE_DIR}/scripts/rth_run.pl)
set(_exit_status ${CMAKE_SOURCE_DIR}/scripts/test_exit_status)

add_test(
  NAME nonsmoke_utility_testCopyFile
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${_rth}
    TITLE="copying files [testCopyFile.passed]"
    USE_SUBDIR=yes
    CMD="$<TARGET_FILE:testCopyFile>"
    ${_exit_status}
    testCopyFile.passed)

add_test(
  NAME nonsmoke_utility_testFileNameClassifier
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${_rth}
    TITLE="file name classification [testFileNameClassifier.passed]"
    USE_SUBDIR=yes
    CMD="$<TARGET_FILE:testFileNameClassifier>"
    ${_exit_status}
    testFileNameClassifier.passed)

add_test(
  NAME nonsmoke_utility_filenameSupport
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${_rth}
    TITLE="file name support [filenameSupport.passed]"
    CMD="$<TARGET_FILE:filenameSupport>"
    ${_exit_status}
    filenameSupport.passed)

set(_utility_ctest_names
  nonsmoke_utility_testCopyFile
  nonsmoke_utility_testFileNameClassifier
  nonsmoke_utility_filenameSupport)

if(minimal_input_code)
  add_test(
    NAME nonsmoke_utility_directorySupport
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${_rth}
      TITLE="directory support [directorySupport.passed]"
      CMD="$<TARGET_FILE:directorySupport> -c -rose:verbose 0 ${minimal_input_code}"
      ${_exit_status}
      directorySupport.passed)
  list(APPEND _utility_ctest_names nonsmoke_utility_directorySupport)
else()
  message(STATUS "Skipping nonsmoke functional Utility directorySupport (no enabled language for minimal input code)")
endif()

if(_utility_ctest_names)
  set_tests_properties(${_utility_ctest_names} PROPERTIES LABELS "nonsmoke;utility")
endif()

# Makefile check targets.
add_test(
  NAME nonsmoke_functional_Utility_check-local
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=nonsmoke_utility_directorySupport\;nonsmoke_utility_filenameSupport\;nonsmoke_utility_testCopyFile\;nonsmoke_utility_testFileNameClassifier
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_Utility_check-local PROPERTIES DEPENDS "nonsmoke_utility_directorySupport;nonsmoke_utility_filenameSupport;nonsmoke_utility_testCopyFile;nonsmoke_utility_testFileNameClassifier" )
