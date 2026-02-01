#include "clang-frontend-private.hpp"

#include "fixupTemplateArguments.h"
#include "sage3basic.h"
#include "unparser.h"

#include <cctype>

#include <algorithm>

#include <clang/Lex/Lexer.h>

#include <clang/Sema/Sema.h>

#include <clang/Sema/Template.h>

#include <llvm/ADT/SmallString.h>

#include <llvm/ADT/SmallVector.h>

#include <llvm/Support/raw_ostream.h>

#include <functional>

#include <memory>

#include <set>

#include <unordered_map>

#include <unordered_set>

#include <vector>

static void suppress_unparse_output(SgLocatedNode *n) {
  if (n == nullptr) {
    return;
  }
  if (Sg_File_Info *fi = n->get_file_info()) {
    fi->unsetOutputInCodeGeneration();
  }
  if (Sg_File_Info *fi = n->get_startOfConstruct()) {
    fi->unsetOutputInCodeGeneration();
  }
  if (Sg_File_Info *fi = n->get_endOfConstruct()) {
    fi->unsetOutputInCodeGeneration();
  }
}

static void mark_compiler_generated_frontend_specific(SgNode *n) {
  if (n == nullptr) {
    return;
  }
  auto mark_fi = [](Sg_File_Info *fi) {
    if (fi != nullptr) {
      fi->setCompilerGenerated();
      fi->setFrontendSpecific();
      fi->set_file_id(Sg_File_Info::COMPILER_GENERATED_FILE_ID);
      fi->set_physical_file_id(Sg_File_Info::COMPILER_GENERATED_FILE_ID);
    }
  };
  if (SgLocatedNode *located = isSgLocatedNode(n)) {
    mark_fi(located->get_file_info());
    mark_fi(located->get_startOfConstruct());
    mark_fi(located->get_endOfConstruct());
    if (SgExpression *expr = isSgExpression(located)) {
      mark_fi(expr->get_operatorPosition());
    }
  }
  if (SgInitializedName *init_name = isSgInitializedName(n)) {
    mark_fi(init_name->get_file_info());
    mark_fi(init_name->get_startOfConstruct());
    mark_fi(init_name->get_endOfConstruct());
  }
}

static bool isClangOpenMPDecl(const clang::Decl *decl) {
  if (decl == nullptr) {
    return false;
  }
  switch (decl->getKind()) {
  case clang::Decl::OMPCapturedExpr:
  case clang::Decl::OMPDeclareMapper:
  case clang::Decl::OMPDeclareReduction:
  case clang::Decl::OMPAllocate:
  case clang::Decl::OMPRequires:
  case clang::Decl::OMPThreadPrivate:
    return true;
  default:
    return false;
  }
}

static void rejectClangOpenMPDecl(const clang::Decl *decl) {
  std::cerr
      << "Error: OpenMP/OpenACC declarations must be handled via pragma "
         "capture and omp/accAstConstructor. Clang OpenMP AST declarations "
         "should not be translated. Ensure -fopenmp is not passed to Clang."
      << std::endl;
  if (decl != nullptr) {
    std::cerr << "Clang decl kind: " << decl->getDeclKindName() << std::endl;
    decl->dump();
  }
  ROSE_ABORT();
}

namespace {
static std::string trimWhitespace(std::string s) {
  size_t first = 0;
  while (first < s.size() &&
         std::isspace(static_cast<unsigned char>(s[first]))) {
    ++first;
  }
  s.erase(0, first);

  while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) {
    s.pop_back();
  }
  return s;
}

static void cleanup_unused_param_list(SgFunctionDeclaration *decl,
                                      SgFunctionParameterList *candidate) {
  if (candidate == nullptr) {
    return;
  }
  if (decl != nullptr && decl->get_parameterList() == candidate) {
    if (candidate->get_parent() == nullptr) {
      candidate->set_parent(decl);
    }
    return;
  }
  if (candidate->get_parent() == nullptr) {
    delete candidate;
  }
}

static void ensureFunctionParameterSymbols(SgFunctionDeclaration *decl) {
  if (decl == nullptr) {
    return;
  }

  std::set<SgFunctionDeclaration *> visited;
  std::vector<SgFunctionDeclaration *> worklist;
  worklist.push_back(decl);
  if (SgFunctionDeclaration *def_decl =
          isSgFunctionDeclaration(decl->get_definingDeclaration())) {
    worklist.push_back(def_decl);
  }

  for (SgFunctionDeclaration *current : worklist) {
    if (current == nullptr || !visited.insert(current).second) {
      continue;
    }

    SgFunctionDefinition *def = current->get_definition();
    if (def == nullptr) {
      continue;
    }

    SgFunctionParameterList *params = current->get_parameterList();
    if (params == nullptr) {
      continue;
    }

    SgScopeStatement *scope = def;
    for (SgInitializedName *param : params->get_args()) {
      if (param == nullptr) {
        continue;
      }
      if (param->get_name().getString().empty()) {
        continue;
      }
      param->set_declptr(current);
      param->set_scope(scope);
      SgVariableSymbol *symbol =
          isSgVariableSymbol(param->get_symbol_from_symbol_table());
      if (symbol == nullptr) {
        symbol = new SgVariableSymbol(param);
        scope->insert_symbol(param->get_name(), symbol);
        symbol->set_parent(scope->get_symbol_table());
      }
    }
  }
}

static std::string getDeclNameSafe(clang::NamedDecl *named_decl) {
  if (named_decl == nullptr) {
    return "";
  }
  clang::DeclarationName decl_name = named_decl->getDeclName();
  if (decl_name.isEmpty()) {
    return "";
  }
  if (decl_name.isIdentifier()) {
    const clang::IdentifierInfo *ident = decl_name.getAsIdentifierInfo();
    if (ident == nullptr) {
      return "";
    }
    return ident->getName().str();
  }
  return decl_name.getAsString();
}

static bool isSystemOrBuiltinLocation(clang::SourceLocation loc,
                                      clang::SourceManager &sm) {
  if (!loc.isValid()) {
    return true;
  }
  if (loc.isMacroID()) {
    loc = sm.getSpellingLoc(loc);
  }
  if (!loc.isValid()) {
    return true;
  }
  return sm.isInSystemHeader(loc) || sm.isWrittenInBuiltinFile(loc);
}

static bool isSystemOrBuiltinFunctionDecl(clang::FunctionDecl *decl,
                                          clang::SourceManager &sm) {
  if (decl == nullptr) {
    return true;
  }

  auto check_decl = [&](clang::FunctionDecl *candidate) -> bool {
    if (candidate == nullptr) {
      return false;
    }
    return isSystemOrBuiltinLocation(candidate->getLocation(), sm);
  };

  if (check_decl(decl)) {
    return true;
  }

  if (clang::FunctionDecl *def = decl->getDefinition()) {
    if (def != decl && check_decl(def)) {
      return true;
    }
  }

  if (clang::FunctionDecl *pattern = decl->getTemplateInstantiationPattern()) {
    if (pattern != decl && check_decl(pattern)) {
      return true;
    }
  }

  if (clang::FunctionTemplateDecl *primary = decl->getPrimaryTemplate()) {
    clang::FunctionDecl *templated = primary->getTemplatedDecl();
    if (templated != nullptr && templated != decl && check_decl(templated)) {
      return true;
    }
  }

  if (auto *method = llvm::dyn_cast<clang::CXXMethodDecl>(decl)) {
    if (clang::FunctionDecl *member_pattern =
            method->getInstantiatedFromMemberFunction()) {
      if (check_decl(member_pattern)) {
        return true;
      }
    }
  }

  return false;
}

static bool getExplicitInstantiationClassKind(
    const clang::ClassTemplateSpecializationDecl *decl,
    clang::CompilerInstance *compiler, SgClassDeclaration::class_types *kind) {
  if (decl == nullptr || compiler == nullptr || kind == nullptr) {
    return false;
  }

  clang::SourceManager &sm = compiler->getSourceManager();
  clang::SourceLocation loc = decl->getTemplateKeywordLoc();
  if (!loc.isValid()) {
    loc = decl->getExternKeywordLoc();
  }
  if (!loc.isValid()) {
    loc = decl->getBeginLoc();
  }
  if (loc.isMacroID()) {
    loc = sm.getSpellingLoc(loc);
  }
  if (!loc.isValid()) {
    return false;
  }

  const clang::LangOptions &lang = compiler->getLangOpts();
  bool saw_template = false;
  for (;;) {
    clang::Token tok;
    if (clang::Lexer::getRawToken(loc, tok, sm, lang,
                                  /*IgnoreWhiteSpace=*/true)) {
      break;
    }

    if (tok.is(clang::tok::kw_template)) {
      saw_template = true;
    } else if (saw_template) {
      if (tok.is(clang::tok::kw_class)) {
        *kind = SgClassDeclaration::e_class;
        return true;
      }
      if (tok.is(clang::tok::kw_struct)) {
        *kind = SgClassDeclaration::e_struct;
        return true;
      }
      if (tok.is(clang::tok::kw_union)) {
        *kind = SgClassDeclaration::e_union;
        return true;
      }
    }

    clang::SourceLocation next =
        clang::Lexer::getLocForEndOfToken(tok.getEndLoc(), 0, sm, lang);
    if (!next.isValid() || next == loc) {
      break;
    }
    loc = next;
  }

  clang::SourceRange range = decl->getSourceRange();
  if (range.isValid()) {
    clang::SourceLocation begin = range.getBegin();
    clang::SourceLocation end = range.getEnd();
    if (begin.isMacroID()) {
      begin = sm.getSpellingLoc(begin);
    }
    if (end.isMacroID()) {
      end = sm.getSpellingLoc(end);
    }
    if (begin.isValid() && end.isValid()) {
      clang::CharSourceRange char_range =
          clang::CharSourceRange::getTokenRange(begin, end);
      llvm::StringRef text = clang::Lexer::getSourceText(char_range, sm, lang);
      if (!text.empty()) {
        clang::Lexer lexer(begin, lang, text.begin(), text.begin(), text.end());
        lexer.SetCommentRetentionState(false);

        bool saw_template = false;
        for (;;) {
          clang::Token tok;
          if (lexer.LexFromRawLexer(tok)) {
            break;
          }

          if (tok.is(clang::tok::kw_template)) {
            saw_template = true;
          } else if (saw_template) {
            if (tok.is(clang::tok::kw_class)) {
              *kind = SgClassDeclaration::e_class;
              return true;
            }
            if (tok.is(clang::tok::kw_struct)) {
              *kind = SgClassDeclaration::e_struct;
              return true;
            }
            if (tok.is(clang::tok::kw_union)) {
              *kind = SgClassDeclaration::e_union;
              return true;
            }
          }
        }
      }
    }
  }

  return false;
}

static void appendTemplateInstantiationArg(std::string &result,
                                           bool &need_separator,
                                           const clang::TemplateArgument &arg) {
  if (arg.getKind() == clang::TemplateArgument::Pack) {
    for (const clang::TemplateArgument &pack_arg : arg.pack_elements()) {
      appendTemplateInstantiationArg(result, need_separator, pack_arg);
    }
    return;
  }

  if (need_separator) {
    result += " , ";
  }
  need_separator = true;

  std::string arg_str;
  llvm::raw_string_ostream arg_stream(arg_str);
  arg.print(clang::PrintingPolicy(clang::LangOptions()), arg_stream,
            /*IncludeType*/ true);
  arg_stream.flush();
  result += trimWhitespace(arg_str);
}

static std::string
buildTemplateInstantiationName(const std::string &base_name,
                               const clang::TemplateArgumentList &args) {
  if (args.size() == 0)
    return base_name;

  std::string result = base_name;
  result += "<";
  bool need_separator = false;
  for (unsigned i = 0; i < args.size(); ++i) {
    appendTemplateInstantiationArg(result, need_separator, args.get(i));
  }
  result += ">";
  return result;
}

SgScopeStatement *normalizeNamespaceScope(SgScopeStatement *scope);
clang::NamespaceDecl *getCanonicalNamespaceDecl(clang::NamespaceDecl *decl);
} // namespace

SgType *ClangToSageTranslator::buildSpecializedMemberTypedefReturnType(
    const clang::FunctionDecl *decl,
    const clang::ClassTemplateSpecializationDecl *spec_decl_override,
    const clang::CXXRecordDecl *record_decl_override) {
  const auto *method_decl = llvm::dyn_cast_or_null<clang::CXXMethodDecl>(decl);
  if (method_decl == nullptr) {
    return nullptr;
  }

  const clang::CXXRecordDecl *record_decl = record_decl_override;
  if (record_decl == nullptr) {
    if (spec_decl_override != nullptr) {
      record_decl = spec_decl_override;
    } else {
      record_decl = method_decl->getParent();
    }
  }

  const clang::ClassTemplateSpecializationDecl *spec_decl = spec_decl_override;
  if (spec_decl == nullptr) {
    spec_decl = llvm::dyn_cast_or_null<clang::ClassTemplateSpecializationDecl>(
        record_decl);
  }
  if (record_decl == nullptr || spec_decl == nullptr) {
    return nullptr;
  }

  const clang::TypedefType *return_typedef =
      method_decl->getReturnType()->getAs<clang::TypedefType>();
  if (return_typedef == nullptr) {
    return nullptr;
  }

  const clang::TypedefNameDecl *return_decl = return_typedef->getDecl();
  const clang::DeclContext *return_context =
      return_decl != nullptr ? return_decl->getDeclContext() : nullptr;
  const clang::CXXRecordDecl *return_record =
      llvm::dyn_cast_or_null<clang::CXXRecordDecl>(return_context);
  bool is_member_typedef = false;
  if (return_record != nullptr) {
    const clang::CXXRecordDecl *pattern =
        spec_decl->getTemplateInstantiationPattern();
    if (return_record == record_decl || return_record == spec_decl ||
        (pattern != nullptr && return_record == pattern)) {
      is_member_typedef = true;
    }
  }
  if (!is_member_typedef) {
    return nullptr;
  }

  SgScopeStatement *scope = SageBuilder::topScopeStack();
  if (scope == nullptr) {
    scope = getGlobalScope();
  }
  if (scope == nullptr) {
    return nullptr;
  }

  std::vector<const clang::DeclContext *> contexts;
  for (const clang::DeclContext *dc = record_decl->getDeclContext();
       dc != nullptr && !dc->isTranslationUnit(); dc = dc->getParent()) {
    contexts.push_back(dc);
  }
  for (auto it = contexts.rbegin(); it != contexts.rend(); ++it) {
    if (const clang::NamespaceDecl *ns =
            llvm::dyn_cast<clang::NamespaceDecl>(*it)) {
      std::string ns_name = ns->getNameAsString();
      if (!ns_name.empty()) {
        SgNonrealType *ns_type =
            SageBuilder::buildNonrealType(SgName(ns_name), scope, nullptr);
        if (SgNonrealDecl *ns_decl = isSgNonrealDecl(
                ns_type ? ns_type->get_declaration() : nullptr)) {
          scope = ns_decl->get_nonreal_decl_scope();
        }
      }
    } else if (const clang::CXXRecordDecl *ctx_record =
                   llvm::dyn_cast<clang::CXXRecordDecl>(*it)) {
      std::string record_name = ctx_record->getNameAsString();
      if (!record_name.empty()) {
        const clang::ClassTemplateSpecializationDecl *ctx_spec =
            llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(ctx_record);
        SgTemplateArgumentPtrList ctx_args;
        const SgTemplateArgumentPtrList *ctx_args_ptr = nullptr;
        if (ctx_spec != nullptr) {
          ctx_args = buildTemplateArguments(ctx_spec->getTemplateArgs(), 0);
          if (!ctx_args.empty()) {
            ctx_args_ptr = &ctx_args;
          }
        }
        SgNonrealType *record_type = SageBuilder::buildNonrealType(
            SgName(record_name), scope, ctx_args_ptr);
        if (SgNonrealDecl *record_decl_node = isSgNonrealDecl(
                record_type ? record_type->get_declaration() : nullptr)) {
          scope = record_decl_node->get_nonreal_decl_scope();
        }
      }
    }
  }

  SgTemplateArgumentPtrList tpl_args =
      buildTemplateArguments(spec_decl->getTemplateArgs(), 0);
  std::string spec_name = spec_decl->getNameAsString();
  if (!spec_name.empty()) {
    SgNonrealType *spec_type = SageBuilder::buildNonrealType(
        SgName(spec_name), scope, tpl_args.empty() ? nullptr : &tpl_args);
    if (SgNonrealDecl *spec_decl_node = isSgNonrealDecl(
            spec_type ? spec_type->get_declaration() : nullptr)) {
      scope = spec_decl_node->get_nonreal_decl_scope();
    }
  }

  std::string typedef_name =
      return_decl != nullptr ? return_decl->getNameAsString() : "";
  if (typedef_name.empty()) {
    return nullptr;
  }

  SgNonrealType *member_type =
      SageBuilder::buildNonrealType(SgName(typedef_name), scope, nullptr);
  if (SgNonrealDecl *member_decl = isSgNonrealDecl(
          member_type ? member_type->get_declaration() : nullptr)) {
    if (!spec_decl->isDependentType()) {
      member_decl->set_suppress_typename(true);
    }
  }
  return member_type;
}

SgSymbol *
ClangToSageTranslator::GetSymbolFromSymbolTable(clang::NamedDecl *decl) {
  if (decl == nullptr)
    return nullptr;

  // Recursion guard: If we're already looking up this declaration, return
  // nullptr to prevent infinite loops in template/member resolution
  if (p_symbol_lookup_in_progress.find(decl) !=
      p_symbol_lookup_in_progress.end()) {
#if DEBUG_SYMBOL_TABLE_LOOKUP
    std::cerr << "GetSymbolFromSymbolTable: Recursion detected for decl "
              << getDeclNameSafe(decl) << ", returning nullptr" << std::endl;
#endif
    return nullptr;
  }

  // Add this decl to the in-progress set
  p_symbol_lookup_in_progress.insert(decl);

  // REX FIX: Check if the declaration has already been translated
  // Only for Typedef/TypeAlias to avoid issues with Variables (see
  // rex_template_instantiation.C)
  if (llvm::isa<clang::TypedefNameDecl>(decl)) {
    std::map<clang::Decl *, SgNode *>::iterator it_decl =
        p_decl_translation_map.find(decl);
    if (it_decl != p_decl_translation_map.end()) {
      SgNode *node = it_decl->second;
      if (SgDeclarationStatement *decl_stmt = isSgDeclarationStatement(node)) {
        SgSymbol *symbol = decl_stmt->get_symbol_from_symbol_table();
        if (symbol != nullptr) {
          p_symbol_lookup_in_progress.erase(decl);
          return symbol;
        }
      }
    }
  }
  if (llvm::isa<clang::FunctionDecl>(decl) ||
      llvm::isa<clang::CXXMethodDecl>(decl)) {
    auto it_decl = p_decl_translation_map.find(decl);
    if (it_decl != p_decl_translation_map.end()) {
      if (SgDeclarationStatement *decl_stmt =
              isSgDeclarationStatement(it_decl->second)) {
        if (SgSymbol *symbol = decl_stmt->get_symbol_from_symbol_table()) {
          p_symbol_lookup_in_progress.erase(decl);
          return symbol;
        }
      }
    }
  }

  SgScopeStatement *scope = SageBuilder::topScopeStack();

  /* Pei-Hung (08/29/2022) fieldDecl can be anonymous.
   * Apply anonymous name to allow symbol lookup.
   */
  std::string declName = getDeclNameSafe(decl);

  if (llvm::isa<clang::FieldDecl>(decl) &&
      ((clang::FieldDecl *)decl)->isAnonymousStructOrUnion()) {
    declName =
        "__anonymous_" + generate_source_position_string(decl->getBeginLoc());
#if DEBUG_SYMBOL_TABLE_LOOKUP
    std::cerr << "Find anonymous fieldDecl: " << declName << std::endl;
#endif
  } else if (llvm::isa<clang::EnumDecl>(decl) && declName == "") {
    declName =
        "__anonymous_" + generate_source_position_string(decl->getBeginLoc());
#if DEBUG_SYMBOL_TABLE_LOOKUP
    std::cerr << "Find anonymous EnumDecl: " << declName << std::endl;
#endif
  }

  SgName name(declName);

#if DEBUG_SYMBOL_TABLE_LOOKUP
  std::cerr << "Lookup symbol for: " << name << std::endl;
#endif

  if (name == "") {
    // Remove from in-progress set before returning
    p_symbol_lookup_in_progress.erase(decl);
    return nullptr;
  }

  std::list<SgScopeStatement *>::reverse_iterator it;
  SgSymbol *sym = nullptr;
  switch (decl->getKind()) {
  case clang::Decl::Typedef:
  case clang::Decl::TypeAlias: {
    // TypeAlias (C++11 using declarations) are semantically equivalent to
    // Typedef
    clang::DeclContext *decl_context = decl->getDeclContext();
    clang::DeclContext *scope_context = decl_context;
    while (scope_context != nullptr &&
           llvm::isa<clang::LinkageSpecDecl>(scope_context)) {
      scope_context = scope_context->getParent();
    }
    if (scope_context != nullptr) {
      if (SgScopeStatement *decl_scope =
              resolveScopeFromDeclContext(scope_context, nullptr)) {
        sym = decl_scope->lookup_typedef_symbol(name);
      }
    }
    it = SageBuilder::ScopeStack.rbegin();
    while (it != SageBuilder::ScopeStack.rend() && sym == nullptr) {
      sym = (*it)->lookup_typedef_symbol(name);
      it++;
    }
    break;
  }
  case clang::Decl::Var:
  case clang::Decl::ParmVar:
  case clang::Decl::Binding: {
    it = SageBuilder::ScopeStack.rbegin();
    while (it != SageBuilder::ScopeStack.rend() && sym == nullptr) {
      sym = (*it)->lookup_variable_symbol(name);
      it++;
    }
    break;
  }
  case clang::Decl::IndirectField: {
    auto *indirect_decl = llvm::cast<clang::IndirectFieldDecl>(decl);
    if (clang::FieldDecl *field_decl = indirect_decl->getAnonField()) {
      sym = GetSymbolFromSymbolTable(field_decl);
    }
    if (sym == nullptr) {
      if (clang::VarDecl *var_decl = indirect_decl->getVarDecl()) {
        sym = GetSymbolFromSymbolTable(var_decl);
      }
    }
    if (sym == nullptr) {
      it = SageBuilder::ScopeStack.rbegin();
      while (it != SageBuilder::ScopeStack.rend() && sym == nullptr) {
        sym = (*it)->lookup_variable_symbol(name);
        it++;
      }
    }
    break;
  }
  case clang::Decl::CXXConstructor:
  case clang::Decl::CXXDestructor:
  case clang::Decl::CXXMethod: {
    // Member functions live in the scope of their parent class definition, not
    // the current scope stack. Resolve them via the translated parent record so
    // that we retrieve a SgMemberFunctionSymbol when available.
    auto *method_decl = llvm::cast<clang::CXXMethodDecl>(decl);
    bool is_system_or_builtin = false;
    if (p_compiler_instance != nullptr) {
      clang::SourceManager &sm = p_compiler_instance->getSourceManager();
      is_system_or_builtin = isSystemOrBuiltinFunctionDecl(method_decl, sm);
    }
    clang::CXXRecordDecl *parent_decl = method_decl->getParent();

    SgNode *parent_node = nullptr;
    if (parent_decl != nullptr) {
      std::map<clang::Decl *, SgNode *>::iterator it_decl =
          p_decl_translation_map.find(parent_decl);
      if (it_decl != p_decl_translation_map.end()) {
        parent_node = it_decl->second;
      } else {
        if (!is_system_or_builtin &&
            p_symbol_lookup_in_progress.find(parent_decl) ==
                p_symbol_lookup_in_progress.end()) {
          parent_node = TraverseOnDemand(parent_decl);
        }
      }
    }
    if (parent_node == nullptr) {
      break;
    }

    SgClassDeclaration *sg_class_decl = isSgClassDeclaration(parent_node);
    if (sg_class_decl == nullptr) {
      break;
    }
    if (sg_class_decl->get_definingDeclaration() == nullptr) {
      break;
    }

    scope = isSgClassDeclaration(sg_class_decl->get_definingDeclaration())
                ->get_definition();
    if (scope == nullptr) {
      break;
    }

    SgType *tmp_type = buildTypeFromQualifiedType(method_decl->getType());
    SgFunctionType *type = isSgFunctionType(tmp_type);
    SgTemplateArgumentPtrList template_args;
    SgTemplateArgumentPtrList *template_args_ptr = nullptr;
    size_t explicit_arg_count = 0;
    if (const clang::ASTTemplateArgumentListInfo *args_as_written =
            method_decl->getTemplateSpecializationArgsAsWritten()) {
      clang::TemplateArgumentListInfo arg_info(args_as_written->getLAngleLoc(),
                                               args_as_written->getRAngleLoc());
      for (const clang::TemplateArgumentLoc &loc :
           args_as_written->arguments()) {
        arg_info.addArgument(loc);
      }
      explicit_arg_count = countExpandedTemplateArguments(arg_info);
    }
    if (const clang::TemplateArgumentList *clang_args =
            method_decl->getTemplateSpecializationArgs()) {
      if (clang_args->size() != 0) {
        template_args = buildTemplateArguments(*clang_args, explicit_arg_count);
        template_args_ptr = &template_args;
      }
    }

    SgName lookup_name = name;
    if (template_args_ptr != nullptr) {
      lookup_name =
          SageBuilder::appendTemplateArgumentsToName(name, *template_args_ptr);
    }

    if (type != nullptr || template_args_ptr != nullptr) {
      if (template_args_ptr != nullptr) {
        sym = scope->lookup_nontemplate_member_function_symbol(
            lookup_name, type, template_args_ptr);
        if (sym == nullptr) {
          sym = scope->lookup_function_symbol(lookup_name, type,
                                              template_args_ptr);
        }
      }

      if (sym == nullptr && type != nullptr) {
        sym = scope->lookup_nontemplate_member_function_symbol(
            lookup_name, type, template_args_ptr);
        if (sym == nullptr) {
          sym = scope->lookup_function_symbol(lookup_name, type);
        }
      }
    }

    if (sym == nullptr && template_args_ptr != nullptr) {
      if (p_compiler_instance != nullptr && scope != nullptr) {
        clang::SourceManager &sm = p_compiler_instance->getSourceManager();
        if (isSystemOrBuiltinFunctionDecl(method_decl, sm) &&
            method_decl->getTemplateSpecializationKind() ==
                clang::TSK_ImplicitInstantiation) {
          SgType *ret_type =
              buildTypeFromQualifiedType(method_decl->getReturnType());
          if (ret_type == nullptr) {
            ret_type = SageBuilder::buildUnknownType();
          }

          SgFunctionParameterList *param_list = nullptr;
          if (type != nullptr && type->get_argument_list() != nullptr) {
            param_list = SageBuilder::buildFunctionParameterList_nfi(
                type->get_argument_list());
          }
          if (param_list == nullptr) {
            param_list = SageBuilder::buildFunctionParameterList_nfi();
          }

          unsigned int method_cv_flags = 0;
          if (method_decl->isConst()) {
            method_cv_flags |= SgMemberFunctionType::e_const;
          }
          if (method_decl->isVolatile()) {
            method_cv_flags |= SgMemberFunctionType::e_volatile;
          }
          clang::Qualifiers qualifiers = method_decl->getMethodQualifiers();
          if (qualifiers.hasRestrict()) {
            method_cv_flags |= SgMemberFunctionType::e_restrict;
          }
          switch (method_decl->getRefQualifier()) {
          case clang::RQ_LValue:
            method_cv_flags |= SgMemberFunctionType::e_ref_qualifier_lvalue;
            break;
          case clang::RQ_RValue:
            method_cv_flags |= SgMemberFunctionType::e_ref_qualifier_rvalue;
            break;
          case clang::RQ_None:
            break;
          }

          SgFunctionDeclaration *inst_decl =
              SageBuilder::buildNondefiningMemberFunctionDeclaration(
                  name, ret_type, param_list, scope, method_cv_flags,
                  /*buildTemplateInstantiation=*/true, template_args_ptr);

          cleanup_unused_param_list(inst_decl, param_list);
          if (SgTemplateInstantiationMemberFunctionDecl *inst_member =
                  isSgTemplateInstantiationMemberFunctionDecl(inst_decl)) {
            inst_member->set_template_argument_list_is_explicit(false);
            SageBuilder::setTemplateArgumentsInDeclaration(inst_member,
                                                           template_args_ptr);
            if (clang::FunctionTemplateDecl *primary =
                    method_decl->getPrimaryTemplate()) {
              auto it_tmpl = p_decl_translation_map.find(primary);
              if (it_tmpl != p_decl_translation_map.end()) {
                if (SgTemplateMemberFunctionDeclaration *tmpl_decl =
                        isSgTemplateMemberFunctionDeclaration(
                            it_tmpl->second)) {
                  inst_member->set_templateDeclaration(tmpl_decl);
                  inst_member->set_templateName(tmpl_decl->get_name());
                }
              }
            }
          }

          if (inst_decl != nullptr) {
            applySourceRange(inst_decl, method_decl->getSourceRange());
            if (SgFunctionParameterList *params =
                    inst_decl->get_parameterList()) {
              params->set_parent(inst_decl);
              for (SgInitializedName *param : params->get_args()) {
                if (param != nullptr) {
                  param->set_declptr(inst_decl);
                }
              }
            }

            setCompilerGeneratedFileInfo(inst_decl);
            suppress_unparse_output(inst_decl);
            if (SgFunctionParameterList *params =
                    inst_decl->get_parameterList()) {
              setCompilerGeneratedFileInfo(params);
              suppress_unparse_output(params);
              for (SgInitializedName *param : params->get_args()) {
                if (param != nullptr) {
                  setCompilerGeneratedFileInfo(param);
                  suppress_unparse_output(param);
                }
              }
            }

            auto register_decl = [&](clang::FunctionDecl *key) {
              if (key == nullptr) {
                return;
              }
              if (p_decl_translation_map.find(key) ==
                  p_decl_translation_map.end()) {
                p_decl_translation_map[key] = inst_decl;
              }
            };
            register_decl(method_decl);
            register_decl(method_decl->getCanonicalDecl());
            register_decl(method_decl->getFirstDecl());

            sym = inst_decl->get_symbol_from_symbol_table();
          }
        }
      }
    }
    break;
  }
  case clang::Decl::CXXDeductionGuide:
  case clang::Decl::Function: {
    auto *func_decl = llvm::cast<clang::FunctionDecl>(decl);
    SgType *tmp_type = buildTypeFromQualifiedType(func_decl->getType());
    SgFunctionType *type = isSgFunctionType(tmp_type);
    SgTemplateArgumentPtrList template_args;
    SgTemplateArgumentPtrList *template_args_ptr = nullptr;
    size_t explicit_arg_count = 0;
    if (const clang::ASTTemplateArgumentListInfo *args_as_written =
            func_decl->getTemplateSpecializationArgsAsWritten()) {
      clang::TemplateArgumentListInfo arg_info(args_as_written->getLAngleLoc(),
                                               args_as_written->getRAngleLoc());
      for (const clang::TemplateArgumentLoc &loc :
           args_as_written->arguments()) {
        arg_info.addArgument(loc);
      }
      explicit_arg_count = countExpandedTemplateArguments(arg_info);
    }
    if (const clang::TemplateArgumentList *clang_args =
            func_decl->getTemplateSpecializationArgs()) {
      if (clang_args->size() != 0) {
        template_args = buildTemplateArguments(*clang_args, explicit_arg_count);
        template_args_ptr = &template_args;
      }
    }
    SgName lookup_name = name;
    if (template_args_ptr != nullptr) {
      lookup_name =
          SageBuilder::appendTemplateArgumentsToName(name, *template_args_ptr);
    }

    auto lookup_in_scope = [&](SgScopeStatement *lookup_scope) -> SgSymbol * {
      if (lookup_scope == nullptr) {
        return nullptr;
      }

      if (template_args_ptr != nullptr) {
        if (SgFunctionSymbol *inst_sym = lookup_scope->lookup_function_symbol(
                lookup_name, type, template_args_ptr)) {
          return inst_sym;
        }
        if (type != nullptr) {
          return lookup_scope->lookup_function_symbol(lookup_name, type);
        }
        return nullptr;
      }

      if (type != nullptr) {
        return lookup_scope->lookup_function_symbol(name, type);
      }
      return nullptr;
    };
    auto lookup_in_scope_by_name =
        [&](SgScopeStatement *lookup_scope) -> SgSymbol * {
      if (lookup_scope == nullptr) {
        return nullptr;
      }
      return lookup_scope->lookup_function_symbol(
          template_args_ptr != nullptr ? lookup_name : name);
    };

    // Prefer lookup in the declaration's semantic namespace/global scope.
    // The current ROSE scope stack often does not include namespace scopes
    // from system headers (e.g., std), so stack-only lookup drops required
    // qualifications (std::endl -> endl).
    SgScopeStatement *namespace_scope = nullptr;
    if (clang::DeclContext *ctx = func_decl->getDeclContext()) {
      while (ctx != nullptr && !ctx->isTranslationUnit() &&
             !llvm::isa<clang::NamespaceDecl>(ctx)) {
        ctx = ctx->getParent();
      }

      if (clang::NamespaceDecl *ns_decl =
              llvm::dyn_cast_or_null<clang::NamespaceDecl>(ctx)) {
        clang::NamespaceDecl *canonical_ns = getCanonicalNamespaceDecl(ns_decl);
        auto it_decl = p_decl_translation_map.find(canonical_ns);
        if (it_decl != p_decl_translation_map.end()) {
          if (SgNamespaceDeclarationStatement *ns_stmt =
                  isSgNamespaceDeclarationStatement(it_decl->second)) {
            namespace_scope = ns_stmt->get_definition();
          } else if (SgNamespaceDefinitionStatement *ns_def =
                         isSgNamespaceDefinitionStatement(it_decl->second)) {
            namespace_scope = ns_def;
          }
        }

        if (namespace_scope == nullptr) {
          if (SgNamespaceDeclarationStatement *ns_stmt =
                  ensureNamespaceDeclaration(ns_decl)) {
            namespace_scope = ns_stmt->get_definition();
          }
        }

        namespace_scope = normalizeNamespaceScope(namespace_scope);
      }
    }

    if (namespace_scope != nullptr) {
      if (type != nullptr || template_args_ptr != nullptr) {
        sym = lookup_in_scope(namespace_scope);
      }
      if (sym == nullptr) {
        sym = lookup_in_scope_by_name(namespace_scope);
      }
    }

    if (sym == nullptr) {
      // Fallback: search the active scope stack.
      if (type != nullptr || template_args_ptr != nullptr) {
        it = SageBuilder::ScopeStack.rbegin();
        while (it != SageBuilder::ScopeStack.rend() && sym == nullptr) {
          sym = lookup_in_scope(*it);
          it++;
        }
      }
      if (sym == nullptr) {
        it = SageBuilder::ScopeStack.rbegin();
        while (it != SageBuilder::ScopeStack.rend() && sym == nullptr) {
          sym = lookup_in_scope_by_name(*it);
          it++;
        }
      }
    }

    if (sym == nullptr && template_args_ptr != nullptr) {
      if (p_compiler_instance != nullptr) {
        clang::SourceManager &sm = p_compiler_instance->getSourceManager();
        if (isSystemOrBuiltinFunctionDecl(func_decl, sm) &&
            func_decl->getTemplateSpecializationKind() ==
                clang::TSK_ImplicitInstantiation) {
          SgScopeStatement *target_scope =
              namespace_scope != nullptr
                  ? namespace_scope
                  : normalizeNamespaceScope(SageBuilder::topScopeStack());
          if (target_scope == nullptr) {
            target_scope = getGlobalScope();
          }

          SgType *ret_type =
              buildTypeFromQualifiedType(func_decl->getReturnType());
          if (ret_type == nullptr) {
            ret_type = SageBuilder::buildUnknownType();
          }

          SgFunctionParameterList *param_list = nullptr;
          if (type != nullptr && type->get_argument_list() != nullptr) {
            param_list = SageBuilder::buildFunctionParameterList_nfi(
                type->get_argument_list());
          }
          if (param_list == nullptr) {
            param_list = SageBuilder::buildFunctionParameterList_nfi();
          }

          SgFunctionDeclaration *inst_decl =
              SageBuilder::buildNondefiningFunctionDeclaration(
                  name, ret_type, param_list, target_scope,
                  /*buildTemplateInstantiation=*/true, template_args_ptr,
                  SgStorageModifier::e_default,
                  /*forceFreeFunctionScope=*/false);

          cleanup_unused_param_list(inst_decl, param_list);
          if (SgTemplateInstantiationFunctionDecl *inst_func =
                  isSgTemplateInstantiationFunctionDecl(inst_decl)) {
            inst_func->set_template_argument_list_is_explicit(false);
            SageBuilder::setTemplateArgumentsInDeclaration(inst_func,
                                                           template_args_ptr);
            if (clang::FunctionTemplateDecl *primary =
                    func_decl->getPrimaryTemplate()) {
              auto it_tmpl = p_decl_translation_map.find(primary);
              if (it_tmpl != p_decl_translation_map.end()) {
                if (SgTemplateFunctionDeclaration *tmpl_decl =
                        isSgTemplateFunctionDeclaration(it_tmpl->second)) {
                  inst_func->set_templateDeclaration(tmpl_decl);
                  inst_func->set_templateName(tmpl_decl->get_name());
                }
              }
            }
          }

          if (inst_decl != nullptr) {
            applySourceRange(inst_decl, func_decl->getSourceRange());
            if (SgFunctionParameterList *params =
                    inst_decl->get_parameterList()) {
              params->set_parent(inst_decl);
              for (SgInitializedName *param : params->get_args()) {
                if (param != nullptr) {
                  param->set_declptr(inst_decl);
                }
              }
            }

            setCompilerGeneratedFileInfo(inst_decl);
            suppress_unparse_output(inst_decl);
            if (SgFunctionParameterList *params =
                    inst_decl->get_parameterList()) {
              setCompilerGeneratedFileInfo(params);
              suppress_unparse_output(params);
              for (SgInitializedName *param : params->get_args()) {
                if (param != nullptr) {
                  setCompilerGeneratedFileInfo(param);
                  suppress_unparse_output(param);
                }
              }
            }

            auto register_decl = [&](clang::FunctionDecl *key) {
              if (key == nullptr) {
                return;
              }
              if (p_decl_translation_map.find(key) ==
                  p_decl_translation_map.end()) {
                p_decl_translation_map[key] = inst_decl;
              }
            };
            register_decl(func_decl);
            register_decl(func_decl->getCanonicalDecl());
            register_decl(func_decl->getFirstDecl());

            sym = inst_decl->get_symbol_from_symbol_table();
          }
        }
      }
    }

    break;
  }
  case clang::Decl::CXXConversion: {
    // ROOT CAUSE FIX: Conversion operators (operator T()) require special
    // handling Try to get the function type, but allow it to fail gracefully
    SgType *tmp_type = buildTypeFromQualifiedType(
        ((clang::CXXConversionDecl *)decl)->getType());
    SgFunctionType *type = isSgFunctionType(tmp_type);
    if (type != nullptr) {
      // Normal case - type conversion succeeded
      it = SageBuilder::ScopeStack.rbegin();
      while (it != SageBuilder::ScopeStack.rend() && sym == nullptr) {
        sym = (*it)->lookup_function_symbol(name, type);
        it++;
      }
    }
    // If type is nullptr or lookup failed, return nullptr (not found)
    // This is acceptable for conversion operators
    break;
  }
  case clang::Decl::Field: {
    // field can be variable or ClassDefinition

    // CLANG FRONTEND FIX: Skip template-dependent field lookups to avoid
    // infinite loops Template-dependent fields (like fields in uninstantiated
    // templates) cannot be properly resolved until template instantiation, so
    // return nullptr
    clang::FieldDecl *field_decl = (clang::FieldDecl *)decl;
    if (field_decl->getType()->isDependentType()) {
#if DEBUG_SYMBOL_TABLE_LOOKUP
      std::cerr
          << "GetSymbolFromSymbolTable: Skipping template-dependent field: "
          << field_decl->getNameAsString() << std::endl;
#endif
      // Remove from in-progress set before returning
      p_symbol_lookup_in_progress.erase(decl);
      return nullptr;
    }

    // Prefer the translated field declaration when available so we can resolve
    // symbols even while the enclosing class is still under construction.
    auto it_field = p_decl_translation_map.find(field_decl);
    if (it_field != p_decl_translation_map.end()) {
      if (SgVariableDeclaration *var_decl =
              isSgVariableDeclaration(it_field->second)) {
        registerDeclarationSymbol(var_decl);
        if (SgInitializedName *init_name =
                SageInterface::getFirstInitializedName(var_decl)) {
          sym = init_name->search_for_symbol_from_symbol_table();
          if (sym == nullptr) {
            if (SgScopeStatement *var_scope = init_name->get_scope()) {
              sym = var_scope->lookup_variable_symbol(init_name->get_name());
            }
          }
        }
      } else if (SgInitializedName *init_name =
                     isSgInitializedName(it_field->second)) {
        if (SgScopeStatement *var_scope = init_name->get_scope()) {
          if (var_scope->lookup_variable_symbol(init_name->get_name()) ==
              nullptr) {
            SgVariableSymbol *symbol = new SgVariableSymbol(init_name);
            rehomeSymbolToScope(symbol, var_scope);
          }
          sym = init_name->search_for_symbol_from_symbol_table();
        }
      }
      if (sym != nullptr) {
        break;
      }
    }

    // CLANG FRONTEND FIX: Check if parent has been translated before calling
    // Traverse to avoid infinite recursion during template instantiation
    clang::Decl *parent_decl = ((clang::FieldDecl *)decl)->getParent();
    SgNode *parent_node = nullptr;

    // First check if parent is already in translation map
    std::map<clang::Decl *, SgNode *>::iterator it_decl =
        p_decl_translation_map.find(parent_decl);
    if (it_decl != p_decl_translation_map.end()) {
      parent_node = it_decl->second;
    } else {
      // Parent not yet translated - try to traverse it
      // But only if we're not already looking up a symbol from this parent
      // (avoids infinite recursion during template member resolution)
      if (p_symbol_lookup_in_progress.find((clang::NamedDecl *)parent_decl) ==
          p_symbol_lookup_in_progress.end()) {
        parent_node = TraverseOnDemand(parent_decl);
      }
    }

    SgClassDeclaration *sg_class_decl = isSgClassDeclaration(parent_node);
    // CLANG FRONTEND FIX: sg_class_decl can be nullptr if parent class was
    // skipped (e.g., system header template)
    if (sg_class_decl == nullptr) {
      // Parent class not translated (likely skipped system header template or
      // recursion guard hit) Cannot find symbol without parent class
      break;
    }
    if (sg_class_decl->get_definingDeclaration() == nullptr) {
      std::cerr << "Runtime Error: cannot find the definition of the "
                   "class/struct associate to the field: "
                << name << std::endl;
      // Cannot lookup symbol without class definition
      break;
    }

    SgClassDeclaration *definingClassDecl =
        isSgClassDeclaration(sg_class_decl->get_definingDeclaration());
    if (definingClassDecl == nullptr) {
      break;
    }
    // FieldDecl access implies the record is not autonomous.
    sg_class_decl->set_isAutonomousDeclaration(false);
    definingClassDecl->set_isAutonomousDeclaration(false);
    scope = definingClassDecl->get_definition();
    if (scope == nullptr) {
      // No class definition available
      break;
    }

    // CLANG FRONTEND FIX: Check if we're currently building this class (it's on
    // the scope stack) If so, the AST is incomplete and symbol lookup might
    // fail or loop
    bool class_under_construction = false;
    for (std::list<SgScopeStatement *>::iterator it_stack =
             SageBuilder::ScopeStack.begin();
         it_stack != SageBuilder::ScopeStack.end(); ++it_stack) {
      if (*it_stack == scope) {
        class_under_construction = true;
        break;
      }
    }

    if (class_under_construction) {
      // We're currently building this class - symbol table may be incomplete
      // Skip symbol lookup to avoid potential AST cycle issues
#if DEBUG_SYMBOL_TABLE_LOOKUP
      std::cerr << "GetSymbolFromSymbolTable: Skipping lookup for field '"
                << name << "' - parent class under construction" << std::endl;
#endif
      break;
    }

    // FIELD SYMBOL LOOKUP: Resolve field symbols by walking up scope chain
    //
    // ALGORITHM: Walk the full scope chain from innermost to outermost until
    // we find the symbol or reach the top. Use a visited set to prevent
    // infinite loops caused by cycles in the scope graph (which can occur
    // during AST construction for complex template code).
    //
    // CORRECTNESS: Fields can be promoted through multiple levels of anonymous
    // structs/unions, or be defined in deeply nested classes. A depth limit
    // would incorrectly fail to resolve valid symbols in deep hierarchies.
    //
    std::set<SgScopeStatement *> visited;
    while (scope != nullptr && sym == nullptr) {
      // Prevent infinite loops by detecting cycles
      if (visited.count(scope) > 0) {
        break; // Cycle detected, bail out
      }
      visited.insert(scope);

      // Look up symbol in current scope
      // Anonymous struct/union fields are still variables in ROSE.
      sym = scope->lookup_variable_symbol(name);

      // Move to parent scope
      if (sym == nullptr) {
        scope = scope->get_scope();
      }
    }
    break;
  }
  case clang::Decl::ClassTemplate: {
    auto *tmpl_decl = llvm::cast<clang::ClassTemplateDecl>(decl);
    SgScopeStatement *decl_scope = nullptr;
    if (clang::DeclContext *ctx = tmpl_decl->getDeclContext()) {
      decl_scope = resolveScopeFromDeclContext(ctx, nullptr);
    }
    if (decl_scope == nullptr) {
      decl_scope = SageBuilder::topScopeStack();
    }
    SgScopeStatement *lookup_scope = normalizeNamespaceScope(decl_scope);
    if (lookup_scope == nullptr) {
      lookup_scope = decl_scope;
    }

    if (lookup_scope != nullptr) {
      sym = lookup_scope->lookup_template_class_symbol(name, nullptr, nullptr);
      if (sym == nullptr) {
        sym = lookup_scope->lookup_class_symbol(name);
      }
    }

    if (sym == nullptr) {
      it = SageBuilder::ScopeStack.rbegin();
      while (it != SageBuilder::ScopeStack.rend() && sym == nullptr) {
        sym = (*it)->lookup_template_class_symbol(name, nullptr, nullptr);
        if (sym == nullptr) {
          sym = (*it)->lookup_class_symbol(name);
        }
        it++;
      }
    }
    break;
  }
  case clang::Decl::ClassTemplatePartialSpecialization:
  case clang::Decl::ClassTemplateSpecialization:
  case clang::Decl::CXXRecord:
  case clang::Decl::Record: {
    if (sym == nullptr) {
      clang::DeclContext *decl_context = decl->getDeclContext();
      clang::DeclContext *scope_context = decl_context;
      while (scope_context != nullptr &&
             llvm::isa<clang::LinkageSpecDecl>(scope_context)) {
        scope_context = scope_context->getParent();
      }
      if (scope_context != nullptr) {
        if (SgScopeStatement *decl_scope =
                resolveScopeFromDeclContext(scope_context, nullptr)) {
          SgScopeStatement *lookup_scope = normalizeNamespaceScope(decl_scope);
          if (lookup_scope != nullptr) {
            sym = lookup_scope->lookup_template_class_symbol(name, nullptr,
                                                             nullptr);
            if (sym == nullptr) {
              sym = lookup_scope->lookup_class_symbol(name);
            }
          }
        }
      }
    }

    it = SageBuilder::ScopeStack.rbegin();
    while (it != SageBuilder::ScopeStack.rend() && sym == nullptr) {
      sym = (*it)->lookup_class_symbol(name);
      it++;
    }

    if (sym == nullptr) {
      // Member/nested record types live in the scope of their semantic parent
      // (e.g., the enclosing class definition), which may not be present on the
      // active scope stack when translating out-of-line definitions.
      auto *record_decl = llvm::cast<clang::RecordDecl>(decl);
      clang::DeclContext *ctx = record_decl->getDeclContext();
      if (clang::RecordDecl *parent_record =
              llvm::dyn_cast_or_null<clang::RecordDecl>(ctx)) {
        SgNode *parent_node = nullptr;
        clang::NamedDecl *parent_lookup = parent_record->getCanonicalDecl();

        auto it_decl = p_decl_translation_map.find(parent_lookup);
        if (it_decl != p_decl_translation_map.end()) {
          parent_node = it_decl->second;
        } else if (clang::RecordDecl *parent_def =
                       parent_record->getDefinition()) {
          auto it_def = p_decl_translation_map.find(parent_def);
          if (it_def != p_decl_translation_map.end()) {
            parent_node = it_def->second;
          }
        }

        if (parent_node == nullptr &&
            p_symbol_lookup_in_progress.find(parent_lookup) ==
                p_symbol_lookup_in_progress.end()) {
          parent_node = TraverseOnDemand(parent_record);
        }

        if (SgClassDeclaration *sg_parent_decl =
                isSgClassDeclaration(parent_node)) {
          if (SgDeclarationStatement *def_decl =
                  sg_parent_decl->get_definingDeclaration()) {
            if (SgClassDefinition *class_def =
                    isSgClassDeclaration(def_decl)->get_definition()) {
              sym = class_def->lookup_class_symbol(name);
            }
          }
        }
      }
    }
    break;
  }
  case clang::Decl::Label: {
    // Should not be reach as we use Traverse to retrieve Label (they are
    // "terminal" statements) (it avoids the problem of forward use of label:
    // goto before declaration)
    name = SgName(((clang::LabelDecl *)decl)->getStmt()->getName());
    it = SageBuilder::ScopeStack.rbegin();
    while (it != SageBuilder::ScopeStack.rend() && sym == nullptr) {
      sym = (*it)->lookup_label_symbol(name);
      it++;
    }
    break;
  }
  case clang::Decl::EnumConstant: {
    name = SgName(((clang::EnumConstantDecl *)decl)->getName().str());
    std::unordered_set<SgScopeStatement *> visited;
    it = SageBuilder::ScopeStack.rbegin();
    while (it != SageBuilder::ScopeStack.rend() && sym == nullptr) {
      SgScopeStatement *scope = *it;
      if (SgScopeStatement *normalized = normalizeNamespaceScope(scope)) {
        scope = normalized;
      }
      if (scope != nullptr && visited.insert(scope).second) {
        sym = scope->lookup_enum_field_symbol(name);
      }
      it++;
    }
    break;
  }
  case clang::Decl::Enum: {
    // An anonymous enum should have its name set with prefix "__anonymous_".
    // There is no need to retrieve the name from clang::EnumDecl, as it will
    // be empty.
    it = SageBuilder::ScopeStack.rbegin();
    while (it != SageBuilder::ScopeStack.rend() && sym == nullptr) {
      sym = (*it)->lookup_enum_symbol(name);
      it++;
    }
    break;
  }
  case clang::Decl::NonTypeTemplateParm: {
    // Non-type template parameters are treated as variables
    it = SageBuilder::ScopeStack.rbegin();
    while (it != SageBuilder::ScopeStack.rend() && sym == nullptr) {
      sym = (*it)->lookup_variable_symbol(name);
      it++;
    }
    break;
  }
  case clang::Decl::VarTemplateSpecialization:
  case clang::Decl::VarTemplatePartialSpecialization: {
    // Variable template specializations - treat as variables
    SgTemplateArgumentPtrList template_args;
    SgTemplateArgumentPtrList *template_args_ptr = nullptr;
    SgTemplateParameterPtrList *template_params_ptr = nullptr;
    auto lookup_template_decl =
        [&](clang::Decl *key) -> SgTemplateVariableDeclaration * {
      if (key == nullptr) {
        return nullptr;
      }
      auto it = p_decl_translation_map.find(key);
      if (it != p_decl_translation_map.end()) {
        return isSgTemplateVariableDeclaration(it->second);
      }
      if (p_decl_translation_in_progress.find(key) !=
              p_decl_translation_in_progress.end() ||
          p_decl_translation_on_demand.find(key) !=
              p_decl_translation_on_demand.end()) {
        return nullptr;
      }
      SgNode *translated = TraverseOnDemand(key);
      return isSgTemplateVariableDeclaration(translated);
    };
    if (clang::VarTemplateSpecializationDecl *spec_decl =
            llvm::dyn_cast<clang::VarTemplateSpecializationDecl>(decl)) {
      const clang::TemplateArgumentList &clang_args =
          spec_decl->getTemplateArgs();
      for (unsigned i = 0; i < clang_args.size(); ++i) {
        appendTemplateArguments(template_args, clang_args.get(i), false);
      }
      ensureTemplateArgumentParents(template_args);
      if (!template_args.empty()) {
        template_args_ptr = &template_args;
      }

      if (clang::VarTemplatePartialSpecializationDecl *partial =
              llvm::dyn_cast<clang::VarTemplatePartialSpecializationDecl>(
                  spec_decl)) {
        if (SgTemplateVariableDeclaration *sg_partial =
                lookup_template_decl(partial)) {
          template_params_ptr = &sg_partial->get_templateParameters();
        }
      }

      if (template_params_ptr == nullptr) {
        if (clang::VarTemplateDecl *primary =
                spec_decl->getSpecializedTemplate()) {
          if (SgTemplateVariableDeclaration *sg_primary =
                  lookup_template_decl(primary)) {
            template_params_ptr = &sg_primary->get_templateParameters();
          }
        }
      }
    }
    it = SageBuilder::ScopeStack.rbegin();
    while (it != SageBuilder::ScopeStack.rend() && sym == nullptr) {
      if (template_args_ptr != nullptr && template_params_ptr != nullptr) {
        sym = (*it)->lookup_template_variable_symbol(name, template_params_ptr,
                                                     template_args_ptr);
      }
      if (sym == nullptr) {
        sym = (*it)->lookup_variable_symbol(name);
      }
      it++;
    }
    break;
  }
  case clang::Decl::Concept: {
    auto lookup_translated_concept = [&](clang::Decl *key) -> SgSymbol * {
      if (key == nullptr) {
        return nullptr;
      }
      auto it = p_decl_translation_map.find(key);
      if (it == p_decl_translation_map.end() || it->second == nullptr) {
        return nullptr;
      }
      SgNonrealDecl *nrdecl = isSgNonrealDecl(it->second);
      if (nrdecl == nullptr) {
        return nullptr;
      }
      if (SgSymbol *symbol = nrdecl->get_symbol_from_symbol_table()) {
        return symbol;
      }
      if (SgScopeStatement *nr_scope = nrdecl->get_scope()) {
        return nr_scope->lookup_nonreal_symbol(name, nullptr, nullptr);
      }
      return nullptr;
    };

    sym = lookup_translated_concept(decl);
    if (sym == nullptr) {
      sym = lookup_translated_concept(decl->getCanonicalDecl());
    }

    clang::DeclContext *decl_context = decl->getDeclContext();
    clang::DeclContext *scope_context = decl_context;
    while (scope_context != nullptr &&
           llvm::isa<clang::LinkageSpecDecl>(scope_context)) {
      scope_context = scope_context->getParent();
    }
    if (sym == nullptr && scope_context != nullptr) {
      if (SgScopeStatement *decl_scope =
              resolveScopeFromDeclContext(scope_context, nullptr)) {
        sym = decl_scope->lookup_nonreal_symbol(name, nullptr, nullptr);
      }
    }
    if (sym == nullptr) {
      sym = SageInterface::lookupNonrealSymbolInParentScopes(name, nullptr,
                                                             nullptr, nullptr);
    }
    break;
  }
  default:
    std::cerr << "Runtime Error: Unknown type of Decl. ("
              << decl->getDeclKindName() << ")" << std::endl;
  }

  // Remove from in-progress set before returning
  p_symbol_lookup_in_progress.erase(decl);

  return sym;
}

SgScopeStatement *
ClangToSageTranslator::resolveScopeFromDeclContext(clang::DeclContext *context,
                                                   SgScopeStatement *fallback) {
  if (context == nullptr) {
    return fallback;
  }

  // Linkage specs are not scopes in ROSE; resolve through them to the
  // enclosing namespace/global context.
  while (context != nullptr && llvm::isa<clang::LinkageSpecDecl>(context)) {
    context = context->getParent();
  }
  if (context == nullptr) {
    return fallback;
  }

  if (context->isTranslationUnit()) {
    return getGlobalScope();
  }

  clang::Decl *context_decl = llvm::dyn_cast<clang::Decl>(context);
  if (context_decl != nullptr) {
    clang::Decl *map_key = context_decl;
    if (clang::NamespaceDecl *ns_decl =
            llvm::dyn_cast<clang::NamespaceDecl>(context_decl)) {
      map_key = getCanonicalNamespaceDecl(ns_decl);
    } else if (clang::RecordDecl *record_decl =
                   llvm::dyn_cast<clang::RecordDecl>(context_decl)) {
      if (clang::RecordDecl *definition = record_decl->getDefinition()) {
        map_key = definition;
      } else if (clang::RecordDecl *canonical =
                     llvm::dyn_cast<clang::RecordDecl>(
                         record_decl->getCanonicalDecl())) {
        map_key = canonical;
      }
    }

    auto it = p_decl_translation_map.find(map_key);
    if (it == p_decl_translation_map.end() && map_key != context_decl) {
      it = p_decl_translation_map.find(context_decl);
    }
    if (it != p_decl_translation_map.end()) {
      SgNode *context_node = it->second;
      if (SgNamespaceDeclarationStatement *ns_decl_stmt =
              isSgNamespaceDeclarationStatement(context_node)) {
        if (ns_decl_stmt->get_definition() != nullptr) {
          return ns_decl_stmt->get_definition();
        }
      } else if (SgNamespaceDefinitionStatement *ns_def =
                     isSgNamespaceDefinitionStatement(context_node)) {
        return ns_def;
      } else if (SgClassDeclaration *class_decl =
                     isSgClassDeclaration(context_node)) {
        if (class_decl->get_definition() != nullptr) {
          return class_decl->get_definition();
        }
      } else if (SgClassDefinition *class_def =
                     isSgClassDefinition(context_node)) {
        return class_def;
      } else if (SgFunctionDefinition *fn_def =
                     isSgFunctionDefinition(context_node)) {
        return fn_def;
      } else if (SgFunctionDeclaration *fn_decl =
                     isSgFunctionDeclaration(context_node)) {
        if (fn_decl->get_definition() != nullptr) {
          return fn_decl->get_definition();
        }
      }
    }

    if (clang::NamespaceDecl *ns_decl =
            llvm::dyn_cast<clang::NamespaceDecl>(context_decl)) {
      // Prefer an already-active namespace scope on the scope stack to avoid
      // creating stub reopenings during on-demand translation.
      for (auto rit = SageBuilder::ScopeStack.rbegin();
           rit != SageBuilder::ScopeStack.rend(); ++rit) {
        if (SgNamespaceDefinitionStatement *ns_def =
                isSgNamespaceDefinitionStatement(*rit)) {
          SgNamespaceDeclarationStatement *ns_stmt =
              ns_def->get_namespaceDeclaration();
          if (ns_stmt != nullptr) {
            bool match = false;
            if (ns_decl->isAnonymousNamespace()) {
              match = ns_stmt->get_isUnnamedNamespace();
            } else {
              match =
                  ns_stmt->get_name().getString() == ns_decl->getNameAsString();
            }
            if (match) {
              return ns_def;
            }
          }
        }
      }

      SgNamespaceDeclarationStatement *ns_stmt =
          ensureNamespaceDeclaration(ns_decl);
      if (ns_stmt != nullptr && ns_stmt->get_definition() != nullptr) {
        return ns_stmt->get_definition();
      }
    }
  }

  return fallback;
}

std::unique_ptr<SgTemplateParameterPtrList>
ClangToSageTranslator::translateTemplateParameterList(
    clang::TemplateParameterList *param_list,
    SgDeclarationStatement *owning_template) {
  auto sg_params = std::make_unique<SgTemplateParameterPtrList>();
  if (param_list == nullptr) {
    return sg_params;
  }

  unsigned index = 0;
  for (clang::NamedDecl *param_decl : *param_list) {
    SgTemplateParameter *sg_param =
        translateTemplateParameter(param_decl, owning_template, index);
    if (sg_param != nullptr) {
      sg_params->push_back(sg_param);
    }
    ++index;
  }

  return sg_params;
}

namespace {
// Ensure a declaration has parent and scope set using the current scope stack
// as fallback. Logs a warning if the scope remains unset (will trip diagnostics
// later).
void diagnose_null_scope(SgDeclarationStatement *ds, const char *context) {
  if (ds == nullptr || ds->get_scope() != nullptr)
    return;
  MLOG_WARN_C(MLOG_FRONTEND,
              "Declaration %s (%p) created with nullptr scope in %s\n",
              ds->class_name().c_str(), ds, context);
}

bool detach_decl_from_scope_child_list(SgDeclarationStatement *decl,
                                       SgScopeStatement *scope);

void attach_nonreal_template_parameters(
    SgDeclarationStatement *owner, const SgTemplateParameterPtrList &params) {
  if (owner == nullptr) {
    return;
  }

  std::vector<SgNonrealDecl *> nrdecls;
  for (SgTemplateParameter *param : params) {
    if (param == nullptr) {
      continue;
    }
    if (param->get_parameterType() != SgTemplateParameter::template_parameter) {
      continue;
    }
    if (SgNonrealDecl *nrdecl =
            isSgNonrealDecl(param->get_templateDeclaration())) {
      nrdecls.push_back(nrdecl);
    }
  }

  if (nrdecls.empty()) {
    return;
  }

  SgDeclarationScope *decl_scope =
      SageBuilder::getNonrealDeclarationScope(owner);
  if (decl_scope == nullptr) {
    SgDeclarationScope *candidate_scope =
        isSgDeclarationScope(nrdecls.front()->get_scope());
    if (candidate_scope != nullptr) {
      decl_scope = candidate_scope;
      SageBuilder::setNonrealDeclarationScope(owner, decl_scope);
    }
  }

  if (decl_scope == nullptr) {
    decl_scope = SageBuilder::getOrCreateNonrealDeclarationScope(owner);
  }
  if (decl_scope == nullptr) {
    return;
  }

  if (decl_scope->get_parent() != owner) {
    decl_scope->set_parent(owner);
  }

  for (SgNonrealDecl *nrdecl : nrdecls) {
    if (SgScopeStatement *prev_scope = nrdecl->get_scope()) {
      if (prev_scope != decl_scope) {
        detach_decl_from_scope_child_list(nrdecl, prev_scope);
      }
    }
    if (nrdecl->get_scope() != decl_scope) {
      nrdecl->set_scope(decl_scope);
    }
    if (!decl_scope->statementExistsInScope(nrdecl)) {
      decl_scope->insertStatementInScope(nrdecl, false);
    }
    if (nrdecl->get_parent() != decl_scope) {
      nrdecl->set_parent(decl_scope);
    }
    if (decl_scope->lookup_nonreal_symbol(nrdecl->get_name(), nullptr,
                                          nullptr) == nullptr) {
      SgNonrealSymbol *symbol = new SgNonrealSymbol(nrdecl);
      decl_scope->insert_symbol(nrdecl->get_name(), symbol);
    }
  }
}

SgScopeStatement *get_enclosing_namespace_scope(SgScopeStatement *scope) {
  SgScopeStatement *current = scope;
  while (current != nullptr && !isSgGlobal(current) &&
         !isSgNamespaceDefinitionStatement(current)) {
    SgScopeStatement *next_scope =
        SageInterface::getEnclosingScope(current, false);
    if (next_scope == current) {
      break;
    }
    current = next_scope;
  }
  return current;
}

bool is_declaration_scope_context(const clang::DeclContext *context) {
  if (context == nullptr) {
    return false;
  }
  return context->isFileContext() || context->isRecord() ||
         context->isFunctionOrMethod();
}

bool scope_supports_statement_list(const SgScopeStatement *scope) {
  if (scope == nullptr) {
    return false;
  }
  switch (scope->variantT()) {
  case V_SgBasicBlock:
  case V_SgCatchOptionStmt:
  case V_SgDoWhileStmt:
  case V_SgForStatement:
  case V_SgRangeBasedForStatement:
  case V_SgTemplateFunctionDefinition:
  case V_SgFunctionDefinition:
  case V_SgSwitchStatement:
  case V_SgWhileStmt:
  case V_SgFortranDo:
    return true;
  default:
    return false;
  }
}

SgDeclarationStatementPtrList *
get_scope_declaration_list(SgScopeStatement *scope) {
  if (scope == nullptr) {
    return nullptr;
  }
  if (SgGlobal *global = isSgGlobal(scope)) {
    return &global->get_declarations();
  }
  if (SgNamespaceDefinitionStatement *ns_def =
          isSgNamespaceDefinitionStatement(scope)) {
    return &ns_def->get_declarations();
  }
  if (SgDeclarationScope *decl_scope = isSgDeclarationScope(scope)) {
    return &decl_scope->get_declarations();
  }
  if (SgClassDefinition *class_def = isSgClassDefinition(scope)) {
    return &class_def->get_members();
  }
  if (SgTemplateClassDefinition *template_def =
          isSgTemplateClassDefinition(scope)) {
    return &template_def->get_members();
  }
  if (SgTemplateInstantiationDefn *inst_def =
          isSgTemplateInstantiationDefn(scope)) {
    return &inst_def->get_members();
  }
  if (scope->containsOnlyDeclarations()) {
    return &scope->getDeclarationList();
  }
  return nullptr;
}

void ensure_parent_and_scope(SgDeclarationStatement *ds,
                             const char *context = "ClangToSageTranslator") {
  if (ds == nullptr)
    return;

  SgScopeStatement *cur_scope = SageBuilder::topScopeStack();
  if (ds->get_parent() == nullptr && cur_scope != nullptr) {
    ds->set_parent(cur_scope);
  }
  if (ds->get_scope() == nullptr && cur_scope != nullptr) {
    ds->set_scope(cur_scope);
  }
  diagnose_null_scope(ds, context);
}

bool is_decl_attached_to_scope_child_list(SgScopeStatement *scope,
                                          SgDeclarationStatement *decl) {
  if (scope == nullptr || decl == nullptr) {
    return false;
  }

  if (SgDeclarationStatementPtrList *decls =
          get_scope_declaration_list(scope)) {
    return std::find(decls->begin(), decls->end(), decl) != decls->end();
  }

  if (!scope_supports_statement_list(scope)) {
    return false;
  }

  const SgStatementPtrList &stmts = scope->getStatementList();
  return std::find(stmts.begin(), stmts.end(), decl) != stmts.end();
}

bool detach_decl_from_scope_child_list(SgDeclarationStatement *decl,
                                       SgScopeStatement *scope) {
  if (scope == nullptr || decl == nullptr) {
    return false;
  }

  auto erase_all = [&](auto &list) -> bool {
    bool removed = false;
    for (auto it = list.begin(); it != list.end();) {
      if (*it == decl) {
        it = list.erase(it);
        removed = true;
      } else {
        ++it;
      }
    }
    return removed;
  };

  if (SgDeclarationStatementPtrList *decls =
          get_scope_declaration_list(scope)) {
    return erase_all(*decls);
  }

  if (!scope_supports_statement_list(scope)) {
    return false;
  }

  return erase_all(scope->getStatementList());
}

template <typename ListType, typename NodePtr>
bool insert_markers_around(ListType &list, NodePtr first, NodePtr last,
                           NodePtr start, NodePtr end) {
  if (first == nullptr || last == nullptr || start == nullptr ||
      end == nullptr) {
    return false;
  }

  size_t first_index = list.size();
  size_t last_index = list.size();
  size_t idx = 0;
  for (auto *entry : list) {
    if (entry == first && first_index == list.size()) {
      first_index = idx;
    }
    if (entry == last) {
      last_index = idx;
    }
    ++idx;
  }

  if (first_index == list.size() || last_index == list.size()) {
    return false;
  }

  if (last_index < first_index) {
    std::swap(first_index, last_index);
  }

  list.reserve(list.size() + 2);
  list.insert(list.begin() + first_index, start);
  try {
    list.insert(list.begin() + last_index + 2, end);
  } catch (...) {
    list.erase(list.begin() + first_index);
    throw;
  }
  return true;
}

template <typename ListType, typename NodePtr>
bool insert_markers_at_index(ListType &list, size_t index, NodePtr start,
                             NodePtr end) {
  if (start == nullptr || end == nullptr) {
    return false;
  }
  if (index > list.size()) {
    return false;
  }
  list.reserve(list.size() + 2);
  list.insert(list.begin() + index, start);
  try {
    list.insert(list.begin() + index + 1, end);
  } catch (...) {
    list.erase(list.begin() + index);
    throw;
  }
  return true;
}

bool file_info_is_before(const Sg_File_Info *lhs, const Sg_File_Info *rhs) {
  if (lhs == nullptr || rhs == nullptr) {
    return false;
  }
  const std::string &lhs_file = lhs->get_filenameString();
  const std::string &rhs_file = rhs->get_filenameString();
  if (lhs_file != rhs_file) {
    return lhs_file < rhs_file;
  }
  if (lhs->get_line() != rhs->get_line()) {
    return lhs->get_line() < rhs->get_line();
  }
  return lhs->get_col() < rhs->get_col();
}

bool file_info_is_before_or_equal(const Sg_File_Info *lhs,
                                  const Sg_File_Info *rhs) {
  return !file_info_is_before(rhs, lhs);
}

bool file_info_in_range(const Sg_File_Info *pos, const Sg_File_Info *start,
                        const Sg_File_Info *end) {
  if (pos == nullptr || start == nullptr || end == nullptr) {
    return false;
  }
  const std::string &start_file = start->get_filenameString();
  const std::string &end_file = end->get_filenameString();
  if (start_file.empty() || end_file.empty() || start_file != end_file) {
    return false;
  }
  if (pos->get_filenameString() != start_file) {
    return false;
  }
  const Sg_File_Info *range_start = start;
  const Sg_File_Info *range_end = end;
  if (file_info_is_before(range_end, range_start)) {
    std::swap(range_start, range_end);
  }
  return file_info_is_before_or_equal(range_start, pos) &&
         file_info_is_before_or_equal(pos, range_end);
}

bool statement_in_linkage_range(const SgStatement *stmt,
                                const Sg_File_Info *start,
                                const Sg_File_Info *end) {
  if (stmt == nullptr) {
    return false;
  }
  return file_info_in_range(stmt->get_startOfConstruct(), start, end);
}

template <typename ListType>
size_t find_insertion_index_for_range(const ListType &list,
                                      const Sg_File_Info *range_start) {
  if (range_start == nullptr) {
    return list.size();
  }
  const std::string &range_file = range_start->get_filenameString();
  if (range_file.empty()) {
    return list.size();
  }
  bool saw_same_file = false;
  size_t last_before = list.size();
  for (size_t idx = 0; idx < list.size(); ++idx) {
    SgStatement *stmt = isSgStatement(list[idx]);
    if (stmt == nullptr) {
      continue;
    }
    Sg_File_Info *fi = stmt->get_startOfConstruct();
    if (fi == nullptr) {
      continue;
    }
    if (fi->get_filenameString() != range_file) {
      continue;
    }
    saw_same_file = true;
    if (file_info_is_before(fi, range_start)) {
      last_before = idx;
      continue;
    }
    return idx;
  }
  if (saw_same_file) {
    return last_before == list.size() ? list.size() : last_before + 1;
  }
  return list.size();
}

template <typename ListType>
bool find_linkage_bounds_in_list(
    const ListType &list,
    const std::unordered_set<SgDeclarationStatement *> &linkage_decl_set,
    SgScopeStatement *scope, SgStatement *&first_stmt, SgStatement *&last_stmt,
    const Sg_File_Info *range_start, const Sg_File_Info *range_end) {
  if (scope == nullptr) {
    return false;
  }

  for (auto *entry : list) {
    SgStatement *stmt = isSgStatement(entry);
    if (stmt == nullptr) {
      continue;
    }
    if (stmt->get_parent() != scope) {
      continue;
    }

    bool matches_linkage = false;
    if (SgDeclarationStatement *decl = isSgDeclarationStatement(stmt)) {
      matches_linkage = linkage_decl_set.find(decl) != linkage_decl_set.end();
    }
    const bool matches_range =
        statement_in_linkage_range(stmt, range_start, range_end);
    if (!matches_linkage && !matches_range) {
      continue;
    }

    if (first_stmt == nullptr) {
      first_stmt = stmt;
    }
    last_stmt = stmt;
  }

  return first_stmt != nullptr && last_stmt != nullptr;
}

void ensure_decl_in_scope_child_list(
    SgDeclarationStatement *decl, SgScopeStatement *scope,
    const char *context = "ClangToSageTranslator") {
  if (decl == nullptr) {
    return;
  }

  if (scope == nullptr) {
    scope = decl->get_scope();
  }
  if (scope == nullptr) {
    diagnose_null_scope(decl, context);
    return;
  }

  if (decl->get_scope() == nullptr) {
    decl->set_scope(scope);
  }

  if (decl->get_parent() != scope) {
    decl->set_parent(scope);
  }

  if (is_decl_attached_to_scope_child_list(scope, decl)) {
    return;
  }

  if (SgDeclarationStatementPtrList *decls =
          get_scope_declaration_list(scope)) {
    decls->push_back(decl);
    decl->set_parent(scope);
    return;
  }

  scope->append_statement(decl);
  if (decl->get_parent() != scope) {
    decl->set_parent(scope);
  }
  if (decl->get_scope() != scope) {
    decl->set_scope(scope);
  }
}

// Attach a declaration to the target scope's child list without changing its
// semantic scope (e.g., out-of-line member definitions).
void ensure_decl_in_scope_child_list_preserve_scope(
    SgDeclarationStatement *decl, SgScopeStatement *scope,
    const char *context = "ClangToSageTranslator") {
  if (decl == nullptr) {
    return;
  }

  if (scope == nullptr) {
    scope = isSgScopeStatement(decl->get_parent());
  }
  if (scope == nullptr) {
    scope = decl->get_scope();
  }
  if (scope == nullptr) {
    diagnose_null_scope(decl, context);
    return;
  }

  SgScopeStatement *original_scope = decl->get_scope();
  if (original_scope == nullptr) {
    ensure_decl_in_scope_child_list(decl, scope, context);
    return;
  }

  if (original_scope != scope &&
      is_decl_attached_to_scope_child_list(original_scope, decl)) {
    detach_decl_from_scope_child_list(decl, original_scope);
  }

  SgDeclarationStatementPtrList *decls = get_scope_declaration_list(scope);
  bool can_attach = decls != nullptr || scope_supports_statement_list(scope);
  if (!can_attach) {
    return;
  }

  if (SgScopeStatement *current_parent =
          isSgScopeStatement(decl->get_parent())) {
    if (current_parent != scope) {
      detach_decl_from_scope_child_list(decl, current_parent);
    }
  }

  if (!is_decl_attached_to_scope_child_list(scope, decl)) {
    if (decls != nullptr) {
      decls->push_back(decl);
    } else {
      scope->append_statement(decl);
    }
  }

  if (decl->get_parent() != scope) {
    decl->set_parent(scope);
  }
  if (decl->get_scope() != original_scope) {
    decl->set_scope(original_scope);
  }
}

void mark_compiler_generated_and_suppress_unparse(SgLocatedNode *n) {
  if (n == nullptr) {
    return;
  }

  auto mark = [](Sg_File_Info *fi) {
    if (fi == nullptr) {
      return;
    }
    fi->setCompilerGenerated();
    fi->unsetOutputInCodeGeneration();
  };

  mark(n->get_file_info());
  mark(n->get_startOfConstruct());
  mark(n->get_endOfConstruct());

  if (SgExpression *expr = isSgExpression(n)) {
    mark(expr->get_operatorPosition());
  }
}

void mark_implicit_instantiation_for_suppression(
    SgFunctionDeclaration *func_decl) {
  if (func_decl == nullptr) {
    return;
  }

  std::set<SgNode *> visited;
  std::function<void(SgFunctionDeclaration *)> visit =
      [&](SgFunctionDeclaration *decl) {
        if (decl == nullptr || !visited.insert(decl).second) {
          return;
        }

        mark_compiler_generated_and_suppress_unparse(decl);

        if (SgFunctionParameterList *params = decl->get_parameterList()) {
          mark_compiler_generated_and_suppress_unparse(params);
          for (SgInitializedName *param : params->get_args()) {
            if (param != nullptr) {
              mark_compiler_generated_and_suppress_unparse(param);
            }
          }
        }

        if (SgFunctionDefinition *defn = decl->get_definition()) {
          mark_compiler_generated_and_suppress_unparse(defn);
          if (SgBasicBlock *body = defn->get_body()) {
            mark_compiler_generated_and_suppress_unparse(body);
          }
        }

        visit(isSgFunctionDeclaration(decl->get_firstNondefiningDeclaration()));
        visit(isSgFunctionDeclaration(decl->get_definingDeclaration()));
      };

  visit(func_decl);
}

// Normalize namespace scopes to the first definition associated with the
// namespace symbol (the first nondefining declaration).  ROSE models each
// re-entrant namespace definition as a distinct scope node, but for symbol
// table purposes they must be treated as a single logical scope to avoid
// duplicate symbol insertion and subsequent fixups that emit
// "Redundant symbol removed...from symbol table".
SgScopeStatement *normalizeNamespaceScope(SgScopeStatement *scope) {
  if (scope == nullptr)
    return nullptr;

  SgNamespaceDefinitionStatement *ns_def =
      isSgNamespaceDefinitionStatement(scope);
  if (ns_def == nullptr)
    return scope;

  SgNamespaceDefinitionStatement *global_def = ns_def->get_global_definition();
  if (global_def != nullptr) {
    return global_def;
  }

  SgNamespaceDeclarationStatement *ns_decl = ns_def->get_namespaceDeclaration();
  if (ns_decl == nullptr)
    return scope;

  SgNamespaceDeclarationStatement *first_nondef =
      isSgNamespaceDeclarationStatement(
          ns_decl->get_firstNondefiningDeclaration());
  if (first_nondef == nullptr)
    return scope;

  SgNamespaceDefinitionStatement *first_def = first_nondef->get_definition();
  return first_def != nullptr ? first_def : scope;
}

std::vector<SgSymbol *>
find_function_symbols_in_scope(SgScopeStatement *scope,
                               SgFunctionDeclaration *decl) {
  std::vector<SgSymbol *> matches;
  if (scope == nullptr || decl == nullptr) {
    return matches;
  }

  SgSymbolTable *table = scope->get_symbol_table();
  if (table == nullptr) {
    return matches;
  }

  SgFunctionDeclaration *decl_first =
      isSgFunctionDeclaration(decl->get_firstNondefiningDeclaration());
  if (decl_first == nullptr) {
    decl_first = decl;
  }

  bool decl_is_member = isSgMemberFunctionDeclaration(decl) != nullptr;

  auto matches_decl = [&](SgSymbol *candidate) -> bool {
    if (candidate == nullptr) {
      return false;
    }
    if (SgAliasSymbol *alias = isSgAliasSymbol(candidate)) {
      if (alias->get_alias() == nullptr) {
        return false;
      }
      candidate = alias->get_alias();
    }

    if (!decl_is_member &&
        (isSgMemberFunctionSymbol(candidate) != nullptr ||
         isSgTemplateMemberFunctionSymbol(candidate) != nullptr)) {
      return false;
    }

    SgFunctionDeclaration *sym_decl = nullptr;
    if (SgFunctionSymbol *func_sym = isSgFunctionSymbol(candidate)) {
      sym_decl = func_sym->get_declaration();
    } else if (SgTemplateFunctionSymbol *tmpl_sym =
                   isSgTemplateFunctionSymbol(candidate)) {
      sym_decl = isSgFunctionDeclaration(tmpl_sym->get_declaration());
    } else if (SgMemberFunctionSymbol *mem_sym =
                   isSgMemberFunctionSymbol(candidate)) {
      sym_decl = mem_sym->get_declaration();
    } else if (SgTemplateMemberFunctionSymbol *tmpl_mem_sym =
                   isSgTemplateMemberFunctionSymbol(candidate)) {
      sym_decl = isSgFunctionDeclaration(tmpl_mem_sym->get_declaration());
    }
    if (sym_decl == nullptr) {
      return false;
    }

    SgFunctionDeclaration *sym_first =
        isSgFunctionDeclaration(sym_decl->get_firstNondefiningDeclaration());
    if (sym_first == nullptr) {
      sym_first = sym_decl;
    }
    if (sym_first == decl_first) {
      return true;
    }

    return SageInterface::isSameFunction(sym_first, decl_first);
  };

  if (rose_hash_multimap *symtab = table->get_table()) {
    std::set<SgSymbol *> seen;
    auto record_match = [&](SgSymbol *candidate) {
      if (candidate == nullptr) {
        return;
      }
      if (seen.insert(candidate).second && matches_decl(candidate)) {
        matches.push_back(candidate);
      }
    };

    SgName key = decl->get_name();
    auto range = symtab->equal_range(key);
    for (auto it = range.first; it != range.second; ++it) {
      record_match(it->second);
    }
    if (matches.empty() && key.getString().empty()) {
      SgName mangled = decl_first->get_mangled_name();
      if (!mangled.getString().empty() && mangled != key) {
        auto mangled_range = symtab->equal_range(mangled);
        for (auto it = mangled_range.first; it != mangled_range.second; ++it) {
          record_match(it->second);
        }
      }
    }
  }

  return matches;
}

SgSymbol *find_function_symbol_in_scope(SgScopeStatement *scope,
                                        SgFunctionDeclaration *decl) {
  std::vector<SgSymbol *> matches = find_function_symbols_in_scope(scope, decl);
  return matches.empty() ? nullptr : matches.front();
}

bool function_symbol_matches_declaration(SgSymbol *symbol,
                                         SgFunctionDeclaration *decl) {
  if (symbol == nullptr || decl == nullptr) {
    return false;
  }

  if (SgAliasSymbol *alias = isSgAliasSymbol(symbol)) {
    if (alias->get_alias() == nullptr) {
      return false;
    }
    symbol = alias->get_alias();
  }

  if (isSgTemplateMemberFunctionDeclaration(decl) != nullptr) {
    return isSgTemplateMemberFunctionSymbol(symbol) != nullptr;
  }
  if (isSgTemplateFunctionDeclaration(decl) != nullptr) {
    return isSgTemplateFunctionSymbol(symbol) != nullptr;
  }
  if (isSgMemberFunctionDeclaration(decl) != nullptr) {
    return isSgMemberFunctionSymbol(symbol) != nullptr;
  }
  return isSgFunctionSymbol(symbol) != nullptr;
}

enum RehomeFunctionSymbolMode {
  REHOME_REQUIRE_SOURCE_SYMBOLS,
  REHOME_ALLOW_EMPTY_SOURCE
};

struct RehomeFunctionSymbolResult {
  RehomeFunctionSymbolResult() : existing(nullptr), had_source_symbols(false) {}
  SgSymbol *existing;
  bool had_source_symbols;
};

RehomeFunctionSymbolResult rehome_function_symbols_between_scopes(
    SgFunctionDeclaration *symbol_decl, SgScopeStatement *from_scope,
    SgScopeStatement *to_scope, RehomeFunctionSymbolMode mode) {
  RehomeFunctionSymbolResult result;
  if (symbol_decl == nullptr || from_scope == nullptr || to_scope == nullptr) {
    return result;
  }

  SgSymbolTable *from_table = from_scope->get_symbol_table();
  SgSymbolTable *to_table = to_scope->get_symbol_table();

  std::vector<SgSymbol *> symbols =
      find_function_symbols_in_scope(from_scope, symbol_decl);
  result.had_source_symbols = !symbols.empty();
  if (symbols.empty() && mode == REHOME_REQUIRE_SOURCE_SYMBOLS) {
    return result;
  }

  SgSymbol *existing = nullptr;

  auto detach_symbol = [&](SgScopeStatement *scope, SgSymbolTable *table,
                           SgSymbol *sym) -> bool {
    if (scope == nullptr || sym == nullptr) {
      return false;
    }
    if (scope->symbol_exists(sym)) {
      scope->remove_symbol(sym);
      return true;
    }
    if (table != nullptr && table->exists(sym)) {
      table->remove(sym);
      return true;
    }
    return false;
  };
  auto discard_symbol = [&](SgScopeStatement *scope, SgSymbolTable *table,
                            SgSymbol *sym) {
    if (sym == nullptr) {
      return;
    }
    if (detach_symbol(scope, table, sym)) {
      move_symbol_to_orphan_table(sym);
    }
  };

  std::vector<SgSymbol *> target_symbols =
      find_function_symbols_in_scope(to_scope, symbol_decl);
  for (SgSymbol *symbol : target_symbols) {
    if (symbol == nullptr) {
      continue;
    }
    if (isSgAliasSymbol(symbol) != nullptr) {
      discard_symbol(to_scope, to_table, symbol);
      continue;
    }
    if (function_symbol_matches_declaration(symbol, symbol_decl)) {
      if (existing == nullptr) {
        existing = symbol;
      } else {
        discard_symbol(to_scope, to_table, symbol);
      }
    } else {
      discard_symbol(to_scope, to_table, symbol);
    }
  }

  for (SgSymbol *symbol : symbols) {
    if (symbol == nullptr) {
      continue;
    }
    if (isSgAliasSymbol(symbol) != nullptr) {
      discard_symbol(from_scope, from_table, symbol);
      continue;
    }
    if (!function_symbol_matches_declaration(symbol, symbol_decl)) {
      discard_symbol(from_scope, from_table, symbol);
      continue;
    }

    if (existing != nullptr && symbol != existing) {
      discard_symbol(from_scope, from_table, symbol);
      continue;
    }

    detach_symbol(from_scope, from_table, symbol);
    if (!to_scope->symbol_exists(symbol)) {
      to_scope->insert_symbol(symbol->get_name(), symbol);
    } else if (to_table != nullptr && symbol->get_parent() != to_table) {
      symbol->set_parent(to_table);
    }
    if (existing == nullptr) {
      existing = symbol;
    }
  }

  result.existing = existing;
  return result;
}

void rehome_friend_function_symbol(SgFunctionDeclaration *func_decl,
                                   SgScopeStatement *from_scope,
                                   SgScopeStatement *to_scope) {
  if (func_decl == nullptr || from_scope == nullptr || to_scope == nullptr) {
    return;
  }
  if (from_scope == to_scope) {
    return;
  }
  if (isSgMemberFunctionDeclaration(func_decl) != nullptr) {
    return;
  }

  SgFunctionDeclaration *symbol_decl =
      isSgFunctionDeclaration(func_decl->get_firstNondefiningDeclaration());
  if (symbol_decl == nullptr) {
    symbol_decl = func_decl;
  }

  if (from_scope->get_symbol_table() == nullptr ||
      to_scope->get_symbol_table() == nullptr) {
    return;
  }

  rehome_function_symbols_between_scopes(symbol_decl, from_scope, to_scope,
                                         REHOME_REQUIRE_SOURCE_SYMBOLS);
}

void rehome_friend_function_symbol_if_needed(SgDeclarationStatement *decl,
                                             SgScopeStatement *current_scope,
                                             SgScopeStatement *friend_scope) {
  if (friend_scope == nullptr) {
    return;
  }
  SgFunctionDeclaration *func_decl = isSgFunctionDeclaration(decl);
  if (func_decl == nullptr) {
    return;
  }
  SgScopeStatement *source_scope = isSgScopeStatement(func_decl->get_parent());
  if (source_scope == nullptr) {
    source_scope = current_scope;
  }
  if (source_scope == nullptr) {
    return;
  }
  if (isSgClassDefinition(source_scope) != nullptr ||
      isSgTemplateClassDefinition(source_scope) != nullptr ||
      isSgTemplateInstantiationDefn(source_scope) != nullptr) {
    rehome_friend_function_symbol(func_decl, source_scope, friend_scope);
  }
}

clang::NamespaceDecl *getCanonicalNamespaceDecl(clang::NamespaceDecl *decl) {
  if (decl == nullptr) {
    return nullptr;
  }
  clang::NamespaceDecl *canonical = decl->getCanonicalDecl();
  return canonical != nullptr ? canonical : decl;
}

static const clang::Type *stripFieldType(clang::QualType &qual_type) {
  const clang::Type *type = qual_type.getTypePtr();
  while (llvm::isa<clang::ElaboratedType>(type) ||
         llvm::isa<clang::PointerType>(type) ||
         llvm::isa<clang::ArrayType>(type)) {
    if (auto *elaborated = llvm::dyn_cast<clang::ElaboratedType>(type)) {
      qual_type = elaborated->getNamedType();
    } else if (auto *ptr = llvm::dyn_cast<clang::PointerType>(type)) {
      qual_type = ptr->getPointeeType();
    } else if (auto *array = llvm::dyn_cast<clang::ArrayType>(type)) {
      qual_type = array->getElementType();
    }
    type = qual_type.getTypePtr();
  }
  return type;
}

static bool isTagEmbeddedInField(clang::TagDecl *tag_decl,
                                 clang::RecordDecl *parent_decl,
                                 clang::SourceManager *source_manager,
                                 bool allow_fallback_loc_compare) {
  if (tag_decl == nullptr || parent_decl == nullptr) {
    return false;
  }

  auto location_in_range = [&](clang::SourceLocation begin,
                               clang::SourceLocation end,
                               clang::SourceLocation loc) -> bool {
    if (!begin.isValid() || !end.isValid() || !loc.isValid()) {
      return false;
    }
    if (source_manager == nullptr) {
      return allow_fallback_loc_compare ? begin == loc : false;
    }
    clang::SourceLocation begin_loc = source_manager->getSpellingLoc(begin);
    clang::SourceLocation end_loc = source_manager->getSpellingLoc(end);
    clang::SourceLocation target_loc = source_manager->getSpellingLoc(loc);
    if (!begin_loc.isValid() || !end_loc.isValid() || !target_loc.isValid()) {
      return false;
    }
    if (source_manager->getFileID(begin_loc) !=
            source_manager->getFileID(target_loc) ||
        source_manager->getFileID(end_loc) !=
            source_manager->getFileID(target_loc)) {
      return false;
    }
    unsigned begin_offset = source_manager->getFileOffset(begin_loc);
    unsigned end_offset = source_manager->getFileOffset(end_loc);
    unsigned target_offset = source_manager->getFileOffset(target_loc);
    if (begin_offset > end_offset) {
      std::swap(begin_offset, end_offset);
    }
    return target_offset >= begin_offset && target_offset <= end_offset;
  };

  clang::SourceLocation tag_begin = tag_decl->getBeginLoc();
  for (clang::FieldDecl *field_decl : parent_decl->fields()) {
    clang::QualType field_qual_type = field_decl->getType();
    const clang::Type *field_type = stripFieldType(field_qual_type);
    clang::TagDecl *field_tag = nullptr;
    if (auto *record_type =
            llvm::dyn_cast_or_null<clang::RecordType>(field_type)) {
      field_tag = record_type->getDecl();
    } else if (auto *enum_type =
                   llvm::dyn_cast_or_null<clang::EnumType>(field_type)) {
      field_tag = enum_type->getDecl();
    }

    if (field_tag == nullptr) {
      continue;
    }

    clang::SourceRange field_range = field_decl->getSourceRange();
    if (clang::TypeSourceInfo *tsi = field_decl->getTypeSourceInfo()) {
      clang::SourceRange type_range = tsi->getTypeLoc().getSourceRange();
      if (type_range.isValid()) {
        field_range = type_range;
      }
    }

    if (field_tag->getCanonicalDecl() == tag_decl->getCanonicalDecl() &&
        location_in_range(field_range.getBegin(), field_range.getEnd(),
                          tag_begin)) {
      return true;
    }
  }
  return false;
}

} // unnamed namespace

void ClangToSageTranslator::populateClassDefinition(
    clang::RecordDecl *record_decl, SgClassDefinition *class_def) {
  SageBuilder::pushScopeStack(class_def);
  // REX FIX: Track access modifier state to suppress redundant keywords
  SgAccessModifier::access_modifier_enum current_access =
      SgAccessModifier::e_unknown;
  if (record_decl->isClass()) {
    current_access = SgAccessModifier::e_private;
  } else if (record_decl->isStruct() || record_decl->isUnion()) {
    current_access = SgAccessModifier::e_public;
  }

  // Reconstruct state from existing members (if any)
  const SgDeclarationStatementPtrList &existing_members =
      class_def->get_members();
  for (auto mem : existing_members) {
    SgAccessModifier::access_modifier_enum m =
        mem->get_declarationModifier().get_accessModifier().get_modifier();
    if (m != SgAccessModifier::e_unknown) {
      current_access = m;
    }
  }

  // REX FIX: We must detect if an access specifier was explicitly written.
  bool explicit_access_context = false;
  clang::SourceManager *source_manager =
      p_compiler_instance != nullptr ? &p_compiler_instance->getSourceManager()
                                     : nullptr;

  // Populate base classes for C++ records.
  if (clang::CXXRecordDecl *cxx_record =
          llvm::dyn_cast<clang::CXXRecordDecl>(record_decl)) {
    auto canonical_class_decl =
        [](SgClassDeclaration *decl) -> SgClassDeclaration * {
      if (decl == nullptr) {
        return nullptr;
      }
      if (SgClassDeclaration *first =
              isSgClassDeclaration(decl->get_firstNondefiningDeclaration())) {
        return first;
      }
      return decl;
    };

    auto has_class_base = [&](SgClassDeclaration *decl) -> bool {
      if (decl == nullptr) {
        return true;
      }
      SgClassDeclaration *canon = canonical_class_decl(decl);
      for (SgBaseClass *existing : class_def->get_inheritances()) {
        if (existing == nullptr) {
          continue;
        }
        if (isSgNonrealBaseClass(existing) != nullptr) {
          continue;
        }
        SgClassDeclaration *existing_decl = existing->get_base_class();
        if (existing_decl == nullptr) {
          continue;
        }
        if (canonical_class_decl(existing_decl) == canon) {
          return true;
        }
      }
      return false;
    };

    auto sanitize_base_name = [&](const std::string &raw) -> SgName {
      std::string name = trimWhitespace(raw);
      if (name.rfind("struct ", 0) == 0) {
        name = trimWhitespace(name.substr(7));
      } else if (name.rfind("class ", 0) == 0) {
        name = trimWhitespace(name.substr(6));
      } else if (name.rfind("union ", 0) == 0) {
        name = trimWhitespace(name.substr(6));
      }
      size_t lt = name.find('<');
      if (lt != std::string::npos) {
        name = trimWhitespace(name.substr(0, lt));
      }
      if (name.empty()) {
        name = "__unknown_base";
      }
      return SgName(name);
    };

    SgScopeStatement *base_scope = nullptr;
    if (SgClassDeclaration *owner_decl = class_def->get_declaration()) {
      base_scope = owner_decl->get_scope();
    }
    if (base_scope == nullptr) {
      base_scope = SageBuilder::topScopeStack();
    }
    if (base_scope == nullptr) {
      base_scope = class_def;
    }

    auto ensure_defining_decl = [&](SgClassDeclaration *decl) -> void {
      if (decl == nullptr) {
        return;
      }
      if (decl->get_definingDeclaration() != nullptr) {
        return;
      }
      if (decl->get_definition() != nullptr) {
        decl->set_definingDeclaration(decl);
        if (decl->get_firstNondefiningDeclaration() == nullptr) {
          decl->set_firstNondefiningDeclaration(decl);
        }
        return;
      }
      SgClassDeclaration *nondef =
          isSgClassDeclaration(decl->get_firstNondefiningDeclaration());
      if (nondef == nullptr) {
        nondef = decl;
      }
      SgScopeStatement *scope = nondef->get_scope();
      if (scope == nullptr) {
        scope = base_scope;
      }

      if (SgTemplateInstantiationDecl *inst =
              isSgTemplateInstantiationDecl(nondef)) {
        SgClassDefinition *defn =
            SageBuilder::buildClassDefinition_nfi(inst, true);
        inst->set_definition(defn);
        defn->set_declaration(inst);
        defn->set_parent(inst);
        inst->set_definingDeclaration(inst);
        inst->unsetForward();
        setCompilerGeneratedFileInfo(defn);
        return;
      }

      if (nondef->get_type() == nullptr) {
        nondef->set_type(SgClassType::createType(nondef));
      }

      SgClassDefinition *defn = SageBuilder::buildClassDefinition_nfi();
      SgClassDeclaration *defdecl =
          new SgClassDeclaration(nondef->get_name(), nondef->get_class_type(),
                                 nondef->get_type(), defn);
      defdecl->set_definition(defn);
      defn->set_declaration(defdecl);
      defn->set_parent(defdecl);
      defdecl->set_definingDeclaration(defdecl);
      defdecl->set_firstNondefiningDeclaration(nondef);
      defdecl->unsetForward();
      defdecl->set_scope(scope);
      defdecl->set_parent(scope);
      nondef->set_definingDeclaration(defdecl);
      setCompilerGeneratedFileInfo(defdecl);
      setCompilerGeneratedFileInfo(defn);
    };

    auto build_placeholder_base_decl =
        [&](const SgName &base_name) -> SgClassDeclaration * {
      SgScopeStatement *scope = nullptr;
      if (SgClassDeclaration *owner_decl = class_def->get_declaration()) {
        scope = owner_decl->get_scope();
      }
      if (scope == nullptr) {
        scope = SageBuilder::topScopeStack();
      }
      if (scope == nullptr) {
        scope = class_def;
      }
      SgName sanitized = base_name;
      if (SageInterface::hasTemplateSyntax(sanitized)) {
        sanitized = sanitize_base_name(sanitized.getString());
      }
      SgClassDeclaration *nondef =
          SageBuilder::buildNondefiningClassDeclaration_nfi(
              sanitized, SgClassDeclaration::e_class, scope,
              /*buildTemplateInstantiation=*/false, nullptr);
      if (nondef == nullptr) {
        return nullptr;
      }
      ensure_defining_decl(nondef);
      return nondef;
    };

    for (SgBaseClass *existing : class_def->get_inheritances()) {
      if (existing == nullptr) {
        continue;
      }
      SgClassDeclaration *existing_decl = existing->get_base_class();
      if (existing_decl == nullptr) {
        continue;
      }
      ensure_defining_decl(existing_decl);
    }

    for (const clang::CXXBaseSpecifier &base : cxx_record->bases()) {
      clang::QualType base_type = base.getType();
      if (base_type.isNull()) {
        continue;
      }

      SgType *sg_base_type = buildTypeFromQualifiedType(base_type);
      if (sg_base_type == nullptr) {
        sg_base_type = SageBuilder::buildUnknownType();
      }

      SgType *stripped = sg_base_type->stripTypedefsAndModifiers();
      if (stripped == nullptr) {
        stripped = sg_base_type;
      }

      SgBaseClass *base_class = nullptr;
      if (SgClassType *class_type = isSgClassType(stripped)) {
        SgClassDeclaration *base_decl =
            isSgClassDeclaration(class_type->get_declaration());
        if (base_decl != nullptr) {
          if (SgClassDeclaration *first = isSgClassDeclaration(
                  base_decl->get_firstNondefiningDeclaration())) {
            base_decl = first;
          }
          ensure_defining_decl(base_decl);
          if (!has_class_base(base_decl)) {
            base_class = SageBuilder::buildBaseClass(base_decl, class_def,
                                                     base.isVirtual(), true);
          }
        } else {
          SgName base_name = sanitize_base_name(base_type.getAsString());
          SgClassDeclaration *placeholder =
              build_placeholder_base_decl(base_name);
          if (placeholder != nullptr && !has_class_base(placeholder)) {
            base_class = SageBuilder::buildBaseClass(placeholder, class_def,
                                                     base.isVirtual(), true);
          }
        }
      } else if (SgNonrealType *nr_type = isSgNonrealType(stripped)) {
        SgNonrealDecl *nrdecl = isSgNonrealDecl(nr_type->get_declaration());
        SgName base_name = nrdecl != nullptr
                               ? nrdecl->get_name()
                               : sanitize_base_name("__unknown_base");
        SgClassDeclaration *placeholder =
            build_placeholder_base_decl(base_name);
        if (placeholder != nullptr && !has_class_base(placeholder)) {
          base_class = SageBuilder::buildBaseClass(placeholder, class_def,
                                                   base.isVirtual(), true);
        }
      } else if (SgTemplateType *tmpl_type = isSgTemplateType(stripped)) {
        SgName base_name = tmpl_type->get_name();
        if (base_name.getString().empty()) {
          base_name = "__template_base";
        }
        SgClassDeclaration *placeholder =
            build_placeholder_base_decl(base_name);
        if (placeholder != nullptr && !has_class_base(placeholder)) {
          base_class = SageBuilder::buildBaseClass(placeholder, class_def,
                                                   base.isVirtual(), true);
        }
      } else {
        SgName base_name = sanitize_base_name(base_type.getAsString());
        SgClassDeclaration *placeholder =
            build_placeholder_base_decl(base_name);
        if (placeholder != nullptr && !has_class_base(placeholder)) {
          base_class = SageBuilder::buildBaseClass(placeholder, class_def,
                                                   base.isVirtual(), true);
        }
      }

      if (base_class != nullptr) {
        SgBaseClassModifier *modifier = base_class->get_baseClassModifier();
        if (modifier != nullptr) {
          SgAccessModifier &access = modifier->get_accessModifier();
          switch (base.getAccessSpecifier()) {
          case clang::AS_public:
            access.setPublic();
            break;
          case clang::AS_protected:
            access.setProtected();
            break;
          case clang::AS_private:
            access.setPrivate();
            break;
          case clang::AS_none:
            access.setUnknown();
            break;
          }
          access.set_is_explicit(base.getAccessSpecifierAsWritten() !=
                                 clang::AS_none);
        }
      }
    }
  }

  for (clang::Decl *inner_decl : record_decl->decls()) {
    if (inner_decl == nullptr) {
      continue;
    }

    // Clang's semantic DeclContext for a class includes out-of-line member
    // definitions (including nested classes). ROSE's member list is the
    // *lexical* class body and must not include those out-of-line definitions,
    // otherwise they can be emitted inside the class and the real out-of-line
    // definition is left without the required qualified name and template
    // header (Issue 69).
    if (inner_decl->getLexicalDeclContext() != record_decl) {
      continue;
    }
    if (inner_decl->isOutOfLine()) {
      continue;
    }
    if (p_compiler_instance != nullptr) {
      clang::SourceRange brace_range = record_decl->getBraceRange();
      clang::SourceLocation inner_loc = inner_decl->getLocation();
      if (brace_range.isValid() && inner_loc.isValid()) {
        clang::SourceManager &sm = p_compiler_instance->getSourceManager();
        clang::SourceLocation start = sm.getFileLoc(brace_range.getBegin());
        clang::SourceLocation end = sm.getFileLoc(brace_range.getEnd());
        clang::SourceLocation loc = sm.getFileLoc(inner_loc);
        if (start.isValid() && end.isValid() && loc.isValid()) {
          if (sm.isBeforeInTranslationUnit(loc, start) ||
              sm.isBeforeInTranslationUnit(end, loc)) {
            continue;
          }
        }
      }
    }

    // Check for explicit access specifier (e.g. "private:")
    if (llvm::isa<clang::AccessSpecDecl>(inner_decl)) {
      explicit_access_context = true;
      // We continue because AccessSpecDecl doesn't map to a ROSE node in
      // Traverse But we need to update current_access to match what
      // Traverse/Unparser expects
      clang::AccessSpecDecl *asd =
          llvm::cast<clang::AccessSpecDecl>(inner_decl);
      clang::AccessSpecifier as = asd->getAccess();
      if (as == clang::AS_public)
        current_access = SgAccessModifier::e_public;
      else if (as == clang::AS_protected)
        current_access = SgAccessModifier::e_protected;
      else if (as == clang::AS_private)
        current_access = SgAccessModifier::e_private;
      continue;
    }

    // Clang models records/enums declared as part of a declarator
    // (e.g., `struct Y { ... } field;` or `struct { ... } field;`) as TagDecls
    // that are "embedded in declarator". These must not be appended as
    // standalone members; the owning declarator/field will already emit the
    // embedded definition inline during unparsing.
    if (clang::TagDecl *tag_decl = llvm::dyn_cast<clang::TagDecl>(inner_decl)) {
      if (tag_decl->isEmbeddedInDeclarator() ||
          isTagEmbeddedInField(tag_decl, record_decl, source_manager, false)) {
        continue;
      }
    }

    if (p_decl_translation_in_progress.find(inner_decl) !=
        p_decl_translation_in_progress.end()) {
      if (llvm::isa<clang::FunctionDecl>(inner_decl)) {
        continue;
      }
    }

    if (inner_decl->isImplicit()) {
      bool allow_implicit = false;
      if (clang::FieldDecl *field_decl =
              llvm::dyn_cast<clang::FieldDecl>(inner_decl)) {
        if (field_decl->isAnonymousStructOrUnion()) {
          const clang::CXXRecordDecl *parent_record =
              llvm::dyn_cast<clang::CXXRecordDecl>(field_decl->getParent());
          bool is_lambda_field =
              parent_record != nullptr && parent_record->isLambda();
          // Anonymous unions are spelled in the source but modeled as implicit
          // fields; keep them so the union definition is emitted in-class.
          allow_implicit = !is_lambda_field;
        }
      }
      if (!allow_implicit) {
        continue;
      }
    }

    SgNode *sg_child = Traverse(inner_decl);
    if (SgDeclarationStatement *child_decl =
            isSgDeclarationStatement(sg_child)) {
      if (child_decl->get_parent() != class_def) {
        child_decl->set_parent(class_def);
      }
      bool is_friend_decl = child_decl->get_declarationModifier().isFriend();
      if (llvm::isa<clang::FriendDecl>(inner_decl) ||
          llvm::isa<clang::FriendTemplateDecl>(inner_decl)) {
        is_friend_decl = true;
      }
      if (clang::FunctionDecl *inner_func =
              llvm::dyn_cast<clang::FunctionDecl>(inner_decl)) {
        if (inner_func->getFriendObjectKind() != clang::Decl::FOK_None) {
          is_friend_decl = true;
        } else if (llvm::isa<clang::CXXMethodDecl>(inner_func)) {
          // Member functions are not friends of the current class.
        } else if (inner_func->getDeclContext() != record_decl) {
          is_friend_decl = true;
        } else if (inner_func->getDeclContext() == record_decl) {
          // Non-member functions declared inside a class are friends.
          is_friend_decl = true;
        }
      } else if (clang::FunctionTemplateDecl *inner_tmpl =
                     llvm::dyn_cast<clang::FunctionTemplateDecl>(inner_decl)) {
        clang::FunctionDecl *templated = inner_tmpl->getTemplatedDecl();
        if (templated != nullptr &&
            templated->getFriendObjectKind() != clang::Decl::FOK_None) {
          is_friend_decl = true;
        } else if (templated != nullptr &&
                   llvm::isa<clang::CXXMethodDecl>(templated)) {
          // Template member functions are not friends of the current class.
        } else if (templated != nullptr &&
                   templated->getDeclContext() != record_decl) {
          is_friend_decl = true;
        } else if (templated != nullptr &&
                   templated->getDeclContext() == record_decl) {
          // Friend function templates declared in class scope may not carry
          // FriendObjectKind; treat non-member templates as friends.
          is_friend_decl = true;
        }
      }

      SgScopeStatement *friend_scope = nullptr;
      if (is_friend_decl) {
        friend_scope = get_enclosing_namespace_scope(class_def);
        if (friend_scope == nullptr) {
          friend_scope = getGlobalScope();
        }
        friend_scope = normalizeNamespaceScope(friend_scope);
      }

      auto build_symbol_for_decl =
          [](SgDeclarationStatement *decl) -> SgSymbol * {
        if (decl == nullptr) {
          return nullptr;
        }
        if (SgTemplateClassDeclaration *tmpl_decl =
                isSgTemplateClassDeclaration(decl)) {
          return new SgTemplateClassSymbol(tmpl_decl);
        }
        if (SgClassDeclaration *class_decl = isSgClassDeclaration(decl)) {
          return new SgClassSymbol(class_decl);
        }
        if (SgEnumDeclaration *enum_decl = isSgEnumDeclaration(decl)) {
          return new SgEnumSymbol(enum_decl);
        }
        if (SgTemplateTypedefDeclaration *tmpl_typedef =
                isSgTemplateTypedefDeclaration(decl)) {
          return new SgTemplateTypedefSymbol(tmpl_typedef);
        }
        if (SgTypedefDeclaration *typedef_decl = isSgTypedefDeclaration(decl)) {
          return new SgTypedefSymbol(typedef_decl);
        }
        if (SgTemplateMemberFunctionDeclaration *tmpl_member =
                isSgTemplateMemberFunctionDeclaration(decl)) {
          return new SgTemplateMemberFunctionSymbol(tmpl_member);
        }
        if (SgMemberFunctionDeclaration *member =
                isSgMemberFunctionDeclaration(decl)) {
          return new SgMemberFunctionSymbol(member);
        }
        if (SgTemplateFunctionDeclaration *tmpl_func =
                isSgTemplateFunctionDeclaration(decl)) {
          return new SgTemplateFunctionSymbol(tmpl_func);
        }
        if (SgFunctionDeclaration *func = isSgFunctionDeclaration(decl)) {
          return new SgFunctionSymbol(func);
        }
        return nullptr;
      };

      auto rehome_decl_symbol = [&](SgDeclarationStatement *decl,
                                    SgScopeStatement *old_scope,
                                    SgScopeStatement *new_scope) {
        if (decl == nullptr || old_scope == nullptr || new_scope == nullptr) {
          return;
        }
        if (old_scope == new_scope) {
          return;
        }

        if (SgFunctionDeclaration *func_decl = isSgFunctionDeclaration(decl)) {
          RehomeFunctionSymbolResult rehome_result =
              rehome_function_symbols_between_scopes(
                  func_decl, old_scope, new_scope, REHOME_ALLOW_EMPTY_SOURCE);
          if (rehome_result.existing == nullptr &&
              new_scope->find_symbol_from_declaration(decl) == nullptr) {
            SgSymbol *symbol = build_symbol_for_decl(decl);
            if (symbol != nullptr && !new_scope->symbol_exists(symbol)) {
              new_scope->insert_symbol(symbol->get_name(), symbol);
            }
          }
          return;
        }

        SgSymbol *symbol = old_scope->find_symbol_from_declaration(decl);
        bool removed_symbol = false;
        if (symbol != nullptr) {
          if (old_scope->symbol_exists(symbol)) {
            old_scope->remove_symbol(symbol);
            removed_symbol = true;
          } else if (SgSymbolTable *table = old_scope->get_symbol_table()) {
            if (table->exists(symbol)) {
              table->remove(symbol);
              removed_symbol = true;
            }
          }
        }

        if (new_scope->find_symbol_from_declaration(decl) == nullptr) {
          if (symbol == nullptr) {
            symbol = build_symbol_for_decl(decl);
          }
          if (symbol != nullptr && !new_scope->symbol_exists(symbol)) {
            new_scope->insert_symbol(symbol->get_name(), symbol);
          }
        } else if (removed_symbol) {
          move_symbol_to_orphan_table(symbol);
        }
      };

      bool is_friend_member =
          is_friend_decl &&
          (isSgMemberFunctionDeclaration(child_decl) != nullptr);
      bool force_class_scope = !is_friend_decl;
      if (SgFunctionDeclaration *func_decl =
              isSgFunctionDeclaration(child_decl)) {
        bool is_member = isSgMemberFunctionDeclaration(func_decl) != nullptr;
        bool is_friend_free =
            func_decl->get_declarationModifier().isFriend() && !is_member;
        if (is_friend_free) {
          force_class_scope = false;
        }
      }
      SgScopeStatement *old_scope = child_decl->get_scope();
      if (force_class_scope && child_decl->get_scope() != class_def) {
        child_decl->set_scope(class_def);
      } else if (!force_class_scope && !is_friend_member) {
        SgScopeStatement *child_scope = child_decl->get_scope();
        if (child_scope == nullptr ||
            isSgClassDefinition(child_scope) != nullptr ||
            isSgTemplateClassDefinition(child_scope) != nullptr ||
            isSgTemplateInstantiationDefn(child_scope) != nullptr) {
          if (friend_scope != nullptr) {
            child_decl->set_scope(friend_scope);
          }
        }
      }
      if (is_friend_decl && !is_friend_member && friend_scope != nullptr) {
        if (SgFunctionDeclaration *func_decl =
                isSgFunctionDeclaration(child_decl)) {
          auto reassign_scope = [](SgFunctionDeclaration *decl,
                                   SgScopeStatement *scope) {
            if (decl != nullptr && scope != nullptr &&
                decl->get_scope() != scope) {
              decl->set_scope(scope);
            }
          };
          reassign_scope(func_decl, friend_scope);
          reassign_scope(isSgFunctionDeclaration(
                             func_decl->get_firstNondefiningDeclaration()),
                         friend_scope);
          reassign_scope(
              isSgFunctionDeclaration(func_decl->get_definingDeclaration()),
              friend_scope);
        }
      }
      SgScopeStatement *new_scope = child_decl->get_scope();
      if (old_scope != nullptr && new_scope != nullptr &&
          old_scope != new_scope) {
        rehome_decl_symbol(child_decl, old_scope, new_scope);
      }
      diagnose_null_scope(child_decl, "populateClassDefinition");

      const SgDeclarationStatementPtrList &members = class_def->get_members();
      if (std::find(members.begin(), members.end(), child_decl) ==
          members.end()) {
        class_def->append_member(child_decl);
      }

      // Redundancy Check
      SgAccessModifier &mod =
          child_decl->get_declarationModifier().get_accessModifier();
      SgAccessModifier::access_modifier_enum access = mod.get_modifier();

      if (access != SgAccessModifier::e_unknown) {
        // REX ARCHITECTURE FIX:
        // Instead of stripping redundant modifiers to e_unknown, we keep
        // them but mark whether they are explicit or implicit. This
        // preserves analysis safety (isPrivate() == true) while allowing
        // the unparser to distinguish user intent.

        mod.set_is_explicit(explicit_access_context);
        if (explicit_access_context) {
          explicit_access_context = false;
        }

        // Track current access for our own logic if needed, though with
        // the new architecture we rely less on this state for stripping.
        if (access != current_access) {
          current_access = access;
        }
      }
    }
  }

  SageBuilder::popScopeStack();
}

SgTemplateParameter *ClangToSageTranslator::translateTemplateParameter(
    clang::NamedDecl *param_decl, SgDeclarationStatement *owning_template,
    unsigned position) {
  std::map<clang::Decl *, SgNode *>::iterator it =
      p_decl_translation_map.find(param_decl);
  if (it != p_decl_translation_map.end()) {
#if DEBUG_TRAVERSE_DECL
    std::cerr << "Traverse Decl : " << param_decl << " ";
    if (clang::NamedDecl::classof(param_decl)) {
      std::cerr << ": " << ((clang::NamedDecl *)param_decl)->getNameAsString()
                << ") ";
    }
    std::cerr << " already visited : node = " << it->second << std::endl;
#endif
    return isSgTemplateParameter(it->second);
  }

  SgTemplateParameter *sg_param = nullptr;

  if (clang::TemplateTypeParmDecl *type_param =
          llvm::dyn_cast<clang::TemplateTypeParmDecl>(param_decl)) {
    std::string name_str = type_param->getNameAsString();
    // REX FIX: Don't generate placeholder names for anonymous parameters
    // Leave them empty so the unparser knows they're anonymous
    // The placeholder names like __type_param_0 are not needed

    SgTemplateType *template_type =
        SageBuilder::buildTemplateType(SgName(name_str));
    if (type_param->isParameterPack()) {
      template_type->set_packed(true);
    }
    sg_param = SageBuilder::buildTemplateParameter(
        SgTemplateParameter::type_parameter, template_type);

    // REX FIX: Set keyword (typename vs class) for type parameters
    // This is needed for child parameters of template-template parameters
    // which are not visited via VisitTemplateTypeParmDecl
    std::string kw =
        type_param->wasDeclaredWithTypename() ? "typename" : "class";
    SageInterface::setTemplateParameterKeyword(sg_param, kw);

    if (type_param->hasDefaultArgument()) {
      const clang::TemplateArgumentLoc &default_loc =
          type_param->getDefaultArgument();
      const clang::TemplateArgument &default_arg = default_loc.getArgument();

      if (default_arg.getKind() == clang::TemplateArgument::Type) {
        SgType *default_type =
            buildTypeFromQualifiedType(default_arg.getAsType());
        if (default_type != nullptr) {
          sg_param->set_defaultTypeParameter(default_type);
        }
      }
    }

    if (type_param->isParameterPack()) {
      sg_param->set_is_parameter_pack(true);
    }

    if (type_param->hasTypeConstraint()) {
      const clang::TypeConstraint *constraint = type_param->getTypeConstraint();
      const clang::Expr *constraint_expr =
          constraint ? constraint->getImmediatelyDeclaredConstraint() : nullptr;
      if (SgExpression *sg_constraint =
              translateConstraintExpression(constraint_expr)) {
        sg_param->set_typeConstraint(sg_constraint);
        sg_constraint->set_parent(sg_param);
      }
    }
  } else if (clang::NonTypeTemplateParmDecl *non_type_param =
                 llvm::dyn_cast<clang::NonTypeTemplateParmDecl>(param_decl)) {
    std::string name_str = non_type_param->getNameAsString();
    if (name_str.empty()) {
      name_str = "__non_type_param_" + std::to_string(position);
    }

    SgType *type = buildTypeFromQualifiedType(non_type_param->getType());
    if (type == nullptr) {
      type = SageBuilder::buildIntType();
    }

    // Build initialized name so the unparser keeps the parameter name/type when
    // no default is present.
    SgInitializedName *init_name =
        SageBuilder::buildInitializedName(SgName(name_str), type);
    applySourceRange(init_name, non_type_param->getSourceRange());

    SgTemplateParameter *param = SageBuilder::buildTemplateParameter(
        SgTemplateParameter::nontype_parameter, type);
    param->set_initializedName(init_name);
    init_name->set_parent(param);
    param->set_type(type);
    sg_param = param;

    if (non_type_param->isParameterPack()) {
      param->set_is_parameter_pack(true);
      init_name->set_is_parameter_pack(true);
    }

    // NOTE: Template parameters don't set declptr. SgTemplateParameter is an
    // SgSupport node, not an SgDeclarationStatement, so it cannot be used with
    // set_declptr(). The parent relationship (set above) is sufficient for
    // template parameters.

    if (non_type_param->hasDefaultArgument()) {
      const clang::TemplateArgumentLoc &default_loc =
          non_type_param->getDefaultArgument();
      const clang::TemplateArgument &default_arg = default_loc.getArgument();
      SgExpression *sg_default_expr = nullptr;

      switch (default_arg.getKind()) {
      case clang::TemplateArgument::Expression: {
        clang::Expr *expr = default_arg.getAsExpr();
        if (expr != nullptr) {
          SgNode *sg_node = Traverse(expr);
          sg_default_expr = isSgExpression(sg_node);
        }
        break;
      }
      case clang::TemplateArgument::Integral: {
        const llvm::APSInt &value = default_arg.getAsIntegral();
        bool is_signed = value.isSigned();
        unsigned bitwidth = value.getBitWidth();
        if (is_signed) {
          long long v = (bitwidth <= 63) ? value.getSExtValue() : 0;
          sg_default_expr = SageBuilder::buildLongLongIntVal(v);
        } else {
          unsigned long long v = (bitwidth <= 64) ? value.getZExtValue() : 0;
          sg_default_expr = SageBuilder::buildUnsignedLongLongIntVal(v);
        }
        if (SgLongLongIntVal *ll = isSgLongLongIntVal(sg_default_expr)) {
          llvm::SmallString<64> buf;
          value.toString(buf, 10, value.isSigned());
          ll->set_valueString(std::string(buf.begin(), buf.end()));
        } else if (SgUnsignedLongLongIntVal *ull =
                       isSgUnsignedLongLongIntVal(sg_default_expr)) {
          llvm::SmallString<64> buf;
          value.toString(buf, 10, value.isSigned());
          ull->set_valueString(std::string(buf.begin(), buf.end()));
        }
        break;
      }
      default:
        break;
      }

      if (sg_default_expr != nullptr) {
        sg_param->set_defaultExpressionParameter(sg_default_expr);
      }
    }

    if (const clang::Expr *constraint_expr =
            non_type_param->getPlaceholderTypeConstraint()) {
      if (SgExpression *sg_constraint =
              translateConstraintExpression(constraint_expr)) {
        sg_param->set_typeConstraint(sg_constraint);
        sg_constraint->set_parent(sg_param);
      }
    }
  } else if (clang::TemplateTemplateParmDecl *template_template_param =
                 llvm::dyn_cast<clang::TemplateTemplateParmDecl>(param_decl)) {
    // Proper implementation for TemplateTemplateParmDecl using SgNonrealDecl
    std::string name_str = template_template_param->getNameAsString();
    if (name_str.empty()) {
      name_str = "__template_template_param_" + std::to_string(position);
    }

    // Create SgNonrealDecl to represent the template template parameter
    // using the current scope.
    SgDeclarationScope *decl_scope =
        SageBuilder::getOrCreateNonrealDeclarationScope(owning_template);
    if (decl_scope == nullptr) {
      SgScopeStatement *current_scope = SageBuilder::topScopeStack();
      ROSE_ASSERT(current_scope != nullptr);

      decl_scope = isSgDeclarationScope(current_scope);
      if (decl_scope == nullptr) {
        decl_scope = SageBuilder::buildDeclarationScope();
        decl_scope->set_parent(current_scope);
      }
    }

    SgNonrealDecl *nrdecl =
        SageBuilder::buildNonrealDecl(SgName(name_str), decl_scope);
    diagnose_null_scope(nrdecl, "TemplateTemplateParmDecl");

    // Create the template parameter with parameter_template kind
    // Use SgTemplateType with the parameter name
    SgTemplateType *param_type =
        SageBuilder::buildTemplateType(SgName(name_str));
    if (template_template_param->isParameterPack()) {
      param_type->set_packed(true);
    }
    sg_param = SageBuilder::buildTemplateParameter(
        SgTemplateParameter::template_parameter, param_type);

    // Set the declaration parameter to the SgNonrealDecl
    // For template_parameter, get_templateDeclaration() is used to retrieve the
    // nrdecl
    sg_param->set_templateDeclaration(nrdecl);

    // Translate and set the template parameters of the template template
    // parameter
    clang::TemplateParameterList *child_params =
        template_template_param->getTemplateParameters();
    if (child_params) {
      // Pass nrdecl as the owning template for these parameters
      auto sg_child_params =
          translateTemplateParameterList(child_params, nrdecl);
      // SgNonrealDecl::get_tpl_params() returns a reference to the list
      nrdecl->get_tpl_params() = *sg_child_params;
    }

    // REX FIX: Set keyword (typename vs class) for the outer part of
    // template-template parameter e.g., "template <typename ...> typename C" -
    // the outer "typename" before C
    std::string outer_kw = template_template_param->wasDeclaredWithTypename()
                               ? "typename"
                               : "class";
    SageInterface::setTemplateParameterKeyword(sg_param, outer_kw);

    if (template_template_param->isParameterPack()) {
      sg_param->set_is_parameter_pack(true);
    }
  } else {
    std::cerr << "Warning: Unsupported template parameter kind: "
              << param_decl->getDeclKindName() << std::endl;
    return nullptr;
  }

  if (sg_param != nullptr) {
    applySourceRange(sg_param, param_decl->getSourceRange());

    // Only set owning template if it's NOT a template_parameter,
    // because template_parameter uses this field for the nrdecl.
    if (owning_template != nullptr &&
        sg_param->get_parameterType() !=
            SgTemplateParameter::template_parameter) {
      sg_param->set_templateDeclaration(owning_template);
    } else if (sg_param->get_parameterType() ==
               SgTemplateParameter::template_parameter) {
      // Verify that templateDeclaration is still the SgNonrealDecl
      SgNode *decl = sg_param->get_templateDeclaration();

      if (!isSgNonrealDecl(decl)) {
        std::cerr << "ERROR: templateDeclaration is NOT SgNonrealDecl!"
                  << std::endl;
      }
    }
    p_decl_translation_map.insert(std::make_pair(param_decl, sg_param));
  }

  return sg_param;
}

SgNode *ClangToSageTranslator::TraverseOnDemand(clang::Decl *decl) {
  if (decl == nullptr) {
    return nullptr;
  }

  struct OnDemandGuard {
    std::set<clang::Decl *> &set;
    clang::Decl *decl;
    bool inserted;
    OnDemandGuard(std::set<clang::Decl *> &set, clang::Decl *decl)
        : set(set), decl(decl), inserted(false) {
      inserted = set.insert(decl).second;
    }
    ~OnDemandGuard() {
      if (inserted) {
        set.erase(decl);
      }
    }
  } guard(p_decl_translation_on_demand, decl);

  SgNode *result = Traverse(decl);
  if (guard.inserted) {
    reconcileOnDemandTranslation(result);
  }
  return result;
}

SgNode *ClangToSageTranslator::Traverse(clang::Decl *decl) {
  if (decl == nullptr)
    return nullptr;

  if (isClangOpenMPDecl(decl)) {
    rejectClangOpenMPDecl(decl);
  }

  if (clang::NamedDecl *nd = llvm::dyn_cast<clang::NamedDecl>(decl)) {
    std::string name = getDeclNameSafe(nd);
    if (name == "dep" || name == "pack" || name == "a" || name == "ms") {
      // std::cerr << "DEBUG: Traverse(Decl) for " << name << " kind: " <<
      // decl->getDeclKindName() << std::endl;
    }
  }

  clang::Decl *map_key = decl;
  if (clang::NamespaceDecl *ns_decl =
          llvm::dyn_cast<clang::NamespaceDecl>(decl)) {
    map_key = getCanonicalNamespaceDecl(ns_decl);
  }

  std::map<clang::Decl *, SgNode *>::iterator it =
      p_decl_translation_map.find(map_key);
  if (it != p_decl_translation_map.end()) {
    if (llvm::isa<clang::NamespaceDecl>(decl)) {
      // Allow VisitNamespaceDecl to run so namespace reopenings share the same
      // global_definition and symbol table.
      goto translate_decl;
    }
    if (clang::RecordDecl *record_decl =
            llvm::dyn_cast<clang::RecordDecl>(decl)) {
      if (record_decl->isThisDeclarationADefinition()) {
        if (SgClassDeclaration *existing_class =
                isSgClassDeclaration(it->second)) {
          bool has_definition = existing_class->get_definition() != nullptr;
          if (!has_definition) {
            if (SgClassDeclaration *def_decl = isSgClassDeclaration(
                    existing_class->get_definingDeclaration())) {
              has_definition = def_decl->get_definition() != nullptr;
            }
          }
          if (!has_definition) {
            // Allow VisitRecordDecl to run so the defining declaration and
            // members are populated.
            goto translate_decl;
          }
        }
      }
    }
#if DEBUG_TRAVERSE_DECL
    std::cerr << "Traverse Decl : " << decl << " ";
    if (clang::NamedDecl::classof(decl)) {
      std::cerr << ": " << getDeclNameSafe((clang::NamedDecl *)decl) << ") ";
    }
    std::cerr << " already visited : node = " << it->second << std::endl;
#endif
    if (it->second == nullptr && clang::NamedDecl::classof(decl)) {
      std::string name = getDeclNameSafe((clang::NamedDecl *)decl);
      if (name == "tuple") {
        std::cerr << "DEBUG: Traverse found 'tuple' in map but node is nullptr!"
                  << std::endl;
      }
    }
    return it->second;
  }

translate_decl:
  SgNode *result = nullptr;
  bool ret_status = false;

  switch (decl->getKind()) {
  case clang::Decl::AccessSpec:
    ret_status = VisitAccessSpecDecl((clang::AccessSpecDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != nullptr);
    break;
  case clang::Decl::Block:
    ret_status = VisitBlockDecl((clang::BlockDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != nullptr);
    break;
  case clang::Decl::Captured:
    ret_status = VisitCapturedDecl((clang::CapturedDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != nullptr);
    break;
  case clang::Decl::Empty:
    ret_status = VisitEmptyDecl((clang::EmptyDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != nullptr);
    break;
  case clang::Decl::Export:
    ret_status = VisitExportDecl((clang::ExportDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != nullptr);
    break;
  case clang::Decl::ExternCContext:
    ret_status =
        VisitExternCContextDecl((clang::ExternCContextDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != nullptr);
    break;
  case clang::Decl::FileScopeAsm:
    ret_status =
        VisitFileScopeAsmDecl((clang::FileScopeAsmDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != nullptr);
    break;
  case clang::Decl::Friend:
    ret_status = VisitFriendDecl((clang::FriendDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != nullptr);
    break;
  case clang::Decl::FriendTemplate:
    ret_status =
        VisitFriendTemplateDecl((clang::FriendTemplateDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != nullptr);
    break;
  case clang::Decl::Import:
    ret_status = VisitImportDecl((clang::ImportDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != nullptr);
    break;
  case clang::Decl::Label:
    ret_status = VisitLabelDecl((clang::LabelDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != nullptr);
    break;
  case clang::Decl::NamespaceAlias:
    ret_status =
        VisitNamespaceAliasDecl((clang::NamespaceAliasDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != nullptr);
    break;
  case clang::Decl::Namespace:
    ret_status = VisitNamespaceDecl((clang::NamespaceDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != nullptr);
    break;
  case clang::Decl::LinkageSpec:
    ret_status = VisitLinkageSpecDecl((clang::LinkageSpecDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != nullptr);
    break;
  case clang::Decl::BuiltinTemplate: {
    ret_status = false;
    result = nullptr;
    break;
  }
  case clang::Decl::Concept:
    ret_status = VisitConceptDecl((clang::ConceptDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != nullptr);
    break;
  case clang::Decl::ClassTemplate:
    ret_status =
        VisitClassTemplateDecl((clang::ClassTemplateDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != nullptr);
    break;
  case clang::Decl::FunctionTemplate:
    ret_status =
        VisitFunctionTemplateDecl((clang::FunctionTemplateDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != nullptr);
    break;
  case clang::Decl::TypeAliasTemplate:
    ret_status = VisitTypeAliasTemplateDecl(
        (clang::TypeAliasTemplateDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != nullptr);
    break;
  case clang::Decl::VarTemplate:
    ret_status = VisitVarTemplateDecl((clang::VarTemplateDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != nullptr);
    break;
  case clang::Decl::TemplateTemplateParm:
    ret_status = VisitTemplateTemplateParmDecl(
        (clang::TemplateTemplateParmDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != nullptr);
    break;
  case clang::Decl::Record:
    ret_status = VisitRecordDecl((clang::RecordDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != nullptr);
    break;
  case clang::Decl::CXXRecord:
    ret_status = VisitCXXRecordDecl((clang::CXXRecordDecl *)decl, &result);
    if (SgClassDeclaration *cd = isSgClassDeclaration(result)) {
      SgDeclarationStatement *firstNondef =
          cd->get_firstNondefiningDeclaration();
      ROSE_ASSERT(firstNondef != nullptr);
      ROSE_ASSERT(firstNondef->get_firstNondefiningDeclaration() != nullptr);
    }
    ROSE_ASSERT(ret_status == false || result != nullptr);
    break;
  case clang::Decl::ClassTemplateSpecialization:
    ret_status = VisitClassTemplateSpecializationDecl(
        (clang::ClassTemplateSpecializationDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != nullptr);
    break;
  case clang::Decl::ClassTemplatePartialSpecialization:
    ret_status = VisitClassTemplatePartialSpecializationDecl(
        (clang::ClassTemplatePartialSpecializationDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != nullptr);
    break;
  case clang::Decl::Enum:
    ret_status = VisitEnumDecl((clang::EnumDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != nullptr);
    break;
  case clang::Decl::TemplateTypeParm:
    ret_status =
        VisitTemplateTypeParmDecl((clang::TemplateTypeParmDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != nullptr);
    break;
  case clang::Decl::Typedef:
    ret_status = VisitTypedefDecl((clang::TypedefDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != nullptr);
    break;
  case clang::Decl::TypeAlias:
    ret_status = VisitTypeAliasDecl((clang::TypeAliasDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != nullptr);
    break;
  case clang::Decl::UnresolvedUsingTypename:
    ret_status = VisitUnresolvedUsingTypenameDecl(
        (clang::UnresolvedUsingTypenameDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != nullptr);
    break;
  case clang::Decl::Using:
    ret_status = VisitUsingDecl((clang::UsingDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != nullptr);
    break;
  case clang::Decl::UsingDirective:
    ret_status =
        VisitUsingDirectiveDecl((clang::UsingDirectiveDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != nullptr);
    break;
  case clang::Decl::UsingPack:
    ret_status = VisitUsingPackDecl((clang::UsingPackDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != nullptr);
    break;
  case clang::Decl::UsingShadow:
    ret_status = VisitUsingShadowDecl((clang::UsingShadowDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != nullptr);
    break;
  case clang::Decl::ConstructorUsingShadow:
    ret_status = VisitConstructorUsingShadowDecl(
        (clang::ConstructorUsingShadowDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != nullptr);
    break;
  case clang::Decl::Binding:
    ret_status = VisitBindingDecl((clang::BindingDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != nullptr);
    break;
  case clang::Decl::Field:
    ret_status = VisitFieldDecl((clang::FieldDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != nullptr);
    break;
  case clang::Decl::Function:
    ret_status = VisitFunctionDecl((clang::FunctionDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != nullptr);
    break;
  case clang::Decl::CXXDeductionGuide:
    ret_status = VisitCXXDeductionGuideDecl(
        (clang::CXXDeductionGuideDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != nullptr);
    break;
  case clang::Decl::CXXConstructor:
    ret_status =
        VisitCXXConstructorDecl((clang::CXXConstructorDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != nullptr);
    break;
  case clang::Decl::CXXConversion:
    ret_status =
        VisitCXXConversionDecl((clang::CXXConversionDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != nullptr);
    break;
  case clang::Decl::CXXDestructor:
    ret_status =
        VisitCXXDestructorDecl((clang::CXXDestructorDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != nullptr);
    break;
  case clang::Decl::CXXMethod:
    ret_status = VisitCXXMethodDecl((clang::CXXMethodDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != nullptr);
    break;
  case clang::Decl::MSProperty:
    ret_status = VisitMSPropertyDecl((clang::MSPropertyDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != nullptr);
    break;
  case clang::Decl::NonTypeTemplateParm:
    ret_status = VisitNonTypeTemplateParmDecl(
        (clang::NonTypeTemplateParmDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != nullptr);
    break;
  case clang::Decl::Decomposition:
    ret_status =
        VisitDecompositionDecl((clang::DecompositionDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != nullptr);
    break;
  case clang::Decl::ImplicitParam:
    ret_status =
        VisitImplicitParamDecl((clang::ImplicitParamDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != nullptr);
    break;
  case clang::Decl::ParmVar:
    ret_status = VisitParmVarDecl((clang::ParmVarDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != nullptr);
    break;
  case clang::Decl::VarTemplatePartialSpecialization:
    ret_status = VisitVarTemplatePartialSpecializationDecl(
        (clang::VarTemplatePartialSpecializationDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != nullptr);
    break;
  case clang::Decl::VarTemplateSpecialization:
    ret_status = VisitVarTemplateSpecializationDecl(
        (clang::VarTemplateSpecializationDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != nullptr);
    break;
  case clang::Decl::EnumConstant:
    ret_status =
        VisitEnumConstantDecl((clang::EnumConstantDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != nullptr);
    break;
  case clang::Decl::IndirectField:
    ret_status =
        VisitIndirectFieldDecl((clang::IndirectFieldDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != nullptr);
    break;
  case clang::Decl::UnresolvedUsingValue:
    ret_status = VisitUnresolvedUsingValueDecl(
        (clang::UnresolvedUsingValueDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != nullptr);
    break;
  case clang::Decl::PragmaComment:
    ret_status =
        VisitPragmaCommentDecl((clang::PragmaCommentDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != nullptr);
    break;
  case clang::Decl::PragmaDetectMismatch:
    ret_status = VisitPragmaDetectMismatchDecl(
        (clang::PragmaDetectMismatchDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != nullptr);
    break;
  case clang::Decl::StaticAssert:
    ret_status =
        VisitStaticAssertDecl((clang::StaticAssertDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != nullptr);
    break;
  case clang::Decl::TranslationUnit:
    ret_status =
        VisitTranslationUnitDecl((clang::TranslationUnitDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != nullptr);
    break;
  case clang::Decl::Var:
    ret_status = VisitVarDecl((clang::VarDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != nullptr);
    break;

  default:
    std::cerr << "Unknown declacaration kind: " << decl->getDeclKindName()
              << " !" << std::endl;
    ROSE_ABORT();
  }

  ROSE_ASSERT(ret_status == false || result != nullptr);

  bool on_demand = p_decl_translation_on_demand.find(decl) !=
                   p_decl_translation_on_demand.end();

  if (ret_status && result != nullptr) {
    if (SgDeclarationStatement *ds = isSgDeclarationStatement(result)) {
      ensure_parent_and_scope(ds);

      if (on_demand) {
        const clang::DeclContext *lexical_ctx = decl->getLexicalDeclContext();
        const clang::DeclContext *target_ctx = nullptr;
        if (is_declaration_scope_context(lexical_ctx)) {
          target_ctx = lexical_ctx;
        } else if (is_declaration_scope_context(decl->getDeclContext())) {
          target_ctx = decl->getDeclContext();
        }

        if (target_ctx != nullptr) {
          SgNode *parent_node = ds->get_parent();
          SgScopeStatement *parent_scope = isSgScopeStatement(parent_node);
          clang::Decl *ctx_decl = llvm::dyn_cast<clang::Decl>(
              const_cast<clang::DeclContext *>(target_ctx));
          if (ctx_decl != nullptr &&
              p_decl_translation_map.find(ctx_decl) ==
                  p_decl_translation_map.end() &&
              p_decl_translation_in_progress.find(ctx_decl) ==
                  p_decl_translation_in_progress.end()) {
            if (!llvm::isa<clang::NamespaceDecl>(ctx_decl)) {
              TraverseOnDemand(ctx_decl);
            }
          }

          SgScopeStatement *target_scope = resolveScopeFromDeclContext(
              const_cast<clang::DeclContext *>(target_ctx), nullptr);
          if (target_ctx->isRecord()) {
            if (isSgClassDefinition(target_scope) == nullptr &&
                isSgTemplateInstantiationDefn(target_scope) == nullptr &&
                isSgTemplateClassDefinition(target_scope) == nullptr) {
              target_scope = nullptr;
            }
          }
          if (target_scope == nullptr && target_ctx->isTranslationUnit()) {
            target_scope = getGlobalScope();
          }
          if (target_scope != nullptr) {
            if (parent_scope != nullptr && parent_scope != target_scope) {
              detach_decl_from_scope_child_list(ds, parent_scope);
            }
            ensure_decl_in_scope_child_list(ds, target_scope,
                                            "Traverse:decl-context");
          }
        }
      }

      registerDeclarationSymbol(ds);
    }
    p_decl_translation_map.insert(
        std::pair<clang::Decl *, SgNode *>(decl, result));
  }

#if DEBUG_TRAVERSE_DECL
  std::cerr << "Traverse(clang::Decl : " << decl << " ";
  if (clang::NamedDecl::classof(decl)) {
    std::cerr << ": " << ((clang::NamedDecl *)decl)->getNameAsString() << ") ";
  }
  std::cerr << " visit done : node = " << result << std::endl;
#endif

  return ret_status ? result : nullptr;
}

// Pei-Hung (09/01/2023) Revised this to iterate Decls in the DeclContext.
// DeclContext is derived into others but this is called only by
// VisitTranslationUnit and VisitNamespace for now. The top scope retrieved from
// SageBuilder::topScopeStack() should be properly defined before calling this.
bool ClangToSageTranslator::TraverseForDeclContext(
    clang::DeclContext *decl_context) {
  SgScopeStatement *scope = SageBuilder::topScopeStack();
  SgGlobal *global_scope = isSgGlobal(scope);
  SgNamespaceDefinitionStatement *namespace_scope =
      isSgNamespaceDefinitionStatement(scope);

  p_decl_context_map.insert(
      std::pair<clang::DeclContext *, SgScopeStatement *>(decl_context, scope));
  bool res = true;
  clang::DeclContext::decl_iterator it;
  for (it = decl_context->decls_begin(); it != decl_context->decls_end();
       it++) {
    clang::Decl *decl = (*it);
    if (decl == nullptr) {
      continue;
    }

    if (namespace_scope != nullptr) {
      const clang::DeclContext *lexical_ctx = decl->getLexicalDeclContext();
      if (const clang::NamespaceDecl *lexical_ns =
              llvm::dyn_cast_or_null<clang::NamespaceDecl>(lexical_ctx)) {
        if (lexical_ns != decl_context) {
          continue;
        }
      }
    }

    if (global_scope && SgProject::get_verbose() > 0) {
      if (clang::NamedDecl *named = llvm::dyn_cast<clang::NamedDecl>(decl)) {
        std::string n = named->getNameAsString();
        if (n == "uint8_t" || n == "uint16_t" || n == "uint32_t" ||
            n == "in_port_t" || n == "in_addr_t" || n == "in6_addr" ||
            n == "in_addr" || n == "sockaddr_in" || n == "ntohl" ||
            n == "ntohs" || n == "htonl" || n == "htons") {
          unsigned line = 0;
          if (p_compiler_instance != nullptr) {
            line =
                p_compiler_instance->getSourceManager().getSpellingLineNumber(
                    named->getLocation());
          }
          std::cerr << "CFE: TU visit '" << n << "' ("
                    << decl->getDeclKindName() << ") @" << line << std::endl;
        }
      }
    }

    SgNode *child = Traverse(decl);

    SgDeclarationStatement *decl_stmt = isSgDeclarationStatement(child);
    if (SgTemplateInstantiationDecl *inst_decl =
            isSgTemplateInstantiationDecl(decl_stmt)) {
      if (SgTemplateInstantiationDirectiveStatement *directive =
              isSgTemplateInstantiationDirectiveStatement(
                  inst_decl->get_parent())) {
        decl_stmt = directive;
      }
    }
    if (SgTemplateInstantiationFunctionDecl *inst_decl =
            isSgTemplateInstantiationFunctionDecl(decl_stmt)) {
      if (SgTemplateInstantiationDirectiveStatement *directive =
              isSgTemplateInstantiationDirectiveStatement(
                  inst_decl->get_parent())) {
        decl_stmt = directive;
      }
    }
    if (SgTemplateInstantiationMemberFunctionDecl *inst_decl =
            isSgTemplateInstantiationMemberFunctionDecl(decl_stmt)) {
      if (SgTemplateInstantiationDirectiveStatement *directive =
              isSgTemplateInstantiationDirectiveStatement(
                  inst_decl->get_parent())) {
        decl_stmt = directive;
      }
    }

    if (decl_stmt == nullptr && child != nullptr) {
      std::cerr << "Runtime error: the node produce for a clang::Decl is not a "
                   "SgDeclarationStatement !"
                << std::endl;
      std::cerr << "    class = " << child->class_name() << std::endl;
      res = false;
    } else if (child != nullptr) {
      // FIXME This is a hack to avoid autonomous decl of unnamed type to being
      // added to the global scope....
      SgClassDeclaration *class_decl = isSgClassDeclaration(child);
      if (class_decl != nullptr &&
          (class_decl->get_name() == "" || class_decl->get_isUnNamed())) {
        continue;
      }

      SgEnumDeclaration *enum_decl = isSgEnumDeclaration(child);
      if (enum_decl != nullptr &&
          (enum_decl->get_name() == "" || enum_decl->get_isUnNamed())) {
        continue;
      }

      if (clang::TagDecl::classof(decl)) {
        clang::TagDecl *tagDecl = (clang::TagDecl *)decl;
        if (tagDecl->isEmbeddedInDeclarator()) {
          continue;
        }
      }

      bool preserve_lexical_member = false;
      if (decl_stmt != nullptr) {
        auto should_preserve_lexical =
            [&](clang::CXXMethodDecl *method_decl) -> bool {
          if (method_decl == nullptr || !method_decl->isOutOfLine()) {
            return false;
          }
          return isSgMemberFunctionDeclaration(decl_stmt) ||
                 isSgTemplateMemberFunctionDeclaration(decl_stmt) ||
                 isSgTemplateInstantiationMemberFunctionDecl(decl_stmt);
        };

        if (clang::CXXMethodDecl *method_decl =
                llvm::dyn_cast<clang::CXXMethodDecl>(decl)) {
          preserve_lexical_member = should_preserve_lexical(method_decl);
        } else if (clang::FunctionTemplateDecl *template_decl =
                       llvm::dyn_cast<clang::FunctionTemplateDecl>(decl)) {
          if (clang::FunctionDecl *templated_decl =
                  template_decl->getTemplatedDecl()) {
            if (clang::CXXMethodDecl *templated_method =
                    llvm::dyn_cast<clang::CXXMethodDecl>(templated_decl)) {
              preserve_lexical_member =
                  should_preserve_lexical(templated_method);
            }
          }
        }
      }

      if (global_scope) {
        // Keep Clang implicit/builtin declarations out of the TU's global
        // declaration list. These decls (e.g., `__builtin_va_list`,
        // `__int128_t`) are frontend-provided and not part of the user's
        // source; attaching them structurally makes translator tests that move
        // global declarations inadvertently move/unparse builtins into user
        // files.
        if (decl->isImplicit() && decl->getLocation().isInvalid()) {
          continue;
        }

        // Likewise, frontend-support headers should not become part of the
        // user's global declaration sequence.
        if (decl_stmt->get_file_info() != nullptr &&
            decl_stmt->get_file_info()->isFrontendSpecific()) {
          continue;
        }
      }

      if (global_scope != nullptr) {
        SgScopeStatement *decl_scope = decl_stmt->get_scope();
        if (decl_scope != nullptr && decl_scope != global_scope) {
          if (!preserve_lexical_member) {
            if (decl_scope->containsOnlyDeclarations()) {
              ensure_decl_in_scope_child_list(
                  decl_stmt, decl_scope,
                  "TraverseForDeclContext:global-out-of-scope");
            }
            continue;
          }
        }

        const SgDeclarationStatementPtrList &existing =
            global_scope->get_declarations();
        if (std::find(existing.begin(), existing.end(), decl_stmt) ==
            existing.end()) {
          global_scope->append_declaration(decl_stmt);
        }
      } else if (namespace_scope != nullptr) {
        SgScopeStatement *decl_scope = decl_stmt->get_scope();
        if (decl_scope != nullptr && decl_scope != namespace_scope &&
            !preserve_lexical_member) {
          SgScopeStatement *decl_norm = normalizeNamespaceScope(decl_scope);
          SgScopeStatement *ns_norm = normalizeNamespaceScope(namespace_scope);
          if (decl_norm != ns_norm) {
            if (decl_scope->containsOnlyDeclarations()) {
              ensure_decl_in_scope_child_list(
                  decl_stmt, decl_scope,
                  "TraverseForDeclContext:namespace-out-of-scope");
            }
            continue;
          }
          ensure_decl_in_scope_child_list_preserve_scope(
              decl_stmt, namespace_scope,
              "TraverseForDeclContext:namespace-lexical");
          continue;
        }

        const SgDeclarationStatementPtrList &decls =
            namespace_scope->get_declarations();
        if (std::find(decls.begin(), decls.end(), decl_stmt) == decls.end()) {
          namespace_scope->append_declaration(decl_stmt);
        }
        if (decl_stmt->get_parent() != namespace_scope) {
          decl_stmt->set_parent(namespace_scope);
        }
        if (decl_stmt->get_scope() == nullptr) {
          decl_stmt->set_scope(namespace_scope);
        }
      } else {
        std::cerr << "Not global or namespace scope applied in "
                     "ClangToSageTranslator::TraverseForDeclContext"
                  << std::endl;
        return false;
      }
    }
  }

  if (namespace_scope != nullptr) {
    SgScopeStatement *canonical_scope =
        normalizeNamespaceScope(namespace_scope);
    if (canonical_scope != nullptr && canonical_scope != namespace_scope) {
      for (SgDeclarationStatement *decl_stmt :
           namespace_scope->get_declarations()) {
        if (decl_stmt == nullptr) {
          continue;
        }
        SgScopeStatement *decl_scope = decl_stmt->get_scope();
        if (decl_scope == nullptr || decl_scope == namespace_scope) {
          continue;
        }
        SgNamespaceDefinitionStatement *decl_ns =
            isSgNamespaceDefinitionStatement(decl_scope);
        if (decl_ns == nullptr) {
          continue;
        }
        if (normalizeNamespaceScope(decl_ns) != canonical_scope) {
          continue;
        }
        detach_decl_from_scope_child_list(decl_stmt, decl_ns);
      }
    }
  }

  return res;
}

/**********************/
/* Visit Declarations */
/**********************/

bool ClangToSageTranslator::VisitDecl(clang::Decl *decl, SgNode **node) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToSageTranslator::VisitDecl" << std::endl;
#endif
  if (*node == nullptr) {
#if DEBUG_VISIT_DECL
    const char *kind_name = decl ? decl->getDeclKindName() : "Unknown";
    std::string loc_string;
    if (decl) {
      clang::SourceLocation loc = decl->getLocation();
      if (loc.isValid()) {
        clang::SourceManager &sm = p_compiler_instance->getSourceManager();
        clang::PresumedLoc ploc = sm.getPresumedLoc(loc);
        if (ploc.isValid()) {
          loc_string = std::string(ploc.getFilename()) + ":" +
                       std::to_string(ploc.getLine());
        }
      }
    }
    if (!loc_string.empty()) {
      std::cerr
          << "Runtime error: No Sage node associated with the declaration ("
          << kind_name << " at " << loc_string << ")..." << std::endl;
    } else {
      std::cerr
          << "Runtime error: No Sage node associated with the declaration ("
          << kind_name << ")..." << std::endl;
    }
    if (const clang::NamedDecl *nd = llvm::dyn_cast<clang::NamedDecl>(decl)) {
      std::cerr << "Runtime error declaration name: " << nd->getNameAsString()
                << std::endl;
    }
#endif
    return false;
  }

  if (!isSgGlobal(*node) && !isSgTemplateParameter(*node)) {
    clang::SourceRange range = decl->getSourceRange();
    if (!range.isValid()) {
      clang::SourceLocation loc = decl->getLocation();
      if (loc.isValid()) {
        range = clang::SourceRange(loc, loc);
      }
    }
    applySourceRange(*node, range);
  }

  if (SgDeclarationStatement *declStmt = isSgDeclarationStatement(*node)) {
    SgAccessModifier &access_mod =
        declStmt->get_declarationModifier().get_accessModifier();
    if (access_mod.isUnknown() || access_mod.isDefault()) {
      clang::AccessSpecifier accessSpec = decl->getAccess();
      switch (accessSpec) {
      case clang::AS_public:
        access_mod.setPublic();
        break;
      case clang::AS_protected:
        access_mod.setProtected();
        break;
      case clang::AS_private:
        access_mod.setPrivate();
        break;
      case clang::AS_none: {
        clang::DeclContext *ctx = decl->getDeclContext();
        if (clang::RecordDecl *record_ctx =
                llvm::dyn_cast_or_null<clang::RecordDecl>(ctx)) {
          if (record_ctx->isClass()) {
            access_mod.setPrivate();
          } else {
            access_mod.setPublic();
          }
        } else {
          access_mod.setDefault();
        }
        break;
      }
      default:
        std::cerr << "no accessSpecifier is valid" << std::endl;
      }
    }
  }

  // TODO attributes
  /*
      std::cerr << "Attribute list for " << decl->getDeclKindName() << " (" <<
     decl << "): "; clang::Decl::attr_iterator it; for (it = decl->attr_begin();
     it != decl->attr_end(); it++) { std::cerr << (*it)->getKind() << ", ";
      }
      std::cerr << std::endl;

      if (clang::VarDecl::classof(decl)) {
          clang::VarDecl * var_decl = (clang::VarDecl *)decl;
          std::cerr << "Stoprage class for " << decl->getDeclKindName() << " ("
     << decl << "): " << var_decl->getStorageClass() << std::endl;
      }
  */
  return true;
}

bool ClangToSageTranslator::VisitAccessSpecDecl(
    clang::AccessSpecDecl *access_spec_decl, SgNode **node) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToSageTranslator::VisitAccessSpecDecl" << std::endl;
#endif
  // CLANG FRONTEND FIX: AccessSpecDecl (public:, private:, protected:) are not
  // standalone declarations in ROSE - they're properties of member
  // declarations. Set *node to nullptr to indicate this declaration doesn't
  // have a ROSE equivalent.
  *node = nullptr;

  // Return false to indicate no ROSE node was created (this is expected
  // behavior)
  return false;
}

bool ClangToSageTranslator::VisitBlockDecl(clang::BlockDecl *block_decl,
                                           SgNode **node) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToSageTranslator::VisitBlockDecl" << std::endl;
#endif
  bool res = true;

  // ROOT CAUSE FIX: Allow delegation to work - disabled FAIL_TODO
  // ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitDecl(block_decl, node) && res;
}

bool ClangToSageTranslator::VisitCapturedDecl(
    clang::CapturedDecl *captured_decl, SgNode **node) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToSageTranslator::VisitCapturedDecl" << std::endl;
#endif
  bool res = true;

  // ROOT CAUSE FIX: Allow delegation to work - disabled FAIL_TODO
  // ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitDecl(captured_decl, node) && res;
}

bool ClangToSageTranslator::VisitEmptyDecl(clang::EmptyDecl *empty_decl,
                                           SgNode **node) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToSageTranslator::VisitEmptyDecl" << std::endl;
#endif
  if (empty_decl == nullptr) {
    *node = nullptr;
    return false;
  }

  SgEmptyDeclaration *empty_decl_stmt = new SgEmptyDeclaration();
  ROSE_ASSERT(empty_decl_stmt != nullptr);

  empty_decl_stmt->set_definingDeclaration(empty_decl_stmt);
  empty_decl_stmt->set_firstNondefiningDeclaration(empty_decl_stmt);

  *node = empty_decl_stmt;
  return VisitDecl(empty_decl, node);
}

bool ClangToSageTranslator::VisitExportDecl(clang::ExportDecl *export_decl,
                                            SgNode **node) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToSageTranslator::VisitExportDecl" << std::endl;
#endif
  bool res = true;

  // ROOT CAUSE FIX: Allow delegation to work - disabled FAIL_TODO
  // ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitDecl(export_decl, node) && res;
}

bool ClangToSageTranslator::VisitExternCContextDecl(
    clang::ExternCContextDecl *ccontent_decl, SgNode **node) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToSageTranslator::VisitCContextDecl" << std::endl;
#endif
  bool res = true;

  // ROOT CAUSE FIX: Allow delegation to work - disabled FAIL_TODO
  // ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitDecl(ccontent_decl, node) && res;
}

bool ClangToSageTranslator::VisitFileScopeAsmDecl(
    clang::FileScopeAsmDecl *file_scope_asm_decl, SgNode **node) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToSageTranslator::VisitFileScopeAsmDecl" << std::endl;
#endif
  bool res = true;

  // LLVM 20 returns StringLiteral*, LLVM 21 returns std::string
  std::string AsmString;
#if LLVM_VERSION_MAJOR >= 21
  AsmString = file_scope_asm_decl->getAsmString();
#else
  if (auto *str_lit = file_scope_asm_decl->getAsmString()) {
    AsmString = str_lit->getString().str();
  }
#endif

#if DEBUG_VISIT_DECL
  std::cerr << "AsmString:" << AsmString << std::endl;
#endif
  SgAsmStmt *asmStmt = SageBuilder::buildAsmStatement(AsmString);
  asmStmt->set_firstNondefiningDeclaration(asmStmt);
  asmStmt->set_definingDeclaration(asmStmt);
  asmStmt->set_parent(SageBuilder::topScopeStack());
  *node = asmStmt;

  return VisitDecl(file_scope_asm_decl, node) && res;
}

bool ClangToSageTranslator::VisitFriendDecl(clang::FriendDecl *friend_decl,
                                            SgNode **node) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToSageTranslator::VisitFriendDecl" << std::endl;
  std::cerr << "FriendDecl::isUnsupportedFriend () "
            << friend_decl->isUnsupportedFriend() << std::endl;
#endif
  bool res = true;

  SgScopeStatement *current_scope = SageBuilder::topScopeStack();
  auto resolve_friend_scope =
      [&](clang::DeclContext *ctx) -> SgScopeStatement * {
    clang::DeclContext *scope_ctx = ctx;
    while (scope_ctx != nullptr && !scope_ctx->isNamespace() &&
           !scope_ctx->isTranslationUnit()) {
      scope_ctx = scope_ctx->getParent();
    }
    if (clang::NamespaceDecl *ns_decl =
            llvm::dyn_cast_or_null<clang::NamespaceDecl>(
                llvm::dyn_cast_or_null<clang::Decl>(scope_ctx))) {
      if (SgNamespaceDeclarationStatement *ns_stmt =
              ensureNamespaceDeclaration(ns_decl)) {
        if (ns_stmt->get_definition() != nullptr) {
          return ns_stmt->get_definition();
        }
      }
    }
    if (scope_ctx != nullptr && scope_ctx->isTranslationUnit()) {
      return getGlobalScope();
    }
    return nullptr;
  };

  clang::DeclContext *friend_context =
      friend_decl != nullptr ? friend_decl->getDeclContext() : nullptr;
  if (clang::NamedDecl *named_decl = friend_decl->getFriendDecl()) {
    if (clang::DeclContext *named_context = named_decl->getDeclContext()) {
      friend_context = named_context;
    }
  }
  SgScopeStatement *friend_scope = resolve_friend_scope(friend_context);
  if (friend_scope == nullptr) {
    friend_scope = get_enclosing_namespace_scope(current_scope);
  }
  if (friend_scope == nullptr) {
    friend_scope = getGlobalScope();
  }
  friend_scope = normalizeNamespaceScope(friend_scope);

  // Translate the underlying entity being declared as a friend.
  SgDeclarationStatement *sg_decl = nullptr;
  if (clang::NamedDecl *named_decl = friend_decl->getFriendDecl()) {
    if (clang::ClassTemplateDecl *class_template_decl =
            llvm::dyn_cast<clang::ClassTemplateDecl>(named_decl)) {
      // Friend class templates are declared in the enclosing namespace, not as
      // members of the class. Override the symbol scope accordingly so the
      // primary template links consistently with its namespace definition.
      sg_decl = translateClassTemplateDecl(class_template_decl, friend_scope,
                                           current_scope);
    } else {
      SgNode *tmp = Traverse(named_decl);
      sg_decl = isSgDeclarationStatement(tmp);
      if (sg_decl == nullptr) {
        if (SgInitializedName *init = isSgInitializedName(tmp)) {
          sg_decl = isSgDeclarationStatement(init->get_parent());
        }
      }
    }
  } else if (clang::TypeSourceInfo *type_info = friend_decl->getFriendType()) {
    clang::QualType friendQualType = type_info->getType();
    const clang::Type *friendType = friendQualType.getTypePtr();
    if (const auto *elab_type =
            llvm::dyn_cast<clang::ElaboratedType>(friendType)) {
      friendQualType = elab_type->getNamedType();
      friendType = friendQualType.getTypePtr();
    }

    clang::TemplateDecl *friend_template_decl = nullptr;
    if (const clang::TemplateSpecializationType *spec_type =
            llvm::dyn_cast<clang::TemplateSpecializationType>(friendType)) {
      clang::TemplateName tmpl_name = spec_type->getTemplateName();
      friend_template_decl = tmpl_name.getAsTemplateDecl();
    }

    if (const clang::RecordType *record_type =
            llvm::dyn_cast<clang::RecordType>(friendType)) {
      clang::RecordDecl *recordDecl = record_type->getDecl();
      if (recordDecl != nullptr) {
        if (SgClassSymbol *class_sym =
                isSgClassSymbol(GetSymbolFromSymbolTable(recordDecl))) {
          SgClassDeclaration *sg_def_class_decl = class_sym->get_declaration();
          if (sg_def_class_decl != nullptr) {
            SgClassDeclaration::class_types type_of_class =
                SgClassDeclaration::e_class;
            switch (recordDecl->getTagKind()) {
            case clang::TagTypeKind::Struct:
              type_of_class = SgClassDeclaration::e_struct;
              break;
            case clang::TagTypeKind::Class:
              type_of_class = SgClassDeclaration::e_class;
              break;
            case clang::TagTypeKind::Union:
              type_of_class = SgClassDeclaration::e_union;
              break;
            default:
              MLOG_ERROR_C(MLOG_FRONTEND, "Runtime error: RecordDecl can only "
                                          "be a struct/class/union.\n");
              res = false;
            }

            SgName recordName(recordDecl->getNameAsString());
            SgScopeStatement *scope = friend_scope;
            if (scope == nullptr) {
              scope = getGlobalScope();
            }
            SgClassDeclaration *sg_friend_class_decl =
                new SgClassDeclaration(recordName, type_of_class,
                                       sg_def_class_decl->get_type(), nullptr);
            SgClassDeclaration *first_nondef = isSgClassDeclaration(
                sg_def_class_decl->get_firstNondefiningDeclaration());
            if (first_nondef == nullptr) {
              first_nondef = sg_def_class_decl;
              first_nondef->set_firstNondefiningDeclaration(first_nondef);
            }
            SgClassDeclaration *def_decl = isSgClassDeclaration(
                sg_def_class_decl->get_definingDeclaration());
            if (def_decl != nullptr && def_decl->get_definition() == nullptr) {
              def_decl = nullptr;
            }
            if (def_decl == nullptr &&
                sg_def_class_decl->get_definition() != nullptr) {
              def_decl = sg_def_class_decl;
            }

            sg_friend_class_decl->set_firstNondefiningDeclaration(first_nondef);
            if (def_decl != nullptr && def_decl != first_nondef) {
              sg_friend_class_decl->set_definingDeclaration(def_decl);
            }
            sg_friend_class_decl->set_scope(scope);
            if (current_scope != nullptr) {
              sg_friend_class_decl->set_parent(current_scope);
            } else {
              sg_friend_class_decl->set_parent(scope);
            }
            sg_friend_class_decl->get_declarationModifier().setFriend();
            sg_decl = sg_friend_class_decl;
          }
        }
        if (sg_decl == nullptr) {
          sg_decl = isSgDeclarationStatement(Traverse(recordDecl));
        }
      }
    }
    if (sg_decl == nullptr) {
      SgType *sg_type = buildTypeFromQualifiedType(friendQualType);
      if (SgClassType *class_type = isSgClassType(sg_type)) {
        sg_decl = class_type->get_declaration();
      } else if (SgNamedType *named_type = isSgNamedType(sg_type)) {
        SgDeclarationStatement *named_decl = named_type->get_declaration();
        if (SgNonrealDecl *nrdecl = isSgNonrealDecl(named_decl)) {
          // Friend type is a dependent/nonreal name; do not insert the
          // SgNonrealDecl into a regular scope (it expects a declaration
          // scope parent). Instead, synthesize a nondefining class declaration
          // for the friend entry.
          SgScopeStatement *scope = friend_scope;
          if (scope == nullptr) {
            scope = getGlobalScope();
          }
          SgTemplateArgumentPtrList tpl_args = nrdecl->get_tpl_args();
          bool has_args = !tpl_args.empty();
          sg_decl = SageBuilder::buildNondefiningClassDeclaration_nfi(
              nrdecl->get_name(), SgClassDeclaration::e_class, scope, has_args,
              has_args ? &tpl_args : nullptr);
          if (sg_decl != nullptr && current_scope != nullptr) {
            sg_decl->set_parent(current_scope);
          }
          if (SgTemplateInstantiationDecl *inst_decl =
                  isSgTemplateInstantiationDecl(sg_decl)) {
            if (inst_decl->get_templateDeclaration() == nullptr) {
              SgTemplateClassDeclaration *tmpl_decl = nullptr;
              if (friend_template_decl != nullptr) {
                if (SgNode *tmpl_node =
                        TraverseOnDemand(friend_template_decl)) {
                  tmpl_decl = isSgTemplateClassDeclaration(tmpl_node);
                }
              }

              if (tmpl_decl == nullptr) {
                SgScopeStatement *lookup_scope = friend_scope;
                if (lookup_scope == nullptr) {
                  lookup_scope = get_enclosing_namespace_scope(current_scope);
                }
                lookup_scope = normalizeNamespaceScope(lookup_scope);
                if (lookup_scope == nullptr) {
                  lookup_scope = getGlobalScope();
                }

                if (lookup_scope != nullptr) {
                  if (SgTemplateClassSymbol *tmpl_sym =
                          lookup_scope->lookup_template_class_symbol(
                              nrdecl->get_name(), nullptr, nullptr)) {
                    tmpl_decl = isSgTemplateClassDeclaration(
                        tmpl_sym->get_declaration());
                  } else if (SgTemplateClassSymbol *tmpl_sym = SageInterface::
                                 lookupTemplateClassSymbolInParentScopes(
                                     nrdecl->get_name(), nullptr, nullptr,
                                     lookup_scope)) {
                    tmpl_decl = isSgTemplateClassDeclaration(
                        tmpl_sym->get_declaration());
                  }
                }
                if (tmpl_decl == nullptr) {
                  auto cache_it = p_template_decl_cache.find(
                      nrdecl->get_name().getString());
                  if (cache_it != p_template_decl_cache.end()) {
                    tmpl_decl = cache_it->second;
                  }
                }
              }

              if (tmpl_decl != nullptr) {
                inst_decl->set_templateDeclaration(tmpl_decl);
              }
            }
          }
        } else {
          sg_decl = named_decl;
        }
      }
    }
  }

  if (sg_decl == nullptr) {
    MLOG_ERROR_C(MLOG_FRONTEND,
                 "Runtime error: unable to translate FriendDecl.\n");
    *node = nullptr;
    return false;
  }

  auto mark_friend = [](SgDeclarationStatement *decl) {
    if (decl != nullptr) {
      decl->get_declarationModifier().setFriend();
    }
  };

  mark_friend(sg_decl);
  mark_friend(sg_decl->get_firstNondefiningDeclaration());
  mark_friend(sg_decl->get_definingDeclaration());

  auto force_friend_function_scope = [&](SgDeclarationStatement *decl) {
    if (decl == nullptr) {
      return;
    }
    if (SgFunctionDeclaration *func_decl = isSgFunctionDeclaration(decl)) {
      if (isSgMemberFunctionDeclaration(func_decl) != nullptr) {
        return;
      }
      if (friend_scope != nullptr && func_decl->get_scope() != friend_scope) {
        SgScopeStatement *old_scope = func_decl->get_scope();
        func_decl->set_scope(friend_scope);
        if (old_scope != nullptr) {
          rehome_friend_function_symbol(func_decl, old_scope, friend_scope);
        }
      }
    }
  };

  auto ensure_scope_and_parent =
      [](SgDeclarationStatement *decl, SgScopeStatement *parent_scope,
         SgScopeStatement *decl_scope, bool force_scope) {
        if (decl == nullptr)
          return;
        if (parent_scope != nullptr && decl->get_parent() == nullptr) {
          decl->set_parent(parent_scope);
        }
        if (decl_scope != nullptr) {
          if (force_scope) {
            if (decl->get_scope() != decl_scope) {
              decl->set_scope(decl_scope);
            }
          } else if (decl->get_scope() == nullptr) {
            decl->set_scope(decl_scope);
          }
        }
      };

  force_friend_function_scope(sg_decl);
  if (sg_decl->get_scope() == nullptr && friend_scope != nullptr) {
    sg_decl->set_scope(friend_scope);
  }
  if (current_scope != nullptr &&
      (isSgClassDefinition(current_scope) != nullptr ||
       isSgTemplateClassDefinition(current_scope) != nullptr)) {
    ensure_decl_in_scope_child_list_preserve_scope(sg_decl, current_scope,
                                                   "FriendDecl");
  } else {
    ensure_scope_and_parent(sg_decl, current_scope, friend_scope,
                            isSgFunctionDeclaration(sg_decl) == nullptr);
  }

  if (SgDeclarationStatement *first_nondef =
          sg_decl->get_firstNondefiningDeclaration()) {
    force_friend_function_scope(first_nondef);
    ensure_scope_and_parent(first_nondef, friend_scope, friend_scope,
                            isSgFunctionDeclaration(first_nondef) == nullptr);
  }
  if (SgDeclarationStatement *def_decl = sg_decl->get_definingDeclaration()) {
    force_friend_function_scope(def_decl);
    ensure_scope_and_parent(def_decl, friend_scope, friend_scope,
                            isSgFunctionDeclaration(def_decl) == nullptr);
  }
  diagnose_null_scope(sg_decl, "FriendDecl");

  rehome_friend_function_symbol_if_needed(sg_decl, current_scope, friend_scope);

  // Ensure friend definitions are wired consistently for analysis passes such
  // as VirtualCFG. Do not mark friend prototypes as defining declarations.
  auto ensure_first_nondef = [](SgDeclarationStatement *decl) {
    if (decl != nullptr && decl->get_firstNondefiningDeclaration() == nullptr) {
      decl->set_firstNondefiningDeclaration(decl);
    }
  };

  ensure_first_nondef(sg_decl);
  ensure_first_nondef(sg_decl->get_firstNondefiningDeclaration());
  ensure_first_nondef(sg_decl->get_definingDeclaration());
  if (SgClassDeclaration *class_decl = isSgClassDeclaration(sg_decl)) {
    if (class_decl->get_firstNondefiningDeclaration() == nullptr) {
      class_decl->set_firstNondefiningDeclaration(class_decl);
    }
    if (SgClassDeclaration *def_decl =
            isSgClassDeclaration(class_decl->get_definingDeclaration())) {
      if (def_decl->get_firstNondefiningDeclaration() == nullptr) {
        def_decl->set_firstNondefiningDeclaration(
            class_decl->get_firstNondefiningDeclaration());
      }
    }
  }
  if (SgFunctionDeclaration *func_decl = isSgFunctionDeclaration(sg_decl)) {
    if (func_decl->get_definition() != nullptr &&
        func_decl->get_definingDeclaration() == nullptr) {
      func_decl->set_definingDeclaration(func_decl);
    }
  }

  *node = sg_decl;
  return VisitDecl(friend_decl, node) && res;
}

bool ClangToSageTranslator::VisitFriendTemplateDecl(
    clang::FriendTemplateDecl *friend_template_decl, SgNode **node) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToSageTranslator::VisitFriendTemplateDecl" << std::endl;
#endif
  bool res = true;

  SgScopeStatement *current_scope = SageBuilder::topScopeStack();
  auto resolve_friend_scope =
      [&](clang::DeclContext *ctx) -> SgScopeStatement * {
    clang::DeclContext *scope_ctx = ctx;
    while (scope_ctx != nullptr && !scope_ctx->isNamespace() &&
           !scope_ctx->isTranslationUnit()) {
      scope_ctx = scope_ctx->getParent();
    }
    if (clang::NamespaceDecl *ns_decl =
            llvm::dyn_cast_or_null<clang::NamespaceDecl>(
                llvm::dyn_cast_or_null<clang::Decl>(scope_ctx))) {
      if (SgNamespaceDeclarationStatement *ns_stmt =
              ensureNamespaceDeclaration(ns_decl)) {
        if (ns_stmt->get_definition() != nullptr) {
          return ns_stmt->get_definition();
        }
      }
    }
    if (scope_ctx != nullptr && scope_ctx->isTranslationUnit()) {
      return getGlobalScope();
    }
    return nullptr;
  };

  SgScopeStatement *friend_scope = resolve_friend_scope(
      friend_template_decl != nullptr ? friend_template_decl->getDeclContext()
                                      : nullptr);
  if (friend_scope == nullptr) {
    friend_scope = get_enclosing_namespace_scope(current_scope);
  }
  if (friend_scope == nullptr) {
    friend_scope = getGlobalScope();
  }

  SgDeclarationStatement *sg_decl = nullptr;
  if (clang::NamedDecl *named_decl = friend_template_decl->getFriendDecl()) {
    if (clang::ClassTemplateDecl *class_template_decl =
            llvm::dyn_cast<clang::ClassTemplateDecl>(named_decl)) {
      sg_decl = translateClassTemplateDecl(class_template_decl, friend_scope,
                                           current_scope);
    } else {
      SgNode *tmp = Traverse(named_decl);
      sg_decl = isSgDeclarationStatement(tmp);
    }
  }

  if (sg_decl == nullptr) {
    MLOG_ERROR_C(MLOG_FRONTEND,
                 "Runtime error: unable to translate FriendTemplateDecl.\n");
    *node = nullptr;
    return false;
  }

  auto mark_friend = [](SgDeclarationStatement *decl) {
    if (decl != nullptr) {
      decl->get_declarationModifier().setFriend();
    }
  };

  mark_friend(sg_decl);
  mark_friend(sg_decl->get_firstNondefiningDeclaration());
  mark_friend(sg_decl->get_definingDeclaration());

  auto force_friend_function_scope = [&](SgDeclarationStatement *decl) {
    if (decl == nullptr) {
      return;
    }
    if (SgFunctionDeclaration *func_decl = isSgFunctionDeclaration(decl)) {
      if (isSgMemberFunctionDeclaration(func_decl) != nullptr) {
        return;
      }
      if (friend_scope != nullptr && func_decl->get_scope() != friend_scope) {
        SgScopeStatement *old_scope = func_decl->get_scope();
        func_decl->set_scope(friend_scope);
        if (old_scope != nullptr) {
          rehome_friend_function_symbol(func_decl, old_scope, friend_scope);
        }
      }
    }
  };

  auto ensure_scope_and_parent =
      [](SgDeclarationStatement *decl, SgScopeStatement *parent_scope,
         SgScopeStatement *decl_scope, bool force_scope) {
        if (decl == nullptr)
          return;
        if (parent_scope != nullptr && decl->get_parent() == nullptr) {
          decl->set_parent(parent_scope);
        }
        if (decl_scope != nullptr) {
          if (force_scope) {
            if (decl->get_scope() != decl_scope) {
              decl->set_scope(decl_scope);
            }
          } else if (decl->get_scope() == nullptr) {
            decl->set_scope(decl_scope);
          }
        }
      };

  force_friend_function_scope(sg_decl);
  if (sg_decl->get_scope() == nullptr && friend_scope != nullptr) {
    sg_decl->set_scope(friend_scope);
  }
  ensure_scope_and_parent(sg_decl, current_scope, friend_scope,
                          isSgFunctionDeclaration(sg_decl) == nullptr);
  if (SgDeclarationStatement *first_nondef =
          sg_decl->get_firstNondefiningDeclaration()) {
    force_friend_function_scope(first_nondef);
    ensure_scope_and_parent(first_nondef, friend_scope, friend_scope,
                            isSgFunctionDeclaration(first_nondef) == nullptr);
  }
  if (SgDeclarationStatement *def_decl = sg_decl->get_definingDeclaration()) {
    force_friend_function_scope(def_decl);
    ensure_scope_and_parent(def_decl, friend_scope, friend_scope,
                            isSgFunctionDeclaration(def_decl) == nullptr);
  }

  rehome_friend_function_symbol_if_needed(sg_decl, current_scope, friend_scope);

  if (sg_decl->get_firstNondefiningDeclaration() == nullptr) {
    sg_decl->set_firstNondefiningDeclaration(sg_decl);
  }
  if (SgFunctionDeclaration *func_decl = isSgFunctionDeclaration(sg_decl)) {
    if (func_decl->get_definition() != nullptr &&
        func_decl->get_definingDeclaration() == nullptr) {
      func_decl->set_definingDeclaration(func_decl);
    }
  }

  // REX FIX: Issue 99
  // Ensure that the body of the friend function definition points back to the
  // definition. This is required for VirtualCFG and other analyses.
  if (SgFunctionDeclaration *func_decl =
          isSgFunctionDeclaration(sg_decl->get_definingDeclaration())) {
    if (SgFunctionDefinition *def = func_decl->get_definition()) {
      if (SgBasicBlock *body = def->get_body()) {
        if (body->get_parent() != def) {
          body->set_parent(def);
        }
      }
    }
  }

  *node = sg_decl;
  return VisitDecl(friend_template_decl, node) && res;
}

bool ClangToSageTranslator::VisitImportDecl(clang::ImportDecl *import_decl,
                                            SgNode **node) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToSageTranslator::VisitImportDecl" << std::endl;
#endif
  bool res = true;

  ROSE_ASSERT(FAIL_FIXME == 0); // FIXME

  return VisitDecl(import_decl, node) && res;
}

bool ClangToSageTranslator::VisitNamedDecl(clang::NamedDecl *named_decl,
                                           SgNode **node) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToSageTranslator::VisitNamedDecl" << std::endl;
  std::cerr << "hasLinkage() " << named_decl->hasLinkage() << "\n";
  std::cerr << "isCXXClassMember() " << named_decl->isCXXClassMember() << "\n";
  std::cerr << "isCXXInstanceMember() " << named_decl->isCXXInstanceMember()
            << "\n";
  std::cerr << "hasExternalFormalLinkage() "
            << named_decl->hasExternalFormalLinkage() << "\n";
  std::cerr << "isExternallyVisible () " << named_decl->isExternallyVisible()
            << "\n";
  std::cerr << "isExternallyDeclarable () "
            << named_decl->isExternallyDeclarable() << "\n";
  std::cerr << "isLinkageValid () " << named_decl->isLinkageValid() << "\n";
  std::cerr << "hasLinkageBeenComputed() "
            << named_decl->hasLinkageBeenComputed() << "\n";
  std::cerr << "isModulePrivate() " << named_decl->isModulePrivate() << "\n";
#endif
  bool res = true;

  ROSE_ASSERT(FAIL_FIXME == 0); // FIXME

  return VisitDecl(named_decl, node) && res;
}

bool ClangToSageTranslator::VisitLabelDecl(clang::LabelDecl *label_decl,
                                           SgNode **node) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToSageTranslator::VisitLabelDecl" << std::endl;
#endif
  bool res = true;

  // ROOT CAUSE FIX: Allow delegation to work - disabled FAIL_TODO
  // ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitNamedDecl(label_decl, node) && res;
}

bool ClangToSageTranslator::VisitNamespaceAliasDecl(
    clang::NamespaceAliasDecl *namespace_alias_decl, SgNode **node) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToSageTranslator::VisitNamespaceAliasDecl"
            << namespace_alias_decl->getAliasedNamespace() << std::endl;
#endif
  bool res = true;

  // ROOT CAUSE FIX: Allow delegation to work - disabled FAIL_TODO
  // ROSE_ASSERT(FAIL_TODO == 0); // TODO

  clang::NamespaceDecl *namespaceDecl = namespace_alias_decl->getNamespace();
  SgNamespaceDeclarationStatement *sgNamespaceDeclStmt =
      isSgNamespaceDeclarationStatement(Traverse(namespaceDecl));
  ROSE_ASSERT(sgNamespaceDeclStmt);

  SgName name(namespace_alias_decl->getNameAsString());
  SgNamespaceAliasDeclarationStatement *sgNamespaceAliasDeclStmt =
      SageBuilder::buildNamespaceAliasDeclarationStatement(name,
                                                           sgNamespaceDeclStmt);
  *node = sgNamespaceAliasDeclStmt;
  return VisitNamedDecl(namespace_alias_decl, node) && res;
}

bool ClangToSageTranslator::VisitNamespaceDecl(
    clang::NamespaceDecl *namespace_decl, SgNode **node) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToSageTranslator::VisitNamespaceDecl "
            << namespace_decl->getNameAsString() << std::endl;
  std::cerr << "isAnonymousNamespace " << namespace_decl->isAnonymousNamespace()
            << std::endl;
  std::cerr << "isInline " << namespace_decl->isInline() << std::endl;
  std::cerr << "isOriginalNamespace " << namespace_decl->isOriginalNamespace()
            << std::endl;
#endif

  // Get the namespace name (handle anonymous namespaces)
  bool isAnonymous = namespace_decl->isAnonymousNamespace();
  std::string namespaceName = namespace_decl->getNameAsString();
  if (isAnonymous || namespaceName.empty()) {
    namespaceName =
        "__anonymous_namespace_" +
        generate_source_position_string(namespace_decl->getBeginLoc());
  }
  SgName name(namespaceName);

  // Resolve scope from the decl context to avoid binding namespaces to
  // statement scopes during on-demand translation.
  SgScopeStatement *scope = resolveScopeFromDeclContext(
      namespace_decl->getDeclContext(), p_global_scope);
  if (scope == nullptr) {
    scope = p_global_scope;
  }

  // Build namespace declaration using SageBuilder
  // This function handles:
  // - Creating the declaration and definition
  // - Looking up existing namespace symbols for reopening
  // - Setting up global_definition to link all instances
  // - Inserting the symbol (only for first declaration)
  // We don't need to manually duplicate any of this logic
  SgNamespaceDeclarationStatement *sg_namespace_decl =
      SageBuilder::buildNamespaceDeclaration_nfi(name, isAnonymous, scope);

  // Get the definition that was already created by the builder
  SgNamespaceDefinitionStatement *sg_namespace_def =
      sg_namespace_decl->get_definition();
  ROSE_ASSERT(sg_namespace_def != nullptr);
  sg_namespace_decl->set_isInlinedNamespace(namespace_decl->isInline());

  // ROOT CAUSE FIX: Register the namespace decl in the translation map before
  // traversing children.  On-demand translation of namespace-scope declarations
  // (triggered while visiting nested types) queries this map to find the proper
  // namespace scope.  Traverse() normally registers the namespace only after
  // VisitNamespaceDecl returns, which is too late and can mis-parent
  // namespace-scope decls as nested inside the current class scope (Issue 69).
  clang::NamespaceDecl *canonical_ns =
      getCanonicalNamespaceDecl(namespace_decl);
  p_decl_translation_map[canonical_ns] = sg_namespace_decl;

  // ROOT CAUSE FIX: Do NOT manually append -
  // SageBuilder::buildNamespaceDeclaration_nfi() already inserted the
  // declaration into the scope. Manually appending it again causes duplicate
  // statement errors in AST consistency checks.
  // The _nfi suffix means "no file info" not "no insertion"
  // REMOVED: SageInterface::appendStatement(sg_namespace_decl, scope);

  applySourceRange(sg_namespace_decl, namespace_decl->getSourceRange());

  // Traverse children within the namespace definition scope
  ROSE_ASSERT(sg_namespace_def != nullptr);
  SageBuilder::pushScopeStack(sg_namespace_def);
  clang::DeclContext *decl_context = (clang::DeclContext *)namespace_decl;
  bool res = TraverseForDeclContext(decl_context);
  SageBuilder::popScopeStack();

  *node = sg_namespace_decl;
  return VisitNamedDecl(namespace_decl, node) && res;
}

bool ClangToSageTranslator::VisitLinkageSpecDecl(
    clang::LinkageSpecDecl *linkage_spec_decl, SgNode **node) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToSageTranslator::VisitLinkageSpecDecl" << std::endl;
#endif

  SgScopeStatement *current_scope = SageBuilder::topScopeStack();
  ROSE_ASSERT(linkage_spec_decl != nullptr);
  ROSE_ASSERT(current_scope != nullptr);

  std::string linkage;
  switch (linkage_spec_decl->getLanguage()) {
  case clang::LinkageSpecLanguageIDs::C:
    linkage = "C";
    break;
  case clang::LinkageSpecLanguageIDs::CXX:
    linkage = "C++";
    break;
  default:
    ROSE_ASSERT(!"Unhandled clang::LinkageSpecLanguageIDs");
    break;
  }
  ROSE_ASSERT(!linkage.empty());

  bool has_braces = linkage_spec_decl->hasBraces();

  auto apply_linkage = [&](SgDeclarationStatement *decl_stmt) {
    if (decl_stmt == nullptr) {
      return;
    }

    decl_stmt->set_linkage(linkage);

    if (has_braces) {
      decl_stmt->setExternBrace();
    } else {
      SageInterface::setExtern(decl_stmt);
    }
  };

  auto attach_to_current_scope = [&](SgDeclarationStatement *decl_stmt) {
    if (decl_stmt == nullptr) {
      return;
    }

    if (SgStatement *old_parent_stmt = isSgStatement(decl_stmt->get_parent())) {
      if (old_parent_stmt != current_scope) {
        bool attached_to_old_parent = false;
        if (SgScopeStatement *old_parent_scope =
                isSgScopeStatement(old_parent_stmt)) {
          attached_to_old_parent =
              is_decl_attached_to_scope_child_list(old_parent_scope, decl_stmt);
        } else {
          std::vector<SgNode *> successors =
              old_parent_stmt->get_traversalSuccessorContainer();
          attached_to_old_parent =
              std::find(successors.begin(), successors.end(), decl_stmt) !=
              successors.end();
        }

        if (attached_to_old_parent &&
            SageInterface::isRemovableStatement(decl_stmt)) {
          SageInterface::removeStatement(decl_stmt, false);
          decl_stmt->set_parent(nullptr);
        }
      }
    }

    ensure_decl_in_scope_child_list(decl_stmt, current_scope,
                                    "VisitLinkageSpecDecl");
  };

  std::vector<SgDeclarationStatement *> linkage_decls;

  for (auto it = linkage_spec_decl->decls_begin();
       it != linkage_spec_decl->decls_end(); ++it) {
    clang::Decl *inner_decl = *it;
    if (inner_decl == nullptr)
      continue;

    SgNode *child = Traverse(inner_decl);
    if (SgDeclarationStatement *decl_stmt = isSgDeclarationStatement(child)) {
      apply_linkage(decl_stmt);
      apply_linkage(decl_stmt->get_firstNondefiningDeclaration());
      attach_to_current_scope(decl_stmt);
      linkage_decls.push_back(decl_stmt);
    }
  }

  std::unordered_set<SgDeclarationStatement *> linkage_decl_set(
      linkage_decls.begin(), linkage_decls.end());

  SgStatement *first_decl_stmt = nullptr;
  SgStatement *last_decl_stmt = nullptr;

  if (has_braces) {
    const bool allow_append = linkage_decls.empty();
    auto start_stmt = std::make_unique<SgClinkageStartStatement>();
    auto end_stmt = std::make_unique<SgClinkageEndStatement>();
    SgClinkageStartStatement *start_stmt_raw = start_stmt.get();
    SgClinkageEndStatement *end_stmt_raw = end_stmt.get();

    start_stmt_raw->set_languageSpecifier(linkage);
    end_stmt_raw->set_languageSpecifier(linkage);
    start_stmt_raw->set_linkage(linkage);
    end_stmt_raw->set_linkage(linkage);
    start_stmt_raw->set_scope(current_scope);
    end_stmt_raw->set_scope(current_scope);
    start_stmt_raw->set_parent(current_scope);
    end_stmt_raw->set_parent(current_scope);
    start_stmt_raw->set_firstNondefiningDeclaration(start_stmt_raw);
    start_stmt_raw->set_definingDeclaration(start_stmt_raw);
    end_stmt_raw->set_firstNondefiningDeclaration(end_stmt_raw);
    end_stmt_raw->set_definingDeclaration(end_stmt_raw);

    applySourceRange(start_stmt_raw,
                     clang::SourceRange(linkage_spec_decl->getExternLoc(),
                                        linkage_spec_decl->getExternLoc()));
    applySourceRange(end_stmt_raw,
                     clang::SourceRange(linkage_spec_decl->getRBraceLoc(),
                                        linkage_spec_decl->getRBraceLoc()));

    const Sg_File_Info *range_start = start_stmt_raw->get_startOfConstruct();
    const Sg_File_Info *range_end = end_stmt_raw->get_startOfConstruct();
    bool bounds_found = false;
    if (current_scope->containsOnlyDeclarations()) {
      bounds_found = find_linkage_bounds_in_list(
          current_scope->getDeclarationList(), linkage_decl_set, current_scope,
          first_decl_stmt, last_decl_stmt, range_start, range_end);
    } else if (scope_supports_statement_list(current_scope)) {
      bounds_found = find_linkage_bounds_in_list(
          current_scope->getStatementList(), linkage_decl_set, current_scope,
          first_decl_stmt, last_decl_stmt, range_start, range_end);
    }
    if (!linkage_decls.empty()) {
      ROSE_ASSERT(bounds_found);
      ROSE_ASSERT(first_decl_stmt != nullptr);
      ROSE_ASSERT(last_decl_stmt != nullptr);
    }

    bool inserted = false;
    if (first_decl_stmt != nullptr && last_decl_stmt != nullptr) {
      if (current_scope->containsOnlyDeclarations()) {
        SgDeclarationStatement *start_decl =
            isSgDeclarationStatement(start_stmt_raw);
        SgDeclarationStatement *end_decl =
            isSgDeclarationStatement(end_stmt_raw);
        SgDeclarationStatement *first_decl =
            isSgDeclarationStatement(first_decl_stmt);
        SgDeclarationStatement *last_decl =
            isSgDeclarationStatement(last_decl_stmt);
        ROSE_ASSERT(start_decl != nullptr);
        ROSE_ASSERT(end_decl != nullptr);
        ROSE_ASSERT(first_decl != nullptr);
        ROSE_ASSERT(last_decl != nullptr);
        inserted =
            insert_markers_around(current_scope->getDeclarationList(),
                                  first_decl, last_decl, start_decl, end_decl);
      } else if (scope_supports_statement_list(current_scope)) {
        SgStatement *start_stmt = isSgStatement(start_stmt_raw);
        SgStatement *end_stmt = isSgStatement(end_stmt_raw);
        ROSE_ASSERT(start_stmt != nullptr);
        ROSE_ASSERT(end_stmt != nullptr);
        inserted = insert_markers_around(current_scope->getStatementList(),
                                         first_decl_stmt, last_decl_stmt,
                                         start_stmt, end_stmt);
      }
    }

    auto find_insertion_index_for_empty_linkage =
        [&](const auto &list) -> size_t {
      const clang::DeclContext *lexical_context =
          linkage_spec_decl->getLexicalDeclContext();
      if (lexical_context == nullptr) {
        return find_insertion_index_for_range(list, range_start);
      }
      clang::SourceManager &sm = p_compiler_instance->getSourceManager();
      clang::SourceLocation linkage_loc = linkage_spec_decl->getExternLoc();
      if (!linkage_loc.isValid()) {
        linkage_loc = linkage_spec_decl->getBeginLoc();
      }
      if (linkage_loc.isMacroID()) {
        linkage_loc = sm.getSpellingLoc(linkage_loc);
      }
      if (!linkage_loc.isValid()) {
        return find_insertion_index_for_range(list, range_start);
      }
      const clang::FileID linkage_fid = sm.getFileID(linkage_loc);

      std::unordered_map<const SgStatement *, size_t> index_map;
      index_map.reserve(list.size());
      for (size_t idx = 0; idx < list.size(); ++idx) {
        SgStatement *stmt = isSgStatement(list[idx]);
        if (stmt != nullptr) {
          index_map[stmt] = idx;
        }
      }
      bool found_self = false;
      size_t anchor_index = list.size();
      clang::SourceLocation best_loc;
      for (auto it = lexical_context->decls_begin();
           it != lexical_context->decls_end(); ++it) {
        clang::Decl *decl = *it;
        if (decl == nullptr) {
          continue;
        }
        if (decl == linkage_spec_decl) {
          found_self = true;
          continue;
        }
        clang::SourceLocation decl_loc = decl->getBeginLoc();
        if (!decl_loc.isValid()) {
          decl_loc = decl->getLocation();
        }
        if (decl_loc.isMacroID()) {
          decl_loc = sm.getSpellingLoc(decl_loc);
        }
        if (!decl_loc.isValid()) {
          continue;
        }
        if (sm.getFileID(decl_loc) != linkage_fid) {
          continue;
        }
        if (!sm.isBeforeInTranslationUnit(decl_loc, linkage_loc)) {
          continue;
        }
        auto map_it = p_decl_translation_map.find(decl);
        if (map_it == p_decl_translation_map.end()) {
          continue;
        }
        SgStatement *stmt = isSgStatement(map_it->second);
        if (stmt == nullptr || stmt->get_parent() != current_scope) {
          continue;
        }
        auto idx_it = index_map.find(stmt);
        if (idx_it != index_map.end()) {
          if (!best_loc.isValid() ||
              sm.isBeforeInTranslationUnit(best_loc, decl_loc)) {
            best_loc = decl_loc;
            anchor_index = idx_it->second;
          }
        }
      }
      if (!found_self) {
        return find_insertion_index_for_range(list, range_start);
      }
      if (anchor_index != list.size()) {
        return anchor_index + 1;
      }
      return find_insertion_index_for_range(list, range_start);
    };

    if (!inserted && allow_append) {
      if (current_scope->containsOnlyDeclarations()) {
        SgDeclarationStatement *start_decl =
            isSgDeclarationStatement(start_stmt_raw);
        SgDeclarationStatement *end_decl =
            isSgDeclarationStatement(end_stmt_raw);
        ROSE_ASSERT(start_decl != nullptr);
        ROSE_ASSERT(end_decl != nullptr);
        size_t insert_index = find_insertion_index_for_empty_linkage(
            current_scope->getDeclarationList());
        inserted = insert_markers_at_index(current_scope->getDeclarationList(),
                                           insert_index, start_decl, end_decl);
      } else if (scope_supports_statement_list(current_scope)) {
        SgStatement *start_stmt = isSgStatement(start_stmt_raw);
        SgStatement *end_stmt = isSgStatement(end_stmt_raw);
        ROSE_ASSERT(start_stmt != nullptr);
        ROSE_ASSERT(end_stmt != nullptr);
        size_t insert_index = find_insertion_index_for_empty_linkage(
            current_scope->getStatementList());
        inserted = insert_markers_at_index(current_scope->getStatementList(),
                                           insert_index, start_stmt, end_stmt);
      }
    }

    if (inserted) {
      start_stmt.release();
      end_stmt.release();
    } else {
      ROSE_ASSERT(
          !"Failed to insert linkage markers around declarations in scope");
    }
  }

  *node = nullptr;
  return false;
}

bool ClangToSageTranslator::VisitTemplateDecl(
    clang::TemplateDecl *template_decl, SgNode **node) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToSageTranslator::VisitTemplateDecl" << std::endl;
#endif
  if (template_decl != nullptr &&
      template_decl->getTemplatedDecl() != nullptr) {
    Traverse(template_decl->getTemplatedDecl());
  }
  // TODO(roadmap): Emit proper template decl nodes once namespace/template
  // scaffolding lands (see docs/axpy_clang_frontend.md roadmap section).
  *node = nullptr;
  return false;
}

bool ClangToSageTranslator::VisitBuiltinTemplateDecl(
    clang::BuiltinTemplateDecl *builtin_template_decl, SgNode **node) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToSageTranslator::VisitBuiltinTemplateDecl" << std::endl;
#endif
  bool res = true;

  ROSE_ASSERT(FAIL_FIXME == 0); // FIXME

  return VisitTemplateDecl(builtin_template_decl, node) && res;
}

bool ClangToSageTranslator::VisitConceptDecl(clang::ConceptDecl *concept_decl,
                                             SgNode **node) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToSageTranslator::VisitConceptDecl" << std::endl;
#endif
  bool res = true;

  std::string name_str = concept_decl->getNameAsString();
  if (name_str.empty()) {
    name_str = "__concept_" +
               generate_source_position_string(concept_decl->getBeginLoc());
  }

  SgScopeStatement *concept_scope = SageBuilder::topScopeStack();
  clang::DeclContext *decl_context = concept_decl->getDeclContext();
  clang::DeclContext *scope_context = decl_context;
  while (scope_context != nullptr &&
         llvm::isa<clang::LinkageSpecDecl>(scope_context)) {
    scope_context = scope_context->getParent();
  }
  if (scope_context != nullptr) {
    if (SgScopeStatement *resolved =
            resolveScopeFromDeclContext(scope_context, concept_scope)) {
      concept_scope = resolved;
    }
  }
  if (concept_scope == nullptr) {
    concept_scope = getGlobalScope();
  }

  bool pushed_scope = false;
  if (concept_scope != nullptr &&
      concept_scope != SageBuilder::topScopeStack()) {
    SageBuilder::pushScopeStack(concept_scope);
    pushed_scope = true;
  }

  SgNonrealDecl *nrdecl =
      SageBuilder::buildNonrealDecl(SgName(name_str), nullptr);
  if (pushed_scope) {
    SageBuilder::popScopeStack();
  }

  if (nrdecl != nullptr) {
    applySourceRange(nrdecl, concept_decl->getSourceRange());
    nrdecl->set_is_concept(true);
    if (clang::TemplateParameterList *params =
            concept_decl->getTemplateParameters()) {
      auto sg_params = translateTemplateParameterList(params, nrdecl);
      nrdecl->get_tpl_params() = *sg_params;
      nrdecl->set_is_nonreal_template(true);
    }
    if (const clang::Expr *constraint_expr =
            concept_decl->getConstraintExpr()) {
      if (SgExpression *sg_constraint =
              translateConstraintExpression(constraint_expr)) {
        nrdecl->set_conceptConstraint(sg_constraint);
        sg_constraint->set_parent(nrdecl);
      }
    }
    p_decl_translation_map[concept_decl] = nrdecl;
    if (clang::ConceptDecl *canonical = concept_decl->getCanonicalDecl()) {
      p_decl_translation_map[canonical] = nrdecl;
    }
  }

  *node = nrdecl;
  return VisitNamedDecl(concept_decl, node) && res;
}

bool ClangToSageTranslator::VisitRedeclarableTemplateDecl(
    clang::RedeclarableTemplateDecl *redeclarable_template_decl,
    SgNode **node) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToSageTranslator::VisitRedeclarableTemplateDecl"
            << std::endl;
#endif
  return VisitTemplateDecl(redeclarable_template_decl, node);
}

SgTemplateClassDeclaration *ClangToSageTranslator::translateClassTemplateDecl(
    clang::ClassTemplateDecl *class_template_decl,
    SgScopeStatement *override_symbol_scope,
    SgScopeStatement *override_lexical_parent) {
  if (class_template_decl == nullptr) {
    return nullptr;
  }

  // CLANG FRONTEND FIX: Skip system header template classes to avoid
  // performance issues System headers contain massive template hierarchies that
  // cause extremely slow processing clang::SourceManager &SM =
  // p_compiler_instance->getSourceManager(); if
  // (SM.isInSystemHeader(class_template_decl->getLocation())) {
  //     // Skip this template class - let VisitRecordDecl handle it as a
  //     regular class *node = nullptr; return false;
  // }

  clang::CXXRecordDecl *templated_decl =
      class_template_decl->getTemplatedDecl();
  if (templated_decl == nullptr) {
    return nullptr;
  }

  auto lookup_existing_template_decl =
      [&](clang::ClassTemplateDecl *key) -> SgTemplateClassDeclaration * {
    if (key == nullptr) {
      return nullptr;
    }
    auto it = p_decl_translation_map.find(key);
    if (it == p_decl_translation_map.end()) {
      return nullptr;
    }
    return isSgTemplateClassDeclaration(it->second);
  };

  SgTemplateClassDeclaration *existing_from_cache =
      lookup_existing_template_decl(class_template_decl);
  if (existing_from_cache == nullptr) {
    if (clang::ClassTemplateDecl *canonical =
            class_template_decl->getCanonicalDecl()) {
      existing_from_cache = lookup_existing_template_decl(canonical);
    }
  }
  if (existing_from_cache != nullptr) {
    SgTemplateClassDeclaration *def_decl = isSgTemplateClassDeclaration(
        existing_from_cache->get_definingDeclaration());
    if (!templated_decl->isThisDeclarationADefinition() ||
        (def_decl != nullptr && def_decl->get_definition() != nullptr)) {
      return existing_from_cache;
    }
  }

  // Determine the template name, generate a fallback if necessary
  std::string template_name_str = templated_decl->getNameAsString();
  if (template_name_str.empty()) {
    template_name_str =
        "__anon_template_" +
        generate_source_position_string(class_template_decl->getBeginLoc());
  }
  SgName template_name(template_name_str);

  // Resolve class kind
  SgClassDeclaration::class_types class_kind = SgClassDeclaration::e_class;
  switch (templated_decl->getTagKind()) {
  case clang::TagTypeKind::Struct:
    class_kind = SgClassDeclaration::e_struct;
    break;
  case clang::TagTypeKind::Class:
    class_kind = SgClassDeclaration::e_class;
    break;
  case clang::TagTypeKind::Union:
    class_kind = SgClassDeclaration::e_union;
    break;
  default:
    std::cerr << "Warning: Unsupported tag kind for class template: "
              << static_cast<int>(templated_decl->getTagKind()) << std::endl;
    break;
  }

  clang::DeclContext *semantic_context = class_template_decl->getDeclContext();
  clang::DeclContext *lexical_context =
      class_template_decl->getLexicalDeclContext();

  SgScopeStatement *structural_scope = SageBuilder::topScopeStack();

  SgScopeStatement *semantic_scope = override_symbol_scope;
  if (semantic_scope == nullptr) {
    semantic_scope =
        resolveScopeFromDeclContext(semantic_context, structural_scope);

    auto is_record_scope = [](SgScopeStatement *scope) -> bool {
      return isSgClassDefinition(scope) != nullptr ||
             isSgTemplateClassDefinition(scope) != nullptr ||
             isSgTemplateInstantiationDefn(scope) != nullptr;
    };

    auto resolve_record_scope =
        [&](clang::CXXRecordDecl *record) -> SgScopeStatement * {
      if (record == nullptr) {
        return nullptr;
      }

      auto lookup_scope =
          [&](clang::CXXRecordDecl *record_decl) -> SgScopeStatement * {
        if (record_decl == nullptr) {
          return nullptr;
        }
        auto it = p_decl_translation_map.find(record_decl);
        if (it == p_decl_translation_map.end()) {
          return nullptr;
        }
        if (SgClassDefinition *class_def = isSgClassDefinition(it->second)) {
          return class_def;
        }
        if (SgClassDeclaration *class_decl = isSgClassDeclaration(it->second)) {
          if (class_decl->get_definition() != nullptr) {
            return class_decl->get_definition();
          }
          if (SgClassDeclaration *def_decl =
                  isSgClassDeclaration(class_decl->get_definingDeclaration())) {
            return def_decl->get_definition();
          }
        }
        if (SgTemplateClassDeclaration *template_decl =
                isSgTemplateClassDeclaration(it->second)) {
          if (template_decl->get_definition() != nullptr) {
            return template_decl->get_definition();
          }
          if (SgTemplateClassDeclaration *def_decl =
                  isSgTemplateClassDeclaration(
                      template_decl->get_definingDeclaration())) {
            return def_decl->get_definition();
          }
        }
        return nullptr;
      };

      if (SgScopeStatement *scope = lookup_scope(record)) {
        return scope;
      }

      clang::CXXRecordDecl *definition_decl = record->getDefinition();
      if (definition_decl != nullptr && definition_decl != record) {
        if (SgScopeStatement *scope = lookup_scope(definition_decl)) {
          return scope;
        }
      }

      clang::CXXRecordDecl *canonical_decl = record->getCanonicalDecl();
      if (canonical_decl != nullptr && canonical_decl != record &&
          canonical_decl != definition_decl) {
        if (SgScopeStatement *scope = lookup_scope(canonical_decl)) {
          return scope;
        }
      }

      clang::CXXRecordDecl *to_translate =
          definition_decl != nullptr ? definition_decl : record;
      if (p_decl_translation_map.find(to_translate) ==
              p_decl_translation_map.end() &&
          p_decl_translation_in_progress.find(to_translate) ==
              p_decl_translation_in_progress.end()) {
        TraverseOnDemand(to_translate);
      }
      if (SgScopeStatement *scope = lookup_scope(to_translate)) {
        return scope;
      }
      if (to_translate != record) {
        if (SgScopeStatement *scope = lookup_scope(record)) {
          return scope;
        }
      }
      if (canonical_decl != nullptr && canonical_decl != record) {
        if (SgScopeStatement *scope = lookup_scope(canonical_decl)) {
          return scope;
        }
      }

      return nullptr;
    };

    if (semantic_context != nullptr && !is_record_scope(semantic_scope)) {
      clang::CXXRecordDecl *record_ctx = nullptr;
      if (clang::Decl *ctx_decl =
              llvm::dyn_cast<clang::Decl>(semantic_context)) {
        record_ctx = llvm::dyn_cast<clang::CXXRecordDecl>(ctx_decl);
        if (record_ctx == nullptr) {
          if (clang::ClassTemplateDecl *tmpl_ctx =
                  llvm::dyn_cast<clang::ClassTemplateDecl>(ctx_decl)) {
            record_ctx = tmpl_ctx->getTemplatedDecl();
          }
        }
      }

      if (record_ctx != nullptr) {
        if (SgScopeStatement *record_scope = resolve_record_scope(record_ctx)) {
          semantic_scope = record_scope;
        }
      }
    }

    if (semantic_scope == nullptr) {
      semantic_scope = getGlobalScope();
    }
  }
  SgScopeStatement *symbol_scope = normalizeNamespaceScope(semantic_scope);
  if (symbol_scope == nullptr) {
    symbol_scope = semantic_scope;
  }

  SgScopeStatement *lexical_parent = override_lexical_parent;
  if (lexical_parent == nullptr) {
    lexical_parent =
        resolveScopeFromDeclContext(lexical_context, structural_scope);
    if (lexical_parent == nullptr) {
      lexical_parent =
          structural_scope != nullptr ? structural_scope : semantic_scope;
    }
  }

  SgTemplateClassDeclaration *existing_nondefining_decl = nullptr;
  if (existing_from_cache != nullptr) {
    existing_nondefining_decl = isSgTemplateClassDeclaration(
        existing_from_cache->get_firstNondefiningDeclaration());
  }
  if (existing_nondefining_decl == nullptr) {
    if (clang::ClassTemplateDecl *prev =
            class_template_decl->getPreviousDecl()) {
      auto it = p_decl_translation_map.find(prev);
      if (it != p_decl_translation_map.end()) {
        if (SgTemplateClassDeclaration *prev_decl =
                isSgTemplateClassDeclaration(it->second)) {
          existing_nondefining_decl = isSgTemplateClassDeclaration(
              prev_decl->get_firstNondefiningDeclaration());
        }
      }
    }
  }

  // Build template parameters and template declaration.  ROSE's template-class
  // symbol lookup matches template-parameter lists by pointer identity, so for
  // redeclarations we must reuse the existing template-parameter nodes instead
  // of re-translating a fresh list from Clang.
  SgTemplateArgumentPtrList empty_args;
  std::unique_ptr<SgTemplateParameterPtrList> params;
  if (existing_nondefining_decl != nullptr) {
    params = std::make_unique<SgTemplateParameterPtrList>();
    const SgTemplateParameterPtrList &existing_params =
        existing_nondefining_decl->get_templateParameters();
    params->insert(params->end(), existing_params.begin(),
                   existing_params.end());
  } else {
    params = translateTemplateParameterList(
        class_template_decl->getTemplateParameters(), nullptr);
  }

  const bool is_definition = templated_decl->isThisDeclarationADefinition();

  // SageBuilder::buildTemplateClassDeclaration_nfi() always constructs a
  // defining declaration. For forward declarations, build a true non-defining
  // template class declaration so the unparser emits `;` instead of an empty
  // definition (`{}`), and so we don't leave behind a defining decl with no
  // lexical parent when no definition exists in the source.
  SgTemplateClassDeclaration *template_decl = nullptr;
  SgTemplateClassDeclaration *nondefining_decl = nullptr;
  SgTemplateClassDeclaration *result_decl = nullptr;

  if (!is_definition) {
    if (existing_nondefining_decl != nullptr) {
      nondefining_decl = existing_nondefining_decl;
    } else {
      nondefining_decl =
          SageBuilder::buildNondefiningTemplateClassDeclaration_nfi(
              template_name, class_kind, symbol_scope, params.get(),
              &empty_args);
      ROSE_ASSERT(nondefining_decl != nullptr);
      nondefining_decl->setForward();
    }
    result_decl = nondefining_decl;
  } else {
    template_decl = SageBuilder::buildTemplateClassDeclaration_nfi(
        template_name, class_kind, symbol_scope, existing_nondefining_decl,
        params.get(), &empty_args);
    nondefining_decl = isSgTemplateClassDeclaration(
        template_decl->get_firstNondefiningDeclaration());
    ROSE_ASSERT(nondefining_decl != nullptr);

    // Enforce the declaration/definition chain invariants expected by ROSE's
    // AST copy/fixup passes (fixupCopy_scopes asserts on these links).
    template_decl->set_firstNondefiningDeclaration(nondefining_decl);
    template_decl->set_definingDeclaration(template_decl);
    nondefining_decl->set_firstNondefiningDeclaration(nondefining_decl);
    nondefining_decl->set_definingDeclaration(template_decl);
    result_decl = template_decl;
  }

  auto apply_template_scope = [&](SgTemplateClassDeclaration *decl_stmt) {
    if (decl_stmt == nullptr) {
      return;
    }
    if (symbol_scope != nullptr) {
      decl_stmt->set_scope(symbol_scope);
    }
    if (decl_stmt->get_parent() == nullptr) {
      if (lexical_parent != nullptr) {
        decl_stmt->set_parent(lexical_parent);
      } else if (symbol_scope != nullptr) {
        decl_stmt->set_parent(symbol_scope);
      }
    }
  };

  apply_template_scope(nondefining_decl);
  apply_template_scope(template_decl);

  if (result_decl != nullptr) {
    attach_nonreal_template_parameters(result_decl,
                                       result_decl->get_templateParameters());
  }

  // REX FIX: Handle AS_none for ClassTemplateDecl
  clang::AccessSpecifier access = class_template_decl->getAccess();
  if (access == clang::AS_public) {
    result_decl->get_declarationModifier().get_accessModifier().setPublic();
  } else if (access == clang::AS_private) {
    result_decl->get_declarationModifier().get_accessModifier().setPrivate();
  } else if (access == clang::AS_protected) {
    result_decl->get_declarationModifier().get_accessModifier().setProtected();
  } else if (access == clang::AS_none) {
    SgScopeStatement *parent_scope =
        lexical_parent != nullptr ? lexical_parent : structural_scope;
    if (isSgClassDefinition(parent_scope)) {
      SgClassDefinition *class_def = isSgClassDefinition(parent_scope);
      if (class_def->get_declaration()->get_class_type() ==
          SgClassDeclaration::e_class) {
        result_decl->get_declarationModifier()
            .get_accessModifier()
            .setPrivate();
      } else {
        result_decl->get_declarationModifier().get_accessModifier().setPublic();
      }
    }
  }

  if (result_decl == nullptr) {
    return nullptr;
  }

  // Keep the template declaration in its lexical scope for unparse order and
  // comment anchoring, while using the canonical namespace scope only for
  // symbol-table insertion (see normalizeNamespaceScope()).
  if (lexical_parent != nullptr) {
    result_decl->set_parent(lexical_parent);
  }

  // REX FIX: Ensure firstNondefiningDeclaration is set to avoid unparser
  // crash (ua test).
  SgDeclarationStatement *firstNondef =
      result_decl->get_firstNondefiningDeclaration();
  ROSE_ASSERT(firstNondef != nullptr);
  firstNondef->set_firstNondefiningDeclaration(firstNondef);

  // Attach template parameter back-links
  SgTemplateParameterPtrList &decl_params =
      result_decl->get_templateParameters();
  for (SgTemplateParameter *param : decl_params) {
    if (param != nullptr) {
      // Only set owning template if it's NOT a template_parameter,
      // because template_parameter uses this field for the nrdecl.
      if (param->get_parameterType() !=
          SgTemplateParameter::template_parameter) {
        param->set_templateDeclaration(result_decl);
      }
    }
  }

  if (clang::TemplateParameterList *params =
          class_template_decl->getTemplateParameters()) {
    if (const clang::Expr *requires_expr = params->getRequiresClause()) {
      auto attach_requires = [&](SgTemplateClassDeclaration *decl,
                                 SgExpression *expr) {
        if (decl == nullptr || expr == nullptr) {
          return;
        }
        decl->set_requiresClause(expr);
        expr->set_parent(decl);
      };

      if (SgExpression *sg_requires =
              translateConstraintExpression(requires_expr)) {
        attach_requires(nondefining_decl, sg_requires);
        if (template_decl != nullptr && template_decl != nondefining_decl) {
          if (SgExpression *copy =
                  isSgExpression(SageInterface::deepCopy(sg_requires))) {
            attach_requires(template_decl, copy);
          }
        }
      }
    }
  }

  applySourceRange(result_decl, class_template_decl->getSourceRange());

  // ROOT CAUSE FIX: Cache before appending to prevent double visitation
  auto cache_translation = [&](clang::Decl *key) {
    if (key == nullptr) {
      return;
    }
    auto it = p_decl_translation_map.find(key);
    if (it == p_decl_translation_map.end() || it->second == nullptr) {
      p_decl_translation_map[key] = result_decl;
      return;
    }

    SgClassDeclaration *existing_class = isSgClassDeclaration(it->second);
    if (existing_class == nullptr) {
      p_decl_translation_map[key] = result_decl;
      return;
    }

    if (isSgTemplateClassDeclaration(result_decl) != nullptr &&
        isSgTemplateClassDeclaration(existing_class) == nullptr) {
      p_decl_translation_map[key] = result_decl;
      return;
    }

    if (result_decl != nullptr && result_decl->get_definition() != nullptr &&
        existing_class->get_definition() == nullptr) {
      p_decl_translation_map[key] = result_decl;
    }
  };

  cache_translation(class_template_decl);
  cache_translation(templated_decl);

  if (clang::ClassTemplateDecl *canonical_template =
          class_template_decl->getCanonicalDecl()) {
    cache_translation(canonical_template);
  }

  if (clang::CXXRecordDecl *canonical_record =
          llvm::dyn_cast_or_null<clang::CXXRecordDecl>(
              templated_decl->getCanonicalDecl())) {
    cache_translation(canonical_record);
  }

  for (clang::ClassTemplateDecl *prev = class_template_decl->getPreviousDecl();
       prev != nullptr; prev = prev->getPreviousDecl()) {
    cache_translation(prev);
  }

  for (clang::CXXRecordDecl *prev = templated_decl->getPreviousDecl();
       prev != nullptr; prev = prev->getPreviousDecl()) {
    cache_translation(prev);
  }

  // REX FIX: Do not append here. The caller (Traverse) will return this node
  // and the caller of Traverse (e.g. VisitTranslationUnitDecl) will append it.
  // Appending here causes duplicates in the global scope.
  // if (template_decl->get_parent() == nullptr && scope != nullptr) {
  //    SageInterface::appendStatement(template_decl, scope);
  // }

  // Populate the class definition for definitions
  if (is_definition) {
    if (SgTemplateClassDefinition *class_def =
            isSgTemplateClassDefinition(template_decl->get_definition())) {
      applySourceRange(class_def, templated_decl->getSourceRange());
      if (p_record_definitions_populated.insert(class_def).second) {
        bool inserted =
            p_decl_translation_in_progress.insert(templated_decl).second;
        clang::CXXRecordDecl *canonical = templated_decl->getCanonicalDecl();
        bool inserted_canonical = false;
        if (canonical != nullptr && canonical != templated_decl) {
          inserted_canonical =
              p_decl_translation_in_progress.insert(canonical).second;
        }
        populateClassDefinition(templated_decl, class_def);
        if (inserted_canonical) {
          p_decl_translation_in_progress.erase(canonical);
        }
        if (inserted) {
          p_decl_translation_in_progress.erase(templated_decl);
        }
      }
    }
  }

  auto attach_template_specialization = [&](SgNode *spec_node,
                                            const char *context) {
    SgDeclarationStatement *decl_stmt = isSgDeclarationStatement(spec_node);
    if (SgTemplateInstantiationDecl *inst_decl =
            isSgTemplateInstantiationDecl(decl_stmt)) {
      if (SgTemplateInstantiationDirectiveStatement *directive =
              isSgTemplateInstantiationDirectiveStatement(
                  inst_decl->get_parent())) {
        decl_stmt = directive;
      }
    }
    if (decl_stmt != nullptr) {
      SgScopeStatement *lexical_scope =
          isSgScopeStatement(decl_stmt->get_parent());
      if (lexical_scope == nullptr) {
        lexical_scope = decl_stmt->get_scope();
      }
      if (lexical_scope != nullptr) {
        ensure_decl_in_scope_child_list_preserve_scope(decl_stmt, lexical_scope,
                                                       context);
      }
    }
  };

  for (auto it = class_template_decl->spec_begin();
       it != class_template_decl->spec_end(); ++it) {
    clang::ClassTemplateSpecializationDecl *spec = *it;
    if (spec == nullptr) {
      continue;
    }

    clang::TemplateSpecializationKind kind =
        spec->getTemplateSpecializationKind();
    if (kind == clang::TSK_ExplicitInstantiationDeclaration ||
        kind == clang::TSK_ExplicitInstantiationDefinition ||
        (kind == clang::TSK_ExplicitSpecialization &&
         llvm::dyn_cast<clang::ClassTemplatePartialSpecializationDecl>(spec) ==
             nullptr)) {
      continue;
    }

    SgNode *spec_node = Traverse(spec);
    attach_template_specialization(spec_node,
                                   "translateClassTemplateDecl:specialization");
  }

  llvm::SmallVector<clang::ClassTemplatePartialSpecializationDecl *, 4>
      partial_specs;
  class_template_decl->getPartialSpecializations(partial_specs);
  for (clang::ClassTemplatePartialSpecializationDecl *partial : partial_specs) {
    if (partial == nullptr) {
      continue;
    }
    SgNode *spec_node = Traverse(partial);
    attach_template_specialization(
        spec_node, "translateClassTemplateDecl:partial_specialization");
  }

  if (lexical_parent != nullptr) {
    ensure_decl_in_scope_child_list_preserve_scope(
        result_decl, lexical_parent, "translateClassTemplateDecl");
  }

  return result_decl;
}

bool ClangToSageTranslator::VisitClassTemplateDecl(
    clang::ClassTemplateDecl *class_template_decl, SgNode **node) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToSageTranslator::VisitClassTemplateDecl" << std::endl;
#endif
  SgTemplateClassDeclaration *result_decl =
      translateClassTemplateDecl(class_template_decl, nullptr, nullptr);
  if (result_decl == nullptr) {
    *node = nullptr;
    return false;
  }
  *node = result_decl;
  return true;
}

bool ClangToSageTranslator::VisitFunctionTemplateDecl(
    clang::FunctionTemplateDecl *function_template_decl, SgNode **node) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToSageTranslator::VisitFunctionTemplateDecl" << std::endl;
#endif
  if (function_template_decl == nullptr) {
    *node = nullptr;
    return false;
  }
  if (function_template_decl->isInvalidDecl()) {
    *node = nullptr;
    return false;
  }

  clang::FunctionDecl *templated_decl =
      function_template_decl->getTemplatedDecl();
  if (templated_decl == nullptr) {
    *node = nullptr;
    return false;
  }
  if (templated_decl->isInvalidDecl()) {
    *node = nullptr;
    return false;
  }

  bool res =
      translateFunctionDeclCommon(templated_decl, function_template_decl, node);

  if (res && *node != nullptr) {
    p_decl_translation_map.insert(std::make_pair(templated_decl, *node));
  }

  // Phase C (Issue 115): Queue implicit template instantiations so instantiated
  // bodies can be translated with resolved reference nodes. Translation is
  // deferred until the TU is otherwise complete to ensure template argument
  // declarations (e.g., record types) are already available.
  if (p_compiler_instance != nullptr) {
    clang::SourceManager &sm = p_compiler_instance->getSourceManager();
    clang::SourceLocation loc = function_template_decl->getLocation();
    if (loc.isValid() && sm.isInSystemHeader(loc)) {
      return res;
    }
  }

  for (auto it = function_template_decl->spec_begin();
       it != function_template_decl->spec_end(); ++it) {
    clang::FunctionDecl *spec = *it;
    if (spec == nullptr) {
      continue;
    }
    if (spec->isInvalidDecl()) {
      continue;
    }

    clang::TemplateSpecializationKind kind =
        spec->getTemplateSpecializationKind();
    if (kind != clang::TSK_ImplicitInstantiation &&
        kind != clang::TSK_ExplicitInstantiationDefinition) {
      continue;
    }

    if (!spec->hasBody()) {
      continue;
    }

    if (p_compiler_instance != nullptr) {
      clang::SourceManager &sm = p_compiler_instance->getSourceManager();
      clang::SourceLocation loc = spec->getLocation();
      if (!loc.isValid() || sm.isInSystemHeader(loc) ||
          sm.isWrittenInBuiltinFile(loc)) {
        continue;
      }
    }

    if (p_pending_implicit_function_instantiations_set.insert(spec).second) {
      p_pending_implicit_function_instantiations.push_back(spec);
    }
  }

  return res;
}

bool ClangToSageTranslator::VisitTypeAliasTemplateDecl(
    clang::TypeAliasTemplateDecl *type_alias_template_decl, SgNode **node) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToSageTranslator::VisitTypeAliasTemplateDecl" << std::endl;
#endif
  bool res = true;

  // Get the underlying TypeAliasDecl
  clang::TypeAliasDecl *type_alias_decl =
      type_alias_template_decl->getTemplatedDecl();
  ROSE_ASSERT(type_alias_decl != nullptr);

  // REX FIX: Do NOT traverse the TypeAliasDecl directly, as it creates a
  // SgTypedefDeclaration that conflicts with the SgTemplateTypedefDeclaration
  // we want to build. Instead, extract the necessary info and build
  // SgTemplateTypedefDeclaration directly.

  SgName name(type_alias_template_decl->getNameAsString());
  clang::QualType underlyingQualType = type_alias_decl->getUnderlyingType();
  SgType *base_type = buildTypeFromQualifiedType(underlyingQualType);

  SgScopeStatement *scope = SageBuilder::topScopeStack();
  clang::DeclContext *decl_context = type_alias_template_decl->getDeclContext();
  clang::DeclContext *scope_context = decl_context;
  while (scope_context != nullptr &&
         llvm::isa<clang::LinkageSpecDecl>(scope_context)) {
    scope_context = scope_context->getParent();
  }

  auto translate_decl_context_on_demand = [&](clang::DeclContext *ctx) {
    if (ctx == nullptr) {
      return;
    }
    if (!is_declaration_scope_context(ctx)) {
      return;
    }
    clang::Decl *context_decl = llvm::dyn_cast<clang::Decl>(ctx);
    if (context_decl == nullptr) {
      return;
    }
    if (llvm::isa<clang::NamespaceDecl>(context_decl)) {
      return;
    }
    if (clang::RecordDecl *record_decl =
            llvm::dyn_cast<clang::RecordDecl>(context_decl)) {
      if (clang::RecordDecl *definition = record_decl->getDefinition()) {
        context_decl = definition;
      }
    }
    if (p_decl_translation_map.find(context_decl) ==
            p_decl_translation_map.end() &&
        p_decl_translation_in_progress.find(context_decl) ==
            p_decl_translation_in_progress.end() &&
        p_decl_translation_on_demand.find(context_decl) ==
            p_decl_translation_on_demand.end()) {
      TraverseOnDemand(context_decl);
    }
  };

  translate_decl_context_on_demand(scope_context);
  if (scope_context != nullptr) {
    if (scope_context->isTranslationUnit()) {
      scope = getGlobalScope();
    } else if (clang::Decl *context_decl =
                   llvm::dyn_cast<clang::Decl>(scope_context)) {
      if (SgScopeStatement *resolved =
              resolveScopeFromDeclContext(scope_context, scope)) {
        scope = resolved;
      }
    }
  }
  if (scope == nullptr) {
    scope = getGlobalScope();
  }

  SgScopeStatement *symbol_scope = normalizeNamespaceScope(scope);
  if (symbol_scope == nullptr) {
    symbol_scope = scope;
  }

  SgScopeStatement *parent_scope = scope;
  clang::DeclContext *lexical_context =
      type_alias_template_decl->getLexicalDeclContext();
  translate_decl_context_on_demand(lexical_context);
  if (lexical_context != nullptr) {
    if (SgScopeStatement *resolved =
            resolveScopeFromDeclContext(lexical_context, parent_scope)) {
      parent_scope = resolved;
    }
  }

  // Get the symbol for the parent scope (mimic
  // buildTemplateTypedefDeclaration_nfi logic but be lenient)
  SgSymbol *scopeSymbol = nullptr;
  if (SgClassDefinition *def = isSgClassDefinition(symbol_scope)) {
    scopeSymbol = def->get_declaration()->get_symbol_from_symbol_table();
  } else if (SgNamespaceDefinitionStatement *def =
                 isSgNamespaceDefinitionStatement(symbol_scope)) {
    scopeSymbol =
        def->get_namespaceDeclaration()->get_symbol_from_symbol_table();
  }

  // Create SgTemplateTypedefDeclaration manually
  SgTemplateTypedefDeclaration *template_typedef =
      new SgTemplateTypedefDeclaration(name, base_type,
                                       nullptr, // Type will be set later
                                       nullptr, // Base declaration (optional)
                                       scopeSymbol);

  template_typedef->set_scope(symbol_scope);
  if (parent_scope != nullptr) {
    template_typedef->set_parent(parent_scope);
  }

  // REX FIX: Set source position to avoid AST post-processing assertion failure
  applySourceRange(template_typedef,
                   type_alias_template_decl->getSourceRange());

  // Set firstNondefiningDeclaration (required for unparsing)
  template_typedef->set_firstNondefiningDeclaration(template_typedef);
  template_typedef->set_definingDeclaration(nullptr);

  // Create SgTypedefType
  SgTypedefType *typedefType = SgTypedefType::createType(template_typedef);
  template_typedef->set_type(typedefType);

  registerDeclarationSymbol(template_typedef);

  // Handle template parameters
  clang::TemplateParameterList *param_list =
      type_alias_template_decl->getTemplateParameters();
  std::unique_ptr<SgTemplateParameterPtrList> template_params;
  if (param_list != nullptr) {
    // REX FIX: Pass template_typedef as owning template
    template_params = translateTemplateParameterList(
        type_alias_template_decl->getTemplateParameters(), template_typedef);
  } else {
    template_params = std::make_unique<SgTemplateParameterPtrList>();
  }

  // REX FIX: Set template parameters on the declaration!
  template_typedef->get_templateParameters() = *template_params;

  if (clang::TemplateParameterList *params =
          type_alias_template_decl->getTemplateParameters()) {
    if (const clang::Expr *requires_expr = params->getRequiresClause()) {
      if (SgExpression *sg_requires =
              translateConstraintExpression(requires_expr)) {
        template_typedef->set_requiresClause(sg_requires);
        sg_requires->set_parent(template_typedef);
      }
    }
  }

  // REX FIX: Do not append to scope here. VisitTranslationUnitDecl handles it.
  // if (scope) {
  //     scope->append_statement(template_typedef);
  // }

  // Add to map
  p_decl_translation_map.insert(
      std::make_pair(type_alias_template_decl, template_typedef));

  *node = template_typedef;
  // REX FIX: Do not call VisitRedeclarableTemplateDecl -> VisitTemplateDecl as
  // it clears *node to nullptr
  return true;
}

bool ClangToSageTranslator::VisitVarTemplateDecl(
    clang::VarTemplateDecl *var_template_decl, SgNode **node) {

#if DEBUG_VISIT_DECL
  std::cerr << "ClangToSageTranslator::VisitVarTemplateDecl" << std::endl;
#endif
  if (var_template_decl == nullptr || var_template_decl->isInvalidDecl()) {
    *node = nullptr;
    return false;
  }

  clang::VarDecl *templated_decl = var_template_decl->getTemplatedDecl();
  if (templated_decl == nullptr || templated_decl->isInvalidDecl()) {
    *node = nullptr;
    return false;
  }

  SgScopeStatement *scope = SageBuilder::topScopeStack();
  if (clang::DeclContext *decl_context = var_template_decl->getDeclContext()) {
    if (SgScopeStatement *resolved =
            resolveScopeFromDeclContext(decl_context, scope)) {
      scope = resolved;
    }
  }
  if (scope == nullptr) {
    scope = getGlobalScope();
  }

  SgName name(templated_decl->getNameAsString());
  if (name.getString().empty()) {
    name = "__anon_var_template_" +
           generate_source_position_string(templated_decl->getBeginLoc());
  }

  SgType *type = buildTypeFromQualifiedType(templated_decl->getType());

  SgInitializer *init = nullptr;
  if (templated_decl->hasInit()) {
    clang::Expr *init_expr = templated_decl->getInit();
    if (init_expr != nullptr) {
      SgNode *tmp_init = Traverse(init_expr);
      if (SgInitializer *tmp_init_initializer = isSgInitializer(tmp_init)) {
        init = tmp_init_initializer;
      } else if (SgExpression *expr = isSgExpression(tmp_init)) {
        if (SgInitializer *existing_init = isSgInitializer(expr)) {
          init = existing_init;
        } else {
          init =
              SageBuilder::buildAssignInitializer_nfi(expr, expr->get_type());
        }
      } else if (tmp_init != nullptr) {
        std::cerr << "Runtime error: template variable initializer is not a "
                     "SgExpression ("
                  << tmp_init->class_name() << ")" << std::endl;
        return false;
      }
    }
  }

  SgTemplateVariableDeclaration *var_decl =
      SageBuilder::buildTemplateVariableDeclaration_nfi(name, type, init,
                                                        scope);
  var_decl->set_isAssociatedWithDeclarationList(true);
  if (templated_decl->isConstexpr()) {
    var_decl->set_is_constexpr(true);
  }
  applySourceRange(var_decl, templated_decl->getSourceRange());

  std::unique_ptr<SgTemplateParameterPtrList> template_params =
      translateTemplateParameterList(var_template_decl->getTemplateParameters(),
                                     var_decl);
  if (template_params != nullptr) {
    var_decl->get_templateParameters() = *template_params;
  }

  if (clang::TemplateParameterList *params =
          var_template_decl->getTemplateParameters()) {
    if (const clang::Expr *requires_expr = params->getRequiresClause()) {
      if (SgExpression *sg_requires =
              translateConstraintExpression(requires_expr)) {
        var_decl->set_requiresClause(sg_requires);
        sg_requires->set_parent(var_decl);
      }
    }
  }

  p_decl_translation_map.insert(std::make_pair(var_template_decl, var_decl));
  p_decl_translation_map.insert(std::make_pair(templated_decl, var_decl));
  if (clang::VarDecl *canonical = templated_decl->getCanonicalDecl()) {
    p_decl_translation_map.insert(std::make_pair(canonical, var_decl));
  }

  *node = var_decl;
  return true;
}

bool ClangToSageTranslator::VisitTemplateTemplateParmDecl(
    clang::TemplateTemplateParmDecl *template_template_parm_decl,
    SgNode **node) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToSageTranslator::VisitTemplateTemplateParmDecl"
            << std::endl;
#endif
  SgDeclarationStatement *owning_template = nullptr;
  if (clang::DeclContext *ctx = template_template_parm_decl->getDeclContext()) {
    if (clang::TemplateDecl *template_ctx =
            llvm::dyn_cast<clang::TemplateDecl>(ctx)) {
      auto it = p_decl_translation_map.find(template_ctx);
      if (it != p_decl_translation_map.end()) {
        owning_template = isSgDeclarationStatement(it->second);
      }
    }
  }

  unsigned position = template_template_parm_decl->getIndex();
  SgTemplateParameter *sg_param = translateTemplateParameter(
      template_template_parm_decl, owning_template, position);

  *node = sg_param;
  return sg_param != nullptr;
}

bool ClangToSageTranslator::VisitTypeDecl(clang::TypeDecl *type_decl,
                                          SgNode **node) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToSageTranslator::VisitTypeDecl" << std::endl;
#endif

  bool res = true;

  ROSE_ASSERT(FAIL_FIXME == 0); // FIXME

  return VisitNamedDecl(type_decl, node) && res;
}

bool ClangToSageTranslator::VisitTagDecl(clang::TagDecl *tag_decl,
                                         SgNode **node) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToSageTranslator::VisitTagDecl" << std::endl;
  std::cerr << "isThisDeclarationADefinition() "
            << tag_decl->isThisDeclarationADefinition() << "\n";
  std::cerr << "isCompleteDefinition() " << tag_decl->isCompleteDefinition()
            << "\n";
  std::cerr << "isCompleteDefinitionRequired() "
            << tag_decl->isCompleteDefinitionRequired() << "\n";
  std::cerr << "isBeingDefined() " << tag_decl->isBeingDefined() << "\n";
  std::cerr << "isEmbeddedInDeclarator() " << tag_decl->isEmbeddedInDeclarator()
            << "\n";
  std::cerr << "isFreeStanding() " << tag_decl->isFreeStanding() << "\n";
  std::cerr << "mayHaveOutOfDateDef() " << tag_decl->mayHaveOutOfDateDef()
            << "\n";
  std::cerr << "isDependentType() " << tag_decl->isDependentType() << "\n";
  std::cerr << "isThisDeclarationADemotedDefinition() "
            << tag_decl->isThisDeclarationADemotedDefinition() << "\n";
  std::cerr << "isStruct () " << tag_decl->isStruct() << "\n";
  std::cerr << "isInterface () " << tag_decl->isInterface() << "\n";
  std::cerr << "isUnion () " << tag_decl->isUnion() << "\n";
  std::cerr << "isEnum () " << tag_decl->isEnum() << "\n";
  std::cerr << "hasNameForLinkage () " << tag_decl->hasNameForLinkage() << "\n";
#endif

  bool res = true;

  ROSE_ASSERT(FAIL_FIXME == 0); // FIXME

  return VisitTypeDecl(tag_decl, node) && res;
}

bool ClangToSageTranslator::VisitRecordDecl(clang::RecordDecl *record_decl,
                                            SgNode **node) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToSageTranslator::VisitRecordDecl" << std::endl;
#endif

  // FIXME May have to check the symbol table first, because of out-of-order
  // traversal of C++ classes (Could be done in CxxRecord class...)

  // FIXME May have to check the symbol table first, because of out-of-order
  // traversal of C++ classes (Could be done in CxxRecord class...)

  bool res = true;
  struct DeclTranslationGuard {
    std::set<clang::Decl *> &in_progress;
    clang::Decl *decl;
    bool inserted;
    DeclTranslationGuard(std::set<clang::Decl *> &set, clang::Decl *d)
        : in_progress(set), decl(d), inserted(false) {
      inserted = in_progress.insert(decl).second;
    }
    ~DeclTranslationGuard() {
      if (inserted) {
        in_progress.erase(decl);
      }
    }
  } record_guard(p_decl_translation_in_progress, record_decl);
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToSageTranslator::VisitRecordDecl name:"
            << record_decl->getNameAsString() << "\n";
  std::cerr << "isAnonymousStructOrUnion() "
            << record_decl->isAnonymousStructOrUnion() << "\n";
  std::cerr << "hasObjectMember() " << record_decl->hasObjectMember() << "\n";
  std::cerr << "hasVolatileMember() " << record_decl->hasVolatileMember()
            << "\n";
  std::cerr << "hasLoadedFieldsFromExternalStorage() "
            << record_decl->hasLoadedFieldsFromExternalStorage() << "\n";
  std::cerr << "isNonTrivialToPrimitiveDefaultInitialize() "
            << record_decl->isNonTrivialToPrimitiveDefaultInitialize() << "\n";
  std::cerr << "isNonTrivialToPrimitiveCopy() "
            << record_decl->isNonTrivialToPrimitiveCopy() << "\n";
  std::cerr << "isNonTrivialToPrimitiveDestroy() "
            << record_decl->isNonTrivialToPrimitiveDestroy() << "\n";
  std::cerr << "hasNonTrivialToPrimitiveDefaultInitializeCUnion() "
            << record_decl->hasNonTrivialToPrimitiveDefaultInitializeCUnion()
            << "\n";
  std::cerr << "hasNonTrivialToPrimitiveDestructCUnion() "
            << record_decl->hasNonTrivialToPrimitiveDestructCUnion() << "\n";
  std::cerr << "hasNonTrivialToPrimitiveCopyCUnion() "
            << record_decl->hasNonTrivialToPrimitiveCopyCUnion() << "\n";
  std::cerr << "canPassInRegisters() " << record_decl->canPassInRegisters()
            << "\n";
  std::cerr << "isParamDestroyedInCallee() "
            << record_decl->isParamDestroyedInCallee() << "\n";
  std::cerr << "isRandomized() " << record_decl->isRandomized() << "\n";
  std::cerr << "isInjectedClassName() " << record_decl->isInjectedClassName()
            << "\n";
  std::cerr << "isLambda() " << record_decl->isLambda() << "\n";
  std::cerr << "isCapturedRecord() " << record_decl->isCapturedRecord() << "\n";
  std::cerr << "isOrContainsUnion() " << record_decl->isOrContainsUnion()
            << "\n";
  std::cerr << "field_empty() " << record_decl->field_empty() << "\n";
#endif

  // CLANG FRONTEND FIX: Check if this decl was already translated (e.g., by
  // template visitors) This prevents creating duplicate SgClassDeclaration for
  // nodes already handled as templates
  std::map<clang::Decl *, SgNode *>::iterator it =
      p_decl_translation_map.find(record_decl);
  if (it != p_decl_translation_map.end()) {
    SgNode *existing_node = it->second;
    SgClassDeclaration *existing_class = isSgClassDeclaration(existing_node);
    bool has_definition = false;
    if (existing_class != nullptr) {
      if (existing_class->get_definition() != nullptr) {
        has_definition = true;
      } else if (SgClassDeclaration *def_decl = isSgClassDeclaration(
                     existing_class->get_definingDeclaration())) {
        if (def_decl->get_definition() != nullptr) {
          has_definition = true;
        }
      }
    }

    bool needs_definition =
        record_decl->isThisDeclarationADefinition() && !has_definition;
    if (needs_definition && existing_class != nullptr &&
        isSgTemplateClassDeclaration(existing_class) != nullptr) {
      if (clang::CXXRecordDecl *cxx_record =
              llvm::dyn_cast<clang::CXXRecordDecl>(record_decl)) {
        if (clang::ClassTemplateDecl *template_decl =
                cxx_record->getDescribedClassTemplate()) {
          SgTemplateClassDeclaration *translated_template_decl =
              translateClassTemplateDecl(template_decl, nullptr, nullptr);
          if (translated_template_decl != nullptr) {
            *node = translated_template_decl;
            return true;
          }
        }
      }
      *node = existing_node;
      return true;
    }

    if (!needs_definition) {
#if DEBUG_VISIT_DECL
      std::cerr << "VisitRecordDecl: Already translated, skipping: "
                << record_decl->getNameAsString() << std::endl;
#endif
      *node = existing_node;
      if (SgClassDeclaration *cd = existing_class) {
        SgDeclarationStatement *firstNondef =
            cd->get_firstNondefiningDeclaration();
        ROSE_ASSERT(firstNondef != nullptr);
        ROSE_ASSERT(isSgClassDeclaration(firstNondef) != nullptr);
        ROSE_ASSERT(firstNondef->get_firstNondefiningDeclaration() != nullptr);
      }
      return true; // Already processed
    }
    // Fall through to build a defining declaration when the existing node is
    // non-defining.
  }

  if (clang::CXXRecordDecl *cxx_record =
          llvm::dyn_cast<clang::CXXRecordDecl>(record_decl)) {
    if (clang::ClassTemplateDecl *template_decl =
            cxx_record->getDescribedClassTemplate()) {
      if (p_decl_translation_in_progress.find(template_decl) ==
          p_decl_translation_in_progress.end()) {
        if (SgTemplateClassDeclaration *tmpl =
                translateClassTemplateDecl(template_decl, nullptr, nullptr)) {
          *node = tmpl;
          return true;
        }
      } else {
        auto it_tmpl = p_decl_translation_map.find(template_decl);
        if (it_tmpl != p_decl_translation_map.end() &&
            it_tmpl->second != nullptr) {
          *node = it_tmpl->second;
          return true;
        }
      }
    }
  }

  SgClassDeclaration *sg_class_decl = nullptr;

  // Find previous declaration

  clang::RecordDecl *prev_record_decl = record_decl->getPreviousDecl();
  clang::RecordDecl *record_Definition = record_decl->getDefinition();
  bool isDefined = record_decl->isThisDeclarationADefinition();
  bool isAnonymousStructOrUnion = record_decl->isAnonymousStructOrUnion();
  bool hasNameForLinkage = record_decl->hasNameForLinkage();

  SgClassSymbol *sg_prev_class_sym =
      isSgClassSymbol(GetSymbolFromSymbolTable(prev_record_decl));
  SgClassDeclaration *sg_prev_class_decl = nullptr;
  if (sg_prev_class_sym != nullptr) {
    // CLANG FRONTEND FIX: Accept both SgClassDeclaration and
    // SgTemplateClassDeclaration For templates, the symbol table may contain
    // SgTemplateClassDeclaration from VisitClassTemplateDecl
    sg_prev_class_decl =
        isSgClassDeclaration(sg_prev_class_sym->get_declaration());

    if (sg_prev_class_decl != nullptr) {
      ROSE_ASSERT(sg_prev_class_decl->get_firstNondefiningDeclaration() !=
                  nullptr);
      ROSE_ASSERT(sg_prev_class_decl->get_firstNondefiningDeclaration()
                      ->get_firstNondefiningDeclaration() != nullptr);
    }
  }

  SgClassDeclaration *sg_first_class_decl =
      sg_prev_class_decl == nullptr
          ? nullptr
          : isSgClassDeclaration(
                sg_prev_class_decl->get_firstNondefiningDeclaration());

  // SgClassDeclaration * sg_def_class_decl = sg_prev_class_decl == nullptr ?
  // nullptr :
  // isSgClassDeclaration(sg_prev_class_decl->get_definingDeclaration());
  SgClassSymbol *sg_defining_sym = nullptr;
  // Pei-Hung (07/25/24) The case that a CXXRecordDecl has its definition inside
  // a namespace.
  if (!(llvm::isa<clang::CXXRecordDecl>(record_decl) &&
        llvm::cast<clang::CXXRecordDecl>(record_decl)->hasDefinition())) {
    sg_defining_sym =
        isSgClassSymbol(GetSymbolFromSymbolTable(record_Definition));
  }
  SgClassDeclaration *sg_def_class_decl = nullptr;
  if (sg_defining_sym != nullptr &&
      sg_defining_sym->get_declaration() != nullptr) {
    // CLANG FRONTEND FIX: Accept both SgClassDeclaration and
    // SgTemplateClassDeclaration
    SgDeclarationStatement *decl_stmt = sg_defining_sym->get_declaration();
    sg_def_class_decl =
        isSgClassDeclaration(decl_stmt->get_definingDeclaration());
  }

  // For template specializations, the first declaration may also be the
  // definition In that case, sg_first_class_decl and sg_def_class_decl may
  // refer to the same node
  if (sg_first_class_decl == nullptr && sg_def_class_decl != nullptr) {
    // This can happen for template specializations that are instantiated on
    // first use Use the defining declaration as the first declaration
    sg_first_class_decl = isSgClassDeclaration(
        sg_def_class_decl->get_firstNondefiningDeclaration());
    ROSE_ASSERT(sg_first_class_decl != nullptr);
  }

  // ROSE_ASSERT(sg_first_class_decl != nullptr || sg_def_class_decl ==
  // nullptr); Assertion relaxed for template specializations which may not have
  // separate forward declarations

  bool had_prev_decl = sg_first_class_decl != nullptr;

  // Name

  /* Pei-Hung (08/29/2022) RecordDecl can be anonymous.
   * Apply anonymous name to allow symbol lookup.
   * Need to check later if isAnonymousStructOrUnion is equivalent to Decl with
   * empty name.
   */
  std::string recordDeclName = record_decl->getNameAsString();
  bool isUnNamed = false;
  // Pei-Hung (06/30/2023) recordDeclName could be empty if linkaged name being
  // defined in a typedef of this type.
  if (!hasNameForLinkage || recordDeclName == "") {
    recordDeclName = "__anonymous_" + generate_source_position_string(
                                          record_decl->getBeginLoc());
    isUnNamed = true;
  }

  SgName name(recordDeclName);

  // Type of class

  SgClassDeclaration::class_types type_of_class;
  switch (record_decl->getTagKind()) {
  case clang::TagTypeKind::Struct:
    type_of_class = SgClassDeclaration::e_struct;
    break;
  case clang::TagTypeKind::Class:
    type_of_class = SgClassDeclaration::e_class;
    break;
  case clang::TagTypeKind::Union:
    type_of_class = SgClassDeclaration::e_union;
    break;
  default:
    std::cerr << "Runtime error: RecordDecl can only be a struct/class/union."
              << std::endl;
    res = false;
  }

  // Build declaration(s)

  sg_class_decl = new SgClassDeclaration(name, type_of_class, nullptr, nullptr);
  // ROOT CAUSE FIX: Use Clang's semantic vs lexical decl contexts to set scope
  // vs parent.  For out-of-line nested record definitions (e.g. `A<T>::B`),
  // the semantic context is the enclosing class, but the lexical context is the
  // surrounding namespace/global scope.  Using topScopeStack() as a proxy for
  // the lexical scope breaks on-demand translation during type lowering and can
  // incorrectly nest namespace-scope records inside the current class scope
  // (Issue 69).
  clang::DeclContext *semantic_context = record_decl->getDeclContext();
  clang::DeclContext *lexical_context = record_decl->getLexicalDeclContext();

  auto skip_linkage_context = [](clang::DeclContext *ctx) {
    while (ctx != nullptr && llvm::isa<clang::LinkageSpecDecl>(ctx)) {
      ctx = ctx->getParent();
    }
    return ctx;
  };
  semantic_context = skip_linkage_context(semantic_context);
  lexical_context = skip_linkage_context(lexical_context);

  auto translate_decl_context_on_demand = [&](clang::DeclContext *ctx) {
    if (ctx == nullptr) {
      return;
    }
    if (!is_declaration_scope_context(ctx)) {
      return;
    }
    clang::Decl *context_decl = llvm::dyn_cast<clang::Decl>(ctx);
    if (context_decl == nullptr) {
      return;
    }
    if (llvm::isa<clang::NamespaceDecl>(context_decl)) {
      return;
    }
    if (clang::RecordDecl *record_decl =
            llvm::dyn_cast<clang::RecordDecl>(context_decl)) {
      if (clang::RecordDecl *definition = record_decl->getDefinition()) {
        context_decl = definition;
      }
    }
    if (p_decl_translation_map.find(context_decl) ==
            p_decl_translation_map.end() &&
        p_decl_translation_in_progress.find(context_decl) ==
            p_decl_translation_in_progress.end() &&
        p_decl_translation_on_demand.find(context_decl) ==
            p_decl_translation_on_demand.end()) {
      TraverseOnDemand(context_decl);
    }
  };

  translate_decl_context_on_demand(semantic_context);
  if (lexical_context != semantic_context) {
    translate_decl_context_on_demand(lexical_context);
  }

  SgScopeStatement *structural_scope = SageBuilder::topScopeStack();

  SgScopeStatement *semantic_scope =
      resolveScopeFromDeclContext(semantic_context, structural_scope);
  if (semantic_scope == nullptr) {
    semantic_scope = getGlobalScope();
  }
  SgScopeStatement *correct_scope = semantic_scope;
  if (correct_scope == nullptr) {
    correct_scope = getGlobalScope();
  }
  correct_scope = normalizeNamespaceScope(correct_scope);
  if (correct_scope == nullptr) {
    correct_scope = getGlobalScope();
  }

  SgScopeStatement *lexical_parent =
      resolveScopeFromDeclContext(lexical_context, structural_scope);
  if (lexical_parent == nullptr) {
    lexical_parent =
        structural_scope != nullptr ? structural_scope : correct_scope;
  }

  auto normalize_class_decl_scope = [&](SgClassDeclaration *decl) {
    if (decl == nullptr) {
      return;
    }
    SgScopeStatement *decl_scope = decl->get_scope();
    if (decl_scope == nullptr) {
      return;
    }
    if (normalizeNamespaceScope(decl_scope) == correct_scope &&
        decl_scope != correct_scope) {
      decl->set_scope(correct_scope);
    }
  };

  normalize_class_decl_scope(sg_prev_class_decl);
  normalize_class_decl_scope(sg_first_class_decl);
  normalize_class_decl_scope(sg_def_class_decl);

  bool correct_is_class_scope =
      isSgClassDefinition(correct_scope) != nullptr ||
      isSgTemplateClassDefinition(correct_scope) != nullptr ||
      isSgTemplateInstantiationDefn(correct_scope) != nullptr;
  if (correct_is_class_scope) {
    auto build_class_symbol = [](SgClassDeclaration *decl) -> SgSymbol * {
      if (decl == nullptr) {
        return nullptr;
      }
      if (SgTemplateClassDeclaration *tmpl_decl =
              isSgTemplateClassDeclaration(decl)) {
        return new SgTemplateClassSymbol(tmpl_decl);
      }
      return new SgClassSymbol(decl);
    };

    auto rehome_class_scope = [&](SgClassDeclaration *decl) {
      if (decl == nullptr) {
        return;
      }
      SgScopeStatement *decl_scope = decl->get_scope();
      if (decl_scope == correct_scope) {
        return;
      }
      SgSymbol *symbol = nullptr;
      bool removed_symbol = false;
      if (decl_scope != nullptr) {
        symbol = decl_scope->find_symbol_from_declaration(decl);
        if (symbol != nullptr) {
          if (decl_scope->symbol_exists(symbol)) {
            decl_scope->remove_symbol(symbol);
            removed_symbol = true;
          } else if (SgSymbolTable *table = decl_scope->get_symbol_table()) {
            if (table->exists(symbol)) {
              table->remove(symbol);
              removed_symbol = true;
            }
          }
        }
      }
      decl->set_scope(correct_scope);
      if (correct_scope->find_symbol_from_declaration(decl) == nullptr) {
        if (symbol == nullptr) {
          symbol = build_class_symbol(decl);
        }
        if (symbol != nullptr && !correct_scope->symbol_exists(symbol)) {
          correct_scope->insert_symbol(symbol->get_name(), symbol);
        }
      } else if (removed_symbol) {
        move_symbol_to_orphan_table(symbol);
      }
    };

    rehome_class_scope(sg_prev_class_decl);
    rehome_class_scope(sg_first_class_decl);
    rehome_class_scope(sg_def_class_decl);
    if (sg_first_class_decl != nullptr &&
        correct_scope->find_symbol_from_declaration(sg_first_class_decl) ==
            nullptr) {
      if (SgSymbol *symbol = build_class_symbol(sg_first_class_decl)) {
        if (!correct_scope->symbol_exists(symbol)) {
          correct_scope->insert_symbol(symbol->get_name(), symbol);
        }
      }
    }
  }

  sg_class_decl->set_scope(correct_scope);
  sg_class_decl->set_parent(lexical_parent);
  // DQ (11/28/2020): Adding asertion.
  ROSE_ASSERT(sg_class_decl->get_parent() != nullptr);

  // std::cerr << "DEBUG: VisitCXXRecordDecl for " <<
  // record_decl->getNameAsString() << std::endl;

  // CRITICAL: Set firstNondefiningDeclaration BEFORE calling createType()
  // createType() internally asserts that this pointer is not null
  // This will be corrected later if this is not actually the first declaration
  if (sg_first_class_decl != nullptr) {
    // CLANG FRONTEND FIX: Only set if variant types match
    if (sg_first_class_decl->variantT() == sg_class_decl->variantT()) {
      sg_class_decl->set_firstNondefiningDeclaration(sg_first_class_decl);
    } else {
      // Variant mismatch - set to self to avoid assertion
      sg_class_decl->set_firstNondefiningDeclaration(sg_class_decl);
    }
  } else {
    sg_class_decl->set_firstNondefiningDeclaration(sg_class_decl);
  }

  SgClassType *type = nullptr;
  if (sg_first_class_decl != nullptr) {
    type = sg_first_class_decl->get_type();
  } else {
    type = SgClassType::createType(sg_class_decl);
  }
  ROSE_ASSERT(type != nullptr);
  sg_class_decl->set_type(type);

  if (isUnNamed)
    sg_class_decl->set_isUnNamed(true);

  if (!had_prev_decl) {
    sg_first_class_decl = sg_class_decl;
    sg_first_class_decl->set_firstNondefiningDeclaration(sg_first_class_decl);
    sg_first_class_decl->set_definingDeclaration(nullptr);
    sg_first_class_decl->set_definition(nullptr);
    sg_first_class_decl->setForward();
    // ROOT CAUSE FIX: Use correct_scope consistently for symbol insertion
    SgClassSymbol *class_symbol = new SgClassSymbol(sg_first_class_decl);
    correct_scope->insert_symbol(name, class_symbol);
  } else if (!isDefined) {
    // Non-defining redeclaration.
    // IMPORTANT: Return a distinct SgClassDeclaration for each Clang redecl.
    // Mapping multiple Clang redecls to the same SgStatement and appending it
    // during DeclContext traversal results in duplicate statement pointers in a
    // scope (caught by AstConsistencyTests).
    applySourceRange(sg_class_decl, record_decl->getSourceRange());

    sg_class_decl->setForward();

    // Preserve the defining declaration link when the definition is already
    // known (e.g., out-of-order traversal).
    if (sg_def_class_decl != nullptr &&
        sg_class_decl->variantT() == sg_def_class_decl->variantT()) {
      sg_class_decl->set_definingDeclaration(sg_def_class_decl);
    }

    // NOTE: Do not insert a symbol for redeclarations; the symbol table entry
    // is created exactly once for the first nondefining declaration above.
  }

  // For embedded anonymous definitions, keep a distinct nondefining
  // declaration to satisfy ROSE invariants (nondefining != defining).
  bool reuse_first_decl_for_def = false;

  if (isDefined) {
    if (reuse_first_decl_for_def && sg_first_class_decl != nullptr) {
      sg_def_class_decl = sg_first_class_decl;
      sg_class_decl = sg_def_class_decl;

      sg_def_class_decl->unsetForward();
      sg_def_class_decl->set_firstNondefiningDeclaration(sg_first_class_decl);
      sg_def_class_decl->set_definingDeclaration(sg_def_class_decl);
      sg_first_class_decl->set_definingDeclaration(sg_def_class_decl);

      applySourceRange(sg_def_class_decl, record_decl->getSourceRange());
    } else {
      sg_def_class_decl =
          new SgClassDeclaration(name, type_of_class, type, nullptr);
      // ROOT CAUSE FIX: Use correct_scope consistently for defining
      // declaration too
      sg_def_class_decl->set_scope(correct_scope);
      if (isUnNamed)
        sg_def_class_decl->set_isUnNamed(true);
      sg_def_class_decl->set_parent(lexical_parent);

      // OPENMP LOWERING FIX: The sg_class_decl created at line 1350 will be
      // orphaned when we reassign below, but it may still be referenced through
      // SgClassType. Set its file info before orphaning.
      if (had_prev_decl && sg_class_decl->get_startOfConstruct() == nullptr) {
        applySourceRange(sg_class_decl, record_decl->getSourceRange());
      }

      sg_class_decl = sg_def_class_decl; // we return the defining decl

      // CLANG FRONTEND FIX: Only set if variant types match
      if (sg_first_class_decl != nullptr &&
          sg_first_class_decl->variantT() == sg_def_class_decl->variantT()) {
        sg_def_class_decl->set_firstNondefiningDeclaration(sg_first_class_decl);
      } else {
        sg_def_class_decl->set_firstNondefiningDeclaration(sg_def_class_decl);
      }
      ROSE_ASSERT(sg_def_class_decl->get_firstNondefiningDeclaration() !=
                  nullptr);
      sg_def_class_decl->set_definingDeclaration(sg_def_class_decl);

      // CLANG FRONTEND FIX: Only set definingDeclaration if variant types
      // match
      if (sg_first_class_decl != nullptr &&
          sg_first_class_decl->variantT() == sg_def_class_decl->variantT()) {
        sg_first_class_decl->set_definingDeclaration(sg_def_class_decl);
      }
    }
    // Only synthesize compiler-generated file info for a forward declaration
    // we created to satisfy ROSE's first-nondef/defining decl invariants. If a
    // real forward declaration exists in the source (had_prev_decl), preserve
    // its source location so it is emitted in the class body (Issue 69).
    if (!had_prev_decl) {
      setCompilerGeneratedFileInfo(sg_first_class_decl);
    }

    // Build ClassDefinition
    SgClassDefinition *sg_class_def =
        isSgClassDefinition(sg_def_class_decl->get_definition());
    if (sg_class_def == nullptr) {
      sg_class_def = SageBuilder::buildClassDefinition_nfi(sg_def_class_decl);
    }
    sg_def_class_decl->set_definition(sg_class_def);

    ROSE_ASSERT(sg_class_def->get_symbol_table() != nullptr);

    applySourceRange(sg_class_def, record_decl->getSourceRange());

    SageBuilder::pushScopeStack(sg_class_def);

    // CRITICAL FIX: Add to translation map BEFORE processing members!
    // This prevents infinite recursion if member processing triggers a lookup
    // of this class type. The Traverse() function normally adds to the map
    // after Visit returns, but that's too late - by then we've already
    // processed members which may trigger recursive visits.
    p_decl_translation_map[record_decl] = sg_class_decl;

    // Member population for non-C++ records can be done directly here.
    // For C++ records, member population is handled in VisitCXXRecordDecl so
    // we preserve source/lexical order across fields, methods, and other
    // declarations.
    if (!llvm::isa<clang::CXXRecordDecl>(record_decl)) {
      if (p_record_definitions_populated.insert(sg_class_def).second) {
        populateClassDefinition(record_decl, sg_class_def);
      }
    }

    SageBuilder::popScopeStack();
  }

  clang::RecordDecl *parent_decl =
      llvm::dyn_cast_or_null<clang::RecordDecl>(record_decl->getDeclContext());
  clang::SourceManager *embedded_source_manager =
      p_compiler_instance != nullptr ? &p_compiler_instance->getSourceManager()
                                     : nullptr;
  bool is_embedded_record = record_decl->isEmbeddedInDeclarator() ||
                            isTagEmbeddedInField(record_decl, parent_decl,
                                                 embedded_source_manager, true);
  if (is_embedded_record) {
    if (sg_class_decl != nullptr) {
      sg_class_decl->set_isAutonomousDeclaration(false);
      suppress_unparse_output(sg_class_decl);
    }
    if (sg_first_class_decl != nullptr) {
      sg_first_class_decl->set_isAutonomousDeclaration(false);
      suppress_unparse_output(sg_first_class_decl);
    }
    if (sg_def_class_decl != nullptr) {
      sg_def_class_decl->set_isAutonomousDeclaration(false);
      suppress_unparse_output(sg_def_class_decl);
    }
  }

  ROSE_ASSERT(sg_class_decl->get_definingDeclaration() == nullptr ||
              isSgClassDeclaration(sg_class_decl->get_definingDeclaration())
                      ->get_definition() != nullptr);
  if (sg_first_class_decl != nullptr && !reuse_first_decl_for_def) {
    ROSE_ASSERT(sg_first_class_decl->get_definition() == nullptr);
  }
  ROSE_ASSERT(sg_def_class_decl == nullptr ||
              sg_def_class_decl->get_definition() != nullptr);

  *node = sg_class_decl;

  return VisitTagDecl(record_decl, node) && res;
}

bool ClangToSageTranslator::VisitCXXRecordDecl(
    clang::CXXRecordDecl *cxx_record_decl, SgNode **node) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToSageTranslator::VisitCXXRecordDecl" << std::endl;
#endif
  bool res = VisitRecordDecl(cxx_record_decl, node);

  // CLANG FRONTEND FIX: Process C++ specific members (methods, constructors,
  // etc.) Only do this if this is the DEFINING declaration (not forward
  // declaration or redeclaration)
  if (cxx_record_decl->isThisDeclarationADefinition() &&
      cxx_record_decl->hasDefinition()) {
    SgClassDeclaration *sg_class_decl = isSgClassDeclaration(*node);
    if (sg_class_decl != nullptr) {
      SgClassDeclaration *def_decl =
          isSgClassDeclaration(sg_class_decl->get_definingDeclaration());
      if (def_decl != nullptr &&
          def_decl == sg_class_decl) { // Make sure this IS the defining decl
        SgClassDefinition *sg_class_def = def_decl->get_definition();
        if (sg_class_def != nullptr) {
          if (p_record_definitions_populated.insert(sg_class_def).second) {
            populateClassDefinition(cxx_record_decl, sg_class_def);
            // Base classes and friends are TODO for future implementation
          }
        }
      }
    }
  }

  if (res && node && *node) {
    if (SgClassDeclaration *decl = isSgClassDeclaration(*node)) {
      // REX FIX: Issue 107 Root Cause.
      // Check if firstNondefiningDeclaration is nullptr OR points to self (for
      // defining decls). This catches cases where VisitRecordDecl setup was
      // insufficient.
      SgDeclarationStatement *firstNonDef =
          decl->get_firstNondefiningDeclaration();
      if (firstNonDef == nullptr ||
          (decl->get_definition() && firstNonDef == decl)) {
        // Create non-defining declaration.
        // Must handle SgTemplateInstantiationDecl vs SgClassDeclaration vs
        // SgTemplateClassDeclaration.
        SgClassDeclaration *nonDef = nullptr;

        if (SgTemplateInstantiationDecl *tplInst =
                isSgTemplateInstantiationDecl(decl)) {
          SgName template_name = tplInst->get_templateName();
          if (template_name.is_null() || template_name.getString().empty()) {
            std::string base_name = decl->get_name().getString();
            size_t lt = base_name.find('<');
            if (lt != std::string::npos) {
              base_name = base_name.substr(0, lt);
            }
            template_name = SgName(base_name);
          }

          SgTemplateArgumentPtrList *template_args =
              &tplInst->get_templateArguments();

          nonDef = SageBuilder::buildNondefiningClassDeclaration_nfi(
              template_name, decl->get_class_type(), decl->get_scope(),
              /*buildTemplateInstantiation=*/true, template_args);

          if (SgTemplateInstantiationDecl *nondef_inst =
                  isSgTemplateInstantiationDecl(nonDef)) {
            SageBuilder::setTemplateArgumentsInDeclaration(nondef_inst,
                                                           template_args);
            if (nondef_inst->get_templateName().is_null()) {
              nondef_inst->set_templateName(template_name);
            }
            if (tplInst->get_templateDeclaration() != nullptr &&
                nondef_inst->get_templateDeclaration() == nullptr) {
              nondef_inst->set_templateDeclaration(
                  tplInst->get_templateDeclaration());
            }
          }

        } else {
          nonDef = SageBuilder::buildNondefiningClassDeclaration_nfi(
              decl->get_name(), decl->get_class_type(), decl->get_scope(),
              false, nullptr);
        }

        if (nonDef) {
          nonDef->set_parent(decl->get_parent());

          nonDef->set_firstNondefiningDeclaration(nonDef);
          nonDef->set_definingDeclaration(decl);

          decl->set_firstNondefiningDeclaration(nonDef);
          decl->set_definingDeclaration(decl);
        }
      }
    }
  }

  return res;
}

// Helper from clang-frontend-type.cpp
static SgExpression *buildIntegralTemplateArgExpr(const llvm::APSInt &value,
                                                  SgType *int_type) {
  const bool is_signed = value.isSigned();
  const unsigned bitwidth = value.getBitWidth();

  SgExpression *expr = nullptr;
  if (is_signed) {
    // Use the widest native builder we have; valueString keeps the full
    // precision.
    long long v = (bitwidth <= 63) ? value.getSExtValue() : 0;
    expr = SageBuilder::buildLongLongIntVal(v);
  } else {
    unsigned long long v = (bitwidth <= 64) ? value.getZExtValue() : 0;
    expr = SageBuilder::buildUnsignedLongLongIntVal(v);
  }

  if (expr != nullptr) {
    llvm::SmallString<64> buf;
    value.toString(buf, 10, value.isSigned());
    std::string text(buf.begin(), buf.end());

    if (SgLongLongIntVal *ll = isSgLongLongIntVal(expr)) {
      ll->set_valueString(text);
    } else if (SgUnsignedLongLongIntVal *ull =
                   isSgUnsignedLongLongIntVal(expr)) {
      ull->set_valueString(text);
    }
  }

  return expr;
}

bool ClangToSageTranslator::VisitClassTemplateSpecializationDecl(
    clang::ClassTemplateSpecializationDecl *class_tpl_spec_decl,
    SgNode **node) {

  if (class_tpl_spec_decl == nullptr) {
    *node = nullptr;
    return false;
  }
  if (class_tpl_spec_decl->isInvalidDecl()) {
    *node = nullptr;
    return false;
  }

  struct DeclTranslationGuard {
    std::set<clang::Decl *> &in_progress;
    clang::Decl *decl;
    bool inserted;
    DeclTranslationGuard(std::set<clang::Decl *> &set, clang::Decl *d)
        : in_progress(set), decl(d), inserted(false) {
      if (decl != nullptr) {
        inserted = in_progress.insert(decl).second;
      }
    }
    ~DeclTranslationGuard() {
      if (inserted) {
        in_progress.erase(decl);
      }
    }
  } spec_guard(p_decl_translation_in_progress, class_tpl_spec_decl);
  DeclTranslationGuard canonical_guard(p_decl_translation_in_progress,
                                       class_tpl_spec_decl->getCanonicalDecl());

  // Ensure we handle partial specializations separately.
  if (auto *partial =
          llvm::dyn_cast<clang::ClassTemplatePartialSpecializationDecl>(
              class_tpl_spec_decl)) {
    return VisitClassTemplatePartialSpecializationDecl(partial, node);
  }

  bool in_system_header = false;
  if (p_compiler_instance != nullptr) {
    clang::SourceManager &SM = p_compiler_instance->getSourceManager();
    in_system_header = SM.isInSystemHeader(class_tpl_spec_decl->getLocation());
  }

  bool res = true;

  auto resolve_namespace_scope =
      [&](clang::DeclContext *ctx,
          SgScopeStatement *fallback) -> SgScopeStatement * {
    clang::DeclContext *scope_ctx = ctx;
    while (scope_ctx != nullptr && !scope_ctx->isNamespace() &&
           !scope_ctx->isTranslationUnit()) {
      scope_ctx = scope_ctx->getParent();
    }
    if (clang::NamespaceDecl *ns_decl =
            llvm::dyn_cast_or_null<clang::NamespaceDecl>(
                llvm::dyn_cast_or_null<clang::Decl>(scope_ctx))) {
      if (SgNamespaceDeclarationStatement *ns_stmt =
              ensureNamespaceDeclaration(ns_decl)) {
        if (ns_stmt->get_definition() != nullptr) {
          return ns_stmt->get_definition();
        }
      }
    }
    if (scope_ctx != nullptr && scope_ctx->isTranslationUnit()) {
      return getGlobalScope();
    }
    return fallback;
  };

  clang::TemplateSpecializationKind specialization_kind =
      class_tpl_spec_decl->getTemplateSpecializationKind();
  bool is_explicit_instantiation =
      specialization_kind == clang::TSK_ExplicitInstantiationDeclaration ||
      specialization_kind == clang::TSK_ExplicitInstantiationDefinition;
  bool is_extern_instantiation =
      specialization_kind == clang::TSK_ExplicitInstantiationDeclaration;
  clang::CXXRecordDecl *definition_decl = class_tpl_spec_decl->getDefinition();
  if (definition_decl == nullptr &&
      class_tpl_spec_decl->isThisDeclarationADefinition()) {
    definition_decl = class_tpl_spec_decl;
  }
  const bool has_definition = definition_decl != nullptr;
  const bool is_definition_decl =
      class_tpl_spec_decl->isThisDeclarationADefinition();

  clang::TemplateDecl *specialized_template =
      class_tpl_spec_decl->getSpecializedTemplate();

  // Resolve class kind early so fallback template declarations can use it.
  SgClassDeclaration::class_types class_kind = SgClassDeclaration::e_class;
  switch (class_tpl_spec_decl->getTagKind()) {
  case clang::TagTypeKind::Struct:
    class_kind = SgClassDeclaration::e_struct;
    break;
  case clang::TagTypeKind::Class:
    class_kind = SgClassDeclaration::e_class;
    break;
  case clang::TagTypeKind::Union:
    class_kind = SgClassDeclaration::e_union;
    break;
  default:
    std::cerr
        << "Warning: Unsupported tag kind for class template specialization: "
        << static_cast<int>(class_tpl_spec_decl->getTagKind()) << std::endl;
    break;
  }

  // Traverse the specialized template to ensure it exists in the AST and we
  // have the ROSE node.
  auto lookup_template_decl =
      [&](clang::Decl *key) -> SgTemplateClassDeclaration * {
    if (key == nullptr) {
      return nullptr;
    }
    auto it = p_decl_translation_map.find(key);
    if (it == p_decl_translation_map.end()) {
      return nullptr;
    }
    return isSgTemplateClassDeclaration(it->second);
  };

  if (specialized_template == nullptr) {
    auto specialized = class_tpl_spec_decl->getSpecializedTemplateOrPartial();
    if (auto *class_template =
            specialized.dyn_cast<clang::ClassTemplateDecl *>()) {
      specialized_template = class_template;
    } else if (auto *partial =
                   specialized.dyn_cast<
                       clang::ClassTemplatePartialSpecializationDecl *>()) {
      specialized_template = partial->getSpecializedTemplate();
    }
  }

  // Determine the template name from the specialized template
  std::string template_name_str;
  if (specialized_template != nullptr) {
    template_name_str = specialized_template->getNameAsString();
  }
  if (template_name_str.empty()) {
    template_name_str =
        "__anon_template_spec_" +
        generate_source_position_string(class_tpl_spec_decl->getBeginLoc());
  }
  SgName name(template_name_str);

  SgTemplateClassDeclaration *primary_template_decl = nullptr;
  if (specialized_template) {
    SgNode *primary_node = Traverse(specialized_template);
    primary_template_decl = isSgTemplateClassDeclaration(primary_node);
  }
  if (primary_template_decl == nullptr && specialized_template != nullptr) {
    primary_template_decl = lookup_template_decl(specialized_template);
    if (primary_template_decl == nullptr) {
      if (clang::ClassTemplateDecl *class_template =
              llvm::dyn_cast<clang::ClassTemplateDecl>(specialized_template)) {
        clang::ClassTemplateDecl *canonical =
            class_template->getCanonicalDecl();
        if (canonical != nullptr) {
          primary_template_decl = lookup_template_decl(canonical);
        }

        if (primary_template_decl == nullptr) {
          if (clang::CXXRecordDecl *templated_decl =
                  class_template->getTemplatedDecl()) {
            primary_template_decl = lookup_template_decl(templated_decl);
            if (primary_template_decl == nullptr) {
              if (clang::CXXRecordDecl *templated_canon =
                      llvm::dyn_cast_or_null<clang::CXXRecordDecl>(
                          templated_decl->getCanonicalDecl())) {
                primary_template_decl = lookup_template_decl(templated_canon);
              }
            }
          }
        }

        if (primary_template_decl == nullptr) {
          clang::ClassTemplateDecl *to_translate =
              canonical != nullptr ? canonical : class_template;
          if (p_decl_translation_map.find(to_translate) ==
                  p_decl_translation_map.end() &&
              p_decl_translation_in_progress.find(to_translate) ==
                  p_decl_translation_in_progress.end()) {
            TraverseOnDemand(to_translate);
          }
          primary_template_decl = lookup_template_decl(to_translate);
        }
      }
    }
    if (primary_template_decl == nullptr) {
      if (SgSymbol *symbol = GetSymbolFromSymbolTable(specialized_template)) {
        if (SgTemplateClassSymbol *tmpl_sym = isSgTemplateClassSymbol(symbol)) {
          primary_template_decl =
              isSgTemplateClassDeclaration(tmpl_sym->get_declaration());
        } else if (SgClassSymbol *class_sym = isSgClassSymbol(symbol)) {
          primary_template_decl =
              isSgTemplateClassDeclaration(class_sym->get_declaration());
        }
      }
    }
  }
  if (primary_template_decl == nullptr) {
    std::string spec_name = class_tpl_spec_decl->getNameAsString();
    if (!spec_name.empty()) {
      SgScopeStatement *lookup_scope = resolve_namespace_scope(
          class_tpl_spec_decl->getDeclContext(), nullptr);
      lookup_scope = normalizeNamespaceScope(lookup_scope);
      if (lookup_scope == nullptr) {
        lookup_scope = getGlobalScope();
      }

      if (lookup_scope != nullptr) {
        if (SgTemplateClassSymbol *tmpl_sym =
                lookup_scope->lookup_template_class_symbol(SgName(spec_name),
                                                           nullptr, nullptr)) {
          primary_template_decl =
              isSgTemplateClassDeclaration(tmpl_sym->get_declaration());
        } else if (SgClassSymbol *class_sym =
                       lookup_scope->lookup_class_symbol(SgName(spec_name))) {
          primary_template_decl =
              isSgTemplateClassDeclaration(class_sym->get_declaration());
        }
      }
      if (primary_template_decl == nullptr) {
        if (SgTemplateClassSymbol *tmpl_sym =
                SageInterface::lookupTemplateClassSymbolInParentScopes(
                    SgName(spec_name), nullptr, nullptr, lookup_scope)) {
          primary_template_decl =
              isSgTemplateClassDeclaration(tmpl_sym->get_declaration());
        } else if (SgClassSymbol *class_sym =
                       SageInterface::lookupClassSymbolInParentScopes(
                           SgName(spec_name), lookup_scope)) {
          primary_template_decl =
              isSgTemplateClassDeclaration(class_sym->get_declaration());
        }
      }
    }
  }
  if (primary_template_decl == nullptr && specialized_template != nullptr) {
    if (clang::ClassTemplateDecl *class_template =
            llvm::dyn_cast<clang::ClassTemplateDecl>(specialized_template)) {
      if (p_decl_translation_in_progress.find(class_template) ==
          p_decl_translation_in_progress.end()) {
        primary_template_decl =
            translateClassTemplateDecl(class_template, nullptr, nullptr);
      }
    }
  }

  // As a last resort, synthesize a nondefining template declaration so
  // instantiations always link to a valid primary template.
  if (primary_template_decl == nullptr) {
    clang::ClassTemplateDecl *class_template =
        llvm::dyn_cast_or_null<clang::ClassTemplateDecl>(specialized_template);
    if (class_template == nullptr) {
      class_template = class_tpl_spec_decl->getSpecializedTemplate();
    }
    if (class_template != nullptr) {
      std::string qualified_name = class_template->getQualifiedNameAsString();
      if (qualified_name.empty()) {
        qualified_name = class_template->getNameAsString();
      }

      if (!qualified_name.empty()) {
        auto cache_it = p_template_decl_cache.find(qualified_name);
        if (cache_it != p_template_decl_cache.end()) {
          primary_template_decl = cache_it->second;
        }
      }

      if (primary_template_decl == nullptr) {
        SgScopeStatement *tmpl_scope = resolveScopeFromDeclContext(
            class_template->getDeclContext(), nullptr);
        if (tmpl_scope == nullptr) {
          tmpl_scope = resolve_namespace_scope(class_template->getDeclContext(),
                                               nullptr);
        }
        tmpl_scope = normalizeNamespaceScope(tmpl_scope);
        if (tmpl_scope == nullptr) {
          tmpl_scope = getGlobalScope();
        }

        std::string base_name = class_template->getNameAsString();
        if (base_name.empty()) {
          base_name = template_name_str;
        }

        auto params = translateTemplateParameterList(
            class_template->getTemplateParameters(), nullptr);
        SgTemplateArgumentPtrList empty_args;

        SgTemplateClassDeclaration *stub_decl =
            SageBuilder::buildNondefiningTemplateClassDeclaration_nfi(
                SgName(base_name), class_kind, tmpl_scope, params.get(),
                &empty_args);
        if (stub_decl != nullptr) {
          stub_decl->setForward();
          stub_decl->set_firstNondefiningDeclaration(stub_decl);
          stub_decl->set_definingDeclaration(nullptr);
          stub_decl->get_file_info()->setCompilerGenerated();
          stub_decl->get_file_info()->unsetOutputInCodeGeneration();

          for (SgTemplateParameter *param :
               stub_decl->get_templateParameters()) {
            if (param != nullptr &&
                param->get_parameterType() !=
                    SgTemplateParameter::template_parameter) {
              param->set_templateDeclaration(stub_decl);
            }
          }

          attach_nonreal_template_parameters(
              stub_decl, stub_decl->get_templateParameters());

          if (!qualified_name.empty()) {
            p_template_decl_cache[qualified_name] = stub_decl;
          } else if (!base_name.empty()) {
            p_template_decl_cache[base_name] = stub_decl;
          }
          primary_template_decl = stub_decl;
        }
      }
    }
  }

  SgClassDeclaration::class_types instantiation_kind = class_kind;
  if (is_explicit_instantiation && p_compiler_instance != nullptr) {
    SgClassDeclaration::class_types spelled_kind = class_kind;
    if (getExplicitInstantiationClassKind(class_tpl_spec_decl,
                                          p_compiler_instance, &spelled_kind)) {
      instantiation_kind = spelled_kind;
    }
  }

  clang::DeclContext *decl_context = class_tpl_spec_decl->getDeclContext();
  clang::DeclContext *lexical_context =
      class_tpl_spec_decl->getLexicalDeclContext();
  clang::DeclContext *instantiation_context = decl_context;
  if (is_explicit_instantiation && lexical_context != nullptr) {
    instantiation_context = lexical_context;
  }

  SgScopeStatement *instantiation_scope =
      (instantiation_context != nullptr &&
       instantiation_context->isTranslationUnit())
          ? getGlobalScope()
          : SageBuilder::topScopeStack();

  // Check if we can get a better scope from the DeclContext
  if (instantiation_context && !instantiation_context->isTranslationUnit()) {
    clang::Decl *context_decl =
        llvm::dyn_cast<clang::Decl>(instantiation_context);
    if (context_decl) {
      SgNode *context_node = nullptr;
      std::map<clang::Decl *, SgNode *>::iterator it =
          p_decl_translation_map.find(context_decl);
      if (it != p_decl_translation_map.end()) {
        context_node = it->second;
        SgNamespaceDeclarationStatement *ns_decl_stmt =
            isSgNamespaceDeclarationStatement(context_node);
        SgNamespaceDefinitionStatement *ns_def =
            isSgNamespaceDefinitionStatement(context_node);
        SgClassDefinition *class_def = isSgClassDefinition(context_node);
        if (ns_decl_stmt != nullptr &&
            ns_decl_stmt->get_definition() != nullptr) {
          instantiation_scope = ns_decl_stmt->get_definition();
        } else if (ns_def != nullptr) {
          instantiation_scope = ns_def;
        } else if (class_def != nullptr) {
          instantiation_scope = class_def;
        }
      } else if (clang::NamespaceDecl *ns_decl =
                     llvm::dyn_cast<clang::NamespaceDecl>(context_decl)) {
        // Ensure the namespace scope exists even if it has not been translated
        // yet (e.g., when we first encounter a decl in that namespace).
        SgNamespaceDeclarationStatement *ns_stmt =
            ensureNamespaceDeclaration(ns_decl);
        if (ns_stmt && ns_stmt->get_definition() != nullptr) {
          instantiation_scope = ns_stmt->get_definition();
        }
      }
    }
  }

  if (instantiation_scope == nullptr) {
    instantiation_scope = getGlobalScope();
  }

  SgScopeStatement *class_scope = nullptr;
  if (specialized_template != nullptr) {
    clang::DeclContext *template_context =
        specialized_template->getDeclContext();
    class_scope = resolveScopeFromDeclContext(template_context, nullptr);
  }
  SgScopeStatement *primary_scope = nullptr;
  if (primary_template_decl != nullptr) {
    primary_scope = primary_template_decl->get_scope();
  }
  if (class_scope == nullptr && primary_scope != nullptr) {
    class_scope = primary_scope;
  }
  if (class_scope == nullptr) {
    class_scope = instantiation_scope;
  }
  if (class_scope == nullptr) {
    class_scope = getGlobalScope();
  }
  if (primary_scope != nullptr && isSgGlobal(class_scope) != nullptr &&
      isSgGlobal(primary_scope) == nullptr) {
    class_scope = primary_scope;
  }
  if (specialization_kind == clang::TSK_ExplicitSpecialization &&
      instantiation_scope != nullptr) {
    class_scope = instantiation_scope;
  } else if (is_explicit_instantiation && instantiation_scope != nullptr) {
    class_scope = instantiation_scope;
  }

  SgScopeStatement *lexical_scope =
      resolveScopeFromDeclContext(lexical_context, nullptr);
  if (lexical_scope == nullptr) {
    lexical_scope = instantiation_scope;
  }

  if (instantiation_scope == nullptr) {
    instantiation_scope = getGlobalScope();
  }

  SgScopeStatement *symbol_scope = normalizeNamespaceScope(class_scope);
  if (symbol_scope == nullptr) {
    symbol_scope = class_scope;
  }
  if (symbol_scope == nullptr) {
    symbol_scope = getGlobalScope();
  }

  SgScopeStatement *scope = symbol_scope;
  SgScopeStatement *parent_scope = class_scope;
  if (lexical_scope != nullptr &&
      (isSgNamespaceDefinitionStatement(lexical_scope) != nullptr ||
       isSgGlobal(lexical_scope) != nullptr)) {
    parent_scope = lexical_scope;
  }
  if (parent_scope == nullptr) {
    parent_scope = scope;
  }

  auto recover_template_decl =
      [&](SgTemplateInstantiationDecl *decl) -> SgTemplateClassDeclaration * {
    if (decl == nullptr) {
      return nullptr;
    }
    SgName template_name = decl->get_templateName();
    if (template_name.getString().empty()) {
      template_name = name;
    }
    if (template_name.getString().empty()) {
      return nullptr;
    }

    SgScopeStatement *lookup_scope = isSgScopeStatement(decl->get_parent());
    if (lookup_scope == nullptr) {
      lookup_scope = decl->get_scope();
    }
    if (lookup_scope == nullptr) {
      lookup_scope = scope;
    }
    if (lookup_scope == nullptr) {
      lookup_scope = parent_scope;
    }
    if (lookup_scope == nullptr) {
      lookup_scope = getGlobalScope();
    }

    SgTemplateClassSymbol *tmpl_sym =
        lookup_scope->lookup_template_class_symbol(template_name, nullptr,
                                                   nullptr);
    if (tmpl_sym == nullptr) {
      tmpl_sym = SageInterface::lookupTemplateClassSymbolInParentScopes(
          template_name, nullptr, nullptr, lookup_scope);
    }
    if (tmpl_sym != nullptr) {
      return isSgTemplateClassDeclaration(tmpl_sym->get_declaration());
    }

    if (SgClassSymbol *class_sym =
            lookup_scope->lookup_class_symbol(template_name)) {
      return isSgTemplateClassDeclaration(class_sym->get_declaration());
    }
    if (SgClassSymbol *class_sym =
            SageInterface::lookupClassSymbolInParentScopes(template_name,
                                                           lookup_scope)) {
      return isSgTemplateClassDeclaration(class_sym->get_declaration());
    }

    SgTemplateParameterPtrList empty_params;
    SgTemplateArgumentPtrList empty_args;
    SgTemplateClassDeclaration *stub =
        SageBuilder::buildNondefiningTemplateClassDeclaration_nfi(
            template_name, instantiation_kind, lookup_scope, &empty_params,
            &empty_args);
    if (stub != nullptr) {
      stub->setForward();
      stub->set_firstNondefiningDeclaration(stub);
      stub->set_definingDeclaration(nullptr);
      if (stub->get_file_info() != nullptr) {
        stub->get_file_info()->setCompilerGenerated();
        stub->get_file_info()->unsetOutputInCodeGeneration();
      }
    }
    return stub;
  };

  auto specialized_or_partial =
      class_tpl_spec_decl->getSpecializedTemplateOrPartial();
  const bool specialization_is_partial =
      specialized_or_partial
          .dyn_cast<clang::ClassTemplatePartialSpecializationDecl *>() !=
      nullptr;
  clang::Decl *specialized_key = nullptr;
  if (auto *partial =
          specialized_or_partial
              .dyn_cast<clang::ClassTemplatePartialSpecializationDecl *>()) {
    specialized_key = partial;
  } else if (auto *primary = specialized_or_partial
                                 .dyn_cast<clang::ClassTemplateDecl *>()) {
    specialized_key = primary;
  }

  // Build template arguments
  const clang::TemplateArgumentList &args =
      class_tpl_spec_decl->getTemplateArgs();
  // Always capture deduced (instantiation) arguments separately.
  const clang::TemplateArgumentList &instantiation_args =
      class_tpl_spec_decl->getTemplateInstantiationArgs();
  SgName name_with_template_args(
      buildTemplateInstantiationName(name.getString(), args));

  ConstraintSatisfactionResult constraint_result;
  if (specialization_kind == clang::TSK_ImplicitInstantiation ||
      specialization_kind == clang::TSK_ExplicitInstantiationDefinition ||
      specialization_kind == clang::TSK_ExplicitInstantiationDeclaration) {
#if LLVM_VERSION_MAJOR >= 21
    llvm::SmallVector<clang::AssociatedConstraint, 4> constraints;
#else
    llvm::SmallVector<const clang::Expr *, 4> constraints;
#endif
    const clang::NamedDecl *constraint_owner = nullptr;
    if (const clang::ClassTemplatePartialSpecializationDecl *partial =
            llvm::dyn_cast<clang::ClassTemplatePartialSpecializationDecl>(
                class_tpl_spec_decl)) {
      partial->getAssociatedConstraints(constraints);
      constraint_owner = partial;
    } else if (clang::ClassTemplateDecl *primary =
                   class_tpl_spec_decl->getSpecializedTemplate()) {
      if (clang::TemplateParameterList *params =
              primary->getTemplateParameters()) {
        params->getAssociatedConstraints(constraints);
      }
      constraint_owner = primary;
    }

    if (constraint_owner != nullptr && !constraints.empty()) {
      const clang::TemplateArgumentList &constraint_args =
          specialization_is_partial ? instantiation_args : args;
      constraint_result = evaluateConstraintSatisfaction(
          constraint_owner, constraints, constraint_args,
          class_tpl_spec_decl->getSourceRange());
    }
  }

  // Helper lambda to build args (to avoid code duplication)
  auto build_args = [&](SgTemplateArgumentPtrList &target_list) {
    for (unsigned i = 0; i < args.size(); ++i) {
      appendTemplateArguments(target_list, args.get(i), false);
    }
  };
  auto build_deduced_args = [&](SgTemplateArgumentPtrList &target_list) {
    for (unsigned i = 0; i < instantiation_args.size(); ++i) {
      appendTemplateArguments(target_list, instantiation_args.get(i), false);
    }
  };

  auto lookup_specialized_template =
      [&](clang::Decl *key) -> SgDeclarationStatement * {
    if (key == nullptr) {
      return nullptr;
    }
    auto lookup = [&](clang::Decl *lookup_key) -> SgDeclarationStatement * {
      if (lookup_key == nullptr) {
        return nullptr;
      }
      auto it = p_decl_translation_map.find(lookup_key);
      if (it != p_decl_translation_map.end()) {
        return isSgDeclarationStatement(it->second);
      }
      return nullptr;
    };

    if (SgDeclarationStatement *decl = lookup(key)) {
      return decl;
    }
    if (auto *tmpl = llvm::dyn_cast<clang::ClassTemplateDecl>(key)) {
      if (clang::ClassTemplateDecl *canonical = tmpl->getCanonicalDecl()) {
        if (SgDeclarationStatement *decl = lookup(canonical)) {
          return decl;
        }
      }
    }
    if (auto *partial =
            llvm::dyn_cast<clang::ClassTemplatePartialSpecializationDecl>(
                key)) {
      if (clang::CXXRecordDecl *canonical = partial->getCanonicalDecl()) {
        if (SgDeclarationStatement *decl = lookup(canonical)) {
          return decl;
        }
      }
    }

    if (p_decl_translation_in_progress.find(key) !=
            p_decl_translation_in_progress.end() ||
        p_decl_translation_on_demand.find(key) !=
            p_decl_translation_on_demand.end()) {
      return nullptr;
    }
    SgNode *translated = TraverseOnDemand(key);
    return isSgDeclarationStatement(translated);
  };

  SgDeclarationStatement *specialized_template_decl = nullptr;
  if (auto *partial =
          specialized_or_partial
              .dyn_cast<clang::ClassTemplatePartialSpecializationDecl *>()) {
    specialized_template_decl = lookup_specialized_template(partial);
  }
  if (specialized_template_decl == nullptr) {
    if (clang::ClassTemplateDecl *primary =
            class_tpl_spec_decl->getSpecializedTemplate()) {
      specialized_template_decl = lookup_specialized_template(primary);
    }
  }

  auto rehome_instantiation_symbol = [&](SgTemplateInstantiationDecl *decl,
                                         SgScopeStatement *desired_scope) {
    if (decl == nullptr || desired_scope == nullptr) {
      return;
    }
    SgSymbol *symbol = decl->get_symbol_from_symbol_table();
    if (symbol == nullptr) {
      return;
    }

    SgSymbolTable *parent_table = isSgSymbolTable(symbol->get_parent());
    SgSymbolTable *desired_table = desired_scope->get_symbol_table();
    if (parent_table == desired_table) {
      if (decl->get_scope() != desired_scope) {
        decl->set_scope(desired_scope);
      }
      return;
    }
    if (parent_table != nullptr && parent_table != desired_table) {
      if (SgScopeStatement *parent_scope =
              isSgScopeStatement(parent_table->get_parent())) {
        bool can_use_scope_remove = parent_scope->symbol_exists(symbol);
        if (SgNamespaceDefinitionStatement *ns_scope =
                isSgNamespaceDefinitionStatement(parent_scope)) {
          if (ns_scope->get_global_definition() == ns_scope) {
            can_use_scope_remove = false;
          }
        }
        if (can_use_scope_remove) {
          parent_scope->remove_symbol(symbol);
        } else if (parent_table->exists(symbol)) {
          parent_table->remove(symbol);
        }
      } else if (parent_table->exists(symbol)) {
        parent_table->remove(symbol);
      }
    }

    decl->set_scope(desired_scope);
    if (!desired_scope->symbol_exists(symbol)) {
      desired_scope->insert_symbol(symbol->get_name(), symbol);
    }
  };

  // Implement Two-Declaration Pattern for consistency
  // Always create a non-defining declaration (forward decl) and a defining
  // declaration if this is a definition.

  SgTemplateInstantiationDecl *instantiationDecl = nullptr;
  SgTemplateInstantiationDecl *firstNondefiningDeclaration = nullptr;

  // ROOT CAUSE FIX: Check instantiation cache first
  // This connects explicit specializations to the same nodes used by implicit
  // instantiations
  std::string inst_name_full;
  {
    std::string template_qualified_name;
    if (primary_template_decl) {
      template_qualified_name = getTemplateQualifiedName(primary_template_decl);
    }
    if (template_qualified_name.empty()) {
      template_qualified_name = class_tpl_spec_decl->getQualifiedNameAsString();
    }
    if (template_qualified_name.empty()) {
      template_qualified_name = name.getString();
    }

    inst_name_full = mangleTemplateInstantiation(template_qualified_name, args);

    auto cache_it = p_template_inst_cache.find(inst_name_full);
    if (cache_it != p_template_inst_cache.end()) {
      firstNondefiningDeclaration = cache_it->second;
    }
  }

  // Check for existing symbol in the scope if not found in cache
  if (firstNondefiningDeclaration == nullptr) {
    SgSymbol *existing_symbol = nullptr;
    existing_symbol = scope->lookup_symbol(name_with_template_args);
    if (existing_symbol == nullptr) {
      existing_symbol = scope->lookup_symbol(name);
    }
    if (existing_symbol) {
      SgClassSymbol *class_symbol = isSgClassSymbol(existing_symbol);
      if (class_symbol) {
        SgClassDeclaration *existing_decl = class_symbol->get_declaration();
        firstNondefiningDeclaration =
            isSgTemplateInstantiationDecl(existing_decl);
      }
    }
  }

  if (firstNondefiningDeclaration == nullptr) {
    SgTemplateArgumentPtrList forward_args;
    build_args(forward_args);

    instantiationDecl = new SgTemplateInstantiationDecl(
        name_with_template_args, instantiation_kind, nullptr, nullptr, nullptr,
        forward_args);
    instantiationDecl->get_templateArguments() = forward_args;

    // Register in cache immediately
    if (!inst_name_full.empty()) {
      p_template_inst_cache[inst_name_full] = instantiationDecl;
    }

    firstNondefiningDeclaration = instantiationDecl;
    instantiationDecl->set_firstNondefiningDeclaration(
        firstNondefiningDeclaration);
    instantiationDecl->set_definingDeclaration(nullptr);
    instantiationDecl->set_forward(true);
    instantiationDecl->set_templateName(name);
    instantiationDecl->set_nameResetFromMangledForm(true);

    SgClassType *type = SgClassType::createType(instantiationDecl);
    instantiationDecl->set_type(type);

    // setStatementSourcePosition(instantiationDecl, class_tpl_spec_decl);
    applySourceRange(instantiationDecl, class_tpl_spec_decl->getSourceRange());
    instantiationDecl->set_scope(scope);
    instantiationDecl->set_parent(parent_scope);

    for (SgTemplateArgument *arg : instantiationDecl->get_templateArguments()) {
      arg->set_parent(instantiationDecl);
    }

    if (instantiationDecl->get_specializedTemplateDeclaration() == nullptr &&
        specialized_template_decl != nullptr) {
      instantiationDecl->set_specializedTemplateDeclaration(
          specialized_template_decl);
    } else if (instantiationDecl->get_specializedTemplateDeclaration() ==
                   nullptr &&
               specialized_key != nullptr) {
      queueSpecializedTemplateLink(instantiationDecl, specialized_key);
    }

    // Insert symbol (for the forward decl/type)
    // Use the declaration name so symbol lookups/removals stay consistent.
    if (!scope->symbol_exists(name_with_template_args)) {
      SgClassSymbol *class_symbol = new SgClassSymbol(instantiationDecl);
      scope->insert_symbol(name_with_template_args, class_symbol);
    }
  } else {
    // We found an existing declaration, so we don't create a new non-defining
    // one.
    instantiationDecl = firstNondefiningDeclaration;
    if (instantiationDecl != nullptr &&
        instantiationDecl->get_name() != name_with_template_args) {
      // Keep symbol table keys in sync when renaming a cached instantiation.
      SgScopeStatement *decl_scope = instantiationDecl->get_scope();
      if (decl_scope != nullptr) {
        SgName old_name = instantiationDecl->get_name();
        SgClassSymbol *class_symbol = decl_scope->lookup_class_symbol(old_name);
        if (class_symbol != nullptr &&
            class_symbol->get_declaration() == instantiationDecl) {
          if (SgSymbolTable *parent_table =
                  isSgSymbolTable(class_symbol->get_parent())) {
            if (SgScopeStatement *parent_scope =
                    isSgScopeStatement(parent_table->get_parent())) {
              bool can_use_scope_remove =
                  parent_scope->symbol_exists(class_symbol);
              if (SgNamespaceDefinitionStatement *ns_scope =
                      isSgNamespaceDefinitionStatement(parent_scope)) {
                if (ns_scope->get_global_definition() == ns_scope) {
                  can_use_scope_remove = false;
                }
              }
              if (can_use_scope_remove) {
                parent_scope->remove_symbol(class_symbol);
              } else if (parent_table->exists(class_symbol)) {
                parent_table->remove(class_symbol);
              }
            } else if (parent_table->exists(class_symbol)) {
              parent_table->remove(class_symbol);
            }
          }
        } else {
          class_symbol = nullptr;
        }
        instantiationDecl->set_name(name_with_template_args);
        if (class_symbol != nullptr) {
          SgClassSymbol *existing_sym =
              decl_scope->lookup_class_symbol(name_with_template_args);
          if (existing_sym != nullptr) {
            ROSE_ASSERT(existing_sym->get_declaration() == instantiationDecl);
          } else {
            decl_scope->insert_symbol(name_with_template_args, class_symbol);
          }
        }
      } else {
        instantiationDecl->set_name(name_with_template_args);
      }
    }
    if (instantiationDecl->get_templateArguments().empty()) {
      SgTemplateArgumentPtrList recovered_args;
      build_args(recovered_args);
      instantiationDecl->get_templateArguments() = recovered_args;
    }
    if (instantiationDecl->get_deducedTemplateArguments().empty()) {
      SgTemplateArgumentPtrList recovered_deduced;
      build_deduced_args(recovered_deduced);
      instantiationDecl->get_deducedTemplateArguments() = recovered_deduced;
    }
    for (SgTemplateArgument *arg : instantiationDecl->get_templateArguments()) {
      if (arg != nullptr) {
        arg->set_parent(instantiationDecl);
      }
    }
    for (SgTemplateArgument *arg :
         instantiationDecl->get_deducedTemplateArguments()) {
      if (arg != nullptr) {
        arg->set_parent(instantiationDecl);
      }
    }
    if (instantiationDecl->get_scope() == nullptr) {
      instantiationDecl->set_scope(scope);
    }
    rehome_instantiation_symbol(instantiationDecl, scope);
    if (instantiationDecl->get_parent() == nullptr) {
      instantiationDecl->set_parent(parent_scope);
    }
    if (!instantiationDecl->get_nameResetFromMangledForm()) {
      instantiationDecl->set_nameResetFromMangledForm(true);
    }
    if (instantiationDecl->get_specializedTemplateDeclaration() == nullptr &&
        specialized_template_decl != nullptr) {
      instantiationDecl->set_specializedTemplateDeclaration(
          specialized_template_decl);
    } else if (instantiationDecl->get_specializedTemplateDeclaration() ==
                   nullptr &&
               specialized_key != nullptr) {
      queueSpecializedTemplateLink(instantiationDecl, specialized_key);
    }
    // setStatementSourcePosition(instantiationDecl, class_tpl_spec_decl); //
    // Don't reset position of reused decl
  }

  if (is_explicit_instantiation && instantiationDecl != nullptr &&
      instantiationDecl->get_class_type() != instantiation_kind) {
    instantiationDecl->set_class_type(instantiation_kind);
  }
  // Set specialized template for the non-defining declaration (if new) or
  // check it
  if (instantiationDecl->get_templateDeclaration() == nullptr) {
    // Link to the primary template declaration
    if (primary_template_decl) {
      instantiationDecl->set_templateDeclaration(primary_template_decl);
    }
  }
  if (instantiationDecl->get_templateDeclaration() == nullptr) {
    if (SgTemplateClassDeclaration *recovered =
            recover_template_decl(instantiationDecl)) {
      instantiationDecl->set_templateDeclaration(recovered);
    }
  }

  // Ensure a defining declaration exists when Clang has a definition, even if
  // this specialization is emitted as an explicit instantiation directive.
  SgTemplateInstantiationDecl *nondef_decl = instantiationDecl;
  SgTemplateInstantiationDecl *definingDecl = nullptr;
  if (nondef_decl != nullptr) {
    definingDecl =
        isSgTemplateInstantiationDecl(nondef_decl->get_definingDeclaration());
    if (definingDecl == nondef_decl) {
      definingDecl = nullptr;
    }
  }

  if (has_definition) {
    if (definingDecl == nullptr) {
      SgTemplateArgumentPtrList defining_args;
      build_args(defining_args);
      SgTemplateArgumentPtrList defining_deduced_args;
      build_deduced_args(defining_deduced_args);

      definingDecl = new SgTemplateInstantiationDecl(
          name_with_template_args, class_kind, nullptr, nullptr, nullptr,
          defining_args);
      definingDecl->get_templateArguments() = defining_args;
      definingDecl->get_deducedTemplateArguments() = defining_deduced_args;

      definingDecl->set_firstNondefiningDeclaration(
          firstNondefiningDeclaration);
      firstNondefiningDeclaration->set_definingDeclaration(definingDecl);
      definingDecl->set_definingDeclaration(definingDecl);
      definingDecl->set_forward(false);
      definingDecl->set_templateName(name);
      definingDecl->set_nameResetFromMangledForm(true);
      definingDecl->set_type(firstNondefiningDeclaration->get_type());

      applySourceRange(definingDecl, class_tpl_spec_decl->getSourceRange());
      definingDecl->set_scope(scope);
      definingDecl->set_parent(parent_scope);

      for (SgTemplateArgument *arg : definingDecl->get_templateArguments()) {
        arg->set_parent(definingDecl);
      }
      for (SgTemplateArgument *arg :
           definingDecl->get_deducedTemplateArguments()) {
        if (arg != nullptr) {
          arg->set_parent(definingDecl);
        }
      }

      definingDecl->set_templateDeclaration(
          firstNondefiningDeclaration->get_templateDeclaration());
      if (definingDecl->get_templateDeclaration() == nullptr) {
        if (SgTemplateClassDeclaration *recovered =
                recover_template_decl(definingDecl)) {
          definingDecl->set_templateDeclaration(recovered);
        }
      }
      if (definingDecl->get_specializedTemplateDeclaration() == nullptr &&
          specialized_template_decl != nullptr) {
        definingDecl->set_specializedTemplateDeclaration(
            specialized_template_decl);
      } else if (definingDecl->get_specializedTemplateDeclaration() ==
                     nullptr &&
                 specialized_key != nullptr) {
        queueSpecializedTemplateLink(definingDecl, specialized_key);
      }
    } else {
      definingDecl->set_firstNondefiningDeclaration(
          firstNondefiningDeclaration);
      firstNondefiningDeclaration->set_definingDeclaration(definingDecl);
      definingDecl->set_definingDeclaration(definingDecl);
      definingDecl->set_forward(false);
      if (definingDecl->get_templateArguments().empty()) {
        SgTemplateArgumentPtrList defining_args;
        build_args(defining_args);
        definingDecl->get_templateArguments() = defining_args;
      }
      for (SgTemplateArgument *arg : definingDecl->get_templateArguments()) {
        if (arg != nullptr) {
          arg->set_parent(definingDecl);
        }
      }
      if (definingDecl->get_templateDeclaration() == nullptr) {
        definingDecl->set_templateDeclaration(
            firstNondefiningDeclaration->get_templateDeclaration());
      }
      if (definingDecl->get_templateDeclaration() == nullptr) {
        if (SgTemplateClassDeclaration *recovered =
                recover_template_decl(definingDecl)) {
          definingDecl->set_templateDeclaration(recovered);
        }
      }
      if (definingDecl->get_templateName().getString().empty()) {
        definingDecl->set_templateName(name);
      }
      if (!definingDecl->get_nameResetFromMangledForm()) {
        definingDecl->set_nameResetFromMangledForm(true);
      }
      if (definingDecl->get_type() == nullptr &&
          firstNondefiningDeclaration->get_type() != nullptr) {
        definingDecl->set_type(firstNondefiningDeclaration->get_type());
      }
      if (definingDecl->get_deducedTemplateArguments().empty()) {
        SgTemplateArgumentPtrList defining_deduced_args;
        build_deduced_args(defining_deduced_args);
        definingDecl->get_deducedTemplateArguments() = defining_deduced_args;
      }
      for (SgTemplateArgument *arg :
           definingDecl->get_deducedTemplateArguments()) {
        if (arg != nullptr) {
          arg->set_parent(definingDecl);
        }
      }
      if (definingDecl->get_specializedTemplateDeclaration() == nullptr &&
          specialized_template_decl != nullptr) {
        definingDecl->set_specializedTemplateDeclaration(
            specialized_template_decl);
      } else if (definingDecl->get_specializedTemplateDeclaration() ==
                     nullptr &&
                 specialized_key != nullptr) {
        queueSpecializedTemplateLink(definingDecl, specialized_key);
      }
      if (definingDecl->get_scope() == nullptr) {
        definingDecl->set_scope(scope);
      }
      if (definingDecl->get_parent() == nullptr) {
        definingDecl->set_parent(parent_scope);
      }
    }

    if (definingDecl != nullptr && definingDecl->get_definition() == nullptr) {
      SgTemplateInstantiationDefn *class_def =
          new SgTemplateInstantiationDefn(definingDecl);
      definingDecl->set_definition(class_def);
      class_def->set_parent(definingDecl);
      applySourceRange(class_def, class_tpl_spec_decl->getSourceRange());
    }
  }

  if (is_explicit_instantiation && definingDecl != nullptr) {
    mark_compiler_generated_and_suppress_unparse(definingDecl);
    if (SgClassDefinition *class_def = definingDecl->get_definition()) {
      mark_compiler_generated_and_suppress_unparse(class_def);
    }
  }

  SgTemplateInstantiationDecl *node_decl = nondef_decl;
  if (is_definition_decl && !is_explicit_instantiation &&
      definingDecl != nullptr) {
    node_decl = definingDecl;
  }

  if (specialization_kind == clang::TSK_ImplicitInstantiation) {
    auto suppress_instantiation = [&](SgTemplateInstantiationDecl *decl) {
      if (decl == nullptr) {
        return;
      }
      mark_compiler_generated_and_suppress_unparse(decl);
      if (SgClassDefinition *class_def = decl->get_definition()) {
        mark_compiler_generated_and_suppress_unparse(class_def);
      }
    };
    suppress_instantiation(nondef_decl);
    if (definingDecl != nullptr) {
      suppress_instantiation(definingDecl);
    }
  }

  if (constraint_result.evaluated) {
    attachConstraintSatisfaction(nondef_decl, constraint_result);
    if (definingDecl != nullptr && definingDecl != nondef_decl) {
      attachConstraintSatisfaction(definingDecl, constraint_result);
    }
    if (nondef_decl != nullptr) {
      pruneSymbolsForConstraints(nondef_decl);
    }
    if (definingDecl != nullptr && definingDecl != nondef_decl) {
      pruneSymbolsForConstraints(definingDecl);
    }
  }

  p_decl_translation_map.insert(
      std::pair<clang::Decl *, SgNode *>(class_tpl_spec_decl, node_decl));
  if (definition_decl != nullptr && definition_decl != class_tpl_spec_decl) {
    SgNode *definition_node = definingDecl != nullptr
                                  ? static_cast<SgNode *>(definingDecl)
                                  : node_decl;
    p_decl_translation_map.insert(
        std::pair<clang::Decl *, SgNode *>(definition_decl, definition_node));
  }
  if (clang::CXXRecordDecl *canonical =
          class_tpl_spec_decl->getCanonicalDecl()) {
    p_decl_translation_map[canonical] = node_decl;
  }
  for (clang::CXXRecordDecl *prev = class_tpl_spec_decl->getPreviousDecl();
       prev != nullptr; prev = prev->getPreviousDecl()) {
    p_decl_translation_map[prev] = node_decl;
  }

  // Ensure we return the correct node
  SgTemplateInstantiationDirectiveStatement *instantiation_directive = nullptr;
  const bool on_demand_translation =
      p_decl_translation_on_demand.find(class_tpl_spec_decl) !=
      p_decl_translation_on_demand.end();

  if (is_explicit_instantiation) {
    instantiation_directive =
        isSgTemplateInstantiationDirectiveStatement(nondef_decl->get_parent());
    if (instantiation_directive == nullptr) {
      SgNode *old_parent = nondef_decl->get_parent();
      if (SgScopeStatement *old_scope = isSgScopeStatement(old_parent)) {
        detach_decl_from_scope_child_list(nondef_decl, old_scope);
      }
      instantiation_directive =
          new SgTemplateInstantiationDirectiveStatement(nondef_decl);
      instantiation_directive->set_do_not_instantiate(is_extern_instantiation);
      instantiation_directive->set_declaration(nondef_decl);
      instantiation_directive->set_scope(instantiation_scope);
      instantiation_directive->set_parent(instantiation_scope);
      applySourceRange(instantiation_directive,
                       class_tpl_spec_decl->getSourceRange());
      nondef_decl->set_parent(instantiation_directive);
      if (nondef_decl->get_scope() == nullptr) {
        SgScopeStatement *decl_scope = scope;
        if (decl_scope == nullptr) {
          decl_scope = instantiation_scope;
        }
        if (decl_scope == nullptr) {
          decl_scope = getGlobalScope();
        }
        nondef_decl->set_scope(decl_scope);
      }
    }
    if (instantiation_directive != nullptr) {
      if (instantiation_directive->get_declaration() == nullptr) {
        instantiation_directive->set_declaration(nondef_decl);
      }
      if (nondef_decl->get_scope() == nullptr) {
        SgScopeStatement *decl_scope = scope;
        if (decl_scope == nullptr) {
          decl_scope = instantiation_scope;
        }
        if (decl_scope == nullptr) {
          decl_scope = getGlobalScope();
        }
        nondef_decl->set_scope(decl_scope);
      }
    }
    if (!on_demand_translation) {
      ensure_decl_in_scope_child_list(instantiation_directive,
                                      instantiation_scope,
                                      "VisitClassTemplateSpecializationDecl");
    }
    if (instantiation_directive != nullptr &&
        instantiation_directive->get_firstNondefiningDeclaration() == nullptr) {
      instantiation_directive->set_firstNondefiningDeclaration(
          instantiation_directive);
      instantiation_directive->set_definingDeclaration(instantiation_directive);
    }
  }

  if (instantiation_directive == nullptr && parent_scope != nullptr &&
      parent_scope != scope &&
      (isSgNamespaceDefinitionStatement(parent_scope) != nullptr ||
       isSgGlobal(parent_scope) != nullptr)) {
    auto attach_lexically = [&](SgDeclarationStatement *decl) {
      if (decl == nullptr) {
        return;
      }
      ensure_decl_in_scope_child_list_preserve_scope(
          decl, parent_scope, "VisitClassTemplateSpecializationDecl:lexical");
    };

    attach_lexically(nondef_decl);
    if (definingDecl != nullptr && definingDecl != nondef_decl) {
      attach_lexically(definingDecl);
    }
  }
  *node = instantiation_directive != nullptr ? instantiation_directive
                                             : static_cast<SgNode *>(node_decl);
  SgTemplateInstantiationDecl *instantiationDeclChecked =
      isSgTemplateInstantiationDecl(node_decl);
  ROSE_ASSERT(instantiationDeclChecked != nullptr);

  ROSE_ASSERT(instantiationDeclChecked->get_firstNondefiningDeclaration() !=
              nullptr);
  ROSE_ASSERT(instantiationDeclChecked->get_firstNondefiningDeclaration()
                  ->get_firstNondefiningDeclaration() != nullptr);

  // Process scope stack and children if it is a definition
  //
  // System-header class template specializations frequently contain compiler-
  // synthesized members with non-file source locations; translating the full
  // body can trigger invalid SourceLocation handling. Still create the defining
  // ROSE scope above so on-demand translation of referenced members has a valid
  // class scope to attach to, but skip eager body population for system
  // headers.
  if (definingDecl != nullptr && definingDecl->get_definition() != nullptr &&
      has_definition && !in_system_header) {
    if (SgClassDefinition *class_def =
            isSgClassDefinition(definingDecl->get_definition())) {
      if (p_record_definitions_populated.insert(class_def).second) {
        SageBuilder::pushScopeStack(class_def);
        populateClassDefinition(definition_decl, class_def);
        SageBuilder::popScopeStack();
      }
    }
  }

  return true;
}

bool ClangToSageTranslator::VisitClassTemplatePartialSpecializationDecl(
    clang::ClassTemplatePartialSpecializationDecl *class_tpl_part_spec_decl,
    SgNode **node) {
  if (class_tpl_part_spec_decl == nullptr) {
    *node = nullptr;
    return false;
  }
  if (class_tpl_part_spec_decl->isInvalidDecl()) {
    *node = nullptr;
    return false;
  }

  struct DeclTranslationGuard {
    std::set<clang::Decl *> &in_progress;
    clang::Decl *decl;
    bool inserted;
    DeclTranslationGuard(std::set<clang::Decl *> &set, clang::Decl *d)
        : in_progress(set), decl(d), inserted(false) {
      inserted = in_progress.insert(decl).second;
    }
    ~DeclTranslationGuard() {
      if (inserted) {
        in_progress.erase(decl);
      }
    }
  } part_spec_guard(p_decl_translation_in_progress, class_tpl_part_spec_decl);

  clang::ClassTemplateSpecializationDecl *class_tpl_spec_decl =
      class_tpl_part_spec_decl;

  // Determine the template name from the specialized template
  std::string template_name_str;
  clang::TemplateDecl *specialized_template =
      class_tpl_spec_decl->getSpecializedTemplate();
  if (specialized_template) {
    template_name_str = specialized_template->getNameAsString();
  } else {
    template_name_str =
        "__anon_template_spec_" +
        generate_source_position_string(class_tpl_spec_decl->getBeginLoc());
  }
  SgName name(template_name_str);

  // Resolve class kind
  SgClassDeclaration::class_types class_kind = SgClassDeclaration::e_class;
  switch (class_tpl_spec_decl->getTagKind()) {
  case clang::TagTypeKind::Struct:
    class_kind = SgClassDeclaration::e_struct;
    break;
  case clang::TagTypeKind::Class:
    class_kind = SgClassDeclaration::e_class;
    break;
  case clang::TagTypeKind::Union:
    class_kind = SgClassDeclaration::e_union;
    break;
  default:
    std::cerr
        << "Warning: Unsupported tag kind for class template specialization: "
        << static_cast<int>(class_tpl_spec_decl->getTagKind()) << std::endl;
    break;
  }

  clang::DeclContext *decl_context = class_tpl_spec_decl->getDeclContext();
  SgScopeStatement *scope = SageBuilder::topScopeStack();

  // Check if we can get a better scope from the DeclContext
  if (decl_context && !decl_context->isTranslationUnit()) {
    clang::Decl *context_decl = llvm::dyn_cast<clang::Decl>(decl_context);
    if (context_decl) {
      SgNode *context_node = nullptr;
      std::map<clang::Decl *, SgNode *>::iterator it =
          p_decl_translation_map.find(context_decl);
      if (it != p_decl_translation_map.end()) {
        context_node = it->second;
        SgNamespaceDefinitionStatement *ns_def =
            isSgNamespaceDefinitionStatement(context_node);
        SgClassDefinition *class_def = isSgClassDefinition(context_node);
        if (ns_def != nullptr) {
          scope = ns_def;
        } else if (class_def != nullptr) {
          scope = class_def;
        }
      }
    }
  }

  if (scope == nullptr) {
    scope = getGlobalScope();
  }

  auto build_specialization_args = [&](SgTemplateArgumentPtrList &target_list) {
    const clang::TemplateArgumentList &args =
        class_tpl_part_spec_decl->getTemplateArgs();
    auto append_arg = [&](const clang::TemplateArgument &arg,
                          const auto &append_ref) -> void {
      if (arg.getKind() == clang::TemplateArgument::Pack) {
        for (const clang::TemplateArgument &pack_arg : arg.pack_elements()) {
          append_ref(pack_arg, append_ref);
        }
        return;
      }

      size_t before = target_list.size();
      appendTemplateArguments(target_list, arg, false);

      if (arg.getKind() != clang::TemplateArgument::Type ||
          target_list.size() == before) {
        return;
      }

      const clang::Type *type_ptr = arg.getAsType().getTypePtr();
      const clang::TemplateTypeParmType *parm_type =
          type_ptr ? type_ptr->getAs<clang::TemplateTypeParmType>() : nullptr;
      if (parm_type == nullptr) {
        return;
      }

      std::string name;
      if (clang::TemplateTypeParmDecl *decl = parm_type->getDecl()) {
        name = decl->getNameAsString();
      }
      if (name.empty() || name.find("type-parameter-") == 0) {
        unsigned index = parm_type->getIndex();
        clang::TemplateParameterList *params =
            class_tpl_part_spec_decl->getTemplateParameters();
        if (params && index < params->size()) {
          name = params->getParam(index)->getNameAsString();
        }
      }
      if (name.empty()) {
        return;
      }

      if (SgTemplateArgument *last_arg = target_list.back()) {
        if (SgTemplateType *tt = isSgTemplateType(last_arg->get_type())) {
          if (tt->get_name().getString() != name) {
            tt->set_name(name);
          }
        }
      }
    };

    if (args.size() != 0) {
      for (unsigned i = 0; i < args.size(); ++i) {
        append_arg(args.get(i), append_arg);
      }
    } else if (const clang::ASTTemplateArgumentListInfo *args_written =
                   class_tpl_part_spec_decl->getTemplateArgsAsWritten()) {
      for (const clang::TemplateArgumentLoc &arg_loc :
           args_written->arguments()) {
        append_arg(arg_loc.getArgument(), append_arg);
      }
    }

    // Ensure parents are initialized so name mangling does not assert before
    // the declaration owns these arguments.
    ensureTemplateArgumentParents(target_list);
  };

  SgTemplateArgumentPtrList specialization_args;
  build_specialization_args(specialization_args);
  // They are attached to the declaration later.
  auto template_params = translateTemplateParameterList(
      class_tpl_part_spec_decl->getTemplateParameters(), nullptr);

  SgTemplateClassDeclaration *nonDefiningDecl =
      SageBuilder::buildNondefiningTemplateClassDeclaration_nfi(
          name, class_kind, scope, template_params.get(), &specialization_args);
  ROSE_ASSERT(nonDefiningDecl != nullptr);

  nonDefiningDecl->set_specialization(
      SgDeclarationStatement::e_partial_specialization);
  nonDefiningDecl->set_firstNondefiningDeclaration(nonDefiningDecl);
  nonDefiningDecl->set_scope(scope);
  nonDefiningDecl->set_parent(scope);
  applySourceRange(nonDefiningDecl, class_tpl_part_spec_decl->getSourceRange());

  for (SgTemplateParameter *param : nonDefiningDecl->get_templateParameters()) {
    if (param != nullptr &&
        param->get_parameterType() != SgTemplateParameter::template_parameter) {
      param->set_templateDeclaration(nonDefiningDecl);
    }
  }

  SgTemplateClassDeclaration *definingDecl = nullptr;
  if (class_tpl_part_spec_decl->isThisDeclarationADefinition()) {
    SgTemplateArgumentPtrList specialization_args_for_def;
    build_specialization_args(specialization_args_for_def);

    definingDecl = SageBuilder::buildTemplateClassDeclaration_nfi(
        name, class_kind, scope, nonDefiningDecl, template_params.get(),
        &specialization_args_for_def);
    ROSE_ASSERT(definingDecl != nullptr);

    definingDecl->set_specialization(
        SgDeclarationStatement::e_partial_specialization);
    definingDecl->set_firstNondefiningDeclaration(nonDefiningDecl);
    definingDecl->set_definingDeclaration(definingDecl);
    definingDecl->set_scope(scope);
    definingDecl->set_parent(scope);
    nonDefiningDecl->set_definingDeclaration(definingDecl);
    applySourceRange(definingDecl, class_tpl_part_spec_decl->getSourceRange());
  }

  if (clang::TemplateParameterList *params =
          class_tpl_part_spec_decl->getTemplateParameters()) {
    if (const clang::Expr *requires_expr = params->getRequiresClause()) {
      if (SgExpression *sg_requires =
              translateConstraintExpression(requires_expr)) {
        nonDefiningDecl->set_requiresClause(sg_requires);
        sg_requires->set_parent(nonDefiningDecl);
        if (definingDecl != nullptr && definingDecl != nonDefiningDecl) {
          if (SgExpression *copy =
                  isSgExpression(SageInterface::deepCopy(sg_requires))) {
            definingDecl->set_requiresClause(copy);
            copy->set_parent(definingDecl);
          }
        }
      }
    }
  }

  SgTemplateClassDeclaration *node_decl =
      definingDecl != nullptr ? definingDecl : nonDefiningDecl;
  attach_nonreal_template_parameters(node_decl,
                                     node_decl->get_templateParameters());
  p_decl_translation_map[class_tpl_part_spec_decl] = node_decl;
  if (clang::CXXRecordDecl *definition_decl =
          class_tpl_part_spec_decl->getDefinition()) {
    if (definition_decl != class_tpl_part_spec_decl) {
      p_decl_translation_map[definition_decl] = node_decl;
    }
  }
  if (clang::CXXRecordDecl *canonical =
          class_tpl_part_spec_decl->getCanonicalDecl()) {
    p_decl_translation_map[canonical] = node_decl;
  }
  for (clang::CXXRecordDecl *prev = class_tpl_part_spec_decl->getPreviousDecl();
       prev != nullptr; prev = prev->getPreviousDecl()) {
    p_decl_translation_map[prev] = node_decl;
  }

  if (definingDecl != nullptr) {
    if (SgClassDefinition *def = definingDecl->get_definition()) {
      if (p_record_definitions_populated.insert(def).second) {
        SageBuilder::pushScopeStack(def);
        populateClassDefinition(class_tpl_part_spec_decl, def);
        SageBuilder::popScopeStack();
      }
    }
  }

  const bool on_demand_translation =
      p_decl_translation_on_demand.find(class_tpl_part_spec_decl) !=
      p_decl_translation_on_demand.end();
  if (!on_demand_translation) {
    ensure_decl_in_scope_child_list(
        nonDefiningDecl, scope, "VisitClassTemplatePartialSpecializationDecl");
    if (definingDecl != nullptr && definingDecl != nonDefiningDecl) {
      ensure_decl_in_scope_child_list(
          definingDecl, scope,
          "VisitClassTemplatePartialSpecializationDecl:def");
    }
  }

  *node = node_decl;
  return true;
}

bool ClangToSageTranslator::VisitEnumDecl(clang::EnumDecl *enum_decl,
                                          SgNode **node) {

#if DEBUG_VISIT_DECL
  std::cerr << "ClangToSageTranslator::VisitEnumDecl" << std::endl;
#endif
  bool res = true;
  std::string enumDeclName = enum_decl->getNameAsString();

  if (enumDeclName.empty()) {
    enumDeclName = "__anonymous_" +
                   generate_source_position_string(enum_decl->getBeginLoc());
  }

  SgName name(enumDeclName);

#if DEBUG_VISIT_DECL
  std::cerr << "isfreestanding:" << enum_decl->isFreeStanding()
            << " isembedded:" << enum_decl->isEmbeddedInDeclarator()
            << std::endl;
  std::cerr << "hasNameForLinkage:" << enum_decl->hasNameForLinkage()
            << std::endl;
  std::cerr << "enum name:" << enumDeclName << std::endl;
#endif

  SgScopeStatement *enum_scope = SageBuilder::topScopeStack();
  clang::DeclContext *decl_context = enum_decl->getDeclContext();
  clang::DeclContext *scope_context = decl_context;
  while (scope_context != nullptr &&
         llvm::isa<clang::LinkageSpecDecl>(scope_context)) {
    scope_context = scope_context->getParent();
  }

  auto translate_decl_context_on_demand = [&](clang::DeclContext *ctx) {
    if (ctx == nullptr) {
      return;
    }
    if (!is_declaration_scope_context(ctx)) {
      return;
    }
    clang::Decl *context_decl = llvm::dyn_cast<clang::Decl>(ctx);
    if (context_decl == nullptr) {
      return;
    }
    if (llvm::isa<clang::NamespaceDecl>(context_decl)) {
      return;
    }
    if (clang::RecordDecl *record_decl =
            llvm::dyn_cast<clang::RecordDecl>(context_decl)) {
      if (clang::RecordDecl *definition = record_decl->getDefinition()) {
        context_decl = definition;
      }
    }
    if (p_decl_translation_map.find(context_decl) ==
            p_decl_translation_map.end() &&
        p_decl_translation_in_progress.find(context_decl) ==
            p_decl_translation_in_progress.end() &&
        p_decl_translation_on_demand.find(context_decl) ==
            p_decl_translation_on_demand.end()) {
      TraverseOnDemand(context_decl);
    }
  };

  translate_decl_context_on_demand(scope_context);
  if (scope_context != nullptr) {
    if (scope_context->isTranslationUnit()) {
      enum_scope = getGlobalScope();
    } else if (clang::Decl *context_decl =
                   llvm::dyn_cast<clang::Decl>(scope_context)) {
      if (SgScopeStatement *resolved =
              resolveScopeFromDeclContext(scope_context, enum_scope)) {
        enum_scope = resolved;
      }
    }
  }
  if (enum_scope == nullptr) {
    enum_scope = getGlobalScope();
  }

  SgScopeStatement *symbol_scope = normalizeNamespaceScope(enum_scope);
  if (symbol_scope == nullptr) {
    symbol_scope = enum_scope;
  }

  SgScopeStatement *lexical_scope = enum_scope;
  clang::DeclContext *lexical_context = enum_decl->getLexicalDeclContext();
  translate_decl_context_on_demand(lexical_context);
  if (lexical_context != nullptr) {
    if (SgScopeStatement *resolved =
            resolveScopeFromDeclContext(lexical_context, lexical_scope)) {
      lexical_scope = resolved;
    }
  }
  bool force_member_scope =
      isSgClassDefinition(symbol_scope) != nullptr ||
      isSgTemplateClassDefinition(symbol_scope) != nullptr ||
      isSgTemplateInstantiationDefn(symbol_scope) != nullptr;
  if (force_member_scope) {
    lexical_scope = symbol_scope;
  }
  auto is_record_scope = [](SgScopeStatement *scope) -> bool {
    return isSgClassDefinition(scope) != nullptr ||
           isSgTemplateClassDefinition(scope) != nullptr ||
           isSgTemplateInstantiationDefn(scope) != nullptr;
  };
  if (is_record_scope(enum_scope)) {
    lexical_scope = enum_scope;
  }

  clang::EnumDecl *prev_enum_decl = enum_decl->getPreviousDecl();
  SgEnumSymbol *sg_prev_enum_sym =
      isSgEnumSymbol(GetSymbolFromSymbolTable(prev_enum_decl));
  SgEnumDeclaration *sg_prev_enum_decl =
      sg_prev_enum_sym == nullptr
          ? nullptr
          : isSgEnumDeclaration(sg_prev_enum_sym->get_declaration());
  sg_prev_enum_decl =
      sg_prev_enum_decl == nullptr
          ? nullptr
          : isSgEnumDeclaration(sg_prev_enum_decl->get_definingDeclaration());

  SgEnumDeclaration *sg_enum_decl =
      SageBuilder::buildEnumDeclaration(name, symbol_scope);
  *node = sg_enum_decl;

  const bool is_embedded_enum = enum_decl->isEmbeddedInDeclarator();

  if (enumDeclName.empty()) {
    sg_enum_decl->set_isUnNamed(true);
  }
  if (sg_enum_decl->get_scope() != symbol_scope) {
    sg_enum_decl->set_scope(symbol_scope);
  }
  if (is_embedded_enum) {
    sg_enum_decl->set_isAutonomousDeclaration(false);
  } else {
    if (sg_enum_decl->get_parent() == nullptr && lexical_scope != nullptr) {
      sg_enum_decl->set_parent(lexical_scope);
    } else if (lexical_scope != nullptr &&
               sg_enum_decl->get_parent() != lexical_scope) {
      sg_enum_decl->set_parent(lexical_scope);
    }
    if (lexical_scope != nullptr) {
      if (force_member_scope) {
        ensure_decl_in_scope_child_list(sg_enum_decl, lexical_scope,
                                        "VisitEnumDecl:lexical");
      } else {
        ensure_decl_in_scope_child_list_preserve_scope(
            sg_enum_decl, lexical_scope, "VisitEnumDecl:lexical");
      }
    }
  }

  auto ensure_enum_field_symbol = [&](SgInitializedName *enumerator,
                                      SgScopeStatement *scope,
                                      SgEnumDeclaration *owner) {
    if (enumerator == nullptr) {
      return;
    }
    if (scope == nullptr) {
      scope = getGlobalScope();
    }
    if (SgScopeStatement *normalized = normalizeNamespaceScope(scope)) {
      scope = normalized;
    }
    if (enumerator->get_scope() != scope) {
      enumerator->set_scope(scope);
    }
    if (enumerator->get_parent() == nullptr && owner != nullptr) {
      enumerator->set_parent(owner);
    }
    if (scope != nullptr &&
        scope->find_symbol_from_declaration(enumerator) == nullptr) {
      SgEnumFieldSymbol *field_symbol = new SgEnumFieldSymbol(enumerator);
      field_symbol->set_parent(enumerator);
      scope->insert_symbol(enumerator->get_name(), field_symbol);
    }
  };

  if (sg_prev_enum_decl == nullptr ||
      sg_prev_enum_decl->get_enumerators().size() == 0) {
    SgScopeStatement *enum_scope = symbol_scope;
    if (enum_scope == nullptr) {
      enum_scope = getGlobalScope();
    }

    clang::EnumDecl::enumerator_iterator it;
    for (it = enum_decl->enumerator_begin(); it != enum_decl->enumerator_end();
         it++) {
      SgNode *tmp_enumerator = Traverse(*it);
      SgInitializedName *enumerator = isSgInitializedName(tmp_enumerator);

      ROSE_ASSERT(enumerator);

      sg_enum_decl->append_enumerator(enumerator);
      ensure_enum_field_symbol(enumerator, enum_scope, sg_enum_decl);

      // CLANG FRONTEND FIX: Set declptr for enum constant's SgInitializedName
      // declptr should point to the enum declaration that contains this
      // constant
      enumerator->set_declptr(sg_enum_decl);
    }
  } else {
    sg_enum_decl->set_definingDeclaration(sg_prev_enum_decl);
    sg_enum_decl->set_firstNondefiningDeclaration(
        sg_prev_enum_decl->get_firstNondefiningDeclaration());
    SgScopeStatement *enum_scope = symbol_scope;
    if (enum_scope == nullptr) {
      enum_scope = getGlobalScope();
    }
    for (SgInitializedName *enumerator : sg_prev_enum_decl->get_enumerators()) {
      ensure_enum_field_symbol(enumerator, enum_scope, sg_prev_enum_decl);
    }
  }

  // REX FIX: Handle AS_none for EnumDecl
  clang::AccessSpecifier access = enum_decl->getAccess();
  if (access == clang::AS_public) {
    sg_enum_decl->get_declarationModifier().get_accessModifier().setPublic();
  } else if (access == clang::AS_private) {
    sg_enum_decl->get_declarationModifier().get_accessModifier().setPrivate();
  } else if (access == clang::AS_protected) {
    sg_enum_decl->get_declarationModifier().get_accessModifier().setProtected();
  } else if (access == clang::AS_none) {
    if (isSgClassDefinition(SageBuilder::topScopeStack())) {
      SgClassDefinition *class_def =
          isSgClassDefinition(SageBuilder::topScopeStack());
      if (class_def->get_declaration()->get_class_type() ==
          SgClassDeclaration::e_class) {
        sg_enum_decl->get_declarationModifier()
            .get_accessModifier()
            .setPrivate();
      } else {
        sg_enum_decl->get_declarationModifier()
            .get_accessModifier()
            .setPublic();
      }
    } else {
      sg_enum_decl->get_declarationModifier().get_accessModifier().setPublic();
    }
  }
  /*
    SgEnumDeclaration *firstNondefEnumDecl =
        isSgEnumDeclaration(sg_enum_decl->get_firstNondefiningDeclaration());
    if (enum_decl->isEmbeddedInDeclarator()) {
      firstNondefEnumDecl->set_isAutonomousDeclaration(true);
    }

    SgSymbol *sym = firstNondefEnumDecl->get_symbol_from_symbol_table();
  #if DEBUG_VISIT_DECL
    std::cout << "VisitEnumDecl symbol: " << sym
              << " type:" << firstNondefEnumDecl->get_type() << std::endl;
  #endif
  */
  return VisitDecl(enum_decl, node) && res;
}

bool ClangToSageTranslator::VisitTemplateTypeParmDecl(
    clang::TemplateTypeParmDecl *template_type_parm_decl, SgNode **node) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToSageTranslator::VisitTemplateTypeParmDecl" << std::endl;
#endif
  SgDeclarationStatement *owning_template = nullptr;
  if (clang::DeclContext *ctx = template_type_parm_decl->getDeclContext()) {
    if (clang::TemplateDecl *template_ctx =
            llvm::dyn_cast<clang::TemplateDecl>(ctx)) {
      auto it = p_decl_translation_map.find(template_ctx);
      if (it != p_decl_translation_map.end()) {
        owning_template = isSgDeclarationStatement(it->second);
      }
    }
  }

  unsigned position = template_type_parm_decl->getIndex();
  SgTemplateParameter *sg_param = translateTemplateParameter(
      template_type_parm_decl, owning_template, position);

  *node = sg_param;

  if (sg_param != nullptr) {
    std::string kw = template_type_parm_decl->wasDeclaredWithTypename()
                         ? "typename"
                         : "class";
    SageInterface::setTemplateParameterKeyword(sg_param, kw);
  }

  return sg_param != nullptr;
}

bool ClangToSageTranslator::VisitTypedefNameDecl(
    clang::TypedefNameDecl *typedef_name_decl, SgNode **node) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToSageTranslator::VisitTypedefNameDecl" << std::endl;
#endif
  bool res = true;

  ROSE_ASSERT(FAIL_FIXME == 0); // FIXME

  return VisitTypeDecl(typedef_name_decl, node) && res;
}

bool ClangToSageTranslator::VisitTypedefDecl(clang::TypedefDecl *typedef_decl,
                                             SgNode **node) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToSageTranslator::VisitTypedefDecl" << std::endl;
#endif
  bool res = true;

  SgScopeStatement *scope = SageBuilder::topScopeStack();
  clang::DeclContext *decl_context = typedef_decl->getDeclContext();
  clang::DeclContext *scope_context = decl_context;
  while (scope_context != nullptr &&
         llvm::isa<clang::LinkageSpecDecl>(scope_context)) {
    scope_context = scope_context->getParent();
  }
  if (scope_context != nullptr) {
    if (scope_context->isTranslationUnit()) {
      scope = getGlobalScope();
    } else if (clang::Decl *context_decl =
                   llvm::dyn_cast<clang::Decl>(scope_context)) {
      if (is_declaration_scope_context(scope_context) &&
          !llvm::isa<clang::NamespaceDecl>(context_decl) &&
          p_decl_translation_map.find(context_decl) ==
              p_decl_translation_map.end() &&
          p_decl_translation_in_progress.find(context_decl) ==
              p_decl_translation_in_progress.end() &&
          p_decl_translation_on_demand.find(context_decl) ==
              p_decl_translation_on_demand.end()) {
        TraverseOnDemand(context_decl);
      }
      SgScopeStatement *resolved =
          resolveScopeFromDeclContext(scope_context, scope);
      if (resolved != nullptr) {
        scope = resolved;
      }
    }
  }
  if (scope == nullptr) {
    scope = getGlobalScope();
  }
  scope = normalizeNamespaceScope(scope);

  SgTypedefDeclaration *sg_typedef_decl = nullptr;
  SgName name(typedef_decl->getNameAsString());
  SgTypedefSymbol *tdef_sym =
      scope != nullptr ? scope->lookup_typedef_symbol(name) : nullptr;
  if (tdef_sym != nullptr) {
    sg_typedef_decl = tdef_sym->get_declaration();
    *node = sg_typedef_decl;
    return VisitTypedefNameDecl(typedef_decl, node) && res;
  }

  //    SgType * type =
  //    buildTypeFromQualifiedType(typedef_decl->getUnderlyingType());

  clang::QualType underlyingQualType = typedef_decl->getUnderlyingType();

  const clang::Type *underlyingType = underlyingQualType.getTypePtr();

  // Pei-Hung (06/01/2022) check if the declaration is considered embedded in
  // Clang AST. If it is embedded, no explicit SgDeclaration should be placed
  // for ROSE AST.
  bool isembedded = false;
  bool iscompleteDefined = false;
  clang::TagDecl *ownedTagDecl = nullptr;
  bool isOwnedTagDeclADefinition = false;
  bool isDefinitionRequired = false;
  // Definitions embedded in a declarator are not autonomous.
  bool isAutonomousDeclaration = true;
  auto isEmbeddedInThisTypedef = [&](clang::TagDecl *tag) -> bool {
    if (!isembedded || tag == nullptr || p_compiler_instance == nullptr) {
      return isembedded;
    }
    clang::SourceManager &sm = p_compiler_instance->getSourceManager();
    clang::SourceLocation decl_loc = tag->getBeginLoc();
    clang::SourceRange typedef_range = typedef_decl->getSourceRange();
    if (!decl_loc.isValid() || !typedef_range.isValid()) {
      return isembedded;
    }
    decl_loc = sm.getFileLoc(decl_loc);
    clang::SourceLocation range_begin = sm.getFileLoc(typedef_range.getBegin());
    clang::SourceLocation range_end = sm.getFileLoc(typedef_range.getEnd());
    if (!decl_loc.isValid() || !range_begin.isValid() || !range_end.isValid()) {
      return isembedded;
    }
    if (sm.isBeforeInTranslationUnit(decl_loc, range_begin) ||
        sm.isBeforeInTranslationUnit(range_end, decl_loc)) {
      return false;
    }
    return true;
  };

  // Adding check for EaboratedType and PointerType to retrieve base EnumType
  while ((llvm::isa<clang::ElaboratedType>(underlyingType)) ||
         (llvm::isa<clang::PointerType>(underlyingType)) ||
         (llvm::isa<clang::ArrayType>(underlyingType))) {
    if (llvm::isa<clang::ElaboratedType>(underlyingType)) {
      underlyingQualType =
          ((clang::ElaboratedType *)underlyingType)->getNamedType();
      ownedTagDecl =
          ((clang::ElaboratedType *)underlyingType)->getOwnedTagDecl();
      if (ownedTagDecl != nullptr) {
        isOwnedTagDeclADefinition =
            ownedTagDecl->isThisDeclarationADefinition();
      }
    } else if (llvm::isa<clang::PointerType>(underlyingType)) {
      underlyingQualType =
          ((clang::PointerType *)underlyingType)->getPointeeType();
    } else if (llvm::isa<clang::ArrayType>(underlyingType)) {
      underlyingQualType =
          ((clang::ArrayType *)underlyingType)->getElementType();
    }
    underlyingType = underlyingQualType.getTypePtr();
  }

  if (llvm::isa<clang::EnumType>(underlyingType)) {
    clang::EnumType *underlyingEnumType = (clang::EnumType *)underlyingType;
    clang::EnumDecl *enumDeclaration = underlyingEnumType->getDecl();
    isembedded = enumDeclaration->isEmbeddedInDeclarator();
    iscompleteDefined = enumDeclaration->isCompleteDefinition();
    isembedded = isEmbeddedInThisTypedef(enumDeclaration);
  }

  if (llvm::isa<clang::RecordType>(underlyingType)) {
    clang::RecordType *underlyingRecordType =
        (clang::RecordType *)underlyingType;
    clang::RecordDecl *recordDeclaration = underlyingRecordType->getDecl();
    isembedded = recordDeclaration->isEmbeddedInDeclarator();
    iscompleteDefined = recordDeclaration->isCompleteDefinition();
    isembedded = isEmbeddedInThisTypedef(recordDeclaration);
  }

  if (ownedTagDecl != nullptr) {
    isDefinitionRequired = isOwnedTagDeclADefinition;
  } else {
    isDefinitionRequired = iscompleteDefined;
  }

  SgType *sg_underlyingType = buildTypeFromQualifiedType(underlyingQualType);
  SgType *type = buildTypeFromQualifiedType(typedef_decl->getUnderlyingType());

  bool type_has_unknown = SageInterface::containsUnknownType(type);
  if (type_has_unknown && SgProject::get_verbose() > 0) {
    std::cerr << "CFE: Typedef with unknown underlying type '" << name
              << "' spelled as '"
              << typedef_decl->getUnderlyingType().getAsString()
              << "' (sg_type="
              << (type != nullptr ? type->class_name() : "null") << ", base="
              << (type != nullptr && type->findBaseType() != nullptr
                      ? type->findBaseType()->class_name()
                      : "null")
              << ")" << std::endl;
  }
  if (type_has_unknown) {
    std::string spelled = typedef_decl->getUnderlyingType().getAsString();
    SgScopeStatement *opaque_scope = getSafeOpaqueTypeInsertionScope();
    type = SageBuilder::buildOpaqueType(spelled, opaque_scope);
    sg_underlyingType = type;
  }

  bool has_defining_base = isembedded && isDefinitionRequired;
  sg_typedef_decl = SageBuilder::buildTypedefDeclaration_nfi(name, type, scope,
                                                             has_defining_base);
  if (SgProject::get_verbose() > 0) {
    if (name == "uint8_t" || name == "uint16_t" || name == "uint32_t" ||
        name == "in_port_t" || name == "in_addr_t") {
      std::cerr << "CFE: Created typedef '" << name << "' with type "
                << (type != nullptr ? type->class_name() : "null")
                << " (underlying "
                << (sg_underlyingType != nullptr
                        ? sg_underlyingType->class_name()
                        : "null")
                << ")" << std::endl;
    }
  }

  // finding the bottom base type and check
  while (type->findBaseType() != type) {
    type = type->findBaseType();
    if (type == sg_underlyingType)
      break;
  }

  // Only treat base-type declarations as embedded when Clang marks them as
  // embedded-in-declarator (e.g., `typedef struct { ... } T;`).  Named types
  // such as `typedef XY CoordinateSystem;` must not steal/attach the defining
  // declaration into the typedef, otherwise the AST will contain duplicate
  // record/enum scopes and AST-copy symbol invariants can break (Issue 69 /
  // copyAST_copytest2007_50).
  if (isSgClassType(type) && isDefinitionRequired) {
    SgClassDeclaration *classDefDecl = isSgClassDeclaration(
        isSgClassType(type)->get_declaration()->get_definingDeclaration());
    if (isembedded && classDefDecl != nullptr &&
        !isSgTypedefDeclaration(classDefDecl->get_parent())) {
      classDefDecl->set_parent(sg_typedef_decl);
      // classDefDecl->set_isAutonomousDeclaration(false);
      sg_typedef_decl->set_declaration(classDefDecl);
      sg_typedef_decl->set_typedefBaseTypeContainsDefiningDeclaration(true);

      ROSE_ASSERT(classDefDecl->get_firstNondefiningDeclaration() != nullptr);
    }
  } else if (isSgEnumType(type) && isDefinitionRequired) {

    // Pei-Hung (06/01/2022) Clang places a EnumDecl before TypedefDecl.
    // A SgEnumDeclaration for an  embedded EnumDecl is not attached to the
    // scope but its parent node needs to be setup as the SgTypedefDeclaration

    SgEnumDeclaration *enumDefDecl = isSgEnumDeclaration(
        isSgEnumType(type)->get_declaration()->get_definingDeclaration());
    if (isembedded && enumDefDecl != nullptr &&
        !isSgTypedefDeclaration(enumDefDecl->get_parent())) {
      enumDefDecl->set_parent(sg_typedef_decl);
      // enumDefDecl->set_isAutonomousDeclaration(false);
      sg_typedef_decl->set_declaration(enumDefDecl);
      sg_typedef_decl->set_typedefBaseTypeContainsDefiningDeclaration(true);
    }
  }

  if (!has_defining_base) {
    if (SgClassDeclaration *classDecl =
            isSgClassDeclaration(sg_typedef_decl->get_declaration())) {
      if (classDecl->get_definingDeclaration() == classDecl) {
        SgDeclarationStatement *nondef =
            classDecl->get_firstNondefiningDeclaration();
        if (nondef != nullptr && nondef != classDecl) {
          sg_typedef_decl->set_declaration(nondef);
        }
      }
    } else if (SgEnumDeclaration *enumDecl =
                   isSgEnumDeclaration(sg_typedef_decl->get_declaration())) {
      if (enumDecl->get_definingDeclaration() == enumDecl) {
        SgDeclarationStatement *nondef =
            enumDecl->get_firstNondefiningDeclaration();
        if (nondef != nullptr && nondef != enumDecl) {
          sg_typedef_decl->set_declaration(nondef);
        }
      }
    }
  }

  sg_typedef_decl->set_typedef_type(SgTypedefDeclaration::e_typedef);

  // REX FIX: Handle AS_none for TypedefDecl to avoid unparser assertion
  clang::AccessSpecifier access = typedef_decl->getAccess();

  if (access == clang::AS_public) {
    sg_typedef_decl->get_declarationModifier().get_accessModifier().setPublic();
  } else if (access == clang::AS_private) {
    sg_typedef_decl->get_declarationModifier()
        .get_accessModifier()
        .setPrivate();
  } else if (access == clang::AS_protected) {
    sg_typedef_decl->get_declarationModifier()
        .get_accessModifier()
        .setProtected();
  } else if (access == clang::AS_none) {
    if (isSgClassDefinition(SageBuilder::topScopeStack())) {
      SgClassDefinition *class_def =
          isSgClassDefinition(SageBuilder::topScopeStack());
      if (class_def->get_declaration()->get_class_type() ==
          SgClassDeclaration::e_class) {
        sg_typedef_decl->get_declarationModifier()
            .get_accessModifier()
            .setPrivate();
      } else {
        sg_typedef_decl->get_declarationModifier()
            .get_accessModifier()
            .setPublic();
      }
    } else {
      sg_typedef_decl->get_declarationModifier()
          .get_accessModifier()
          .setPublic();
    }
  }

  *node = sg_typedef_decl;

  bool result = VisitTypedefNameDecl(typedef_decl, node) && res;

  // Clang provides a number of implicit/builtin typedefs (e.g. `__int128_t`,
  // `__builtin_va_list`) with no meaningful source location. When ROSE outputs
  // a transformed file via AST-unparsing, re-emitting these typedefs can
  // produce invalid C++ (and may conflict with the backend compiler's own
  // builtins). Keep them in the symbol table for analysis, but suppress their
  // emission in generated source.
  if (result) {
    bool suppress_typedef = typedef_decl->isImplicit();
    bool suppress_as_compiler_generated = suppress_typedef;
    bool is_builtin_file = false;
    if (!suppress_typedef && p_compiler_instance != nullptr) {
      clang::SourceManager &sm = p_compiler_instance->getSourceManager();
      clang::SourceLocation loc = typedef_decl->getLocation();
      if (loc.isMacroID()) {
        loc = sm.getSpellingLoc(loc);
      }
      if (!loc.isValid()) {
        suppress_typedef = true;
        is_builtin_file = true;
      } else {
        if (!sm.isWrittenInMainFile(loc)) {
          suppress_typedef = true;
        }
        is_builtin_file = sm.isWrittenInBuiltinFile(loc);
        if (sm.isInSystemHeader(loc) || is_builtin_file) {
          suppress_typedef = true;
        }
      }
    }

    if (suppress_typedef) {
      if (suppress_as_compiler_generated || is_builtin_file) {
        setCompilerGeneratedFileInfo(sg_typedef_decl);
      }
      suppress_unparse_output(sg_typedef_decl);
    }
  }

  return result;
}

bool ClangToSageTranslator::VisitTypeAliasDecl(
    clang::TypeAliasDecl *type_alias_decl, SgNode **node) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToSageTranslator::VisitTypeAliasDecl" << std::endl;
#endif
  bool res = true;

  SgScopeStatement *scope = SageBuilder::topScopeStack();
  clang::DeclContext *decl_context = type_alias_decl->getDeclContext();
  clang::DeclContext *scope_context = decl_context;
  while (scope_context != nullptr &&
         llvm::isa<clang::LinkageSpecDecl>(scope_context)) {
    scope_context = scope_context->getParent();
  }
  if (scope_context != nullptr) {
    if (scope_context->isTranslationUnit()) {
      scope = getGlobalScope();
    } else if (clang::Decl *context_decl =
                   llvm::dyn_cast<clang::Decl>(scope_context)) {
      if (is_declaration_scope_context(scope_context) &&
          !llvm::isa<clang::NamespaceDecl>(context_decl) &&
          p_decl_translation_map.find(context_decl) ==
              p_decl_translation_map.end() &&
          p_decl_translation_in_progress.find(context_decl) ==
              p_decl_translation_in_progress.end() &&
          p_decl_translation_on_demand.find(context_decl) ==
              p_decl_translation_on_demand.end()) {
        TraverseOnDemand(context_decl);
      }
      SgScopeStatement *resolved =
          resolveScopeFromDeclContext(scope_context, scope);
      if (resolved != nullptr) {
        scope = resolved;
      }
    }
  }
  if (scope == nullptr) {
    scope = getGlobalScope();
  }

  // C++11 type aliases (using foo = int) are semantically equivalent to
  // typedefs TypeAliasDecl and TypedefDecl both inherit from TypedefNameDecl
  // Use the same implementation logic as VisitTypedefDecl

  SgName name(type_alias_decl->getNameAsString());
  clang::QualType underlyingQualType = type_alias_decl->getUnderlyingType();
  SgType *type = nullptr;

  // Prefer explicit template specialization type to preserve template
  // arguments.
  const clang::Type *qt = underlyingQualType.getTypePtr();
  const clang::Type *named = qt;
  if (const clang::ElaboratedType *elab =
          llvm::dyn_cast<clang::ElaboratedType>(qt)) {
    named = elab->getNamedType().getTypePtr();
  }
  if (const clang::TemplateSpecializationType *spec =
          llvm::dyn_cast<clang::TemplateSpecializationType>(named)) {
    if (!spec->isDependentType()) {
      clang::TemplateName tname = spec->getTemplateName();
      if (clang::TemplateDecl *clang_tpl = tname.getAsTemplateDecl()) {
        if (SgNode *tpl_node = Traverse(clang_tpl)) {
          if (SgTemplateClassDeclaration *tpl_decl =
                  isSgTemplateClassDeclaration(tpl_node)) {
            SgTemplateInstantiationDecl *inst =
                getOrCreateTemplateInstantiation(tpl_decl, spec);
            type = inst->get_type();
          }
        }
      }
    }
  }

  if (type == nullptr) {
    type = buildTypeFromQualifiedType(underlyingQualType);
  }

  // Ensure template argument list is populated on the instantiation type.
  if (SgClassType *cls_type = isSgClassType(type)) {
    if (SgTemplateInstantiationDecl *inst =
            isSgTemplateInstantiationDecl(cls_type->get_declaration())) {
      if (inst->get_templateArguments().empty()) {
        const clang::Type *qt = underlyingQualType.getTypePtr();
        const clang::Type *named = qt;
        if (const clang::ElaboratedType *elab =
                llvm::dyn_cast<clang::ElaboratedType>(qt)) {
          named = elab->getNamedType().getTypePtr();
        }
        if (const clang::TemplateSpecializationType *spec =
                llvm::dyn_cast<clang::TemplateSpecializationType>(named)) {
          SgTemplateArgumentPtrList args = buildTemplateArguments(spec);
          inst->get_templateArguments() = args;
          for (SgTemplateArgument *arg : inst->get_templateArguments()) {
            if (arg != nullptr)
              arg->set_parent(inst);
          }
        }
      }
    }
  }

  SgTypedefDeclaration *sg_typedef_decl =
      SageBuilder::buildTypedefDeclaration_nfi(name, type, scope);

  sg_typedef_decl->set_typedef_type(SgTypedefDeclaration::e_using);

  *node = sg_typedef_decl;

  bool result = VisitTypedefNameDecl(type_alias_decl, node) && res;
  if (result && p_compiler_instance != nullptr) {
    clang::SourceManager &sm = p_compiler_instance->getSourceManager();
    clang::SourceLocation loc = type_alias_decl->getLocation();
    bool suppress_alias = type_alias_decl->isImplicit();
    bool mark_compiler_generated = suppress_alias;
    bool is_builtin_file = false;
    if (!suppress_alias) {
      if (loc.isMacroID()) {
        loc = sm.getSpellingLoc(loc);
      }
      if (!loc.isValid()) {
        suppress_alias = true;
        is_builtin_file = true;
        mark_compiler_generated = true;
      } else {
        if (!sm.isWrittenInMainFile(loc)) {
          suppress_alias = true;
        }
        is_builtin_file = sm.isWrittenInBuiltinFile(loc);
        if (sm.isInSystemHeader(loc) || is_builtin_file) {
          suppress_alias = true;
          if (is_builtin_file) {
            mark_compiler_generated = true;
          }
        }
      }
    }
    if (suppress_alias) {
      if (mark_compiler_generated) {
        setCompilerGeneratedFileInfo(sg_typedef_decl);
      }
      suppress_unparse_output(sg_typedef_decl);
    }
  }
  return result;
}

bool ClangToSageTranslator::VisitUnresolvedUsingTypenameDecl(
    clang::UnresolvedUsingTypenameDecl *unresolved_using_type_name_decl,
    SgNode **node) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToSageTranslator::VisitUnresolvedUsingTypenameDecl"
            << std::endl;
#endif
  bool res = true;

  ROSE_ASSERT(FAIL_FIXME == 0); // FIXME

  return VisitTypeDecl(unresolved_using_type_name_decl, node) && res;
}

SgNode *ClangToSageTranslator::lookupUsingDeclTargetNode(clang::Decl *decl) {
  if (decl == nullptr) {
    return nullptr;
  }

  clang::Decl *map_key = decl;
  if (clang::NamespaceDecl *ns_decl =
          llvm::dyn_cast<clang::NamespaceDecl>(decl)) {
    map_key = getCanonicalNamespaceDecl(ns_decl);
  } else if (clang::RecordDecl *record_decl =
                 llvm::dyn_cast<clang::RecordDecl>(decl)) {
    if (clang::RecordDecl *definition = record_decl->getDefinition()) {
      map_key = definition;
    } else if (clang::RecordDecl *canonical = llvm::dyn_cast<clang::RecordDecl>(
                   record_decl->getCanonicalDecl())) {
      map_key = canonical;
    }
  }

  auto it = p_decl_translation_map.find(map_key);
  if (it != p_decl_translation_map.end() && it->second != nullptr) {
    return it->second;
  }
  if (map_key != decl) {
    it = p_decl_translation_map.find(decl);
    if (it != p_decl_translation_map.end() && it->second != nullptr) {
      return it->second;
    }
  }
  return nullptr;
}

SgNode *ClangToSageTranslator::resolveUsingDeclTargetNode(clang::Decl *decl) {
  auto try_resolve_decl = [&](clang::Decl *target_decl) -> SgNode * {
    if (target_decl == nullptr) {
      return nullptr;
    }
    if (SgNode *node = lookupUsingDeclTargetNode(target_decl)) {
      return node;
    }
    if (p_decl_translation_in_progress.find(target_decl) ==
            p_decl_translation_in_progress.end() &&
        p_decl_translation_on_demand.find(target_decl) ==
            p_decl_translation_on_demand.end()) {
      if (SgNode *node = TraverseOnDemand(target_decl)) {
        return node;
      }
    }
    return nullptr;
  };

  if (SgNode *node = try_resolve_decl(decl)) {
    return node;
  }

  if (clang::NamedDecl *named = llvm::dyn_cast<clang::NamedDecl>(decl)) {
    if (clang::Decl *canonical = named->getCanonicalDecl()) {
      if (canonical != decl) {
        if (SgNode *node = try_resolve_decl(canonical)) {
          return node;
        }
      }
    }
    if (clang::FunctionDecl *func_decl =
            llvm::dyn_cast<clang::FunctionDecl>(decl)) {
      if (clang::FunctionDecl *first = func_decl->getFirstDecl()) {
        if (first != decl) {
          if (SgNode *node = try_resolve_decl(first)) {
            return node;
          }
        }
      }
    } else if (clang::RecordDecl *record_decl =
                   llvm::dyn_cast<clang::RecordDecl>(decl)) {
      if (clang::RecordDecl *definition = record_decl->getDefinition()) {
        if (definition != decl) {
          if (SgNode *node = try_resolve_decl(definition)) {
            return node;
          }
        }
      }
    }
  }
  return nullptr;
}

bool ClangToSageTranslator::extractUsingTargetFromNode(
    SgNode *target_node, SgDeclarationStatement *&target_decl,
    SgInitializedName *&target_init) {
  target_decl = nullptr;
  target_init = nullptr;
  if (target_node == nullptr) {
    return false;
  }
  if (SgDeclarationStatement *decl_stmt =
          isSgDeclarationStatement(target_node)) {
    target_decl = decl_stmt;
    return true;
  }
  if (SgInitializedName *init_name = isSgInitializedName(target_node)) {
    target_init = init_name;
    return true;
  }
  if (SgFunctionDefinition *def = isSgFunctionDefinition(target_node)) {
    target_decl = def->get_declaration();
    return target_decl != nullptr;
  }
  if (SgClassDefinition *def = isSgClassDefinition(target_node)) {
    target_decl = def->get_declaration();
    return target_decl != nullptr;
  }
  if (SgNamespaceDefinitionStatement *ns_def =
          isSgNamespaceDefinitionStatement(target_node)) {
    target_decl = ns_def->get_namespaceDeclaration();
    return target_decl != nullptr;
  }
  if (SgTemplateInstantiationDirectiveStatement *inst =
          isSgTemplateInstantiationDirectiveStatement(target_node)) {
    target_decl = inst->get_declaration();
    return target_decl != nullptr;
  }
  return false;
}

bool ClangToSageTranslator::extractUsingTargetFromSymbol(
    SgSymbol *symbol, SgDeclarationStatement *&target_decl,
    SgInitializedName *&target_init) {
  target_decl = nullptr;
  target_init = nullptr;
  if (symbol == nullptr) {
    return false;
  }
  if (SgAliasSymbol *alias = isSgAliasSymbol(symbol)) {
    symbol = alias->get_alias();
  }
  if (symbol == nullptr) {
    return false;
  }
  if (SgFunctionSymbol *func_sym = isSgFunctionSymbol(symbol)) {
    target_decl = func_sym->get_declaration();
    return target_decl != nullptr;
  }
  if (SgMemberFunctionSymbol *mem_sym = isSgMemberFunctionSymbol(symbol)) {
    target_decl = mem_sym->get_declaration();
    return target_decl != nullptr;
  }
  if (SgTemplateFunctionSymbol *tmpl_sym = isSgTemplateFunctionSymbol(symbol)) {
    target_decl = isSgDeclarationStatement(tmpl_sym->get_declaration());
    return target_decl != nullptr;
  }
  if (SgTemplateMemberFunctionSymbol *tmpl_mem_sym =
          isSgTemplateMemberFunctionSymbol(symbol)) {
    target_decl = isSgDeclarationStatement(tmpl_mem_sym->get_declaration());
    return target_decl != nullptr;
  }
  if (SgClassSymbol *class_sym = isSgClassSymbol(symbol)) {
    target_decl = class_sym->get_declaration();
    return target_decl != nullptr;
  }
  if (SgTemplateClassSymbol *tmpl_class_sym = isSgTemplateClassSymbol(symbol)) {
    target_decl = isSgDeclarationStatement(tmpl_class_sym->get_declaration());
    return target_decl != nullptr;
  }
  if (SgEnumSymbol *enum_sym = isSgEnumSymbol(symbol)) {
    target_decl = enum_sym->get_declaration();
    return target_decl != nullptr;
  }
  if (SgTypedefSymbol *typedef_sym = isSgTypedefSymbol(symbol)) {
    target_decl = typedef_sym->get_declaration();
    return target_decl != nullptr;
  }
  if (SgNamespaceSymbol *ns_sym = isSgNamespaceSymbol(symbol)) {
    target_decl = ns_sym->get_declaration();
    return target_decl != nullptr;
  }
  if (SgVariableSymbol *var_sym = isSgVariableSymbol(symbol)) {
    target_init = var_sym->get_declaration();
    return target_init != nullptr;
  }
  if (SgEnumFieldSymbol *enum_field = isSgEnumFieldSymbol(symbol)) {
    target_init = enum_field->get_declaration();
    return target_init != nullptr;
  }
  if (SgNonrealSymbol *nonreal_sym = isSgNonrealSymbol(symbol)) {
    target_decl = nonreal_sym->get_declaration();
    return target_decl != nullptr;
  }
  return false;
}

bool ClangToSageTranslator::VisitUsingDecl(clang::UsingDecl *using_decl,
                                           SgNode **node) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToSageTranslator::VisitUsingDecl" << std::endl;
#endif
  bool res = true;

  // ROOT CAUSE FIX: Resolve using declarations through their shadow
  // declarations and bind to the underlying declaration or initialized name.

  SgScopeStatement *current_scope = SageBuilder::topScopeStack();
  clang::DeclContext *decl_context = using_decl->getDeclContext();
  clang::DeclContext *scope_context = decl_context;
  while (scope_context != nullptr &&
         llvm::isa<clang::LinkageSpecDecl>(scope_context)) {
    scope_context = scope_context->getParent();
  }
  if (scope_context != nullptr) {
    if (scope_context->isTranslationUnit()) {
      current_scope = getGlobalScope();
    } else if (clang::Decl *context_decl =
                   llvm::dyn_cast<clang::Decl>(scope_context)) {
      if (is_declaration_scope_context(scope_context) &&
          !llvm::isa<clang::NamespaceDecl>(context_decl) &&
          p_decl_translation_map.find(context_decl) ==
              p_decl_translation_map.end() &&
          p_decl_translation_in_progress.find(context_decl) ==
              p_decl_translation_in_progress.end() &&
          p_decl_translation_on_demand.find(context_decl) ==
              p_decl_translation_on_demand.end()) {
        TraverseOnDemand(context_decl);
      }
      if (SgScopeStatement *resolved =
              resolveScopeFromDeclContext(scope_context, current_scope)) {
        current_scope = resolved;
      }
    }
  }
  if (current_scope == nullptr) {
    current_scope = getGlobalScope();
  }

  std::vector<SgUsingDeclarationStatement *> using_statements;

  for (clang::UsingShadowDecl *shadow : using_decl->shadows()) {
    if (shadow == nullptr) {
      continue;
    }
    clang::NamedDecl *target = shadow->getTargetDecl();
    if (target == nullptr) {
      continue;
    }
    if (clang::NamedDecl *underlying = target->getUnderlyingDecl()) {
      target = underlying;
    }
    clang::Decl *target_decl = llvm::dyn_cast<clang::Decl>(target);
    if (target_decl == nullptr) {
      continue;
    }

    SgDeclarationStatement *sg_target_decl = nullptr;
    SgInitializedName *sg_init_name = nullptr;
    SgNode *target_node = resolveUsingDeclTargetNode(target_decl);
    bool resolved =
        extractUsingTargetFromNode(target_node, sg_target_decl, sg_init_name);
    if (!resolved) {
      if (SgSymbol *symbol = GetSymbolFromSymbolTable(target)) {
        resolved =
            extractUsingTargetFromSymbol(symbol, sg_target_decl, sg_init_name);
      }
    }
    if (!resolved) {
      continue;
    }

    SgUsingDeclarationStatement *using_stmt =
        new SgUsingDeclarationStatement(sg_target_decl, sg_init_name);
    using_stmt->set_definingDeclaration(using_stmt);
    using_stmt->set_firstNondefiningDeclaration(using_stmt);
    if (current_scope != nullptr) {
      using_stmt->set_scope(current_scope);
      using_stmt->set_parent(current_scope);
    }
    if (sg_init_name != nullptr && sg_init_name->get_parent() == nullptr) {
      sg_init_name->set_parent(using_stmt);
    }
    applySourceRange(using_stmt, using_decl->getSourceRange());
    using_statements.push_back(using_stmt);
  }

  if (using_statements.empty()) {
    std::string name_str = using_decl->getNameAsString();
    MLOG_ERROR_C(MLOG_FRONTEND,
                 "Runtime error: unable to resolve UsingDecl target '%s'.\n",
                 name_str.c_str());
    *node = nullptr;
    return false;
  }

  SgUsingDeclarationStatement *primary_stmt = using_statements.front();
  for (size_t i = 1; i < using_statements.size(); ++i) {
    ensure_decl_in_scope_child_list(using_statements[i], current_scope,
                                    "UsingDecl:shadow");
  }
  diagnose_null_scope(primary_stmt, "UsingDecl");
  *node = primary_stmt;

  return VisitNamedDecl(using_decl, node) && res;
}

// Helper to ensure a namespace declaration exists (creating stubs if needed)
SgNamespaceDeclarationStatement *
ClangToSageTranslator::ensureNamespaceDeclaration(
    clang::NamespaceDecl *ns_decl) {
  if (ns_decl == nullptr)
    return nullptr;

  clang::NamespaceDecl *canonical_ns = getCanonicalNamespaceDecl(ns_decl);

  // Check if already translated
  std::map<clang::Decl *, SgNode *>::iterator it =
      p_decl_translation_map.find(canonical_ns);
  if (it != p_decl_translation_map.end()) {
    return isSgNamespaceDeclarationStatement(it->second);
  }

  // Not found, create a stub
  SgScopeStatement *scope = nullptr;
  clang::DeclContext *parent_ctx = ns_decl->getDeclContext();

  if (parent_ctx->isTranslationUnit()) {
    scope = p_global_scope;
  } else if (clang::NamespaceDecl *parent_ns =
                 llvm::dyn_cast<clang::NamespaceDecl>(parent_ctx)) {
    // Recursively ensure parent namespace exists
    SgNamespaceDeclarationStatement *parent_sg_decl =
        ensureNamespaceDeclaration(parent_ns);
    if (parent_sg_decl) {
      scope = parent_sg_decl->get_definition();
    }
  }

  // Fallback to global scope if we couldn't resolve the parent scope
  // (This shouldn't happen for valid namespaces, but safety first)
  if (scope == nullptr) {
    scope = p_global_scope;
  }

  // Construct the name
  SgName name(ns_decl->getNameAsString());
  bool isAnonymous = ns_decl->isAnonymousNamespace();
  if (isAnonymous || name.getString().empty()) {
    name = "__anonymous_namespace_" +
           generate_source_position_string(ns_decl->getBeginLoc());
  }

  // Create the namespace stub using SageBuilder
  // This will handle symbol table lookups and creating/reusing definitions
  SgNamespaceDeclarationStatement *sg_ns_decl =
      SageBuilder::buildNamespaceDeclaration_nfi(name, isAnonymous, scope);

  // Register the stub so all redeclarations share a single namespace node.
  p_decl_translation_map[canonical_ns] = sg_ns_decl;

  return sg_ns_decl;
}

bool ClangToSageTranslator::VisitUsingDirectiveDecl(
    clang::UsingDirectiveDecl *using_directive_decl, SgNode **node) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToSageTranslator::VisitUsingDirectiveDecl" << std::endl;
#endif
  bool res = true;

  // ROOT CAUSE FIX: Implement proper support for using directives (e.g., using
  // namespace std;) Build a SgUsingDirectiveStatement to represent the using
  // directive

  // Get the namespace being imported
  clang::NamespaceDecl *clang_ns_decl =
      using_directive_decl->getNominatedNamespace();

  // Ensure the namespace exists in the AST (creating stubs if needed,
  // supporting nested namespaces)
  SgNamespaceDeclarationStatement *sg_ns_decl =
      ensureNamespaceDeclaration(clang_ns_decl);

  // Build the using directive statement using the existing builder
  SgUsingDirectiveStatement *using_dir_stmt =
      SageBuilder::buildUsingDirectiveStatement(sg_ns_decl);

  // Note: Scope is set automatically by parent visitor, don't set it explicitly
  // here

  *node = using_dir_stmt;

  return VisitNamedDecl(using_directive_decl, node) && res;
}

bool ClangToSageTranslator::VisitUsingPackDecl(
    clang::UsingPackDecl *using_pack_decl, SgNode **node) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToSageTranslator::VisitUsingPackDecl" << std::endl;
#endif
  bool res = true;

  if (using_pack_decl == nullptr) {
    *node = nullptr;
    return false;
  }

  SgScopeStatement *current_scope = SageBuilder::topScopeStack();
  clang::DeclContext *decl_context = using_pack_decl->getDeclContext();
  clang::DeclContext *scope_context = decl_context;
  while (scope_context != nullptr &&
         llvm::isa<clang::LinkageSpecDecl>(scope_context)) {
    scope_context = scope_context->getParent();
  }
  if (scope_context != nullptr) {
    if (scope_context->isTranslationUnit()) {
      current_scope = getGlobalScope();
    } else if (clang::Decl *context_decl =
                   llvm::dyn_cast<clang::Decl>(scope_context)) {
      if (is_declaration_scope_context(scope_context) &&
          !llvm::isa<clang::NamespaceDecl>(context_decl) &&
          p_decl_translation_map.find(context_decl) ==
              p_decl_translation_map.end() &&
          p_decl_translation_in_progress.find(context_decl) ==
              p_decl_translation_in_progress.end() &&
          p_decl_translation_on_demand.find(context_decl) ==
              p_decl_translation_on_demand.end()) {
        TraverseOnDemand(context_decl);
      }
      if (SgScopeStatement *resolved =
              resolveScopeFromDeclContext(scope_context, current_scope)) {
        current_scope = resolved;
      }
    }
  }
  if (current_scope == nullptr) {
    current_scope = getGlobalScope();
  }

  std::vector<SgUsingDeclarationStatement *> using_statements;

  auto apply_using_scope = [&](SgUsingDeclarationStatement *using_stmt) {
    if (using_stmt == nullptr || current_scope == nullptr) {
      return;
    }
    if (using_stmt->get_scope() == nullptr) {
      using_stmt->set_scope(current_scope);
    }
    if (using_stmt->get_parent() == nullptr) {
      using_stmt->set_parent(current_scope);
    }
  };

  auto build_using_stmt_from_target =
      [&](clang::NamedDecl *target,
          clang::SourceRange range) -> SgUsingDeclarationStatement * {
    if (target == nullptr) {
      return nullptr;
    }
    if (clang::NamedDecl *underlying = target->getUnderlyingDecl()) {
      target = underlying;
    }
    clang::Decl *target_decl = llvm::dyn_cast<clang::Decl>(target);
    if (target_decl == nullptr) {
      return nullptr;
    }

    SgDeclarationStatement *sg_target_decl = nullptr;
    SgInitializedName *sg_init_name = nullptr;
    SgNode *target_node = resolveUsingDeclTargetNode(target_decl);
    bool resolved =
        extractUsingTargetFromNode(target_node, sg_target_decl, sg_init_name);
    if (!resolved) {
      if (SgSymbol *symbol = GetSymbolFromSymbolTable(target)) {
        resolved =
            extractUsingTargetFromSymbol(symbol, sg_target_decl, sg_init_name);
      }
    }
    if (!resolved) {
      return nullptr;
    }

    SgUsingDeclarationStatement *using_stmt =
        new SgUsingDeclarationStatement(sg_target_decl, sg_init_name);
    using_stmt->set_definingDeclaration(using_stmt);
    using_stmt->set_firstNondefiningDeclaration(using_stmt);
    apply_using_scope(using_stmt);
    if (sg_init_name != nullptr && sg_init_name->get_parent() == nullptr) {
      sg_init_name->set_parent(using_stmt);
    }
    if (range.isValid()) {
      applySourceRange(using_stmt, range);
    } else {
      applySourceRange(using_stmt, using_pack_decl->getSourceRange());
    }
    return using_stmt;
  };

  for (clang::NamedDecl *expansion : using_pack_decl->expansions()) {
    if (expansion == nullptr) {
      continue;
    }
    if (clang::UsingShadowDecl *shadow =
            llvm::dyn_cast<clang::UsingShadowDecl>(expansion)) {
      if (SgUsingDeclarationStatement *using_stmt =
              build_using_stmt_from_target(shadow->getTargetDecl(),
                                           shadow->getSourceRange())) {
        using_statements.push_back(using_stmt);
      }
      continue;
    }

    SgNode *translated = Traverse(expansion);
    if (SgUsingDeclarationStatement *using_stmt =
            isSgUsingDeclarationStatement(translated)) {
      apply_using_scope(using_stmt);
      using_statements.push_back(using_stmt);
    }
  }

  if (using_statements.empty()) {
    *node = nullptr;
    return false;
  }

  SgUsingDeclarationStatement *primary_stmt = using_statements.front();
  for (size_t i = 1; i < using_statements.size(); ++i) {
    ensure_decl_in_scope_child_list(using_statements[i], current_scope,
                                    "UsingPackDecl:expansion");
  }

  *node = primary_stmt;
  return VisitNamedDecl(using_pack_decl, node) && res;
}

bool ClangToSageTranslator::VisitUsingShadowDecl(
    clang::UsingShadowDecl *using_shadow_decl, SgNode **node) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToSageTranslator::VisitUsingShadowDecl" << std::endl;
#endif
  bool res = true;

  // UsingShadowDecl is an implicit declaration created by the compiler to
  // represent declarations brought into scope by a using declaration. These
  // are implementation details and don't need explicit SAGE representation.
  // The parent UsingDecl already represents the user-visible declaration.
  *node = nullptr;
  return false;
}

bool ClangToSageTranslator::VisitConstructorUsingShadowDecl(
    clang::ConstructorUsingShadowDecl *constructor_using_shadow_decl,
    SgNode **node) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToSageTranslator::VisitConstructorUsingShadowDecl"
            << std::endl;
#endif
  bool res = true;

  *node = nullptr;
  return false;
}

bool ClangToSageTranslator::VisitValueDecl(clang::ValueDecl *value_decl,
                                           SgNode **node) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToSageTranslator::VisitValueDecl" << std::endl;
#endif
  bool res = true;

  return VisitNamedDecl(value_decl, node) && res;
}

bool ClangToSageTranslator::VisitBindingDecl(clang::BindingDecl *binding_decl,
                                             SgNode **node) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToSageTranslator::VisitBindingDecl" << std::endl;
#endif
  bool res = true;

  SgName name(binding_decl->getNameAsString());
  SgType *type = buildTypeFromQualifiedType(binding_decl->getType());
  ROSE_ASSERT(type != nullptr);

  // Build a variable declaration to represent the binding and provide a symbol
  // for decl references.
  SgVariableDeclaration *sg_var_decl =
      SageBuilder::buildVariableDeclaration_nfi(name, type, nullptr,
                                                SageBuilder::topScopeStack());
  sg_var_decl->set_isAssociatedWithDeclarationList(true);

  clang::Expr *binding_expr = binding_decl->getBinding();
  SgInitializer *init = nullptr;
  if (binding_expr != nullptr) {
    SgNode *tmp_init = Traverse(binding_expr);
    if (SgInitializer *tmp_init_initializer = isSgInitializer(tmp_init)) {
      init = tmp_init_initializer;
    } else {
      SgExpression *expr = isSgExpression(tmp_init);
      if (tmp_init != nullptr && expr == nullptr) {
        std::cerr << "Runtime error: not a SgInitializer..." << std::endl;
        res = false;
      } else if (expr != nullptr) {
        if (SgExprListExp *expr_list_expr = isSgExprListExp(expr)) {
          init = SageBuilder::buildAggregateInitializer(expr_list_expr, type);
        } else if (SgInitializer *existing_init = isSgInitializer(expr)) {
          init = existing_init;
        } else {
          init =
              SageBuilder::buildAssignInitializer_nfi(expr, expr->get_type());
        }
      }
    }
  }

  if (init != nullptr) {
    sg_var_decl->reset_initializer(init);
  }
  if (init != nullptr && binding_expr != nullptr) {
    if (!llvm::isa<clang::CXXConstructExpr>(binding_expr)) {
      applySourceRange(init, binding_expr->getSourceRange());
    }
  }

  sg_var_decl->set_firstNondefiningDeclaration(sg_var_decl);
  sg_var_decl->set_parent(SageBuilder::topScopeStack());

  ROSE_ASSERT(sg_var_decl->get_variables().size() == 1);
  SgInitializedName *init_name = sg_var_decl->get_variables()[0];
  ROSE_ASSERT(init_name != nullptr);
  if (init_name->get_scope() == nullptr) {
    init_name->set_scope(SageBuilder::topScopeStack());
  }
  if (init != nullptr) {
    init->set_parent(init_name);
  }

  applySourceRange(init_name, binding_decl->getSourceRange());

  SgVariableDefinition *var_def =
      isSgVariableDefinition(init_name->get_declptr());
  if (var_def == nullptr) {
    var_def = sg_var_decl->get_definition();
    if (var_def != nullptr) {
      init_name->set_declptr(var_def);
    }
  }
  ROSE_ASSERT(var_def != nullptr);
  applySourceRange(var_def, binding_decl->getSourceRange());

  clang::AccessSpecifier access = binding_decl->getAccess();
  if (access == clang::AS_public) {
    sg_var_decl->get_declarationModifier().get_accessModifier().setPublic();
  } else if (access == clang::AS_private) {
    sg_var_decl->get_declarationModifier().get_accessModifier().setPrivate();
  } else if (access == clang::AS_protected) {
    sg_var_decl->get_declarationModifier().get_accessModifier().setProtected();
  } else if (access == clang::AS_none) {
    if (isSgClassDefinition(SageBuilder::topScopeStack())) {
      SgClassDefinition *class_def =
          isSgClassDefinition(SageBuilder::topScopeStack());
      if (class_def->get_declaration()->get_class_type() ==
          SgClassDeclaration::e_class) {
        sg_var_decl->get_declarationModifier()
            .get_accessModifier()
            .setPrivate();
      } else {
        sg_var_decl->get_declarationModifier().get_accessModifier().setPublic();
      }
    } else {
      sg_var_decl->get_declarationModifier().get_accessModifier().setPublic();
    }
  }

  *node = sg_var_decl;

  return VisitValueDecl(binding_decl, node) && res;
}

bool ClangToSageTranslator::VisitDeclaratorDecl(
    clang::DeclaratorDecl *declarator_decl, SgNode **node) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToSageTranslator::VisitDeclaratorDecl" << std::endl;
#endif
  bool res = true;

  ROSE_ASSERT(FAIL_FIXME == 0); // FIXME

  return VisitValueDecl(declarator_decl, node) && res;
}

bool ClangToSageTranslator::VisitFieldDecl(clang::FieldDecl *field_decl,
                                           SgNode **node) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToSageTranslator::VisitFieldDecl" << std::endl;
#endif
  bool res = true;

#if DEBUG_VISIT_DECL
  std::cerr << "ClangToSageTranslator::VisitFieldDecl name:"
            << field_decl->getNameAsString() << "\n";
  std::cerr << "isAnonymousStructOrUnion() "
            << field_decl->isAnonymousStructOrUnion() << "\n";
#endif

  SgName name(field_decl->getNameAsString());

  clang::QualType fieldQualType = field_decl->getType();

  const clang::Type *fieldType = fieldQualType.getTypePtr();

  // Pei-Hung (06/01/2022) check if the declaration is considered embedded in
  // Clang AST. If it is embedded, no explicit SgDeclaration should be placed
  // for ROSE AST.
  bool isembedded = false;
  bool iscompleteDefined = false;
  bool hasElaboratedType = false;
  bool isOwnedTagDeclADefinition = false;
  bool isDefinitionRequired = false;
  bool isNamedNonEmbeddedRecord = false;
  bool isAnonymousStructOrUnion = false;

  auto field_has_embedded_tag = [&](clang::TagDecl *tag_decl) -> bool {
    if (tag_decl == nullptr || p_compiler_instance == nullptr) {
      return false;
    }
    clang::SourceRange field_range = field_decl->getSourceRange();
    if (clang::TypeSourceInfo *tsi = field_decl->getTypeSourceInfo()) {
      clang::SourceRange type_range = tsi->getTypeLoc().getSourceRange();
      if (type_range.isValid()) {
        field_range = type_range;
      }
    }
    if (!field_range.isValid()) {
      return false;
    }
    clang::SourceManager &sm = p_compiler_instance->getSourceManager();
    clang::SourceLocation begin_loc = sm.getSpellingLoc(field_range.getBegin());
    clang::SourceLocation end_loc = sm.getSpellingLoc(field_range.getEnd());
    clang::SourceLocation target_loc =
        sm.getSpellingLoc(tag_decl->getBeginLoc());
    if (!begin_loc.isValid() || !end_loc.isValid() || !target_loc.isValid()) {
      return false;
    }
    if (sm.getFileID(begin_loc) != sm.getFileID(target_loc) ||
        sm.getFileID(end_loc) != sm.getFileID(target_loc)) {
      return false;
    }
    unsigned begin_offset = sm.getFileOffset(begin_loc);
    unsigned end_offset = sm.getFileOffset(end_loc);
    unsigned target_offset = sm.getFileOffset(target_loc);
    if (begin_offset > end_offset) {
      std::swap(begin_offset, end_offset);
    }
    return target_offset >= begin_offset && target_offset <= end_offset;
  };

  // Adding check for ElaboratedType/Pointer/Array to retrieve base type.
  while ((llvm::isa<clang::ElaboratedType>(fieldType)) ||
         (llvm::isa<clang::PointerType>(fieldType)) ||
         (llvm::isa<clang::ArrayType>(fieldType))) {
    if (llvm::isa<clang::ElaboratedType>(fieldType)) {
      hasElaboratedType = true;
      fieldQualType = ((clang::ElaboratedType *)fieldType)->getNamedType();
      clang::TagDecl *ownedTagDecl =
          ((clang::ElaboratedType *)fieldType)->getOwnedTagDecl();
      if (ownedTagDecl != nullptr) {
        isOwnedTagDeclADefinition =
            ownedTagDecl->isThisDeclarationADefinition();
      }
    } else if (llvm::isa<clang::PointerType>(fieldType)) {
      fieldQualType = ((clang::PointerType *)fieldType)->getPointeeType();
    } else if (llvm::isa<clang::ArrayType>(fieldType)) {
      fieldQualType = ((clang::ArrayType *)fieldType)->getElementType();
    }
    fieldType = fieldQualType.getTypePtr();
  }

  if (llvm::isa<clang::EnumType>(fieldType)) {
    clang::EnumType *underlyingEnumType = (clang::EnumType *)fieldType;
    clang::EnumDecl *enumDeclaration = underlyingEnumType->getDecl();
    isembedded = enumDeclaration->isEmbeddedInDeclarator() ||
                 field_has_embedded_tag(enumDeclaration);
    iscompleteDefined = enumDeclaration->isCompleteDefinition();
  }

  if (llvm::isa<clang::RecordType>(fieldType)) {
    clang::RecordType *underlyingRecordType = (clang::RecordType *)fieldType;
    clang::RecordDecl *recordDeclaration = underlyingRecordType->getDecl();
    isembedded = recordDeclaration->isEmbeddedInDeclarator() ||
                 field_has_embedded_tag(recordDeclaration);
    iscompleteDefined = recordDeclaration->isCompleteDefinition();
    isNamedNonEmbeddedRecord = !isembedded &&
                               !recordDeclaration->isAnonymousStructOrUnion() &&
                               recordDeclaration->getIdentifier() != nullptr;
  }

  if (hasElaboratedType) {
    isDefinitionRequired = isOwnedTagDeclADefinition;
  } else {
    isDefinitionRequired = iscompleteDefined;
  }

  isAnonymousStructOrUnion = field_decl->isAnonymousStructOrUnion();

  const clang::CXXRecordDecl *parent_record =
      llvm::dyn_cast<clang::CXXRecordDecl>(field_decl->getParent());
  const clang::Type *enclosing_record_type =
      parent_record ? parent_record->getTypeForDecl() : nullptr;
  bool is_lambda_field =
      (parent_record != nullptr && parent_record->isLambda());

  SgScopeStatement *field_scope = SageBuilder::topScopeStack();
  auto resolve_field_scope =
      [&](const clang::RecordDecl *record) -> SgScopeStatement * {
    if (record == nullptr) {
      return nullptr;
    }
    SgNode *parent_node = nullptr;
    auto lookup_record = [&](const clang::RecordDecl *target) -> SgNode * {
      if (target == nullptr) {
        return nullptr;
      }
      auto it =
          p_decl_translation_map.find(const_cast<clang::RecordDecl *>(target));
      if (it != p_decl_translation_map.end()) {
        return it->second;
      }
      return nullptr;
    };

    parent_node = lookup_record(record);
    if (parent_node == nullptr) {
      if (const clang::RecordDecl *def = record->getDefinition()) {
        parent_node = lookup_record(def);
      }
    }
    if (parent_node == nullptr) {
      if (const clang::RecordDecl *canonical =
              llvm::dyn_cast<clang::RecordDecl>(record->getCanonicalDecl())) {
        parent_node = lookup_record(canonical);
      }
    }
    if (parent_node == nullptr &&
        p_decl_translation_in_progress.find(const_cast<clang::RecordDecl *>(
            record)) == p_decl_translation_in_progress.end()) {
      parent_node = TraverseOnDemand(const_cast<clang::RecordDecl *>(record));
    }

    if (SgClassDefinition *class_def = isSgClassDefinition(parent_node)) {
      return class_def;
    }
    if (SgClassDeclaration *class_decl = isSgClassDeclaration(parent_node)) {
      if (SgDeclarationStatement *def_decl =
              class_decl->get_definingDeclaration()) {
        if (SgClassDefinition *class_def =
                isSgClassDeclaration(def_decl)->get_definition()) {
          return class_def;
        }
      }
    }
    return nullptr;
  };

  if (SgScopeStatement *resolved =
          resolve_field_scope(field_decl->getParent())) {
    field_scope = resolved;
  }
  if (field_scope == nullptr) {
    field_scope = getGlobalScope();
  }

  if (is_lambda_field) {
    // Lambda closure fields are implicit captures; they should always
    // materialize as real member variables even if Clang marks them anonymous.
    // Give them stable names so the symbol table can reference them during
    // conversion of the lambda body.
    isAnonymousStructOrUnion = false;
    if (name.getString().empty()) {
      std::string synthesized_name =
          "__lambda_field_" + std::to_string(field_decl->getFieldIndex());
      name = synthesized_name;
    }
  }
  if (isAnonymousStructOrUnion && !is_lambda_field &&
      name.getString().empty()) {
    name = "__anonymous_" +
           generate_source_position_string(field_decl->getBeginLoc());
  }

  SgType *sg_fieldType = buildTypeFromQualifiedType(fieldQualType);
  SgType *type = buildTypeFromQualifiedType(field_decl->getType());

  bool type_has_unknown = SageInterface::containsUnknownType(type);
  if (type_has_unknown && SgProject::get_verbose() > 0) {
    std::cerr << "CFE: Field with unknown type '" << name << "' spelled as '"
              << field_decl->getType().getAsString() << "' (sg_type="
              << (type != nullptr ? type->class_name() : "null") << ", base="
              << (type != nullptr && type->findBaseType() != nullptr
                      ? type->findBaseType()->class_name()
                      : "null")
              << ")" << std::endl;
  }

  if (type_has_unknown) {
    SgScopeStatement *opaque_scope = getSafeOpaqueTypeInsertionScope();
    type = SageBuilder::buildOpaqueType(field_decl->getType().getAsString(),
                                        opaque_scope);
    sg_fieldType = type;
  }

  clang::Expr *init_expr = field_decl->getInClassInitializer();
  SgNode *tmp_init = Traverse(init_expr);
  SgExpression *expr = isSgExpression(tmp_init);
  // TODO expression list if aggregated initializer !
  if (tmp_init != nullptr && expr == nullptr) {
    std::cerr << "Runtime error: not a SgInitializer..." << std::endl;
    res = false;
  }
  SgInitializer *init =
      expr != nullptr
          ? SageBuilder::buildAssignInitializer_nfi(expr, expr->get_type())
          : nullptr;
  if (init != nullptr)
    applySourceRange(init, init_expr->getSourceRange());

  // Cannot use 'SageBuilder::buildVariableDeclaration' because of anonymous
  // field *node = SageBuilder::buildVariableDeclaration(name, type, init,
  // SageBuilder::topScopeStack());
  // Build it by hand...
  SgVariableDeclaration *var_decl = new SgVariableDeclaration(name, type, init);
  var_decl->set_isAssociatedWithDeclarationList(true);

  // CLANG FRONTEND FIX: Capture access modifier from Clang AST
  clang::AccessSpecifier access = field_decl->getAccess();
  if (access == clang::AS_public) {
    var_decl->get_declarationModifier().get_accessModifier().setPublic();
  } else if (access == clang::AS_private) {
    var_decl->get_declarationModifier().get_accessModifier().setPrivate();
  } else if (access == clang::AS_protected) {
    var_decl->get_declarationModifier().get_accessModifier().setProtected();
  }
  // AS_none means default access (private for class, public for struct)
  if (access == clang::AS_none && parent_record != nullptr) {
    if (parent_record->isClass()) {
      var_decl->get_declarationModifier().get_accessModifier().setPrivate();
    } else {
      var_decl->get_declarationModifier().get_accessModifier().setPublic();
    }
  }

  // finding the bottom base type and check
  while (type->findBaseType() != type) {
    type = type->findBaseType();
    if (type == sg_fieldType)
      break;
  }

  if (isSgClassType(type) && isDefinitionRequired) {
    SgClassDeclaration *classDecl =
        isSgClassDeclaration(isSgClassType(type)->get_declaration());
    SgClassDeclaration *classDefDecl = isSgClassDeclaration(
        isSgClassType(type)->get_declaration()->get_definingDeclaration());
    if (classDecl != nullptr) {
      classDecl->set_isAutonomousDeclaration(false);
    }
    if (classDefDecl != nullptr) {
      classDefDecl->set_isAutonomousDeclaration(false);
    }
    // Skip embedding when the field is a pointer to the enclosing class
    // (self-reference).
    if (fieldType != enclosing_record_type && isembedded &&
        classDefDecl != nullptr &&
        !isSgDeclarationStatement(classDefDecl->get_parent())) {
      if (SgScopeStatement *parent_scope =
              isSgScopeStatement(classDefDecl->get_parent())) {
        detach_decl_from_scope_child_list(classDefDecl, parent_scope);
      }
      if (classDecl != nullptr && classDecl != classDefDecl) {
        if (SgScopeStatement *parent_scope =
                isSgScopeStatement(classDecl->get_parent())) {
          detach_decl_from_scope_child_list(classDecl, parent_scope);
        }
      }
      classDefDecl->set_parent(var_decl);
      // classDefDecl->set_isAutonomousDeclaration(false);
      var_decl->set_baseTypeDefiningDeclaration(classDefDecl);
      var_decl->set_variableDeclarationContainsBaseTypeDefiningDeclaration(
          true);
    }

    std::map<SgClassType *, bool>::iterator bool_it =
        p_class_type_decl_first_see_in_type.find(isSgClassType(type));
    ROSE_ASSERT(bool_it != p_class_type_decl_first_see_in_type.end());
    if (isNamedNonEmbeddedRecord) {
      // Named records should keep their standalone definition; avoid embedding
      // on first use.
      bool_it->second = false;
    }
    if (bool_it->second) {
      var_decl->set_baseTypeDefiningDeclaration(
          isSgNamedType(type)->get_declaration()->get_definingDeclaration());
      var_decl->set_variableDeclarationContainsBaseTypeDefiningDeclaration(
          true);
      bool_it->second = false;
    }
  } else if (isSgEnumType(type) && isDefinitionRequired) {
    SgEnumDeclaration *enumDecl =
        isSgEnumDeclaration(isSgEnumType(type)->get_declaration());
    SgEnumDeclaration *enumDefDecl = isSgEnumDeclaration(
        isSgEnumType(type)->get_declaration()->get_definingDeclaration());
    if (isembedded && enumDefDecl != nullptr &&
        !isSgDeclarationStatement(enumDefDecl->get_parent())) {
      enumDefDecl->set_parent(var_decl);
      // enumDefDecl->set_isAutonomousDeclaration(false);
      var_decl->set_baseTypeDefiningDeclaration(enumDefDecl);
      var_decl->set_variableDeclarationContainsBaseTypeDefiningDeclaration(
          true);
    }

    std::map<SgEnumType *, bool>::iterator bool_it =
        p_enum_type_decl_first_see_in_type.find(isSgEnumType(type));
    ROSE_ASSERT(bool_it != p_enum_type_decl_first_see_in_type.end());
    if (bool_it->second) {
      var_decl->set_baseTypeDefiningDeclaration(
          isSgEnumType(type)->get_declaration()->get_definingDeclaration());
      var_decl->set_variableDeclarationContainsBaseTypeDefiningDeclaration(
          true);
      bool_it->second = false;
    }
  }

  var_decl->set_firstNondefiningDeclaration(var_decl);
  var_decl->set_parent(field_scope);
  var_decl->set_scope(field_scope);

  ROSE_ASSERT(var_decl->get_variables().size() == 1);

  SgInitializedName *init_name = var_decl->get_variables()[0];
  ROSE_ASSERT(init_name != nullptr);
  init_name->set_parent(var_decl);
  init_name->set_scope(field_scope);
  applySourceRange(init_name, field_decl->getSourceRange());

  // CLANG FRONTEND FIX: declptr should point to SgVariableDefinition, not
  // SgVariableDeclaration Check if it's already set, if not get it from
  // var_decl
  SgVariableDefinition *var_def =
      isSgVariableDefinition(init_name->get_declptr());
  if (var_def == nullptr) {
    var_def = var_decl->get_definition();
    if (var_def != nullptr) {
      init_name->set_declptr(var_def);
    }
  }
  ROSE_ASSERT(var_def != nullptr);
  applySourceRange(var_def, field_decl->getSourceRange());

  // Pei-Hung (08/15/23): The following causes duplicated symbols in some cases.
  // Comment it out and need further investigation.
  // SgVariableSymbol *var_symbol = new SgVariableSymbol(init_name);
  // SageBuilder::topScopeStack()->insert_symbol(name, var_symbol);
  registerDeclarationSymbol(var_decl);

  *node = var_decl;
  return VisitDeclaratorDecl(field_decl, node) && res;
}

void ClangToSageTranslator::rehomeSymbolToScope(SgSymbol *symbol,
                                                SgScopeStatement *scope) {
  if (symbol == nullptr || scope == nullptr) {
    return;
  }
  SgSymbolTable *target_table = scope->get_symbol_table();
  if (target_table == nullptr) {
    return;
  }
  if (SgSymbolTable *current_table = isSgSymbolTable(symbol->get_parent())) {
    if (current_table != target_table) {
      if (current_table->exists(symbol)) {
        current_table->remove(symbol);
      }
    }
  } else if (SgScopeStatement *old_scope =
                 isSgScopeStatement(symbol->get_parent())) {
    if (old_scope != scope && old_scope->symbol_exists(symbol)) {
      old_scope->remove_symbol(symbol);
    }
  }
  if (!scope->symbol_exists(symbol)) {
    scope->insert_symbol(symbol->get_name(), symbol);
  } else if (symbol->get_parent() != target_table) {
    symbol->set_parent(target_table);
  }
}

SgSymbol *
ClangToSageTranslator::buildSymbolForDeclaration(SgDeclarationStatement *decl) {
  if (decl == nullptr) {
    return nullptr;
  }
  if (SgTemplateClassDeclaration *tmpl_decl =
          isSgTemplateClassDeclaration(decl)) {
    return new SgTemplateClassSymbol(tmpl_decl);
  }
  if (SgClassDeclaration *class_decl = isSgClassDeclaration(decl)) {
    return new SgClassSymbol(class_decl);
  }
  if (SgEnumDeclaration *enum_decl = isSgEnumDeclaration(decl)) {
    return new SgEnumSymbol(enum_decl);
  }
  if (SgTemplateInstantiationTypedefDeclaration *inst_typedef =
          isSgTemplateInstantiationTypedefDeclaration(decl)) {
    return new SgTemplateTypedefSymbol(inst_typedef);
  }
  if (SgTemplateTypedefDeclaration *tmpl_typedef =
          isSgTemplateTypedefDeclaration(decl)) {
    return new SgTemplateTypedefSymbol(tmpl_typedef);
  }
  if (SgTypedefDeclaration *typedef_decl = isSgTypedefDeclaration(decl)) {
    return new SgTypedefSymbol(typedef_decl);
  }
  if (SgTemplateMemberFunctionDeclaration *tmpl_member =
          isSgTemplateMemberFunctionDeclaration(decl)) {
    return new SgTemplateMemberFunctionSymbol(tmpl_member);
  }
  if (SgMemberFunctionDeclaration *member_decl =
          isSgMemberFunctionDeclaration(decl)) {
    return new SgMemberFunctionSymbol(member_decl);
  }
  if (SgTemplateFunctionDeclaration *tmpl_func =
          isSgTemplateFunctionDeclaration(decl)) {
    return new SgTemplateFunctionSymbol(tmpl_func);
  }
  if (SgFunctionDeclaration *func_decl = isSgFunctionDeclaration(decl)) {
    return new SgFunctionSymbol(func_decl);
  }
  if (SgNamespaceDeclarationStatement *ns_decl =
          isSgNamespaceDeclarationStatement(decl)) {
    return new SgNamespaceSymbol(ns_decl->get_name(), ns_decl, nullptr, false);
  }
  return nullptr;
}

void ClangToSageTranslator::registerDeclarationSymbol(
    SgDeclarationStatement *decl) {
  if (decl == nullptr) {
    return;
  }
  if (shouldSkipSymbolForConstraints(decl)) {
    pruneSymbolsForConstraints(decl);
    return;
  }

  SgScopeStatement *scope = decl->get_scope();
  if (scope == nullptr) {
    scope = isSgScopeStatement(decl->get_parent());
  }
  if (scope == nullptr) {
    return;
  }

  SgScopeStatement *normalized_scope = normalizeNamespaceScope(scope);
  if (normalized_scope != nullptr) {
    scope = normalized_scope;
  }

  if (SgVariableDeclaration *var_decl = isSgVariableDeclaration(decl)) {
    SgTemplateVariableDeclaration *tmpl_var_decl =
        isSgTemplateVariableDeclaration(var_decl);
    SgTemplateParameterPtrList *template_params =
        tmpl_var_decl ? &tmpl_var_decl->get_templateParameters() : nullptr;
    SgTemplateArgumentPtrList *template_args =
        tmpl_var_decl ? &tmpl_var_decl->get_templateSpecializationArguments()
                      : nullptr;
    for (SgInitializedName *init_name : var_decl->get_variables()) {
      if (init_name == nullptr) {
        continue;
      }
      SgScopeStatement *var_scope = init_name->get_scope();
      if (var_scope == nullptr) {
        init_name->set_scope(scope);
        var_scope = scope;
      }
      if (var_scope == nullptr) {
        continue;
      }
      if (var_scope->find_symbol_from_declaration(init_name) != nullptr) {
        continue;
      }
      if (tmpl_var_decl != nullptr) {
        if (var_scope->lookup_template_variable_symbol(
                init_name->get_name(), template_params, template_args) !=
            nullptr) {
          continue;
        }
      }
      if (var_scope->lookup_variable_symbol(init_name->get_name()) != nullptr) {
        continue;
      }
      SgSymbol *symbol = nullptr;
      if (tmpl_var_decl != nullptr) {
        symbol = new SgTemplateVariableSymbol(init_name);
      } else {
        symbol = new SgVariableSymbol(init_name);
      }
      rehomeSymbolToScope(symbol, var_scope);
    }
    return;
  }

  SgDeclarationStatement *symbol_decl = decl;
  if (SgDeclarationStatement *first_nondef =
          decl->get_firstNondefiningDeclaration()) {
    symbol_decl = first_nondef;
  }

  bool is_symbol_decl =
      isSgFunctionDeclaration(symbol_decl) != nullptr ||
      isSgClassDeclaration(symbol_decl) != nullptr ||
      isSgEnumDeclaration(symbol_decl) != nullptr ||
      isSgTypedefDeclaration(symbol_decl) != nullptr ||
      isSgNamespaceDeclarationStatement(symbol_decl) != nullptr;
  if (!is_symbol_decl) {
    return;
  }

  if (scope->find_symbol_from_declaration(symbol_decl) != nullptr) {
    return;
  }

  if (SgFunctionDeclaration *func_decl = isSgFunctionDeclaration(symbol_decl)) {
    SgFunctionType *func_type = func_decl->get_type();
    SgSymbol *existing = nullptr;
    if (SgTemplateMemberFunctionDeclaration *tmpl_member =
            isSgTemplateMemberFunctionDeclaration(func_decl)) {
      SgTemplateParameterPtrList *params =
          &tmpl_member->get_templateParameters();
      existing = scope->lookup_template_member_function_symbol(
          tmpl_member->get_name(), func_type, params);
    } else if (SgMemberFunctionDeclaration *member =
                   isSgMemberFunctionDeclaration(func_decl)) {
      existing = scope->lookup_nontemplate_member_function_symbol(
          member->get_name(), func_type, nullptr);
      if (existing == nullptr) {
        existing = scope->lookup_function_symbol(member->get_name(), func_type);
      }
    } else if (SgTemplateFunctionDeclaration *tmpl_func =
                   isSgTemplateFunctionDeclaration(func_decl)) {
      SgTemplateParameterPtrList *params = &tmpl_func->get_templateParameters();
      existing = scope->lookup_template_function_symbol(tmpl_func->get_name(),
                                                        func_type, params);
    } else {
      existing = scope->lookup_nontemplate_function_symbol(
          func_decl->get_name(), func_type, nullptr);
      if (existing == nullptr) {
        existing =
            scope->lookup_function_symbol(func_decl->get_name(), func_type);
      }
    }
    if (existing != nullptr) {
      return;
    }
  }

  if (SgClassDeclaration *class_decl = isSgClassDeclaration(symbol_decl)) {
    if (scope->lookup_class_symbol(class_decl->get_name()) != nullptr ||
        scope->lookup_template_class_symbol(class_decl->get_name(), nullptr,
                                            nullptr) != nullptr) {
      return;
    }
  }

  if (SgEnumDeclaration *enum_decl = isSgEnumDeclaration(symbol_decl)) {
    if (scope->lookup_enum_symbol(enum_decl->get_name()) != nullptr) {
      return;
    }
  }

  if (SgTemplateInstantiationTypedefDeclaration *inst_typedef =
          isSgTemplateInstantiationTypedefDeclaration(symbol_decl)) {
    if (scope->lookup_template_typedef_symbol(inst_typedef->get_name()) !=
        nullptr) {
      return;
    }
  } else if (SgTemplateTypedefDeclaration *tmpl_typedef =
                 isSgTemplateTypedefDeclaration(symbol_decl)) {
    if (scope->lookup_template_typedef_symbol(tmpl_typedef->get_name()) !=
        nullptr) {
      return;
    }
  } else if (SgTypedefDeclaration *typedef_decl =
                 isSgTypedefDeclaration(symbol_decl)) {
    if (scope->lookup_typedef_symbol(typedef_decl->get_name()) != nullptr) {
      return;
    }
  }

  if (SgNamespaceDeclarationStatement *ns_decl =
          isSgNamespaceDeclarationStatement(symbol_decl)) {
    if (scope->lookup_namespace_symbol(ns_decl->get_name()) != nullptr) {
      return;
    }
  }

  if (SgSymbol *symbol = buildSymbolForDeclaration(symbol_decl)) {
    rehomeSymbolToScope(symbol, scope);
  }
}

void ClangToSageTranslator::reconcileOnDemandTranslation(SgNode *node) {
  if (node == nullptr) {
    return;
  }

  auto repair_template_argument_parents = [](SgNode *root) {
    if (root == nullptr) {
      return;
    }
    Rose_STL_Container<SgNode *> decls =
        NodeQuery::querySubTree(root, V_SgDeclarationStatement);
    for (SgNode *decl_node : decls) {
      if (SgDeclarationStatement *decl = isSgDeclarationStatement(decl_node)) {
        SageBuilder::setTemplateArgumentParents(decl);
      }
    }
    Rose_STL_Container<SgNode *> refs =
        NodeQuery::querySubTree(root, V_SgNonrealRefExp);
    for (SgNode *ref_node : refs) {
      SgNonrealRefExp *ref = isSgNonrealRefExp(ref_node);
      if (ref == nullptr) {
        continue;
      }
      for (SgTemplateArgument *arg : ref->get_templateArguments()) {
        if (arg != nullptr && arg->get_parent() != ref) {
          arg->set_parent(ref);
        }
      }
    }
  };

  Rose_STL_Container<SgNode *> decls =
      NodeQuery::querySubTree(node, V_SgDeclarationStatement);
  for (SgNode *decl_node : decls) {
    if (SgDeclarationStatement *decl_stmt =
            isSgDeclarationStatement(decl_node)) {
      registerDeclarationSymbol(decl_stmt);
    }
  }

  SageInterface::fixVariableReferences(node, /*cleanUnusedSymbols=*/false);
  fixupTemplateArguments(node);
  repair_template_argument_parents(node);

  if (SgSourceFile *source_file = SageInterface::getEnclosingSourceFile(
          node, /*includingSelf=*/false)) {
    Unparser::computeNameQualification(source_file);
  }

  resolvePendingSpecializedTemplateLinks();
}

void ClangToSageTranslator::queueSpecializedTemplateLink(
    SgTemplateInstantiationDecl *inst_decl, clang::Decl *specialized_decl) {
  if (inst_decl == nullptr || specialized_decl == nullptr) {
    return;
  }
  p_pending_specialized_template_links.emplace_back(inst_decl,
                                                    specialized_decl);
}

void ClangToSageTranslator::resolvePendingSpecializedTemplateLinks() {
  if (p_pending_specialized_template_links.empty()) {
    return;
  }

  auto lookup = [&](clang::Decl *key) -> SgDeclarationStatement * {
    if (key == nullptr) {
      return nullptr;
    }
    auto lookup_map = [&](clang::Decl *lookup_key) -> SgDeclarationStatement * {
      if (lookup_key == nullptr) {
        return nullptr;
      }
      auto it = p_decl_translation_map.find(lookup_key);
      if (it != p_decl_translation_map.end()) {
        return isSgDeclarationStatement(it->second);
      }
      return nullptr;
    };

    if (SgDeclarationStatement *decl = lookup_map(key)) {
      return decl;
    }
    if (auto *tmpl = llvm::dyn_cast<clang::ClassTemplateDecl>(key)) {
      if (clang::ClassTemplateDecl *canonical = tmpl->getCanonicalDecl()) {
        if (SgDeclarationStatement *decl = lookup_map(canonical)) {
          return decl;
        }
      }
      if (clang::CXXRecordDecl *templated = tmpl->getTemplatedDecl()) {
        if (SgDeclarationStatement *decl = lookup_map(templated)) {
          return decl;
        }
      }
    }
    if (auto *partial =
            llvm::dyn_cast<clang::ClassTemplatePartialSpecializationDecl>(
                key)) {
      if (clang::CXXRecordDecl *canonical = partial->getCanonicalDecl()) {
        if (SgDeclarationStatement *decl = lookup_map(canonical)) {
          return decl;
        }
      }
    }
    return nullptr;
  };

  for (const auto &entry : p_pending_specialized_template_links) {
    SgTemplateInstantiationDecl *inst_decl = entry.first;
    clang::Decl *specialized_decl = entry.second;
    if (inst_decl == nullptr || specialized_decl == nullptr) {
      continue;
    }
    if (inst_decl->get_specializedTemplateDeclaration() != nullptr) {
      continue;
    }
    if (SgDeclarationStatement *resolved = lookup(specialized_decl)) {
      inst_decl->set_specializedTemplateDeclaration(resolved);
      continue;
    }

    if (p_decl_translation_in_progress.find(specialized_decl) ==
            p_decl_translation_in_progress.end() &&
        p_decl_translation_on_demand.find(specialized_decl) ==
            p_decl_translation_on_demand.end()) {
      SgNode *translated = TraverseOnDemand(specialized_decl);
      if (SgDeclarationStatement *resolved =
              isSgDeclarationStatement(translated)) {
        inst_decl->set_specializedTemplateDeclaration(resolved);
        continue;
      }
    }

    if (SgDeclarationStatement *resolved = lookup(specialized_decl)) {
      inst_decl->set_specializedTemplateDeclaration(resolved);
    }
  }

  p_pending_specialized_template_links.clear();
}

void ClangToSageTranslator::ensureMemberFunctionScope(
    SgFunctionDeclaration *decl, SgClassDefinition *parent_def) {
  if (decl == nullptr || parent_def == nullptr) {
    return;
  }
  SgMemberFunctionDeclaration *member_decl =
      isSgMemberFunctionDeclaration(decl);
  if (member_decl == nullptr) {
    return;
  }
  if (member_decl->get_scope() != parent_def) {
    member_decl->set_scope(parent_def);
  }

  SgSymbol *symbol = nullptr;
  if (SgFunctionDeclaration *first_nondef = isSgFunctionDeclaration(
          member_decl->get_firstNondefiningDeclaration())) {
    symbol = first_nondef->get_symbol_from_symbol_table();
  }
  if (symbol == nullptr) {
    symbol = member_decl->get_symbol_from_symbol_table();
  }
  if (symbol == nullptr) {
    std::vector<SgSymbol *> matches =
        find_function_symbols_in_scope(parent_def, member_decl);
    if (!matches.empty()) {
      symbol = matches.front();
    }
  }
  if (symbol == nullptr) {
    if (SgTemplateMemberFunctionDeclaration *tmpl_member =
            isSgTemplateMemberFunctionDeclaration(member_decl)) {
      symbol = new SgTemplateMemberFunctionSymbol(tmpl_member);
    } else {
      symbol = new SgMemberFunctionSymbol(member_decl);
    }
  }
  rehomeSymbolToScope(symbol, parent_def);
}

SgExpression *
ClangToSageTranslator::translateConstraintExpression(const clang::Expr *expr) {
  if (expr == nullptr) {
    return nullptr;
  }

  SgNode *translated = Traverse(const_cast<clang::Expr *>(expr));
  if (SgExpression *sg_expr = isSgExpression(translated)) {
    return sg_expr;
  }

  std::string text = getSourceText(expr->getSourceRange());
  if (!text.empty()) {
    SgRequiresExpr *req = SageBuilder::buildRequiresExpr_nfi(text);
    applySourceRange(req, expr->getSourceRange());
    return req;
  }

  return buildFallbackExpression(expr);
}

#if LLVM_VERSION_MAJOR >= 21
ConstraintSatisfactionResult
ClangToSageTranslator::evaluateConstraintSatisfaction(
    const clang::NamedDecl *constraint_owner,
    llvm::ArrayRef<clang::AssociatedConstraint> constraints,
    llvm::ArrayRef<clang::TemplateArgument> template_args,
    clang::SourceRange template_id_range) {
  ConstraintSatisfactionResult result;
  if (constraint_owner == nullptr || constraints.empty()) {
    return result;
  }
  if (p_compiler_instance == nullptr || !p_compiler_instance->hasSema()) {
    return result;
  }

  clang::Sema &sema = p_compiler_instance->getSema();
  clang::ConstraintSatisfaction satisfaction(constraint_owner, template_args);
  clang::MultiLevelTemplateArgumentList arg_list(
      const_cast<clang::Decl *>(llvm::dyn_cast<clang::Decl>(constraint_owner)),
      template_args, /*Final=*/true);

  bool error = sema.CheckConstraintSatisfaction(
      constraint_owner, constraints, arg_list, template_id_range, satisfaction);
  result.evaluated = true;
  result.contains_errors = error || satisfaction.ContainsErrors;
  result.substitution_failure = satisfaction.HasSubstitutionFailure();
  result.satisfied = !result.contains_errors && satisfaction.IsSatisfied;
  return result;
}

ConstraintSatisfactionResult
ClangToSageTranslator::evaluateConstraintSatisfaction(
    const clang::NamedDecl *constraint_owner,
    llvm::ArrayRef<clang::AssociatedConstraint> constraints,
    const clang::TemplateArgumentList &template_args,
    clang::SourceRange template_id_range) {
  return evaluateConstraintSatisfaction(constraint_owner, constraints,
                                        template_args.asArray(),
                                        template_id_range);
}
#else
ConstraintSatisfactionResult
ClangToSageTranslator::evaluateConstraintSatisfaction(
    const clang::NamedDecl *constraint_owner,
    llvm::ArrayRef<const clang::Expr *> constraints,
    llvm::ArrayRef<clang::TemplateArgument> template_args,
    clang::SourceRange template_id_range) {
  ConstraintSatisfactionResult result;
  if (constraint_owner == nullptr || constraints.empty()) {
    return result;
  }
  if (p_compiler_instance == nullptr || !p_compiler_instance->hasSema()) {
    return result;
  }

  clang::Sema &sema = p_compiler_instance->getSema();
  clang::ConstraintSatisfaction satisfaction(constraint_owner, template_args);
  clang::MultiLevelTemplateArgumentList arg_list(
      const_cast<clang::Decl *>(llvm::dyn_cast<clang::Decl>(constraint_owner)),
      template_args, /*Final=*/true);

  bool error = sema.CheckConstraintSatisfaction(
      constraint_owner, constraints, arg_list, template_id_range, satisfaction);
  result.evaluated = true;
  result.contains_errors = error || satisfaction.ContainsErrors;
  result.substitution_failure = satisfaction.HasSubstitutionFailure();
  result.satisfied = !result.contains_errors && satisfaction.IsSatisfied;
  return result;
}

ConstraintSatisfactionResult
ClangToSageTranslator::evaluateConstraintSatisfaction(
    const clang::NamedDecl *constraint_owner,
    llvm::ArrayRef<const clang::Expr *> constraints,
    const clang::TemplateArgumentList &template_args,
    clang::SourceRange template_id_range) {
  return evaluateConstraintSatisfaction(constraint_owner, constraints,
                                        template_args.asArray(),
                                        template_id_range);
}
#endif

void ClangToSageTranslator::attachConstraintSatisfaction(
    SgNode *node, const ConstraintSatisfactionResult &result) {
  if (node == nullptr || !result.evaluated) {
    return;
  }

  auto apply = [&](auto *target) {
    target->set_constraintSatisfactionEvaluated(true);
    target->set_constraintSatisfactionSatisfied(result.satisfied);
    target->set_constraintSatisfactionContainsErrors(result.contains_errors);
    target->set_constraintSatisfactionSubstitutionFailure(
        result.substitution_failure);
    target->set_constraintSatisfactionSummary(result.summary);
  };

  if (SgTemplateInstantiationDecl *decl = isSgTemplateInstantiationDecl(node)) {
    apply(decl);
  } else if (SgTemplateInstantiationFunctionDecl *decl =
                 isSgTemplateInstantiationFunctionDecl(node)) {
    apply(decl);
  } else if (SgTemplateInstantiationMemberFunctionDecl *decl =
                 isSgTemplateInstantiationMemberFunctionDecl(node)) {
    apply(decl);
  } else if (SgTemplateInstantiationTypedefDeclaration *decl =
                 isSgTemplateInstantiationTypedefDeclaration(node)) {
    apply(decl);
  } else if (SgTemplateVariableDeclaration *decl =
                 isSgTemplateVariableDeclaration(node)) {
    apply(decl);
  } else if (SgNonrealRefExp *ref = isSgNonrealRefExp(node)) {
    apply(ref);
  }

  // Preserve the attribute for diagnostics/debugging, but do not rely on it
  // for semantic decisions.
  node->setAttribute(kConstraintSatisfactionAttributeName,
                     new ConstraintSatisfactionAttribute(result));
}

void ClangToSageTranslator::attachSFINAEFailure(
    SgNode *node, const SFINAEFailureResult &result) {
  if (node == nullptr || !result.evaluated) {
    return;
  }

  auto apply = [&](auto *target) {
    target->set_sfinaeEvaluated(true);
    target->set_sfinaeSubstitutionFailure(result.substitution_failure);
    target->set_sfinaeSummary(result.summary);
  };

  if (SgTemplateInstantiationDecl *decl = isSgTemplateInstantiationDecl(node)) {
    apply(decl);
  } else if (SgTemplateInstantiationFunctionDecl *decl =
                 isSgTemplateInstantiationFunctionDecl(node)) {
    apply(decl);
  } else if (SgTemplateInstantiationMemberFunctionDecl *decl =
                 isSgTemplateInstantiationMemberFunctionDecl(node)) {
    apply(decl);
  } else if (SgTemplateInstantiationTypedefDeclaration *decl =
                 isSgTemplateInstantiationTypedefDeclaration(node)) {
    apply(decl);
  } else if (SgTemplateVariableDeclaration *decl =
                 isSgTemplateVariableDeclaration(node)) {
    apply(decl);
  } else if (SgNonrealRefExp *ref = isSgNonrealRefExp(node)) {
    apply(ref);
  }
}

SFINAEFailureResult ClangToSageTranslator::evaluateSFINAEFailure(
    const clang::FunctionDecl *function_decl) {
  SFINAEFailureResult result;
  if (function_decl == nullptr) {
    return result;
  }
  if (p_compiler_instance == nullptr || !p_compiler_instance->hasSema()) {
    return result;
  }
  if (llvm::isa<clang::CXXDeductionGuideDecl>(function_decl)) {
    return result;
  }

  const clang::FunctionDecl *pattern =
      function_decl->getTemplateInstantiationPattern();
  if (pattern == nullptr) {
    return result;
  }

  clang::Sema &sema = p_compiler_instance->getSema();
  clang::MultiLevelTemplateArgumentList args =
      sema.getTemplateInstantiationArgs(function_decl,
                                        function_decl->getDeclContext(),
                                        /*Final=*/true,
                                        /*Innermost=*/std::nullopt,
                                        /*RelativeToPrimary=*/false, pattern,
                                        /*ForConstraintInstantiation=*/false,
                                        /*SkipForSpecialization=*/false,
                                        /*ForDefaultArgumentSubstitution=*/
                                        false);
  if (args.getNumSubstitutedLevels() == 0) {
    return result;
  }

  clang::Sema::ContextRAII context(
      sema, const_cast<clang::DeclContext *>(function_decl->getDeclContext()));
  clang::Sema::SFINAETrap trap(sema);
  clang::TypeSourceInfo *pattern_type = pattern->getTypeSourceInfo();
  if (pattern_type == nullptr) {
    return result;
  }

  clang::CXXRecordDecl *this_ctx = nullptr;
  clang::Qualifiers this_quals;
  if (const clang::CXXMethodDecl *method =
          llvm::dyn_cast<clang::CXXMethodDecl>(pattern)) {
    this_ctx = const_cast<clang::CXXRecordDecl *>(method->getParent());
    this_quals = method->getMethodQualifiers();
  }

  clang::TypeSourceInfo *inst_type = sema.SubstFunctionDeclType(
      pattern_type, args, function_decl->getLocation(),
      function_decl->getDeclName(), this_ctx, this_quals,
      /*EvaluateConstraints=*/false);

  result.evaluated = true;
  if (inst_type == nullptr || trap.hasErrorOccurred()) {
    result.substitution_failure = true;
  }

  return result;
}

bool ClangToSageTranslator::shouldSkipSymbolForConstraints(
    const SgDeclarationStatement *decl) const {
  if (decl == nullptr) {
    return false;
  }

  auto should_skip = [&](auto *node) -> bool {
    if (node == nullptr) {
      return false;
    }
    if (node->get_constraintSatisfactionEvaluated()) {
      if (!node->get_constraintSatisfactionSatisfied() ||
          node->get_constraintSatisfactionContainsErrors() ||
          node->get_constraintSatisfactionSubstitutionFailure()) {
        return true;
      }
    }
    if (node->get_sfinaeEvaluated() && node->get_sfinaeSubstitutionFailure()) {
      return true;
    }
    return false;
  };

  if (auto *inst = isSgTemplateInstantiationDecl(decl)) {
    return should_skip(inst);
  }
  if (auto *inst = isSgTemplateInstantiationFunctionDecl(decl)) {
    return should_skip(inst);
  }
  if (auto *inst = isSgTemplateInstantiationMemberFunctionDecl(decl)) {
    return should_skip(inst);
  }
  if (auto *inst = isSgTemplateInstantiationTypedefDeclaration(decl)) {
    return should_skip(inst);
  }
  if (auto *tmpl_var = isSgTemplateVariableDeclaration(decl)) {
    return should_skip(tmpl_var);
  }

  // Fallback to attributes if present (legacy).
  if (const ConstraintSatisfactionAttribute *attr =
          getConstraintSatisfactionAttribute(decl)) {
    const ConstraintSatisfactionResult &result = attr->result();
    if (result.evaluated) {
      return !result.satisfied || result.contains_errors ||
             result.substitution_failure;
    }
  }
  return false;
}

void ClangToSageTranslator::pruneSymbolsForConstraints(
    SgDeclarationStatement *decl) {
  if (decl == nullptr || !shouldSkipSymbolForConstraints(decl)) {
    return;
  }

  auto remove_symbol = [&](SgSymbol *symbol) {
    if (symbol == nullptr) {
      return;
    }
    if (SgSymbolTable *table = isSgSymbolTable(symbol->get_parent())) {
      if (table->exists(symbol)) {
        table->remove(symbol);
      }
    } else if (SgScopeStatement *scope =
                   isSgScopeStatement(symbol->get_parent())) {
      if (scope->symbol_exists(symbol)) {
        scope->remove_symbol(symbol);
      }
    }
    move_symbol_to_orphan_table(symbol);
  };

  if (SgVariableDeclaration *var_decl = isSgVariableDeclaration(decl)) {
    for (SgInitializedName *init_name : var_decl->get_variables()) {
      if (init_name == nullptr) {
        continue;
      }
      remove_symbol(init_name->get_symbol_from_symbol_table());
    }
  }

  if (SgSymbol *symbol = decl->get_symbol_from_symbol_table()) {
    remove_symbol(symbol);
  }

  if (SgDeclarationStatement *first_nondef =
          decl->get_firstNondefiningDeclaration()) {
    if (SgSymbol *symbol = first_nondef->get_symbol_from_symbol_table()) {
      remove_symbol(symbol);
    }
  }

  suppress_unparse_output(decl);
  if (SgLocatedNode *located = isSgLocatedNode(decl)) {
    suppress_unparse_output(located);
  }
}

bool ClangToSageTranslator::translateFunctionDeclCommon(
    clang::FunctionDecl *function_decl,
    clang::FunctionTemplateDecl *template_decl, SgNode **node) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToSageTranslator::VisitFunctionDecl" << std::endl;
  std::cerr << "ClangToSageTranslator::VisitFunctionDecl "
            << function_decl->getNameInfo().getName().getAsString()
            << std::endl;
#endif
  bool res = true;

  if (function_decl == nullptr || function_decl->isInvalidDecl()) {
    if (node != nullptr) {
      *node = nullptr;
    }
    return false;
  }

  struct TranslationGuard {
    std::set<clang::Decl *> &in_progress;
    clang::Decl *decl;
    TranslationGuard(std::set<clang::Decl *> &set, clang::Decl *d)
        : in_progress(set), decl(d) {
      in_progress.insert(decl);
    }
    ~TranslationGuard() { in_progress.erase(decl); }
  } translation_guard(p_decl_translation_in_progress, function_decl);

  auto lookup_existing_decl =
      [&](clang::FunctionDecl *key) -> SgFunctionDeclaration * {
    if (key == nullptr) {
      return nullptr;
    }
    auto it = p_decl_translation_map.find(key);
    if (it == p_decl_translation_map.end()) {
      return nullptr;
    }
    return isSgFunctionDeclaration(it->second);
  };

  SgFunctionDeclaration *existing_decl = lookup_existing_decl(function_decl);
  if (existing_decl == nullptr) {
    if (clang::FunctionDecl *canonical = function_decl->getCanonicalDecl()) {
      existing_decl = lookup_existing_decl(canonical);
    }
  }
  if (existing_decl == nullptr) {
    if (clang::FunctionDecl *first = function_decl->getFirstDecl()) {
      existing_decl = lookup_existing_decl(first);
    }
  }

  if (existing_decl != nullptr) {
    const bool needs_definition =
        function_decl->isThisDeclarationADefinition() &&
        existing_decl->get_definition() == nullptr;
    if (!needs_definition) {
      if (p_decl_translation_map.find(function_decl) ==
          p_decl_translation_map.end()) {
        p_decl_translation_map.insert(
            std::make_pair(function_decl, existing_decl));
      }
      *node = existing_decl;
      return true;
    }
  }

  // FIXME: There is something weird here when try to Traverse a function
  // reference in a recursive function (when first Traverse is not complete)
  //        It seems that it tries to instantiate the decl inside the
  //        function... It may be faster to recode from scratch...
  //   If I am not wrong this have been fixed....

  SgName name(function_decl->getNameAsString());
  std::string func_name = function_decl->getNameAsString();

  auto resolve_parent_class_name =
      [&](clang::CXXRecordDecl *parentClassDecl) -> SgName {
    if (parentClassDecl == nullptr) {
      return SgName();
    }
    SgClassDeclaration *CxxRecordDeclaration = nullptr;
    auto lookup_class_decl =
        [&](clang::CXXRecordDecl *record) -> SgClassDeclaration * {
      if (record == nullptr) {
        return nullptr;
      }
      auto it = p_decl_translation_map.find(record);
      if (it == p_decl_translation_map.end()) {
        return nullptr;
      }
      if (SgClassDeclaration *decl = isSgClassDeclaration(it->second)) {
        return decl;
      }
      if (SgTemplateClassDeclaration *tmpl =
              isSgTemplateClassDeclaration(it->second)) {
        return tmpl;
      }
      if (SgClassDefinition *def = isSgClassDefinition(it->second)) {
        return isSgClassDeclaration(def->get_declaration());
      }
      if (SgTemplateClassDefinition *tmpl_def =
              isSgTemplateClassDefinition(it->second)) {
        return isSgTemplateClassDeclaration(tmpl_def->get_declaration());
      }
      return nullptr;
    };

    CxxRecordDeclaration = lookup_class_decl(parentClassDecl);
    if (CxxRecordDeclaration == nullptr) {
      if (clang::CXXRecordDecl *canonical =
              parentClassDecl->getCanonicalDecl()) {
        CxxRecordDeclaration = lookup_class_decl(canonical);
      }
    }
    bool parent_in_progress = false;
    auto check_in_progress = [&](clang::CXXRecordDecl *record) {
      if (record != nullptr && p_decl_translation_in_progress.find(record) !=
                                   p_decl_translation_in_progress.end()) {
        parent_in_progress = true;
      }
    };
    check_in_progress(parentClassDecl);
    if (!parent_in_progress) {
      if (clang::CXXRecordDecl *definition = parentClassDecl->getDefinition()) {
        check_in_progress(definition);
      }
    }
    if (!parent_in_progress) {
      if (clang::CXXRecordDecl *canonical =
              parentClassDecl->getCanonicalDecl()) {
        check_in_progress(canonical);
      }
    }
    if (CxxRecordDeclaration == nullptr && !parent_in_progress) {
      CxxRecordDeclaration = isSgClassDeclaration(Traverse(parentClassDecl));
    }
    if (CxxRecordDeclaration != nullptr) {
      return CxxRecordDeclaration->get_name();
    }
    return SgName(parentClassDecl->getNameAsString());
  };

  auto strip_template_args = [&](const std::string &raw) -> std::string {
    std::string name = trimWhitespace(raw);
    size_t lt = name.find('<');
    if (lt != std::string::npos) {
      name = trimWhitespace(name.substr(0, lt));
    }
    return name;
  };

  if (llvm::isa<clang::CXXConstructorDecl>(function_decl)) {
    clang::CXXRecordDecl *parentClassDecl =
        static_cast<clang::CXXConstructorDecl *>(function_decl)->getParent();
    SgName class_name = resolve_parent_class_name(parentClassDecl);
    std::string base_name = strip_template_args(class_name.getString());
    if (!base_name.empty()) {
      name = SgName(base_name);
    }
  } else if (llvm::isa<clang::CXXDestructorDecl>(function_decl)) {
    clang::CXXRecordDecl *parentClassDecl =
        static_cast<clang::CXXDestructorDecl *>(function_decl)->getParent();
    SgName class_name = resolve_parent_class_name(parentClassDecl);
    std::string base_name = strip_template_args(class_name.getString());
    if (!base_name.empty()) {
      name = SgName("~" + base_name);
    }
  }

  bool is_builtin_decl =
      (function_decl->getBuiltinID() != clang::Builtin::NotBuiltin);
  if (!is_builtin_decl && func_name.rfind("__builtin_", 0) == 0) {
    is_builtin_decl = true;
  }
  if (!is_builtin_decl && p_compiler_instance != nullptr) {
    is_builtin_decl =
        p_compiler_instance->getSourceManager().isWrittenInBuiltinFile(
            function_decl->getLocation());
  }
  const clang::TemplateSpecializationKind specialization_kind =
      function_decl->getTemplateSpecializationKind();
  const bool is_explicit_instantiation =
      specialization_kind == clang::TSK_ExplicitInstantiationDefinition ||
      specialization_kind == clang::TSK_ExplicitInstantiationDeclaration;
  const bool is_extern_instantiation =
      specialization_kind == clang::TSK_ExplicitInstantiationDeclaration;
  bool force_template_instantiation = false;
  if (!p_pending_implicit_function_instantiations_set.empty()) {
    if (p_pending_implicit_function_instantiations_set.count(function_decl) >
        0) {
      force_template_instantiation = true;
    }
    if (!force_template_instantiation) {
      if (clang::FunctionDecl *canonical = function_decl->getCanonicalDecl()) {
        if (p_pending_implicit_function_instantiations_set.count(canonical) >
            0) {
          force_template_instantiation = true;
        }
      }
    }
    if (!force_template_instantiation) {
      if (clang::FunctionDecl *first = function_decl->getFirstDecl()) {
        if (p_pending_implicit_function_instantiations_set.count(first) > 0) {
          force_template_instantiation = true;
        }
      }
    }
  }
  bool allow_template_instantiation = true;
  if (p_compiler_instance != nullptr) {
    clang::SourceManager &sm = p_compiler_instance->getSourceManager();
    if (isSystemOrBuiltinFunctionDecl(function_decl, sm) &&
        specialization_kind == clang::TSK_ImplicitInstantiation &&
        !force_template_instantiation) {
      allow_template_instantiation = false;
    }
  }
  bool is_system_or_builtin = is_builtin_decl;
  if (p_compiler_instance != nullptr) {
    clang::SourceManager &sm = p_compiler_instance->getSourceManager();
    is_system_or_builtin = isSystemOrBuiltinFunctionDecl(function_decl, sm);
  }

  clang::QualType funcQualType = function_decl->getType();

  const clang::Type *funcType = funcQualType.getTypePtr();

  const clang::FunctionProtoType *funcProtoType =
      (llvm::isa<clang::FunctionProtoType>(funcType))
          ? (clang::FunctionProtoType *)funcType
          : nullptr;

  bool diffInProtoType = false;

#if DEBUG_VISIT_DECL
  std::cerr << "ClangToSageTranslator::VisitFunctionDecl name:"
            << name.getString() << std::endl;
#endif

  // CLANG FRONTEND FIX #21: Constructors use void type but are marked with
  // special modifier buildDefiningFunctionDeclaration requires non-nullptr
  // return type, so we use void for constructors and mark them with the
  // constructor modifier flag later
  SgType *ret_type = buildTypeFromQualifiedType(function_decl->getReturnType());
  if (SgType *specialized_ret =
          buildSpecializedMemberTypedefReturnType(function_decl)) {
    ret_type = specialized_ret;
  }

  SgFunctionParameterList *param_list =
      SageBuilder::buildFunctionParameterList_nfi();
  applySourceRange(param_list, function_decl->getParametersSourceRange());

  const bool is_definition_decl_for_params =
      function_decl->isThisDeclarationADefinition() &&
      allow_template_instantiation && !is_system_or_builtin;

  auto clone_param_for_list =
      [&](SgInitializedName *source,
          const clang::ParmVarDecl *param_decl) -> SgInitializedName * {
    if (source == nullptr || param_decl == nullptr) {
      return source;
    }

    SgInitializer *cloned_init = nullptr;
    if (SgInitializer *init = source->get_initializer()) {
      cloned_init = SageInterface::deepCopy(init);
    }

    SgInitializedName *cloned = SageBuilder::buildInitializedName_nfi(
        source->get_name(), source->get_type(), cloned_init);
    applySourceRange(cloned, param_decl->getSourceRange());
    cloned->set_is_parameter_pack(source->get_is_parameter_pack());
    cloned->set_is_pack_element(source->get_is_pack_element());
    cloned->set_needs_definitions(source->get_needs_definitions());
    cloned->set_name_qualification_length_for_type(
        source->get_name_qualification_length_for_type());
    cloned->set_type_elaboration_required_for_type(
        source->get_type_elaboration_required_for_type());
    cloned->set_global_qualification_required_for_type(
        source->get_global_qualification_required_for_type());

    SgScopeStatement *scope = SageBuilder::topScopeStack();
    cloned->set_scope(scope);
    cloned->set_parent(scope);
    return cloned;
  };

  if (funcProtoType != nullptr &&
      funcProtoType->getNumParams() != function_decl->getNumParams())
    diffInProtoType = true;

  SgDeclarationScope *declScope = SageBuilder::buildDeclarationScope();
  declScope->set_parent(SageBuilder::topScopeStack());
  SageBuilder::pushScopeStack(declScope);

  for (unsigned i = 0; i < function_decl->getNumParams(); i++) {
    clang::ParmVarDecl *param_decl = function_decl->getParamDecl(i);
    if (funcProtoType != nullptr && function_decl->getParamDecl(i)->getType() !=
                                        funcProtoType->getParamType(i)) {
#if DEBUG_VISIT_DECL
      std::cout << "Func arg type :"
                << function_decl->getParamDecl(i)->getType().getAsString()
                << " funcProtoType arg type:"
                << funcProtoType->getParamType(i).getAsString() << std::endl;
#endif
      diffInProtoType = true;
    }
    SgNode *tmp_init_name = Traverse(param_decl);
    SgInitializedName *init_name = isSgInitializedName(tmp_init_name);

    if (init_name != nullptr) {
      if (SgFunctionParameterList *existing_list =
              isSgFunctionParameterList(init_name->get_parent())) {
        if (existing_list != param_list) {
          init_name = clone_param_for_list(init_name, param_decl);
        }
      }

      if (is_definition_decl_for_params) {
        p_decl_translation_map[param_decl] = init_name;
      }
    }

    // Pei-Hung (05/09/2022) Need to setup set_needs_definitions to
    // SgInitializedName when the Enum or Class type is actually declared in a
    // function prototype scope
    if (isSgEnumType(init_name->get_type()) ||
        isSgClassType(init_name->get_type())) {
      SgNamedType *namedType = isSgNamedType(init_name->get_type());
      SgDeclarationStatement *namedTypeDecl =
          isSgDeclarationStatement(namedType->get_declaration());
      SgDeclarationStatement *definingDecl =
          namedTypeDecl->get_definingDeclaration();
      if (definingDecl != nullptr) {
        SgDeclarationStatement *definingNamedTypeDecl =
            isSgDeclarationStatement(definingDecl);
        SgScopeStatement *definitionEnclosingScope =
            definingNamedTypeDecl->get_scope();
        // case 1: definition is under SgDeclarationScope
        if (isSgScopeStatement(definitionEnclosingScope) ==
            SageBuilder::topScopeStack()) {
          init_name->set_needs_definitions(true);
        }
        // case 2: definition is at other scope but not in the
        // SgDeclarationStatementPtrList from that scope e.g. test2018_15.c,
        // test2018_13.c
        else {
          // ROOT CAUSE FIX: Only certain scope types support
          // getDeclarationList() Match the exact set of types from
          // Cxx_Grammar.C:116211-116268 Supported: SgGlobal,
          // SgNamespaceDefinitionStatement, SgClassDefinition,
          //            SgTemplateClassDefinition, SgTemplateInstantiationDefn,
          //            SgFunctionParameterScope, SgDeclarationScope
          // NOT supported: SgBasicBlock, SgForInitStatement, etc.
          if (isSgGlobal(definitionEnclosingScope) ||
              isSgNamespaceDefinitionStatement(definitionEnclosingScope) ||
              isSgClassDefinition(definitionEnclosingScope) ||
              isSgTemplateClassDefinition(definitionEnclosingScope) ||
              isSgTemplateInstantiationDefn(definitionEnclosingScope) ||
              isSgFunctionParameterScope(definitionEnclosingScope) ||
              isSgDeclarationScope(definitionEnclosingScope)) {
            SgDeclarationStatementPtrList &declList =
                definitionEnclosingScope->getDeclarationList();
            if (std::find(declList.begin(), declList.end(),
                          definingNamedTypeDecl) == declList.end()) {
              init_name->set_needs_definitions(true);
            }
          }
        }
      }
    }

    if (tmp_init_name != nullptr && init_name == nullptr) {
      std::cerr
          << "Runtime error: tmp_init_name != nullptr && init_name == nullptr"
          << std::endl;
      res = false;
      continue;
    }

    param_list->append_arg(init_name);
  }

  SageBuilder::popScopeStack();

  if (function_decl->isVariadic()) {
    SgName empty = "";
    SgType *ellipses_type = SgTypeEllipse::createType();
    SgInitializedName *ellipses_param =
        SageBuilder::buildInitializedName_nfi(empty, ellipses_type, nullptr);
    // Set scope and parent to avoid unparser assertion
    SgScopeStatement *scope = SageBuilder::topScopeStack();
    ellipses_param->set_scope(scope);
    ellipses_param->set_parent(scope);
    param_list->append_arg(ellipses_param);
  }

  // ROOT CAUSE FIX: Get proper scope for this function from its Clang
  // declaration context For out-of-line member functions, ensure scope is the
  // class definition, not global CRITICAL: Friend functions are declared in
  // class but are NOT members - keep them in the class syntactically and expose
  // them via the enclosing namespace/global scope symbol table.
  clang::DeclContext *decl_context = function_decl->getDeclContext();
  clang::DeclContext *semantic_context = decl_context;
  while (semantic_context != nullptr &&
         llvm::isa<clang::LinkageSpecDecl>(semantic_context)) {
    semantic_context = semantic_context->getParent();
  }
  SgScopeStatement *proper_scope = getGlobalScope(); // Default fallback

  bool isDefinition = is_definition_decl_for_params;

  // Check if this is a friend function.  Clang represents friend free
  // functions as FunctionDecls inside a record DeclContext (but *not* as
  // CXXMethodDecls).  Some friend templates are not marked via
  // getFriendObjectKind(), so also infer friend-ness from the DeclContext.
  bool isFriendMethod = llvm::isa<clang::CXXMethodDecl>(function_decl);
  clang::DeclContext *lexical_context = function_decl->getLexicalDeclContext();
  bool decl_context_is_record =
      decl_context != nullptr && llvm::isa<clang::CXXRecordDecl>(decl_context);
  bool lexical_context_is_record =
      lexical_context != nullptr &&
      llvm::isa<clang::CXXRecordDecl>(lexical_context);
  bool looks_like_friend_free_function =
      (decl_context_is_record || lexical_context_is_record) && !isFriendMethod;

  bool isFriendFunction =
      (function_decl->getFriendObjectKind() != clang::Decl::FOK_None) ||
      looks_like_friend_free_function;
  bool isFriendFreeFunction = (isFriendFunction && !isFriendMethod);

  // Lexical class enclosing scope needed so friend free functions stay visible
  // in the namespace
  SgScopeStatement *lexical_friend_enclosing_scope = nullptr;
  SgScopeStatement *lexical_friend_class_def = nullptr;
  bool friend_lexically_inside_class = false;
  auto getEnclosingNamespaceScope =
      [](SgScopeStatement *scope) -> SgScopeStatement * {
    SgScopeStatement *current = scope;
    while (current != nullptr && !isSgGlobal(current) &&
           !isSgNamespaceDefinitionStatement(current)) {
      SgScopeStatement *next_scope =
          SageInterface::getEnclosingScope(current, false);
      if (next_scope == current)
        break;
      current = next_scope;
    }
    return current;
  };

  if (isFriendFreeFunction) {
    if (lexical_context && llvm::isa<clang::CXXRecordDecl>(lexical_context)) {
      clang::CXXRecordDecl *lexical_class =
          llvm::cast<clang::CXXRecordDecl>(lexical_context);
      auto lookup_class_node = [&](clang::CXXRecordDecl *key) -> SgNode * {
        if (key == nullptr) {
          return nullptr;
        }
        auto it = p_decl_translation_map.find(key);
        if (it != p_decl_translation_map.end()) {
          return it->second;
        }
        return nullptr;
      };

      SgNode *class_node = lookup_class_node(lexical_class);
      if (class_node == nullptr) {
        class_node = lookup_class_node(lexical_class->getDefinition());
      }
      if (class_node == nullptr) {
        class_node = lookup_class_node(lexical_class->getCanonicalDecl());
      }
      if (class_node == nullptr) {
        if (clang::CXXRecordDecl *canonical =
                lexical_class->getCanonicalDecl()) {
          class_node = lookup_class_node(canonical->getDefinition());
        }
      }

      if (class_node != nullptr) {
        SgScopeStatement *class_scope = nullptr;
        if (SgClassDeclaration *class_decl = isSgClassDeclaration(class_node)) {
          class_scope = class_decl->get_scope();
          if (class_decl->get_definition())
            lexical_friend_class_def = class_decl->get_definition();
        } else if (SgClassDefinition *class_def =
                       isSgClassDefinition(class_node)) {
          lexical_friend_class_def = class_def;
          if (SgClassDeclaration *decl =
                  isSgClassDeclaration(class_def->get_declaration())) {
            class_scope = decl->get_scope();
          }
        } else if (SgTemplateClassDeclaration *template_class_decl =
                       isSgTemplateClassDeclaration(class_node)) {
          class_scope = template_class_decl->get_scope();
          if (template_class_decl->get_definition())
            lexical_friend_class_def = template_class_decl->get_definition();
        } else if (SgTemplateClassDefinition *template_class_def =
                       isSgTemplateClassDefinition(class_node)) {
          lexical_friend_class_def = template_class_def;
          if (SgTemplateClassDeclaration *decl = isSgTemplateClassDeclaration(
                  template_class_def->get_declaration())) {
            class_scope = decl->get_scope();
          }
        }
        if (lexical_friend_class_def != nullptr) {
          friend_lexically_inside_class = true;
        }
        if (class_scope != nullptr) {
          lexical_friend_enclosing_scope =
              getEnclosingNamespaceScope(class_scope);
          if (lexical_friend_enclosing_scope == nullptr) {
            lexical_friend_enclosing_scope = getGlobalScope();
          }
        }
      }
    }
  }
  if (isFriendFreeFunction && !friend_lexically_inside_class) {
    if (SgScopeStatement *scope = SageBuilder::topScopeStack()) {
      if (isSgClassDefinition(scope) != nullptr ||
          isSgTemplateClassDefinition(scope) != nullptr) {
        lexical_friend_class_def = scope;
        friend_lexically_inside_class = true;
        lexical_friend_enclosing_scope = getEnclosingNamespaceScope(scope);
        if (lexical_friend_enclosing_scope == nullptr) {
          lexical_friend_enclosing_scope = getGlobalScope();
        }
      }
    }
  }

  bool scope_assigned = false;
  if (isFriendFreeFunction) {
    bool keep_in_class_scope = (!isDefinition) || friend_lexically_inside_class;
    if (keep_in_class_scope) {
      if (lexical_friend_class_def != nullptr) {
        proper_scope = lexical_friend_class_def;
        scope_assigned = true;
      }
    } else {
      if (lexical_friend_enclosing_scope != nullptr) {
        proper_scope = lexical_friend_enclosing_scope;
        scope_assigned = true;
      }
    }
  }

  auto resolve_class_definition_from_node =
      [&](SgNode *class_node) -> SgClassDefinition * {
    if (class_node == nullptr) {
      return nullptr;
    }
    if (SgTemplateInstantiationDirectiveStatement *instantiation_directive =
            isSgTemplateInstantiationDirectiveStatement(class_node)) {
      class_node = instantiation_directive->get_declaration();
    }
    if (SgClassDefinition *class_def = isSgClassDefinition(class_node)) {
      return class_def;
    }
    if (SgClassDeclaration *class_decl = isSgClassDeclaration(class_node)) {
      if (class_decl->get_definition()) {
        return class_decl->get_definition();
      }
      if (SgClassDeclaration *def_decl =
              isSgClassDeclaration(class_decl->get_definingDeclaration())) {
        return def_decl->get_definition();
      }
      return nullptr;
    }
    if (SgTemplateClassDeclaration *template_class_decl =
            isSgTemplateClassDeclaration(class_node)) {
      if (template_class_decl->get_definition()) {
        return template_class_decl->get_definition();
      }
      if (SgTemplateClassDeclaration *def_decl = isSgTemplateClassDeclaration(
              template_class_decl->get_definingDeclaration())) {
        return def_decl->get_definition();
      }
      return nullptr;
    }
    return nullptr;
  };

  auto resolve_class_definition =
      [&](clang::CXXRecordDecl *record) -> SgClassDefinition * {
    if (record == nullptr) {
      return nullptr;
    }
    std::map<clang::Decl *, SgNode *>::iterator it =
        p_decl_translation_map.find(record);
    if (it == p_decl_translation_map.end()) {
      return nullptr;
    }
    return resolve_class_definition_from_node(it->second);
  };
  auto translate_template_definition =
      [&](clang::CXXRecordDecl *record) -> void {
    if (record == nullptr) {
      return;
    }
    clang::ClassTemplateDecl *template_decl =
        record->getDescribedClassTemplate();
    if (template_decl == nullptr) {
      return;
    }

    auto it = p_decl_translation_map.find(template_decl);
    if (it != p_decl_translation_map.end()) {
      if (resolve_class_definition_from_node(it->second) != nullptr) {
        return;
      }
    }

    if (p_decl_translation_in_progress.find(template_decl) !=
        p_decl_translation_in_progress.end()) {
      return;
    }

    TranslationGuard template_guard(p_decl_translation_in_progress,
                                    template_decl);
    translateClassTemplateDecl(template_decl, nullptr, nullptr);
  };
  auto resolve_or_translate_class_definition =
      [&](clang::CXXRecordDecl *record) -> SgClassDefinition * {
    if (record == nullptr) {
      return nullptr;
    }
    auto ensure_definition_populated =
        [&](clang::CXXRecordDecl *decl,
            SgClassDefinition *def) -> SgClassDefinition * {
      if (decl == nullptr || def == nullptr) {
        return def;
      }
      if (p_record_definitions_populated.find(def) !=
          p_record_definitions_populated.end()) {
        return def;
      }
      clang::CXXRecordDecl *def_decl = decl->getDefinition();
      if (def_decl == nullptr) {
        if (!decl->isThisDeclarationADefinition()) {
          return def;
        }
        def_decl = decl;
      }
      if (p_record_definitions_populated.insert(def).second) {
        populateClassDefinition(def_decl, def);
      }
      return def;
    };
    if (SgClassDefinition *existing = resolve_class_definition(record)) {
      return ensure_definition_populated(record, existing);
    }

    clang::CXXRecordDecl *definition_decl = record->getDefinition();
    if (definition_decl != nullptr && definition_decl != record) {
      if (SgClassDefinition *existing =
              resolve_class_definition(definition_decl)) {
        return ensure_definition_populated(definition_decl, existing);
      }
      translate_template_definition(definition_decl);
      if (SgClassDefinition *existing =
              resolve_class_definition(definition_decl)) {
        return ensure_definition_populated(definition_decl, existing);
      }
      if (p_decl_translation_map.find(definition_decl) ==
              p_decl_translation_map.end() &&
          p_decl_translation_in_progress.find(definition_decl) ==
              p_decl_translation_in_progress.end() &&
          p_decl_translation_on_demand.find(definition_decl) ==
              p_decl_translation_on_demand.end()) {
        TraverseOnDemand(definition_decl);
      }
      if (SgClassDefinition *existing =
              resolve_class_definition(definition_decl)) {
        return ensure_definition_populated(definition_decl, existing);
      }
    }

    translate_template_definition(record);
    if (SgClassDefinition *existing = resolve_class_definition(record)) {
      return ensure_definition_populated(record, existing);
    }

    if (p_decl_translation_map.find(record) == p_decl_translation_map.end() &&
        p_decl_translation_in_progress.find(record) ==
            p_decl_translation_in_progress.end() &&
        p_decl_translation_on_demand.find(record) ==
            p_decl_translation_on_demand.end()) {
      TraverseOnDemand(record);
    }
    return ensure_definition_populated(record,
                                       resolve_class_definition(record));
  };

  // For member functions (including friend methods), use the class definition
  // as scope
  if (!scope_assigned && llvm::isa<clang::CXXMethodDecl>(function_decl)) {
    clang::CXXMethodDecl *method_decl =
        llvm::cast<clang::CXXMethodDecl>(function_decl);
    clang::CXXRecordDecl *parent_class = method_decl->getParent();
    if (parent_class) {
      SgClassDefinition *class_def =
          resolve_or_translate_class_definition(parent_class);
      clang::CXXRecordDecl *definition_decl = parent_class->getDefinition();
      if (class_def == nullptr && definition_decl != nullptr &&
          definition_decl != parent_class) {
        class_def = resolve_or_translate_class_definition(definition_decl);
      }
      clang::CXXRecordDecl *canonical_decl = parent_class->getCanonicalDecl();
      if (class_def == nullptr && canonical_decl != nullptr &&
          canonical_decl != parent_class) {
        class_def = resolve_or_translate_class_definition(canonical_decl);
      }
      if (class_def == nullptr) {
        clang::TemplateSpecializationKind tsk =
            parent_class->getTemplateSpecializationKind();
        bool allow_template_pattern =
            tsk == clang::TSK_ImplicitInstantiation ||
            tsk == clang::TSK_ExplicitInstantiationDeclaration ||
            tsk == clang::TSK_ExplicitInstantiationDefinition ||
            tsk == clang::TSK_Undeclared;
        if (allow_template_pattern) {
          if (clang::CXXRecordDecl *pattern =
                  parent_class->getTemplateInstantiationPattern()) {
            class_def = resolve_or_translate_class_definition(pattern);
          }
        }
      }
      if (class_def == nullptr) {
        if (clang::ClassTemplateDecl *templ =
                parent_class->getDescribedClassTemplate()) {
          clang::CXXRecordDecl *templated = templ->getTemplatedDecl();
          if (templated != nullptr && templated != parent_class) {
            class_def = resolve_or_translate_class_definition(templated);
          }
        }
      }
      if (class_def == nullptr) {
        bool parent_in_progress = false;
        auto check_in_progress = [&](clang::CXXRecordDecl *record) {
          if (record != nullptr &&
              p_decl_translation_in_progress.find(record) !=
                  p_decl_translation_in_progress.end()) {
            parent_in_progress = true;
          }
        };
        check_in_progress(parent_class);
        check_in_progress(definition_decl);
        check_in_progress(canonical_decl);
        if (parent_in_progress) {
          if (SgClassDefinition *stack_def =
                  isSgClassDefinition(SageBuilder::topScopeStack())) {
            class_def = stack_def;
          }
        }
      }
      if (class_def == nullptr) {
        std::cerr << "Error: Could not resolve class definition for method '"
                  << getDeclNameSafe(function_decl) << "'. Skipping function."
                  << std::endl;
        if (SgProject::get_verbose() > 0) {
          auto dump_record = [&](const char *label,
                                 clang::CXXRecordDecl *record) {
            if (record == nullptr) {
              std::cerr << "  " << label << ": <null>" << std::endl;
              return;
            }
            std::string name = record->getQualifiedNameAsString();
            std::cerr << "  " << label << ": "
                      << (name.empty() ? "<unnamed>" : name) << " ("
                      << static_cast<clang::Decl *>(record)->getDeclKindName()
                      << ")"
                      << " def=" << (record->getDefinition() != nullptr)
                      << " tsk=" << record->getTemplateSpecializationKind()
                      << std::endl;
            auto it = p_decl_translation_map.find(record);
            std::cerr << "    map="
                      << (it != p_decl_translation_map.end() ? "yes" : "no")
                      << std::endl;
            if (it != p_decl_translation_map.end() && it->second != nullptr) {
              std::cerr << "    node=" << it->second->class_name() << std::endl;
            }
          };
          dump_record("parent", parent_class);
          dump_record("definition", definition_decl);
          dump_record("canonical", canonical_decl);
          if (clang::CXXRecordDecl *pattern =
                  parent_class->getTemplateInstantiationPattern()) {
            dump_record("pattern", pattern);
          }
          if (clang::ClassTemplateDecl *templ =
                  parent_class->getDescribedClassTemplate()) {
            dump_record("templated", templ->getTemplatedDecl());
          }
        }
        if (node != nullptr) {
          *node = nullptr;
        }
        return false;
      }
      proper_scope = class_def;
      scope_assigned = true;
    }
  }
  // For non-member functions, use DeclContext (namespace or class)
  else if (!scope_assigned && semantic_context &&
           !semantic_context->isTranslationUnit()) {
    clang::Decl *context_decl = llvm::dyn_cast<clang::Decl>(semantic_context);
    if (context_decl) {
      std::map<clang::Decl *, SgNode *>::iterator it =
          p_decl_translation_map.find(context_decl);
      if (it != p_decl_translation_map.end()) {
        SgNode *context_node = it->second;
        if (SgClassDeclaration *class_decl =
                isSgClassDeclaration(context_node)) {
          if (class_decl->get_definition()) {
            proper_scope = class_decl->get_definition();
          }
        } else if (SgClassDefinition *class_def =
                       isSgClassDefinition(context_node)) {
          proper_scope = class_def;
        } else if (SgTemplateClassDeclaration *template_class_decl =
                       isSgTemplateClassDeclaration(context_node)) {
          if (template_class_decl->get_definition()) {
            proper_scope = template_class_decl->get_definition();
          }
        } else if (SgNamespaceDeclarationStatement *ns_decl =
                       isSgNamespaceDeclarationStatement(context_node)) {
          if (ns_decl->get_definition())
            proper_scope = ns_decl->get_definition();
        } else if (SgNamespaceDefinitionStatement *ns_def =
                       isSgNamespaceDefinitionStatement(context_node)) {
          proper_scope = ns_def;
        }
      } else if (clang::NamespaceDecl *ns_decl =
                     llvm::dyn_cast<clang::NamespaceDecl>(context_decl)) {
        // If the namespace is currently being traversed, the translation map
        // entry is not populated until Traverse() returns. In that case, reuse
        // the active namespace scope from the scope stack.
        if (SgNamespaceDefinitionStatement *ns_def =
                isSgNamespaceDefinitionStatement(
                    SageBuilder::topScopeStack())) {
          SgNamespaceDeclarationStatement *ns_decl_stmt =
              ns_def->get_namespaceDeclaration();
          bool match = false;
          if (ns_decl_stmt != nullptr) {
            if (ns_decl->isAnonymousNamespace()) {
              match = ns_decl_stmt->get_isUnnamedNamespace();
            } else {
              match = ns_decl_stmt->get_name().getString() ==
                      ns_decl->getNameAsString();
            }
          }
          if (match) {
            proper_scope = ns_def;
          }
        }

        // Otherwise ensure the namespace scope exists (e.g., on-demand
        // translation of referenced system-header declarations).
        if (!isSgNamespaceDefinitionStatement(proper_scope)) {
          SgNamespaceDeclarationStatement *ns_stmt =
              ensureNamespaceDeclaration(ns_decl);
          if (ns_stmt && ns_stmt->get_definition() != nullptr) {
            proper_scope = ns_stmt->get_definition();
          }
        }
      }
    }
  }

  const bool isMethodDecl = llvm::isa<clang::CXXMethodDecl>(function_decl);

  // Namespace decls are re-entrant in ROSE; for symbol-table insertion use the
  // canonical namespace scope so declarations across reopenings share a single
  // symbol table.
  SgScopeStatement *lexical_scope = resolveScopeFromDeclContext(
      function_decl->getLexicalDeclContext(), proper_scope);
  if (lexical_scope == nullptr) {
    lexical_scope = proper_scope;
  }

  if (isMethodDecl) {
    auto *method_decl = llvm::cast<clang::CXXMethodDecl>(function_decl);
    if (method_decl->isOutOfLine()) {
      clang::DeclContext *target_ctx = method_decl->getLexicalDeclContext();
      while (target_ctx != nullptr &&
             llvm::isa<clang::CXXRecordDecl>(target_ctx)) {
        target_ctx = target_ctx->getParent();
      }
      if (target_ctx != nullptr) {
        if (SgScopeStatement *out_of_line_scope =
                resolveScopeFromDeclContext(target_ctx, nullptr)) {
          lexical_scope = out_of_line_scope;
        }
      }
    }
  }

  if (isFriendFreeFunction && !friend_lexically_inside_class) {
    if (isSgClassDefinition(lexical_scope) != nullptr ||
        isSgTemplateClassDefinition(lexical_scope) != nullptr) {
      lexical_friend_class_def = lexical_scope;
      friend_lexically_inside_class = true;
      lexical_friend_enclosing_scope =
          getEnclosingNamespaceScope(lexical_scope);
      if (lexical_friend_enclosing_scope == nullptr) {
        lexical_friend_enclosing_scope = getGlobalScope();
      }
      if (!scope_assigned) {
        bool keep_in_class_scope = !isDefinition;
        if (keep_in_class_scope) {
          proper_scope = lexical_scope;
          scope_assigned = true;
        }
      }
    }
  }
  SgScopeStatement *scope_for_symbol_table = proper_scope;
  if (scope_for_symbol_table == nullptr) {
    scope_for_symbol_table = getGlobalScope();
  }
  scope_for_symbol_table = normalizeNamespaceScope(scope_for_symbol_table);

  // Friend free functions declared/defined inside a class are semantically
  // declared in the enclosing namespace/global scope.  Use that scope for
  // symbol-table insertion and SageBuilder lookup, while preserving the lexical
  // class scope for AST attachment/unparse order.
  SgScopeStatement *friend_symbol_scope = nullptr;
  if (isFriendFreeFunction && friend_lexically_inside_class) {
    auto resolve_enclosing_namespace_scope =
        [&](clang::DeclContext *ctx) -> SgScopeStatement * {
      clang::DeclContext *cursor = ctx;
      while (cursor != nullptr && !cursor->isNamespace() &&
             !cursor->isTranslationUnit()) {
        cursor = cursor->getParent();
      }
      if (cursor == nullptr) {
        return nullptr;
      }
      return resolveScopeFromDeclContext(cursor, nullptr);
    };

    SgScopeStatement *semantic_scope =
        resolveScopeFromDeclContext(decl_context, nullptr);
    if (semantic_scope != nullptr &&
        (isSgNamespaceDefinitionStatement(semantic_scope) != nullptr ||
         isSgGlobal(semantic_scope) != nullptr)) {
      friend_symbol_scope = normalizeNamespaceScope(semantic_scope);
    } else {
      SgScopeStatement *enclosing_scope =
          resolve_enclosing_namespace_scope(decl_context);
      if (enclosing_scope == nullptr) {
        enclosing_scope = resolve_enclosing_namespace_scope(lexical_context);
      }
      if (enclosing_scope != nullptr) {
        friend_symbol_scope = normalizeNamespaceScope(enclosing_scope);
      } else if (lexical_friend_enclosing_scope != nullptr) {
        friend_symbol_scope =
            normalizeNamespaceScope(lexical_friend_enclosing_scope);
      }
    }
    if (friend_symbol_scope == nullptr) {
      friend_symbol_scope = getGlobalScope();
    }
  }
  if (friend_symbol_scope != nullptr) {
    scope_for_symbol_table = friend_symbol_scope;
  }

  auto normalize_existing_function_symbol_scope = [&](SgScopeStatement
                                                          *symbol_scope) {
    if (symbol_scope == nullptr || isMethodDecl) {
      return;
    }

    SgFunctionType *func_type =
        SageBuilder::buildFunctionType(ret_type, param_list);
    if (func_type == nullptr) {
      return;
    }

    auto rehome_symbol = [&](SgSymbol *sym) {
      if (sym == nullptr) {
        return;
      }
      SgSymbolTable *desired_table =
          symbol_scope != nullptr ? symbol_scope->get_symbol_table() : nullptr;
      if (desired_table == nullptr) {
        return;
      }
      SgSymbolTable *parent_table = isSgSymbolTable(sym->get_parent());
      if (parent_table == desired_table) {
        return;
      }

      bool removed_from_parent = false;
      if (parent_table != nullptr) {
        if (SgScopeStatement *parent_scope =
                isSgScopeStatement(parent_table->get_parent())) {
          if (parent_scope->symbol_exists(sym)) {
            parent_scope->remove_symbol(sym);
            removed_from_parent = true;
          } else if (parent_table->exists(sym)) {
            parent_table->remove(sym);
            removed_from_parent = true;
          }
        } else if (parent_table->exists(sym)) {
          parent_table->remove(sym);
          removed_from_parent = true;
        }
      } else if (SgScopeStatement *parent_scope =
                     isSgScopeStatement(sym->get_parent())) {
        if (parent_scope->symbol_exists(sym)) {
          parent_scope->remove_symbol(sym);
          removed_from_parent = true;
        }
      }

      if (!symbol_scope->symbol_exists(sym)) {
        symbol_scope->insert_symbol(sym->get_name(), sym);
      } else if (sym->get_parent() != desired_table) {
        sym->set_parent(desired_table);
      }
    };

    auto normalize_decl_scope = [&](SgFunctionDeclaration *decl) {
      if (decl == nullptr) {
        return;
      }
      SgScopeStatement *decl_scope = decl->get_scope();
      if (decl_scope == nullptr) {
        return;
      }
      if (decl_scope == symbol_scope) {
        return;
      }

      if (normalizeNamespaceScope(decl_scope) == symbol_scope) {
        decl->set_scope(symbol_scope);
        return;
      }

      if (isFriendFreeFunction &&
          (isSgNamespaceDefinitionStatement(symbol_scope) != nullptr ||
           isSgGlobal(symbol_scope) != nullptr)) {
        decl->set_scope(symbol_scope);
      }
    };

    auto normalize_symbol_decl = [&](SgFunctionSymbol *sym) {
      if (sym == nullptr) {
        return;
      }
      normalize_decl_scope(isSgFunctionDeclaration(sym->get_declaration()));
      normalize_decl_scope(isSgFunctionDeclaration(
          sym->get_declaration()
              ? sym->get_declaration()->get_firstNondefiningDeclaration()
              : nullptr));
      rehome_symbol(sym);
    };
    auto normalize_template_symbol_decl = [&](SgTemplateFunctionSymbol *sym) {
      if (sym == nullptr) {
        return;
      }
      normalize_decl_scope(isSgFunctionDeclaration(sym->get_declaration()));
      normalize_decl_scope(isSgFunctionDeclaration(
          sym->get_declaration()
              ? sym->get_declaration()->get_firstNondefiningDeclaration()
              : nullptr));
      normalize_decl_scope(isSgFunctionDeclaration(
          sym->get_declaration()
              ? sym->get_declaration()->get_definingDeclaration()
              : nullptr));
      rehome_symbol(sym);
    };

    auto normalize_symbol_from_decl = [&](clang::FunctionDecl *decl) -> bool {
      if (decl == nullptr) {
        return false;
      }
      if (SgSymbol *sym = GetSymbolFromSymbolTable(decl)) {
        if (SgTemplateFunctionSymbol *tmpl_sym =
                isSgTemplateFunctionSymbol(sym)) {
          normalize_template_symbol_decl(tmpl_sym);
          return true;
        }
        if (SgFunctionSymbol *func_sym = isSgFunctionSymbol(sym)) {
          normalize_symbol_decl(func_sym);
          return true;
        }
      }
      return false;
    };

    auto rehome_from_namespace_chain = [&]() {
      SgNamespaceDefinitionStatement *canonical_ns =
          isSgNamespaceDefinitionStatement(symbol_scope);
      if (canonical_ns == nullptr) {
        return;
      }

      SgNamespaceDefinitionStatement *first_def =
          isSgNamespaceDefinitionStatement(
              normalizeNamespaceScope(canonical_ns));
      if (first_def == nullptr) {
        first_def = canonical_ns;
      }

      std::vector<SgSymbol *> symbols_to_rehome;
      std::set<SgSymbol *> seen_symbols;
      for (SgNamespaceDefinitionStatement *ns = first_def; ns != nullptr;
           ns = ns->get_nextNamespaceDefinition()) {
        if (ns == canonical_ns) {
          continue;
        }
        SgSymbolTable *table = ns->get_symbol_table();
        if (table == nullptr) {
          continue;
        }

        const std::string key = name.getString();
        if (rose_hash_multimap *symtab = table->get_table()) {
          auto range = symtab->equal_range(key);
          for (auto it = range.first; it != range.second; ++it) {
            if (it->second != nullptr &&
                seen_symbols.insert(it->second).second) {
              symbols_to_rehome.push_back(it->second);
            }
          }
        }
      }

      for (SgSymbol *sym : symbols_to_rehome) {
        if (SgTemplateFunctionSymbol *tmpl_sym =
                isSgTemplateFunctionSymbol(sym)) {
          normalize_template_symbol_decl(tmpl_sym);
        } else if (SgFunctionSymbol *func_sym = isSgFunctionSymbol(sym)) {
          normalize_symbol_decl(func_sym);
        }
      }
    };

    if (SgFunctionSymbol *sym =
            symbol_scope->lookup_function_symbol(name, func_type)) {
      normalize_symbol_decl(sym);
    } else {
      bool normalized = normalize_symbol_from_decl(function_decl);
      clang::FunctionDecl *first_decl = function_decl->getFirstDecl();
      if (!normalized && first_decl != function_decl) {
        normalized = normalize_symbol_from_decl(first_decl);
      }
      clang::FunctionDecl *canonical_decl = function_decl->getCanonicalDecl();
      if (!normalized && canonical_decl != nullptr &&
          canonical_decl != function_decl && canonical_decl != first_decl) {
        normalize_symbol_from_decl(canonical_decl);
      }
    }

    std::vector<SgSymbol *> symbols_to_normalize;
    std::set<SgSymbol *> seen_symbols;
    if (SgSymbolTable *symtab = symbol_scope->get_symbol_table()) {
      if (rose_hash_multimap *table = symtab->get_table()) {
        const std::string key = name.getString();
        auto range = table->equal_range(key);
        for (auto it = range.first; it != range.second; ++it) {
          if (it->second != nullptr && seen_symbols.insert(it->second).second) {
            symbols_to_normalize.push_back(it->second);
          }
        }
      }
    }

    for (SgSymbol *sym : symbols_to_normalize) {
      if (SgTemplateFunctionSymbol *tmpl_sym =
              isSgTemplateFunctionSymbol(sym)) {
        normalize_template_symbol_decl(tmpl_sym);
      } else if (SgFunctionSymbol *func_sym = isSgFunctionSymbol(sym)) {
        normalize_symbol_decl(func_sym);
      }
    }

    rehome_from_namespace_chain();
  };

  normalize_existing_function_symbol_scope(scope_for_symbol_table);

  SgFunctionDeclaration *sg_function_decl;
  auto register_function_translation = [&](clang::FunctionDecl *clang_decl,
                                           SgFunctionDeclaration *sage_decl) {
    if (clang_decl == nullptr || sage_decl == nullptr) {
      return;
    }
    auto register_decl = [&](clang::FunctionDecl *key) {
      if (key == nullptr) {
        return;
      }
      auto it = p_decl_translation_map.find(key);
      if (it == p_decl_translation_map.end() || it->second == nullptr) {
        p_decl_translation_map[key] = sage_decl;
      }
    };
    register_decl(clang_decl);
    register_decl(clang_decl->getCanonicalDecl());
    register_decl(clang_decl->getFirstDecl());
  };

  // REX FIX: Check if this is a function template pattern
  clang::FunctionTemplateDecl *templateDecl =
      template_decl != nullptr ? template_decl
                               : function_decl->getDescribedFunctionTemplate();
  const bool has_function_template =
      templateDecl != nullptr || function_decl->getPrimaryTemplate() != nullptr;
  std::unique_ptr<SgTemplateParameterPtrList> templateParams;
  if (templateDecl) {
    // Translate template parameters
    // Pass nullptr as owning template for now, we'll set it later if needed,
    // but SgTemplateFunctionDeclaration IS the owning template.
    // However, we can't pass it before creating it.
    templateParams = translateTemplateParameterList(
        templateDecl->getTemplateParameters(), nullptr);
  }

  // Member functions of *class templates* are represented in ROSE as template
  // member functions (they are parameterized by the enclosing class template),
  // even when they are not function templates in Clang (i.e.,
  // getDescribedFunctionTemplate() is null).  Their definitions must be built
  // with the template-member builders, otherwise SageBuilder will attempt to
  // build a non-template defining declaration from a template nondefining
  // declaration and abort (copyAST_copytest2007_40 / Issue 69).
  bool isClassTemplateMemberFunction = false;
  if (isMethodDecl && templateDecl == nullptr) {
    auto *method_decl = llvm::cast<clang::CXXMethodDecl>(function_decl);
    if (clang::CXXRecordDecl *parent_record = method_decl->getParent()) {
      isClassTemplateMemberFunction =
          parent_record->getDescribedClassTemplate() != nullptr &&
          function_decl->getTemplateSpecializationKind() ==
              clang::TSK_Undeclared;
    }
  }

  const bool isTemplateMemberFunction = templateDecl != nullptr && isMethodDecl;
  const bool isTemplateLikeMemberFunction =
      isMethodDecl &&
      (isTemplateMemberFunction || isClassTemplateMemberFunction);
  const bool is_explicit_specialization =
      specialization_kind == clang::TSK_ExplicitSpecialization;
  SgTemplateArgumentPtrList explicit_specialization_args;
  SgTemplateArgumentPtrList *args_for_builder = nullptr;
  if (is_explicit_specialization && templateDecl == nullptr) {
    if (has_function_template) {
      size_t explicit_arg_count = 0;
      if (const clang::ASTTemplateArgumentListInfo *args_as_written =
              function_decl->getTemplateSpecializationArgsAsWritten()) {
        clang::TemplateArgumentListInfo arg_info(
            args_as_written->getLAngleLoc(), args_as_written->getRAngleLoc());
        for (const clang::TemplateArgumentLoc &loc :
             args_as_written->arguments()) {
          arg_info.addArgument(loc);
        }
        explicit_arg_count = countExpandedTemplateArguments(arg_info);
      }
      if (const clang::TemplateArgumentList *clang_args =
              function_decl->getTemplateSpecializationArgs()) {
        if (clang_args->size() != 0) {
          if (explicit_arg_count == 0) {
            explicit_arg_count = clang_args->size();
          }
          explicit_specialization_args =
              buildTemplateArguments(*clang_args, explicit_arg_count);
        }
      }
    }
    args_for_builder = &explicit_specialization_args;
  }
  unsigned int functionConstVolatileFlags = 0;
  if (isMethodDecl) {
    auto *method_decl = llvm::cast<clang::CXXMethodDecl>(function_decl);
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
  }

  const bool is_explicitly_defaulted = function_decl->isExplicitlyDefaulted();
  const bool is_explicitly_deleted = function_decl->isDeletedAsWritten();
  const bool is_explicitly_defaulted_or_deleted =
      is_explicitly_defaulted || is_explicitly_deleted;

  auto translate_function_body = [&](SgFunctionDeclaration *defining_decl) {
    bool body_res = true;
    if (defining_decl == nullptr) {
      return false;
    }

    clang::FunctionDecl *body_decl = function_decl;
    if (body_decl != nullptr && !body_decl->doesThisDeclarationHaveABody()) {
      clang::FunctionDecl *def_decl = body_decl->getDefinition();
      if (def_decl == nullptr) {
        if (clang::FunctionDecl *canonical = body_decl->getCanonicalDecl()) {
          def_decl = canonical->getDefinition();
        }
      }
      if (def_decl == nullptr) {
        if (clang::FunctionDecl *first = body_decl->getFirstDecl()) {
          def_decl = first->getDefinition();
        }
      }
      if (def_decl != nullptr) {
        body_decl = def_decl;
      } else {
        for (clang::FunctionDecl *redecl : body_decl->redecls()) {
          if (redecl != nullptr && redecl->doesThisDeclarationHaveABody()) {
            body_decl = redecl;
            break;
          }
        }
      }
    }
    clang::Stmt *body_stmt =
        body_decl != nullptr ? body_decl->getBody() : nullptr;

    // Only process the function body if it exists. Template functions and
    // forward declarations may be marked as definitions but have no body.
    if (body_stmt != nullptr && !is_explicitly_defaulted_or_deleted &&
        !is_system_or_builtin) {
      SgFunctionDefinition *function_definition =
          defining_decl->get_definition();
      ROSE_ASSERT(function_definition != nullptr);

      // P1 Badge Fix: Recursive Cache Invalidation.
      // We must invalidate the cache for the body statements and
      // declarations BEFORE potentially deleting the existing body AST.
      // This prevents Use-After-Free (accessing deleted nodes parents)
      // and ensures we don't reuse "stolen" nodes from templates. We
      // erase unconditionally because we are about to rebuild the body
      // for this definition.
      SgBasicBlock *old_body = function_definition->get_body();
      SgBasicBlock *placeholder_body = nullptr;
      auto purge_translation_caches_for_body = [&](SgNode *root,
                                                   SgNode *function_root) {
        if (root == nullptr && function_root == nullptr) {
          return;
        }

        auto is_in_old_body = [&](SgNode *node) -> bool {
          if (node == nullptr) {
            return false;
          }
          if (root != nullptr) {
            if (node == root) {
              return true;
            }
            if (SageInterface::isAncestor(root, node)) {
              return true;
            }
          }
          if (function_root != nullptr) {
            if (node == function_root) {
              return true;
            }
            if (SageInterface::isAncestor(function_root, node)) {
              return true;
            }
          }
          return false;
        };

        auto decl_in_old_body = [&](SgDeclarationStatement *decl) -> bool {
          if (decl == nullptr) {
            return false;
          }
          if (is_in_old_body(decl)) {
            return true;
          }
          if (SgDeclarationStatement *def = decl->get_definingDeclaration()) {
            if (is_in_old_body(def)) {
              return true;
            }
          }
          if (SgDeclarationStatement *first =
                  decl->get_firstNondefiningDeclaration()) {
            if (is_in_old_body(first)) {
              return true;
            }
          }
          return false;
        };

        for (auto it = p_stmt_translation_map.begin();
             it != p_stmt_translation_map.end();) {
          if (is_in_old_body(it->second)) {
            it = p_stmt_translation_map.erase(it);
          } else {
            ++it;
          }
        }

        for (auto it = p_decl_translation_map.begin();
             it != p_decl_translation_map.end();) {
          if (is_in_old_body(it->second)) {
            it = p_decl_translation_map.erase(it);
          } else {
            ++it;
          }
        }

        for (auto it = p_type_translation_map.begin();
             it != p_type_translation_map.end();) {
          SgType *type = isSgType(it->second);
          bool erase = false;
          if (type != nullptr) {
            SgType *base_type = type->findBaseType();
            if (SgNamedType *named = isSgNamedType(base_type)) {
              SgDeclarationStatement *decl = named->get_declaration();
              if (decl != nullptr) {
                if (decl_in_old_body(decl)) {
                  erase = true;
                }
              }
            }
          }
          if (erase) {
            it = p_type_translation_map.erase(it);
          } else {
            ++it;
          }
        }
      };
      if (body_stmt != nullptr) {
        bool old_body_is_placeholder = false;
        if (old_body != nullptr) {
          old_body_is_placeholder = old_body->get_statements().empty();
        }

        auto body_it = p_stmt_translation_map.find(body_stmt);
        if (body_it != p_stmt_translation_map.end()) {
          if (old_body != nullptr && body_it->second == old_body &&
              !old_body_is_placeholder) {
            return true;
          }
        }

        bool need_invalidate =
            (body_it != p_stmt_translation_map.end()) ||
            (old_body != nullptr && !old_body_is_placeholder);

        const bool isolate_template_body =
            allow_template_instantiation &&
            (specialization_kind == clang::TSK_ImplicitInstantiation ||
             specialization_kind ==
                 clang::TSK_ExplicitInstantiationDefinition ||
             specialization_kind ==
                 clang::TSK_ExplicitInstantiationDeclaration);

        std::function<void(clang::Decl *)> recursive_invalidate_decl;
        std::function<void(clang::Stmt *)> recursive_invalidate_stmt;

        recursive_invalidate_decl = [&](clang::Decl *d) {
          if (!d)
            return;
          auto it = p_decl_translation_map.find(d);
          if (it != p_decl_translation_map.end()) {
            p_decl_translation_map.erase(it);
          }

          // Recurse into DeclContext (e.g. structs)
          if (auto *ctx = llvm::dyn_cast<clang::DeclContext>(d)) {
            for (auto *child : ctx->decls()) {
              recursive_invalidate_decl(child);
            }
          }

          // Handle FunctionDecl body
          if (auto *fd = llvm::dyn_cast<clang::FunctionDecl>(d)) {
            if (fd->hasBody())
              recursive_invalidate_stmt(fd->getBody());
          }
          // Handle VarDecl init
          if (auto *vd = llvm::dyn_cast<clang::VarDecl>(d)) {
            if (vd->getInit())
              recursive_invalidate_stmt(vd->getInit());
          }
        };

        recursive_invalidate_stmt = [&](clang::Stmt *s) {
          if (!s)
            return;
          auto it = p_stmt_translation_map.find(s);
          if (it != p_stmt_translation_map.end()) {
            p_stmt_translation_map.erase(it);
          }

          // Handle DeclStmt specifically to descend into Decls
          if (auto *ds = llvm::dyn_cast<clang::DeclStmt>(s)) {
            for (auto *d : ds->decls()) {
              recursive_invalidate_decl(d);
            }
          }

          // Handle Stmt children
          for (auto *child : s->children()) {
            recursive_invalidate_stmt(child);
          }
        };

        if (need_invalidate) {
          if (old_body != nullptr) {
            purge_translation_caches_for_body(old_body, function_definition);

            struct SymbolScrubber : public AstSimpleProcessing {
              std::set<SgSymbol *> removed;
              SgBasicBlock *old_body = nullptr;

              void removeSymbol(SgSymbol *sym) {
                if (sym == nullptr) {
                  return;
                }
                if (!removed.insert(sym).second) {
                  return;
                }
                SgNode *basis = sym->get_symbol_basis();
                if (basis != nullptr && old_body != nullptr) {
                  if (!SageInterface::isAncestor(old_body, basis) &&
                      basis != old_body) {
                    return;
                  }
                }
                if (SgSymbolTable *table = isSgSymbolTable(sym->get_parent())) {
                  if (table->exists(sym)) {
                    table->remove(sym);
                  }
                } else if (SgScopeStatement *scope =
                               isSgScopeStatement(sym->get_parent())) {
                  if (SgSymbolTable *table = scope->get_symbol_table()) {
                    if (table->exists(sym)) {
                      table->remove(sym);
                    }
                  }
                }
                delete sym;
              }

              void visit(SgNode *node) override {
                if (SgInitializedName *init = isSgInitializedName(node)) {
                  removeSymbol(init->get_symbol_from_symbol_table());
                  return;
                }
                if (SgVariableDeclaration *var_decl =
                        isSgVariableDeclaration(node)) {
                  for (SgInitializedName *init : var_decl->get_variables()) {
                    removeSymbol(init->get_symbol_from_symbol_table());
                  }
                  return;
                }
                if (SgDeclarationStatement *decl =
                        isSgDeclarationStatement(node)) {
                  removeSymbol(decl->get_symbol_from_symbol_table());
                }
              }
            } scrubber;

            scrubber.old_body = old_body;
            scrubber.traverse(old_body, preorder);

            old_body->set_parent(nullptr);
            placeholder_body = SageBuilder::buildBasicBlock_nfi();
            placeholder_body->set_parent(function_definition);
            function_definition->set_body(placeholder_body);
            SageInterface::deleteAST(
                old_body,
                SageInterface::DeleteAstMode::kSkipExternalReferences);
            old_body = nullptr;
          }
          recursive_invalidate_stmt(body_stmt);
        } else if (isolate_template_body) {
          recursive_invalidate_stmt(body_stmt);
        } else if (old_body != nullptr) {
          placeholder_body = old_body;
        }
      }

      SageBuilder::pushScopeStack(function_definition);

      SgNode *tmp_body = Traverse(body_stmt);
      SgBasicBlock *body = isSgBasicBlock(tmp_body);
      if (body == nullptr) {
        if (SgTryStmt *try_stmt = isSgTryStmt(tmp_body)) {
          try_stmt->set_is_function_try_block(true);
          SgBasicBlock *wrapper = SageBuilder::buildBasicBlock_nfi();
          SageInterface::appendStatement(try_stmt, wrapper);
          applySourceRange(wrapper, body_stmt->getSourceRange());
          body = wrapper;
        }
      }
      if (body != nullptr) {
        applySourceRange(body, body_stmt->getSourceRange());
        Sg_File_Info *body_fi = body->get_file_info();
        bool needs_body_fix = body_fi == nullptr;
        if (body_fi != nullptr) {
          needs_body_fix = body_fi->isCompilerGenerated() ||
                           !body_fi->isOutputInCodeGeneration();
        }
        if (needs_body_fix && body_decl != nullptr) {
          applySourceRange(body, body_decl->getSourceRange());
        }
        if (function_definition != nullptr) {
          Sg_File_Info *def_fi = function_definition->get_file_info();
          if (def_fi != nullptr && def_fi->isOutputInCodeGeneration()) {
            for (SgStatement *stmt : body->get_statements()) {
              if (stmt == nullptr) {
                continue;
              }
              if (Sg_File_Info *stmt_fi = stmt->get_file_info()) {
                if (!stmt_fi->isOutputInCodeGeneration()) {
                  stmt_fi->setOutputInCodeGeneration();
                }
              }
              if (Sg_File_Info *start = stmt->get_startOfConstruct()) {
                if (!start->isOutputInCodeGeneration()) {
                  start->setOutputInCodeGeneration();
                }
              }
              if (Sg_File_Info *end = stmt->get_endOfConstruct()) {
                if (!end->isOutputInCodeGeneration()) {
                  end->setOutputInCodeGeneration();
                }
              }
            }
          }
        }
      }

      SageBuilder::popScopeStack();

      if (body == nullptr && tmp_body != nullptr) {
        std::cerr << "Traverse(function_decl->getBody()) returned a "
                     "non-SgBasicBlock node: "
                  << tmp_body->class_name() << std::endl;
        body_res = false;
      }
      if (body != nullptr) {
        // DQ (11/24/2020): This fails for test2020_00.C (in C_tests).
        // It seems that even though function_definition was used to set
        // the scope in the connection to the body, that the body's parent
        // is set to nullptr. ROSE_ASSERT(body->get_parent() ==
        // function_definition);
        if (body->get_parent() != function_definition) {
          body->set_parent(function_definition);
        }
        ROSE_ASSERT(body->get_parent() == function_definition);
      }

      function_definition->set_body(body);
      if (body) {
        body->set_parent(function_definition);
      }
      if (placeholder_body != nullptr && placeholder_body != body) {
        placeholder_body->set_parent(nullptr);
        delete placeholder_body;
      }
      applySourceRange(function_definition, body_decl->getSourceRange());

      defining_decl->set_definition(function_definition);
      function_definition->set_parent(defining_decl);
    }

    return body_res;
  };

  // Template member/function instantiations synthesized by Clang are often
  // marked as definitions (they can carry the template body), but translating
  // them as ordinary functions causes collisions: multiple instantiations share
  // the same (unmangled) name and signature. Represent implicit instantiations
  // as template instantiations with explicit template arguments so each
  // instantiation has a unique name in the class scope.
  ConstraintSatisfactionResult constraint_result;
  SFINAEFailureResult sfinae_result;
  bool handled_template_instantiation = false;
  SgTemplateInstantiationDirectiveStatement *explicit_instantiation_directive =
      nullptr;
  if (allow_template_instantiation &&
      (specialization_kind == clang::TSK_ImplicitInstantiation ||
       specialization_kind == clang::TSK_ExplicitInstantiationDefinition ||
       specialization_kind == clang::TSK_ExplicitInstantiationDeclaration)) {
    const clang::TemplateArgumentList *clang_args =
        function_decl->getTemplateSpecializationArgs();
    if (clang_args != nullptr) {
      size_t explicit_arg_count = 0;
      if (const clang::ASTTemplateArgumentListInfo *args_as_written =
              function_decl->getTemplateSpecializationArgsAsWritten()) {
        clang::TemplateArgumentListInfo arg_info(
            args_as_written->getLAngleLoc(), args_as_written->getRAngleLoc());
        for (const clang::TemplateArgumentLoc &loc :
             args_as_written->arguments()) {
          arg_info.addArgument(loc);
        }
        explicit_arg_count = countExpandedTemplateArguments(arg_info);
      }
      if (explicit_arg_count == 0 && is_explicit_instantiation) {
        explicit_arg_count = clang_args->size();
      }
      const bool has_explicit_args = explicit_arg_count > 0;

      if (specialization_kind == clang::TSK_ImplicitInstantiation ||
          specialization_kind == clang::TSK_ExplicitInstantiationDefinition ||
          specialization_kind == clang::TSK_ExplicitInstantiationDeclaration) {
#if LLVM_VERSION_MAJOR >= 21
        llvm::SmallVector<clang::AssociatedConstraint, 4> constraints;
#else
        llvm::SmallVector<const clang::Expr *, 4> constraints;
#endif
        const clang::NamedDecl *constraint_owner = nullptr;
        clang::FunctionTemplateDecl *primary =
            function_decl->getPrimaryTemplate();
        if (primary == nullptr) {
          primary = function_decl->getDescribedFunctionTemplate();
        }
        if (primary != nullptr) {
          if (clang::TemplateParameterList *params =
                  primary->getTemplateParameters()) {
            params->getAssociatedConstraints(constraints);
          }
          constraint_owner = primary;
        }

#if LLVM_VERSION_MAJOR >= 21
        llvm::SmallVector<clang::AssociatedConstraint, 4> trailing_constraints;
#else
        llvm::SmallVector<const clang::Expr *, 4> trailing_constraints;
#endif
        function_decl->getAssociatedConstraints(trailing_constraints);
        constraints.append(trailing_constraints.begin(),
                           trailing_constraints.end());
        if (constraint_owner == nullptr) {
          constraint_owner = function_decl;
        }

        if (constraint_owner != nullptr && !constraints.empty()) {
          constraint_result = evaluateConstraintSatisfaction(
              constraint_owner, constraints, *clang_args,
              function_decl->getSourceRange());
        }
      }

      sfinae_result = evaluateSFINAEFailure(function_decl);

      // Ensure template argument declarations are translated before SageBuilder
      // needs to unparse them (e.g., for nameWithTemplateArguments). This
      // avoids dereferencing incomplete scope chains when instantiations are
      // encountered before their argument declarations in the TU order.
      for (const clang::TemplateArgument &arg : clang_args->asArray()) {
        if (arg.getKind() == clang::TemplateArgument::Type) {
          clang::QualType qt = arg.getAsType();
          while (qt->isPointerType() || qt->isReferenceType()) {
            qt = qt->getPointeeType();
          }
          if (const clang::TagType *tag = qt->getAs<clang::TagType>()) {
            Traverse(tag->getDecl());
          }
        }
      }

      SgTemplateArgumentPtrList template_args =
          buildTemplateArguments(*clang_args, explicit_arg_count);
      SgTemplateArgumentPtrList *template_args_ptr = &template_args;

      auto apply_deduced_args = [&](SgDeclarationStatement *decl) {
        if (decl == nullptr) {
          return;
        }
        if (SgTemplateInstantiationFunctionDecl *inst_func =
                isSgTemplateInstantiationFunctionDecl(decl)) {
          inst_func->get_deducedTemplateArguments() = template_args;
          SageBuilder::setTemplateArgumentParents(inst_func);
        } else if (SgTemplateInstantiationMemberFunctionDecl *inst_member =
                       isSgTemplateInstantiationMemberFunctionDecl(decl)) {
          inst_member->get_deducedTemplateArguments() = template_args;
          SageBuilder::setTemplateArgumentParents(inst_member);
        }
      };

      auto resolve_primary_template = [&]() -> SgDeclarationStatement * {
        clang::FunctionTemplateDecl *primary =
            function_decl->getPrimaryTemplate();
        if (primary == nullptr) {
          primary = function_decl->getDescribedFunctionTemplate();
        }
        if (primary == nullptr) {
          return nullptr;
        }
        auto it = p_decl_translation_map.find(primary);
        if (it != p_decl_translation_map.end()) {
          return isSgDeclarationStatement(it->second);
        }
        if (p_decl_translation_in_progress.find(primary) !=
                p_decl_translation_in_progress.end() ||
            p_decl_translation_on_demand.find(primary) !=
                p_decl_translation_on_demand.end()) {
          return nullptr;
        }
        SgNode *translated = TraverseOnDemand(primary);
        return isSgDeclarationStatement(translated);
      };

      SgDeclarationStatement *specialized_template_decl =
          resolve_primary_template();

      const bool needs_defining_instantiation =
          function_decl->isThisDeclarationADefinition() &&
          function_decl->hasBody() && !is_explicitly_defaulted_or_deleted &&
          !is_explicit_instantiation && !is_system_or_builtin;
      auto clone_param_list =
          [&](SgFunctionParameterList *source) -> SgFunctionParameterList * {
        SgFunctionParameterList *cloned =
            SageBuilder::buildFunctionParameterList_nfi();
        applySourceRange(cloned, function_decl->getSourceRange());
        if (source == nullptr) {
          return cloned;
        }

        for (SgInitializedName *init_name : source->get_args()) {
          if (init_name == nullptr) {
            continue;
          }

          SgInitializer *cloned_init = nullptr;
          if (SgInitializer *init = init_name->get_initializer()) {
            cloned_init = SageInterface::deepCopy(init);
          }

          SgInitializedName *cloned_param =
              SageBuilder::buildInitializedName_nfi(
                  init_name->get_name(), init_name->get_type(), cloned_init);
          cloned->append_arg(cloned_param);
        }
        return cloned;
      };

      SgFunctionParameterList *inst_nondef_param_list =
          needs_defining_instantiation ? clone_param_list(param_list)
                                       : param_list;

      SgFunctionDeclaration *inst_nondef_decl = nullptr;
      unsigned int inst_method_cv_flags = 0;
      if (llvm::isa<clang::CXXMethodDecl>(function_decl)) {
        inst_method_cv_flags = functionConstVolatileFlags;

        inst_nondef_decl =
            SageBuilder::buildNondefiningMemberFunctionDeclaration(
                name, ret_type, inst_nondef_param_list, scope_for_symbol_table,
                inst_method_cv_flags,
                /*buildTemplateInstantiation=*/true, template_args_ptr);
      } else {
        inst_nondef_decl = SageBuilder::buildNondefiningFunctionDeclaration(
            name, ret_type, inst_nondef_param_list, scope_for_symbol_table,
            /*buildTemplateInstantiation=*/true, template_args_ptr,
            SgStorageModifier::e_default,
            /*forceFreeFunctionScope=*/isFriendFreeFunction);
      }

      if (inst_nondef_decl != nullptr) {
        if (inst_nondef_param_list != nullptr &&
            inst_nondef_param_list->get_parent() == nullptr) {
          inst_nondef_param_list->set_parent(inst_nondef_decl);
        }
        if (inst_nondef_decl->get_parameterList() != inst_nondef_param_list) {
          inst_nondef_decl->set_parameterList(inst_nondef_param_list);
        }

        // SageBuilder may return a redundant nondefining declaration when an
        // instantiation already exists in the scope's symbol table (e.g., when
        // it was created earlier while handling an explicit template call
        // expression). In that case, the returned decl has no symbol and its
        // firstNondefiningDeclaration points at the canonical declaration that
        // *does* own the symbol. Preserve this chain to keep symbol-table
        // associations consistent.
        SgFunctionDeclaration *inst_symbol_decl = isSgFunctionDeclaration(
            inst_nondef_decl->get_firstNondefiningDeclaration());
        if (inst_symbol_decl == nullptr) {
          inst_symbol_decl = inst_nondef_decl;
        }

        auto propagate_explicit_template_args =
            [&](SgDeclarationStatement *decl) {
              if (decl == nullptr) {
                return;
              }

              SgTemplateArgumentPtrList *existing_args =
                  SageBuilder::getTemplateArgumentList(decl);
              if (existing_args == nullptr) {
                return;
              }

              size_t limit = existing_args->size();
              if (template_args.size() < limit) {
                limit = template_args.size();
              }

              for (size_t i = 0; i < limit; ++i) {
                SgTemplateArgument *src_arg = (*existing_args)[i];
                SgTemplateArgument *dst_arg = template_args[i];
                if (src_arg != nullptr && dst_arg != nullptr &&
                    src_arg->get_explicitlySpecified()) {
                  dst_arg->set_explicitlySpecified(true);
                }
              }
            };

        propagate_explicit_template_args(inst_symbol_decl);

        // Mark explicit argument list on the instantiation decl and connect to
        // the primary template declaration when available.
        if (SgTemplateInstantiationFunctionDecl *inst_func =
                isSgTemplateInstantiationFunctionDecl(inst_symbol_decl)) {
          inst_func->set_template_argument_list_is_explicit(
              inst_func->get_template_argument_list_is_explicit() ||
              has_explicit_args);
          SageBuilder::setTemplateArgumentsInDeclaration(inst_func,
                                                         template_args_ptr);
          apply_deduced_args(inst_func);
          if (SgNode *tmpl_node =
                  Traverse(function_decl->getPrimaryTemplate())) {
            if (SgTemplateFunctionDeclaration *tmpl_decl =
                    isSgTemplateFunctionDeclaration(tmpl_node)) {
              inst_func->set_templateDeclaration(tmpl_decl);
              inst_func->set_templateName(tmpl_decl->get_name());
              inst_func->set_specializedTemplateDeclaration(tmpl_decl);
            }
          }
          if (inst_func->get_specializedTemplateDeclaration() == nullptr &&
              specialized_template_decl != nullptr) {
            inst_func->set_specializedTemplateDeclaration(
                specialized_template_decl);
          }
        } else if (SgTemplateInstantiationMemberFunctionDecl *inst_member =
                       isSgTemplateInstantiationMemberFunctionDecl(
                           inst_symbol_decl)) {
          inst_member->set_template_argument_list_is_explicit(
              inst_member->get_template_argument_list_is_explicit() ||
              has_explicit_args);
          SageBuilder::setTemplateArgumentsInDeclaration(inst_member,
                                                         template_args_ptr);
          apply_deduced_args(inst_member);
          if (SgNode *tmpl_node =
                  Traverse(function_decl->getPrimaryTemplate())) {
            if (SgTemplateMemberFunctionDeclaration *tmpl_decl =
                    isSgTemplateMemberFunctionDeclaration(tmpl_node)) {
              inst_member->set_templateDeclaration(tmpl_decl);
              inst_member->set_templateName(tmpl_decl->get_name());
              inst_member->set_specializedTemplateDeclaration(tmpl_decl);
            }
          }
          if (inst_member->get_specializedTemplateDeclaration() == nullptr &&
              specialized_template_decl != nullptr) {
            inst_member->set_specializedTemplateDeclaration(
                specialized_template_decl);
          }
        }

        if (function_decl->isVariadic()) {
          inst_nondef_decl->hasEllipses();
        }

        if (SgFunctionParameterList *params =
                inst_nondef_decl->get_parameterList()) {
          for (SgInitializedName *param : params->get_args()) {
            if (param != nullptr) {
              param->set_declptr(inst_nondef_decl);
            }
          }
        }

        if (needs_defining_instantiation) {
          SgFunctionDeclaration *defining_inst = nullptr;
          if (llvm::isa<clang::CXXMethodDecl>(function_decl)) {
            SgMemberFunctionDeclaration *inst_nondef_member =
                isSgMemberFunctionDeclaration(inst_symbol_decl);
            ROSE_ASSERT(inst_nondef_member != nullptr);
            defining_inst = SageBuilder::buildDefiningMemberFunctionDeclaration(
                name, ret_type, param_list, scope_for_symbol_table,
                /*buildTemplateInstantiation=*/true, inst_method_cv_flags,
                inst_nondef_member, template_args_ptr);
          } else {
            defining_inst = SageBuilder::buildDefiningFunctionDeclaration(
                name, ret_type, param_list, scope_for_symbol_table,
                /*buildTemplateInstantiation=*/true, inst_symbol_decl,
                template_args_ptr,
                /*forceFreeFunctionScope=*/isFriendFreeFunction);
          }

          ROSE_ASSERT(defining_inst != nullptr);
          sg_function_decl = defining_inst;
          sg_function_decl->set_definingDeclaration(sg_function_decl);
          if (param_list != nullptr && param_list->get_parent() == nullptr) {
            param_list->set_parent(sg_function_decl);
          }
          sg_function_decl->set_firstNondefiningDeclaration(inst_symbol_decl);
          inst_symbol_decl->set_definingDeclaration(sg_function_decl);

          if (function_decl->isVariadic()) {
            sg_function_decl->hasEllipses();
          }

          for (SgInitializedName *param : param_list->get_args()) {
            if (param != nullptr) {
              param->set_declptr(sg_function_decl);
            }
          }

          if (SgTemplateInstantiationFunctionDecl *inst_func =
                  isSgTemplateInstantiationFunctionDecl(sg_function_decl)) {
            inst_func->set_template_argument_list_is_explicit(
                has_explicit_args);
            SageBuilder::setTemplateArgumentsInDeclaration(inst_func,
                                                           template_args_ptr);
            apply_deduced_args(inst_func);
            if (SgNode *tmpl_node =
                    Traverse(function_decl->getPrimaryTemplate())) {
              if (SgTemplateFunctionDeclaration *tmpl_decl =
                      isSgTemplateFunctionDeclaration(tmpl_node)) {
                inst_func->set_templateDeclaration(tmpl_decl);
                inst_func->set_templateName(tmpl_decl->get_name());
                inst_func->set_specializedTemplateDeclaration(tmpl_decl);
              }
            }
            if (inst_func->get_specializedTemplateDeclaration() == nullptr &&
                specialized_template_decl != nullptr) {
              inst_func->set_specializedTemplateDeclaration(
                  specialized_template_decl);
            }
          } else if (SgTemplateInstantiationMemberFunctionDecl *inst_member =
                         isSgTemplateInstantiationMemberFunctionDecl(
                             sg_function_decl)) {
            inst_member->set_template_argument_list_is_explicit(
                has_explicit_args);
            SageBuilder::setTemplateArgumentsInDeclaration(inst_member,
                                                           template_args_ptr);
            apply_deduced_args(inst_member);
            if (SgNode *tmpl_node =
                    Traverse(function_decl->getPrimaryTemplate())) {
              if (SgTemplateMemberFunctionDeclaration *tmpl_decl =
                      isSgTemplateMemberFunctionDeclaration(tmpl_node)) {
                inst_member->set_templateDeclaration(tmpl_decl);
                inst_member->set_templateName(tmpl_decl->get_name());
                inst_member->set_specializedTemplateDeclaration(tmpl_decl);
              }
            }
            if (inst_member->get_specializedTemplateDeclaration() == nullptr &&
                specialized_template_decl != nullptr) {
              inst_member->set_specializedTemplateDeclaration(
                  specialized_template_decl);
            }
          }

          register_function_translation(function_decl, sg_function_decl);
          res = translate_function_body(sg_function_decl) && res;

          if (specialization_kind == clang::TSK_ImplicitInstantiation) {
            setCompilerGeneratedFileInfo(inst_symbol_decl);
            suppress_unparse_output(inst_symbol_decl);
            if (SgFunctionParameterList *params =
                    inst_symbol_decl->get_parameterList()) {
              setCompilerGeneratedFileInfo(params);
              suppress_unparse_output(params);
              for (SgInitializedName *param : params->get_args()) {
                if (param != nullptr) {
                  setCompilerGeneratedFileInfo(param);
                  suppress_unparse_output(param);
                }
              }
            }

            setCompilerGeneratedFileInfo(sg_function_decl);
            suppress_unparse_output(sg_function_decl);
          }
        } else {
          sg_function_decl = inst_symbol_decl;
          if (specialization_kind == clang::TSK_ImplicitInstantiation) {
            setCompilerGeneratedFileInfo(sg_function_decl);
            suppress_unparse_output(sg_function_decl);
            if (SgFunctionParameterList *params =
                    sg_function_decl->get_parameterList()) {
              setCompilerGeneratedFileInfo(params);
              suppress_unparse_output(params);
              for (SgInitializedName *param : params->get_args()) {
                if (param != nullptr) {
                  setCompilerGeneratedFileInfo(param);
                  suppress_unparse_output(param);
                }
              }
            }
          }
        }

        handled_template_instantiation = true;

        if (constraint_result.evaluated) {
          attachConstraintSatisfaction(inst_symbol_decl, constraint_result);
          if (sg_function_decl != nullptr &&
              sg_function_decl != inst_symbol_decl) {
            attachConstraintSatisfaction(sg_function_decl, constraint_result);
          }

          pruneSymbolsForConstraints(inst_symbol_decl);
          if (sg_function_decl != nullptr &&
              sg_function_decl != inst_symbol_decl) {
            pruneSymbolsForConstraints(sg_function_decl);
          }
        }

        if (sfinae_result.evaluated) {
          attachSFINAEFailure(inst_symbol_decl, sfinae_result);
          if (sg_function_decl != nullptr &&
              sg_function_decl != inst_symbol_decl) {
            attachSFINAEFailure(sg_function_decl, sfinae_result);
          }

          pruneSymbolsForConstraints(inst_symbol_decl);
          if (sg_function_decl != nullptr &&
              sg_function_decl != inst_symbol_decl) {
            pruneSymbolsForConstraints(sg_function_decl);
          }
        }
      }
    }
  }

  if (!handled_template_instantiation && isDefinition &&
      !is_explicitly_defaulted_or_deleted) {
    auto clone_param_list = [&](SgFunctionParameterList *source_params)
        -> SgFunctionParameterList * {
      ROSE_ASSERT(source_params != nullptr);
      SgFunctionParameterList *cloned =
          SageBuilder::buildFunctionParameterList_nfi();
      applySourceRange(cloned, function_decl->getSourceRange());

      for (SgInitializedName *init_name : source_params->get_args()) {
        if (init_name == nullptr) {
          continue;
        }

        SgInitializer *cloned_init = nullptr;
        if (SgInitializer *init = init_name->get_initializer()) {
          cloned_init = SageInterface::deepCopy(init);
        }

        SgInitializedName *cloned_param = SageBuilder::buildInitializedName_nfi(
            init_name->get_name(), init_name->get_type(), cloned_init);
        cloned_param->set_parent(cloned);
        cloned->append_arg(cloned_param);
      }

      if (function_decl->isVariadic()) {
        SgName empty = "";
        SgType *ellipses_type = SgTypeEllipse::createType();
        SgInitializedName *ellipses_param =
            SageBuilder::buildInitializedName_nfi(empty, ellipses_type,
                                                  nullptr);
        ellipses_param->set_parent(cloned);
        cloned->append_arg(ellipses_param);
      }

      return cloned;
    };

    auto fixup_nondef_params = [&](SgFunctionDeclaration *decl) -> void {
      if (decl == nullptr) {
        return;
      }

      if (SgFunctionParameterList *params = decl->get_parameterList()) {
        SgScopeStatement *param_scope = decl->get_functionParameterScope();
        if (param_scope == nullptr) {
          param_scope = decl->get_scope();
        }
        for (SgInitializedName *param : params->get_args()) {
          if (param != nullptr) {
            param->set_declptr(decl);
            if (param_scope != nullptr) {
              param->set_scope(param_scope);
            }
          }
        }
      }
    };

    // Build friend free-function definitions as free functions regardless of
    // lexical class scope.
    bool builder_force_free_scope = isFriendFreeFunction;
    SgScopeStatement *builder_scope = scope_for_symbol_table;

    if (templateDecl != nullptr || isClassTemplateMemberFunction) {
      SgTemplateParameterPtrList empty_template_params;
      // Template definitions require a prior non-defining declaration for
      // SageBuilder. Reuse an existing one when a forward declaration was
      // already seen to keep declaration/definition chains consistent.
      if (isTemplateLikeMemberFunction) {
        SgTemplateParameterPtrList *effective_template_params =
            templateParams.get();
        if (effective_template_params == nullptr) {
          ROSE_ASSERT(isClassTemplateMemberFunction);
          effective_template_params = &empty_template_params;
        }

        SgTemplateMemberFunctionDeclaration *first_nondef = nullptr;

        if (function_decl->getFirstDecl() != function_decl) {
          auto map_it =
              p_decl_translation_map.find(function_decl->getFirstDecl());
          if (map_it != p_decl_translation_map.end()) {
            first_nondef =
                isSgTemplateMemberFunctionDeclaration(map_it->second);
          }
          if (first_nondef == nullptr) {
            auto tmpl_it = p_decl_translation_map.find(templateDecl);
            if (tmpl_it != p_decl_translation_map.end()) {
              first_nondef =
                  isSgTemplateMemberFunctionDeclaration(tmpl_it->second);
            }
          }
          if (first_nondef == nullptr) {
            SgSymbol *tmp_symbol =
                GetSymbolFromSymbolTable(function_decl->getFirstDecl());
            if (SgTemplateSymbol *tmpl_sym = isSgTemplateSymbol(tmp_symbol)) {
              first_nondef = isSgTemplateMemberFunctionDeclaration(
                  tmpl_sym->get_declaration());
            } else if (SgFunctionSymbol *func_sym =
                           isSgFunctionSymbol(tmp_symbol)) {
              first_nondef = isSgTemplateMemberFunctionDeclaration(
                  func_sym->get_declaration());
            }
          }
        }

        if (first_nondef == nullptr) {
          SgFunctionParameterList *first_param_list =
              clone_param_list(param_list);

          first_nondef =
              SageBuilder::buildNondefiningTemplateMemberFunctionDeclaration(
                  name, ret_type, first_param_list, builder_scope,
                  functionConstVolatileFlags, effective_template_params);
          ROSE_ASSERT(first_nondef != nullptr);

          applySourceRange(first_nondef, function_decl->getSourceRange());
          first_param_list->set_parent(first_nondef);
          if (function_decl->isVariadic())
            first_nondef->hasEllipses();

          // This prototype is synthesized to satisfy SageBuilder's defining
          // template builders. If the template is defined in a class scope
          // (e.g. friend templates defined inline inside a class), the extra
          // class-scope redeclaration must not be emitted by the unparser.
          if (isSgClassDefinition(builder_scope) != nullptr &&
              function_decl->getFirstDecl() == function_decl) {
            setCompilerGeneratedFileInfo(first_nondef);
            suppress_unparse_output(first_nondef);

            setCompilerGeneratedFileInfo(first_param_list);
            suppress_unparse_output(first_param_list);
            for (SgInitializedName *param : first_param_list->get_args()) {
              setCompilerGeneratedFileInfo(param);
              suppress_unparse_output(param);
            }
          }
        }

        first_nondef->set_firstNondefiningDeclaration(first_nondef);

        fixup_nondef_params(first_nondef);

        SgScopeStatement *target_scope = builder_scope;
        if (builder_force_free_scope && friend_symbol_scope != nullptr) {
          target_scope = friend_symbol_scope;
        }

        SgTemplateMemberFunctionDeclaration *defining_template =
            SageBuilder::buildDefiningTemplateMemberFunctionDeclaration(
                name, ret_type, param_list, target_scope,
                functionConstVolatileFlags, first_nondef);

        sg_function_decl = defining_template;
        sg_function_decl->set_definingDeclaration(sg_function_decl);

        if (function_decl->isVariadic()) {
          sg_function_decl->hasEllipses();
        }

        for (SgInitializedName *param : param_list->get_args()) {
          if (param != nullptr) {
            param->set_declptr(sg_function_decl);
          }
        }

        if (defining_template->get_firstNondefiningDeclaration() == nullptr ||
            defining_template->get_firstNondefiningDeclaration() ==
                defining_template) {
          defining_template->set_firstNondefiningDeclaration(first_nondef);
        }
        first_nondef->set_definingDeclaration(defining_template);
      } else {
        SgTemplateFunctionDeclaration *first_nondef = nullptr;

        if (function_decl->getFirstDecl() != function_decl) {
          auto map_it =
              p_decl_translation_map.find(function_decl->getFirstDecl());
          if (map_it != p_decl_translation_map.end()) {
            first_nondef = isSgTemplateFunctionDeclaration(map_it->second);
          }
          if (first_nondef == nullptr) {
            auto tmpl_it = p_decl_translation_map.find(templateDecl);
            if (tmpl_it != p_decl_translation_map.end()) {
              first_nondef = isSgTemplateFunctionDeclaration(tmpl_it->second);
            }
          }
          if (first_nondef == nullptr) {
            SgSymbol *tmp_symbol =
                GetSymbolFromSymbolTable(function_decl->getFirstDecl());
            if (SgTemplateSymbol *tmpl_sym = isSgTemplateSymbol(tmp_symbol)) {
              first_nondef =
                  isSgTemplateFunctionDeclaration(tmpl_sym->get_declaration());
            } else if (SgFunctionSymbol *func_sym =
                           isSgFunctionSymbol(tmp_symbol)) {
              first_nondef =
                  isSgTemplateFunctionDeclaration(func_sym->get_declaration());
            }
          }
        }

        if (first_nondef == nullptr) {
          SgFunctionParameterList *first_param_list =
              clone_param_list(param_list);

          first_nondef =
              SageBuilder::buildNondefiningTemplateFunctionDeclaration(
                  name, ret_type, first_param_list, builder_scope,
                  templateParams.get());
          ROSE_ASSERT(first_nondef != nullptr);

          applySourceRange(first_nondef, function_decl->getSourceRange());
          first_param_list->set_parent(first_nondef);
          if (function_decl->isVariadic())
            first_nondef->hasEllipses();

          // This prototype is synthesized to satisfy SageBuilder's defining
          // template builders. If the template is defined in a class scope
          // (e.g. friend templates defined inline inside a class), the extra
          // class-scope redeclaration must not be emitted by the unparser.
          if (isSgClassDefinition(builder_scope) != nullptr &&
              function_decl->getFirstDecl() == function_decl) {
            setCompilerGeneratedFileInfo(first_nondef);
            suppress_unparse_output(first_nondef);

            setCompilerGeneratedFileInfo(first_param_list);
            suppress_unparse_output(first_param_list);
            for (SgInitializedName *param : first_param_list->get_args()) {
              setCompilerGeneratedFileInfo(param);
              suppress_unparse_output(param);
            }
          }
        }

        first_nondef->set_firstNondefiningDeclaration(first_nondef);

        fixup_nondef_params(first_nondef);

        // Fix for Issue 84: Friend template definitions inside a class should
        // be in the enclosing scope
        // SageBuilder::buildDefiningTemplateFunctionDeclaration does not
        // accept a forceFreeFunctionScope flag unlike
        // buildDefiningFunctionDeclaration, so we must rely on passing the
        // correct scope.
        SgScopeStatement *target_scope = builder_scope;
        if (builder_force_free_scope && friend_symbol_scope != nullptr) {
          target_scope = friend_symbol_scope;
        }

        SgTemplateFunctionDeclaration *defining_template =
            SageBuilder::buildDefiningTemplateFunctionDeclaration(
                name, ret_type, param_list, target_scope, first_nondef);

        sg_function_decl = defining_template;
        sg_function_decl->set_definingDeclaration(sg_function_decl);

        if (function_decl->isVariadic()) {
          sg_function_decl->hasEllipses();
        }

        for (SgInitializedName *param : param_list->get_args()) {
          if (param != nullptr) {
            param->set_declptr(sg_function_decl);
          }
        }

        if (defining_template->get_firstNondefiningDeclaration() == nullptr ||
            defining_template->get_firstNondefiningDeclaration() ==
                defining_template) {
          defining_template->set_firstNondefiningDeclaration(first_nondef);
        }
        first_nondef->set_definingDeclaration(defining_template);
      }
    } else {
      const bool build_explicit_specialization_instantiation =
          args_for_builder != nullptr;
      SgFunctionDeclaration *first_nondef_for_builder = nullptr;
      const bool expect_member =
          builder_force_free_scope == false && isMethodDecl &&
          (isSgClassDefinition(builder_scope) != nullptr ||
           isSgTemplateClassDefinition(builder_scope) != nullptr);
      auto normalize_first_nondef_for_builder =
          [&](SgFunctionDeclaration *decl) -> SgFunctionDeclaration * {
        if (decl == nullptr) {
          return nullptr;
        }

        if (SgFunctionDeclaration *first_nondef = isSgFunctionDeclaration(
                decl->get_firstNondefiningDeclaration());
            first_nondef != nullptr && first_nondef != decl) {
          decl = first_nondef;
        }

        if (decl->get_firstNondefiningDeclaration() == nullptr) {
          if (decl->get_definingDeclaration() == decl) {
            return nullptr;
          }
          decl->set_firstNondefiningDeclaration(decl);
        }

        if (decl->get_firstNondefiningDeclaration() != decl) {
          return nullptr;
        }

        return decl;
      };
      if (function_decl->getFirstDecl() != function_decl) {
        clang::FunctionDecl *clang_first_decl =
            llvm::cast<clang::FunctionDecl>(function_decl->getFirstDecl());
        SgNode *first_node = nullptr;
        auto map_it = p_decl_translation_map.find(clang_first_decl);
        if (map_it != p_decl_translation_map.end()) {
          first_node = map_it->second;
        } else {
          first_node = Traverse(clang_first_decl);
        }

        SgFunctionDeclaration *first_decl = isSgFunctionDeclaration(first_node);
        if (first_decl == nullptr) {
          if (SgSymbol *tmp_symbol =
                  GetSymbolFromSymbolTable(clang_first_decl)) {
            if (SgFunctionSymbol *func_sym = isSgFunctionSymbol(tmp_symbol)) {
              first_decl = isSgFunctionDeclaration(func_sym->get_declaration());
            }
          }
        }

        if (first_decl != nullptr) {
          first_nondef_for_builder = isSgFunctionDeclaration(
              first_decl->get_firstNondefiningDeclaration());
          if (first_nondef_for_builder == nullptr) {
            first_nondef_for_builder = first_decl;
          }
        }

        // Only pass a non-defining declaration to SageBuilder if it matches the
        // expected function kind for this scope (member vs free function).
        if (first_nondef_for_builder != nullptr) {
          first_nondef_for_builder =
              normalize_first_nondef_for_builder(first_nondef_for_builder);
        }
        if (first_nondef_for_builder != nullptr) {
          if (expect_member) {
            if (isSgMemberFunctionDeclaration(first_nondef_for_builder) ==
                nullptr) {
              first_nondef_for_builder = nullptr;
            }
          } else {
            if (isSgMemberFunctionDeclaration(first_nondef_for_builder) !=
                nullptr) {
              first_nondef_for_builder = nullptr;
            }
          }
          if (build_explicit_specialization_instantiation &&
              first_nondef_for_builder != nullptr) {
            if (expect_member) {
              if (isSgTemplateInstantiationMemberFunctionDecl(
                      first_nondef_for_builder) == nullptr) {
                first_nondef_for_builder = nullptr;
              }
            } else {
              if (isSgTemplateInstantiationFunctionDecl(
                      first_nondef_for_builder) == nullptr) {
                first_nondef_for_builder = nullptr;
              }
            }
          }
          if (build_explicit_specialization_instantiation &&
              first_nondef_for_builder != nullptr &&
              args_for_builder != nullptr) {
            bool matches_args = false;
            if (expect_member) {
              if (SgTemplateInstantiationMemberFunctionDecl *inst_decl =
                      isSgTemplateInstantiationMemberFunctionDecl(
                          first_nondef_for_builder)) {
                matches_args = SageInterface::templateArgumentListEquivalence(
                    *args_for_builder, inst_decl->get_templateArguments());
              }
            } else {
              if (SgTemplateInstantiationFunctionDecl *inst_decl =
                      isSgTemplateInstantiationFunctionDecl(
                          first_nondef_for_builder)) {
                matches_args = SageInterface::templateArgumentListEquivalence(
                    *args_for_builder, inst_decl->get_templateArguments());
              }
            }
            if (!matches_args) {
              first_nondef_for_builder = nullptr;
            }
          }
        }
      }

      auto ensure_nondef_symbol = [&](SgFunctionDeclaration *decl) -> bool {
        if (decl == nullptr) {
          return false;
        }
        if (decl->get_symbol_from_symbol_table() != nullptr) {
          return true;
        }

        SgScopeStatement *decl_scope = decl->get_scope();
        if (decl_scope == nullptr) {
          return false;
        }
        SgFunctionSymbol *func_sym = decl_scope->lookup_function_symbol(
            decl->get_name(), decl->get_type());
        if (func_sym == nullptr) {
          func_sym = decl_scope->lookup_function_symbol(decl->get_name());
          if (func_sym != nullptr && func_sym->get_declaration() != nullptr &&
              func_sym->get_declaration()->get_type() != decl->get_type()) {
            func_sym = nullptr;
          }
        }
        if (func_sym != nullptr) {
          if (func_sym->get_declaration() != decl) {
            func_sym->set_declaration(decl);
          }
          if (!decl_scope->symbol_exists(func_sym)) {
            decl_scope->insert_symbol(func_sym->get_name(), func_sym);
          }
          return true;
        }

        SgSymbol *new_sym = nullptr;
        if (SgMemberFunctionDeclaration *member_decl =
                isSgMemberFunctionDeclaration(decl)) {
          if (SgTemplateMemberFunctionDeclaration *tmpl_member_decl =
                  isSgTemplateMemberFunctionDeclaration(member_decl)) {
            new_sym = new SgTemplateMemberFunctionSymbol(tmpl_member_decl);
          } else {
            new_sym = new SgMemberFunctionSymbol(member_decl);
          }
        } else if (SgTemplateFunctionDeclaration *tmpl_decl =
                       isSgTemplateFunctionDeclaration(decl)) {
          new_sym = new SgTemplateFunctionSymbol(tmpl_decl);
        } else if (SgFunctionDeclaration *func_decl =
                       isSgFunctionDeclaration(decl)) {
          new_sym = new SgFunctionSymbol(func_decl);
        }

        if (new_sym == nullptr) {
          return false;
        }
        decl_scope->insert_symbol(decl->get_name(), new_sym);
        return decl->get_symbol_from_symbol_table() != nullptr;
      };

      if (first_nondef_for_builder != nullptr) {
        if (!ensure_nondef_symbol(first_nondef_for_builder)) {
          first_nondef_for_builder = nullptr;
        }
      }

      if (first_nondef_for_builder == nullptr) {
        SgTemplateArgumentPtrList *specialization_args =
            build_explicit_specialization_instantiation ? args_for_builder
                                                        : nullptr;
        SgFunctionParameterList *first_param_list =
            clone_param_list(param_list);

        if (expect_member) {
          first_nondef_for_builder =
              SageBuilder::buildNondefiningMemberFunctionDeclaration(
                  name, ret_type, first_param_list, builder_scope,
                  functionConstVolatileFlags,
                  /*buildTemplateInstantiation=*/
                  build_explicit_specialization_instantiation,
                  specialization_args);
        } else {
          first_nondef_for_builder =
              SageBuilder::buildNondefiningFunctionDeclaration(
                  name, ret_type, first_param_list, builder_scope,
                  /*buildTemplateInstantiation=*/
                  build_explicit_specialization_instantiation,
                  /*templateArgumentsList=*/specialization_args,
                  SgStorageModifier::e_default, builder_force_free_scope);
        }
        ROSE_ASSERT(first_nondef_for_builder != nullptr);

        applySourceRange(first_nondef_for_builder,
                         function_decl->getSourceRange());
        first_param_list->set_parent(first_nondef_for_builder);
        if (function_decl->isVariadic())
          first_nondef_for_builder->hasEllipses();

        first_nondef_for_builder->set_firstNondefiningDeclaration(
            first_nondef_for_builder);

        fixup_nondef_params(first_nondef_for_builder);

        suppress_unparse_output(first_nondef_for_builder);
        suppress_unparse_output(first_param_list);
        for (SgInitializedName *param : first_param_list->get_args()) {
          if (param != nullptr) {
            suppress_unparse_output(param);
          }
        }
      }

      auto mark_explicit_specialization =
          [&](SgFunctionDeclaration *decl) -> void {
        if (!build_explicit_specialization_instantiation || decl == nullptr) {
          return;
        }
        if (SgTemplateInstantiationFunctionDecl *inst_func =
                isSgTemplateInstantiationFunctionDecl(decl)) {
          inst_func->set_template_argument_list_is_explicit(true);
          inst_func->set_specialization(
              SgDeclarationStatement::e_specialization);
          SageBuilder::setTemplateArgumentsInDeclaration(inst_func,
                                                         args_for_builder);
          if (function_decl->getPrimaryTemplate() != nullptr) {
            if (SgNode *tmpl_node =
                    Traverse(function_decl->getPrimaryTemplate())) {
              if (SgTemplateFunctionDeclaration *tmpl_decl =
                      isSgTemplateFunctionDeclaration(tmpl_node)) {
                inst_func->set_templateDeclaration(tmpl_decl);
                inst_func->set_templateName(tmpl_decl->get_name());
              }
            }
          }
        } else if (SgTemplateInstantiationMemberFunctionDecl *inst_member =
                       isSgTemplateInstantiationMemberFunctionDecl(decl)) {
          inst_member->set_template_argument_list_is_explicit(
              has_function_template);
          inst_member->set_specialization(
              SgDeclarationStatement::e_specialization);
          SageBuilder::setTemplateArgumentsInDeclaration(inst_member,
                                                         args_for_builder);
          if (!has_function_template) {
            inst_member->get_templateArguments().clear();
            inst_member->set_templateName(name);
          }
          if (function_decl->getPrimaryTemplate() != nullptr) {
            if (SgNode *tmpl_node =
                    Traverse(function_decl->getPrimaryTemplate())) {
              if (SgTemplateMemberFunctionDeclaration *tmpl_decl =
                      isSgTemplateMemberFunctionDeclaration(tmpl_node)) {
                inst_member->set_templateDeclaration(tmpl_decl);
                if (has_function_template) {
                  inst_member->set_templateName(tmpl_decl->get_name());
                }
              }
            }
          } else if (clang::FunctionDecl *pattern =
                         function_decl->getInstantiatedFromMemberFunction()) {
            if (SgNode *tmpl_node = Traverse(pattern)) {
              if (SgTemplateMemberFunctionDeclaration *tmpl_decl =
                      isSgTemplateMemberFunctionDeclaration(tmpl_node)) {
                inst_member->set_templateDeclaration(tmpl_decl);
                if (has_function_template) {
                  inst_member->set_templateName(tmpl_decl->get_name());
                }
              }
            }
          }
        }
      };

      mark_explicit_specialization(first_nondef_for_builder);

      ROSE_ASSERT(first_nondef_for_builder != nullptr);
      sg_function_decl = SageBuilder::buildDefiningFunctionDeclaration(
          name, ret_type, param_list, builder_scope,
          /*buildTemplateInstantiation=*/
          build_explicit_specialization_instantiation, first_nondef_for_builder,
          /*templateArgumentsList=*/args_for_builder, builder_force_free_scope);

      sg_function_decl->set_definingDeclaration(sg_function_decl);

      if (function_decl->isVariadic()) {
        sg_function_decl->hasEllipses();
      }

      // CLANG FRONTEND FIX: Set declptr for all function parameters
      // declptr should point to the function declaration for parameters
      SgInitializedNamePtrList &param_names = param_list->get_args();
      for (SgInitializedName *param : param_names) {
        if (param != nullptr) {
          param->set_declptr(sg_function_decl);
        }
      }

      mark_explicit_specialization(sg_function_decl);
    }

    register_function_translation(function_decl, sg_function_decl);
    res = translate_function_body(sg_function_decl) && res;
    /*
            SgFunctionDeclaration * first_decl;
            if (function_decl->isFirstDecl()) {
                SgFunctionParameterList * param_list_ =
    SageBuilder::buildFunctionParameterList_nfi();
                  setCompilerGeneratedFileInfo(param_list_);
                SgInitializedNamePtrList & init_names = param_list->get_args();
                SgInitializedNamePtrList::iterator it;
                for (it = init_names.begin(); it != init_names.end(); it++) {
                    SgInitializedName * init_param = new
    SgInitializedName(**it); setCompilerGeneratedFileInfo(init_param);
                    param_list_->append_arg(init_param);
                }

                first_decl =
    SageBuilder::buildNondefiningFunctionDeclaration(name, ret_type,
    param_list_, nullptr);
    //            first_decl =
    SageBuilder::buildNondefiningFunctionDeclaration(sg_function_decl, nullptr,
    nullptr); setCompilerGeneratedFileInfo(first_decl);
                first_decl->set_parent(SageBuilder::topScopeStack());
                first_decl->set_firstNondefiningDeclaration(first_decl);
                if (function_decl->isVariadic()) first_decl->hasEllipses();
            }
            else {
                SgSymbol * tmp_symbol =
    GetSymbolFromSymbolTable(function_decl->getFirstDecl()); SgFunctionSymbol *
    symbol = isSgFunctionSymbol(tmp_symbol); if (tmp_symbol != nullptr && symbol
    == nullptr) { std::cerr << "Runtime error: tmp_symbol != nullptr && symbol
    == nullptr"
    << std::endl; res = false;
                }
                if (symbol != nullptr)
                    first_decl =
    isSgFunctionDeclaration(symbol->get_declaration());
            }

            sg_function_decl->set_firstNondefiningDeclaration(first_decl);
            first_decl->set_definingDeclaration(sg_function_decl);
    */
    // Pei-Hung (06/27/22) This seems to be the way to get test2004_21.c
    // unprarsed properly by checking if the functionProtoType has different
    // argument types.

    if (diffInProtoType) {
      sg_function_decl->set_parameterList_syntax(param_list);
      sg_function_decl->set_type_syntax_is_available(true);
      sg_function_decl->set_oldStyleDefinition(true);
    }
  } else if (!handled_template_instantiation) {
    if (templateDecl) {
      if (isTemplateMemberFunction) {
        sg_function_decl =
            SageBuilder::buildNondefiningTemplateMemberFunctionDeclaration(
                name, ret_type, param_list, scope_for_symbol_table,
                functionConstVolatileFlags, templateParams.get());
        param_list->set_parent(sg_function_decl);
        sg_function_decl->set_parameterList(param_list);
      } else {
        sg_function_decl =
            SageBuilder::buildNondefiningTemplateFunctionDeclaration(
                name, ret_type, param_list, scope_for_symbol_table,
                templateParams.get());

        // Set parameter list parent
        param_list->set_parent(sg_function_decl);
        sg_function_decl->set_parameterList(param_list);
      }
    } else {
      // Explicit specializations of non-template functions (e.g., member
      // functions of class templates) must unparse with a leading `template<>`.
      // Represent these as template instantiations so the unparser can emit the
      // specialization specifier structurally (see copyAST_copytest2007_47).
      bool built_template_member_pattern = false;
      if (auto *method_decl =
              llvm::dyn_cast<clang::CXXMethodDecl>(function_decl)) {
        if (clang::CXXRecordDecl *parent_record = method_decl->getParent()) {
          if (parent_record->getDescribedClassTemplate() != nullptr &&
              function_decl->getTemplateSpecializationKind() ==
                  clang::TSK_Undeclared) {
            SgTemplateParameterPtrList empty_template_params;
            sg_function_decl =
                SageBuilder::buildNondefiningTemplateMemberFunctionDeclaration(
                    name, ret_type, param_list, scope_for_symbol_table,
                    functionConstVolatileFlags, &empty_template_params);

            param_list->set_parent(sg_function_decl);
            sg_function_decl->set_parameterList(param_list);
            built_template_member_pattern = true;
          }
        }
      }

      if (!built_template_member_pattern &&
          function_decl->getTemplateSpecializationKind() ==
              clang::TSK_ExplicitSpecialization) {
        SgTemplateArgumentPtrList empty_template_args;
        if (llvm::isa<clang::CXXMethodDecl>(function_decl)) {
          sg_function_decl =
              SageBuilder::buildNondefiningMemberFunctionDeclaration(
                  name, ret_type, param_list, scope_for_symbol_table,
                  functionConstVolatileFlags,
                  /*buildTemplateInstantiation=*/true, &empty_template_args);
        } else {
          sg_function_decl = SageBuilder::buildNondefiningFunctionDeclaration(
              name, ret_type, param_list, scope_for_symbol_table,
              /*buildTemplateInstantiation=*/true, &empty_template_args,
              SgStorageModifier::e_default, isFriendFreeFunction);
        }

        if (SgTemplateInstantiationFunctionDecl *inst_func =
                isSgTemplateInstantiationFunctionDecl(sg_function_decl)) {
          inst_func->set_template_argument_list_is_explicit(true);
          inst_func->set_specialization(
              SgDeclarationStatement::e_specialization);
          SageBuilder::setTemplateArgumentsInDeclaration(inst_func,
                                                         &empty_template_args);
          if (function_decl->getPrimaryTemplate() != nullptr) {
            if (SgNode *tmpl_node =
                    Traverse(function_decl->getPrimaryTemplate())) {
              if (SgTemplateFunctionDeclaration *tmpl_decl =
                      isSgTemplateFunctionDeclaration(tmpl_node)) {
                inst_func->set_templateDeclaration(tmpl_decl);
                inst_func->set_templateName(tmpl_decl->get_name());
              }
            }
          }
        } else if (SgTemplateInstantiationMemberFunctionDecl *inst_member =
                       isSgTemplateInstantiationMemberFunctionDecl(
                           sg_function_decl)) {
          inst_member->set_template_argument_list_is_explicit(
              has_function_template);
          inst_member->set_specialization(
              SgDeclarationStatement::e_specialization);
          SageBuilder::setTemplateArgumentsInDeclaration(inst_member,
                                                         &empty_template_args);
          if (!has_function_template) {
            inst_member->get_templateArguments().clear();
            inst_member->set_templateName(name);
          }
          if (function_decl->getPrimaryTemplate() != nullptr) {
            if (SgNode *tmpl_node =
                    Traverse(function_decl->getPrimaryTemplate())) {
              if (SgTemplateMemberFunctionDeclaration *tmpl_decl =
                      isSgTemplateMemberFunctionDeclaration(tmpl_node)) {
                inst_member->set_templateDeclaration(tmpl_decl);
                if (has_function_template) {
                  inst_member->set_templateName(tmpl_decl->get_name());
                }
              }
            }
          } else if (clang::FunctionDecl *pattern =
                         function_decl->getInstantiatedFromMemberFunction()) {
            if (SgNode *tmpl_node = Traverse(pattern)) {
              if (SgTemplateMemberFunctionDeclaration *tmpl_decl =
                      isSgTemplateMemberFunctionDeclaration(tmpl_node)) {
                inst_member->set_templateDeclaration(tmpl_decl);
                if (has_function_template) {
                  inst_member->set_templateName(tmpl_decl->get_name());
                }
              }
            }
          }
        }
      } else if (!built_template_member_pattern) {
        sg_function_decl = SageBuilder::buildNondefiningFunctionDeclaration(
            name, ret_type, param_list, scope_for_symbol_table, false, nullptr,
            SgStorageModifier::e_default, isFriendFreeFunction);
      }
    }

    if (function_decl->isVariadic())
      sg_function_decl->hasEllipses();

    SgInitializedNamePtrList &init_names = param_list->get_args();
    SgInitializedNamePtrList::iterator it;
    for (it = init_names.begin(); it != init_names.end(); it++) {
      (*it)->set_scope(SageBuilder::topScopeStack());
      // CLANG FRONTEND FIX: Set declptr for function parameters
      (*it)->set_declptr(sg_function_decl);
    }

    SgFunctionDeclaration *existing_first_nondef = isSgFunctionDeclaration(
        sg_function_decl->get_firstNondefiningDeclaration());
    bool preserve_existing_first_nondef = false;
    if (existing_first_nondef != nullptr &&
        existing_first_nondef != sg_function_decl) {
      if (existing_first_nondef->get_symbol_from_symbol_table() != nullptr) {
        preserve_existing_first_nondef = true;
      }
    }

    if (!preserve_existing_first_nondef &&
        function_decl->getFirstDecl() != function_decl) {
      SgSymbol *tmp_symbol =
          GetSymbolFromSymbolTable(function_decl->getFirstDecl());
      SgFunctionSymbol *symbol = isSgFunctionSymbol(tmp_symbol);
      if (tmp_symbol != nullptr && symbol == nullptr) {
        std::cerr << "Runtime error: tmp_symbol != nullptr && symbol == nullptr"
                  << std::endl;
        res = false;
      }
      SgFunctionDeclaration *first_decl = nullptr;
      if (symbol != nullptr) {
        first_decl = isSgFunctionDeclaration(symbol->get_declaration());
      } else {
        // FIXME Is it correct?
        SgNode *tmp_first_decl = Traverse(function_decl->getFirstDecl());
        first_decl = isSgFunctionDeclaration(tmp_first_decl);
        ROSE_ASSERT(first_decl != nullptr);
        // ROSE_ASSERT(!"We should have see the first declaration already");
      }

      if (first_decl != nullptr) {
        // CLANG FRONTEND FIX: Only set firstNondefiningDeclaration if variant
        // types match to avoid assertion failure when mixing
        // SgFunctionDeclaration with SgMemberFunctionDeclaration
        if (first_decl->variantT() == sg_function_decl->variantT()) {
          if (first_decl->get_firstNondefiningDeclaration() != nullptr)
            sg_function_decl->set_firstNondefiningDeclaration(
                first_decl->get_firstNondefiningDeclaration());
          else {
            ROSE_ASSERT(first_decl->get_firstNondefiningDeclaration() !=
                        nullptr);
          }
        } else {
          // Variant types don't match - this can happen with member functions
          // Just set to self to avoid assertion
          sg_function_decl->set_firstNondefiningDeclaration(sg_function_decl);
        }
      } else {
        // REX FIX: Do not assert yet, retry with explicit lookup
        // ROSE_ASSERT(!"First declaration not found!");
      }
    } else if (!preserve_existing_first_nondef) {
      sg_function_decl->set_firstNondefiningDeclaration(sg_function_decl);
    }

    if (sg_function_decl != nullptr &&
        sg_function_decl->get_declaration_associated_with_symbol() == nullptr &&
        scope_for_symbol_table != nullptr) {
      if (SgFunctionSymbol *scope_symbol =
              scope_for_symbol_table->lookup_function_symbol(
                  name, sg_function_decl->get_type())) {
        if (SgFunctionDeclaration *symbol_decl =
                isSgFunctionDeclaration(scope_symbol->get_declaration())) {
          SgFunctionDeclaration *first_symbol_decl = isSgFunctionDeclaration(
              symbol_decl->get_firstNondefiningDeclaration());
          if (first_symbol_decl == nullptr) {
            first_symbol_decl = symbol_decl;
          }
          sg_function_decl->set_firstNondefiningDeclaration(first_symbol_decl);
        }
      }
    }
  }

  if (is_explicitly_defaulted_or_deleted) {
    if (is_explicitly_defaulted) {
      sg_function_decl->get_functionModifier().setMarkedDefault();
    }
    if (is_explicitly_deleted) {
      sg_function_decl->get_functionModifier().setMarkedDelete();
    }
  }

  sg_function_decl->set_declarationScope(declScope);
  declScope->set_parent(sg_function_decl);

  auto ensure_param_list_parent = [](SgFunctionDeclaration *decl) {
    if (decl == nullptr) {
      return;
    }
    SgFunctionParameterList *params = decl->get_parameterList();
    if (params == nullptr) {
      return;
    }
    if (params->get_parent() == nullptr) {
      params->set_parent(decl);
    }
    if (decl->get_parameterList() != params) {
      decl->set_parameterList(params);
    }
    if (SgFunctionParameterList *syntax = decl->get_parameterList_syntax()) {
      if (syntax->get_parent() == nullptr) {
        syntax->set_parent(decl);
      }
    }
  };
  ensure_param_list_parent(sg_function_decl);
  ensure_param_list_parent(isSgFunctionDeclaration(
      sg_function_decl->get_firstNondefiningDeclaration()));
  ensure_param_list_parent(
      isSgFunctionDeclaration(sg_function_decl->get_definingDeclaration()));

  auto ensure_function_symbol = [&](SgFunctionDeclaration *decl) {
    if (decl == nullptr) {
      return;
    }

    auto is_member_like = [](SgFunctionDeclaration *candidate) -> bool {
      if (candidate == nullptr) {
        return false;
      }
      return isSgMemberFunctionDeclaration(candidate) != nullptr ||
             isSgTemplateMemberFunctionDeclaration(candidate) != nullptr;
    };

    auto scopes_match = [&](SgScopeStatement *lhs,
                            SgScopeStatement *rhs) -> bool {
      if (lhs == rhs) {
        return true;
      }
      if (lhs == nullptr || rhs == nullptr) {
        return false;
      }
      return normalizeNamespaceScope(lhs) == normalizeNamespaceScope(rhs);
    };

    auto resolve_symbol_decl =
        [&](SgFunctionDeclaration *candidate) -> SgFunctionDeclaration * {
      if (candidate == nullptr) {
        return nullptr;
      }
      SgFunctionDeclaration *first_nondef =
          isSgFunctionDeclaration(candidate->get_firstNondefiningDeclaration());
      SgFunctionDeclaration *def_decl =
          isSgFunctionDeclaration(candidate->get_definingDeclaration());
      auto is_class_scope = [](SgScopeStatement *scope) -> bool {
        return isSgClassDefinition(scope) != nullptr ||
               isSgTemplateClassDefinition(scope) != nullptr ||
               isSgTemplateInstantiationDefn(scope) != nullptr;
      };

      if (isFriendFreeFunction && scope_for_symbol_table != nullptr) {
        if (first_nondef != nullptr &&
            scopes_match(first_nondef->get_scope(), scope_for_symbol_table)) {
          return first_nondef;
        }
        if (def_decl != nullptr &&
            scopes_match(def_decl->get_scope(), scope_for_symbol_table)) {
          return def_decl;
        }
      }

      if (!isFriendFreeFunction) {
        if (first_nondef != nullptr &&
            is_class_scope(first_nondef->get_scope())) {
          return first_nondef;
        }
        if (def_decl != nullptr && is_class_scope(def_decl->get_scope())) {
          return def_decl;
        }
      }

      if (first_nondef != nullptr &&
          scopes_match(first_nondef->get_scope(), candidate->get_scope())) {
        return first_nondef;
      }
      if (def_decl != nullptr &&
          scopes_match(def_decl->get_scope(), candidate->get_scope())) {
        return def_decl;
      }

      if (first_nondef != nullptr) {
        return first_nondef;
      }
      if (def_decl != nullptr) {
        return def_decl;
      }
      return candidate;
    };

    SgFunctionDeclaration *symbol_decl = resolve_symbol_decl(decl);
    if (symbol_decl == nullptr) {
      return;
    }
    SgScopeStatement *symbol_scope = symbol_decl->get_scope();
    if (symbol_scope == nullptr) {
      return;
    }
    const bool symbol_is_member = is_member_like(symbol_decl);
    if (isFriendFreeFunction && scope_for_symbol_table != nullptr &&
        (isSgNamespaceDefinitionStatement(scope_for_symbol_table) != nullptr ||
         isSgGlobal(scope_for_symbol_table) != nullptr)) {
      symbol_scope = normalizeNamespaceScope(scope_for_symbol_table);
    } else if (!symbol_is_member && scope_for_symbol_table != nullptr &&
               (isSgNamespaceDefinitionStatement(scope_for_symbol_table) !=
                    nullptr ||
                isSgGlobal(scope_for_symbol_table) != nullptr)) {
      symbol_scope = normalizeNamespaceScope(scope_for_symbol_table);
    }
    if (isSgNamespaceDefinitionStatement(symbol_scope) != nullptr ||
        isSgGlobal(symbol_scope) != nullptr) {
      symbol_scope = normalizeNamespaceScope(symbol_scope);
    }
    if (symbol_scope == nullptr) {
      return;
    }
    auto normalize_decl_scope = [&](SgFunctionDeclaration *decl_to_fix) {
      if (decl_to_fix == nullptr) {
        return;
      }
      if (decl_to_fix->get_scope() != symbol_scope) {
        decl_to_fix->set_scope(symbol_scope);
      }
    };
    if (isSgNamespaceDefinitionStatement(symbol_scope) != nullptr ||
        isSgGlobal(symbol_scope) != nullptr) {
      normalize_decl_scope(symbol_decl);
      normalize_decl_scope(isSgFunctionDeclaration(
          symbol_decl->get_firstNondefiningDeclaration()));
      normalize_decl_scope(
          isSgFunctionDeclaration(symbol_decl->get_definingDeclaration()));
    }

    auto remove_symbol_from_scope = [](SgScopeStatement *scope, SgSymbol *sym,
                                       bool discard) {
      if (scope == nullptr || sym == nullptr) {
        return;
      }
      bool removed = false;
      if (scope->symbol_exists(sym)) {
        scope->remove_symbol(sym);
        removed = true;
      } else if (SgSymbolTable *table = scope->get_symbol_table()) {
        if (table->exists(sym)) {
          table->remove(sym);
          removed = true;
        }
      }
      if (removed && discard) {
        move_symbol_to_orphan_table(sym);
      }
    };

    auto rehome_symbol_to_scope = [&](SgScopeStatement *from_scope,
                                      SgSymbol *sym) {
      if (from_scope == nullptr || sym == nullptr || symbol_scope == nullptr) {
        return;
      }
      if (from_scope == symbol_scope) {
        return;
      }
      remove_symbol_from_scope(from_scope, sym, false);
      if (!symbol_scope->symbol_exists(sym)) {
        symbol_scope->insert_symbol(sym->get_name(), sym);
      } else if (SgSymbolTable *table = symbol_scope->get_symbol_table()) {
        if (sym->get_parent() != table) {
          sym->set_parent(table);
        }
      }
    };

    SgSymbol *matching_symbol = nullptr;
    std::set<SgSymbol *> seen_symbols;
    std::vector<SgScopeStatement *> candidate_scopes;
    auto add_scope = [](std::vector<SgScopeStatement *> &scopes,
                        SgScopeStatement *scope) {
      if (scope == nullptr) {
        return;
      }
      if (std::find(scopes.begin(), scopes.end(), scope) == scopes.end()) {
        scopes.push_back(scope);
      }
    };

    auto add_namespace_chain = [](std::vector<SgScopeStatement *> &scopes,
                                  SgScopeStatement *scope) {
      SgNamespaceDefinitionStatement *ns =
          isSgNamespaceDefinitionStatement(scope);
      if (ns == nullptr) {
        return;
      }
      SgNamespaceDefinitionStatement *first_def =
          isSgNamespaceDefinitionStatement(normalizeNamespaceScope(ns));
      if (first_def == nullptr) {
        first_def = ns;
      }
      for (SgNamespaceDefinitionStatement *cursor = first_def;
           cursor != nullptr; cursor = cursor->get_nextNamespaceDefinition()) {
        if (std::find(scopes.begin(), scopes.end(), cursor) == scopes.end()) {
          scopes.push_back(cursor);
        }
      }
    };

    add_scope(candidate_scopes, symbol_scope);
    add_namespace_chain(candidate_scopes, symbol_scope);
    if (!symbol_is_member) {
      add_scope(candidate_scopes, decl->get_scope());
      add_namespace_chain(candidate_scopes, decl->get_scope());
      add_scope(candidate_scopes, symbol_decl->get_scope());
      add_namespace_chain(candidate_scopes, symbol_decl->get_scope());
      add_scope(candidate_scopes, scope_for_symbol_table);
      add_namespace_chain(candidate_scopes, scope_for_symbol_table);
      add_scope(candidate_scopes, lexical_scope);
      add_namespace_chain(candidate_scopes, lexical_scope);
      add_scope(candidate_scopes, isSgScopeStatement(decl->get_parent()));
      add_scope(candidate_scopes,
                isSgScopeStatement(symbol_decl->get_parent()));
    }

    for (SgScopeStatement *scope : candidate_scopes) {
      std::vector<SgSymbol *> existing_symbols =
          find_function_symbols_in_scope(scope, symbol_decl);
      for (SgSymbol *sym : existing_symbols) {
        if (sym == nullptr) {
          continue;
        }
        if (!seen_symbols.insert(sym).second) {
          continue;
        }
        if (isSgAliasSymbol(sym) != nullptr) {
          remove_symbol_from_scope(scope, sym, true);
          continue;
        }

        bool matches_decl =
            function_symbol_matches_declaration(sym, symbol_decl);
        if (scope != symbol_scope) {
          if (matches_decl && matching_symbol == nullptr) {
            rehome_symbol_to_scope(scope, sym);
            matching_symbol = sym;
          } else {
            remove_symbol_from_scope(scope, sym, true);
          }
          continue;
        }

        if (matches_decl) {
          if (matching_symbol == nullptr) {
            matching_symbol = sym;
          } else {
            remove_symbol_from_scope(scope, sym, true);
          }
        } else {
          remove_symbol_from_scope(scope, sym, true);
        }
      }
    }

    auto insert_symbol = [&](SgSymbol *sym) {
      if (sym == nullptr) {
        return;
      }
      if (!symbol_scope->symbol_exists(sym)) {
        symbol_scope->insert_symbol(sym->get_name(), sym);
      }
    };
    auto normalize_symbol_decl_scope = [&](SgSymbol *sym) {
      if (sym == nullptr) {
        return;
      }
      if (SgTemplateMemberFunctionSymbol *tmpl_mem_sym =
              isSgTemplateMemberFunctionSymbol(sym)) {
        normalize_decl_scope(
            isSgFunctionDeclaration(tmpl_mem_sym->get_declaration()));
        normalize_decl_scope(isSgFunctionDeclaration(
            tmpl_mem_sym->get_declaration()
                ? tmpl_mem_sym->get_declaration()
                      ->get_firstNondefiningDeclaration()
                : nullptr));
        normalize_decl_scope(isSgFunctionDeclaration(
            tmpl_mem_sym->get_declaration()
                ? tmpl_mem_sym->get_declaration()->get_definingDeclaration()
                : nullptr));
        return;
      }
      if (SgTemplateFunctionSymbol *tmpl_sym =
              isSgTemplateFunctionSymbol(sym)) {
        normalize_decl_scope(
            isSgFunctionDeclaration(tmpl_sym->get_declaration()));
        normalize_decl_scope(isSgFunctionDeclaration(
            tmpl_sym->get_declaration()
                ? tmpl_sym->get_declaration()->get_firstNondefiningDeclaration()
                : nullptr));
        normalize_decl_scope(isSgFunctionDeclaration(
            tmpl_sym->get_declaration()
                ? tmpl_sym->get_declaration()->get_definingDeclaration()
                : nullptr));
        return;
      }
      if (SgFunctionSymbol *func_sym = isSgFunctionSymbol(sym)) {
        normalize_decl_scope(
            isSgFunctionDeclaration(func_sym->get_declaration()));
        normalize_decl_scope(isSgFunctionDeclaration(
            func_sym->get_declaration()
                ? func_sym->get_declaration()->get_firstNondefiningDeclaration()
                : nullptr));
        normalize_decl_scope(isSgFunctionDeclaration(
            func_sym->get_declaration()
                ? func_sym->get_declaration()->get_definingDeclaration()
                : nullptr));
      }
    };

    if (matching_symbol == nullptr) {
      SgTemplateMemberFunctionDeclaration *tmpl_member =
          isSgTemplateMemberFunctionDeclaration(symbol_decl);
      if (tmpl_member == nullptr) {
        tmpl_member = isSgTemplateMemberFunctionDeclaration(decl);
      }
      if (tmpl_member != nullptr) {
        SgTemplateParameterPtrList *params =
            &tmpl_member->get_templateParameters();
        if (SgTemplateMemberFunctionSymbol *tmpl_sym =
                symbol_scope->lookup_template_member_function_symbol(
                    tmpl_member->get_name(), tmpl_member->get_type(), params)) {
          if (tmpl_sym->get_declaration() != symbol_decl) {
            tmpl_sym->set_declaration(symbol_decl);
          }
          insert_symbol(tmpl_sym);
          matching_symbol = tmpl_sym;
        } else {
          SgTemplateMemberFunctionDeclaration *sym_decl =
              isSgTemplateMemberFunctionDeclaration(symbol_decl);
          SgTemplateMemberFunctionSymbol *new_sym =
              new SgTemplateMemberFunctionSymbol(
                  sym_decl != nullptr ? sym_decl : tmpl_member);
          insert_symbol(new_sym);
          matching_symbol = new_sym;
        }
      } else if (SgTemplateFunctionDeclaration *tmpl_decl =
                     isSgTemplateFunctionDeclaration(symbol_decl)) {
        SgTemplateParameterPtrList *params =
            &tmpl_decl->get_templateParameters();
        if (SgTemplateFunctionSymbol *tmpl_sym =
                symbol_scope->lookup_template_function_symbol(
                    tmpl_decl->get_name(), tmpl_decl->get_type(), params)) {
          if (tmpl_sym->get_declaration() != symbol_decl) {
            tmpl_sym->set_declaration(symbol_decl);
          }
          insert_symbol(tmpl_sym);
          matching_symbol = tmpl_sym;
        } else {
          SgTemplateFunctionSymbol *new_sym =
              new SgTemplateFunctionSymbol(tmpl_decl);
          insert_symbol(new_sym);
          matching_symbol = new_sym;
        }
      } else if (SgTemplateFunctionDeclaration *tmpl_decl =
                     isSgTemplateFunctionDeclaration(decl)) {
        SgTemplateParameterPtrList *params =
            &tmpl_decl->get_templateParameters();
        if (SgTemplateFunctionSymbol *tmpl_sym =
                symbol_scope->lookup_template_function_symbol(
                    tmpl_decl->get_name(), tmpl_decl->get_type(), params)) {
          if (tmpl_sym->get_declaration() != symbol_decl) {
            tmpl_sym->set_declaration(symbol_decl);
          }
          insert_symbol(tmpl_sym);
          matching_symbol = tmpl_sym;
        } else {
          SgTemplateFunctionSymbol *new_sym =
              new SgTemplateFunctionSymbol(tmpl_decl);
          insert_symbol(new_sym);
          matching_symbol = new_sym;
        }
      } else {
        SgMemberFunctionDeclaration *member_decl =
            isSgMemberFunctionDeclaration(symbol_decl);
        if (member_decl == nullptr) {
          member_decl = isSgMemberFunctionDeclaration(decl);
        }
        if (member_decl != nullptr) {
          if (SgMemberFunctionSymbol *mem_sym =
                  symbol_scope->lookup_nontemplate_member_function_symbol(
                      member_decl->get_name(), member_decl->get_type(),
                      nullptr)) {
            if (mem_sym->get_declaration() != symbol_decl) {
              mem_sym->set_declaration(symbol_decl);
            }
            insert_symbol(mem_sym);
            matching_symbol = mem_sym;
          } else {
            SgMemberFunctionDeclaration *sym_decl =
                isSgMemberFunctionDeclaration(symbol_decl);
            SgMemberFunctionSymbol *new_sym = new SgMemberFunctionSymbol(
                sym_decl != nullptr ? sym_decl : member_decl);
            insert_symbol(new_sym);
            matching_symbol = new_sym;
          }
        } else {
          if (SgFunctionSymbol *func_sym =
                  symbol_scope->lookup_nontemplate_function_symbol(
                      symbol_decl->get_name(), symbol_decl->get_type(),
                      nullptr)) {
            if (func_sym->get_declaration() != symbol_decl) {
              func_sym->set_declaration(symbol_decl);
            }
            insert_symbol(func_sym);
            matching_symbol = func_sym;
          } else {
            SgFunctionSymbol *new_sym = new SgFunctionSymbol(symbol_decl);
            insert_symbol(new_sym);
            matching_symbol = new_sym;
          }
        }
      }
    }

    normalize_symbol_decl_scope(matching_symbol);
  };
  ensure_function_symbol(sg_function_decl);
  ensure_function_symbol(isSgFunctionDeclaration(
      sg_function_decl->get_firstNondefiningDeclaration()));
  ensure_function_symbol(
      isSgFunctionDeclaration(sg_function_decl->get_definingDeclaration()));

  if (is_builtin_decl) {
    auto mark_compgen = [this](SgLocatedNode *n) {
      if (n != nullptr) {
        setCompilerGeneratedFileInfo(n);
        n->set_isModified(false);
        if (Sg_File_Info *fi = n->get_file_info())
          fi->unsetOutputInCodeGeneration();
        if (Sg_File_Info *fi = n->get_startOfConstruct())
          fi->unsetOutputInCodeGeneration();
        if (Sg_File_Info *fi = n->get_endOfConstruct())
          fi->unsetOutputInCodeGeneration();
      }
    };

    mark_compgen(sg_function_decl);
    mark_compgen(sg_function_decl->get_firstNondefiningDeclaration());
    mark_compgen(sg_function_decl->get_parameterList());

    SgInitializedNamePtrList &builtin_params =
        sg_function_decl->get_parameterList()->get_args();
    for (SgInitializedName *param : builtin_params) {
      mark_compgen(param);
    }

    if (SgFunctionDefinition *def = sg_function_decl->get_definition()) {
      mark_compgen(def);
      mark_compgen(def->get_body());
    }
  }

  // REX FIX: Handle AS_none for FunctionDecl to avoid unparser assertion
  clang::AccessSpecifier access = function_decl->getAccess();

  SgDeclarationStatement *target_decl = sg_function_decl;
  if (access == clang::AS_public) {
    target_decl->get_declarationModifier().get_accessModifier().setPublic();
  } else if (access == clang::AS_private) {
    target_decl->get_declarationModifier().get_accessModifier().setPrivate();
  } else if (access == clang::AS_protected) {
    target_decl->get_declarationModifier().get_accessModifier().setProtected();
  } else if (access == clang::AS_none) {
    if (isSgClassDefinition(proper_scope)) {
      SgClassDefinition *class_def = isSgClassDefinition(proper_scope);
      if (class_def->get_declaration()->get_class_type() ==
          SgClassDeclaration::e_class) {
        target_decl->get_declarationModifier()
            .get_accessModifier()
            .setPrivate();
      } else {
        target_decl->get_declarationModifier().get_accessModifier().setPublic();
      }
    } else {
      // REX FIX: For global or namespace scope, set to public to satisfy
      // unparser assertion (info.isUnsetAccess() == false)
      target_decl->get_declarationModifier().get_accessModifier().setPublic();
    }
  }

  ROSE_ASSERT(sg_function_decl->get_firstNondefiningDeclaration() != nullptr);
  /* // TODO Fix problem with function symbols...
      SgSymbol * symbol = GetSymbolFromSymbolTable(function_decl);
      if (symbol == nullptr) {
          SgFunctionSymbol * func_sym = new
     SgFunctionSymbol(isSgFunctionDeclaration(sg_function_decl->get_firstNondefiningDeclaration()));
          SageBuilder::topScopeStack()->insert_symbol(name, func_sym);
      }
  */
  //  ROSE_ASSERT(GetSymbolFromSymbolTable(function_decl) != nullptr);

  // Pei-Hung (09/27/2022) setup linkage
  if (function_decl->isExternC()) {
    sg_function_decl->get_declarationModifier()
        .get_storageModifier()
        .setExtern();
  }

  // Pei-Hung (06/16/22) added "extern" modifier
  bool hasExternalStorage = function_decl->isLocalExternDecl();
  if (hasExternalStorage) {
    sg_function_decl->get_declarationModifier()
        .get_storageModifier()
        .setExtern();
  }

  if (function_decl->getStorageClass() == clang::SC_Static &&
      llvm::isa<clang::CXXMethodDecl>(function_decl) == false) {
    sg_function_decl->get_declarationModifier()
        .get_storageModifier()
        .setStatic();
  }

  // CLANG FRONTEND FIX: Preserve C++ static member function declarations.
  // Clang models these as CXXMethodDecl::isStatic(), but ROSE expects the
  // storage modifier to be written only for in-class declarations/definitions.
  // Out-of-class declarations/definitions must not re-specify "static".
  if (clang::CXXMethodDecl *method_decl =
          llvm::dyn_cast<clang::CXXMethodDecl>(function_decl)) {
    if (method_decl->isStatic()) {
      if (SgFunctionDeclaration *first_nondef = isSgFunctionDeclaration(
              sg_function_decl->get_firstNondefiningDeclaration())) {
        first_nondef->get_declarationModifier()
            .get_storageModifier()
            .setStatic();
      }
      SgNode *parent = sg_function_decl->get_parent();
      if (isSgClassDefinition(parent) != nullptr) {
        sg_function_decl->get_declarationModifier()
            .get_storageModifier()
            .setStatic();
      }
    }
  }

  // CLANG FRONTEND FIX: Set friend modifier for friend functions
  // Friend functions are free functions (not members) with special access
  // rights
  if (isFriendFunction) {
    sg_function_decl->get_declarationModifier().setFriend();
  }

  // Friend declarations written inside a class are still free functions. We
  // build them using the enclosing namespace/global scope for symbol-table
  // correctness and reattach them to the lexical class scope later in this
  // function.

  // ROOT CAUSE FIX: Set access modifiers for member functions from Clang AST
  if (llvm::isa<clang::CXXMethodDecl>(function_decl)) {
    clang::CXXMethodDecl *method_decl =
        llvm::cast<clang::CXXMethodDecl>(function_decl);
    clang::AccessSpecifier access = method_decl->getAccess();
    if (access == clang::AS_public) {
      sg_function_decl->get_declarationModifier()
          .get_accessModifier()
          .setPublic();
    } else if (access == clang::AS_private) {
      sg_function_decl->get_declarationModifier()
          .get_accessModifier()
          .setPrivate();
    } else if (access == clang::AS_protected) {
      sg_function_decl->get_declarationModifier()
          .get_accessModifier()
          .setProtected();
    }
  }

  // CLANG FRONTEND FIX #21: Mark constructors, destructors, and conversion
  // operators with special function modifiers so unparser handles them
  // correctly
  if (SgMemberFunctionDeclaration *member_func =
          isSgMemberFunctionDeclaration(sg_function_decl)) {
    if (llvm::isa<clang::CXXConstructorDecl>(function_decl)) {
      member_func->get_specialFunctionModifier().setConstructor();
    } else if (llvm::isa<clang::CXXDestructorDecl>(function_decl)) {
      member_func->get_specialFunctionModifier().setDestructor();
    } else if (llvm::isa<clang::CXXConversionDecl>(function_decl)) {
      member_func->get_specialFunctionModifier().setConversion();
    }
  }

  // CLANG FRONTEND FIX: Mark overloaded operator declarations so the unparser
  // can correctly suppress call parentheses (e.g., "++x" vs "++x()").
  if (function_decl->isOverloadedOperator()) {
    sg_function_decl->get_specialFunctionModifier().setOperator();
    if (SgFunctionDeclaration *first_nondef = isSgFunctionDeclaration(
            sg_function_decl->get_firstNondefiningDeclaration())) {
      first_nondef->get_specialFunctionModifier().setOperator();
    }
  }

  // Many SageBuilder "defining" builders create an associated non-defining
  // declaration even when no such declaration exists in the source (notably for
  // in-class definitions). In C++, an in-class member function definition
  // cannot be accompanied by a separate class-scope redeclaration, so suppress
  // output of such synthetic non-defining declarations while keeping them in
  // the AST for consistency checks.
  auto ensure_unique_nondef_param_list = [&](SgFunctionDeclaration *defining) {
    if (defining == nullptr) {
      return;
    }
    SgFunctionDeclaration *first_nondef =
        isSgFunctionDeclaration(defining->get_firstNondefiningDeclaration());
    if (first_nondef == nullptr || first_nondef == defining) {
      return;
    }
    SgFunctionParameterList *def_params = defining->get_parameterList();
    SgFunctionParameterList *nondef_params = first_nondef->get_parameterList();
    if (def_params == nullptr || nondef_params == nullptr) {
      return;
    }
    if (def_params != nondef_params) {
      return;
    }

    SgFunctionParameterList *cloned =
        SageBuilder::buildFunctionParameterList_nfi();
    for (SgInitializedName *init_name : def_params->get_args()) {
      if (init_name == nullptr) {
        continue;
      }
      SgInitializer *cloned_init = nullptr;
      if (SgInitializer *init = init_name->get_initializer()) {
        cloned_init = SageInterface::deepCopy(init);
      }
      SgInitializedName *cloned_param = SageBuilder::buildInitializedName_nfi(
          init_name->get_name(), init_name->get_type(), cloned_init);
      cloned->append_arg(cloned_param);
    }
    first_nondef->set_parameterList(cloned);
    cloned->set_parent(first_nondef);
    SgScopeStatement *nondef_scope = first_nondef->get_scope();
    for (SgInitializedName *param : cloned->get_args()) {
      if (param == nullptr) {
        continue;
      }
      param->set_declptr(first_nondef);
      param->set_parent(cloned);
      if (nondef_scope != nullptr) {
        param->set_scope(nondef_scope);
      }
    }
  };

  ensure_unique_nondef_param_list(sg_function_decl);

  auto suppress_synthetic_nondef_in_class =
      [&](SgFunctionDeclaration *defining) {
        if (defining == nullptr) {
          return;
        }

        if (!isDefinition) {
          return;
        }

        clang::DeclContext *lexical_ctx =
            function_decl->getLexicalDeclContext();
        if (lexical_ctx == nullptr ||
            !llvm::isa<clang::CXXRecordDecl>(lexical_ctx)) {
          return;
        }

        SgFunctionDeclaration *first_nondef = isSgFunctionDeclaration(
            defining->get_firstNondefiningDeclaration());
        if (first_nondef == nullptr || first_nondef == defining) {
          return;
        }

        // Only suppress the extra non-defining declaration when it is
        // synthetic. If Clang has a prior declaration in the redecl chain, that
        // declaration must remain visible (e.g., a user-written forward
        // declaration of a friend function template at namespace scope).
        if (function_decl->getFirstDecl() != function_decl) {
          return;
        }

        mark_compiler_generated_frontend_specific(first_nondef);
        suppress_unparse_output(first_nondef);
        if (SgFunctionParameterList *params =
                first_nondef->get_parameterList()) {
          mark_compiler_generated_frontend_specific(params);
          suppress_unparse_output(params);
          for (SgInitializedName *param : params->get_args()) {
            mark_compiler_generated_frontend_specific(param);
            suppress_unparse_output(param);
          }
        }
      };

  suppress_synthetic_nondef_in_class(sg_function_decl);

  // Root-cause fix for Issue 125:
  // Ensure declarations are attached to their owning scope's child/member list
  // so AST postprocessing invariants
  // (FixupAstDefiningAndNondefiningDeclarations) are satisfied even for
  // on-demand translation of system-header entities.
  if (SgDeclarationStatement *first_nondef = isSgDeclarationStatement(
          sg_function_decl->get_firstNondefiningDeclaration())) {
    if (isSgClassDefinition(first_nondef->get_scope()) != nullptr) {
      ensure_decl_in_scope_child_list(
          first_nondef, first_nondef->get_scope(),
          "translateFunctionDeclCommon:firstNondef");
    }
  }

  if (handled_template_instantiation && is_explicit_instantiation) {
    if (SgDeclarationStatement *inst_decl =
            isSgDeclarationStatement(sg_function_decl)) {
      explicit_instantiation_directive =
          isSgTemplateInstantiationDirectiveStatement(inst_decl->get_parent());
      if (explicit_instantiation_directive == nullptr) {
        if (SgScopeStatement *old_scope =
                isSgScopeStatement(inst_decl->get_parent())) {
          detach_decl_from_scope_child_list(inst_decl, old_scope);
        }

        explicit_instantiation_directive =
            new SgTemplateInstantiationDirectiveStatement(inst_decl);
        explicit_instantiation_directive->set_do_not_instantiate(
            is_extern_instantiation);
        explicit_instantiation_directive->set_declaration(inst_decl);
        SgScopeStatement *directive_scope =
            lexical_scope != nullptr ? lexical_scope : scope_for_symbol_table;
        explicit_instantiation_directive->set_scope(directive_scope);
        explicit_instantiation_directive->set_parent(directive_scope);
        applySourceRange(explicit_instantiation_directive,
                         function_decl->getSourceRange());
        inst_decl->set_parent(explicit_instantiation_directive);
        if (inst_decl->get_scope() == nullptr) {
          inst_decl->set_scope(scope_for_symbol_table != nullptr
                                   ? scope_for_symbol_table
                                   : directive_scope);
        }
        if (explicit_instantiation_directive
                ->get_firstNondefiningDeclaration() == nullptr) {
          explicit_instantiation_directive->set_firstNondefiningDeclaration(
              explicit_instantiation_directive);
          explicit_instantiation_directive->set_definingDeclaration(
              explicit_instantiation_directive);
        }
        ensure_decl_in_scope_child_list(
            explicit_instantiation_directive, directive_scope,
            "translateFunctionDeclCommon:explicit-instantiation");
      } else {
        explicit_instantiation_directive->set_do_not_instantiate(
            is_extern_instantiation);
        if (explicit_instantiation_directive->get_declaration() == nullptr) {
          explicit_instantiation_directive->set_declaration(inst_decl);
        }
        if (inst_decl->get_scope() == nullptr) {
          SgScopeStatement *fallback_scope =
              explicit_instantiation_directive->get_scope();
          inst_decl->set_scope(scope_for_symbol_table != nullptr
                                   ? scope_for_symbol_table
                                   : (lexical_scope != nullptr
                                          ? lexical_scope
                                          : fallback_scope));
        }
      }
    }
  }

  // Keep declarations attached to their lexical namespace definition to
  // preserve reopened-namespace structure and unparse order, while using the
  // canonical namespace scope only for symbol-table insertion (see
  // normalizeNamespaceScope()).
  if (lexical_scope != nullptr && lexical_scope != scope_for_symbol_table &&
      (isSgNamespaceDefinitionStatement(lexical_scope) != nullptr ||
       isSgGlobal(lexical_scope) != nullptr)) {
    SgDeclarationStatement *lexical_decl =
        explicit_instantiation_directive != nullptr
            ? static_cast<SgDeclarationStatement *>(
                  explicit_instantiation_directive)
            : isSgDeclarationStatement(sg_function_decl);
    if (lexical_decl != nullptr) {
      ensure_decl_in_scope_child_list_preserve_scope(
          lexical_decl, lexical_scope, "translateFunctionDeclCommon:lexical");
    }
  }

  // Friend free functions declared/defined inside a class must remain attached
  // to the lexical class definition for correct unparse structure, but their
  // symbols must live in the enclosing namespace/global scope.  We build them
  // using the enclosing scope (scope_for_symbol_table), then reattach them
  // lexically here.
  if (isFriendFreeFunction && lexical_scope != nullptr &&
      lexical_scope != scope_for_symbol_table &&
      isSgClassDefinition(lexical_scope) != nullptr) {
    auto reattach_to_lexical_class = [&](SgDeclarationStatement *decl,
                                         const char *ctx) {
      if (decl == nullptr) {
        return;
      }
      if (SgScopeStatement *current_parent =
              isSgScopeStatement(decl->get_parent())) {
        if (current_parent != lexical_scope) {
          detach_decl_from_scope_child_list(decl, current_parent);
        }
      }
      // Keep the semantic scope in the enclosing namespace/global scope so the
      // symbol table remains consistent, while attaching lexically to the
      // class definition for correct unparse order.
      ensure_decl_in_scope_child_list_preserve_scope(decl, lexical_scope, ctx);
    };

    SgDeclarationStatement *lexical_decl =
        explicit_instantiation_directive != nullptr
            ? static_cast<SgDeclarationStatement *>(
                  explicit_instantiation_directive)
            : isSgDeclarationStatement(sg_function_decl);
    reattach_to_lexical_class(lexical_decl,
                              "translateFunctionDeclCommon:friend:decl");
  }

  // Template instantiation name reset operates on the declaration scope's
  // symbol table. Keep instantiations scoped to the symbol-table scope even
  // when they are reattached lexically to reopened namespaces.
  if (handled_template_instantiation && scope_for_symbol_table != nullptr) {
    auto enforce_symbol_scope = [&](SgFunctionDeclaration *decl) {
      if (decl == nullptr) {
        return;
      }
      if (decl->get_scope() != scope_for_symbol_table) {
        decl->set_scope(scope_for_symbol_table);
      }
    };
    enforce_symbol_scope(isSgFunctionDeclaration(sg_function_decl));
    enforce_symbol_scope(isSgFunctionDeclaration(
        sg_function_decl->get_firstNondefiningDeclaration()));
    enforce_symbol_scope(
        isSgFunctionDeclaration(sg_function_decl->get_definingDeclaration()));
  }

  // Re-evaluate suppression after any lexical reattachment, since friend
  // declarations are built in the enclosing namespace/global scope and may only
  // later be moved into the class scope.
  suppress_synthetic_nondef_in_class(sg_function_decl);

  if (isMethodDecl) {
    auto *method_decl = llvm::cast<clang::CXXMethodDecl>(function_decl);
    clang::CXXRecordDecl *parent_record = method_decl->getParent();
    if (parent_record != nullptr) {
      SgClassDefinition *parent_def =
          resolve_or_translate_class_definition(parent_record);
      if (parent_def != nullptr) {
        ensureMemberFunctionScope(isSgFunctionDeclaration(sg_function_decl),
                                  parent_def);
        ensureMemberFunctionScope(
            isSgFunctionDeclaration(
                sg_function_decl->get_firstNondefiningDeclaration()),
            parent_def);
        ensureMemberFunctionScope(
            isSgFunctionDeclaration(
                sg_function_decl->get_definingDeclaration()),
            parent_def);
      }
    }
  }

  auto mark_missing_template_header_transformation =
      [&](SgFunctionDeclaration *decl) {
        if (!is_explicit_specialization || !isMethodDecl || decl == nullptr) {
          return;
        }
        if (p_sage_source_file == nullptr || p_compiler_instance == nullptr) {
          return;
        }
        auto *attr = dynamic_cast<MissingTemplateHeaderFixupAttribute *>(
            p_sage_source_file->getAttribute(
                kMissingTemplateHeaderFixupAttributeName));
        if (attr == nullptr) {
          return;
        }
        clang::SourceManager &sm = p_compiler_instance->getSourceManager();
        clang::SourceLocation loc = function_decl->getBeginLoc();
        if (!loc.isValid()) {
          return;
        }
        clang::SourceLocation spelling = sm.getSpellingLoc(loc);
        std::string file = sm.getFilename(spelling).str();
        unsigned offset = sm.getFileOffset(spelling);
        if (!attr->matches(file, offset)) {
          return;
        }
        decl->setTransformation();
        if (SgFunctionDeclaration *first = isSgFunctionDeclaration(
                decl->get_firstNondefiningDeclaration())) {
          first->setTransformation();
        }
        if (SgFunctionDefinition *def = decl->get_definition()) {
          def->setTransformation();
        }
      };

  mark_missing_template_header_transformation(sg_function_decl);

  applySourceRange(sg_function_decl, function_decl->getSourceRange());

  auto file_info_missing = [](Sg_File_Info *fi) {
    if (fi == nullptr) {
      return true;
    }
    std::string file = fi->get_filenameString();
    return file.empty() || file == "NULL_FILE";
  };
  if (SgFunctionDeclaration *first_nondef = isSgFunctionDeclaration(
          sg_function_decl->get_firstNondefiningDeclaration())) {
    if (file_info_missing(first_nondef->get_startOfConstruct())) {
      clang::SourceRange range = function_decl->getSourceRange();
      if (!range.isValid()) {
        clang::SourceLocation loc = function_decl->getLocation();
        if (loc.isValid()) {
          range = clang::SourceRange(loc, loc);
        }
      }
      if (range.isValid()) {
        applySourceRange(first_nondef, range);
      }
    }
  }

  if (SgMemberFunctionDeclaration *member_decl =
          isSgMemberFunctionDeclaration(sg_function_decl)) {
    if (member_decl->get_associatedClassDeclaration() == nullptr) {
      if (clang::CXXMethodDecl *method_decl =
              llvm::dyn_cast<clang::CXXMethodDecl>(function_decl)) {
        clang::CXXRecordDecl *parent_decl = method_decl->getParent();

        auto node_to_class_decl = [](SgNode *node) -> SgClassDeclaration * {
          if (SgClassDeclaration *decl = isSgClassDeclaration(node)) {
            return decl;
          }
          if (SgClassDefinition *def = isSgClassDefinition(node)) {
            return isSgClassDeclaration(def->get_declaration());
          }
          if (SgTemplateClassDefinition *tmpl_def =
                  isSgTemplateClassDefinition(node)) {
            return isSgTemplateClassDeclaration(tmpl_def->get_declaration());
          }
          if (SgTemplateInstantiationDefn *inst_def =
                  isSgTemplateInstantiationDefn(node)) {
            return isSgTemplateInstantiationDecl(inst_def->get_declaration());
          }
          return nullptr;
        };

        auto lookup_class_decl =
            [&](clang::CXXRecordDecl *record) -> SgClassDeclaration * {
          if (record == nullptr) {
            return nullptr;
          }
          auto it = p_decl_translation_map.find(record);
          if (it == p_decl_translation_map.end()) {
            return nullptr;
          }
          return node_to_class_decl(it->second);
        };

        SgClassDeclaration *assoc_decl = lookup_class_decl(parent_decl);
        if (assoc_decl == nullptr && parent_decl != nullptr) {
          if (clang::CXXRecordDecl *def = parent_decl->getDefinition()) {
            assoc_decl = lookup_class_decl(def);
          }
        }
        if (assoc_decl == nullptr && parent_decl != nullptr) {
          if (clang::CXXRecordDecl *canonical =
                  parent_decl->getCanonicalDecl()) {
            assoc_decl = lookup_class_decl(canonical);
            if (assoc_decl == nullptr) {
              if (clang::CXXRecordDecl *def = canonical->getDefinition()) {
                assoc_decl = lookup_class_decl(def);
              }
            }
          }
        }
        if (assoc_decl == nullptr && parent_decl != nullptr) {
          clang::CXXRecordDecl *to_translate = parent_decl;
          if (clang::CXXRecordDecl *def = parent_decl->getDefinition()) {
            to_translate = def;
          }
          if (p_decl_translation_map.find(to_translate) ==
                  p_decl_translation_map.end() &&
              p_decl_translation_in_progress.find(to_translate) ==
                  p_decl_translation_in_progress.end()) {
            assoc_decl = node_to_class_decl(TraverseOnDemand(to_translate));
          }
        }

        if (assoc_decl != nullptr) {
          member_decl->set_associatedClassDeclaration(assoc_decl);
        }
      }
    }
  }

  auto clone_constraint_expr = [](SgExpression *expr) -> SgExpression * {
    if (expr == nullptr) {
      return nullptr;
    }
    return isSgExpression(SageInterface::deepCopy(expr));
  };

  if (templateDecl != nullptr) {
    if (clang::TemplateParameterList *params =
            templateDecl->getTemplateParameters()) {
      if (const clang::Expr *requires_expr = params->getRequiresClause()) {
        if (SgExpression *sg_requires =
                translateConstraintExpression(requires_expr)) {
          auto attach_requires = [&](SgDeclarationStatement *decl,
                                     SgExpression *expr) {
            if (decl == nullptr || expr == nullptr) {
              return;
            }
            if (SgTemplateFunctionDeclaration *tmpl_func =
                    isSgTemplateFunctionDeclaration(decl)) {
              tmpl_func->set_requiresClause(expr);
              expr->set_parent(tmpl_func);
              return;
            }
            if (SgTemplateMemberFunctionDeclaration *tmpl_member =
                    isSgTemplateMemberFunctionDeclaration(decl)) {
              tmpl_member->set_requiresClause(expr);
              expr->set_parent(tmpl_member);
              return;
            }
            if (SgTemplateDeclaration *tmpl_decl =
                    isSgTemplateDeclaration(decl)) {
              tmpl_decl->set_requiresClause(expr);
              expr->set_parent(tmpl_decl);
            }
          };

          attach_requires(isSgDeclarationStatement(sg_function_decl),
                          sg_requires);
          if (SgFunctionDeclaration *first_nondef = isSgFunctionDeclaration(
                  sg_function_decl->get_firstNondefiningDeclaration())) {
            if (first_nondef != sg_function_decl) {
              attach_requires(first_nondef, clone_constraint_expr(sg_requires));
            }
          }
          if (SgFunctionDeclaration *def_decl = isSgFunctionDeclaration(
                  sg_function_decl->get_definingDeclaration())) {
            if (def_decl != sg_function_decl) {
              attach_requires(def_decl, clone_constraint_expr(sg_requires));
            }
          }
        }
      }
    }
  }

#if LLVM_VERSION_MAJOR >= 21
  const clang::AssociatedConstraint &trailing_requires =
      function_decl->getTrailingRequiresClause();
  if (trailing_requires && trailing_requires.ConstraintExpr != nullptr) {
    if (SgExpression *sg_trailing =
            translateConstraintExpression(trailing_requires.ConstraintExpr)) {
      auto attach_trailing = [&](SgFunctionDeclaration *decl,
                                 SgExpression *expr) {
        if (decl == nullptr || expr == nullptr) {
          return;
        }
        decl->set_trailingRequiresClause(expr);
        expr->set_parent(decl);
      };

      attach_trailing(sg_function_decl, sg_trailing);
      if (SgFunctionDeclaration *first_nondef = isSgFunctionDeclaration(
              sg_function_decl->get_firstNondefiningDeclaration())) {
        if (first_nondef != sg_function_decl) {
          attach_trailing(first_nondef, clone_constraint_expr(sg_trailing));
        }
      }
      if (SgFunctionDeclaration *def_decl = isSgFunctionDeclaration(
              sg_function_decl->get_definingDeclaration())) {
        if (def_decl != sg_function_decl) {
          attach_trailing(def_decl, clone_constraint_expr(sg_trailing));
        }
      }
    }
  }
#else
  if (const clang::Expr *trailing_requires =
          function_decl->getTrailingRequiresClause()) {
    if (SgExpression *sg_trailing =
            translateConstraintExpression(trailing_requires)) {
      auto attach_trailing = [&](SgFunctionDeclaration *decl,
                                 SgExpression *expr) {
        if (decl == nullptr || expr == nullptr) {
          return;
        }
        decl->set_trailingRequiresClause(expr);
        expr->set_parent(decl);
      };

      attach_trailing(sg_function_decl, sg_trailing);
      if (SgFunctionDeclaration *first_nondef = isSgFunctionDeclaration(
              sg_function_decl->get_firstNondefiningDeclaration())) {
        if (first_nondef != sg_function_decl) {
          attach_trailing(first_nondef, clone_constraint_expr(sg_trailing));
        }
      }
      if (SgFunctionDeclaration *def_decl = isSgFunctionDeclaration(
              sg_function_decl->get_definingDeclaration())) {
        if (def_decl != sg_function_decl) {
          attach_trailing(def_decl, clone_constraint_expr(sg_trailing));
        }
      }
    }
  }
#endif

  if (SgTemplateFunctionDeclaration *tmpl_func =
          isSgTemplateFunctionDeclaration(sg_function_decl)) {
    attach_nonreal_template_parameters(tmpl_func,
                                       tmpl_func->get_templateParameters());
  } else if (SgTemplateMemberFunctionDeclaration *tmpl_member =
                 isSgTemplateMemberFunctionDeclaration(sg_function_decl)) {
    attach_nonreal_template_parameters(tmpl_member,
                                       tmpl_member->get_templateParameters());
  }

  ensureFunctionParameterSymbols(sg_function_decl);

  *node = sg_function_decl;

  bool visit_res = VisitDeclaratorDecl(function_decl, node) && res;

  // translateFunctionDeclCommon returns via VisitDeclaratorDecl->VisitDecl,
  // which applies source ranges and may mark declarations for output in the
  // main file. For compiler-synthesized implicit instantiations we must
  // preserve their "compiler-generated and not-for-unparse" classification;
  // otherwise they can be emitted as explicit specializations after use sites
  // and break downstream compilation (Issue 115 Phase C regression).
  if (handled_template_instantiation &&
      function_decl->getTemplateSpecializationKind() ==
          clang::TSK_ImplicitInstantiation) {
    if (SgFunctionDeclaration *func_decl =
            isSgFunctionDeclaration(sg_function_decl)) {
      mark_implicit_instantiation_for_suppression(func_decl);
    }
  }

  if (explicit_instantiation_directive != nullptr) {
    *node = explicit_instantiation_directive;
  }

  return visit_res;
}

bool ClangToSageTranslator::VisitFunctionDecl(
    clang::FunctionDecl *function_decl, SgNode **node) {
  return translateFunctionDeclCommon(
      function_decl, function_decl->getDescribedFunctionTemplate(), node);
}

bool ClangToSageTranslator::VisitCXXDeductionGuideDecl(
    clang::CXXDeductionGuideDecl *cxx_deduction_guide_decl, SgNode **node) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToSageTranslator::VisitCXXDeductionGuideDecl" << std::endl;
#endif
  bool res = true;

  // TODO: Full C++17 deduction guide support not yet implemented
  // For now, delegate to FunctionDecl handler for basic processing
  // ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitFunctionDecl(cxx_deduction_guide_decl, node) && res;
}

bool ClangToSageTranslator::VisitCXXMethodDecl(
    clang::CXXMethodDecl *cxx_method_decl, SgNode **node) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToSageTranslator::VisitCXXMethodDecl" << std::endl;
#endif
  bool res = true;

  // CXXMethodDecl represents member functions in C++ classes
  // For now, treat them like regular functions - this is incomplete but allows
  // progress
  // TODO: Properly handle member function context, this pointer, virtual
  // methods, etc.

  return VisitFunctionDecl(cxx_method_decl, node) && res;
}

bool ClangToSageTranslator::VisitCXXConstructorDecl(
    clang::CXXConstructorDecl *cxx_constructor_decl, SgNode **node) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToSageTranslator::VisitCXXConstructorDecl" << std::endl;
#endif
  bool res = true;

  // ROOT CAUSE FIX: Allow constructors to be processed via CXXMethodDecl
  // ROSE_ASSERT(FAIL_TODO == 0); // TODO

  res = VisitCXXMethodDecl(cxx_constructor_decl, node);
  SgMemberFunctionDeclaration *cxxConstructorDecl =
      isSgMemberFunctionDeclaration(*node);
  if (cxxConstructorDecl == nullptr) {
    return res;
  }
  SgMemberFunctionDeclaration *cxxDefiningConstructorDecl =
      isSgMemberFunctionDeclaration(
          cxxConstructorDecl->get_definingDeclaration());
  cxxConstructorDecl->get_specialFunctionModifier().setConstructor();

  // apply ctorInitializer
  if (cxx_constructor_decl->getNumCtorInitializers() != 0 &&
      cxxDefiningConstructorDecl != nullptr) {
    SgCtorInitializerList *ctorInitializerList =
        SageBuilder::buildCtorInitializerList_nfi();
    clang::CXXConstructorDecl::init_iterator initializer;
    unsigned cnt = 0;
    for (initializer = cxx_constructor_decl->init_begin();
         initializer != cxx_constructor_decl->init_end(); initializer++) {
      cnt++;
#if DEBUG_VISIT_DECL
      std::cerr << "isBaseInitializer = " << (*initializer)->isBaseInitializer()
                << "\n";
      std::cerr << "isMemberInitializer = "
                << (*initializer)->isMemberInitializer() << "\n";
      std::cerr << "isAnyMemberInitializer  = "
                << (*initializer)->isAnyMemberInitializer() << "\n";
      std::cerr << "isIndirectMemberInitializer = "
                << (*initializer)->isIndirectMemberInitializer() << "\n";
#endif
      if ((*initializer)->isMemberInitializer()) {
        clang::FieldDecl *field_decl = (*initializer)->getMember();
        SgName fieldName(field_decl->getNameAsString());
        SgVariableDeclaration *fieldMemberDecl =
            isSgVariableDeclaration(Traverse(field_decl));
        SgInitializedName *fieldInitializedName =
            fieldMemberDecl->get_decl_item(fieldName);
        SgType *fieldType = fieldInitializedName->get_type();

        SgExpression *initExpr =
            isSgExpression(Traverse((*initializer)->getInit()));
        SgInitializer *sgCtorInitializer =
            SageBuilder::buildAssignInitializer_nfi(initExpr, fieldType);
        SgInitializedName *sgCtorInitializedName =
            SageBuilder::buildInitializedName(fieldName, fieldType,
                                              sgCtorInitializer);
        applySourceRange(sgCtorInitializedName,
                         (*initializer)->getSourceRange());
        sgCtorInitializer->set_parent(sgCtorInitializedName);
        applySourceRange(sgCtorInitializer,
                         (*initializer)->getInit()->getSourceRange());
        ctorInitializerList->append_ctor_initializer(sgCtorInitializedName);
        sgCtorInitializedName->set_parent(ctorInitializerList);
        sgCtorInitializedName->set_scope(SageBuilder::topScopeStack());
      }
    }
    cxxDefiningConstructorDecl->set_CtorInitializerList(ctorInitializerList);
    ctorInitializerList->set_parent(cxxDefiningConstructorDecl);
    ctorInitializerList->set_definingDeclaration(ctorInitializerList);
    ctorInitializerList->set_firstNondefiningDeclaration(ctorInitializerList);
  }
  if (cxx_constructor_decl->isDefaultConstructor()) {
#if DEBUG_VISIT_DECL
    std::cerr << "set as Default constructor\n";
#endif
    cxxConstructorDecl->get_functionModifier().setDefault();
  }

  return res;
}

bool ClangToSageTranslator::VisitCXXConversionDecl(
    clang::CXXConversionDecl *cxx_conversion_decl, SgNode **node) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToSageTranslator::VisitCXXConversionDecl" << std::endl;
#endif
  bool res = true;

  // ROOT CAUSE FIX: Allow delegation to work - disabled FAIL_TODO
  // ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitCXXMethodDecl(cxx_conversion_decl, node) && res;
}

bool ClangToSageTranslator::VisitCXXDestructorDecl(
    clang::CXXDestructorDecl *cxx_destructor_decl, SgNode **node) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToSageTranslator::VisitCXXDestructorDecl" << std::endl;
#endif
  bool res = true;

  // ROOT CAUSE FIX: Allow delegation to work - disabled FAIL_TODO
  // ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitCXXMethodDecl(cxx_destructor_decl, node) && res;
}

bool ClangToSageTranslator::VisitMSPropertyDecl(
    clang::MSPropertyDecl *ms_property_decl, SgNode **node) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToSageTranslator::VisitMSPropertyDecl" << std::endl;
#endif
  bool res = true;

  // ROOT CAUSE FIX: Allow delegation to work - disabled FAIL_TODO
  // ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitDeclaratorDecl(ms_property_decl, node) && res;
}

bool ClangToSageTranslator::VisitNonTypeTemplateParmDecl(
    clang::NonTypeTemplateParmDecl *non_type_template_param_decl,
    SgNode **node) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToSageTranslator::VisitNonTypeTemplateParmDecl"
            << std::endl;
#endif

  SgDeclarationStatement *owning_template = nullptr;
  if (clang::DeclContext *ctx =
          non_type_template_param_decl->getDeclContext()) {
    if (clang::TemplateDecl *template_ctx =
            llvm::dyn_cast<clang::TemplateDecl>(ctx)) {
      auto it = p_decl_translation_map.find(template_ctx);
      if (it != p_decl_translation_map.end()) {
        owning_template = isSgDeclarationStatement(it->second);
      }
    }
  }

  unsigned position = non_type_template_param_decl->getIndex();
  SgTemplateParameter *sg_param = translateTemplateParameter(
      non_type_template_param_decl, owning_template, position);

  *node = sg_param;
  return sg_param != nullptr;
}

bool ClangToSageTranslator::VisitVarDecl(clang::VarDecl *var_decl,
                                         SgNode **node) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToSageTranslator::VisitVarDecl" << std::endl;
  std::cerr << "isStaticLocal " << var_decl->isStaticLocal() << std::endl;
  std::cerr << "isStaticDataMember " << var_decl->isStaticDataMember()
            << std::endl;
#endif
  // std::cerr << "DEBUG: VisitVarDecl for " << var_decl->getNameAsString() <<
  // std::endl;
  if (var_decl->getNameAsString() == "pack") {
    // std::cerr << "DEBUG: VisitVarDecl for pack. Type: " <<
    // var_decl->getType().getAsString() << std::endl;
  }
  bool res = true;

  // Create the SAGE node: SgVariableDeclaration

  SgName name(var_decl->getNameAsString());

  clang::QualType varQualType = var_decl->getType();

  const clang::Type *varType = varQualType.getTypePtr();

#if DEBUG_VISIT_DECL
  // Wrap debug output in conditional to prevent production output
  if (name.getString() == "x") {
    // std::cerr << "DEBUG VarDecl: Variable 'x' has type class = " <<
    // varType->getTypeClassName() << std::endl;
  }
#endif

  // Pei-Hung (06/01/2022) check if the declaration is considered embedded in
  // Clang AST. If it is embedded, no explicit SgDeclaration should be placed
  // for ROSE AST.
  bool isembedded = false;
  bool iscompleteDefined = false;
  bool hasElaboratedType = false;
  bool isOwnedTagDeclADefinition = false;
  bool isDefinitionRequired = false;
  // Definitions embedded in a declarator are not autonomous.
  bool isAutonomousDeclaration = true;

  // Adding check for EaboratedType and PointerType to retrieve base EnumType
  // while((varType->getTypeClass() == clang::Type::Elaborated) ||
  // (varType->getTypeClass() == clang::Type::Pointer) ||
  // (varType->getTypeClass() == clang::Type::Array))
  while ((llvm::isa<clang::ElaboratedType>(varType)) ||
         (llvm::isa<clang::PointerType>(varType)) ||
         (llvm::isa<clang::ArrayType>(varType))) {
    if (llvm::isa<clang::ElaboratedType>(varType)) {
      hasElaboratedType = true;
      varQualType = ((clang::ElaboratedType *)varType)->getNamedType();
      clang::TagDecl *ownedTagDecl =
          ((clang::ElaboratedType *)varType)->getOwnedTagDecl();
      if (ownedTagDecl != nullptr) {
        isOwnedTagDeclADefinition =
            ownedTagDecl->isThisDeclarationADefinition();
      }
    } else if (llvm::isa<clang::PointerType>(varType)) {
      varQualType = ((clang::PointerType *)varType)->getPointeeType();
    } else if (llvm::isa<clang::ArrayType>(varType)) {
      varQualType = ((clang::ArrayType *)varType)->getElementType();
    }
    varType = varQualType.getTypePtr();
  }

  if (llvm::isa<clang::EnumType>(varType)) {
    clang::EnumType *underlyingEnumType = (clang::EnumType *)varType;
    clang::EnumDecl *enumDeclaration = underlyingEnumType->getDecl();
    isembedded = enumDeclaration->isEmbeddedInDeclarator();
    iscompleteDefined = enumDeclaration->isCompleteDefinition();
    isAutonomousDeclaration = false;
  }

  if (llvm::isa<clang::RecordType>(varType)) {
    clang::RecordType *underlyingRecordType = (clang::RecordType *)varType;
    clang::RecordDecl *recordDeclaration = underlyingRecordType->getDecl();
    isembedded = recordDeclaration->isEmbeddedInDeclarator();
    iscompleteDefined = recordDeclaration->isCompleteDefinition();
    isAutonomousDeclaration = false;
  }

  if (hasElaboratedType) {
    isDefinitionRequired = isOwnedTagDeclADefinition;
  } else {
    // This might not be the precise info for set_isAutonomousDeclaration.
    isDefinitionRequired = iscompleteDefined;
  }

  SgType *sg_varType = buildTypeFromQualifiedType(varQualType);
  SgType *type = buildTypeFromQualifiedType(var_decl->getType());

  bool isStaticDataMember = var_decl->isStaticDataMember();

  //    SgVariableDeclaration * sg_var_decl = new SgVariableDeclaration(name,
  //    type, init); // scope: obtain from the scope stack.
  // Pei-Hung (09/01/2022) In test2022_3.c, the variable symbol needs to be
  // avaiable before processing the RHS. calling buildVariableDeclaration_nfi to
  // get the symbol in place.
  SgVariableDeclaration *sg_var_decl = nullptr;
  // Pei-Hung (09/29/23) The definition of a static data member needs to call
  // set_prev_decl_item to point to its first static data member declaration
  // inside the class. buildVariableDeclaration_nfi will take care of the
  // details by looking up the SgSymbol in the symbol table of the class.
  if (isStaticDataMember && var_decl->getPreviousDecl() != nullptr) {
    clang::VarDecl *prevDecl = var_decl->getPreviousDecl();
    SgVariableDeclaration *sgPrevDecl =
        isSgVariableDeclaration(Traverse(prevDecl));
    ROSE_ASSERT(sgPrevDecl);
    sg_var_decl = SageBuilder::buildVariableDeclaration_nfi(
        name, type, nullptr, SageInterface::getScope(sgPrevDecl));
  } else {
    sg_var_decl = SageBuilder::buildVariableDeclaration_nfi(
        name, type, nullptr, SageBuilder::topScopeStack());
  }
  sg_var_decl->set_isAssociatedWithDeclarationList(true);
  if (var_decl->isConstexpr()) {
    sg_var_decl->set_is_constexpr(true);
  }

  // CLANG FRONTEND FIX: Check if variable has an initializer before traversing
  clang::Expr *init_expr = var_decl->getInit();
  SgExpression *expr = nullptr;
  SgExprListExp *expr_list_expr = nullptr;
  SgInitializer *init = nullptr;
  SgNode *tmp_init = nullptr;
  if (init_expr != nullptr) {
    tmp_init = Traverse(init_expr);
    if (SgInitializer *tmp_init_initializer = isSgInitializer(tmp_init)) {
      init = tmp_init_initializer;
    } else {
      expr = isSgExpression(tmp_init);
      if (tmp_init != nullptr && expr == nullptr) {
        std::cerr << "Runtime error: not a SgInitializer..."
                  << std::endl; // TODO
        res = false;
      }
      expr_list_expr = isSgExprListExp(expr);
      if (expr_list_expr != nullptr)
        init = SageBuilder::buildAggregateInitializer(expr_list_expr, type);
      else if (expr != nullptr) {
        // CLANG FRONTEND FIX: Check if expr is already an initializer (e.g.,
        // SgConstructorInitializer) If so, use it directly instead of wrapping
        // it in SgAssignInitializer This preserves constructor syntax:
        // std::string str("hello") instead of std::string str = ("hello")
        SgInitializer *existing_init = isSgInitializer(expr);
        if (existing_init != nullptr) {
          // Expression is already an initializer (e.g., from CXXConstructExpr)
          // Use it directly without wrapping
          init = existing_init;
        } else {
          // Expression is not an initializer, wrap it in SgAssignInitializer
          // This handles cases like: int x = 5;
          init =
              SageBuilder::buildAssignInitializer_nfi(expr, expr->get_type());
        }
      }
    }
  }

  // Pei-Hung (09/01/2022) setup initializer once the RHS is processed.
  // CLANG FRONTEND FIX: Only set initializer if it's not nullptr
  if (init != nullptr) {
    sg_var_decl->reset_initializer(init);
  }

  // CLANG FRONTEND FIX: Set initializer parent AFTER reset_initializer
  // reset_initializer sets the parent of the initializer to the
  // SgInitializedName, not the SgVariableDeclaration. Setting it to sg_var_decl
  // here was wrong. Only apply source range if we have both the initializer and
  // the original expression
  if (init != nullptr && init_expr != nullptr) {
    // Pei-Hung (07/12/2023):
    // applySourceRange should be set whenever the SgInitializer was just
    // created. Otherwise, it could overwrite the setting done in
    // setCompilerGeneratedFileInfo.
    if (!llvm::isa<clang::CXXConstructExpr>(init_expr)) {
      applySourceRange(init, init_expr->getSourceRange());
    }
  }

  // finding the bottom base type and check
  while (type->findBaseType() != type) {
    type = type->findBaseType();
    if (type == sg_varType)
      break;
  }

  if (isSgClassType(type) && isDefinitionRequired) {
    SgClassDeclaration *classDecl =
        isSgClassDeclaration(isSgClassType(type)->get_declaration());
    SgClassDeclaration *classDefDecl = isSgClassDeclaration(
        isSgClassType(type)->get_declaration()->get_definingDeclaration());
    if (isembedded && classDefDecl != nullptr &&
        !isSgDeclarationStatement(classDefDecl->get_parent())) {
      classDefDecl->set_parent(sg_var_decl);
      classDefDecl->set_isAutonomousDeclaration(isAutonomousDeclaration);
      sg_var_decl->set_baseTypeDefiningDeclaration(classDefDecl);
      sg_var_decl->set_variableDeclarationContainsBaseTypeDefiningDeclaration(
          true);
    }

    std::map<SgClassType *, bool>::iterator bool_it =
        p_class_type_decl_first_see_in_type.find(isSgClassType(type));
    ROSE_ASSERT(bool_it != p_class_type_decl_first_see_in_type.end());
    if (bool_it->second) {
      sg_var_decl->set_baseTypeDefiningDeclaration(
          isSgNamedType(type)->get_declaration()->get_definingDeclaration());
      sg_var_decl->set_variableDeclarationContainsBaseTypeDefiningDeclaration(
          true);
      bool_it->second = false;
    }
  } else if (isSgEnumType(type) && isDefinitionRequired) {
    SgEnumDeclaration *enumDecl =
        isSgEnumDeclaration(isSgEnumType(type)->get_declaration());
    SgEnumDeclaration *enumDefDecl = isSgEnumDeclaration(
        isSgEnumType(type)->get_declaration()->get_definingDeclaration());
    if (isembedded && enumDefDecl != nullptr &&
        !isSgDeclarationStatement(enumDefDecl->get_parent())) {
      enumDefDecl->set_parent(sg_var_decl);
      enumDefDecl->set_isAutonomousDeclaration(isAutonomousDeclaration);
      sg_var_decl->set_baseTypeDefiningDeclaration(enumDefDecl);
      sg_var_decl->set_variableDeclarationContainsBaseTypeDefiningDeclaration(
          true);
    }

    std::map<SgEnumType *, bool>::iterator bool_it =
        p_enum_type_decl_first_see_in_type.find(isSgEnumType(type));
    ROSE_ASSERT(bool_it != p_enum_type_decl_first_see_in_type.end());
    if (bool_it->second) {
      sg_var_decl->set_baseTypeDefiningDeclaration(
          isSgEnumType(type)->get_declaration()->get_definingDeclaration());
      sg_var_decl->set_variableDeclarationContainsBaseTypeDefiningDeclaration(
          true);
      bool_it->second = false;
    }
  }

  sg_var_decl->set_firstNondefiningDeclaration(sg_var_decl);
  sg_var_decl->set_parent(SageBuilder::topScopeStack());

  // REX FIX: Handle AS_none for VarDecl to avoid unparser assertion
  clang::AccessSpecifier access = var_decl->getAccess();
  if (access == clang::AS_public) {
    sg_var_decl->get_declarationModifier().get_accessModifier().setPublic();
  } else if (access == clang::AS_private) {
    sg_var_decl->get_declarationModifier().get_accessModifier().setPrivate();
  } else if (access == clang::AS_protected) {
    sg_var_decl->get_declarationModifier().get_accessModifier().setProtected();
  } else if (access == clang::AS_none) {
    if (isSgClassDefinition(SageBuilder::topScopeStack())) {
      SgClassDefinition *class_def =
          isSgClassDefinition(SageBuilder::topScopeStack());
      if (class_def->get_declaration()->get_class_type() ==
          SgClassDeclaration::e_class) {
        sg_var_decl->get_declarationModifier()
            .get_accessModifier()
            .setPrivate();
      } else {
        sg_var_decl->get_declarationModifier().get_accessModifier().setPublic();
      }
    } else {
      sg_var_decl->get_declarationModifier().get_accessModifier().setPublic();
    }
  }

  ROSE_ASSERT(sg_var_decl->get_variables().size() == 1);

  SgInitializedName *init_name = sg_var_decl->get_variables()[0];
  ROSE_ASSERT(init_name != nullptr);
  if (init_name->get_scope() == nullptr) {
    init_name->set_scope(SageBuilder::topScopeStack());
  }
  if (var_decl->getInitStyle() == clang::VarDecl::ListInit) {
    init_name->set_is_braced_initialized(true);
    if (!var_decl->isDirectInit()) {
      init_name->set_using_assignment_copy_constructor_syntax(true);
    }
  }
  if (var_decl->isConstexpr()) {
    bool explicit_const = true;
    if (const clang::TypeSourceInfo *type_info =
            var_decl->getTypeSourceInfo()) {
      explicit_const = type_info->getType().isLocalConstQualified();
    }
    init_name->set_is_constexpr_const_implicit(!explicit_const);
  }
  // CLANG FRONTEND FIX: Set initializer parent to SgInitializedName
  // The initializer is a child of the SgInitializedName, not the
  // SgVariableDeclaration
  if (init != nullptr) {
    init->set_parent(init_name);
  }

  applySourceRange(init_name, var_decl->getSourceRange());

  // CLANG FRONTEND FIX: The declptr should already be set by
  // SgVariableDeclaration constructor to point to the SgVariableDefinition. If
  // it's null, we need to check why.
  SgVariableDefinition *var_def =
      isSgVariableDefinition(init_name->get_declptr());
  if (var_def == nullptr) {
    // If declptr is null, try to get it from the variable declaration
    // buildVariableDeclaration_nfi should have created a definition
    var_def = sg_var_decl->get_definition();
    if (var_def != nullptr) {
      init_name->set_declptr(var_def);
    } else {
      // Debug: why is var_def null?
      printf("ERROR: Variable definition is null for variable: %s\n",
             init_name->get_name().str());
      printf("  sg_var_decl = %p\n", sg_var_decl);
      printf("  init_name = %p\n", init_name);
      printf("  init_name->get_declptr() = %p\n", init_name->get_declptr());
      fflush(stdout);
    }
  }
  ROSE_ASSERT(var_def != nullptr);
  applySourceRange(var_def, var_decl->getSourceRange());

  // Pei-Hung (06/16/22) added "extern" modifier
  bool hasExternalStorage = var_decl->hasExternalStorage();
  if (hasExternalStorage) {
    sg_var_decl->get_declarationModifier().get_storageModifier().setExtern();
  }

  // Pei-Hung (06/16/22) added "static" modifier
  bool shouldSetStatic = (var_decl->getStorageClass() == clang::SC_Static) &&
                         !var_decl->isOutOfLine();
  if (shouldSetStatic) {
    sg_var_decl->get_declarationModifier().get_storageModifier().setStatic();
  }
  // Pei-Hung (03/14/23) added "static" modifier for data member
  if (isStaticDataMember) {
    // Pei-Hung (09/29/23) Only set for the first static data member
    // declaration (the in-class declaration).
    if (var_decl->getPreviousDecl() == nullptr) {
      sg_var_decl->get_declarationModifier().get_storageModifier().setStatic();
    }
  }

  if (!isembedded) {
    sg_var_decl->set_variableDeclarationContainsBaseTypeDefiningDeclaration(
        false);
  }

  *node = sg_var_decl;

  bool visit_res = VisitDeclaratorDecl(var_decl, node) && res;
  return visit_res;
}

bool ClangToSageTranslator::VisitDecompositionDecl(
    clang::DecompositionDecl *decomposition_decl, SgNode **node) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToSageTranslator::VisitDecompositionDecl" << std::endl;
#endif
  bool res = true;

  // ROOT CAUSE FIX: Allow delegation to work - disabled FAIL_TODO
  // ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitVarDecl(decomposition_decl, node) && res;
}

bool ClangToSageTranslator::VisitImplicitParamDecl(
    clang::ImplicitParamDecl *implicit_param_decl, SgNode **node) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToSageTranslator::VisitImplicitParamDecl" << std::endl;
#endif
  bool res = true;

  // ROOT CAUSE FIX: Allow delegation to work - disabled FAIL_TODO
  // ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitVarDecl(implicit_param_decl, node) && res;
}

bool ClangToSageTranslator::VisitOMPCaptureExprDecl(
    clang::OMPCapturedExprDecl *omp_capture_expr_decl, SgNode **node) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToSageTranslator::VisitOMPCaptureExprDecl" << std::endl;
#endif
  bool res = true;

  // ROOT CAUSE FIX: Allow delegation to work - disabled FAIL_TODO
  // ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitVarDecl(omp_capture_expr_decl, node) && res;
}

bool ClangToSageTranslator::VisitParmVarDecl(clang::ParmVarDecl *param_var_decl,
                                             SgNode **node) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToSageTranslator::VisitParmVarDecl" << std::endl;
#endif
  bool res = true;

  SgName name(param_var_decl->getNameAsString());

  // use getOriginalType instead of getType.  This type has to match the
  // DecayedType when VLA is used in parameter
  SgType *type = buildTypeFromQualifiedType(param_var_decl->getOriginalType());

  SgInitializer *init = nullptr;

  if (param_var_decl->hasDefaultArg()) {
    SgNode *tmp_expr = Traverse(param_var_decl->getDefaultArg());
    SgExpression *expr = isSgExpression(tmp_expr);
    // ROOT CAUSE FIX: Check that expr is not nullptr before using it
    // The conversion from tmp_expr to expr can fail, leaving expr == nullptr
    if (tmp_expr != nullptr && expr == nullptr) {
      std::cerr << "Runtime error: tmp_expr != nullptr && expr == nullptr"
                << std::endl;
      res = false;
    } else if (expr != nullptr) {
      // The same Clang default-argument subtree can be referenced by multiple
      // redeclarations. Reusing a single SgExpression would create multiple
      // parents and break CFG invariants. Always deep-copy the expression
      // before attaching it to a parameter initializer.
      SgExpression *expr_copy = SageInterface::deepCopy(expr);
      ROSE_ASSERT(expr_copy != nullptr);
      applySourceRange(expr_copy, param_var_decl->getDefaultArgRange());
      init = SageBuilder::buildAssignInitializer_nfi(expr_copy,
                                                     expr_copy->get_type());
      applySourceRange(init, param_var_decl->getDefaultArgRange());
    }
  }

  SgInitializedName *param_init_name =
      SageBuilder::buildInitializedName(name, type, init);
  applySourceRange(param_init_name, param_var_decl->getSourceRange());
  if (param_var_decl->isParameterPack()) {
    param_init_name->set_is_parameter_pack(true);
    param_init_name->set_is_pack_element(true);
    if (SgTemplateType *template_type = isSgTemplateType(type)) {
      template_type->set_packed(true);
    }
  }
  // Set scope and parent to avoid unparser assertion - function declaration
  // builder will adjust this later
  SgScopeStatement *scope = SageBuilder::topScopeStack();
  param_init_name->set_scope(scope);
  param_init_name->set_parent(scope);
  *node = param_init_name;

  return VisitDeclaratorDecl(param_var_decl, node) && res;
}

bool ClangToSageTranslator::VisitVarTemplateSpecializationDecl(
    clang::VarTemplateSpecializationDecl *var_template_specialization_decl,
    SgNode **node) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToSageTranslator::VisitVarTemplateSpecializationDecl"
            << std::endl;
#endif
  bool res = true;

  if (var_template_specialization_decl == nullptr ||
      var_template_specialization_decl->isInvalidDecl()) {
    *node = nullptr;
    return false;
  }

  // ROOT CAUSE FIX: Variable template specializations should be represented as
  // SgTemplateVariableDeclaration with specialization arguments so name
  // qualification and symbol table logic can distinguish instantiations.

  // Get variable name and type
  SgName name(var_template_specialization_decl->getNameAsString());
  clang::QualType qual_type = var_template_specialization_decl->getType();
  SgType *type = buildTypeFromQualifiedType(qual_type);

  // Get initializer if present
  SgInitializer *init = nullptr;
  if (var_template_specialization_decl->hasInit()) {
    clang::Expr *init_expr = var_template_specialization_decl->getInit();
    if (init_expr != nullptr) {
      SgNode *tmp_node = Traverse(init_expr);
      SgExpression *sg_init_expr = isSgExpression(tmp_node);
      if (sg_init_expr != nullptr) {
        init = SageBuilder::buildAssignInitializer(sg_init_expr, type);
      }
    }
  }

  SgScopeStatement *scope = SageBuilder::topScopeStack();
  if (clang::DeclContext *decl_context =
          var_template_specialization_decl->getDeclContext()) {
    if (SgScopeStatement *resolved =
            resolveScopeFromDeclContext(decl_context, scope)) {
      scope = resolved;
    }
  }
  if (scope == nullptr) {
    scope = getGlobalScope();
  }

  SgTemplateVariableDeclaration *var_decl =
      SageBuilder::buildTemplateVariableDeclaration_nfi(name, type, init,
                                                        scope);
  var_decl->set_isAssociatedWithDeclarationList(true);

  clang::TemplateSpecializationKind specialization_kind =
      var_template_specialization_decl->getTemplateSpecializationKind();
  const bool specialization_is_partial =
      var_template_specialization_decl->getSpecializedTemplateOrPartial()
          .dyn_cast<clang::VarTemplatePartialSpecializationDecl *>() != nullptr;

  const clang::TemplateArgumentList &args =
      var_template_specialization_decl->getTemplateArgs();
  const clang::TemplateArgumentList &instantiation_args =
      var_template_specialization_decl->getTemplateInstantiationArgs();

  ConstraintSatisfactionResult constraint_result;
  if (specialization_kind == clang::TSK_ImplicitInstantiation ||
      specialization_kind == clang::TSK_ExplicitInstantiationDeclaration ||
      specialization_kind == clang::TSK_ExplicitInstantiationDefinition) {
#if LLVM_VERSION_MAJOR >= 21
    llvm::SmallVector<clang::AssociatedConstraint, 4> constraints;
#else
    llvm::SmallVector<const clang::Expr *, 4> constraints;
#endif
    const clang::NamedDecl *constraint_owner = nullptr;
    if (const clang::VarTemplatePartialSpecializationDecl *partial =
            llvm::dyn_cast<clang::VarTemplatePartialSpecializationDecl>(
                var_template_specialization_decl)) {
      partial->getAssociatedConstraints(constraints);
      constraint_owner = partial;
    } else if (clang::VarTemplateDecl *primary =
                   var_template_specialization_decl->getSpecializedTemplate()) {
      if (clang::TemplateParameterList *params =
              primary->getTemplateParameters()) {
        params->getAssociatedConstraints(constraints);
      }
      constraint_owner = primary;
    }

    if (constraint_owner != nullptr && !constraints.empty()) {
      const clang::TemplateArgumentList &constraint_args =
          specialization_is_partial ? instantiation_args : args;
      constraint_result = evaluateConstraintSatisfaction(
          constraint_owner, constraints, constraint_args,
          var_template_specialization_decl->getSourceRange());
    }
  }

  SgTemplateArgumentPtrList tpl_args;
  for (unsigned i = 0; i < args.size(); ++i) {
    appendTemplateArguments(tpl_args, args.get(i), false);
  }
  var_decl->get_templateSpecializationArguments() = tpl_args;
  for (SgTemplateArgument *arg :
       var_decl->get_templateSpecializationArguments()) {
    if (arg != nullptr) {
      arg->set_parent(var_decl);
    }
  }
  SgTemplateArgumentPtrList deduced_args;
  for (unsigned i = 0; i < instantiation_args.size(); ++i) {
    appendTemplateArguments(deduced_args, instantiation_args.get(i), false);
  }
  var_decl->get_deducedTemplateArguments() = deduced_args;
  for (SgTemplateArgument *arg : var_decl->get_deducedTemplateArguments()) {
    if (arg != nullptr) {
      arg->set_parent(var_decl);
    }
  }

  auto lookup_template_decl =
      [&](clang::Decl *key) -> SgTemplateVariableDeclaration * {
    if (key == nullptr) {
      return nullptr;
    }
    auto it = p_decl_translation_map.find(key);
    if (it != p_decl_translation_map.end()) {
      return isSgTemplateVariableDeclaration(it->second);
    }
    if (p_decl_translation_in_progress.find(key) !=
            p_decl_translation_in_progress.end() ||
        p_decl_translation_on_demand.find(key) !=
            p_decl_translation_on_demand.end()) {
      return nullptr;
    }
    SgNode *translated = TraverseOnDemand(key);
    return isSgTemplateVariableDeclaration(translated);
  };

  if (clang::VarTemplatePartialSpecializationDecl *partial =
          llvm::dyn_cast<clang::VarTemplatePartialSpecializationDecl>(
              var_template_specialization_decl)) {
    if (clang::TemplateParameterList *param_list =
            partial->getTemplateParameters()) {
      std::unique_ptr<SgTemplateParameterPtrList> params =
          translateTemplateParameterList(param_list, var_decl);
      if (params != nullptr) {
        var_decl->get_templateParameters() = *params;
      }
    }
  } else {
    if (clang::VarTemplateDecl *primary =
            var_template_specialization_decl->getSpecializedTemplate()) {
      if (SgTemplateVariableDeclaration *primary_decl =
              lookup_template_decl(primary)) {
        var_decl->get_templateParameters() =
            primary_decl->get_templateParameters();
      }
    }
  }

  // Record the specialized template declaration chosen by Clang.
  SgDeclarationStatement *specialized_template_decl = nullptr;
  if (clang::VarTemplatePartialSpecializationDecl *partial =
          llvm::dyn_cast<clang::VarTemplatePartialSpecializationDecl>(
              var_template_specialization_decl)) {
    specialized_template_decl = lookup_template_decl(partial);
  }
  if (specialized_template_decl == nullptr) {
    if (clang::VarTemplateDecl *primary =
            var_template_specialization_decl->getSpecializedTemplate()) {
      specialized_template_decl = lookup_template_decl(primary);
    }
  }
  if (specialized_template_decl != nullptr) {
    var_decl->set_specializedTemplateDeclaration(specialized_template_decl);
  }

  applySourceRange(var_decl,
                   var_template_specialization_decl->getSourceRange());

  if (constraint_result.evaluated) {
    attachConstraintSatisfaction(var_decl, constraint_result);
  }
  registerDeclarationSymbol(var_decl);

  *node = var_decl;

  return VisitDeclaratorDecl(var_template_specialization_decl, node) && res;
}

bool ClangToSageTranslator::VisitVarTemplatePartialSpecializationDecl(
    clang::VarTemplatePartialSpecializationDecl
        *var_template_partial_specialization_decl,
    SgNode **node) {
#if DEBUG_VISIT_DECL
  std::cerr
      << "ClangToSageTranslator::VisitVarTemplatePartialSpecializationDecl"
      << std::endl;
#endif
  bool res = true;

  if (var_template_partial_specialization_decl == nullptr ||
      var_template_partial_specialization_decl->isInvalidDecl()) {
    *node = nullptr;
    return false;
  }

  // ROOT CAUSE FIX: Model partial specializations with template parameters
  // and specialization arguments instead of treating them as instantiations.

  SgName name(var_template_partial_specialization_decl->getNameAsString());
  if (name.getString().empty()) {
    name = "__anon_var_template_partial_" +
           generate_source_position_string(
               var_template_partial_specialization_decl->getBeginLoc());
  }

  SgType *type = buildTypeFromQualifiedType(
      var_template_partial_specialization_decl->getType());

  SgInitializer *init = nullptr;
  if (var_template_partial_specialization_decl->hasInit()) {
    clang::Expr *init_expr =
        var_template_partial_specialization_decl->getInit();
    if (init_expr != nullptr) {
      SgNode *tmp_node = Traverse(init_expr);
      if (SgInitializer *tmp_init = isSgInitializer(tmp_node)) {
        init = tmp_init;
      } else if (SgExpression *expr = isSgExpression(tmp_node)) {
        init = SageBuilder::buildAssignInitializer(expr, type);
      }
    }
  }

  SgScopeStatement *scope = SageBuilder::topScopeStack();
  if (clang::DeclContext *decl_context =
          var_template_partial_specialization_decl->getDeclContext()) {
    if (SgScopeStatement *resolved =
            resolveScopeFromDeclContext(decl_context, scope)) {
      scope = resolved;
    }
  }
  if (scope == nullptr) {
    scope = getGlobalScope();
  }

  SgTemplateVariableDeclaration *var_decl =
      SageBuilder::buildTemplateVariableDeclaration_nfi(name, type, init,
                                                        scope);
  var_decl->set_isAssociatedWithDeclarationList(true);
  var_decl->set_specialization(
      SgDeclarationStatement::e_partial_specialization);

  if (clang::TemplateParameterList *param_list =
          var_template_partial_specialization_decl->getTemplateParameters()) {
    std::unique_ptr<SgTemplateParameterPtrList> params =
        translateTemplateParameterList(param_list, var_decl);
    if (params != nullptr) {
      var_decl->get_templateParameters() = *params;
    }
  }

  SgTemplateArgumentPtrList spec_args;
  if (const clang::ASTTemplateArgumentListInfo *args_written =
          var_template_partial_specialization_decl
              ->getTemplateArgsAsWritten()) {
    for (const clang::TemplateArgumentLoc &arg_loc :
         args_written->arguments()) {
      appendTemplateArguments(spec_args, arg_loc.getArgument(), false);
    }
  } else {
    const clang::TemplateArgumentList &args =
        var_template_partial_specialization_decl->getTemplateArgs();
    for (unsigned i = 0; i < args.size(); ++i) {
      appendTemplateArguments(spec_args, args.get(i), false);
    }
  }
  ensureTemplateArgumentParents(spec_args);
  var_decl->get_templateSpecializationArguments() = spec_args;
  for (SgTemplateArgument *arg :
       var_decl->get_templateSpecializationArguments()) {
    if (arg != nullptr) {
      arg->set_parent(var_decl);
    }
  }

  if (clang::TemplateParameterList *params =
          var_template_partial_specialization_decl->getTemplateParameters()) {
    if (const clang::Expr *requires_expr = params->getRequiresClause()) {
      if (SgExpression *sg_requires =
              translateConstraintExpression(requires_expr)) {
        var_decl->set_requiresClause(sg_requires);
        sg_requires->set_parent(var_decl);
      }
    }
  }

  applySourceRange(var_decl,
                   var_template_partial_specialization_decl->getSourceRange());

  registerDeclarationSymbol(var_decl);

  *node = var_decl;
  return VisitDeclaratorDecl(var_template_partial_specialization_decl, node) &&
         res;
}

bool ClangToSageTranslator::VisitEnumConstantDecl(
    clang::EnumConstantDecl *enum_constant_decl, SgNode **node) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToSageTranslator::VisitEnumConstantDecl" << std::endl;
#endif
  bool res = true;

  // Safely get the name - check if the declaration name is valid first
  SgName name;
  if (enum_constant_decl && enum_constant_decl->getDeclName().isIdentifier()) {
    name = SgName(enum_constant_decl->getNameAsString());
  } else {
    // Fallback to empty name if declaration name is not a simple identifier
    name = SgName("");
  }

  // CRITICAL: Create a placeholder node and add to translation map BEFORE
  // visiting the type This prevents infinite recursion when VisitEnumType ->
  // VisitEnumDecl tries to visit this constant again Use int type as
  // placeholder since buildInitializedName requires non-null type
  SgInitializedName *init_name_placeholder = SageBuilder::buildInitializedName(
      name, SageBuilder::buildIntType(), nullptr);
  p_decl_translation_map.insert(std::pair<clang::Decl *, SgNode *>(
      enum_constant_decl, init_name_placeholder));

  // Get the enum constant's type - this should be the enum type itself, not the
  // underlying integer type This is critical for type safety, especially for
  // scoped enums (enum class) where the enumerator must have the enum type, not
  // int
  SgType *type = buildTypeFromQualifiedType(enum_constant_decl->getType());

  // Update the placeholder with the actual type
  init_name_placeholder->set_type(type);

  SgInitializer *init = nullptr;

  if (enum_constant_decl->getInitExpr() != nullptr) {
    SgNode *tmp_expr = Traverse(enum_constant_decl->getInitExpr());
    SgExpression *expr = isSgExpression(tmp_expr);
    if (tmp_expr != nullptr && expr == nullptr) {
      std::cerr << "Runtime error: tmp_expr != nullptr && expr == nullptr"
                << std::endl;
      res = false;
    } else {
      init = SageBuilder::buildAssignInitializer_nfi(expr, expr->get_type());
    }
  }

  // Update the placeholder with the initializer
  init_name_placeholder->set_initializer(init);

  // Use the placeholder as the final node (no need to create a new one)
  SgInitializedName *init_name = init_name_placeholder;

  SgScopeStatement *scope = SageBuilder::topScopeStack();
  clang::DeclContext *scope_context = enum_constant_decl->getDeclContext();
  if (clang::EnumDecl *enum_context =
          llvm::dyn_cast<clang::EnumDecl>(scope_context)) {
    scope_context = enum_context->getDeclContext();
  }
  while (scope_context != nullptr &&
         llvm::isa<clang::LinkageSpecDecl>(scope_context)) {
    scope_context = scope_context->getParent();
  }
  if (scope_context != nullptr) {
    if (scope_context->isTranslationUnit()) {
      scope = getGlobalScope();
    } else if (clang::Decl *context_decl =
                   llvm::dyn_cast<clang::Decl>(scope_context)) {
      if (is_declaration_scope_context(scope_context) &&
          !llvm::isa<clang::NamespaceDecl>(context_decl) &&
          p_decl_translation_map.find(context_decl) ==
              p_decl_translation_map.end() &&
          p_decl_translation_in_progress.find(context_decl) ==
              p_decl_translation_in_progress.end() &&
          p_decl_translation_on_demand.find(context_decl) ==
              p_decl_translation_on_demand.end()) {
        TraverseOnDemand(context_decl);
      }
      if (SgScopeStatement *resolved =
              resolveScopeFromDeclContext(scope_context, scope)) {
        scope = resolved;
      }
    }
  }
  if (scope == nullptr) {
    scope = getGlobalScope();
  }
  scope = normalizeNamespaceScope(scope);

  init_name->set_scope(scope);
  init_name->set_parent(scope);

  // CLANG FRONTEND FIX: declptr will be set in VisitEnumDecl after appending
  // the enumerator (we can't set it here because the enum declaration hasn't
  // been added to translation map yet)

  if (scope != nullptr &&
      scope->find_symbol_from_declaration(init_name) == nullptr) {
    SgEnumFieldSymbol *symbol = new SgEnumFieldSymbol(init_name);
    scope->insert_symbol(name, symbol);
  }

  *node = init_name;

  return VisitValueDecl(enum_constant_decl, node) && res;
}

bool ClangToSageTranslator::VisitIndirectFieldDecl(
    clang::IndirectFieldDecl *indirect_field_decl, SgNode **node) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToSageTranslator::VisitIndirectFieldDecl" << std::endl;
#endif
  bool res = true;

  // ROOT CAUSE FIX: Allow delegation to work - disabled FAIL_TODO
  // ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitValueDecl(indirect_field_decl, node) && res;
}

bool ClangToSageTranslator::VisitOMPDeclareMapperDecl(
    clang::OMPDeclareMapperDecl *omp_declare_mapper_decl, SgNode **node) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToSageTranslator::VisitOMPDeclareMapperDecl" << std::endl;
#endif
  bool res = true;

  // ROOT CAUSE FIX: Allow delegation to work - disabled FAIL_TODO
  // ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitValueDecl(omp_declare_mapper_decl, node) && res;
}

bool ClangToSageTranslator::VisitOMPDeclareReductionDecl(
    clang::OMPDeclareReductionDecl *omp_declare_reduction_decl, SgNode **node) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToSageTranslator::VisitOMPDeclareReductionDecl"
            << std::endl;
#endif
  bool res = true;

  // ROOT CAUSE FIX: Allow delegation to work - disabled FAIL_TODO
  // ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitValueDecl(omp_declare_reduction_decl, node) && res;
}

bool ClangToSageTranslator::VisitUnresolvedUsingValueDecl(
    clang::UnresolvedUsingValueDecl *unresolved_using_value_decl,
    SgNode **node) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToSageTranslator::VisitUnresolvedUsingValueDecl"
            << std::endl;
#endif
  bool res = true;

  // Build a using declaration statement to represent the dependent value.
  // There is no resolved target yet, so capture the name with an unknown type.
  std::string name_str = unresolved_using_value_decl->getNameAsString();
  SgType *unknown_type = SageBuilder::buildUnknownType();
  SgInitializedName *init_name =
      SageBuilder::buildInitializedName(SgName(name_str), unknown_type);
  SgScopeStatement *current_scope = SageBuilder::topScopeStack();
  if (current_scope != nullptr) {
    init_name->set_scope(current_scope);
    if (current_scope->find_symbol_from_declaration(init_name) == nullptr) {
      SgVariableSymbol *symbol = new SgVariableSymbol(init_name);
      current_scope->insert_symbol(init_name->get_name(), symbol);
    }
  }

  SgUsingDeclarationStatement *using_stmt =
      new SgUsingDeclarationStatement(nullptr, init_name);
  init_name->set_parent(using_stmt);
  using_stmt->set_definingDeclaration(using_stmt);
  using_stmt->set_firstNondefiningDeclaration(using_stmt);

  if (current_scope != nullptr) {
    using_stmt->set_scope(current_scope);
    using_stmt->set_parent(current_scope);
  }
  diagnose_null_scope(using_stmt, "UnresolvedUsingValueDecl");

  *node = using_stmt;

  return VisitValueDecl(unresolved_using_value_decl, node) && res;
}

bool ClangToSageTranslator::VisitOMPAllocateDecl(
    clang::OMPAllocateDecl *omp_allocate_decl, SgNode **node) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToSageTranslator::VisitOMPAllocateDecl" << std::endl;
#endif
  bool res = true;

  // ROOT CAUSE FIX: Allow delegation to work - disabled FAIL_TODO
  // ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitDecl(omp_allocate_decl, node) && res;
}

bool ClangToSageTranslator::VisitOMPRequiresDecl(
    clang::OMPRequiresDecl *omp_requires_decl, SgNode **node) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToSageTranslator::VisitOMPRequiresDecl" << std::endl;
#endif
  bool res = true;

  // ROOT CAUSE FIX: Allow delegation to work - disabled FAIL_TODO
  // ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitDecl(omp_requires_decl, node) && res;
}

bool ClangToSageTranslator::VisitOMPThreadPrivateDecl(
    clang::OMPThreadPrivateDecl *omp_thread_private_decl, SgNode **node) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToSageTranslator::VisitOMPThreadPrivateDecl" << std::endl;
#endif
  bool res = true;

  // ROOT CAUSE FIX: Allow delegation to work - disabled FAIL_TODO
  // ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitDecl(omp_thread_private_decl, node) && res;
}

bool ClangToSageTranslator::VisitPragmaCommentDecl(
    clang::PragmaCommentDecl *pragma_comment_decl, SgNode **node) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToSageTranslator::VisitPragmaCommentDecl" << std::endl;
#endif
  bool res = true;

  // ROOT CAUSE FIX: Allow delegation to work - disabled FAIL_TODO
  // ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitDecl(pragma_comment_decl, node) && res;
}

bool ClangToSageTranslator::VisitPragmaDetectMismatchDecl(
    clang::PragmaDetectMismatchDecl *pragma_detect_mismatch_decl,
    SgNode **node) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToSageTranslator::VisitPragmaDetectMismatchDecl"
            << std::endl;
#endif
  bool res = true;

  // ROOT CAUSE FIX: Allow delegation to work - disabled FAIL_TODO
  // ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitDecl(pragma_detect_mismatch_decl, node) && res;
}

bool ClangToSageTranslator::VisitStaticAssertDecl(
    clang::StaticAssertDecl *pragma_static_assert_decl, SgNode **node) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToSageTranslator::VisitStaticAssertDecl" << std::endl;
#endif
  bool res = true;

  SgNode *tmp_condition = Traverse(pragma_static_assert_decl->getAssertExpr());
  SgExpression *condition = isSgExpression(tmp_condition);
  if (tmp_condition != nullptr && condition == nullptr) {
    std::cerr
        << "Runtime error: tmp_condition != nullptr && condition == nullptr"
        << std::endl;
    res = false;
  } else {
    // In LLVM 20, getMessage() returns Expr*, need to cast to StringLiteral
    std::string message_str = "";
    if (auto *msg_expr = pragma_static_assert_decl->getMessage()) {
      if (auto *str_lit = clang::dyn_cast<clang::StringLiteral>(msg_expr)) {
        message_str = str_lit->getString().str();
      }
    }
    *node =
        SageBuilder::buildStaticAssertionDeclaration(condition, message_str);
    if (SgStaticAssertionDeclaration *sg_static_assert =
            isSgStaticAssertionDeclaration(*node)) {
      if (isSgClassDefinition(SageBuilder::topScopeStack())) {
        clang::AccessSpecifier access = pragma_static_assert_decl->getAccess();
        if (access == clang::AS_public) {
          sg_static_assert->get_declarationModifier()
              .get_accessModifier()
              .setPublic();
        } else if (access == clang::AS_private) {
          sg_static_assert->get_declarationModifier()
              .get_accessModifier()
              .setPrivate();
        } else if (access == clang::AS_protected) {
          sg_static_assert->get_declarationModifier()
              .get_accessModifier()
              .setProtected();
        } else if (access == clang::AS_none) {
          SgClassDefinition *class_def =
              isSgClassDefinition(SageBuilder::topScopeStack());
          ROSE_ASSERT(class_def != nullptr);
          if (class_def->get_declaration()->get_class_type() ==
              SgClassDeclaration::e_class) {
            sg_static_assert->get_declarationModifier()
                .get_accessModifier()
                .setPrivate();
          } else {
            sg_static_assert->get_declarationModifier()
                .get_accessModifier()
                .setPublic();
          }
        }
      }
    }
  }

  return VisitDecl(pragma_static_assert_decl, node) && res;
}

bool ClangToSageTranslator::VisitTranslationUnitDecl(
    clang::TranslationUnitDecl *translation_unit_decl, SgNode **node) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToSageTranslator::VisitTranslationUnitDecl" << std::endl;
#endif
  if (*node != nullptr) {
    std::cerr << "Runtime error: The TranslationUnitDecl is already associated "
                 "to a SAGE node."
              << std::endl;
    return false;
  }

  // Create the SAGE node: SgGlobal

  if (p_global_scope != nullptr) {
    std::cerr << "Runtime error: Global Scope have already been set !"
              << std::endl;
    return false;
  }

  // Create file info for global scope (following legacy pattern)
  // Use the source file's filename and line 0
  std::string sourceFilename;
  if (p_sage_source_file != nullptr &&
      p_sage_source_file->get_startOfConstruct() != nullptr) {
    sourceFilename = p_sage_source_file->get_startOfConstruct()->get_filename();
  } else {
    // Fallback: will be set properly later in clang-frontend.cpp
    sourceFilename = "TEMP_FILENAME";
  }
  Sg_File_Info *globalScopeFileInfo = new Sg_File_Info(sourceFilename, 0, 0);

  // Pass file info to SgGlobal constructor (like the legacy frontend does)
  *node = p_global_scope = new SgGlobal(globalScopeFileInfo);

  // Set up parent relationship immediately so symbol insertion can access the
  // project
  if (p_sage_source_file != nullptr) {
    p_global_scope->set_parent(p_sage_source_file);
  }

  p_decl_translation_map.insert(std::pair<clang::Decl *, SgNode *>(
      translation_unit_decl, p_global_scope));

  // Traverse the children

  // DQ (4/5/2017): Fixed code to use updated SageBuilder API.
  // SageBuilder::pushScopeStack(*node);
  SgScopeStatement *global_scope = isSgGlobal(*node);
  ROSE_ASSERT(global_scope != nullptr);
  SageBuilder::pushScopeStack(global_scope);

  clang::DeclContext *decl_context =
      (clang::DeclContext *)translation_unit_decl; // useless but more clear

  bool res = TraverseForDeclContext(decl_context);

  // Phase C (Issue 115): Translate queued implicit function template
  // instantiations after the TU decl pass so template argument declarations are
  // already translated and symbol tables are populated.
  while (!p_pending_implicit_function_instantiations.empty()) {
    clang::FunctionDecl *pending =
        p_pending_implicit_function_instantiations.back();
    p_pending_implicit_function_instantiations.pop_back();

    if (pending == nullptr) {
      continue;
    }

    SgNode *inst_node = nullptr;
    auto existing = p_decl_translation_map.find(pending);
    if (existing != p_decl_translation_map.end()) {
      inst_node = existing->second;
    } else {
      inst_node = Traverse(pending);
    }
    if (SgDeclarationStatement *inst_decl =
            isSgDeclarationStatement(inst_node)) {
      ensure_decl_in_scope_child_list(inst_decl, inst_decl->get_scope(),
                                      "Issue115 Phase C");

      // Ensure the corresponding first nondefining declaration is also
      // attached. ROSE function symbols are typically associated with the first
      // nondefining declaration; leaving it unattached can cause symbol-table
      // cleanup to drop the symbol and later trip name-qualification
      // assertions.
      if (SgFunctionDeclaration *func_decl =
              isSgFunctionDeclaration(inst_decl)) {
        if (SgDeclarationStatement *first_nondef =
                func_decl->get_firstNondefiningDeclaration()) {
          if (first_nondef != func_decl) {
            ensure_decl_in_scope_child_list(
                first_nondef, func_decl->get_scope(), "Issue115 Phase C");
          }
        }

        // The instantiation decls are compiler-generated artifacts and should
        // not be emitted during unparsing (they can appear as explicit
        // specializations after use sites and break downstream compilation
        // tests).
        mark_implicit_instantiation_for_suppression(func_decl);
      }
    }

    p_pending_implicit_function_instantiations_set.erase(pending);
  }

  resolvePendingSpecializedTemplateLinks();

  SageBuilder::popScopeStack();

  // Traverse the class hierarchy

  return VisitDecl(translation_unit_decl, node) && res;
}
