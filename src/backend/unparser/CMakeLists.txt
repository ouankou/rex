add_subdirectory(CxxCodeGeneration)
add_subdirectory(languageIndependenceSupport)
add_subdirectory(formatSupport)
add_subdirectory(FortranCodeGeneration)

########### next target ###############

add_library (unparser OBJECT
  FortranCodeGeneration/unparseFortran_expressions.C
  FortranCodeGeneration/unparseFortran_modfile.C
  FortranCodeGeneration/unparseFortran_statements.C
  FortranCodeGeneration/unparseFortran_types.C
  CxxCodeGeneration/unparseCxx_types.C
  CxxCodeGeneration/unparseCxx_expressions.C
  CxxCodeGeneration/unparseCxx_statements.C
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

set(unparser_headers copy_unparser.h unparser.h unparse_sym.h
  astUnparseAttribute.h IncludedFilesUnparser.h includeFileSupport.h nameQualificationSupport.h)

install(FILES ${unparser_headers} DESTINATION ${INCLUDE_INSTALL_DIR})
