set(astProcessing_SRC
  AstNodeVisitMapping.C
  AstTextAttributesHandling.C
  AstProcessing.C
  AstSimpleProcessing.C
  AstNodePtrs.C
  AstSuccessorsSelectors.C
  AstAttributeMechanism.C
  AstReverseSimpleProcessing.C
  AstClearVisitFlags.C
  AstTraversal.C
  AstCombinedSimpleProcessing.C
  AstSharedMemoryParallelSimpleProcessing.C
  plugin.C
  AstRestructure.C)

add_library(astProcessing OBJECT ${astProcessing_SRC})

if(NOT ENABLE_ALL_WARNINGS)
  # Ignore -Wmisleading-indentation per 6-25-24 ROSE meeting discussion
  target_compile_options(astProcessing PRIVATE $<$<CXX_COMPILER_ID:GNU>:-Wno-misleading-indentation>)
endif()

add_dependencies(astProcessing rosetta_generated)


########### install files ###############

set(files_to_install
  AstNodeVisitMapping.h AstAttributeMechanism.h
  AstTextAttributesHandling.h AstProcessing.h
  AstSimpleProcessing.h AstTraverseToRoot.h AstNodePtrs.h
  AstSuccessorsSelectors.h AstReverseProcessing.h
  AstReverseSimpleProcessing.h AstRestructure.h AstClearVisitFlags.h
  AstTraversal.h AstCombinedProcessing.h AstCombinedProcessingImpl.h
  AstCombinedSimpleProcessing.h StackFrameVector.h
  AstSharedMemoryParallelProcessing.h AstSharedMemoryParallelProcessingImpl.h
  AstSharedMemoryParallelSimpleProcessing.h
  SgGraphTemplate.h graphProcessing.h
  plugin.h
  TreeTraversal.h)

install(FILES ${files_to_install} DESTINATION ${INCLUDE_INSTALL_DIR})
