add_subdirectory(abstractLayer)
add_subdirectory(astDiagnostics)
add_subdirectory(astDump)
add_subdirectory(astProcessing)
add_subdirectory(astQuery)
add_subdirectory(astUtil)
add_subdirectory(programTransformation)
add_subdirectory(programAnalysis)


add_library(midend OBJECT
  astQuery/nameQuery.C
  astQuery/numberQuery.C
  astQuery/astQuery.C
  astQuery/nodeQuery.C)

add_dependencies(midend rosetta_generated)
