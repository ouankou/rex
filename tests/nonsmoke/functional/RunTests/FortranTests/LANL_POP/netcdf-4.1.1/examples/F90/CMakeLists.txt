set(_netcdf_examples_f90_tests
  do_comps.sh
  run_nc4_comps.sh
  run_f90_par_examples.sh
  simple_xy_wr
  simple_xy_rd
  sfc_pres_temp_wr
  sfc_pres_temp_rd
  pres_temp_4D_wr
  pres_temp_4D_rd
  nc4_simple_xy_wr
  nc4_sfc_pres_temp_wr
  nc4_pres_temp_4D_wr
  simple_xy_par_wr
  simple_xy_par_rd
  simple_xy_nc4_wr
  simple_xy_nc4_rd
)

foreach(test IN LISTS _netcdf_examples_f90_tests)
  string(REPLACE "." "_" _name "${test}")
  string(REPLACE "-" "_" _name "${_name}")
  if(test MATCHES "\\\\.sh$")
    add_test(
      NAME netcdf_examples_f90_${_name}
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      COMMAND ${CMAKE_COMMAND} -E env
        srcdir=${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/${test}
    )
  else()
    add_test(
      NAME netcdf_examples_f90_${_name}
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      COMMAND ${CMAKE_CURRENT_BINARY_DIR}/${test}
    )
  endif()
endforeach()
