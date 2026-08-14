#include "clang-frontend-private.hpp"
#include "clang-nns-utils.hpp"
#include "clang-source-qualification.hpp"

#include "RoseAst.h"
#include "nonrealQualificationSupport.h"
#include "sage3basic.h"

#include "sageInterface.h"

#include <algorithm>
#include <cctype>
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

#include <functional>
#include <limits>
#include <unordered_set>
namespace {
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

SgType *requireExactDecltypeSemanticBase(
    const clang::DecltypeType *decltype_type, SgExpression *expression,
    SgType *translated_underlying_type, const char *context) {
  if (translated_underlying_type != nullptr) {
    return requireExactType(translated_underlying_type, context, decltype_type);
  }
  if (decltype_type == nullptr || expression == nullptr ||
      (!decltype_type->isDependentType() &&
       !decltype_type->isInstantiationDependentType())) {
    failExactTypeTranslation(context, decltype_type);
  }

  // Clang intentionally leaves getUnderlyingType() null for an unresolved
  // dependent decltype.  The translated expression still owns the exact
  // dependent semantic type (normally an SgNonrealType or a wrapper around
  // one), which is the only valid base for the source SgDeclType until
  // instantiation resolves it.
  SgType *expression_type = expression->get_type();
  if (expression_type == nullptr ||
      SageInterface::containsUnknownType(expression_type) ||
      isSgTypeDefault(expression_type) != nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[" << context
              << "]: dependent decltype expression " << expression->class_name()
              << " has no exact dependent semantic type" << std::endl;
    ROSE_ABORT();
  }
  return expression_type;
}

static bool clangFrontendRunningOnValgrind() {
#if ROSE_USE_VALGRIND
  static const bool running = RUNNING_ON_VALGRIND;
  return running;
#else
  return false;
#endif
}

static void markClangAstStorageDefined(const void *address, size_t size) {
  markClangAstStorageRangeDefinedForFrontend(address, size);
}

static void markClangPrintedStringDefined(const std::string &value) {
#if ROSE_USE_VALGRIND
  if (clangFrontendRunningOnValgrind()) {
    VALGRIND_MAKE_MEM_DEFINED(const_cast<std::string *>(&value), sizeof(value));
    if (!value.empty()) {
      VALGRIND_MAKE_MEM_DEFINED(const_cast<char *>(value.data()), value.size());
    }
  }
#else
  (void)value;
#endif
}

template <typename T> T *markClangAstObjectDefined(T *node) {
#if ROSE_USE_VALGRIND
  if (clangFrontendRunningOnValgrind()) {
    // Clang's ASTContext uses bump allocation and leaves padding/unused bits in
    // AST objects undefined. REX only reads through Clang's public AST API;
    // make the Clang-owned object storage visible to Memcheck at that boundary.
    VALGRIND_MAKE_MEM_DEFINED(&node, sizeof(node));
    markClangAstStorageDefined(node, sizeof(*node));
  }
#endif
  return node;
}

template <typename T> const T *markClangAstObjectDefined(const T *node) {
#if ROSE_USE_VALGRIND
  if (clangFrontendRunningOnValgrind()) {
    VALGRIND_MAKE_MEM_DEFINED(&node, sizeof(node));
    markClangAstStorageDefined(node, sizeof(*node));
  }
#endif
  return node;
}

template <typename T> const T *markClangLocalObjectDefined(const T *object) {
#if ROSE_USE_VALGRIND
  if (clangFrontendRunningOnValgrind()) {
    VALGRIND_MAKE_MEM_DEFINED(&object, sizeof(object));
    if (object != nullptr) {
      VALGRIND_MAKE_MEM_DEFINED(const_cast<T *>(object), sizeof(*object));
    }
  }
#endif
  return object;
}

template <typename T> const T &markClangValueDefined(const T &value) {
#if ROSE_USE_VALGRIND
  if (clangFrontendRunningOnValgrind()) {
    VALGRIND_MAKE_MEM_DEFINED(const_cast<T *>(&value), sizeof(value));
  }
#endif
  return value;
}

template <typename F>
static auto readClangApiValueDefined(F &&read) -> decltype(read()) {
#if ROSE_USE_VALGRIND
  if (clangFrontendRunningOnValgrind()) {
    VALGRIND_DISABLE_ERROR_REPORTING;
    auto value = read();
    VALGRIND_ENABLE_ERROR_REPORTING;
    markClangValueDefined(value);
    return value;
  }
#endif
  auto value = read();
  markClangValueDefined(value);
  return value;
}

static const clang::Type *
markClangTypeObjectDefinedByClass(const clang::Type *type);
static const clang::Expr *
markClangExprObjectDefinedByClass(const clang::Expr *expr);
static clang::NestedNameSpecifier
markClangNestedNameSpecifierDefined(clang::NestedNameSpecifier specifier);

static void
markClangTagTypeQualifierStorageDefined(const clang::TagType *tag_type,
                                        clang::NestedNameSpecifier qualifier) {
#if ROSE_USE_VALGRIND
  if (!clangFrontendRunningOnValgrind() || tag_type == nullptr || !qualifier) {
    return;
  }

  const void *trailing_pointer = nullptr;
  switch (tag_type->getTypeClass()) {
  case clang::Type::Enum:
    trailing_pointer = static_cast<const clang::EnumType *>(tag_type) + 1;
    break;
  case clang::Type::Record:
    trailing_pointer = static_cast<const clang::RecordType *>(tag_type) + 1;
    break;
  case clang::Type::InjectedClassName:
    trailing_pointer =
        static_cast<const clang::InjectedClassNameType *>(tag_type) + 1;
    break;
  default:
    break;
  }
  if (trailing_pointer == nullptr) {
    return;
  }

  const std::uintptr_t raw = reinterpret_cast<std::uintptr_t>(trailing_pointer);
  const std::uintptr_t alignment = alignof(clang::NestedNameSpecifier *);
  const std::uintptr_t aligned = (raw + alignment - 1) & ~(alignment - 1);
  VALGRIND_MAKE_MEM_DEFINED(reinterpret_cast<void *>(aligned),
                            sizeof(clang::NestedNameSpecifier));
#else
  (void)tag_type;
  (void)qualifier;
#endif
}

static void markClangTypeLocDataDefined(const clang::TypeLoc &type_loc) {
#if ROSE_USE_VALGRIND
  markClangValueDefined(type_loc);
  if (!clangFrontendRunningOnValgrind() || type_loc.isNull()) {
    return;
  }

  clang::QualType qual_type =
      readClangApiValueDefined([&]() { return type_loc.getType(); });
  markClangValueDefined(qual_type);
  markClangTypeObjectDefinedByClass(qual_type.getTypePtrOrNull());

  void *data = type_loc.getOpaqueData();
  const unsigned size = readClangApiValueDefined(
      [&]() { return clang::TypeLoc::getFullDataSizeForType(qual_type); });
  if (data != nullptr && size != 0) {
    VALGRIND_MAKE_MEM_DEFINED(data, size);
  }
#else
  (void)type_loc;
#endif
}

static clang::QualType markClangQualTypeDefined(clang::QualType qual_type) {
#if ROSE_USE_VALGRIND
  markClangValueDefined(qual_type);
  if (clangFrontendRunningOnValgrind()) {
    markClangTypeObjectDefinedByClass(qual_type.getTypePtrOrNull());
  }
#endif
  return qual_type;
}

static const clang::Decl *
markClangDeclObjectDefinedByKind(const clang::Decl *decl);
static void
markClangDeclarationNameForPrintingDefined(const clang::NamedDecl *decl);
static void markClangQualTypeForPrintingDefined(
    clang::QualType qual_type,
    llvm::SmallPtrSetImpl<const clang::Type *> &seen);
static const clang::CXXRecordDecl *
markClangCXXRecordTemplateOrInstantiationStateDefined(
    const clang::CXXRecordDecl *record);
static const clang::ClassTemplateDecl *
markClangClassTemplateDeclForPrintingDefined(
    const clang::ClassTemplateDecl *decl);
static const clang::ClassTemplateSpecializationDecl *
markClangClassTemplateSpecializationForPrintingDefined(
    const clang::ClassTemplateSpecializationDecl *decl);
static void markClangTemplateParameterInjectedArgumentsDefined(
    const clang::TemplateParameterList *params,
    const clang::ASTContext &context);
static void markClangTemplateParameterInjectedArgumentsForPrintingDefined(
    const clang::TemplateParameterList *params,
    const clang::ASTContext &context);

static const clang::TemplateParameterList *
markClangTemplateParameterListDefined(
    const clang::TemplateParameterList *params) {
#if ROSE_USE_VALGRIND
  markClangValueDefined(params);
  if (!clangFrontendRunningOnValgrind() || params == nullptr) {
    return params;
  }

  params = markClangAstObjectDefined(params);
  const unsigned param_count =
      readClangApiValueDefined([&]() { return params->size(); });
  const clang::TemplateParameterList::const_iterator param_begin =
      readClangApiValueDefined([&]() { return params->begin(); });
  markClangValueDefined(param_begin);
  if (param_begin != nullptr && param_count != 0) {
    VALGRIND_MAKE_MEM_DEFINED(const_cast<clang::NamedDecl **>(param_begin),
                              param_count * sizeof(clang::NamedDecl *));
    for (unsigned i = 0; i < param_count; ++i) {
      markClangDeclObjectDefinedByKind(param_begin[i]);
    }
  }
#endif
  return params;
}

static const clang::Decl *
markClangDeclObjectDefinedByKind(const clang::Decl *decl) {
#if ROSE_USE_VALGRIND
  markClangValueDefined(decl);
  if (!clangFrontendRunningOnValgrind() || decl == nullptr) {
    return decl;
  }

  decl = markClangAstObjectDefined(decl);
  if (auto *specific =
          llvm::dyn_cast<clang::ClassTemplatePartialSpecializationDecl>(decl)) {
    specific = markClangAstObjectDefined(specific);
    markClangCXXRecordTemplateOrInstantiationStateDefined(specific);
    markClangClassTemplateSpecializationForPrintingDefined(specific);
    return specific;
  }
  if (auto *specific =
          llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(decl)) {
    specific = markClangAstObjectDefined(specific);
    markClangCXXRecordTemplateOrInstantiationStateDefined(specific);
    markClangClassTemplateSpecializationForPrintingDefined(specific);
    return specific;
  }
  if (auto *specific = llvm::dyn_cast<clang::ClassTemplateDecl>(decl)) {
    return markClangClassTemplateDeclForPrintingDefined(specific);
  }
  if (auto *specific = llvm::dyn_cast<clang::CXXRecordDecl>(decl)) {
    specific = markClangAstObjectDefined(specific);
    markClangCXXRecordTemplateOrInstantiationStateDefined(specific);
    return specific;
  }
  if (auto *specific = llvm::dyn_cast<clang::RecordDecl>(decl)) {
    return markClangAstObjectDefined(specific);
  }
  if (auto *specific = llvm::dyn_cast<clang::NonTypeTemplateParmDecl>(decl)) {
    return markClangAstObjectDefined(specific);
  }
  if (auto *specific = llvm::dyn_cast<clang::TemplateTypeParmDecl>(decl)) {
    return markClangAstObjectDefined(specific);
  }
  if (auto *specific = llvm::dyn_cast<clang::TemplateTemplateParmDecl>(decl)) {
    return markClangAstObjectDefined(specific);
  }
  if (auto *specific = llvm::dyn_cast<clang::FunctionTemplateDecl>(decl)) {
    return markClangAstObjectDefined(specific);
  }
  if (auto *specific = llvm::dyn_cast<clang::TypeAliasTemplateDecl>(decl)) {
    return markClangAstObjectDefined(specific);
  }
  if (auto *specific =
          llvm::dyn_cast<clang::VarTemplatePartialSpecializationDecl>(decl)) {
    return markClangAstObjectDefined(specific);
  }
  if (auto *specific =
          llvm::dyn_cast<clang::VarTemplateSpecializationDecl>(decl)) {
    return markClangAstObjectDefined(specific);
  }
  if (auto *specific = llvm::dyn_cast<clang::ParmVarDecl>(decl)) {
    return markClangAstObjectDefined(specific);
  }
  if (auto *specific = llvm::dyn_cast<clang::VarDecl>(decl)) {
    return markClangAstObjectDefined(specific);
  }
  if (auto *specific = llvm::dyn_cast<clang::VarTemplateDecl>(decl)) {
    return markClangAstObjectDefined(specific);
  }
  if (auto *specific = llvm::dyn_cast<clang::BuiltinTemplateDecl>(decl)) {
    return markClangAstObjectDefined(specific);
  }
  if (auto *specific = llvm::dyn_cast<clang::TemplateDecl>(decl)) {
    return markClangAstObjectDefined(specific);
  }
  if (auto *specific = llvm::dyn_cast<clang::TypeAliasDecl>(decl)) {
    return markClangAstObjectDefined(specific);
  }
  if (auto *specific = llvm::dyn_cast<clang::TypedefNameDecl>(decl)) {
    return markClangAstObjectDefined(specific);
  }
  if (auto *specific = llvm::dyn_cast<clang::CXXConstructorDecl>(decl)) {
    specific = markClangAstObjectDefined(specific);
    markClangDeclarationNameForPrintingDefined(specific);
    return specific;
  }
  if (auto *specific = llvm::dyn_cast<clang::CXXDestructorDecl>(decl)) {
    specific = markClangAstObjectDefined(specific);
    markClangDeclarationNameForPrintingDefined(specific);
    return specific;
  }
  if (auto *specific = llvm::dyn_cast<clang::CXXConversionDecl>(decl)) {
    specific = markClangAstObjectDefined(specific);
    markClangDeclarationNameForPrintingDefined(specific);
    return specific;
  }
  if (auto *specific = llvm::dyn_cast<clang::UsingShadowDecl>(decl)) {
    return markClangAstObjectDefined(specific);
  }
  if (auto *specific = llvm::dyn_cast<clang::UsingDecl>(decl)) {
    return markClangAstObjectDefined(specific);
  }
  if (auto *specific = llvm::dyn_cast<clang::ConceptDecl>(decl)) {
    return markClangAstObjectDefined(specific);
  }
  if (auto *specific = llvm::dyn_cast<clang::NamespaceAliasDecl>(decl)) {
    return markClangAstObjectDefined(specific);
  }
  if (auto *specific = llvm::dyn_cast<clang::DeclaratorDecl>(decl)) {
    return markClangAstObjectDefined(specific);
  }
  if (auto *specific = llvm::dyn_cast<clang::ValueDecl>(decl)) {
    return markClangAstObjectDefined(specific);
  }
  if (auto *specific = llvm::dyn_cast<clang::EnumDecl>(decl)) {
    return markClangAstObjectDefined(specific);
  }
  if (auto *specific = llvm::dyn_cast<clang::NamespaceDecl>(decl)) {
    return markClangAstObjectDefined(specific);
  }
  if (auto *specific = llvm::dyn_cast<clang::TranslationUnitDecl>(decl)) {
    return markClangAstObjectDefined(specific);
  }
  return decl;
#else
  return decl;
#endif
}

static void
markClangDeclarationNameForPrintingDefined(const clang::NamedDecl *decl) {
#if ROSE_USE_VALGRIND
  decl = markClangAstObjectDefined(decl);
  if (!clangFrontendRunningOnValgrind() || decl == nullptr) {
    return;
  }
  if (!markClangFrontendValgrindPublicationOnce(
          ClangFrontendValgrindPublicationKind::TypeNamedDeclaration, decl)) {
    return;
  }

  clang::DeclarationName decl_name =
      readClangApiValueDefined([&]() { return decl->getDeclName(); });
  markClangValueDefined(decl_name);
  switch (readClangApiValueDefined([&]() { return decl_name.getNameKind(); })) {
  case clang::DeclarationName::CXXConstructorName:
  case clang::DeclarationName::CXXDestructorName:
  case clang::DeclarationName::CXXConversionFunctionName:
    if (clang::QualType cxx_name_type = readClangApiValueDefined(
            [&]() { return decl_name.getCXXNameType(); });
        !cxx_name_type.isNull()) {
      llvm::SmallPtrSet<const clang::Type *, 32> seen;
      markClangQualTypeForPrintingDefined(cxx_name_type, seen);
    }
    break;
  default:
    break;
  }
#else
  (void)decl;
#endif
}

static const clang::CXXRecordDecl *
markClangCXXRecordTemplateOrInstantiationStateDefined(
    const clang::CXXRecordDecl *record) {
#if ROSE_USE_VALGRIND
  record = markClangAstObjectDefined(record);
  if (!clangFrontendRunningOnValgrind() || record == nullptr) {
    return record;
  }
  if (!markClangFrontendValgrindPublicationOnce(
          ClangFrontendValgrindPublicationKind::TypeRecordTemplateState,
          record)) {
    return record;
  }

  if (const clang::ClassTemplateDecl *described_template =
          readClangApiValueDefined(
              [&]() { return record->getDescribedClassTemplate(); })) {
    markClangDeclObjectDefinedByKind(described_template);
    const clang::TemplateParameterList *params = readClangApiValueDefined(
        [&]() { return described_template->getTemplateParameters(); });
    markClangTemplateParameterListDefined(params);
    const clang::ASTContext *context =
        readClangApiValueDefined([&]() { return &record->getASTContext(); });
    markClangTemplateParameterInjectedArgumentsForPrintingDefined(params,
                                                                  *context);
  }

  if (clang::MemberSpecializationInfo *member_info = readClangApiValueDefined(
          [&]() { return record->getMemberSpecializationInfo(); })) {
    markClangAstObjectDefined(member_info);
    markClangDeclObjectDefinedByKind(readClangApiValueDefined(
        [&]() { return member_info->getInstantiatedFrom(); }));
  }
#endif
  return record;
}

static const clang::ClassTemplateDecl *
markClangClassTemplateDeclForPrintingDefined(
    const clang::ClassTemplateDecl *decl) {
#if ROSE_USE_VALGRIND
  decl = markClangAstObjectDefined(decl);
  if (!clangFrontendRunningOnValgrind() || decl == nullptr) {
    return decl;
  }
  if (!markClangFrontendValgrindPublicationOnce(
          ClangFrontendValgrindPublicationKind::TypeClassTemplateState, decl)) {
    return decl;
  }

  const clang::TemplateParameterList *params =
      readClangApiValueDefined([&]() { return decl->getTemplateParameters(); });
  markClangTemplateParameterListDefined(params);

  if (const clang::CXXRecordDecl *templated_record = readClangApiValueDefined(
          [&]() { return decl->getTemplatedDecl(); })) {
    templated_record = llvm::dyn_cast_or_null<clang::CXXRecordDecl>(
        markClangDeclObjectDefinedByKind(templated_record));
    if (templated_record != nullptr) {
      const clang::ASTContext *context = readClangApiValueDefined(
          [&]() { return &templated_record->getASTContext(); });
      markClangTemplateParameterInjectedArgumentsForPrintingDefined(params,
                                                                    *context);
    }
  }
#endif
  return decl;
}

template <typename T> static T *markClangSpecificDeclDefined(T *decl) {
  return const_cast<T *>(
      llvm::dyn_cast_or_null<T>(markClangDeclObjectDefinedByKind(decl)));
}

template <typename TemplateDeclT>
static TemplateDeclT *
clangTemplateInstantiatedFromMemberTemplateDefined(TemplateDeclT *decl) {
  decl = markClangSpecificDeclDefined(decl);
  if (decl == nullptr) {
    return nullptr;
  }

  TemplateDeclT *member_template = readClangApiValueDefined(
      [&]() { return decl->getInstantiatedFromMemberTemplate(); });
  return markClangSpecificDeclDefined(member_template);
}

static clang::DeclContext *
markClangDeclContextObjectDefined(clang::DeclContext *context) {
#if ROSE_USE_VALGRIND
  markClangValueDefined(context);
  if (!clangFrontendRunningOnValgrind() || context == nullptr) {
    return context;
  }

  context = markClangAstObjectDefined(context);
  clang::Decl *decl = readClangApiValueDefined(
      [&]() { return clang::Decl::castFromDeclContext(context); });
  decl = const_cast<clang::Decl *>(markClangDeclObjectDefinedByKind(decl));
  return clang::Decl::castToDeclContext(decl);
#else
  return context;
#endif
}

static const clang::DeclContext *
markClangDeclContextObjectDefined(const clang::DeclContext *context) {
  return markClangDeclContextObjectDefined(
      const_cast<clang::DeclContext *>(context));
}

static clang::Decl *
clangDeclFromDeclContextDefined(clang::DeclContext *context) {
  context = markClangDeclContextObjectDefined(context);
  if (context == nullptr) {
    return nullptr;
  }
  clang::Decl *decl = readClangApiValueDefined(
      [&]() { return clang::Decl::castFromDeclContext(context); });
  return const_cast<clang::Decl *>(markClangDeclObjectDefinedByKind(decl));
}

static const clang::Decl *
clangDeclFromDeclContextDefined(const clang::DeclContext *context) {
  return clangDeclFromDeclContextDefined(
      const_cast<clang::DeclContext *>(context));
}

static clang::RecordDecl *markClangRecordDeclDefined(clang::RecordDecl *decl) {
  return const_cast<clang::RecordDecl *>(
      llvm::dyn_cast_or_null<clang::RecordDecl>(
          markClangDeclObjectDefinedByKind(decl)));
}

static const clang::CXXRecordDecl *
cxxRecordDeclFromQualTypeWithoutDefinitionLookup(clang::QualType qual_type) {
  qual_type = markClangQualTypeDefined(qual_type);
  if (qual_type.isNull()) {
    return nullptr;
  }

  clang::QualType canonical =
      markClangQualTypeDefined(qual_type.getCanonicalType());
  const clang::Type *canonical_type =
      markClangTypeObjectDefinedByClass(canonical.getTypePtrOrNull());
  const clang::TagType *tag_type =
      llvm::dyn_cast_or_null<clang::TagType>(canonical_type);
  if (tag_type == nullptr) {
    return nullptr;
  }

  const clang::TagDecl *tag_decl = llvm::dyn_cast_or_null<clang::TagDecl>(
      markClangDeclObjectDefinedByKind(tag_type->getDecl()));
  return llvm::dyn_cast_or_null<clang::CXXRecordDecl>(tag_decl);
}

static bool isClangLocalClassContext(clang::DeclContext *context) {
  context = markClangDeclContextObjectDefined(context);
  while (context != nullptr) {
    const clang::Decl *context_decl = markClangDeclObjectDefinedByKind(
        clang::Decl::castFromDeclContext(context));
    if (llvm::isa<clang::FunctionDecl>(context_decl)) {
      return true;
    }
    const clang::CXXRecordDecl *record =
        llvm::dyn_cast_or_null<clang::CXXRecordDecl>(context_decl);
    if (record == nullptr) {
      return false;
    }
    context = markClangDeclContextObjectDefined(
        const_cast<clang::DeclContext *>(record->getDeclContext()));
  }
  return false;
}

static clang::TemplateName
markClangTemplateNameDefined(clang::TemplateName name) {
#if ROSE_USE_VALGRIND
  markClangValueDefined(name);
  if (!clangFrontendRunningOnValgrind()) {
    return name;
  }

  switch (name.getKind()) {
  case clang::TemplateName::QualifiedTemplate:
    if (clang::QualifiedTemplateName *qualified =
            name.getAsQualifiedTemplateName()) {
      markClangAstObjectDefined(qualified);
      markClangNestedNameSpecifierDefined(qualified->getQualifier());
      markClangTemplateNameDefined(qualified->getUnderlyingTemplate());
    }
    break;
  case clang::TemplateName::DependentTemplate:
    if (clang::DependentTemplateName *dependent =
            name.getAsDependentTemplateName()) {
      markClangAstObjectDefined(dependent);
      markClangNestedNameSpecifierDefined(dependent->getQualifier());
    }
    break;
  case clang::TemplateName::SubstTemplateTemplateParm:
    if (clang::SubstTemplateTemplateParmStorage *subst =
            name.getAsSubstTemplateTemplateParm()) {
      markClangAstObjectDefined(subst);
      markClangDeclObjectDefinedByKind(subst->getAssociatedDecl());
      markClangTemplateNameDefined(subst->getReplacement());
    }
    break;
  case clang::TemplateName::Template:
  case clang::TemplateName::UsingTemplate:
  case clang::TemplateName::OverloadedTemplate:
  case clang::TemplateName::AssumedTemplate:
  case clang::TemplateName::SubstTemplateTemplateParmPack:
  case clang::TemplateName::DeducedTemplate:
    break;
  }

  if (clang::TemplateDecl *decl = name.getAsTemplateDecl()) {
    markClangDeclObjectDefinedByKind(decl);
  }
  if (clang::UsingShadowDecl *decl = name.getAsUsingShadowDecl()) {
    markClangDeclObjectDefinedByKind(decl);
  }
#endif
  return name;
}

static llvm::ArrayRef<clang::TemplateArgument>
markClangTemplateArgumentArrayDefined(
    llvm::ArrayRef<clang::TemplateArgument> args);

static const clang::TemplateArgument &
markClangTemplateArgumentDefined(const clang::TemplateArgument &arg) {
#if ROSE_USE_VALGRIND
  markClangValueDefined(arg);
  if (!clangFrontendRunningOnValgrind()) {
    return arg;
  }

  switch (readClangApiValueDefined([&]() { return arg.getKind(); })) {
  case clang::TemplateArgument::Type:
    markClangQualTypeDefined(
        readClangApiValueDefined([&]() { return arg.getAsType(); }));
    break;
  case clang::TemplateArgument::Declaration:
    markClangDeclObjectDefinedByKind(
        readClangApiValueDefined([&]() { return arg.getAsDecl(); }));
    markClangQualTypeDefined(
        readClangApiValueDefined([&]() { return arg.getParamTypeForDecl(); }));
    break;
  case clang::TemplateArgument::NullPtr:
    markClangQualTypeDefined(
        readClangApiValueDefined([&]() { return arg.getNullPtrType(); }));
    break;
  case clang::TemplateArgument::Integral:
    markClangQualTypeDefined(
        readClangApiValueDefined([&]() { return arg.getIntegralType(); }));
    break;
  case clang::TemplateArgument::StructuralValue:
    markClangLocalObjectDefined(readClangApiValueDefined(
        [&]() { return &arg.getAsStructuralValue(); }));
    markClangQualTypeDefined(readClangApiValueDefined(
        [&]() { return arg.getStructuralValueType(); }));
    break;
  case clang::TemplateArgument::Template:
  case clang::TemplateArgument::TemplateExpansion:
    markClangTemplateNameDefined(readClangApiValueDefined(
        [&]() { return arg.getAsTemplateOrTemplatePattern(); }));
    break;
  case clang::TemplateArgument::Expression:
    markClangExprObjectDefinedByClass(
        readClangApiValueDefined([&]() { return arg.getAsExpr(); }));
    break;
  case clang::TemplateArgument::Pack:
    (void)markClangTemplateArgumentArrayDefined(
        readClangApiValueDefined([&]() { return arg.pack_elements(); }));
    break;
  case clang::TemplateArgument::Null:
    break;
  }
#endif
  return arg;
}

static llvm::ArrayRef<clang::TemplateArgument>
markClangTemplateArgumentArrayDefined(
    llvm::ArrayRef<clang::TemplateArgument> args) {
#if ROSE_USE_VALGRIND
  markClangValueDefined(args);
  if (!clangFrontendRunningOnValgrind()) {
    return args;
  }
  if (args.data() != nullptr && !args.empty()) {
    VALGRIND_MAKE_MEM_DEFINED(
        const_cast<clang::TemplateArgument *>(args.data()),
        args.size() * sizeof(clang::TemplateArgument));
  }
  for (const clang::TemplateArgument &arg : args) {
    markClangTemplateArgumentDefined(arg);
  }
#endif
  return args;
}

static void markClangTemplateParameterInjectedArgumentsDefined(
    const clang::TemplateParameterList *params,
    const clang::ASTContext &context) {
#if ROSE_USE_VALGRIND
  params = markClangTemplateParameterListDefined(params);
  if (!clangFrontendRunningOnValgrind() || params == nullptr) {
    return;
  }

  auto *mutable_params = const_cast<clang::TemplateParameterList *>(params);
  llvm::ArrayRef<clang::TemplateArgument> injected_args =
      readClangApiValueDefined(
          [&]() { return mutable_params->getInjectedTemplateArgs(context); });
  params = markClangTemplateParameterListDefined(mutable_params);
  (void)params;
  (void)markClangTemplateArgumentArrayDefined(injected_args);
#else
  (void)params;
  (void)context;
#endif
}

static const clang::TemplateArgumentList *
markClangTemplateArgumentListDefined(const clang::TemplateArgumentList *args) {
#if ROSE_USE_VALGRIND
  if (clangFrontendRunningOnValgrind()) {
    VALGRIND_MAKE_MEM_DEFINED(&args, sizeof(args));
  }
  if (!clangFrontendRunningOnValgrind() || args == nullptr) {
    return args;
  }

  args = markClangAstObjectDefined(args);
  (void)markClangTemplateArgumentArrayDefined(
      readClangApiValueDefined([&]() { return args->asArray(); }));
#endif
  return args;
}

static const clang::ASTTemplateArgumentListInfo *
markClangASTTemplateArgumentListInfoDefined(
    const clang::ASTTemplateArgumentListInfo *args) {
#if ROSE_USE_VALGRIND
  if (clangFrontendRunningOnValgrind()) {
    VALGRIND_MAKE_MEM_DEFINED(&args, sizeof(args));
  }
  if (!clangFrontendRunningOnValgrind() || args == nullptr) {
    return args;
  }

  args = markClangAstObjectDefined(args);
  llvm::ArrayRef<clang::TemplateArgumentLoc> arg_locs = args->arguments();
  markClangValueDefined(arg_locs);
  if (!arg_locs.empty()) {
    VALGRIND_MAKE_MEM_DEFINED(
        const_cast<clang::TemplateArgumentLoc *>(arg_locs.data()),
        arg_locs.size() * sizeof(clang::TemplateArgumentLoc));
  }
  for (const clang::TemplateArgumentLoc &arg_loc : arg_locs) {
    markClangValueDefined(arg_loc);
    markClangTemplateArgumentDefined(
        readClangApiValueDefined([&]() { return arg_loc.getArgument(); }));
    if (clang::TypeSourceInfo *type_info = readClangApiValueDefined(
            [&]() { return arg_loc.getTypeSourceInfo(); })) {
      markClangAstObjectDefined(type_info);
      markClangTypeLocDataDefined(
          readClangApiValueDefined([&]() { return type_info->getTypeLoc(); }));
    }
  }
#endif
  return args;
}

static const clang::Type *
markClangTypeObjectDefinedByClass(const clang::Type *type) {
#if ROSE_USE_VALGRIND
  if (!clangFrontendRunningOnValgrind()) {
    return type;
  }

  type = markClangAstObjectDefined(type);
  if (type == nullptr) {
    return nullptr;
  }

  auto mark_tag_type =
      [](const clang::TagType *tag_type) -> const clang::TagType * {
    switch (tag_type->getTypeClass()) {
    case clang::Type::Enum:
      tag_type = markClangAstObjectDefined(
          static_cast<const clang::EnumType *>(tag_type));
      break;
    case clang::Type::Record:
      tag_type = markClangAstObjectDefined(
          static_cast<const clang::RecordType *>(tag_type));
      break;
    case clang::Type::InjectedClassName:
      tag_type = markClangAstObjectDefined(
          static_cast<const clang::InjectedClassNameType *>(tag_type));
      break;
    default:
      tag_type = markClangAstObjectDefined(tag_type);
      break;
    }
    if (tag_type != nullptr) {
      markClangDeclObjectDefinedByKind(tag_type->getDecl());
    }
    return tag_type;
  };

  switch (type->getTypeClass()) {
  case clang::Type::Adjusted:
    return markClangAstObjectDefined(
        static_cast<const clang::AdjustedType *>(type));
  case clang::Type::Decayed:
    return markClangAstObjectDefined(
        static_cast<const clang::DecayedType *>(type));
  case clang::Type::ConstantArray:
    return markClangAstObjectDefined(
        static_cast<const clang::ConstantArrayType *>(type));
  case clang::Type::DependentSizedArray:
    return markClangAstObjectDefined(
        static_cast<const clang::DependentSizedArrayType *>(type));
  case clang::Type::IncompleteArray:
    return markClangAstObjectDefined(
        static_cast<const clang::IncompleteArrayType *>(type));
  case clang::Type::VariableArray:
    return markClangAstObjectDefined(
        static_cast<const clang::VariableArrayType *>(type));
  case clang::Type::Atomic:
    return markClangAstObjectDefined(
        static_cast<const clang::AtomicType *>(type));
  case clang::Type::Attributed:
    return markClangAstObjectDefined(
        static_cast<const clang::AttributedType *>(type));
  case clang::Type::BlockPointer:
    return markClangAstObjectDefined(
        static_cast<const clang::BlockPointerType *>(type));
  case clang::Type::Builtin:
    return markClangAstObjectDefined(
        static_cast<const clang::BuiltinType *>(type));
  case clang::Type::Complex:
    return markClangAstObjectDefined(
        static_cast<const clang::ComplexType *>(type));
  case clang::Type::Decltype:
    return markClangAstObjectDefined(
        static_cast<const clang::DecltypeType *>(type));
  case clang::Type::Auto:
    return markClangAstObjectDefined(
        static_cast<const clang::AutoType *>(type));
  case clang::Type::DeducedTemplateSpecialization:
    return markClangAstObjectDefined(
        static_cast<const clang::DeducedTemplateSpecializationType *>(type));
  case clang::Type::DependentSizedExtVector:
    return markClangAstObjectDefined(
        static_cast<const clang::DependentSizedExtVectorType *>(type));
  case clang::Type::DependentVector:
    return markClangAstObjectDefined(
        static_cast<const clang::DependentVectorType *>(type));
  case clang::Type::FunctionNoProto:
    return markClangAstObjectDefined(
        static_cast<const clang::FunctionNoProtoType *>(type));
  case clang::Type::FunctionProto:
    return markClangAstObjectDefined(
        static_cast<const clang::FunctionProtoType *>(type));
  case clang::Type::InjectedClassName:
    return mark_tag_type(
        static_cast<const clang::InjectedClassNameType *>(type));
  case clang::Type::MacroQualified:
    return markClangAstObjectDefined(
        static_cast<const clang::MacroQualifiedType *>(type));
  case clang::Type::MemberPointer:
    return markClangAstObjectDefined(
        static_cast<const clang::MemberPointerType *>(type));
  case clang::Type::PackExpansion:
    return markClangAstObjectDefined(
        static_cast<const clang::PackExpansionType *>(type));
  case clang::Type::Paren:
    return markClangAstObjectDefined(
        static_cast<const clang::ParenType *>(type));
  case clang::Type::Pipe:
    return markClangAstObjectDefined(
        static_cast<const clang::PipeType *>(type));
  case clang::Type::Pointer:
    return markClangAstObjectDefined(
        static_cast<const clang::PointerType *>(type));
  case clang::Type::LValueReference:
    return markClangAstObjectDefined(
        static_cast<const clang::LValueReferenceType *>(type));
  case clang::Type::RValueReference:
    return markClangAstObjectDefined(
        static_cast<const clang::RValueReferenceType *>(type));
  case clang::Type::SubstTemplateTypeParmPack:
    return markClangAstObjectDefined(
        static_cast<const clang::SubstTemplateTypeParmPackType *>(type));
  case clang::Type::SubstTemplateTypeParm:
    return markClangAstObjectDefined(
        static_cast<const clang::SubstTemplateTypeParmType *>(type));
  case clang::Type::Enum:
    return mark_tag_type(static_cast<const clang::EnumType *>(type));
  case clang::Type::Record:
    return mark_tag_type(static_cast<const clang::RecordType *>(type));
  case clang::Type::TemplateSpecialization:
    return markClangAstObjectDefined(
        static_cast<const clang::TemplateSpecializationType *>(type));
  case clang::Type::TemplateTypeParm:
    return markClangAstObjectDefined(
        static_cast<const clang::TemplateTypeParmType *>(type));
  case clang::Type::Typedef:
    return markClangAstObjectDefined(
        static_cast<const clang::TypedefType *>(type));
  case clang::Type::TypeOfExpr:
    return markClangAstObjectDefined(
        static_cast<const clang::TypeOfExprType *>(type));
  case clang::Type::TypeOf:
    return markClangAstObjectDefined(
        static_cast<const clang::TypeOfType *>(type));
  case clang::Type::DependentName:
    return markClangAstObjectDefined(
        static_cast<const clang::DependentNameType *>(type));
  case clang::Type::UnaryTransform:
    return markClangAstObjectDefined(
        static_cast<const clang::UnaryTransformType *>(type));
  case clang::Type::UnresolvedUsing:
    return markClangAstObjectDefined(
        static_cast<const clang::UnresolvedUsingType *>(type));
  case clang::Type::Vector:
    return markClangAstObjectDefined(
        static_cast<const clang::VectorType *>(type));
  case clang::Type::ExtVector:
    return markClangAstObjectDefined(
        static_cast<const clang::ExtVectorType *>(type));
  case clang::Type::Using:
    return markClangAstObjectDefined(
        static_cast<const clang::UsingType *>(type));
  default:
    return type;
  }
#else
  return type;
#endif
}

static const clang::Stmt *
markClangStmtObjectDefinedByClass(const clang::Stmt *stmt) {
#if ROSE_USE_VALGRIND
  if (clangFrontendRunningOnValgrind()) {
    VALGRIND_MAKE_MEM_DEFINED(&stmt, sizeof(stmt));
  }
  if (!clangFrontendRunningOnValgrind() || stmt == nullptr) {
    return stmt;
  }

  stmt = markClangAstObjectDefined(stmt);
  switch (stmt->getStmtClass()) {
  case clang::Stmt::ConstantExprClass:
    return markClangAstObjectDefined(
        static_cast<const clang::ConstantExpr *>(stmt));
  case clang::Stmt::CXXBindTemporaryExprClass:
    return markClangAstObjectDefined(
        static_cast<const clang::CXXBindTemporaryExpr *>(stmt));
  case clang::Stmt::CXXConstructExprClass:
    return markClangAstObjectDefined(
        static_cast<const clang::CXXConstructExpr *>(stmt));
  case clang::Stmt::CXXDefaultArgExprClass:
    return markClangAstObjectDefined(
        static_cast<const clang::CXXDefaultArgExpr *>(stmt));
  case clang::Stmt::DeclRefExprClass:
    return markClangAstObjectDefined(
        static_cast<const clang::DeclRefExpr *>(stmt));
  case clang::Stmt::ExprWithCleanupsClass:
    return markClangAstObjectDefined(
        static_cast<const clang::ExprWithCleanups *>(stmt));
  case clang::Stmt::ImplicitCastExprClass:
    return markClangAstObjectDefined(
        static_cast<const clang::ImplicitCastExpr *>(stmt));
  case clang::Stmt::MaterializeTemporaryExprClass:
    return markClangAstObjectDefined(
        static_cast<const clang::MaterializeTemporaryExpr *>(stmt));
  case clang::Stmt::ParenExprClass:
    return markClangAstObjectDefined(
        static_cast<const clang::ParenExpr *>(stmt));
  case clang::Stmt::SubstNonTypeTemplateParmExprClass:
    return markClangAstObjectDefined(
        static_cast<const clang::SubstNonTypeTemplateParmExpr *>(stmt));
  default:
    return stmt;
  }
#else
  return stmt;
#endif
}

template <typename F>
static auto readClangTypeLocDefined(F &&read) -> decltype(read()) {
  auto type_loc = readClangApiValueDefined(std::forward<F>(read));
  markClangTypeLocDataDefined(type_loc);
  return type_loc;
}

static const clang::Expr *
markClangExprObjectDefinedByClass(const clang::Expr *expr) {
  return llvm::dyn_cast_or_null<clang::Expr>(
      markClangStmtObjectDefinedByClass(expr));
}

static clang::NestedNameSpecifier
markClangNestedNameSpecifierDefined(clang::NestedNameSpecifier specifier) {
#if ROSE_USE_VALGRIND
  markClangValueDefined(specifier);
  if (!clangFrontendRunningOnValgrind()) {
    return specifier;
  }

  for (clang::NestedNameSpecifier current = specifier;;) {
    markClangValueDefined(current);
    if (!current) {
      break;
    }
    switch (readClangApiValueDefined([&]() { return current.getKind(); })) {
    case clang::NestedNameSpecifier::Kind::Namespace:
      markClangDeclObjectDefinedByKind(
          nestedNameSpecifierNamespaceBase(current));
      break;
    case clang::NestedNameSpecifier::Kind::Type:
      markClangTypeObjectDefinedByClass(
          readClangApiValueDefined([&]() { return current.getAsType(); }));
      break;
    case clang::NestedNameSpecifier::Kind::MicrosoftSuper:
      markClangDeclObjectDefinedByKind(readClangApiValueDefined(
          [&]() { return current.getAsRecordDecl(); }));
      break;
    case clang::NestedNameSpecifier::Kind::Global:
    case clang::NestedNameSpecifier::Kind::Null:
      break;
    }
    current = markClangNestedNameSpecifierDefined(readClangApiValueDefined(
        [&]() { return nestedNameSpecifierPrefix(current); }));
  }
#endif
  return specifier;
}

static void markClangTypeForPrintingDefined(
    const clang::Type *type, llvm::SmallPtrSetImpl<const clang::Type *> &seen);

static void markClangQualTypeForPrintingDefined(
    clang::QualType qual_type,
    llvm::SmallPtrSetImpl<const clang::Type *> &seen) {
#if ROSE_USE_VALGRIND
  qual_type = markClangQualTypeDefined(qual_type);
  if (!qual_type.isNull()) {
    markClangTypeForPrintingDefined(qual_type.getTypePtrOrNull(), seen);
  }
#else
  (void)qual_type;
  (void)seen;
#endif
}

static const clang::TemplateArgument &
markClangTemplateArgumentForPrintingDefined(
    const clang::TemplateArgument &arg,
    llvm::SmallPtrSetImpl<const clang::Type *> &seen) {
#if ROSE_USE_VALGRIND
  const clang::TemplateArgument &defined_arg =
      markClangTemplateArgumentDefined(arg);
  if (!clangFrontendRunningOnValgrind()) {
    return defined_arg;
  }

  switch (defined_arg.getKind()) {
  case clang::TemplateArgument::Type:
    markClangQualTypeForPrintingDefined(defined_arg.getAsType(), seen);
    break;
  case clang::TemplateArgument::Pack: {
    llvm::ArrayRef<clang::TemplateArgument> elements =
        markClangTemplateArgumentArrayDefined(defined_arg.pack_elements());
    for (const clang::TemplateArgument &element : elements) {
      markClangTemplateArgumentForPrintingDefined(element, seen);
    }
    break;
  }
  case clang::TemplateArgument::Declaration:
  case clang::TemplateArgument::NullPtr:
  case clang::TemplateArgument::Integral:
  case clang::TemplateArgument::StructuralValue:
  case clang::TemplateArgument::Template:
  case clang::TemplateArgument::TemplateExpansion:
  case clang::TemplateArgument::Null:
    break;
  case clang::TemplateArgument::Expression:
    markClangExprObjectDefinedByClass(
        readClangApiValueDefined([&]() { return defined_arg.getAsExpr(); }));
    break;
  }
  return defined_arg;
#else
  (void)seen;
  return arg;
#endif
}

static const clang::TemplateArgument &
markClangTemplateArgumentForPrintingDefined(
    const clang::TemplateArgument &arg) {
#if ROSE_USE_VALGRIND
  llvm::SmallPtrSet<const clang::Type *, 32> seen;
  return markClangTemplateArgumentForPrintingDefined(arg, seen);
#else
  return arg;
#endif
}

static void markClangTemplateParameterInjectedArgumentsForPrintingDefined(
    const clang::TemplateParameterList *params,
    const clang::ASTContext &context) {
#if ROSE_USE_VALGRIND
  params = markClangTemplateParameterListDefined(params);
  if (!clangFrontendRunningOnValgrind() || params == nullptr) {
    return;
  }

  auto *mutable_params = const_cast<clang::TemplateParameterList *>(params);
  llvm::ArrayRef<clang::TemplateArgument> injected_args =
      readClangApiValueDefined(
          [&]() { return mutable_params->getInjectedTemplateArgs(context); });
  params = markClangTemplateParameterListDefined(mutable_params);
  (void)params;

  injected_args = markClangTemplateArgumentArrayDefined(injected_args);
  llvm::SmallPtrSet<const clang::Type *, 32> seen;
  for (const clang::TemplateArgument &arg : injected_args) {
    markClangTemplateArgumentForPrintingDefined(arg, seen);
  }
#else
  (void)params;
  (void)context;
#endif
}

static const clang::TemplateArgumentList *
markClangTemplateArgumentListForPrintingDefined(
    const clang::TemplateArgumentList *args,
    llvm::SmallPtrSetImpl<const clang::Type *> &seen) {
#if ROSE_USE_VALGRIND
  args = markClangTemplateArgumentListDefined(args);
  if (!clangFrontendRunningOnValgrind() || args == nullptr) {
    return args;
  }

  for (unsigned i = 0; i < args->size(); ++i) {
    markClangTemplateArgumentForPrintingDefined(args->get(i), seen);
  }
#else
  (void)seen;
#endif
  return args;
}

static const clang::ClassTemplateSpecializationDecl *
markClangClassTemplateSpecializationForPrintingDefined(
    const clang::ClassTemplateSpecializationDecl *decl) {
#if ROSE_USE_VALGRIND
  static thread_local std::unordered_set<
      const clang::ClassTemplateSpecializationDecl *>
      active_specializations;
  const clang::ClassTemplateSpecializationDecl *active_key = decl;
  if (active_key != nullptr &&
      !active_specializations.insert(active_key).second) {
    return decl;
  }

  decl = llvm::dyn_cast_or_null<clang::ClassTemplateSpecializationDecl>(
      markClangDeclObjectDefinedByKind(decl));
  if (!clangFrontendRunningOnValgrind() || decl == nullptr) {
    if (active_key != nullptr) {
      active_specializations.erase(active_key);
    }
    return decl;
  }
  if (!markClangFrontendValgrindPublicationOnce(
          ClangFrontendValgrindPublicationKind::
              TypeClassTemplateSpecializationState,
          decl)) {
    if (active_key != nullptr) {
      active_specializations.erase(active_key);
    }
    return decl;
  }
  llvm::SmallPtrSet<const clang::Type *, 32> seen;
  markClangTemplateArgumentListForPrintingDefined(&decl->getTemplateArgs(),
                                                  seen);
  markClangTemplateArgumentListForPrintingDefined(
      &decl->getTemplateInstantiationArgs(), seen);
  if (const clang::ASTTemplateArgumentListInfo *written_args =
          markClangASTTemplateArgumentListInfoDefined(
              decl->getTemplateArgsAsWritten())) {
    for (const clang::TemplateArgumentLoc &arg_loc :
         written_args->arguments()) {
      markClangTemplateArgumentForPrintingDefined(arg_loc.getArgument(), seen);
    }
  }

  auto specialized = readClangApiValueDefined(
      [&]() { return decl->getSpecializedTemplateOrPartial(); });
  if (const auto *class_template =
          specialized.dyn_cast<clang::ClassTemplateDecl *>()) {
    markClangClassTemplateDeclForPrintingDefined(class_template);
  } else if (const auto *partial =
                 specialized.dyn_cast<
                     clang::ClassTemplatePartialSpecializationDecl *>()) {
    markClangDeclObjectDefinedByKind(partial);
    markClangClassTemplateDeclForPrintingDefined(readClangApiValueDefined(
        [&]() { return partial->getSpecializedTemplate(); }));
  }

  if (active_key != nullptr) {
    active_specializations.erase(active_key);
  }
#endif
  return decl;
}

static void markClangTypeForPrintingDefined(
    const clang::Type *type, llvm::SmallPtrSetImpl<const clang::Type *> &seen) {
#if ROSE_USE_VALGRIND
  if (!clangFrontendRunningOnValgrind()) {
    return;
  }

  type = markClangTypeObjectDefinedByClass(type);
  if (type == nullptr || !seen.insert(type).second ||
      !markClangFrontendValgrindPublicationOnce(
          ClangFrontendValgrindPublicationKind::TypePrinting, type)) {
    return;
  }

  switch (type->getTypeClass()) {
  case clang::Type::Enum:
  case clang::Type::Record:
  case clang::Type::InjectedClassName: {
    const auto *tag_type = llvm::dyn_cast<clang::TagType>(type);
    if (tag_type != nullptr) {
      clang::NestedNameSpecifier qualifier =
          readClangApiValueDefined([&]() { return tag_type->getQualifier(); });
      markClangTagTypeQualifierStorageDefined(tag_type, qualifier);
      markClangNestedNameSpecifierDefined(qualifier);
      const clang::TagDecl *raw_tag_decl =
          readClangApiValueDefined([&]() { return tag_type->getDecl(); });
      const clang::TagDecl *tag_decl = llvm::dyn_cast_or_null<clang::TagDecl>(
          markClangDeclObjectDefinedByKind(raw_tag_decl));
      if (const auto *injected_type =
              llvm::dyn_cast<clang::InjectedClassNameType>(type)) {
        const clang::CXXRecordDecl *record =
            llvm::dyn_cast_or_null<clang::CXXRecordDecl>(tag_decl);
        if (record != nullptr) {
          if (const clang::ClassTemplateDecl *template_decl =
                  readClangApiValueDefined(
                      [&]() { return injected_type->getTemplateDecl(); })) {
            markClangDeclObjectDefinedByKind(template_decl);
            const clang::TemplateParameterList *params =
                readClangApiValueDefined(
                    [&]() { return template_decl->getTemplateParameters(); });
            const clang::ASTContext *context = readClangApiValueDefined(
                [&]() { return &record->getASTContext(); });
            markClangTemplateParameterInjectedArgumentsForPrintingDefined(
                params, *context);
          }

          if (auto *specialization =
                  llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(
                      record)) {
            markClangClassTemplateSpecializationForPrintingDefined(
                specialization);
          } else if (const clang::ClassTemplateDecl *described_template =
                         record->getDescribedClassTemplate()) {
            markClangDeclObjectDefinedByKind(described_template);
            const clang::ASTContext *context = readClangApiValueDefined(
                [&]() { return &record->getASTContext(); });
            markClangTemplateParameterInjectedArgumentsForPrintingDefined(
                described_template->getTemplateParameters(), *context);
          }
        }
      }
    }
    break;
  }
  case clang::Type::TemplateSpecialization: {
    const auto *template_specialization =
        static_cast<const clang::TemplateSpecializationType *>(
            markClangTypeObjectDefinedByClass(type));
    markClangTemplateNameDefined(readClangApiValueDefined(
        [&]() { return template_specialization->getTemplateName(); }));
    llvm::ArrayRef<clang::TemplateArgument> args =
        markClangTemplateArgumentArrayDefined(readClangApiValueDefined(
            [&]() { return template_specialization->template_arguments(); }));
    for (const clang::TemplateArgument &arg : args) {
      markClangTemplateArgumentForPrintingDefined(arg, seen);
    }
    break;
  }
  default:
    break;
  }
#else
  (void)type;
  (void)seen;
#endif
}

static unsigned
clangNestedNameSpecifierLocDataLength(clang::NestedNameSpecifier specifier) {
  unsigned length = 0;
  for (clang::NestedNameSpecifier current =
           markClangNestedNameSpecifierDefined(specifier);
       current; current = markClangNestedNameSpecifierDefined(
                    current.getAsNamespaceAndPrefix().Prefix)) {
    length += sizeof(clang::SourceLocation::UIntTy);
    switch (current.getKind()) {
    case clang::NestedNameSpecifier::Kind::Global:
      break;
    case clang::NestedNameSpecifier::Kind::Namespace:
    case clang::NestedNameSpecifier::Kind::MicrosoftSuper:
      length += sizeof(clang::SourceLocation::UIntTy);
      break;
    case clang::NestedNameSpecifier::Kind::Type:
      length += sizeof(void *);
      break;
    case clang::NestedNameSpecifier::Kind::Null:
      return length;
    }
    if (current.getKind() != clang::NestedNameSpecifier::Kind::Namespace) {
      break;
    }
  }
  return length;
}

static clang::NestedNameSpecifierLoc markClangNestedNameSpecifierLocDefined(
    clang::NestedNameSpecifierLoc qualifier_loc) {
#if ROSE_USE_VALGRIND
  markClangValueDefined(qualifier_loc);
  if (!clangFrontendRunningOnValgrind() || !qualifier_loc) {
    return qualifier_loc;
  }

  clang::NestedNameSpecifier qualifier = markClangNestedNameSpecifierDefined(
      qualifier_loc.getNestedNameSpecifier());
  if (void *data = qualifier_loc.getOpaqueData()) {
    const unsigned size = clangNestedNameSpecifierLocDataLength(qualifier);
    if (size != 0) {
      VALGRIND_MAKE_MEM_DEFINED(data, size);
    }
  }
  if (qualifier.getKind() == clang::NestedNameSpecifier::Kind::Type) {
    markClangTypeLocDataDefined(qualifier_loc.getAsTypeLoc());
  }
#endif
  return qualifier_loc;
}

std::string buildOverloadedOperatorName(clang::OverloadedOperatorKind op) {
  const char *spelling = clang::getOperatorSpelling(op);
  ROSE_ASSERT(spelling != nullptr);

  std::string result = "operator";
  if (std::isalpha(static_cast<unsigned char>(spelling[0])) ||
      spelling[0] == '_') {
    result += ' ';
  }
  result += spelling;
  return result;
}

unsigned int roseMemberFunctionSpecifierFromClangProto(
    const clang::FunctionProtoType *type) {
  if (type == nullptr) {
    return 0;
  }

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
  const SgTypePtrList &argument_types = arguments->get_arguments();
  const SgFunctionTypeArgumentPtrList &qualification_use_sites =
      arguments->get_argument_qualification_use_sites();
  if (argument_types.size() != qualification_use_sites.size()) {
    std::cerr
        << "REX_FRONTEND_INVARIANT[function-argument-qualification-use-site]: "
        << "context=" << (context != nullptr ? context : "<unknown>")
        << " function type " << function_type << " has "
        << argument_types.size() << " argument types but "
        << qualification_use_sites.size() << " qualification identities"
        << std::endl;
    ROSE_ABORT();
  }
  for (size_t i = 0; i < argument_types.size(); ++i) {
    SgType *argument_type = argument_types[i];
    SgFunctionTypeArgument *qualification_use_site = qualification_use_sites[i];
    if (argument_type == nullptr || qualification_use_site == nullptr ||
        qualification_use_site->get_type() != argument_type ||
        qualification_use_site->get_parent() != arguments) {
      std::cerr << "REX_FRONTEND_INVARIANT[function-argument-qualification-"
                   "use-site]: context="
                << (context != nullptr ? context : "<unknown>")
                << " function type " << function_type << " argument " << i
                << " lacks an exact typed qualification identity" << std::endl;
      ROSE_ABORT();
    }
  }
}

SgFunctionParameterTypeList *
cloneFunctionArgumentTypes(const SgFunctionType *source, const char *context) {
  requireExactFunctionArgumentQualificationUseSites(source, context);
  SgFunctionParameterTypeList *clone = new SgFunctionParameterTypeList();
  ASSERT_not_null(clone);
  const SgTypePtrList &argument_types = source->get_arguments();
  const SgFunctionTypeArgumentPtrList &source_positions =
      source->get_argument_list()->get_argument_qualification_use_sites();
  for (size_t index = 0; index < argument_types.size(); ++index) {
    clone->append_argument(argument_types[index]);
    SgFunctionTypeArgument *cloned_position =
        clone->get_argument_qualification_use_sites().back();
    ASSERT_not_null(cloned_position);
    cloned_position->set_is_pack_expansion(
        source_positions[index]->get_is_pack_expansion());
    cloned_position->set_source_type_qualification_present(
        source_positions[index]->get_source_type_qualification_present());
    cloned_position->set_source_type_global_qualification(
        source_positions[index]->get_source_type_global_qualification());
    cloned_position->get_source_type_qualification_tokens() =
        source_positions[index]->get_source_type_qualification_tokens();
    cloned_position->set_source_type_elaboration_required(
        source_positions[index]->get_source_type_elaboration_required());
  }
  return clone;
}

SgMemberFunctionType *buildMemberFunctionTypeWithClonedArguments(
    SgFunctionType *source, SgType *class_type, unsigned int mfunc_specifier,
    const char *context) {
  ASSERT_not_null(source);
  ASSERT_not_null(class_type);
  SgFunctionParameterTypeList *cloned_arguments =
      cloneFunctionArgumentTypes(source, context);
  std::vector<bool> argument_pack_expansions;
  argument_pack_expansions.reserve(
      cloned_arguments->get_argument_qualification_use_sites().size());
  for (SgFunctionTypeArgument *position :
       cloned_arguments->get_argument_qualification_use_sites()) {
    ASSERT_not_null(position);
    argument_pack_expansions.push_back(position->get_is_pack_expansion());
  }
  SgMemberFunctionType *member_type = SageBuilder::buildMemberFunctionType(
      source->get_return_type(), cloned_arguments, class_type, mfunc_specifier);
  ASSERT_not_null(member_type);
  if (member_type->get_argument_list() != cloned_arguments) {
    if (cloned_arguments->get_parent() != nullptr) {
      std::cerr
          << "REX_FRONTEND_INVARIANT[function-argument-qualification-use-"
             "site]: unconsumed cloned member-function argument list acquired "
             "an owner"
          << std::endl;
      ROSE_ABORT();
    }
    SageInterface::deleteAST(
        cloned_arguments,
        SageInterface::DeleteAstMode::kSkipExternalReferences);
  }
  requireExactFunctionArgumentQualificationUseSites(member_type, context);
  const SgFunctionTypeArgumentPtrList &published_positions =
      member_type->get_argument_list()->get_argument_qualification_use_sites();
  if (published_positions.size() != argument_pack_expansions.size()) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[function-argument-pack-surface]: "
            "interned member-function type has %zu argument positions but "
            "producer supplied %zu\n",
            published_positions.size(), argument_pack_expansions.size());
    ROSE_ABORT();
  }
  for (size_t index = 0; index < published_positions.size(); ++index) {
    SgFunctionTypeArgument *published_position = published_positions[index];
    ASSERT_not_null(published_position);
    if (published_position->get_is_pack_expansion() &&
        !argument_pack_expansions[index]) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[function-argument-pack-surface]: "
              "interned member-function argument %zu owns an ellipsis but "
              "the exact producer does not\n",
              index);
      ROSE_ABORT();
    }
    published_position->set_is_pack_expansion(argument_pack_expansions[index]);
  }
  return member_type;
}

SgFunctionType *buildFunctionTypeForClangProto(
    SgType *return_type, SgFunctionParameterTypeList *param_type_list,
    const clang::FunctionProtoType *proto_type, SgType *class_type = nullptr,
    bool exact_source_surface = false) {
  ASSERT_not_null(return_type);
  ASSERT_not_null(param_type_list);
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
  std::vector<bool> argument_pack_expansions;
  argument_pack_expansions.reserve(
      param_type_list->get_argument_qualification_use_sites().size());
  for (SgFunctionTypeArgument *position :
       param_type_list->get_argument_qualification_use_sites()) {
    ASSERT_not_null(position);
    argument_pack_expansions.push_back(position->get_is_pack_expansion());
  }
  const unsigned int mfunc_specifier =
      roseMemberFunctionSpecifierFromClangProto(proto_type);
  SgFunctionType *function_type = nullptr;
  if (exact_source_surface) {
    function_type =
        mfunc_specifier == 0
            ? static_cast<SgFunctionType *>(
                  new SgFunctionType(return_type, false))
            : static_cast<SgFunctionType *>(new SgMemberFunctionType(
                  return_type, false, class_type, mfunc_specifier));
    ASSERT_not_null(function_type);
    SgFunctionParameterTypeList *default_arguments =
        function_type->get_argument_list();
    if (default_arguments == nullptr ||
        default_arguments->get_parent() != function_type ||
        param_type_list->get_parent() != nullptr) {
      std::cerr
          << "REX_FRONTEND_INVARIANT[function-type-source-surface]: exact "
             "source function type has malformed argument-list ownership"
          << std::endl;
      ROSE_ABORT();
    }
    function_type->set_argument_list(param_type_list);
    param_type_list->set_parent(function_type);
    default_arguments->set_parent(nullptr);
    SageInterface::deleteAST(
        default_arguments,
        SageInterface::DeleteAstMode::kSkipExternalReferences);
  } else {
    function_type =
        mfunc_specifier == 0
            ? SageBuilder::buildFunctionType(return_type, param_type_list)
            : SageBuilder::buildMemberFunctionType(return_type, param_type_list,
                                                   class_type, mfunc_specifier);
  }
  ROSE_ASSERT(function_type != nullptr);
  if (proto_type != nullptr && proto_type->isVariadic()) {
    function_type->set_has_ellipses(1);
  }
  if (!exact_source_surface &&
      function_type->get_argument_list() != param_type_list) {
    if (param_type_list->get_parent() != nullptr) {
      std::cerr
          << "REX_FRONTEND_INVARIANT[function-argument-qualification-use-"
             "site]: unconsumed function-prototype argument list acquired an "
             "owner"
          << std::endl;
      ROSE_ABORT();
    }
    SageInterface::deleteAST(
        param_type_list, SageInterface::DeleteAstMode::kSkipExternalReferences);
  }
  requireExactFunctionArgumentQualificationUseSites(function_type,
                                                    "clang-function-prototype");
  const SgFunctionTypeArgumentPtrList &published_positions =
      function_type->get_argument_list()
          ->get_argument_qualification_use_sites();
  if (published_positions.size() != argument_pack_expansions.size()) {
    std::cerr << "REX_FRONTEND_INVARIANT[function-argument-pack-surface]: "
              << "interned function type has " << published_positions.size()
              << " argument positions but producer supplied "
              << argument_pack_expansions.size() << std::endl;
    ROSE_ABORT();
  }
  for (size_t index = 0; index < published_positions.size(); ++index) {
    SgFunctionTypeArgument *published_position = published_positions[index];
    ASSERT_not_null(published_position);
    if (published_position->get_is_pack_expansion() &&
        !argument_pack_expansions[index]) {
      std::cerr << "REX_FRONTEND_INVARIANT[function-argument-pack-surface]: "
                << "interned function argument " << index
                << " owns an ellipsis but the exact producer does not"
                << std::endl;
      ROSE_ABORT();
    }
    published_position->set_is_pack_expansion(argument_pack_expansions[index]);
  }
  return function_type;
}

bool nestedNameSpecifierHasGlobal(clang::NestedNameSpecifier qualifier) {
  qualifier = markClangNestedNameSpecifierDefined(qualifier);
  for (clang::NestedNameSpecifier nns = qualifier; nns;
       nns = markClangNestedNameSpecifierDefined(readClangApiValueDefined(
           [&]() { return nestedNameSpecifierPrefix(nns); }))) {
    nns = markClangNestedNameSpecifierDefined(nns);
    if (readClangApiValueDefined([&]() { return nns.getKind(); }) ==
        clang::NestedNameSpecifier::Kind::Global) {
      return true;
    }
  }
  return false;
}

bool nestedNameSpecifierHasTypeQualifier(clang::NestedNameSpecifier qualifier) {
  qualifier = markClangNestedNameSpecifierDefined(qualifier);
  for (clang::NestedNameSpecifier nns = qualifier; nns;
       nns = markClangNestedNameSpecifierDefined(readClangApiValueDefined(
           [&]() { return nestedNameSpecifierPrefix(nns); }))) {
    nns = markClangNestedNameSpecifierDefined(nns);
    if (readClangApiValueDefined([&]() { return nns.getKind(); }) ==
        clang::NestedNameSpecifier::Kind::Type) {
      return true;
    }
  }
  return false;
}

bool nestedNameSpecifierHasDependentTypeQualifier(
    clang::NestedNameSpecifier qualifier) {
  qualifier = markClangNestedNameSpecifierDefined(qualifier);
  for (clang::NestedNameSpecifier nns = qualifier; nns;
       nns = markClangNestedNameSpecifierDefined(readClangApiValueDefined(
           [&]() { return nestedNameSpecifierPrefix(nns); }))) {
    nns = markClangNestedNameSpecifierDefined(nns);
    if (readClangApiValueDefined([&]() { return nns.getKind(); }) !=
        clang::NestedNameSpecifier::Kind::Type) {
      continue;
    }
    const clang::Type *qualifier_type = markClangTypeObjectDefinedByClass(
        readClangApiValueDefined([&]() { return nns.getAsType(); }));
    if (qualifier_type != nullptr && qualifier_type->isDependentType()) {
      return true;
    }
  }
  return false;
}

bool nestedNameSpecifierHasNamespaceQualifier(
    clang::NestedNameSpecifier qualifier) {
  qualifier = markClangNestedNameSpecifierDefined(qualifier);
  for (clang::NestedNameSpecifier nns = qualifier; nns;
       nns = markClangNestedNameSpecifierDefined(readClangApiValueDefined(
           [&]() { return nestedNameSpecifierPrefix(nns); }))) {
    nns = markClangNestedNameSpecifierDefined(nns);
    switch (readClangApiValueDefined([&]() { return nns.getKind(); })) {
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
                                   clang::CompilerInstance *compiler_instance) {
  if (compiler_instance == nullptr || !compiler_instance->hasSourceManager() ||
      decltype_loc.getDecltypeLoc().isInvalid()) {
    return false;
  }

  clang::SourceManager &source_manager = compiler_instance->getSourceManager();
  const clang::LangOptions &lang_opts = compiler_instance->getLangOpts();
  clang::SourceLocation spelling_loc =
      source_manager.getSpellingLoc(decltype_loc.getDecltypeLoc());
  if (spelling_loc.isInvalid()) {
    return false;
  }

  clang::Token token;
  if (clang::Lexer::getRawToken(spelling_loc, token, source_manager, lang_opts,
                                /*IgnoreWhiteSpace=*/true)) {
    return false;
  }

  bool invalid = false;
  const std::string spelling =
      clang::Lexer::getSpelling(token, source_manager, lang_opts, &invalid);
  return !invalid && spelling == "__decltype";
}

bool injectedClassNameTypeLocHasExplicitTemplateArguments(
    const clang::InjectedClassNameTypeLoc &injected_loc,
    clang::CompilerInstance *compiler_instance) {
  if (compiler_instance == nullptr || !compiler_instance->hasSourceManager() ||
      injected_loc.getNameLoc().isInvalid()) {
    return false;
  }

  clang::SourceManager &source_manager = compiler_instance->getSourceManager();
  const clang::LangOptions &lang_opts = compiler_instance->getLangOpts();
  clang::SourceLocation name_loc =
      source_manager.getSpellingLoc(injected_loc.getNameLoc());
  if (name_loc.isInvalid()) {
    return false;
  }

  clang::SourceLocation after_name =
      clang::Lexer::getLocForEndOfToken(name_loc, 0, source_manager, lang_opts);
  if (after_name.isInvalid()) {
    return false;
  }

  clang::Token token;
  if (clang::Lexer::getRawToken(after_name, token, source_manager, lang_opts,
                                /*IgnoreWhiteSpace=*/true)) {
    return false;
  }

  return token.is(clang::tok::less);
}

bool nestedNameSpecifierLocHasExplicitGlobal(
    clang::NestedNameSpecifierLoc qualifier_loc) {
  for (clang::NestedNameSpecifierLoc current =
           markClangNestedNameSpecifierLocDefined(qualifier_loc);
       current; current = [&]() {
         current = markClangNestedNameSpecifierLocDefined(current);
         clang::NestedNameSpecifier current_specifier =
             markClangNestedNameSpecifierDefined(readClangApiValueDefined(
                 [&]() { return current.getNestedNameSpecifier(); }));
         switch (readClangApiValueDefined(
             [&]() { return current_specifier.getKind(); })) {
         case clang::NestedNameSpecifier::Kind::Namespace:
           return markClangNestedNameSpecifierLocDefined(
               readClangApiValueDefined(
                   [&]() { return current.getAsNamespaceAndPrefix().Prefix; }));
         case clang::NestedNameSpecifier::Kind::Type:
           return markClangNestedNameSpecifierLocDefined(
               readClangApiValueDefined([&]() {
                 return readClangTypeLocDefined(
                            [&]() { return current.getAsTypeLoc(); })
                     .getPrefix();
               }));
         case clang::NestedNameSpecifier::Kind::Global:
         case clang::NestedNameSpecifier::Kind::MicrosoftSuper:
         case clang::NestedNameSpecifier::Kind::Null:
           return clang::NestedNameSpecifierLoc();
         }
         return clang::NestedNameSpecifierLoc();
       }()) {
    current = markClangNestedNameSpecifierLocDefined(current);
    clang::NestedNameSpecifier current_specifier =
        markClangNestedNameSpecifierDefined(readClangApiValueDefined(
            [&]() { return current.getNestedNameSpecifier(); }));
    if (readClangApiValueDefined([&]() {
          return current_specifier.getKind();
        }) == clang::NestedNameSpecifier::Kind::Global) {
      return readClangApiValueDefined(
                 [&]() { return current.getLocalBeginLoc(); })
          .isValid();
    }
  }
  return false;
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
  source_reference = llvm::cast<clang::DeclRefExpr>(
      markClangExprObjectDefinedByClass(source_reference));

  clang::NestedNameSpecifierLoc qualifier_loc =
      markClangNestedNameSpecifierLocDefined(readClangApiValueDefined(
          [&]() { return source_reference->getQualifierLoc(); }));
  SourceQualification source_qualification;
  int structural_depth = 0;
  if (qualifier_loc) {
    source_qualification = sourceQualificationFromNestedNameSpecifierLoc(
        qualifier_loc, compiler_instance,
        "semantic-template-declaration-reference");
    clang::NestedNameSpecifier qualifier =
        markClangNestedNameSpecifierDefined(readClangApiValueDefined(
            [&]() { return qualifier_loc.getNestedNameSpecifier(); }));
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
  markClangTypeLocDataDefined(type_loc);
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

  auto expected_keyword_spelling =
      [](clang::ElaboratedTypeKeyword keyword) -> llvm::StringRef {
    switch (keyword) {
    case clang::ElaboratedTypeKeyword::None:
      return {};
    case clang::ElaboratedTypeKeyword::Typename:
      return "typename";
    case clang::ElaboratedTypeKeyword::Class:
      return "class";
    case clang::ElaboratedTypeKeyword::Struct:
      return "struct";
    case clang::ElaboratedTypeKeyword::Union:
      return "union";
    case clang::ElaboratedTypeKeyword::Enum:
      return "enum";
    case clang::ElaboratedTypeKeyword::Interface:
      return "__interface";
    }
    ROSE_ABORT();
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
    bool invalid_spelling = false;
    const std::string spelling = clang::Lexer::getSpelling(
        token, source_manager, language_options, &invalid_spelling);
    if (invalid_spelling) {
      fprintf(stderr, "REX_FRONTEND_INVARIANT[exact-elaborated-type-keyword]: "
                      "exact keyword token has no spelling\n");
      ROSE_ABORT();
    }
    return spelling == expected_keyword_spelling(keyword);
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
       current = readClangTypeLocDefined(
           [&]() { return current.getNextTypeLoc(); })) {
    markClangTypeLocDataDefined(current);
    markClangTypeObjectDefinedByClass(current.getTypePtr());
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
  markClangTypeLocDataDefined(type_loc);
  for (clang::TypeLoc current = type_loc; !current.isNull();
       current = readClangTypeLocDefined(
           [&]() { return current.getNextTypeLoc(); })) {
    markClangTypeLocDataDefined(current);
    markClangTypeObjectDefinedByClass(current.getTypePtr());
    if (auto spec_loc = current.getAs<clang::TemplateSpecializationTypeLoc>()) {
      markClangTypeLocDataDefined(spec_loc);
      markClangTypeObjectDefinedByClass(spec_loc.getTypePtr());
      clang::NestedNameSpecifierLoc qualifier_loc =
          markClangNestedNameSpecifierLocDefined(readClangApiValueDefined(
              [&]() { return spec_loc.getQualifierLoc(); }));
      if (qualifier_loc) {
        return qualifier_loc;
      }
    }
    if (auto dep_name_loc = current.getAs<clang::DependentNameTypeLoc>()) {
      markClangTypeLocDataDefined(dep_name_loc);
      if (clang::NestedNameSpecifierLoc qualifier_loc =
              markClangNestedNameSpecifierLocDefined(readClangApiValueDefined(
                  [&]() { return dep_name_loc.getQualifierLoc(); }))) {
        return qualifier_loc;
      }
    }
    if (auto typedef_loc = current.getAs<clang::TypedefTypeLoc>()) {
      markClangTypeLocDataDefined(typedef_loc);
      if (clang::NestedNameSpecifierLoc qualifier_loc =
              markClangNestedNameSpecifierLocDefined(readClangApiValueDefined(
                  [&]() { return typedef_loc.getQualifierLoc(); }))) {
        return qualifier_loc;
      }
    }
    if (auto using_loc = current.getAs<clang::UsingTypeLoc>()) {
      markClangTypeLocDataDefined(using_loc);
      if (clang::NestedNameSpecifierLoc qualifier_loc =
              markClangNestedNameSpecifierLocDefined(readClangApiValueDefined(
                  [&]() { return using_loc.getQualifierLoc(); }))) {
        return qualifier_loc;
      }
    }
    if (auto unresolved_using_loc =
            current.getAs<clang::UnresolvedUsingTypeLoc>()) {
      markClangTypeLocDataDefined(unresolved_using_loc);
      if (clang::NestedNameSpecifierLoc qualifier_loc =
              markClangNestedNameSpecifierLocDefined(readClangApiValueDefined(
                  [&]() { return unresolved_using_loc.getQualifierLoc(); }))) {
        return qualifier_loc;
      }
    }
    if (auto tag_loc = current.getAs<clang::TagTypeLoc>()) {
      markClangTypeLocDataDefined(tag_loc);
      const clang::TagType *tag_type = llvm::dyn_cast_or_null<clang::TagType>(
          markClangTypeObjectDefinedByClass(tag_loc.getTypePtr()));
      clang::NestedNameSpecifier qualifier =
          tag_type != nullptr ? readClangApiValueDefined(
                                    [&]() { return tag_type->getQualifier(); })
                              : clang::NestedNameSpecifier();
      markClangTagTypeQualifierStorageDefined(tag_type, qualifier);
      markClangNestedNameSpecifierDefined(qualifier);
      if (clang::NestedNameSpecifierLoc qualifier_loc =
              markClangNestedNameSpecifierLocDefined(readClangApiValueDefined(
                  [&]() { return tag_loc.getQualifierLoc(); }))) {
        return qualifier_loc;
      }
    }
    if (auto injected_loc = current.getAs<clang::InjectedClassNameTypeLoc>()) {
      const clang::TagType *tag_type = llvm::dyn_cast_or_null<clang::TagType>(
          markClangTypeObjectDefinedByClass(injected_loc.getTypePtr()));
      clang::NestedNameSpecifier qualifier =
          tag_type != nullptr ? readClangApiValueDefined(
                                    [&]() { return tag_type->getQualifier(); })
                              : clang::NestedNameSpecifier();
      markClangTagTypeQualifierStorageDefined(tag_type, qualifier);
      markClangNestedNameSpecifierDefined(qualifier);
      if (clang::NestedNameSpecifierLoc qualifier_loc =
              injected_loc.getQualifierLoc()) {
        return qualifier_loc;
      }
    }
  }

  return clang::NestedNameSpecifierLoc();
}

bool canCacheTypeTranslation(const clang::Type *candidate) {
  candidate = markClangTypeObjectDefinedByClass(candidate);
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
        llvm::dyn_cast_or_null<clang::TypedefNameDecl>(
            markClangDeclObjectDefinedByKind(typedef_type->getDecl()));
    clang::DeclContext *ctx =
        decl != nullptr
            ? markClangDeclContextObjectDefined(
                  const_cast<clang::DeclContext *>(decl->getDeclContext()))
            : nullptr;
    return !(ctx != nullptr && ctx->isRecord());
  }
  case clang::Type::Using: {
    const clang::UsingType *using_type =
        llvm::cast<clang::UsingType>(candidate);
    const clang::UsingShadowDecl *decl =
        llvm::dyn_cast_or_null<clang::UsingShadowDecl>(
            markClangDeclObjectDefinedByKind(using_type->getDecl()));
    clang::DeclContext *ctx =
        decl != nullptr
            ? markClangDeclContextObjectDefined(
                  const_cast<clang::DeclContext *>(decl->getDeclContext()))
            : nullptr;
    return !(ctx != nullptr && ctx->isRecord());
  }
  default:
    return true;
  }
}

bool requiresSourceOwnedDependentTypeIdentity(
    const clang::QualType &qual_type) {
  const clang::Type *type =
      markClangTypeObjectDefinedByClass(qual_type.getTypePtrOrNull());
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
    if (directive->get_declaration() != nullptr) {
      decl = directive->get_declaration();
    }
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
      nonreal->get_name() != expected_name ||
      nonreal->get_semantic_name() != expected_name ||
      nonreal->get_type() == nullptr ||
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

  if (SgClassDeclaration *class_decl = isSgClassDeclaration(decl)) {
    return isSgTemplateClassDeclaration(
        SageInterface::getTemplateDeclaration(class_decl));
  }

  return nullptr;
}

const clang::TemplateParameterList *
templateParametersForDeclContext(const clang::DeclContext *context) {
  context = markClangDeclContextObjectDefined(context);
  const clang::Decl *decl = llvm::dyn_cast_or_null<clang::Decl>(context);
  decl = markClangDeclObjectDefinedByKind(decl);
  if (decl == nullptr) {
    return nullptr;
  }

  if (const clang::TemplateDecl *template_decl =
          llvm::dyn_cast<clang::TemplateDecl>(decl)) {
    return markClangTemplateParameterListDefined(readClangApiValueDefined(
        [&]() { return template_decl->getTemplateParameters(); }));
  }

  if (const clang::ClassTemplatePartialSpecializationDecl *class_partial =
          llvm::dyn_cast<clang::ClassTemplatePartialSpecializationDecl>(decl)) {
    return markClangTemplateParameterListDefined(readClangApiValueDefined(
        [&]() { return class_partial->getTemplateParameters(); }));
  }

  if (const clang::VarTemplatePartialSpecializationDecl *var_partial =
          llvm::dyn_cast<clang::VarTemplatePartialSpecializationDecl>(decl)) {
    return markClangTemplateParameterListDefined(readClangApiValueDefined(
        [&]() { return var_partial->getTemplateParameters(); }));
  }

  if (const clang::FunctionDecl *function_decl =
          llvm::dyn_cast<clang::FunctionDecl>(decl)) {
    return markClangTemplateParameterListDefined(readClangApiValueDefined(
        [&]() { return function_decl->getDescribedTemplateParams(); }));
  }

  if (const clang::CXXRecordDecl *record_decl =
          llvm::dyn_cast<clang::CXXRecordDecl>(decl)) {
    if (const clang::ClassTemplatePartialSpecializationDecl *partial =
            llvm::dyn_cast<clang::ClassTemplatePartialSpecializationDecl>(
                record_decl)) {
      return markClangTemplateParameterListDefined(readClangApiValueDefined(
          [&]() { return partial->getTemplateParameters(); }));
    }
    if (llvm::isa<clang::ClassTemplateSpecializationDecl>(record_decl)) {
      return nullptr;
    }

    if (const clang::CXXRecordDecl *canonical_record =
            llvm::dyn_cast_or_null<clang::CXXRecordDecl>(
                markClangDeclObjectDefinedByKind(readClangApiValueDefined(
                    [&]() { return record_decl->getCanonicalDecl(); })))) {
      if (const clang::ClassTemplatePartialSpecializationDecl *partial =
              llvm::dyn_cast<clang::ClassTemplatePartialSpecializationDecl>(
                  canonical_record)) {
        return markClangTemplateParameterListDefined(readClangApiValueDefined(
            [&]() { return partial->getTemplateParameters(); }));
      }
    }

    if (const clang::ClassTemplateDecl *described_template =
            llvm::dyn_cast_or_null<clang::ClassTemplateDecl>(
                markClangDeclObjectDefinedByKind(
                    readClangApiValueDefined([&]() {
                      return record_decl->getDescribedClassTemplate();
                    })))) {
      return markClangTemplateParameterListDefined(readClangApiValueDefined(
          [&]() { return described_template->getTemplateParameters(); }));
    }
  }

  return nullptr;
}

std::vector<const clang::TemplateParameterList *>
collectTemplateParameterLevelsFromDeclContext(
    const clang::DeclContext *start_context) {
  std::vector<const clang::TemplateParameterList *> template_levels;
  start_context = markClangDeclContextObjectDefined(start_context);
  if (start_context == nullptr) {
    return template_levels;
  }

  const clang::Decl *decl = llvm::dyn_cast_or_null<clang::Decl>(start_context);
  decl = markClangDeclObjectDefinedByKind(decl);
  unsigned written_outer_template_levels = 0;
  if (const clang::DeclaratorDecl *declarator_decl =
          llvm::dyn_cast_or_null<clang::DeclaratorDecl>(decl)) {
    written_outer_template_levels = readClangApiValueDefined(
        [&]() { return declarator_decl->getNumTemplateParameterLists(); });
    for (unsigned i = 0; i < written_outer_template_levels; ++i) {
      if (const clang::TemplateParameterList *params =
              markClangTemplateParameterListDefined(
                  readClangApiValueDefined([&]() {
                    return declarator_decl->getTemplateParameterList(i);
                  }))) {
        template_levels.push_back(params);
      }
    }
  }

  std::vector<const clang::TemplateParameterList *> contextual_levels;
  for (const clang::DeclContext *ctx = start_context; ctx != nullptr;
       ctx = markClangDeclContextObjectDefined(
           readClangApiValueDefined([&]() { return ctx->getParent(); }))) {
    ctx = markClangDeclContextObjectDefined(ctx);
    if (const clang::TemplateParameterList *params =
            markClangTemplateParameterListDefined(
                templateParametersForDeclContext(ctx))) {
      contextual_levels.push_back(params);
    }
  }

  std::reverse(contextual_levels.begin(), contextual_levels.end());
  if (written_outer_template_levels != 0) {
    size_t drop_count =
        std::min(static_cast<size_t>(written_outer_template_levels),
                 contextual_levels.size());
    contextual_levels.erase(contextual_levels.begin(),
                            contextual_levels.begin() + drop_count);
  }

  template_levels.insert(template_levels.end(), contextual_levels.begin(),
                         contextual_levels.end());
  return template_levels;
}

const clang::NamedDecl *
resolveTemplateParameterFromDeclContext(const clang::DeclContext *start_context,
                                        unsigned depth, unsigned index) {
  start_context = markClangDeclContextObjectDefined(start_context);
  if (start_context == nullptr) {
    return nullptr;
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
    const clang::Decl *start_decl = llvm::dyn_cast_or_null<clang::Decl>(
        markClangDeclContextObjectDefined(start_context));
    const clang::FunctionDecl *start_function =
        llvm::dyn_cast_or_null<clang::FunctionDecl>(
            markClangDeclObjectDefinedByKind(start_decl));
    const clang::DeclContext *lexical_context =
        start_function != nullptr
            ? markClangDeclContextObjectDefined(readClangApiValueDefined(
                  [&]() { return start_function->getLexicalDeclContext(); }))
            : nullptr;
    const clang::DeclContext *semantic_context =
        start_function != nullptr
            ? markClangDeclContextObjectDefined(readClangApiValueDefined(
                  [&]() { return start_function->getDeclContext(); }))
            : nullptr;
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
         ctx = markClangDeclContextObjectDefined(
             readClangApiValueDefined([&]() { return ctx->getParent(); }))) {
      const auto *function = llvm::dyn_cast_or_null<clang::FunctionDecl>(
          markClangDeclObjectDefinedByKind(
              llvm::dyn_cast_or_null<clang::Decl>(ctx)));
      if (function == nullptr) {
        continue;
      }

      llvm::SmallPtrSet<const clang::FunctionDecl *, 2> patterns;
      if (const clang::FunctionDecl *pattern = readClangApiValueDefined(
              [&]() { return function->getTemplateInstantiationPattern(); })) {
        patterns.insert(pattern);
      }
      if (const auto *method = llvm::dyn_cast<clang::CXXMethodDecl>(function)) {
        if (const clang::FunctionDecl *pattern =
                readClangApiValueDefined([&]() {
                  return method->getInstantiatedFromMemberFunction();
                })) {
          patterns.insert(pattern);
        }
      }

      llvm::SmallPtrSet<const clang::TemplateParameterList *, 3>
          pattern_parameter_lists;
      for (const clang::FunctionDecl *pattern : patterns) {
        pattern = llvm::dyn_cast_or_null<clang::FunctionDecl>(
            markClangDeclObjectDefinedByKind(pattern));
        const clang::TemplateParameterList *parameters =
            pattern != nullptr
                ? markClangTemplateParameterListDefined(
                      readClangApiValueDefined([&]() {
                        return pattern->getDescribedTemplateParams();
                      }))
                : nullptr;
        if (parameters != nullptr) {
          pattern_parameter_lists.insert(parameters);
        }
      }
      if (const clang::FunctionTemplateDecl *function_template =
              llvm::dyn_cast_or_null<clang::FunctionTemplateDecl>(
                  markClangDeclObjectDefinedByKind(
                      function->getDescribedFunctionTemplate()))) {
        const clang::FunctionTemplateDecl *source_template =
            clangTemplateInstantiatedFromMemberTemplateDefined(
                const_cast<clang::FunctionTemplateDecl *>(function_template));
        if (source_template != nullptr) {
          const clang::TemplateParameterList *parameters =
              markClangTemplateParameterListDefined(
                  source_template->getTemplateParameters());
          if (parameters != nullptr) {
            pattern_parameter_lists.insert(parameters);
          }
        }
      }

      for (const clang::TemplateParameterList *parameters :
           pattern_parameter_lists) {
        parameters = markClangTemplateParameterListDefined(parameters);
        if (index >= parameters->size()) {
          continue;
        }
        const auto *candidate =
            llvm::dyn_cast_or_null<clang::TemplateTypeParmDecl>(
                markClangDeclObjectDefinedByKind(parameters->getParam(index)));
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
         ctx = markClangDeclContextObjectDefined(
             readClangApiValueDefined([&]() { return ctx->getParent(); }))) {
      const auto *specialization =
          llvm::dyn_cast_or_null<clang::ClassTemplateSpecializationDecl>(
              markClangDeclObjectDefinedByKind(
                  llvm::dyn_cast_or_null<clang::Decl>(ctx)));
      if (specialization == nullptr ||
          llvm::isa<clang::ClassTemplatePartialSpecializationDecl>(
              specialization)) {
        continue;
      }

      const auto instantiated_from = readClangApiValueDefined(
          [&]() { return specialization->getInstantiatedFrom(); });
      const clang::TemplateParameterList *pattern_parameters = nullptr;
      if (const auto *partial =
              instantiated_from.dyn_cast<
                  clang::ClassTemplatePartialSpecializationDecl *>()) {
        pattern_parameters = readClangApiValueDefined(
            [&]() { return partial->getTemplateParameters(); });
      } else if (const auto *primary =
                     instantiated_from.dyn_cast<clang::ClassTemplateDecl *>()) {
        pattern_parameters = readClangApiValueDefined(
            [&]() { return primary->getTemplateParameters(); });
      }
      pattern_parameters =
          markClangTemplateParameterListDefined(pattern_parameters);
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
  params = markClangTemplateParameterListDefined(params);
  if (params == nullptr ||
      index >= readClangApiValueDefined([&]() { return params->size(); })) {
    return nullptr;
  }

  return llvm::dyn_cast_or_null<clang::NamedDecl>(
      markClangDeclObjectDefinedByKind(
          readClangApiValueDefined([&]() { return params->getParam(index); })));
}

std::string resolveTemplateParameterNameFromDeclContext(
    const clang::DeclContext *start_context, unsigned depth, unsigned index) {
  const clang::NamedDecl *named_param =
      resolveTemplateParameterFromDeclContext(start_context, depth, index);
  if (named_param == nullptr) {
    return "";
  }
  return normalizeClangTemplateParamName(readClangApiValueDefined(
      [&]() { return named_param->getNameAsString(); }));
}

std::vector<const clang::NamespaceDecl *>
collectNamespaceContexts(clang::DeclContext *context) {
  std::vector<const clang::NamespaceDecl *> namespaces;
  for (clang::DeclContext *ctx = markClangDeclContextObjectDefined(context);
       ctx != nullptr;
       ctx = markClangDeclContextObjectDefined(
           readClangApiValueDefined([&]() { return ctx->getParent(); }))) {
    const clang::Decl *ctx_decl = clangDeclFromDeclContextDefined(ctx);
    if (const clang::NamespaceDecl *ns = llvm::dyn_cast<clang::NamespaceDecl>(
            markClangDeclObjectDefinedByKind(ctx_decl))) {
      if (!readClangApiValueDefined(
              [&]() { return ns->isAnonymousNamespace(); })) {
        markClangValueDefined(ns);
        namespaces.push_back(ns);
      }
    }
  }
  std::reverse(namespaces.begin(), namespaces.end());
  return namespaces;
}

bool declContextCanUseReachableNamespaceScope(clang::DeclContext *context) {
  bool saw_context = false;
  for (clang::DeclContext *ctx = markClangDeclContextObjectDefined(context);
       ctx != nullptr;
       ctx = markClangDeclContextObjectDefined(
           readClangApiValueDefined([&]() { return ctx->getParent(); }))) {
    saw_context = true;
    const clang::Decl *ctx_decl = clangDeclFromDeclContextDefined(ctx);
    if (ctx_decl == nullptr) {
      return false;
    }
    if (readClangApiValueDefined([&]() { return ctx->isTranslationUnit(); }) ||
        readClangApiValueDefined([&]() { return ctx->isNamespace(); })) {
      continue;
    }
    if (llvm::isa<clang::LinkageSpecDecl>(ctx_decl)) {
      continue;
    }
    return false;
  }
  return saw_context;
}

std::vector<std::string>
collectNamespaceNamesFromScope(SgScopeStatement *scope) {
  std::vector<std::string> names;
  for (SgNode *node = scope; node != nullptr; node = node->get_parent()) {
    if (SgNamespaceDefinitionStatement *ns_def =
            isSgNamespaceDefinitionStatement(node)) {
      SgNamespaceDeclarationStatement *ns_decl =
          ns_def->get_namespaceDeclaration();
      if (ns_decl != nullptr && !ns_decl->get_isUnnamedNamespace()) {
        names.push_back(ns_decl->get_name().getString());
      }
    }
  }
  std::reverse(names.begin(), names.end());
  return names;
}

bool scopeIsWithinNamespaceChain(SgScopeStatement *scope,
                                 clang::DeclContext *context) {
  if (scope == nullptr || context == nullptr) {
    return false;
  }
  std::vector<const clang::NamespaceDecl *> decl_namespaces =
      collectNamespaceContexts(context);
  if (decl_namespaces.empty()) {
    return false;
  }
  std::vector<std::string> scope_names = collectNamespaceNamesFromScope(scope);
  if (scope_names.size() < decl_namespaces.size()) {
    return false;
  }
  for (size_t i = 0; i < decl_namespaces.size(); ++i) {
    const clang::NamespaceDecl *ns = decl_namespaces[i];
    if (ns == nullptr) {
      return false;
    }
    if (scope_names[i] !=
        readClangApiValueDefined([&]() { return ns->getNameAsString(); })) {
      return false;
    }
  }
  return true;
}

std::string normalizeTemplateTypeParamName(std::string name) {
  if (isImplicitAutoPlaceholderTemplateParamName(name)) {
    return "auto";
  }
  return normalizeClangTemplateParamName(name);
}

const SgTemplateParameterPtrList *
templateParametersForSgDeclaration(const SgDeclarationStatement *decl) {
  return templateParametersForSageDeclarationShared(decl);
}

std::string templateParameterNameFromSg(const SgTemplateParameter *param) {
  return templateParameterNameFromSageShared(param,
                                             normalizeTemplateTypeParamName);
}

std::string resolveTemplateParameterNameFromSageScope(SgScopeStatement *scope,
                                                      unsigned depth,
                                                      unsigned index) {
  return resolveTemplateParameterNameFromSageScopeShared(
      scope, depth, index, normalizeTemplateTypeParamName);
}

bool expressionReferencesTemplateParameterPack(const clang::Expr *expr) {
  expr = markClangExprObjectDefinedByClass(expr);
  if (expr == nullptr) {
    return false;
  }

  if (readClangApiValueDefined(
          [&]() { return expr->containsUnexpandedParameterPack(); })) {
    return true;
  }

  const clang::Expr *stripped = markClangExprObjectDefinedByClass(
      readClangApiValueDefined([&]() { return expr->IgnoreParenImpCasts(); }));
  if (const clang::DeclRefExpr *decl_ref =
          llvm::dyn_cast<clang::DeclRefExpr>(stripped)) {
    decl_ref = static_cast<const clang::DeclRefExpr *>(
        markClangStmtObjectDefinedByClass(decl_ref));
    if (const clang::NonTypeTemplateParmDecl *param =
            llvm::dyn_cast<clang::NonTypeTemplateParmDecl>(
                markClangDeclObjectDefinedByKind(decl_ref->getDecl()))) {
      return readClangApiValueDefined(
          [&]() { return param->isParameterPack(); });
    }
  }

  if (const clang::SubstNonTypeTemplateParmExpr *subst =
          llvm::dyn_cast<clang::SubstNonTypeTemplateParmExpr>(stripped)) {
    subst = static_cast<const clang::SubstNonTypeTemplateParmExpr *>(
        markClangStmtObjectDefinedByClass(subst));
    if (const clang::NonTypeTemplateParmDecl *param =
            llvm::dyn_cast_or_null<clang::NonTypeTemplateParmDecl>(
                markClangDeclObjectDefinedByKind(subst->getParameter()))) {
      return readClangApiValueDefined(
          [&]() { return param->isParameterPack(); });
    }
  }

  return false;
}

bool templateArgumentNeedsPackEllipsis(const clang::TemplateArgument &arg) {
  const clang::TemplateArgument &defined_arg =
      markClangTemplateArgumentDefined(arg);
  if (defined_arg.isPackExpansion() ||
      defined_arg.containsUnexpandedParameterPack()) {
    return true;
  }

  switch (defined_arg.getKind()) {
  case clang::TemplateArgument::Type: {
    clang::QualType qt = markClangQualTypeDefined(defined_arg.getAsType());
    return !qt.isNull() && qt->containsUnexpandedParameterPack();
  }

  case clang::TemplateArgument::Expression:
    return expressionReferencesTemplateParameterPack(defined_arg.getAsExpr());

  case clang::TemplateArgument::Declaration: {
    const clang::ValueDecl *value_decl =
        llvm::dyn_cast_or_null<clang::ValueDecl>(
            markClangDeclObjectDefinedByKind(defined_arg.getAsDecl()));
    if (value_decl == nullptr) {
      return false;
    }
    if (const clang::NonTypeTemplateParmDecl *non_type_param =
            llvm::dyn_cast<clang::NonTypeTemplateParmDecl>(value_decl)) {
      return non_type_param->isParameterPack();
    }
    if (const clang::TemplateTypeParmDecl *type_param =
            llvm::dyn_cast<clang::TemplateTypeParmDecl>(value_decl)) {
      return type_param->isParameterPack();
    }
    if (const clang::TemplateTemplateParmDecl *template_param =
            llvm::dyn_cast<clang::TemplateTemplateParmDecl>(value_decl)) {
      return template_param->isParameterPack();
    }
    return false;
  }

  case clang::TemplateArgument::Pack:
    for (const clang::TemplateArgument &pack_arg :
         markClangTemplateArgumentArrayDefined(defined_arg.pack_elements())) {
      if (templateArgumentNeedsPackEllipsis(pack_arg)) {
        return true;
      }
    }
    return false;

  default:
    return false;
  }
}

bool isLikelyPlaceholderTemplateParamName(const std::string &name) {
  if (name.size() < 2 || name[0] != '_') {
    return false;
  }
  bool has_alpha = false;
  for (size_t i = 1; i < name.size(); ++i) {
    const unsigned char ch = static_cast<unsigned char>(name[i]);
    if (!(std::isalnum(ch) || ch == '_')) {
      return false;
    }
    has_alpha = has_alpha || std::isalpha(ch);
  }
  return has_alpha;
}

int templateParamNameQuality(const std::string &raw_name) {
  std::string name = normalizeTemplateTypeParamName(raw_name);
  if (name.empty()) {
    return 0;
  }
  if (isClangSyntheticTemplateParamName(name)) {
    return 1;
  }
  if (isLikelyPlaceholderTemplateParamName(name)) {
    return 2;
  }
  return 3;
}

std::string preferHigherQualityTemplateParamName(const std::string &preferred,
                                                 const std::string &candidate) {
  std::string normalized_candidate = normalizeTemplateTypeParamName(candidate);
  if (templateParamNameQuality(normalized_candidate) >
      templateParamNameQuality(preferred)) {
    return normalized_candidate;
  }
  return normalizeTemplateTypeParamName(preferred);
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
    ns = llvm::dyn_cast_or_null<clang::NamespaceDecl>(
        markClangDeclObjectDefinedByKind(ns));
    if (ns == nullptr || readClangApiValueDefined(
                             [&]() { return ns->isAnonymousNamespace(); })) {
      continue;
    }
    qualifier = markClangNestedNameSpecifierDefined(qualifier);
    markClangValueDefined(ns);
    markClangValueDefined(qualifier);
    qualifier = readClangApiValueDefined([&]() {
      return clang::NestedNameSpecifier(ast_context, ns, qualifier);
    });
    qualifier = markClangNestedNameSpecifierDefined(qualifier);
  }
  return markClangNestedNameSpecifierDefined(qualifier);
}

bool canSynthesizeNamespaceQualifierFromDeclContext(
    const clang::DeclContext *context) {
  context = markClangDeclContextObjectDefined(
      const_cast<clang::DeclContext *>(context));
  return context != nullptr &&
         (readClangApiValueDefined([&]() { return context->isNamespace(); }) ||
          readClangApiValueDefined(
              [&]() { return context->isTranslationUnit(); }));
}

clang::QualType getInjectedClassNameSpecializationType(
    const clang::InjectedClassNameType *injected_class_name_type,
    const clang::ASTContext &ast_context) {
  if (injected_class_name_type == nullptr) {
    return {};
  }

  injected_class_name_type = static_cast<const clang::InjectedClassNameType *>(
      markClangTypeObjectDefinedByClass(injected_class_name_type));
  const clang::CXXRecordDecl *record =
      llvm::dyn_cast_or_null<clang::CXXRecordDecl>(
          markClangDeclObjectDefinedByKind(
              injected_class_name_type->getDecl()));
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
    specialization_type = readClangApiValueDefined([&]() {
      return partial->getCanonicalInjectedSpecializationType(ast_context);
    });
  } else {
    const clang::ClassTemplateDecl *template_decl =
        injected_class_name_type->getTemplateDecl();
    template_decl = llvm::dyn_cast_or_null<clang::ClassTemplateDecl>(
        markClangDeclObjectDefinedByKind(template_decl));
    if (template_decl == nullptr) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[injected-class-name-type]: record=%p "
              "has no exact owning class template\n",
              static_cast<const void *>(record));
      ROSE_ABORT();
    }
    specialization_type = readClangApiValueDefined([&]() {
      return template_decl->getCanonicalInjectedSpecializationType(ast_context);
    });
  }

  specialization_type = markClangQualTypeDefined(specialization_type);
  const clang::Type *specialization = specialization_type.getTypePtrOrNull();
  specialization = markClangTypeObjectDefinedByClass(specialization);
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

bool collectQualifierNamespaces(
    clang::NestedNameSpecifier qualifier,
    std::vector<const clang::NamespaceDecl *> *namespaces) {
  if (namespaces == nullptr) {
    return false;
  }
  namespaces->clear();
  qualifier = markClangNestedNameSpecifierDefined(qualifier);
  for (clang::NestedNameSpecifier nns = qualifier; nns;
       nns = nestedNameSpecifierPrefix(nns)) {
    nns = markClangNestedNameSpecifierDefined(nns);
    switch (nns.getKind()) {
    case clang::NestedNameSpecifier::Kind::Namespace: {
      const clang::NamespaceDecl *ns =
          llvm::dyn_cast_or_null<clang::NamespaceDecl>(
              markClangDeclObjectDefinedByKind(
                  nestedNameSpecifierNamespace(nns)));
      if (ns == nullptr || ns->isAnonymousNamespace()) {
        return false;
      }
      namespaces->push_back(ns);
      break;
    }
    case clang::NestedNameSpecifier::Kind::MicrosoftSuper:
      return false;
    default:
      break;
    }
  }
  std::reverse(namespaces->begin(), namespaces->end());
  return true;
}

bool namespaceChainIsSuffix(
    const std::vector<const clang::NamespaceDecl *> &full_chain,
    const std::vector<const clang::NamespaceDecl *> &suffix_chain) {
  if (suffix_chain.size() > full_chain.size()) {
    return false;
  }
  size_t offset = full_chain.size() - suffix_chain.size();
  for (size_t i = 0; i < suffix_chain.size(); ++i) {
    if (full_chain[offset + i] != suffix_chain[i]) {
      return false;
    }
  }
  return true;
}

clang::NestedNameSpecifier
cloneQualifierWithPrefix(clang::NestedNameSpecifier prefix,
                         clang::NestedNameSpecifier suffix,
                         clang::ASTContext &context) {
  prefix = markClangNestedNameSpecifierDefined(prefix);
  suffix = markClangNestedNameSpecifierDefined(suffix);
  if (!suffix) {
    return prefix;
  }
  std::vector<clang::NestedNameSpecifier> segments;
  for (clang::NestedNameSpecifier nns = suffix; nns;
       nns = nestedNameSpecifierPrefix(nns)) {
    nns = markClangNestedNameSpecifierDefined(nns);
    segments.push_back(nns);
  }
  std::reverse(segments.begin(), segments.end());

  clang::NestedNameSpecifier result = prefix;
  for (clang::NestedNameSpecifier segment : segments) {
    switch (segment.getKind()) {
    case clang::NestedNameSpecifier::Kind::Namespace: {
      const clang::NamespaceBaseDecl *ns =
          llvm::dyn_cast_or_null<clang::NamespaceBaseDecl>(
              markClangDeclObjectDefinedByKind(
                  nestedNameSpecifierNamespaceBase(segment)));
      if (ns == nullptr) {
        break;
      }
      result = markClangNestedNameSpecifierDefined(result);
      result = clang::NestedNameSpecifier(context, ns, result);
      break;
    }
    case clang::NestedNameSpecifier::Kind::Type: {
      const clang::Type *type =
          markClangTypeObjectDefinedByClass(segment.getAsType());
      if (type == nullptr) {
        break;
      }
      result = clang::NestedNameSpecifier(type);
      break;
    }
    case clang::NestedNameSpecifier::Kind::Global:
      result = clang::NestedNameSpecifier::getGlobal();
      break;
    case clang::NestedNameSpecifier::Kind::MicrosoftSuper:
      return suffix;
    case clang::NestedNameSpecifier::Kind::Null:
      break;
    }
  }
  return markClangNestedNameSpecifierDefined(result);
}

clang::NestedNameSpecifier
prependNamespaceQualifiers(clang::NestedNameSpecifier qualifier,
                           clang::DeclContext *decl_context,
                           clang::ASTContext &context) {
  if (!qualifier || decl_context == nullptr) {
    return qualifier;
  }
  if (nestedNameSpecifierHasGlobal(qualifier)) {
    return qualifier;
  }

  std::vector<const clang::NamespaceDecl *> decl_namespaces =
      collectNamespaceContexts(decl_context);
  if (decl_namespaces.empty()) {
    return qualifier;
  }

  std::vector<const clang::NamespaceDecl *> qualifier_namespaces;
  if (!collectQualifierNamespaces(qualifier, &qualifier_namespaces)) {
    return qualifier;
  }

  if (!qualifier_namespaces.empty() &&
      !namespaceChainIsSuffix(decl_namespaces, qualifier_namespaces)) {
    return qualifier;
  }

  size_t missing_count = decl_namespaces.size() - qualifier_namespaces.size();
  if (missing_count == 0) {
    return qualifier;
  }

  clang::NestedNameSpecifier prefix = std::nullopt;
  for (size_t i = 0; i < missing_count; ++i) {
    const clang::NamespaceDecl *ns = decl_namespaces[i];
    ns = llvm::dyn_cast_or_null<clang::NamespaceDecl>(
        markClangDeclObjectDefinedByKind(ns));
    if (ns == nullptr || ns->isAnonymousNamespace()) {
      continue;
    }
    prefix = markClangNestedNameSpecifierDefined(prefix);
    prefix = clang::NestedNameSpecifier(context, ns, prefix);
  }
  if (!prefix) {
    return qualifier;
  }
  return cloneQualifierWithPrefix(prefix, qualifier, context);
}

struct DependentTemplateSpecializationNameInfo {
  clang::NestedNameSpecifier qualifier = std::nullopt;
  std::string base_name;
  bool has_template_keyword = false;
};

// Forward declaration to keep helper ordering simple.
std::string getTemplateNameBase(const clang::TemplateName &tname);

clang::TemplateDecl *
resolveTemplateNameDeclaration(clang::TemplateName template_name) {
  template_name = markClangTemplateNameDefined(template_name);
  for (;;) {
    if (clang::TemplateDecl *declaration = template_name.getAsTemplateDecl()) {
      return const_cast<clang::TemplateDecl *>(
          llvm::dyn_cast_or_null<clang::TemplateDecl>(
              markClangDeclObjectDefinedByKind(declaration)));
    }
    if (const clang::QualifiedTemplateName *qualified =
            template_name.getAsQualifiedTemplateName()) {
      template_name =
          markClangTemplateNameDefined(qualified->getUnderlyingTemplate());
      continue;
    }
    if (const clang::SubstTemplateTemplateParmStorage *substitution =
            template_name.getAsSubstTemplateTemplateParm()) {
      template_name =
          markClangTemplateNameDefined(substitution->getReplacement());
      continue;
    }
    if (clang::UsingShadowDecl *using_shadow =
            template_name.getAsUsingShadowDecl()) {
      using_shadow = const_cast<clang::UsingShadowDecl *>(
          llvm::dyn_cast_or_null<clang::UsingShadowDecl>(
              markClangDeclObjectDefinedByKind(using_shadow)));
      return using_shadow != nullptr
                 ? const_cast<clang::TemplateDecl *>(
                       llvm::dyn_cast_or_null<clang::TemplateDecl>(
                           markClangDeclObjectDefinedByKind(
                               using_shadow->getTargetDecl())))
                 : nullptr;
    }
    return nullptr;
  }
}

DependentTemplateSpecializationNameInfo getDependentTemplateSpecializationName(
    const clang::TemplateName &template_name) {
  DependentTemplateSpecializationNameInfo info;
  if (const clang::QualifiedTemplateName *qualified =
          template_name.getAsQualifiedTemplateName()) {
    info.qualifier = qualified->getQualifier();
    info.base_name = getTemplateNameBase(qualified->getUnderlyingTemplate());
    info.has_template_keyword = qualified->hasTemplateKeyword();
  } else if (const clang::DependentTemplateName *dependent =
                 template_name.getAsDependentTemplateName()) {
    info.qualifier = dependent->getQualifier();
    info.has_template_keyword = dependent->hasTemplateKeyword();
    clang::IdentifierOrOverloadedOperator base = dependent->getName();
    if (const clang::IdentifierInfo *id = base.getIdentifier()) {
      info.base_name = id->getName().str();
    } else {
      info.base_name = buildOverloadedOperatorName(base.getOperator());
    }
  } else {
    info.base_name = getTemplateNameBase(template_name);
  }

  ROSE_ASSERT(!info.base_name.empty());
  return info;
}
// Generate unique name for template declaration with full namespace
// qualification
std::string mangleTemplateName(const clang::TemplateName &tname) {
  clang::TemplateName marked_name = markClangTemplateNameDefined(tname);

  // Get fully qualified name from the underlying TemplateDecl
  if (clang::TemplateDecl *template_decl = marked_name.getAsTemplateDecl()) {
    // Get qualified name from the declaration (includes namespace)
    std::string result = template_decl->getQualifiedNameAsString();
    return result;
  }

  auto qualifierToString =
      [](clang::NestedNameSpecifier qualifier) -> std::string {
    if (!qualifier) {
      return "";
    }
    std::string result;
    llvm::raw_string_ostream stream(result);
    clang::LangOptions opts;
    clang::PrintingPolicy policy(opts);
#if ROSE_USE_VALGRIND
    if (clangFrontendRunningOnValgrind()) {
      VALGRIND_DISABLE_ERROR_REPORTING;
    }
#endif
    qualifier.print(stream, policy);
#if ROSE_USE_VALGRIND
    if (clangFrontendRunningOnValgrind()) {
      VALGRIND_ENABLE_ERROR_REPORTING;
    }
#endif
    stream.flush();
    return result;
  };

  auto appendQualifier = [](std::string qualifier,
                            const std::string &base) -> std::string {
    if (qualifier.empty()) {
      return base;
    }
    if (qualifier.size() < 2 ||
        qualifier.substr(qualifier.size() - 2) != "::") {
      qualifier += "::";
    }
    return qualifier + base;
  };

  if (const clang::QualifiedTemplateName *qtn =
          marked_name.getAsQualifiedTemplateName()) {
    std::string base = getTemplateNameBase(qtn->getUnderlyingTemplate());
    if (base.empty()) {
      std::cerr << "REX_FRONTEND_INVARIANT[template-name]: qualified template "
                   "has no exact base name"
                << std::endl;
      ROSE_ABORT();
    }
    clang::NestedNameSpecifier qualifier = qtn->getQualifier();
    if (qualifier) {
      std::string qualifier_str = qualifierToString(qualifier);
      if (qualifier_str.empty()) {
        std::cerr << "REX_FRONTEND_INVARIANT[template-name]: qualified "
                     "template qualifier has no exact spelling"
                  << std::endl;
        ROSE_ABORT();
      }
      return appendQualifier(qualifier_str, base);
    }
    return base;
  }

  if (const clang::DependentTemplateName *dtn =
          marked_name.getAsDependentTemplateName()) {
    std::string base = getTemplateNameBase(marked_name);
    if (base.empty()) {
      std::cerr << "REX_FRONTEND_INVARIANT[template-name]: dependent template "
                   "has no exact base name"
                << std::endl;
      ROSE_ABORT();
    }
    clang::NestedNameSpecifier qualifier = dtn->getQualifier();
    if (qualifier) {
      std::string qualifier_str = qualifierToString(qualifier);
      if (qualifier_str.empty()) {
        std::cerr << "REX_FRONTEND_INVARIANT[template-name]: dependent "
                     "template qualifier has no exact spelling"
                  << std::endl;
        ROSE_ABORT();
      }
      return appendQualifier(qualifier_str, base);
    }
    return base;
  }

  std::string base = getTemplateNameBase(marked_name);
  if (base.empty()) {
    std::cerr << "REX_FRONTEND_INVARIANT[template-name]: clang template name "
                 "kind "
              << static_cast<int>(marked_name.getKind())
              << " has no exact base name" << std::endl;
    ROSE_ABORT();
  }
  return base;
}

std::string getTemplateNameBase(const clang::TemplateName &tname) {
  clang::TemplateName marked_name = markClangTemplateNameDefined(tname);
  if (clang::TemplateDecl *template_decl = marked_name.getAsTemplateDecl()) {
    template_decl = const_cast<clang::TemplateDecl *>(
        llvm::dyn_cast_or_null<clang::TemplateDecl>(
            markClangDeclObjectDefinedByKind(template_decl)));
    if (template_decl == nullptr) {
      return "";
    }
    if (clang::TemplateTemplateParmDecl *parm =
            llvm::dyn_cast_or_null<clang::TemplateTemplateParmDecl>(
                template_decl)) {
      std::string name = parm->getNameAsString();
      if (!name.empty()) {
        return name;
      }
      std::cerr << "REX_FRONTEND_INVARIANT[template-name]: unnamed template "
                   "template parameter reached a name-bearing type"
                << std::endl;
      ROSE_ABORT();
    }

    std::string name = template_decl->getNameAsString();
    ROSE_ASSERT(!name.empty());
    return name;
  }

  if (const clang::QualifiedTemplateName *qtn =
          marked_name.getAsQualifiedTemplateName()) {
    return getTemplateNameBase(qtn->getUnderlyingTemplate());
  }

  if (const clang::DependentTemplateName *dtn =
          marked_name.getAsDependentTemplateName()) {
    clang::IdentifierOrOverloadedOperator name = dtn->getName();
    if (const clang::IdentifierInfo *id = name.getIdentifier()) {
      return id->getName().str();
    }
    return buildOverloadedOperatorName(name.getOperator());
  }

  if (const clang::SubstTemplateTemplateParmStorage *subst =
          tname.getAsSubstTemplateTemplateParm()) {
    return getTemplateNameBase(subst->getReplacement());
  }

  if (clang::UsingShadowDecl *using_shadow = tname.getAsUsingShadowDecl()) {
    return using_shadow->getNameAsString();
  }

  if (clang::AssumedTemplateStorage *assumed =
          tname.getAsAssumedTemplateName()) {
    clang::DeclarationName decl_name = assumed->getDeclName();
    if (decl_name.isIdentifier()) {
      return decl_name.getAsIdentifierInfo()->getName().str();
    }
    if (clang::OverloadedOperatorKind op = decl_name.getCXXOverloadedOperator();
        op != clang::OO_None) {
      std::string name = "operator";
      name += clang::getOperatorSpelling(op);
      return name;
    }
    return decl_name.getAsString();
  }

  if (clang::OverloadedTemplateStorage *overloaded =
          tname.getAsOverloadedTemplate()) {
    for (clang::NamedDecl *decl : overloaded->decls()) {
      if (decl != nullptr) {
        std::string name = decl->getNameAsString();
        if (!name.empty()) {
          return name;
        }
      }
    }
  }

  if (const clang::SubstTemplateTemplateParmPackStorage *pack =
          tname.getAsSubstTemplateTemplateParmPack()) {
    if (clang::TemplateTemplateParmDecl *parm = pack->getParameterPack()) {
      std::string name = parm->getNameAsString();
      if (!name.empty()) {
        return name;
      }
      std::cerr << "REX_FRONTEND_INVARIANT[template-name]: unnamed template "
                   "template parameter pack reached a name-bearing type"
                << std::endl;
      ROSE_ABORT();
    }
  }

  if (const clang::DeducedTemplateStorage *deduced =
          tname.getAsDeducedTemplateName()) {
    return getTemplateNameBase(deduced->getUnderlying());
  }

  ROSE_ASSERT(!"Unhandled clang::TemplateName kind");
  return "";
}

std::string trimWhitespace(std::string s) {
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

std::vector<std::string>
splitQualifiedNameOutsideTemplates(const std::string &name) {
  std::vector<std::string> components;
  size_t start = 0;
  int depth = 0;
  for (size_t i = 0; i < name.size(); ++i) {
    char c = name[i];
    if (c == '<') {
      ++depth;
    } else if (c == '>') {
      if (depth > 0) {
        --depth;
      }
    } else if (c == ':' && depth == 0 && i + 1 < name.size() &&
               name[i + 1] == ':') {
      components.push_back(name.substr(start, i - start));
      ++i;
      start = i + 1;
    }
  }
  components.push_back(name.substr(start));
  return components;
}

std::string stripTemplateArgs(const std::string &name) {
  std::string trimmed = trimWhitespace(name);
  size_t lt = trimmed.find('<');
  if (lt == std::string::npos) {
    return trimmed;
  }
  return trimWhitespace(trimmed.substr(0, lt));
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
  markClangTypeLocDataDefined(type_loc);
  return sourceElaborationKind(exactWrittenElaboratedKeyword(
      type_loc,
      readClangApiValueDefined([&]() { return type_loc.getSourceRange(); }),
      compiler_instance));
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
  if (type_loc.isNull() || p_compiler_instance == nullptr) {
    fprintf(stderr, "REX_FRONTEND_INVARIANT[type-source-qualification]: exact "
                    "TypeLoc and compiler identity are required\n");
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

void ClangToSageTranslator::linkNonrealTemplateDeclaration(
    SgNonrealDecl *decl, clang::Decl *template_decl, const char *context) {
  template_decl = const_cast<clang::Decl *>(
      markClangDeclObjectDefinedByKind(template_decl));
  if (decl == nullptr || template_decl == nullptr) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[nonreal-template-link]: context=%s "
            "nonreal=%p clang-template=%p cannot be null\n",
            context != nullptr ? context : "<unknown>",
            static_cast<void *>(decl), static_cast<void *>(template_decl));
    ROSE_ABORT();
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
      key = const_cast<clang::Decl *>(markClangDeclObjectDefinedByKind(key));
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
  if (resolved == nullptr) {
    // Only a template family that is not already under construction may be
    // translated on demand. Re-entering an active source template from one of
    // its signature TypeLocs publishes its members under the temporary
    // function-declarator scope and corrupts both ownership and provenance.
    resolved = normalizeNonrealTemplateDeclarationTarget(
        lookupSgDeclarationForClangDecl(template_decl,
                                        /*allow_on_demand=*/true));
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
            "nonreal=%p has no exact translated target for Clang %s "
            "name=%s template=%p canonical=%p in-progress=%d/%d "
            "on-demand=%d/%d mapped=%p/%p\n",
            context != nullptr ? context : "<unknown>",
            static_cast<void *>(decl), template_decl->getDeclKindName(),
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
  SgDeclarationStatement *existing = normalizeNonrealTemplateDeclarationTarget(
      decl->get_templateDeclaration());
  if (existing != nullptr && existing != resolved) {
    if (isExactSimpleNonrealTemplateBase(existing, decl->get_name())) {
      // A qualified dependent template-id is owned at its lexical use site.
      // Until Clang resolves its template family, its typed edge names the
      // simple terminal declaration in the shared qualifier chain.  Replace
      // only that exact provisional identity with the resolved declaration.
      decl->set_templateDeclaration(resolved);
      return;
    }
    SgTemplateInstantiationDecl *resolved_instantiation =
        isSgTemplateInstantiationDecl(resolved);
    SgTemplateInstantiationDecl *existing_instantiation =
        isSgTemplateInstantiationDecl(existing);
    SgTemplateClassDeclaration *existing_template =
        isSgTemplateClassDeclaration(existing);
    SgTemplateClassDeclaration *resolved_template =
        isSgTemplateClassDeclaration(resolved);
    if (resolved_instantiation != nullptr && existing_template != nullptr &&
        normalizeNonrealTemplateDeclarationTarget(
            resolved_instantiation->get_templateDeclaration()) ==
            normalizeNonrealTemplateDeclarationTarget(existing_template)) {
      // The source template is published before Clang materializes the exact
      // specialization. Refine the per-use source node to that specialization;
      // this is one declaration family, not an identity conflict.
      decl->set_templateDeclaration(resolved_instantiation);
      return;
    }
    if (existing_instantiation != nullptr && resolved_template != nullptr &&
        normalizeNonrealTemplateDeclarationTarget(
            existing_instantiation->get_templateDeclaration()) ==
            normalizeNonrealTemplateDeclarationTarget(resolved_template)) {
      // Do not discard an already-published exact specialization when a later
      // producer reports its canonical source template.
      return;
    }
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[nonreal-template-link]: context=%s "
            "nonreal=%p name=%s already links declaration=%p/%s/%s but "
            "Clang %s resolves to declaration=%p/%s/%s\n",
            context != nullptr ? context : "<unknown>",
            static_cast<void *>(decl), decl->get_name().str(),
            static_cast<void *>(decl->get_templateDeclaration()),
            decl->get_templateDeclaration()->class_name().c_str(),
            SageInterface::get_name(decl->get_templateDeclaration()).c_str(),
            template_decl->getDeclKindName(), static_cast<void *>(resolved),
            resolved->class_name().c_str(),
            SageInterface::get_name(resolved).c_str());
    ROSE_ABORT();
  }
  decl->set_templateDeclaration(resolved);
}

std::string ClangToSageTranslator::buildExactTemplateInstantiationName(
    const std::string &base_name,
    llvm::ArrayRef<clang::TemplateArgument> template_arguments,
    const clang::DeclContext *template_parameter_context) const {
  if (p_compiler_instance == nullptr) {
    std::cerr
        << "REX_FRONTEND_INVARIANT[template-instantiation-language-policy]: "
           "exact template name construction requires a compiler instance"
        << std::endl;
    ROSE_ABORT();
  }
  return buildClangTemplateInstantiationNameForFrontend(
      base_name, template_arguments, p_compiler_instance->getLangOpts(),
      template_parameter_context, currentExactTemplateParameterNames());
}

void markClangQualTypeForPrintingDefinedForFrontend(clang::QualType type) {
#if ROSE_USE_VALGRIND
  llvm::SmallPtrSet<const clang::Type *, 32> seen;
  markClangQualTypeForPrintingDefined(type, seen);
#else
  (void)type;
#endif
}

std::string ClangToSageTranslator::getTemplateQualifiedName(
    SgTemplateClassDeclaration *template_decl) {
  std::string template_base_name = template_decl->get_name().getString();
  std::string template_qualified_name = template_base_name;

  SgScopeStatement *decl_scope = template_decl->get_scope();
  while (decl_scope && !isSgGlobal(decl_scope)) {
    if (SgNamespaceDefinitionStatement *ns_def =
            isSgNamespaceDefinitionStatement(decl_scope)) {
      SgNamespaceDeclarationStatement *ns_decl =
          ns_def->get_namespaceDeclaration();
      template_qualified_name =
          ns_decl->get_name().getString() + "::" + template_qualified_name;
    } else if (SgClassDefinition *class_def = isSgClassDefinition(decl_scope)) {
      SgClassDeclaration *class_decl = class_def->get_declaration();
      template_qualified_name =
          class_decl->get_name().getString() + "::" + template_qualified_name;
    }
    decl_scope = decl_scope->get_scope();
  }
  return template_qualified_name;
}

namespace {

ClangTemplateInstantiationCacheKey makeTemplateInstantiationCacheKey(
    const std::string &template_name,
    llvm::ArrayRef<clang::TemplateArgument> arguments,
    const clang::DeclContext *semantic_owner,
    const clang::ASTContext &context) {
  if (template_name.empty()) {
    std::cerr << "REX_FRONTEND_INVARIANT[template-instantiation-identity]: "
                 "cache key has no template name"
              << std::endl;
    ROSE_ABORT();
  }

  semantic_owner = markClangDeclContextObjectDefined(semantic_owner);
  if (semantic_owner == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[template-instantiation-identity]: "
                 "cache key has no exact semantic owner"
              << std::endl;
    ROSE_ABORT();
  }
  clang::DeclContext *canonical_owner =
      const_cast<clang::DeclContext *>(semantic_owner)->getPrimaryContext();
  markClangValueDefined(canonical_owner);
  canonical_owner = markClangDeclContextObjectDefined(canonical_owner);
  if (canonical_owner == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[template-instantiation-identity]: "
                 "cache key semantic owner has no canonical context"
              << std::endl;
    ROSE_ABORT();
  }

  ClangTemplateInstantiationCacheKey key;
  key.template_name = template_name;
  key.semantic_owner = canonical_owner;
  key.arguments.reserve(arguments.size());

  llvm::FoldingSetNodeID profile;
  profile.AddString(template_name);
  profile.AddPointer(canonical_owner);
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

ClangTemplateInstantiationCacheKey
ClangToSageTranslator::buildTemplateInstantiationCacheKey(
    const std::string &template_name,
    const clang::TemplateSpecializationType *spec_type,
    const clang::DeclContext *semantic_owner) {
  spec_type = static_cast<const clang::TemplateSpecializationType *>(
      markClangTypeObjectDefinedByClass(spec_type));
  if (spec_type == nullptr || p_compiler_instance == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[template-instantiation-identity]: "
                 "type cache key requires an exact specialization type and "
                 "compiler instance"
              << std::endl;
    ROSE_ABORT();
  }
  llvm::ArrayRef<clang::TemplateArgument> arguments =
      markClangTemplateArgumentArrayDefined(spec_type->template_arguments());
  return makeTemplateInstantiationCacheKey(
      template_name, arguments, semantic_owner,
      p_compiler_instance->getASTContext());
}

ClangTemplateInstantiationCacheKey
ClangToSageTranslator::buildTemplateInstantiationCacheKey(
    const std::string &template_name, const clang::TemplateArgumentList &args,
    const clang::DeclContext *semantic_owner) {
  if (p_compiler_instance == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[template-instantiation-identity]: "
                 "declaration cache key requires a compiler instance"
              << std::endl;
    ROSE_ABORT();
  }
  const clang::TemplateArgumentList *defined_args =
      markClangTemplateArgumentListDefined(&args);
  if (defined_args == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[template-instantiation-identity]: "
                 "declaration has no template argument list"
              << std::endl;
    ROSE_ABORT();
  }
  return makeTemplateInstantiationCacheKey(
      template_name, defined_args->asArray(), semantic_owner,
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

void publishSemanticAuxiliaryDeclaration(SgDeclarationStatement *declaration,
                                         SgScopeStatement *target_scope,
                                         const char *context) {
  if (declaration == nullptr || target_scope == nullptr) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[semantic-declaration-owner]: context=%s "
            "has a null declaration or scope\n",
            context);
    ROSE_ABORT();
  }

  if (SgAuxiliaryDeclarationList *auxiliary =
          isSgAuxiliaryDeclarationList(declaration->get_parent())) {
    SgScopeStatement *old_scope = isSgScopeStatement(auxiliary->get_parent());
    requireExactAuxiliaryDeclarationOwner(declaration, old_scope, context);
    if (old_scope != target_scope) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[semantic-declaration-owner]: "
              "context=%s declaration=%p auxiliary scope=%p does not match "
              "exact target=%p\n",
              context, static_cast<void *>(declaration),
              static_cast<void *>(old_scope),
              static_cast<void *>(target_scope));
      ROSE_ABORT();
    }
    return;
  } else if (SgScopeStatement *old_scope =
                 isSgScopeStatement(declaration->get_parent())) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[semantic-declaration-owner]: context=%s "
            "declaration=%p already has lexical owner=%p; semantic ownership "
            "must be selected by its builder\n",
            context, static_cast<void *>(declaration),
            static_cast<void *>(old_scope));
    ROSE_ABORT();
  } else if (declaration->get_parent() != nullptr) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[semantic-declaration-owner]: context=%s "
            "declaration=%p type=%s already has a non-auxiliary typed "
            "owner\n",
            context, static_cast<void *>(declaration),
            declaration->class_name().c_str());
    ROSE_ABORT();
  }

  if (declaration->get_parent() == nullptr) {
    declaration->set_scope(target_scope);
    SageBuilder::attachAuxiliaryDeclaration(target_scope, declaration);
  }
  requireExactAuxiliaryDeclarationOwner(declaration, target_scope, context);
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

void diagnose_null_scope(SgDeclarationStatement *decl, const char *context) {
  if (decl == nullptr || decl->get_scope() != nullptr) {
    return;
  }
  fprintf(stderr,
          "REX_FRONTEND_INVARIANT[declaration-scope]: declaration=%p "
          "type=%s has null scope in %s\n",
          static_cast<void *>(decl), decl->class_name().c_str(),
          context != nullptr ? context : "unknown");
  ROSE_ABORT();
}

bool isConcreteClassTemplateSpecialization(
    const clang::RecordDecl *record_decl) {
  return record_decl != nullptr &&
         llvm::isa<clang::ClassTemplateSpecializationDecl>(record_decl) &&
         !llvm::isa<clang::ClassTemplatePartialSpecializationDecl>(record_decl);
}

SgClassDeclaration *
translatedRecordTypeDeclaration(const clang::RecordDecl *record_decl,
                                SgDeclarationStatement *translated_decl) {
  SgClassDeclaration *class_decl = isSgClassDeclaration(translated_decl);
  if (class_decl == nullptr) {
    return nullptr;
  }

  if (!isConcreteClassTemplateSpecialization(record_decl)) {
    if (SgClassDeclaration *first_nondef = isSgClassDeclaration(
            class_decl->get_firstNondefiningDeclaration())) {
      class_decl = first_nondef;
    }
    return class_decl;
  }

  if (SgTemplateInstantiationDecl *inst_decl =
          isSgTemplateInstantiationDecl(class_decl)) {
    return inst_decl;
  }
  if (SgTemplateInstantiationDecl *first_nondef = isSgTemplateInstantiationDecl(
          class_decl->get_firstNondefiningDeclaration())) {
    return first_nondef;
  }
  if (SgTemplateInstantiationDecl *def_decl = isSgTemplateInstantiationDecl(
          class_decl->get_definingDeclaration())) {
    return def_decl;
  }

  return nullptr;
}

} // anonymous namespace

SgType *ClangToSageTranslator::getTypeFromTranslatedRecordDecl(
    clang::RecordDecl *record_decl) {
  record_decl = markClangRecordDeclDefined(record_decl);
  if (record_decl == nullptr) {
    return nullptr;
  }

  clang::RecordDecl *cache_key = record_decl;
  if (clang::RecordDecl *canonical =
          markClangRecordDeclDefined(llvm::dyn_cast<clang::RecordDecl>(
              markClangAstObjectDefined(record_decl->getCanonicalDecl())))) {
    cache_key = canonical;
  }

  auto cache_type = [&](SgDeclarationStatement *translated_decl) -> SgType * {
    SgClassDeclaration *sg_decl =
        translatedRecordTypeDeclaration(record_decl, translated_decl);
    if (sg_decl == nullptr) {
      return nullptr;
    }

    SgType *type = sg_decl->get_type();
    if (type != nullptr) {
      rememberClassTypeFirstSeenState(p_class_type_decl_first_see_in_type, type,
                                      false);
      p_record_decl_type_map[cache_key] = type;
      if (record_decl != cache_key) {
        p_record_decl_type_map[record_decl] = type;
      }
    }
    return type;
  };

  if (auto cached = p_record_decl_type_map.find(cache_key);
      cached != p_record_decl_type_map.end()) {
    return cached->second;
  }
  if (record_decl != cache_key) {
    if (auto cached = p_record_decl_type_map.find(record_decl);
        cached != p_record_decl_type_map.end()) {
      return cached->second;
    }
  }

  if (SgDeclarationStatement *cached_decl = lookupSgDeclarationForClangDecl(
          record_decl, /*allow_on_demand=*/false)) {
    return cache_type(cached_decl);
  }
  return nullptr;
}

bool ClangToSageTranslator::scopeReachableFromCurrentFile(
    SgScopeStatement *candidate) {
  if (candidate == nullptr) {
    return false;
  }

  // During translation the SgSourceFile still owns its pre-translation
  // placeholder global through get_globalScope().  clang_main replaces that
  // edge only after the complete Clang AST has been translated, so consulting
  // it here rejects declarations that are correctly attached to the new AST.
  // The translator-produced global is the sole authoritative in-progress
  // lexical root and must already have exact ownership by this source file.
  if (p_global_scope == nullptr || p_sage_source_file == nullptr ||
      p_global_scope->get_parent() != p_sage_source_file) {
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
  SgGlobal *file_global = p_global_scope;

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

    return true;
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

SgScopeStatement *ClangToSageTranslator::resolveReachableNamespaceScope(
    clang::DeclContext *decl_context) {
  if (decl_context == nullptr) {
    return nullptr;
  }

  SgScopeStatement *reachable_scope = getGlobalScope();
  if (reachable_scope == nullptr) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[translation-global-owner]: namespace "
            "resolution has no authoritative translator global\n");
    ROSE_ABORT();
  }

  std::vector<const clang::NamespaceDecl *> namespaces =
      collectNamespaceContexts(decl_context);
  for (const clang::NamespaceDecl *ns_decl : namespaces) {
    ns_decl = llvm::dyn_cast_or_null<clang::NamespaceDecl>(
        markClangDeclObjectDefinedByKind(ns_decl));
    if (ns_decl == nullptr || readClangApiValueDefined([&]() {
          return ns_decl->isAnonymousNamespace();
        })) {
      continue;
    }

    // Resolve the exact Clang namespace declaration.  Looking up only by name
    // can select a sibling reopening or a namespace symbol owned by another
    // file.  The former loses the source fragment's identity; the latter used
    // to trigger construction of a fragmentless compiler-generated namespace
    // in the current file's lexical declaration list.
    SgNamespaceDeclarationStatement *ns_stmt =
        ensureNamespaceDeclaration(const_cast<clang::NamespaceDecl *>(ns_decl));
    SgNamespaceDefinitionStatement *ns_definition =
        ns_stmt != nullptr ? ns_stmt->get_definition() : nullptr;
    const bool imported_namespace = ns_decl->isFromASTFile();
    const clang::Module *imported_module =
        imported_namespace ? ns_decl->getImportedOwningModule() : nullptr;
    if (ns_stmt == nullptr || ns_definition == nullptr ||
        ns_stmt->get_name().getString() != readClangApiValueDefined([&]() {
          return ns_decl->getNameAsString();
        }) ||
        (imported_namespace && imported_module == nullptr) ||
        !scopeReachableFromCurrentFile(ns_definition)) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[namespace-owner]: exact namespace=%s "
              "has no reachable declaration and definition pair\n",
              readClangApiValueDefined([&]() {
                return ns_decl->getQualifiedNameAsString();
              }).c_str());
      ROSE_ABORT();
    }

    if (imported_namespace) {
      SgAuxiliaryDeclarationList *auxiliary =
          isSgAuxiliaryDeclarationList(ns_stmt->get_parent());
      if (auxiliary == nullptr || auxiliary->get_parent() != reachable_scope ||
          reachable_scope->get_auxiliary_declarations() != auxiliary ||
          ns_stmt->get_scope() != reachable_scope ||
          ns_stmt->has_source_fragments() ||
          std::count(auxiliary->get_declarations().begin(),
                     auxiliary->get_declarations().end(), ns_stmt) != 1) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[namespace-module-owner]: module=%s "
                "namespace=%s lacks exact semantic-only ownership\n",
                imported_module->getFullModuleName(/*AllowStringLiterals=*/true)
                    .c_str(),
                ns_decl->getQualifiedNameAsString().c_str());
        ROSE_ABORT();
      }
      reachable_scope = ns_definition;
      continue;
    }

    if (!ns_stmt->has_source_fragments()) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[namespace-source-fragment]: exact "
              "namespace=%s has no typed source fragment pair\n",
              ns_decl->getQualifiedNameAsString().c_str());
      ROSE_ABORT();
    }
    ns_stmt->validate_source_fragments();

    ensureDeclInScopeChildList(ns_stmt, reachable_scope,
                               "resolveReachableNamespaceScope");
    if (ns_stmt->get_parent() != reachable_scope ||
        !scopeReachableFromCurrentFile(ns_definition)) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[namespace-source-owner]: exact "
              "namespace=%s was not published in its reachable lexical "
              "scope\n",
              readClangApiValueDefined([&]() {
                return ns_decl->getQualifiedNameAsString();
              }).c_str());
      ROSE_ABORT();
    }

    reachable_scope = ns_definition;
  }

  return reachable_scope;
}

SgType *ClangToSageTranslator::buildTypeFromQualifiedType(
    const clang::QualType &qual_type) {
  markClangValueDefined(qual_type);
  markClangTypeObjectDefinedByClass(qual_type.getTypePtrOrNull());
  const bool cache_translation = p_explicit_template_id_type_use_depth == 0 &&
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

  SgNode *tmp_type =
      Traverse(markClangTypeObjectDefinedByClass(qual_type.getTypePtrOrNull()));
  SgType *type = requireExactType(isSgType(tmp_type), "qualified-type",
                                  qual_type.getTypePtrOrNull());

  // Prefer the exact translated specialization declaration only when the
  // source type itself denotes that specialization. A typedef, using-alias, or
  // alias-template specialization can have the same canonical RecordType;
  // replacing its exact translated type here discards source identity such as
  // `map_type::value_type` or `alias<T>`.
  const clang::Type *written_type =
      markClangTypeObjectDefinedByClass(qual_type.getTypePtrOrNull());
  const clang::TemplateSpecializationType *written_specialization =
      llvm::dyn_cast_or_null<clang::TemplateSpecializationType>(written_type);
  const bool source_denotes_concrete_specialization =
      llvm::isa_and_nonnull<clang::RecordType>(written_type) ||
      (written_specialization != nullptr &&
       !written_specialization->isTypeAlias());
  clang::QualType canonical = qual_type.getCanonicalType();
  markClangValueDefined(canonical);
  if (p_explicit_template_id_type_use_depth == 0 &&
      source_denotes_concrete_specialization && !canonical.isNull()) {
    markClangTypeObjectDefinedByClass(canonical.getTypePtrOrNull());
    if (const clang::RecordType *record_type =
            canonical->getAs<clang::RecordType>()) {
      record_type = static_cast<const clang::RecordType *>(
          markClangTypeObjectDefinedByClass(record_type));
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

    SgModifierType *modified_type = new SgModifierType(type);
    SgTypeModifier &sg_modifer = modified_type->get_typeModifier();

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
    modified_type =
        SgModifierType::insertModifierTypeIntoTypeTable(modified_type);
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
  markClangTypeLocDataDefined(type_loc);
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
    unsigned &preserve_omitted_injected_class_template_id_depth;
    const char *context;

    ExplicitTemplateIdTypeLocUseGuard(
        unsigned &explicit_type_use_depth,
        unsigned &explicit_type_loc_use_depth,
        unsigned &written_specialization_depth,
        unsigned &preserve_omitted_injected_class_template_id_depth,
        const char *context)
        : explicit_type_use_depth(explicit_type_use_depth),
          explicit_type_loc_use_depth(explicit_type_loc_use_depth),
          written_specialization_depth(written_specialization_depth),
          preserve_omitted_injected_class_template_id_depth(
              preserve_omitted_injected_class_template_id_depth),
          context(context) {
      ++explicit_type_use_depth;
      ++explicit_type_loc_use_depth;
      ++written_specialization_depth;
      ++preserve_omitted_injected_class_template_id_depth;
    }

    ~ExplicitTemplateIdTypeLocUseGuard() {
      if (explicit_type_use_depth == 0 || explicit_type_loc_use_depth == 0 ||
          written_specialization_depth == 0 ||
          preserve_omitted_injected_class_template_id_depth == 0) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[explicit-template-id-type-use]: "
                "context=%s lost its exact TypeLoc construction depth\n",
                context != nullptr ? context : "<null>");
        ROSE_ABORT();
      }
      --preserve_omitted_injected_class_template_id_depth;
      --written_specialization_depth;
      --explicit_type_loc_use_depth;
      --explicit_type_use_depth;
    }
  } explicit_type_use(p_explicit_template_id_type_use_depth,
                      p_explicit_template_id_type_loc_use_depth,
                      p_force_written_template_specialization_depth,
                      p_preserve_omitted_injected_class_template_id_depth,
                      context);
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
ClangToSageTranslator::buildTypeFromTypeLoc(const clang::TypeLoc &type_loc) {
  SgType *result = buildTypeFromTypeLocImpl(type_loc);
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

  SgType *result = buildTypeFromTypeLoc(type_loc);
  if (result == nullptr) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[type-loc-semantic-owner]: context=%s "
            "TypeLoc produced no exact Sage type\n",
            context);
    ROSE_ABORT();
  }
  return result;
}

SgType *ClangToSageTranslator::buildTypeFromTypeLocImpl(
    const clang::TypeLoc &type_loc) {
  markClangTypeLocDataDefined(type_loc);
  if (type_loc.isNull()) {
    return nullptr;
  }
  markClangTypeObjectDefinedByClass(type_loc.getTypePtr());

  if (clang::SubstTemplateTypeParmTypeLoc substituted_loc =
          type_loc.getAs<clang::SubstTemplateTypeParmTypeLoc>()) {
    const clang::SubstTemplateTypeParmType *substituted_type =
        substituted_loc.getTypePtr();
    const clang::TemplateTypeParmDecl *replaced_parameter =
        substituted_type != nullptr
            ? llvm::dyn_cast_or_null<clang::TemplateTypeParmDecl>(
                  markClangDeclObjectDefinedByKind(
                      substituted_type->getReplacedParameter()))
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
      const clang::TemplateArgument &argument =
          markClangTemplateArgumentDefined(argument_loc.getArgument());
      const clang::TypeSourceInfo *type_info =
          markClangAstObjectDefined(readClangApiValueDefined(
              [&]() { return argument_loc.getTypeSourceInfo(); }));
      if (argument.getKind() != clang::TemplateArgument::Type ||
          type_info == nullptr) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[explicit-instantiation-type-"
                "substitution]: template type parameter=%p has no exact "
                "written type argument\n",
                static_cast<const void *>(replaced_parameter));
        ROSE_ABORT();
      }

      clang::TypeLoc argument_type_loc =
          readClangApiValueDefined([&]() { return type_info->getTypeLoc(); });
      markClangTypeLocDataDefined(argument_type_loc);
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
    markClangValueDefined(qual_type);
    if (base_type == nullptr || !qual_type.hasLocalQualifiers()) {
      return base_type;
    }

    SgModifierType *modified_type = new SgModifierType(base_type);
    SgTypeModifier &modifier = modified_type->get_typeModifier();
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

    return SgModifierType::insertModifierTypeIntoTypeTable(modified_type);
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

    clang::QualType qualifier_qual_type(
        markClangTypeObjectDefinedByClass(qualifier.getAsType()), 0);
    markClangValueDefined(qualifier_qual_type);
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

  auto append_default_args_from_template_decl =
      [&](const std::string &base_name, SgScopeStatement *scope,
          SgTemplateArgumentPtrList &args) {
        if (scope == nullptr || base_name.empty()) {
          return;
        }
        SgTemplateClassDeclaration *tmpl_decl = nullptr;
        if (SgTemplateClassSymbol *tmpl_sym =
                scope->lookup_template_class_symbol(SgName(base_name), nullptr,
                                                    nullptr)) {
          tmpl_decl = isSgTemplateClassDeclaration(tmpl_sym->get_declaration());
        }
        if (tmpl_decl == nullptr) {
          if (SgTemplateClassSymbol *tmpl_sym =
                  SageInterface::lookupTemplateClassSymbolInParentScopes(
                      SgName(base_name), nullptr, nullptr, scope)) {
            tmpl_decl =
                isSgTemplateClassDeclaration(tmpl_sym->get_declaration());
          }
        }
        if (tmpl_decl == nullptr) {
          return;
        }
        SgTemplateParameterPtrList &params =
            tmpl_decl->get_templateParameters();
        if (args.size() >= params.size()) {
          return;
        }
        for (size_t i = args.size(); i < params.size(); ++i) {
          SgTemplateParameter *param = params[i];
          if (param == nullptr) {
            break;
          }
          if (param->get_parameterType() ==
              SgTemplateParameter::type_parameter) {
            if (SgType *def_type = param->get_defaultTypeParameter()) {
              args.push_back(new SgTemplateArgument(def_type, false));
              continue;
            }
            break;
          }
          if (param->get_parameterType() ==
              SgTemplateParameter::nontype_parameter) {
            if (SgExpression *expr = param->get_expression()) {
              SgType *expression_type = expr->get_type();
              if (expression_type == nullptr) {
                fprintf(stderr,
                        "REX_FRONTEND_INVARIANT[default-template-argument]: "
                        "non-type default expression has no exact type\n");
                ROSE_ABORT();
              }
              SgTemplateArgument *argument = new SgTemplateArgument(
                  SgTemplateArgument::nontype_argument,
                  /*isArrayBoundUnknownType=*/false, expression_type, expr,
                  /*templateDeclaration=*/nullptr,
                  /*explicitlySpecified=*/false);
              expr->set_parent(argument);
              args.push_back(argument);
              continue;
            }
            break;
          }
          if (param->get_parameterType() ==
              SgTemplateParameter::template_parameter) {
            if (SgDeclarationStatement *templ_decl =
                    param->get_templateDeclaration()) {
              args.push_back(new SgTemplateArgument(
                  SgTemplateArgument::template_template_argument,
                  /*isArrayBoundUnknownType=*/false, /*type=*/nullptr,
                  /*expression=*/nullptr, /*templateDeclaration=*/templ_decl,
                  /*explicitlySpecified=*/false));
              continue;
            }
            break;
          }
          break;
        }
      };

  auto resolve_template_decl =
      [&](const clang::TemplateName &name) -> clang::TemplateDecl * {
    clang::TemplateName current = markClangTemplateNameDefined(name);
    for (;;) {
      if (clang::TemplateDecl *decl = current.getAsTemplateDecl()) {
        return const_cast<clang::TemplateDecl *>(
            llvm::dyn_cast_or_null<clang::TemplateDecl>(
                markClangDeclObjectDefinedByKind(decl)));
      }
      if (const clang::QualifiedTemplateName *qtn =
              current.getAsQualifiedTemplateName()) {
        clang::TemplateName underlying =
            markClangTemplateNameDefined(qtn->getUnderlyingTemplate());
        if (clang::TemplateDecl *decl = underlying.getAsTemplateDecl()) {
          return const_cast<clang::TemplateDecl *>(
              llvm::dyn_cast_or_null<clang::TemplateDecl>(
                  markClangDeclObjectDefinedByKind(decl)));
        }
        current = underlying;
        continue;
      }
      if (const clang::SubstTemplateTemplateParmStorage *subst =
              current.getAsSubstTemplateTemplateParm()) {
        current = markClangTemplateNameDefined(subst->getReplacement());
        continue;
      }
      if (clang::UsingShadowDecl *using_shadow =
              current.getAsUsingShadowDecl()) {
        using_shadow = const_cast<clang::UsingShadowDecl *>(
            llvm::dyn_cast_or_null<clang::UsingShadowDecl>(
                markClangDeclObjectDefinedByKind(using_shadow)));
        return const_cast<clang::TemplateDecl *>(
            llvm::dyn_cast_or_null<clang::TemplateDecl>(
                markClangDeclObjectDefinedByKind(
                    using_shadow != nullptr ? using_shadow->getTargetDecl()
                                            : nullptr)));
      }
      return nullptr;
    }
  };

  const bool enable_default_template_args = false;

  auto append_default_args_from_clang_template_decl =
      [&](clang::TemplateDecl *tmpl_decl, SgTemplateArgumentPtrList &args) {
        if (tmpl_decl == nullptr) {
          return;
        }
        clang::TemplateParameterList *params =
            tmpl_decl->getTemplateParameters();
        if (params == nullptr) {
          return;
        }
        if (args.size() >= params->size()) {
          return;
        }
        for (unsigned i = args.size(); i < params->size(); ++i) {
          clang::NamedDecl *param = params->getParam(i);
          if (auto *type_param =
                  llvm::dyn_cast_or_null<clang::TemplateTypeParmDecl>(param)) {
            if (type_param->hasDefaultArgument()) {
              appendTemplateArguments(args, type_param->getDefaultArgument(),
                                      false);
              continue;
            }
            break;
          }
          if (auto *non_type_param =
                  llvm::dyn_cast_or_null<clang::NonTypeTemplateParmDecl>(
                      param)) {
            if (non_type_param->hasDefaultArgument()) {
              appendTemplateArguments(
                  args, non_type_param->getDefaultArgument(), false);
              continue;
            }
            break;
          }
          if (auto *tmpl_param =
                  llvm::dyn_cast_or_null<clang::TemplateTemplateParmDecl>(
                      param)) {
            if (tmpl_param->hasDefaultArgument()) {
              appendTemplateArguments(args, tmpl_param->getDefaultArgument(),
                                      false);
              continue;
            }
            break;
          }
          break;
        }
      };

  auto append_nonreal_template_arguments =
      [&](SgTemplateArgumentPtrList &args,
          clang::TemplateSpecializationTypeLoc specialization_loc,
          clang::TemplateDecl *template_decl, const char *context) {
        // SgNonrealDecl owns a semantic template-id identity.  Its argument
        // expressions may originate in a TypeLoc, but they are not lexical
        // children of that use site and must never retain source provenance
        // when adopted by the shared semantic declaration.
        SemanticExpressionConstruction semantic_arguments(
            p_semantic_template_argument_expression_depth, context);
        markClangTypeLocDataDefined(specialization_loc);
        const unsigned argument_count = readClangApiValueDefined(
            [&]() { return specialization_loc.getNumArgs(); });
        for (unsigned index = 0; index < argument_count; ++index) {
          appendTemplateArguments(args, readClangApiValueDefined([&]() {
                                    return specialization_loc.getArgLoc(index);
                                  }),
                                  true);
        }
        if (enable_default_template_args) {
          append_default_args_from_clang_template_decl(template_decl, args);
        }
      };

  auto translated_instantiation_decl_from_type =
      [](SgType *candidate) -> SgTemplateInstantiationDecl * {
    if (candidate == nullptr) {
      return nullptr;
    }

    SgClassType *class_type = isSgClassType(candidate->findBaseType());
    SgClassDeclaration *class_decl = isSgClassDeclaration(
        class_type != nullptr ? class_type->get_declaration() : nullptr);
    if (class_decl == nullptr) {
      return nullptr;
    }

    if (SgTemplateInstantiationDecl *inst_decl =
            isSgTemplateInstantiationDecl(class_decl)) {
      return inst_decl;
    }
    if (SgTemplateInstantiationDecl *first_nondef =
            isSgTemplateInstantiationDecl(
                class_decl->get_firstNondefiningDeclaration())) {
      return first_nondef;
    }
    if (SgTemplateInstantiationDecl *def_decl = isSgTemplateInstantiationDecl(
            class_decl->get_definingDeclaration())) {
      return def_decl;
    }

    return nullptr;
  };

  auto preserve_written_template_specialization_arguments =
      [&](clang::TemplateSpecializationTypeLoc spec_loc,
          SgType *resolved_type) {
        // A ClassTemplateSpecializationDecl instantiated from a primary
        // template retains the primary member's TypeLoc in Clang. That TypeLoc
        // is not a lexical surface of the semantic Sage instantiation body.
        // Publishing it onto the resolved canonical class would, for example,
        // overwrite allocator<int>'s semantic `int` argument with the source
        // argument of an alias used in the primary template. Only source-backed
        // specialization definitions may transfer written argument spelling.
        SgTemplateInstantiationDefn *active_definition = nullptr;
        std::unordered_set<SgNode *> active_owner_chain;
        for (SgNode *owner = SageBuilder::topScopeStack(); owner != nullptr;
             owner = owner->get_parent()) {
          if (!active_owner_chain.insert(owner).second) {
            fprintf(stderr,
                    "REX_FRONTEND_INVARIANT[template-argument-source-surface]: "
                    "active type-construction owner=%p/%s has a parent cycle\n",
                    static_cast<void *>(owner), owner->class_name().c_str());
            ROSE_ABORT();
          }
          if ((active_definition = isSgTemplateInstantiationDefn(owner)) !=
              nullptr) {
            break;
          }
        }
        if (active_definition != nullptr) {
          SgTemplateInstantiationDecl *active_declaration =
              isSgTemplateInstantiationDecl(
                  active_definition->get_declaration());
          if (active_declaration == nullptr ||
              active_definition->get_parent() != active_declaration ||
              active_declaration->get_definition() != active_definition) {
            fprintf(stderr,
                    "REX_FRONTEND_INVARIANT[template-argument-source-surface]: "
                    "active semantic instantiation definition=%p has no exact "
                    "declaration owner\n",
                    static_cast<void *>(active_definition));
            ROSE_ABORT();
          }
          const std::array<Sg_File_Info *, 6> provenance = {
              active_declaration->get_file_info(),
              active_declaration->get_startOfConstruct(),
              active_declaration->get_endOfConstruct(),
              active_definition->get_file_info(),
              active_definition->get_startOfConstruct(),
              active_definition->get_endOfConstruct()};
          const bool semantic_only =
              std::all_of(provenance.begin(), provenance.end(),
                          [](Sg_File_Info *file_info) {
                            return file_info != nullptr &&
                                   file_info->isCompilerGenerated() &&
                                   file_info->isFrontendSpecific();
                          });
          const bool source_backed =
              std::all_of(provenance.begin(), provenance.end(),
                          [](Sg_File_Info *file_info) {
                            return file_info != nullptr &&
                                   !file_info->isCompilerGenerated() &&
                                   !file_info->isFrontendSpecific() &&
                                   file_info->get_physical_file_id() >= 0;
                          });
          if (!semantic_only && !source_backed) {
            fprintf(stderr,
                    "REX_FRONTEND_INVARIANT[template-argument-source-surface]: "
                    "active instantiation=%p name=%s has mixed declaration/"
                    "definition provenance\n",
                    static_cast<void *>(active_declaration),
                    active_declaration->get_name().getString().c_str());
            ROSE_ABORT();
          }
          if (semantic_only) {
            return;
          }
        } else {
          // A TypeLoc reached from an ordinary use of an instantiation owns a
          // per-use spelling graph; it must never publish typedef spelling or
          // explicitness onto the canonical instantiation arguments.  The
          // caller constructs that graph as SgNonrealType.  Only the exact
          // source-backed specialization definition above may own spelling on
          // its declaration arguments.
          return;
        }

        SgTemplateInstantiationDecl *inst_decl =
            translated_instantiation_decl_from_type(resolved_type);
        if (inst_decl == nullptr) {
          return;
        }

        SgTemplateArgumentPtrList written_args;
        const unsigned arg_count = spec_loc.getNumArgs();
        for (unsigned i = 0; i < arg_count; ++i) {
          appendTemplateArguments(written_args, spec_loc.getArgLoc(i), true);
        }
        if (written_args.empty()) {
          return;
        }

        SgTemplateArgumentPtrList &semantic_args =
            inst_decl->get_templateArguments();
        if (semantic_args.empty()) {
          semantic_args = written_args;
          if (inst_decl->get_deducedTemplateArguments().empty()) {
            inst_decl->get_deducedTemplateArguments() =
                cloneTemplateArgumentSurfacePreservingIdentity(written_args);
          }
          SageBuilder::setTemplateArgumentParents(inst_decl);
          return;
        }

        if (semantic_args.size() < written_args.size()) {
          fprintf(stderr,
                  "REX_FRONTEND_INVARIANT[template-argument-source-surface]: "
                  "instantiation=%p name=%s has %zu semantic arguments but "
                  "%zu exact written arguments\n",
                  static_cast<void *>(inst_decl),
                  inst_decl->get_name().getString().c_str(),
                  semantic_args.size(), written_args.size());
          ROSE_ABORT();
        }

        for (size_t index = 0; index < written_args.size(); ++index) {
          SgTemplateArgument *semantic_arg = semantic_args[index];
          SgTemplateArgument *written_arg = written_args[index];
          if (semantic_arg == nullptr || written_arg == nullptr ||
              !SageInterface::templateArgumentEquivalence(semantic_arg,
                                                          written_arg)) {
            fprintf(
                stderr,
                "REX_FRONTEND_INVARIANT[template-argument-source-"
                "surface]: instantiation=%p name=%s argument=%zu exact "
                "written identity disagrees with its semantic argument "
                "semantic=%p/%s semantic-type=%p/%s written=%p/%s "
                "written-type=%p/%s source-type=%p/%s\n",
                static_cast<void *>(inst_decl),
                inst_decl->get_name().getString().c_str(), index,
                static_cast<void *>(semantic_arg),
                semantic_arg != nullptr ? semantic_arg->class_name().c_str()
                                        : "<null>",
                static_cast<void *>(semantic_arg != nullptr
                                        ? semantic_arg->get_type()
                                        : nullptr),
                semantic_arg != nullptr && semantic_arg->get_type() != nullptr
                    ? semantic_arg->get_type()->class_name().c_str()
                    : "<null>",
                static_cast<void *>(written_arg),
                written_arg != nullptr ? written_arg->class_name().c_str()
                                       : "<null>",
                static_cast<void *>(
                    written_arg != nullptr ? written_arg->get_type() : nullptr),
                written_arg != nullptr && written_arg->get_type() != nullptr
                    ? written_arg->get_type()->class_name().c_str()
                    : "<null>",
                static_cast<void *>(written_arg != nullptr
                                        ? written_arg->get_sourceSpelledType()
                                        : nullptr),
                written_arg != nullptr &&
                        written_arg->get_sourceSpelledType() != nullptr
                    ? written_arg->get_sourceSpelledType()->class_name().c_str()
                    : "<null>");
            ROSE_ABORT();
          }

          // A previously source-backed argument means this shared
          // instantiation has already been asked to represent an exact use
          // site.  Accept only the identical source type; contradictory
          // spellings require a distinct use-site node rather than last-writer
          // wins mutation.
          if (SgType *prior_source = semantic_arg->get_sourceSpelledType()) {
            SgType *written_source = written_arg->get_sourceSpelledType();
            if (prior_source != written_source ||
                semantic_arg->get_name_qualification_length() !=
                    written_arg->get_name_qualification_length() ||
                semantic_arg->get_global_qualification_required() !=
                    written_arg->get_global_qualification_required() ||
                semantic_arg->get_type_elaboration_required() !=
                    written_arg->get_type_elaboration_required()) {
              auto source_type_name = [](SgType *type) {
                if (SgTypedefType *typedefType = isSgTypedefType(type)) {
                  SgTypedefDeclaration *declaration =
                      isSgTypedefDeclaration(typedefType->get_declaration());
                  return declaration != nullptr
                             ? declaration->get_name().getString()
                             : std::string("<unnamed-typedef>");
                }
                if (SgClassType *classType = isSgClassType(type)) {
                  SgClassDeclaration *declaration =
                      isSgClassDeclaration(classType->get_declaration());
                  return declaration != nullptr
                             ? declaration->get_name().getString()
                             : std::string("<unnamed-class>");
                }
                return std::string("<unnamed-type>");
              };
              const std::string written_spelling = trimWhitespace(
                  getSourceText(spec_loc.getArgLoc(index).getSourceRange()));
              fprintf(stderr,
                      "REX_FRONTEND_INVARIANT[template-argument-source-"
                      "surface]: instantiation=%p name=%s argument=%zu has "
                      "conflicting exact source spellings: prior=%p/%s name=%s "
                      "qualification=(%d,%d,%d) written=%p/%s name=%s "
                      "qualification=(%d,%d,%d) text='%s'\n",
                      static_cast<void *>(inst_decl),
                      inst_decl->get_name().getString().c_str(), index,
                      static_cast<void *>(prior_source),
                      prior_source->class_name().c_str(),
                      source_type_name(prior_source).c_str(),
                      semantic_arg->get_name_qualification_length(),
                      semantic_arg->get_global_qualification_required(),
                      semantic_arg->get_type_elaboration_required(),
                      static_cast<void *>(written_source),
                      written_source != nullptr
                          ? written_source->class_name().c_str()
                          : "<null>",
                      source_type_name(written_source).c_str(),
                      written_arg->get_name_qualification_length(),
                      written_arg->get_global_qualification_required(),
                      written_arg->get_type_elaboration_required(),
                      written_spelling.c_str());
              ROSE_ABORT();
            }
            semantic_arg->set_explicitlySpecified(true);
            continue;
          }

          // Replace only the explicitly written prefix.  Defaulted semantic
          // arguments after that prefix remain canonical.  The new argument
          // carries the same validated semantic identity plus its exact
          // TemplateArgumentLoc source type and qualification payload.
          semantic_args[index] = written_arg;
        }
        SageBuilder::setTemplateArgumentParents(inst_decl);
      };

  auto resolves_to_primary_template_class_type =
      [&](SgType *candidate) -> bool {
    if (candidate == nullptr ||
        translated_instantiation_decl_from_type(candidate) != nullptr) {
      return false;
    }

    SgClassType *class_type = isSgClassType(candidate->findBaseType());
    SgClassDeclaration *class_decl = isSgClassDeclaration(
        class_type != nullptr ? class_type->get_declaration() : nullptr);
    if (class_decl == nullptr) {
      return false;
    }

    return isSgTemplateClassDeclaration(class_decl) != nullptr ||
           isSgTemplateClassDeclaration(
               class_decl->get_firstNondefiningDeclaration()) != nullptr ||
           isSgTemplateClassDeclaration(
               class_decl->get_definingDeclaration()) != nullptr;
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
    if (translated_decl == nullptr &&
        p_decl_translation_in_progress.find(typedef_decl) ==
            p_decl_translation_in_progress.end() &&
        p_decl_translation_on_demand.find(typedef_decl) ==
            p_decl_translation_on_demand.end()) {
      TraverseOnDemand(typedef_decl);
      translated_decl = lookupSgDeclarationForClangDecl(
          typedef_decl, /*allow_on_demand=*/true);
    }

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

  auto scope_contains_node = [](SgScopeStatement *ancestor,
                                SgScopeStatement *candidate) -> bool {
    for (SgNode *node = candidate; node != nullptr; node = node->get_parent()) {
      if (node == ancestor) {
        return true;
      }
    }
    return false;
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
      if (SgTemplateClassDefinition *def = isSgTemplateClassDefinition(node)) {
        return def;
      }
      if (SgTemplateInstantiationDefn *def =
              isSgTemplateInstantiationDefn(node)) {
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
      if (SgTemplateClassDeclaration *decl =
              isSgTemplateClassDeclaration(node)) {
        if (decl->get_definition() != nullptr) {
          return decl->get_definition();
        }
        if (SgTemplateClassDeclaration *def_decl =
                isSgTemplateClassDeclaration(decl->get_definingDeclaration())) {
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

  auto build_current_instantiation_template_arguments =
      [&](const clang::DeclContext *record_context)
      -> SgTemplateArgumentPtrList {
    SgTemplateArgumentPtrList args;
    const clang::TemplateParameterList *params =
        templateParametersForDeclContext(record_context);
    if (params == nullptr) {
      return args;
    }

    auto translated_template_param =
        [&](const clang::NamedDecl *param_decl) -> SgTemplateParameter * {
      if (param_decl == nullptr) {
        return nullptr;
      }

      auto it = p_decl_translation_map.find(
          const_cast<clang::NamedDecl *>(param_decl));
      if (it != p_decl_translation_map.end()) {
        return isSgTemplateParameter(it->second);
      }

      return isSgTemplateParameter(
          Traverse(const_cast<clang::NamedDecl *>(param_decl)));
    };

    for (unsigned i = 0; i < params->size(); ++i) {
      const clang::NamedDecl *param_decl = params->getParam(i);
      if (param_decl == nullptr) {
        continue;
      }

      if (const auto *type_param =
              llvm::dyn_cast<clang::TemplateTypeParmDecl>(param_decl)) {
        SgTemplateParameter *sg_param = translated_template_param(type_param);
        SgTemplateType *param_type = isSgTemplateType(
            sg_param != nullptr ? sg_param->get_type() : nullptr);
        if (sg_param == nullptr || param_type == nullptr ||
            sg_param->get_parameterType() !=
                SgTemplateParameter::type_parameter ||
            param_type->get_template_parameter() != sg_param ||
            param_type->get_template_parameter_depth() !=
                static_cast<int>(type_param->getDepth()) ||
            param_type->get_template_parameter_position() !=
                static_cast<int>(type_param->getIndex()) ||
            param_type->get_packed() != type_param->isParameterPack()) {
          std::cerr
              << "REX_FRONTEND_INVARIANT[current-instantiation-argument]: "
                 "type template parameter has no exact SAGE type identity"
              << std::endl;
          ROSE_ABORT();
        }

        SgTemplateArgument *arg = new SgTemplateArgument(param_type, false);
        if (type_param->isParameterPack()) {
          // The parameter declaration and this pack-expansion use own distinct
          // ellipses.  Keep the declaration's exact semantic type on the
          // argument and publish an unexpanded spelling surface for this use.
          SgTemplateType *source_type =
              new SgTemplateType(param_type->get_name());
          source_type->set_template_parameter_position(
              param_type->get_template_parameter_position());
          source_type->set_template_parameter_depth(
              param_type->get_template_parameter_depth());
          if (param_type->get_canonical_source_identity().has_value()) {
            source_type->initialize_canonical_source_identity(
                *param_type->get_canonical_source_identity());
          }
          source_type->set_class_type(param_type->get_class_type());
          source_type->set_parent_class_type(
              param_type->get_parent_class_type());
          source_type->set_template_parameter(sg_param);
          source_type->get_tpl_args() = param_type->get_tpl_args();
          source_type->get_part_spec_tpl_args() =
              param_type->get_part_spec_tpl_args();
          source_type->set_packed(false);
          arg->set_sourceSpelledType(source_type);
          arg->set_is_pack_element(true);
        }
        args.push_back(arg);
        continue;
      }

      if (const auto *non_type_param =
              llvm::dyn_cast<clang::NonTypeTemplateParmDecl>(param_decl)) {
        SgTemplateParameter *sg_param =
            translated_template_param(non_type_param);
        SgInitializedName *initialized_name =
            sg_param != nullptr ? sg_param->get_initializedName() : nullptr;
        if (sg_param == nullptr || initialized_name == nullptr ||
            sg_param->get_type() == nullptr) {
          std::cerr
              << "REX_FRONTEND_INVARIANT[current-instantiation-argument]: "
                 "non-type template parameter has no exact SAGE parameter"
              << std::endl;
          ROSE_ABORT();
        }

        SgTemplateParameterVal *arg_expr =
            SageBuilder::buildTemplateParameterVal_nfi(
                non_type_param->getIndex(),
                initialized_name->get_name().getString());
        arg_expr->set_valueType(sg_param->get_type());
        // Current-instantiation arguments are owned by the semantic
        // SgNonrealDecl identity built below.  The template parameter's range
        // is construction evidence, not an independently written token
        // surface for this argument.  Make that role explicit here so the
        // argument can never be source-classified and then repaired when its
        // semantic owner adopts it.
        SemanticExpressionConstruction semantic_argument(
            p_semantic_template_argument_expression_depth,
            "build_current_instantiation_template_arguments");
        publishSemanticExpressionSourceProvenance(
            arg_expr, non_type_param->getSourceRange(),
            "build_current_instantiation_template_arguments");

        SgTemplateArgument *arg = new SgTemplateArgument(
            SgTemplateArgument::nontype_argument,
            /*isArrayBoundUnknownType=*/false, sg_param->get_type(), arg_expr,
            /*templateDeclaration=*/nullptr, /*explicitlySpecified=*/false);
        arg_expr->set_parent(arg);
        arg->set_is_pack_element(non_type_param->isParameterPack());
        args.push_back(arg);
        continue;
      }

      if (const auto *template_param =
              llvm::dyn_cast<clang::TemplateTemplateParmDecl>(param_decl)) {
        SgDeclarationStatement *template_arg_decl = nullptr;
        if (SgTemplateParameter *sg_param =
                translated_template_param(template_param)) {
          template_arg_decl = sg_param->get_templateDeclaration();
        }
        if (template_arg_decl != nullptr) {
          SgTemplateArgument *arg = new SgTemplateArgument(
              SgTemplateArgument::template_template_argument,
              /*isArrayBoundUnknownType=*/false, /*type=*/nullptr,
              /*expression=*/nullptr,
              /*templateDeclaration=*/template_arg_decl,
              /*explicitlySpecified=*/false);
          arg->set_is_pack_element(template_param->isParameterPack());
          args.push_back(arg);
        }
      }
    }

    ensureTemplateArgumentParents(args);
    return args;
  };

  auto nested_name_specifier_loc_prefix =
      [](clang::NestedNameSpecifierLoc qualifier_loc)
      -> clang::NestedNameSpecifierLoc {
    qualifier_loc = markClangNestedNameSpecifierLocDefined(qualifier_loc);
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

  auto build_auto_type_constraint_text =
      [&](clang::AutoTypeLoc auto_loc) -> std::string {
    if (!auto_loc.isConstrained()) {
      return std::string();
    }

    clang::SourceLocation begin = auto_loc.getConceptNameLoc();
    if (clang::NestedNameSpecifierLoc nested =
            auto_loc.getNestedNameSpecifierLoc()) {
      begin = nested.getBeginLoc();
    }

    clang::SourceLocation end = auto_loc.getConceptNameLoc();
    if (auto_loc.hasExplicitTemplateArgs() &&
        auto_loc.getLAngleLoc().isValid() &&
        auto_loc.getRAngleLoc().isValid()) {
      end = auto_loc.getRAngleLoc();
    }

    if (!begin.isValid() || !end.isValid()) {
      std::cerr << "REX_FRONTEND_INVARIANT[constrained-auto-source]: written "
                   "constraint has no exact source range"
                << std::endl;
      ROSE_ABORT();
    }
    const std::string text =
        trimWhitespace(getSourceText(clang::SourceRange(begin, end)));
    if (text.empty()) {
      std::cerr << "REX_FRONTEND_INVARIANT[constrained-auto-source]: written "
                   "constraint has no exact source spelling"
                << std::endl;
      ROSE_ABORT();
    }
    return text;
  };

  auto annotate_auto_type_constraints = [&](SgType *sg_type) {
    if (sg_type == nullptr) {
      return;
    }

    std::vector<SgAutoType *> auto_types;
    auto collect_auto_types = [&](auto &&self, SgType *current) -> void {
      if (current == nullptr) {
        return;
      }
      if (SgAutoType *auto_type = isSgAutoType(current)) {
        auto_types.push_back(auto_type);
        return;
      }
      if (SgModifierType *modifier_type = isSgModifierType(current)) {
        self(self, modifier_type->get_base_type());
        return;
      }
      if (SgPointerType *pointer_type = isSgPointerType(current)) {
        self(self, pointer_type->get_base_type());
        return;
      }
      if (SgPointerMemberType *pointer_member_type =
              isSgPointerMemberType(current)) {
        self(self, pointer_member_type->get_base_type());
        return;
      }
      if (SgReferenceType *reference_type = isSgReferenceType(current)) {
        self(self, reference_type->get_base_type());
        return;
      }
      if (SgRvalueReferenceType *rvalue_reference_type =
              isSgRvalueReferenceType(current)) {
        self(self, rvalue_reference_type->get_base_type());
        return;
      }
      if (SgArrayType *array_type = isSgArrayType(current)) {
        self(self, array_type->get_base_type());
        return;
      }
      if (SgTypedefType *typedef_type = isSgTypedefType(current)) {
        self(self, typedef_type->get_base_type());
        return;
      }
    };

    collect_auto_types(collect_auto_types, sg_type);
    std::vector<clang::AutoTypeLoc> auto_locs;
    for (clang::TypeLoc current_loc = type_loc; !current_loc.isNull();
         current_loc = current_loc.getNextTypeLoc()) {
      if (clang::AutoTypeLoc auto_loc =
              current_loc.getAs<clang::AutoTypeLoc>()) {
        auto_locs.push_back(auto_loc);
      }
    }
    if (auto_types.size() != auto_locs.size()) {
      std::cerr << "REX_FRONTEND_INVARIANT[auto-type-source]: translated type "
                   "and exact TypeLoc contain different auto placeholder "
                   "counts: type="
                << auto_types.size() << " source=" << auto_locs.size()
                << std::endl;
      ROSE_ABORT();
    }
    for (size_t auto_index = 0; auto_index < auto_types.size(); ++auto_index) {
      SgAutoType *auto_type = auto_types[auto_index];
      clang::AutoTypeLoc auto_loc = auto_locs[auto_index];
      const std::string constraint = build_auto_type_constraint_text(auto_loc);
      if (auto_loc.isConstrained() != !constraint.empty()) {
        std::cerr << "REX_FRONTEND_INVARIANT[constrained-auto-source]: "
                     "semantic constraint state does not match source "
                     "spelling"
                  << std::endl;
        ROSE_ABORT();
      }
      auto_type->set_is_constrained(auto_loc.isConstrained());
      auto_type->set_source_constraint_spelling(constraint);
    }
  };

  auto finalize_spelled_type = [&](SgType *candidate) -> SgType * {
    candidate = apply_local_qualifiers(candidate, type_loc.getType());
    annotate_auto_type_constraints(candidate);
    return candidate;
  };

  if (clang::PackExpansionTypeLoc pack_loc =
          type_loc.getAs<clang::PackExpansionTypeLoc>()) {
    clang::TypeLoc pattern_loc =
        readClangApiValueDefined([&]() { return pack_loc.getPatternLoc(); });
    markClangTypeLocDataDefined(pattern_loc);
    const clang::SourceLocation ellipsis_loc =
        readClangApiValueDefined([&]() { return pack_loc.getEllipsisLoc(); });
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
    const clang::TemplateTypeParmDecl *parameter_decl =
        readClangApiValueDefined([&]() { return parameter_loc.getDecl(); });
    std::string name;
    SgTemplateParameter *sage_parameter = nullptr;
    if (parameter_decl != nullptr) {
      name = normalizeTemplateTypeParamName(parameter_decl->getNameAsString());
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
          name = normalizeTemplateTypeParamName(
              active_parameter->getNameAsString());
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
      name =
          normalizeTemplateTypeParamName(active_parameter->getNameAsString());
    }
    if (name.empty()) {
      name = normalizeTemplateTypeParamName(resolveExactTemplateParameterName(
          parameter_type->getDepth(), parameter_type->getIndex()));
    }
    if (name.empty() && sage_parameter != nullptr) {
      if (SgTemplateType *identity_type =
              isSgTemplateType(sage_parameter->get_type())) {
        name = identity_type->get_name().getString();
      }
    }
    if (name.empty()) {
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

    // A template parameter declaration owns pack identity, but a reference to
    // that parameter owns no ellipsis unless an enclosing PackExpansionTypeLoc
    // says so.  Publish a per-use spelling type instead of returning or
    // mutating the declaration's SgTemplateType.
    SgTemplateType *source_type = new SgTemplateType(SgName(name));
    source_type->set_template_parameter_depth(
        static_cast<int>(parameter_type->getDepth()));
    source_type->set_template_parameter_position(
        static_cast<int>(parameter_type->getIndex()));
    SgTemplateType *sage_parameter_type = isSgTemplateType(
        sage_parameter != nullptr ? sage_parameter->get_type() : nullptr);
    if (sage_parameter_type != nullptr &&
        sage_parameter_type->get_canonical_source_identity().has_value()) {
      source_type->initialize_canonical_source_identity(
          *sage_parameter_type->get_canonical_source_identity());
    } else {
      publishCanonicalTemplateParameterSourceIdentity(
          source_type,
          const_cast<clang::TemplateTypeParmDecl *>(parameter_decl),
          "buildTypeFromTypeLoc:template-parameter");
    }
    source_type->set_template_parameter(sage_parameter);
    source_type->set_packed(false);
    if (source_type->get_packed()) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[template-parameter-type-surface]: "
              "new per-use template parameter type retained pack syntax\n");
      ROSE_ABORT();
    }
    return finalize_spelled_type(source_type);
  }

  if (clang::SubstTemplateTypeParmTypeLoc substituted_loc =
          type_loc.getAs<clang::SubstTemplateTypeParmTypeLoc>()) {
    const clang::SubstTemplateTypeParmType *substituted_type =
        substituted_loc.getTypePtr();
    const clang::TemplateTypeParmDecl *parameter_decl =
        substituted_type != nullptr
            ? llvm::dyn_cast_or_null<clang::TemplateTypeParmDecl>(
                  markClangDeclObjectDefinedByKind(
                      substituted_type->getReplacedParameter()))
            : nullptr;
    if (parameter_decl == nullptr) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[substituted-parameter-type-surface]: "
              "SubstTemplateTypeParmTypeLoc has no exact replaced parameter\n");
      ROSE_ABORT();
    }

    std::string name =
        normalizeTemplateTypeParamName(parameter_decl->getNameAsString());
    SgTemplateParameter *sage_parameter = nullptr;
    auto mapped = p_decl_translation_map.find(
        const_cast<clang::TemplateTypeParmDecl *>(parameter_decl));
    if (mapped != p_decl_translation_map.end()) {
      sage_parameter = isSgTemplateParameter(mapped->second);
    }
    if (name.empty() && sage_parameter != nullptr) {
      if (SgTemplateType *identity_type =
              isSgTemplateType(sage_parameter->get_type())) {
        name = identity_type->get_name().getString();
      }
    }
    if (name.empty()) {
      // An unnamed template parameter cannot be referenced in source.  For
      // such a substitution the only possible typed surface is Clang's exact
      // replacement type; treating the anonymous declaration identity as a
      // name would fabricate syntax.
      clang::QualType replacement_type =
          markClangQualTypeDefined(substituted_type->getReplacementType());
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

    SgTemplateType *source_type = new SgTemplateType(SgName(name));
    source_type->set_template_parameter_depth(
        static_cast<int>(parameter_decl->getDepth()));
    source_type->set_template_parameter_position(
        static_cast<int>(parameter_decl->getIndex()));
    publishCanonicalTemplateParameterSourceIdentity(
        source_type, const_cast<clang::TemplateTypeParmDecl *>(parameter_decl),
        "buildTypeFromTypeLoc:substituted-template-parameter");
    source_type->set_template_parameter(sage_parameter);
    source_type->set_packed(false);
    return finalize_spelled_type(source_type);
  }

  if (clang::SubstTemplateTypeParmPackTypeLoc substituted_loc =
          type_loc.getAs<clang::SubstTemplateTypeParmPackTypeLoc>()) {
    const clang::SubstTemplateTypeParmPackType *substituted_type =
        substituted_loc.getTypePtr();
    const clang::TemplateTypeParmDecl *parameter_decl =
        substituted_type != nullptr
            ? llvm::dyn_cast_or_null<clang::TemplateTypeParmDecl>(
                  markClangDeclObjectDefinedByKind(
                      substituted_type->getReplacedParameter()))
            : nullptr;
    if (parameter_decl == nullptr) {
      fprintf(stderr, "REX_FRONTEND_INVARIANT[substituted-pack-type-surface]: "
                      "SubstTemplateTypeParmPackTypeLoc has no exact replaced "
                      "parameter\n");
      ROSE_ABORT();
    }

    std::string name =
        normalizeTemplateTypeParamName(parameter_decl->getNameAsString());
    SgTemplateParameter *sage_parameter = nullptr;
    auto mapped = p_decl_translation_map.find(
        const_cast<clang::TemplateTypeParmDecl *>(parameter_decl));
    if (mapped != p_decl_translation_map.end()) {
      sage_parameter = isSgTemplateParameter(mapped->second);
    }
    if (name.empty() && sage_parameter != nullptr) {
      if (SgTemplateType *identity_type =
              isSgTemplateType(sage_parameter->get_type())) {
        name = identity_type->get_name().getString();
      }
    }
    if (name.empty()) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[substituted-pack-type-surface]: "
              "replaced parameter depth=%u index=%u has no exact written "
              "name\n",
              parameter_decl->getDepth(), parameter_decl->getIndex());
      ROSE_ABORT();
    }

    SgTemplateType *source_type = new SgTemplateType(SgName(name));
    source_type->set_template_parameter_depth(
        static_cast<int>(parameter_decl->getDepth()));
    source_type->set_template_parameter_position(
        static_cast<int>(parameter_decl->getIndex()));
    publishCanonicalTemplateParameterSourceIdentity(
        source_type, const_cast<clang::TemplateTypeParmDecl *>(parameter_decl),
        "buildTypeFromTypeLoc:substituted-template-parameter-pack");
    source_type->set_template_parameter(sage_parameter);
    source_type->set_packed(false);
    return finalize_spelled_type(source_type);
  }

  auto classify_tag_decl_spelled_in_type_loc =
      [&](clang::TypeLoc spelled_type_loc) -> bool {
    markClangTypeLocDataDefined(spelled_type_loc);
    if (spelled_type_loc.isNull() || p_compiler_instance == nullptr) {
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

    clang::SourceRange type_range = readClangApiValueDefined(
        [&]() { return spelled_type_loc.getSourceRange(); });
    clang::SourceLocation range_begin = file_loc(type_range.getBegin());
    clang::SourceLocation range_end = file_loc(type_range.getEnd());
    if (!range_begin.isValid() || !range_end.isValid()) {
      return false;
    }

    clang::QualType current_qual_type = markClangQualTypeDefined(
        readClangApiValueDefined([&]() { return spelled_type_loc.getType(); }));
    const clang::Type *current_type = current_qual_type.getTypePtrOrNull();
    clang::TagDecl *tag_decl = nullptr;

    // Type objects name a canonical semantic tag and can therefore lose the
    // exact redeclaration that supplied this TypeLoc's source spelling.  The
    // TypeLoc is the authority for declarator ownership: select its concrete
    // TagTypeLoc declaration before consulting the shared semantic type.
    for (clang::TypeLoc current = spelled_type_loc; !current.isNull();
         current = current.getNextTypeLoc()) {
      markClangTypeLocDataDefined(current);
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

    while (tag_decl == nullptr && current_type != nullptr) {
      if (const auto *tag_type = llvm::dyn_cast<clang::TagType>(current_type)) {
        tag_decl = tag_type->getDecl();
        break;
      }
      if (const auto *injected =
              llvm::dyn_cast<clang::InjectedClassNameType>(current_type)) {
        tag_decl = injected->getDecl();
        break;
      }
      if (const auto *record_type =
              llvm::dyn_cast<clang::RecordType>(current_type)) {
        tag_decl = record_type->getDecl();
        break;
      }
      clang::QualType next_qual_type;
      if (const auto *paren_type =
              llvm::dyn_cast<clang::ParenType>(current_type)) {
        next_qual_type = paren_type->getInnerType();
      } else if (const auto *pointer_type =
                     llvm::dyn_cast<clang::PointerType>(current_type)) {
        next_qual_type = pointer_type->getPointeeType();
      } else if (const auto *reference_type =
                     llvm::dyn_cast<clang::ReferenceType>(current_type)) {
        next_qual_type = reference_type->getPointeeType();
      } else if (const auto *array_type =
                     llvm::dyn_cast<clang::ArrayType>(current_type)) {
        next_qual_type = array_type->getElementType();
      } else if (const auto *attributed_type =
                     llvm::dyn_cast<clang::AttributedType>(current_type)) {
        next_qual_type = attributed_type->getModifiedType();
      } else if (const auto *adjusted_type =
                     llvm::dyn_cast<clang::AdjustedType>(current_type)) {
        next_qual_type = adjusted_type->getOriginalType();
      } else if (qualifiedTypeHasQualifier(current_type)) {
        next_qual_type = current_qual_type.getCanonicalType();
      } else {
        break;
      }

      const clang::Type *next_type = next_qual_type.getTypePtrOrNull();
      if (next_type == nullptr || next_type == current_type) {
        break;
      }
      current_qual_type = next_qual_type;
      current_type = next_type;
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
    if (SgType *inner_type =
            buildTypeFromTypeLoc(qualified_loc.getUnqualifiedLoc())) {
      return finalize_spelled_type(inner_type);
    }
  }

  if (auto paren_loc = type_loc.getAs<clang::ParenTypeLoc>()) {
    if (SgType *inner_type = buildTypeFromTypeLoc(paren_loc.getInnerLoc())) {
      inner_type = apply_local_qualifiers(inner_type, type_loc.getType());
      annotate_auto_type_constraints(inner_type);
      return inner_type;
    }
  }

  if (clang::AutoTypeLoc auto_loc = type_loc.getAs<clang::AutoTypeLoc>()) {
    SgAutoType *source_auto = SageBuilder::buildAutoType();
    const std::string constraint = build_auto_type_constraint_text(auto_loc);
    if (auto_loc.isConstrained() != !constraint.empty()) {
      std::cerr << "REX_FRONTEND_INVARIANT[constrained-auto-source]: semantic "
                   "constraint state does not match source spelling"
                << std::endl;
      ROSE_ABORT();
    }
    source_auto->set_is_constrained(auto_loc.isConstrained());
    source_auto->set_source_constraint_spelling(constraint);
    return finalize_spelled_type(source_auto);
  }

  if (auto decltype_loc = type_loc.getAs<clang::DecltypeTypeLoc>()) {
    const clang::DecltypeType *decltype_type = decltype_loc.getTypePtr();
    if (decltype_type != nullptr) {
      const bool is_gnu_decltype =
          decltypeTypeLocUsesGNUKeyword(decltype_loc, p_compiler_instance);
      SgType *sg_underlying_type = nullptr;
      clang::QualType underlying_type = decltype_type->getUnderlyingType();
      if (!underlying_type.isNull()) {
        sg_underlying_type = buildTypeFromQualifiedType(underlying_type);
      }

      auto decltype_parameter_type_needs_direct_declarator =
          [](SgType *type) -> bool {
        while (type != nullptr) {
          if (SgModifierType *modifier_type = isSgModifierType(type)) {
            type = modifier_type->get_base_type();
            continue;
          }
          if (SgPointerType *pointer_type = isSgPointerType(type)) {
            type = pointer_type->get_base_type();
            continue;
          }
          if (SgReferenceType *reference_type = isSgReferenceType(type)) {
            type = reference_type->get_base_type();
            continue;
          }
          if (SgRvalueReferenceType *reference_type =
                  isSgRvalueReferenceType(type)) {
            type = reference_type->get_base_type();
            continue;
          }
          if (SgPointerMemberType *member_pointer_type =
                  isSgPointerMemberType(type)) {
            type = member_pointer_type->get_base_type();
            continue;
          }

          return isSgArrayType(type) != nullptr ||
                 isSgFunctionType(type) != nullptr ||
                 isSgMemberFunctionType(type) != nullptr;
        }

        return false;
      };

      const bool is_dependent_decltype =
          decltype_type->isDependentType() ||
          decltype_type->isInstantiationDependentType();
      const bool can_encode_plain_parameter_ref =
          sg_underlying_type != nullptr &&
          isSgAutoType(sg_underlying_type) == nullptr &&
          !is_dependent_decltype &&
          !shouldPreserveDependentDecltypeExpression(decltype_type) &&
          !decltypeNeedsExpressionPreservationForUnnamedType(underlying_type);
      if (can_encode_plain_parameter_ref) {
        if (const clang::Expr *underlying_expr =
                decltype_type->getUnderlyingExpr()) {
          const clang::Expr *stripped_expr =
              underlying_expr->IgnoreParenImpCasts();
          if (const clang::DeclRefExpr *decl_ref =
                  llvm::dyn_cast<clang::DeclRefExpr>(stripped_expr)) {
            if (const clang::ParmVarDecl *param_decl =
                    llvm::dyn_cast<clang::ParmVarDecl>(decl_ref->getDecl())) {
              if (decltype_parameter_type_needs_direct_declarator(
                      sg_underlying_type)) {
                SgType *decl_type = sg_underlying_type;
                decl_type =
                    apply_local_qualifiers(decl_type, type_loc.getType());
                annotate_auto_type_constraints(decl_type);
                return decl_type;
              }

              SgFunctionParameterRefExp *param_ref =
                  SageBuilder::buildFunctionParameterRefExp_nfi(
                      static_cast<int>(param_decl->getFunctionScopeIndex()),
                      static_cast<int>(param_decl->getFunctionScopeDepth()),
                      sg_underlying_type);
              applySourceRange(param_ref, decl_ref->getSourceRange());
              SgType *decl_type =
                  SageBuilder::buildDeclType(param_ref, sg_underlying_type);
              if (SgDeclType *sg_decl_type = isSgDeclType(decl_type)) {
                sg_decl_type->set_is_gnu_decltype(is_gnu_decltype);
              }
              decl_type = apply_local_qualifiers(decl_type, type_loc.getType());
              annotate_auto_type_constraints(decl_type);
              return decl_type;
            }
          }
        }
      }

      if (const clang::Expr *underlying_expr =
              decltype_type->getUnderlyingExpr()) {
        SgNode *expr_node =
            Traverse(const_cast<clang::Expr *>(underlying_expr));
        if (SgExpression *expr = isSgExpression(expr_node)) {
          if (SgExpression *expr_copy = prepareExpressionForAttachment(expr)) {
            SgType *exact_semantic_base = requireExactDecltypeSemanticBase(
                decltype_type, expr_copy, sg_underlying_type,
                "source-dependent-decltype-base");
            SgType *decl_type =
                SageBuilder::buildDeclType(expr_copy, exact_semantic_base);
            if (SgDeclType *sg_decl_type = isSgDeclType(decl_type)) {
              sg_decl_type->set_is_gnu_decltype(is_gnu_decltype);
            }
            decl_type = apply_local_qualifiers(decl_type, type_loc.getType());
            annotate_auto_type_constraints(decl_type);
            return decl_type;
          }
        }
      }
    }
  }

  if (clang::AdjustedTypeLoc adjusted_loc =
          type_loc.getAs<clang::AdjustedTypeLoc>()) {
    clang::TypeLoc original_loc = readClangTypeLocDefined(
        [&]() { return adjusted_loc.getOriginalLoc(); });
    if (original_loc.isNull()) {
      std::cerr << "REX_CFE_TYPE_INVARIANT[adjusted-source-type]: adjusted "
                   "TypeLoc has no exact original source TypeLoc\n";
      ROSE_ABORT();
    }
    (void)classify_tag_decl_spelled_in_type_loc(original_loc);
    SgType *original_type = buildTypeFromTypeLoc(original_loc);
    if (original_type == nullptr) {
      failExactTypeTranslation("adjusted-source-typeloc",
                               adjusted_loc.getTypePtr());
    }
    return finalize_spelled_type(original_type);
  }

  if (auto pointer_loc = type_loc.getAs<clang::PointerTypeLoc>()) {
    clang::TypeLoc pointee_loc =
        readClangApiValueDefined([&]() { return pointer_loc.getPointeeLoc(); });
    markClangTypeLocDataDefined(pointee_loc);
    if (SgType *pointee_type = buildTypeFromTypeLoc(pointee_loc)) {
      clang::QualType pointee_loc_type = markClangQualTypeDefined(
          readClangApiValueDefined([&]() { return pointee_loc.getType(); }));
      const clang::PointerType *pointer_type_ptr =
          llvm::dyn_cast_or_null<clang::PointerType>(
              markClangTypeObjectDefinedByClass(pointer_loc.getTypePtr()));
      if (pointer_type_ptr == nullptr) {
        failExactTypeTranslation("pointer-typeloc", pointer_loc.getTypePtr());
      }
      clang::QualType pointee_type_qt =
          markClangQualTypeDefined(readClangApiValueDefined(
              [&]() { return pointer_type_ptr->getPointeeType(); }));
      pointee_type = apply_exact_pointee_qualifiers(
          pointee_type, pointee_loc_type, pointee_type_qt);
      // TypeLoc translation publishes a per-use spelling tree.  Interning this
      // wrapper can return an older semantic pointer whose child is the
      // declaration-owned template-pack identity rather than this exact
      // unexpanded source type.
      SgType *pointer_type = new SgPointerType(pointee_type);
      pointer_type = apply_local_qualifiers(pointer_type, type_loc.getType());
      annotate_auto_type_constraints(pointer_type);
      return pointer_type;
    }
  }

  if (auto lvalue_ref_loc = type_loc.getAs<clang::LValueReferenceTypeLoc>()) {
    clang::TypeLoc pointee_loc = readClangApiValueDefined(
        [&]() { return lvalue_ref_loc.getPointeeLoc(); });
    markClangTypeLocDataDefined(pointee_loc);
    if (SgType *pointee_type = buildTypeFromTypeLoc(pointee_loc)) {
      clang::QualType pointee_loc_type = markClangQualTypeDefined(
          readClangApiValueDefined([&]() { return pointee_loc.getType(); }));
      const clang::ReferenceType *reference_type_ptr =
          llvm::dyn_cast_or_null<clang::ReferenceType>(
              markClangTypeObjectDefinedByClass(lvalue_ref_loc.getTypePtr()));
      if (reference_type_ptr == nullptr) {
        failExactTypeTranslation("lvalue-reference-typeloc",
                                 lvalue_ref_loc.getTypePtr());
      }
      clang::QualType pointee_type_qt =
          markClangQualTypeDefined(readClangApiValueDefined(
              [&]() { return reference_type_ptr->getPointeeType(); }));
      pointee_type = apply_exact_pointee_qualifiers(
          pointee_type, pointee_loc_type, pointee_type_qt);
      // Keep the exact TypeLoc child; canonical reference interning is a
      // semantic identity operation and must not replace this source surface.
      SgType *reference_type = new SgReferenceType(pointee_type);
      reference_type =
          apply_local_qualifiers(reference_type, type_loc.getType());
      annotate_auto_type_constraints(reference_type);
      return reference_type;
    }
  }

  if (auto rvalue_ref_loc = type_loc.getAs<clang::RValueReferenceTypeLoc>()) {
    clang::TypeLoc pointee_loc = readClangApiValueDefined(
        [&]() { return rvalue_ref_loc.getPointeeLoc(); });
    markClangTypeLocDataDefined(pointee_loc);
    if (SgType *pointee_type = buildTypeFromTypeLoc(pointee_loc)) {
      clang::QualType pointee_loc_type = markClangQualTypeDefined(
          readClangApiValueDefined([&]() { return pointee_loc.getType(); }));
      const clang::ReferenceType *reference_type_ptr =
          llvm::dyn_cast_or_null<clang::ReferenceType>(
              markClangTypeObjectDefinedByClass(rvalue_ref_loc.getTypePtr()));
      if (reference_type_ptr == nullptr) {
        failExactTypeTranslation("rvalue-reference-typeloc",
                                 rvalue_ref_loc.getTypePtr());
      }
      clang::QualType pointee_type_qt =
          markClangQualTypeDefined(readClangApiValueDefined(
              [&]() { return reference_type_ptr->getPointeeType(); }));
      pointee_type = apply_exact_pointee_qualifiers(
          pointee_type, pointee_loc_type, pointee_type_qt);
      // Keep the exact TypeLoc child.  In particular, a parameter-pack
      // reference must remain unexpanded here so its initialized-name
      // declarator can own the ellipsis after `&&`.
      SgType *reference_type = new SgRvalueReferenceType(pointee_type);
      if (SgTemplateType *parameter = isSgTemplateType(pointee_type);
          parameter != nullptr && parameter->get_packed()) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[rvalue-reference-type-surface]: "
                "exact pointee retains declaration-owned pack syntax\n");
        ROSE_ABORT();
      }
      reference_type =
          apply_local_qualifiers(reference_type, type_loc.getType());
      annotate_auto_type_constraints(reference_type);
      return reference_type;
    }
  }

  if (auto function_no_proto_loc =
          type_loc.getAs<clang::FunctionNoProtoTypeLoc>()) {
    const clang::FunctionNoProtoType *function_no_proto_type =
        function_no_proto_loc.getTypePtr();
    SgFunctionParameterTypeList *param_type_list =
        new SgFunctionParameterTypeList();
    (void)classify_tag_decl_spelled_in_type_loc(
        function_no_proto_loc.getReturnLoc());
    SgType *ret_type =
        buildTypeFromTypeLoc(function_no_proto_loc.getReturnLoc());
    if (ret_type == nullptr) {
      ret_type =
          buildTypeFromQualifiedType(function_no_proto_type->getReturnType());
    }

    SgType *function_type =
        SageBuilder::buildFunctionType(ret_type, param_type_list);
    function_type = apply_local_qualifiers(function_type, type_loc.getType());
    annotate_auto_type_constraints(function_type);
    return function_type;
  }

  if (auto function_proto_loc = type_loc.getAs<clang::FunctionProtoTypeLoc>()) {
    const clang::FunctionProtoType *function_proto_type =
        function_proto_loc.getTypePtr();
    SgFunctionParameterTypeList *param_type_list =
        new SgFunctionParameterTypeList();

    for (unsigned i = 0; i < function_proto_type->getNumParams(); ++i) {
      SgType *param_type = nullptr;
      bool has_exact_parameter_source = false;
      bool source_qualification_present = false;
      bool source_global_qualification = false;
      SgStringList source_qualification_tokens;
      bool source_elaboration_required = false;
      const clang::QualType semantic_parameter_type =
          markClangQualTypeDefined(function_proto_type->getParamType(i));
      bool is_pack_expansion =
          semantic_parameter_type->getAs<clang::PackExpansionType>() != nullptr;
      if (clang::ParmVarDecl *param_decl = function_proto_loc.getParam(i)) {
        if (clang::TypeSourceInfo *type_info =
                param_decl->getTypeSourceInfo()) {
          clang::TypeLoc parameter_loc = type_info->getTypeLoc();
          has_exact_parameter_source = true;
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
          if (param_decl->isParameterPack() != source_pack_expansion ||
              is_pack_expansion != source_pack_expansion) {
            fprintf(stderr,
                    "REX_FRONTEND_INVARIANT[function-argument-pack-surface]: "
                    "parameter %u declaration-pack=%d semantic-pack=%d "
                    "source-pack=%d\n",
                    i, param_decl->isParameterPack() ? 1 : 0,
                    is_pack_expansion ? 1 : 0, source_pack_expansion ? 1 : 0);
            ROSE_ABORT();
          }
          param_type = buildTypeFromTypeLoc(parameter_loc);
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
              parameter_loc, type_info->getTypeLoc().getSourceRange(),
              p_compiler_instance);
        }
      }
      if (param_type == nullptr) {
        param_type = buildTypeFromQualifiedType(semantic_parameter_type);
      }
      param_type = requireExactType(param_type, "function-prototype-parameter",
                                    semantic_parameter_type.getTypePtrOrNull());
      param_type_list->append_argument(param_type);
      SgFunctionTypeArgument *argument_position =
          param_type_list->get_argument_qualification_use_sites().back();
      ASSERT_not_null(argument_position);
      argument_position->set_is_pack_expansion(is_pack_expansion);
      argument_position->set_source_type_qualification_present(
          source_qualification_present);
      argument_position->set_source_type_global_qualification(
          source_global_qualification);
      argument_position->get_source_type_qualification_tokens() =
          source_qualification_tokens;
      argument_position->set_source_type_elaboration_required(
          source_elaboration_required);
      if (has_exact_parameter_source && !source_qualification_present &&
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
    SgType *ret_type = buildTypeFromTypeLoc(function_proto_loc.getReturnLoc());
    if (ret_type == nullptr) {
      ret_type =
          buildTypeFromQualifiedType(function_proto_type->getReturnType());
    }
    ret_type = requireExactType(
        ret_type, "function-prototype-return",
        function_proto_type->getReturnType().getTypePtrOrNull());

    SgFunctionType *function_type = buildFunctionTypeForClangProto(
        ret_type, param_type_list, function_proto_type,
        /*class_type=*/nullptr, /*exact_source_surface=*/true);

    SgType *qualified_function_type =
        apply_local_qualifiers(function_type, type_loc.getType());
    annotate_auto_type_constraints(qualified_function_type);
    return qualified_function_type;
  }

  if (auto type_of_type_loc = type_loc.getAs<clang::TypeOfTypeLoc>()) {
    SgType *underlying_type = nullptr;
    if (clang::TypeSourceInfo *unmodified_tinfo =
            type_of_type_loc.getUnmodifiedTInfo()) {
      underlying_type = buildTypeFromTypeLoc(unmodified_tinfo->getTypeLoc());
    }
    if (underlying_type == nullptr) {
      underlying_type =
          buildTypeFromQualifiedType(type_of_type_loc.getUnmodifiedType());
    }
    if (underlying_type != nullptr) {
      return finalize_spelled_type(
          SageBuilder::buildTypeOfType(nullptr, underlying_type));
    }
  }

  auto build_nonreal_template_type =
      [&](const std::string &base_name, SgScopeStatement *scope,
          clang::NestedNameSpecifier qualifier, bool has_template_keyword,
          const SgName &semantic_name, SgTemplateArgumentPtrList &tpl_args,
          clang::TemplateDecl *template_decl) -> SgType * {
    if (base_name.empty()) {
      return nullptr;
    }
    SgScopeStatement *effective_scope =
        scope != nullptr ? scope : resolve_scope();
    if (enable_default_template_args) {
      append_default_args_from_template_decl(base_name, effective_scope,
                                             tpl_args);
    }
    ensureTemplateArgumentParents(tpl_args);

    clang::NestedNameSpecifier effective_qualifier =
        markClangNestedNameSpecifierDefined(qualifier);
    template_decl = const_cast<clang::TemplateDecl *>(
        llvm::dyn_cast_or_null<clang::TemplateDecl>(
            markClangDeclObjectDefinedByKind(template_decl)));
    clang::DeclContext *template_context =
        template_decl != nullptr
            ? markClangDeclContextObjectDefined(template_decl->getDeclContext())
            : nullptr;
    if (!effective_qualifier && template_decl != nullptr &&
        p_compiler_instance != nullptr &&
        canSynthesizeNamespaceQualifierFromDeclContext(template_context)) {
      if (!scopeIsWithinNamespaceChain(effective_scope, template_context)) {
        effective_qualifier = buildNamespaceQualifierForDeclContext(
            template_context, p_compiler_instance->getASTContext());
      }
    }

    SgNonrealType *nrtype = nullptr;
    if (effective_qualifier) {
      nrtype = buildSemanticNonrealTypeFromNestedNameSpecifier(
          effective_qualifier, effective_scope, SgName(base_name), &tpl_args,
          &semantic_name);
    } else {
      nrtype = SageBuilder::buildSemanticNonrealType(
          SgName(base_name), effective_scope, &tpl_args, &semantic_name);
    }
    if (SgNonrealDecl *nrdecl =
            isSgNonrealDecl(nrtype ? nrtype->get_declaration() : nullptr)) {
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
      if (template_decl != nullptr) {
        if (clang::TemplateTemplateParmDecl *template_parameter =
                llvm::dyn_cast<clang::TemplateTemplateParmDecl>(
                    template_decl)) {
          SgTemplateParameter *sage_parameter =
              lookupActiveTemplateParameterSurface(template_parameter,
                                                   "written-template-id");
          if (sage_parameter == nullptr) {
            auto translated = p_decl_translation_map.find(template_parameter);
            sage_parameter = translated != p_decl_translation_map.end()
                                 ? isSgTemplateParameter(translated->second)
                                 : nullptr;
          }
          SgTemplateDeclaration *parameter_identity =
              sage_parameter != nullptr
                  ? isSgTemplateDeclaration(
                        sage_parameter->get_templateDeclaration())
                  : nullptr;
          SgTemplateType *parameter_type =
              sage_parameter != nullptr
                  ? isSgTemplateType(sage_parameter->get_type())
                  : nullptr;
          if (sage_parameter == nullptr || parameter_identity == nullptr ||
              parameter_type == nullptr ||
              parameter_type->get_template_parameter() != sage_parameter ||
              parameter_type->get_template_parameter_depth() !=
                  static_cast<int>(template_parameter->getDepth()) ||
              parameter_type->get_template_parameter_position() !=
                  static_cast<int>(template_parameter->getIndex())) {
            fprintf(stderr,
                    "REX_FRONTEND_INVARIANT[written-template-link]: "
                    "template-id=%s parameter depth=%u index=%u has no exact "
                    "producer-published Sage template identity\n",
                    base_name.c_str(), template_parameter->getDepth(),
                    template_parameter->getIndex());
            ROSE_ABORT();
          }
          nrdecl->set_templateDeclaration(parameter_identity);
        } else {
          // A per-use template-id is a source spelling surface, but its
          // terminal declaration still names one exact semantic template
          // family. Publish that typed edge at construction time. Deferring
          // the link until an unparser or consumer happens to need the name
          // creates a malformed AST and hides declaration-ordering bugs.
          linkNonrealTemplateDeclaration(nrdecl, template_decl,
                                         "written-template-id");
        }
      }
      // A specialization of a Clang builtin template is a semantic type
      // identity, not a declaration written at the use site.  Classify that
      // exact producer result immediately; otherwise the generic nonreal-type
      // builder's physical-output publication makes the intrinsic appear to
      // be a second source declaration beside the builtin canonical.
      if (llvm::isa_and_nonnull<clang::BuiltinTemplateDecl>(template_decl)) {
        mark_compiler_generated_frontend_specific(nrdecl);
      }
    }
    return nrtype;
  };
  auto class_template_decl_for_record =
      [](clang::CXXRecordDecl *record) -> clang::TemplateDecl * {
    record = const_cast<clang::CXXRecordDecl *>(
        llvm::dyn_cast_or_null<clang::CXXRecordDecl>(
            markClangDeclObjectDefinedByKind(record)));
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
  auto attach_decl_to_nonreal = [&](SgType *candidate, clang::Decl *decl_key,
                                    bool allow_on_demand_lookup) -> SgType * {
    SgNonrealType *nrtype = isSgNonrealType(candidate);
    if (nrtype == nullptr || decl_key == nullptr) {
      return candidate;
    }

    SgNonrealDecl *nrdecl = isSgNonrealDecl(nrtype->get_declaration());
    if (nrdecl == nullptr) {
      return candidate;
    }

    SgDeclarationStatement *sg_decl =
        lookupSgDeclarationForClangDecl(decl_key, allow_on_demand_lookup);
    sg_decl = normalizeNonrealTemplateDeclarationTarget(sg_decl);
    if (sg_decl != nullptr) {
      nrdecl->set_templateDeclaration(sg_decl);
      return candidate;
    }

    if (auto *spec_decl =
            llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(decl_key)) {
      queuePendingImplicitClassTemplateSpecialization(spec_decl);
    }

    if (llvm::isa<clang::ClassTemplateSpecializationDecl>(decl_key) ||
        llvm::isa<clang::ClassTemplatePartialSpecializationDecl>(decl_key) ||
        llvm::isa<clang::ClassTemplateDecl>(decl_key) ||
        llvm::isa<clang::TypeAliasTemplateDecl>(decl_key)) {
      linkNonrealTemplateDeclaration(nrdecl, decl_key, "nested-name-specifier");
    }

    return candidate;
  };
  std::function<SgNonrealType *(clang::TypeLoc, SgScopeStatement *, bool)>
      build_nonreal_type_for_nested_name_specifier_typeloc;
  std::function<SgNonrealType *(
      clang::NestedNameSpecifierLoc, SgScopeStatement *, const SgName &,
      const SgTemplateArgumentPtrList *, const SgName *)>
      build_nonreal_type_from_nested_name_specifier_loc;

  build_nonreal_type_for_nested_name_specifier_typeloc =
      [&](clang::TypeLoc nested_type_loc, SgScopeStatement *scope,
          bool prefer_current_scope) -> SgNonrealType * {
    markClangTypeLocDataDefined(nested_type_loc);
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
      markClangTypeLocDataDefined(spec_loc);
      if (const clang::TemplateSpecializationType *tst =
              static_cast<const clang::TemplateSpecializationType *>(
                  markClangTypeObjectDefinedByClass(readClangApiValueDefined(
                      [&]() { return spec_loc.getTypePtr(); })))) {
        clang::TemplateName tname = markClangTemplateNameDefined(
            readClangApiValueDefined([&]() { return tst->getTemplateName(); }));
        std::string base_name = getTemplateNameBase(tname);
        if (!base_name.empty()) {
          clang::Decl *translated_decl_key = nullptr;
          if (!readClangApiValueDefined(
                  [&]() { return tst->isDependentType(); })) {
            if (const clang::CXXRecordDecl *record_decl =
                    cxxRecordDeclFromQualTypeWithoutDefinitionLookup(
                        clang::QualType(tst, 0))) {
              translated_decl_key =
                  const_cast<clang::CXXRecordDecl *>(record_decl);
            }
          }
          if (translated_decl_key == nullptr) {
            translated_decl_key = llvm::dyn_cast_or_null<clang::Decl>(
                resolve_template_decl(tname));
          }
          SgTemplateArgumentPtrList tpl_args;
          append_nonreal_template_arguments(
              tpl_args, spec_loc, resolve_template_decl(tname),
              "nested-name-specifier-template-id");

          clang::NestedNameSpecifier qualifier = std::nullopt;
          if (!prefer_current_scope) {
            if (clang::NestedNameSpecifierLoc prefix_loc =
                    markClangNestedNameSpecifierLocDefined(
                        readClangApiValueDefined(
                            [&]() { return spec_loc.getPrefix(); }))) {
              qualifier =
                  markClangNestedNameSpecifierDefined(readClangApiValueDefined(
                      [&]() { return prefix_loc.getNestedNameSpecifier(); }));
            }
            if (!qualifier) {
              if (const clang::QualifiedTemplateName *qtn =
                      tname.getAsQualifiedTemplateName()) {
                qualifier = markClangNestedNameSpecifierDefined(
                    readClangApiValueDefined(
                        [&]() { return qtn->getQualifier(); }));
              } else if (const clang::DependentTemplateName *dtn =
                             tname.getAsDependentTemplateName()) {
                qualifier = markClangNestedNameSpecifierDefined(
                    readClangApiValueDefined(
                        [&]() { return dtn->getQualifier(); }));
              }
            }
          }

          const bool has_template_keyword =
              readClangApiValueDefined([&]() {
                return spec_loc.getTemplateKeywordLoc();
              }).isValid();
          const SgName semantic_name(buildExactTemplateInstantiationName(
              base_name, tst->template_arguments(),
              currentTemplateParameterDeclContext()));
          if (SgType *nr = build_nonreal_template_type(
                  base_name, scope, qualifier, has_template_keyword,
                  semantic_name, tpl_args, resolve_template_decl(tname))) {
            nr = attach_decl_to_nonreal(nr, translated_decl_key,
                                        /*allow_on_demand_lookup=*/false);
            nr = apply_global_qualifier_from_loc(
                nr,
                markClangNestedNameSpecifierLocDefined(readClangApiValueDefined(
                    [&]() { return spec_loc.getPrefix(); })));
            return isSgNonrealType(nr);
          }
        }
      }
    }

    if (!prefer_current_scope) {
      if (clang::NestedNameSpecifierLoc prefix_loc =
              markClangNestedNameSpecifierLocDefined(readClangApiValueDefined(
                  [&]() { return nested_type_loc.getPrefix(); }))) {
        auto build_from_named_decl =
            [&](const clang::NamedDecl *decl) -> SgNonrealType * {
          if (decl == nullptr) {
            return nullptr;
          }
          decl = llvm::dyn_cast_or_null<clang::NamedDecl>(
              markClangDeclObjectDefinedByKind(decl));
          std::string name_str = decl != nullptr
                                     ? readClangApiValueDefined([&]() {
                                         return decl->getNameAsString();
                                       })
                                     : "";
          if (name_str.empty()) {
            return nullptr;
          }
          return build_nonreal_type_from_nested_name_specifier_loc(
              prefix_loc, scope, SgName(name_str), nullptr, nullptr);
        };

        if (auto tag_loc = readClangTypeLocDefined(
                [&]() { return nested_type_loc.getAs<clang::TagTypeLoc>(); })) {
          if (SgNonrealType *nr =
                  build_from_named_decl(readClangApiValueDefined(
                      [&]() { return tag_loc.getDecl(); }))) {
            return nr;
          }
        }
        if (auto typedef_loc = readClangTypeLocDefined([&]() {
              return nested_type_loc.getAs<clang::TypedefTypeLoc>();
            })) {
          const clang::TypedefType *typedef_type =
              llvm::dyn_cast_or_null<clang::TypedefType>(
                  markClangTypeObjectDefinedByClass(readClangApiValueDefined(
                      [&]() { return typedef_loc.getTypePtr(); })));
          if (SgNonrealType *nr = build_from_named_decl(
                  typedef_type != nullptr ? readClangApiValueDefined([&]() {
                    return typedef_type->getDecl();
                  })
                                          : nullptr)) {
            return nr;
          }
        }
        if (auto using_loc = readClangTypeLocDefined([&]() {
              return nested_type_loc.getAs<clang::UsingTypeLoc>();
            })) {
          const clang::UsingType *using_type =
              llvm::dyn_cast_or_null<clang::UsingType>(
                  markClangTypeObjectDefinedByClass(readClangApiValueDefined(
                      [&]() { return using_loc.getTypePtr(); })));
          if (SgNonrealType *nr = build_from_named_decl(
                  using_type != nullptr ? readClangApiValueDefined([&]() {
                    return using_type->getDecl();
                  })
                                        : nullptr)) {
            return nr;
          }
        }
        if (auto injected_loc = readClangTypeLocDefined([&]() {
              return nested_type_loc.getAs<clang::InjectedClassNameTypeLoc>();
            })) {
          if (SgNonrealType *nr =
                  build_from_named_decl(readClangApiValueDefined(
                      [&]() { return injected_loc.getDecl(); }))) {
            return nr;
          }
        }
      }
    }

    SgNonrealType *nrtype = buildNonrealTypeForNestedNameSpecifierType(
        markClangTypeObjectDefinedByClass(readClangApiValueDefined(
            [&]() { return nested_type_loc.getTypePtr(); })),
        scope, prefer_current_scope);
    if (nrtype != nullptr) {
      nrtype = isSgNonrealType(apply_global_qualifier_from_loc(
          nrtype,
          markClangNestedNameSpecifierLocDefined(readClangApiValueDefined(
              [&]() { return nested_type_loc.getPrefix(); }))));
      if (SgNonrealDecl *nrdecl = isSgNonrealDecl(nrtype->get_declaration())) {
        if (readClangApiValueDefined([&]() {
              return nested_type_loc.getTemplateKeywordLoc();
            }).isValid()) {
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
          const SgName *terminal_semantic_name) -> SgNonrealType * {
    qualifier_loc = markClangNestedNameSpecifierLocDefined(qualifier_loc);
    if (!qualifier_loc) {
      return SageBuilder::buildSemanticNonrealType(
          terminal_name, scope, terminal_template_args, terminal_semantic_name);
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
      current_loc = markClangNestedNameSpecifierLocDefined(current_loc);
      if (!current_loc) {
        return current_scope;
      }

      current_scope = build_chain(nested_name_specifier_loc_prefix(current_loc),
                                  current_scope);

      clang::NestedNameSpecifier current_nns =
          markClangNestedNameSpecifierDefined(readClangApiValueDefined(
              [&]() { return current_loc.getNestedNameSpecifier(); }));
      SgNonrealType *segment_type = nullptr;
      switch (
          readClangApiValueDefined([&]() { return current_nns.getKind(); })) {
      case clang::NestedNameSpecifier::Kind::Namespace: {
        const clang::NamespaceBaseDecl *ns =
            llvm::dyn_cast_or_null<clang::NamespaceBaseDecl>(
                markClangDeclObjectDefinedByKind(
                    nestedNameSpecifierNamespaceBase(current_nns)));
        std::string name_str = ns ? readClangApiValueDefined(
                                        [&]() { return ns->getNameAsString(); })
                                  : "";
        ROSE_ASSERT(!name_str.empty());
        segment_type = SageBuilder::buildSemanticNonrealType(
            SgName(name_str), current_scope, nullptr, nullptr);
        break;
      }
      case clang::NestedNameSpecifier::Kind::Type: {
        bool prefer_current =
            static_cast<bool>(markClangNestedNameSpecifierLocDefined(
                nested_name_specifier_loc_prefix(current_loc)));
        segment_type = build_nonreal_type_for_nested_name_specifier_typeloc(
            readClangTypeLocDefined(
                [&]() { return current_loc.getAsTypeLoc(); }),
            current_scope, prefer_current);
        break;
      }
      case clang::NestedNameSpecifier::Kind::Global:
        break;
      case clang::NestedNameSpecifier::Kind::MicrosoftSuper: {
        clang::CXXRecordDecl *record = const_cast<clang::CXXRecordDecl *>(
            llvm::dyn_cast_or_null<clang::CXXRecordDecl>(
                markClangDeclObjectDefinedByKind(readClangApiValueDefined(
                    [&]() { return current_nns.getAsRecordDecl(); }))));
        std::string name_str = record ? readClangApiValueDefined([&]() {
          return record->getNameAsString();
        })
                                      : "";
        if (name_str.empty()) {
          name_str = "__super";
        }
        segment_type = SageBuilder::buildSemanticNonrealType(
            SgName(name_str), current_scope, nullptr, nullptr);
        break;
      }
      case clang::NestedNameSpecifier::Kind::Null:
        break;
      }

      if (segment_type != nullptr) {
        SgNonrealDecl *segment_decl =
            isSgNonrealDecl(segment_type->get_declaration());
        ROSE_ASSERT(segment_decl != nullptr);
        if (clang::NestedNameSpecifierLoc prefix_loc =
                markClangNestedNameSpecifierLocDefined(
                    nested_name_specifier_loc_prefix(current_loc))) {
          prefix_loc = markClangNestedNameSpecifierLocDefined(prefix_loc);
          clang::NestedNameSpecifier prefix =
              markClangNestedNameSpecifierDefined(readClangApiValueDefined(
                  [&]() { return prefix_loc.getNestedNameSpecifier(); }));
          if (readClangApiValueDefined([&]() { return prefix.getKind(); }) ==
                  clang::NestedNameSpecifier::Kind::Global &&
              readClangApiValueDefined([&]() {
                return prefix_loc.getLocalBeginLoc();
              }).isValid()) {
            segment_decl->set_has_global_qualifier(true);
          }
        }
        current_scope = segment_decl->get_nonreal_decl_scope();
      }

      return current_scope;
    };

    SgScopeStatement *chain_scope = build_chain(qualifier_loc, effective_scope);
    ROSE_ASSERT(chain_scope != nullptr);

    // A qualified template-id's qualifier chain describes semantic lookup, but
    // the template-id remains a source occurrence in the enclosing lexical
    // scope.  When an argument names a local declaration, placing that argument
    // beneath the qualifier's namespace/class declaration scope moves source
    // grammar outside the declaration's lifetime.  Select the lexical owner
    // exactly when required by such an argument and prove that it encloses
    // every referenced local declaration.
    SgScopeStatement *terminal_owner_scope = chain_scope;
    if (terminal_template_args != nullptr) {
      auto is_within_scope = [](SgNode *node, SgScopeStatement *candidate) {
        for (SgNode *current = node; current != nullptr;
             current = current->get_parent()) {
          if (current == candidate) {
            return true;
          }
        }
        return false;
      };
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
          if (!is_within_scope(lexical_scope, declaration_scope)) {
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
        terminal_semantic_name);
    ROSE_ASSERT(nrtype != nullptr);

    SgNonrealDecl *nrdecl = isSgNonrealDecl(nrtype->get_declaration());
    ROSE_ASSERT(nrdecl != nullptr);

    auto qualifier_requires_typename = [](clang::NestedNameSpecifier nns) {
      return nestedNameSpecifierHasDependentTypeQualifier(nns);
    };

    if (!qualifier_requires_typename(
            markClangNestedNameSpecifierDefined(readClangApiValueDefined(
                [&]() { return qualifier_loc.getNestedNameSpecifier(); })))) {
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
        markClangNestedNameSpecifierDefined(readClangApiValueDefined(
            [&]() { return qualifier_loc.getNestedNameSpecifier(); }));
    const bool semantic_macro_fragment =
        sourceQualificationIsSemanticMacroFragment(source_qualification);
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
    auto build_translated_member_pointer_class_type =
        [&](const clang::CXXRecordDecl *record) -> SgType * {
      if (record == nullptr) {
        return nullptr;
      }

      SgDeclarationStatement *translated_decl = lookupSgDeclarationForClangDecl(
          const_cast<clang::CXXRecordDecl *>(record),
          /*allow_on_demand=*/true);
      if (translated_decl == nullptr) {
        translated_decl = lookupSgDeclarationForClangDecl(
            const_cast<clang::CXXRecordDecl *>(record->getCanonicalDecl()),
            /*allow_on_demand=*/true);
      }

      SgClassDeclaration *class_decl = isSgClassDeclaration(translated_decl);
      if (class_decl == nullptr) {
        return nullptr;
      }
      if (SgClassDeclaration *first_nondef = isSgClassDeclaration(
              class_decl->get_firstNondefiningDeclaration())) {
        class_decl = first_nondef;
      }

      SgType *translated_type = class_decl->get_type();
      if (translated_type == nullptr) {
        translated_type = SgClassType::createType(class_decl);
      }

      return translated_type;
    };
    if (clang::NestedNameSpecifierLoc qualifier_loc =
            member_pointer_loc.getQualifierLoc()) {
      auto build_source_spelled_member_pointer_class_type =
          [&](clang::TypeLoc qualifier_type_loc) -> SgType * {
        if (qualifier_type_loc.isNull()) {
          return nullptr;
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
            std::string base_name = getTemplateNameBase(tname);
            if (base_name.empty()) {
              return nullptr;
            }

            SgTemplateArgumentPtrList tpl_args;
            append_nonreal_template_arguments(
                tpl_args, spec_loc, resolve_template_decl(tname),
                "member-pointer-qualifier-template-id");

            const SgName semantic_name(buildExactTemplateInstantiationName(
                base_name, tst->template_arguments(),
                currentTemplateParameterDeclContext()));
            return build_nonreal_type_from_nested_name_specifier_loc(
                prefix_loc, resolve_scope(), SgName(base_name), &tpl_args,
                &semantic_name);
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
                prefix_loc, resolve_scope(), SgName(name_str), nullptr,
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
        if (const clang::Type *qualifier_type =
                markClangTypeObjectDefinedByClass(
                    qualifier_type_loc.getTypePtr())) {
          if (const clang::CXXRecordDecl *record =
                  cxxRecordDeclFromQualTypeWithoutDefinitionLookup(
                      clang::QualType(qualifier_type, 0))) {
            class_type = build_translated_member_pointer_class_type(record);
          }
        }
      }
      if (class_type == nullptr) {
        class_type =
            build_class_type_from_member_pointer_qualifier_loc(qualifier_loc);
      }
      if (clang::TypeLoc qualifier_type_loc = qualifier_loc.getAsTypeLoc();
          class_type == nullptr && !qualifier_type_loc.isNull()) {
        class_type = build_nonreal_type_for_nested_name_specifier_typeloc(
            qualifier_type_loc, resolve_scope(),
            /*prefer_current_scope=*/false);
      }
    }
    if (class_type == nullptr && member_pointer_type != nullptr) {
      if (clang::CXXRecordDecl *record =
              member_pointer_type->getMostRecentCXXRecordDecl()) {
        class_type = build_translated_member_pointer_class_type(record);
      }
    }
    if (class_type == nullptr && member_pointer_type != nullptr) {
      clang::QualType class_qual_type;
      if (clang::NestedNameSpecifier qualifier =
              member_pointer_type->getQualifier()) {
        if (const clang::Type *qual_type = qualifier.getAsType()) {
          class_qual_type = clang::QualType(qual_type, 0);
        } else if (clang::CXXRecordDecl *record =
                       member_pointer_type->getMostRecentCXXRecordDecl()) {
          class_qual_type = record->getASTContext().getTypeDeclType(
              static_cast<const clang::TypeDecl *>(record));
        }
      }
      if (!class_qual_type.isNull()) {
        class_type = buildTypeFromQualifiedType(class_qual_type);
      }
    }
    class_type = requireExactType(class_type, "member-pointer-class",
                                  member_pointer_type);

    SgType *base_type =
        buildTypeFromTypeLoc(member_pointer_loc.getPointeeLoc());
    if (base_type == nullptr && member_pointer_type != nullptr) {
      base_type =
          buildTypeFromQualifiedType(member_pointer_type->getPointeeType());
    }
    base_type = requireExactType(
        base_type, "member-pointer-pointee",
        member_pointer_type != nullptr
            ? member_pointer_type->getPointeeType().getTypePtrOrNull()
            : nullptr);

    if (member_pointer_type != nullptr &&
        member_pointer_type->isMemberFunctionPointer()) {
      if (const clang::FunctionProtoType *proto =
              member_pointer_type->getPointeeType()
                  ->getAs<clang::FunctionProtoType>()) {
        const unsigned int mfunc_specifier =
            roseMemberFunctionSpecifierFromClangProto(proto);
        if (SgFunctionType *function_type = isSgFunctionType(base_type)) {
          base_type = buildMemberFunctionTypeWithClonedArguments(
              function_type, class_type, mfunc_specifier,
              "type-loc-member-function-pointer");
        }
      }
    }

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
    annotate_auto_type_constraints(member_pointer_sg_type);
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
    for (clang::NestedNameSpecifierLoc current =
             markClangNestedNameSpecifierLocDefined(qualifier_loc);
         current; current = markClangNestedNameSpecifierLocDefined(
                      nested_name_specifier_loc_prefix(current))) {
      current = markClangNestedNameSpecifierLocDefined(current);
      clang::NestedNameSpecifier nns =
          markClangNestedNameSpecifierDefined(readClangApiValueDefined(
              [&]() { return current.getNestedNameSpecifier(); }));
      if (readClangApiValueDefined([&]() { return nns.getKind(); }) !=
          clang::NestedNameSpecifier::Kind::Type) {
        continue;
      }

      const clang::Type *qualifier_type = markClangTypeObjectDefinedByClass(
          readClangApiValueDefined([&]() { return nns.getAsType(); }));
      if (qualifier_type != nullptr && readClangApiValueDefined([&]() {
            return qualifier_type->isDependentType();
          })) {
        return true;
      }

      clang::TypeLoc qualifier_type_loc =
          readClangTypeLocDefined([&]() { return current.getAsTypeLoc(); });
      markClangTypeLocDataDefined(qualifier_type_loc);
      if (qualifier_type_loc.isNull()) {
        continue;
      }

      if (readClangApiValueDefined([&]() {
            return qualifier_type_loc
                .getAs<clang::TemplateSpecializationTypeLoc>();
          }) ||
          readClangApiValueDefined([&]() {
            return qualifier_type_loc.getAs<clang::DependentNameTypeLoc>();
          })) {
        return true;
      }

      clang::QualType written =
          markClangQualTypeDefined(readClangApiValueDefined(
              [&]() { return qualifier_type_loc.getType(); }));
      if (!written.isNull() &&
          written.getTypePtrOrNull() !=
              markClangQualTypeDefined(readClangApiValueDefined([&]() {
                return written.getCanonicalType();
              })).getTypePtrOrNull()) {
        const clang::Type *written_type =
            markClangTypeObjectDefinedByClass(written.getTypePtrOrNull());
        if (written_type != nullptr && !readClangApiValueDefined([&]() {
              return written_type->isDependentType();
            }) &&
            markClangDeclObjectDefinedByKind(readClangApiValueDefined(
                [&]() { return written_type->getAsTagDecl(); })) != nullptr) {
          continue;
        }
        return true;
      }
    }

    return false;
  };

  auto build_nonreal_from_specialization_decl_context =
      [&](const clang::ClassTemplateSpecializationDecl *spec_decl,
          SgTemplateArgumentPtrList &written_args,
          const SgName &semantic_name) -> SgType * {
    if (spec_decl == nullptr) {
      return nullptr;
    }

    auto attach_specialization_to_nonreal =
        [&](SgNonrealType *nrtype,
            const clang::ClassTemplateSpecializationDecl *specialization)
        -> void {
      if (nrtype == nullptr || specialization == nullptr) {
        return;
      }

      SgNonrealDecl *nrdecl = isSgNonrealDecl(nrtype->get_declaration());
      if (nrdecl == nullptr) {
        return;
      }

      auto *mutable_spec =
          const_cast<clang::ClassTemplateSpecializationDecl *>(specialization);
      SgDeclarationStatement *translated_decl =
          lookupSgDeclarationForClangDecl(mutable_spec,
                                          /*allow_on_demand=*/false);
      if (translated_decl == nullptr &&
          p_decl_translation_in_progress.find(mutable_spec) ==
              p_decl_translation_in_progress.end() &&
          p_decl_translation_on_demand.find(mutable_spec) ==
              p_decl_translation_on_demand.end()) {
        translated_decl = lookupSgDeclarationForClangDecl(
            mutable_spec, /*allow_on_demand=*/true);
      }

      translated_decl =
          normalizeNonrealTemplateDeclarationTarget(translated_decl);
      if (translated_decl != nullptr) {
        nrdecl->set_templateDeclaration(translated_decl);
        return;
      }

      queuePendingImplicitClassTemplateSpecialization(mutable_spec);

      linkNonrealTemplateDeclaration(nrdecl, mutable_spec,
                                     "specialization-decl-context");
    };

    SgScopeStatement *scope = resolve_scope();
    if (scope == nullptr) {
      scope = SageBuilder::topScopeStack();
    }
    if (scope == nullptr) {
      scope = getGlobalScope();
    }
    if (scope == nullptr) {
      return nullptr;
    }

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
              SgName(ns_name), scope, nullptr, nullptr);
          if (SgNonrealDecl *ns_decl = isSgNonrealDecl(
                  ns_type ? ns_type->get_declaration() : nullptr)) {
            scope = ns_decl->get_nonreal_decl_scope();
          }
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
      SgName ctx_semantic_name;
      if (ctx_spec != nullptr) {
        ctx_args = buildTemplateArguments(ctx_spec->getTemplateArgs(), 0);
        ctx_args_ptr = &ctx_args;
        ctx_semantic_name = SgName(buildExactTemplateInstantiationName(
            record_name, ctx_spec->getTemplateArgs().asArray(), ctx_spec));
      }

      SgNonrealType *record_type = SageBuilder::buildSemanticNonrealType(
          SgName(record_name), scope, ctx_args_ptr,
          ctx_args_ptr != nullptr ? &ctx_semantic_name : nullptr);
      if (SgNonrealDecl *record_decl = isSgNonrealDecl(
              record_type ? record_type->get_declaration() : nullptr)) {
        if (ctx_spec != nullptr) {
          attach_specialization_to_nonreal(record_type, ctx_spec);
        }
        scope = record_decl->get_nonreal_decl_scope();
      }
    }

    std::string spec_name =
        spec_decl->getSpecializedTemplate() != nullptr
            ? spec_decl->getSpecializedTemplate()->getNameAsString()
            : spec_decl->getNameAsString();
    if (spec_name.empty()) {
      spec_name = spec_decl->getNameAsString();
    }
    if (spec_name.empty()) {
      return nullptr;
    }

    ensureTemplateArgumentParents(written_args);
    SgNonrealType *nrtype = SageBuilder::buildSemanticNonrealType(
        SgName(spec_name), scope, &written_args, &semantic_name);
    attach_specialization_to_nonreal(nrtype, spec_decl);
    return nrtype;
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
  auto build_injected_class_template_id_syntax =
      [&](clang::InjectedClassNameTypeLoc injected_loc) -> SgType * {
    const bool preserve_omitted_template_id =
        p_preserve_omitted_injected_class_template_id_depth != 0;
    const bool has_explicit_template_arguments =
        injectedClassNameTypeLocHasExplicitTemplateArguments(
            injected_loc, p_compiler_instance);
    if (!preserve_omitted_template_id && !has_explicit_template_arguments) {
      return nullptr;
    }

    const clang::InjectedClassNameType *injected =
        static_cast<const clang::InjectedClassNameType *>(
            markClangTypeObjectDefinedByClass(injected_loc.getTypePtr()));
    if (injected == nullptr) {
      return nullptr;
    }

    clang::CXXRecordDecl *record = injected->getDecl();
    record = const_cast<clang::CXXRecordDecl *>(
        llvm::dyn_cast_or_null<clang::CXXRecordDecl>(
            markClangDeclObjectDefinedByKind(record)));
    if (record == nullptr) {
      return nullptr;
    }

    clang::CXXRecordDecl *template_record = record;
    if (record->isInjectedClassName()) {
      template_record = const_cast<clang::CXXRecordDecl *>(
          llvm::dyn_cast_or_null<clang::CXXRecordDecl>(
              markClangDeclContextObjectDefined(record->getDeclContext())));
    }
    template_record = const_cast<clang::CXXRecordDecl *>(
        llvm::dyn_cast_or_null<clang::CXXRecordDecl>(
            markClangDeclObjectDefinedByKind(template_record)));
    if (template_record == nullptr) {
      return nullptr;
    }

    std::string base_name = template_record->getNameAsString();
    if (base_name.empty()) {
      return nullptr;
    }

    clang::TemplateDecl *template_decl =
        class_template_decl_for_record(template_record);
    clang::Decl *decl_key = template_decl != nullptr
                                ? static_cast<clang::Decl *>(template_decl)
                                : static_cast<clang::Decl *>(template_record);
    if (preserve_omitted_template_id && !has_explicit_template_arguments) {
      // An injected class name such as `_Hashtable` in its own member return
      // type denotes the complete current instantiation semantically, but the
      // exact source surface intentionally omits `<...>`.  A null template-
      // argument pointer is the typed SgNonrealDecl representation of that
      // omission; an empty list would instead manufacture the distinct syntax
      // `_Hashtable<>`.
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
      SgNonrealType *omitted_type =
          qualifier_loc
              ? build_nonreal_type_from_nested_name_specifier_loc(
                    qualifier_loc, scope, SgName(base_name), nullptr, nullptr)
              : SageBuilder::buildSemanticNonrealType(SgName(base_name), scope,
                                                      nullptr, nullptr);
      if (omitted_type == nullptr) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[injected-class-source-syntax]: "
                "class=%s could not publish omitted template-id syntax\n",
                base_name.c_str());
        ROSE_ABORT();
      }
      return apply_elaborated_keyword_to_nonreal(
          attach_decl_to_nonreal(omitted_type, decl_key,
                                 /*allow_on_demand_lookup=*/true),
          injected->getKeyword());
    }

    SgTemplateArgumentPtrList tpl_args =
        build_current_instantiation_template_arguments(template_record);
    if (tpl_args.empty()) {
      return nullptr;
    }

    clang::NestedNameSpecifier qualifier =
        has_explicit_template_arguments
            ? injected_loc.getQualifierLoc().getNestedNameSpecifier()
            : std::nullopt;
    clang::QualType injected_specialization =
        getInjectedClassNameSpecializationType(
            injected, p_compiler_instance->getASTContext());
    std::string semantic_name_string = clangQualTypeAsStringDefinedForFrontend(
        injected_specialization, p_compiler_instance->getLangOpts());
    std::vector<std::string> semantic_name_components =
        splitQualifiedNameOutsideTemplates(semantic_name_string);
    ROSE_ASSERT(!semantic_name_components.empty());
    semantic_name_string = trimWhitespace(semantic_name_components.back());
    ROSE_ASSERT(semantic_name_string.find('<') != std::string::npos);
    const SgName semantic_name(semantic_name_string);
    SgType *nrtype = build_nonreal_template_type(
        base_name, resolve_scope(), qualifier,
        /*has_template_keyword=*/false, semantic_name, tpl_args, template_decl);
    if (nrtype == nullptr) {
      return nullptr;
    }
    SgNonrealType *linked_nonreal_type = isSgNonrealType(
        attach_decl_to_nonreal(nrtype, decl_key,
                               /*allow_on_demand_lookup=*/true));
    if (linked_nonreal_type == nullptr ||
        isSgNonrealDecl(linked_nonreal_type->get_declaration()) == nullptr ||
        isSgNonrealDecl(linked_nonreal_type->get_declaration())
                ->get_templateDeclaration() == nullptr) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[injected-class-template-link]: "
              "class=%s record=%p template=%p did not publish one exact Sage "
              "template identity\n",
              base_name.c_str(), static_cast<void *>(template_record),
              static_cast<void *>(template_decl));
      ROSE_ABORT();
    }
    nrtype = linked_nonreal_type;

    if (!has_explicit_template_arguments) {
      return apply_elaborated_keyword_to_nonreal(nrtype,
                                                 injected->getKeyword());
    }

    return apply_elaborated_keyword_to_nonreal(nrtype, injected->getKeyword());
  };

  auto is_semantic_outer_embedded_forward_tag =
      [](const clang::TagDecl *tag_decl) -> bool {
    const clang::RecordDecl *record_decl =
        llvm::dyn_cast_or_null<clang::RecordDecl>(
            markClangDeclObjectDefinedByKind(tag_decl));
    if (record_decl == nullptr || readClangApiValueDefined([&]() {
          return record_decl->isThisDeclarationADefinition();
        })) {
      return false;
    }

    if (!readClangApiValueDefined(
            [&]() { return record_decl->isEmbeddedInDeclarator(); })) {
      return false;
    }

    const clang::DeclContext *semantic_context =
        markClangDeclContextObjectDefined(readClangApiValueDefined(
            [&]() { return record_decl->getDeclContext(); }));
    const clang::DeclContext *lexical_context =
        markClangDeclContextObjectDefined(readClangApiValueDefined(
            [&]() { return record_decl->getLexicalDeclContext(); }));
    return semantic_context != nullptr && lexical_context != nullptr &&
           semantic_context != lexical_context;
  };

  auto build_nonreal_from_elaborated_parts =
      [&](const clang::Type *named_type, clang::ElaboratedTypeKeyword keyword,
          clang::NestedNameSpecifier qualifier, clang::TypeLoc named_loc,
          clang::NestedNameSpecifierLoc explicit_qualifier_loc =
              clang::NestedNameSpecifierLoc()) -> SgType * {
    named_type = markClangTypeObjectDefinedByClass(named_type);
    qualifier = markClangNestedNameSpecifierDefined(qualifier);
    markClangTypeLocDataDefined(named_loc);
    explicit_qualifier_loc =
        markClangNestedNameSpecifierLocDefined(explicit_qualifier_loc);
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
        explicit_qualifier_loc
            ? explicit_qualifier_loc
            : markClangNestedNameSpecifierLocDefined(named_loc.getPrefix());
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
          rememberEnumTypeFirstSeenState(p_enum_type_decl_first_see_in_type,
                                         sg_enum_decl->get_type(), false);
          return sg_enum_decl->get_type();
        }
      }

      return nullptr;
    };

    auto attach_named_decl_to_nonreal =
        [&](SgType *candidate, const clang::NamedDecl *decl) -> SgType * {
      SgNonrealType *nonreal_type = isSgNonrealType(candidate);
      if (nonreal_type == nullptr || decl == nullptr) {
        return candidate;
      }

      SgNonrealDecl *nonreal_decl =
          isSgNonrealDecl(nonreal_type->get_declaration());
      if (nonreal_decl == nullptr) {
        return candidate;
      }

      bool allow_on_demand_lookup = true;
      if (p_compiler_instance != nullptr) {
        clang::SourceManager &sm = p_compiler_instance->getSourceManager();
        clang::SourceLocation loc = decl->getLocation();
        if (loc.isMacroID()) {
          loc = sm.getSpellingLoc(loc);
        }
        if (loc.isValid() &&
            (sm.isInSystemHeader(loc) || sm.isWrittenInBuiltinFile(loc))) {
          allow_on_demand_lookup = false;
        }
      }

      SgDeclarationStatement *sg_decl = lookupSgDeclarationForClangDecl(
          const_cast<clang::NamedDecl *>(decl),
          /*allow_on_demand=*/allow_on_demand_lookup);
      sg_decl = normalizeNonrealTemplateDeclarationTarget(sg_decl);

      // A source-qualified alias in a system header can be intentionally
      // absent from the translated declaration map even though Clang has
      // already resolved its exact canonical type.  The written nonreal node
      // still needs a typed semantic target.  Bind it to the declaration of
      // that canonical type instead of leaving an untyped name for a later
      // matcher or unparser to guess.
      if (sg_decl == nullptr) {
        clang::QualType canonical_alias_type;
        if (const clang::TypedefNameDecl *typedef_decl =
                llvm::dyn_cast<clang::TypedefNameDecl>(decl)) {
          canonical_alias_type = markClangQualTypeDefined(
              typedef_decl->getUnderlyingType().getCanonicalType());
        } else if (const clang::UsingShadowDecl *using_decl =
                       llvm::dyn_cast<clang::UsingShadowDecl>(decl)) {
          const clang::NamedDecl *target =
              llvm::dyn_cast_or_null<clang::NamedDecl>(
                  markClangDeclObjectDefinedByKind(
                      using_decl->getTargetDecl()));
          if (const clang::TypedefNameDecl *typedef_target =
                  llvm::dyn_cast_or_null<clang::TypedefNameDecl>(target)) {
            canonical_alias_type = markClangQualTypeDefined(
                typedef_target->getUnderlyingType().getCanonicalType());
          }
        }

        if (!canonical_alias_type.isNull()) {
          SgType *semantic_type =
              buildTypeFromQualifiedType(canonical_alias_type);
          SgNamedType *semantic_named_type = isSgNamedType(
              semantic_type != nullptr ? semantic_type->findBaseType()
                                       : nullptr);
          sg_decl = normalizeNonrealTemplateDeclarationTarget(
              semantic_named_type != nullptr
                  ? semantic_named_type->get_declaration()
                  : nullptr);
        }
      }

      if (sg_decl != nullptr) {
        nonreal_decl->set_templateDeclaration(sg_decl);
      } else if (llvm::isa<clang::ClassTemplateSpecializationDecl>(decl) ||
                 llvm::isa<clang::ClassTemplatePartialSpecializationDecl>(
                     decl) ||
                 llvm::isa<clang::ClassTemplateDecl>(decl) ||
                 llvm::isa<clang::TypeAliasTemplateDecl>(decl)) {
        if (const auto *spec_decl =
                llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(decl)) {
          auto *mutable_spec =
              const_cast<clang::ClassTemplateSpecializationDecl *>(spec_decl);
          queuePendingImplicitClassTemplateSpecialization(mutable_spec);
        }

        linkNonrealTemplateDeclaration(
            nonreal_decl,
            const_cast<clang::Decl *>(llvm::cast<clang::Decl>(decl)),
            "named-type-location");
      }

      return candidate;
    };

    if (auto spec_loc =
            named_loc.getAs<clang::TemplateSpecializationTypeLoc>()) {
      const clang::TemplateSpecializationType *tst = spec_loc.getTypePtr();
      if (tst != nullptr) {
        tst = static_cast<const clang::TemplateSpecializationType *>(
            markClangTypeObjectDefinedByClass(tst));
        clang::TemplateName tname =
            markClangTemplateNameDefined(tst->getTemplateName());
        std::string base_name = getTemplateNameBase(tname);
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
              // The resolved class type carries the complete semantic
              // argument list, including defaults.  This TypeLoc still owns
              // the exact written prefix.  Publish that distinction before
              // returning through the namespace-qualified fast path; the
              // general specialization path below is otherwise bypassed and
              // every semantic argument is incorrectly classified as
              // implicit.
              preserve_written_template_specialization_arguments(spec_loc,
                                                                 resolved_type);
              annotate_auto_type_constraints(resolved_type);
              return resolved_type;
            }
          }

          SgTemplateArgumentPtrList tpl_args;
          append_nonreal_template_arguments(tpl_args, spec_loc,
                                            resolve_template_decl(tname),
                                            "named-type-location-template-id");

          bool has_template_keyword = false;
          if (const clang::QualifiedTemplateName *qtn =
                  tname.getAsQualifiedTemplateName()) {
            has_template_keyword = qtn->hasTemplateKeyword();
          } else if (tname.getAsDependentTemplateName() != nullptr) {
            has_template_keyword = true;
          }

          const SgName semantic_name(buildExactTemplateInstantiationName(
              base_name, tst->template_arguments(),
              currentTemplateParameterDeclContext()));
          if (SgType *nr =
                  qualifier_loc
                      ? static_cast<SgType *>(
                            build_nonreal_type_from_nested_name_specifier_loc(
                                qualifier_loc, scope, SgName(base_name),
                                &tpl_args, &semantic_name))
                      : build_nonreal_template_type(
                            base_name, scope, qualifier, has_template_keyword,
                            semantic_name, tpl_args,
                            resolve_template_decl(tname))) {
            if (SgNonrealDecl *nrdecl =
                    isSgNonrealDecl(isSgNonrealType(nr) != nullptr
                                        ? isSgNonrealType(nr)->get_declaration()
                                        : nullptr)) {
              if (has_template_keyword ||
                  spec_loc.getTemplateKeywordLoc().isValid()) {
                nrdecl->set_has_template_keyword(true);
              }
            }
            return apply_elaborated_keyword_to_nonreal(
                attach_named_decl_to_nonreal(nr, resolve_template_decl(tname)),
                keyword);
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
                          nullptr, nullptr))
                : static_cast<SgType *>(
                      buildSemanticNonrealTypeFromNestedNameSpecifier(
                          qualifier, scope, SgName(id->getName().str()),
                          nullptr, nullptr)),
            keyword);
      }
    }

    if (const clang::TypedefType *typedef_type =
            llvm::dyn_cast<clang::TypedefType>(named_type)) {
      clang::TypedefNameDecl *typedef_decl = typedef_type->getDecl();
      if (typedef_decl != nullptr) {
        return apply_elaborated_keyword_to_nonreal(
            attach_named_decl_to_nonreal(
                qualifier_loc
                    ? static_cast<SgType *>(
                          build_nonreal_type_from_nested_name_specifier_loc(
                              qualifier_loc, scope,
                              SgName(typedef_decl->getNameAsString()), nullptr,
                              nullptr))
                    : static_cast<SgType *>(
                          buildSemanticNonrealTypeFromNestedNameSpecifier(
                              qualifier, scope,
                              SgName(typedef_decl->getNameAsString()), nullptr,
                              nullptr)),
                typedef_decl),
            keyword);
      }
    }

    if (const clang::UsingType *using_type =
            llvm::dyn_cast<clang::UsingType>(named_type)) {
      clang::UsingShadowDecl *using_decl = using_type->getDecl();
      if (using_decl != nullptr) {
        return apply_elaborated_keyword_to_nonreal(
            qualifier_loc
                ? static_cast<SgType *>(
                      build_nonreal_type_from_nested_name_specifier_loc(
                          qualifier_loc, scope,
                          SgName(using_decl->getNameAsString()), nullptr,
                          nullptr))
                : static_cast<SgType *>(
                      buildSemanticNonrealTypeFromNestedNameSpecifier(
                          qualifier, scope,
                          SgName(using_decl->getNameAsString()), nullptr,
                          nullptr)),
            keyword);
      }
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
                          nullptr, nullptr))
                : static_cast<SgType *>(
                      buildSemanticNonrealTypeFromNestedNameSpecifier(
                          qualifier, scope, SgName(decl->getNameAsString()),
                          nullptr, nullptr)),
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
        return apply_elaborated_keyword_to_nonreal(
            attach_named_decl_to_nonreal(
                qualifier_loc
                    ? static_cast<SgType *>(
                          build_nonreal_type_from_nested_name_specifier_loc(
                              qualifier_loc, scope,
                              SgName(tag_decl->getNameAsString()), nullptr,
                              nullptr))
                    : static_cast<SgType *>(
                          buildSemanticNonrealTypeFromNestedNameSpecifier(
                              qualifier, scope,
                              SgName(tag_decl->getNameAsString()), nullptr,
                              nullptr)),
                tag_decl),
            keyword);
      }
    }

    if (const clang::InjectedClassNameType *injected =
            llvm::dyn_cast<clang::InjectedClassNameType>(named_type)) {
      clang::QualType injected_qt;
      if (p_compiler_instance != nullptr) {
        injected_qt = getInjectedClassNameSpecializationType(
            injected, p_compiler_instance->getASTContext());
      }
      const clang::Type *injected_ty = injected_qt.getTypePtrOrNull();
      if (injected_ty != nullptr) {
        if (SgNonrealType *nrtype = buildNonrealTypeForNestedNameSpecifierType(
                injected_ty, scope, /*prefer_current_scope=*/false)) {
          return apply_elaborated_keyword_to_nonreal(
              attach_named_decl_to_nonreal(nrtype, injected->getDecl()),
              keyword);
        }
      }
      if (clang::CXXRecordDecl *decl = injected->getDecl()) {
        return apply_elaborated_keyword_to_nonreal(
            attach_named_decl_to_nonreal(
                qualifier_loc
                    ? static_cast<SgType *>(
                          build_nonreal_type_from_nested_name_specifier_loc(
                              qualifier_loc, scope,
                              SgName(decl->getNameAsString()), nullptr,
                              nullptr))
                    : static_cast<SgType *>(
                          buildSemanticNonrealTypeFromNestedNameSpecifier(
                              qualifier, scope, SgName(decl->getNameAsString()),
                              nullptr, nullptr)),
                decl),
            keyword);
      }
    }

    return nullptr;
  };

  if (auto spec_loc = type_loc.getAs<clang::TemplateSpecializationTypeLoc>()) {
    markClangTypeLocDataDefined(spec_loc);
    const clang::TemplateSpecializationType *spec_type =
        static_cast<const clang::TemplateSpecializationType *>(
            markClangTypeObjectDefinedByClass(spec_loc.getTypePtr()));
    clang::NestedNameSpecifierLoc qualifier_loc =
        markClangNestedNameSpecifierLocDefined(readClangApiValueDefined(
            [&]() { return spec_loc.getQualifierLoc(); }));
    if (SgType *written_type = build_nonreal_from_elaborated_parts(
            spec_type, spec_type->getKeyword(),
            qualifier_loc.getNestedNameSpecifier(), spec_loc, qualifier_loc)) {
      return finalize_spelled_type(written_type);
    }
  }

  if (auto dep_name_loc = type_loc.getAs<clang::DependentNameTypeLoc>()) {
    markClangTypeLocDataDefined(dep_name_loc);
    if (SgType *written_type = build_nonreal_from_elaborated_parts(
            dep_name_loc.getTypePtr(), dep_name_loc.getTypePtr()->getKeyword(),
            dep_name_loc.getQualifierLoc().getNestedNameSpecifier(),
            dep_name_loc, dep_name_loc.getQualifierLoc())) {
      return finalize_spelled_type(written_type);
    }
  }

  if (auto typedef_loc = type_loc.getAs<clang::TypedefTypeLoc>()) {
    markClangTypeLocDataDefined(typedef_loc);
#if ROSE_USE_VALGRIND
    if (clangFrontendRunningOnValgrind()) {
      if (void *data = typedef_loc.getOpaqueData()) {
        const unsigned local_size =
            std::max<unsigned>(typedef_loc.getLocalDataSize(),
                               sizeof(clang::ElaboratedNameLocInfo));
        if (local_size != 0) {
          VALGRIND_MAKE_MEM_DEFINED(data, local_size);
        }
      }
    }
#endif
    const clang::TypedefType *typedef_type =
        llvm::dyn_cast_or_null<clang::TypedefType>(
            markClangTypeObjectDefinedByClass(typedef_loc.getTypePtr()));
    if (typedef_type != nullptr) {
      markClangNestedNameSpecifierDefined(typedef_type->getQualifier());
    }
    auto typedef_qualifier_loc = [&]() -> clang::NestedNameSpecifierLoc {
      if (typedef_type == nullptr) {
        return markClangNestedNameSpecifierLocDefined(
            typedef_loc.getQualifierLoc());
      }
      clang::NestedNameSpecifier qualifier =
          markClangNestedNameSpecifierDefined(readClangApiValueDefined(
              [&]() { return typedef_type->getQualifier(); }));
      if (!qualifier) {
        return clang::NestedNameSpecifierLoc();
      }
#if ROSE_USE_VALGRIND
      if (clangFrontendRunningOnValgrind()) {
        if (void *data = typedef_loc.getOpaqueData()) {
          void *qualifier_data = nullptr;
          constexpr unsigned qualifier_data_offset =
              2 * sizeof(clang::SourceLocation);
          std::memcpy(&qualifier_data,
                      static_cast<char *>(data) + qualifier_data_offset,
                      sizeof(qualifier_data));
          return markClangNestedNameSpecifierLocDefined(
              clang::NestedNameSpecifierLoc(qualifier, qualifier_data));
        }
      }
#endif
      return markClangNestedNameSpecifierLocDefined(readClangApiValueDefined(
          [&]() { return typedef_loc.getQualifierLoc(); }));
    };
    clang::NestedNameSpecifierLoc qualifier_loc = typedef_qualifier_loc();
    clang::NestedNameSpecifier qualifier =
        markClangNestedNameSpecifierDefined(readClangApiValueDefined(
            [&]() { return qualifier_loc.getNestedNameSpecifier(); }));
    const bool has_written_qualifier = static_cast<bool>(qualifier);
    const bool qualifier_has_type_qualifier =
        nestedNameSpecifierHasTypeQualifier(qualifier);
    const bool qualifier_spells_namespace_or_global =
        nestedNameSpecifierLocHasExplicitGlobal(qualifier_loc) ||
        nestedNameSpecifierHasNamespaceQualifier(qualifier);
    if (!qualifier && typedef_type != nullptr &&
        !readClangApiValueDefined(
            [&]() { return typedef_type->isDependentType(); })) {
      qualifier = markClangNestedNameSpecifierDefined(readClangApiValueDefined(
          [&]() { return typedef_type->getQualifier(); }));
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
        readClangApiValueDefined([&]() {
          return typedef_type->getKeyword();
        }) == clang::ElaboratedTypeKeyword::None &&
        !readClangApiValueDefined(
            [&]() { return typedef_type->isDependentType(); }) &&
        !qualifier_has_type_qualifier && !qualifier_requires_written_nonreal &&
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
      annotate_auto_type_constraints(resolved_type);
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
        annotate_auto_type_constraints(resolved_type);
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
    if (SgType *template_id_syntax =
            build_injected_class_template_id_syntax(injected_loc)) {
      return finalize_spelled_type(template_id_syntax);
    }
    if (SgType *written_type = build_nonreal_from_elaborated_parts(
            injected_loc.getTypePtr(), injected_loc.getTypePtr()->getKeyword(),
            injected_loc.getQualifierLoc().getNestedNameSpecifier(),
            injected_loc, injected_loc.getQualifierLoc())) {
      return finalize_spelled_type(written_type);
    }
  }

  if (auto tag_loc = readClangTypeLocDefined(
          [&]() { return type_loc.getAs<clang::TagTypeLoc>(); })) {
    const clang::TagType *tag_type = llvm::dyn_cast_or_null<clang::TagType>(
        markClangTypeObjectDefinedByClass(
            readClangApiValueDefined([&]() { return tag_loc.getTypePtr(); })));
    clang::NestedNameSpecifierLoc qualifier_loc =
        markClangNestedNameSpecifierLocDefined(readClangApiValueDefined(
            [&]() { return tag_loc.getQualifierLoc(); }));
    clang::NestedNameSpecifier qualifier =
        markClangNestedNameSpecifierDefined(readClangApiValueDefined(
            [&]() { return qualifier_loc.getNestedNameSpecifier(); }));
    const bool has_explicit_written_qualifier = static_cast<bool>(qualifier);
    if (!qualifier && tag_type != nullptr) {
      qualifier = markClangNestedNameSpecifierDefined(
          readClangApiValueDefined([&]() { return tag_type->getQualifier(); }));
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
      clang::TagDecl *tag_decl =
          const_cast<clang::TagDecl *>(llvm::dyn_cast_or_null<clang::TagDecl>(
              markClangDeclObjectDefinedByKind(raw_tag_decl)));
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
          rememberEnumTypeFirstSeenState(p_enum_type_decl_first_see_in_type,
                                         sg_enum_decl->get_type(), false);
          return sg_enum_decl->get_type();
        }
      }
      return nullptr;
    };
    SgType *translated_tag_type = translated_tag_type_if_available(
        tag_type != nullptr
            ? llvm::dyn_cast_or_null<clang::TagDecl>(
                  markClangDeclObjectDefinedByKind(readClangApiValueDefined(
                      [&]() { return tag_type->getDecl(); })))
            : nullptr);

    auto build_current_instantiation_member_tag_type =
        [&](const clang::TagDecl *raw_tag_decl) -> SgType * {
      clang::TagDecl *tag_decl =
          const_cast<clang::TagDecl *>(llvm::dyn_cast_or_null<clang::TagDecl>(
              markClangDeclObjectDefinedByKind(raw_tag_decl)));
      if (tag_decl == nullptr || tag_type == nullptr ||
          has_explicit_written_qualifier ||
          readClangApiValueDefined([&]() { return tag_type->getKeyword(); }) !=
              clang::ElaboratedTypeKeyword::None) {
        return nullptr;
      }

      const auto *owner_record = llvm::dyn_cast_or_null<clang::CXXRecordDecl>(
          markClangDeclContextObjectDefined(readClangApiValueDefined(
              [&]() { return tag_decl->getDeclContext(); })));
      if (owner_record == nullptr ||
          (templateParametersForDeclContext(owner_record) == nullptr &&
           !readClangApiValueDefined(
               [&]() { return owner_record->isDependentContext(); }))) {
        return nullptr;
      }

      const clang::FunctionDecl *current_function = nullptr;
      for (auto it = p_template_parameter_decl_context_stack.rbegin();
           it != p_template_parameter_decl_context_stack.rend(); ++it) {
        current_function = llvm::dyn_cast_or_null<clang::FunctionDecl>(*it);
        if (current_function != nullptr &&
            markClangNestedNameSpecifierLocDefined(readClangApiValueDefined(
                [&]() { return current_function->getQualifierLoc(); }))) {
          break;
        }
        current_function = nullptr;
      }
      const auto *method_decl =
          llvm::dyn_cast_or_null<clang::CXXMethodDecl>(current_function);
      if (method_decl == nullptr ||
          markClangDeclObjectDefinedByKind(readClangApiValueDefined(
              [&]() { return method_decl->getParent(); })) == nullptr) {
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
          markClangNestedNameSpecifierLocDefined(readClangApiValueDefined(
              [&]() { return current_function->getQualifierLoc(); }));
      clang::NestedNameSpecifier method_qualifier =
          markClangNestedNameSpecifierDefined(readClangApiValueDefined(
              [&]() { return method_qualifier_loc.getNestedNameSpecifier(); }));
      if (!method_qualifier || readClangApiValueDefined([&]() {
                                 return method_qualifier.getKind();
                               }) != clang::NestedNameSpecifier::Kind::Type) {
        return nullptr;
      }
      const clang::CXXRecordDecl *qualified_record =
          llvm::dyn_cast_or_null<clang::CXXRecordDecl>(
              markClangDeclObjectDefinedByKind(readClangApiValueDefined(
                  [&]() { return method_qualifier.getAsRecordDecl(); })));
      const clang::CXXRecordDecl *method_parent =
          llvm::dyn_cast_or_null<clang::CXXRecordDecl>(
              markClangDeclObjectDefinedByKind(readClangApiValueDefined(
                  [&]() { return method_decl->getParent(); })));
      if (!same_template_record(owner_record, method_parent) ||
          !same_template_record(owner_record, qualified_record)) {
        return nullptr;
      }

      std::string tag_name = readClangApiValueDefined(
          [&]() { return tag_decl->getNameAsString(); });
      if (tag_name.empty()) {
        return nullptr;
      }

      SgNonrealType *nrtype = build_nonreal_type_from_nested_name_specifier_loc(
          method_qualifier_loc, resolve_scope(), SgName(tag_name), nullptr,
          nullptr);
      if (nrtype == nullptr) {
        return nullptr;
      }
      return attach_decl_to_nonreal(nrtype, tag_decl,
                                    /*allow_on_demand_lookup=*/true);
    };

    if (SgType *current_instantiation_tag_type =
            build_current_instantiation_member_tag_type(
                tag_type != nullptr ? llvm::dyn_cast_or_null<clang::TagDecl>(
                                          markClangDeclObjectDefinedByKind(
                                              readClangApiValueDefined([&]() {
                                                return tag_type->getDecl();
                                              })))
                                    : nullptr)) {
      return finalize_spelled_type(current_instantiation_tag_type);
    }

    const bool use_translated_tag_type =
        translated_tag_type != nullptr && tag_type != nullptr &&
        has_written_qualifier &&
        readClangApiValueDefined([&]() { return tag_type->getKeyword(); }) !=
            clang::ElaboratedTypeKeyword::Typename &&
        !readClangApiValueDefined(
            [&]() { return tag_type->isDependentType(); }) &&
        !qualifier_has_type_qualifier && !qualifier_requires_written_nonreal &&
        !force_written_qualified_tag_type;

    // Qualified tag references such as `Outer::Inner` should use the
    // translated declaration type instead of a synthetic SgNonrealType.
    // The nonreal path is needed for dependent and elaborated spellings, but
    // for resolved tags it loses the declaration identity and can corrupt
    // ownership/name-qualification for nested and namespace-qualified tags.
    if (use_translated_tag_type) {
      annotate_auto_type_constraints(translated_tag_type);
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
            qualifier_loc ? build_nonreal_type_from_nested_name_specifier_loc(
                                qualifier_loc, scope,
                                SgName(id->getName().str()), nullptr, nullptr)
                          : buildSemanticNonrealTypeFromNestedNameSpecifier(
                                qualifier, scope, SgName(id->getName().str()),
                                nullptr, nullptr);
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
      std::string base_name = getTemplateNameBase(tname);
      if (!base_name.empty()) {
        SgTemplateArgumentPtrList tpl_args;
        append_nonreal_template_arguments(tpl_args, spec_loc,
                                          resolve_template_decl(tname),
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

        clang::TemplateDecl *template_decl = resolve_template_decl(tname);
        SgScopeStatement *scope = resolve_scope();
        const clang::ClassTemplateSpecializationDecl *spec_record_decl =
            llvm::dyn_cast_or_null<clang::ClassTemplateSpecializationDecl>(
                cxxRecordDeclFromQualTypeWithoutDefinitionLookup(
                    clang::QualType(tst, 0)));
        const bool uses_nested_specialization_decl_context =
            spec_record_decl != nullptr &&
            llvm::isa<clang::CXXRecordDecl>(spec_record_decl->getDeclContext());
        const bool has_written_prefix = static_cast<bool>(spec_loc.getPrefix());
        const bool force_written_template_syntax =
            p_force_written_template_specialization_depth != 0;
        // Keep semantic class types for ordinary unqualified specializations
        // such as `X<A>`; lowering those to SgNonrealType breaks analyses that
        // need the instantiated class declaration.
        const bool preserve_written_lookup =
            force_written_template_syntax || has_written_prefix ||
            uses_nested_specialization_decl_context;
        const SgName semantic_name(buildExactTemplateInstantiationName(
            base_name, tst->template_arguments(),
            spec_record_decl != nullptr
                ? static_cast<const clang::DeclContext *>(spec_record_decl)
                : currentTemplateParameterDeclContext()));
        if (preserve_written_lookup ||
            template_specialization_type_loc_requires_written_nonreal(
                spec_loc)) {
          if (has_written_prefix) {
            SgType *nr = build_nonreal_type_from_nested_name_specifier_loc(
                spec_loc.getPrefix(), scope, SgName(base_name), &tpl_args,
                &semantic_name);
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
            if (SgType *nr = build_nonreal_from_specialization_decl_context(
                    spec_record_decl, tpl_args, semantic_name)) {
              return finalize_spelled_type(nr);
            }
          }
          if (SgType *nr = build_nonreal_template_type(
                  base_name, scope, qualifier, has_template_keyword,
                  semantic_name, tpl_args, template_decl)) {
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
        }
      }
    }

    if (tst != nullptr && tst->isDependentType()) {
      clang::TemplateName tname = tst->getTemplateName();
      std::string base_name = getTemplateNameBase(tname);
      if (!base_name.empty()) {
        SgTemplateArgumentPtrList tpl_args;
        append_nonreal_template_arguments(tpl_args, spec_loc,
                                          resolve_template_decl(tname),
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

        const SgName semantic_name(buildExactTemplateInstantiationName(
            base_name, tst->template_arguments(),
            currentTemplateParameterDeclContext()));
        if (SgType *nr = build_nonreal_template_type(
                base_name, resolve_scope(), qualifier, has_template_keyword,
                semantic_name, tpl_args, resolve_template_decl(tname))) {
          if (clang::NestedNameSpecifierLoc qualifier_loc =
                  spec_loc.getPrefix()) {
            nr = build_nonreal_type_from_nested_name_specifier_loc(
                qualifier_loc, resolve_scope(), SgName(base_name), &tpl_args,
                &semantic_name);
          }
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
      }
    }
  }

  clang::QualType type_loc_qt = markClangQualTypeDefined(
      readClangApiValueDefined([&]() { return type_loc.getType(); }));
  const clang::Type *type_loc_ptr =
      markClangTypeObjectDefinedByClass(type_loc_qt.getTypePtrOrNull());
  if (const clang::TemplateSpecializationType *tst =
          type_loc_ptr != nullptr
              ? markClangAstObjectDefined(readClangApiValueDefined([&]() {
                  return type_loc_ptr
                      ->getAs<clang::TemplateSpecializationType>();
                }))
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
          std::string base_name = getTemplateNameBase(tname);
          if (!base_name.empty()) {
            SgTemplateArgumentPtrList tpl_args;
            append_nonreal_template_arguments(
                tpl_args, spec_loc, resolve_template_decl(tname),
                "primary-template-written-template-id");

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

            const SgName semantic_name(buildExactTemplateInstantiationName(
                base_name, tst->template_arguments(),
                currentTemplateParameterDeclContext()));
            if (SgType *nr = build_nonreal_template_type(
                    base_name, resolve_scope(), qualifier,
                    has_template_keyword ||
                        spec_loc.getTemplateKeywordLoc().isValid(),
                    semantic_name, tpl_args, resolve_template_decl(tname))) {
              annotate_auto_type_constraints(nr);
              return finalize_spelled_type(nr);
            }
          }
          std::cerr << "REX_FRONTEND_INVARIANT[template-argument-source-"
                       "surface]: explicit template-id failed to construct "
                       "one per-use written type surface\n";
          ROSE_ABORT();
        }
        preserve_written_template_specialization_arguments(spec_loc,
                                                           resolved_type);
      }
      annotate_auto_type_constraints(resolved_type);
      return resolved_type;
    }
    clang::TemplateName tname = tst->getTemplateName();
    std::string base_name = getTemplateNameBase(tname);
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
      const SgName semantic_name(buildExactTemplateInstantiationName(
          base_name, tst->template_arguments(),
          currentTemplateParameterDeclContext()));
      if (SgType *nr = build_nonreal_template_type(
              base_name, resolve_scope(), qualifier, has_template_keyword,
              semantic_name, tpl_args, resolve_template_decl(tname))) {
        return finalize_spelled_type(nr);
      }
    }
  }

  SgType *result = buildTypeFromQualifiedType(type_loc.getType());
  annotate_auto_type_constraints(result);
  return result;
}

SgNode *ClangToSageTranslator::Traverse(const clang::Type *type) {
  type = markClangTypeObjectDefinedByClass(type);
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
      p_semantic_template_argument_expression_depth != 0,
      p_exact_template_parameter_name_stack.contextId());
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
#if DEBUG_TRAVERSE_TYPE
  std::cerr << "Traverse Type : " << type << " " << type->getTypeClassName()
            << std::endl;
#endif
  if (cached_result != nullptr) {
#if DEBUG_TRAVERSE_TYPE
    std::cerr << " already visited : node = " << cached_result << std::endl;
#endif
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
    // case clang::Type::DependentDecltype:
    //     ret_status = VisitDependentDecltypeType((clang::DependentDecltypeType
    //     *)type, &result); break;
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
    // case clang::Type::LocInfo:
    //     ret_status = VisitLocInfoType((clang::LocInfoType *)type, &result);
    //     break;
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
    //  case clang::Type::DependentTypeOfExpr:
    //      ret_status =
    //      VisitDependentTypeOfExprType((clang::DependentTypeOfExprType *)type,
    //      &result); break;
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

#if DEBUG_TRAVERSE_TYPE
  std::cerr << "Traverse(clang::Type : " << type << " ";
  std::cerr << " visit done : node = " << result << std::endl;
#endif
  return result;
}

/***************/
/* Visit Types */
/***************/

bool ClangToSageTranslator::VisitType(clang::Type *type, SgNode **node) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToSageTranslator::VisitType" << std::endl;
#endif

  if (*node == NULL) {
    std::cerr << "Runtime error: No Sage node associated with the type: "
              << type->getTypeClassName() << std::endl;
    return false;
  }
  /*
      std::cerr << "Dump type " << type->getTypeClassName() << "(" << type <<
     "): "; type->dump(); std::cerr << std::endl;
  */
  // TODO

  return true;
}

bool ClangToSageTranslator::VisitAdjustedType(
    clang::AdjustedType *adjusted_type, SgNode **node) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToSageTranslator::VisitAdjustedType" << std::endl;
#endif
  bool res = true;

  clang::QualType adjusted = adjusted_type->getAdjustedType();
  if (!adjusted.isNull()) {
    *node = buildTypeFromQualifiedType(adjusted);
  } else {
    *node = buildTypeFromQualifiedType(adjusted_type->getOriginalType());
  }

  return VisitType(adjusted_type, node) && res;
}

bool ClangToSageTranslator::VisitDecayedType(clang::DecayedType *decayed_type,
                                             SgNode **node) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToSageTranslator::VisitDecayedType" << std::endl;
#endif
  bool res = true;

  //    SgType * decayType =
  //    buildTypeFromQualifiedType(decayed_type->getDecayedType ());
  clang::QualType pointee_qual_type =
      markClangQualTypeDefined(readClangApiValueDefined(
          [&]() { return decayed_type->getPointeeType(); }));
  const clang::Type *pointee_clang_type =
      markClangTypeObjectDefinedByClass(pointee_qual_type.getTypePtrOrNull());
  SgType *pointeeType = buildTypeFromQualifiedType(pointee_qual_type);

  //    *node = pointeeType;
  // Pei-Hung (04/08/2022) Building SgArrayyType to represent the DecayedType in
  // Clang, in order to match the type of ParmVarDecl  in FunctionProtoType
  // Might need to check the case when the pointeeType is a functionType
  const clang::Type::TypeClass pointee_type_class =
      pointee_clang_type != nullptr ? pointee_clang_type->getTypeClass()
                                    : clang::Type::Builtin;
  if (pointee_type_class == clang::Type::VariableArray ||
      pointee_type_class == clang::Type::ConstantArray ||
      pointee_type_class == clang::Type::DependentSizedArray ||
      pointee_type_class == clang::Type::IncompleteArray)
    *node = SageBuilder::buildArrayType(pointeeType);
  else
    *node = pointeeType;

  return VisitAdjustedType(decayed_type, node) && res;
}

bool ClangToSageTranslator::VisitArrayType(clang::ArrayType *array_type,
                                           SgNode **node) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToSageTranslator::VisitArrayType" << std::endl;
#endif
  bool res = true;

  // Array type handling is implemented in child visitor functions
  // (ConstantArrayType, VariableArrayType, DependentSizedArrayType,
  // IncompleteArrayType) which set *node before calling this base function
  // ROSE_ASSERT(FAIL_FIXME == 0); // FIXME

  // DQ (11/28/2020): Added assertion.
  ROSE_ASSERT(*node != NULL);

  return VisitType(array_type, node) && res;
}

bool ClangToSageTranslator::VisitConstantArrayType(
    clang::ConstantArrayType *constant_array_type, SgNode **node) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToSageTranslator::VisitConstantArrayType" << std::endl;
#endif

  SgType *type =
      buildTypeFromQualifiedType(constant_array_type->getElementType());

  // TODO clang::ArrayType::ArraySizeModifier

  SgExpression *expr =
      SageBuilder::buildIntVal(constant_array_type->getSize().getSExtValue());

  *node = SageBuilder::buildArrayType(type, expr);

  return VisitArrayType(constant_array_type, node);
}

bool ClangToSageTranslator::VisitDependentSizedArrayType(
    clang::DependentSizedArrayType *dependent_sized_array_type, SgNode **node) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToSageTranslator::VisitDependentSizedArrayType"
            << std::endl;
#endif
  bool res = true;

  SgType *type =
      buildTypeFromQualifiedType(dependent_sized_array_type->getElementType());

  clang::Expr *size_expr = dependent_sized_array_type->getSizeExpr();
  if (size_expr != NULL) {
    SgNode *tmp_expr = Traverse(size_expr);
    SgExpression *array_size = isSgExpression(tmp_expr);
    ROSE_ASSERT(array_size != NULL);
    *node = SageBuilder::buildArrayType(type, array_size);
  } else {
    // Size may be deduced from an initializer; represent as unknown-size array.
    *node = SageBuilder::buildArrayType(type);
  }

  return VisitArrayType(dependent_sized_array_type, node) && res;
}

bool ClangToSageTranslator::VisitIncompleteArrayType(
    clang::IncompleteArrayType *incomplete_array_type, SgNode **node) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToSageTranslator::VisitIncompleteArrayType" << std::endl;
#endif

  SgType *type =
      buildTypeFromQualifiedType(incomplete_array_type->getElementType());

  // ArraySizeModifier moved from ArrayType:: to clang:: namespace

  clang::ArraySizeModifier sizeModifier =
      incomplete_array_type->getSizeModifier();

  if (sizeModifier == clang::ArraySizeModifier::Star) {
    SgExprListExp *exprListExp =
        SageBuilder::buildExprListExp(SageBuilder::buildNullExpression(
            SgNullExpression::e_null_expression_syntactic_absence));
    SgArrayType *arrayType = SageBuilder::buildArrayType(type, exprListExp);
    arrayType->set_is_variable_length_array(true);
    *node = arrayType;
  } else if (sizeModifier == clang::ArraySizeModifier::Static) {
    // TODO check how to handle Static
    *node = SageBuilder::buildArrayType(type);
  } else // clang::ArraySizeModifier::Normal
  {
    *node = SageBuilder::buildArrayType(type);
  }

  // DQ (11/28/2020): Added assertion.
  // ROSE_ASSERT(*node != NULL);

  return VisitArrayType(incomplete_array_type, node);
}

bool ClangToSageTranslator::VisitVariableArrayType(
    clang::VariableArrayType *variable_array_type, SgNode **node) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToSageTranslator::VisitVariableArrayType" << std::endl;
#endif
  bool res = true;

  SgType *type =
      buildTypeFromQualifiedType(variable_array_type->getElementType());

  SgNode *tmp_expr = Traverse(variable_array_type->getSizeExpr());
  SgExpression *array_size = isSgExpression(tmp_expr);

  SgArrayType *arrayType = SageBuilder::buildArrayType(type, array_size);
  arrayType->set_is_variable_length_array(true);
  *node = arrayType;

  // DQ (11/28/2020): Added assertion.
  ROSE_ASSERT(*node != NULL);

  return VisitArrayType(variable_array_type, node) && res;
}

bool ClangToSageTranslator::VisitAtomicType(clang::AtomicType *atomic_type,
                                            SgNode **node) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToSageTranslator::VisitAtomicType" << std::endl;
#endif
  bool res = true;

  SgType *base_type = buildTypeFromQualifiedType(atomic_type->getValueType());
  base_type = requireExactType(base_type, "atomic-value-type",
                               atomic_type->getValueType().getTypePtrOrNull());
  *node = base_type;

  return VisitType(atomic_type, node) && res;
}

bool ClangToSageTranslator::VisitAttributedType(
    clang::AttributedType *attributed_type, SgNode **node) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToSageTranslator::VisitAttributedType" << std::endl;
#endif

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
    SgType *equivalent_type =
        buildTypeFromQualifiedType(attributed_type->getEquivalentType());
    *node = equivalent_type != nullptr ? equivalent_type : type;
  }

  return VisitType(attributed_type, node);
}

bool ClangToSageTranslator::VisitBlockPointerType(
    clang::BlockPointerType *block_pointer_type, SgNode **node) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToSageTranslator::VisitBlockPointerType" << std::endl;
#endif
  bool res = true;

  SgType *pointee_type =
      buildTypeFromQualifiedType(block_pointer_type->getPointeeType());
  pointee_type =
      requireExactType(pointee_type, "block-pointer-pointee",
                       block_pointer_type->getPointeeType().getTypePtrOrNull());
  *node = SageBuilder::buildPointerType(pointee_type);

  return VisitType(block_pointer_type, node) && res;
}

bool ClangToSageTranslator::VisitBuiltinType(clang::BuiltinType *builtin_type,
                                             SgNode **node) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToSageTranslator::VisitBuiltinType" << std::endl;
#endif

  auto build_target_builtin = [&](SgTypeTargetBuiltin::target_family_enum
                                      target_family) {
    if (p_compiler_instance == nullptr) {
      failExactTypeTranslation("target-builtin-compiler-state", builtin_type);
    }
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
  // TODO ROSE type ?
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

  case clang::BuiltinType::Dependent: {
    SgScopeStatement *scope = SageBuilder::topScopeStack();
    if (scope == nullptr) {
      scope = getGlobalScope();
    }
    SgNonrealType *nrtype = nullptr;
    if (scope != nullptr) {
      if (SgNonrealSymbol *sym =
              scope->lookup_nonreal_symbol(SgName("__dependent_type"))) {
        if (SgNonrealDecl *nrdecl = sym->get_declaration()) {
          nrtype = nrdecl->get_type();
        }
      }
    }
    if (nrtype == nullptr) {
      nrtype = SageBuilder::buildSemanticNonrealType(SgName("__dependent_type"),
                                                     scope, nullptr, nullptr);
    }
    if (nrtype != nullptr) {
      if (SgNonrealDecl *nrdecl = isSgNonrealDecl(nrtype->get_declaration())) {
        requireExactSynthesizedProvenance(
            nrdecl, "VisitBuiltinType:dependent-type-provenance");
        requireTypedNonLexicalDeclarationOwner(
            nrdecl, "VisitBuiltinType:dependent-type-owner");
      }
    }
    if (nrtype == nullptr) {
      failExactTypeTranslation("dependent-builtin-type", builtin_type);
    }
    *node = nrtype;
    break;
  }

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
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToSageTranslator::VisitComplexType" << std::endl;
#endif

  bool res = true;

  SgType *type = buildTypeFromQualifiedType(complex_type->getElementType());

  *node = SageBuilder::buildComplexType(type);

  return VisitType(complex_type, node) && res;
}

bool ClangToSageTranslator::VisitDecltypeType(
    clang::DecltypeType *decltype_type, SgNode **node) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToSageTranslator::VisitDecltypeType" << std::endl;
#endif
  bool res = true;

  clang::QualType underlying_type =
      markClangQualTypeDefined(readClangApiValueDefined(
          [&]() { return decltype_type->getUnderlyingType(); }));
  SgType *sg_underlying_type = nullptr;
  if (!underlying_type.isNull()) {
    sg_underlying_type = buildTypeFromQualifiedType(underlying_type);
  }

  if (sg_underlying_type != nullptr) {
    auto strip_modifier_layers = [](SgType *type) -> SgType * {
      while (SgModifierType *modifier_type = isSgModifierType(type)) {
        type = modifier_type->get_base_type();
      }
      return type;
    };

    clang::QualType canonical_type =
        markClangQualTypeDefined(readClangApiValueDefined(
            [&]() { return decltype_type->getCanonicalTypeInternal(); }));
    SgType *underlying_core = strip_modifier_layers(sg_underlying_type);
    if (!canonical_type.isNull() && canonical_type->isPointerType() &&
        isSgPointerType(underlying_core) == nullptr) {
      SgType *sg_canonical_type = buildTypeFromQualifiedType(canonical_type);
      if (isSgPointerType(strip_modifier_layers(sg_canonical_type)) !=
          nullptr) {
        sg_underlying_type = sg_canonical_type;
      }
    }
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
    if (const clang::Expr *underlying_expr =
            decltype_type->getUnderlyingExpr()) {
      SgNode *expr_node = Traverse(const_cast<clang::Expr *>(underlying_expr));
      if (SgExpression *expr = isSgExpression(expr_node)) {
        SgExpression *expr_copy = prepareExpressionForAttachment(expr);
        if (expr_copy != nullptr) {
          SgType *exact_semantic_base = requireExactDecltypeSemanticBase(
              decltype_type, expr_copy, sg_underlying_type,
              "dependent-decltype-base");
          sg_decltype =
              SageBuilder::buildDeclType(expr_copy, exact_semantic_base);
        }
      }
    }
  }

  if (sg_decltype != nullptr) {
    *node = sg_decltype;
  } else if (sg_underlying_type != nullptr) {
    *node = sg_underlying_type;
  } else {
    failExactTypeTranslation("decltype-underlying-type", decltype_type);
  }

  return VisitType(decltype_type, node) && res;
}

bool ClangToSageTranslator::VisitDependentDecltypeType(
    clang::DependentDecltypeType *dependent_decltype_type, SgNode **node) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToSageTranslator::VisitDependentDecltypeType" << std::endl;
#endif
  bool res = true;

  return VisitDecltypeType(dependent_decltype_type, node) && res;
}

bool ClangToSageTranslator::VisitDeducedType(clang::DeducedType *deduced_type,
                                             SgNode **node) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToSageTranslator::VisitDeducedType" << std::endl;
#endif
  bool res = true;

  clang::QualType deduced = deduced_type->getDeducedType();
  if (!deduced.isNull()) {
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
    *node = buildTypeFromQualifiedType(semantic_deduced);
    if (*node == nullptr) {
      std::cerr << "REX_FRONTEND_INVARIANT[deduced-type]: canonical Clang "
                   "deduction result produced no Sage type"
                << std::endl;
      ROSE_ABORT();
    }
  } else if (*node == nullptr) {
    *node = SageBuilder::buildAutoType();
  }

  return VisitType(deduced_type, node) && res;
}

bool ClangToSageTranslator::VisitAutoType(clang::AutoType *auto_type,
                                          SgNode **node) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToSageTranslator::VisitAutoType" << std::endl;
#endif
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
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToSageTranslator::VisitDeducedTemplateSpecializationType"
            << std::endl;
#endif
  bool res = true;
  if (deduced_template_specialization_type == nullptr) {
    *node = nullptr;
    return false;
  }

  clang::QualType deduced =
      deduced_template_specialization_type->getDeducedType();
  if (!deduced.isNull()) {
    *node = buildTypeFromQualifiedType(deduced);
    return VisitDeducedType(deduced_template_specialization_type, node) && res;
  }

  clang::TemplateName tname =
      deduced_template_specialization_type->getTemplateName();
  std::string base_name = getTemplateNameBase(tname);
  if (base_name.empty()) {
    base_name = "__deduced_template";
  }

  SgScopeStatement *base_scope = SageBuilder::topScopeStack();
  if (base_scope == nullptr) {
    base_scope = getGlobalScope();
  }

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
        qualifier, base_scope, SgName(base_name), nullptr, nullptr);
  } else {
    *node = SageBuilder::buildSemanticNonrealType(SgName(base_name), base_scope,
                                                  nullptr, nullptr);
  }

  return VisitDeducedType(deduced_template_specialization_type, node) && res;
}

bool ClangToSageTranslator::VisitDependentAddressSpaceType(
    clang::DependentAddressSpaceType *dependent_address_space_type,
    SgNode **node) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToSageTranslator::VisitDependentAddressSpaceType"
            << std::endl;
#endif
  bool res = true;

  SgType *pointee_type = buildTypeFromQualifiedType(
      dependent_address_space_type->getPointeeType());
  pointee_type = requireExactType(
      pointee_type, "dependent-address-space-pointee",
      dependent_address_space_type->getPointeeType().getTypePtrOrNull());
  SgModifierType *modified_type = new SgModifierType(pointee_type);
  SgTypeModifier &modifier = modified_type->get_typeModifier();
  modifier.setAddressSpace();
  *node = SgModifierType::insertModifierTypeIntoTypeTable(modified_type);

  return VisitType(dependent_address_space_type, node) && res;
}

bool ClangToSageTranslator::VisitDependentSizedExtVectorType(
    clang::DependentSizedExtVectorType *dependent_sized_ext_vector_type,
    SgNode **node) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToSageTranslator::DependentSizedExtVectorType"
            << std::endl;
#endif
  bool res = true;

  SgType *elem_type = buildTypeFromQualifiedType(
      dependent_sized_ext_vector_type->getElementType());
  elem_type = requireExactType(
      elem_type, "dependent-sized-vector-element",
      dependent_sized_ext_vector_type->getElementType().getTypePtrOrNull());
  SgModifierType *modified_type = new SgModifierType(elem_type);
  SgTypeModifier &modifier = modified_type->get_typeModifier();
  modifier.setVectorType();
  if (const clang::Expr *size_expr =
          dependent_sized_ext_vector_type->getSizeExpr()) {
    if (const clang::IntegerLiteral *int_lit =
            llvm::dyn_cast<clang::IntegerLiteral>(size_expr)) {
      modifier.set_vector_size(int_lit->getValue().getSExtValue());
    }
  }
  *node = SgModifierType::insertModifierTypeIntoTypeTable(modified_type);

  return VisitType(dependent_sized_ext_vector_type, node) && res;
}

bool ClangToSageTranslator::VisitDependentVectorType(
    clang::DependentVectorType *dependent_vector_type, SgNode **node) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToSageTranslator::DependentVectorType" << std::endl;
#endif
  bool res = true;

  SgType *elem_type =
      buildTypeFromQualifiedType(dependent_vector_type->getElementType());
  elem_type = requireExactType(
      elem_type, "dependent-vector-element",
      dependent_vector_type->getElementType().getTypePtrOrNull());
  SgModifierType *modified_type = new SgModifierType(elem_type);
  SgTypeModifier &modifier = modified_type->get_typeModifier();
  modifier.setVectorType();
  if (const clang::Expr *size_expr = dependent_vector_type->getSizeExpr()) {
    if (const clang::IntegerLiteral *int_lit =
            llvm::dyn_cast<clang::IntegerLiteral>(size_expr)) {
      modifier.set_vector_size(int_lit->getValue().getSExtValue());
    }
  }
  *node = SgModifierType::insertModifierTypeIntoTypeTable(modified_type);

  return VisitType(dependent_vector_type, node) && res;
}

bool ClangToSageTranslator::VisitFunctionType(
    clang::FunctionType *function_type, SgNode **node) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToSageTranslator::VisitFunctionType" << std::endl;
#endif
  bool res = true;

  SgFunctionParameterTypeList *param_type_list =
      new SgFunctionParameterTypeList();
  SgType *ret_type = buildTypeFromQualifiedType(function_type->getReturnType());
  ret_type =
      requireExactType(ret_type, "function-return-type",
                       function_type->getReturnType().getTypePtrOrNull());
  *node = SageBuilder::buildFunctionType(ret_type, param_type_list);

  return VisitType(function_type, node) && res;
}

bool ClangToSageTranslator::VisitFunctionNoProtoType(
    clang::FunctionNoProtoType *function_no_proto_type, SgNode **node) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToSageTranslator::VisitFunctionNoProtoType" << std::endl;
#endif

  bool res = true;

  SgFunctionParameterTypeList *param_type_list =
      new SgFunctionParameterTypeList();

  SgType *ret_type =
      buildTypeFromQualifiedType(function_no_proto_type->getReturnType());

  *node = SageBuilder::buildFunctionType(ret_type, param_type_list);

  return VisitType(function_no_proto_type, node) && res;
}

bool ClangToSageTranslator::VisitFunctionProtoType(
    clang::FunctionProtoType *function_proto_type, SgNode **node) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToSageTranslator::VisitFunctionProtoType" << std::endl;
#endif

  bool res = true;
  SgFunctionParameterTypeList *param_type_list =
      new SgFunctionParameterTypeList();
  for (unsigned i = 0; i < function_proto_type->getNumParams(); i++) {
#if DEBUG_VISIT_TYPE
    std::cerr << "funcProtoType: " << i << " th param" << std::endl;
#endif
    clang::QualType clang_param_type =
        markClangQualTypeDefined(function_proto_type->getParamType(i));
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

  SgType *ret_type =
      buildTypeFromQualifiedType(function_proto_type->getReturnType());

  SgFunctionType *func_type = buildFunctionTypeForClangProto(
      ret_type, param_type_list, function_proto_type);

  *node = func_type;

  return VisitType(function_proto_type, node) && res;
}

bool ClangToSageTranslator::VisitInjectedClassNameType(
    clang::InjectedClassNameType *injected_class_name_type, SgNode **node) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToSageTranslator::InjectedClassNameType" << std::endl;
#endif
  bool res = true;

  // InjectedClassNameType is a semantic current-instantiation identity.  Its
  // canonical specialization carries the injected template arguments even
  // when source syntax inside the class may omit the template-id.  TypeLoc
  // translation records that use-site spelling separately; QualType
  // translation must never collapse the semantic type to `Class<>`.
  if (injected_class_name_type == nullptr || p_compiler_instance == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[injected-class-name-type]: exact "
                 "Clang type and AST context are required"
              << std::endl;
    ROSE_ABORT();
  }
  clang::QualType injected_qt = getInjectedClassNameSpecializationType(
      injected_class_name_type, p_compiler_instance->getASTContext());
  if (injected_qt.isNull() ||
      injected_qt.getTypePtrOrNull() == injected_class_name_type) {
    std::cerr << "REX_FRONTEND_INVARIANT[injected-class-name-type]: current "
                 "instantiation has no distinct canonical specialization"
              << std::endl;
    ROSE_ABORT();
  }
  *node = buildTypeFromQualifiedType(injected_qt);
  if (*node == nullptr || isSgType(*node) == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[injected-class-name-type]: canonical "
                 "current instantiation did not translate to an exact type"
              << std::endl;
    ROSE_ABORT();
  }

  return VisitType(injected_class_name_type, node) && res;
}

// LocInfoType was removed in LLVM
/*
bool ClangToSageTranslator::VisitLocInfoType(clang::LocInfoType * loc_info_type,
SgNode ** node) { #if DEBUG_VISIT_TYPE std::cerr <<
"ClangToSageTranslator::LocInfoType" << std::endl; #endif bool res = true;

    ROSE_ASSERT(FAIL_FIXME == 0); // FIXME

    return VisitType(loc_info_type, node) && res;
}
*/

bool ClangToSageTranslator::VisitMacroQualifiedType(
    clang::MacroQualifiedType *macro_qualified_type, SgNode **node) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToSageTranslator::MacroQualifiedType" << std::endl;
#endif
  bool res = true;

  clang::QualType desugared = macro_qualified_type->desugar();
  if (desugared.isNull()) {
    desugared = macro_qualified_type->getUnderlyingType();
  }
  *node = buildTypeFromQualifiedType(desugared);

  return VisitType(macro_qualified_type, node) && res;
}

bool ClangToSageTranslator::VisitMemberPointerType(
    clang::MemberPointerType *member_pointer_type, SgNode **node) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToSageTranslator::MemberPointerType" << std::endl;
  std::cerr << "isMemberFunctionPointer  "
            << member_pointer_type->isMemberFunctionPointer() << std::endl;
  std::cerr << "isMemberDataPointer  "
            << member_pointer_type->isMemberDataPointer() << std::endl;
  std::cerr << "isSugared  " << member_pointer_type->isSugared() << std::endl;
#endif
  bool res = true;
  auto build_translated_member_pointer_class_type =
      [&](const clang::CXXRecordDecl *record) -> SgType * {
    if (record == nullptr) {
      return nullptr;
    }
    return getTypeFromTranslatedRecordDecl(
        const_cast<clang::CXXRecordDecl *>(record));
  };

  auto build_class_type_from_member_pointer_qualifier =
      [&](clang::NestedNameSpecifier qualifier,
          const clang::NamedDecl *associated_decl) -> SgType * {
    clang::NestedNameSpecifier prefix = std::nullopt;
    std::string name_str;
    SgTemplateArgumentPtrList tpl_args;
    const SgTemplateArgumentPtrList *tpl_args_ptr = nullptr;
    SgName semantic_name;
    const SgName *semantic_name_ptr = nullptr;
    if (qualifier) {
      prefix = nestedNameSpecifierPrefix(qualifier);
      if (qualifier.getKind() == clang::NestedNameSpecifier::Kind::Type) {
        if (const clang::Type *qualifier_type = qualifier.getAsType()) {
          if (const auto *spec =
                  llvm::dyn_cast<clang::TemplateSpecializationType>(
                      qualifier_type)) {
            name_str = getTemplateNameBase(spec->getTemplateName());
            tpl_args = buildTemplateArguments(spec, true);
            tpl_args_ptr = &tpl_args;
            semantic_name = SgName(buildExactTemplateInstantiationName(
                name_str, spec->template_arguments(),
                currentTemplateParameterDeclContext()));
            semantic_name_ptr = &semantic_name;
          } else if (const auto *tag =
                         llvm::dyn_cast<clang::TagType>(qualifier_type)) {
            name_str = tag->getDecl()->getNameAsString();
          } else if (const auto *typedef_type =
                         llvm::dyn_cast<clang::TypedefType>(qualifier_type)) {
            name_str = typedef_type->getDecl()->getNameAsString();
          } else if (const auto *using_type =
                         llvm::dyn_cast<clang::UsingType>(qualifier_type)) {
            if (clang::UsingShadowDecl *using_decl = using_type->getDecl()) {
              name_str = using_decl->getNameAsString();
            }
          } else if (const auto *injected =
                         llvm::dyn_cast<clang::InjectedClassNameType>(
                             qualifier_type)) {
            name_str = injected->getDecl()->getNameAsString();
          }
        }
      }
    }

    if (name_str.empty() && associated_decl != nullptr) {
      name_str = associated_decl->getNameAsString();
    }
    if (name_str.empty()) {
      return nullptr;
    }

    SgScopeStatement *lookup_scope = SageBuilder::topScopeStack();
    while (lookup_scope != nullptr && isSgGlobal(lookup_scope) == nullptr &&
           isSgNamespaceDefinitionStatement(lookup_scope) == nullptr) {
      lookup_scope = SageInterface::getEnclosingScope(lookup_scope, false);
    }
    if (lookup_scope == nullptr) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[member-pointer-lookup-scope]: Clang "
              "member-pointer type=%p class=%s has no enclosing Sage lookup "
              "scope\n",
              static_cast<void *>(member_pointer_type), name_str.c_str());
      ROSE_ABORT();
    }

    return buildSemanticNonrealTypeFromNestedNameSpecifier(
        prefix, lookup_scope, SgName(name_str), tpl_args_ptr,
        semantic_name_ptr);
  };

  clang::QualType classQualType;
  SgType *classType = nullptr;
  if (clang::NestedNameSpecifier qualifier =
          member_pointer_type->getQualifier();
      qualifier) {
    if (const clang::Type *qual_type = qualifier.getAsType()) {
      if (const clang::CXXRecordDecl *record =
              cxxRecordDeclFromQualTypeWithoutDefinitionLookup(
                  clang::QualType(qual_type, 0))) {
        classType = build_translated_member_pointer_class_type(record);
        if (classType == nullptr) {
          classType =
              build_class_type_from_member_pointer_qualifier(qualifier, record);
        }
      }
      if (classType == nullptr) {
        SgScopeStatement *scope = SageBuilder::topScopeStack();
        if (scope == nullptr) {
          fprintf(stderr,
                  "REX_FRONTEND_INVARIANT[member-pointer-type-scope]: Clang "
                  "member-pointer type=%p has no active Sage scope\n",
                  static_cast<void *>(member_pointer_type));
          ROSE_ABORT();
        }
        classType = buildNonrealTypeForNestedNameSpecifierType(
            qual_type, scope, /*prefer_current_scope=*/false);
      }
    }
  }
  if (clang::CXXRecordDecl *record =
          member_pointer_type->getMostRecentCXXRecordDecl();
      classType == nullptr && record != nullptr) {
    classType = build_translated_member_pointer_class_type(record);
    if (classType == nullptr) {
      classType = build_class_type_from_member_pointer_qualifier(
          member_pointer_type->getQualifier(), record);
    }
  }
  if (clang::NestedNameSpecifier qualifier =
          member_pointer_type->getQualifier()) {
    if (const clang::Type *qual_type = qualifier.getAsType()) {
      classQualType = clang::QualType(qual_type, 0);
    } else if (clang::CXXRecordDecl *record =
                   member_pointer_type->getMostRecentCXXRecordDecl()) {
      classQualType = record->getASTContext().getTypeDeclType(
          static_cast<const clang::TypeDecl *>(record));
    }
  }
  if (classType == nullptr) {
    if (!classQualType.isNull()) {
      classType = buildTypeFromQualifiedType(classQualType);
    }
  }
  const clang::Type *class_type_ptr = classQualType.getTypePtrOrNull();
  SgType *classTypeStripped =
      classType != NULL ? classType->stripTypedefsAndModifiers() : NULL;
  auto is_exact_member_pointer_class_type = [](SgType *type) -> bool {
    if (type == nullptr) {
      return false;
    }
    if (isSgClassType(type) != nullptr || isSgNonrealType(type) != nullptr) {
      return true;
    }
    if (isSgTypedefType(type) != nullptr) {
      return true;
    }
    return false;
  };
  if (!is_exact_member_pointer_class_type(classTypeStripped)) {
    SgScopeStatement *member_pointer_scope = SageBuilder::topScopeStack();
    ASSERT_not_null(member_pointer_scope);
    if (class_type_ptr != nullptr) {
      if (SgNonrealType *nrtype = buildNonrealTypeForNestedNameSpecifierType(
              class_type_ptr, member_pointer_scope,
              /*prefer_current_scope=*/true)) {
        classType = nrtype;
      }
    }
    classTypeStripped =
        classType != NULL ? classType->stripTypedefsAndModifiers() : NULL;
    if (!is_exact_member_pointer_class_type(classTypeStripped)) {
      failExactTypeTranslation("member-pointer-class", member_pointer_type);
    }
  }
  classType =
      requireExactType(classType, "member-pointer-class", member_pointer_type);

  clang::QualType pointee_type = member_pointer_type->getPointeeType();
  SgType *baseType = buildTypeFromQualifiedType(pointee_type);
  ROSE_ASSERT(baseType);
  if (member_pointer_type->isMemberFunctionPointer()) {
    unsigned int mfunc_specifier = 0;
    if (const clang::FunctionProtoType *proto =
            pointee_type->getAs<clang::FunctionProtoType>()) {
      clang::Qualifiers qualifiers = proto->getMethodQuals();
      if (qualifiers.hasConst()) {
        mfunc_specifier |= SgMemberFunctionType::e_const;
      }
      if (qualifiers.hasVolatile()) {
        mfunc_specifier |= SgMemberFunctionType::e_volatile;
      }
      if (qualifiers.hasRestrict()) {
        mfunc_specifier |= SgMemberFunctionType::e_restrict;
      }
      switch (proto->getRefQualifier()) {
      case clang::RQ_LValue:
        mfunc_specifier |= SgMemberFunctionType::e_ref_qualifier_lvalue;
        break;
      case clang::RQ_RValue:
        mfunc_specifier |= SgMemberFunctionType::e_ref_qualifier_rvalue;
        break;
      case clang::RQ_None:
        break;
      }
    }
    SgFunctionType *functionType = isSgFunctionType(baseType);
    if (functionType != NULL) {
      SgMemberFunctionType *memFuncType =
          buildMemberFunctionTypeWithClonedArguments(
              functionType, classType, mfunc_specifier,
              "qualified-type-member-function-pointer");
      baseType = memFuncType;
      ROSE_ASSERT(baseType);
    }
  }

  SgPointerMemberType *pointerToMemberType =
      SageBuilder::buildPointerMemberType(baseType, classType);

  *node = pointerToMemberType;

  return VisitType(member_pointer_type, node) && res;
}

bool ClangToSageTranslator::VisitPackExpansionType(
    clang::PackExpansionType *pack_expansion_type, SgNode **node) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToSageTranslator::PackExpansionType" << std::endl;
#endif
  bool res = true;

  // ROOT CAUSE FIX: Pack expansion types (e.g., Args... in variadic templates)
  // represent template parameter packs that are expanded
  // Try to get the pattern type (the type being expanded)
  clang::QualType pattern = pack_expansion_type->getPattern();
  SgType *pattern_type = buildTypeFromQualifiedType(pattern);

  pattern_type = requireExactType(pattern_type, "pack-expansion-pattern",
                                  pattern.getTypePtrOrNull());
  if (SgTemplateType *parameter_type = isSgTemplateType(pattern_type);
      parameter_type != nullptr && parameter_type->get_packed()) {
    SgTemplateType *use_type = new SgTemplateType(parameter_type->get_name());
    use_type->set_template_parameter_position(
        parameter_type->get_template_parameter_position());
    use_type->set_template_parameter_depth(
        parameter_type->get_template_parameter_depth());
    if (parameter_type->get_canonical_source_identity().has_value()) {
      use_type->initialize_canonical_source_identity(
          *parameter_type->get_canonical_source_identity());
    }
    use_type->set_class_type(parameter_type->get_class_type());
    use_type->set_parent_class_type(parameter_type->get_parent_class_type());
    use_type->set_template_parameter(parameter_type->get_template_parameter());
    use_type->get_tpl_args() = parameter_type->get_tpl_args();
    use_type->get_part_spec_tpl_args() =
        parameter_type->get_part_spec_tpl_args();
    use_type->set_packed(false);
    pattern_type = use_type;
  }
  // A PackExpansionType is a use-site expansion.  The canonical pattern type
  // can be shared by non-expanding uses, so mutating it would leak an ellipsis
  // into every use of the declaration.  Expansion is represented by the exact
  // enclosing template argument/declarator syntax instead.
  *node = pattern_type;

  return VisitType(pack_expansion_type, node) && res;
}

bool ClangToSageTranslator::VisitParenType(clang::ParenType *paren_type,
                                           SgNode **node) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToSageTranslator::VisitParenType" << std::endl;
  std::cerr << "isSugared " << paren_type->isSugared() << std::endl;
#endif

  if (paren_type->isSugared())
    *node = buildTypeFromQualifiedType(paren_type->desugar());
  else
    *node = buildTypeFromQualifiedType(paren_type->getInnerType());

  return VisitType(paren_type, node);
}

bool ClangToSageTranslator::VisitPipeType(clang::PipeType *pipe_type,
                                          SgNode **node) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToSageTranslator::PipeType" << std::endl;
#endif
  bool res = true;

  SgType *elem_type = buildTypeFromQualifiedType(pipe_type->getElementType());
  elem_type = requireExactType(elem_type, "pipe-element-type",
                               pipe_type->getElementType().getTypePtrOrNull());
  *node = SageBuilder::buildPointerType(elem_type);

  return VisitType(pipe_type, node) && res;
}

bool ClangToSageTranslator::VisitPointerType(clang::PointerType *pointer_type,
                                             SgNode **node) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToSageTranslator::VisitPointerType" << std::endl;
#endif

  SgType *type = buildTypeFromQualifiedType(pointer_type->getPointeeType());

  *node = SageBuilder::buildPointerType(type);

  return VisitType(pointer_type, node);
}

bool ClangToSageTranslator::VisitReferenceType(
    clang::ReferenceType *reference_type, SgNode **node) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToSageTranslator::ReferenceType" << std::endl;
#endif

  reference_type = const_cast<clang::ReferenceType *>(
      llvm::dyn_cast_or_null<clang::ReferenceType>(
          markClangTypeObjectDefinedByClass(reference_type)));
  ROSE_ASSERT(reference_type != nullptr);

  // A semantic QualType contains the already-collapsed reference.  Rebuilding
  // getPointeeTypeAsWritten() here preserved an inner reference from a
  // dependent spelling such as T& with T substituted by U&, manufacturing an
  // illegal SgReferenceType-to-SgReferenceType chain.  Written reference
  // structure belongs exclusively to buildTypeFromTypeLoc(); this visitor
  // must translate Clang's exact collapsed pointee.
  clang::QualType pointee_qual_type =
      markClangQualTypeDefined(readClangApiValueDefined(
          [&]() { return reference_type->getPointeeType(); }));
  if (pointee_qual_type.isNull() || readClangApiValueDefined([&]() {
        return pointee_qual_type->isReferenceType();
      })) {
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
  if (pointee_type == nullptr || semantic_pointee == nullptr ||
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
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToSageTranslator::LValueReferenceType" << std::endl;
#endif
  return VisitReferenceType(lvalue_reference_type, node);
}

bool ClangToSageTranslator::VisitRValueReferenceType(
    clang::RValueReferenceType *rvalue_reference_type, SgNode **node) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToSageTranslator::RValueReferenceType" << std::endl;
#endif
  return VisitReferenceType(rvalue_reference_type, node);
}

bool ClangToSageTranslator::VisitSubstTemplateTypeParmPackType(
    clang::SubstTemplateTypeParmPackType *subst_template_type, SgNode **node) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToSageTranslator::SubstTemplateTypeParmPackType"
            << std::endl;
#endif
  bool res = true;

  std::string name;
  if (const clang::TemplateTypeParmDecl *decl =
          subst_template_type->getReplacedParameter()) {
    name = normalizeTemplateTypeParamName(decl->getNameAsString());
  }
  if (name.empty()) {
    if (const clang::IdentifierInfo *id =
            subst_template_type->getIdentifier()) {
      name = normalizeTemplateTypeParamName(id->getName().str());
    }
  }
  if (name.empty()) {
    name = "__template_pack";
  }

  SgTemplateType *pack_type = SageBuilder::buildTemplateType(SgName(name));
  if (pack_type == nullptr) {
    failExactTypeTranslation("substituted-template-type-pack",
                             subst_template_type);
  }
  // This node denotes the substituted parameter-pack identity.  It does not
  // own an ellipsis token; only a surrounding PackExpansionType or declarator
  // can provide that source syntax.
  pack_type->set_packed(false);
  clang::TemplateArgument pack_arg = subst_template_type->getArgumentPack();
  if (pack_arg.getKind() == clang::TemplateArgument::Pack) {
    for (const clang::TemplateArgument &arg : pack_arg.pack_elements()) {
      appendTemplateArguments(pack_type->get_tpl_args(), arg, false);
    }
  } else {
    appendTemplateArguments(pack_type->get_tpl_args(), pack_arg, false);
  }
  for (SgTemplateArgument *arg : pack_type->get_tpl_args()) {
    if (arg != nullptr) {
      arg->set_parent(pack_type);
    }
  }
  *node = pack_type;

  return VisitType(subst_template_type, node) && res;
}

bool ClangToSageTranslator::VisitSubstTemplateTypeParmType(
    clang::SubstTemplateTypeParmType *subst_template_type_parm_type,
    SgNode **node) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToSageTranslator::SubstTemplateTypeParmType" << std::endl;
#endif

  clang::QualType replacement_type =
      subst_template_type_parm_type->getReplacementType();
  *node = buildTypeFromQualifiedType(replacement_type);

  return VisitType(subst_template_type_parm_type, node);
}

bool ClangToSageTranslator::VisitTagType(clang::TagType *tag_type,
                                         SgNode **node) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToSageTranslator::VisitTagType" << std::endl;
#endif
  bool res = true;

  auto translated_tag_type_if_available =
      [&](clang::TagDecl *tag_decl) -> SgType * {
    if (tag_decl == nullptr) {
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

  if (tag_type->getQualifier()) {
    if (!tag_type->isDependentType()) {
      if (SgType *translated_type =
              translated_tag_type_if_available(tag_type->getDecl())) {
        *node = translated_type;
        return VisitType(tag_type, node) && res;
      }
    }
    SgScopeStatement *scope = SageBuilder::topScopeStack();
    if (scope == nullptr) {
      scope = getGlobalScope();
    }
    if (SgNonrealType *written_type =
            buildNonrealTypeForNestedNameSpecifierType(
                tag_type, scope, /*prefer_current_scope=*/false)) {
      *node = written_type;
      return VisitType(tag_type, node) && res;
    }
  }

  if (clang::RecordType *record_type =
          llvm::dyn_cast<clang::RecordType>(tag_type)) {
    return VisitRecordType(record_type, node) && res;
  }
  if (clang::EnumType *enum_type = llvm::dyn_cast<clang::EnumType>(tag_type)) {
    return VisitEnumType(enum_type, node) && res;
  }

  if (clang::TagDecl *tag_decl = tag_type->getDecl()) {
    if (SgNode *decl_node = Traverse(tag_decl)) {
      if (SgClassDeclaration *class_decl = isSgClassDeclaration(decl_node)) {
        *node = class_decl->get_type();
      } else if (SgEnumDeclaration *enum_decl =
                     isSgEnumDeclaration(decl_node)) {
        *node = enum_decl->get_type();
      }
    }
  }
  if (*node == nullptr) {
    failExactTypeTranslation("tag-type", tag_type);
  }

  return VisitType(tag_type, node) && res;
}

bool ClangToSageTranslator::VisitEnumType(clang::EnumType *enum_type,
                                          SgNode **node) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToSageTranslator::VisitEnumType" << std::endl;
  std::cerr << "VisitEnumType isSugared: " << enum_type->isSugared()
            << std::endl;
#endif

  auto canonical_enum_decl =
      [](SgEnumDeclaration *decl) -> SgEnumDeclaration * {
    if (decl == nullptr) {
      return nullptr;
    }
    if (SgEnumDeclaration *first =
            isSgEnumDeclaration(decl->get_firstNondefiningDeclaration())) {
      return first;
    }
    return decl;
  };
  auto scope_has_external_named_type_identity = [](SgScopeStatement *scope) {
    std::unordered_set<SgScopeStatement *> visited;
    while (scope != nullptr && visited.insert(scope).second) {
      if (isSgGlobal(scope) != nullptr) {
        return true;
      }
      if (SgNamespaceDefinitionStatement *namespace_definition =
              isSgNamespaceDefinitionStatement(scope)) {
        SgNamespaceDeclarationStatement *namespace_declaration =
            namespace_definition->get_namespaceDeclaration();
        if (namespace_declaration == nullptr ||
            namespace_declaration->get_name().is_null()) {
          return false;
        }
        scope = namespace_declaration->get_scope();
        continue;
      }
      if (SgClassDefinition *class_definition = isSgClassDefinition(scope)) {
        SgClassDeclaration *class_declaration =
            class_definition->get_declaration();
        if (class_declaration == nullptr ||
            class_declaration->get_isUnNamed() ||
            class_declaration->get_name().is_null()) {
          return false;
        }
        scope = class_declaration->get_scope();
        continue;
      }
      return false;
    }
    return false;
  };
  auto is_exact_cross_translation_unit_enum_identity =
      [&](SgEnumDeclaration *current, SgEnumDeclaration *type_owner,
          SgEnumType *type) {
        if (current == nullptr || type_owner == nullptr || type == nullptr ||
            current == type_owner || current->get_type() != type ||
            type_owner->get_type() != type || current->get_isUnNamed() ||
            type_owner->get_isUnNamed() || current->get_name().is_null() ||
            current->get_name() != type_owner->get_name() ||
            current->get_isScopedEnum() != type_owner->get_isScopedEnum() ||
            current->get_qualified_name() != type_owner->get_qualified_name() ||
            !scope_has_external_named_type_identity(current->get_scope()) ||
            !scope_has_external_named_type_identity(type_owner->get_scope())) {
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
               owner_file != nullptr && current_file != owner_file &&
               !type->get_mangled().is_null();
      };
  clang::EnumDecl *clang_decl = markClangSpecificDeclDefined(
      readClangApiValueDefined([&]() { return enum_type->getDecl(); }));
  if (clang_decl == nullptr) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[enum-type]: Clang enum type has no exact "
            "declaration\n");
    ROSE_ABORT();
  }

  SgDeclarationStatement *translated =
      lookupSgDeclarationForClangDecl(clang_decl,
                                      /*allow_on_demand=*/false);
  const bool first_seen_in_type = translated == nullptr;
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
  rememberEnumTypeFirstSeenState(p_enum_type_decl_first_see_in_type, exact_type,
                                 first_seen_in_type);

  return VisitType(enum_type, node);
}

bool ClangToSageTranslator::VisitRecordType(clang::RecordType *record_type,
                                            SgNode **node) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToSageTranslator::VisitRecordType" << std::endl;
#endif

  record_type = const_cast<clang::RecordType *>(
      llvm::dyn_cast<clang::RecordType>(markClangTypeObjectDefinedByClass(
          static_cast<const clang::Type *>(record_type))));
  if (record_type == nullptr || node == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[record-type]: null Clang record type "
                 "or output slot"
              << std::endl;
    ROSE_ABORT();
  }

  clang::RecordDecl *record_decl = markClangRecordDeclDefined(
      readClangApiValueDefined([&]() { return record_type->getDecl(); }));
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
  clang::RecordDecl *lookup_decl = record_decl;
  if (!is_specialization) {
    clang::TagDecl *canonical =
        const_cast<clang::TagDecl *>(llvm::dyn_cast_or_null<clang::TagDecl>(
            markClangDeclObjectDefinedByKind(readClangApiValueDefined(
                [&]() { return record_decl->getCanonicalDecl(); }))));
    if (clang::RecordDecl *canonical_record =
            llvm::dyn_cast_or_null<clang::RecordDecl>(canonical)) {
      lookup_decl = canonical_record;
    }
  }

  auto exact_class_declaration =
      [&](SgDeclarationStatement *translated) -> SgClassDeclaration * {
    if (translated == nullptr) {
      return nullptr;
    }
    if (SgTemplateInstantiationDirectiveStatement *directive =
            isSgTemplateInstantiationDirectiveStatement(translated)) {
      translated = directive->get_declaration();
    }
    SgClassDeclaration *decl =
        translatedRecordTypeDeclaration(record_decl, translated);
    if (decl == nullptr && lookup_decl != record_decl) {
      decl = translatedRecordTypeDeclaration(lookup_decl, translated);
    }
    if (decl == nullptr) {
      decl = isSgClassDeclaration(translated);
    }
    if (decl == nullptr) {
      return nullptr;
    }
    if (SgClassDeclaration *first =
            isSgClassDeclaration(decl->get_firstNondefiningDeclaration())) {
      return first;
    }
    return decl;
  };

  const bool active_source_declarator_tag =
      activeSourceDeclaratorTagScope(lookup_decl) != nullptr;
  SgDeclarationStatement *translated = lookupSgDeclarationForClangDecl(
      lookup_decl, /*allow_on_demand=*/active_source_declarator_tag);
  SgClassDeclaration *class_decl = exact_class_declaration(translated);
  const bool had_exact_symbol_before_translation =
      class_decl != nullptr &&
      class_decl->get_symbol_from_symbol_table() != nullptr;

  if (class_decl == nullptr) {
    bool defer_specialization_population = false;
    clang::ClassTemplateSpecializationDecl *specialization =
        llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(lookup_decl);
    if (specialization != nullptr &&
        !llvm::isa<clang::ClassTemplatePartialSpecializationDecl>(
            specialization)) {
      clang::ClassTemplateDecl *primary = markClangSpecificDeclDefined(
          specialization->getSpecializedTemplate());
      const clang::CXXRecordDecl *pattern =
          primary != nullptr ? llvm::cast_or_null<clang::CXXRecordDecl>(
                                   markClangDeclObjectDefinedByKind(
                                       primary->getTemplatedDecl()))
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
    class_decl = exact_class_declaration(translated);
    if (defer_specialization_population) {
      queuePendingImplicitClassTemplateSpecialization(specialization);
    }
  }

  if (class_decl == nullptr) {
    clang::SourceRange lookup_range = readClangApiValueDefined(
        [&]() { return lookup_decl->getSourceRange(); });
    clang::SourceLocation lookup_location =
        readClangApiValueDefined([&]() { return lookup_decl->getLocation(); });
    SgScopeStatement *body_owner =
        !p_function_body_translation_owner_stack.empty()
            ? p_function_body_translation_owner_stack.back().sage_owner
            : nullptr;
    size_t contextual_name_matches = 0;
    for (const auto &entry : p_contextual_local_type_declaration_map) {
      if (std::get<4>(entry.first) == lookup_decl->getNameAsString()) {
        ++contextual_name_matches;
        SgScopeStatement *contextual_owner =
            reinterpret_cast<SgScopeStatement *>(std::get<0>(entry.first));
        std::cerr << "  contextual-local-type owner=" << contextual_owner << "/"
                  << (contextual_owner != nullptr
                          ? contextual_owner->class_name()
                          : std::string("<null>"))
                  << " begin=" << std::get<1>(entry.first)
                  << " location=" << std::get<2>(entry.first)
                  << " end=" << std::get<3>(entry.first)
                  << " declaration=" << entry.second << "/"
                  << (entry.second != nullptr ? entry.second->class_name()
                                              : std::string("<null>"))
                  << " scope="
                  << (entry.second != nullptr ? entry.second->get_scope()
                                              : nullptr)
                  << std::endl;
      }
    }
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
              << " contextual-name-matches=" << contextual_name_matches
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
  if (class_symbol == nullptr && class_decl->get_scope() != nullptr) {
    // A source-level explicit-instantiation declaration can own the symbol
    // while a compiler-generated semantic declaration owns the canonical
    // class type.  Both are exact surfaces of the same specialization.  Use
    // the specialization's exact scope/key and require the symbol surface to
    // share that canonical type.
    class_symbol = class_decl->get_scope()->lookup_class_symbol(
        class_decl->get_name(), nullptr);
    if (class_symbol != nullptr) {
      SgClassDeclaration *symbol_decl = class_symbol->get_declaration();
      if (symbol_decl == nullptr || symbol_decl->get_type() != class_type) {
        std::cerr
            << "REX_FRONTEND_INVARIANT[record-type]: exact declaration '"
            << lookup_decl->getQualifiedNameAsString()
            << "' has a scope symbol bound to a different canonical class "
               "type"
            << std::endl;
        ROSE_ABORT();
      }
    }
  }
  if (class_symbol == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[record-type]: exact declaration '"
              << lookup_decl->getQualifiedNameAsString()
              << "' has no SgClassSymbol" << std::endl;
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
  rememberClassTypeFirstSeenState(p_class_type_decl_first_see_in_type,
                                  class_type,
                                  !had_exact_symbol_before_translation);

  return VisitType(record_type, node);
}

SgTemplateClassDeclaration *
ClangToSageTranslator::lookupTranslatedTemplateDeclarationForRecord(
    clang::CXXRecordDecl *record) {
  for (clang::CXXRecordDecl *candidate :
       {record, record != nullptr ? record->getCanonicalDecl() : nullptr}) {
    if (candidate == nullptr) {
      continue;
    }

    if (SgTemplateClassDeclaration *template_decl =
            getTemplateDeclarationForSgDecl(
                lookupSgDeclarationForClangDecl(candidate,
                                                /*allow_on_demand=*/false))) {
      return template_decl;
    }
  }

  return nullptr;
}

SgTemplateClassDeclaration *
ClangToSageTranslator::requireExactTemplateDeclaration(
    const std::string &template_name,
    const clang::TemplateSpecializationType *clang_type) {
  if (clang_type == nullptr) {
    std::cerr
        << "REX_FRONTEND_INVARIANT[template-specialization-primary-identity]: "
           "template specialization '"
        << template_name << "' has no Clang type" << std::endl;
    ROSE_ABORT();
  }

  clang::TemplateName template_name_node =
      markClangTemplateNameDefined(clang_type->getTemplateName());
  clang::TemplateDecl *clang_template_decl =
      template_name_node.getAsTemplateDecl();
  clang_template_decl = const_cast<clang::TemplateDecl *>(
      llvm::dyn_cast_or_null<clang::TemplateDecl>(
          markClangDeclObjectDefinedByKind(clang_template_decl)));
  if (clang_template_decl == nullptr) {
    std::cerr
        << "REX_FRONTEND_INVARIANT[template-specialization-primary-identity]: "
           "template specialization '"
        << template_name << "' has no exact Clang TemplateDecl" << std::endl;
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
void setIntegralLiteralValueString(SgExpression *expr,
                                   const std::string &text) {
  if (expr == nullptr) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[template-integral-literal-spelling]: "
            "cannot publish value spelling '%s' on a null expression\n",
            text.c_str());
    ROSE_ABORT();
  }

  if (SgCharVal *char_val = isSgCharVal(expr)) {
    char_val->set_valueString(text);
  } else if (SgWcharVal *wchar_val = isSgWcharVal(expr)) {
    wchar_val->set_valueString(text);
  } else if (SgChar16Val *char16_val = isSgChar16Val(expr)) {
    char16_val->set_valueString(text);
  } else if (SgChar32Val *char32_val = isSgChar32Val(expr)) {
    char32_val->set_valueString(text);
  } else if (SgSignedCharVal *signed_char_val = isSgSignedCharVal(expr)) {
    signed_char_val->set_valueString(text);
  } else if (SgUnsignedCharVal *unsigned_char_val = isSgUnsignedCharVal(expr)) {
    unsigned_char_val->set_valueString(text);
  } else if (SgShortVal *short_val = isSgShortVal(expr)) {
    short_val->set_valueString(text);
  } else if (SgUnsignedShortVal *unsigned_short_val =
                 isSgUnsignedShortVal(expr)) {
    unsigned_short_val->set_valueString(text);
  } else if (SgIntVal *int_val = isSgIntVal(expr)) {
    int_val->set_valueString(text);
  } else if (SgUnsignedIntVal *unsigned_int_val = isSgUnsignedIntVal(expr)) {
    unsigned_int_val->set_valueString(text);
  } else if (SgLongIntVal *long_val = isSgLongIntVal(expr)) {
    long_val->set_valueString(text);
  } else if (SgUnsignedLongVal *unsigned_long_val = isSgUnsignedLongVal(expr)) {
    unsigned_long_val->set_valueString(text);
  } else if (SgLongLongIntVal *long_long_val = isSgLongLongIntVal(expr)) {
    long_long_val->set_valueString(text);
  } else if (SgUnsignedLongLongIntVal *unsigned_long_long_val =
                 isSgUnsignedLongLongIntVal(expr)) {
    unsigned_long_long_val->set_valueString(text);
  }
}

// Build a literal expression from an APSInt while preserving sign and (as text)
// width.
SgExpression *buildIntegralTemplateArgExpr(const llvm::APSInt &value,
                                           SgType *int_type) {
  auto finish_semantic_literal =
      [int_type](SgExpression *literal) -> SgExpression * {
    SgValueExp *value_expression = isSgValueExp(literal);
    if (value_expression == nullptr || int_type == nullptr) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[template-argument-integral-literal]: "
              "integral semantic argument has no exact literal and converted "
              "type\n");
      ROSE_ABORT();
    }
    value_expression->set_literal_type(int_type);
    if (value_expression->get_type() != int_type) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[template-argument-integral-literal]: "
              "integral semantic literal did not retain its exact converted "
              "type\n");
      ROSE_ABORT();
    }
    return literal;
  };

  if (int_type != nullptr && isSgTypeBool(int_type)) {
    return finish_semantic_literal(
        SageBuilder::buildBoolValExp_nfi(value.getBoolValue()));
  }

  SgType *base_type = int_type;
  if (base_type != nullptr) {
    base_type = base_type->stripType(SgType::STRIP_MODIFIER_TYPE |
                                     SgType::STRIP_TYPEDEF_TYPE);
  }

  SgExpression *expr = NULL;
  auto get_signed_value = [&]() -> long long {
    return value.getBitWidth() <= 64 ? value.getSExtValue() : 0;
  };
  auto get_unsigned_value = [&]() -> unsigned long long {
    return value.getBitWidth() <= 64 ? value.getZExtValue() : 0;
  };
  const bool is_character_like_type =
      base_type != nullptr && (isSgTypeChar(base_type) != nullptr ||
                               isSgTypeSignedChar(base_type) != nullptr ||
                               isSgTypeUnsignedChar(base_type) != nullptr ||
                               isSgTypeWchar(base_type) != nullptr ||
                               isSgTypeChar16(base_type) != nullptr ||
                               isSgTypeChar32(base_type) != nullptr);

  if (base_type != nullptr) {
    if (isSgTypeChar(base_type) != nullptr) {
      long long raw = value.isSigned()
                          ? get_signed_value()
                          : static_cast<long long>(get_unsigned_value());
      expr = SageBuilder::buildCharVal_nfi(static_cast<char>(raw), "");
    } else if (isSgTypeSignedChar(base_type) != nullptr) {
      expr = SageBuilder::buildSignedCharVal_nfi(
          static_cast<signed char>(get_signed_value()), "");
    } else if (isSgTypeUnsignedChar(base_type) != nullptr) {
      expr = SageBuilder::buildUnsignedCharVal_nfi(
          static_cast<unsigned char>(get_unsigned_value()), "");
    } else if (isSgTypeWchar(base_type) != nullptr) {
      unsigned long long raw =
          value.isSigned() ? static_cast<unsigned long long>(get_signed_value())
                           : get_unsigned_value();
      expr = SageBuilder::buildWcharVal_nfi(static_cast<wchar_t>(raw), "");
    } else if (isSgTypeChar16(base_type) != nullptr) {
      expr = SageBuilder::buildChar16Val_nfi(
          static_cast<unsigned short>(get_unsigned_value()), "");
    } else if (isSgTypeChar32(base_type) != nullptr) {
      expr = SageBuilder::buildChar32Val_nfi(
          static_cast<unsigned int>(get_unsigned_value()), "");
    } else if (isSgTypeShort(base_type) != nullptr) {
      expr = SageBuilder::buildShortVal_nfi(
          static_cast<short>(get_signed_value()), "");
    } else if (isSgTypeUnsignedShort(base_type) != nullptr) {
      expr = SageBuilder::buildUnsignedShortVal_nfi(
          static_cast<unsigned short>(get_unsigned_value()), "");
    } else if (isSgTypeInt(base_type) != nullptr) {
      expr = SageBuilder::buildIntVal_nfi(static_cast<int>(get_signed_value()));
    } else if (isSgTypeUnsignedInt(base_type) != nullptr) {
      expr = SageBuilder::buildUnsignedIntVal_nfi(
          static_cast<unsigned int>(get_unsigned_value()), "");
    } else if (isSgTypeLong(base_type) != nullptr) {
      expr = SageBuilder::buildLongIntVal_nfi(
          static_cast<long>(get_signed_value()), "");
    } else if (isSgTypeUnsignedLong(base_type) != nullptr) {
      expr = SageBuilder::buildUnsignedLongVal_nfi(
          static_cast<unsigned long>(get_unsigned_value()), "");
    } else if (isSgTypeLongLong(base_type) != nullptr) {
      expr = SageBuilder::buildLongLongIntVal_nfi(get_signed_value(), "");
    } else if (isSgTypeUnsignedLongLong(base_type) != nullptr) {
      expr = SageBuilder::buildUnsignedLongLongIntVal_nfi(get_unsigned_value(),
                                                          "");
    }
  }

  if (expr == NULL) {
    if (value.isSigned()) {
      expr = SageBuilder::buildLongLongIntVal_nfi(get_signed_value(), "");
    } else {
      expr = SageBuilder::buildUnsignedLongLongIntVal_nfi(get_unsigned_value(),
                                                          "");
    }
  }

  if (expr != NULL && !is_character_like_type) {
    llvm::SmallString<64> buf;
    value.toString(buf, 10, value.isSigned());
    setIntegralLiteralValueString(expr, std::string(buf.begin(), buf.end()));
  }

  return finish_semantic_literal(expr);
}

static bool templateArgumentNeedsExplicitAddressOf(SgType *param_type,
                                                   SgExpression *expr) {
  if (param_type == nullptr || expr == nullptr ||
      isSgAddressOfOp(expr) != nullptr || isSgNullptrValExp(expr) != nullptr) {
    return false;
  }

  SgType *base_param_type = param_type->stripType(SgType::STRIP_MODIFIER_TYPE |
                                                  SgType::STRIP_TYPEDEF_TYPE);
  if (isSgPointerMemberType(base_param_type) != nullptr) {
    return true;
  }
  if (isSgPointerType(base_param_type) == nullptr) {
    return false;
  }

  SgType *expr_type =
      expr->get_type() != nullptr
          ? expr->get_type()->stripType(SgType::STRIP_MODIFIER_TYPE |
                                        SgType::STRIP_TYPEDEF_TYPE)
          : nullptr;
  if (expr_type == nullptr) {
    return true;
  }

  if (isSgArrayType(expr_type) != nullptr ||
      isSgTypeString(expr_type) != nullptr) {
    return false;
  }

  if (isSgFunctionType(expr_type) != nullptr ||
      isSgMemberFunctionType(expr_type) != nullptr) {
    return true;
  }

  return isSgPointerType(expr_type) == nullptr &&
         isSgPointerMemberType(expr_type) == nullptr;
}

size_t countExpandedTemplateArgument(const clang::TemplateArgument &arg) {
  const clang::TemplateArgument &defined_arg =
      markClangTemplateArgumentDefined(arg);
  if (defined_arg.getKind() == clang::TemplateArgument::Pack) {
    size_t count = 0;
    const llvm::ArrayRef<clang::TemplateArgument> elements =
        markClangTemplateArgumentArrayDefined(defined_arg.pack_elements());
    // An empty pack contributes no typed arguments.  Whether an explicit
    // angle-bracket list was written is recorded independently on the owning
    // reference; an untyped list sentinel is never an AST argument.
    if (elements.empty()) {
      return 0;
    }
    for (const clang::TemplateArgument &pack_arg : elements) {
      count += countExpandedTemplateArgument(pack_arg);
    }
    return count;
  }
  return 1;
}
} // namespace

SgTemplateArgument *ClangToSageTranslator::translateTemplateArgument(
    const clang::TemplateArgument &arg, bool explicitlySpecified) {
  const clang::TemplateArgument &defined_arg =
      markClangTemplateArgumentDefined(arg);
  if (readClangApiValueDefined(
          [&]() { return defined_arg.isPackExpansion(); })) {
    clang::TemplateArgument pattern = defined_arg.getPackExpansionPattern();
    markClangTemplateArgumentDefined(pattern);
    if (SgTemplateArgument *sg_pattern =
            translateTemplateArgument(pattern, explicitlySpecified)) {
      sg_pattern->set_is_pack_element(true);
      return sg_pattern;
    }
  }

  SgTemplateArgument *sg_arg = nullptr;

  auto build_decl_expr =
      [&](clang::ValueDecl *decl,
          SgInitializedName **out_init_name) -> SgExpression * {
    auto build_enum_value_expr =
        [&](clang::EnumConstantDecl *enum_const_decl) -> SgExpression * {
      if (enum_const_decl == nullptr) {
        return nullptr;
      }

      SgEnumDeclaration *enum_decl = nullptr;
      if (const clang::EnumDecl *clang_enum_decl =
              llvm::dyn_cast<clang::EnumDecl>(
                  enum_const_decl->getDeclContext())) {
        SgDeclarationStatement *translated_decl =
            lookupSgDeclarationForClangDecl(
                const_cast<clang::EnumDecl *>(clang_enum_decl),
                /*allow_on_demand=*/true);
        if (translated_decl == nullptr) {
          translated_decl = isSgDeclarationStatement(
              TraverseOnDemand(const_cast<clang::EnumDecl *>(clang_enum_decl)));
        }
        enum_decl = isSgEnumDeclaration(translated_decl);
        if (enum_decl != nullptr) {
          if (SgEnumDeclaration *def_decl =
                  isSgEnumDeclaration(enum_decl->get_definingDeclaration())) {
            enum_decl = def_decl;
          }
        }
      }

      if (enum_decl == nullptr) {
        return nullptr;
      }

      const long long enum_value = enum_const_decl->getInitVal().getExtValue();
      SgExpression *expression = SageBuilder::buildEnumVal_nfi(
          enum_value, enum_decl, SgName(enum_const_decl->getNameAsString()));
      publishCanonicalSemanticExpressionSourceProvenance(
          expression, "semantic-template-enum-value");
      return expression;
    };

    auto apply_explicit_qualifier_from_decl_context =
        [&](SgExpression *expr, const clang::ValueDecl *value_decl) -> void {
      if (expr == nullptr || value_decl == nullptr) {
        return;
      }

      SgStringList tokens;
      for (const clang::DeclContext *ctx = value_decl->getDeclContext();
           ctx != nullptr; ctx = ctx->getParent()) {
        if (ctx->isTranslationUnit()) {
          break;
        }
        if (llvm::isa<clang::LinkageSpecDecl>(ctx)) {
          continue;
        }
        if (const clang::NamespaceDecl *ns =
                llvm::dyn_cast<clang::NamespaceDecl>(ctx)) {
          if (!ns->isAnonymousNamespace() && !ns->getName().empty()) {
            tokens.push_back(ns->getNameAsString() + "::");
          }
          continue;
        }
        if (const clang::NamespaceAliasDecl *alias =
                llvm::dyn_cast<clang::NamespaceAliasDecl>(ctx)) {
          if (!alias->getName().empty()) {
            tokens.push_back(alias->getNameAsString() + "::");
          }
          continue;
        }
        if (const clang::RecordDecl *record =
                llvm::dyn_cast<clang::RecordDecl>(ctx)) {
          if (!record->getName().empty()) {
            tokens.push_back(record->getNameAsString() + "::");
          }
          continue;
        }
      }

      if (tokens.empty()) {
        return;
      }
      std::reverse(tokens.begin(), tokens.end());

      auto apply_tokens = [&](auto *ref) {
        ref->set_explicit_name_qualification_tokens(tokens);
        ref->set_explicit_name_qualification_length(
            static_cast<int>(tokens.size()));
        ref->set_explicit_global_qualification(false);
      };

      if (SgTemplateMemberFunctionRefExp *tmpl_member =
              isSgTemplateMemberFunctionRefExp(expr)) {
        apply_tokens(tmpl_member);
        tmpl_member->set_need_qualifier(true);
      } else if (SgMemberFunctionRefExp *member_ref =
                     isSgMemberFunctionRefExp(expr)) {
        apply_tokens(member_ref);
        member_ref->set_need_qualifier(true);
      } else if (SgTemplateFunctionRefExp *tmpl_func =
                     isSgTemplateFunctionRefExp(expr)) {
        apply_tokens(tmpl_func);
      } else if (SgFunctionRefExp *func_ref = isSgFunctionRefExp(expr)) {
        apply_tokens(func_ref);
      } else if (SgVarRefExp *var_ref = isSgVarRefExp(expr)) {
        apply_tokens(var_ref);
      } else if (SgNonrealRefExp *nonreal_ref = isSgNonrealRefExp(expr)) {
        apply_tokens(nonreal_ref);
      } else if (SgEnumVal *enum_val = isSgEnumVal(expr)) {
        apply_tokens(enum_val);
      }
    };

    if (out_init_name != nullptr) {
      *out_init_name = nullptr;
    }
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
      if (out_init_name != nullptr) {
        *out_init_name = var_sym->get_declaration();
      }
      SgExpression *expr = SageBuilder::buildVarRefExp_nfi(var_sym);
      publishCanonicalSemanticExpressionSourceProvenance(
          expr, "semantic-template-variable-reference");
      apply_explicit_qualifier_from_decl_context(expr, decl);
      return expr;
    }
    if (SgMemberFunctionSymbol *member_sym = isSgMemberFunctionSymbol(sym)) {
      SgExpression *expr = SageBuilder::buildSemanticMemberFunctionRefExp(
          member_sym, false, false);
      publishCanonicalSemanticExpressionSourceProvenance(
          expr, "semantic-template-member-function-reference");
      apply_explicit_qualifier_from_decl_context(expr, decl);
      return expr;
    }
    if (SgFunctionSymbol *func_sym = isSgFunctionSymbol(sym)) {
      SgExpression *expr = registerFunctionReferenceTypeUse(
          SageBuilder::buildFunctionRefExp_nfi(func_sym));
      publishCanonicalSemanticExpressionSourceProvenance(
          expr, "semantic-template-function-reference");
      apply_explicit_qualifier_from_decl_context(expr, decl);
      return expr;
    }
    if (SgEnumFieldSymbol *enum_sym = isSgEnumFieldSymbol(sym)) {
      SgInitializedName *init_name = enum_sym->get_declaration();
      if (out_init_name != nullptr) {
        *out_init_name = init_name;
      }

      SgEnumDeclaration *enum_decl = nullptr;
      if (init_name != nullptr && init_name->get_type() != nullptr) {
        if (SgEnumType *enum_type = isSgEnumType(init_name->get_type())) {
          enum_decl = isSgEnumDeclaration(enum_type->get_declaration());
        }
      }
      if (enum_decl == nullptr && init_name != nullptr) {
        enum_decl = isSgEnumDeclaration(init_name->get_parent());
      }
      if (enum_decl != nullptr) {
        const clang::EnumConstantDecl *enum_const_decl =
            llvm::dyn_cast<clang::EnumConstantDecl>(decl);
        ROSE_ASSERT(enum_const_decl != nullptr);
        SgExpression *expr = build_enum_value_expr(
            const_cast<clang::EnumConstantDecl *>(enum_const_decl));
        if (expr != nullptr) {
          apply_explicit_qualifier_from_decl_context(expr, decl);
          return expr;
        }
      }
    }

    if (clang::EnumConstantDecl *enum_const_decl =
            llvm::dyn_cast<clang::EnumConstantDecl>(decl)) {
      SgExpression *expr = build_enum_value_expr(enum_const_decl);
      if (expr != nullptr) {
        apply_explicit_qualifier_from_decl_context(expr, decl);
        return expr;
      }
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
      llvm::SmallString<64> buf;
      value.getFloat().toString(buf);
      std::string text(buf.begin(), buf.end());

      SgExpression *expression = nullptr;
      if (isSgTypeFloat(value_type)) {
        expression = SageBuilder::buildFloatVal_nfi(0.0f, text);
      } else if (isSgTypeLongDouble(value_type)) {
        expression = SageBuilder::buildLongDoubleVal_nfi(0.0L, text);
      } else {
        expression = SageBuilder::buildDoubleVal_nfi(0.0, text);
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
        return build_decl_expr(const_cast<clang::ValueDecl *>(decl), nullptr);
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
          build_decl_expr(const_cast<clang::ValueDecl *>(member), nullptr);
      if (member_reference == nullptr || value_type == nullptr) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[semantic-template-member-pointer]: "
                "member=%s reference=%p type=%p has no exact Sage identity\n",
                member->getQualifiedNameAsString().c_str(),
                static_cast<void *>(member_reference),
                static_cast<void *>(value_type));
        ROSE_ABORT();
      }
      SgExpression *address =
          SageBuilder::buildAddressOfOp_nfi(member_reference, value_type);
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
    SgExpression *address =
        SageBuilder::buildAddressOfOp_nfi(operand, result_type);
    publishCanonicalSemanticExpressionSourceProvenance(
        address, "semantic-template-address-of");
    return address;
  };
  auto build_template_decl_argument =
      [&](clang::TemplateName template_name) -> SgDeclarationStatement * {
    clang::TemplateDecl *template_decl = template_name.getAsTemplateDecl();
    SgDeclarationStatement *sg_decl = nullptr;
    if (template_decl != nullptr) {
      SgNode *traverse_result = nullptr;
      if (clang::TemplateTemplateParmDecl *template_parameter =
              llvm::dyn_cast<clang::TemplateTemplateParmDecl>(template_decl)) {
        // A semantic template argument can retain the exact parameter
        // declaration from an active outer template header even when Clang
        // assigns that parameter the translation-unit DeclContext.  Resolve
        // it through the typed header surface that already owns the Sage
        // parameter; on-demand declaration traversal would have to guess an
        // owner that Clang does not encode in DeclContext.
        traverse_result = lookupActiveTemplateParameterSurface(
            template_parameter, "semantic-template-template-argument");
        if (traverse_result == nullptr) {
          const clang::DeclContext *active_context =
              currentTemplateParameterDeclContext();
          const clang::TemplateTemplateParmDecl *active_parameter = nullptr;
          for (const clang::TemplateParameterList *level :
               collectTemplateParameterLevelsFromDeclContext(active_context)) {
            level = markClangTemplateParameterListDefined(level);
            const unsigned index = template_parameter->getIndex();
            if (level == nullptr || index >= level->size()) {
              continue;
            }
            auto *candidate =
                llvm::dyn_cast_or_null<clang::TemplateTemplateParmDecl>(
                    markClangDeclObjectDefinedByKind(level->getParam(index)));
            if (candidate == nullptr ||
                candidate->getDepth() != template_parameter->getDepth() ||
                candidate->getIndex() != index) {
              continue;
            }
            const clang::DeclContext *parameter_context =
                markClangDeclContextObjectDefined(
                    template_parameter->getDeclContext());
            const bool clang_lost_written_owner =
                parameter_context != nullptr &&
                parameter_context->isTranslationUnit();
            if (!clang_lost_written_owner &&
                candidate->getCanonicalDecl() !=
                    template_parameter->getCanonicalDecl()) {
              continue;
            }
            if (active_parameter != nullptr &&
                active_parameter->getCanonicalDecl() !=
                    candidate->getCanonicalDecl()) {
              std::cerr << "REX_FRONTEND_INVARIANT[template-template-argument-"
                           "owner]: depth="
                        << template_parameter->getDepth() << " index=" << index
                        << " resolves to multiple active parameter families"
                        << std::endl;
              ROSE_ABORT();
            }
            active_parameter = candidate;
          }
          if (active_parameter != nullptr) {
            traverse_result = lookupActiveTemplateParameterSurface(
                active_parameter,
                "semantic-template-template-argument-active-context");
            if (traverse_result == nullptr) {
              auto active_mapping = p_decl_translation_map.find(
                  const_cast<clang::TemplateTemplateParmDecl *>(
                      active_parameter));
              if (active_mapping != p_decl_translation_map.end()) {
                traverse_result = active_mapping->second;
              }
            }
          }
        }
      }
      if (traverse_result == nullptr) {
        auto mapped = p_decl_translation_map.find(template_decl);
        if (mapped != p_decl_translation_map.end()) {
          traverse_result = mapped->second;
        } else {
          traverse_result = TraverseOnDemand(template_decl);
        }
      }
      sg_decl = isSgDeclarationStatement(traverse_result);

      // A template-template parameter is represented by a typed
      // SgTemplateParameter whose templateDeclaration is the exact template
      // identity.  Do not fall back to manufacturing a name-bearing nonreal
      // declaration for an unnamed parameter.
      if (SgTemplateParameter *param = isSgTemplateParameter(traverse_result)) {
        SgDeclarationStatement *inner_decl = param->get_templateDeclaration();
        if (isSgTemplateDeclaration(inner_decl) == nullptr) {
          std::cerr << "REX_FRONTEND_INVARIANT[template-template-argument-"
                       "identity]: parameter has no exact template "
                       "declaration"
                    << std::endl;
          ROSE_ABORT();
        }
        sg_decl = inner_decl;
      }
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
      name_str = getTemplateNameBase(template_name);
    } else {
      name_str = template_decl ? template_decl->getNameAsString() : "";
    }

    if (sg_decl == nullptr && name_str.empty()) {
      name_str = getTemplateNameBase(template_name);
    }

    if (qualifier && !name_str.empty()) {
      SgScopeStatement *scope = SageBuilder::topScopeStack();
      if (scope == nullptr) {
        scope = getGlobalScope();
      }
      SgNonrealType *nr_type = buildSemanticNonrealTypeFromNestedNameSpecifier(
          qualifier, scope, SgName(name_str), nullptr, nullptr);
      if (SgNonrealDecl *nr_decl =
              isSgNonrealDecl(nr_type ? nr_type->get_declaration() : nullptr)) {
        if (sg_decl != nullptr && sg_decl != nr_decl) {
          nr_decl->set_templateDeclaration(sg_decl);
        }
        if (has_template_keyword) {
          nr_decl->set_has_template_keyword(true);
        }
        sg_decl = nr_decl;
      }
    }

    if (sg_decl == nullptr && !name_str.empty()) {
      SgScopeStatement *scope = SageBuilder::topScopeStack();
      if (scope == nullptr) {
        scope = getGlobalScope();
      }
      SgNonrealType *nr_type = SageBuilder::buildSemanticNonrealType(
          SgName(name_str), scope, nullptr, nullptr);
      if (SgNonrealDecl *nr_decl =
              isSgNonrealDecl(nr_type ? nr_type->get_declaration() : nullptr)) {
        if (has_template_keyword) {
          nr_decl->set_has_template_keyword(true);
        }
        sg_decl = nr_decl;
      }
    }

    return sg_decl;
  };

  switch (defined_arg.getKind()) {
  case clang::TemplateArgument::Type: {
    clang::QualType arg_qt = markClangQualTypeDefined(defined_arg.getAsType());
    const clang::Type *argument_type =
        markClangTypeObjectDefinedByClass(arg_qt.getTypePtrOrNull());
    ASSERT_not_null(argument_type);
    const bool requires_source_owned_dependent_identity =
        requiresSourceOwnedDependentTypeIdentity(arg_qt);
    clang::QualType semantic_qt = markClangQualTypeDefined(
        requires_source_owned_dependent_identity ? arg_qt
                                                 : arg_qt.getCanonicalType());
    struct CanonicalArgumentTypeSurface {
      std::array<unsigned *, 7> depths;
      std::array<unsigned, 7> suspended{};

      explicit CanonicalArgumentTypeSurface(std::array<unsigned *, 7> values)
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
           &p_force_written_named_type_qualification_depth,
           &p_preserve_omitted_injected_class_template_id_depth,
           &p_force_nonlocal_injected_class_name_depth});
      arg_type = buildTypeFromQualifiedType(semantic_qt);
    }
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
      arg_tst = llvm::dyn_cast_or_null<clang::TemplateSpecializationType>(
          markClangTypeObjectDefinedByClass(arg_tst));
      auto build_template_specialization_type =
          [&](const clang::TemplateSpecializationType *tst) -> SgType * {
        tst = llvm::dyn_cast_or_null<clang::TemplateSpecializationType>(
            markClangTypeObjectDefinedByClass(tst));
        if (tst == nullptr) {
          return nullptr;
        }
        clang::TemplateName tname =
            markClangTemplateNameDefined(tst->getTemplateName());
        std::string base_name = getTemplateNameBase(tname);
        if (base_name.empty()) {
          return nullptr;
        }

        // This reconstructed nonreal declaration is the semantic identity
        // carried by the source-spelled type argument.  Its owned argument
        // expressions therefore require semantic provenance even though the
        // outer SgTemplateArgument separately records the written type
        // surface.
        SemanticExpressionConstruction semantic_arguments(
            p_semantic_template_argument_expression_depth,
            "translated-type-argument-nonreal-template-id");
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

        SgScopeStatement *scope = SageBuilder::topScopeStack();
        if (scope == nullptr) {
          scope = getGlobalScope();
        }

        const SgName semantic_name(buildExactTemplateInstantiationName(
            base_name, tst->template_arguments(),
            currentTemplateParameterDeclContext()));
        SgNonrealType *nrtype = nullptr;
        if (qualifier) {
          nrtype = buildSemanticNonrealTypeFromNestedNameSpecifier(
              qualifier, scope, SgName(base_name), &tpl_args, &semantic_name);
        } else {
          nrtype = SageBuilder::buildSemanticNonrealType(
              SgName(base_name), scope, &tpl_args, &semantic_name);
        }
        if (SgNonrealDecl *nrdecl =
                isSgNonrealDecl(nrtype ? nrtype->get_declaration() : nullptr)) {
          if (has_template_keyword) {
            nrdecl->set_has_template_keyword(true);
          }
          if (clang::TemplateDecl *template_declaration =
                  resolveTemplateNameDeclaration(tname)) {
            if (clang::TemplateTemplateParmDecl *template_parameter =
                    llvm::dyn_cast<clang::TemplateTemplateParmDecl>(
                        template_declaration)) {
              SgTemplateParameter *sage_parameter =
                  lookupActiveTemplateParameterSurface(
                      template_parameter,
                      "source-type-template-argument-identity");
              if (sage_parameter == nullptr) {
                auto translated =
                    p_decl_translation_map.find(template_parameter);
                sage_parameter = translated != p_decl_translation_map.end()
                                     ? isSgTemplateParameter(translated->second)
                                     : nullptr;
              }
              SgTemplateDeclaration *parameter_identity =
                  sage_parameter != nullptr
                      ? isSgTemplateDeclaration(
                            sage_parameter->get_templateDeclaration())
                      : nullptr;
              SgTemplateType *parameter_type =
                  sage_parameter != nullptr
                      ? isSgTemplateType(sage_parameter->get_type())
                      : nullptr;
              if (sage_parameter == nullptr || parameter_identity == nullptr ||
                  parameter_type == nullptr ||
                  parameter_type->get_template_parameter() != sage_parameter ||
                  parameter_type->get_template_parameter_depth() !=
                      static_cast<int>(template_parameter->getDepth()) ||
                  parameter_type->get_template_parameter_position() !=
                      static_cast<int>(template_parameter->getIndex())) {
                fprintf(
                    stderr,
                    "REX_FRONTEND_INVARIANT[source-type-template-argument-"
                    "identity]: template=%s parameter depth=%u index=%u has "
                    "no exact producer-published Sage template identity\n",
                    base_name.c_str(), template_parameter->getDepth(),
                    template_parameter->getIndex());
                ROSE_ABORT();
              }
              nrdecl->set_templateDeclaration(parameter_identity);
            } else {
              linkNonrealTemplateDeclaration(
                  nrdecl, template_declaration,
                  "source-type-template-argument-identity");
            }
          }
        }
        return nrtype;
      };

      bool needs_rebuild = false;
      bool is_alias = arg_tst->isTypeAlias();
      if (SgNonrealType *nrtype = isSgNonrealType(source_arg_type)) {
        if (SgNonrealDecl *nrdecl =
                isSgNonrealDecl(nrtype->get_declaration())) {
          if (nrdecl->get_tpl_args().empty() &&
              (is_alias || !markClangTemplateArgumentArrayDefined(
                                arg_tst->template_arguments())
                                .empty())) {
            needs_rebuild = true;
          }
        }
      } else if (isSgTypedefType(source_arg_type) != nullptr) {
        if (is_alias || !markClangTemplateArgumentArrayDefined(
                             arg_tst->template_arguments())
                             .empty()) {
          needs_rebuild = true;
        }
      }

      if (reconstruct_source_surface && needs_rebuild) {
        if (SgType *spec_type = build_template_specialization_type(arg_tst)) {
          source_arg_type = spec_type;
        }
      }

      if (SgNonrealType *source_nonreal = isSgNonrealType(source_arg_type)) {
        SgNonrealDecl *source_declaration =
            isSgNonrealDecl(source_nonreal->get_declaration());
        if (source_declaration == nullptr) {
          fprintf(stderr,
                  "REX_FRONTEND_INVARIANT[source-template-argument-type]: "
                  "template-id=%s source nonreal type has no exact "
                  "declaration\n",
                  getTemplateNameBase(arg_tst->getTemplateName()).c_str());
          ROSE_ABORT();
        }

        clang::TemplateName template_name =
            markClangTemplateNameDefined(arg_tst->getTemplateName());
        clang::TemplateDecl *template_declaration =
            resolveTemplateNameDeclaration(template_name);
        if (clang::TemplateTemplateParmDecl *template_parameter =
                llvm::dyn_cast_or_null<clang::TemplateTemplateParmDecl>(
                    template_declaration)) {
          SgDeclarationStatement *parameter_identity =
              build_template_decl_argument(template_name);
          if (isSgTemplateDeclaration(parameter_identity) == nullptr) {
            fprintf(stderr,
                    "REX_FRONTEND_INVARIANT[source-template-argument-type]: "
                    "template-template parameter=%s depth=%u index=%u has no "
                    "exact Sage template identity\n",
                    getTemplateNameBase(template_name).c_str(),
                    template_parameter->getDepth(),
                    template_parameter->getIndex());
            ROSE_ABORT();
          }
          SgDeclarationStatement *existing =
              normalizeNonrealTemplateDeclarationTarget(
                  source_declaration->get_templateDeclaration());
          if (existing != nullptr && existing != parameter_identity) {
            fprintf(stderr,
                    "REX_FRONTEND_INVARIANT[source-template-argument-type]: "
                    "template-id=%s already links a distinct declaration "
                    "%p/%s instead of exact parameter identity=%p/%s\n",
                    getTemplateNameBase(template_name).c_str(),
                    static_cast<void *>(existing),
                    existing->class_name().c_str(),
                    static_cast<void *>(parameter_identity),
                    parameter_identity->class_name().c_str());
            ROSE_ABORT();
          }
          source_declaration->set_templateDeclaration(parameter_identity);
        } else if (template_declaration != nullptr) {
          linkNonrealTemplateDeclaration(source_declaration,
                                         template_declaration,
                                         "source-template-argument-type");
        }
      }
    }

    if (arg_type != nullptr) {
      sg_arg = new SgTemplateArgument(arg_type, explicitlySpecified);
      if (source_arg_type != nullptr && source_arg_type != arg_type) {
        sg_arg->set_sourceSpelledType(source_arg_type);
      }
    } else {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[template-argument-type-identity]: "
              "canonical type argument has no exact semantic Sage type\n");
      ROSE_ABORT();
    }
    break;
  }

  case clang::TemplateArgument::Integral: {
    llvm::APSInt value = defined_arg.getAsIntegral();
    clang::QualType integral_type =
        markClangQualTypeDefined(defined_arg.getIntegralType());
    SgType *int_type = buildTypeFromQualifiedType(integral_type);

    SgExpression *value_expr = nullptr;
    if (const clang::EnumType *enum_type =
            integral_type->getAs<clang::EnumType>()) {
      if (const clang::EnumDecl *enum_decl = enum_type->getDecl()) {
        for (const clang::EnumConstantDecl *enum_const :
             enum_decl->enumerators()) {
          if (enum_const != nullptr &&
              llvm::APSInt::isSameValue(enum_const->getInitVal(), value)) {
            value_expr = build_decl_expr(
                const_cast<clang::EnumConstantDecl *>(enum_const), nullptr);
            if (value_expr != nullptr) {
              break;
            }
          }
        }
      }
    }
    if (value_expr == nullptr) {
      value_expr = buildIntegralTemplateArgExpr(value, int_type);
      publishCanonicalSemanticExpressionSourceProvenance(
          value_expr, "semantic-template-integral-value");
    }

    if (value_expr != nullptr) {
      if (value_expr->get_type() == nullptr || int_type == nullptr ||
          !SageInterface::isEquivalentType(value_expr->get_type(), int_type)) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[template-argument-integral-type]: "
                "canonical integral argument has value-type=%p and converted "
                "parameter-type=%p that are not equivalent\n",
                static_cast<void *>(value_expr->get_type()),
                static_cast<void *>(int_type));
        ROSE_ABORT();
      }
      sg_arg =
          new SgTemplateArgument(SgTemplateArgument::nontype_argument,
                                 false /*isArrayBoundUnknownType*/, int_type,
                                 value_expr, nullptr, explicitlySpecified);
      value_expr->set_parent(sg_arg);
    }
    break;
  }

  case clang::TemplateArgument::Declaration: {
    clang::ValueDecl *decl =
        const_cast<clang::ValueDecl *>(llvm::dyn_cast_or_null<clang::ValueDecl>(
            markClangDeclObjectDefinedByKind(defined_arg.getAsDecl())));
    clang::QualType param_qual_type =
        markClangQualTypeDefined(defined_arg.getParamTypeForDecl());
    SgType *param_type = buildTypeFromQualifiedType(param_qual_type);
    if (param_type == nullptr) {
      fprintf(stderr, "REX_FRONTEND_INVARIANT[template-argument-declaration]: "
                      "declaration argument has no exact parameter type\n");
      ROSE_ABORT();
    }
    SgInitializedName *init_name = nullptr;
    SgExpression *decl_expr = build_decl_expr(decl, &init_name);
    if (decl_expr != nullptr &&
        templateArgumentNeedsExplicitAddressOf(param_type, decl_expr)) {
      decl_expr = build_semantic_address_of(decl_expr, param_type);
    }
    SgType *decl_expr_type =
        decl_expr != nullptr ? decl_expr->get_type() : nullptr;
    SgType *stripped_decl_expr_type =
        decl_expr_type != nullptr ? decl_expr_type->stripTypedefsAndModifiers()
                                  : nullptr;
    if (decl_expr != nullptr &&
        isSgArrayType(stripped_decl_expr_type) != nullptr &&
        isSgPointerType(param_type->stripTypedefsAndModifiers()) != nullptr) {
      decl_expr = SageBuilder::buildCastExp_nfi(
          decl_expr, param_type, SgCastExp::e_implicit_cast,
          SgCastExp::e_semantic_conversion_ArrayToPointerDecay,
          SgCastExp::e_value_category_prvalue, {});
      publishCanonicalSemanticExpressionSourceProvenance(
          decl_expr, "semantic-template-array-to-pointer-conversion");
    }
    if (decl_expr != nullptr) {
      SgReferenceType *reference_param = isSgReferenceType(param_type);
      SgType *reference_base = reference_param != nullptr
                                   ? reference_param->get_base_type()
                                   : nullptr;
      SgType *value_type = decl_expr->get_type();
      const bool exact_qualification_binding =
          reference_base != nullptr && value_type != nullptr &&
          !SageInterface::isEquivalentType(reference_base, value_type) &&
          SageInterface::isEquivalentType(
              reference_base->stripTypedefsAndModifiers(),
              value_type->stripTypedefsAndModifiers());
      if (exact_qualification_binding) {
        decl_expr = SageBuilder::buildCastExp_nfi(
            decl_expr, reference_base, SgCastExp::e_implicit_cast,
            SgCastExp::e_semantic_conversion_NoOp,
            SgCastExp::e_value_category_lvalue, {});
        publishCanonicalSemanticExpressionSourceProvenance(
            decl_expr, "semantic-template-qualification-conversion");
      }
    }
    if (decl_expr != nullptr || init_name != nullptr) {
      sg_arg = new SgTemplateArgument(SgTemplateArgument::nontype_argument,
                                      /*isArrayBoundUnknownType=*/false,
                                      /*type=*/param_type,
                                      /*expression=*/decl_expr,
                                      /*templateDeclaration=*/nullptr,
                                      explicitlySpecified);
      if (decl_expr != nullptr) {
        decl_expr->set_parent(sg_arg);
      }
      if (decl_expr == nullptr && init_name != nullptr) {
        sg_arg->set_initializedName(init_name);
      }
    }
    break;
  }

  case clang::TemplateArgument::NullPtr: {
    SgType *null_type = buildTypeFromQualifiedType(
        markClangQualTypeDefined(defined_arg.getNullPtrType()));
    SgExpression *null_expr = SageBuilder::buildNullptrValExp_nfi();
    publishCanonicalSemanticExpressionSourceProvenance(
        null_expr, "semantic-template-null-pointer");
    sg_arg = new SgTemplateArgument(SgTemplateArgument::nontype_argument,
                                    /*isArrayBoundUnknownType=*/false,
                                    /*type=*/null_type,
                                    /*expression=*/null_expr,
                                    /*templateDeclaration=*/nullptr,
                                    explicitlySpecified);
    null_expr->set_parent(sg_arg);
    break;
  }

  case clang::TemplateArgument::StructuralValue: {
    clang::QualType structural_qual_type =
        markClangQualTypeDefined(defined_arg.getStructuralValueType());
    SgType *value_type = buildTypeFromQualifiedType(structural_qual_type);
    const clang::APValue &value = defined_arg.getAsStructuralValue();
    markClangLocalObjectDefined(&value);
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
      value_expr->set_parent(sg_arg);
    }
    break;
  }

  case clang::TemplateArgument::Template: {
    clang::TemplateName template_name =
        markClangTemplateNameDefined(defined_arg.getAsTemplate());
    SgDeclarationStatement *sg_decl =
        build_template_decl_argument(template_name);

    if (sg_decl != nullptr) {
      sg_arg =
          new SgTemplateArgument(SgTemplateArgument::template_template_argument,
                                 /*isArrayBoundUnknownType=*/false,
                                 /*type=*/nullptr,
                                 /*expression=*/nullptr,
                                 /*templateDeclaration=*/sg_decl,
                                 /*explicitlySpecified=*/explicitlySpecified);
    } else {
      fprintf(stderr, "REX_FRONTEND_INVARIANT[template-argument]: failed to "
                      "translate template declaration\n");
      ROSE_ABORT();
    }
    break;
  }

  case clang::TemplateArgument::TemplateExpansion: {
    clang::TemplateName template_name = markClangTemplateNameDefined(
        defined_arg.getAsTemplateOrTemplatePattern());
    SgDeclarationStatement *sg_decl =
        build_template_decl_argument(template_name);

    if (sg_decl != nullptr) {
      sg_arg =
          new SgTemplateArgument(SgTemplateArgument::template_template_argument,
                                 /*isArrayBoundUnknownType=*/false,
                                 /*type=*/nullptr,
                                 /*expression=*/nullptr,
                                 /*templateDeclaration=*/sg_decl,
                                 /*explicitlySpecified=*/explicitlySpecified);
    } else {
      fprintf(stderr, "REX_FRONTEND_INVARIANT[template-argument]: failed to "
                      "translate template-expansion declaration\n");
      ROSE_ABORT();
    }
    break;
  }

  case clang::TemplateArgument::Expression: {
    clang::Expr *clang_expr = const_cast<clang::Expr *>(
        markClangExprObjectDefinedByClass(defined_arg.getAsExpr()));
    if (clang_expr != nullptr) {
      SgExpression *sg_expr = nullptr;
      SgType *expression_type = nullptr;
      if (p_compiler_instance == nullptr) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[semantic-template-argument]: "
                "expression=%s has no compiler AST context\n",
                clang_expr->getStmtClassName());
        ROSE_ABORT();
      }
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
        source_surface_expression = markClangExprObjectDefinedByClass(wrapped);
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
                : buildTypeFromQualifiedType(
                      markClangQualTypeDefined(clang_expr->getType()));
        if (expression_type == nullptr) {
          fprintf(stderr,
                  "REX_FRONTEND_INVARIANT[semantic-template-conversion]: "
                  "dependent expression=%s has no exact semantic result "
                  "type\n",
                  clang_expr->getStmtClassName());
          ROSE_ABORT();
        }
        for (auto conversion = semantic_conversions.rbegin();
             conversion != semantic_conversions.rend(); ++conversion) {
          const clang::ImplicitCastExpr *implicit =
              markClangAstObjectDefined(*conversion);
          SgType *conversion_type = buildTypeFromQualifiedType(
              markClangQualTypeDefined(implicit->getType()));
          if (conversion_type == nullptr) {
            fprintf(stderr,
                    "REX_FRONTEND_INVARIANT[semantic-template-conversion]: "
                    "dependent cast kind=%s has no exact result type\n",
                    implicit->getCastKindName());
            ROSE_ABORT();
          }
          sg_expr = SageBuilder::buildCastExp_nfi(
              sg_expr, conversion_type, SgCastExp::e_implicit_cast,
              translateClangCastKind(implicit->getCastKind()),
              translateClangValueCategory(implicit->getValueKind()), {});
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
        markClangLocalObjectDefined(&evaluation.Val);
        expression_type = buildTypeFromQualifiedType(
            markClangQualTypeDefined(clang_expr->getType()));
        const clang::APValue *semantic_value = &evaluation.Val;
        SgType *semantic_value_type = expression_type;
        clang::Expr::EvalResult source_evaluation;
        clang::QualType source_surface_qual_type =
            markClangQualTypeDefined(source_surface_expression->getType());
        const bool declaration_identity_conversion =
            llvm::isa<clang::DeclRefExpr>(source_surface_expression) &&
            !semantic_conversions.empty() &&
            (source_surface_qual_type->isArrayType() ||
             source_surface_qual_type->isFunctionType());
        if (declaration_identity_conversion) {
          sg_expr = isSgExpression(
              Traverse(const_cast<clang::Expr *>(source_surface_expression)));
          semantic_value_type =
              buildTypeFromQualifiedType(source_surface_qual_type);
          if (sg_expr == nullptr || sg_expr->get_parent() != nullptr ||
              sg_expr->get_originalExpressionTree() != nullptr ||
              semantic_value_type == nullptr ||
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
        } else if (!semantic_conversions.empty()) {
          const bool source_evaluated =
              source_surface_expression->EvaluateAsRValue(
                  source_evaluation, p_compiler_instance->getASTContext());
          if (!source_evaluated) {
            fprintf(stderr,
                    "REX_FRONTEND_INVARIANT[semantic-template-conversion]: "
                    "source expression=%s has no exact pre-conversion "
                    "constant value\n",
                    source_surface_expression->getStmtClassName());
            ROSE_ABORT();
          }
          markClangLocalObjectDefined(&source_evaluation.Val);
          semantic_value = &source_evaluation.Val;
          // This node is the canonical pre-conversion value, not the written
          // source surface.  Preserve aliases and qualification on the
          // independently owned source expression, while giving the evaluated
          // value the exact canonical Clang type.  Reusing the TypeLoc-facing
          // spelling here can manufacture a nonreal type for an integral
          // APValue (for example `const std::size_t kElements`), leaving an
          // integer literal with a non-integral semantic type.
          semantic_value_type =
              buildTypeFromQualifiedType(markClangQualTypeDefined(
                  source_surface_expression->getType().getCanonicalType()));
        }
        if (sg_expr == nullptr) {
          sg_expr = build_structural_expr(*semantic_value, semantic_value_type);
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
        for (auto conversion = semantic_conversions.rbegin();
             conversion != semantic_conversions.rend(); ++conversion) {
          const clang::ImplicitCastExpr *implicit =
              markClangAstObjectDefined(*conversion);
          SgType *conversion_type = buildTypeFromQualifiedType(
              markClangQualTypeDefined(implicit->getType()));
          if (conversion_type == nullptr) {
            fprintf(stderr,
                    "REX_FRONTEND_INVARIANT[semantic-template-conversion]: "
                    "cast kind=%s has no exact result type\n",
                    implicit->getCastKindName());
            ROSE_ABORT();
          }
          sg_expr = SageBuilder::buildCastExp_nfi(
              sg_expr, conversion_type, SgCastExp::e_implicit_cast,
              translateClangCastKind(implicit->getCastKind()),
              translateClangValueCategory(implicit->getValueKind()), {});
          publishCanonicalSemanticImplicitConversionProvenance(
              isSgCastExp(sg_expr), "semantic-template-implicit-conversion");
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
        if (expression_type == nullptr) {
          fprintf(stderr,
                  "REX_FRONTEND_INVARIANT[template-argument-expression]: "
                  "semantic expression argument has no exact type\n");
          ROSE_ABORT();
        }
        sg_arg = new SgTemplateArgument(
            SgTemplateArgument::nontype_argument,
            /*isArrayBoundUnknownType=*/false, expression_type, sg_expr,
            /*templateDeclaration=*/nullptr, explicitlySpecified);
        sg_expr->set_parent(sg_arg);
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
            static_cast<int>(defined_arg.getKind()));
    ROSE_ABORT();
  }

  return sg_arg;
}

SgType *ClangToSageTranslator::translateTypeTemplateArgument(
    const clang::TemplateArgumentLoc &arg_loc,
    SgScopeStatement *semantic_owner_scope) {
  markClangValueDefined(arg_loc);
  if (semantic_owner_scope == nullptr) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[template-argument-type-owner]: "
            "source-spelled type argument has no exact semantic owner\n");
    ROSE_ABORT();
  }
  clang::TemplateArgument arg =
      readClangApiValueDefined([&]() { return arg_loc.getArgument(); });
  markClangTemplateArgumentDefined(arg);

  if (arg.getKind() != clang::TemplateArgument::Type) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[template-argument-type-kind]: "
            "type-argument producer received Clang kind=%d\n",
            static_cast<int>(arg.getKind()));
    ROSE_ABORT();
  }

  if (const clang::TypeSourceInfo *type_info =
          markClangAstObjectDefined(readClangApiValueDefined(
              [&]() { return arg_loc.getTypeSourceInfo(); }))) {
    const clang::TypeLoc written_type_loc =
        readClangApiValueDefined([&]() { return type_info->getTypeLoc(); });
    markClangTypeLocDataDefined(written_type_loc);
    const bool written_type_spells_elaborated_keyword =
        typeLocSpellsElaboratedKeyword(written_type_loc,
                                       readClangApiValueDefined([&]() {
                                         return arg_loc.getSourceRange();
                                       }),
                                       p_compiler_instance);

    SgType *arg_type = buildTypeFromTypeLocWithSemanticOwner(
        written_type_loc, semantic_owner_scope,
        "source-spelled type template argument");
    if (arg_type != nullptr) {
      auto build_translated_qualified_tag_type = [&]() -> SgType * {
        clang::TypeLoc type_loc = written_type_loc;
        while (!type_loc.isNull()) {
          if (auto tag_loc = type_loc.getAs<clang::TagTypeLoc>()) {
            const clang::TagType *tag_type =
                llvm::dyn_cast_or_null<clang::TagType>(
                    markClangTypeObjectDefinedByClass(tag_loc.getTypePtr()));
            if (tag_type == nullptr || tag_type->isDependentType()) {
              return nullptr;
            }

            clang::NestedNameSpecifier written_qualifier =
                readClangApiValueDefined(
                    [&]() { return tag_type->getQualifier(); });
            markClangTagTypeQualifierStorageDefined(tag_type,
                                                    written_qualifier);
            markClangNestedNameSpecifierDefined(written_qualifier);
            clang::NestedNameSpecifierLoc qualifier_loc =
                tag_loc.getQualifierLoc();
            clang::NestedNameSpecifier qualifier =
                qualifier_loc.getNestedNameSpecifier();
            if (!qualifier) {
              qualifier = written_qualifier;
            }
            bool qualifier_requires_written_nonreal = false;
            auto next_qualifier_loc = [](clang::NestedNameSpecifierLoc current)
                -> clang::NestedNameSpecifierLoc {
              current = markClangNestedNameSpecifierLocDefined(current);
              if (!current) {
                return clang::NestedNameSpecifierLoc();
              }

              switch (current.getNestedNameSpecifier().getKind()) {
              case clang::NestedNameSpecifier::Kind::Namespace:
                return current.getAsNamespaceAndPrefix().Prefix;
              case clang::NestedNameSpecifier::Kind::Type:
                return current.getAsTypeLoc().getPrefix();
              case clang::NestedNameSpecifier::Kind::Global:
              case clang::NestedNameSpecifier::Kind::MicrosoftSuper:
              case clang::NestedNameSpecifier::Kind::Null:
                return clang::NestedNameSpecifierLoc();
              }

              return clang::NestedNameSpecifierLoc();
            };

            for (clang::NestedNameSpecifierLoc current = qualifier_loc; current;
                 current = next_qualifier_loc(current)) {
              current = markClangNestedNameSpecifierLocDefined(current);
              clang::NestedNameSpecifier nns = current.getNestedNameSpecifier();
              if (nns.getKind() != clang::NestedNameSpecifier::Kind::Type) {
                continue;
              }

              const clang::Type *qualifier_type = nns.getAsType();
              if (qualifier_type != nullptr &&
                  qualifier_type->isDependentType()) {
                qualifier_requires_written_nonreal = true;
                break;
              }

              clang::TypeLoc qualifier_type_loc = current.getAsTypeLoc();
              if (!qualifier_type_loc.isNull() &&
                  (qualifier_type_loc
                       .getAs<clang::TemplateSpecializationTypeLoc>() ||
                   qualifier_type_loc.getAs<clang::DependentNameTypeLoc>())) {
                qualifier_requires_written_nonreal = true;
                break;
              }
            }

            if (!qualifier || qualifier_requires_written_nonreal) {
              return nullptr;
            }

            return buildTypeFromQualifiedType(
                markClangQualTypeDefined(arg.getAsType()));
          }

          type_loc = type_loc.getNextTypeLoc();
        }

        return nullptr;
      };

      clang::QualType arg_qt = markClangQualTypeDefined(arg.getAsType());
      SgType *resolved_type = arg_type;
      // buildTypeFromTypeLoc is the sole producer of the source-spelled type.
      // Rebuilding a template-id here from the canonical TemplateArgument
      // discarded the exact NestedNameSpecifierLoc and moved its terminal
      // declaration out of the written qualifier chain. That malformed source
      // type later lost qualifiers such as `std::` and `A<B>::`.
      for (clang::TypeLoc current = written_type_loc; !current.isNull();
           current = current.getNextTypeLoc()) {
        if (current.getAs<clang::TemplateSpecializationTypeLoc>()) {
          return resolved_type;
        }
      }

      if (p_force_written_named_type_qualification_depth != 0 &&
          isSgNonrealType(resolved_type) != nullptr &&
          typeLocQualifierLoc(written_type_loc)) {
        return resolved_type;
      }
      if (SgType *qualified_tag_type = build_translated_qualified_tag_type()) {
        if (written_type_spells_elaborated_keyword ||
            resolved_type == nullptr ||
            isSgTypeUnknown(resolved_type) != nullptr ||
            isSgNonrealType(resolved_type) != nullptr) {
          resolved_type = qualified_tag_type;
        }
      }
      if (written_type_spells_elaborated_keyword &&
          (resolved_type == nullptr ||
           isSgTypeUnknown(resolved_type) != nullptr ||
           isSgNonrealType(resolved_type) != nullptr)) {
        if (SgType *semantic_tag_type = buildTypeFromQualifiedType(arg_qt)) {
          resolved_type = semantic_tag_type;
        }
      }
      if (resolved_type == nullptr ||
          isSgTypeUnknown(resolved_type) != nullptr) {
        if (SgType *qualified_tag_type =
                build_translated_qualified_tag_type()) {
          resolved_type = qualified_tag_type;
        }
      }
      arg_qt = markClangQualTypeDefined(arg_qt);
      const clang::Type *raw_type =
          markClangTypeObjectDefinedByClass(arg_qt.getTypePtrOrNull());
      const clang::RecordType *record_type =
          raw_type != nullptr ? readClangApiValueDefined([&]() {
            return raw_type->getAs<clang::RecordType>();
          })
                              : nullptr;
      record_type = llvm::dyn_cast_or_null<clang::RecordType>(
          markClangTypeObjectDefinedByClass(record_type));
      const clang::RecordDecl *record_decl =
          record_type != nullptr
              ? llvm::dyn_cast_or_null<clang::RecordDecl>(
                    markClangDeclObjectDefinedByKind(record_type->getDecl()))
              : nullptr;
      if (record_decl != nullptr &&
          (resolved_type == nullptr ||
           isSgTypeUnknown(resolved_type) != nullptr)) {
        SgClassDeclaration *record_sg_decl =
            isSgClassDeclaration(lookupSgDeclarationForClangDecl(
                const_cast<clang::RecordDecl *>(record_decl),
                /*allow_on_demand=*/true));
        if (record_sg_decl == nullptr &&
            p_decl_translation_in_progress.find(const_cast<clang::RecordDecl *>(
                record_decl)) == p_decl_translation_in_progress.end()) {
          if (SgNode *record_node = TraverseOnDemand(
                  const_cast<clang::RecordDecl *>(record_decl))) {
            record_sg_decl = isSgClassDeclaration(record_node);
          }
        }
        if (record_sg_decl != nullptr) {
          if (SgClassDeclaration *first_nondef = isSgClassDeclaration(
                  record_sg_decl->get_firstNondefiningDeclaration())) {
            record_sg_decl = first_nondef;
          }
          resolved_type = SgClassType::createType(record_sg_decl);
        }
      }

      return resolved_type;
    }
  }

  fprintf(stderr, "REX_FRONTEND_INVARIANT[template-argument-type-source]: "
                  "source-spelled type argument has no exact TypeSourceInfo\n");
  ROSE_ABORT();
}

SgTemplateArgument *ClangToSageTranslator::translateTemplateArgument(
    const clang::TemplateArgumentLoc &arg_loc, bool explicitlySpecified) {
  markClangValueDefined(arg_loc);
  clang::TemplateArgument arg =
      readClangApiValueDefined([&]() { return arg_loc.getArgument(); });
  markClangTemplateArgumentDefined(arg);

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

  switch (readClangApiValueDefined([&]() { return arg.getKind(); })) {
  case clang::TemplateArgument::Type: {
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
    clang::QualType semantic_qual_type = markClangQualTypeDefined(
        readClangApiValueDefined([&]() { return arg.getAsType(); }));
    const bool requires_source_owned_dependent_identity =
        requiresSourceOwnedDependentTypeIdentity(semantic_qual_type);
    if (!requires_source_owned_dependent_identity) {
      semantic_qual_type =
          markClangQualTypeDefined(semantic_qual_type.getCanonicalType());
    }
    const clang::TemplateTypeParmType *canonical_parameter_type =
        llvm::dyn_cast_or_null<clang::TemplateTypeParmType>(
            markClangTypeObjectDefinedByClass(
                semantic_qual_type.getTypePtrOrNull()));
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
      // A canonical TemplateTypeParmType can rebase its depth relative to the
      // consuming specialization and omit getDecl().  The exact TypeLoc is
      // still the source surface for this same TemplateArgument and carries
      // the stable declaration-token identity.  Preserve that declaration's
      // coordinates on the semantic Sage type; copying the rebased canonical
      // coordinates makes the source and semantic halves name different
      // template parameters.
      SgTemplateType *semantic_parameter =
          SageBuilder::buildTemplateType(exact_source_parameter->get_name());
      semantic_parameter->set_template_parameter_depth(
          exact_source_parameter->get_template_parameter_depth());
      semantic_parameter->set_template_parameter_position(
          exact_source_parameter->get_template_parameter_position());
      semantic_parameter->initialize_canonical_source_identity(
          *source_identity);
      semantic_parameter->set_template_parameter(
          exact_source_parameter->get_template_parameter());
      semantic_parameter->set_packed(
          canonical_parameter_type->isParameterPack());
      semantic_type = semantic_parameter;
    } else {
      semantic_type = buildTypeFromQualifiedType(semantic_qual_type);
    }
    if (source_spelled_type == nullptr || semantic_type == nullptr) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[template-argument-type-surface]: "
              "TemplateArgumentLoc type has source type=%p semantic type=%p\n",
              static_cast<void *>(source_spelled_type),
              static_cast<void *>(semantic_type));
      ROSE_ABORT();
    }

    const clang::TypeSourceInfo *type_info =
        markClangAstObjectDefined(readClangApiValueDefined(
            [&]() { return arg_loc.getTypeSourceInfo(); }));
    if (type_info == nullptr) {
      fprintf(stderr, "REX_FRONTEND_INVARIANT[template-argument-type-surface]: "
                      "type argument has no exact TypeSourceInfo\n");
      ROSE_ABORT();
    }
    clang::TypeLoc type_loc =
        readClangApiValueDefined([&]() { return type_info->getTypeLoc(); });
    markClangTypeLocDataDefined(type_loc);

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
          SgTemplateType *source_pattern =
              new SgTemplateType(packed_pattern->get_name());
          source_pattern->set_template_parameter_position(
              packed_pattern->get_template_parameter_position());
          source_pattern->set_template_parameter_depth(
              packed_pattern->get_template_parameter_depth());
          if (packed_pattern->get_canonical_source_identity().has_value()) {
            source_pattern->initialize_canonical_source_identity(
                *packed_pattern->get_canonical_source_identity());
          }
          source_pattern->set_template_parameter(
              packed_pattern->get_template_parameter());
          source_pattern->get_tpl_args() = packed_pattern->get_tpl_args();
          source_pattern->get_part_spec_tpl_args() =
              packed_pattern->get_part_spec_tpl_args();
          source_pattern->set_packed(false);
          source_spelled_type = source_pattern;
        }
      }
    }

    SgTemplateArgument *template_argument =
        new SgTemplateArgument(semantic_type, explicitlySpecified);
    publishSourceElaborationKind(source_spelled_type, type_loc,
                                 p_compiler_instance);
    template_argument->set_sourceSpelledType(source_spelled_type);

    clang::NestedNameSpecifierLoc qualifier_loc = typeLocQualifierLoc(type_loc);
    clang::NestedNameSpecifier qualifier = markClangNestedNameSpecifierDefined(
        qualifier_loc.getNestedNameSpecifier());
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
                "qualification]: argument=%p structural qualifier "
                "disagrees with Clang identity\n",
                static_cast<void *>(template_argument));
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
              "argument=%p source type=%p/%s leaks use-site qualifier state "
              "through shared semantic identity\n",
              static_cast<void *>(template_argument),
              static_cast<void *>(source_spelled_type),
              source_spelled_type->class_name().c_str());
      ROSE_ABORT();
    }
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
    template_argument->set_type_elaboration_required(
        typeLocSpellsElaboratedKeyword(type_loc,
                                       readClangApiValueDefined([&]() {
                                         return arg_loc.getSourceRange();
                                       }),
                                       p_compiler_instance));

    return template_argument;
  }

  case clang::TemplateArgument::Expression: {
    const clang::Expr *expr =
        markClangExprObjectDefinedByClass(readClangApiValueDefined(
            [&]() { return arg_loc.getSourceExpression(); }));
    if (expr == nullptr) {
      expr = markClangExprObjectDefinedByClass(
          readClangApiValueDefined([&]() { return arg.getAsExpr(); }));
    }
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
          implicit = llvm::cast<clang::ImplicitCastExpr>(
              markClangExprObjectDefinedByClass(implicit));
          wrapped = readClangApiValueDefined(
              [&]() { return implicit->getSubExpr(); });
        } else if (const clang::ConstantExpr *constant =
                       llvm::dyn_cast<clang::ConstantExpr>(expr)) {
          constant = llvm::cast<clang::ConstantExpr>(
              markClangExprObjectDefinedByClass(constant));
          wrapped = readClangApiValueDefined(
              [&]() { return constant->getSubExpr(); });
        } else {
          break;
        }
        expr = markClangExprObjectDefinedByClass(wrapped);
        if (expr == nullptr) {
          fprintf(stderr,
                  "REX_FRONTEND_INVARIANT[template-argument-expression-"
                  "source]: implicit semantic wrapper has no exact source "
                  "operand\n");
          ROSE_ABORT();
        }
      }
      const bool dependent_expression = readClangApiValueDefined([&]() {
        return expr->isTypeDependent() || expr->isValueDependent() ||
               expr->isInstantiationDependent() ||
               expr->containsUnexpandedParameterPack();
      });
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
      if (const clang::DeclRefExpr *direct_source_reference =
              llvm::dyn_cast<clang::DeclRefExpr>(expr)) {
        SgTemplateArgument *semantic_argument = nullptr;
        {
          SemanticExpressionConstruction semantic_expression(
              p_semantic_template_argument_expression_depth,
              "template-argument-expression-source");
          semantic_argument = translateTemplateArgument(
              static_cast<const clang::TemplateArgument &>(arg),
              explicitlySpecified);
        }
        SgExpression *semantic_expression =
            semantic_argument != nullptr ? semantic_argument->get_expression()
                                         : nullptr;
        SgExpression *source_surface_owner =
            semantic_expression != nullptr
                ? exact_source_surface_owner(
                      semantic_expression,
                      "template-argument-expression-source")
                : nullptr;
        if (source_surface_owner != nullptr &&
            applyDirectDeclRefSourceSurface(direct_source_reference,
                                            p_compiler_instance,
                                            source_surface_owner)) {
          return semantic_argument;
        }
        SgExpression *source_expression =
            isSgExpression(Traverse(const_cast<clang::Expr *>(expr)));
        if (source_expression != nullptr) {
          return attach_source_expression_to_semantic_argument(
              source_expression, "template-argument-expression-source",
              semantic_argument);
        }
        break;
      }
      SgNode *node = Traverse(const_cast<clang::Expr *>(expr));
      if (SgExpression *sg_expr = isSgExpression(node)) {
        return attach_source_expression_to_semantic_argument(
            sg_expr, "template-argument-expression-source", nullptr);
      }
    }
    break;
  }

  case clang::TemplateArgument::Integral: {
    if (const clang::Expr *expr =
            markClangExprObjectDefinedByClass(readClangApiValueDefined(
                [&]() { return arg_loc.getSourceIntegralExpression(); }))) {
      SgNode *node = Traverse(const_cast<clang::Expr *>(expr));
      if (SgExpression *sg_expr = isSgExpression(node)) {
        return attach_source_expression_to_semantic_argument(
            sg_expr, "template-argument-integral-source", nullptr);
      }
    }
    break;
  }

  case clang::TemplateArgument::NullPtr: {
    const clang::Expr *expr =
        markClangExprObjectDefinedByClass(readClangApiValueDefined(
            [&]() { return arg_loc.getSourceNullPtrExpression(); }));
    if (expr == nullptr) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[template-argument-nullptr-surface]: "
              "written null pointer argument has no exact source "
              "expression\n");
      ROSE_ABORT();
    }

    SgExpression *sg_expr =
        isSgExpression(Traverse(const_cast<clang::Expr *>(expr)));
    SgType *null_type = buildTypeFromQualifiedType(markClangQualTypeDefined(
        readClangApiValueDefined([&]() { return arg.getNullPtrType(); })));
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
    sg_expr->set_parent(sg_arg);
    return sg_arg;
  }

  case clang::TemplateArgument::Declaration: {
    if (const clang::Expr *expr =
            markClangExprObjectDefinedByClass(readClangApiValueDefined(
                [&]() { return arg_loc.getSourceDeclExpression(); }))) {
      const clang::Expr *source_expression = expr;
      while (true) {
        const clang::Expr *wrapped = nullptr;
        if (const clang::ImplicitCastExpr *implicit =
                llvm::dyn_cast<clang::ImplicitCastExpr>(source_expression)) {
          implicit = llvm::cast<clang::ImplicitCastExpr>(
              markClangExprObjectDefinedByClass(implicit));
          wrapped = readClangApiValueDefined(
              [&]() { return implicit->getSubExpr(); });
        } else if (const clang::ConstantExpr *constant =
                       llvm::dyn_cast<clang::ConstantExpr>(source_expression)) {
          constant = llvm::cast<clang::ConstantExpr>(
              markClangExprObjectDefinedByClass(constant));
          wrapped = readClangApiValueDefined(
              [&]() { return constant->getSubExpr(); });
        } else {
          break;
        }
        source_expression = markClangExprObjectDefinedByClass(wrapped);
        if (source_expression == nullptr) {
          fprintf(stderr,
                  "REX_FRONTEND_INVARIANT[template-argument-declaration]: "
                  "implicit semantic wrapper has no exact source operand\n");
          ROSE_ABORT();
        }
      }
      SgNode *node = Traverse(const_cast<clang::Expr *>(source_expression));
      if (SgExpression *sg_expr = isSgExpression(node)) {
        clang::QualType param_qual_type =
            markClangQualTypeDefined(readClangApiValueDefined(
                [&]() { return arg.getParamTypeForDecl(); }));
        SgType *param_type = buildTypeFromQualifiedType(param_qual_type);
        if (param_type == nullptr) {
          fprintf(stderr,
                  "REX_FRONTEND_INVARIANT[template-argument-declaration]: "
                  "source declaration argument has no exact parameter "
                  "type\n");
          ROSE_ABORT();
        }
        if (templateArgumentNeedsExplicitAddressOf(param_type, sg_expr)) {
          sg_expr = SageBuilder::buildAddressOfOp(sg_expr, param_type);
        }
        SgType *payload_type = sg_expr->get_type();
        SgType *stripped_payload_type =
            payload_type != nullptr ? payload_type->stripTypedefsAndModifiers()
                                    : nullptr;
        SgType *stripped_param_type = param_type->stripTypedefsAndModifiers();
        if (isSgArrayType(stripped_payload_type) != nullptr &&
            isSgPointerType(stripped_param_type) != nullptr) {
          // A declaration-form non-type template argument retains the exact
          // source DeclRefExpr, but Clang's semantic argument type includes
          // array-to-pointer decay and any simultaneous qualification
          // conversion.  Materialize that typed implicit conversion here so
          // the value payload and the parameter type agree without losing the
          // source-spelled operand.
          sg_expr = SageBuilder::buildCastExp_nfi(
              sg_expr, param_type, SgCastExp::e_implicit_cast,
              SgCastExp::e_semantic_conversion_ArrayToPointerDecay,
              SgCastExp::e_value_category_prvalue, {});
          publishCanonicalSemanticExpressionSourceProvenance(
              sg_expr, "source-template-array-to-pointer-conversion");
        }
        SgReferenceType *reference_param = isSgReferenceType(param_type);
        SgType *reference_base = reference_param != nullptr
                                     ? reference_param->get_base_type()
                                     : nullptr;
        SgType *value_type = sg_expr->get_type();
        const bool exact_qualification_binding =
            reference_base != nullptr && value_type != nullptr &&
            !SageInterface::isEquivalentType(reference_base, value_type) &&
            SageInterface::isEquivalentType(
                reference_base->stripTypedefsAndModifiers(),
                value_type->stripTypedefsAndModifiers());
        if (exact_qualification_binding) {
          sg_expr = SageBuilder::buildCastExp_nfi(
              sg_expr, reference_base, SgCastExp::e_implicit_cast,
              SgCastExp::e_semantic_conversion_NoOp,
              SgCastExp::e_value_category_lvalue, {});
          publishCanonicalSemanticExpressionSourceProvenance(
              sg_expr, "source-template-qualification-conversion");
        }
        SgTemplateArgument *sg_arg = new SgTemplateArgument(
            SgTemplateArgument::nontype_argument,
            /*isArrayBoundUnknownType=*/false, param_type, sg_expr,
            /*templateDeclaration=*/nullptr, explicitlySpecified);
        sg_expr->set_parent(sg_arg);
        return sg_arg;
      }
    }
    break;
  }

  case clang::TemplateArgument::Template:
  case clang::TemplateArgument::TemplateExpansion: {
    if (SgTemplateArgument *sg_arg =
            translateTemplateArgument(arg, explicitlySpecified)) {
      if (sg_arg->get_argumentType() ==
              SgTemplateArgument::template_template_argument &&
          sg_arg->get_templateDeclaration() != nullptr) {
        clang::NestedNameSpecifierLoc qualifier_loc =
            markClangNestedNameSpecifierLocDefined(readClangApiValueDefined(
                [&]() { return arg_loc.getTemplateQualifierLoc(); }));
        clang::NestedNameSpecifier qualifier =
            markClangNestedNameSpecifierDefined(
                qualifier_loc.getNestedNameSpecifier());
        if (qualifier) {
          std::string name_str;
          if (SgTemplateDeclaration *template_decl =
                  isSgTemplateDeclaration(sg_arg->get_templateDeclaration())) {
            name_str = template_decl->get_name().getString();
          } else if (SgNonrealDecl *nonreal_decl =
                         isSgNonrealDecl(sg_arg->get_templateDeclaration())) {
            name_str = nonreal_decl->get_name().getString();
          }
          if (!name_str.empty()) {
            SgScopeStatement *scope = SageBuilder::topScopeStack();
            if (scope == nullptr) {
              scope = getGlobalScope();
            }
            SgNonrealType *nr_type =
                buildSemanticNonrealTypeFromNestedNameSpecifier(
                    qualifier, scope, SgName(name_str), nullptr, nullptr);
            if (SgNonrealDecl *nr_decl = isSgNonrealDecl(
                    nr_type ? nr_type->get_declaration() : nullptr)) {
              if (SgDeclarationStatement *template_decl =
                      sg_arg->get_templateDeclaration()) {
                if (template_decl != nr_decl) {
                  nr_decl->set_templateDeclaration(template_decl);
                }
              }
              sg_arg->set_templateDeclaration(nr_decl);
            }
          }
        }
      }
      return sg_arg;
    }
    break;
  }

  default:
    break;
  }

  return translateTemplateArgument(arg, explicitlySpecified);
}

void ClangToSageTranslator::appendTemplateArguments(
    SgTemplateArgumentPtrList &arg_list, const clang::TemplateArgument &arg,
    bool explicitlySpecified) {
  const clang::TemplateArgument &defined_arg =
      markClangTemplateArgumentDefined(arg);
  if (defined_arg.isPackExpansion()) {
    if (SgTemplateArgument *sg_arg =
            translateTemplateArgument(defined_arg, explicitlySpecified)) {
      arg_list.push_back(sg_arg);
      if (sg_arg->get_parent() == nullptr) {
        ensureTemplateArgumentParents(arg_list);
      }
    }
    return;
  }

  if (readClangApiValueDefined([&]() { return defined_arg.getKind(); }) ==
      clang::TemplateArgument::Pack) {
    auto elements =
        markClangTemplateArgumentArrayDefined(readClangApiValueDefined(
            [&]() { return defined_arg.pack_elements(); }));
    if (elements.empty()) {
      // An empty pack contributes no arguments.  The legacy synthetic marker
      // was an untyped list delimiter that escaped into the completed AST and
      // forced every consumer to recognize a non-argument payload.
    } else {
      bool dependent_pack = readClangApiValueDefined(
          [&]() { return defined_arg.containsUnexpandedParameterPack(); });
      if (!dependent_pack && elements.size() == 1) {
        dependent_pack = readClangApiValueDefined([&]() {
          return defined_arg.isInstantiationDependent() ||
                 elements.front().isInstantiationDependent();
        });
      }
      for (const clang::TemplateArgument &pack_arg : elements) {
        size_t before = arg_list.size();
        appendTemplateArguments(arg_list, pack_arg, explicitlySpecified);
        if (dependent_pack) {
          for (size_t i = before; i < arg_list.size(); ++i) {
            SgTemplateArgument *expanded_arg = arg_list[i];
            if (expanded_arg != nullptr &&
                expanded_arg->get_argumentType() !=
                    SgTemplateArgument::start_of_pack_expansion_argument) {
              expanded_arg->set_is_pack_element(true);
            }
          }
        }
      }
    }
    return;
  }

  if (SgTemplateArgument *sg_arg =
          translateTemplateArgument(defined_arg, explicitlySpecified)) {
    arg_list.push_back(sg_arg);
    if (sg_arg->get_parent() == nullptr) {
      ensureTemplateArgumentParents(arg_list);
    }
  }
}

void ClangToSageTranslator::appendTemplateArguments(
    SgTemplateArgumentPtrList &arg_list,
    const clang::TemplateArgumentLoc &arg_loc, bool explicitlySpecified) {
  // TemplateArgumentLoc::getArgument() returns by value.  Keep that value in
  // this frame before marking it: binding the reference returned by the
  // marking helper directly to the call result leaves it referring to a
  // destroyed full-expression temporary.
  clang::TemplateArgument arg =
      readClangApiValueDefined([&]() { return arg_loc.getArgument(); });
  markClangTemplateArgumentDefined(arg);

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
    if (sg_arg->get_parent() == nullptr) {
      ensureTemplateArgumentParents(arg_list);
    }
    return;
  }

  if (arg.isPackExpansion()) {
    if (SgTemplateArgument *sg_arg =
            translateTemplateArgument(arg, explicitlySpecified)) {
      arg_list.push_back(sg_arg);
      if (sg_arg->get_parent() == nullptr) {
        ensureTemplateArgumentParents(arg_list);
      }
    }
    return;
  }

  if (arg.getKind() == clang::TemplateArgument::Pack) {
    auto elements = markClangTemplateArgumentArrayDefined(arg.pack_elements());
    if (elements.empty()) {
      // An empty pack contributes no arguments.  Pack expansion is carried by
      // the typed elements when any exist, never by a sentinel AST node.
    } else {
      bool dependent_pack = arg.containsUnexpandedParameterPack();
      if (!dependent_pack && elements.size() == 1) {
        dependent_pack = arg.isInstantiationDependent() ||
                         elements.front().isInstantiationDependent();
      }
      for (const clang::TemplateArgument &pack_arg : elements) {
        size_t before = arg_list.size();
        appendTemplateArguments(arg_list, pack_arg, explicitlySpecified);
        if (dependent_pack) {
          for (size_t i = before; i < arg_list.size(); ++i) {
            SgTemplateArgument *expanded_arg = arg_list[i];
            if (expanded_arg != nullptr &&
                expanded_arg->get_argumentType() !=
                    SgTemplateArgument::start_of_pack_expansion_argument) {
              expanded_arg->set_is_pack_element(true);
            }
          }
        }
      }
    }
    return;
  }

  if (SgTemplateArgument *sg_arg =
          translateTemplateArgument(arg_loc, explicitlySpecified)) {
    arg_list.push_back(sg_arg);
    if (sg_arg->get_parent() == nullptr) {
      ensureTemplateArgumentParents(arg_list);
    }
  }
}

SgTemplateArgumentPtrList ClangToSageTranslator::buildTemplateArguments(
    const clang::TemplateSpecializationType *clang_type,
    bool reconstruct_explicit_source_surface) {

  SgTemplateArgumentPtrList arg_list;

  clang_type = static_cast<const clang::TemplateSpecializationType *>(
      markClangTypeObjectDefinedByClass(clang_type));
  if (clang_type == nullptr) {
    return arg_list;
  }

  auto args_as_written =
      markClangTemplateArgumentArrayDefined(clang_type->template_arguments());
  const clang::TemplateArgumentList *full_args = nullptr;

  if (clang::QualType qt =
          markClangQualTypeDefined(clang::QualType(clang_type, 0));
      !qt.isNull()) {
    clang::QualType canonical_qt =
        markClangQualTypeDefined(qt.getCanonicalType());
    if (const auto *record_type = llvm::dyn_cast_or_null<clang::RecordType>(
            markClangTypeObjectDefinedByClass(
                canonical_qt.getTypePtrOrNull()))) {
      const clang::CXXRecordDecl *record_decl =
          llvm::dyn_cast_or_null<clang::CXXRecordDecl>(
              markClangDeclObjectDefinedByKind(record_type->getDecl()));
      if (record_decl != nullptr) {
        record_decl = llvm::dyn_cast_or_null<clang::CXXRecordDecl>(
            markClangDeclObjectDefinedByKind(record_decl));
        if (const auto *spec_decl =
                llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(
                    record_decl)) {
          spec_decl = llvm::cast<clang::ClassTemplateSpecializationDecl>(
              markClangDeclObjectDefinedByKind(spec_decl));
          full_args = markClangTemplateArgumentListDefined(
              &spec_decl->getTemplateArgs());
        }
      }
    }
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
    appendTemplateArguments(arg_list, markClangTemplateArgumentDefined(arg),
                            true);
  };

  if (use_full_args) {
    unsigned argument_index = 0;
    for (const clang::TemplateArgument &arg :
         markClangTemplateArgumentArrayDefined(full_args->asArray())) {
      if (argument_index < args_as_written.size()) {
        // The specialization type's written array is the only exact spelling
        // evidence for the explicit prefix.  The completed specialization
        // list is canonical semantic state and can erase typedef/alias sugar
        // (for example, `mbstate_t` becomes its anonymous struct).  Preserve
        // the written argument here and append only the defaulted suffix from
        // the full list.
        append_written_argument(args_as_written[argument_index]);
      } else {
        appendTemplateArguments(arg_list, markClangTemplateArgumentDefined(arg),
                                false);
      }
      ++argument_index;
    }
  } else {
    for (const clang::TemplateArgument &arg : args_as_written) {
      append_written_argument(arg);
    }
  }

  ensureTemplateArgumentParents(arg_list);
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

  ensureTemplateArgumentParents(arg_list);
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
    bool reconstruct_explicit_source_surface) {
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
  const clang::TemplateArgumentList *defined_args =
      markClangTemplateArgumentListDefined(&args);
  const size_t semantic_expanded_count =
      countExpandedTemplateArguments(*defined_args);
  if (explicit_count > semantic_expanded_count) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[template-argument-prefix]: requested "
            "%zu explicit arguments from a semantic list containing only "
            "%zu expanded arguments in %u top-level arguments\n",
            explicit_count, semantic_expanded_count, defined_args->size());
    ROSE_ABORT();
  }
  for (unsigned i = 0; i < defined_args->size(); ++i) {
    const size_t prior_argument_count = arg_list.size();
    const clang::TemplateArgument &argument =
        markClangTemplateArgumentDefined(defined_args->get(i));
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
    appendTemplateArguments(arg_list, argument,
                            reconstruct_explicit_source_surface &&
                                overlaps_explicit_prefix);
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
  for (size_t index = 0; index < arg_list.size(); ++index) {
    SgTemplateArgument *argument = arg_list[index];
    if (argument == nullptr ||
        argument->get_explicitlySpecified() != (index < explicit_count)) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[template-argument-prefix]: semantic "
              "argument=%zu did not preserve the exact expanded explicit "
              "prefix of length=%zu\n",
              index, explicit_count);
      ROSE_ABORT();
    }
  }
  ensureTemplateArgumentParents(arg_list);
  return arg_list;
}

void ClangToSageTranslator::ensureTemplateArgumentParents(
    SgTemplateArgumentPtrList &args) {
  for (SgTemplateArgument *arg : args) {
    ASSERT_not_null(arg);
  }
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
  const clang::TemplateArgumentList *defined_args =
      markClangTemplateArgumentListDefined(&args);
  for (const clang::TemplateArgument &argument : defined_args->asArray()) {
    count += countExpandedTemplateArgument(argument);
  }
  return count;
}

SgNonrealType *
ClangToSageTranslator::buildNonrealTypeForNestedNameSpecifierType(
    const clang::Type *clang_type, SgScopeStatement *scope,
    bool prefer_current_scope, SgScopeStatement *template_use_scope) {
  clang_type = markClangTypeObjectDefinedByClass(clang_type);
  if (clang_type == nullptr) {
    return nullptr;
  }

  if (const clang::DecltypeType *decltype_type =
          llvm::dyn_cast<clang::DecltypeType>(clang_type)) {
    decltype_type = llvm::dyn_cast_or_null<clang::DecltypeType>(
        markClangTypeObjectDefinedByClass(decltype_type));
    ASSERT_not_null(decltype_type);
    clang::QualType underlying_type =
        markClangQualTypeDefined(readClangApiValueDefined(
            [&]() { return decltype_type->getUnderlyingType(); }));
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
    subst = llvm::dyn_cast_or_null<clang::SubstTemplateTypeParmType>(
        markClangTypeObjectDefinedByClass(subst));
    clang::QualType replacement =
        markClangQualTypeDefined(subst->getReplacementType());
    return buildNonrealTypeForNestedNameSpecifierType(
        replacement.getTypePtrOrNull(), scope, prefer_current_scope,
        template_use_scope);
  }

  if (const clang::SubstTemplateTypeParmPackType *pack =
          llvm::dyn_cast<clang::SubstTemplateTypeParmPackType>(clang_type)) {
    pack = llvm::dyn_cast_or_null<clang::SubstTemplateTypeParmPackType>(
        markClangTypeObjectDefinedByClass(pack));
    std::string name_str;
    if (const clang::TemplateTypeParmDecl *decl =
            llvm::dyn_cast_or_null<clang::TemplateTypeParmDecl>(
                markClangDeclObjectDefinedByKind(
                    pack->getReplacedParameter()))) {
      name_str = preferHigherQualityTemplateParamName(name_str,
                                                      decl->getNameAsString());
      std::string scope_name = resolveTemplateParameterNameFromSageScope(
          scope, decl->getDepth(), decl->getIndex());
      if (scope_name.empty()) {
        scope_name = resolveTemplateParameterNameFromSageScope(
            SageBuilder::topScopeStack(), decl->getDepth(), decl->getIndex());
      }
      name_str = preferHigherQualityTemplateParamName(name_str, scope_name);
      if (name_str.empty()) {
        name_str = resolveTemplateParameterNameFromSageScope(
            scope, decl->getDepth(), decl->getIndex());
      }
      if (name_str.empty()) {
        name_str = resolveTemplateParameterNameFromSageScope(
            SageBuilder::topScopeStack(), decl->getDepth(), decl->getIndex());
      }
      if (name_str.empty()) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[template-type-parameter-pack]: "
                "depth=%u index=%u has no exact source name\n",
                decl->getDepth(), decl->getIndex());
        ROSE_ABORT();
      }
    }
    if (name_str.empty()) {
      if (const clang::IdentifierInfo *id = pack->getIdentifier()) {
        name_str = normalizeTemplateTypeParamName(id->getName().str());
      }
    }
    if (name_str.empty()) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[template-type-parameter-pack]: "
              "substituted pack index=%u has no exact source name\n",
              pack->getIndex());
      ROSE_ABORT();
    }
    ROSE_ASSERT(!name_str.empty());

    SgNonrealType *nrtype = SageBuilder::buildSemanticNonrealType(
        SgName(name_str), scope, nullptr, nullptr);
    if (SgNonrealDecl *nrdecl =
            isSgNonrealDecl(nrtype ? nrtype->get_declaration() : nullptr)) {
      nrdecl->set_is_template_param(true);
    }
    return nrtype;
  }

  auto sanitize_nonreal_name = [&](const std::string &raw) -> std::string {
    if (raw.empty()) {
      return raw;
    }
    std::vector<std::string> components =
        splitQualifiedNameOutsideTemplates(raw);
    std::string base = components.empty() ? raw : components.back();
    base = stripTemplateArgs(base);
    return trimWhitespace(base);
  };

  auto build_with_qualifier =
      [&](clang::NestedNameSpecifier qualifier, const std::string &name,
          const SgTemplateArgumentPtrList *tpl_args) -> SgNonrealType * {
    qualifier = markClangNestedNameSpecifierDefined(qualifier);
    std::string base_name = sanitize_nonreal_name(name);
    ROSE_ASSERT(!base_name.empty());
    std::vector<std::string> name_components =
        splitQualifiedNameOutsideTemplates(name);
    std::string semantic_name =
        trimWhitespace(name_components.empty() ? name : name_components.back());
    ROSE_ASSERT(tpl_args == nullptr || !semantic_name.empty());
    const SgName semantic_sg_name(semantic_name);
    if (prefer_current_scope) {
      qualifier = std::nullopt;
    }
    if (qualifier) {
      return buildSemanticNonrealTypeFromNestedNameSpecifier(
          qualifier, scope, SgName(base_name), tpl_args,
          tpl_args != nullptr ? &semantic_sg_name : nullptr);
    }
    if (tpl_args != nullptr && template_use_scope != nullptr &&
        template_use_scope != scope) {
      SgNonrealType *qualified_base = SageBuilder::buildSemanticNonrealType(
          SgName(base_name), scope, nullptr, nullptr);
      SgNonrealDecl *qualified_base_declaration = isSgNonrealDecl(
          qualified_base != nullptr ? qualified_base->get_declaration()
                                    : nullptr);
      if (!isExactSimpleNonrealTemplateBase(qualified_base_declaration,
                                            SgName(base_name))) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[nonreal-template-use-owner]: "
                "nested qualifier terminal=%s has no exact simple base\n",
                base_name.c_str());
        ROSE_ABORT();
      }
      SgNonrealType *use_type = SageBuilder::buildSemanticNonrealType(
          SgName(base_name), template_use_scope, tpl_args, &semantic_sg_name);
      SgNonrealDecl *use_declaration = isSgNonrealDecl(
          use_type != nullptr ? use_type->get_declaration() : nullptr);
      if (use_declaration == nullptr ||
          use_declaration == qualified_base_declaration) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[nonreal-template-use-owner]: "
                "nested qualifier terminal=%s base=%p and lexical use=%p do "
                "not form distinct typed identities\n",
                base_name.c_str(),
                static_cast<void *>(qualified_base_declaration),
                static_cast<void *>(use_declaration));
        ROSE_ABORT();
      }
      use_declaration->set_templateDeclaration(qualified_base_declaration);
      return use_type;
    }
    return SageBuilder::buildSemanticNonrealType(
        SgName(base_name), scope, tpl_args,
        tpl_args != nullptr ? &semantic_sg_name : nullptr);
  };
  auto attach_named_decl_to_nonreal =
      [&](SgNonrealType *nrtype,
          const clang::NamedDecl *decl) -> SgNonrealType * {
    decl = llvm::dyn_cast_or_null<clang::NamedDecl>(
        markClangDeclObjectDefinedByKind(decl));
    if (nrtype == nullptr || decl == nullptr) {
      return nrtype;
    }

    SgNonrealDecl *nrdecl = isSgNonrealDecl(nrtype->get_declaration());
    if (nrdecl == nullptr) {
      return nrtype;
    }

    SgDeclarationStatement *sg_decl = lookupSgDeclarationForClangDecl(
        const_cast<clang::NamedDecl *>(decl), /*allow_on_demand=*/true);
    sg_decl = normalizeNonrealTemplateDeclarationTarget(sg_decl);

    if (sg_decl != nullptr) {
      nrdecl->set_templateDeclaration(sg_decl);
    } else if (llvm::isa<clang::ClassTemplateSpecializationDecl>(decl) ||
               llvm::isa<clang::ClassTemplatePartialSpecializationDecl>(decl) ||
               llvm::isa<clang::ClassTemplateDecl>(decl) ||
               llvm::isa<clang::TypeAliasTemplateDecl>(decl)) {
      linkNonrealTemplateDeclaration(
          nrdecl, const_cast<clang::Decl *>(llvm::cast<clang::Decl>(decl)),
          "dependent-named-type");
    }

    return nrtype;
  };
  auto attach_translated_decl_to_nonreal =
      [&](SgNonrealType *nrtype,
          SgDeclarationStatement *decl) -> SgNonrealType * {
    if (nrtype == nullptr || decl == nullptr) {
      return nrtype;
    }

    SgNonrealDecl *nrdecl = isSgNonrealDecl(nrtype->get_declaration());
    if (nrdecl == nullptr) {
      return nrtype;
    }

    decl = normalizeNonrealTemplateDeclarationTarget(decl);
    if (decl != nullptr) {
      nrdecl->set_templateDeclaration(decl);
    }

    return nrtype;
  };
  auto translated_decl_resolves_template_target =
      [&](SgDeclarationStatement *decl) -> bool {
    decl = normalizeNonrealTemplateDeclarationTarget(decl);
    return isSgTemplateInstantiationDecl(decl) != nullptr ||
           isSgTemplateClassDeclaration(decl) != nullptr ||
           isSgTemplateTypedefDeclaration(decl) != nullptr ||
           isSgTemplateVariableDeclaration(decl) != nullptr;
  };
  std::function<SgDeclarationStatement *(SgType *)>
      declaration_from_translated_type =
          [&](SgType *translated_type) -> SgDeclarationStatement * {
    if (translated_type == nullptr) {
      return nullptr;
    }

    SgType *base_type = translated_type->findBaseType();
    if (SgNamedType *named_type = isSgNamedType(base_type)) {
      return named_type->get_declaration();
    }

    if (SgModifierType *modifier_type = isSgModifierType(translated_type)) {
      return declaration_from_translated_type(modifier_type->get_base_type());
    }

    if (SgReferenceType *reference_type = isSgReferenceType(translated_type)) {
      return declaration_from_translated_type(reference_type->get_base_type());
    }

    if (SgPointerType *pointer_type = isSgPointerType(translated_type)) {
      return declaration_from_translated_type(pointer_type->get_base_type());
    }

    if (SgArrayType *array_type = isSgArrayType(translated_type)) {
      return declaration_from_translated_type(array_type->get_base_type());
    }

    return nullptr;
  };
  auto materialize_translated_template_decl =
      [&](clang::QualType qt) -> SgDeclarationStatement * {
    qt = markClangQualTypeDefined(qt);
    if (qt.isNull()) {
      return nullptr;
    }

    SgType *translated_type = buildTypeFromQualifiedType(qt);
    return declaration_from_translated_type(translated_type);
  };
  if (const clang::UnresolvedUsingType *uut =
          llvm::dyn_cast<clang::UnresolvedUsingType>(clang_type)) {
    uut = llvm::dyn_cast_or_null<clang::UnresolvedUsingType>(
        markClangTypeObjectDefinedByClass(uut));
    const clang::UnresolvedUsingTypenameDecl *decl =
        llvm::dyn_cast_or_null<clang::UnresolvedUsingTypenameDecl>(
            markClangDeclObjectDefinedByKind(uut->getDecl()));
    std::string name_str = decl ? decl->getNameAsString() : "";
    ROSE_ASSERT(!name_str.empty());
    return build_with_qualifier(
        decl != nullptr
            ? markClangNestedNameSpecifierDefined(decl->getQualifier())
            : std::nullopt,
        name_str, nullptr);
  }

  if (const clang::TypedefType *typedef_type =
          llvm::dyn_cast<clang::TypedefType>(clang_type)) {
    typedef_type = llvm::dyn_cast_or_null<clang::TypedefType>(
        markClangTypeObjectDefinedByClass(typedef_type));
    const clang::TypedefNameDecl *decl =
        llvm::dyn_cast_or_null<clang::TypedefNameDecl>(
            markClangDeclObjectDefinedByKind(typedef_type->getDecl()));
    std::string name_str = decl != nullptr ? decl->getNameAsString() : "";
    clang::NestedNameSpecifier qualifier =
        markClangNestedNameSpecifierDefined(typedef_type->getQualifier());
    if (!name_str.empty() && qualifier) {
      return attach_named_decl_to_nonreal(
          build_with_qualifier(qualifier, name_str, nullptr), decl);
    }
  }

  if (const clang::UsingType *using_type =
          llvm::dyn_cast<clang::UsingType>(clang_type)) {
    using_type = llvm::dyn_cast_or_null<clang::UsingType>(
        markClangTypeObjectDefinedByClass(using_type));
    const clang::UsingShadowDecl *decl =
        llvm::dyn_cast_or_null<clang::UsingShadowDecl>(
            markClangDeclObjectDefinedByKind(using_type->getDecl()));
    std::string name_str = decl != nullptr ? decl->getNameAsString() : "";
    clang::NestedNameSpecifier qualifier =
        markClangNestedNameSpecifierDefined(using_type->getQualifier());
    if (!name_str.empty() && qualifier) {
      return attach_named_decl_to_nonreal(
          build_with_qualifier(qualifier, name_str, nullptr), decl);
    }
  }

  if (const clang::DependentNameType *dnt =
          llvm::dyn_cast<clang::DependentNameType>(clang_type)) {
    dnt = llvm::dyn_cast_or_null<clang::DependentNameType>(
        markClangTypeObjectDefinedByClass(dnt));
    const clang::IdentifierInfo *id = dnt->getIdentifier();
    ROSE_ASSERT(id != nullptr);
    return build_with_qualifier(
        markClangNestedNameSpecifierDefined(dnt->getQualifier()),
        id->getName().str(), nullptr);
  }

  if (const clang::TemplateSpecializationType *tst =
          llvm::dyn_cast<clang::TemplateSpecializationType>(clang_type)) {
    tst = llvm::dyn_cast_or_null<clang::TemplateSpecializationType>(
        markClangTypeObjectDefinedByClass(tst));
    clang::TemplateName tname =
        markClangTemplateNameDefined(tst->getTemplateName());
    auto resolve_template_decl =
        [&](const clang::TemplateName &name) -> clang::TemplateDecl * {
      clang::TemplateName current = markClangTemplateNameDefined(name);
      for (;;) {
        if (clang::TemplateDecl *decl = current.getAsTemplateDecl()) {
          return const_cast<clang::TemplateDecl *>(
              llvm::dyn_cast_or_null<clang::TemplateDecl>(
                  markClangDeclObjectDefinedByKind(decl)));
        }
        if (const clang::QualifiedTemplateName *qtn =
                current.getAsQualifiedTemplateName()) {
          clang::TemplateName underlying =
              markClangTemplateNameDefined(qtn->getUnderlyingTemplate());
          if (clang::TemplateDecl *decl = underlying.getAsTemplateDecl()) {
            return const_cast<clang::TemplateDecl *>(
                llvm::dyn_cast_or_null<clang::TemplateDecl>(
                    markClangDeclObjectDefinedByKind(decl)));
          }
          current = underlying;
          continue;
        }
        if (const clang::SubstTemplateTemplateParmStorage *subst =
                current.getAsSubstTemplateTemplateParm()) {
          current = markClangTemplateNameDefined(subst->getReplacement());
          continue;
        }
        if (clang::UsingShadowDecl *using_shadow =
                current.getAsUsingShadowDecl()) {
          using_shadow = const_cast<clang::UsingShadowDecl *>(
              llvm::dyn_cast_or_null<clang::UsingShadowDecl>(
                  markClangDeclObjectDefinedByKind(using_shadow)));
          return const_cast<clang::TemplateDecl *>(
              llvm::dyn_cast_or_null<clang::TemplateDecl>(
                  markClangDeclObjectDefinedByKind(
                      using_shadow->getTargetDecl())));
        }
        return nullptr;
      }
    };
    clang::TemplateDecl *template_decl = resolve_template_decl(tname);
    SgTemplateParameter *template_parameter_surface = nullptr;
    SgTemplateDeclaration *template_parameter_identity = nullptr;
    SgDeclarationStatement *template_parameter_owner = nullptr;
    auto resolve_template_parameter_surface =
        [&](clang::TemplateTemplateParmDecl *parameter)
        -> SgTemplateParameter * {
      parameter = const_cast<clang::TemplateTemplateParmDecl *>(
          llvm::dyn_cast_or_null<clang::TemplateTemplateParmDecl>(
              markClangDeclObjectDefinedByKind(parameter)));
      ASSERT_not_null(parameter);

      SgTemplateParameter *surface = lookupActiveTemplateParameterSurface(
          parameter, "nested-template-specialization-type");
      auto publish_surface_owner = [&](SgTemplateParameter *candidate) {
        if (candidate == nullptr) {
          return;
        }
        if (SgDeclarationStatement *parent =
                isSgDeclarationStatement(candidate->get_parent())) {
          template_parameter_owner = parent;
          return;
        }
        for (auto frame = p_template_parameter_surface_stack.rbegin();
             frame != p_template_parameter_surface_stack.rend(); ++frame) {
          if (frame->sage_parameters == nullptr || frame->owner == nullptr ||
              std::find(frame->sage_parameters->begin(),
                        frame->sage_parameters->end(),
                        candidate) == frame->sage_parameters->end()) {
            continue;
          }
          if (template_parameter_owner != nullptr &&
              template_parameter_owner != frame->owner) {
            fprintf(stderr,
                    "REX_FRONTEND_INVARIANT[template-template-qualifier-"
                    "owner]: Sage parameter=%p belongs to multiple active "
                    "declaration owners=%p/%p\n",
                    static_cast<void *>(candidate),
                    static_cast<void *>(template_parameter_owner),
                    static_cast<void *>(frame->owner));
            ROSE_ABORT();
          }
          template_parameter_owner = frame->owner;
        }
      };
      publish_surface_owner(surface);
      if (surface == nullptr) {
        const clang::DeclContext *active_context =
            currentTemplateParameterDeclContext();
        const clang::TemplateTemplateParmDecl *active_parameter = nullptr;
        for (const clang::TemplateParameterList *level :
             collectTemplateParameterLevelsFromDeclContext(active_context)) {
          level = markClangTemplateParameterListDefined(level);
          const unsigned parameter_index = parameter->getIndex();
          if (level == nullptr || parameter_index >= level->size()) {
            continue;
          }
          auto *candidate =
              llvm::dyn_cast_or_null<clang::TemplateTemplateParmDecl>(
                  markClangDeclObjectDefinedByKind(
                      level->getParam(parameter_index)));
          if (candidate == nullptr ||
              candidate->getDepth() != parameter->getDepth() ||
              candidate->getIndex() != parameter_index) {
            continue;
          }
          const clang::DeclContext *parameter_context =
              markClangDeclContextObjectDefined(parameter->getDeclContext());
          const bool clang_lost_written_owner =
              parameter_context != nullptr &&
              parameter_context->isTranslationUnit();
          if (!clang_lost_written_owner &&
              candidate->getCanonicalDecl() != parameter->getCanonicalDecl()) {
            continue;
          }
          if (active_parameter != nullptr &&
              active_parameter->getCanonicalDecl() !=
                  candidate->getCanonicalDecl()) {
            fprintf(stderr,
                    "REX_FRONTEND_INVARIANT[template-template-qualifier-"
                    "owner]: depth=%u index=%u resolves to multiple active "
                    "parameter families\n",
                    parameter->getDepth(), parameter_index);
            ROSE_ABORT();
          }
          active_parameter = candidate;
        }
        if (active_parameter != nullptr) {
          surface = lookupActiveTemplateParameterSurface(
              active_parameter,
              "nested-template-specialization-type-active-context");
          if (surface == nullptr) {
            auto active_mapping = p_decl_translation_map.find(
                const_cast<clang::TemplateTemplateParmDecl *>(
                    active_parameter));
            if (active_mapping != p_decl_translation_map.end()) {
              surface = isSgTemplateParameter(active_mapping->second);
            }
          }
          publish_surface_owner(surface);
        }
      }
      if (surface == nullptr) {
        auto mapped = p_decl_translation_map.find(parameter);
        if (mapped != p_decl_translation_map.end()) {
          surface = isSgTemplateParameter(mapped->second);
        }
        publish_surface_owner(surface);
      }
      if (surface == nullptr) {
        const clang::DeclContext *parameter_context =
            markClangDeclContextObjectDefined(parameter->getDeclContext());
        if (parameter_context != nullptr &&
            parameter_context->isTranslationUnit()) {
          for (auto frame = p_template_parameter_surface_stack.rbegin();
               frame != p_template_parameter_surface_stack.rend(); ++frame) {
            const clang::TemplateParameterList *clang_parameters =
                markClangTemplateParameterListDefined(
                    const_cast<clang::TemplateParameterList *>(
                        frame->clang_parameters));
            if (clang_parameters == nullptr ||
                frame->sage_parameters == nullptr) {
              continue;
            }
            for (unsigned parameter_index = 0;
                 parameter_index < clang_parameters->size();
                 ++parameter_index) {
              auto *candidate =
                  llvm::dyn_cast_or_null<clang::TemplateTemplateParmDecl>(
                      markClangDeclObjectDefinedByKind(
                          clang_parameters->getParam(parameter_index)));
              if (candidate == nullptr ||
                  candidate->getDepth() != parameter->getDepth() ||
                  candidate->getIndex() != parameter->getIndex()) {
                continue;
              }
              if (parameter_index >= frame->sage_parameters->size() ||
                  (*frame->sage_parameters)[parameter_index] == nullptr) {
                fprintf(stderr,
                        "REX_FRONTEND_INVARIANT[template-template-qualifier-"
                        "ordering]: depth=%u index=%u was referenced before "
                        "its exact active Sage parameter was constructed\n",
                        parameter->getDepth(), parameter->getIndex());
                ROSE_ABORT();
              }
              SgTemplateParameter *candidate_surface =
                  (*frame->sage_parameters)[parameter_index];
              if (surface != nullptr && surface != candidate_surface) {
                fprintf(stderr,
                        "REX_FRONTEND_INVARIANT[template-template-qualifier-"
                        "owner]: depth=%u index=%u resolves to multiple active "
                        "Sage parameter surfaces=%p/%p\n",
                        parameter->getDepth(), parameter->getIndex(),
                        static_cast<void *>(surface),
                        static_cast<void *>(candidate_surface));
                ROSE_ABORT();
              }
              surface = candidate_surface;
              publish_surface_owner(surface);
            }
          }
        }
      }
      if (surface == nullptr) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[template-template-qualifier-lookup]: "
                "parameter=%p name='%s' depth=%u index=%u active-surfaces=%zu "
                "has no exact typed Sage surface\n",
                static_cast<void *>(parameter),
                parameter->getNameAsString().c_str(), parameter->getDepth(),
                parameter->getIndex(),
                p_template_parameter_surface_stack.size());
        ROSE_ABORT();
      }
      return surface;
    };

    std::string base_name;
    if (auto *template_parameter =
            llvm::dyn_cast_or_null<clang::TemplateTemplateParmDecl>(
                template_decl)) {
      template_parameter_surface =
          resolve_template_parameter_surface(template_parameter);
      template_parameter_identity =
          template_parameter_surface != nullptr
              ? isSgTemplateDeclaration(
                    template_parameter_surface->get_templateDeclaration())
              : nullptr;
      if (template_parameter_identity == nullptr ||
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
      base_name = template_parameter_identity->get_name().getString();
    } else {
      base_name = getTemplateNameBase(tname);
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
      for (const clang::TemplateArgument &arg :
           markClangTemplateArgumentArrayDefined(tst->template_arguments())) {
        appendTemplateArguments(tpl_args, markClangTemplateArgumentDefined(arg),
                                /*explicitlySpecified=*/true);
      }
    }
    ensureTemplateArgumentParents(tpl_args);
    clang::NestedNameSpecifier qualifier = std::nullopt;
    bool has_template_keyword = false;
    if (const clang::QualifiedTemplateName *qtn =
            tname.getAsQualifiedTemplateName()) {
      qualifier = markClangNestedNameSpecifierDefined(qtn->getQualifier());
      has_template_keyword = qtn->hasTemplateKeyword();
    } else if (const clang::DependentTemplateName *dtn =
                   tname.getAsDependentTemplateName()) {
      qualifier = markClangNestedNameSpecifierDefined(dtn->getQualifier());
      has_template_keyword = true;
    }
    if (prefer_current_scope) {
      qualifier = std::nullopt;
    }
    SgDeclarationStatement *translated_decl = template_parameter_identity;
    clang::Decl *translated_decl_key = nullptr;
    if (!tst->isDependentType()) {
      clang::QualType translated_qt =
          markClangQualTypeDefined(clang::QualType(tst, 0));
      if (const clang::CXXRecordDecl *record_decl =
              cxxRecordDeclFromQualTypeWithoutDefinitionLookup(translated_qt)) {
        translated_decl_key = const_cast<clang::CXXRecordDecl *>(
            llvm::dyn_cast_or_null<clang::CXXRecordDecl>(
                markClangDeclObjectDefinedByKind(record_decl)));
      }
      // An explicit-instantiation signature is a declaration-local type-use
      // surface.  Translating this same TemplateSpecializationType here would
      // recurse back through VisitTemplateSpecializationType, while using the
      // canonical specialization would lose the fact that every semantic
      // argument is explicit at this use.  The canonical declaration has
      // already been materialized by the function specialization; use it only
      // as the target identity for the new nonreal type.
      if (p_explicit_template_id_type_use_depth != 0) {
        if (template_decl == nullptr) {
          fprintf(stderr,
                  "REX_FRONTEND_INVARIANT[explicit-template-id-type-use]: "
                  "nondependent template-id has no exact primary template "
                  "declaration\n");
          ROSE_ABORT();
        }
        translated_decl_key = const_cast<clang::TemplateDecl *>(
            llvm::dyn_cast_or_null<clang::TemplateDecl>(
                markClangDeclObjectDefinedByKind(template_decl)));
        translated_decl = normalizeNonrealTemplateDeclarationTarget(
            lookupSgDeclarationForClangDecl(translated_decl_key,
                                            /*allow_on_demand=*/true));
        if (translated_decl == nullptr) {
          fprintf(stderr,
                  "REX_FRONTEND_INVARIANT[explicit-template-id-type-use]: "
                  "primary template=%p/%s has no exact translated Sage "
                  "identity\n",
                  static_cast<void *>(translated_decl_key),
                  translated_decl_key->getDeclKindName());
          ROSE_ABORT();
        }
      } else if (tst->isTypeAlias()) {
        clang::QualType aliased_qt =
            markClangQualTypeDefined(tst->getAliasedType());
        translated_decl = materialize_translated_template_decl(aliased_qt);
        if (translated_decl_key == nullptr) {
          if (const clang::CXXRecordDecl *aliased_record_decl =
                  cxxRecordDeclFromQualTypeWithoutDefinitionLookup(
                      aliased_qt)) {
            translated_decl_key = const_cast<clang::CXXRecordDecl *>(
                llvm::dyn_cast_or_null<clang::CXXRecordDecl>(
                    markClangDeclObjectDefinedByKind(aliased_record_decl)));
          }
        }
      }
      if (translated_decl == nullptr &&
          p_explicit_template_id_type_use_depth == 0) {
        translated_decl = materialize_translated_template_decl(translated_qt);
      }
      if (translated_decl_key == nullptr && template_decl != nullptr) {
        translated_decl_key = const_cast<clang::Decl *>(
            markClangDeclObjectDefinedByKind(resolve_template_decl(tname)));
      }
    }
    // A dependent TemplateSpecializationType can still name an exact primary
    // template declaration.  The semantic type has no instantiated class
    // declaration yet, but dropping the known primary here creates an
    // unlinked SgNonrealDecl that cannot be matched to the source TypeLoc.
    if (translated_decl_key == nullptr && template_decl != nullptr &&
        template_parameter_identity == nullptr) {
      translated_decl_key = const_cast<clang::TemplateDecl *>(
          llvm::dyn_cast_or_null<clang::TemplateDecl>(
              markClangDeclObjectDefinedByKind(template_decl)));
    }
    SgNonrealType *nrtype = nullptr;
    const SgName semantic_name(buildExactTemplateInstantiationName(
        base_name, tst->template_arguments(),
        currentTemplateParameterDeclContext()));
    if (qualifier) {
      nrtype =
          build_with_qualifier(qualifier, semantic_name.getString(), &tpl_args);
    } else {
      clang::NestedNameSpecifier ns_qualifier = std::nullopt;
      clang::DeclContext *template_decl_context =
          template_decl != nullptr ? markClangDeclContextObjectDefined(
                                         template_decl->getDeclContext())
                                   : nullptr;
      if (!prefer_current_scope && template_parameter_identity == nullptr &&
          template_decl_context != nullptr && p_compiler_instance != nullptr &&
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
        nrtype = build_with_qualifier(ns_qualifier, semantic_name.getString(),
                                      &tpl_args);
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
          clang::DeclContext *decl_context = markClangDeclContextObjectDefined(
              template_decl->getDeclContext());
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
        if (template_use_scope != nullptr &&
            template_use_scope != template_scope) {
          SgNonrealType *qualified_base = SageBuilder::buildSemanticNonrealType(
              SgName(base_name), template_scope, nullptr, nullptr);
          SgNonrealDecl *qualified_base_declaration = isSgNonrealDecl(
              qualified_base != nullptr ? qualified_base->get_declaration()
                                        : nullptr);
          if (!isExactSimpleNonrealTemplateBase(qualified_base_declaration,
                                                SgName(base_name))) {
            fprintf(stderr,
                    "REX_FRONTEND_INVARIANT[nonreal-template-use-owner]: "
                    "dependent qualifier terminal=%s has no exact simple "
                    "base\n",
                    base_name.c_str());
            ROSE_ABORT();
          }
          nrtype = SageBuilder::buildSemanticNonrealType(
              SgName(base_name), template_use_scope, &tpl_args, &semantic_name);
          SgNonrealDecl *use_declaration = isSgNonrealDecl(
              nrtype != nullptr ? nrtype->get_declaration() : nullptr);
          if (use_declaration == nullptr ||
              use_declaration == qualified_base_declaration) {
            fprintf(stderr,
                    "REX_FRONTEND_INVARIANT[nonreal-template-use-owner]: "
                    "dependent qualifier terminal=%s base=%p and lexical "
                    "use=%p do not form distinct typed identities\n",
                    base_name.c_str(),
                    static_cast<void *>(qualified_base_declaration),
                    static_cast<void *>(use_declaration));
            ROSE_ABORT();
          }
          use_declaration->set_templateDeclaration(qualified_base_declaration);
        } else {
          nrtype = SageBuilder::buildSemanticNonrealType(
              SgName(base_name), template_scope, &tpl_args, &semantic_name);
        }
      }
    }
    if (SgNonrealDecl *nrdecl =
            isSgNonrealDecl(nrtype ? nrtype->get_declaration() : nullptr)) {
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
      translated_decl_key = const_cast<clang::Decl *>(
          markClangDeclObjectDefinedByKind(translated_decl_key));
      if (auto *spec_decl =
              llvm::dyn_cast_or_null<clang::ClassTemplateSpecializationDecl>(
                  translated_decl_key)) {
        queuePendingImplicitClassTemplateSpecialization(spec_decl);
      }
      if (translated_decl_key != nullptr &&
          !translated_decl_resolves_template_target(translated_decl)) {
        linkNonrealTemplateDeclaration(nrdecl, translated_decl_key,
                                       "dependent-template-specialization");
      }
    }
    nrtype = attach_translated_decl_to_nonreal(nrtype, translated_decl);
    return nrtype;
  }

  if (const clang::TagType *tag_type =
          llvm::dyn_cast<clang::TagType>(clang_type)) {
    tag_type = llvm::dyn_cast_or_null<clang::TagType>(
        markClangTypeObjectDefinedByClass(tag_type));
    const clang::TagDecl *decl =
        tag_type != nullptr
            ? markClangSpecificDeclDefined(readClangApiValueDefined(
                  [&]() { return tag_type->getDecl(); }))
            : nullptr;
    std::string name_str = decl != nullptr ? readClangApiValueDefined([&]() {
      return decl->getNameAsString();
    })
                                           : "";
    clang::NestedNameSpecifier qualifier =
        tag_type != nullptr
            ? markClangNestedNameSpecifierDefined(readClangApiValueDefined(
                  [&]() { return tag_type->getQualifier(); }))
            : std::nullopt;
    if (!name_str.empty() && qualifier) {
      return attach_named_decl_to_nonreal(
          build_with_qualifier(qualifier, name_str, nullptr), decl);
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
                       active_semantic_parameter) != 1 ||
            construction_scope != nullptr) {
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
              p_compiler_instance != nullptr
                  ? param_decl->getLocation().printToString(
                        p_compiler_instance->getSourceManager())
                  : "<no-compiler-instance>";
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

      SgDeclarationStatement *owner =
          resolveTemplateParameterOwner(param_decl, /*allow_on_demand=*/true);
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
    auto resolve_from_decl_context_stack = [&](unsigned depth,
                                               unsigned index) -> std::string {
      if (p_template_parameter_decl_context_stack.empty()) {
        return "";
      }
      return resolveTemplateParameterNameFromDeclContext(
          p_template_parameter_decl_context_stack.back(), depth, index);
    };
    auto resolve_from_sage_scope = [&](unsigned depth,
                                       unsigned index) -> std::string {
      std::string resolved = resolveTemplateParameterNameFromSageScope(
          template_param_scope, depth, index);
      return resolved;
    };

    if (const clang::TemplateTypeParmDecl *decl = ttp->getDecl()) {
      auto map_it = p_decl_translation_map.find(
          const_cast<clang::TemplateTypeParmDecl *>(decl));
      if (map_it != p_decl_translation_map.end()) {
        if (SgTemplateParameter *sg_param =
                isSgTemplateParameter(map_it->second)) {
          if (SgTemplateType *existing_type =
                  isSgTemplateType(sg_param->get_type())) {
            name_str = existing_type->get_name().getString();
            if (existing_type->get_packed() || ttp->isParameterPack()) {
              existing_type->set_packed(true);
            }
          }

          SgNode *parameter_parent = sg_param->get_parent();
          if (parameter_parent != nullptr &&
              isSgDeclarationStatement(parameter_parent) == nullptr) {
            fprintf(stderr,
                    "REX_CFE_TYPE_INVARIANT[template-parameter-owner]: "
                    "parameter=%p has non-declaration parent=%p/%s\n",
                    static_cast<void *>(sg_param),
                    static_cast<void *>(parameter_parent),
                    parameter_parent->class_name().c_str());
            ROSE_ABORT();
          }
          if (SgDeclarationStatement *parameter_owner =
                  isSgDeclarationStatement(parameter_parent)) {
            if (SgDeclarationScope *decl_scope =
                    SageBuilder::getOrCreateNonrealDeclarationScope(
                        parameter_owner)) {
              template_param_scope = decl_scope;
            }
          }
        }
      }

      // A parameter declaration is the exact semantic identity for this
      // qualifier type.  p_decl_translation_map can intentionally retain a
      // different source redeclaration surface from the same template family;
      // its spelling must not replace this declaration-owned name.
      name_str = normalizeTemplateTypeParamName(decl->getNameAsString());
      if (const clang::IdentifierInfo *identifier = ttp->getIdentifier()) {
        const std::string identifier_name =
            normalizeTemplateTypeParamName(readClangApiValueDefined([&]() {
                                             return identifier->getName();
                                           }).str());
        if (!name_str.empty() && !identifier_name.empty() &&
            name_str != identifier_name) {
          fprintf(stderr,
                  "REX_FRONTEND_INVARIANT[template-type-parameter-source]: "
                  "declaration=%p name='%s' and nested qualifier type "
                  "identifier='%s' disagree\n",
                  static_cast<const void *>(decl), name_str.c_str(),
                  identifier_name.c_str());
          ROSE_ABORT();
        }
        if (name_str.empty()) {
          name_str = identifier_name;
        }
      }

      if (isSgDeclarationScope(template_param_scope) == nullptr) {
        template_param_scope = get_template_param_scope(
            const_cast<clang::TemplateTypeParmDecl *>(decl));
        if (name_str.empty()) {
          auto translated = p_decl_translation_map.find(
              const_cast<clang::TemplateTypeParmDecl *>(decl));
          if (translated != p_decl_translation_map.end()) {
            if (SgTemplateParameter *sg_param =
                    isSgTemplateParameter(translated->second)) {
              if (SgTemplateType *existing_type =
                      isSgTemplateType(sg_param->get_type())) {
                name_str = existing_type->get_name().getString();
              }
            }
          }
        }
      }
    }

    if (ttp->getDecl() == nullptr) {
      // Depth/index coordinates are local to one template parameter list.
      // Consult the active source map only when Clang has dropped the
      // declaration identity; applying an ambient map to a declared parameter
      // confuses unrelated lists that reuse the same coordinates.
      const std::string exact_surface_name = normalizeTemplateTypeParamName(
          resolveExactTemplateParameterName(ttp->getDepth(), ttp->getIndex()));
      if (!exact_surface_name.empty()) {
        name_str = exact_surface_name;
      }
    }

    if (name_str.empty()) {
      name_str = resolve_from_sage_scope(ttp->getDepth(), ttp->getIndex());
    }
    std::string active_context_name =
        resolve_from_decl_context_stack(ttp->getDepth(), ttp->getIndex());
    if (const clang::TemplateTypeParmDecl *decl = ttp->getDecl()) {
      std::string context_name = resolveTemplateParameterNameFromDeclContext(
          decl->getDeclContext(), ttp->getDepth(), ttp->getIndex());
      name_str = preferHigherQualityTemplateParamName(name_str, context_name);
    }
    if (ttp->getDecl() == nullptr) {
      if (!active_context_name.empty()) {
        name_str =
            preferHigherQualityTemplateParamName(name_str, active_context_name);
      }
      std::string scope_name =
          resolve_from_sage_scope(ttp->getDepth(), ttp->getIndex());
      name_str = preferHigherQualityTemplateParamName(name_str, scope_name);
    }

    unsigned depth = 0;
    unsigned index = 0;
    if (parseTemplateParamDepthAndIndex(name_str, &depth, &index)) {
      std::string resolved_name;
      if (const clang::TemplateTypeParmDecl *decl = ttp->getDecl()) {
        resolved_name = resolveTemplateParameterNameFromDeclContext(
            decl->getDeclContext(), depth, index);
      }
      if (resolved_name.empty()) {
        resolved_name = resolve_from_decl_context_stack(depth, index);
      }
      if (resolved_name.empty()) {
        resolved_name = resolve_from_sage_scope(depth, index);
      }
      if (!resolved_name.empty()) {
        name_str = resolved_name;
      }
    }

    if (name_str.empty()) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[template-type-parameter]: depth=%u "
              "index=%u has no exact declared name\n",
              ttp->getDepth(), ttp->getIndex());
      ROSE_ABORT();
    }
    ROSE_ASSERT(!name_str.empty());

    SgNonrealType *nrtype = SageBuilder::buildSemanticNonrealType(
        SgName(name_str), template_param_scope, nullptr, nullptr);
    if (SgNonrealDecl *nrdecl =
            isSgNonrealDecl(nrtype ? nrtype->get_declaration() : nullptr)) {
      nrdecl->set_is_template_param(true);
      nrdecl->set_template_parameter_depth(static_cast<int>(ttp->getDepth()));
      nrdecl->set_template_parameter_position(
          static_cast<int>(ttp->getIndex()));
      nrdecl->set_suppress_typename(true);
    }
    return nrtype;
  }

  if (const clang::TypedefType *tdef =
          llvm::dyn_cast<clang::TypedefType>(clang_type)) {
    std::string name_str = tdef->getDecl()->getNameAsString();
    ROSE_ASSERT(!name_str.empty());
    clang::NestedNameSpecifier qualifier = std::nullopt;
    if (!prefer_current_scope && p_compiler_instance != nullptr &&
        canSynthesizeNamespaceQualifierFromDeclContext(
            tdef->getDecl()->getDeclContext())) {
      if (!scopeIsWithinNamespaceChain(scope,
                                       tdef->getDecl()->getDeclContext())) {
        qualifier = buildNamespaceQualifierForDeclContext(
            tdef->getDecl()->getDeclContext(),
            p_compiler_instance->getASTContext());
      }
    }
    return build_with_qualifier(qualifier, name_str, nullptr);
  }

  if (const clang::UsingType *using_type =
          llvm::dyn_cast<clang::UsingType>(clang_type)) {
    clang::UsingShadowDecl *using_decl = using_type->getDecl();
    std::string name_str = using_decl ? using_decl->getNameAsString() : "";
    if (name_str.empty()) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[using-type-name]: UsingType has no "
              "exact declaration name\n");
      ROSE_ABORT();
    }
    ROSE_ASSERT(!name_str.empty());
    return build_with_qualifier(std::nullopt, name_str, nullptr);
  }

  if (const clang::TagType *tag = llvm::dyn_cast<clang::TagType>(clang_type)) {
    clang::TagDecl *tag_decl = markClangSpecificDeclDefined(tag->getDecl());
    if (const clang::ClassTemplateSpecializationDecl *specialization =
            llvm::dyn_cast_or_null<clang::ClassTemplateSpecializationDecl>(
                tag_decl)) {
      specialization =
          llvm::dyn_cast_or_null<clang::ClassTemplateSpecializationDecl>(
              markClangDeclObjectDefinedByKind(specialization));
      ASSERT_not_null(specialization);
      const clang::ClassTemplateDecl *specialized_template =
          llvm::dyn_cast_or_null<clang::ClassTemplateDecl>(
              markClangDeclObjectDefinedByKind(
                  specialization->getSpecializedTemplate()));
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

      const clang::TemplateArgumentList *arguments =
          markClangTemplateArgumentListDefined(
              &specialization->getTemplateArgs());
      if (arguments == nullptr) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[nested-specialization-arguments]: "
                "class-template specialization=%p name=%s has no exact "
                "argument list\n",
                static_cast<const void *>(specialization), base_name.c_str());
        ROSE_ABORT();
      }
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
          *arguments, arguments->size(), reconstruct_source_surface);
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
          const bool needs_named_source_type =
              class_declaration != nullptr &&
              class_declaration->get_isUnNamed() &&
              sage_argument->get_sourceSpelledType() == nullptr;
          if (!needs_named_source_type) {
            continue;
          }

          if (declaration_arguments == nullptr ||
              index >= declaration_arguments->getNumTemplateArgs() ||
              index >= arguments->size()) {
            fprintf(stderr,
                    "REX_FRONTEND_INVARIANT[explicit-instantiation-type-"
                    "surface]: specialization=%s anonymous argument=%zu has "
                    "no exact declaration argument TypeLoc\n",
                    specialization->getQualifiedNameAsString().c_str(), index);
            ROSE_ABORT();
          }
          const clang::TemplateArgumentLoc &source_argument_loc =
              declaration_arguments->arguments()[index];
          const clang::TemplateArgument &source_argument =
              markClangTemplateArgumentDefined(
                  source_argument_loc.getArgument());
          const clang::TemplateArgument &semantic_argument =
              markClangTemplateArgumentDefined(arguments->get(index));
          if (source_argument.getKind() != clang::TemplateArgument::Type ||
              semantic_argument.getKind() != clang::TemplateArgument::Type ||
              p_compiler_instance == nullptr ||
              !p_compiler_instance->getASTContext().hasSameType(
                  markClangQualTypeDefined(source_argument.getAsType()),
                  markClangQualTypeDefined(semantic_argument.getAsType()))) {
            fprintf(stderr,
                    "REX_FRONTEND_INVARIANT[explicit-instantiation-type-"
                    "surface]: specialization=%s declaration and semantic "
                    "arguments disagree at index=%zu\n",
                    specialization->getQualifiedNameAsString().c_str(), index);
            ROSE_ABORT();
          }

          SgType *source_type =
              translateTypeTemplateArgument(source_argument_loc, scope);
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
          if (source_type == nullptr ||
              (source_class_declaration != nullptr &&
               source_class_declaration->get_isUnNamed()) ||
              !has_exact_named_source_identity) {
            fprintf(stderr,
                    "REX_FRONTEND_INVARIANT[explicit-instantiation-type-"
                    "surface]: specialization=%s argument=%zu declaration "
                    "TypeLoc did not produce one exact named Sage type\n",
                    specialization->getQualifiedNameAsString().c_str(), index);
            ROSE_ABORT();
          }
          sage_argument->set_sourceSpelledType(source_type);
        }
      }
      const std::string semantic_name = buildExactTemplateInstantiationName(
          base_name, arguments->asArray(),
          currentTemplateParameterDeclContext());
      if (semantic_name.find('<') == std::string::npos) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[nested-specialization-name]: "
                "class-template specialization=%p name=%s has incomplete "
                "semantic spelling '%s'\n",
                static_cast<const void *>(specialization), base_name.c_str(),
                semantic_name.c_str());
        ROSE_ABORT();
      }

      clang::NestedNameSpecifier qualifier = std::nullopt;
      clang::DeclContext *specialization_context =
          markClangDeclContextObjectDefined(const_cast<clang::DeclContext *>(
              specialization->getDeclContext()));
      if (!prefer_current_scope && p_compiler_instance != nullptr &&
          canSynthesizeNamespaceQualifierFromDeclContext(
              specialization_context) &&
          !scopeIsWithinNamespaceChain(scope, specialization_context)) {
        qualifier = buildNamespaceQualifierForDeclContext(
            specialization_context, p_compiler_instance->getASTContext());
      }
      return attach_named_decl_to_nonreal(
          build_with_qualifier(qualifier, semantic_name, &template_arguments),
          specialization);
    }

    std::string name_str = tag_decl->getNameAsString();
    if (name_str.empty()) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[tag-type-name]: TagType has no exact "
              "declared name\n");
      ROSE_ABORT();
    }
    ROSE_ASSERT(!name_str.empty());
    clang::NestedNameSpecifier qualifier = std::nullopt;
    if (!prefer_current_scope && p_compiler_instance != nullptr &&
        canSynthesizeNamespaceQualifierFromDeclContext(
            tag->getDecl()->getDeclContext())) {
      if (!scopeIsWithinNamespaceChain(scope,
                                       tag->getDecl()->getDeclContext())) {
        qualifier = buildNamespaceQualifierForDeclContext(
            tag->getDecl()->getDeclContext(),
            p_compiler_instance->getASTContext());
      }
    }
    SgNonrealType *nrtype = build_with_qualifier(qualifier, name_str, nullptr);
    if (SgNonrealDecl *nrdecl =
            isSgNonrealDecl(nrtype ? nrtype->get_declaration() : nullptr)) {
      if (tag_decl != nullptr) {
        SgDeclarationStatement *sg_decl =
            lookupSgDeclarationForClangDecl(tag_decl, /*allow_on_demand=*/true);
        if (translated_decl_resolves_template_target(sg_decl)) {
          nrdecl->set_templateDeclaration(
              normalizeNonrealTemplateDeclarationTarget(sg_decl));
        } else if (auto *spec_decl =
                       llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(
                           tag_decl)) {
          queuePendingImplicitClassTemplateSpecialization(spec_decl);
          linkNonrealTemplateDeclaration(nrdecl, spec_decl,
                                         "tag-template-specialization");
        }
      }
    }
    return nrtype;
  }

  if (const clang::InjectedClassNameType *inj =
          llvm::dyn_cast<clang::InjectedClassNameType>(clang_type)) {
    if (p_compiler_instance == nullptr) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[injected-class-name-type]: exact AST "
              "context is required for an injected current instantiation\n");
      ROSE_ABORT();
    }
    clang::QualType injected_qt = getInjectedClassNameSpecializationType(
        inj, p_compiler_instance->getASTContext());
    const clang::Type *injected_ty = injected_qt.getTypePtrOrNull();
    if (injected_ty == nullptr || injected_ty == clang_type) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[injected-class-name-type]: current "
              "instantiation has no distinct exact specialization\n");
      ROSE_ABORT();
    }
    SgNonrealType *nrtype = buildNonrealTypeForNestedNameSpecifierType(
        injected_ty, scope, prefer_current_scope, template_use_scope);
    if (nrtype == nullptr) {
      fprintf(stderr, "REX_FRONTEND_INVARIANT[injected-class-name-type]: exact "
                      "specialization did not translate to a nonreal type\n");
      ROSE_ABORT();
    }
    return nrtype;
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
  ROSE_ASSERT(!name_str.empty());
  return build_with_qualifier(std::nullopt, name_str, nullptr);
}

SgNonrealType *
ClangToSageTranslator::buildSemanticNonrealTypeFromNestedNameSpecifier(
    clang::NestedNameSpecifier qualifier, SgScopeStatement *scope,
    const SgName &terminalName,
    const SgTemplateArgumentPtrList *terminalTemplateArgs,
    const SgName *terminalSemanticName) {
  qualifier = markClangNestedNameSpecifierDefined(qualifier);
  SgScopeStatement *lexical_scope = scope;
  if (lexical_scope == nullptr) {
    lexical_scope = SageBuilder::topScopeStack();
  }
  ROSE_ASSERT(lexical_scope != nullptr);

  SgScopeStatement *qualifier_root_scope = lexical_scope;
  bool has_global_qualifier = nestedNameSpecifierHasGlobal(qualifier);
  if (has_global_qualifier ||
      nestedNameSpecifierHasNamespaceQualifier(qualifier)) {
    qualifier_root_scope = getGlobalScope();
  }
  ROSE_ASSERT(qualifier_root_scope != nullptr);

  std::function<SgScopeStatement *(clang::NestedNameSpecifier,
                                   SgScopeStatement *)>
      build_chain;
  build_chain = [&](clang::NestedNameSpecifier nns,
                    SgScopeStatement *current_scope) -> SgScopeStatement * {
    nns = markClangNestedNameSpecifierDefined(nns);
    if (!nns) {
      return current_scope;
    }

    current_scope = build_chain(nestedNameSpecifierPrefix(nns), current_scope);

    SgNonrealType *segment_type = nullptr;
    switch (nns.getKind()) {
    case clang::NestedNameSpecifier::Kind::Namespace: {
      const clang::NamespaceBaseDecl *ns =
          llvm::dyn_cast_or_null<clang::NamespaceBaseDecl>(
              markClangDeclObjectDefinedByKind(
                  nestedNameSpecifierNamespaceBase(nns)));
      std::string name_str = ns ? ns->getNameAsString() : "";
      ROSE_ASSERT(!name_str.empty());
      segment_type = SageBuilder::buildSemanticNonrealType(
          SgName(name_str), current_scope, nullptr, nullptr);
      break;
    }

    case clang::NestedNameSpecifier::Kind::Type: {
      bool prefer_current = static_cast<bool>(
          markClangNestedNameSpecifierDefined(nestedNameSpecifierPrefix(nns)));
      segment_type = buildNonrealTypeForNestedNameSpecifierType(
          markClangTypeObjectDefinedByClass(
              readClangApiValueDefined([&]() { return nns.getAsType(); })),
          current_scope, prefer_current, lexical_scope);
      break;
    }

    case clang::NestedNameSpecifier::Kind::Global:
      break;

    case clang::NestedNameSpecifier::Kind::MicrosoftSuper: {
      clang::CXXRecordDecl *record = const_cast<clang::CXXRecordDecl *>(
          llvm::dyn_cast_or_null<clang::CXXRecordDecl>(
              markClangDeclObjectDefinedByKind(readClangApiValueDefined(
                  [&]() { return nns.getAsRecordDecl(); }))));
      std::string name_str = record ? readClangApiValueDefined([&]() {
        return record->getNameAsString();
      })
                                    : "";
      if (name_str.empty()) {
        name_str = "__super";
      }
      segment_type = SageBuilder::buildSemanticNonrealType(
          SgName(name_str), current_scope, nullptr, nullptr);
      break;
    }
    case clang::NestedNameSpecifier::Kind::Null:
      break;
    }

    if (segment_type != nullptr) {
      SgNonrealDecl *segment_decl =
          isSgNonrealDecl(segment_type->get_declaration());
      ROSE_ASSERT(segment_decl != nullptr);
      if (nestedNameSpecifierHasTemplateKeyword(nns)) {
        segment_decl->set_has_template_keyword(true);
      }
      if (clang::NestedNameSpecifier prefix =
              markClangNestedNameSpecifierDefined(
                  nestedNameSpecifierPrefix(nns))) {
        if (prefix.getKind() == clang::NestedNameSpecifier::Kind::Global) {
          segment_decl->set_has_global_qualifier(true);
        }
      }
      current_scope = segment_decl->get_nonreal_decl_scope();
    }

    return current_scope;
  };

  SgScopeStatement *chain_scope = build_chain(qualifier, qualifier_root_scope);
  ROSE_ASSERT(chain_scope != nullptr);

  auto qualifier_requires_typename = [](clang::NestedNameSpecifier nns) {
    return nestedNameSpecifierHasDependentTypeQualifier(nns);
  };

  SgNonrealType *nrtype = nullptr;
  if (terminalTemplateArgs == nullptr) {
    nrtype = SageBuilder::buildSemanticNonrealType(terminalName, chain_scope,
                                                   nullptr, nullptr);
  } else {
    // The qualifier chain is a shared semantic identity, but a dependent
    // template-id is a use-site identity: its arguments can own references to
    // locals, parameters, or other lexical declarations.  Publish a simple
    // terminal under the qualifier chain and keep only a typed, non-owning
    // template edge from the lexical template-id to that shared base.
    SgNonrealType *qualified_base = SageBuilder::buildSemanticNonrealType(
        terminalName, chain_scope, nullptr, nullptr);
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

    auto is_within_scope = [](SgNode *node, SgScopeStatement *candidate) {
      if (node == nullptr || candidate == nullptr) {
        return false;
      }
      for (SgNode *current = node; current != nullptr;
           current = current->get_parent()) {
        if (current == candidate) {
          return true;
        }
      }
      return false;
    };
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
            is_within_scope(declaration_scope, local_dependency_owner)) {
          local_dependency_owner = declaration_scope;
        } else if (!is_within_scope(local_dependency_owner,
                                    declaration_scope)) {
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
    if (local_dependency_owner != nullptr &&
        !is_within_scope(lexical_scope, local_dependency_owner)) {
      lexical_scope = local_dependency_owner;
    }
    if (local_dependency_owner != nullptr &&
        !is_within_scope(lexical_scope, local_dependency_owner)) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[nonreal-template-use-owner]: "
              "terminal=%s lexical owner=%p/%s is outside exact local "
              "dependency owner=%p/%s\n",
              terminalName.getString().c_str(),
              static_cast<void *>(lexical_scope),
              lexical_scope->class_name().c_str(),
              static_cast<void *>(local_dependency_owner),
              local_dependency_owner->class_name().c_str());
      ROSE_ABORT();
    }

    nrtype = SageBuilder::buildSemanticNonrealType(terminalName, lexical_scope,
                                                   terminalTemplateArgs,
                                                   terminalSemanticName);
    SgNonrealDecl *use_declaration = isSgNonrealDecl(
        nrtype != nullptr ? nrtype->get_declaration() : nullptr);
    if (use_declaration == nullptr ||
        use_declaration == qualified_base_declaration ||
        use_declaration->get_scope() == nullptr ||
        use_declaration->get_parent() != use_declaration->get_scope()) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[nonreal-template-use-owner]: "
              "terminal=%s qualifier-base=%p and lexical use=%p do not form "
              "distinct exact identities\n",
              terminalName.getString().c_str(),
              static_cast<void *>(qualified_base_declaration),
              static_cast<void *>(use_declaration));
      ROSE_ABORT();
    }
    use_declaration->set_templateDeclaration(qualified_base_declaration);
  }
  ROSE_ASSERT(nrtype != nullptr);

  SgNonrealDecl *nrdecl = isSgNonrealDecl(nrtype->get_declaration());
  ROSE_ASSERT(nrdecl != nullptr);
  if (qualifier && !qualifier_requires_typename(qualifier)) {
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
  qualifier = markClangNestedNameSpecifierDefined(qualifier);
  if (!qualifier) {
    return scope;
  }

  std::vector<clang::NestedNameSpecifier> segments;
  for (clang::NestedNameSpecifier nns = qualifier; nns;
       nns = nestedNameSpecifierPrefix(nns)) {
    nns = markClangNestedNameSpecifierDefined(nns);
    segments.push_back(nns);
  }

  SgScopeStatement *current_scope = scope;
  if (nestedNameSpecifierHasNamespaceQualifier(qualifier)) {
    current_scope = getGlobalScope();
  }
  for (auto it = segments.rbegin(); it != segments.rend(); ++it) {
    clang::NestedNameSpecifier nns = markClangNestedNameSpecifierDefined(*it);
    if (nns.getKind() == clang::NestedNameSpecifier::Kind::Global) {
      current_scope = getGlobalScope();
      continue;
    }

    SgNonrealType *segment_type = nullptr;
    switch (nns.getKind()) {
    case clang::NestedNameSpecifier::Kind::Namespace: {
      const clang::NamespaceBaseDecl *ns =
          llvm::dyn_cast_or_null<clang::NamespaceBaseDecl>(
              markClangDeclObjectDefinedByKind(
                  nestedNameSpecifierNamespaceBase(nns)));
      std::string name_str = ns ? ns->getNameAsString() : "";
      ROSE_ASSERT(!name_str.empty());
      segment_type = SageBuilder::buildSemanticNonrealType(
          SgName(name_str), current_scope, nullptr, nullptr);
      break;
    }
    case clang::NestedNameSpecifier::Kind::Type: {
      bool prefer_current = static_cast<bool>(
          markClangNestedNameSpecifierDefined(nestedNameSpecifierPrefix(nns)));
      segment_type = buildNonrealTypeForNestedNameSpecifierType(
          markClangTypeObjectDefinedByClass(
              readClangApiValueDefined([&]() { return nns.getAsType(); })),
          current_scope, prefer_current, scope);
      break;
    }
    case clang::NestedNameSpecifier::Kind::MicrosoftSuper: {
      clang::CXXRecordDecl *record = const_cast<clang::CXXRecordDecl *>(
          llvm::dyn_cast_or_null<clang::CXXRecordDecl>(
              markClangDeclObjectDefinedByKind(readClangApiValueDefined(
                  [&]() { return nns.getAsRecordDecl(); }))));
      std::string name_str = record ? readClangApiValueDefined([&]() {
        return record->getNameAsString();
      })
                                    : "";
      if (name_str.empty()) {
        name_str = "__super";
      }
      segment_type = SageBuilder::buildSemanticNonrealType(
          SgName(name_str), current_scope, nullptr, nullptr);
      break;
    }
    case clang::NestedNameSpecifier::Kind::Global:
    case clang::NestedNameSpecifier::Kind::Null:
      break;
    }

    if (segment_type != nullptr) {
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
    }
  }

  return current_scope;
}

SgTemplateInstantiationDecl *
ClangToSageTranslator::getOrCreateTemplateInstantiation(
    SgTemplateClassDeclaration *template_decl,
    const clang::TemplateSpecializationType *clang_type) {
  auto canonical_namespace_scope = [](SgScopeStatement *scope) {
    SgNamespaceDefinitionStatement *namespace_definition =
        isSgNamespaceDefinitionStatement(scope);
    if (namespace_definition == nullptr) {
      return scope;
    }

    if (SgNamespaceDefinitionStatement *global_definition =
            namespace_definition->get_global_definition()) {
      return static_cast<SgScopeStatement *>(global_definition);
    }

    SgNamespaceDeclarationStatement *namespace_declaration =
        namespace_definition->get_namespaceDeclaration();
    SgNamespaceDeclarationStatement *first_nondefining =
        namespace_declaration != nullptr
            ? isSgNamespaceDeclarationStatement(
                  namespace_declaration->get_firstNondefiningDeclaration())
            : nullptr;
    if (first_nondefining != nullptr &&
        first_nondefining->get_definition() != nullptr) {
      return static_cast<SgScopeStatement *>(
          first_nondefining->get_definition());
    }
    return scope;
  };

  clang::TemplateDecl *inst_clang_template_decl = nullptr;
  clang::DeclContext *inst_decl_context = nullptr;
  const clang::ClassTemplateSpecializationDecl *inst_record_spec_decl = nullptr;
  clang::TemplateName clang_tname;
  if (clang_type != nullptr) {
    clang_type = static_cast<const clang::TemplateSpecializationType *>(
        markClangTypeObjectDefinedByClass(clang_type));
    clang_tname = markClangTemplateNameDefined(readClangApiValueDefined(
        [&]() { return clang_type->getTemplateName(); }));
    inst_clang_template_decl =
        markClangSpecificDeclDefined(readClangApiValueDefined(
            [&]() { return clang_tname.getAsTemplateDecl(); }));
    clang::QualType canonical_qt =
        markClangQualTypeDefined(readClangApiValueDefined([&]() {
          return clang::QualType(clang_type, 0).getCanonicalType();
        }));
    const clang::Type *canonical_type =
        markClangTypeObjectDefinedByClass(canonical_qt.getTypePtrOrNull());
    if (const auto *tag_type =
            llvm::dyn_cast_or_null<clang::TagType>(canonical_type)) {
      const clang::TagDecl *tag_decl = llvm::dyn_cast_or_null<clang::TagDecl>(
          markClangDeclObjectDefinedByKind(
              readClangApiValueDefined([&]() { return tag_type->getDecl(); })));
      const clang::CXXRecordDecl *record_decl =
          llvm::dyn_cast_or_null<clang::CXXRecordDecl>(tag_decl);
      record_decl = llvm::dyn_cast_or_null<clang::CXXRecordDecl>(
          markClangDeclObjectDefinedByKind(record_decl));
      inst_record_spec_decl =
          llvm::dyn_cast_or_null<clang::ClassTemplateSpecializationDecl>(
              record_decl);
      inst_record_spec_decl =
          llvm::dyn_cast_or_null<clang::ClassTemplateSpecializationDecl>(
              markClangDeclObjectDefinedByKind(inst_record_spec_decl));
    }
    if (inst_record_spec_decl != nullptr) {
      inst_decl_context = const_cast<clang::DeclContext *>(
          markClangDeclContextObjectDefined(readClangApiValueDefined(
              [&]() { return inst_record_spec_decl->getDeclContext(); })));
    } else if (inst_clang_template_decl != nullptr) {
      inst_decl_context =
          markClangDeclContextObjectDefined(readClangApiValueDefined(
              [&]() { return inst_clang_template_decl->getDeclContext(); }));
    }
  }

  auto class_kind_for_record =
      [](const clang::RecordDecl *decl) -> SgClassDeclaration::class_types {
    decl = llvm::dyn_cast_or_null<clang::RecordDecl>(
        markClangDeclObjectDefinedByKind(decl));
    if (decl == nullptr) {
      return SgClassDeclaration::e_class;
    }

    switch (decl->getTagKind()) {
    case clang::TagTypeKind::Struct:
      return SgClassDeclaration::e_struct;
    case clang::TagTypeKind::Union:
      return SgClassDeclaration::e_union;
    case clang::TagTypeKind::Class:
    default:
      return SgClassDeclaration::e_class;
    }
  };

  SgClassDeclaration::class_types inst_class_kind =
      template_decl != nullptr ? template_decl->get_class_type()
                               : SgClassDeclaration::e_class;
  if (inst_record_spec_decl != nullptr) {
    const clang::CXXRecordDecl *kind_decl =
        llvm::dyn_cast_or_null<clang::CXXRecordDecl>(
            markClangDeclObjectDefinedByKind(inst_record_spec_decl));
    inst_class_kind = class_kind_for_record(kind_decl);
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

  auto lookup_translated_source_template_decl =
      [&](clang::TemplateDecl *clang_template_decl)
      -> SgTemplateClassDeclaration * {
    auto lookup_decl = [&](clang::Decl *key) -> SgTemplateClassDeclaration * {
      return key != nullptr ? getTemplateDeclarationForSgDecl(
                                  lookupSgDeclarationForClangDecl(
                                      key, /*allow_on_demand=*/false))
                            : nullptr;
    };

    if (clang_template_decl == nullptr) {
      return nullptr;
    }

    if (SgTemplateClassDeclaration *decl =
            lookup_decl(llvm::cast<clang::Decl>(clang_template_decl))) {
      return decl;
    }

    if (clang::ClassTemplateDecl *class_template =
            llvm::dyn_cast<clang::ClassTemplateDecl>(clang_template_decl)) {
      if (SgTemplateClassDeclaration *decl =
              lookup_decl(class_template->getCanonicalDecl())) {
        return decl;
      }

      if (SgTemplateClassDeclaration *decl =
              lookupTranslatedTemplateDeclarationForRecord(
                  class_template->getTemplatedDecl())) {
        return decl;
      }
    }

    return nullptr;
  };

  SgTemplateClassDeclaration *translated_source_template_decl =
      lookup_translated_source_template_decl(inst_clang_template_decl);
  if (translated_source_template_decl != nullptr &&
      !same_template_decl_chain(template_decl,
                                translated_source_template_decl)) {
    template_decl = translated_source_template_decl;
  }
  const bool template_decl_is_translated_source_decl =
      translated_source_template_decl != nullptr &&
      same_template_decl_chain(template_decl, translated_source_template_decl);

  SgDeclarationStatement *exact_specialized_template_decl = template_decl;
  if (inst_record_spec_decl != nullptr) {
    auto selected_template = readClangApiValueDefined([&]() {
      return inst_record_spec_decl->getSpecializedTemplateOrPartial();
    });
    if (auto *partial =
            selected_template
                .dyn_cast<clang::ClassTemplatePartialSpecializationDecl *>()) {
      const auto *defined_partial =
          llvm::cast<clang::ClassTemplatePartialSpecializationDecl>(
              markClangDeclObjectDefinedByKind(partial));
      exact_specialized_template_decl = lookupSgDeclarationForClangDecl(
          const_cast<clang::ClassTemplatePartialSpecializationDecl *>(
              defined_partial),
          /*allow_on_demand=*/true);
    }
  }
  if (exact_specialized_template_decl == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[template-instantiation-specialized-"
                 "identity]: Clang specialization has no exact translated "
                 "primary or partial template declaration"
              << std::endl;
    ROSE_ABORT();
  }

  auto has_source_surface = [](SgLocatedNode *node) {
    if (node == nullptr) {
      return false;
    }
    Sg_File_Info *fi = node->get_file_info();
    return fi != nullptr && fi->get_line() > 0 && !fi->isCompilerGenerated() &&
           !fi->isSourcePositionUnavailableInFrontend();
  };

  auto validate_source_decl_scope = [&](SgDeclarationStatement *decl,
                                        SgScopeStatement *target_scope,
                                        const char *context) {
    SgScopeStatement *lexical_owner =
        decl != nullptr ? isSgScopeStatement(decl->get_parent()) : nullptr;
    SgScopeStatement *canonical_target =
        canonical_namespace_scope(target_scope);
    SgScopeStatement *canonical_lexical_owner =
        canonical_namespace_scope(lexical_owner);
    if (decl == nullptr || target_scope == nullptr ||
        decl->get_scope() != target_scope || lexical_owner == nullptr ||
        canonical_target == nullptr ||
        canonical_lexical_owner != canonical_target ||
        countLexicalDeclarationEdges(lexical_owner, decl) != 1) {
      std::cerr << "REX_FRONTEND_INVARIANT[source-declaration-owner]: "
                << context << " declaration=" << decl << " semantic-scope="
                << (decl != nullptr ? decl->get_scope() : nullptr)
                << " expected-semantic-scope=" << target_scope
                << " lexical-owner=" << lexical_owner
                << " canonical-lexical-owner=" << canonical_lexical_owner
                << " canonical-semantic-scope=" << canonical_target
                << " lexical-edges="
                << (decl != nullptr && lexical_owner != nullptr
                        ? countLexicalDeclarationEdges(lexical_owner, decl)
                        : 0)
                << " lacks one exact semantic namespace identity and physical "
                   "source owner"
                << std::endl;
      ROSE_ABORT();
    }
  };
  auto normalize_template_decl_scope = [&](SgTemplateClassDeclaration *decl,
                                           SgScopeStatement *target_scope,
                                           const char *context) {
    if (decl == nullptr || target_scope == nullptr) {
      return;
    }
    auto normalize_one = [&](SgTemplateClassDeclaration *candidate) {
      if (candidate == nullptr) {
        return;
      }
      if (template_decl_is_translated_source_decl &&
          has_source_surface(candidate)) {
        validate_source_decl_scope(candidate, target_scope, context);
      } else {
        publishSemanticAuxiliaryDeclaration(candidate, target_scope, context);
      }
    };

    normalize_one(decl);
    if (SgTemplateClassDeclaration *first = isSgTemplateClassDeclaration(
            decl->get_firstNondefiningDeclaration())) {
      if (first != decl) {
        normalize_one(first);
      }
    }
    if (SgTemplateClassDeclaration *def =
            isSgTemplateClassDeclaration(decl->get_definingDeclaration())) {
      if (def != decl) {
        normalize_one(def);
      }
    }
    registerDeclarationSymbol(decl);
  };

  auto normalize_instantiation_scope = [&](SgTemplateInstantiationDecl *decl,
                                           SgScopeStatement *target_scope,
                                           const char *context) {
    if (decl == nullptr || target_scope == nullptr) {
      return;
    }
    auto normalize_one = [&](SgTemplateInstantiationDecl *candidate) {
      if (candidate == nullptr) {
        return;
      }
      if (has_source_surface(candidate)) {
        validate_source_decl_scope(candidate, target_scope, context);
      } else {
        publishSemanticAuxiliaryDeclaration(candidate, target_scope, context);
      }
    };
    normalize_one(decl);
    if (SgTemplateInstantiationDecl *first = isSgTemplateInstantiationDecl(
            decl->get_firstNondefiningDeclaration())) {
      if (first != decl) {
        normalize_one(first);
      }
    }
    if (SgTemplateInstantiationDecl *def =
            isSgTemplateInstantiationDecl(decl->get_definingDeclaration())) {
      if (def != decl) {
        normalize_one(def);
      }
    }
    registerDeclarationSymbol(decl);
  };

  auto translated_record_scope =
      [&](SgDeclarationStatement *decl) -> SgScopeStatement * {
    if (SgTemplateInstantiationDecl *inst_decl =
            isSgTemplateInstantiationDecl(decl)) {
      if (SgClassDefinition *def = inst_decl->get_definition()) {
        return def;
      }
      if (SgTemplateInstantiationDecl *defining_decl =
              isSgTemplateInstantiationDecl(
                  inst_decl->get_definingDeclaration())) {
        if (SgClassDefinition *def = defining_decl->get_definition()) {
          return def;
        }
      }
      if (SgDeclarationScope *decl_scope =
              SageBuilder::getOrCreateNonrealDeclarationScope(inst_decl)) {
        return decl_scope;
      }
      return inst_decl->get_scope();
    }
    if (SgTemplateClassDeclaration *tmpl_decl =
            isSgTemplateClassDeclaration(decl)) {
      if (SgClassDefinition *def = tmpl_decl->get_definition()) {
        return def;
      }
      if (SgTemplateClassDeclaration *defining_decl =
              isSgTemplateClassDeclaration(
                  tmpl_decl->get_definingDeclaration())) {
        if (SgClassDefinition *def = defining_decl->get_definition()) {
          return def;
        }
      }
      return tmpl_decl->get_scope();
    }
    if (SgClassDeclaration *class_decl = isSgClassDeclaration(decl)) {
      if (SgClassDefinition *def = class_decl->get_definition()) {
        return def;
      }
      if (SgClassDeclaration *defining_decl =
              isSgClassDeclaration(class_decl->get_definingDeclaration())) {
        if (SgClassDefinition *def = defining_decl->get_definition()) {
          return def;
        }
      }
      return class_decl->get_scope();
    }
    return nullptr;
  };

  std::function<SgScopeStatement *(const clang::DeclContext *)>
      resolve_nested_specialization_scope =
          [&](const clang::DeclContext *context) -> SgScopeStatement * {
    context = markClangDeclContextObjectDefined(context);
    while (context != nullptr) {
      const clang::Decl *context_decl =
          clangDeclFromDeclContextDefined(context);
      if (!llvm::isa_and_nonnull<clang::LinkageSpecDecl>(context_decl)) {
        break;
      }
      context = markClangDeclContextObjectDefined(
          readClangApiValueDefined([&]() { return context->getParent(); }));
    }
    if (context == nullptr) {
      return nullptr;
    }
    if (readClangApiValueDefined([&]() { return context->isNamespace(); }) ||
        readClangApiValueDefined(
            [&]() { return context->isTranslationUnit(); })) {
      return canonical_namespace_scope(resolveScopeFromDeclContext(
          const_cast<clang::DeclContext *>(context)));
    }

    const clang::CXXRecordDecl *record =
        llvm::dyn_cast_or_null<clang::CXXRecordDecl>(
            clangDeclFromDeclContextDefined(context));
    if (record == nullptr) {
      return resolveScopeFromDeclContext(
          const_cast<clang::DeclContext *>(context));
    }

    SgScopeStatement *parent_scope = resolve_nested_specialization_scope(
        readClangApiValueDefined([&]() { return record->getDeclContext(); }));
    SgDeclarationStatement *translated_decl = lookupSgDeclarationForClangDecl(
        const_cast<clang::CXXRecordDecl *>(record),
        /*allow_on_demand=*/true);
    if (translated_decl == nullptr) {
      translated_decl = lookupSgDeclarationForClangDecl(
          const_cast<clang::CXXRecordDecl *>(
              markClangSpecificDeclDefined(readClangApiValueDefined(
                  [&]() { return record->getCanonicalDecl(); }))),
          /*allow_on_demand=*/true);
    }

    if (SgTemplateInstantiationDecl *inst_decl =
            isSgTemplateInstantiationDecl(translated_decl)) {
      if (parent_scope != nullptr && inst_decl->get_scope() != parent_scope) {
        normalize_instantiation_scope(
            inst_decl, parent_scope,
            "getOrCreateTemplateInstantiation:nested-context");
      }
      if (SgScopeStatement *inst_scope = translated_record_scope(inst_decl)) {
        return inst_scope;
      }
    } else if (SgTemplateClassDeclaration *tmpl_decl =
                   isSgTemplateClassDeclaration(translated_decl)) {
      if (parent_scope != nullptr && tmpl_decl->get_scope() != parent_scope) {
        auto has_real_source = [](SgTemplateClassDeclaration *candidate) {
          if (candidate == nullptr) {
            return false;
          }
          Sg_File_Info *fi = candidate->get_file_info();
          return fi != nullptr && fi->get_line() > 0 &&
                 !fi->isCompilerGenerated() &&
                 !fi->isSourcePositionUnavailableInFrontend();
        };
        const bool source_owned_pattern =
            has_real_source(tmpl_decl) ||
            has_real_source(isSgTemplateClassDeclaration(
                tmpl_decl->get_firstNondefiningDeclaration())) ||
            has_real_source(isSgTemplateClassDeclaration(
                tmpl_decl->get_definingDeclaration()));
        if (source_owned_pattern) {
          // Clang aliases an inherited member-template record to its exact
          // source pattern.  The pattern remains structurally owned by the
          // primary class definition; the instantiated parent is the semantic
          // context in which this nested specialization is being created.
          return parent_scope;
        }
        normalize_template_decl_scope(
            tmpl_decl, parent_scope,
            "getOrCreateTemplateInstantiation:nested-template-context");
      }
      if (SgScopeStatement *tmpl_scope = translated_record_scope(tmpl_decl)) {
        return tmpl_scope;
      }
    } else if (translated_decl != nullptr) {
      if (SgScopeStatement *record_scope =
              translated_record_scope(translated_decl)) {
        return record_scope;
      }
    }

    return resolveScopeFromDeclContext(
        const_cast<clang::DeclContext *>(context));
  };

  auto is_explicit_specialization_chain =
      [](SgTemplateInstantiationDecl *decl) -> bool {
    auto is_explicit_specialization =
        [](SgTemplateInstantiationDecl *candidate) -> bool {
      return candidate != nullptr &&
             candidate->get_specialization() ==
                 SgDeclarationStatement::e_specialization;
    };

    if (is_explicit_specialization(decl)) {
      return true;
    }
    if (SgTemplateInstantiationDecl *first = isSgTemplateInstantiationDecl(
            decl != nullptr ? decl->get_firstNondefiningDeclaration()
                            : nullptr)) {
      if (is_explicit_specialization(first)) {
        return true;
      }
    }
    if (SgTemplateInstantiationDecl *def = isSgTemplateInstantiationDecl(
            decl != nullptr ? decl->get_definingDeclaration() : nullptr)) {
      if (is_explicit_specialization(def)) {
        return true;
      }
    }
    return false;
  };

  auto namespace_scope_matches_inst_context =
      [&](SgScopeStatement *scope) -> bool {
    if (scope == nullptr) {
      return false;
    }
    if (isSgGlobal(scope) == nullptr &&
        isSgNamespaceDefinitionStatement(scope) == nullptr) {
      return false;
    }
    if (inst_decl_context == nullptr) {
      return true;
    }

    clang::DeclContext *context = inst_decl_context;
    while (context != nullptr) {
      context = markClangDeclContextObjectDefined(context);
      const clang::Decl *context_decl =
          clangDeclFromDeclContextDefined(context);
      if (!llvm::isa_and_nonnull<clang::LinkageSpecDecl>(context_decl)) {
        break;
      }
      context = markClangDeclContextObjectDefined(
          readClangApiValueDefined([&]() { return context->getParent(); }));
    }
    if (context == nullptr) {
      return false;
    }
    if (readClangApiValueDefined(
            [&]() { return context->isTranslationUnit(); })) {
      return isSgGlobal(scope) != nullptr;
    }
    return scopeIsWithinNamespaceChain(scope, context);
  };

  auto pick_reachable_existing_template_scope =
      [&](SgTemplateClassDeclaration *decl) -> SgScopeStatement * {
    auto current_namespace_or_global_scope = [&]() -> SgScopeStatement * {
      for (SgNode *node = SageBuilder::topScopeStack(); node != nullptr;
           node = node->get_parent()) {
        if (SgNamespaceDefinitionStatement *ns =
                isSgNamespaceDefinitionStatement(node)) {
          return ns;
        }
        if (SgGlobal *global = isSgGlobal(node)) {
          return global;
        }
      }
      return nullptr;
    };

    auto try_parent_scope =
        [&](SgScopeStatement *candidate) -> SgScopeStatement * {
      if (candidate == nullptr ||
          (isSgGlobal(candidate) == nullptr &&
           isSgNamespaceDefinitionStatement(candidate) == nullptr) ||
          !scopeReachableFromCurrentFile(candidate)) {
        return nullptr;
      }
      return candidate;
    };

    auto try_scope = [&](SgScopeStatement *candidate) -> SgScopeStatement * {
      if (candidate == nullptr || !scopeReachableFromCurrentFile(candidate) ||
          !namespace_scope_matches_inst_context(candidate)) {
        return nullptr;
      }
      return candidate;
    };

    auto try_decl =
        [&](SgTemplateClassDeclaration *candidate) -> SgScopeStatement * {
      if (candidate == nullptr) {
        return nullptr;
      }
      if (SgScopeStatement *scope = current_namespace_or_global_scope()) {
        if (namespace_scope_matches_inst_context(scope)) {
          return scope;
        }
      }
      if (SgScopeStatement *scope =
              try_parent_scope(isSgScopeStatement(candidate->get_parent()))) {
        return scope;
      }
      if (SgScopeStatement *scope =
              try_scope(isSgScopeStatement(candidate->get_parent()))) {
        return scope;
      }
      return try_scope(candidate->get_scope());
    };

    if (SgScopeStatement *scope = try_decl(decl)) {
      return scope;
    }
    if (SgTemplateClassDeclaration *first = isSgTemplateClassDeclaration(
            decl != nullptr ? decl->get_firstNondefiningDeclaration()
                            : nullptr)) {
      if (first != decl) {
        if (SgScopeStatement *scope = try_decl(first)) {
          return scope;
        }
      }
    }
    if (SgTemplateClassDeclaration *def = isSgTemplateClassDeclaration(
            decl != nullptr ? decl->get_definingDeclaration() : nullptr)) {
      if (def != decl) {
        if (SgScopeStatement *scope = try_decl(def)) {
          return scope;
        }
      }
    }
    return nullptr;
  };

  if (inst_decl_context != nullptr &&
      declContextCanUseReachableNamespaceScope(inst_decl_context) &&
      !scopeReachableFromCurrentFile(template_decl->get_scope()) &&
      !template_decl_is_translated_source_decl) {
    SgScopeStatement *reachable_scope =
        pick_reachable_existing_template_scope(template_decl);
    if (reachable_scope == nullptr) {
      reachable_scope = resolveReachableNamespaceScope(inst_decl_context);
    }
    if (reachable_scope != nullptr) {
      normalize_template_decl_scope(
          template_decl, reachable_scope,
          "getOrCreateTemplateInstantiation:template-decl");
    }
  }
  // Extract both base name and qualified name for the template.
  std::string template_base_name = template_decl->get_name().getString();
  std::string template_qualified_name = getTemplateQualifiedName(template_decl);
  const clang::TemplateArgumentList *semantic_specialization_args =
      inst_record_spec_decl != nullptr
          ? markClangTemplateArgumentListDefined(
                &inst_record_spec_decl->getTemplateArgs())
          : nullptr;
  const clang::TemplateArgumentList *semantic_deduced_args =
      inst_record_spec_decl != nullptr
          ? markClangTemplateArgumentListDefined(
                &inst_record_spec_decl->getTemplateInstantiationArgs())
          : nullptr;

  // A TemplateSpecializationType retains only the arguments written at its use
  // site, while the canonical ClassTemplateSpecializationDecl owns the full
  // semantic list, including defaults.  Declaration identity and class-type
  // interning must use that full list; otherwise `vector<int>` and
  // `vector<int, allocator<int>>` create competing canonical declarations for
  // one Clang specialization.
  ClangTemplateInstantiationCacheKey instantiation_cache_key =
      semantic_specialization_args != nullptr
          ? buildTemplateInstantiationCacheKey(template_qualified_name,
                                               *semantic_specialization_args,
                                               inst_decl_context)
          : buildTemplateInstantiationCacheKey(template_qualified_name,
                                               clang_type, inst_decl_context);
  std::string inst_display_name = buildExactTemplateInstantiationName(
      template_base_name,
      semantic_specialization_args != nullptr
          ? markClangTemplateArgumentArrayDefined(
                semantic_specialization_args->asArray())
          : markClangTemplateArgumentArrayDefined(
                clang_type->template_arguments()),
      inst_record_spec_decl != nullptr
          ? static_cast<const clang::DeclContext *>(inst_record_spec_decl)
          : currentTemplateParameterDeclContext());

  auto resolve_exact_instantiation_scope = [&]() -> SgScopeStatement * {
    // Use the template declaration scope as the default semantic owner. A
    // nested specialization instead belongs to its exact instantiated record
    // context; a qualified dependent template belongs to the scope described
    // by that qualifier.
    SgScopeStatement *resolved_scope = template_decl->get_scope();
    if (clang_type != nullptr) {
      if (inst_record_spec_decl != nullptr) {
        if (SgScopeStatement *context_scope =
                resolve_nested_specialization_scope(
                    readClangApiValueDefined([&]() {
                      return inst_record_spec_decl->getDeclContext();
                    }))) {
          resolved_scope = context_scope;
        }
      } else if (inst_clang_template_decl != nullptr) {
        if (SgScopeStatement *context_scope =
                resolveScopeFromDeclContext(markClangDeclContextObjectDefined(
                    readClangApiValueDefined([&]() {
                      return inst_clang_template_decl->getDeclContext();
                    })))) {
          resolved_scope = context_scope;
        }
      } else {
        clang::NestedNameSpecifier qualifier = std::nullopt;
        if (const clang::QualifiedTemplateName *qtn = readClangApiValueDefined(
                [&]() { return clang_tname.getAsQualifiedTemplateName(); })) {
          markClangAstObjectDefined(qtn);
          qualifier = markClangNestedNameSpecifierDefined(
              readClangApiValueDefined([&]() { return qtn->getQualifier(); }));
        } else if (const clang::DependentTemplateName *dtn =
                       readClangApiValueDefined([&]() {
                         return clang_tname.getAsDependentTemplateName();
                       })) {
          markClangAstObjectDefined(dtn);
          qualifier = markClangNestedNameSpecifierDefined(
              readClangApiValueDefined([&]() { return dtn->getQualifier(); }));
        }
        if (qualifier) {
          SgScopeStatement *base_scope = SageBuilder::topScopeStack();
          if (nestedNameSpecifierHasGlobal(qualifier)) {
            base_scope = getGlobalScope();
          }
          if (SgScopeStatement *context_scope =
                  buildNonrealScopeFromNestedNameSpecifier(qualifier,
                                                           base_scope)) {
            resolved_scope = context_scope;
          }
        }
      }
    }
    if (!scopeReachableFromCurrentFile(resolved_scope)) {
      SgScopeStatement *reachable_scope = nullptr;
      if (inst_decl_context != nullptr &&
          declContextCanUseReachableNamespaceScope(inst_decl_context)) {
        reachable_scope = resolveReachableNamespaceScope(inst_decl_context);
      }
      if (reachable_scope == nullptr) {
        reachable_scope = getGlobalScope();
      }
      if (reachable_scope != nullptr) {
        resolved_scope = reachable_scope;
      }
    }
    if (resolved_scope == nullptr) {
      resolved_scope = getGlobalScope();
    }
    return canonical_namespace_scope(resolved_scope);
  };
  SgScopeStatement *inst_scope = resolve_exact_instantiation_scope();
  if (inst_scope == nullptr) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[template-instantiation-scope]: "
            "template=%s specialization=%s has no exact semantic scope\n",
            template_qualified_name.c_str(), inst_display_name.c_str());
    ROSE_ABORT();
  }

  // A declaration-first traversal or a re-entrant record translation can
  // publish the canonical specialization before any type spelling reaches
  // this cache.  Clang's exact record identity is primary; the
  // string/argument cache is only an index.  The ordinary declaration map is
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
    if (SgTemplateInstantiationDirectiveStatement *directive =
            isSgTemplateInstantiationDirectiveStatement(mapped_declaration)) {
      mapped_declaration = directive->get_declaration();
    }
    if (mapped_declaration == nullptr) {
      mapped_declaration = lookupRecordTypePlaceholderDecl(
          const_cast<clang::ClassTemplateSpecializationDecl *>(
              inst_record_spec_decl));
    }
    if (mapped_declaration == nullptr) {
      return nullptr;
    }

    SgTemplateInstantiationDecl *mapped =
        isSgTemplateInstantiationDecl(mapped_declaration);
    if (mapped == nullptr) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[template-instantiation-identity]: "
              "Clang specialization=%p name=%s maps to declaration=%p/%s "
              "outside the template-instantiation family\n",
              static_cast<const void *>(inst_record_spec_decl),
              inst_display_name.c_str(),
              static_cast<void *>(mapped_declaration),
              mapped_declaration->class_name().c_str());
      ROSE_ABORT();
    }
    if (SgTemplateInstantiationDecl *first = isSgTemplateInstantiationDecl(
            mapped->get_firstNondefiningDeclaration())) {
      mapped = first;
    }
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

  // Check cache
  auto it = p_template_inst_cache.find(instantiation_cache_key);
  if (it != p_template_inst_cache.end()) {
    SgTemplateInstantiationDecl *inst_decl = it->second;
    if (inst_decl != nullptr) {
      if (SgTemplateInstantiationDecl *first = isSgTemplateInstantiationDecl(
              inst_decl->get_firstNondefiningDeclaration())) {
        inst_decl = first;
      }
    }
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
    if (inst_decl != nullptr) {
      const bool explicit_specialization_chain =
          is_explicit_specialization_chain(inst_decl);
      if (!explicit_specialization_chain &&
          inst_decl->get_class_type() != inst_class_kind) {
        inst_decl->set_class_type(inst_class_kind);
      }
      // Preserve any source-faithful argument spelling already attached via
      // TypeLoc-based reconstruction; canonical template arguments can lose
      // qualification such as `&foo::member`.
      if (inst_decl->get_templateArguments().empty()) {
        inst_decl->get_templateArguments() =
            buildTemplateArguments(clang_type, false);
      }
      if (inst_decl->get_semanticTemplateArguments().empty() &&
          semantic_specialization_args != nullptr &&
          semantic_specialization_args->size() != 0) {
        SemanticExpressionConstruction semantic_expressions(
            p_semantic_template_argument_expression_depth,
            "getOrCreateTemplateInstantiation:cached-semantic-identity-"
            "arguments");
        inst_decl->get_semanticTemplateArguments() =
            buildTemplateArguments(*semantic_specialization_args, 0);
      }
      if (inst_decl->get_deducedTemplateArguments().empty()) {
        SemanticExpressionConstruction semantic_expressions(
            p_semantic_template_argument_expression_depth,
            "getOrCreateTemplateInstantiation:cached-deduced-arguments");
        inst_decl->get_deducedTemplateArguments() =
            semantic_deduced_args != nullptr
                ? buildTemplateArguments(*semantic_deduced_args, 0)
                : buildTemplateArguments(clang_type, false);
      }
      SgDeclarationStatement *cached_specialized =
          inst_decl->get_specializedTemplateDeclaration();
      const bool owns_exact_specialized_template_family = [&]() {
        if (cached_specialized == exact_specialized_template_decl) {
          return true;
        }
        SgTemplateClassDeclaration *cached_template =
            isSgTemplateClassDeclaration(cached_specialized);
        SgTemplateClassDeclaration *exact_template =
            isSgTemplateClassDeclaration(exact_specialized_template_decl);
        return cached_template != nullptr && exact_template != nullptr &&
               same_template_decl_chain(cached_template, exact_template);
      }();
      if (!owns_exact_specialized_template_family) {
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
                  << " does not own Clang's exact primary or partial template "
                     "declaration family"
                  << std::endl;
        ROSE_ABORT();
      }
      SageBuilder::setTemplateArgumentParents(inst_decl);
      if (inst_record_spec_decl != nullptr) {
        if (SgScopeStatement *nested_scope =
                resolve_nested_specialization_scope(
                    readClangApiValueDefined([&]() {
                      return inst_record_spec_decl->getDeclContext();
                    }))) {
          nested_scope = canonical_namespace_scope(nested_scope);
          normalize_instantiation_scope(
              inst_decl, nested_scope,
              "getOrCreateTemplateInstantiation:nested-cache");
        }
      }
      if (!explicit_specialization_chain &&
          !scopeReachableFromCurrentFile(inst_decl->get_scope())) {
        SgScopeStatement *reachable_scope = nullptr;
        if (inst_decl_context != nullptr &&
            declContextCanUseReachableNamespaceScope(inst_decl_context)) {
          reachable_scope = resolveReachableNamespaceScope(inst_decl_context);
        }
        if (reachable_scope == nullptr) {
          reachable_scope = getGlobalScope();
        }
        if (reachable_scope != nullptr) {
          normalize_instantiation_scope(
              inst_decl, reachable_scope,
              "getOrCreateTemplateInstantiation:reachable-cache");
        }
      }
    }
    if (inst_decl == nullptr || inst_decl->get_scope() == nullptr) {
      std::cerr << "REX_FRONTEND_INVARIANT[template-instantiation-owner]: "
                   "cached type instantiation has no exact declaration or "
                   "scope"
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
    SgScopeStatement *canonical_scope =
        canonical_namespace_scope(inst_decl->get_scope());
    if (canonical_scope != inst_decl->get_scope()) {
      normalize_instantiation_scope(
          inst_decl, canonical_scope,
          "getOrCreateTemplateInstantiation:cached-namespace-owner");
    }
    normalize_instantiation_scope(
        inst_decl, inst_decl->get_scope(),
        "getOrCreateTemplateInstantiation:cached-owner");
    return inst_decl;
  }

  if (record_identity_instantiation != nullptr) {
    p_template_inst_cache.emplace(instantiation_cache_key,
                                  record_identity_instantiation);
    return record_identity_instantiation;
  }

  // Build template arguments
  SgTemplateArgumentPtrList args = buildTemplateArguments(clang_type, false);
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
             markClangTemplateArgumentArrayDefined(
                 clang_type->template_arguments())) {
          appendTemplateArguments(result, argument, false);
        }
        ensureTemplateArgumentParents(result);
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
      for (SgNode *node : RoseAst(argument)) {
        if (node != nullptr) {
          transient_nodes.insert(node);
        }
      }
    };
    for (SgTemplateArgument *argument : args) {
      collect_transient_nodes(argument);
    }
    for (SgTemplateArgument *argument : semantic_args) {
      collect_transient_nodes(argument);
    }
    for (SgTemplateArgument *argument : deduced_args) {
      collect_transient_nodes(argument);
    }

    for (SgNode *node : transient_nodes) {
      p_lexical_source_nodes.erase(node);
      p_synthesized_source_nodes.erase(node);
    }
    for (auto translated = p_stmt_translation_map.begin();
         translated != p_stmt_translation_map.end();) {
      if (transient_nodes.count(translated->second) != 0) {
        translated = p_stmt_translation_map.erase(translated);
      } else {
        ++translated;
      }
    }

    std::unordered_set<SgTemplateArgument *> deleted_roots;
    auto delete_transient_argument = [&](SgTemplateArgument *argument) {
      if (deleted_roots.insert(argument).second) {
        SageInterface::deleteAST(
            argument, SageInterface::DeleteAstMode::kRequireIsolated);
      }
    };
    for (SgTemplateArgument *argument : args) {
      delete_transient_argument(argument);
    }
    for (SgTemplateArgument *argument : semantic_args) {
      delete_transient_argument(argument);
    }
    for (SgTemplateArgument *argument : deduced_args) {
      delete_transient_argument(argument);
    }

    // Re-enter the ordinary cache path so all canonical declaration, scope,
    // type, and template-family validations remain centralized there.
    return getOrCreateTemplateInstantiation(template_decl, clang_type);
  }

  // Create class type first (will be set on instantiation)
  SgClassType *class_type = nullptr;

  // Create template instantiation declaration with all parameters
  // ROOT CAUSE FIX: Use qualified name (e.g., "std::array") not just base name
  // ("array") This ensures the unparser outputs the correct namespace
  // qualification
  SgTemplateInstantiationDecl *inst_decl = new SgTemplateInstantiationDecl(
      SgName(inst_display_name), inst_class_kind,
      class_type, // type (initially nullptr, will be set)
      nullptr,    // definition
      template_decl, args, semantic_args);
  inst_decl->get_deducedTemplateArguments() = deduced_args;
  inst_decl->set_specializedTemplateDeclaration(
      exact_specialized_template_decl);

  inst_decl->get_templateArguments() = args;

  setSynthesizedFileInfo(inst_decl);
  requireExactSynthesizedProvenance(
      inst_decl, "getOrCreateTemplateInstantiation:new-provenance");
  inst_decl->setForward();
  inst_decl->set_definingDeclaration(nullptr);
  inst_decl->set_firstNondefiningDeclaration(inst_decl);
  SageBuilder::setTemplateArgumentParents(inst_decl);

  if (inst_decl->get_templateDeclaration() != template_decl) {
    std::cerr << "REX_FRONTEND_INVARIANT[template-instantiation-constructor-"
                 "identity]: "
                 "SgTemplateInstantiationDecl constructor did not preserve its "
                 "exact primary-template declaration"
              << std::endl;
    ROSE_ABORT();
  }

  inst_decl->set_scope(inst_scope);

  // CRITICAL: Set template name before creating type
  // get_mangled_name() requires this to be set and will assert if it's null
  // Use ONLY base name for templateName (e.g., "array" not "std::array")
  // The qualified name is in the declaration name above
  // NameQualificationTraversal will add the necessary qualification (e.g.
  // "std::")
  inst_decl->set_templateName(SgName(template_base_name));

  // Create class type pointing to this instantiation
  class_type = SgClassType::createType(inst_decl);
  inst_decl->set_type(class_type);

  // Insert symbol via the unified registration path to avoid duplicates.
  registerDeclarationSymbol(inst_decl);
  inst_scope = inst_decl->get_scope();
  if (inst_scope == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[template-instantiation-scope]: "
                 "symbol registration removed the instantiation scope"
              << std::endl;
    ROSE_ABORT();
  }
  // Cache it with full name
  p_template_inst_cache[instantiation_cache_key] = inst_decl;
  publishSemanticAuxiliaryDeclaration(
      inst_decl, inst_scope,
      "getOrCreateTemplateInstantiation:new-semantic-owner");

  return inst_decl;
}

bool ClangToSageTranslator::VisitTemplateSpecializationType(
    clang::TemplateSpecializationType *template_specialization_type,
    SgNode **node) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToSageTranslator::TemplateSpecializationType" << std::endl;
#endif

  template_specialization_type =
      const_cast<clang::TemplateSpecializationType *>(
          llvm::cast_or_null<clang::TemplateSpecializationType>(
              markClangTypeObjectDefinedByClass(template_specialization_type)));
  if (template_specialization_type == nullptr) {
    return VisitType(template_specialization_type, node);
  }

  if (p_explicit_template_id_type_use_depth != 0 &&
      !template_specialization_type->isTypeAlias()) {
    SgScopeStatement *scope = SageBuilder::topScopeStack();
    if (scope == nullptr) {
      scope = getGlobalScope();
    }
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

  if (template_specialization_type->isTypeAlias()) {
    clang::TemplateName tname = markClangTemplateNameDefined(
        template_specialization_type->getTemplateName());
    auto resolve_template_decl =
        [&](const clang::TemplateName &name) -> clang::TemplateDecl * {
      clang::TemplateName current = markClangTemplateNameDefined(name);
      for (;;) {
        if (clang::TemplateDecl *decl = current.getAsTemplateDecl()) {
          return const_cast<clang::TemplateDecl *>(
              llvm::dyn_cast_or_null<clang::TemplateDecl>(
                  markClangDeclObjectDefinedByKind(decl)));
        }
        if (const clang::QualifiedTemplateName *qtn =
                current.getAsQualifiedTemplateName()) {
          clang::TemplateName underlying =
              markClangTemplateNameDefined(qtn->getUnderlyingTemplate());
          if (clang::TemplateDecl *decl = underlying.getAsTemplateDecl()) {
            return const_cast<clang::TemplateDecl *>(
                llvm::dyn_cast_or_null<clang::TemplateDecl>(
                    markClangDeclObjectDefinedByKind(decl)));
          }
          current = underlying;
          continue;
        }
        if (const clang::SubstTemplateTemplateParmStorage *subst =
                current.getAsSubstTemplateTemplateParm()) {
          current = markClangTemplateNameDefined(subst->getReplacement());
          continue;
        }
        if (clang::UsingShadowDecl *using_shadow =
                current.getAsUsingShadowDecl()) {
          using_shadow = const_cast<clang::UsingShadowDecl *>(
              llvm::dyn_cast_or_null<clang::UsingShadowDecl>(
                  markClangDeclObjectDefinedByKind(using_shadow)));
          return const_cast<clang::TemplateDecl *>(
              llvm::dyn_cast_or_null<clang::TemplateDecl>(
                  markClangDeclObjectDefinedByKind(
                      using_shadow->getTargetDecl())));
        }
        return nullptr;
      }
    };
    clang::TemplateDecl *clang_template_decl = resolve_template_decl(tname);
    clang::TypeAliasTemplateDecl *alias_decl =
        llvm::dyn_cast_or_null<clang::TypeAliasTemplateDecl>(
            clang_template_decl);
    SgDeclarationStatement *alias_semantic_sg_decl = nullptr;

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
    if (clang_template_decl != nullptr) {
      alias_semantic_sg_decl = lookupSgDeclarationForClangDecl(
          clang_template_decl, /*allow_on_demand=*/false);
    }

    SgTemplateTypedefDeclaration *alias_sg_decl = nullptr;
    if (alias_decl != nullptr) {
      if (SgDeclarationStatement *found_decl =
              lookupSgDeclarationForClangDecl(alias_decl,
                                              /*allow_on_demand=*/true)) {
        alias_sg_decl = isSgTemplateTypedefDeclaration(found_decl);
      }
      if (alias_sg_decl == nullptr &&
          p_decl_translation_in_progress.find(alias_decl) ==
              p_decl_translation_in_progress.end() &&
          p_decl_translation_on_demand.find(alias_decl) ==
              p_decl_translation_on_demand.end()) {
        if (SgNode *translated = TraverseOnDemand(alias_decl)) {
          alias_sg_decl = isSgTemplateTypedefDeclaration(translated);
        }
      }
      if (alias_sg_decl != nullptr) {
        alias_semantic_sg_decl = alias_sg_decl;
      }
    }
    SgTemplateArgumentPtrList template_args =
        buildTemplateArguments(template_specialization_type, false);
    SgTemplateArgumentPtrList deduced_args =
        buildTemplateArguments(template_specialization_type, false);
    if (alias_decl != nullptr) {
      size_t param_count = 0;
      bool has_parameter_pack = false;
      if (clang::TemplateParameterList *params =
              alias_decl->getTemplateParameters()) {
        param_count = params->size();
        for (unsigned i = 0; i < params->size(); ++i) {
          clang::NamedDecl *param = params->getParam(i);
          if ((llvm::dyn_cast_or_null<clang::TemplateTypeParmDecl>(param) !=
                   nullptr &&
               llvm::cast<clang::TemplateTypeParmDecl>(param)
                   ->isParameterPack()) ||
              (llvm::dyn_cast_or_null<clang::NonTypeTemplateParmDecl>(param) !=
                   nullptr &&
               llvm::cast<clang::NonTypeTemplateParmDecl>(param)
                   ->isParameterPack()) ||
              (llvm::dyn_cast_or_null<clang::TemplateTemplateParmDecl>(param) !=
                   nullptr &&
               llvm::cast<clang::TemplateTemplateParmDecl>(param)
                   ->isParameterPack())) {
            has_parameter_pack = true;
            break;
          }
        }
      }
      // Alias templates with parameter packs may legitimately carry more
      // expanded arguments than the raw parameter count.
      if (param_count > 0 && !has_parameter_pack) {
        if (template_args.size() > param_count) {
          template_args.resize(param_count);
        }
        if (deduced_args.size() > param_count) {
          deduced_args.resize(param_count);
        }
      }
    }

    SgScopeStatement *alias_type_scope = nullptr;
    if (alias_sg_decl != nullptr) {
      alias_type_scope = alias_sg_decl->get_scope();
    }
    if (alias_type_scope == nullptr && alias_decl != nullptr) {
      alias_type_scope =
          resolveScopeFromDeclContext(alias_decl->getDeclContext());
    }

    auto build_alias_type_in_scope = [&](clang::QualType qt) -> SgType * {
      qt = markClangQualTypeDefined(qt);
      if (qt.isNull()) {
        return nullptr;
      }
      if (alias_type_scope == nullptr ||
          alias_type_scope == SageBuilder::topScopeStack()) {
        return buildTypeFromQualifiedType(qt);
      }

      SageBuilder::pushScopeStack(alias_type_scope);
      SgType *type_in_alias_scope = buildTypeFromQualifiedType(qt);
      SageBuilder::popScopeStack();
      return type_in_alias_scope;
    };

    clang::QualType exact_aliased_qt = markClangQualTypeDefined(
        template_specialization_type->getAliasedType());
    SgType *aliased_type = build_alias_type_in_scope(exact_aliased_qt);
    if (isSgNonrealType(aliased_type) != nullptr &&
        !template_specialization_type->isDependentType()) {
      clang::QualType canonical_qt = markClangQualTypeDefined(
          template_specialization_type->getCanonicalTypeInternal());
      if (!canonical_qt.isNull()) {
        if (SgType *canonical_type = build_alias_type_in_scope(canonical_qt)) {
          aliased_type = canonical_type;
        }
      }
    }

    clang::NestedNameSpecifier alias_qualifier = std::nullopt;
    if (const clang::QualifiedTemplateName *qtn =
            tname.getAsQualifiedTemplateName()) {
      alias_qualifier = qtn->getQualifier();
    } else if (const clang::DependentTemplateName *dtn =
                   tname.getAsDependentTemplateName()) {
      alias_qualifier = dtn->getQualifier();
    }

    std::string alias_name =
        alias_decl != nullptr ? alias_decl->getNameAsString() : std::string();
    if (alias_name.empty()) {
      alias_name = getTemplateNameBase(tname);
    }
    if (alias_name.empty()) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[alias-template-name]: Clang type=%p has "
              "neither a declaration name nor a written template name\n",
              static_cast<void *>(template_specialization_type));
      ROSE_ABORT();
    }
    const SgName semantic_alias_name(buildExactTemplateInstantiationName(
        alias_name, template_specialization_type->template_arguments(),
        currentTemplateParameterDeclContext()));

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

        const clang::Type *segment_type =
            markClangTypeObjectDefinedByClass(nns.getAsType());
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
          spec = llvm::dyn_cast_or_null<clang::TemplateSpecializationType>(
              markClangTypeObjectDefinedByClass(spec));
          if (!spec->isDependentType()) {
            clang::QualType semantic_qt =
                spec->isTypeAlias()
                    ? markClangQualTypeDefined(spec->getAliasedType())
                    : markClangQualTypeDefined(clang::QualType(spec, 0));
            if (!semantic_qt.isNull()) {
              (void)build_alias_type_in_scope(semantic_qt);
            }
          }
          continue;
        }

        clang::QualType segment_qt =
            markClangQualTypeDefined(clang::QualType(segment_type, 0));
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

    bool alias_in_record_context = false;
    if (alias_decl != nullptr) {
      clang::DeclContext *alias_context =
          markClangDeclContextObjectDefined(readClangApiValueDefined(
              [&]() { return alias_decl->getDeclContext(); }));
      while (alias_context != nullptr) {
        const clang::Decl *alias_context_decl =
            clangDeclFromDeclContextDefined(alias_context);
        if (!llvm::isa_and_nonnull<clang::LinkageSpecDecl>(
                alias_context_decl)) {
          break;
        }
        alias_context =
            markClangDeclContextObjectDefined(readClangApiValueDefined(
                [&]() { return alias_context->getParent(); }));
      }
      alias_in_record_context =
          alias_context != nullptr &&
          readClangApiValueDefined([&]() { return alias_context->isRecord(); });
    }
    if (alias_in_record_context) {
      // Preserve explicitly qualified alias template spelling (e.g.
      // `Outer<T>::template Alias<U>`) rather than eagerly materializing a
      // typedef instantiation, which can drop the source qualifier.

      SgScopeStatement *base_scope = SageBuilder::topScopeStack();
      if (base_scope == nullptr) {
        base_scope = getGlobalScope();
      }

      SgType *record_alias_type = nullptr;
      if (alias_qualifier) {
        if (nestedNameSpecifierHasGlobal(alias_qualifier)) {
          base_scope = getGlobalScope();
        }
        record_alias_type = buildSemanticNonrealTypeFromNestedNameSpecifier(
            alias_qualifier, base_scope, SgName(alias_name), &template_args,
            &semantic_alias_name);
      } else {
        record_alias_type = SageBuilder::buildSemanticNonrealType(
            SgName(alias_name), base_scope, &template_args,
            &semantic_alias_name);
      }

      if (record_alias_type == nullptr) {
        std::cerr << "REX_FRONTEND_INVARIANT[record-template-alias]: failed "
                     "to build the exact alias type"
                  << std::endl;
        ROSE_ABORT();
      }
      *node = record_alias_type;
      return VisitType(template_specialization_type, node);
    }

    auto resolve_alias_decl_scope = [&]() -> SgScopeStatement * {
      auto resolve_alias_context =
          [&](clang::DeclContext *ctx) -> SgScopeStatement * {
        ctx = markClangDeclContextObjectDefined(ctx);
        while (ctx != nullptr) {
          const clang::Decl *ctx_decl = clangDeclFromDeclContextDefined(ctx);
          if (!llvm::isa_and_nonnull<clang::LinkageSpecDecl>(ctx_decl)) {
            break;
          }
          ctx = markClangDeclContextObjectDefined(
              readClangApiValueDefined([&]() { return ctx->getParent(); }));
        }
        if (ctx == nullptr) {
          std::cerr << "REX_FRONTEND_INVARIANT[template-alias-scope]: alias "
                       "has no declaration context"
                    << std::endl;
          ROSE_ABORT();
        }
        if (readClangApiValueDefined(
                [&]() { return ctx->isTranslationUnit(); })) {
          return getGlobalScope();
        }
        if (clang::NamespaceDecl *ns_decl =
                llvm::dyn_cast_or_null<clang::NamespaceDecl>(
                    clangDeclFromDeclContextDefined(ctx))) {
          if (SgNamespaceDeclarationStatement *sg_ns_decl =
                  ensureNamespaceDeclaration(ns_decl)) {
            if (SgNamespaceDefinitionStatement *ns_def =
                    sg_ns_decl->get_definition()) {
              return ns_def;
            }
          }
        }
        SgScopeStatement *resolved = resolveScopeFromDeclContext(ctx);
        if (resolved == nullptr) {
          std::cerr << "REX_FRONTEND_INVARIANT[template-alias-scope]: alias "
                       "declaration context has no exact translated scope"
                    << std::endl;
          ROSE_ABORT();
        }
        return resolved;
      };

      SgScopeStatement *resolved_scope =
          alias_semantic_sg_decl != nullptr
              ? alias_semantic_sg_decl->get_scope()
              : nullptr;

      clang::TemplateDecl *scope_decl =
          alias_decl != nullptr ? static_cast<clang::TemplateDecl *>(alias_decl)
                                : clang_template_decl;
      if (scope_decl != nullptr) {
        SgScopeStatement *lexical_scope = nullptr;
        if (clang::DeclContext *lexical_context =
                markClangDeclContextObjectDefined(readClangApiValueDefined(
                    [&]() { return scope_decl->getLexicalDeclContext(); }))) {
          lexical_scope = resolve_alias_context(lexical_context);
        }
        SgScopeStatement *semantic_scope = resolve_alias_context(
            markClangDeclContextObjectDefined(readClangApiValueDefined(
                [&]() { return scope_decl->getDeclContext(); })));
        if (lexical_scope != nullptr && lexical_scope != semantic_scope) {
          std::cerr << "REX_FRONTEND_INVARIANT[template-alias-scope]: lexical "
                       "and semantic scopes differ"
                    << std::endl;
          ROSE_ABORT();
        }
        resolved_scope = semantic_scope;
      }
      if (resolved_scope == nullptr) {
        std::cerr << "REX_FRONTEND_INVARIANT[template-alias-scope]: alias has "
                     "no exact translated scope"
                  << std::endl;
        ROSE_ABORT();
      }

      return resolved_scope;
    };

    SgScopeStatement *alias_decl_scope = resolve_alias_decl_scope();
    SgScopeStatement *scope = alias_decl_scope;
    ASSERT_not_null(scope);

    SgScopeStatement *alias_decl_target_scope = alias_decl_scope;

    if (alias_sg_decl != nullptr && alias_decl_target_scope != nullptr) {
      if (isSgAuxiliaryDeclarationList(alias_sg_decl->get_parent()) !=
          nullptr) {
        if (alias_sg_decl->get_scope() != alias_decl_target_scope) {
          std::cerr << "REX_FRONTEND_INVARIANT[template-alias-owner]: semantic "
                       "alias declaration scope does not match its exact Clang "
                       "declaration context"
                    << std::endl;
          ROSE_ABORT();
        }
        requireTypedNonLexicalDeclarationOwner(
            alias_sg_decl, "VisitTemplateSpecializationType:semantic-alias");
      } else {
        if (alias_sg_decl->get_parent() != alias_decl_target_scope ||
            alias_sg_decl->get_scope() != alias_decl_target_scope ||
            !alias_decl_target_scope->statementExistsInScope(alias_sg_decl)) {
          std::cerr << "REX_FRONTEND_INVARIANT[template-alias-owner]: source "
                       "alias declaration has no exact lexical owner"
                    << std::endl;
          ROSE_ABORT();
        }
        (void)requireClangOrderedDeclarationProvenanceForFrontend(
            alias_sg_decl, "VisitTemplateSpecializationType:source-alias");
      }
    }

    if (alias_sg_decl != nullptr && aliased_type != nullptr &&
        scope != nullptr) {
      SgName alias_name = alias_sg_decl->get_name();
      if (alias_decl != nullptr) {
        alias_name = SgName(alias_decl->getNameAsString());
      }
      SgName alias_name_with_args(buildExactTemplateInstantiationName(
          alias_name.getString(),
          markClangTemplateArgumentArrayDefined(
              template_specialization_type->template_arguments()),
          currentTemplateParameterDeclContext()));
      if (SgTemplateTypedefSymbol *existing_symbol =
              scope->lookup_template_typedef_symbol(alias_name_with_args)) {
        SgTemplateInstantiationTypedefDeclaration *existing_instantiation =
            isSgTemplateInstantiationTypedefDeclaration(
                existing_symbol->get_declaration());
        if (existing_instantiation == nullptr) {
          SgScopeStatement *base_scope = scope;
          *node = SageBuilder::buildSemanticNonrealType(
              alias_name, base_scope, &template_args, &alias_name_with_args);
          return VisitType(template_specialization_type, node);
        }
        if (existing_instantiation->get_type() == nullptr ||
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
      SgTemplateInstantiationTypedefDeclaration *inst_decl =
          SageBuilder::buildTemplateInstantiationTypedefDeclaration_nfi(
              SageBuilder::typedef_declaration_ownership::semanticAuxiliary(),
              SgTypedefDeclaration::e_using, alias_name, aliased_type, scope,
              alias_sg_decl, template_args, alias_name_with_args);
      if (inst_decl != nullptr) {
        validateTypedefDeclarationReferenceShared(inst_decl);
        inst_decl->get_templateArguments() = template_args;
        inst_decl->get_deducedTemplateArguments() = deduced_args;
        setSynthesizedFileInfo(inst_decl);
        SageBuilder::setTemplateArgumentParents(inst_decl);
        if (inst_decl->get_specializedTemplateDeclaration() == nullptr) {
          inst_decl->set_specializedTemplateDeclaration(alias_sg_decl);
        }
        registerDeclarationSymbol(inst_decl);
        requireTypedNonLexicalDeclarationOwner(
            inst_decl,
            "VisitTemplateSpecializationType:alias-instantiation-owner");
        *node = inst_decl->get_type();
        return VisitType(template_specialization_type, node);
      }
    }

    if (aliased_type != nullptr) {
      *node = aliased_type;
      return VisitType(template_specialization_type, node);
    }

    std::cerr << "REX_FRONTEND_INVARIANT[alias-template-type]: alias '"
              << alias_name
              << "' has neither an exact translated alias declaration nor "
                 "an aliased type"
              << std::endl;
    ROSE_ABORT();
  }

  if (template_specialization_type->isDependentType()) {
    clang::TemplateName tname = markClangTemplateNameDefined(
        template_specialization_type->getTemplateName());
    std::string base_name;
    if (clang::TemplateTemplateParmDecl *parameter =
            llvm::dyn_cast_or_null<clang::TemplateTemplateParmDecl>(
                tname.getAsTemplateDecl());
        parameter != nullptr && parameter->getName().empty()) {
      base_name = resolveExactTemplateParameterName(parameter->getDepth(),
                                                    parameter->getIndex());
      if (base_name.empty()) {
        base_name = resolveTemplateParameterNameFromDeclContext(
            currentTemplateParameterDeclContext(), parameter->getDepth(),
            parameter->getIndex());
      }
      if (base_name.empty()) {
        base_name = resolveTemplateParameterNameFromSageScope(
            SageBuilder::topScopeStack(), parameter->getDepth(),
            parameter->getIndex());
      }
      if (base_name.empty()) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[template-name]: anonymous Clang "
                "template-template parameter depth=%u index=%u has no exact "
                "source parameter identity\n",
                parameter->getDepth(), parameter->getIndex());
        ROSE_ABORT();
      }
    } else {
      base_name = getTemplateNameBase(tname);
    }
    ROSE_ASSERT(!base_name.empty());

    // A dependent TemplateSpecializationType is represented by a semantic
    // SgNonrealDecl below.  Build its argument expressions inside the same
    // explicit semantic transaction; otherwise a parameter reference or
    // dependent expression is first published as a lexical source node and
    // only discovered to be semantic when buildNonrealType adopts it.
    SgTemplateArgumentPtrList tpl_args;
    {
      SemanticExpressionConstruction semantic_arguments(
          p_semantic_template_argument_expression_depth,
          "dependent-template-specialization-arguments");
      tpl_args = buildTemplateArguments(template_specialization_type, true);
    }

    SgScopeStatement *base_scope = SageBuilder::topScopeStack();
    ROSE_ASSERT(base_scope != nullptr);
    auto resolve_semantic_template_decl =
        [&](clang::TemplateName current) -> clang::TemplateDecl * {
      current = markClangTemplateNameDefined(current);
      for (;;) {
        if (clang::TemplateDecl *decl = current.getAsTemplateDecl()) {
          return const_cast<clang::TemplateDecl *>(
              llvm::dyn_cast_or_null<clang::TemplateDecl>(
                  markClangDeclObjectDefinedByKind(decl)));
        }
        if (const clang::QualifiedTemplateName *qualified =
                current.getAsQualifiedTemplateName()) {
          current =
              markClangTemplateNameDefined(qualified->getUnderlyingTemplate());
          continue;
        }
        if (const clang::SubstTemplateTemplateParmStorage *substitution =
                current.getAsSubstTemplateTemplateParm()) {
          current =
              markClangTemplateNameDefined(substitution->getReplacement());
          continue;
        }
        if (clang::UsingShadowDecl *using_shadow =
                current.getAsUsingShadowDecl()) {
          using_shadow = const_cast<clang::UsingShadowDecl *>(
              llvm::dyn_cast_or_null<clang::UsingShadowDecl>(
                  markClangDeclObjectDefinedByKind(using_shadow)));
          return using_shadow != nullptr
                     ? const_cast<clang::TemplateDecl *>(
                           llvm::dyn_cast_or_null<clang::TemplateDecl>(
                               markClangDeclObjectDefinedByKind(
                                   using_shadow->getTargetDecl())))
                     : nullptr;
        }
        return nullptr;
      }
    };
    clang::TemplateDecl *semantic_template_decl =
        resolve_semantic_template_decl(tname);
    const bool semantic_template_is_parameter =
        llvm::isa_and_nonnull<clang::TemplateTemplateParmDecl>(
            semantic_template_decl);
    if (semantic_template_decl != nullptr && !semantic_template_is_parameter) {
      clang::DeclContext *semantic_context = markClangDeclContextObjectDefined(
          semantic_template_decl->getDeclContext());
      if (semantic_context == nullptr) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[dependent-template-scope]: "
                "template=%s has no exact declaration context\n",
                base_name.c_str());
        ROSE_ABORT();
      }
      base_scope = resolveScopeFromDeclContext(semantic_context);
      if (base_scope == nullptr) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[dependent-template-scope]: "
                "template=%s declaration context has no exact Sage scope\n",
                base_name.c_str());
        ROSE_ABORT();
      }
    }

    if (template_specialization_type->isCurrentInstantiation() &&
        semantic_template_decl == nullptr) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[current-instantiation-template]: "
              "template-id=%s has no exact primary template declaration\n",
              base_name.c_str());
      ROSE_ABORT();
    }

    auto publish_semantic_template_identity =
        [&](SgType *translated_type) -> SgType * {
      SgNonrealType *nonreal_type = isSgNonrealType(translated_type);
      SgNonrealDecl *nonreal_declaration =
          nonreal_type != nullptr
              ? isSgNonrealDecl(nonreal_type->get_declaration())
              : nullptr;
      if (nonreal_declaration == nullptr) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[dependent-template-type]: "
                "template-id=%s did not produce one exact nonreal type "
                "identity\n",
                base_name.c_str());
        ROSE_ABORT();
      }
      if (clang::TemplateTemplateParmDecl *template_parameter =
              llvm::dyn_cast_or_null<clang::TemplateTemplateParmDecl>(
                  semantic_template_decl)) {
        auto mapped_sage_parameter =
            [&](const clang::TemplateTemplateParmDecl *parameter)
            -> SgTemplateParameter * {
          if (parameter == nullptr) {
            return nullptr;
          }
          if (SgTemplateParameter *active =
                  lookupActiveTemplateParameterSurface(
                      parameter,
                      "dependent-template-template-parameter-exact")) {
            return active;
          }

          const clang::TemplateTemplateParmDecl *active_parameter = nullptr;
          for (const clang::TemplateParameterList *level :
               collectTemplateParameterLevelsFromDeclContext(
                   currentTemplateParameterDeclContext())) {
            level = markClangTemplateParameterListDefined(level);
            const unsigned index = parameter->getIndex();
            if (level == nullptr || index >= level->size()) {
              continue;
            }
            const auto *candidate =
                llvm::dyn_cast_or_null<clang::TemplateTemplateParmDecl>(
                    markClangDeclObjectDefinedByKind(level->getParam(index)));
            if (candidate == nullptr ||
                candidate->getDepth() != parameter->getDepth() ||
                candidate->getIndex() != index) {
              continue;
            }
            const clang::DeclContext *parameter_context =
                markClangDeclContextObjectDefined(parameter->getDeclContext());
            const bool clang_lost_written_owner =
                parameter_context != nullptr &&
                parameter_context->isTranslationUnit();
            if (!clang_lost_written_owner &&
                candidate->getCanonicalDecl() !=
                    parameter->getCanonicalDecl()) {
              continue;
            }
            if (active_parameter != nullptr &&
                active_parameter->getCanonicalDecl() !=
                    candidate->getCanonicalDecl()) {
              fprintf(stderr,
                      "REX_FRONTEND_INVARIANT[template-template-parameter-"
                      "type]: template-id=%s depth=%u index=%u has multiple "
                      "active parameter families\n",
                      base_name.c_str(), parameter->getDepth(), index);
              ROSE_ABORT();
            }
            active_parameter = candidate;
          }
          if (active_parameter != nullptr) {
            if (SgTemplateParameter *active =
                    lookupActiveTemplateParameterSurface(
                        active_parameter,
                        "dependent-template-template-parameter-context")) {
              return active;
            }
            auto active_mapping = p_decl_translation_map.find(
                const_cast<clang::TemplateTemplateParmDecl *>(
                    active_parameter));
            if (active_mapping != p_decl_translation_map.end()) {
              if (SgTemplateParameter *active =
                      isSgTemplateParameter(active_mapping->second)) {
                return active;
              }
            }
          }
          auto translated_parameter = p_decl_translation_map.find(
              const_cast<clang::TemplateTemplateParmDecl *>(parameter));
          return translated_parameter != p_decl_translation_map.end()
                     ? isSgTemplateParameter(translated_parameter->second)
                     : nullptr;
        };

        const clang::NamedDecl *context_parameter =
            resolveTemplateParameterFromDeclContext(
                currentTemplateParameterDeclContext(),
                template_parameter->getDepth(), template_parameter->getIndex());
        const clang::TemplateTemplateParmDecl *context_template_parameter =
            llvm::dyn_cast_or_null<clang::TemplateTemplateParmDecl>(
                context_parameter);
        if (context_parameter != nullptr &&
            (context_template_parameter == nullptr ||
             context_template_parameter->getDepth() !=
                 template_parameter->getDepth() ||
             context_template_parameter->getIndex() !=
                 template_parameter->getIndex())) {
          fprintf(stderr,
                  "REX_FRONTEND_INVARIANT[template-template-parameter-type]: "
                  "template-id=%s semantic parameter depth=%u index=%u "
                  "resolved context parameter kind=%s depth=%u index=%u\n",
                  base_name.c_str(), template_parameter->getDepth(),
                  template_parameter->getIndex(),
                  context_parameter->getDeclKindName(),
                  context_template_parameter != nullptr
                      ? context_template_parameter->getDepth()
                      : 0,
                  context_template_parameter != nullptr
                      ? context_template_parameter->getIndex()
                      : 0);
          ROSE_ABORT();
        }

        SgTemplateParameter *semantic_mapped_parameter =
            mapped_sage_parameter(template_parameter);
        SgTemplateParameter *context_mapped_parameter =
            mapped_sage_parameter(context_template_parameter);
        if (semantic_mapped_parameter != nullptr &&
            context_mapped_parameter != nullptr &&
            semantic_mapped_parameter != context_mapped_parameter) {
          fprintf(stderr,
                  "REX_FRONTEND_INVARIANT[template-template-parameter-type]: "
                  "template-id=%s semantic=%p context=%p map to distinct Sage "
                  "parameters semantic=%p context=%p\n",
                  base_name.c_str(), static_cast<void *>(template_parameter),
                  static_cast<const void *>(context_template_parameter),
                  static_cast<void *>(semantic_mapped_parameter),
                  static_cast<void *>(context_mapped_parameter));
          ROSE_ABORT();
        }
        SgTemplateParameter *mapped_parameter =
            semantic_mapped_parameter != nullptr ? semantic_mapped_parameter
                                                 : context_mapped_parameter;
        SgTemplateParameter *sage_parameter = mapped_parameter;
        SgTemplateDeclaration *parameter_identity =
            sage_parameter != nullptr
                ? isSgTemplateDeclaration(
                      sage_parameter->get_templateDeclaration())
                : nullptr;
        SgTemplateType *parameter_type =
            sage_parameter != nullptr
                ? isSgTemplateType(sage_parameter->get_type())
                : nullptr;
        if (sage_parameter == nullptr || parameter_identity == nullptr ||
            parameter_type == nullptr ||
            parameter_type->get_template_parameter() != sage_parameter ||
            parameter_type->get_template_parameter_depth() !=
                static_cast<int>(template_parameter->getDepth()) ||
            parameter_type->get_template_parameter_position() !=
                static_cast<int>(template_parameter->getIndex()) ||
            (nonreal_declaration->get_templateDeclaration() != nullptr &&
             nonreal_declaration->get_templateDeclaration() !=
                 parameter_identity)) {
          fprintf(stderr,
                  "REX_FRONTEND_INVARIANT[template-template-parameter-type]: "
                  "template-id=%s parameter=%p context=%p mapped=%p "
                  "identity=%p type=%p has no exact producer-published "
                  "parameter "
                  "identity\n",
                  base_name.c_str(), static_cast<void *>(template_parameter),
                  static_cast<const void *>(context_template_parameter),
                  static_cast<void *>(mapped_parameter),
                  static_cast<void *>(parameter_identity),
                  static_cast<void *>(parameter_type));
          ROSE_ABORT();
        }
        nonreal_declaration->set_templateDeclaration(parameter_identity);
      } else if (semantic_template_decl != nullptr) {
        linkNonrealTemplateDeclaration(nonreal_declaration,
                                       semantic_template_decl,
                                       "dependent-template-specialization");
      }
      if (template_specialization_type->isCurrentInstantiation() &&
          isSgTemplateClassDeclaration(
              nonreal_declaration->get_templateDeclaration()) == nullptr) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[current-instantiation-template]: "
                "template-id=%s did not publish its exact primary Sage class "
                "template\n",
                base_name.c_str());
        ROSE_ABORT();
      }
      return translated_type;
    };

    clang::NestedNameSpecifier qualifier = std::nullopt;
    if (const clang::QualifiedTemplateName *qtn =
            tname.getAsQualifiedTemplateName()) {
      qualifier = qtn->getQualifier();
    } else if (const clang::DependentTemplateName *dtn =
                   tname.getAsDependentTemplateName()) {
      qualifier = dtn->getQualifier();
    }

    if (qualifier) {
      const SgName semantic_name(buildExactTemplateInstantiationName(
          base_name, template_specialization_type->template_arguments(),
          currentTemplateParameterDeclContext()));
      *node = publish_semantic_template_identity(
          buildSemanticNonrealTypeFromNestedNameSpecifier(
              qualifier, base_scope, SgName(base_name), &tpl_args,
              &semantic_name));
      return VisitType(template_specialization_type, node);
    }

    // For dependent names without an explicit qualifier, preserve the spelling
    // as-written. Synthesizing declaration-context qualifiers here loses
    // dependent template arguments (e.g., `Outer<T>::Inner<U>` becoming
    // `Outer::Inner<U>`), which breaks correctness.
    const SgName semantic_name(buildExactTemplateInstantiationName(
        base_name, template_specialization_type->template_arguments(),
        currentTemplateParameterDeclContext()));
    *node = publish_semantic_template_identity(
        SageBuilder::buildSemanticNonrealType(SgName(base_name), base_scope,
                                              &tpl_args, &semantic_name));
    return VisitType(template_specialization_type, node);
  }

  // Extract template name
  clang::TemplateName tname = markClangTemplateNameDefined(
      template_specialization_type->getTemplateName());
  std::string template_name = mangleTemplateName(tname);
  if (clang::QualType qt = markClangQualTypeDefined(
          clang::QualType(template_specialization_type, 0));
      !qt.isNull()) {
    if (const clang::CXXRecordDecl *record_decl =
            cxxRecordDeclFromQualTypeWithoutDefinitionLookup(qt)) {
      record_decl = llvm::dyn_cast_or_null<clang::CXXRecordDecl>(
          markClangDeclObjectDefinedByKind(record_decl));
      if (llvm::isa<clang::ClassTemplateSpecializationDecl>(record_decl) ||
          llvm::isa<clang::ClassTemplatePartialSpecializationDecl>(
              record_decl)) {
        auto *mutable_record = const_cast<clang::CXXRecordDecl *>(record_decl);
        mutable_record = llvm::dyn_cast_or_null<clang::CXXRecordDecl>(
            const_cast<clang::Decl *>(
                markClangDeclObjectDefinedByKind(mutable_record)));
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
        SgTemplateInstantiationDecl *inst_decl =
            isSgTemplateInstantiationDecl(specialization_decl);
        if (inst_decl == nullptr) {
          if (SgClassDeclaration *sg_decl =
                  isSgClassDeclaration(specialization_decl)) {
            inst_decl = isSgTemplateInstantiationDecl(
                sg_decl->get_firstNondefiningDeclaration());
            if (inst_decl == nullptr) {
              inst_decl = isSgTemplateInstantiationDecl(
                  sg_decl->get_definingDeclaration());
            }
          }
        }
        if (inst_decl != nullptr && inst_decl->get_type() != nullptr) {
          *node = inst_decl->get_type();
          return VisitType(template_specialization_type, node);
        }
      }
    }
  }

  SgTemplateClassDeclaration *template_decl = requireExactTemplateDeclaration(
      template_name, template_specialization_type);

  // Get or create template instantiation
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
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToSageTranslator::VisitTemplateTypeParmType" << std::endl;
#endif
  bool res = true;

  const clang::TemplateTypeParmDecl *param_decl =
      llvm::dyn_cast_or_null<clang::TemplateTypeParmDecl>(
          markClangDeclObjectDefinedByKind(template_type_parm_type->getDecl()));

  if (param_decl != nullptr && param_decl->isImplicit() &&
      isImplicitAutoPlaceholderTemplateParamName(
          param_decl->getNameAsString())) {
    *node = SageBuilder::buildAutoType();
    return VisitType(template_type_parm_type, node) && res;
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
      level = markClangTemplateParameterListDefined(level);
      if (level == nullptr || index >= level->size()) {
        continue;
      }
      auto *candidate = const_cast<clang::TemplateTypeParmDecl *>(
          llvm::dyn_cast_or_null<clang::TemplateTypeParmDecl>(
              markClangDeclObjectDefinedByKind(level->getParam(index))));
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
  clang::NamedDecl *mapped_param_key = nullptr;
  const char *mapped_param_key_kind = "none";
  clang::TemplateTypeParmDecl *active_parameter = resolve_active_parameter();
  if (active_parameter == nullptr && param_decl == nullptr) {
    active_parameter = resolveActiveTemplateTypeParameterSurface(
        template_type_parm_type->getDepth(),
        template_type_parm_type->getIndex(),
        "VisitTemplateTypeParmType:canonical-active-surface");
  }
  if (active_parameter != nullptr) {
    mapped_param = lookupActiveTemplateParameterSurface(
        active_parameter, "VisitTemplateTypeParmType:active-context");
    if (mapped_param != nullptr) {
      mapped_param_key = active_parameter;
      mapped_param_key_kind = "active-source-surface";
    } else {
      mapped_param = lookupPublishedTemplateTypeParameterFamily(
          active_parameter, "VisitTemplateTypeParmType:active-family",
          /*require_exact_owner=*/false);
      if (mapped_param != nullptr) {
        mapped_param_key = active_parameter;
        mapped_param_key_kind = "active-family";
      }
    }
  }
  if (mapped_param == nullptr && active_parameter == nullptr &&
      param_decl != nullptr) {
    auto *mutable_param_decl =
        const_cast<clang::TemplateTypeParmDecl *>(param_decl);

    mapped_param = lookupActiveTemplateParameterSurface(
        mutable_param_decl, "VisitTemplateTypeParmType:exact-parameter");
    if (mapped_param != nullptr) {
      mapped_param_key = mutable_param_decl;
      mapped_param_key_kind = "exact-source-surface";
    } else {
      mapped_param = lookupPublishedTemplateTypeParameterFamily(
          mutable_param_decl, "VisitTemplateTypeParmType:published-family",
          /*require_exact_owner=*/false);
      if (mapped_param != nullptr) {
        mapped_param_key = mutable_param_decl;
        mapped_param_key_kind = "published-family";
      }
    }

    // Type lowering is a consumer of template-parameter identity, not a
    // declaration producer.  In particular, a constraint can mention its own
    // parameter while the owning declaration shell is intentionally not yet
    // published.  Re-entering declaration translation here used to hide that
    // ordering contract and recursively rebuilt the active function.  The
    // exact Clang declaration and producer-owned depth/index name map below
    // provide the spelling; an already-published Sage parameter is optional.
  }

  std::string param_name;
  auto resolve_from_decl_context_stack = [&](unsigned depth,
                                             unsigned index) -> std::string {
    if (p_template_parameter_decl_context_stack.empty()) {
      return "";
    }
    return resolveTemplateParameterNameFromDeclContext(
        p_template_parameter_decl_context_stack.back(), depth, index);
  };
  auto resolve_from_sage_scope = [&](unsigned depth,
                                     unsigned index) -> std::string {
    std::string resolved_name;
    if (mapped_param != nullptr) {
      if (SgDeclarationStatement *owner_decl = isSgDeclarationStatement(
              mapped_param->get_templateDeclaration())) {
        if (SgDeclarationScope *decl_scope =
                SageBuilder::getOrCreateNonrealDeclarationScope(owner_decl)) {
          resolved_name = resolveTemplateParameterNameFromSageScope(
              decl_scope, depth, index);
        }
        if (resolved_name.empty()) {
          resolved_name = resolveTemplateParameterNameFromSageScope(
              owner_decl->get_scope(), depth, index);
        }
      }
    }
    if (resolved_name.empty()) {
      resolved_name = resolveTemplateParameterNameFromSageScope(
          SageBuilder::topScopeStack(), depth, index);
    }
    return resolved_name;
  };

  if (mapped_param != nullptr) {
    if (SgTemplateType *mapped_type =
            isSgTemplateType(mapped_param->get_type());
        mapped_type != nullptr) {
      param_name = mapped_type->get_name().getString();
    }
    if (param_name.empty()) {
      if (SgInitializedName *mapped_init = mapped_param->get_initializedName();
          mapped_init != nullptr) {
        param_name = mapped_init->get_name().getString();
      }
    }
    param_name = normalizeTemplateTypeParamName(param_name);
  }

  std::string mapped_name_for_debug = param_name;

  if (active_parameter != nullptr) {
    // Clang canonicalizes some partial-specialization argument types to a
    // primary-template parameter declaration at the same depth/index.  The
    // exact active written list is the source and semantic owner here,
    // including when its parameter is intentionally unnamed.
    param_name =
        normalizeTemplateTypeParamName(active_parameter->getNameAsString());
  } else if (param_decl != nullptr) {
    // A TemplateTypeParmType carrying a declaration has an exact semantic
    // owner.  Its spelling must not be replaced by an ambient depth/index name
    // map: coordinates are local to a template parameter list and therefore
    // are not globally unique.  The mapped Sage parameter, when present, is
    // validated against this declaration-owned spelling below.
    param_name = normalizeTemplateTypeParamName(param_decl->getNameAsString());
    if (const clang::IdentifierInfo *identifier =
            template_type_parm_type->getIdentifier()) {
      const std::string identifier_name =
          normalizeTemplateTypeParamName(identifier->getName().str());
      if (!param_name.empty() && !identifier_name.empty() &&
          param_name != identifier_name) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[template-type-parameter-source]: "
                "declaration=%p name='%s' and type identifier='%s' disagree\n",
                static_cast<const void *>(param_decl), param_name.c_str(),
                identifier_name.c_str());
        ROSE_ABORT();
      }
      if (param_name.empty()) {
        param_name = identifier_name;
      }
    }
  } else {
    const std::string exact_source_name = normalizeTemplateTypeParamName(
        resolveExactTemplateParameterName(template_type_parm_type->getDepth(),
                                          template_type_parm_type->getIndex()));
    if (!exact_source_name.empty()) {
      param_name = exact_source_name;
    }

    if (const clang::IdentifierInfo *identifier =
            template_type_parm_type->getIdentifier()) {
      param_name = preferHigherQualityTemplateParamName(
          param_name, identifier->getName().str());
    }
    std::string active_context_name =
        resolve_from_decl_context_stack(template_type_parm_type->getDepth(),
                                        template_type_parm_type->getIndex());
    if (!active_context_name.empty()) {
      param_name =
          preferHigherQualityTemplateParamName(param_name, active_context_name);
    }
    std::string scope_name =
        resolve_from_sage_scope(template_type_parm_type->getDepth(),
                                template_type_parm_type->getIndex());
    param_name = preferHigherQualityTemplateParamName(param_name, scope_name);
  }

  auto resolve_exact_parameter_decl =
      [&]() -> const clang::TemplateTypeParmDecl * {
    if (active_parameter != nullptr) {
      return active_parameter;
    }
    if (param_decl != nullptr) {
      return param_decl;
    }
    if (const clang::TemplateTypeParmDecl *active =
            resolve_active_parameter()) {
      return active;
    }
    return resolveExactTemplateTypeParameterDeclaration(
        template_type_parm_type->getDepth(),
        template_type_parm_type->getIndex());
  };
  const clang::TemplateTypeParmDecl *identity_param_decl =
      resolve_exact_parameter_decl();
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
      readClangApiValueDefined(
          [&]() { return identity_param_decl->getIdentifier(); }) == nullptr;
  const bool is_canonical_parameter_identity =
      param_decl == nullptr && readClangApiValueDefined([&]() {
        return template_type_parm_type->isCanonicalUnqualified();
      });

  if (param_name.empty() && !is_exactly_unnamed_parameter &&
      !is_canonical_parameter_identity) {
    const clang::Decl *active_context_decl =
        llvm::dyn_cast_or_null<clang::Decl>(active_context);
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[template-type-parameter]: depth=%u "
            "index=%u mapped-name='%s' clang-decl=%p clang-name='%s' "
            "active-context=%p(%s) context-depth=%zu exact-name-depth=%zu "
            "has no exact declared or canonical identity\n",
            template_type_parm_type->getDepth(),
            template_type_parm_type->getIndex(), mapped_name_for_debug.c_str(),
            static_cast<const void *>(param_decl),
            param_decl != nullptr ? param_decl->getNameAsString().c_str() : "",
            static_cast<const void *>(active_context),
            active_context_decl != nullptr
                ? active_context_decl->getDeclKindName()
                : "<null>",
            p_template_parameter_decl_context_stack.size(),
            p_exact_template_parameter_name_stack.size());
    ROSE_ABORT();
  }

  unsigned depth = 0;
  unsigned index = 0;
  if (active_parameter == nullptr &&
      parseTemplateParamDepthAndIndex(param_name, &depth, &index)) {
    std::string resolved_name;
    if (param_decl != nullptr) {
      resolved_name = resolveTemplateParameterNameFromDeclContext(
          param_decl->getDeclContext(), depth, index);
    }
    if (resolved_name.empty()) {
      resolved_name = resolve_from_decl_context_stack(depth, index);
    }
    if (resolved_name.empty()) {
      resolved_name = resolve_from_sage_scope(depth, index);
    }

    if (!resolved_name.empty()) {
      param_name = resolved_name;
    }
  }

  if (mapped_param != nullptr) {
    SgTemplateType *mapped_type = isSgTemplateType(mapped_param->get_type());
    // SgTemplateType represents the declared parameter identity.  A
    // canonical TemplateTypeParmType can describe a pack-expansion use and
    // report a use-site pack bit that differs from the declaration.  Once the
    // exact declaration is known, its pack property is the only valid owner
    // property for the Sage parameter type.
    const bool is_pack =
        identity_param_decl != nullptr
            ? readClangApiValueDefined(
                  [&]() { return identity_param_decl->isParameterPack(); })
            : readClangApiValueDefined(
                  [&]() { return template_type_parm_type->isParameterPack(); });
    if (mapped_type == nullptr ||
        mapped_param->get_parameterType() !=
            SgTemplateParameter::type_parameter ||
        mapped_type->get_template_parameter() != mapped_param ||
        mapped_type->get_template_parameter_depth() !=
            static_cast<int>(identity_depth) ||
        mapped_type->get_template_parameter_position() !=
            static_cast<int>(identity_index) ||
        mapped_type->get_packed() != is_pack ||
        mapped_type->get_name().getString() != param_name) {
      fprintf(
          stderr,
          "REX_FRONTEND_INVARIANT[template-type-parameter-identity]: "
          "parameter=%p parameter-kind=%d type=%p type-parameter=%p "
          "expected-depth=%u actual-depth=%d expected-index=%u "
          "actual-index=%d expected-name='%s' actual-name='%s' "
          "expected-pack=%d actual-pack=%d does not own its exact "
          "semantic type; clang-parameter=%p name='%s' mapped-key=%p "
          "name='%s' lookup=%s active-parameter=%p depth=%d index=%d "
          "use-depth=%u use-index=%u owner=%p/%s\n",
          static_cast<void *>(mapped_param),
          static_cast<int>(mapped_param->get_parameterType()),
          static_cast<void *>(mapped_type),
          static_cast<void *>(mapped_type != nullptr
                                  ? mapped_type->get_template_parameter()
                                  : nullptr),
          identity_depth,
          mapped_type != nullptr ? mapped_type->get_template_parameter_depth()
                                 : -1,
          identity_index,
          mapped_type != nullptr
              ? mapped_type->get_template_parameter_position()
              : -1,
          param_name.c_str(),
          mapped_type != nullptr ? mapped_type->get_name().getString().c_str()
                                 : "<null>",
          is_pack ? 1 : 0,
          mapped_type != nullptr && mapped_type->get_packed() ? 1 : 0,
          static_cast<const void *>(param_decl),
          param_decl != nullptr ? param_decl->getNameAsString().c_str()
                                : "<null>",
          static_cast<void *>(mapped_param_key),
          mapped_param_key != nullptr
              ? mapped_param_key->getNameAsString().c_str()
              : "<null>",
          mapped_param_key_kind, static_cast<void *>(active_parameter),
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
    *node = mapped_type;
    return VisitType(template_type_parm_type, node) && res;
  }

  SgTemplateType *template_type =
      SageBuilder::buildTemplateType(SgName(param_name));
  template_type->set_template_parameter_depth(static_cast<int>(identity_depth));
  template_type->set_template_parameter_position(
      static_cast<int>(identity_index));
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
        p_exact_template_parameter_name_stack.size(),
        p_template_parameter_surface_stack.size());
    for (size_t context_index = 0;
         context_index < p_template_parameter_decl_context_stack.size();
         ++context_index) {
      const clang::DeclContext *context =
          p_template_parameter_decl_context_stack[context_index];
      const clang::Decl *context_decl =
          llvm::dyn_cast_or_null<clang::Decl>(context);
      const clang::NamedDecl *named_context =
          llvm::dyn_cast_or_null<clang::NamedDecl>(context_decl);
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[template-type-parameter-context-"
              "stack]: slot=%zu context=%p/%s/%s\n",
              context_index, static_cast<const void *>(context),
              context_decl != nullptr ? context_decl->getDeclKindName()
                                      : "<null>",
              named_context != nullptr
                  ? named_context->getQualifiedNameAsString().c_str()
                  : "<unnamed>");
    }
    unsigned level_index = 0;
    for (const clang::TemplateParameterList *level :
         collectTemplateParameterLevelsFromDeclContext(active_context)) {
      level = markClangTemplateParameterListDefined(level);
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[template-type-parameter-level]: "
              "level=%u parameters=%p size=%u\n",
              level_index++, static_cast<const void *>(level),
              level != nullptr ? level->size() : 0);
      if (level == nullptr) {
        continue;
      }
      for (unsigned parameter_index = 0; parameter_index < level->size();
           ++parameter_index) {
        const clang::NamedDecl *candidate =
            llvm::dyn_cast_or_null<clang::NamedDecl>(
                markClangDeclObjectDefinedByKind(
                    level->getParam(parameter_index)));
        const clang::TemplateTypeParmDecl *type_candidate =
            llvm::dyn_cast_or_null<clang::TemplateTypeParmDecl>(candidate);
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[template-type-parameter-level]: "
                "level=%u slot=%u declaration=%p/%s name='%s' "
                "depth=%d index=%d\n",
                level_index - 1, parameter_index,
                static_cast<const void *>(candidate),
                candidate != nullptr ? candidate->getDeclKindName() : "<null>",
                candidate != nullptr ? candidate->getNameAsString().c_str()
                                     : "",
                type_candidate != nullptr
                    ? static_cast<int>(type_candidate->getDepth())
                    : -1,
                type_candidate != nullptr
                    ? static_cast<int>(type_candidate->getIndex())
                    : -1);
      }
    }
    ROSE_ABORT();
  }
  publishCanonicalTemplateParameterSourceIdentity(
      template_type,
      const_cast<clang::TemplateTypeParmDecl *>(identity_param_decl),
      "VisitTemplateTypeParmType");
  template_type->set_packed(
      identity_param_decl != nullptr
          ? readClangApiValueDefined(
                [&]() { return identity_param_decl->isParameterPack(); })
          : readClangApiValueDefined(
                [&]() { return template_type_parm_type->isParameterPack(); }));
  *node = template_type;

  return VisitType(template_type_parm_type, node) && res;
}

bool ClangToSageTranslator::VisitTypedefType(clang::TypedefType *typedef_type,
                                             SgNode **node) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToSageTranslator::VisitTypedefType" << std::endl;
#endif

  typedef_type = const_cast<clang::TypedefType *>(
      llvm::dyn_cast_or_null<clang::TypedefType>(
          markClangTypeObjectDefinedByClass(typedef_type)));
  if (typedef_type == nullptr) {
    return VisitType(typedef_type, node);
  }

  bool res = true;
  clang::NestedNameSpecifier typedef_qualifier =
      markClangNestedNameSpecifierDefined(typedef_type->getQualifier());
  if (typedef_qualifier) {
    SgScopeStatement *scope = SageBuilder::topScopeStack();
    if (scope == nullptr) {
      scope = getGlobalScope();
    }
    if (SgNonrealType *written_type =
            buildNonrealTypeForNestedNameSpecifierType(
                typedef_type, scope, /*prefer_current_scope=*/false)) {
      *node = written_type;
      return VisitType(typedef_type, node) && res;
    }
  }

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

  return VisitType(typedef_type, node) && res;
}

bool ClangToSageTranslator::VisitTypeOfExprType(
    clang::TypeOfExprType *type_of_expr_type, SgNode **node) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToSageTranslator::TypeOfExprType" << std::endl;
#endif
  bool res = true;

  SgNode *tmp_expr = Traverse(type_of_expr_type->getUnderlyingExpr());

  // printf ("In VisitTypeOfExprType(): tmp_expr = %p = %s
  // \n",tmp_expr,tmp_expr->class_name().c_str());

  SgExpression *expr = isSgExpression(tmp_expr);
  SgType *type = SageBuilder::buildTypeOfType(expr, NULL);

  *node = type;

  return VisitType(type_of_expr_type, node) && res;
}

bool ClangToSageTranslator::VisitDependentTypeOfExprType(
    clang::DependentTypeOfExprType *dependent_type_of_expr_type,
    SgNode **node) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToSageTranslator::DependentTypeOfExprType" << std::endl;
#endif
  bool res = true;

  return VisitTypeOfExprType(dependent_type_of_expr_type, node) && res;
}

bool ClangToSageTranslator::VisitTypeOfType(clang::TypeOfType *type_of_type,
                                            SgNode **node) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToSageTranslator::TypeOfType" << std::endl;
#endif
  bool res = true;

  // getUnderlyingType() was renamed to getUnmodifiedType()
  SgType *underlyinigType =
      buildTypeFromQualifiedType(type_of_type->getUnmodifiedType());

  SgType *type = SageBuilder::buildTypeOfType(NULL, underlyinigType);

  *node = type;

  return VisitType(type_of_type, node) && res;
}

bool ClangToSageTranslator::VisitTypeWithKeyword(
    clang::TypeWithKeyword *type_with_keyword, SgNode **node) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToSageTranslator::VisitTypeWithKeyword" << std::endl;
#endif
  bool res = true;

  if (*node == nullptr) {
    clang::QualType desugared;
    if (desugared.isNull()) {
      clang::QualType qt(type_with_keyword, 0);
      desugared = qt.getCanonicalType();
    }

    if (!desugared.isNull() && desugared.getTypePtr() != type_with_keyword) {
      *node = buildTypeFromQualifiedType(desugared);
    } else {
      failExactTypeTranslation("type-with-keyword", type_with_keyword);
    }
  }

  return VisitType(type_with_keyword, node) && res;
}

bool ClangToSageTranslator::VisitDependentNameType(
    clang::DependentNameType *dependent_name_type, SgNode **node) {
  bool res = true;

  const clang::IdentifierInfo *id = dependent_name_type->getIdentifier();
  ROSE_ASSERT(id != nullptr);

  SgScopeStatement *base_scope = SageBuilder::topScopeStack();
  ROSE_ASSERT(base_scope != nullptr);
  *node = buildSemanticNonrealTypeFromNestedNameSpecifier(
      dependent_name_type->getQualifier(), base_scope,
      SgName(id->getName().str()), nullptr, nullptr);
  if (SgNonrealType *nrtype = isSgNonrealType(*node)) {
    if (SgNonrealDecl *nrdecl = isSgNonrealDecl(nrtype->get_declaration())) {
      nrdecl->set_suppress_typename(false);
    }
  }

  return VisitTypeWithKeyword(dependent_name_type, node) && res;
}

bool ClangToSageTranslator::VisitUnaryTransformType(
    clang::UnaryTransformType *unary_transform_type, SgNode **node) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToSageTranslator::UnaryTransformType" << std::endl;
#endif
  bool res = true;

  // UnaryTransformType is sugar that wraps another type (e.g.,
  // __underlying_type). Desugar it so downstream consumers see the real
  // underlying type instead of an unknown placeholder.
  clang::QualType underlying = unary_transform_type->desugar();
  if (underlying.isNull()) {
    underlying = unary_transform_type->getUnderlyingType();
  }
  if (underlying.isNull()) {
    underlying = unary_transform_type->getBaseType();
  }

  if (underlying.isNull()) {
    failExactTypeTranslation("unary-transform-underlying",
                             unary_transform_type);
  }
  *node = buildTypeFromQualifiedType(underlying);

  return VisitType(unary_transform_type, node) && res;
}

// DependentUnaryTransformType was removed/renamed in LLVM
/*
bool
ClangToSageTranslator::VisitDependentUnaryTransformType(clang::DependentUnaryTransformType
* dependent_unary_transform_type, SgNode ** node) { #if DEBUG_VISIT_TYPE
    std::cerr << "ClangToSageTranslator::DependentUnaryTransformType" <<
std::endl; #endif bool res = true;

    ROSE_ASSERT(FAIL_FIXME == 0); // FIXME

    return VisitUnaryTransformType(dependent_unary_transform_type, node) && res;
}
*/

bool ClangToSageTranslator::VisitUnresolvedUsingType(
    clang::UnresolvedUsingType *unresolved_using_type, SgNode **node) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToSageTranslator::UnresolvedUsingType" << std::endl;
#endif
  const clang::UnresolvedUsingType *defined_type =
      llvm::dyn_cast_or_null<clang::UnresolvedUsingType>(
          markClangTypeObjectDefinedByClass(unresolved_using_type));
  if (defined_type == nullptr || node == nullptr) {
    failExactTypeTranslation("unresolved-using-type", defined_type);
  }

  const clang::UnresolvedUsingTypenameDecl *declaration =
      llvm::dyn_cast_or_null<clang::UnresolvedUsingTypenameDecl>(
          markClangDeclObjectDefinedByKind(defined_type->getDecl()));
  clang::NestedNameSpecifier qualifier =
      declaration != nullptr
          ? markClangNestedNameSpecifierDefined(declaration->getQualifier())
          : std::nullopt;
  clang::DeclContext *declaration_context =
      declaration != nullptr
          ? markClangDeclContextObjectDefined(
                const_cast<clang::DeclContext *>(declaration->getDeclContext()))
          : nullptr;
  SgScopeStatement *semantic_scope =
      declaration_context != nullptr
          ? resolveScopeFromDeclContext(declaration_context)
          : nullptr;
  const std::string name =
      declaration != nullptr ? declaration->getNameAsString() : std::string();
  if (declaration == nullptr || !qualifier || semantic_scope == nullptr ||
      name.empty()) {
    failExactTypeTranslation("unresolved-using-type", defined_type);
  }

  *node = requireExactType(
      buildSemanticNonrealTypeFromNestedNameSpecifier(
          qualifier, semantic_scope, SgName(name), nullptr, nullptr),
      "unresolved-using-type", defined_type);
  return VisitType(unresolved_using_type, node);
}

bool ClangToSageTranslator::VisitVectorType(clang::VectorType *vector_type,
                                            SgNode **node) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToSageTranslator::VisitVectorType" << std::endl;
#endif

  SgType *type = buildTypeFromQualifiedType(vector_type->getElementType());

  SgModifierType *modified_type = new SgModifierType(type);
  SgTypeModifier &sg_modifer = modified_type->get_typeModifier();

  sg_modifer.setVectorType();
  sg_modifer.set_vector_size(vector_type->getNumElements());

  *node = SgModifierType::insertModifierTypeIntoTypeTable(modified_type);

  return VisitType(vector_type, node);
}

bool ClangToSageTranslator::VisitExtVectorType(
    clang::ExtVectorType *ext_vector_type, SgNode **node) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToSageTranslator::VisitExtVectorType" << std::endl;
#endif
  bool res = true;

  return VisitVectorType(ext_vector_type, node) && res;
}

bool ClangToSageTranslator::VisitUsingType(clang::UsingType *using_type,
                                           SgNode **node) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToSageTranslator::VisitUsingType" << std::endl;
#endif
  bool res = true;

  if (using_type->getQualifier()) {
    SgScopeStatement *scope = SageBuilder::topScopeStack();
    if (scope == nullptr) {
      scope = getGlobalScope();
    }
    if (SgNonrealType *written_type =
            buildNonrealTypeForNestedNameSpecifierType(
                using_type, scope, /*prefer_current_scope=*/false)) {
      *node = written_type;
      return VisitType(using_type, node) && res;
    }
  }

  // ROOT CAUSE FIX: UsingType is a type alias from a using declaration
  // Desugar it to get the underlying type
  clang::QualType underlying = using_type->desugar();
  *node = buildTypeFromQualifiedType(underlying);

  return res;
}
