
#ifndef _CLANG_FRONTEND_PRIVATE_HPP_
#define _CLANG_FRONTEND_PRIVATE_HPP_

#include "clang-frontend.hpp"

#include "sage3basic.h"

#include <algorithm>
#include <iostream>
#include <memory>
#include <set>
#include <string>
#include <vector>

static inline SgSymbolTable &get_orphan_symbol_table() {
  static std::unique_ptr<SgSymbolTable> orphan_table;
  if (!orphan_table) {
    orphan_table.reset(new SgSymbolTable());
  }
  return *orphan_table;
}

static inline void move_symbol_to_orphan_table(SgSymbol *symbol) {
  if (symbol == nullptr) {
    return;
  }
  if (SgSymbolTable *parent_table = isSgSymbolTable(symbol->get_parent())) {
    if (parent_table->exists(symbol)) {
      return;
    }
  }

  // Keep detached symbols alive without reintroducing them into a scope.
  get_orphan_symbol_table().insert(symbol->get_name(), symbol);
}

class MissingTemplateHeaderFixupAttribute : public AstAttribute {
public:
  MissingTemplateHeaderFixupAttribute(std::string file,
                                      std::vector<unsigned> offsets)
      : file_(std::move(file)), offsets_(std::move(offsets)) {
    std::sort(offsets_.begin(), offsets_.end());
    offsets_.erase(std::unique(offsets_.begin(), offsets_.end()),
                   offsets_.end());
  }

  bool matches(const std::string &file, unsigned offset) const {
    if (!file_.empty() && file != file_) {
      return false;
    }
    return std::binary_search(offsets_.begin(), offsets_.end(), offset);
  }

  const std::string &file() const { return file_; }

private:
  std::string file_;
  std::vector<unsigned> offsets_;
};

inline constexpr char kMissingTemplateHeaderFixupAttributeName[] =
    "rex_missing_template_header_fixups";

#include <clang/AST/AST.h>

#include <clang/AST/ASTConsumer.h>

#include <clang/AST/ASTContext.h>

#include <clang/AST/Decl.h>

#include <clang/AST/DeclCXX.h>

#include <clang/AST/DeclFriend.h>

#include <clang/AST/DeclGroup.h>

#include <clang/AST/DeclObjC.h>

#include <clang/AST/DeclTemplate.h>

#include <clang/AST/DeclVisitor.h>

#include <clang/AST/DeclarationName.h>

#include <clang/AST/Expr.h>

#include <clang/AST/ExprCXX.h>

#include <clang/AST/ExprObjC.h>

#include <clang/AST/NestedNameSpecifier.h>

#include <clang/AST/ParentMap.h>

#include <clang/AST/RecursiveASTVisitor.h>

#include <clang/AST/Stmt.h>

#include <clang/AST/StmtCXX.h>

#include <clang/AST/StmtObjC.h>

#include <clang/AST/StmtVisitor.h>

#include <clang/AST/TemplateBase.h>

#include <clang/AST/TemplateName.h>

#include <clang/AST/Type.h>

#include <clang/AST/TypeLoc.h>

#include <clang/AST/TypeLocVisitor.h>

#include <clang/Basic/Builtins.h>

#include <clang/Basic/Diagnostic.h>

#include <clang/Basic/FileManager.h>

#include <clang/Basic/IdentifierTable.h>

#include <clang/Basic/LangOptions.h>

#include <clang/Basic/LangStandard.h>

#include <clang/Basic/SourceLocation.h>

#include <clang/Basic/SourceManager.h>

#include <clang/Basic/TargetInfo.h>

#include <clang/Basic/TargetOptions.h>

#include <clang/Basic/DiagnosticOptions.h>

#include <clang/Frontend/CompilerInstance.h>

#include <clang/Frontend/CompilerInvocation.h>

#include <clang/Frontend/FrontendOptions.h>

#include <clang/Lex/PreprocessorOptions.h>

#include <clang/Frontend/TextDiagnosticBuffer.h>

#include <clang/Frontend/TextDiagnosticPrinter.h>

#include <clang/FrontendTool/Utils.h>

#include <clang/Lex/HeaderSearch.h>

#include <clang/Lex/PPCallbacks.h>

#include <clang/Lex/Pragma.h>

#include <clang/Lex/Preprocessor.h>

#include <clang/Parse/ParseAST.h>

#include <clang/Sema/Sema.h>

#include <llvm/ADT/IntrusiveRefCntPtr.h>

#include <llvm/ADT/StringRef.h>

#include <llvm/Config/llvm-config.h>

#include <llvm/Frontend/OpenMP/OMPIRBuilder.h>

#include <llvm/Support/raw_os_ostream.h>

#include <llvm/Support/raw_ostream.h>

#include <llvm/TargetParser/Host.h>

#include <llvm/TargetParser/Triple.h>
// DQ (11/27/2020): Turn on/off the debugging information as we visit clang IR
// nodes.
#define DEBUG_VISITOR 0
#define DEBUG_TRAVERSAL 0
#define DEBUG_SOURCE_LOCATION 0
#define DEBUG_SYMBOL_TABLE_LOOKUP 0
#define DEBUG_ARGS 0
#define DEBUG_TRAVERSE_DECL 0
#define DEBUG_VISIT_STMT 0
#define DEBUG_IMPLICIT_NODE 0
#define DEBUG_IMPLICIT_NODE 0

// Print visitor name when visiting a node inheritance hierarchy
#ifdef DEBUG_VISITOR
#ifndef DEBUG_VISIT_STMT
#define DEBUG_VISIT_STMT DEBUG_VISITOR
#endif
#ifndef DEBUG_VISIT_DECL
#define DEBUG_VISIT_DECL DEBUG_VISITOR
#endif
#ifndef DEBUG_VISIT_TYPE
#define DEBUG_VISIT_TYPE DEBUG_VISITOR
#endif
#else
#define DEBUG_VISITOR 0
#define DEBUG_VISIT_STMT 0
#define DEBUG_VISIT_DECL 0
#define DEBUG_VISIT_TYPE 0
#endif

// Print results of traversal of the nodes: "already done?" and "generated Sage
// ptr"
#ifdef DEBUG_TRAVERSAL
#ifndef DEBUG_TRAVERSE_STMT
#define DEBUG_TRAVERSE_STMT DEBUG_TRAVERSAL
#endif
#ifndef DEBUG_TRAVERSE_DECL
#define DEBUG_TRAVERSE_DECL DEBUG_TRAVERSAL
#endif
#ifndef DEBUG_TRAVERSE_TYPE
#define DEBUG_TRAVERSE_TYPE DEBUG_TRAVERSAL
#endif
#else
#define DEBUG_TRAVERSAL 0
#define DEBUG_TRAVERSE_STMT 0
#define DEBUG_TRAVERSE_DECL 0
#define DEBUG_TRAVERSE_TYPE 0
#endif

// Print debug info when attaching source location
#ifndef DEBUG_SOURCE_LOCATION
#define DEBUG_SOURCE_LOCATION 0
#endif

// Display symbol lookup process
#ifndef DEBUG_SYMBOL_TABLE_LOOKUP
#define DEBUG_SYMBOL_TABLE_LOOKUP 0
#endif

// Print args receive by clang_main
#ifndef DEBUG_ARGS
#define DEBUG_ARGS 0
#endif

// Fail when a FIXME is reach
#ifndef FAIL_FIXME
#define FAIL_FIXME 0
#endif

// Fail when a TODO is reach
#ifndef FAIL_TODO
#define FAIL_TODO 1
#endif

// PP Callbacks to capture pragmas before Clang processes them
class RoseOpenMPPragmaCallback : public clang::PPCallbacks {
private:
  // Use (FileID, line) pair as key to handle pragmas from multiple files
  // correctly
  std::map<std::pair<clang::FileID, unsigned>, std::string> line_to_pragma;
  std::set<std::pair<clang::FileID, unsigned>> openmp_lines;
  std::set<std::pair<clang::FileID, unsigned>> pragma_continuation_lines;
  clang::SourceManager &p_source_manager;
  clang::Preprocessor &p_preprocessor;

  static bool isOpenMPPragmaText(const std::string &text) {
    size_t pos = 0;
    auto skipWS = [](const std::string &s, size_t p) {
      while (p < s.size() && (s[p] == ' ' || s[p] == '\t'))
        ++p;
      return p;
    };
    // Expect leading '#'
    if (pos >= text.size() || text[pos] != '#')
      return false;
    pos = skipWS(text, pos + 1);
    // "pragma"
    if (pos + 6 > text.size() || text.compare(pos, 6, "pragma") != 0)
      return false;
    pos = skipWS(text, pos + 6);
    // "omp"
    return (pos + 3 <= text.size() && text.compare(pos, 3, "omp") == 0);
  }

public:
  // Helper function to check if character is whitespace (space or tab)
  static bool isWhitespace(char c) { return c == ' ' || c == '\t'; }

