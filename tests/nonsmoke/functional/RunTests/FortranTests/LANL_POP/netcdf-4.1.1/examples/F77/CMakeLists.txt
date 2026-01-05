set(_netcdf_examples_f77_tests
  simple_xy_wr
  simple_xy_rd
  sfc_pres_temp_wr
  sfc_pres_temp_rd
  pres_temp_4D_wr
  pres_temp_4D_rd
  simple_xy_nc4_wr
  simple_xy_nc4_rd
  do_comps.sh
)

foreach(test IN LISTS _netcdf_examples_f77_tests)
  string(REPLACE "." "_" _name "${test}")
  string(REPLACE "-" "_" _name "${_name}")
  if(test MATCHES "\\\\.sh$")
    add_test(
      NAME netcdf_examples_f77_${_name}
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      COMMAND ${CMAKE_COMMAND} -E env
        srcdir=${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/${test}
    )
  else()
    add_test(
      NAME netcdf_examples_f77_${_name}
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      COMMAND ${CMAKE_CURRENT_BINARY_DIR}/${test}
    )
  endif()
endforeach()
