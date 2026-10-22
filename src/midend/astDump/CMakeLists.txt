set(astDumpSources
  astGraph.C
  AstAttributeDOT.C
  wholeAST.C
  copyGraph.C
  walrus.C
  includeGraph.C
  AstDOTGeneration.C
  AstTerm.C)

if(ROSE_WITH_LIBHARU)
  list(APPEND astDumpSources
    AstPDFGeneration.C
    PDFGeneration.C)
endif()

add_library(astDump OBJECT ${astDumpSources})
add_dependencies(astDump rosetta_generated)
if(TARGET REX::HPDF)
  target_link_libraries(astDump PRIVATE REX::HPDF)
endif()

set(astDumpHeaders
  AstAttributeDOT.h astGraph.h astGraphTemplateImpl.h wholeAST.h
  wholeAST_API.h copyGraph.h walrus.h
  includeGraph.h
  AstDOTGeneration.h
  AstDOTGenerationImpl.C
  DOTGeneration.h DOTGenerationImpl.h
  DOTRepresentation.h DOTRepresentationImpl.h
  AstTerm.h)

if(ROSE_WITH_LIBHARU)
  list(APPEND astDumpHeaders
    AstPDFGeneration.h
    PDFGeneration.h)
endif()

install(FILES ${astDumpHeaders} DESTINATION ${INCLUDE_INSTALL_DIR})
