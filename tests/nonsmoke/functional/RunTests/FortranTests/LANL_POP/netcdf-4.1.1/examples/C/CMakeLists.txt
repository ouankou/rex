set(_netcdf_examples_c_tests
  simple_xy_wr
  simple_xy_rd
  sfc_pres_temp_wr
  sfc_pres_temp_rd
  pres_temp_4D_wr
  pres_temp_4D_rd
  simple_nc4_wr
  simple_nc4_rd
  simple_xy_nc4_wr
  simple_xy_nc4_rd
)

foreach(test IN LISTS _netcdf_examples_c_tests)
  string(REPLACE "." "_" _name "${test}")
  string(REPLACE "-" "_" _name "${_name}")
  add_test(
    NAME netcdf_examples_c_${_name}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/${test}
  )
endforeach()
