add_subdirectory(StringUtility)

install(
  FILES BitOps.h Constants.h FileSystem.h StringUtility.h SourceLocation.h
  DESTINATION ${INCLUDE_INSTALL_DIR}/Rose)
