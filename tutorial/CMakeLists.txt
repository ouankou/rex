if(ENABLE-TUTORIAL-DIRECTORY)
  include_directories(${ROSE_INCLUDES})

  add_subdirectory(outliner)

  set(tutorials
    ASTGraphGenerator
    ASTGraphGeneratorCustomized AST_PDF_Generator wholeASTGraphGenerator
    identityTranslator queryLibraryExample nestedQueryExample
    visitorTraversal prePostTraversal combinedTraversals traverseIRnodeTypes
    classicObjectOrientedVisitorPatternTraversal inheritedAttributeTraversal
    synthesizedAttributeTraversal inheritedAndSynthesizedAttributeTraversal
    accumulatorAttributeTraversal persistantAttributes nestedTraversal
    visitorMemoryPoolTraversal
    classicObjectOrientedVisitorPatternMemoryPoolTraversal loopRecognition
    typeInfoFromFunctionParameters resolveOverloadedFunction templateParameter
    instrumentEndOfFunction reductionRecognition
    addVariableDeclaration addVariableDeclaration2
    addAssignmentStmt addExpression addFunctionCalls addFunctionDeclaration
    addFunctionDeclaration2 addFunctionDeclaration3 loopOptimization LoopProcessor
    buildCallGraph buildVFA livenessAnalysis CharmSupport
    debuggingIRnodeToString debuggingSourceCodePositionInformation
    commandlineProcessing interveneAtVariables changeFunction classHierarchyGraph
    dataBaseUsage
    loopNestingInfoProcessing typeTransformation
    generatingUniqueNamesFromDeclaration
    symbolTableHandling scopeInformation customGraphGeneration inlineTransformations
    addComments collectComments
    collectDefineDirectives partialRedundancyElimination
    codeGenerationFormatControl dataFlowGraph customGraphGenerationWithFiltering
    graphTraversal wholeGraphAST traversalShortCircuit
    customGraphGenerationWithColors
    constantFoldingTransformation constantUnFoldingTransformation
    selectedFileTranslation wholeGraphFilterOnPersistantAttributes
    graphGenerationOfSubtree virtualCFG interproceduralCFG
    virtualFunctionIdentification switchWithoutDefault addStruct
    sprintf_to_snprintf seedBugsExample_arrayIndexing volatileTypeModifier
    findVariableDeclarations
    buildCFG buildFile defuseAnalysis)

  if(HAVE_PTHREAD_H)
    list(APPEND tutorials sharedMemoryTraversals)
  endif()

  foreach(tutorial ${tutorials})
    add_executable(${tutorial}Tutorial "${tutorial}.C")
    target_link_libraries(${tutorial}Tutorial ROSE_DLL)
  endforeach()

  if(HAVE_SYS_TIME_H)
    add_executable(codeCoverage codeCoverage.C coverageCodeFromShmuel.c)
    target_link_libraries(codeCoverage ROSE_DLL)
  endif()
endif()
