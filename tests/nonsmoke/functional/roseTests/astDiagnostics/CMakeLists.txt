add_executable(astDiagnostics_RunAllTests RunAllTests.C)
target_link_libraries(astDiagnostics_RunAllTests ROSE_DLL ${link_with_libraries})

set(_rth ${CMAKE_SOURCE_DIR}/scripts/rth_run.pl)
set(_exit_status ${CMAKE_SOURCE_DIR}/scripts/test_exit_status)

add_test(
  NAME astDiagnostics_test2010_2
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${_rth}
    "TITLE=RunAllTests test2010_2.C [astDiagnostics_test2010_2.passed]"
    "USE_SUBDIR=yes"
    "CMD=$<TARGET_FILE:astDiagnostics_RunAllTests> -rose:verbose 0 -c ${CMAKE_CURRENT_SOURCE_DIR}/test2010_2.C"
    ${_exit_status}
    astDiagnostics_test2010_2.passed
)

install(TARGETS astDiagnostics_RunAllTests DESTINATION bin)

