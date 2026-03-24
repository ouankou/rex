# DQ (4/17/2010): These directories are now organized based on what sets of
# languages are enabled in ROSE.

if(ENABLE-C)
  add_subdirectory (astMergeTests)
  add_subdirectory (astDiagnostics)
  add_subdirectory (astPerformanceTests)
  add_subdirectory (astProcessingTests)
  add_subdirectory (astQueryTests)
  add_subdirectory (astSymbolTableTests)
  add_subdirectory (astTokenStreamTests)
  add_subdirectory (programTransformationTests)
  #add_subdirectory (graph_tests)
  add_subdirectory (mergeTraversal_tests)
  add_subdirectory (astLValueTests)
  add_subdirectory (varDeclNorm)
  add_subdirectory (ROSETTA)
  add_subdirectory (astInterfaceTests)
  add_subdirectory (astInliningTests)
  add_subdirectory (ompLoweringTests)
  add_subdirectory (programAnalysisTests)
  add_subdirectory (astOutliningTests)
  #add_subdirectory (loopProcessingTests)
  add_subdirectory (fileLocation_tests)
endif() # C/C++

