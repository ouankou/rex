set(_netcdf_libcf_src_tests
  tst_coords
  tst_files
  tst_vars
  tst_cvars
  tst_vert
  tst_subsets
  tst_f03_file
  tst_example_data
)

foreach(test IN LISTS _netcdf_libcf_src_tests)
  string(REPLACE "." "_" _name "${test}")
  string(REPLACE "-" "_" _name "${_name}")
  add_test(
    NAME netcdf_libcf_src_${_name}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/${test}
  )
endforeach()
