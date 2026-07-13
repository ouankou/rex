add_library( astTokenStream OBJECT
  linearizeAST.C
  tokenStreamMapping.C
  frontierDetection.C
  simpleFrontierDetection.C
  artificialFrontier.C
  maxExtents.C
  detectMacroOrIncludeFileExpansions.C
  detectMacroExpansionsToBeUnparsedAsAstTransformations.C)
add_dependencies(astTokenStream rosetta_generated)



########### install files ###############

install(FILES
  linearizeAST.h
  tokenStreamMapping.h frontierDetection.h simpleFrontierDetection.h
  maxExtents.h detectMacroOrIncludeFileExpansions.h artificialFrontier.h
  detectMacroExpansionsToBeUnparsedAsAstTransformations.h
  DESTINATION ${INCLUDE_INSTALL_DIR})
