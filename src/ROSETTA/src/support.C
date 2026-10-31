#include "featureTests.h"

#include "grammar.h"

#include "rose_config.h"

#include "AstNodeClass.h"

#include "grammarString.h"

#include "ROSETTA_macros.h"

void Grammar::setUpSupport() {
  NEW_TERMINAL_MACRO(Name, "Name", "NameTag");
  NEW_TERMINAL_MACRO(SymbolTable, "SymbolTable", "SymbolTableTag");

  NEW_TERMINAL_MACRO(Pragma, "Pragma", "PragmaTag");

  NEW_TERMINAL_MACRO(ModifierNodes, "ModifierNodes", "ModifierNodesTag");

  NEW_TERMINAL_MACRO(ConstVolatileModifier, "ConstVolatileModifier",
                     "ConstVolatileModifierTag");
  NEW_TERMINAL_MACRO(StorageModifier, "StorageModifier", "StorageModifierTag");
  NEW_TERMINAL_MACRO(AccessModifier, "AccessModifier", "AccessModifierTag");
  NEW_TERMINAL_MACRO(FunctionModifier, "FunctionModifier",
                     "FunctionModifierTag");
  NEW_TERMINAL_MACRO(LinkageModifier, "LinkageModifier", "LinkageModifierTag");
  NEW_TERMINAL_MACRO(SpecialFunctionModifier, "SpecialFunctionModifier",
                     "SpecialFunctionModifierTag");
  NEW_TERMINAL_MACRO(TypeModifier, "TypeModifier", "TypeModifierTag");
  NEW_TERMINAL_MACRO(ElaboratedTypeModifier, "ElaboratedTypeModifier",
                     "ElaboratedTypeModifierTag");
  NEW_TERMINAL_MACRO(BaseClassModifier, "BaseClassModifier",
                     "BaseClassModifierTag");
  NEW_TERMINAL_MACRO(DeclarationModifier, "DeclarationModifier",
                     "DeclarationModifierTag");

  NEW_TERMINAL_MACRO(OpenclAccessModeModifier, "OpenclAccessModeModifier",
                     "OPENCL_ACCESS_MODE");

  // DQ (4/21/2004): Order is important here since the objects are included in
  //                 other objects and the size must be known.
  NEW_NONTERMINAL_MACRO(
      Modifier,
      ModifierNodes | ConstVolatileModifier | StorageModifier | AccessModifier |
          FunctionModifier | SpecialFunctionModifier | ElaboratedTypeModifier |
          LinkageModifier | BaseClassModifier | TypeModifier |
          DeclarationModifier | OpenclAccessModeModifier,
      "Modifier", "ModifierTag", false);

  NEW_TERMINAL_MACRO(File_Info, "_File_Info", "_File_InfoTag");

  NEW_TERMINAL_MACRO(SourceFile, "SourceFile", "SourceFileTag");

  NEW_NONTERMINAL_MACRO(File, SourceFile, "File", "FileTag", false);
  NEW_TERMINAL_MACRO(FileList, "FileList", "FileListTag");
  NEW_TERMINAL_MACRO(Directory, "Directory", "DirectoryTag");
  NEW_TERMINAL_MACRO(DirectoryList, "DirectoryList", "DirectoryListTag");
  NEW_TERMINAL_MACRO(Project, "Project", "ProjectTag");
  Project.setAutomaticGenerationOfConstructor(false);
  NEW_TERMINAL_MACRO(Options, "Options", "OptionsTag");
  NEW_TERMINAL_MACRO(Unparse_Info, "Unparse_Info", "Unparse_InfoTag");

  NEW_TERMINAL_MACRO(IncludeFile, "IncludeFile", "IncludeFileTag");

  NEW_TERMINAL_MACRO(FuncDecl_attr, "FuncDecl_attr", "FuncDecl_attrTag");
  NEW_TERMINAL_MACRO(ClassDecl_attr, "ClassDecl_attr", "ClassDecl_attrTag");

  NEW_TERMINAL_MACRO(FunctionTypeArgument, "FunctionTypeArgument",
                     "T_FUNCTION_TYPE_ARGUMENT");
  NEW_TERMINAL_MACRO(OmpIteratorDefinition, "OmpIteratorDefinition",
                     "OMP_ITERATOR_DEFINITION");
  NEW_TERMINAL_MACRO(FunctionParameterTypeList, "FunctionParameterTypeList",
                     "T_FUNCTION_PARAMETER_TYPE_LIST");

  NEW_TERMINAL_MACRO(TemplateParameter, "TemplateParameter",
                     "TemplateParameterTag");
  NEW_TERMINAL_MACRO(TemplateArgument, "TemplateArgument",
                     "TemplateArgumentTag");

  NEW_TERMINAL_MACRO(TemplateParameterList, "TemplateParameterList",
                     "TemplateParameterListTag");

  NEW_NONTERMINAL_MACRO(BitAttribute, FuncDecl_attr | ClassDecl_attr,
                        "BitAttribute", "BitAttributeTag", false);

  NEW_NONTERMINAL_MACRO(Attribute, Pragma | BitAttribute, "Attribute",
                        "AttributeTag", false);

  // DQ (4/25/2004): Must be placed before the modifiers (since it includes one
  // as a data member)
  NEW_TERMINAL_MACRO(ExpBaseClass, "ExpBaseClass", "ExpBaseClassTag");
  NEW_TERMINAL_MACRO(NonrealBaseClass, "NonrealBaseClass",
                     "NonrealBaseClassTag");
  NEW_NONTERMINAL_MACRO(BaseClass, ExpBaseClass | NonrealBaseClass, "BaseClass",
                        "BaseClassTag", false);

  // Types of graph edges:
  NEW_TERMINAL_MACRO(UndirectedGraphEdge, "UndirectedGraphEdge",
                     "UndirectedGraphEdgeTag");
  NEW_TERMINAL_MACRO(DirectedGraphEdge, "DirectedGraphEdge",
                     "DirectedGraphEdgeTag");

  // Types of graph nodes and edges:
  NEW_TERMINAL_MACRO(GraphNode, "GraphNode", "GraphNodeTag");
  NEW_NONTERMINAL_MACRO(GraphEdge, DirectedGraphEdge | UndirectedGraphEdge,
                        "GraphEdge", "GraphEdgeTag", false);

  // Types of Bi-directional graphs:
  NEW_TERMINAL_MACRO(StringKeyedBidirectionalGraph,
                     "StringKeyedBidirectionalGraph",
                     "StringKeyedBidirectionalGraphTag");
  NEW_TERMINAL_MACRO(IntKeyedBidirectionalGraph, "IntKeyedBidirectionalGraph",
                     "IntKeyedBidirectionalGraphTag");
  NEW_NONTERMINAL_MACRO(BidirectionalGraph,
                        StringKeyedBidirectionalGraph |
                            IntKeyedBidirectionalGraph,
                        "BidirectionalGraph", "BidirectionalGraphTag", false);

  // A bi-directional graph is a type of directed graph (also called an
  // Incidence Directed Graph)
  NEW_NONTERMINAL_MACRO(IncidenceDirectedGraph, BidirectionalGraph,
                        "IncidenceDirectedGraph", "IncidenceDirectedGraphTag",
                        false);

  // Types of graphs:
  NEW_TERMINAL_MACRO(IncidenceUndirectedGraph, "IncidenceUndirectedGraph",
                     "IncidenceUndirectedGraphTag");
  NEW_NONTERMINAL_MACRO(Graph,
                        IncidenceDirectedGraph | IncidenceUndirectedGraph,
                        "Graph", "GraphTag", false);

  // Internal supporting IR nodes for graphs:
  NEW_TERMINAL_MACRO(GraphNodeList, "GraphNodeList", "GraphNodeListTag");
  NEW_TERMINAL_MACRO(GraphEdgeList, "GraphEdgeList", "GraphEdgeListTag");

  NEW_TERMINAL_MACRO(NameGroup, "NameGroup", "TEMP_Name_Group");

  NEW_TERMINAL_MACRO(DimensionObject, "DimensionObject",
                     "TEMP_DimensionObject");

  NEW_TERMINAL_MACRO(DataStatementGroup, "DataStatementGroup",
                     "TEMP_DataStatementGroup");
  NEW_TERMINAL_MACRO(DataStatementObject, "DataStatementObject",
                     "TEMP_DataStatementObject");
  NEW_TERMINAL_MACRO(DataStatementValue, "DataStatementValue",
                     "TEMP_DataStatementValue");

  NEW_TERMINAL_MACRO(FormatItem, "FormatItem", "TEMP_FormatItem");

  NEW_TERMINAL_MACRO(FormatItemList, "FormatItemList", "TEMP_FormatItemList");

  NEW_TERMINAL_MACRO(TypeTable, "TypeTable", "TYPE_TABLE");

  NEW_NONTERMINAL_MACRO(
      Support,
      Modifier | Name | SymbolTable | Attribute | File_Info | File | Project |
          Options | Unparse_Info | BaseClass | TemplateParameter |
          TemplateArgument | Directory | FileList | DirectoryList |
          FunctionTypeArgument | OmpIteratorDefinition |
          FunctionParameterTypeList | TemplateParameterList | Graph |
          GraphNode | GraphEdge | GraphNodeList | GraphEdgeList | TypeTable |
          NameGroup | DimensionObject | FormatItem | FormatItemList |
          DataStatementGroup | DataStatementObject | IncludeFile |
          DataStatementValue,
      "Support", "SupportTag", false);

  // ***********************************************************************
  // ***********************************************************************
  //                       Header Code Declaration
  // ***********************************************************************
  // ***********************************************************************

  // Header declarations for Support
  Support.setFunctionPrototype("HEADER", "../Grammar/Support.code");

  SymbolTable.setFunctionPrototype("HEADER_SYMBOL_TABLE",
                                   "../Grammar/Support.code");
  // SgSymbolTable keeps this cursor for its iteration API.
  GrammarString iterator("hash_iterator", "iterator", "",
                         NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                         NO_TRAVERSAL, NO_DELETE, COPY_DATA);
  // The cursor has no standalone value to format, especially at end().
  iterator.setIncludeInRTI(false);
  SymbolTable.setDataPrototype(iterator);

  // The name is used internally within the find member function to hold the
  // string being sought
  SymbolTable.setDataPrototype("SgName", "name", "= \"\"",
                               NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                               NO_TRAVERSAL, NO_DELETE);
  // The no_name is used internally within the find member function to indicate
  // if the name is being used
  SymbolTable.setDataPrototype("bool", "no_name", "= false",
                               NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                               NO_TRAVERSAL, NO_DELETE);

  SymbolTable.setDataPrototype(" rose_hash_multimap*", "table", "= nullptr",
                               NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                               NO_TRAVERSAL, DEF_DELETE);

  SymbolTable.setDataPrototype("SgNodeSet", "symbolSet", "",
                               NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                               NO_TRAVERSAL, NO_DELETE);

  TypeTable.setFunctionPrototype("HEADER_TYPE_TABLE",
                                 "../Grammar/Support.code");
  TypeTable.setAutomaticGenerationOfConstructor(false);
  TypeTable.setDataPrototype("SgSymbolTable*", "type_table", "= nullptr",
                             CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                             NO_TRAVERSAL, DEF_DELETE);

  Name.setFunctionPrototype("HEADER_NAME", "../Grammar/Support.code");

  Name.setDataPrototype("std::string", "char", "= \"\"",
                        NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                        NO_TRAVERSAL, NO_DELETE);

  Attribute.setFunctionPrototype("HEADER_ATTRIBUTE", "../Grammar/Support.code");
  Pragma.setFunctionPrototype("HEADER_PRAGMA", "../Grammar/Support.code");

  Modifier.setFunctionPrototype("HEADER_MODIFIER", "../Grammar/Support.code");
  ModifierNodes.setFunctionPrototype("HEADER_MODIFIER_NODES",
                                     "../Grammar/Support.code");
  ConstVolatileModifier.setFunctionPrototype("HEADER_CV_TYPE_MODIFIER",
                                             "../Grammar/Support.code");
  StorageModifier.setFunctionPrototype("HEADER_STORAGE_MODIFIER",
                                       "../Grammar/Support.code");
  AccessModifier.setFunctionPrototype("HEADER_ACCESS_MODIFIER",
                                      "../Grammar/Support.code");
  FunctionModifier.setFunctionPrototype("HEADER_FUNCTION_MODIFIER",
                                        "../Grammar/Support.code");
  SpecialFunctionModifier.setFunctionPrototype(
      "HEADER_SPECIAL_FUNCTION_MODIFIER", "../Grammar/Support.code");
  TypeModifier.setFunctionPrototype("HEADER_TYPE_MODIFIER",
                                    "../Grammar/Support.code");
  DeclarationModifier.setFunctionPrototype("HEADER_DECLARATION_MODIFIER",
                                           "../Grammar/Support.code");
  ElaboratedTypeModifier.setFunctionPrototype("HEADER_ELABORATED_TYPE_MODIFIER",
                                              "../Grammar/Support.code");
  LinkageModifier.setFunctionPrototype("HEADER_LINKAGE_MODIFIER",
                                       "../Grammar/Support.code");
  BaseClassModifier.setFunctionPrototype("HEADER_BASECLASS_MODIFIER",
                                         "../Grammar/Support.code");

  File_Info.setFunctionPrototype("HEADER_FILE_INFORMATION",
                                 "../Grammar/Support.code");

  SourceFile.setFunctionPrototype("HEADER_APPLICATION_SOURCE_FILE",
                                  "../Grammar/Support.code");
  IncludeFile.setFunctionPrototype("HEADER_INCLUDE_FILE",
                                   "../Grammar/Support.code");
  IncludeFile.setFunctionSource("SOURCE_INCLUDE_FILE",
                                "../Grammar/Support.code");

  File.setFunctionPrototype("HEADER_APPLICATION_FILE",
                            "../Grammar/Support.code");

  FileList.setFunctionPrototype("HEADER_APPLICATION_FILE_LIST",
                                "../Grammar/Support.code");
  Directory.setFunctionPrototype("HEADER_APPLICATION_DIRECTORY",
                                 "../Grammar/Support.code");
  DirectoryList.setFunctionPrototype("HEADER_APPLICATION_DIRECTORY_LIST",
                                     "../Grammar/Support.code");

  File_Info.setAutomaticGenerationOfConstructor(false);

  Project.setFunctionPrototype("HEADER_APPLICATION_PROJECT",
                               "../Grammar/Support.code");

  Options.setFunctionPrototype("HEADER_OPTIONS", "../Grammar/Support.code");
  Unparse_Info.setFunctionPrototype("HEADER_UNPARSE_INFO",
                                    "../Grammar/Support.code");
  // SgUnparse_Info is unparser configuration, not program AST.  Its setters do
  // not advance the AST modification sequence, so unparsing never looks like
  // an AST edit to caches and sessions keyed by that sequence.

  Unparse_Info.setDataPrototype("SgBitVector", "unparse_attribute", "",
                                NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                                NO_TRAVERSAL, NO_DELETE);
  // Space for access_attr_enum tag
  Unparse_Info.setDataPrototype(
      "int", "access_attribute", "= 0", NO_CONSTRUCTOR_PARAMETER,
      BUILD_FLAG_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  Unparse_Info.setDataPrototype("int", "nested_expression", "= 0",
                                NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                                NO_TRAVERSAL, NO_DELETE);
  Unparse_Info.setDataPrototype("std::string", "operator_name", "= \"\"",
                                NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                                NO_TRAVERSAL, NO_DELETE);
  // Keep ptr to the declaration statement
  Unparse_Info.setDataPrototype("SgDeclarationStatement*", "declstatement_ptr",
                                "= nullptr", NO_CONSTRUCTOR_PARAMETER,
                                BUILD_FLAG_ACCESS_FUNCTIONS, NO_TRAVERSAL,
                                NO_DELETE);
  // Preserve the exact statement where a shared template argument is emitted.
  // Contextual qualification is keyed by this use site, not by the shared
  // SgTemplateArgument alone.
  Unparse_Info.setDataPrototype(
      "SgStatement*", "template_argument_qualification_context", "= nullptr",
      NO_CONSTRUCTOR_PARAMETER, BUILD_FLAG_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);

  // All copies made during one unparser traversal share this state, while
  // separately constructed roots have independent state. This preserves the
  // inherited-attribute behavior without process-global leakage between files
  // or threads.
  Unparse_Info.setDataPrototype(
      "std::shared_ptr<UnparseSessionState>", "unparse_session_state", "",
      NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  // Build our own constructor to initialize the bit vector and create a fresh
  // session state. Copy construction preserves inherited-attribute context and
  // shares only that session state.
  Unparse_Info.setAutomaticGenerationOfConstructor(false);

  // DQ (10/20/2006): Added support for output of function calls.  We need to
  // know if we are unparsing a function call so that we can add the name
  // qualification properly.  The function call expression (SgFunctionCallExp)
  // is used to query the AST as ot the relative position of any defining or
  // nondefining function declarations that would define the scope of the
  // function.
  Unparse_Info.setDataPrototype("SgFunctionCallExp*", "current_function_call",
                                "= nullptr", NO_CONSTRUCTOR_PARAMETER,
                                BUILD_FLAG_ACCESS_FUNCTIONS, NO_TRAVERSAL,
                                NO_DELETE);

  Unparse_Info.setDataPrototype("SgScopeStatement*", "current_scope",
                                "= nullptr", NO_CONSTRUCTOR_PARAMETER,
                                BUILD_FLAG_ACCESS_FUNCTIONS, NO_TRAVERSAL,
                                NO_DELETE);

  // DQ (5/21/2011): Added information required for new name qualification
  // support. This is the node used to lookup the string computed to support the
  // name qualified name for either the declaration name or type name that
  // requires qualiticcation.
  Unparse_Info.setDataPrototype("SgNode*", "reference_node_for_qualification",
                                "= nullptr", NO_CONSTRUCTOR_PARAMETER,
                                BUILD_FLAG_ACCESS_FUNCTIONS, NO_TRAVERSAL,
                                NO_DELETE);

  Unparse_Info.setDataPrototype(
      "int", "name_qualification_length", "= 0", NO_CONSTRUCTOR_PARAMETER,
      BUILD_FLAG_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  Unparse_Info.setDataPrototype(
      "bool", "type_elaboration_required", "= false", NO_CONSTRUCTOR_PARAMETER,
      BUILD_FLAG_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  Unparse_Info.setDataPrototype("bool", "global_qualification_required",
                                "= false", NO_CONSTRUCTOR_PARAMETER,
                                BUILD_FLAG_ACCESS_FUNCTIONS, NO_TRAVERSAL,
                                NO_DELETE);

  Unparse_Info.setDataPrototype(
      "int", "nestingLevel", "= 0", NO_CONSTRUCTOR_PARAMETER,
      BUILD_FLAG_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE, COPY_DATA);

  // Explicit language selection supports unparsing before a node has a file
  // ancestor, including template arguments during construction.
  Unparse_Info.setDataPrototype(
      "SgFile::languageOption_enum", "language", "= SgFile::e_default_language",
      NO_CONSTRUCTOR_PARAMETER, BUILD_FLAG_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE, COPY_DATA);

  Unparse_Info.setDataPrototype("SgSourceFile*", "current_source_file",
                                "= nullptr", NO_CONSTRUCTOR_PARAMETER,
                                BUILD_FLAG_ACCESS_FUNCTIONS, NO_TRAVERSAL,
                                NO_DELETE);
  // A materialized header can have multiple semantic AST expansions in one
  // translation unit. Header emission selects one exact frontend-published
  // lexical occurrence and carries it through copied unparse contexts.
  Unparse_Info.setDataPrototype(
      "unsigned int", "current_physical_file_occurrence_id", "= 0",
      NO_CONSTRUCTOR_PARAMETER, BUILD_FLAG_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE, COPY_DATA);

  Unparse_Info.setDataPrototype(
      "bool", "use_generated_name_for_template_arguments", "= false",
      NO_CONSTRUCTOR_PARAMETER, BUILD_FLAG_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);

  Unparse_Info.setDataPrototype("bool", "context_for_added_parentheses",
                                "= false", NO_CONSTRUCTOR_PARAMETER,
                                BUILD_FLAG_ACCESS_FUNCTIONS, NO_TRAVERSAL,
                                NO_DELETE);

  BaseClass.setFunctionPrototype("HEADER_BASECLASS", "../Grammar/Support.code");
  ExpBaseClass.setFunctionPrototype("HEADER_EXP_BASE_CLASS",
                                    "../Grammar/Support.code");
  NonrealBaseClass.setFunctionPrototype("HEADER_NONREAL_BASE_CLASS",
                                        "../Grammar/Support.code");

  BaseClass.setDataPrototype("SgClassDeclaration*", "base_class", "= nullptr",
                             CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                             NO_TRAVERSAL, NO_DELETE, COPY_REFERENCE);

  // Preserve the exact type written at this inheritance use site. The
  // semantic base declaration is insufficient for typedef/alias spelling and
  // dependent template bases, and fabricating a declaration to carry that
  // spelling corrupts the class hierarchy.
  BaseClass.setDataPrototype("SgType*", "source_type", "= nullptr",
                             CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                             NO_TRAVERSAL, NO_DELETE, COPY_REFERENCE);
  BaseClass.setDataPrototype("SgSymbol*", "fortran_source_derived_type_symbol",
                             "= nullptr", NO_CONSTRUCTOR_PARAMETER,
                             BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE,
                             COPY_REFERENCE);

  // Preserve the exact nested-name-specifier written on this inheritance use
  // site.  SgType nodes are shared semantic identities, so source spelling
  // belongs to the SgBaseClass edge rather than to source_type.
  BaseClass.setDataPrototype("bool", "source_type_qualification_present",
                             "= false", NO_CONSTRUCTOR_PARAMETER,
                             BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE,
                             COPY_DATA);
  BaseClass.setDataPrototype("bool", "source_type_global_qualification",
                             "= false", NO_CONSTRUCTOR_PARAMETER,
                             BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE,
                             COPY_DATA);
  BaseClass.setDataPrototype("SgStringList", "source_type_qualification_tokens",
                             "", NO_CONSTRUCTOR_PARAMETER,
                             BUILD_LIST_ACCESS_FUNCTIONS, NO_TRAVERSAL,
                             NO_DELETE, COPY_DATA);
  // A macro invocation can provide both the semantic qualifier and the
  // terminal type name (and, for a function-like macro, its template
  // arguments). Record those source roles explicitly so the backend does not
  // append semantic components that were already spelled by the macro.
  BaseClass.setDataPrototype(
      "bool", "source_type_qualification_owns_terminal_name", "= false",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE,
      COPY_DATA);
  BaseClass.setDataPrototype(
      "bool", "source_type_qualification_owns_template_arguments", "= false",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE,
      COPY_DATA);

  BaseClass.setDataPrototype("bool", "isDirectBaseClass", "= false",
                             CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                             NO_TRAVERSAL, NO_DELETE);

  // Track whether this specific base-specifier carries an ellipsis
  // (e.g., "Bases...") so unparsing can preserve pack expansion directly
  // from AST structure rather than declaration-level attributes.
  BaseClass.setDataPrototype("bool", "pack_expansion", "= false",
                             NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                             NO_TRAVERSAL, NO_DELETE);

  BaseClass.setDataPrototype("SgBaseClassModifier*", "baseClassModifier",
                             "= nullptr", NO_CONSTRUCTOR_PARAMETER,
                             BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, DEF_DELETE,
                             CLONE_PTR);

  // Qualification accessors must not mark the AST as modified.
  BaseClass.setDataPrototype("int", "name_qualification_length", "= 0",
                             NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                             NO_TRAVERSAL, NO_DELETE);

  BaseClass.setDataPrototype("bool", "type_elaboration_required", "= false",
                             NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                             NO_TRAVERSAL, NO_DELETE);

  BaseClass.setDataPrototype("bool", "global_qualification_required", "= false",
                             NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                             NO_TRAVERSAL, NO_DELETE);

  NonrealBaseClass.setDataPrototype("SgNonrealDecl*", "base_class_nonreal",
                                    "= nullptr", CONSTRUCTOR_PARAMETER,
                                    BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
                                    NO_DELETE, COPY_REFERENCE);

  FuncDecl_attr.setFunctionPrototype("HEADER_FUNCTION_DECLARATION_ATTRIBUTE",
                                     "../Grammar/Support.code");
  ClassDecl_attr.setFunctionPrototype("HEADER_CLASS_DECLARATION_ATTRIBUTE",
                                      "../Grammar/Support.code");

  // DQ (1/11/13): The astCopy and astDelete are inconsistant if we share the
  // pointer (we need to copy using CLONE_PTR so that we will build a new
  // Sg_File_Info object).
  Pragma.setDataPrototype("Sg_File_Info*", "startOfConstruct", "= nullptr",
                          CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                          NO_TRAVERSAL, DEF_DELETE, CLONE_PTR);
  Pragma.setDataPrototype("Sg_File_Info*", "endOfConstruct", "= nullptr",
                          CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                          NO_TRAVERSAL, DEF_DELETE, CLONE_PTR);
  Pragma.setDataPrototype("short", "printed", "= 0", NO_CONSTRUCTOR_PARAMETER,
                          NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  Pragma.setDataPrototype("AstAttributeMechanism*", "attributeMechanism",
                          "= nullptr", NO_CONSTRUCTOR_PARAMETER,
                          BUILD_FLAG_ACCESS_FUNCTIONS, NO_TRAVERSAL, DEF_DELETE,
                          CLONE_PTR);
  Pragma.setFunctionPrototype("HEADER_ATTRIBUTE_SUPPORT",
                              "../Grammar/Support.code");
  Pragma.setFunctionSource("SOURCE_ATTRIBUTE_SUPPORT",
                           "../Grammar/Support.code");

  Directory.setDataPrototype("std::string", "name", "= \"\"",
                             CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                             NO_TRAVERSAL, NO_DELETE);
  Directory.setDataPrototype("SgFileList*", "fileList", "= nullptr",
                             NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                             DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
                             OPTIONAL_TRAVERSAL_MEMBER);

  Directory.setDataPrototype("SgDirectoryList*", "directoryList", "= nullptr",
                             NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                             DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
                             OPTIONAL_TRAVERSAL_MEMBER);

  Directory.setDataPrototype("AstAttributeMechanism*", "attributeMechanism",
                             "= nullptr", NO_CONSTRUCTOR_PARAMETER,
                             BUILD_FLAG_ACCESS_FUNCTIONS, NO_TRAVERSAL,
                             DEF_DELETE, CLONE_PTR);
  Directory.setFunctionPrototype("HEADER_ATTRIBUTE_SUPPORT",
                                 "../Grammar/Support.code");
  Directory.setFunctionSource("SOURCE_ATTRIBUTE_SUPPORT",
                              "../Grammar/Support.code");

  FileList.setDataPrototype(
      "SgFilePtrList", "listOfFiles", "", NO_CONSTRUCTOR_PARAMETER,
      BUILD_LIST_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE);

  FileList.setDataPrototype("AstAttributeMechanism*", "attributeMechanism",
                            "= nullptr", NO_CONSTRUCTOR_PARAMETER,
                            BUILD_FLAG_ACCESS_FUNCTIONS, NO_TRAVERSAL,
                            DEF_DELETE, CLONE_PTR);
  FileList.setFunctionPrototype("HEADER_ATTRIBUTE_SUPPORT",
                                "../Grammar/Support.code");
  FileList.setFunctionSource("SOURCE_ATTRIBUTE_SUPPORT",
                             "../Grammar/Support.code");

  DirectoryList.setDataPrototype(
      "SgDirectoryPtrList", "listOfDirectories", "", NO_CONSTRUCTOR_PARAMETER,
      BUILD_LIST_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE);

  // ******************************************************************
  // **************************** File IR Node ************************
  // ******************************************************************

  File.setDataPrototype("Sg_File_Info*", "startOfConstruct", "= nullptr",
                        NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                        NO_TRAVERSAL, DEF_DELETE, CLONE_PTR);

  SourceFile.setDataPrototype("SgGlobal*", "globalScope", "= nullptr",
                              NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                              DEF_TRAVERSAL, DELETE_IF_OWNED, CLONE_TREE,
                              OPTIONAL_TRAVERSAL_MEMBER);

  // Every source-file constructor owns its token associations. The global
  // source-file index observes this state and never owns it.
  SourceFile.setDataPrototype(
      "std::map<SgNode*, TokenStreamSequenceToNodeMapping*>*",
      "tokenSubsequenceMap", "= nullptr", NO_CONSTRUCTOR_PARAMETER,
      NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, DEF_DELETE, NO_COPY_DATA);
  SourceFile.setMemberDestructor("tokenSubsequenceMap",
                                 "release_tokenSubsequenceMap();");
  // Destroying a file is an AST modification.  State keyed by file identity,
  // such as an open qualification session, must observe it before the
  // address can be reused.
  SourceFile.setDestructorPrologue("markAsModified();");

  // Exact target ABI type returned by sizeof/alignof.  Clang publishes this
  // from ASTContext::getSizeType(); transformations must obtain it from their
  // one owning translation unit instead of guessing from the host or width.
  SourceFile.setDataPrototype("SgType*", "target_size_type", "= nullptr",
                              NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                              NO_TRAVERSAL, NO_DELETE);
  SourceFile.setDataPrototype("SgSourceFile::target_abi_info", "target_abi",
                              "= SgSourceFile::target_abi_info()",
                              NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                              NO_TRAVERSAL, NO_DELETE, COPY_DATA);

  SourceFile.setDataPrototype(
      "SgTokenPtrList", "token_list", "", NO_CONSTRUCTOR_PARAMETER,
      BUILD_LIST_ACCESS_FUNCTIONS, NO_TRAVERSAL, DEF_DELETE, CLONE_TREE);

  SourceFile.setDataPrototype("bool", "isHeaderFile", "= false",
                              NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                              NO_TRAVERSAL, NO_DELETE);

  // A generated source file is an output-only translation unit constructed
  // directly through SageBuilder.  It has no frontend input whose contents
  // the unparser could overwrite.  Keep this identity in the IR instead of
  // encoding it through a fabricated input filename.
  SourceFile.setDataPrototype("bool", "isGeneratedSource", "= false",
                              CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                              NO_TRAVERSAL, NO_DELETE);

  // A project can also contain parsed copies and transformation-created
  // output files.  Record the exact CLI-input role when SgProject constructs
  // the original translation units; file-list membership is not sufficient
  // to distinguish those identities.
  SourceFile.setDataPrototype("bool", "isCommandLineInputSource", "= false",
                              CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                              NO_TRAVERSAL, NO_DELETE);

  SourceFile.setDataPrototype("SgIncludeFile*", "associated_include_file",
                              "= nullptr", NO_CONSTRUCTOR_PARAMETER,
                              BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
                              DELETE_IF_OWNED, CLONE_TREE);

  // Exact physical paths observed by the active frontend's include callbacks.
  // This ownership metadata is independent of the optional SgIncludeFile tree,
  // which is materialized only for header-file unparsing.
  SourceFile.setDataPrototype(
      "SgStringList", "frontendIncludeOwnershipPathList", "",
      NO_CONSTRUCTOR_PARAMETER, BUILD_LIST_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);
  SourceFile.setDataPrototype(
      "SgStringList", "frontendSystemIncludeOwnershipPathList", "",
      NO_CONSTRUCTOR_PARAMETER, BUILD_LIST_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);
  // Exact active include edges reported by Clang.  The key is the resolved
  // physical target and each value is the source directive that produced that
  // edge.  Keep this on the translation-unit source file because the records
  // are frontend-owned until secondary preprocessing attachment completes;
  // header-tree construction must consume this typed relation instead of
  // resolving raw directive spellings a second time.
  SourceFile.setDataPrototype(
      "std::map<std::string, std::set<PreprocessingInfo*> >",
      "frontendResolvedIncludeDirectivesMap", "", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE, NO_COPY_DATA);
  // Physical dependencies whose declarations must not be mutated in place.
  // This includes system headers and imported Fortran module files.  Keeping
  // it separate from the include lists avoids pretending that every frontend
  // dependency is a C/C++ include.
  SourceFile.setDataPrototype(
      "SgStringList", "frontendExternalOwnershipPathList", "",
      NO_CONSTRUCTOR_PARAMETER, BUILD_LIST_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);
  // Imported modules and materialized header wrappers belong to the requesting
  // translation unit and are not lexical project inputs.
  SourceFile.setDataPrototype("SgFileList*", "frontendExternalFileList",
                              "= nullptr", NO_CONSTRUCTOR_PARAMETER,
                              BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, DEF_DELETE,
                              CLONE_TREE);

  // DQ (11/18/2019): Header file unparsing can depend on which SgGlobal (global
  // scope) is used from which source file. Where the IR nodes are not shared
  // across multiple files, getting the header file from the correct source file
  // is important. When the CPP directives and comments are collected are added
  // for a header file, the nodes that are not shared in global scope will not
  // have attached CPP directives and comments.  If those are the node in the
  // global scope of the SgSourceFile that represents a header file then we
  // don't unparse the header file correctly.
  SourceFile.setDataPrototype("bool",
                              "processedToIncludeCppDirectivesAndComments",
                              "= false", NO_CONSTRUCTOR_PARAMETER,
                              BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  IncludeFile.setDataPrototype("SgName", "filename", "= \"\"",
                               CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                               NO_TRAVERSAL, NO_DELETE);

  IncludeFile.setDataPrototype("SgSourceFile*", "source_file", " = nullptr",
                               NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                               NO_TRAVERSAL, NO_DELETE, COPY_REFERENCE);

  IncludeFile.setDataPrototype(
      "SgIncludeFilePtrList", "include_file_list", "", NO_CONSTRUCTOR_PARAMETER,
      BUILD_LIST_ACCESS_FUNCTIONS, DEF_TRAVERSAL, DEF_DELETE, CLONE_TREE);

  IncludeFile.setDataPrototype("SgName", "name_used_in_include_directive",
                               "= \"\"", NO_CONSTRUCTOR_PARAMETER,
                               BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  IncludeFile.setDataPrototype("SgSourceFile*",
                               "source_file_of_translation_unit", " = nullptr",
                               NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                               NO_TRAVERSAL, NO_DELETE, COPY_REFERENCE);
  IncludeFile.setDataPrototype("SgSourceFile*", "including_source_file",
                               " = nullptr", NO_CONSTRUCTOR_PARAMETER,
                               BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE,
                               COPY_REFERENCE);

  IncludeFile.setDataPrototype("SgIncludeFile*", "parent_include_file",
                               " = nullptr", NO_CONSTRUCTOR_PARAMETER,
                               BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE,
                               COPY_REFERENCE);

  IncludeFile.setDataPrototype("bool", "isSystemInclude", "= false",
                               NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                               NO_TRAVERSAL, NO_DELETE);
  IncludeFile.setDataPrototype("bool", "isPreinclude", "= false",
                               NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                               NO_TRAVERSAL, NO_DELETE);

  IncludeFile.setDataPrototype("SgName", "directory_prefix", "= \"\"",
                               NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                               NO_TRAVERSAL, NO_DELETE);

  IncludeFile.setDataPrototype("SgName", "applicationRootDirectory", "= \"\"",
                               NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                               NO_TRAVERSAL, NO_DELETE);

  IncludeFile.setDataPrototype("bool", "isApplicationFile", "= false",
                               NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                               NO_TRAVERSAL, NO_DELETE);

  IncludeFile.setDataPrototype("bool", "isRootSourceFile", "= false",
                               NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                               NO_TRAVERSAL, NO_DELETE);

  File.setDataPrototype("SgStringList", "originalCommandLineArgumentList", "",
                        NO_CONSTRUCTOR_PARAMETER, BUILD_LIST_ACCESS_FUNCTIONS,
                        NO_TRAVERSAL, NO_DELETE);
  File.setDataPrototype("int", "verbose", "= 0", NO_CONSTRUCTOR_PARAMETER,
                        BUILD_FLAG_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  File.setDataPrototype("bool", "C_only", "= false", NO_CONSTRUCTOR_PARAMETER,
                        BUILD_FLAG_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  File.setDataPrototype("bool", "Cxx_only", "= false", NO_CONSTRUCTOR_PARAMETER,
                        BUILD_FLAG_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  File.setDataPrototype("bool", "Fortran_only", "= false",
                        NO_CONSTRUCTOR_PARAMETER, BUILD_FLAG_ACCESS_FUNCTIONS,
                        NO_TRAVERSAL, NO_DELETE);
  File.setDataPrototype("bool", "CoArrayFortran_only", "= false",
                        NO_CONSTRUCTOR_PARAMETER, BUILD_FLAG_ACCESS_FUNCTIONS,
                        NO_TRAVERSAL, NO_DELETE);

  File.setDataPrototype("bool", "Cuda_only", "= false",
                        NO_CONSTRUCTOR_PARAMETER, BUILD_FLAG_ACCESS_FUNCTIONS,
                        NO_TRAVERSAL, NO_DELETE);
  File.setDataPrototype("bool", "OpenCL_only", "= false",
                        NO_CONSTRUCTOR_PARAMETER, BUILD_FLAG_ACCESS_FUNCTIONS,
                        NO_TRAVERSAL, NO_DELETE);

  File.setDataPrototype("bool", "requires_C_preprocessor", "= false",
                        NO_CONSTRUCTOR_PARAMETER, BUILD_FLAG_ACCESS_FUNCTIONS,
                        NO_TRAVERSAL, NO_DELETE);

  File.setDataPrototype("SgFile::outputFormatOption_enum", "inputFormat",
                        "= SgFile::e_unknown_output_format",
                        NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                        NO_TRAVERSAL, NO_DELETE);
  File.setDataPrototype("SgFile::outputFormatOption_enum", "outputFormat",
                        "= SgFile::e_unknown_output_format",
                        NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                        NO_TRAVERSAL, NO_DELETE);
  File.setDataPrototype(
      "SgFile::outputFormatOption_enum", "backendCompileFormat",
      "= SgFile::e_unknown_output_format", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  File.setDataPrototype("bool", "openmp", "= false", NO_CONSTRUCTOR_PARAMETER,
                        BUILD_FLAG_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  // Liao, 5/31/2009: Only invoke the OpenMP C/C++ and Fortran directive parsers
  // and generate OmpAttributes
  File.setDataPrototype("bool", "openmp_parse_only", "= false",
                        NO_CONSTRUCTOR_PARAMETER, BUILD_FLAG_ACCESS_FUNCTIONS,
                        NO_TRAVERSAL, NO_DELETE);
  // Only generate SgOmp* nodes from OmpAttributes in AST, no further actions
  File.setDataPrototype("bool", "openmp_ast_only", "= false",
                        NO_CONSTRUCTOR_PARAMETER, BUILD_FLAG_ACCESS_FUNCTIONS,
                        NO_TRAVERSAL, NO_DELETE);
  // Lowering OpenMP directives to code with explicit runtime calls
  File.setDataPrototype("bool", "openmp_lowering", "= false",
                        NO_CONSTRUCTOR_PARAMETER, BUILD_FLAG_ACCESS_FUNCTIONS,
                        NO_TRAVERSAL, NO_DELETE);
  // Analyze OpenMP constructs without lowering (analysis-only mode)
  File.setDataPrototype("bool", "openmp_analyzing", "= false",
                        NO_CONSTRUCTOR_PARAMETER, BUILD_FLAG_ACCESS_FUNCTIONS,
                        NO_TRAVERSAL, NO_DELETE);
  // Track whether OpenMP AST construction has already run for this file.
  File.setDataPrototype("bool", "openmp_processed", "= false",
                        NO_CONSTRUCTOR_PARAMETER, BUILD_FLAG_ACCESS_FUNCTIONS,
                        NO_TRAVERSAL, NO_DELETE);
  File.setDataPrototype("bool", "openacc", "= false", NO_CONSTRUCTOR_PARAMETER,
                        BUILD_FLAG_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  File.setDataPrototype("bool", "openacc_parse_only", "= false",
                        NO_CONSTRUCTOR_PARAMETER, BUILD_FLAG_ACCESS_FUNCTIONS,
                        NO_TRAVERSAL, NO_DELETE);
  File.setDataPrototype("bool", "openacc_ast_only", "= false",
                        NO_CONSTRUCTOR_PARAMETER, BUILD_FLAG_ACCESS_FUNCTIONS,
                        NO_TRAVERSAL, NO_DELETE);

  File.setDataPrototype("bool", "cray_pointer_support", "= false",
                        NO_CONSTRUCTOR_PARAMETER, BUILD_FLAG_ACCESS_FUNCTIONS,
                        NO_TRAVERSAL, NO_DELETE);

  File.setDataPrototype("bool", "exit_after_parser", "= false",
                        NO_CONSTRUCTOR_PARAMETER, BUILD_FLAG_ACCESS_FUNCTIONS,
                        NO_TRAVERSAL, NO_DELETE);
  // TV (09/24/2018): Adding the ability to skip the parser when testing the
  // backend with ROSE command line processing
  File.setDataPrototype("bool", "skip_parser", "= false",
                        NO_CONSTRUCTOR_PARAMETER, BUILD_FLAG_ACCESS_FUNCTIONS,
                        NO_TRAVERSAL, NO_DELETE);

  File.setDataPrototype("bool", "skip_transformation", "= false",
                        NO_CONSTRUCTOR_PARAMETER, BUILD_FLAG_ACCESS_FUNCTIONS,
                        NO_TRAVERSAL, NO_DELETE);
  File.setDataPrototype("bool", "skip_unparse", "= false",
                        NO_CONSTRUCTOR_PARAMETER, BUILD_FLAG_ACCESS_FUNCTIONS,
                        NO_TRAVERSAL, NO_DELETE);
  File.setDataPrototype("bool", "skipfinalCompileStep", "= false",
                        NO_CONSTRUCTOR_PARAMETER, BUILD_FLAG_ACCESS_FUNCTIONS,
                        NO_TRAVERSAL, NO_DELETE);

  // Code generation options:
  File.setDataPrototype("bool", "unparse_includes", "= false",
                        NO_CONSTRUCTOR_PARAMETER, BUILD_FLAG_ACCESS_FUNCTIONS,
                        NO_TRAVERSAL, NO_DELETE);

  // DQ (12/4/2007): Added command-line support for unparsing #line directives
  // (previously removed feature) The use of #line directives permits the
  // debugger to reference the original source code instead of ROSE generated
  // source code.
  File.setDataPrototype("bool", "unparse_line_directives", "= false",
                        NO_CONSTRUCTOR_PARAMETER, BUILD_FLAG_ACCESS_FUNCTIONS,
                        NO_TRAVERSAL, NO_DELETE);

  // Selecting an output language does not translate the AST's constructs
  // between languages.
  File.setDataPrototype("SgFile::languageOption_enum", "outputLanguage",
                        "= SgFile::e_default_language",
                        NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                        NO_TRAVERSAL, NO_DELETE);

  File.setDataPrototype("SgFile::languageOption_enum", "inputLanguage",
                        "= SgFile::e_default_language",
                        NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                        NO_TRAVERSAL, NO_DELETE);

  // Internal data
  File.setDataPrototype("std::string", "sourceFileNameWithPath", "= \"\"",
                        NO_CONSTRUCTOR_PARAMETER, BUILD_FLAG_ACCESS_FUNCTIONS,
                        NO_TRAVERSAL, NO_DELETE);
  File.setDataPrototype("std::string", "sourceFileNameWithoutPath", "= \"\"",
                        NO_CONSTRUCTOR_PARAMETER, BUILD_FLAG_ACCESS_FUNCTIONS,
                        NO_TRAVERSAL, NO_DELETE);
  File.setDataPrototype("std::string", "unparse_output_filename", "= \"\"",
                        NO_CONSTRUCTOR_PARAMETER, BUILD_FLAG_ACCESS_FUNCTIONS,
                        NO_TRAVERSAL, NO_DELETE);

  // DQ (4/17/2015): Adding support to save the original specification of the
  // object file name from the command line (as specified using the "-o"
  // option). This is required for multiple file support which is using the "-o"
  // option to specify the executable name when linking.  We have to split up
  // the command so that we can call ROSE separately with each file to generate
  // a new source file and then call the backend compiler to generate the object
  // file, and then call the linker separately to using the object files to
  // generate the named executable.
  File.setDataPrototype("std::string", "objectFileNameWithPath", "= \"\"",
                        NO_CONSTRUCTOR_PARAMETER, BUILD_FLAG_ACCESS_FUNCTIONS,
                        NO_TRAVERSAL, NO_DELETE);
  File.setDataPrototype("std::string", "objectFileNameWithoutPath", "= \"\"",
                        NO_CONSTRUCTOR_PARAMETER, BUILD_FLAG_ACCESS_FUNCTIONS,
                        NO_TRAVERSAL, NO_DELETE);

  File.setDataPrototype("bool", "useBackendOnly", "= false",
                        NO_CONSTRUCTOR_PARAMETER, BUILD_FLAG_ACCESS_FUNCTIONS,
                        NO_TRAVERSAL, NO_DELETE);

  File.setDataPrototype("bool", "compileOnly", "= false",
                        NO_CONSTRUCTOR_PARAMETER, BUILD_FLAG_ACCESS_FUNCTIONS,
                        NO_TRAVERSAL, NO_DELETE);

  // DQ (4/20/2006): Permit skipping all comments and CPP directives (provides
  // faster processing) But this can not be used if generating code to be
  // compiled by the backend.
  File.setDataPrototype("bool", "skip_commentsAndDirectives", "= false",
                        NO_CONSTRUCTOR_PARAMETER, BUILD_FLAG_ACCESS_FUNCTIONS,
                        NO_TRAVERSAL, NO_DELETE);

  File.setDataPrototype("bool", "collectAllCommentsAndDirectives", "= false",
                        NO_CONSTRUCTOR_PARAMETER, BUILD_FLAG_ACCESS_FUNCTIONS,
                        NO_TRAVERSAL, NO_DELETE);

  File.setDataPrototype("bool", "unparseHeaderFiles", "= false",
                        NO_CONSTRUCTOR_PARAMETER, BUILD_FLAG_ACCESS_FUNCTIONS,
                        NO_TRAVERSAL, NO_DELETE);

  File.setDataPrototype("ROSEAttributesListContainer*",
                        "preprocessorDirectivesAndCommentsList", "= nullptr",
                        NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                        NO_TRAVERSAL, DEF_DELETE, CLONE_PTR);

  File.setDataPrototype("AstAttributeMechanism*", "attributeMechanism",
                        "= nullptr", NO_CONSTRUCTOR_PARAMETER,
                        BUILD_FLAG_ACCESS_FUNCTIONS, NO_TRAVERSAL, DEF_DELETE,
                        CLONE_PTR);
  File.setFunctionPrototype("HEADER_ATTRIBUTE_SUPPORT",
                            "../Grammar/Support.code");
  File.setFunctionSource("SOURCE_ATTRIBUTE_SUPPORT", "../Grammar/Support.code");

  File.setDataPrototype("bool", "markGeneratedFiles", "= false",
                        NO_CONSTRUCTOR_PARAMETER, BUILD_FLAG_ACCESS_FUNCTIONS,
                        NO_TRAVERSAL, NO_DELETE);

  File.setDataPrototype("bool", "negative_test", "= false",
                        NO_CONSTRUCTOR_PARAMETER, BUILD_FLAG_ACCESS_FUNCTIONS,
                        NO_TRAVERSAL, NO_DELETE);

  File.setDataPrototype("bool", "strict_language_handling", "= false",
                        NO_CONSTRUCTOR_PARAMETER, BUILD_FLAG_ACCESS_FUNCTIONS,
                        NO_TRAVERSAL, NO_DELETE);

  File.setDataPrototype("bool", "sourceFileUsesCppFileExtension", "= false",
                        NO_CONSTRUCTOR_PARAMETER, BUILD_FLAG_ACCESS_FUNCTIONS,
                        NO_TRAVERSAL, NO_DELETE);

  File.setDataPrototype("bool", "sourceFileUsesFortranFileExtension", "= false",
                        NO_CONSTRUCTOR_PARAMETER, BUILD_FLAG_ACCESS_FUNCTIONS,
                        NO_TRAVERSAL, NO_DELETE);
  File.setDataPrototype("bool", "sourceFileUsesFortran77FileExtension",
                        "= false", NO_CONSTRUCTOR_PARAMETER,
                        BUILD_FLAG_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  File.setDataPrototype("bool", "sourceFileUsesFortran90FileExtension",
                        "= false", NO_CONSTRUCTOR_PARAMETER,
                        BUILD_FLAG_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  File.setDataPrototype("bool", "sourceFileUsesFortran95FileExtension",
                        "= false", NO_CONSTRUCTOR_PARAMETER,
                        BUILD_FLAG_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  File.setDataPrototype("bool", "sourceFileUsesFortran2003FileExtension",
                        "= false", NO_CONSTRUCTOR_PARAMETER,
                        BUILD_FLAG_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  File.setDataPrototype("bool", "sourceFileUsesFortran2008FileExtension",
                        "= false", NO_CONSTRUCTOR_PARAMETER,
                        BUILD_FLAG_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  File.setDataPrototype("bool", "sourceFileUsesCoArrayFortranFileExtension",
                        "= false", NO_CONSTRUCTOR_PARAMETER,
                        BUILD_FLAG_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  // DQ (9/26/2011): Added support to detect dangling pointers in ROSE
  // translators. This is not an expensive test, but it fails for isolated parts
  // of ROSE currently so it should be made optional at this early stage (before
  // it is made a default test in the AST consistency tests).
  File.setDataPrototype("int", "detect_dangling_pointers", "= false",
                        NO_CONSTRUCTOR_PARAMETER, BUILD_FLAG_ACCESS_FUNCTIONS,
                        NO_TRAVERSAL, NO_DELETE);

  // DQ (11/20/2010): Unparse using tokens were possible instead of from the
  // AST.  This is possible where transformations have not been done (i.e. where
  // the AST has not be modified).  Where this is possible the generated code is
  // fundamentally more portable and can be diff'ed against to generate patches
  // (useful on large scale projects to design new tools).
  File.setDataPrototype("bool", "unparse_tokens", "= false",
                        NO_CONSTRUCTOR_PARAMETER, BUILD_FLAG_ACCESS_FUNCTIONS,
                        NO_TRAVERSAL, NO_DELETE);

  File.setDataPrototype("bool", "unparse_template_ast", "= false",
                        NO_CONSTRUCTOR_PARAMETER, BUILD_FLAG_ACCESS_FUNCTIONS,
                        NO_TRAVERSAL, NO_DELETE);

  // DQ (2/17/2013): Added support to skip AST consistancy testing
  // AstTests::runAllTests(SgProject*) This testing is useful but interferes
  // with performance testing using HPCToolKit. Note that the
  // AstTests::runAllTests() function must be called explicitly in the user's
  // translator.
  File.setDataPrototype("bool", "skipAstConsistancyTests", "= false",
                        NO_CONSTRUCTOR_PARAMETER, BUILD_FLAG_ACCESS_FUNCTIONS,
                        NO_TRAVERSAL, NO_DELETE);

  File.setDataPrototype("bool", "multifile_support", "= false",
                        NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                        NO_TRAVERSAL, NO_DELETE);

  // DQ (5/24/2015): Record if optimization is specified on the command line
  // (later maybe also save what level). This is required to set the
  // __OPTIMIZED__ macro (to follow the GNU API).  See test2015_153.c.
  File.setDataPrototype("bool", "optimization", "= false",
                        NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                        NO_TRAVERSAL, NO_DELETE);

  File.setDataPrototype("bool", "clang_il_to_graphviz", "= false",
                        NO_CONSTRUCTOR_PARAMETER, BUILD_FLAG_ACCESS_FUNCTIONS,
                        NO_TRAVERSAL, NO_DELETE);

  // Header files selected for unparsing collect their preprocessing records.
  File.setDataPrototype("bool",
                        "header_file_unparsing_optimization_header_file",
                        "= false", NO_CONSTRUCTOR_PARAMETER,
                        BUILD_FLAG_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  File.setDataPrototype("SgFile::standard_enum", "standard",
                        "= e_default_standard", NO_CONSTRUCTOR_PARAMETER,
                        NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  File.setDataPrototype("bool", "gnu_standard", "= false",
                        NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                        NO_TRAVERSAL, NO_DELETE);

  // ******************************************************************************
  // ******************************************************************************
  //                             ROSE Graph IR Nodes
  // ******************************************************************************
  // ******************************************************************************

  // ******************************************************************************
  //                                SgGraphNode
  // ******************************************************************************

  GraphNode.setFunctionPrototype("HEADER_GRAPHNODE", "../Grammar/Support.code");

  GraphNode.setDataPrototype("std::string", "name", "= \"\"",
                             CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                             NO_TRAVERSAL, NO_DELETE);

  GraphNode.setDataPrototype("SgNode*", "SgNode", "= nullptr",
                             NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                             NO_TRAVERSAL, NO_DELETE);

  GraphNode.setDataPrototype("int", "index", "= -1", NO_CONSTRUCTOR_PARAMETER,
                             BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  GraphNode.setDataPrototype("static int", "index_counter", "",
                             NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                             NO_TRAVERSAL, NO_DELETE);

  GraphNode.setDataPrototype("AstAttributeMechanism*", "attributeMechanism",
                             "= nullptr", NO_CONSTRUCTOR_PARAMETER,
                             BUILD_FLAG_ACCESS_FUNCTIONS, NO_TRAVERSAL,
                             DEF_DELETE, CLONE_PTR);

  GraphNode.setFunctionPrototype("HEADER_ATTRIBUTE_SUPPORT",
                                 "../Grammar/Support.code");
  GraphNode.setFunctionSource("SOURCE_ATTRIBUTE_SUPPORT",
                              "../Grammar/Support.code");

  // ******************************************************************************
  //                                SgGraphEdge
  // ******************************************************************************

  GraphEdge.setFunctionPrototype("HEADER_GRAPHEDGE", "../Grammar/Support.code");
  GraphEdge.setDataPrototype("SgGraphNode*", "node_A", "= nullptr",
                             CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                             NO_TRAVERSAL, NO_DELETE);
  GraphEdge.setDataPrototype("SgGraphNode*", "node_B", "= nullptr",
                             CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                             NO_TRAVERSAL, NO_DELETE);
  GraphEdge.setDataPrototype("std::string", "name", "= \"\"",
                             CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                             NO_TRAVERSAL, NO_DELETE);

  GraphEdge.setDataPrototype("int", "index", "= -1", NO_CONSTRUCTOR_PARAMETER,
                             BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  GraphEdge.setDataPrototype("static int", "index_counter", "",
                             NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                             NO_TRAVERSAL, NO_DELETE);

  GraphEdge.setDataPrototype("AstAttributeMechanism*", "attributeMechanism",
                             "= nullptr", NO_CONSTRUCTOR_PARAMETER,
                             BUILD_FLAG_ACCESS_FUNCTIONS, NO_TRAVERSAL,
                             DEF_DELETE, CLONE_PTR);
  GraphEdge.setFunctionPrototype("HEADER_ATTRIBUTE_SUPPORT",
                                 "../Grammar/Support.code");
  GraphEdge.setFunctionSource("SOURCE_ATTRIBUTE_SUPPORT",
                              "../Grammar/Support.code");

  // ******************************************************************************
  //                      Edge Types derived from SgGraphEdge
  // ******************************************************************************

  DirectedGraphEdge.setFunctionPrototype("HEADER_DIRECTED_GRAPH_EDGE",
                                         "../Grammar/Support.code");

  // ******************************************************************************
  //                                  SgGraph
  // ******************************************************************************

  Graph.setPredeclarationString("HEADER_GRAPH_PREDECLARATION",
                                "../Grammar/Support.code");
  Graph.setFunctionPrototype("HEADER_GRAPH", "../Grammar/Support.code");
  Graph.setDataPrototype("std::string", "name", "= \"\"", CONSTRUCTOR_PARAMETER,
                         BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  Graph.setDataPrototype("rose_graph_integer_node_hash_map",
                         "node_index_to_node_map", "", NO_CONSTRUCTOR_PARAMETER,
                         BUILD_LIST_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  Graph.setDataPrototype("rose_graph_integer_edge_hash_map",
                         "edge_index_to_edge_map", "", NO_CONSTRUCTOR_PARAMETER,
                         BUILD_LIST_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  Graph.setDataPrototype("rose_graph_integerpair_edge_hash_multimap",
                         "node_index_pair_to_edge_multimap", "",
                         NO_CONSTRUCTOR_PARAMETER, BUILD_LIST_ACCESS_FUNCTIONS,
                         NO_TRAVERSAL, NO_DELETE);
  Graph.setDataPrototype("rose_graph_string_integer_hash_multimap",
                         "string_to_edge_index_multimap", "",
                         NO_CONSTRUCTOR_PARAMETER, BUILD_LIST_ACCESS_FUNCTIONS,
                         NO_TRAVERSAL, NO_DELETE);

  Graph.setDataPrototype("int", "index", "= -1", NO_CONSTRUCTOR_PARAMETER,
                         BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  Graph.setDataPrototype("static int", "index_counter", "",
                         NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                         NO_TRAVERSAL, NO_DELETE);

  Graph.setDataPrototype("AstAttributeMechanism*", "attributeMechanism",
                         "= nullptr", NO_CONSTRUCTOR_PARAMETER,
                         BUILD_FLAG_ACCESS_FUNCTIONS, NO_TRAVERSAL, DEF_DELETE,
                         CLONE_PTR);

  Graph.setFunctionPrototype("HEADER_ATTRIBUTE_SUPPORT",
                             "../Grammar/Support.code");
  Graph.setFunctionSource("SOURCE_ATTRIBUTE_SUPPORT",
                          "../Grammar/Support.code");

  // ******************************************************************************
  //                          Graphs derived from SgGraph
  // ******************************************************************************

  IncidenceDirectedGraph.setFunctionPrototype("HEADER_INCIDENCE_DIRECTED_GRAPH",
                                              "../Grammar/Support.code");
  IncidenceDirectedGraph.setFunctionSource("SOURCE_INCIDENCE_DIRECTED_GRAPH",
                                           "../Grammar/Support.code");

  IncidenceDirectedGraph.setDataPrototype(
      "rose_graph_integer_edge_hash_multimap",
      "node_index_to_edge_multimap_edgesOut", "", NO_CONSTRUCTOR_PARAMETER,
      BUILD_LIST_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  IncidenceDirectedGraph.setDataPrototype(
      "rose_graph_integer_edge_hash_multimap",
      "node_index_to_edge_multimap_edgesIn", "", NO_CONSTRUCTOR_PARAMETER,
      BUILD_LIST_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  // ******************************************************************
  // ************************* Project IR Node ************************
  // ******************************************************************

  // DQ (1/20/2010): Change to provide uniformity in ROSE for how SgFile is
  // handled (just like any other IR node). This avoids was was previously
  // references that we returned and allows the ROSETTA rules to be followed
  // that avoid having more than one list or mixing lists with data members.
  // SgFileList is an IR node and this is a data member pointer of that type.
  // Allow Project constructor to be automatically generated, see gitlab #126
  // [Rasmussen 10.05.06]
  // Runtime state belongs to one project; copies start fresh.
  Project.setDataPrototype("MutationState*", "mutationState", "= nullptr",
                           NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                           NO_TRAVERSAL, NO_DELETE, NO_COPY_DATA);
  Project.setMemberDestructor("mutationState", "release_mutationState();");
  Project.setDataPrototype("CallGraphState*", "callGraphState", "= nullptr",
                           NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                           NO_TRAVERSAL, NO_DELETE, NO_COPY_DATA);
  Project.setMemberDestructor("callGraphState", "release_callGraphState();");
  // Destroying a project is an AST modification, like destroying a file.
  Project.setDestructorPrologue("markAsModified();");

  Project.setDataPrototype("SgFileList*", "fileList_ptr", "= nullptr",
                           NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                           DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
                           OPTIONAL_TRAVERSAL_MEMBER);

  Project.setDataPrototype("SgStringList", "originalCommandLineArgumentList",
                           "", NO_CONSTRUCTOR_PARAMETER,
                           BUILD_LIST_ACCESS_FUNCTIONS, NO_TRAVERSAL,
                           NO_DELETE);
  Project.setDataPrototype(
      "std::map<std::string, std::string>", "original_header_snapshots", "",
      NO_CONSTRUCTOR_PARAMETER, BUILD_LIST_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);

  Project.setDataPrototype("int", "frontendErrorCode", "= 0",
                           NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                           NO_TRAVERSAL, NO_DELETE);
  Project.setDataPrototype("int", "midendErrorCode", "= 0",
                           NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                           NO_TRAVERSAL, NO_DELETE);
  Project.setDataPrototype(
      "int", "backendErrorCode", "= 0", NO_CONSTRUCTOR_PARAMETER,
      BUILD_FLAG_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  File.setDataPrototype("int", "frontendErrorCode", "= 0",
                        NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                        NO_TRAVERSAL, NO_DELETE);
  File.setDataPrototype("int", "midendErrorCode", "= 0",
                        NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                        NO_TRAVERSAL, NO_DELETE);
  File.setDataPrototype("int", "unparserErrorCode", "= 0",
                        NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                        NO_TRAVERSAL, NO_DELETE);

  File.setDataPrototype("int", "backendCompilerErrorCode", "= 0",
                        NO_CONSTRUCTOR_PARAMETER, BUILD_FLAG_ACCESS_FUNCTIONS,
                        NO_TRAVERSAL, NO_DELETE);

  // TOO1 (03/20/2014): Dangerous rope for Pontetec,
  // -rose:unparser:clobber_input_file
  Project.setDataPrototype("bool", "unparser__clobber_input_file", "= false",
                           NO_CONSTRUCTOR_PARAMETER,
                           BUILD_FLAG_ACCESS_FUNCTIONS, NO_TRAVERSAL,
                           NO_DELETE);

  Project.setDataPrototype("std::string", "outputFileName", "= \"\"",
                           NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                           NO_TRAVERSAL, NO_DELETE);
  Project.setDataPrototype(
      "SgStringList", "sourceFileNameList", "", NO_CONSTRUCTOR_PARAMETER,
      BUILD_LIST_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  Project.setDataPrototype(
      "SgStringList", "objectFileNameList", "", NO_CONSTRUCTOR_PARAMETER,
      BUILD_LIST_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  Project.setDataPrototype(
      "SgStringList", "libraryFileList", "", NO_CONSTRUCTOR_PARAMETER,
      BUILD_LIST_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  Project.setDataPrototype(
      "SgStringList", "librarySpecifierList", "", NO_CONSTRUCTOR_PARAMETER,
      BUILD_LIST_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  Project.setDataPrototype("SgStringList", "libraryDirectorySpecifierList", "",
                           NO_CONSTRUCTOR_PARAMETER,
                           BUILD_LIST_ACCESS_FUNCTIONS, NO_TRAVERSAL,
                           NO_DELETE);

  Project.setDataPrototype("SgStringList", "includeDirectorySpecifierList", "",
                           NO_CONSTRUCTOR_PARAMETER,
                           BUILD_LIST_ACCESS_FUNCTIONS, NO_TRAVERSAL,
                           NO_DELETE);

  // DQ (10/18/2010): Added support to extract "-D" options so that for Fortran
  // we could process them separately by triggering the use of the C
  // preprocessor and passing them to CPP explicitly.
  Project.setDataPrototype(
      "SgStringList", "macroSpecifierList", "", NO_CONSTRUCTOR_PARAMETER,
      BUILD_LIST_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  Project.setDataPrototype(
      "SgStringList", "preincludeFileList", "", NO_CONSTRUCTOR_PARAMETER,
      BUILD_LIST_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  Project.setDataPrototype(
      "SgStringList", "preincludeDirectoryList", "", NO_CONSTRUCTOR_PARAMETER,
      BUILD_LIST_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  Project.setDataPrototype("bool", "compileOnly", "= false",
                           NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                           NO_TRAVERSAL, NO_DELETE);

  // Milind Chabbi (9/9/2013): Added a commandline option to use a file to
  // generate persistent id for files used in different compilation units.
  Project.setDataPrototype("std::string", "projectSpecificDatabaseFile",
                           "= \"\"", NO_CONSTRUCTOR_PARAMETER,
                           BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  // DQ (10/16/2005): Added support to detect use of "-E" on compiler's command
  // line. Special processing is done when "-E" is used with multiple file names
  // on the command line.
  Project.setDataPrototype("bool", "C_PreprocessorOnly", "= false",
                           NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                           NO_TRAVERSAL, NO_DELETE);

  Project.setDataPrototype("AstAttributeMechanism*", "attributeMechanism",
                           "= nullptr", NO_CONSTRUCTOR_PARAMETER,
                           BUILD_FLAG_ACCESS_FUNCTIONS, NO_TRAVERSAL,
                           DEF_DELETE, CLONE_PTR);
  Project.setFunctionPrototype("HEADER_ATTRIBUTE_SUPPORT",
                               "../Grammar/Support.code");
  Project.setFunctionSource("SOURCE_ATTRIBUTE_SUPPORT",
                            "../Grammar/Support.code");

  // DQ (8/29/2006): Support for CSV data file reporting performance of
  // compilation. This file accumulates information (if specified) and permits
  // plots of performance for different parts of the compilation internally
  // within ROSE.
  Project.setDataPrototype("std::string", "compilationPerformanceFile",
                           "= \"\"", NO_CONSTRUCTOR_PARAMETER,
                           BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  // Translators define the semantics of these include/exclude filters.
  Project.setDataPrototype("SgStringList", "includePathList", "",
                           NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                           NO_TRAVERSAL, NO_DELETE);
  Project.setDataPrototype("SgStringList", "excludePathList", "",
                           NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                           NO_TRAVERSAL, NO_DELETE);
  Project.setDataPrototype("SgStringList", "includeFileList", "",
                           NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                           NO_TRAVERSAL, NO_DELETE);
  Project.setDataPrototype("SgStringList", "excludeFileList", "",
                           NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                           NO_TRAVERSAL, NO_DELETE);
  Project.setDataPrototype("std::string", "dataBaseFilename", "= \"\"",
                           NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                           NO_TRAVERSAL, NO_DELETE);

  // DQ (1/20/2010): Added list of directories to the SgProject to represent
  // large scale projects (as can be useful for code generation). Note that this
  // is marked as NO_TRAVERSAL because it will otherwise interfere with
  // "SgFilePtrList fileList" This can be changed later, but it could effect a
  // number of interfaces internally, so don't change it yet.
  Project.setDataPrototype("SgDirectoryList*", "directoryList", "= nullptr",
                           NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                           NO_TRAVERSAL, DEF_DELETE, CLONE_TREE);

  // DQ (4/7/2010): Support for C, C++, and Fortran. The SgProject needs this
  // state so that it can save information from the command line when there are
  // no files such as when it is used for linking (where there is just a list of
  // object files).
  Project.setDataPrototype("bool", "C_only", "= false",
                           NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                           NO_TRAVERSAL, NO_DELETE);
  Project.setDataPrototype("bool", "Cxx_only", "= false",
                           NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                           NO_TRAVERSAL, NO_DELETE);

  Project.setDataPrototype("bool", "C11_only", "= false",
                           NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                           NO_TRAVERSAL, NO_DELETE);
  Project.setDataPrototype("bool", "Cxx0x_only", "= false",
                           NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                           NO_TRAVERSAL, NO_DELETE);
  Project.setDataPrototype("bool", "Cxx11_only", "= false",
                           NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                           NO_TRAVERSAL, NO_DELETE);

  Project.setDataPrototype("bool", "C14_only", "= false",
                           NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                           NO_TRAVERSAL, NO_DELETE);
  Project.setDataPrototype("bool", "Cxx14_only", "= false",
                           NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                           NO_TRAVERSAL, NO_DELETE);

  Project.setDataPrototype("bool", "Fortran_only", "= false",
                           NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                           NO_TRAVERSAL, NO_DELETE);

  // Liao 6/29/2012: support using rose translator to act like a linker wrapper
  // if -rose:openmp:lowering is specified Add a flag to indicate if this is a
  // linking involved in the lowered OpenMP input program. The openmp_lowering
  // flag at SgFile level won't exist when there are no input source files at
  // the linking stage
  //
  // Simply using OpenMP_Only is not sufficient for OpenMP since OpenMP input
  // files can be handled in three different ways in ROSE: 1) parsing only 2)
  // generating dedicated AST 3) actual OpenMP lowering Only the one with
  // lowering will need special linking support to connect to libxomp.a and
  // pthreads.
  Project.setDataPrototype(
      "bool", "openmp_linking", "= false", NO_CONSTRUCTOR_PARAMETER,
      BUILD_FLAG_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  Project.setDataPrototype(
      "bool", "addCppDirectivesToAST", "= false", NO_CONSTRUCTOR_PARAMETER,
      BUILD_FLAG_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  // negara1 (06/23/2011): This field tracks for each included file its
  // including preprocessing infos.
  Project.setDataPrototype(
      "std::map<std::string, std::set<PreprocessingInfo*> >",
      "includingPreprocessingInfosMap", "", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  // negara1 (08/17/2011): Added to permit specifying an optional root folder
  // for header files unparsing.
  Project.setDataPrototype("std::string", "unparseHeaderFilesRootFolder",
                           "= \"\"", NO_CONSTRUCTOR_PARAMETER,
                           BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  Project.setDataPrototype(
      "SgProject::constant_folding_enum", "frontendConstantFolding",
      "= SgProject::e_original_expressions_only", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  // The project owns its cross-file symbol scope. A project copy must own an
  // independent scope; sharing it lets destruction of either project invalidate
  // the other's symbol lookup.
  Project.setDataPrototype("SgGlobal*", "globalScopeAcrossFiles", "= nullptr",
                           NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                           NO_TRAVERSAL, DEF_DELETE, CLONE_TREE);

  Project.setDataPrototype("bool", "unparse_in_same_directory_as_input_file",
                           "= false", NO_CONSTRUCTOR_PARAMETER,
                           BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  Project.setDataPrototype("bool",
                           "stop_after_compilation_do_not_assemble_file",
                           "= false", NO_CONSTRUCTOR_PARAMETER,
                           BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  Project.setDataPrototype("std::string", "gnuOptionForUndefinedSymbol",
                           "= \"\"", NO_CONSTRUCTOR_PARAMETER,
                           BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  Project.setDataPrototype("bool", "mode_32_bit", "= false",
                           NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                           NO_TRAVERSAL, NO_DELETE);

  // DQ (3/19/2014): This option -rose:noclobber causes the output of source
  // code to an existing file to be an error.
  Project.setDataPrototype("bool", "noclobber_output_file", "= false",
                           NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                           NO_TRAVERSAL, NO_DELETE);

  // DQ (3/19/2014): This option -rose:noclobber causes the output of source
  // code to an existing file to be an error if it results in a different file.
  // Output of an identifal file is not an error.
  Project.setDataPrototype("bool", "noclobber_if_different_output_file",
                           "= false", NO_CONSTRUCTOR_PARAMETER,
                           BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  // DQ (8/14/2025): This is a performance optimization, name qualification is
  // about 40% of the performance cost (time) and need only be done on the part
  // of the translation unit that will be unparsed (the source file, and any
  // transformations (modified or added code) of the source file).
  Project.setDataPrototype(
      "bool", "suppressNameQualificationAcrossWholeTranslationUnit", "= false",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);

  // Pei-Hung (8/6/2014): This option -rose:appendPID appends PID into the
  // temporary output name to avoid issues in parallel compilation.
  Project.setDataPrototype("bool", "appendPID", "= false",
                           NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                           NO_TRAVERSAL, NO_DELETE);

  Project.setDataPrototype("bool", "reportOnHeaderFileUnparsing", "= false",
                           NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                           NO_TRAVERSAL, NO_DELETE);

  Project.setDataPrototype("std::string", "applicationRootDirectory", "= \"\"",
                           NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                           NO_TRAVERSAL, NO_DELETE);
  Project.setDataPrototype("bool", "usingApplicationRootDirectory", "= false",
                           NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                           NO_TRAVERSAL, NO_DELETE);

  Project.setDataPrototype("std::string", "astfile_out", "",
                           NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                           NO_TRAVERSAL, NO_DELETE);
  Project.setDataPrototype("std::list<std::string>", "astfiles_in", "",
                           NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                           NO_TRAVERSAL, NO_DELETE);

  // Place generated-header paths before user paths and tool-specific paths
  // after.
  Project.setDataPrototype(
      "SgStringList", "extraIncludeDirectorySpecifierBeforeList", "",
      NO_CONSTRUCTOR_PARAMETER, BUILD_LIST_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);
  Project.setDataPrototype(
      "SgStringList", "extraIncludeDirectorySpecifierAfterList", "",
      NO_CONSTRUCTOR_PARAMETER, BUILD_LIST_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);

  Project.setDataPrototype("bool", "skip_post_processing", "= false",
                           NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                           NO_TRAVERSAL, NO_DELETE);

  Attribute.setDataPrototype("std::string", "name", "= \"\"",
                             CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                             NO_TRAVERSAL, NO_DELETE);
  BitAttribute.setDataPrototype("unsigned long int", "bitflag", "= 0",
                                CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                                NO_TRAVERSAL, NO_DELETE);

  ModifierNodes.setDataPrototype(
      "SgModifierTypePtrVector", "nodes", "", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  ModifierNodes.setDataPrototype(
      "SgModifierNodes*", "next", "= nullptr", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, DEF_DELETE, CLONE_TREE);
  ModifierNodes.setAutomaticGenerationOfConstructor(false);
  ModifierNodes.setDestructorPrologue(R"(
    SgNode *owner = get_parent();
    if (SgNode::isLiveNode(owner)) {
      if (auto *type = isSgType(owner)) {
        if (type->get_modifiers() == this)
          type->set_modifiers(nullptr);
      } else if (auto *previous = isSgModifierNodes(owner)) {
        if (previous->get_next() == this)
          previous->set_next(nullptr);
      }
    }
)");

  ConstVolatileModifier.setDataPrototype(
      "SgConstVolatileModifier::cv_modifier_enum", "modifier",
      "= SgConstVolatileModifier::e_unknown", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  StorageModifier.setDataPrototype(
      "SgStorageModifier::storage_modifier_enum", "modifier",
      "= SgStorageModifier::e_unknown", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  StorageModifier.setDataPrototype(
      "bool", "thread_local_storage", "= false", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  AccessModifier.setDataPrototype(
      "SgAccessModifier::access_modifier_enum", "modifier",
      "= SgAccessModifier::e_unknown", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  AccessModifier.setDataPrototype(
      "bool", "is_explicit", "= false", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  // Note that more than one value can be set, so this implements a bit vector
  // of flags to be used.
  FunctionModifier.setDataPrototype(
      "SgBitVector", "modifierVector", "", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  FunctionModifier.setDataPrototype(
      "unsigned long int", "gnu_attribute_constructor_destructor_priority",
      "= 0", NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);
  FunctionModifier.setDataPrototype("std::string", "gnu_attribute_named_alias",
                                    "=\"\"", NO_CONSTRUCTOR_PARAMETER,
                                    BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
                                    NO_DELETE);
  FunctionModifier.setDataPrototype(
      "SgFunctionModifier::gnu_attribute_parameter_index_list_list_t",
      "gnu_attribute_nonnull_parameter_index_lists", "",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);
  FunctionModifier.setDataPrototype(
      "SgType *", "opencl_vec_type", "= nullptr", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  FunctionModifier.setDataPrototype(
      "SgFunctionModifier::opencl_work_group_size_t", "opencl_work_group_size",
      "", NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);

  SpecialFunctionModifier.setDataPrototype(
      "SgBitVector", "modifierVector", "", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  DeclarationModifier.setDataPrototype(
      "SgBitVector", "modifierVector", "", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  DeclarationModifier.setDataPrototype(
      "SgTypeModifier", "typeModifier", ".reset()", NO_CONSTRUCTOR_PARAMETER,
      NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  DeclarationModifier.setDataPrototype(
      "SgAccessModifier", "accessModifier", ".reset()",
      NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  DeclarationModifier.setDataPrototype(
      "SgStorageModifier", "storageModifier", ".reset()",
      NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  DeclarationModifier.setDataPrototype(
      "SgDeclarationModifier::gnu_declaration_visibility_enum",
      "gnu_attribute_visibility",
      "= SgDeclarationModifier::e_unspecified_visibility",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);
  DeclarationModifier.setDataPrototype(
      "SgDeclarationModifier::gnu_declaration_visibility_enum",
      "gnu_type_visibility",
      "= SgDeclarationModifier::e_unspecified_visibility",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);

  TypeModifier.setDataPrototype(
      "SgBitVector", "modifierVector", "", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  TypeModifier.setDataPrototype(
      "SgConstVolatileModifier", "constVolatileModifier", ".reset()",
      NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  TypeModifier.setDataPrototype(
      "SgElaboratedTypeModifier", "elaboratedTypeModifier", ".reset()",
      NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  // Zero is a valid alignment; -1 means no alignment was specified.
  TypeModifier.setDataPrototype(
      "int", "gnu_attribute_alignment", "= -1", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  TypeModifier.setDataPrototype(
      "long", "gnu_attribute_sentinel", "= -1", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  TypeModifier.setDataPrototype(
      "unsigned", "address_space_value", "= 0", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  TypeModifier.setDataPrototype(
      "unsigned", "vector_size", "= 1", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  ElaboratedTypeModifier.setDataPrototype(
      "SgElaboratedTypeModifier::elaborated_type_modifier_enum", "modifier",
      "= SgElaboratedTypeModifier::e_unknown", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  LinkageModifier.setDataPrototype(
      "SgLinkageModifier::linkage_modifier_enum", "modifier",
      "= SgLinkageModifier::e_unknown", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  BaseClassModifier.setDataPrototype(
      "SgBaseClassModifier::baseclass_modifier_enum", "modifier",
      "= SgBaseClassModifier::e_unknown", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  BaseClassModifier.setDataPrototype(
      "SgAccessModifier", "accessModifier", "", NO_CONSTRUCTOR_PARAMETER,
      NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  File_Info.setDataPrototype("int", "file_id", "= NULL_FILE_ID",
                             NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                             NO_TRAVERSAL, NO_DELETE);
  File_Info.setDataPrototype("int", "line", "= 0", NO_CONSTRUCTOR_PARAMETER,
                             NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  File_Info.setDataPrototype("int", "col", "= 0", NO_CONSTRUCTOR_PARAMETER,
                             NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  // We need a way to classify statements as being from a transformation, or a
  // if from a transformation either representing the transformation,
  // declarations required by the transformation, or intializations associated
  // with a transformation. Indidual bits within the integer are used to
  // classify File_Info objects. Bits 0-15 are presently reserved.
  File_Info.setDataPrototype("unsigned int", "classificationBitField", "= 0",
                             NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                             NO_TRAVERSAL, NO_DELETE);

  // DQ (12/18/2012): Adding physical source position (existing file_id and line
  // data is for logical file position). Note that the column number is the same
  // for both logical and physical positions.
  File_Info.setDataPrototype("int", "physical_file_id", "= NULL_FILE_ID",
                             NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                             NO_TRAVERSAL, NO_DELETE);
  File_Info.setDataPrototype("int", "physical_line", "= 0",
                             NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                             NO_TRAVERSAL, NO_DELETE);

  File_Info.setDataPrototype("unsigned int", "source_sequence_number", "= 0",
                             NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                             NO_TRAVERSAL, NO_DELETE);
  // The physical file identifies one output artifact. Clang's FileID
  // additionally distinguishes each textual inclusion of that artifact.
  File_Info.setDataPrototype("unsigned int", "physical_file_occurrence_id",
                             "= 0", NO_CONSTRUCTOR_PARAMETER,
                             NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  // Shared nodes are unparsed only in the listed files. The list is otherwise
  // empty; list accessors must update the stored list rather than a copy.
  File_Info.setDataPrototype(
      "SgFileIdList", "fileIDsToUnparse", "", NO_CONSTRUCTOR_PARAMETER,
      BUILD_LIST_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  File_Info.setDataPrototype("SgFileLineNumberList", "fileLineNumbersToUnparse",
                             "", NO_CONSTRUCTOR_PARAMETER,
                             BUILD_LIST_ACCESS_FUNCTIONS, NO_TRAVERSAL,
                             NO_DELETE);

  File_Info.setDataPrototype("static std::map<int, std::string>",
                             "fileidtoname_map", "", NO_CONSTRUCTOR_PARAMETER,
                             NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  File_Info.setDataPrototype("static std::map<std::string, int>",
                             "nametofileid_map", "", NO_CONSTRUCTOR_PARAMETER,
                             NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  Name.setAutomaticGenerationOfConstructor(false);

  SymbolTable.setPredeclarationString("HEADER_SYMBOL_TABLE_PREDECLARATION",
                                      "../Grammar/Support.code");
  SymbolTable.setAutomaticGenerationOfConstructor(false);

  FuncDecl_attr.setAutomaticGenerationOfConstructor(false);
  ClassDecl_attr.setAutomaticGenerationOfConstructor(false);

  Options.setDataPrototype("std::ostream*", "default_output", "= nullptr",
                           NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                           NO_TRAVERSAL, DEF_DELETE);
  Options.setDataPrototype("std::ostream*", "debug_output", "= nullptr",
                           NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                           NO_TRAVERSAL, DEF_DELETE);
  Options.setDataPrototype("std::ostream*", "error_output", "= nullptr",
                           NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                           NO_TRAVERSAL, DEF_DELETE);
  Options.setDataPrototype("std::ostream*", "logging_output", "= nullptr",
                           NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                           NO_TRAVERSAL, DEF_DELETE);
  Options.setDataPrototype("int", "debug_level", "= 0",
                           NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                           NO_TRAVERSAL, NO_DELETE);
  Options.setDataPrototype("int", "logging_level", "= 0",
                           NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                           NO_TRAVERSAL, NO_DELETE);

  TemplateParameter.setFunctionPrototype("HEADER_TEMPLATE_PARAMETER",
                                         "../Grammar/Support.code");
  TemplateParameter.setDataPrototype(
      "SgTemplateParameter::template_parameter_enum", "parameterType",
      "= parameter_undefined", CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
      NO_TRAVERSAL, NO_DELETE);
  TemplateParameter.setDataPrototype(
      "SgType*", "type", "= nullptr", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE, COPY_REFERENCE);
  TemplateParameter.setDataPrototype(
      "SgType*", "defaultTypeParameter", "= nullptr", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE, COPY_REFERENCE);
  TemplateParameter.setDataPrototype(
      "SgExpression*", "typeConstraint", "= nullptr", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);
  // The semantic immediately-declared constraint prepends the constrained
  // parameter itself to the concept arguments.  That expression is not the
  // source grammar surface.  Preserve the independently typed concept-id that
  // was written before the parameter so unparsing never has to reverse Clang's
  // semantic normalization.
  TemplateParameter.setDataPrototype(
      "SgExpression*", "sourceTypeConstraint", "= nullptr",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL,
      NO_DELETE, COPY_DATA, OPTIONAL_TRAVERSAL_MEMBER);
  TemplateParameter.setDataPrototype(
      "SgExpression*", "defaultExpressionParameter", "= nullptr",
      CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE,
      COPY_DATA, OPTIONAL_TRAVERSAL_MEMBER, DIRECT_TRAVERSAL_ACCESS,
      COPY_AFTER_OWNER_CONSTRUCTION);
  TemplateParameter.setDataPrototype(
      "SgDeclarationStatement*", "templateDeclaration", "= nullptr",
      CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE,
      COPY_REFERENCE);
  // Redeclaration syntax can spell a different nested template header;
  // preserve it independently from the semantic template identity.
  TemplateParameter.setDataPrototype(
      "SgTemplateDeclaration*", "sourceSpelledTemplateDeclaration", "= nullptr",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL,
      NO_DELETE, COPY_DATA, OPTIONAL_TRAVERSAL_MEMBER);
  TemplateParameter.setDataPrototype(
      "SgTemplateArgument*", "defaultTemplateArgument", "= nullptr",
      CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE,
      COPY_DATA, OPTIONAL_TRAVERSAL_MEMBER, DIRECT_TRAVERSAL_ACCESS,
      COPY_AFTER_OWNER_CONSTRUCTION);

  TemplateParameter.setDataPrototype(
      "SgInitializedName*", "initializedName", "= nullptr",
      CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE,
      COPY_DATA, OPTIONAL_TRAVERSAL_MEMBER, DIRECT_TRAVERSAL_ACCESS,
      COPY_AFTER_OWNER_CONSTRUCTION);
  TemplateParameter.setDataPrototype("SgTemplateParameter*",
                                     "canonicalNonTypeParameter", "= nullptr",
                                     CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                                     NO_TRAVERSAL, NO_DELETE, COPY_REFERENCE);
  TemplateParameter.setDataPrototype(
      "std::optional<canonical_source_identity>", "canonical_source_identity",
      "= std::nullopt", NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
      NO_TRAVERSAL, NO_DELETE, COPY_DATA);
  TemplateParameter.setDataPrototype(
      "SgTemplateParameter::template_parameter_keyword_enum",
      "templateParameterKeyword", "= keyword_unspecified",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE,
      COPY_DATA);
  TemplateParameter.setDataPrototype(
      "bool", "isAbbreviatedFunctionTemplateParameter", "= false",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE,
      COPY_DATA);
  TemplateParameter.setDataPrototype(
      "bool", "is_parameter_pack", "= false", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE, COPY_DATA);
  TemplateArgument.setFunctionPrototype("HEADER_TEMPLATE_ARGUMENT",
                                        "../Grammar/Support.code");
  TemplateArgument.setDataPrototype(
      "SgTemplateArgument::template_argument_enum", "argumentType",
      "= argument_undefined", CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
      NO_TRAVERSAL, NO_DELETE);
  TemplateArgument.setDataPrototype(
      "bool", "isArrayBoundUnknownType", "= false", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  TemplateArgument.setDataPrototype(
      "SgType*", "type", "= nullptr", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE, COPY_REFERENCE);

  // The canonical semantic type remains in p_type.  When Clang provides a
  // TemplateArgumentLoc, this separate edge records the exact source-spelled
  // type surface (including typedef/using aliases and written qualification)
  // that the unparser is required to emit.
  TemplateArgument.setDataPrototype(
      "SgType*", "sourceSpelledType", "= nullptr", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, DELETE_IF_OWNED, CLONE_TREE);
  // Preserve the qualifier written on the exact type argument use.  Presence
  // distinguishes an explicitly unqualified source spelling from missing
  // producer data; sourceSpelledType continues to own dependent qualifier
  // chains that are represented structurally as SgNonrealType.
  TemplateArgument.setDataPrototype("bool", "source_type_qualification_present",
                                    "= false", NO_CONSTRUCTOR_PARAMETER,
                                    BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
                                    NO_DELETE);
  TemplateArgument.setDataPrototype("bool", "source_type_global_qualification",
                                    "= false", NO_CONSTRUCTOR_PARAMETER,
                                    BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
                                    NO_DELETE);
  TemplateArgument.setDataPrototype(
      "SgStringList", "source_type_qualification_tokens", "",
      NO_CONSTRUCTOR_PARAMETER, BUILD_LIST_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);

  TemplateArgument.setDataPrototype(
      "SgExpression*", "expression", "= nullptr", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);

  // This semantic identity edge refers to a declaration owned by its lexical
  // scope, not syntax owned by the argument.
  // A copy transaction remaps the edge only when that declaration is copied
  // independently through its real structural owner.
  TemplateArgument.setDataPrototype(
      "SgDeclarationStatement*", "templateDeclaration", "= nullptr",
      CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE,
      COPY_REFERENCE);

  TemplateArgument.setDataPrototype(
      "bool", "explicitlySpecified", "= true", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  // Qualification accessors must not mark the AST as modified.
  TemplateArgument.setDataPrototype(
      "int", "name_qualification_length", "= 0", NO_CONSTRUCTOR_PARAMETER,
      NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  TemplateArgument.setDataPrototype(
      "bool", "type_elaboration_required", "= false", NO_CONSTRUCTOR_PARAMETER,
      NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  TemplateArgument.setDataPrototype(
      "bool", "global_qualification_required", "= false",
      NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  TemplateArgument.setDataPrototype(
      "bool", "requiresGlobalNameQualificationOnType", "= false",
      NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  TemplateArgument.setDataPrototype(
      "int", "name_qualification_length_for_type", "= 0",
      NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  TemplateArgument.setDataPrototype(
      "bool", "type_elaboration_required_for_type", "= false",
      NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  TemplateArgument.setDataPrototype(
      "bool", "global_qualification_required_for_type", "= false",
      NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  TemplateArgument.setDataPrototype(
      "bool", "is_pack_element", "= false", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  TemplateParameterList.setFunctionPrototype("HEADER_TEMPLATE_PARAMETER_LIST",
                                             "../Grammar/Support.code");
  TemplateParameterList.setDataPrototype(
      "SgTemplateParameterPtrList", "args", "", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE);
  TemplateParameterList.setDataPrototype(
      "SgTemplateParameterList::source_header_separator_enum",
      "source_header_separator",
      "= SgTemplateParameterList::e_source_header_separator_unset",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);

  NameGroup.setFunctionPrototype("HEADER_NAME_GROUP",
                                 "../Grammar/Support.code");
  NameGroup.setDataPrototype("std::string", "group_name", "",
                             CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                             NO_TRAVERSAL, NO_DELETE);
  NameGroup.setDataPrototype("SgVarRefExpPtrList", "objects", "",
                             CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                             DEF_TRAVERSAL, NO_DELETE, CLONE_TREE);

  DimensionObject.setDataPrototype(
      "SgInitializedName*", "array", "= nullptr", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  DimensionObject.setDataPrototype(
      "SgExprListExp*", "shape", "= nullptr", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  DataStatementGroup.setFunctionPrototype("HEADER_DATA_STATEMENT_GROUP",
                                          "../Grammar/Support.code");
  DataStatementGroup.setDataPrototype(
      "SgDataStatementObjectPtrList", "object_list", "", CONSTRUCTOR_PARAMETER,
      NO_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, CLONE_TREE);
  DataStatementGroup.setDataPrototype(
      "SgDataStatementValuePtrList", "value_list", "", CONSTRUCTOR_PARAMETER,
      NO_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, CLONE_TREE);

  DataStatementObject.setFunctionPrototype("HEADER_DATA_STATEMENT_OBJECT",
                                           "../Grammar/Support.code");
  DataStatementObject.setFunctionSource("SOURCE_DATA_STATEMENT_OBJECT",
                                        "../Grammar/Support.code");
  DataStatementObject.setDataPrototype(
      "SgExprListExp*", "variableReference_list", "", CONSTRUCTOR_PARAMETER,
      NO_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, CLONE_TREE,
      REQUIRED_TRAVERSAL_MEMBER);

  DataStatementValue.setFunctionPrototype("HEADER_DATA_STATEMENT_VALUE",
                                          "../Grammar/Support.code");
  DataStatementValue.setDataPrototype(
      "SgExpression*", "constant_expression", "", CONSTRUCTOR_PARAMETER,
      NO_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, CLONE_TREE,
      REQUIRED_TRAVERSAL_MEMBER);
  DataStatementValue.setDataPrototype(
      "SgExpression*", "repeat_expression", "= nullptr", CONSTRUCTOR_PARAMETER,
      NO_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, CLONE_TREE,
      OPTIONAL_TRAVERSAL_MEMBER);

  FormatItem.setFunctionPrototype("HEADER_FORMAT_ITEM",
                                  "../Grammar/Support.code");
  FormatItem.setFunctionSource("SOURCE_FORMAT_ITEM", "../Grammar/Support.code");
  FormatItem.setDataPrototype("SgFormatItem::Descriptor", "descriptor", "",
                              CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                              NO_TRAVERSAL, NO_DELETE);
  FormatItem.setDataPrototype("std::optional<std::uint64_t>", "repeat_count",
                              "", CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                              NO_TRAVERSAL, NO_DELETE);
  FormatItem.setDataPrototype("SgFormatItemList*", "format_item_list", "",
                              CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                              DEF_TRAVERSAL, NO_DELETE, CLONE_TREE,
                              OPTIONAL_TRAVERSAL_MEMBER);
  FormatItemList.setFunctionPrototype("HEADER_FORMAT_ITEM_LIST",
                                      "../Grammar/Support.code");
  FormatItemList.setDataPrototype("SgFormatItemPtrList", "format_item_list", "",
                                  CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                                  DEF_TRAVERSAL, NO_DELETE, CLONE_TREE);

  OpenclAccessModeModifier.setFunctionPrototype(
      "HEADER_OPENCL_ACCESS_MODE_MODIFIER", "../Grammar/Support.code");

  OpenclAccessModeModifier.setDataPrototype(
      "SgOpenclAccessModeModifier::access_mode_modifier_enum", "modifier",
      "= SgOpenclAccessModeModifier::e_unknown", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  // ***********************************************************************
  SymbolTable.setFunctionSource("SOURCE_SYMBOL_TABLE",
                                "../Grammar/Support.code");

  TypeTable.setFunctionSource("SOURCE_TYPE_TABLE", "../Grammar/Support.code");

  Name.setFunctionSource("SOURCE_NAME", "../Grammar/Support.code");
  Pragma.setFunctionSource("SOURCE_PRAGMA", "../Grammar/Support.code");

  Modifier.setFunctionSource("SOURCE_MODIFIER", "../Grammar/Support.code");
  ModifierNodes.setFunctionSource("SOURCE_MODIFIER_NODES",
                                  "../Grammar/Support.code");
  ConstVolatileModifier.setFunctionSource("SOURCE_CV_TYPE_MODIFIER",
                                          "../Grammar/Support.code");
  StorageModifier.setFunctionSource("SOURCE_STORAGE_MODIFIER",
                                    "../Grammar/Support.code");
  AccessModifier.setFunctionSource("SOURCE_ACCESS_MODIFIER",
                                   "../Grammar/Support.code");
  FunctionModifier.setFunctionSource("SOURCE_FUNCTION_MODIFIER",
                                     "../Grammar/Support.code");
  SpecialFunctionModifier.setFunctionSource("SOURCE_SPECIAL_FUNCTION_MODIFIER",
                                            "../Grammar/Support.code");
  DeclarationModifier.setFunctionSource("SOURCE_DECLARATION_MODIFIER",
                                        "../Grammar/Support.code");
  TypeModifier.setFunctionSource("SOURCE_TYPE_MODIFIER",
                                 "../Grammar/Support.code");
  ElaboratedTypeModifier.setFunctionSource("SOURCE_ELABORATED_TYPE_MODIFIER",
                                           "../Grammar/Support.code");
  LinkageModifier.setFunctionSource("SOURCE_LINKAGE_MODIFIER",
                                    "../Grammar/Support.code");
  BaseClassModifier.setFunctionSource("SOURCE_BASECLASS_MODIFIER",
                                      "../Grammar/Support.code");

  File_Info.setFunctionSource("SOURCE_FILE_INFORMATION",
                              "../Grammar/Support.code");

  Directory.setFunctionSource("SOURCE_APPLICATION_DIRECTORY",
                              "../Grammar/Support.code");
  File.setFunctionSource("SOURCE_APPLICATION_FILE", "../Grammar/Support.code");
  SourceFile.setFunctionSource("SOURCE_APPLICATION_SOURCE_FILE",
                               "../Grammar/Support.code");
  Project.setFunctionSource("SOURCE_APPLICATION_PROJECT",
                            "../Grammar/Support.code");
  Unparse_Info.setFunctionSource("SOURCE_UNPARSE_INFO",
                                 "../Grammar/Support.code");

  BaseClass.setFunctionSource("SOURCE_BASECLASS", "../Grammar/Support.code");
  ExpBaseClass.setFunctionSource("SOURCE_EXP_BASE_CLASS",
                                 "../Grammar/Support.code");
  NonrealBaseClass.setFunctionSource("SOURCE_NONREAL_BASE_CLASS",
                                     "../Grammar/Support.code");

  FuncDecl_attr.setFunctionSource("SOURCE_FUNCTION_DECLARATION_ATTRIBUTE",
                                  "../Grammar/Support.code");
  ClassDecl_attr.setFunctionSource("SOURCE_CLASS_DECLARATION_ATTRIBUTE",
                                   "../Grammar/Support.code");

  FunctionParameterTypeList.setFunctionPrototype(
      "HEADER_FUNCTION_PARAMETER_TYPE_LIST", "../Grammar/Support.code");
  FunctionParameterTypeList.setFunctionSource(
      "SOURCE_FUNCTION_PARAMETER_TYPE_LIST", "../Grammar/Support.code");
  FunctionParameterTypeList.setAutomaticGenerationOfConstructor(false);
  FunctionTypeArgument.setFunctionPrototype("HEADER_FUNCTION_TYPE_ARGUMENT",
                                            "../Grammar/Support.code");
  FunctionTypeArgument.setFunctionSource("SOURCE_FUNCTION_TYPE_ARGUMENT",
                                         "../Grammar/Support.code");
  FunctionTypeArgument.setDataPrototype(
      "SgType*", "type", "= NULL", CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
      NO_TRAVERSAL, NO_DELETE, CLONE_TREE);
  FunctionTypeArgument.setMemberDestructor("type", R"(
    if (SgNode::isLiveNode(p_type) && p_type->get_parent() == this)
      p_type->set_parent(nullptr);
)");
  FunctionTypeArgument.setDataPrototype(
      "bool", "is_pack_expansion", "= false", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  FunctionTypeArgument.setDataPrototype(
      "bool", "source_type_qualification_present", "= false",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);
  FunctionTypeArgument.setDataPrototype(
      "bool", "source_type_global_qualification", "= false",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);
  FunctionTypeArgument.setDataPrototype(
      "SgStringList", "source_type_qualification_tokens", "",
      NO_CONSTRUCTOR_PARAMETER, BUILD_LIST_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);
  FunctionTypeArgument.setDataPrototype(
      "bool", "source_type_elaboration_required", "= false",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);
  // One OpenMP iterator-specifier.  The named fields encode the five grammar
  // roles directly; clauses own lists of these nodes structurally instead of
  // hiding expressions in nested raw containers.
  OmpIteratorDefinition.setFunctionPrototype("HEADER_OMP_ITERATOR_DEFINITION",
                                             "../Grammar/Support.code");
  OmpIteratorDefinition.setFunctionSource("SOURCE_OMP_ITERATOR_DEFINITION",
                                          "../Grammar/Support.code");
  OmpIteratorDefinition.setDataPrototype(
      "SgTypeExpression*", "iterator_type", "", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, CLONE_PTR,
      OPTIONAL_TRAVERSAL_MEMBER);
  OmpIteratorDefinition.setDataPrototype(
      "SgOmpNameExpression*", "iterator_name", "", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, CLONE_PTR,
      OPTIONAL_TRAVERSAL_MEMBER);
  OmpIteratorDefinition.setDataPrototype(
      "SgExpression*", "begin", "", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, CLONE_PTR,
      OPTIONAL_TRAVERSAL_MEMBER);
  OmpIteratorDefinition.setDataPrototype(
      "SgExpression*", "end", "", CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
      DEF_TRAVERSAL, NO_DELETE, CLONE_PTR, OPTIONAL_TRAVERSAL_MEMBER);
  OmpIteratorDefinition.setDataPrototype(
      "SgExpression*", "step", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, CLONE_PTR,
      OPTIONAL_TRAVERSAL_MEMBER);
  FunctionParameterTypeList.setDataPrototype(
      "SgFunctionParameterScope*", "source_parameter_scope", "= nullptr",
      NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, DEF_TRAVERSAL, DEF_DELETE,
      CLONE_TREE, OPTIONAL_TRAVERSAL_MEMBER);
  FunctionParameterTypeList.setDataPrototype(
      "SgFunctionParameterList*", "source_parameters", "= nullptr",
      NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, DEF_TRAVERSAL, DEF_DELETE,
      CLONE_TREE, OPTIONAL_TRAVERSAL_MEMBER);
  FunctionParameterTypeList.setDataPrototype(
      "SgTypePtrList", "arguments", "", NO_CONSTRUCTOR_PARAMETER,
      NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE, CLONE_TREE);
  FunctionParameterTypeList.setDataPrototype(
      "SgFunctionTypeArgumentPtrList", "argument_qualification_use_sites", "",
      NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, DEF_TRAVERSAL, DEF_DELETE);

  TemplateParameter.setFunctionSource("SOURCE_TEMPLATE_PARAMETER",
                                      "../Grammar/Support.code");
  TemplateArgument.setFunctionSource("SOURCE_TEMPLATE_ARGUMENT",
                                     "../Grammar/Support.code");

  Graph.setFunctionSource("SOURCE_GRAPH", "../Grammar/Support.code");
  GraphNode.setFunctionSource("SOURCE_GRAPHNODE", "../Grammar/Support.code");
  GraphEdge.setFunctionSource("SOURCE_GRAPHEDGE", "../Grammar/Support.code");

  NameGroup.setFunctionSource("SOURCE_NAME_GROUP", "../Grammar/Support.code");

  DataStatementGroup.setFunctionSource("SOURCE_DATA_STATEMENT_GROUP",
                                       "../Grammar/Support.code");
  DataStatementValue.setFunctionSource("SOURCE_DATA_STATEMENT_VALUE",
                                       "../Grammar/Support.code");

  FormatItemList.setFunctionSource("SOURCE_FORMAT_ITEM_LIST",
                                   "../Grammar/Support.code");

  OpenclAccessModeModifier.setFunctionSource(
      "SOURCE_OPENCL_ACCESS_MODE_MODIFIER", "../Grammar/Support.code");
}
