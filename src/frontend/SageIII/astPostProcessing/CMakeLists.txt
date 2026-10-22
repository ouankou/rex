add_library(astPostProcessing OBJECT
  astPostProcessing.C
  resetParentPointers.C
  buildStatementNumbering.C
  detectTransformations.C
  checkPhysicalSourcePosition.C
)

add_dependencies(astPostProcessing rosetta_generated)

install(FILES
    astPostProcessing.h
    resetParentPointers.h
    checkPhysicalSourcePosition.h
    detectTransformations.h
    DESTINATION ${INCLUDE_INSTALL_DIR})