  // Helper to skip whitespace in a string
  static size_t skipWhitespace(const std::string &str, size_t pos) {
    while (pos < str.size() && isWhitespace(str[pos])) {
      ++pos;
    }
    return pos;
  }
  RoseOpenMPPragmaCallback(clang::SourceManager &SM, clang::Preprocessor &PP)
      : p_source_manager(SM), p_preprocessor(PP) {}

  void PragmaDirective(clang::SourceLocation Loc,
                       clang::PragmaIntroducerKind Introducer) override {
    // Get the FileID and starting line number where the pragma appears
    clang::FileID file_id = p_source_manager.getFileID(Loc);
    unsigned line = p_source_manager.getPresumedLineNumber(Loc);

    // Capture the full pragma text, including any backslash-continued lines
    const char *current = p_source_manager.getCharacterData(Loc);
    std::string original_text;
    unsigned line_count = 1;

    while (current != nullptr) {
      const char *line_end = current;
      while (*line_end != '\n' && *line_end != '\r' && *line_end != '\0') {
        ++line_end;
      }

      // Check for continuation BEFORE appending to properly normalize the text
      const char *back = line_end;
      while (back > current && isWhitespace(*(back - 1))) {
        --back;
      }
      bool has_continuation = (back > current && *(back - 1) == '\\');

      if (has_continuation) {
        // For continuation lines: append up to (but not including) the
        // backslash First, skip any whitespace before the backslash
        const char *effective_end =
            back - 1; // -1 to exclude the backslash itself
        while (effective_end > current && isWhitespace(*(effective_end - 1))) {
          --effective_end;
        }
        original_text.append(current, effective_end - current);
        // Add a single space to separate tokens from the next line
        original_text.push_back(' ');
      } else {
        // For non-continuation lines: append the entire line as-is
        original_text.append(current, line_end - current);
      }

      if (*line_end == '\0') {
        break;
      }

      // Move to the next line
      if (*line_end == '\r' && *(line_end + 1) == '\n') {
        current = line_end + 2;
      } else {
        current = line_end + 1;
      }

      if (!has_continuation) {
        // Trim trailing whitespace from the stored directive
        while (!original_text.empty() &&
               isspace(static_cast<unsigned char>(original_text.back()))) {
          original_text.pop_back();
        }
        break;
      }

      ++line_count;
    }

    // Check if this is an OMP pragma - handle multiple spaces
    // Pattern: # <spaces> pragma <spaces> omp <rest>
    size_t pos = 0;

    // Check for '#'
    if (pos >= original_text.size() || original_text[pos] != '#') {
      return;
    }
    ++pos;

    // Skip whitespace after '#'
    pos = skipWhitespace(original_text, pos);

    // Check for "pragma"
    if (original_text.compare(pos, 6, "pragma") != 0) {
      return;
    }
    pos += 6;

    // Must have at least one whitespace after "pragma"
    if (pos >= original_text.size() || !isWhitespace(original_text[pos])) {
      return;
    }
    pos = skipWhitespace(original_text, pos);

    // Store with (FileID, line) key to handle multi-file TUs
    line_to_pragma[std::make_pair(file_id, line)] = original_text;
    if (isOpenMPPragmaText(original_text)) {
      openmp_lines.insert(std::make_pair(file_id, line));
    }
    for (unsigned offset = 1; offset < line_count; ++offset) {
      pragma_continuation_lines.insert(std::make_pair(file_id, line + offset));
    }
  }

  // Lookup pragma by (FileID, line) - returns true if found, false otherwise
  // Passes result by reference to avoid ABI issues with std::string returns
  bool getPragmaAtLine(clang::FileID file_id, unsigned line,
                       std::string &result) const {
    auto it = line_to_pragma.find(std::make_pair(file_id, line));
    if (it != line_to_pragma.end()) {
      result = it->second;
      return true;
    }
    return false;
  }

  size_t getCount() const { return line_to_pragma.size(); }

  bool isOpenMPPragmaAtLine(clang::FileID file_id, unsigned line) const {
    return openmp_lines.count(std::make_pair(file_id, line)) > 0;
  }

  bool isContinuationLine(clang::FileID file_id, unsigned line) const {
    return pragma_continuation_lines.count(std::make_pair(file_id, line)) > 0;
  }
};

class SagePreprocessorRecord;

/*! \brief Translator from Clang AST to SAGE III (ROSE Compiler AST)
 */
class ClangToSageTranslator : public clang::ASTConsumer {
public:
  /*! \brief the 5 C-family languages supported by Clang
   */
  enum Language { C, CPLUSPLUS, OBJC, CUDA, OPENCL, unknown };

  /*! \brief Update a Sage node source position using Clang information
   *  \param node Sage node to update
   *  \param source_range Clang's source position information
   */
  void applySourceRange(SgNode *node, clang::SourceRange source_range);
  /*! \brief Set a Sage node source position to compiler generated
   *  \param node Sage node to update
   *  \param to_be_unparse should this compiler generated node be unparse?
   */
  void setCompilerGeneratedFileInfo(SgNode *node, bool to_be_unparse = false);
  /*! \brief Apply a source range to a ROSE node, extending the range to
   * include a trailing semicolon when present. This is used when Clang
   * provides an expression in statement position and ROSE must build a
   * wrapper SgExprStatement.
   */
  void applySourceRangeWithTrailingSemicolon(SgNode *rose_node,
                                             const clang::Stmt *clang_stmt);

protected:
  std::map<clang::Decl *, SgNode *> p_decl_translation_map;
  std::map<clang::Stmt *, SgNode *> p_stmt_translation_map;
  std::map<const clang::Type *, SgNode *> p_type_translation_map;
  std::map<clang::DeclContext *, SgScopeStatement *> p_decl_context_map;
  std::map<SgFunctionDefinition *, const clang::Stmt *> p_function_body_map;
  SgGlobal *p_global_scope;

  std::map<SgClassType *, bool> p_class_type_decl_first_see_in_type;
  std::map<SgEnumType *, bool> p_enum_type_decl_first_see_in_type;

  const RoseOpenMPPragmaCallback *p_openmp_pragma_callback;

  // Template declaration cache - maps template name to
  // SgTemplateClassDeclaration Key: mangled template name (e.g., "std::array")
  // Value: Template class declaration
  std::map<std::string, SgTemplateClassDeclaration *> p_template_decl_cache;

  // Template instantiation cache - maps instantiation signature to
  // SgTemplateInstantiationDecl Key: mangled instantiation name (e.g.,
  // "std::array<double, 1024>") Value: Template instantiation declaration
  std::map<std::string, SgTemplateInstantiationDecl *> p_template_inst_cache;
  struct CapturedPragma {
    unsigned line;
    std::string text;
    bool is_openmp;
  };

  // Recursion guard for GetSymbolFromSymbolTable to prevent infinite loops
  // when resolving symbols that reference each other (e.g., template members)
  std::set<clang::NamedDecl *> p_symbol_lookup_in_progress;

  // Recursion guard for decl translation to avoid re-entrant Traverse()
  // calls (e.g., when forcing translation of direct callees).
  std::set<clang::Decl *> p_decl_translation_in_progress;
  // Track decls translated on-demand (e.g., during type lowering or
  // symbol lookup) so we can repair scope attachments without duplicating
  // normal traversal insertions.
  std::set<clang::Decl *> p_decl_translation_on_demand;
  // Track class definitions already populated to avoid duplicate member
  // insertion during on-demand/re-entrant translation.
  std::set<const SgClassDefinition *> p_record_definitions_populated;

  // Deferred translation queue for implicit function template
  // instantiations. These instantiations are discovered while traversing
  // user code (e.g., at call sites) but are translated only after the
  // translation unit is otherwise complete to avoid ordering issues.
  std::vector<clang::FunctionDecl *> p_pending_implicit_function_instantiations;
  std::set<clang::FunctionDecl *>
      p_pending_implicit_function_instantiations_set;

  clang::CompilerInstance *p_compiler_instance;
  std::unique_ptr<SagePreprocessorRecord> p_sage_preprocessor_recorder;
  SgSourceFile *p_sage_source_file; // Parent file for connecting global scope

  Language language;

  SgSymbol *GetSymbolFromSymbolTable(clang::NamedDecl *decl);

  SgScopeStatement *resolveScopeFromDeclContext(clang::DeclContext *context,
                                                SgScopeStatement *fallback);

  // Select a scope that can safely accept an opaque type declaration.
  SgScopeStatement *getOpaqueTypeInsertionScope(SgScopeStatement *scope) const;

  // Find a safe insertion scope for opaque types using current scope
  // stack and falling back to global scope.
  SgScopeStatement *getSafeOpaqueTypeInsertionScope() const;

  SgType *buildTypeFromQualifiedType(const clang::QualType &qual_type);

