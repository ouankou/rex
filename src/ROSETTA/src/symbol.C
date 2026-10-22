#include "AstNodeClass.h"

#include "ROSETTA_macros.h"

#include "featureTests.h"

#include "grammar.h"

void Grammar::setUpSymbols() {
  NEW_TERMINAL_MACRO(TemplateVariableSymbol, "TemplateVariableSymbol",
                     "TEMPLATE_VARIABLE_NAME");
  NEW_NONTERMINAL_MACRO(VariableSymbol, TemplateVariableSymbol,
                        "VariableSymbol", "VARIABLE_NAME", true);

  NEW_TERMINAL_MACRO(FunctionTypeSymbol, "FunctionTypeSymbol", "FUNCTYPE_NAME");

  NEW_TERMINAL_MACRO(TemplateClassSymbol, "TemplateClassSymbol",
                     "TEMPLATE_CLASS_NAME");
  NEW_NONTERMINAL_MACRO(ClassSymbol, TemplateClassSymbol, "ClassSymbol",
                        "CLASS_NAME", true);

  NEW_TERMINAL_MACRO(TemplateSymbol, "TemplateSymbol", "TEMPLATE_NAME");

  NEW_TERMINAL_MACRO(EnumSymbol, "EnumSymbol", "ENUM_NAME");
  NEW_TERMINAL_MACRO(EnumFieldSymbol, "EnumFieldSymbol", "FIELD_NAME");

  NEW_TERMINAL_MACRO(TemplateTypedefSymbol, "TemplateTypedefSymbol",
                     "TEMPLATE_TYPEDEF_NAME");
  NEW_NONTERMINAL_MACRO(TypedefSymbol, TemplateTypedefSymbol, "TypedefSymbol",
                        "TYPEDEF_NAME", true);

  NEW_TERMINAL_MACRO(TemplateFunctionSymbol, "TemplateFunctionSymbol",
                     "TEMPLATE_FUNC_NAME");
  NEW_TERMINAL_MACRO(TemplateMemberFunctionSymbol,
                     "TemplateMemberFunctionSymbol",
                     "TEMPLATE_MEMBER_FUNC_NAME");

  NEW_TERMINAL_MACRO(LabelSymbol, "LabelSymbol", "LABEL_NAME");

  NEW_TERMINAL_MACRO(DefaultSymbol, "DefaultSymbol", "DEFAULT_NAME");

  NEW_TERMINAL_MACRO(NamespaceSymbol, "NamespaceSymbol", "NAMESPACE_NAME");

  NEW_TERMINAL_MACRO(IntrinsicSymbol, "IntrinsicSymbol", "INTRINSIC_SYMBOL");
  NEW_TERMINAL_MACRO(ModuleSymbol, "ModuleSymbol", "MODULE_SYMBOL");
  NEW_TERMINAL_MACRO(InterfaceSymbol, "InterfaceSymbol", "INTERFACE_SYMBOL");
  NEW_TERMINAL_MACRO(CommonSymbol, "CommonSymbol", "COMMON_SYMBOL");
  NEW_TERMINAL_MACRO(NamelistSymbol, "NamelistSymbol", "NAMELIST_SYMBOL");

  // Fortran interface renames retain function-symbol identity.
  NEW_TERMINAL_MACRO(RenameSymbol, "RenameSymbol", "RENAME_SYMBOL");

  NEW_NONTERMINAL_MACRO(MemberFunctionSymbol, TemplateMemberFunctionSymbol,
                        "MemberFunctionSymbol", "MEMBER_FUNC_NAME", true);
  NEW_NONTERMINAL_MACRO(FunctionSymbol,
                        MemberFunctionSymbol | TemplateFunctionSymbol |
                            RenameSymbol,
                        "FunctionSymbol", "FUNCTION_NAME", true);

  NEW_TERMINAL_MACRO(AliasSymbol, "AliasSymbol", "ALIAS_SYMBOL");

  NEW_TERMINAL_MACRO(NonrealSymbol, "NonrealSymbol", "NONREAL_SYMBOL");

  NEW_NONTERMINAL_MACRO(Symbol,
                        VariableSymbol | NonrealSymbol | FunctionSymbol |
                            FunctionTypeSymbol | ClassSymbol | TemplateSymbol |
                            EnumSymbol | EnumFieldSymbol | TypedefSymbol |
                            LabelSymbol | DefaultSymbol | NamespaceSymbol |
                            IntrinsicSymbol | ModuleSymbol | InterfaceSymbol |
                            CommonSymbol | NamelistSymbol | AliasSymbol,
                        "Symbol", "SymbolTag", false);

  Symbol.setFunctionPrototype("HEADER", "../Grammar/Symbol.code");

  Symbol.setSubTreeFunctionPrototype("HEADER_GET_NAME",
                                     "../Grammar/Symbol.code");
  Symbol.excludeFunctionPrototype("HEADER_GET_NAME", "../Grammar/Symbol.code");

  Symbol.setSubTreeFunctionPrototype("HEADER_GET_TYPE",
                                     "../Grammar/Symbol.code");
  Symbol.excludeFunctionPrototype("HEADER_GET_TYPE", "../Grammar/Symbol.code");
  Symbol.setAutomaticGenerationOfCopyFunction(false);

  NamelistSymbol.excludeFunctionPrototype("HEADER_GET_NAME",
                                          "../Grammar/Symbol.code");
  NamelistSymbol.excludeFunctionPrototype("HEADER_GET_TYPE",
                                          "../Grammar/Symbol.code");
  NamelistSymbol.setFunctionPrototype("HEADER_NAMELIST_SYMBOL",
                                      "../Grammar/Symbol.code");
  NamelistSymbol.setFunctionSource("SOURCE_NAMELIST_SYMBOL",
                                   "../Grammar/Symbol.code");
  NamelistSymbol.setFunctionSource("SOURCE_EMPTY_GET_TYPE",
                                   "../Grammar/Symbol.code");
  NamelistSymbol.setDataPrototype(
      "SgNameGroupPtrList", "declarations", "", CONSTRUCTOR_PARAMETER,
      NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE, COPY_CONSTRUCTOR_REFERENCE);

  // Attribute accessors must not mark the symbol as modified.
  Symbol.setDataPrototype("AstAttributeMechanism*", "attributeMechanism",
                          "= NULL", NO_CONSTRUCTOR_PARAMETER,
                          BUILD_FLAG_ACCESS_FUNCTIONS, NO_TRAVERSAL, DEF_DELETE,
                          CLONE_PTR);
  Symbol.setFunctionPrototype("HEADER_ATTRIBUTE_SUPPORT",
                              "../Grammar/Support.code");
  Symbol.setFunctionSource("SOURCE_ATTRIBUTE_SUPPORT",
                           "../Grammar/Support.code");

  VariableSymbol.setDataPrototype("SgInitializedName*", "declaration", "= NULL",
                                  CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                                  NO_TRAVERSAL, NO_DELETE);

  // A symbol table owns symbol nodes; symbols only reference the declarations
  // and other semantic nodes that define their meaning.  Treating those basis
  // pointers as traversal ownership made a symbol copy recursively manufacture
  // declarations outside the requested copy root.  Copied symbol tables now
  // clone their exact source symbols and remap these non-owning edges through
  // the active SgCopyHelp transaction.

  FunctionSymbol.setDataPrototype("SgFunctionDeclaration*", "declaration",
                                  "= NULL", CONSTRUCTOR_PARAMETER,
                                  BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
                                  NO_DELETE, COPY_CONSTRUCTOR_REFERENCE);

  FunctionTypeSymbol.excludeFunctionPrototype("HEADER_GET_NAME",
                                              "../Grammar/Symbol.code");
  FunctionTypeSymbol.excludeFunctionPrototype("HEADER_GET_TYPE",
                                              "../Grammar/Symbol.code");
  FunctionTypeSymbol.setDataPrototype(
      "SgName", "name", "= \"\"", CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
      NO_TRAVERSAL, NO_DELETE);
  FunctionTypeSymbol.setDataPrototype(
      "SgType*", "type", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  ClassSymbol.setDataPrototype("SgClassDeclaration*", "declaration", "= NULL",
                               CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                               NO_TRAVERSAL, NO_DELETE, COPY_DATA);
  TemplateSymbol.setDataPrototype(
      "SgTemplateDeclaration*", "declaration", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE,
      COPY_DATA);

  NonrealSymbol.setDataPrototype("SgNonrealDecl*", "declaration", "= NULL",
                                 CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                                 NO_TRAVERSAL, NO_DELETE, COPY_DATA);

  EnumSymbol.setDataPrototype("SgEnumDeclaration*", "declaration", "= NULL",
                              CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                              NO_TRAVERSAL, NO_DELETE, COPY_DATA);
  EnumFieldSymbol.setDataPrototype(
      "SgInitializedName*", "declaration", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  TypedefSymbol.setDataPrototype(
      "SgTypedefDeclaration*", "declaration", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE, COPY_DATA);

  MemberFunctionSymbol.setFunctionPrototype("HEADER_DECLARATION",
                                            "../Grammar/Symbol.code");

  LabelSymbol.setFunctionPrototype("HEADER_LABEL_SYMBOL",
                                   "../Grammar/Symbol.code");
  LabelSymbol.setDataPrototype("SgLabelStatement*", "declaration", "= NULL",
                               CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                               NO_TRAVERSAL, NO_DELETE, COPY_DATA);
  LabelSymbol.setDataPrototype("SgStatement*", "fortran_statement", "= NULL",
                               NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                               NO_TRAVERSAL, NO_DELETE);
  LabelSymbol.setDataPrototype("SgInitializedName*",
                               "fortran_alternate_return_parameter", "= NULL",
                               NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                               NO_TRAVERSAL, NO_DELETE);
  LabelSymbol.setDataPrototype("int", "numeric_label_value", "= -1",
                               NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                               NO_TRAVERSAL, NO_DELETE);
  LabelSymbol.setDataPrototype("SgLabelSymbol::label_type_enum", "label_type",
                               "= SgLabelSymbol::e_unknown_label_type",
                               NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                               NO_TRAVERSAL, NO_DELETE);
  DefaultSymbol.excludeFunctionPrototype("HEADER_GET_TYPE",
                                         "../Grammar/Symbol.code");

  DefaultSymbol.setDataPrototype("SgType*", "type", "= NULL",
                                 CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                                 NO_TRAVERSAL, NO_DELETE);

  NamespaceSymbol.setFunctionPrototype("HEADER_NAMESPACE_SYMBOL",
                                       "../Grammar/Symbol.code");
  NamespaceSymbol.setDataPrototype(
      "SgNamespaceDeclarationStatement*", "declaration", "= NULL",
      CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE,
      COPY_DATA);
  NamespaceSymbol.setDataPrototype(
      "SgNamespaceAliasDeclarationStatement*", "aliasDeclaration", "= NULL",
      CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE,
      COPY_DATA);

  IntrinsicSymbol.setFunctionPrototype("HEADER_GET_NAME",
                                       "../Grammar/Symbol.code");
  ModuleSymbol.setFunctionPrototype("HEADER_GET_NAME",
                                    "../Grammar/Symbol.code");
  InterfaceSymbol.setFunctionPrototype("HEADER_GET_NAME",
                                       "../Grammar/Symbol.code");
  CommonSymbol.setFunctionPrototype("HEADER_GET_NAME",
                                    "../Grammar/Symbol.code");

  IntrinsicSymbol.setFunctionPrototype("HEADER_GET_TYPE",
                                       "../Grammar/Symbol.code");
  ModuleSymbol.setFunctionPrototype("HEADER_GET_TYPE",
                                    "../Grammar/Symbol.code");
  InterfaceSymbol.setFunctionPrototype("HEADER_GET_TYPE",
                                       "../Grammar/Symbol.code");
  CommonSymbol.setFunctionPrototype("HEADER_GET_TYPE",
                                    "../Grammar/Symbol.code");

  IntrinsicSymbol.setDataPrototype(
      "SgInitializedName*", "declaration", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  InterfaceSymbol.setDataPrototype(
      "SgInterfaceStatement*", "declaration", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  ModuleSymbol.setDataPrototype("SgModuleStatement*", "declaration", "= NULL",
                                CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                                NO_TRAVERSAL, NO_DELETE, COPY_DATA);
  CommonSymbol.setDataPrototype("SgInitializedName*", "declaration", "= NULL",
                                CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                                NO_TRAVERSAL, NO_DELETE);

  AliasSymbol.setFunctionPrototype("HEADER_ALIAS_SYMBOL",
                                   "../Grammar/Symbol.code");
  AliasSymbol.setDataPrototype("SgSymbol*", "alias", "= NULL",
                               CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                               NO_TRAVERSAL, NO_DELETE);

  AliasSymbol.setDataPrototype("bool", "isRenamed", "= false",
                               CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                               NO_TRAVERSAL, NO_DELETE);
  AliasSymbol.setDataPrototype("SgName", "new_name", "= \"\"",
                               CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                               NO_TRAVERSAL, NO_DELETE);

  // Causal declarations determine alias visibility at a source position
  // during name qualification.
  AliasSymbol.setDataPrototype(
      "SgNodePtrList", "causal_nodes", "", NO_CONSTRUCTOR_PARAMETER,
      BUILD_LIST_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  RenameSymbol.setFunctionPrototype("HEADER_RENAME_SYMBOL",
                                    "../Grammar/Symbol.code");
  RenameSymbol.setDataPrototype("SgSymbol*", "original_symbol", "= NULL",
                                CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                                NO_TRAVERSAL, NO_DELETE);
  RenameSymbol.setDataPrototype("SgName", "new_name", "= \"\"",
                                CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                                NO_TRAVERSAL, NO_DELETE);
  RenameSymbol.setDataPrototype(
      "SgNodePtrList", "causal_nodes", "", NO_CONSTRUCTOR_PARAMETER,
      BUILD_LIST_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  Symbol.setFunctionSource("SOURCE", "../Grammar/Symbol.code");

  VariableSymbol.setFunctionSource("SOURCE_GET_TYPE", "../Grammar/Symbol.code");
  TemplateVariableSymbol.setFunctionSource("SOURCE_GET_TYPE",
                                           "../Grammar/Symbol.code");

  IntrinsicSymbol.setFunctionSource("SOURCE_GET_TYPE",
                                    "../Grammar/Symbol.code");
  ModuleSymbol.setFunctionSource("SOURCE_EMPTY_GET_TYPE",
                                 "../Grammar/Symbol.code");
  InterfaceSymbol.setFunctionSource("SOURCE_EMPTY_GET_TYPE",
                                    "../Grammar/Symbol.code");
  CommonSymbol.setFunctionSource("SOURCE_GET_TYPE", "../Grammar/Symbol.code");

  FunctionSymbol.setFunctionSource("SOURCE_GET_TYPE", "../Grammar/Symbol.code");
  TemplateFunctionSymbol.setFunctionSource("SOURCE_GET_TYPE",
                                           "../Grammar/Symbol.code");
  ClassSymbol.setFunctionSource("SOURCE_GET_TYPE", "../Grammar/Symbol.code");
  TemplateClassSymbol.setFunctionSource("SOURCE_GET_TYPE",
                                        "../Grammar/Symbol.code");
  NonrealSymbol.setFunctionSource("SOURCE_GET_TYPE", "../Grammar/Symbol.code");

  TemplateSymbol.setFunctionSource("SOURCE_GET_TYPE",
                                   "../Grammar/Symbol.code");

  EnumSymbol.setFunctionSource("SOURCE_GET_TYPE", "../Grammar/Symbol.code");
  EnumFieldSymbol.setFunctionSource("SOURCE_GET_TYPE",
                                    "../Grammar/Symbol.code");

  TypedefSymbol.setFunctionSource("SOURCE_GET_TYPE", "../Grammar/Symbol.code");

  TemplateTypedefSymbol.setFunctionSource("SOURCE_GET_TYPE",
                                          "../Grammar/Symbol.code");

  MemberFunctionSymbol.setFunctionSource("SOURCE_GET_TYPE",
                                         "../Grammar/Symbol.code");
  TemplateMemberFunctionSymbol.setFunctionSource("SOURCE_GET_TYPE",
                                                 "../Grammar/Symbol.code");

  LabelSymbol.setFunctionSource("SOURCE_LABEL_SYMBOL",
                                "../Grammar/Symbol.code");

  for (auto *symbol :
       {&VariableSymbol, &TemplateVariableSymbol, &FunctionSymbol,
        &TemplateFunctionSymbol, &ClassSymbol, &TemplateClassSymbol,
        &NonrealSymbol, &TemplateSymbol, &EnumSymbol, &EnumFieldSymbol,
        &TypedefSymbol, &TemplateTypedefSymbol, &MemberFunctionSymbol,
        &TemplateMemberFunctionSymbol, &ModuleSymbol, &IntrinsicSymbol,
        &InterfaceSymbol, &CommonSymbol})
    symbol->setFunctionSource("SOURCE_SHORT_GET_NAME",
                              "../Grammar/Symbol.code");

  MemberFunctionSymbol.setFunctionSource("SOURCE_DECLARATION",
                                         "../Grammar/Symbol.code");

  NamespaceSymbol.setFunctionSource("SOURCE_NAMESPACE_SYMBOL",
                                    "../Grammar/Symbol.code");
  NamespaceSymbol.setFunctionSource("SOURCE_EMPTY_GET_TYPE",
                                    "../Grammar/Symbol.code");

  DefaultSymbol.setFunctionSource("SOURCE_SHORT_DEFAULT_GET_NAME",
                                  "../Grammar/Symbol.code");

  AliasSymbol.setFunctionSource("SOURCE_ALIAS_SYMBOL",
                                "../Grammar/Symbol.code");

  RenameSymbol.setFunctionSource("SOURCE_RENAME_SYMBOL",
                                 "../Grammar/Symbol.code");
}
