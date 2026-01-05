set(_netcdf_libsrc4_tests
  tst_h_files
  tst_h_files2
  tst_h_atts
  tst_h_atts3
  tst_h_vars
  tst_h_vars2
  tst_h_vars3
  tst_h_grps
  tst_h_compounds
  tst_h_compounds2
  tst_h_wrt_cmp
  tst_h_rd_cmp
  tst_h_vl
  tst_h_opaques
  tst_h_strings
  tst_h_strings1
  tst_h_dimscales
  tst_h_dimscales1
  tst_h_dimscales2
  tst_h_dimscales3
  tst_h_enums
  tst_lists
  tst_dims
  tst_dims2
  tst_dims3
  tst_files
  tst_files4
  tst_vars
  tst_varms
  tst_unlim_vars
  tst_atts
  tst_atts2
  tst_converts
  tst_converts2
  tst_grps
  tst_compounds
  tst_compounds2
  tst_compounds3
  tst_vl
  tst_h_vl2
  tst_opaques
  tst_strings
  tst_interops
  tst_interops4
  tst_enums
  tst_vars2
  tst_vars3
  tst_chunks
  tst_coords
  tst_coords2
  tst_coords3
  tst_utf8
  tst_fills
  tst_fillbug
  tst_v2
  tst_xplatform
  tst_xplatform2
  tst_h_atts2
  tst_endian_fill
  tst_interops2
  run_get_hdf4_files.sh
  tst_interops3
  run_par_tests.sh
  tst_files2
  tst_files3
  tst_ar5
  tst_h_files3
  run_valgrind_tests.sh
  run_hdf4_valgrind_tests.sh
)

foreach(test IN LISTS _netcdf_libsrc4_tests)
  string(REPLACE "." "_" _name "${test}")
  string(REPLACE "-" "_" _name "${_name}")
  if(test MATCHES "\\\\.sh$")
    add_test(
      NAME netcdf_libsrc4_${_name}
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      COMMAND ${CMAKE_COMMAND} -E env
        srcdir=${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/${test}
    )
  else()
    add_test(
      NAME netcdf_libsrc4_${_name}
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      COMMAND ${CMAKE_CURRENT_BINARY_DIR}/${test}
    )
  endif()
endforeach()
