if(ENABLE-FORTRAN-OFP)
  add_executable(testBafio testBafio.C)
  target_link_libraries(testBafio ROSE_DLL ${link_with_libraries})

  set(_rth ${CMAKE_SOURCE_DIR}/scripts/rth_run.pl)
  set(_exit_status ${CMAKE_SOURCE_DIR}/scripts/test_exit_status)

  add_test(
    NAME smoke_fortran_testBafio
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${_rth}
      "TITLE=Fortran parser test [testBafio.passed]"
      "CMD=$<TARGET_FILE:testBafio>"
      ${_exit_status}
      testBafio.passed
  )
  set_tests_properties(smoke_fortran_testBafio PROPERTIES LABELS "smoke;fortran")
endif()
