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

install(TARGETS xomp
  ARCHIVE DESTINATION ${LIB_INSTALL_DIR})

########### install files ###############

install(FILES  omp_lowering.h libgomp_g.h libompc.h libxompf.fh libxomp.h
        DESTINATION ${INCLUDE_INSTALL_DIR})
