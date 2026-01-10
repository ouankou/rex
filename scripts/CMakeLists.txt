# List in "distributedScripts" only those scripts that are useful after ROSE is installed

set(distributedScripts
  autoconf-filter.pl
  quiet-mode
  Error.pm
  Git.pm
  check-dist-file
  dotgenLetter
  dotgen
  dotgen2
  dotgenLetterPDF
  dotgenLetterPNG
  generateDirectoryGraph
  libtoolcp
  lsdot
  policies-checker.sh
  publicConfiguration.pl
  rth_run.pl
  rth_stats.pl
  runscript
  shtool)

set(distributedPolicyScripts
  policies/CaseInsensitiveFileNames.pl
  policies/EightColumnTabstops.pl.disabled
  policies/FileLister.pm
  policies/NoExceedinglyLongLines.pl
  policies/NoRoseIncludes.pl
  policies/NoTabCharacters.pl
  policies/NoUsingInHeaders.pl
  policies/Policies.pm
  policies/UniqueFortranModules.pl
  policies/UniqueHeaderNames.pl
  policies/UnixLineTermination.pl
  policies/UnusedHeaders.pl
  policies/UnusedSources.pl)

foreach(script IN LISTS distributedScripts)
  install(PROGRAMS ${CMAKE_CURRENT_SOURCE_DIR}/${script} DESTINATION ${BIN_INSTALL_DIR})
endforeach()

foreach(script IN LISTS distributedPolicyScripts)
  install(PROGRAMS ${CMAKE_CURRENT_SOURCE_DIR}/${script} DESTINATION ${BIN_INSTALL_DIR}/policies)
endforeach()
