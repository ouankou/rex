set(astDumpSources
  astGraph.C
  AstAttributeDOT.C
  wholeAST.C
  copyGraph.C
  walrus.C
  intermediateRepresentationNodeGraphs.C
  includeGraph.C
  AstDOTGeneration.C
  DOTGraphInterface.C
  AstTerm.C)

if(ROSE_WITH_LIBHARU)
  list(APPEND astDumpSources
    AstPDFGeneration.C
    PDFGeneration.C)
endif()

add_library(astDump OBJECT ${astDumpSources})
add_dependencies(astDump rosetta_generated)


########### install files ###############

set(astDumpHeaders
  AstAttributeDOT.h astGraph.h astGraphTemplateImpl.h wholeAST.h
  wholeAST_API.h copyGraph.h walrus.h
  intermediateRepresentationNodeGraphs.h
  includeGraph.h
  AstDOTGeneration.h
  DOTGeneration.h DOTGenerationImpl.h DOTGraphInterface.h
  DOTRepresentation.h DOTRepresentationImpl.h DOTSubgraphRepresentation.h
  DOTSubgraphRepresentationImpl.h
  AstTerm.h)

if(ROSE_WITH_LIBHARU)
  list(APPEND astDumpHeaders
    AstPDFGeneration.h
    PDFGeneration.h)
endif()

install(FILES ${astDumpHeaders} DESTINATION ${INCLUDE_INSTALL_DIR})
