set(_netcdf_libcf_cfcheck_tests
  tst_cfcheck.sh
)

foreach(test IN LISTS _netcdf_libcf_cfcheck_tests)
  string(REPLACE "." "_" _name "${test}")
  string(REPLACE "-" "_" _name "${_name}")
  add_test(
    NAME netcdf_libcf_cfcheck_${_name}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${CMAKE_COMMAND} -E env
      srcdir=${CMAKE_CURRENT_SOURCE_DIR}
      ${CMAKE_CURRENT_SOURCE_DIR}/${test}
  )
endforeach()
