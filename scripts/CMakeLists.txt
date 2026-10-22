install(PROGRAMS
  lsdot
  policies-checker.sh
  rth_run.pl
  DESTINATION ${BIN_INSTALL_DIR})

install(PROGRAMS
  policies/CaseInsensitiveFileNames.pl
  policies/NoExceedinglyLongLines.pl
  policies/NoRoseIncludes.pl
  policies/NoRoseHHeaderInLibrarySource.pl
  policies/NoTabCharacters.pl
  policies/NoUsingInHeaders.pl
  policies/RoseNamespace.pl
  policies/UniqueFortranModules.pl
  policies/UniqueHeaderNames.pl
  policies/UnixLineTermination.pl
  policies/UnusedHeaders.pl
  policies/UnusedSources.pl
  DESTINATION ${BIN_INSTALL_DIR}/policies)

install(FILES
  policies/FileLister.pm
  policies/Policies.pm
  DESTINATION ${BIN_INSTALL_DIR}/policies)
