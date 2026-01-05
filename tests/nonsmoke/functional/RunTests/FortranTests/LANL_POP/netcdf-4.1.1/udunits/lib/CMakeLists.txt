add_subdirectory(xmlFailures)
add_subdirectory(xmlSuccesses)
set(_netcdf_udunits_tests
  testUnits
  testUnits-1
)

foreach(test IN LISTS _netcdf_udunits_tests)
  string(REPLACE "." "_" _name "${test}")
  string(REPLACE "-" "_" _name "${_name}")
  add_test(
    NAME netcdf_udunits_${_name}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/${test}
  )
endforeach()
