add_library(util_commandlineProcessing OBJECT
  commandline_processing.C
  sla++.C)

# commandline_processing.C includes rosePublicConfig.h

install(FILES
  sla.h commandline_processing.h DESTINATION ${INCLUDE_INSTALL_DIR})
