add_subdirectory(cfcheck)
add_subdirectory(gridspec)
add_subdirectory(src)

# Makefile check targets.
add_test(
  NAME nonsmoke_functional_RunTests_FortranTests_LANL_POP_netcdf-4.1.1_libcf_check_install
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=netcdf_libcf_src_tst_coords\;netcdf_libcf_src_tst_cvars\;netcdf_libcf_src_tst_example_data\;netcdf_libcf_src_tst_f03_file\;netcdf_libcf_src_tst_files\;netcdf_libcf_src_tst_subsets\;netcdf_libcf_src_tst_vars\;netcdf_libcf_src_tst_vert
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_RunTests_FortranTests_LANL_POP_netcdf-4.1.1_libcf_check_install PROPERTIES LABELS disabled DEPENDS "netcdf_libcf_src_tst_coords;netcdf_libcf_src_tst_cvars;netcdf_libcf_src_tst_example_data;netcdf_libcf_src_tst_f03_file;netcdf_libcf_src_tst_files;netcdf_libcf_src_tst_subsets;netcdf_libcf_src_tst_vars;netcdf_libcf_src_tst_vert" )
if(NOT REX_ENABLE_DISABLED_TESTS)
  set_tests_properties(nonsmoke_functional_RunTests_FortranTests_LANL_POP_netcdf-4.1.1_libcf_check_install PROPERTIES DISABLED TRUE)
endif()
