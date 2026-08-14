set(_rex_uninitialized_field_tests_default FALSE)
if(ROSE_USE_VALGRIND)
  set(_rex_uninitialized_field_tests_default TRUE)
endif()
option(REX_ENABLE_UNINITIALIZED_FIELD_TESTS
  "Build and register the Valgrind-backed uninitialized-field test suite"
  ${_rex_uninitialized_field_tests_default})
unset(_rex_uninitialized_field_tests_default)

if(REX_ENABLE_UNINITIALIZED_FIELD_TESTS AND NOT ROSE_USE_VALGRIND)
  message(FATAL_ERROR
    "REX_ENABLE_UNINITIALIZED_FIELD_TESTS=ON requires executable Valgrind "
    "and its development headers. Install Valgrind or explicitly configure "
    "with -DREX_ENABLE_UNINITIALIZED_FIELD_TESTS=OFF.")
endif()