  // Helper: Build nonreal return type for member typedefs of template
  // specializations (e.g., Spec::value_type) when needed for unparsing.
  SgType *buildSpecializedMemberTypedefReturnType(
      const clang::FunctionDecl *decl,
      const clang::ClassTemplateSpecializationDecl *spec_decl_override =
          nullptr,
      const clang::CXXRecordDecl *record_decl_override = nullptr);

  // Template helper methods
  // Helper: Get or create template class declaration
  SgTemplateClassDeclaration *getOrCreateTemplateDeclaration(
      const std::string &template_name,
      const clang::TemplateSpecializationType *clang_type,
      SgScopeStatement *scope_override = nullptr);

  // Helper: Build a nonreal scope chain for a nested name qualifier.
  SgScopeStatement *buildNonrealScopeFromNestedNameSpecifier(
      clang::NestedNameSpecifier *qualifier, SgScopeStatement *scope);

  // Helper: Get or create template instantiation
  SgTemplateInstantiationDecl *getOrCreateTemplateInstantiation(
      SgTemplateClassDeclaration *template_decl,
      const clang::TemplateSpecializationType *clang_type);

  // Helper: Build template arguments from Clang
  SgTemplateArgumentPtrList
  buildTemplateArguments(const clang::TemplateSpecializationType *clang_type);
  SgTemplateArgumentPtrList
  buildTemplateArguments(const clang::TemplateArgumentListInfo &arg_info,
                         bool explicitlySpecified = false);
  SgTemplateArgumentPtrList
  buildTemplateArguments(const clang::TemplateArgumentList &args,
                         size_t explicit_count = 0);
  void ensureTemplateArgumentParents(SgTemplateArgumentPtrList &args);
  void applyExplicitTemplateArgumentFlags(SgTemplateArgumentPtrList &args,
                                          size_t explicit_count);
  size_t countExpandedTemplateArguments(
      const clang::TemplateArgumentListInfo &arg_info);

  // Helper: Append translated template argument(s), flattening Clang
  // argument packs (TemplateArgument::Pack) into individual arguments.
  void appendTemplateArguments(SgTemplateArgumentPtrList &arg_list,
                               const clang::TemplateArgument &arg,
                               bool explicitlySpecified = false);

  // Helper: Translate a single template argument
  SgTemplateArgument *
  translateTemplateArgument(const clang::TemplateArgument &arg,
                            bool explicitlySpecified = false);

  // Helper: Build template parameters (inferred from arguments)
  std::unique_ptr<SgTemplateParameterPtrList>
  buildTemplateParameters(const clang::TemplateSpecializationType *clang_type);

  // Helper: Get qualified name for a template declaration (e.g.,
  // "std::array")
  std::string
  getTemplateQualifiedName(SgTemplateClassDeclaration *template_decl);

  // Helper: Generate mangled name for instantiation cache key
  std::string mangleTemplateInstantiation(
      const std::string &template_name,
      const clang::TemplateSpecializationType *spec_type);

  // Helper: Generate mangled name for instantiation cache key (overload
  // for declarations)
  std::string
  mangleTemplateInstantiation(const std::string &template_name,
                              const clang::TemplateArgumentList &args);

  // Helper: Translate template parameter lists on declarations
  SgTemplateParameter *
  translateTemplateParameter(clang::NamedDecl *param_decl,
                             SgDeclarationStatement *owning_template,
                             unsigned position);

  std::unique_ptr<SgTemplateParameterPtrList>
  translateTemplateParameterList(clang::TemplateParameterList *param_list,
                                 SgDeclarationStatement *owning_template);

  SgTemplateClassDeclaration *
  translateClassTemplateDecl(clang::ClassTemplateDecl *class_template_decl,
                             SgScopeStatement *override_symbol_scope,
                             SgScopeStatement *override_lexical_parent);

  bool translateFunctionDeclCommon(clang::FunctionDecl *function_decl,
                                   clang::FunctionTemplateDecl *template_decl,
                                   SgNode **node);

  void populateClassDefinition(clang::RecordDecl *record_decl,
                               SgClassDefinition *class_def);
  bool collectOpenMPPragmas(clang::Stmt *stmt,
                            std::vector<CapturedPragma> &pragmas);
  SgPragmaDeclaration *
  buildOpenMPPragmaDeclaration(const std::string &directive,
                               unsigned pragma_line, SgScopeStatement *scope);
  void appendOpenMPPragmasBefore(clang::Stmt *stmt, SgScopeStatement *scope);
  SgStatement *wrapStatementWithOpenMPPragmas(clang::Stmt *stmt,
                                              SgStatement *statement);

  // Helper: Ensure a namespace declaration exists (creating stubs if
  // needed, recursively)
  SgNamespaceDeclarationStatement *
  ensureNamespaceDeclaration(clang::NamespaceDecl *ns_decl);

  // Helper: Build a non-real qualified type from a Clang nested-name
  // specifier plus a terminal name (optionally with template arguments).
  SgNonrealType *buildNonrealTypeFromNestedNameSpecifier(
      clang::NestedNameSpecifier *qualifier, SgScopeStatement *scope,
      const SgName &terminalName,
      const SgTemplateArgumentPtrList *terminalTemplateArgs);

  SgNonrealRefExp *buildNonrealRefExpFromNestedNameSpecifier(
      clang::NestedNameSpecifier *qualifier, SgScopeStatement *scope,
      const SgName &terminalName, bool terminalHasTemplateKeyword,
      const SgTemplateArgumentPtrList *terminalTemplateArgs);

  // Helper: Translate a Clang type used in a nested-name-specifier
  // (TypeSpec / TypeSpecWithTemplate) into a SgNonrealType, created in
  // the provided scope.
  SgNonrealType *
  buildNonrealTypeForNestedNameSpecifierType(const clang::Type *clang_type,
                                             SgScopeStatement *scope);

public:
  ClangToSageTranslator(clang::CompilerInstance *compiler_instance,
                        Language language_, SgSourceFile *sage_source_file);

  virtual ~ClangToSageTranslator();

  SgGlobal *getGlobalScope() const;

  void setOpenMPPragmaCallback(const RoseOpenMPPragmaCallback *callback) {
    p_openmp_pragma_callback = callback;
  }

  /* ASTConsumer's methods overload */

  virtual void HandleTranslationUnit(clang::ASTContext &ast_context);

  /* Traverse methods */

  virtual SgNode *Traverse(clang::Decl *decl);
  virtual SgNode *Traverse(clang::Stmt *stmt);
  virtual SgNode *Traverse(const clang::Type *type);
  virtual bool TraverseForDeclContext(clang::DeclContext *decl_context);
  virtual SgNode *TraverseOnDemand(clang::Decl *decl);

