if(ENABLE-FORTRAN AND ENABLE-FORTRAN-FLANG)
  add_library(flang_frontend OBJECT
    fortran_flang_support.C
    unparse-sage.cpp
    sage-build.C)

  target_include_directories(flang_frontend PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/3rdPartyLibraries/flang-parser
    ${CMAKE_SOURCE_DIR}/src/frontend/Experimental_General_Language_Support
    ${FLANG_INCLUDE_DIRS})

  target_compile_features(flang_frontend PRIVATE cxx_std_17)
endif()

install(FILES fortran_flang_support.h DESTINATION ${INCLUDE_INSTALL_DIR})
