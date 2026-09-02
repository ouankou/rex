set(_rex_version_output_regex
  "^${ROSE_PACKAGE_VERSION} \\(configured [0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9] [0-9][0-9]:[0-9][0-9]:[0-9][0-9] UTC\\)")
add_test(
  NAME rex_rose_config_version_reporting
  COMMAND $<TARGET_FILE:rose-config> --version)
add_test(
  NAME rex_rose_compiler_version_reporting
  COMMAND $<TARGET_FILE:rose-compiler> --version)
set_tests_properties(
  rex_rose_config_version_reporting
  rex_rose_compiler_version_reporting
  PROPERTIES
    LABELS "nonsmoke;utility"
    PASS_REGULAR_EXPRESSION "${_rex_version_output_regex}")
unset(_rex_version_output_regex)

function(rex_add_rose_compiler_smoke_test language source expected_output)
  set(_test_name "rex_rose_compiler_${language}_compile_run")
  add_test(
    NAME "${_test_name}"
    COMMAND
      ${CMAKE_COMMAND}
      "-DROSE_COMPILER=$<TARGET_FILE:rose-compiler>"
      "-DSOURCE_FILE=${source}"
      "-DWORK_DIRECTORY=${CMAKE_CURRENT_BINARY_DIR}/${_test_name}"
      "-DEXPECTED_OUTPUT=${expected_output}"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/rex_rose_compiler_compile_run.cmake")
  set_tests_properties(
    "${_test_name}"
    PROPERTIES
      LABELS "nonsmoke;driver;${language}"
      TIMEOUT 120)
endfunction()

if(ENABLE-C)
  rex_add_rose_compiler_smoke_test(
    c
    "${CMAKE_CURRENT_SOURCE_DIR}/rex_driver_compile_run.c"
    "REX C driver smoke")
  rex_add_rose_compiler_smoke_test(
    cxx
    "${CMAKE_CURRENT_SOURCE_DIR}/rex_driver_compile_run.cpp"
    "REX C++ driver smoke")
endif()

if(ENABLE-FORTRAN)
  rex_add_rose_compiler_smoke_test(
    fortran
    "${CMAKE_CURRENT_SOURCE_DIR}/rex_driver_compile_run.f90"
    "REX Fortran driver smoke")
endif()
