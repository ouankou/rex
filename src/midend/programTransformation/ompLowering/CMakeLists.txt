set(xomp_sources
  xomp.c
  run_me_callers.inc
  run_me_defs.inc
  run_me_callers2.inc
  run_me_task_defs.inc)

set_source_files_properties(
  run_me_callers.inc
  run_me_defs.inc
  run_me_callers2.inc
  run_me_task_defs.inc
  PROPERTIES HEADER_FILE_ONLY ON)

add_library(xomp STATIC ${xomp_sources})

add_library(kmpc_fortran_abi STATIC kmpc_fortran_wrappers.c)

install(TARGETS xomp
  ARCHIVE DESTINATION ${LIB_INSTALL_DIR})

install(TARGETS kmpc_fortran_abi
  ARCHIVE DESTINATION ${LIB_INSTALL_DIR})

########### install files ###############

install(FILES  omp_lowering.h libxompf.fh libxomp.h rex_kmp.h
        DESTINATION ${INCLUDE_INSTALL_DIR})
