set(_netcdf_nc_test4_tests
  bm_file
  cdm_sea_soundings
  tst_large
  tst_ar4
  tst_ar4_3d
  tst_ar4_4d
  tst_create_files
  tst_mpi_parallel
  tst_nc4perf
  tst_parallel
  tst_parallel2
  tst_parallel3
  tst_parallel4
  run_bm_test1.sh
  run_bm_elena.sh
  run_bm_test2.sh
  run_tst_chunks.sh
  run_bm_ar4.sh
  run_par_bm_test.sh
  run_par_test.sh
  run_pnetcdf_test.sh
)

foreach(test IN LISTS _netcdf_nc_test4_tests)
  string(REPLACE "." "_" _name "${test}")
  string(REPLACE "-" "_" _name "${_name}")
  if(test MATCHES "\\\\.sh$")
    add_test(
      NAME netcdf_nc_test4_${_name}
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      COMMAND ${CMAKE_COMMAND} -E env
        srcdir=${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/${test}
    )
  else()
    add_test(
      NAME netcdf_nc_test4_${_name}
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      COMMAND ${CMAKE_CURRENT_BINARY_DIR}/${test}
    )
  endif()
endforeach()

rex_add_disabled_test(
  NAME netcdf_nc_test4_tst_chunks
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${CMAKE_CURRENT_BINARY_DIR}/tst_chunks
)
