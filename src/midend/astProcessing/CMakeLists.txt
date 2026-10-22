add_library(astProcessing OBJECT
  AstNodeVisitMapping.C
  AstProcessing.C
  AstSimpleProcessing.C
  AstSuccessorsSelectors.C
  AstAttributeMechanism.C
  AstReverseSimpleProcessing.C
  AstTraversal.C
  AstCombinedSimpleProcessing.C
  AstSharedMemoryParallelSimpleProcessing.C
  plugin.C)

if(NOT ENABLE_ALL_WARNINGS)
  # Ignore -Wmisleading-indentation per 6-25-24 ROSE meeting discussion
  target_compile_options(astProcessing PRIVATE $<$<CXX_COMPILER_ID:GNU>:-Wno-misleading-indentation>)
endif()

add_dependencies(astProcessing rosetta_generated)

install(FILES
  AstNodeVisitMapping.h AstAttributeMechanism.h
  AstProcessing.h
  AstSimpleProcessing.h
  AstSuccessorsSelectors.h AstReverseProcessing.h
  AstReverseSimpleProcessing.h
  AstTraversal.h AstCombinedProcessing.h AstCombinedProcessingImpl.h
  AstCombinedSimpleProcessing.h StackFrameVector.h
  AstSharedMemoryParallelProcessing.h AstSharedMemoryParallelProcessingImpl.h
  AstSharedMemoryParallelSimpleProcessing.h
  SgGraphTemplate.h graphProcessing.h
  plugin.h
  TreeTraversal.h
  DESTINATION ${INCLUDE_INSTALL_DIR})