  /* Visit methods */
  /*
     Reference: clang/AST/Decl.h
     Overall 84 decl AST nodes according as 04/24/2019
  */
  virtual bool VisitDecl(clang::Decl *decl, SgNode **node);
  virtual bool VisitAccessSpecDecl(clang::AccessSpecDecl *access_spec_decl,
                                   SgNode **node);
  virtual bool VisitBlockDecl(clang::BlockDecl *block_decl, SgNode **node);
  virtual bool VisitCapturedDecl(clang::CapturedDecl *capture_decl,
                                 SgNode **node);
  virtual bool VisitEmptyDecl(clang::EmptyDecl *empty_decl, SgNode **node);
  virtual bool VisitExportDecl(clang::ExportDecl *export_decl, SgNode **node);
  virtual bool VisitExternCContextDecl(clang::ExternCContextDecl *ccontext_decl,
                                       SgNode **node);
  virtual bool
  VisitFileScopeAsmDecl(clang::FileScopeAsmDecl *file_scope_asm_decl,
                        SgNode **node);
  virtual bool VisitFriendDecl(clang::FriendDecl *friend_decl, SgNode **node);
  virtual bool
  VisitFriendTemplateDecl(clang::FriendTemplateDecl *friend_template_decl,
                          SgNode **node);
  virtual bool VisitImportDecl(clang::ImportDecl *import_decl, SgNode **node);
  virtual bool VisitNamedDecl(clang::NamedDecl *named_decl, SgNode **node);
  virtual bool VisitLabelDecl(clang::LabelDecl *label_decl, SgNode **node);
  virtual bool
  VisitNamespaceAliasDecl(clang::NamespaceAliasDecl *namespace_alias_decl,
                          SgNode **node);
  virtual bool VisitNamespaceDecl(clang::NamespaceDecl *namespace_decl,
                                  SgNode **node);
  virtual bool VisitLinkageSpecDecl(clang::LinkageSpecDecl *linkage_spec_decl,
                                    SgNode **node);
  // virtual bool VisitObjCCompatibleAliasDecl
  // virtual bool VisitObjCContainerDecl
  // virtual bool VisitObjCCategoryDecl
  // virtual bool VisitObjCInterfaceDecl
  // virtual bool VisitBuiCProtocolDecl
  // virtual bool VisitBuiltinTemplateDecl
  // virtual bool VisitObjCMethodDecl
  // virtual bool VisitObjCPropertyDecl
  virtual bool VisitTemplateDecl(clang::TemplateDecl *template_decl,
                                 SgNode **node);
  virtual bool
  VisitBuiltinTemplateDecl(clang::BuiltinTemplateDecl *builtin_template_decl,
                           SgNode **node);
  virtual bool VisitConceptDecl(clang::ConceptDecl *concept_decl,
                                SgNode **node);
  virtual bool VisitRedeclarableTemplateDecl(
      clang::RedeclarableTemplateDecl *redeclarable_template_decl,
      SgNode **node);
  virtual bool
  VisitClassTemplateDecl(clang::ClassTemplateDecl *class_template_decl,
                         SgNode **node);
  virtual bool
  VisitFunctionTemplateDecl(clang::FunctionTemplateDecl *function_template_decl,
                            SgNode **node);
  virtual bool VisitTypeAliasTemplateDecl(
      clang::TypeAliasTemplateDecl *type_alias_template_decl, SgNode **node);
  virtual bool VisitVarTemplateDecl(clang::VarTemplateDecl *var_template_decl,
                                    SgNode **node);
  virtual bool VisitTemplateTemplateParmDecl(
      clang::TemplateTemplateParmDecl *template_template_parm_decl,
      SgNode **node);
  virtual bool VisitTypeDecl(clang::TypeDecl *type_decl, SgNode **node);
  virtual bool VisitTagDecl(clang::TagDecl *tag_decl, SgNode **node);
  virtual bool VisitRecordDecl(clang::RecordDecl *record_decl, SgNode **node);
  virtual bool VisitCXXRecordDecl(clang::CXXRecordDecl *cxx_record_decl,
                                  SgNode **node);
  virtual bool VisitClassTemplateSpecializationDecl(
      clang::ClassTemplateSpecializationDecl *class_tpl_spec_decl,
      SgNode **node);
  bool VisitClassTemplateSpecializationDecl_Impl(
      clang::ClassTemplateSpecializationDecl *class_tpl_spec_decl,
      SgNode **node);
  virtual bool VisitClassTemplatePartialSpecializationDecl(
      clang::ClassTemplatePartialSpecializationDecl *class_tpl_part_spec_decl,
      SgNode **node);

  virtual bool VisitEnumDecl(clang::EnumDecl *enum_decl, SgNode **node);
  virtual bool VisitTemplateTypeParmDecl(
      clang::TemplateTypeParmDecl *template_type_parm_decl, SgNode **node);
  virtual bool VisitTypedefNameDecl(clang::TypedefNameDecl *typedef_name_decl,
                                    SgNode **node);
  virtual bool VisitTypedefDecl(clang::TypedefDecl *typedef_decl,
                                SgNode **node);
  virtual bool VisitTypeAliasDecl(clang::TypeAliasDecl *type_alias_decl,
                                  SgNode **node);
  // virtual bool
  // VisitObjCTypeParamDecl(clang::ObjCTypeParamDecl
  // * obj_type_param_decl, SgNode ** node);
  virtual bool VisitUnresolvedUsingTypenameDecl(
      clang::UnresolvedUsingTypenameDecl *unresolved_using_type_name_decl,
      SgNode **node);
  virtual bool VisitUsingDecl(clang::UsingDecl *using_decl, SgNode **node);
  virtual bool
  VisitUsingDirectiveDecl(clang::UsingDirectiveDecl *using_directive_decl,
                          SgNode **node);
  virtual bool VisitUsingPackDecl(clang::UsingPackDecl *using_pack_decl,
                                  SgNode **node);
  virtual bool VisitUsingShadowDecl(clang::UsingShadowDecl *using_shadow_decl,
                                    SgNode **node);
  virtual bool VisitConstructorUsingShadowDecl(
      clang::ConstructorUsingShadowDecl *constructor_using_shadow_decl,
      SgNode **node);
  virtual bool VisitValueDecl(clang::ValueDecl *value_decl, SgNode **node);
  virtual bool VisitBindingDecl(clang::BindingDecl *binding_decl,
                                SgNode **node);
  virtual bool VisitDeclaratorDecl(clang::DeclaratorDecl *declarator_decl,
                                   SgNode **node);
  virtual bool VisitFieldDecl(clang::FieldDecl *field_decl, SgNode **node);
  // virtual bool VisitObjCAtDefsFieldDecl
  // virtual bool VisitObjCvarDecl
  virtual bool VisitFunctionDecl(clang::FunctionDecl *function_decl,
                                 SgNode **node);
  virtual bool VisitCXXDeductionGuideDecl(
      clang::CXXDeductionGuideDecl *cxx_deduction_guide_guide, SgNode **node);
  virtual bool VisitCXXMethodDecl(clang::CXXMethodDecl *cxx_method_decl,
                                  SgNode **node);
  virtual bool
  VisitCXXConstructorDecl(clang::CXXConstructorDecl *cxx_constructor_decl,
                          SgNode **node);
  virtual bool
  VisitCXXConversionDecl(clang::CXXConversionDecl *cxx_conversion_decl,
                         SgNode **node);
  virtual bool
  VisitCXXDestructorDecl(clang::CXXDestructorDecl *cxx_destructor_decl,
                         SgNode **node);
  virtual bool VisitMSPropertyDecl(clang::MSPropertyDecl *ms_property_decl,
                                   SgNode **node);
  virtual bool VisitNonTypeTemplateParmDecl(
      clang::NonTypeTemplateParmDecl *non_type_template_param_decl,
      SgNode **node);
  virtual bool VisitVarDecl(clang::VarDecl *var_decl, SgNode **node);
  virtual bool
  VisitDecompositionDecl(clang::DecompositionDecl *decomposition_decl,
                         SgNode **node);
  virtual bool
  VisitImplicitParamDecl(clang::ImplicitParamDecl *implicit_param_decl,
                         SgNode **node);
  virtual bool
  VisitOMPCaptureExprDecl(clang::OMPCapturedExprDecl *omp_capture_expr_decl,
                          SgNode **node);
  virtual bool VisitParmVarDecl(clang::ParmVarDecl *param_var_decl,
                                SgNode **node);
  virtual bool VisitVarTemplateSpecializationDecl(
      clang::VarTemplateSpecializationDecl *var_template_specialization_decl,
      SgNode **node);
  virtual bool VisitVarTemplatePartialSpecializationDecl(
      clang::VarTemplatePartialSpecializationDecl
          *var_template_partial_specialization,
      SgNode **node);
  virtual bool
  VisitEnumConstantDecl(clang::EnumConstantDecl *enum_constant_decl,
                        SgNode **node);
  virtual bool
  VisitIndirectFieldDecl(clang::IndirectFieldDecl *indirect_field_decl,
                         SgNode **node);
  virtual bool VisitOMPDeclareMapperDecl(
      clang::OMPDeclareMapperDecl *omp_declare_mapper_decl, SgNode **node);
  virtual bool VisitOMPDeclareReductionDecl(
      clang::OMPDeclareReductionDecl *omp_declare_reduction_decl,
      SgNode **node);
  virtual bool VisitUnresolvedUsingValueDecl(
      clang::UnresolvedUsingValueDecl *unresolved_using_value_decl,
      SgNode **node);
  // virtual bool VisitObjCPropertyImplDecl
  virtual bool VisitOMPAllocateDecl(clang::OMPAllocateDecl *omp_allocate_decl,
                                    SgNode **node);
  virtual bool VisitOMPRequiresDecl(clang::OMPRequiresDecl *omp_requires_decl,
                                    SgNode **node);
  virtual bool VisitOMPThreadPrivateDecl(
      clang::OMPThreadPrivateDecl *omp_thread_private_decl, SgNode **node);
  virtual bool
  VisitPragmaCommentDecl(clang::PragmaCommentDecl *pragma_comment_decl,
                         SgNode **node);
  virtual bool VisitPragmaDetectMismatchDecl(
      clang::PragmaDetectMismatchDecl *pragma_detect_mismatch, SgNode **node);
  virtual bool
  VisitStaticAssertDecl(clang::StaticAssertDecl *static_assert_decl,
                        SgNode **node);
  virtual bool
  VisitTranslationUnitDecl(clang::TranslationUnitDecl *translation_unit_decl,
                           SgNode **node);

