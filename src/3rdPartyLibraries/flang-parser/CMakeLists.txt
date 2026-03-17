if(ENABLE-FORTRAN AND ENABLE-FORTRAN-FLANG)
  add_library(flang_parser OBJECT
    flang-external-builder-main.cc)

  target_include_directories(flang_parser PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/frontend/Experimental_Flang_ROSE_Connection)
  rose_force_system_include_dirs(flang_parser ${FLANG_INCLUDE_DIRS})

  if(DEFINED ROSE_FLANG_ENDIAN_DEFINES)
    target_compile_definitions(flang_parser PRIVATE ${ROSE_FLANG_ENDIAN_DEFINES})
  endif()

  add_dependencies(flang_parser rosetta_generated)
  target_compile_features(flang_parser PRIVATE cxx_std_17)
endif()
