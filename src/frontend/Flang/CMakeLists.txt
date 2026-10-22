if(ENABLE-FORTRAN)
  add_library(libroseFlangFrontend OBJECT
    FlangModuleInfo.C
    fortran_flang_support.C
    sage-build.C
    builder/PosInfo.C
    builder/SageTreeBuilder.C
    driver/flang-parser-driver.cc)

  target_include_directories(libroseFlangFrontend PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/driver)
  rose_force_system_include_dirs(libroseFlangFrontend
    ${FLANG_INCLUDE_DIRS}
    ${LLVM_INCLUDE_DIRS})

  if(FLANG_INTRINSIC_INCLUDE_DIR)
    target_compile_definitions(
      libroseFlangFrontend PRIVATE
      "ROSE_FLANG_INTRINSIC_INCLUDEDIR=\"${FLANG_INTRINSIC_INCLUDE_DIR}\"")
  endif()

  add_dependencies(libroseFlangFrontend rosetta_generated)
  target_compile_features(libroseFlangFrontend PRIVATE cxx_std_17)
  rose_enable_private_pch(libroseFlangFrontend
    "${ROSE_TOP_SRC_DIR}/cmake/pch/flang-frontend-pch.hpp")
  install(
    FILES
      intrinsics/openacc.f90
      intrinsics/openacc.rmod
    DESTINATION share/rose)
endif()

install(FILES fortran_flang_support.h DESTINATION ${INCLUDE_INSTALL_DIR})
