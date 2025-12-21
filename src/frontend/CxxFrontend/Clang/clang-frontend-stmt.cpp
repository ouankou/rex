#include "clang-frontend-private.hpp"
#include "clang-to-rose-support.hpp"
#include "sage3basic.h"
#include <algorithm>
#include <cctype>
#include <regex>
#include <utility>
#include <vector>

#include "sageInterface.h"
#include "clang/AST/LambdaCapture.h"
#include "clang/Lex/Lexer.h"

using llvm::isa; // For LLVM type checking (isa<Type>)

namespace {

static bool canLexTrailingToken(clang::SourceLocation loc,
                                clang::SourceManager &sm) {
  if (!loc.isValid() || loc.isMacroID()) {
    return false;
  }

  clang::FileID file_id = sm.getFileID(loc);
  if (file_id.isInvalid() || file_id != sm.getMainFileID()) {
    return false;
  }

  auto buffer = sm.getBufferDataOrNone(file_id);
  if (!buffer) {
    return false;
  }

  unsigned offset = sm.getFileOffset(loc);
  if (offset >= buffer->size()) {
    return false;
  }

  bool invalid = false;
  (void)sm.getCharacterData(loc, &invalid);
  return !invalid;
}

clang::SourceRange
extendSourceRangeWithTrailingSemicolon(clang::SourceRange range,
                                       clang::SourceManager &sm,
                                       const clang::LangOptions &lang_opts) {
  if (!range.isValid()) {
    return range;
  }

  clang::SourceLocation end = range.getEnd();
  if (!end.isValid()) {
    return range;
  }
  if (end.isMacroID()) {
    end = sm.getExpansionLoc(end);
  }
  if (!end.isValid()) {
    return range;
  }

  if (!canLexTrailingToken(end, sm)) {
    return range;
  }

  // Lex the next non-whitespace token after the end token.
  clang::SourceLocation after_end =
      clang::Lexer::getLocForEndOfToken(end, 0, sm, lang_opts);
  if (!after_end.isValid()) {
    return range;
  }

  clang::Token tok;
  if (clang::Lexer::getRawToken(after_end, tok, sm, lang_opts,
                                /*IgnoreWhiteSpace=*/true)) {
    return range;
  }
  if (!tok.is(clang::tok::semi)) {
    return range;
  }

  range.setEnd(tok.getLocation());
  return range;
}

std::string getFloatingLiteralSpelling(const clang::FloatingLiteral *literal,
                                       clang::SourceManager &sm,
                                       const clang::LangOptions &lang_opts) {
  if (literal == NULL) {
    return "";
  }

  clang::SourceLocation loc = literal->getLocation();
  if (!loc.isValid()) {
    return "";
  }

  loc = sm.getSpellingLoc(loc);
  if (loc.isValid()) {
    llvm::SmallString<64> buf;
    bool invalid = false;
    llvm::StringRef spelling =
        clang::Lexer::getSpelling(loc, buf, sm, lang_opts, &invalid);
    if (!invalid && !spelling.empty()) {
      return spelling.str();
    }
  }

  clang::SourceRange range = literal->getSourceRange();
  if (!range.isValid()) {
    return "";
  }

  clang::SourceLocation begin = sm.getSpellingLoc(range.getBegin());
  clang::SourceLocation end = sm.getSpellingLoc(range.getEnd());
  if (!begin.isValid() || !end.isValid()) {
    return "";
  }

  bool invalid = false;
  llvm::StringRef text = clang::Lexer::getSourceText(
      clang::CharSourceRange::getTokenRange(begin, end), sm, lang_opts,
      &invalid);
  if (invalid) {
    return "";
  }

  return text.str();
}

std::string getIntegerLiteralSpelling(const clang::IntegerLiteral *literal,
                                      clang::SourceManager &sm,
                                      const clang::LangOptions &lang_opts) {
  if (literal == NULL) {
    return "";
  }

  clang::SourceLocation loc = literal->getLocation();
  if (!loc.isValid()) {
    return "";
  }

  loc = sm.getSpellingLoc(loc);
  if (loc.isValid()) {
    llvm::SmallString<64> buf;
    bool invalid = false;
    llvm::StringRef spelling =
        clang::Lexer::getSpelling(loc, buf, sm, lang_opts, &invalid);
    if (!invalid && !spelling.empty()) {
      return spelling.str();
    }
  }

  clang::SourceRange range = literal->getSourceRange();
  if (!range.isValid()) {
    return "";
  }

  clang::SourceLocation begin = sm.getSpellingLoc(range.getBegin());
  clang::SourceLocation end = sm.getSpellingLoc(range.getEnd());
  if (!begin.isValid() || !end.isValid()) {
    return "";
  }

  bool invalid = false;
  llvm::StringRef text = clang::Lexer::getSourceText(
      clang::CharSourceRange::getTokenRange(begin, end), sm, lang_opts,
      &invalid);
  if (invalid) {
    return "";
  }

  return text.str();
}

SgSymbol *findEnclosingThisSymbol(SgScopeStatement *starting_scope) {
  for (SgNode *node = starting_scope; node != NULL; node = node->get_parent()) {
    SgClassDefinition *class_def = isSgClassDefinition(node);
    if (class_def == NULL) {
      continue;
    }

    SgClassDeclaration *class_decl = class_def->get_declaration();
    ROSE_ASSERT(class_decl != NULL);
    SgScopeStatement *decl_scope = class_decl->get_scope();
    ROSE_ASSERT(decl_scope != NULL);

    if (SgClassSymbol *class_sym =
            SageInterface::lookupClassSymbolInParentScopes(
                class_decl->get_name(), decl_scope)) {
      return class_sym;
    }

    if (SgClassSymbol *class_sym =
            decl_scope->lookup_class_symbol(class_decl->get_name())) {
      return class_sym;
    }

    if (SgSymbol *sym = class_decl->search_for_symbol_from_symbol_table()) {
      if (isSgClassSymbol(sym) != NULL || isSgNonrealSymbol(sym) != NULL) {
        return sym;
      }
    }

    break;
  }

  return NULL;
}

std::string trimWhitespace(const std::string &input) {
  size_t start = 0;
  while (start < input.size() &&
         (input[start] == ' ' || input[start] == '\t')) {
    ++start;
  }
  size_t end = input.size();
  while (end > start && (input[end - 1] == ' ' || input[end - 1] == '\t')) {
    --end;
  }
  return input.substr(start, end - start);
}

std::string
printNestedNameSpecifier(const clang::NestedNameSpecifier *specifier,
                         const clang::ASTContext &context) {
  if (specifier == nullptr) {
    return "";
  }
  std::string buffer;
  llvm::raw_string_ostream stream(buffer);
  clang::PrintingPolicy policy(context.getLangOpts());
  policy.SuppressScope = false;
  policy.SuppressTagKeyword = true;
  specifier->print(stream, policy);
  stream.flush();
  return buffer;
}

std::string
deriveNestedNameSpecifierToken(const clang::NestedNameSpecifier *specifier,
                               const clang::ASTContext &context) {
  if (specifier == nullptr) {
    return "";
  }

  std::string full_text = printNestedNameSpecifier(specifier, context);
  if (full_text.empty()) {
    return "";
  }

  std::string prefix_text;
  if (const clang::NestedNameSpecifier *prefix = specifier->getPrefix()) {
    prefix_text = printNestedNameSpecifier(prefix, context);
  }

  std::string token = full_text;
  if (!prefix_text.empty()) {
    if (token.rfind(prefix_text, 0) != 0) {
      return "";
    }
    token = token.substr(prefix_text.size());
  }

  token = trimWhitespace(token);
  while (token.rfind("::", 0) == 0) {
    token = token.substr(2);
  }
  while (token.size() >= 2 && token.compare(token.size() - 2, 2, "::") == 0) {
    token.erase(token.size() - 2);
  }
  token = trimWhitespace(token);
  return token;
}

struct ExplicitQualifierInfo {
  int depth = 0;
  bool has_global = false;
  SgStringList tokens;
};

ExplicitQualifierInfo
getExplicitQualifierInfo(const clang::NestedNameSpecifier *qualifier,
                         const clang::ASTContext &context) {
  ExplicitQualifierInfo info;
  bool tokens_valid = true;
  std::vector<std::string> reversed_tokens;
  for (const clang::NestedNameSpecifier *nns = qualifier; nns != nullptr;
       nns = nns->getPrefix()) {
    if (nns->getKind() == clang::NestedNameSpecifier::Global) {
      info.has_global = true;
      continue;
    }
    ++info.depth;
    std::string token;
    bool used_print_fallback = false;
    switch (nns->getKind()) {
    case clang::NestedNameSpecifier::Namespace: {
      const clang::NamespaceDecl *ns = nns->getAsNamespace();
      if (ns != nullptr) {
        if (!ns->isAnonymousNamespace()) {
          token = ns->getNameAsString();
        }
      }
      break;
    }
    case clang::NestedNameSpecifier::NamespaceAlias: {
      const clang::NamespaceAliasDecl *alias = nns->getAsNamespaceAlias();
      if (alias != nullptr) {
        token = alias->getNameAsString();
      }
      break;
    }
    case clang::NestedNameSpecifier::Identifier: {
      const clang::IdentifierInfo *ident = nns->getAsIdentifier();
      if (ident != nullptr) {
        token = ident->getName().str();
      }
      break;
    }
    case clang::NestedNameSpecifier::TypeSpec:
    case clang::NestedNameSpecifier::TypeSpecWithTemplate: {
      const clang::Type *type = nns->getAsType();
      if (const clang::ElaboratedType *elab =
              llvm::dyn_cast_or_null<clang::ElaboratedType>(type)) {
        type = elab->getNamedType().getTypePtr();
      }
      if (const clang::TypedefType *typedef_type =
              llvm::dyn_cast_or_null<clang::TypedefType>(type)) {
        token = typedef_type->getDecl()->getNameAsString();
      } else if (const clang::UsingType *using_type =
                     llvm::dyn_cast_or_null<clang::UsingType>(type)) {
        clang::UsingShadowDecl *using_shadow = using_type->getFoundDecl();
        if (using_shadow != nullptr) {
          token = using_shadow->getNameAsString();
        }
      } else if (const clang::RecordType *record_type =
                     llvm::dyn_cast_or_null<clang::RecordType>(type)) {
        token = record_type->getDecl()->getNameAsString();
      } else if (const clang::EnumType *enum_type =
                     llvm::dyn_cast_or_null<clang::EnumType>(type)) {
        token = enum_type->getDecl()->getNameAsString();
      } else if (const clang::InjectedClassNameType *injected_type =
                     llvm::dyn_cast_or_null<clang::InjectedClassNameType>(
                         type)) {
        token = injected_type->getDecl()->getNameAsString();
      } else if (type != nullptr) {
        clang::QualType qual_type(type, 0);
        clang::PrintingPolicy policy(context.getLangOpts());
        policy.SuppressScope = true;
        policy.SuppressTagKeyword = true;
        token = qual_type.getAsString(policy);
      }
      break;
    }
    case clang::NestedNameSpecifier::Super:
      token = "super";
      break;
    case clang::NestedNameSpecifier::Global:
      break;
    }
    if (token.empty()) {
      token = deriveNestedNameSpecifierToken(nns, context);
      used_print_fallback = !token.empty();
    }
    if (!token.empty() && !used_print_fallback &&
        nns->getKind() == clang::NestedNameSpecifier::TypeSpecWithTemplate) {
      token = "template " + token;
    }
    if (token.empty()) {
      tokens_valid = false;
    } else {
      reversed_tokens.push_back(token);
    }
  }
  if (tokens_valid && !reversed_tokens.empty()) {
    for (auto it = reversed_tokens.rbegin(); it != reversed_tokens.rend();
         ++it) {
      info.tokens.push_back(*it);
    }
  }
  return info;
}

template <typename T>
void setExplicitQualifierOnRef(T *ref, const ExplicitQualifierInfo &info) {
  if (ref == nullptr) {
    return;
  }
  ref->set_explicit_name_qualification_length(info.depth);
  ref->set_explicit_global_qualification(info.has_global);
  if (!info.tokens.empty()) {
    ref->set_explicit_name_qualification_tokens(info.tokens);
  }
}

void setExplicitQualifierOnExpr(SgExpression *expr,
                                const ExplicitQualifierInfo &info) {
  if (expr == nullptr || (info.depth == 0 && !info.has_global)) {
    return;
  }
  if (SgVarRefExp *var_ref = isSgVarRefExp(expr)) {
    setExplicitQualifierOnRef(var_ref, info);
  } else if (SgTemplateMemberFunctionRefExp *tmpl_member =
                 isSgTemplateMemberFunctionRefExp(expr)) {
    setExplicitQualifierOnRef(tmpl_member, info);
  } else if (SgMemberFunctionRefExp *member_ref =
                 isSgMemberFunctionRefExp(expr)) {
    setExplicitQualifierOnRef(member_ref, info);
  } else if (SgTemplateFunctionRefExp *tmpl_func =
                 isSgTemplateFunctionRefExp(expr)) {
    setExplicitQualifierOnRef(tmpl_func, info);
  } else if (SgFunctionRefExp *func_ref = isSgFunctionRefExp(expr)) {
    setExplicitQualifierOnRef(func_ref, info);
  } else if (SgNonrealRefExp *nonreal_ref = isSgNonrealRefExp(expr)) {
    setExplicitQualifierOnRef(nonreal_ref, info);
  } else if (SgEnumVal *enum_val = isSgEnumVal(expr)) {
    setExplicitQualifierOnRef(enum_val, info);
  }
}

SgScopeStatement *normalizeNamespaceScope(SgScopeStatement *scope) {
  if (scope == NULL) {
    return NULL;
  }

  SgNamespaceDefinitionStatement *ns_def =
      isSgNamespaceDefinitionStatement(scope);
  if (ns_def == NULL) {
    return scope;
  }

  SgNamespaceDeclarationStatement *ns_decl = ns_def->get_namespaceDeclaration();
  if (ns_decl == NULL) {
    return scope;
  }

  SgNamespaceDeclarationStatement *first_nondef =
      isSgNamespaceDeclarationStatement(
          ns_decl->get_firstNondefiningDeclaration());
  if (first_nondef == NULL) {
    return scope;
  }

  SgNamespaceDefinitionStatement *first_def = first_nondef->get_definition();
  return first_def != NULL ? first_def : scope;
}

bool isSkippableLineBetweenPragmaAndStatement(const std::string &trimmed_line) {
  if (trimmed_line.empty()) {
    return true;
  }

  if (trimmed_line.size() >= 2 && trimmed_line[0] == '/' &&
      trimmed_line[1] == '/') {
    return true;
  }
  if (trimmed_line.size() >= 2 && trimmed_line[0] == '/' &&
      trimmed_line[1] == '*') {
    return true;
  }
  if (!trimmed_line.empty() && trimmed_line[0] == '*') {
    return true;
  }
  if (trimmed_line.size() >= 2 &&
      trimmed_line[trimmed_line.size() - 2] == '*' &&
      trimmed_line.back() == '/') {
    return true;
  }

  if (!trimmed_line.empty() && trimmed_line.back() == '\\') {
    return true;
  }

  if (trimmed_line.back() == ':') {
    bool is_case = trimmed_line.rfind("case", 0) == 0;
    bool is_default = trimmed_line.rfind("default", 0) == 0;
    if (!is_case && !is_default) {
      bool valid_label = true;
      for (size_t i = 0; i + 1 < trimmed_line.size(); ++i) {
        char c = trimmed_line[i];
        if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_')) {
          valid_label = false;
          break;
        }
      }
      if (valid_label) {
        return true;
      }
    }
  }

  return false;
}

bool getLineContent(const clang::SourceManager &source_manager,
                    clang::FileID file_id, unsigned line, std::string &result) {
  if (line == 0) {
    return false;
  }
  clang::SourceLocation line_start =
      source_manager.translateLineCol(file_id, line, 1);
  if (!line_start.isValid()) {
    return false;
  }

  const char *begin = source_manager.getCharacterData(line_start);
  const char *current = begin;
  while (*current != '\n' && *current != '\r' && *current != '\0') {
    ++current;
  }

  result.assign(begin, current - begin);
  return true;
}

std::string extractOpenMPDirective(const std::string &pragma_text) {
  size_t pragma_pos = pragma_text.find("pragma");
  if (pragma_pos == std::string::npos) {
    return std::string();
  }

  size_t hash_pos = pragma_text.find('#');
  size_t spaces_after_hash = 0;
  if (hash_pos != std::string::npos && pragma_pos > hash_pos) {
    spaces_after_hash = pragma_pos - hash_pos - 1;
  }
  size_t after_pragma = pragma_pos + 6; // Skip "pragma"

  if (after_pragma < pragma_text.size() &&
      RoseOpenMPPragmaCallback::isWhitespace(pragma_text[after_pragma])) {
    ++after_pragma;
  }

  return std::string(spaces_after_hash, ' ') + pragma_text.substr(after_pragma);
}

} // namespace

namespace {
class RexNonrealFlagAttribute : public AstAttribute {
public:
  OwnershipPolicy getOwnershipPolicy() const override {
    return CONTAINER_OWNERSHIP;
  }

  AstAttribute *copy() const override { return new RexNonrealFlagAttribute(); }

  std::string attribute_class_name() const override {
    return "RexNonrealFlagAttribute";
  }

  std::string toString() override { return ""; }
};

const char kRexNonrealTemplateKeywordAttr[] = "rex_nonreal_template_keyword";
} // namespace

void ClangToSageTranslator::applySourceRangeWithTrailingSemicolon(
    SgNode *rose_node, const clang::Stmt *clang_stmt) {
  if (rose_node == nullptr || clang_stmt == nullptr ||
      p_compiler_instance == nullptr) {
    return;
  }

  clang::SourceRange range = clang_stmt->getSourceRange();
  range = extendSourceRangeWithTrailingSemicolon(
      range, p_compiler_instance->getSourceManager(),
      p_compiler_instance->getLangOpts());
  applySourceRange(rose_node, range);
}

SgNonrealRefExp *
ClangToSageTranslator::buildNonrealRefExpFromNestedNameSpecifier(
    clang::NestedNameSpecifier *qualifier, SgScopeStatement *scope,
    const SgName &terminalName, bool terminalHasTemplateKeyword,
    const SgTemplateArgumentPtrList *terminalTemplateArgs) {
  SgNonrealType *nrtype = buildNonrealTypeFromNestedNameSpecifier(
      qualifier, scope, terminalName, terminalTemplateArgs);
  ROSE_ASSERT(nrtype != nullptr);

  SgNonrealDecl *nrdecl = isSgNonrealDecl(nrtype->get_declaration());
  ROSE_ASSERT(nrdecl != nullptr);

  if (terminalHasTemplateKeyword) {
    nrdecl->setAttribute(kRexNonrealTemplateKeywordAttr,
                         new RexNonrealFlagAttribute());
  }

  SgNonrealSymbol *sym =
      isSgNonrealSymbol(nrdecl->get_symbol_from_symbol_table());
  ROSE_ASSERT(sym != nullptr);

  return SageBuilder::buildNonrealRefExp_nfi(sym);
}

