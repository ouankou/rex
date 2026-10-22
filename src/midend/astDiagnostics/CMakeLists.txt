add_library(astDiagnostics OBJECT
  AstConsistencyTests.C AstStatistics.C AstPerformance.C)
add_dependencies(astDiagnostics rosetta_generated)

install(FILES
  AstDiagnostics.h AstConsistencyTests.h AstStatistics.h
  AstPerformance.h
  DESTINATION ${INCLUDE_INSTALL_DIR})