  /*
     Reference: clang/AST/Stmt.h
     Overall 198 stmt AST nodes according as 02/19/2019
  */
  virtual bool VisitStmt(clang::Stmt *stmt, SgNode **node);
  virtual bool VisitAsmStmt(clang::AsmStmt *asm_stmt, SgNode **node);
  virtual bool VisitGCCAsmStmt(clang::GCCAsmStmt *gcc_asm_stmt, SgNode **node);
  virtual bool VisitMSAsmStmt(clang::MSAsmStmt *ms_asm_stmt, SgNode **node);
  virtual bool VisitBreakStmt(clang::BreakStmt *break_stmt, SgNode **node);
  virtual bool VisitCapturedStmt(clang::CapturedStmt *captured_stmt,
                                 SgNode **node);
  virtual bool VisitCompoundStmt(clang::CompoundStmt *compound_stmt,
                                 SgNode **node);
  virtual bool VisitContinueStmt(clang::ContinueStmt *continue_stmt,
                                 SgNode **node);
  virtual bool VisitCoreturnStmt(clang::CoreturnStmt *coreturn_stmt,
                                 SgNode **node);
  virtual bool
  VisitCoroutineBodyStmt(clang::CoroutineBodyStmt *coroutine_body_stmt,
                         SgNode **node);
  virtual bool VisitCXXCatchStmt(clang::CXXCatchStmt *cxx_catch_stmt,
                                 SgNode **node);
  virtual bool VisitCXXForRangeStmt(clang::CXXForRangeStmt *cxx_for_range_stmt,
                                    SgNode **node);
  virtual bool VisitCXXTryStmt(clang::CXXTryStmt *cxx_try_stmt, SgNode **node);
  virtual bool VisitDeclStmt(clang::DeclStmt *decl_stmt, SgNode **node);
  virtual bool VisitDoStmt(clang::DoStmt *do_stmt, SgNode **node);
  virtual bool VisitForStmt(clang::ForStmt *for_stmt, SgNode **node);
  virtual bool VisitGotoStmt(clang::GotoStmt *goto_stmt, SgNode **node);
  virtual bool VisitIfStmt(clang::IfStmt *if_stmt, SgNode **node);
  virtual bool
  VisitIndirectGotoStmt(clang::IndirectGotoStmt *indirect_goto_stmt,
                        SgNode **node);
  virtual bool VisitMSDependentExistsStmt(
      clang::MSDependentExistsStmt *ms_dependent_exists_stmt, SgNode **node);
  virtual bool VisitNullStmt(clang::NullStmt *null_stmt, SgNode **node);
  // virtual bool VisitObjCAtCatchStmt
  // virtual bool VisitObjCAtFinallyStmt
  // virtual bool VisitObjCAtSynchronizedStmt
  // virtual bool VisitObjCAtThrowStmt
  // virtual bool VisitObjCAtTryStmt
  // virtual bool VisitObjCAutoreleasePoolStmt
  // virtual bool VisitObjCForCollectionStmt
  virtual bool VisitOMPExecutableDirective(
      clang::OMPExecutableDirective *omp_executable_directive, SgNode **node);
  virtual bool
  VisitOMPAtomicDirective(clang::OMPAtomicDirective *omp_atomic_directive,
                          SgNode **node);
  virtual bool
  VisitOMPBarrierDirective(clang::OMPBarrierDirective *omp_barrier_directive,
                           SgNode **node);
  virtual bool
  VisitOMPCancelDirective(clang::OMPCancelDirective *omp_cancel_directive,
                          SgNode **node);
  virtual bool VisitOMPCancellationPointDirective(
      clang::OMPCancellationPointDirective *omp_cancellation_point_directive,
      SgNode **node);
  virtual bool
  VisitOMPCriticalDirective(clang::OMPCriticalDirective *omp_critical_directive,
                            SgNode **node);
  virtual bool
  VisitOMPFlushDirective(clang::OMPFlushDirective *omp_flush_directive,
                         SgNode **node);
  virtual bool
  VisitOMPLoopDirective(clang::OMPLoopDirective *omp_loop_directive,
                        SgNode **node);
  virtual bool VisitOMPDistributeDirective(
      clang::OMPDistributeDirective *omp_distribute_directive, SgNode **node);
  virtual bool VisitOMPDistributeParallelForDirective(
      clang::OMPDistributeParallelForDirective
          *omp_distribute_parallel_for_directive,
      SgNode **node);
  virtual bool VisitOMPDistributeParallelForSimdDirective(
      clang::OMPDistributeParallelForSimdDirective
          *omp_distribute_parallel_for_simd_directive,
      SgNode **node);
  virtual bool VisitOMPDistributeSimdDirective(
      clang::OMPDistributeSimdDirective *omp_distribute__simd_directive,
      SgNode **node);
  virtual bool VisitOMPForDirective(clang::OMPForDirective *omp_for_directive,
                                    SgNode **node);
  virtual bool
  VisitOMPForSimdDirective(clang::OMPForSimdDirective *omp_for_simd_directive,
                           SgNode **node);
  //  virtual bool
  //  VisitOMPMasterTaskLoopDirective(clang::OMPMasterTaskLoopDirective *
  //  omp_master_task_loop_directive, SgNode ** node);
  // virtual bool VisitOMPMasterTaskLoopSimdDirective
  virtual bool VisitOMPParallelForDirective(
      clang::OMPParallelForDirective *omp_parallel_for_directive,
      SgNode **node);
  virtual bool VisitOMPParallelForSimdDirective(
      clang::OMPParallelForSimdDirective *omp_parallel_for_simd_directive,
      SgNode **node);
  // virtual bool VisitOMPParallelMasterTaskLoopDirective
  virtual bool
  VisitOMPSimdDirective(clang::OMPSimdDirective *omp_simd_directive,
                        SgNode **node);
  virtual bool VisitOMPTargetParallelForDirective(
      clang::OMPTargetParallelForDirective *omp_target_parallel_for_directive,
      SgNode **node);
  virtual bool VisitOMPTargetParallelForSimdDirective(
      clang::OMPTargetParallelForSimdDirective
          *omp_target_parallel_for_simd_directive,
      SgNode **node);
  virtual bool VisitOMPTargetSimdDirective(
      clang::OMPTargetSimdDirective *omp_target_simd_directive, SgNode **node);
  virtual bool VisitOMPTargetTeamsDistributeDirective(
      clang::OMPTargetTeamsDistributeDirective
          *omp_target_teams_distribute_directive,
      SgNode **node);
  // virtual bool VisitOMPTargetTeamsDistributeParallelForSimdDirective
  virtual bool VisitOMPTargetTeamsDistributeSimdDirective(
      clang::OMPTargetTeamsDistributeSimdDirective
          *omp_target_teams_distribute_simd_directive,
      SgNode **node);
  virtual bool VisitOMPTaskLoopDirective(
      clang::OMPTaskLoopDirective *omp_task_loop_directive, SgNode **node);
  virtual bool VisitOMPTaskLoopSimdDirective(
      clang::OMPTaskLoopSimdDirective *omp_task_loop_simd_directive,
      SgNode **node);
  // virtual bool VisitOMPTeamDistributeDirective
  // virtual bool VisitOMPTeamDistributeParallelForSimdDirective
  // virtual bool VisitOMPTeamDistributeSimdDirective
  virtual bool
  VisitOMPMasterDirective(clang::OMPMasterDirective *omp_master_directive,
                          SgNode **node);
  virtual bool
  VisitOMPOrderedDirective(clang::OMPOrderedDirective *omp_ordered_directive,
                           SgNode **node);
  virtual bool
  VisitOMPParallelDirective(clang::OMPParallelDirective *omp_parallel_directive,
                            SgNode **node);
  virtual bool VisitOMPParallelSectionsDirective(
      clang::OMPParallelSectionsDirective *omp_parallel_sections_directive,
      SgNode **node);
  virtual bool VisitReturnStmt(clang::ReturnStmt *return_stmt, SgNode **node);
  virtual bool VisitSEHExceptStmt(clang::SEHExceptStmt *seh_except_stmt,
                                  SgNode **node);
  virtual bool VisitSEHFinallyStmt(clang::SEHFinallyStmt *seh_finally_stmt,
                                   SgNode **node);
  virtual bool VisitSEHLeaveStmt(clang::SEHLeaveStmt *seh_leave_stmt,
                                 SgNode **node);
  virtual bool VisitSEHTryStmt(clang::SEHTryStmt *seh_try_stmt, SgNode **node);
  virtual bool VisitSwitchCase(clang::SwitchCase *switch_case, SgNode **node);
  virtual bool VisitCaseStmt(clang::CaseStmt *case_stmt, SgNode **node);
  virtual bool VisitDefaultStmt(clang::DefaultStmt *default_stmt,
                                SgNode **node);
  virtual bool VisitSwitchStmt(clang::SwitchStmt *switch_stmt, SgNode **node);
  virtual bool VisitValueStmt(clang::ValueStmt *value_stmt, SgNode **node);
  virtual bool VisitAttributedStmt(clang::AttributedStmt *attributed_stmt,
                                   SgNode **node);
  virtual bool VisitExpr(clang::Expr *expr, SgNode **node);
  virtual bool VisitAbstractConditionalOperator(
      clang::AbstractConditionalOperator *abstract_conditional_operator,
      SgNode **node);
  virtual bool VisitBinaryConditionalOperator(
      clang::BinaryConditionalOperator *binary_conditional_operator,
      SgNode **node);
  virtual bool
  VisitConditionalOperator(clang::ConditionalOperator *conditional_operator,
                           SgNode **node);
  virtual bool VisitAddrLabelExpr(clang::AddrLabelExpr *addr_label_expr,
                                  SgNode **node);
  virtual bool
  VisitArrayInitIndexExpr(clang::ArrayInitIndexExpr *array_init_index_expr,
                          SgNode **node);
  virtual bool
  VisitArrayInitLoopExpr(clang::ArrayInitLoopExpr *array_init_loop_expr,
                         SgNode **node);
  virtual bool
  VisitArraySubscriptExpr(clang::ArraySubscriptExpr *array_subscript_expr,
                          SgNode **node);
  virtual bool
  VisitArrayTypeTraitExpr(clang::ArrayTypeTraitExpr *array_type_trait_expr,
                          SgNode **node);
  virtual bool VisitAsTypeExpr(clang::AsTypeExpr *as_type_expr, SgNode **node);
  virtual bool VisitAtomicExpr(clang::AtomicExpr *atomic_expr, SgNode **node);
  virtual bool VisitBinaryOperator(clang::BinaryOperator *binary_operator,
                                   SgNode **node);
  virtual bool VisitCompoundAssignOperator(
      clang::CompoundAssignOperator *compound_assign_operator, SgNode **node);
  virtual bool VisitBlockExpr(clang::BlockExpr *block_expr, SgNode **node);
  virtual bool VisitCallExpr(clang::CallExpr *call_expr, SgNode **node);
  virtual bool
  VisitCUDAKernelCallExpr(clang::CUDAKernelCallExpr *cuda_kernel_call_expr,
                          SgNode **node);
  virtual bool
  VisitCXXMemberCallExpr(clang::CXXMemberCallExpr *cxx_member_call_expr,
                         SgNode **node);
  virtual bool
  VisitCXXOperatorCallExpr(clang::CXXOperatorCallExpr *cxx_operator_call_expr,
                           SgNode **node);
  virtual bool
  VisitUserDefinedLiteral(clang::UserDefinedLiteral *user_defined_literal,
                          SgNode **node);
  virtual bool VisitCastExpr(clang::CastExpr *cast_expr, SgNode **node);
  virtual bool
  VisitExplicitCastExpr(clang::ExplicitCastExpr *explicit_cast_expr,
                        SgNode **node);
  virtual bool
  VisitBuiltinBitCastExpr(clang::BuiltinBitCastExpr *builtin_bit_cast_expr,
                          SgNode **node);
  virtual bool VisitCStyleCastExpr(clang::CStyleCastExpr *c_style_cast,
                                   SgNode **node);
  virtual bool VisitCXXFunctionalCastExpr(
      clang::CXXFunctionalCastExpr *cxx_functional_cast_expr, SgNode **node);
  virtual bool
  VisitCXXNamedCastExpr(clang::CXXNamedCastExpr *cxx_named_cast_expr,
                        SgNode **node);
  virtual bool
  VisitCXXConstCastExpr(clang::CXXConstCastExpr *cxx_const_cast_expr,
                        SgNode **node);
  virtual bool
  VisitCXXDynamicCastExpr(clang::CXXDynamicCastExpr *cxx_dynamic_cast_expr,
                          SgNode **node);
  virtual bool VisitCXXReinterpretCastExpr(
      clang::CXXReinterpretCastExpr *cxx_reinterpret_cast_expr, SgNode **node);
  virtual bool
  VisitCXXStaticCastExpr(clang::CXXStaticCastExpr *cxx_static_cast_expr,
                         SgNode **node);
  // virtual bool VisitObjCBridgedCastExpr
  virtual bool
  VisitImplicitCastExpr(clang::ImplicitCastExpr *implicit_cast_expr,
                        SgNode **node);
  virtual bool VisitCharacterLiteral(clang::CharacterLiteral *character_literal,
                                     SgNode **node);
  virtual bool VisitChooseExpr(clang::ChooseExpr *choose_expr, SgNode **node);
  virtual bool
  VisitCompoundLiteralExpr(clang::CompoundLiteralExpr *compound_literal,
                           SgNode **node);
  //                    virtual bool
  //                    VisitConceptSpecializationExpr(clang::ConceptSpecializationExpr
  //                    * concept_specialization_expr, SgNode ** node);
  virtual bool
  VisitConvertVectorExpr(clang::ConvertVectorExpr *convert_vector_expr,
                         SgNode **node);
  virtual bool
  VisitCoroutineSuspendExpr(clang::CoroutineSuspendExpr *coroutine_suspend_expr,
                            SgNode **node);
  virtual bool VisitCoawaitExpr(clang::CoawaitExpr *coawait_expr,
                                SgNode **node);
  virtual bool VisitCoyieldExpr(clang::CoyieldExpr *coyield_expr,
                                SgNode **node);
  virtual bool VisitCXXBindTemporaryExpr(
      clang::CXXBindTemporaryExpr *cxx_bind_temporary_expr, SgNode **node);
  virtual bool
  VisitCXXBoolLiteralExpr(clang::CXXBoolLiteralExpr *cxx_bool_literal_expr,
                          SgNode **node);
  virtual bool
  VisitCXXConstructExpr(clang::CXXConstructExpr *cxx_construct_expr,
                        SgNode **node);
  virtual bool VisitCXXTemporaryObjectExpr(
      clang::CXXTemporaryObjectExpr *cxx_temporary_object_expr, SgNode **node);
  virtual bool
  VisitCXXDefaultArgExpr(clang::CXXDefaultArgExpr *cxx_default_arg_expr,
                         SgNode **node);
  virtual bool
  VisitCXXDefaultInitExpr(clang::CXXDefaultInitExpr *cxx_default_init_expr,
                          SgNode **node);
  virtual bool VisitCXXDeleteExpr(clang::CXXDeleteExpr *cxx_delete_expr,
                                  SgNode **node);
  virtual bool VisitCXXDependentScopeMemberExpr(
      clang::CXXDependentScopeMemberExpr *cxx_dependent_scope_member_expr,
      SgNode **node);
  virtual bool VisitCXXFoldExpr(clang::CXXFoldExpr *cxx_fold_expr,
                                SgNode **node);
  virtual bool VisitCXXInheritedCtorInitExpr(
      clang::CXXInheritedCtorInitExpr *cxx_inherited_ctor_init_expr,
      SgNode **node);
  virtual bool VisitCXXNewExpr(clang::CXXNewExpr *cxx_new_expr, SgNode **node);
  virtual bool VisitCXXNoexceptExpr(clang::CXXNoexceptExpr *cxx_noexcept_expr,
                                    SgNode **node);
  virtual bool VisitCXXNullPtrLiteralExpr(
      clang::CXXNullPtrLiteralExpr *cxx_null_ptr_literal_expr, SgNode **node);
  virtual bool VisitCXXPseudoDestructorExpr(
      clang::CXXPseudoDestructorExpr *cxx_pseudo_destructor_expr,
      SgNode **node);
  //                    virtual bool
  //                    VisitCXXRewrittenBinaryOperator(clang::CXXRewrittenBinaryOperator
  //                    * cxx_rewrite_binary_operator, SgNode ** node);
  virtual bool VisitCXXScalarValueInitExpr(
      clang::CXXScalarValueInitExpr *cxx_scalar_value_init_expr, SgNode **node);
  virtual bool VisitCXXStdInitializerListExpr(
      clang::CXXStdInitializerListExpr *cxx_std_initializer_list_expr,
      SgNode **node);
  virtual bool VisitCXXThisExpr(clang::CXXThisExpr *cxx_this_expr,
                                SgNode **node);
  virtual bool VisitCXXThrowExpr(clang::CXXThrowExpr *cxx_throw_expr,
                                 SgNode **node);
  virtual bool VisitCXXTypeidExpr(clang::CXXTypeidExpr *cxx_typeid_expr,
                                  SgNode **node);
  virtual bool VisitCXXUnresolvedConstructExpr(
      clang::CXXUnresolvedConstructExpr *cxx_unresolved_construct_expr,
      SgNode **node);
  virtual bool VisitCXXUuidofExpr(clang::CXXUuidofExpr *cxx_uuidof_expr,
                                  SgNode **node);
  virtual bool VisitDeclRefExpr(clang::DeclRefExpr *decl_ref_expr,
                                SgNode **node);
  virtual bool
  VisitDependentCoawaitExpr(clang::DependentCoawaitExpr *dependent_coawait_expr,
                            SgNode **node);
  virtual bool VisitDependentScopeDeclRefExpr(
      clang::DependentScopeDeclRefExpr *dependent_scope_decl_ref_expr,
      SgNode **node);
  virtual bool
  VisitDesignatedInitExpr(clang::DesignatedInitExpr *designated_init_expr,
                          SgNode **node);
  virtual bool VisitDesignatedInitUpdateExpr(
      clang::DesignatedInitUpdateExpr *designated_init_update_expr,
      SgNode **node);

