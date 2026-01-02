if(ENABLE-FORTRAN AND ENABLE-FORTRAN-FLANG)
  add_library(flang_parser OBJECT
    flang-external-builder-main.cc)

  target_include_directories(flang_parser PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/frontend/Experimental_Flang_ROSE_Connection
    ${FLANG_INCLUDE_DIRS})

  target_compile_features(flang_parser PRIVATE cxx_std_17)
endif()
