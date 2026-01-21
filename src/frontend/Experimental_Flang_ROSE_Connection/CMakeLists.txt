if(ENABLE-FORTRAN AND ENABLE-FORTRAN-FLANG)
  add_library(flang_frontend OBJECT
    FlangModuleInfo.C
    fortran_flang_support.C
    unparse-sage.cpp
    sage-build.C)

  target_include_directories(flang_frontend PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/3rdPartyLibraries/flang-parser
    ${CMAKE_SOURCE_DIR}/src/frontend/Experimental_General_Language_Support
    ${FLANG_INCLUDE_DIRS})

  if(LLVM_INCLUDE_DIRS)
    target_include_directories(flang_frontend SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS})
  endif()

  if(DEFINED ROSE_FLANG_ENDIAN_DEFINES)
    target_compile_definitions(flang_frontend PRIVATE ${ROSE_FLANG_ENDIAN_DEFINES})
  endif()

  add_dependencies(flang_frontend rosetta_generated)
  target_compile_features(flang_frontend PRIVATE cxx_std_17)
endif()

install(FILES fortran_flang_support.h DESTINATION ${INCLUDE_INSTALL_DIR})
