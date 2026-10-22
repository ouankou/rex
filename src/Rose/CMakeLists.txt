add_subdirectory(StringUtility)

install(
  FILES FloatingValue.h Integer128Value.h As.h BitOps.h Constants.h FileSystem.h StringUtility.h SourceLocation.h
  DESTINATION ${INCLUDE_INSTALL_DIR}/Rose)