  virtual bool
  VisitExpressionTraitExpr(clang::ExpressionTraitExpr *expression_trait_expr,
                           SgNode **node);
  virtual bool VisitExtVectorElementExpr(
      clang::ExtVectorElementExpr *ext_vector_element_expr, SgNode **node);
  virtual bool
  VisitFixedPointLiteral(clang::FixedPointLiteral *fixed_point_literal,
                         SgNode **node);
  virtual bool VisitFloatingLiteral(clang::FloatingLiteral *floating_literal,
                                    SgNode **node);
  virtual bool VisitFullExpr(clang::FullExpr *full_expr, SgNode **node);
  virtual bool VisitConstantExpr(clang::ConstantExpr *constant_expr,
                                 SgNode **node);
  virtual bool
  VisitExprWithCleanups(clang::ExprWithCleanups *expr_with_cleanups,
                        SgNode **node);
  virtual bool VisitFunctionParmPackExpr(
      clang::FunctionParmPackExpr *function_parm_pack_expr, SgNode **node);
  virtual bool
  VisitGenericSelectionExpr(clang::GenericSelectionExpr *generic_selection_expr,
                            SgNode **node);
  virtual bool VisitGNUNullExpr(clang::GNUNullExpr *gnu_null_expr,
                                SgNode **node);
  virtual bool VisitImaginaryLiteral(clang::ImaginaryLiteral *imaginary_literal,
                                     SgNode **node);
  virtual bool VisitImplicitValueInitExpr(
      clang::ImplicitValueInitExpr *implicit_value_init_expr, SgNode **node);
  virtual bool VisitInitListExpr(clang::InitListExpr *init_list_expr,
                                 SgNode **node);
  virtual bool VisitIntegerLiteral(clang::IntegerLiteral *integer_literal,
                                   SgNode **node);
  virtual bool VisitLambdaExpr(clang::LambdaExpr *lambda_expr, SgNode **node);
  virtual bool VisitMaterializeTemporaryExpr(
      clang::MaterializeTemporaryExpr *materialize_temporary_expr,
      SgNode **node);
  virtual bool VisitMemberExpr(clang::MemberExpr *member_expr, SgNode **node);
  virtual bool
  VisitMSPropertyRefExpr(clang::MSPropertyRefExpr *ms_property_expr,
                         SgNode **node);
  virtual bool VisitMSPropertySubscriptExpr(
      clang::MSPropertySubscriptExpr *ms_property_subscript_expr,
      SgNode **node);
  virtual bool VisitNoInitExpr(clang::NoInitExpr *no_init_expr, SgNode **node);
  // virtual bool VisitObjCArrayLiteral
  // virtual bool VisitObjCAvailabilityCheckExpr
  // virtual bool VisitObjCBoolLiteralExpr
  // virtual bool VisitObjCBoxedExpr
  // virtual bool VisitObjCDictionaryLiteral
  // virtual bool VisitObjCEncodeExpr
  // virtual bool VisitObjCIndirectCopyRestoreExpr
  // virtual bool VisitObjCIsaExpr
  // virtual bool VisitObjClvarRefExpr
  // virtual bool VisitObjCMessageExpr
  // virtual bool VisitObjCPropertyRefExpr
  // virtual bool VisitObjCProtocolExpr
  // virtual bool VisitObjCSelectorExpr
  // virtual bool VisitObjCCStringLiteral
  // virtual bool VisitObjCSubscriptRefexpr
  virtual bool VisitOffsetOfExpr(clang::OffsetOfExpr *offset_of_expr,
                                 SgNode **node);
  virtual bool
  VisitOMPArraySectionExpr(clang::ArraySectionExpr *omp_array_section_expr,
                           SgNode **node);
  virtual bool VisitOpaqueValueExpr(clang::OpaqueValueExpr *opaque_value_expr,
                                    SgNode **node);
  virtual bool VisitOverloadExpr(clang::OverloadExpr *overload_expr,
                                 SgNode **node);
  virtual bool
  VisitUnresolvedLookupExpr(clang::UnresolvedLookupExpr *unresolved_lookup_expr,
                            SgNode **node);
  virtual bool
  VisitUnresolvedMemberExpr(clang::UnresolvedMemberExpr *unresolved_member_expr,
                            SgNode **node);
  virtual bool
  VisitPackExpansionExpr(clang::PackExpansionExpr *pack_expansion_expr,
                         SgNode **node);
  virtual bool VisitParenExpr(clang::ParenExpr *paren_expr, SgNode **node);
  virtual bool VisitParenListExpr(clang::ParenListExpr *paran_list_expr,
                                  SgNode **node);
  virtual bool VisitPredefinedExpr(clang::PredefinedExpr *predefined_expr,
                                   SgNode **node);
  virtual bool
  VisitPseudoObjectExpr(clang::PseudoObjectExpr *pseudo_object_expr,
                        SgNode **node);
  virtual bool
  VisitShuffleVectorExpr(clang::ShuffleVectorExpr *shuffle_vector_expr,
                         SgNode **node);
  virtual bool VisitSizeOfPackExpr(clang::SizeOfPackExpr *size_of_pack_expr,
                                   SgNode **node);
  virtual bool VisitSourceLocExpr(clang::SourceLocExpr *source_loc_expr,
                                  SgNode **node);
  virtual bool VisitStmtExpr(clang::StmtExpr *stmt_expr, SgNode **node);
  virtual bool VisitStringLiteral(clang::StringLiteral *string_literal,
                                  SgNode **node);
  virtual bool VisitSubstNonTypeTemplateParmExpr(
      clang::SubstNonTypeTemplateParmExpr *subst_non_type_template_parm_expr,
      SgNode **node);
  virtual bool VisitSubstNonTypeTemplateParmPackExpr(
      clang::SubstNonTypeTemplateParmPackExpr
          *subst_non_type_template_parm_pack_expr,
      SgNode **node);
  virtual bool VisitTypeTraitExpr(clang::TypeTraitExpr *type_trait,
                                  SgNode **node);
  // TypoExpr was removed in LLVM 20
  // virtual bool VisitTypoExpr(clang::TypoExpr * typo_expr, SgNode ** node);
  virtual bool VisitUnaryExprOrTypeTraitExpr(
      clang::UnaryExprOrTypeTraitExpr *unary_expr_or_type_trait_expr,
      SgNode **node);
  virtual bool VisitUnaryOperator(clang::UnaryOperator *unary_operator,
                                  SgNode **node);
  virtual bool VisitVAArgExpr(clang::VAArgExpr *va_arg_expr, SgNode **node);
  virtual bool VisitLabelStmt(clang::LabelStmt *label_stmt, SgNode **node);
  virtual bool VisitWhileStmt(clang::WhileStmt *while_stmt, SgNode **node);

