add_library(kmpc_fortran_abi STATIC kmpc_fortran_wrappers.c)
add_library(xomp STATIC xomp.c $<TARGET_OBJECTS:kmpc_fortran_abi>)

install(TARGETS xomp
  ARCHIVE DESTINATION ${LIB_INSTALL_DIR})

install(TARGETS kmpc_fortran_abi
  ARCHIVE DESTINATION ${LIB_INSTALL_DIR})

install(FILES  omp_lowering.h libxompf.fh libxomp.h rex_kmp.h
        DESTINATION ${INCLUDE_INSTALL_DIR})