SgNode *ClangToSageTranslator::Traverse(clang::Stmt *stmt) {
  if (stmt == NULL)
    return NULL;

  std::map<clang::Stmt *, SgNode *>::iterator it =
      p_stmt_translation_map.find(stmt);
  if (it != p_stmt_translation_map.end())
    return it->second;

  SgNode *result = NULL;
  bool ret_status = false;

  switch (stmt->getStmtClass()) {
  case clang::Stmt::GCCAsmStmtClass:
    ret_status = VisitGCCAsmStmt((clang::GCCAsmStmt *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::MSAsmStmtClass:
    ret_status = VisitMSAsmStmt((clang::MSAsmStmt *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::BreakStmtClass:
    ret_status = VisitBreakStmt((clang::BreakStmt *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::CapturedStmtClass:
    ret_status = VisitCapturedStmt((clang::CapturedStmt *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::CompoundStmtClass:
    ret_status = VisitCompoundStmt((clang::CompoundStmt *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::ContinueStmtClass:
    ret_status = VisitContinueStmt((clang::ContinueStmt *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::CoreturnStmtClass:
    ret_status = VisitCoreturnStmt((clang::CoreturnStmt *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::CXXCatchStmtClass:
    ret_status = VisitCXXCatchStmt((clang::CXXCatchStmt *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::CXXForRangeStmtClass:
    ret_status = VisitCXXForRangeStmt((clang::CXXForRangeStmt *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::CXXTryStmtClass:
    ret_status = VisitCXXTryStmt((clang::CXXTryStmt *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::DeclStmtClass:
    ret_status = VisitDeclStmt((clang::DeclStmt *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::DoStmtClass:
    ret_status = VisitDoStmt((clang::DoStmt *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::ForStmtClass:
    ret_status = VisitForStmt((clang::ForStmt *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::GotoStmtClass:
    ret_status = VisitGotoStmt((clang::GotoStmt *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::IfStmtClass:
    ret_status = VisitIfStmt((clang::IfStmt *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::IndirectGotoStmtClass:
    ret_status =
        VisitIndirectGotoStmt((clang::IndirectGotoStmt *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::MSDependentExistsStmtClass:
    ret_status = VisitMSDependentExistsStmt(
        (clang::MSDependentExistsStmt *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::NullStmtClass:
    ret_status = VisitNullStmt((clang::NullStmt *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::OMPAtomicDirectiveClass:
    ret_status =
        VisitOMPAtomicDirective((clang::OMPAtomicDirective *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::OMPBarrierDirectiveClass:
    ret_status =
        VisitOMPBarrierDirective((clang::OMPBarrierDirective *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::OMPCancellationPointDirectiveClass:
    ret_status = VisitOMPCancellationPointDirective(
        (clang::OMPCancellationPointDirective *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::OMPCriticalDirectiveClass:
    ret_status =
        VisitOMPCriticalDirective((clang::OMPCriticalDirective *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::OMPFlushDirectiveClass:
    ret_status =
        VisitOMPFlushDirective((clang::OMPFlushDirective *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::OMPDistributeDirectiveClass:
    ret_status = VisitOMPDistributeDirective(
        (clang::OMPDistributeDirective *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::OMPDistributeParallelForDirectiveClass:
    ret_status = VisitOMPDistributeParallelForDirective(
        (clang::OMPDistributeParallelForDirective *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::OMPDistributeParallelForSimdDirectiveClass:
    ret_status = VisitOMPDistributeParallelForSimdDirective(
        (clang::OMPDistributeParallelForSimdDirective *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::OMPDistributeSimdDirectiveClass:
    ret_status = VisitOMPDistributeSimdDirective(
        (clang::OMPDistributeSimdDirective *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::OMPForDirectiveClass:
    ret_status = VisitOMPForDirective((clang::OMPForDirective *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::OMPForSimdDirectiveClass:
    ret_status =
        VisitOMPForSimdDirective((clang::OMPForSimdDirective *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  // case clang::Stmt::OMPMasterTaskLoopDirectiveClass:
  //     ret_status =
  //     VisitOMPMasterTaskLoopDirective((clang::OMPMasterTaskLoopDirective
  //     *)stmt, &result); break;
  // case clang::Stmt::OMPMasterTaskLoopSimdDirectiveClass:
  //     ret_status =
  //     VisitOMPMasterTaskLoopSimdDirective((clang::OMPMasterTaskLoopSimdDirective
  //     *)stmt, &result); break;
  case clang::Stmt::OMPParallelForDirectiveClass:
    ret_status = VisitOMPParallelForDirective(
        (clang::OMPParallelForDirective *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::OMPParallelForSimdDirectiveClass:
    ret_status = VisitOMPParallelForSimdDirective(
        (clang::OMPParallelForSimdDirective *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  // case clang::Stmt::OMPParallelMasterTaskLoopDirectiveClass:
  //     ret_status =
  //     VisitOMPParallelMasterTaskLoopDirective((clang::OMPParallelMasterTaskLoopDirective
  //     *)stmt, &result); break;
  case clang::Stmt::OMPSimdDirectiveClass:
    ret_status =
        VisitOMPSimdDirective((clang::OMPSimdDirective *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::OMPTargetParallelForDirectiveClass:
    ret_status = VisitOMPTargetParallelForDirective(
        (clang::OMPTargetParallelForDirective *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::OMPTargetParallelForSimdDirectiveClass:
    ret_status = VisitOMPTargetParallelForSimdDirective(
        (clang::OMPTargetParallelForSimdDirective *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::OMPTargetSimdDirectiveClass:
    ret_status = VisitOMPTargetSimdDirective(
        (clang::OMPTargetSimdDirective *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::OMPTargetTeamsDistributeDirectiveClass:
    ret_status = VisitOMPTargetTeamsDistributeDirective(
        (clang::OMPTargetTeamsDistributeDirective *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  // case clang::Stmt::OMPTargetTeamsDistributeParallelForSimdDirectiveClass:
  //     ret_status =
  //     VisitOMPTargetTeamsDistributeParallelForSimdDirective((clang::OMPTargetTeamsDistributeParallelForSimdDirective
  //     *)stmt, &result); break;
  case clang::Stmt::OMPTargetTeamsDistributeSimdDirectiveClass:
    ret_status = VisitOMPTargetTeamsDistributeSimdDirective(
        (clang::OMPTargetTeamsDistributeSimdDirective *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::OMPTaskLoopDirectiveClass:
    ret_status =
        VisitOMPTaskLoopDirective((clang::OMPTaskLoopDirective *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::OMPTaskLoopSimdDirectiveClass:
    ret_status = VisitOMPTaskLoopSimdDirective(
        (clang::OMPTaskLoopSimdDirective *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  // case clang::Stmt::OMPTeamDistributeDirectiveClass:
  //     ret_status =
  //     VisitOMPTeamDistributeDirective((clang::OMPTeamDistributeDirective
  //     *)stmt, &result); break;
  // case clang::Stmt::OMPTeamDistributeParallelForSimdDirectiveClass:
  //     ret_status =
  //     VisitOMPTeamDistributeParallelForSimdDirective((clang::OMPTeamDistributeParallelForSimdDirective
  //     *)stmt, &result); break;
  // case clang::Stmt::OMPTeamDistributeSimdDirectiveClass:
  //     ret_status =
  //     VisitOMPTeamDistributeSimdDirective((clang::OMPTeamDistributeSimdDirective
  //     *)stmt, &result); break;
  case clang::Stmt::OMPMasterDirectiveClass:
    ret_status =
        VisitOMPMasterDirective((clang::OMPMasterDirective *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::OMPOrderedDirectiveClass:
    ret_status =
        VisitOMPOrderedDirective((clang::OMPOrderedDirective *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::OMPParallelDirectiveClass:
    ret_status =
        VisitOMPParallelDirective((clang::OMPParallelDirective *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::OMPParallelSectionsDirectiveClass:
    ret_status = VisitOMPParallelSectionsDirective(
        (clang::OMPParallelSectionsDirective *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::ReturnStmtClass:
    ret_status = VisitReturnStmt((clang::ReturnStmt *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::SEHExceptStmtClass:
    ret_status = VisitSEHExceptStmt((clang::SEHExceptStmt *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::SEHFinallyStmtClass:
    ret_status = VisitSEHFinallyStmt((clang::SEHFinallyStmt *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::SEHLeaveStmtClass:
    ret_status = VisitSEHLeaveStmt((clang::SEHLeaveStmt *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::SEHTryStmtClass:
    ret_status = VisitSEHTryStmt((clang::SEHTryStmt *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::CaseStmtClass:
    ret_status = VisitCaseStmt((clang::CaseStmt *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::DefaultStmtClass:
    ret_status = VisitDefaultStmt((clang::DefaultStmt *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::SwitchStmtClass:
    ret_status = VisitSwitchStmt((clang::SwitchStmt *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::AttributedStmtClass:
    ret_status = VisitAttributedStmt((clang::AttributedStmt *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::BinaryConditionalOperatorClass:
    ret_status = VisitBinaryConditionalOperator(
        (clang::BinaryConditionalOperator *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::ConditionalOperatorClass:
    ret_status =
        VisitConditionalOperator((clang::ConditionalOperator *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::AddrLabelExprClass:
    ret_status = VisitAddrLabelExpr((clang::AddrLabelExpr *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::ArrayInitIndexExprClass:
    ret_status =
        VisitArrayInitIndexExpr((clang::ArrayInitIndexExpr *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::ArrayInitLoopExprClass:
    ret_status =
        VisitArrayInitLoopExpr((clang::ArrayInitLoopExpr *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::ArraySubscriptExprClass:
    ret_status =
        VisitArraySubscriptExpr((clang::ArraySubscriptExpr *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::ArrayTypeTraitExprClass:
    ret_status =
        VisitArrayTypeTraitExpr((clang::ArrayTypeTraitExpr *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::AsTypeExprClass:
    ret_status = VisitAsTypeExpr((clang::AsTypeExpr *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::AtomicExprClass:
    ret_status = VisitAtomicExpr((clang::AtomicExpr *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::CompoundAssignOperatorClass:
    ret_status = VisitCompoundAssignOperator(
        (clang::CompoundAssignOperator *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::BlockExprClass:
    ret_status = VisitBlockExpr((clang::BlockExpr *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::CUDAKernelCallExprClass:
    ret_status =
        VisitCUDAKernelCallExpr((clang::CUDAKernelCallExpr *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::CXXMemberCallExprClass:
    ret_status =
        VisitCXXMemberCallExpr((clang::CXXMemberCallExpr *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::CXXOperatorCallExprClass:
    ret_status =
        VisitCXXOperatorCallExpr((clang::CXXOperatorCallExpr *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::UserDefinedLiteralClass:
    ret_status =
        VisitUserDefinedLiteral((clang::UserDefinedLiteral *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::BuiltinBitCastExprClass:
    ret_status =
        VisitBuiltinBitCastExpr((clang::BuiltinBitCastExpr *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::CStyleCastExprClass:
    ret_status = VisitCStyleCastExpr((clang::CStyleCastExpr *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::CXXFunctionalCastExprClass:
    ret_status = VisitCXXFunctionalCastExpr(
        (clang::CXXFunctionalCastExpr *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::CXXConstCastExprClass:
    ret_status =
        VisitCXXConstCastExpr((clang::CXXConstCastExpr *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::CXXDynamicCastExprClass:
    ret_status =
        VisitCXXDynamicCastExpr((clang::CXXDynamicCastExpr *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::CXXReinterpretCastExprClass:
    ret_status = VisitCXXReinterpretCastExpr(
        (clang::CXXReinterpretCastExpr *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::CXXStaticCastExprClass:
    ret_status =
        VisitCXXStaticCastExpr((clang::CXXStaticCastExpr *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::ImplicitCastExprClass:
    ret_status =
        VisitImplicitCastExpr((clang::ImplicitCastExpr *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::CharacterLiteralClass:
    ret_status =
        VisitCharacterLiteral((clang::CharacterLiteral *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::ChooseExprClass:
    ret_status = VisitChooseExpr((clang::ChooseExpr *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::CompoundLiteralExprClass:
    ret_status =
        VisitCompoundLiteralExpr((clang::CompoundLiteralExpr *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  // case clang::Stmt::ConceptSpecializationExprClass:
  //     ret_status =
  //     VisitConceptSpecializationExpr((clang::ConceptSpecializationExpr
  //     *)stmt, &result); break;
  case clang::Stmt::ConvertVectorExprClass:
    ret_status =
        VisitConvertVectorExpr((clang::ConvertVectorExpr *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::CoawaitExprClass:
    ret_status = VisitCoawaitExpr((clang::CoawaitExpr *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::CoyieldExprClass:
    ret_status = VisitCoyieldExpr((clang::CoyieldExpr *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::CXXBindTemporaryExprClass:
    ret_status =
        VisitCXXBindTemporaryExpr((clang::CXXBindTemporaryExpr *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::CXXBoolLiteralExprClass:
    ret_status =
        VisitCXXBoolLiteralExpr((clang::CXXBoolLiteralExpr *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::CXXConstructExprClass:
    ret_status =
        VisitCXXConstructExpr((clang::CXXConstructExpr *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::CXXTemporaryObjectExprClass:
    ret_status = VisitCXXTemporaryObjectExpr(
        (clang::CXXTemporaryObjectExpr *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::CXXDefaultArgExprClass:
    ret_status =
        VisitCXXDefaultArgExpr((clang::CXXDefaultArgExpr *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::CXXDefaultInitExprClass:
    ret_status =
        VisitCXXDefaultInitExpr((clang::CXXDefaultInitExpr *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::CXXDeleteExprClass:
    ret_status = VisitCXXDeleteExpr((clang::CXXDeleteExpr *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::CXXDependentScopeMemberExprClass:
    ret_status = VisitCXXDependentScopeMemberExpr(
        (clang::CXXDependentScopeMemberExpr *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::CXXFoldExprClass:
    ret_status = VisitCXXFoldExpr((clang::CXXFoldExpr *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::CXXInheritedCtorInitExprClass:
    ret_status = VisitCXXInheritedCtorInitExpr(
        (clang::CXXInheritedCtorInitExpr *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::CXXNewExprClass:
    ret_status = VisitCXXNewExpr((clang::CXXNewExpr *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::CXXNoexceptExprClass:
    ret_status = VisitCXXNoexceptExpr((clang::CXXNoexceptExpr *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::CXXNullPtrLiteralExprClass:
    ret_status = VisitCXXNullPtrLiteralExpr(
        (clang::CXXNullPtrLiteralExpr *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::CXXPseudoDestructorExprClass:
    ret_status = VisitCXXPseudoDestructorExpr(
        (clang::CXXPseudoDestructorExpr *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  // case clang::Stmt::CXXRewrittenBinaryOperatorClass:
  //     ret_status =
  //     VisitCXXRewrittenBinaryOperator((clang::CXXRewrittenBinaryOperator
  //     *)stmt, &result); break;
  case clang::Stmt::CXXScalarValueInitExprClass:
    ret_status = VisitCXXScalarValueInitExpr(
        (clang::CXXScalarValueInitExpr *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::CXXStdInitializerListExprClass:
    ret_status = VisitCXXStdInitializerListExpr(
        (clang::CXXStdInitializerListExpr *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::CXXThisExprClass:
    ret_status = VisitCXXThisExpr((clang::CXXThisExpr *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::CXXThrowExprClass:
    ret_status = VisitCXXThrowExpr((clang::CXXThrowExpr *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::CXXTypeidExprClass:
    ret_status = VisitCXXTypeidExpr((clang::CXXTypeidExpr *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::CXXUnresolvedConstructExprClass:
    ret_status = VisitCXXUnresolvedConstructExpr(
        (clang::CXXUnresolvedConstructExpr *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::CXXUuidofExprClass:
    ret_status = VisitCXXUuidofExpr((clang::CXXUuidofExpr *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::DeclRefExprClass:
    ret_status = VisitDeclRefExpr((clang::DeclRefExpr *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::DependentCoawaitExprClass:
    ret_status =
        VisitDependentCoawaitExpr((clang::DependentCoawaitExpr *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::DependentScopeDeclRefExprClass:
    ret_status = VisitDependentScopeDeclRefExpr(
        (clang::DependentScopeDeclRefExpr *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::DesignatedInitExprClass:
    ret_status =
        VisitDesignatedInitExpr((clang::DesignatedInitExpr *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::DesignatedInitUpdateExprClass:
    ret_status = VisitDesignatedInitUpdateExpr(
        (clang::DesignatedInitUpdateExpr *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::ExpressionTraitExprClass:
    ret_status =
        VisitExpressionTraitExpr((clang::ExpressionTraitExpr *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::ExtVectorElementExprClass:
    ret_status =
        VisitExtVectorElementExpr((clang::ExtVectorElementExpr *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::FixedPointLiteralClass:
    ret_status =
        VisitFixedPointLiteral((clang::FixedPointLiteral *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::FloatingLiteralClass:
    ret_status = VisitFloatingLiteral((clang::FloatingLiteral *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::ConstantExprClass:
    ret_status = VisitConstantExpr((clang::ConstantExpr *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::ExprWithCleanupsClass:
    ret_status =
        VisitExprWithCleanups((clang::ExprWithCleanups *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::FunctionParmPackExprClass:
    ret_status =
        VisitFunctionParmPackExpr((clang::FunctionParmPackExpr *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::GenericSelectionExprClass:
    ret_status =
        VisitGenericSelectionExpr((clang::GenericSelectionExpr *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::GNUNullExprClass:
    ret_status = VisitGNUNullExpr((clang::GNUNullExpr *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::ImaginaryLiteralClass:
    ret_status =
        VisitImaginaryLiteral((clang::ImaginaryLiteral *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::ImplicitValueInitExprClass:
    ret_status = VisitImplicitValueInitExpr(
        (clang::ImplicitValueInitExpr *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::InitListExprClass:
    ret_status = VisitInitListExpr((clang::InitListExpr *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::IntegerLiteralClass:
    ret_status = VisitIntegerLiteral((clang::IntegerLiteral *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::LambdaExprClass:
    ret_status = VisitLambdaExpr((clang::LambdaExpr *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::MaterializeTemporaryExprClass:
    ret_status = VisitMaterializeTemporaryExpr(
        (clang::MaterializeTemporaryExpr *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::MemberExprClass:
    ret_status = VisitMemberExpr((clang::MemberExpr *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::MSPropertyRefExprClass:
    ret_status =
        VisitMSPropertyRefExpr((clang::MSPropertyRefExpr *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::MSPropertySubscriptExprClass:
    ret_status = VisitMSPropertySubscriptExpr(
        (clang::MSPropertySubscriptExpr *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::NoInitExprClass:
    ret_status = VisitNoInitExpr((clang::NoInitExpr *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::OffsetOfExprClass:
    ret_status = VisitOffsetOfExpr((clang::OffsetOfExpr *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::ArraySectionExprClass:
    ret_status =
        VisitOMPArraySectionExpr((clang::ArraySectionExpr *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::OpaqueValueExprClass:
    ret_status = VisitOpaqueValueExpr((clang::OpaqueValueExpr *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::UnresolvedLookupExprClass:
    ret_status =
        VisitUnresolvedLookupExpr((clang::UnresolvedLookupExpr *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::UnresolvedMemberExprClass:
    ret_status =
        VisitUnresolvedMemberExpr((clang::UnresolvedMemberExpr *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::PackExpansionExprClass:
    ret_status =
        VisitPackExpansionExpr((clang::PackExpansionExpr *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::ParenExprClass:
    ret_status = VisitParenExpr((clang::ParenExpr *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::ParenListExprClass:
    ret_status = VisitParenListExpr((clang::ParenListExpr *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::PredefinedExprClass:
    ret_status = VisitPredefinedExpr((clang::PredefinedExpr *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::PseudoObjectExprClass:
    ret_status =
        VisitPseudoObjectExpr((clang::PseudoObjectExpr *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::ShuffleVectorExprClass:
    ret_status =
        VisitShuffleVectorExpr((clang::ShuffleVectorExpr *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::SizeOfPackExprClass:
    ret_status = VisitSizeOfPackExpr((clang::SizeOfPackExpr *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::SourceLocExprClass:
    ret_status = VisitSourceLocExpr((clang::SourceLocExpr *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::StmtExprClass:
    ret_status = VisitStmtExpr((clang::StmtExpr *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::StringLiteralClass:
    ret_status = VisitStringLiteral((clang::StringLiteral *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::SubstNonTypeTemplateParmPackExprClass:
    ret_status = VisitSubstNonTypeTemplateParmPackExpr(
        (clang::SubstNonTypeTemplateParmPackExpr *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::SubstNonTypeTemplateParmExprClass:
    ret_status = VisitSubstNonTypeTemplateParmExpr(
        (clang::SubstNonTypeTemplateParmExpr *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::TypeTraitExprClass:
    ret_status = VisitTypeTraitExpr((clang::TypeTraitExpr *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  // TypoExpr was removed in LLVM 20
  // case clang::Stmt::TypoExprClass:
  //     ret_status = VisitTypoExpr((clang::TypoExpr *)stmt, &result);
  //     ROSE_ASSERT(result != NULL);
  //     break;
  case clang::Stmt::UnaryExprOrTypeTraitExprClass:
    ret_status = VisitUnaryExprOrTypeTraitExpr(
        (clang::UnaryExprOrTypeTraitExpr *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::VAArgExprClass:
    ret_status = VisitVAArgExpr((clang::VAArgExpr *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::LabelStmtClass:
    ret_status = VisitLabelStmt((clang::LabelStmt *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::WhileStmtClass:
    ret_status = VisitWhileStmt((clang::WhileStmt *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::UnaryOperatorClass:
    ret_status = VisitUnaryOperator((clang::UnaryOperator *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::CallExprClass:
    ret_status = VisitCallExpr((clang::CallExpr *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::BinaryOperatorClass:
    ret_status = VisitBinaryOperator((clang::BinaryOperator *)stmt, &result);
    ROSE_ASSERT(result != NULL);
    break;
  case clang::Stmt::RecoveryExprClass:
    // CLANG FRONTEND FIX: Use SgNullExpression instead of SgIntVal(42) for
    // RecoveryExpr WHY: Clang creates RecoveryExpr during parse errors or
    // incomplete template instantiations. Using SgIntVal(42) causes downstream
    // errors when it appears as a function in function calls. SgNullExpression
    // is better semantically as it represents a missing/unknown expression.
    result = SageBuilder::buildNullExpression();
    // Note: Assertion removed since RecoveryExpr is a valid (though error)
    // state during parsing ROSE_ASSERT(FAIL_FIXME == 0); // There is no concept
    // of recovery expression in ROSE
    break;

  default:
    std::cerr << "Unknown statement kind: " << stmt->getStmtClassName() << " !"
              << std::endl;
    ROSE_ABORT();
  }

  ROSE_ASSERT(result != NULL);

  p_stmt_translation_map.insert(
      std::pair<clang::Stmt *, SgNode *>(stmt, result));

  return result;
}

/********************/
/* Visit Statements */
/********************/

bool ClangToSageTranslator::VisitStmt(clang::Stmt *stmt, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitStmt" << std::endl;
#endif

  if (*node == NULL) {
    std::cerr << "Runtime error: No Sage node associated with the Statement: "
              << stmt->getStmtClassName() << std::endl;
    stmt->dump();
    return false;
  }

  // TODO Is there anything else todo?

  if (isSgLocatedNode(*node) != NULL &&
      (isSgLocatedNode(*node)->get_file_info() == NULL ||
       !(isSgLocatedNode(*node)->get_file_info()->isCompilerGenerated()))) {
    clang::SourceRange range = stmt->getSourceRange();

    // Token-stream mapping expects statement extents to cover the full spelled
    // statement, including terminating semicolons when present.
    switch (stmt->getStmtClass()) {
    case clang::Stmt::ReturnStmtClass:
    case clang::Stmt::BreakStmtClass:
    case clang::Stmt::ContinueStmtClass:
    case clang::Stmt::GotoStmtClass:
    case clang::Stmt::IndirectGotoStmtClass:
    case clang::Stmt::DoStmtClass:
      if (p_compiler_instance != nullptr) {
        range = extendSourceRangeWithTrailingSemicolon(
            range, p_compiler_instance->getSourceManager(),
            p_compiler_instance->getLangOpts());
      }
      break;
    default:
      break;
    }

    applySourceRange(*node, range);
  }

  return true;
}

bool ClangToSageTranslator::VisitAsmStmt(clang::AsmStmt *asm_stmt,
                                         SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitAsmStmt" << std::endl;
#endif
  bool res = true;

  ROSE_ASSERT(FAIL_TODO == 0); // TODO
  return VisitStmt(asm_stmt, node) && res;
}

bool ClangToSageTranslator::VisitGCCAsmStmt(clang::GCCAsmStmt *gcc_asm_stmt,
                                            SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitGCCAsmStmt" << std::endl;
#endif
  bool res = true;

  unsigned asmNumInput = gcc_asm_stmt->getNumInputs();
  unsigned asmNumOutput = gcc_asm_stmt->getNumOutputs();
  unsigned asmClobber = gcc_asm_stmt->getNumClobbers();

  // LLVM 20 returns StringLiteral*, LLVM 21 returns std::string
  std::string AsmString;
#if LLVM_VERSION_MAJOR >= 21
  AsmString = gcc_asm_stmt->getAsmString();
#else
  if (auto *str_lit = gcc_asm_stmt->getAsmString()) {
    AsmString = str_lit->getString().str();
  }
#endif

  std::cout << "input op:" << asmNumInput << " output op: " << asmNumOutput
            << std::endl;
#if DEBUG_VISIT_STMT
  std::cerr << "AsmString:" << AsmString << std::endl;
#endif

  SgAsmStmt *asmStmt = SageBuilder::buildAsmStatement(AsmString);
  asmStmt->set_firstNondefiningDeclaration(asmStmt);
  asmStmt->set_definingDeclaration(asmStmt);
  asmStmt->set_parent(SageBuilder::topScopeStack());
  asmStmt->set_useGnuExtendedFormat(true);

  // Pei-Hung (03/22/2022)  The clobber string is available.
  // The implementation adding clobber into ROSE AST is not in place.
  for (unsigned i = 0; i < asmClobber; ++i) {
    std::string clobberStr =
        static_cast<std::string>(gcc_asm_stmt->getClobber(i));
#if DEBUG_VISIT_STMT
    std::cerr << "AsmOp clobber[" << i << "]: " << clobberStr << std::endl;
#endif
    // Pei-Hung "cc" clobber is skipped by EDG
    if (clobberStr.compare(0, sizeof(clobberStr), "cc") == 0)
      continue;

    SgInitializedName::asm_register_name_enum sageRegisterName =
        get_sgAsmRegister(clobberStr);
    asmStmt->get_clobberRegisterList().push_back(sageRegisterName);
  }

  // Pei-Hung (03/22/2022) use regular expression to check the first modifier, +
  // and =, for ouput Ops. Then the second modifier for both input and output
  // Ops.  The rest is for constraints. regex_match should report 4 matched
  // results:
  // 1. the whole matched string
  // 2. first modifier: =, +, or empty
  // 3. second modifier: empty or &, %, *, #, ?, !
  // 4. The constraint
  std::regex e("([\\=\\+]*)([\\&\\%\\*\\#\\?\\!]*)(.+)",
               std::regex_constants::ECMAScript | std::regex_constants::icase);

  // process output
  for (unsigned i = 0; i < asmNumOutput; ++i) {
    SgNode *tmp_node = Traverse(gcc_asm_stmt->getOutputExpr(i));
    SgExpression *outputExpr = isSgExpression(tmp_node);
    ROSE_ASSERT(outputExpr != NULL);

    std::string outputConstraintStr =
        static_cast<std::string>(gcc_asm_stmt->getOutputConstraint(i));
// Clang's constraint is equivalent to ROSE's modifier + operand constraints
#if DEBUG_VISIT_STMT
    std::cerr << "AsmOp output constraint[" << i << "]: " << outputConstraintStr
              << std::endl;
#endif

    std::smatch sm;
    std::regex_match(outputConstraintStr, sm, e);
#if DEBUG_VISIT_STMT
    std::cout << "string literal: " << outputConstraintStr << "  with "
              << sm.size() << " matches\n";
    if (sm.size())
      std::cout << "the matches were: ";
    for (unsigned i = 0; i < sm.size(); ++i) {
      std::cout << "[" << sm[i] << "] \n";
    }
    if (sm.size())
      std::cout << std::endl;
#endif

    SgAsmOp::asm_operand_constraint_enum constraint =
        (SgAsmOp::asm_operand_constraint_enum)SgAsmOp::e_any;
    SgAsmOp::asm_operand_modifier_enum modifiers =
        (SgAsmOp::asm_operand_modifier_enum)SgAsmOp::e_unknown;
    SgAsmOp *sageAsmOp = new SgAsmOp(constraint, modifiers, outputExpr);
    outputExpr->set_parent(sageAsmOp);

    sageAsmOp->set_recordRawAsmOperandDescriptions(false);

    // set as an output AsmOp
    sageAsmOp->set_isOutputOperand(true);

    ROSE_ASSERT(sm.size() == 4);

    unsigned modifierVal = static_cast<int>(modifiers);
    if (!sm[1].str().empty())
      modifierVal += static_cast<int>(get_sgAsmOperandModifier(sm[1].str()));

    if (!sm[2].str().empty())
      modifierVal += static_cast<int>(get_sgAsmOperandModifier(sm[2].str()));

    sageAsmOp->set_modifiers(
        static_cast<SgAsmOp::asm_operand_modifier_enum>(modifierVal));

    // set constraint
    sageAsmOp->set_constraint(get_sgAsmOperandConstraint(sm[3].str()));
    sageAsmOp->set_constraintString(sm[3]);

    Sg_File_Info *start_fi =
        Sg_File_Info::generateDefaultFileInfoForCompilerGeneratedNode();
    start_fi->setCompilerGenerated();
    sageAsmOp->set_startOfConstruct(start_fi);

    Sg_File_Info *end_fi =
        Sg_File_Info::generateDefaultFileInfoForCompilerGeneratedNode();
    end_fi->setCompilerGenerated();
    sageAsmOp->set_endOfConstruct(end_fi);

    asmStmt->get_operands().push_back(sageAsmOp);
    sageAsmOp->set_parent(asmStmt);
  }

  // process input
  for (unsigned i = 0; i < asmNumInput; ++i) {
    SgNode *tmp_node = Traverse(gcc_asm_stmt->getInputExpr(i));
    SgExpression *inputExpr = isSgExpression(tmp_node);
    ROSE_ASSERT(inputExpr != NULL);

    std::string inputConstraintStr =
        static_cast<std::string>(gcc_asm_stmt->getInputConstraint(i));
// Clang's constraint is equivalent to ROSE's modifier + operand constraints
#if DEBUG_VISIT_STMT
    std::cerr << "AsmOp input constraint[" << i << "]: " << inputConstraintStr
              << std::endl;
#endif

    std::smatch sm;
    std::regex_match(inputConstraintStr, sm, e);
#if DEBUG_VISIT_STMT
    std::cout << "string literal: " << inputConstraintStr << "  with "
              << sm.size() << " matches\n";
    if (sm.size())
      std::cout << "the matches were: ";
    for (unsigned i = 0; i < sm.size(); ++i) {
      std::cout << "[" << sm[i] << "] \n";
    }
    if (sm.size())
      std::cout << std::endl;
#endif

    SgAsmOp::asm_operand_constraint_enum constraint =
        (SgAsmOp::asm_operand_constraint_enum)SgAsmOp::e_any;
    SgAsmOp::asm_operand_modifier_enum modifiers =
        (SgAsmOp::asm_operand_modifier_enum)SgAsmOp::e_unknown;
    SgAsmOp *sageAsmOp = new SgAsmOp(constraint, modifiers, inputExpr);
    inputExpr->set_parent(sageAsmOp);

    sageAsmOp->set_recordRawAsmOperandDescriptions(false);

    // set as an input AsmOp
    sageAsmOp->set_isOutputOperand(false);

    ROSE_ASSERT(sm.size() == 4);

    unsigned modifierVal = static_cast<int>(modifiers);

    // "+" and "=" should not be part of the input AsmOp.  Skip checking sm[1]
    // for the inputs.

    //      if(!sm[1].str().empty())
    //        modifierVal +=
    //        static_cast<int>(get_sgAsmOperandModifier(sm[1].str())); modifiers
    //        &= get_sgAsmOperandModifier(sm[1].str());

    if (!sm[2].str().empty())
      modifierVal += static_cast<int>(get_sgAsmOperandModifier(sm[2].str()));

    sageAsmOp->set_modifiers(
        static_cast<SgAsmOp::asm_operand_modifier_enum>(modifierVal));

    // set constraint
    sageAsmOp->set_constraint(get_sgAsmOperandConstraint(sm[3].str()));
    sageAsmOp->set_constraintString(sm[3]);

    Sg_File_Info *start_fi =
        Sg_File_Info::generateDefaultFileInfoForCompilerGeneratedNode();
    start_fi->setCompilerGenerated();
    sageAsmOp->set_startOfConstruct(start_fi);

    Sg_File_Info *end_fi =
        Sg_File_Info::generateDefaultFileInfoForCompilerGeneratedNode();
    end_fi->setCompilerGenerated();
    sageAsmOp->set_endOfConstruct(end_fi);

    asmStmt->get_operands().push_back(sageAsmOp);
    sageAsmOp->set_parent(asmStmt);
  }
  *node = asmStmt;

  return VisitStmt(gcc_asm_stmt, node) && res;
}

bool ClangToSageTranslator::VisitMSAsmStmt(clang::MSAsmStmt *ms_asm_stmt,
                                           SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitMSAsmStmt" << std::endl;
#endif
  bool res = true;

  return VisitStmt(ms_asm_stmt, node) && res;
}

bool ClangToSageTranslator::VisitBreakStmt(clang::BreakStmt *break_stmt,
                                           SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitBreakStmt" << std::endl;
#endif

  *node = SageBuilder::buildBreakStmt();
  return VisitStmt(break_stmt, node);
}

bool ClangToSageTranslator::VisitCapturedStmt(
    clang::CapturedStmt *captured_stmt, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitCapturedStmt" << std::endl;
#endif
  bool res = true;

  clang::Stmt *clang_body = captured_stmt->getCapturedStmt();
  SgNode *tmp_stmt = Traverse(clang_body);
  SgStatement *body = isSgStatement(tmp_stmt);

  // If the traversal returned an expression instead of a statement,
  // wrap it in an expression statement (this happens when the captured
  // region contains a single expression like a function call)
  if (body == NULL && tmp_stmt != NULL) {
    SgExpression *expr = isSgExpression(tmp_stmt);
    if (expr != NULL) {
      body = SageBuilder::buildExprStatement(expr);
      if (clang_body != NULL) {
        applySourceRange(body, clang_body->getSourceRange());
      }
    } else {
      std::cerr << "Runtime error: CapturedStmt child did not translate into "
                   "an SgStatement or SgExpression."
                << std::endl;
      res = false;
    }
  }

  if (body == NULL) {
    body = SageBuilder::buildNullStatement();
  }

  *node = body;

  return VisitStmt(captured_stmt, node) && res;
}

bool ClangToSageTranslator::collectOpenMPPragmas(
    clang::Stmt *stmt, std::vector<CapturedPragma> &pragmas) {
  pragmas.clear();

  if (p_openmp_pragma_callback == nullptr || stmt == nullptr ||
      p_compiler_instance == nullptr) {
    return false;
  }

  clang::SourceLocation loc = stmt->getBeginLoc();
  if (!loc.isValid()) {
    return false;
  }

  clang::SourceManager &source_manager =
      p_compiler_instance->getSourceManager();
  clang::FileID file_id = source_manager.getFileID(loc);
  unsigned stmt_line = source_manager.getPresumedLineNumber(loc);
  if (stmt_line == 0) {
    return false;
  }

  bool found_any = false;
  std::string pragma_text;

  bool continue_across_multiline_directive = false;

  for (unsigned search_line = stmt_line; search_line > 0; --search_line) {

    if (p_openmp_pragma_callback->getPragmaAtLine(file_id, search_line,
                                                  pragma_text)) {
      bool is_openmp =
          p_openmp_pragma_callback->isOpenMPPragmaAtLine(file_id, search_line);
      std::string directive_text = pragma_text;
      if (is_openmp) {
        std::string extracted = extractOpenMPDirective(pragma_text);
        if (extracted.empty()) {
          continue;
        }
        directive_text = extracted;
      }
      pragmas.push_back({search_line, directive_text, is_openmp});
      found_any = true;
      continue_across_multiline_directive = true;
      continue;
    }

    std::string line_content;
    if (!getLineContent(source_manager, file_id, search_line, line_content)) {
      break;
    }

    std::string trimmed = trimWhitespace(line_content);
    bool is_statement_line = (search_line == stmt_line);

    if (is_statement_line) {
      continue;
    }

    if (trimmed.empty()) {
      continue_across_multiline_directive = false;
      continue;
    }

    // Check if this line is a continuation of a multi-line pragma (ends with
    // backslash) We must check this regardless of whether we've found the
    // pragma yet, because when scanning backwards, we encounter continuation
    // lines BEFORE the pragma line itself
    if (p_openmp_pragma_callback->isContinuationLine(file_id, search_line)) {
      continue;
    }

    if (isSkippableLineBetweenPragmaAndStatement(trimmed)) {
      continue_across_multiline_directive = false;
      continue;
    }

    // Non-empty, non-continuation, non-skippable line - stop scanning
    break;
  }

  if (found_any) {
    std::reverse(pragmas.begin(), pragmas.end());
  }

  return found_any;
}

SgPragmaDeclaration *ClangToSageTranslator::buildOpenMPPragmaDeclaration(
    const std::string &directive, unsigned pragma_line,
    SgScopeStatement *scope) {
  if (scope == NULL || directive.empty()) {
    return NULL;
  }

  // Don't normalize here - OMP unparser will handle normalization
  SgPragmaDeclaration *pragma_decl =
      SageBuilder::buildPragmaDeclaration(directive, scope);

  // Use actual source file info instead of COMPILER_GENERATED so that
  // attachPreprocessingInfo() can properly attach comments relative to the
  // pragma based on source positions
  std::string filename = p_sage_source_file->getFileName();
  Sg_File_Info *start_fi = new Sg_File_Info(filename, pragma_line, 1);
  Sg_File_Info *end_fi = new Sg_File_Info(filename, pragma_line, 1);

  pragma_decl->set_startOfConstruct(start_fi);
  pragma_decl->set_endOfConstruct(end_fi);
  start_fi->set_parent(pragma_decl);
  end_fi->set_parent(pragma_decl);
  pragma_decl->set_parent(scope);

  return pragma_decl;
}

void ClangToSageTranslator::appendOpenMPPragmasBefore(clang::Stmt *stmt,
                                                      SgScopeStatement *scope) {
  if (scope == NULL) {
    return;
  }

  std::vector<CapturedPragma> pragmas;
  if (!collectOpenMPPragmas(stmt, pragmas)) {
    return;
  }

  for (const auto &entry : pragmas) {
    SgPragmaDeclaration *pragma_decl = NULL;
    if (entry.is_openmp) {
      pragma_decl = buildOpenMPPragmaDeclaration(entry.text, entry.line, scope);
    } else {
      // Strip leading "#pragma" if present; SageBuilder re-adds it on unparse.
      std::string pragma_body = entry.text;
      const std::string prefix = "#pragma";
      if (pragma_body.compare(0, prefix.size(), prefix) == 0) {
        pragma_body = trimWhitespace(pragma_body.substr(prefix.size()));
      }
      pragma_decl = SageBuilder::buildPragmaDeclaration(pragma_body, scope);
      if (pragma_decl != NULL) {
        pragma_decl->set_parent(scope);
      }
    }
    if (pragma_decl == NULL) {
      continue;
    }

    if (SgBasicBlock *block = isSgBasicBlock(scope)) {
      block->append_statement(pragma_decl);
    } else {
      SageInterface::appendStatement(pragma_decl, scope);
    }
  }
}

SgStatement *
ClangToSageTranslator::wrapStatementWithOpenMPPragmas(clang::Stmt *stmt,
                                                      SgStatement *statement) {
  if (statement == NULL) {
    return NULL;
  }

  std::vector<CapturedPragma> pragmas;
  if (!collectOpenMPPragmas(stmt, pragmas)) {
    return statement;
  }

  SgBasicBlock *wrapper_block = SageBuilder::buildBasicBlock();
  setCompilerGeneratedFileInfo(wrapper_block, true);

  for (const auto &entry : pragmas) {
    SgPragmaDeclaration *pragma_decl = NULL;
    if (entry.is_openmp) {
      pragma_decl =
          buildOpenMPPragmaDeclaration(entry.text, entry.line, wrapper_block);
    } else {
      std::string pragma_body = entry.text;
      const std::string prefix = "#pragma";
      if (pragma_body.compare(0, prefix.size(), prefix) == 0) {
        pragma_body = trimWhitespace(pragma_body.substr(prefix.size()));
      }
      pragma_decl =
          SageBuilder::buildPragmaDeclaration(pragma_body, wrapper_block);
      if (pragma_decl != NULL) {
        pragma_decl->set_parent(wrapper_block);
      }
    }
    if (pragma_decl != NULL) {
      wrapper_block->append_statement(pragma_decl);
    }
  }

  wrapper_block->append_statement(statement);
  return wrapper_block;
}

bool ClangToSageTranslator::VisitCompoundStmt(
    clang::CompoundStmt *compound_stmt, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitCompoundStmt" << std::endl;
#endif

  bool res = true;

  SgBasicBlock *block = SageBuilder::buildBasicBlock();

  block->set_parent(SageBuilder::topScopeStack());

  SageBuilder::pushScopeStack(block);

  clang::CompoundStmt::body_iterator it;
  for (it = compound_stmt->body_begin(); it != compound_stmt->body_end();
       it++) {
    clang::Stmt *child_stmt = *it;

    appendOpenMPPragmasBefore(child_stmt, block);

    SgNode *tmp_node = Traverse(child_stmt);

#if DEBUG_VISIT_STMT
    if (tmp_node != NULL)
      std::cerr << "In VisitCompoundStmt : child is " << tmp_node->class_name()
                << std::endl;
    else
      std::cerr << "In VisitCompoundStmt : child is NULL" << std::endl;
#endif

    SgClassDeclaration *class_decl = isSgClassDeclaration(tmp_node);
    if (class_decl != NULL &&
        (class_decl->get_name() == "" || class_decl->get_isUnNamed()))
      continue;
    SgEnumDeclaration *enum_decl = isSgEnumDeclaration(tmp_node);
    if (enum_decl != NULL &&
        (enum_decl->get_name() == "" || enum_decl->get_isUnNamed()))
      continue;
#if DEBUG_VISIT_STMT
    else if (enum_decl != NULL)
      std::cerr << "enum_decl = " << enum_decl
                << " >> name: " << enum_decl->get_name() << std::endl;
#endif

    SgStatement *stmt = isSgStatement(tmp_node);
    SgExpression *expr = isSgExpression(tmp_node);
    if (tmp_node != NULL && stmt == NULL && expr == NULL) {
      std::cerr
          << "Runtime error: tmp_node != NULL && stmt == NULL && expr == NULL"
          << std::endl;
      res = false;
    } else if (stmt != NULL) {
      block->append_statement(stmt);
    } else if (expr != NULL) {
      SgExprStatement *expr_stmt = SageBuilder::buildExprStatement(expr);
      applySourceRangeWithTrailingSemicolon(expr_stmt, child_stmt);
      block->append_statement(expr_stmt);
    }
  }

  SageBuilder::popScopeStack();

  *node = block;

  return VisitStmt(compound_stmt, node) && res;
}

bool ClangToSageTranslator::VisitContinueStmt(
    clang::ContinueStmt *continue_stmt, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitContinueStmt" << std::endl;
#endif

  *node = SageBuilder::buildContinueStmt();
  return VisitStmt(continue_stmt, node);
}

bool ClangToSageTranslator::VisitCoreturnStmt(
    clang::CoreturnStmt *core_turn_stmt, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitCoreturnStmt" << std::endl;
#endif
  bool res = true;

  ROSE_ASSERT(FAIL_TODO == 0); // TODO
  return VisitStmt(core_turn_stmt, node) && res;
}

bool ClangToSageTranslator::VisitCoroutineBodyStmt(
    clang::CoroutineBodyStmt *coroutine_body_stmt, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitCoroutineBodyStmt" << std::endl;
#endif
  bool res = true;

  ROSE_ASSERT(FAIL_TODO == 0); // TODO
  return VisitStmt(coroutine_body_stmt, node) && res;
}

bool ClangToSageTranslator::VisitCXXCatchStmt(
    clang::CXXCatchStmt *cxx_catch_stmt, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitCXXCatchStmt" << std::endl;
#endif
  bool res = true;

  ROSE_ASSERT(FAIL_TODO == 0); // TODO
  return VisitStmt(cxx_catch_stmt, node) && res;
}

bool ClangToSageTranslator::VisitCXXForRangeStmt(
    clang::CXXForRangeStmt *cxx_for_range_stmt, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitCXXForRangeStmt" << std::endl;
#endif
  bool res = true;

  // Build the scope first so that any declarations created while lowering the
  // range-for (e.g., `__range`, `__begin`, `__end`, and the loop variable) are
  // inserted into the correct symbol table (see VisitForStmt()).
  SgForStatement *sg_for_stmt = new SgForStatement(
      (SgStatement *)NULL, (SgExpression *)NULL, (SgStatement *)NULL);

  sg_for_stmt->set_parent(SageBuilder::topScopeStack());
  ROSE_ASSERT(sg_for_stmt->get_parent() != NULL);
  SageBuilder::pushScopeStack(sg_for_stmt);

  // ROOT CAUSE FIX: C++11 range-based for loop: `for (auto x : container) { ...
  // }`.
  //
  // Clang can leave `getCond()` / `getInc()` null for dependent range-for loops
  // in templates. ROSE's `SgForStatement` requires a non-null test statement
  // (unparser asserts on it), so we must always materialize a test statement
  // even when Clang did not provide the desugared condition/increment.

  // Get the desugared components from Clang.
  // Range stmt declares the internal `__range` variable.
  SgStatement *range_stmt = nullptr;
  if (cxx_for_range_stmt->getRangeStmt() != nullptr) {
    SgNode *tmp_range_stmt = Traverse(cxx_for_range_stmt->getRangeStmt());
    range_stmt = isSgStatement(tmp_range_stmt);
  }

  // Begin/end iterator setup - must be included in init_stmts
  // The condition and increment expressions reference __begin/__end variables
  // created here
  SgNode *tmp_begin = cxx_for_range_stmt->getBeginStmt() != nullptr
                          ? Traverse(cxx_for_range_stmt->getBeginStmt())
                          : nullptr;
  SgStatement *begin_stmt = isSgStatement(tmp_begin);

  SgNode *tmp_end = cxx_for_range_stmt->getEndStmt() != nullptr
                        ? Traverse(cxx_for_range_stmt->getEndStmt())
                        : nullptr;
  SgStatement *end_stmt = isSgStatement(tmp_end);

  // Loop variable declaration
  SgNode *tmp_loop_var = cxx_for_range_stmt->getLoopVarStmt() != nullptr
                             ? Traverse(cxx_for_range_stmt->getLoopVarStmt())
                             : nullptr;
  SgStatement *loop_var_decl = isSgStatement(tmp_loop_var);

  // Condition, increment, and body
  clang::Expr *clang_cond = cxx_for_range_stmt->getCond();
  SgStatement *test_stmt = nullptr;
  if (clang_cond != nullptr) {
    SgNode *tmp_cond = Traverse(clang_cond);
    if (SgExpression *cond_expr = isSgExpression(tmp_cond)) {
      test_stmt = SageBuilder::buildExprStatement(cond_expr);
      applySourceRange(test_stmt, clang_cond->getSourceRange());
    } else if (SgStatement *cond_as_stmt = isSgStatement(tmp_cond)) {
      test_stmt = cond_as_stmt;
      applySourceRange(test_stmt, clang_cond->getSourceRange());
    } else if (tmp_cond != nullptr) {
      std::cerr
          << "Runtime error: CXXForRangeStmt cond translated to non-statement/"
             "non-expression node: "
          << tmp_cond->class_name() << std::endl;
      res = false;
    }
  }
  if (test_stmt == nullptr) {
    test_stmt = SageBuilder::buildNullStatement_nfi();
    setCompilerGeneratedFileInfo(test_stmt, true);
  }

  clang::Expr *clang_inc = cxx_for_range_stmt->getInc();
  SgExpression *inc = nullptr;
  if (clang_inc != nullptr) {
    SgNode *tmp_inc = Traverse(clang_inc);
    inc = isSgExpression(tmp_inc);
    if (tmp_inc != nullptr && inc == nullptr) {
      if (SgExprStatement *inc_stmt = isSgExprStatement(tmp_inc)) {
        inc = inc_stmt->get_expression();
      }
    }
    if (tmp_inc != nullptr && inc == nullptr) {
      std::cerr << "Runtime error: CXXForRangeStmt inc translated to non-"
                   "expression node: "
                << tmp_inc->class_name() << std::endl;
      res = false;
    }
  }
  if (inc == nullptr) {
    inc = SageBuilder::buildNullExpression_nfi();
    setCompilerGeneratedFileInfo(inc, true);
  }

  SgNode *tmp_body = cxx_for_range_stmt->getBody()
                         ? Traverse(cxx_for_range_stmt->getBody())
                         : nullptr;
  SgStatement *body = isSgStatement(tmp_body);
  if (body == nullptr) {
    SgExpression *body_expr = isSgExpression(tmp_body);
    if (body_expr != nullptr) {
      body = SageBuilder::buildExprStatement(body_expr);
      applySourceRange(body, cxx_for_range_stmt->getBody()->getSourceRange());
    }
  }
  if (body != nullptr) {
    body = wrapStatementWithOpenMPPragmas(cxx_for_range_stmt->getBody(), body);
  }

  // Build initialization statement list (range + begin/end iterators + loop
  // variable).
  SgStatementPtrList init_stmts;
  if (range_stmt != nullptr)
    init_stmts.push_back(range_stmt);
  if (begin_stmt)
    init_stmts.push_back(begin_stmt);
  if (end_stmt)
    init_stmts.push_back(end_stmt);
  if (loop_var_decl)
    init_stmts.push_back(loop_var_decl);
  if (init_stmts.empty()) {
    SgNullStatement *nullStmt = SageBuilder::buildNullStatement_nfi();
    setCompilerGeneratedFileInfo(nullStmt, true);
    init_stmts.push_back(nullStmt);
  }

  SgForInitStatement *for_init =
      SageBuilder::buildForInitStatement_nfi(init_stmts);

  SageBuilder::popScopeStack();

  for_init->set_parent(sg_for_stmt);
  if (sg_for_stmt->get_for_init_stmt() != NULL)
    SageInterface::deleteAST(sg_for_stmt->get_for_init_stmt());
  sg_for_stmt->set_for_init_stmt(for_init);

  if (test_stmt != nullptr) {
    test_stmt->set_parent(sg_for_stmt);
    sg_for_stmt->set_test(test_stmt);
  }

  if (inc != nullptr) {
    inc->set_parent(sg_for_stmt);
    sg_for_stmt->set_increment(inc);
  }

  if (body != nullptr) {
    body->set_parent(sg_for_stmt);
    sg_for_stmt->set_loop_body(body);
  }

  SageBuilder::buildForStatement_nfi(sg_for_stmt, for_init, test_stmt, inc,
                                     body);
  ROSE_ASSERT(sg_for_stmt->get_parent() != NULL);

  *node = sg_for_stmt;

  return VisitStmt(cxx_for_range_stmt, node) && res;
}

bool ClangToSageTranslator::VisitCXXTryStmt(clang::CXXTryStmt *cxx_try_stmt,
                                            SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitCXXTryStmt" << std::endl;
#endif
  bool res = true;

  ROSE_ASSERT(FAIL_TODO == 0); // TODO
  return VisitStmt(cxx_try_stmt, node) && res;
}

bool ClangToSageTranslator::VisitDeclStmt(clang::DeclStmt *decl_stmt,
                                          SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitDeclStmt" << std::endl;
#endif

  bool res = true;

  if (decl_stmt->isSingleDecl()) {
    *node = Traverse(decl_stmt->getSingleDecl());
#if DEBUG_VISIT_STMT
    printf("In VisitDeclStmt(): *node = %p = %s \n", *node,
           (*node)->class_name().c_str());
#endif
  } else {
    std::vector<SgNode *> tmp_decls;
    // SgDeclarationStatement * decl;
    clang::DeclStmt::decl_iterator it;

    SgScopeStatement *scope = SageBuilder::topScopeStack();

    for (it = decl_stmt->decl_begin(); it != decl_stmt->decl_end() - 1; it++) {
      clang::Decl *decl = (*it);
      if (decl == nullptr)
        continue;
      SgNode *child = Traverse(decl);

      SgDeclarationStatement *sub_decl_stmt = isSgDeclarationStatement(child);
      if (sub_decl_stmt == NULL && child != NULL) {
        std::cerr << "Runtime error: the node produce for a clang::Decl is not "
                     "a SgDeclarationStatement !"
                  << std::endl;
        std::cerr << "    class = " << child->class_name() << std::endl;
        res = false;
        continue;
      } else if (child != NULL) {
        // FIXME This is a hack to avoid autonomous decl of unnamed type to
        // being added to the global scope....
        SgClassDeclaration *class_decl = isSgClassDeclaration(child);
        if (class_decl != NULL &&
            (class_decl->get_name() == "" || class_decl->get_isUnNamed()))
          continue;

        SgEnumDeclaration *enum_decl = isSgEnumDeclaration(child);
        if (enum_decl != NULL &&
            (enum_decl->get_name() == "" || enum_decl->get_isUnNamed()))
          continue;
        if (clang::TagDecl::classof(decl)) {
          clang::TagDecl *tagDecl = (clang::TagDecl *)decl;
          if (tagDecl->isEmbeddedInDeclarator())
            continue;
        }
      }
      scope->append_statement(sub_decl_stmt);
      sub_decl_stmt->set_parent(scope);
    }
    // last declaration in scope
    it = decl_stmt->decl_end();
    --it;
    SgNode *lastDecl = Traverse((clang::Decl *)(*it));
    SgDeclarationStatement *last_decl_Stmt = isSgDeclarationStatement(lastDecl);
    if (lastDecl != NULL && last_decl_Stmt == NULL) {
      std::cerr << "Runtime error: lastDecl != NULL && last_decl_Stmt == NULL"
                << std::endl;
      res = false;
    }
    *node = last_decl_Stmt;
  }

#if DEBUG_VISIT_STMT
  printf("In VisitDeclStmt(): identify where the parent is not set: *node = %p "
         "= %s \n",
         *node, (*node)->class_name().c_str());
  printf(" --- *node parent = %p \n", (*node)->get_parent());
#endif

  return res;
}

bool ClangToSageTranslator::VisitDoStmt(clang::DoStmt *do_stmt, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitDoStmt" << std::endl;
#endif

  SgNode *tmp_cond = Traverse(do_stmt->getCond());
  SgExpression *cond = isSgExpression(tmp_cond);
  ROSE_ASSERT(cond != NULL);

  SgStatement *expr_stmt = SageBuilder::buildExprStatement(cond);
  if (p_compiler_instance != nullptr && do_stmt->getCond() != nullptr) {
    applySourceRange(expr_stmt, do_stmt->getCond()->getSourceRange());
  }

  ROSE_ASSERT(expr_stmt != NULL);

  SgDoWhileStmt *sg_do_stmt =
      SageBuilder::buildDoWhileStmt_nfi(expr_stmt, NULL);

  sg_do_stmt->set_condition(expr_stmt);

  cond->set_parent(expr_stmt);
  expr_stmt->set_parent(sg_do_stmt);

  SageBuilder::pushScopeStack(sg_do_stmt);

  SgNode *tmp_body = Traverse(do_stmt->getBody());
  SgStatement *body = isSgStatement(tmp_body);
  SgExpression *expr = isSgExpression(tmp_body);
  if (expr != NULL) {
    body = SageBuilder::buildExprStatement(expr);
    applySourceRangeWithTrailingSemicolon(body, do_stmt->getBody());
  }
  ROSE_ASSERT(body != NULL);
  body = wrapStatementWithOpenMPPragmas(do_stmt->getBody(), body);

  body->set_parent(sg_do_stmt);

  SageBuilder::popScopeStack();

  sg_do_stmt->set_body(body);

  *node = sg_do_stmt;

  return VisitStmt(do_stmt, node);
}

bool ClangToSageTranslator::VisitForStmt(clang::ForStmt *for_stmt,
                                         SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitForStmt" << std::endl;
#endif

  bool res = true;

  // DQ (11/28/2020): We have to build the scope first, and then build the rest
  // bottom up.
  SgForStatement *sg_for_stmt = new SgForStatement(
      (SgStatement *)NULL, (SgExpression *)NULL, (SgStatement *)NULL);

#if DEBUG_VISIT_STMT
  printf("In VisitForStmt(): Setting the parent of the sg_for_stmt \n");
#endif

  // DQ (11/28/2020): this is required for test2012_127.c.
  sg_for_stmt->set_parent(SageBuilder::topScopeStack());

  // DQ (11/28/2020): Adding asertion.
  ROSE_ASSERT(sg_for_stmt->get_parent() != NULL);

  SageBuilder::pushScopeStack(sg_for_stmt);

  // Initialization

  SgForInitStatement *for_init_stmt = NULL;

  {
    SgStatementPtrList for_init_stmt_list;
    SgNode *tmp_init = Traverse(for_stmt->getInit());
    SgStatement *init_stmt = isSgStatement(tmp_init);
    SgExpression *init_expr = isSgExpression(tmp_init);
    if (tmp_init != NULL && init_stmt == NULL && init_expr == NULL) {
      std::cerr << "Runtime error: tmp_init != NULL && init_stmt == NULL && "
                   "init_expr == NULL ("
                << tmp_init->class_name() << ")" << std::endl;
      res = false;
    } else if (init_expr != NULL) {
      init_stmt = SageBuilder::buildExprStatement(init_expr);
      applySourceRange(init_stmt, for_stmt->getInit()->getSourceRange());
    }
    if (init_stmt != NULL)
      for_init_stmt_list.push_back(init_stmt);

    if (for_init_stmt_list.size() == 0) {
      SgNullStatement *nullStmt = SageBuilder::buildNullStatement_nfi();
      setCompilerGeneratedFileInfo(nullStmt, true);
      for_init_stmt_list.push_back(nullStmt);
    }

    for_init_stmt = SageBuilder::buildForInitStatement_nfi(for_init_stmt_list);

#if DEBUG_VISIT_STMT
    printf("In VisitForStmt(): for_init_stmt = %p  \n");
#endif

    if (for_stmt->getInit() != NULL)
      applySourceRange(for_init_stmt, for_stmt->getInit()->getSourceRange());
    else
      setCompilerGeneratedFileInfo(for_init_stmt, true);
  }

  // Condition

  SgStatement *cond_stmt = NULL;

  {
    SgNode *tmp_cond = Traverse(for_stmt->getCond());
    SgExpression *cond = isSgExpression(tmp_cond);
    if (tmp_cond != NULL && cond == NULL) {
      std::cerr << "Runtime error: tmp_cond != NULL && cond == NULL"
                << std::endl;
      res = false;
    }
    if (cond != NULL) {
      cond_stmt = SageBuilder::buildExprStatement(cond);
      applySourceRange(cond_stmt, for_stmt->getCond()->getSourceRange());
    } else {
      cond_stmt = SageBuilder::buildNullStatement_nfi();
      setCompilerGeneratedFileInfo(cond_stmt, true);
    }

    if (cond_stmt != NULL) {
      auto *expr_stmt = isSgExprStatement(cond_stmt);
      if (expr_stmt != NULL) {
        auto simplifyOperand = [](SgExpression *operand) -> SgExpression * {
          SgExpression *current = operand;
          while (auto cast = isSgCastExp(current)) {
            current = cast->get_operand_i();
          }
          if (isSgVarRefExp(current) != NULL || isSgIntVal(current) != NULL ||
              isSgUnsignedIntVal(current) != NULL ||
              isSgLongLongIntVal(current) != NULL ||
              isSgUnsignedLongLongIntVal(current) != NULL) {
            return SageInterface::copyExpression(current);
          }
          return nullptr;
        };

        if (auto *less_than = isSgLessThanOp(expr_stmt->get_expression())) {
          SgExpression *lhs_simplified =
              simplifyOperand(less_than->get_lhs_operand());
          SgExpression *rhs_simplified =
              simplifyOperand(less_than->get_rhs_operand());
          if (lhs_simplified != NULL && rhs_simplified != NULL) {
            SgExpression *new_cond =
                SageBuilder::buildLessThanOp(lhs_simplified, rhs_simplified);
            applySourceRange(new_cond, for_stmt->getCond()->getSourceRange());
            expr_stmt->set_expression(new_cond);
            new_cond->set_parent(expr_stmt);
          }
        }
      }
    }
  }

  // Increment

  SgExpression *inc = NULL;

  {
    SgNode *tmp_inc = Traverse(for_stmt->getInc());
    inc = isSgExpression(tmp_inc);
    if (tmp_inc != NULL && inc == NULL) {
      std::cerr << "Runtime error: tmp_inc != NULL && inc == NULL" << std::endl;
      res = false;
    }
    if (inc == NULL) {
      inc = SageBuilder::buildNullExpression_nfi();
      setCompilerGeneratedFileInfo(inc, true);
    }
  }

  // Body

  SgStatement *body = NULL;

  {
    SgNode *tmp_body = Traverse(for_stmt->getBody());
    body = isSgStatement(tmp_body);
    if (body == NULL) {
      SgExpression *body_expr = isSgExpression(tmp_body);
      if (body_expr != NULL) {
        body = SageBuilder::buildExprStatement(body_expr);
        applySourceRange(body, for_stmt->getBody()->getSourceRange());
      }
    }
    if (tmp_body != NULL && body == NULL) {
      std::cerr << "Runtime error: tmp_body != NULL && body == NULL"
                << std::endl;
      res = false;
    }
    if (body == NULL) {
      body = SageBuilder::buildNullStatement_nfi();
      setCompilerGeneratedFileInfo(body);
    }
  }
  body = wrapStatementWithOpenMPPragmas(for_stmt->getBody(), body);

  SageBuilder::popScopeStack();

  // Attach sub trees to the for statement

  for_init_stmt->set_parent(sg_for_stmt);
  if (sg_for_stmt->get_for_init_stmt() != NULL)
    SageInterface::deleteAST(sg_for_stmt->get_for_init_stmt());
  sg_for_stmt->set_for_init_stmt(for_init_stmt);

  if (cond_stmt != NULL) {
    cond_stmt->set_parent(sg_for_stmt);
    sg_for_stmt->set_test(cond_stmt);
  }

  if (inc != NULL) {
    inc->set_parent(sg_for_stmt);
    sg_for_stmt->set_increment(inc);
  }

  if (body != NULL) {
    body->set_parent(sg_for_stmt);
    sg_for_stmt->set_loop_body(body);
  }

  // DQ (11/28/2020): Now we want to use the scope that is already on the stack
  // (instead of adding a new one).
  SageBuilder::buildForStatement_nfi(sg_for_stmt, for_init_stmt, cond_stmt, inc,
                                     body);

  // DQ (11/28/2020): Adding asertion.
  ROSE_ASSERT(sg_for_stmt->get_parent() != NULL);

  *node = sg_for_stmt;

  return VisitStmt(for_stmt, node) && res;
}

bool ClangToSageTranslator::VisitGotoStmt(clang::GotoStmt *goto_stmt,
                                          SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitGotoStmt" << std::endl;
#endif

  bool res = true;

  SgSymbol *tmp_sym = GetSymbolFromSymbolTable(goto_stmt->getLabel());
  SgLabelSymbol *sym = isSgLabelSymbol(tmp_sym);
  if (sym == NULL) {
    SgNode *tmp_label = Traverse(goto_stmt->getLabel()->getStmt());
    SgLabelStatement *label_stmt = isSgLabelStatement(tmp_label);
    if (label_stmt == NULL) {
      std::cerr << "Runtime error: Cannot find the symbol for the label: \""
                << goto_stmt->getLabel()->getStmt()->getName() << "\"."
                << std::endl;
      std::cerr << "Runtime Error: Cannot find the label: \""
                << goto_stmt->getLabel()->getStmt()->getName() << "\"."
                << std::endl;
      res = false;
    } else {
      *node = SageBuilder::buildGotoStatement(label_stmt);
    }
  } else {
    *node = SageBuilder::buildGotoStatement(sym->get_declaration());
  }

  /*
      SgNode * tmp_label = Traverse(goto_stmt->getLabel()->getStmt());
      SgLabelStatement * label_stmt = isSgLabelStatement(tmp_label);
      if (label_stmt == NULL) {
          std::cerr << "Runtime Error: Cannot find the label: \"" <<
     goto_stmt->getLabel()->getStmt()->getName() << "\"." << std::endl; res =
     false;
      }
      else {
          *node = SageBuilder::buildGotoStatement(label_stmt);
      }
  */
  return VisitStmt(goto_stmt, node) && res;
}

bool ClangToSageTranslator::VisitIfStmt(clang::IfStmt *if_stmt, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitIfStmt" << std::endl;
#endif

  bool res = true;

  // TODO if_stmt->getConditionVariable() appears when a variable is declared in
  // the condition...

  *node = SageBuilder::buildIfStmt_nfi(NULL, NULL, NULL);

  // Pei-Hung (04/22/22) Needs to setup parent node before processing the
  // operands. Needed for test2013_55.c and other similar tests
  (*node)->set_parent(SageBuilder::topScopeStack());
  SageBuilder::pushScopeStack(isSgScopeStatement(*node));

  SgNode *tmp_cond = Traverse(if_stmt->getCond());
  SgExpression *cond_expr = isSgExpression(tmp_cond);
  SgStatement *cond_stmt = SageBuilder::buildExprStatement(cond_expr);
  applySourceRange(cond_stmt, if_stmt->getCond()->getSourceRange());

  SgNode *tmp_then = Traverse(if_stmt->getThen());
  SgStatement *then_stmt = isSgStatement(tmp_then);
  if (then_stmt == NULL) {
    SgExpression *then_expr = isSgExpression(tmp_then);
    ROSE_ASSERT(then_expr != NULL);
    then_stmt = SageBuilder::buildExprStatement(then_expr);
  }
  applySourceRange(then_stmt, if_stmt->getThen()->getSourceRange());
  then_stmt = wrapStatementWithOpenMPPragmas(if_stmt->getThen(), then_stmt);

  SgNode *tmp_else = Traverse(if_stmt->getElse());
  SgStatement *else_stmt = isSgStatement(tmp_else);
  if (else_stmt == NULL) {
    SgExpression *else_expr = isSgExpression(tmp_else);
    if (else_expr != NULL)
      else_stmt = SageBuilder::buildExprStatement(else_expr);
  }
  if (else_stmt != NULL) {
    applySourceRange(else_stmt, if_stmt->getElse()->getSourceRange());
    else_stmt = wrapStatementWithOpenMPPragmas(if_stmt->getElse(), else_stmt);
  }

  SageBuilder::popScopeStack();

  cond_stmt->set_parent(*node);
  isSgIfStmt(*node)->set_conditional(cond_stmt);

  then_stmt->set_parent(*node);
  isSgIfStmt(*node)->set_true_body(then_stmt);
  if (else_stmt != NULL) {
    else_stmt->set_parent(*node);
    isSgIfStmt(*node)->set_false_body(else_stmt);
  }

  return VisitStmt(if_stmt, node) && res;
}

bool ClangToSageTranslator::VisitIndirectGotoStmt(
    clang::IndirectGotoStmt *indirect_goto_stmt, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitIndirectGotoStmt" << std::endl;
#endif
  bool res = true;

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitStmt(indirect_goto_stmt, node) && res;
}

bool ClangToSageTranslator::VisitMSDependentExistsStmt(
    clang::MSDependentExistsStmt *ms_dependent_exists_stmt, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitMSDependentExistsStmt" << std::endl;
#endif
  bool res = true;

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitStmt(ms_dependent_exists_stmt, node) && res;
}

bool ClangToSageTranslator::VisitNullStmt(clang::NullStmt *null_stmt,
                                          SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitNullStmt" << std::endl;
#endif
  bool res = true;

  *node = SageBuilder::buildNullStatement();

  return VisitStmt(null_stmt, node) && res;
}

bool ClangToSageTranslator::VisitOMPExecutableDirective(
    clang::OMPExecutableDirective *omp_executable_directive, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitOMPExecutableDirective"
            << std::endl;
#endif
  bool res = true;
  SgStatement *associated_stmt = NULL;

  // Traverse the body statement
  if (clang::Stmt *clang_associated_stmt =
          omp_executable_directive->getAssociatedStmt()) {
    SgNode *tmp_stmt = Traverse(clang_associated_stmt);
    associated_stmt = isSgStatement(tmp_stmt);
    if (tmp_stmt != NULL && associated_stmt == NULL) {
      std::cerr << "Runtime error: associated OpenMP statement did not "
                   "translate into an SgStatement."
                << std::endl;
      res = false;
    }
  }

  SgStatement *target_stmt = associated_stmt;
  if (target_stmt == NULL) {
    target_stmt = SageBuilder::buildNullStatement();
    target_stmt->set_parent(SageBuilder::topScopeStack());
  }

  // Just return the body statement
  // The parent (VisitCompoundStmt) will handle pragma insertion via
  // appendOpenMPPragmasBefore DO NOT call VisitStmt here - it would apply the
  // directive's source range to the body statement, overwriting the body's
  // correct source info
  *node = target_stmt;
  return res;
}

bool ClangToSageTranslator::VisitOMPAtomicDirective(
    clang::OMPAtomicDirective *omp_atomic_directive, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitOMPAtomicDirective" << std::endl;
#endif
  bool res = true;

  return VisitOMPExecutableDirective(omp_atomic_directive, node) && res;
}

bool ClangToSageTranslator::VisitOMPBarrierDirective(
    clang::OMPBarrierDirective *omp_barrier_directive, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitOMPBarrierDirective" << std::endl;
#endif
  bool res = true;

  return VisitOMPExecutableDirective(omp_barrier_directive, node) && res;
}

bool ClangToSageTranslator::VisitOMPCancelDirective(
    clang::OMPCancelDirective *omp_cancel_directive, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitOMPCancelDirective" << std::endl;
#endif
  bool res = true;

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitOMPExecutableDirective(omp_cancel_directive, node) && res;
}

bool ClangToSageTranslator::VisitOMPCancellationPointDirective(
    clang::OMPCancellationPointDirective *omp_cancellation_point_directive,
    SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitOMPCancellationPointDirective"
            << std::endl;
#endif
  bool res = true;

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitOMPExecutableDirective(omp_cancellation_point_directive, node) &&
         res;
}

bool ClangToSageTranslator::VisitOMPCriticalDirective(
    clang::OMPCriticalDirective *omp_critical_directive, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitOMPCriticalDirective" << std::endl;
#endif
  bool res = true;

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitOMPExecutableDirective(omp_critical_directive, node) && res;
}

bool ClangToSageTranslator::VisitOMPFlushDirective(
    clang::OMPFlushDirective *omp_flush_directive, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitOMPFlushDirective" << std::endl;
#endif
  bool res = true;

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitOMPExecutableDirective(omp_flush_directive, node) && res;
}

bool ClangToSageTranslator::VisitOMPLoopDirective(
    clang::OMPLoopDirective *omp_loop_directive, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitOMPLoopDirective" << std::endl;
#endif
  bool res = true;

  return VisitOMPExecutableDirective(omp_loop_directive, node) && res;
}

bool ClangToSageTranslator::VisitOMPDistributeDirective(
    clang::OMPDistributeDirective *omp_distribute_directive, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitOMPDistributeDirective"
            << std::endl;
#endif
  bool res = true;

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitOMPLoopDirective(omp_distribute_directive, node) && res;
}

bool ClangToSageTranslator::VisitOMPDistributeParallelForDirective(
    clang::OMPDistributeParallelForDirective
        *omp_distribute_parallel_for_directive,
    SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitOMPDistributeParallelForDirective"
            << std::endl;
#endif
  bool res = true;

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitOMPExecutableDirective(omp_distribute_parallel_for_directive,
                                     node) &&
         res;
}

bool ClangToSageTranslator::VisitOMPDistributeParallelForSimdDirective(
    clang::OMPDistributeParallelForSimdDirective
        *omp_distribute_parallel_for_simd_directive,
    SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr
      << "ClangToSageTranslator::VisitOMPDistributeParallelForSimdDirective"
      << std::endl;
#endif
  bool res = true;

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitOMPExecutableDirective(omp_distribute_parallel_for_simd_directive,
                                     node) &&
         res;
}

bool ClangToSageTranslator::VisitOMPDistributeSimdDirective(
    clang::OMPDistributeSimdDirective *omp_distribute__simd_directive,
    SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitOMPDistributeSimdDirective"
            << std::endl;
#endif
  bool res = true;

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitOMPExecutableDirective(omp_distribute__simd_directive, node) &&
         res;
}

bool ClangToSageTranslator::VisitOMPForDirective(
    clang::OMPForDirective *omp_for_directive, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitOMPForDirective" << std::endl;
#endif
  bool res = true;

  return VisitOMPLoopDirective(omp_for_directive, node) && res;
}

bool ClangToSageTranslator::VisitOMPForSimdDirective(
    clang::OMPForSimdDirective *omp_for_simd_directive, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitOMPForSimdDirective" << std::endl;
#endif
  bool res = true;

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitOMPLoopDirective(omp_for_simd_directive, node) && res;
}

bool ClangToSageTranslator::VisitOMPParallelForDirective(
    clang::OMPParallelForDirective *omp_parallel_for_directive, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitOMPParallelForDirective"
            << std::endl;
#endif
  bool res = true;

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitOMPLoopDirective(omp_parallel_for_directive, node) && res;
}

bool ClangToSageTranslator::VisitOMPParallelForSimdDirective(
    clang::OMPParallelForSimdDirective *omp_parallel_for_simd_directive,
    SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitOMPParallelForSimdDirective"
            << std::endl;
#endif
  bool res = true;

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitOMPLoopDirective(omp_parallel_for_simd_directive, node) && res;
}

bool ClangToSageTranslator::VisitOMPSimdDirective(
    clang::OMPSimdDirective *omp_simd_directive, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitOMPSimdDirective" << std::endl;
#endif
  bool res = true;

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitOMPLoopDirective(omp_simd_directive, node) && res;
}

bool ClangToSageTranslator::VisitOMPTargetParallelForDirective(
    clang::OMPTargetParallelForDirective *omp_target_parallel_for_directive,
    SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitOMPTargetParallelForDirective"
            << std::endl;
#endif
  bool res = true;

  return VisitOMPLoopDirective(omp_target_parallel_for_directive, node) && res;
}

bool ClangToSageTranslator::VisitOMPTargetParallelForSimdDirective(
    clang::OMPTargetParallelForSimdDirective
        *omp_target_parallel_for_simd_directive,
    SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitOMPTargetParallelForSimdDirective"
            << std::endl;
#endif
  bool res = true;

  return VisitOMPLoopDirective(omp_target_parallel_for_simd_directive, node) &&
         res;
}

bool ClangToSageTranslator::VisitOMPTargetSimdDirective(
    clang::OMPTargetSimdDirective *omp_target_simd_directive, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitOMPTargetSimdDirective"
            << std::endl;
#endif
  bool res = true;

  return VisitOMPLoopDirective(omp_target_simd_directive, node) && res;
}

bool ClangToSageTranslator::VisitOMPTargetTeamsDistributeDirective(
    clang::OMPTargetTeamsDistributeDirective
        *omp_target_teams_distribute_directive,
    SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitOMPTargetTeamsDistributeDirective"
            << std::endl;
#endif
  bool res = true;

  return VisitOMPLoopDirective(omp_target_teams_distribute_directive, node) &&
         res;
}

bool ClangToSageTranslator::VisitOMPTargetTeamsDistributeSimdDirective(
    clang::OMPTargetTeamsDistributeSimdDirective
        *omp_target_teams_distribute_simd_directive,
    SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr
      << "ClangToSageTranslator::VisitOMPTargetTeamsDistributeSimdDirective"
      << std::endl;
#endif
  bool res = true;

  return VisitOMPLoopDirective(omp_target_teams_distribute_simd_directive,
                               node) &&
         res;
}

bool ClangToSageTranslator::VisitOMPTaskLoopDirective(
    clang::OMPTaskLoopDirective *omp_task_loop_directive, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitOMPTaskLoopDirective" << std::endl;
#endif
  bool res = true;

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitOMPLoopDirective(omp_task_loop_directive, node) && res;
}

bool ClangToSageTranslator::VisitOMPTaskLoopSimdDirective(
    clang::OMPTaskLoopSimdDirective *omp_task_loop_simd_directive,
    SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitOMPTaskLoopSimdDirective"
            << std::endl;
#endif
  bool res = true;

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitOMPLoopDirective(omp_task_loop_simd_directive, node) && res;
}

bool ClangToSageTranslator::VisitOMPMasterDirective(
    clang::OMPMasterDirective *omp_master_directive, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitOMPMasterDirective" << std::endl;
#endif
  bool res = true;

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitOMPExecutableDirective(omp_master_directive, node) && res;
}

bool ClangToSageTranslator::VisitOMPOrderedDirective(
    clang::OMPOrderedDirective *omp_ordered_directive, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitOMPOrderedDirective" << std::endl;
#endif
  bool res = true;

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitOMPExecutableDirective(omp_ordered_directive, node) && res;
}

bool ClangToSageTranslator::VisitOMPParallelDirective(
    clang::OMPParallelDirective *omp_parallel_directive, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitOMPParallelDirective" << std::endl;
#endif
  bool res = true;

  return VisitOMPExecutableDirective(omp_parallel_directive, node) && res;
}

bool ClangToSageTranslator::VisitOMPParallelSectionsDirective(
    clang::OMPParallelSectionsDirective *omp_parallel_sections_directive,
    SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitOMPParallelSectionsDirective"
            << std::endl;
#endif
  bool res = true;

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitOMPExecutableDirective(omp_parallel_sections_directive, node) &&
         res;
}

bool ClangToSageTranslator::VisitReturnStmt(clang::ReturnStmt *return_stmt,
                                            SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitReturnStmt" << std::endl;
#endif

  bool res = true;

  SgNode *tmp_expr = Traverse(return_stmt->getRetValue());
  SgExpression *expr = isSgExpression(tmp_expr);
  if (tmp_expr != NULL && expr == NULL) {
    std::cerr << "Runtime error: tmp_expr != NULL && expr == NULL" << std::endl;
    res = false;
  }
  *node = SageBuilder::buildReturnStmt(expr);

  return VisitStmt(return_stmt, node) && res;
}

bool ClangToSageTranslator::VisitSEHExceptStmt(
    clang::SEHExceptStmt *seh_except_stmt, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitSEHExceptStmt" << std::endl;
#endif
  bool res = true;

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitStmt(seh_except_stmt, node) && res;
}

bool ClangToSageTranslator::VisitSEHFinallyStmt(
    clang::SEHFinallyStmt *seh_finally_stmt, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitSEHFinallyStmt" << std::endl;
#endif
  bool res = true;

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitStmt(seh_finally_stmt, node) && res;
}

bool ClangToSageTranslator::VisitSEHLeaveStmt(
    clang::SEHLeaveStmt *seh_leave_stmt, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitSEHLeaveStmt" << std::endl;
#endif
  bool res = true;

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitStmt(seh_leave_stmt, node) && res;
}

bool ClangToSageTranslator::VisitSEHTryStmt(clang::SEHTryStmt *seh_try_stmt,
                                            SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitSEHTryStmt" << std::endl;
#endif
  bool res = true;

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitStmt(seh_try_stmt, node) && res;
}

bool ClangToSageTranslator::VisitSwitchCase(clang::SwitchCase *switch_case,
                                            SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitSwitchCase" << std::endl;
#endif
  bool res = true;

  // TODO

  return VisitStmt(switch_case, node) && res;
}

bool ClangToSageTranslator::VisitCaseStmt(clang::CaseStmt *case_stmt,
                                          SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitCaseStmt" << std::endl;
#endif

  SgNode *tmp_stmt = Traverse(case_stmt->getSubStmt());
  SgStatement *stmt = isSgStatement(tmp_stmt);
  SgExpression *expr = isSgExpression(tmp_stmt);
  if (expr != NULL) {
    stmt = SageBuilder::buildExprStatement(expr);
    applySourceRangeWithTrailingSemicolon(stmt, case_stmt->getSubStmt());
  }
  ROSE_ASSERT(stmt != NULL);
  stmt = wrapStatementWithOpenMPPragmas(case_stmt->getSubStmt(), stmt);

  SgNode *tmp_lhs = Traverse(case_stmt->getLHS());
  SgExpression *lhs = isSgExpression(tmp_lhs);
  ROSE_ASSERT(lhs != NULL);

  /*  FIXME GNU extension not-handled by ROSE
      SgNode * tmp_rhs = Traverse(case_stmt->getRHS());
      SgExpression * rhs = isSgExpression(tmp_rhs);
      ROSE_ASSERT(rhs != NULL);
  */
  ROSE_ASSERT(case_stmt->getRHS() == NULL);

  *node = SageBuilder::buildCaseOptionStmt_nfi(lhs, stmt);

  return VisitSwitchCase(case_stmt, node);
}

bool ClangToSageTranslator::VisitDefaultStmt(clang::DefaultStmt *default_stmt,
                                             SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitDefaultStmt" << std::endl;
#endif

  SgNode *tmp_stmt = Traverse(default_stmt->getSubStmt());
  SgStatement *stmt = isSgStatement(tmp_stmt);
  SgExpression *expr = isSgExpression(tmp_stmt);
  if (expr != NULL) {
    stmt = SageBuilder::buildExprStatement(expr);
    applySourceRangeWithTrailingSemicolon(stmt, default_stmt->getSubStmt());
  }
  ROSE_ASSERT(stmt != NULL);
  stmt = wrapStatementWithOpenMPPragmas(default_stmt->getSubStmt(), stmt);

  *node = SageBuilder::buildDefaultOptionStmt_nfi(stmt);

  return VisitSwitchCase(default_stmt, node);
}

bool ClangToSageTranslator::VisitSwitchStmt(clang::SwitchStmt *switch_stmt,
                                            SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitSwitchStmt" << std::endl;
#endif

  SgNode *tmp_cond = Traverse(switch_stmt->getCond());
  SgExpression *cond = isSgExpression(tmp_cond);
  ROSE_ASSERT(cond != NULL);

  SgStatement *expr_stmt = SageBuilder::buildExprStatement(cond);
  applySourceRange(expr_stmt, switch_stmt->getCond()->getSourceRange());

  SgSwitchStatement *sg_switch_stmt =
      SageBuilder::buildSwitchStatement_nfi(expr_stmt, NULL);

  sg_switch_stmt->set_parent(SageBuilder::topScopeStack());

  cond->set_parent(expr_stmt);
  expr_stmt->set_parent(sg_switch_stmt);

  SageBuilder::pushScopeStack(sg_switch_stmt);

  SgNode *tmp_body = Traverse(switch_stmt->getBody());
  SgStatement *body = isSgStatement(tmp_body);
  ROSE_ASSERT(body != NULL);

  SageBuilder::popScopeStack();

  sg_switch_stmt->set_body(body);

  *node = sg_switch_stmt;

  return VisitStmt(switch_stmt, node);
}

bool ClangToSageTranslator::VisitValueStmt(clang::ValueStmt *value_stmt,
                                           SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitValueStmt" << std::endl;
#endif
  bool res = true;

  // DQ (11/28/2020): In test2020_45.c: I think this is the enum field.
  // clang::Expr* expr = value_stmt->getExprStmt();
  // ROSE_ASSERT(expr != NULL);

  // DQ (11/28/2020): Note that value_stmt->getExprStmt() == value_stmt, but not
  // sure why.

  // DQ (11/28/2020): This was previously commented out, and I think there is
  // nothing to do here. The actual implementation was done in VisitFullExp
  // ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitStmt(value_stmt, node) && res;
}

bool ClangToSageTranslator::VisitAttributedStmt(
    clang::AttributedStmt *attributed_stmt, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitAttributedStmt" << std::endl;
#endif
  bool res = true;

  // Attributes like [[fallthrough]] wrap an inner statement. We currently drop
  // the attribute and translate the wrapped statement directly.
  clang::Stmt *sub_stmt = attributed_stmt->getSubStmt();
  SgNode *sub_node = Traverse(sub_stmt);
  if (sub_node == NULL) {
    sub_node = SageBuilder::buildNullStatement();
  }
  *node = sub_node;

  if (SgStatement *sg_stmt = isSgStatement(sub_node)) {
    applySourceRange(sg_stmt, attributed_stmt->getSourceRange());
  }

  return VisitValueStmt(attributed_stmt, node) && res;
}

bool ClangToSageTranslator::VisitExpr(clang::Expr *expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitExpr" << std::endl;
#endif

  // TODO Is there anything to be done? (maybe in relation with typing?)

  return VisitValueStmt(expr, node);
}

bool ClangToSageTranslator::VisitAbstractConditionalOperator(
    clang::AbstractConditionalOperator *abstract_conditional_operator,
    SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitAbstractConditionalOperator"
            << std::endl;
#endif
  bool res = true;

  // TODO

  return VisitStmt(abstract_conditional_operator, node) && res;
}

bool ClangToSageTranslator::VisitBinaryConditionalOperator(
    clang::BinaryConditionalOperator *binary_conditional_operator,
    SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitBinaryConditionalOperator"
            << std::endl;
#endif
  bool res = true;

  // TODO

  return VisitStmt(binary_conditional_operator, node) && res;
}

bool ClangToSageTranslator::VisitConditionalOperator(
    clang::ConditionalOperator *conditional_operator, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitConditionalOperator" << std::endl;
#endif

  bool res = true;

  SgNode *tmp_cond = Traverse(conditional_operator->getCond());
  SgExpression *cond_expr = isSgExpression(tmp_cond);
  ROSE_ASSERT(cond_expr);
  SgNode *tmp_true = Traverse(conditional_operator->getTrueExpr());
  SgExpression *true_expr = isSgExpression(tmp_true);
  ROSE_ASSERT(true_expr);
  SgNode *tmp_false = Traverse(conditional_operator->getFalseExpr());
  SgExpression *false_expr = isSgExpression(tmp_false);
  ROSE_ASSERT(false_expr);

  *node = SageBuilder::buildConditionalExp(cond_expr, true_expr, false_expr);

  return VisitAbstractConditionalOperator(conditional_operator, node) && res;
}

bool ClangToSageTranslator::VisitAddrLabelExpr(
    clang::AddrLabelExpr *addr_label_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitAddrLabelExpr" << std::endl;
#endif
  bool res = true;

  // TODO

  return VisitExpr(addr_label_expr, node) && res;
}

bool ClangToSageTranslator::VisitArrayInitIndexExpr(
    clang::ArrayInitIndexExpr *array_init_index_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitArrayInitIndexExpr" << std::endl;
#endif
  bool res = true;

  // TODO

  return VisitExpr(array_init_index_expr, node) && res;
}

bool ClangToSageTranslator::VisitArrayInitLoopExpr(
    clang::ArrayInitLoopExpr *array_init_loop_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitArrayInitLoopExpr" << std::endl;
#endif
  bool res = true;

  // TODO

  return VisitExpr(array_init_loop_expr, node) && res;
}

bool ClangToSageTranslator::VisitArraySubscriptExpr(
    clang::ArraySubscriptExpr *array_subscript_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitArraySubscriptExpr" << std::endl;
#endif

  bool res = true;

  SgNode *tmp_base = Traverse(array_subscript_expr->getBase());
  SgExpression *base = isSgExpression(tmp_base);
  if (tmp_base != NULL && base == NULL) {
    std::cerr << "Runtime error: tmp_base != NULL && base == NULL" << std::endl;
    res = false;
  }
  if (SgCastExp *cast = isSgCastExp(base)) {
    auto pointerInfo = [](SgType *type) -> std::pair<int, SgType *> {
      int depth = 0;
      SgType *current = type;
      while (current != nullptr) {
        current = current->stripTypedefsAndModifiers();
        SgPointerType *ptrType = isSgPointerType(current);
        if (ptrType == nullptr) {
          break;
        }
        ++depth;
        current = ptrType->get_base_type();
      }
      return std::make_pair(depth, current != nullptr
                                       ? current->stripTypedefsAndModifiers()
                                       : nullptr);
    };

    SgType *operandType = cast->get_operand_i()->get_type();
    if (operandType != nullptr) {
      operandType = operandType->stripTypedefsAndModifiers();
      if (SgArrayType *arrayType = isSgArrayType(operandType)) {
        SgType *elementType = arrayType->get_base_type();
        ROSE_ASSERT(elementType != nullptr);
        SgType *targetType = SgPointerType::createType(elementType);
        ROSE_ASSERT(targetType != nullptr);
        if (pointerInfo(cast->get_type()) != pointerInfo(targetType)) {
          cast->set_type(targetType);
        }
      }
    }
  }

  SgNode *tmp_idx = Traverse(array_subscript_expr->getIdx());
  SgExpression *idx = isSgExpression(tmp_idx);
  if (tmp_idx != NULL && idx == NULL) {
    std::cerr << "Runtime error: tmp_idx != NULL && idx == NULL" << std::endl;
    res = false;
  }

  *node = SageBuilder::buildPntrArrRefExp(base, idx);

  return VisitExpr(array_subscript_expr, node) && res;
}

bool ClangToSageTranslator::VisitArrayTypeTraitExpr(
    clang::ArrayTypeTraitExpr *array_type_trait_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitArrayTypeTraitExpr" << std::endl;
#endif
  bool res = true;

  // TODO

  return VisitExpr(array_type_trait_expr, node) && res;
}

bool ClangToSageTranslator::VisitAsTypeExpr(clang::AsTypeExpr *as_type_expr,
                                            SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitAsTypeExpr" << std::endl;
#endif
  bool res = true;

  // TODO

  return VisitExpr(as_type_expr, node) && res;
}

bool ClangToSageTranslator::VisitAtomicExpr(clang::AtomicExpr *atomic_expr,
                                            SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitAtomicExpr" << std::endl;
#endif
  bool res = true;

  // ROOT CAUSE FIX: AtomicExpr represents C11/C++11 atomic operations like:
  // __atomic_load, __atomic_store, __atomic_fetch_add, __atomic_exchange, etc.
  // Build a function call expression to represent the atomic builtin.

  // Traverse all sub-expressions (pointer, values, memory orders, etc.)
  SgExprListExp *args = SageBuilder::buildExprListExp();
  for (unsigned i = 0; i < atomic_expr->getNumSubExprs(); i++) {
    SgNode *tmp_arg = Traverse(atomic_expr->getSubExprs()[i]);
    SgExpression *arg = isSgExpression(tmp_arg);
    if (arg != NULL) {
      args->append_expression(arg);
    } else if (tmp_arg != NULL) {
      std::cerr << "Warning: AtomicExpr argument " << i
                << " is not an expression" << std::endl;
      res = false;
    }
  }

  // Build the function call expression with the actual builtin name
  // Get the real builtin name (e.g., "__atomic_load", "__atomic_fetch_add",
  // etc.) so the unparsed code will compile correctly
  std::string builtin_name = atomic_expr->getOpAsString().str();

  // Use the actual return type from Clang, not void - many atomics return
  // values Example: __atomic_fetch_add returns the old value before addition
  SgType *return_type = buildTypeFromQualifiedType(atomic_expr->getType());
  *node = SageBuilder::buildFunctionCallExp(builtin_name, return_type, args,
                                            SageBuilder::topScopeStack());

  return VisitExpr(atomic_expr, node) && res;
}

bool ClangToSageTranslator::VisitBinaryOperator(
    clang::BinaryOperator *binary_operator, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitBinaryOperator" << std::endl;
#endif

  bool res = true;

  SgNode *tmp_lhs = Traverse(binary_operator->getLHS());
  SgExpression *lhs = isSgExpression(tmp_lhs);
  if (tmp_lhs != NULL && lhs == NULL) {
    std::cerr << "Runtime error: tmp_lhs != NULL && lhs == NULL" << std::endl;
    res = false;
  }

  SgNode *tmp_rhs = Traverse(binary_operator->getRHS());
  SgExpression *rhs = isSgExpression(tmp_rhs);
  if (tmp_rhs != NULL && rhs == NULL) {
    std::cerr << "Runtime error: tmp_rhs != NULL && rhs == NULL" << std::endl;
    res = false;
  }

  switch (binary_operator->getOpcode()) {
  // ROOT CAUSE FIX: Pointer-to-member operators for C++ member function
  // pointers
  case clang::BO_PtrMemD:
    *node = SageBuilder::buildDotStarOp(lhs, rhs);
    break; // obj.*ptr_to_member
  case clang::BO_PtrMemI:
    *node = SageBuilder::buildArrowStarOp(lhs, rhs);
    break; // ptr->*ptr_to_member
  case clang::BO_Mul:
    *node = SageBuilder::buildMultiplyOp(lhs, rhs);
    break;
  case clang::BO_Div:
    *node = SageBuilder::buildDivideOp(lhs, rhs);
    break;
  case clang::BO_Rem:
    *node = SageBuilder::buildModOp(lhs, rhs);
    break;
  case clang::BO_Add:
    *node = SageBuilder::buildAddOp(lhs, rhs);
    break;
  case clang::BO_Sub:
    *node = SageBuilder::buildSubtractOp(lhs, rhs);
    break;
  case clang::BO_Shl:
    *node = SageBuilder::buildLshiftOp(lhs, rhs);
    break;
  case clang::BO_Shr:
    *node = SageBuilder::buildRshiftOp(lhs, rhs);
    break;
  case clang::BO_LT:
    *node = SageBuilder::buildLessThanOp(lhs, rhs);
    break;
  case clang::BO_GT:
    *node = SageBuilder::buildGreaterThanOp(lhs, rhs);
    break;
  case clang::BO_LE:
    *node = SageBuilder::buildLessOrEqualOp(lhs, rhs);
    break;
  case clang::BO_GE:
    *node = SageBuilder::buildGreaterOrEqualOp(lhs, rhs);
    break;
  case clang::BO_EQ:
    *node = SageBuilder::buildEqualityOp(lhs, rhs);
    break;
  case clang::BO_NE:
    *node = SageBuilder::buildNotEqualOp(lhs, rhs);
    break;
  case clang::BO_And:
    *node = SageBuilder::buildBitAndOp(lhs, rhs);
    break;
  case clang::BO_Xor:
    *node = SageBuilder::buildBitXorOp(lhs, rhs);
    break;
  case clang::BO_Or:
    *node = SageBuilder::buildBitOrOp(lhs, rhs);
    break;
  case clang::BO_LAnd:
    *node = SageBuilder::buildAndOp(lhs, rhs);
    break;
  case clang::BO_LOr:
    *node = SageBuilder::buildOrOp(lhs, rhs);
    break;
  case clang::BO_Assign:
    *node = SageBuilder::buildAssignOp(lhs, rhs);
    break;
  case clang::BO_MulAssign:
    *node = SageBuilder::buildMultAssignOp(lhs, rhs);
    break;
  case clang::BO_DivAssign:
    *node = SageBuilder::buildDivAssignOp(lhs, rhs);
    break;
  case clang::BO_RemAssign:
    *node = SageBuilder::buildModAssignOp(lhs, rhs);
    break;
  case clang::BO_AddAssign:
    *node = SageBuilder::buildPlusAssignOp(lhs, rhs);
    break;
  case clang::BO_SubAssign:
    *node = SageBuilder::buildMinusAssignOp(lhs, rhs);
    break;
  case clang::BO_ShlAssign:
    *node = SageBuilder::buildLshiftAssignOp(lhs, rhs);
    break;
  case clang::BO_ShrAssign:
    *node = SageBuilder::buildRshiftAssignOp(lhs, rhs);
    break;
  case clang::BO_AndAssign:
    *node = SageBuilder::buildAndAssignOp(lhs, rhs);
    break;
  case clang::BO_XorAssign:
    *node = SageBuilder::buildXorAssignOp(lhs, rhs);
    break;
  case clang::BO_OrAssign:
    *node = SageBuilder::buildIorAssignOp(lhs, rhs);
    break;
  case clang::BO_Comma:
    *node = SageBuilder::buildCommaOpExp(lhs, rhs);
    break;
  default:
    std::cerr << "Unknown opcode for binary operator: "
              << binary_operator->getOpcodeStr().str() << std::endl;
    res = false;
  }

  return VisitExpr(binary_operator, node) && res;
}

bool ClangToSageTranslator::VisitCompoundAssignOperator(
    clang::CompoundAssignOperator *compound_assign_operator, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitCompoundAssignOperator"
            << std::endl;
#endif
  bool res = true;

  // TODO

  return VisitBinaryOperator(compound_assign_operator, node) && res;
}

bool ClangToSageTranslator::VisitBlockExpr(clang::BlockExpr *block_expr,
                                           SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitBlockExpr" << std::endl;
#endif
  bool res = true;

  // TODO

  return VisitExpr(block_expr, node) && res;
}

bool ClangToSageTranslator::VisitCallExpr(clang::CallExpr *call_expr,
                                          SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitCallExpr" << std::endl;
#endif

  bool res = true;

  // Phase C (Issue 115): Queue implicit template instantiations that are
  // referenced from user code. They will be translated after the TU decl pass
  // completes so their bodies can be materialized with resolved references.
  if (clang::FunctionDecl *direct_callee = call_expr->getDirectCallee()) {
    clang::TemplateSpecializationKind kind =
        direct_callee->getTemplateSpecializationKind();
    if ((kind == clang::TSK_ImplicitInstantiation ||
         kind == clang::TSK_ExplicitInstantiationDefinition) &&
        direct_callee->hasBody()) {
      bool eligible = true;
      if (p_compiler_instance != nullptr) {
        clang::SourceManager &sm = p_compiler_instance->getSourceManager();
        clang::SourceLocation loc = direct_callee->getLocation();
        if (!loc.isValid() || sm.isInSystemHeader(loc) ||
            sm.isWrittenInBuiltinFile(loc)) {
          eligible = false;
        }
      }

      if (eligible &&
          p_pending_implicit_function_instantiations_set.insert(direct_callee)
              .second) {
        p_pending_implicit_function_instantiations.push_back(direct_callee);
      }
    }
  }

  SgNode *tmp_expr = Traverse(call_expr->getCallee());
  SgExpression *expr = isSgExpression(tmp_expr);
  if (tmp_expr != NULL && expr == NULL) {
    std::cerr << "Runtime error: tmp_expr != NULL && expr == NULLL"
              << std::endl;
    res = false;
  }

  SgExprListExp *param_list = SageBuilder::buildExprListExp_nfi();
  applySourceRange(param_list, call_expr->getSourceRange());

  clang::CallExpr::arg_iterator it;
  for (it = call_expr->arg_begin(); it != call_expr->arg_end(); ++it) {
    SgNode *tmp_expr = Traverse(*it);
    SgExpression *expr = isSgExpression(tmp_expr);
    if (tmp_expr != NULL && expr == NULL) {
      std::cerr << "Runtime error: tmp_expr != NULL && expr == NULL"
                << std::endl;
      res = false;
      continue;
    }
    param_list->append_expression(expr);
  }

  *node = SageBuilder::buildFunctionCallExp_nfi(expr, param_list);

  return VisitExpr(call_expr, node) && res;
}

bool ClangToSageTranslator::VisitCUDAKernelCallExpr(
    clang::CUDAKernelCallExpr *cuda_kernel_call_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitCUDAKernelCallExpr" << std::endl;
#endif
  bool res = true;

  // TODO

  return VisitExpr(cuda_kernel_call_expr, node) && res;
}

bool ClangToSageTranslator::VisitCXXMemberCallExpr(
    clang::CXXMemberCallExpr *cxx_member_call_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitCXXMemberCallExpr" << std::endl;
#endif
  bool res = true;

  // CXXMemberCallExpr represents calls to member functions (e.g., obj.method()
  // or ptr->method()) Delegate to CallExpr handler which will handle function
  // call expression generation
  return VisitCallExpr(cxx_member_call_expr, node) && res;
}

bool ClangToSageTranslator::VisitCXXOperatorCallExpr(
    clang::CXXOperatorCallExpr *cxx_operator_call_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitCXXOperatorCallExpr" << std::endl;
#endif
  bool res = true;

  // C++ overloaded operators (operator+, operator[], etc.) are represented as
  // function calls. For member operators, Clang models the implicit object as
  // the first argument, while ROSE represents it as a dot/arrow expression in
  // the call's callee. Convert member-operator calls into the ROSE form to
  // preserve correct operator arity and unparsing.
  if (clang::FunctionDecl *direct_callee =
          cxx_operator_call_expr->getDirectCallee()) {
    if (llvm::isa<clang::CXXMethodDecl>(direct_callee) &&
        cxx_operator_call_expr->getNumArgs() > 0) {
      // operator[] is special: ROSE's unparser expects the implicit object to
      // remain as the first argument so it can emit `obj[idx]`. Do not convert
      // the implicit object into a dot/arrow callee for this operator.
      if (cxx_operator_call_expr->getOperator() == clang::OO_Subscript) {
        SgNode *tmp_decl = NULL;
        auto it_decl = p_decl_translation_map.find(direct_callee);
        if (it_decl != p_decl_translation_map.end()) {
          tmp_decl = it_decl->second;
        } else {
          tmp_decl = Traverse(direct_callee);
        }

        SgMemberFunctionDeclaration *member_decl =
            isSgMemberFunctionDeclaration(tmp_decl);
        SgMemberFunctionSymbol *member_sym = NULL;
        if (member_decl != NULL) {
          member_sym = isSgMemberFunctionSymbol(
              member_decl->get_symbol_from_symbol_table());
          if (member_sym == NULL) {
            if (SgScopeStatement *decl_scope = member_decl->get_scope()) {
              member_sym =
                  isSgMemberFunctionSymbol(decl_scope->lookup_function_symbol(
                      member_decl->get_name(), member_decl->get_type()));
            }
          }
          if (member_sym == NULL) {
            member_sym = new SgMemberFunctionSymbol(member_decl);
            member_sym->set_parent(member_decl);
          }
        }

        if (member_sym != NULL) {
          SgExprListExp *param_list = SageBuilder::buildExprListExp_nfi();
          applySourceRange(param_list,
                           cxx_operator_call_expr->getSourceRange());

          for (unsigned i = 0; i < cxx_operator_call_expr->getNumArgs(); ++i) {
            SgNode *tmp_arg = Traverse(cxx_operator_call_expr->getArg(i));
            SgExpression *arg = isSgExpression(tmp_arg);
            if (tmp_arg != NULL && arg == NULL) {
              std::cerr << "Runtime error: tmp_arg != NULL && arg == NULL"
                        << std::endl;
              res = false;
              continue;
            }
            param_list->append_expression(arg);
          }

          SgExpression *callee = SageBuilder::buildFunctionRefExp(member_sym);
          SgFunctionCallExp *call =
              SageBuilder::buildFunctionCallExp_nfi(callee, param_list);
          call->set_uses_operator_syntax(true);
          *node = call;
          return VisitExpr(cxx_operator_call_expr, node) && res;
        }
      }

      SgNode *tmp_base = Traverse(cxx_operator_call_expr->getArg(0));
      SgExpression *base = isSgExpression(tmp_base);
      if (tmp_base != NULL && base == NULL) {
        std::cerr << "Runtime error: tmp_base != NULL && base == NULL"
                  << std::endl;
        res = false;
      }

      SgNode *tmp_decl = NULL;
      auto it_decl = p_decl_translation_map.find(direct_callee);
      if (it_decl != p_decl_translation_map.end()) {
        tmp_decl = it_decl->second;
      } else {
        tmp_decl = Traverse(direct_callee);
      }

      SgMemberFunctionDeclaration *member_decl =
          isSgMemberFunctionDeclaration(tmp_decl);
      SgMemberFunctionSymbol *member_sym = NULL;
      if (member_decl != NULL) {
        member_sym = isSgMemberFunctionSymbol(
            member_decl->get_symbol_from_symbol_table());
        if (member_sym == NULL) {
          if (SgScopeStatement *decl_scope = member_decl->get_scope()) {
            member_sym =
                isSgMemberFunctionSymbol(decl_scope->lookup_function_symbol(
                    member_decl->get_name(), member_decl->get_type()));
          }
        }
        if (member_sym == NULL) {
          member_sym = new SgMemberFunctionSymbol(member_decl);
          member_sym->set_parent(member_decl);
        }
      }

      if (base != NULL && member_sym != NULL) {
        SgExpression *member_ref = SageBuilder::buildMemberFunctionRefExp_nfi(
            member_sym, false, false);

        SgExpression *callee = NULL;
        if (isSgPointerType(base->get_type())) {
          callee = SageBuilder::buildArrowExp(base, member_ref);
        } else {
          callee = SageBuilder::buildDotExp(base, member_ref);
        }

        SgExprListExp *param_list = SageBuilder::buildExprListExp_nfi();
        applySourceRange(param_list, cxx_operator_call_expr->getSourceRange());

        for (unsigned i = 1; i < cxx_operator_call_expr->getNumArgs(); ++i) {
          SgNode *tmp_arg = Traverse(cxx_operator_call_expr->getArg(i));
          SgExpression *arg = isSgExpression(tmp_arg);
          if (tmp_arg != NULL && arg == NULL) {
            std::cerr << "Runtime error: tmp_arg != NULL && arg == NULL"
                      << std::endl;
            res = false;
            continue;
          }
          param_list->append_expression(arg);
        }

        SgFunctionCallExp *call =
            SageBuilder::buildFunctionCallExp_nfi(callee, param_list);
        call->set_uses_operator_syntax(true);
        *node = call;

        return VisitExpr(cxx_operator_call_expr, node) && res;
      }
    }
  }

  // Non-member overloaded operators are modeled as regular calls where all
  // operands appear as explicit arguments.
  res = VisitCallExpr(cxx_operator_call_expr, node) && res;

  // CLANG FRONTEND FIX: Set uses_operator_syntax flag to true for operator
  // overloads This tells the unparser to generate operator syntax (e.g., a + b)
  // instead of explicit function call syntax (e.g., operator+(a, b))
  if (*node != NULL && res) {
    SgFunctionCallExp *funcCall = isSgFunctionCallExp(*node);
    if (funcCall != NULL) {
      funcCall->set_uses_operator_syntax(true);
    }
  }

  return res;
}

bool ClangToSageTranslator::VisitUserDefinedLiteral(
    clang::UserDefinedLiteral *user_defined_literal, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitUserDefinedLiteral" << std::endl;
#endif
  bool res = true;

  // TODO

  return VisitExpr(user_defined_literal, node) && res;
}

bool ClangToSageTranslator::VisitCastExpr(clang::CastExpr *cast_expr,
                                          SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitCastExpr" << std::endl;
#endif
  bool res = true;

  // Process the sub-expression being cast
  SgNode *tmp_expr = Traverse(cast_expr->getSubExpr());
  SgExpression *sg_expr = isSgExpression(tmp_expr);
  ROSE_ASSERT(sg_expr != NULL);

  // Get the target type
  SgType *sg_type = buildTypeFromQualifiedType(cast_expr->getType());

  // Create the cast expression
  *node = SageBuilder::buildCastExp(sg_expr, sg_type);

  return VisitExpr(cast_expr, node) && res;
}

bool ClangToSageTranslator::VisitExplicitCastExpr(
    clang::ExplicitCastExpr *explicit_cast_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitExplicitCastExpr" << std::endl;
#endif
  bool res = true;

  if (auto *functional =
          llvm::dyn_cast<clang::CXXFunctionalCastExpr>(explicit_cast_expr)) {
    return VisitCXXFunctionalCastExpr(functional, node);
  }

  SgNode *tmp_expr = Traverse(explicit_cast_expr->getSubExpr());
  SgExpression *sg_expr = isSgExpression(tmp_expr);
  ROSE_ASSERT(sg_expr != NULL);

  SgType *sg_type =
      buildTypeFromQualifiedType(explicit_cast_expr->getTypeAsWritten());

  SgCastExp::cast_type_enum cast_kind = SgCastExp::e_C_style_cast;
  if (llvm::isa<clang::CXXStaticCastExpr>(explicit_cast_expr)) {
    cast_kind = SgCastExp::e_static_cast;
  } else if (llvm::isa<clang::CXXConstCastExpr>(explicit_cast_expr)) {
    cast_kind = SgCastExp::e_const_cast;
  } else if (llvm::isa<clang::CXXDynamicCastExpr>(explicit_cast_expr)) {
    cast_kind = SgCastExp::e_dynamic_cast;
  } else if (llvm::isa<clang::CXXReinterpretCastExpr>(explicit_cast_expr)) {
    cast_kind = SgCastExp::e_reinterpret_cast;
  }

  *node = SageBuilder::buildCastExp(sg_expr, sg_type, cast_kind);

  return VisitExpr(explicit_cast_expr, node) && res;
}

bool ClangToSageTranslator::VisitBuiltinBitCastExpr(
    clang::BuiltinBitCastExpr *builtin_bit_cast_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitBuiltinBitCastExpr" << std::endl;
#endif
  bool res = true;

  // TODO

  return VisitExplicitCastExpr(builtin_bit_cast_expr, node) && res;
}

bool ClangToSageTranslator::VisitCStyleCastExpr(
    clang::CStyleCastExpr *c_style_cast, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitCStyleCastExpr" << std::endl;
#endif

  bool res = true;

  SgNode *tmp_expr = Traverse(c_style_cast->getSubExpr());
  SgExpression *expr = isSgExpression(tmp_expr);

  ROSE_ASSERT(expr);

  SgType *type = buildTypeFromQualifiedType(c_style_cast->getTypeAsWritten());

  *node = SageBuilder::buildCastExp(expr, type, SgCastExp::e_C_style_cast);

  return VisitExplicitCastExpr(c_style_cast, node) && res;
}

bool ClangToSageTranslator::VisitCXXFunctionalCastExpr(
    clang::CXXFunctionalCastExpr *cxx_functional_cast_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitCXXFunctionalCastExpr" << std::endl;
#endif
  bool res = true;

  SgExprListExp *args = SageBuilder::buildExprListExp_nfi();
  if (clang::Expr *sub_expr = cxx_functional_cast_expr->getSubExpr()) {
    SgNode *tmp_expr = Traverse(sub_expr);
    if (SgExprListExp *expr_list = isSgExprListExp(tmp_expr)) {
      args = expr_list;
    } else if (SgExpression *expr = isSgExpression(tmp_expr)) {
      args->append_expression(expr);
    }
  }

  SgType *cast_type =
      buildTypeFromQualifiedType(cxx_functional_cast_expr->getTypeAsWritten());

  bool class_unknown = false;
  if (cast_type != nullptr) {
    if (isSgTypedefType(cast_type) == nullptr &&
        isSgClassType(cast_type) == nullptr) {
      class_unknown = true;
    }
  } else {
    class_unknown = true;
  }

  SgConstructorInitializer *ctor_init =
      SageBuilder::buildConstructorInitializer_nfi(
          NULL, // declaration (filled in later by AST fixup if needed)
          args, cast_type,
          true,         // need_name
          false,        // need_qualifier
          false,        // need_parenthesis_after_name
          class_unknown // associated_class_unknown
      );

  ctor_init->set_is_explicit_cast(true);
  ctor_init->set_is_braced_initialized(
      cxx_functional_cast_expr->isListInitialization());

  *node = ctor_init;

  return VisitExpr(cxx_functional_cast_expr, node) && res;
}

bool ClangToSageTranslator::VisitCXXNamedCastExpr(
    clang::CXXNamedCastExpr *cxx_named_cast_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitCXXNamedCastExpr" << std::endl;
#endif
  bool res = true;

  // TODO

  return VisitExplicitCastExpr(cxx_named_cast_expr, node) && res;
}

bool ClangToSageTranslator::VisitCXXConstCastExpr(
    clang::CXXConstCastExpr *cxx_const_cast_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitCXXConstCastExpr" << std::endl;
#endif
  bool res = true;

  // TODO

  return VisitCXXNamedCastExpr(cxx_const_cast_expr, node) && res;
}

bool ClangToSageTranslator::VisitCXXDynamicCastExpr(
    clang::CXXDynamicCastExpr *cxx_dynamic_cast_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitCXXDynamicCastExpr" << std::endl;
#endif
  bool res = true;

  // TODO

  return VisitCXXNamedCastExpr(cxx_dynamic_cast_expr, node) && res;
}

bool ClangToSageTranslator::VisitCXXReinterpretCastExpr(
    clang::CXXReinterpretCastExpr *cxx_reinterpret_cast_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitCXXReinterpretCastExpr"
            << std::endl;
#endif
  bool res = true;

  // TODO

  return VisitCXXNamedCastExpr(cxx_reinterpret_cast_expr, node) && res;
}

bool ClangToSageTranslator::VisitCXXStaticCastExpr(
    clang::CXXStaticCastExpr *cxx_static_cast_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitCXXStaticCastExpr" << std::endl;
#endif
  bool res = true;

  // TODO

  return VisitCXXNamedCastExpr(cxx_static_cast_expr, node) && res;
}

bool ClangToSageTranslator::VisitImplicitCastExpr(
    clang::ImplicitCastExpr *implicit_cast_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitImplicitCastExpr" << std::endl;
#endif

  SgNode *tmp_expr = Traverse(implicit_cast_expr->getSubExpr());
  SgExpression *expr = isSgExpression(tmp_expr);

  ROSE_ASSERT(expr != NULL);

  // FIX: Pass through implicit casts without creating SgCastExp nodes
  // EDG frontend doesn't create explicit cast nodes for implicit casts
  // Creating them breaks parent pointer relationships (e.g., FunctionRefExp
  // parent becomes CastExp instead of FunctionCallExp) This matches the
  // behavior expected by existing ROSE tests

  // LIMITATION: The sub-expression retains its original type (e.g., int stays
  // int even if cast to double). SgExpression types are immutable in ROSE -
  // there is no set_type() method. This matches EDG frontend behavior where
  // implicit casts don't create SgCastExp nodes. Most ROSE analyses handle this
  // correctly. If type correctness is critical for a specific analysis, that
  // analysis should consult the Clang AST context or implement its own type
  // inference.
  *node = expr;

  return VisitExpr(implicit_cast_expr, node);
}

bool ClangToSageTranslator::VisitCharacterLiteral(
    clang::CharacterLiteral *character_literal, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitCharacterLiteral" << std::endl;
#endif

  *node = SageBuilder::buildCharVal(character_literal->getValue());

  return VisitExpr(character_literal, node);
}

bool ClangToSageTranslator::VisitChooseExpr(clang::ChooseExpr *choose_expr,
                                            SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitChooseExpr" << std::endl;
#endif
  bool res = true;

  // TODO

  return VisitExpr(choose_expr, node) && res;
}

bool ClangToSageTranslator::VisitCompoundLiteralExpr(
    clang::CompoundLiteralExpr *compound_literal, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitCompoundLiteralExpr" << std::endl;
#endif

  SgNode *tmp_node = Traverse(compound_literal->getInitializer());
  SgExprListExp *expr = isSgExprListExp(tmp_node);
  ROSE_ASSERT(expr != NULL);

  SgType *type = buildTypeFromQualifiedType(compound_literal->getType());
  ROSE_ASSERT(type != NULL);

  SgAggregateInitializer *initializer =
      SageBuilder::buildAggregateInitializer_nfi(expr, type);

  initializer->set_uses_compound_literal(true);

  SgName name = std::string("compound_literal_") +
                Rose::StringUtility::numberToString(compound_literal);
  SgScopeStatement *scope = SageBuilder::topScopeStack();
  SgVariableDeclaration *var_decl =
      new SgVariableDeclaration(SgName(name), type, initializer);
  var_decl->set_scope(scope);
  var_decl->set_parent(scope);
  var_decl->set_firstNondefiningDeclaration(var_decl);
  var_decl->set_definingDeclaration(var_decl);
  var_decl->set_file_info(
      Sg_File_Info::generateDefaultFileInfoForCompilerGeneratedNode());
  if (Sg_File_Info *fi = var_decl->get_file_info()) {
    fi->setCompilerGenerated();
    fi->unsetOutputInCodeGeneration();
  }

  ROSE_ASSERT(!var_decl->get_variables().empty());
  SgInitializedName *iname = var_decl->get_variables()[0];
  iname->set_scope(scope);
  iname->set_parent(var_decl);
  iname->set_file_info(
      Sg_File_Info::generateDefaultFileInfoForCompilerGeneratedNode());
  if (Sg_File_Info *fi = iname->get_file_info()) {
    fi->setCompilerGenerated();
    fi->unsetOutputInCodeGeneration();
  }

  if (initializer != NULL) {
    initializer->set_parent(iname);
  }

  SgVariableSymbol *vsym = new SgVariableSymbol(iname);
  ROSE_ASSERT(vsym != nullptr);

  scope->insert_symbol(name, vsym);

  // REX FIX: Check if the type is defined inside the compound literal
  // If so, mark the declaration as non-autonomous so it gets unparsed
  // correctly
  if (clang::TypeSourceInfo *TInfo = compound_literal->getTypeSourceInfo()) {
    clang::TypeLoc TL = TInfo->getTypeLoc();
    // Drill down to the underlying TagTypeLoc, stripping common wrappers
    // (const qualifiers, parens, attributes, arrays, pointers, elaborations).
    while (true) {
      bool advanced = false;

      // Strip top-level qualifiers
      TL = TL.getUnqualifiedLoc();

      if (auto ParenTL = TL.getAs<clang::ParenTypeLoc>()) {
        TL = ParenTL.getInnerLoc();
        advanced = true;
      } else if (auto AttrTL = TL.getAs<clang::AttributedTypeLoc>()) {
        TL = AttrTL.getModifiedLoc();
        advanced = true;
      } else if (auto AdjTL = TL.getAs<clang::AdjustedTypeLoc>()) {
        TL = AdjTL.getOriginalLoc();
        advanced = true;
      } else if (auto ETL = TL.getAs<clang::ElaboratedTypeLoc>()) {
        TL = ETL.getNamedTypeLoc();
        advanced = true;
      } else if (auto ATL = TL.getAs<clang::ArrayTypeLoc>()) {
        TL = ATL.getElementLoc();
        advanced = true;
      } else if (auto PTL = TL.getAs<clang::PointerTypeLoc>()) {
        TL = PTL.getPointeeLoc();
        advanced = true;
      }

      if (!advanced)
        break;
    }

    if (auto TagTL = TL.getAs<clang::TagTypeLoc>()) {
      if (TagTL.isDefinition()) {
        clang::TagDecl *TD = TagTL.getDecl();
        std::map<clang::Decl *, SgNode *>::iterator it =
            p_decl_translation_map.find(TD);
        if (it != p_decl_translation_map.end()) {
          SgNode *node = it->second;
          if (SgClassDeclaration *classDecl = isSgClassDeclaration(node)) {
            classDecl->set_isAutonomousDeclaration(false);
            if (classDecl->get_definingDeclaration()) {
              if (SgClassDeclaration *defDecl = isSgClassDeclaration(
                      classDecl->get_definingDeclaration())) {
                defDecl->set_isAutonomousDeclaration(false);
              }
            }
          } else if (SgEnumDeclaration *enumDecl = isSgEnumDeclaration(node)) {
            enumDecl->set_isAutonomousDeclaration(false);
            if (enumDecl->get_definingDeclaration()) {
              if (SgEnumDeclaration *defDecl = isSgEnumDeclaration(
                      enumDecl->get_definingDeclaration())) {
                defDecl->set_isAutonomousDeclaration(false);
              }
            }
          }
        }
      }
    }
  }

  *node = SageBuilder::buildCompoundLiteralExp_nfi(vsym);

  return VisitExpr(compound_literal, node);
}

// bool
// ClangToSageTranslator::VisitConceptSpecializationExpr(clang::ConceptSpecializationExpr
// * concept_specialization_expr, SgNode ** node) { #if DEBUG_VISIT_STMT
//     std::cerr << "ClangToSageTranslator::VisitConceptSpecializationExpr" <<
//     std::endl;
// #endif
//     bool res = true;
//
//     // TODO
//
//     return VisitExpr(concept_specialization_expr, node) && res;
// }

bool ClangToSageTranslator::VisitConvertVectorExpr(
    clang::ConvertVectorExpr *convert_vector_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitConvertVectorExpr" << std::endl;
#endif
  bool res = true;

  // TODO

  return VisitExpr(convert_vector_expr, node) && res;
}

bool ClangToSageTranslator::VisitCoroutineSuspendExpr(
    clang::CoroutineSuspendExpr *coroutine_suspend_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitCoroutineSuspendExpr" << std::endl;
#endif
  bool res = true;

  // TODO

  return VisitExpr(coroutine_suspend_expr, node) && res;
}

bool ClangToSageTranslator::VisitCoawaitExpr(clang::CoawaitExpr *coawait_expr,
                                             SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitCoawaitExpr" << std::endl;
#endif
  bool res = true;

  // TODO

  return VisitCoroutineSuspendExpr(coawait_expr, node) && res;
}

bool ClangToSageTranslator::VisitCoyieldExpr(clang::CoyieldExpr *coyield_expr,
                                             SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitCoyieldExpr" << std::endl;
#endif
  bool res = true;

  // TODO

  return VisitCoroutineSuspendExpr(coyield_expr, node) && res;
}

bool ClangToSageTranslator::VisitCXXBindTemporaryExpr(
    clang::CXXBindTemporaryExpr *cxx_bind_temporary_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitCXXBindTemporaryExpr" << std::endl;
#endif
  bool res = true;

  // ROOT CAUSE FIX: CXXBindTemporaryExpr extends lifetime of temporary object
  // ROSE handles temporaries differently - just traverse the subexpression
  clang::Expr *sub_expr = cxx_bind_temporary_expr->getSubExpr();
  if (sub_expr != NULL) {
    *node = Traverse(sub_expr);
    if (*node == NULL) {
      return false;
    }
  } else {
    return false;
  }

  return VisitExpr(cxx_bind_temporary_expr, node) && res;
}

bool ClangToSageTranslator::VisitCXXBoolLiteralExpr(
    clang::CXXBoolLiteralExpr *cxx_bool_literal_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitCXXBoolLiteralExpr" << std::endl;
#endif
  bool res = true;

  // C++ boolean literals (true/false)
  bool value = cxx_bool_literal_expr->getValue();
  *node = SageBuilder::buildBoolValExp(value);

  return VisitExpr(cxx_bool_literal_expr, node) && res;
}

bool ClangToSageTranslator::VisitCXXConstructExpr(
    clang::CXXConstructExpr *cxx_construct_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitCXXConstructExpr" << std::endl;
#endif
  bool res = true;

  // Get the constructor being called
  clang::CXXConstructorDecl *ctor_decl = cxx_construct_expr->getConstructor();

  if (ctor_decl != nullptr) {
    // Get the type being constructed
    SgType *constructed_type =
        buildTypeFromQualifiedType(cxx_construct_expr->getType());

    // Build argument list for constructor call
    // Note: Empty argument lists are intentional and valid for default
    // constructors or when all arguments fail traversal (e.g.,
    // template-dependent arguments)
    SgExprListExp *args = SageBuilder::buildExprListExp_nfi();

    // Traverse constructor arguments
    for (unsigned i = 0; i < cxx_construct_expr->getNumArgs(); ++i) {
      clang::Expr *arg = cxx_construct_expr->getArg(i);
      if (arg != nullptr) {
        // CLANG FRONTEND FIX #19: Skip default arguments
        // CXXDefaultArgExpr represents implicit default arguments that
        // shouldn't appear in the explicit argument list. For example:
        //   std::string str("hello");  // Should have 1 arg, not 2
        // The allocator parameter is a default argument and should be omitted.
        if (clang::isa<clang::CXXDefaultArgExpr>(arg)) {
          continue;
        }

        SgNode *sg_arg = Traverse(arg);
        if (SgExpression *sg_expr = isSgExpression(sg_arg)) {
          args->append_expression(sg_expr);
        }
      }
    }

    // Use SgConstructorInitializer to properly represent constructor calls
    // This ensures the expression has the constructed class type, not void

    // Check if the type satisfies SgConstructorInitializer requirements
    // The assertion requires: isSgTypedefType or isSgClassType or
    // associated_class_unknown==true
    bool class_unknown = false;
    if (constructed_type != nullptr) {
      if (isSgTypedefType(constructed_type) == nullptr &&
          isSgClassType(constructed_type) == nullptr) {
        // Type is neither typedef nor class type, set flag to true
        class_unknown = true;
      }
    } else {
      // No type available, set flag to true
      class_unknown = true;
    }

    SgConstructorInitializer *ctor_init =
        SageBuilder::buildConstructorInitializer_nfi(
            NULL, // declaration (filled in later by AST fixup if needed)
            args, constructed_type,
            false,        // need_name
            false,        // need_qualifier
            false,        // need_parenthesis_after_name
            class_unknown // associated_class_unknown
        );

    // Preserve brace-init vs paren-init. Without this, list-initialization like
    // `T t{};` may be unparsed as `T t;`, which can change semantics (e.g.,
    // value-initialization vs default-initialization).
    ctor_init->set_is_braced_initialized(
        cxx_construct_expr->isListInitialization());

    *node = ctor_init;
  } else {
    // No constructor available, create a null expression
    *node = SageBuilder::buildNullExpression();
  }

  return VisitExpr(cxx_construct_expr, node) && res;
}

bool ClangToSageTranslator::VisitCXXTemporaryObjectExpr(
    clang::CXXTemporaryObjectExpr *cxx_temporary_object_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitCXXTemporaryObjectExpr"
            << std::endl;
#endif
  bool res = true;

  // TODO

  return VisitCXXConstructExpr(cxx_temporary_object_expr, node) && res;
}

bool ClangToSageTranslator::VisitCXXDefaultArgExpr(
    clang::CXXDefaultArgExpr *cxx_default_arg_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitCXXDefaultArgExpr" << std::endl;
#endif
  bool res = true;

  // CXXDefaultArgExpr represents use of a default argument in a function call
  // The referenced default argument expression is owned by the parameter
  // declaration. Reusing the same SgNode in both the declaration and call-site
  // would create multiple parents and break CFG invariants. Always deep-copy
  // the translated expression for the call-site use.
  if (cxx_default_arg_expr->getExpr() != nullptr) {
    SgNode *tmp_expr = Traverse(cxx_default_arg_expr->getExpr());
    SgExpression *expr = isSgExpression(tmp_expr);
    if (tmp_expr != NULL && expr == NULL) {
      std::cerr << "Runtime error: tmp_expr != NULL && expr == NULL"
                << std::endl;
      res = false;
      *node = SageBuilder::buildNullExpression();
    } else if (expr != NULL) {
      SgExpression *expr_copy = SageInterface::deepCopy(expr);
      ROSE_ASSERT(expr_copy != NULL);
      *node = expr_copy;
    } else {
      *node = SageBuilder::buildNullExpression();
    }
  } else {
    // No expression available, use null expression as placeholder
    *node = SageBuilder::buildNullExpression();
  }

  return VisitExpr(cxx_default_arg_expr, node) && res;
}

bool ClangToSageTranslator::VisitCXXDefaultInitExpr(
    clang::CXXDefaultInitExpr *cxx_default_init_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitCXXDefaultInitExpr" << std::endl;
#endif
  bool res = true;

  // TODO

  return VisitExpr(cxx_default_init_expr, node) && res;
}

bool ClangToSageTranslator::VisitCXXDeleteExpr(
    clang::CXXDeleteExpr *cxx_delete_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitCXXDeleteExpr" << std::endl;
#endif
  bool res = true;

  // ROOT CAUSE FIX: CXXDeleteExpr represents the C++ delete operator
  // Examples: delete ptr; or delete[] array;

  // Get the expression being deleted
  SgNode *tmp_arg = Traverse(cxx_delete_expr->getArgument());
  SgExpression *arg = isSgExpression(tmp_arg);

  if (arg == NULL) {
    // If we can't get the argument, create a null expression as placeholder
    arg = SageBuilder::buildNullExpression();
  }

  // Check if this is array delete (delete[]) or single object delete (delete)
  bool is_array = cxx_delete_expr->isArrayForm();

  // Build the delete expression
  *node = SageBuilder::buildDeleteExp(arg, is_array, false, NULL);

  return VisitExpr(cxx_delete_expr, node) && res;
}

bool ClangToSageTranslator::VisitCXXDependentScopeMemberExpr(
    clang::CXXDependentScopeMemberExpr *cxx_dependent_scope_member_expr,
    SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitCXXDependentScopeMemberExpr"
            << std::endl;
#endif
  // std::cerr << "DEBUG: VisitCXXDependentScopeMemberExpr. Member: " <<
  // cxx_dependent_scope_member_expr->getMember().getAsString() << std::endl;
  bool res = true;

  // CXXDependentScopeMemberExpr represents member access on a
  // template-dependent type (e.g., obj.begin(), obj->data()).

  // Get the member name
  std::string member_name =
      cxx_dependent_scope_member_expr->getMember().getAsString();

  SgScopeStatement *current_scope = SageBuilder::topScopeStack();
  if (current_scope == NULL) {
    current_scope = getGlobalScope();
  }
  ROSE_ASSERT(current_scope != NULL);

  SgExpression *base_expr = NULL;
  if (!cxx_dependent_scope_member_expr->isImplicitAccess()) {
    SgNode *tmp_base = Traverse(cxx_dependent_scope_member_expr->getBase());
    base_expr = isSgExpression(tmp_base);
    ROSE_ASSERT(base_expr != NULL);
  }

  SgTemplateArgumentPtrList template_args;
  const SgTemplateArgumentPtrList *template_args_ptr = NULL;
  if (cxx_dependent_scope_member_expr->hasExplicitTemplateArgs()) {
    clang::TemplateArgumentListInfo arg_info;
    cxx_dependent_scope_member_expr->copyTemplateArgumentsInto(arg_info);
    template_args = buildTemplateArguments(arg_info, true);
    template_args_ptr = &template_args;
  }

  SgNonrealRefExp *member_ref = buildNonrealRefExpFromNestedNameSpecifier(
      cxx_dependent_scope_member_expr->getQualifier(), current_scope,
      SgName(member_name),
      cxx_dependent_scope_member_expr->hasTemplateKeyword(), template_args_ptr);
  ROSE_ASSERT(member_ref != NULL);

  if (base_expr != NULL) {
    if (cxx_dependent_scope_member_expr->isArrow()) {
      *node = SageBuilder::buildArrowExp(base_expr, member_ref);
    } else {
      *node = SageBuilder::buildDotExp(base_expr, member_ref);
    }
  } else {
    *node = member_ref;
  }

  // Set source position
  SgExpression *expr = isSgExpression(*node);
  if (expr != NULL) {
    applySourceRange(expr, cxx_dependent_scope_member_expr->getSourceRange());
  }

  return VisitExpr(cxx_dependent_scope_member_expr, node) && res;
}

bool ClangToSageTranslator::VisitCXXFoldExpr(clang::CXXFoldExpr *cxx_fold_expr,
                                             SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitCXXFoldExpr" << std::endl;
#endif
  bool res = true;

  // CXXFoldExpr represents C++17 fold expressions like (... && args)
  // These are template-dependent, use placeholder for now
  *node = SageBuilder::buildNullExpression();

  return VisitExpr(cxx_fold_expr, node) && res;
}

bool ClangToSageTranslator::VisitCXXInheritedCtorInitExpr(
    clang::CXXInheritedCtorInitExpr *cxx_inherited_ctor_init_expr,
    SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitCXXInheritedCtorInitExpr"
            << std::endl;
#endif
  bool res = true;

  // TODO

  return VisitExpr(cxx_inherited_ctor_init_expr, node) && res;
}

bool ClangToSageTranslator::VisitCXXNewExpr(clang::CXXNewExpr *cxx_new_expr,
                                            SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitCXXNewExpr" << std::endl;
#endif
  bool res = true;

  // ROOT CAUSE FIX: Implement new expression support
  // Get the allocated type
  SgType *allocated_type =
      buildTypeFromQualifiedType(cxx_new_expr->getAllocatedType());

  // Handle array size if this is array new
  SgExpression *array_size = NULL;
  if (cxx_new_expr->isArray()) {
    if (clang::Expr *size_expr =
            cxx_new_expr->getArraySize().value_or(nullptr)) {
      SgNode *tmp_size = Traverse(size_expr);
      array_size = isSgExpression(tmp_size);
    }
  }

  // Handle initializer (constructor call)
  SgConstructorInitializer *ctor_init = NULL;
  if (cxx_new_expr->hasInitializer()) {
    clang::Expr *initializer = cxx_new_expr->getInitializer();
    if (initializer != NULL) {
      SgNode *tmp_init = Traverse(initializer);
      // The initializer might be a CXXConstructExpr or other expression
      ctor_init = isSgConstructorInitializer(tmp_init);
    }
  }

  // Build the expression list for array new (if any)
  SgExprListExp *array_expr_list = NULL;
  if (array_size != NULL) {
    array_expr_list = SageBuilder::buildExprListExp(array_size);
  }

  // Build the new expression
  // buildNewExp(type, exprListExp, constInit, expr, val, funcDecl)
  SgNewExp *new_exp =
      SageBuilder::buildNewExp(allocated_type,  // type
                               array_expr_list, // exprListExp (array size list)
                               ctor_init, // constInit (constructor initializer)
                               NULL,      // expr (placement new expression)
                               0,   // val (need_global_specifier as short)
                               NULL // funcDecl (operator new function)
      );

  *node = new_exp;

  return VisitExpr(cxx_new_expr, node) && res;
}

bool ClangToSageTranslator::VisitCXXNoexceptExpr(
    clang::CXXNoexceptExpr *cxx_noexcept_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitCXXNoexceptExpr" << std::endl;
#endif
  bool res = true;

  if (cxx_noexcept_expr->isValueDependent()) {
    // Value-dependent noexcept results can't be evaluated until instantiation
    // Create a placeholder expression so translation can proceed
    *node = SageBuilder::buildNullExpression();
    if (SgExpression *expr = isSgExpression(*node)) {
      expr->get_file_info()->setCompilerGenerated();
    }
  } else {
    // noexcept operator evaluates at compile-time whether an expression can
    // throw
    bool can_throw = cxx_noexcept_expr->getValue();
    *node = SageBuilder::buildBoolValExp(can_throw);
  }

  if (SgExpression *expr = isSgExpression(*node)) {
    applySourceRange(expr, cxx_noexcept_expr->getSourceRange());
  }

  return VisitExpr(cxx_noexcept_expr, node) && res;
}

bool ClangToSageTranslator::VisitCXXNullPtrLiteralExpr(
    clang::CXXNullPtrLiteralExpr *cxx_null_ptr_literal_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitCXXNullPtrLiteralExpr" << std::endl;
#endif
  bool res = true;

  // ROOT CAUSE FIX: CXXNullPtrLiteralExpr represents C++11's nullptr literal
  // nullptr is a null pointer constant of type std::nullptr_t
  // In SAGE, we represent it with SgNullptrValExp

  // Get the type (should be std::nullptr_t)
  SgType *nullptr_type =
      buildTypeFromQualifiedType(cxx_null_ptr_literal_expr->getType());

  if (nullptr_type == NULL) {
    // Fallback: if type conversion fails, use void pointer type
    nullptr_type = SageBuilder::buildPointerType(SageBuilder::buildVoidType());
  }

  // Create the nullptr value expression
  *node = SageBuilder::buildNullptrValExp();

  return VisitExpr(cxx_null_ptr_literal_expr, node) && res;
}

bool ClangToSageTranslator::VisitCXXPseudoDestructorExpr(
    clang::CXXPseudoDestructorExpr *cxx_pseudo_destructor_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitCXXPseudoDestructorExpr"
            << std::endl;
#endif
  bool res = true;

  // Clang models pseudo-destructor calls as
  // CallExpr(callee=CXXPseudoDestructorExpr). Build the callee structurally so
  // the enclosing SgFunctionCallExp can emit the call parentheses without
  // duplication.

  SgNode *tmp_base = Traverse(cxx_pseudo_destructor_expr->getBase());
  SgExpression *base = isSgExpression(tmp_base);
  ROSE_ASSERT(base != NULL);

  SgType *destroyed_type = NULL;
  if (clang::TypeSourceInfo *type_info =
          cxx_pseudo_destructor_expr->getDestroyedTypeInfo()) {
    destroyed_type = buildTypeFromQualifiedType(type_info->getType());
  } else {
    clang::QualType qual_type = cxx_pseudo_destructor_expr->getDestroyedType();
    if (!qual_type.isNull()) {
      destroyed_type = buildTypeFromQualifiedType(qual_type);
    } else if (const clang::IdentifierInfo *id =
                   cxx_pseudo_destructor_expr->getDestroyedTypeIdentifier()) {
      destroyed_type =
          SageBuilder::buildTemplateType(SgName(id->getName().str()));
    }
  }
  ROSE_ASSERT(destroyed_type != NULL);

  Sg_File_Info *file_info =
      Sg_File_Info::generateDefaultFileInfoForCompilerGeneratedNode();
  ROSE_ASSERT(file_info != NULL);

  SgPseudoDestructorRefExp *pseudo_dtor =
      new SgPseudoDestructorRefExp(file_info, destroyed_type);
  ROSE_ASSERT(pseudo_dtor != NULL);
  pseudo_dtor->post_construction_initialization();

  clang::SourceLocation tilde_loc = cxx_pseudo_destructor_expr->getTildeLoc();
  clang::SourceLocation name_end_loc;
  if (clang::TypeSourceInfo *type_info =
          cxx_pseudo_destructor_expr->getDestroyedTypeInfo()) {
    name_end_loc = type_info->getTypeLoc().getEndLoc();
  }
  if (tilde_loc.isValid() && name_end_loc.isValid()) {
    applySourceRange(pseudo_dtor, clang::SourceRange(tilde_loc, name_end_loc));
  } else {
    applySourceRange(pseudo_dtor, cxx_pseudo_destructor_expr->getSourceRange());
  }

  SgExpression *callee = NULL;
  if (cxx_pseudo_destructor_expr->isArrow()) {
    callee =
        SageBuilder::buildBinaryExpression_nfi<SgArrowExp>(base, pseudo_dtor);
  } else {
    callee =
        SageBuilder::buildBinaryExpression_nfi<SgDotExp>(base, pseudo_dtor);
  }
  ROSE_ASSERT(callee != NULL);
  applySourceRange(callee, cxx_pseudo_destructor_expr->getSourceRange());

  *node = callee;

  return VisitExpr(cxx_pseudo_destructor_expr, node) && res;
}

// bool
// ClangToSageTranslator::VisitCXXRewrittenBinaryOperator(clang::CXXRewrittenBinaryOperator
// * cxx_rewrite_binary_operator, SgNode ** node) { #if DEBUG_VISIT_STMT
//     std::cerr << "ClangToSageTranslator::VisitCXXRewrittenBinaryOperator" <<
//     std::endl;
// #endif
//     bool res = true;
//
//     // TODO
//
//     return VisitExpr(cxx_rewrite_binary_operator, node) && res;
// }

bool ClangToSageTranslator::VisitCXXScalarValueInitExpr(
    clang::CXXScalarValueInitExpr *cxx_scalar_value_init_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitCXXScalarValueInitExpr"
            << std::endl;
#endif
  bool res = true;

  // ROOT CAUSE FIX: CXXScalarValueInitExpr represents value initialization of
  // scalar types Examples: int(), char(), double*() - all initialize to
  // zero/null This is equivalent to a cast expression with default
  // initialization

  // Get the type being initialized
  SgType *type =
      buildTypeFromQualifiedType(cxx_scalar_value_init_expr->getType());

  if (type == NULL) {
    type = SageBuilder::buildVoidType();
  }

  // Create a cast expression with a null operand to represent the default
  // initialization The cast will produce the zero-initialized value of the
  // target type
  *node = SageBuilder::buildCastExp(SageBuilder::buildIntVal(0), type);

  return VisitExpr(cxx_scalar_value_init_expr, node) && res;
}

bool ClangToSageTranslator::VisitCXXStdInitializerListExpr(
    clang::CXXStdInitializerListExpr *cxx_std_initializer_list_expr,
    SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitCXXStdInitializerListExpr"
            << std::endl;
#endif
  bool res = true;

  // TODO

  return VisitExpr(cxx_std_initializer_list_expr, node) && res;
}

bool ClangToSageTranslator::VisitCXXThisExpr(clang::CXXThisExpr *cxx_this_expr,
                                             SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitCXXThisExpr" << std::endl;
#endif
  bool res = true;

  SgSymbol *this_symbol = findEnclosingThisSymbol(SageBuilder::topScopeStack());
  if (this_symbol == NULL) {
    const clang::Type *this_type_ptr =
        cxx_this_expr->getType().getTypePtrOrNull();
    const clang::PointerType *pointer_type =
        this_type_ptr != NULL ? this_type_ptr->getAs<clang::PointerType>()
                              : NULL;
    const clang::CXXRecordDecl *record_decl = NULL;
    if (pointer_type != NULL) {
      record_decl = pointer_type->getPointeeType()->getAsCXXRecordDecl();
    }

    if (record_decl != NULL) {
      const clang::CXXRecordDecl *canonical_decl =
          record_decl->getCanonicalDecl();
      auto it = p_decl_translation_map.find(
          const_cast<clang::CXXRecordDecl *>(canonical_decl));
      if (it == p_decl_translation_map.end()) {
        it = p_decl_translation_map.find(
            const_cast<clang::CXXRecordDecl *>(record_decl));
      }

      SgClassDeclaration *class_decl = NULL;
      if (it != p_decl_translation_map.end()) {
        class_decl = isSgClassDeclaration(it->second);
      } else {
        SgNode *tmp_class =
            Traverse(const_cast<clang::CXXRecordDecl *>(canonical_decl));
        class_decl = isSgClassDeclaration(tmp_class);
      }

      if (class_decl != NULL && class_decl->get_scope() != NULL) {
        SgScopeStatement *decl_scope = class_decl->get_scope();
        SgName class_name = class_decl->get_name();

        // Prefer the symbol already associated with the declaration. This
        // preserves correct symbol kinds (e.g., SgTemplateClassSymbol for
        // SgTemplateClassDeclaration) and avoids inserting invalid
        // SgClassSymbol entries that trip AstConsistencyTests.
        SgSymbol *class_sym = class_decl->get_symbol_from_symbol_table();
        if (class_sym == NULL) {
          class_sym = decl_scope->lookup_symbol(class_name);
        }

        if (class_sym == NULL) {
          if (SgTemplateClassDeclaration *tmpl_decl =
                  isSgTemplateClassDeclaration(class_decl)) {
            class_sym = new SgTemplateClassSymbol(tmpl_decl);
          } else {
            class_sym = new SgClassSymbol(class_decl);
          }
          decl_scope->insert_symbol(class_name, class_sym);
        }

        this_symbol = class_sym;
      }
    }
  }

  ROSE_ASSERT(this_symbol != NULL);

  SgThisExp *this_exp = SageBuilder::buildThisExp_nfi(this_symbol);
  *node = this_exp;

  return VisitExpr(cxx_this_expr, node) && res;
}

bool ClangToSageTranslator::VisitCXXThrowExpr(
    clang::CXXThrowExpr *cxx_throw_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitCXXThrowExpr" << std::endl;
#endif
  bool res = true;

  // ROOT CAUSE FIX: CXXThrowExpr represents C++ throw expressions
  // Can be either "throw expr;" or bare "throw;" (rethrow)
  SgExpression *throw_operand = NULL;
  SgThrowOp::e_throw_kind throw_kind;

  // Check if this is a rethrow (bare "throw;") or throw with expression
  clang::Expr *sub_expr = cxx_throw_expr->getSubExpr();
  if (sub_expr != NULL) {
    // Regular throw with an expression
    SgNode *tmp_expr = Traverse(sub_expr);
    throw_operand = isSgExpression(tmp_expr);
    if (throw_operand == NULL) {
      std::cerr << "Error: Failed to convert throw operand expression"
                << std::endl;
      return false;
    }
    throw_kind = SgThrowOp::throw_expression;
  } else {
    // Rethrow (bare "throw;")
    throw_kind = SgThrowOp::rethrow;
  }

  // Build the throw operation
  SgThrowOp *throw_op = SageBuilder::buildThrowOp(throw_operand, throw_kind);
  ROSE_ASSERT(throw_op != NULL);

  *node = throw_op;

  return VisitExpr(cxx_throw_expr, node) && res;
}

bool ClangToSageTranslator::VisitCXXTypeidExpr(
    clang::CXXTypeidExpr *cxx_typeid_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitCXXTypeidExpr" << std::endl;
#endif
  bool res = true;

  // TODO

  return VisitExpr(cxx_typeid_expr, node) && res;
}

bool ClangToSageTranslator::VisitCXXUnresolvedConstructExpr(
    clang::CXXUnresolvedConstructExpr *cxx_unresolved_construct_expr,
    SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitCXXUnresolvedConstructExpr"
            << std::endl;
#endif
  bool res = true;

  // ROOT CAUSE FIX: Template-dependent constructor calls (e.g., T(args) where T
  // is a template parameter) Build a proper constructor call expression instead
  // of using null placeholder

  // Get the type being constructed (may be a dependent type)
  SgType *type = buildTypeFromQualifiedType(
      cxx_unresolved_construct_expr->getTypeAsWritten());

  // Build expression list for constructor arguments
  SgExprListExp *args = SageBuilder::buildExprListExp_nfi();
  for (unsigned i = 0; i < cxx_unresolved_construct_expr->getNumArgs(); i++) {
    SgNode *tmp_expr = Traverse(cxx_unresolved_construct_expr->getArg(i));
    SgExpression *arg = isSgExpression(tmp_expr);
    if (arg != NULL) {
      args->append_expression(arg);
    }
  }

  // Build constructor initializer for the unresolved construct
  SgConstructorInitializer *ctor_init =
      SageBuilder::buildConstructorInitializer_nfi(
          NULL, // declaration will be NULL for unresolved/dependent
                // constructors
          args, type,
          false, // need_name
          false, // need_qualifier
          false, // need_parenthesis_after_name
          true // associated_class_unknown - set to true for template-dependent
               // types
      );

  *node = ctor_init;

  return VisitExpr(cxx_unresolved_construct_expr, node) && res;
}

bool ClangToSageTranslator::VisitCXXUuidofExpr(
    clang::CXXUuidofExpr *cxx_uuidof_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitCXXUuidofExpr" << std::endl;
#endif
  bool res = true;

  // TODO

  return VisitExpr(cxx_uuidof_expr, node) && res;
}

bool ClangToSageTranslator::VisitDeclRefExpr(clang::DeclRefExpr *decl_ref_expr,
                                             SgNode **node) {
  if (clang::NonTypeTemplateParmDecl *non_type_param =
          llvm::dyn_cast<clang::NonTypeTemplateParmDecl>(
              decl_ref_expr->getDecl())) {
    SgDeclarationStatement *owning_template = NULL;
    if (clang::DeclContext *ctx = non_type_param->getDeclContext()) {
      if (clang::TemplateDecl *template_ctx =
              llvm::dyn_cast<clang::TemplateDecl>(ctx)) {
        auto it = p_decl_translation_map.find(template_ctx);
        if (it != p_decl_translation_map.end()) {
          owning_template = isSgDeclarationStatement(it->second);
        }
      }
    }

    unsigned position = non_type_param->getIndex();
    SgTemplateParameter *sg_param =
        translateTemplateParameter(non_type_param, owning_template, position);

    if (sg_param != NULL) {
      SgTemplateParameterVal *param_val =
          SageBuilder::buildTemplateParameterVal(non_type_param->getIndex());
      param_val->set_valueString(non_type_param->getNameAsString());
      if (sg_param->get_type() != NULL) {
        param_val->set_valueType(sg_param->get_type());
      }
      applySourceRange(param_val, decl_ref_expr->getSourceRange());
      *node = param_val;
      return true;
    }
  }

  bool res = true;
  auto attach_explicit_qualifier = [&](SgExpression *expr) {
    if (expr == nullptr || decl_ref_expr == nullptr ||
        !decl_ref_expr->hasQualifier()) {
      return;
    }
    if (p_compiler_instance == nullptr) {
      return;
    }
    const clang::NestedNameSpecifier *qualifier = decl_ref_expr->getQualifier();
    if (qualifier == nullptr) {
      return;
    }
    const ExplicitQualifierInfo info = getExplicitQualifierInfo(
        qualifier, p_compiler_instance->getASTContext());
    setExplicitQualifierOnExpr(expr, info);
  };

  // Phase C (Issue 115): Queue implicit template instantiations that are
  // referenced from user code. Translation is deferred until the TU pass
  // completes to avoid ordering issues.
  if (clang::FunctionDecl *func_decl =
          llvm::dyn_cast<clang::FunctionDecl>(decl_ref_expr->getDecl())) {
    clang::TemplateSpecializationKind kind =
        func_decl->getTemplateSpecializationKind();
    if ((kind == clang::TSK_ImplicitInstantiation ||
         kind == clang::TSK_ExplicitInstantiationDefinition) &&
        func_decl->hasBody()) {
      bool eligible = true;
      if (p_compiler_instance != nullptr) {
        clang::SourceManager &sm = p_compiler_instance->getSourceManager();
        clang::SourceLocation loc = func_decl->getLocation();
        if (!loc.isValid() || sm.isInSystemHeader(loc) ||
            sm.isWrittenInBuiltinFile(loc)) {
          eligible = false;
        }
      }

      if (eligible &&
          p_pending_implicit_function_instantiations_set.insert(func_decl)
              .second) {
        p_pending_implicit_function_instantiations.push_back(func_decl);
      }
    }
  }

  // SgNode * tmp_node = Traverse(decl_ref_expr->getDecl());
  // DONE: Do not use Traverse(...) as the declaration can not be complete
  // (recursive functions)
  //       Instead use SymbolTable from ROSE as the symbol should be ready
  //       (cannot have a reference before the declaration)
  // FIXME: This fix will not work for C++ (methods/fields can be use before
  // they are declared...)
  // FIXME: I feel like it could work now, we will see ....

  SgSymbol *sym = GetSymbolFromSymbolTable(decl_ref_expr->getDecl());

  if (sym == NULL) {
    // If Clang resolved a templated member call to a concrete FunctionDecl,
    // traversing that declaration can build multiple "foo" declarations that
    // collide (same name/signature) across different template arguments.
    // For references with explicit template arguments, build an explicit
    // template instantiation ref directly instead of traversing the resolved
    // specialization.
    if (decl_ref_expr->hasExplicitTemplateArgs()) {
      if (clang::FunctionDecl *clang_func =
              llvm::dyn_cast<clang::FunctionDecl>(decl_ref_expr->getDecl())) {
        if (clang::CXXMethodDecl *method_decl =
                llvm::dyn_cast<clang::CXXMethodDecl>(clang_func)) {
          clang::TemplateArgumentListInfo arg_info;
          decl_ref_expr->copyTemplateArgumentsInto(arg_info);
          const size_t explicit_arg_count =
              countExpandedTemplateArguments(arg_info);

          clang::CXXRecordDecl *parent_decl = method_decl->getParent();
          SgScopeStatement *class_scope = NULL;
          if (parent_decl != NULL) {
            SgNode *parent_node = NULL;
            std::map<clang::Decl *, SgNode *>::iterator it_decl =
                p_decl_translation_map.find(parent_decl);
            if (it_decl != p_decl_translation_map.end()) {
              parent_node = it_decl->second;
            } else {
              parent_node = Traverse(parent_decl);
            }

            if (SgClassDeclaration *class_decl =
                    isSgClassDeclaration(parent_node)) {
              class_scope = class_decl->get_definition();
            } else if (SgClassDefinition *class_def =
                           isSgClassDefinition(parent_node)) {
              class_scope = class_def;
            } else if (SgTemplateClassDeclaration *template_class_decl =
                           isSgTemplateClassDeclaration(parent_node)) {
              class_scope = template_class_decl->get_definition();
            }
          }

          if (class_scope != NULL) {
            unsigned int functionConstVolatileFlags = 0;
            if (method_decl->isConst()) {
              functionConstVolatileFlags |= SgMemberFunctionType::e_const;
            }
            if (method_decl->isVolatile()) {
              functionConstVolatileFlags |= SgMemberFunctionType::e_volatile;
            }
            clang::Qualifiers qualifiers = method_decl->getMethodQualifiers();
            if (qualifiers.hasRestrict()) {
              functionConstVolatileFlags |= SgMemberFunctionType::e_restrict;
            }
            switch (method_decl->getRefQualifier()) {
            case clang::RQ_LValue:
              functionConstVolatileFlags |=
                  SgMemberFunctionType::e_ref_qualifier_lvalue;
              break;
            case clang::RQ_RValue:
              functionConstVolatileFlags |=
                  SgMemberFunctionType::e_ref_qualifier_rvalue;
              break;
            case clang::RQ_None:
              break;
            }

            SgType *ret_type =
                buildTypeFromQualifiedType(method_decl->getReturnType());
            if (ret_type == NULL) {
              ret_type = SageBuilder::buildUnknownType();
            }

            SgFunctionParameterList *param_list = NULL;
            if (SgFunctionType *func_type = isSgFunctionType(
                    buildTypeFromQualifiedType(method_decl->getType()))) {
              if (func_type->get_argument_list() != NULL) {
                param_list = SageBuilder::buildFunctionParameterList_nfi(
                    func_type->get_argument_list());
              }
            }
            if (param_list == NULL) {
              param_list = SageBuilder::buildFunctionParameterList_nfi();
            }

            SgTemplateArgumentPtrList template_args;
            if (const clang::TemplateArgumentList *clang_args =
                    clang_func->getTemplateSpecializationArgs()) {
              if (clang_args->size() != 0) {
                template_args =
                    buildTemplateArguments(*clang_args, explicit_arg_count);
              } else {
                template_args = buildTemplateArguments(arg_info, true);
              }
            } else {
              template_args = buildTemplateArguments(arg_info, true);
            }
            SgTemplateArgumentPtrList *builder_args =
                new SgTemplateArgumentPtrList(template_args);
            SgTemplateInstantiationMemberFunctionDecl *inst_decl =
                isSgTemplateInstantiationMemberFunctionDecl(
                    SageBuilder::buildNondefiningMemberFunctionDeclaration(
                        SgName(method_decl->getNameAsString()), ret_type,
                        param_list, class_scope, /*decoratorList=*/NULL,
                        functionConstVolatileFlags,
                        /*buildTemplateInstantiation=*/true, builder_args));

            if (inst_decl != NULL) {
              inst_decl->set_template_argument_list_is_explicit(true);
              SageBuilder::setTemplateArgumentsInDeclaration(inst_decl,
                                                             &template_args);

              if (inst_decl->get_templateDeclaration() == NULL) {
                clang::FunctionTemplateDecl *primary_template =
                    clang_func->getPrimaryTemplate();
                if (primary_template == NULL) {
                  primary_template = clang_func->getDescribedFunctionTemplate();
                }
                if (primary_template != NULL) {
                  SgNode *tmpl_node = Traverse(primary_template);
                  if (SgTemplateMemberFunctionDeclaration *tmpl_decl =
                          isSgTemplateMemberFunctionDeclaration(tmpl_node)) {
                    inst_decl->set_templateDeclaration(tmpl_decl);
                    inst_decl->set_templateName(tmpl_decl->get_name());
                  }
                }
              }

              if (SgMemberFunctionSymbol *inst_sym = isSgMemberFunctionSymbol(
                      inst_decl->get_symbol_from_symbol_table())) {
                SgExpression *ref_exp =
                    SageBuilder::buildMemberFunctionRefExp_nfi(inst_sym, false,
                                                               false);
                attach_explicit_qualifier(ref_exp);
                *node = ref_exp;
                return VisitExpr(decl_ref_expr, node) && res;
              }
            }
          }
        }
      }
    }

    SgNode *tmp_decl = Traverse(decl_ref_expr->getDecl());

    // DQ (11/29/2020): Added assertion.
    ROSE_ASSERT(tmp_decl != NULL);

#if DEBUG_VISIT_STMT
    printf("tmp_decl = %p = %s \n", tmp_decl, tmp_decl->class_name().c_str());
#endif
    SgInitializedName *initializedName = isSgInitializedName(tmp_decl);
#if DEBUG_VISIT_STMT
    if (initializedName != NULL) {
      printf("Found SgInitializedName: initializedName->get_name() = %s \n",
             initializedName->get_name().str());
    }
#endif

    if (tmp_decl != NULL) {
      sym = GetSymbolFromSymbolTable(decl_ref_expr->getDecl());
    }

    // FIXME hack Traverse have added the symbol but we cannot find it
    // (probably: problem with type and function lookup)

    if (sym == NULL && isSgFunctionDeclaration(tmp_decl) != NULL) {
      if (SgMemberFunctionDeclaration *member_func_decl =
              isSgMemberFunctionDeclaration(tmp_decl)) {
        SgScopeStatement *decl_scope = member_func_decl->get_scope();
        SgFunctionSymbol *existing_sym = NULL;
        if (decl_scope != NULL) {
          existing_sym = decl_scope->lookup_function_symbol(
              member_func_decl->get_name(), member_func_decl->get_type());
        }
        if (SgMemberFunctionSymbol *member_sym =
                isSgMemberFunctionSymbol(existing_sym)) {
          sym = member_sym;
        } else {
          if (existing_sym != NULL && decl_scope != NULL) {
            decl_scope->remove_symbol(existing_sym);
          }
          SgMemberFunctionSymbol *new_sym =
              new SgMemberFunctionSymbol(member_func_decl);
          if (decl_scope != NULL) {
            decl_scope->insert_symbol(member_func_decl->get_name(), new_sym);
          }
          sym = new_sym;
        }
      } else {
        SgFunctionDeclaration *func_decl = isSgFunctionDeclaration(tmp_decl);
        SgFunctionSymbol *new_sym = new SgFunctionSymbol(func_decl);
        if (SgScopeStatement *decl_scope = func_decl->get_scope()) {
          decl_scope->insert_symbol(func_decl->get_name(), new_sym);
        } else {
          new_sym->set_parent(func_decl);
        }
        sym = new_sym;
      }
    }
    // ROOT CAUSE FIX: Handle SgVariableDeclaration from VisitVarDecl
    // Extract the InitializedName and create symbol if needed
    if (sym == NULL && isSgVariableDeclaration(tmp_decl) != NULL) {
      SgVariableDeclaration *var_decl_result =
          isSgVariableDeclaration(tmp_decl);
      if (var_decl_result->get_variables().size() > 0) {
        SgInitializedName *init_name = var_decl_result->get_variables()[0];
        if (init_name != NULL) {
          // Try to get existing symbol first
          SgScopeStatement *init_scope = init_name->get_scope();
          if (init_scope != NULL) {
            sym = init_scope->lookup_variable_symbol(init_name->get_name());
          }
          // If still not found, create new symbol
          if (sym == NULL) {
            sym = new SgVariableSymbol(init_name);
            sym->set_parent(init_name);
            if (init_scope != NULL) {
              init_scope->insert_symbol(init_name->get_name(), sym);
            }
          }
        }
      }
    }
    // Pei-Hung (04/07/2022) sym can be NULL in the case for C99 VLA
    if (sym == NULL && isSgInitializedName(tmp_decl) != NULL) {
      sym = new SgVariableSymbol(isSgInitializedName(tmp_decl));
      sym->set_parent(tmp_decl);
      SageBuilder::topScopeStack()->insert_symbol(
          isSgInitializedName(tmp_decl)->get_name(), sym);
    }
  }

  if (sym != NULL) {
    // Not else if it was NULL we have try to traverse it....
    SgVariableSymbol *var_sym = isSgVariableSymbol(sym);
    SgMemberFunctionSymbol *member_func_sym = isSgMemberFunctionSymbol(sym);
    SgFunctionSymbol *func_sym = isSgFunctionSymbol(sym);
    SgEnumFieldSymbol *enum_sym = isSgEnumFieldSymbol(sym);

    if (var_sym != NULL) {
      SgExpression *ref_exp = SageBuilder::buildVarRefExp(var_sym);
      attach_explicit_qualifier(ref_exp);
      *node = ref_exp;
    } else {
      SgMemberFunctionSymbol *member_sym = member_func_sym;
      if (member_sym == NULL && func_sym != NULL) {
        if (SgMemberFunctionDeclaration *member_decl =
                isSgMemberFunctionDeclaration(func_sym->get_declaration())) {
          SgScopeStatement *decl_scope = member_decl->get_scope();
          SgFunctionSymbol *existing_sym = NULL;
          if (decl_scope != NULL) {
            existing_sym = decl_scope->lookup_function_symbol(
                member_decl->get_name(), member_decl->get_type());
          }

          member_sym = isSgMemberFunctionSymbol(existing_sym);
          if (member_sym == NULL) {
            if (existing_sym != NULL && decl_scope != NULL) {
              decl_scope->remove_symbol(existing_sym);
            }
            member_sym = new SgMemberFunctionSymbol(member_decl);
            if (decl_scope != NULL) {
              decl_scope->insert_symbol(member_decl->get_name(), member_sym);
            }
          }
        }
      }

      clang::FunctionDecl *clang_func =
          llvm::dyn_cast<clang::FunctionDecl>(decl_ref_expr->getDecl());
      auto get_explicit_template_arg_info =
          [&](clang::TemplateArgumentListInfo &arg_info,
              size_t &explicit_arg_count) -> bool {
        if (!decl_ref_expr->hasExplicitTemplateArgs()) {
          return false;
        }

        decl_ref_expr->copyTemplateArgumentsInto(arg_info);
        explicit_arg_count = countExpandedTemplateArguments(arg_info);
        return true;
      };

      clang::TemplateArgumentListInfo explicit_arg_info;
      size_t explicit_arg_count = 0;
      const bool has_explicit_template_args =
          get_explicit_template_arg_info(explicit_arg_info, explicit_arg_count);
      auto get_decl_namespace_scope =
          [&](clang::NamedDecl *clang_decl) -> SgScopeStatement * {
        if (clang_decl == NULL) {
          return NULL;
        }

        clang::DeclContext *ctx = clang_decl->getDeclContext();
        while (ctx != NULL && !ctx->isTranslationUnit() &&
               !llvm::isa<clang::NamespaceDecl>(ctx)) {
          ctx = ctx->getParent();
        }

        clang::NamespaceDecl *ns_decl =
            llvm::dyn_cast_or_null<clang::NamespaceDecl>(ctx);
        if (ns_decl == NULL) {
          return NULL;
        }

        SgScopeStatement *ns_scope = NULL;
        auto it_decl = p_decl_translation_map.find(ns_decl);
        if (it_decl != p_decl_translation_map.end()) {
          if (SgNamespaceDeclarationStatement *ns_stmt =
                  isSgNamespaceDeclarationStatement(it_decl->second)) {
            ns_scope = ns_stmt->get_definition();
          } else if (SgNamespaceDefinitionStatement *ns_def =
                         isSgNamespaceDefinitionStatement(it_decl->second)) {
            ns_scope = ns_def;
          }
        }

        if (ns_scope == NULL) {
          if (SgNamespaceDeclarationStatement *ns_stmt =
                  ensureNamespaceDeclaration(ns_decl)) {
            ns_scope = ns_stmt->get_definition();
          }
        }

        return normalizeNamespaceScope(ns_scope);
      };

      if (member_sym != NULL) {
        SgMemberFunctionSymbol *ref_member_sym = member_sym;
        if (has_explicit_template_args) {
          SgTemplateArgumentPtrList template_args;
          bool template_args_built = false;
          auto ensure_template_args = [&]() -> SgTemplateArgumentPtrList * {
            if (!template_args_built) {
              if (clang_func != NULL) {
                if (const clang::TemplateArgumentList *clang_args =
                        clang_func->getTemplateSpecializationArgs()) {
                  if (clang_args->size() != 0) {
                    template_args =
                        buildTemplateArguments(*clang_args, explicit_arg_count);
                  } else {
                    template_args =
                        buildTemplateArguments(explicit_arg_info, true);
                  }
                } else {
                  template_args =
                      buildTemplateArguments(explicit_arg_info, true);
                }
              } else {
                template_args = buildTemplateArguments(explicit_arg_info, true);
              }
              template_args_built = true;
            }
            return &template_args;
          };

          SgMemberFunctionDeclaration *member_decl =
              isSgMemberFunctionDeclaration(member_sym->get_declaration());
          SgScopeStatement *func_scope =
              member_decl != NULL ? member_decl->get_scope() : NULL;
          if (func_scope == NULL) {
            func_scope = SageBuilder::topScopeStack();
          }

          SgName template_base_name = member_sym->get_name();
          if (member_decl != NULL) {
            if (SgTemplateInstantiationMemberFunctionDecl *existing_inst =
                    isSgTemplateInstantiationMemberFunctionDecl(member_decl)) {
              if (existing_inst->get_templateName().is_null() == false) {
                template_base_name = existing_inst->get_templateName();
              }
            } else if (SgTemplateInstantiationMemberFunctionDecl
                           *existing_inst =
                               isSgTemplateInstantiationMemberFunctionDecl(
                                   member_decl
                                       ->get_firstNondefiningDeclaration())) {
              if (existing_inst->get_templateName().is_null() == false) {
                template_base_name = existing_inst->get_templateName();
              }
            }
          }

          bool reused_instantiation = false;
          if (SgTemplateInstantiationMemberFunctionDecl *inst_decl =
                  isSgTemplateInstantiationMemberFunctionDecl(member_decl)) {
            inst_decl->set_template_argument_list_is_explicit(
                has_explicit_template_args);
            SageBuilder::setTemplateArgumentsInDeclaration(
                inst_decl, ensure_template_args());
            if (inst_decl->get_templateName().is_null() &&
                template_base_name.is_null() == false) {
              inst_decl->set_templateName(template_base_name);
            }
            reused_instantiation = true;
          } else if (member_decl != NULL) {
            if (SgTemplateInstantiationMemberFunctionDecl *inst_decl =
                    isSgTemplateInstantiationMemberFunctionDecl(
                        member_decl->get_firstNondefiningDeclaration())) {
              inst_decl->set_template_argument_list_is_explicit(
                  has_explicit_template_args);
              SageBuilder::setTemplateArgumentsInDeclaration(
                  inst_decl, ensure_template_args());
              if (inst_decl->get_templateName().is_null() &&
                  template_base_name.is_null() == false) {
                inst_decl->set_templateName(template_base_name);
              }
              reused_instantiation = true;
            }
          }

          if (!reused_instantiation) {
            SgType *sym_type = member_sym->get_type();
            if (sym_type == NULL && member_decl != NULL) {
              sym_type = member_decl->get_type();
            }

            SgType *lookup_type = sym_type;
            if (clang::FunctionDecl *clang_func =
                    llvm::dyn_cast<clang::FunctionDecl>(
                        decl_ref_expr->getDecl())) {
              if (SgType *clang_type =
                      buildTypeFromQualifiedType(clang_func->getType())) {
                if (isSgMemberFunctionType(clang_type) != NULL ||
                    lookup_type == NULL) {
                  lookup_type = clang_type;
                }
              }
            }

            SgFunctionType *func_type = isSgFunctionType(lookup_type);
            SgFunctionSymbol *existing_inst_sym = NULL;
            if (func_scope != NULL) {
              existing_inst_sym = func_scope->lookup_function_symbol(
                  template_base_name, lookup_type, ensure_template_args());
              if (existing_inst_sym == NULL && lookup_type != NULL) {
                SgFunctionSymbol *fallback_sym =
                    func_scope->lookup_function_symbol(template_base_name,
                                                       lookup_type);
                if (fallback_sym != NULL) {
                  if (isSgTemplateInstantiationMemberFunctionDecl(
                          fallback_sym->get_declaration()) != NULL) {
                    existing_inst_sym = fallback_sym;
                  } else if (SgMemberFunctionDeclaration *first_nondef =
                                 isSgMemberFunctionDeclaration(
                                     fallback_sym->get_declaration()
                                         ->get_firstNondefiningDeclaration())) {
                    if (isSgTemplateInstantiationMemberFunctionDecl(
                            first_nondef) != NULL) {
                      existing_inst_sym = fallback_sym;
                    }
                  }
                }
              }
            }

            SgMemberFunctionSymbol *inst_member_sym =
                isSgMemberFunctionSymbol(existing_inst_sym);
            SgTemplateInstantiationMemberFunctionDecl *inst_decl =
                inst_member_sym != NULL
                    ? isSgTemplateInstantiationMemberFunctionDecl(
                          inst_member_sym->get_declaration())
                    : NULL;
            if (inst_decl == NULL && inst_member_sym != NULL) {
              if (SgMemberFunctionDeclaration *first_nondef =
                      isSgMemberFunctionDeclaration(
                          inst_member_sym->get_declaration()
                              ->get_firstNondefiningDeclaration())) {
                inst_decl =
                    isSgTemplateInstantiationMemberFunctionDecl(first_nondef);
              }
            }

            if (inst_member_sym != NULL && inst_decl != NULL) {
              ref_member_sym = inst_member_sym;
              inst_decl->set_template_argument_list_is_explicit(
                  has_explicit_template_args);
              SageBuilder::setTemplateArgumentsInDeclaration(
                  inst_decl, ensure_template_args());
              if (inst_decl->get_templateName().is_null() &&
                  template_base_name.is_null() == false) {
                inst_decl->set_templateName(template_base_name);
              }
            } else {
              SgType *ret_type =
                  func_type != NULL ? func_type->get_return_type() : NULL;
              if (ret_type == NULL) {
                ret_type = SageBuilder::buildUnknownType();
              }

              SgFunctionParameterList *param_list = NULL;
              if (func_type != NULL && func_type->get_argument_list() != NULL) {
                param_list = SageBuilder::buildFunctionParameterList_nfi(
                    func_type->get_argument_list());
              } else {
                param_list = SageBuilder::buildFunctionParameterList_nfi();
              }

              unsigned int functionConstVolatileFlags = 0;
              if (SgMemberFunctionType *member_type =
                      isSgMemberFunctionType(sym_type)) {
                functionConstVolatileFlags = member_type->get_mfunc_specifier();
              }

              SgTemplateArgumentPtrList *builder_args =
                  new SgTemplateArgumentPtrList(*ensure_template_args());
              SgTemplateInstantiationMemberFunctionDecl *inst_decl =
                  isSgTemplateInstantiationMemberFunctionDecl(
                      SageBuilder::buildNondefiningMemberFunctionDeclaration(
                          template_base_name, ret_type, param_list, func_scope,
                          /*decoratorList=*/NULL, functionConstVolatileFlags,
                          /*buildTemplateInstantiation=*/true, builder_args));

              if (inst_decl != NULL) {
                inst_decl->set_template_argument_list_is_explicit(
                    has_explicit_template_args);
                SageBuilder::setTemplateArgumentsInDeclaration(
                    inst_decl, ensure_template_args());

                if (member_decl != NULL) {
                  if (SgTemplateMemberFunctionDeclaration *tmpl_decl =
                          isSgTemplateMemberFunctionDeclaration(member_decl)) {
                    inst_decl->set_templateDeclaration(tmpl_decl);
                    inst_decl->set_templateName(tmpl_decl->get_name());
                  } else if (SgTemplateMemberFunctionDeclaration *first_nondef =
                                 isSgTemplateMemberFunctionDeclaration(
                                     member_decl
                                         ->get_firstNondefiningDeclaration())) {
                    inst_decl->set_templateDeclaration(first_nondef);
                    inst_decl->set_templateName(first_nondef->get_name());
                  }
                }

                if (inst_decl->get_templateDeclaration() == NULL) {
                  if (clang::FunctionDecl *clang_func =
                          llvm::dyn_cast<clang::FunctionDecl>(
                              decl_ref_expr->getDecl())) {
                    clang::FunctionTemplateDecl *primary_template =
                        clang_func->getPrimaryTemplate();
                    if (primary_template == NULL) {
                      primary_template =
                          clang_func->getDescribedFunctionTemplate();
                    }

                    if (primary_template != NULL) {
                      SgNode *tmpl_node = Traverse(primary_template);
                      if (SgTemplateMemberFunctionDeclaration *tmpl_decl =
                              isSgTemplateMemberFunctionDeclaration(
                                  tmpl_node)) {
                        inst_decl->set_templateDeclaration(tmpl_decl);
                        inst_decl->set_templateName(tmpl_decl->get_name());
                      }
                    }
                  }
                }

                SgMemberFunctionSymbol *inst_sym = isSgMemberFunctionSymbol(
                    inst_decl->get_symbol_from_symbol_table());
                if (inst_sym == NULL) {
                  if (SgMemberFunctionDeclaration *first_nondef =
                          isSgMemberFunctionDeclaration(
                              inst_decl->get_firstNondefiningDeclaration())) {
                    inst_sym = isSgMemberFunctionSymbol(
                        first_nondef->get_symbol_from_symbol_table());
                  }
                }
                if (inst_sym == NULL && func_scope != NULL) {
                  SgFunctionSymbol *lookup_sym =
                      func_scope->lookup_function_symbol(
                          template_base_name, lookup_type,
                          ensure_template_args());
                  if (lookup_sym == NULL && lookup_type != NULL) {
                    lookup_sym = func_scope->lookup_function_symbol(
                        template_base_name, lookup_type);
                  }
                  if (lookup_sym != NULL) {
                    if (SgTemplateInstantiationMemberFunctionDecl *lookup_decl =
                            isSgTemplateInstantiationMemberFunctionDecl(
                                lookup_sym->get_declaration())) {
                      inst_sym = isSgMemberFunctionSymbol(lookup_sym);
                    } else if (
                        SgMemberFunctionDeclaration *first_nondef =
                            isSgMemberFunctionDeclaration(
                                lookup_sym->get_declaration()
                                    ->get_firstNondefiningDeclaration())) {
                      if (isSgTemplateInstantiationMemberFunctionDecl(
                              first_nondef) != NULL) {
                        inst_sym = isSgMemberFunctionSymbol(lookup_sym);
                      }
                    }
                  }
                }
                if (inst_sym == NULL && func_scope != NULL) {
                  inst_sym = new SgMemberFunctionSymbol(inst_decl);
                  inst_sym->set_parent(inst_decl);
                  func_scope->insert_symbol(template_base_name, inst_sym);
                }
                if (inst_sym != NULL) {
                  ref_member_sym = inst_sym;
                }
              }
            }
          }
        }

        const bool need_qualifier = decl_ref_expr->hasQualifier();
        SgExpression *ref_exp = SageBuilder::buildMemberFunctionRefExp_nfi(
            ref_member_sym, false, need_qualifier);
        attach_explicit_qualifier(ref_exp);
        *node = ref_exp;
      } else if (func_sym != NULL) {
        SgFunctionSymbol *ref_func_sym = func_sym;
        if (has_explicit_template_args) {
          SgTemplateArgumentPtrList template_args;
          bool template_args_built = false;
          auto ensure_template_args = [&]() -> SgTemplateArgumentPtrList * {
            if (!template_args_built) {
              if (clang_func != NULL) {
                if (const clang::TemplateArgumentList *clang_args =
                        clang_func->getTemplateSpecializationArgs()) {
                  if (clang_args->size() != 0) {
                    template_args =
                        buildTemplateArguments(*clang_args, explicit_arg_count);
                  } else {
                    template_args =
                        buildTemplateArguments(explicit_arg_info, true);
                  }
                } else {
                  template_args =
                      buildTemplateArguments(explicit_arg_info, true);
                }
              } else {
                template_args = buildTemplateArguments(explicit_arg_info, true);
              }
              template_args_built = true;
            }
            return &template_args;
          };

          SgScopeStatement *func_scope = func_sym->get_scope();
          if (SgScopeStatement *decl_scope =
                  get_decl_namespace_scope(decl_ref_expr->getFoundDecl())) {
            func_scope = decl_scope;
          } else if (SgScopeStatement *decl_scope = get_decl_namespace_scope(
                         llvm::dyn_cast_or_null<clang::NamedDecl>(
                             decl_ref_expr->getDecl()))) {
            func_scope = decl_scope;
          }
          if (func_scope == NULL) {
            func_scope = SageBuilder::topScopeStack();
          }

          SgFunctionDeclaration *decl =
              isSgFunctionDeclaration(func_sym->get_declaration());
          SgName template_base_name = func_sym->get_name();
          if (decl != NULL) {
            if (SgTemplateInstantiationFunctionDecl *existing_inst =
                    isSgTemplateInstantiationFunctionDecl(decl)) {
              if (existing_inst->get_templateName().is_null() == false) {
                template_base_name = existing_inst->get_templateName();
              }
            } else if (SgTemplateInstantiationFunctionDecl *existing_inst =
                           isSgTemplateInstantiationFunctionDecl(
                               decl->get_firstNondefiningDeclaration())) {
              if (existing_inst->get_templateName().is_null() == false) {
                template_base_name = existing_inst->get_templateName();
              }
            }
          }

          bool reused_instantiation = false;
          if (SgTemplateInstantiationFunctionDecl *inst_decl =
                  isSgTemplateInstantiationFunctionDecl(decl)) {
            inst_decl->set_template_argument_list_is_explicit(
                has_explicit_template_args);
            SageBuilder::setTemplateArgumentsInDeclaration(
                inst_decl, ensure_template_args());
            if (inst_decl->get_templateName().is_null() &&
                template_base_name.is_null() == false) {
              inst_decl->set_templateName(template_base_name);
            }
            reused_instantiation = true;
          } else if (decl != NULL) {
            if (SgTemplateInstantiationFunctionDecl *inst_decl =
                    isSgTemplateInstantiationFunctionDecl(
                        decl->get_firstNondefiningDeclaration())) {
              inst_decl->set_template_argument_list_is_explicit(
                  has_explicit_template_args);
              SageBuilder::setTemplateArgumentsInDeclaration(
                  inst_decl, ensure_template_args());
              if (inst_decl->get_templateName().is_null() &&
                  template_base_name.is_null() == false) {
                inst_decl->set_templateName(template_base_name);
              }
              reused_instantiation = true;
            }
          }

          if (!reused_instantiation) {
            SgType *lookup_type = func_sym->get_type();
            if (clang::FunctionDecl *clang_func =
                    llvm::dyn_cast<clang::FunctionDecl>(
                        decl_ref_expr->getDecl())) {
              if (SgType *clang_type =
                      buildTypeFromQualifiedType(clang_func->getType())) {
                if (isSgFunctionType(clang_type) != NULL ||
                    lookup_type == NULL) {
                  lookup_type = clang_type;
                }
              }
            }

            SgFunctionType *func_type = isSgFunctionType(lookup_type);
            SgFunctionSymbol *existing_inst_sym = NULL;
            if (func_scope != NULL) {
              existing_inst_sym = func_scope->lookup_function_symbol(
                  template_base_name, lookup_type, ensure_template_args());
              if (existing_inst_sym == NULL && lookup_type != NULL) {
                SgFunctionSymbol *fallback_sym =
                    func_scope->lookup_function_symbol(template_base_name,
                                                       lookup_type);
                if (fallback_sym != NULL) {
                  if (isSgTemplateInstantiationFunctionDecl(
                          fallback_sym->get_declaration()) != NULL) {
                    existing_inst_sym = fallback_sym;
                  } else if (SgFunctionDeclaration *first_nondef =
                                 isSgFunctionDeclaration(
                                     fallback_sym->get_declaration()
                                         ->get_firstNondefiningDeclaration())) {
                    if (isSgTemplateInstantiationFunctionDecl(first_nondef) !=
                        NULL) {
                      existing_inst_sym = fallback_sym;
                    }
                  }
                }
              }
            }

            SgFunctionSymbol *inst_func_sym =
                isSgFunctionSymbol(existing_inst_sym);
            SgTemplateInstantiationFunctionDecl *inst_decl =
                inst_func_sym != NULL ? isSgTemplateInstantiationFunctionDecl(
                                            inst_func_sym->get_declaration())
                                      : NULL;
            if (inst_decl == NULL && inst_func_sym != NULL) {
              if (SgFunctionDeclaration *first_nondef = isSgFunctionDeclaration(
                      inst_func_sym->get_declaration()
                          ->get_firstNondefiningDeclaration())) {
                inst_decl = isSgTemplateInstantiationFunctionDecl(first_nondef);
              }
            }

            if (inst_func_sym != NULL && inst_decl != NULL) {
              ref_func_sym = inst_func_sym;
              inst_decl->set_template_argument_list_is_explicit(
                  has_explicit_template_args);
              SageBuilder::setTemplateArgumentsInDeclaration(
                  inst_decl, ensure_template_args());
              if (inst_decl->get_templateName().is_null() &&
                  template_base_name.is_null() == false) {
                inst_decl->set_templateName(template_base_name);
              }
            } else {
              SgType *ret_type =
                  func_type != NULL ? func_type->get_return_type() : NULL;
              if (ret_type == NULL) {
                ret_type = SageBuilder::buildUnknownType();
              }

              SgFunctionParameterList *param_list = NULL;
              if (func_type != NULL && func_type->get_argument_list() != NULL) {
                param_list = SageBuilder::buildFunctionParameterList_nfi(
                    func_type->get_argument_list());
              } else {
                param_list = SageBuilder::buildFunctionParameterList_nfi();
              }

              SgTemplateArgumentPtrList *builder_args =
                  new SgTemplateArgumentPtrList(*ensure_template_args());
              SgTemplateInstantiationFunctionDecl *inst_decl =
                  isSgTemplateInstantiationFunctionDecl(
                      SageBuilder::buildNondefiningFunctionDeclaration(
                          template_base_name, ret_type, param_list, func_scope,
                          /*decoratorList=*/NULL,
                          /*buildTemplateInstantiation=*/true, builder_args,
                          SgStorageModifier::e_default,
                          /*forceFreeFunctionScope=*/false));

              if (inst_decl != NULL) {
                inst_decl->set_template_argument_list_is_explicit(
                    has_explicit_template_args);
                SageBuilder::setTemplateArgumentsInDeclaration(
                    inst_decl, ensure_template_args());

                SgFunctionDeclaration *base_decl =
                    isSgFunctionDeclaration(func_sym->get_declaration());
                if (base_decl != NULL) {
                  if (SgTemplateFunctionDeclaration *tmpl_decl =
                          isSgTemplateFunctionDeclaration(base_decl)) {
                    inst_decl->set_templateDeclaration(tmpl_decl);
                    inst_decl->set_templateName(tmpl_decl->get_name());
                  } else if (SgTemplateFunctionDeclaration *first_nondef =
                                 isSgTemplateFunctionDeclaration(
                                     base_decl
                                         ->get_firstNondefiningDeclaration())) {
                    inst_decl->set_templateDeclaration(first_nondef);
                    inst_decl->set_templateName(first_nondef->get_name());
                  }
                }

                if (inst_decl->get_templateDeclaration() == NULL) {
                  if (clang::FunctionDecl *clang_func =
                          llvm::dyn_cast<clang::FunctionDecl>(
                              decl_ref_expr->getDecl())) {
                    clang::FunctionTemplateDecl *primary_template =
                        clang_func->getPrimaryTemplate();
                    if (primary_template == NULL) {
                      primary_template =
                          clang_func->getDescribedFunctionTemplate();
                    }

                    if (primary_template != NULL) {
                      SgNode *tmpl_node = Traverse(primary_template);
                      if (SgTemplateFunctionDeclaration *tmpl_decl =
                              isSgTemplateFunctionDeclaration(tmpl_node)) {
                        inst_decl->set_templateDeclaration(tmpl_decl);
                        inst_decl->set_templateName(tmpl_decl->get_name());
                      }
                    }
                  }
                }

                SgFunctionSymbol *inst_sym = isSgFunctionSymbol(
                    inst_decl->get_symbol_from_symbol_table());
                if (inst_sym == NULL) {
                  if (SgFunctionDeclaration *first_nondef =
                          isSgFunctionDeclaration(
                              inst_decl->get_firstNondefiningDeclaration())) {
                    inst_sym = isSgFunctionSymbol(
                        first_nondef->get_symbol_from_symbol_table());
                  }
                }
                if (inst_sym == NULL && func_scope != NULL) {
                  inst_sym = func_scope->lookup_function_symbol(
                      template_base_name, lookup_type, ensure_template_args());
                  if (inst_sym == NULL && lookup_type != NULL) {
                    inst_sym = func_scope->lookup_function_symbol(
                        template_base_name, lookup_type);
                  }
                  if (inst_sym != NULL) {
                    if (isSgTemplateInstantiationFunctionDecl(
                            inst_sym->get_declaration()) == NULL) {
                      if (SgFunctionDeclaration *first_nondef =
                              isSgFunctionDeclaration(
                                  inst_sym->get_declaration()
                                      ->get_firstNondefiningDeclaration())) {
                        if (isSgTemplateInstantiationFunctionDecl(
                                first_nondef) == NULL) {
                          inst_sym = NULL;
                        }
                      } else {
                        inst_sym = NULL;
                      }
                    }
                  }
                }
                if (inst_sym == NULL && func_scope != NULL) {
                  inst_sym = new SgFunctionSymbol(inst_decl);
                  inst_sym->set_parent(inst_decl);
                  func_scope->insert_symbol(template_base_name, inst_sym);
                }
                if (inst_sym != NULL) {
                  ref_func_sym = inst_sym;
                }
              }
            }
          }
        }
        SgExpression *ref_exp = SageBuilder::buildFunctionRefExp(ref_func_sym);
        attach_explicit_qualifier(ref_exp);
        *node = ref_exp;
      } else {
        if (enum_sym != NULL) {
          // ROOT CAUSE FIX: Get enum declaration from the type instead of
          // parent The Clang frontend may not set parent pointers
          // correctly for enum constants But the type is always set
          // correctly to an SgEnumType
          SgInitializedName *init_name = enum_sym->get_declaration();
          SgEnumDeclaration *enum_decl = NULL;

          if (init_name != NULL && init_name->get_type() != NULL) {
            SgEnumType *enum_type = isSgEnumType(init_name->get_type());
            if (enum_type != NULL) {
              enum_decl = isSgEnumDeclaration(enum_type->get_declaration());
            }
          }

          // Fallback: try getting from parent if type method didn't work
          if (enum_decl == NULL && init_name != NULL) {
            enum_decl = isSgEnumDeclaration(init_name->get_parent());
          }

          ROSE_ASSERT(enum_decl != NULL);
          SgName name = enum_sym->get_name();
          SgExpression *ref_exp =
              SageBuilder::buildEnumVal_nfi(0, enum_decl, name);
          attach_explicit_qualifier(ref_exp);
          *node = ref_exp;
        } else {
          if (sym != NULL) {
            std::cerr << "Runtime error: Unknown type of symbol for a "
                         "declaration reference."
                      << std::endl;
            std::cerr << "    sym->class_name() = " << sym->class_name()
                      << std::endl;
            ROSE_ABORT();
          }
        }
      }
    }
  } else {
    // ROOT CAUSE FIX: Handle template-dependent and unresolved declarations
    clang::Decl *clang_decl = decl_ref_expr->getDecl();
    std::string decl_name = "unresolved_symbol";

    // Get declaration name and type info for better handling
    if (clang_decl && clang::isa<clang::NamedDecl>(clang_decl)) {
      clang::NamedDecl *named_decl = clang::cast<clang::NamedDecl>(clang_decl);
      decl_name = named_decl->getNameAsString();

      // Log what type of declaration couldn't be resolved
      std::cerr << "Warning: Cannot resolve symbol for "
                << clang_decl->getDeclKindName() << " '" << decl_name
                << "', using placeholder" << std::endl;
    } else {
      std::cerr << "Warning: Cannot resolve symbol for declaration reference, "
                   "using placeholder"
                << std::endl;
    }

    SgScopeStatement *current_scope = SageBuilder::topScopeStack();
    if (current_scope == NULL) {
      current_scope = getGlobalScope();
    }
    ROSE_ASSERT(current_scope != NULL);

    SgTemplateArgumentPtrList template_args;
    const SgTemplateArgumentPtrList *template_args_ptr = NULL;
    if (decl_ref_expr->hasExplicitTemplateArgs()) {
      clang::TemplateArgumentListInfo arg_info;
      decl_ref_expr->copyTemplateArgumentsInto(arg_info);
      template_args = buildTemplateArguments(arg_info, true);
      template_args_ptr = &template_args;
    }

    SgExpression *ref_exp = buildNonrealRefExpFromNestedNameSpecifier(
        decl_ref_expr->getQualifier(), current_scope, SgName(decl_name),
        decl_ref_expr->hasTemplateKeyword(), template_args_ptr);
    attach_explicit_qualifier(ref_exp);
    *node = ref_exp;
  }

  return VisitExpr(decl_ref_expr, node) && res;
}

bool ClangToSageTranslator::VisitDependentCoawaitExpr(
    clang::DependentCoawaitExpr *dependent_coawait_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitDependentCoawaitExpr" << std::endl;
#endif
  bool res = true;

  // TODO

  return VisitExpr(dependent_coawait_expr, node) && res;
}

bool ClangToSageTranslator::VisitDependentScopeDeclRefExpr(
    clang::DependentScopeDeclRefExpr *dependent_scope_decl_ref_expr,
    SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitDependentScopeDeclRefExpr"
            << std::endl;
#endif
  bool res = true;

  // DependentScopeDeclRefExpr represents a reference to a declaration that
  // depends on template parameters (e.g., variable references like 'x' or 'y'
  // in template-dependent contexts).

  std::string decl_name =
      dependent_scope_decl_ref_expr->getDeclName().getAsString();

  SgScopeStatement *current_scope = SageBuilder::topScopeStack();
  if (current_scope == NULL) {
    current_scope = getGlobalScope();
  }
  ROSE_ASSERT(current_scope != NULL);

  SgTemplateArgumentPtrList template_args;
  const SgTemplateArgumentPtrList *template_args_ptr = NULL;
  if (dependent_scope_decl_ref_expr->hasExplicitTemplateArgs()) {
    clang::TemplateArgumentListInfo arg_info;
    dependent_scope_decl_ref_expr->copyTemplateArgumentsInto(arg_info);
    template_args = buildTemplateArguments(arg_info, true);
    template_args_ptr = &template_args;
  }

  *node = buildNonrealRefExpFromNestedNameSpecifier(
      dependent_scope_decl_ref_expr->getQualifier(), current_scope,
      SgName(decl_name), dependent_scope_decl_ref_expr->hasTemplateKeyword(),
      template_args_ptr);

  // Set source position
  SgExpression *expr = isSgExpression(*node);
  if (expr != NULL) {
    applySourceRange(expr, dependent_scope_decl_ref_expr->getSourceRange());
  }

  return VisitExpr(dependent_scope_decl_ref_expr, node) && res;
}

// bool
// ClangToSageTranslator::VisitDependentScopeDeclRefExpr(clang::DependentScopeDeclRefExpr
// * dependent_scope_decl_ref_expr);

bool ClangToSageTranslator::VisitDesignatedInitExpr(
    clang::DesignatedInitExpr *designated_init_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitDesignatedInitExpr" << std::endl;
#endif

  SgInitializer *base_init = NULL;
  SgDesignatedInitializer *designated_init = NULL;
  SgExprListExp *expr_list_exp = NULL;
  {
    SgNode *tmp_expr = Traverse(designated_init_expr->getInit());
    SgExpression *expr = isSgExpression(tmp_expr);
    ROSE_ASSERT(expr != NULL);
    SgExprListExp *expr_list_exp = isSgExprListExp(expr);
    if (expr_list_exp != NULL) {
      // FIXME get the type right...
      base_init =
          SageBuilder::buildAggregateInitializer_nfi(expr_list_exp, NULL);
    } else {
      base_init =
          SageBuilder::buildAssignInitializer_nfi(expr, expr->get_type());
    }
    ROSE_ASSERT(base_init != NULL);
    applySourceRange(base_init,
                     designated_init_expr->getInit()->getSourceRange());
  }

  /* Pei-Hung (06/10/2022) revision to handle Initializer in test2013_37.c
   *  After calling getSyntacticForm from InitListExpr, the type and
   * multidimensional array hierarchy is missing. This version can construct the
   * array structure but need additional support to grab the type structure from
   *  parent AST node, such as VarDecl.
   */

  auto designatorSize = designated_init_expr->size();

  for (auto it = designatorSize; it > 0; it--) {
    expr_list_exp = SageBuilder::buildExprListExp_nfi();
    SgExpression *expr = NULL;
    clang::DesignatedInitExpr::Designator *D =
        designated_init_expr->getDesignator(it - 1);
    if (D->isFieldDesignator()) {
      // In LLVM 20, getField() was renamed to getFieldDecl()
      SgSymbol *symbol = GetSymbolFromSymbolTable(D->getFieldDecl());
      SgVariableSymbol *var_sym = isSgVariableSymbol(symbol);
      ROSE_ASSERT(var_sym != NULL);
      expr = SageBuilder::buildVarRefExp_nfi(var_sym);
    } else if (D->isArrayDesignator()) {
      SgNode *tmp_expr = NULL;
      if (clang::ConstantExpr::classof(
              designated_init_expr->getArrayIndex(*D))) {
        clang::FullExpr *fullExpr =
            (clang::FullExpr *)designated_init_expr->getArrayIndex(*D);
        clang::IntegerLiteral *integerLiteral =
            (clang::IntegerLiteral *)fullExpr->getSubExpr();
        tmp_expr = SageBuilder::buildUnsignedLongVal(
            (unsigned long)integerLiteral->getValue().getSExtValue());
      } else {
        tmp_expr = Traverse(designated_init_expr->getArrayIndex(*D));
      }
      expr = isSgExpression(tmp_expr);
      ROSE_ASSERT(expr != NULL);

    } else if (D->isArrayRangeDesignator()) {
      ROSE_ASSERT(!"I don't believe range designator initializer are supported "
                   "by ROSE...");
    } else
      ROSE_ABORT();

    ROSE_ASSERT(expr != NULL);

    applySourceRange(expr, D->getSourceRange());
    expr->set_parent(expr_list_exp);
    expr_list_exp->append_expression(expr);
    if (it > 1) {
      SgDesignatedInitializer *design_init =
          new SgDesignatedInitializer(expr_list_exp, base_init);
      applySourceRange(design_init,
                       designated_init_expr->getDesignatorsSourceRange());
      expr_list_exp->set_parent(design_init);
      base_init->set_parent(design_init);
      SgExprListExp *aggListExp = SageBuilder::buildExprListExp_nfi();
      design_init->set_parent(aggListExp);
      aggListExp->append_expression(design_init);
      SgAggregateInitializer *newAggInit =
          SageBuilder::buildAggregateInitializer_nfi(aggListExp, NULL);
      expr_list_exp = SageBuilder::buildExprListExp_nfi();
      base_init = newAggInit;
    }
  }

  applySourceRange(expr_list_exp,
                   designated_init_expr->getDesignatorsSourceRange());
  designated_init = new SgDesignatedInitializer(expr_list_exp, base_init);
  expr_list_exp->set_parent(base_init);
  base_init->set_parent(designated_init);

  *node = designated_init;

  return VisitExpr(designated_init_expr, node);

  // Pei-Hung (06/10/2022) keep the original implementation which has the array
  // information stored in the list
  /*
      for (auto it=0; it < designatorSize; it++) {
          SgExpression * expr = NULL;
          clang::DesignatedInitExpr::Designator * D =
  designated_init_expr->getDesignator(it); if (D->isFieldDesignator()) {
              SgSymbol * symbol = GetSymbolFromSymbolTable(D->getField());
              SgVariableSymbol * var_sym = isSgVariableSymbol(symbol);
              ROSE_ASSERT(var_sym != NULL);
              expr = SageBuilder::buildVarRefExp_nfi(var_sym);
              applySourceRange(expr, D->getSourceRange());
          }
          else if (D->isArrayDesignator()) {
              SgNode * tmp_expr = NULL;
              if(clang::ConstantExpr::classof(designated_init_expr->getArrayIndex(*D)))
              {
                 clang::FullExpr* fullExpr = (clang::FullExpr*)
  designated_init_expr->getArrayIndex(*D); clang::IntegerLiteral* integerLiteral
  = (clang::IntegerLiteral*) fullExpr->getSubExpr(); tmp_expr =
  SageBuilder::buildUnsignedLongVal((unsigned long)
  integerLiteral->getValue().getSExtValue()); std::cerr << "idx:" <<
  integerLiteral->getValue().getSExtValue() << std::endl;
              }
              else
              {
                 tmp_expr = Traverse(designated_init_expr->getArrayIndex(*D));
              }
              expr = isSgExpression(tmp_expr);
              ROSE_ASSERT(expr != NULL);
          }
          else if (D->isArrayRangeDesignator()) {
              ROSE_ASSERT(!"I don't believe range designator initializer are
  supported by ROSE...");
          }
          else ROSE_ABORT();

          ROSE_ASSERT(expr != NULL);

          expr->set_parent(expr_list_exp);
          expr_list_exp->append_expression(expr);
      }

      applySourceRange(expr_list_exp,
  designated_init_expr->getDesignatorsSourceRange());

      SgDesignatedInitializer * design_init = new
  SgDesignatedInitializer(expr_list_exp, init);
      expr_list_exp->set_parent(design_init);
      init->set_parent(design_init);

      *node = design_init;

      return VisitExpr(designated_init_expr, node);
  */
}

bool ClangToSageTranslator::VisitDesignatedInitUpdateExpr(
    clang::DesignatedInitUpdateExpr *designated_init_update, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitDesignatedInitUpdateExpr"
            << std::endl;
#endif
  bool res = true;

  // TODO

  return VisitExpr(designated_init_update, node) && res;
}

bool ClangToSageTranslator::VisitExpressionTraitExpr(
    clang::ExpressionTraitExpr *expression_trait_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitExpressionTraitExpr" << std::endl;
#endif
  bool res = true;

  // TODO

  return VisitExpr(expression_trait_expr, node) && res;
}

bool ClangToSageTranslator::VisitExtVectorElementExpr(
    clang::ExtVectorElementExpr *ext_vector_element_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitExtVectorElementExpr" << std::endl;
#endif

  SgNode *tmp_base = Traverse(ext_vector_element_expr->getBase());
  SgExpression *base = isSgExpression(tmp_base);
#if DEBUG_VISIT_STMT
  // if (base) std::cerr << "DEBUG: VisitExtVectorElementExpr base node type: "
  // << base->class_name() << std::endl; else std::cerr << "DEBUG:
  // VisitExtVectorElementExpr base node is NULL" << std::endl;
#endif
  ROSE_ASSERT(base != NULL);

  SgType *type = buildTypeFromQualifiedType(ext_vector_element_expr->getType());

  clang::IdentifierInfo &ident_info = ext_vector_element_expr->getAccessor();
  std::string ident = ident_info.getName().str();

  SgScopeStatement *scope = SageBuilder::ScopeStack.front();
  SgGlobal *global = isSgGlobal(scope);
  ROSE_ASSERT(global != NULL);

  // Build Manually a SgVarRefExp to have the same Accessor (text version) TODO
  // ExtVectorAccessor and ExtVectorType
  SgInitializedName *init_name = SageBuilder::buildInitializedName(
      ident, SageBuilder::buildVoidType(), NULL);
  setCompilerGeneratedFileInfo(init_name);
  init_name->set_scope(global);
  SgVariableSymbol *var_symbol = new SgVariableSymbol(init_name);
  SgVarRefExp *pseudo_field = new SgVarRefExp(var_symbol);
  setCompilerGeneratedFileInfo(pseudo_field, true);
  init_name->set_parent(pseudo_field);

  SgExpression *res = NULL;
  if (ext_vector_element_expr->isArrow())
    res = SageBuilder::buildArrowExp(base, pseudo_field);
  else
    res = SageBuilder::buildDotExp(base, pseudo_field);

  ROSE_ASSERT(res != NULL);

  *node = res;

  return VisitExpr(ext_vector_element_expr, node);
}

bool ClangToSageTranslator::VisitFixedPointLiteral(
    clang::FixedPointLiteral *fixed_point_literal, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitFixedPointLiteral" << std::endl;
#endif
  bool res = true;

  // TODO

  return VisitExpr(fixed_point_literal, node) && res;
}

bool ClangToSageTranslator::VisitFloatingLiteral(
    clang::FloatingLiteral *floating_literal, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitFloatingLiteral" << std::endl;
#endif

  unsigned int precision = llvm::APFloat::semanticsPrecision(
      floating_literal->getValue().getSemantics());
  if (precision == 24) {
    // 32-bit float
    *node = SageBuilder::buildFloatVal(
        floating_literal->getValue().convertToFloat());
  } else if (precision == 53) {
    // 64-bit double
    *node = SageBuilder::buildDoubleVal(
        floating_literal->getValue().convertToDouble());
  } else if (precision == 64 || precision == 113) {
    // 80-bit or 128-bit long double - use double as approximation
    *node = SageBuilder::buildLongDoubleVal(
        floating_literal->getValue().convertToDouble());
  } else if (precision == 11) {
    // 16-bit half precision - use float
    *node = SageBuilder::buildFloatVal(
        floating_literal->getValue().convertToFloat());
  } else {
    // Fallback for other sizes - use double
    std::cerr << "Warning: Unsupported float precision " << precision
              << ", using double" << std::endl;
    *node = SageBuilder::buildDoubleVal(
        floating_literal->getValue().convertToDouble());
  }

  std::string spelling = getFloatingLiteralSpelling(
      floating_literal, p_compiler_instance->getSourceManager(),
      p_compiler_instance->getLangOpts());
  if (!spelling.empty()) {
    if (SgFloatVal *float_val = isSgFloatVal(*node)) {
      float_val->set_valueString(spelling);
    } else if (SgDoubleVal *double_val = isSgDoubleVal(*node)) {
      double_val->set_valueString(spelling);
    } else if (SgLongDoubleVal *long_double_val = isSgLongDoubleVal(*node)) {
      long_double_val->set_valueString(spelling);
    }
  }

  return VisitExpr(floating_literal, node);
}

bool ClangToSageTranslator::VisitFullExpr(clang::FullExpr *full_expr,
                                          SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitFullExpr" << std::endl;
#endif
  bool res = true;

  SgNode *tmp_expr = Traverse(full_expr->getSubExpr());
  SgExpression *expr = isSgExpression(tmp_expr);

  // printf ("In VisitFullExpr(): built: expr = %p = %s
  // \n",expr,expr->class_name().c_str());

  *node = expr;

  // TODO

  return VisitExpr(full_expr, node) && res;
}

bool ClangToSageTranslator::VisitConstantExpr(
    clang::ConstantExpr *constant_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitConstantExpr" << std::endl;
#endif
  bool res = true;

  // TODO

  return VisitFullExpr(constant_expr, node) && res;
}

bool ClangToSageTranslator::VisitExprWithCleanups(
    clang::ExprWithCleanups *expr_with_cleanups, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitExprWithCleanups" << std::endl;
#endif
  bool res = true;

  // TODO

  return VisitFullExpr(expr_with_cleanups, node) && res;
}

bool ClangToSageTranslator::VisitFunctionParmPackExpr(
    clang::FunctionParmPackExpr *function_parm_pack_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitFunctionParmPackExpr" << std::endl;
#endif
  bool res = true;

  // TODO

  return VisitExpr(function_parm_pack_expr, node) && res;
}

bool ClangToSageTranslator::VisitGenericSelectionExpr(
    clang::GenericSelectionExpr *generic_Selection_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitGenericSelectionExpr" << std::endl;
#endif
  bool res = true;

  // TODO

  return VisitExpr(generic_Selection_expr, node) && res;
}

bool ClangToSageTranslator::VisitGNUNullExpr(clang::GNUNullExpr *gnu_null_expr,
                                             SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitGNUNullExpr" << std::endl;
#endif
  bool res = true;

  // ROOT CAUSE FIX: GNUNullExpr is GNU's __null extension, which represents a
  // null pointer constant It has type long (or long long on 64-bit) but behaves
  // as a null pointer Create an integer literal with value 0, VisitExpr will
  // handle the type
  *node = SageBuilder::buildIntVal(0);

  return VisitExpr(gnu_null_expr, node) && res;
}

bool ClangToSageTranslator::VisitImaginaryLiteral(
    clang::ImaginaryLiteral *imaginary_literal, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitImaginaryLiteral" << std::endl;
#endif

  SgNode *tmp_imag_val = Traverse(imaginary_literal->getSubExpr());
  SgValueExp *imag_val = isSgValueExp(tmp_imag_val);
  ROSE_ASSERT(imag_val != NULL);

  SgComplexVal *comp_val =
      new SgComplexVal(NULL, imag_val, imag_val->get_type(), "");

  *node = comp_val;

  return VisitExpr(imaginary_literal, node);
}

bool ClangToSageTranslator::VisitImplicitValueInitExpr(
    clang::ImplicitValueInitExpr *implicit_value_init_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitImplicitValueInitExpr" << std::endl;
#endif
  bool res = true;

  // TODO

  return VisitExpr(implicit_value_init_expr, node) && res;
}

bool ClangToSageTranslator::VisitInitListExpr(
    clang::InitListExpr *init_list_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitInitListExpr" << std::endl;
#endif

  // We use the syntactic version of the initializer if it exists
  if (init_list_expr->getSyntacticForm() != NULL)
    return VisitInitListExpr(init_list_expr->getSyntacticForm(), node);

  SgExprListExp *expr_list_expr = SageBuilder::buildExprListExp_nfi();

  clang::InitListExpr::iterator it;
  for (it = init_list_expr->begin(); it != init_list_expr->end(); it++) {
    SgNode *tmp_expr = Traverse(*it);
    SgExpression *expr = isSgExpression(tmp_expr);
    ROSE_ASSERT(expr != NULL);

    // Pei-Hung (05/13/2022) the expr can another InitListExpr
    SgExprListExp *child_expr_list_expr = isSgExprListExp(expr);
    SgInitializer *init = NULL;
    if (child_expr_list_expr != NULL) {
      SgType *type = expr->get_type();
      init = SageBuilder::buildAggregateInitializer(child_expr_list_expr, type);
    }

    if (init != NULL) {
      applySourceRange(init, (*it)->getSourceRange());
      expr_list_expr->append_expression(init);
    } else
      expr_list_expr->append_expression(expr);
  }

  *node = expr_list_expr;

  return VisitExpr(init_list_expr, node);
}

bool ClangToSageTranslator::VisitIntegerLiteral(
    clang::IntegerLiteral *integer_literal, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitIntegerLiteral" << std::endl;
#endif
  SgValueExp *value_exp = nullptr;
  const clang::QualType literal_type = integer_literal->getType();
  const clang::BuiltinType *builtin_type =
      literal_type->getAs<clang::BuiltinType>();
  const llvm::APInt value = integer_literal->getValue();
  std::string spelling = getIntegerLiteralSpelling(
      integer_literal, p_compiler_instance->getSourceManager(),
      p_compiler_instance->getLangOpts());
  bool has_spelling = !spelling.empty();

  if (builtin_type != nullptr) {
    switch (builtin_type->getKind()) {
    case clang::BuiltinType::UChar:
    case clang::BuiltinType::Char_U:
      if (has_spelling) {
        value_exp = SageBuilder::buildUnsignedCharVal_nfi(
            static_cast<unsigned char>(value.getLimitedValue()), spelling);
      } else {
        value_exp = SageBuilder::buildUnsignedCharVal(
            static_cast<unsigned char>(value.getLimitedValue()));
      }
      break;
    case clang::BuiltinType::UShort:
      if (has_spelling) {
        value_exp = SageBuilder::buildUnsignedShortVal_nfi(
            static_cast<unsigned short>(value.getLimitedValue()), spelling);
      } else {
        value_exp = SageBuilder::buildUnsignedShortVal(
            static_cast<unsigned short>(value.getLimitedValue()));
      }
      break;
    case clang::BuiltinType::UInt:
      if (has_spelling) {
        value_exp = SageBuilder::buildUnsignedIntVal_nfi(
            static_cast<unsigned int>(value.getLimitedValue()), spelling);
      } else {
        value_exp = SageBuilder::buildUnsignedIntVal(
            static_cast<unsigned int>(value.getLimitedValue()));
      }
      break;
    case clang::BuiltinType::ULong:
      if (has_spelling) {
        value_exp = SageBuilder::buildUnsignedLongVal_nfi(
            static_cast<unsigned long>(value.getLimitedValue()), spelling);
      } else {
        value_exp = SageBuilder::buildUnsignedLongVal(
            static_cast<unsigned long>(value.getLimitedValue()));
      }
      break;
    case clang::BuiltinType::ULongLong:
      if (has_spelling) {
        value_exp = SageBuilder::buildUnsignedLongLongIntVal_nfi(
            static_cast<unsigned long long>(value.getLimitedValue()), spelling);
      } else {
        value_exp = SageBuilder::buildUnsignedLongLongIntVal(
            static_cast<unsigned long long>(value.getLimitedValue()));
      }
      break;
    case clang::BuiltinType::SChar:
    case clang::BuiltinType::Char_S:
      if (has_spelling) {
        value_exp = SageBuilder::buildCharVal_nfi(
            static_cast<char>(value.getSExtValue()), spelling);
      } else {
        value_exp =
            SageBuilder::buildCharVal(static_cast<char>(value.getSExtValue()));
      }
      break;
    case clang::BuiltinType::Short:
      if (has_spelling) {
        value_exp = SageBuilder::buildShortVal_nfi(
            static_cast<short>(value.getSExtValue()), spelling);
      } else {
        value_exp = SageBuilder::buildShortVal(
            static_cast<short>(value.getSExtValue()));
      }
      break;
    case clang::BuiltinType::Int:
      if (has_spelling) {
        value_exp = SageBuilder::buildIntVal_nfi(
            static_cast<int>(value.getSExtValue()), spelling);
      } else {
        value_exp =
            SageBuilder::buildIntVal(static_cast<int>(value.getSExtValue()));
      }
      break;
    case clang::BuiltinType::Long:
      if (has_spelling) {
        value_exp = SageBuilder::buildLongIntVal_nfi(
            static_cast<long>(value.getSExtValue()), spelling);
      } else {
        value_exp = SageBuilder::buildLongIntVal(
            static_cast<long>(value.getSExtValue()));
      }
      break;
    case clang::BuiltinType::LongLong:
      if (has_spelling) {
        value_exp = SageBuilder::buildLongLongIntVal_nfi(
            static_cast<long long>(value.getSExtValue()), spelling);
      } else {
        value_exp = SageBuilder::buildLongLongIntVal(
            static_cast<long long>(value.getSExtValue()));
      }
      break;
    default:
      break;
    }
  }

  if (value_exp == nullptr) {
    if (literal_type->isUnsignedIntegerType()) {
      if (has_spelling) {
        value_exp = SageBuilder::buildUnsignedLongLongIntVal_nfi(
            static_cast<unsigned long long>(value.getLimitedValue()), spelling);
      } else {
        value_exp = SageBuilder::buildUnsignedLongLongIntVal(
            static_cast<unsigned long long>(value.getLimitedValue()));
      }
    } else {
      if (has_spelling) {
        value_exp = SageBuilder::buildLongLongIntVal_nfi(
            static_cast<long long>(value.getSExtValue()), spelling);
      } else {
        value_exp = SageBuilder::buildLongLongIntVal(
            static_cast<long long>(value.getSExtValue()));
      }
    }
  }

  *node = value_exp;

  return VisitExpr(integer_literal, node);
}

bool ClangToSageTranslator::VisitLambdaExpr(clang::LambdaExpr *lambda_expr,
                                            SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitLambdaExpr" << std::endl;
#endif
  bool res = true;

  auto detach_declaration_from_scope =
      [](SgDeclarationStatement *decl) -> SgScopeStatement * {
    if (decl == NULL) {
      return NULL;
    }

    SgScopeStatement *original_scope = decl->get_scope();

    // Remove any symbol that was registered for this declaration in the
    // enclosing scope.
    if (SgSymbol *associated_symbol =
            decl->search_for_symbol_from_symbol_table()) {
      if (SgScopeStatement *symbol_scope = associated_symbol->get_scope()) {
        symbol_scope->remove_symbol(associated_symbol);
      }
    }

    auto is_attached_to_parent_container = [](SgStatement *stmt) -> bool {
      if (stmt == NULL || stmt->get_parent() == NULL) {
        return false;
      }

      SgNode *parent = stmt->get_parent();
      if (SgBasicBlock *bb = isSgBasicBlock(parent)) {
        const SgStatementPtrList &stmts = bb->get_statements();
        return std::find(stmts.begin(), stmts.end(), stmt) != stmts.end();
      }

      if (SgGlobal *global = isSgGlobal(parent)) {
        const SgDeclarationStatementPtrList &decls = global->get_declarations();
        return std::find(decls.begin(), decls.end(), stmt) != decls.end();
      }

      if (SgNamespaceDefinitionStatement *ns =
              isSgNamespaceDefinitionStatement(parent)) {
        const SgDeclarationStatementPtrList &decls = ns->get_declarations();
        return std::find(decls.begin(), decls.end(), stmt) != decls.end();
      }

      if (SgClassDefinition *class_def = isSgClassDefinition(parent)) {
        const SgDeclarationStatementPtrList &members = class_def->get_members();
        return std::find(members.begin(), members.end(), stmt) != members.end();
      }

      return false;
    };

    if (original_scope != NULL) {
      if (is_attached_to_parent_container(decl)) {
        SageInterface::removeStatement(decl, false);
      }
      decl->set_parent(NULL);
    }

    return original_scope;
  };

  // Get the lambda class (closure type) from Clang
  const clang::CXXRecordDecl *clang_lambda_class =
      lambda_expr->getLambdaClass();

  // Get the call operator (operator()) from Clang
  const clang::CXXMethodDecl *clang_call_operator =
      lambda_expr->getCallOperator();

  // Convert Clang lambda class to ROSE class declaration
  SgClassDeclaration *lambda_closure_class = NULL;
  SgScopeStatement *lambda_closure_lexical_scope = NULL;
  if (clang_lambda_class != NULL) {
    SgNode *tmp_class =
        Traverse(const_cast<clang::CXXRecordDecl *>(clang_lambda_class));
    lambda_closure_class = isSgClassDeclaration(tmp_class);

    // Remove from enclosing scope using SageInterface so symbols/scopes stay
    // consistent
    lambda_closure_lexical_scope =
        detach_declaration_from_scope(lambda_closure_class);

    // Preserve a valid scope so downstream lookups don't see NULL after
    // detachment.
    if (lambda_closure_class != NULL && lambda_closure_lexical_scope != NULL) {
      lambda_closure_class->set_scope(lambda_closure_lexical_scope);
      if (SgClassDefinition *class_def =
              lambda_closure_class->get_definition()) {
        class_def->set_scope(lambda_closure_lexical_scope);
      }
    }
  }

  // Convert Clang call operator to ROSE function declaration
  SgFunctionDeclaration *lambda_function = NULL;
  SgScopeStatement *lambda_function_scope = NULL;
  if (clang_call_operator != NULL) {
    SgNode *tmp_func =
        Traverse(const_cast<clang::CXXMethodDecl *>(clang_call_operator));
    lambda_function = isSgFunctionDeclaration(tmp_func);

    lambda_function_scope = detach_declaration_from_scope(lambda_function);

    // Restore the scope pointer for operator() so later queries succeed.
    if (lambda_function != NULL) {
      if (lambda_function_scope != NULL) {
        lambda_function->set_scope(lambda_function_scope);
      } else if (lambda_closure_class != NULL &&
                 lambda_closure_class->get_definition() != NULL) {
        lambda_function->set_scope(lambda_closure_class->get_definition());
      }
    }
  }

  SgLambdaCaptureList *lambda_capture_list =
      SageBuilder::buildLambdaCaptureList();
  unsigned total_captures = lambda_expr->capture_size();
  unsigned capture_index = 0;
  for (auto capture_it = lambda_expr->capture_begin();
       capture_it != lambda_expr->capture_end();
       ++capture_it, ++capture_index) {
    const clang::LambdaCapture &capture = *capture_it;
    SgExpression *capture_expression = NULL;
    bool handled_capture = false;

    if (capture.capturesThis()) {
      SgSymbol *this_symbol =
          findEnclosingThisSymbol(SageBuilder::topScopeStack());
      ROSE_ASSERT(this_symbol != NULL);
      capture_expression = SageBuilder::buildThisExp_nfi(this_symbol);
      handled_capture = true;
    } else if (capture.capturesVariable()) {
      clang::ValueDecl *captured_val = capture.getCapturedVar();
      clang::VarDecl *captured_var =
          llvm::dyn_cast_or_null<clang::VarDecl>(captured_val);
      if (captured_var != NULL) {
        // Look up the existing symbol instead of traversing again (avoids
        // duplicate decls)
        SgSymbol *captured_symbol = GetSymbolFromSymbolTable(captured_var);
        if (captured_symbol == NULL) {
          clang::VarDecl *canonical_decl = captured_var->getCanonicalDecl();
          if (canonical_decl != captured_var) {
            captured_symbol = GetSymbolFromSymbolTable(canonical_decl);
          }
        }

        if (SgVariableSymbol *var_symbol =
                isSgVariableSymbol(captured_symbol)) {
          capture_expression = SageBuilder::buildVarRefExp(var_symbol);
          handled_capture = true;
        } else if (captured_var->isInitCapture()) {
          // Create a dangling var ref for init-captures (symbol not in current
          // scope yet)
          SgName capture_name(captured_var->getNameAsString());
          capture_expression = SageBuilder::buildDanglingVarRefExp(
              capture_name, SageBuilder::topScopeStack());
          handled_capture = true;
        }
      }
    } else if (capture.capturesVLAType()) {
      // VLA captures are represented on the closure type; nothing to emit in
      // capture list.
      continue;
    }

    if (!handled_capture || capture_expression == NULL) {
      continue;
    }

    bool is_init_capture = lambda_expr->isInitCapture(&capture);
    clang::LambdaCaptureKind capture_kind = capture.getCaptureKind();
    bool capture_by_reference =
        (capture_kind == clang::LCK_ByRef || capture_kind == clang::LCK_This);

    SgLambdaCapture *sg_capture =
        SageBuilder::buildLambdaCapture(capture_expression, NULL, NULL);
    sg_capture->set_capture_by_reference(capture_by_reference);
    sg_capture->set_implicit(capture.isImplicit());
    sg_capture->set_pack_expansion(capture.isPackExpansion());
    if (is_init_capture && capture_index < total_captures) {
      clang::Expr *clang_init_expr =
          lambda_expr->capture_init_begin()[capture_index];
      if (clang_init_expr != NULL) {
        SgNode *init_node = Traverse(clang_init_expr);
        if (SgExpression *init_expr = isSgExpression(init_node)) {
          sg_capture->set_source_closure_variable(init_expr);
          init_expr->set_parent(sg_capture);
        }
      }
    }

    lambda_capture_list->get_capture_list().push_back(sg_capture);
    sg_capture->set_parent(lambda_capture_list);
  }

  SgLambdaExp *built_lambda = SageBuilder::buildLambdaExp(
      lambda_capture_list, lambda_closure_class, lambda_function);
  ROSE_ASSERT(built_lambda != NULL);

  clang::LambdaCaptureDefault capture_default =
      lambda_expr->getCaptureDefault();
  bool has_default_capture = (capture_default != clang::LCD_None);
  built_lambda->set_capture_default(has_default_capture);
  built_lambda->set_default_is_by_reference(capture_default ==
                                            clang::LCD_ByRef);

  *node = built_lambda;

  return VisitExpr(lambda_expr, node) && res;
}

bool ClangToSageTranslator::VisitMaterializeTemporaryExpr(
    clang::MaterializeTemporaryExpr *materialize_temporary_expr,
    SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitMaterializeTemporaryExpr"
            << std::endl;
#endif
  bool res = true;

  // MaterializeTemporaryExpr creates a temporary object from a prvalue
  // For now, just traverse the temporary expression itself
  // The temporary materialization is implicit in C++ and doesn't need explicit
  // AST representation in ROSE
  *node = Traverse(materialize_temporary_expr->getSubExpr());

  return VisitExpr(materialize_temporary_expr, node) && res;
}

bool ClangToSageTranslator::VisitMemberExpr(clang::MemberExpr *member_expr,
                                            SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitMemberExpr" << std::endl;
#endif

  bool res = true;

  bool implicit_access = member_expr->isImplicitAccess();
  SgExpression *base = NULL;
  if (!implicit_access) {
    SgNode *tmp_base = Traverse(member_expr->getBase());
    base = isSgExpression(tmp_base);
    if (base == NULL) {
      // std::cerr << "DEBUG: VisitMemberExpr base is NULL! Base stmt class: "
      // << member_expr->getBase()->getStmtClassName() << std::endl;
    } else {
      // std::cerr << "DEBUG: VisitMemberExpr base type: " << base->class_name()
      // << std::endl;
    }
    ROSE_ASSERT(base != NULL);
  }

  SgSymbol *sym = GetSymbolFromSymbolTable(member_expr->getMemberDecl());

  SgVariableSymbol *var_sym = isSgVariableSymbol(sym);
  SgMemberFunctionSymbol *func_sym = isSgMemberFunctionSymbol(sym);
  SgFunctionSymbol *plain_func_sym =
      isSgFunctionSymbol(sym); // Regular function symbol (not member)
  SgClassSymbol *class_sym = isSgClassSymbol(sym);

  SgExpression *sg_member_expr = NULL;

  bool successful_cast = var_sym || func_sym || plain_func_sym || class_sym;
  if (sym != NULL && !successful_cast) {
    std::cerr << "Runtime error: Unknown type of symbol for a member reference."
              << std::endl;
    std::cerr << "    sym->class_name() = " << sym->class_name() << std::endl;
    res = false;
  } else if (var_sym != NULL) {
    sg_member_expr = SageBuilder::buildVarRefExp(var_sym);
  } else if (func_sym != NULL) { // C++ member function
    sg_member_expr = SageBuilder::buildMemberFunctionRefExp_nfi(
        func_sym, false, false);       // FIXME 2nd and 3rd params ?
  } else if (plain_func_sym != NULL) { // Regular function treated as member
                                       // (e.g., static member or inherited)
    sg_member_expr = SageBuilder::buildFunctionRefExp(plain_func_sym);
  } else if (class_sym != NULL) {
    SgClassDeclaration *classDecl = class_sym->get_declaration();
    SgClassDeclaration *classDefDecl =
        isSgClassDeclaration(classDecl->get_definition());
    SgType *classType = classDecl->get_type();
    //        if(classDecl->get_isUnNamed())
    {
      SgName varName(generate_name_for_variable(member_expr));
      std::cerr << "build varName:" << varName << std::endl;
      SgVariableDeclaration *var_decl = SageBuilder::buildVariableDeclaration(
          varName, classType, NULL, SageBuilder::topScopeStack());
      var_decl->set_baseTypeDefiningDeclaration(classDefDecl);
      var_decl->set_variableDeclarationContainsBaseTypeDefiningDeclaration(
          true);
      var_decl->set_parent(SageBuilder::topScopeStack());

      sg_member_expr = SageBuilder::buildVarRefExp(var_decl);
    }
  } else if (sym == NULL) {
    // Symbol not found - check if member declaration has already been traversed
    // to avoid infinite recursion during template member access
    SgNode *tmp_member = NULL;
    clang::ValueDecl *member_decl = member_expr->getMemberDecl();

    // First check if already in translation map
    if (llvm::isa<clang::Decl>(member_decl)) {
      std::map<clang::Decl *, SgNode *>::iterator it_decl =
          p_decl_translation_map.find((clang::Decl *)member_decl);
      if (it_decl != p_decl_translation_map.end()) {
        tmp_member = it_decl->second;
      }
    }

    // If not in map, traverse it (but this might fail for template members)
    if (tmp_member == NULL) {
      tmp_member = Traverse(member_decl);
    }

#if DEBUG_VISIT_STMT
    if (tmp_member != NULL) {
      // std::cerr << "DEBUG VisitMemberExpr: Got/traversed member, node type: "
      // << tmp_member->class_name() << std::endl;
    } else {
      // std::cerr << "DEBUG VisitMemberExpr: Member not available (NULL)" <<
      // std::endl;
    }
#endif
    if (tmp_member != NULL) {
      // CLANG FRONTEND FIX: Extract symbol from the traversed member
      // declaration.
      //
      // WHY: After successfully traversing the member's declaration above, we
      // need to get its symbol. However, calling GetSymbolFromSymbolTable again
      // here would create infinite recursion when template members reference
      // each other.
      //
      // SOLUTION: Set sym=NULL to skip the GetSymbolFromSymbolTable call below
      // (around line 4290), and instead extract the symbol directly from the
      // already-constructed SAGE node (tmp_member).
      //
      // This breaks the cycle: GetSymbolFromSymbolTable → VisitMemberExpr →
      // Traverse(member) → GetSymbolFromSymbolTable (AVOIDED by sym=NULL)
      //
      sym = NULL; // Skip GetSymbolFromSymbolTable below; extract from
                  // tmp_member instead

      // Extract symbol directly from the traversed node
      if (isSgVariableDeclaration(tmp_member)) {
        SgInitializedName *init_name = SageInterface::getFirstInitializedName(
            isSgVariableDeclaration(tmp_member));
        if (init_name) {
          sym = init_name->search_for_symbol_from_symbol_table();
        }
      } else if (isSgFunctionDeclaration(tmp_member)) {
        SgFunctionDeclaration *func_decl = isSgFunctionDeclaration(tmp_member);
        SgScopeStatement *decl_scope = func_decl->get_scope();
        if (decl_scope) {
          // Use type-aware lookup to handle overloaded functions correctly
          SgFunctionType *func_type = func_decl->get_type();
          sym = decl_scope->lookup_function_symbol(func_decl->get_name(),
                                                   func_type);
        }
      }
      if (isSgVariableSymbol(sym)) {
        sg_member_expr = SageBuilder::buildVarRefExp(isSgVariableSymbol(sym));
      } else if (isSgMemberFunctionSymbol(sym)) {
        sg_member_expr = SageBuilder::buildMemberFunctionRefExp_nfi(
            isSgMemberFunctionSymbol(sym), false, false);
      } else if (isSgFunctionSymbol(sym)) {
        // ROOT CAUSE FIX: Handle plain SgFunctionSymbol (not member function
        // symbol) This happens when VisitFunctionDecl creates a regular
        // function declaration
        sg_member_expr =
            SageBuilder::buildFunctionRefExp(isSgFunctionSymbol(sym));
      } else if (isSgInitializedName(tmp_member)) {
        // Create a temporary symbol if we got an initialized name
        SgVariableSymbol *temp_sym =
            new SgVariableSymbol(isSgInitializedName(tmp_member));
        sg_member_expr = SageBuilder::buildVarRefExp(temp_sym);
      }
      // ROOT CAUSE FIX: Handle SgMemberFunctionDeclaration from
      // VisitCXXMethodDecl
      else if (sym == NULL &&
               isSgMemberFunctionDeclaration(tmp_member) != NULL) {
        SgMemberFunctionDeclaration *member_func_decl =
            isSgMemberFunctionDeclaration(tmp_member);
        // Try to find existing symbol in the class scope
        SgScopeStatement *decl_scope = member_func_decl->get_scope();
        if (decl_scope != NULL) {
          // Use type-aware lookup to handle overloaded member functions
          // correctly
          SgFunctionType *func_type = member_func_decl->get_type();
          sym = decl_scope->lookup_function_symbol(member_func_decl->get_name(),
                                                   func_type);
        }
        // If still not found, create new member function symbol
        if (sym == NULL) {
          SgMemberFunctionSymbol *new_func_sym =
              new SgMemberFunctionSymbol(member_func_decl);
          new_func_sym->set_parent(member_func_decl);
          if (decl_scope != NULL) {
            decl_scope->insert_symbol(member_func_decl->get_name(),
                                      new_func_sym);
          }
          sym = new_func_sym;
        }
        if (isSgMemberFunctionSymbol(sym)) {
          sg_member_expr = SageBuilder::buildMemberFunctionRefExp_nfi(
              isSgMemberFunctionSymbol(sym), false, false);
        }
      }
      // Also handle regular function declarations that might be static members
      else if (sym == NULL && isSgFunctionDeclaration(tmp_member) != NULL) {
        SgFunctionDeclaration *func_decl = isSgFunctionDeclaration(tmp_member);
        // Try to find existing symbol
        SgScopeStatement *decl_scope = func_decl->get_scope();
        if (decl_scope != NULL) {
          // Use type-aware lookup to handle overloaded functions correctly
          SgFunctionType *func_type = func_decl->get_type();
          sym = decl_scope->lookup_function_symbol(func_decl->get_name(),
                                                   func_type);
        }
        // If not found, create new function symbol
        if (sym == NULL) {
          SgFunctionSymbol *new_func_sym = new SgFunctionSymbol(func_decl);
          new_func_sym->set_parent(func_decl);
          if (decl_scope != NULL) {
            decl_scope->insert_symbol(func_decl->get_name(), new_func_sym);
          }
          sym = new_func_sym;
        }
        if (isSgMemberFunctionSymbol(sym)) {
          sg_member_expr = SageBuilder::buildMemberFunctionRefExp_nfi(
              isSgMemberFunctionSymbol(sym), false, false);
        } else if (isSgFunctionSymbol(sym)) {
          sg_member_expr =
              SageBuilder::buildFunctionRefExp(isSgFunctionSymbol(sym));
        }
      }
    }

    // If still NULL, create a placeholder
    if (sg_member_expr == NULL) {
      std::string member_name = member_expr->getMemberNameInfo().getAsString();
      clang::ValueDecl *member_decl = member_expr->getMemberDecl();
      if (member_decl) {
        std::cerr << "Warning: Cannot resolve "
                  << member_decl->getDeclKindName() << " member '"
                  << member_name << "'";
        if (tmp_member != NULL) {
          std::cerr << " (traversed to " << tmp_member->class_name() << ")";
        } else {
          std::cerr << " (traverse returned NULL)";
        }
        std::cerr << ", using placeholder" << std::endl;
      } else {
        std::cerr << "Warning: Cannot resolve member '" << member_name
                  << "', using placeholder" << std::endl;
      }
      sg_member_expr = SageBuilder::buildDanglingVarRefExp(
          SgName(member_name), SageBuilder::topScopeStack());
    }
  }

  ROSE_ASSERT(sg_member_expr != NULL);

  // TODO (C++) member_expr->getQualifier() : for 'a->Base::foo'

  if (implicit_access) {
    *node = sg_member_expr;
  } else if (member_expr->isArrow()) {
    *node = SageBuilder::buildArrowExp(base, sg_member_expr);
  } else {
    *node = SageBuilder::buildDotExp(base, sg_member_expr);
  }

  return VisitExpr(member_expr, node) && res;
}

bool ClangToSageTranslator::VisitMSPropertyRefExpr(
    clang::MSPropertyRefExpr *ms_property_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitMSPropertyRefExpr" << std::endl;
#endif
  bool res = true;

  // TODO

  return VisitExpr(ms_property_expr, node) && res;
}

bool ClangToSageTranslator::VisitMSPropertySubscriptExpr(
    clang::MSPropertySubscriptExpr *ms_property_subscript_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitMSPropertySubscriptExpr"
            << std::endl;
#endif
  bool res = true;

  // TODO

  return VisitExpr(ms_property_subscript_expr, node) && res;
}

bool ClangToSageTranslator::VisitNoInitExpr(clang::NoInitExpr *no_init_expr,
                                            SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitNoInitExpr" << std::endl;
#endif
  bool res = true;

  // TODO

  return VisitExpr(no_init_expr, node) && res;
}

bool ClangToSageTranslator::VisitOffsetOfExpr(
    clang::OffsetOfExpr *offset_of_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitOffsetOfExpr" << std::endl;
#endif
  bool res = true;

  SgNodePtrList nodePtrList;

  SgType *type = buildTypeFromQualifiedType(
      offset_of_expr->getTypeSourceInfo()->getType());

  nodePtrList.push_back(type);

  SgExpression *topExp = nullptr;

  for (unsigned i = 0, n = offset_of_expr->getNumComponents(); i < n; ++i) {
    clang::OffsetOfNode ON = offset_of_expr->getComponent(i);

    switch (ON.getKind()) {
    case clang::OffsetOfNode::Array: {
      // Array node
      SgExpression *arrayIdx = isSgExpression(
          Traverse(offset_of_expr->getIndexExpr(ON.getArrayExprIndex())));
      SgPntrArrRefExp *pntrArrRefExp =
          SageBuilder::buildPntrArrRefExp(topExp, arrayIdx);
      topExp = isSgExpression(pntrArrRefExp);
      break;
    }
    case clang::OffsetOfNode::Field: {
      // OffsetOfNode still uses getField(), not getFieldDecl()
      SgNode *fieldNode = Traverse(ON.getField());
      SgName fieldName(ON.getFieldName()->getName().str());
      SgVarRefExp *varExp = SageBuilder::buildDanglingVarRefExp(
          fieldName, SageBuilder::topScopeStack());
      if (topExp == nullptr) {
        topExp = isSgExpression(varExp);
      } else {
        SgDotExp *dotExp = SageBuilder::buildDotExp(topExp, varExp);
        topExp = isSgExpression(dotExp);
      }
      break;
    }
    // TODO
    case clang::OffsetOfNode::Identifier: {
      SgName fieldName(ON.getFieldName()->getName().str());
      SgVarRefExp *varExp = SageBuilder::buildDanglingVarRefExp(
          fieldName, SageBuilder::topScopeStack());
      break;
    }
    // TODO
    case clang::OffsetOfNode::Base:
      break;
    }
  }
  nodePtrList.push_back(topExp);

  SgTypeTraitBuiltinOperator *typeTraitBuiltinOperator =
      SageBuilder::buildTypeTraitBuiltinOperator("__builtin_offsetof",
                                                 nodePtrList);

  *node = typeTraitBuiltinOperator;

  return VisitExpr(offset_of_expr, node) && res;
}

bool ClangToSageTranslator::VisitOMPArraySectionExpr(
    clang::ArraySectionExpr *omp_array_section_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitOMPArraySectionExpr" << std::endl;
#endif
  bool res = true;

  // TODO

  return VisitExpr(omp_array_section_expr, node) && res;
}

bool ClangToSageTranslator::VisitOpaqueValueExpr(
    clang::OpaqueValueExpr *opaque_value_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitOpaqueValueExpr" << std::endl;
#endif
  bool res = true;

  // ROOT CAUSE FIX: OpaqueValueExpr is a Clang internal node representing a
  // value that appears multiple times in the AST but should only be evaluated
  // once. It's used for desugaring constructs like the conditional operator and
  // range-based for. We just traverse the source expression and return it.

  clang::Expr *source_expr = opaque_value_expr->getSourceExpr();
  if (source_expr) {
    *node = Traverse(source_expr);
  } else {
    // No source expression - OpaqueValueExpr is used as a placeholder
    // Create a null expression placeholder for ROSE
    // This happens in range-based for loops and other desugared constructs
    *node = SageBuilder::buildNullExpression();
  }

  return VisitExpr(opaque_value_expr, node) && res;
}

bool ClangToSageTranslator::VisitOverloadExpr(
    clang::OverloadExpr *overload_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitOverloadExpr" << std::endl;
#endif
  bool res = true;

  // TODO

  return VisitExpr(overload_expr, node) && res;
}

bool ClangToSageTranslator::VisitUnresolvedLookupExpr(
    clang::UnresolvedLookupExpr *unresolved_lookup_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitUnresolvedLookupExpr" << std::endl;
#endif
  bool res = true;

  // UnresolvedLookupExpr represents a reference to a name that couldn't be
  // resolved during parsing (e.g., template-dependent function names like
  // `foo(t)` that will be resolved during instantiation/ADL).

  std::string function_name = unresolved_lookup_expr->getName().getAsString();

  SgScopeStatement *current_scope = SageBuilder::topScopeStack();
  if (current_scope == NULL) {
    current_scope = getGlobalScope();
  }
  ROSE_ASSERT(current_scope != NULL);

  SgTemplateArgumentPtrList template_args;
  const SgTemplateArgumentPtrList *template_args_ptr = NULL;
  if (unresolved_lookup_expr->hasExplicitTemplateArgs()) {
    clang::TemplateArgumentListInfo arg_info;
    unresolved_lookup_expr->copyTemplateArgumentsInto(arg_info);
    template_args = buildTemplateArguments(arg_info, true);
    template_args_ptr = &template_args;
  }

  *node = buildNonrealRefExpFromNestedNameSpecifier(
      unresolved_lookup_expr->getQualifier(), current_scope,
      SgName(function_name), unresolved_lookup_expr->hasTemplateKeyword(),
      template_args_ptr);

  // Set source position
  SgExpression *expr = isSgExpression(*node);
  if (expr != NULL) {
    applySourceRange(expr, unresolved_lookup_expr->getSourceRange());
  }

  return VisitOverloadExpr(unresolved_lookup_expr, node) && res;
}

bool ClangToSageTranslator::VisitUnresolvedMemberExpr(
    clang::UnresolvedMemberExpr *unresolved_member_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitUnresolvedMemberExpr" << std::endl;
#endif
  bool res = true;

  // ROOT CAUSE FIX: UnresolvedMemberExpr represents a member access expression
  // where the member couldn't be resolved at parse time (typically in
  // template-dependent code) Example: template<typename T> void foo(T t) {
  // t.bar(); }  // 'bar' is unresolved

  // Get the member name
  std::string member_name =
      unresolved_member_expr->getMemberName().getAsString();

  SgScopeStatement *current_scope = SageBuilder::topScopeStack();
  if (current_scope == NULL) {
    current_scope = getGlobalScope();
  }
  ROSE_ASSERT(current_scope != NULL);

  // Handle the base expression (the object/pointer being accessed)
  SgExpression *base_expr = NULL;
  if (!unresolved_member_expr->isImplicitAccess()) {
    SgNode *tmp_base = Traverse(unresolved_member_expr->getBase());
    base_expr = isSgExpression(tmp_base);
    ROSE_ASSERT(base_expr != NULL);
  }

  SgTemplateArgumentPtrList template_args;
  const SgTemplateArgumentPtrList *template_args_ptr = NULL;
  if (unresolved_member_expr->hasExplicitTemplateArgs()) {
    clang::TemplateArgumentListInfo arg_info;
    unresolved_member_expr->copyTemplateArgumentsInto(arg_info);
    template_args = buildTemplateArguments(arg_info, true);
    template_args_ptr = &template_args;
  }

  SgNonrealRefExp *member_ref = buildNonrealRefExpFromNestedNameSpecifier(
      unresolved_member_expr->getQualifier(), current_scope,
      SgName(member_name), unresolved_member_expr->hasTemplateKeyword(),
      template_args_ptr);
  ROSE_ASSERT(member_ref != NULL);

  if (base_expr != NULL) {
    if (unresolved_member_expr->isArrow()) {
      *node = SageBuilder::buildArrowExp(base_expr, member_ref);
    } else {
      *node = SageBuilder::buildDotExp(base_expr, member_ref);
    }
  } else {
    *node = member_ref;
  }

  if (SgExpression *expr = isSgExpression(*node)) {
    applySourceRange(expr, unresolved_member_expr->getSourceRange());
  }

  return VisitOverloadExpr(unresolved_member_expr, node) && res;
}

bool ClangToSageTranslator::VisitPackExpansionExpr(
    clang::PackExpansionExpr *pack_expansion_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitPackExpansionExpr" << std::endl;
#endif
  bool res = true;

  // ROOT CAUSE FIX: Pack expansion expressions (e.g., f(args...) where args is
  // a pack) Traverse the pattern expression (the expression before the ...)
  clang::Expr *pattern = pack_expansion_expr->getPattern();
  if (pattern != NULL) {
    SgNode *tmp_node = Traverse(pattern);
    SgExpression *pattern_expr = isSgExpression(tmp_node);
    if (pattern_expr != NULL) {
      *node = pattern_expr;
      return VisitExpr(pack_expansion_expr, node) && res;
    }
  }

  // Fallback if pattern can't be traversed
  *node = SageBuilder::buildNullExpression();

  return VisitExpr(pack_expansion_expr, node) && res;
}

bool ClangToSageTranslator::VisitParenExpr(clang::ParenExpr *paren_expr,
                                           SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitParenExpr" << std::endl;
#endif

  bool res = true;

  SgNode *tmp_subexpr = Traverse(paren_expr->getSubExpr());
  SgExpression *subexpr = isSgExpression(tmp_subexpr);
  if (tmp_subexpr != NULL && subexpr == NULL) {
    std::cerr << "Runtime error: tmp_subexpr != NULL && subexpr == NULL"
              << std::endl;
    res = false;
  }

  // bypass ParenExpr, their is nothing equivalent in SageIII
  *node = subexpr;

  return VisitExpr(paren_expr, node) && res;
}

bool ClangToSageTranslator::VisitParenListExpr(
    clang::ParenListExpr *paran_list_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitParenListExpr" << std::endl;
#endif
  bool res = true;

  // ROOT CAUSE FIX: ParenListExpr represents a parenthesized list of
  // expressions like in aggregate initialization: Point p(1, 2); Build an
  // expression list to represent the paren list

  unsigned num_exprs = paran_list_expr->getNumExprs();

  if (num_exprs == 1) {
    // Single expression - just unwrap the parentheses and return the expression
    // directly
    *node = Traverse(paran_list_expr->getExpr(0));
  } else {
    // Multiple expressions - build an ExprListExp
    SgExprListExp *expr_list = SageBuilder::buildExprListExp();
    for (unsigned i = 0; i < num_exprs; i++) {
      SgNode *tmp_expr = Traverse(paran_list_expr->getExpr(i));
      SgExpression *expr = isSgExpression(tmp_expr);
      if (expr != NULL) {
        expr_list->append_expression(expr);
      } else if (tmp_expr != NULL) {
        std::cerr << "Warning: ParenListExpr element " << i
                  << " is not an expression" << std::endl;
        res = false;
      }
    }
    *node = expr_list;
  }

  return VisitExpr(paran_list_expr, node) && res;
}

bool ClangToSageTranslator::VisitPredefinedExpr(
    clang::PredefinedExpr *predefined_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitPredefinedExpr" << std::endl;
#endif

  // FIXME It's get tricky here: PredefinedExpr represent compiler generateed
  // variables
  //    I choose to attach those variables on demand in the function definition
  //    scope

  // Traverse the scope's stack to find the last function definition:

  SgFunctionDefinition *func_def = NULL;
  std::list<SgScopeStatement *>::reverse_iterator it =
      SageBuilder::ScopeStack.rbegin();
  while (it != SageBuilder::ScopeStack.rend() && func_def == NULL) {
    func_def = isSgFunctionDefinition(*it);
    it++;
  }
  ROSE_ASSERT(func_def != NULL);

  // Determine the name of the variable

  SgName name;

  // (01/29/2020) Pei-Hung: change to getIndentKind.  And this list is
  // incomplete for Clang 9 In LLVM 20, enum is PredefinedIdentKind with values
  // Func, Function, etc.
  switch (predefined_expr->getIdentKind()) {
  case clang::PredefinedIdentKind::Func:
  case clang::PredefinedIdentKind::FuncDName:
  case clang::PredefinedIdentKind::FuncSig:
  case clang::PredefinedIdentKind::LFuncSig:
    name = "__func__";
    break;
  case clang::PredefinedIdentKind::Function:
  case clang::PredefinedIdentKind::LFunction:
    name = "__FUNCTION__";
    break;
  case clang::PredefinedIdentKind::PrettyFunction:
  case clang::PredefinedIdentKind::PrettyFunctionNoVirtual:
    name = "__PRETTY_FUNCTION__";
    break;
  default:
    name = "__func__";
    break;
  }

  // Retrieve the associate symbol if it exists

  SgVariableSymbol *symbol = func_def->lookup_variable_symbol(name);

  // Else, build a compiler generated initialized name for this variable in the
  // function defintion scope.

  if (symbol == NULL) {
    SgInitializedName *init_name = SageBuilder::buildInitializedName_nfi(
        name, SageBuilder::buildPointerType(SageBuilder::buildCharType()),
        NULL);

    init_name->set_parent(func_def);
    init_name->set_scope(func_def);

    Sg_File_Info *start_fi =
        Sg_File_Info::generateDefaultFileInfoForCompilerGeneratedNode();
    start_fi->setCompilerGenerated();
    init_name->set_startOfConstruct(start_fi);

    Sg_File_Info *end_fi =
        Sg_File_Info::generateDefaultFileInfoForCompilerGeneratedNode();
    end_fi->setCompilerGenerated();
    init_name->set_endOfConstruct(end_fi);

    symbol = new SgVariableSymbol(init_name);

    func_def->insert_symbol(name, symbol);
  }
  ROSE_ASSERT(symbol != NULL);

  // Finally build the variable reference

  *node = SageBuilder::buildVarRefExp_nfi(symbol);

  return VisitExpr(predefined_expr, node);
}

bool ClangToSageTranslator::VisitPseudoObjectExpr(
    clang::PseudoObjectExpr *pseudo_object_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitPseudoObjectExpr" << std::endl;
#endif
  bool res = true;

  // TODO

  return VisitExpr(pseudo_object_expr, node) && res;
}

bool ClangToSageTranslator::VisitShuffleVectorExpr(
    clang::ShuffleVectorExpr *shuffle_vector_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitShuffleVectorExpr" << std::endl;
#endif
  bool res = true;

  // TODO

  return VisitExpr(shuffle_vector_expr, node) && res;
}

bool ClangToSageTranslator::VisitSizeOfPackExpr(
    clang::SizeOfPackExpr *size_of_pack_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitSizeOfPackExpr" << std::endl;
#endif
  bool res = true;

  // ROOT CAUSE FIX: sizeof...(Args) returns the compile-time count of pack
  // elements However, for template-dependent packs, the size isn't known until
  // instantiation

  if (!size_of_pack_expr->isValueDependent()) {
    // Non-dependent: get the pack length and create an integer literal
    unsigned pack_length = size_of_pack_expr->getPackLength();
    *node = SageBuilder::buildUnsignedIntVal(pack_length);
  } else {
    // Value-dependent: create an opaque expression placeholder
    // The actual size will be determined at template instantiation time
    *node = SageBuilder::buildOpaqueVarRefExp("__sizeof_pack_dependent",
                                              getGlobalScope());
  }

  return VisitExpr(size_of_pack_expr, node) && res;
}

bool ClangToSageTranslator::VisitSourceLocExpr(
    clang::SourceLocExpr *source_loc_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitSourceLocExpr" << std::endl;
#endif
  bool res = true;

  // CLANG FRONTEND FIX: SourceLocExpr represents __builtin_FILE(),
  // __builtin_LINE(),
  // __builtin_FUNCTION(), __builtin_COLUMN() - compiler builtins that return
  // source location information at runtime.
  //
  // We must preserve correct types:
  // - __builtin_FILE(), __builtin_FUNCTION(), __builtin_FileName(),
  // __builtin_FuncSig()
  //   return const char* → SgStringVal
  // - __builtin_LINE() and __builtin_COLUMN() return unsigned int → SgIntVal
  //
  // Current implementation returns placeholder values (empty string "" or 0)
  // since extracting actual source location info requires SourceManager
  // integration.

  clang::SourceLocIdentKind kind = source_loc_expr->getIdentKind();

  switch (kind) {
  case clang::SourceLocIdentKind::File:
  case clang::SourceLocIdentKind::FileName:
  case clang::SourceLocIdentKind::Function:
  case clang::SourceLocIdentKind::FuncSig:
    // String-typed builtins: return empty string to preserve type correctness
    // This allows code like `const char *f = __builtin_FILE();` to unparse
    // correctly
    *node = SageBuilder::buildStringVal("");
    break;

  case clang::SourceLocIdentKind::Line:
  case clang::SourceLocIdentKind::Column:
    // Integer-typed builtins: return 0 as placeholder
    *node = SageBuilder::buildIntVal(0);
    break;

  default:
    // Unknown builtin kind (e.g., SourceLocStruct): default to integer 0
    *node = SageBuilder::buildIntVal(0);
    break;
  }

  return VisitExpr(source_loc_expr, node) && res;
}

bool ClangToSageTranslator::VisitStmtExpr(clang::StmtExpr *stmt_expr,
                                          SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitStmtExpr" << std::endl;
#endif

  bool res = true;

  SgNode *tmp_substmt = Traverse(stmt_expr->getSubStmt());
  SgStatement *substmt = isSgStatement(tmp_substmt);
  if (tmp_substmt != NULL && substmt == NULL) {
    std::cerr << "Runtime error: tmp_substmt != NULL && substmt == NULL"
              << std::endl;
    res = false;
  }

  *node = new SgStatementExpression(substmt);

  return VisitExpr(stmt_expr, node) && res;
}

bool ClangToSageTranslator::VisitStringLiteral(
    clang::StringLiteral *string_literal, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitStringLiteral" << std::endl;
#endif

  // ROOT CAUSE FIX: Check character byte width to handle wide/unicode string
  // literals getString() only works for regular char strings (width=1) For wide
  // strings (L"...", u"...", U"..."), we need to use getBytes() instead
  std::string tmp;
  unsigned char_byte_width = string_literal->getCharByteWidth();

  if (char_byte_width == 1) {
    // Regular char string or UTF-8 string
    tmp = string_literal->getString().str();
  } else {
    // Wide string literal (wchar_t, char16_t, char32_t)
    // Use getBytes() which returns raw bytes regardless of encoding
    llvm::StringRef bytes = string_literal->getBytes();
    tmp = bytes.str();
  }

  const char *raw_str = tmp.data();

  // Get byte length from Clang instead of searching for '\0'
  // For wide strings, getBytes() includes embedded NULs between characters
  // Example: L"AB" becomes {'A',0,'B',0,0,0}, so we can't use '\0' as
  // terminator
  unsigned byte_length = string_literal->getByteLength();

  std::string escaped;
  escaped.reserve(byte_length * 4 + 1);
  auto appendHexEscape = [&](unsigned char byte) {
    static const char hex_digits[] = "0123456789ABCDEF";
    escaped += "\\x";
    escaped += hex_digits[(byte >> 4) & 0xF];
    escaped += hex_digits[byte & 0xF];
  };

  for (unsigned i = 0; i < byte_length; ++i) {
    unsigned char ch = static_cast<unsigned char>(raw_str[i]);
    if (char_byte_width == 1) {
      switch (ch) {
      case '\\':
        escaped += "\\\\";
        break;
      case '\n':
        escaped += "\\n";
        break;
      case '\r':
        escaped += "\\r";
        break;
      case '"':
        escaped += "\\\"";
        break;
      case '\0':
        escaped += "\\0";
        break;
      default:
        escaped.push_back(static_cast<char>(ch));
        break;
      }
    } else {
      appendHexEscape(ch);
    }
  }

  *node = SageBuilder::buildStringVal(escaped);

  return VisitExpr(string_literal, node);
}

bool ClangToSageTranslator::VisitSubstNonTypeTemplateParmExpr(
    clang::SubstNonTypeTemplateParmExpr *subst_non_type_template_parm_expr,
    SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitSubstNonTypeTemplateParmExpr"
            << std::endl;
#endif
  bool res = true;

  // SubstNonTypeTemplateParmExpr represents a non-type template parameter that
  // has been substituted with its actual value (e.g., N in array<T,N> being
  // replaced with 1024) Traverse to the replacement expression
  *node = Traverse(subst_non_type_template_parm_expr->getReplacement());

  return VisitExpr(subst_non_type_template_parm_expr, node) && res;
}

bool ClangToSageTranslator::VisitSubstNonTypeTemplateParmPackExpr(
    clang::SubstNonTypeTemplateParmPackExpr
        *subst_non_type_template_parm_pack_expr,
    SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitSubstNonTypeTemplateParmPackExpr"
            << std::endl;
#endif
  bool res = true;

  // TODO

  return VisitExpr(subst_non_type_template_parm_pack_expr, node) && res;
}

bool ClangToSageTranslator::VisitTypeTraitExpr(clang::TypeTraitExpr *type_trait,
                                               SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitTypeTraitExpr" << std::endl;
#endif
  bool res = true;

  // ROOT CAUSE FIX: Type traits (std::is_integral, std::is_same, etc.) evaluate
  // at compile-time However, template-dependent type traits cannot be evaluated
  // until instantiation

  if (!type_trait->isValueDependent()) {
    // Non-dependent: get the compile-time result and create a bool literal
    bool trait_value = type_trait->getValue();
    *node = SageBuilder::buildBoolValExp(trait_value);
  } else {
    // Value-dependent (template parameter dependent): create an opaque type
    // expression The actual value will be determined at template instantiation
    // time
    *node = SageBuilder::buildOpaqueVarRefExp("__type_trait_dependent",
                                              getGlobalScope());
  }

  return VisitExpr(type_trait, node) && res;
}

// TypoExpr was removed in LLVM 20
/*
bool ClangToSageTranslator::VisitTypoExpr(clang::TypoExpr * typo_expr, SgNode **
node) { #if DEBUG_VISIT_STMT std::cerr << "ClangToSageTranslator::VisitTypoExpr"
<< std::endl; #endif bool res = true;

    // TODO

    return VisitExpr(typo_expr, node) && res;
}
*/

bool ClangToSageTranslator::VisitUnaryExprOrTypeTraitExpr(
    clang::UnaryExprOrTypeTraitExpr *unary_expr_or_type_trait_expr,
    SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitUnaryExprOrTypeTraitExpr"
            << std::endl;
#endif

  bool res = true;

  SgExpression *expr = NULL;
  SgType *type = NULL;

  if (unary_expr_or_type_trait_expr->isArgumentType()) {
    type = buildTypeFromQualifiedType(
        unary_expr_or_type_trait_expr->getArgumentType());
  } else {
    SgNode *tmp_expr =
        Traverse(unary_expr_or_type_trait_expr->getArgumentExpr());
    expr = isSgExpression(tmp_expr);

    if (tmp_expr != NULL && expr == NULL) {
      std::cerr << "Runtime error: tmp_expr != NULL && expr == NULL"
                << std::endl;
      res = false;
    }
  }

  switch (unary_expr_or_type_trait_expr->getKind()) {
  case clang::UETT_SizeOf:
    if (type != NULL) {
      std::map<SgClassType *, bool>::iterator bool_it =
          p_class_type_decl_first_see_in_type.find(isSgClassType(type));
      SgSizeOfOp *sizeofOp = SageBuilder::buildSizeOfOp_nfi(type);

      // Pei-Hung (08/16/22): try to follow VisitTypedefDecl to check if the
      // classType is first seen

      clang::QualType argumentQualType =
          unary_expr_or_type_trait_expr->getArgumentType();
      const clang::Type *argumentType = argumentQualType.getTypePtr();
      bool isembedded = false;
      bool iscompleteDefined = false;

      while ((isa<clang::ElaboratedType>(argumentType)) ||
             (isa<clang::PointerType>(argumentType)) ||
             (isa<clang::ArrayType>(argumentType))) {
        if (isa<clang::ElaboratedType>(argumentType)) {
          argumentQualType =
              ((clang::ElaboratedType *)argumentType)->getNamedType();
        } else if (isa<clang::PointerType>(argumentType)) {
          argumentQualType =
              ((clang::PointerType *)argumentType)->getPointeeType();
        } else if (isa<clang::ArrayType>(argumentType)) {
          argumentQualType =
              ((clang::ArrayType *)argumentType)->getElementType();
        }
        argumentType = argumentQualType.getTypePtr();
      }

      if (isa<clang::RecordType>(argumentType)) {
        clang::RecordType *argumentRecordType =
            (clang::RecordType *)argumentType;
        clang::RecordDecl *recordDeclaration = argumentRecordType->getDecl();
        isembedded = recordDeclaration->isEmbeddedInDeclarator();
        iscompleteDefined = recordDeclaration->isCompleteDefinition();
      }

      if (isSgClassType(type) && iscompleteDefined) {
        std::map<SgClassType *, bool>::iterator bool_it =
            p_class_type_decl_first_see_in_type.find(isSgClassType(type));
        ROSE_ASSERT(bool_it != p_class_type_decl_first_see_in_type.end());
        if (bool_it->second) {
          // Pei-Hung (08/16/22) If it is first seen, the definition should be
          // unparsed in sizeofOp
          sizeofOp->set_sizeOfContainsBaseTypeDefiningDeclaration(true);
          bool_it->second = false;
        }
      }

      *node = sizeofOp;
    } else if (expr != NULL)
      *node = SageBuilder::buildSizeOfOp_nfi(expr);
    else
      res = false;
    break;
  case clang::UETT_AlignOf:
  case clang::UETT_PreferredAlignOf:
    if (type != NULL) {
      *node = SageBuilder::buildSizeOfOp_nfi(type);
      ROSE_ASSERT(FAIL_FIXME ==
                  0); // difference between AlignOf and PreferredAlignOf is not
                      // represented in ROSE
    } else if (expr != NULL)
      *node = SageBuilder::buildSizeOfOp_nfi(expr);
    else
      res = false;
    break;
  case clang::UETT_VecStep:
    ROSE_ASSERT(!"OpenCL - VecStep is not supported!");
  default:
    ROSE_ASSERT(!"Unknown clang::UETT_xx");
  }

  return VisitStmt(unary_expr_or_type_trait_expr, node) && res;
}

bool ClangToSageTranslator::VisitUnaryOperator(
    clang::UnaryOperator *unary_operator, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitUnaryOperator" << std::endl;
#endif

  bool res = true;

  SgNode *tmp_subexpr = Traverse(unary_operator->getSubExpr());
  SgExpression *subexpr = isSgExpression(tmp_subexpr);
  if (tmp_subexpr != NULL && subexpr == NULL) {
    std::cerr << "Runtime error: tmp_subexpr != NULL && subexpr == NULL"
              << std::endl;
    res = false;
  }

  switch (unary_operator->getOpcode()) {
  case clang::UO_PostInc:
    *node = SageBuilder::buildPlusPlusOp(subexpr, SgUnaryOp::postfix);
    break;
  case clang::UO_PostDec:
    *node = SageBuilder::buildMinusMinusOp(subexpr, SgUnaryOp::postfix);
    break;
  case clang::UO_PreInc:
    *node = SageBuilder::buildPlusPlusOp(subexpr, SgUnaryOp::prefix);
    break;
  case clang::UO_PreDec:
    *node = SageBuilder::buildMinusMinusOp(subexpr, SgUnaryOp::prefix);
    break;
  case clang::UO_AddrOf:
    *node = SageBuilder::buildAddressOfOp(subexpr);
    break;
  case clang::UO_Deref:
    *node = SageBuilder::buildPointerDerefExp(subexpr);
    break;
  case clang::UO_Plus:
    *node = SageBuilder::buildUnaryAddOp(subexpr);
    break;
  case clang::UO_Minus:
    *node = SageBuilder::buildMinusOp(subexpr);
    break;
  // Def. in Clang: UNARY_OPERATION(Not, "~")
  case clang::UO_Not:
    *node = SageBuilder::buildBitComplementOp(subexpr);
    break;
  // Def. in UNARY_OPERATION(LNot, "!")
  case clang::UO_LNot:
    *node = SageBuilder::buildNotOp(subexpr);
    break;
  case clang::UO_Real:
    *node = SageBuilder::buildImagPartOp(subexpr);
    break;
  case clang::UO_Imag:
    *node = SageBuilder::buildRealPartOp(subexpr);
    break;
  case clang::UO_Extension:
    *node = subexpr;
    break;
  default:
    std::cerr << "Runtime error: Unknown unary operator." << std::endl;
    res = false;
  }

  return VisitExpr(unary_operator, node) && res;
}

bool ClangToSageTranslator::VisitVAArgExpr(clang::VAArgExpr *va_arg_expr,
                                           SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitVAArgExpr" << std::endl;
#endif

  SgNode *tmp_expr = Traverse(va_arg_expr->getSubExpr());
  SgExpression *expr = isSgExpression(tmp_expr);
  ROSE_ASSERT(expr != NULL);

  SgType *type =
      buildTypeFromQualifiedType(va_arg_expr->getWrittenTypeInfo()->getType());
  ROSE_ASSERT(type != NULL);

  *node = SageBuilder::buildVarArgOp_nfi(expr, type);

  return VisitExpr(va_arg_expr, node);
}
bool ClangToSageTranslator::VisitLabelStmt(clang::LabelStmt *label_stmt,
                                           SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitLabelStmt" << std::endl;
#endif

  bool res = true;

  SgName name(label_stmt->getName());

  *node = SageBuilder::buildLabelStatement_nfi(name, NULL,
                                               SageBuilder::topScopeStack());
  SgLabelStatement *sg_label_stmt = isSgLabelStatement(*node);

  SgFunctionDefinition *label_scope = NULL;
  std::list<SgScopeStatement *>::reverse_iterator it =
      SageBuilder::ScopeStack.rbegin();
  while (it != SageBuilder::ScopeStack.rend() && label_scope == NULL) {
    label_scope = isSgFunctionDefinition(*it);
    it++;
  }
  if (label_scope == NULL) {
    std::cerr << "Runtime error: Cannot find a surrounding function definition "
                 "for the label statement: \""
              << name << "\"." << std::endl;
    res = false;
  } else {
    sg_label_stmt->set_scope(label_scope);
    SgLabelSymbol *label_sym = new SgLabelSymbol(sg_label_stmt);
    label_scope->insert_symbol(label_sym->get_name(), label_sym);
  }

  SgNode *tmp_sub_stmt = Traverse(label_stmt->getSubStmt());
  SgStatement *sg_sub_stmt = isSgStatement(tmp_sub_stmt);
  if (sg_sub_stmt == NULL) {
    SgExpression *sg_sub_expr = isSgExpression(tmp_sub_stmt);
    ROSE_ASSERT(sg_sub_expr != NULL);
    sg_sub_stmt = SageBuilder::buildExprStatement(sg_sub_expr);
    applySourceRangeWithTrailingSemicolon(sg_sub_stmt,
                                          label_stmt->getSubStmt());
  }

  ROSE_ASSERT(sg_sub_stmt != NULL);
  sg_sub_stmt =
      wrapStatementWithOpenMPPragmas(label_stmt->getSubStmt(), sg_sub_stmt);

  sg_sub_stmt->set_parent(sg_label_stmt);
  sg_label_stmt->set_statement(sg_sub_stmt);

  return VisitStmt(label_stmt, node) && res;
}

bool ClangToSageTranslator::VisitWhileStmt(clang::WhileStmt *while_stmt,
                                           SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitWhileStmt" << std::endl;
#endif

  SgNode *tmp_cond = Traverse(while_stmt->getCond());
  SgExpression *cond = isSgExpression(tmp_cond);
  ROSE_ASSERT(cond != NULL);

  SgStatement *expr_stmt = SageBuilder::buildExprStatement(cond);
  if (p_compiler_instance != nullptr && while_stmt->getCond() != nullptr) {
    applySourceRange(expr_stmt, while_stmt->getCond()->getSourceRange());
  }

  SgWhileStmt *sg_while_stmt = SageBuilder::buildWhileStmt_nfi(expr_stmt, NULL);

  cond->set_parent(expr_stmt);
  expr_stmt->set_parent(sg_while_stmt);

  SageBuilder::pushScopeStack(sg_while_stmt);

  SgNode *tmp_body = Traverse(while_stmt->getBody());
  SgStatement *body = isSgStatement(tmp_body);
  SgExpression *expr = isSgExpression(tmp_body);
  if (expr != NULL) {
    body = SageBuilder::buildExprStatement(expr);
    applySourceRangeWithTrailingSemicolon(body, while_stmt->getBody());
  }
  ROSE_ASSERT(body != NULL);
  body = wrapStatementWithOpenMPPragmas(while_stmt->getBody(), body);

  body->set_parent(sg_while_stmt);

  SageBuilder::popScopeStack();

  sg_while_stmt->set_body(body);

  *node = sg_while_stmt;

  return VisitStmt(while_stmt, node);
}
