set(_netcdf_examples_cdl_tests
  create_sample_files.sh
  do_comps.sh
)

foreach(test IN LISTS _netcdf_examples_cdl_tests)
  string(REPLACE "." "_" _name "${test}")
  string(REPLACE "-" "_" _name "${_name}")
  add_test(
    NAME netcdf_examples_cdl_${_name}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${CMAKE_COMMAND} -E env
      srcdir=${CMAKE_CURRENT_SOURCE_DIR}
      ${CMAKE_CURRENT_SOURCE_DIR}/${test}
  )
endforeach()
