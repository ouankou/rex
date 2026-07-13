set(astPostProcessingSources
  astPostProcessing.C
  checkIsModifiedFlag.C
  resetParentPointers.C
  fixupTypes.C
  propagateHiddenListData.C
  markLhsValues.C
  buildStatementNumbering.C
  detectTransformations.C
  checkPhysicalSourcePosition.C
)

add_library(astPostProcessing OBJECT ${astPostProcessingSources})

add_dependencies(astPostProcessing rosetta_generated)


########### install files ###############

install(FILES
    astPostProcessing.h
    checkIsModifiedFlag.h
    resetParentPointers.h
    fixupTypes.h
    propagateHiddenListData.h
    markLhsValues.h
    checkPhysicalSourcePosition.h
    detectTransformations.h
    DESTINATION ${INCLUDE_INSTALL_DIR})
