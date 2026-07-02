#include "clang-frontend-private.hpp"
#include "clang-nns-utils.hpp"

#include "sage3basic.h"

#include "sageInterface.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include <clang/AST/APValue.h>
#include <clang/AST/TemplateBase.h>

#include <clang/Basic/AttrKinds.h>
#include <clang/Basic/OperatorKinds.h>
#include <clang/Lex/Lexer.h>

#include <llvm/ADT/SmallPtrSet.h>
#include <llvm/ADT/SmallString.h>

#include <functional>
#include <unordered_set>
namespace {
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
  static std::unordered_set<const clang::NamedDecl *> marked_names;
  if (!marked_names.insert(decl).second) {
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
  static std::unordered_set<const clang::CXXRecordDecl *> marked_records;
  if (!marked_records.insert(record).second) {
    return record;
  }

  if (const clang::ClassTemplateDecl *described_template =
          readClangApiValueDefined(
              [&]() { return record->getDescribedClassTemplate(); })) {
    markClangDeclObjectDefinedByKind(described_template);
    const clang::TemplateParameterList *params = readClangApiValueDefined(
        [&]() { return described_template->getTemplateParameters(); });
    markClangTemplateParameterListDefined(params);
    markClangTemplateParameterInjectedArgumentsForPrintingDefined(
        params, record->getASTContext());
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
  static std::unordered_set<const clang::ClassTemplateDecl *> marked_templates;
  if (!marked_templates.insert(decl).second) {
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
      markClangTemplateParameterInjectedArgumentsForPrintingDefined(
          params, templated_record->getASTContext());
    }
  }
#endif
  return decl;
}

template <typename T> static T *markClangSpecificDeclDefined(T *decl) {
  return const_cast<T *>(
      llvm::dyn_cast_or_null<T>(markClangDeclObjectDefinedByKind(decl)));
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

  switch (arg.getKind()) {
  case clang::TemplateArgument::Type:
    markClangQualTypeDefined(arg.getAsType());
    break;
  case clang::TemplateArgument::Declaration:
    markClangDeclObjectDefinedByKind(arg.getAsDecl());
    markClangQualTypeDefined(arg.getParamTypeForDecl());
    break;
  case clang::TemplateArgument::NullPtr:
    markClangQualTypeDefined(arg.getNullPtrType());
    break;
  case clang::TemplateArgument::Integral:
    markClangQualTypeDefined(arg.getIntegralType());
    break;
  case clang::TemplateArgument::StructuralValue:
    markClangLocalObjectDefined(&arg.getAsStructuralValue());
    markClangQualTypeDefined(arg.getStructuralValueType());
    break;
  case clang::TemplateArgument::Template:
  case clang::TemplateArgument::TemplateExpansion:
    markClangTemplateNameDefined(arg.getAsTemplateOrTemplatePattern());
    break;
  case clang::TemplateArgument::Expression:
    markClangExprObjectDefinedByClass(
        readClangApiValueDefined([&]() { return arg.getAsExpr(); }));
    break;
  case clang::TemplateArgument::Pack:
    (void)markClangTemplateArgumentArrayDefined(arg.pack_elements());
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
  (void)markClangTemplateArgumentArrayDefined(args->asArray());
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
    markClangTemplateArgumentDefined(arg_loc.getArgument());
    if (clang::TypeSourceInfo *type_info = arg_loc.getTypeSourceInfo()) {
      markClangAstObjectDefined(type_info);
      markClangTypeLocDataDefined(type_info->getTypeLoc());
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
  static std::unordered_set<const clang::ClassTemplateSpecializationDecl *>
      marked_specializations;
  static std::unordered_set<const clang::ClassTemplateSpecializationDecl *>
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
  if (!marked_specializations.insert(decl).second) {
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

  static std::unordered_set<const clang::Type *> marked_printing_types;
  type = markClangTypeObjectDefinedByClass(type);
  if (type == nullptr || !seen.insert(type).second ||
      !marked_printing_types.insert(type).second) {
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
            markClangTemplateParameterInjectedArgumentsForPrintingDefined(
                params, record->getASTContext());
          }

          if (auto *specialization =
                  llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(
                      record)) {
            markClangClassTemplateSpecializationForPrintingDefined(
                specialization);
          } else if (const clang::ClassTemplateDecl *described_template =
                         record->getDescribedClassTemplate()) {
            markClangDeclObjectDefinedByKind(described_template);
            markClangTemplateParameterInjectedArgumentsForPrintingDefined(
                described_template->getTemplateParameters(),
                record->getASTContext());
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

SgFunctionType *buildFunctionTypeForClangProto(
    SgType *return_type, SgFunctionParameterTypeList *param_type_list,
    const clang::FunctionProtoType *proto_type, SgType *class_type = nullptr) {
  const unsigned int mfunc_specifier =
      roseMemberFunctionSpecifierFromClangProto(proto_type);
  SgFunctionType *function_type =
      mfunc_specifier == 0
          ? SageBuilder::buildFunctionType(return_type, param_type_list)
          : SageBuilder::buildMemberFunctionType(return_type, param_type_list,
                                                 class_type, mfunc_specifier);
  ROSE_ASSERT(function_type != nullptr);
  if (proto_type != nullptr && proto_type->isVariadic()) {
    function_type->set_has_ellipses(1);
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

std::string
nestedNameSpecifierLocToString(clang::NestedNameSpecifierLoc qualifier_loc,
                               clang::CompilerInstance *compiler_instance) {
  if (!qualifier_loc) {
    return "";
  }
  clang::NestedNameSpecifier qualifier = qualifier_loc.getNestedNameSpecifier();
  if (!qualifier) {
    return "";
  }

  std::string result;
  llvm::raw_string_ostream stream(result);
  clang::PrintingPolicy policy =
      compiler_instance != nullptr
          ? compiler_instance->getASTContext().getPrintingPolicy()
          : clang::PrintingPolicy(clang::LangOptions());
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

bool typeSpellsElaboratedKeyword(const clang::Type *type) {
  type = markClangTypeObjectDefinedByClass(type);
  if (const auto *template_spec =
          llvm::dyn_cast_or_null<clang::TemplateSpecializationType>(type)) {
    return template_spec->getKeyword() != clang::ElaboratedTypeKeyword::None;
  }
  if (const auto *dependent_name =
          llvm::dyn_cast_or_null<clang::DependentNameType>(type)) {
    return dependent_name->getKeyword() != clang::ElaboratedTypeKeyword::None;
  }
  if (const auto *typedef_type =
          llvm::dyn_cast_or_null<clang::TypedefType>(type)) {
    return typedef_type->getKeyword() != clang::ElaboratedTypeKeyword::None;
  }
  if (const auto *using_type = llvm::dyn_cast_or_null<clang::UsingType>(type)) {
    return using_type->getKeyword() != clang::ElaboratedTypeKeyword::None;
  }
  if (const auto *unresolved_using =
          llvm::dyn_cast_or_null<clang::UnresolvedUsingType>(type)) {
    return unresolved_using->getKeyword() != clang::ElaboratedTypeKeyword::None;
  }
  if (const auto *tag_type = llvm::dyn_cast_or_null<clang::TagType>(type)) {
    return tag_type->getKeyword() != clang::ElaboratedTypeKeyword::None;
  }
  if (const auto *injected_class_name =
          llvm::dyn_cast_or_null<clang::InjectedClassNameType>(type)) {
    return injected_class_name->getKeyword() !=
           clang::ElaboratedTypeKeyword::None;
  }
  return false;
}

bool typeLocSpellsElaboratedKeyword(clang::TypeLoc type_loc) {
  markClangTypeLocDataDefined(type_loc);
  for (clang::TypeLoc current = type_loc; !current.isNull();
       current = current.getNextTypeLoc()) {
    markClangTypeLocDataDefined(current);
    markClangTypeObjectDefinedByClass(current.getTypePtr());
    if (auto tag_loc = current.getAs<clang::TagTypeLoc>()) {
      if (tag_loc.getElaboratedKeywordLoc().isValid()) {
        return true;
      }
    }
    if (auto typedef_loc = current.getAs<clang::TypedefTypeLoc>()) {
      if (typedef_loc.getElaboratedKeywordLoc().isValid()) {
        return true;
      }
    }
    if (auto using_loc = current.getAs<clang::UsingTypeLoc>()) {
      if (using_loc.getElaboratedKeywordLoc().isValid()) {
        return true;
      }
    }
    if (auto unresolved_using_loc =
            current.getAs<clang::UnresolvedUsingTypeLoc>()) {
      if (unresolved_using_loc.getElaboratedKeywordLoc().isValid()) {
        return true;
      }
    }
    if (typeSpellsElaboratedKeyword(current.getTypePtr())) {
      return true;
    }
  }
  return false;
}

clang::NestedNameSpecifierLoc typeLocQualifierLoc(clang::TypeLoc type_loc) {
  markClangTypeLocDataDefined(type_loc);
  for (clang::TypeLoc current = type_loc; !current.isNull();
       current = current.getNextTypeLoc()) {
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
      const clang::TagType *tag_type = llvm::dyn_cast_or_null<clang::TagType>(
          markClangTypeObjectDefinedByClass(tag_loc.getTypePtr()));
      clang::NestedNameSpecifier qualifier =
          tag_type != nullptr ? readClangApiValueDefined(
                                    [&]() { return tag_type->getQualifier(); })
                              : clang::NestedNameSpecifier();
      markClangTagTypeQualifierStorageDefined(tag_type, qualifier);
      markClangNestedNameSpecifierDefined(qualifier);
      if (clang::NestedNameSpecifierLoc qualifier_loc =
              tag_loc.getQualifierLoc()) {
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

std::string resolveTemplateParameterNameFromDeclContext(
    const clang::DeclContext *start_context, unsigned depth, unsigned index) {
  start_context = markClangDeclContextObjectDefined(start_context);
  if (start_context == nullptr) {
    return "";
  }

  std::vector<const clang::TemplateParameterList *> template_levels =
      collectTemplateParameterLevelsFromDeclContext(start_context);

  if (depth >= template_levels.size()) {
    return "";
  }
  const clang::TemplateParameterList *params = template_levels[depth];
  params = markClangTemplateParameterListDefined(params);
  if (params == nullptr ||
      index >= readClangApiValueDefined([&]() { return params->size(); })) {
    return "";
  }

  const clang::NamedDecl *named_param =
      llvm::dyn_cast_or_null<clang::NamedDecl>(markClangDeclObjectDefinedByKind(
          readClangApiValueDefined([&]() { return params->getParam(index); })));
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

void suppressFrontendOnlyNode(SgLocatedNode *node) {
  if (node == nullptr) {
    return;
  }

  auto mark = [](Sg_File_Info *fi) {
    if (fi == nullptr) {
      return;
    }
    fi->setFrontendSpecific();
    fi->unsetOutputInCodeGeneration();
  };

  mark(node->get_file_info());
  mark(node->get_startOfConstruct());
  mark(node->get_endOfConstruct());
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
  (void)ast_context;
  if (injected_class_name_type == nullptr) {
    return {};
  }

  injected_class_name_type = static_cast<const clang::InjectedClassNameType *>(
      markClangTypeObjectDefinedByClass(injected_class_name_type));
  clang::QualType canonical_type =
      injected_class_name_type->getCanonicalTypeInternal();
  markClangValueDefined(canonical_type);
  markClangTypeObjectDefinedByClass(canonical_type.getTypePtrOrNull());
  return canonical_type;
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
    clang::NestedNameSpecifier qualifier = qtn->getQualifier();
    if (qualifier && !base.empty()) {
      std::string qualifier_str = qualifierToString(qualifier);
      if (!qualifier_str.empty()) {
        return appendQualifier(qualifier_str, base);
      }
    }
  }

  if (const clang::DependentTemplateName *dtn =
          marked_name.getAsDependentTemplateName()) {
    std::string base = getTemplateNameBase(marked_name);
    clang::NestedNameSpecifier qualifier = dtn->getQualifier();
    if (qualifier && !base.empty()) {
      std::string qualifier_str = qualifierToString(qualifier);
      if (!qualifier_str.empty()) {
        return appendQualifier(qualifier_str, base);
      }
    }
  }

  // Fallback: just use the template name without qualification
  std::string result;
  llvm::raw_string_ostream stream(result);
  clang::LangOptions opts;
  clang::PrintingPolicy policy(opts);
#if ROSE_USE_VALGRIND
  if (clangFrontendRunningOnValgrind()) {
    VALGRIND_DISABLE_ERROR_REPORTING;
  }
#endif
  marked_name.print(stream, policy);
#if ROSE_USE_VALGRIND
  if (clangFrontendRunningOnValgrind()) {
    VALGRIND_ENABLE_ERROR_REPORTING;
  }
#endif
  stream.flush();
  return result;
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
      return "__template_template_param_" + std::to_string(parm->getIndex());
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
      return "__template_template_param_" + std::to_string(parm->getIndex());
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

bool hasTemplateSyntaxComponent(const std::string &name) {
  return name.find('<') != std::string::npos &&
         name.find('>') != std::string::npos;
}

std::string stripTemplateArgs(const std::string &name) {
  std::string trimmed = trimWhitespace(name);
  size_t lt = trimmed.find('<');
  if (lt == std::string::npos) {
    return trimmed;
  }
  return trimWhitespace(trimmed.substr(0, lt));
}

std::string normalizeTemplateDeclCacheKey(const std::string &name) {
  std::vector<std::string> components =
      splitQualifiedNameOutsideTemplates(name);
  if (components.empty()) {
    return name;
  }
  std::string result;
  for (size_t i = 0; i < components.size(); ++i) {
    std::string clean = stripTemplateArgs(components[i]);
    if (clean.empty()) {
      continue;
    }
    if (!result.empty()) {
      result += "::";
    }
    result += clean;
  }
  return result.empty() ? name : result;
}

void appendTemplateInstantiationArg(std::string &result, bool &need_separator,
                                    const clang::TemplateArgument &arg) {
  const clang::TemplateArgument &defined_arg =
      markClangTemplateArgumentForPrintingDefined(arg);
  auto append_single_arg = [&](const clang::TemplateArgument &single_arg,
                               bool force_pack_ellipsis) {
    const clang::TemplateArgument &defined_single_arg =
        markClangTemplateArgumentForPrintingDefined(single_arg);
    if (need_separator) {
      result += " , ";
    }
    need_separator = true;

    std::string arg_str;
    llvm::raw_string_ostream arg_stream(arg_str);
#if ROSE_USE_VALGRIND
    if (clangFrontendRunningOnValgrind()) {
      VALGRIND_DISABLE_ERROR_REPORTING;
      defined_single_arg.print(clang::PrintingPolicy(clang::LangOptions()),
                               arg_stream, true);
      VALGRIND_ENABLE_ERROR_REPORTING;
    } else
#endif
    {
      defined_single_arg.print(clang::PrintingPolicy(clang::LangOptions()),
                               arg_stream, true);
    }
    arg_stream.flush();
    markClangPrintedStringDefined(arg_str);

    arg_str = trimWhitespace(arg_str);
    bool needs_pack_ellipsis =
        force_pack_ellipsis ||
        templateArgumentNeedsPackEllipsis(defined_single_arg);
    if (needs_pack_ellipsis &&
        (arg_str.size() < 3 ||
         arg_str.compare(arg_str.size() - 3, 3, "...") != 0)) {
      arg_str += "...";
    }

    result += arg_str;
  };

  if (defined_arg.getKind() == clang::TemplateArgument::Pack) {
    auto elements =
        markClangTemplateArgumentArrayDefined(defined_arg.pack_elements());
    const bool force_pack_ellipsis =
        elements.size() == 1 &&
        (defined_arg.containsUnexpandedParameterPack() ||
         defined_arg.isInstantiationDependent() ||
         elements.front().isInstantiationDependent());
    for (const clang::TemplateArgument &pack_arg : elements) {
      append_single_arg(pack_arg, force_pack_ellipsis);
    }
    return;
  }

  append_single_arg(defined_arg, false);
}

std::string
buildTemplateInstantiationName(const std::string &base_name,
                               llvm::ArrayRef<clang::TemplateArgument> args) {
  args = markClangTemplateArgumentArrayDefined(args);
  if (args.empty())
    return base_name;

  std::string result = base_name;
  result += "<";
  bool need_separator = false;
  for (const clang::TemplateArgument &arg : args) {
    appendTemplateInstantiationArg(result, need_separator, arg);
  }
  result += ">";
  return result;
}

} // namespace

void markClangQualTypeForPrintingDefinedForFrontend(clang::QualType type) {
#if ROSE_USE_VALGRIND
  llvm::SmallPtrSet<const clang::Type *, 32> seen;
  markClangQualTypeForPrintingDefined(type, seen);
#else
  (void)type;
#endif
}

// Generate unique name for template instantiation
// Note: Must not contain < > characters for ROSE mangling
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

std::string ClangToSageTranslator::mangleTemplateInstantiation(
    const std::string &template_name,
    const clang::TemplateSpecializationType *spec_type) {
  spec_type = static_cast<const clang::TemplateSpecializationType *>(
      markClangTypeObjectDefinedByClass(spec_type));
  std::string safe_template_name = template_name;
  for (char &c : safe_template_name) {
    if (c == '<' || c == '>' || c == ',' || c == ' ' || c == ':' || c == '*' ||
        c == '&') {
      c = '_';
    }
  }
  std::string result = safe_template_name + "_";
  auto args =
      markClangTemplateArgumentArrayDefined(spec_type->template_arguments());
  bool first = true;
  for (const clang::TemplateArgument &arg : args) {
    const clang::TemplateArgument &defined_arg =
        markClangTemplateArgumentForPrintingDefined(arg);
    if (!first)
      result += "_";
    first = false;

    std::string arg_str;
    llvm::raw_string_ostream arg_stream(arg_str);
#if ROSE_USE_VALGRIND
    if (clangFrontendRunningOnValgrind()) {
      VALGRIND_DISABLE_ERROR_REPORTING;
      defined_arg.print(clang::PrintingPolicy(clang::LangOptions()), arg_stream,
                        true);
      VALGRIND_ENABLE_ERROR_REPORTING;
    } else
#endif
    {
      defined_arg.print(clang::PrintingPolicy(clang::LangOptions()), arg_stream,
                        true);
    }
    arg_stream.flush();
    markClangPrintedStringDefined(arg_str);

    // Replace special characters that can't be in mangled names
    for (char &c : arg_str) {
      if (c == '<' || c == '>' || c == ',' || c == ' ' || c == ':' ||
          c == '*' || c == '&') {
        c = '_';
      }
    }
    result += arg_str;
  }
  return result;
}

std::string ClangToSageTranslator::mangleTemplateInstantiation(
    const std::string &template_name, const clang::TemplateArgumentList &args) {
  std::string safe_template_name = template_name;
  for (char &c : safe_template_name) {
    if (c == '<' || c == '>' || c == ',' || c == ' ' || c == ':' || c == '*' ||
        c == '&') {
      c = '_';
    }
  }
  std::string result = safe_template_name + "_";
  bool first = true;
  const clang::TemplateArgumentList *defined_args =
      markClangTemplateArgumentListDefined(&args);
  for (unsigned i = 0; i < defined_args->size(); ++i) {
    const clang::TemplateArgument &arg =
        markClangTemplateArgumentForPrintingDefined(defined_args->get(i));
    if (!first)
      result += "_";
    first = false;

    std::string arg_str;
    llvm::raw_string_ostream arg_stream(arg_str);
#if ROSE_USE_VALGRIND
    if (clangFrontendRunningOnValgrind()) {
      VALGRIND_DISABLE_ERROR_REPORTING;
      arg.print(clang::PrintingPolicy(clang::LangOptions()), arg_stream, true);
      VALGRIND_ENABLE_ERROR_REPORTING;
    } else
#endif
    {
      arg.print(clang::PrintingPolicy(clang::LangOptions()), arg_stream, true);
    }
    arg_stream.flush();
    markClangPrintedStringDefined(arg_str);

    for (char &c : arg_str) {
      if (c == '<' || c == '>' || c == ',' || c == ' ' || c == ':' ||
          c == '*' || c == '&') {
        c = '_';
      }
    }
    result += arg_str;
  }
  return result;
}

namespace {

void suppress_unparse_output(SgLocatedNode *n) {
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

void diagnose_null_scope(SgDeclarationStatement *decl, const char *context) {
  if (decl == NULL || decl->get_scope() != NULL)
    return;
  MLOG_WARN_C(MLOG_FRONTEND,
              "Declaration %s (%p) created with NULL scope in %s\n",
              decl->class_name().c_str(), decl, context);
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

SgScopeStatement *ClangToSageTranslator::getOpaqueTypeInsertionScope(
    SgScopeStatement *scope) const {
  while (scope != nullptr) {
    if (scope->containsOnlyDeclarations() || isSgBasicBlock(scope)) {
      return scope;
    }
    scope = SageInterface::getEnclosingScope(scope, false);
  }
  return nullptr;
}

SgScopeStatement *
ClangToSageTranslator::getSafeOpaqueTypeInsertionScope() const {
  SgScopeStatement *scope =
      getOpaqueTypeInsertionScope(SageBuilder::topScopeStack());
  if (scope == nullptr) {
    scope = getGlobalScope();
  }
  return scope;
}

bool ClangToSageTranslator::scopeReachableFromCurrentFile(
    SgScopeStatement *candidate) {
  if (candidate == nullptr || p_sage_source_file == nullptr) {
    return false;
  }

  SgGlobal *file_global = p_sage_source_file->get_globalScope();
  if (file_global == nullptr) {
    return false;
  }

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

    if (SgScopeStatement *parent_scope = isSgScopeStatement(parent)) {
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
  if (reachable_scope == nullptr && p_sage_source_file != nullptr) {
    reachable_scope = p_sage_source_file->get_globalScope();
  }
  if (reachable_scope == nullptr) {
    return nullptr;
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

    SgName ns_name(
        readClangApiValueDefined([&]() { return ns_decl->getNameAsString(); }));
    SgNamespaceDeclarationStatement *ns_stmt = nullptr;
    if (SgNamespaceSymbol *ns_symbol =
            reachable_scope->lookup_namespace_symbol(ns_name)) {
      ns_stmt = ns_symbol->get_declaration();
    }

    if (ns_stmt == nullptr || ns_stmt->get_definition() == nullptr ||
        !scopeReachableFromCurrentFile(ns_stmt->get_definition())) {
      ns_stmt = SageBuilder::buildNamespaceDeclaration_nfi(ns_name, false,
                                                           reachable_scope);
      if (ns_stmt != nullptr) {
        setCompilerGeneratedFileInfo(ns_stmt);
        if (SgNamespaceDefinitionStatement *ns_def =
                ns_stmt->get_definition()) {
          setCompilerGeneratedFileInfo(ns_def);
        }
      }
    }

    if (ns_stmt == nullptr || ns_stmt->get_definition() == nullptr) {
      return nullptr;
    }

    ensureDeclInScopeChildList(ns_stmt, reachable_scope,
                               "resolveReachableNamespaceScope");
    suppressFrontendOnlyNode(ns_stmt);
    suppressFrontendOnlyNode(ns_stmt->get_definition());

    reachable_scope = ns_stmt->get_definition();
  }

  return reachable_scope;
}

SgType *ClangToSageTranslator::buildTypeFromQualifiedType(
    const clang::QualType &qual_type) {
  markClangValueDefined(qual_type);
  markClangTypeObjectDefinedByClass(qual_type.getTypePtrOrNull());
  const bool cache_translation = canCacheQualifiedTypeTranslation(qual_type);
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
  SgType *type = isSgType(tmp_type);

  ROSE_ASSERT(type != NULL);

  // Always prefer the exact translated specialization declaration for
  // class-template specialization types. The type visitor can otherwise reuse
  // a same-named template from the wrong scope before the precise
  // ClassTemplateSpecializationDecl has been materialized.
  clang::QualType canonical = qual_type.getCanonicalType();
  markClangValueDefined(canonical);
  if (!canonical.isNull()) {
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
          type = inst_type;
        }
      }
    }
  }

  if (qual_type.hasLocalQualifiers()) {
    SgModifierType *modified_type = new SgModifierType(type);
    SgTypeModifier &sg_modifer = modified_type->get_typeModifier();
    clang::Qualifiers qualifier = qual_type.getLocalQualifiers();

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
      p_qualified_type_translation_map[cache_key] = modified_type;
    }

    return modified_type;
  } else {
    return type;
  }
}

SgType *
ClangToSageTranslator::buildTypeFromTypeLoc(const clang::TypeLoc &type_loc) {
  markClangTypeLocDataDefined(type_loc);
  if (type_loc.isNull()) {
    return nullptr;
  }
  markClangTypeObjectDefinedByClass(type_loc.getTypePtr());

  auto resolve_scope = [&]() -> SgScopeStatement * {
    SgScopeStatement *scope = SageBuilder::topScopeStack();
    if (scope == nullptr) {
      scope = getGlobalScope();
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

  auto recover_missing_pointee_qualifiers =
      [&](SgType *pointee_type, const clang::QualType &written_pointee_type,
          const clang::QualType &semantic_pointee_type) -> SgType * {
    if (pointee_type == nullptr) {
      return nullptr;
    }
    if (!written_pointee_type.isNull() &&
        written_pointee_type.hasLocalQualifiers()) {
      return pointee_type;
    }
    if (!semantic_pointee_type.isNull() &&
        semantic_pointee_type.hasLocalQualifiers()) {
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
              args.push_back(new SgTemplateArgument(expr, false));
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
        SgTemplateInstantiationDecl *inst_decl =
            translated_instantiation_decl_from_type(resolved_type);
        if (inst_decl == nullptr ||
            !inst_decl->get_templateArguments().empty()) {
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

        inst_decl->get_templateArguments() = written_args;
        if (inst_decl->get_deducedTemplateArguments().empty()) {
          inst_decl->get_deducedTemplateArguments() = written_args;
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

  auto build_translated_typedef_type_from_decl =
      [&](clang::TypedefNameDecl *typedef_decl) -> SgType * {
    if (typedef_decl == nullptr) {
      return nullptr;
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
      repairTypedefDeclarationReferenceShared(sg_typedef_decl);
      return sg_typedef_decl->get_type();
    }

    if (SgTemplateTypedefDeclaration *sg_template_typedef =
            isSgTemplateTypedefDeclaration(translated_decl)) {
      return sg_template_typedef->get_type();
    }

    return nullptr;
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
        std::string param_name =
            normalizeTemplateTypeParamName(type_param->getNameAsString());
        param_name = preferHigherQualityTemplateParamName(
            param_name, resolveTemplateParameterNameFromDeclContext(
                            record_context, type_param->getDepth(),
                            type_param->getIndex()));
        if (param_name.empty()) {
          param_name = "__template_type_param_" + std::to_string(i);
        }

        SgTemplateType *param_type =
            SageBuilder::buildTemplateType(SgName(param_name));
        if (type_param->isParameterPack()) {
          param_type->set_packed(true);
        }
        args.push_back(new SgTemplateArgument(param_type, false));
        continue;
      }

      if (const auto *non_type_param =
              llvm::dyn_cast<clang::NonTypeTemplateParmDecl>(param_decl)) {
        std::string param_name = non_type_param->getNameAsString();
        if (param_name.empty()) {
          param_name = "__template_non_type_param_" + std::to_string(i);
        }

        SgType *param_type = nullptr;
        if (const clang::TypeSourceInfo *type_info =
                non_type_param->getTypeSourceInfo()) {
          param_type = buildTypeFromTypeLoc(type_info->getTypeLoc());
        }
        if (param_type == nullptr) {
          param_type = buildTypeFromQualifiedType(non_type_param->getType());
        }
        if (param_type == nullptr) {
          continue;
        }

        SgVarRefExp *arg_expr =
            SageBuilder::buildOpaqueVarRefExp(param_name, resolve_scope());
        if (arg_expr == nullptr) {
          continue;
        }

        SgTemplateArgument *arg = new SgTemplateArgument(
            SgTemplateArgument::nontype_argument,
            /*isArrayBoundUnknownType=*/false, param_type, arg_expr,
            /*templateDeclaration=*/nullptr, /*explicitlySpecified=*/false);
        arg_expr->set_parent(arg);
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
          args.push_back(new SgTemplateArgument(
              SgTemplateArgument::template_template_argument,
              /*isArrayBoundUnknownType=*/false, /*type=*/nullptr,
              /*expression=*/nullptr,
              /*templateDeclaration=*/template_arg_decl,
              /*explicitlySpecified=*/false));
        }
      }
    }

    ensureTemplateArgumentParents(args);
    return args;
  };

  auto build_unqualified_dependent_alias_nonreal =
      [&](const clang::NamedDecl *alias_decl,
          clang::ElaboratedTypeKeyword keyword) -> SgType * {
    if (alias_decl == nullptr) {
      return nullptr;
    }

    alias_decl = llvm::dyn_cast_or_null<clang::NamedDecl>(
        markClangDeclObjectDefinedByKind(alias_decl));
    const clang::DeclContext *alias_context =
        markClangDeclContextObjectDefined(readClangApiValueDefined(
            [&]() { return alias_decl->getDeclContext(); }));
    const bool alias_context_has_template_parameters =
        templateParametersForDeclContext(alias_context) != nullptr;
    const bool alias_context_dependent =
        alias_context != nullptr && readClangApiValueDefined([&]() {
          return alias_context->isDependentContext();
        });
    if (alias_context == nullptr ||
        (!alias_context_dependent && !alias_context_has_template_parameters)) {
      return nullptr;
    }

    std::string alias_name = readClangApiValueDefined(
        [&]() { return alias_decl->getNameAsString(); });
    if (alias_name.empty()) {
      return nullptr;
    }

    SgScopeStatement *current_scope = resolve_scope();
    SgScopeStatement *scope = resolveScopeFromDeclContext(
        const_cast<clang::DeclContext *>(alias_context), current_scope);
    auto scope_contains = [](SgScopeStatement *ancestor,
                             SgScopeStatement *candidate) -> bool {
      for (SgNode *node = candidate; node != nullptr;
           node = node->get_parent()) {
        if (node == ancestor) {
          return true;
        }
      }
      return false;
    };
    const bool use_nested_lookup_scope = current_scope != nullptr &&
                                         scope != nullptr &&
                                         scope_contains(scope, current_scope);

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
    auto enclosing_class_decl =
        [&](SgScopeStatement *candidate_scope) -> SgClassDeclaration * {
      for (SgNode *node = candidate_scope; node != nullptr;
           node = node->get_parent()) {
        if (SgClassDefinition *def = isSgClassDefinition(node)) {
          return canonical_sg_class_decl(def->get_declaration());
        }
        if (SgTemplateClassDefinition *def =
                isSgTemplateClassDefinition(node)) {
          return canonical_sg_class_decl(def->get_declaration());
        }
        if (SgTemplateInstantiationDefn *def =
                isSgTemplateInstantiationDefn(node)) {
          return canonical_sg_class_decl(def->get_declaration());
        }
      }
      return nullptr;
    };
    auto class_template_parameter_count =
        [](SgClassDeclaration *decl) -> size_t {
      if (decl == nullptr) {
        return 0;
      }
      if (SgTemplateClassDeclaration *tmpl =
              isSgTemplateClassDeclaration(decl)) {
        return tmpl->get_templateParameters().size();
      }
      if (SgTemplateInstantiationDecl *inst =
              isSgTemplateInstantiationDecl(decl)) {
        if (SgTemplateClassDeclaration *tmpl =
                isSgTemplateClassDeclaration(inst->get_templateDeclaration())) {
          return tmpl->get_templateParameters().size();
        }
      }
      return 0;
    };
    auto is_hidden_frontend_class_surface =
        [](SgClassDeclaration *decl) -> bool {
      if (decl == nullptr) {
        return false;
      }
      Sg_File_Info *fi = decl->get_file_info();
      return fi != nullptr && fi->isCompilerGenerated() &&
             fi->isFrontendSpecific();
    };
    auto class_declaration_scope =
        [](SgClassDeclaration *decl) -> SgScopeStatement * {
      if (decl == nullptr) {
        return nullptr;
      }
      if (SgScopeStatement *parent_scope =
              isSgScopeStatement(decl->get_parent())) {
        return parent_scope;
      }
      return decl->get_scope();
    };
    auto same_logical_scope = [](SgScopeStatement *lhs,
                                 SgScopeStatement *rhs) -> bool {
      if (lhs == nullptr || rhs == nullptr) {
        return false;
      }
      if (lhs == rhs || SgScopeStatement::isEquivalentScope(lhs, rhs)) {
        return true;
      }
      if (isSgGlobal(lhs) != nullptr && isSgGlobal(rhs) != nullptr) {
        return true;
      }

      SgNamespaceDefinitionStatement *lhs_namespace =
          isSgNamespaceDefinitionStatement(lhs);
      SgNamespaceDefinitionStatement *rhs_namespace =
          isSgNamespaceDefinitionStatement(rhs);
      if (lhs_namespace == nullptr || rhs_namespace == nullptr) {
        return false;
      }

      SgNamespaceDeclarationStatement *lhs_decl =
          lhs_namespace->get_namespaceDeclaration();
      SgNamespaceDeclarationStatement *rhs_decl =
          rhs_namespace->get_namespaceDeclaration();
      if (lhs_decl == nullptr || rhs_decl == nullptr) {
        return false;
      }

      SgDeclarationStatement *lhs_first =
          lhs_decl->get_firstNondefiningDeclaration();
      if (lhs_first == nullptr) {
        lhs_first = lhs_decl;
      }
      SgDeclarationStatement *rhs_first =
          rhs_decl->get_firstNondefiningDeclaration();
      if (rhs_first == nullptr) {
        rhs_first = rhs_decl;
      }
      return lhs_first == rhs_first;
    };
    auto same_logical_template_owner =
        [&](SgClassDeclaration *current, SgClassDeclaration *semantic) -> bool {
      current = canonical_sg_class_decl(current);
      semantic = canonical_sg_class_decl(semantic);
      if (current == nullptr || semantic == nullptr ||
          current->get_name() != semantic->get_name()) {
        return false;
      }

      const size_t current_count = class_template_parameter_count(current);
      const size_t semantic_count = class_template_parameter_count(semantic);
      if (current_count == 0 || current_count != semantic_count) {
        return false;
      }

      if (is_hidden_frontend_class_surface(current) ||
          is_hidden_frontend_class_surface(semantic)) {
        return true;
      }

      return same_logical_scope(class_declaration_scope(current),
                                class_declaration_scope(semantic));
    };
    auto same_clang_record_decl = [](const clang::CXXRecordDecl *lhs,
                                     const clang::CXXRecordDecl *rhs) -> bool {
      lhs = llvm::dyn_cast_or_null<clang::CXXRecordDecl>(
          markClangDeclObjectDefinedByKind(lhs));
      rhs = llvm::dyn_cast_or_null<clang::CXXRecordDecl>(
          markClangDeclObjectDefinedByKind(rhs));
      if (lhs == nullptr || rhs == nullptr) {
        return false;
      }
      const clang::CXXRecordDecl *lhs_canonical =
          llvm::dyn_cast_or_null<clang::CXXRecordDecl>(
              markClangDeclObjectDefinedByKind(readClangApiValueDefined(
                  [&]() { return lhs->getCanonicalDecl(); })));
      const clang::CXXRecordDecl *rhs_canonical =
          llvm::dyn_cast_or_null<clang::CXXRecordDecl>(
              markClangDeclObjectDefinedByKind(readClangApiValueDefined(
                  [&]() { return rhs->getCanonicalDecl(); })));
      return lhs_canonical != nullptr && lhs_canonical == rhs_canonical;
    };
    auto active_translation_context_is_in_alias_owner = [&]() -> bool {
      const clang::DeclContext *defined_alias_context =
          markClangDeclContextObjectDefined(alias_context);
      const auto *alias_record =
          llvm::dyn_cast_or_null<clang::CXXRecordDecl>(defined_alias_context);
      if (alias_record == nullptr) {
        return false;
      }

      for (auto it = p_template_parameter_decl_context_stack.rbegin();
           it != p_template_parameter_decl_context_stack.rend(); ++it) {
        for (const clang::DeclContext *ctx =
                 markClangDeclContextObjectDefined(*it);
             ctx != nullptr;
             ctx = markClangDeclContextObjectDefined(readClangApiValueDefined(
                 [&]() { return ctx->getParent(); }))) {
          if (ctx == defined_alias_context) {
            return true;
          }
          const auto *ctx_record = llvm::dyn_cast_or_null<clang::CXXRecordDecl>(
              markClangDeclContextObjectDefined(ctx));
          if (same_clang_record_decl(ctx_record, alias_record)) {
            return true;
          }
        }
      }

      return false;
    };

    auto build_explicit_enclosing_member_alias = [&]() -> SgNonrealType * {
      const clang::DeclContext *defined_alias_context =
          markClangDeclContextObjectDefined(alias_context);
      const auto *alias_record =
          llvm::dyn_cast_or_null<clang::CXXRecordDecl>(defined_alias_context);
      if (alias_record == nullptr) {
        return nullptr;
      }

      SgClassDeclaration *current_class = enclosing_class_decl(current_scope);
      SgClassDeclaration *alias_class = canonical_sg_class_decl(
          isSgClassDeclaration(lookupSgDeclarationForClangDecl(
              const_cast<clang::CXXRecordDecl *>(alias_record),
              /*allow_on_demand=*/true)));
      if (current_class == nullptr || alias_class == nullptr ||
          current_class == alias_class ||
          same_logical_template_owner(current_class, alias_class)) {
        return nullptr;
      }

      SgScopeStatement *chain_scope = getGlobalScope();
      if (chain_scope == nullptr) {
        chain_scope = current_scope != nullptr ? current_scope
                                               : SageBuilder::topScopeStack();
      }
      if (chain_scope == nullptr) {
        return nullptr;
      }

      std::vector<const clang::DeclContext *> contexts;
      for (const clang::DeclContext *dc = defined_alias_context;
           dc != nullptr &&
           !readClangApiValueDefined([&]() { return dc->isTranslationUnit(); });
           dc = markClangDeclContextObjectDefined(
               readClangApiValueDefined([&]() { return dc->getParent(); }))) {
        contexts.push_back(dc);
      }

      for (auto it = contexts.rbegin(); it != contexts.rend(); ++it) {
        if (const auto *ns = llvm::dyn_cast_or_null<clang::NamespaceDecl>(
                markClangDeclContextObjectDefined(*it))) {
          std::string ns_name =
              readClangApiValueDefined([&]() { return ns->getNameAsString(); });
          if (ns_name.empty()) {
            continue;
          }
          SgNonrealType *ns_type = SageBuilder::buildNonrealType(
              SgName(ns_name), chain_scope, nullptr);
          SgNonrealDecl *ns_decl = isSgNonrealDecl(
              ns_type != nullptr ? ns_type->get_declaration() : nullptr);
          if (ns_decl == nullptr) {
            return nullptr;
          }
          chain_scope = ns_decl->get_nonreal_decl_scope();
          continue;
        }

        const auto *record = llvm::dyn_cast_or_null<clang::CXXRecordDecl>(
            markClangDeclContextObjectDefined(*it));
        if (record == nullptr) {
          continue;
        }

        std::string record_name = readClangApiValueDefined(
            [&]() { return record->getNameAsString(); });
        if (record_name.empty()) {
          return nullptr;
        }

        SgTemplateArgumentPtrList record_args =
            build_current_instantiation_template_arguments(record);
        const SgTemplateArgumentPtrList *record_args_ptr =
            record_args.empty() ? nullptr : &record_args;

        SgNonrealType *record_type = SageBuilder::buildNonrealType(
            SgName(record_name), chain_scope, record_args_ptr);
        SgNonrealDecl *record_decl = isSgNonrealDecl(
            record_type != nullptr ? record_type->get_declaration() : nullptr);
        if (record_decl == nullptr) {
          return nullptr;
        }
        chain_scope = record_decl->get_nonreal_decl_scope();
      }

      SgNonrealType *qualified_alias_type = SageBuilder::buildNonrealType(
          SgName(alias_name), chain_scope, nullptr);
      SgNonrealDecl *qualified_alias_decl =
          isSgNonrealDecl(qualified_alias_type != nullptr
                              ? qualified_alias_type->get_declaration()
                              : nullptr);
      if (qualified_alias_decl == nullptr) {
        return nullptr;
      }

      // The enclosing current instantiation is now explicit, so this must stay
      // a qualified dependent type instead of being treated like a bare local
      // alias.
      qualified_alias_decl->set_suppress_typename(false);
      return qualified_alias_type;
    };

    if (use_nested_lookup_scope) {
      if (SgNonrealType *qualified_alias_type =
              build_explicit_enclosing_member_alias()) {
        return qualified_alias_type;
      }
    }

    if (use_nested_lookup_scope) {
      scope = current_scope;
    }
    if (scope == nullptr) {
      scope = current_scope;
    }
    if (scope == nullptr) {
      return nullptr;
    }

    SgNonrealType *alias_type =
        SageBuilder::buildNonrealType(SgName(alias_name), scope, nullptr);
    SgNonrealDecl *alias_nonreal_decl = isSgNonrealDecl(
        alias_type != nullptr ? alias_type->get_declaration() : nullptr);
    if (alias_nonreal_decl == nullptr) {
      return nullptr;
    }

    alias_nonreal_decl->set_suppress_typename(
        keyword != clang::ElaboratedTypeKeyword::Typename);

    SgDeclarationStatement *sg_decl = lookupSgDeclarationForClangDecl(
        const_cast<clang::NamedDecl *>(alias_decl), /*allow_on_demand=*/true);
    sg_decl = normalizeNonrealTemplateDeclarationTarget(sg_decl);
    if (sg_decl != nullptr && !use_nested_lookup_scope &&
        !active_translation_context_is_in_alias_owner()) {
      alias_nonreal_decl->set_templateDeclaration(sg_decl);
    }

    return alias_type;
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

  const bool enable_default_template_args = false;

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

    std::string text;
    if (begin.isValid() && end.isValid()) {
      text = trimWhitespace(getSourceText(clang::SourceRange(begin, end)));
    }

    if (text.empty()) {
      if (clang::ConceptReference *concept_ref =
              auto_loc.getConceptReference()) {
        clang::PrintingPolicy policy =
            p_compiler_instance != nullptr
                ? p_compiler_instance->getASTContext().getPrintingPolicy()
                : clang::PrintingPolicy(clang::LangOptions());
        llvm::raw_string_ostream os(text);
        concept_ref->print(os, policy);
        os.flush();
        text = trimWhitespace(text);
      }
    }

    if (text.empty()) {
      if (clang::NestedNameSpecifierLoc nested =
              auto_loc.getNestedNameSpecifierLoc()) {
        text += getSourceText(nested.getSourceRange());
      }
      if (const clang::NamedDecl *concept_decl = auto_loc.getNamedConcept()) {
        text += concept_decl->getNameAsString();
      }
      if (auto_loc.hasExplicitTemplateArgs() &&
          auto_loc.getLAngleLoc().isValid() &&
          auto_loc.getRAngleLoc().isValid()) {
        text += getSourceText(clang::SourceRange(auto_loc.getLAngleLoc(),
                                                 auto_loc.getRAngleLoc()));
      }
      text = trimWhitespace(text);
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
    if (auto_types.empty()) {
      return;
    }

    size_t auto_index = 0;
    for (clang::TypeLoc current_loc = type_loc;
         !current_loc.isNull() && auto_index < auto_types.size();
         current_loc = current_loc.getNextTypeLoc()) {
      if (clang::AutoTypeLoc auto_loc =
              current_loc.getAs<clang::AutoTypeLoc>()) {
        SageInterface::setAutoTypeConstraint(
            auto_types[auto_index], build_auto_type_constraint_text(auto_loc));
        ++auto_index;
      }
    }
  };

  auto finalize_spelled_type = [&](SgType *candidate) -> SgType * {
    candidate = apply_local_qualifiers(candidate, type_loc.getType());
    annotate_auto_type_constraints(candidate);
    return candidate;
  };

  auto detach_decl_from_scope_child_lists_for_spelled_type =
      [&](SgDeclarationStatement *decl) {
        if (decl == nullptr) {
          return;
        }

        auto erase_decl_from_scope = [&](SgScopeStatement *scope) {
          if (scope == nullptr) {
            return;
          }

          auto erase_all = [&](auto &list) {
            for (auto it = list.begin(); it != list.end();) {
              if (*it == decl) {
                it = list.erase(it);
              } else {
                ++it;
              }
            }
          };

          if (SgGlobal *global = isSgGlobal(scope)) {
            erase_all(global->get_declarations());
            return;
          }
          if (SgNamespaceDefinitionStatement *ns_def =
                  isSgNamespaceDefinitionStatement(scope)) {
            erase_all(ns_def->get_declarations());
            return;
          }
          if (SgDeclarationScope *decl_scope = isSgDeclarationScope(scope)) {
            erase_all(decl_scope->get_declarations());
            return;
          }
          if (SgClassDefinition *class_def = isSgClassDefinition(scope)) {
            erase_all(class_def->get_members());
            return;
          }
          if (SgTemplateClassDefinition *template_def =
                  isSgTemplateClassDefinition(scope)) {
            erase_all(template_def->get_members());
            return;
          }
          if (SgTemplateInstantiationDefn *inst_def =
                  isSgTemplateInstantiationDefn(scope)) {
            erase_all(inst_def->get_members());
            return;
          }
          if (scope->containsOnlyDeclarations()) {
            erase_all(scope->getDeclarationList());
            return;
          }
          erase_all(scope->getStatementList());
        };

        SgScopeStatement *parent_scope = isSgScopeStatement(decl->get_parent());
        erase_decl_from_scope(parent_scope);

        SgScopeStatement *decl_scope = decl->get_scope();
        if (decl_scope != parent_scope) {
          erase_decl_from_scope(decl_scope);
        }
      };

  auto suppress_tag_decl_spelled_in_type_loc =
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

    while (current_type != nullptr) {
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

    auto suppress_class_decl = [&](SgClassDeclaration *decl) {
      if (decl == nullptr) {
        return;
      }
      detach_decl_from_scope_child_lists_for_spelled_type(decl);
      decl->set_isAutonomousDeclaration(false);
      suppress_unparse_output(decl);
    };
    auto suppress_enum_decl = [&](SgEnumDeclaration *decl) {
      if (decl == nullptr) {
        return;
      }
      detach_decl_from_scope_child_lists_for_spelled_type(decl);
      decl->set_isAutonomousDeclaration(false);
      suppress_unparse_output(decl);
    };

    if (SgClassDeclaration *class_decl =
            isSgClassDeclaration(lookupSgDeclarationForClangDecl(
                tag_decl, /*allow_on_demand=*/true))) {
      suppress_class_decl(class_decl);
      suppress_class_decl(
          isSgClassDeclaration(class_decl->get_firstNondefiningDeclaration()));
      suppress_class_decl(
          isSgClassDeclaration(class_decl->get_definingDeclaration()));
      return true;
    }

    if (SgEnumDeclaration *enum_decl =
            isSgEnumDeclaration(lookupSgDeclarationForClangDecl(
                tag_decl, /*allow_on_demand=*/true))) {
      suppress_enum_decl(enum_decl);
      suppress_enum_decl(
          isSgEnumDeclaration(enum_decl->get_firstNondefiningDeclaration()));
      suppress_enum_decl(
          isSgEnumDeclaration(enum_decl->get_definingDeclaration()));
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
                      static_cast<int>(param_decl->getFunctionScopeDepth()));
              param_ref->set_parameter_type(sg_underlying_type);
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
            SgType *decl_type =
                SageBuilder::buildDeclType(expr_copy, sg_underlying_type);
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
      clang::QualType pointee_type_qt =
          pointer_type_ptr != nullptr
              ? markClangQualTypeDefined(readClangApiValueDefined(
                    [&]() { return pointer_type_ptr->getPointeeType(); }))
              : clang::QualType();
      pointee_type = recover_missing_pointee_qualifiers(
          pointee_type, pointee_loc_type, pointee_type_qt);
      SgType *pointer_type = SageBuilder::buildPointerType(pointee_type);
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
      clang::QualType pointee_type_qt =
          reference_type_ptr != nullptr
              ? markClangQualTypeDefined(readClangApiValueDefined(
                    [&]() { return reference_type_ptr->getPointeeType(); }))
              : clang::QualType();
      pointee_type = recover_missing_pointee_qualifiers(
          pointee_type, pointee_loc_type, pointee_type_qt);
      SgType *reference_type = SageBuilder::buildReferenceType(pointee_type);
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
      clang::QualType pointee_type_qt =
          reference_type_ptr != nullptr
              ? markClangQualTypeDefined(readClangApiValueDefined(
                    [&]() { return reference_type_ptr->getPointeeType(); }))
              : clang::QualType();
      pointee_type = recover_missing_pointee_qualifiers(
          pointee_type, pointee_loc_type, pointee_type_qt);
      SgType *reference_type =
          SageBuilder::buildRvalueReferenceType(pointee_type);
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
    (void)suppress_tag_decl_spelled_in_type_loc(
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
      if (clang::ParmVarDecl *param_decl = function_proto_loc.getParam(i)) {
        if (clang::TypeSourceInfo *type_info =
                param_decl->getTypeSourceInfo()) {
          (void)suppress_tag_decl_spelled_in_type_loc(type_info->getTypeLoc());
          param_type = buildTypeFromTypeLoc(type_info->getTypeLoc());
        }
      }
      if (param_type == nullptr) {
        param_type =
            buildTypeFromQualifiedType(function_proto_type->getParamType(i));
      }
      if (param_type == nullptr) {
        param_type = SageBuilder::buildUnknownType();
      }
      param_type_list->append_argument(param_type);
    }

    if (function_proto_type->isVariadic()) {
      param_type_list->append_argument(SgTypeEllipse::createType());
    }

    (void)suppress_tag_decl_spelled_in_type_loc(
        function_proto_loc.getReturnLoc());
    SgType *ret_type = buildTypeFromTypeLoc(function_proto_loc.getReturnLoc());
    if (ret_type == nullptr) {
      ret_type =
          buildTypeFromQualifiedType(function_proto_type->getReturnType());
    }
    if (ret_type == nullptr) {
      ret_type = SageBuilder::buildUnknownType();
    }

    SgFunctionType *function_type = buildFunctionTypeForClangProto(
        ret_type, param_type_list, function_proto_type);

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
          SgTemplateArgumentPtrList &tpl_args,
          bool preserve_empty_template_argument_list,
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

    // This path is only constructing the spelled template type syntax.
    // Forcing on-demand declaration translation here re-enters template
    // materialization from dependent and unevaluated contexts, which can
    // recursively explode system-header templates (e.g. aligned_union ->
    // aligned_storage) while the surrounding declaration is still incomplete.
    // Let semantic paths translate the declaration when they actually need it.

    const bool has_template_args =
        preserve_empty_template_argument_list || !tpl_args.empty();

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
      nrtype = buildNonrealTypeFromNestedNameSpecifier(
          effective_qualifier, effective_scope, SgName(base_name),
          has_template_args ? &tpl_args : nullptr);
    } else {
      nrtype = SageBuilder::buildNonrealType(SgName(base_name), effective_scope,
                                             has_template_args ? &tpl_args
                                                               : nullptr);
    }
    if (SgNonrealDecl *nrdecl =
            isSgNonrealDecl(nrtype ? nrtype->get_declaration() : nullptr)) {
      if (has_template_keyword) {
        nrdecl->set_has_template_keyword(true);
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
      p_pending_nonreal_template_decl_links[nrdecl] = decl_key;
    }

    return candidate;
  };
  std::function<SgNonrealType *(clang::TypeLoc, SgScopeStatement *, bool)>
      build_nonreal_type_for_nested_name_specifier_typeloc;
  std::function<SgNonrealType *(clang::NestedNameSpecifierLoc,
                                SgScopeStatement *, const SgName &,
                                const SgTemplateArgumentPtrList *)>
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
          const unsigned num_args =
              readClangApiValueDefined([&]() { return spec_loc.getNumArgs(); });
          for (unsigned i = 0; i < num_args; ++i) {
            appendTemplateArguments(tpl_args, readClangApiValueDefined([&]() {
                                      return spec_loc.getArgLoc(i);
                                    }),
                                    true);
          }
          if (enable_default_template_args) {
            append_default_args_from_clang_template_decl(
                resolve_template_decl(tname), tpl_args);
          }

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
          const bool has_empty_angle_spelling =
              num_args == 0 && readClangApiValueDefined([&]() {
                                 return spec_loc.getLAngleLoc();
                               }).isValid();
          if (SgType *nr = build_nonreal_template_type(
                  base_name, scope, qualifier, has_template_keyword, tpl_args,
                  has_empty_angle_spelling, resolve_template_decl(tname))) {
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
              prefix_loc, scope, SgName(name_str), nullptr);
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
          const SgTemplateArgumentPtrList *terminal_template_args)
      -> SgNonrealType * {
    qualifier_loc = markClangNestedNameSpecifierLocDefined(qualifier_loc);
    if (!qualifier_loc) {
      return SageBuilder::buildNonrealType(terminal_name, scope,
                                           terminal_template_args);
    }

    SgScopeStatement *effective_scope = scope;
    if (effective_scope == nullptr) {
      effective_scope = SageBuilder::topScopeStack();
    }
    if (nestedNameSpecifierLocHasExplicitGlobal(qualifier_loc) ||
        nestedNameSpecifierHasNamespaceQualifier(
            qualifier_loc.getNestedNameSpecifier())) {
      effective_scope = getGlobalScope();
    }
    ROSE_ASSERT(effective_scope != nullptr);

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
        segment_type = SageBuilder::buildNonrealType(SgName(name_str),
                                                     current_scope, nullptr);
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
        segment_type = SageBuilder::buildNonrealType(SgName(name_str),
                                                     current_scope, nullptr);
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

    SgNonrealType *nrtype = SageBuilder::buildNonrealType(
        terminal_name, chain_scope, terminal_template_args);
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

    return nrtype;
  };

  if (auto member_pointer_loc = type_loc.getAs<clang::MemberPointerTypeLoc>()) {
    const clang::MemberPointerType *member_pointer_type =
        member_pointer_loc.getTypePtr();
    SgType *class_type = nullptr;
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

        clang::NestedNameSpecifierLoc prefix_loc =
            nested_name_specifier_loc_prefix(current_loc);
        if (!prefix_loc) {
          return nullptr;
        }

        if (clang::TypeLoc qualifier_type_loc = current_loc.getAsTypeLoc();
            !qualifier_type_loc.isNull()) {
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
            for (unsigned i = 0; i < spec_loc.getNumArgs(); ++i) {
              appendTemplateArguments(tpl_args, spec_loc.getArgLoc(i), true);
            }
            if (enable_default_template_args) {
              append_default_args_from_clang_template_decl(
                  resolve_template_decl(tname), tpl_args);
            }

            return build_nonreal_type_from_nested_name_specifier_loc(
                prefix_loc, resolve_scope(), SgName(base_name),
                (spec_loc.getNumArgs() == 0 &&
                 spec_loc.getLAngleLoc().isValid()) ||
                        !tpl_args.empty()
                    ? &tpl_args
                    : nullptr);
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
                prefix_loc, resolve_scope(), SgName(name_str), nullptr);
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

      if (nestedNameSpecifierLocHasExplicitGlobal(qualifier_loc) ||
          static_cast<bool>(nested_name_specifier_loc_prefix(qualifier_loc))) {
        class_type =
            build_class_type_from_member_pointer_qualifier_loc(qualifier_loc);
      }
      if (clang::TypeLoc qualifier_type_loc = qualifier_loc.getAsTypeLoc();
          class_type == nullptr && !qualifier_type_loc.isNull()) {
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
    if (class_type == nullptr) {
      class_type = SageBuilder::buildUnknownType();
    }

    SgType *base_type =
        buildTypeFromTypeLoc(member_pointer_loc.getPointeeLoc());
    if (base_type == nullptr && member_pointer_type != nullptr) {
      base_type =
          buildTypeFromQualifiedType(member_pointer_type->getPointeeType());
    }
    if (base_type == nullptr) {
      base_type = SageBuilder::buildUnknownType();
    }

    if (member_pointer_type != nullptr &&
        member_pointer_type->isMemberFunctionPointer()) {
      if (const clang::FunctionProtoType *proto =
              member_pointer_type->getPointeeType()
                  ->getAs<clang::FunctionProtoType>()) {
        const unsigned int mfunc_specifier =
            roseMemberFunctionSpecifierFromClangProto(proto);
        if (SgFunctionType *function_type = isSgFunctionType(base_type)) {
          base_type = SageBuilder::buildMemberFunctionType(
              function_type->get_return_type(),
              function_type->get_argument_list(), class_type, mfunc_specifier);
        }
      }
    }

    SgPointerMemberType *raw_member_pointer_sg_type =
        SageBuilder::buildPointerMemberType(base_type, class_type);
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
          std::string return_qualifier = nestedNameSpecifierLocToString(
              return_qualifier_loc, p_compiler_instance);
          if (!return_qualifier.empty()) {
            SgNode::get_globalQualifiedNameMapForTypes()
                [raw_member_pointer_sg_type] = return_qualifier;
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
    if (spec_loc.getNumArgs() == 0 && spec_loc.getLAngleLoc().isValid() &&
        spec_loc.getRAngleLoc().isValid()) {
      return true;
    }

    for (unsigned i = 0; i < spec_loc.getNumArgs(); ++i) {
      clang::TemplateArgumentLoc arg_loc = spec_loc.getArgLoc(i);
      if (clang::TypeSourceInfo *type_info = arg_loc.getTypeSourceInfo()) {
        markClangTypeLocDataDefined(type_info->getTypeLoc());
        clang::QualType written =
            markClangQualTypeDefined(type_info->getType());
        clang::QualType canonical =
            markClangQualTypeDefined(written.getCanonicalType());
        if (!written.isNull() &&
            written.getTypePtrOrNull() != canonical.getTypePtrOrNull()) {
          return true;
        }
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
      [&](const clang::ClassTemplateSpecializationDecl *spec_decl) -> SgType * {
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

      p_pending_nonreal_template_decl_links[nrdecl] = mutable_spec;
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
          SgNonrealType *ns_type =
              SageBuilder::buildNonrealType(SgName(ns_name), scope, nullptr);
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
      if (ctx_spec != nullptr) {
        ctx_args = buildTemplateArguments(ctx_spec->getTemplateArgs(), 0);
        if (!ctx_args.empty()) {
          ctx_args_ptr = &ctx_args;
        }
      }

      SgNonrealType *record_type = SageBuilder::buildNonrealType(
          SgName(record_name), scope, ctx_args_ptr);
      if (SgNonrealDecl *record_decl = isSgNonrealDecl(
              record_type ? record_type->get_declaration() : nullptr)) {
        if (ctx_spec != nullptr) {
          attach_specialization_to_nonreal(record_type, ctx_spec);
        }
        scope = record_decl->get_nonreal_decl_scope();
      }
    }

    SgTemplateArgumentPtrList tpl_args =
        buildTemplateArguments(spec_decl->getTemplateArgs(), 0);
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

    SgNonrealType *nrtype = SageBuilder::buildNonrealType(
        SgName(spec_name), scope, tpl_args.empty() ? nullptr : &tpl_args);
    attach_specialization_to_nonreal(nrtype, spec_decl);
    return nrtype;
  };

  auto apply_elaborated_keyword_to_nonreal =
      [&](SgType *candidate, clang::ElaboratedTypeKeyword keyword) -> SgType * {
    SgNonrealType *nrtype = isSgNonrealType(candidate);
    SgNonrealDecl *nrdecl = isSgNonrealDecl(
        nrtype != nullptr ? nrtype->get_declaration() : nullptr);
    if (nrdecl != nullptr) {
      nrdecl->set_suppress_typename(keyword !=
                                    clang::ElaboratedTypeKeyword::Typename);
    }
    return candidate;
  };
  auto build_injected_class_template_id_syntax =
      [&](clang::InjectedClassNameTypeLoc injected_loc) -> SgType * {
    const bool force_template_id_syntax =
        p_force_injected_class_name_template_id_depth != 0;
    const bool has_explicit_template_arguments =
        injectedClassNameTypeLocHasExplicitTemplateArguments(
            injected_loc, p_compiler_instance);
    if (!force_template_id_syntax && !has_explicit_template_arguments) {
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

    SgTemplateArgumentPtrList tpl_args =
        build_current_instantiation_template_arguments(template_record);
    if (tpl_args.empty()) {
      return nullptr;
    }

    clang::NestedNameSpecifier qualifier =
        has_explicit_template_arguments
            ? injected_loc.getQualifierLoc().getNestedNameSpecifier()
            : std::nullopt;
    SgType *nrtype = build_nonreal_template_type(
        base_name, resolve_scope(), qualifier,
        /*has_template_keyword=*/false, tpl_args,
        /*preserve_empty_template_argument_list=*/false,
        /*template_decl=*/nullptr);
    if (nrtype == nullptr) {
      return nullptr;
    }

    if (!has_explicit_template_arguments) {
      return apply_elaborated_keyword_to_nonreal(nrtype,
                                                 injected->getKeyword());
    }

    clang::TemplateDecl *template_decl =
        class_template_decl_for_record(template_record);
    clang::Decl *decl_key = template_decl != nullptr
                                ? static_cast<clang::Decl *>(template_decl)
                                : static_cast<clang::Decl *>(template_record);
    return apply_elaborated_keyword_to_nonreal(
        attach_decl_to_nonreal(nrtype, decl_key,
                               /*allow_on_demand_lookup=*/true),
        injected->getKeyword());
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
    if (named_type == nullptr || !qualifier) {
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
      if (p_force_written_tag_type_qualification_depth != 0 && qualifier_loc) {
        return nullptr;
      }
      if (tag_type == nullptr || tag_type->isDependentType() ||
          keyword != clang::ElaboratedTypeKeyword::None ||
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

        p_pending_nonreal_template_decl_links[nonreal_decl] =
            const_cast<clang::Decl *>(llvm::cast<clang::Decl>(decl));
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
              annotate_auto_type_constraints(resolved_type);
              return resolved_type;
            }
          }

          SgTemplateArgumentPtrList tpl_args;
          const unsigned arg_count = spec_loc.getNumArgs();
          for (unsigned i = 0; i < arg_count; ++i) {
            appendTemplateArguments(tpl_args, spec_loc.getArgLoc(i), true);
          }
          if (enable_default_template_args) {
            append_default_args_from_clang_template_decl(
                resolve_template_decl(tname), tpl_args);
          }

          bool has_template_keyword = false;
          if (const clang::QualifiedTemplateName *qtn =
                  tname.getAsQualifiedTemplateName()) {
            has_template_keyword = qtn->hasTemplateKeyword();
          } else if (tname.getAsDependentTemplateName() != nullptr) {
            has_template_keyword = true;
          }

          if (SgType *nr =
                  qualifier_loc
                      ? static_cast<SgType *>(
                            build_nonreal_type_from_nested_name_specifier_loc(
                                qualifier_loc, scope, SgName(base_name),
                                (arg_count == 0 &&
                                 spec_loc.getLAngleLoc().isValid()) ||
                                        !tpl_args.empty()
                                    ? &tpl_args
                                    : nullptr))
                      : build_nonreal_template_type(
                            base_name, scope, qualifier, has_template_keyword,
                            tpl_args,
                            arg_count == 0 && spec_loc.getLAngleLoc().isValid(),
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
            return apply_elaborated_keyword_to_nonreal(nr, keyword);
          }
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
                          nullptr))
                : static_cast<SgType *>(buildNonrealTypeFromNestedNameSpecifier(
                      qualifier, scope, SgName(id->getName().str()), nullptr)),
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
                              SgName(typedef_decl->getNameAsString()), nullptr))
                    : static_cast<SgType *>(
                          buildNonrealTypeFromNestedNameSpecifier(
                              qualifier, scope,
                              SgName(typedef_decl->getNameAsString()),
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
                          SgName(using_decl->getNameAsString()), nullptr))
                : static_cast<SgType *>(buildNonrealTypeFromNestedNameSpecifier(
                      qualifier, scope, SgName(using_decl->getNameAsString()),
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
                          nullptr))
                : static_cast<SgType *>(buildNonrealTypeFromNestedNameSpecifier(
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
        return apply_elaborated_keyword_to_nonreal(
            attach_named_decl_to_nonreal(
                qualifier_loc
                    ? static_cast<SgType *>(
                          build_nonreal_type_from_nested_name_specifier_loc(
                              qualifier_loc, scope,
                              SgName(tag_decl->getNameAsString()), nullptr))
                    : static_cast<SgType *>(
                          buildNonrealTypeFromNestedNameSpecifier(
                              qualifier, scope,
                              SgName(tag_decl->getNameAsString()), nullptr)),
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
                              SgName(decl->getNameAsString()), nullptr))
                    : static_cast<SgType *>(
                          buildNonrealTypeFromNestedNameSpecifier(
                              qualifier, scope, SgName(decl->getNameAsString()),
                              nullptr)),
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
    auto typedef_has_dependent_underlying =
        [](const clang::TypedefType *type) -> bool {
      if (type == nullptr || type->getDecl() == nullptr) {
        return false;
      }
      markClangDeclObjectDefinedByKind(type->getDecl());
      clang::QualType underlying =
          markClangQualTypeDefined(readClangApiValueDefined(
              [&]() { return type->getDecl()->getUnderlyingType(); }));
      return !underlying.isNull() &&
             (readClangApiValueDefined(
                  [&]() { return underlying->isDependentType(); }) ||
              readClangApiValueDefined([&]() {
                return underlying->isInstantiationDependentType();
              }) ||
              readClangApiValueDefined([&]() {
                return underlying->containsUnexpandedParameterPack();
              }));
    };
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
    if (!qualifier && typedef_type != nullptr &&
        !readClangApiValueDefined(
            [&]() { return typedef_type->isDependentType(); })) {
      qualifier = markClangNestedNameSpecifierDefined(readClangApiValueDefined(
          [&]() { return typedef_type->getQualifier(); }));
    }
    const bool qualifier_has_type_qualifier =
        nestedNameSpecifierHasTypeQualifier(qualifier);
    const bool qualifier_spells_namespace_or_global =
        nestedNameSpecifierLocHasExplicitGlobal(qualifier_loc) ||
        nestedNameSpecifierHasNamespaceQualifier(qualifier);
    const bool qualifier_requires_written_nonreal =
        nested_name_specifier_loc_requires_written_nonreal(qualifier_loc);
    const bool preserve_unqualified_written_typedef =
        typedef_type != nullptr && !has_written_qualifier &&
        !readClangApiValueDefined(
            [&]() { return typedef_type->isDependentType(); }) &&
        !qualifier_requires_written_nonreal;
    const bool use_translated_typedef_type =
        typedef_type != nullptr && has_written_qualifier &&
        readClangApiValueDefined([&]() {
          return typedef_type->getKeyword();
        }) == clang::ElaboratedTypeKeyword::None &&
        !readClangApiValueDefined(
            [&]() { return typedef_type->isDependentType(); }) &&
        !qualifier_has_type_qualifier && !qualifier_requires_written_nonreal &&
        !qualifier_spells_namespace_or_global;

    if (typedef_type != nullptr && !has_written_qualifier &&
        (readClangApiValueDefined(
             [&]() { return typedef_type->isDependentType(); }) ||
         typedef_has_dependent_underlying(typedef_type)) &&
        !qualifier_requires_written_nonreal) {
      if (SgType *written_dependent_typedef =
              build_unqualified_dependent_alias_nonreal(
                  llvm::dyn_cast_or_null<clang::TypedefNameDecl>(
                      markClangDeclObjectDefinedByKind(readClangApiValueDefined(
                          [&]() { return typedef_type->getDecl(); }))),
                  readClangApiValueDefined(
                      [&]() { return typedef_type->getKeyword(); }))) {
        return finalize_spelled_type(written_dependent_typedef);
      }
    }

    if (preserve_unqualified_written_typedef) {
      SgNode *translated_typedef_node = nullptr;
      if (VisitTypedefType(const_cast<clang::TypedefType *>(typedef_type),
                           &translated_typedef_node)) {
        if (SgType *resolved_typedef_type = isSgType(translated_typedef_node)) {
          p_type_translation_map[typedef_type] = resolved_typedef_type;
          resolved_typedef_type =
              apply_local_qualifiers(resolved_typedef_type, type_loc.getType());
          annotate_auto_type_constraints(resolved_typedef_type);
          return resolved_typedef_type;
        }
      }

      if (SgType *resolved_typedef_type =
              build_translated_typedef_type_from_decl(
                  typedef_type->getDecl())) {
        p_type_translation_map[typedef_type] = resolved_typedef_type;
        resolved_typedef_type =
            apply_local_qualifiers(resolved_typedef_type, type_loc.getType());
        annotate_auto_type_constraints(resolved_typedef_type);
        return resolved_typedef_type;
      }
    }

    if (use_translated_typedef_type) {
      if (SgType *resolved_type =
              buildTypeFromQualifiedType(type_loc.getType())) {
        annotate_auto_type_constraints(resolved_type);
        return resolved_type;
      }
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
        p_force_written_tag_type_qualification_depth != 0 && qualifier_loc;
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
          method_qualifier_loc, resolve_scope(), SgName(tag_name), nullptr);
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
        readClangApiValueDefined([&]() { return tag_type->getKeyword(); }) ==
            clang::ElaboratedTypeKeyword::None &&
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
            qualifier_loc
                ? build_nonreal_type_from_nested_name_specifier_loc(
                      qualifier_loc, scope, SgName(id->getName().str()),
                      nullptr)
                : buildNonrealTypeFromNestedNameSpecifier(
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
      std::string base_name = getTemplateNameBase(tname);
      if (!base_name.empty()) {
        SgTemplateArgumentPtrList tpl_args;
        const unsigned arg_count = spec_loc.getNumArgs();
        for (unsigned i = 0; i < arg_count; ++i) {
          appendTemplateArguments(tpl_args, spec_loc.getArgLoc(i), true);
        }
        if (enable_default_template_args) {
          append_default_args_from_clang_template_decl(
              resolve_template_decl(tname), tpl_args);
        }

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
        if (preserve_written_lookup ||
            template_specialization_type_loc_requires_written_nonreal(
                spec_loc)) {
          if (has_written_prefix) {
            SgType *nr = build_nonreal_type_from_nested_name_specifier_loc(
                spec_loc.getPrefix(), scope, SgName(base_name),
                (arg_count == 0 && spec_loc.getLAngleLoc().isValid()) ||
                        !tpl_args.empty()
                    ? &tpl_args
                    : nullptr);
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
                    spec_record_decl)) {
              return finalize_spelled_type(nr);
            }
          }
          if (SgType *nr = build_nonreal_template_type(
                  base_name, scope, qualifier, has_template_keyword, tpl_args,
                  arg_count == 0 && spec_loc.getLAngleLoc().isValid(),
                  template_decl)) {
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
        unsigned arg_count = spec_loc.getNumArgs();
        for (unsigned i = 0; i < arg_count; ++i) {
          appendTemplateArguments(tpl_args, spec_loc.getArgLoc(i), true);
        }
        if (enable_default_template_args) {
          append_default_args_from_clang_template_decl(
              resolve_template_decl(tname), tpl_args);
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

        if (SgType *nr = build_nonreal_template_type(
                base_name, resolve_scope(), qualifier, has_template_keyword,
                tpl_args, arg_count == 0 && spec_loc.getLAngleLoc().isValid(),
                resolve_template_decl(tname))) {
          if (clang::NestedNameSpecifierLoc qualifier_loc =
                  spec_loc.getPrefix()) {
            nr = build_nonreal_type_from_nested_name_specifier_loc(
                qualifier_loc, resolve_scope(), SgName(base_name),
                (arg_count == 0 && spec_loc.getLAngleLoc().isValid()) ||
                        !tpl_args.empty()
                    ? &tpl_args
                    : nullptr);
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
        preserve_written_template_specialization_arguments(spec_loc,
                                                           resolved_type);
        if (resolves_to_primary_template_class_type(resolved_type)) {
          clang::TemplateName tname = tst->getTemplateName();
          std::string base_name = getTemplateNameBase(tname);
          if (!base_name.empty()) {
            SgTemplateArgumentPtrList tpl_args;
            const unsigned arg_count = spec_loc.getNumArgs();
            for (unsigned i = 0; i < arg_count; ++i) {
              appendTemplateArguments(tpl_args, spec_loc.getArgLoc(i), true);
            }
            if (enable_default_template_args) {
              append_default_args_from_clang_template_decl(
                  resolve_template_decl(tname), tpl_args);
            }

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

            if (SgType *nr = build_nonreal_template_type(
                    base_name, resolve_scope(), qualifier,
                    has_template_keyword ||
                        spec_loc.getTemplateKeywordLoc().isValid(),
                    tpl_args,
                    arg_count == 0 && spec_loc.getLAngleLoc().isValid(),
                    resolve_template_decl(tname))) {
              annotate_auto_type_constraints(nr);
              return finalize_spelled_type(nr);
            }
          }
        }
      }
      annotate_auto_type_constraints(resolved_type);
      return resolved_type;
    }
    clang::TemplateName tname = tst->getTemplateName();
    std::string base_name = getTemplateNameBase(tname);
    if (!base_name.empty()) {
      SgTemplateArgumentPtrList tpl_args = buildTemplateArguments(tst);
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
      if (SgType *nr = build_nonreal_template_type(
              base_name, resolve_scope(), qualifier, has_template_keyword,
              tpl_args, false, resolve_template_decl(tname))) {
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

  const bool cache_translation = canCacheTypeTranslation(type);
  auto it = p_type_translation_map.end();
  if (cache_translation) {
    it = p_type_translation_map.find(type);
  }
#if DEBUG_TRAVERSE_TYPE
  std::cerr << "Traverse Type : " << type << " " << type->getTypeClassName()
            << std::endl;
#endif
  if (it != p_type_translation_map.end()) {
#if DEBUG_TRAVERSE_TYPE
    std::cerr << " already visited : node = " << it->second << std::endl;
#endif
    return it->second;
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
    std::cerr << "Warning: Unhandled clang::Type '" << type->getTypeClassName()
              << "'. Using opaque type." << std::endl;
    ret_status = true;
    break;
  }

  if (result == NULL) {
    result = SageBuilder::buildUnknownType();
  }

  bool store_translation_in_cache = cache_translation;
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

  if (store_translation_in_cache) {
    p_type_translation_map.insert(
        std::pair<const clang::Type *, SgNode *>(type, result));
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
        SageBuilder::buildExprListExp(SageBuilder::buildNullExpression());
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
  if (base_type == nullptr) {
    base_type = SageBuilder::buildUnknownType();
  }
  *node = base_type;

  return VisitType(atomic_type, node) && res;
}

bool ClangToSageTranslator::VisitAttributedType(
    clang::AttributedType *attributed_type, SgNode **node) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToSageTranslator::VisitAttributedType" << std::endl;
#endif

  SgType *type = buildTypeFromQualifiedType(attributed_type->getModifiedType());
  if (type == nullptr) {
    type = SageBuilder::buildUnknownType();
  }
  SgModifierType *modified_type = nullptr;
  auto ensure_modifier = [&]() -> SgTypeModifier & {
    if (modified_type == nullptr) {
      modified_type = SageBuilder::buildModifierType(type);
    }
    return modified_type->get_typeModifier();
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

  if (modified_type != nullptr) {
    *node = SgModifierType::insertModifierTypeIntoTypeTable(modified_type);
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
  if (pointee_type == nullptr) {
    pointee_type = SageBuilder::buildUnknownType();
  }
  *node = SageBuilder::buildPointerType(pointee_type);

  return VisitType(block_pointer_type, node) && res;
}

bool ClangToSageTranslator::VisitBuiltinType(clang::BuiltinType *builtin_type,
                                             SgNode **node) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToSageTranslator::VisitBuiltinType" << std::endl;
#endif

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
    /*
            case clang::BuiltinType::NullPtr:    *node = SageBuilder::build();
       break;
    */
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
      nrtype = SageBuilder::buildNonrealType(SgName("__dependent_type"), scope,
                                             nullptr);
    }
    if (nrtype != nullptr) {
      if (SgNonrealDecl *nrdecl = isSgNonrealDecl(nrtype->get_declaration())) {
        setCompilerGeneratedFileInfo(nrdecl);
        suppress_unparse_output(nrdecl);
      }
    }
    *node = nrtype != nullptr
                ? static_cast<SgNode *>(nrtype)
                : static_cast<SgNode *>(SageBuilder::buildUnknownType());
    break;
  }

  case clang::BuiltinType::ObjCId:
  case clang::BuiltinType::ObjCClass:
  case clang::BuiltinType::ObjCSel:
  case clang::BuiltinType::Overload:
  case clang::BuiltinType::BoundMember:
  case clang::BuiltinType::UnknownAny:
  default: {
    // Fallback for unknown builtin types (e.g., ARM SVE types, vendor
    // extensions)
    std::string type_name =
        builtin_type->getName(p_compiler_instance->getLangOpts()).str();
    // Using fallback type for unknown builtin (suppressed)

    // Prefer a scope that can accept a typedef; avoid scopes like SgIfStmt.
    SgScopeStatement *scope = getSafeOpaqueTypeInsertionScope();
    ROSE_ASSERT(scope != nullptr);
    *node = SageBuilder::buildOpaqueType(type_name, scope);
    break;
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
          sg_decltype =
              SageBuilder::buildDeclType(expr_copy, sg_underlying_type);
        }
      }
    }
  }

  if (sg_decltype != nullptr) {
    *node = sg_decltype;
  } else if (sg_underlying_type != nullptr) {
    *node = sg_underlying_type;
  } else {
    *node = SageBuilder::buildUnknownType();
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
    *node = buildTypeFromQualifiedType(deduced);
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
  bool res = true;

  // Represent C++ auto explicitly so we do not synthesize an opaque typedef.
  *node = SageBuilder::buildAutoType();
  if (SgAutoType *sg_auto = isSgAutoType(*node)) {
    if (auto_type->isConstrained()) {
      std::string constraint_text;
      if (const clang::NamedDecl *constraint_decl =
              auto_type->getTypeConstraintConcept()) {
        constraint_text = constraint_decl->getQualifiedNameAsString();
        llvm::ArrayRef<clang::TemplateArgument> constraint_args =
            auto_type->getTypeConstraintArguments();
        if (!constraint_args.empty()) {
          constraint_text += "<";
          bool need_separator = false;
          for (const clang::TemplateArgument &arg : constraint_args) {
            appendTemplateInstantiationArg(constraint_text, need_separator,
                                           arg);
          }
          constraint_text += ">";
        }
      }
      SageInterface::setAutoTypeConstraint(sg_auto, constraint_text);
    }
  }

  return VisitDeducedType(auto_type, node) && res;
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
    *node = buildNonrealTypeFromNestedNameSpecifier(qualifier, base_scope,
                                                    SgName(base_name), nullptr);
  } else {
    *node =
        SageBuilder::buildNonrealType(SgName(base_name), base_scope, nullptr);
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
  if (pointee_type == nullptr) {
    pointee_type = SageBuilder::buildUnknownType();
  }
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
  if (elem_type == nullptr) {
    elem_type = SageBuilder::buildUnknownType();
  }
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
  if (elem_type == nullptr) {
    elem_type = SageBuilder::buildUnknownType();
  }
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
  if (ret_type == nullptr) {
    ret_type = SageBuilder::buildUnknownType();
  }
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
    SgType *param_type =
        buildTypeFromQualifiedType(function_proto_type->getParamType(i));

    param_type_list->append_argument(param_type);
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

  auto injected_name_is_local_to_current_class = [&]() -> bool {
    SgScopeStatement *current_scope = SageBuilder::topScopeStack();
    if (current_scope == nullptr) {
      return false;
    }

    SgClassDefinition *enclosing_def =
        SageInterface::getEnclosingClassDefinition(current_scope, true);
    if (enclosing_def == nullptr) {
      return false;
    }

    SgClassDeclaration *enclosing_decl =
        canonical_sg_class_decl(enclosing_def->get_declaration());
    SgClassDeclaration *target_decl =
        canonical_sg_class_decl(isSgClassDeclaration(
            lookupSgDeclarationForClangDecl(injected_class_name_type->getDecl(),
                                            /*allow_on_demand=*/true)));
    return enclosing_decl != nullptr && enclosing_decl == target_decl;
  };

  const bool force_nonlocal_injected_name =
      p_force_nonlocal_injected_class_name_depth != 0;
  if ((force_nonlocal_injected_name ||
       !injected_name_is_local_to_current_class()) &&
      p_compiler_instance != nullptr) {
    clang::QualType injected_qt = getInjectedClassNameSpecializationType(
        injected_class_name_type, p_compiler_instance->getASTContext());
    if (!injected_qt.isNull() &&
        injected_qt.getTypePtrOrNull() != injected_class_name_type) {
      *node = buildTypeFromQualifiedType(injected_qt);
      if (*node != nullptr) {
        return VisitType(injected_class_name_type, node) && res;
      }
    }
  }

  std::string name_str = injected_class_name_type->getDecl()->getNameAsString();
  if (name_str.empty()) {
    name_str = "__injected_class";
  }

  SgScopeStatement *scope = SageBuilder::topScopeStack();
  if (scope == nullptr) {
    scope = getGlobalScope();
  }

  SgNonrealType *nrtype =
      SageBuilder::buildNonrealType(SgName(name_str), scope, nullptr);
  if (SgNonrealDecl *nrdecl =
          isSgNonrealDecl(nrtype ? nrtype->get_declaration() : nullptr)) {
    nrdecl->set_suppress_typename(true);
  }
  *node = nrtype;

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
          const clang::NamedDecl *fallback_decl) -> SgType * {
    clang::NestedNameSpecifier prefix = std::nullopt;
    std::string name_str;
    SgTemplateArgumentPtrList tpl_args;
    const SgTemplateArgumentPtrList *tpl_args_ptr = nullptr;
    if (qualifier) {
      prefix = nestedNameSpecifierPrefix(qualifier);
      if (qualifier.getKind() == clang::NestedNameSpecifier::Kind::Type) {
        if (const clang::Type *qualifier_type = qualifier.getAsType()) {
          if (const auto *spec =
                  llvm::dyn_cast<clang::TemplateSpecializationType>(
                      qualifier_type)) {
            name_str = getTemplateNameBase(spec->getTemplateName());
            tpl_args = buildTemplateArguments(spec);
            applyExplicitTemplateArgumentFlags(tpl_args, tpl_args.size());
            if (!tpl_args.empty()) {
              tpl_args_ptr = &tpl_args;
            }
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

    if (name_str.empty() && fallback_decl != nullptr) {
      name_str = fallback_decl->getNameAsString();
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
      lookup_scope = getGlobalScope();
    }

    return buildNonrealTypeFromNestedNameSpecifier(
        prefix, lookup_scope, SgName(name_str), tpl_args_ptr);
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
          scope = getGlobalScope();
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
    classType = buildTypeFromQualifiedType(classQualType);
  }
  if (classType == NULL) {
    classType = SageBuilder::buildUnknownType();
  }
  const clang::Type *class_type_ptr = classQualType.getTypePtrOrNull();
  SgType *classTypeStripped =
      classType != NULL ? classType->stripTypedefsAndModifiers() : NULL;
  auto needs_nonreal_fallback = [](SgType *type) -> bool {
    if (type == nullptr) {
      return true;
    }
    if (isSgClassType(type) != nullptr || isSgNonrealType(type) != nullptr) {
      return false;
    }
    if (isSgTypedefType(type) != nullptr) {
      return false;
    }
    return true;
  };
  if (needs_nonreal_fallback(classTypeStripped)) {
    SgScopeStatement *fallback_scope = SageBuilder::topScopeStack();
    if (fallback_scope == nullptr) {
      fallback_scope = getGlobalScope();
    }
    if (class_type_ptr != nullptr) {
      if (SgNonrealType *nrtype = buildNonrealTypeForNestedNameSpecifierType(
              class_type_ptr, fallback_scope, /*prefer_current_scope=*/true)) {
        classType = nrtype;
      }
    }
    classTypeStripped =
        classType != NULL ? classType->stripTypedefsAndModifiers() : NULL;
    if (needs_nonreal_fallback(classTypeStripped)) {
      std::string class_name =
          clangQualTypeAsStringDefinedForFrontend(classQualType);
      if (class_name.empty()) {
        class_name = "unknown_member_class";
      }
      classType = SageBuilder::buildNonrealType(SgName(class_name),
                                                fallback_scope, nullptr);
    }
  }

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
      SgMemberFunctionType *memFuncType = SageBuilder::buildMemberFunctionType(
          functionType->get_return_type(), functionType->get_argument_list(),
          classType, mfunc_specifier);
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

  if (pattern_type != NULL) {
    if (SgTemplateType *template_type = isSgTemplateType(pattern_type)) {
      template_type->set_packed(true);
    }
    // Use the pattern type directly - the pack expansion is handled at a higher
    // level
    *node = pattern_type;
  } else {
    // Fallback: create a packed template type to preserve template context
    SgTemplateType *pack_type =
        SageBuilder::buildTemplateType(SgName("pack_expansion"));
    pack_type->set_packed(true);
    *node = pack_type;
  }

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
  if (elem_type == nullptr) {
    elem_type = SageBuilder::buildUnknownType();
  }
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

  clang::QualType pointee_qual_type =
      markClangQualTypeDefined(reference_type->getPointeeTypeAsWritten());
  if (reference_type->isInnerRef()) {
    const clang::ReferenceType *current_ref = reference_type;
    do {
      pointee_qual_type =
          markClangQualTypeDefined(current_ref->getPointeeTypeAsWritten());
      current_ref = llvm::dyn_cast_or_null<clang::ReferenceType>(
          markClangTypeObjectDefinedByClass(
              pointee_qual_type.getTypePtrOrNull()));
    } while (current_ref != nullptr && current_ref->isInnerRef());
  }
  SgType *pointee_type = buildTypeFromQualifiedType(pointee_qual_type);
  if (pointee_type == nullptr) {
    return false;
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
  if (pack_type != nullptr) {
    pack_type->set_packed(true);
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
  } else {
    *node = SageBuilder::buildUnknownType();
  }

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
    *node = SageBuilder::buildUnknownType();
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
  auto canonicalize_enum_type =
      [&](SgEnumType *rose_enum_type) -> SgEnumType * {
    if (rose_enum_type == nullptr) {
      return nullptr;
    }
    if (SgEnumDeclaration *decl = canonical_enum_decl(
            isSgEnumDeclaration(rose_enum_type->get_declaration()))) {
      if (rose_enum_type->get_declaration() != decl) {
        rose_enum_type->set_declaration(decl);
      }
    }
    return rose_enum_type;
  };

  SgSymbol *sym = GetSymbolFromSymbolTable(enum_type->getDecl());

  SgEnumSymbol *enum_sym = isSgEnumSymbol(sym);

  if (enum_sym == NULL) {
    SgNode *tmp_decl = TraverseOnDemand(enum_type->getDecl());
    SgEnumDeclaration *sg_decl =
        canonical_enum_decl(isSgEnumDeclaration(tmp_decl));

    ROSE_ASSERT(sg_decl != NULL);

    *node = canonicalize_enum_type(isSgEnumType(sg_decl->get_type()));
  } else {
    *node = canonicalize_enum_type(isSgEnumType(enum_sym->get_type()));
  }

  rememberEnumTypeFirstSeenState(p_enum_type_decl_first_see_in_type,
                                 isSgType(*node), enum_sym == NULL);

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
  clang::RecordDecl *record_decl = markClangRecordDeclDefined(
      readClangApiValueDefined([&]() { return record_type->getDecl(); }));
  bool used_header_placeholder_decl = false;

  bool is_specialization =
      record_decl != nullptr &&
      (llvm::isa<clang::ClassTemplateSpecializationDecl>(record_decl) ||
       llvm::isa<clang::ClassTemplatePartialSpecializationDecl>(record_decl));

  // Prefer the canonical declaration for symbol lookup and type association.
  //
  // Clang's RecordType can point at a later redeclaration/definition (including
  // out-of-line member definitions). Using that as the identity for record
  // types can force on-demand translation of the definition while we're still
  // inside a different lexical context, which destabilizes ROSE's
  // defining/nondefining declaration chains and name-qualification (Issue 69).
  //
  // For class-template (partial) specializations we must keep the
  // specialization decl as the identity (Issue 126).
  clang::RecordDecl *lookup_decl = record_decl;
  if (!is_specialization && record_decl != NULL) {
    clang::TagDecl *canonical =
        const_cast<clang::TagDecl *>(llvm::dyn_cast_or_null<clang::TagDecl>(
            markClangDeclObjectDefinedByKind(readClangApiValueDefined(
                [&]() { return record_decl->getCanonicalDecl(); }))));
    if (clang::RecordDecl *canonical_record =
            llvm::dyn_cast_or_null<clang::RecordDecl>(canonical)) {
      lookup_decl = canonical_record;
    }
  }

  auto build_header_placeholder_record_decl =
      [&](clang::RecordDecl *target_decl) -> SgClassDeclaration * {
    target_decl = markClangRecordDeclDefined(target_decl);
    if (target_decl == nullptr || p_compiler_instance == nullptr) {
      return nullptr;
    }

    clang::SourceLocation target_loc = target_decl->getLocation();
    if (!target_loc.isValid()) {
      return nullptr;
    }
    clang::SourceManager &sm = p_compiler_instance->getSourceManager();
    clang::SourceLocation file_loc = sm.getFileLoc(target_loc);
    if (!file_loc.isValid() || sm.isInMainFile(file_loc)) {
      return nullptr;
    }

    if (llvm::isa<clang::ClassTemplatePartialSpecializationDecl>(target_decl)) {
      return nullptr;
    }

    if (target_decl->isAnonymousStructOrUnion() ||
        target_decl->getIdentifier() == nullptr) {
      return nullptr;
    }

    if (clang::CXXRecordDecl *cxx_record =
            llvm::dyn_cast<clang::CXXRecordDecl>(target_decl)) {
      cxx_record = const_cast<clang::CXXRecordDecl *>(
          llvm::cast_or_null<clang::CXXRecordDecl>(
              markClangDeclObjectDefinedByKind(cxx_record)));
      if (isClangLocalClassContext(cxx_record->getDeclContext()) ||
          markClangDeclObjectDefinedByKind(
              cxx_record->getDescribedClassTemplate()) != nullptr) {
        return nullptr;
      }
    }

    SgDeclarationStatement *existing =
        lookupSgDeclarationForClangDecl(target_decl, /*allow_on_demand=*/false);
    if (SgClassDeclaration *existing_class = isSgClassDeclaration(existing)) {
      return existing_class;
    }
    if (SgClassDeclaration *existing_placeholder =
            lookupRecordTypePlaceholderDecl(target_decl)) {
      return existing_placeholder;
    }

    auto class_kind_for_record =
        [](const clang::RecordDecl *decl) -> SgClassDeclaration::class_types {
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

    clang::DeclContext *scope_context =
        markClangDeclContextObjectDefined(target_decl->getDeclContext());
    while (scope_context != nullptr &&
           llvm::isa<clang::LinkageSpecDecl>(scope_context)) {
      scope_context =
          markClangDeclContextObjectDefined(scope_context->getParent());
    }

    SgScopeStatement *scope = resolveScopeFromDeclContext(
        scope_context, SageBuilder::topScopeStack());
    if (scope == nullptr) {
      scope = SageBuilder::topScopeStack();
    }
    if (scope == nullptr) {
      scope = getGlobalScope();
    }

    SgClassDeclaration *placeholder = nullptr;
    if (clang::ClassTemplateSpecializationDecl *spec_decl =
            llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(
                target_decl)) {
      std::string base_name =
          spec_decl->getSpecializedTemplate() != nullptr
              ? spec_decl->getSpecializedTemplate()->getNameAsString()
              : spec_decl->getNameAsString();
      if (base_name.empty()) {
        return nullptr;
      }

      SgTemplateArgumentPtrList tpl_args;
      const clang::TemplateArgumentList *args_ptr =
          markClangTemplateArgumentListDefined(&spec_decl->getTemplateArgs());
      ROSE_ASSERT(args_ptr != nullptr);
      for (const clang::TemplateArgument &arg :
           markClangTemplateArgumentArrayDefined(args_ptr->asArray())) {
        appendTemplateArguments(tpl_args, arg, false);
      }

      placeholder = SageBuilder::buildNondefiningClassDeclaration_nfi(
          SgName(base_name), class_kind_for_record(spec_decl), scope,
          /*buildTemplateInstantiation=*/true, &tpl_args);
    } else {
      std::string record_name = target_decl->getNameAsString();
      if (record_name.empty()) {
        return nullptr;
      }

      placeholder = SageBuilder::buildNondefiningClassDeclaration_nfi(
          SgName(record_name), class_kind_for_record(target_decl), scope,
          /*buildTemplateInstantiation=*/false, /*templateArgumentsList=*/NULL);
    }

    if (placeholder == nullptr) {
      return nullptr;
    }

    applySourceRange(placeholder, readClangApiValueDefined([&]() {
                       return target_decl->getSourceRange();
                     }));
    placeholder->set_scope(scope);
    if (placeholder->get_parent() == nullptr) {
      placeholder->set_parent(scope);
    }
    placeholder->set_isAutonomousDeclaration(false);
    if (SgNamedType *named_type = isSgNamedType(placeholder->get_type())) {
      named_type->set_autonomous_declaration(false);
    }

    // Header-only placeholder declarations are semantic scaffolding for type
    // binding. If they stay output-visible, semantic-only template-type uses
    // can surface as bogus namespace-scope forward declarations.
    auto hide_placeholder_file_info = [](Sg_File_Info *fi) {
      if (fi == nullptr) {
        return;
      }
      fi->setCompilerGenerated();
      fi->setFrontendSpecific();
      fi->unsetOutputInCodeGeneration();
    };
    hide_placeholder_file_info(placeholder->get_file_info());
    hide_placeholder_file_info(placeholder->get_startOfConstruct());
    hide_placeholder_file_info(placeholder->get_endOfConstruct());

    cacheRecordTypePlaceholderDecl(target_decl, placeholder);

    used_header_placeholder_decl = true;
    return placeholder;
  };

  SgClassSymbol *class_sym = NULL;

  // Record types for class-template specializations (e.g. `A<>`) must resolve
  // to the specialization/instantiation declaration, not the primary template.
  // Otherwise the unparser can emit invalid type spellings such as `template A`
  // (Issue 126).
  if (is_specialization) {
    *node = getTypeFromTranslatedRecordDecl(record_decl);
    if (*node == NULL) {
      if (SgClassDeclaration *placeholder =
              build_header_placeholder_record_decl(record_decl)) {
        *node = placeholder->get_type();
      }
    }
    if (*node == NULL) {
      if (SgDeclarationStatement *resolved =
              lookupSgDeclarationForClangDecl(record_decl,
                                              /*allow_on_demand=*/true)) {
        if (SgClassDeclaration *sg_decl =
                translatedRecordTypeDeclaration(record_decl, resolved)) {
          *node = sg_decl->get_type();
        }
      }
    }
  }

  if (*node == NULL && !is_specialization) {
    if (SgDeclarationStatement *cached =
            lookupSgDeclarationForClangDecl(lookup_decl,
                                            /*allow_on_demand=*/false)) {
      if (SgClassDeclaration *sg_decl = isSgClassDeclaration(cached)) {
        ROSE_ASSERT(sg_decl->get_firstNondefiningDeclaration() != NULL);
        *node = sg_decl->get_type();
      }
    }
    if (*node == NULL) {
      *node = getTypeFromTranslatedRecordDecl(lookup_decl);
    }
    if (*node == NULL) {
      if (SgClassDeclaration *placeholder =
              build_header_placeholder_record_decl(lookup_decl)) {
        *node = placeholder->get_type();
      }
    }
  }

  if (*node == NULL) {
    SgSymbol *sym = GetSymbolFromSymbolTable(lookup_decl);
    class_sym = isSgClassSymbol(sym);

    if (class_sym != NULL && is_specialization) {
      SgClassDeclaration *sym_decl = class_sym->get_declaration();
      if (isSgTemplateInstantiationDecl(sym_decl) == NULL) {
        // Avoid binding specializations to primary-template symbols.
        class_sym = NULL;
      }
    }

    if (class_sym == NULL) {
      if (*node == NULL && !is_specialization) {
        if (SgDeclarationStatement *resolved =
                lookupSgDeclarationForClangDecl(lookup_decl,
                                                /*allow_on_demand=*/true)) {
          if (SgClassDeclaration *sg_decl = isSgClassDeclaration(resolved)) {
            if (SgClassDeclaration *first_nondef = isSgClassDeclaration(
                    sg_decl->get_firstNondefiningDeclaration())) {
              sg_decl = first_nondef;
            }
            *node = sg_decl->get_type();
          }
        }
      }
      if (*node == NULL) {
        if (is_specialization) {
          if (clang::ClassTemplateSpecializationDecl *spec_decl =
                  llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(
                      record_decl)) {
            std::string base_name =
                spec_decl->getSpecializedTemplate() != nullptr
                    ? spec_decl->getSpecializedTemplate()->getNameAsString()
                    : spec_decl->getNameAsString();
            if (base_name.empty()) {
              base_name = "__template_specialization";
            }

            SgTemplateArgumentPtrList tpl_args;
            const clang::TemplateArgumentList &args =
                spec_decl->getTemplateArgs();
            for (unsigned i = 0; i < args.size(); ++i) {
              appendTemplateArguments(tpl_args, args.get(i), false);
            }
            if (clang::ClassTemplateDecl *primary =
                    spec_decl->getSpecializedTemplate()) {
              if (clang::TemplateParameterList *params =
                      primary->getTemplateParameters()) {
                if (tpl_args.size() < params->size()) {
                  for (unsigned i = tpl_args.size(); i < params->size(); ++i) {
                    clang::NamedDecl *param = params->getParam(i);
                    if (auto *type_param =
                            llvm::dyn_cast_or_null<clang::TemplateTypeParmDecl>(
                                param)) {
                      if (type_param->hasDefaultArgument()) {
                        appendTemplateArguments(
                            tpl_args, type_param->getDefaultArgument(), false);
                        continue;
                      }
                      break;
                    }
                    if (auto *non_type_param = llvm::dyn_cast_or_null<
                            clang::NonTypeTemplateParmDecl>(param)) {
                      if (non_type_param->hasDefaultArgument()) {
                        appendTemplateArguments(
                            tpl_args, non_type_param->getDefaultArgument(),
                            false);
                        continue;
                      }
                      break;
                    }
                    if (auto *tmpl_param = llvm::dyn_cast_or_null<
                            clang::TemplateTemplateParmDecl>(param)) {
                      if (tmpl_param->hasDefaultArgument()) {
                        appendTemplateArguments(
                            tpl_args, tmpl_param->getDefaultArgument(), false);
                        continue;
                      }
                      break;
                    }
                    break;
                  }
                }
              }
            }

            SgScopeStatement *tmpl_scope = nullptr;
            if (clang::DeclContext *ctx = spec_decl->getDeclContext()) {
              tmpl_scope = resolveScopeFromDeclContext(
                  ctx, SageBuilder::topScopeStack());
            }
            if (tmpl_scope == nullptr) {
              tmpl_scope = SageBuilder::topScopeStack();
            }
            if (tmpl_scope == nullptr) {
              tmpl_scope = getGlobalScope();
            }
            *node = SageBuilder::buildNonrealType(SgName(base_name), tmpl_scope,
                                                  &tpl_args);
            if (*node == NULL) {
              SgTemplateType *tmpl_type =
                  SageBuilder::buildTemplateType(SgName(base_name));
              if (tmpl_type != nullptr) {
                tmpl_type->get_tpl_args() = tpl_args;
                for (SgTemplateArgument *arg : tmpl_type->get_tpl_args()) {
                  if (arg != nullptr) {
                    arg->set_parent(tmpl_type);
                  }
                }
                *node = tmpl_type;
              }
            }
          }
        }

        if (*node == NULL && is_specialization) {
          std::string base_name = record_decl != nullptr
                                      ? record_decl->getNameAsString()
                                      : std::string();
          if (base_name.empty()) {
            base_name = "__template_specialization";
          }
          SgTemplateType *tmpl_type =
              SageBuilder::buildTemplateType(SgName(base_name));
          if (tmpl_type != nullptr) {
            *node = tmpl_type;
          }
        }

        if (*node == NULL && !is_specialization) {
          std::string qualified_name = lookup_decl->getQualifiedNameAsString();
          if (qualified_name.empty()) {
            qualified_name = "__anonymous_record";
          }
          // std::isalnum expects values representable as unsigned char; cast to
          // avoid UB for negative char.
          for (char &ch : qualified_name) {
            if (!(std::isalnum(static_cast<unsigned char>(ch)) || ch == '_')) {
              ch = '_';
            }
          }
          SgScopeStatement *scope = getSafeOpaqueTypeInsertionScope();
          *node = SageBuilder::buildOpaqueType(qualified_name, scope);
        }
      }
    } else {
      *node = class_sym->get_type();
    }
  }

  // Preserve whether the symbol was missing before the post-translation
  // refresh. This drives first-seen tracking for contexts like sizeof(type),
  // where ROSE may need to emit an inline base-type defining declaration.
  const bool class_symbol_missing_before_refresh = (class_sym == NULL);

  // After translating the declaration, the symbol should now exist; refresh the
  // lookup for downstream users.
  if (class_sym == NULL && !used_header_placeholder_decl) {
    class_sym = isSgClassSymbol(GetSymbolFromSymbolTable(lookup_decl));
  }

  bool first_see_in_type = class_symbol_missing_before_refresh;
  if (used_header_placeholder_decl) {
    first_see_in_type = false;
  }
  rememberClassTypeFirstSeenState(p_class_type_decl_first_see_in_type,
                                  isSgType(*node), first_see_in_type);

  return VisitType(record_type, node);
}

// Build template parameters by inferring from instantiation arguments
std::unique_ptr<SgTemplateParameterPtrList>
ClangToSageTranslator::buildTemplateParameters(
    const clang::TemplateSpecializationType *clang_type) {

  // For Clang frontend, we don't have access to the original template parameter
  // declarations since they're in standard library headers. We need to infer
  // parameters from the instantiation arguments.

  auto param_list = std::make_unique<SgTemplateParameterPtrList>();

  auto args = clang_type->template_arguments();
  int param_position = 0;

  for (const clang::TemplateArgument &arg : args) {
    markClangValueDefined(arg);
    SgType *param_type = nullptr;
    SgTemplateParameter::template_parameter_enum param_kind;
    bool is_param_pack = false;

    switch (arg.getKind()) {
    case clang::TemplateArgument::Type:
      // Type parameter (e.g., typename T)
      param_kind = SgTemplateParameter::type_parameter;
      param_type = SageBuilder::buildTemplateType(
          SgName("T" + std::to_string(param_position)));
      break;

    case clang::TemplateArgument::Integral:
      // Non-type parameter (e.g., size_t N)
      param_kind = SgTemplateParameter::nontype_parameter;
      param_type = buildTypeFromQualifiedType(arg.getIntegralType());
      if (param_type == nullptr) {
        param_type = SageBuilder::buildIntType();
      }
      break;

    case clang::TemplateArgument::Template: {
      // Template template parameter
      param_kind = SgTemplateParameter::template_parameter;
      std::string param_name = "Template" + std::to_string(param_position);
      SgTemplateType *ttype =
          SageBuilder::buildTemplateType(SgName(param_name));

      // Create SgNonrealDecl
      SgScopeStatement *current_scope = SageBuilder::topScopeStack();
      ROSE_ASSERT(current_scope != NULL);
      SgDeclarationScope *decl_scope = isSgDeclarationScope(current_scope);
      if (decl_scope == NULL) {
        decl_scope = SageBuilder::buildDeclarationScope();
        decl_scope->set_parent(current_scope);
      }

      SgNonrealDecl *nrdecl =
          SageBuilder::buildNonrealDecl(SgName(param_name), decl_scope);
      diagnose_null_scope(nrdecl, "TemplateTemplateArgument");
      // nrdecl->set_type(ttype); // Removed: SgNonrealDecl expects
      // SgNonrealType

      // Translate inner parameters
      clang::TemplateName tname = arg.getAsTemplate();
      clang::TemplateDecl *tdecl = tname.getAsTemplateDecl();

      if (tdecl) {
        auto inner_params = translateTemplateParameterList(
            tdecl->getTemplateParameters(), nrdecl);
        nrdecl->get_tpl_params() = *inner_params;
      }

      SgTemplateParameter *param = SageBuilder::buildTemplateParameter(
          SgTemplateParameter::template_parameter, ttype);
      param->set_templateDeclaration(nrdecl);

      param_list->push_back(param);
      param_position++;
      continue;
    }

    case clang::TemplateArgument::Pack:
      // Parameter pack (e.g., typename ... Args)
      // We infer it as a type parameter pack
      param_kind = SgTemplateParameter::type_parameter;
      param_type = SageBuilder::buildTemplateType(
          SgName("Args" + std::to_string(param_position)));
      if (SgTemplateType *ttype = isSgTemplateType(param_type)) {
        ttype->set_packed(true);
      }
      is_param_pack = true;
      break;

    case clang::TemplateArgument::Expression:
      // Expression argument (e.g., sizeof(T))
      // We infer it as a non-type parameter
      param_kind = SgTemplateParameter::nontype_parameter;
      if (clang::Expr *expr = arg.getAsExpr()) {
        param_type = buildTypeFromQualifiedType(expr->getType());
      } else {
        // Fallback if expression is null (shouldn't happen for Expression kind)
        param_type = SageBuilder::buildIntType();
      }
      if (param_type == nullptr) {
        param_type = SageBuilder::buildIntType();
      }
      break;

    case clang::TemplateArgument::Declaration: {
      // Non-type parameter (e.g., void (*F)())
      param_kind = SgTemplateParameter::nontype_parameter;
      clang::ValueDecl *decl = arg.getAsDecl();
      if (decl) {
        param_type = buildTypeFromQualifiedType(decl->getType());
      } else {
        // Should not happen for Declaration kind
        param_type = SageBuilder::buildVoidType();
      }
      if (param_type == nullptr) {
        param_type = SageBuilder::buildVoidType();
      }
      break;
    }

    case clang::TemplateArgument::NullPtr:
      // Non-type parameter (e.g., nullptr)
      param_kind = SgTemplateParameter::nontype_parameter;
      param_type = buildTypeFromQualifiedType(arg.getNullPtrType());
      if (param_type == nullptr) {
        param_type = SageBuilder::buildNullptrType();
      }
      break;

    case clang::TemplateArgument::StructuralValue:
      // Non-type parameter (C++20 structural)
      param_kind = SgTemplateParameter::nontype_parameter;
      param_type = buildTypeFromQualifiedType(arg.getStructuralValueType());
      if (param_type == nullptr) {
        param_type = SageBuilder::buildIntType();
      }
      break;

    default:
      std::cerr << "Warning: Unsupported template parameter kind: "
                << arg.getKind() << " (Pack=" << clang::TemplateArgument::Pack
                << ", Expression=" << clang::TemplateArgument::Expression
                << ", Template=" << clang::TemplateArgument::Template
                << ", Integral=" << clang::TemplateArgument::Integral
                << ", Type=" << clang::TemplateArgument::Type
                << ", Declaration=" << clang::TemplateArgument::Declaration
                << ", NullPtr=" << clang::TemplateArgument::NullPtr
                << ", Null=" << clang::TemplateArgument::Null << ")"
                << std::endl;
      continue;
    }

    SgTemplateParameter *param =
        SageBuilder::buildTemplateParameter(param_kind, param_type);
    if (param != nullptr) {
      if (is_param_pack) {
        param->set_is_parameter_pack(true);
      }
      if (param_kind == SgTemplateParameter::nontype_parameter) {
        std::string name = "__non_type_param_" + std::to_string(param_position);
        SgInitializedName *init_name =
            SageBuilder::buildInitializedName(SgName(name), param_type);
        param->set_initializedName(init_name);
        init_name->set_parent(param);
        if (is_param_pack) {
          init_name->set_is_parameter_pack(true);
        }
      }
      param_list->push_back(param);
    }
    param_position++;
  }

  return param_list;
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
ClangToSageTranslator::getOrCreateTemplateDeclaration(
    const std::string &template_name,
    const clang::TemplateSpecializationType *clang_type,
    SgScopeStatement *scope_override) {

  std::string cache_key = normalizeTemplateDeclCacheKey(template_name);
  auto get_template_source_range =
      [](clang::TemplateDecl *clang_template_decl) -> clang::SourceRange {
    if (clang_template_decl == nullptr) {
      return clang::SourceRange();
    }

    clang::SourceRange range = clang_template_decl->getSourceRange();
    if (range.isValid()) {
      return range;
    }

    if (clang::ClassTemplateDecl *class_template =
            llvm::dyn_cast<clang::ClassTemplateDecl>(clang_template_decl)) {
      if (clang::CXXRecordDecl *templated_decl =
              class_template->getTemplatedDecl()) {
        range = templated_decl->getSourceRange();
      }
    }

    return range;
  };

  auto normalize_template_decl_source =
      [&](SgTemplateClassDeclaration *template_decl,
          clang::TemplateDecl *clang_template_decl) {
        if (template_decl == nullptr || clang_template_decl == nullptr) {
          return;
        }

        clang::SourceRange range =
            get_template_source_range(clang_template_decl);
        if (!range.isValid()) {
          return;
        }

        auto apply_if_missing = [&](SgTemplateClassDeclaration *candidate) {
          if (candidate == nullptr) {
            return;
          }
          Sg_File_Info *fi = candidate->get_file_info();
          if (fi != nullptr && fi->get_line() > 0 &&
              !fi->isCompilerGenerated()) {
            return;
          }
          applySourceRange(candidate, range);
        };

        apply_if_missing(template_decl);
        if (SgTemplateClassDeclaration *first = isSgTemplateClassDeclaration(
                template_decl->get_firstNondefiningDeclaration())) {
          if (first != template_decl) {
            apply_if_missing(first);
          }
        }
        if (SgTemplateClassDeclaration *def = isSgTemplateClassDeclaration(
                template_decl->get_definingDeclaration())) {
          if (def != template_decl) {
            apply_if_missing(def);
          }
        }
      };

  auto lookup_actual_template_decl =
      [&](clang::TemplateDecl *clang_template_decl)
      -> SgTemplateClassDeclaration * {
    if (clang_template_decl == nullptr) {
      return nullptr;
    }

    if (SgTemplateClassDeclaration *mapped =
            isSgTemplateClassDeclaration(lookupSgDeclarationForClangDecl(
                llvm::cast<clang::Decl>(clang_template_decl),
                /*allow_on_demand=*/true))) {
      normalize_template_decl_source(mapped, clang_template_decl);
      return mapped;
    }

    if (clang::ClassTemplateDecl *class_template =
            llvm::dyn_cast<clang::ClassTemplateDecl>(clang_template_decl)) {
      if (SgTemplateClassDeclaration *mapped =
              lookupTranslatedTemplateDeclarationForRecord(
                  class_template->getTemplatedDecl())) {
        normalize_template_decl_source(mapped, clang_template_decl);
        return mapped;
      }
    }

    return nullptr;
  };

  clang::TemplateDecl *source_template_decl = nullptr;
  if (clang_type != nullptr) {
    if (clang::TemplateDecl *clang_template_decl =
            clang_type->getTemplateName().getAsTemplateDecl()) {
      source_template_decl = clang_template_decl;
      if (SgTemplateClassDeclaration *mapped =
              lookup_actual_template_decl(clang_template_decl)) {
        p_template_decl_cache[cache_key] = mapped;
        return mapped;
      }
    }
  }

  auto it = p_template_decl_cache.find(cache_key);
  if (it != p_template_decl_cache.end()) {
    return it->second;
  }

  std::vector<std::string> components =
      splitQualifiedNameOutsideTemplates(template_name);
  std::string base_name =
      components.empty() ? template_name : components.back();
  base_name = stripTemplateArgs(base_name);

  SgScopeStatement *scope = scope_override;
  if (scope == nullptr) {
    scope = getGlobalScope();
    if (components.size() > 1) {
      for (size_t i = 0; i + 1 < components.size(); ++i) {
        std::string component = trimWhitespace(components[i]);
        if (component.empty()) {
          continue;
        }
        bool has_template = hasTemplateSyntaxComponent(component);
        std::string name = stripTemplateArgs(component);
        if (name.empty()) {
          continue;
        }

        if (has_template) {
          SgNonrealType *nr_type =
              SageBuilder::buildNonrealType(SgName(name), scope, nullptr);
          SgNonrealDecl *nr_decl = isSgNonrealDecl(nr_type->get_declaration());
          ROSE_ASSERT(nr_decl != nullptr);
          scope = nr_decl->get_nonreal_decl_scope();
          continue;
        }

        SgNamespaceSymbol *ns_sym =
            scope->lookup_namespace_symbol(SgName(name));
        if (ns_sym) {
          scope = ns_sym->get_declaration()->get_definition();
        } else {
          SgNamespaceDeclarationStatement *ns_decl =
              SageBuilder::buildNamespaceDeclaration(SgName(name), scope);
          scope = ns_decl->get_definition();
        }
      }
    }
  }

  // Build template parameters
  auto params = buildTemplateParameters(clang_type);

  // Create empty template argument list for primary template
  SgTemplateArgumentPtrList empty_args;

  // Create template class declaration
  SgTemplateClassDeclaration *template_decl =
      SageBuilder::buildNondefiningTemplateClassDeclaration_nfi(
          SgName(base_name),
          SgClassDeclaration::e_class, // Assume class (could be struct)
          scope, params.get(),
          &empty_args // No specialization arguments for primary template
      );

  // REX FIX: Ensure firstNondefiningDeclaration is set to avoid unparser
  // crash (ua test).
  template_decl->set_firstNondefiningDeclaration(template_decl);

  // Synthetic namespace/global template declarations participate as the
  // original template of later instantiations, so they must not remain marked
  // compiler-generated or AstConsistency will reject them.
  template_decl->setForward();
  template_decl->set_isUnNamed(false);
  normalize_template_decl_source(template_decl, source_template_decl);
  if (isSgGlobal(scope) != nullptr ||
      isSgNamespaceDefinitionStatement(scope) != nullptr) {
    template_decl->unsetCompilerGenerated();
    template_decl->unsetFrontendSpecific();
    if (Sg_File_Info *fi = template_decl->get_file_info()) {
      fi->unsetCompilerGenerated();
      fi->unsetFrontendSpecific();
      fi->unsetOutputInCodeGeneration();
    }
    if (Sg_File_Info *fi = template_decl->get_endOfConstruct()) {
      fi->unsetCompilerGenerated();
      fi->unsetFrontendSpecific();
      fi->unsetOutputInCodeGeneration();
    }
  } else if (template_decl->get_file_info() != nullptr) {
    template_decl->get_file_info()->setCompilerGenerated();
  }
  if (template_decl->get_file_info() != nullptr) {
    template_decl->get_file_info()->unsetOutputInCodeGeneration();
  }

  // Ensure the template declaration scope matches the enclosing scope so
  // symbol insertion does not mismatch the declaration's scope.
  if (scope != nullptr) {
    template_decl->set_scope(scope);
    if (template_decl->get_parent() == nullptr) {
      template_decl->set_parent(scope);
    }
  }

  // Do not manually insert a SgClassSymbol here.
  // SageBuilder::buildNondefiningTemplateClassDeclaration_nfi() installs the
  // appropriate SgTemplateClassSymbol; inserting a SgClassSymbol for a
  // SgTemplateClassDeclaration violates AST invariants and triggers
  // AstConsistencyTests assertions.

  // Cache it
  p_template_decl_cache[cache_key] = template_decl;

  return template_decl;
}

namespace {
void setIntegralLiteralValueString(SgExpression *expr,
                                   const std::string &text) {
  if (expr == nullptr) {
    return;
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
  if (int_type != nullptr && isSgTypeBool(int_type)) {
    return SageBuilder::buildBoolValExp(value.getBoolValue());
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
      expr = SageBuilder::buildCharVal(static_cast<char>(raw));
    } else if (isSgTypeSignedChar(base_type) != nullptr) {
      expr = SageBuilder::buildSignedCharVal(
          static_cast<signed char>(get_signed_value()));
    } else if (isSgTypeUnsignedChar(base_type) != nullptr) {
      expr = SageBuilder::buildUnsignedCharVal(
          static_cast<unsigned char>(get_unsigned_value()));
    } else if (isSgTypeWchar(base_type) != nullptr) {
      unsigned long long raw =
          value.isSigned() ? static_cast<unsigned long long>(get_signed_value())
                           : get_unsigned_value();
      expr = SageBuilder::buildWcharVal(static_cast<wchar_t>(raw));
    } else if (isSgTypeChar16(base_type) != nullptr) {
      expr = SageBuilder::buildChar16Val(
          static_cast<unsigned short>(get_unsigned_value()));
    } else if (isSgTypeChar32(base_type) != nullptr) {
      expr = SageBuilder::buildChar32Val(
          static_cast<unsigned int>(get_unsigned_value()));
    } else if (isSgTypeShort(base_type) != nullptr) {
      expr = SageBuilder::buildShortVal(static_cast<short>(get_signed_value()));
    } else if (isSgTypeUnsignedShort(base_type) != nullptr) {
      expr = SageBuilder::buildUnsignedShortVal(
          static_cast<unsigned short>(get_unsigned_value()));
    } else if (isSgTypeInt(base_type) != nullptr) {
      expr = SageBuilder::buildIntVal(static_cast<int>(get_signed_value()));
    } else if (isSgTypeUnsignedInt(base_type) != nullptr) {
      expr = SageBuilder::buildUnsignedIntVal(
          static_cast<unsigned int>(get_unsigned_value()));
    } else if (isSgTypeLong(base_type) != nullptr) {
      expr =
          SageBuilder::buildLongIntVal(static_cast<long>(get_signed_value()));
    } else if (isSgTypeUnsignedLong(base_type) != nullptr) {
      expr = SageBuilder::buildUnsignedLongVal(
          static_cast<unsigned long>(get_unsigned_value()));
    } else if (isSgTypeLongLong(base_type) != nullptr) {
      expr = SageBuilder::buildLongLongIntVal(get_signed_value());
    } else if (isSgTypeUnsignedLongLong(base_type) != nullptr) {
      expr = SageBuilder::buildUnsignedLongLongIntVal(get_unsigned_value());
    }
  }

  if (expr == NULL) {
    if (value.isSigned()) {
      expr = SageBuilder::buildLongLongIntVal(get_signed_value());
    } else {
      expr = SageBuilder::buildUnsignedLongLongIntVal(get_unsigned_value());
    }
  }

  if (expr != NULL && !is_character_like_type) {
    llvm::SmallString<64> buf;
    value.toString(buf, 10, value.isSigned());
    setIntegralLiteralValueString(expr, std::string(buf.begin(), buf.end()));
  }

  return expr;
}

static bool templateArgumentNeedsExplicitAddressOf(SgType *param_type,
                                                   SgExpression *expr) {
  if (param_type == nullptr || expr == nullptr ||
      isSgAddressOfOp(expr) != nullptr) {
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
    for (const clang::TemplateArgument &pack_arg :
         markClangTemplateArgumentArrayDefined(defined_arg.pack_elements())) {
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
  if (defined_arg.isPackExpansion()) {
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
      return SageBuilder::buildEnumVal_nfi(
          enum_value, enum_decl, SgName(enum_const_decl->getNameAsString()));
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
            tokens.push_back(ns->getNameAsString());
          }
          continue;
        }
        if (const clang::NamespaceAliasDecl *alias =
                llvm::dyn_cast<clang::NamespaceAliasDecl>(ctx)) {
          if (!alias->getName().empty()) {
            tokens.push_back(alias->getNameAsString());
          }
          continue;
        }
        if (const clang::RecordDecl *record =
                llvm::dyn_cast<clang::RecordDecl>(ctx)) {
          if (!record->getName().empty()) {
            tokens.push_back(record->getNameAsString());
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
      SgExpression *expr = SageBuilder::buildVarRefExp(var_sym);
      apply_explicit_qualifier_from_decl_context(expr, decl);
      return expr;
    }
    if (SgMemberFunctionSymbol *member_sym = isSgMemberFunctionSymbol(sym)) {
      SgExpression *expr =
          SageBuilder::buildMemberFunctionRefExp_nfi(member_sym, false, false);
      apply_explicit_qualifier_from_decl_context(expr, decl);
      return expr;
    }
    if (SgFunctionSymbol *func_sym = isSgFunctionSymbol(sym)) {
      SgExpression *expr = SageBuilder::buildFunctionRefExp(func_sym);
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
      return buildIntegralTemplateArgExpr(value.getInt(), value_type);
    }
    if (value.isFloat()) {
      llvm::SmallString<64> buf;
      value.getFloat().toString(buf);
      std::string text(buf.begin(), buf.end());

      if (isSgTypeFloat(value_type)) {
        return SageBuilder::buildFloatVal_nfi(0.0f, text);
      }
      if (isSgTypeLongDouble(value_type)) {
        return SageBuilder::buildLongDoubleVal_nfi(0.0L, text);
      }
      return SageBuilder::buildDoubleVal_nfi(0.0, text);
    }
    if (value.isLValue()) {
      clang::APValue::LValueBase base = value.getLValueBase();
      if (const clang::ValueDecl *decl =
              base.dyn_cast<const clang::ValueDecl *>()) {
        return build_decl_expr(const_cast<clang::ValueDecl *>(decl), nullptr);
      }
      if (const clang::Expr *expr = base.dyn_cast<const clang::Expr *>()) {
        SgNode *sg_node = Traverse(const_cast<clang::Expr *>(expr));
        return isSgExpression(sg_node);
      }
    }
    return nullptr;
  };
  auto build_template_decl_argument =
      [&](clang::TemplateName template_name) -> SgDeclarationStatement * {
    clang::TemplateDecl *template_decl = template_name.getAsTemplateDecl();
    SgDeclarationStatement *sg_decl = nullptr;
    if (template_decl != nullptr) {
      SgNode *traverse_result = TraverseOnDemand(template_decl);
      sg_decl = isSgDeclarationStatement(traverse_result);

      // Traverse returns SgTemplateParameter for TemplateTemplateParmDecl via
      // VisitTemplateTemplateParmDecl. SgTemplateParameter is NOT an
      // SgDeclarationStatement, but it holds the SgNonrealDecl we need.
      if (SgTemplateParameter *param = isSgTemplateParameter(traverse_result)) {
        if (SgDeclarationStatement *inner_decl =
                param->get_templateDeclaration()) {
          if (isSgNonrealDecl(inner_decl)) {
            sg_decl = inner_decl;
          }
        }
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
      SgNonrealType *nr_type = buildNonrealTypeFromNestedNameSpecifier(
          qualifier, scope, SgName(name_str), nullptr);
      if (SgNonrealDecl *nr_decl =
              isSgNonrealDecl(nr_type ? nr_type->get_declaration() : nullptr)) {
        if (SgTemplateDeclaration *template_decl_stmt =
                isSgTemplateDeclaration(sg_decl)) {
          nr_decl->set_templateDeclaration(template_decl_stmt);
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
      SgNonrealType *nr_type =
          SageBuilder::buildNonrealType(SgName(name_str), scope, nullptr);
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
    SgType *arg_type = buildTypeFromQualifiedType(arg_qt);
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

        SgTemplateArgumentPtrList tpl_args = buildTemplateArguments(tst);
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

        SgNonrealType *nrtype = nullptr;
        if (qualifier) {
          nrtype = buildNonrealTypeFromNestedNameSpecifier(
              qualifier, scope, SgName(base_name),
              tpl_args.empty() ? nullptr : &tpl_args);
        } else {
          nrtype = SageBuilder::buildNonrealType(
              SgName(base_name), scope, tpl_args.empty() ? nullptr : &tpl_args);
        }
        if (SgNonrealDecl *nrdecl =
                isSgNonrealDecl(nrtype ? nrtype->get_declaration() : nullptr)) {
          if (has_template_keyword) {
            nrdecl->set_has_template_keyword(true);
          }
        }
        return nrtype;
      };

      bool needs_rebuild = false;
      bool is_alias = arg_tst->isTypeAlias();
      if (SgNonrealType *nrtype = isSgNonrealType(arg_type)) {
        if (SgNonrealDecl *nrdecl =
                isSgNonrealDecl(nrtype->get_declaration())) {
          if (nrdecl->get_tpl_args().empty() &&
              (is_alias || !markClangTemplateArgumentArrayDefined(
                                arg_tst->template_arguments())
                                .empty())) {
            needs_rebuild = true;
          }
        }
      } else if (isSgTypedefType(arg_type) != nullptr) {
        if (is_alias || !markClangTemplateArgumentArrayDefined(
                             arg_tst->template_arguments())
                             .empty()) {
          needs_rebuild = true;
        }
      }

      if (needs_rebuild) {
        if (SgType *spec_type = build_template_specialization_type(arg_tst)) {
          arg_type = spec_type;
        }
      }
    }

    if (arg_type != NULL) {
      sg_arg = new SgTemplateArgument(arg_type, explicitlySpecified);
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
    }

    if (value_expr != nullptr) {
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
      param_type = SageBuilder::buildIntType();
    }
    SgInitializedName *init_name = nullptr;
    SgExpression *decl_expr = build_decl_expr(decl, &init_name);
    if (decl_expr != nullptr &&
        templateArgumentNeedsExplicitAddressOf(param_type, decl_expr)) {
      decl_expr = SageBuilder::buildAddressOfOp(decl_expr);
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
      value_expr = SageBuilder::buildAddressOfOp(value_expr);
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
      MLOG_WARN_C(MLOG_FRONTEND,
                  "Warning: Failed to translate template declaration for "
                  "template argument.\n");
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
      MLOG_WARN_C(MLOG_FRONTEND,
                  "Warning: Failed to translate template declaration for "
                  "template argument.\n");
    }
    break;
  }

  case clang::TemplateArgument::Expression: {
    clang::Expr *clang_expr = const_cast<clang::Expr *>(
        markClangExprObjectDefinedByClass(defined_arg.getAsExpr()));
    if (clang_expr != nullptr) {
      SgNode *node = Traverse(clang_expr);
      if (SgExpression *sg_expr = isSgExpression(node)) {
        sg_arg = new SgTemplateArgument(sg_expr, explicitlySpecified);
        if (expressionReferencesTemplateParameterPack(clang_expr)) {
          sg_arg->set_is_pack_element(true);
        }
      }
    }
    break;
  }

  case clang::TemplateArgument::Pack: {
    SgTemplateArgument *pack_marker = new SgTemplateArgument();
    pack_marker->set_argumentType(
        SgTemplateArgument::start_of_pack_expansion_argument);
    sg_arg = pack_marker;
    break;
  }

  case clang::TemplateArgument::Null:
    return nullptr;

  default:
    std::cerr << "Warning: Unsupported template argument kind: "
              << defined_arg.getKind() << "\n";
    break;
  }

  if (sg_arg != nullptr && defined_arg.containsUnexpandedParameterPack() &&
      sg_arg->get_argumentType() !=
          SgTemplateArgument::start_of_pack_expansion_argument) {
    sg_arg->set_is_pack_element(true);
  }

  if (sg_arg != nullptr && sg_arg->get_parent() == nullptr) {
    SgNode *fallback_parent = nullptr;
    if (p_sage_source_file != nullptr) {
      fallback_parent = p_sage_source_file;
    }
    if (fallback_parent == nullptr) {
      fallback_parent = SageBuilder::topScopeStack();
    }
    if (fallback_parent == nullptr) {
      fallback_parent = getGlobalScope();
    }
    if (fallback_parent != nullptr) {
      sg_arg->set_parent(fallback_parent);
    }
  }

  return sg_arg;
}

SgType *ClangToSageTranslator::translateTypeTemplateArgument(
    const clang::TemplateArgumentLoc &arg_loc) {
  clang::TemplateArgument arg =
      readClangApiValueDefined([&]() { return arg_loc.getArgument(); });
  markClangTemplateArgumentDefined(arg);

  if (arg.getKind() != clang::TemplateArgument::Type) {
    return nullptr;
  }

  if (const clang::TypeSourceInfo *type_info =
          markClangAstObjectDefined(readClangApiValueDefined(
              [&]() { return arg_loc.getTypeSourceInfo(); }))) {
    const clang::TypeLoc written_type_loc =
        readClangApiValueDefined([&]() { return type_info->getTypeLoc(); });
    markClangTypeLocDataDefined(written_type_loc);
    const bool written_type_spells_elaborated_keyword =
        typeLocSpellsElaboratedKeyword(written_type_loc);

    SgType *arg_type = buildTypeFromTypeLoc(written_type_loc);
    if (arg_type != nullptr) {
      auto collect_template_args_from_type_loc =
          [&](SgTemplateArgumentPtrList &tpl_args,
              bool *has_explicit_empty = nullptr) -> bool {
        clang::TypeLoc type_loc = written_type_loc;
        while (!type_loc.isNull()) {
          if (auto tst_loc =
                  type_loc.getAs<clang::TemplateSpecializationTypeLoc>()) {
            const unsigned arg_count = tst_loc.getNumArgs();
            for (unsigned i = 0; i < arg_count; ++i) {
              appendTemplateArguments(tpl_args, tst_loc.getArgLoc(i), true);
            }
            if (has_explicit_empty != nullptr) {
              *has_explicit_empty =
                  (arg_count == 0 && tst_loc.getLAngleLoc().isValid());
            }
            return true;
          }
          type_loc = type_loc.getNextTypeLoc();
        }
        return false;
      };

      auto build_template_specialization_type =
          [&](const clang::TemplateSpecializationType *tst) -> SgType * {
        if (tst == nullptr) {
          return nullptr;
        }
        tst = static_cast<const clang::TemplateSpecializationType *>(
            markClangTypeObjectDefinedByClass(tst));
        clang::TemplateName tname =
            markClangTemplateNameDefined(tst->getTemplateName());
        std::string base_name = getTemplateNameBase(tname);
        if (base_name.empty()) {
          return nullptr;
        }

        SgTemplateArgumentPtrList tpl_args;
        bool has_explicit_empty_template_args = false;
        if (!collect_template_args_from_type_loc(
                tpl_args, &has_explicit_empty_template_args)) {
          tpl_args = buildTemplateArguments(tst);
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

        SgScopeStatement *scope = SageBuilder::topScopeStack();
        if (scope == nullptr) {
          scope = getGlobalScope();
        }

        const bool has_template_args =
            !tpl_args.empty() || has_explicit_empty_template_args;
        SgNonrealType *nrtype = nullptr;
        if (qualifier) {
          nrtype = buildNonrealTypeFromNestedNameSpecifier(
              qualifier, scope, SgName(base_name),
              has_template_args ? &tpl_args : nullptr);
        } else {
          nrtype = SageBuilder::buildNonrealType(SgName(base_name), scope,
                                                 has_template_args ? &tpl_args
                                                                   : nullptr);
        }
        if (SgNonrealDecl *nrdecl =
                isSgNonrealDecl(nrtype ? nrtype->get_declaration() : nullptr)) {
          if (has_template_keyword) {
            nrdecl->set_has_template_keyword(true);
          }
        }
        return nrtype;
      };

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

      const clang::TemplateSpecializationType *arg_tst = nullptr;
      clang::QualType arg_qt = markClangQualTypeDefined(arg.getAsType());
      const clang::Type *arg_type_ptr = arg_qt.getTypePtrOrNull();
      if (arg_type_ptr != nullptr) {
        arg_tst =
            llvm::dyn_cast<clang::TemplateSpecializationType>(arg_type_ptr);
      }

      SgType *resolved_type = arg_type;
      if (arg_tst != nullptr) {
        if (SgType *spelled_type =
                build_template_specialization_type(arg_tst)) {
          resolved_type = spelled_type;
        }
      } else {
        if (p_force_written_tag_type_qualification_depth != 0 &&
            isSgNonrealType(resolved_type) != nullptr &&
            typeLocQualifierLoc(written_type_loc)) {
          return resolved_type;
        }
        if (SgType *qualified_tag_type =
                build_translated_qualified_tag_type()) {
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
              p_decl_translation_in_progress.find(
                  const_cast<clang::RecordDecl *>(record_decl)) ==
                  p_decl_translation_in_progress.end()) {
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
      }

      return resolved_type;
    }
  }

  return buildTypeFromQualifiedType(markClangQualTypeDefined(arg.getAsType()));
}

SgTemplateArgument *ClangToSageTranslator::translateTemplateArgument(
    const clang::TemplateArgumentLoc &arg_loc, bool explicitlySpecified) {
  clang::TemplateArgument arg =
      readClangApiValueDefined([&]() { return arg_loc.getArgument(); });
  markClangTemplateArgumentDefined(arg);

  switch (arg.getKind()) {
  case clang::TemplateArgument::Type: {
    if (SgType *resolved_type = translateTypeTemplateArgument(arg_loc)) {
      auto strip_modifier_layers = [](SgType *type) -> SgType * {
        while (SgModifierType *modifier = isSgModifierType(type)) {
          type = modifier->get_base_type();
        }
        return type;
      };

      SgType *semantic_type =
          buildTypeFromQualifiedType(markClangQualTypeDefined(arg.getAsType()));
      SgType *semantic_core = strip_modifier_layers(semantic_type);
      SgType *resolved_core = strip_modifier_layers(resolved_type);
      const bool lost_outer_wrapper =
          (isSgPointerType(semantic_core) != nullptr &&
           isSgPointerType(resolved_core) == nullptr) ||
          (isSgPointerMemberType(semantic_core) != nullptr &&
           isSgPointerMemberType(resolved_core) == nullptr) ||
          (isSgReferenceType(semantic_core) != nullptr &&
           isSgReferenceType(resolved_core) == nullptr) ||
          (isSgRvalueReferenceType(semantic_core) != nullptr &&
           isSgRvalueReferenceType(resolved_core) == nullptr) ||
          (isSgArrayType(semantic_core) != nullptr &&
           isSgArrayType(resolved_core) == nullptr);
      if (lost_outer_wrapper && semantic_type != nullptr) {
        resolved_type = semantic_type;
      }

      SgTemplateArgument *template_argument =
          new SgTemplateArgument(resolved_type, explicitlySpecified);

      if (const clang::TypeSourceInfo *type_info =
              markClangAstObjectDefined(readClangApiValueDefined(
                  [&]() { return arg_loc.getTypeSourceInfo(); }));
          type_info != nullptr && isSgNonrealType(resolved_type) == nullptr) {
        clang::TypeLoc type_loc =
            readClangApiValueDefined([&]() { return type_info->getTypeLoc(); });
        markClangTypeLocDataDefined(type_loc);
        clang::NestedNameSpecifierLoc qualifier_loc =
            typeLocQualifierLoc(type_loc);
        clang::NestedNameSpecifier qualifier =
            markClangNestedNameSpecifierDefined(
                qualifier_loc.getNestedNameSpecifier());
        template_argument->set_name_qualification_length(
            nestedNameSpecifierComponentCount(qualifier));
        template_argument->set_global_qualification_required(
            nestedNameSpecifierLocHasExplicitGlobal(qualifier_loc));
        template_argument->set_type_elaboration_required(
            typeLocSpellsElaboratedKeyword(type_loc));
      }

      return template_argument;
    }
    break;
  }

  case clang::TemplateArgument::Expression: {
    // Prefer source-expression spelling from TemplateArgumentLoc. Canonical
    // argument expressions may drop dependent qualification.
    const clang::Expr *expr = arg_loc.getSourceExpression();
    if (expr == nullptr) {
      expr = markClangExprObjectDefinedByClass(arg.getAsExpr());
    }
    if (expr != nullptr) {
      SgNode *node = Traverse(const_cast<clang::Expr *>(expr));
      if (SgExpression *sg_expr = isSgExpression(node)) {
        return new SgTemplateArgument(sg_expr, explicitlySpecified);
      }
    }
    break;
  }

  case clang::TemplateArgument::Integral: {
    // Preserve source-level spelling for integral non-type template arguments
    // (e.g., enum constants) when available.
    if (const clang::Expr *expr = arg_loc.getSourceIntegralExpression()) {
      SgNode *node = Traverse(const_cast<clang::Expr *>(expr));
      if (SgExpression *sg_expr = isSgExpression(node)) {
        SgType *int_type = buildTypeFromQualifiedType(
            markClangQualTypeDefined(arg.getIntegralType()));
        SgTemplateArgument *sg_arg = new SgTemplateArgument(
            SgTemplateArgument::nontype_argument,
            /*isArrayBoundUnknownType=*/false, int_type, sg_expr,
            /*templateDeclaration=*/nullptr, explicitlySpecified);
        sg_expr->set_parent(sg_arg);
        return sg_arg;
      }
    }
    break;
  }

  case clang::TemplateArgument::Declaration: {
    if (const clang::Expr *expr = arg_loc.getSourceDeclExpression()) {
      SgNode *node = Traverse(const_cast<clang::Expr *>(expr));
      if (SgExpression *sg_expr = isSgExpression(node)) {
        clang::QualType param_qual_type =
            markClangQualTypeDefined(arg.getParamTypeForDecl());
        SgType *param_type = buildTypeFromQualifiedType(param_qual_type);
        if (param_type == nullptr) {
          param_type = SageBuilder::buildIntType();
        }
        if (templateArgumentNeedsExplicitAddressOf(param_type, sg_expr)) {
          sg_expr = SageBuilder::buildAddressOfOp(sg_expr);
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
            arg_loc.getTemplateQualifierLoc();
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
            SgNonrealType *nr_type = buildNonrealTypeFromNestedNameSpecifier(
                qualifier, scope, SgName(name_str), nullptr);
            if (SgNonrealDecl *nr_decl = isSgNonrealDecl(
                    nr_type ? nr_type->get_declaration() : nullptr)) {
              if (SgTemplateDeclaration *template_decl =
                      isSgTemplateDeclaration(
                          sg_arg->get_templateDeclaration())) {
                nr_decl->set_templateDeclaration(template_decl);
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

  if (defined_arg.getKind() == clang::TemplateArgument::Pack) {
    auto elements =
        markClangTemplateArgumentArrayDefined(defined_arg.pack_elements());
    if (elements.empty()) {
      SgTemplateArgument *pack_marker = new SgTemplateArgument();
      pack_marker->set_argumentType(
          SgTemplateArgument::start_of_pack_expansion_argument);
      arg_list.push_back(pack_marker);
      ensureTemplateArgumentParents(arg_list);
    } else {
      bool dependent_pack = defined_arg.containsUnexpandedParameterPack();
      if (!dependent_pack && elements.size() == 1) {
        dependent_pack = defined_arg.isInstantiationDependent() ||
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
  const clang::TemplateArgument &arg =
      markClangTemplateArgumentDefined(arg_loc.getArgument());

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
      SgTemplateArgument *pack_marker = new SgTemplateArgument();
      pack_marker->set_argumentType(
          SgTemplateArgument::start_of_pack_expansion_argument);
      arg_list.push_back(pack_marker);
      ensureTemplateArgumentParents(arg_list);
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
    const clang::TemplateSpecializationType *clang_type) {

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

  if (use_full_args) {
    for (const clang::TemplateArgument &arg :
         markClangTemplateArgumentArrayDefined(full_args->asArray())) {
      appendTemplateArguments(arg_list, markClangTemplateArgumentDefined(arg),
                              false);
    }
  } else {
    for (const clang::TemplateArgument &arg : args_as_written) {
      appendTemplateArguments(arg_list, markClangTemplateArgumentDefined(arg),
                              true);
    }
  }

  ensureTemplateArgumentParents(arg_list);
  return arg_list;
}

SgTemplateArgumentPtrList ClangToSageTranslator::buildTemplateArguments(
    const clang::TemplateArgumentListInfo &arg_info, bool explicitlySpecified) {
  SgTemplateArgumentPtrList arg_list;

  for (const clang::TemplateArgumentLoc &arg_loc : arg_info.arguments()) {
    appendTemplateArguments(arg_list, arg_loc, explicitlySpecified);
  }

  ensureTemplateArgumentParents(arg_list);
  return arg_list;
}

SgTemplateArgumentPtrList ClangToSageTranslator::buildTemplateArguments(
    const clang::TemplateArgumentList &args, size_t explicit_count) {
  SgTemplateArgumentPtrList arg_list;
  const clang::TemplateArgumentList *defined_args =
      markClangTemplateArgumentListDefined(&args);
  for (unsigned i = 0; i < defined_args->size(); ++i) {
    appendTemplateArguments(
        arg_list, markClangTemplateArgumentDefined(defined_args->get(i)),
        false);
  }

  applyExplicitTemplateArgumentFlags(arg_list, explicit_count);
  ensureTemplateArgumentParents(arg_list);
  return arg_list;
}

void ClangToSageTranslator::ensureTemplateArgumentParents(
    SgTemplateArgumentPtrList &args) {
  SgNode *fallback_parent = nullptr;
  if (p_sage_source_file != NULL) {
    fallback_parent = p_sage_source_file;
  }
  if (fallback_parent == nullptr) {
    fallback_parent = SageBuilder::topScopeStack();
  }
  if (fallback_parent == nullptr) {
    fallback_parent = getGlobalScope();
  }
  if (fallback_parent == nullptr) {
    return;
  }

  for (SgTemplateArgument *arg : args) {
    if (arg != NULL && arg->get_parent() == NULL) {
      arg->set_parent(fallback_parent);
    }
  }
}

void ClangToSageTranslator::applyExplicitTemplateArgumentFlags(
    SgTemplateArgumentPtrList &args, size_t explicit_count) {
  if (explicit_count == 0) {
    return;
  }

  size_t limit = explicit_count < args.size() ? explicit_count : args.size();
  for (size_t i = 0; i < limit; ++i) {
    SgTemplateArgument *arg = args[i];
    if (arg != NULL && !arg->get_explicitlySpecified()) {
      arg->set_explicitlySpecified(true);
    }
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

SgNonrealType *
ClangToSageTranslator::buildNonrealTypeForNestedNameSpecifierType(
    const clang::Type *clang_type, SgScopeStatement *scope,
    bool prefer_current_scope) {
  clang_type = markClangTypeObjectDefinedByClass(clang_type);
  if (clang_type == nullptr) {
    return nullptr;
  }

  if (const clang::SubstTemplateTypeParmType *subst =
          llvm::dyn_cast<clang::SubstTemplateTypeParmType>(clang_type)) {
    subst = llvm::dyn_cast_or_null<clang::SubstTemplateTypeParmType>(
        markClangTypeObjectDefinedByClass(subst));
    clang::QualType replacement =
        markClangQualTypeDefined(subst->getReplacementType());
    return buildNonrealTypeForNestedNameSpecifierType(
        replacement.getTypePtrOrNull(), scope, prefer_current_scope);
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
        name_str = "template_type_param_" + std::to_string(decl->getDepth()) +
                   "_" + std::to_string(decl->getIndex());
      }
    }
    if (name_str.empty()) {
      if (const clang::IdentifierInfo *id = pack->getIdentifier()) {
        name_str = normalizeTemplateTypeParamName(id->getName().str());
      }
    }
    if (name_str.empty()) {
      name_str = "template_type_param_" + std::to_string(pack->getIndex());
    }
    ROSE_ASSERT(!name_str.empty());

    SgNonrealType *nrtype =
        SageBuilder::buildNonrealType(SgName(name_str), scope, nullptr);
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
    if (prefer_current_scope) {
      qualifier = std::nullopt;
    }
    if (qualifier) {
      return buildNonrealTypeFromNestedNameSpecifier(
          qualifier, scope, SgName(base_name), tpl_args);
    }
    return SageBuilder::buildNonrealType(SgName(base_name), scope, tpl_args);
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
      p_pending_nonreal_template_decl_links[nrdecl] =
          const_cast<clang::Decl *>(llvm::cast<clang::Decl>(decl));
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
  auto fallback_type_name = [&](const clang::Type *type) -> std::string {
    type = markClangTypeObjectDefinedByClass(type);
    if (type == nullptr) {
      return "";
    }
    const char *class_name = type->getTypeClassName();
    std::string result = "__";
    if (class_name != nullptr && class_name[0] != '\0') {
      result += class_name;
    } else {
      result += "unknown_type";
    }
    result += "_" + Rose::StringUtility::numberToString(
                        reinterpret_cast<uintptr_t>(type));
    return result;
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
    std::string base_name = getTemplateNameBase(tname);
    ROSE_ASSERT(!base_name.empty());

    SgTemplateArgumentPtrList tpl_args = buildTemplateArguments(tst);
    applyExplicitTemplateArgumentFlags(tpl_args, tpl_args.size());
    clang::NestedNameSpecifier qualifier = std::nullopt;
    bool has_template_keyword = false;
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

    clang::TemplateDecl *template_decl = nullptr;
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
    template_decl = resolve_template_decl(tname);
    SgDeclarationStatement *translated_decl = nullptr;
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
      if (tst->isTypeAlias()) {
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
      if (translated_decl == nullptr) {
        translated_decl = materialize_translated_template_decl(translated_qt);
      }
      if (translated_decl_key == nullptr && template_decl != nullptr) {
        translated_decl_key = const_cast<clang::Decl *>(
            markClangDeclObjectDefinedByKind(resolve_template_decl(tname)));
      }
    }
    SgNonrealType *nrtype = nullptr;
    if (qualifier) {
      nrtype = build_with_qualifier(qualifier, base_name, &tpl_args);
    } else {
      clang::NestedNameSpecifier ns_qualifier = std::nullopt;
      clang::DeclContext *template_decl_context =
          template_decl != nullptr ? markClangDeclContextObjectDefined(
                                         template_decl->getDeclContext())
                                   : nullptr;
      if (!prefer_current_scope && template_decl_context != nullptr &&
          p_compiler_instance != nullptr &&
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
        const bool scope_is_inside_class =
            qualification_scope != nullptr &&
            SageInterface::getEnclosingClassDefinition(qualification_scope,
                                                       true) != nullptr;
        if (!scope_within_namespace_chain || !scope_is_inside_class) {
          ns_qualifier = buildNamespaceQualifierForDeclContext(
              template_decl_context, p_compiler_instance->getASTContext());
        }
      }
      if (ns_qualifier) {
        nrtype = build_with_qualifier(ns_qualifier, base_name, &tpl_args);
      } else {
        SgScopeStatement *template_scope = scope;
        if (!prefer_current_scope && template_decl != nullptr) {
          if (clang::DeclContext *decl_context =
                  markClangDeclContextObjectDefined(
                      template_decl->getDeclContext())) {
            if (SgScopeStatement *resolved_scope =
                    resolveScopeFromDeclContext(decl_context, nullptr)) {
              template_scope = resolved_scope;
            } else {
              clang::DeclContext *scope_ctx = decl_context;
              while (scope_ctx != nullptr && !scope_ctx->isNamespace() &&
                     !scope_ctx->isTranslationUnit()) {
                scope_ctx =
                    markClangDeclContextObjectDefined(scope_ctx->getParent());
              }
              if (clang::NamespaceDecl *ns_decl =
                      const_cast<clang::NamespaceDecl *>(
                          llvm::dyn_cast_or_null<clang::NamespaceDecl>(
                              markClangDeclObjectDefinedByKind(
                                  llvm::dyn_cast_or_null<clang::Decl>(
                                      scope_ctx))))) {
                if (SgNamespaceDeclarationStatement *ns_stmt =
                        ensureNamespaceDeclaration(ns_decl)) {
                  if (ns_stmt->get_definition() != nullptr) {
                    template_scope = ns_stmt->get_definition();
                  }
                }
              } else if (scope_ctx != nullptr &&
                         scope_ctx->isTranslationUnit()) {
                template_scope = getGlobalScope();
              }
            }
          }
        }
        nrtype = SageBuilder::buildNonrealType(SgName(base_name),
                                               template_scope, &tpl_args);
      }
    }
    if (SgNonrealDecl *nrdecl =
            isSgNonrealDecl(nrtype ? nrtype->get_declaration() : nullptr)) {
      if (has_template_keyword) {
        nrdecl->set_has_template_keyword(true);
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
        p_pending_nonreal_template_decl_links[nrdecl] = translated_decl_key;
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
    auto get_fallback_template_param_scope =
        [&](const clang::DeclContext *decl_context) -> SgDeclarationScope * {
      decl_context = markClangDeclContextObjectDefined(decl_context);
      if (decl_context == nullptr) {
        return nullptr;
      }

      auto cached_scope =
          p_template_parameter_decl_scope_map.find(decl_context);
      if (cached_scope != p_template_parameter_decl_scope_map.end()) {
        return cached_scope->second;
      }

      SgScopeStatement *anchor_scope = resolveScopeFromDeclContext(
          const_cast<clang::DeclContext *>(decl_context), template_param_scope);
      if (anchor_scope == nullptr) {
        anchor_scope = template_param_scope;
      }
      if (anchor_scope == nullptr) {
        anchor_scope = SageBuilder::topScopeStack();
      }
      if (anchor_scope == nullptr) {
        return nullptr;
      }

      SgDeclarationScope *decl_scope = isSgDeclarationScope(anchor_scope);
      if (decl_scope == nullptr) {
        decl_scope = SageBuilder::buildDeclarationScope();
        if (decl_scope->get_parent() != anchor_scope) {
          decl_scope->set_parent(anchor_scope);
        }
      }

      p_template_parameter_decl_scope_map[decl_context] = decl_scope;
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
      if (resolved.empty()) {
        resolved = resolveTemplateParameterNameFromSageScope(
            SageBuilder::topScopeStack(), depth, index);
      }
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

          if (SgDeclarationStatement *owner_decl = isSgDeclarationStatement(
                  sg_param->get_templateDeclaration())) {
            if (SgDeclarationScope *decl_scope =
                    SageBuilder::getOrCreateNonrealDeclarationScope(
                        owner_decl)) {
              template_param_scope = decl_scope;
            }
          }
        }
      }

      if (name_str.empty() && decl->getDeclName().isIdentifier()) {
        name_str = decl->getNameAsString();
      }
      if (name_str.empty()) {
        name_str = decl->getNameAsString();
      }
      name_str = preferHigherQualityTemplateParamName(name_str,
                                                      decl->getNameAsString());

      if (isSgDeclarationScope(template_param_scope) == nullptr) {
        if (SgDeclarationScope *decl_scope =
                get_fallback_template_param_scope(decl->getDeclContext())) {
          template_param_scope = decl_scope;
        }
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
    if (!active_context_name.empty()) {
      name_str =
          preferHigherQualityTemplateParamName(name_str, active_context_name);
    }
    std::string scope_name =
        resolve_from_sage_scope(ttp->getDepth(), ttp->getIndex());
    name_str = preferHigherQualityTemplateParamName(name_str, scope_name);

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
      name_str = "template_type_param_" + std::to_string(ttp->getDepth()) +
                 "_" + std::to_string(ttp->getIndex());
    }
    ROSE_ASSERT(!name_str.empty());

    SgNonrealType *nrtype = SageBuilder::buildNonrealType(
        SgName(name_str), template_param_scope, nullptr);
    if (SgNonrealDecl *nrdecl =
            isSgNonrealDecl(nrtype ? nrtype->get_declaration() : nullptr)) {
      nrdecl->set_is_template_param(true);
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
      name_str = fallback_type_name(clang_type);
    }
    ROSE_ASSERT(!name_str.empty());
    return build_with_qualifier(std::nullopt, name_str, nullptr);
  }

  if (const clang::TagType *tag = llvm::dyn_cast<clang::TagType>(clang_type)) {
    std::string name_str = tag->getDecl()->getNameAsString();
    if (name_str.empty()) {
      name_str = fallback_type_name(clang_type);
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
      if (clang::TagDecl *tag_decl = tag->getDecl()) {
        SgDeclarationStatement *sg_decl =
            lookupSgDeclarationForClangDecl(tag_decl, /*allow_on_demand=*/true);
        if (translated_decl_resolves_template_target(sg_decl)) {
          nrdecl->set_templateDeclaration(
              normalizeNonrealTemplateDeclarationTarget(sg_decl));
        } else if (auto *spec_decl =
                       llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(
                           tag_decl)) {
          queuePendingImplicitClassTemplateSpecialization(spec_decl);
          p_pending_nonreal_template_decl_links[nrdecl] = spec_decl;
        }
      }
    }
    return nrtype;
  }

  if (const clang::InjectedClassNameType *inj =
          llvm::dyn_cast<clang::InjectedClassNameType>(clang_type)) {
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

    auto injected_name_is_local_to_scope = [&]() -> bool {
      SgScopeStatement *current_scope =
          scope != nullptr ? scope : SageBuilder::topScopeStack();
      if (current_scope == nullptr) {
        return false;
      }

      SgClassDefinition *enclosing_def =
          SageInterface::getEnclosingClassDefinition(current_scope, true);
      if (enclosing_def == nullptr) {
        return false;
      }

      SgClassDeclaration *enclosing_decl =
          canonical_sg_class_decl(enclosing_def->get_declaration());
      SgClassDeclaration *target_decl =
          canonical_sg_class_decl(isSgClassDeclaration(
              lookupSgDeclarationForClangDecl(inj->getDecl(),
                                              /*allow_on_demand=*/true)));
      return enclosing_decl != nullptr && enclosing_decl == target_decl;
    };

    const bool force_nonlocal_injected_name =
        p_force_nonlocal_injected_class_name_depth != 0;
    if (force_nonlocal_injected_name || !injected_name_is_local_to_scope()) {
      clang::QualType injected_qt;
      if (p_compiler_instance != nullptr) {
        injected_qt = getInjectedClassNameSpecializationType(
            inj, p_compiler_instance->getASTContext());
      }
      const clang::Type *injected_ty = injected_qt.getTypePtrOrNull();
      if (injected_ty != nullptr && injected_ty != clang_type) {
        if (SgNonrealType *nrtype = buildNonrealTypeForNestedNameSpecifierType(
                injected_ty, scope, prefer_current_scope)) {
          return nrtype;
        }
      }
    }

    std::string name_str = inj->getDecl()->getNameAsString();
    if (name_str.empty()) {
      name_str = fallback_type_name(clang_type);
    }
    ROSE_ASSERT(!name_str.empty());
    SgNonrealType *nrtype =
        build_with_qualifier(std::nullopt, name_str, nullptr);
    return nrtype;
  }

  std::string name_str;
  if (const clang::TypeDecl *decl = clang_type->getAsTagDecl()) {
    name_str = decl->getNameAsString();
  }
  if (name_str.empty()) {
    name_str = fallback_type_name(clang_type);
  }
  ROSE_ASSERT(!name_str.empty());
  return build_with_qualifier(std::nullopt, name_str, nullptr);
}

SgNonrealType *ClangToSageTranslator::buildNonrealTypeFromNestedNameSpecifier(
    clang::NestedNameSpecifier qualifier, SgScopeStatement *scope,
    const SgName &terminalName,
    const SgTemplateArgumentPtrList *terminalTemplateArgs) {
  qualifier = markClangNestedNameSpecifierDefined(qualifier);
  SgScopeStatement *effective_scope = scope;
  if (effective_scope == nullptr) {
    effective_scope = SageBuilder::topScopeStack();
  }
  bool has_global_qualifier = nestedNameSpecifierHasGlobal(qualifier);
  if (has_global_qualifier ||
      nestedNameSpecifierHasNamespaceQualifier(qualifier)) {
    effective_scope = getGlobalScope();
  }
  ROSE_ASSERT(effective_scope != nullptr);

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
      segment_type = SageBuilder::buildNonrealType(SgName(name_str),
                                                   current_scope, nullptr);
      break;
    }

    case clang::NestedNameSpecifier::Kind::Type: {
      bool prefer_current = static_cast<bool>(
          markClangNestedNameSpecifierDefined(nestedNameSpecifierPrefix(nns)));
      segment_type = buildNonrealTypeForNestedNameSpecifierType(
          markClangTypeObjectDefinedByClass(
              readClangApiValueDefined([&]() { return nns.getAsType(); })),
          current_scope, prefer_current);
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
      segment_type = SageBuilder::buildNonrealType(SgName(name_str),
                                                   current_scope, nullptr);
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

  SgScopeStatement *chain_scope = build_chain(qualifier, effective_scope);
  ROSE_ASSERT(chain_scope != nullptr);

  auto qualifier_requires_typename = [](clang::NestedNameSpecifier nns) {
    return nestedNameSpecifierHasDependentTypeQualifier(nns);
  };

  SgNonrealType *nrtype = SageBuilder::buildNonrealType(
      terminalName, chain_scope, terminalTemplateArgs);
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
      segment_type = SageBuilder::buildNonrealType(SgName(name_str),
                                                   current_scope, nullptr);
      break;
    }
    case clang::NestedNameSpecifier::Kind::Type: {
      bool prefer_current = static_cast<bool>(
          markClangNestedNameSpecifierDefined(nestedNameSpecifierPrefix(nns)));
      segment_type = buildNonrealTypeForNestedNameSpecifierType(
          markClangTypeObjectDefinedByClass(
              readClangApiValueDefined([&]() { return nns.getAsType(); })),
          current_scope, prefer_current);
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
      segment_type = SageBuilder::buildNonrealType(SgName(name_str),
                                                   current_scope, nullptr);
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
  auto ensure_file_info = [this](SgTemplateInstantiationDecl *decl) {
    if (decl == nullptr) {
      return;
    }
    Sg_File_Info *start = decl->get_startOfConstruct();
    Sg_File_Info *end = decl->get_endOfConstruct();
    if (start == nullptr && end == nullptr) {
      setCompilerGeneratedFileInfo(decl);
      return;
    }
    if (start != nullptr && end == nullptr) {
      Sg_File_Info *end_copy = new Sg_File_Info(*start);
      decl->set_endOfConstruct(end_copy);
      end_copy->set_parent(decl);
    } else if (start == nullptr && end != nullptr) {
      Sg_File_Info *start_copy = new Sg_File_Info(*end);
      decl->set_startOfConstruct(start_copy);
      start_copy->set_parent(decl);
    }
  };
  auto suppress_unparse = [](SgLocatedNode *node) {
    if (node == nullptr) {
      return;
    }
    auto mark = [](Sg_File_Info *fi) {
      if (fi == nullptr) {
        return;
      }
      fi->setCompilerGenerated();
      fi->unsetOutputInCodeGeneration();
    };
    mark(node->get_file_info());
    mark(node->get_startOfConstruct());
    mark(node->get_endOfConstruct());
    if (SgExpression *expr = isSgExpression(node)) {
      mark(expr->get_operatorPosition());
    }
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

  auto normalize_decl_scope = [&](SgDeclarationStatement *decl,
                                  SgScopeStatement *target_scope,
                                  const char *context) {
    if (decl == nullptr || target_scope == nullptr) {
      return;
    }
    if (decl->get_scope() != target_scope ||
        decl->get_parent() != target_scope) {
      ensureDeclInScopeChildListPreserveScope(decl, target_scope, context);
      decl->set_scope(target_scope);
      if (decl->get_parent() != target_scope) {
        decl->set_parent(target_scope);
      }
    }
    ensureDeclInScopeChildList(decl, target_scope, context);
  };

  auto suppress_frontend_only_if_synthetic = [](SgLocatedNode *node) {
    if (node == nullptr) {
      return;
    }

    Sg_File_Info *fi = node->get_file_info();
    const bool has_real_source = fi != nullptr && fi->get_line() > 0 &&
                                 !fi->isCompilerGenerated() &&
                                 !fi->isSourcePositionUnavailableInFrontend();
    if (has_real_source) {
      return;
    }

    suppressFrontendOnlyNode(node);
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
      normalize_decl_scope(candidate, target_scope, context);
      suppress_frontend_only_if_synthetic(candidate);
      suppress_frontend_only_if_synthetic(candidate->get_definition());
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

  auto normalize_template_decl_flags = [&](SgTemplateClassDeclaration *decl) {
    auto normalize_file_info = [](Sg_File_Info *fi) {
      if (fi == nullptr) {
        return;
      }
      fi->unsetCompilerGenerated();
      fi->unsetFrontendSpecific();
      fi->unsetOutputInCodeGeneration();
    };

    auto normalize_one = [&](SgTemplateClassDeclaration *candidate) {
      if (candidate == nullptr) {
        return;
      }
      SgScopeStatement *scope = candidate->get_scope();
      SgNode *parent = candidate->get_parent();
      const bool namespace_or_global_scope =
          isSgGlobal(scope) != nullptr ||
          isSgNamespaceDefinitionStatement(scope) != nullptr ||
          isSgGlobal(parent) != nullptr ||
          isSgNamespaceDefinitionStatement(parent) != nullptr;
      if (!namespace_or_global_scope) {
        return;
      }
      candidate->unsetCompilerGenerated();
      candidate->unsetFrontendSpecific();
      normalize_file_info(candidate->get_file_info());
      normalize_file_info(candidate->get_startOfConstruct());
      normalize_file_info(candidate->get_endOfConstruct());
    };

    normalize_one(decl);
    if (SgTemplateClassDeclaration *first = isSgTemplateClassDeclaration(
            decl != nullptr ? decl->get_firstNondefiningDeclaration()
                            : nullptr)) {
      if (first != decl) {
        normalize_one(first);
      }
    }
    if (SgTemplateClassDeclaration *def = isSgTemplateClassDeclaration(
            decl != nullptr ? decl->get_definingDeclaration() : nullptr)) {
      if (def != decl) {
        normalize_one(def);
      }
    }
  };

  auto normalize_instantiation_scope = [&](SgTemplateInstantiationDecl *decl,
                                           SgScopeStatement *target_scope,
                                           const char *context) {
    if (decl == nullptr || target_scope == nullptr) {
      return;
    }
    normalize_decl_scope(decl, target_scope, context);
    if (SgTemplateInstantiationDecl *first = isSgTemplateInstantiationDecl(
            decl->get_firstNondefiningDeclaration())) {
      if (first != decl) {
        normalize_decl_scope(first, target_scope, context);
      }
    }
    if (SgTemplateInstantiationDecl *def =
            isSgTemplateInstantiationDecl(decl->get_definingDeclaration())) {
      if (def != decl) {
        normalize_decl_scope(def, target_scope, context);
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
      return tmpl_decl->get_scope();
    }
    if (SgClassDeclaration *class_decl = isSgClassDeclaration(decl)) {
      if (SgClassDefinition *def = class_decl->get_definition()) {
        return def;
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
      return resolveScopeFromDeclContext(
          const_cast<clang::DeclContext *>(context), nullptr);
    }

    const clang::CXXRecordDecl *record =
        llvm::dyn_cast_or_null<clang::CXXRecordDecl>(
            clangDeclFromDeclContextDefined(context));
    if (record == nullptr) {
      return resolveScopeFromDeclContext(
          const_cast<clang::DeclContext *>(context), nullptr);
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
        const_cast<clang::DeclContext *>(context), nullptr);
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
  normalize_template_decl_flags(template_decl);

  // Extract both base name and qualified name for the template.
  std::string template_base_name = template_decl->get_name().getString();
  std::string template_qualified_name = getTemplateQualifiedName(template_decl);

  // Use qualified names in the cache key to avoid namespace collisions.
  std::string inst_name_full =
      mangleTemplateInstantiation(template_qualified_name, clang_type);
  bool has_dependent_args = false;
  for (const clang::TemplateArgument &arg :
       markClangTemplateArgumentArrayDefined(
           clang_type->template_arguments())) {
    markClangTemplateArgumentDefined(arg);
    if (arg.isInstantiationDependent() ||
        arg.containsUnexpandedParameterPack()) {
      has_dependent_args = true;
      break;
    }
  }
  std::string inst_display_name;
  if (has_dependent_args) {
    SgTemplateArgumentPtrList display_args = buildTemplateArguments(clang_type);
    if (display_args.empty()) {
      inst_display_name = template_base_name;
    } else {
      inst_display_name = SageBuilder::appendTemplateArgumentsToName(
                              SgName(template_base_name), display_args)
                              .getString();
    }
  } else {
    inst_display_name = buildTemplateInstantiationName(
        template_base_name, markClangTemplateArgumentArrayDefined(
                                clang_type->template_arguments()));
  }

  // Check cache
  auto it = p_template_inst_cache.find(inst_name_full);
  if (it != p_template_inst_cache.end()) {
    SgTemplateInstantiationDecl *inst_decl = it->second;
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
        inst_decl->get_templateArguments() = buildTemplateArguments(clang_type);
      }
      if (inst_decl->get_deducedTemplateArguments().empty()) {
        inst_decl->get_deducedTemplateArguments() =
            inst_decl->get_templateArguments();
      }
      if (inst_decl->get_specializedTemplateDeclaration() == nullptr) {
        inst_decl->set_specializedTemplateDeclaration(template_decl);
      }
      SageBuilder::setTemplateArgumentParents(inst_decl);
      if (!inst_decl->get_nameResetFromMangledForm()) {
        inst_decl->set_nameResetFromMangledForm(true);
      }
      if (inst_record_spec_decl != nullptr) {
        if (SgScopeStatement *nested_scope =
                resolve_nested_specialization_scope(
                    readClangApiValueDefined([&]() {
                      return inst_record_spec_decl->getDeclContext();
                    }))) {
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
    ensure_file_info(inst_decl);
    suppress_unparse(inst_decl);
    if (SgClassDefinition *defn = inst_decl->get_definition()) {
      suppress_unparse(defn);
    }
    return inst_decl;
  }

  // Build template arguments
  SgTemplateArgumentPtrList args = buildTemplateArguments(clang_type);
  SgTemplateArgumentPtrList deduced_args = buildTemplateArguments(clang_type);

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
      template_decl, args);
  inst_decl->get_deducedTemplateArguments() = deduced_args;
  inst_decl->set_specializedTemplateDeclaration(template_decl);

  inst_decl->get_templateArguments() = args;

  // Set file info and mark as compiler generated
  // Create synthetic file info since this is a compiler-generated node
  Sg_File_Info *file_info =
      Sg_File_Info::generateDefaultFileInfoForCompilerGeneratedNode();
  inst_decl->set_file_info(file_info);
  setCompilerGeneratedFileInfo(inst_decl);
  ensure_file_info(inst_decl);
  suppress_unparse(inst_decl);
  inst_decl->setForward();
  inst_decl->set_definingDeclaration(nullptr);
  inst_decl->set_firstNondefiningDeclaration(inst_decl);
  SageBuilder::setTemplateArgumentParents(inst_decl);

  if (inst_decl->get_templateDeclaration() == NULL) {
    std::cerr << "CRITICAL ERROR: inst_decl->get_templateDeclaration() is NULL "
                 "immediately after creation! Setting it explicitly."
              << std::endl;
    inst_decl->set_templateDeclaration(template_decl);
  }

  // Use the template declaration scope as the instantiation scope so we
  // stay consistent with later declaration-based instantiation handling.
  SgScopeStatement *inst_scope = template_decl->get_scope();
  if (clang_type != nullptr) {
    if (inst_record_spec_decl != nullptr) {
      if (SgScopeStatement *context_scope =
              resolve_nested_specialization_scope(readClangApiValueDefined(
                  [&]() { return inst_record_spec_decl->getDeclContext(); }))) {
        inst_scope = context_scope;
      }
    } else if (inst_clang_template_decl != nullptr) {
      if (SgScopeStatement *context_scope = resolveScopeFromDeclContext(
              markClangDeclContextObjectDefined(readClangApiValueDefined([&]() {
                return inst_clang_template_decl->getDeclContext();
              })),
              nullptr)) {
        inst_scope = context_scope;
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
          inst_scope = context_scope;
        }
      }
    }
  }
  if (!scopeReachableFromCurrentFile(inst_scope)) {
    SgScopeStatement *reachable_scope = nullptr;
    if (inst_decl_context != nullptr &&
        declContextCanUseReachableNamespaceScope(inst_decl_context)) {
      reachable_scope = resolveReachableNamespaceScope(inst_decl_context);
    }
    if (reachable_scope == nullptr) {
      reachable_scope = getGlobalScope();
    }
    if (reachable_scope != nullptr) {
      inst_scope = reachable_scope;
    }
  }
  if (inst_scope == nullptr) {
    inst_scope = getGlobalScope();
  }

  inst_decl->set_scope(inst_scope);
  inst_decl->set_parent(inst_scope);

  // CRITICAL: Set template name before creating type
  // get_mangled_name() requires this to be set and will assert if it's null
  // Use ONLY base name for templateName (e.g., "array" not "std::array")
  // The qualified name is in the declaration name above
  // NameQualificationTraversal will add the necessary qualification (e.g.
  // "std::")
  inst_decl->set_templateName(SgName(template_base_name));
  inst_decl->set_nameResetFromMangledForm(true);

  // Create class type pointing to this instantiation
  class_type = SgClassType::createType(inst_decl);
  inst_decl->set_type(class_type);

  // Insert symbol via the unified registration path to avoid duplicates.
  registerDeclarationSymbol(inst_decl);

  // Cache it with full name
  p_template_inst_cache[inst_name_full] = inst_decl;
  normalize_instantiation_scope(inst_decl, inst_scope,
                                "getOrCreateTemplateInstantiation");

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
      if (alias_sg_decl == nullptr &&
          p_decl_translation_in_progress.find(alias_decl) ==
              p_decl_translation_in_progress.end() &&
          p_decl_translation_on_demand.find(alias_decl) ==
              p_decl_translation_on_demand.end()) {
        if (SgNode *translated = TraverseOnDemand(alias_decl)) {
          alias_sg_decl = isSgTemplateTypedefDeclaration(translated);
        }
      }
    }
    SgTemplateArgumentPtrList template_args =
        buildTemplateArguments(template_specialization_type);
    SgTemplateArgumentPtrList deduced_args =
        buildTemplateArguments(template_specialization_type);
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

    if (template_specialization_type->isDependentType()) {
      clang::TemplateName tname = markClangTemplateNameDefined(
          template_specialization_type->getTemplateName());
      clang::NestedNameSpecifier qualifier = std::nullopt;
      if (const clang::QualifiedTemplateName *qtn =
              tname.getAsQualifiedTemplateName()) {
        qualifier = qtn->getQualifier();
      } else if (const clang::DependentTemplateName *dtn =
                     tname.getAsDependentTemplateName()) {
        qualifier = dtn->getQualifier();
      }

      std::string alias_name =
          alias_decl != nullptr ? alias_decl->getNameAsString() : std::string();
      if (alias_name.empty()) {
        alias_name = getTemplateNameBase(tname);
      }
      if (alias_name.empty()) {
        alias_name = "__alias_template";
      }

      SgScopeStatement *base_scope = SageBuilder::topScopeStack();
      if (base_scope == nullptr) {
        base_scope = getGlobalScope();
      }

      SgType *dep_alias_type = nullptr;
      if (qualifier) {
        dep_alias_type = buildNonrealTypeFromNestedNameSpecifier(
            qualifier, base_scope, SgName(alias_name),
            template_args.empty() ? nullptr : &template_args);
      } else {
        dep_alias_type = SageBuilder::buildNonrealType(
            SgName(alias_name), base_scope,
            template_args.empty() ? nullptr : &template_args);
      }

      if (dep_alias_type != nullptr) {
        *node = dep_alias_type;
        return VisitType(template_specialization_type, node);
      }
    }

    SgScopeStatement *alias_type_scope = nullptr;
    if (alias_sg_decl != nullptr) {
      alias_type_scope = alias_sg_decl->get_scope();
    }
    if (alias_type_scope == nullptr && alias_decl != nullptr) {
      alias_type_scope = resolveScopeFromDeclContext(
          alias_decl->getDeclContext(), SageBuilder::topScopeStack());
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

    SgType *aliased_type = build_alias_type_in_scope(markClangQualTypeDefined(
        template_specialization_type->getAliasedType()));
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

    auto has_pack_marker = [](const SgTemplateArgumentPtrList &args) -> bool {
      for (SgTemplateArgument *arg : args) {
        if (arg != nullptr &&
            arg->get_argumentType() ==
                SgTemplateArgument::start_of_pack_expansion_argument) {
          return true;
        }
      }
      return false;
    };
    const bool dependent_pack =
        template_specialization_type->containsUnexpandedParameterPack() ||
        template_specialization_type->isDependentType();
    const bool needs_alias_fallback =
        (template_args.empty() || has_pack_marker(template_args)) &&
        dependent_pack;

    if (needs_alias_fallback && aliased_type != nullptr) {
      *node = aliased_type;
      return VisitType(template_specialization_type, node);
    }

    const bool preserve_written_alias_instantiation =
        p_force_written_template_specialization_depth != 0;
    if (!preserve_written_alias_instantiation && aliased_type != nullptr) {
      *node = aliased_type;
      return VisitType(template_specialization_type, node);
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
      alias_name = "__alias_template";
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
        record_alias_type = buildNonrealTypeFromNestedNameSpecifier(
            alias_qualifier, base_scope, SgName(alias_name),
            template_args.empty() ? nullptr : &template_args);
      } else {
        record_alias_type = SageBuilder::buildNonrealType(
            SgName(alias_name), base_scope,
            template_args.empty() ? nullptr : &template_args);
      }

      if (record_alias_type != nullptr) {
        *node = record_alias_type;
        return VisitType(template_specialization_type, node);
      }

      clang::QualType canonical_qt =
          template_specialization_type->getCanonicalTypeInternal();
      if (!canonical_qt.isNull()) {
        if (SgType *canonical_type = build_alias_type_in_scope(canonical_qt)) {
          *node = canonical_type;
          return VisitType(template_specialization_type, node);
        }
      }
      if (aliased_type != nullptr) {
        *node = aliased_type;
        return VisitType(template_specialization_type, node);
      }
    }

    auto resolve_alias_decl_scope = [&]() -> SgScopeStatement * {
      auto resolve_alias_context =
          [&](clang::DeclContext *ctx,
              SgScopeStatement *fallback) -> SgScopeStatement * {
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
          return fallback;
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
        return resolveScopeFromDeclContext(ctx, fallback);
      };

      SgScopeStatement *resolved_scope =
          alias_sg_decl != nullptr ? alias_sg_decl->get_scope() : nullptr;
      if (resolved_scope == nullptr) {
        resolved_scope = SageBuilder::topScopeStack();
      }
      if (resolved_scope == nullptr) {
        resolved_scope = getGlobalScope();
      }

      if (alias_decl != nullptr) {
        if (clang::DeclContext *lexical_context =
                markClangDeclContextObjectDefined(readClangApiValueDefined(
                    [&]() { return alias_decl->getLexicalDeclContext(); }))) {
          resolved_scope =
              resolve_alias_context(lexical_context, resolved_scope);
        }
        resolved_scope = resolve_alias_context(
            markClangDeclContextObjectDefined(readClangApiValueDefined(
                [&]() { return alias_decl->getDeclContext(); })),
            resolved_scope);
      }

      return resolved_scope;
    };

    SgScopeStatement *alias_decl_scope = resolve_alias_decl_scope();
    SgScopeStatement *scope = alias_decl_scope;
    if (scope == nullptr && alias_sg_decl != nullptr) {
      scope = alias_sg_decl->get_scope();
    }
    if (scope == nullptr) {
      scope = SageBuilder::topScopeStack();
    }
    if (scope == nullptr) {
      scope = getGlobalScope();
    }

    auto normalize_alias_decl_scope = [&](SgTemplateTypedefDeclaration *decl,
                                          SgScopeStatement *target_scope,
                                          const char *context) {
      if (decl == nullptr || target_scope == nullptr) {
        return;
      }
      auto normalize_one = [&](SgTemplateTypedefDeclaration *candidate) {
        if (candidate == nullptr) {
          return;
        }
        if (candidate->get_scope() != target_scope ||
            candidate->get_parent() != target_scope) {
          ensureDeclInScopeChildListPreserveScope(candidate, target_scope,
                                                  context);
          candidate->set_scope(target_scope);
          if (candidate->get_parent() != target_scope) {
            candidate->set_parent(target_scope);
          }
        }
        ensureDeclInScopeChildList(candidate, target_scope, context);
        registerDeclarationSymbol(candidate);
      };

      normalize_one(decl);
      if (SgTemplateTypedefDeclaration *first = isSgTemplateTypedefDeclaration(
              decl->get_firstNondefiningDeclaration())) {
        if (first != decl) {
          normalize_one(first);
        }
      }
      if (SgTemplateTypedefDeclaration *def =
              isSgTemplateTypedefDeclaration(decl->get_definingDeclaration())) {
        if (def != decl) {
          normalize_one(def);
        }
      }
    };

    clang::DeclContext *alias_context =
        alias_decl != nullptr
            ? markClangDeclContextObjectDefined(readClangApiValueDefined(
                  [&]() { return alias_decl->getDeclContext(); }))
            : nullptr;

    if (!scopeReachableFromCurrentFile(scope) && alias_context != nullptr &&
        declContextCanUseReachableNamespaceScope(alias_context)) {
      if (SgScopeStatement *reachable_scope =
              resolveReachableNamespaceScope(alias_context)) {
        scope = reachable_scope;
      }
    }

    if (!scopeReachableFromCurrentFile(scope)) {
      if (SgScopeStatement *global_scope = getGlobalScope()) {
        scope = global_scope;
      } else if (p_sage_source_file != nullptr &&
                 p_sage_source_file->get_globalScope() != nullptr) {
        scope = p_sage_source_file->get_globalScope();
      }
    }

    // Keep the declaration in its lexical AST scope. The separate `scope`
    // variable below is only for lookup/instantiation reachability.
    SgScopeStatement *alias_decl_target_scope =
        alias_decl_scope != nullptr ? alias_decl_scope : scope;

    if (alias_sg_decl != nullptr && alias_decl_target_scope != nullptr) {
      normalize_alias_decl_scope(alias_sg_decl, alias_decl_target_scope,
                                 "VisitTemplateSpecializationType:alias");
    }

    if (alias_sg_decl != nullptr && scope != nullptr &&
        alias_decl_target_scope == scope &&
        !scopeReachableFromCurrentFile(alias_sg_decl->get_scope())) {
      SgName reachable_alias_name = alias_sg_decl->get_name();
      if (alias_decl != nullptr) {
        reachable_alias_name = SgName(readClangApiValueDefined(
            [&]() { return alias_decl->getNameAsString(); }));
      }
      bool rebound_to_reachable_symbol = false;
      if (alias_context != nullptr &&
          declContextCanUseReachableNamespaceScope(alias_context)) {
        if (SgTemplateTypedefSymbol *reachable_alias_sym =
                scope->lookup_template_typedef_symbol(reachable_alias_name)) {
          if (SgTemplateTypedefDeclaration *reachable_alias_decl =
                  isSgTemplateTypedefDeclaration(
                      reachable_alias_sym->get_declaration())) {
            alias_sg_decl = reachable_alias_decl;
            rebound_to_reachable_symbol = true;
          }
        }
        if (!rebound_to_reachable_symbol) {
          normalize_alias_decl_scope(
              alias_sg_decl, scope,
              "VisitTemplateSpecializationType:reachable-alias");
        }
      }
    }

    if (alias_sg_decl != nullptr && aliased_type != nullptr &&
        scope != nullptr) {
      SgName alias_name = alias_sg_decl->get_name();
      if (alias_decl != nullptr) {
        alias_name = SgName(alias_decl->getNameAsString());
      }
      SgName alias_name_with_args =
          SageBuilder::appendTemplateArgumentsToName(alias_name, template_args);
      if (SgTemplateTypedefSymbol *existing_symbol =
              scope->lookup_template_typedef_symbol(alias_name_with_args)) {
        if (isSgTemplateInstantiationTypedefDeclaration(
                existing_symbol->get_declaration()) == nullptr) {
          SgScopeStatement *base_scope = scope;
          if (base_scope == nullptr) {
            base_scope = SageBuilder::topScopeStack();
          }
          if (base_scope == nullptr) {
            base_scope = getGlobalScope();
          }
          *node = SageBuilder::buildNonrealType(
              alias_name, base_scope,
              template_args.empty() ? nullptr : &template_args);
          return VisitType(template_specialization_type, node);
        }
      }
      SgTemplateInstantiationTypedefDeclaration *inst_decl =
          SageBuilder::buildTemplateInstantiationTypedefDeclaration_nfi(
              alias_name, aliased_type, scope, /*has_defining_base=*/false,
              alias_sg_decl, template_args);
      if (inst_decl != nullptr) {
        inst_decl->set_typedef_type(SgTypedefDeclaration::e_using);
        repairTypedefDeclarationReferenceShared(inst_decl);
        inst_decl->get_templateArguments() = template_args;
        inst_decl->get_deducedTemplateArguments() = deduced_args;
        inst_decl->set_nameResetFromMangledForm(true);
        inst_decl->unsetCompilerGenerated();
        if (Sg_File_Info *fi = inst_decl->get_file_info()) {
          fi->unsetOutputInCodeGeneration();
        }
        SageBuilder::setTemplateArgumentParents(inst_decl);
        if (inst_decl->get_specializedTemplateDeclaration() == nullptr) {
          inst_decl->set_specializedTemplateDeclaration(alias_sg_decl);
        }
        registerDeclarationSymbol(inst_decl);
        // Keep alias-template instantiations attached to their semantic
        // scope. If the original scope belongs to a detached header fragment,
        // normalize to an equivalent scope reachable from the current source
        // file before attachment.
        ensureDeclInScopeChildList(inst_decl, scope,
                                   "VisitTemplateSpecializationType");
        *node = inst_decl->get_type();
        return VisitType(template_specialization_type, node);
      }
    }

    if (aliased_type != nullptr) {
      *node = aliased_type;
      return VisitType(template_specialization_type, node);
    }

    // Fallback: preserve alias spelling as a nonreal type.
    SgScopeStatement *base_scope = SageBuilder::topScopeStack();
    if (base_scope == nullptr) {
      base_scope = getGlobalScope();
    }

    if (alias_qualifier) {
      if (nestedNameSpecifierHasGlobal(alias_qualifier)) {
        base_scope = getGlobalScope();
      }
      if (SgType *qualified_alias = buildNonrealTypeFromNestedNameSpecifier(
              alias_qualifier, base_scope, SgName(alias_name),
              template_args.empty() ? nullptr : &template_args)) {
        *node = qualified_alias;
        return VisitType(template_specialization_type, node);
      }
    }

    *node = SageBuilder::buildNonrealType(
        SgName(alias_name), base_scope,
        template_args.empty() ? nullptr : &template_args);
    return VisitType(template_specialization_type, node);
  }

  if (template_specialization_type->isDependentType()) {
    clang::TemplateName tname = template_specialization_type->getTemplateName();
    std::string base_name = getTemplateNameBase(tname);
    ROSE_ASSERT(!base_name.empty());

    SgTemplateArgumentPtrList tpl_args =
        buildTemplateArguments(template_specialization_type);

    SgScopeStatement *base_scope = SageBuilder::topScopeStack();
    ROSE_ASSERT(base_scope != nullptr);

    clang::NestedNameSpecifier qualifier = std::nullopt;
    if (const clang::QualifiedTemplateName *qtn =
            tname.getAsQualifiedTemplateName()) {
      qualifier = qtn->getQualifier();
    } else if (const clang::DependentTemplateName *dtn =
                   tname.getAsDependentTemplateName()) {
      qualifier = dtn->getQualifier();
    }

    if (qualifier) {
      *node = buildNonrealTypeFromNestedNameSpecifier(
          qualifier, base_scope, SgName(base_name), &tpl_args);
      return VisitType(template_specialization_type, node);
    }

    // For dependent names without an explicit qualifier, preserve the spelling
    // as-written. Synthesizing declaration-context qualifiers here loses
    // dependent template arguments (e.g., `Outer<T>::Inner<U>` becoming
    // `Outer::Inner<U>`), which breaks correctness.
    *node =
        SageBuilder::buildNonrealType(SgName(base_name), base_scope, &tpl_args);
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

  // Get or create template class declaration
  SgTemplateClassDeclaration *template_decl = NULL;

  clang::TemplateDecl *clang_template_decl = tname.getAsTemplateDecl();
  if (clang_template_decl) {
    clang_template_decl = const_cast<clang::TemplateDecl *>(
        llvm::dyn_cast_or_null<clang::TemplateDecl>(
            markClangDeclObjectDefinedByKind(clang_template_decl)));
    template_decl =
        isSgTemplateClassDeclaration(lookupSgDeclarationForClangDecl(
            llvm::cast<clang::Decl>(clang_template_decl),
            /*allow_on_demand=*/true));
  }

  if (template_decl == NULL) {
    SgScopeStatement *template_scope = nullptr;
    if (clang_template_decl != nullptr) {
      clang::DeclContext *decl_context = markClangDeclContextObjectDefined(
          clang_template_decl->getDeclContext());
      template_scope = resolveScopeFromDeclContext(
          decl_context, SageBuilder::topScopeStack());
    }

    if (template_scope == nullptr) {
      clang::NestedNameSpecifier qualifier = std::nullopt;
      if (const clang::QualifiedTemplateName *qtn =
              tname.getAsQualifiedTemplateName()) {
        qualifier = qtn->getQualifier();
      } else if (const clang::DependentTemplateName *dtn =
                     tname.getAsDependentTemplateName()) {
        qualifier = dtn->getQualifier();
      }

      if (qualifier) {
        SgScopeStatement *base_scope = SageBuilder::topScopeStack();
        if (nestedNameSpecifierHasGlobal(qualifier)) {
          base_scope = getGlobalScope();
        }
        template_scope =
            buildNonrealScopeFromNestedNameSpecifier(qualifier, base_scope);
      }
    }

    template_decl = getOrCreateTemplateDeclaration(
        template_name, template_specialization_type, template_scope);
  }

  if (template_decl == NULL) {
    std::cerr << "CRITICAL ERROR: template_decl is NULL for " << template_name
              << std::endl;
  }

  // Get or create template instantiation
  SgTemplateInstantiationDecl *inst_decl = getOrCreateTemplateInstantiation(
      template_decl, template_specialization_type);

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

  auto lookup_mapped_template_param =
      [&](clang::NamedDecl *decl) -> SgTemplateParameter * {
    if (decl == nullptr) {
      return nullptr;
    }
    auto it = p_decl_translation_map.find(decl);
    if (it == p_decl_translation_map.end()) {
      return nullptr;
    }
    return isSgTemplateParameter(it->second);
  };

  SgTemplateParameter *mapped_param = nullptr;
  if (param_decl != nullptr) {
    auto *mutable_param_decl =
        const_cast<clang::TemplateTypeParmDecl *>(param_decl);

    mapped_param = lookup_mapped_template_param(mutable_param_decl);
    if (mapped_param == nullptr) {
      if (const clang::NamedDecl *canonical = llvm::dyn_cast<clang::NamedDecl>(
              mutable_param_decl->getCanonicalDecl())) {
        mapped_param = lookup_mapped_template_param(
            const_cast<clang::NamedDecl *>(canonical));
      }
    }
    if (mapped_param == nullptr) {
      if (clang::TemplateTypeParmDecl *recent =
              llvm::dyn_cast_or_null<clang::TemplateTypeParmDecl>(
                  mutable_param_decl->getMostRecentDecl())) {
        mapped_param = lookup_mapped_template_param(recent);
      }
    }
    if (mapped_param == nullptr) {
      for (clang::Decl *redecl_decl : mutable_param_decl->redecls()) {
        if (clang::TemplateTypeParmDecl *redecl =
                llvm::dyn_cast_or_null<clang::TemplateTypeParmDecl>(
                    redecl_decl)) {
          mapped_param = lookup_mapped_template_param(redecl);
          if (mapped_param != nullptr) {
            break;
          }
        }
      }
    }

    if (mapped_param == nullptr) {
      SgDeclarationStatement *owning_template = nullptr;
      if (clang::DeclContext *ctx = mutable_param_decl->getDeclContext()) {
        ctx = markClangDeclContextObjectDefined(ctx);
        if (clang::TemplateDecl *template_ctx =
                llvm::dyn_cast<clang::TemplateDecl>(ctx)) {
          template_ctx = const_cast<clang::TemplateDecl *>(
              llvm::dyn_cast_or_null<clang::TemplateDecl>(
                  markClangDeclObjectDefinedByKind(template_ctx)));
          if (template_ctx != nullptr) {
            auto it = p_decl_translation_map.find(template_ctx);
            if (it == p_decl_translation_map.end() &&
                p_decl_translation_in_progress.find(template_ctx) ==
                    p_decl_translation_in_progress.end() &&
                p_decl_translation_on_demand.find(template_ctx) ==
                    p_decl_translation_on_demand.end()) {
              TraverseOnDemand(template_ctx);
              it = p_decl_translation_map.find(template_ctx);
            }
            if (it != p_decl_translation_map.end()) {
              owning_template = isSgDeclarationStatement(it->second);
            }
          }
        }
      }
      if (owning_template != nullptr) {
        mapped_param =
            translateTemplateParameter(mutable_param_decl, owning_template,
                                       mutable_param_decl->getIndex());
      }
    }
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

  if (param_decl != nullptr && param_decl->getDeclName().isIdentifier()) {
    param_name = preferHigherQualityTemplateParamName(
        param_name, param_decl->getNameAsString());
  }
  if (param_decl != nullptr) {
    param_name = preferHigherQualityTemplateParamName(
        param_name, param_decl->getNameAsString());
  }
  if (const clang::IdentifierInfo *identifier =
          template_type_parm_type->getIdentifier()) {
    param_name = preferHigherQualityTemplateParamName(
        param_name, identifier->getName().str());
  }
  if (param_decl != nullptr) {
    std::string context_name = resolveTemplateParameterNameFromDeclContext(
        markClangDeclContextObjectDefined(param_decl->getDeclContext()),
        template_type_parm_type->getDepth(),
        template_type_parm_type->getIndex());
    param_name = preferHigherQualityTemplateParamName(param_name, context_name);
  }
  std::string active_context_name = resolve_from_decl_context_stack(
      template_type_parm_type->getDepth(), template_type_parm_type->getIndex());
  if (!active_context_name.empty()) {
    param_name =
        preferHigherQualityTemplateParamName(param_name, active_context_name);
  }
  std::string scope_name = resolve_from_sage_scope(
      template_type_parm_type->getDepth(), template_type_parm_type->getIndex());
  param_name = preferHigherQualityTemplateParamName(param_name, scope_name);
  if (param_name.empty()) {
    param_name = "template_type_param_" +
                 std::to_string(template_type_parm_type->getDepth()) + "_" +
                 std::to_string(template_type_parm_type->getIndex());
  }

  unsigned depth = 0;
  unsigned index = 0;
  if (parseTemplateParamDepthAndIndex(param_name, &depth, &index)) {
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

  SgTemplateType *template_type =
      SageBuilder::buildTemplateType(SgName(param_name));
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

  auto build_specialized_member_typedef =
      [&](const clang::TypedefNameDecl *typedef_decl) -> SgType * {
    typedef_decl = llvm::dyn_cast_or_null<clang::TypedefNameDecl>(
        markClangDeclObjectDefinedByKind(typedef_decl));
    if (typedef_decl == nullptr) {
      return nullptr;
    }
    const clang::DeclContext *decl_context = markClangDeclContextObjectDefined(
        const_cast<clang::DeclContext *>(typedef_decl->getDeclContext()));
    const clang::CXXRecordDecl *record_decl =
        llvm::dyn_cast_or_null<clang::CXXRecordDecl>(decl_context);
    const clang::ClassTemplateSpecializationDecl *spec_decl =
        llvm::dyn_cast_or_null<clang::ClassTemplateSpecializationDecl>(
            record_decl);
    spec_decl = llvm::dyn_cast_or_null<clang::ClassTemplateSpecializationDecl>(
        markClangDeclObjectDefinedByKind(spec_decl));
    if (spec_decl == nullptr || record_decl == nullptr) {
      return nullptr;
    }
    if (readClangApiValueDefined(
            [&]() { return spec_decl->isDependentType(); })) {
      return nullptr;
    }
    for (const clang::TemplateArgument &tmpl_arg :
         markClangTemplateArgumentListDefined(&spec_decl->getTemplateArgs())
             ->asArray()) {
      markClangTemplateArgumentDefined(tmpl_arg);
      if (readClangApiValueDefined(
              [&]() { return tmpl_arg.isInstantiationDependent(); }) ||
          readClangApiValueDefined(
              [&]() { return tmpl_arg.containsUnexpandedParameterPack(); })) {
        return nullptr;
      }
    }

    SgScopeStatement *scope = SageBuilder::topScopeStack();
    if (scope == nullptr) {
      scope = getGlobalScope();
    }
    if (scope == nullptr) {
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

      p_pending_nonreal_template_decl_links[nrdecl] = mutable_spec;
    };

    auto translated_specialization_scope =
        [&](const clang::ClassTemplateSpecializationDecl *spec)
        -> SgScopeStatement * {
      if (spec == nullptr) {
        return nullptr;
      }

      auto *mutable_spec =
          const_cast<clang::ClassTemplateSpecializationDecl *>(spec);
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

      if (SgTemplateInstantiationDecl *inst_decl =
              isSgTemplateInstantiationDecl(translated_decl)) {
        if (SgClassDefinition *def = inst_decl->get_definition()) {
          return def;
        }
        if (SgDeclarationScope *decl_scope =
                SageBuilder::getOrCreateNonrealDeclarationScope(inst_decl)) {
          return decl_scope;
        }
        return inst_decl->get_scope();
      }

      if (SgClassDeclaration *class_decl =
              isSgClassDeclaration(translated_decl)) {
        if (SgClassDefinition *def = class_decl->get_definition()) {
          return def;
        }
        return class_decl->get_scope();
      }

      return nullptr;
    };

    if (SgScopeStatement *materialized_scope =
            translated_specialization_scope(spec_decl)) {
      scope = materialized_scope;
    }

    if (scope == nullptr || isSgClassDefinition(scope) == nullptr) {
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
                llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(
                    ctx_record);
            SgTemplateArgumentPtrList ctx_args;
            const SgTemplateArgumentPtrList *ctx_args_ptr = nullptr;
            if (ctx_spec != nullptr) {
              ctx_args =
                  buildTemplateArguments(*markClangTemplateArgumentListDefined(
                                             &ctx_spec->getTemplateArgs()),
                                         0);
              if (!ctx_args.empty()) {
                ctx_args_ptr = &ctx_args;
              }
            }
            SgNonrealType *record_type = SageBuilder::buildNonrealType(
                SgName(record_name), scope, ctx_args_ptr);
            if (SgNonrealDecl *record_decl = isSgNonrealDecl(
                    record_type ? record_type->get_declaration() : nullptr)) {
              if (ctx_spec != nullptr) {
                attach_specialization_to_nonreal(record_type, ctx_spec);
              }
              scope = record_decl->get_nonreal_decl_scope();
            }
          }
        }
      }

      SgTemplateArgumentPtrList tpl_args = buildTemplateArguments(
          *markClangTemplateArgumentListDefined(&spec_decl->getTemplateArgs()),
          0);
      std::string spec_name = spec_decl->getNameAsString();
      if (!spec_name.empty()) {
        SgNonrealType *spec_type = SageBuilder::buildNonrealType(
            SgName(spec_name), scope, tpl_args.empty() ? nullptr : &tpl_args);
        if (SgNonrealDecl *spec_decl_node = isSgNonrealDecl(
                spec_type ? spec_type->get_declaration() : nullptr)) {
          attach_specialization_to_nonreal(spec_type, spec_decl);
          scope = spec_decl_node->get_nonreal_decl_scope();
        }
      }
    }

    std::string typedef_name = typedef_decl->getNameAsString();
    if (typedef_name.empty()) {
      return nullptr;
    }
    SgNonrealType *member_type =
        SageBuilder::buildNonrealType(SgName(typedef_name), scope, nullptr);
    if (SgNonrealDecl *member_decl = isSgNonrealDecl(
            member_type ? member_type->get_declaration() : nullptr)) {
      if (!readClangApiValueDefined(
              [&]() { return spec_decl->isDependentType(); })) {
        member_decl->set_suppress_typename(true);
      }
    }
    return member_type;
  };

  if (SgType *specialized_type =
          build_specialized_member_typedef(typedef_type->getDecl())) {
    *node = specialized_type;
    return VisitType(typedef_type, node) && res;
  }

  SgTypedefDeclaration *sg_typedef_decl = NULL;
  auto it = p_decl_translation_map.find(typedef_type->getDecl());
  if (it != p_decl_translation_map.end()) {
    sg_typedef_decl = isSgTypedefDeclaration(it->second);
  }

  if (sg_typedef_decl == NULL) {
    TraverseOnDemand(
        const_cast<clang::TypedefNameDecl *>(typedef_type->getDecl()));
    it = p_decl_translation_map.find(typedef_type->getDecl());
    if (it != p_decl_translation_map.end()) {
      sg_typedef_decl = isSgTypedefDeclaration(it->second);
    }
  }

  if (sg_typedef_decl == NULL) {
    clang::TypedefNameDecl *typedef_decl = typedef_type->getDecl();
    clang::DeclContext *scope_context = typedef_decl->getDeclContext();
    while (scope_context != NULL &&
           llvm::isa<clang::LinkageSpecDecl>(scope_context)) {
      scope_context = scope_context->getParent();
    }
    SgScopeStatement *decl_scope =
        resolveScopeFromDeclContext(scope_context, NULL);
    if (decl_scope == NULL) {
      if (clang::RecordDecl *record_ctx =
              llvm::dyn_cast_or_null<clang::RecordDecl>(scope_context)) {
        clang::RecordDecl *def_ctx = record_ctx->getDefinition();
        clang::RecordDecl *canon_ctx =
            llvm::dyn_cast_or_null<clang::RecordDecl>(
                record_ctx->getCanonicalDecl());
        bool in_progress =
            p_decl_translation_in_progress.count(record_ctx) > 0 ||
            (def_ctx && p_decl_translation_in_progress.count(def_ctx) > 0) ||
            (canon_ctx && p_decl_translation_in_progress.count(canon_ctx) > 0);
        if (in_progress) {
          SgScopeStatement *current = SageBuilder::topScopeStack();
          if (isSgClassDefinition(current) != nullptr ||
              isSgTemplateClassDefinition(current) != nullptr ||
              isSgTemplateInstantiationDefn(current) != nullptr) {
            decl_scope = current;
          }
        }
      }
    }
    if (decl_scope == NULL) {
      decl_scope = SageBuilder::topScopeStack();
    }
    if (decl_scope != NULL) {
      SgName name(typedef_decl->getNameAsString());
      SgType *underlying_type =
          buildTypeFromQualifiedType(typedef_decl->getUnderlyingType());
      sg_typedef_decl = SageBuilder::buildTypedefDeclaration_nfi(
          name, underlying_type, decl_scope);
      sg_typedef_decl->set_parent(decl_scope);
      sg_typedef_decl->set_typedef_type(
          llvm::isa<clang::TypeAliasDecl>(typedef_decl)
              ? SgTypedefDeclaration::e_using
              : SgTypedefDeclaration::e_typedef);
      repairTypedefDeclarationReferenceShared(sg_typedef_decl);
      setCompilerGeneratedFileInfo(sg_typedef_decl);
      suppress_unparse_output(sg_typedef_decl);
      p_decl_translation_map.insert(
          std::make_pair(typedef_decl, sg_typedef_decl));
    }
  }

  if (sg_typedef_decl != NULL) {
    repairTypedefDeclarationReferenceShared(sg_typedef_decl);
    *node = sg_typedef_decl->get_type();
  } else {
    SgSymbol *sym = GetSymbolFromSymbolTable(typedef_type->getDecl());
    SgTypedefSymbol *tdef_sym = isSgTypedefSymbol(sym);

    if (tdef_sym == NULL && SgProject::get_verbose() > 0) {
      std::cerr << "CFE: Missing typedef symbol for '"
                << typedef_type->getDecl()->getNameAsString() << "'"
                << std::endl;
    }

    SgType *symbol_type = (tdef_sym != NULL) ? tdef_sym->get_type()
                                             : SageBuilder::buildUnknownType();
    *node = symbol_type;
  }

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
      *node = SageBuilder::buildUnknownType();
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
  *node = buildNonrealTypeFromNestedNameSpecifier(
      dependent_name_type->getQualifier(), base_scope,
      SgName(id->getName().str()), nullptr);
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
    *node = SageBuilder::buildUnknownType();
  } else {
    *node = buildTypeFromQualifiedType(underlying);
  }

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
  bool res = true;

  // Preserve the type as an unresolved dependent name rather than leaving a
  // null node (which triggers runtime errors and forces an unknown type later).
  *node = SageBuilder::buildUnknownType();

  return VisitType(unresolved_using_type, node) && res;
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
