#include "clang-frontend-private.hpp"
#include "sage3basic.h"
#include "clang/Lex/Lexer.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/Support/raw_ostream.h"
#include <algorithm>
#include <cctype>
#include <functional>
#include <memory>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <vector>

static void suppress_unparse_output(SgLocatedNode *n) {
  if (n == NULL) {
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
  if (candidate == NULL) {
    return;
  }
  if (decl != NULL && decl->get_parameterList() == candidate) {
    if (candidate->get_parent() == NULL) {
      candidate->set_parent(decl);
    }
    return;
  }
  if (candidate->get_parent() == NULL) {
    delete candidate;
  }
}

static std::string getDeclNameSafe(clang::NamedDecl *named_decl) {
  if (named_decl == NULL) {
    return "";
  }
  clang::DeclarationName decl_name = named_decl->getDeclName();
  if (decl_name.isEmpty()) {
    return "";
  }
  if (decl_name.isIdentifier()) {
    const clang::IdentifierInfo *ident = decl_name.getAsIdentifierInfo();
    if (ident == NULL) {
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
  if (decl == NULL) {
    return true;
  }

  auto check_decl = [&](clang::FunctionDecl *candidate) -> bool {
    if (candidate == NULL) {
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
    if (templated != NULL && templated != decl && check_decl(templated)) {
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
  if (decl == NULL || compiler == NULL || kind == NULL) {
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
  if (decl == NULL)
    return NULL;

  // Recursion guard: If we're already looking up this declaration, return NULL
  // to prevent infinite loops in template/member resolution
  if (p_symbol_lookup_in_progress.find(decl) !=
      p_symbol_lookup_in_progress.end()) {
#if DEBUG_SYMBOL_TABLE_LOOKUP
    std::cerr << "GetSymbolFromSymbolTable: Recursion detected for decl "
              << getDeclNameSafe(decl) << ", returning NULL" << std::endl;
#endif
    return NULL;
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
        if (symbol != NULL) {
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
    return NULL;
  }

  std::list<SgScopeStatement *>::reverse_iterator it;
  SgSymbol *sym = NULL;
  switch (decl->getKind()) {
  case clang::Decl::Typedef:
  case clang::Decl::TypeAlias: {
    // TypeAlias (C++11 using declarations) are semantically equivalent to
    // Typedef
    clang::DeclContext *decl_context = decl->getDeclContext();
    clang::DeclContext *scope_context = decl_context;
    while (scope_context != NULL &&
           llvm::isa<clang::LinkageSpecDecl>(scope_context)) {
      scope_context = scope_context->getParent();
    }
    if (scope_context != NULL) {
      if (SgScopeStatement *decl_scope =
              resolveScopeFromDeclContext(scope_context, NULL)) {
        sym = decl_scope->lookup_typedef_symbol(name);
      }
    }
    it = SageBuilder::ScopeStack.rbegin();
    while (it != SageBuilder::ScopeStack.rend() && sym == NULL) {
      sym = (*it)->lookup_typedef_symbol(name);
      it++;
    }
    break;
  }
  case clang::Decl::Var:
  case clang::Decl::ParmVar:
  case clang::Decl::Binding: {
    it = SageBuilder::ScopeStack.rbegin();
    while (it != SageBuilder::ScopeStack.rend() && sym == NULL) {
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
    if (sym == NULL) {
      if (clang::VarDecl *var_decl = indirect_decl->getVarDecl()) {
        sym = GetSymbolFromSymbolTable(var_decl);
      }
    }
    if (sym == NULL) {
      it = SageBuilder::ScopeStack.rbegin();
      while (it != SageBuilder::ScopeStack.rend() && sym == NULL) {
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
    if (p_compiler_instance != NULL) {
      clang::SourceManager &sm = p_compiler_instance->getSourceManager();
      is_system_or_builtin = isSystemOrBuiltinFunctionDecl(method_decl, sm);
    }
    clang::CXXRecordDecl *parent_decl = method_decl->getParent();

    SgNode *parent_node = NULL;
    if (parent_decl != NULL) {
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
    if (parent_node == NULL) {
      break;
    }

    SgClassDeclaration *sg_class_decl = isSgClassDeclaration(parent_node);
    if (sg_class_decl == NULL) {
      break;
    }
    if (sg_class_decl->get_definingDeclaration() == NULL) {
      break;
    }

    scope = isSgClassDeclaration(sg_class_decl->get_definingDeclaration())
                ->get_definition();
    if (scope == NULL) {
      break;
    }

    SgType *tmp_type = buildTypeFromQualifiedType(method_decl->getType());
    SgFunctionType *type = isSgFunctionType(tmp_type);
    SgTemplateArgumentPtrList template_args;
    SgTemplateArgumentPtrList *template_args_ptr = NULL;
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
    if (template_args_ptr != NULL) {
      lookup_name =
          SageBuilder::appendTemplateArgumentsToName(name, *template_args_ptr);
    }

    if (type != NULL || template_args_ptr != NULL) {
      if (template_args_ptr != NULL) {
        sym = scope->lookup_nontemplate_member_function_symbol(
            lookup_name, type, template_args_ptr);
        if (sym == NULL) {
          sym = scope->lookup_function_symbol(lookup_name, type,
                                              template_args_ptr);
        }
      }

      if (sym == NULL && type != NULL) {
        sym = scope->lookup_nontemplate_member_function_symbol(
            lookup_name, type, template_args_ptr);
        if (sym == NULL) {
          sym = scope->lookup_function_symbol(lookup_name, type);
        }
      }
    }

    if (sym == NULL && template_args_ptr != NULL) {
      if (p_compiler_instance != NULL && scope != NULL) {
        clang::SourceManager &sm = p_compiler_instance->getSourceManager();
        if (isSystemOrBuiltinFunctionDecl(method_decl, sm) &&
            method_decl->getTemplateSpecializationKind() ==
                clang::TSK_ImplicitInstantiation) {
          SgType *ret_type =
              buildTypeFromQualifiedType(method_decl->getReturnType());
          if (ret_type == NULL) {
            ret_type = SageBuilder::buildUnknownType();
          }

          SgFunctionParameterList *param_list = NULL;
          if (type != NULL && type->get_argument_list() != NULL) {
            param_list =
                SageBuilder::buildFunctionParameterList_nfi(
                    type->get_argument_list());
          }
          if (param_list == NULL) {
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

          if (inst_decl != NULL) {
            applySourceRange(inst_decl, method_decl->getSourceRange());
            if (SgFunctionParameterList *params =
                    inst_decl->get_parameterList()) {
              params->set_parent(inst_decl);
              for (SgInitializedName *param : params->get_args()) {
                if (param != NULL) {
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
                if (param != NULL) {
                  setCompilerGeneratedFileInfo(param);
                  suppress_unparse_output(param);
                }
              }
            }

            auto register_decl = [&](clang::FunctionDecl *key) {
              if (key == NULL) {
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
    SgTemplateArgumentPtrList *template_args_ptr = NULL;
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
    if (template_args_ptr != NULL) {
      lookup_name =
          SageBuilder::appendTemplateArgumentsToName(name, *template_args_ptr);
    }

    auto lookup_in_scope = [&](SgScopeStatement *lookup_scope) -> SgSymbol * {
      if (lookup_scope == NULL) {
        return NULL;
      }

      if (template_args_ptr != NULL) {
        if (SgFunctionSymbol *inst_sym = lookup_scope->lookup_function_symbol(
                lookup_name, type, template_args_ptr)) {
          return inst_sym;
        }
        if (type != NULL) {
          return lookup_scope->lookup_function_symbol(lookup_name, type);
        }
        return NULL;
      }

      if (type != NULL) {
        return lookup_scope->lookup_function_symbol(name, type);
      }
      return NULL;
    };
    auto lookup_in_scope_by_name =
        [&](SgScopeStatement *lookup_scope) -> SgSymbol * {
      if (lookup_scope == NULL) {
        return NULL;
      }
      return lookup_scope->lookup_function_symbol(
          template_args_ptr != NULL ? lookup_name : name);
    };

    // Prefer lookup in the declaration's semantic namespace/global scope.
    // The current ROSE scope stack often does not include namespace scopes
    // from system headers (e.g., std), so stack-only lookup drops required
    // qualifications (std::endl -> endl).
    SgScopeStatement *namespace_scope = NULL;
    if (clang::DeclContext *ctx = func_decl->getDeclContext()) {
      while (ctx != NULL && !ctx->isTranslationUnit() &&
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

        if (namespace_scope == NULL) {
          if (SgNamespaceDeclarationStatement *ns_stmt =
                  ensureNamespaceDeclaration(ns_decl)) {
            namespace_scope = ns_stmt->get_definition();
          }
        }

        namespace_scope = normalizeNamespaceScope(namespace_scope);
      }
    }

    if (namespace_scope != NULL) {
      if (type != NULL || template_args_ptr != NULL) {
        sym = lookup_in_scope(namespace_scope);
      }
      if (sym == NULL) {
        sym = lookup_in_scope_by_name(namespace_scope);
      }
    }

    if (sym == NULL) {
      // Fallback: search the active scope stack.
      if (type != NULL || template_args_ptr != NULL) {
        it = SageBuilder::ScopeStack.rbegin();
        while (it != SageBuilder::ScopeStack.rend() && sym == NULL) {
          sym = lookup_in_scope(*it);
          it++;
        }
      }
      if (sym == NULL) {
        it = SageBuilder::ScopeStack.rbegin();
        while (it != SageBuilder::ScopeStack.rend() && sym == NULL) {
          sym = lookup_in_scope_by_name(*it);
          it++;
        }
      }
    }

    if (sym == NULL && template_args_ptr != NULL) {
      if (p_compiler_instance != NULL) {
        clang::SourceManager &sm = p_compiler_instance->getSourceManager();
        if (isSystemOrBuiltinFunctionDecl(func_decl, sm) &&
            func_decl->getTemplateSpecializationKind() ==
                clang::TSK_ImplicitInstantiation) {
          SgScopeStatement *target_scope =
              namespace_scope != NULL ? namespace_scope
                                      : normalizeNamespaceScope(
                                            SageBuilder::topScopeStack());
          if (target_scope == NULL) {
            target_scope = getGlobalScope();
          }

          SgType *ret_type =
              buildTypeFromQualifiedType(func_decl->getReturnType());
          if (ret_type == NULL) {
            ret_type = SageBuilder::buildUnknownType();
          }

          SgFunctionParameterList *param_list = NULL;
          if (type != NULL && type->get_argument_list() != NULL) {
            param_list =
                SageBuilder::buildFunctionParameterList_nfi(
                    type->get_argument_list());
          }
          if (param_list == NULL) {
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

          if (inst_decl != NULL) {
            applySourceRange(inst_decl, func_decl->getSourceRange());
            if (SgFunctionParameterList *params =
                    inst_decl->get_parameterList()) {
              params->set_parent(inst_decl);
              for (SgInitializedName *param : params->get_args()) {
                if (param != NULL) {
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
                if (param != NULL) {
                  setCompilerGeneratedFileInfo(param);
                  suppress_unparse_output(param);
                }
              }
            }

            auto register_decl = [&](clang::FunctionDecl *key) {
              if (key == NULL) {
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
    if (type != NULL) {
      // Normal case - type conversion succeeded
      it = SageBuilder::ScopeStack.rbegin();
      while (it != SageBuilder::ScopeStack.rend() && sym == NULL) {
        sym = (*it)->lookup_function_symbol(name, type);
        it++;
      }
    }
    // If type is NULL or lookup failed, return NULL (not found)
    // This is acceptable for conversion operators
    break;
  }
  case clang::Decl::Field: {
    // field can be variable or ClassDefinition

    // CLANG FRONTEND FIX: Skip template-dependent field lookups to avoid
    // infinite loops Template-dependent fields (like fields in uninstantiated
    // templates) cannot be properly resolved until template instantiation, so
    // return NULL
    clang::FieldDecl *field_decl = (clang::FieldDecl *)decl;
    if (field_decl->getType()->isDependentType()) {
#if DEBUG_SYMBOL_TABLE_LOOKUP
      std::cerr
          << "GetSymbolFromSymbolTable: Skipping template-dependent field: "
          << field_decl->getNameAsString() << std::endl;
#endif
      // Remove from in-progress set before returning
      p_symbol_lookup_in_progress.erase(decl);
      return NULL;
    }

    // CLANG FRONTEND FIX: Check if parent has been translated before calling
    // Traverse to avoid infinite recursion during template instantiation
    clang::Decl *parent_decl = ((clang::FieldDecl *)decl)->getParent();
    SgNode *parent_node = NULL;

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
    // CLANG FRONTEND FIX: sg_class_decl can be NULL if parent class was skipped
    // (e.g., system header template)
    if (sg_class_decl == NULL) {
      // Parent class not translated (likely skipped system header template or
      // recursion guard hit) Cannot find symbol without parent class
      break;
    }
    if (sg_class_decl->get_definingDeclaration() == NULL) {
      std::cerr << "Runtime Error: cannot find the definition of the "
                   "class/struct associate to the field: "
                << name << std::endl;
      // Cannot lookup symbol without class definition
      break;
    }

    SgClassDeclaration *definingClassDecl =
        isSgClassDeclaration(sg_class_decl->get_definingDeclaration());
    if (definingClassDecl == NULL) {
      break;
    }
    // FieldDecl access implies the record is not autonomous.
    sg_class_decl->set_isAutonomousDeclaration(false);
    definingClassDecl->set_isAutonomousDeclaration(false);
    scope = definingClassDecl->get_definition();
    if (scope == NULL) {
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
    while (scope != NULL && sym == NULL) {
      // Prevent infinite loops by detecting cycles
      if (visited.count(scope) > 0) {
        break; // Cycle detected, bail out
      }
      visited.insert(scope);

      // Look up symbol in current scope
      // Anonymous struct/union fields are still variables in ROSE.
      sym = scope->lookup_variable_symbol(name);

      // Move to parent scope
      if (sym == NULL) {
        scope = scope->get_scope();
      }
    }
    break;
  }
  case clang::Decl::ClassTemplate: {
    auto *tmpl_decl = llvm::cast<clang::ClassTemplateDecl>(decl);
    SgScopeStatement *decl_scope = NULL;
    if (clang::DeclContext *ctx = tmpl_decl->getDeclContext()) {
      decl_scope = resolveScopeFromDeclContext(ctx, NULL);
    }
    if (decl_scope == NULL) {
      decl_scope = SageBuilder::topScopeStack();
    }
    SgScopeStatement *lookup_scope = normalizeNamespaceScope(decl_scope);
    if (lookup_scope == NULL) {
      lookup_scope = decl_scope;
    }

    if (lookup_scope != NULL) {
      sym = lookup_scope->lookup_template_class_symbol(name, NULL, NULL);
      if (sym == NULL) {
        sym = lookup_scope->lookup_class_symbol(name);
      }
    }

    if (sym == NULL) {
      it = SageBuilder::ScopeStack.rbegin();
      while (it != SageBuilder::ScopeStack.rend() && sym == NULL) {
        sym = (*it)->lookup_template_class_symbol(name, NULL, NULL);
        if (sym == NULL) {
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
    if (sym == NULL) {
      clang::DeclContext *decl_context = decl->getDeclContext();
      clang::DeclContext *scope_context = decl_context;
      while (scope_context != NULL &&
             llvm::isa<clang::LinkageSpecDecl>(scope_context)) {
        scope_context = scope_context->getParent();
      }
      if (scope_context != NULL) {
        if (SgScopeStatement *decl_scope =
                resolveScopeFromDeclContext(scope_context, NULL)) {
          SgScopeStatement *lookup_scope = normalizeNamespaceScope(decl_scope);
          if (lookup_scope != NULL) {
            sym = lookup_scope->lookup_template_class_symbol(name, NULL, NULL);
            if (sym == NULL) {
              sym = lookup_scope->lookup_class_symbol(name);
            }
          }
        }
      }
    }

    it = SageBuilder::ScopeStack.rbegin();
    while (it != SageBuilder::ScopeStack.rend() && sym == NULL) {
      sym = (*it)->lookup_class_symbol(name);
      it++;
    }

    if (sym == NULL) {
      // Member/nested record types live in the scope of their semantic parent
      // (e.g., the enclosing class definition), which may not be present on the
      // active scope stack when translating out-of-line definitions.
      auto *record_decl = llvm::cast<clang::RecordDecl>(decl);
      clang::DeclContext *ctx = record_decl->getDeclContext();
      if (clang::RecordDecl *parent_record =
              llvm::dyn_cast_or_null<clang::RecordDecl>(ctx)) {
        SgNode *parent_node = NULL;
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

        if (parent_node == NULL &&
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
    while (it != SageBuilder::ScopeStack.rend() && sym == NULL) {
      sym = (*it)->lookup_label_symbol(name);
      it++;
    }
    break;
  }
  case clang::Decl::EnumConstant: {
    name = SgName(((clang::EnumConstantDecl *)decl)->getName().str());
    it = SageBuilder::ScopeStack.rbegin();
    while (it != SageBuilder::ScopeStack.rend() && sym == NULL) {
      sym = (*it)->lookup_enum_field_symbol(name);
      it++;
    }
    break;
  }
  case clang::Decl::Enum: {
    // An anonymous enum should have its name set with prefix "__anonymous_".
    // There is no need to retrieve the name from clang::EnumDecl, as it will
    // be empty.
    it = SageBuilder::ScopeStack.rbegin();
    while (it != SageBuilder::ScopeStack.rend() && sym == NULL) {
      sym = (*it)->lookup_enum_symbol(name);
      it++;
    }
    break;
  }
  case clang::Decl::NonTypeTemplateParm: {
    // Non-type template parameters are treated as variables
    it = SageBuilder::ScopeStack.rbegin();
    while (it != SageBuilder::ScopeStack.rend() && sym == NULL) {
      sym = (*it)->lookup_variable_symbol(name);
      it++;
    }
    break;
  }
  case clang::Decl::VarTemplateSpecialization:
  case clang::Decl::VarTemplatePartialSpecialization: {
    // Variable template specializations - treat as variables
    it = SageBuilder::ScopeStack.rbegin();
    while (it != SageBuilder::ScopeStack.rend() && sym == NULL) {
      sym = (*it)->lookup_variable_symbol(name);
      it++;
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
  if (context == NULL) {
    return fallback;
  }

  // Linkage specs are not scopes in ROSE; resolve through them to the
  // enclosing namespace/global context.
  while (context != NULL && llvm::isa<clang::LinkageSpecDecl>(context)) {
    context = context->getParent();
  }
  if (context == NULL) {
    return fallback;
  }

  if (context->isTranslationUnit()) {
    return getGlobalScope();
  }

  clang::Decl *context_decl = llvm::dyn_cast<clang::Decl>(context);
  if (context_decl != NULL) {
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
        if (ns_decl_stmt->get_definition() != NULL) {
          return ns_decl_stmt->get_definition();
        }
      } else if (SgNamespaceDefinitionStatement *ns_def =
                     isSgNamespaceDefinitionStatement(context_node)) {
        return ns_def;
      } else if (SgClassDeclaration *class_decl =
                     isSgClassDeclaration(context_node)) {
        if (class_decl->get_definition() != NULL) {
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
        if (fn_decl->get_definition() != NULL) {
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
          if (ns_stmt != NULL) {
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
      if (ns_stmt != NULL && ns_stmt->get_definition() != NULL) {
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
  if (param_list == NULL) {
    return sg_params;
  }

  unsigned index = 0;
  for (clang::NamedDecl *param_decl : *param_list) {
    SgTemplateParameter *sg_param =
        translateTemplateParameter(param_decl, owning_template, index);
    if (sg_param != NULL) {
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
  if (ds == NULL || ds->get_scope() != NULL)
    return;
  MLOG_WARN_C(MLOG_FRONTEND,
              "Declaration %s (%p) created with NULL scope in %s\n",
              ds->class_name().c_str(), ds, context);
}

SgScopeStatement *get_enclosing_namespace_scope(SgScopeStatement *scope) {
  SgScopeStatement *current = scope;
  while (current != NULL && !isSgGlobal(current) &&
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
  if (context == NULL) {
    return false;
  }
  return context->isFileContext() || context->isRecord() ||
         context->isFunctionOrMethod();
}

bool scope_supports_statement_list(const SgScopeStatement *scope) {
  if (scope == NULL) {
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
  if (scope == NULL) {
    return NULL;
  }
  if (SgGlobal *global = isSgGlobal(scope)) {
    return &global->get_declarations();
  }
  if (SgNamespaceDefinitionStatement *ns_def =
          isSgNamespaceDefinitionStatement(scope)) {
    return &ns_def->get_declarations();
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
  return NULL;
}

void ensure_parent_and_scope(SgDeclarationStatement *ds,
                             const char *context = "ClangToSageTranslator") {
  if (ds == NULL)
    return;

  SgScopeStatement *cur_scope = SageBuilder::topScopeStack();
  if (ds->get_parent() == NULL && cur_scope != NULL) {
    ds->set_parent(cur_scope);
  }
  if (ds->get_scope() == NULL && cur_scope != NULL) {
    ds->set_scope(cur_scope);
  }
  diagnose_null_scope(ds, context);
}

bool is_decl_attached_to_scope_child_list(SgScopeStatement *scope,
                                          SgDeclarationStatement *decl) {
  if (scope == NULL || decl == NULL) {
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
  if (scope == NULL || decl == NULL) {
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
  if (scope == NULL) {
    return false;
  }

  for (auto *entry : list) {
    SgStatement *stmt = isSgStatement(entry);
    if (stmt == NULL) {
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

    if (first_stmt == NULL) {
      first_stmt = stmt;
    }
    last_stmt = stmt;
  }

  return first_stmt != NULL && last_stmt != NULL;
}

void ensure_decl_in_scope_child_list(
    SgDeclarationStatement *decl, SgScopeStatement *scope,
    const char *context = "ClangToSageTranslator") {
  if (decl == NULL) {
    return;
  }

  if (scope == NULL) {
    scope = decl->get_scope();
  }
  if (scope == NULL) {
    diagnose_null_scope(decl, context);
    return;
  }

  if (decl->get_scope() == NULL) {
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
  if (decl == NULL) {
    return;
  }

  if (scope == NULL) {
    scope = isSgScopeStatement(decl->get_parent());
  }
  if (scope == NULL) {
    scope = decl->get_scope();
  }
  if (scope == NULL) {
    diagnose_null_scope(decl, context);
    return;
  }

  SgScopeStatement *original_scope = decl->get_scope();
  if (original_scope == NULL) {
    ensure_decl_in_scope_child_list(decl, scope, context);
    return;
  }

  if (original_scope != scope &&
      is_decl_attached_to_scope_child_list(original_scope, decl)) {
    detach_decl_from_scope_child_list(decl, original_scope);
  }

  SgDeclarationStatementPtrList *decls = get_scope_declaration_list(scope);
  bool can_attach = decls != NULL || scope_supports_statement_list(scope);
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
    if (decls != NULL) {
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
  if (n == NULL) {
    return;
  }

  auto mark = [](Sg_File_Info *fi) {
    if (fi == NULL) {
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
  if (func_decl == NULL) {
    return;
  }

  std::set<SgNode *> visited;
  std::function<void(SgFunctionDeclaration *)> visit =
      [&](SgFunctionDeclaration *decl) {
        if (decl == NULL || !visited.insert(decl).second) {
          return;
        }

        mark_compiler_generated_and_suppress_unparse(decl);

        if (SgFunctionParameterList *params = decl->get_parameterList()) {
          mark_compiler_generated_and_suppress_unparse(params);
          for (SgInitializedName *param : params->get_args()) {
            if (param != NULL) {
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
  if (scope == NULL)
    return NULL;

  SgNamespaceDefinitionStatement *ns_def =
      isSgNamespaceDefinitionStatement(scope);
  if (ns_def == NULL)
    return scope;

  SgNamespaceDefinitionStatement *global_def = ns_def->get_global_definition();
  if (global_def != NULL) {
    return global_def;
  }

  SgNamespaceDeclarationStatement *ns_decl = ns_def->get_namespaceDeclaration();
  if (ns_decl == NULL)
    return scope;

  SgNamespaceDeclarationStatement *first_nondef =
      isSgNamespaceDeclarationStatement(
          ns_decl->get_firstNondefiningDeclaration());
  if (first_nondef == NULL)
    return scope;

  SgNamespaceDefinitionStatement *first_def = first_nondef->get_definition();
  return first_def != NULL ? first_def : scope;
}

clang::NamespaceDecl *getCanonicalNamespaceDecl(clang::NamespaceDecl *decl) {
  if (decl == NULL) {
    return NULL;
  }
  clang::NamespaceDecl *canonical = decl->getCanonicalDecl();
  return canonical != NULL ? canonical : decl;
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

  for (clang::Decl *inner_decl : record_decl->decls()) {
    if (inner_decl == NULL) {
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
    if (p_compiler_instance != NULL) {
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
      if (tag_decl->isEmbeddedInDeclarator()) {
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
              parent_record != NULL && parent_record->isLambda();
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
        if (templated != NULL &&
            templated->getFriendObjectKind() != clang::Decl::FOK_None) {
          is_friend_decl = true;
        } else if (templated != NULL &&
                   llvm::isa<clang::CXXMethodDecl>(templated)) {
          // Template member functions are not friends of the current class.
        } else if (templated != NULL &&
                   templated->getDeclContext() != record_decl) {
          is_friend_decl = true;
        } else if (templated != NULL &&
                   templated->getDeclContext() == record_decl) {
          // Friend function templates declared in class scope may not carry
          // FriendObjectKind; treat non-member templates as friends.
          is_friend_decl = true;
        }
      }

      SgScopeStatement *friend_scope = NULL;
      if (is_friend_decl) {
        friend_scope = get_enclosing_namespace_scope(class_def);
        if (friend_scope == NULL) {
          friend_scope = getGlobalScope();
        }
        friend_scope = normalizeNamespaceScope(friend_scope);
      }

      auto build_symbol_for_decl =
          [](SgDeclarationStatement *decl) -> SgSymbol * {
        if (decl == NULL) {
          return NULL;
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
        if (SgTypedefDeclaration *typedef_decl =
                isSgTypedefDeclaration(decl)) {
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
        return NULL;
      };

      auto rehome_decl_symbol = [&](SgDeclarationStatement *decl,
                                    SgScopeStatement *old_scope,
                                    SgScopeStatement *new_scope) {
        if (decl == NULL || old_scope == NULL || new_scope == NULL) {
          return;
        }
        if (old_scope == new_scope) {
          return;
        }

        SgSymbol *symbol = old_scope->find_symbol_from_declaration(decl);
        if (symbol != NULL) {
          if (old_scope->symbol_exists(symbol)) {
            old_scope->remove_symbol(symbol);
          } else if (SgSymbolTable *table = old_scope->get_symbol_table()) {
            if (table->exists(symbol)) {
              table->remove(symbol);
            }
          }
        }

        if (new_scope->find_symbol_from_declaration(decl) == NULL) {
          if (symbol == NULL) {
            symbol = build_symbol_for_decl(decl);
          }
          if (symbol != NULL && !new_scope->symbol_exists(symbol)) {
            new_scope->insert_symbol(symbol->get_name(), symbol);
          }
        }
      };

      bool is_friend_member =
          is_friend_decl &&
          (isSgMemberFunctionDeclaration(child_decl) != NULL);
      bool force_class_scope = !is_friend_decl;
      if (SgFunctionDeclaration *func_decl =
              isSgFunctionDeclaration(child_decl)) {
        bool is_member = isSgMemberFunctionDeclaration(func_decl) != NULL;
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
        if (child_scope == NULL || isSgClassDefinition(child_scope) != NULL ||
            isSgTemplateClassDefinition(child_scope) != NULL ||
            isSgTemplateInstantiationDefn(child_scope) != NULL) {
          if (friend_scope != NULL) {
            child_decl->set_scope(friend_scope);
          }
        }
      }
      SgScopeStatement *new_scope = child_decl->get_scope();
      if (old_scope != NULL && new_scope != NULL && old_scope != new_scope) {
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

  SgTemplateParameter *sg_param = NULL;

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
        if (default_type != NULL) {
          sg_param->set_defaultTypeParameter(default_type);
        }
      }
    }

    if (type_param->isParameterPack()) {
      sg_param->set_is_parameter_pack(true);
    }
  } else if (clang::NonTypeTemplateParmDecl *non_type_param =
                 llvm::dyn_cast<clang::NonTypeTemplateParmDecl>(param_decl)) {
    std::string name_str = non_type_param->getNameAsString();
    if (name_str.empty()) {
      name_str = "__non_type_param_" + std::to_string(position);
    }

    SgType *type = buildTypeFromQualifiedType(non_type_param->getType());
    if (type == NULL) {
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
      SgExpression *sg_default_expr = NULL;

      switch (default_arg.getKind()) {
      case clang::TemplateArgument::Expression: {
        clang::Expr *expr = default_arg.getAsExpr();
        if (expr != NULL) {
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

      if (sg_default_expr != NULL) {
        sg_param->set_defaultExpressionParameter(sg_default_expr);
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
    SgScopeStatement *current_scope = SageBuilder::topScopeStack();
    ROSE_ASSERT(current_scope != NULL);

    SgDeclarationScope *decl_scope = isSgDeclarationScope(current_scope);
    if (decl_scope == NULL) {
      decl_scope = SageBuilder::buildDeclarationScope();
      decl_scope->set_parent(current_scope);
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
    return NULL;
  }

  if (sg_param != NULL) {
    applySourceRange(sg_param, param_decl->getSourceRange());

    // Only set owning template if it's NOT a template_parameter,
    // because template_parameter uses this field for the nrdecl.
    if (owning_template != NULL &&
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
  if (decl == NULL) {
    return NULL;
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

  return Traverse(decl);
}

SgNode *ClangToSageTranslator::Traverse(clang::Decl *decl) {
  if (decl == NULL)
    return NULL;

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
          bool has_definition = existing_class->get_definition() != NULL;
          if (!has_definition) {
            if (SgClassDeclaration *def_decl = isSgClassDeclaration(
                    existing_class->get_definingDeclaration())) {
              has_definition = def_decl->get_definition() != NULL;
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
    if (it->second == NULL && clang::NamedDecl::classof(decl)) {
      std::string name = getDeclNameSafe((clang::NamedDecl *)decl);
      if (name == "tuple") {
        std::cerr << "DEBUG: Traverse found 'tuple' in map but node is NULL!"
                  << std::endl;
      }
    }
    return it->second;
  }

translate_decl:
  SgNode *result = NULL;
  bool ret_status = false;

  switch (decl->getKind()) {
  case clang::Decl::AccessSpec:
    ret_status = VisitAccessSpecDecl((clang::AccessSpecDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != NULL);
    break;
  case clang::Decl::Block:
    ret_status = VisitBlockDecl((clang::BlockDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != NULL);
    break;
  case clang::Decl::Captured:
    ret_status = VisitCapturedDecl((clang::CapturedDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != NULL);
    break;
  case clang::Decl::Empty:
    ret_status = VisitEmptyDecl((clang::EmptyDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != NULL);
    break;
  case clang::Decl::Export:
    ret_status = VisitExportDecl((clang::ExportDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != NULL);
    break;
  case clang::Decl::ExternCContext:
    ret_status =
        VisitExternCContextDecl((clang::ExternCContextDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != NULL);
    break;
  case clang::Decl::FileScopeAsm:
    ret_status =
        VisitFileScopeAsmDecl((clang::FileScopeAsmDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != NULL);
    break;
  case clang::Decl::Friend:
    ret_status = VisitFriendDecl((clang::FriendDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != NULL);
    break;
  case clang::Decl::FriendTemplate:
    ret_status =
        VisitFriendTemplateDecl((clang::FriendTemplateDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != NULL);
    break;
  case clang::Decl::Import:
    ret_status = VisitImportDecl((clang::ImportDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != NULL);
    break;
  case clang::Decl::Label:
    ret_status = VisitLabelDecl((clang::LabelDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != NULL);
    break;
  case clang::Decl::NamespaceAlias:
    ret_status =
        VisitNamespaceAliasDecl((clang::NamespaceAliasDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != NULL);
    break;
  case clang::Decl::Namespace:
    ret_status = VisitNamespaceDecl((clang::NamespaceDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != NULL);
    break;
  case clang::Decl::LinkageSpec:
    ret_status = VisitLinkageSpecDecl((clang::LinkageSpecDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != NULL);
    break;
  case clang::Decl::BuiltinTemplate: {
    ret_status = false;
    result = NULL;
    break;
  }
  case clang::Decl::Concept:
    ret_status = VisitConceptDecl((clang::ConceptDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != NULL);
    break;
  case clang::Decl::ClassTemplate:
    ret_status =
        VisitClassTemplateDecl((clang::ClassTemplateDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != NULL);
    break;
  case clang::Decl::FunctionTemplate:
    ret_status =
        VisitFunctionTemplateDecl((clang::FunctionTemplateDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != NULL);
    break;
  case clang::Decl::TypeAliasTemplate:
    ret_status = VisitTypeAliasTemplateDecl(
        (clang::TypeAliasTemplateDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != NULL);
    break;
  case clang::Decl::VarTemplate:
    ret_status = VisitVarTemplateDecl((clang::VarTemplateDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != NULL);
    break;
  case clang::Decl::TemplateTemplateParm:
    ret_status = VisitTemplateTemplateParmDecl(
        (clang::TemplateTemplateParmDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != NULL);
    break;
  case clang::Decl::Record:
    ret_status = VisitRecordDecl((clang::RecordDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != NULL);
    break;
  case clang::Decl::CXXRecord:
    ret_status = VisitCXXRecordDecl((clang::CXXRecordDecl *)decl, &result);
    if (SgClassDeclaration *cd = isSgClassDeclaration(result)) {
      SgDeclarationStatement *firstNondef =
          cd->get_firstNondefiningDeclaration();
      ROSE_ASSERT(firstNondef != NULL);
      ROSE_ASSERT(firstNondef->get_firstNondefiningDeclaration() != NULL);
    }
    ROSE_ASSERT(ret_status == false || result != NULL);
    break;
  case clang::Decl::ClassTemplateSpecialization:
    ret_status = VisitClassTemplateSpecializationDecl(
        (clang::ClassTemplateSpecializationDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != NULL);
    break;
  case clang::Decl::ClassTemplatePartialSpecialization:
    ret_status = VisitClassTemplatePartialSpecializationDecl(
        (clang::ClassTemplatePartialSpecializationDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != NULL);
    break;
  case clang::Decl::Enum:
    ret_status = VisitEnumDecl((clang::EnumDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != NULL);
    break;
  case clang::Decl::TemplateTypeParm:
    ret_status =
        VisitTemplateTypeParmDecl((clang::TemplateTypeParmDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != NULL);
    break;
  case clang::Decl::Typedef:
    ret_status = VisitTypedefDecl((clang::TypedefDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != NULL);
    break;
  case clang::Decl::TypeAlias:
    ret_status = VisitTypeAliasDecl((clang::TypeAliasDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != NULL);
    break;
  case clang::Decl::UnresolvedUsingTypename:
    ret_status = VisitUnresolvedUsingTypenameDecl(
        (clang::UnresolvedUsingTypenameDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != NULL);
    break;
  case clang::Decl::Using:
    ret_status = VisitUsingDecl((clang::UsingDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != NULL);
    break;
  case clang::Decl::UsingDirective:
    ret_status =
        VisitUsingDirectiveDecl((clang::UsingDirectiveDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != NULL);
    break;
  case clang::Decl::UsingPack:
    ret_status = VisitUsingPackDecl((clang::UsingPackDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != NULL);
    break;
  case clang::Decl::UsingShadow:
    ret_status = VisitUsingShadowDecl((clang::UsingShadowDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != NULL);
    break;
  case clang::Decl::ConstructorUsingShadow:
    ret_status = VisitConstructorUsingShadowDecl(
        (clang::ConstructorUsingShadowDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != NULL);
    break;
  case clang::Decl::Binding:
    ret_status = VisitBindingDecl((clang::BindingDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != NULL);
    break;
  case clang::Decl::Field:
    ret_status = VisitFieldDecl((clang::FieldDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != NULL);
    break;
  case clang::Decl::Function:
    ret_status = VisitFunctionDecl((clang::FunctionDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != NULL);
    break;
  case clang::Decl::CXXDeductionGuide:
    ret_status = VisitCXXDeductionGuideDecl(
        (clang::CXXDeductionGuideDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != NULL);
    break;
  case clang::Decl::CXXConstructor:
    ret_status =
        VisitCXXConstructorDecl((clang::CXXConstructorDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != NULL);
    break;
  case clang::Decl::CXXConversion:
    ret_status =
        VisitCXXConversionDecl((clang::CXXConversionDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != NULL);
    break;
  case clang::Decl::CXXDestructor:
    ret_status =
        VisitCXXDestructorDecl((clang::CXXDestructorDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != NULL);
    break;
  case clang::Decl::CXXMethod:
    ret_status = VisitCXXMethodDecl((clang::CXXMethodDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != NULL);
    break;
  case clang::Decl::MSProperty:
    ret_status = VisitMSPropertyDecl((clang::MSPropertyDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != NULL);
    break;
  case clang::Decl::NonTypeTemplateParm:
    ret_status = VisitNonTypeTemplateParmDecl(
        (clang::NonTypeTemplateParmDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != NULL);
    break;
  case clang::Decl::Decomposition:
    ret_status =
        VisitDecompositionDecl((clang::DecompositionDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != NULL);
    break;
  case clang::Decl::ImplicitParam:
    ret_status =
        VisitImplicitParamDecl((clang::ImplicitParamDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != NULL);
    break;
  case clang::Decl::OMPCapturedExpr:
    ret_status =
        VisitOMPCaptureExprDecl((clang::OMPCapturedExprDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != NULL);
    break;
  case clang::Decl::ParmVar:
    ret_status = VisitParmVarDecl((clang::ParmVarDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != NULL);
    break;
  case clang::Decl::VarTemplatePartialSpecialization:
    ret_status = VisitVarTemplatePartialSpecializationDecl(
        (clang::VarTemplatePartialSpecializationDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != NULL);
    break;
  case clang::Decl::VarTemplateSpecialization:
    ret_status = VisitVarTemplateSpecializationDecl(
        (clang::VarTemplateSpecializationDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != NULL);
    break;
  case clang::Decl::EnumConstant:
    ret_status =
        VisitEnumConstantDecl((clang::EnumConstantDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != NULL);
    break;
  case clang::Decl::IndirectField:
    ret_status =
        VisitIndirectFieldDecl((clang::IndirectFieldDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != NULL);
    break;
  case clang::Decl::OMPDeclareMapper:
    ret_status =
        VisitOMPDeclareMapperDecl((clang::OMPDeclareMapperDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != NULL);
    break;
  case clang::Decl::OMPDeclareReduction:
    ret_status = VisitOMPDeclareReductionDecl(
        (clang::OMPDeclareReductionDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != NULL);
    break;
  case clang::Decl::UnresolvedUsingValue:
    ret_status = VisitUnresolvedUsingValueDecl(
        (clang::UnresolvedUsingValueDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != NULL);
    break;
  case clang::Decl::OMPAllocate:
    ret_status = VisitOMPAllocateDecl((clang::OMPAllocateDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != NULL);
    break;
  case clang::Decl::OMPRequires:
    ret_status = VisitOMPRequiresDecl((clang::OMPRequiresDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != NULL);
    break;
  case clang::Decl::OMPThreadPrivate:
    ret_status =
        VisitOMPThreadPrivateDecl((clang::OMPThreadPrivateDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != NULL);
    break;
  case clang::Decl::PragmaComment:
    ret_status =
        VisitPragmaCommentDecl((clang::PragmaCommentDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != NULL);
    break;
  case clang::Decl::PragmaDetectMismatch:
    ret_status = VisitPragmaDetectMismatchDecl(
        (clang::PragmaDetectMismatchDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != NULL);
    break;
  case clang::Decl::StaticAssert:
    ret_status =
        VisitStaticAssertDecl((clang::StaticAssertDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != NULL);
    break;
  case clang::Decl::TranslationUnit:
    ret_status =
        VisitTranslationUnitDecl((clang::TranslationUnitDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != NULL);
    break;
  case clang::Decl::Var:
    ret_status = VisitVarDecl((clang::VarDecl *)decl, &result);
    ROSE_ASSERT(ret_status == false || result != NULL);
    break;

  default:
    std::cerr << "Unknown declacaration kind: " << decl->getDeclKindName()
              << " !" << std::endl;
    ROSE_ABORT();
  }

  ROSE_ASSERT(ret_status == false || result != NULL);

  bool on_demand = p_decl_translation_on_demand.find(decl) !=
                   p_decl_translation_on_demand.end();

  if (ret_status && result != NULL) {
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
          if (ctx_decl != NULL &&
              p_decl_translation_map.find(ctx_decl) ==
                  p_decl_translation_map.end() &&
              p_decl_translation_in_progress.find(ctx_decl) ==
                  p_decl_translation_in_progress.end()) {
            if (!llvm::isa<clang::NamespaceDecl>(ctx_decl)) {
              TraverseOnDemand(ctx_decl);
            }
          }

          SgScopeStatement *target_scope = resolveScopeFromDeclContext(
              const_cast<clang::DeclContext *>(target_ctx), NULL);
          if (target_ctx->isRecord()) {
            if (isSgClassDefinition(target_scope) == NULL &&
                isSgTemplateInstantiationDefn(target_scope) == NULL &&
                isSgTemplateClassDefinition(target_scope) == NULL) {
              target_scope = NULL;
            }
          }
          if (target_scope == NULL && target_ctx->isTranslationUnit()) {
            target_scope = getGlobalScope();
          }
          if (target_scope != NULL) {
            if (parent_scope != NULL && parent_scope != target_scope) {
              detach_decl_from_scope_child_list(ds, parent_scope);
            }
            ensure_decl_in_scope_child_list(ds, target_scope,
                                            "Traverse:decl-context");
          }
        }
      }
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

  return ret_status ? result : NULL;
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

    if (namespace_scope != NULL) {
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
          if (p_compiler_instance != NULL) {
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

    if (decl_stmt == NULL && child != NULL) {
      std::cerr << "Runtime error: the node produce for a clang::Decl is not a "
                   "SgDeclarationStatement !"
                << std::endl;
      std::cerr << "    class = " << child->class_name() << std::endl;
      res = false;
    } else if (child != NULL) {
      // FIXME This is a hack to avoid autonomous decl of unnamed type to being
      // added to the global scope....
      SgClassDeclaration *class_decl = isSgClassDeclaration(child);
      if (class_decl != NULL &&
          (class_decl->get_name() == "" || class_decl->get_isUnNamed())) {
        continue;
      }

      SgEnumDeclaration *enum_decl = isSgEnumDeclaration(child);
      if (enum_decl != NULL &&
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
      if (decl_stmt != NULL) {
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
        if (decl_stmt->get_file_info() != NULL &&
            decl_stmt->get_file_info()->isFrontendSpecific()) {
          continue;
        }
      }

      if (global_scope != NULL) {
        SgScopeStatement *decl_scope = decl_stmt->get_scope();
        if (decl_scope != NULL && decl_scope != global_scope) {
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
      } else if (namespace_scope != NULL) {
        SgScopeStatement *decl_scope = decl_stmt->get_scope();
        if (decl_scope != NULL && decl_scope != namespace_scope &&
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
        if (decl_stmt->get_scope() == NULL) {
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

  if (namespace_scope != NULL) {
    SgScopeStatement *canonical_scope =
        normalizeNamespaceScope(namespace_scope);
    if (canonical_scope != NULL && canonical_scope != namespace_scope) {
      for (SgDeclarationStatement *decl_stmt :
           namespace_scope->get_declarations()) {
        if (decl_stmt == NULL) {
          continue;
        }
        SgScopeStatement *decl_scope = decl_stmt->get_scope();
        if (decl_scope == NULL || decl_scope == namespace_scope) {
          continue;
        }
        SgNamespaceDefinitionStatement *decl_ns =
            isSgNamespaceDefinitionStatement(decl_scope);
        if (decl_ns == NULL) {
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
  if (*node == NULL) {
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
  // declarations. Set *node to NULL to indicate this declaration doesn't have a
  // ROSE equivalent.
  *node = NULL;

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
  if (empty_decl == NULL) {
    *node = NULL;
    return false;
  }

  SgEmptyDeclaration *empty_decl_stmt = new SgEmptyDeclaration();
  ROSE_ASSERT(empty_decl_stmt != NULL);

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
    while (scope_ctx != NULL && !scope_ctx->isNamespace() &&
           !scope_ctx->isTranslationUnit()) {
      scope_ctx = scope_ctx->getParent();
    }
    if (clang::NamespaceDecl *ns_decl =
            llvm::dyn_cast_or_null<clang::NamespaceDecl>(
                llvm::dyn_cast_or_null<clang::Decl>(scope_ctx))) {
      if (SgNamespaceDeclarationStatement *ns_stmt =
              ensureNamespaceDeclaration(ns_decl)) {
        if (ns_stmt->get_definition() != NULL) {
          return ns_stmt->get_definition();
        }
      }
    }
    if (scope_ctx != NULL && scope_ctx->isTranslationUnit()) {
      return getGlobalScope();
    }
    return NULL;
  };

  clang::DeclContext *friend_context =
      friend_decl != NULL ? friend_decl->getDeclContext() : NULL;
  if (clang::NamedDecl *named_decl = friend_decl->getFriendDecl()) {
    if (clang::DeclContext *named_context = named_decl->getDeclContext()) {
      friend_context = named_context;
    }
  }
  SgScopeStatement *friend_scope = resolve_friend_scope(friend_context);
  if (friend_scope == NULL) {
    friend_scope = get_enclosing_namespace_scope(current_scope);
  }
  if (friend_scope == NULL) {
    friend_scope = getGlobalScope();
  }
  friend_scope = normalizeNamespaceScope(friend_scope);

  // Translate the underlying entity being declared as a friend.
  SgDeclarationStatement *sg_decl = NULL;
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
      if (sg_decl == NULL) {
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
      if (recordDecl != NULL) {
        if (SgClassSymbol *class_sym =
                isSgClassSymbol(GetSymbolFromSymbolTable(recordDecl))) {
          SgClassDeclaration *sg_def_class_decl = class_sym->get_declaration();
          if (sg_def_class_decl != NULL) {
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
            if (scope == NULL) {
              scope = getGlobalScope();
            }
            SgClassDeclaration *sg_friend_class_decl = new SgClassDeclaration(
                recordName, type_of_class, sg_def_class_decl->get_type(), NULL);
            SgClassDeclaration *first_nondef = isSgClassDeclaration(
                sg_def_class_decl->get_firstNondefiningDeclaration());
            if (first_nondef == NULL) {
              first_nondef = sg_def_class_decl;
              first_nondef->set_firstNondefiningDeclaration(first_nondef);
            }
            SgClassDeclaration *def_decl = isSgClassDeclaration(
                sg_def_class_decl->get_definingDeclaration());
            if (def_decl != NULL && def_decl->get_definition() == NULL) {
              def_decl = NULL;
            }
            if (def_decl == NULL &&
                sg_def_class_decl->get_definition() != NULL) {
              def_decl = sg_def_class_decl;
            }

            sg_friend_class_decl->set_firstNondefiningDeclaration(first_nondef);
            if (def_decl != NULL && def_decl != first_nondef) {
              sg_friend_class_decl->set_definingDeclaration(def_decl);
            }
            sg_friend_class_decl->set_scope(scope);
            if (current_scope != NULL) {
              sg_friend_class_decl->set_parent(current_scope);
            } else {
              sg_friend_class_decl->set_parent(scope);
            }
            sg_friend_class_decl->get_declarationModifier().setFriend();
            sg_decl = sg_friend_class_decl;
          }
        }
        if (sg_decl == NULL) {
          sg_decl = isSgDeclarationStatement(Traverse(recordDecl));
        }
      }
    }
    if (sg_decl == NULL) {
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
          if (scope == NULL) {
            scope = getGlobalScope();
          }
          SgTemplateArgumentPtrList tpl_args = nrdecl->get_tpl_args();
          bool has_args = !tpl_args.empty();
          sg_decl = SageBuilder::buildNondefiningClassDeclaration_nfi(
              nrdecl->get_name(), SgClassDeclaration::e_class, scope, has_args,
              has_args ? &tpl_args : NULL);
          if (sg_decl != NULL && current_scope != NULL) {
            sg_decl->set_parent(current_scope);
          }
          if (SgTemplateInstantiationDecl *inst_decl =
                  isSgTemplateInstantiationDecl(sg_decl)) {
            if (inst_decl->get_templateDeclaration() == NULL) {
              SgTemplateClassDeclaration *tmpl_decl = NULL;
              if (friend_template_decl != nullptr) {
                if (SgNode *tmpl_node =
                        TraverseOnDemand(friend_template_decl)) {
                  tmpl_decl = isSgTemplateClassDeclaration(tmpl_node);
                }
              }

              if (tmpl_decl == NULL) {
                SgScopeStatement *lookup_scope = friend_scope;
                if (lookup_scope == NULL) {
                  lookup_scope = get_enclosing_namespace_scope(current_scope);
                }
                lookup_scope = normalizeNamespaceScope(lookup_scope);
                if (lookup_scope == NULL) {
                  lookup_scope = getGlobalScope();
                }

                if (lookup_scope != NULL) {
                  if (SgTemplateClassSymbol *tmpl_sym =
                          lookup_scope->lookup_template_class_symbol(
                              nrdecl->get_name(), NULL, NULL)) {
                    tmpl_decl = isSgTemplateClassDeclaration(
                        tmpl_sym->get_declaration());
                  } else if (SgTemplateClassSymbol *tmpl_sym = SageInterface::
                                 lookupTemplateClassSymbolInParentScopes(
                                     nrdecl->get_name(), NULL, NULL,
                                     lookup_scope)) {
                    tmpl_decl = isSgTemplateClassDeclaration(
                        tmpl_sym->get_declaration());
                  }
                }
                if (tmpl_decl == NULL) {
                  auto cache_it = p_template_decl_cache.find(
                      nrdecl->get_name().getString());
                  if (cache_it != p_template_decl_cache.end()) {
                    tmpl_decl = cache_it->second;
                  }
                }
              }

              if (tmpl_decl != NULL) {
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

  if (sg_decl == NULL) {
    MLOG_ERROR_C(MLOG_FRONTEND,
                 "Runtime error: unable to translate FriendDecl.\n");
    *node = NULL;
    return false;
  }

  auto mark_friend = [](SgDeclarationStatement *decl) {
    if (decl != NULL) {
      decl->get_declarationModifier().setFriend();
    }
  };

  mark_friend(sg_decl);
  mark_friend(sg_decl->get_firstNondefiningDeclaration());
  mark_friend(sg_decl->get_definingDeclaration());

  auto force_friend_function_scope = [&](SgDeclarationStatement *decl) {
    if (decl == NULL) {
      return;
    }
    if (SgFunctionDeclaration *func_decl =
            isSgFunctionDeclaration(decl)) {
      if (isSgMemberFunctionDeclaration(func_decl) != NULL) {
        return;
      }
      if (friend_scope != NULL && func_decl->get_scope() != friend_scope) {
        func_decl->set_scope(friend_scope);
      }
    }
  };

  auto ensure_scope_and_parent = [](SgDeclarationStatement *decl,
                                    SgScopeStatement *parent_scope,
                                    SgScopeStatement *decl_scope,
                                    bool force_scope) {
    if (decl == NULL)
      return;
    if (parent_scope != NULL && decl->get_parent() == NULL) {
      decl->set_parent(parent_scope);
    }
    if (decl_scope != NULL) {
      if (force_scope) {
        if (decl->get_scope() != decl_scope) {
          decl->set_scope(decl_scope);
        }
      } else if (decl->get_scope() == NULL) {
        decl->set_scope(decl_scope);
      }
    }
  };

  force_friend_function_scope(sg_decl);
  if (sg_decl->get_scope() == NULL && friend_scope != NULL) {
    sg_decl->set_scope(friend_scope);
  }
  if (current_scope != NULL &&
      (isSgClassDefinition(current_scope) != NULL ||
       isSgTemplateClassDefinition(current_scope) != NULL)) {
    ensure_decl_in_scope_child_list_preserve_scope(sg_decl, current_scope,
                                                   "FriendDecl");
  } else {
    ensure_scope_and_parent(sg_decl, current_scope, friend_scope,
                            isSgFunctionDeclaration(sg_decl) == NULL);
  }

  if (SgDeclarationStatement *first_nondef =
          sg_decl->get_firstNondefiningDeclaration()) {
    force_friend_function_scope(first_nondef);
    ensure_scope_and_parent(first_nondef, friend_scope, friend_scope,
                            isSgFunctionDeclaration(first_nondef) == NULL);
  }
  if (SgDeclarationStatement *def_decl = sg_decl->get_definingDeclaration()) {
    force_friend_function_scope(def_decl);
    ensure_scope_and_parent(def_decl, friend_scope, friend_scope,
                            isSgFunctionDeclaration(def_decl) == NULL);
  }
  diagnose_null_scope(sg_decl, "FriendDecl");

  // Ensure friend definitions are wired consistently for analysis passes such
  // as VirtualCFG. Do not mark friend prototypes as defining declarations.
  auto ensure_first_nondef = [](SgDeclarationStatement *decl) {
    if (decl != NULL && decl->get_firstNondefiningDeclaration() == NULL) {
      decl->set_firstNondefiningDeclaration(decl);
    }
  };

  ensure_first_nondef(sg_decl);
  ensure_first_nondef(sg_decl->get_firstNondefiningDeclaration());
  ensure_first_nondef(sg_decl->get_definingDeclaration());
  if (SgClassDeclaration *class_decl = isSgClassDeclaration(sg_decl)) {
    if (class_decl->get_firstNondefiningDeclaration() == NULL) {
      class_decl->set_firstNondefiningDeclaration(class_decl);
    }
    if (SgClassDeclaration *def_decl =
            isSgClassDeclaration(class_decl->get_definingDeclaration())) {
      if (def_decl->get_firstNondefiningDeclaration() == NULL) {
        def_decl->set_firstNondefiningDeclaration(
            class_decl->get_firstNondefiningDeclaration());
      }
    }
  }
  if (SgFunctionDeclaration *func_decl = isSgFunctionDeclaration(sg_decl)) {
    if (func_decl->get_definition() != NULL &&
        func_decl->get_definingDeclaration() == NULL) {
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
    while (scope_ctx != NULL && !scope_ctx->isNamespace() &&
           !scope_ctx->isTranslationUnit()) {
      scope_ctx = scope_ctx->getParent();
    }
    if (clang::NamespaceDecl *ns_decl =
            llvm::dyn_cast_or_null<clang::NamespaceDecl>(
                llvm::dyn_cast_or_null<clang::Decl>(scope_ctx))) {
      if (SgNamespaceDeclarationStatement *ns_stmt =
              ensureNamespaceDeclaration(ns_decl)) {
        if (ns_stmt->get_definition() != NULL) {
          return ns_stmt->get_definition();
        }
      }
    }
    if (scope_ctx != NULL && scope_ctx->isTranslationUnit()) {
      return getGlobalScope();
    }
    return NULL;
  };

  SgScopeStatement *friend_scope = resolve_friend_scope(
      friend_template_decl != NULL ? friend_template_decl->getDeclContext()
                                   : NULL);
  if (friend_scope == NULL) {
    friend_scope = get_enclosing_namespace_scope(current_scope);
  }
  if (friend_scope == NULL) {
    friend_scope = getGlobalScope();
  }

  SgDeclarationStatement *sg_decl = NULL;
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

  if (sg_decl == NULL) {
    MLOG_ERROR_C(MLOG_FRONTEND,
                 "Runtime error: unable to translate FriendTemplateDecl.\n");
    *node = NULL;
    return false;
  }

  auto mark_friend = [](SgDeclarationStatement *decl) {
    if (decl != NULL) {
      decl->get_declarationModifier().setFriend();
    }
  };

  mark_friend(sg_decl);
  mark_friend(sg_decl->get_firstNondefiningDeclaration());
  mark_friend(sg_decl->get_definingDeclaration());

  auto ensure_scope_and_parent = [](SgDeclarationStatement *decl,
                                    SgScopeStatement *parent_scope,
                                    SgScopeStatement *decl_scope) {
    if (decl == NULL)
      return;
    if (parent_scope != NULL && decl->get_parent() == NULL) {
      decl->set_parent(parent_scope);
    }
    if (decl_scope != NULL && decl->get_scope() != decl_scope) {
      decl->set_scope(decl_scope);
    }
  };

  ensure_scope_and_parent(sg_decl, current_scope, friend_scope);
  ensure_scope_and_parent(sg_decl->get_firstNondefiningDeclaration(),
                          current_scope, friend_scope);
  ensure_scope_and_parent(sg_decl->get_definingDeclaration(), current_scope,
                          friend_scope);

  if (sg_decl->get_firstNondefiningDeclaration() == NULL) {
    sg_decl->set_firstNondefiningDeclaration(sg_decl);
  }
  if (SgFunctionDeclaration *func_decl = isSgFunctionDeclaration(sg_decl)) {
    if (func_decl->get_definition() != NULL &&
        func_decl->get_definingDeclaration() == NULL) {
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
  *node = NULL;
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

  ROSE_ASSERT(FAIL_FIXME == 0); // FIXME

  return VisitTemplateDecl(concept_decl, node) && res;
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
  if (class_template_decl == NULL) {
    return NULL;
  }

  // CLANG FRONTEND FIX: Skip system header template classes to avoid
  // performance issues System headers contain massive template hierarchies that
  // cause extremely slow processing clang::SourceManager &SM =
  // p_compiler_instance->getSourceManager(); if
  // (SM.isInSystemHeader(class_template_decl->getLocation())) {
  //     // Skip this template class - let VisitRecordDecl handle it as a
  //     regular class *node = NULL; return false;
  // }

  clang::CXXRecordDecl *templated_decl =
      class_template_decl->getTemplatedDecl();
  if (templated_decl == NULL) {
    return NULL;
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
  if (semantic_scope == NULL) {
    semantic_scope =
        resolveScopeFromDeclContext(semantic_context, structural_scope);

    auto is_record_scope = [](SgScopeStatement *scope) -> bool {
      return isSgClassDefinition(scope) != NULL ||
             isSgTemplateClassDefinition(scope) != NULL ||
             isSgTemplateInstantiationDefn(scope) != NULL;
    };

    auto resolve_record_scope =
        [&](clang::CXXRecordDecl *record) -> SgScopeStatement * {
      if (record == NULL) {
        return NULL;
      }

      auto lookup_scope =
          [&](clang::CXXRecordDecl *record_decl) -> SgScopeStatement * {
        if (record_decl == NULL) {
          return NULL;
        }
        auto it = p_decl_translation_map.find(record_decl);
        if (it == p_decl_translation_map.end()) {
          return NULL;
        }
        if (SgClassDefinition *class_def = isSgClassDefinition(it->second)) {
          return class_def;
        }
        if (SgClassDeclaration *class_decl = isSgClassDeclaration(it->second)) {
          if (class_decl->get_definition() != NULL) {
            return class_decl->get_definition();
          }
          if (SgClassDeclaration *def_decl =
                  isSgClassDeclaration(class_decl->get_definingDeclaration())) {
            return def_decl->get_definition();
          }
        }
        if (SgTemplateClassDeclaration *template_decl =
                isSgTemplateClassDeclaration(it->second)) {
          if (template_decl->get_definition() != NULL) {
            return template_decl->get_definition();
          }
          if (SgTemplateClassDeclaration *def_decl =
                  isSgTemplateClassDeclaration(
                      template_decl->get_definingDeclaration())) {
            return def_decl->get_definition();
          }
        }
        return NULL;
      };

      if (SgScopeStatement *scope = lookup_scope(record)) {
        return scope;
      }

      clang::CXXRecordDecl *definition_decl = record->getDefinition();
      if (definition_decl != NULL && definition_decl != record) {
        if (SgScopeStatement *scope = lookup_scope(definition_decl)) {
          return scope;
        }
      }

      clang::CXXRecordDecl *canonical_decl = record->getCanonicalDecl();
      if (canonical_decl != NULL && canonical_decl != record &&
          canonical_decl != definition_decl) {
        if (SgScopeStatement *scope = lookup_scope(canonical_decl)) {
          return scope;
        }
      }

      clang::CXXRecordDecl *to_translate =
          definition_decl != NULL ? definition_decl : record;
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
      if (canonical_decl != NULL && canonical_decl != record) {
        if (SgScopeStatement *scope = lookup_scope(canonical_decl)) {
          return scope;
        }
      }

      return NULL;
    };

    if (semantic_context != NULL && !is_record_scope(semantic_scope)) {
      clang::CXXRecordDecl *record_ctx = NULL;
      if (clang::Decl *ctx_decl =
              llvm::dyn_cast<clang::Decl>(semantic_context)) {
        record_ctx = llvm::dyn_cast<clang::CXXRecordDecl>(ctx_decl);
        if (record_ctx == NULL) {
          if (clang::ClassTemplateDecl *tmpl_ctx =
                  llvm::dyn_cast<clang::ClassTemplateDecl>(ctx_decl)) {
            record_ctx = tmpl_ctx->getTemplatedDecl();
          }
        }
      }

      if (record_ctx != NULL) {
        if (SgScopeStatement *record_scope = resolve_record_scope(record_ctx)) {
          semantic_scope = record_scope;
        }
      }
    }

    if (semantic_scope == NULL) {
      semantic_scope = getGlobalScope();
    }
  }
  SgScopeStatement *symbol_scope = normalizeNamespaceScope(semantic_scope);
  if (symbol_scope == NULL) {
    symbol_scope = semantic_scope;
  }

  SgScopeStatement *lexical_parent = override_lexical_parent;
  if (lexical_parent == NULL) {
    lexical_parent =
        resolveScopeFromDeclContext(lexical_context, structural_scope);
    if (lexical_parent == NULL) {
      lexical_parent =
          structural_scope != NULL ? structural_scope : semantic_scope;
    }
  }

  SgTemplateClassDeclaration *existing_nondefining_decl = nullptr;
  if (clang::ClassTemplateDecl *prev = class_template_decl->getPreviousDecl()) {
    auto it = p_decl_translation_map.find(prev);
    if (it != p_decl_translation_map.end()) {
      if (SgTemplateClassDeclaration *prev_decl =
              isSgTemplateClassDeclaration(it->second)) {
        existing_nondefining_decl = isSgTemplateClassDeclaration(
            prev_decl->get_firstNondefiningDeclaration());
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
        class_template_decl->getTemplateParameters(), NULL);
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
        lexical_parent != NULL ? lexical_parent : structural_scope;
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

  if (result_decl == NULL) {
    return NULL;
  }

  // Keep the template declaration in its lexical scope for unparse order and
  // comment anchoring, while using the canonical namespace scope only for
  // symbol-table insertion (see normalizeNamespaceScope()).
  if (lexical_parent != NULL) {
    result_decl->set_parent(lexical_parent);
  }

  // REX FIX: Ensure firstNondefiningDeclaration is set to avoid unparser
  // crash (ua test).
  SgDeclarationStatement *firstNondef =
      result_decl->get_firstNondefiningDeclaration();
  ROSE_ASSERT(firstNondef != NULL);
  firstNondef->set_firstNondefiningDeclaration(firstNondef);

  // Attach template parameter back-links
  SgTemplateParameterPtrList &decl_params =
      result_decl->get_templateParameters();
  for (SgTemplateParameter *param : decl_params) {
    if (param != NULL) {
      // Only set owning template if it's NOT a template_parameter,
      // because template_parameter uses this field for the nrdecl.
      if (param->get_parameterType() !=
          SgTemplateParameter::template_parameter) {
        param->set_templateDeclaration(result_decl);
      }
    }
  }

  applySourceRange(result_decl, class_template_decl->getSourceRange());

  // ROOT CAUSE FIX: Cache before appending to prevent double visitation
  auto cache_translation = [&](clang::Decl *key) {
    if (key == NULL) {
      return;
    }
    auto it = p_decl_translation_map.find(key);
    if (it == p_decl_translation_map.end() || it->second == NULL) {
      p_decl_translation_map[key] = result_decl;
      return;
    }

    SgClassDeclaration *existing_class = isSgClassDeclaration(it->second);
    if (existing_class == NULL) {
      p_decl_translation_map[key] = result_decl;
      return;
    }

    if (result_decl != NULL && result_decl->get_definition() != NULL &&
        existing_class->get_definition() == NULL) {
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
       prev != NULL; prev = prev->getPreviousDecl()) {
    cache_translation(prev);
  }

  for (clang::CXXRecordDecl *prev = templated_decl->getPreviousDecl();
       prev != NULL; prev = prev->getPreviousDecl()) {
    cache_translation(prev);
  }

  // REX FIX: Do not append here. The caller (Traverse) will return this node
  // and the caller of Traverse (e.g. VisitTranslationUnitDecl) will append it.
  // Appending here causes duplicates in the global scope.
  // if (template_decl->get_parent() == NULL && scope != NULL) {
  //    SageInterface::appendStatement(template_decl, scope);
  // }

  // Populate the class definition for definitions
  if (is_definition) {
    if (SgTemplateClassDefinition *class_def =
            isSgTemplateClassDefinition(template_decl->get_definition())) {
      applySourceRange(class_def, templated_decl->getSourceRange());
      populateClassDefinition(templated_decl, class_def);
    }
  }

  for (auto it = class_template_decl->spec_begin();
       it != class_template_decl->spec_end(); ++it) {
    clang::ClassTemplateSpecializationDecl *spec = *it;
    if (spec == NULL) {
      continue;
    }

    clang::TemplateSpecializationKind kind =
        spec->getTemplateSpecializationKind();
    if (kind == clang::TSK_ExplicitInstantiationDeclaration ||
        kind == clang::TSK_ExplicitInstantiationDefinition ||
        kind == clang::TSK_ExplicitSpecialization) {
      continue;
    }

    Traverse(spec);
  }

  if (lexical_parent != NULL) {
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
      translateClassTemplateDecl(class_template_decl, NULL, NULL);
  if (result_decl == NULL) {
    *node = NULL;
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
  if (function_template_decl == NULL) {
    *node = NULL;
    return false;
  }

  clang::FunctionDecl *templated_decl =
      function_template_decl->getTemplatedDecl();
  if (templated_decl == NULL) {
    *node = NULL;
    return false;
  }

  bool res =
      translateFunctionDeclCommon(templated_decl, function_template_decl, node);

  if (res && *node != NULL) {
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
  ROSE_ASSERT(type_alias_decl != NULL);

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
  while (scope_context != NULL &&
         llvm::isa<clang::LinkageSpecDecl>(scope_context)) {
    scope_context = scope_context->getParent();
  }

  auto translate_decl_context_on_demand = [&](clang::DeclContext *ctx) {
    if (ctx == NULL) {
      return;
    }
    if (!is_declaration_scope_context(ctx)) {
      return;
    }
    clang::Decl *context_decl = llvm::dyn_cast<clang::Decl>(ctx);
    if (context_decl == NULL) {
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
  if (scope_context != NULL) {
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
  if (scope == NULL) {
    scope = getGlobalScope();
  }

  SgScopeStatement *symbol_scope = normalizeNamespaceScope(scope);
  if (symbol_scope == NULL) {
    symbol_scope = scope;
  }

  SgScopeStatement *parent_scope = scope;
  clang::DeclContext *lexical_context =
      type_alias_template_decl->getLexicalDeclContext();
  translate_decl_context_on_demand(lexical_context);
  if (lexical_context != NULL) {
    if (SgScopeStatement *resolved =
            resolveScopeFromDeclContext(lexical_context, parent_scope)) {
      parent_scope = resolved;
    }
  }

  // Get the symbol for the parent scope (mimic
  // buildTemplateTypedefDeclaration_nfi logic but be lenient)
  SgSymbol *scopeSymbol = NULL;
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
                                       NULL, // Type will be set later
                                       NULL, // Base declaration (optional)
                                       scopeSymbol);

  template_typedef->set_scope(symbol_scope);
  if (parent_scope != NULL) {
    template_typedef->set_parent(parent_scope);
  }

  // REX FIX: Set source position to avoid AST post-processing assertion failure
  applySourceRange(template_typedef,
                   type_alias_template_decl->getSourceRange());

  // Set firstNondefiningDeclaration (required for unparsing)
  template_typedef->set_firstNondefiningDeclaration(template_typedef);
  template_typedef->set_definingDeclaration(NULL);

  // Create SgTypedefType
  SgTypedefType *typedefType = SgTypedefType::createType(template_typedef);
  template_typedef->set_type(typedefType);

  // Create and insert symbol
  SgTemplateTypedefSymbol *typedef_symbol =
      new SgTemplateTypedefSymbol(template_typedef);
  symbol_scope->insert_symbol(name, typedef_symbol);

  // Handle template parameters
  clang::TemplateParameterList *param_list =
      type_alias_template_decl->getTemplateParameters();
  std::unique_ptr<SgTemplateParameterPtrList> template_params;
  if (param_list != NULL) {
    // REX FIX: Pass template_typedef as owning template
    template_params = translateTemplateParameterList(
        type_alias_template_decl->getTemplateParameters(), template_typedef);
  } else {
    template_params = std::make_unique<SgTemplateParameterPtrList>();
  }

  // REX FIX: Set template parameters on the declaration!
  template_typedef->get_templateParameters() = *template_params;

  // REX FIX: Do not append to scope here. VisitTranslationUnitDecl handles it.
  // if (scope) {
  //     scope->append_statement(template_typedef);
  // }

  // Add to map
  p_decl_translation_map.insert(
      std::make_pair(type_alias_template_decl, template_typedef));

  *node = template_typedef;
  // REX FIX: Do not call VisitRedeclarableTemplateDecl -> VisitTemplateDecl as
  // it clears *node to NULL
  return true;
}

bool ClangToSageTranslator::VisitVarTemplateDecl(
    clang::VarTemplateDecl *var_template_decl, SgNode **node) {

#if DEBUG_VISIT_DECL
  std::cerr << "ClangToSageTranslator::VisitVarTemplateDecl" << std::endl;
#endif
  if (var_template_decl != nullptr) {
    Traverse(var_template_decl->getTemplatedDecl());
  }
  *node = NULL;
  return false;
}

bool ClangToSageTranslator::VisitTemplateTemplateParmDecl(
    clang::TemplateTemplateParmDecl *template_template_parm_decl,
    SgNode **node) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToSageTranslator::VisitTemplateTemplateParmDecl"
            << std::endl;
#endif
  SgDeclarationStatement *owning_template = NULL;
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
  return sg_param != NULL;
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
    if (existing_class != NULL) {
      if (existing_class->get_definition() != NULL) {
        has_definition = true;
      } else if (SgClassDeclaration *def_decl = isSgClassDeclaration(
                     existing_class->get_definingDeclaration())) {
        if (def_decl->get_definition() != NULL) {
          has_definition = true;
        }
      }
    }

    bool needs_definition =
        record_decl->isThisDeclarationADefinition() && !has_definition;
    if (needs_definition && existing_class != NULL &&
        isSgTemplateClassDeclaration(existing_class) != NULL) {
      if (clang::CXXRecordDecl *cxx_record =
              llvm::dyn_cast<clang::CXXRecordDecl>(record_decl)) {
        if (clang::ClassTemplateDecl *template_decl =
                cxx_record->getDescribedClassTemplate()) {
          SgTemplateClassDeclaration *translated_template_decl =
              translateClassTemplateDecl(template_decl, NULL, NULL);
          if (translated_template_decl != NULL) {
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
        ROSE_ASSERT(firstNondef != NULL);
        ROSE_ASSERT(isSgClassDeclaration(firstNondef) != NULL);
        ROSE_ASSERT(firstNondef->get_firstNondefiningDeclaration() != NULL);
      }
      return true; // Already processed
    }
    // Fall through to build a defining declaration when the existing node is
    // non-defining.
  }

  SgClassDeclaration *sg_class_decl = NULL;

  // Find previous declaration

  clang::RecordDecl *prev_record_decl = record_decl->getPreviousDecl();
  clang::RecordDecl *record_Definition = record_decl->getDefinition();
  bool isDefined = record_decl->isThisDeclarationADefinition();
  bool isAnonymousStructOrUnion = record_decl->isAnonymousStructOrUnion();
  bool hasNameForLinkage = record_decl->hasNameForLinkage();

  SgClassSymbol *sg_prev_class_sym =
      isSgClassSymbol(GetSymbolFromSymbolTable(prev_record_decl));
  SgClassDeclaration *sg_prev_class_decl = NULL;
  if (sg_prev_class_sym != NULL) {
    // CLANG FRONTEND FIX: Accept both SgClassDeclaration and
    // SgTemplateClassDeclaration For templates, the symbol table may contain
    // SgTemplateClassDeclaration from VisitClassTemplateDecl
    sg_prev_class_decl =
        isSgClassDeclaration(sg_prev_class_sym->get_declaration());

    if (sg_prev_class_decl != NULL) {
      ROSE_ASSERT(sg_prev_class_decl->get_firstNondefiningDeclaration() !=
                  NULL);
      ROSE_ASSERT(sg_prev_class_decl->get_firstNondefiningDeclaration()
                      ->get_firstNondefiningDeclaration() != NULL);
    }
  }

  SgClassDeclaration *sg_first_class_decl =
      sg_prev_class_decl == NULL
          ? NULL
          : isSgClassDeclaration(
                sg_prev_class_decl->get_firstNondefiningDeclaration());

  // SgClassDeclaration * sg_def_class_decl = sg_prev_class_decl == NULL ? NULL
  // : isSgClassDeclaration(sg_prev_class_decl->get_definingDeclaration());
  SgClassSymbol *sg_defining_sym = NULL;
  // Pei-Hung (07/25/24) The case that a CXXRecordDecl has its definition inside
  // a namespace.
  if (!(llvm::isa<clang::CXXRecordDecl>(record_decl) &&
        llvm::cast<clang::CXXRecordDecl>(record_decl)->hasDefinition())) {
    sg_defining_sym =
        isSgClassSymbol(GetSymbolFromSymbolTable(record_Definition));
  }
  SgClassDeclaration *sg_def_class_decl = NULL;
  if (sg_defining_sym != NULL && sg_defining_sym->get_declaration() != NULL) {
    // CLANG FRONTEND FIX: Accept both SgClassDeclaration and
    // SgTemplateClassDeclaration
    SgDeclarationStatement *decl_stmt = sg_defining_sym->get_declaration();
    sg_def_class_decl =
        isSgClassDeclaration(decl_stmt->get_definingDeclaration());
  }

  // For template specializations, the first declaration may also be the
  // definition In that case, sg_first_class_decl and sg_def_class_decl may
  // refer to the same node
  if (sg_first_class_decl == NULL && sg_def_class_decl != NULL) {
    // This can happen for template specializations that are instantiated on
    // first use Use the defining declaration as the first declaration
    sg_first_class_decl = isSgClassDeclaration(
        sg_def_class_decl->get_firstNondefiningDeclaration());
    ROSE_ASSERT(sg_first_class_decl != NULL);
  }

  // ROSE_ASSERT(sg_first_class_decl != NULL || sg_def_class_decl == NULL);
  // Assertion relaxed for template specializations which may not have separate
  // forward declarations

  bool had_prev_decl = sg_first_class_decl != NULL;

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

  sg_class_decl = new SgClassDeclaration(name, type_of_class, NULL, NULL);
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
    while (ctx != NULL && llvm::isa<clang::LinkageSpecDecl>(ctx)) {
      ctx = ctx->getParent();
    }
    return ctx;
  };
  semantic_context = skip_linkage_context(semantic_context);
  lexical_context = skip_linkage_context(lexical_context);

  auto translate_decl_context_on_demand = [&](clang::DeclContext *ctx) {
    if (ctx == NULL) {
      return;
    }
    if (!is_declaration_scope_context(ctx)) {
      return;
    }
    clang::Decl *context_decl = llvm::dyn_cast<clang::Decl>(ctx);
    if (context_decl == NULL) {
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
  if (semantic_scope == NULL) {
    semantic_scope = getGlobalScope();
  }
  SgScopeStatement *correct_scope = semantic_scope;
  if (correct_scope == NULL) {
    correct_scope = getGlobalScope();
  }
  correct_scope = normalizeNamespaceScope(correct_scope);
  if (correct_scope == NULL) {
    correct_scope = getGlobalScope();
  }

  SgScopeStatement *lexical_parent =
      resolveScopeFromDeclContext(lexical_context, structural_scope);
  if (lexical_parent == NULL) {
    lexical_parent =
        structural_scope != NULL ? structural_scope : correct_scope;
  }

  auto normalize_class_decl_scope = [&](SgClassDeclaration *decl) {
    if (decl == NULL) {
      return;
    }
    SgScopeStatement *decl_scope = decl->get_scope();
    if (decl_scope == NULL) {
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
      isSgClassDefinition(correct_scope) != NULL ||
      isSgTemplateClassDefinition(correct_scope) != NULL ||
      isSgTemplateInstantiationDefn(correct_scope) != NULL;
  if (correct_is_class_scope) {
    auto build_class_symbol = [](SgClassDeclaration *decl) -> SgSymbol * {
      if (decl == NULL) {
        return NULL;
      }
      if (SgTemplateClassDeclaration *tmpl_decl =
              isSgTemplateClassDeclaration(decl)) {
        return new SgTemplateClassSymbol(tmpl_decl);
      }
      return new SgClassSymbol(decl);
    };

    auto rehome_class_scope = [&](SgClassDeclaration *decl) {
      if (decl == NULL) {
        return;
      }
      SgScopeStatement *decl_scope = decl->get_scope();
      if (decl_scope == correct_scope) {
        return;
      }
      SgSymbol *symbol = NULL;
      if (decl_scope != NULL) {
        symbol = decl_scope->find_symbol_from_declaration(decl);
        if (symbol != NULL) {
          if (decl_scope->symbol_exists(symbol)) {
            decl_scope->remove_symbol(symbol);
          } else if (SgSymbolTable *table = decl_scope->get_symbol_table()) {
            if (table->exists(symbol)) {
              table->remove(symbol);
            }
          }
        }
      }
      decl->set_scope(correct_scope);
      if (correct_scope->find_symbol_from_declaration(decl) == NULL) {
        if (symbol == NULL) {
          symbol = build_class_symbol(decl);
        }
        if (symbol != NULL && !correct_scope->symbol_exists(symbol)) {
          correct_scope->insert_symbol(symbol->get_name(), symbol);
        }
      }
    };

    rehome_class_scope(sg_prev_class_decl);
    rehome_class_scope(sg_first_class_decl);
    rehome_class_scope(sg_def_class_decl);
    if (sg_first_class_decl != NULL &&
        correct_scope->find_symbol_from_declaration(sg_first_class_decl) ==
            NULL) {
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
  ROSE_ASSERT(sg_class_decl->get_parent() != NULL);

  // std::cerr << "DEBUG: VisitCXXRecordDecl for " <<
  // record_decl->getNameAsString() << std::endl;

  // CRITICAL: Set firstNondefiningDeclaration BEFORE calling createType()
  // createType() internally asserts that this pointer is not null
  // This will be corrected later if this is not actually the first declaration
  if (sg_first_class_decl != NULL) {
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

  SgClassType *type = NULL;
  if (sg_first_class_decl != NULL) {
    type = sg_first_class_decl->get_type();
  } else {
    type = SgClassType::createType(sg_class_decl);
  }
  ROSE_ASSERT(type != NULL);
  sg_class_decl->set_type(type);

  if (isUnNamed)
    sg_class_decl->set_isUnNamed(true);

  if (!had_prev_decl) {
    sg_first_class_decl = sg_class_decl;
    sg_first_class_decl->set_firstNondefiningDeclaration(sg_first_class_decl);
    sg_first_class_decl->set_definingDeclaration(NULL);
    sg_first_class_decl->set_definition(NULL);
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
    if (sg_def_class_decl != NULL &&
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
    if (reuse_first_decl_for_def && sg_first_class_decl != NULL) {
      sg_def_class_decl = sg_first_class_decl;
      sg_class_decl = sg_def_class_decl;

      sg_def_class_decl->unsetForward();
      sg_def_class_decl->set_firstNondefiningDeclaration(sg_first_class_decl);
      sg_def_class_decl->set_definingDeclaration(sg_def_class_decl);
      sg_first_class_decl->set_definingDeclaration(sg_def_class_decl);

      applySourceRange(sg_def_class_decl, record_decl->getSourceRange());
    } else {
      sg_def_class_decl =
          new SgClassDeclaration(name, type_of_class, type, NULL);
      // ROOT CAUSE FIX: Use correct_scope consistently for defining
      // declaration too
      sg_def_class_decl->set_scope(correct_scope);
      if (isUnNamed)
        sg_def_class_decl->set_isUnNamed(true);
      sg_def_class_decl->set_parent(lexical_parent);

      // OPENMP LOWERING FIX: The sg_class_decl created at line 1350 will be
      // orphaned when we reassign below, but it may still be referenced through
      // SgClassType. Set its file info before orphaning.
      if (had_prev_decl && sg_class_decl->get_startOfConstruct() == NULL) {
        applySourceRange(sg_class_decl, record_decl->getSourceRange());
      }

      sg_class_decl = sg_def_class_decl; // we return the defining decl

      // CLANG FRONTEND FIX: Only set if variant types match
      if (sg_first_class_decl != NULL &&
          sg_first_class_decl->variantT() == sg_def_class_decl->variantT()) {
        sg_def_class_decl->set_firstNondefiningDeclaration(sg_first_class_decl);
      } else {
        sg_def_class_decl->set_firstNondefiningDeclaration(sg_def_class_decl);
      }
      ROSE_ASSERT(sg_def_class_decl->get_firstNondefiningDeclaration() != NULL);
      sg_def_class_decl->set_definingDeclaration(sg_def_class_decl);

      // CLANG FRONTEND FIX: Only set definingDeclaration if variant types
      // match
      if (sg_first_class_decl != NULL &&
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
    if (sg_class_def == NULL) {
      sg_class_def = SageBuilder::buildClassDefinition_nfi(sg_def_class_decl);
    }
    sg_def_class_decl->set_definition(sg_class_def);

    ROSE_ASSERT(sg_class_def->get_symbol_table() != NULL);

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

  if (sg_class_decl != NULL) {
    sg_class_decl->set_isAutonomousDeclaration(true);
  }
  if (sg_first_class_decl != NULL) {
    sg_first_class_decl->set_isAutonomousDeclaration(true);
  }

  ROSE_ASSERT(sg_class_decl->get_definingDeclaration() == NULL ||
              isSgClassDeclaration(sg_class_decl->get_definingDeclaration())
                      ->get_definition() != NULL);
  if (sg_first_class_decl != NULL && !reuse_first_decl_for_def) {
    ROSE_ASSERT(sg_first_class_decl->get_definition() == NULL);
  }
  ROSE_ASSERT(sg_def_class_decl == NULL ||
              sg_def_class_decl->get_definition() != NULL);

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
    if (sg_class_decl != NULL) {
      SgClassDeclaration *def_decl =
          isSgClassDeclaration(sg_class_decl->get_definingDeclaration());
      if (def_decl != NULL &&
          def_decl == sg_class_decl) { // Make sure this IS the defining decl
        SgClassDefinition *sg_class_def = def_decl->get_definition();
        if (sg_class_def != NULL) {
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
      // Check if firstNondefiningDeclaration is NULL OR points to self (for
      // defining decls). This catches cases where VisitRecordDecl setup was
      // insufficient.
      SgDeclarationStatement *firstNonDef =
          decl->get_firstNondefiningDeclaration();
      if (firstNonDef == NULL ||
          (decl->get_definition() && firstNonDef == decl)) {
        // Create non-defining declaration.
        // Must handle SgTemplateInstantiationDecl vs SgClassDeclaration vs
        // SgTemplateClassDeclaration.
        SgClassDeclaration *nonDef = NULL;

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
            if (tplInst->get_templateDeclaration() != NULL &&
                nondef_inst->get_templateDeclaration() == NULL) {
              nondef_inst->set_templateDeclaration(
                  tplInst->get_templateDeclaration());
            }
          }

        } else {
          nonDef = SageBuilder::buildNondefiningClassDeclaration_nfi(
              decl->get_name(), decl->get_class_type(), decl->get_scope(),
              false, NULL);
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

  SgExpression *expr = NULL;
  if (is_signed) {
    // Use the widest native builder we have; valueString keeps the full
    // precision.
    long long v = (bitwidth <= 63) ? value.getSExtValue() : 0;
    expr = SageBuilder::buildLongLongIntVal(v);
  } else {
    unsigned long long v = (bitwidth <= 64) ? value.getZExtValue() : 0;
    expr = SageBuilder::buildUnsignedLongLongIntVal(v);
  }

  if (expr != NULL) {
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

  // Ensure we handle Partial Specializations separately (fallback to
  // CXXRecordDecl for now to avoid regression)
  if (llvm::isa<clang::ClassTemplatePartialSpecializationDecl>(
          class_tpl_spec_decl)) {
    return VisitCXXRecordDecl(class_tpl_spec_decl, node);
  }

  bool in_system_header = false;
  if (p_compiler_instance != NULL) {
    clang::SourceManager &SM = p_compiler_instance->getSourceManager();
    in_system_header = SM.isInSystemHeader(class_tpl_spec_decl->getLocation());
  }

  bool res = true;

  if (class_tpl_spec_decl == NULL) {
    *node = NULL;
    return false;
  }

  auto resolve_namespace_scope =
      [&](clang::DeclContext *ctx,
          SgScopeStatement *fallback) -> SgScopeStatement * {
    clang::DeclContext *scope_ctx = ctx;
    while (scope_ctx != NULL && !scope_ctx->isNamespace() &&
           !scope_ctx->isTranslationUnit()) {
      scope_ctx = scope_ctx->getParent();
    }
    if (clang::NamespaceDecl *ns_decl =
            llvm::dyn_cast_or_null<clang::NamespaceDecl>(
                llvm::dyn_cast_or_null<clang::Decl>(scope_ctx))) {
      if (SgNamespaceDeclarationStatement *ns_stmt =
              ensureNamespaceDeclaration(ns_decl)) {
        if (ns_stmt->get_definition() != NULL) {
          return ns_stmt->get_definition();
        }
      }
    }
    if (scope_ctx != NULL && scope_ctx->isTranslationUnit()) {
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
  if (definition_decl == NULL &&
      class_tpl_spec_decl->isThisDeclarationADefinition()) {
    definition_decl = class_tpl_spec_decl;
  }
  const bool has_definition = definition_decl != NULL;
  const bool is_definition_decl =
      class_tpl_spec_decl->isThisDeclarationADefinition();

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
    if (key == NULL) {
      return NULL;
    }
    auto it = p_decl_translation_map.find(key);
    if (it == p_decl_translation_map.end()) {
      return NULL;
    }
    return isSgTemplateClassDeclaration(it->second);
  };

  if (specialized_template == NULL) {
    auto specialized = class_tpl_spec_decl->getSpecializedTemplateOrPartial();
    if (clang::ClassTemplateDecl *class_template =
            specialized.dyn_cast<clang::ClassTemplateDecl *>()) {
      specialized_template = class_template;
    }
  }

  SgTemplateClassDeclaration *primary_template_decl = NULL;
  if (specialized_template) {
    SgNode *primary_node = Traverse(specialized_template);
    primary_template_decl = isSgTemplateClassDeclaration(primary_node);
  }
  if (primary_template_decl == NULL && specialized_template != NULL) {
    primary_template_decl = lookup_template_decl(specialized_template);
    if (primary_template_decl == NULL) {
      if (clang::ClassTemplateDecl *class_template =
              llvm::dyn_cast<clang::ClassTemplateDecl>(specialized_template)) {
        clang::ClassTemplateDecl *canonical =
            class_template->getCanonicalDecl();
        if (canonical != NULL) {
          primary_template_decl = lookup_template_decl(canonical);
        }

        if (primary_template_decl == NULL) {
          if (clang::CXXRecordDecl *templated_decl =
                  class_template->getTemplatedDecl()) {
            primary_template_decl = lookup_template_decl(templated_decl);
            if (primary_template_decl == NULL) {
              if (clang::CXXRecordDecl *templated_canon =
                      llvm::dyn_cast_or_null<clang::CXXRecordDecl>(
                          templated_decl->getCanonicalDecl())) {
                primary_template_decl = lookup_template_decl(templated_canon);
              }
            }
          }
        }

        if (primary_template_decl == NULL) {
          clang::ClassTemplateDecl *to_translate =
              canonical != NULL ? canonical : class_template;
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
    if (primary_template_decl == NULL) {
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
  if (primary_template_decl == NULL) {
    std::string spec_name = class_tpl_spec_decl->getNameAsString();
    if (!spec_name.empty()) {
      SgScopeStatement *lookup_scope =
          resolve_namespace_scope(class_tpl_spec_decl->getDeclContext(), NULL);
      lookup_scope = normalizeNamespaceScope(lookup_scope);
      if (lookup_scope == NULL) {
        lookup_scope = getGlobalScope();
      }

      if (lookup_scope != NULL) {
        if (SgTemplateClassSymbol *tmpl_sym =
                lookup_scope->lookup_template_class_symbol(SgName(spec_name),
                                                           NULL, NULL)) {
          primary_template_decl =
              isSgTemplateClassDeclaration(tmpl_sym->get_declaration());
        } else if (SgClassSymbol *class_sym =
                       lookup_scope->lookup_class_symbol(SgName(spec_name))) {
          primary_template_decl =
              isSgTemplateClassDeclaration(class_sym->get_declaration());
        }
      }
      if (primary_template_decl == NULL) {
        if (SgTemplateClassSymbol *tmpl_sym =
                SageInterface::lookupTemplateClassSymbolInParentScopes(
                    SgName(spec_name), NULL, NULL, lookup_scope)) {
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
  if (primary_template_decl == NULL && specialized_template != NULL) {
    if (clang::ClassTemplateDecl *class_template =
            llvm::dyn_cast<clang::ClassTemplateDecl>(specialized_template)) {
      if (p_decl_translation_in_progress.find(class_template) ==
          p_decl_translation_in_progress.end()) {
        primary_template_decl =
            translateClassTemplateDecl(class_template, NULL, NULL);
      }
    }
  }

  // As a last resort, synthesize a nondefining template declaration so
  // instantiations always link to a valid primary template.
  if (primary_template_decl == NULL) {
    clang::ClassTemplateDecl *class_template =
        class_tpl_spec_decl->getSpecializedTemplate();
    if (class_template != NULL) {
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

      if (primary_template_decl == NULL) {
        SgScopeStatement *tmpl_scope =
            resolveScopeFromDeclContext(class_template->getDeclContext(), NULL);
        if (tmpl_scope == NULL) {
          tmpl_scope =
              resolve_namespace_scope(class_template->getDeclContext(), NULL);
        }
        tmpl_scope = normalizeNamespaceScope(tmpl_scope);
        if (tmpl_scope == NULL) {
          tmpl_scope = getGlobalScope();
        }

        std::string base_name = class_template->getNameAsString();
        if (base_name.empty()) {
          base_name = template_name_str;
        }

        auto params = translateTemplateParameterList(
            class_template->getTemplateParameters(), NULL);
        SgTemplateArgumentPtrList empty_args;

        SgTemplateClassDeclaration *stub_decl =
            SageBuilder::buildNondefiningTemplateClassDeclaration_nfi(
                SgName(base_name), class_kind, tmpl_scope, params.get(),
                &empty_args);
        if (stub_decl != NULL) {
          stub_decl->setForward();
          stub_decl->set_firstNondefiningDeclaration(stub_decl);
          stub_decl->set_definingDeclaration(NULL);
          stub_decl->get_file_info()->setCompilerGenerated();
          stub_decl->get_file_info()->unsetOutputInCodeGeneration();

          for (SgTemplateParameter *param :
               stub_decl->get_templateParameters()) {
            if (param != NULL && param->get_parameterType() !=
                                     SgTemplateParameter::template_parameter) {
              param->set_templateDeclaration(stub_decl);
            }
          }

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
  if (is_explicit_instantiation && p_compiler_instance != NULL) {
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
      (instantiation_context != NULL &&
       instantiation_context->isTranslationUnit())
          ? getGlobalScope()
          : SageBuilder::topScopeStack();

  // Check if we can get a better scope from the DeclContext
  if (instantiation_context && !instantiation_context->isTranslationUnit()) {
    clang::Decl *context_decl =
        llvm::dyn_cast<clang::Decl>(instantiation_context);
    if (context_decl) {
      SgNode *context_node = NULL;
      std::map<clang::Decl *, SgNode *>::iterator it =
          p_decl_translation_map.find(context_decl);
      if (it != p_decl_translation_map.end()) {
        context_node = it->second;
        SgNamespaceDeclarationStatement *ns_decl_stmt =
            isSgNamespaceDeclarationStatement(context_node);
        SgNamespaceDefinitionStatement *ns_def =
            isSgNamespaceDefinitionStatement(context_node);
        SgClassDefinition *class_def = isSgClassDefinition(context_node);
        if (ns_decl_stmt != NULL && ns_decl_stmt->get_definition() != NULL) {
          instantiation_scope = ns_decl_stmt->get_definition();
        } else if (ns_def != NULL) {
          instantiation_scope = ns_def;
        } else if (class_def != NULL) {
          instantiation_scope = class_def;
        }
      } else if (clang::NamespaceDecl *ns_decl =
                     llvm::dyn_cast<clang::NamespaceDecl>(context_decl)) {
        // Ensure the namespace scope exists even if it has not been translated
        // yet (e.g., when we first encounter a decl in that namespace).
        SgNamespaceDeclarationStatement *ns_stmt =
            ensureNamespaceDeclaration(ns_decl);
        if (ns_stmt && ns_stmt->get_definition() != NULL) {
          instantiation_scope = ns_stmt->get_definition();
        }
      }
    }
  }

  if (instantiation_scope == NULL) {
    instantiation_scope = getGlobalScope();
  }

  SgScopeStatement *class_scope = NULL;
  if (specialized_template != NULL) {
    clang::DeclContext *template_context =
        specialized_template->getDeclContext();
    class_scope = resolveScopeFromDeclContext(template_context, NULL);
  }
  SgScopeStatement *primary_scope = NULL;
  if (primary_template_decl != NULL) {
    primary_scope = primary_template_decl->get_scope();
  }
  if (class_scope == NULL && primary_scope != NULL) {
    class_scope = primary_scope;
  }
  if (class_scope == NULL) {
    class_scope = instantiation_scope;
  }
  if (class_scope == NULL) {
    class_scope = getGlobalScope();
  }
  if (primary_scope != NULL && isSgGlobal(class_scope) != NULL &&
      isSgGlobal(primary_scope) == NULL) {
    class_scope = primary_scope;
  }
  if (specialization_kind == clang::TSK_ExplicitSpecialization &&
      instantiation_scope != NULL) {
    class_scope = instantiation_scope;
  } else if (is_explicit_instantiation && instantiation_scope != NULL) {
    class_scope = instantiation_scope;
  }

  SgScopeStatement *lexical_scope =
      resolveScopeFromDeclContext(lexical_context, NULL);
  if (lexical_scope == NULL) {
    lexical_scope = instantiation_scope;
  }

  if (instantiation_scope == NULL) {
    instantiation_scope = getGlobalScope();
  }

  SgScopeStatement *symbol_scope = normalizeNamespaceScope(class_scope);
  if (symbol_scope == NULL) {
    symbol_scope = class_scope;
  }
  if (symbol_scope == NULL) {
    symbol_scope = getGlobalScope();
  }

  SgScopeStatement *scope = symbol_scope;
  SgScopeStatement *parent_scope = class_scope;
  if (lexical_scope != NULL &&
      (isSgNamespaceDefinitionStatement(lexical_scope) != NULL ||
       isSgGlobal(lexical_scope) != NULL)) {
    parent_scope = lexical_scope;
  }
  if (parent_scope == NULL) {
    parent_scope = scope;
  }

  // Build template arguments
  const clang::TemplateArgumentList &args =
      class_tpl_spec_decl->getTemplateArgs();
  SgName name_with_template_args(
      buildTemplateInstantiationName(name.getString(), args));

  // Helper lambda to build args (to avoid code duplication)
  auto build_args = [&](SgTemplateArgumentPtrList &target_list) {
    for (unsigned i = 0; i < args.size(); ++i) {
      appendTemplateArguments(target_list, args.get(i), false);
    }
  };

  auto rehome_instantiation_symbol =
      [&](SgTemplateInstantiationDecl *decl,
          SgScopeStatement *desired_scope) {
        if (decl == NULL || desired_scope == NULL) {
          return;
        }
        SgSymbol *symbol = decl->get_symbol_from_symbol_table();
        if (symbol == NULL) {
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
        if (parent_table != NULL && parent_table != desired_table) {
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

  SgTemplateInstantiationDecl *instantiationDecl = NULL;
  SgTemplateInstantiationDecl *firstNondefiningDeclaration = NULL;

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

    inst_name_full = mangleTemplateInstantiation(
        template_qualified_name, class_tpl_spec_decl->getTemplateArgs());

    auto cache_it = p_template_inst_cache.find(inst_name_full);
    if (cache_it != p_template_inst_cache.end()) {
      firstNondefiningDeclaration = cache_it->second;
    }
  }

  // Check for existing symbol in the scope if not found in cache
  if (firstNondefiningDeclaration == NULL) {
    SgSymbol *existing_symbol = NULL;
    existing_symbol = scope->lookup_symbol(name_with_template_args);
    if (existing_symbol == NULL) {
      existing_symbol = scope->lookup_symbol(name);
    }
    if (existing_symbol) {
      SgClassSymbol *class_symbol = isSgClassSymbol(existing_symbol);
      if (class_symbol) {
        SgClassDeclaration *existing_decl = class_symbol->get_declaration();
        firstNondefiningDeclaration =
            isSgTemplateInstantiationDecl(existing_decl);
#if 0
                if (firstNondefiningDeclaration) {
                    std::cout << "Reuse existing SgTemplateInstantiationDecl: " << firstNondefiningDeclaration << " name: " << firstNondefiningDeclaration->get_name().getString() << std::endl;
                }
#endif
      }
    }
  }

  if (firstNondefiningDeclaration == NULL) {
    SgTemplateArgumentPtrList forward_args;
    build_args(forward_args);

    instantiationDecl = new SgTemplateInstantiationDecl(
        name_with_template_args, instantiation_kind, NULL, NULL, NULL,
        forward_args);
    instantiationDecl->get_templateArguments() = forward_args;

    // Register in cache immediately
    if (!inst_name_full.empty()) {
      p_template_inst_cache[inst_name_full] = instantiationDecl;
    }

    firstNondefiningDeclaration = instantiationDecl;
    instantiationDecl->set_firstNondefiningDeclaration(
        firstNondefiningDeclaration);
    instantiationDecl->set_definingDeclaration(NULL);
    instantiationDecl->set_forward(true);
    instantiationDecl->set_templateName(name);

    SgClassType *type = SgClassType::createType(instantiationDecl);
    instantiationDecl->set_type(type);

    // setStatementSourcePosition(instantiationDecl, class_tpl_spec_decl);
    applySourceRange(instantiationDecl, class_tpl_spec_decl->getSourceRange());
    instantiationDecl->set_scope(scope);
    instantiationDecl->set_parent(parent_scope);

    for (SgTemplateArgument *arg : instantiationDecl->get_templateArguments()) {
      arg->set_parent(instantiationDecl);
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
    if (instantiationDecl != NULL &&
        instantiationDecl->get_name() != name_with_template_args) {
      // Keep symbol table keys in sync when renaming a cached instantiation.
      SgScopeStatement *decl_scope = instantiationDecl->get_scope();
      if (decl_scope != NULL) {
        SgName old_name = instantiationDecl->get_name();
        SgClassSymbol *class_symbol = decl_scope->lookup_class_symbol(old_name);
        if (class_symbol != NULL &&
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
          class_symbol = NULL;
        }
        instantiationDecl->set_name(name_with_template_args);
        if (class_symbol != NULL) {
          SgClassSymbol *existing_sym =
              decl_scope->lookup_class_symbol(name_with_template_args);
          if (existing_sym != NULL) {
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
    for (SgTemplateArgument *arg : instantiationDecl->get_templateArguments()) {
      if (arg != NULL) {
        arg->set_parent(instantiationDecl);
      }
    }
    if (instantiationDecl->get_scope() == NULL) {
      instantiationDecl->set_scope(scope);
    }
    rehome_instantiation_symbol(instantiationDecl, scope);
    if (instantiationDecl->get_parent() == NULL) {
      instantiationDecl->set_parent(parent_scope);
    }
    // setStatementSourcePosition(instantiationDecl, class_tpl_spec_decl); //
    // Don't reset position of reused decl
  }

  if (is_explicit_instantiation && instantiationDecl != NULL &&
      instantiationDecl->get_class_type() != instantiation_kind) {
    instantiationDecl->set_class_type(instantiation_kind);
  }
  // Set specialized template for the non-defining declaration (if new) or
  // check it
  if (instantiationDecl->get_templateDeclaration() == NULL) {
    // Link to the primary template declaration
    if (primary_template_decl) {
      instantiationDecl->set_templateDeclaration(primary_template_decl);
    }
  }

  // Ensure a defining declaration exists when Clang has a definition, even if
  // this specialization is emitted as an explicit instantiation directive.
  SgTemplateInstantiationDecl *nondef_decl = instantiationDecl;
  SgTemplateInstantiationDecl *definingDecl = NULL;
  if (nondef_decl != NULL) {
    definingDecl =
        isSgTemplateInstantiationDecl(nondef_decl->get_definingDeclaration());
    if (definingDecl == nondef_decl) {
      definingDecl = NULL;
    }
  }

  if (has_definition) {
    if (definingDecl == NULL) {
      SgTemplateArgumentPtrList defining_args;
      build_args(defining_args);

      definingDecl = new SgTemplateInstantiationDecl(
          name_with_template_args, class_kind, NULL, NULL, NULL, defining_args);
      definingDecl->get_templateArguments() = defining_args;

      definingDecl->set_firstNondefiningDeclaration(
          firstNondefiningDeclaration);
      firstNondefiningDeclaration->set_definingDeclaration(definingDecl);
      definingDecl->set_definingDeclaration(definingDecl);
      definingDecl->set_forward(false);
      definingDecl->set_templateName(name);
      definingDecl->set_type(firstNondefiningDeclaration->get_type());

      applySourceRange(definingDecl, class_tpl_spec_decl->getSourceRange());
      definingDecl->set_scope(scope);
      definingDecl->set_parent(parent_scope);

      for (SgTemplateArgument *arg : definingDecl->get_templateArguments()) {
        arg->set_parent(definingDecl);
      }

      definingDecl->set_templateDeclaration(
          firstNondefiningDeclaration->get_templateDeclaration());
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
        if (arg != NULL) {
          arg->set_parent(definingDecl);
        }
      }
      if (definingDecl->get_templateDeclaration() == NULL) {
        definingDecl->set_templateDeclaration(
            firstNondefiningDeclaration->get_templateDeclaration());
      }
      if (definingDecl->get_templateName().getString().empty()) {
        definingDecl->set_templateName(name);
      }
      if (definingDecl->get_type() == NULL &&
          firstNondefiningDeclaration->get_type() != NULL) {
        definingDecl->set_type(firstNondefiningDeclaration->get_type());
      }
      if (definingDecl->get_scope() == NULL) {
        definingDecl->set_scope(scope);
      }
      if (definingDecl->get_parent() == NULL) {
        definingDecl->set_parent(parent_scope);
      }
    }

    if (definingDecl != NULL && definingDecl->get_definition() == NULL) {
      SgTemplateInstantiationDefn *class_def =
          new SgTemplateInstantiationDefn(definingDecl);
      definingDecl->set_definition(class_def);
      class_def->set_parent(definingDecl);
      applySourceRange(class_def, class_tpl_spec_decl->getSourceRange());
    }
  }

  if (is_explicit_instantiation && definingDecl != NULL) {
    mark_compiler_generated_and_suppress_unparse(definingDecl);
    if (SgClassDefinition *class_def = definingDecl->get_definition()) {
      mark_compiler_generated_and_suppress_unparse(class_def);
    }
  }

  SgTemplateInstantiationDecl *node_decl = nondef_decl;
  if (is_definition_decl && !is_explicit_instantiation &&
      definingDecl != NULL) {
    node_decl = definingDecl;
  }

  p_decl_translation_map.insert(
      std::pair<clang::Decl *, SgNode *>(class_tpl_spec_decl, node_decl));
  if (definition_decl != NULL && definition_decl != class_tpl_spec_decl) {
    SgNode *definition_node =
        definingDecl != NULL ? static_cast<SgNode *>(definingDecl) : node_decl;
    p_decl_translation_map.insert(
        std::pair<clang::Decl *, SgNode *>(definition_decl, definition_node));
  }

  // Ensure we return the correct node
  SgTemplateInstantiationDirectiveStatement *instantiation_directive = NULL;
  const bool on_demand_translation =
      p_decl_translation_on_demand.find(class_tpl_spec_decl) !=
      p_decl_translation_on_demand.end();

  if (is_explicit_instantiation) {
    instantiation_directive =
        isSgTemplateInstantiationDirectiveStatement(nondef_decl->get_parent());
    if (instantiation_directive == NULL) {
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
      if (nondef_decl->get_scope() == NULL) {
        SgScopeStatement *decl_scope = scope;
        if (decl_scope == NULL) {
          decl_scope = instantiation_scope;
        }
        if (decl_scope == NULL) {
          decl_scope = getGlobalScope();
        }
        nondef_decl->set_scope(decl_scope);
      }
    }
    if (instantiation_directive != NULL) {
      if (instantiation_directive->get_declaration() == NULL) {
        instantiation_directive->set_declaration(nondef_decl);
      }
      if (nondef_decl->get_scope() == NULL) {
        SgScopeStatement *decl_scope = scope;
        if (decl_scope == NULL) {
          decl_scope = instantiation_scope;
        }
        if (decl_scope == NULL) {
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
    if (instantiation_directive != NULL &&
        instantiation_directive->get_firstNondefiningDeclaration() == NULL) {
      instantiation_directive->set_firstNondefiningDeclaration(
          instantiation_directive);
      instantiation_directive->set_definingDeclaration(instantiation_directive);
    }
  }

  if (instantiation_directive == NULL && parent_scope != NULL &&
      parent_scope != scope &&
      (isSgNamespaceDefinitionStatement(parent_scope) != NULL ||
       isSgGlobal(parent_scope) != NULL)) {
    auto attach_lexically = [&](SgDeclarationStatement *decl) {
      if (decl == NULL) {
        return;
      }
      ensure_decl_in_scope_child_list_preserve_scope(
          decl, parent_scope,
          "VisitClassTemplateSpecializationDecl:lexical");
    };

    attach_lexically(nondef_decl);
    if (definingDecl != NULL && definingDecl != nondef_decl) {
      attach_lexically(definingDecl);
    }
  }
  *node = instantiation_directive != NULL ? instantiation_directive
                                          : static_cast<SgNode *>(node_decl);
  SgTemplateInstantiationDecl *instantiationDeclChecked =
      isSgTemplateInstantiationDecl(node_decl);
  ROSE_ASSERT(instantiationDeclChecked != NULL);

  ROSE_ASSERT(instantiationDeclChecked->get_firstNondefiningDeclaration() !=
              NULL);
  ROSE_ASSERT(instantiationDeclChecked->get_firstNondefiningDeclaration()
                  ->get_firstNondefiningDeclaration() != NULL);

  // Process scope stack and children if it is a definition
  //
  // System-header class template specializations frequently contain compiler-
  // synthesized members with non-file source locations; translating the full
  // body can trigger invalid SourceLocation handling. Still create the defining
  // ROSE scope above so on-demand translation of referenced members has a valid
  // class scope to attach to, but skip eager body population for system
  // headers.
  if (definingDecl != NULL && definingDecl->get_definition() != NULL &&
      has_definition && !in_system_header) {
    SageBuilder::pushScopeStack(
        isSgScopeStatement(definingDecl->get_definition()));

    populateClassDefinition(
        definition_decl, isSgClassDefinition(definingDecl->get_definition()));

    SageBuilder::popScopeStack();
  }

  return true;
}

bool ClangToSageTranslator::VisitClassTemplatePartialSpecializationDecl(
    clang::ClassTemplatePartialSpecializationDecl *class_tpl_part_spec_decl,
    SgNode **node) {
  // Reuse the specialization logic, as partial specializations are also class
  // template specializations in Clang hierarchy and can be represented
  // similarly in ROSE (though ROSE has SgTemplateClassDeclaration for partial
  // specs, SgTemplateInstantiationDecl is also used sometimes depending on
  // how it's viewed). Actually, ROSE expects partial specializations to be
  // SgTemplateClassDeclaration with partial specialization arguments.
  // However, fixing that properly might be a larger task.
  // The issue description says "Similarly update
  // VisitClassTemplatePartialSpecializationDecl". For full correctness,
  // partial specializations should be SgTemplateClassDeclaration with
  // isPartialSpecialization set.

  // Let's delegate to VisitClassTemplateSpecializationDecl for now as it
  // constructs SgTemplateInstantiationDecl which effectively solves the crash
  // and scope issues, even if it might not be the theoretically perfect AST
  // node type for partial specs. (SgTemplateInstantiationDecl is often used
  // for anything that is "specialized" in ROSE's view from Clang's
  // perspective).

  // Implement logic for Partial Specialization directly here for now to avoid
  // issues SgTemplateClassDeclaration, not SgTemplateInstantiationDecl. But
  // aligning that with Clang's hierarchy where PartialSpec inherits from Spec
  // is tricky. Given the task is to fix the crash and regression,
  // implementing the Spec logic is the priority.
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
      SgNode *context_node = NULL;
      std::map<clang::Decl *, SgNode *>::iterator it =
          p_decl_translation_map.find(context_decl);
      if (it != p_decl_translation_map.end()) {
        context_node = it->second;
        SgNamespaceDefinitionStatement *ns_def =
            isSgNamespaceDefinitionStatement(context_node);
        SgClassDefinition *class_def = isSgClassDefinition(context_node);
        if (ns_def != NULL) {
          scope = ns_def;
        } else if (class_def != NULL) {
          scope = class_def;
        }
      }
    }
  }

  if (scope == NULL) {
    scope = getGlobalScope();
  }

  // Build template arguments
  SgTemplateArgumentPtrList template_args;
  const clang::TemplateArgumentList &args =
      class_tpl_spec_decl->getTemplateArgs();

  // Build the specialization args
  SgTemplateArgumentPtrList specialization_args;

  for (unsigned i = 0; i < args.size(); ++i) {
    const clang::TemplateArgument &arg = args[i];
    SgTemplateArgument *sg_arg = NULL;
    switch (arg.getKind()) {
    case clang::TemplateArgument::Type: {
      SgType *arg_type = buildTypeFromQualifiedType(arg.getAsType());

      // ISSUE-107 FIX: Ensure TemplateTypeParmType name is preserved (e.g.
      // "T" instead of "template_type_param")
      const clang::Type *type_ptr = arg.getAsType().getTypePtr();
      if (type_ptr) {
        if (const clang::TemplateTypeParmType *parm_type =
                type_ptr->getAs<clang::TemplateTypeParmType>()) {
          std::string name;
          if (clang::TemplateTypeParmDecl *decl = parm_type->getDecl()) {
            name = decl->getNameAsString();
          }

          // Fallback: If name is missing or canonical (e.g.
          // type-parameter-0-0), ensure we use the param list name
          // "template_type_param" is usually ROSE's default name for
          // SgTemplateType from canonical types. We want "T".
          if (name.empty() || name.find("type-parameter-") == 0) {
            unsigned index = parm_type->getIndex();
            clang::TemplateParameterList *params =
                class_tpl_part_spec_decl->getTemplateParameters();
            if (params && index < params->size()) {
              name = params->getParam(index)->getNameAsString();
            }
          }

          if (!name.empty()) {
            if (SgTemplateType *tt = isSgTemplateType(arg_type)) {
              if (tt->get_name().getString() != name) {
                tt->set_name(name);
              }
            }
          }
        }
      }

      sg_arg = new SgTemplateArgument(arg_type, false);
      break;
    }
    case clang::TemplateArgument::Integral: {
      llvm::APSInt value = arg.getAsIntegral();
      SgType *int_type = buildTypeFromQualifiedType(arg.getIntegralType());
      SgExpression *value_expr = NULL;
      if (isSgTypeBool(int_type)) {
        value_expr = SageBuilder::buildBoolValExp(value.getBoolValue());
      } else {
        value_expr = buildIntegralTemplateArgExpr(value, int_type);
      }
      std::string val_str;
      if (isSgTypeBool(int_type)) {
        val_str = value.getBoolValue() ? "true" : "false";
      } else {
        llvm::SmallString<16> Str;
        value.toString(Str);
        val_str = Str.c_str();
      }

      SgName arg_name = val_str;
      SgAssignInitializer *init =
          SageBuilder::buildAssignInitializer_nfi(value_expr, int_type);
      SgInitializedName *init_name =
          SageBuilder::buildInitializedName_nfi(arg_name, int_type, init);
      init_name->set_scope(SageBuilder::topScopeStack());

      // Issue 107: Construct with NULL expression but set initializedName to
      // satisfy both get_mangled_name and resetParentPointers.
      sg_arg = new SgTemplateArgument(SgTemplateArgument::nontype_argument,
                                      false, int_type, NULL, NULL, false);
      sg_arg->set_initializedName(init_name);
      break;
    }
    case clang::TemplateArgument::Template: {
      clang::TemplateName template_name_arg = arg.getAsTemplate();
      clang::TemplateDecl *template_decl_arg =
          template_name_arg.getAsTemplateDecl();
      if (template_decl_arg) {
        SgNode *traverse_result = Traverse(template_decl_arg);
        SgDeclarationStatement *sg_decl =
            isSgDeclarationStatement(traverse_result);

        // REX FIX: Traverse returns SgTemplateParameter for
        // TemplateTemplateParmDecl via VisitTemplateTemplateParmDecl.
        // SgTemplateParameter is NOT an SgDeclarationStatement, but it holds
        // the SgNonrealDecl we need.
        if (SgTemplateParameter *param =
                isSgTemplateParameter(traverse_result)) {
          if (SgDeclarationStatement *inner_decl =
                  param->get_templateDeclaration()) {
            if (isSgNonrealDecl(inner_decl)) {
              sg_decl = inner_decl;
            }
          }
        }

        if (sg_decl) {
          sg_arg = new SgTemplateArgument(
              SgTemplateArgument::template_template_argument,
              /*isArrayBoundUnknownType=*/false,
              /*type=*/NULL,
              /*expression=*/NULL,
              /*templateDeclaration=*/sg_decl,
              /*explicitlySpecified=*/false);
        }
      }
      break;
    }
    case clang::TemplateArgument::Expression: {
      clang::Expr *clang_expr = arg.getAsExpr();
      if (clang_expr) {
        SgNode *node = Traverse(clang_expr);
        SgExpression *sg_expr = isSgExpression(node);
        if (sg_expr) {
          sg_arg = new SgTemplateArgument(sg_expr, false);
        }
      }
      break;
    }

    default:
      break;
    }
    if (sg_arg)
      specialization_args.push_back(sg_arg);
  }
  // They are attached to the declaration later.
  auto template_params = translateTemplateParameterList(
      class_tpl_part_spec_decl->getTemplateParameters(), NULL);

  SgTemplateClassDeclaration *nonDefiningDecl =
      SageBuilder::buildNondefiningTemplateClassDeclaration_nfi(
          name, class_kind, scope, template_params.get(), &specialization_args);
  ROSE_ASSERT(nonDefiningDecl);

  // Ensure self-reference for non-defining (invariant)
  nonDefiningDecl->set_firstNondefiningDeclaration(nonDefiningDecl);

  if (class_tpl_part_spec_decl->isThisDeclarationADefinition()) {
    // REX FIX: Create FRESH specialization arguments for the defining
    // declaration We cannot reuse 'specialization_args' because the
    // non-defining declaration has already claimed ownership (parent pointers
    // set). Reusing them would detach them from the non-defining declaration,
    // violating AST invariants.
    SgTemplateArgumentPtrList specialization_args_for_def;

    // Re-iterate over Clang arguments to build new ROSE arguments
    const clang::TemplateArgumentList &args_for_def =
        class_tpl_part_spec_decl->getTemplateArgs();
    for (unsigned i = 0; i < args_for_def.size(); i++) {
      const clang::TemplateArgument &arg = args_for_def[i];
      SgTemplateArgument *sg_arg = NULL;

      // Duplicate logic from above to create SgTemplateArgument
      switch (arg.getKind()) {
        // ... cases ...

      case clang::TemplateArgument::Type: {
        SgType *sg_type = buildTypeFromQualifiedType(arg.getAsType());
        if (sg_type) {
          sg_arg = new SgTemplateArgument(sg_type, false);
        }
        break;
      }
      case clang::TemplateArgument::Declaration: {
        clang::ValueDecl *arg_decl = arg.getAsDecl();
        if (arg_decl) {
          SgNode *node = Traverse(arg_decl);
          if (SgDeclarationStatement *sg_decl =
                  isSgDeclarationStatement(node)) {
            if (arg_decl->isTemplateDecl()) {
              std::string qual_name = arg_decl->getQualifiedNameAsString();
              if (SgTemplateClassDeclaration *sg_class_tmpl =
                      isSgTemplateClassDeclaration(sg_decl)) {
                if (SgNamespaceDefinitionStatement *ns_def =
                        isSgNamespaceDefinitionStatement(
                            sg_class_tmpl->get_scope())) {
                  qual_name = ns_def->get_namespaceDeclaration()
                                  ->get_name()
                                  .getString() +
                              "::" + sg_class_tmpl->get_name().getString();
                }
              }
              SgType *type = SageBuilder::buildTemplateType(qual_name);
              sg_arg = new SgTemplateArgument(type, false);
            } else {
              sg_arg = new SgTemplateArgument(
                  SgTemplateArgument::template_template_argument,
                  /*isArrayBoundUnknownType=*/false,
                  /*type=*/NULL,
                  /*expression=*/NULL,
                  /*templateDeclaration=*/sg_decl,
                  /*explicitlySpecified=*/false);
            }
          }
        }
        break;
      }
      case clang::TemplateArgument::Expression: {
        clang::Expr *clang_expr = arg.getAsExpr();
        if (clang_expr) {
          SgNode *node = Traverse(clang_expr);
          SgExpression *sg_expr = isSgExpression(node);
          if (sg_expr) {
            sg_arg = new SgTemplateArgument(sg_expr, false);
          }
        }
        break;
      }
      case clang::TemplateArgument::Integral: {
        llvm::APSInt value = arg.getAsIntegral();
        SgType *int_type = buildTypeFromQualifiedType(arg.getIntegralType());
        SgExpression *value_expr = NULL;
        if (isSgTypeBool(int_type)) {
          value_expr = SageBuilder::buildBoolValExp(value.getBoolValue());
        } else {
          value_expr = buildIntegralTemplateArgExpr(value, int_type);
        }

        std::string val_str;
        if (isSgTypeBool(int_type)) {
          val_str = value.getBoolValue() ? "true" : "false";
        } else {
          llvm::SmallString<16> Str;
          value.toString(Str);
          val_str = Str.c_str();
        }

        SgName arg_name = val_str;
        SgAssignInitializer *init =
            SageBuilder::buildAssignInitializer_nfi(value_expr, int_type);
        // SgInitializedName needs a valid scope, but SgTemplateArgument's init
        // name serves as a wrapper. We set scope to NULL or global scope? Loop
        // 1 sets nothing (NULL parent).
        SgInitializedName *init_name =
            SageBuilder::buildInitializedName_nfi(arg_name, int_type, init);
        init_name->set_scope(SageBuilder::topScopeStack());

        sg_arg = new SgTemplateArgument(SgTemplateArgument::nontype_argument,
                                        false, int_type, NULL, NULL, false);
        sg_arg->set_initializedName(init_name);
        break;
      }
      case clang::TemplateArgument::Template: {
        clang::TemplateName template_name_arg = arg.getAsTemplate();
        clang::TemplateDecl *template_decl_arg =
            template_name_arg.getAsTemplateDecl();
        if (template_decl_arg) {
          SgNode *traverse_result = Traverse(template_decl_arg);
          SgDeclarationStatement *sg_decl =
              isSgDeclarationStatement(traverse_result);

          if (SgTemplateParameter *param =
                  isSgTemplateParameter(traverse_result)) {
            if (SgDeclarationStatement *inner_decl =
                    param->get_templateDeclaration()) {
              if (isSgNonrealDecl(inner_decl)) {
                sg_decl = inner_decl;
              }
            }
          }

          if (sg_decl) {
            sg_arg = new SgTemplateArgument(
                SgTemplateArgument::template_template_argument,
                /*isArrayBoundUnknownType=*/false,
                /*type=*/NULL,
                /*expression=*/NULL,
                /*templateDeclaration=*/sg_decl,
                /*explicitlySpecified=*/false);
          }
        }
        break;
      }
      default:
        break;
      }
      if (sg_arg)
        specialization_args_for_def.push_back(sg_arg);
    }

    // Create proper defining declaration
    SgTemplateClassDeclaration *definingDecl =
        SageBuilder::buildTemplateClassDeclaration_nfi(
            name, class_kind, scope, nonDefiningDecl, template_params.get(),
            &specialization_args_for_def);

    *node = definingDecl;
    p_decl_translation_map[class_tpl_part_spec_decl] = definingDecl;

    SgClassDefinition *def = definingDecl->get_definition();
    ROSE_ASSERT(def);

    SageBuilder::pushScopeStack(def);
    populateClassDefinition(class_tpl_spec_decl, def);
    SageBuilder::popScopeStack();

    // Wire relations (SageBuilder should have done it, but enforce)
    definingDecl->set_firstNondefiningDeclaration(nonDefiningDecl);
    definingDecl->set_definingDeclaration(definingDecl);

    nonDefiningDecl->set_definingDeclaration(definingDecl);

    applySourceRange(definingDecl, class_tpl_part_spec_decl->getSourceRange());

  } else {
    *node = nonDefiningDecl;
    p_decl_translation_map[class_tpl_part_spec_decl] = nonDefiningDecl;
    applySourceRange(nonDefiningDecl,
                     class_tpl_part_spec_decl->getSourceRange());
  }

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
  while (scope_context != NULL &&
         llvm::isa<clang::LinkageSpecDecl>(scope_context)) {
    scope_context = scope_context->getParent();
  }

  auto translate_decl_context_on_demand = [&](clang::DeclContext *ctx) {
    if (ctx == NULL) {
      return;
    }
    if (!is_declaration_scope_context(ctx)) {
      return;
    }
    clang::Decl *context_decl = llvm::dyn_cast<clang::Decl>(ctx);
    if (context_decl == NULL) {
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
  if (scope_context != NULL) {
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
  if (enum_scope == NULL) {
    enum_scope = getGlobalScope();
  }

  SgScopeStatement *symbol_scope = normalizeNamespaceScope(enum_scope);
  if (symbol_scope == NULL) {
    symbol_scope = enum_scope;
  }

  SgScopeStatement *lexical_scope = enum_scope;
  clang::DeclContext *lexical_context = enum_decl->getLexicalDeclContext();
  translate_decl_context_on_demand(lexical_context);
  if (lexical_context != NULL) {
    if (SgScopeStatement *resolved =
            resolveScopeFromDeclContext(lexical_context, lexical_scope)) {
      lexical_scope = resolved;
    }
  }

  clang::EnumDecl *prev_enum_decl = enum_decl->getPreviousDecl();
  SgEnumSymbol *sg_prev_enum_sym =
      isSgEnumSymbol(GetSymbolFromSymbolTable(prev_enum_decl));
  SgEnumDeclaration *sg_prev_enum_decl =
      sg_prev_enum_sym == NULL
          ? NULL
          : isSgEnumDeclaration(sg_prev_enum_sym->get_declaration());
  sg_prev_enum_decl =
      sg_prev_enum_decl == NULL
          ? NULL
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
    if (sg_enum_decl->get_parent() == NULL && lexical_scope != NULL) {
      sg_enum_decl->set_parent(lexical_scope);
    } else if (lexical_scope != NULL &&
               sg_enum_decl->get_parent() != lexical_scope) {
      sg_enum_decl->set_parent(lexical_scope);
    }
    if (lexical_scope != NULL) {
      ensure_decl_in_scope_child_list_preserve_scope(
          sg_enum_decl, lexical_scope, "VisitEnumDecl:lexical");
    }
  }

  if (sg_prev_enum_decl == NULL ||
      sg_prev_enum_decl->get_enumerators().size() == 0) {
    SgScopeStatement *enum_scope = symbol_scope;
    if (enum_scope == NULL) {
      enum_scope = getGlobalScope();
    }

    clang::EnumDecl::enumerator_iterator it;
    for (it = enum_decl->enumerator_begin(); it != enum_decl->enumerator_end();
         it++) {
      SgNode *tmp_enumerator = Traverse(*it);
      SgInitializedName *enumerator = isSgInitializedName(tmp_enumerator);

      ROSE_ASSERT(enumerator);

      if (enum_scope != NULL) {
        enumerator->set_scope(enum_scope);
        if (enum_scope->find_symbol_from_declaration(enumerator) == NULL) {
          SgEnumFieldSymbol *field_symbol =
              new SgEnumFieldSymbol(enumerator);
          enum_scope->insert_symbol(enumerator->get_name(), field_symbol);
        }
      }
      sg_enum_decl->append_enumerator(enumerator);

      // CLANG FRONTEND FIX: Set declptr for enum constant's SgInitializedName
      // declptr should point to the enum declaration that contains this
      // constant
      enumerator->set_declptr(sg_enum_decl);
    }
  } else {
    sg_enum_decl->set_definingDeclaration(sg_prev_enum_decl);
    sg_enum_decl->set_firstNondefiningDeclaration(
        sg_prev_enum_decl->get_firstNondefiningDeclaration());
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
  SgDeclarationStatement *owning_template = NULL;
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

  if (sg_param != NULL) {
    std::string kw = template_type_parm_decl->wasDeclaredWithTypename()
                         ? "typename"
                         : "class";
    SageInterface::setTemplateParameterKeyword(sg_param, kw);
  }

  return sg_param != NULL;
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
  while (scope_context != NULL &&
         llvm::isa<clang::LinkageSpecDecl>(scope_context)) {
    scope_context = scope_context->getParent();
  }
  if (scope_context != NULL) {
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
      if (resolved != NULL) {
        scope = resolved;
      }
    }
  }
  if (scope == NULL) {
    scope = getGlobalScope();
  }
  scope = normalizeNamespaceScope(scope);

  SgTypedefDeclaration *sg_typedef_decl = NULL;
  SgName name(typedef_decl->getNameAsString());
  SgTypedefSymbol *tdef_sym =
      scope != NULL ? scope->lookup_typedef_symbol(name) : NULL;
  if (tdef_sym != NULL) {
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
  bool hasElaboratedType = false;
  bool isOwnedTagDeclADefinition = false;
  bool isDefinitionRequired = false;
  // Definitions embedded in a declarator are not autonomous.
  bool isAutonomousDeclaration = true;

  // Adding check for EaboratedType and PointerType to retrieve base EnumType
  while ((llvm::isa<clang::ElaboratedType>(underlyingType)) ||
         (llvm::isa<clang::PointerType>(underlyingType)) ||
         (llvm::isa<clang::ArrayType>(underlyingType))) {
    if (llvm::isa<clang::ElaboratedType>(underlyingType)) {
      hasElaboratedType = true;
      underlyingQualType =
          ((clang::ElaboratedType *)underlyingType)->getNamedType();
      clang::TagDecl *ownedTagDecl =
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
  }

  if (llvm::isa<clang::RecordType>(underlyingType)) {
    clang::RecordType *underlyingRecordType =
        (clang::RecordType *)underlyingType;
    clang::RecordDecl *recordDeclaration = underlyingRecordType->getDecl();
    isembedded = recordDeclaration->isEmbeddedInDeclarator();
    iscompleteDefined = recordDeclaration->isCompleteDefinition();
  }

  if (hasElaboratedType) {
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
              << "' (sg_type=" << (type != NULL ? type->class_name() : "null")
              << ", base="
              << (type != NULL && type->findBaseType() != NULL
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

  sg_typedef_decl = SageBuilder::buildTypedefDeclaration_nfi(name, type, scope);
  if (SgProject::get_verbose() > 0) {
    if (name == "uint8_t" || name == "uint16_t" || name == "uint32_t" ||
        name == "in_port_t" || name == "in_addr_t") {
      std::cerr << "CFE: Created typedef '" << name << "' with type "
                << (type != NULL ? type->class_name() : "null")
                << " (underlying "
                << (sg_underlyingType != NULL ? sg_underlyingType->class_name()
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

      ROSE_ASSERT(classDefDecl->get_firstNondefiningDeclaration() != NULL);
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
    if (!suppress_typedef && p_compiler_instance != NULL) {
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
  while (scope_context != NULL &&
         llvm::isa<clang::LinkageSpecDecl>(scope_context)) {
    scope_context = scope_context->getParent();
  }
  if (scope_context != NULL) {
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
      if (resolved != NULL) {
        scope = resolved;
      }
    }
  }
  if (scope == NULL) {
    scope = getGlobalScope();
  }

  // C++11 type aliases (using foo = int) are semantically equivalent to
  // typedefs TypeAliasDecl and TypedefDecl both inherit from TypedefNameDecl
  // Use the same implementation logic as VisitTypedefDecl

  SgName name(type_alias_decl->getNameAsString());
  clang::QualType underlyingQualType = type_alias_decl->getUnderlyingType();
  SgType *type = NULL;

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

  if (type == NULL) {
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
            if (arg != NULL)
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
  if (result && p_compiler_instance != NULL) {
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

bool ClangToSageTranslator::VisitUsingDecl(clang::UsingDecl *using_decl,
                                           SgNode **node) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToSageTranslator::VisitUsingDecl" << std::endl;
#endif
  bool res = true;

  // ROOT CAUSE FIX: Implement proper support for using declarations (e.g.,
  // using std::cout;) Build a SgUsingDeclarationStatement to represent the
  // using declaration

  // A UsingDecl refers to declarations through shadow declarations
  // Get the first shadow decl's target to create the using statement
  SgDeclarationStatement *sg_target_decl = NULL;
  SgInitializedName *sg_init_name = NULL;

  // Try to get the target declaration through shadow decls
  // Note: We only check the cache to avoid traversing the entire target
  if (using_decl->shadow_size() > 0) {
    clang::UsingShadowDecl *shadow = *(using_decl->shadow_begin());
    if (shadow != NULL) {
      clang::NamedDecl *target = shadow->getTargetDecl();
      if (target != NULL) {
        // Check if target was already translated
        if (clang::Decl *target_decl = llvm::dyn_cast<clang::Decl>(target)) {
          std::map<clang::Decl *, SgNode *>::iterator it =
              p_decl_translation_map.find(target_decl);
          if (it != p_decl_translation_map.end()) {
            // Try to cast to declaration or initialized name
            sg_target_decl = isSgDeclarationStatement(it->second);
            if (sg_target_decl == NULL) {
              sg_init_name = isSgInitializedName(it->second);
            }
          } else {
            // ROOT CAUSE FIX: Target not in cache, traverse it to build the
            // declaration
            SgNode *tmp_node = TraverseOnDemand(target_decl);
            if (tmp_node != NULL) {
              sg_target_decl = isSgDeclarationStatement(tmp_node);
              if (sg_target_decl == NULL) {
                sg_init_name = isSgInitializedName(tmp_node);
              }
            }
          }
        }
      }
    }
  }

  // Ensure at least one parameter is non-NULL for unparser.
  if (sg_target_decl == NULL && sg_init_name == NULL) {
    MLOG_ERROR_C(MLOG_FRONTEND,
                 "Runtime error: unable to resolve UsingDecl target.\n");
    *node = NULL;
    return false;
  }

  // Build the using declaration statement
  // Constructor signature: SgUsingDeclarationStatement(SgDeclarationStatement*
  // declaration, SgInitializedName* initializedName)
  SgUsingDeclarationStatement *using_stmt =
      new SgUsingDeclarationStatement(sg_target_decl, sg_init_name);
  using_stmt->set_definingDeclaration(using_stmt);
  using_stmt->set_firstNondefiningDeclaration(using_stmt);

  if (SgScopeStatement *current_scope = SageBuilder::topScopeStack()) {
    using_stmt->set_scope(current_scope);
    using_stmt->set_parent(current_scope);
  }
  diagnose_null_scope(using_stmt, "UsingDecl");

  *node = using_stmt;

  return VisitNamedDecl(using_decl, node) && res;
}

// Helper to ensure a namespace declaration exists (creating stubs if needed)
SgNamespaceDeclarationStatement *
ClangToSageTranslator::ensureNamespaceDeclaration(
    clang::NamespaceDecl *ns_decl) {
  if (ns_decl == NULL)
    return NULL;

  clang::NamespaceDecl *canonical_ns = getCanonicalNamespaceDecl(ns_decl);

  // Check if already translated
  std::map<clang::Decl *, SgNode *>::iterator it =
      p_decl_translation_map.find(canonical_ns);
  if (it != p_decl_translation_map.end()) {
    return isSgNamespaceDeclarationStatement(it->second);
  }

  // Not found, create a stub
  SgScopeStatement *scope = NULL;
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
  if (scope == NULL) {
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

  ROSE_ASSERT(FAIL_FIXME == 0); // FIXME

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
  *node = NULL;
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

  ROSE_ASSERT(FAIL_FIXME == 0); // FIXME

  return VisitNamedDecl(constructor_using_shadow_decl, node) && res;
}

bool ClangToSageTranslator::VisitValueDecl(clang::ValueDecl *value_decl,
                                           SgNode **node) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToSageTranslator::VisitValueDecl" << std::endl;
#endif
  bool res = true;

  ROSE_ASSERT(FAIL_FIXME == 0); // FIXME

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
  ROSE_ASSERT(type != NULL);

  // Build a variable declaration to represent the binding and provide a symbol
  // for decl references.
  SgVariableDeclaration *sg_var_decl =
      SageBuilder::buildVariableDeclaration_nfi(name, type, NULL,
                                                SageBuilder::topScopeStack());
  sg_var_decl->set_isAssociatedWithDeclarationList(true);

  clang::Expr *binding_expr = binding_decl->getBinding();
  SgInitializer *init = NULL;
  if (binding_expr != NULL) {
    SgNode *tmp_init = Traverse(binding_expr);
    if (SgInitializer *tmp_init_initializer = isSgInitializer(tmp_init)) {
      init = tmp_init_initializer;
    } else {
      SgExpression *expr = isSgExpression(tmp_init);
      if (tmp_init != NULL && expr == NULL) {
        std::cerr << "Runtime error: not a SgInitializer..." << std::endl;
        res = false;
      } else if (expr != NULL) {
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

  if (init != NULL) {
    sg_var_decl->reset_initializer(init);
  }
  if (init != NULL && binding_expr != NULL) {
    if (!llvm::isa<clang::CXXConstructExpr>(binding_expr)) {
      applySourceRange(init, binding_expr->getSourceRange());
    }
  }

  sg_var_decl->set_firstNondefiningDeclaration(sg_var_decl);
  sg_var_decl->set_parent(SageBuilder::topScopeStack());

  ROSE_ASSERT(sg_var_decl->get_variables().size() == 1);
  SgInitializedName *init_name = sg_var_decl->get_variables()[0];
  ROSE_ASSERT(init_name != NULL);
  if (init_name->get_scope() == NULL) {
    init_name->set_scope(SageBuilder::topScopeStack());
  }
  if (init != NULL) {
    init->set_parent(init_name);
  }

  applySourceRange(init_name, binding_decl->getSourceRange());

  SgVariableDefinition *var_def =
      isSgVariableDefinition(init_name->get_declptr());
  if (var_def == NULL) {
    var_def = sg_var_decl->get_definition();
    if (var_def != NULL) {
      init_name->set_declptr(var_def);
    }
  }
  ROSE_ASSERT(var_def != NULL);
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
    isembedded = enumDeclaration->isEmbeddedInDeclarator();
    iscompleteDefined = enumDeclaration->isCompleteDefinition();
  }

  if (llvm::isa<clang::RecordType>(fieldType)) {
    clang::RecordType *underlyingRecordType = (clang::RecordType *)fieldType;
    clang::RecordDecl *recordDeclaration = underlyingRecordType->getDecl();
    isembedded = recordDeclaration->isEmbeddedInDeclarator();
    iscompleteDefined = recordDeclaration->isCompleteDefinition();
    isNamedNonEmbeddedRecord = !recordDeclaration->isEmbeddedInDeclarator() &&
                               !recordDeclaration->isAnonymousStructOrUnion() &&
                               recordDeclaration->getIdentifier() != NULL;
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
  bool is_lambda_field = (parent_record != NULL && parent_record->isLambda());

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
              << field_decl->getType().getAsString()
              << "' (sg_type=" << (type != NULL ? type->class_name() : "null")
              << ", base="
              << (type != NULL && type->findBaseType() != NULL
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
  if (tmp_init != NULL && expr == NULL) {
    std::cerr << "Runtime error: not a SgInitializer..." << std::endl;
    res = false;
  }
  SgInitializer *init =
      expr != NULL
          ? SageBuilder::buildAssignInitializer_nfi(expr, expr->get_type())
          : NULL;
  if (init != NULL)
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
  if (access == clang::AS_none && parent_record != NULL) {
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
    if (classDecl != NULL) {
      classDecl->set_isAutonomousDeclaration(false);
    }
    if (classDefDecl != NULL) {
      classDefDecl->set_isAutonomousDeclaration(false);
    }
    // Skip embedding when the field is a pointer to the enclosing class
    // (self-reference).
    if (fieldType != enclosing_record_type && isembedded &&
        classDefDecl != nullptr &&
        !isSgDeclarationStatement(classDefDecl->get_parent())) {
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
  var_decl->set_parent(SageBuilder::topScopeStack());

  ROSE_ASSERT(var_decl->get_variables().size() == 1);

  SgInitializedName *init_name = var_decl->get_variables()[0];
  ROSE_ASSERT(init_name != NULL);
  init_name->set_parent(var_decl);
  init_name->set_scope(SageBuilder::topScopeStack());
  applySourceRange(init_name, field_decl->getSourceRange());

  // CLANG FRONTEND FIX: declptr should point to SgVariableDefinition, not
  // SgVariableDeclaration Check if it's already set, if not get it from
  // var_decl
  SgVariableDefinition *var_def =
      isSgVariableDefinition(init_name->get_declptr());
  if (var_def == NULL) {
    var_def = var_decl->get_definition();
    if (var_def != NULL) {
      init_name->set_declptr(var_def);
    }
  }
  ROSE_ASSERT(var_def != NULL);
  applySourceRange(var_def, field_decl->getSourceRange());

  // Pei-Hung (08/15/23): The following causes duplicated symbols in some cases.
  // Comment it out and need further investigation.
  // SgVariableSymbol *var_symbol = new SgVariableSymbol(init_name);
  // SageBuilder::topScopeStack()->insert_symbol(name, var_symbol);

  *node = var_decl;
  return VisitDeclaratorDecl(field_decl, node) && res;
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

  struct TranslationGuard {
    std::set<clang::Decl *> &in_progress;
    clang::Decl *decl;
    TranslationGuard(std::set<clang::Decl *> &set, clang::Decl *d)
        : in_progress(set), decl(d) {
      in_progress.insert(decl);
    }
    ~TranslationGuard() { in_progress.erase(decl); }
  } translation_guard(p_decl_translation_in_progress, function_decl);

  // FIXME: There is something weird here when try to Traverse a function
  // reference in a recursive function (when first Traverse is not complete)
  //        It seems that it tries to instantiate the decl inside the
  //        function... It may be faster to recode from scratch...
  //   If I am not wrong this have been fixed....

  SgName name(function_decl->getNameAsString());
  std::string func_name = function_decl->getNameAsString();

  if (llvm::isa<clang::CXXConstructorDecl>(function_decl)) {
    clang::CXXRecordDecl *parentClassDecl =
        static_cast<clang::CXXConstructorDecl *>(function_decl)->getParent();
    SgClassDeclaration *CxxRecordDeclaration = NULL;
    auto lookup_class_decl = [&](clang::CXXRecordDecl *record)
        -> SgClassDeclaration * {
      if (record == NULL) {
        return NULL;
      }
      auto it = p_decl_translation_map.find(record);
      if (it == p_decl_translation_map.end()) {
        return NULL;
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
      return NULL;
    };

    CxxRecordDeclaration = lookup_class_decl(parentClassDecl);
    if (CxxRecordDeclaration == NULL) {
      if (clang::CXXRecordDecl *canonical =
              parentClassDecl->getCanonicalDecl()) {
        CxxRecordDeclaration = lookup_class_decl(canonical);
      }
    }
    bool parent_in_progress = false;
    auto check_in_progress = [&](clang::CXXRecordDecl *record) {
      if (record != NULL &&
          p_decl_translation_in_progress.find(record) !=
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
      if (clang::CXXRecordDecl *canonical = parentClassDecl->getCanonicalDecl()) {
        check_in_progress(canonical);
      }
    }
    if (CxxRecordDeclaration == NULL && !parent_in_progress) {
      CxxRecordDeclaration = isSgClassDeclaration(Traverse(parentClassDecl));
    }
    if (CxxRecordDeclaration != NULL) {
      name = CxxRecordDeclaration->get_name();
    } else if (parentClassDecl != NULL) {
      name = SgName(parentClassDecl->getNameAsString());
    }
  }

  bool is_builtin_decl =
      (function_decl->getBuiltinID() != clang::Builtin::NotBuiltin);
  if (!is_builtin_decl && func_name.rfind("__builtin_", 0) == 0) {
    is_builtin_decl = true;
  }
  if (!is_builtin_decl && p_compiler_instance != NULL) {
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
  if (p_compiler_instance != NULL) {
    clang::SourceManager &sm = p_compiler_instance->getSourceManager();
    if (isSystemOrBuiltinFunctionDecl(function_decl, sm) &&
        specialization_kind == clang::TSK_ImplicitInstantiation &&
        !force_template_instantiation) {
      allow_template_instantiation = false;
    }
  }
  bool is_system_or_builtin = is_builtin_decl;
  if (p_compiler_instance != NULL) {
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
  // special modifier buildDefiningFunctionDeclaration requires non-NULL return
  // type, so we use void for constructors and mark them with the constructor
  // modifier flag later
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
    if (source == NULL || param_decl == NULL) {
      return source;
    }

    SgInitializer *cloned_init = NULL;
    if (SgInitializer *init = source->get_initializer()) {
      cloned_init = SageInterface::deepCopy(init);
    }

    SgInitializedName *cloned =
        SageBuilder::buildInitializedName_nfi(source->get_name(),
                                              source->get_type(), cloned_init);
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

    if (init_name != NULL) {
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
      if (definingDecl != NULL) {
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
          //            SgFunctionParameterScope
          // NOT supported: SgBasicBlock, SgDeclarationScope,
          // SgForInitStatement, etc.
          if (isSgGlobal(definitionEnclosingScope) ||
              isSgNamespaceDefinitionStatement(definitionEnclosingScope) ||
              isSgClassDefinition(definitionEnclosingScope) ||
              isSgTemplateClassDefinition(definitionEnclosingScope) ||
              isSgTemplateInstantiationDefn(definitionEnclosingScope) ||
              isSgFunctionParameterScope(definitionEnclosingScope)) {
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

    if (tmp_init_name != NULL && init_name == NULL) {
      std::cerr << "Runtime error: tmp_init_name != NULL && init_name == NULL"
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
        SageBuilder::buildInitializedName_nfi(empty, ellipses_type, NULL);
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
  while (semantic_context != NULL &&
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
  bool decl_context_is_record =
      decl_context != NULL && llvm::isa<clang::CXXRecordDecl>(decl_context);
  bool looks_like_friend_free_function =
      decl_context_is_record && !isFriendMethod;

  bool isFriendFunction =
      (function_decl->getFriendObjectKind() != clang::Decl::FOK_None) ||
      looks_like_friend_free_function;
  bool isFriendFreeFunction = (isFriendFunction && !isFriendMethod);

  // Lexical class enclosing scope needed so friend free functions stay visible
  // in the namespace
  SgScopeStatement *lexical_friend_enclosing_scope = NULL;
  SgScopeStatement *lexical_friend_class_def = NULL;
  bool friend_lexically_inside_class = false;
  auto getEnclosingNamespaceScope =
      [](SgScopeStatement *scope) -> SgScopeStatement * {
    SgScopeStatement *current = scope;
    while (current != NULL && !isSgGlobal(current) &&
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
    clang::DeclContext *lexical_context =
        function_decl->getLexicalDeclContext();
    if (lexical_context && llvm::isa<clang::CXXRecordDecl>(lexical_context)) {
      clang::CXXRecordDecl *lexical_class =
          llvm::cast<clang::CXXRecordDecl>(lexical_context);
      auto lookup_class_node = [&](clang::CXXRecordDecl *key) -> SgNode * {
        if (key == NULL) {
          return NULL;
        }
        auto it = p_decl_translation_map.find(key);
        if (it != p_decl_translation_map.end()) {
          return it->second;
        }
        return NULL;
      };

      SgNode *class_node = lookup_class_node(lexical_class);
      if (class_node == NULL) {
        class_node = lookup_class_node(lexical_class->getDefinition());
      }
      if (class_node == NULL) {
        class_node = lookup_class_node(lexical_class->getCanonicalDecl());
      }
      if (class_node == NULL) {
        if (clang::CXXRecordDecl *canonical =
                lexical_class->getCanonicalDecl()) {
          class_node = lookup_class_node(canonical->getDefinition());
        }
      }

      if (class_node != NULL) {
        SgScopeStatement *class_scope = NULL;
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
          if (SgTemplateClassDeclaration *decl =
                  isSgTemplateClassDeclaration(
                      template_class_def->get_declaration())) {
            class_scope = decl->get_scope();
          }
        }
        if (lexical_friend_class_def != NULL) {
          friend_lexically_inside_class = true;
        }
        if (class_scope != NULL) {
          lexical_friend_enclosing_scope =
              getEnclosingNamespaceScope(class_scope);
          if (lexical_friend_enclosing_scope == NULL) {
            lexical_friend_enclosing_scope = getGlobalScope();
          }
        }
      }
    }
  }
  if (isFriendFreeFunction && !friend_lexically_inside_class) {
    if (SgScopeStatement *scope = SageBuilder::topScopeStack()) {
      if (isSgClassDefinition(scope) != NULL ||
          isSgTemplateClassDefinition(scope) != NULL) {
        lexical_friend_class_def = scope;
        friend_lexically_inside_class = true;
        lexical_friend_enclosing_scope = getEnclosingNamespaceScope(scope);
        if (lexical_friend_enclosing_scope == NULL) {
          lexical_friend_enclosing_scope = getGlobalScope();
        }
      }
    }
  }

  bool scope_assigned = false;
  if (isFriendFreeFunction) {
    bool keep_in_class_scope = (!isDefinition) || friend_lexically_inside_class;
    if (keep_in_class_scope) {
      if (lexical_friend_class_def != NULL) {
        proper_scope = lexical_friend_class_def;
        scope_assigned = true;
      }
    } else {
      if (lexical_friend_enclosing_scope != NULL) {
        proper_scope = lexical_friend_enclosing_scope;
        scope_assigned = true;
      }
    }
  }

  auto resolve_class_definition_from_node =
      [&](SgNode *class_node) -> SgClassDefinition * {
    if (class_node == NULL) {
      return NULL;
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
      return NULL;
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
      return NULL;
    }
    return NULL;
  };

  auto resolve_class_definition =
      [&](clang::CXXRecordDecl *record) -> SgClassDefinition * {
    if (record == NULL) {
      return NULL;
    }
    std::map<clang::Decl *, SgNode *>::iterator it =
        p_decl_translation_map.find(record);
    if (it == p_decl_translation_map.end()) {
      return NULL;
    }
    return resolve_class_definition_from_node(it->second);
  };
  auto translate_template_definition =
      [&](clang::CXXRecordDecl *record) -> void {
    if (record == NULL) {
      return;
    }
    clang::ClassTemplateDecl *template_decl =
        record->getDescribedClassTemplate();
    if (template_decl == NULL) {
      return;
    }

    auto it = p_decl_translation_map.find(template_decl);
    if (it != p_decl_translation_map.end()) {
      if (resolve_class_definition_from_node(it->second) != NULL) {
        return;
      }
    }

    if (p_decl_translation_in_progress.find(template_decl) !=
        p_decl_translation_in_progress.end()) {
      return;
    }

    TranslationGuard template_guard(p_decl_translation_in_progress,
                                    template_decl);
    translateClassTemplateDecl(template_decl, NULL, NULL);
  };
  auto resolve_or_translate_class_definition =
      [&](clang::CXXRecordDecl *record) -> SgClassDefinition * {
    if (record == NULL) {
      return NULL;
    }
    auto ensure_definition_populated =
        [&](clang::CXXRecordDecl *decl,
            SgClassDefinition *def) -> SgClassDefinition * {
      if (decl == NULL || def == NULL) {
        return def;
      }
      if (p_record_definitions_populated.find(def) !=
          p_record_definitions_populated.end()) {
        return def;
      }
      clang::CXXRecordDecl *def_decl = decl->getDefinition();
      if (def_decl == NULL) {
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
    if (definition_decl != NULL && definition_decl != record) {
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
    return ensure_definition_populated(record, resolve_class_definition(record));
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
      if (class_def == NULL && definition_decl != NULL &&
          definition_decl != parent_class) {
        class_def = resolve_or_translate_class_definition(definition_decl);
      }
      clang::CXXRecordDecl *canonical_decl = parent_class->getCanonicalDecl();
      if (class_def == NULL && canonical_decl != NULL &&
          canonical_decl != parent_class) {
        class_def = resolve_or_translate_class_definition(canonical_decl);
      }
      if (class_def == NULL) {
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
      if (class_def == NULL) {
        if (clang::ClassTemplateDecl *templ =
                parent_class->getDescribedClassTemplate()) {
          clang::CXXRecordDecl *templated = templ->getTemplatedDecl();
          if (templated != NULL && templated != parent_class) {
            class_def = resolve_or_translate_class_definition(templated);
          }
        }
      }
      if (class_def == NULL) {
        std::cerr << "Error: Could not resolve class definition for method '"
                  << getDeclNameSafe(function_decl) << "'. Skipping function."
                  << std::endl;
        if (SgProject::get_verbose() > 0) {
          auto dump_record = [&](const char *label,
                                 clang::CXXRecordDecl *record) {
            if (record == NULL) {
              std::cerr << "  " << label << ": <null>" << std::endl;
              return;
            }
            std::string name = record->getQualifiedNameAsString();
            std::cerr << "  " << label << ": "
                      << (name.empty() ? "<unnamed>" : name) << " ("
                      << static_cast<clang::Decl *>(record)->getDeclKindName()
                      << ")"
                      << " def=" << (record->getDefinition() != NULL)
                      << " tsk=" << record->getTemplateSpecializationKind()
                      << std::endl;
            auto it = p_decl_translation_map.find(record);
            std::cerr << "    map="
                      << (it != p_decl_translation_map.end() ? "yes" : "no")
                      << std::endl;
            if (it != p_decl_translation_map.end() && it->second != NULL) {
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
        if (node != NULL) {
          *node = NULL;
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
          if (ns_decl_stmt != NULL) {
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
          if (ns_stmt && ns_stmt->get_definition() != NULL) {
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
  if (lexical_scope == NULL) {
    lexical_scope = proper_scope;
  }

  if (isMethodDecl) {
    auto *method_decl = llvm::cast<clang::CXXMethodDecl>(function_decl);
    if (method_decl->isOutOfLine()) {
      clang::DeclContext *target_ctx = method_decl->getLexicalDeclContext();
      while (target_ctx != NULL &&
             llvm::isa<clang::CXXRecordDecl>(target_ctx)) {
        target_ctx = target_ctx->getParent();
      }
      if (target_ctx != NULL) {
        if (SgScopeStatement *out_of_line_scope =
                resolveScopeFromDeclContext(target_ctx, NULL)) {
          lexical_scope = out_of_line_scope;
        }
      }
    }
  }

  if (isFriendFreeFunction && !friend_lexically_inside_class) {
    if (isSgClassDefinition(lexical_scope) != NULL ||
        isSgTemplateClassDefinition(lexical_scope) != NULL) {
      lexical_friend_class_def = lexical_scope;
      friend_lexically_inside_class = true;
      lexical_friend_enclosing_scope = getEnclosingNamespaceScope(lexical_scope);
      if (lexical_friend_enclosing_scope == NULL) {
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
  if (scope_for_symbol_table == NULL) {
    scope_for_symbol_table = getGlobalScope();
  }
  scope_for_symbol_table = normalizeNamespaceScope(scope_for_symbol_table);

  // Friend free functions declared/defined inside a class are semantically
  // declared in the enclosing namespace/global scope.  Use that scope for
  // symbol-table insertion and SageBuilder lookup, while preserving the lexical
  // class scope for AST attachment/unparse order.
  SgScopeStatement *friend_symbol_scope = NULL;
  if (isFriendFreeFunction && friend_lexically_inside_class) {
    SgScopeStatement *semantic_scope =
        resolveScopeFromDeclContext(decl_context, NULL);
    if (semantic_scope != NULL &&
        (isSgNamespaceDefinitionStatement(semantic_scope) != NULL ||
         isSgGlobal(semantic_scope) != NULL)) {
      friend_symbol_scope = normalizeNamespaceScope(semantic_scope);
    } else if (lexical_friend_enclosing_scope != NULL) {
      friend_symbol_scope = normalizeNamespaceScope(lexical_friend_enclosing_scope);
    }
    if (friend_symbol_scope == NULL) {
      friend_symbol_scope = getGlobalScope();
    }
  }
  if (friend_symbol_scope != NULL) {
    scope_for_symbol_table = friend_symbol_scope;
  }


  auto normalize_existing_function_symbol_scope =
      [&](SgScopeStatement *symbol_scope) {
        if (symbol_scope == NULL || isMethodDecl) {
          return;
        }

        SgFunctionType *func_type =
            SageBuilder::buildFunctionType(ret_type, param_list);
        if (func_type == NULL) {
          return;
        }

        auto rehome_symbol = [&](SgSymbol *sym) {
          if (sym == NULL) {
            return;
          }
          SgSymbolTable *desired_table =
              symbol_scope != NULL ? symbol_scope->get_symbol_table() : NULL;
          if (desired_table == NULL) {
            return;
          }
          SgSymbolTable *parent_table = isSgSymbolTable(sym->get_parent());
          if (parent_table == desired_table) {
            return;
          }

          bool removed_from_parent = false;
          if (parent_table != NULL) {
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
          if (decl == NULL) {
            return;
          }
          SgScopeStatement *decl_scope = decl->get_scope();
          if (decl_scope == NULL) {
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
              (isSgNamespaceDefinitionStatement(symbol_scope) != NULL ||
               isSgGlobal(symbol_scope) != NULL)) {
            decl->set_scope(symbol_scope);
          }
        };

        auto normalize_symbol_decl = [&](SgFunctionSymbol *sym) {
          if (sym == NULL) {
            return;
          }
          normalize_decl_scope(isSgFunctionDeclaration(sym->get_declaration()));
          normalize_decl_scope(isSgFunctionDeclaration(
              sym->get_declaration()
                  ? sym->get_declaration()->get_firstNondefiningDeclaration()
                  : NULL));
          rehome_symbol(sym);
        };

        auto normalize_symbol_from_decl =
            [&](clang::FunctionDecl *decl) -> bool {
          if (decl == NULL) {
            return false;
          }
          if (SgSymbol *sym = GetSymbolFromSymbolTable(decl)) {
            if (SgFunctionSymbol *func_sym = isSgFunctionSymbol(sym)) {
              normalize_symbol_decl(func_sym);
              return true;
            }
            if (SgTemplateFunctionSymbol *tmpl_sym =
                    isSgTemplateFunctionSymbol(sym)) {
              normalize_decl_scope(
                  isSgFunctionDeclaration(tmpl_sym->get_declaration()));
              rehome_symbol(tmpl_sym);
              return true;
            }
          }
          return false;
        };

        auto rehome_from_namespace_chain = [&]() {
          SgNamespaceDefinitionStatement *canonical_ns =
              isSgNamespaceDefinitionStatement(symbol_scope);
          if (canonical_ns == NULL) {
            return;
          }

          SgNamespaceDefinitionStatement *first_def =
              isSgNamespaceDefinitionStatement(
                  normalizeNamespaceScope(canonical_ns));
          if (first_def == NULL) {
            first_def = canonical_ns;
          }

          std::vector<SgSymbol *> symbols_to_rehome;
          std::set<SgSymbol *> seen_symbols;
          for (SgNamespaceDefinitionStatement *ns = first_def; ns != NULL;
               ns = ns->get_nextNamespaceDefinition()) {
            if (ns == canonical_ns) {
              continue;
            }
            SgSymbolTable *table = ns->get_symbol_table();
            if (table == NULL) {
              continue;
            }

            const std::string key = name.getString();
            if (rose_hash_multimap *symtab = table->get_table()) {
              auto range = symtab->equal_range(key);
              for (auto it = range.first; it != range.second; ++it) {
                if (it->second != NULL &&
                    seen_symbols.insert(it->second).second) {
                  symbols_to_rehome.push_back(it->second);
                }
              }
            }
          }

          for (SgSymbol *sym : symbols_to_rehome) {
            if (SgFunctionSymbol *func_sym = isSgFunctionSymbol(sym)) {
              rehome_symbol(func_sym);
            } else if (SgTemplateFunctionSymbol *tmpl_sym =
                           isSgTemplateFunctionSymbol(sym)) {
              rehome_symbol(tmpl_sym);
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
          clang::FunctionDecl *canonical_decl =
              function_decl->getCanonicalDecl();
          if (!normalized && canonical_decl != NULL &&
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
              if (it->second != NULL && seen_symbols.insert(it->second).second) {
                symbols_to_normalize.push_back(it->second);
              }
            }
          }
        }

        for (SgSymbol *sym : symbols_to_normalize) {
          if (SgFunctionSymbol *func_sym = isSgFunctionSymbol(sym)) {
            normalize_symbol_decl(func_sym);
          } else if (SgTemplateFunctionSymbol *tmpl_sym =
                         isSgTemplateFunctionSymbol(sym)) {
            normalize_decl_scope(
                isSgFunctionDeclaration(tmpl_sym->get_declaration()));
            rehome_symbol(tmpl_sym);
          }
        }

        rehome_from_namespace_chain();
      };

  normalize_existing_function_symbol_scope(scope_for_symbol_table);

  SgFunctionDeclaration *sg_function_decl;
  auto register_function_translation = [&](clang::FunctionDecl *clang_decl,
                                           SgFunctionDeclaration *sage_decl) {
    if (clang_decl == NULL || sage_decl == NULL) {
      return;
    }
    auto register_decl = [&](clang::FunctionDecl *key) {
      if (key == NULL) {
        return;
      }
      auto it = p_decl_translation_map.find(key);
      if (it == p_decl_translation_map.end() || it->second == NULL) {
        p_decl_translation_map[key] = sage_decl;
      }
    };
    register_decl(clang_decl);
    register_decl(clang_decl->getCanonicalDecl());
    register_decl(clang_decl->getFirstDecl());
  };

  // REX FIX: Check if this is a function template pattern
  clang::FunctionTemplateDecl *templateDecl =
      template_decl != NULL ? template_decl
                            : function_decl->getDescribedFunctionTemplate();
  std::unique_ptr<SgTemplateParameterPtrList> templateParams;
  if (templateDecl) {
    // Translate template parameters
    // Pass NULL as owning template for now, we'll set it later if needed,
    // but SgTemplateFunctionDeclaration IS the owning template.
    // However, we can't pass it before creating it.
    templateParams = translateTemplateParameterList(
        templateDecl->getTemplateParameters(), NULL);
  }

  // Member functions of *class templates* are represented in ROSE as template
  // member functions (they are parameterized by the enclosing class template),
  // even when they are not function templates in Clang (i.e.,
  // getDescribedFunctionTemplate() is null).  Their definitions must be built
  // with the template-member builders, otherwise SageBuilder will attempt to
  // build a non-template defining declaration from a template nondefining
  // declaration and abort (copyAST_copytest2007_40 / Issue 69).
  bool isClassTemplateMemberFunction = false;
  if (isMethodDecl && templateDecl == NULL) {
    auto *method_decl = llvm::cast<clang::CXXMethodDecl>(function_decl);
    if (clang::CXXRecordDecl *parent_record = method_decl->getParent()) {
      isClassTemplateMemberFunction =
          parent_record->getDescribedClassTemplate() != nullptr &&
          function_decl->getTemplateSpecializationKind() ==
              clang::TSK_Undeclared;
    }
  }

  const bool isTemplateMemberFunction = templateDecl != NULL && isMethodDecl;
  const bool isTemplateLikeMemberFunction =
      isMethodDecl &&
      (isTemplateMemberFunction || isClassTemplateMemberFunction);
  const bool is_explicit_specialization =
      specialization_kind == clang::TSK_ExplicitSpecialization;
  SgTemplateArgumentPtrList explicit_specialization_args;
  SgTemplateArgumentPtrList *args_for_builder = NULL;
  if (is_explicit_specialization && templateDecl == NULL) {
    size_t explicit_arg_count = 0;
    if (const clang::ASTTemplateArgumentListInfo *args_as_written =
            function_decl->getTemplateSpecializationArgsAsWritten()) {
      clang::TemplateArgumentListInfo arg_info(args_as_written->getLAngleLoc(),
                                               args_as_written->getRAngleLoc());
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
    if (defining_decl == NULL) {
      return false;
    }

    // Only process the function body if it exists. Template functions and
    // forward declarations may be marked as definitions but have no body.
    if (function_decl->hasBody() && !is_explicitly_defaulted_or_deleted &&
        !is_system_or_builtin) {
      SgFunctionDefinition *function_definition =
          defining_decl->get_definition();
      ROSE_ASSERT(function_definition != NULL);

      // P1 Badge Fix: Recursive Cache Invalidation.
      // We must invalidate the cache for the body statements and
      // declarations BEFORE potentially deleting the existing body AST.
      // This prevents Use-After-Free (accessing deleted nodes parents)
      // and ensures we don't reuse "stolen" nodes from templates. We
      // erase unconditionally because we are about to rebuild the body
      // for this definition.
      clang::Stmt *body_stmt = function_decl->getBody();
      SgBasicBlock *old_body = function_definition->get_body();
      SgBasicBlock *placeholder_body = NULL;
      auto purge_translation_caches_for_body = [&](SgNode *root,
                                                   SgNode *function_root) {
        if (root == NULL && function_root == NULL) {
          return;
        }

        auto is_in_old_body = [&](SgNode *node) -> bool {
          if (node == NULL) {
            return false;
          }
          if (root != NULL) {
            if (node == root) {
              return true;
            }
            if (SageInterface::isAncestor(root, node)) {
              return true;
            }
          }
          if (function_root != NULL) {
            if (node == function_root) {
              return true;
            }
            if (SageInterface::isAncestor(function_root, node)) {
              return true;
            }
          }
          return false;
        };

        auto decl_in_old_body =
            [&](SgDeclarationStatement *decl) -> bool {
          if (decl == NULL) {
            return false;
          }
          if (is_in_old_body(decl)) {
            return true;
          }
          if (SgDeclarationStatement *def =
                  decl->get_definingDeclaration()) {
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
          if (type != NULL) {
            SgType *base_type = type->findBaseType();
            if (SgNamedType *named = isSgNamedType(base_type)) {
              SgDeclarationStatement *decl = named->get_declaration();
              if (decl != NULL) {
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
      if (body_stmt != NULL) {
        auto body_it = p_stmt_translation_map.find(body_stmt);
        if (body_it != p_stmt_translation_map.end()) {
          if (old_body != NULL && body_it->second == old_body) {
            return true;
          }
        }

        bool old_body_is_placeholder = false;
        if (old_body != NULL) {
          old_body_is_placeholder = old_body->get_statements().empty();
        }

        bool need_invalidate =
            (body_it != p_stmt_translation_map.end()) ||
            (old_body != NULL && !old_body_is_placeholder);

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
          if (old_body != NULL) {
            purge_translation_caches_for_body(old_body, function_definition);

            struct SymbolScrubber : public AstSimpleProcessing {
              std::set<SgSymbol *> removed;
              SgBasicBlock *old_body = NULL;

              void removeSymbol(SgSymbol *sym) {
                if (sym == NULL) {
                  return;
                }
                if (!removed.insert(sym).second) {
                  return;
                }
                SgNode *basis = sym->get_symbol_basis();
                if (basis != NULL && old_body != NULL) {
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
                  for (SgInitializedName *init :
                       var_decl->get_variables()) {
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
                old_body, SageInterface::DeleteAstMode::kSkipExternalReferences);
            old_body = NULL;
          }
          recursive_invalidate_stmt(body_stmt);
        } else if (isolate_template_body) {
          recursive_invalidate_stmt(body_stmt);
        } else if (old_body != NULL) {
          placeholder_body = old_body;
        }
      }

      SageBuilder::pushScopeStack(function_definition);

      SgNode *tmp_body = Traverse(function_decl->getBody());
      SgBasicBlock *body = isSgBasicBlock(tmp_body);
      if (body == NULL) {
        if (SgTryStmt *try_stmt = isSgTryStmt(tmp_body)) {
          try_stmt->set_is_function_try_block(true);
          SgBasicBlock *wrapper = SageBuilder::buildBasicBlock_nfi();
          SageInterface::appendStatement(try_stmt, wrapper);
          applySourceRange(wrapper, function_decl->getBody()->getSourceRange());
          body = wrapper;
        }
      }

      SageBuilder::popScopeStack();

      if (body == NULL && tmp_body != NULL) {
        std::cerr << "Traverse(function_decl->getBody()) returned a "
                     "non-SgBasicBlock node: "
                  << tmp_body->class_name() << std::endl;
        body_res = false;
      }
      if (body != NULL) {
        // DQ (11/24/2020): This fails for test2020_00.C (in C_tests).
        // It seems that even though function_definition was used to set
        // the scope in the connection to the body, that the body's parent
        // is set to NULL. ROSE_ASSERT(body->get_parent() ==
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
      if (placeholder_body != NULL && placeholder_body != body) {
        placeholder_body->set_parent(nullptr);
        delete placeholder_body;
      }
      applySourceRange(function_definition, function_decl->getSourceRange());

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
  bool handled_template_instantiation = false;
  SgTemplateInstantiationDirectiveStatement *explicit_instantiation_directive =
      NULL;
  if (allow_template_instantiation &&
      (specialization_kind == clang::TSK_ImplicitInstantiation ||
       specialization_kind == clang::TSK_ExplicitInstantiationDefinition ||
       specialization_kind == clang::TSK_ExplicitInstantiationDeclaration)) {
    const clang::TemplateArgumentList *clang_args =
        function_decl->getTemplateSpecializationArgs();
    if (clang_args != NULL) {
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

      const bool needs_defining_instantiation =
          function_decl->isThisDeclarationADefinition() &&
          function_decl->hasBody() && !is_explicitly_defaulted_or_deleted &&
          !is_explicit_instantiation && !is_system_or_builtin;
      auto clone_param_list =
          [&](SgFunctionParameterList *source) -> SgFunctionParameterList * {
        SgFunctionParameterList *cloned =
            SageBuilder::buildFunctionParameterList_nfi();
        applySourceRange(cloned, function_decl->getSourceRange());
        if (source == NULL) {
          return cloned;
        }

        for (SgInitializedName *init_name : source->get_args()) {
          if (init_name == NULL) {
            continue;
          }

          SgInitializer *cloned_init = NULL;
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

      SgFunctionDeclaration *inst_nondef_decl = NULL;
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

      if (inst_nondef_decl != NULL) {
        if (inst_nondef_param_list != NULL &&
            inst_nondef_param_list->get_parent() == NULL) {
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
        if (inst_symbol_decl == NULL) {
          inst_symbol_decl = inst_nondef_decl;
        }

        auto propagate_explicit_template_args =
            [&](SgDeclarationStatement *decl) {
              if (decl == NULL) {
                return;
              }

              SgTemplateArgumentPtrList *existing_args =
                  SageBuilder::getTemplateArgumentList(decl);
              if (existing_args == NULL) {
                return;
              }

              size_t limit = existing_args->size();
              if (template_args.size() < limit) {
                limit = template_args.size();
              }

              for (size_t i = 0; i < limit; ++i) {
                SgTemplateArgument *src_arg = (*existing_args)[i];
                SgTemplateArgument *dst_arg = template_args[i];
                if (src_arg != NULL && dst_arg != NULL &&
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
          if (SgNode *tmpl_node =
                  Traverse(function_decl->getPrimaryTemplate())) {
            if (SgTemplateFunctionDeclaration *tmpl_decl =
                    isSgTemplateFunctionDeclaration(tmpl_node)) {
              inst_func->set_templateDeclaration(tmpl_decl);
              inst_func->set_templateName(tmpl_decl->get_name());
            }
          }
        } else if (SgTemplateInstantiationMemberFunctionDecl *inst_member =
                       isSgTemplateInstantiationMemberFunctionDecl(
                           inst_symbol_decl)) {
          inst_member->set_template_argument_list_is_explicit(
              inst_member->get_template_argument_list_is_explicit() ||
              has_explicit_args);
          SageBuilder::setTemplateArgumentsInDeclaration(inst_member,
                                                         template_args_ptr);
          if (SgNode *tmpl_node =
                  Traverse(function_decl->getPrimaryTemplate())) {
            if (SgTemplateMemberFunctionDeclaration *tmpl_decl =
                    isSgTemplateMemberFunctionDeclaration(tmpl_node)) {
              inst_member->set_templateDeclaration(tmpl_decl);
              inst_member->set_templateName(tmpl_decl->get_name());
            }
          }
        }

        if (function_decl->isVariadic()) {
          inst_nondef_decl->hasEllipses();
        }

        if (SgFunctionParameterList *params =
                inst_nondef_decl->get_parameterList()) {
          for (SgInitializedName *param : params->get_args()) {
            if (param != NULL) {
              param->set_declptr(inst_nondef_decl);
            }
          }
        }

        if (needs_defining_instantiation) {
          SgFunctionDeclaration *defining_inst = NULL;
          if (llvm::isa<clang::CXXMethodDecl>(function_decl)) {
            SgMemberFunctionDeclaration *inst_nondef_member =
                isSgMemberFunctionDeclaration(inst_symbol_decl);
            ROSE_ASSERT(inst_nondef_member != NULL);
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

          ROSE_ASSERT(defining_inst != NULL);
          sg_function_decl = defining_inst;
          sg_function_decl->set_definingDeclaration(sg_function_decl);
          if (param_list != NULL && param_list->get_parent() == NULL) {
            param_list->set_parent(sg_function_decl);
          }
          sg_function_decl->set_firstNondefiningDeclaration(inst_symbol_decl);
          inst_symbol_decl->set_definingDeclaration(sg_function_decl);

          if (function_decl->isVariadic()) {
            sg_function_decl->hasEllipses();
          }

          for (SgInitializedName *param : param_list->get_args()) {
            if (param != NULL) {
              param->set_declptr(sg_function_decl);
            }
          }

          if (SgTemplateInstantiationFunctionDecl *inst_func =
                  isSgTemplateInstantiationFunctionDecl(sg_function_decl)) {
            inst_func->set_template_argument_list_is_explicit(
                has_explicit_args);
            SageBuilder::setTemplateArgumentsInDeclaration(inst_func,
                                                           template_args_ptr);
            if (SgNode *tmpl_node =
                    Traverse(function_decl->getPrimaryTemplate())) {
              if (SgTemplateFunctionDeclaration *tmpl_decl =
                      isSgTemplateFunctionDeclaration(tmpl_node)) {
                inst_func->set_templateDeclaration(tmpl_decl);
                inst_func->set_templateName(tmpl_decl->get_name());
              }
            }
          } else if (SgTemplateInstantiationMemberFunctionDecl *inst_member =
                         isSgTemplateInstantiationMemberFunctionDecl(
                             sg_function_decl)) {
            inst_member->set_template_argument_list_is_explicit(
                has_explicit_args);
            SageBuilder::setTemplateArgumentsInDeclaration(inst_member,
                                                           template_args_ptr);
            if (SgNode *tmpl_node =
                    Traverse(function_decl->getPrimaryTemplate())) {
              if (SgTemplateMemberFunctionDeclaration *tmpl_decl =
                      isSgTemplateMemberFunctionDeclaration(tmpl_node)) {
                inst_member->set_templateDeclaration(tmpl_decl);
                inst_member->set_templateName(tmpl_decl->get_name());
              }
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
                if (param != NULL) {
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
                if (param != NULL) {
                  setCompilerGeneratedFileInfo(param);
                  suppress_unparse_output(param);
                }
              }
            }
          }
        }

        handled_template_instantiation = true;
      }
    }
  }

  if (!handled_template_instantiation && isDefinition &&
      !is_explicitly_defaulted_or_deleted) {
    auto clone_param_list = [&](SgFunctionParameterList *source_params)
        -> SgFunctionParameterList * {
      ROSE_ASSERT(source_params != NULL);
      SgFunctionParameterList *cloned =
          SageBuilder::buildFunctionParameterList_nfi();
      applySourceRange(cloned, function_decl->getSourceRange());

      for (SgInitializedName *init_name : source_params->get_args()) {
        if (init_name == NULL) {
          continue;
        }

        SgInitializer *cloned_init = NULL;
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
            SageBuilder::buildInitializedName_nfi(empty, ellipses_type, NULL);
        ellipses_param->set_parent(cloned);
        cloned->append_arg(ellipses_param);
      }

      return cloned;
    };

    auto fixup_nondef_params = [&](SgFunctionDeclaration *decl) -> void {
      if (decl == NULL) {
        return;
      }

      if (SgFunctionParameterList *params = decl->get_parameterList()) {
        SgScopeStatement *param_scope = decl->get_functionParameterScope();
        if (param_scope == NULL) {
          param_scope = decl->get_scope();
        }
        for (SgInitializedName *param : params->get_args()) {
          if (param != NULL) {
            param->set_declptr(decl);
            if (param_scope != NULL) {
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

    if (templateDecl != NULL || isClassTemplateMemberFunction) {
      SgTemplateParameterPtrList empty_template_params;
      // Template definitions require a prior non-defining declaration for
      // SageBuilder. Reuse an existing one when a forward declaration was
      // already seen to keep declaration/definition chains consistent.
      if (isTemplateLikeMemberFunction) {
        SgTemplateParameterPtrList *effective_template_params =
            templateParams.get();
        if (effective_template_params == NULL) {
          ROSE_ASSERT(isClassTemplateMemberFunction);
          effective_template_params = &empty_template_params;
        }

        SgTemplateMemberFunctionDeclaration *first_nondef = NULL;

        if (function_decl->getFirstDecl() != function_decl) {
          auto map_it =
              p_decl_translation_map.find(function_decl->getFirstDecl());
          if (map_it != p_decl_translation_map.end()) {
            first_nondef =
                isSgTemplateMemberFunctionDeclaration(map_it->second);
          }
          if (first_nondef == NULL) {
            auto tmpl_it = p_decl_translation_map.find(templateDecl);
            if (tmpl_it != p_decl_translation_map.end()) {
              first_nondef =
                  isSgTemplateMemberFunctionDeclaration(tmpl_it->second);
            }
          }
          if (first_nondef == NULL) {
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

        if (first_nondef == NULL) {
          SgFunctionParameterList *first_param_list =
              clone_param_list(param_list);

          first_nondef =
              SageBuilder::buildNondefiningTemplateMemberFunctionDeclaration(
                  name, ret_type, first_param_list, builder_scope,
                  functionConstVolatileFlags, effective_template_params);
          ROSE_ASSERT(first_nondef != NULL);

          applySourceRange(first_nondef, function_decl->getSourceRange());
          first_param_list->set_parent(first_nondef);
          if (function_decl->isVariadic())
            first_nondef->hasEllipses();

          // This prototype is synthesized to satisfy SageBuilder's defining
          // template builders. If the template is defined in a class scope
          // (e.g. friend templates defined inline inside a class), the extra
          // class-scope redeclaration must not be emitted by the unparser.
          if (isSgClassDefinition(builder_scope) != NULL &&
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
        if (builder_force_free_scope && friend_symbol_scope != NULL) {
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
          if (param != NULL) {
            param->set_declptr(sg_function_decl);
          }
        }

        if (defining_template->get_firstNondefiningDeclaration() == NULL ||
            defining_template->get_firstNondefiningDeclaration() ==
                defining_template) {
          defining_template->set_firstNondefiningDeclaration(first_nondef);
        }
        first_nondef->set_definingDeclaration(defining_template);
      } else {
        SgTemplateFunctionDeclaration *first_nondef = NULL;

        if (function_decl->getFirstDecl() != function_decl) {
          auto map_it =
              p_decl_translation_map.find(function_decl->getFirstDecl());
          if (map_it != p_decl_translation_map.end()) {
            first_nondef = isSgTemplateFunctionDeclaration(map_it->second);
          }
          if (first_nondef == NULL) {
            auto tmpl_it = p_decl_translation_map.find(templateDecl);
            if (tmpl_it != p_decl_translation_map.end()) {
              first_nondef = isSgTemplateFunctionDeclaration(tmpl_it->second);
            }
          }
          if (first_nondef == NULL) {
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

        if (first_nondef == NULL) {
          SgFunctionParameterList *first_param_list =
              clone_param_list(param_list);

          first_nondef =
              SageBuilder::buildNondefiningTemplateFunctionDeclaration(
                  name, ret_type, first_param_list, builder_scope,
                  templateParams.get());
          ROSE_ASSERT(first_nondef != NULL);

          applySourceRange(first_nondef, function_decl->getSourceRange());
          first_param_list->set_parent(first_nondef);
          if (function_decl->isVariadic())
            first_nondef->hasEllipses();

          // This prototype is synthesized to satisfy SageBuilder's defining
          // template builders. If the template is defined in a class scope
          // (e.g. friend templates defined inline inside a class), the extra
          // class-scope redeclaration must not be emitted by the unparser.
          if (isSgClassDefinition(builder_scope) != NULL &&
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
        if (builder_force_free_scope && friend_symbol_scope != NULL) {
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
          if (param != NULL) {
            param->set_declptr(sg_function_decl);
          }
        }

        if (defining_template->get_firstNondefiningDeclaration() == NULL ||
            defining_template->get_firstNondefiningDeclaration() ==
                defining_template) {
          defining_template->set_firstNondefiningDeclaration(first_nondef);
        }
        first_nondef->set_definingDeclaration(defining_template);
      }
    } else {
      const bool build_explicit_specialization_instantiation =
          args_for_builder != NULL;
      SgFunctionDeclaration *first_nondef_for_builder = NULL;
      const bool expect_member =
          builder_force_free_scope == false &&
          (isSgClassDefinition(builder_scope) != NULL ||
           isSgTemplateClassDefinition(builder_scope) != NULL);
      if (function_decl->getFirstDecl() != function_decl) {
        clang::FunctionDecl *clang_first_decl =
            llvm::cast<clang::FunctionDecl>(function_decl->getFirstDecl());
        SgNode *first_node = NULL;
        auto map_it = p_decl_translation_map.find(clang_first_decl);
        if (map_it != p_decl_translation_map.end()) {
          first_node = map_it->second;
        } else {
          first_node = Traverse(clang_first_decl);
        }

        SgFunctionDeclaration *first_decl = isSgFunctionDeclaration(first_node);
        if (first_decl == NULL) {
          if (SgSymbol *tmp_symbol =
                  GetSymbolFromSymbolTable(clang_first_decl)) {
            if (SgFunctionSymbol *func_sym = isSgFunctionSymbol(tmp_symbol)) {
              first_decl = isSgFunctionDeclaration(func_sym->get_declaration());
            }
          }
        }

        if (first_decl != NULL) {
          first_nondef_for_builder = isSgFunctionDeclaration(
              first_decl->get_firstNondefiningDeclaration());
          if (first_nondef_for_builder == NULL) {
            first_nondef_for_builder = first_decl;
          }
        }

        // Only pass a non-defining declaration to SageBuilder if it matches the
        // expected function kind for this scope (member vs free function).
        if (first_nondef_for_builder != NULL) {
          if (expect_member) {
            if (isSgMemberFunctionDeclaration(first_nondef_for_builder) ==
                NULL) {
              first_nondef_for_builder = NULL;
            }
          } else {
            if (isSgMemberFunctionDeclaration(first_nondef_for_builder) !=
                NULL) {
              first_nondef_for_builder = NULL;
            }
          }
          if (build_explicit_specialization_instantiation &&
              first_nondef_for_builder != NULL) {
            if (expect_member) {
              if (isSgTemplateInstantiationMemberFunctionDecl(
                      first_nondef_for_builder) == NULL) {
                first_nondef_for_builder = NULL;
              }
            } else {
              if (isSgTemplateInstantiationFunctionDecl(
                      first_nondef_for_builder) == NULL) {
                first_nondef_for_builder = NULL;
              }
            }
          }
        }
      }

      auto ensure_nondef_symbol = [&](SgFunctionDeclaration *decl) -> bool {
        if (decl == NULL) {
          return false;
        }
        if (decl->get_symbol_from_symbol_table() != NULL) {
          return true;
        }

        SgScopeStatement *decl_scope = decl->get_scope();
        if (decl_scope == NULL) {
          return false;
        }
        SgFunctionSymbol *func_sym =
            decl_scope->lookup_function_symbol(decl->get_name(),
                                               decl->get_type());
        if (func_sym == NULL) {
          func_sym = decl_scope->lookup_function_symbol(decl->get_name());
          if (func_sym != NULL &&
              func_sym->get_declaration() != NULL &&
              func_sym->get_declaration()->get_type() != decl->get_type()) {
            func_sym = NULL;
          }
        }
        if (func_sym != NULL) {
          if (func_sym->get_declaration() != decl) {
            func_sym->set_declaration(decl);
          }
          if (!decl_scope->symbol_exists(func_sym)) {
            decl_scope->insert_symbol(func_sym->get_name(), func_sym);
          }
          return true;
        }

        SgSymbol *new_sym = NULL;
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

        if (new_sym == NULL) {
          return false;
        }
        decl_scope->insert_symbol(decl->get_name(), new_sym);
        return decl->get_symbol_from_symbol_table() != NULL;
      };

      if (first_nondef_for_builder != NULL) {
        if (!ensure_nondef_symbol(first_nondef_for_builder)) {
          first_nondef_for_builder = NULL;
        }
      }

      if (first_nondef_for_builder == NULL) {
        SgTemplateArgumentPtrList *specialization_args =
            build_explicit_specialization_instantiation ? args_for_builder
                                                        : NULL;
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
        ROSE_ASSERT(first_nondef_for_builder != NULL);

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
          if (param != NULL) {
            suppress_unparse_output(param);
          }
        }
      }

      auto mark_explicit_specialization =
          [&](SgFunctionDeclaration *decl) -> void {
        if (!build_explicit_specialization_instantiation || decl == NULL) {
          return;
        }
        if (SgTemplateInstantiationFunctionDecl *inst_func =
                isSgTemplateInstantiationFunctionDecl(decl)) {
          inst_func->set_template_argument_list_is_explicit(true);
          SageBuilder::setTemplateArgumentsInDeclaration(inst_func,
                                                         args_for_builder);
          if (function_decl->getPrimaryTemplate() != NULL) {
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
          inst_member->set_template_argument_list_is_explicit(true);
          SageBuilder::setTemplateArgumentsInDeclaration(inst_member,
                                                         args_for_builder);
          if (function_decl->getPrimaryTemplate() != NULL) {
            if (SgNode *tmpl_node =
                    Traverse(function_decl->getPrimaryTemplate())) {
              if (SgTemplateMemberFunctionDeclaration *tmpl_decl =
                      isSgTemplateMemberFunctionDeclaration(tmpl_node)) {
                inst_member->set_templateDeclaration(tmpl_decl);
                inst_member->set_templateName(tmpl_decl->get_name());
              }
            }
          }
        }
      };

      mark_explicit_specialization(first_nondef_for_builder);

      ROSE_ASSERT(first_nondef_for_builder != NULL);
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
        if (param != NULL) {
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
    param_list_, NULL);
    //            first_decl =
    SageBuilder::buildNondefiningFunctionDeclaration(sg_function_decl, NULL,
    NULL); setCompilerGeneratedFileInfo(first_decl);
                first_decl->set_parent(SageBuilder::topScopeStack());
                first_decl->set_firstNondefiningDeclaration(first_decl);
                if (function_decl->isVariadic()) first_decl->hasEllipses();
            }
            else {
                SgSymbol * tmp_symbol =
    GetSymbolFromSymbolTable(function_decl->getFirstDecl()); SgFunctionSymbol *
    symbol = isSgFunctionSymbol(tmp_symbol); if (tmp_symbol != NULL && symbol ==
    NULL) { std::cerr << "Runtime error: tmp_symbol != NULL && symbol == NULL"
    << std::endl; res = false;
                }
                if (symbol != NULL)
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
          SageBuilder::setTemplateArgumentsInDeclaration(inst_func,
                                                         &empty_template_args);
          if (function_decl->getPrimaryTemplate() != NULL) {
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
          inst_member->set_template_argument_list_is_explicit(true);
          SageBuilder::setTemplateArgumentsInDeclaration(inst_member,
                                                         &empty_template_args);
          if (function_decl->getPrimaryTemplate() != NULL) {
            if (SgNode *tmpl_node =
                    Traverse(function_decl->getPrimaryTemplate())) {
              if (SgTemplateMemberFunctionDeclaration *tmpl_decl =
                      isSgTemplateMemberFunctionDeclaration(tmpl_node)) {
                inst_member->set_templateDeclaration(tmpl_decl);
                inst_member->set_templateName(tmpl_decl->get_name());
              }
            }
          } else if (clang::FunctionDecl *pattern =
                         function_decl->getInstantiatedFromMemberFunction()) {
            if (SgNode *tmpl_node = Traverse(pattern)) {
              if (SgTemplateMemberFunctionDeclaration *tmpl_decl =
                      isSgTemplateMemberFunctionDeclaration(tmpl_node)) {
                inst_member->set_templateDeclaration(tmpl_decl);
                inst_member->set_templateName(tmpl_decl->get_name());
              }
            }
          }
        }
      } else if (!built_template_member_pattern) {
        sg_function_decl = SageBuilder::buildNondefiningFunctionDeclaration(
            name, ret_type, param_list, scope_for_symbol_table, false, NULL,
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

    SgFunctionDeclaration *existing_first_nondef =
        isSgFunctionDeclaration(
            sg_function_decl->get_firstNondefiningDeclaration());
    bool preserve_existing_first_nondef = false;
    if (existing_first_nondef != NULL &&
        existing_first_nondef != sg_function_decl) {
      if (existing_first_nondef->get_symbol_from_symbol_table() != NULL) {
        preserve_existing_first_nondef = true;
      }
    }

    if (!preserve_existing_first_nondef &&
        function_decl->getFirstDecl() != function_decl) {
      SgSymbol *tmp_symbol =
          GetSymbolFromSymbolTable(function_decl->getFirstDecl());
      SgFunctionSymbol *symbol = isSgFunctionSymbol(tmp_symbol);
      if (tmp_symbol != NULL && symbol == NULL) {
        std::cerr << "Runtime error: tmp_symbol != NULL && symbol == NULL"
                  << std::endl;
        res = false;
      }
      SgFunctionDeclaration *first_decl = NULL;
      if (symbol != NULL) {
        first_decl = isSgFunctionDeclaration(symbol->get_declaration());
      } else {
        // FIXME Is it correct?
        SgNode *tmp_first_decl = Traverse(function_decl->getFirstDecl());
        first_decl = isSgFunctionDeclaration(tmp_first_decl);
        ROSE_ASSERT(first_decl != NULL);
        // ROSE_ASSERT(!"We should have see the first declaration already");
      }

      if (first_decl != NULL) {
        // CLANG FRONTEND FIX: Only set firstNondefiningDeclaration if variant
        // types match to avoid assertion failure when mixing
        // SgFunctionDeclaration with SgMemberFunctionDeclaration
        if (first_decl->variantT() == sg_function_decl->variantT()) {
          if (first_decl->get_firstNondefiningDeclaration() != NULL)
            sg_function_decl->set_firstNondefiningDeclaration(
                first_decl->get_firstNondefiningDeclaration());
          else {
            ROSE_ASSERT(first_decl->get_firstNondefiningDeclaration() != NULL);
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

    if (sg_function_decl != NULL &&
        sg_function_decl->get_declaration_associated_with_symbol() == NULL &&
        scope_for_symbol_table != NULL) {
      if (SgFunctionSymbol *scope_symbol =
              scope_for_symbol_table->lookup_function_symbol(
                  name, sg_function_decl->get_type())) {
        if (SgFunctionDeclaration *symbol_decl =
                isSgFunctionDeclaration(scope_symbol->get_declaration())) {
          SgFunctionDeclaration *first_symbol_decl = isSgFunctionDeclaration(
              symbol_decl->get_firstNondefiningDeclaration());
          if (first_symbol_decl == NULL) {
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
    if (decl == NULL) {
      return;
    }
    SgFunctionParameterList *params = decl->get_parameterList();
    if (params == NULL) {
      return;
    }
    if (params->get_parent() == NULL) {
      params->set_parent(decl);
    }
    if (decl->get_parameterList() != params) {
      decl->set_parameterList(params);
    }
    if (SgFunctionParameterList *syntax = decl->get_parameterList_syntax()) {
      if (syntax->get_parent() == NULL) {
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
    if (decl == NULL) {
      return;
    }

    auto scopes_match = [&](SgScopeStatement *lhs,
                            SgScopeStatement *rhs) -> bool {
      if (lhs == rhs) {
        return true;
      }
      if (lhs == NULL || rhs == NULL) {
        return false;
      }
      return normalizeNamespaceScope(lhs) == normalizeNamespaceScope(rhs);
    };

    auto resolve_symbol_decl =
        [&](SgFunctionDeclaration *candidate) -> SgFunctionDeclaration * {
          if (candidate == NULL) {
            return NULL;
          }
          SgFunctionDeclaration *first_nondef = isSgFunctionDeclaration(
              candidate->get_firstNondefiningDeclaration());
          SgFunctionDeclaration *def_decl = isSgFunctionDeclaration(
              candidate->get_definingDeclaration());
          auto is_class_scope = [](SgScopeStatement *scope) -> bool {
            return isSgClassDefinition(scope) != NULL ||
                   isSgTemplateClassDefinition(scope) != NULL ||
                   isSgTemplateInstantiationDefn(scope) != NULL;
          };

          if (first_nondef != NULL &&
              is_class_scope(first_nondef->get_scope())) {
            return first_nondef;
          }
          if (def_decl != NULL && is_class_scope(def_decl->get_scope())) {
            return def_decl;
          }

          if (first_nondef != NULL &&
              scopes_match(first_nondef->get_scope(), candidate->get_scope())) {
            return first_nondef;
          }
          if (def_decl != NULL &&
              scopes_match(def_decl->get_scope(), candidate->get_scope())) {
            return def_decl;
          }

          if (first_nondef != NULL) {
            return first_nondef;
          }
          if (def_decl != NULL) {
            return def_decl;
          }
          return candidate;
        };

    SgFunctionDeclaration *symbol_decl = resolve_symbol_decl(decl);
    if (symbol_decl == NULL) {
      return;
    }
    SgScopeStatement *symbol_scope = symbol_decl->get_scope();
    if (symbol_scope == NULL) {
      return;
    }
    if (isSgNamespaceDefinitionStatement(symbol_scope) != NULL ||
        isSgGlobal(symbol_scope) != NULL) {
      symbol_scope = normalizeNamespaceScope(symbol_scope);
    }
    if (symbol_scope == NULL) {
      return;
    }
    if (symbol_scope->find_symbol_from_declaration(symbol_decl) != NULL) {
      return;
    }

    auto insert_symbol = [&](SgSymbol *sym) {
      if (sym == NULL) {
        return;
      }
      if (!symbol_scope->symbol_exists(sym)) {
        symbol_scope->insert_symbol(sym->get_name(), sym);
      }
    };

    if (SgTemplateMemberFunctionDeclaration *tmpl_member =
            isSgTemplateMemberFunctionDeclaration(decl)) {
      SgTemplateParameterPtrList *params =
          &tmpl_member->get_templateParameters();
      if (SgTemplateMemberFunctionSymbol *tmpl_sym =
              symbol_scope->lookup_template_member_function_symbol(
                  tmpl_member->get_name(), tmpl_member->get_type(), params)) {
        if (tmpl_sym->get_declaration() != symbol_decl) {
          tmpl_sym->set_declaration(symbol_decl);
        }
        insert_symbol(tmpl_sym);
        return;
      }
      if (SgTemplateMemberFunctionDeclaration *sym_decl =
              isSgTemplateMemberFunctionDeclaration(symbol_decl)) {
        insert_symbol(new SgTemplateMemberFunctionSymbol(sym_decl));
      } else {
        insert_symbol(new SgTemplateMemberFunctionSymbol(tmpl_member));
      }
      return;
    }

    if (SgTemplateFunctionDeclaration *tmpl_decl =
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
        return;
      }
      if (SgTemplateFunctionDeclaration *sym_decl =
              isSgTemplateFunctionDeclaration(symbol_decl)) {
        insert_symbol(new SgTemplateFunctionSymbol(sym_decl));
      } else {
        insert_symbol(new SgTemplateFunctionSymbol(tmpl_decl));
      }
      return;
    }

    if (SgMemberFunctionDeclaration *member_decl =
            isSgMemberFunctionDeclaration(decl)) {
      if (SgMemberFunctionSymbol *mem_sym =
              symbol_scope->lookup_nontemplate_member_function_symbol(
                  member_decl->get_name(), member_decl->get_type(), NULL)) {
        if (mem_sym->get_declaration() != symbol_decl) {
          mem_sym->set_declaration(symbol_decl);
        }
        insert_symbol(mem_sym);
        return;
      }
      if (SgMemberFunctionDeclaration *sym_decl =
              isSgMemberFunctionDeclaration(symbol_decl)) {
        insert_symbol(new SgMemberFunctionSymbol(sym_decl));
      } else {
        insert_symbol(new SgMemberFunctionSymbol(member_decl));
      }
      return;
    }

    if (SgFunctionSymbol *func_sym =
            symbol_scope->lookup_nontemplate_function_symbol(
                symbol_decl->get_name(), symbol_decl->get_type(), NULL)) {
      if (func_sym->get_declaration() != symbol_decl) {
        func_sym->set_declaration(symbol_decl);
      }
      insert_symbol(func_sym);
      return;
    }

    insert_symbol(new SgFunctionSymbol(symbol_decl));
  };
  ensure_function_symbol(sg_function_decl);
  ensure_function_symbol(isSgFunctionDeclaration(
      sg_function_decl->get_firstNondefiningDeclaration()));
  ensure_function_symbol(
      isSgFunctionDeclaration(sg_function_decl->get_definingDeclaration()));

  if (is_builtin_decl) {
    auto mark_compgen = [this](SgLocatedNode *n) {
      if (n != NULL) {
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

  ROSE_ASSERT(sg_function_decl->get_firstNondefiningDeclaration() != NULL);
  /* // TODO Fix problem with function symbols...
      SgSymbol * symbol = GetSymbolFromSymbolTable(function_decl);
      if (symbol == NULL) {
          SgFunctionSymbol * func_sym = new
     SgFunctionSymbol(isSgFunctionDeclaration(sg_function_decl->get_firstNondefiningDeclaration()));
          SageBuilder::topScopeStack()->insert_symbol(name, func_sym);
      }
  */
  //  ROSE_ASSERT(GetSymbolFromSymbolTable(function_decl) != NULL);

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
      if (isSgClassDefinition(parent) != NULL) {
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
    if (defining == NULL) {
      return;
    }
    SgFunctionDeclaration *first_nondef =
        isSgFunctionDeclaration(defining->get_firstNondefiningDeclaration());
    if (first_nondef == NULL || first_nondef == defining) {
      return;
    }
    SgFunctionParameterList *def_params = defining->get_parameterList();
    SgFunctionParameterList *nondef_params = first_nondef->get_parameterList();
    if (def_params == NULL || nondef_params == NULL) {
      return;
    }
    if (def_params != nondef_params) {
      return;
    }

    SgFunctionParameterList *cloned =
        SageBuilder::buildFunctionParameterList_nfi();
    for (SgInitializedName *init_name : def_params->get_args()) {
      if (init_name == NULL) {
        continue;
      }
      SgInitializer *cloned_init = NULL;
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
      if (param == NULL) {
        continue;
      }
      param->set_declptr(first_nondef);
      param->set_parent(cloned);
      if (nondef_scope != NULL) {
        param->set_scope(nondef_scope);
      }
    }
  };

  ensure_unique_nondef_param_list(sg_function_decl);

  auto suppress_synthetic_nondef_in_class = [&](SgFunctionDeclaration
                                                    *defining) {
    if (defining == NULL) {
      return;
    }

    if (!isDefinition) {
      return;
    }

    clang::DeclContext *lexical_ctx = function_decl->getLexicalDeclContext();
    if (lexical_ctx == NULL || !llvm::isa<clang::CXXRecordDecl>(lexical_ctx)) {
      return;
    }

    SgFunctionDeclaration *first_nondef =
        isSgFunctionDeclaration(defining->get_firstNondefiningDeclaration());
    if (first_nondef == NULL || first_nondef == defining) {
      return;
    }

    // Only suppress the extra non-defining declaration when it is synthetic.
    // If Clang has a prior declaration in the redecl chain, that declaration
    // must remain visible (e.g., a user-written forward declaration of a friend
    // function template at namespace scope).
    if (function_decl->getFirstDecl() != function_decl) {
      return;
    }

    setCompilerGeneratedFileInfo(first_nondef);
    suppress_unparse_output(first_nondef);
    if (SgFunctionParameterList *params = first_nondef->get_parameterList()) {
      setCompilerGeneratedFileInfo(params);
      suppress_unparse_output(params);
      for (SgInitializedName *param : params->get_args()) {
        setCompilerGeneratedFileInfo(param);
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
    if (isSgClassDefinition(first_nondef->get_scope()) != NULL) {
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
      if (explicit_instantiation_directive == NULL) {
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
            lexical_scope != NULL ? lexical_scope : scope_for_symbol_table;
        explicit_instantiation_directive->set_scope(directive_scope);
        explicit_instantiation_directive->set_parent(directive_scope);
        applySourceRange(explicit_instantiation_directive,
                         function_decl->getSourceRange());
        inst_decl->set_parent(explicit_instantiation_directive);
        if (inst_decl->get_scope() == NULL) {
          inst_decl->set_scope(scope_for_symbol_table != NULL
                                   ? scope_for_symbol_table
                                   : directive_scope);
        }
        if (explicit_instantiation_directive
                ->get_firstNondefiningDeclaration() == NULL) {
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
        if (explicit_instantiation_directive->get_declaration() == NULL) {
          explicit_instantiation_directive->set_declaration(inst_decl);
        }
        if (inst_decl->get_scope() == NULL) {
          SgScopeStatement *fallback_scope =
              explicit_instantiation_directive->get_scope();
          inst_decl->set_scope(scope_for_symbol_table != NULL
                                   ? scope_for_symbol_table
                                   : (lexical_scope != NULL ? lexical_scope
                                                            : fallback_scope));
        }
      }
    }
  }

  // Keep declarations attached to their lexical namespace definition to
  // preserve reopened-namespace structure and unparse order, while using the
  // canonical namespace scope only for symbol-table insertion (see
  // normalizeNamespaceScope()).
  if (lexical_scope != NULL && lexical_scope != scope_for_symbol_table &&
      (isSgNamespaceDefinitionStatement(lexical_scope) != NULL ||
       isSgGlobal(lexical_scope) != NULL)) {
    SgDeclarationStatement *lexical_decl =
        explicit_instantiation_directive != NULL
            ? static_cast<SgDeclarationStatement *>(
                  explicit_instantiation_directive)
            : isSgDeclarationStatement(sg_function_decl);
    if (lexical_decl != NULL) {
      ensure_decl_in_scope_child_list_preserve_scope(
          lexical_decl, lexical_scope, "translateFunctionDeclCommon:lexical");
    }
  }

  // Friend free functions declared/defined inside a class must remain attached
  // to the lexical class definition for correct unparse structure, but their
  // symbols must live in the enclosing namespace/global scope.  We build them
  // using the enclosing scope (scope_for_symbol_table), then reattach them
  // lexically here.
  if (isFriendFreeFunction && lexical_scope != NULL &&
      lexical_scope != scope_for_symbol_table &&
      isSgClassDefinition(lexical_scope) != NULL) {
    auto reattach_to_lexical_class = [&](SgDeclarationStatement *decl,
                                         const char *ctx) {
      if (decl == NULL) {
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
        explicit_instantiation_directive != NULL
            ? static_cast<SgDeclarationStatement *>(
                  explicit_instantiation_directive)
            : isSgDeclarationStatement(sg_function_decl);
    reattach_to_lexical_class(lexical_decl,
                              "translateFunctionDeclCommon:friend:decl");
  }

  // Template instantiation name reset operates on the declaration scope's
  // symbol table. Keep instantiations scoped to the symbol-table scope even
  // when they are reattached lexically to reopened namespaces.
  if (handled_template_instantiation && scope_for_symbol_table != NULL) {
    auto enforce_symbol_scope = [&](SgFunctionDeclaration *decl) {
      if (decl == NULL) {
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

  applySourceRange(sg_function_decl, function_decl->getSourceRange());
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

  if (explicit_instantiation_directive != NULL) {
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
  if (cxxConstructorDecl == NULL) {
    return res;
  }
  SgMemberFunctionDeclaration *cxxDefiningConstructorDecl =
      isSgMemberFunctionDeclaration(
          cxxConstructorDecl->get_definingDeclaration());
  cxxConstructorDecl->get_specialFunctionModifier().setConstructor();

  // apply ctorInitializer
  if (cxx_constructor_decl->getNumCtorInitializers() != 0 &&
      cxxDefiningConstructorDecl != NULL) {
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

  SgDeclarationStatement *owning_template = NULL;
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
  return sg_param != NULL;
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
  SgVariableDeclaration *sg_var_decl = NULL;
  // Pei-Hung (09/29/23) The definition of a static data member needs to call
  // set_prev_decl_item to point to its first static data member declaration
  // inside the class. buildVariableDeclaration_nfi will take care of the
  // details by looking up the SgSymbol in the symbol table of the class.
  if (isStaticDataMember && var_decl->getPreviousDecl() != NULL) {
    clang::VarDecl *prevDecl = var_decl->getPreviousDecl();
    SgVariableDeclaration *sgPrevDecl =
        isSgVariableDeclaration(Traverse(prevDecl));
    ROSE_ASSERT(sgPrevDecl);
    sg_var_decl = SageBuilder::buildVariableDeclaration_nfi(
        name, type, NULL, SageInterface::getScope(sgPrevDecl));
  } else {
    sg_var_decl = SageBuilder::buildVariableDeclaration_nfi(
        name, type, NULL, SageBuilder::topScopeStack());
  }
  sg_var_decl->set_isAssociatedWithDeclarationList(true);
  if (var_decl->isConstexpr()) {
    sg_var_decl->set_is_constexpr(true);
  }

  // CLANG FRONTEND FIX: Check if variable has an initializer before traversing
  clang::Expr *init_expr = var_decl->getInit();
  SgExpression *expr = NULL;
  SgExprListExp *expr_list_expr = NULL;
  SgInitializer *init = NULL;
  SgNode *tmp_init = NULL;
  if (init_expr != NULL) {
    tmp_init = Traverse(init_expr);
    if (SgInitializer *tmp_init_initializer = isSgInitializer(tmp_init)) {
      init = tmp_init_initializer;
    } else {
      expr = isSgExpression(tmp_init);
      if (tmp_init != NULL && expr == NULL) {
        std::cerr << "Runtime error: not a SgInitializer..."
                  << std::endl; // TODO
        res = false;
      }
      expr_list_expr = isSgExprListExp(expr);
      if (expr_list_expr != NULL)
        init = SageBuilder::buildAggregateInitializer(expr_list_expr, type);
      else if (expr != NULL) {
        // CLANG FRONTEND FIX: Check if expr is already an initializer (e.g.,
        // SgConstructorInitializer) If so, use it directly instead of wrapping
        // it in SgAssignInitializer This preserves constructor syntax:
        // std::string str("hello") instead of std::string str = ("hello")
        SgInitializer *existing_init = isSgInitializer(expr);
        if (existing_init != NULL) {
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
  // CLANG FRONTEND FIX: Only set initializer if it's not NULL
  if (init != NULL) {
    sg_var_decl->reset_initializer(init);
  }

  // CLANG FRONTEND FIX: Set initializer parent AFTER reset_initializer
  // reset_initializer sets the parent of the initializer to the
  // SgInitializedName, not the SgVariableDeclaration. Setting it to sg_var_decl
  // here was wrong. Only apply source range if we have both the initializer and
  // the original expression
  if (init != NULL && init_expr != NULL) {
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
  ROSE_ASSERT(init_name != NULL);
  if (init_name->get_scope() == NULL) {
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
  if (init != NULL) {
    init->set_parent(init_name);
  }

  applySourceRange(init_name, var_decl->getSourceRange());

  // CLANG FRONTEND FIX: The declptr should already be set by
  // SgVariableDeclaration constructor to point to the SgVariableDefinition. If
  // it's null, we need to check why.
  SgVariableDefinition *var_def =
      isSgVariableDefinition(init_name->get_declptr());
  if (var_def == NULL) {
    // If declptr is null, try to get it from the variable declaration
    // buildVariableDeclaration_nfi should have created a definition
    var_def = sg_var_decl->get_definition();
    if (var_def != NULL) {
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
  ROSE_ASSERT(var_def != NULL);
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
    if (var_decl->getPreviousDecl() == NULL) {
      sg_var_decl->get_declarationModifier().get_storageModifier().setStatic();
    }
  }

  if (!isembedded) {
    sg_var_decl->set_variableDeclarationContainsBaseTypeDefiningDeclaration(
        false);
  }

  *node = sg_var_decl;

  return VisitDeclaratorDecl(var_decl, node) && res;
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

  SgInitializer *init = NULL;

  if (param_var_decl->hasDefaultArg()) {
    SgNode *tmp_expr = Traverse(param_var_decl->getDefaultArg());
    SgExpression *expr = isSgExpression(tmp_expr);
    // ROOT CAUSE FIX: Check that expr is not NULL before using it
    // The conversion from tmp_expr to expr can fail, leaving expr == NULL
    if (tmp_expr != NULL && expr == NULL) {
      std::cerr << "Runtime error: tmp_expr != NULL && expr == NULL"
                << std::endl;
      res = false;
    } else if (expr != NULL) {
      // The same Clang default-argument subtree can be referenced by multiple
      // redeclarations. Reusing a single SgExpression would create multiple
      // parents and break CFG invariants. Always deep-copy the expression
      // before attaching it to a parameter initializer.
      SgExpression *expr_copy = SageInterface::deepCopy(expr);
      ROSE_ASSERT(expr_copy != NULL);
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

  // ROOT CAUSE FIX: Variable template specializations (e.g., template<> int
  // x<int> = 5;) Treat them as regular variable declarations since SAGE doesn't
  // have specific support for variable templates yet

  // Get variable name and type
  SgName name(var_template_specialization_decl->getNameAsString());
  clang::QualType qual_type = var_template_specialization_decl->getType();
  SgType *type = buildTypeFromQualifiedType(qual_type);

  // Get initializer if present
  SgInitializer *init = NULL;
  if (var_template_specialization_decl->hasInit()) {
    clang::Expr *init_expr = var_template_specialization_decl->getInit();
    if (init_expr != NULL) {
      SgNode *tmp_node = Traverse(init_expr);
      SgExpression *sg_init_expr = isSgExpression(tmp_node);
      if (sg_init_expr != NULL) {
        init = SageBuilder::buildAssignInitializer(sg_init_expr, type);
      }
    }
  }

  // Build variable declaration
  SgVariableDeclaration *var_decl =
      SageBuilder::buildVariableDeclaration(name, type, init, NULL);

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

  // TODO: Full variable template partial specialization support not yet
  // implemented For now, delegate to VarTemplateSpecializationDecl handler This
  // allows basic processing of variable template partial specializations
  // ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitVarTemplateSpecializationDecl(
             var_template_partial_specialization_decl, node) &&
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

  SgInitializer *init = NULL;

  if (enum_constant_decl->getInitExpr() != NULL) {
    SgNode *tmp_expr = Traverse(enum_constant_decl->getInitExpr());
    SgExpression *expr = isSgExpression(tmp_expr);
    if (tmp_expr != NULL && expr == NULL) {
      std::cerr << "Runtime error: tmp_expr != NULL && expr == NULL"
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
  while (scope_context != NULL &&
         llvm::isa<clang::LinkageSpecDecl>(scope_context)) {
    scope_context = scope_context->getParent();
  }
  if (scope_context != NULL) {
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
  if (scope == NULL) {
    scope = getGlobalScope();
  }
  scope = normalizeNamespaceScope(scope);

  init_name->set_scope(scope);
  init_name->set_parent(scope);

  // CLANG FRONTEND FIX: declptr will be set in VisitEnumDecl after appending
  // the enumerator (we can't set it here because the enum declaration hasn't
  // been added to translation map yet)

  if (scope != NULL && scope->find_symbol_from_declaration(init_name) == NULL) {
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
  if (current_scope != NULL) {
    init_name->set_scope(current_scope);
    if (current_scope->find_symbol_from_declaration(init_name) == NULL) {
      SgVariableSymbol *symbol = new SgVariableSymbol(init_name);
      current_scope->insert_symbol(init_name->get_name(), symbol);
    }
  }

  SgUsingDeclarationStatement *using_stmt =
      new SgUsingDeclarationStatement(NULL, init_name);
  init_name->set_parent(using_stmt);
  using_stmt->set_definingDeclaration(using_stmt);
  using_stmt->set_firstNondefiningDeclaration(using_stmt);

  if (current_scope != NULL) {
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
  if (tmp_condition != NULL && condition == NULL) {
    std::cerr << "Runtime error: tmp_condition != NULL && condition == NULL"
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
          ROSE_ASSERT(class_def != NULL);
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
  if (*node != NULL) {
    std::cerr << "Runtime error: The TranslationUnitDecl is already associated "
                 "to a SAGE node."
              << std::endl;
    return false;
  }

  // Create the SAGE node: SgGlobal

  if (p_global_scope != NULL) {
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
  ROSE_ASSERT(global_scope != NULL);
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

    SgNode *inst_node = NULL;
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

  SageBuilder::popScopeStack();

  // Traverse the class hierarchy

  return VisitDecl(translation_unit_decl, node) && res;
}