  /*
     Reference: clang/AST/Type.h
     Overall 58 type AST nodes according as 02/19/2019
  */

  virtual bool VisitType(clang::Type *type, SgNode **node);
  virtual bool VisitAdjustedType(clang::AdjustedType *adjusted_type,
                                 SgNode **node);
  virtual bool VisitDecayedType(clang::DecayedType *decayed_type,
                                SgNode **node);
  virtual bool VisitArrayType(clang::ArrayType *array_type, SgNode **node);
  virtual bool
  VisitConstantArrayType(clang::ConstantArrayType *constant_array_type,
                         SgNode **node);
  virtual bool VisitDependentSizedArrayType(
      clang::DependentSizedArrayType *dependent_sized_array_type,
      SgNode **node);
  virtual bool
  VisitIncompleteArrayType(clang::IncompleteArrayType *incomplete_array_type,
                           SgNode **node);
  virtual bool
  VisitVariableArrayType(clang::VariableArrayType *variable_array_type,
                         SgNode **node);
  virtual bool VisitAtomicType(clang::AtomicType *atomic_type, SgNode **node);
  virtual bool VisitAttributedType(clang::AttributedType *attributed_type,
                                   SgNode **node);
  virtual bool
  VisitBlockPointerType(clang::BlockPointerType *block_pointer_type,
                        SgNode **node);
  virtual bool VisitBuiltinType(clang::BuiltinType *builtin_type,
                                SgNode **node);
  virtual bool VisitComplexType(clang::ComplexType *complex_type,
                                SgNode **node);
  virtual bool VisitDecltypeType(clang::DecltypeType *decltype_type,
                                 SgNode **node);
  virtual bool VisitDependentDecltypeType(
      clang::DependentDecltypeType *dependent_decltype_type, SgNode **node);
  virtual bool VisitDeducedType(clang::DeducedType *deduced_type,
                                SgNode **node);
  virtual bool VisitAutoType(clang::AutoType *auto_type, SgNode **node);
  virtual bool VisitDeducedTemplateSpecializationType(
      clang::DeducedTemplateSpecializationType
          *deduced_template_specialization_type,
      SgNode **node);
  virtual bool VisitDependentAddressSpaceType(
      clang::DependentAddressSpaceType *dependent_address_space_type,
      SgNode **node);
  virtual bool VisitDependentSizedExtVectorType(
      clang::DependentSizedExtVectorType *dependent_sized_ext_vector_type,
      SgNode **node);
  virtual bool
  VisitDependentVectorType(clang::DependentVectorType *dependent_vector_type,
                           SgNode **node);
  virtual bool VisitFunctionType(clang::FunctionType *function_type,
                                 SgNode **node);
  virtual bool
  VisitFunctionNoProtoType(clang::FunctionNoProtoType *function_no_proto_type,
                           SgNode **node);
  virtual bool
  VisitFunctionProtoType(clang::FunctionProtoType *function_proass_symo_type,
                         SgNode **node);
  virtual bool VisitInjectedClassNameType(
      clang::InjectedClassNameType *injected_class_name_type, SgNode **node);
  // LocInfoType was removed in LLVM 20
  // virtual bool VisitLocInfoType(clang::LocInfoType * loc_info_type, SgNode **
  // node);
  virtual bool
  VisitMacroQualifiedType(clang::MacroQualifiedType *macro_qualified_type,
                          SgNode **node);
  virtual bool
  VisitMemberPointerType(clang::MemberPointerType *member_pointer_type,
                         SgNode **node);
  // virtual bool VisitObjCObjectPointerType
  // virtual bool VisitObjCObjectType
  // virtual bool VisitObjCTypeParamType
  virtual bool
  VisitPackExpansionType(clang::PackExpansionType *pack_expansion_type,
                         SgNode **node);
  virtual bool VisitParenType(clang::ParenType *paren_type, SgNode **node);
  virtual bool VisitPipeType(clang::PipeType *pipe_type, SgNode **node);
  virtual bool VisitPointerType(clang::PointerType *pointer_type,
                                SgNode **node);
  virtual bool VisitReferenceType(clang::ReferenceType *reference_type,
                                  SgNode **node);
  virtual bool
  VisitLValueReferenceType(clang::LValueReferenceType *lvalue_reference_type,
                           SgNode **node);
  virtual bool
  VisitRValueReferenceType(clang::RValueReferenceType *rvalue_reference_type,
                           SgNode **node);
  virtual bool VisitSubstTemplateTypeParmPackType(
      clang::SubstTemplateTypeParmPackType *subst_template_type_parm_pack_type,
      SgNode **node);
  virtual bool VisitSubstTemplateTypeParmType(
      clang::SubstTemplateTypeParmType *subst_template_type_parm_type,
      SgNode **node);
  virtual bool VisitTagType(clang::TagType *tag_type, SgNode **node);
  virtual bool VisitEnumType(clang::EnumType *enum_type, SgNode **node);
  virtual bool VisitRecordType(clang::RecordType *record_type, SgNode **node);
  virtual bool VisitTemplateSpecializationType(
      clang::TemplateSpecializationType *template_specialization_type,
      SgNode **node);
  virtual bool VisitTemplateTypeParmType(
      clang::TemplateTypeParmType *template_type_parm_type, SgNode **node);
  virtual bool VisitTypedefType(clang::TypedefType *typedef_type,
                                SgNode **node);
  virtual bool VisitTypeOfExprType(clang::TypeOfExprType *type_of_expr_type,
                                   SgNode **node);
  virtual bool VisitDependentTypeOfExprType(
      clang::DependentTypeOfExprType *dependent_type_of_expr_type,
      SgNode **node);
  virtual bool VisitTypeOfType(clang::TypeOfType *type_of_type, SgNode **node);
  virtual bool VisitTypeWithKeyword(clang::TypeWithKeyword *type_with_keyword,
                                    SgNode **node);
  virtual bool
  VisitDependentNameType(clang::DependentNameType *dependent_name_type,
                         SgNode **node);
  virtual bool VisitDependentTemplateSpecializationType(
      clang::DependentTemplateSpecializationType
          *dependent_template_specialization_type,
      SgNode **node);
  virtual bool VisitElaboratedType(clang::ElaboratedType *elaborated_type,
                                   SgNode **node);
  virtual bool
  VisitUnaryTransformType(clang::UnaryTransformType *unary_transform_type,
                          SgNode **node);
  // DependentUnaryTransformType was removed/renamed in LLVM 20
  // virtual bool
  // VisitDependentUnaryTransformType(clang::DependentUnaryTransformType *
  // dependent_unary_transform_type, SgNode ** node);
  virtual bool
  VisitUnresolvedUsingType(clang::UnresolvedUsingType *unresolved_using_type,
                           SgNode **node);
  virtual bool VisitVectorType(clang::VectorType *vector_type, SgNode **node);
  virtual bool VisitExtVectorType(clang::ExtVectorType *ext_vector_type,
                                  SgNode **node);
  virtual bool VisitUsingType(clang::UsingType *using_type, SgNode **node);

