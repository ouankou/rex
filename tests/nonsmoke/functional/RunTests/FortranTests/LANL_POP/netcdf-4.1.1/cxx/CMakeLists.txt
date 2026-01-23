set(_netcdf_cxx_tests
  nctst
  tst_failure
  run_nc_tests.sh
  run_nc4_tests.sh
)

foreach(test IN LISTS _netcdf_cxx_tests)
  string(REPLACE "." "_" _name "${test}")
  string(REPLACE "-" "_" _name "${_name}")
  if(test MATCHES "\\\\.sh$")
    add_test(
      NAME netcdf_cxx_${_name}
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      COMMAND ${CMAKE_COMMAND} -E env
        srcdir=${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/${test}
    )
  else()
    add_test(
      NAME netcdf_cxx_${_name}
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      COMMAND ${CMAKE_CURRENT_BINARY_DIR}/${test}
    )
  endif()
endforeach()
