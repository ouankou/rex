add_subdirectory(fregrid)
add_subdirectory(make_coupler_mosaic)
add_subdirectory(make_hgrid)
add_subdirectory(make_solo_mosaic)
add_subdirectory(make_topog)
add_subdirectory(make_vgrid)
add_subdirectory(river_regrid)
add_subdirectory(shared)
add_subdirectory(transfer_to_mosaic_grid)
set(_netcdf_gridspec_tools_tests
  get_data.sh
  test.sh
  test_all.sh
)

foreach(test IN LISTS _netcdf_gridspec_tools_tests)
  string(REPLACE "." "_" _name "${test}")
  string(REPLACE "-" "_" _name "${_name}")
  add_test(
    NAME netcdf_gridspec_tools_${_name}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${CMAKE_COMMAND} -E env
      srcdir=${CMAKE_CURRENT_SOURCE_DIR}
      ${CMAKE_CURRENT_SOURCE_DIR}/${test}
  )
endforeach()
