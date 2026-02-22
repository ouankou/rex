add_subdirectory(CxxCodeGeneration)
add_subdirectory(languageIndependenceSupport)
add_subdirectory(formatSupport)
add_subdirectory(FortranCodeGeneration)

########### next target ###############

add_library (unparser OBJECT
  FortranLineWrapSupport.C
  FortranCodeGeneration/UnparserFortran.C
  FortranCodeGeneration/unparseFortran_expressions.C
  FortranCodeGeneration/unparseFortran_modfile.C
  FortranCodeGeneration/unparseFortran_statements.C
  FortranCodeGeneration/unparseFortran_types.C
  CxxCodeGeneration/unparseCxx_types.C
  CxxCodeGeneration/unparseCxx_expressions.C
  CxxCodeGeneration/unparseCxx_statements.C
  CxxCodeGeneration/unparseNewOp.C
  CxxCodeGeneration/unparseAggrInit.C
  CxxCodeGeneration/unparseAssnInit.C
  CxxCodeGeneration/unparseBracedInit.C
  CxxCodeGeneration/unparseCompInit.C
  CxxCodeGeneration/unparseCtorInit.C
  CxxCodeGeneration/unparseDesignatedInitializer.C
  CxxCodeGeneration/unparseVarDeclStmt.C
  CxxCodeGeneration/helpers.C
  astUnparseAttribute.C
  unparser.C
  nameQualificationSupport.C
  unparse_sym.C
  IncludedFilesUnparser.C
  includeFileSupport.C
  formatSupport/unparseFormatHelp.C
  formatSupport/unparse_format.C
  languageIndependenceSupport/modified_sage_isUnaryOp.C
  languageIndependenceSupport/unparser_opt.C
  languageIndependenceSupport/modified_sage.C
  languageIndependenceSupport/name_qualification_support.C
  languageIndependenceSupport/unparse_debug.C
  languageIndependenceSupport/unparseLanguageIndependentConstructs.C)
add_dependencies(unparser rosetta_generated)

########### install files ###############

set(unparser_headers
  astUnparseAttribute.h
  copy_unparser.h
  IncludedFilesUnparser.h
  includeFileSupport.h
  nameQualificationSupport.h
  UnparserDelegate.h
  unparser.h
  unparse_sym.h
)

install(FILES ${unparser_headers} DESTINATION ${INCLUDE_INSTALL_DIR})
