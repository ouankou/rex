add_library( includeDirectivesProcessing OBJECT
  IncludingPreprocessingInfosCollector.C)
add_dependencies(includeDirectivesProcessing rosetta_generated)


########### install files ###############

install(FILES
  IncludingPreprocessingInfosCollector.h
  DESTINATION ${INCLUDE_INSTALL_DIR})
