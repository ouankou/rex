#include "sageAstJsonPrivate.h"

namespace Rose {
namespace AstJson {

SgNode *createNodeFromRecord(const NodeRecord &record, SgProject *project,
                             const JsonValue &metadata) {
  const JsonValue &p = record.properties;
  const std::string &kind = record.kind;
  if (kind == "SgSourceFile") {
    std::vector<std::string> args = commandLineFromMetadata(metadata);
    SgSourceFile *file = new SgSourceFile(args, project);
    file->set_sourceFileNameWithPath(p.stringOr(
        "source_filename_with_path", metadata.stringOr("source_file")));
    file->set_sourceFileNameWithoutPath(p.stringOr(
        "source_filename_without_path", file->get_sourceFileNameWithoutPath()));
    file->set_unparse_output_filename(p.stringOr(
        "unparse_output_filename", metadata.stringOr("output_file")));
    file->set_C_only(p.boolOr("C_only", file->get_C_only()));
    file->set_Cxx_only(p.boolOr("Cxx_only", file->get_Cxx_only()));
    file->set_Fortran_only(p.boolOr("Fortran_only", file->get_Fortran_only()));
    file->set_CoArrayFortran_only(
        p.boolOr("CoArrayFortran_only", file->get_CoArrayFortran_only()));
    file->set_Cuda_only(p.boolOr("Cuda_only", file->get_Cuda_only()));
    file->set_OpenCL_only(p.boolOr("OpenCL_only", file->get_OpenCL_only()));
    file->set_requires_C_preprocessor(p.boolOr(
        "requires_C_preprocessor", file->get_requires_C_preprocessor()));
    file->set_inputFormat(
        fileOutputFormatFromJson(p, "input_format", file->get_inputFormat()));
    file->set_outputFormat(
        fileOutputFormatFromJson(p, "output_format", file->get_outputFormat()));
    file->set_backendCompileFormat(fileOutputFormatFromJson(
        p, "backend_compile_format", file->get_backendCompileFormat()));
    file->set_fortran_implicit_none(
        p.boolOr("fortran_implicit_none", file->get_fortran_implicit_none()));
    file->set_inputLanguage(
        fileLanguageFromJson(p, "input_language", file->get_inputLanguage()));
    file->set_outputLanguage(
        fileLanguageFromJson(p, "output_language", file->get_outputLanguage()));
    file->set_strict_language_handling(p.boolOr(
        "strict_language_handling", file->get_strict_language_handling()));
    file->set_sourceFileUsesCppFileExtension(
        p.boolOr("source_uses_cpp_extension",
                 file->get_sourceFileUsesCppFileExtension()));
    file->set_sourceFileUsesFortranFileExtension(
        p.boolOr("source_uses_fortran_extension",
                 file->get_sourceFileUsesFortranFileExtension()));
    file->set_sourceFileUsesFortran77FileExtension(
        p.boolOr("source_uses_fortran77_extension",
                 file->get_sourceFileUsesFortran77FileExtension()));
    file->set_sourceFileUsesFortran90FileExtension(
        p.boolOr("source_uses_fortran90_extension",
                 file->get_sourceFileUsesFortran90FileExtension()));
    file->set_sourceFileUsesFortran95FileExtension(
        p.boolOr("source_uses_fortran95_extension",
                 file->get_sourceFileUsesFortran95FileExtension()));
    file->set_sourceFileUsesFortran2003FileExtension(
        p.boolOr("source_uses_fortran2003_extension",
                 file->get_sourceFileUsesFortran2003FileExtension()));
    file->set_sourceFileUsesFortran2008FileExtension(
        p.boolOr("source_uses_fortran2008_extension",
                 file->get_sourceFileUsesFortran2008FileExtension()));
    file->set_sourceFileUsesCoArrayFortranFileExtension(
        p.boolOr("source_uses_coarray_fortran_extension",
                 file->get_sourceFileUsesCoArrayFortranFileExtension()));
    file->set_sourceFileTypeIsUnknown(p.boolOr(
        "source_file_type_is_unknown", file->get_sourceFileTypeIsUnknown()));
    file->set_experimental_flang_frontend(
        p.boolOr("experimental_flang_frontend",
                 file->get_experimental_flang_frontend()));
    file->set_openmp(metadata.boolOr("openmp", false));
    file->set_openmp_parse_only(metadata.boolOr("openmp_parse_only", false));
    file->set_openmp_ast_only(metadata.boolOr("openmp_ast_only", false));
    file->set_openmp_analyzing(metadata.boolOr("openmp_analyzing", false));
    file->set_openmp_lowering(metadata.boolOr("openmp_lowering", false));
    file->set_openmp_processed(metadata.boolOr("openmp_processed", false));
    file->set_openacc(metadata.boolOr("openacc", false));
    file->set_skipfinalCompileStep(
        metadata.boolOr("skipfinalCompileStep", false));
    file->set_suppress_variable_declaration_normalization(
        metadata.boolOr("suppress_variable_declaration_normalization", false));
    file->set_unparse_tokens(metadata.boolOr("unparse_tokens", false));
    return file;
  }
  if (kind == "SgGlobal") {
    return new SgGlobal();
  }
  if (kind == "SgBasicBlock") {
    return new SgBasicBlock(static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgInitializedName") {
    return new SgInitializedName(nullptr, SgName(p.stringOr("name")),
                                 SageBuilder::buildUnknownType(), nullptr,
                                 nullptr, nullptr, nullptr);
  }
  if (kind == "SgVariableDeclaration") {
    return new SgVariableDeclaration();
  }
  if (kind == "SgVariableDefinition") {
    return new SgVariableDefinition(static_cast<Sg_File_Info *>(nullptr),
                                    static_cast<SgInitializedName *>(nullptr),
                                    static_cast<SgExpression *>(nullptr));
  }
  if (kind == "SgTemplateVariableDeclaration") {
    return new SgTemplateVariableDeclaration();
  }
  if (kind == "SgProgramHeaderStatement") {
    SgFunctionType *function_type = nullptr;
    if (const JsonValue *type = p.find("function_type")) {
      function_type = isSgFunctionType(earlyTypeFromJson(*type));
    }
    return new SgProgramHeaderStatement(SgName(p.stringOr("name")),
                                        function_type, nullptr);
  }
  if (kind == "SgProcedureHeaderStatement") {
    SgFunctionType *function_type = nullptr;
    if (const JsonValue *type = p.find("function_type")) {
      function_type = isSgFunctionType(earlyTypeFromJson(*type));
    }
    SgProcedureHeaderStatement *decl = new SgProcedureHeaderStatement(
        SgName(p.stringOr("name")), function_type, nullptr);
    decl->set_subprogram_kind(
        static_cast<SgProcedureHeaderStatement::subprogram_kind_enum>(
            p.intOr("subprogram_kind",
                    SgProcedureHeaderStatement::e_function_subprogram_kind)));
    return decl;
  }
  if (kind == "SgContainsStatement") {
    return new SgContainsStatement();
  }
  if (kind == "SgFortranContinueStmt") {
    return new SgFortranContinueStmt();
  }
  if (kind == "SgProcessControlStatement") {
    SgProcessControlStatement *stmt =
        new SgProcessControlStatement(static_cast<SgExpression *>(nullptr));
    stmt->set_control_kind(static_cast<SgProcessControlStatement::control_enum>(
        p.intOr("control_kind", SgProcessControlStatement::e_unknown)));
    return stmt;
  }
  if (kind == "SgCommonBlock") {
    return new SgCommonBlock(static_cast<Sg_File_Info *>(nullptr));
  }
  if (kind == "SgCommonBlockObject") {
    SgCommonBlockObject *object =
        new SgCommonBlockObject(static_cast<Sg_File_Info *>(nullptr));
    object->set_block_name(p.stringOr("block_name"));
    return object;
  }
  if (kind == "SgFortranIncludeLine") {
    return new SgFortranIncludeLine(p.stringOr("filename"));
  }
  if (kind == "SgFunctionDeclaration") {
    return new SgFunctionDeclaration(SgName(p.stringOr("name")), nullptr,
                                     nullptr);
  }
  if (kind == "SgTemplateInstantiationFunctionDecl") {
    SgTemplateArgumentPtrList arguments;
    return new SgTemplateInstantiationFunctionDecl(
        SgName(p.stringOr("name")), nullptr, nullptr, nullptr, arguments);
  }
  if (kind == "SgTemplateFunctionDeclaration") {
    return new SgTemplateFunctionDeclaration(SgName(p.stringOr("name")),
                                             nullptr, nullptr);
  }
  if (kind == "SgFunctionParameterList") {
    return new SgFunctionParameterList();
  }
  if (kind == "SgFunctionDefinition") {
    return new SgFunctionDefinition(static_cast<SgBasicBlock *>(nullptr));
  }
  if (kind == "SgTemplateFunctionDefinition") {
    return new SgTemplateFunctionDefinition(
        static_cast<SgBasicBlock *>(nullptr));
  }
  if (kind == "SgTypedefDeclaration") {
    return new SgTypedefDeclaration(SgName(p.stringOr("name")),
                                    SageBuilder::buildIntType(), nullptr,
                                    nullptr, nullptr);
  }
  if (kind == "SgTemplateTypedefDeclaration") {
    SgType *base_type = SageBuilder::buildUnknownType();
    if (const JsonValue *type = p.find("base_type")) {
      base_type = earlyTypeFromJson(*type);
    }
    return new SgTemplateTypedefDeclaration(
        SgName(p.stringOr("name")), base_type, nullptr, nullptr, nullptr);
  }
  if (kind == "SgTemplateInstantiationTypedefDeclaration") {
    SgTemplateArgumentPtrList arguments;
    SgType *base_type = SageBuilder::buildUnknownType();
    if (const JsonValue *type = p.find("base_type")) {
      base_type = earlyTypeFromJson(*type);
    }
    return new SgTemplateInstantiationTypedefDeclaration(
        SgName(p.stringOr("name")), base_type, nullptr, nullptr, nullptr,
        nullptr, arguments);
  }
  if (kind == "SgModuleStatement") {
    return new SgModuleStatement(
        SgName(p.stringOr("name")),
        static_cast<SgClassDeclaration::class_types>(
            p.intOr("class_type", SgClassDeclaration::e_class)),
        nullptr, nullptr);
  }
  if (kind == "SgDerivedTypeStatement") {
    return new SgDerivedTypeStatement(
        SgName(p.stringOr("name")),
        static_cast<SgClassDeclaration::class_types>(
            p.intOr("class_type", SgClassDeclaration::e_struct)),
        nullptr, nullptr);
  }
  if (kind == "SgClassDeclaration") {
    return new SgClassDeclaration(
        SgName(p.stringOr("name")),
        static_cast<SgClassDeclaration::class_types>(
            p.intOr("class_type", SgClassDeclaration::e_struct)),
        nullptr, nullptr);
  }
  if (kind == "SgClassDefinition") {
    return new SgClassDefinition();
  }
  if (kind == "SgTemplateInstantiationDefn") {
    throw std::runtime_error(
        "AST JSON SgTemplateInstantiationDefn requires delayed construction");
  }
  if (kind == "SgBaseClass") {
    return new SgBaseClass(nullptr, p.boolOr("is_direct_base_class", false));
  }
  if (kind == "SgExpBaseClass") {
    return new SgExpBaseClass(nullptr, p.boolOr("is_direct_base_class", false),
                              nullptr);
  }
  if (kind == "SgNonrealBaseClass") {
    return new SgNonrealBaseClass(
        nullptr, p.boolOr("is_direct_base_class", false), nullptr);
  }
  if (kind == "SgDeclarationScope") {
    return new SgDeclarationScope();
  }
  if (kind == "SgFunctionParameterScope") {
    return new SgFunctionParameterScope();
  }
  if (kind == "SgTemplateClassDefinition") {
    return new SgTemplateClassDefinition();
  }
  if (kind == "SgNamespaceDeclarationStatement") {
    return new SgNamespaceDeclarationStatement(
        SgName(p.stringOr("name")), nullptr,
        p.boolOr("is_unnamed_namespace", false));
  }
  if (kind == "SgNamespaceDefinitionStatement") {
    SgNamespaceDefinitionStatement *def = new SgNamespaceDefinitionStatement(
        static_cast<SgNamespaceDeclarationStatement *>(nullptr));
    def->set_isUnionOfReentrantNamespaceDefinitions(
        p.boolOr("is_union_of_reentrant_namespace_definitions", false));
    return def;
  }
  if (kind == "SgEnumDeclaration") {
    return new SgEnumDeclaration(SgName(p.stringOr("name")), nullptr);
  }
  if (kind == "SgCtorInitializerList") {
    return new SgCtorInitializerList();
  }
  if (kind == "SgEmptyDeclaration") {
    return new SgEmptyDeclaration();
  }
  if (kind == "SgUsingDirectiveStatement") {
    return new SgUsingDirectiveStatement(
        static_cast<SgNamespaceDeclarationStatement *>(nullptr));
  }
  if (kind == "SgUsingDeclarationStatement") {
    return new SgUsingDeclarationStatement(
        static_cast<SgDeclarationStatement *>(nullptr),
        static_cast<SgInitializedName *>(nullptr));
  }
  if (kind == "SgUseStatement") {
    return new SgUseStatement(SgName(p.stringOr("name")),
                              p.boolOr("only_option", false),
                              p.stringOr("module_nature"));
  }
  if (kind == "SgRenamePair") {
    return new SgRenamePair(SgName(p.stringOr("local_name")),
                            SgName(p.stringOr("use_name")));
  }
  if (kind == "SgToken") {
    return new SgToken(
        p.stringOr("lexeme_string"),
        static_cast<unsigned int>(p.intOr("classification_code", 0)));
  }
  if (kind == "SgImplicitStatement") {
    SgImplicitStatement *stmt =
        new SgImplicitStatement(p.boolOr("implicit_none", false));
    stmt->set_implicit_spec(
        static_cast<SgImplicitStatement::implicit_spec_enum>(
            p.intOr("implicit_spec", stmt->get_implicit_spec())));
    return stmt;
  }
  if (kind == "SgMemberFunctionDeclaration") {
    return new SgMemberFunctionDeclaration(SgName(p.stringOr("name")), nullptr,
                                           nullptr);
  }
  if (kind == "SgTemplateInstantiationMemberFunctionDecl") {
    SgTemplateArgumentPtrList arguments;
    return new SgTemplateInstantiationMemberFunctionDecl(
        SgName(p.stringOr("name")), nullptr, nullptr, nullptr, arguments);
  }
  if (kind == "SgTemplateMemberFunctionDeclaration") {
    return new SgTemplateMemberFunctionDeclaration(SgName(p.stringOr("name")),
                                                   nullptr, nullptr);
  }
  if (kind == "SgTemplateClassDeclaration") {
    SgTemplateClassDeclaration *decl = new SgTemplateClassDeclaration(
        SgName(p.stringOr("name")),
        static_cast<SgClassDeclaration::class_types>(
            p.intOr("class_type", SgClassDeclaration::e_class)),
        nullptr, nullptr);
    std::string template_name = p.stringOr("template_name");
    if (template_name.empty()) {
      template_name = p.stringOr("name");
    }
    decl->set_templateName(SgName(template_name));
    return decl;
  }
  if (kind == "SgTemplateInstantiationDecl") {
    SgTemplateArgumentPtrList arguments;
    SgTemplateInstantiationDecl *decl = new SgTemplateInstantiationDecl(
        SgName(p.stringOr("name")),
        static_cast<SgClassDeclaration::class_types>(
            p.intOr("class_type", SgClassDeclaration::e_class)),
        nullptr, nullptr, nullptr, arguments);
    std::string template_name = p.stringOr("template_name");
    if (template_name.empty()) {
      template_name = p.stringOr("name");
      const size_t args = template_name.find('<');
      if (args != std::string::npos) {
        template_name = trim(template_name.substr(0, args));
      }
    }
    decl->set_templateName(SgName(template_name));
    decl->set_nameResetFromMangledForm(p.boolOr(
        "name_reset_from_mangled_form", decl->get_nameResetFromMangledForm()));
    return decl;
  }
  if (kind == "SgTemplateInstantiationDirectiveStatement") {
    return new SgTemplateInstantiationDirectiveStatement(
        static_cast<SgDeclarationStatement *>(nullptr));
  }
  if (kind == "SgNonrealDecl") {
    const std::string name = p.stringOr("name", p.stringOr("unparse"));
    return new SgNonrealDecl(SgName(name));
  }
  if (kind == "SgTypeExpression") {
    return new SgTypeExpression(earlyTypeFromProperties(p));
  }
  if (kind == "SgAsteriskShapeExp") {
    return new SgAsteriskShapeExp();
  }
  if (kind == "SgColonShapeExp") {
    return new SgColonShapeExp();
  }
  if (kind == "SgExprStatement") {
    return new SgExprStatement(static_cast<SgExpression *>(nullptr));
  }
  if (kind == "SgReturnStmt") {
    return new SgReturnStmt(static_cast<SgExpression *>(nullptr));
  }
  if (kind == "SgBreakStmt") {
    return new SgBreakStmt();
  }
  if (kind == "SgContinueStmt") {
    return new SgContinueStmt();
  }
  if (kind == "SgGotoStatement") {
    return new SgGotoStatement(static_cast<SgLabelStatement *>(nullptr));
  }
  if (kind == "SgForStatement") {
    return new SgForStatement(static_cast<SgStatement *>(nullptr),
                              static_cast<SgExpression *>(nullptr),
                              static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgFortranDo") {
    return new SgFortranDo(static_cast<SgExpression *>(nullptr),
                           static_cast<SgExpression *>(nullptr),
                           static_cast<SgExpression *>(nullptr),
                           static_cast<SgBasicBlock *>(nullptr));
  }
  if (kind == "SgFortranNonblockedDo") {
    return new SgFortranNonblockedDo(static_cast<SgExpression *>(nullptr),
                                     static_cast<SgExpression *>(nullptr),
                                     static_cast<SgExpression *>(nullptr),
                                     static_cast<SgBasicBlock *>(nullptr),
                                     static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgImpliedDo") {
    return new SgImpliedDo(static_cast<Sg_File_Info *>(nullptr),
                           static_cast<SgExpression *>(nullptr),
                           static_cast<SgExpression *>(nullptr),
                           static_cast<SgExpression *>(nullptr),
                           static_cast<SgExprListExp *>(nullptr),
                           static_cast<SgScopeStatement *>(nullptr));
  }
  if (kind == "SgAllocateStatement") {
    return new SgAllocateStatement(static_cast<Sg_File_Info *>(nullptr));
  }
  if (kind == "SgNullifyStatement") {
    return new SgNullifyStatement(static_cast<Sg_File_Info *>(nullptr));
  }
  if (kind == "SgDeallocateStatement") {
    return new SgDeallocateStatement(static_cast<Sg_File_Info *>(nullptr));
  }
  if (kind == "SgForInitStatement") {
    return new SgForInitStatement();
  }
  if (kind == "SgIfStmt") {
    return new SgIfStmt(static_cast<SgStatement *>(nullptr),
                        static_cast<SgStatement *>(nullptr),
                        static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgWhileStmt") {
    return new SgWhileStmt(static_cast<SgStatement *>(nullptr),
                           static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgDoWhileStmt") {
    return new SgDoWhileStmt(static_cast<SgStatement *>(nullptr),
                             static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgSwitchStatement") {
    return new SgSwitchStatement(static_cast<SgStatement *>(nullptr),
                                 static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgCaseOptionStmt") {
    return new SgCaseOptionStmt(static_cast<SgExpression *>(nullptr),
                                static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgDefaultOptionStmt") {
    return new SgDefaultOptionStmt(static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgNullStatement") {
    return new SgNullStatement();
  }
  if (kind == "SgLabelStatement") {
    return new SgLabelStatement(SgName(p.stringOr("label")),
                                static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgPragma") {
    return new SgPragma(p.stringOr("name"), nullptr, nullptr);
  }
  if (kind == "SgPragmaDeclaration") {
    return new SgPragmaDeclaration(static_cast<SgPragma *>(nullptr));
  }
  if (kind == "SgAttributeSpecificationStatement") {
    return new SgAttributeSpecificationStatement();
  }
  if (kind == "SgInterfaceStatement") {
    return new SgInterfaceStatement(
        SgName(p.stringOr("name")),
        static_cast<SgInterfaceStatement::generic_spec_enum>(p.intOr(
            "generic_spec", SgInterfaceStatement::e_default_interface_type)));
  }
  if (kind == "SgInterfaceBody") {
    return new SgInterfaceBody(SgName(p.stringOr("function_name")),
                               static_cast<SgFunctionDeclaration *>(nullptr),
                               p.boolOr("use_function_name", false));
  }
  if (kind == "SgPrintStatement") {
    return new SgPrintStatement();
  }
  if (kind == "SgReadStatement") {
    return new SgReadStatement();
  }
  if (kind == "SgWriteStatement") {
    return new SgWriteStatement();
  }
  if (kind == "SgOpenStatement") {
    return new SgOpenStatement();
  }
  if (kind == "SgCloseStatement") {
    return new SgCloseStatement();
  }
  if (kind == "SgFlushStatement") {
    return new SgFlushStatement();
  }
  if (kind == "SgBackspaceStatement") {
    return new SgBackspaceStatement();
  }
  if (kind == "SgRewindStatement") {
    return new SgRewindStatement();
  }
  if (kind == "SgEndfileStatement") {
    return new SgEndfileStatement();
  }
  if (kind == "SgWaitStatement") {
    return new SgWaitStatement();
  }
  if (kind == "SgStaticAssertionDeclaration") {
    return new SgStaticAssertionDeclaration(
        static_cast<SgExpression *>(nullptr),
        SgName(p.stringOr("string_literal")));
  }
  if (kind == "SgVarRefExp") {
    return new SgVarRefExp(static_cast<SgVariableSymbol *>(nullptr));
  }
  if (kind == "SgLabelRefExp") {
    return new SgLabelRefExp(static_cast<SgLabelSymbol *>(nullptr));
  }
  if (kind == "SgActualArgumentExpression") {
    SgExpression *placeholder = new SgNullExpression();
    SgActualArgumentExpression *actual = new SgActualArgumentExpression(
        SgName(p.stringOr("argument_name")), placeholder);
    placeholder->set_parent(actual);
    return actual;
  }
  if (kind == "SgFunctionRefExp") {
    return new SgFunctionRefExp(static_cast<SgFunctionSymbol *>(nullptr),
                                static_cast<SgFunctionType *>(nullptr));
  }
  if (kind == "SgTemplateFunctionRefExp") {
    return new SgTemplateFunctionRefExp(
        static_cast<SgTemplateFunctionSymbol *>(nullptr));
  }
  if (kind == "SgMemberFunctionRefExp") {
    return new SgMemberFunctionRefExp(
        static_cast<SgMemberFunctionSymbol *>(nullptr),
        static_cast<int>(p.intOr("virtual_call", 0)),
        isSgFunctionType(earlyTypeFromProperties(p)),
        static_cast<int>(p.intOr("need_qualifier", true)));
  }
  if (kind == "SgExprListExp") {
    return new SgExprListExp();
  }
  if (kind == "SgFunctionCallExp") {
    return new SgFunctionCallExp(static_cast<SgExpression *>(nullptr),
                                 static_cast<SgExprListExp *>(nullptr),
                                 SageBuilder::buildUnknownType());
  }
  if (kind == "SgAssignInitializer") {
    return new SgAssignInitializer(static_cast<SgExpression *>(nullptr),
                                   SageBuilder::buildUnknownType());
  }
  if (kind == "SgAggregateInitializer") {
    return new SgAggregateInitializer(static_cast<SgExprListExp *>(nullptr),
                                      earlyTypeFromProperties(p));
  }
  if (kind == "SgBracedInitializer") {
    return new SgBracedInitializer(static_cast<SgExprListExp *>(nullptr),
                                   earlyTypeFromProperties(p));
  }
  if (kind == "SgBoolValExp") {
    return new SgBoolValExp(static_cast<int>(p.intOr("value", 0)));
  }
  if (kind == "SgShortVal") {
    return new SgShortVal(static_cast<short>(p.intOr("value", 0)),
                          p.stringOr("value_string"));
  }
  if (kind == "SgUnsignedShortVal") {
    return new SgUnsignedShortVal(
        static_cast<unsigned short>(p.intOr("value", 0)),
        p.stringOr("value_string"));
  }
  if (kind == "SgIntVal") {
    return new SgIntVal(static_cast<int>(p.intOr("value", 0)),
                        p.stringOr("value_string"));
  }
  if (kind == "SgUnsignedIntVal") {
    return new SgUnsignedIntVal(static_cast<unsigned int>(p.intOr("value", 0)),
                                p.stringOr("value_string"));
  }
  if (kind == "SgLongIntVal") {
    return new SgLongIntVal(static_cast<long>(p.intOr("value", 0)),
                            p.stringOr("value_string"));
  }
  if (kind == "SgUnsignedLongVal") {
    return new SgUnsignedLongVal(
        static_cast<unsigned long>(p.intOr("value", 0)),
        p.stringOr("value_string"));
  }
  if (kind == "SgLongLongIntVal") {
    return new SgLongLongIntVal(p.intOr("value", 0),
                                p.stringOr("value_string"));
  }
  if (kind == "SgUnsignedLongLongIntVal") {
    return new SgUnsignedLongLongIntVal(
        static_cast<unsigned long long>(p.intOr("value", 0)),
        p.stringOr("value_string"));
  }
  if (kind == "SgCharVal") {
    return new SgCharVal(static_cast<char>(p.intOr("value", 0)),
                         p.stringOr("value_string"));
  }
  if (kind == "SgUnsignedCharVal") {
    return new SgUnsignedCharVal(
        static_cast<unsigned char>(p.intOr("value", 0)),
        p.stringOr("value_string"));
  }
  if (kind == "SgFloatVal") {
    const std::string value = p.stringOr("value", "0");
    return new SgFloatVal(std::stof(value), value);
  }
  if (kind == "SgDoubleVal") {
    const std::string value = p.stringOr("value", "0");
    return new SgDoubleVal(std::stod(value), value);
  }
  if (kind == "SgComplexVal") {
    SgType *precision_type = SageBuilder::buildUnknownType();
    if (const JsonValue *type = p.find("precision_type")) {
      precision_type = earlyTypeFromJson(*type);
    }
    return new SgComplexVal(static_cast<Sg_File_Info *>(nullptr),
                            static_cast<SgValueExp *>(nullptr),
                            static_cast<SgValueExp *>(nullptr), precision_type,
                            p.stringOr("value_string"));
  }
  if (kind == "SgStringVal") {
    SgStringVal *value = new SgStringVal(p.stringOr("value"));
    value->set_wcharString(p.boolOr("wchar_string", false));
    value->set_stringDelimiter(
        static_cast<char>(p.intOr("string_delimiter", 0)));
    value->set_is16bitString(p.boolOr("is_16bit_string", false));
    value->set_is32bitString(p.boolOr("is_32bit_string", false));
    value->set_isRawString(p.boolOr("is_raw_string", false));
    value->set_raw_string_value(p.stringOr("raw_string_value"));
    return value;
  }
  if (kind == "SgEnumVal") {
    return new SgEnumVal(p.intOr("value", 0),
                         static_cast<SgEnumDeclaration *>(nullptr),
                         SgName(p.stringOr("name")));
  }
  if (kind == "SgTemplateParameterVal") {
    SgTemplateParameterVal *value = new SgTemplateParameterVal(
        static_cast<int>(p.intOr("template_parameter_position", -1)),
        p.stringOr("value_string"));
    value->set_valueType(SageBuilder::buildUnknownType());
    return value;
  }
  if (kind == "SgThisExp") {
    return new SgThisExp(static_cast<SgClassSymbol *>(nullptr),
                         static_cast<SgNonrealSymbol *>(nullptr),
                         static_cast<int>(p.intOr("pobj_this", 0)));
  }
  if (kind == "SgNonrealRefExp") {
    return new SgNonrealRefExp(static_cast<SgNonrealSymbol *>(nullptr));
  }
  if (kind == "SgNullExpression") {
    return new SgNullExpression();
  }
  if (kind == "SgNullptrValExp") {
    return new SgNullptrValExp();
  }
  if (kind == "SgCastExp") {
    return new SgCastExp(static_cast<SgExpression *>(nullptr),
                         earlyTypeFromProperties(p),
                         static_cast<SgCastExp::cast_type_enum>(
                             p.intOr("cast_type", SgCastExp::e_C_style_cast)));
  }
  if (kind == "SgConstructorInitializer") {
    // The final expression type can reference declarations by serialized node
    // id. Allocate a valid placeholder now and rebuild it after all ids exist.
    return new SgConstructorInitializer(
        static_cast<SgMemberFunctionDeclaration *>(nullptr),
        static_cast<SgExprListExp *>(nullptr), SageBuilder::buildUnknownType(),
        p.boolOr("need_name", false), p.boolOr("need_qualifier", false),
        p.boolOr("need_parenthesis_after_name", false), true);
  }
  if (kind == "SgSizeOfOp") {
    return new SgSizeOfOp(static_cast<SgExpression *>(nullptr), nullptr,
                          earlyTypeFromProperties(p));
  }
  if (kind == "SgConditionalExp") {
    return new SgConditionalExp(static_cast<SgExpression *>(nullptr),
                                static_cast<SgExpression *>(nullptr),
                                static_cast<SgExpression *>(nullptr),
                                earlyTypeFromProperties(p));
  }
  if (kind == "SgStatementExpression") {
    return new SgStatementExpression(static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgNewExp") {
    SgNewExp *expr = new SgNewExp(
        SageBuilder::buildUnknownType(), static_cast<SgExprListExp *>(nullptr),
        static_cast<SgConstructorInitializer *>(nullptr),
        static_cast<SgExpression *>(nullptr),
        static_cast<short>(p.intOr("need_global_specifier", 0)),
        static_cast<SgFunctionDeclaration *>(nullptr));
    expr->set_type_id_is_parenthesized(
        p.boolOr("type_id_is_parenthesized", false));
    return expr;
  }
  if (kind == "SgDeleteExp") {
    return new SgDeleteExp(
        static_cast<SgExpression *>(nullptr),
        static_cast<short>(p.intOr("is_array", 0)),
        static_cast<short>(p.intOr("need_global_specifier", 0)),
        static_cast<SgFunctionDeclaration *>(nullptr));
  }
  if (kind == "SgPackExpansionExpr") {
    return new SgPackExpansionExpr(static_cast<SgExpression *>(nullptr));
  }
  if (kind == "SgTypeTraitBuiltinOperator") {
    return new SgTypeTraitBuiltinOperator(SgName(p.stringOr("name")));
  }

  if (SgBinaryOp *binary =
          buildBinaryOpForKind(kind, SageBuilder::buildUnknownType())) {
    return binary;
  }

  if (SgUnaryOp *unary =
          buildUnaryOpForKind(kind, SageBuilder::buildUnknownType(), p)) {
    return unary;
  }
  if (kind == "SgSubscriptExpression") {
    return new SgSubscriptExpression(static_cast<SgExpression *>(nullptr),
                                     static_cast<SgExpression *>(nullptr),
                                     static_cast<SgExpression *>(nullptr));
  }

  if (kind == "SgTemplateArgument") {
    return new SgTemplateArgument(
        static_cast<SgTemplateArgument::template_argument_enum>(
            p.intOr("argument_type", SgTemplateArgument::argument_undefined)),
        p.boolOr("is_array_bound_unknown_type", false), nullptr, nullptr,
        nullptr, p.boolOr("explicitly_specified", true));
  }
  if (kind == "SgTemplateParameter") {
    return new SgTemplateParameter(
        static_cast<SgTemplateParameter::template_parameter_enum>(p.intOr(
            "parameter_type", SgTemplateParameter::parameter_undefined)),
        nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
  }

  if (kind == "SgOmpMapClause") {
    return new SgOmpMapClause(
        static_cast<SgExprListExp *>(nullptr),
        static_cast<SgOmpClause::omp_map_operator_enum>(
            p.intOr("operation", SgOmpClause::e_omp_map_unknown)));
  }
  if (kind == "SgOmpDependClause") {
    return new SgOmpDependClause(
        static_cast<SgExprListExp *>(nullptr),
        static_cast<SgOmpClause::omp_depend_modifier_enum>(p.intOr(
            "depend_modifier", SgOmpClause::e_omp_depend_modifier_unspecified)),
        static_cast<SgOmpClause::omp_dependence_type_enum>(
            p.intOr("dependence_type", SgOmpClause::e_omp_depend_unspecified)));
  }
  if (kind == "SgOmpAffinityClause") {
    return new SgOmpAffinityClause(
        static_cast<SgExprListExp *>(nullptr),
        static_cast<SgOmpClause::omp_affinity_modifier_enum>(
            p.intOr("affinity_modifier",
                    SgOmpClause::e_omp_affinity_modifier_unspecified)));
  }
  if (kind == "SgOmpToClause") {
    return new SgOmpToClause(static_cast<SgExprListExp *>(nullptr),
                             static_cast<SgOmpClause::omp_to_kind_enum>(p.intOr(
                                 "kind", SgOmpClause::e_omp_to_kind_unknown)));
  }
  if (kind == "SgOmpFromClause") {
    return new SgOmpFromClause(
        static_cast<SgExprListExp *>(nullptr),
        static_cast<SgOmpClause::omp_from_kind_enum>(
            p.intOr("kind", SgOmpClause::e_omp_from_kind_unknown)));
  }
  if (kind == "SgOmpDefaultClause") {
    return new SgOmpDefaultClause(
        static_cast<SgOmpClause::omp_default_option_enum>(
            p.intOr("data_sharing", SgOmpClause::e_omp_default_unknown)),
        static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpProcBindClause") {
    return new SgOmpProcBindClause(
        static_cast<SgOmpClause::omp_proc_bind_policy_enum>(
            p.intOr("policy", SgOmpClause::e_omp_proc_bind_policy_unknown)));
  }
  if (kind == "SgOmpNowaitClause") {
    return new SgOmpNowaitClause(static_cast<SgExpression *>(nullptr));
  }
  if (kind == "SgOmpOrderedClause") {
    return new SgOmpOrderedClause(static_cast<SgExpression *>(nullptr));
  }
  if (kind == "SgOmpCollapseClause") {
    return new SgOmpCollapseClause(static_cast<SgExpression *>(nullptr));
  }
  if (kind == "SgOmpIfClause") {
    return new SgOmpIfClause(
        static_cast<SgExpression *>(nullptr),
        static_cast<SgOmpClause::omp_if_modifier_enum>(
            p.intOr("modifier", SgOmpClause::e_omp_if_modifier_unknown)));
  }
  if (kind == "SgOmpNumThreadsClause") {
    return new SgOmpNumThreadsClause(static_cast<SgExpression *>(nullptr));
  }
  if (kind == "SgOmpNumTeamsClause") {
    return new SgOmpNumTeamsClause(static_cast<SgExpression *>(nullptr));
  }
  if (kind == "SgOmpSafelenClause") {
    return new SgOmpSafelenClause(static_cast<SgExpression *>(nullptr));
  }
  if (kind == "SgOmpSimdlenClause") {
    return new SgOmpSimdlenClause(static_cast<SgExpression *>(nullptr));
  }
  if (kind == "SgOmpPrivateClause") {
    return new SgOmpPrivateClause(static_cast<SgExprListExp *>(nullptr));
  }
  if (kind == "SgOmpFirstprivateClause") {
    return new SgOmpFirstprivateClause(static_cast<SgExprListExp *>(nullptr));
  }
  if (kind == "SgOmpCopyinClause") {
    return new SgOmpCopyinClause(static_cast<SgExprListExp *>(nullptr));
  }
  if (kind == "SgOmpLastprivateClause") {
    return new SgOmpLastprivateClause(
        static_cast<SgExprListExp *>(nullptr),
        static_cast<SgOmpClause::omp_lastprivate_modifier_enum>(p.intOr(
            "modifier", SgOmpClause::e_omp_lastprivate_modifier_unspecified)));
  }
  if (kind == "SgOmpReductionClause") {
    return new SgOmpReductionClause(
        static_cast<SgExprListExp *>(nullptr),
        static_cast<SgOmpClause::omp_reduction_modifier_enum>(
            p.intOr("modifier", SgOmpClause::e_omp_reduction_modifier_unknown)),
        static_cast<SgOmpClause::omp_reduction_identifier_enum>(
            p.intOr("identifier", SgOmpClause::e_omp_reduction_unknown)),
        static_cast<SgExpression *>(nullptr));
  }
  if (kind == "SgOmpLinearClause") {
    return new SgOmpLinearClause(
        static_cast<SgExprListExp *>(nullptr),
        static_cast<SgExpression *>(nullptr),
        static_cast<SgOmpClause::omp_linear_modifier_enum>(p.intOr(
            "modifier", SgOmpClause::e_omp_linear_modifier_unspecified)));
  }
  if (kind == "SgOmpAlignedClause") {
    return new SgOmpAlignedClause(static_cast<SgExprListExp *>(nullptr),
                                  static_cast<SgExpression *>(nullptr));
  }
  if (kind == "SgOmpSharedClause") {
    return new SgOmpSharedClause(static_cast<SgExprListExp *>(nullptr));
  }
  if (kind == "SgOmpScheduleClause") {
    return new SgOmpScheduleClause(
        static_cast<SgOmpClause::omp_schedule_modifier_enum>(p.intOr(
            "modifier", SgOmpClause::e_omp_schedule_modifier_unspecified)),
        static_cast<SgOmpClause::omp_schedule_modifier_enum>(p.intOr(
            "modifier1", SgOmpClause::e_omp_schedule_modifier_unspecified)),
        static_cast<SgOmpClause::omp_schedule_kind_enum>(
            p.intOr("kind", SgOmpClause::e_omp_schedule_kind_unspecified)),
        static_cast<SgExpression *>(nullptr));
  }
  if (kind == "SgOmpDistScheduleClause") {
    return new SgOmpDistScheduleClause(
        static_cast<SgOmpClause::omp_dist_schedule_kind_enum>(
            p.intOr("kind", SgOmpClause::e_omp_dist_schedule_kind_unspecified)),
        static_cast<SgExpression *>(nullptr));
  }
  if (kind == "SgOmpOrderClause") {
    return new SgOmpOrderClause(
        static_cast<SgOmpClause::omp_order_kind_enum>(
            p.intOr("kind", SgOmpClause::e_omp_order_kind_unspecified)),
        static_cast<SgOmpClause::omp_order_modifier_enum>(p.intOr(
            "modifier", SgOmpClause::e_omp_order_modifier_unspecified)));
  }
  if (kind == "SgOmpAtomicDefaultMemOrderClause") {
    return new SgOmpAtomicDefaultMemOrderClause(
        static_cast<SgOmpClause::omp_atomic_default_mem_order_kind_enum>(
            p.intOr(
                "kind",
                SgOmpClause::e_omp_atomic_default_mem_order_kind_unspecified)));
  }
  if (kind == "SgOmpDefaultmapClause") {
    return new SgOmpDefaultmapClause(
        static_cast<SgOmpClause::omp_defaultmap_behavior_enum>(p.intOr(
            "behavior", SgOmpClause::e_omp_defaultmap_behavior_unspecified)),
        static_cast<SgOmpClause::omp_defaultmap_category_enum>(p.intOr(
            "category", SgOmpClause::e_omp_defaultmap_category_unspecified)));
  }
  if (kind == "SgOmpBindClause") {
    return new SgOmpBindClause(static_cast<SgOmpClause::omp_bind_binding_enum>(
        p.intOr("binding", SgOmpClause::e_omp_bind_binding_unspecified)));
  }
  if (kind == "SgOmpParallelClause") {
    return new SgOmpParallelClause();
  }
  if (kind == "SgOmpSectionsClause") {
    return new SgOmpSectionsClause();
  }
  if (kind == "SgOmpForClause") {
    return new SgOmpForClause();
  }
  if (kind == "SgOmpTaskgroupClause") {
    return new SgOmpTaskgroupClause();
  }
  if (kind == "SgOmpFullClause") {
    return new SgOmpFullClause();
  }
  if (kind == "SgOmpInbranchClause") {
    return new SgOmpInbranchClause();
  }
  if (kind == "SgOmpNocontextClause") {
    return new SgOmpNocontextClause(static_cast<SgExpression *>(nullptr));
  }
  if (kind == "SgOmpNovariantsClause") {
    return new SgOmpNovariantsClause(static_cast<SgExpression *>(nullptr));
  }
  if (kind == "SgOmpPartialClause") {
    return new SgOmpPartialClause(static_cast<SgExpression *>(nullptr));
  }
  if (kind == "SgOmpBeginClause") {
    return new SgOmpBeginClause();
  }
  if (kind == "SgOmpEndClause") {
    return new SgOmpEndClause();
  }
  if (kind == "SgOmpReadClause") {
    return new SgOmpReadClause();
  }
  if (kind == "SgOmpWriteClause") {
    return new SgOmpWriteClause();
  }
  if (kind == "SgOmpUpdateClause") {
    return new SgOmpUpdateClause();
  }
  if (kind == "SgOmpCaptureClause") {
    return new SgOmpCaptureClause();
  }
  if (kind == "SgOmpCompareClause") {
    return new SgOmpCompareClause();
  }
  if (kind == "SgOmpSeqCstClause") {
    return new SgOmpSeqCstClause();
  }
  if (kind == "SgOmpAcqRelClause") {
    return new SgOmpAcqRelClause();
  }
  if (kind == "SgOmpReleaseClause") {
    return new SgOmpReleaseClause();
  }
  if (kind == "SgOmpAcquireClause") {
    return new SgOmpAcquireClause();
  }
  if (kind == "SgOmpRelaxedClause") {
    return new SgOmpRelaxedClause();
  }
  if (kind == "SgOmpFailClause") {
    return new SgOmpFailClause(
        static_cast<SgOmpClause::omp_fail_memory_order_kind_enum>(
            p.intOr("memory_order",
                    SgOmpClause::e_omp_fail_memory_order_kind_unspecified)));
  }
  if (kind == "SgOmpCopyprivateClause") {
    return new SgOmpCopyprivateClause(static_cast<SgExprListExp *>(nullptr));
  }
  if (kind == "SgOmpAllocateClause") {
    return new SgOmpAllocateClause(
        static_cast<SgExprListExp *>(nullptr),
        static_cast<SgOmpClause::omp_allocate_modifier_enum>(
            p.intOr("modifier", SgOmpClause::e_omp_allocate_modifier_unknown)),
        static_cast<SgExpression *>(nullptr));
  }
  if (kind == "SgOmpAllocatorClause") {
    return new SgOmpAllocatorClause(
        static_cast<SgOmpClause::omp_allocator_modifier_enum>(
            p.intOr("modifier", SgOmpClause::e_omp_allocator_modifier_unknown)),
        static_cast<SgExpression *>(nullptr));
  }
  if (kind == "SgOmpAdjustArgsClause") {
    return new SgOmpAdjustArgsClause(
        static_cast<SgExprListExp *>(nullptr),
        static_cast<SgOmpClause::omp_adjust_args_modifier_enum>(p.intOr(
            "modifier", SgOmpClause::e_omp_adjust_args_modifier_unknown)));
  }
  if (kind == "SgOmpInReductionClause") {
    return new SgOmpInReductionClause(
        static_cast<SgExprListExp *>(nullptr),
        static_cast<SgOmpClause::omp_in_reduction_identifier_enum>(
            p.intOr("identifier",
                    SgOmpClause::e_omp_in_reduction_identifier_unspecified)),
        static_cast<SgExpression *>(nullptr));
  }
  if (kind == "SgOmpTaskReductionClause") {
    return new SgOmpTaskReductionClause(
        static_cast<SgExprListExp *>(nullptr),
        static_cast<SgOmpClause::omp_task_reduction_identifier_enum>(
            p.intOr("identifier",
                    SgOmpClause::e_omp_task_reduction_identifier_unspecified)),
        static_cast<SgExpression *>(nullptr));
  }
  if (kind == "SgOmpUniformClause") {
    return new SgOmpUniformClause(static_cast<SgExprListExp *>(nullptr));
  }
  if (kind == "SgOmpHintClause") {
    return new SgOmpHintClause(static_cast<SgExpression *>(nullptr));
  }
  if (kind == "SgOmpNogroupClause") {
    return new SgOmpNogroupClause();
  }
  if (kind == "SgOmpUntiedClause") {
    return new SgOmpUntiedClause();
  }
  if (kind == "SgOmpMergeableClause") {
    return new SgOmpMergeableClause();
  }
  if (kind == "SgOmpReverseOffloadClause") {
    return new SgOmpReverseOffloadClause();
  }
  if (kind == "SgOmpUnifiedAddressClause") {
    return new SgOmpUnifiedAddressClause();
  }
  if (kind == "SgOmpUnifiedSharedMemoryClause") {
    return new SgOmpUnifiedSharedMemoryClause();
  }
  if (kind == "SgOmpDynamicAllocatorsClause") {
    return new SgOmpDynamicAllocatorsClause();
  }
  if (kind == "SgOmpDepobjUpdateClause") {
    return new SgOmpDepobjUpdateClause(
        static_cast<SgOmpClause::omp_depobj_modifier_enum>(
            p.intOr("modifier", SgOmpClause::e_omp_depobj_modifier_unknown)));
  }
  if (kind == "SgOmpDestroyClause") {
    return new SgOmpDestroyClause();
  }
  if (kind == "SgOmpThreadLimitClause") {
    return new SgOmpThreadLimitClause(static_cast<SgExpression *>(nullptr));
  }
  if (kind == "SgOmpUsesAllocatorsClause") {
    return new SgOmpUsesAllocatorsClause();
  }
  if (kind == "SgOmpUsesAllocatorsDefination") {
    SgOmpUsesAllocatorsDefination *definition =
        new SgOmpUsesAllocatorsDefination();
    definition->set_user_defined_allocator(nullptr);
    definition->set_allocator_traits_array(nullptr);
    return definition;
  }
  if (kind == "SgOmpWhenClause") {
    return new SgOmpWhenClause(
        static_cast<SgExpression *>(nullptr),
        static_cast<SgExpression *>(nullptr),
        static_cast<SgExpression *>(nullptr),
        static_cast<SgExpression *>(nullptr),
        static_cast<SgOmpClause::omp_when_context_kind_enum>(p.intOr(
            "device_kind", SgOmpClause::e_omp_when_context_kind_unknown)),
        static_cast<SgOmpClause::omp_when_context_vendor_enum>(
            p.intOr("implementation_vendor",
                    SgOmpClause::e_omp_when_context_vendor_unspecified)),
        static_cast<SgExpression *>(nullptr),
        static_cast<SgExpression *>(nullptr),
        static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpMatchClause") {
    SgOmpMatchClause *clause = new SgOmpMatchClause(
        static_cast<SgExpression *>(nullptr),
        static_cast<SgExpression *>(nullptr),
        static_cast<SgExpression *>(nullptr),
        static_cast<SgExpression *>(nullptr),
        static_cast<SgOmpClause::omp_when_context_kind_enum>(p.intOr(
            "device_kind", SgOmpClause::e_omp_when_context_kind_unknown)),
        static_cast<SgOmpClause::omp_when_context_vendor_enum>(
            p.intOr("implementation_vendor",
                    SgOmpClause::e_omp_when_context_vendor_unspecified)),
        static_cast<SgExpression *>(nullptr),
        static_cast<SgExpression *>(nullptr));
    clause->set_target_device_selector(
        p.boolOr("target_device_selector", false));
    return clause;
  }
  if (kind == "SgOmpNontemporalClause") {
    return new SgOmpNontemporalClause(static_cast<SgExprListExp *>(nullptr));
  }
  if (kind == "SgOmpIsDevicePtrClause") {
    return new SgOmpIsDevicePtrClause(static_cast<SgExprListExp *>(nullptr));
  }
  if (kind == "SgOmpUseDevicePtrClause") {
    return new SgOmpUseDevicePtrClause(static_cast<SgExprListExp *>(nullptr));
  }
  if (kind == "SgOmpUseDeviceAddrClause") {
    return new SgOmpUseDeviceAddrClause(static_cast<SgExprListExp *>(nullptr));
  }
  if (kind == "SgOmpHasDeviceAddrClause") {
    return new SgOmpHasDeviceAddrClause(static_cast<SgExprListExp *>(nullptr));
  }
  if (kind == "SgOmpDetachClause") {
    return new SgOmpDetachClause(static_cast<SgExpression *>(nullptr));
  }
  if (kind == "SgOmpNumTasksClause") {
    return new SgOmpNumTasksClause(
        static_cast<SgExpression *>(nullptr),
        static_cast<SgOmpClause::omp_num_tasks_modifier_enum>(p.intOr(
            "modifier", SgOmpClause::e_omp_num_tasks_modifier_unspecified)));
  }
  if (kind == "SgOmpGrainsizeClause") {
    return new SgOmpGrainsizeClause(
        static_cast<SgExpression *>(nullptr),
        static_cast<SgOmpClause::omp_grainsize_modifier_enum>(p.intOr(
            "modifier", SgOmpClause::e_omp_grainsize_modifier_unspecified)));
  }
  if (kind == "SgOmpSizesClause") {
    return new SgOmpSizesClause(static_cast<SgExpression *>(nullptr));
  }
  if (kind == "SgOmpPriorityClause") {
    return new SgOmpPriorityClause(static_cast<SgExpression *>(nullptr));
  }
  if (kind == "SgOmpFinalClause") {
    return new SgOmpFinalClause(static_cast<SgExpression *>(nullptr));
  }
  if (kind == "SgOmpNotinbranchClause") {
    return new SgOmpNotinbranchClause();
  }
  if (kind == "SgOmpExclusiveClause") {
    return new SgOmpExclusiveClause(static_cast<SgExprListExp *>(nullptr));
  }
  if (kind == "SgOmpInclusiveClause") {
    return new SgOmpInclusiveClause(static_cast<SgExprListExp *>(nullptr));
  }
  if (kind == "SgOmpExtImplementationDefinedRequirementClause") {
    return new SgOmpExtImplementationDefinedRequirementClause(
        static_cast<SgExpression *>(nullptr));
  }
  if (kind == "SgOmpDeviceClause") {
    return new SgOmpDeviceClause(
        static_cast<SgExpression *>(nullptr),
        static_cast<SgOmpClause::omp_device_modifier_enum>(p.intOr(
            "modifier", SgOmpClause::e_omp_device_modifier_unspecified)));
  }
  if (kind == "SgOmpRequiresStatement") {
    return new SgOmpRequiresStatement();
  }
  if (kind == "SgOmpBarrierStatement") {
    return new SgOmpBarrierStatement();
  }
  if (kind == "SgOmpTaskwaitStatement") {
    return new SgOmpTaskwaitStatement();
  }
  if (kind == "SgOmpOrderedDependStatement") {
    return new SgOmpOrderedDependStatement();
  }
  if (kind == "SgOmpScanStatement") {
    return new SgOmpScanStatement(static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpFlushStatement") {
    return new SgOmpFlushStatement();
  }
  if (kind == "SgOmpDeclareSimdStatement") {
    return new SgOmpDeclareSimdStatement();
  }
  if (kind == "SgOmpDeclareMapperStatement") {
    return new SgOmpDeclareMapperStatement();
  }
  if (kind == "SgOmpDeclareTargetStatement") {
    SgOmpDeclareTargetStatement *stmt = new SgOmpDeclareTargetStatement();
    stmt->set_device_type_kind(
        static_cast<SgOmpClause::omp_when_context_kind_enum>(p.intOr(
            "device_type_kind", SgOmpClause::e_omp_when_context_kind_unknown)));
    return stmt;
  }
  if (kind == "SgOmpEndDeclareTargetStatement") {
    return new SgOmpEndDeclareTargetStatement();
  }
  if (kind == "SgOmpDeclareVariantStatement") {
    return new SgOmpDeclareVariantStatement();
  }
  if (kind == "SgOmpBeginDeclareVariantStatement") {
    SgOmpBeginDeclareVariantStatement *stmt =
        new SgOmpBeginDeclareVariantStatement();
    stmt->set_captured_region(p.stringOr("captured_region"));
    return stmt;
  }
  if (kind == "SgOmpEndDeclareVariantStatement") {
    return new SgOmpEndDeclareVariantStatement();
  }
  if (kind == "SgOmpThreadprivateStatement") {
    return new SgOmpThreadprivateStatement();
  }
  if (kind == "SgOmpAllocateStatement") {
    return new SgOmpAllocateStatement();
  }
  if (kind == "SgOmpTargetUpdateStatement") {
    return new SgOmpTargetUpdateStatement();
  }
  if (kind == "SgOmpTargetEnterDataStatement") {
    return new SgOmpTargetEnterDataStatement(
        static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpTargetExitDataStatement") {
    return new SgOmpTargetExitDataStatement(
        static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpTargetStatement") {
    return new SgOmpTargetStatement(static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpTargetDataStatement") {
    return new SgOmpTargetDataStatement(static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpParallelStatement") {
    return new SgOmpParallelStatement(static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpForStatement") {
    return new SgOmpForStatement(static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpDoStatement") {
    return new SgOmpDoStatement(static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpForSimdStatement") {
    return new SgOmpForSimdStatement(static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpLoopStatement") {
    return new SgOmpLoopStatement(static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpSimdStatement") {
    return new SgOmpSimdStatement(static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpSingleStatement") {
    return new SgOmpSingleStatement(static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpTaskStatement") {
    return new SgOmpTaskStatement(static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpMasterStatement") {
    return new SgOmpMasterStatement(static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpParallelMasterStatement") {
    return new SgOmpParallelMasterStatement(
        static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpMasterTaskloopStatement") {
    return new SgOmpMasterTaskloopStatement(
        static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpMasterTaskloopSimdStatement") {
    return new SgOmpMasterTaskloopSimdStatement(
        static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpParallelMasterTaskloopStatement") {
    return new SgOmpParallelMasterTaskloopStatement(
        static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpParallelMasterTaskloopSimdStatement") {
    return new SgOmpParallelMasterTaskloopSimdStatement(
        static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpSectionStatement") {
    return new SgOmpSectionStatement(static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpSectionsStatement") {
    return new SgOmpSectionsStatement(static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpCriticalStatement") {
    return new SgOmpCriticalStatement(static_cast<SgStatement *>(nullptr),
                                      SgName(p.stringOr("name")));
  }
  if (kind == "SgOmpAtomicStatement") {
    return new SgOmpAtomicStatement(static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpOrderedStatement") {
    return new SgOmpOrderedStatement(static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpParallelLoopStatement") {
    return new SgOmpParallelLoopStatement(static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpTaskgroupStatement") {
    return new SgOmpTaskgroupStatement(static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpTaskloopStatement") {
    return new SgOmpTaskloopStatement(static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpTaskloopSimdStatement") {
    return new SgOmpTaskloopSimdStatement(static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpTaskyieldStatement") {
    return new SgOmpTaskyieldStatement();
  }
  if (kind == "SgOmpCancelStatement") {
    return new SgOmpCancelStatement();
  }
  if (kind == "SgOmpCancellationPointStatement") {
    return new SgOmpCancellationPointStatement();
  }
  if (kind == "SgOmpDepobjStatement") {
    return new SgOmpDepobjStatement(static_cast<SgStatement *>(nullptr),
                                    SgName(p.stringOr("name")));
  }
  if (kind == "SgOmpMetadirectiveStatement") {
    return new SgOmpMetadirectiveStatement(static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpDispatchStatement") {
    return new SgOmpDispatchStatement(static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpDistributeStatement") {
    return new SgOmpDistributeStatement(static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpWorkdistributeStatement") {
    return new SgOmpWorkdistributeStatement(
        static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpDistributeSimdStatement") {
    return new SgOmpDistributeSimdStatement(
        static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpDistributeParallelForStatement") {
    return new SgOmpDistributeParallelForStatement(
        static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpDistributeParallelForSimdStatement") {
    return new SgOmpDistributeParallelForSimdStatement(
        static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpTargetParallelStatement") {
    return new SgOmpTargetParallelStatement(
        static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpTargetParallelForStatement") {
    return new SgOmpTargetParallelForStatement(
        static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpTargetParallelForSimdStatement") {
    return new SgOmpTargetParallelForSimdStatement(
        static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpTargetParallelLoopStatement") {
    return new SgOmpTargetParallelLoopStatement(
        static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpTargetSimdStatement") {
    return new SgOmpTargetSimdStatement(static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpTargetTeamsStatement") {
    return new SgOmpTargetTeamsStatement(static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpTeamsStatement") {
    return new SgOmpTeamsStatement(static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpTeamsDistributeStatement") {
    return new SgOmpTeamsDistributeStatement(
        static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpTeamsDistributeSimdStatement") {
    return new SgOmpTeamsDistributeSimdStatement(
        static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpTeamsDistributeParallelForStatement") {
    return new SgOmpTeamsDistributeParallelForStatement(
        static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpTeamsDistributeParallelForSimdStatement") {
    return new SgOmpTeamsDistributeParallelForSimdStatement(
        static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpTeamsLoopStatement") {
    return new SgOmpTeamsLoopStatement(static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpWorkshareStatement") {
    return new SgOmpWorkshareStatement(static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpUnrollStatement") {
    return new SgOmpUnrollStatement(static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpTileStatement") {
    return new SgOmpTileStatement(static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpTargetTeamsDistributeStatement") {
    return new SgOmpTargetTeamsDistributeStatement(
        static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpTargetTeamsDistributeSimdStatement") {
    return new SgOmpTargetTeamsDistributeSimdStatement(
        static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpTargetTeamsLoopStatement") {
    return new SgOmpTargetTeamsLoopStatement(
        static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpTargetTeamsDistributeParallelForStatement") {
    return new SgOmpTargetTeamsDistributeParallelForStatement(
        static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpTargetTeamsDistributeParallelForSimdStatement") {
    return new SgOmpTargetTeamsDistributeParallelForSimdStatement(
        static_cast<SgStatement *>(nullptr));
  }

  throw std::runtime_error("AST JSON deserializer does not support Sage node " +
                           kind);
}

std::vector<EdgeRecord> edgesFor(const NodeRecord &record,
                                 const std::string &field) {
  std::vector<EdgeRecord> result;
  for (const EdgeRecord &edge : record.edges) {
    if (edge.field == field) {
      result.push_back(edge);
    }
  }
  std::sort(result.begin(), result.end(),
            [](const EdgeRecord &a, const EdgeRecord &b) {
              return a.index < b.index;
            });
  return result;
}

uint64_t singleEdgeTarget(const NodeRecord &record, const std::string &field) {
  std::vector<EdgeRecord> edges = edgesFor(record, field);
  return edges.empty() ? 0 : edges.front().target;
}

SgBitVector bitVectorFromJson(const JsonValue &json,
                              const std::string &field_name) {
  if (json.kind != JsonValue::Kind::Array) {
    throw std::runtime_error("AST JSON " + field_name +
                             " field is not a bool array");
  }
  SgBitVector bits;
  bits.reserve(json.array.size());
  for (const JsonValue &value : json.array) {
    if (value.kind != JsonValue::Kind::Bool) {
      throw std::runtime_error("AST JSON " + field_name +
                               " contains a non-bool value");
    }
    bits.push_back(value.asBool());
  }
  return bits;
}

SgModuleStatement *externalModuleFromJson(const JsonValue &json) {
  if (!json.boolOr("present", false)) {
    return nullptr;
  }
  const std::string name = json.at("name").asString();
  if (name.empty()) {
    throw std::runtime_error("AST JSON external_module is missing module name");
  }
  SgModuleStatement *module =
      new SgModuleStatement(SgName(name),
                            static_cast<SgClassDeclaration::class_types>(
                                json.at("class_type").asInt()),
                            nullptr, nullptr);
  installTransformationSourcePosition(module);
  markAstJsonExternalModule(module, json.stringOr("source_file"));
  return module;
}

SgFunctionDeclaration *externalFunctionFromJson(const JsonValue &json,
                                                const NodeMap &nodes) {
  if (!json.boolOr("present", false)) {
    return nullptr;
  }
  const std::string name = json.at("name").asString();
  if (name.empty()) {
    throw std::runtime_error("AST JSON external_function is missing name");
  }
  const std::string source_file = json.at("source_file").asString();
  if (source_file.empty()) {
    throw std::runtime_error("AST JSON external_function " + name +
                             " requires a non-empty source_file");
  }
  SgFunctionType *function_type =
      isSgFunctionType(typeFromJson(json.at("function_type"), nodes));
  if (function_type == nullptr) {
    throw std::runtime_error(
        "AST JSON external_function function_type is not a SgFunctionType");
  }

  SgFunctionDeclaration *decl = nullptr;
  const std::string kind = json.at("kind").asString();
  if (kind == "SgProcedureHeaderStatement") {
    SgProcedureHeaderStatement *procedure =
        new SgProcedureHeaderStatement(SgName(name), function_type, nullptr);
    procedure->set_subprogram_kind(
        static_cast<SgProcedureHeaderStatement::subprogram_kind_enum>(
            json.intOr(
                "subprogram_kind",
                SgProcedureHeaderStatement::e_function_subprogram_kind)));
    decl = procedure;
  } else if (kind == "SgFunctionDeclaration") {
    decl = new SgFunctionDeclaration(SgName(name), function_type, nullptr);
  } else {
    throw std::runtime_error(
        "AST JSON external_function has unsupported declaration kind: " + kind);
  }

  const JsonValue *location = json.find("location");
  if (location == nullptr) {
    throw std::runtime_error("AST JSON external_function " + name +
                             " has no location");
  }
  restoreNodeSourcePositionFromJson(decl, *location,
                                    "external_function " + name);
  SgFunctionParameterList *parameter_list = decl->get_parameterList();
  if (parameter_list == nullptr) {
    throw std::runtime_error("AST JSON external_function " + name +
                             " constructor did not create a parameterList");
  }
  const JsonValue *parameter_list_location =
      json.find("parameter_list_location");
  if (parameter_list_location == nullptr) {
    throw std::runtime_error("AST JSON external_function " + name +
                             " has no parameter_list_location");
  }
  restoreNodeSourcePositionFromJson(parameter_list, *parameter_list_location,
                                    "external_function parameterList " + name);
  parameter_list->set_parent(decl);
  if (json.boolOr("parameter_list_syntax_aliases_parameter_list", false)) {
    decl->set_parameterList_syntax(parameter_list);
  }
  const JsonValue *function_parameter_scope =
      json.find("function_parameter_scope");
  if (function_parameter_scope == nullptr) {
    throw std::runtime_error("AST JSON external_function " + name +
                             " has no function_parameter_scope");
  }
  SgFunctionParameterScope *parameter_scope =
      externalFunctionParameterScopeFromJson(*function_parameter_scope, nodes,
                                             name);
  if (parameter_scope != nullptr) {
    parameter_scope->set_parent(decl);
    decl->set_functionParameterScope(parameter_scope);
  }
  decl->set_firstNondefiningDeclaration(decl);
  decl->set_definingDeclaration(nullptr);
  markAstJsonExternalFunction(decl, source_file);
  return decl;
}

SgDeclarationStatement *externalDeclarationReferenceFromJson(
    const JsonValue *json, const NodeMap &nodes, const std::string &context) {
  if (json == nullptr || !json->boolOr("present", false)) {
    return nullptr;
  }
  const std::string kind = json->at("kind").asString();
  if (const JsonValue *external = json->find("external_function")) {
    SgFunctionDeclaration *decl = externalFunctionFromJson(*external, nodes);
    if (decl == nullptr) {
      throw std::runtime_error("AST JSON " + context +
                               " external_function reconstructed as null");
    }
    return decl;
  }
  if (const JsonValue *external = json->find("external_module")) {
    SgModuleStatement *module = externalModuleFromJson(*external);
    if (module == nullptr) {
      throw std::runtime_error("AST JSON " + context +
                               " external_module reconstructed as null");
    }
    return module;
  }
  if (const JsonValue *external = json->find("external_class")) {
    SgClassDeclaration *decl = externalClassDeclarationFromJson(*external);
    if (decl == nullptr) {
      throw std::runtime_error("AST JSON " + context +
                               " external_class reconstructed as null");
    }
    return decl;
  }
  throw std::runtime_error(
      "AST JSON " + context +
      " has unsupported external declaration kind: " + kind);
}

template <typename ListT, typename NodeT>
void appendEdgeList(ListT &list, const NodeRecord &record,
                    const std::string &field, const NodeMap &nodes,
                    SgNode *parent, bool owns_children = true) {
  for (const EdgeRecord &edge : edgesFor(record, field)) {
    NodeT *child = nodeByIdAs<NodeT>(nodes, edge.target);
    list.push_back(child);
    if (owns_children) {
      child->set_parent(parent);
      if (SgDeclarationStatement *decl = isSgDeclarationStatement(child)) {
        if (decl->get_scope() == nullptr) {
          decl->set_scope(isSgScopeStatement(parent));
        }
      }
    }
  }
}

void setNodeSourcePosition(SgNode *node, const NodeRecord &record) {
  const JsonValue *start = record.location.find("start");
  const JsonValue *end = record.location.find("end");
  Sg_File_Info *start_info =
      start != nullptr ? buildFileInfo(*start, node) : nullptr;
  Sg_File_Info *end_info = end != nullptr ? buildFileInfo(*end, node) : nullptr;

  if (SgLocatedNode *located = isSgLocatedNode(node)) {
    located->set_startOfConstruct(start_info);
    located->set_endOfConstruct(end_info);
  } else if (SgPragma *pragma = isSgPragma(node)) {
    pragma->set_startOfConstruct(start_info);
    pragma->set_endOfConstruct(end_info);
  } else if (SgInitializedName *name = isSgInitializedName(node)) {
    name->set_startOfConstruct(start_info);
    name->set_endOfConstruct(end_info);
  } else if (SgFile *file = isSgFile(node)) {
    file->set_startOfConstruct(start_info);
  }
}

void setNodeFlags(SgNode *node, const NodeRecord &record) {
  node->set_containsTransformation(
      record.flags.boolOr("contains_transformation", false));
  if (SgLocatedNode *located = isSgLocatedNode(node)) {
    located->set_containsTransformationToSurroundingWhitespace(
        record.flags.boolOr("contains_transformation_to_surrounding_whitespace",
                            false));
  }
}

void restoreAvailableSourcePositionsAndScopes(const AstFileRecord &ast,
                                              const NodeMap &nodes) {
  for (const NodeRecord &record : ast.nodes) {
    auto found = nodes.find(record.id);
    if (found == nodes.end()) {
      continue;
    }
    setNodeSourcePosition(found->second, record);
    setNodeFlags(found->second, record);
  }

  for (const NodeRecord &record : ast.nodes) {
    auto found = nodes.find(record.id);
    if (found == nodes.end()) {
      continue;
    }
    if (uint64_t target = singleEdgeTarget(record, "parent")) {
      auto parent = nodes.find(target);
      if (parent != nodes.end()) {
        found->second->set_parent(parent->second);
      }
    }
  }

  for (const NodeRecord &record : ast.nodes) {
    auto found = nodes.find(record.id);
    if (found == nodes.end()) {
      continue;
    }
    SgNode *node = found->second;
    if (SgInitializedName *name = isSgInitializedName(node)) {
      if (uint64_t target = singleEdgeTarget(record, "scope")) {
        auto scope = nodes.find(target);
        if (scope != nodes.end()) {
          name->set_scope(isSgScopeStatement(scope->second));
        }
      }
    }
    if (SgDeclarationStatement *decl = isSgDeclarationStatement(node)) {
      if (uint64_t target = singleEdgeTarget(record, "scope")) {
        auto scope = nodes.find(target);
        if (scope != nodes.end()) {
          decl->set_scope(isSgScopeStatement(scope->second));
        }
      }
    }
  }
}

void attachPreprocessingInfo(SgNode *node, const NodeRecord &record) {
  SgLocatedNode *located = isSgLocatedNode(node);
  if (located == nullptr ||
      record.preprocessing.kind != JsonValue::Kind::Array) {
    return;
  }
  auto make_info = [](const JsonValue &entry) {
    if (entry.kind != JsonValue::Kind::Object) {
      throw std::runtime_error("AST JSON preprocessing entry is not an object");
    }
    Sg_File_Info *file_info =
        buildFileInfo(entry.at("file_info"), SgTypeDefault::createType());
    if (file_info == nullptr) {
      throw std::runtime_error(
          "AST JSON preprocessing entry requires present file_info");
    }
    PreprocessingInfo *info = new PreprocessingInfo(
        static_cast<PreprocessingInfo::DirectiveType>(
            entry.intOr("directive", 0)),
        entry.stringOr("text"), file_info->get_filenameString(),
        file_info->get_line(), file_info->get_col(),
        static_cast<int>(entry.intOr("lines", 1)),
        static_cast<PreprocessingInfo::RelativePositionType>(
            entry.intOr("relative", PreprocessingInfo::before)));
    Sg_File_Info *constructor_file_info = info->get_file_info();
    info->set_file_info(file_info);
    delete constructor_file_info;
    if (entry.boolOr("transformation", false)) {
      info->setAsTransformation();
    } else {
      info->unsetAsTransformation();
    }
    return info;
  };
  AttachedPreprocessingInfoType *&infos =
      located->getAttachedPreprocessingInfo();
  if (infos == nullptr) {
    infos = new AttachedPreprocessingInfoType;
  } else {
    infos->clear();
  }
  for (const JsonValue &entry : record.preprocessing.array) {
    infos->push_back(make_info(entry));
  }
}

void attachAstAttributes(SgNode *node, const NodeRecord &record) {
  const JsonValue *attributes = record.properties.find("attributes");
  if (node == nullptr || attributes == nullptr ||
      attributes->kind != JsonValue::Kind::Array) {
    return;
  }
  for (const JsonValue &entry : attributes->array) {
    const std::string name = entry.stringOr("name");
    const std::string type = entry.stringOr("type");
    if (name.empty()) {
      continue;
    }
    if (type == "AstIntAttribute") {
      node->setAttribute(
          name, new AstIntAttribute(static_cast<int>(entry.intOr("value", 0))));
    } else if (type == "AstStringAttribute") {
      node->setAttribute(
          name, new AstValueAttribute<std::string>(entry.stringOr("value")));
    } else if (type == "AstMarkerAttribute") {
      node->setAttribute(name, new AstJsonMarkerAttribute());
    } else {
      throw std::runtime_error("AST JSON attribute type is unsupported: " +
                               type);
    }
  }
}

void linkNodeEdges(const NodeRecord &record, const NodeMap &nodes) {
  SgNode *node = nodeById(nodes, record.id);
  if (uint64_t target = singleEdgeTarget(record, "parent")) {
    node->set_parent(nodeById(nodes, target));
  }
  if (SgStatement *stmt = isSgStatement(node)) {
    if (uint64_t target = singleEdgeTarget(record, "numeric_label")) {
      SgLabelRefExp *label = nodeByIdAs<SgLabelRefExp>(nodes, target);
      stmt->set_numeric_label(label);
      label->set_parent(stmt);
    }
  }

  if (SgSourceFile *file = isSgSourceFile(node)) {
    const uint64_t target = singleEdgeTarget(record, "globalScope");
    if (target != 0) {
      SgGlobal *global = nodeByIdAs<SgGlobal>(nodes, target);
      file->set_globalScope(global);
      global->set_parent(file);
    }
    appendEdgeList<SgTokenPtrList, SgToken>(file->get_token_list(), record,
                                            "token_list", nodes, file);
    return;
  }
  if (SgGlobal *global = isSgGlobal(node)) {
    appendEdgeList<SgDeclarationStatementPtrList, SgDeclarationStatement>(
        global->get_declarations(), record, "declarations", nodes, global);
    return;
  }
  if (SgNamespaceDefinitionStatement *def =
          isSgNamespaceDefinitionStatement(node)) {
    appendEdgeList<SgDeclarationStatementPtrList, SgDeclarationStatement>(
        def->get_declarations(), record, "declarations", nodes, def,
        !def->get_isUnionOfReentrantNamespaceDefinitions());
  }
  if (SgDeclarationScope *scope = isSgDeclarationScope(node)) {
    appendEdgeList<SgDeclarationStatementPtrList, SgDeclarationStatement>(
        scope->get_declarations(), record, "declarations", nodes, scope);
  }
  if (SgFunctionParameterScope *scope = isSgFunctionParameterScope(node)) {
    appendEdgeList<SgDeclarationStatementPtrList, SgDeclarationStatement>(
        scope->get_declarations(), record, "declarations", nodes, scope);
  }
  if (SgBasicBlock *block = isSgBasicBlock(node)) {
    appendEdgeList<SgStatementPtrList, SgStatement>(
        block->get_statements(), record, "statements", nodes, block);
    return;
  }
  if (SgFunctionParameterList *params = isSgFunctionParameterList(node)) {
    appendEdgeList<SgInitializedNamePtrList, SgInitializedName>(
        params->get_args(), record, "args", nodes, params);
  }
  if (SgExprListExp *exprs = isSgExprListExp(node)) {
    appendEdgeList<SgExpressionPtrList, SgExpression>(
        exprs->get_expressions(), record, "expressions", nodes, exprs);
    return;
  }
  if (SgClassDefinition *def = isSgClassDefinition(node)) {
    if (uint64_t target = singleEdgeTarget(record, "declaration")) {
      SgClassDeclaration *decl = nodeByIdAs<SgClassDeclaration>(nodes, target);
      def->set_declaration(decl);
      if (decl->get_definition() == nullptr) {
        decl->set_definition(def);
      }
    }
    appendEdgeList<SgBaseClassPtrList, SgBaseClass>(
        def->get_inheritances(), record, "inheritances", nodes, def);
    appendEdgeList<SgDeclarationStatementPtrList, SgDeclarationStatement>(
        def->get_members(), record, "members", nodes, def);
    return;
  }
  if (SgBaseClass *base = isSgBaseClass(node)) {
    uint64_t target = singleEdgeTarget(record, "base_class");
    if (target == 0) {
      target = static_cast<uint64_t>(record.properties.intOr("base_class", 0));
    }
    if (target != 0) {
      base->set_base_class(nodeByIdAs<SgClassDeclaration>(nodes, target));
    }
    if (SgExpBaseClass *expr_base = isSgExpBaseClass(base)) {
      if (uint64_t expr_target = singleEdgeTarget(record, "base_class_exp")) {
        SgExpression *expr = nodeByIdAs<SgExpression>(nodes, expr_target);
        expr_base->set_base_class_exp(expr);
        expr->set_parent(expr_base);
      } else if (const JsonValue *expr_json =
                     record.properties.find("base_class_exp")) {
        if (SgExpression *expr = expressionFromRef(*expr_json, nodes)) {
          expr_base->set_base_class_exp(expr);
          expr->set_parent(expr_base);
        }
      }
    }
    if (SgNonrealBaseClass *nonreal_base = isSgNonrealBaseClass(base)) {
      uint64_t nonreal_target = singleEdgeTarget(record, "base_class_nonreal");
      if (nonreal_target == 0) {
        nonreal_target = static_cast<uint64_t>(
            record.properties.intOr("base_class_nonreal", 0));
      }
      if (nonreal_target != 0) {
        nonreal_base->set_base_class_nonreal(
            nodeByIdAs<SgNonrealDecl>(nodes, nonreal_target));
      }
    }
    return;
  }
  if (SgCtorInitializerList *ctors = isSgCtorInitializerList(node)) {
    appendEdgeList<SgInitializedNamePtrList, SgInitializedName>(
        ctors->get_ctors(), record, "ctors", nodes, ctors);
  }
  if (SgVariableDeclaration *decl = isSgVariableDeclaration(node)) {
    appendEdgeList<SgInitializedNamePtrList, SgInitializedName>(
        decl->get_variables(), record, "variables", nodes, decl);
    if (uint64_t target =
            singleEdgeTarget(record, "baseTypeDefiningDeclaration")) {
      SgDeclarationStatement *base_decl =
          nodeByIdAs<SgDeclarationStatement>(nodes, target);
      decl->set_baseTypeDefiningDeclaration(base_decl);
      base_decl->set_parent(decl);
    }
  }
  if (SgVariableDefinition *def = isSgVariableDefinition(node)) {
    if (uint64_t target = singleEdgeTarget(record, "vardefn")) {
      SgInitializedName *name = nodeByIdAs<SgInitializedName>(nodes, target);
      def->set_vardefn(name);
      name->set_definition(def);
      def->set_parent(name);
    }
    if (uint64_t target = singleEdgeTarget(record, "bitfield")) {
      SgExpression *bitfield = nodeByIdAs<SgExpression>(nodes, target);
      def->set_bitfield(bitfield);
      bitfield->set_parent(def);
    }
  }
  if (SgCommonBlock *stmt = isSgCommonBlock(node)) {
    appendEdgeList<SgCommonBlockObjectPtrList, SgCommonBlockObject>(
        stmt->get_block_list(), record, "block_list", nodes, stmt);
  }
  if (SgCommonBlockObject *object = isSgCommonBlockObject(node)) {
    object->set_block_name(record.properties.stringOr("block_name"));
    if (uint64_t target = singleEdgeTarget(record, "variable_reference_list")) {
      SgExprListExp *list = nodeByIdAs<SgExprListExp>(nodes, target);
      object->set_variable_reference_list(list);
      list->set_parent(object);
    }
  }
  if (SgEnumDeclaration *decl = isSgEnumDeclaration(node)) {
    appendEdgeList<SgInitializedNamePtrList, SgInitializedName>(
        decl->get_enumerators(), record, "enumerators", nodes, decl);
  }
  if (SgImplicitStatement *stmt = isSgImplicitStatement(node)) {
    appendEdgeList<SgInitializedNamePtrList, SgInitializedName>(
        stmt->get_variables(), record, "variables", nodes, stmt);
  }
  if (SgForInitStatement *init = isSgForInitStatement(node)) {
    appendEdgeList<SgStatementPtrList, SgStatement>(
        init->get_init_stmt(), record, "init_stmt", nodes, init);
  }
  if (SgOmpClauseStatement *stmt = isSgOmpClauseStatement(node)) {
    appendEdgeList<SgOmpClausePtrList, SgOmpClause>(stmt->get_clauses(), record,
                                                    "clauses", nodes, stmt);
  } else if (SgOmpClauseBodyStatement *stmt =
                 isSgOmpClauseBodyStatement(node)) {
    appendEdgeList<SgOmpClausePtrList, SgOmpClause>(stmt->get_clauses(), record,
                                                    "clauses", nodes, stmt);
  }
  if (SgOmpDeclareSimdStatement *stmt = isSgOmpDeclareSimdStatement(node)) {
    appendEdgeList<SgOmpClausePtrList, SgOmpClause>(stmt->get_clauses(), record,
                                                    "clauses", nodes, stmt);
  }
  if (SgOmpDeclareVariantStatement *stmt =
          isSgOmpDeclareVariantStatement(node)) {
    appendEdgeList<SgOmpClausePtrList, SgOmpClause>(stmt->get_clauses(), record,
                                                    "clauses", nodes, stmt);
    if (uint64_t target = singleEdgeTarget(record, "variant_function_ref")) {
      SgExpression *expr = nodeByIdAs<SgExpression>(nodes, target);
      stmt->set_variant_function_ref(expr);
      expr->set_parent(stmt);
    }
  }
  if (SgOmpBeginDeclareVariantStatement *stmt =
          isSgOmpBeginDeclareVariantStatement(node)) {
    appendEdgeList<SgOmpClausePtrList, SgOmpClause>(stmt->get_clauses(), record,
                                                    "clauses", nodes, stmt);
    stmt->set_captured_region(record.properties.stringOr("captured_region"));
  }
  if (SgOmpDeclareTargetStatement *stmt = isSgOmpDeclareTargetStatement(node)) {
    appendEdgeList<SgOmpClausePtrList, SgOmpClause>(stmt->get_clauses(), record,
                                                    "clauses", nodes, stmt);
    appendEdgeList<SgStatementPtrList, SgStatement>(
        stmt->get_statements(), record, "statements", nodes, stmt);
    stmt->set_device_type_kind(
        static_cast<SgOmpClause::omp_when_context_kind_enum>(
            record.properties.intOr(
                "device_type_kind",
                SgOmpClause::e_omp_when_context_kind_unknown)));
  }
  if (SgOmpDeclareMapperStatement *stmt = isSgOmpDeclareMapperStatement(node)) {
    appendEdgeList<SgOmpClausePtrList, SgOmpClause>(stmt->get_clauses(), record,
                                                    "clauses", nodes, stmt);
    if (uint64_t target = singleEdgeTarget(record, "user_defined_identifier")) {
      SgExpression *expr = nodeByIdAs<SgExpression>(nodes, target);
      stmt->set_user_defined_identifier(expr);
      expr->set_parent(stmt);
    }
    if (uint64_t target = singleEdgeTarget(record, "mapper_type")) {
      SgExpression *expr = nodeByIdAs<SgExpression>(nodes, target);
      stmt->set_mapper_type(expr);
      expr->set_parent(stmt);
    }
    if (uint64_t target = singleEdgeTarget(record, "mapper_variable")) {
      SgExpression *expr = nodeByIdAs<SgExpression>(nodes, target);
      stmt->set_mapper_variable(expr);
      expr->set_parent(stmt);
    }
  }
  if (SgOmpRequiresStatement *stmt = isSgOmpRequiresStatement(node)) {
    appendEdgeList<SgOmpClausePtrList, SgOmpClause>(stmt->get_clauses(), record,
                                                    "clauses", nodes, stmt);
  }
  if (SgOmpTaskwaitStatement *stmt = isSgOmpTaskwaitStatement(node)) {
    appendEdgeList<SgOmpClausePtrList, SgOmpClause>(stmt->get_clauses(), record,
                                                    "clauses", nodes, stmt);
  }
  if (SgIOStatement *stmt = isSgIOStatement(node)) {
    if (uint64_t target = singleEdgeTarget(record, "io_stmt_list")) {
      SgExprListExp *expr = nodeByIdAs<SgExprListExp>(nodes, target);
      stmt->set_io_stmt_list(expr);
      expr->set_parent(stmt);
    }
    if (uint64_t target = singleEdgeTarget(record, "unit")) {
      SgExpression *expr = nodeByIdAs<SgExpression>(nodes, target);
      stmt->set_unit(expr);
      expr->set_parent(stmt);
    }
    if (uint64_t target = singleEdgeTarget(record, "iostat")) {
      SgExpression *expr = nodeByIdAs<SgExpression>(nodes, target);
      stmt->set_iostat(expr);
      expr->set_parent(stmt);
    }
    if (uint64_t target = singleEdgeTarget(record, "err")) {
      SgExpression *expr = nodeByIdAs<SgExpression>(nodes, target);
      stmt->set_err(expr);
      expr->set_parent(stmt);
    }
    if (uint64_t target = singleEdgeTarget(record, "iomsg")) {
      SgExpression *expr = nodeByIdAs<SgExpression>(nodes, target);
      stmt->set_iomsg(expr);
      expr->set_parent(stmt);
    }
  }
  if (SgLabelStatement *stmt = isSgLabelStatement(node)) {
    if (uint64_t target = singleEdgeTarget(record, "scope")) {
      stmt->set_scope(nodeByIdAs<SgScopeStatement>(nodes, target));
    }
    if (uint64_t target = singleEdgeTarget(record, "statement")) {
      SgStatement *child = nodeByIdAs<SgStatement>(nodes, target);
      stmt->set_statement(child);
      child->set_parent(stmt);
    }
  }
  if (SgPrintStatement *stmt = isSgPrintStatement(node)) {
    if (uint64_t target = singleEdgeTarget(record, "format")) {
      SgExpression *expr = nodeByIdAs<SgExpression>(nodes, target);
      stmt->set_format(expr);
      expr->set_parent(stmt);
    }
  }
  if (SgWriteStatement *stmt = isSgWriteStatement(node)) {
    if (uint64_t target = singleEdgeTarget(record, "format")) {
      SgExpression *expr = nodeByIdAs<SgExpression>(nodes, target);
      stmt->set_format(expr);
      expr->set_parent(stmt);
    }
  }
  if (SgReadStatement *stmt = isSgReadStatement(node)) {
    if (uint64_t target = singleEdgeTarget(record, "format")) {
      SgExpression *expr = nodeByIdAs<SgExpression>(nodes, target);
      stmt->set_format(expr);
      expr->set_parent(stmt);
    }
  }
  if (SgAttributeSpecificationStatement *stmt =
          isSgAttributeSpecificationStatement(node)) {
    if (uint64_t target = singleEdgeTarget(record, "parameter_list")) {
      SgExprListExp *exprs = nodeByIdAs<SgExprListExp>(nodes, target);
      stmt->set_parameter_list(exprs);
      exprs->set_parent(stmt);
    }
    if (uint64_t target = singleEdgeTarget(record, "bind_list")) {
      SgExprListExp *exprs = nodeByIdAs<SgExprListExp>(nodes, target);
      stmt->set_bind_list(exprs);
      exprs->set_parent(stmt);
    }
  }
  if (SgInterfaceStatement *stmt = isSgInterfaceStatement(node)) {
    appendEdgeList<SgInterfaceBodyPtrList, SgInterfaceBody>(
        stmt->get_interface_body_list(), record, "interface_body_list", nodes,
        stmt);
    if (uint64_t target = singleEdgeTarget(record, "end_numeric_label")) {
      SgLabelRefExp *label = nodeByIdAs<SgLabelRefExp>(nodes, target);
      stmt->set_end_numeric_label(label);
      label->set_parent(stmt);
    }
  }
  if (SgInterfaceBody *body = isSgInterfaceBody(node)) {
    uint64_t target = singleEdgeTarget(record, "functionDeclaration");
    if (target == 0) {
      target = static_cast<uint64_t>(
          record.properties.intOr("function_declaration", 0));
    }
    if (target != 0) {
      body->set_functionDeclaration(
          nodeByIdAs<SgFunctionDeclaration>(nodes, target));
    }
  }

  auto set_statement = [&](const std::string &field,
                           void (SgStatement::*setter)(SgStatement *)) {
    const uint64_t target = singleEdgeTarget(record, field);
    if (target != 0) {
      SgStatement *child = nodeByIdAs<SgStatement>(nodes, target);
      (isSgStatement(node)->*setter)(child);
      child->set_parent(node);
    }
  };
  (void)set_statement;

  if (SgInitializedName *name = isSgInitializedName(node)) {
    if (uint64_t target = singleEdgeTarget(record, "initptr")) {
      SgInitializer *init = nodeByIdAs<SgInitializer>(nodes, target);
      name->set_initptr(init);
      init->set_parent(name);
    }
    if (uint64_t target = singleEdgeTarget(record, "declptr")) {
      name->set_declptr(nodeByIdAs<SgDeclarationStatement>(nodes, target));
    }
    if (uint64_t target = singleEdgeTarget(record, "prev_decl_item")) {
      name->set_prev_decl_item(nodeByIdAs<SgInitializedName>(nodes, target));
    }
    if (uint64_t target = singleEdgeTarget(record, "scope")) {
      name->set_scope(nodeByIdAs<SgScopeStatement>(nodes, target));
    }
  }
  if (SgDeclarationStatement *decl = isSgDeclarationStatement(node)) {
    if (uint64_t target = singleEdgeTarget(record, "parent")) {
      decl->set_parent(nodeById(nodes, target));
    }
    if (uint64_t target = singleEdgeTarget(record, "scope")) {
      decl->set_scope(nodeByIdAs<SgScopeStatement>(nodes, target));
    }
    if (uint64_t target =
            singleEdgeTarget(record, "firstNondefiningDeclaration")) {
      decl->set_firstNondefiningDeclaration(
          nodeByIdAs<SgDeclarationStatement>(nodes, target));
    }
    if (uint64_t target = singleEdgeTarget(record, "definingDeclaration")) {
      decl->set_definingDeclaration(
          nodeByIdAs<SgDeclarationStatement>(nodes, target));
    }
    if (uint64_t target = singleEdgeTarget(record, "declarationScope")) {
      decl->set_declarationScope(nodeByIdAs<SgDeclarationScope>(nodes, target));
    }
    if (uint64_t target = singleEdgeTarget(record, "nonreal_decl_scope")) {
      decl->set_nonreal_decl_scope(
          nodeByIdAs<SgDeclarationScope>(nodes, target));
    }
  }
  auto link_result_name = [&](auto *decl) {
    if (uint64_t target = singleEdgeTarget(record, "result_name")) {
      SgInitializedName *name = nodeByIdAs<SgInitializedName>(nodes, target);
      decl->set_result_name(name);
      if (name->get_parent() == nullptr) {
        name->set_parent(decl);
      }
      if (name->get_scope() == nullptr) {
        name->set_scope(decl->get_scope() != nullptr ? decl->get_scope()
                                                     : nearestScope(decl));
      }
    }
  };
  if (SgProcedureHeaderStatement *decl = isSgProcedureHeaderStatement(node)) {
    link_result_name(decl);
  }
  if (SgEntryStatement *decl = isSgEntryStatement(node)) {
    link_result_name(decl);
  }
  if (SgNonrealDecl *decl = isSgNonrealDecl(node)) {
    appendEdgeList<SgTemplateArgumentPtrList, SgTemplateArgument>(
        decl->get_tpl_args(), record, "tpl_args", nodes, decl);
    if (uint64_t target = singleEdgeTarget(record, "templateDeclaration")) {
      decl->set_templateDeclaration(
          nodeByIdAs<SgDeclarationStatement>(nodes, target));
    }
    if (uint64_t target = singleEdgeTarget(record, "conceptConstraint")) {
      SgExpression *expr = nodeByIdAs<SgExpression>(nodes, target);
      decl->set_conceptConstraint(expr);
      expr->set_parent(decl);
    }
  }
  if (SgTypedefDeclaration *decl = isSgTypedefDeclaration(node)) {
    if (uint64_t target = singleEdgeTarget(record, "declaration")) {
      SgDeclarationStatement *base_decl =
          nodeByIdAs<SgDeclarationStatement>(nodes, target);
      decl->set_declaration(base_decl);
      if (base_decl->get_parent() == nullptr) {
        base_decl->set_parent(decl);
      }
    }
  }
  if (SgTemplateInstantiationDirectiveStatement *decl =
          isSgTemplateInstantiationDirectiveStatement(node)) {
    if (uint64_t target = singleEdgeTarget(record, "declaration")) {
      SgDeclarationStatement *instantiated_decl =
          nodeByIdAs<SgDeclarationStatement>(nodes, target);
      decl->set_declaration(instantiated_decl);
      instantiated_decl->set_parent(decl);
    }
  }
  if (SgUsingDirectiveStatement *decl = isSgUsingDirectiveStatement(node)) {
    if (uint64_t target = singleEdgeTarget(record, "namespaceDeclaration")) {
      decl->set_namespaceDeclaration(
          nodeByIdAs<SgNamespaceDeclarationStatement>(nodes, target));
    } else if (uint64_t target = static_cast<uint64_t>(
                   record.properties.intOr("namespace_declaration", 0))) {
      decl->set_namespaceDeclaration(
          nodeByIdAs<SgNamespaceDeclarationStatement>(nodes, target));
    }
  }
  if (SgUsingDeclarationStatement *decl = isSgUsingDeclarationStatement(node)) {
    uint64_t target = singleEdgeTarget(record, "declaration");
    if (target == 0) {
      target = static_cast<uint64_t>(record.properties.intOr("declaration", 0));
    }
    if (target != 0) {
      decl->set_declaration(nodeByIdAs<SgDeclarationStatement>(nodes, target));
    }
    target = singleEdgeTarget(record, "initializedName");
    if (target == 0) {
      target =
          static_cast<uint64_t>(record.properties.intOr("initialized_name", 0));
    }
    if (target != 0) {
      decl->set_initializedName(nodeByIdAs<SgInitializedName>(nodes, target));
    }
  }
  if (SgUseStatement *stmt = isSgUseStatement(node)) {
    appendEdgeList<SgRenamePairPtrList, SgRenamePair>(
        stmt->get_rename_list(), record, "rename_list", nodes, stmt);
    if (uint64_t target = singleEdgeTarget(record, "module")) {
      stmt->set_module(nodeByIdAs<SgModuleStatement>(nodes, target));
    } else if (const JsonValue *external =
                   record.properties.find("external_module")) {
      stmt->set_module(externalModuleFromJson(*external));
    }
  }
  auto set_template_requires_clause = [&](auto *decl) {
    if (uint64_t target = singleEdgeTarget(record, "requiresClause")) {
      SgExpression *expr = nodeByIdAs<SgExpression>(nodes, target);
      decl->set_requiresClause(expr);
      expr->set_parent(decl);
    }
  };
  if (SgTemplateVariableDeclaration *decl =
          isSgTemplateVariableDeclaration(node)) {
    appendEdgeList<SgTemplateParameterPtrList, SgTemplateParameter>(
        decl->get_templateParameters(), record, "templateParameters", nodes,
        decl);
    appendEdgeList<SgTemplateArgumentPtrList, SgTemplateArgument>(
        decl->get_templateSpecializationArguments(), record,
        "templateSpecializationArguments", nodes, decl);
    appendEdgeList<SgTemplateArgumentPtrList, SgTemplateArgument>(
        decl->get_deducedTemplateArguments(), record,
        "deducedTemplateArguments", nodes, decl);
    if (uint64_t target =
            singleEdgeTarget(record, "specializedTemplateDeclaration")) {
      decl->set_specializedTemplateDeclaration(
          nodeByIdAs<SgDeclarationStatement>(nodes, target));
    }
  }
  if (SgTemplateTypedefDeclaration *decl =
          isSgTemplateTypedefDeclaration(node)) {
    appendEdgeList<SgTemplateParameterPtrList, SgTemplateParameter>(
        decl->get_templateParameters(), record, "templateParameters", nodes,
        decl);
    appendEdgeList<SgTemplateArgumentPtrList, SgTemplateArgument>(
        decl->get_templateSpecializationArguments(), record,
        "templateSpecializationArguments", nodes, decl);
    set_template_requires_clause(decl);
  }
  if (SgTemplateClassDeclaration *decl = isSgTemplateClassDeclaration(node)) {
    appendEdgeList<SgTemplateParameterPtrList, SgTemplateParameter>(
        decl->get_templateParameters(), record, "templateParameters", nodes,
        decl);
    appendEdgeList<SgTemplateArgumentPtrList, SgTemplateArgument>(
        decl->get_templateSpecializationArguments(), record,
        "templateSpecializationArguments", nodes, decl);
    set_template_requires_clause(decl);
  }
  if (SgTemplateFunctionDeclaration *decl =
          isSgTemplateFunctionDeclaration(node)) {
    appendEdgeList<SgTemplateParameterPtrList, SgTemplateParameter>(
        decl->get_templateParameters(), record, "templateParameters", nodes,
        decl);
    appendEdgeList<SgTemplateArgumentPtrList, SgTemplateArgument>(
        decl->get_templateSpecializationArguments(), record,
        "templateSpecializationArguments", nodes, decl);
    set_template_requires_clause(decl);
  }
  if (SgTemplateMemberFunctionDeclaration *decl =
          isSgTemplateMemberFunctionDeclaration(node)) {
    appendEdgeList<SgTemplateParameterPtrList, SgTemplateParameter>(
        decl->get_templateParameters(), record, "templateParameters", nodes,
        decl);
    appendEdgeList<SgTemplateArgumentPtrList, SgTemplateArgument>(
        decl->get_templateSpecializationArguments(), record,
        "templateSpecializationArguments", nodes, decl);
    set_template_requires_clause(decl);
  }
  if (SgTemplateParameter *parameter = isSgTemplateParameter(node)) {
    auto set_expression =
        [&](const std::string &field,
            void (SgTemplateParameter::*setter)(SgExpression *)) {
          if (uint64_t target = singleEdgeTarget(record, field)) {
            SgExpression *expr = nodeByIdAs<SgExpression>(nodes, target);
            (parameter->*setter)(expr);
            expr->set_parent(parameter);
          } else if (const JsonValue *value = record.properties.find(field)) {
            if (SgExpression *expr = expressionFromRef(*value, nodes)) {
              (parameter->*setter)(expr);
              expr->set_parent(parameter);
            }
          }
        };
    set_expression("expression", &SgTemplateParameter::set_expression);
    set_expression("typeConstraint", &SgTemplateParameter::set_typeConstraint);
    set_expression("type_constraint", &SgTemplateParameter::set_typeConstraint);
    set_expression("defaultExpressionParameter",
                   &SgTemplateParameter::set_defaultExpressionParameter);
    set_expression("default_expression_parameter",
                   &SgTemplateParameter::set_defaultExpressionParameter);
    if (uint64_t target = singleEdgeTarget(record, "templateDeclaration")) {
      parameter->set_templateDeclaration(
          nodeByIdAs<SgDeclarationStatement>(nodes, target));
    } else if (uint64_t target = static_cast<uint64_t>(
                   record.properties.intOr("template_declaration", 0))) {
      parameter->set_templateDeclaration(
          nodeByIdAs<SgDeclarationStatement>(nodes, target));
    }
    if (uint64_t target =
            singleEdgeTarget(record, "defaultTemplateDeclarationParameter")) {
      parameter->set_defaultTemplateDeclarationParameter(
          nodeByIdAs<SgDeclarationStatement>(nodes, target));
    } else if (uint64_t target = static_cast<uint64_t>(record.properties.intOr(
                   "default_template_declaration_parameter", 0))) {
      parameter->set_defaultTemplateDeclarationParameter(
          nodeByIdAs<SgDeclarationStatement>(nodes, target));
    }
    if (uint64_t target = singleEdgeTarget(record, "initializedName")) {
      parameter->set_initializedName(
          nodeByIdAs<SgInitializedName>(nodes, target));
    } else if (uint64_t target = static_cast<uint64_t>(
                   record.properties.intOr("initialized_name", 0))) {
      parameter->set_initializedName(
          nodeByIdAs<SgInitializedName>(nodes, target));
    }
  }
  if (SgClassDeclaration *decl = isSgClassDeclaration(node)) {
    if (uint64_t target = singleEdgeTarget(record, "definition")) {
      SgClassDefinition *def = nodeByIdAs<SgClassDefinition>(nodes, target);
      decl->set_definition(def);
      def->set_declaration(decl);
      def->set_parent(decl);
    }
    if (SgModuleStatement *module = isSgModuleStatement(decl)) {
      if (uint64_t target = singleEdgeTarget(record, "end_numeric_label")) {
        SgLabelRefExp *label = nodeByIdAs<SgLabelRefExp>(nodes, target);
        module->set_end_numeric_label(label);
        label->set_parent(module);
      }
    }
    if (SgDerivedTypeStatement *derived = isSgDerivedTypeStatement(decl)) {
      if (uint64_t target = singleEdgeTarget(record, "end_numeric_label")) {
        SgLabelRefExp *label = nodeByIdAs<SgLabelRefExp>(nodes, target);
        derived->set_end_numeric_label(label);
        label->set_parent(derived);
      }
    }
  }
  if (SgTemplateInstantiationDecl *decl = isSgTemplateInstantiationDecl(node)) {
    if (uint64_t target = singleEdgeTarget(record, "templateDeclaration")) {
      decl->set_templateDeclaration(
          nodeByIdAs<SgTemplateClassDeclaration>(nodes, target));
    }
    if (uint64_t target =
            singleEdgeTarget(record, "specializedTemplateDeclaration")) {
      decl->set_specializedTemplateDeclaration(
          nodeByIdAs<SgDeclarationStatement>(nodes, target));
    }
  }
  if (SgTemplateInstantiationTypedefDeclaration *decl =
          isSgTemplateInstantiationTypedefDeclaration(node)) {
    if (uint64_t target = singleEdgeTarget(record, "templateDeclaration")) {
      decl->set_templateDeclaration(
          nodeByIdAs<SgTemplateTypedefDeclaration>(nodes, target));
    }
    appendEdgeList<SgTemplateArgumentPtrList, SgTemplateArgument>(
        decl->get_templateArguments(), record, "templateArguments", nodes,
        decl);
    appendEdgeList<SgTemplateArgumentPtrList, SgTemplateArgument>(
        decl->get_deducedTemplateArguments(), record,
        "deducedTemplateArguments", nodes, decl);
    if (uint64_t target =
            singleEdgeTarget(record, "specializedTemplateDeclaration")) {
      decl->set_specializedTemplateDeclaration(
          nodeByIdAs<SgDeclarationStatement>(nodes, target));
    }
  }
  if (SgTemplateInstantiationFunctionDecl *decl =
          isSgTemplateInstantiationFunctionDecl(node)) {
    if (uint64_t target = singleEdgeTarget(record, "templateDeclaration")) {
      decl->set_templateDeclaration(
          nodeByIdAs<SgTemplateFunctionDeclaration>(nodes, target));
    }
    appendEdgeList<SgTemplateArgumentPtrList, SgTemplateArgument>(
        decl->get_templateArguments(), record, "templateArguments", nodes,
        decl);
    appendEdgeList<SgTemplateArgumentPtrList, SgTemplateArgument>(
        decl->get_deducedTemplateArguments(), record,
        "deducedTemplateArguments", nodes, decl);
    if (uint64_t target =
            singleEdgeTarget(record, "specializedTemplateDeclaration")) {
      decl->set_specializedTemplateDeclaration(
          nodeByIdAs<SgDeclarationStatement>(nodes, target));
    }
  }
  if (SgTemplateInstantiationMemberFunctionDecl *decl =
          isSgTemplateInstantiationMemberFunctionDecl(node)) {
    if (uint64_t target = singleEdgeTarget(record, "templateDeclaration")) {
      decl->set_templateDeclaration(
          nodeByIdAs<SgTemplateMemberFunctionDeclaration>(nodes, target));
    }
    appendEdgeList<SgTemplateArgumentPtrList, SgTemplateArgument>(
        decl->get_templateArguments(), record, "templateArguments", nodes,
        decl);
    appendEdgeList<SgTemplateArgumentPtrList, SgTemplateArgument>(
        decl->get_deducedTemplateArguments(), record,
        "deducedTemplateArguments", nodes, decl);
    if (uint64_t target =
            singleEdgeTarget(record, "specializedTemplateDeclaration")) {
      decl->set_specializedTemplateDeclaration(
          nodeByIdAs<SgDeclarationStatement>(nodes, target));
    }
  }
  if (SgNamespaceDeclarationStatement *decl =
          isSgNamespaceDeclarationStatement(node)) {
    if (uint64_t target = singleEdgeTarget(record, "definition")) {
      SgNamespaceDefinitionStatement *def =
          nodeByIdAs<SgNamespaceDefinitionStatement>(nodes, target);
      decl->set_definition(def);
      def->set_parent(decl);
      def->set_namespaceDeclaration(decl);
    }
  }
  if (SgNamespaceDefinitionStatement *def =
          isSgNamespaceDefinitionStatement(node)) {
    if (uint64_t target = singleEdgeTarget(record, "namespaceDeclaration")) {
      def->set_namespaceDeclaration(
          nodeByIdAs<SgNamespaceDeclarationStatement>(nodes, target));
    }
    if (uint64_t target =
            singleEdgeTarget(record, "previousNamespaceDefinition")) {
      def->set_previousNamespaceDefinition(
          nodeByIdAs<SgNamespaceDefinitionStatement>(nodes, target));
    }
    if (uint64_t target = singleEdgeTarget(record, "nextNamespaceDefinition")) {
      def->set_nextNamespaceDefinition(
          nodeByIdAs<SgNamespaceDefinitionStatement>(nodes, target));
    }
    if (uint64_t target = singleEdgeTarget(record, "global_definition")) {
      def->set_global_definition(
          nodeByIdAs<SgNamespaceDefinitionStatement>(nodes, target));
    }
  }
  if (SgFunctionDeclaration *decl = isSgFunctionDeclaration(node)) {
    if (uint64_t target = singleEdgeTarget(record, "parameterList")) {
      SgFunctionParameterList *params =
          nodeByIdAs<SgFunctionParameterList>(nodes, target);
      decl->set_parameterList(params);
      if (params->get_parent() == nullptr || params->get_parent() == decl) {
        params->set_parent(decl);
      }
    }
    if (uint64_t target = singleEdgeTarget(record, "parameterList_syntax")) {
      decl->set_parameterList_syntax(
          nodeByIdAs<SgFunctionParameterList>(nodes, target));
    }
    if (uint64_t target = singleEdgeTarget(record, "definition")) {
      SgFunctionDefinition *def =
          nodeByIdAs<SgFunctionDefinition>(nodes, target);
      decl->set_definition(def);
      def->set_parent(decl);
      def->set_declaration(decl);
    }
    if (uint64_t target = singleEdgeTarget(record, "functionParameterScope")) {
      decl->set_functionParameterScope(
          nodeByIdAs<SgFunctionParameterScope>(nodes, target));
    } else if (const JsonValue *external_scope = record.properties.find(
                   "external_function_parameter_scope")) {
      if (!external_scope->boolOr("present", false)) {
        throw std::runtime_error(
            "AST JSON " + record.kind +
            " external_function_parameter_scope is present but false");
      }
      const std::string source = record.properties.stringOr(
          "external_function_parameter_scope_source");
      SgFunctionDeclaration *peer = nullptr;
      if (source == "firstNondefiningDeclaration") {
        peer = isSgFunctionDeclaration(decl->get_firstNondefiningDeclaration());
      } else if (source == "definingDeclaration") {
        peer = isSgFunctionDeclaration(decl->get_definingDeclaration());
      } else {
        throw std::runtime_error(
            "AST JSON " + record.kind +
            " external_function_parameter_scope has unsupported source: " +
            source);
      }
      if (peer == nullptr || peer->get_functionParameterScope() == nullptr) {
        throw std::runtime_error(
            "AST JSON " + record.kind +
            " cannot restore external functionParameterScope from " + source);
      }
      decl->set_functionParameterScope(peer->get_functionParameterScope());
    }
  }
  if (SgMemberFunctionDeclaration *decl = isSgMemberFunctionDeclaration(node)) {
    if (uint64_t target = singleEdgeTarget(record, "CtorInitializerList")) {
      SgCtorInitializerList *ctors =
          nodeByIdAs<SgCtorInitializerList>(nodes, target);
      decl->set_CtorInitializerList(ctors);
      ctors->set_parent(decl);
    }
    if (uint64_t target =
            singleEdgeTarget(record, "associatedClassDeclaration")) {
      decl->set_associatedClassDeclaration(
          nodeByIdAs<SgDeclarationStatement>(nodes, target));
    }
  }
  if (SgFunctionDefinition *def = isSgFunctionDefinition(node)) {
    if (uint64_t target = singleEdgeTarget(record, "body")) {
      SgBasicBlock *body = nodeByIdAs<SgBasicBlock>(nodes, target);
      def->set_body(body);
      body->set_parent(def);
    }
  }
  if (SgPragmaDeclaration *decl = isSgPragmaDeclaration(node)) {
    if (uint64_t target = singleEdgeTarget(record, "pragma")) {
      SgPragma *pragma = nodeByIdAs<SgPragma>(nodes, target);
      decl->set_pragma(pragma);
    }
  }
  if (SgPragma *pragma = isSgPragma(node)) {
    if (uint64_t target = singleEdgeTarget(record, "parent")) {
      pragma->set_parent(nodeById(nodes, target));
    }
  }
  if (SgExprStatement *stmt = isSgExprStatement(node)) {
    if (uint64_t target = singleEdgeTarget(record, "expression")) {
      SgExpression *expr = nodeByIdAs<SgExpression>(nodes, target);
      stmt->set_expression(expr);
      expr->set_parent(stmt);
    }
  }
  if (SgReturnStmt *stmt = isSgReturnStmt(node)) {
    if (uint64_t target = singleEdgeTarget(record, "expression")) {
      SgExpression *expr = nodeByIdAs<SgExpression>(nodes, target);
      stmt->set_expression(expr);
      expr->set_parent(stmt);
    }
  }
  if (SgBreakStmt *stmt = isSgBreakStmt(node)) {
    stmt->set_do_string_label(record.properties.stringOr("do_string_label"));
  }
  if (SgContinueStmt *stmt = isSgContinueStmt(node)) {
    stmt->set_do_string_label(record.properties.stringOr("do_string_label"));
  }
  if (SgProcessControlStatement *stmt = isSgProcessControlStatement(node)) {
    stmt->set_control_kind(static_cast<SgProcessControlStatement::control_enum>(
        record.properties.intOr("control_kind",
                                SgProcessControlStatement::e_unknown)));
    if (uint64_t target = singleEdgeTarget(record, "code")) {
      SgExpression *expr = nodeByIdAs<SgExpression>(nodes, target);
      stmt->set_code(expr);
      expr->set_parent(stmt);
    }
    if (uint64_t target = singleEdgeTarget(record, "quiet")) {
      SgExpression *expr = nodeByIdAs<SgExpression>(nodes, target);
      stmt->set_quiet(expr);
      expr->set_parent(stmt);
    }
  }
  if (SgAllocateStatement *stmt = isSgAllocateStatement(node)) {
    if (uint64_t target = singleEdgeTarget(record, "expr_list")) {
      SgExprListExp *child = nodeByIdAs<SgExprListExp>(nodes, target);
      stmt->set_expr_list(child);
      child->set_parent(stmt);
    }
    auto set_expression =
        [&](const std::string &field,
            void (SgAllocateStatement::*setter)(SgExpression *)) {
          if (uint64_t target = singleEdgeTarget(record, field)) {
            SgExpression *child = nodeByIdAs<SgExpression>(nodes, target);
            (stmt->*setter)(child);
            child->set_parent(stmt);
          }
        };
    set_expression("stat_expression",
                   &SgAllocateStatement::set_stat_expression);
    set_expression("errmsg_expression",
                   &SgAllocateStatement::set_errmsg_expression);
    set_expression("source_expression",
                   &SgAllocateStatement::set_source_expression);
    set_expression("mold_expression",
                   &SgAllocateStatement::set_mold_expression);
    set_expression("stream_expression",
                   &SgAllocateStatement::set_stream_expression);
    set_expression("pinned_expression",
                   &SgAllocateStatement::set_pinned_expression);
  }
  if (SgDeallocateStatement *stmt = isSgDeallocateStatement(node)) {
    if (uint64_t target = singleEdgeTarget(record, "expr_list")) {
      SgExprListExp *child = nodeByIdAs<SgExprListExp>(nodes, target);
      stmt->set_expr_list(child);
      child->set_parent(stmt);
    }
    if (uint64_t target = singleEdgeTarget(record, "stat_expression")) {
      SgExpression *child = nodeByIdAs<SgExpression>(nodes, target);
      stmt->set_stat_expression(child);
      child->set_parent(stmt);
    }
    if (uint64_t target = singleEdgeTarget(record, "errmsg_expression")) {
      SgExpression *child = nodeByIdAs<SgExpression>(nodes, target);
      stmt->set_errmsg_expression(child);
      child->set_parent(stmt);
    }
  }
  if (SgNullifyStatement *stmt = isSgNullifyStatement(node)) {
    if (uint64_t target = singleEdgeTarget(record, "pointer_list")) {
      SgExprListExp *child = nodeByIdAs<SgExprListExp>(nodes, target);
      stmt->set_pointer_list(child);
      child->set_parent(stmt);
    }
  }
  if (SgIfStmt *stmt = isSgIfStmt(node)) {
    if (uint64_t target = singleEdgeTarget(record, "conditional")) {
      SgStatement *child = nodeByIdAs<SgStatement>(nodes, target);
      stmt->set_conditional(child);
      child->set_parent(stmt);
    }
    if (uint64_t target = singleEdgeTarget(record, "true_body")) {
      SgStatement *child = nodeByIdAs<SgStatement>(nodes, target);
      stmt->set_true_body(child);
      child->set_parent(stmt);
    }
    if (uint64_t target = singleEdgeTarget(record, "false_body")) {
      SgStatement *child = nodeByIdAs<SgStatement>(nodes, target);
      stmt->set_false_body(child);
      child->set_parent(stmt);
    }
    if (uint64_t target = singleEdgeTarget(record, "else_numeric_label")) {
      SgLabelRefExp *child = nodeByIdAs<SgLabelRefExp>(nodes, target);
      stmt->set_else_numeric_label(child);
      child->set_parent(stmt);
    }
    if (uint64_t target = singleEdgeTarget(record, "end_numeric_label")) {
      SgLabelRefExp *child = nodeByIdAs<SgLabelRefExp>(nodes, target);
      stmt->set_end_numeric_label(child);
      child->set_parent(stmt);
    }
  }
  if (SgForStatement *stmt = isSgForStatement(node)) {
    if (uint64_t target = singleEdgeTarget(record, "for_init_stmt")) {
      SgForInitStatement *child = nodeByIdAs<SgForInitStatement>(nodes, target);
      stmt->set_for_init_stmt(child);
      child->set_parent(stmt);
    }
    if (uint64_t target = singleEdgeTarget(record, "test")) {
      SgStatement *child = nodeByIdAs<SgStatement>(nodes, target);
      stmt->set_test(child);
      child->set_parent(stmt);
    }
    if (uint64_t target = singleEdgeTarget(record, "increment")) {
      SgExpression *child = nodeByIdAs<SgExpression>(nodes, target);
      stmt->set_increment(child);
      child->set_parent(stmt);
    }
    if (uint64_t target = singleEdgeTarget(record, "loop_body")) {
      SgStatement *child = nodeByIdAs<SgStatement>(nodes, target);
      stmt->set_loop_body(child);
      child->set_parent(stmt);
    }
  }
  if (SgGotoStatement *stmt = isSgGotoStatement(node)) {
    if (uint64_t target = singleEdgeTarget(record, "label")) {
      stmt->set_label(nodeByIdAs<SgLabelStatement>(nodes, target));
    }
    if (uint64_t target = singleEdgeTarget(record, "label_expression")) {
      SgLabelRefExp *label = nodeByIdAs<SgLabelRefExp>(nodes, target);
      stmt->set_label_expression(label);
      label->set_parent(stmt);
    }
    if (uint64_t target = singleEdgeTarget(record, "selector_expression")) {
      SgExpression *selector = nodeByIdAs<SgExpression>(nodes, target);
      stmt->set_selector_expression(selector);
      selector->set_parent(stmt);
    }
  }
  if (SgFortranDo *stmt = isSgFortranDo(node)) {
    if (uint64_t target = singleEdgeTarget(record, "initialization")) {
      SgExpression *child = nodeByIdAs<SgExpression>(nodes, target);
      stmt->set_initialization(child);
      child->set_parent(stmt);
    }
    if (uint64_t target = singleEdgeTarget(record, "bound")) {
      SgExpression *child = nodeByIdAs<SgExpression>(nodes, target);
      stmt->set_bound(child);
      child->set_parent(stmt);
    }
    if (uint64_t target = singleEdgeTarget(record, "increment")) {
      SgExpression *child = nodeByIdAs<SgExpression>(nodes, target);
      stmt->set_increment(child);
      child->set_parent(stmt);
    }
    if (uint64_t target = singleEdgeTarget(record, "body")) {
      SgBasicBlock *child = nodeByIdAs<SgBasicBlock>(nodes, target);
      stmt->set_body(child);
      child->set_parent(stmt);
    }
    if (uint64_t target = singleEdgeTarget(record, "end_numeric_label")) {
      SgLabelRefExp *child = nodeByIdAs<SgLabelRefExp>(nodes, target);
      stmt->set_end_numeric_label(child);
      child->set_parent(stmt);
    }
  }
  if (SgFortranNonblockedDo *stmt = isSgFortranNonblockedDo(node)) {
    if (uint64_t target = singleEdgeTarget(record, "end_statement")) {
      SgStatement *child = nodeByIdAs<SgStatement>(nodes, target);
      stmt->set_end_statement(child);
      child->set_parent(stmt);
    }
  }
  if (SgImpliedDo *expr = isSgImpliedDo(node)) {
    if (uint64_t target = singleEdgeTarget(record, "do_var_initialization")) {
      SgExpression *child = nodeByIdAs<SgExpression>(nodes, target);
      expr->set_do_var_initialization(child);
      child->set_parent(expr);
    }
    if (uint64_t target = singleEdgeTarget(record, "last_val")) {
      SgExpression *child = nodeByIdAs<SgExpression>(nodes, target);
      expr->set_last_val(child);
      child->set_parent(expr);
    }
    if (uint64_t target = singleEdgeTarget(record, "increment")) {
      SgExpression *child = nodeByIdAs<SgExpression>(nodes, target);
      expr->set_increment(child);
      child->set_parent(expr);
    }
    if (uint64_t target = singleEdgeTarget(record, "object_list")) {
      SgExprListExp *child = nodeByIdAs<SgExprListExp>(nodes, target);
      expr->set_object_list(child);
      child->set_parent(expr);
    }
    if (uint64_t target = singleEdgeTarget(record, "implied_do_scope")) {
      expr->set_implied_do_scope(nodeByIdAs<SgScopeStatement>(nodes, target));
    }
  }
  if (SgWhileStmt *stmt = isSgWhileStmt(node)) {
    if (uint64_t target = singleEdgeTarget(record, "condition")) {
      SgStatement *child = nodeByIdAs<SgStatement>(nodes, target);
      stmt->set_condition(child);
      child->set_parent(stmt);
    }
    if (uint64_t target = singleEdgeTarget(record, "body")) {
      SgStatement *child = nodeByIdAs<SgStatement>(nodes, target);
      stmt->set_body(child);
      child->set_parent(stmt);
    }
    if (uint64_t target = singleEdgeTarget(record, "end_numeric_label")) {
      SgLabelRefExp *child = nodeByIdAs<SgLabelRefExp>(nodes, target);
      stmt->set_end_numeric_label(child);
      child->set_parent(stmt);
    }
  }
  if (SgDoWhileStmt *stmt = isSgDoWhileStmt(node)) {
    if (uint64_t target = singleEdgeTarget(record, "condition")) {
      SgStatement *child = nodeByIdAs<SgStatement>(nodes, target);
      stmt->set_condition(child);
      child->set_parent(stmt);
    }
    if (uint64_t target = singleEdgeTarget(record, "body")) {
      SgStatement *child = nodeByIdAs<SgStatement>(nodes, target);
      stmt->set_body(child);
      child->set_parent(stmt);
    }
  }
  if (SgSwitchStatement *stmt = isSgSwitchStatement(node)) {
    if (uint64_t target = singleEdgeTarget(record, "item_selector")) {
      SgStatement *child = nodeByIdAs<SgStatement>(nodes, target);
      stmt->set_item_selector(child);
      child->set_parent(stmt);
    }
    if (uint64_t target = singleEdgeTarget(record, "body")) {
      SgBasicBlock *child = nodeByIdAs<SgBasicBlock>(nodes, target);
      stmt->set_body(child);
      child->set_parent(stmt);
    }
  }
  if (SgCaseOptionStmt *stmt = isSgCaseOptionStmt(node)) {
    stmt->set_case_construct_name(
        record.properties.stringOr("case_construct_name"));
    if (uint64_t target = singleEdgeTarget(record, "key")) {
      SgExpression *child = nodeByIdAs<SgExpression>(nodes, target);
      stmt->set_key(child);
      child->set_parent(stmt);
    }
    if (uint64_t target = singleEdgeTarget(record, "key_range_end")) {
      SgExpression *child = nodeByIdAs<SgExpression>(nodes, target);
      stmt->set_key_range_end(child);
      child->set_parent(stmt);
    }
    if (uint64_t target = singleEdgeTarget(record, "body")) {
      SgStatement *child = nodeByIdAs<SgStatement>(nodes, target);
      stmt->set_body(child);
      child->set_parent(stmt);
    }
  }
  if (SgDefaultOptionStmt *stmt = isSgDefaultOptionStmt(node)) {
    stmt->set_default_construct_name(
        record.properties.stringOr("default_construct_name"));
    if (uint64_t target = singleEdgeTarget(record, "body")) {
      SgStatement *child = nodeByIdAs<SgStatement>(nodes, target);
      stmt->set_body(child);
      child->set_parent(stmt);
    }
  }
  if (SgOmpBodyStatement *stmt = isSgOmpBodyStatement(node)) {
    if (uint64_t target = singleEdgeTarget(record, "body")) {
      SgStatement *body = nodeByIdAs<SgStatement>(nodes, target);
      stmt->set_body(body);
      body->set_parent(stmt);
    }
  }
  if (SgStaticAssertionDeclaration *decl =
          isSgStaticAssertionDeclaration(node)) {
    decl->set_string_literal(
        SgName(record.properties.stringOr("string_literal")));
    if (uint64_t target = singleEdgeTarget(record, "condition")) {
      SgExpression *condition = nodeByIdAs<SgExpression>(nodes, target);
      decl->set_condition(condition);
      condition->set_parent(decl);
    }
  }
  if (SgOmpExecStatement *stmt = isSgOmpExecStatement(node)) {
    if (uint64_t target = singleEdgeTarget(record, "omp_parent")) {
      stmt->set_omp_parent(nodeByIdAs<SgStatement>(nodes, target));
    }
    for (const EdgeRecord &edge : edgesFor(record, "omp_children")) {
      stmt->get_omp_children().push_back(
          nodeByIdAs<SgStatement>(nodes, edge.target));
    }
  }
  if (SgAssignInitializer *init = isSgAssignInitializer(node)) {
    if (uint64_t target = singleEdgeTarget(record, "operand_i")) {
      SgExpression *expr = nodeByIdAs<SgExpression>(nodes, target);
      init->set_operand_i(expr);
      expr->set_parent(init);
    }
  }
  if (SgAggregateInitializer *init = isSgAggregateInitializer(node)) {
    if (uint64_t target = singleEdgeTarget(record, "initializers")) {
      SgExprListExp *exprs = nodeByIdAs<SgExprListExp>(nodes, target);
      init->set_initializers(exprs);
      exprs->set_parent(init);
    }
  }
  if (SgBracedInitializer *init = isSgBracedInitializer(node)) {
    if (uint64_t target = singleEdgeTarget(record, "initializers")) {
      SgExprListExp *exprs = nodeByIdAs<SgExprListExp>(nodes, target);
      init->set_initializers(exprs);
      exprs->set_parent(init);
    }
  }
  if (SgFunctionCallExp *call = isSgFunctionCallExp(node)) {
    call->set_uses_operator_syntax(
        record.properties.boolOr("uses_operator_syntax", false));
    if (uint64_t target = singleEdgeTarget(record, "function")) {
      SgExpression *function = nodeByIdAs<SgExpression>(nodes, target);
      call->set_function(function);
      function->set_parent(call);
    }
    if (uint64_t target = singleEdgeTarget(record, "args")) {
      SgExprListExp *args = nodeByIdAs<SgExprListExp>(nodes, target);
      call->set_args(args);
      args->set_parent(call);
    }
  }
  if (SgActualArgumentExpression *actual = isSgActualArgumentExpression(node)) {
    actual->set_argument_name(
        SgName(record.properties.stringOr("argument_name")));
    if (uint64_t target = singleEdgeTarget(record, "expression")) {
      SgExpression *expr = nodeByIdAs<SgExpression>(nodes, target);
      if (SgExpression *old = actual->get_expression()) {
        if (old != expr) {
          old->set_parent(nullptr);
        }
      }
      actual->set_expression(expr);
      expr->set_parent(actual);
    }
  }
  if (SgExpression *expr = isSgExpression(node)) {
    if (uint64_t target = singleEdgeTarget(record, "originalExpressionTree")) {
      SgExpression *original = nodeByIdAs<SgExpression>(nodes, target);
      expr->set_originalExpressionTree(original);
    }
  }
  if (SgTypeTraitBuiltinOperator *op = isSgTypeTraitBuiltinOperator(node)) {
    op->get_args().clear();
    if (const JsonValue *args = record.properties.find("args")) {
      if (args->kind != JsonValue::Kind::Array) {
        throw std::runtime_error(
            "AST JSON SgTypeTraitBuiltinOperator args is not an array");
      }
      for (const JsonValue &entry : args->array) {
        SgNode *arg = nullptr;
        const std::string kind = entry.stringOr("kind");
        if (kind == "type") {
          arg = typeFromJson(entry.at("type"), nodes);
        } else if (kind == "node") {
          arg = nodeById(nodes, static_cast<uint64_t>(entry.intOr("node", 0)));
        } else {
          throw std::runtime_error(
              "AST JSON SgTypeTraitBuiltinOperator arg has unknown kind: " +
              kind);
        }
        if (arg == nullptr) {
          throw std::runtime_error(
              "AST JSON SgTypeTraitBuiltinOperator arg resolved to null");
        }
        op->get_args().push_back(arg);
        arg->set_parent(op);
      }
    }
  }
  if (SgUnaryOp *op = isSgUnaryOp(node)) {
    op->set_mode(static_cast<SgUnaryOp::Sgop_mode>(
        record.properties.intOr("mode", SgUnaryOp::prefix)));
    if (uint64_t target = singleEdgeTarget(record, "operand_i")) {
      SgExpression *operand = nodeByIdAs<SgExpression>(nodes, target);
      op->set_operand_i(operand);
      operand->set_parent(op);
    }
  }
  if (SgBinaryOp *op = isSgBinaryOp(node)) {
    if (uint64_t target = singleEdgeTarget(record, "lhs_operand_i")) {
      SgExpression *lhs = nodeByIdAs<SgExpression>(nodes, target);
      op->set_lhs_operand_i(lhs);
      lhs->set_parent(op);
    }
    if (uint64_t target = singleEdgeTarget(record, "rhs_operand_i")) {
      SgExpression *rhs = nodeByIdAs<SgExpression>(nodes, target);
      op->set_rhs_operand_i(rhs);
      rhs->set_parent(op);
    }
  }
  if (SgComplexVal *value = isSgComplexVal(node)) {
    if (const JsonValue *type = record.properties.find("precision_type")) {
      value->set_precisionType(typeFromJson(*type, nodes));
    }
    if (uint64_t target = singleEdgeTarget(record, "real_value")) {
      SgValueExp *real = nodeByIdAs<SgValueExp>(nodes, target);
      value->set_real_value(real);
      real->set_parent(value);
    } else if (const JsonValue *real_json =
                   record.properties.find("real_value")) {
      SgExpression *real = expressionFromRef(*real_json, nodes);
      if (real != nullptr && isSgValueExp(real) == nullptr) {
        throw std::runtime_error(
            "AST JSON SgComplexVal real_value is not a SgValueExp");
      }
      value->set_real_value(isSgValueExp(real));
      if (real != nullptr) {
        real->set_parent(value);
      }
    }
    if (uint64_t target = singleEdgeTarget(record, "imaginary_value")) {
      SgValueExp *imag = nodeByIdAs<SgValueExp>(nodes, target);
      value->set_imaginary_value(imag);
      imag->set_parent(value);
    } else if (const JsonValue *imag_json =
                   record.properties.find("imaginary_value")) {
      SgExpression *imag = expressionFromRef(*imag_json, nodes);
      if (imag != nullptr && isSgValueExp(imag) == nullptr) {
        throw std::runtime_error(
            "AST JSON SgComplexVal imaginary_value is not a SgValueExp");
      }
      value->set_imaginary_value(isSgValueExp(imag));
      if (imag != nullptr) {
        imag->set_parent(value);
      }
    }
    value->set_valueString(record.properties.stringOr("value_string"));
  }
  if (SgSubscriptExpression *expr = isSgSubscriptExpression(node)) {
    if (uint64_t target = singleEdgeTarget(record, "lowerBound")) {
      SgExpression *child = nodeByIdAs<SgExpression>(nodes, target);
      expr->set_lowerBound(child);
      child->set_parent(expr);
    }
    if (uint64_t target = singleEdgeTarget(record, "upperBound")) {
      SgExpression *child = nodeByIdAs<SgExpression>(nodes, target);
      expr->set_upperBound(child);
      child->set_parent(expr);
    }
    if (uint64_t target = singleEdgeTarget(record, "stride")) {
      SgExpression *child = nodeByIdAs<SgExpression>(nodes, target);
      expr->set_stride(child);
      child->set_parent(expr);
    }
  }
  if (SgSizeOfOp *op = isSgSizeOfOp(node)) {
    op->set_sizeOfContainsBaseTypeDefiningDeclaration(record.properties.boolOr(
        "size_of_contains_base_type_defining_declaration", false));
    op->set_is_objectless_nonstatic_data_member_reference(
        record.properties.boolOr(
            "is_objectless_nonstatic_data_member_reference", false));
    op->set_is_sizeof_pack(record.properties.boolOr("is_sizeof_pack", false));
    if (const JsonValue *type = record.properties.find("operand_type")) {
      if (type->boolOr("present", false)) {
        op->set_operand_type(typeFromJson(*type, nodes));
      } else {
        op->set_operand_type(nullptr);
      }
    }
    if (uint64_t target = singleEdgeTarget(record, "operand_expr")) {
      SgExpression *operand = nodeByIdAs<SgExpression>(nodes, target);
      op->set_operand_expr(operand);
      operand->set_parent(op);
    }
  }
  if (SgConditionalExp *expr = isSgConditionalExp(node)) {
    if (uint64_t target = singleEdgeTarget(record, "conditional_exp")) {
      SgExpression *child = nodeByIdAs<SgExpression>(nodes, target);
      expr->set_conditional_exp(child);
      child->set_parent(expr);
    }
    if (uint64_t target = singleEdgeTarget(record, "true_exp")) {
      SgExpression *child = nodeByIdAs<SgExpression>(nodes, target);
      expr->set_true_exp(child);
      child->set_parent(expr);
    }
    if (uint64_t target = singleEdgeTarget(record, "false_exp")) {
      SgExpression *child = nodeByIdAs<SgExpression>(nodes, target);
      expr->set_false_exp(child);
      child->set_parent(expr);
    }
  }
  if (SgStatementExpression *expr = isSgStatementExpression(node)) {
    if (uint64_t target = singleEdgeTarget(record, "statement")) {
      SgStatement *stmt = nodeByIdAs<SgStatement>(nodes, target);
      expr->set_statement(stmt);
      stmt->set_parent(expr);
    }
  }
  if (SgConstructorInitializer *init = isSgConstructorInitializer(node)) {
    if (uint64_t target = singleEdgeTarget(record, "declaration")) {
      init->set_declaration(
          nodeByIdAs<SgMemberFunctionDeclaration>(nodes, target));
    }
    if (uint64_t target = singleEdgeTarget(record, "args")) {
      SgExprListExp *args = nodeByIdAs<SgExprListExp>(nodes, target);
      init->set_args(args);
      args->set_parent(init);
    }
  }
  if (SgOmpVariablesClause *clause = isSgOmpVariablesClause(node)) {
    if (uint64_t target = singleEdgeTarget(record, "variables")) {
      SgExprListExp *vars = nodeByIdAs<SgExprListExp>(nodes, target);
      clause->set_variables(vars);
      vars->set_parent(clause);
    }
  }
  if (SgOmpExclusiveClause *clause = isSgOmpExclusiveClause(node)) {
    if (uint64_t target = singleEdgeTarget(record, "variables")) {
      SgExprListExp *vars = nodeByIdAs<SgExprListExp>(nodes, target);
      clause->set_variables(vars);
      vars->set_parent(clause);
    }
  }
  if (SgOmpExpressionClause *clause = isSgOmpExpressionClause(node)) {
    if (uint64_t target = singleEdgeTarget(record, "expression")) {
      SgExpression *expr = nodeByIdAs<SgExpression>(nodes, target);
      clause->set_expression(expr);
      expr->set_parent(clause);
    }
  }
  if (SgOmpDefaultClause *clause = isSgOmpDefaultClause(node)) {
    if (uint64_t target = singleEdgeTarget(record, "variant_directive")) {
      SgStatement *stmt = nodeByIdAs<SgStatement>(nodes, target);
      clause->set_variant_directive(stmt);
      stmt->set_parent(clause);
    }
  }
  if (SgOmpReductionClause *clause = isSgOmpReductionClause(node)) {
    if (uint64_t target = singleEdgeTarget(record, "user_defined_identifier")) {
      SgExpression *expr = nodeByIdAs<SgExpression>(nodes, target);
      clause->set_user_defined_identifier(expr);
      expr->set_parent(clause);
    } else if (const JsonValue *identifier =
                   record.properties.find("user_defined_identifier")) {
      if (SgExpression *expr = expressionFromRef(*identifier, nodes)) {
        clause->set_user_defined_identifier(expr);
        expr->set_parent(clause);
      }
    }
  }
  if (SgOmpLinearClause *clause = isSgOmpLinearClause(node)) {
    if (uint64_t target = singleEdgeTarget(record, "step")) {
      SgExpression *expr = nodeByIdAs<SgExpression>(nodes, target);
      clause->set_step(expr);
      expr->set_parent(clause);
    }
  }
  if (SgOmpAlignedClause *clause = isSgOmpAlignedClause(node)) {
    if (uint64_t target = singleEdgeTarget(record, "alignment")) {
      SgExpression *expr = nodeByIdAs<SgExpression>(nodes, target);
      clause->set_alignment(expr);
      expr->set_parent(clause);
    }
  }
  if (SgOmpScheduleClause *clause = isSgOmpScheduleClause(node)) {
    if (uint64_t target = singleEdgeTarget(record, "chunk_size")) {
      SgExpression *expr = nodeByIdAs<SgExpression>(nodes, target);
      clause->set_chunk_size(expr);
      expr->set_parent(clause);
    }
  }
  if (SgOmpDistScheduleClause *clause = isSgOmpDistScheduleClause(node)) {
    if (uint64_t target = singleEdgeTarget(record, "chunk_size")) {
      SgExpression *expr = nodeByIdAs<SgExpression>(nodes, target);
      clause->set_chunk_size(expr);
      expr->set_parent(clause);
    }
  }
  if (SgOmpFlushStatement *stmt = isSgOmpFlushStatement(node)) {
    appendEdgeList<SgExpressionPtrList, SgExpression>(
        stmt->get_variables(), record, "variables", nodes, stmt);
  }
  if (SgOmpThreadprivateStatement *stmt = isSgOmpThreadprivateStatement(node)) {
    appendEdgeList<SgExpressionPtrList, SgExpression>(
        stmt->get_variables(), record, "variables", nodes, stmt);
  }
  if (SgOmpAllocateStatement *stmt = isSgOmpAllocateStatement(node)) {
    appendEdgeList<SgExpressionPtrList, SgExpression>(
        stmt->get_variables(), record, "variables", nodes, stmt);
  }
  if (SgOmpExtImplementationDefinedRequirementClause *clause =
          isSgOmpExtImplementationDefinedRequirementClause(node)) {
    if (uint64_t target =
            singleEdgeTarget(record, "implementation_defined_requirement")) {
      SgExpression *expr = nodeByIdAs<SgExpression>(nodes, target);
      clause->set_implementation_defined_requirement(expr);
      expr->set_parent(clause);
    }
  }
  if (SgOmpAllocateClause *clause = isSgOmpAllocateClause(node)) {
    if (uint64_t target = singleEdgeTarget(record, "user_defined_modifier")) {
      SgExpression *expr = nodeByIdAs<SgExpression>(nodes, target);
      clause->set_user_defined_modifier(expr);
      expr->set_parent(clause);
    } else if (const JsonValue *modifier =
                   record.properties.find("user_defined_modifier")) {
      if (SgExpression *expr = expressionFromRef(*modifier, nodes)) {
        clause->set_user_defined_modifier(expr);
        expr->set_parent(clause);
      }
    }
  }
  if (SgOmpAllocatorClause *clause = isSgOmpAllocatorClause(node)) {
    if (uint64_t target = singleEdgeTarget(record, "user_defined_modifier")) {
      SgExpression *expr = nodeByIdAs<SgExpression>(nodes, target);
      clause->set_user_defined_modifier(expr);
      expr->set_parent(clause);
    } else if (const JsonValue *modifier =
                   record.properties.find("user_defined_modifier")) {
      if (SgExpression *expr = expressionFromRef(*modifier, nodes)) {
        clause->set_user_defined_modifier(expr);
        expr->set_parent(clause);
      }
    }
  }
  if (SgOmpAdjustArgsClause *clause = isSgOmpAdjustArgsClause(node)) {
    if (uint64_t target = singleEdgeTarget(record, "arguments")) {
      SgExprListExp *arguments = nodeByIdAs<SgExprListExp>(nodes, target);
      clause->set_arguments(arguments);
      arguments->set_parent(clause);
    }
    if (uint64_t target = singleEdgeTarget(record, "user_defined_modifier")) {
      SgExpression *expr = nodeByIdAs<SgExpression>(nodes, target);
      clause->set_user_defined_modifier(expr);
      expr->set_parent(clause);
    } else if (const JsonValue *modifier =
                   record.properties.find("user_defined_modifier")) {
      if (SgExpression *expr = expressionFromRef(*modifier, nodes)) {
        clause->set_user_defined_modifier(expr);
        expr->set_parent(clause);
      }
    }
  }
  if (SgOmpInReductionClause *clause = isSgOmpInReductionClause(node)) {
    if (uint64_t target = singleEdgeTarget(record, "user_defined_identifier")) {
      SgExpression *expr = nodeByIdAs<SgExpression>(nodes, target);
      clause->set_user_defined_identifier(expr);
      expr->set_parent(clause);
    }
  }
  if (SgOmpTaskReductionClause *clause = isSgOmpTaskReductionClause(node)) {
    if (uint64_t target = singleEdgeTarget(record, "user_defined_identifier")) {
      SgExpression *expr = nodeByIdAs<SgExpression>(nodes, target);
      clause->set_user_defined_identifier(expr);
      expr->set_parent(clause);
    } else if (const JsonValue *identifier =
                   record.properties.find("user_defined_identifier")) {
      if (SgExpression *expr = expressionFromRef(*identifier, nodes)) {
        clause->set_user_defined_identifier(expr);
        expr->set_parent(clause);
      }
    }
  }
  if (SgOmpMapClause *clause = isSgOmpMapClause(node)) {
    if (uint64_t target = singleEdgeTarget(record, "mapper_identifier")) {
      SgExpression *expr = nodeByIdAs<SgExpression>(nodes, target);
      clause->set_mapper_identifier(expr);
      expr->set_parent(clause);
    }
  }
  if (SgOmpToClause *clause = isSgOmpToClause(node)) {
    if (uint64_t target = singleEdgeTarget(record, "mapper_identifier")) {
      SgExpression *expr = nodeByIdAs<SgExpression>(nodes, target);
      clause->set_mapper_identifier(expr);
      expr->set_parent(clause);
    }
  }
  if (SgOmpFromClause *clause = isSgOmpFromClause(node)) {
    if (uint64_t target = singleEdgeTarget(record, "mapper_identifier")) {
      SgExpression *expr = nodeByIdAs<SgExpression>(nodes, target);
      clause->set_mapper_identifier(expr);
      expr->set_parent(clause);
    }
  }
  if (SgOmpWhenClause *clause = isSgOmpWhenClause(node)) {
    auto set_expression = [&](const std::string &field,
                              void (SgOmpWhenClause::*setter)(SgExpression *)) {
      if (uint64_t target = singleEdgeTarget(record, field)) {
        SgExpression *expr = nodeByIdAs<SgExpression>(nodes, target);
        (clause->*setter)(expr);
        expr->set_parent(clause);
      } else if (const JsonValue *value = record.properties.find(field)) {
        if (SgExpression *expr = expressionFromRef(*value, nodes)) {
          (clause->*setter)(expr);
          expr->set_parent(clause);
        }
      }
    };
    set_expression("user_condition", &SgOmpWhenClause::set_user_condition);
    set_expression("user_condition_score",
                   &SgOmpWhenClause::set_user_condition_score);
    set_expression("device_arch", &SgOmpWhenClause::set_device_arch);
    set_expression("device_isa", &SgOmpWhenClause::set_device_isa);
    set_expression("device_num", &SgOmpWhenClause::set_device_num);
    set_expression("implementation_user_defined",
                   &SgOmpWhenClause::set_implementation_user_defined);
    set_expression("implementation_extension",
                   &SgOmpWhenClause::set_implementation_extension);
    if (uint64_t target = singleEdgeTarget(record, "variant_directive")) {
      SgStatement *stmt = nodeByIdAs<SgStatement>(nodes, target);
      clause->set_variant_directive(stmt);
      stmt->set_parent(clause);
    }
    appendEdgeList<SgStatementPtrList, SgStatement>(
        clause->get_construct_directives(), record, "construct_directives",
        nodes, clause);
  }
  if (SgOmpMatchClause *clause = isSgOmpMatchClause(node)) {
    auto set_expression =
        [&](const std::string &field,
            void (SgOmpMatchClause::*setter)(SgExpression *)) {
          if (uint64_t target = singleEdgeTarget(record, field)) {
            SgExpression *expr = nodeByIdAs<SgExpression>(nodes, target);
            (clause->*setter)(expr);
            expr->set_parent(clause);
          } else if (const JsonValue *value = record.properties.find(field)) {
            if (SgExpression *expr = expressionFromRef(*value, nodes)) {
              (clause->*setter)(expr);
              expr->set_parent(clause);
            }
          }
        };
    set_expression("user_condition", &SgOmpMatchClause::set_user_condition);
    set_expression("user_condition_score",
                   &SgOmpMatchClause::set_user_condition_score);
    set_expression("device_arch", &SgOmpMatchClause::set_device_arch);
    set_expression("device_isa", &SgOmpMatchClause::set_device_isa);
    set_expression("device_num", &SgOmpMatchClause::set_device_num);
    set_expression("implementation_user_defined",
                   &SgOmpMatchClause::set_implementation_user_defined);
    set_expression("implementation_extension",
                   &SgOmpMatchClause::set_implementation_extension);
    clause->set_target_device_selector(
        record.properties.boolOr("target_device_selector", false));
    appendEdgeList<SgStatementPtrList, SgStatement>(
        clause->get_construct_directives(), record, "construct_directives",
        nodes, clause);
  }
  if (SgOmpUsesAllocatorsDefination *definition =
          isSgOmpUsesAllocatorsDefination(node)) {
    if (uint64_t target = singleEdgeTarget(record, "user_defined_allocator")) {
      SgExpression *expr = nodeByIdAs<SgExpression>(nodes, target);
      definition->set_user_defined_allocator(expr);
      expr->set_parent(definition);
    }
    if (uint64_t target = singleEdgeTarget(record, "allocator_traits_array")) {
      SgExpression *expr = nodeByIdAs<SgExpression>(nodes, target);
      definition->set_allocator_traits_array(expr);
      expr->set_parent(definition);
    }
  }
  if (SgNewExp *expr = isSgNewExp(node)) {
    if (uint64_t target = singleEdgeTarget(record, "placement_args")) {
      SgExprListExp *args = nodeByIdAs<SgExprListExp>(nodes, target);
      expr->set_placement_args(args);
      args->set_parent(expr);
    }
    if (uint64_t target = singleEdgeTarget(record, "constructor_args")) {
      SgConstructorInitializer *args =
          nodeByIdAs<SgConstructorInitializer>(nodes, target);
      expr->set_constructor_args(args);
      args->set_parent(expr);
    }
    if (uint64_t target = singleEdgeTarget(record, "builtin_args")) {
      SgExpression *args = nodeByIdAs<SgExpression>(nodes, target);
      expr->set_builtin_args(args);
      args->set_parent(expr);
    }
    if (uint64_t target = singleEdgeTarget(record, "newOperatorDeclaration")) {
      expr->set_newOperatorDeclaration(
          nodeByIdAs<SgFunctionDeclaration>(nodes, target));
    }
  }
  if (SgDeleteExp *expr = isSgDeleteExp(node)) {
    if (uint64_t target = singleEdgeTarget(record, "variable")) {
      SgExpression *variable = nodeByIdAs<SgExpression>(nodes, target);
      expr->set_variable(variable);
      variable->set_parent(expr);
    }
    if (uint64_t target =
            singleEdgeTarget(record, "deleteOperatorDeclaration")) {
      expr->set_deleteOperatorDeclaration(
          nodeByIdAs<SgFunctionDeclaration>(nodes, target));
    }
  }
  if (SgPackExpansionExpr *expr = isSgPackExpansionExpr(node)) {
    if (uint64_t target = singleEdgeTarget(record, "pattern_expression")) {
      SgExpression *pattern = nodeByIdAs<SgExpression>(nodes, target);
      expr->set_pattern_expression(pattern);
      pattern->set_parent(expr);
    }
  }
  if (SgNonrealRefExp *expr = isSgNonrealRefExp(node)) {
    appendEdgeList<SgTemplateArgumentPtrList, SgTemplateArgument>(
        expr->get_templateArguments(), record, "templateArguments", nodes,
        expr);
  }
}

SgScopeStatement *nearestScope(SgNode *node) {
  for (SgNode *current = node; current != nullptr;
       current = current->get_parent()) {
    if (SgScopeStatement *scope = isSgScopeStatement(current)) {
      return scope;
    }
  }
  return nullptr;
}

SgSymbol *createSymbolForKindAndBasis(const std::string &kind, SgNode *basis);
SgSymbol *createExternalSymbolFromJson(const JsonValue &json,
                                       const NodeMap &nodes);

} // namespace AstJson
} // namespace Rose