  // Preprocessing access
  std::pair<Sg_File_Info *, PreprocessingInfo *> preprocessor_top();
  bool preprocessor_pop();
  size_t preprocessor_list_size();

  SgAsmOp::asm_operand_modifier_enum
  get_sgAsmOperandModifier(std::string modifier);
  SgAsmOp::asm_operand_constraint_enum
  get_sgAsmOperandConstraint(std::string constraint);
  SgInitializedName::asm_register_name_enum get_sgAsmRegister(std::string reg);

  std::string generate_source_position_string(clang::SourceLocation srcLoc);
  std::string generate_name_for_variable(clang::Stmt *stmt);
  std::string generate_name_for_type(clang::TypeSourceInfo *typeInfo);
};

void finishSageAST(ClangToSageTranslator &translator);

class SagePreprocessorRecord : public clang::PPCallbacks {
public:
protected:
  clang::SourceManager *p_source_manager;

  std::vector<std::pair<Sg_File_Info *, PreprocessingInfo *>>
      p_preprocessor_record_list;

public:
  SagePreprocessorRecord(clang::SourceManager *source_manager);

  void InclusionDirective(clang::SourceLocation HashLoc,
                          const clang::Token &IncludeTok,
                          llvm::StringRef FileName, bool IsAngled,
                          const clang::FileEntry *File,
                          clang::SourceLocation EndLoc,
                          llvm::StringRef SearchPath,
                          llvm::StringRef RelativePath);
  void EndOfMainFile();
  void Ident(clang::SourceLocation Loc, const std::string &str);
  void PragmaComment(clang::SourceLocation Loc,
                     const clang::IdentifierInfo *Kind, const std::string &Str);
  void PragmaMessage(clang::SourceLocation Loc, llvm::StringRef Str);
  void PragmaDiagnosticPush(clang::SourceLocation Loc,
                            llvm::StringRef Namespace);
  void PragmaDiagnosticPop(clang::SourceLocation Loc,
                           llvm::StringRef Namespace);
  void PragmaDiagnostic(clang::SourceLocation Loc, llvm::StringRef Namespace,
                        clang::diag::Severity Severity, llvm::StringRef Str);
  void MacroExpands(const clang::Token &MacroNameTok,
                    const clang::MacroInfo *MI, clang::SourceRange Range);
  void MacroDefined(const clang::Token &MacroNameTok,
                    const clang::MacroInfo *MI);
  void MacroUndefined(const clang::Token &MacroNameTok,
                      const clang::MacroInfo *MI);
  void Defined(const clang::Token &MacroNameTok);
  void SourceRangeSkipped(clang::SourceRange Range);
  void If(clang::SourceRange Range);
  void Elif(clang::SourceRange Range);
  void Ifdef(const clang::Token &MacroNameTok);
  void Ifndef(const clang::Token &MacroNameTok);
  void Else();
  void Endif();

  std::pair<Sg_File_Info *, PreprocessingInfo *> top();
  bool pop();
  size_t size() const { return p_preprocessor_record_list.size(); }
};

struct NextPreprocessorToInsert {
  Sg_File_Info *cursor;
  SgLocatedNode *candidat;
  PreprocessingInfo *next_to_insert;
  ClangToSageTranslator &translator;

  NextPreprocessorToInsert(ClangToSageTranslator &);

  NextPreprocessorToInsert *next();
};

class PreprocessorInserter
    : public AstTopDownProcessing<NextPreprocessorToInsert *> {
public:
  NextPreprocessorToInsert *
  evaluateInheritedAttribute(SgNode *astNode,
                             NextPreprocessorToInsert *inheritedValue);

private:
  std::vector<std::unique_ptr<NextPreprocessorToInsert>> owned_inherited_;
};

#endif /* _CLANG_FRONTEND_PRIVATE_HPP_ */
