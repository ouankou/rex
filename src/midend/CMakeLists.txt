add_subdirectory(abstractLayer)
add_subdirectory(astDiagnostics)
add_subdirectory(astDump)
add_subdirectory(astProcessing)
add_subdirectory(astQuery)
add_subdirectory(astUtil)
add_subdirectory(programTransformation)
if(NOT ENABLE-INTERNALFRONTENDDEVELOPMENT)
  add_subdirectory(programAnalysis)
endif()


add_library(midend OBJECT
  astQuery/nodeQueryInheritedAttribute.C
  astQuery/nameQuery.C
  astQuery/astQueryInheritedAttribute.C
  astQuery/numberQueryInheritedAttribute.C
  astQuery/booleanQuery.C
  astQuery/booleanQueryInheritedAttribute.C
  astQuery/numberQuery.C
  astQuery/astQuery.C
  astQuery/nameQueryInheritedAttribute.C
  astQuery/nodeQuery.C)

add_dependencies(midend rosetta_generated)
