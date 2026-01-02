add_subdirectory(StringUtility)

add_library(roseLogger OBJECT
  Logger.C)

install(
  FILES As.h BitOps.h Constants.h FileSystem.h Logger.h StringUtility.h SourceLocation.h
  DESTINATION ${INCLUDE_INSTALL_DIR}/Rose)
