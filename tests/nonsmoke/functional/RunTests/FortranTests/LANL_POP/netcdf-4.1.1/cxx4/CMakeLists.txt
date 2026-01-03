set(_netcdf_cxx4_tests
  test_group
  test_dim
  test_att
  test_var
  test_var2
  test_type
)

foreach(test IN LISTS _netcdf_cxx4_tests)
  string(REPLACE "." "_" _name "${test}")
  string(REPLACE "-" "_" _name "${_name}")
  add_test(
    NAME netcdf_cxx4_${_name}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/${test}
  )
endforeach()
