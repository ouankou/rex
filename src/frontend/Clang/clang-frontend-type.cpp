#include "clang-decl-utils.hpp"
#include "clang-floating-value.hpp"
#include "clang-frontend-private.hpp"
#include "clang-integer-value.hpp"
#include "clang-nns-utils.hpp"
#include "clang-source-qualification.hpp"

#include "AstNodes/Expression/ExpressionTypeClassification.h"
#include "RoseAst.h"
#include "nonrealQualificationSupport.h"
#include "sage3basic.h"

#include "sageInterface.h"

#include <algorithm>
#include <cinttypes>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include <clang/AST/APValue.h>
#include <clang/AST/TemplateBase.h>

#include <clang/Basic/AttrKinds.h>
#include <clang/Basic/Module.h>
#include <clang/Basic/OperatorKinds.h>
#include <clang/Lex/Lexer.h>

#include <llvm/ADT/FoldingSet.h>
#include <llvm/ADT/SmallPtrSet.h>
#include <llvm/ADT/SmallString.h>
#include <llvm/Support/SaveAndRestore.h>

#include <functional>
#include <limits>
#include <unordered_set>
namespace {
SgValueExp *buildConstantArrayBound(const clang::ConstantArrayType *type) {
  const llvm::APInt &size = type->getSize();
  ROSE_ASSERT(size.getActiveBits() <=
              std::numeric_limits<unsigned long long>::digits);
  SgValueExp *extent = nullptr;
  if (size.getActiveBits() <= std::numeric_limits<int>::digits)
    extent =
        SageBuilder::buildIntVal_nfi(static_cast<int>(size.getZExtValue()));
  else
    extent =
        SageBuilder::buildUnsignedLongLongIntVal_nfi(size.getZExtValue(), "");
  extent->set_literal_spelling_form(SgValueExp::e_literal_canonical_generated);
  SageBuilder::initializeSemanticExpressionSourceProvenance(extent);
  return extent;
}

SgTemplateType *buildUnexpandedTemplateParameterUse(SgTemplateType *parameter) {
  ASSERT_not_null(parameter);
  ROSE_ASSERT(parameter->get_packed());
  ROSE_ASSERT(parameter->get_tpl_args().empty());
  ROSE_ASSERT(!parameter->get_is_pack_substitution());
  ROSE_ASSERT(parameter->get_template_parameter_depth() >= 0);
  ROSE_ASSERT(parameter->get_template_parameter_position() >= 0);
  ROSE_ASSERT(parameter->get_canonical_source_identity().has_value());
  auto *use = new SgTemplateType(parameter->get_name());
  use->set_template_parameter_position(
      parameter->get_template_parameter_position());
  use->set_template_parameter_depth(parameter->get_template_parameter_depth());
  use->initialize_canonical_source_identity(
      *parameter->get_canonical_source_identity());
  use->set_template_parameter(parameter->get_template_parameter());
  return use;
}

[[noreturn]] void failExactTypeTranslation(const char *context,
                                           const clang::Type *type) {
  std::cerr << "REX_FRONTEND_INVARIANT[" << context
            << "]: exact type translation failed";
  if (type != nullptr) {
    std::cerr << " for Clang " << type->getTypeClassName();
    if (const clang::BuiltinType *builtin =
            llvm::dyn_cast<clang::BuiltinType>(type)) {
      std::cerr << " kind=" << static_cast<unsigned>(builtin->getKind());
    }
  }
  std::cerr << std::endl;
  ROSE_ABORT();
}

SgType *requireExactType(SgType *type, const char *context,
                         const clang::Type *clang_type) {
  if (type == nullptr || SageInterface::containsUnknownType(type)) {
    failExactTypeTranslation(context, clang_type);
  }
  return type;
}

clang::QualType memberPointerClassType(const clang::MemberPointerType *type) {
  if (const clang::Type *owner = type->getQualifier().getAsType())
    return clang::QualType(owner, 0);
  if (clang::CXXRecordDecl *record = type->getMostRecentCXXRecordDecl())
    return record->getASTContext().getTypeDeclType(
        static_cast<const clang::TypeDecl *>(record));
  failExactTypeTranslation("member-pointer-class", type);
}

SgType *requireExactDecltypeSemanticBase(
    const clang::DecltypeType *decltype_type, SgExpression *expression,
    SgType *translated_underlying_type, const char *context) {
  if (translated_underlying_type != nullptr &&
      isSgTypeDependent(translated_underlying_type) == nullptr) {
    return requireExactType(translated_underlying_type, context, decltype_type);
  }
  if (decltype_type == nullptr || expression == nullptr ||
      (!decltype_type->isDependentType() &&
       !decltype_type->isInstantiationDependentType())) {
    failExactTypeTranslation(context, decltype_type);
  }

  // An unresolved decltype has a null or generic dependent underlying type.
  // Its expression owns the exact dependent identity until instantiation.
  SgType *expression_type = expression->get_type();
  if (SageInterface::containsUnknownType(expression_type)) {
    std::cerr << "REX_FRONTEND_INVARIANT[" << context
              << "]: dependent decltype expression " << expression->class_name()
              << " has no exact dependent semantic type" << std::endl;
    ROSE_ABORT();
  }
  return expression_type;
}

unsigned int roseMemberFunctionSpecifierFromClangProto(
    const clang::FunctionProtoType *type) {
  ASSERT_not_null(type);

  unsigned int result = 0;
  const clang::Qualifiers qualifiers = type->getMethodQuals();
  if (qualifiers.hasConst()) {
    result |= SgMemberFunctionType::e_const;
  }
  if (qualifiers.hasVolatile()) {
    result |= SgMemberFunctionType::e_volatile;
  }
  if (qualifiers.hasRestrict()) {
    result |= SgMemberFunctionType::e_restrict;
  }

  switch (type->getRefQualifier()) {
  case clang::RQ_LValue:
    result |= SgMemberFunctionType::e_ref_qualifier_lvalue;
    break;
  case clang::RQ_RValue:
    result |= SgMemberFunctionType::e_ref_qualifier_rvalue;
    break;
  case clang::RQ_None:
    break;
  }

  return result;
}

void requireExactFunctionArgumentQualificationUseSites(
    const SgFunctionType *function_type, const char *context) {
  ASSERT_not_null(function_type);
  SgFunctionParameterTypeList *arguments = function_type->get_argument_list();
  ASSERT_not_null(arguments);
  arguments->validate_argument_qualification_use_sites(context);
}

bool nestedNameSpecifierHasTypeQualifier(clang::NestedNameSpecifier qualifier) {

  for (clang::NestedNameSpecifier nns = qualifier; nns;
       nns = nestedNameSpecifierPrefix(nns)) {

    if (nns.getKind() == clang::NestedNameSpecifier::Kind::Type) {
      return true;
    }
  }
  return false;
}

bool nestedNameSpecifierHasDependentTypeQualifier(
    clang::NestedNameSpecifier qualifier) {

  for (clang::NestedNameSpecifier nns = qualifier; nns;
       nns = nestedNameSpecifierPrefix(nns)) {

    if (nns.getKind() != clang::NestedNameSpecifier::Kind::Type) {
      continue;
    }
    const clang::Type *qualifier_type = nns.getAsType();
    if (qualifier_type != nullptr && qualifier_type->isDependentType()) {
      return true;
    }
  }
  return false;
}

bool nestedNameSpecifierHasNamespaceQualifier(
    clang::NestedNameSpecifier qualifier) {

  for (clang::NestedNameSpecifier nns = qualifier; nns;
       nns = nestedNameSpecifierPrefix(nns)) {

    switch (nns.getKind()) {
    case clang::NestedNameSpecifier::Kind::Namespace:
    case clang::NestedNameSpecifier::Kind::Global:
      return true;
    default:
      break;
    }
  }
  return false;
}

bool decltypeTypeLocUsesGNUKeyword(const clang::DecltypeTypeLoc &decltype_loc,
                                   clang::CompilerInstance &compiler_instance) {
  if (!compiler_instance.hasSourceManager() ||
      decltype_loc.getDecltypeLoc().isInvalid()) {
    failExactTypeTranslation("decltype-keyword-source",
                             decltype_loc.getTypePtr());
  }

  clang::SourceManager &source_manager = compiler_instance.getSourceManager();
  const clang::LangOptions &lang_opts = compiler_instance.getLangOpts();
  clang::SourceLocation spelling_loc =
      source_manager.getSpellingLoc(decltype_loc.getDecltypeLoc());
  if (spelling_loc.isInvalid()) {
    failExactTypeTranslation("decltype-keyword-source",
                             decltype_loc.getTypePtr());
  }

  clang::Token token;
  if (clang::Lexer::getRawToken(spelling_loc, token, source_manager, lang_opts,
                                /*IgnoreWhiteSpace=*/true)) {
    failExactTypeTranslation("decltype-keyword-token",
                             decltype_loc.getTypePtr());
  }

  bool invalid = false;
  const std::string spelling =
      clang::Lexer::getSpelling(token, source_manager, lang_opts, &invalid);
  if (invalid || (spelling != "decltype" && spelling != "__decltype")) {
    failExactTypeTranslation("decltype-keyword-spelling",
                             decltype_loc.getTypePtr());
  }
  return spelling == "__decltype";
}

int nestedNameSpecifierComponentCount(clang::NestedNameSpecifier qualifier) {
  int count = 0;
  for (clang::NestedNameSpecifier nns = qualifier; nns;
       nns = nestedNameSpecifierPrefix(nns)) {
    switch (nns.getKind()) {
    case clang::NestedNameSpecifier::Kind::Namespace:
    case clang::NestedNameSpecifier::Kind::Type:
      ++count;
      break;
    case clang::NestedNameSpecifier::Kind::Global:
    case clang::NestedNameSpecifier::Kind::MicrosoftSuper:
    case clang::NestedNameSpecifier::Kind::Null:
      break;
    }
  }
  return count;
}

bool applyDirectDeclRefSourceSurface(const clang::DeclRefExpr *source_reference,
                                     clang::CompilerInstance *compiler_instance,
                                     SgExpression *semantic_expression) {
  ASSERT_not_null(source_reference);
  ASSERT_not_null(compiler_instance);
  ASSERT_not_null(semantic_expression);

  clang::NestedNameSpecifierLoc qualifier_loc =
      source_reference->getQualifierLoc();
  SourceQualification source_qualification;
  int structural_depth = 0;
  if (qualifier_loc) {
    source_qualification = sourceQualificationFromNestedNameSpecifierLoc(
        qualifier_loc, compiler_instance,
        "semantic-template-declaration-reference");
    clang::NestedNameSpecifier qualifier =
        qualifier_loc.getNestedNameSpecifier();
    structural_depth = nestedNameSpecifierComponentCount(qualifier);
    if (!sourceQualificationMatchesSemanticIdentity(
            source_qualification, qualifier,
            nestedNameSpecifierLocHasExplicitGlobal(qualifier_loc),
            static_cast<std::size_t>(structural_depth))) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[semantic-template-reference-surface]: "
              "direct declaration reference has divergent source and "
              "semantic qualifier identities\n");
      ROSE_ABORT();
    }
  }

  auto apply = [&](auto *reference) {
    if (reference == nullptr) {
      return false;
    }
    if (sourceQualificationIsSemanticMacroFragment(source_qualification)) {
      if (structural_depth <= 0) {
        fprintf(stderr, "REX_FRONTEND_INVARIANT[semantic-template-reference-"
                        "surface]: semantic macro qualifier has no structural "
                        "components\n");
        ROSE_ABORT();
      }
      reference->set_explicit_name_qualification_length(structural_depth);
      reference->set_explicit_global_qualification(false);
      reference->set_explicit_name_qualification_tokens({});
    } else {
      reference->set_explicit_name_qualification_length(
          static_cast<int>(source_qualification.tokens.size()));
      reference->set_explicit_global_qualification(source_qualification.global);
      reference->set_explicit_name_qualification_tokens(
          source_qualification.tokens);
    }
    return true;
  };

  if (apply(isSgVarRefExp(semantic_expression)) ||
      apply(isSgTemplateMemberFunctionRefExp(semantic_expression)) ||
      apply(isSgMemberFunctionRefExp(semantic_expression)) ||
      apply(isSgTemplateFunctionRefExp(semantic_expression)) ||
      apply(isSgFunctionRefExp(semantic_expression)) ||
      apply(isSgNonrealRefExp(semantic_expression)) ||
      apply(isSgEnumVal(semantic_expression))) {
    return true;
  }
  return false;
}

clang::ElaboratedTypeKeyword exactWrittenElaboratedKeyword(
    clang::TypeLoc type_loc, clang::SourceRange exact_written_range,
    const clang::CompilerInstance *compiler_instance) {

  ASSERT_not_null(compiler_instance);
  const clang::SourceManager &source_manager =
      compiler_instance->getSourceManager();
  clang::SourceLocation range_begin = exact_written_range.getBegin();
  clang::SourceLocation range_end = exact_written_range.getEnd();
  if (range_begin.isInvalid() || range_end.isInvalid()) {
    return clang::ElaboratedTypeKeyword::None;
  }
  range_begin = range_begin.isMacroID()
                    ? source_manager.getExpansionLoc(range_begin)
                    : source_manager.getFileLoc(range_begin);
  range_end = range_end.isMacroID() ? source_manager.getExpansionLoc(range_end)
                                    : source_manager.getFileLoc(range_end);
  if (range_begin.isInvalid() || range_end.isInvalid() ||
      source_manager.getFileID(range_begin) !=
          source_manager.getFileID(range_end) ||
      source_manager.isBeforeInTranslationUnit(range_end, range_begin)) {
    return clang::ElaboratedTypeKeyword::None;
  }

  auto belongs_to_exact_type_surface =
      [&](clang::SourceLocation keyword_location) {
        if (keyword_location.isInvalid()) {
          return false;
        }
        keyword_location =
            keyword_location.isMacroID()
                ? source_manager.getExpansionLoc(keyword_location)
                : source_manager.getFileLoc(keyword_location);
        return keyword_location.isValid() &&
               source_manager.getFileID(keyword_location) ==
                   source_manager.getFileID(range_begin) &&
               !source_manager.isBeforeInTranslationUnit(keyword_location,
                                                         range_begin) &&
               !source_manager.isBeforeInTranslationUnit(range_end,
                                                         keyword_location);
      };

  auto source_token_spells_keyword = [&](clang::SourceLocation keyword_location,
                                         clang::ElaboratedTypeKeyword keyword) {
    if (keyword == clang::ElaboratedTypeKeyword::None ||
        !belongs_to_exact_type_surface(keyword_location)) {
      return false;
    }
    const clang::SourceLocation spelling_location =
        source_manager.getSpellingLoc(keyword_location);
    if (spelling_location.isInvalid()) {
      fprintf(stderr, "REX_FRONTEND_INVARIANT[exact-elaborated-type-keyword]: "
                      "exact keyword location has no spelling location\n");
      ROSE_ABORT();
    }
    const clang::LangOptions &language_options =
        compiler_instance->getLangOpts();
    clang::Token token;
    if (clang::Lexer::getRawToken(spelling_location, token, source_manager,
                                  language_options,
                                  /*IgnoreWhiteSpace=*/true)) {
      fprintf(stderr, "REX_FRONTEND_INVARIANT[exact-elaborated-type-keyword]: "
                      "exact keyword location has no source token\n");
      ROSE_ABORT();
    }
    if (token.is(clang::tok::raw_identifier)) {
      ASSERT_not_null(
          compiler_instance->getPreprocessor().LookUpIdentifierInfo(token));
    }
    switch (keyword) {
    case clang::ElaboratedTypeKeyword::None:
      return false;
    case clang::ElaboratedTypeKeyword::Typename:
      return token.is(clang::tok::kw_typename);
    case clang::ElaboratedTypeKeyword::Class:
      return token.is(clang::tok::kw_class);
    case clang::ElaboratedTypeKeyword::Struct:
      return token.is(clang::tok::kw_struct);
    case clang::ElaboratedTypeKeyword::Union:
      return token.is(clang::tok::kw_union);
    case clang::ElaboratedTypeKeyword::Enum:
      return token.is(clang::tok::kw_enum);
    case clang::ElaboratedTypeKeyword::Interface:
      return token.is(clang::tok::kw___interface);
    }
    ROSE_ABORT();
  };

  clang::ElaboratedTypeKeyword result = clang::ElaboratedTypeKeyword::None;
  auto record = [&](clang::SourceLocation keyword_location,
                    clang::ElaboratedTypeKeyword keyword) {
    if (!source_token_spells_keyword(keyword_location, keyword)) {
      return;
    }
    if (result != clang::ElaboratedTypeKeyword::None && result != keyword) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[exact-elaborated-type-keyword]: one "
              "exact TypeLoc has conflicting source keywords\n");
      ROSE_ABORT();
    }
    result = keyword;
  };

  // Type objects are shared semantic identities.  A TypedefType or template
  // specialization can retain a keyword from a different declaration surface.
  // Some TypeLoc classes also use the identifier as their elaborated-keyword
  // location while exposing the declaration's semantic tag kind.  The exact
  // source range, typed keyword, and source token must all agree before REX may
  // publish an elaborated source use.
  for (clang::TypeLoc current = type_loc; !current.isNull();
       current = current.getNextTypeLoc()) {
    if (auto injected_loc = current.getAs<clang::InjectedClassNameTypeLoc>()) {
      record(injected_loc.getElaboratedKeywordLoc(),
             injected_loc.getTypePtr()->getKeyword());
    } else if (auto tag_loc = current.getAs<clang::TagTypeLoc>()) {
      record(tag_loc.getElaboratedKeywordLoc(),
             tag_loc.getTypePtr()->getKeyword());
    } else if (auto typedef_loc = current.getAs<clang::TypedefTypeLoc>()) {
      record(typedef_loc.getElaboratedKeywordLoc(),
             typedef_loc.getTypePtr()->getKeyword());
    } else if (auto using_loc = current.getAs<clang::UsingTypeLoc>()) {
      record(using_loc.getElaboratedKeywordLoc(),
             using_loc.getTypePtr()->getKeyword());
    } else if (auto unresolved_using_loc =
                   current.getAs<clang::UnresolvedUsingTypeLoc>()) {
      record(unresolved_using_loc.getElaboratedKeywordLoc(),
             unresolved_using_loc.getTypePtr()->getKeyword());
    } else if (auto template_specialization_loc =
                   current.getAs<clang::TemplateSpecializationTypeLoc>()) {
      record(template_specialization_loc.getElaboratedKeywordLoc(),
             template_specialization_loc.getTypePtr()->getKeyword());
    } else if (auto deduced_template_specialization_loc =
                   current
                       .getAs<clang::DeducedTemplateSpecializationTypeLoc>()) {
      record(deduced_template_specialization_loc.getElaboratedKeywordLoc(),
             deduced_template_specialization_loc.getTypePtr()->getKeyword());
    } else if (auto dependent_name_loc =
                   current.getAs<clang::DependentNameTypeLoc>()) {
      record(dependent_name_loc.getElaboratedKeywordLoc(),
             dependent_name_loc.getTypePtr()->getKeyword());
    }
  }
  return result;
}

bool typeLocSpellsElaboratedKeyword(
    clang::TypeLoc type_loc, clang::SourceRange exact_written_range,
    const clang::CompilerInstance *compiler_instance) {
  return exactWrittenElaboratedKeyword(type_loc, exact_written_range,
                                       compiler_instance) !=
         clang::ElaboratedTypeKeyword::None;
}

clang::NestedNameSpecifierLoc typeLocQualifierLoc(clang::TypeLoc type_loc) {

  for (clang::TypeLoc current = type_loc; !current.isNull();
       current = current.getNextTypeLoc()) {
    if (auto spec_loc = current.getAs<clang::TemplateSpecializationTypeLoc>()) {
      clang::NestedNameSpecifierLoc qualifier_loc = spec_loc.getQualifierLoc();
      if (qualifier_loc) {
        return qualifier_loc;
      }
    }
    if (auto dep_name_loc = current.getAs<clang::DependentNameTypeLoc>()) {
      if (clang::NestedNameSpecifierLoc qualifier_loc =
              dep_name_loc.getQualifierLoc()) {
        return qualifier_loc;
      }
    }
    if (auto typedef_loc = current.getAs<clang::TypedefTypeLoc>()) {
      if (clang::NestedNameSpecifierLoc qualifier_loc =
              typedef_loc.getQualifierLoc()) {
        return qualifier_loc;
      }
    }
    if (auto using_loc = current.getAs<clang::UsingTypeLoc>()) {
      if (clang::NestedNameSpecifierLoc qualifier_loc =
              using_loc.getQualifierLoc()) {
        return qualifier_loc;
      }
    }
    if (auto unresolved_using_loc =
            current.getAs<clang::UnresolvedUsingTypeLoc>()) {
      if (clang::NestedNameSpecifierLoc qualifier_loc =
              unresolved_using_loc.getQualifierLoc()) {
        return qualifier_loc;
      }
    }
    if (auto tag_loc = current.getAs<clang::TagTypeLoc>()) {
      if (clang::NestedNameSpecifierLoc qualifier_loc =
              tag_loc.getQualifierLoc()) {
        return qualifier_loc;
      }
    }
    if (auto injected_loc = current.getAs<clang::InjectedClassNameTypeLoc>()) {
      if (clang::NestedNameSpecifierLoc qualifier_loc =
              injected_loc.getQualifierLoc()) {
        return qualifier_loc;
      }
    }
  }

  return clang::NestedNameSpecifierLoc();
}

bool canCacheTypeTranslation(const clang::Type *candidate) {

  if (candidate == nullptr) {
    return false;
  }

  if (candidate->isDependentType() ||
      candidate->isInstantiationDependentType() ||
      candidate->containsUnexpandedParameterPack()) {
    return false;
  }

  switch (candidate->getTypeClass()) {
  case clang::Type::TemplateTypeParm:
  case clang::Type::SubstTemplateTypeParm:
  case clang::Type::SubstTemplateTypeParmPack:
    // Template-parameter type spelling depends on the current template scope.
    // Clang canonicalizes these types by depth/index, so caching solely by raw
    // type pointer leaks names across unrelated templates.
    return false;
  case clang::Type::Typedef: {
    const clang::TypedefType *typedef_type =
        llvm::cast<clang::TypedefType>(candidate);
    const clang::TypedefNameDecl *decl =
        llvm::dyn_cast_or_null<clang::TypedefNameDecl>(typedef_type->getDecl());
    clang::DeclContext *ctx =
        decl != nullptr
            ? const_cast<clang::DeclContext *>(decl->getDeclContext())
            : nullptr;
    return !(ctx != nullptr && ctx->isRecord());
  }
  case clang::Type::Using: {
    const clang::UsingType *using_type =
        llvm::cast<clang::UsingType>(candidate);
    const clang::UsingShadowDecl *decl =
        llvm::dyn_cast_or_null<clang::UsingShadowDecl>(using_type->getDecl());
    clang::DeclContext *ctx =
        decl != nullptr
            ? const_cast<clang::DeclContext *>(decl->getDeclContext())
            : nullptr;
    return !(ctx != nullptr && ctx->isRecord());
  }
  default:
    return true;
  }
}

bool requiresSourceOwnedDependentTypeIdentity(
    const clang::QualType &qual_type) {
  const clang::Type *type = qual_type.getTypePtrOrNull();
  return type != nullptr &&
         (type->isDependentType() || type->isInstantiationDependentType() ||
          type->containsUnexpandedParameterPack());
}

bool canCacheQualifiedTypeTranslation(const clang::QualType &qual_type) {
  return !qual_type.isNull() && qual_type.hasLocalQualifiers() &&
         canCacheTypeTranslation(qual_type.getTypePtr());
}

SgDeclarationStatement *
normalizeNonrealTemplateDeclarationTarget(SgDeclarationStatement *decl) {
  if (decl == nullptr) {
    return nullptr;
  }

  if (SgTemplateInstantiationDirectiveStatement *directive =
          isSgTemplateInstantiationDirectiveStatement(decl)) {
    decl = directive->get_declaration();
    ASSERT_not_null(decl);
  }

  if (SgClassDeclaration *class_decl = isSgClassDeclaration(decl)) {
    if (SgClassDeclaration *first_nondef = isSgClassDeclaration(
            class_decl->get_firstNondefiningDeclaration())) {
      decl = first_nondef;
    }
  }

  return decl;
}

bool isExactSimpleNonrealTemplateBase(SgDeclarationStatement *declaration,
                                      const SgName &expected_name) {
  SgNonrealDecl *nonreal = isSgNonrealDecl(declaration);
  if (nonreal == nullptr ||
      nonreal->get_nonreal_template_role() !=
          SgNonrealDecl::e_nonreal_template_none ||
      !nonreal->get_tpl_args().empty() ||
      nonreal->get_name() != expected_name || nonreal->get_type() == nullptr ||
      nonreal->get_type()->get_declaration() != nonreal ||
      nonreal->get_type()->get_parent() != nonreal ||
      nonreal->get_scope() == nullptr ||
      nonreal->get_parent() != nonreal->get_scope() ||
      nonreal->get_nonreal_decl_scope() == nullptr ||
      nonreal->get_nonreal_decl_scope()->get_parent() != nonreal) {
    return false;
  }

  SgSymbol *symbol =
      nonreal->get_scope()->find_symbol_from_declaration(nonreal);
  return isSgNonrealSymbol(symbol) != nullptr &&
         symbol->get_symbol_basis() == nonreal &&
         symbol->get_parent() == nonreal->get_scope()->get_symbol_table() &&
         nonreal->get_scope()->symbol_exists(symbol);
}

const clang::Type *stripDecltypePreservationWrappers(const clang::Type *type) {
  while (type != nullptr) {
    if (const auto *paren_type = llvm::dyn_cast<clang::ParenType>(type)) {
      type = paren_type->getInnerType().getTypePtrOrNull();
      continue;
    }
    if (const auto *attr_type = llvm::dyn_cast<clang::AttributedType>(type)) {
      type = attr_type->getModifiedType().getTypePtrOrNull();
      continue;
    }
    if (const auto *adjusted_type = llvm::dyn_cast<clang::AdjustedType>(type)) {
      type = adjusted_type->getOriginalType().getTypePtrOrNull();
      continue;
    }
    break;
  }

  return type;
}

bool decltypeNeedsExpressionPreservationForUnnamedType(
    clang::QualType underlying_type) {
  const clang::Type *type =
      stripDecltypePreservationWrappers(underlying_type.getTypePtrOrNull());
  const clang::TagType *tag_type = llvm::dyn_cast_or_null<clang::TagType>(type);
  if (tag_type == nullptr) {
    return false;
  }

  const clang::TagDecl *tag_decl = tag_type->getDecl();
  if (tag_decl == nullptr) {
    return false;
  }

  return tag_decl->getDeclName().isEmpty() && !tag_decl->hasNameForLinkage();
}

uintptr_t qualifiedTypeCacheKey(const clang::QualType &qual_type) {
  return reinterpret_cast<uintptr_t>(qual_type.getAsOpaquePtr());
}

SgTemplateClassDeclaration *
getTemplateDeclarationForSgDecl(SgDeclarationStatement *decl) {
  if (decl == nullptr) {
    return nullptr;
  }

  if (SgTemplateClassDeclaration *template_decl =
          isSgTemplateClassDeclaration(decl)) {
    return template_decl;
  }

  if (SgTemplateInstantiationDecl *instantiation =
          isSgTemplateInstantiationDecl(decl)) {
    return isSgTemplateClassDeclaration(
        instantiation->get_templateDeclaration());
  }

  return nullptr;
}

const clang::NamedDecl *resolveTemplateParameterFromDeclContext(
    const clang::DeclContext *start_context, unsigned depth, unsigned index,
    const ClangTemplateParameterMap *exact_parameters = nullptr) {

  if (start_context == nullptr) {
    if (exact_parameters == nullptr)
      return nullptr;
    const auto found = exact_parameters->find({depth, index});
    return found != exact_parameters->end() ? found->second : nullptr;
  }

  std::vector<const clang::TemplateParameterList *> template_levels =
      collectTemplateParameterLevelsFromDeclContext(start_context);

  if (depth >= template_levels.size()) {
    // A friend function declared in a class template has namespace semantic
    // context but retains the class definition as its exact lexical
    // DeclContext.  Clang represents that friend as a non-template
    // FunctionDecl even though dependent types in its signature and body
    // refer to the enclosing class-template parameters.  Follow the typed
    // lexical owner before considering instantiation patterns; the semantic
    // namespace cannot own those parameter declarations.
    const clang::Decl *start_decl =
        llvm::dyn_cast_or_null<clang::Decl>(start_context);
    const clang::FunctionDecl *start_function =
        llvm::dyn_cast_or_null<clang::FunctionDecl>(start_decl);
    const clang::DeclContext *lexical_context =
        start_function != nullptr ? start_function->getLexicalDeclContext()
                                  : nullptr;
    const clang::DeclContext *semantic_context =
        start_function != nullptr ? start_function->getDeclContext() : nullptr;
    if (lexical_context != nullptr && lexical_context != start_context &&
        lexical_context != semantic_context) {
      if (const clang::NamedDecl *lexical_parameter =
              resolveTemplateParameterFromDeclContext(lexical_context, depth,
                                                      index)) {
        return lexical_parameter;
      }
    }

    // Instantiating a member function template of a class specialization
    // rebases the copied TemplateTypeParmDecl nodes to depth zero, while
    // constraint expressions retained from the source member template still
    // refer to its original depth.  Clang keeps the exact source producer on
    // the instantiated member-function backlink.  Consult that declaration
    // before interpreting the rebased list; a numeric depth/index alone is not
    // an identity.
    const clang::TemplateTypeParmDecl *member_pattern_parameter = nullptr;
    for (const clang::DeclContext *ctx = start_context; ctx != nullptr;
         ctx = ctx->getParent()) {
      const auto *function = llvm::dyn_cast_or_null<clang::FunctionDecl>(
          llvm::dyn_cast_or_null<clang::Decl>(ctx));
      if (function == nullptr) {
        continue;
      }

      llvm::SmallPtrSet<const clang::FunctionDecl *, 2> patterns;
      if (const clang::FunctionDecl *pattern =
              function->getTemplateInstantiationPattern()) {
        patterns.insert(pattern);
      }
      if (const auto *method = llvm::dyn_cast<clang::CXXMethodDecl>(function)) {
        if (const clang::FunctionDecl *pattern =
                method->getInstantiatedFromMemberFunction()) {
          patterns.insert(pattern);
        }
      }

      llvm::SmallPtrSet<const clang::TemplateParameterList *, 3>
          pattern_parameter_lists;
      for (const clang::FunctionDecl *pattern : patterns) {
        const clang::TemplateParameterList *parameters =
            pattern != nullptr ? pattern->getDescribedTemplateParams()
                               : nullptr;
        if (parameters != nullptr) {
          pattern_parameter_lists.insert(parameters);
        }
      }
      if (const clang::FunctionTemplateDecl *function_template =
              function->getDescribedFunctionTemplate()) {
        const clang::FunctionTemplateDecl *source_template =
            function_template->getInstantiatedFromMemberTemplate();
        if (source_template != nullptr) {
          const clang::TemplateParameterList *parameters =
              source_template->getTemplateParameters();
          if (parameters != nullptr) {
            pattern_parameter_lists.insert(parameters);
          }
        }
      }

      for (const clang::TemplateParameterList *parameters :
           pattern_parameter_lists) {

        if (index >= parameters->size()) {
          continue;
        }
        const auto *candidate =
            llvm::dyn_cast_or_null<clang::TemplateTypeParmDecl>(
                parameters->getParam(index));
        if (candidate == nullptr || candidate->getDepth() != depth ||
            candidate->getIndex() != index) {
          continue;
        }
        if (member_pattern_parameter != nullptr &&
            member_pattern_parameter->getCanonicalDecl() !=
                candidate->getCanonicalDecl()) {
          fprintf(stderr,
                  "REX_FRONTEND_INVARIANT[template-parameter-context]: "
                  "member function context=%p resolves depth=%u index=%u to "
                  "multiple exact source parameter families\n",
                  static_cast<const void *>(start_context), depth, index);
          ROSE_ABORT();
        }
        member_pattern_parameter = candidate;
      }
    }
    if (member_pattern_parameter != nullptr) {
      return member_pattern_parameter;
    }

    // An implicitly instantiated class specialization does not itself own a
    // TemplateParameterList, but dependent member types and constraints can
    // retain the depth/index coordinates of the primary or selected partial
    // specialization.  Recover that exact pattern declaration identity from
    // Clang instead of inventing a name for the canonicalized type.
    std::vector<const clang::TemplateParameterList *> pattern_levels;
    for (const clang::DeclContext *ctx = start_context; ctx != nullptr;
         ctx = ctx->getParent()) {
      const auto *specialization =
          llvm::dyn_cast_or_null<clang::ClassTemplateSpecializationDecl>(
              llvm::dyn_cast_or_null<clang::Decl>(ctx));
      if (specialization == nullptr ||
          llvm::isa<clang::ClassTemplatePartialSpecializationDecl>(
              specialization)) {
        continue;
      }

      const auto instantiated_from = specialization->getInstantiatedFrom();
      const clang::TemplateParameterList *pattern_parameters = nullptr;
      if (const auto *partial =
              instantiated_from.dyn_cast<
                  clang::ClassTemplatePartialSpecializationDecl *>()) {
        pattern_parameters = partial->getTemplateParameters();
      } else if (const auto *primary =
                     instantiated_from.dyn_cast<clang::ClassTemplateDecl *>()) {
        pattern_parameters = primary->getTemplateParameters();
      }

      if (pattern_parameters != nullptr) {
        pattern_levels.push_back(pattern_parameters);
      }
    }
    std::reverse(pattern_levels.begin(), pattern_levels.end());
    if (depth >= pattern_levels.size()) {
      return nullptr;
    }
    template_levels = std::move(pattern_levels);
  }
  const clang::TemplateParameterList *params = template_levels[depth];

  if (params == nullptr || index >= params->size()) {
    return nullptr;
  }

  return llvm::dyn_cast_or_null<clang::NamedDecl>(params->getParam(index));
}

std::vector<const clang::NamespaceDecl *>
collectNamespaceContexts(clang::DeclContext *context) {
  std::vector<const clang::NamespaceDecl *> namespaces;
  for (clang::DeclContext *ctx = context; ctx != nullptr;
       ctx = ctx->getParent()) {
    const clang::Decl *ctx_decl = clang::Decl::castFromDeclContext(ctx);
    if (const clang::NamespaceDecl *ns =
            llvm::dyn_cast<clang::NamespaceDecl>(ctx_decl)) {
      if (!ns->isAnonymousNamespace()) {

        namespaces.push_back(ns);
      }
    }
  }
  std::reverse(namespaces.begin(), namespaces.end());
  return namespaces;
}

bool shouldPreserveDependentDecltypeExpression(
    const clang::DecltypeType *decltype_type) {
  if (decltype_type == nullptr) {
    return false;
  }

  const clang::Expr *underlying_expr = decltype_type->getUnderlyingExpr();
  if (underlying_expr == nullptr) {
    return false;
  }

  const clang::Expr *stripped_expr = underlying_expr->IgnoreParenImpCasts();
  const clang::DeclRefExpr *decl_ref =
      llvm::dyn_cast<clang::DeclRefExpr>(stripped_expr);
  if (decl_ref == nullptr) {
    return false;
  }

  const clang::ParmVarDecl *param_decl =
      llvm::dyn_cast<clang::ParmVarDecl>(decl_ref->getDecl());
  if (param_decl == nullptr) {
    return false;
  }

  if (param_decl->isParameterPack()) {
    return true;
  }

  clang::QualType param_type = param_decl->getType();
  if (param_type.isNull()) {
    return false;
  }
  if (param_type->getContainedAutoType() != nullptr) {
    return true;
  }

  return param_type->isDependentType();
}

clang::NestedNameSpecifier
buildNamespaceQualifierForDeclContext(clang::DeclContext *context,
                                      clang::ASTContext &ast_context) {
  if (context == nullptr) {
    return std::nullopt;
  }
  std::vector<const clang::NamespaceDecl *> namespaces =
      collectNamespaceContexts(context);
  if (namespaces.empty()) {
    return std::nullopt;
  }

  clang::NestedNameSpecifier qualifier = std::nullopt;
  for (const clang::NamespaceDecl *ns : namespaces) {
    if (ns == nullptr || ns->isAnonymousNamespace()) {
      continue;
    }

    qualifier = clang::NestedNameSpecifier(ast_context, ns, qualifier);
  }
  return qualifier;
}

bool canSynthesizeNamespaceQualifierFromDeclContext(
    const clang::DeclContext *context) {
  context = const_cast<clang::DeclContext *>(context);
  return context != nullptr &&
         (context->isNamespace() || context->isTranslationUnit());
}

clang::QualType getInjectedClassNameSpecializationType(
    const clang::InjectedClassNameType *injected_class_name_type,
    const clang::ASTContext &ast_context) {
  ASSERT_not_null(injected_class_name_type);

  const clang::CXXRecordDecl *record = injected_class_name_type->getDecl();
  if (record == nullptr) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[injected-class-name-type]: injected "
            "class has no exact CXX record declaration\n");
    ROSE_ABORT();
  }

  clang::QualType specialization_type;
  if (const auto *partial =
          llvm::dyn_cast<clang::ClassTemplatePartialSpecializationDecl>(
              record)) {
    specialization_type =
        partial->getCanonicalInjectedSpecializationType(ast_context);
  } else {
    const clang::ClassTemplateDecl *template_decl =
        injected_class_name_type->getTemplateDecl();
    if (template_decl == nullptr) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[injected-class-name-type]: record=%p "
              "has no exact owning class template\n",
              static_cast<const void *>(record));
      ROSE_ABORT();
    }
    specialization_type =
        template_decl->getCanonicalInjectedSpecializationType(ast_context);
  }

  const clang::Type *specialization = specialization_type.getTypePtrOrNull();

  if (specialization == nullptr || specialization == injected_class_name_type ||
      !llvm::isa<clang::TemplateSpecializationType>(specialization)) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[injected-class-name-type]: record=%p "
            "has no distinct exact template specialization type\n",
            static_cast<const void *>(record));
    ROSE_ABORT();
  }
  return specialization_type;
}

std::string getTemplateNameBase(const clang::TemplateName &tname,
                                clang::ASTContext &context) {
  auto nameString = [](clang::DeclarationName name) {
    ROSE_ASSERT(!name.isEmpty());
    std::string result = name.getAsString();
    ROSE_ASSERT(!result.empty());
    return result;
  };
  if (clang::TemplateDecl *template_decl = tname.getAsTemplateDecl()) {
    return nameString(template_decl->getDeclName());
  }

  if (const clang::QualifiedTemplateName *qtn =
          tname.getAsQualifiedTemplateName()) {
    return getTemplateNameBase(qtn->getUnderlyingTemplate(), context);
  }

  if (const clang::DependentTemplateName *dtn =
          tname.getAsDependentTemplateName()) {
    clang::IdentifierOrOverloadedOperator name = dtn->getName();
    if (const clang::IdentifierInfo *id = name.getIdentifier()) {
      return nameString(clang::DeclarationName(id));
    }
    const auto op = name.getOperator();
    ROSE_ASSERT(op != clang::OO_None && op < clang::NUM_OVERLOADED_OPERATORS);
    return nameString(context.DeclarationNames.getCXXOperatorName(op));
  }

  if (const clang::SubstTemplateTemplateParmStorage *subst =
          tname.getAsSubstTemplateTemplateParm()) {
    return getTemplateNameBase(subst->getReplacement(), context);
  }

  if (clang::UsingShadowDecl *using_shadow = tname.getAsUsingShadowDecl()) {
    return nameString(using_shadow->getDeclName());
  }

  if (clang::AssumedTemplateStorage *assumed =
          tname.getAsAssumedTemplateName()) {
    return nameString(assumed->getDeclName());
  }

  if (clang::OverloadedTemplateStorage *overloaded =
          tname.getAsOverloadedTemplate()) {
    auto declarations = overloaded->decls();
    ROSE_ASSERT(!declarations.empty() && declarations.front() != nullptr);
    clang::DeclarationName name = declarations.front()->getDeclName();
    for (clang::NamedDecl *declaration : declarations) {
      ROSE_ASSERT(declaration != nullptr && declaration->getDeclName() == name);
    }
    return nameString(name);
  }

  if (const clang::SubstTemplateTemplateParmPackStorage *pack =
          tname.getAsSubstTemplateTemplateParmPack()) {
    clang::TemplateTemplateParmDecl *parm = pack->getParameterPack();
    ROSE_ASSERT(parm != nullptr);
    return nameString(parm->getDeclName());
  }

  if (const clang::DeducedTemplateStorage *deduced =
          tname.getAsDeducedTemplateName()) {
    return getTemplateNameBase(deduced->getUnderlying(), context);
  }

  std::cerr << "REX_FRONTEND_INVARIANT[template-name]: unsupported Clang "
               "TemplateName kind"
            << std::endl;
  ROSE_ABORT();
}

SgNonrealDecl::source_elaboration_kind_enum
sourceElaborationKind(clang::ElaboratedTypeKeyword keyword) {
  switch (keyword) {
  case clang::ElaboratedTypeKeyword::None:
    return SgNonrealDecl::e_source_elaboration_none;
  case clang::ElaboratedTypeKeyword::Typename:
    return SgNonrealDecl::e_source_elaboration_typename;
  case clang::ElaboratedTypeKeyword::Class:
    return SgNonrealDecl::e_source_elaboration_class;
  case clang::ElaboratedTypeKeyword::Struct:
    return SgNonrealDecl::e_source_elaboration_struct;
  case clang::ElaboratedTypeKeyword::Union:
    return SgNonrealDecl::e_source_elaboration_union;
  case clang::ElaboratedTypeKeyword::Enum:
    return SgNonrealDecl::e_source_elaboration_enum;
  case clang::ElaboratedTypeKeyword::Interface:
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[nonreal-source-elaboration]: Microsoft "
            "interface elaboration is unsupported\n");
    ROSE_ABORT();
  }
  ROSE_ABORT();
}

SgNonrealDecl::source_elaboration_kind_enum
sourceElaborationKind(clang::TypeLoc type_loc,
                      const clang::CompilerInstance *compiler_instance) {

  return sourceElaborationKind(exactWrittenElaboratedKeyword(
      type_loc, type_loc.getSourceRange(), compiler_instance));
}

void publishSourceElaborationKind(
    SgType *result, clang::TypeLoc type_loc,
    const clang::CompilerInstance *compiler_instance) {
  if (result == nullptr) {
    return;
  }
  SgType *source_surface = result;
  while (source_surface != nullptr) {
    if (SgModifierType *modifier = isSgModifierType(source_surface)) {
      source_surface = modifier->get_base_type();
    } else if (SgPointerType *pointer = isSgPointerType(source_surface)) {
      source_surface = pointer->get_base_type();
    } else if (SgPointerMemberType *pointer_member =
                   isSgPointerMemberType(source_surface)) {
      source_surface = pointer_member->get_base_type();
    } else if (SgReferenceType *reference = isSgReferenceType(source_surface)) {
      source_surface = reference->get_base_type();
    } else if (SgRvalueReferenceType *reference =
                   isSgRvalueReferenceType(source_surface)) {
      source_surface = reference->get_base_type();
    } else if (SgArrayType *array = isSgArrayType(source_surface)) {
      source_surface = array->get_base_type();
    } else {
      break;
    }
  }
  SgNonrealType *source_type = isSgNonrealType(source_surface);
  if (source_type == nullptr) {
    return;
  }
  SgNonrealDecl *source_declaration =
      isSgNonrealDecl(source_type->get_declaration());
  if (source_declaration == nullptr) {
    fprintf(stderr, "REX_FRONTEND_INVARIANT[nonreal-source-elaboration]: exact "
                    "TypeLoc produced a nonreal type without a declaration\n");
    ROSE_ABORT();
  }
  const SgNonrealDecl::source_elaboration_kind_enum exact_kind =
      sourceElaborationKind(type_loc, compiler_instance);
  const SgNonrealDecl::source_elaboration_kind_enum prior_kind =
      source_declaration->get_source_elaboration_kind();
  if (prior_kind != SgNonrealDecl::e_source_elaboration_unspecified &&
      prior_kind != exact_kind) {
    fprintf(stderr, "REX_FRONTEND_INVARIANT[nonreal-source-elaboration]: exact "
                    "TypeLoc disagrees with its nonreal source surface\n");
    ROSE_ABORT();
  }
  source_declaration->set_source_elaboration_kind(exact_kind);
  source_declaration->set_suppress_typename(
      exact_kind != SgNonrealDecl::e_source_elaboration_typename);
}

} // namespace

ClangToSageTranslator::FunctionExceptionSpecification
ClangToSageTranslator::prepareFunctionExceptionSpecification(
    const clang::FunctionProtoType *type, const clang::FunctionDecl *owner,
    bool is_written, bool semantic) {
  ASSERT_not_null(type);
  FunctionExceptionSpecification result;
  result.is_written = is_written;
  const auto spec = type->getExceptionSpecInfo();
  switch (spec.Type) {
  case clang::EST_None:
    result.kind = SgFunctionType::e_exception_none;
    break;
  case clang::EST_DynamicNone:
    result.kind = SgFunctionType::e_exception_dynamic_none;
    break;
  case clang::EST_Dynamic:
    result.kind = SgFunctionType::e_exception_dynamic;
    break;
  case clang::EST_MSAny:
    result.kind = SgFunctionType::e_exception_ms_any;
    break;
  case clang::EST_NoThrow:
    result.kind = SgFunctionType::e_exception_nothrow;
    break;
  case clang::EST_BasicNoexcept:
    result.kind = SgFunctionType::e_exception_noexcept;
    break;
  case clang::EST_DependentNoexcept:
    result.kind = SgFunctionType::e_exception_noexcept_dependent;
    break;
  case clang::EST_NoexceptFalse:
    result.kind = SgFunctionType::e_exception_noexcept_false;
    break;
  case clang::EST_NoexceptTrue:
    result.kind = SgFunctionType::e_exception_noexcept_true;
    break;
  case clang::EST_Unevaluated:
    result.kind = SgFunctionType::e_exception_unevaluated;
    break;
  case clang::EST_Uninstantiated:
    result.kind = SgFunctionType::e_exception_uninstantiated;
    break;
  case clang::EST_Unparsed:
  default:
    failExactTypeTranslation("unparsed-function-exception-specification", type);
  }
  if (spec.NoexceptExpr != nullptr) {
    FunctionExceptionConstructionFrame frame{
        type, semantic, p_function_body_translation_owner_stack.size()};
    struct ExceptionConstructionGuard {
      const FunctionExceptionConstructionFrame *&active;
      const FunctionExceptionConstructionFrame *previous;
      ~ExceptionConstructionGuard() { active = previous; }
    } guard{p_function_exception_construction,
            std::exchange(p_function_exception_construction, &frame)};
    SgExpression *expression =
        semantic ? translateSemanticExpressionSubtree(
                       spec.NoexceptExpr, "function-exception-specification")
                 : isSgExpression(Traverse(spec.NoexceptExpr));
    ASSERT_not_null(expression);
    if (!semantic)
      expression = prepareExpressionForAttachment(expression);
    ROSE_ASSERT(expression->get_parent() == nullptr);
    result.expression.reset(expression);
  }
  for (clang::QualType exception_type : spec.Exceptions) {
    const bool pack =
        exception_type->getAs<clang::PackExpansionType>() != nullptr;
    SgType *translated = semantic ? buildSemanticTypeFromQualifiedType(
                                        exception_type.getCanonicalType(),
                                        "function-exception-type")
                                  : buildTypeFromQualifiedType(exception_type);
    result.types.push_back(requireExactType(
        translated, "function-exception-type", exception_type.getTypePtr()));
    result.type_pack_expansions.push_back(pack);
  }
  if (spec.SourceDecl != nullptr) {
    if (spec.SourceDecl == owner) {
      result.source_is_self = true;
    } else {
      result.source = isSgFunctionDeclaration(Traverse(spec.SourceDecl));
      ASSERT_not_null(result.source);
    }
  }
  if (spec.SourceTemplate != nullptr) {
    ROSE_ASSERT(spec.SourceTemplate != owner);
    result.pattern = isSgFunctionDeclaration(Traverse(spec.SourceTemplate));
    ASSERT_not_null(result.pattern);
  }
  SgFunctionType::validate_exception_specification(
      result.kind, result.expression.get(), result.types,
      result.type_pack_expansions, result.source, result.pattern,
      result.source_is_self, result.is_written);
  return result;
}

SgFunctionType *ClangToSageTranslator::buildFunctionTypeForClangProto(
    SgType *return_type,
    std::unique_ptr<SgFunctionParameterTypeList> parameters,
    const clang::FunctionProtoType *proto_type, SgType *class_type,
    const clang::FunctionProtoTypeLoc *source) {
  auto *param_type_list = parameters.get();
  ASSERT_not_null(return_type);
  ASSERT_not_null(param_type_list);
  ASSERT_not_null(proto_type);
  ROSE_ASSERT(source == nullptr || source->getTypePtr() == proto_type);
  if (param_type_list->get_parent() != nullptr) {
    std::cerr
        << "REX_FRONTEND_INVARIANT[function-argument-qualification-use-site]: "
        << "new function parameter type list " << param_type_list
        << " is already owned by " << param_type_list->get_parent()
        << std::endl;
    ROSE_ABORT();
  }
  param_type_list->validate_argument_qualification_use_sites(
      "new-clang-function-prototype");
  const unsigned int mfunc_specifier =
      roseMemberFunctionSpecifierFromClangProto(proto_type);
  const bool has_ellipses = proto_type->isVariadic();
  ROSE_ASSERT(
      has_ellipses ==
      (!param_type_list->get_arguments().empty() &&
       isSgTypeEllipse(param_type_list->get_arguments().back()) != nullptr));
  const auto *exception_type = source != nullptr
                                   ? proto_type
                                   : proto_type->getCanonicalTypeInternal()
                                         ->getAs<clang::FunctionProtoType>();
  ASSERT_not_null(exception_type);
  SgFunctionType *semantic_signature = nullptr;
  if (source != nullptr) {
    semantic_signature = isSgFunctionType(
        buildTypeFromQualifiedType(proto_type->getCanonicalTypeInternal()));
    ASSERT_not_null(semantic_signature);
    if (class_type != nullptr)
      semantic_signature = SageBuilder::buildMemberFunctionType(
          semantic_signature, class_type, mfunc_specifier);
  }
  auto spec = prepareFunctionExceptionSpecification(
      exception_type, nullptr,
      source != nullptr && source->getExceptionSpecRange().isValid(),
      source == nullptr);
  std::unique_ptr<SgFunctionType> candidate;
  if (class_type == nullptr && mfunc_specifier == 0)
    candidate = std::make_unique<SgFunctionType>(
        return_type, has_ellipses, param_type_list, spec.kind,
        spec.expression.get(), spec.types, spec.type_pack_expansions,
        spec.source, spec.pattern, spec.source_is_self, spec.is_written,
        semantic_signature);
  else
    candidate = std::make_unique<SgMemberFunctionType>(
        return_type, has_ellipses, param_type_list, spec.kind,
        spec.expression.get(), spec.types, spec.type_pack_expansions,
        spec.source, spec.pattern, spec.source_is_self, spec.is_written,
        semantic_signature, true, class_type, mfunc_specifier);
  parameters.release();
  spec.expression.release();
  ROSE_ASSERT(candidate->get_argument_list() == param_type_list);
  ROSE_ASSERT(param_type_list->get_parent() == candidate.get());
  SgFunctionType *function_type = nullptr;
  if (source != nullptr) {
    function_type = candidate.release();
  } else {
    auto *table = SgNode::get_globalFunctionTypeTable();
    ASSERT_not_null(table);
    if (candidate->get_exception_specification_expression() != nullptr)
      function_type = table->lookup_exact_function_type(candidate.get());
    if (function_type != nullptr) {
      // Retire frontend caches with the duplicate's owned exception operands.
      deleteTransientAST({candidate.release()});
    } else {
      function_type = SageBuilder::internFunctionType(std::move(candidate));
    }
  }
  ROSE_ASSERT(function_type->get_has_ellipses() == has_ellipses);
  requireExactFunctionArgumentQualificationUseSites(function_type,
                                                    "clang-function-prototype");
  return function_type;
}

SgFunctionType *ClangToSageTranslator::buildDeclaredFunctionExceptionSignature(
    SgFunctionType *canonical_type, const clang::FunctionProtoType *type,
    const clang::FunctionDecl *owner, bool is_written, bool semantic) {
  ASSERT_not_null(canonical_type);
  ASSERT_not_null(owner);
  auto spec =
      prepareFunctionExceptionSpecification(type, owner, is_written, semantic);
  ROSE_ASSERT(spec.kind != SgFunctionType::e_exception_none);
  auto arguments = std::make_unique<SgFunctionParameterTypeList>();
  copyClangFunctionArgumentTypes(canonical_type, arguments.get(),
                                 "declared-function-exception-signature");
  std::unique_ptr<SgFunctionType> signature;
  if (auto *member = isSgMemberFunctionType(canonical_type))
    signature = std::make_unique<SgMemberFunctionType>(
        canonical_type->get_return_type(), canonical_type->get_has_ellipses(),
        arguments.get(), spec.kind, spec.expression.get(), spec.types,
        spec.type_pack_expansions, spec.source, spec.pattern,
        spec.source_is_self, spec.is_written, canonical_type,
        canonical_type->get_has_prototype(), member->get_class_type(),
        member->get_mfunc_specifier());
  else
    signature = std::make_unique<SgFunctionType>(
        canonical_type->get_return_type(), canonical_type->get_has_ellipses(),
        arguments.get(), spec.kind, spec.expression.get(), spec.types,
        spec.type_pack_expansions, spec.source, spec.pattern,
        spec.source_is_self, spec.is_written, canonical_type,
        canonical_type->get_has_prototype());
  arguments.release();
  spec.expression.release();
  return signature.release();
}

SageBuilder::function_declaration_ownership
ClangToSageTranslator::prepareFunctionDeclarationExceptionOwnership(
    SageBuilder::function_declaration_ownership ownership,
    const clang::FunctionDecl *declaration, SgFunctionType *canonical_type,
    bool is_written, bool semantic) {
  ASSERT_not_null(declaration);
  ownership = ownership.withFunctionType(canonical_type);
  const auto *prototype =
      declaration->getType()->getAs<clang::FunctionProtoType>();
  if (prototype == nullptr) {
    ROSE_ASSERT(declaration->getType()->getAs<clang::FunctionNoProtoType>() !=
                nullptr);
    ROSE_ASSERT(!is_written);
    return ownership;
  }
  if (prototype->getExceptionSpecType() == clang::EST_None) {
    ROSE_ASSERT(!is_written);
    return ownership;
  }
  return ownership.withDeclaredExceptionSignature(
      buildDeclaredFunctionExceptionSignature(
          canonical_type, prototype, declaration, is_written, semantic));
}

void copyClangFunctionArgumentTypes(const SgFunctionType *source,
                                    SgFunctionParameterTypeList *destination,
                                    const char *context) {
  requireExactFunctionArgumentQualificationUseSites(source, context);
  ASSERT_not_null(destination);
  destination->validate_argument_qualification_use_sites(context);
  ROSE_ASSERT(destination->get_arguments().empty());
  const SgTypePtrList &argument_types = source->get_arguments();
  const SgFunctionTypeArgumentPtrList &source_positions =
      source->get_argument_list()->get_argument_qualification_use_sites();
  for (size_t index = 0; index < argument_types.size(); ++index) {
    destination->append_argument(argument_types[index],
                                 *source_positions[index]);
  }
}

SgNonrealDecl::source_elaboration_kind_enum
ClangToSageTranslator::exactTypeLocSourceElaborationKind(
    const clang::TypeLoc &type_loc) const {
  if (type_loc.isNull()) {
    fprintf(stderr, "REX_FRONTEND_INVARIANT[nonreal-source-elaboration]: exact "
                    "TypeLoc is required for a source type use\n");
    ROSE_ABORT();
  }
  return sourceElaborationKind(type_loc, p_compiler_instance);
}

bool ClangToSageTranslator::exactTypeLocSourceQualification(
    const clang::TypeLoc &type_loc, SgStringList &tokens,
    bool &global_qualification) {
  if (type_loc.isNull()) {
    fprintf(stderr, "REX_FRONTEND_INVARIANT[type-source-qualification]: exact "
                    "TypeLoc is required\n");
    ROSE_ABORT();
  }
  tokens.clear();
  global_qualification = false;
  clang::NestedNameSpecifierLoc qualifier_loc = typeLocQualifierLoc(type_loc);
  if (!qualifier_loc) {
    return false;
  }
  const SourceQualification source_qualification =
      sourceQualificationFromNestedNameSpecifierLoc(
          qualifier_loc, p_compiler_instance, "type-loc-source-qualification");
  tokens = source_qualification.tokens;
  global_qualification = source_qualification.global;
  return true;
}

SgDeclarationStatement *
ClangToSageTranslator::resolveNonrealTemplateDeclarationTarget(
    clang::Decl *template_decl, const char *context) {
  if (template_decl == nullptr) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[nonreal-template-link]: context=%s "
            "Clang template declaration cannot be null\n",
            context != nullptr ? context : "<unknown>");
    ROSE_ABORT();
  }

  if (auto *parameter =
          llvm::dyn_cast<clang::TemplateTemplateParmDecl>(template_decl)) {
    SgTemplateParameter *surface =
        lookupActiveTemplateParameterSurface(parameter, context);
    if (surface == nullptr) {
      auto translated = p_decl_translation_map.find(parameter);
      surface = translated != p_decl_translation_map.end()
                    ? isSgTemplateParameter(translated->second)
                    : nullptr;
    }
    auto *identity =
        surface != nullptr
            ? isSgTemplateDeclaration(surface->get_templateDeclaration())
            : nullptr;
    auto *type =
        surface != nullptr ? isSgTemplateType(surface->get_type()) : nullptr;
    if (surface == nullptr || identity == nullptr || type == nullptr ||
        type->get_template_parameter() != surface ||
        type->get_template_parameter_depth() !=
            static_cast<int>(parameter->getDepth()) ||
        type->get_template_parameter_position() !=
            static_cast<int>(parameter->getIndex())) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[nonreal-template-link]: context=%s "
              "template=%s parameter depth=%u index=%u has no exact "
              "producer-published Sage template identity "
              "(surface=%p identity=%p type=%p type-depth=%d type-index=%d)\n",
              context != nullptr ? context : "<unknown>",
              parameter->getNameAsString().c_str(), parameter->getDepth(),
              parameter->getIndex(), static_cast<void *>(surface),
              static_cast<void *>(identity), static_cast<void *>(type),
              type != nullptr ? type->get_template_parameter_depth() : -1,
              type != nullptr ? type->get_template_parameter_position() : -1);
      ROSE_ABORT();
    }
    return identity;
  }

  clang::ClassTemplateDecl *class_template =
      llvm::dyn_cast<clang::ClassTemplateDecl>(template_decl);
  clang::ClassTemplateDecl *canonical_class_template =
      class_template != nullptr ? class_template->getCanonicalDecl() : nullptr;
  SgDeclarationStatement *resolved = normalizeNonrealTemplateDeclarationTarget(
      lookupSgDeclarationForClangDecl(template_decl,
                                      /*allow_on_demand=*/false));
  const bool family_translation_is_active =
      class_template != nullptr &&
      (p_decl_translation_in_progress.count(class_template) != 0 ||
       p_decl_translation_in_progress.count(canonical_class_template) != 0);
  if (resolved == nullptr && family_translation_is_active) {
    // A defining class-template redeclaration publishes its exact Sage
    // declaration before populating members.  An injected-class type in
    // those members can still name the canonical Clang redeclaration, whose
    // distinct source declaration must remain available for later lexical
    // traversal. Resolve the already-published sibling to the one canonical
    // Sage template root without caching over that unvisited source key.
    SgTemplateClassDeclaration *resolved_family = nullptr;
    auto consume_published_family_member = [&](clang::Decl *key) {
      auto found = key != nullptr ? p_decl_translation_map.find(key)
                                  : p_decl_translation_map.end();
      if (found == p_decl_translation_map.end()) {
        return;
      }

      SgDeclarationStatement *mapped_decl =
          isSgDeclarationStatement(found->second);
      SgTemplateClassDeclaration *mapped_template =
          isSgTemplateClassDeclaration(
              normalizeNonrealTemplateDeclarationTarget(mapped_decl));
      if (mapped_template == nullptr) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[nonreal-template-link]: context=%s "
                "active class-template family key=%p maps to %p/%s instead "
                "of an SgTemplateClassDeclaration\n",
                context != nullptr ? context : "<unknown>",
                static_cast<void *>(key), static_cast<void *>(found->second),
                found->second != nullptr ? found->second->class_name().c_str()
                                         : "<null>");
        ROSE_ABORT();
      }
      if (resolved_family != nullptr && resolved_family != mapped_template) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[nonreal-template-link]: context=%s "
                "active Clang class-template family has conflicting Sage "
                "roots %p/%s and %p/%s\n",
                context != nullptr ? context : "<unknown>",
                static_cast<void *>(resolved_family),
                SageInterface::get_name(resolved_family).c_str(),
                static_cast<void *>(mapped_template),
                SageInterface::get_name(mapped_template).c_str());
        ROSE_ABORT();
      }
      resolved_family = mapped_template;
    };

    for (clang::RedeclarableTemplateDecl *template_redecl :
         class_template->redecls()) {
      clang::ClassTemplateDecl *redecl =
          llvm::cast<clang::ClassTemplateDecl>(template_redecl);
      consume_published_family_member(redecl);
      clang::CXXRecordDecl *record = redecl->getTemplatedDecl();
      consume_published_family_member(record);
      if (record != nullptr) {
        consume_published_family_member(record->getDefinition());
        consume_published_family_member(record->getCanonicalDecl());
      }
    }
    resolved = resolved_family;
  }
  if (resolved == nullptr && !family_translation_is_active) {
    // Only a template family that is not already under construction may be
    // translated on demand. Re-entering an active source template from one of
    // its signature TypeLocs publishes its members under the temporary
    // function-declarator scope and corrupts both ownership and provenance.
    auto *specialization =
        llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(template_decl);
    const bool defer_population =
        specialization != nullptr &&
        !llvm::isa<clang::ClassTemplatePartialSpecializationDecl>(
            specialization);
    llvm::SaveAndRestore<unsigned> population_guard(
        p_defer_on_demand_cxx_record_population_depth,
        p_defer_on_demand_cxx_record_population_depth +
            (defer_population ? 1 : 0));
    resolved = normalizeNonrealTemplateDeclarationTarget(
        lookupSgDeclarationForClangDecl(template_decl,
                                        /*allow_on_demand=*/true));
    if (defer_population)
      queuePendingImplicitClassTemplateSpecialization(specialization);
  }
  if (resolved == nullptr) {
    clang::NamedDecl *named_template =
        llvm::dyn_cast<clang::NamedDecl>(template_decl);
    clang::Decl *canonical_template = nullptr;
    if (clang::ClassTemplateDecl *class_template =
            llvm::dyn_cast<clang::ClassTemplateDecl>(template_decl)) {
      canonical_template = class_template->getCanonicalDecl();
    } else if (clang::FunctionTemplateDecl *function_template =
                   llvm::dyn_cast<clang::FunctionTemplateDecl>(template_decl)) {
      canonical_template = function_template->getCanonicalDecl();
    } else if (clang::VarTemplateDecl *variable_template =
                   llvm::dyn_cast<clang::VarTemplateDecl>(template_decl)) {
      canonical_template = variable_template->getCanonicalDecl();
    } else if (clang::TypeAliasTemplateDecl *alias_template =
                   llvm::dyn_cast<clang::TypeAliasTemplateDecl>(
                       template_decl)) {
      canonical_template = alias_template->getCanonicalDecl();
    }
    auto mapped_node = [&](clang::Decl *key) -> SgNode * {
      auto found = key != nullptr ? p_decl_translation_map.find(key)
                                  : p_decl_translation_map.end();
      return found != p_decl_translation_map.end() ? found->second : nullptr;
    };
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[nonreal-template-link]: context=%s "
            "has no exact translated target for Clang %s "
            "name=%s template=%p canonical=%p in-progress=%d/%d "
            "on-demand=%d/%d mapped=%p/%p\n",
            context != nullptr ? context : "<unknown>",
            template_decl->getDeclKindName(),
            named_template != nullptr
                ? named_template->getQualifiedNameAsString().c_str()
                : "<unnamed>",
            static_cast<void *>(template_decl),
            static_cast<void *>(canonical_template),
            p_decl_translation_in_progress.count(template_decl) != 0 ? 1 : 0,
            p_decl_translation_in_progress.count(canonical_template) != 0 ? 1
                                                                          : 0,
            p_decl_translation_on_demand.count(template_decl) != 0 ? 1 : 0,
            p_decl_translation_on_demand.count(canonical_template) != 0 ? 1 : 0,
            static_cast<void *>(mapped_node(template_decl)),
            static_cast<void *>(mapped_node(canonical_template)));
    ROSE_ABORT();
  }
  return resolved;
}

namespace {

ClangTemplateInstantiationCacheKey makeTemplateInstantiationCacheKey(
    const clang::TemplateDecl *primary_template,
    llvm::ArrayRef<clang::TemplateArgument> arguments,
    const clang::DeclContext *semantic_owner, const clang::ASTContext &context,
    const SgType *aliased_type = nullptr) {
  if (primary_template == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[template-instantiation-identity]: "
                 "cache key has no exact primary template"
              << std::endl;
    ROSE_ABORT();
  }

  if (semantic_owner == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[template-instantiation-identity]: "
                 "cache key has no exact semantic owner"
              << std::endl;
    ROSE_ABORT();
  }
  clang::DeclContext *canonical_owner =
      const_cast<clang::DeclContext *>(semantic_owner)->getPrimaryContext();

  if (canonical_owner == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[template-instantiation-identity]: "
                 "cache key semantic owner has no canonical context"
              << std::endl;
    ROSE_ABORT();
  }

  ClangTemplateInstantiationCacheKey key;
  key.primary_template =
      llvm::cast<clang::TemplateDecl>(primary_template->getCanonicalDecl());
  key.semantic_owner = canonical_owner;
  key.aliased_type = aliased_type;
  key.arguments.reserve(arguments.size());

  llvm::FoldingSetNodeID profile;
  profile.AddPointer(key.primary_template);
  profile.AddPointer(canonical_owner);
  profile.AddPointer(aliased_type);
  profile.AddInteger(arguments.size());
  for (const clang::TemplateArgument &argument : arguments) {
    clang::TemplateArgument canonical =
        context.getCanonicalTemplateArgument(argument);
    key.arguments.push_back(canonical);
    key.arguments.back().Profile(profile, context);
  }
  key.profile_hash = profile.ComputeHash();
  return key;
}

} // namespace

clang::NonTypeTemplateParmDecl *
ClangToSageTranslator::resolveCanonicalNonTypeTemplateParameterUse(
    clang::NonTypeTemplateParmDecl *parameter) {
  ROSE_ASSERT(parameter != nullptr);
  ROSE_ASSERT(p_canonical_template_argument_context.has_value());
  const clang::DeclContext *context = *p_canonical_template_argument_context;
  auto *bound = const_cast<clang::NonTypeTemplateParmDecl *>(
      llvm::dyn_cast_or_null<clang::NonTypeTemplateParmDecl>(
          resolveTemplateParameterFromDeclContext(
              context, parameter->getDepth(), parameter->getIndex(),
              currentExactTemplateParameters())));
  ROSE_ASSERT(bound != nullptr);
  ROSE_ASSERT(bound->isParameterPack() == parameter->isParameterPack());
  ROSE_ASSERT(p_compiler_instance->getASTContext().hasSameType(
      bound->getType(), parameter->getType()));
  return bound;
}

ClangTemplateInstantiationCacheKey
ClangToSageTranslator::buildTemplateInstantiationCacheKey(
    const clang::TemplateDecl *primary_template,
    const clang::TemplateSpecializationType *spec_type,
    const clang::DeclContext *semantic_owner, const SgType *aliased_type) {
  if (spec_type == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[template-instantiation-identity]: "
                 "type cache key requires an exact specialization type"
              << std::endl;
    ROSE_ABORT();
  }
  llvm::ArrayRef<clang::TemplateArgument> arguments =
      spec_type->template_arguments();
  return makeTemplateInstantiationCacheKey(
      primary_template, arguments, semantic_owner,
      p_compiler_instance->getASTContext(), aliased_type);
}

ClangTemplateInstantiationCacheKey
ClangToSageTranslator::buildTemplateInstantiationCacheKey(
    const clang::ClassTemplateDecl *primary_template,
    const clang::TemplateArgumentList &args,
    const clang::DeclContext *semantic_owner) {
  return makeTemplateInstantiationCacheKey(
      primary_template, args.asArray(), semantic_owner,
      p_compiler_instance->getASTContext());
}

namespace {

std::size_t countLexicalDeclarationEdges(SgScopeStatement *scope,
                                         SgDeclarationStatement *declaration) {
  if (scope == nullptr || declaration == nullptr) {
    return 0;
  }
  if (scope->containsOnlyDeclarations()) {
    const SgDeclarationStatementPtrList &declarations =
        scope->getDeclarationList();
    return static_cast<std::size_t>(
        std::count(declarations.begin(), declarations.end(), declaration));
  }
  const SgStatementPtrList &statements = scope->getStatementList();
  return static_cast<std::size_t>(
      std::count(statements.begin(), statements.end(), declaration));
}

void requireExactSynthesizedProvenance(SgLocatedNode *node,
                                       const char *context) {
  if (node == nullptr || context == nullptr) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[synthesized-provenance]: context=%s "
            "node=%p lacks an exact producer\n",
            context != nullptr ? context : "<null>", static_cast<void *>(node));
    ROSE_ABORT();
  }
  Sg_File_Info *start = node->get_startOfConstruct();
  Sg_File_Info *end = node->get_endOfConstruct();
  SgExpression *expression = isSgExpression(node);
  Sg_File_Info *operator_position =
      expression != nullptr ? expression->get_operatorPosition() : nullptr;
  auto is_exact = [node](Sg_File_Info *file_info) {
    return file_info != nullptr && file_info->get_parent() == node &&
           file_info->isCompilerGenerated() && !file_info->isTransformation() &&
           file_info->isOutputInCodeGeneration() &&
           file_info->get_file_id() == Sg_File_Info::COMPILER_GENERATED_FILE_ID;
  };
  if (!is_exact(start) || !is_exact(end) ||
      (expression != nullptr && !is_exact(operator_position))) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[synthesized-provenance]: context=%s "
            "node=%p type=%s has incomplete, shared, or non-synthesized "
            "file info\n",
            context, static_cast<void *>(node), node->class_name().c_str());
    ROSE_ABORT();
  }
}

void requireExactSourceProvenance(SgLocatedNode *node, const char *context) {
  if (node == nullptr || context == nullptr) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[source-provenance]: context=%s node=%p "
            "lacks an exact producer\n",
            context != nullptr ? context : "<null>", static_cast<void *>(node));
    ROSE_ABORT();
  }
  Sg_File_Info *start = node->get_startOfConstruct();
  Sg_File_Info *end = node->get_endOfConstruct();
  auto is_exact = [node](Sg_File_Info *file_info) {
    return file_info != nullptr && file_info->get_parent() == node &&
           file_info->get_line() > 0 && !file_info->isCompilerGenerated() &&
           !file_info->isTransformation() &&
           !file_info->isSourcePositionUnavailableInFrontend();
  };
  if (!is_exact(start) || !is_exact(end)) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[source-provenance]: context=%s node=%p "
            "type=%s has incomplete, shared, or non-source file info\n",
            context, static_cast<void *>(node), node->class_name().c_str());
    ROSE_ABORT();
  }
}

void requireExactAuxiliaryDeclarationOwner(SgDeclarationStatement *declaration,
                                           SgScopeStatement *scope,
                                           const char *context) {
  SgAuxiliaryDeclarationList *auxiliary =
      declaration != nullptr
          ? isSgAuxiliaryDeclarationList(declaration->get_parent())
          : nullptr;
  if (declaration == nullptr || scope == nullptr || auxiliary == nullptr ||
      auxiliary->get_parent() != scope ||
      scope->get_auxiliary_declarations() != auxiliary ||
      declaration->get_scope() != scope ||
      countLexicalDeclarationEdges(scope, declaration) != 0 ||
      std::count(auxiliary->get_declarations().begin(),
                 auxiliary->get_declarations().end(), declaration) != 1) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[semantic-declaration-auxiliary-owner]: "
            "context=%s declaration=%p type=%s has no exact auxiliary "
            "owner\n",
            context, static_cast<void *>(declaration),
            declaration != nullptr ? declaration->class_name().c_str()
                                   : "<null>");
    ROSE_ABORT();
  }
}

void requireTypedNonLexicalDeclarationOwner(SgDeclarationStatement *declaration,
                                            const char *context) {
  if (declaration == nullptr) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[semantic-declaration-typed-owner]: "
            "context=%s has a null declaration\n",
            context);
    ROSE_ABORT();
  }
  SgNode *parent = declaration->get_parent();
  if (SgAuxiliaryDeclarationList *auxiliary =
          isSgAuxiliaryDeclarationList(parent)) {
    requireExactAuxiliaryDeclarationOwner(
        declaration, isSgScopeStatement(auxiliary->get_parent()), context);
    return;
  }
  if (SgDeclarationScope *declaration_scope = isSgDeclarationScope(parent)) {
    if (declaration->get_scope() == nullptr ||
        countLexicalDeclarationEdges(declaration_scope, declaration) != 1) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[semantic-declaration-typed-owner]: "
              "context=%s declaration=%p has malformed declaration-scope "
              "ownership\n",
              context, static_cast<void *>(declaration));
      ROSE_ABORT();
    }
    return;
  }
  if (parent == nullptr || isSgScopeStatement(parent) != nullptr) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[semantic-declaration-typed-owner]: "
            "context=%s declaration=%p has no typed non-lexical owner\n",
            context, static_cast<void *>(declaration));
    ROSE_ABORT();
  }
  std::size_t owner_edges = 0;
  for (const auto &successor : parent->returnDataMemberPointers()) {
    if (successor.first == declaration) {
      ++owner_edges;
    }
  }
  SgScopeStatement *semantic_scope = declaration->get_scope();
  if (owner_edges != 1 ||
      (semantic_scope != nullptr &&
       countLexicalDeclarationEdges(semantic_scope, declaration) != 0)) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[semantic-declaration-typed-owner]: "
            "context=%s declaration=%p has %zu typed owner edges or a "
            "lexical edge\n",
            context, static_cast<void *>(declaration), owner_edges);
    ROSE_ABORT();
  }
}

SgClassDeclaration *
translatedRecordTypeDeclaration(const clang::RecordDecl *record_decl,
                                SgDeclarationStatement *translated_decl) {
  ROSE_ASSERT(record_decl != nullptr);
  if (translated_decl == nullptr)
    return nullptr;
  if (auto *directive =
          isSgTemplateInstantiationDirectiveStatement(translated_decl)) {
    translated_decl = directive->get_declaration();
    ROSE_ASSERT(translated_decl != nullptr);
  }
  auto *declaration = isSgClassDeclaration(translated_decl);
  ROSE_ASSERT(declaration != nullptr);
  auto *first =
      isSgClassDeclaration(declaration->get_firstNondefiningDeclaration());
  ROSE_ASSERT(first != nullptr &&
              first->get_firstNondefiningDeclaration() == first);
  const bool specialization =
      llvm::isa<clang::ClassTemplateSpecializationDecl>(record_decl) &&
      !llvm::isa<clang::ClassTemplatePartialSpecializationDecl>(record_decl);
  ROSE_ASSERT(!specialization ||
              (isSgTemplateInstantiationDecl(declaration) != nullptr &&
               isSgTemplateInstantiationDecl(first) != nullptr));
  return first;
}

} // anonymous namespace

SgType *ClangToSageTranslator::getTypeFromTranslatedRecordDecl(
    clang::RecordDecl *record_decl) {

  if (record_decl == nullptr) {
    return nullptr;
  }

  clang::RecordDecl *cache_key =
      llvm::dyn_cast<clang::RecordDecl>(record_decl->getCanonicalDecl());
  ROSE_ASSERT(cache_key != nullptr);

  auto cache_type = [&](SgDeclarationStatement *translated_decl) -> SgType * {
    SgClassDeclaration *sg_decl =
        translatedRecordTypeDeclaration(record_decl, translated_decl);

    SgType *type = sg_decl->get_type();
    if (type != nullptr) {
      p_record_decl_type_map[cache_key] = type;
    }
    return type;
  };

  if (auto cached = p_record_decl_type_map.find(cache_key);
      cached != p_record_decl_type_map.end()) {
    return cached->second;
  }

  if (SgDeclarationStatement *cached_decl = lookupSgDeclarationForClangDecl(
          record_decl, /*allow_on_demand=*/false)) {
    return cache_type(cached_decl);
  }
  return nullptr;
}

bool ClangToSageTranslator::scopeIsWithinNamespaceChain(
    SgScopeStatement *scope, clang::DeclContext *context) const {
  if (scope == nullptr || context == nullptr)
    return false;

  std::vector<const clang::NamespaceDecl *> expected;
  for (const clang::DeclContext *owner = context; owner != nullptr;
       owner = owner->getParent()) {
    if (const auto *ns = llvm::dyn_cast<clang::NamespaceDecl>(
            clang::Decl::castFromDeclContext(owner)))
      expected.push_back(ns);
  }
  if (expected.empty())
    return false;
  std::reverse(expected.begin(), expected.end());

  std::vector<const SgNamespaceDeclarationStatement *> actual;
  for (const SgNode *owner = scope; owner != nullptr;
       owner = owner->get_parent()) {
    if (const auto *definition = isSgNamespaceDefinitionStatement(owner)) {
      const auto *declaration = definition->get_namespaceDeclaration();
      ROSE_ASSERT(declaration != nullptr);
      actual.push_back(declaration);
    }
  }
  if (actual.size() < expected.size())
    return false;
  std::reverse(actual.begin(), actual.end());
  for (size_t index = 0; index < expected.size(); ++index) {
    const auto mapped =
        p_namespace_canonical_decl_map.find(const_cast<clang::NamespaceDecl *>(
            expected[index]->getCanonicalDecl()));
    if (mapped == p_namespace_canonical_decl_map.end())
      return false;
    ROSE_ASSERT(mapped->second != nullptr);
    const auto *canonical = mapped->second->get_firstNondefiningDeclaration();
    ROSE_ASSERT(canonical != nullptr);
    ROSE_ASSERT(actual[index]->get_firstNondefiningDeclaration() != nullptr);
    if (actual[index]->get_firstNondefiningDeclaration() != canonical)
      return false;
  }
  return true;
}

bool ClangToSageTranslator::scopeReachableFromCurrentFile(
    SgScopeStatement *candidate) {
  if (candidate == nullptr) {
    return false;
  }

  if (p_global_scope == nullptr || p_sage_source_file == nullptr ||
      p_global_scope->get_parent() != p_sage_source_file ||
      p_sage_source_file->get_globalScope() != p_global_scope) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[translation-global-owner]: "
            "translator-global=%p parent=%p source-file=%p is not an exact "
            "in-progress AST root\n",
            static_cast<void *>(p_global_scope),
            static_cast<void *>(p_global_scope != nullptr
                                    ? p_global_scope->get_parent()
                                    : nullptr),
            static_cast<void *>(p_sage_source_file));
    ROSE_ABORT();
  }
  SgGlobal *file_global = p_sage_source_file->get_globalScope();

  auto attached_to_parent = [](SgNode *child, SgNode *parent) -> bool {
    if (child == nullptr || parent == nullptr) {
      return false;
    }

    auto is_structural_successor = [](SgNode *node, SgNode *owner) -> bool {
      const std::vector<SgNode *> successors =
          owner->get_traversalSuccessorContainer();
      return std::find(successors.begin(), successors.end(), node) !=
             successors.end();
    };

    if (SgAuxiliaryDeclarationList *auxiliary =
            isSgAuxiliaryDeclarationList(parent)) {
      SgDeclarationStatement *declaration = isSgDeclarationStatement(child);
      SgScopeStatement *owner = isSgScopeStatement(auxiliary->get_parent());
      return declaration != nullptr && owner != nullptr &&
             owner->get_auxiliary_declarations() == auxiliary &&
             declaration->get_scope() == owner &&
             std::count(auxiliary->get_declarations().begin(),
                        auxiliary->get_declarations().end(), declaration) == 1;
    }

    if (SgScopeStatement *parent_scope = isSgScopeStatement(parent)) {
      if (SgAuxiliaryDeclarationList *auxiliary =
              isSgAuxiliaryDeclarationList(child)) {
        return auxiliary->get_parent() == parent_scope &&
               parent_scope->get_auxiliary_declarations() == auxiliary;
      }
      if (SgStatement *stmt = isSgStatement(child)) {
        if (parent_scope->containsOnlyDeclarations()) {
          if (isSgDeclarationStatement(stmt) != nullptr) {
            return parent_scope->statementExistsInScope(stmt);
          }
          return is_structural_successor(child, parent);
        }
        return parent_scope->statementExistsInScope(stmt);
      }
    }

    if (SgNamespaceDeclarationStatement *ns_decl =
            isSgNamespaceDeclarationStatement(parent)) {
      return ns_decl->get_definition() == child;
    }
    if (SgClassDeclaration *class_decl = isSgClassDeclaration(parent)) {
      return class_decl->get_definition() == child;
    }
    if (SgFunctionDeclaration *fn_decl = isSgFunctionDeclaration(parent)) {
      return fn_decl->get_definition() == child;
    }

    return is_structural_successor(child, parent);
  };

  for (SgNode *cursor = candidate; cursor != nullptr;
       cursor = cursor->get_parent()) {
    // A scope is reachable only if its parent chain passes through this
    // translation unit's actual global scope. Detached sibling globals can
    // share the same source file parent without belonging to the returned AST.
    if (cursor == file_global) {
      return true;
    }

    SgNode *parent = cursor->get_parent();
    if (parent == nullptr) {
      return false;
    }
    if (!attached_to_parent(cursor, parent)) {
      return false;
    }
  }

  return false;
}

SgType *ClangToSageTranslator::buildTypeFromQualifiedType(
    const clang::QualType &qual_type, SgType *unqualified_type) {

  if (unqualified_type == nullptr && qual_type.hasLocalQualifiers() &&
      llvm::isa_and_nonnull<clang::ArrayType>(qual_type.getTypePtrOrNull())) {
    const clang::ArrayType *array =
        p_compiler_instance->getASTContext().getAsArrayType(qual_type);
    ASSERT_not_null(array);
    llvm::SaveAndRestore<std::optional<const clang::DeclContext *>>
        canonical_context(p_canonical_template_argument_context,
                          qual_type.isCanonical()
                              ? std::optional<const clang::DeclContext *>(
                                    currentTemplateParameterDeclContext())
                              : p_canonical_template_argument_context);
    return requireExactType(isSgType(Traverse(array)), "qualified-array-type",
                            array);
  }

  const bool cache_translation = unqualified_type == nullptr &&
                                 p_explicit_template_id_type_use_depth == 0 &&
                                 canCacheQualifiedTypeTranslation(qual_type);
  const uintptr_t cache_key =
      cache_translation ? qualifiedTypeCacheKey(qual_type) : 0;
  if (cache_translation) {
    std::map<uintptr_t, SgType *>::const_iterator cached =
        p_qualified_type_translation_map.find(cache_key);
    if (cached != p_qualified_type_translation_map.end()) {
      return cached->second;
    }
  }

  SgNode *tmp_type = unqualified_type != nullptr
                         ? unqualified_type
                         : Traverse(qual_type.getTypePtrOrNull());
  SgType *type = requireExactType(isSgType(tmp_type), "qualified-type",
                                  qual_type.getTypePtrOrNull());

  // Prefer the exact translated specialization declaration only when the
  // source type itself denotes that specialization. A typedef, using-alias, or
  // alias-template specialization can have the same canonical RecordType;
  // replacing its exact translated type here discards source identity such as
  // `map_type::value_type` or `alias<T>`.
  const clang::Type *written_type = qual_type.getTypePtrOrNull();
  const clang::TemplateSpecializationType *written_specialization =
      llvm::dyn_cast_or_null<clang::TemplateSpecializationType>(written_type);
  const bool source_denotes_concrete_specialization =
      llvm::isa_and_nonnull<clang::RecordType>(written_type) ||
      (written_specialization != nullptr &&
       !written_specialization->isTypeAlias());
  clang::QualType canonical = qual_type.getCanonicalType();

  if (unqualified_type == nullptr &&
      p_explicit_template_id_type_use_depth == 0 &&
      source_denotes_concrete_specialization && !canonical.isNull()) {
    if (const clang::RecordType *record_type =
            canonical->getAs<clang::RecordType>()) {
      clang::RecordDecl *record_decl = record_type->getDecl();
      if (llvm::isa<clang::ClassTemplateSpecializationDecl>(record_decl) ||
          llvm::isa<clang::ClassTemplatePartialSpecializationDecl>(
              record_decl)) {
        if (SgType *inst_type = getTypeFromTranslatedRecordDecl(record_decl)) {
          type = requireExactType(inst_type, "template-specialization-type",
                                  qual_type.getTypePtrOrNull());
        }
      }
    }
  }

  if (qual_type.hasLocalQualifiers()) {
    clang::Qualifiers qualifier = qual_type.getLocalQualifiers();

    auto modifier_matches_qualifiers =
        [&](const SgTypeModifier &modifier) -> bool {
      if (modifier.get_constVolatileModifier().isConst() !=
              qualifier.hasConst() ||
          modifier.get_constVolatileModifier().isVolatile() !=
              qualifier.hasVolatile() ||
          modifier.isRestrict() != qualifier.hasRestrict()) {
        return false;
      }

      if (!qualifier.hasAddressSpace()) {
        return !modifier.haveAddressSpace() && !modifier.isOpenclGlobal() &&
               !modifier.isOpenclLocal() && !modifier.isOpenclConstant();
      }

      switch (qualifier.getAddressSpace()) {
      case clang::LangAS::opencl_global:
        return !modifier.haveAddressSpace() && modifier.isOpenclGlobal() &&
               !modifier.isOpenclLocal() && !modifier.isOpenclConstant();
      case clang::LangAS::opencl_local:
        return !modifier.haveAddressSpace() && !modifier.isOpenclGlobal() &&
               modifier.isOpenclLocal() && !modifier.isOpenclConstant();
      case clang::LangAS::opencl_constant:
        return !modifier.haveAddressSpace() && !modifier.isOpenclGlobal() &&
               !modifier.isOpenclLocal() && modifier.isOpenclConstant();
      default:
        return modifier.haveAddressSpace() && !modifier.isOpenclGlobal() &&
               !modifier.isOpenclLocal() && !modifier.isOpenclConstant() &&
               modifier.get_address_space_value() ==
                   static_cast<unsigned int>(qualifier.getAddressSpace());
      }
    };

    // Translating the unqualified dependency can recursively request this
    // same qualified type (for example, while materializing members of a class
    // specialization).  That nested transaction is allowed to publish the
    // unique wrapper first.  Recheck the cache after the dependency boundary
    // and require the published wrapper to have the exact base and qualifier
    // contract before adopting it.
    if (cache_translation) {
      auto reentrant = p_qualified_type_translation_map.find(cache_key);
      if (reentrant != p_qualified_type_translation_map.end()) {
        SgModifierType *cached_modifier = isSgModifierType(reentrant->second);
        if (cached_modifier == nullptr ||
            cached_modifier->get_base_type() != type ||
            !modifier_matches_qualifiers(cached_modifier->get_typeModifier())) {
          fprintf(stderr,
                  "REX_FRONTEND_INVARIANT[qualified-type-cache]: Clang "
                  "QualType key=%" PRIuPTR
                  " was reentrantly published with a different exact base "
                  "or qualifier set\n",
                  cache_key);
          ROSE_ABORT();
        }
        return cached_modifier;
      }
    }

    SgTypeModifier sg_modifer;

    if (qualifier.hasConst())
      sg_modifer.get_constVolatileModifier().setConst();
    if (qualifier.hasVolatile())
      sg_modifer.get_constVolatileModifier().setVolatile();
    if (qualifier.hasRestrict())
      sg_modifer.setRestrict();

    if (qualifier.hasAddressSpace()) {
      clang::LangAS addrspace = qualifier.getAddressSpace();
      switch (addrspace) {
      case clang::LangAS::opencl_global:
        sg_modifer.setOpenclGlobal();
        break;
      case clang::LangAS::opencl_local:
        sg_modifer.setOpenclLocal();
        break;
      case clang::LangAS::opencl_constant:
        sg_modifer.setOpenclConstant();
        break;
      default:
        sg_modifer.setAddressSpace();
        sg_modifer.set_address_space_value(
            static_cast<unsigned int>(addrspace));
      }
    }
    SgModifierType *modified_type =
        SageBuilder::buildModifierType(type, sg_modifer);
    if (cache_translation) {
      auto published =
          p_qualified_type_translation_map.emplace(cache_key, modified_type);
      if (!published.second && published.first->second != modified_type) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[qualified-type-cache]: Clang "
                "QualType key=%" PRIuPTR
                " acquired two exact Sage modifier identities\n",
                cache_key);
        ROSE_ABORT();
      }
    }

    return modified_type;
  } else {
    return type;
  }
}

SgType *ClangToSageTranslator::buildSemanticTypeFromQualifiedType(
    const clang::QualType &qual_type, const char *context) {
  SemanticExpressionConstruction construction(
      p_semantic_template_argument_expression_depth, context);
  SgType *type = buildTypeFromQualifiedType(qual_type);
  if (type == nullptr) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[semantic-type-construction]: context=%s "
            "produced no exact type\n",
            context);
    ROSE_ABORT();
  }
  return type;
}

SgType *ClangToSageTranslator::buildExplicitTemplateIdTypeUseFromQualifiedType(
    const clang::QualType &qual_type, const char *context) {
  struct ExplicitTemplateIdTypeUseGuard {
    unsigned &depth;
    const char *context;

    ExplicitTemplateIdTypeUseGuard(unsigned &depth, const char *context)
        : depth(depth), context(context) {
      ++depth;
    }

    ~ExplicitTemplateIdTypeUseGuard() {
      if (depth == 0) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[explicit-template-id-type-use]: "
                "context=%s lost its exact construction depth\n",
                context != nullptr ? context : "<null>");
        ROSE_ABORT();
      }
      --depth;
    }
  } explicit_type_use(p_explicit_template_id_type_use_depth, context);
  SemanticExpressionConstruction semantic_arguments(
      p_semantic_template_argument_expression_depth, context);
  SgType *type = buildTypeFromQualifiedType(qual_type);
  if (type == nullptr) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[explicit-template-id-type-use]: "
            "context=%s produced no exact type\n",
            context != nullptr ? context : "<null>");
    ROSE_ABORT();
  }
  return type;
}

SgType *ClangToSageTranslator::buildExplicitTemplateIdTypeUseFromTypeLoc(
    const clang::TypeLoc &type_loc, const char *context) {

  if (type_loc.isNull()) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[explicit-template-id-type-use]: "
            "context=%s has no exact TypeLoc\n",
            context != nullptr ? context : "<null>");
    ROSE_ABORT();
  }

  struct ExplicitTemplateIdTypeLocUseGuard {
    unsigned &explicit_type_use_depth;
    unsigned &explicit_type_loc_use_depth;
    unsigned &written_specialization_depth;
    const char *context;

    ExplicitTemplateIdTypeLocUseGuard(unsigned &explicit_type_use_depth,
                                      unsigned &explicit_type_loc_use_depth,
                                      unsigned &written_specialization_depth,
                                      const char *context)
        : explicit_type_use_depth(explicit_type_use_depth),
          explicit_type_loc_use_depth(explicit_type_loc_use_depth),
          written_specialization_depth(written_specialization_depth),
          context(context) {
      ++explicit_type_use_depth;
      ++explicit_type_loc_use_depth;
      ++written_specialization_depth;
    }

    ~ExplicitTemplateIdTypeLocUseGuard() {
      if (explicit_type_use_depth == 0 || explicit_type_loc_use_depth == 0 ||
          written_specialization_depth == 0) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[explicit-template-id-type-use]: "
                "context=%s lost its exact TypeLoc construction depth\n",
                context != nullptr ? context : "<null>");
        ROSE_ABORT();
      }
      --written_specialization_depth;
      --explicit_type_loc_use_depth;
      --explicit_type_use_depth;
    }
  } explicit_type_use(p_explicit_template_id_type_use_depth,
                      p_explicit_template_id_type_loc_use_depth,
                      p_force_written_template_specialization_depth, context);
  SemanticExpressionConstruction semantic_arguments(
      p_semantic_template_argument_expression_depth, context);
  SgType *type = buildTypeFromTypeLoc(type_loc);
  if (type == nullptr) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[explicit-template-id-type-use]: "
            "context=%s TypeLoc produced no exact type\n",
            context != nullptr ? context : "<null>");
    ROSE_ABORT();
  }
  if (SgNonrealType *nonreal_type = isSgNonrealType(type)) {
    SgNonrealDecl *nonreal_declaration =
        isSgNonrealDecl(nonreal_type->get_declaration());
    if (nonreal_declaration != nullptr &&
        nonreal_declaration->get_templateDeclaration() == nullptr &&
        !nonreal_declaration->get_tpl_args().empty()) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[explicit-template-id-type-use]: "
              "context=%s result=%s name=%s retains %zu template arguments "
              "without one exact template declaration; root-clang-type=%s\n",
              context != nullptr ? context : "<null>",
              type->class_name().c_str(), nonreal_declaration->get_name().str(),
              nonreal_declaration->get_tpl_args().size(),
              type_loc.getTypePtr()->getTypeClassName());
      ROSE_ABORT();
    }
  }
  return type;
}

SgType *
ClangToSageTranslator::buildTypeFromTypeLoc(const clang::TypeLoc &type_loc,
                                            SgType *member_function_class) {
  SgType *result = buildTypeFromTypeLocImpl(type_loc, member_function_class);
  if (result == nullptr) {
    return nullptr;
  }
  publishSourceElaborationKind(result, type_loc, p_compiler_instance);
  return result;
}

SgType *ClangToSageTranslator::buildTypeFromTypeLocWithSemanticOwner(
    const clang::TypeLoc &type_loc, SgScopeStatement *semantic_owner_scope,
    const char *context) {
  if (semantic_owner_scope == nullptr || context == nullptr ||
      *context == '\0') {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[type-loc-semantic-owner]: context=%s "
            "TypeLoc requires one exact nonnull semantic owner\n",
            context != nullptr ? context : "<null>");
    ROSE_ABORT();
  }

  p_type_loc_semantic_owner_scope_stack.push_back(semantic_owner_scope);
  struct SemanticOwnerGuard {
    std::vector<SgScopeStatement *> &stack;
    SgScopeStatement *owner;
    const char *context;
    ~SemanticOwnerGuard() {
      if (stack.empty() || stack.back() != owner) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[type-loc-semantic-owner]: "
                "context=%s lost its exact semantic owner transaction\n",
                context);
        ROSE_ABORT();
      }
      stack.pop_back();
    }
  } semantic_owner_guard{p_type_loc_semantic_owner_scope_stack,
                         semantic_owner_scope, context};

  return requireExactType(
      buildTypeFromTypeLoc(type_loc), context,
      type_loc.isNull() ? nullptr : type_loc.getType().getTypePtrOrNull());
}

SgType *
ClangToSageTranslator::buildTypeFromTypeLocImpl(const clang::TypeLoc &type_loc,
                                                SgType *member_function_class) {

  if (type_loc.isNull()) {
    return nullptr;
  }

  if (clang::SubstTemplateTypeParmTypeLoc substituted_loc =
          type_loc.getAs<clang::SubstTemplateTypeParmTypeLoc>()) {
    const clang::SubstTemplateTypeParmType *substituted_type =
        substituted_loc.getTypePtr();
    const clang::TemplateTypeParmDecl *replaced_parameter =
        substituted_type != nullptr
            ? llvm::dyn_cast_or_null<clang::TemplateTypeParmDecl>(
                  substituted_type->getReplacedParameter())
            : nullptr;
    for (auto context = p_exact_template_type_argument_loc_stack.rbegin();
         replaced_parameter != nullptr &&
         context != p_exact_template_type_argument_loc_stack.rend();
         ++context) {
      auto binding = context->find(replaced_parameter);
      if (binding == context->end()) {
        continue;
      }

      const clang::TemplateArgumentLoc &argument_loc = binding->second;
      const clang::TemplateArgument &argument = argument_loc.getArgument();
      const clang::TypeSourceInfo *type_info = argument_loc.getTypeSourceInfo();
      if (argument.getKind() != clang::TemplateArgument::Type ||
          type_info == nullptr) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[explicit-instantiation-type-"
                "substitution]: template type parameter=%p has no exact "
                "written type argument\n",
                static_cast<const void *>(replaced_parameter));
        ROSE_ABORT();
      }

      clang::TypeLoc argument_type_loc = type_info->getTypeLoc();

      if (argument_type_loc.isNull() ||
          argument_type_loc.getTypePtr() == type_loc.getTypePtr()) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[explicit-instantiation-type-"
                "substitution]: template type parameter=%p maps to a null or "
                "recursive exact TypeLoc\n",
                static_cast<const void *>(replaced_parameter));
        ROSE_ABORT();
      }
      SgType *written_type = buildTypeFromTypeLoc(argument_type_loc);
      if (written_type == nullptr) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[explicit-instantiation-type-"
                "substitution]: template type parameter=%p produced no exact "
                "written Sage type\n",
                static_cast<const void *>(replaced_parameter));
        ROSE_ABORT();
      }
      return written_type;
    }
  }

  auto resolve_scope = [&]() -> SgScopeStatement * {
    SgScopeStatement *scope = !p_type_loc_semantic_owner_scope_stack.empty()
                                  ? p_type_loc_semantic_owner_scope_stack.back()
                                  : SageBuilder::topScopeStack();
    if (scope == nullptr) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[type-loc-semantic-owner]: TypeLoc=%s "
              "has neither an explicit producer-owned semantic scope nor an "
              "active exact construction scope\n",
              type_loc.getTypePtr()->getTypeClassName());
      ROSE_ABORT();
    }
    return scope;
  };

  auto apply_local_qualifiers =
      [&](SgType *base_type, const clang::QualType &qual_type) -> SgType * {
    if (base_type == nullptr || !qual_type.hasLocalQualifiers()) {
      return base_type;
    }

    SgTypeModifier modifier;
    clang::Qualifiers qualifier = qual_type.getLocalQualifiers();

    if (qualifier.hasConst()) {
      modifier.get_constVolatileModifier().setConst();
    }
    if (qualifier.hasVolatile()) {
      modifier.get_constVolatileModifier().setVolatile();
    }
    if (qualifier.hasRestrict()) {
      modifier.setRestrict();
    }

    if (qualifier.hasAddressSpace()) {
      clang::LangAS addrspace = qualifier.getAddressSpace();
      switch (addrspace) {
      case clang::LangAS::opencl_global:
        modifier.setOpenclGlobal();
        break;
      case clang::LangAS::opencl_local:
        modifier.setOpenclLocal();
        break;
      case clang::LangAS::opencl_constant:
        modifier.setOpenclConstant();
        break;
      default:
        modifier.setAddressSpace();
        modifier.set_address_space_value(static_cast<unsigned int>(addrspace));
      }
    }

    if (base_type->has_source_prototype()) {
      auto *written = new SgModifierType(base_type);
      written->get_typeModifier() = modifier;
      return written;
    }
    return SageBuilder::buildModifierType(base_type, modifier);
  };

  auto apply_exact_pointee_qualifiers =
      [&](SgType *pointee_type, const clang::QualType &written_pointee_type,
          const clang::QualType &semantic_pointee_type) -> SgType * {
    if (pointee_type == nullptr || written_pointee_type.isNull() ||
        semantic_pointee_type.isNull()) {
      std::cerr << "REX_CFE_TYPE_INVARIANT[pointee-qualifiers]: pointer or "
                   "reference TypeLoc lacks an exact written or semantic "
                   "pointee type\n";
      ROSE_ABORT();
    }
    if (written_pointee_type.hasLocalQualifiers()) {
      return pointee_type;
    }
    if (semantic_pointee_type.hasLocalQualifiers()) {
      return apply_local_qualifiers(pointee_type, semantic_pointee_type);
    }
    return pointee_type;
  };

  auto canonical_sg_class_decl =
      [](SgClassDeclaration *decl) -> SgClassDeclaration * {
    if (decl == nullptr) {
      return nullptr;
    }
    if (SgClassDeclaration *first =
            isSgClassDeclaration(decl->get_firstNondefiningDeclaration())) {
      return first;
    }
    if (SgClassDeclaration *def =
            isSgClassDeclaration(decl->get_definingDeclaration())) {
      return def;
    }
    return decl;
  };

  auto qualifier_targets_current_enclosing_class =
      [&](clang::NestedNameSpecifier qualifier,
          SgScopeStatement *scope) -> bool {
    if (!qualifier ||
        qualifier.getKind() != clang::NestedNameSpecifier::Kind::Type ||
        scope == nullptr) {
      return false;
    }

    SgClassDefinition *enclosing_def =
        SageInterface::getEnclosingClassDefinition(scope, true);
    if (enclosing_def == nullptr) {
      return false;
    }

    clang::QualType qualifier_qual_type(qualifier.getAsType(), 0);

    if (qualifier_qual_type.isNull()) {
      return false;
    }

    const clang::CXXRecordDecl *target_record =
        cxxRecordDeclFromQualTypeWithoutDefinitionLookup(qualifier_qual_type);
    if (target_record == nullptr) {
      return false;
    }

    SgClassDeclaration *target_decl = canonical_sg_class_decl(
        isSgClassDeclaration(lookupSgDeclarationForClangDecl(
            const_cast<clang::CXXRecordDecl *>(target_record),
            /*allow_on_demand=*/false)));
    if (target_decl == nullptr) {
      target_decl = canonical_sg_class_decl(
          isSgClassDeclaration(lookupSgDeclarationForClangDecl(
              const_cast<clang::CXXRecordDecl *>(
                  target_record->getCanonicalDecl()),
              /*allow_on_demand=*/false)));
    }

    SgClassDeclaration *enclosing_decl =
        canonical_sg_class_decl(enclosing_def->get_declaration());
    return target_decl != nullptr && enclosing_decl == target_decl;
  };

  auto append_nonreal_template_arguments =
      [&](SgTemplateArgumentPtrList &args,
          clang::TemplateSpecializationTypeLoc specialization_loc,
          const char *context) {
        // SgNonrealDecl owns a semantic template-id identity.  Its argument
        // expressions may originate in a TypeLoc, but they are not lexical
        // children of that use site and must never retain source provenance
        // when adopted by the shared semantic declaration.
        SemanticExpressionConstruction semantic_arguments(
            p_semantic_template_argument_expression_depth, context);

        std::vector<const clang::TemplateArgument *> converted_arguments;
        if (const auto *record_type =
                specialization_loc.getType()->getAs<clang::RecordType>()) {
          if (const auto *specialization =
                  llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(
                      record_type->getDecl())) {
            std::function<void(const clang::TemplateArgument &)>
                append_argument = [&](const clang::TemplateArgument &argument) {
                  if (argument.getKind() == clang::TemplateArgument::Pack) {
                    for (const auto &element : argument.pack_elements())
                      append_argument(element);
                  } else {
                    converted_arguments.push_back(&argument);
                  }
                };
            for (const auto &argument :
                 specialization->getTemplateArgs().asArray())
              append_argument(argument);
          }
        }
        const unsigned argument_count = specialization_loc.getNumArgs();
        for (unsigned index = 0; index < argument_count; ++index) {
          const auto &written = specialization_loc.getArgLoc(index);
          const size_t semantic_index = args.size();
          if (written.getArgument().getKind() ==
                  clang::TemplateArgument::Expression &&
              semantic_index < converted_arguments.size() &&
              converted_arguments[semantic_index]->getKind() ==
                  clang::TemplateArgument::Declaration) {
            clang::TemplateArgumentLoc converted(
                *converted_arguments[semantic_index],
                written.getSourceExpression());
            appendTemplateArguments(args, converted, true);
          } else {
            appendTemplateArguments(args, written, true);
          }
        }
      };

  auto resolves_to_primary_template_class_type = [](SgType *candidate) -> bool {
    if (candidate == nullptr) {
      return false;
    }

    auto *class_type = isSgClassType(candidate->findBaseType());
    if (class_type == nullptr) {
      return false;
    }
    auto *declaration = isSgClassDeclaration(class_type->get_declaration());
    ROSE_ASSERT(declaration != nullptr &&
                declaration->get_firstNondefiningDeclaration() == declaration &&
                declaration->get_type() == class_type);
    return isSgTemplateClassDeclaration(declaration) != nullptr;
  };

  struct TranslatedTypedefTypeIdentity {
    SgType *type = nullptr;
    SgDeclarationStatement *declaration = nullptr;
  };
  auto build_translated_typedef_type_from_decl =
      [&](clang::TypedefNameDecl *typedef_decl)
      -> TranslatedTypedefTypeIdentity {
    if (typedef_decl == nullptr) {
      return {};
    }

    SgDeclarationStatement *translated_decl =
        lookupSgDeclarationForClangDecl(typedef_decl, /*allow_on_demand=*/true);

    if (SgTypedefDeclaration *sg_typedef_decl =
            isSgTypedefDeclaration(translated_decl)) {
      validateTypedefDeclarationReferenceShared(sg_typedef_decl);
      return {sg_typedef_decl->get_type(), translated_decl};
    }

    if (SgTemplateTypedefDeclaration *sg_template_typedef =
            isSgTemplateTypedefDeclaration(translated_decl)) {
      return {sg_template_typedef->get_type(), translated_decl};
    }

    return {};
  };

  auto translated_class_like_scope =
      [&](clang::Decl *decl_key) -> SgScopeStatement * {
    if (decl_key == nullptr) {
      return nullptr;
    }

    auto scope_from_translated_node = [](SgNode *node) -> SgScopeStatement * {
      if (SgClassDefinition *def = isSgClassDefinition(node)) {
        return def;
      }
      if (SgClassDeclaration *decl = isSgClassDeclaration(node)) {
        if (decl->get_definition() != nullptr) {
          return decl->get_definition();
        }
        if (SgClassDeclaration *def_decl =
                isSgClassDeclaration(decl->get_definingDeclaration())) {
          if (def_decl->get_definition() != nullptr) {
            return def_decl->get_definition();
          }
        }
      }
      return nullptr;
    };

    if (SgScopeStatement *scope =
            scope_from_translated_node(lookupSgDeclarationForClangDecl(
                decl_key, /*allow_on_demand=*/true))) {
      return scope;
    }
    return nullptr;
  };

  auto nested_name_specifier_loc_prefix =
      [](clang::NestedNameSpecifierLoc qualifier_loc)
      -> clang::NestedNameSpecifierLoc {
    if (!qualifier_loc) {
      return clang::NestedNameSpecifierLoc();
    }

    switch (qualifier_loc.getNestedNameSpecifier().getKind()) {
    case clang::NestedNameSpecifier::Kind::Namespace:
      return qualifier_loc.getAsNamespaceAndPrefix().Prefix;
    case clang::NestedNameSpecifier::Kind::Type:
      return qualifier_loc.getAsTypeLoc().getPrefix();
    case clang::NestedNameSpecifier::Kind::Global:
    case clang::NestedNameSpecifier::Kind::MicrosoftSuper:
    case clang::NestedNameSpecifier::Kind::Null:
      return clang::NestedNameSpecifierLoc();
    }

    return clang::NestedNameSpecifierLoc();
  };

  auto apply_global_qualifier_from_loc =
      [&](SgType *candidate,
          clang::NestedNameSpecifierLoc qualifier_loc) -> SgType * {
    SgNonrealType *nrtype = isSgNonrealType(candidate);
    SgNonrealDecl *nrdecl = isSgNonrealDecl(
        nrtype != nullptr ? nrtype->get_declaration() : nullptr);
    if (nrdecl != nullptr && qualifier_loc &&
        nestedNameSpecifierLocHasExplicitGlobal(qualifier_loc)) {
      nrdecl->set_has_global_qualifier(true);
    }
    return candidate;
  };

  auto finalize_spelled_type = [&](SgType *candidate) -> SgType * {
    candidate = apply_local_qualifiers(candidate, type_loc.getType());
    return candidate;
  };

  if (clang::PackExpansionTypeLoc pack_loc =
          type_loc.getAs<clang::PackExpansionTypeLoc>()) {
    clang::TypeLoc pattern_loc = pack_loc.getPatternLoc();

    const clang::SourceLocation ellipsis_loc = pack_loc.getEllipsisLoc();
    if (pattern_loc.isNull() || ellipsis_loc.isInvalid()) {
      fprintf(
          stderr,
          "REX_FRONTEND_INVARIANT[pack-expansion-type-surface]: written "
          "pack expansion has no exact pattern TypeLoc or ellipsis token\n");
      ROSE_ABORT();
    }

    // PackExpansionTypeLoc describes a use-site declarator.  The enclosing
    // typed grammar owner records its ellipsis: SgInitializedName for a
    // parameter declaration, SgTemplateArgument for a template argument, or
    // the dedicated base/expression pack-expansion node.  Returning the
    // declaration-owned SgTemplateType here would move the token inside pointer
    // and reference operators (for example, `Args...&&` instead of
    // `Args&&...`) and would leak it into every use of the parameter.
    SgType *pattern_type = buildTypeFromTypeLoc(pattern_loc);
    if (pattern_type == nullptr) {
      failExactTypeTranslation("pack-expansion-pattern-typeloc",
                               pack_loc.getTypePtr());
    }
    if (SgRvalueReferenceType *reference =
            isSgRvalueReferenceType(pattern_type)) {
      if (SgTemplateType *parameter =
              isSgTemplateType(reference->get_base_type());
          parameter != nullptr && parameter->get_packed()) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[pack-expansion-type-surface]: exact "
                "rvalue-reference pattern retained declaration-owned pack "
                "syntax\n");
        ROSE_ABORT();
      }
    }
    pattern_type = finalize_spelled_type(pattern_type);
    if (SgRvalueReferenceType *reference =
            isSgRvalueReferenceType(pattern_type)) {
      if (SgTemplateType *parameter =
              isSgTemplateType(reference->get_base_type());
          parameter != nullptr && parameter->get_packed()) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[pack-expansion-type-surface]: "
                "finalization replaced the exact rvalue-reference pattern "
                "with declaration-owned pack syntax\n");
        ROSE_ABORT();
      }
    }
    return pattern_type;
  }

  if (clang::TemplateTypeParmTypeLoc parameter_loc =
          type_loc.getAs<clang::TemplateTypeParmTypeLoc>()) {
    const clang::TemplateTypeParmType *parameter_type =
        parameter_loc.getTypePtr();
    const clang::TemplateTypeParmDecl *parameter_decl = parameter_loc.getDecl();
    std::string name;
    SgTemplateParameter *sage_parameter = nullptr;
    if (parameter_decl != nullptr) {
      name = clangTemplateParameterSpelling(parameter_decl);
      sage_parameter = lookupActiveTemplateParameterSurface(
          parameter_decl, "template-parameter-type-source");
      if (sage_parameter == nullptr) {
        if (clang::TemplateTypeParmDecl *active_parameter =
                resolveActiveTemplateTypeParameterSourceCounterpart(
                    parameter_decl,
                    "template-parameter-type-source-semantic-counterpart")) {
          parameter_decl = active_parameter;
          sage_parameter = lookupActiveTemplateParameterSurface(
              active_parameter,
              "template-parameter-type-source-semantic-counterpart");
          name = clangTemplateParameterSpelling(active_parameter);
        }
      }
      if (sage_parameter == nullptr) {
        sage_parameter = lookupPublishedTemplateTypeParameterFamily(
            parameter_decl, "template-parameter-type-source",
            /*require_exact_owner=*/true);
      }
    } else if (clang::TemplateTypeParmDecl *active_parameter =
                   resolveActiveTemplateTypeParameterSurface(
                       parameter_type->getDepth(), parameter_type->getIndex(),
                       "template-parameter-type-source")) {
      parameter_decl = active_parameter;
      sage_parameter = lookupActiveTemplateParameterSurface(
          active_parameter, "template-parameter-type-source-coordinate");
      if (sage_parameter == nullptr) {
        sage_parameter = lookupPublishedTemplateTypeParameterFamily(
            active_parameter,
            "template-parameter-type-source-coordinate-family",
            /*require_exact_owner=*/true);
      }
      name = clangTemplateParameterSpelling(active_parameter);
    }
    const bool is_implicit_auto =
        parameter_decl != nullptr && parameter_decl->isImplicit();
    if (name.empty() && !is_implicit_auto) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[template-parameter-type-surface]: "
              "depth=%u index=%u has no exact written parameter name\n",
              parameter_type->getDepth(), parameter_type->getIndex());
      ROSE_ABORT();
    }
    if (sage_parameter == nullptr) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[template-parameter-type-owner]: "
              "parameter=%p name='%s' depth=%u index=%u active-surfaces=%zu "
              "has no exact Sage declaration surface\n",
              static_cast<const void *>(parameter_decl), name.c_str(),
              parameter_type->getDepth(), parameter_type->getIndex(),
              p_template_parameter_surface_stack.size());
      ROSE_ABORT();
    }
    ROSE_ASSERT(!is_implicit_auto ||
                sage_parameter->get_isAbbreviatedFunctionTemplateParameter());
    SgTemplateType *sage_parameter_type =
        isSgTemplateType(sage_parameter->get_type());
    if (sage_parameter_type == nullptr ||
        sage_parameter_type->get_template_parameter() != sage_parameter ||
        !sage_parameter_type->get_canonical_source_identity().has_value()) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[template-parameter-type-owner]: "
              "parameter=%p has no exact typed canonical source identity\n",
              static_cast<void *>(sage_parameter));
      ROSE_ABORT();
    }

    // A template parameter declaration owns pack identity, but a reference to
    // that parameter owns no ellipsis unless an enclosing PackExpansionTypeLoc
    // says so.  Publish a per-use spelling type instead of returning or
    // mutating the declaration's SgTemplateType.
    SgTemplateType *source_type = new SgTemplateType(SgName(name));
    source_type->set_template_parameter_depth(
        static_cast<int>(parameter_type->getDepth()));
    source_type->set_template_parameter_position(
        static_cast<int>(parameter_type->getIndex()));
    source_type->initialize_canonical_source_identity(
        *sage_parameter_type->get_canonical_source_identity());
    source_type->set_template_parameter(sage_parameter);
    return finalize_spelled_type(source_type);
  }

  if (clang::SubstTemplateTypeParmTypeLoc substituted_loc =
          type_loc.getAs<clang::SubstTemplateTypeParmTypeLoc>()) {
    const clang::SubstTemplateTypeParmType *substituted_type =
        substituted_loc.getTypePtr();
    const clang::TemplateTypeParmDecl *parameter_decl =
        substituted_type != nullptr
            ? llvm::dyn_cast_or_null<clang::TemplateTypeParmDecl>(
                  substituted_type->getReplacedParameter())
            : nullptr;
    if (parameter_decl == nullptr) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[substituted-parameter-type-surface]: "
              "SubstTemplateTypeParmTypeLoc has no exact replaced parameter\n");
      ROSE_ABORT();
    }

    const std::string name = clangTemplateParameterSpelling(parameter_decl);
    if (name.empty()) {
      // An unnamed template parameter cannot be referenced in source.  For
      // such a substitution the only possible typed surface is Clang's exact
      // replacement type; treating the anonymous declaration identity as a
      // name would fabricate syntax.
      clang::QualType replacement_type = substituted_type->getReplacementType();
      if (replacement_type.isNull() ||
          replacement_type.getTypePtrOrNull() == substituted_type) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[substituted-parameter-type-surface]: "
                "unnamed replaced parameter depth=%u index=%u has no exact "
                "replacement type\n",
                parameter_decl->getDepth(), parameter_decl->getIndex());
        ROSE_ABORT();
      }
      SgType *replacement = buildTypeFromQualifiedType(replacement_type);
      if (replacement == nullptr) {
        failExactTypeTranslation("substituted-unnamed-parameter-replacement",
                                 substituted_type);
      }
      return finalize_spelled_type(replacement);
    }

    auto *parameter = const_cast<clang::TemplateTypeParmDecl *>(parameter_decl);
    SgTemplateParameter *sage_parameter = lookupActiveTemplateParameterSurface(
        parameter, "substituted-template-parameter-source");
    if (sage_parameter == nullptr) {
      SgDeclarationStatement *owner =
          resolveTemplateParameterOwner(parameter, /*allow_on_demand=*/true);
      sage_parameter = requirePublishedTemplateParameter(
          parameter, owner, "substituted-template-parameter-source");
    }
    ASSERT_not_null(sage_parameter);
    SgTemplateType *source_type = new SgTemplateType(SgName(name));
    source_type->set_template_parameter_depth(
        static_cast<int>(parameter_decl->getDepth()));
    source_type->set_template_parameter_position(
        static_cast<int>(parameter_decl->getIndex()));
    publishCanonicalTemplateParameterSourceIdentity(
        source_type, const_cast<clang::TemplateTypeParmDecl *>(parameter_decl),
        "buildTypeFromTypeLoc:substituted-template-parameter");
    source_type->set_template_parameter(sage_parameter);
    return finalize_spelled_type(source_type);
  }

  if (clang::SubstTemplateTypeParmPackTypeLoc substituted_loc =
          type_loc.getAs<clang::SubstTemplateTypeParmPackTypeLoc>()) {
    const clang::SubstTemplateTypeParmPackType *substituted_type =
        substituted_loc.getTypePtr();
    const clang::TemplateTypeParmDecl *parameter_decl =
        substituted_type != nullptr
            ? llvm::dyn_cast_or_null<clang::TemplateTypeParmDecl>(
                  substituted_type->getReplacedParameter())
            : nullptr;
    if (parameter_decl == nullptr) {
      fprintf(stderr, "REX_FRONTEND_INVARIANT[substituted-pack-type-surface]: "
                      "SubstTemplateTypeParmPackTypeLoc has no exact replaced "
                      "parameter\n");
      ROSE_ABORT();
    }

    const std::string name = clangTemplateParameterSpelling(parameter_decl);
    if (name.empty()) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[substituted-pack-type-surface]: "
              "replaced parameter depth=%u index=%u has no exact written "
              "name\n",
              parameter_decl->getDepth(), parameter_decl->getIndex());
      ROSE_ABORT();
    }

    auto *parameter = const_cast<clang::TemplateTypeParmDecl *>(parameter_decl);
    SgTemplateParameter *sage_parameter = lookupActiveTemplateParameterSurface(
        parameter, "substituted-template-parameter-pack-source");
    if (sage_parameter == nullptr) {
      SgDeclarationStatement *owner =
          resolveTemplateParameterOwner(parameter, /*allow_on_demand=*/true);
      sage_parameter = requirePublishedTemplateParameter(
          parameter, owner, "substituted-template-parameter-pack-source");
    }
    ASSERT_not_null(sage_parameter);
    SgTemplateType *source_type = new SgTemplateType(SgName(name));
    source_type->set_template_parameter_depth(
        static_cast<int>(parameter_decl->getDepth()));
    source_type->set_template_parameter_position(
        static_cast<int>(parameter_decl->getIndex()));
    publishCanonicalTemplateParameterSourceIdentity(
        source_type, const_cast<clang::TemplateTypeParmDecl *>(parameter_decl),
        "buildTypeFromTypeLoc:substituted-template-parameter-pack");
    source_type->set_template_parameter(sage_parameter);
    return finalize_spelled_type(source_type);
  }

  auto classify_tag_decl_spelled_in_type_loc =
      [&](clang::TypeLoc spelled_type_loc) -> bool {
    if (spelled_type_loc.isNull()) {
      return false;
    }

    clang::SourceManager &sm = p_compiler_instance->getSourceManager();
    auto file_loc = [&](clang::SourceLocation loc) -> clang::SourceLocation {
      if (!loc.isValid()) {
        return clang::SourceLocation();
      }
      if (loc.isMacroID()) {
        loc = sm.getSpellingLoc(loc);
      }
      return sm.getFileLoc(loc);
    };

    clang::SourceRange type_range = spelled_type_loc.getSourceRange();
    clang::SourceLocation range_begin = file_loc(type_range.getBegin());
    clang::SourceLocation range_end = file_loc(type_range.getEnd());
    if (!range_begin.isValid() || !range_end.isValid()) {
      return false;
    }

    clang::TagDecl *tag_decl = nullptr;

    for (clang::TypeLoc current = spelled_type_loc; !current.isNull();
         current = current.getNextTypeLoc()) {

      if (clang::TagTypeLoc tag_loc = current.getAs<clang::TagTypeLoc>()) {
        clang::TagDecl *written_tag = tag_loc.getDecl();
        clang::SourceLocation written_location =
            written_tag != nullptr ? file_loc(written_tag->getBeginLoc())
                                   : clang::SourceLocation();
        if (written_location.isValid() &&
            !sm.isBeforeInTranslationUnit(written_location, range_begin) &&
            !sm.isBeforeInTranslationUnit(range_end, written_location)) {
          tag_decl = written_tag;
          break;
        }
      }
    }

    if (tag_decl == nullptr) {
      return false;
    }

    if (!tag_decl->isEmbeddedInDeclarator()) {
      return false;
    }

    clang::SourceLocation decl_loc = file_loc(tag_decl->getBeginLoc());
    if (!decl_loc.isValid() ||
        sm.isBeforeInTranslationUnit(decl_loc, range_begin) ||
        sm.isBeforeInTranslationUnit(range_end, decl_loc)) {
      return false;
    }

    if (tag_decl->isThisDeclarationADefinition()) {
      p_inline_tag_decls.insert(tag_decl->getCanonicalDecl());
    }

    SgScopeStatement *typed_owner_scope =
        activeTypedOwnerTagConstructionScope(tag_decl);
    SgDeclarationScope *source_declarator_scope =
        activeSourceDeclaratorTagScope(tag_decl);
    SgScopeStatement *declarator_introduction_scope =
        activeDeclaratorOwnedTagIntroductionScope(tag_decl);
    if (source_declarator_scope != nullptr &&
        declarator_introduction_scope != nullptr) {
      std::cerr << "REX_FRONTEND_INVARIANT[embedded-tag-owner]: Clang tag="
                << tag_decl
                << " is claimed by both nested and direct declarator owners"
                << std::endl;
      ROSE_ABORT();
    }
    auto publish_spelled_tag = [&](SgDeclarationStatement *decl,
                                   SgDeclarationStatement
                                       *defining_declaration) {
      if (decl == nullptr) {
        return;
      }
      if (typed_owner_scope != nullptr) {
        if (decl->get_scope() != typed_owner_scope ||
            countLexicalDeclarationEdges(typed_owner_scope, decl) != 0) {
          std::cerr << "REX_FRONTEND_INVARIANT[embedded-tag-owner]: "
                       "declaration="
                    << decl
                    << " was not constructed in the typed owner's exact "
                       "lexical scope"
                    << std::endl;
          ROSE_ABORT();
        }
        if (decl == defining_declaration) {
          if (decl->get_parent() != nullptr) {
            std::cerr << "REX_FRONTEND_INVARIANT[embedded-tag-owner]: source "
                         "defining declaration="
                      << decl
                      << " was not constructed detached for its typed owner"
                      << std::endl;
            ROSE_ABORT();
          }
        } else {
          SgAuxiliaryDeclarationList *auxiliary =
              isSgAuxiliaryDeclarationList(decl->get_parent());
          if (auxiliary == nullptr ||
              auxiliary->get_parent() != typed_owner_scope ||
              typed_owner_scope->get_auxiliary_declarations() != auxiliary ||
              std::count(auxiliary->get_declarations().begin(),
                         auxiliary->get_declarations().end(), decl) != 1) {
            std::cerr
                << "REX_FRONTEND_INVARIANT[embedded-tag-owner]: canonical "
                   "declaration="
                << decl
                << " has no exact auxiliary semantic owner in the typed "
                   "owner's lexical scope"
                << std::endl;
            ROSE_ABORT();
          }
        }
      } else if (source_declarator_scope != nullptr) {
        if (decl->get_parent() != source_declarator_scope ||
            decl->get_scope() == nullptr ||
            std::count(source_declarator_scope->get_declarations().begin(),
                       source_declarator_scope->get_declarations().end(),
                       decl) != 1 ||
            countLexicalDeclarationEdges(decl->get_scope(), decl) != 0) {
          std::cerr
              << "REX_FRONTEND_INVARIANT[embedded-tag-owner]: declaration="
              << decl
              << " lacks one exact source-declarator-scope owner while "
                 "retaining its semantic tag scope: parent="
              << decl->get_parent() << " expected=" << source_declarator_scope
              << " semantic-scope=" << decl->get_scope() << " occurrences="
              << std::count(source_declarator_scope->get_declarations().begin(),
                            source_declarator_scope->get_declarations().end(),
                            decl)
              << " semantic-lexical-edges="
              << (decl->get_scope() != nullptr
                      ? countLexicalDeclarationEdges(decl->get_scope(), decl)
                      : 0)
              << std::endl;
          ROSE_ABORT();
        }
      } else if (declarator_introduction_scope != nullptr) {
        SgAuxiliaryDeclarationList *auxiliary =
            isSgAuxiliaryDeclarationList(decl->get_parent());
        if (decl->get_scope() != declarator_introduction_scope ||
            decl->get_firstNondefiningDeclaration() != decl ||
            decl->get_definingDeclaration() == decl || auxiliary == nullptr ||
            auxiliary->get_parent() != declarator_introduction_scope ||
            declarator_introduction_scope->get_auxiliary_declarations() !=
                auxiliary ||
            std::count(auxiliary->get_declarations().begin(),
                       auxiliary->get_declarations().end(), decl) != 1 ||
            countLexicalDeclarationEdges(declarator_introduction_scope, decl) !=
                0) {
          std::cerr << "REX_FRONTEND_INVARIANT[embedded-tag-owner]: direct "
                       "declarator tag introduction="
                    << decl
                    << " has no exact temporary auxiliary semantic owner"
                    << std::endl;
          ROSE_ABORT();
        }
      } else if (defining_declaration != nullptr &&
                 tag_decl->isThisDeclarationADefinition() &&
                 p_inline_tag_decls.count(tag_decl->getCanonicalDecl()) == 1) {
        SgScopeStatement *semantic_scope = defining_declaration->get_scope();
        if (semantic_scope == nullptr || decl->get_scope() != semantic_scope ||
            decl->get_definingDeclaration() != defining_declaration ||
            countLexicalDeclarationEdges(semantic_scope, decl) != 0) {
          std::cerr
              << "REX_FRONTEND_INVARIANT[embedded-tag-owner]: completed "
                 "inline declaration="
              << decl
              << " does not retain one exact semantic scope and declaration "
                 "chain"
              << std::endl;
          ROSE_ABORT();
        }
        if (decl == defining_declaration) {
          if (decl->get_parent() != nullptr) {
            std::cerr << "REX_FRONTEND_INVARIANT[embedded-tag-owner]: "
                         "completed inline defining declaration="
                      << decl << " is not detached for its typed owner"
                      << std::endl;
            ROSE_ABORT();
          }
        } else {
          SgAuxiliaryDeclarationList *auxiliary =
              isSgAuxiliaryDeclarationList(decl->get_parent());
          if (auxiliary == nullptr ||
              auxiliary->get_parent() != semantic_scope ||
              semantic_scope->get_auxiliary_declarations() != auxiliary ||
              std::count(auxiliary->get_declarations().begin(),
                         auxiliary->get_declarations().end(), decl) != 1) {
            std::cerr << "REX_FRONTEND_INVARIANT[embedded-tag-owner]: "
                         "completed inline canonical declaration="
                      << decl
                      << " has no exact auxiliary semantic owner beside its "
                         "detached defining declaration"
                      << std::endl;
            ROSE_ABORT();
          }
        }
      } else if (decl->get_parent() != nullptr ||
                 decl->get_scope() == nullptr ||
                 countLexicalDeclarationEdges(decl->get_scope(), decl) != 0) {
        std::cerr
            << "REX_FRONTEND_INVARIANT[embedded-tag-owner]: declaration="
            << decl << "/" << decl->class_name()
            << " was not constructed as a detached embedded declarator child"
            << ": parent=" << decl->get_parent() << "/"
            << (decl->get_parent() != nullptr ? decl->get_parent()->class_name()
                                              : std::string("<null>"))
            << " semantic-scope=" << decl->get_scope() << "/"
            << (decl->get_scope() != nullptr ? decl->get_scope()->class_name()
                                             : std::string("<null>"))
            << " semantic-lexical-edges="
            << (decl->get_scope() != nullptr
                    ? countLexicalDeclarationEdges(decl->get_scope(), decl)
                    : 0)
            << " clang-tag=" << tag_decl << "/"
            << tag_decl->getQualifiedNameAsString() << " clang-definition="
            << (tag_decl->isThisDeclarationADefinition() ? 1 : 0)
            << " defining-declaration=" << defining_declaration
            << " inline-canonical="
            << p_inline_tag_decls.count(tag_decl->getCanonicalDecl())
            << std::endl;
        ROSE_ABORT();
      }
      if (SgClassDeclaration *class_decl = isSgClassDeclaration(decl)) {
        class_decl->set_isAutonomousDeclaration(false);
      } else if (SgEnumDeclaration *enum_decl = isSgEnumDeclaration(decl)) {
        enum_decl->set_isAutonomousDeclaration(false);
      } else {
        std::cerr << "REX_FRONTEND_INVARIANT[embedded-tag-typed-owner]: "
                     "translated tag is neither a class nor enum declaration"
                  << std::endl;
        ROSE_ABORT();
      }
    };

    if (SgClassDeclaration *class_decl =
            isSgClassDeclaration(lookupSgDeclarationForClangDecl(
                tag_decl, /*allow_on_demand=*/true))) {
      SgClassDeclaration *defining_declaration =
          isSgClassDeclaration(class_decl->get_definingDeclaration());
      publish_spelled_tag(class_decl, defining_declaration);
      if (source_declarator_scope == nullptr) {
        publish_spelled_tag(
            isSgClassDeclaration(class_decl->get_firstNondefiningDeclaration()),
            defining_declaration);
        publish_spelled_tag(defining_declaration, defining_declaration);
      }
      return true;
    }

    if (SgEnumDeclaration *enum_decl =
            isSgEnumDeclaration(lookupSgDeclarationForClangDecl(
                tag_decl, /*allow_on_demand=*/true))) {
      SgEnumDeclaration *defining_declaration =
          isSgEnumDeclaration(enum_decl->get_definingDeclaration());
      publish_spelled_tag(enum_decl, defining_declaration);
      if (source_declarator_scope == nullptr) {
        publish_spelled_tag(
            isSgEnumDeclaration(enum_decl->get_firstNondefiningDeclaration()),
            defining_declaration);
        publish_spelled_tag(defining_declaration, defining_declaration);
      }
      return true;
    }

    return false;
  };

  if (auto qualified_loc = type_loc.getAs<clang::QualifiedTypeLoc>()) {
    return finalize_spelled_type(requireExactType(
        buildTypeFromTypeLoc(qualified_loc.getUnqualifiedLoc(),
                             member_function_class),
        "qualified-inner-typeloc", qualified_loc.getTypePtr()));
  }

  if (auto paren_loc = type_loc.getAs<clang::ParenTypeLoc>()) {
    SgType *inner_type = requireExactType(
        buildTypeFromTypeLoc(paren_loc.getInnerLoc(), member_function_class),
        "parenthesized-inner-typeloc", paren_loc.getTypePtr());
    return apply_local_qualifiers(inner_type, type_loc.getType());
  }

  if (auto array_loc = type_loc.getAs<clang::ArrayTypeLoc>()) {
    if (clang::Expr *bound = array_loc.getSizeExpr()) {
      clang::Expr *converted_bound = bound;
      if (const auto *dependent =
              llvm::dyn_cast<clang::DependentSizedArrayType>(
                  array_loc.getTypePtr())) {
        converted_bound = dependent->getSizeExpr();
      } else if (const auto *variable =
                     llvm::dyn_cast<clang::VariableArrayType>(
                         array_loc.getTypePtr())) {
        converted_bound = variable->getSizeExpr();
      }
      ROSE_ASSERT(converted_bound != nullptr &&
                  converted_bound->IgnoreParenImpCasts() ==
                      bound->IgnoreParenImpCasts());
      bound = converted_bound;
      SgType *element_type =
          requireExactType(buildTypeFromTypeLoc(array_loc.getElementLoc()),
                           "array-element-typeloc", array_loc.getTypePtr());
      SgExpression *index =
          prepareExpressionForAttachment(isSgExpression(Traverse(bound)));
      requireExactType(index->get_type(), "array-bound-typeloc",
                       array_loc.getTypePtr());
      if (auto *constant = llvm::dyn_cast<clang::ConstantArrayType>(
              array_loc.getTypePtr())) {
        SgValueExp *extent = buildConstantArrayBound(constant);
        extent->set_originalExpressionTree(index);
        index->set_parent(extent);
        index = extent;
      }
      SgArrayType *array = SageBuilder::buildArrayType(element_type, index);
      array->set_is_variable_length_array(
          array_loc.getTypePtr()->isVariableArrayType());
      SgType *written_array = array;
      if (array_loc.getTypePtr()->getIndexTypeQualifiers().hasRestrict())
        written_array = SageBuilder::buildRestrictType(written_array);
      return finalize_spelled_type(written_array);
    }
  }

  if (clang::AutoTypeLoc auto_loc = type_loc.getAs<clang::AutoTypeLoc>()) {
    SgNonrealRefExp *constraint = nullptr;
    if (auto_loc.isConstrained()) {
      ROSE_ASSERT(auto_loc.getConceptReference() != nullptr);
      constraint = buildConceptReference(*auto_loc.getConceptReference());
    }
    SgAutoType *source_auto =
        new SgAutoType(auto_loc.isConstrained(), constraint);
    return finalize_spelled_type(source_auto);
  }

  if (auto decltype_loc = type_loc.getAs<clang::DecltypeTypeLoc>()) {
    const clang::DecltypeType *decltype_type = decltype_loc.getTypePtr();
    if (decltype_type != nullptr) {
      const clang::Expr *underlying_expr = decltype_type->getUnderlyingExpr();
      if (underlying_expr == nullptr)
        failExactTypeTranslation("source-decltype-expression", decltype_type);
      const bool is_gnu_decltype =
          decltypeTypeLocUsesGNUKeyword(decltype_loc, *p_compiler_instance);
      SgType *sg_underlying_type = nullptr;
      clang::QualType underlying_type = decltype_type->getUnderlyingType();
      if (!underlying_type.isNull()) {
        sg_underlying_type = buildTypeFromQualifiedType(underlying_type);
      }

      SgExpression *expr =
          isSgExpression(Traverse(const_cast<clang::Expr *>(underlying_expr)));
      if (expr == nullptr)
        failExactTypeTranslation("source-decltype-expression", decltype_type);
      SgExpression *expr_copy = prepareExpressionForAttachment(expr);
      SgType *exact_semantic_base = requireExactDecltypeSemanticBase(
          decltype_type, expr_copy, sg_underlying_type,
          "source-dependent-decltype-base");
      SgType *decl_type = SageBuilder::buildDeclType(
          expr_copy, exact_semantic_base, is_gnu_decltype);
      decl_type = apply_local_qualifiers(decl_type, type_loc.getType());
      return decl_type;
    } else {
      failExactTypeTranslation("source-decltype-type", nullptr);
    }
  }

  if (clang::AdjustedTypeLoc adjusted_loc =
          type_loc.getAs<clang::AdjustedTypeLoc>()) {
    clang::TypeLoc original_loc = adjusted_loc.getOriginalLoc();
    if (original_loc.isNull()) {
      std::cerr << "REX_CFE_TYPE_INVARIANT[adjusted-source-type]: adjusted "
                   "TypeLoc has no exact original source TypeLoc\n";
      ROSE_ABORT();
    }
    (void)classify_tag_decl_spelled_in_type_loc(original_loc);
    SgType *original_type =
        buildTypeFromTypeLoc(original_loc, member_function_class);
    if (original_type == nullptr) {
      failExactTypeTranslation("adjusted-source-typeloc",
                               adjusted_loc.getTypePtr());
    }
    return finalize_spelled_type(original_type);
  }

  if (auto pointer_loc = type_loc.getAs<clang::PointerTypeLoc>()) {
    clang::TypeLoc pointee_loc = pointer_loc.getPointeeLoc();
    SgType *pointee_type =
        requireExactType(buildTypeFromTypeLoc(pointee_loc),
                         "pointer-pointee-typeloc", pointer_loc.getTypePtr());
    pointee_type = apply_exact_pointee_qualifiers(
        pointee_type, pointee_loc.getType(),
        pointer_loc.getTypePtr()->getPointeeType());
    return apply_local_qualifiers(pointee_type->has_source_prototype()
                                      ? new SgPointerType(pointee_type)
                                      : SgPointerType::createType(pointee_type),
                                  type_loc.getType());
  }

  if (auto lvalue_ref_loc = type_loc.getAs<clang::LValueReferenceTypeLoc>()) {
    clang::TypeLoc pointee_loc = lvalue_ref_loc.getPointeeLoc();
    SgType *pointee_type = requireExactType(buildTypeFromTypeLoc(pointee_loc),
                                            "lvalue-reference-pointee-typeloc",
                                            lvalue_ref_loc.getTypePtr());
    pointee_type = apply_exact_pointee_qualifiers(
        pointee_type, pointee_loc.getType(),
        lvalue_ref_loc.getTypePtr()->getPointeeType());
    return apply_local_qualifiers(
        pointee_type->has_source_prototype()
            ? new SgReferenceType(pointee_type)
            : SgReferenceType::createType(pointee_type),
        type_loc.getType());
  }

  if (auto rvalue_ref_loc = type_loc.getAs<clang::RValueReferenceTypeLoc>()) {
    clang::TypeLoc pointee_loc = rvalue_ref_loc.getPointeeLoc();
    SgType *pointee_type = requireExactType(buildTypeFromTypeLoc(pointee_loc),
                                            "rvalue-reference-pointee-typeloc",
                                            rvalue_ref_loc.getTypePtr());
    pointee_type = apply_exact_pointee_qualifiers(
        pointee_type, pointee_loc.getType(),
        rvalue_ref_loc.getTypePtr()->getPointeeType());
    // The initialized-name declarator owns the ellipsis after `&&`.
    if (SgTemplateType *parameter = isSgTemplateType(pointee_type);
        parameter != nullptr && parameter->get_packed()) {
      fprintf(stderr, "REX_FRONTEND_INVARIANT[rvalue-reference-type-surface]: "
                      "exact pointee retains declaration-owned pack syntax\n");
      ROSE_ABORT();
    }
    return apply_local_qualifiers(
        pointee_type->has_source_prototype()
            ? new SgRvalueReferenceType(pointee_type)
            : SgRvalueReferenceType::createType(pointee_type),
        type_loc.getType());
  }

  if (auto function_no_proto_loc =
          type_loc.getAs<clang::FunctionNoProtoTypeLoc>()) {
    const clang::FunctionNoProtoType *function_no_proto_type =
        function_no_proto_loc.getTypePtr();
    auto param_type_list = std::make_unique<SgFunctionParameterTypeList>();
    (void)classify_tag_decl_spelled_in_type_loc(
        function_no_proto_loc.getReturnLoc());
    SgType *ret_type = requireExactType(
        buildTypeFromTypeLoc(function_no_proto_loc.getReturnLoc()),
        "function-no-prototype-return",
        function_no_proto_type->getReturnType().getTypePtrOrNull());

    auto *function_type =
        SageBuilder::buildFunctionType(ret_type, param_type_list.get(), false);
    if (function_type->get_argument_list() == param_type_list.get())
      param_type_list.release();
    else
      ROSE_ASSERT(param_type_list->get_parent() == nullptr);
    return apply_local_qualifiers(function_type, type_loc.getType());
  }

  if (auto function_proto_loc = type_loc.getAs<clang::FunctionProtoTypeLoc>()) {
    const clang::FunctionProtoType *function_proto_type =
        function_proto_loc.getTypePtr();
    auto param_type_list = std::make_unique<SgFunctionParameterTypeList>();
    auto *parameter_scope = new SgFunctionParameterScope();
    setSynthesizedFileInfo(parameter_scope);
    auto *parameters = SageBuilder::buildFunctionParameterList_nfi();
    applySourceRange(parameters, function_proto_loc.getParensRange());
    param_type_list->initialize_source_prototype(parameter_scope, parameters,
                                                 resolve_scope());
    p_function_type_parameter_construction_stack.push_back(
        {parameter_scope, function_proto_type, {}});
    struct PrototypeParametersGuard {
      std::vector<FunctionTypeParameterConstructionFrame> &frames;
      SgFunctionParameterScope *scope;
      ~PrototypeParametersGuard() {
        ROSE_ASSERT(!frames.empty() && frames.back().scope == scope);
        frames.pop_back();
      }
    } parameter_guard{p_function_type_parameter_construction_stack,
                      parameter_scope};

    for (unsigned i = 0; i < function_proto_type->getNumParams(); ++i) {
      SgType *param_type = nullptr;
      bool source_qualification_present = false;
      bool source_global_qualification = false;
      SgStringList source_qualification_tokens;
      bool source_elaboration_required = false;
      const clang::QualType semantic_parameter_type =
          function_proto_type->getParamType(i);
      bool is_pack_expansion =
          semantic_parameter_type->getAs<clang::PackExpansionType>() != nullptr;
      clang::ParmVarDecl *parameter = function_proto_loc.getParam(i);
      if (parameter == nullptr || parameter->getTypeSourceInfo() == nullptr)
        failExactTypeTranslation("function-prototype-parameter-source",
                                 function_proto_type);
      clang::TypeLoc parameter_loc =
          parameter->getTypeSourceInfo()->getTypeLoc();
      if (parameter_loc.isNull())
        failExactTypeTranslation("function-prototype-parameter-source",
                                 function_proto_type);
      (void)classify_tag_decl_spelled_in_type_loc(parameter_loc);
      bool source_pack_expansion = false;
      for (clang::TypeLoc current = parameter_loc; !current.isNull();
           current = current.getNextTypeLoc()) {
        if (current.getAs<clang::PackExpansionTypeLoc>()) {
          if (source_pack_expansion) {
            fprintf(stderr,
                    "REX_FRONTEND_INVARIANT[function-argument-pack-"
                    "surface]: parameter %u has more than one pack "
                    "expansion owner\n",
                    i);
            ROSE_ABORT();
          }
          source_pack_expansion = true;
        }
      }
      if (parameter->isParameterPack() != source_pack_expansion ||
          is_pack_expansion != source_pack_expansion) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[function-argument-pack-surface]: "
                "parameter %u declaration-pack=%d semantic-pack=%d "
                "source-pack=%d\n",
                i, parameter->isParameterPack() ? 1 : 0,
                is_pack_expansion ? 1 : 0, source_pack_expansion ? 1 : 0);
        ROSE_ABORT();
      }
      param_type = buildTypeFromTypeLocWithSemanticOwner(
          parameter_loc, parameter_scope,
          "function-prototype-parameter-source");
      const bool source_type_owns_qualification =
          SageInterface::typeCarriesWrittenNonrealQualification(param_type);
      if (!source_type_owns_qualification) {
        source_qualification_present = true;
        clang::NestedNameSpecifierLoc qualifier_loc =
            typeLocQualifierLoc(parameter_loc);
        if (qualifier_loc) {
          SourceQualification source_qualification =
              sourceQualificationFromNestedNameSpecifierLoc(
                  qualifier_loc, p_compiler_instance,
                  "function-parameter-type");
          source_global_qualification = source_qualification.global;
          source_qualification_tokens = source_qualification.tokens;
        }
      }
      source_elaboration_required = typeLocSpellsElaboratedKeyword(
          parameter_loc, parameter_loc.getSourceRange(), p_compiler_instance);
      param_type = requireExactType(param_type, "function-prototype-parameter",
                                    semantic_parameter_type.getTypePtrOrNull());
      param_type_list->append_argument(param_type);
      SgType *object_type =
          requireExactType(buildTypeFromQualifiedType(parameter->getType()),
                           "function-prototype-parameter-object",
                           parameter->getType().getTypePtr());
      auto *name = SageBuilder::buildInitializedName_nfi(
          parameter->getNameAsString(), object_type, nullptr);
      name->set_scope(parameter_scope);
      name->set_is_parameter_pack(parameter->isParameterPack());
      parameters->append_arg(name);
      applySourceRange(name, parameter->getSourceRange());
      if (!name->get_name().getString().empty())
        attachSymbolToExactScope(new SgVariableSymbol(name), parameter_scope);
      const bool inserted = p_function_type_parameter_construction_stack.back()
                                .parameters.emplace(parameter, name)
                                .second;
      ROSE_ASSERT(inserted);
      SgFunctionTypeArgument *argument_position =
          param_type_list->get_argument_qualification_use_sites().back();
      ASSERT_not_null(argument_position);
      argument_position->set_is_pack_expansion(is_pack_expansion);
      argument_position->set_source_type_qualification_present(
          source_qualification_present);
      argument_position->set_source_type_global_qualification(
          source_global_qualification);
      argument_position->get_source_type_qualification_tokens() =
          std::move(source_qualification_tokens);
      argument_position->set_source_type_elaboration_required(
          source_elaboration_required);
      if (!source_qualification_present &&
          !SageInterface::typeCarriesWrittenNonrealQualification(param_type)) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[function-parameter-source-surface]: "
                "parameter %u has TypeLoc syntax but no exact qualification "
                "owner\n",
                i);
        ROSE_ABORT();
      }
    }

    if (function_proto_type->isVariadic()) {
      param_type_list->append_argument(SgTypeEllipse::createType());
    }

    (void)classify_tag_decl_spelled_in_type_loc(
        function_proto_loc.getReturnLoc());
    SgType *ret_type = requireExactType(
        buildTypeFromTypeLocWithSemanticOwner(
            function_proto_loc.getReturnLoc(), parameter_scope,
            "function-prototype-return-source"),
        "function-prototype-return",
        function_proto_type->getReturnType().getTypePtrOrNull());

    SgFunctionType *function_type = buildFunctionTypeForClangProto(
        ret_type, std::move(param_type_list), function_proto_type,
        member_function_class, &function_proto_loc);
    function_type->set_source_trailing_return(
        function_proto_type->hasTrailingReturn());

    SgType *qualified_function_type =
        apply_local_qualifiers(function_type, type_loc.getType());
    return qualified_function_type;
  }

  if (auto type_of_type_loc = type_loc.getAs<clang::TypeOfTypeLoc>()) {
    SgType *underlying_type = nullptr;
    if (clang::TypeSourceInfo *unmodified_tinfo =
            type_of_type_loc.getUnmodifiedTInfo()) {
      underlying_type = buildTypeFromTypeLoc(unmodified_tinfo->getTypeLoc());
    } else {
      underlying_type =
          buildTypeFromQualifiedType(type_of_type_loc.getUnmodifiedType());
    }
    underlying_type = requireExactType(underlying_type, "typeof-type-operand",
                                       type_of_type_loc.getTypePtr());
    return finalize_spelled_type(
        SageBuilder::buildTypeOfType(nullptr, underlying_type));
  }

  auto exact_written_template_identity =
      [&](const clang::TemplateSpecializationType *specialization,
          clang::Decl *target) -> SgDeclarationStatement * {
    SgDeclarationStatement *identity =
        target != nullptr ? resolveNonrealTemplateDeclarationTarget(
                                target, "written-template-id")
                          : nullptr;
    if (specialization == nullptr || !specialization->isTypeAlias() ||
        specialization->isInstantiationDependentType() ||
        llvm::isa_and_nonnull<clang::BuiltinTemplateDecl>(target))
      return identity;
    llvm::SaveAndRestore<unsigned> semantic_type_use(
        p_explicit_template_id_type_use_depth, 0);
    SemanticExpressionConstruction semantic_arguments(
        p_semantic_template_argument_expression_depth,
        "written-alias-template-identity");
    auto *alias_type = isSgTypedefType(
        buildTypeFromQualifiedType(clang::QualType(specialization, 0)));
    auto *instantiation = isSgTemplateInstantiationTypedefDeclaration(
        alias_type != nullptr ? alias_type->get_declaration() : nullptr);
    ROSE_ASSERT(instantiation != nullptr &&
                instantiation->get_templateDeclaration() == identity);
    return instantiation;
  };

  auto build_nonreal_template_type =
      [&](const std::string &base_name, SgScopeStatement *scope,
          clang::NestedNameSpecifier qualifier, bool has_template_keyword,
          SgTemplateArgumentPtrList &tpl_args, clang::Decl *template_decl,
          SgDeclarationStatement *dependent_template_identity = nullptr,
          const clang::TemplateSpecializationType *specialization =
              nullptr) -> SgType * {
    ROSE_ASSERT(!base_name.empty());
    SgScopeStatement *effective_scope =
        scope != nullptr ? scope : resolve_scope();
    clang::NestedNameSpecifier effective_qualifier = qualifier;
    clang::DeclContext *template_context =
        template_decl != nullptr ? template_decl->getDeclContext() : nullptr;
    if (!effective_qualifier && template_decl != nullptr &&
        canSynthesizeNamespaceQualifierFromDeclContext(template_context)) {
      if (!scopeIsWithinNamespaceChain(effective_scope, template_context)) {
        effective_qualifier = buildNamespaceQualifierForDeclContext(
            template_context, p_compiler_instance->getASTContext());
      }
    }

    SgDeclarationStatement *template_identity =
        template_decl != nullptr
            ? exact_written_template_identity(specialization, template_decl)
            : dependent_template_identity;
    ROSE_ASSERT(template_decl == nullptr ||
                dependent_template_identity == nullptr);

    SgType *aliased_type = buildDependentAliasUnderlyingType(specialization);
    SgNonrealType *nrtype = nullptr;
    if (effective_qualifier) {
      nrtype = buildSemanticNonrealTypeFromNestedNameSpecifier(
          effective_qualifier, effective_scope, SgName(base_name), &tpl_args,
          template_identity, aliased_type);
    } else {
      nrtype = SageBuilder::buildSemanticNonrealType(
          SgName(base_name), effective_scope, &tpl_args, template_identity,
          aliased_type);
    }
    ROSE_ASSERT(nrtype != nullptr);
    SgNonrealDecl *nrdecl = isSgNonrealDecl(nrtype->get_declaration());
    ROSE_ASSERT(nrdecl != nullptr);
    if (nrdecl->get_source_name_qualification_present() ||
        nrdecl->get_source_name_global_qualification() ||
        !nrdecl->get_source_name_qualification_tokens().empty()) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[written-template-qualification]: "
              "shared semantic template-id=%s owns use-site qualifier "
              "state\n",
              base_name.c_str());
      ROSE_ABORT();
    }
    if (has_template_keyword) {
      nrdecl->set_has_template_keyword(true);
    }
    if ((template_identity != nullptr || !effective_qualifier) &&
        nrdecl->get_templateDeclaration() != template_identity) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[written-template-link]: "
              "template-id=%s lost its builder-supplied declaration\n",
              base_name.c_str());
      ROSE_ABORT();
    }
    // A specialization of a Clang builtin template is a semantic type
    // identity, not a declaration written at the use site.  Classify that
    // exact producer result immediately; otherwise the generic nonreal-type
    // builder's physical-output publication makes the intrinsic appear to
    // be a second source declaration beside the builtin canonical.
    if (llvm::isa_and_nonnull<clang::BuiltinTemplateDecl>(template_decl)) {
      mark_compiler_generated_frontend_specific(nrdecl);
    }
    return nrtype;
  };
  auto class_template_decl_for_record =
      [](clang::CXXRecordDecl *record) -> clang::TemplateDecl * {
    if (record == nullptr) {
      return nullptr;
    }

    if (auto *partial =
            llvm::dyn_cast<clang::ClassTemplatePartialSpecializationDecl>(
                record)) {
      return const_cast<clang::ClassTemplateDecl *>(
          partial->getSpecializedTemplate());
    }

    if (auto *specialization =
            llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(record)) {
      return const_cast<clang::ClassTemplateDecl *>(
          specialization->getSpecializedTemplate());
    }

    return const_cast<clang::ClassTemplateDecl *>(
        record->getDescribedClassTemplate());
  };
  std::function<SgNonrealType *(clang::TypeLoc, SgScopeStatement *, bool)>
      build_nonreal_type_for_nested_name_specifier_typeloc;
  std::function<SgNonrealType *(
      clang::NestedNameSpecifierLoc, SgScopeStatement *, const SgName &,
      const SgTemplateArgumentPtrList *, SgDeclarationStatement *,
      const clang::TemplateSpecializationType *)>
      build_nonreal_type_from_nested_name_specifier_loc;

  build_nonreal_type_for_nested_name_specifier_typeloc =
      [&](clang::TypeLoc nested_type_loc, SgScopeStatement *scope,
          bool prefer_current_scope) -> SgNonrealType * {
    if (nested_type_loc.isNull()) {
      return nullptr;
    }

    if (auto qualified_loc = nested_type_loc.getAs<clang::QualifiedTypeLoc>()) {
      return build_nonreal_type_for_nested_name_specifier_typeloc(
          qualified_loc.getUnqualifiedLoc(), scope, prefer_current_scope);
    }

    if (auto paren_loc = nested_type_loc.getAs<clang::ParenTypeLoc>()) {
      return build_nonreal_type_for_nested_name_specifier_typeloc(
          paren_loc.getInnerLoc(), scope, prefer_current_scope);
    }

    if (auto spec_loc =
            nested_type_loc.getAs<clang::TemplateSpecializationTypeLoc>()) {

      if (const clang::TemplateSpecializationType *tst =
              static_cast<const clang::TemplateSpecializationType *>(
                  spec_loc.getTypePtr())) {
        clang::TemplateName tname = tst->getTemplateName();
        std::string base_name =
            getTemplateNameBase(tname, p_compiler_instance->getASTContext());
        if (!base_name.empty()) {
          clang::Decl *translated_decl_key = nullptr;
          if (!tst->isDependentType() && !tst->isTypeAlias()) {
            if (const clang::CXXRecordDecl *record_decl =
                    cxxRecordDeclFromQualTypeWithoutDefinitionLookup(
                        clang::QualType(tst, 0))) {
              translated_decl_key =
                  const_cast<clang::CXXRecordDecl *>(record_decl);
            }
          }
          if (translated_decl_key == nullptr) {
            translated_decl_key = llvm::dyn_cast_or_null<clang::Decl>(
                resolveTemplateNameDeclaration(tname));
          }
          SgTemplateArgumentPtrList tpl_args;
          append_nonreal_template_arguments(
              tpl_args, spec_loc, "nested-name-specifier-template-id");

          clang::NestedNameSpecifier qualifier = std::nullopt;
          if (!prefer_current_scope) {
            if (clang::NestedNameSpecifierLoc prefix_loc =
                    spec_loc.getPrefix()) {
              qualifier = prefix_loc.getNestedNameSpecifier();
            }
            if (!qualifier) {
              if (const clang::QualifiedTemplateName *qtn =
                      tname.getAsQualifiedTemplateName()) {
                qualifier = qtn->getQualifier();
              } else if (const clang::DependentTemplateName *dtn =
                             tname.getAsDependentTemplateName()) {
                qualifier = dtn->getQualifier();
              }
            }
          }

          const bool has_template_keyword =
              spec_loc.getTemplateKeywordLoc().isValid();
          SgDeclarationStatement *dependent_template_identity = nullptr;
          if (prefer_current_scope && translated_decl_key == nullptr) {
            const clang::DependentTemplateName *dependent_name =
                tname.getAsDependentTemplateName();
            if (dependent_name == nullptr ||
                !nestedNameSpecifierHasDependentTypeQualifier(
                    dependent_name->getQualifier())) {
              fprintf(stderr,
                      "REX_FRONTEND_INVARIANT[written-template-id-target]: "
                      "template=%s has no exact dependent qualifier\n",
                      base_name.c_str());
              ROSE_ABORT();
            }
            dependent_template_identity = SageBuilder::buildSemanticNonrealType(
                                              SgName(base_name), scope, nullptr)
                                              ->get_declaration();
          }
          SgType *nr = build_nonreal_template_type(
              base_name, scope, qualifier, has_template_keyword, tpl_args,
              translated_decl_key, dependent_template_identity, tst);
          nr = apply_global_qualifier_from_loc(nr, spec_loc.getPrefix());
          return isSgNonrealType(nr);
        }
      }
    }

    if (!prefer_current_scope) {
      if (clang::NestedNameSpecifierLoc prefix_loc =
              nested_type_loc.getPrefix()) {
        auto build_from_named_decl =
            [&](const clang::NamedDecl *decl) -> SgNonrealType * {
          if (decl == nullptr) {
            return nullptr;
          }
          std::string name_str = decl->getNameAsString();
          if (name_str.empty()) {
            return nullptr;
          }
          SgDeclarationStatement *identity =
              resolveNonrealTemplateDeclarationTarget(
                  const_cast<clang::NamedDecl *>(decl),
                  "nested-type-location-target");
          return build_nonreal_type_from_nested_name_specifier_loc(
              prefix_loc, scope, SgName(name_str), nullptr, identity, nullptr);
        };

        if (auto tag_loc = nested_type_loc.getAs<clang::TagTypeLoc>()) {
          if (SgNonrealType *nr = build_from_named_decl(tag_loc.getDecl())) {
            return nr;
          }
        }
        if (auto typedef_loc = nested_type_loc.getAs<clang::TypedefTypeLoc>()) {
          const clang::TypedefType *typedef_type =
              llvm::dyn_cast_or_null<clang::TypedefType>(
                  typedef_loc.getTypePtr());
          if (SgNonrealType *nr = build_from_named_decl(
                  typedef_type != nullptr ? typedef_type->getDecl()
                                          : nullptr)) {
            return nr;
          }
        }
        if (auto using_loc = nested_type_loc.getAs<clang::UsingTypeLoc>()) {
          const clang::UsingType *using_type =
              llvm::dyn_cast_or_null<clang::UsingType>(using_loc.getTypePtr());
          if (SgNonrealType *nr = build_from_named_decl(
                  using_type != nullptr ? using_type->getDecl() : nullptr)) {
            return nr;
          }
        }
        if (auto injected_loc =
                nested_type_loc.getAs<clang::InjectedClassNameTypeLoc>()) {
          if (SgNonrealType *nr =
                  build_from_named_decl(injected_loc.getDecl())) {
            return nr;
          }
        }
      }
    }

    SgNonrealType *nrtype = buildNonrealTypeForNestedNameSpecifierType(
        nested_type_loc.getTypePtr(), scope, prefer_current_scope);
    if (nrtype != nullptr) {
      nrtype = isSgNonrealType(
          apply_global_qualifier_from_loc(nrtype, nested_type_loc.getPrefix()));
      if (SgNonrealDecl *nrdecl = isSgNonrealDecl(nrtype->get_declaration())) {
        if (nested_type_loc.getTemplateKeywordLoc().isValid()) {
          nrdecl->set_has_template_keyword(true);
        }
      }
    }
    return nrtype;
  };

  build_nonreal_type_from_nested_name_specifier_loc =
      [&](clang::NestedNameSpecifierLoc qualifier_loc, SgScopeStatement *scope,
          const SgName &terminal_name,
          const SgTemplateArgumentPtrList *terminal_template_args,
          SgDeclarationStatement *terminal_declaration,
          const clang::TemplateSpecializationType *terminal_specialization)
      -> SgNonrealType * {
    SgType *aliased_type =
        buildDependentAliasUnderlyingType(terminal_specialization);
    if (!qualifier_loc) {
      return SageBuilder::buildSemanticNonrealType(
          terminal_name, scope, terminal_template_args, terminal_declaration,
          aliased_type);
    }

    SgScopeStatement *lexical_scope = scope;
    if (lexical_scope == nullptr) {
      lexical_scope = SageBuilder::topScopeStack();
    }
    SgScopeStatement *effective_scope = lexical_scope;
    if (nestedNameSpecifierLocHasExplicitGlobal(qualifier_loc) ||
        nestedNameSpecifierHasNamespaceQualifier(
            qualifier_loc.getNestedNameSpecifier())) {
      effective_scope = getGlobalScope();
    }
    if (lexical_scope == nullptr || effective_scope == nullptr) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[qualified-template-id-lexical-owner]: "
              "terminal=%s has no exact lexical or semantic qualifier scope\n",
              terminal_name.getString().c_str());
      ROSE_ABORT();
    }

    std::function<SgScopeStatement *(clang::NestedNameSpecifierLoc,
                                     SgScopeStatement *)>
        build_chain;
    build_chain = [&](clang::NestedNameSpecifierLoc current_loc,
                      SgScopeStatement *current_scope) -> SgScopeStatement * {
      if (!current_loc) {
        return current_scope;
      }

      current_scope = build_chain(nested_name_specifier_loc_prefix(current_loc),
                                  current_scope);

      clang::NestedNameSpecifier current_nns =
          current_loc.getNestedNameSpecifier();
      SgNonrealType *segment_type = nullptr;
      switch (current_nns.getKind()) {
      case clang::NestedNameSpecifier::Kind::Namespace: {
        const clang::NamespaceBaseDecl *ns =
            llvm::dyn_cast_or_null<clang::NamespaceBaseDecl>(
                nestedNameSpecifierNamespaceBase(current_nns));
        ROSE_ASSERT(ns != nullptr);
        std::string name_str = ns->getNameAsString();
        ROSE_ASSERT(!name_str.empty());
        segment_type = SageBuilder::buildSemanticNonrealType(
            SgName(name_str), current_scope, nullptr);
        break;
      }
      case clang::NestedNameSpecifier::Kind::Type: {
        bool prefer_current =
            static_cast<bool>(nested_name_specifier_loc_prefix(current_loc));
        segment_type = build_nonreal_type_for_nested_name_specifier_typeloc(
            current_loc.getAsTypeLoc(), current_scope, prefer_current);
        break;
      }
      case clang::NestedNameSpecifier::Kind::Global:
        break;
      case clang::NestedNameSpecifier::Kind::MicrosoftSuper:
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[nonreal-qualifier]: __super has no "
                "typed nonreal qualifier representation\n");
        ROSE_ABORT();
      case clang::NestedNameSpecifier::Kind::Null:
        break;
      }

      if (segment_type != nullptr) {
        SgNonrealDecl *segment_decl =
            isSgNonrealDecl(segment_type->get_declaration());
        ROSE_ASSERT(segment_decl != nullptr);
        if (clang::NestedNameSpecifierLoc prefix_loc =
                nested_name_specifier_loc_prefix(current_loc)) {

          clang::NestedNameSpecifier prefix =
              prefix_loc.getNestedNameSpecifier();
          if (prefix.getKind() == clang::NestedNameSpecifier::Kind::Global &&
              prefix_loc.getLocalBeginLoc().isValid()) {
            segment_decl->set_has_global_qualifier(true);
          }
        }
        current_scope = segment_decl->get_nonreal_decl_scope();
      }

      return current_scope;
    };

    SgScopeStatement *chain_scope = build_chain(qualifier_loc, effective_scope);
    ROSE_ASSERT(chain_scope != nullptr);
    if (terminal_template_args != nullptr && terminal_declaration == nullptr) {
      if (!nestedNameSpecifierHasDependentTypeQualifier(
              qualifier_loc.getNestedNameSpecifier())) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[qualified-template-id-target]: "
                "terminal=%s has no exact resolved template declaration\n",
                terminal_name.getString().c_str());
        ROSE_ABORT();
      }
      terminal_declaration = SageBuilder::buildSemanticNonrealType(
                                 terminal_name, chain_scope, nullptr)
                                 ->get_declaration();
    }

    // A qualified template-id's qualifier chain describes semantic lookup, but
    // the template-id remains a source occurrence in the enclosing lexical
    // scope.  When an argument names a local declaration, placing that argument
    // beneath the qualifier's namespace/class declaration scope moves source
    // grammar outside the declaration's lifetime.  Select the lexical owner
    // exactly when required by such an argument and prove that it encloses
    // every referenced local declaration.
    SgScopeStatement *terminal_owner_scope = chain_scope;
    if (terminal_template_args != nullptr) {
      for (SgTemplateArgument *argument : *terminal_template_args) {
        ASSERT_not_null(argument);
        for (SgNode *node : NodeQuery::querySubTree(argument, V_SgVarRefExp)) {
          SgVarRefExp *reference = isSgVarRefExp(node);
          SgInitializedName *declaration =
              reference != nullptr && reference->get_symbol() != nullptr
                  ? reference->get_symbol()->get_declaration()
                  : nullptr;
          SgScopeStatement *declaration_scope =
              declaration != nullptr ? declaration->get_scope() : nullptr;
          if (declaration_scope == nullptr) {
            fprintf(
                stderr,
                "REX_FRONTEND_INVARIANT[qualified-template-id-lexical-owner]: "
                "terminal=%s has a variable reference without an exact "
                "declaration scope\n",
                terminal_name.getString().c_str());
            ROSE_ABORT();
          }
          if (isSgGlobal(declaration_scope) != nullptr ||
              isSgNamespaceDefinitionStatement(declaration_scope) != nullptr ||
              isSgClassDefinition(declaration_scope) != nullptr) {
            continue;
          }
          if (lexical_scope != declaration_scope &&
              !SageInterface::isAncestor(declaration_scope, lexical_scope)) {
            fprintf(
                stderr,
                "REX_FRONTEND_INVARIANT[qualified-template-id-lexical-owner]: "
                "terminal=%s lexical-owner=%p/%s is outside local declaration "
                "%s scope=%p/%s\n",
                terminal_name.getString().c_str(),
                static_cast<void *>(lexical_scope),
                lexical_scope->class_name().c_str(),
                declaration->get_name().getString().c_str(),
                static_cast<void *>(declaration_scope),
                declaration_scope->class_name().c_str());
            ROSE_ABORT();
          }
          terminal_owner_scope = lexical_scope;
        }
      }
    }

    SgNonrealType *nrtype = SageBuilder::buildSemanticNonrealType(
        terminal_name, terminal_owner_scope, terminal_template_args,
        terminal_declaration, aliased_type);
    ROSE_ASSERT(nrtype != nullptr);

    SgNonrealDecl *nrdecl = isSgNonrealDecl(nrtype->get_declaration());
    ROSE_ASSERT(nrdecl != nullptr);

    if (!nestedNameSpecifierHasDependentTypeQualifier(
            qualifier_loc.getNestedNameSpecifier())) {
      nrdecl->set_suppress_typename(true);
    }
    if (nestedNameSpecifierLocHasExplicitGlobal(qualifier_loc)) {
      nrdecl->set_has_global_qualifier(true);
    }

    const SourceQualification source_qualification =
        sourceQualificationFromNestedNameSpecifierLoc(
            qualifier_loc, p_compiler_instance,
            "source-nonreal-type-qualification");
    clang::NestedNameSpecifier qualifier =
        qualifier_loc.getNestedNameSpecifier();
    if (!sourceQualificationMatchesSemanticIdentity(
            source_qualification, qualifier,
            nestedNameSpecifierLocHasExplicitGlobal(qualifier_loc),
            static_cast<std::size_t>(
                nestedNameSpecifierComponentCount(qualifier)))) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[source-nonreal-type-qualification]: "
              "terminal=%s structural qualifier disagrees with Clang "
              "identity\n",
              terminal_name.getString().c_str());
      ROSE_ABORT();
    }
    // SgNonrealDecl is a shared semantic identity. Exact spelling belongs to
    // the declaration, expression, base, or template-argument TypeLoc use
    // site; caching it here makes one occurrence poison every later occurrence
    // of the same type (for example `std::size_t` followed by `size_t(...)`).
    if (nrdecl->get_source_name_qualification_present() ||
        nrdecl->get_source_name_global_qualification() ||
        !nrdecl->get_source_name_qualification_tokens().empty()) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[source-nonreal-type-qualification]: "
              "terminal=%s shared semantic identity owns use-site qualifier "
              "state\n",
              terminal_name.getString().c_str());
      ROSE_ABORT();
    }

    return nrtype;
  };

  if (auto member_pointer_loc = type_loc.getAs<clang::MemberPointerTypeLoc>()) {
    const clang::MemberPointerType *member_pointer_type =
        member_pointer_loc.getTypePtr();
    SgType *class_type = nullptr;
    bool source_class_type_is_unqualified_injected_name = false;
    if (clang::NestedNameSpecifierLoc qualifier_loc =
            member_pointer_loc.getQualifierLoc()) {
      auto build_source_spelled_member_pointer_class_type =
          [&](clang::TypeLoc qualifier_type_loc) -> SgType * {
        if (qualifier_type_loc.isNull()) {
          return nullptr;
        }

        if (qualifier_type_loc.getAs<clang::TemplateTypeParmTypeLoc>()) {
          return requireExactType(buildTypeFromTypeLoc(qualifier_type_loc),
                                  "member-pointer-class-parameter",
                                  qualifier_type_loc.getTypePtr());
        }

        if (auto injected_loc =
                qualifier_type_loc.getAs<clang::InjectedClassNameTypeLoc>()) {
          const bool source_spells_qualified_name =
              static_cast<bool>(injected_loc.getQualifierLoc()) ||
              static_cast<bool>(qualifier_type_loc.getPrefix());
          return build_nonreal_type_for_nested_name_specifier_typeloc(
              qualifier_type_loc, resolve_scope(),
              /*prefer_current_scope=*/!source_spells_qualified_name);
        }

        return nullptr;
      };
      auto build_class_type_from_member_pointer_qualifier_loc =
          [&](clang::NestedNameSpecifierLoc current_loc) -> SgType * {
        if (!current_loc) {
          return nullptr;
        }

        clang::NestedNameSpecifier current_nns =
            current_loc.getNestedNameSpecifier();
        if (current_nns.getKind() != clang::NestedNameSpecifier::Kind::Type) {
          return nullptr;
        }

        if (clang::TypeLoc qualifier_type_loc = current_loc.getAsTypeLoc();
            !qualifier_type_loc.isNull()) {
          clang::NestedNameSpecifierLoc prefix_loc =
              nested_name_specifier_loc_prefix(current_loc);
          if (auto spec_loc =
                  qualifier_type_loc
                      .getAs<clang::TemplateSpecializationTypeLoc>()) {
            const clang::TemplateSpecializationType *tst =
                spec_loc.getTypePtr();
            if (tst == nullptr) {
              return nullptr;
            }

            clang::TemplateName tname = tst->getTemplateName();
            std::string base_name = getTemplateNameBase(
                tname, p_compiler_instance->getASTContext());

            SgTemplateArgumentPtrList tpl_args;
            append_nonreal_template_arguments(
                tpl_args, spec_loc, "member-pointer-qualifier-template-id");
            clang::TemplateDecl *target = resolveTemplateNameDeclaration(tname);
            SgDeclarationStatement *identity =
                target != nullptr ? exact_written_template_identity(tst, target)
                                  : nullptr;

            return build_nonreal_type_from_nested_name_specifier_loc(
                prefix_loc, resolve_scope(), SgName(base_name), &tpl_args,
                identity, tst);
          }

          if (!prefix_loc) {
            return nullptr;
          }

          auto build_named_type =
              [&](const clang::NamedDecl *decl) -> SgType * {
            if (decl == nullptr) {
              return nullptr;
            }
            std::string name_str = decl->getNameAsString();
            if (name_str.empty()) {
              return nullptr;
            }
            return build_nonreal_type_from_nested_name_specifier_loc(
                prefix_loc, resolve_scope(), SgName(name_str), nullptr, nullptr,
                nullptr);
          };

          if (auto tag_loc = qualifier_type_loc.getAs<clang::TagTypeLoc>()) {
            if (SgType *type = build_named_type(tag_loc.getDecl())) {
              return type;
            }
          }
          if (auto typedef_loc =
                  qualifier_type_loc.getAs<clang::TypedefTypeLoc>()) {
            if (SgType *type =
                    build_named_type(typedef_loc.getTypePtr()->getDecl())) {
              return type;
            }
          }
          if (auto injected_loc =
                  qualifier_type_loc.getAs<clang::InjectedClassNameTypeLoc>()) {
            if (SgType *type = build_named_type(injected_loc.getDecl())) {
              return type;
            }
          }
        }

        return nullptr;
      };

      clang::TypeLoc direct_qualifier_type_loc = qualifier_loc.getAsTypeLoc();
      if (nestedNameSpecifierLocHasExplicitGlobal(qualifier_loc) ||
          static_cast<bool>(nested_name_specifier_loc_prefix(qualifier_loc)) ||
          (!direct_qualifier_type_loc.isNull() &&
           direct_qualifier_type_loc
               .getAs<clang::TemplateSpecializationTypeLoc>())) {
        class_type =
            build_class_type_from_member_pointer_qualifier_loc(qualifier_loc);
      }
      if (clang::TypeLoc qualifier_type_loc = qualifier_loc.getAsTypeLoc();
          class_type == nullptr && !qualifier_type_loc.isNull()) {
        source_class_type_is_unqualified_injected_name =
            static_cast<bool>(
                qualifier_type_loc.getAs<clang::InjectedClassNameTypeLoc>()) &&
            !static_cast<bool>(qualifier_type_loc.getPrefix()) &&
            !static_cast<bool>(
                qualifier_type_loc.getAs<clang::InjectedClassNameTypeLoc>()
                    .getQualifierLoc());
        class_type =
            build_source_spelled_member_pointer_class_type(qualifier_type_loc);
      }
      if (clang::TypeLoc qualifier_type_loc = qualifier_loc.getAsTypeLoc();
          class_type == nullptr && !qualifier_type_loc.isNull()) {
        class_type = buildTypeFromTypeLoc(qualifier_type_loc);
      }
    }
    class_type = requireExactType(class_type, "member-pointer-class",
                                  member_pointer_type);

    SgType *function_class =
        member_pointer_type->isMemberFunctionPointer()
            ? buildSemanticTypeFromQualifiedType(
                  memberPointerClassType(member_pointer_type),
                  "member-pointer-function-class")
            : nullptr;
    SgType *base_type = buildTypeFromTypeLoc(member_pointer_loc.getPointeeLoc(),
                                             function_class);
    base_type = requireExactType(
        base_type, "member-pointer-pointee",
        member_pointer_type != nullptr
            ? member_pointer_type->getPointeeType().getTypePtrOrNull()
            : nullptr);

    // This node records the exact TypeLoc spelling for one use site. The
    // semantic member-pointer identity is built independently from canonical
    // QualType through SageBuilder; interning this source wrapper would either
    // discard its qualification payload or mutate a shared semantic type.
    SgPointerMemberType *raw_member_pointer_sg_type =
        new SgPointerMemberType(base_type, class_type);
    raw_member_pointer_sg_type
        ->set_source_class_type_is_unqualified_injected_name(
            source_class_type_is_unqualified_injected_name);
    if (SgPointerMemberType::isCanonicalSemanticType(
            raw_member_pointer_sg_type)) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[pointer-member-type-surface]: exact "
              "TypeLoc wrapper was published as a canonical semantic type\n");
      ROSE_ABORT();
    }
    if (member_pointer_type != nullptr &&
        member_pointer_type->isMemberFunctionPointer()) {
      clang::FunctionProtoTypeLoc pointee_function_proto_loc;
      for (clang::TypeLoc current_loc = type_loc; !current_loc.isNull();
           current_loc = current_loc.getNextTypeLoc()) {
        if (auto function_proto_loc =
                current_loc.getAs<clang::FunctionProtoTypeLoc>()) {
          pointee_function_proto_loc = function_proto_loc;
          break;
        }
      }
      if (!pointee_function_proto_loc.isNull()) {
        if (clang::NestedNameSpecifierLoc return_qualifier_loc =
                typeLocQualifierLoc(
                    pointee_function_proto_loc.getReturnLoc())) {
          const SourceQualification source_qualification =
              sourceQualificationFromNestedNameSpecifierLoc(
                  return_qualifier_loc, p_compiler_instance,
                  "pointer-member-function-return-type");
          clang::NestedNameSpecifier qualifier =
              return_qualifier_loc.getNestedNameSpecifier();
          const int qualifier_length =
              nestedNameSpecifierComponentCount(qualifier);
          const bool has_global_qualifier =
              nestedNameSpecifierLocHasExplicitGlobal(return_qualifier_loc);
          if (!sourceQualificationMatchesSemanticIdentity(
                  source_qualification, qualifier, has_global_qualifier,
                  static_cast<std::size_t>(qualifier_length))) {
            fprintf(stderr,
                    "REX_FRONTEND_INVARIANT[source-base-type-"
                    "qualification]: pointer-member-type=%p structural "
                    "return qualifier disagrees with Clang identity\n",
                    static_cast<void *>(raw_member_pointer_sg_type));
            ROSE_ABORT();
          }
          if (!sourceQualificationIsSemanticMacroFragment(
                  source_qualification)) {
            if (raw_member_pointer_sg_type
                    ->get_source_base_type_qualification_present()) {
              if (raw_member_pointer_sg_type
                          ->get_source_base_type_global_qualification() !=
                      source_qualification.global ||
                  raw_member_pointer_sg_type
                          ->get_source_base_type_qualification_tokens() !=
                      source_qualification.tokens) {
                fprintf(stderr,
                        "REX_FRONTEND_INVARIANT[source-base-type-"
                        "qualification]: pointer-member-type=%p has "
                        "conflicting structural return qualifiers\n",
                        static_cast<void *>(raw_member_pointer_sg_type));
                ROSE_ABORT();
              }
            } else {
              raw_member_pointer_sg_type
                  ->set_source_base_type_global_qualification(
                      source_qualification.global);
              raw_member_pointer_sg_type
                  ->get_source_base_type_qualification_tokens() =
                  source_qualification.tokens;
              raw_member_pointer_sg_type
                  ->set_source_base_type_qualification_present(true);
            }
          }
        }
      }
    }
    SgType *member_pointer_sg_type = raw_member_pointer_sg_type;
    member_pointer_sg_type =
        apply_local_qualifiers(member_pointer_sg_type, type_loc.getType());
    return member_pointer_sg_type;
  }

  auto template_specialization_type_loc_requires_written_nonreal =
      [&](clang::TemplateSpecializationTypeLoc spec_loc) -> bool {
    if (spec_loc.isNull()) {
      return false;
    }
    // A canonical SgTemplateInstantiationDecl owns semantic template
    // arguments, not the spelling of every TypeLoc that resolves to it.  Any
    // explicitly delimited template-id therefore requires a per-use
    // SgNonrealType surface, even when its written argument happens to be
    // pointer-identical to the canonical type.  Otherwise the first use site
    // mutates the shared declaration and a later equivalent spelling (for
    // example a qualified typedef versus `unsigned`) conflicts with it.
    if (spec_loc.getLAngleLoc().isValid() &&
        spec_loc.getRAngleLoc().isValid()) {
      return true;
    }
    for (unsigned index = 0; index < spec_loc.getNumArgs(); ++index) {
      clang::TemplateArgumentLoc argument_loc = spec_loc.getArgLoc(index);
      clang::TemplateArgument argument = argument_loc.getArgument();
      if (argument.getKind() != clang::TemplateArgument::Type) {
        continue;
      }
      const clang::TypeSourceInfo *source_info =
          argument_loc.getTypeSourceInfo();
      if (source_info == nullptr) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[template-argument-source-surface]: "
                "written type argument=%u has no exact TypeSourceInfo\n",
                index);
        ROSE_ABORT();
      }
      clang::QualType written_type = source_info->getType();
      clang::QualType semantic_type = argument.getAsType().getCanonicalType();
      if (written_type.isNull() || semantic_type.isNull()) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[template-argument-source-surface]: "
                "written type argument=%u has no exact written/semantic "
                "type pair\n",
                index);
        ROSE_ABORT();
      }
      if (written_type.getTypePtrOrNull() != semantic_type.getTypePtrOrNull()) {
        return true;
      }
    }
    return false;
  };

  auto nested_name_specifier_loc_requires_written_nonreal =
      [&](clang::NestedNameSpecifierLoc qualifier_loc) -> bool {
    for (clang::NestedNameSpecifierLoc current = qualifier_loc; current;
         current = nested_name_specifier_loc_prefix(current)) {

      clang::NestedNameSpecifier nns = current.getNestedNameSpecifier();
      if (nns.getKind() != clang::NestedNameSpecifier::Kind::Type) {
        continue;
      }

      const clang::Type *qualifier_type = nns.getAsType();
      if (qualifier_type != nullptr && qualifier_type->isDependentType()) {
        return true;
      }

      clang::TypeLoc qualifier_type_loc = current.getAsTypeLoc();

      if (qualifier_type_loc.isNull()) {
        continue;
      }

      if (qualifier_type_loc.getAs<clang::TemplateSpecializationTypeLoc>() ||
          qualifier_type_loc.getAs<clang::DependentNameTypeLoc>()) {
        return true;
      }

      clang::QualType written = qualifier_type_loc.getType();
      if (!written.isNull() &&
          written.getTypePtrOrNull() !=
              written.getCanonicalType().getTypePtrOrNull()) {
        const clang::Type *written_type = written.getTypePtrOrNull();
        if (written_type != nullptr && !written_type->isDependentType() &&
            written_type->getAsTagDecl() != nullptr) {
          continue;
        }
        return true;
      }
    }

    return false;
  };

  auto build_nonreal_from_specialization_decl_context =
      [&](const clang::ClassTemplateSpecializationDecl *spec_decl,
          SgTemplateArgumentPtrList &written_args) -> SgType * {
    ROSE_ASSERT(spec_decl != nullptr);

    auto resolve_specialization =
        [&](const clang::ClassTemplateSpecializationDecl *specialization)
        -> SgDeclarationStatement * {
      ROSE_ASSERT(specialization != nullptr);
      auto *mutable_spec =
          const_cast<clang::ClassTemplateSpecializationDecl *>(specialization);
      if (lookupSgDeclarationForClangDecl(mutable_spec,
                                          /*allow_on_demand=*/false) == nullptr)
        queuePendingImplicitClassTemplateSpecialization(mutable_spec);
      return resolveNonrealTemplateDeclarationTarget(
          mutable_spec, "specialization-decl-context");
    };

    SgScopeStatement *scope = resolve_scope();

    std::vector<const clang::DeclContext *> contexts;
    for (const clang::DeclContext *dc = spec_decl->getDeclContext();
         dc != nullptr && !dc->isTranslationUnit(); dc = dc->getParent()) {
      contexts.push_back(dc);
    }

    for (auto it = contexts.rbegin(); it != contexts.rend(); ++it) {
      if (const clang::NamespaceDecl *ns =
              llvm::dyn_cast<clang::NamespaceDecl>(*it)) {
        std::string ns_name = ns->getNameAsString();
        if (!ns_name.empty()) {
          SgNonrealType *ns_type = SageBuilder::buildSemanticNonrealType(
              SgName(ns_name), scope, nullptr);
          ROSE_ASSERT(ns_type != nullptr);
          SgNonrealDecl *ns_decl = isSgNonrealDecl(ns_type->get_declaration());
          ROSE_ASSERT(ns_decl != nullptr);
          scope = ns_decl->get_nonreal_decl_scope();
          ROSE_ASSERT(scope != nullptr);
        }
        continue;
      }

      const clang::CXXRecordDecl *ctx_record =
          llvm::dyn_cast<clang::CXXRecordDecl>(*it);
      if (ctx_record == nullptr) {
        continue;
      }

      std::string record_name = ctx_record->getNameAsString();
      if (record_name.empty()) {
        continue;
      }

      const clang::ClassTemplateSpecializationDecl *ctx_spec =
          llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(ctx_record);
      SgTemplateArgumentPtrList ctx_args;
      const SgTemplateArgumentPtrList *ctx_args_ptr = nullptr;
      SgDeclarationStatement *ctx_identity = nullptr;
      if (ctx_spec != nullptr) {
        ctx_identity = resolve_specialization(ctx_spec);
        ctx_args = buildTemplateArguments(ctx_spec->getTemplateArgs(), 0);
        ctx_args_ptr = &ctx_args;
      }

      SgNonrealType *record_type = SageBuilder::buildSemanticNonrealType(
          SgName(record_name), scope, ctx_args_ptr, ctx_identity);
      ROSE_ASSERT(record_type != nullptr);
      SgNonrealDecl *record_decl =
          isSgNonrealDecl(record_type->get_declaration());
      ROSE_ASSERT(record_decl != nullptr);
      scope = record_decl->get_nonreal_decl_scope();
      ROSE_ASSERT(scope != nullptr);
    }

    const clang::ClassTemplateDecl *primary_template =
        spec_decl->getSpecializedTemplate();
    ROSE_ASSERT(primary_template != nullptr);
    const std::string spec_name = primary_template->getNameAsString();
    if (spec_name.empty()) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[specialization-decl-context]: "
              "specialization=%p has no primary-template name\n",
              static_cast<const void *>(spec_decl));
      ROSE_ABORT();
    }

    return SageBuilder::buildSemanticNonrealType(
        SgName(spec_name), scope, &written_args,
        resolve_specialization(spec_decl));
  };

  auto apply_elaborated_keyword_to_nonreal =
      [&](SgType *candidate, clang::ElaboratedTypeKeyword keyword) -> SgType * {
    SgNonrealType *nrtype = isSgNonrealType(candidate);
    SgNonrealDecl *nrdecl = isSgNonrealDecl(
        nrtype != nullptr ? nrtype->get_declaration() : nullptr);
    if (nrdecl != nullptr) {
      const SgNonrealDecl::source_elaboration_kind_enum source_kind =
          sourceElaborationKind(keyword);
      nrdecl->set_source_elaboration_kind(source_kind);
      nrdecl->set_suppress_typename(keyword !=
                                    clang::ElaboratedTypeKeyword::Typename);
    }
    return candidate;
  };
  auto build_injected_class_name_syntax =
      [&](clang::InjectedClassNameTypeLoc injected_loc) -> SgType * {
    const clang::InjectedClassNameType *injected = injected_loc.getTypePtr();
    ASSERT_not_null(injected);
    clang::CXXRecordDecl *record = injected->getDecl();
    ASSERT_not_null(record);

    clang::CXXRecordDecl *template_record = record;
    if (record->isInjectedClassName()) {
      template_record = const_cast<clang::CXXRecordDecl *>(
          llvm::dyn_cast_or_null<clang::CXXRecordDecl>(
              record->getDeclContext()));
    }
    ASSERT_not_null(template_record);

    std::string base_name = template_record->getNameAsString();
    ROSE_ASSERT(!base_name.empty());

    clang::TemplateDecl *template_decl =
        class_template_decl_for_record(template_record);
    clang::Decl *decl_key = template_decl != nullptr
                                ? static_cast<clang::Decl *>(template_decl)
                                : static_cast<clang::Decl *>(template_record);
    // InjectedClassNameTypeLoc has no written template arguments; explicit
    // arguments belong to TemplateSpecializationTypeLoc. A null argument
    // pointer preserves the bare name, while an empty list would spell <>.
    SgScopeStatement *scope = resolve_scope();
    if (scope == nullptr) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[injected-class-source-syntax]: "
              "class=%s has no exact source syntax scope\n",
              base_name.c_str());
      ROSE_ABORT();
    }
    clang::NestedNameSpecifierLoc qualifier_loc =
        injected_loc.getQualifierLoc();
    SgDeclarationStatement *template_identity =
        resolveNonrealTemplateDeclarationTarget(decl_key,
                                                "injected-class-source-syntax");
    SgNonrealType *omitted_type =
        qualifier_loc
            ? build_nonreal_type_from_nested_name_specifier_loc(
                  qualifier_loc, scope, SgName(base_name), nullptr,
                  template_identity, nullptr)
            : SageBuilder::buildSemanticNonrealType(SgName(base_name), scope,
                                                    nullptr, template_identity);
    if (omitted_type == nullptr) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[injected-class-source-syntax]: "
              "class=%s could not publish omitted template-id syntax\n",
              base_name.c_str());
      ROSE_ABORT();
    }
    return apply_elaborated_keyword_to_nonreal(omitted_type,
                                               injected->getKeyword());
  };

  auto is_semantic_outer_embedded_forward_tag =
      [](const clang::TagDecl *tag_decl) -> bool {
    const clang::RecordDecl *record_decl =
        llvm::dyn_cast_or_null<clang::RecordDecl>(tag_decl);
    if (record_decl == nullptr || record_decl->isThisDeclarationADefinition()) {
      return false;
    }

    if (!record_decl->isEmbeddedInDeclarator()) {
      return false;
    }

    const clang::DeclContext *semantic_context = record_decl->getDeclContext();
    const clang::DeclContext *lexical_context =
        record_decl->getLexicalDeclContext();
    return semantic_context != nullptr && lexical_context != nullptr &&
           semantic_context != lexical_context;
  };

  auto build_nonreal_from_elaborated_parts =
      [&](const clang::Type *named_type, clang::ElaboratedTypeKeyword keyword,
          clang::NestedNameSpecifier qualifier, clang::TypeLoc named_loc,
          clang::NestedNameSpecifierLoc explicit_qualifier_loc =
              clang::NestedNameSpecifierLoc()) -> SgType * {
    // The ordinary path only needs this builder for an explicit qualifier;
    // otherwise the translated named declaration type is the correct semantic
    // identity.  A forced written-name transaction also covers an unqualified
    // alias such as the source owner in `template <> int A_int::value`: that
    // use site must retain its own SgNonrealType surface instead of collapsing
    // into the canonical SgTypedefType.
    if (named_type == nullptr ||
        (!qualifier && p_force_written_named_type_qualification_depth == 0)) {
      return nullptr;
    }

    SgScopeStatement *scope = resolve_scope();
    if (p_preserve_current_class_qualifier_depth == 0 &&
        qualifier_targets_current_enclosing_class(qualifier, scope)) {
      qualifier = std::nullopt;
    }
    clang::NestedNameSpecifierLoc qualifier_loc =
        explicit_qualifier_loc ? explicit_qualifier_loc : named_loc.getPrefix();
    auto translated_resolved_tag_type =
        [&](const clang::TagType *tag_type) -> SgType * {
      if (p_force_written_named_type_qualification_depth != 0 &&
          qualifier_loc) {
        return nullptr;
      }
      if (tag_type == nullptr || tag_type->isDependentType() ||
          keyword == clang::ElaboratedTypeKeyword::Typename ||
          nestedNameSpecifierHasDependentTypeQualifier(qualifier) ||
          nested_name_specifier_loc_requires_written_nonreal(qualifier_loc)) {
        return nullptr;
      }

      clang::TagDecl *tag_decl = tag_type->getDecl();
      if (is_semantic_outer_embedded_forward_tag(tag_decl)) {
        return nullptr;
      }
      if (clang::RecordDecl *record_decl =
              llvm::dyn_cast_or_null<clang::RecordDecl>(tag_decl)) {
        return getTypeFromTranslatedRecordDecl(record_decl);
      }

      if (clang::EnumDecl *enum_decl =
              llvm::dyn_cast_or_null<clang::EnumDecl>(tag_decl)) {
        if (SgEnumDeclaration *sg_enum_decl = isSgEnumDeclaration(
                lookupSgDeclarationForClangDecl(enum_decl,
                                                /*allow_on_demand=*/false))) {
          return sg_enum_decl->get_type();
        }
      }

      return nullptr;
    };

    if (auto spec_loc =
            named_loc.getAs<clang::TemplateSpecializationTypeLoc>()) {
      const clang::TemplateSpecializationType *tst = spec_loc.getTypePtr();
      if (tst != nullptr) {
        clang::TemplateName tname = tst->getTemplateName();
        std::string base_name =
            getTemplateNameBase(tname, p_compiler_instance->getASTContext());
        if (!base_name.empty()) {
          clang::NestedNameSpecifierLoc qualifier_loc =
              explicit_qualifier_loc ? explicit_qualifier_loc
                                     : spec_loc.getQualifierLoc();
          const bool qualifier_is_namespace_or_global =
              nestedNameSpecifierLocHasExplicitGlobal(qualifier_loc) ||
              nestedNameSpecifierHasNamespaceQualifier(qualifier);
          const bool qualifier_requires_written_nonreal =
              nested_name_specifier_loc_requires_written_nonreal(qualifier_loc);
          const bool template_args_require_written_nonreal =
              template_specialization_type_loc_requires_written_nonreal(
                  spec_loc);
          const bool force_written_template_syntax =
              p_force_written_template_specialization_depth != 0;
          if (!force_written_template_syntax && !tst->isDependentType() &&
              keyword == clang::ElaboratedTypeKeyword::None &&
              qualifier_is_namespace_or_global &&
              !qualifier_requires_written_nonreal &&
              !template_args_require_written_nonreal) {
            if (SgType *resolved_type =
                    buildTypeFromQualifiedType(named_loc.getType())) {
              return resolved_type;
            }
          }

          SgTemplateArgumentPtrList tpl_args;
          append_nonreal_template_arguments(tpl_args, spec_loc,
                                            "named-type-location-template-id");

          bool has_template_keyword = false;
          if (const clang::QualifiedTemplateName *qtn =
                  tname.getAsQualifiedTemplateName()) {
            has_template_keyword = qtn->hasTemplateKeyword();
          } else if (tname.getAsDependentTemplateName() != nullptr) {
            has_template_keyword = true;
          }

          clang::Decl *target = resolveTemplateNameDeclaration(tname);
          if (!tst->isDependentType() && !tst->isTypeAlias()) {
            if (const clang::CXXRecordDecl *record =
                    cxxRecordDeclFromQualTypeWithoutDefinitionLookup(
                        clang::QualType(tst, 0)))
              target = const_cast<clang::CXXRecordDecl *>(record);
          }
          SgDeclarationStatement *identity =
              qualifier_loc && target != nullptr
                  ? exact_written_template_identity(tst, target)
                  : nullptr;
          if (SgType *nr =
                  qualifier_loc
                      ? static_cast<SgType *>(
                            build_nonreal_type_from_nested_name_specifier_loc(
                                qualifier_loc, scope, SgName(base_name),
                                &tpl_args, identity, tst))
                      : build_nonreal_template_type(
                            base_name, scope, qualifier, has_template_keyword,
                            tpl_args, target, nullptr, tst)) {
            if (SgNonrealDecl *nrdecl =
                    isSgNonrealDecl(isSgNonrealType(nr) != nullptr
                                        ? isSgNonrealType(nr)->get_declaration()
                                        : nullptr)) {
              if (has_template_keyword ||
                  spec_loc.getTemplateKeywordLoc().isValid()) {
                nrdecl->set_has_template_keyword(true);
              }
            }
            return apply_elaborated_keyword_to_nonreal(nr, keyword);
          }
          std::cerr << "REX_FRONTEND_INVARIANT[template-argument-source-"
                       "surface]: named template-id failed to construct one "
                       "per-use written type surface\n";
          ROSE_ABORT();
        }
      }
    }

    if (const clang::DependentNameType *dnt =
            llvm::dyn_cast<clang::DependentNameType>(named_type)) {
      const clang::IdentifierInfo *id = dnt->getIdentifier();
      if (id != nullptr) {
        return apply_elaborated_keyword_to_nonreal(
            qualifier_loc
                ? static_cast<SgType *>(
                      build_nonreal_type_from_nested_name_specifier_loc(
                          qualifier_loc, scope, SgName(id->getName().str()),
                          nullptr, nullptr, nullptr))
                : static_cast<SgType *>(
                      buildSemanticNonrealTypeFromNestedNameSpecifier(
                          qualifier, scope, SgName(id->getName().str()),
                          nullptr)),
            keyword);
      }
    }

    auto build_alias_type = [&](clang::NamedDecl *alias_decl,
                                bool using_alias) -> SgType * {
      if (alias_decl == nullptr || alias_decl->getNameAsString().empty()) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[written-alias-declaration]: "
                "qualified alias has no exact Clang declaration or name\n");
        ROSE_ABORT();
      }
      SgDeclarationStatement *exact_decl =
          lookupSgDeclarationForClangDecl(alias_decl,
                                          /*allow_on_demand=*/true);
      if ((using_alias &&
           isSgUsingDeclarationStatement(exact_decl) == nullptr) ||
          (!using_alias && isSgTypedefDeclaration(exact_decl) == nullptr)) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[written-alias-declaration]: alias=%s "
                "has no exact translated declaration\n",
                alias_decl->getQualifiedNameAsString().c_str());
        ROSE_ABORT();
      }
      const SgName name(alias_decl->getNameAsString());
      SgNonrealType *type =
          qualifier_loc
              ? build_nonreal_type_from_nested_name_specifier_loc(
                    qualifier_loc, scope, name, nullptr, exact_decl, nullptr)
              : buildSemanticNonrealTypeFromNestedNameSpecifier(
                    qualifier, scope, name, nullptr, exact_decl);
      return apply_elaborated_keyword_to_nonreal(type, keyword);
    };
    if (const auto *typedef_type =
            llvm::dyn_cast<clang::TypedefType>(named_type)) {
      return build_alias_type(typedef_type->getDecl(), false);
    }
    if (const auto *using_type = llvm::dyn_cast<clang::UsingType>(named_type)) {
      return build_alias_type(using_type->getDecl(), true);
    }

    if (const clang::UnresolvedUsingType *unresolved_using =
            llvm::dyn_cast<clang::UnresolvedUsingType>(named_type)) {
      clang::UnresolvedUsingTypenameDecl *decl = unresolved_using->getDecl();
      if (decl != nullptr) {
        return apply_elaborated_keyword_to_nonreal(
            qualifier_loc
                ? static_cast<SgType *>(
                      build_nonreal_type_from_nested_name_specifier_loc(
                          qualifier_loc, scope, SgName(decl->getNameAsString()),
                          nullptr, nullptr, nullptr))
                : static_cast<SgType *>(
                      buildSemanticNonrealTypeFromNestedNameSpecifier(
                          qualifier, scope, SgName(decl->getNameAsString()),
                          nullptr)),
            keyword);
      }
    }

    if (const clang::TagType *tag_type =
            llvm::dyn_cast<clang::TagType>(named_type)) {
      if (SgType *translated_tag_type =
              translated_resolved_tag_type(tag_type)) {
        return translated_tag_type;
      }
      clang::TagDecl *tag_decl = tag_type->getDecl();
      if (tag_decl != nullptr) {
        SgDeclarationStatement *identity =
            resolveNonrealTemplateDeclarationTarget(tag_decl,
                                                    "named-type-location");
        return apply_elaborated_keyword_to_nonreal(
            qualifier_loc
                ? static_cast<SgType *>(
                      build_nonreal_type_from_nested_name_specifier_loc(
                          qualifier_loc, scope,
                          SgName(tag_decl->getNameAsString()), nullptr,
                          identity, nullptr))
                : static_cast<SgType *>(
                      buildSemanticNonrealTypeFromNestedNameSpecifier(
                          qualifier, scope, SgName(tag_decl->getNameAsString()),
                          nullptr, identity)),
            keyword);
      }
    }

    if (const clang::InjectedClassNameType *injected =
            llvm::dyn_cast<clang::InjectedClassNameType>(named_type)) {
      clang::QualType injected_qt = getInjectedClassNameSpecializationType(
          injected, p_compiler_instance->getASTContext());
      const clang::Type *injected_ty = injected_qt.getTypePtrOrNull();
      ASSERT_not_null(injected_ty);
      SgNonrealType *nrtype = buildNonrealTypeForNestedNameSpecifierType(
          injected_ty, scope, /*prefer_current_scope=*/false);
      ASSERT_not_null(nrtype);
      return apply_elaborated_keyword_to_nonreal(nrtype, keyword);
    }

    return nullptr;
  };

  if (auto spec_loc = type_loc.getAs<clang::TemplateSpecializationTypeLoc>()) {

    const clang::TemplateSpecializationType *spec_type =
        static_cast<const clang::TemplateSpecializationType *>(
            spec_loc.getTypePtr());
    clang::NestedNameSpecifierLoc qualifier_loc = spec_loc.getQualifierLoc();
    if (SgType *written_type = build_nonreal_from_elaborated_parts(
            spec_type, spec_type->getKeyword(),
            qualifier_loc.getNestedNameSpecifier(), spec_loc, qualifier_loc)) {
      return finalize_spelled_type(written_type);
    }
  }

  if (auto dep_name_loc = type_loc.getAs<clang::DependentNameTypeLoc>()) {

    if (SgType *written_type = build_nonreal_from_elaborated_parts(
            dep_name_loc.getTypePtr(), dep_name_loc.getTypePtr()->getKeyword(),
            dep_name_loc.getQualifierLoc().getNestedNameSpecifier(),
            dep_name_loc, dep_name_loc.getQualifierLoc())) {
      return finalize_spelled_type(written_type);
    }
  }

  if (auto typedef_loc = type_loc.getAs<clang::TypedefTypeLoc>()) {

    const clang::TypedefType *typedef_type = typedef_loc.getTypePtr();
    clang::NestedNameSpecifierLoc qualifier_loc = typedef_loc.getQualifierLoc();
    clang::NestedNameSpecifier qualifier =
        qualifier_loc.getNestedNameSpecifier();
    const bool has_written_qualifier = static_cast<bool>(qualifier);
    const bool qualifier_has_type_qualifier =
        nestedNameSpecifierHasTypeQualifier(qualifier);
    const bool qualifier_spells_namespace_or_global =
        nestedNameSpecifierLocHasExplicitGlobal(qualifier_loc) ||
        nestedNameSpecifierHasNamespaceQualifier(qualifier);
    if (!qualifier && typedef_type != nullptr &&
        !typedef_type->isDependentType()) {
      qualifier = typedef_type->getQualifier();
    }
    const bool qualifier_requires_written_nonreal =
        nested_name_specifier_loc_requires_written_nonreal(qualifier_loc);
    const bool preserve_unqualified_written_typedef =
        typedef_type != nullptr && !has_written_qualifier &&
        !qualifier_requires_written_nonreal &&
        p_force_written_named_type_qualification_depth == 0;
    const bool use_translated_typedef_type =
        typedef_type != nullptr && has_written_qualifier &&
        p_force_written_named_type_qualification_depth == 0 &&
        typedef_type->getKeyword() == clang::ElaboratedTypeKeyword::None &&
        !typedef_type->isDependentType() && !qualifier_has_type_qualifier &&
        !qualifier_requires_written_nonreal &&
        qualifier_spells_namespace_or_global;

    if (preserve_unqualified_written_typedef) {
      // TypedefType carries an exact TypedefNameDecl even when its underlying
      // type depends on the surrounding template.  Preserve that declaration
      // identity instead of replacing the written alias with an unresolved
      // SgNonrealDecl that later name qualification cannot make visible.
      TranslatedTypedefTypeIdentity resolved_identity =
          build_translated_typedef_type_from_decl(typedef_type->getDecl());
      SgType *resolved_type = resolved_identity.type;
      SgTypedefType *resolved_typedef = isSgTypedefType(resolved_type);
      SgDeclarationStatement *translated_declaration =
          resolved_identity.declaration;
      SgType *translated_declaration_type = nullptr;
      if (SgTypedefDeclaration *typedef_declaration =
              isSgTypedefDeclaration(translated_declaration)) {
        translated_declaration_type = typedef_declaration->get_type();
      } else if (SgTemplateTypedefDeclaration *template_declaration =
                     isSgTemplateTypedefDeclaration(translated_declaration)) {
        translated_declaration_type = template_declaration->get_type();
      }
      if (resolved_typedef == nullptr || translated_declaration == nullptr ||
          resolved_typedef->get_declaration() != translated_declaration ||
          translated_declaration_type != resolved_typedef) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[unqualified-typedef-type]: "
                "typedef=%s resolved=%p declaration=%p resolved-declaration=%p "
                "declaration-type=%p active-scope=%p has no exact translated "
                "SgTypedefType identity\n",
                typedef_type->getDecl()->getQualifiedNameAsString().c_str(),
                static_cast<void *>(resolved_typedef),
                static_cast<void *>(translated_declaration),
                static_cast<void *>(resolved_typedef != nullptr
                                        ? resolved_typedef->get_declaration()
                                        : nullptr),
                static_cast<void *>(translated_declaration_type),
                static_cast<void *>(SageBuilder::topScopeStack()));
        ROSE_ABORT();
      }
      p_type_translation_map[typedef_type] = resolved_typedef;
      resolved_type =
          apply_local_qualifiers(resolved_typedef, type_loc.getType());
      return resolved_type;
    }

    if (use_translated_typedef_type) {
      // Namespace and global qualification are source spelling carried by the
      // exact declaration use site.  The type graph must retain the semantic
      // SgTypedefType edge; representing a concrete alias as SgNonrealType
      // loses the declaration dependency and forces later output repair.
      TranslatedTypedefTypeIdentity resolved_identity =
          build_translated_typedef_type_from_decl(typedef_type->getDecl());
      SgType *resolved_type = resolved_identity.type;
      SgTypedefType *resolved_typedef = isSgTypedefType(resolved_type);
      SgTypedefDeclaration *resolved_declaration =
          isSgTypedefDeclaration(resolved_identity.declaration);
      if (resolved_typedef == nullptr || resolved_declaration == nullptr ||
          resolved_typedef->get_declaration() != resolved_declaration ||
          resolved_declaration->get_type() != resolved_typedef) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[qualified-typedef-type]: typedef=%s "
                "has no exact translated SgTypedefType identity\n",
                typedef_type->getDecl()->getQualifiedNameAsString().c_str());
        ROSE_ABORT();
      }
      p_type_translation_map[typedef_type] = resolved_typedef;
      return finalize_spelled_type(resolved_typedef);
    }

    if (SgType *written_type = build_nonreal_from_elaborated_parts(
            typedef_loc.getTypePtr(), typedef_loc.getTypePtr()->getKeyword(),
            qualifier, typedef_loc, qualifier_loc)) {
      return finalize_spelled_type(written_type);
    }
  }

  if (auto using_loc = type_loc.getAs<clang::UsingTypeLoc>()) {
    const clang::UsingType *using_type = using_loc.getTypePtr();
    clang::NestedNameSpecifierLoc qualifier_loc = using_loc.getQualifierLoc();
    clang::NestedNameSpecifier qualifier =
        qualifier_loc.getNestedNameSpecifier();
    if (!qualifier && using_type != nullptr) {
      qualifier = using_type->getQualifier();
    }
    const bool has_written_qualifier = static_cast<bool>(qualifier);
    const bool qualifier_has_type_qualifier =
        nestedNameSpecifierHasTypeQualifier(qualifier);
    const bool qualifier_spells_namespace_or_global =
        nestedNameSpecifierLocHasExplicitGlobal(qualifier_loc) ||
        nestedNameSpecifierHasNamespaceQualifier(qualifier);
    const bool qualifier_requires_written_nonreal =
        nested_name_specifier_loc_requires_written_nonreal(qualifier_loc);
    const bool use_translated_using_type =
        using_type != nullptr && has_written_qualifier &&
        using_type->getKeyword() == clang::ElaboratedTypeKeyword::None &&
        !using_type->isDependentType() && !qualifier_has_type_qualifier &&
        !qualifier_requires_written_nonreal &&
        !qualifier_spells_namespace_or_global;

    if (use_translated_using_type) {
      if (SgType *resolved_type =
              buildTypeFromQualifiedType(type_loc.getType())) {
        return resolved_type;
      }
    }

    if (SgType *written_type = build_nonreal_from_elaborated_parts(
            using_loc.getTypePtr(), using_loc.getTypePtr()->getKeyword(),
            qualifier, using_loc, using_loc.getQualifierLoc())) {
      return finalize_spelled_type(written_type);
    }
  }

  if (auto unresolved_using_loc =
          type_loc.getAs<clang::UnresolvedUsingTypeLoc>()) {
    if (SgType *written_type = build_nonreal_from_elaborated_parts(
            unresolved_using_loc.getTypePtr(),
            unresolved_using_loc.getTypePtr()->getKeyword(),
            unresolved_using_loc.getQualifierLoc().getNestedNameSpecifier(),
            unresolved_using_loc, unresolved_using_loc.getQualifierLoc())) {
      return finalize_spelled_type(written_type);
    }
  }

  if (auto injected_loc = type_loc.getAs<clang::InjectedClassNameTypeLoc>()) {
    return finalize_spelled_type(
        build_injected_class_name_syntax(injected_loc));
  }

  if (auto tag_loc = type_loc.getAs<clang::TagTypeLoc>()) {
    const clang::TagType *tag_type =
        llvm::dyn_cast_or_null<clang::TagType>(tag_loc.getTypePtr());
    clang::NestedNameSpecifierLoc qualifier_loc = tag_loc.getQualifierLoc();
    clang::NestedNameSpecifier qualifier =
        qualifier_loc.getNestedNameSpecifier();
    const bool has_explicit_written_qualifier = static_cast<bool>(qualifier);
    if (!qualifier && tag_type != nullptr) {
      qualifier = tag_type->getQualifier();
    }
    const bool has_written_qualifier = static_cast<bool>(qualifier);
    const bool qualifier_has_type_qualifier =
        nestedNameSpecifierHasTypeQualifier(qualifier);
    const bool qualifier_spells_namespace_or_global =
        nestedNameSpecifierLocHasExplicitGlobal(qualifier_loc) ||
        nestedNameSpecifierHasNamespaceQualifier(qualifier);
    const bool qualifier_requires_written_nonreal =
        nested_name_specifier_loc_requires_written_nonreal(qualifier_loc);
    const bool force_written_qualified_tag_type =
        p_force_written_named_type_qualification_depth != 0 && qualifier_loc;
    auto translated_tag_type_if_available =
        [&](const clang::TagDecl *raw_tag_decl) -> SgType * {
      clang::TagDecl *tag_decl = const_cast<clang::TagDecl *>(raw_tag_decl);
      if (tag_decl == nullptr) {
        return nullptr;
      }
      if (is_semantic_outer_embedded_forward_tag(tag_decl)) {
        return nullptr;
      }
      if (clang::RecordDecl *record_decl =
              llvm::dyn_cast<clang::RecordDecl>(tag_decl)) {
        return getTypeFromTranslatedRecordDecl(record_decl);
      }
      if (clang::EnumDecl *enum_decl =
              llvm::dyn_cast<clang::EnumDecl>(tag_decl)) {
        if (SgEnumDeclaration *sg_enum_decl = isSgEnumDeclaration(
                lookupSgDeclarationForClangDecl(enum_decl,
                                                /*allow_on_demand=*/false))) {
          return sg_enum_decl->get_type();
        }
      }
      return nullptr;
    };
    SgType *translated_tag_type = translated_tag_type_if_available(
        tag_type != nullptr
            ? llvm::dyn_cast_or_null<clang::TagDecl>(tag_type->getDecl())
            : nullptr);

    auto build_current_instantiation_member_tag_type =
        [&](const clang::TagDecl *raw_tag_decl) -> SgType * {
      clang::TagDecl *tag_decl = const_cast<clang::TagDecl *>(raw_tag_decl);
      if (tag_decl == nullptr || tag_type == nullptr ||
          has_explicit_written_qualifier ||
          tag_type->getKeyword() != clang::ElaboratedTypeKeyword::None) {
        return nullptr;
      }

      const auto *owner_record = llvm::dyn_cast_or_null<clang::CXXRecordDecl>(
          tag_decl->getDeclContext());
      if (owner_record == nullptr ||
          (templateParametersForDeclContext(owner_record) == nullptr &&
           !owner_record->isDependentContext())) {
        return nullptr;
      }

      const clang::FunctionDecl *current_function = nullptr;
      for (auto it = p_template_parameter_decl_context_stack.rbegin();
           it != p_template_parameter_decl_context_stack.rend(); ++it) {
        current_function = llvm::dyn_cast_or_null<clang::FunctionDecl>(*it);
        if (current_function != nullptr &&
            current_function->getQualifierLoc()) {
          break;
        }
        current_function = nullptr;
      }
      const auto *method_decl =
          llvm::dyn_cast_or_null<clang::CXXMethodDecl>(current_function);
      if (method_decl == nullptr || method_decl->getParent() == nullptr) {
        return nullptr;
      }

      auto template_identity_record = [](const clang::CXXRecordDecl *record)
          -> const clang::CXXRecordDecl * {
        if (record == nullptr) {
          return nullptr;
        }
        record = record->getCanonicalDecl();
        if (const auto *partial =
                llvm::dyn_cast<clang::ClassTemplatePartialSpecializationDecl>(
                    record)) {
          if (const clang::ClassTemplateDecl *tmpl =
                  partial->getSpecializedTemplate()) {
            return tmpl->getTemplatedDecl()->getCanonicalDecl();
          }
        }
        if (const auto *spec =
                llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(
                    record)) {
          if (const clang::ClassTemplateDecl *tmpl =
                  spec->getSpecializedTemplate()) {
            return tmpl->getTemplatedDecl()->getCanonicalDecl();
          }
        }
        if (const clang::ClassTemplateDecl *tmpl =
                record->getDescribedClassTemplate()) {
          return tmpl->getTemplatedDecl()->getCanonicalDecl();
        }
        return record;
      };
      auto same_template_record = [&](const clang::CXXRecordDecl *lhs,
                                      const clang::CXXRecordDecl *rhs) {
        const clang::CXXRecordDecl *lhs_id = template_identity_record(lhs);
        const clang::CXXRecordDecl *rhs_id = template_identity_record(rhs);
        return lhs_id != nullptr && lhs_id == rhs_id;
      };

      clang::NestedNameSpecifierLoc method_qualifier_loc =
          current_function->getQualifierLoc();
      clang::NestedNameSpecifier method_qualifier =
          method_qualifier_loc.getNestedNameSpecifier();
      if (!method_qualifier || method_qualifier.getKind() !=
                                   clang::NestedNameSpecifier::Kind::Type) {
        return nullptr;
      }
      const clang::CXXRecordDecl *qualified_record =
          llvm::dyn_cast_or_null<clang::CXXRecordDecl>(
              method_qualifier.getAsRecordDecl());
      const clang::CXXRecordDecl *method_parent =
          llvm::dyn_cast_or_null<clang::CXXRecordDecl>(
              method_decl->getParent());
      if (!same_template_record(owner_record, method_parent) ||
          !same_template_record(owner_record, qualified_record)) {
        return nullptr;
      }

      std::string tag_name = tag_decl->getNameAsString();
      if (tag_name.empty()) {
        return nullptr;
      }

      SgDeclarationStatement *translated_tag =
          normalizeNonrealTemplateDeclarationTarget(
              lookupSgDeclarationForClangDecl(tag_decl,
                                              /*allow_on_demand=*/true));
      if (translated_tag == nullptr) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[current-instantiation-member-tag]: "
                "tag=%s has no exact translated declaration\n",
                tag_name.c_str());
        ROSE_ABORT();
      }
      SgNonrealType *nrtype = build_nonreal_type_from_nested_name_specifier_loc(
          method_qualifier_loc, resolve_scope(), SgName(tag_name), nullptr,
          translated_tag, nullptr);
      ROSE_ASSERT(nrtype != nullptr);
      return nrtype;
    };

    if (SgType *current_instantiation_tag_type =
            build_current_instantiation_member_tag_type(
                tag_type != nullptr ? llvm::dyn_cast_or_null<clang::TagDecl>(
                                          tag_type->getDecl())
                                    : nullptr)) {
      return finalize_spelled_type(current_instantiation_tag_type);
    }

    const bool use_translated_tag_type =
        translated_tag_type != nullptr && tag_type != nullptr &&
        has_written_qualifier &&
        tag_type->getKeyword() != clang::ElaboratedTypeKeyword::Typename &&
        !tag_type->isDependentType() && !qualifier_has_type_qualifier &&
        !qualifier_requires_written_nonreal &&
        !force_written_qualified_tag_type;

    // Qualified tag references such as `Outer::Inner` should use the
    // translated declaration type instead of a synthetic SgNonrealType.
    // The nonreal path is needed for dependent and elaborated spellings, but
    // for resolved tags it loses the declaration identity and can corrupt
    // ownership/name-qualification for nested and namespace-qualified tags.
    if (use_translated_tag_type) {
      return finalize_spelled_type(translated_tag_type);
    }

    if (SgType *written_type = build_nonreal_from_elaborated_parts(
            tag_type, tag_type->getKeyword(), qualifier, tag_loc,
            qualifier_loc)) {
      return finalize_spelled_type(written_type);
    }
  }

  if (auto dep_name_loc = type_loc.getAs<clang::DependentNameTypeLoc>()) {
    const clang::DependentNameType *dnt = dep_name_loc.getTypePtr();
    if (dnt != nullptr) {
      const clang::IdentifierInfo *id = dnt->getIdentifier();
      if (id != nullptr) {
        SgScopeStatement *scope = resolve_scope();
        clang::NestedNameSpecifierLoc qualifier_loc =
            dep_name_loc.getQualifierLoc();
        clang::NestedNameSpecifier qualifier = dnt->getQualifier();
        if (!qualifier) {
          qualifier = qualifier_loc.getNestedNameSpecifier();
        }
        SgNonrealType *nrtype =
            qualifier_loc
                ? build_nonreal_type_from_nested_name_specifier_loc(
                      qualifier_loc, scope, SgName(id->getName().str()),
                      nullptr, nullptr, nullptr)
                : buildSemanticNonrealTypeFromNestedNameSpecifier(
                      qualifier, scope, SgName(id->getName().str()), nullptr);
        if (SgNonrealDecl *nrdecl =
                isSgNonrealDecl(nrtype ? nrtype->get_declaration() : nullptr)) {
          nrdecl->set_suppress_typename(false);
        }
        if (nrtype != nullptr) {
          return finalize_spelled_type(nrtype);
        }
      }
    }
  }

  if (auto spec_loc = type_loc.getAs<clang::TemplateSpecializationTypeLoc>()) {
    const clang::TemplateSpecializationType *tst = spec_loc.getTypePtr();
    if (tst != nullptr && !tst->isDependentType()) {
      clang::TemplateName tname = tst->getTemplateName();
      std::string base_name =
          getTemplateNameBase(tname, p_compiler_instance->getASTContext());
      if (!base_name.empty()) {
        SgTemplateArgumentPtrList tpl_args;
        append_nonreal_template_arguments(tpl_args, spec_loc,
                                          "nondependent-written-template-id");

        clang::NestedNameSpecifier qualifier =
            spec_loc.getQualifierLoc().getNestedNameSpecifier();
        bool has_template_keyword = false;
        if (const clang::QualifiedTemplateName *qtn =
                tname.getAsQualifiedTemplateName()) {
          if (!qualifier) {
            qualifier = qtn->getQualifier();
          }
          has_template_keyword = qtn->hasTemplateKeyword();
        } else if (const clang::DependentTemplateName *dtn =
                       tname.getAsDependentTemplateName()) {
          if (!qualifier) {
            qualifier = dtn->getQualifier();
          }
          has_template_keyword = true;
        }

        clang::TemplateDecl *template_decl =
            resolveTemplateNameDeclaration(tname);
        SgScopeStatement *scope = resolve_scope();
        const clang::ClassTemplateSpecializationDecl *spec_record_decl =
            llvm::dyn_cast_or_null<clang::ClassTemplateSpecializationDecl>(
                cxxRecordDeclFromQualTypeWithoutDefinitionLookup(
                    clang::QualType(tst, 0)));
        const bool uses_nested_specialization_decl_context =
            !tst->isTypeAlias() && spec_record_decl != nullptr &&
            llvm::isa<clang::CXXRecordDecl>(spec_record_decl->getDeclContext());
        const bool has_written_prefix = static_cast<bool>(spec_loc.getPrefix());
        const bool force_written_template_syntax =
            p_force_written_template_specialization_depth != 0;
        const bool preserve_written_lookup =
            force_written_template_syntax || has_written_prefix ||
            uses_nested_specialization_decl_context;
        if (preserve_written_lookup ||
            template_specialization_type_loc_requires_written_nonreal(
                spec_loc)) {
          clang::Decl *written_template_target =
              tst->isTypeAlias() || spec_record_decl == nullptr
                  ? static_cast<clang::Decl *>(template_decl)
                  : const_cast<clang::ClassTemplateSpecializationDecl *>(
                        spec_record_decl);
          if (has_written_prefix) {
            SgDeclarationStatement *template_identity =
                written_template_target != nullptr
                    ? exact_written_template_identity(tst,
                                                      written_template_target)
                    : nullptr;
            SgType *nr = build_nonreal_type_from_nested_name_specifier_loc(
                spec_loc.getPrefix(), scope, SgName(base_name), &tpl_args,
                template_identity, tst);
            if (SgNonrealDecl *nrdecl =
                    isSgNonrealDecl(isSgNonrealType(nr) != nullptr
                                        ? isSgNonrealType(nr)->get_declaration()
                                        : nullptr)) {
              if (has_template_keyword ||
                  spec_loc.getTemplateKeywordLoc().isValid()) {
                nrdecl->set_has_template_keyword(true);
              }
            }
            return finalize_spelled_type(nr);
          }
          if (uses_nested_specialization_decl_context) {
            SgType *nr = build_nonreal_from_specialization_decl_context(
                spec_record_decl, tpl_args);
            return finalize_spelled_type(nr);
          }
          SgType *nr = build_nonreal_template_type(
              base_name, scope, qualifier, has_template_keyword, tpl_args,
              written_template_target, nullptr, tst);
          SgNonrealDecl *nrdecl =
              isSgNonrealDecl(isSgNonrealType(nr)->get_declaration());
          ROSE_ASSERT(nrdecl != nullptr);
          if (has_template_keyword ||
              spec_loc.getTemplateKeywordLoc().isValid()) {
            nrdecl->set_has_template_keyword(true);
          }
          return finalize_spelled_type(nr);
        }
      }
    }

    if (tst != nullptr && tst->isDependentType()) {
      clang::TemplateName tname = tst->getTemplateName();
      std::string base_name =
          getTemplateNameBase(tname, p_compiler_instance->getASTContext());
      if (!base_name.empty()) {
        SgTemplateArgumentPtrList tpl_args;
        append_nonreal_template_arguments(tpl_args, spec_loc,
                                          "dependent-written-template-id");

        clang::NestedNameSpecifier qualifier = std::nullopt;
        bool has_template_keyword = false;
        if (const clang::QualifiedTemplateName *qtn =
                tname.getAsQualifiedTemplateName()) {
          qualifier = qtn->getQualifier();
          has_template_keyword = qtn->hasTemplateKeyword();
        } else if (const clang::DependentTemplateName *dtn =
                       tname.getAsDependentTemplateName()) {
          qualifier = dtn->getQualifier();
          has_template_keyword = true;
        }

        SgType *nr = nullptr;
        if (clang::NestedNameSpecifierLoc qualifier_loc =
                spec_loc.getPrefix()) {
          clang::TemplateDecl *target = resolveTemplateNameDeclaration(tname);
          SgDeclarationStatement *identity =
              target != nullptr ? exact_written_template_identity(tst, target)
                                : nullptr;
          nr = build_nonreal_type_from_nested_name_specifier_loc(
              qualifier_loc, resolve_scope(), SgName(base_name), &tpl_args,
              identity, tst);
        } else {
          nr = build_nonreal_template_type(
              base_name, resolve_scope(), qualifier, has_template_keyword,
              tpl_args, resolveTemplateNameDeclaration(tname), nullptr, tst);
        }
        ROSE_ASSERT(nr != nullptr);
        SgNonrealType *nonreal_type = isSgNonrealType(nr);
        ROSE_ASSERT(nonreal_type != nullptr);
        SgNonrealDecl *nrdecl =
            isSgNonrealDecl(nonreal_type->get_declaration());
        ROSE_ASSERT(nrdecl != nullptr);
        if (has_template_keyword ||
            spec_loc.getTemplateKeywordLoc().isValid()) {
          nrdecl->set_has_template_keyword(true);
        }
        return finalize_spelled_type(nr);
      }
    }
  }

  clang::QualType type_loc_qt = type_loc.getType();
  const clang::Type *type_loc_ptr = type_loc_qt.getTypePtrOrNull();
  if (const clang::TemplateSpecializationType *tst =
          type_loc_ptr != nullptr
              ? type_loc_ptr->getAs<clang::TemplateSpecializationType>()
              : nullptr) {
    if (!tst->isDependentType()) {
      if (tst->isTypeAlias()) {
        ++p_force_written_template_specialization_depth;
      }
      SgType *resolved_type = buildTypeFromQualifiedType(type_loc.getType());
      if (tst->isTypeAlias()) {
        ROSE_ASSERT(p_force_written_template_specialization_depth > 0);
        --p_force_written_template_specialization_depth;
      }
      if (auto spec_loc =
              type_loc.getAs<clang::TemplateSpecializationTypeLoc>()) {
        const bool requires_written_nonreal =
            template_specialization_type_loc_requires_written_nonreal(spec_loc);
        if (requires_written_nonreal ||
            resolves_to_primary_template_class_type(resolved_type)) {
          clang::TemplateName tname = tst->getTemplateName();
          std::string base_name =
              getTemplateNameBase(tname, p_compiler_instance->getASTContext());
          if (!base_name.empty()) {
            SgTemplateArgumentPtrList tpl_args;
            append_nonreal_template_arguments(
                tpl_args, spec_loc, "primary-template-written-template-id");

            clang::NestedNameSpecifier qualifier =
                spec_loc.getQualifierLoc().getNestedNameSpecifier();
            bool has_template_keyword = false;
            if (const clang::QualifiedTemplateName *qtn =
                    tname.getAsQualifiedTemplateName()) {
              if (!qualifier) {
                qualifier = qtn->getQualifier();
              }
              has_template_keyword = qtn->hasTemplateKeyword();
            } else if (const clang::DependentTemplateName *dtn =
                           tname.getAsDependentTemplateName()) {
              if (!qualifier) {
                qualifier = dtn->getQualifier();
              }
              has_template_keyword = true;
            }

            return finalize_spelled_type(build_nonreal_template_type(
                base_name, resolve_scope(), qualifier,
                has_template_keyword ||
                    spec_loc.getTemplateKeywordLoc().isValid(),
                tpl_args, resolveTemplateNameDeclaration(tname), nullptr, tst));
          }
          std::cerr << "REX_FRONTEND_INVARIANT[template-argument-source-"
                       "surface]: explicit template-id failed to construct "
                       "one per-use written type surface\n";
          ROSE_ABORT();
        }
      }
      return resolved_type;
    }
    clang::TemplateName tname = tst->getTemplateName();
    std::string base_name =
        getTemplateNameBase(tname, p_compiler_instance->getASTContext());
    if (!base_name.empty()) {
      SgTemplateArgumentPtrList tpl_args = buildTemplateArguments(tst, true);
      clang::NestedNameSpecifier qualifier = std::nullopt;
      bool has_template_keyword = false;
      if (const clang::QualifiedTemplateName *qtn =
              tname.getAsQualifiedTemplateName()) {
        qualifier = qtn->getQualifier();
        has_template_keyword = qtn->hasTemplateKeyword();
      } else if (const clang::DependentTemplateName *dtn =
                     tname.getAsDependentTemplateName()) {
        qualifier = dtn->getQualifier();
        has_template_keyword = true;
      }
      return finalize_spelled_type(build_nonreal_template_type(
          base_name, resolve_scope(), qualifier, has_template_keyword, tpl_args,
          resolveTemplateNameDeclaration(tname), nullptr, tst));
    }
  }

  SgType *result = buildTypeFromQualifiedType(type_loc.getType());
  return result;
}

SgNode *ClangToSageTranslator::Traverse(const clang::Type *type) {

  if (type == NULL)
    return NULL;

  const bool cache_translation = p_explicit_template_id_type_use_depth == 0;
  const bool globally_cacheable =
      cache_translation && canCacheTypeTranslation(type);
  const ContextualTypeTranslationKey contextual_cache_key = std::make_tuple(
      reinterpret_cast<uintptr_t>(type),
      reinterpret_cast<uintptr_t>(currentTemplateParameterDeclContext()),
      reinterpret_cast<uintptr_t>(currentTemplateParameterConstructionScope()),
      reinterpret_cast<uintptr_t>(SageBuilder::topScopeStack()),
      reinterpret_cast<uintptr_t>(currentMemberCallSpecializationContext()),
      reinterpret_cast<uintptr_t>(
          p_canonical_template_argument_context.value_or(nullptr)),
      p_semantic_template_argument_expression_depth != 0,
      p_exact_template_parameter_stack.contextId());
  SgNode *cached_result = nullptr;
  if (globally_cacheable) {
    auto cached = p_type_translation_map.find(type);
    if (cached != p_type_translation_map.end()) {
      cached_result = cached->second;
    }
  } else if (cache_translation) {
    auto cached = p_contextual_type_translation_map.find(contextual_cache_key);
    if (cached != p_contextual_type_translation_map.end()) {
      cached_result = cached->second;
    }
  }
  if (cached_result != nullptr) {
    return cached_result;
  }

  SgNode *result = NULL;
  bool ret_status = false;

  switch (type->getTypeClass()) {
  case clang::Type::Decayed:
    ret_status = VisitDecayedType((clang::DecayedType *)type, &result);
    break;
  case clang::Type::ConstantArray:
    ret_status =
        VisitConstantArrayType((clang::ConstantArrayType *)type, &result);
    break;
  case clang::Type::DependentSizedArray:
    ret_status = VisitDependentSizedArrayType(
        (clang::DependentSizedArrayType *)type, &result);
    break;
  case clang::Type::IncompleteArray:
    ret_status =
        VisitIncompleteArrayType((clang::IncompleteArrayType *)type, &result);
    break;
  case clang::Type::VariableArray:
    ret_status =
        VisitVariableArrayType((clang::VariableArrayType *)type, &result);
    break;
  case clang::Type::Atomic:
    ret_status = VisitAtomicType((clang::AtomicType *)type, &result);
    break;
  case clang::Type::Attributed:
    ret_status = VisitAttributedType((clang::AttributedType *)type, &result);
    break;
  case clang::Type::BlockPointer:
    ret_status =
        VisitBlockPointerType((clang::BlockPointerType *)type, &result);
    break;
  case clang::Type::Builtin:
    ret_status = VisitBuiltinType((clang::BuiltinType *)type, &result);
    break;
  case clang::Type::Complex:
    ret_status = VisitComplexType((clang::ComplexType *)type, &result);
    break;
  case clang::Type::Decltype:
    ret_status = VisitDecltypeType((clang::DecltypeType *)type, &result);
    break;
  case clang::Type::Auto:
    ret_status = VisitAutoType((clang::AutoType *)type, &result);
    break;
  case clang::Type::DeducedTemplateSpecialization:
    ret_status = VisitDeducedTemplateSpecializationType(
        (clang::DeducedTemplateSpecializationType *)type, &result);
    break;
  case clang::Type::DependentSizedExtVector:
    ret_status = VisitDependentSizedExtVectorType(
        (clang::DependentSizedExtVectorType *)type, &result);
    break;
  case clang::Type::DependentVector:
    ret_status =
        VisitDependentVectorType((clang::DependentVectorType *)type, &result);
    break;
  case clang::Type::FunctionNoProto:
    ret_status =
        VisitFunctionNoProtoType((clang::FunctionNoProtoType *)type, &result);
    break;
  case clang::Type::FunctionProto:
    ret_status =
        VisitFunctionProtoType((clang::FunctionProtoType *)type, &result);
    break;
  case clang::Type::InjectedClassName:
    ret_status = VisitInjectedClassNameType(
        (clang::InjectedClassNameType *)type, &result);
    break;
  case clang::Type::MacroQualified:
    ret_status =
        VisitMacroQualifiedType((clang::MacroQualifiedType *)type, &result);
    break;
  case clang::Type::MemberPointer:
    ret_status =
        VisitMemberPointerType((clang::MemberPointerType *)type, &result);
    break;
  case clang::Type::PackExpansion:
    ret_status =
        VisitPackExpansionType((clang::PackExpansionType *)type, &result);
    break;
  case clang::Type::Paren:
    ret_status = VisitParenType((clang::ParenType *)type, &result);
    break;
  case clang::Type::Pipe:
    ret_status = VisitPipeType((clang::PipeType *)type, &result);
    break;
  case clang::Type::Pointer:
    ret_status = VisitPointerType((clang::PointerType *)type, &result);
    break;
  case clang::Type::LValueReference:
    ret_status =
        VisitLValueReferenceType((clang::LValueReferenceType *)type, &result);
    break;
  case clang::Type::RValueReference:
    ret_status =
        VisitRValueReferenceType((clang::RValueReferenceType *)type, &result);
    break;
  case clang::Type::SubstTemplateTypeParmPack:
    ret_status = VisitSubstTemplateTypeParmPackType(
        (clang::SubstTemplateTypeParmPackType *)type, &result);
    break;
  case clang::Type::SubstTemplateTypeParm:
    ret_status = VisitSubstTemplateTypeParmType(
        (clang::SubstTemplateTypeParmType *)type, &result);
    break;
  case clang::Type::Enum:
    ret_status = VisitEnumType((clang::EnumType *)type, &result);
    break;
  case clang::Type::Record:
    ret_status = VisitRecordType((clang::RecordType *)type, &result);
    break;
  case clang::Type::TemplateSpecialization:
    ret_status = VisitTemplateSpecializationType(
        (clang::TemplateSpecializationType *)type, &result);
    break;
  case clang::Type::TemplateTypeParm:
    ret_status =
        VisitTemplateTypeParmType((clang::TemplateTypeParmType *)type, &result);
    break;
  case clang::Type::Typedef:
    ret_status = VisitTypedefType((clang::TypedefType *)type, &result);
    break;
  case clang::Type::TypeOfExpr:
    ret_status = VisitTypeOfExprType((clang::TypeOfExprType *)type, &result);
    break;
  case clang::Type::TypeOf:
    ret_status = VisitTypeOfType((clang::TypeOfType *)type, &result);
    break;
  case clang::Type::DependentName:
    ret_status =
        VisitDependentNameType((clang::DependentNameType *)type, &result);
    break;
  case clang::Type::UnaryTransform:
    ret_status =
        VisitUnaryTransformType((clang::UnaryTransformType *)type, &result);
    break;
  case clang::Type::UnresolvedUsing:
    ret_status =
        VisitUnresolvedUsingType((clang::UnresolvedUsingType *)type, &result);
    break;
  case clang::Type::Vector:
    ret_status = VisitVectorType((clang::VectorType *)type, &result);
    break;
  case clang::Type::ExtVector:
    ret_status = VisitExtVectorType((clang::ExtVectorType *)type, &result);
    break;
  case clang::Type::Using:
    ret_status = VisitUsingType((clang::UsingType *)type, &result);
    break;
  case clang::Type::PredefinedSugar:
    ret_status = true;
    result = buildTypeFromQualifiedType(
        ((clang::PredefinedSugarType *)type)->desugar());
    break;

  default:
    failExactTypeTranslation("unhandled-type-class", type);
  }

  if (!ret_status) {
    failExactTypeTranslation("type-visitor-status", type);
  }
  requireExactType(isSgType(result), "type-visitor-result", type);

  bool store_translation_in_cache = globally_cacheable;
  if (store_translation_in_cache &&
      type->getTypeClass() == clang::Type::Typedef &&
      isSgTypedefType(isSgType(result)) == nullptr) {
    store_translation_in_cache = false;
  }
  if (store_translation_in_cache &&
      type->getTypeClass() == clang::Type::Using) {
    SgType *sg_type = isSgType(result);
    if (isSgTypedefType(sg_type) == nullptr &&
        isSgNonrealType(sg_type) == nullptr) {
      store_translation_in_cache = false;
    }
  }

  if (!cache_translation) {
    return result;
  }
  if (store_translation_in_cache) {
    auto published = p_type_translation_map.emplace(type, result);
    if (!published.second && published.first->second != result) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[type-translation-cache]: Clang "
              "type=%p acquired two global Sage identities\n",
              static_cast<const void *>(type));
      ROSE_ABORT();
    }
  } else {
    auto published =
        p_contextual_type_translation_map.emplace(contextual_cache_key, result);
    if (!published.second && published.first->second != result) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[type-translation-cache]: Clang "
              "type=%p acquired two Sage identities in one exact template "
              "translation context\n",
              static_cast<const void *>(type));
      ROSE_ABORT();
    }
  }

  return result;
}

/***************/
/* Visit Types */
/***************/

bool ClangToSageTranslator::VisitType(clang::Type *type, SgNode **node) {
  if (*node == NULL) {
    std::cerr << "Runtime error: No Sage node associated with the type: "
              << type->getTypeClassName() << std::endl;
    return false;
  }
  return true;
}

bool ClangToSageTranslator::VisitAdjustedType(
    clang::AdjustedType *adjusted_type, SgNode **node) {
  ROSE_ASSERT(adjusted_type != nullptr);
  ROSE_ASSERT(node != nullptr);
  clang::QualType adjusted = adjusted_type->getAdjustedType();
  ROSE_ASSERT(!adjusted.isNull());
  *node = buildTypeFromQualifiedType(adjusted);

  return VisitType(adjusted_type, node);
}

bool ClangToSageTranslator::VisitDecayedType(clang::DecayedType *decayed_type,
                                             SgNode **node) {
  // Decay's adjusted type is the semantic pointer type. The base visitor
  // constructs it directly.
  return VisitAdjustedType(decayed_type, node);
}

bool ClangToSageTranslator::VisitArrayType(clang::ArrayType *array_type,
                                           SgNode **node) {
  ROSE_ASSERT(*node != NULL);
  if (array_type->getIndexTypeQualifiers().hasRestrict())
    *node = SageBuilder::buildRestrictType(isSgType(*node));

  return VisitType(array_type, node);
}

bool ClangToSageTranslator::VisitConstantArrayType(
    clang::ConstantArrayType *constant_array_type, SgNode **node) {
  SgType *type = requireExactType(
      buildTypeFromQualifiedType(constant_array_type->getElementType()),
      "constant-array-element-type", constant_array_type);

  *node = SageBuilder::buildArrayType(
      type, buildConstantArrayBound(constant_array_type));

  return VisitArrayType(constant_array_type, node);
}

bool ClangToSageTranslator::VisitDependentSizedArrayType(
    clang::DependentSizedArrayType *dependent_sized_array_type, SgNode **node) {
  SgType *type = requireExactType(
      buildTypeFromQualifiedType(dependent_sized_array_type->getElementType()),
      "dependent-array-element-type", dependent_sized_array_type);

  clang::Expr *size_expr = dependent_sized_array_type->getSizeExpr();
  if (size_expr != NULL) {
    SemanticExpressionConstruction semantic_expression(
        p_semantic_expression_subtree_depth, "dependent-array-bound");
    // Clang interns dependent bounds across templates. Resolve their parameter
    // coordinates in this use's typed owner before constructing the expression.
    llvm::SaveAndRestore<std::optional<const clang::DeclContext *>>
        canonical_context(p_canonical_template_argument_context,
                          dependent_sized_array_type->isCanonicalUnqualified()
                              ? std::optional<const clang::DeclContext *>(
                                    currentTemplateParameterDeclContext())
                              : p_canonical_template_argument_context);
    SgNode *tmp_expr = Traverse(size_expr);
    SgExpression *array_size =
        prepareExpressionForAttachment(isSgExpression(tmp_expr));
    requireExactType(array_size->get_type(), "dependent-array-size-type",
                     dependent_sized_array_type);
    *node = SageBuilder::buildArrayType(type, array_size);
  } else {
    // Size may be deduced from an initializer; represent as unknown-size array.
    *node = SageBuilder::buildArrayType(type);
  }

  return VisitArrayType(dependent_sized_array_type, node);
}

bool ClangToSageTranslator::VisitIncompleteArrayType(
    clang::IncompleteArrayType *incomplete_array_type, SgNode **node) {
  SgType *type = requireExactType(
      buildTypeFromQualifiedType(incomplete_array_type->getElementType()),
      "incomplete-array-element-type", incomplete_array_type);

  clang::ArraySizeModifier sizeModifier =
      incomplete_array_type->getSizeModifier();

  if (sizeModifier == clang::ArraySizeModifier::Star) {
    SgArrayType *arrayType = SageBuilder::buildArrayType(type);
    arrayType->set_is_variable_length_array(true);
    *node = arrayType;
  } else {
    ASSERT_require(sizeModifier == clang::ArraySizeModifier::Normal);
    *node = SageBuilder::buildArrayType(type);
  }

  return VisitArrayType(incomplete_array_type, node);
}

bool ClangToSageTranslator::VisitVariableArrayType(
    clang::VariableArrayType *variable_array_type, SgNode **node) {
  SgType *type = requireExactType(
      buildTypeFromQualifiedType(variable_array_type->getElementType()),
      "variable-array-element-type", variable_array_type);

  clang::Expr *size_expr = variable_array_type->getSizeExpr();
  SgExpression *array_size = nullptr;
  if (variable_array_type->getSizeModifier() ==
      clang::ArraySizeModifier::Star) {
    ASSERT_require(size_expr == nullptr);
  } else {
    if (size_expr == nullptr)
      failExactTypeTranslation("variable-array-size", variable_array_type);
    array_size =
        prepareExpressionForAttachment(isSgExpression(Traverse(size_expr)));
    requireExactType(array_size->get_type(), "variable-array-size-type",
                     variable_array_type);
  }

  SgArrayType *arrayType = SageBuilder::buildArrayType(type, array_size);
  arrayType->set_is_variable_length_array(true);
  *node = arrayType;

  return VisitArrayType(variable_array_type, node);
}

bool ClangToSageTranslator::VisitAtomicType(clang::AtomicType *atomic_type,
                                            SgNode **node) {
  SgType *base_type = buildTypeFromQualifiedType(atomic_type->getValueType());
  base_type = requireExactType(base_type, "atomic-value-type",
                               atomic_type->getValueType().getTypePtrOrNull());
  SgTypeModifier modifier;
  modifier.setAtomic();
  *node = SageBuilder::buildModifierType(base_type, modifier);

  return VisitType(atomic_type, node);
}

bool ClangToSageTranslator::VisitAttributedType(
    clang::AttributedType *attributed_type, SgNode **node) {
  SgType *type = buildTypeFromQualifiedType(attributed_type->getModifiedType());
  type =
      requireExactType(type, "attributed-modified-type",
                       attributed_type->getModifiedType().getTypePtrOrNull());
  SgTypeModifier exact_modifier;
  bool has_modifier = false;
  auto ensure_modifier = [&]() -> SgTypeModifier & {
    has_modifier = true;
    return exact_modifier;
  };

  switch (attributed_type->getAttrKind()) {
  case clang::attr::NoReturn:
    ensure_modifier().setGnuAttributeNoReturn();
    break;
  case clang::attr::CDecl:
    ensure_modifier().setGnuAttributeCdecl();
    break;
  case clang::attr::StdCall:
    ensure_modifier().setGnuAttributeStdcall();
    break;
  default:
    break;
  }

  if (has_modifier) {
    *node = SageBuilder::buildModifierType(type, exact_modifier);
  } else if (attributed_type->isQualifier()) {
    *node = type;
  } else {
    *node = buildTypeFromQualifiedType(attributed_type->getEquivalentType());
  }

  return VisitType(attributed_type, node);
}

bool ClangToSageTranslator::VisitBlockPointerType(
    clang::BlockPointerType *block_pointer_type, SgNode **node) {
  (void)node;
  // A block pointer has a distinct calling representation. Ordinary pointer
  // storage would silently change its ABI even without a block expression.
  failExactTypeTranslation("unsupported-block-pointer-type",
                           block_pointer_type);
}

bool ClangToSageTranslator::VisitBuiltinType(clang::BuiltinType *builtin_type,
                                             SgNode **node) {
  auto build_target_builtin = [&](SgTypeTargetBuiltin::target_family_enum
                                      target_family) {
    clang::PrintingPolicy policy(p_compiler_instance->getLangOpts());
    const std::string spelling = builtin_type->getName(policy).str();
    if (spelling.empty() || spelling.front() == '<' || spelling.back() == '>') {
      std::cerr << "REX_FRONTEND_INVARIANT[target-builtin-type]: Clang kind="
                << static_cast<unsigned>(builtin_type->getKind())
                << " has no exact target source spelling" << std::endl;
      ROSE_ABORT();
    }
    *node =
        SageBuilder::buildTargetBuiltinType(SgName(spelling), target_family);
  };

  switch (builtin_type->getKind()) {
  case clang::BuiltinType::Void:
    *node = SageBuilder::buildVoidType();
    break;
  case clang::BuiltinType::Bool:
    *node = SageBuilder::buildBoolType();
    break;
  case clang::BuiltinType::Short:
    *node = SageBuilder::buildShortType();
    break;
  case clang::BuiltinType::Int:
    *node = SageBuilder::buildIntType();
    break;
  case clang::BuiltinType::Long:
    *node = SageBuilder::buildLongType();
    break;
  case clang::BuiltinType::LongLong:
    *node = SageBuilder::buildLongLongType();
    break;
  case clang::BuiltinType::Float:
    *node = SageBuilder::buildFloatType();
    break;
  case clang::BuiltinType::Double:
    *node = SageBuilder::buildDoubleType();
    break;
  case clang::BuiltinType::LongDouble:
    *node = SageBuilder::buildLongDoubleType();
    break;
  case clang::BuiltinType::Half:
    *node = SageBuilder::buildFp16Type();
    break;
  case clang::BuiltinType::Float16:
    *node = SageBuilder::buildFloat16Type();
    break;
  case clang::BuiltinType::BFloat16:
    *node = SageBuilder::buildBFloat16Type();
    break;
  case clang::BuiltinType::Float128:
    *node = SageBuilder::buildFloat128Type();
    break;

  case clang::BuiltinType::Char_S:
    *node = SageBuilder::buildCharType();
    break;

  case clang::BuiltinType::UInt:
    *node = SageBuilder::buildUnsignedIntType();
    break;
  case clang::BuiltinType::UChar:
    *node = SageBuilder::buildUnsignedCharType();
    break;
  case clang::BuiltinType::SChar:
    *node = SageBuilder::buildSignedCharType();
    break;
  case clang::BuiltinType::UShort:
    *node = SageBuilder::buildUnsignedShortType();
    break;
  case clang::BuiltinType::ULong:
    *node = SageBuilder::buildUnsignedLongType();
    break;
  case clang::BuiltinType::ULongLong:
    *node = SageBuilder::buildUnsignedLongLongType();
    break;
  case clang::BuiltinType::NullPtr:
    *node = SageBuilder::buildNullptrType();
    break;
  case clang::BuiltinType::UInt128:
    *node = SageBuilder::buildUnsigned128bitIntegerType();
    break;
  case clang::BuiltinType::Int128:
    *node = SageBuilder::buildSigned128bitIntegerType();
    break;

  // Wide character and Unicode types map to their dedicated ROSE scalar
  // types so the unparser can preserve the source spelling.
  case clang::BuiltinType::Char_U:
    *node = SageBuilder::buildCharType();
    break;
  case clang::BuiltinType::WChar_U:
    *node = SageBuilder::buildWcharType();
    break;
  case clang::BuiltinType::WChar_S:
    *node = SageBuilder::buildWcharType();
    break;
  case clang::BuiltinType::Char8:
    *node = SageBuilder::buildChar8Type();
    break;
  case clang::BuiltinType::Char16:
    *node = SageBuilder::buildChar16Type();
    break;
  case clang::BuiltinType::Char32:
    *node = SageBuilder::buildChar32Type();
    break;

  case clang::BuiltinType::Dependent:
    *node = SgTypeDependent::createType();
    break;

  case clang::BuiltinType::OCLSampler:
  case clang::BuiltinType::OCLEvent:
  case clang::BuiltinType::OCLClkEvent:
  case clang::BuiltinType::OCLQueue:
  case clang::BuiltinType::OCLReserveID:
#define IMAGE_TYPE(ImgType, Id, SingletonId, Access, Suffix)                   \
  case clang::BuiltinType::Id:
#include <clang/Basic/OpenCLImageTypes.def>
#define EXT_OPAQUE_TYPE(ExtType, Id, Ext) case clang::BuiltinType::Id:
#include <clang/Basic/OpenCLExtensionTypes.def>
    build_target_builtin(SgTypeTargetBuiltin::e_target_builtin_opencl);
    break;

#define SVE_TYPE(Name, Id, SingletonId) case clang::BuiltinType::Id:
#include <clang/Basic/AArch64ACLETypes.def>
    build_target_builtin(SgTypeTargetBuiltin::e_target_builtin_aarch64);
    break;

#define PPC_VECTOR_TYPE(Name, Id, Size) case clang::BuiltinType::Id:
#include <clang/Basic/PPCTypes.def>
    build_target_builtin(SgTypeTargetBuiltin::e_target_builtin_powerpc);
    break;

#define RVV_TYPE(Name, Id, SingletonId) case clang::BuiltinType::Id:
#include <clang/Basic/RISCVVTypes.def>
    build_target_builtin(SgTypeTargetBuiltin::e_target_builtin_riscv);
    break;

#define WASM_TYPE(Name, Id, SingletonId) case clang::BuiltinType::Id:
#include <clang/Basic/WebAssemblyReferenceTypes.def>
    build_target_builtin(SgTypeTargetBuiltin::e_target_builtin_webassembly);
    break;

#define AMDGPU_TYPE(Name, Id, SingletonId, Width, Align)                       \
  case clang::BuiltinType::Id:
#include <clang/Basic/AMDGPUTypes.def>
    build_target_builtin(SgTypeTargetBuiltin::e_target_builtin_amdgpu);
    break;

#define HLSL_INTANGIBLE_TYPE(Name, Id, SingletonId) case clang::BuiltinType::Id:
#include <clang/Basic/HLSLIntangibleTypes.def>
    build_target_builtin(SgTypeTargetBuiltin::e_target_builtin_hlsl);
    break;

  case clang::BuiltinType::ObjCId:
  case clang::BuiltinType::ObjCClass:
  case clang::BuiltinType::ObjCSel:
  case clang::BuiltinType::Overload:
  case clang::BuiltinType::BoundMember:
  case clang::BuiltinType::UnknownAny:
  default: {
    failExactTypeTranslation("unsupported-builtin-type", builtin_type);
  }
  }

  return VisitType(builtin_type, node);
}

bool ClangToSageTranslator::VisitComplexType(clang::ComplexType *complex_type,
                                             SgNode **node) {

  SgType *type = requireExactType(
      buildTypeFromQualifiedType(complex_type->getElementType()),
      "complex-element-type", complex_type);

  *node = SageBuilder::buildComplexType(type);

  return VisitType(complex_type, node);
}

bool ClangToSageTranslator::VisitDecltypeType(
    clang::DecltypeType *decltype_type, SgNode **node) {

  clang::QualType underlying_type = decltype_type->getUnderlyingType();
  SgType *sg_underlying_type = nullptr;
  if (!underlying_type.isNull()) {
    sg_underlying_type = buildTypeFromQualifiedType(underlying_type);
  }

  SgType *sg_decltype = nullptr;
  bool is_dependent_decltype = decltype_type->isDependentType() ||
                               decltype_type->isInstantiationDependentType();
  bool preserve_decltype_expression =
      isSgAutoType(sg_underlying_type) != nullptr ||
      shouldPreserveDependentDecltypeExpression(decltype_type) ||
      decltypeNeedsExpressionPreservationForUnnamedType(underlying_type) ||
      is_dependent_decltype;

  // Preserve decltype(expr) when the deduced underlying type is still auto.
  // This is required for generic-lambda placeholders such as decltype(r),
  // where reducing to plain "auto" would lose semantics.
  if (preserve_decltype_expression) {
    const clang::Expr *underlying_expr = decltype_type->getUnderlyingExpr();
    if (underlying_expr == nullptr)
      failExactTypeTranslation("dependent-decltype-expression", decltype_type);
    SemanticExpressionConstruction semantic_expression(
        p_semantic_expression_subtree_depth, "semantic-decltype-expression");
    SgExpression *expr =
        isSgExpression(Traverse(const_cast<clang::Expr *>(underlying_expr)));
    if (expr == nullptr)
      failExactTypeTranslation("dependent-decltype-expression", decltype_type);
    SgExpression *expr_copy = prepareExpressionForAttachment(expr);
    SgType *exact_semantic_base = requireExactDecltypeSemanticBase(
        decltype_type, expr_copy, sg_underlying_type,
        "dependent-decltype-base");
    sg_decltype = SageBuilder::buildDeclType(expr_copy, exact_semantic_base);
  }

  if (sg_decltype != nullptr) {
    *node = sg_decltype;
  } else if (sg_underlying_type != nullptr) {
    *node = sg_underlying_type;
  } else {
    failExactTypeTranslation("decltype-underlying-type", decltype_type);
  }

  return VisitType(decltype_type, node);
}

bool ClangToSageTranslator::VisitDeducedType(clang::DeducedType *deduced_type,
                                             SgNode **node) {
  ASSERT_not_null(deduced_type);
  ASSERT_not_null(node);
  clang::QualType deduced = deduced_type->getDeducedType();
  if (deduced.isNull()) {
    failExactTypeTranslation("missing-deduction-result", deduced_type);
  }
  // A resolved placeholder has no written spelling for its result type: the
  // source surface is `auto`, `decltype(auto)`, or a deduced template
  // specialization.  Clang may nevertheless retain diagnostic sugar from
  // the expression that established the result, including a typedef local
  // to a different function body.  Publishing that sugar in the placeholder
  // owner's signature would create an out-of-scope Sage type reference and
  // can recursively require the body-local declaration while its owning
  // function is still being constructed.  The canonical QualType is the
  // exact externally visible semantic result of deduction.
  clang::QualType semantic_deduced = deduced.getCanonicalType();
  if (semantic_deduced.isNull()) {
    std::cerr << "REX_FRONTEND_INVARIANT[deduced-type]: resolved Clang "
                 "placeholder has no canonical semantic type"
              << std::endl;
    ROSE_ABORT();
  }
  *node = requireExactType(buildTypeFromQualifiedType(semantic_deduced),
                           "deduced-type", semantic_deduced.getTypePtrOrNull());

  return VisitType(deduced_type, node);
}

bool ClangToSageTranslator::VisitAutoType(clang::AutoType *auto_type,
                                          SgNode **node) {
  if (auto_type == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[auto-type-semantic]: null Clang "
                 "AutoType"
              << std::endl;
    ROSE_ABORT();
  }
  if (!auto_type->getDeducedType().isNull()) {
    return VisitDeducedType(auto_type, node);
  }
  if (auto_type->isConstrained()) {
    std::cerr << "REX_FRONTEND_INVARIANT[constrained-auto-source]: semantic "
                 "translation of an undeduced constrained auto requires its "
                 "exact TypeLoc"
              << std::endl;
    ROSE_ABORT();
  }
  *node = SageBuilder::buildAutoType();
  return VisitType(auto_type, node);
}

bool ClangToSageTranslator::VisitDeducedTemplateSpecializationType(
    clang::DeducedTemplateSpecializationType
        *deduced_template_specialization_type,
    SgNode **node) {
  ASSERT_not_null(deduced_template_specialization_type);
  ASSERT_not_null(node);

  clang::QualType deduced =
      deduced_template_specialization_type->getDeducedType();
  if (!deduced.isNull()) {
    return VisitDeducedType(deduced_template_specialization_type, node);
  }

  clang::TemplateName tname =
      deduced_template_specialization_type->getTemplateName();
  std::string base_name =
      getTemplateNameBase(tname, p_compiler_instance->getASTContext());

  SgScopeStatement *base_scope = SageBuilder::topScopeStack();
  ASSERT_not_null(base_scope);

  SgDeclarationStatement *template_identity =
      resolveNonrealTemplateDeclarationTarget(
          resolveTemplateNameDeclaration(tname), "undeduced-template-type");

  clang::NestedNameSpecifier qualifier = std::nullopt;
  if (const clang::QualifiedTemplateName *qtn =
          tname.getAsQualifiedTemplateName()) {
    qualifier = qtn->getQualifier();
  } else if (const clang::DependentTemplateName *dtn =
                 tname.getAsDependentTemplateName()) {
    qualifier = dtn->getQualifier();
  }

  if (qualifier) {
    *node = buildSemanticNonrealTypeFromNestedNameSpecifier(
        qualifier, base_scope, SgName(base_name), nullptr, template_identity);
  } else {
    *node = SageBuilder::buildSemanticNonrealType(SgName(base_name), base_scope,
                                                  nullptr, template_identity);
  }

  *node = requireExactType(isSgType(*node), "undeduced-template-type",
                           deduced_template_specialization_type);
  return VisitType(deduced_template_specialization_type, node);
}

bool ClangToSageTranslator::VisitDependentSizedExtVectorType(
    clang::DependentSizedExtVectorType *dependent_sized_ext_vector_type,
    SgNode **node) {
  (void)node;
  // Numeric vector storage cannot represent this unevaluated dimension.
  // Reject before publishing a type with a fabricated element count.
  failExactTypeTranslation("unsupported-dependent-vector-type",
                           dependent_sized_ext_vector_type);
}

bool ClangToSageTranslator::VisitDependentVectorType(
    clang::DependentVectorType *dependent_vector_type, SgNode **node) {
  (void)node;
  // Numeric vector storage cannot represent this unevaluated dimension.
  // Reject before publishing a type with a fabricated element count.
  failExactTypeTranslation("unsupported-dependent-vector-type",
                           dependent_vector_type);
}

bool ClangToSageTranslator::VisitFunctionNoProtoType(
    clang::FunctionNoProtoType *function_no_proto_type, SgNode **node) {

  auto param_type_list = std::make_unique<SgFunctionParameterTypeList>();

  SgType *ret_type = buildTypeFromQualifiedType(
      function_no_proto_type->getReturnType().getCanonicalType());

  auto *function_type =
      SageBuilder::buildFunctionType(ret_type, param_type_list.get(), false);
  if (function_type->get_argument_list() == param_type_list.get())
    param_type_list.release();
  else
    ROSE_ASSERT(param_type_list->get_parent() == nullptr);
  *node = function_type;

  return VisitType(function_no_proto_type, node);
}

bool ClangToSageTranslator::VisitFunctionProtoType(
    clang::FunctionProtoType *function_proto_type, SgNode **node) {
  auto param_type_list = std::make_unique<SgFunctionParameterTypeList>();
  for (unsigned i = 0; i < function_proto_type->getNumParams(); i++) {
    clang::QualType clang_param_type =
        p_compiler_instance->getASTContext().getCanonicalParamType(
            function_proto_type->getParamType(i));
    const clang::PackExpansionType *pack_expansion =
        clang_param_type->getAs<clang::PackExpansionType>();
    SgType *param_type = buildTypeFromQualifiedType(clang_param_type);

    param_type_list->append_argument(param_type);
    SgFunctionTypeArgument *argument_position =
        param_type_list->get_argument_qualification_use_sites().back();
    ASSERT_not_null(argument_position);
    argument_position->set_is_pack_expansion(pack_expansion != nullptr);
  }

  if (function_proto_type->isVariadic()) {
    param_type_list->append_argument(SgTypeEllipse::createType());
  }

  SgType *ret_type = buildTypeFromQualifiedType(
      function_proto_type->getReturnType().getCanonicalType());

  SgFunctionType *func_type = buildFunctionTypeForClangProto(
      ret_type, std::move(param_type_list), function_proto_type);

  *node = func_type;

  return VisitType(function_proto_type, node);
}

bool ClangToSageTranslator::VisitInjectedClassNameType(
    clang::InjectedClassNameType *injected_class_name_type, SgNode **node) {
  ASSERT_not_null(injected_class_name_type);
  ASSERT_not_null(node);
  clang::QualType specialization = getInjectedClassNameSpecializationType(
      injected_class_name_type, p_compiler_instance->getASTContext());
  *node = requireExactType(buildTypeFromQualifiedType(specialization),
                           "injected-class-name-specialization",
                           injected_class_name_type);

  return VisitType(injected_class_name_type, node);
}

bool ClangToSageTranslator::VisitMacroQualifiedType(
    clang::MacroQualifiedType *macro_qualified_type, SgNode **node) {
  *node = buildTypeFromQualifiedType(macro_qualified_type->getUnderlyingType());

  return VisitType(macro_qualified_type, node);
}

bool ClangToSageTranslator::VisitMemberPointerType(
    clang::MemberPointerType *member_pointer_type, SgNode **node) {
  SgType *classType = requireExactType(
      buildTypeFromQualifiedType(memberPointerClassType(member_pointer_type)),
      "member-pointer-class", member_pointer_type);
  SgType *classTypeStripped = classType->stripTypedefsAndModifiers();
  if (isSgClassType(classTypeStripped) == nullptr &&
      isSgNonrealType(classTypeStripped) == nullptr &&
      isSgTemplateType(classTypeStripped) == nullptr) {
    failExactTypeTranslation("member-pointer-class", member_pointer_type);
  }
  clang::QualType pointee_type = member_pointer_type->getPointeeType();
  SgType *baseType = requireExactType(buildTypeFromQualifiedType(pointee_type),
                                      "member-pointer-pointee",
                                      pointee_type.getTypePtrOrNull());
  if (member_pointer_type->isMemberFunctionPointer()) {
    const auto *proto = pointee_type->getAs<clang::FunctionProtoType>();
    ASSERT_not_null(proto);
    const unsigned int mfunc_specifier =
        roseMemberFunctionSpecifierFromClangProto(proto);
    SgFunctionType *functionType =
        isSgFunctionType(baseType->stripTypedefsAndModifiers());
    if (functionType == nullptr)
      failExactTypeTranslation("member-function-pointer-pointee",
                               pointee_type.getTypePtr());
    // Bind the semantic function even when Clang's pointee is a typedef.
    // The separate TypeLoc wrapper retains that alias's source spelling.
    baseType = SageBuilder::buildMemberFunctionType(functionType, classType,
                                                    mfunc_specifier);
    requireExactFunctionArgumentQualificationUseSites(
        isSgFunctionType(baseType), "qualified-type-member-function-pointer");
  }

  SgPointerMemberType *pointerToMemberType =
      SageBuilder::buildPointerMemberType(baseType, classType);

  *node = pointerToMemberType;

  return VisitType(member_pointer_type, node);
}

bool ClangToSageTranslator::VisitPackExpansionType(
    clang::PackExpansionType *pack_expansion_type, SgNode **node) {
  clang::QualType pattern = pack_expansion_type->getPattern();
  SgType *pattern_type = buildTypeFromQualifiedType(pattern);

  pattern_type = requireExactType(pattern_type, "pack-expansion-pattern",
                                  pattern.getTypePtrOrNull());
  if (p_semantic_template_argument_expression_depth == 0) {
    if (SgTemplateType *parameter_type = isSgTemplateType(pattern_type);
        parameter_type != nullptr && parameter_type->get_packed())
      pattern_type = buildUnexpandedTemplateParameterUse(parameter_type);
  }
  // A PackExpansionType is a use-site expansion.  The canonical pattern type
  // can be shared by non-expanding uses, so mutating it would leak an ellipsis
  // into every use of the declaration.  Expansion is represented by the exact
  // enclosing template argument/declarator syntax instead.
  *node = pattern_type;

  return VisitType(pack_expansion_type, node);
}

bool ClangToSageTranslator::VisitParenType(clang::ParenType *paren_type,
                                           SgNode **node) {

  *node =
      requireExactType(buildTypeFromQualifiedType(paren_type->getInnerType()),
                       "parenthesized-inner-type", paren_type);

  return VisitType(paren_type, node);
}

bool ClangToSageTranslator::VisitPipeType(clang::PipeType *pipe_type,
                                          SgNode **node) {
  SgType *elem_type = buildTypeFromQualifiedType(pipe_type->getElementType());
  elem_type = requireExactType(elem_type, "pipe-element-type",
                               pipe_type->getElementType().getTypePtrOrNull());
  *node = SageBuilder::buildPointerType(elem_type);

  return VisitType(pipe_type, node);
}

bool ClangToSageTranslator::VisitPointerType(clang::PointerType *pointer_type,
                                             SgNode **node) {
  SgType *type = requireExactType(
      buildTypeFromQualifiedType(pointer_type->getPointeeType()),
      "pointer-pointee-type", pointer_type);

  *node = SageBuilder::buildPointerType(type);

  return VisitType(pointer_type, node);
}

bool ClangToSageTranslator::VisitReferenceType(
    clang::ReferenceType *reference_type, SgNode **node) {
  ROSE_ASSERT(reference_type != nullptr);

  // A semantic QualType contains the already-collapsed reference.  Rebuilding
  // getPointeeTypeAsWritten() here preserved an inner reference from a
  // dependent spelling such as T& with T substituted by U&, manufacturing an
  // illegal SgReferenceType-to-SgReferenceType chain.  Written reference
  // structure belongs exclusively to buildTypeFromTypeLoc(); this visitor
  // must translate Clang's exact collapsed pointee.
  clang::QualType pointee_qual_type = reference_type->getPointeeType();
  if (pointee_qual_type.isNull() || pointee_qual_type->isReferenceType()) {
    std::cerr << "REX_FRONTEND_INVARIANT[reference-collapse]: Clang "
                 "semantic reference has no exact collapsed pointee; "
                 "reference='"
              << clang::QualType(reference_type, 0).getAsString()
              << "' pointee='" << pointee_qual_type.getAsString() << "'"
              << std::endl;
    ROSE_ABORT();
  }
  SgType *pointee_type = buildTypeFromQualifiedType(pointee_qual_type);
  SgType *semantic_pointee =
      pointee_type != nullptr
          ? pointee_type->stripType(SgType::STRIP_TYPEDEF_TYPE |
                                    SgType::STRIP_MODIFIER_TYPE)
          : nullptr;
  if (pointee_type == nullptr ||
      SageInterface::containsUnknownType(pointee_type) ||
      semantic_pointee == nullptr ||
      isSgReferenceType(semantic_pointee) != nullptr ||
      isSgRvalueReferenceType(semantic_pointee) != nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[reference-collapse]: collapsed "
                 "Clang pointee translated to "
              << (pointee_type != nullptr ? pointee_type->class_name()
                                          : std::string("<null>"))
              << " with semantic core "
              << (semantic_pointee != nullptr ? semantic_pointee->class_name()
                                              : std::string("<null>"))
              << std::endl;
    ROSE_ABORT();
  }

  if (clang::isa<clang::RValueReferenceType>(reference_type)) {
    *node = SageBuilder::buildRvalueReferenceType(pointee_type);
  } else {
    *node = SageBuilder::buildReferenceType(pointee_type);
  }

  return VisitType(reference_type, node);
}

bool ClangToSageTranslator::VisitLValueReferenceType(
    clang::LValueReferenceType *lvalue_reference_type, SgNode **node) {
  return VisitReferenceType(lvalue_reference_type, node);
}

bool ClangToSageTranslator::VisitRValueReferenceType(
    clang::RValueReferenceType *rvalue_reference_type, SgNode **node) {
  return VisitReferenceType(rvalue_reference_type, node);
}

bool ClangToSageTranslator::VisitSubstTemplateTypeParmPackType(
    clang::SubstTemplateTypeParmPackType *subst_template_type, SgNode **node) {
  const clang::TemplateTypeParmDecl *parameter =
      subst_template_type->getReplacedParameter();
  ASSERT_not_null(parameter);
  ROSE_ASSERT(parameter->isParameterPack());
  const clang::TemplateArgument arguments =
      subst_template_type->getArgumentPack();
  ROSE_ASSERT(arguments.getKind() == clang::TemplateArgument::Pack);
  for (const clang::TemplateArgument &argument : arguments.pack_elements())
    ROSE_ASSERT(argument.getKind() == clang::TemplateArgument::Type);
  auto *sage_parameter = lookupActiveTemplateParameterSurface(
      const_cast<clang::TemplateTypeParmDecl *>(parameter),
      "VisitSubstTemplateTypeParmPackType");
  if (sage_parameter == nullptr) {
    clang::Decl *owner = subst_template_type->getAssociatedDecl();
    ASSERT_not_null(owner);
    ASSERT_not_null(lookupSgDeclarationForClangDecl(owner,
                                                    /*allow_on_demand=*/true));
    sage_parameter = lookupPublishedTemplateTypeParameterFamily(
        parameter, "VisitSubstTemplateTypeParmPackType",
        /*require_exact_owner=*/true);
  }
  ASSERT_not_null(sage_parameter);
  SgTemplateType *pack_type = SageBuilder::buildTemplateType(
      SgName(clangTemplateParameterSpelling(parameter)));
  ASSERT_not_null(pack_type);
  pack_type->set_template_parameter_depth(parameter->getDepth());
  pack_type->set_template_parameter_position(parameter->getIndex());
  publishCanonicalTemplateParameterSourceIdentity(
      pack_type, const_cast<clang::TemplateTypeParmDecl *>(parameter),
      "VisitSubstTemplateTypeParmPackType");
  pack_type->set_template_parameter(sage_parameter);

  // Substitution owns typed arguments, not a source ellipsis. An unnamed
  // parameter retains its empty spelling and exact declaration identity.
  pack_type->set_is_pack_substitution(true);
  for (const clang::TemplateArgument &argument : arguments.pack_elements()) {
    SgTemplateArgument *translated = translateTemplateArgument(argument, false);
    ASSERT_not_null(translated);
    ROSE_ASSERT(translated->get_argumentType() ==
                SgTemplateArgument::type_argument);
    requireExactType(translated->get_type(),
                     "VisitSubstTemplateTypeParmPackType", subst_template_type);
    ROSE_ASSERT(translated->get_parent() == nullptr);
    translated->set_parent(pack_type);
    pack_type->get_tpl_args().push_back(translated);
  }
  *node = pack_type;

  return VisitType(subst_template_type, node);
}

bool ClangToSageTranslator::VisitSubstTemplateTypeParmType(
    clang::SubstTemplateTypeParmType *subst_template_type_parm_type,
    SgNode **node) {
  clang::QualType replacement_type =
      subst_template_type_parm_type->getReplacementType();
  *node = buildTypeFromQualifiedType(replacement_type);

  return VisitType(subst_template_type_parm_type, node);
}

bool ClangToSageTranslator::VisitEnumType(clang::EnumType *enum_type,
                                          SgNode **node) {

  auto canonical_enum_decl =
      [](SgEnumDeclaration *decl) -> SgEnumDeclaration * {
    if (decl == nullptr) {
      return nullptr;
    }
    SgEnumDeclaration *first =
        isSgEnumDeclaration(decl->get_firstNondefiningDeclaration());
    if (first == nullptr || first->get_firstNondefiningDeclaration() != first) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[enum-type-canonical-owner]: enum=%p "
              "has no exact canonical declaration\n",
              static_cast<void *>(decl));
      ROSE_ABORT();
    }
    return first;
  };
  using Rose::AST::namedTypeScopeHasCrossTranslationUnitIdentity;
  auto is_exact_cross_translation_unit_enum_identity =
      [&](SgEnumDeclaration *current, SgEnumDeclaration *type_owner,
          SgEnumType *type) {
        if (current == nullptr || type_owner == nullptr || type == nullptr ||
            current == type_owner || current->get_type() != type ||
            type_owner->get_type() != type || current->get_isUnNamed() ||
            type_owner->get_isUnNamed() || current->get_name().is_null() ||
            type_owner->get_name().is_null() ||
            current->get_isScopedEnum() != type_owner->get_isScopedEnum() ||
            type->get_declaration() != type_owner ||
            !namedTypeScopeHasCrossTranslationUnitIdentity(
                current->get_scope()) ||
            !namedTypeScopeHasCrossTranslationUnitIdentity(
                type_owner->get_scope())) {
          return false;
        }
        SgGlobal *current_global = SageInterface::getGlobalScope(current);
        SgGlobal *owner_global = SageInterface::getGlobalScope(type_owner);
        SgSourceFile *current_file =
            current_global != nullptr
                ? isSgSourceFile(current_global->get_parent())
                : nullptr;
        SgSourceFile *owner_file =
            owner_global != nullptr ? isSgSourceFile(owner_global->get_parent())
                                    : nullptr;
        return current_global != nullptr && owner_global != nullptr &&
               current_global != owner_global && current_file != nullptr &&
               owner_file != nullptr && current_file != owner_file;
      };
  clang::EnumDecl *clang_decl = enum_type->getDecl();
  if (clang_decl == nullptr) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[enum-type]: Clang enum type has no exact "
            "declaration\n");
    ROSE_ABORT();
  }

  SgDeclarationStatement *translated =
      lookupSgDeclarationForClangDecl(clang_decl,
                                      /*allow_on_demand=*/false);
  if (translated == nullptr) {
    translated = lookupSgDeclarationForClangDecl(clang_decl,
                                                 /*allow_on_demand=*/true);
  }

  SgEnumDeclaration *sg_decl =
      canonical_enum_decl(isSgEnumDeclaration(translated));
  SgEnumType *exact_type =
      isSgEnumType(sg_decl != nullptr ? sg_decl->get_type() : nullptr);
  SgEnumDeclaration *type_owner = exact_type != nullptr
                                      ? canonical_enum_decl(isSgEnumDeclaration(
                                            exact_type->get_declaration()))
                                      : nullptr;
  const bool exact_local_family = type_owner == sg_decl;
  const bool exact_external_identity =
      is_exact_cross_translation_unit_enum_identity(sg_decl, type_owner,
                                                    exact_type);
  if (sg_decl == nullptr || exact_type == nullptr ||
      (!exact_local_family && !exact_external_identity)) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[enum-type]: Clang enum declaration=%p/%s "
            "translated=%p/%s canonical=%p type=%p owner=%p without one "
            "exact declaration-family type\n",
            static_cast<void *>(clang_decl),
            clang_decl->getQualifiedNameAsString().c_str(),
            static_cast<void *>(translated),
            translated != nullptr ? translated->class_name().c_str() : "<null>",
            static_cast<void *>(sg_decl), static_cast<void *>(exact_type),
            static_cast<void *>(exact_type != nullptr
                                    ? exact_type->get_declaration()
                                    : nullptr));
    ROSE_ABORT();
  }

  *node = exact_type;

  return VisitType(enum_type, node);
}

bool ClangToSageTranslator::VisitRecordType(clang::RecordType *record_type,
                                            SgNode **node) {
  if (record_type == nullptr || node == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[record-type]: null Clang record type "
                 "or output slot"
              << std::endl;
    ROSE_ABORT();
  }

  clang::RecordDecl *record_decl = record_type->getDecl();
  if (record_decl == nullptr) {
    std::cerr
        << "REX_FRONTEND_INVARIANT[record-type]: record type has no declaration"
        << std::endl;
    ROSE_ABORT();
  }

  const bool is_specialization =
      llvm::isa<clang::ClassTemplateSpecializationDecl>(record_decl) ||
      llvm::isa<clang::ClassTemplatePartialSpecializationDecl>(record_decl);

  if (is_specialization && p_explicit_template_id_type_use_depth != 0) {
    SgScopeStatement *scope = SageBuilder::topScopeStack();
    if (scope == nullptr) {
      scope = getGlobalScope();
    }
    SgNonrealType *type_use = buildNonrealTypeForNestedNameSpecifierType(
        record_type, scope, /*prefer_current_scope=*/false);
    if (type_use == nullptr) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[explicit-template-id-type-use]: "
              "specialization=%s has no exact declaration-local type use\n",
              record_decl->getQualifiedNameAsString().c_str());
      ROSE_ABORT();
    }
    *node = type_use;
    return VisitType(record_type, node);
  }

  // Ordinary record types use the canonical declaration as their semantic
  // identity. A specialization declaration is already the exact identity and
  // must not be replaced by its primary template.
  clang::RecordDecl *lookup_decl =
      is_specialization ? record_decl
                        : llvm::dyn_cast_or_null<clang::RecordDecl>(
                              record_decl->getCanonicalDecl());
  if (lookup_decl == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[record-type]: record declaration has "
                 "no exact canonical declaration"
              << std::endl;
    ROSE_ABORT();
  }

  const bool active_source_declarator_tag =
      activeSourceDeclaratorTagScope(lookup_decl) != nullptr;
  SgDeclarationStatement *translated = lookupSgDeclarationForClangDecl(
      lookup_decl, /*allow_on_demand=*/active_source_declarator_tag);
  SgClassDeclaration *class_decl =
      translatedRecordTypeDeclaration(record_decl, translated);

  if (class_decl == nullptr) {
    bool defer_specialization_population = false;
    clang::ClassTemplateSpecializationDecl *specialization =
        llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(lookup_decl);
    if (specialization != nullptr &&
        !llvm::isa<clang::ClassTemplatePartialSpecializationDecl>(
            specialization)) {
      clang::ClassTemplateDecl *primary =
          specialization->getSpecializedTemplate();
      const clang::CXXRecordDecl *pattern =
          primary != nullptr ? llvm::cast_or_null<clang::CXXRecordDecl>(
                                   primary->getTemplatedDecl())
                             : nullptr;
      defer_specialization_population =
          (primary != nullptr &&
           p_decl_translation_in_progress.count(primary) != 0) ||
          (pattern != nullptr &&
           p_decl_translation_in_progress.count(
               const_cast<clang::CXXRecordDecl *>(pattern)) != 0);
    }

    if (defer_specialization_population) {
      ++p_defer_on_demand_cxx_record_population_depth;
    }
    struct SpecializationPopulationGuard {
      unsigned &depth;
      bool active;
      ~SpecializationPopulationGuard() {
        if (active) {
          ROSE_ASSERT(depth > 0);
          --depth;
        }
      }
    } population_guard{p_defer_on_demand_cxx_record_population_depth,
                       defer_specialization_population};
    translated =
        lookupSgDeclarationForClangDecl(lookup_decl, /*allow_on_demand=*/true);
    class_decl = translatedRecordTypeDeclaration(record_decl, translated);
    if (defer_specialization_population) {
      queuePendingImplicitClassTemplateSpecialization(specialization);
    }
  }

  if (class_decl == nullptr) {
    clang::SourceRange lookup_range = lookup_decl->getSourceRange();
    clang::SourceLocation lookup_location = lookup_decl->getLocation();
    SgScopeStatement *body_owner =
        !p_function_body_translation_owner_stack.empty()
            ? p_function_body_translation_owner_stack.back().sage_owner
            : nullptr;
    for (const FunctionBodyTranslationOwner &frame :
         p_function_body_translation_owner_stack) {
      std::cerr << "  function-body-frame clang=" << frame.clang_declaration
                << "/"
                << (frame.clang_declaration != nullptr
                        ? frame.clang_declaration->getQualifiedNameAsString()
                        : std::string("<null>"))
                << " sage=" << frame.sage_owner << "/"
                << (frame.sage_owner != nullptr ? frame.sage_owner->class_name()
                                                : std::string("<null>"))
                << std::endl;
    }
    std::cerr << "REX_FRONTEND_INVARIANT[record-type]: Clang "
              << llvm::cast<clang::Decl>(lookup_decl)->getDeclKindName()
              << " declaration '" << lookup_decl->getQualifiedNameAsString()
              << "' has no exact translated SgClassDeclaration; in-progress="
              << (p_decl_translation_in_progress.find(lookup_decl) !=
                          p_decl_translation_in_progress.end()
                      ? "true"
                      : "false")
              << " on-demand="
              << (p_decl_translation_on_demand.find(lookup_decl) !=
                          p_decl_translation_on_demand.end()
                      ? "true"
                      : "false")
              << " translated=" << translated << "/"
              << (translated != nullptr ? translated->class_name()
                                        : std::string("<null>"))
              << " rebuild-depth=" << p_rebuild_translation_cache_depth
              << " body-owner=" << body_owner << " body-stack="
              << p_function_body_translation_owner_stack.size()
              << " begin=" << lookup_range.getBegin().getRawEncoding()
              << " location=" << lookup_location.getRawEncoding()
              << " end=" << lookup_range.getEnd().getRawEncoding()
              << " pending-body-completions="
              << p_pending_function_body_completions.size()
              << " pending-body-families="
              << p_pending_function_body_completions_set.size()
              << " active-body-completions="
              << p_active_function_body_completions.size() << std::endl;
    ROSE_ABORT();
  }

  if (is_specialization &&
      isSgTemplateInstantiationDecl(class_decl) == nullptr &&
      isSgTemplateClassDeclaration(class_decl) == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[record-type]: specialization '"
              << record_decl->getQualifiedNameAsString()
              << "' resolved to non-template declaration "
              << class_decl->class_name() << std::endl;
    ROSE_ABORT();
  }

  SgClassType *class_type = isSgClassType(class_decl->get_type());
  if (class_type == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[record-type]: exact declaration '"
              << lookup_decl->getQualifiedNameAsString()
              << "' has no SgClassType" << std::endl;
    ROSE_ABORT();
  }

  SgClassSymbol *class_symbol =
      isSgClassSymbol(class_decl->get_symbol_from_symbol_table());
  if (class_symbol == nullptr) {
    SgDeclarationStatement *source_surface = translated;
    if (SgTemplateInstantiationDirectiveStatement *directive =
            isSgTemplateInstantiationDirectiveStatement(source_surface)) {
      source_surface = directive->get_declaration();
    }
    SgClassDeclaration *source_class = isSgClassDeclaration(source_surface);
    if (source_class != nullptr && source_class != class_decl &&
        source_class->get_firstNondefiningDeclaration() == class_decl &&
        source_class->get_type() == class_type &&
        source_class->get_scope() != nullptr) {
      class_symbol = isSgClassSymbol(
          source_class->get_scope()->find_symbol_from_declaration(
              source_class));
    }
  }
  if (class_symbol == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[record-type]: exact declaration '"
              << lookup_decl->getQualifiedNameAsString()
              << "' has no SgClassSymbol" << std::endl;
    ROSE_ABORT();
  }
  if (class_symbol->get_declaration() == nullptr ||
      class_symbol->get_declaration()->get_type() != class_type) {
    std::cerr << "REX_FRONTEND_INVARIANT[record-type]: exact declaration '"
              << lookup_decl->getQualifiedNameAsString()
              << "' has a class symbol bound to a different canonical type"
              << std::endl;
    ROSE_ABORT();
  }
  if (is_specialization && isSgTemplateInstantiationDecl(
                               class_symbol->get_declaration()) == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[record-type]: specialization '"
              << record_decl->getQualifiedNameAsString()
              << "' is bound to a non-instantiation class symbol" << std::endl;
    ROSE_ABORT();
  }

  *node = class_type;

  return VisitType(record_type, node);
}

SgTemplateClassDeclaration *
ClangToSageTranslator::requireExactTemplateDeclaration(
    const clang::TemplateSpecializationType *clang_type) {
  if (clang_type == nullptr) {
    std::cerr
        << "REX_FRONTEND_INVARIANT[template-specialization-primary-identity]: "
           "template specialization has no Clang type"
        << std::endl;
    ROSE_ABORT();
  }

  clang::TemplateName template_name_node = clang_type->getTemplateName();
  clang::TemplateDecl *clang_template_decl =
      template_name_node.getAsTemplateDecl();
  if (clang_template_decl == nullptr) {
    std::cerr
        << "REX_FRONTEND_INVARIANT[template-specialization-primary-identity]: "
           "template specialization has no exact Clang TemplateDecl; name kind="
        << static_cast<int>(template_name_node.getKind()) << std::endl;
    ROSE_ABORT();
  }

  SgDeclarationStatement *mapped = lookupSgDeclarationForClangDecl(
      clang_template_decl, /*allow_on_demand=*/true);
  SgTemplateClassDeclaration *template_decl =
      isSgTemplateClassDeclaration(mapped);
  if (template_decl == nullptr) {
    std::cerr
        << "REX_FRONTEND_INVARIANT[template-specialization-primary-identity]: "
           "Clang "
        << clang_template_decl->getDeclKindName() << " '"
        << clang_template_decl->getQualifiedNameAsString()
        << "' did not publish one exact SgTemplateClassDeclaration"
        << std::endl;
    ROSE_ABORT();
  }

  return template_decl;
}

namespace {
static bool templateArgumentNeedsExplicitAddressOf(SgType *param_type,
                                                   SgExpression *expr) {
  ASSERT_not_null(expr);
  SgType *base_param_type =
      requireExactType(param_type, "template-address-parameter-type", nullptr)
          ->stripTypedefsAndModifiers();
  SgType *expr_type =
      requireExactType(expr->get_type(), "template-address-value-type", nullptr)
          ->stripTypedefsAndModifiers();
  if (isSgAddressOfOp(expr) != nullptr || isSgNullptrValExp(expr) != nullptr) {
    return false;
  }

  if (isSgPointerMemberType(base_param_type) != nullptr) {
    return true;
  }
  if (isSgPointerType(base_param_type) == nullptr) {
    return false;
  }

  if (isSgArrayType(expr_type) != nullptr ||
      isSgTypeString(expr_type) != nullptr) {
    return false;
  }

  return isSgPointerType(expr_type) == nullptr &&
         isSgPointerMemberType(expr_type) == nullptr;
}

size_t countExpandedTemplateArgument(const clang::TemplateArgument &arg) {

  if (arg.getKind() == clang::TemplateArgument::Pack) {
    size_t count = 0;
    const llvm::ArrayRef<clang::TemplateArgument> elements =
        arg.pack_elements();
    for (const clang::TemplateArgument &pack_arg : elements) {
      count += countExpandedTemplateArgument(pack_arg);
    }
    return count;
  }
  return 1;
}
} // namespace

SgExpression *
ClangToSageTranslator::buildTemplateDeclarationValue(SgExpression *expression,
                                                     SgType *parameter_type) {
  ASSERT_not_null(expression);
  requireExactType(parameter_type, "template-declaration-parameter-type",
                   nullptr);
  SgType *value_type = requireExactType(
      expression->get_type(), "template-declaration-value-type", nullptr);
  if (isSgArrayType(value_type->stripTypedefsAndModifiers()) != nullptr &&
      isSgPointerType(parameter_type->stripTypedefsAndModifiers()) != nullptr) {
    expression = SageBuilder::buildCastExp_nfi(
        expression, parameter_type, SgCastExp::e_implicit_cast,
        SgCastExp::e_semantic_conversion_ArrayToPointerDecay,
        SgCastExp::e_value_category_prvalue, {}, nullptr);
    publishCanonicalSemanticImplicitConversionProvenance(
        isSgCastExp(expression),
        "template-declaration-array-to-pointer-conversion");
    value_type = expression->get_type();
  }
  auto *reference = isSgReferenceType(parameter_type);
  if (reference != nullptr &&
      !SageInterface::isEquivalentType(reference->get_base_type(),
                                       value_type) &&
      SageInterface::isEquivalentType(
          reference->get_base_type()->stripTypedefsAndModifiers(),
          value_type->stripTypedefsAndModifiers())) {
    expression = SageBuilder::buildCastExp_nfi(
        expression, reference->get_base_type(), SgCastExp::e_implicit_cast,
        SgCastExp::e_semantic_conversion_NoOp,
        SgCastExp::e_value_category_lvalue, {}, nullptr);
    publishCanonicalSemanticImplicitConversionProvenance(
        isSgCastExp(expression),
        "template-declaration-qualification-conversion");
  }
  return expression;
}

SgTemplateArgument *ClangToSageTranslator::translateTemplateArgument(
    const clang::TemplateArgument &arg, bool explicitlySpecified,
    clang::NestedNameSpecifier source_qualifier) {

  if (arg.isPackExpansion()) {
    clang::TemplateArgument pattern = arg.getPackExpansionPattern();

    if (SgTemplateArgument *sg_pattern = translateTemplateArgument(
            pattern, explicitlySpecified, source_qualifier)) {
      sg_pattern->set_is_pack_element(true);
      return sg_pattern;
    }
  }

  SgTemplateArgument *sg_arg = nullptr;

  auto build_decl_expr = [&](clang::ValueDecl *decl) -> SgExpression * {
    if (decl == nullptr) {
      return nullptr;
    }

    SgSymbol *sym = GetSymbolFromSymbolTable(decl);
    if (sym == nullptr) {
      Traverse(decl);
      sym = GetSymbolFromSymbolTable(decl);
    }
    if (SgAliasSymbol *alias_sym = isSgAliasSymbol(sym)) {
      if (SgSymbol *base_sym = alias_sym->get_base()) {
        sym = base_sym;
      }
    }

    if (SgVariableSymbol *var_sym = isSgVariableSymbol(sym)) {
      SgExpression *expr = SageBuilder::buildVarRefExp_nfi(var_sym);
      publishCanonicalSemanticExpressionSourceProvenance(
          expr, "semantic-template-variable-reference");
      return expr;
    }
    if (SgMemberFunctionSymbol *member_sym = isSgMemberFunctionSymbol(sym)) {
      const auto *method = llvm::cast<clang::CXXMethodDecl>(decl);
      SgExpression *expr =
          method->isStatic()
              ? static_cast<SgExpression *>(
                    buildFunctionReference(member_sym, method->getType(), true))
              : SageBuilder::buildSemanticMemberFunctionRefExp(member_sym,
                                                               false, true);
      publishCanonicalSemanticExpressionSourceProvenance(
          expr, "semantic-template-member-function-reference");
      return expr;
    }
    if (SgFunctionSymbol *func_sym = isSgFunctionSymbol(sym)) {
      SgExpression *expr = buildFunctionReference(
          func_sym, llvm::cast<clang::ValueDecl>(decl)->getType(), true);
      publishCanonicalSemanticExpressionSourceProvenance(
          expr, "semantic-template-function-reference");
      return expr;
    }
    if (llvm::isa<clang::EnumConstantDecl>(decl)) {
      auto *enum_symbol = isSgEnumFieldSymbol(sym);
      ASSERT_not_null(enum_symbol);
      auto *enumerator = enum_symbol->get_declaration();
      ASSERT_not_null(enumerator);
      auto *enum_decl = isSgEnumDeclaration(enumerator->get_declptr());
      ASSERT_not_null(enum_decl);
      ROSE_ASSERT(enumerator->get_enum_constant_value().has_value());
      SgExpression *expr =
          SageBuilder::buildEnumVal_nfi(*enumerator->get_enum_constant_value(),
                                        enum_decl, enumerator->get_name());
      publishCanonicalSemanticExpressionSourceProvenance(
          expr, "semantic-template-enum-value");
      return expr;
    }

    return nullptr;
  };
  auto build_structural_expr = [&](const clang::APValue &value,
                                   SgType *value_type) -> SgExpression * {
    if (value.isInt()) {
      SgExpression *expression =
          buildIntegralTemplateArgExpr(value.getInt(), value_type);
      publishCanonicalSemanticExpressionSourceProvenance(
          expression, "semantic-template-structural-integral");
      return expression;
    }
    if (value.isFloat()) {
      SgExpression *expression = nullptr;
      if (isSgTypeFloat(value_type)) {
        expression = SageBuilder::buildFloatVal_nfi(
            value.getFloat().convertToFloat(), "");
      } else if (isSgTypeLongDouble(value_type)) {
        expression = SageBuilder::buildLongDoubleVal_nfi(
            floatingValueFromClang(value.getFloat()), "");
      } else if (isSgTypeFloat128(value_type)) {
        expression = SageBuilder::buildFloat128Val_nfi(
            floatingValueFromClang(value.getFloat()), "");
      } else if (isSgTypeDouble(value_type)) {
        expression = SageBuilder::buildDoubleVal_nfi(
            value.getFloat().convertToDouble(), "");
      } else {
        failExactTypeTranslation("semantic-template-structural-floating",
                                 nullptr);
      }
      publishCanonicalSemanticExpressionSourceProvenance(
          expression, "semantic-template-structural-floating");
      return expression;
    }
    if (value.isLValue()) {
      clang::APValue::LValueBase base = value.getLValueBase();
      if (value.isNullPointer()) {
        SgType *canonical_value_type =
            value_type != nullptr
                ? value_type->stripType(SgType::STRIP_MODIFIER_TYPE |
                                        SgType::STRIP_TYPEDEF_TYPE)
                : nullptr;
        if (value_type == nullptr ||
            (isSgPointerType(canonical_value_type) == nullptr &&
             isSgPointerMemberType(canonical_value_type) == nullptr &&
             isSgTypeNullptr(canonical_value_type) == nullptr)) {
          fprintf(stderr,
                  "REX_FRONTEND_INVARIANT[semantic-template-null-pointer]: "
                  "null lvalue APValue has no exact pointer or "
                  "pointer-to-member type\n");
          ROSE_ABORT();
        }
        SgNullptrValExp *null_pointer = SageBuilder::buildNullptrValExp_nfi();
        null_pointer->set_literal_type(value_type);
        null_pointer->set_literal_spelling_form(
            SgValueExp::e_literal_canonical_generated);
        publishCanonicalSemanticExpressionSourceProvenance(
            null_pointer, "semantic-template-null-lvalue");
        if (null_pointer->get_type() != value_type) {
          fprintf(stderr,
                  "REX_FRONTEND_INVARIANT[semantic-template-null-pointer]: "
                  "typed null lvalue expression did not retain its exact "
                  "APValue type\n");
          ROSE_ABORT();
        }
        return null_pointer;
      }
      if (const clang::ValueDecl *decl =
              base.dyn_cast<const clang::ValueDecl *>()) {
        return build_decl_expr(const_cast<clang::ValueDecl *>(decl));
      }
      if (const clang::Expr *expr = base.dyn_cast<const clang::Expr *>()) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[semantic-template-argument]: "
                "APValue lvalue base expression=%s has no exact declaration "
                "identity\n",
                expr->getStmtClassName());
        ROSE_ABORT();
      }
    }
    if (value.isMemberPointer()) {
      const clang::ValueDecl *member = value.getMemberPointerDecl();
      if (member == nullptr) {
        SgType *canonical_value_type =
            value_type != nullptr
                ? value_type->stripType(SgType::STRIP_MODIFIER_TYPE |
                                        SgType::STRIP_TYPEDEF_TYPE)
                : nullptr;
        if (value_type == nullptr ||
            isSgPointerMemberType(canonical_value_type) == nullptr) {
          fprintf(stderr,
                  "REX_FRONTEND_INVARIANT[semantic-template-member-pointer]: "
                  "null member-pointer APValue has no exact pointer-to-member "
                  "type\n");
          ROSE_ABORT();
        }
        SgNullptrValExp *null_member = SageBuilder::buildNullptrValExp_nfi();
        null_member->set_literal_type(value_type);
        null_member->set_literal_spelling_form(
            SgValueExp::e_literal_canonical_generated);
        publishCanonicalSemanticExpressionSourceProvenance(
            null_member, "semantic-template-null-member-pointer");
        if (null_member->get_type() != value_type) {
          fprintf(stderr,
                  "REX_FRONTEND_INVARIANT[semantic-template-member-pointer]: "
                  "typed null member-pointer expression did not retain its "
                  "exact APValue type\n");
          ROSE_ABORT();
        }
        return null_member;
      }
      SgExpression *member_reference =
          build_decl_expr(const_cast<clang::ValueDecl *>(member));
      if (member_reference == nullptr || value_type == nullptr) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[semantic-template-member-pointer]: "
                "member=%s reference=%p type=%p has no exact Sage identity\n",
                member->getQualifiedNameAsString().c_str(),
                static_cast<void *>(member_reference),
                static_cast<void *>(value_type));
        ROSE_ABORT();
      }
      SgExpression *address = SageBuilder::buildAddressOfOp_nfi(
          member_reference, value_type,
          SgAddressOfOp::e_address_operand_qualified_id);
      publishCanonicalSemanticExpressionSourceProvenance(
          address, "semantic-template-member-pointer");
      return address;
    }
    return nullptr;
  };
  auto build_semantic_address_of = [&](SgExpression *operand,
                                       SgType *result_type) -> SgExpression * {
    ASSERT_not_null(operand);
    ASSERT_not_null(result_type);
    // A canonical semantic expression selects member syntax for member
    // pointers.
    const auto form = isSgPointerMemberType(
                          result_type->stripTypedefsAndModifiers()) != nullptr
                          ? SgAddressOfOp::e_address_operand_qualified_id
                          : SgAddressOfOp::e_address_operand_object;
    SgExpression *address =
        SageBuilder::buildAddressOfOp_nfi(operand, result_type, form);
    publishCanonicalSemanticExpressionSourceProvenance(
        address, "semantic-template-address-of");
    return address;
  };
  auto build_template_decl_argument =
      [&](clang::TemplateName template_name) -> SgDeclarationStatement * {
    clang::TemplateDecl *template_decl = template_name.getAsTemplateDecl();
    SgDeclarationStatement *sg_decl = nullptr;
    if (template_decl != nullptr) {
      if (llvm::isa<clang::TemplateTemplateParmDecl>(template_decl)) {
        sg_decl = resolveNonrealTemplateDeclarationTarget(
            template_decl, "semantic-template-template-argument");
      } else {
        SgNode *traverse_result = nullptr;
        auto mapped = p_decl_translation_map.find(template_decl);
        if (mapped != p_decl_translation_map.end()) {
          traverse_result = mapped->second;
        } else {
          traverse_result = TraverseOnDemand(template_decl);
        }
        sg_decl = isSgDeclarationStatement(traverse_result);
      }
      ROSE_ASSERT(sg_decl != nullptr);
    }

    clang::NestedNameSpecifier qualifier = std::nullopt;
    std::string name_str;
    bool has_template_keyword = false;
    if (const clang::QualifiedTemplateName *qualified =
            template_name.getAsQualifiedTemplateName()) {
      qualifier = qualified->getQualifier();
      has_template_keyword = qualified->hasTemplateKeyword();
      const clang::TemplateDecl *qualified_decl =
          qualified->getUnderlyingTemplate().getAsTemplateDecl();
      name_str = qualified_decl
                     ? qualified_decl->getNameAsString()
                     : (template_decl ? template_decl->getNameAsString() : "");
    } else if (const clang::DependentTemplateName *dependent =
                   template_name.getAsDependentTemplateName()) {
      qualifier = dependent->getQualifier();
      has_template_keyword = true;
      name_str = getTemplateNameBase(template_name,
                                     p_compiler_instance->getASTContext());
    } else {
      name_str = template_decl ? template_decl->getNameAsString() : "";
    }

    if (source_qualifier) {
      qualifier = source_qualifier;
      name_str = getTemplateNameBase(template_name,
                                     p_compiler_instance->getASTContext());
    }
    if (qualifier && name_str.empty()) {
      fprintf(stderr, "REX_FRONTEND_INVARIANT[template-argument-qualifier]: "
                      "qualified template argument has no exact Clang name\n");
      ROSE_ABORT();
    }
    if (qualifier) {
      SgScopeStatement *scope = SageBuilder::topScopeStack();
      if (scope == nullptr) {
        scope = getGlobalScope();
      }
      SgNonrealType *nr_type = buildSemanticNonrealTypeFromNestedNameSpecifier(
          qualifier, scope, SgName(name_str), nullptr, sg_decl);
      ROSE_ASSERT(nr_type != nullptr);
      auto *nr_decl = isSgNonrealDecl(nr_type->get_declaration());
      ROSE_ASSERT(nr_decl != nullptr);
      ROSE_ASSERT(nr_decl->get_templateDeclaration() == sg_decl);
      nr_decl->set_has_template_keyword(has_template_keyword);
      sg_decl = nr_decl;
    }

    ROSE_ASSERT(sg_decl != nullptr);
    return sg_decl;
  };

  switch (arg.getKind()) {
  case clang::TemplateArgument::Type: {
    clang::QualType arg_qt = arg.getAsType();
    const clang::Type *argument_type = arg_qt.getTypePtrOrNull();
    ASSERT_not_null(argument_type);
    const bool requires_source_owned_dependent_identity =
        requiresSourceOwnedDependentTypeIdentity(arg_qt);
    clang::QualType semantic_qt = requires_source_owned_dependent_identity
                                      ? arg_qt
                                      : arg_qt.getCanonicalType();
    struct CanonicalArgumentTypeSurface {
      std::array<unsigned *, 5> depths;
      std::array<unsigned, 5> suspended{};

      explicit CanonicalArgumentTypeSurface(std::array<unsigned *, 5> values)
          : depths(values) {
        for (size_t index = 0; index < depths.size(); ++index) {
          suspended[index] = *depths[index];
          *depths[index] = 0;
        }
      }

      ~CanonicalArgumentTypeSurface() {
        for (size_t index = 0; index < depths.size(); ++index) {
          if (*depths[index] != 0) {
            fprintf(stderr,
                    "REX_FRONTEND_INVARIANT[canonical-template-argument-"
                    "surface]: role=%zu completed with unbalanced depth=%u\n",
                    index, *depths[index]);
            ROSE_ABORT();
          }
          *depths[index] = suspended[index];
        }
      }

      CanonicalArgumentTypeSurface(const CanonicalArgumentTypeSurface &) =
          delete;
      CanonicalArgumentTypeSurface &
      operator=(const CanonicalArgumentTypeSurface &) = delete;
    };
    SgType *arg_type = nullptr;
    if (requires_source_owned_dependent_identity) {
      // Clang structurally interns canonical dependent types.  A canonical
      // DecltypeType can therefore retain the expression from an equivalent
      // type written under a different declaration owner, including a local
      // parameter of an earlier requires-expression.  Traversing that
      // canonical expression here would attach the template argument to the
      // wrong owner.  The source type is already the complete semantic input:
      // retain its exact typedef/alias and expression ownership.
      arg_type = buildTypeFromQualifiedType(semantic_qt);
    } else {
      CanonicalArgumentTypeSurface canonical_surface(
          {&p_explicit_template_id_type_use_depth,
           &p_explicit_template_id_type_loc_use_depth,
           &p_reconstructed_template_argument_surface_depth,
           &p_force_written_template_specialization_depth,
           &p_force_written_named_type_qualification_depth});
      arg_type = buildTypeFromQualifiedType(semantic_qt);
    }
    requireExactType(arg_type, "template-argument-type-identity",
                     semantic_qt.getTypePtrOrNull());
    const bool reconstruct_source_surface =
        explicitlySpecified &&
        (p_reconstructed_template_argument_surface_depth != 0 ||
         p_explicit_template_id_type_loc_use_depth != 0);
    SgType *source_arg_type =
        reconstruct_source_surface
            ? buildExplicitTemplateIdTypeUseFromQualifiedType(
                  arg_qt, "translateTemplateArgument:explicit-type")
            : nullptr;
    if (const clang::TemplateSpecializationType *arg_tst =
            llvm::dyn_cast_or_null<clang::TemplateSpecializationType>(
                arg_qt.getTypePtrOrNull())) {
      if (SgNonrealType *source_nonreal = isSgNonrealType(source_arg_type)) {
        SgNonrealDecl *source_declaration =
            isSgNonrealDecl(source_nonreal->get_declaration());
        if (source_declaration == nullptr) {
          fprintf(stderr,
                  "REX_FRONTEND_INVARIANT[source-template-argument-type]: "
                  "template-id=%s source nonreal type has no exact "
                  "declaration\n",
                  getTemplateNameBase(arg_tst->getTemplateName(),
                                      p_compiler_instance->getASTContext())
                      .c_str());
          ROSE_ABORT();
        }

        clang::TemplateName template_name = arg_tst->getTemplateName();
        clang::TemplateDecl *template_declaration =
            resolveTemplateNameDeclaration(template_name);
        if (template_declaration != nullptr) {
          SgDeclarationStatement *template_identity =
              resolveNonrealTemplateDeclarationTarget(
                  template_declaration, "source-template-argument-type");
          SgDeclarationStatement *existing =
              normalizeNonrealTemplateDeclarationTarget(
                  source_declaration->get_templateDeclaration());
          if (auto *specialization = isSgTemplateInstantiationDecl(existing))
            existing = normalizeNonrealTemplateDeclarationTarget(
                specialization->get_templateDeclaration());
          if (auto *alias =
                  isSgTemplateInstantiationTypedefDeclaration(existing))
            existing = normalizeNonrealTemplateDeclarationTarget(
                alias->get_templateDeclaration());
          if (existing != template_identity) {
            fprintf(stderr,
                    "REX_FRONTEND_INVARIANT[source-template-argument-type]: "
                    "template-id=%s already links a distinct declaration "
                    "%p/%s instead of exact template identity=%p/%s\n",
                    getTemplateNameBase(template_name,
                                        p_compiler_instance->getASTContext())
                        .c_str(),
                    static_cast<void *>(existing),
                    existing != nullptr ? existing->class_name().c_str()
                                        : "<null>",
                    static_cast<void *>(template_identity),
                    template_identity->class_name().c_str());
            ROSE_ABORT();
          }
        }
      }
    }

    sg_arg = new SgTemplateArgument(
        arg_type, explicitlySpecified,
        source_arg_type != arg_type ? source_arg_type : nullptr);
    break;
  }

  case clang::TemplateArgument::Integral: {
    llvm::APSInt value = arg.getAsIntegral();
    clang::QualType integral_type = arg.getIntegralType();
    SgType *int_type = requireExactType(
        buildTypeFromQualifiedType(integral_type),
        "template-argument-integral-type", integral_type.getTypePtrOrNull());

    SgValueExp *value_expr = buildIntegralTemplateArgExpr(value, int_type);
    if (value_expr == nullptr ||
        SageInterface::containsUnknownType(value_expr->get_type()) ||
        !SageInterface::isEquivalentType(value_expr->get_type(), int_type)) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[template-argument-integral-type]: "
              "canonical integral argument has value-type=%p and converted "
              "parameter-type=%p that are not equivalent\n",
              static_cast<void *>(value_expr != nullptr ? value_expr->get_type()
                                                        : nullptr),
              static_cast<void *>(int_type));
      ROSE_ABORT();
    }
    publishCanonicalSemanticExpressionSourceProvenance(
        value_expr, "semantic-template-integral-value");
    sg_arg = new SgTemplateArgument(SgTemplateArgument::nontype_argument,
                                    false /*isArrayBoundUnknownType*/, int_type,
                                    value_expr, nullptr, explicitlySpecified);
    break;
  }

  case clang::TemplateArgument::Declaration: {
    clang::ValueDecl *decl = const_cast<clang::ValueDecl *>(
        llvm::dyn_cast_or_null<clang::ValueDecl>(arg.getAsDecl()));
    clang::QualType param_qual_type = arg.getParamTypeForDecl();
    SgType *param_type = requireExactType(
        buildTypeFromQualifiedType(param_qual_type),
        "template-argument-declaration", param_qual_type.getTypePtrOrNull());
    SgExpression *decl_expr = build_decl_expr(decl);
    ASSERT_not_null(decl_expr);
    if (templateArgumentNeedsExplicitAddressOf(param_type, decl_expr)) {
      decl_expr = build_semantic_address_of(decl_expr, param_type);
    }
    decl_expr = buildTemplateDeclarationValue(decl_expr, param_type);
    sg_arg = new SgTemplateArgument(SgTemplateArgument::nontype_argument,
                                    /*isArrayBoundUnknownType=*/false,
                                    /*type=*/param_type,
                                    /*expression=*/decl_expr,
                                    /*templateDeclaration=*/nullptr,
                                    explicitlySpecified);
    break;
  }

  case clang::TemplateArgument::NullPtr: {
    clang::QualType null_qual_type = arg.getNullPtrType();
    SgType *null_type = requireExactType(
        buildTypeFromQualifiedType(null_qual_type),
        "template-argument-null-pointer", null_qual_type.getTypePtrOrNull());
    SgExpression *null_expr = SageBuilder::buildNullptrValExp_nfi();
    publishCanonicalSemanticExpressionSourceProvenance(
        null_expr, "semantic-template-null-pointer");
    sg_arg = new SgTemplateArgument(SgTemplateArgument::nontype_argument,
                                    /*isArrayBoundUnknownType=*/false,
                                    /*type=*/null_type,
                                    /*expression=*/null_expr,
                                    /*templateDeclaration=*/nullptr,
                                    explicitlySpecified);
    break;
  }

  case clang::TemplateArgument::StructuralValue: {
    clang::QualType structural_qual_type = arg.getStructuralValueType();
    SgType *value_type =
        requireExactType(buildTypeFromQualifiedType(structural_qual_type),
                         "template-argument-structural-value",
                         structural_qual_type.getTypePtrOrNull());
    const clang::APValue &value = arg.getAsStructuralValue();
    SgExpression *value_expr = build_structural_expr(value, value_type);
    if (value_expr != nullptr &&
        templateArgumentNeedsExplicitAddressOf(value_type, value_expr)) {
      value_expr = build_semantic_address_of(value_expr, value_type);
    }
    if (value_expr != nullptr) {
      sg_arg = new SgTemplateArgument(SgTemplateArgument::nontype_argument,
                                      /*isArrayBoundUnknownType=*/false,
                                      /*type=*/value_type,
                                      /*expression=*/value_expr,
                                      /*templateDeclaration=*/nullptr,
                                      explicitlySpecified);
    }
    break;
  }

  case clang::TemplateArgument::Template:
  case clang::TemplateArgument::TemplateExpansion: {
    clang::TemplateName template_name = arg.getAsTemplateOrTemplatePattern();
    SgDeclarationStatement *sg_decl =
        build_template_decl_argument(template_name);
    sg_arg =
        new SgTemplateArgument(SgTemplateArgument::template_template_argument,
                               /*isArrayBoundUnknownType=*/false,
                               /*type=*/nullptr,
                               /*expression=*/nullptr,
                               /*templateDeclaration=*/sg_decl,
                               /*explicitlySpecified=*/explicitlySpecified);
    break;
  }

  case clang::TemplateArgument::Expression: {
    clang::Expr *clang_expr = const_cast<clang::Expr *>(arg.getAsExpr());
    if (clang_expr != nullptr) {
      const bool canonical_expression =
          p_semantic_template_argument_expression_depth != 0 &&
          arg.isCanonicalExpr();
      // Canonical dependent arguments use depth/index coordinates rather than
      // retaining a distinct parameter declaration for every use context.
      llvm::SaveAndRestore<std::optional<const clang::DeclContext *>>
          canonical_context(p_canonical_template_argument_context,
                            canonical_expression
                                ? std::optional<const clang::DeclContext *>(
                                      currentTemplateParameterDeclContext())
                                : std::nullopt);
      SgExpression *sg_expr = nullptr;
      SgType *expression_type = nullptr;
      const bool dependent_expression =
          clang_expr->isTypeDependent() || clang_expr->isValueDependent() ||
          clang_expr->isInstantiationDependent() ||
          clang_expr->containsUnexpandedParameterPack();
      const bool preserve_source_spelling =
          p_semantic_template_argument_expression_depth == 0;
      const clang::Expr *source_surface_expression = clang_expr;
      std::vector<const clang::ImplicitCastExpr *> semantic_conversions;
      while (true) {
        const clang::Expr *wrapped = nullptr;
        if (const clang::ImplicitCastExpr *implicit =
                llvm::dyn_cast<clang::ImplicitCastExpr>(
                    source_surface_expression)) {
          semantic_conversions.push_back(implicit);
          wrapped = implicit->getSubExpr();
        } else if (const clang::ConstantExpr *constant =
                       llvm::dyn_cast<clang::ConstantExpr>(
                           source_surface_expression)) {
          wrapped = constant->getSubExpr();
        } else {
          break;
        }
        source_surface_expression = wrapped;
        if (source_surface_expression == nullptr) {
          fprintf(stderr,
                  "REX_FRONTEND_INVARIANT[semantic-template-argument-source]: "
                  "implicit semantic wrapper has no exact source operand\n");
          ROSE_ABORT();
        }
      }
      const clang::DeclRefExpr *direct_source_reference =
          preserve_source_spelling && !dependent_expression
              ? llvm::dyn_cast<clang::DeclRefExpr>(source_surface_expression)
              : nullptr;
      SgExpression *source_expression =
          preserve_source_spelling && direct_source_reference == nullptr
              ? isSgExpression(Traverse(
                    const_cast<clang::Expr *>(source_surface_expression)))
              : nullptr;
      SgExpression *semantic_source_owner = nullptr;
      if (dependent_expression) {
        sg_expr = source_expression != nullptr
                      ? source_expression
                      : isSgExpression(Traverse(const_cast<clang::Expr *>(
                            source_surface_expression)));
        if (sg_expr == nullptr) {
          fprintf(stderr,
                  "REX_FRONTEND_INVARIANT[semantic-template-conversion]: "
                  "dependent expression=%s has no exact typed source "
                  "operand\n",
                  clang_expr->getStmtClassName());
          ROSE_ABORT();
        }
        // Without an explicit Clang conversion, the translated dependent
        // expression owns its exact unresolved type identity. Clang can report
        // a more concrete contextual type for a dependent declaration
        // reference, but there is no conversion node that authorizes replacing
        // the source expression's typed identity. When an ImplicitCastExpr is
        // present, its outer type is the exact semantic argument type.
        expression_type =
            semantic_conversions.empty()
                ? sg_expr->get_type()
                : buildTypeFromQualifiedType(clang_expr->getType());
        if (SageInterface::containsUnknownType(expression_type)) {
          fprintf(stderr,
                  "REX_FRONTEND_INVARIANT[semantic-template-conversion]: "
                  "dependent expression=%s has no exact semantic result "
                  "type\n",
                  clang_expr->getStmtClassName());
          ROSE_ABORT();
        }
        for (auto conversion = semantic_conversions.rbegin();
             conversion != semantic_conversions.rend(); ++conversion) {
          const clang::ImplicitCastExpr *implicit = *conversion;
          SgType *conversion_type =
              buildTypeFromQualifiedType(implicit->getType());
          if (SageInterface::containsUnknownType(conversion_type)) {
            fprintf(stderr,
                    "REX_FRONTEND_INVARIANT[semantic-template-conversion]: "
                    "dependent cast kind=%s has no exact result type\n",
                    implicit->getCastKindName());
            ROSE_ABORT();
          }
          sg_expr = SageBuilder::buildCastExp_nfi(
              sg_expr, conversion_type, SgCastExp::e_implicit_cast,
              translateClangCastKind(implicit->getCastKind()),
              translateClangValueCategory(implicit->getValueKind()), {},
              nullptr);
          publishCanonicalSemanticImplicitConversionProvenance(
              isSgCastExp(sg_expr),
              "semantic-template-dependent-implicit-conversion");
        }
        if (sg_expr->get_type() != expression_type &&
            !SageInterface::cxxSourceTypeMatchesSemanticType(
                sg_expr->get_type(), expression_type)) {
          fprintf(stderr,
                  "REX_FRONTEND_INVARIANT[semantic-template-conversion]: "
                  "dependent expression=%s/%s source=%s/%s converted "
                  "expression type=%p/%s does not match semantic argument "
                  "type=%p/%s conversion-count=%zu\n",
                  clang_expr->getStmtClassName(),
                  clang_expr->getType().getAsString().c_str(),
                  source_surface_expression->getStmtClassName(),
                  source_surface_expression->getType().getAsString().c_str(),
                  static_cast<void *>(sg_expr->get_type()),
                  sg_expr->get_type()->class_name().c_str(),
                  static_cast<void *>(expression_type),
                  expression_type->class_name().c_str(),
                  semantic_conversions.size());
          for (const clang::ImplicitCastExpr *implicit : semantic_conversions) {
            fprintf(stderr,
                    "REX_FRONTEND_INVARIANT[semantic-template-conversion]: "
                    "implicit kind=%s type=%s value-kind=%d\n",
                    implicit->getCastKindName(),
                    implicit->getType().getAsString().c_str(),
                    static_cast<int>(implicit->getValueKind()));
          }
          ROSE_ABORT();
        }
      } else {
        clang::Expr::EvalResult evaluation;
        // TemplateArgument::Expression retains the source expression's value
        // category, but a non-type template argument is represented by its
        // converted constant value. In particular, a constant variable such
        // as `variant_npos` remains a DeclRefExpr glvalue even though the
        // integral template parameter consumes its lvalue-to-rvalue result.
        // Evaluate the semantic constant value; address and reference
        // arguments are published by Clang as declaration/structural
        // arguments and retain their typed lvalue identity on those paths.
        const bool resolved_declaration_identity =
            p_resolved_reference_current_semantic_argument_kind.has_value() &&
            *p_resolved_reference_current_semantic_argument_kind ==
                clang::TemplateArgument::Declaration;
        const bool evaluate_as_lvalue =
            clang_expr->isGLValue() &&
            (!p_resolved_reference_current_semantic_argument_kind.has_value() ||
             resolved_declaration_identity);
        const bool evaluated =
            evaluate_as_lvalue
                ? clang_expr->EvaluateAsLValue(
                      evaluation, p_compiler_instance->getASTContext())
                : clang_expr->EvaluateAsRValue(
                      evaluation, p_compiler_instance->getASTContext());
        if (!evaluated) {
          fprintf(stderr,
                  "REX_FRONTEND_INVARIANT[semantic-template-argument]: "
                  "expression=%s category=%s is not an exact constant value\n",
                  clang_expr->getStmtClassName(),
                  evaluate_as_lvalue ? "resolved-declaration-glvalue"
                                     : "resolved-value");
          ROSE_ABORT();
        }
        // Evaluated values require the resolved semantic type. Source-facing
        // aliases can retain nonreal template identities even when Clang has
        // resolved their underlying type (including Boolean constants).
        // The independently owned source expression retains that spelling.
        clang::QualType canonical_expression_type =
            clang_expr->getType().getCanonicalType();
        expression_type = requireExactType(
            buildTypeFromQualifiedType(canonical_expression_type),
            "template-argument-constant-expression",
            canonical_expression_type.getTypePtrOrNull());
        SgType *semantic_value_type = expression_type;
        clang::QualType source_surface_qual_type =
            source_surface_expression->getType();
        const bool declaration_identity_conversion =
            llvm::isa<clang::DeclRefExpr>(source_surface_expression) &&
            !semantic_conversions.empty() &&
            (source_surface_qual_type->isArrayType() ||
             source_surface_qual_type->isFunctionType());
        if (declaration_identity_conversion) {
          sg_expr = isSgExpression(
              Traverse(const_cast<clang::Expr *>(source_surface_expression)));
          semantic_value_type = requireExactType(
              buildTypeFromQualifiedType(source_surface_qual_type),
              "template-argument-declaration-surface",
              source_surface_qual_type.getTypePtrOrNull());
          if (sg_expr == nullptr || sg_expr->get_parent() != nullptr ||
              sg_expr->get_originalExpressionTree() != nullptr ||
              (sg_expr->get_type() != semantic_value_type &&
               !SageInterface::cxxSourceTypeMatchesSemanticType(
                   sg_expr->get_type(), semantic_value_type))) {
            fprintf(stderr,
                    "REX_FRONTEND_INVARIANT[semantic-template-conversion]: "
                    "declaration identity expression=%s has no exact typed "
                    "pre-conversion surface\n",
                    source_surface_expression->getStmtClassName());
            ROSE_ABORT();
          }
        }
        if (sg_expr == nullptr) {
          sg_expr = build_structural_expr(evaluation.Val, expression_type);
          if (sg_expr == nullptr) {
            fprintf(stderr,
                    "REX_FRONTEND_INVARIANT[semantic-template-argument]: "
                    "expression=%s has unsupported APValue kind=%d\n",
                    clang_expr->getStmtClassName(),
                    static_cast<int>(evaluation.Val.getKind()));
            ROSE_ABORT();
          }
        }
        semantic_source_owner = sg_expr;
        if (declaration_identity_conversion) {
          for (auto conversion = semantic_conversions.rbegin();
               conversion != semantic_conversions.rend(); ++conversion) {
            const clang::ImplicitCastExpr *implicit = *conversion;
            SgType *conversion_type =
                buildTypeFromQualifiedType(implicit->getType());
            if (SageInterface::containsUnknownType(conversion_type)) {
              fprintf(stderr,
                      "REX_FRONTEND_INVARIANT[semantic-template-conversion]: "
                      "cast kind=%s has no exact result type\n",
                      implicit->getCastKindName());
              ROSE_ABORT();
            }
            sg_expr = SageBuilder::buildCastExp_nfi(
                sg_expr, conversion_type, SgCastExp::e_implicit_cast,
                translateClangCastKind(implicit->getCastKind()),
                translateClangValueCategory(implicit->getValueKind()), {},
                nullptr);
            publishCanonicalSemanticImplicitConversionProvenance(
                isSgCastExp(sg_expr), "semantic-template-implicit-conversion");
          }
        }
        if (templateArgumentNeedsExplicitAddressOf(expression_type, sg_expr)) {
          sg_expr = build_semantic_address_of(sg_expr, expression_type);
        }
        if (sg_expr->get_type() != expression_type &&
            !SageInterface::cxxSourceTypeMatchesSemanticType(
                sg_expr->get_type(), expression_type)) {
          fprintf(stderr,
                  "REX_FRONTEND_INVARIANT[semantic-template-conversion]: "
                  "converted expression type=%p/%s does not match semantic "
                  "argument type=%p/%s\n",
                  static_cast<void *>(sg_expr->get_type()),
                  sg_expr->get_type()->class_name().c_str(),
                  static_cast<void *>(expression_type),
                  expression_type != nullptr
                      ? expression_type->class_name().c_str()
                      : "<null>");
          ROSE_ABORT();
        }
        if (direct_source_reference != nullptr &&
            !applyDirectDeclRefSourceSurface(direct_source_reference,
                                             p_compiler_instance,
                                             semantic_source_owner)) {
          source_expression = isSgExpression(Traverse(clang_expr));
        }
        if (source_expression != nullptr) {
          if (semantic_source_owner == nullptr ||
              source_expression == semantic_source_owner ||
              source_expression->get_parent() != nullptr ||
              source_expression->get_originalExpressionTree() != nullptr ||
              semantic_source_owner->get_originalExpressionTree() != nullptr) {
            fprintf(
                stderr,
                "REX_FRONTEND_INVARIANT[semantic-template-argument-source]: "
                "expression=%s source=%p semantic=%p has contradictory "
                "source ownership\n",
                clang_expr->getStmtClassName(),
                static_cast<void *>(source_expression),
                static_cast<void *>(sg_expr));
            ROSE_ABORT();
          }
          semantic_source_owner->set_originalExpressionTree(source_expression);
          source_expression->set_parent(semantic_source_owner);
        }
      }
      if (sg_expr != nullptr) {
        if (SageInterface::containsUnknownType(expression_type)) {
          fprintf(stderr,
                  "REX_FRONTEND_INVARIANT[template-argument-expression]: "
                  "semantic expression argument has no exact type\n");
          ROSE_ABORT();
        }
        sg_arg = new SgTemplateArgument(
            SgTemplateArgument::nontype_argument,
            /*isArrayBoundUnknownType=*/false, expression_type, sg_expr,
            /*templateDeclaration=*/nullptr, explicitlySpecified);
      }
    }
    break;
  }

  case clang::TemplateArgument::Pack: {
    std::cerr << "REX_FRONTEND_INVARIANT[template-pack-construction]: "
                 "a pack must be flattened by its typed argument-list "
                 "producer"
              << std::endl;
    ROSE_ABORT();
  }

  case clang::TemplateArgument::Null:
    break;

  default:
    break;
  }

  if (sg_arg == nullptr) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[template-argument-kind]: kind=%d did not "
            "produce an exact SgTemplateArgument\n",
            static_cast<int>(arg.getKind()));
    ROSE_ABORT();
  }

  return sg_arg;
}

SgType *ClangToSageTranslator::translateTypeTemplateArgument(
    const clang::TemplateArgumentLoc &arg_loc,
    SgScopeStatement *semantic_owner_scope) {

  if (semantic_owner_scope == nullptr) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[template-argument-type-owner]: "
            "source-spelled type argument has no exact semantic owner\n");
    ROSE_ABORT();
  }
  clang::TemplateArgument arg = arg_loc.getArgument();

  if (arg.getKind() != clang::TemplateArgument::Type) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[template-argument-type-kind]: "
            "type-argument producer received Clang kind=%d\n",
            static_cast<int>(arg.getKind()));
    ROSE_ABORT();
  }

  if (const clang::TypeSourceInfo *type_info = arg_loc.getTypeSourceInfo()) {
    const clang::TypeLoc written_type_loc = type_info->getTypeLoc();
    return buildTypeFromTypeLocWithSemanticOwner(
        written_type_loc, semantic_owner_scope,
        "source-spelled type template argument");
  }

  fprintf(stderr, "REX_FRONTEND_INVARIANT[template-argument-type-source]: "
                  "source-spelled type argument has no exact TypeSourceInfo\n");
  ROSE_ABORT();
}

SgTemplateArgument *ClangToSageTranslator::translateTemplateArgument(
    const clang::TemplateArgumentLoc &arg_loc, bool explicitlySpecified) {

  clang::TemplateArgument arg = arg_loc.getArgument();

  auto exact_source_surface_owner = [](SgExpression *semantic_expression,
                                       const char *producer) -> SgExpression * {
    ASSERT_not_null(semantic_expression);
    ASSERT_not_null(producer);
    std::set<SgExpression *> visited;
    SgExpression *owner = semantic_expression;
    while (SgCastExp *conversion = isSgCastExp(owner)) {
      if (!visited.insert(owner).second) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[template-argument-source-owner]: "
                "producer=%s semantic conversion=%p is cyclic\n",
                producer, static_cast<void *>(conversion));
        ROSE_ABORT();
      }
      conversion->validate_semantic_conversion();
      if (conversion->get_cast_type() != SgCastExp::e_implicit_cast) {
        return owner;
      }
      owner = conversion->get_operand();
    }
    return owner;
  };

  auto attach_source_expression_to_semantic_argument =
      [&](SgExpression *source_expression, const char *producer,
          SgTemplateArgument *semantic_argument) -> SgTemplateArgument * {
    ASSERT_not_null(source_expression);
    ASSERT_not_null(producer);
    if (source_expression->get_parent() != nullptr ||
        source_expression->get_originalExpressionTree() != nullptr) {
      fprintf(
          stderr,
          "REX_FRONTEND_INVARIANT[template-argument-source-expression]: "
          "producer=%s source=%p/%s has parent=%p or nested original "
          "expression=%p before adoption\n",
          producer, static_cast<void *>(source_expression),
          source_expression->class_name().c_str(),
          static_cast<void *>(source_expression->get_parent()),
          static_cast<void *>(source_expression->get_originalExpressionTree()));
      ROSE_ABORT();
    }

    if (semantic_argument == nullptr) {
      SemanticExpressionConstruction semantic_expression(
          p_semantic_template_argument_expression_depth, producer);
      semantic_argument = translateTemplateArgument(
          static_cast<const clang::TemplateArgument &>(arg),
          explicitlySpecified);
    }
    SgExpression *semantic_expression =
        semantic_argument != nullptr ? semantic_argument->get_expression()
                                     : nullptr;
    SgExpression *source_surface_owner =
        semantic_expression != nullptr
            ? exact_source_surface_owner(semantic_expression, producer)
            : nullptr;
    const bool exact_argument_kind = semantic_argument != nullptr &&
                                     semantic_argument->get_argumentType() ==
                                         SgTemplateArgument::nontype_argument;
    const bool exact_expression_owner =
        semantic_expression != nullptr &&
        semantic_expression->get_parent() == semantic_argument;
    const bool distinct_source_owner =
        source_surface_owner != nullptr &&
        source_surface_owner != source_expression;
    const bool source_owner_available =
        source_surface_owner != nullptr &&
        source_surface_owner->get_originalExpressionTree() == nullptr;
    const bool source_types_available =
        source_expression->get_type() != nullptr &&
        source_surface_owner != nullptr &&
        source_surface_owner->get_type() != nullptr;
    const bool exact_source_conversion =
        source_types_available &&
        SageInterface::cxxNonTypeTemplateArgumentTypeConversionIsExact(
            source_expression->get_type(), source_surface_owner->get_type());
    if (!exact_argument_kind || !exact_expression_owner ||
        !distinct_source_owner || !source_owner_available ||
        !source_types_available || !exact_source_conversion) {
      SgType *source_stripped =
          source_expression->get_type() != nullptr
              ? source_expression->get_type()->stripTypedefsAndModifiers()
              : nullptr;
      SgType *owner_stripped =
          source_surface_owner != nullptr &&
                  source_surface_owner->get_type() != nullptr
              ? source_surface_owner->get_type()->stripTypedefsAndModifiers()
              : nullptr;
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[template-argument-source-expression]: "
              "producer=%s source=%p/%s source-type=%p/%s "
              "semantic-argument=%p kind=%d semantic-expression=%p/%s "
              "semantic-expression-type=%p/%s source-owner=%p/%s "
              "source-owner-type=%p/%s semantic-type=%p/%s has no exact "
              "canonical non-type argument ownership\n",
              producer, static_cast<void *>(source_expression),
              source_expression->class_name().c_str(),
              static_cast<void *>(source_expression->get_type()),
              source_expression->get_type() != nullptr
                  ? source_expression->get_type()->class_name().c_str()
                  : "<null>",
              static_cast<void *>(semantic_argument),
              semantic_argument != nullptr
                  ? static_cast<int>(semantic_argument->get_argumentType())
                  : -1,
              static_cast<void *>(semantic_expression),
              semantic_expression != nullptr
                  ? semantic_expression->class_name().c_str()
                  : "<null>",
              static_cast<void *>(semantic_expression != nullptr
                                      ? semantic_expression->get_type()
                                      : nullptr),
              semantic_expression != nullptr &&
                      semantic_expression->get_type() != nullptr
                  ? semantic_expression->get_type()->class_name().c_str()
                  : "<null>",
              static_cast<void *>(source_surface_owner),
              source_surface_owner != nullptr
                  ? source_surface_owner->class_name().c_str()
                  : "<null>",
              static_cast<void *>(source_surface_owner != nullptr
                                      ? source_surface_owner->get_type()
                                      : nullptr),
              source_surface_owner != nullptr &&
                      source_surface_owner->get_type() != nullptr
                  ? source_surface_owner->get_type()->class_name().c_str()
                  : "<null>",
              static_cast<void *>(semantic_argument != nullptr
                                      ? semantic_argument->get_type()
                                      : nullptr),
              semantic_argument != nullptr &&
                      semantic_argument->get_type() != nullptr
                  ? semantic_argument->get_type()->class_name().c_str()
                  : "<null>");
      fprintf(stderr,
              "REX_FRONTEND_DETAIL[template-argument-source-expression]: "
              "argument-kind=%d expression-owner=%d distinct-source-owner=%d "
              "source-owner-available=%d source-types-available=%d "
              "source-conversion=%d source-stripped=%p/%s "
              "source-integer=%d owner-stripped=%p/%s owner-integer=%d\n",
              exact_argument_kind ? 1 : 0, exact_expression_owner ? 1 : 0,
              distinct_source_owner ? 1 : 0, source_owner_available ? 1 : 0,
              source_types_available ? 1 : 0, exact_source_conversion ? 1 : 0,
              static_cast<void *>(source_stripped),
              source_stripped != nullptr ? source_stripped->class_name().c_str()
                                         : "<null>",
              source_expression->get_type() != nullptr &&
                      source_expression->get_type()->isIntegerType()
                  ? 1
                  : 0,
              static_cast<void *>(owner_stripped),
              owner_stripped != nullptr ? owner_stripped->class_name().c_str()
                                        : "<null>",
              source_surface_owner != nullptr &&
                      source_surface_owner->get_type() != nullptr &&
                      source_surface_owner->get_type()->isIntegerType()
                  ? 1
                  : 0);
      ROSE_ABORT();
    }

    source_surface_owner->set_originalExpressionTree(source_expression);
    source_expression->set_parent(source_surface_owner);
    return semantic_argument;
  };

  auto translate_source_expression = [&](const clang::Expr *expression,
                                         const char *producer) {
    SgTemplateArgument *semantic_argument = nullptr;
    {
      SemanticExpressionConstruction semantic_expression(
          p_semantic_template_argument_expression_depth, producer);
      semantic_argument = translateTemplateArgument(arg, explicitlySpecified);
    }
    ASSERT_not_null(semantic_argument);
    SgExpression *semantic_expression = semantic_argument->get_expression();
    ASSERT_not_null(semantic_expression);
    auto *surface = exact_source_surface_owner(semantic_expression, producer);
    if (auto *reference = llvm::dyn_cast<clang::DeclRefExpr>(expression)) {
      if (applyDirectDeclRefSourceSurface(reference, p_compiler_instance,
                                          surface))
        return semantic_argument;
    }
    auto *source =
        isSgExpression(Traverse(const_cast<clang::Expr *>(expression)));
    return attach_source_expression_to_semantic_argument(source, producer,
                                                         semantic_argument);
  };

  switch (arg.getKind()) {
  case clang::TemplateArgument::Type: {
    const clang::TypeSourceInfo *type_info = arg_loc.getTypeSourceInfo();
    if (type_info == nullptr) {
      fprintf(stderr, "REX_FRONTEND_INVARIANT[template-argument-type-surface]: "
                      "type argument has no exact TypeSourceInfo\n");
      ROSE_ABORT();
    }
    SgScopeStatement *source_type_owner =
        !p_type_loc_semantic_owner_scope_stack.empty()
            ? p_type_loc_semantic_owner_scope_stack.back()
            : SageBuilder::topScopeStack();
    if (source_type_owner == nullptr) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[template-argument-type-owner]: "
              "source-spelled type argument has no exact semantic owner\n");
      ROSE_ABORT();
    }
    SgType *source_spelled_type =
        translateTypeTemplateArgument(arg_loc, source_type_owner);
    clang::QualType semantic_qual_type = arg.getAsType();
    const bool requires_source_owned_dependent_identity =
        requiresSourceOwnedDependentTypeIdentity(semantic_qual_type);
    if (!requires_source_owned_dependent_identity) {
      semantic_qual_type = semantic_qual_type.getCanonicalType();
    }
    const clang::TemplateTypeParmType *canonical_parameter_type =
        llvm::dyn_cast_or_null<clang::TemplateTypeParmType>(
            semantic_qual_type.getTypePtrOrNull());
    SgTemplateType *exact_source_parameter =
        isSgTemplateType(source_spelled_type);
    SgType *semantic_type = nullptr;
    if (canonical_parameter_type != nullptr &&
        canonical_parameter_type->getDecl() == nullptr &&
        exact_source_parameter != nullptr) {
      const auto &source_identity =
          exact_source_parameter->get_canonical_source_identity();
      if (!source_identity.has_value() ||
          exact_source_parameter->get_template_parameter_depth() < 0 ||
          exact_source_parameter->get_template_parameter_position() < 0) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[template-argument-parameter-"
                "identity]: declaration-less canonical parameter depth=%u "
                "index=%u has no exact declaration-token TypeLoc identity\n",
                canonical_parameter_type->getDepth(),
                canonical_parameter_type->getIndex());
        ROSE_ABORT();
      }
      const auto *written_parameter =
          type_info->getType()->getAs<clang::TemplateTypeParmType>();
      ASSERT_not_null(written_parameter);
      auto *parameter = const_cast<clang::TemplateTypeParmDecl *>(
          written_parameter->getDecl() != nullptr
              ? written_parameter->getDecl()
              : resolveExactTemplateTypeParameterDeclaration(
                    exact_source_parameter->get_template_parameter_depth(),
                    exact_source_parameter->get_template_parameter_position()));
      ASSERT_not_null(parameter);
      ROSE_ASSERT(requireCanonicalTemplateParameterSourceIdentity(
                      parameter, "template-argument-semantic-parameter") ==
                  *source_identity);
      semantic_type = buildSemanticTypeFromQualifiedType(
          clang::QualType(parameter->getTypeForDecl(), 0),
          "template-argument-semantic-parameter");
    } else {
      semantic_type = buildSemanticTypeFromQualifiedType(
          semantic_qual_type, "template-argument-semantic-type");
    }
    if (source_spelled_type == nullptr || semantic_type == nullptr) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[template-argument-type-surface]: "
              "TemplateArgumentLoc type has source type=%p semantic type=%p\n",
              static_cast<void *>(source_spelled_type),
              static_cast<void *>(semantic_type));
      ROSE_ABORT();
    }

    clang::TypeLoc type_loc = type_info->getTypeLoc();

    // A template parameter declaration and a use-site pack expansion have
    // distinct ellipsis owners.  SgTemplateType::packed describes the former;
    // SgTemplateArgument::is_pack_element describes the latter.  Reusing the
    // declaration type as source spelling for `T...` makes both layers emit
    // the same token.  Keep semantic identity on semantic_type and publish a
    // spelling-only pattern type with the declaration ellipsis removed.
    if (type_loc.getAs<clang::PackExpansionTypeLoc>()) {
      if (SgTemplateType *packed_pattern =
              isSgTemplateType(source_spelled_type)) {
        if (packed_pattern->get_packed()) {
          source_spelled_type =
              buildUnexpandedTemplateParameterUse(packed_pattern);
        }
      }
    }

    publishSourceElaborationKind(source_spelled_type, type_loc,
                                 p_compiler_instance);
    clang::NestedNameSpecifierLoc qualifier_loc = typeLocQualifierLoc(type_loc);
    clang::NestedNameSpecifier qualifier =
        qualifier_loc.getNestedNameSpecifier();
    SourceQualification source_qualification;
    if (qualifier_loc) {
      source_qualification = sourceQualificationFromNestedNameSpecifierLoc(
          qualifier_loc, p_compiler_instance, "template-type-argument");
      if (!sourceQualificationMatchesSemanticIdentity(
              source_qualification, qualifier,
              nestedNameSpecifierLocHasExplicitGlobal(qualifier_loc),
              static_cast<std::size_t>(
                  nestedNameSpecifierComponentCount(qualifier)))) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[template-argument-source-"
                "qualification]: structural qualifier disagrees with Clang "
                "identity\n");
        ROSE_ABORT();
      }
    }
    // The template argument is the sole grammar owner of this TypeLoc's
    // qualifier.  A nonreal declaration is shared semantic type identity and
    // must never retain occurrence-specific spelling.
    if (SageInterface::typeCarriesWrittenNonrealQualification(
            source_spelled_type)) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[template-argument-source-qualification]: "
              "source type=%p/%s leaks use-site qualifier state "
              "through shared semantic identity\n",
              static_cast<void *>(source_spelled_type),
              source_spelled_type->class_name().c_str());
      ROSE_ABORT();
    }
    const bool type_elaboration_required = typeLocSpellsElaboratedKeyword(
        type_loc, arg_loc.getSourceRange(), p_compiler_instance);
    SgTemplateArgument *template_argument = new SgTemplateArgument(
        semantic_type, explicitlySpecified, source_spelled_type);
    if (sourceQualificationIsSemanticMacroFragment(source_qualification)) {
      template_argument->set_source_type_qualification_present(false);
      template_argument->set_source_type_global_qualification(false);
      template_argument->get_source_type_qualification_tokens().clear();
      template_argument->set_name_qualification_length(0);
      template_argument->set_global_qualification_required(false);
    } else {
      template_argument->set_source_type_qualification_present(true);
      template_argument->set_source_type_global_qualification(
          source_qualification.global);
      template_argument->get_source_type_qualification_tokens() =
          source_qualification.tokens;
      template_argument->set_name_qualification_length(
          nestedNameSpecifierComponentCount(qualifier));
      template_argument->set_global_qualification_required(
          nestedNameSpecifierLocHasExplicitGlobal(qualifier_loc));
    }
    template_argument->set_type_elaboration_required(type_elaboration_required);

    return template_argument;
  }

  case clang::TemplateArgument::Expression: {
    const clang::Expr *expr = arg_loc.getSourceExpression();
    if (expr != nullptr) {
      // TemplateArgumentLoc can expose semantic-only ImplicitCastExpr and
      // ConstantExpr envelopes around the written argument.  Those nodes own
      // no source tokens and therefore cannot be adopted as the spelling of a
      // semantic Sage expression.  Select the exact written operand before
      // deciding whether the source surface is a direct declaration reference
      // or a detached expression tree.
      while (true) {
        const clang::Expr *wrapped = nullptr;
        if (const clang::ImplicitCastExpr *implicit =
                llvm::dyn_cast<clang::ImplicitCastExpr>(expr)) {
          wrapped = implicit->getSubExpr();
        } else if (const clang::ConstantExpr *constant =
                       llvm::dyn_cast<clang::ConstantExpr>(expr)) {
          wrapped = constant->getSubExpr();
        } else {
          break;
        }
        expr = wrapped;
        if (expr == nullptr) {
          fprintf(stderr,
                  "REX_FRONTEND_INVARIANT[template-argument-expression-"
                  "source]: implicit semantic wrapper has no exact source "
                  "operand\n");
          ROSE_ABORT();
        }
      }
      const bool dependent_expression = expr->isTypeDependent() ||
                                        expr->isValueDependent() ||
                                        expr->isInstantiationDependent() ||
                                        expr->containsUnexpandedParameterPack();
      if (dependent_expression) {
        // A dependent non-type argument has no independently evaluated semantic
        // value.  Its written expression is therefore the argument's one typed
        // expression child, not an original-expression annotation on a
        // duplicate semantic tree.
        SgTemplateArgument *source_argument = translateTemplateArgument(
            static_cast<const clang::TemplateArgument &>(arg),
            explicitlySpecified);
        SgExpression *source_expression =
            source_argument != nullptr ? source_argument->get_expression()
                                       : nullptr;
        if (source_argument == nullptr ||
            source_argument->get_argumentType() !=
                SgTemplateArgument::nontype_argument ||
            source_expression == nullptr ||
            source_expression->get_parent() != source_argument ||
            source_expression->get_originalExpressionTree() != nullptr) {
          fprintf(
              stderr,
              "REX_FRONTEND_INVARIANT[dependent-template-argument-source-owner]"
              ": "
              "expression=%s argument=%p kind=%d source=%p/%s parent=%p "
              "original=%p does not form one exact source-expression edge\n",
              expr->getStmtClassName(), static_cast<void *>(source_argument),
              source_argument != nullptr
                  ? static_cast<int>(source_argument->get_argumentType())
                  : -1,
              static_cast<void *>(source_expression),
              source_expression != nullptr
                  ? source_expression->class_name().c_str()
                  : "<null>",
              static_cast<void *>(source_expression != nullptr
                                      ? source_expression->get_parent()
                                      : nullptr),
              static_cast<void *>(
                  source_expression != nullptr
                      ? source_expression->get_originalExpressionTree()
                      : nullptr));
          ROSE_ABORT();
        }
        return source_argument;
      }
      return translate_source_expression(expr,
                                         "template-argument-expression-source");
    }
    if (explicitlySpecified) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[template-argument-expression-source]: "
              "written expression has no exact typed source operand\n");
      ROSE_ABORT();
    }
    break;
  }

  case clang::TemplateArgument::Integral: {
    if (const clang::Expr *expr = arg_loc.getSourceIntegralExpression()) {
      SgNode *node = Traverse(const_cast<clang::Expr *>(expr));
      if (SgExpression *sg_expr = isSgExpression(node)) {
        return attach_source_expression_to_semantic_argument(
            sg_expr, "template-argument-integral-source", nullptr);
      }
    }
    if (explicitlySpecified) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[template-argument-integral-source]: "
              "written integral argument has no exact typed source operand\n");
      ROSE_ABORT();
    }
    break;
  }

  case clang::TemplateArgument::NullPtr: {
    const clang::Expr *expr = arg_loc.getSourceNullPtrExpression();
    if (expr == nullptr) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[template-argument-nullptr-surface]: "
              "written null pointer argument has no exact source "
              "expression\n");
      ROSE_ABORT();
    }

    SgExpression *sg_expr =
        isSgExpression(Traverse(const_cast<clang::Expr *>(expr)));
    SgType *null_type = buildTypeFromQualifiedType(arg.getNullPtrType());
    if (sg_expr == nullptr || isSgNullptrValExp(sg_expr) == nullptr ||
        sg_expr->get_type() == nullptr || null_type == nullptr) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[template-argument-nullptr-surface]: "
              "written null pointer argument produced expression=%p/%s "
              "expression-type=%p parameter-type=%p\n",
              static_cast<void *>(sg_expr),
              sg_expr != nullptr ? sg_expr->class_name().c_str() : "null",
              static_cast<void *>(sg_expr != nullptr ? sg_expr->get_type()
                                                     : nullptr),
              static_cast<void *>(null_type));
      ROSE_ABORT();
    }

    SgTemplateArgument *sg_arg = new SgTemplateArgument(
        SgTemplateArgument::nontype_argument,
        /*isArrayBoundUnknownType=*/false, null_type, sg_expr,
        /*templateDeclaration=*/nullptr, explicitlySpecified);
    return sg_arg;
  }

  case clang::TemplateArgument::Declaration: {
    if (const clang::Expr *expr = arg_loc.getSourceDeclExpression()) {
      const clang::Expr *source_expression = expr;
      while (true) {
        const clang::Expr *wrapped = nullptr;
        if (const clang::ImplicitCastExpr *implicit =
                llvm::dyn_cast<clang::ImplicitCastExpr>(source_expression)) {
          wrapped = implicit->getSubExpr();
        } else if (const clang::ConstantExpr *constant =
                       llvm::dyn_cast<clang::ConstantExpr>(source_expression)) {
          wrapped = constant->getSubExpr();
        } else {
          break;
        }
        source_expression = wrapped;
        if (source_expression == nullptr) {
          fprintf(stderr,
                  "REX_FRONTEND_INVARIANT[template-argument-declaration]: "
                  "implicit semantic wrapper has no exact source operand\n");
          ROSE_ABORT();
        }
      }
      return translate_source_expression(
          source_expression, "template-argument-declaration-source");
    }
    if (explicitlySpecified) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[template-argument-declaration-source]: "
              "written declaration argument has no exact typed source "
              "operand\n");
      ROSE_ABORT();
    }
    break;
  }

  case clang::TemplateArgument::StructuralValue: {
    if (const clang::Expr *expr =
            arg_loc.getSourceStructuralValueExpression()) {
      SgNode *node = Traverse(const_cast<clang::Expr *>(expr));
      if (SgExpression *source = isSgExpression(node)) {
        return attach_source_expression_to_semantic_argument(
            source, "template-argument-structural-source", nullptr);
      }
    }
    if (explicitlySpecified) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[template-argument-structural-source]: "
              "written structural value has no exact typed source operand\n");
      ROSE_ABORT();
    }
    break;
  }

  case clang::TemplateArgument::Template:
  case clang::TemplateArgument::TemplateExpansion: {
    clang::NestedNameSpecifier qualifier =
        arg_loc.getTemplateQualifierLoc().getNestedNameSpecifier();
    return translateTemplateArgument(arg, explicitlySpecified, qualifier);
  }

  default:
    break;
  }

  if (explicitlySpecified) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[template-argument-source]: kind=%d "
            "has no exact typed written surface\n",
            static_cast<int>(arg.getKind()));
    ROSE_ABORT();
  }
  return translateTemplateArgument(arg, false);
}

void ClangToSageTranslator::appendTemplateArguments(
    SgTemplateArgumentPtrList &arg_list, const clang::TemplateArgument &arg,
    bool explicitlySpecified) {

  if (!arg.isPackExpansion() &&
      arg.getKind() == clang::TemplateArgument::Pack) {
    for (const clang::TemplateArgument &pack_arg : arg.pack_elements()) {
      appendTemplateArguments(arg_list, pack_arg, explicitlySpecified);
    }
    return;
  }

  SgTemplateArgument *sg_arg =
      translateTemplateArgument(arg, explicitlySpecified);
  ASSERT_not_null(sg_arg);
  arg_list.push_back(sg_arg);
}

void ClangToSageTranslator::appendTemplateArguments(
    SgTemplateArgumentPtrList &arg_list,
    const clang::TemplateArgumentLoc &arg_loc, bool explicitlySpecified) {
  clang::TemplateArgument arg = arg_loc.getArgument();

  if (arg.isPackExpansion() && arg.getKind() == clang::TemplateArgument::Type) {
    SgTemplateArgument *sg_arg =
        translateTemplateArgument(arg_loc, explicitlySpecified);
    if (sg_arg == nullptr ||
        sg_arg->get_argumentType() != SgTemplateArgument::type_argument) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[template-pack-source-surface]: "
              "written type pack expansion did not produce one exact typed "
              "template argument\n");
      ROSE_ABORT();
    }
    sg_arg->set_is_pack_element(true);
    arg_list.push_back(sg_arg);
    return;
  }

  if (arg.isPackExpansion() || arg.getKind() == clang::TemplateArgument::Pack) {
    appendTemplateArguments(arg_list, arg, explicitlySpecified);
    return;
  }

  SgTemplateArgument *sg_arg =
      translateTemplateArgument(arg_loc, explicitlySpecified);
  ASSERT_not_null(sg_arg);
  arg_list.push_back(sg_arg);
}

SgTemplateArgumentPtrList ClangToSageTranslator::buildTemplateArguments(
    const clang::TemplateSpecializationType *clang_type,
    bool reconstruct_explicit_source_surface) {
  ASSERT_not_null(clang_type);
  SgTemplateArgumentPtrList arg_list;

  auto args_as_written = clang_type->template_arguments();
  const clang::TemplateArgumentList *full_args = nullptr;

  const clang::QualType canonical_qt =
      clang::QualType(clang_type, 0).getCanonicalType();
  if (const auto *record_type =
          llvm::dyn_cast<clang::RecordType>(canonical_qt.getTypePtr())) {
    if (const auto *spec_decl =
            llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(
                record_type->getDecl()))
      full_args = &spec_decl->getTemplateArgs();
  }

  const bool use_full_args =
      !clang_type->isTypeAlias() && !clang_type->isCurrentInstantiation() &&
      full_args != nullptr && full_args->size() > args_as_written.size();

  struct ReconstructedWrittenArgumentGuard {
    unsigned &depth;
    const bool active;

    ReconstructedWrittenArgumentGuard(unsigned &depth, bool active)
        : depth(depth), active(active) {
      if (active) {
        ++depth;
      }
    }

    ~ReconstructedWrittenArgumentGuard() {
      if (!active) {
        return;
      }
      if (depth == 0) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[template-argument-surface-state]: "
                "written template argument lost its reconstruction "
                "transaction\n");
        ROSE_ABORT();
      }
      --depth;
    }
  };
  auto append_written_argument = [&](const clang::TemplateArgument &arg) {
    ReconstructedWrittenArgumentGuard reconstructed(
        p_reconstructed_template_argument_surface_depth,
        reconstruct_explicit_source_surface);
    appendTemplateArguments(arg_list, arg, true);
  };

  if (use_full_args) {
    unsigned argument_index = 0;
    for (const clang::TemplateArgument &arg : full_args->asArray()) {
      if (argument_index < args_as_written.size()) {
        // The specialization type's written array is the only exact spelling
        // evidence for the explicit prefix.  The completed specialization
        // list is canonical semantic state and can erase typedef/alias sugar
        // (for example, `mbstate_t` becomes its anonymous struct).  Preserve
        // the written argument here and append only the defaulted suffix from
        // the full list.
        append_written_argument(args_as_written[argument_index]);
      } else {
        appendTemplateArguments(arg_list, arg, false);
      }
      ++argument_index;
    }
  } else {
    for (const clang::TemplateArgument &arg : args_as_written) {
      append_written_argument(arg);
    }
  }

  return arg_list;
}

SgTemplateArgumentPtrList ClangToSageTranslator::buildTemplateArguments(
    const clang::TemplateArgumentListInfo &arg_info, bool explicitlySpecified) {
  SgTemplateArgumentPtrList arg_list;

  for (const clang::TemplateArgumentLoc &arg_loc : arg_info.arguments()) {
    const bool resolved_reference_surface =
        p_resolved_reference_written_argument_owner == &arg_info;
    const size_t expanded_count =
        countExpandedTemplateArgument(arg_loc.getArgument());
    if (resolved_reference_surface) {
      const size_t cursor = p_resolved_reference_semantic_argument_cursor;
      if (cursor + expanded_count >
          p_resolved_reference_semantic_argument_kinds.size()) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[explicit-template-reference-"
                "arguments]: written argument range=[%zu,%zu) exceeds "
                "resolved semantic argument count=%zu\n",
                cursor, cursor + expanded_count,
                p_resolved_reference_semantic_argument_kinds.size());
        ROSE_ABORT();
      }
      std::optional<clang::TemplateArgument::ArgKind> expected_kind;
      for (size_t index = cursor; index < cursor + expanded_count; ++index) {
        const clang::TemplateArgument::ArgKind kind =
            p_resolved_reference_semantic_argument_kinds[index];
        if (expected_kind.has_value() && *expected_kind != kind) {
          fprintf(stderr,
                  "REX_FRONTEND_INVARIANT[explicit-template-reference-"
                  "arguments]: one written argument expands to heterogeneous "
                  "semantic kinds=%d/%d at index=%zu\n",
                  static_cast<int>(*expected_kind), static_cast<int>(kind),
                  index);
          ROSE_ABORT();
        }
        expected_kind = kind;
      }
      p_resolved_reference_current_semantic_argument_kind = expected_kind;
    }
    appendTemplateArguments(arg_list, arg_loc, explicitlySpecified);
    if (resolved_reference_surface) {
      p_resolved_reference_semantic_argument_cursor += expanded_count;
      p_resolved_reference_current_semantic_argument_kind.reset();
    }
  }

  return arg_list;
}

SgTemplateArgumentPtrList ClangToSageTranslator::buildSemanticTemplateArguments(
    const clang::TemplateArgumentListInfo &arg_info, bool explicitlySpecified,
    const char *context) {
  SemanticExpressionConstruction semantic_arguments(
      p_semantic_template_argument_expression_depth, context);
  return buildTemplateArguments(arg_info, explicitlySpecified);
}

SgTemplateArgumentPtrList ClangToSageTranslator::buildTemplateArguments(
    const clang::TemplateArgumentList &args, size_t explicit_count,
    bool reconstruct_explicit_source_surface,
    const clang::ASTTemplateArgumentListInfo *written_args) {
  struct ReconstructedArgumentSurfaceGuard {
    unsigned &depth;
    const bool active;

    ReconstructedArgumentSurfaceGuard(unsigned &depth, bool active)
        : depth(depth), active(active) {
      if (active) {
        ++depth;
      }
    }

    ~ReconstructedArgumentSurfaceGuard() {
      if (!active) {
        return;
      }
      if (depth == 0) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[template-argument-surface-state]: "
                "reconstructed source transaction lost its exact depth\n");
        ROSE_ABORT();
      }
      --depth;
    }
  } reconstructed_surface(p_reconstructed_template_argument_surface_depth,
                          reconstruct_explicit_source_surface);

  SgTemplateArgumentPtrList arg_list;
  const size_t semantic_expanded_count = countExpandedTemplateArguments(args);
  if (explicit_count > semantic_expanded_count) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[template-argument-prefix]: requested "
            "%zu explicit arguments from a semantic list containing only "
            "%zu expanded arguments in %u top-level arguments\n",
            explicit_count, semantic_expanded_count, args.size());
    ROSE_ABORT();
  }
  for (unsigned i = 0; i < args.size(); ++i) {
    const size_t prior_argument_count = arg_list.size();
    const clang::TemplateArgument &argument = args.get(i);
    const size_t top_level_expanded_count =
        countExpandedTemplateArgument(argument);
    const bool overlaps_explicit_prefix = prior_argument_count < explicit_count;
    if (reconstruct_explicit_source_surface && overlaps_explicit_prefix &&
        prior_argument_count + top_level_expanded_count > explicit_count) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[template-argument-prefix]: source "
              "reconstruction cannot split semantic top-level argument=%u "
              "with expanded range=[%zu,%zu) at explicit boundary=%zu\n",
              i, prior_argument_count,
              prior_argument_count + top_level_expanded_count, explicit_count);
      ROSE_ABORT();
    }
    const clang::RecordType *record_type =
        argument.getKind() == clang::TemplateArgument::Type
            ? argument.getAsType()
                  .getCanonicalType()
                  ->getAs<clang::RecordType>()
            : nullptr;
    const bool needs_named_source_type =
        reconstruct_explicit_source_surface && overlaps_explicit_prefix &&
        written_args != nullptr && record_type != nullptr &&
        record_type->getDecl()->getName().empty();
    if (needs_named_source_type) {
      if (prior_argument_count >= written_args->getNumTemplateArgs()) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[explicit-instantiation-type-"
                "surface]: anonymous semantic argument=%zu has no exact "
                "declaration argument TypeLoc\n",
                prior_argument_count);
        ROSE_ABORT();
      }
      const clang::TemplateArgumentLoc &written_arg =
          written_args->arguments()[prior_argument_count];
      const clang::TemplateArgument &written_value = written_arg.getArgument();
      if (written_value.getKind() != clang::TemplateArgument::Type ||
          !p_compiler_instance->getASTContext().hasSameType(
              written_value.getAsType(), argument.getAsType())) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[explicit-instantiation-type-"
                "surface]: declaration and semantic arguments disagree at "
                "index=%zu\n",
                prior_argument_count);
        ROSE_ABORT();
      }
      appendTemplateArguments(arg_list, written_arg, true);
    } else {
      appendTemplateArguments(arg_list, argument,
                              reconstruct_explicit_source_surface &&
                                  overlaps_explicit_prefix);
    }
    if (arg_list.size() != prior_argument_count + top_level_expanded_count) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[template-argument-prefix]: semantic "
              "top-level argument=%u reported %zu expanded arguments but "
              "translation produced %zu\n",
              i, top_level_expanded_count,
              arg_list.size() - prior_argument_count);
      ROSE_ABORT();
    }
    for (size_t index = prior_argument_count; index < arg_list.size();
         ++index) {
      SgTemplateArgument *translated = arg_list[index];
      if (translated == nullptr) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[template-argument-prefix]: semantic "
                "argument=%zu is null after pack expansion\n",
                index);
        ROSE_ABORT();
      }
      translated->set_explicitlySpecified(index < explicit_count);
    }
  }
  return arg_list;
}

size_t ClangToSageTranslator::countExpandedTemplateArguments(
    const clang::TemplateArgumentListInfo &arg_info) {
  size_t count = 0;
  for (const clang::TemplateArgumentLoc &loc : arg_info.arguments()) {
    count += countExpandedTemplateArgument(loc.getArgument());
  }
  return count;
}

size_t ClangToSageTranslator::countExpandedTemplateArguments(
    const clang::TemplateArgumentList &args) {
  size_t count = 0;
  for (const clang::TemplateArgument &argument : args.asArray()) {
    count += countExpandedTemplateArgument(argument);
  }
  return count;
}

SgNonrealType *
ClangToSageTranslator::buildNonrealTypeForNestedNameSpecifierType(
    const clang::Type *clang_type, SgScopeStatement *scope,
    bool prefer_current_scope, SgScopeStatement *template_use_scope) {

  if (clang_type == nullptr) {
    return nullptr;
  }

  if (const clang::DecltypeType *decltype_type =
          llvm::dyn_cast<clang::DecltypeType>(clang_type)) {
    clang::QualType underlying_type = decltype_type->getUnderlyingType();
    const clang::Type *underlying = underlying_type.getTypePtrOrNull();
    if (underlying == nullptr || underlying == clang_type) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[nested-decltype-owner]: "
              "decltype qualifier=%p has no distinct exact semantic type\n",
              static_cast<const void *>(decltype_type));
      ROSE_ABORT();
    }
    // A non-dependent decltype nested-name-specifier is lowered to its exact
    // semantic owner type, matching VisitDecltypeType.  Preserve every class
    // specialization argument while doing so; reducing the owner to its bare
    // template name produces invalid spellings such as map::mapped_type.
    return buildNonrealTypeForNestedNameSpecifierType(
        underlying, scope, prefer_current_scope, template_use_scope);
  }

  if (const clang::SubstTemplateTypeParmType *subst =
          llvm::dyn_cast<clang::SubstTemplateTypeParmType>(clang_type)) {
    clang::QualType replacement = subst->getReplacementType();
    return buildNonrealTypeForNestedNameSpecifierType(
        replacement.getTypePtrOrNull(), scope, prefer_current_scope,
        template_use_scope);
  }

  if (const auto *pack =
          llvm::dyn_cast<clang::SubstTemplateTypeParmPackType>(clang_type)) {
    const clang::TemplateTypeParmDecl *parameter = pack->getReplacedParameter();
    ASSERT_not_null(parameter);
    const clang::Type *parameter_type = parameter->getTypeForDecl();
    ASSERT_not_null(parameter_type);
    return buildNonrealTypeForNestedNameSpecifierType(
        parameter_type, scope, prefer_current_scope, template_use_scope);
  }

  auto build_with_qualifier =
      [&](clang::NestedNameSpecifier qualifier, const std::string &base_name,
          const SgTemplateArgumentPtrList *tpl_args,
          SgDeclarationStatement *terminal_declaration = nullptr,
          SgType *aliased_type = nullptr) -> SgNonrealType * {
    ROSE_ASSERT(!base_name.empty());
    if (prefer_current_scope) {
      if (tpl_args != nullptr && terminal_declaration == nullptr) {
        if (!nestedNameSpecifierHasDependentTypeQualifier(qualifier)) {
          fprintf(stderr,
                  "REX_FRONTEND_INVARIANT[nested-template-id-target]: "
                  "template=%s has no exact dependent qualifier\n",
                  base_name.c_str());
          ROSE_ABORT();
        }
        terminal_declaration = SageBuilder::buildSemanticNonrealType(
                                   SgName(base_name), scope, nullptr)
                                   ->get_declaration();
      }
      qualifier = std::nullopt;
    }
    if (qualifier) {
      return buildSemanticNonrealTypeFromNestedNameSpecifier(
          qualifier, scope, SgName(base_name), tpl_args, terminal_declaration,
          aliased_type);
    }
    SgScopeStatement *use_scope =
        tpl_args != nullptr && template_use_scope != nullptr
            ? template_use_scope
            : scope;
    return SageBuilder::buildSemanticNonrealType(SgName(base_name), use_scope,
                                                 tpl_args, terminal_declaration,
                                                 aliased_type);
  };
  if (const clang::UnresolvedUsingType *uut =
          llvm::dyn_cast<clang::UnresolvedUsingType>(clang_type)) {
    const clang::UnresolvedUsingTypenameDecl *decl = uut->getDecl();
    ROSE_ASSERT(decl != nullptr);
    std::string name_str = decl->getNameAsString();
    ROSE_ASSERT(!name_str.empty());
    return build_with_qualifier(decl->getQualifier(), name_str, nullptr);
  }

  auto build_alias_type = [&](clang::NamedDecl *decl,
                              clang::NestedNameSpecifier qualifier,
                              bool using_alias) -> SgNonrealType * {
    if (decl == nullptr || decl->getNameAsString().empty()) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[nonreal-alias]: exact Clang alias "
              "declaration and name are required\n");
      ROSE_ABORT();
    }
    if (!qualifier && !using_alias && !prefer_current_scope &&
        canSynthesizeNamespaceQualifierFromDeclContext(
            decl->getDeclContext()) &&
        !scopeIsWithinNamespaceChain(scope, decl->getDeclContext())) {
      qualifier = buildNamespaceQualifierForDeclContext(
          decl->getDeclContext(), p_compiler_instance->getASTContext());
    }
    SgDeclarationStatement *exact_decl =
        lookupSgDeclarationForClangDecl(decl, /*allow_on_demand=*/true);
    if ((using_alias && isSgUsingDeclarationStatement(exact_decl) == nullptr) ||
        (!using_alias && isSgTypedefDeclaration(exact_decl) == nullptr)) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[nonreal-alias]: alias=%s has no "
              "exact translated declaration\n",
              decl->getQualifiedNameAsString().c_str());
      ROSE_ABORT();
    }
    SgNonrealType *alias = build_with_qualifier(
        qualifier, decl->getNameAsString(), nullptr, exact_decl);
    ROSE_ASSERT(alias != nullptr);
    return alias;
  };
  if (const auto *typedef_type =
          llvm::dyn_cast<clang::TypedefType>(clang_type)) {
    return build_alias_type(typedef_type->getDecl(),
                            typedef_type->getQualifier(), false);
  }
  if (const auto *using_type = llvm::dyn_cast<clang::UsingType>(clang_type)) {
    return build_alias_type(using_type->getDecl(), using_type->getQualifier(),
                            true);
  }

  if (const clang::DependentNameType *dnt =
          llvm::dyn_cast<clang::DependentNameType>(clang_type)) {
    const clang::IdentifierInfo *id = dnt->getIdentifier();
    ROSE_ASSERT(id != nullptr);
    ROSE_ASSERT(!id->getName().empty());
    return build_with_qualifier(dnt->getQualifier(), id->getName().str(),
                                nullptr);
  }

  if (const clang::TemplateSpecializationType *tst =
          llvm::dyn_cast<clang::TemplateSpecializationType>(clang_type)) {
    clang::TemplateName tname = tst->getTemplateName();
    clang::TemplateDecl *template_decl = resolveTemplateNameDeclaration(tname);
    SgTemplateParameter *template_parameter_surface = nullptr;
    SgTemplateDeclaration *template_parameter_identity = nullptr;
    SgDeclarationStatement *template_parameter_owner = nullptr;

    std::string base_name;
    if (auto *template_parameter =
            llvm::dyn_cast_or_null<clang::TemplateTemplateParmDecl>(
                template_decl)) {
      const TemplateParameterSurfaceFrame *frame = nullptr;
      template_parameter_surface = lookupActiveTemplateParameterSurface(
          template_parameter, "nested-template-specialization-type", &frame);
      if (template_parameter_surface == nullptr) {
        auto mapped = p_decl_translation_map.find(template_parameter);
        template_parameter_surface = mapped != p_decl_translation_map.end()
                                         ? isSgTemplateParameter(mapped->second)
                                         : nullptr;
      }
      template_parameter_identity =
          isSgTemplateDeclaration(resolveNonrealTemplateDeclarationTarget(
              template_parameter, "nested-template-specialization-type"));
      if (template_parameter_surface == nullptr ||
          template_parameter_identity == nullptr ||
          template_parameter_surface->get_templateDeclaration() !=
              template_parameter_identity ||
          template_parameter_surface->get_parameterType() !=
              SgTemplateParameter::template_parameter) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[template-template-qualifier-identity]: "
                "parameter=%p depth=%u index=%u has no exact typed Sage "
                "template identity\n",
                static_cast<void *>(template_parameter),
                template_parameter->getDepth(), template_parameter->getIndex());
        ROSE_ABORT();
      }
      template_parameter_owner =
          isSgDeclarationStatement(template_parameter_surface->get_parent());
      if (frame != nullptr && frame->owner != nullptr) {
        ROSE_ASSERT(template_parameter_owner == nullptr ||
                    template_parameter_owner == frame->owner);
        template_parameter_owner = frame->owner;
      }
      base_name = template_parameter_identity->get_name().getString();
    } else {
      base_name =
          getTemplateNameBase(tname, p_compiler_instance->getASTContext());
    }
    ROSE_ASSERT(!base_name.empty());

    // A nested-name-specifier is a source surface.  Preserve only arguments
    // written in that qualifier; Clang's completed specialization argument
    // list also contains defaults and constraint-only implementation types
    // that have no spelling here.
    SgTemplateArgumentPtrList tpl_args;
    {
      SemanticExpressionConstruction semantic_arguments(
          p_semantic_template_argument_expression_depth,
          "nested-name-specifier-semantic-template-id");
      for (const clang::TemplateArgument &arg : tst->template_arguments()) {
        appendTemplateArguments(tpl_args, arg,
                                /*explicitlySpecified=*/true);
      }
    }
    clang::NestedNameSpecifier qualifier = std::nullopt;
    bool has_template_keyword = false;
    if (const clang::QualifiedTemplateName *qtn =
            tname.getAsQualifiedTemplateName()) {
      qualifier = qtn->getQualifier();
      has_template_keyword = qtn->hasTemplateKeyword();
    } else if (const clang::DependentTemplateName *dtn =
                   tname.getAsDependentTemplateName()) {
      qualifier = dtn->getQualifier();
      has_template_keyword = true;
    }
    clang::Decl *translated_decl_key = template_decl;
    if (!tst->isDependentType() && !tst->isTypeAlias() &&
        p_explicit_template_id_type_use_depth == 0) {
      if (const clang::CXXRecordDecl *record =
              cxxRecordDeclFromQualTypeWithoutDefinitionLookup(
                  clang::QualType(tst, 0)))
        translated_decl_key = const_cast<clang::CXXRecordDecl *>(record);
    }
    if (auto *specialization =
            llvm::dyn_cast_or_null<clang::ClassTemplateSpecializationDecl>(
                translated_decl_key))
      queuePendingImplicitClassTemplateSpecialization(specialization);
    SgDeclarationStatement *translated_decl =
        translated_decl_key != nullptr
            ? resolveNonrealTemplateDeclarationTarget(
                  translated_decl_key, "dependent-template-specialization")
            : nullptr;
    if (tst->isTypeAlias() && !tst->isInstantiationDependentType() &&
        !llvm::isa_and_nonnull<clang::BuiltinTemplateDecl>(
            translated_decl_key)) {
      llvm::SaveAndRestore<unsigned> semantic_alias(
          p_explicit_template_id_type_use_depth, 0);
      auto *alias =
          isSgTypedefType(buildTypeFromQualifiedType(clang::QualType(tst, 0)));
      auto *instantiation = isSgTemplateInstantiationTypedefDeclaration(
          alias != nullptr ? alias->get_declaration() : nullptr);
      ROSE_ASSERT(instantiation != nullptr &&
                  instantiation->get_templateDeclaration() == translated_decl);
      translated_decl = instantiation;
    }
    SgType *aliased_type = buildDependentAliasUnderlyingType(tst);
    SgNonrealType *nrtype = nullptr;
    if (qualifier) {
      nrtype = build_with_qualifier(qualifier, base_name, &tpl_args,
                                    translated_decl, aliased_type);
    } else {
      clang::NestedNameSpecifier ns_qualifier = std::nullopt;
      clang::DeclContext *template_decl_context =
          template_decl != nullptr ? template_decl->getDeclContext() : nullptr;
      if (!prefer_current_scope && template_parameter_identity == nullptr &&
          template_decl_context != nullptr &&
          canSynthesizeNamespaceQualifierFromDeclContext(
              template_decl_context)) {
        SgScopeStatement *qualification_scope = scope;
        if (qualification_scope == nullptr) {
          qualification_scope = SageBuilder::topScopeStack();
        }
        if (qualification_scope == nullptr) {
          qualification_scope = getGlobalScope();
        }
        const bool scope_within_namespace_chain = scopeIsWithinNamespaceChain(
            qualification_scope, template_decl_context);
        // A declaration-context namespace is needed only when it is not
        // already reachable from the lexical construction scope.  Adding it
        // while translating an unqualified template-id inside that same
        // namespace changes the written type identity (for example,
        // `enable_if<T>::type` becomes `std::enable_if<T>::type`).
        if (!scope_within_namespace_chain) {
          ns_qualifier = buildNamespaceQualifierForDeclContext(
              template_decl_context, p_compiler_instance->getASTContext());
        }
      }
      if (ns_qualifier) {
        nrtype = build_with_qualifier(ns_qualifier, base_name, &tpl_args,
                                      translated_decl, aliased_type);
      } else {
        SgScopeStatement *template_scope = scope;
        if (!prefer_current_scope && template_parameter_identity != nullptr) {
          SgDeclarationStatement *parameter_owner =
              template_parameter_owner != nullptr
                  ? template_parameter_owner
                  : (template_parameter_surface != nullptr
                         ? isSgDeclarationStatement(
                               template_parameter_surface->get_parent())
                         : nullptr);
          if (parameter_owner != nullptr) {
            SgDeclarationScope *parameter_scope =
                SageBuilder::getOrCreateNonrealDeclarationScope(
                    parameter_owner);
            template_scope = parameter_scope;
            if (parameter_scope == nullptr ||
                SageBuilder::getDeclarationScopeOwner(parameter_scope) !=
                    parameter_owner) {
              fprintf(stderr,
                      "REX_FRONTEND_INVARIANT[template-template-qualifier-"
                      "scope]: parameter=%p identity=%p has no exact owning "
                      "declaration scope\n",
                      static_cast<void *>(template_parameter_surface),
                      static_cast<void *>(template_parameter_identity));
              ROSE_ABORT();
            }
          } else {
            size_t active_detached_surface_count = 0;
            for (const TemplateParameterSurfaceFrame &frame :
                 p_template_parameter_surface_stack) {
              if (frame.owner == nullptr && frame.sage_parameters != nullptr &&
                  std::find(frame.sage_parameters->begin(),
                            frame.sage_parameters->end(),
                            template_parameter_surface) !=
                      frame.sage_parameters->end()) {
                ++active_detached_surface_count;
              }
            }
            if (active_detached_surface_count != 1 ||
                template_parameter_surface == nullptr ||
                template_parameter_surface->get_parent() != nullptr ||
                template_parameter_identity->get_scope() != nullptr ||
                scope == nullptr) {
              fprintf(
                  stderr,
                  "REX_FRONTEND_INVARIANT[template-template-qualifier-"
                  "construction-scope]: parameter=%p identity=%p has "
                  "active-detached-count=%zu parent=%p identity-scope=%p "
                  "type-use-scope=%p\n",
                  static_cast<void *>(template_parameter_surface),
                  static_cast<void *>(template_parameter_identity),
                  active_detached_surface_count,
                  template_parameter_surface != nullptr
                      ? static_cast<void *>(
                            template_parameter_surface->get_parent())
                      : nullptr,
                  static_cast<void *>(template_parameter_identity->get_scope()),
                  static_cast<void *>(scope));
              ROSE_ABORT();
            }
            // A detached template header is a bounded construction input for a
            // function builder.  Its exact type-use scope is the scope passed
            // by that producer; the templateDeclaration edge above retains the
            // parameter identity until the builder adopts the same parameter
            // node.  No name or ownership is reconstructed afterward.
            template_scope = scope;
          }
        } else if (!prefer_current_scope && template_decl != nullptr) {
          clang::DeclContext *decl_context = template_decl->getDeclContext();
          if (decl_context == nullptr) {
            std::cerr << "REX_FRONTEND_INVARIANT[dependent-template-scope]: "
                         "template declaration has no context"
                      << std::endl;
            ROSE_ABORT();
          }
          template_scope = resolveScopeFromDeclContext(decl_context);
          if (template_scope == nullptr) {
            std::cerr << "REX_FRONTEND_INVARIANT[dependent-template-scope]: "
                         "template declaration context has no exact scope"
                      << std::endl;
            ROSE_ABORT();
          }
        }
        ASSERT_not_null(template_scope);
        SgScopeStatement *use_scope =
            template_use_scope != nullptr ? template_use_scope : template_scope;
        nrtype = SageBuilder::buildSemanticNonrealType(
            SgName(base_name), use_scope, &tpl_args, translated_decl,
            aliased_type);
      }
    }
    ROSE_ASSERT(nrtype != nullptr);
    SgNonrealDecl *nrdecl = isSgNonrealDecl(nrtype->get_declaration());
    ROSE_ASSERT(nrdecl != nullptr);
    if (has_template_keyword) {
      nrdecl->set_has_template_keyword(true);
    }
    if (template_parameter_surface != nullptr) {
      nrdecl->set_is_template_param(true);
      nrdecl->set_template_parameter_depth(static_cast<int>(
          llvm::cast<clang::TemplateTemplateParmDecl>(template_decl)
              ->getDepth()));
      nrdecl->set_template_parameter_position(static_cast<int>(
          llvm::cast<clang::TemplateTemplateParmDecl>(template_decl)
              ->getIndex()));
    }
    if (!tst->isDependentType()) {
      nrdecl->set_suppress_typename(true);
    }
    return nrtype;
  }

  if (const clang::TagType *tag_type =
          llvm::dyn_cast<clang::TagType>(clang_type)) {
    const clang::TagDecl *decl = tag_type->getDecl();
    ROSE_ASSERT(decl != nullptr);
    std::string name_str = decl->getNameAsString();
    clang::NestedNameSpecifier qualifier = tag_type->getQualifier();
    if (!name_str.empty() && qualifier) {
      SgDeclarationStatement *identity =
          resolveNonrealTemplateDeclarationTarget(
              const_cast<clang::TagDecl *>(decl), "dependent-named-type");
      return build_with_qualifier(qualifier, name_str, nullptr, identity);
    }
  }

  if (const clang::TemplateTypeParmType *ttp =
          llvm::dyn_cast<clang::TemplateTypeParmType>(clang_type)) {
    std::string name_str;
    SgScopeStatement *template_param_scope = scope;
    auto get_template_param_scope =
        [&](clang::NamedDecl *param_decl) -> SgDeclarationScope * {
      SgTemplateParameter *active_semantic_parameter =
          lookupActiveSemanticTemplateParameterSurface(
              param_decl, "buildNonrealTypeFromClangType");
      SgTemplateParameter *active_syntax_parameter =
          lookupActiveTemplateParameterSurface(param_decl,
                                               "buildNonrealTypeFromClangType");
      SgDeclarationScope *construction_scope =
          lookupActiveTemplateParameterConstructionScope(
              param_decl, "buildNonrealTypeFromClangType");
      SgDeclarationStatement *active_surface_owner =
          lookupActiveTemplateParameterSurfaceOwner(
              param_decl, "buildNonrealTypeFromClangType");
      if (active_semantic_parameter != nullptr) {
        SgDeclarationStatement *owner =
            isSgDeclarationStatement(active_semantic_parameter->get_parent());
        SgTemplateParameterPtrList *owner_parameters =
            owner != nullptr ? SageBuilder::getTemplateParameterList(owner)
                             : nullptr;
        if (owner_parameters == nullptr ||
            std::count(owner_parameters->begin(), owner_parameters->end(),
                       active_semantic_parameter) != 1) {
          std::cerr
              << "REX_FRONTEND_INVARIANT[template-parameter-scope]: active "
                 "semantic parameter has no single exact attached owner"
              << std::endl;
          ROSE_ABORT();
        }
        SgDeclarationScope *decl_scope =
            SageBuilder::getOrCreateNonrealDeclarationScope(owner);
        if (decl_scope == nullptr ||
            SageBuilder::getDeclarationScopeOwner(decl_scope) != owner) {
          std::cerr << "REX_FRONTEND_INVARIANT[template-parameter-scope]: "
                       "parameter owner has no exact declaration scope"
                    << std::endl;
          ROSE_ABORT();
        }
        return decl_scope;
      }
      if (active_surface_owner != nullptr) {
        SgTemplateParameterPtrList *owner_parameters =
            SageBuilder::getTemplateParameterList(active_surface_owner);
        const size_t owner_membership_count =
            owner_parameters != nullptr && active_syntax_parameter != nullptr
                ? std::count(owner_parameters->begin(), owner_parameters->end(),
                             active_syntax_parameter)
                : 0;
        const bool detached_construction =
            active_syntax_parameter != nullptr &&
            active_syntax_parameter->get_parent() == nullptr &&
            owner_membership_count == 0;
        const bool exact_attached_revisit =
            active_syntax_parameter != nullptr &&
            active_syntax_parameter->get_parent() == active_surface_owner &&
            owner_membership_count == 1;
        if (owner_parameters == nullptr ||
            (!detached_construction && !exact_attached_revisit) ||
            construction_scope != nullptr) {
          fprintf(
              stderr,
              "REX_FRONTEND_INVARIANT[template-parameter-attached-"
              "construction-scope]: parameter=%p syntax=%p parent=%p "
              "owner=%p/%s owner-membership=%zu construction-scope=%p has no "
              "single exact attached transaction\n",
              static_cast<void *>(param_decl),
              static_cast<void *>(active_syntax_parameter),
              static_cast<void *>(active_syntax_parameter != nullptr
                                      ? active_syntax_parameter->get_parent()
                                      : nullptr),
              static_cast<void *>(active_surface_owner),
              active_surface_owner->class_name().c_str(),
              owner_membership_count, static_cast<void *>(construction_scope));
          ROSE_ABORT();
        }
        SgDeclarationScope *decl_scope =
            SageBuilder::getOrCreateNonrealDeclarationScope(
                active_surface_owner);
        if (decl_scope == nullptr || SageBuilder::getDeclarationScopeOwner(
                                         decl_scope) != active_surface_owner) {
          std::cerr << "REX_FRONTEND_INVARIANT[template-parameter-scope]: "
                       "active attached owner has no exact declaration scope"
                    << std::endl;
          ROSE_ABORT();
        }
        return decl_scope;
      }
      if (active_syntax_parameter != nullptr) {
        if (construction_scope == nullptr) {
          const std::string parameter_location =
              param_decl->getLocation().printToString(
                  p_compiler_instance->getSourceManager());
          fprintf(stderr,
                  "REX_FRONTEND_INVARIANT[template-parameter-construction-"
                  "scope]: parameter=%p name='%s' kind=%s source=%s syntax=%p "
                  "parent=%p scope=%p current=%p active-surfaces=%zu has no "
                  "exact construction scope\n",
                  static_cast<void *>(param_decl),
                  param_decl->getQualifiedNameAsString().c_str(),
                  param_decl->getDeclKindName(), parameter_location.c_str(),
                  static_cast<void *>(active_syntax_parameter),
                  static_cast<void *>(active_syntax_parameter->get_parent()),
                  static_cast<void *>(construction_scope),
                  static_cast<void *>(SageBuilder::topScopeStack()),
                  p_template_parameter_surface_stack.size());
          ROSE_ABORT();
        }
        SgNode *construction_owner =
            SageBuilder::getDeclarationScopeOwner(construction_scope);
        if (active_syntax_parameter->get_parent() == nullptr) {
          SgDeclarationStatement *construction_declaration =
              isSgDeclarationStatement(construction_owner);
          SgTemplateParameterPtrList *construction_parameters =
              construction_declaration != nullptr
                  ? SageBuilder::getTemplateParameterList(
                        construction_declaration)
                  : nullptr;
          SgScopeStatement *provisional_scope_owner =
              isSgScopeStatement(construction_owner);
          SgDeclarationScopeList *provisional_scope_container =
              provisional_scope_owner != nullptr
                  ? provisional_scope_owner->get_auxiliary_declaration_scopes()
                  : nullptr;
          const bool exact_provisional_scope_owner =
              provisional_scope_container != nullptr &&
              provisional_scope_container->get_parent() ==
                  provisional_scope_owner &&
              construction_scope->get_parent() == provisional_scope_container &&
              std::count(provisional_scope_container->get_scopes().begin(),
                         provisional_scope_container->get_scopes().end(),
                         construction_scope) == 1;
          const bool exact_empty_declaration_owner =
              construction_declaration != nullptr &&
              construction_parameters != nullptr &&
              construction_parameters->empty() &&
              construction_scope->get_parent() == construction_declaration;
          if (!exact_provisional_scope_owner &&
              !exact_empty_declaration_owner) {
            fprintf(stderr,
                    "REX_FRONTEND_INVARIANT[template-parameter-construction-"
                    "scope]: detached parameter=%p syntax=%p scope=%p "
                    "current=%p owner=%p/%s owner-parameter-count=%zu is not "
                    "one exact structurally owned active transaction\n",
                    static_cast<void *>(param_decl),
                    static_cast<void *>(active_syntax_parameter),
                    static_cast<void *>(construction_scope),
                    static_cast<void *>(SageBuilder::topScopeStack()),
                    static_cast<void *>(construction_owner),
                    construction_owner != nullptr
                        ? construction_owner->class_name().c_str()
                        : "<null>",
                    construction_parameters != nullptr
                        ? construction_parameters->size()
                        : 0);
            ROSE_ABORT();
          }
        } else {
          SgFunctionDeclaration *function_owner =
              isSgFunctionDeclaration(active_syntax_parameter->get_parent());
          SgTemplateParameterPtrList *owned_parameters =
              function_owner != nullptr
                  ? SageBuilder::getTemplateParameterList(function_owner)
                  : nullptr;
          if (function_owner == nullptr || owned_parameters == nullptr ||
              std::count(owned_parameters->begin(), owned_parameters->end(),
                         active_syntax_parameter) != 1 ||
              function_owner->get_function_declarator_scope() !=
                  construction_scope ||
              construction_owner != function_owner) {
            fprintf(stderr,
                    "REX_FRONTEND_INVARIANT[template-parameter-construction-"
                    "scope]: adopted parameter=%p syntax=%p parent=%p/%s "
                    "scope=%p owner=%p/%s does not preserve one exact "
                    "function declarator transaction\n",
                    static_cast<void *>(param_decl),
                    static_cast<void *>(active_syntax_parameter),
                    static_cast<void *>(active_syntax_parameter->get_parent()),
                    active_syntax_parameter->get_parent()->class_name().c_str(),
                    static_cast<void *>(construction_scope),
                    static_cast<void *>(construction_owner),
                    construction_owner != nullptr
                        ? construction_owner->class_name().c_str()
                        : "<null>");
            ROSE_ABORT();
          }
        }
        return construction_scope;
      }

      // A concept's parameter shell can already own its declaration scope
      // while the concept itself is still under construction. Consume that
      // exact typed edge without recursively translating the concept again.
      SgDeclarationStatement *owner = nullptr;
      auto mapped = p_decl_translation_map.find(param_decl);
      if (mapped != p_decl_translation_map.end()) {
        auto *parameter = isSgTemplateParameter(mapped->second);
        ASSERT_not_null(parameter);
        owner = isSgDeclarationStatement(parameter->get_parent());
        ASSERT_not_null(owner);
        const auto *parameters =
            templateParametersForSageDeclarationShared(owner);
        ROSE_ASSERT(
            parameters != nullptr &&
            std::count(parameters->begin(), parameters->end(), parameter) == 1);
      } else {
        owner =
            resolveTemplateParameterOwner(param_decl, /*allow_on_demand=*/true);
      }
      SgDeclarationScope *decl_scope =
          SageBuilder::getOrCreateNonrealDeclarationScope(owner);
      if (decl_scope == nullptr ||
          SageBuilder::getDeclarationScopeOwner(decl_scope) != owner) {
        std::cerr << "REX_FRONTEND_INVARIANT[template-parameter-scope]: "
                     "parameter owner has no exact declaration scope"
                  << std::endl;
        ROSE_ABORT();
      }
      return decl_scope;
    };
    const clang::TemplateTypeParmDecl *parameter = ttp->getDecl();
    if (p_canonical_template_argument_context.has_value()) {
      const auto *bound = llvm::dyn_cast_or_null<clang::TemplateTypeParmDecl>(
          resolveTemplateParameterFromDeclContext(
              *p_canonical_template_argument_context, ttp->getDepth(),
              ttp->getIndex(), currentExactTemplateParameters()));
      ROSE_ASSERT(bound != nullptr &&
                  (parameter == nullptr ||
                   bound->isParameterPack() == parameter->isParameterPack()));
      parameter = bound;
    }
    if (parameter != nullptr) {
      if (const clang::TemplateTypeParmDecl *surface =
              resolveActiveTemplateTypeParameterSourceCounterpart(
                  parameter, "nested-template-type-parameter-source")) {
        parameter = surface;
      }
    } else {
      parameter = resolveActiveTemplateTypeParameterSurface(
          ttp->getDepth(), ttp->getIndex(),
          "nested-template-type-parameter-coordinate");
      // Canonical parameter types omit their declaration. The active
      // declarator retains the exact template pattern and its typed parameter
      // list, even after the source-header construction frame has closed.
      if (parameter == nullptr) {
        for (auto context = p_template_parameter_decl_context_stack.rbegin();
             context != p_template_parameter_decl_context_stack.rend();
             ++context) {
          const auto *candidate = resolveTemplateParameterFromDeclContext(
              *context, ttp->getDepth(), ttp->getIndex());
          if (candidate != nullptr) {
            parameter = llvm::cast<clang::TemplateTypeParmDecl>(candidate);
            break;
          }
        }
      }
      if (parameter == nullptr) {
        parameter = resolveExactTemplateTypeParameterDeclaration(
            ttp->getDepth(), ttp->getIndex());
      }
    }
    if (parameter == nullptr || parameter->getIdentifier() == nullptr) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[template-type-parameter]: depth=%u "
              "index=%u has no exact named qualifier declaration\n",
              ttp->getDepth(), ttp->getIndex());
      ROSE_ABORT();
    }
    name_str = parameter->getNameAsString();
    template_param_scope = get_template_param_scope(
        const_cast<clang::TemplateTypeParmDecl *>(parameter));

    SgNonrealType *nrtype = SageBuilder::buildSemanticNonrealType(
        SgName(name_str), template_param_scope, nullptr);
    if (SgNonrealDecl *nrdecl =
            isSgNonrealDecl(nrtype ? nrtype->get_declaration() : nullptr)) {
      nrdecl->set_is_template_param(true);
      nrdecl->set_template_parameter_depth(
          static_cast<int>(parameter->getDepth()));
      nrdecl->set_template_parameter_position(
          static_cast<int>(parameter->getIndex()));
      nrdecl->set_suppress_typename(true);
    }
    return nrtype;
  }

  if (const clang::TagType *tag = llvm::dyn_cast<clang::TagType>(clang_type)) {
    clang::TagDecl *tag_decl = tag->getDecl();
    if (const clang::ClassTemplateSpecializationDecl *specialization =
            llvm::dyn_cast_or_null<clang::ClassTemplateSpecializationDecl>(
                tag_decl)) {
      const clang::ClassTemplateDecl *specialized_template =
          llvm::dyn_cast_or_null<clang::ClassTemplateDecl>(
              specialization->getSpecializedTemplate());
      if (specialized_template == nullptr) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[nested-specialization-template]: "
                "class-template specialization=%p has no exact specialized "
                "template declaration\n",
                static_cast<const void *>(specialization));
        ROSE_ABORT();
      }
      const std::string base_name = specialized_template->getNameAsString();
      if (base_name.empty()) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[nested-specialization-name]: "
                "class-template specialization=%p has no exact template "
                "name\n",
                static_cast<const void *>(specialization));
        ROSE_ABORT();
      }

      const clang::TemplateArgumentList &arguments =
          specialization->getTemplateArgs();
      // This branch can reconstruct a declaration-local template-id from the
      // canonical RecordType reached through a substituted TypeLoc.  A nested
      // implicit specialization, however, owns no declaration-local argument
      // spelling merely because an outer TypeLoc or reconstructed template
      // argument is active; the outer producer owns that source surface.
      // Require an exact as-written declaration argument list before assigning
      // a source role to this canonical specialization.  In particular, a
      // semantic instantiation such as tuple<closure-type> must not claim that
      // the anonymous closure was written as a nested template argument.
      const clang::ASTTemplateArgumentListInfo *declaration_arguments =
          specialization->getTemplateArgsAsWritten();
      const bool reconstruct_source_surface =
          p_explicit_template_id_type_loc_use_depth != 0 &&
          declaration_arguments != nullptr;
      SgTemplateArgumentPtrList template_arguments = buildTemplateArguments(
          arguments, arguments.size(), reconstruct_source_surface,
          declaration_arguments);
      if (reconstruct_source_surface) {
        for (size_t index = 0; index < template_arguments.size(); ++index) {
          SgTemplateArgument *sage_argument = template_arguments[index];
          SgClassType *class_type =
              sage_argument != nullptr && sage_argument->get_argumentType() ==
                                              SgTemplateArgument::type_argument
                  ? isSgClassType(sage_argument->get_type())
                  : nullptr;
          SgClassDeclaration *class_declaration =
              class_type != nullptr
                  ? isSgClassDeclaration(class_type->get_declaration())
                  : nullptr;
          if (class_declaration == nullptr ||
              !class_declaration->get_isUnNamed()) {
            continue;
          }
          if (index >= arguments.size() ||
              arguments.get(index).getKind() != clang::TemplateArgument::Type) {
            fprintf(stderr,
                    "REX_FRONTEND_INVARIANT[explicit-instantiation-type-"
                    "surface]: specialization=%s anonymous argument=%zu "
                    "has no matching semantic type argument\n",
                    specialization->getQualifiedNameAsString().c_str(), index);
            ROSE_ABORT();
          }
          SgType *source_type = sage_argument->get_sourceSpelledType();
          requireExactType(source_type, "explicit-instantiation-type-surface",
                           arguments.get(index).getAsType().getTypePtrOrNull());
          SgClassType *source_class_type = isSgClassType(source_type);
          SgClassDeclaration *source_class_declaration =
              source_class_type != nullptr
                  ? isSgClassDeclaration(source_class_type->get_declaration())
                  : nullptr;
          SgNonrealDecl *source_nonreal_declaration = isSgNonrealDecl(
              isSgNonrealType(source_type) != nullptr
                  ? isSgNonrealType(source_type)->get_declaration()
                  : nullptr);
          const bool has_exact_named_source_identity =
              isSgTypedefType(source_type) != nullptr ||
              (source_nonreal_declaration != nullptr &&
               !source_nonreal_declaration->get_name().is_null() &&
               !source_nonreal_declaration->get_name().getString().empty() &&
               source_nonreal_declaration->get_templateDeclaration() !=
                   nullptr);
          if ((source_class_declaration != nullptr &&
               source_class_declaration->get_isUnNamed()) ||
              !has_exact_named_source_identity) {
            fprintf(stderr,
                    "REX_FRONTEND_INVARIANT[explicit-instantiation-type-"
                    "surface]: specialization=%s argument=%zu declaration "
                    "TypeLoc did not produce one exact named Sage type\n",
                    specialization->getQualifiedNameAsString().c_str(), index);
            ROSE_ABORT();
          }
        }
      }

      clang::NestedNameSpecifier qualifier = std::nullopt;
      clang::DeclContext *specialization_context =
          const_cast<clang::DeclContext *>(specialization->getDeclContext());
      if (!prefer_current_scope &&
          canSynthesizeNamespaceQualifierFromDeclContext(
              specialization_context) &&
          !scopeIsWithinNamespaceChain(scope, specialization_context)) {
        qualifier = buildNamespaceQualifierForDeclContext(
            specialization_context, p_compiler_instance->getASTContext());
      }
      SgDeclarationStatement *identity =
          resolveNonrealTemplateDeclarationTarget(
              const_cast<clang::ClassTemplateSpecializationDecl *>(
                  specialization),
              "tag-template-specialization");
      return build_with_qualifier(qualifier, base_name, &template_arguments,
                                  identity);
    }

    std::string name_str = tag_decl->getNameAsString();
    if (name_str.empty()) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[tag-type-name]: TagType has no exact "
              "declared name\n");
      ROSE_ABORT();
    }
    clang::NestedNameSpecifier qualifier = std::nullopt;
    if (!prefer_current_scope && canSynthesizeNamespaceQualifierFromDeclContext(
                                     tag_decl->getDeclContext())) {
      if (!scopeIsWithinNamespaceChain(scope, tag_decl->getDeclContext())) {
        qualifier = buildNamespaceQualifierForDeclContext(
            tag_decl->getDeclContext(), p_compiler_instance->getASTContext());
      }
    }
    SgDeclarationStatement *identity =
        resolveNonrealTemplateDeclarationTarget(tag_decl, "tag-type-identity");
    return build_with_qualifier(qualifier, name_str, nullptr, identity);
  }

  std::string name_str;
  if (const clang::TypeDecl *decl = clang_type->getAsTagDecl()) {
    name_str = decl->getNameAsString();
  }
  if (name_str.empty()) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[dependent-type-name]: Clang type class %s "
            "has no exact representable name\n",
            clang_type->getTypeClassName());
    ROSE_ABORT();
  }
  return build_with_qualifier(std::nullopt, name_str, nullptr);
}

SgNonrealType *
ClangToSageTranslator::buildSemanticNonrealTypeFromNestedNameSpecifier(
    clang::NestedNameSpecifier qualifier, SgScopeStatement *scope,
    const SgName &terminalName,
    const SgTemplateArgumentPtrList *terminalTemplateArgs,
    SgDeclarationStatement *terminalDeclaration, SgType *aliasedType) {
  SgScopeStatement *lexical_scope = scope;
  if (lexical_scope == nullptr) {
    lexical_scope = SageBuilder::topScopeStack();
  }
  ROSE_ASSERT(lexical_scope != nullptr);

  SgScopeStatement *chain_scope =
      buildNonrealScopeFromNestedNameSpecifier(qualifier, lexical_scope);
  ROSE_ASSERT(chain_scope != nullptr);

  SgNonrealType *nrtype = nullptr;
  if (terminalTemplateArgs == nullptr) {
    nrtype = SageBuilder::buildSemanticNonrealType(
        terminalName, chain_scope, nullptr, terminalDeclaration, aliasedType);
  } else {
    // Template arguments belong to the lexical use. A resolved template binds
    // at construction; an unresolved dependent terminal belongs to its exact
    // qualifier chain.
    SgDeclarationStatement *template_declaration = terminalDeclaration;
    if (template_declaration == nullptr) {
      SgNonrealType *qualified_base = SageBuilder::buildSemanticNonrealType(
          terminalName, chain_scope, nullptr);
      SgNonrealDecl *qualified_base_declaration = isSgNonrealDecl(
          qualified_base != nullptr ? qualified_base->get_declaration()
                                    : nullptr);
      if (!isExactSimpleNonrealTemplateBase(qualified_base_declaration,
                                            terminalName)) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[nonreal-template-use-owner]: "
                "terminal=%s has no exact simple identity in its qualifier "
                "chain\n",
                terminalName.getString().c_str());
        ROSE_ABORT();
      }
      template_declaration = qualified_base_declaration;
    }

    SgScopeStatement *local_dependency_owner = nullptr;
    for (SgTemplateArgument *argument : *terminalTemplateArgs) {
      ASSERT_not_null(argument);
      for (SgNode *node : NodeQuery::querySubTree(argument, V_SgVarRefExp)) {
        SgVarRefExp *reference = isSgVarRefExp(node);
        SgInitializedName *declaration =
            reference != nullptr && reference->get_symbol() != nullptr
                ? reference->get_symbol()->get_declaration()
                : nullptr;
        SgScopeStatement *declaration_scope =
            declaration != nullptr ? declaration->get_scope() : nullptr;
        if (declaration_scope == nullptr) {
          fprintf(stderr,
                  "REX_FRONTEND_INVARIANT[nonreal-template-use-owner]: "
                  "terminal=%s argument variable reference=%p has no exact "
                  "declaration scope\n",
                  terminalName.getString().c_str(),
                  static_cast<void *>(reference));
          ROSE_ABORT();
        }
        if (isSgGlobal(declaration_scope) != nullptr ||
            isSgNamespaceDefinitionStatement(declaration_scope) != nullptr ||
            isSgClassDefinition(declaration_scope) != nullptr) {
          continue;
        }
        if (local_dependency_owner == nullptr ||
            declaration_scope == local_dependency_owner ||
            SageInterface::isAncestor(local_dependency_owner,
                                      declaration_scope)) {
          local_dependency_owner = declaration_scope;
        } else if (!SageInterface::isAncestor(declaration_scope,
                                              local_dependency_owner)) {
          fprintf(stderr,
                  "REX_FRONTEND_INVARIANT[nonreal-template-use-owner]: "
                  "terminal=%s has local argument dependencies in unrelated "
                  "scopes=%p/%s and %p/%s\n",
                  terminalName.getString().c_str(),
                  static_cast<void *>(local_dependency_owner),
                  local_dependency_owner->class_name().c_str(),
                  static_cast<void *>(declaration_scope),
                  declaration_scope->class_name().c_str());
          ROSE_ABORT();
        }
      }
    }
    SgScopeStatement *use_scope = local_dependency_owner != nullptr
                                      ? local_dependency_owner
                                      : chain_scope;
    nrtype = SageBuilder::buildSemanticNonrealType(
        terminalName, use_scope, terminalTemplateArgs, template_declaration,
        aliasedType);
    SgNonrealDecl *use_declaration = isSgNonrealDecl(
        nrtype != nullptr ? nrtype->get_declaration() : nullptr);
    if (use_declaration == nullptr || use_declaration == template_declaration ||
        use_declaration->get_templateDeclaration() != template_declaration ||
        use_declaration->get_scope() == nullptr ||
        use_declaration->get_parent() != use_declaration->get_scope()) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[nonreal-template-use-owner]: "
              "terminal=%s target=%p and lexical use=%p do not form "
              "distinct exact identities\n",
              terminalName.getString().c_str(),
              static_cast<void *>(template_declaration),
              static_cast<void *>(use_declaration));
      ROSE_ABORT();
    }
  }
  ROSE_ASSERT(nrtype != nullptr);

  SgNonrealDecl *nrdecl = isSgNonrealDecl(nrtype->get_declaration());
  ROSE_ASSERT(nrdecl != nullptr);
  if (qualifier && !nestedNameSpecifierHasDependentTypeQualifier(qualifier)) {
    nrdecl->set_suppress_typename(true);
  }

  // Preserve a leading global qualifier for terminal references so namespace
  // lookup does not accidentally bind to a local shadow.
  if (qualifier && nestedNameSpecifierHasGlobal(qualifier)) {
    nrdecl->set_has_global_qualifier(true);
  }

  return nrtype;
}

SgScopeStatement *
ClangToSageTranslator::buildNonrealScopeFromNestedNameSpecifier(
    clang::NestedNameSpecifier qualifier, SgScopeStatement *scope) {

  if (!qualifier) {
    return scope;
  }

  std::vector<clang::NestedNameSpecifier> segments;
  for (clang::NestedNameSpecifier nns = qualifier; nns;
       nns = nestedNameSpecifierPrefix(nns)) {

    segments.push_back(nns);
  }

  SgScopeStatement *current_scope = scope;
  if (nestedNameSpecifierHasNamespaceQualifier(qualifier)) {
    current_scope = getGlobalScope();
  }
  for (auto it = segments.rbegin(); it != segments.rend(); ++it) {
    clang::NestedNameSpecifier nns = *it;
    if (nns.getKind() == clang::NestedNameSpecifier::Kind::Global) {
      current_scope = getGlobalScope();
      continue;
    }

    SgNonrealType *segment_type = nullptr;
    switch (nns.getKind()) {
    case clang::NestedNameSpecifier::Kind::Namespace: {
      const clang::NamespaceBaseDecl *ns =
          llvm::dyn_cast_or_null<clang::NamespaceBaseDecl>(
              nestedNameSpecifierNamespaceBase(nns));
      ROSE_ASSERT(ns != nullptr);
      std::string name_str = ns->getNameAsString();
      ROSE_ASSERT(!name_str.empty());
      segment_type = SageBuilder::buildSemanticNonrealType(
          SgName(name_str), current_scope, nullptr);
      break;
    }
    case clang::NestedNameSpecifier::Kind::Type: {
      bool prefer_current = static_cast<bool>(nestedNameSpecifierPrefix(nns));
      segment_type = buildNonrealTypeForNestedNameSpecifierType(
          nns.getAsType(), current_scope, prefer_current, scope);
      break;
    }
    case clang::NestedNameSpecifier::Kind::MicrosoftSuper:
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[nonreal-qualifier]: __super has no "
              "typed nonreal qualifier representation\n");
      ROSE_ABORT();
    case clang::NestedNameSpecifier::Kind::Global:
    case clang::NestedNameSpecifier::Kind::Null:
      break;
    }

    ROSE_ASSERT(segment_type != nullptr);
    SgNonrealDecl *segment_decl =
        isSgNonrealDecl(segment_type->get_declaration());
    ROSE_ASSERT(segment_decl != nullptr);
    if (nestedNameSpecifierHasTemplateKeyword(nns)) {
      segment_decl->set_has_template_keyword(true);
    }
    if (clang::NestedNameSpecifier prefix = nestedNameSpecifierPrefix(nns)) {
      if (prefix.getKind() == clang::NestedNameSpecifier::Kind::Global) {
        segment_decl->set_has_global_qualifier(true);
      }
    }
    current_scope = segment_decl->get_nonreal_decl_scope();
    ROSE_ASSERT(current_scope != nullptr);
  }

  ROSE_ASSERT(current_scope != nullptr);
  return current_scope;
}

SgTemplateInstantiationDecl *
ClangToSageTranslator::getOrCreateTemplateInstantiation(
    SgTemplateClassDeclaration *template_decl,
    const clang::TemplateSpecializationType *clang_type) {
  ASSERT_not_null(clang_type);
  ASSERT_not_null(template_decl);
  auto canonical_namespace_scope = [](SgScopeStatement *scope) {
    SgNamespaceDefinitionStatement *namespace_definition =
        isSgNamespaceDefinitionStatement(scope);
    if (namespace_definition == nullptr) {
      return scope;
    }

    SgNamespaceDefinitionStatement *global_definition =
        namespace_definition->get_global_definition();
    ASSERT_not_null(global_definition);
    return static_cast<SgScopeStatement *>(global_definition);
  };

  clang::TemplateDecl *inst_clang_template_decl =
      clang_type->getTemplateName().getAsTemplateDecl();
  ASSERT_not_null(inst_clang_template_decl);
  clang::DeclContext *inst_decl_context = nullptr;
  const clang::ClassTemplateSpecializationDecl *inst_record_spec_decl = nullptr;
  clang::QualType canonical_qt =
      clang::QualType(clang_type, 0).getCanonicalType();
  const clang::Type *canonical_type = canonical_qt.getTypePtrOrNull();
  if (const auto *tag_type =
          llvm::dyn_cast_or_null<clang::TagType>(canonical_type)) {
    const clang::TagDecl *tag_decl =
        llvm::dyn_cast_or_null<clang::TagDecl>(tag_type->getDecl());
    const clang::CXXRecordDecl *record_decl =
        llvm::dyn_cast_or_null<clang::CXXRecordDecl>(tag_decl);
    inst_record_spec_decl =
        llvm::dyn_cast_or_null<clang::ClassTemplateSpecializationDecl>(
            record_decl);
  }
  if (inst_record_spec_decl != nullptr) {
    inst_decl_context = const_cast<clang::DeclContext *>(
        inst_record_spec_decl->getDeclContext());
  } else {
    inst_decl_context = inst_clang_template_decl->getDeclContext();
  }

  auto class_kind_for_record =
      [](const clang::RecordDecl *decl) -> SgClassDeclaration::class_types {
    ASSERT_not_null(decl);
    switch (decl->getTagKind()) {
    case clang::TagTypeKind::Struct:
      return SgClassDeclaration::e_struct;
    case clang::TagTypeKind::Union:
      return SgClassDeclaration::e_union;
    case clang::TagTypeKind::Class:
      return SgClassDeclaration::e_class;
    default:
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[template-instantiation-tag]: "
              "unsupported Clang record tag kind=%d\n",
              static_cast<int>(decl->getTagKind()));
      ROSE_ABORT();
    }
  };

  SgClassDeclaration::class_types inst_class_kind =
      template_decl->get_class_type();
  if (inst_record_spec_decl != nullptr) {
    inst_class_kind = class_kind_for_record(inst_record_spec_decl);
  }

  auto same_template_decl_chain = [](SgTemplateClassDeclaration *lhs,
                                     SgTemplateClassDeclaration *rhs) -> bool {
    if (lhs == nullptr || rhs == nullptr) {
      return false;
    }
    if (lhs == rhs) {
      return true;
    }

    auto first_decl =
        [](SgTemplateClassDeclaration *decl) -> SgTemplateClassDeclaration * {
      return isSgTemplateClassDeclaration(
          decl->get_firstNondefiningDeclaration());
    };
    auto defining_decl =
        [](SgTemplateClassDeclaration *decl) -> SgTemplateClassDeclaration * {
      return isSgTemplateClassDeclaration(decl->get_definingDeclaration());
    };

    SgTemplateClassDeclaration *lhs_first = first_decl(lhs);
    SgTemplateClassDeclaration *rhs_first = first_decl(rhs);
    if (lhs_first != nullptr && lhs_first == rhs_first) {
      return true;
    }

    SgTemplateClassDeclaration *lhs_def = defining_decl(lhs);
    SgTemplateClassDeclaration *rhs_def = defining_decl(rhs);
    if (lhs_def != nullptr && lhs_def == rhs_def) {
      return true;
    }

    return false;
  };

  ROSE_ASSERT(template_decl != nullptr);
  template_decl = isSgTemplateClassDeclaration(
      template_decl->get_firstNondefiningDeclaration());
  ROSE_ASSERT(template_decl != nullptr);
  ROSE_ASSERT(template_decl->get_firstNondefiningDeclaration() ==
              template_decl);

  clang::Decl *selected_template_decl = inst_clang_template_decl;
  if (inst_record_spec_decl != nullptr) {
    auto selected_template =
        inst_record_spec_decl->getSpecializedTemplateOrPartial();
    if (auto *partial =
            selected_template
                .dyn_cast<clang::ClassTemplatePartialSpecializationDecl *>()) {
      selected_template_decl = partial;
    } else {
      selected_template_decl =
          selected_template.dyn_cast<clang::ClassTemplateDecl *>();
    }
  }
  SgTemplateClassDeclaration *exact_specialized_template_decl =
      isSgTemplateClassDeclaration(lookupSgDeclarationForClangDecl(
          selected_template_decl, /*allow_on_demand=*/true));
  if (exact_specialized_template_decl == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[template-instantiation-specialized-"
                 "identity]: Clang specialization has no exact translated "
                 "primary or partial template declaration"
              << std::endl;
    ROSE_ABORT();
  }
  exact_specialized_template_decl = isSgTemplateClassDeclaration(
      exact_specialized_template_decl->get_firstNondefiningDeclaration());
  ROSE_ASSERT(exact_specialized_template_decl != nullptr);
  ROSE_ASSERT(
      exact_specialized_template_decl->get_firstNondefiningDeclaration() ==
      exact_specialized_template_decl);

  auto has_source_surface = [](SgLocatedNode *node) {
    if (node == nullptr) {
      return false;
    }
    Sg_File_Info *fi = node->get_file_info();
    return fi != nullptr && fi->get_line() > 0 && !fi->isCompilerGenerated() &&
           !fi->isSourcePositionUnavailableInFrontend();
  };

  auto require_instantiation_scope = [&](SgTemplateInstantiationDecl *decl,
                                         SgScopeStatement *target_scope,
                                         const char *context) {
    ASSERT_not_null(decl);
    ASSERT_not_null(target_scope);
    auto *first =
        isSgTemplateInstantiationDecl(decl->get_firstNondefiningDeclaration());
    ASSERT_not_null(first);
    ROSE_ASSERT(first->get_firstNondefiningDeclaration() == first);
    ROSE_ASSERT(first->get_class_type() == inst_class_kind);
    for (auto *candidate :
         {decl, first,
          isSgTemplateInstantiationDecl(decl->get_definingDeclaration())}) {
      if (candidate == nullptr) {
        continue;
      }
      ROSE_ASSERT(candidate->get_scope() == target_scope);
      if (auto *lexical_owner = isSgScopeStatement(candidate->get_parent())) {
        requireExactSourceProvenance(candidate, context);
        ROSE_ASSERT(countLexicalDeclarationEdges(lexical_owner, candidate) ==
                    1);
      } else {
        requireTypedNonLexicalDeclarationOwner(candidate, context);
      }
    }
    SgSymbol *symbol = target_scope->find_symbol_from_declaration(first);
    ASSERT_not_null(symbol);
    ROSE_ASSERT(symbol->get_symbol_basis() == first);
    ROSE_ASSERT(symbol->get_parent() == target_scope->get_symbol_table());
  };

  // The display name is separate from the typed cache identity.
  std::string template_base_name = template_decl->get_name().getString();
  const clang::ClassTemplateDecl *cache_primary_template =
      inst_record_spec_decl != nullptr
          ? inst_record_spec_decl->getSpecializedTemplate()
          : llvm::dyn_cast_or_null<clang::ClassTemplateDecl>(
                inst_clang_template_decl);
  const clang::TemplateArgumentList *semantic_specialization_args =
      inst_record_spec_decl != nullptr
          ? &inst_record_spec_decl->getTemplateArgs()
          : nullptr;
  const clang::TemplateArgumentList *semantic_deduced_args =
      inst_record_spec_decl != nullptr
          ? &inst_record_spec_decl->getTemplateInstantiationArgs()
          : nullptr;

  // A TemplateSpecializationType retains only the arguments written at its use
  // site, while the canonical ClassTemplateSpecializationDecl owns the full
  // semantic list, including defaults.  Declaration identity and class-type
  // interning must use that full list; otherwise `vector<int>` and
  // `vector<int, allocator<int>>` create competing canonical declarations for
  // one Clang specialization.
  ClangTemplateInstantiationCacheKey instantiation_cache_key =
      semantic_specialization_args != nullptr
          ? buildTemplateInstantiationCacheKey(cache_primary_template,
                                               *semantic_specialization_args,
                                               inst_decl_context)
          : buildTemplateInstantiationCacheKey(cache_primary_template,
                                               clang_type, inst_decl_context);
  std::string inst_display_name =
      buildClangTemplateInstantiationNameForFrontend(
          template_base_name,
          semantic_specialization_args != nullptr
              ? semantic_specialization_args->asArray()
              : clang_type->template_arguments(),
          p_compiler_instance->getLangOpts());

  SgScopeStatement *inst_scope =
      canonical_namespace_scope(resolveScopeFromDeclContext(inst_decl_context));
  if (inst_scope == nullptr) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[template-instantiation-scope]: "
            "template=%s specialization=%s has no exact semantic scope\n",
            template_base_name.c_str(), inst_display_name.c_str());
    ROSE_ABORT();
  }

  // A declaration-first traversal or a re-entrant record translation can
  // publish the canonical specialization before any type spelling reaches
  // this cache.  Clang's exact record identity is primary; the
  // typed cache is only an index. The ordinary declaration map is
  // intentionally exact-declaration keyed, while the record-type registry
  // also covers the canonical/definition/redeclaration chain and construction
  // placeholders.
  auto resolve_record_identity_instantiation =
      [&]() -> SgTemplateInstantiationDecl * {
    if (inst_record_spec_decl == nullptr) {
      return nullptr;
    }

    SgDeclarationStatement *mapped_declaration =
        lookupSgDeclarationForClangDecl(
            const_cast<clang::ClassTemplateSpecializationDecl *>(
                inst_record_spec_decl),
            /*allow_on_demand=*/false);
    if (mapped_declaration == nullptr) {
      mapped_declaration = lookupRecordTypePlaceholderDecl(
          const_cast<clang::ClassTemplateSpecializationDecl *>(
              inst_record_spec_decl));
    }
    if (mapped_declaration == nullptr) {
      return nullptr;
    }

    SgTemplateInstantiationDecl *mapped =
        isSgTemplateInstantiationDecl(translatedRecordTypeDeclaration(
            inst_record_spec_decl, mapped_declaration));
    ASSERT_not_null(mapped);
    SgScopeStatement *mapped_scope =
        canonical_namespace_scope(mapped->get_scope());
    if (mapped->get_name() != SgName(inst_display_name) ||
        mapped_scope != inst_scope || mapped->get_type() == nullptr ||
        mapped->get_type()->get_declaration() != mapped ||
        !same_template_decl_chain(mapped->get_templateDeclaration(),
                                  template_decl)) {
      fprintf(
          stderr,
          "REX_FRONTEND_INVARIANT[template-instantiation-identity]: "
          "Clang specialization=%p exact mapping=%p name=%s scope=%p "
          "does not own the requested canonical template=%p name=%s "
          "scope=%p type=%p\n",
          static_cast<const void *>(inst_record_spec_decl),
          static_cast<void *>(mapped), mapped->get_name().getString().c_str(),
          static_cast<void *>(mapped_scope), static_cast<void *>(template_decl),
          inst_display_name.c_str(), static_cast<void *>(inst_scope),
          static_cast<void *>(mapped->get_type()));
      ROSE_ABORT();
    }
    return mapped;
  };
  SgTemplateInstantiationDecl *record_identity_instantiation =
      resolve_record_identity_instantiation();

  auto it = p_template_inst_cache.find(instantiation_cache_key);
  if (it != p_template_inst_cache.end()) {
    auto *inst_decl = isSgTemplateInstantiationDecl(it->second);
    ASSERT_not_null(inst_decl);
    ROSE_ASSERT(inst_decl->get_firstNondefiningDeclaration() == inst_decl);
    if (record_identity_instantiation != nullptr &&
        inst_decl != record_identity_instantiation) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[template-instantiation-identity]: "
              "Clang specialization=%p record identity=%p and cache "
              "identity=%p disagree for name=%s scope=%p\n",
              static_cast<const void *>(inst_record_spec_decl),
              static_cast<void *>(record_identity_instantiation),
              static_cast<void *>(inst_decl), inst_display_name.c_str(),
              static_cast<void *>(inst_scope));
      ROSE_ABORT();
    }
    if (semantic_specialization_args != nullptr) {
      ROSE_ASSERT(
          inst_decl->get_semanticTemplateArguments().size() ==
          countExpandedTemplateArguments(*semantic_specialization_args));
    }
    if (semantic_deduced_args != nullptr) {
      ROSE_ASSERT(inst_decl->get_deducedTemplateArguments().size() ==
                  countExpandedTemplateArguments(*semantic_deduced_args));
    }
    SgDeclarationStatement *cached_specialized =
        inst_decl->get_specializedTemplateDeclaration();
    if (cached_specialized != exact_specialized_template_decl) {
      std::cerr << "REX_FRONTEND_INVARIANT[template-instantiation-"
                   "specialized-identity]: Clang specialization="
                << inst_record_spec_decl << " name=" << inst_display_name
                << " cached-instantiation=" << inst_decl
                << " cached-specialized=" << cached_specialized << "/"
                << (cached_specialized != nullptr
                        ? cached_specialized->class_name()
                        : std::string("<null>"))
                << " exact-specialized=" << exact_specialized_template_decl
                << "/" << exact_specialized_template_decl->class_name()
                << " did not retain Clang's exact selected template "
                   "declaration"
                << std::endl;
      ROSE_ABORT();
    }
    for (const auto *arguments : {&inst_decl->get_templateArguments(),
                                  &inst_decl->get_semanticTemplateArguments(),
                                  &inst_decl->get_deducedTemplateArguments()}) {
      for (SgTemplateArgument *argument : *arguments) {
        ASSERT_not_null(argument);
        ROSE_ASSERT(argument->get_parent() == inst_decl);
      }
    }
    if (inst_decl->get_scope() == nullptr) {
      std::cerr << "REX_FRONTEND_INVARIANT[template-instantiation-owner]: "
                   "cached type instantiation has no exact scope"
                << std::endl;
      ROSE_ABORT();
    }
    if (has_source_surface(inst_decl)) {
      requireExactSourceProvenance(
          inst_decl, "getOrCreateTemplateInstantiation:cached-provenance");
    } else {
      requireExactSynthesizedProvenance(
          inst_decl, "getOrCreateTemplateInstantiation:cached-provenance");
    }
    require_instantiation_scope(
        inst_decl, inst_scope, "getOrCreateTemplateInstantiation:cached-owner");
    return inst_decl;
  }

  if (record_identity_instantiation != nullptr) {
    require_instantiation_scope(
        record_identity_instantiation, inst_scope,
        "getOrCreateTemplateInstantiation:record-cache-owner");
    p_template_inst_cache.emplace(instantiation_cache_key,
                                  record_identity_instantiation);
    return record_identity_instantiation;
  }

  auto build_semantic_argument_surface =
      [&](const clang::TemplateArgumentList *argument_list,
          const char *context) {
        SemanticExpressionConstruction semantic_expressions(
            p_semantic_template_argument_expression_depth, context);
        if (argument_list != nullptr) {
          return buildTemplateArguments(*argument_list, 0);
        }
        SgTemplateArgumentPtrList result;
        for (const clang::TemplateArgument &argument :
             clang_type->template_arguments()) {
          appendTemplateArguments(result, argument, false);
        }
        return result;
      };
  SgTemplateArgumentPtrList semantic_args = build_semantic_argument_surface(
      semantic_specialization_args,
      "getOrCreateTemplateInstantiation:semantic-identity-arguments");
  SgTemplateArgumentPtrList deduced_args = build_semantic_argument_surface(
      semantic_deduced_args,
      "getOrCreateTemplateInstantiation:deduced-arguments");

  // Translating a template argument can complete the same canonical
  // specialization re-entrantly (for example through a dependent typedef in
  // a nested-name specifier).  The identity check above necessarily precedes
  // argument translation, so repeat the commit check before allocating a
  // declaration or class type.  This is an optimistic construction
  // transaction: exactly one completed specialization wins.
  if (p_template_inst_cache.find(instantiation_cache_key) !=
      p_template_inst_cache.end()) {
    std::unordered_set<SgNode *> transient_nodes;
    auto collect_transient_nodes = [&](SgTemplateArgument *argument) {
      if (argument == nullptr || argument->get_parent() != nullptr) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[template-instantiation-transaction]: "
                "re-entrant argument=%p must be a detached exact subtree\n",
                static_cast<void *>(argument));
        ROSE_ABORT();
      }
      transient_nodes.insert(argument);
    };
    for (SgTemplateArgument *argument : semantic_args) {
      collect_transient_nodes(argument);
    }
    for (SgTemplateArgument *argument : deduced_args) {
      collect_transient_nodes(argument);
    }

    deleteTransientAST(transient_nodes);

    // Re-enter the ordinary cache path so all canonical declaration, scope,
    // type, and template-family validations remain centralized there.
    return getOrCreateTemplateInstantiation(template_decl, clang_type);
  }

  SgTemplateInstantiationDecl *inst_decl = new SgTemplateInstantiationDecl(
      SgName(inst_display_name), inst_class_kind, nullptr, nullptr,
      template_decl, SgTemplateArgumentPtrList(), semantic_args, false,
      deduced_args);
  inst_decl->set_specializedTemplateDeclaration(
      exact_specialized_template_decl);

  setSynthesizedFileInfo(inst_decl);
  requireExactSynthesizedProvenance(
      inst_decl, "getOrCreateTemplateInstantiation:new-provenance");
  inst_decl->setForward();
  inst_decl->set_definingDeclaration(nullptr);
  inst_decl->set_firstNondefiningDeclaration(inst_decl);

  if (inst_decl->get_templateDeclaration() != template_decl) {
    std::cerr << "REX_FRONTEND_INVARIANT[template-instantiation-constructor-"
                 "identity]: "
                 "SgTemplateInstantiationDecl constructor did not preserve its "
                 "exact primary-template declaration"
              << std::endl;
    ROSE_ABORT();
  }

  inst_decl->set_scope(inst_scope);

  inst_decl->set_templateName(SgName(template_base_name));

  SageBuilder::attachAuxiliaryDeclaration(inst_scope, inst_decl);
  SgClassType::createType(inst_decl);

  registerDeclarationSymbol(inst_decl);
  require_instantiation_scope(
      inst_decl, inst_scope,
      "getOrCreateTemplateInstantiation:new-cache-owner");
  p_template_inst_cache[instantiation_cache_key] = inst_decl;

  return inst_decl;
}

SgType *ClangToSageTranslator::buildDependentAliasUnderlyingType(
    const clang::TemplateSpecializationType *type) {
  if (type == nullptr || !type->isTypeAlias() ||
      !type->isInstantiationDependentType())
    return nullptr;
  const clang::QualType aliased = type->getAliasedType();
  ROSE_ASSERT(!aliased.isNull() && aliased.getTypePtr() != type);
  llvm::SaveAndRestore<unsigned> semantic_use(
      p_explicit_template_id_type_use_depth, 0);
  SemanticExpressionConstruction semantic_arguments(
      p_semantic_template_argument_expression_depth,
      "dependent-alias-underlying-type");
  return requireExactType(buildTypeFromQualifiedType(aliased),
                          "dependent-alias-underlying-type",
                          aliased.getTypePtr());
}

bool ClangToSageTranslator::VisitTemplateSpecializationType(
    clang::TemplateSpecializationType *template_specialization_type,
    SgNode **node) {
  ASSERT_not_null(template_specialization_type);
  ASSERT_not_null(node);
  if (p_explicit_template_id_type_use_depth != 0) {
    SgScopeStatement *scope = SageBuilder::topScopeStack();
    ASSERT_not_null(scope);
    SgNonrealType *type_use = buildNonrealTypeForNestedNameSpecifierType(
        template_specialization_type, scope,
        /*prefer_current_scope=*/false);
    if (type_use == nullptr) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[explicit-template-id-type-use]: "
              "template specialization=%p has no exact declaration-local "
              "type use\n",
              static_cast<void *>(template_specialization_type));
      ROSE_ABORT();
    }
    *node = type_use;
    return VisitType(template_specialization_type, node);
  }

  // Don't desugar or use canonical type for template specializations
  // We want to create proper SgTemplateInstantiationDecl nodes with template
  // arguments Desugaring would lose the template argument information

  if (template_specialization_type->isTypeAlias() &&
      !template_specialization_type->isInstantiationDependentType()) {
    clang::TemplateName tname = template_specialization_type->getTemplateName();
    const std::string alias_name =
        getTemplateNameBase(tname, p_compiler_instance->getASTContext());
    clang::TemplateDecl *clang_template_decl =
        resolveTemplateNameDeclaration(tname);
    clang::TypeAliasTemplateDecl *alias_decl =
        llvm::dyn_cast_or_null<clang::TypeAliasTemplateDecl>(
            clang_template_decl);

    // Clang builtin alias templates such as `__type_pack_element` resolve to
    // BuiltinTemplateDecl rather than TypeAliasTemplateDecl. Materialize the
    // declaration on demand so the surrounding AST can observe the template
    // declaration itself, not just the aliased type.
    if (alias_decl == nullptr && clang_template_decl != nullptr &&
        p_decl_translation_in_progress.find(clang_template_decl) ==
            p_decl_translation_in_progress.end() &&
        p_decl_translation_on_demand.find(clang_template_decl) ==
            p_decl_translation_on_demand.end()) {
      (void)lookupSgDeclarationForClangDecl(clang_template_decl,
                                            /*allow_on_demand=*/true);
    }
    SgTemplateTypedefDeclaration *alias_sg_decl = nullptr;
    if (alias_decl != nullptr) {
      if (SgDeclarationStatement *found_decl =
              lookupSgDeclarationForClangDecl(alias_decl,
                                              /*allow_on_demand=*/true)) {
        alias_sg_decl = isSgTemplateTypedefDeclaration(found_decl);
      }
      if (alias_sg_decl == nullptr)
        failExactTypeTranslation("alias-template-declaration",
                                 template_specialization_type);
    } else if (!llvm::isa_and_nonnull<clang::BuiltinTemplateDecl>(
                   clang_template_decl)) {
      failExactTypeTranslation("alias-template-declaration",
                               template_specialization_type);
    }
    if (alias_decl != nullptr) {
      clang::TemplateParameterList *params =
          alias_decl->getTemplateParameters();
      if (params == nullptr) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[alias-template-arguments]: alias=%p "
                "has no template parameter list\n",
                static_cast<void *>(alias_decl));
        ROSE_ABORT();
      }
      size_t argument_count = 0;
      for (const clang::TemplateArgument &argument :
           template_specialization_type->template_arguments()) {
        argument_count += countExpandedTemplateArgument(argument);
      }
      if (!params->hasParameterPack() && argument_count > params->size()) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[alias-template-arguments]: alias=%p "
                "has %zu expanded arguments for %u non-pack parameters\n",
                static_cast<void *>(alias_decl), argument_count,
                params->size());
        ROSE_ABORT();
      }
    }
    clang::QualType exact_aliased_qt =
        template_specialization_type->getAliasedType();
    if (exact_aliased_qt.isNull())
      failExactTypeTranslation("alias-underlying-type",
                               template_specialization_type);
    SgType *aliased_type = buildTypeFromQualifiedType(
        alias_decl != nullptr &&
                !requiresSourceOwnedDependentTypeIdentity(exact_aliased_qt)
            ? p_compiler_instance->getASTContext().getCanonicalType(
                  exact_aliased_qt)
            : exact_aliased_qt);

    clang::NestedNameSpecifier alias_qualifier = std::nullopt;
    if (const clang::QualifiedTemplateName *qtn =
            tname.getAsQualifiedTemplateName()) {
      alias_qualifier = qtn->getQualifier();
    } else if (const clang::DependentTemplateName *dtn =
                   tname.getAsDependentTemplateName()) {
      alias_qualifier = dtn->getQualifier();
    }

    auto materialize_qualifier_type_segments = [&](clang::NestedNameSpecifier
                                                       qualifier) {
      auto queue_implicit_class_specialization =
          [&](const clang::CXXRecordDecl *record_decl) {
            auto *spec_decl =
                llvm::dyn_cast_or_null<clang::ClassTemplateSpecializationDecl>(
                    const_cast<clang::CXXRecordDecl *>(record_decl));
            queuePendingImplicitClassTemplateSpecialization(spec_decl);
          };
      std::vector<clang::NestedNameSpecifier> segments;
      for (clang::NestedNameSpecifier nns = qualifier; nns;
           nns = nestedNameSpecifierPrefix(nns)) {
        segments.push_back(nns);
      }

      for (auto it = segments.rbegin(); it != segments.rend(); ++it) {
        clang::NestedNameSpecifier nns = *it;
        if (nns.getKind() != clang::NestedNameSpecifier::Kind::Type) {
          continue;
        }

        const clang::Type *segment_type = nns.getAsType();
        if (segment_type == nullptr) {
          continue;
        }

        if (const clang::SubstTemplateTypeParmType *subst =
                llvm::dyn_cast<clang::SubstTemplateTypeParmType>(
                    segment_type)) {
          segment_type = subst->getReplacementType().getTypePtrOrNull();
        }
        if (segment_type == nullptr) {
          continue;
        }

        if (const clang::TemplateSpecializationType *spec =
                llvm::dyn_cast<clang::TemplateSpecializationType>(
                    segment_type)) {
          if (!spec->isDependentType()) {
            clang::QualType semantic_qt = spec->isTypeAlias()
                                              ? spec->getAliasedType()
                                              : clang::QualType(spec, 0);
            if (!semantic_qt.isNull()) {
              (void)buildTypeFromQualifiedType(semantic_qt);
            }
          }
          continue;
        }

        clang::QualType segment_qt = clang::QualType(segment_type, 0);
        if (segment_qt.isNull()) {
          continue;
        }

        if (const clang::CXXRecordDecl *record_decl =
                cxxRecordDeclFromQualTypeWithoutDefinitionLookup(segment_qt)) {
          if (llvm::isa<clang::ClassTemplateSpecializationDecl>(record_decl) ||
              llvm::isa<clang::ClassTemplatePartialSpecializationDecl>(
                  record_decl)) {
            queue_implicit_class_specialization(record_decl);
            (void)lookupSgDeclarationForClangDecl(
                const_cast<clang::CXXRecordDecl *>(record_decl),
                /*allow_on_demand=*/true);
          }
        }
      }
    };

    if (alias_qualifier && !template_specialization_type->isDependentType()) {
      materialize_qualifier_type_segments(alias_qualifier);
    }

    ASSERT_not_null(clang_template_decl);
    SgScopeStatement *scope =
        resolveScopeFromDeclContext(clang_template_decl->getDeclContext());
    SgScopeStatement *lexical_scope = resolveScopeFromDeclContext(
        clang_template_decl->getLexicalDeclContext());
    if (lexical_scope != scope) {
      std::cerr << "REX_FRONTEND_INVARIANT[template-alias-scope]: lexical "
                   "and semantic scopes differ"
                << std::endl;
      ROSE_ABORT();
    }

    if (alias_sg_decl != nullptr) {
      if (isSgAuxiliaryDeclarationList(alias_sg_decl->get_parent()) !=
          nullptr) {
        if (alias_sg_decl->get_scope() != scope) {
          std::cerr << "REX_FRONTEND_INVARIANT[template-alias-owner]: semantic "
                       "alias declaration scope does not match its exact Clang "
                       "declaration context"
                    << std::endl;
          ROSE_ABORT();
        }
        requireTypedNonLexicalDeclarationOwner(
            alias_sg_decl, "VisitTemplateSpecializationType:semantic-alias");
      } else {
        if (alias_sg_decl->get_parent() != scope ||
            alias_sg_decl->get_scope() != scope ||
            !scope->statementExistsInScope(alias_sg_decl)) {
          std::cerr << "REX_FRONTEND_INVARIANT[template-alias-owner]: source "
                       "alias declaration has no exact lexical owner"
                    << std::endl;
          ROSE_ABORT();
        }
        (void)requireClangOrderedDeclarationProvenanceForFrontend(
            alias_sg_decl, "VisitTemplateSpecializationType:source-alias");
      }
    }

    if (alias_sg_decl != nullptr) {
      SgName alias_sage_name(alias_name);
      ROSE_ASSERT(alias_sg_decl->get_name() == alias_sage_name);
      const auto cache_key = buildTemplateInstantiationCacheKey(
          alias_decl, template_specialization_type,
          alias_decl->getDeclContext(), aliased_type);
      const auto cached = p_template_inst_cache.find(cache_key);
      if (cached != p_template_inst_cache.end()) {
        SgTemplateInstantiationTypedefDeclaration *existing_instantiation =
            isSgTemplateInstantiationTypedefDeclaration(cached->second);
        if (existing_instantiation == nullptr ||
            existing_instantiation->get_type() == nullptr ||
            existing_instantiation->get_base_type() != aliased_type ||
            existing_instantiation->get_specializedTemplateDeclaration() !=
                alias_sg_decl) {
          std::cerr << "REX_FRONTEND_INVARIANT[template-alias-instantiation]: "
                       "cached alias instantiation has an inconsistent type or "
                       "specialized declaration"
                    << std::endl;
          ROSE_ABORT();
        }
        requireExactSynthesizedProvenance(
            existing_instantiation,
            "VisitTemplateSpecializationType:cached-alias-provenance");
        requireTypedNonLexicalDeclarationOwner(
            existing_instantiation,
            "VisitTemplateSpecializationType:cached-alias-owner");
        *node = existing_instantiation->get_type();
        return VisitType(template_specialization_type, node);
      }
      SgName alias_name_with_args(
          buildClangTemplateInstantiationNameForFrontend(
              alias_name, template_specialization_type->template_arguments(),
              p_compiler_instance->getLangOpts()));
      SgTemplateArgumentPtrList template_args, deduced_args;
      {
        SemanticExpressionConstruction semantic_arguments(
            p_semantic_template_argument_expression_depth,
            "alias-instantiation-arguments");
        template_args =
            buildTemplateArguments(template_specialization_type, false);
        deduced_args =
            buildTemplateArguments(template_specialization_type, false);
      }
      if (p_template_inst_cache.count(cache_key) != 0) {
        std::unordered_set<SgNode *> transient_arguments;
        for (auto *argument : template_args) {
          ROSE_ASSERT(argument != nullptr && argument->get_parent() == nullptr);
          transient_arguments.insert(argument);
        }
        for (auto *argument : deduced_args) {
          ROSE_ASSERT(argument != nullptr && argument->get_parent() == nullptr);
          transient_arguments.insert(argument);
        }
        deleteTransientAST(transient_arguments);
        return VisitTemplateSpecializationType(template_specialization_type,
                                               node);
      }
      SgTemplateInstantiationTypedefDeclaration *inst_decl =
          SageBuilder::buildTemplateInstantiationTypedefDeclaration_nfi(
              SageBuilder::typedef_declaration_ownership::semanticAuxiliary(),
              SgTypedefDeclaration::e_using, alias_sage_name, aliased_type,
              scope, alias_sg_decl, template_args, alias_name_with_args);
      ROSE_ASSERT(inst_decl != nullptr);
      ROSE_ASSERT(inst_decl->get_specializedTemplateDeclaration() ==
                  alias_sg_decl);
      validateTypedefDeclarationReferenceShared(inst_decl);
      inst_decl->get_deducedTemplateArguments() = deduced_args;
      setSynthesizedFileInfo(inst_decl);
      SageBuilder::setTemplateArgumentParents(inst_decl);
      registerDeclarationSymbol(inst_decl);
      requireTypedNonLexicalDeclarationOwner(
          inst_decl,
          "VisitTemplateSpecializationType:alias-instantiation-owner");
      ROSE_ASSERT(p_template_inst_cache.emplace(cache_key, inst_decl).second);
      *node = inst_decl->get_type();
      return VisitType(template_specialization_type, node);
    }

    *node = aliased_type;
    return VisitType(template_specialization_type, node);
  }

  if (template_specialization_type->isDependentType() ||
      (template_specialization_type->isTypeAlias() &&
       template_specialization_type->isInstantiationDependentType())) {
    clang::TemplateName tname = template_specialization_type->getTemplateName();
    clang::TemplateDecl *semantic_template_decl =
        resolveTemplateNameDeclaration(tname);
    const bool semantic_template_is_parameter =
        llvm::isa_and_nonnull<clang::TemplateTemplateParmDecl>(
            semantic_template_decl);
    SgDeclarationStatement *template_identity = nullptr;
    std::string base_name;
    if (semantic_template_is_parameter) {
      auto *identity =
          isSgTemplateDeclaration(resolveNonrealTemplateDeclarationTarget(
              semantic_template_decl, "dependent-template-specialization"));
      ASSERT_not_null(identity);
      template_identity = identity;
      base_name = identity->get_name().getString();
    } else {
      base_name =
          getTemplateNameBase(tname, p_compiler_instance->getASTContext());
    }
    ROSE_ASSERT(!base_name.empty());

    // Construct dependent template arguments with semantic ownership.
    SgTemplateArgumentPtrList tpl_args;
    {
      SemanticExpressionConstruction semantic_arguments(
          p_semantic_template_argument_expression_depth,
          "dependent-template-specialization-arguments");
      tpl_args = buildTemplateArguments(template_specialization_type, true);
    }

    SgScopeStatement *base_scope = SageBuilder::topScopeStack();
    ROSE_ASSERT(base_scope != nullptr);

    if (semantic_template_decl != nullptr && !semantic_template_is_parameter)
      template_identity = resolveNonrealTemplateDeclarationTarget(
          semantic_template_decl, "dependent-template-specialization");
    if (template_specialization_type->isCurrentInstantiation()) {
      ASSERT_not_null(semantic_template_decl);
      ASSERT_not_null(isSgTemplateClassDeclaration(template_identity));
    }
    SgType *aliased_type =
        buildDependentAliasUnderlyingType(template_specialization_type);
    clang::NestedNameSpecifier qualifier = std::nullopt;
    if (const clang::QualifiedTemplateName *qtn =
            tname.getAsQualifiedTemplateName()) {
      qualifier = qtn->getQualifier();
    } else if (const clang::DependentTemplateName *dtn =
                   tname.getAsDependentTemplateName()) {
      qualifier = dtn->getQualifier();
    }

    if (qualifier) {
      *node = buildSemanticNonrealTypeFromNestedNameSpecifier(
          qualifier, base_scope, SgName(base_name), &tpl_args,
          template_identity, aliased_type);
      return VisitType(template_specialization_type, node);
    }

    // For dependent names without an explicit qualifier, preserve the spelling
    // as-written. Synthesizing declaration-context qualifiers here loses
    // dependent template arguments (e.g., `Outer<T>::Inner<U>` becoming
    // `Outer::Inner<U>`), which breaks correctness.
    *node = SageBuilder::buildSemanticNonrealType(SgName(base_name), base_scope,
                                                  &tpl_args, template_identity,
                                                  aliased_type);
    return VisitType(template_specialization_type, node);
  }

  if (clang::QualType qt = clang::QualType(template_specialization_type, 0);
      !qt.isNull()) {
    if (const clang::CXXRecordDecl *record_decl =
            cxxRecordDeclFromQualTypeWithoutDefinitionLookup(qt)) {
      if (llvm::isa<clang::ClassTemplateSpecializationDecl>(record_decl) ||
          llvm::isa<clang::ClassTemplatePartialSpecializationDecl>(
              record_decl)) {
        auto *mutable_record = const_cast<clang::CXXRecordDecl *>(record_decl);
        SgDeclarationStatement *specialization_decl =
            lookupSgDeclarationForClangDecl(mutable_record,
                                            /*allow_on_demand=*/false);
        bool should_materialize_specialization_decl =
            llvm::isa<clang::ClassTemplatePartialSpecializationDecl>(
                mutable_record);
        if (!should_materialize_specialization_decl) {
          if (const clang::ClassTemplateSpecializationDecl *spec_decl =
                  llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(
                      mutable_record)) {
            switch (spec_decl->getTemplateSpecializationKind()) {
            case clang::TSK_ExplicitInstantiationDeclaration:
            case clang::TSK_ExplicitInstantiationDefinition:
            case clang::TSK_ExplicitSpecialization:
              should_materialize_specialization_decl = true;
              break;
            case clang::TSK_ImplicitInstantiation:
            case clang::TSK_Undeclared:
              break;
            }
          }
        }

        if (specialization_decl == nullptr &&
            should_materialize_specialization_decl &&
            p_decl_translation_in_progress.find(mutable_record) ==
                p_decl_translation_in_progress.end() &&
            p_decl_translation_on_demand.find(mutable_record) ==
                p_decl_translation_on_demand.end()) {
          const bool defer_specialization_population =
              !llvm::isa<clang::ClassTemplatePartialSpecializationDecl>(
                  mutable_record);
          if (defer_specialization_population) {
            ++p_defer_on_demand_cxx_record_population_depth;
          }
          struct SpecializationTypePopulationGuard {
            unsigned &depth;
            bool active;
            ~SpecializationTypePopulationGuard() {
              if (active) {
                ROSE_ASSERT(depth > 0);
                --depth;
              }
            }
          } population_guard{p_defer_on_demand_cxx_record_population_depth,
                             defer_specialization_population};
          specialization_decl = lookupSgDeclarationForClangDecl(
              mutable_record, /*allow_on_demand=*/true);
        }
        if (specialization_decl != nullptr) {
          auto *inst_decl =
              isSgTemplateInstantiationDecl(translatedRecordTypeDeclaration(
                  mutable_record, specialization_decl));
          ROSE_ASSERT(inst_decl != nullptr && inst_decl->get_type() != nullptr);
          *node = inst_decl->get_type();
          return VisitType(template_specialization_type, node);
        }
      }
    }
  }

  SgTemplateClassDeclaration *template_decl =
      requireExactTemplateDeclaration(template_specialization_type);

  SgTemplateInstantiationDecl *inst_decl = getOrCreateTemplateInstantiation(
      template_decl, template_specialization_type);
  ASSERT_not_null(inst_decl);

  // Return the class type
  *node = inst_decl->get_type();
  ROSE_ASSERT(*node != nullptr);

  return VisitType(template_specialization_type, node);
}

bool ClangToSageTranslator::VisitTemplateTypeParmType(
    clang::TemplateTypeParmType *template_type_parm_type, SgNode **node) {
  const clang::TemplateTypeParmDecl *param_decl =
      template_type_parm_type->getDecl();
  if (p_canonical_template_argument_context.has_value()) {
    const auto *bound = llvm::dyn_cast_or_null<clang::TemplateTypeParmDecl>(
        resolveTemplateParameterFromDeclContext(
            *p_canonical_template_argument_context,
            template_type_parm_type->getDepth(),
            template_type_parm_type->getIndex(),
            currentExactTemplateParameters()));
    ASSERT_not_null(bound);
    ROSE_ASSERT(param_decl == nullptr ||
                bound->isParameterPack() == param_decl->isParameterPack());
    param_decl = bound;
  }

  const clang::DeclContext *active_context =
      p_template_parameter_decl_context_stack.empty()
          ? nullptr
          : p_template_parameter_decl_context_stack.back();
  auto resolve_active_parameter = [&]() -> clang::TemplateTypeParmDecl * {
    const unsigned depth = template_type_parm_type->getDepth();
    const unsigned index = template_type_parm_type->getIndex();
    clang::TemplateTypeParmDecl *result = nullptr;
    for (const clang::TemplateParameterList *level :
         collectTemplateParameterLevelsFromDeclContext(active_context)) {

      if (level == nullptr || index >= level->size()) {
        continue;
      }
      auto *candidate = const_cast<clang::TemplateTypeParmDecl *>(
          llvm::dyn_cast_or_null<clang::TemplateTypeParmDecl>(
              level->getParam(index)));
      if (candidate == nullptr || candidate->getDepth() != depth ||
          candidate->getIndex() != index) {
        continue;
      }

      // An active declaration context is only an owner oracle when it
      // identifies the same Clang parameter family as the type. Nested default
      // arguments can be translated while an unrelated parameter list with
      // the same numerical depth/index is active; treating that coordinate
      // collision as identity rewrites one declaration's type to another
      // declaration's name.
      if (param_decl != nullptr &&
          candidate->getCanonicalDecl() != param_decl->getCanonicalDecl()) {
        continue;
      }
      if (result != nullptr &&
          result->getCanonicalDecl() != candidate->getCanonicalDecl()) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[template-type-parameter-context]: "
                "depth=%u index=%u resolves to two distinct active "
                "parameter families=%p/%p\n",
                depth, index, static_cast<void *>(result),
                static_cast<void *>(candidate));
        ROSE_ABORT();
      }
      result = candidate;
    }
    if (result == nullptr) {
      const clang::NamedDecl *context_parameter =
          resolveTemplateParameterFromDeclContext(active_context, depth, index);
      auto *candidate = const_cast<clang::TemplateTypeParmDecl *>(
          llvm::dyn_cast_or_null<clang::TemplateTypeParmDecl>(
              context_parameter));
      if (candidate != nullptr &&
          (param_decl == nullptr ||
           candidate->getCanonicalDecl() == param_decl->getCanonicalDecl())) {
        // A class-template specialization has no TemplateParameterList of its
        // own.  The exact active DeclContext nevertheless carries Clang's
        // instantiated-from primary/partial-specialization backlink; the
        // shared context resolver follows that backlink to the one canonical
        // parameter declaration.
        result = candidate;
      }
    }
    if (result == nullptr &&
        p_template_parameter_decl_context_stack.size() > 1) {
      for (auto context =
               std::next(p_template_parameter_decl_context_stack.rbegin());
           context != p_template_parameter_decl_context_stack.rend();
           ++context) {
        const clang::NamedDecl *context_parameter =
            resolveTemplateParameterFromDeclContext(*context, depth, index);
        auto *candidate = const_cast<clang::TemplateTypeParmDecl *>(
            llvm::dyn_cast_or_null<clang::TemplateTypeParmDecl>(
                context_parameter));
        if (candidate == nullptr ||
            (param_decl != nullptr &&
             candidate->getCanonicalDecl() != param_decl->getCanonicalDecl())) {
          continue;
        }
        result = candidate;
        // The stack is a nested producer transaction, not a bag of
        // depth/index namespaces. Once the newest enclosing context owns the
        // requested coordinate, older source redeclarations with identical
        // coordinates must not compete with that exact active owner.
        break;
      }
    }
    return result;
  };

  SgTemplateParameter *mapped_param = nullptr;
  clang::TemplateTypeParmDecl *active_parameter = resolve_active_parameter();
  if (active_parameter == nullptr && param_decl == nullptr) {
    active_parameter = resolveActiveTemplateTypeParameterSurface(
        template_type_parm_type->getDepth(),
        template_type_parm_type->getIndex(),
        "VisitTemplateTypeParmType:canonical-active-surface");
  }
  const clang::TemplateTypeParmDecl *parameter =
      active_parameter != nullptr ? active_parameter : param_decl;
  if (p_semantic_template_argument_expression_depth != 0 &&
      parameter != nullptr) {
    mapped_param = lookupActiveSemanticTemplateParameterSurface(
        parameter, "VisitTemplateTypeParmType:semantic-surface");
    if (mapped_param == nullptr)
      mapped_param = lookupPublishedTemplateTypeParameterFamily(
          parameter, "VisitTemplateTypeParmType:semantic-family",
          /*require_exact_owner=*/false);
  }
  if (mapped_param == nullptr && parameter != nullptr) {
    mapped_param = lookupActiveTemplateParameterSurface(
        parameter, "VisitTemplateTypeParmType:exact-parameter");
    if (mapped_param == nullptr) {
      mapped_param = lookupPublishedTemplateTypeParameterFamily(
          parameter, "VisitTemplateTypeParmType:published-family",
          /*require_exact_owner=*/true);
    }
  }

  const clang::TemplateTypeParmDecl *identity_param_decl = parameter;
  if (identity_param_decl == nullptr)
    identity_param_decl = resolveExactTemplateTypeParameterDeclaration(
        template_type_parm_type->getDepth(),
        template_type_parm_type->getIndex());
  // Resolve one typed declaration before choosing its spelling. Underscores,
  // coordinate-like identifiers, and unnamed parameters carry no ranking.
  const std::string param_name = identity_param_decl != nullptr
                                     ? identity_param_decl->getNameAsString()
                                     : std::string();
  // Canonical TemplateTypeParmType nodes can rebase their depth relative to
  // the consuming declaration and omit getDecl().  The SAGE template type is
  // the declared parameter identity, so its stored coordinate is owned by the
  // exact resolved TemplateTypeParmDecl, not by that canonical use-site view.
  const unsigned identity_depth = identity_param_decl != nullptr
                                      ? identity_param_decl->getDepth()
                                      : template_type_parm_type->getDepth();
  const unsigned identity_index = identity_param_decl != nullptr
                                      ? identity_param_decl->getIndex()
                                      : template_type_parm_type->getIndex();
  const bool is_exactly_unnamed_parameter =
      identity_param_decl != nullptr &&
      identity_param_decl->getIdentifier() == nullptr;
  const bool is_canonical_parameter_identity =
      param_decl == nullptr &&
      template_type_parm_type->isCanonicalUnqualified();

  if (param_name.empty() && !is_exactly_unnamed_parameter &&
      !is_canonical_parameter_identity) {
    const clang::Decl *active_context_decl =
        llvm::dyn_cast_or_null<clang::Decl>(active_context);
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[template-type-parameter]: depth=%u "
            "index=%u clang-decl=%p clang-name='%s' "
            "active-context=%p(%s) context-depth=%zu exact-name-depth=%zu "
            "has no exact declared or canonical identity\n",
            template_type_parm_type->getDepth(),
            template_type_parm_type->getIndex(),
            static_cast<const void *>(param_decl),
            param_decl != nullptr ? param_decl->getNameAsString().c_str() : "",
            static_cast<const void *>(active_context),
            active_context_decl != nullptr
                ? active_context_decl->getDeclKindName()
                : "<null>",
            p_template_parameter_decl_context_stack.size(),
            p_exact_template_parameter_stack.size());
    ROSE_ABORT();
  }

  if (mapped_param != nullptr) {
    SgTemplateType *mapped_type = isSgTemplateType(mapped_param->get_type());
    // SgTemplateType represents the declared parameter identity.  A
    // canonical TemplateTypeParmType can describe a pack-expansion use and
    // report a use-site pack bit that differs from the declaration.  Once the
    // exact declaration is known, its pack property is the only valid owner
    // property for the Sage parameter type.
    const bool is_pack = identity_param_decl != nullptr
                             ? identity_param_decl->isParameterPack()
                             : template_type_parm_type->isParameterPack();
    if (mapped_type == nullptr ||
        mapped_param->get_parameterType() !=
            SgTemplateParameter::type_parameter ||
        mapped_type->get_template_parameter() != mapped_param ||
        mapped_type->get_template_parameter_depth() !=
            static_cast<int>(identity_depth) ||
        mapped_type->get_template_parameter_position() !=
            static_cast<int>(identity_index) ||
        mapped_type->get_packed() != is_pack) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[template-type-parameter-identity]: "
              "parameter=%p parameter-kind=%d type=%p type-parameter=%p "
              "expected-depth=%u actual-depth=%d expected-index=%u "
              "actual-index=%d expected-pack=%d actual-pack=%d does not own "
              "its exact semantic type; clang-parameter=%p "
              "active-parameter=%p depth=%d index=%d "
              "use-depth=%u use-index=%u owner=%p/%s\n",
              static_cast<void *>(mapped_param),
              static_cast<int>(mapped_param->get_parameterType()),
              static_cast<void *>(mapped_type),
              static_cast<void *>(mapped_type != nullptr
                                      ? mapped_type->get_template_parameter()
                                      : nullptr),
              identity_depth,
              mapped_type != nullptr
                  ? mapped_type->get_template_parameter_depth()
                  : -1,
              identity_index,
              mapped_type != nullptr
                  ? mapped_type->get_template_parameter_position()
                  : -1,
              is_pack ? 1 : 0,
              mapped_type != nullptr && mapped_type->get_packed() ? 1 : 0,
              static_cast<const void *>(param_decl),
              static_cast<void *>(active_parameter),
              active_parameter != nullptr
                  ? static_cast<int>(active_parameter->getDepth())
                  : -1,
              active_parameter != nullptr
                  ? static_cast<int>(active_parameter->getIndex())
                  : -1,
              template_type_parm_type->getDepth(),
              template_type_parm_type->getIndex(),
              static_cast<void *>(mapped_param->get_parent()),
              mapped_param->get_parent() != nullptr
                  ? mapped_param->get_parent()->class_name().c_str()
                  : "<null>");
      const clang::Decl *active_context_decl =
          llvm::dyn_cast_or_null<clang::Decl>(active_context);
      const clang::DeclContext *active_parameter_context =
          active_parameter != nullptr ? active_parameter->getDeclContext()
                                      : nullptr;
      const clang::Decl *active_parameter_owner =
          llvm::dyn_cast_or_null<clang::Decl>(active_parameter_context);
      const clang::NamedDecl *active_parameter_named_owner =
          llvm::dyn_cast_or_null<clang::NamedDecl>(active_parameter_owner);
      fprintf(
          stderr,
          "REX_FRONTEND_INVARIANT[template-type-parameter-context-"
          "detail]: active-context=%p/%s active-parameter-context=%p/%s "
          "active-parameter-owner-name='%s'\n",
          static_cast<const void *>(active_context),
          active_context_decl != nullptr
              ? active_context_decl->getDeclKindName()
              : "<null>",
          static_cast<const void *>(active_parameter_context),
          active_parameter_owner != nullptr
              ? active_parameter_owner->getDeclKindName()
              : "<null>",
          active_parameter_named_owner != nullptr
              ? active_parameter_named_owner->getQualifiedNameAsString().c_str()
              : "<unnamed>");
      ROSE_ABORT();
    }
    *node = mapped_type->get_packed()
                ? buildUnexpandedTemplateParameterUse(mapped_type)
                : mapped_type;
    return VisitType(template_type_parm_type, node);
  }

  if (identity_param_decl == nullptr) {
    const clang::Decl *active_context_decl =
        llvm::dyn_cast_or_null<clang::Decl>(active_context);
    fprintf(
        stderr,
        "REX_FRONTEND_INVARIANT[template-type-parameter-identity]: "
        "depth=%u index=%u type-name='%s' canonical=%d "
        "active-context=%p/%s/%s context-depth=%zu exact-map-depth=%zu "
        "surface-depth=%zu has no exact canonical parameter declaration\n",
        template_type_parm_type->getDepth(),
        template_type_parm_type->getIndex(), param_name.c_str(),
        template_type_parm_type->isCanonicalUnqualified() ? 1 : 0,
        static_cast<const void *>(active_context),
        active_context_decl != nullptr ? active_context_decl->getDeclKindName()
                                       : "<null>",
        llvm::dyn_cast_or_null<clang::NamedDecl>(active_context_decl) != nullptr
            ? llvm::dyn_cast<clang::NamedDecl>(active_context_decl)
                  ->getQualifiedNameAsString()
                  .c_str()
            : "<unnamed>",
        p_template_parameter_decl_context_stack.size(),
        p_exact_template_parameter_stack.size(),
        p_template_parameter_surface_stack.size());
    ROSE_ABORT();
  }
  fprintf(stderr,
          "REX_FRONTEND_INVARIANT[template-type-parameter-publication]: "
          "parameter=%p name='%s' location=%s depth=%u index=%u "
          "has no published Sage owner\n",
          static_cast<const void *>(identity_param_decl), param_name.c_str(),
          identity_param_decl->getLocation()
              .printToString(p_compiler_instance->getSourceManager())
              .c_str(),
          identity_depth, identity_index);
  ROSE_ABORT();
}

bool ClangToSageTranslator::VisitTypedefType(clang::TypedefType *typedef_type,
                                             SgNode **node) {
  ROSE_ASSERT(typedef_type != nullptr);
  ROSE_ASSERT(node != nullptr);

  SgTypedefDeclaration *sg_typedef_decl = NULL;
  auto it = p_decl_translation_map.find(typedef_type->getDecl());
  if (it != p_decl_translation_map.end()) {
    sg_typedef_decl = isSgTypedefDeclaration(it->second);
  }

  if (sg_typedef_decl == NULL) {
    SgNode *on_demand_result = TraverseOnDemand(
        const_cast<clang::TypedefNameDecl *>(typedef_type->getDecl()));
    it = p_decl_translation_map.find(typedef_type->getDecl());
    if (it != p_decl_translation_map.end()) {
      sg_typedef_decl = isSgTypedefDeclaration(it->second);
    }
    if (sg_typedef_decl == nullptr) {
      clang::TypedefNameDecl *canonical =
          typedef_type->getDecl()->getCanonicalDecl();
      auto canonical_mapping = p_decl_translation_map.find(canonical);
      std::cerr
          << "REX_FRONTEND_INVARIANT[typedef-declaration-detail]: "
          << "typedef=" << typedef_type->getDecl() << " canonical=" << canonical
          << " qualified-name='"
          << typedef_type->getDecl()->getQualifiedNameAsString()
          << "' context-kind="
          << (typedef_type->getDecl()->getDeclContext() != nullptr
                  ? clang::Decl::castFromDeclContext(
                        typedef_type->getDecl()->getDeclContext())
                        ->getDeclKindName()
                  : "<null>")
          << " in-progress="
          << p_decl_translation_in_progress.count(
                 const_cast<clang::TypedefNameDecl *>(typedef_type->getDecl()))
          << " on-demand="
          << p_decl_translation_on_demand.count(
                 const_cast<clang::TypedefNameDecl *>(typedef_type->getDecl()))
          << " result=" << on_demand_result
          << (on_demand_result != nullptr ? "/" + on_demand_result->class_name()
                                          : std::string("/<null>"))
          << " canonical-result="
          << (canonical_mapping != p_decl_translation_map.end()
                  ? canonical_mapping->second
                  : nullptr)
          << std::endl;
    }
  }

  if (sg_typedef_decl == NULL) {
    std::cerr << "REX_FRONTEND_INVARIANT[typedef-declaration]: exact on-demand "
                 "translation did not publish typedef '"
              << typedef_type->getDecl()->getNameAsString() << "'" << std::endl;
    ROSE_ABORT();
  }

  validateTypedefDeclarationReferenceShared(sg_typedef_decl);
  SgTypedefType *sg_typedef_type = isSgTypedefType(sg_typedef_decl->get_type());
  if (sg_typedef_type == nullptr ||
      sg_typedef_type->get_declaration() != sg_typedef_decl) {
    SgTypedefDeclaration *type_declaration = isSgTypedefDeclaration(
        sg_typedef_type != nullptr ? sg_typedef_type->get_declaration()
                                   : nullptr);
    std::cerr << "REX_FRONTEND_INVARIANT[typedef-type-identity]: typedef '"
              << typedef_type->getDecl()->getQualifiedNameAsString()
              << "' declaration=" << sg_typedef_decl
              << " scope=" << sg_typedef_decl->get_scope()
              << " first=" << sg_typedef_decl->get_firstNondefiningDeclaration()
              << " type=" << sg_typedef_type
              << " type-declaration=" << type_declaration << " type-scope="
              << (type_declaration != nullptr ? type_declaration->get_scope()
                                              : nullptr)
              << " type-first="
              << (type_declaration != nullptr
                      ? type_declaration->get_firstNondefiningDeclaration()
                      : nullptr)
              << " does not own its exact SgTypedefType" << std::endl;
    ROSE_ABORT();
  }
  *node = sg_typedef_type;

  return VisitType(typedef_type, node);
}

bool ClangToSageTranslator::VisitTypeOfExprType(
    clang::TypeOfExprType *type_of_expr_type, SgNode **node) {
  SgExpression *expr =
      isSgExpression(Traverse(type_of_expr_type->getUnderlyingExpr()));
  if (expr == nullptr)
    failExactTypeTranslation("typeof-expression", type_of_expr_type);
  expr = prepareExpressionForAttachment(expr);
  requireExactType(expr->get_type(), "typeof-expression-type",
                   type_of_expr_type);
  *node = SageBuilder::buildTypeOfType(expr, nullptr);

  return VisitType(type_of_expr_type, node);
}

bool ClangToSageTranslator::VisitTypeOfType(clang::TypeOfType *type_of_type,
                                            SgNode **node) {
  SgType *underlying_type = requireExactType(
      buildTypeFromQualifiedType(type_of_type->getUnmodifiedType()),
      "typeof-type-operand", type_of_type);
  *node = SageBuilder::buildTypeOfType(nullptr, underlying_type);

  return VisitType(type_of_type, node);
}

bool ClangToSageTranslator::VisitDependentNameType(
    clang::DependentNameType *dependent_name_type, SgNode **node) {
  if (dependent_name_type == nullptr || node == nullptr) {
    failExactTypeTranslation("dependent-name-type", dependent_name_type);
  }
  const clang::IdentifierInfo *id = dependent_name_type->getIdentifier();
  SgScopeStatement *base_scope = SageBuilder::topScopeStack();
  if (id == nullptr || id->getName().empty() || base_scope == nullptr) {
    failExactTypeTranslation("dependent-name-identity", dependent_name_type);
  }
  SgNonrealType *nrtype = buildSemanticNonrealTypeFromNestedNameSpecifier(
      dependent_name_type->getQualifier(), base_scope,
      SgName(id->getName().str()), nullptr);
  SgNonrealDecl *nrdecl =
      nrtype != nullptr ? isSgNonrealDecl(nrtype->get_declaration()) : nullptr;
  if (nrdecl == nullptr) {
    failExactTypeTranslation("dependent-name-declaration", dependent_name_type);
  }
  nrdecl->set_suppress_typename(false);
  *node = nrtype;

  return VisitType(dependent_name_type, node);
}

bool ClangToSageTranslator::VisitUnaryTransformType(
    clang::UnaryTransformType *unary_transform_type, SgNode **node) {
  if (unary_transform_type->isDependentType()) {
    using Operation = SgUnaryTransformType;
    Operation::operation_enum operation = Operation::e_invalid;
    switch (unary_transform_type->getUTTKind()) {
    case clang::UnaryTransformType::AddLvalueReference:
      operation = Operation::e_add_lvalue_reference;
      break;
    case clang::UnaryTransformType::AddPointer:
      operation = Operation::e_add_pointer;
      break;
    case clang::UnaryTransformType::AddRvalueReference:
      operation = Operation::e_add_rvalue_reference;
      break;
    case clang::UnaryTransformType::Decay:
      operation = Operation::e_decay;
      break;
    case clang::UnaryTransformType::MakeSigned:
      operation = Operation::e_make_signed;
      break;
    case clang::UnaryTransformType::MakeUnsigned:
      operation = Operation::e_make_unsigned;
      break;
    case clang::UnaryTransformType::RemoveAllExtents:
      operation = Operation::e_remove_all_extents;
      break;
    case clang::UnaryTransformType::RemoveConst:
      operation = Operation::e_remove_const;
      break;
    case clang::UnaryTransformType::RemoveCV:
      operation = Operation::e_remove_cv;
      break;
    case clang::UnaryTransformType::RemoveCVRef:
      operation = Operation::e_remove_cvref;
      break;
    case clang::UnaryTransformType::RemoveExtent:
      operation = Operation::e_remove_extent;
      break;
    case clang::UnaryTransformType::RemovePointer:
      operation = Operation::e_remove_pointer;
      break;
    case clang::UnaryTransformType::RemoveReference:
      operation = Operation::e_remove_reference;
      break;
    case clang::UnaryTransformType::RemoveRestrict:
      operation = Operation::e_remove_restrict;
      break;
    case clang::UnaryTransformType::RemoveVolatile:
      operation = Operation::e_remove_volatile;
      break;
    case clang::UnaryTransformType::EnumUnderlyingType:
      operation = Operation::e_enum_underlying_type;
      break;
    default:
      failExactTypeTranslation("unary-transform-operation",
                               unary_transform_type);
    }
    SgType *operand = requireExactType(
        buildTypeFromQualifiedType(unary_transform_type->getBaseType()),
        "unary-transform-operand", unary_transform_type);
    *node = SgUnaryTransformType::createType(operand, operation);
    return VisitType(unary_transform_type, node);
  }
  clang::QualType underlying = unary_transform_type->getUnderlyingType();

  if (underlying.isNull()) {
    failExactTypeTranslation("unary-transform-underlying",
                             unary_transform_type);
  }
  *node = requireExactType(buildTypeFromQualifiedType(underlying),
                           "unary-transform-underlying", unary_transform_type);

  return VisitType(unary_transform_type, node);
}

bool ClangToSageTranslator::VisitUnresolvedUsingType(
    clang::UnresolvedUsingType *unresolved_using_type, SgNode **node) {
  if (unresolved_using_type == nullptr || node == nullptr) {
    failExactTypeTranslation("unresolved-using-type", unresolved_using_type);
  }

  clang::UnresolvedUsingTypenameDecl *declaration =
      unresolved_using_type->getDecl();
  if (declaration == nullptr) {
    failExactTypeTranslation("unresolved-using-declaration",
                             unresolved_using_type);
  }
  clang::DeclContext *declaration_context = declaration->getDeclContext();
  if (declaration_context == nullptr) {
    failExactTypeTranslation("unresolved-using-context", unresolved_using_type);
  }
  clang::NestedNameSpecifier qualifier = declaration->getQualifier();
  const std::string name = declaration->getNameAsString();
  if (!qualifier || name.empty()) {
    failExactTypeTranslation("unresolved-using-type", unresolved_using_type);
  }
  SgScopeStatement *semantic_scope =
      resolveScopeFromDeclContext(declaration_context);
  if (semantic_scope == nullptr) {
    failExactTypeTranslation("unresolved-using-scope", unresolved_using_type);
  }

  *node =
      requireExactType(buildSemanticNonrealTypeFromNestedNameSpecifier(
                           qualifier, semantic_scope, SgName(name), nullptr),
                       "unresolved-using-type", unresolved_using_type);
  return VisitType(unresolved_using_type, node);
}

bool ClangToSageTranslator::VisitVectorType(clang::VectorType *vector_type,
                                            SgNode **node) {
  SgType *type = requireExactType(
      buildTypeFromQualifiedType(vector_type->getElementType()),
      "vector-element-type", vector_type);

  SgTypeModifier sg_modifer;

  if (llvm::isa<clang::ExtVectorType>(vector_type))
    sg_modifer.setExtendedVectorType();
  else
    sg_modifer.setVectorType();
  sg_modifer.set_vector_size(vector_type->getNumElements());

  *node = SageBuilder::buildModifierType(type, sg_modifer);

  return VisitType(vector_type, node);
}

bool ClangToSageTranslator::VisitExtVectorType(
    clang::ExtVectorType *ext_vector_type, SgNode **node) {
  return VisitVectorType(ext_vector_type, node);
}

bool ClangToSageTranslator::VisitUsingType(clang::UsingType *using_type,
                                           SgNode **node) {
  clang::QualType underlying = using_type->desugar();
  *node =
      requireExactType(buildTypeFromQualifiedType(underlying),
                       "using-underlying-type", underlying.getTypePtrOrNull());

  return VisitType(using_type, node);
}
