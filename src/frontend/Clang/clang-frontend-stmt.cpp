#include "clang-decl-utils.hpp"
#include "clang-floating-value.hpp"
#include "clang-frontend-private.hpp"
#include "clang-integer-value.hpp"
#include "clang-nns-utils.hpp"
#include "clang-source-qualification.hpp"
#include "clang-stmt-contract.hpp"

#include "nonrealQualificationSupport.h"
#include "ompAstConstruction.h"
#include "sage3basic.h"

#include <algorithm>

#include <array>

#include <cctype>

#include <cstdint>

#include <cstring>

#include <functional>

#include <iomanip>

#include <iterator>

#include <limits>

#include <sstream>

#include <set>

#include <tuple>

#include <utility>

#include <unordered_set>

#include <vector>

#include <clang/Basic/CharInfo.h>

#include <clang/Basic/OperatorKinds.h>

#include "RoseAst.h"
#include "sageInterface.h"

#include <clang/AST/APValue.h>

#include <clang/AST/Attr.h>

#include <clang/AST/LambdaCapture.h>
#include <clang/AST/StmtOpenMP.h>

#include <clang/Basic/TypeTraits.h>

#include <clang/Lex/Lexer.h>

#include <clang/Lex/LiteralSupport.h>
using llvm::isa; // For LLVM type checking (isa<Type>)
using Rose::ClangFrontend::canonical_function_identity;

namespace {
clang::QualType nativeCallResultType(const clang::CallExpr &call,
                                     const clang::ASTContext &context,
                                     const clang::FunctionDecl *direct_callee) {
  clang::QualType return_type = direct_callee != nullptr
                                    ? direct_callee->getReturnType()
                                    : call.getCallReturnType(context);
  ROSE_ASSERT(!return_type.isNull());
  // Expr::getType() erases reference returns; both paths use semantic types.
  return (return_type->isReferenceType() ? return_type : call.getType())
      .getCanonicalType();
}

void requireOperatorSymbolBinding(SgFunctionDeclaration *declaration,
                                  SgFunctionDeclaration *canonical,
                                  SgSymbol *symbol, const char *context) {
  ASSERT_not_null(declaration);
  ASSERT_not_null(canonical);
  auto *scope = declaration->get_scope();
  auto *defining = declaration->get_definingDeclaration();
  auto *member = isSgMemberFunctionDeclaration(declaration);
  ROSE_ASSERT(scope != nullptr && declaration->get_type() != nullptr);
  ROSE_ASSERT(member == nullptr ||
              isSgMemberFunctionType(declaration->get_type()) != nullptr);
  std::set<SgFunctionDeclaration *> chain{declaration, canonical};
  if (defining != nullptr)
    chain.insert(isSgFunctionDeclaration(defining));
  for (auto *candidate : chain) {
    ASSERT_not_null(candidate);
    auto *candidate_member = isSgMemberFunctionDeclaration(candidate);
    auto *direct_symbol = scope->find_symbol_from_declaration(candidate);
    const bool exact_identity =
        candidate->variantT() == declaration->variantT() &&
        candidate->get_name() == declaration->get_name() &&
        candidate->get_type() == declaration->get_type() &&
        candidate->get_scope() == scope &&
        (member != nullptr
             ? candidate_member != nullptr &&
                   candidate_member->get_associatedClassDeclaration() ==
                       member->get_associatedClassDeclaration()
             : candidate_member == nullptr) &&
        candidate->get_firstNondefiningDeclaration() ==
            declaration->get_firstNondefiningDeclaration() &&
        candidate->get_definingDeclaration() == defining;
    const bool exact_direct_symbol = candidate == canonical
                                         ? direct_symbol == symbol
                                         : direct_symbol == nullptr;
    if (!exact_identity || !exact_direct_symbol) {
      std::cerr << "REX_FRONTEND_INVARIANT[" << context
                << "]: declaration=" << declaration
                << " chain-member=" << candidate
                << " direct-symbol=" << direct_symbol
                << " does not preserve one canonical function identity"
                << std::endl;
      ROSE_ABORT();
    }
  }
  auto *table = scope->get_symbol_table();
  auto *member_symbol = isSgMemberFunctionSymbol(symbol);
  if (symbol == nullptr || table == nullptr ||
      symbol->get_symbol_basis() != canonical ||
      symbol->get_type() != declaration->get_type() ||
      symbol->get_scope() != scope || symbol->get_parent() != table ||
      !table->exists(symbol) ||
      scope->find_symbol_from_declaration(canonical) != symbol ||
      (member != nullptr && (member_symbol == nullptr ||
                             member_symbol->get_declaration() != canonical))) {
    std::cerr << "REX_FRONTEND_INVARIANT[" << context
              << "]: declaration=" << declaration << " canonical=" << canonical
              << " defining=" << defining << " symbol=" << symbol
              << " table=" << table
              << " has malformed canonical symbol ownership" << std::endl;
    ROSE_ABORT();
  }
}

struct SubscriptLayout {
  std::optional<long long> result_storage_size_bytes;
  std::optional<long long> stride_bits;
};

SubscriptLayout measureSubscriptLayout(const clang::ASTContext &context,
                                       clang::QualType result,
                                       clang::QualType base) {
  SubscriptLayout layout;
  if (result->isObjectType() && !result->isIncompleteType() &&
      !result->isDependentType() && result->isConstantSizeType()) {
    layout.result_storage_size_bytes =
        context.getTypeSizeInChars(result).getQuantity();
    const auto bits = context.getTypeSize(result);
    ROSE_ASSERT(bits <= std::numeric_limits<long long>::max());
    layout.stride_bits = bits;
  }
  if (base->isPackedVectorBoolType(context))
    layout.stride_bits = 1;
  if (result->isVoidType() || result->isFunctionType())
    layout.stride_bits = context.getCharWidth();
  return layout;
}
} // namespace

Rose::ClangFrontend::ChooseExprOperands
Rose::ClangFrontend::requireChooseExprOperands(clang::ChooseExpr *expression) {
  if (expression == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[choose-expression-operand]: null "
                 "Clang ChooseExpr"
              << std::endl;
    ROSE_ABORT();
  }

  ChooseExprOperands result{};
  clang::Expr **operands[] = {&result.condition, &result.true_expression,
                              &result.false_expression};
  const char *labels[] = {"condition", "true", "false"};
  auto child = expression->child_begin();
  const auto end = expression->child_end();
  for (std::size_t index = 0; index < std::size(operands); ++index) {
    if (child == end || *child == nullptr || !llvm::isa<clang::Expr>(*child)) {
      std::cerr << "REX_FRONTEND_INVARIANT[choose-expression-operand]: "
                << labels[index] << " operand is not an exact Clang expression"
                << std::endl;
      ROSE_ABORT();
    }
    *operands[index] = llvm::cast<clang::Expr>(*child);
    ++child;
  }
  if (child != end) {
    std::cerr << "REX_FRONTEND_INVARIANT[choose-expression-operand]: Clang "
                 "ChooseExpr has more than three operands"
              << std::endl;
    ROSE_ABORT();
  }
  return result;
}

void Rose::ClangFrontend::requireExactPredefinedIdentifierType(
    SgInitializedName *name, SgType *expected_type) {
  if (name == nullptr || !name->get_is_predefined_identifier() ||
      expected_type == nullptr || name->get_type() != expected_type) {
    std::cerr << "REX_FRONTEND_INVARIANT[predefined-identifier-type]: "
                 "predefined identifier does not retain its exact canonical "
                 "Clang type identity"
              << std::endl;
    ROSE_ABORT();
  }
}

namespace {
clang::TagDecl *definingTagFromTypeLoc(clang::TypeLoc type_loc) {
  ROSE_ASSERT(!type_loc.isNull());
  clang::TagDecl *definition = nullptr;
  for (clang::TypeLoc current = type_loc; !current.isNull();
       current = current.getNextTypeLoc()) {
    auto tag_loc = current.getAs<clang::TagTypeLoc>();
    if (!tag_loc || !tag_loc.isDefinition())
      continue;
    clang::TagDecl *tag = tag_loc.getDecl();
    ROSE_ASSERT(tag != nullptr);
    ROSE_ASSERT(definition == nullptr ||
                definition->getCanonicalDecl() == tag->getCanonicalDecl());
    definition = tag;
  }
  return definition;
}

clang::SourceRange expressionSourceRange(const clang::Expr &expression,
                                         clang::ASTContext &context) {
  const auto *allocation = llvm::dyn_cast<clang::CXXNewExpr>(&expression);
  if (allocation == nullptr)
    return expression.getSourceRange();
  auto *type_info = allocation->getAllocatedTypeSourceInfo();
  ROSE_ASSERT(type_info != nullptr && !type_info->getTypeLoc().isNull());
  clang::SourceLocation end;
  switch (allocation->getInitializationStyle()) {
  case clang::CXXNewInitializationStyle::Parens:
    end = allocation->getDirectInitRange().getEnd();
    break;
  case clang::CXXNewInitializationStyle::Braces:
    ROSE_ASSERT(allocation->getInitializer() != nullptr);
    end = allocation->getInitializer()->getEndLoc();
    break;
  case clang::CXXNewInitializationStyle::None:
    if (allocation->isParenTypeId()) {
      end = allocation->getTypeIdParens().getEnd();
    } else if (allocation->isArray() &&
               context.hasSameType(type_info->getType(),
                                   allocation->getAllocatedType())) {
      // Clang removes the written outer array declarator from the TypeLoc.
      end = allocation->getEndLoc();
    } else {
      // LLVM22 ActOnCXXNew stores Declarator::getEndLoc(), which is invalid
      // for `new decltype(array)`. The retained TypeLoc owns its exact end.
      end = type_info->getTypeLoc().getEndLoc();
    }
    break;
  }
  clang::SourceRange range(allocation->getBeginLoc(), end);
  ROSE_ASSERT(range.isValid());
  return range;
}

class PendingConditionDeclarationScopeGuard {
  using Frame = PendingConditionDeclarationConstruction;
  std::vector<Frame> &stack_;
  Frame frame_;

public:
  PendingConditionDeclarationScopeGuard(std::vector<Frame> &stack,
                                        const clang::DeclStmt *statement,
                                        SgScopeStatement *scope)
      : stack_(stack), frame_{statement, scope} {
    if (frame_.statement == nullptr || frame_.scope == nullptr) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[condition-declaration-transaction]: "
              "cannot begin a condition declaration without its exact Clang "
              "DeclStmt and Sage statement scope\n");
      ROSE_ABORT();
    }
    stack_.push_back(frame_);
  }

  ~PendingConditionDeclarationScopeGuard() {
    if (stack_.empty() || stack_.back().statement != frame_.statement ||
        stack_.back().scope != frame_.scope) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[condition-declaration-transaction]: "
              "condition declaration scope stack was corrupted\n");
      ROSE_ABORT();
    }
    stack_.pop_back();
  }
};

[[noreturn]] void failExactExpressionTranslation(const char *context,
                                                 const clang::Expr *expr) {
  std::cerr << "REX_FRONTEND_INVARIANT[" << context
            << "]: exact expression translation failed";
  if (expr != nullptr) {
    std::cerr << " for Clang " << expr->getStmtClassName();
  }
  std::cerr << std::endl;
  ROSE_ABORT();
}

void requireExactConditionDeclarationResult(clang::DeclStmt *clangDeclaration,
                                            SgStatement *sageDeclaration,
                                            SgScopeStatement *conditionScope,
                                            const char *context) {
  clang::Decl *onlyClangDeclaration =
      clangDeclaration != nullptr && clangDeclaration->isSingleDecl()
          ? clangDeclaration->getSingleDecl()
          : nullptr;
  SgVariableDeclaration *sageVariable =
      isSgVariableDeclaration(sageDeclaration);
  SgInitializedName *sageName =
      sageVariable != nullptr && sageVariable->get_variables().size() == 1
          ? sageVariable->get_variables().front()
          : nullptr;
  if (llvm::dyn_cast_or_null<clang::VarDecl>(onlyClangDeclaration) == nullptr ||
      sageVariable == nullptr || conditionScope == nullptr ||
      sageVariable->get_parent() != nullptr || sageName == nullptr ||
      sageName->get_parent() != sageVariable ||
      sageName->get_scope() != conditionScope) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[condition-declaration-result]: context=%s "
            "did not produce one exact detached variable for condition "
            "scope=%p\n",
            context != nullptr ? context : "<unknown>",
            static_cast<void *>(conditionScope));
    ROSE_ABORT();
  }
}

void requireExactAttachedConditionDeclaration(SgStatement *sageCondition,
                                              SgScopeStatement *conditionScope,
                                              const char *context) {
  SgDeclarationStatement *sageDeclaration =
      isSgDeclarationStatement(sageCondition);
  if (sageDeclaration == nullptr)
    return;

  bool exactConditionEdge = false;
  if (SgIfStmt *ifStatement = isSgIfStmt(conditionScope)) {
    exactConditionEdge = ifStatement->get_conditional() == sageCondition;
  } else if (SgForStatement *forStatement = isSgForStatement(conditionScope)) {
    exactConditionEdge = forStatement->get_test() == sageCondition;
  } else if (SgSwitchStatement *switchStatement =
                 isSgSwitchStatement(conditionScope)) {
    exactConditionEdge = switchStatement->get_item_selector() == sageCondition;
  } else if (SgWhileStmt *whileStatement = isSgWhileStmt(conditionScope)) {
    exactConditionEdge = whileStatement->get_condition() == sageCondition;
  }

  SgVariableDeclaration *sageVariable =
      isSgVariableDeclaration(sageDeclaration);
  const bool exactDeclarators =
      sageVariable != nullptr && !sageVariable->get_variables().empty() &&
      std::all_of(sageVariable->get_variables().begin(),
                  sageVariable->get_variables().end(),
                  [conditionScope, sageVariable](SgInitializedName *name) {
                    return name != nullptr &&
                           name->get_parent() == sageVariable &&
                           name->get_scope() == conditionScope;
                  });
  if (conditionScope == nullptr ||
      sageDeclaration->get_parent() != conditionScope ||
      sageDeclaration->get_scope() != conditionScope || !exactConditionEdge ||
      !exactDeclarators) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[condition-declaration-owner]: context=%s "
            "declaration=%p scope=%p parent=%p edge=%d declarators=%d\n",
            context != nullptr ? context : "<unknown>",
            static_cast<void *>(sageDeclaration),
            static_cast<void *>(conditionScope),
            static_cast<void *>(sageDeclaration->get_parent()),
            exactConditionEdge ? 1 : 0, exactDeclarators ? 1 : 0);
    ROSE_ABORT();
  }
  (void)SageInterface::getConditionValue(conditionScope);
}

static bool isImplicitCxxThis(clang::Expr *expr) {

  while (expr != nullptr) {
    if (clang::ParenExpr *paren = llvm::dyn_cast<clang::ParenExpr>(expr)) {
      expr = llvm::dyn_cast_or_null<clang::Expr>(paren->getSubExpr());
      continue;
    }

    if (clang::ImplicitCastExpr *cast =
            llvm::dyn_cast<clang::ImplicitCastExpr>(expr)) {
      const clang::CastKind kind = cast->getCastKind();
      if (kind == clang::CK_NoOp || kind == clang::CK_LValueToRValue ||
          kind == clang::CK_DerivedToBase ||
          kind == clang::CK_UncheckedDerivedToBase) {
        expr = llvm::dyn_cast_or_null<clang::Expr>(cast->getSubExpr());
        continue;
      }
    }

    if (clang::UnaryOperator *unary =
            llvm::dyn_cast<clang::UnaryOperator>(expr)) {
      if (unary->getOpcode() == clang::UO_Extension) {
        expr = llvm::dyn_cast_or_null<clang::Expr>(unary->getSubExpr());
        continue;
      }
    }

    if (clang::MaterializeTemporaryExpr *materialized =
            llvm::dyn_cast<clang::MaterializeTemporaryExpr>(expr)) {
      expr = llvm::dyn_cast_or_null<clang::Expr>(materialized->getSubExpr());
      continue;
    }

    if (clang::CXXThisExpr *this_expr =
            llvm::dyn_cast<clang::CXXThisExpr>(expr)) {

      return this_expr->isImplicit();
    }

    return false;
  }

  return false;
}

static bool
scope_supports_statement_list_for_instantiation_ref(SgScopeStatement *scope) {
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

} // namespace

SgType *ClangToSageTranslator::buildUnqualifiedWrittenRecordMemberType(
    const clang::Type *written_type, SgScopeStatement *scope) {
  for (;;) {

    if (written_type == nullptr) {
      break;
    }
    if (const auto *paren_type =
            llvm::dyn_cast<clang::ParenType>(written_type)) {
      written_type = paren_type->getInnerType().getTypePtrOrNull();
      continue;
    }
    if (const auto *attr_type =
            llvm::dyn_cast<clang::AttributedType>(written_type)) {
      written_type = attr_type->getModifiedType().getTypePtrOrNull();
      continue;
    }
    if (const auto *adjusted_type =
            llvm::dyn_cast<clang::AdjustedType>(written_type)) {
      written_type = adjusted_type->getOriginalType().getTypePtrOrNull();
      continue;
    }
    break;
  }

  if (written_type == nullptr) {
    return nullptr;
  }

  auto build_from_named_decl = [&](clang::NamedDecl *decl) -> SgType * {
    if (decl == nullptr) {
      return nullptr;
    }

    const clang::DeclContext *decl_context = decl->getDeclContext();
    if (decl_context == nullptr || !decl_context->isRecord()) {
      return nullptr;
    }
    std::string name = decl->getNameAsString();
    if (name.empty()) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[written-member-type-name]: record "
              "member has no exact source name\n");
      ROSE_ABORT();
    }

    SgDeclarationStatement *exact_decl =
        lookupSgDeclarationForClangDecl(decl, /*allow_on_demand=*/true);
    if (exact_decl == nullptr ||
        (llvm::isa<clang::TypedefNameDecl>(decl) &&
         isSgTypedefDeclaration(exact_decl) == nullptr) ||
        (llvm::isa<clang::UsingShadowDecl>(decl) &&
         isSgUsingDeclarationStatement(exact_decl) == nullptr)) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[written-member-type-declaration]: "
              "member=%s has no exact translated declaration\n",
              name.c_str());
      ROSE_ABORT();
    }
    if (auto *alias = isSgTypedefDeclaration(exact_decl)) {
      SgTypedefType *type = alias->get_type();
      ROSE_ASSERT(type != nullptr && type->get_declaration() == alias);
      return type;
    }
    return SageBuilder::buildSemanticNonrealType(SgName(name), scope, nullptr,
                                                 exact_decl);
  };

  if (const auto *typedef_type =
          llvm::dyn_cast<clang::TypedefType>(written_type)) {
    return build_from_named_decl(typedef_type->getDecl());
  }

  if (const auto *using_type = llvm::dyn_cast<clang::UsingType>(written_type)) {
    return build_from_named_decl(using_type->getDecl());
  }

  return nullptr;
}

namespace {

static SgDeclarationStatementPtrList *
getScopeDeclarationList(SgScopeStatement *scope) {
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
  if (scope->containsOnlyDeclarations()) {
    return &scope->getDeclarationList();
  }
  return nullptr;
}

static bool is_decl_attached_to_scope_child_list_for_instantiation_ref(
    SgScopeStatement *scope, SgDeclarationStatement *decl) {
  if (scope == nullptr || decl == nullptr) {
    return false;
  }
  if (SgDeclarationStatementPtrList *decls = getScopeDeclarationList(scope)) {
    return std::find(decls->begin(), decls->end(), decl) != decls->end();
  }
  if (!scope_supports_statement_list_for_instantiation_ref(scope)) {
    return false;
  }
  const SgStatementPtrList &stmts = scope->getStatementList();
  return std::find(stmts.begin(), stmts.end(), decl) != stmts.end();
}

static bool
is_implicit_c_function_decl_for_ref(const clang::FunctionDecl *decl,
                                    const clang::CompilerInstance *ci,
                                    const SagePreprocessorRecord *record) {
  if (decl == nullptr) {
    return false;
  }
  if (llvm::isa<clang::CXXMethodDecl>(decl)) {
    return false;
  }
  if (ci != nullptr && ci->getLangOpts().CPlusPlus) {
    return false;
  }
  if (decl->getBuiltinID() != clang::Builtin::NotBuiltin ||
      clangDeclHasBuiltinAttr(decl) ||
      clangFunctionDeclIsBuiltinSupportForFrontend(decl, ci, record)) {
    return false;
  }
  if (!decl->isImplicit()) {
    return false;
  }
  return true;
}

static void
validate_implicit_c_function_reference(SgFunctionDeclaration *decl) {
  if (decl == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[implicit-c-function-reference]: "
                 "reference has no translated declaration"
              << std::endl;
    ROSE_ABORT();
  }

  if (SgFunctionDeclaration *first_nondef =
          isSgFunctionDeclaration(decl->get_firstNondefiningDeclaration())) {
    decl = first_nondef;
  }
  SgScopeStatement *semantic_scope = decl->get_scope();
  SgAuxiliaryDeclarationList *owner =
      isSgAuxiliaryDeclarationList(decl->get_parent());
  if (!decl->get_is_implicit_function() || semantic_scope == nullptr ||
      owner == nullptr || owner->get_parent() != semantic_scope ||
      semantic_scope->get_auxiliary_declarations() != owner ||
      std::count(owner->get_declarations().begin(),
                 owner->get_declarations().end(), decl) != 1 ||
      is_decl_attached_to_scope_child_list_for_instantiation_ref(semantic_scope,
                                                                 decl)) {
    std::cerr << "REX_FRONTEND_INVARIANT[implicit-c-function-reference]: "
                 "implicit declaration is not exclusively auxiliary-owned"
              << std::endl;
    ROSE_ABORT();
  }

  auto validate_hidden_file_info = [&](const Sg_File_Info *file_info,
                                       const char *role) {
    if (file_info == nullptr || !file_info->isCompilerGenerated() ||
        !file_info->isFrontendSpecific() ||
        !file_info->isOutputInCodeGeneration()) {
      std::cerr << "REX_FRONTEND_INVARIANT[implicit-c-function-reference]: "
                   "implicit declaration has invalid "
                << role << " classification" << std::endl;
      ROSE_ABORT();
    }
  };
  validate_hidden_file_info(decl->get_file_info(), "file-info");
  validate_hidden_file_info(decl->get_startOfConstruct(), "start");
  validate_hidden_file_info(decl->get_endOfConstruct(), "end");
}

static bool detach_decl_from_scope_child_list_for_instantiation_ref(
    SgDeclarationStatement *decl, SgScopeStatement *scope) {
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

  if (SgDeclarationStatementPtrList *decls = getScopeDeclarationList(scope)) {
    return erase_all(*decls);
  }
  if (!scope_supports_statement_list_for_instantiation_ref(scope)) {
    return false;
  }
  return erase_all(scope->getStatementList());
}

static SgDeclarationStatement *
canonical_sage_tag_declaration(SgDeclarationStatement *decl) {
  if (decl == nullptr) {
    return nullptr;
  }
  SgDeclarationStatement *first = decl->get_firstNondefiningDeclaration();
  return first != nullptr ? first : decl;
}

static void require_inline_tag_source_provenance(SgLocatedNode *located,
                                                 const char *context,
                                                 const char *role) {
  if (located == nullptr || context == nullptr || role == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[inline-tag-source-provenance]: "
                 "missing exact node or diagnostic context"
              << std::endl;
    ROSE_ABORT();
  }
  Sg_File_Info *primary = located->get_file_info();
  Sg_File_Info *start = located->get_startOfConstruct();
  Sg_File_Info *end = located->get_endOfConstruct();
  auto is_exact_source = [](const Sg_File_Info *file_info) {
    return file_info != nullptr && file_info->get_line() > 0 &&
           file_info->get_col() > 0 && file_info->get_physical_file_id() >= 0 &&
           !file_info->isCompilerGenerated() &&
           !file_info->isFrontendSpecific() &&
           !file_info->isSourcePositionUnavailableInFrontend();
  };
  const bool one_physical_source =
      is_exact_source(primary) && is_exact_source(start) &&
      is_exact_source(end) &&
      primary->get_physical_file_id() == start->get_physical_file_id() &&
      primary->get_physical_file_id() == end->get_physical_file_id() &&
      primary->get_line() == start->get_line() &&
      primary->get_col() == start->get_col() &&
      (start->get_line() < end->get_line() ||
       (start->get_line() == end->get_line() &&
        start->get_col() <= end->get_col()));
  if (!one_physical_source) {
    std::cerr << "REX_FRONTEND_INVARIANT[inline-tag-source-provenance]: "
              << context << " " << role
              << " lacks one exact physical source range" << std::endl;
    ROSE_ABORT();
  }
}

static SgDeclarationStatement *
adopt_typed_owner_tag_definition_core(SgNode *owner,
                                      SgDeclarationStatement *defining_tag,
                                      SgType *type, const char *context) {
  if (owner == nullptr || defining_tag == nullptr || type == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[typed-inline-tag-owner]: "
              << (context != nullptr ? context : "<unknown>")
              << " has a null owner, declaration, or type" << std::endl;
    ROSE_ABORT();
  }

  SgNamedType *named_type = isSgNamedType(type->findBaseType());
  SgDeclarationStatement *type_decl =
      named_type != nullptr ? named_type->get_declaration() : nullptr;
  const bool exact_class_definition =
      isSgClassDeclaration(defining_tag) != nullptr &&
      defining_tag->get_definingDeclaration() == defining_tag &&
      isSgClassDeclaration(defining_tag)->get_definition() != nullptr;
  const bool exact_enum_definition =
      isSgEnumDeclaration(defining_tag) != nullptr &&
      defining_tag->get_definingDeclaration() == defining_tag &&
      !isSgEnumDeclaration(defining_tag)->isForward();
  if ((!exact_class_definition && !exact_enum_definition) ||
      type_decl == nullptr ||
      canonical_sage_tag_declaration(defining_tag) !=
          canonical_sage_tag_declaration(type_decl)) {
    std::cerr << "REX_FRONTEND_INVARIANT[typed-inline-tag-owner]: "
              << (context != nullptr ? context : "<unknown>")
              << " has no exact defining declaration in its operand type"
              << std::endl;
    ROSE_ABORT();
  }

  if (defining_tag->get_parent() != nullptr ||
      defining_tag->get_scope() == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[typed-inline-tag-owner]: "
              << (context != nullptr ? context : "<unknown>")
              << " defining declaration was not produced detached in its "
                 "exact semantic scope"
              << std::endl;
    ROSE_ABORT();
  }

  if (SgClassDeclaration *class_tag = isSgClassDeclaration(defining_tag)) {
    SgClassDefinition *definition = class_tag->get_definition();
    ROSE_ASSERT(definition->get_declaration() == class_tag);
    ROSE_ASSERT(definition->get_parent() == class_tag);
    ROSE_ASSERT(definition->get_construction_physical_output_owner() ==
                defining_tag->get_scope());
    class_tag->set_isAutonomousDeclaration(false);
    definition->set_construction_physical_output_owner(nullptr);
  } else if (SgEnumDeclaration *enum_tag = isSgEnumDeclaration(defining_tag)) {
    enum_tag->set_isAutonomousDeclaration(false);
  } else {
    std::cerr << "REX_FRONTEND_INVARIANT[typed-inline-tag-owner]: "
                 "typed owner is neither a class nor enum declaration"
              << std::endl;
    ROSE_ABORT();
  }
  defining_tag->set_parent(owner);

  // During detached construction the non-autonomous tag is intentionally not
  // an independent lexical output surface.  Adoption by a typed expression is
  // the exact point where that same source definition acquires its one output
  // owner: the cast/sizeof/alignof type spelling.  Publish the role here in the
  // frontend transaction instead of asking the unparser to infer it from the
  // eventual parent shape.
  for (Sg_File_Info *position :
       {defining_tag->get_file_info(), defining_tag->get_startOfConstruct(),
        defining_tag->get_endOfConstruct()}) {
    if (position == nullptr || position->isCompilerGenerated() ||
        position->isFrontendSpecific() ||
        position->isSourcePositionUnavailableInFrontend()) {
      std::cerr << "REX_FRONTEND_INVARIANT[typed-inline-tag-owner]: " << context
                << " defining declaration has no exact physical output "
                   "position"
                << std::endl;
      ROSE_ABORT();
    }
    position->setOutputInCodeGeneration();
  }
  if (!defining_tag->isOutputInCodeGeneration()) {
    std::cerr << "REX_FRONTEND_INVARIANT[typed-inline-tag-owner]: " << context
              << " defining declaration did not publish its typed output role"
              << std::endl;
    ROSE_ABORT();
  }
  require_inline_tag_source_provenance(defining_tag, context,
                                       "defining declaration");
  if (SgClassDeclaration *class_decl = isSgClassDeclaration(defining_tag)) {
    require_inline_tag_source_provenance(class_decl->get_definition(), context,
                                         "class definition");
  }
  return defining_tag;
}

static SgDeclarationStatement *
adopt_typed_owner_tag_definition(SgNode *owner,
                                 SgDeclarationStatement *defining_tag,
                                 SgType *type, const char *context) {
  if (isSgExpression(owner) == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[expression-inline-tag-owner]: "
              << (context != nullptr ? context : "<unknown>")
              << " requires one exact expression owner" << std::endl;
    ROSE_ABORT();
  }
  return adopt_typed_owner_tag_definition_core(owner, defining_tag, type,
                                               context);
}

static SgDeclarationStatement *
adopt_variable_owner_tag_definition(SgVariableDeclaration *variable,
                                    SgDeclarationStatement *defining_tag,
                                    SgType *type, const char *context) {
  SgAuxiliaryDeclarationList *container =
      variable != nullptr ? isSgAuxiliaryDeclarationList(variable->get_parent())
                          : nullptr;
  SgScopeStatement *scope = container != nullptr
                                ? isSgScopeStatement(container->get_parent())
                                : nullptr;
  SgInitializedName *initialized_name =
      variable != nullptr && variable->get_variables().size() == 1
          ? variable->get_variables().front()
          : nullptr;
  const std::vector<SgNode *> scope_successors =
      scope != nullptr ? scope->get_traversalSuccessorContainer()
                       : std::vector<SgNode *>();
  if (variable == nullptr || defining_tag == nullptr || type == nullptr ||
      container == nullptr || scope == nullptr ||
      scope->get_auxiliary_declarations() != container ||
      variable->get_scope() != scope ||
      std::count(container->get_declarations().begin(),
                 container->get_declarations().end(), variable) != 1 ||
      std::count(scope_successors.begin(), scope_successors.end(), variable) !=
          0 ||
      initialized_name == nullptr ||
      initialized_name->get_parent() != variable ||
      initialized_name->get_scope() != scope ||
      initialized_name->get_type() != type ||
      variable->get_baseTypeDefiningDeclaration() != nullptr ||
      defining_tag->get_scope() != scope) {
    std::cerr << "REX_FRONTEND_INVARIANT[variable-inline-tag-owner]: "
              << (context != nullptr ? context : "<unknown>")
              << " lacks one exact auxiliary variable ownership transaction"
              << std::endl;
    ROSE_ABORT();
  }

  SgDeclarationStatement *adopted = adopt_typed_owner_tag_definition_core(
      variable, defining_tag, type, context);
  variable->set_baseTypeDefiningDeclaration(adopted);
  if (adopted != defining_tag || defining_tag->get_parent() != variable ||
      variable->get_baseTypeDefiningDeclaration() != defining_tag) {
    std::cerr << "REX_FRONTEND_INVARIANT[variable-inline-tag-owner]: "
              << (context != nullptr ? context : "<unknown>")
              << " did not publish its exact reciprocal defining-tag edge"
              << std::endl;
    ROSE_ABORT();
  }
  return defining_tag;
}

static bool canLexTrailingToken(clang::SourceLocation loc,
                                clang::SourceManager &sm) {
  if (!loc.isValid() || loc.isMacroID()) {
    return false;
  }

  clang::FileID file_id = sm.getFileID(loc);
  if (file_id.isInvalid()) {
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

static SgStringVal::string_literal_encoding_enum
cxxStringLiteralEncoding(const clang::StringLiteral *literal) {
  if (literal == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[cxx-string-encoding]: null Clang "
                 "string literal"
              << std::endl;
    ROSE_ABORT();
  }
  if (literal->isOrdinary()) {
    return SgStringVal::e_string_encoding_ordinary;
  }
  if (literal->isUnevaluated()) {
    return SgStringVal::e_string_encoding_ordinary;
  }
  if (literal->isWide()) {
    return SgStringVal::e_string_encoding_wide;
  }
  if (literal->isUTF8()) {
    return SgStringVal::e_string_encoding_utf8;
  }
  if (literal->isUTF16()) {
    return SgStringVal::e_string_encoding_utf16;
  }
  if (literal->isUTF32()) {
    return SgStringVal::e_string_encoding_utf32;
  }
  std::cerr << "REX_FRONTEND_INVARIANT[cxx-string-encoding]: Clang literal "
               "has no supported encoding"
            << std::endl;
  ROSE_ABORT();
}

static const char *
cxxStringLiteralPrefix(SgStringVal::string_literal_encoding_enum encoding) {
  switch (encoding) {
  case SgStringVal::e_string_encoding_ordinary:
    return "";
  case SgStringVal::e_string_encoding_wide:
    return "L";
  case SgStringVal::e_string_encoding_utf8:
    return "u8";
  case SgStringVal::e_string_encoding_utf16:
    return "u";
  case SgStringVal::e_string_encoding_utf32:
    return "U";
  default:
    std::cerr << "REX_FRONTEND_INVARIANT[cxx-string-encoding]: invalid Sage "
                 "encoding="
              << static_cast<int>(encoding) << std::endl;
    ROSE_ABORT();
  }
}

static SgStringVal::string_literal_encoding_enum
cxxStringLiteralTokenEncoding(clang::tok::TokenKind kind) {
  switch (kind) {
  case clang::tok::utf8_string_literal:
    return SgStringVal::e_string_encoding_utf8;
  case clang::tok::wide_string_literal:
    return SgStringVal::e_string_encoding_wide;
  case clang::tok::utf16_string_literal:
    return SgStringVal::e_string_encoding_utf16;
  case clang::tok::utf32_string_literal:
    return SgStringVal::e_string_encoding_utf32;
  case clang::tok::string_literal:
    return SgStringVal::e_string_encoding_ordinary;
  default:
    std::cerr << "REX_FRONTEND_INVARIANT[cxx-string-surface]: token kind="
              << clang::tok::getTokenName(kind)
              << " has no exact C++ string-literal encoding" << std::endl;
    ROSE_ABORT();
  }
}

static void
setCxxStringLiteralSurface(SgStringVal *literal,
                           SgStringVal::string_literal_encoding_enum encoding,
                           const std::string &single_token_spelling) {
  if (literal == nullptr || single_token_spelling.empty()) {
    std::cerr << "REX_FRONTEND_INVARIANT[cxx-string-surface]: literal and "
                 "single-token spelling are required"
              << std::endl;
    ROSE_ABORT();
  }

  literal->set_literal_encoding(encoding);
  literal->set_isRawString(false);
  literal->set_raw_string_delimiter("");
  literal->set_raw_string_payload("");

  const std::string prefix = cxxStringLiteralPrefix(encoding);
  const std::string raw_introducer = prefix + "R\"";
  if (single_token_spelling.compare(0, raw_introducer.size(), raw_introducer) ==
      0) {
    const size_t open = single_token_spelling.find('(', raw_introducer.size());
    if (open == std::string::npos) {
      std::cerr << "REX_FRONTEND_INVARIANT[cxx-raw-string-surface]: raw "
                   "literal has no payload opener"
                << std::endl;
      ROSE_ABORT();
    }
    const std::string delimiter = single_token_spelling.substr(
        raw_introducer.size(), open - raw_introducer.size());
    const std::string terminator = ")" + delimiter + "\"";
    if (single_token_spelling.size() < open + 1 + terminator.size() ||
        single_token_spelling.compare(single_token_spelling.size() -
                                          terminator.size(),
                                      terminator.size(), terminator) != 0) {
      std::cerr << "REX_FRONTEND_INVARIANT[cxx-raw-string-surface]: raw "
                   "literal has no matching exact terminator"
                << std::endl;
      ROSE_ABORT();
    }
    const size_t payload_size =
        single_token_spelling.size() - (open + 1) - terminator.size();
    literal->set_isRawString(true);
    literal->set_raw_string_delimiter(delimiter);
    literal->set_raw_string_payload(
        single_token_spelling.substr(open + 1, payload_size));
    literal->validate_cxx_literal_state();
    return;
  }

  const std::string ordinary_introducer = prefix + "\"";
  if (single_token_spelling.compare(0, ordinary_introducer.size(),
                                    ordinary_introducer) != 0 ||
      single_token_spelling.back() != '"') {
    std::cerr << "REX_FRONTEND_INVARIANT[cxx-string-surface]: token spelling='"
              << single_token_spelling << "' does not match Clang encoding="
              << static_cast<int>(encoding) << " prefix='" << prefix << "'"
              << std::endl;
    ROSE_ABORT();
  }
  literal->validate_cxx_literal_state();
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
    // getExpansionLoc() identifies only the first token of a macro use.  A
    // function-like macro can provide the semantic expression's last token,
    // so using that location truncates the enclosing statement before the
    // invocation arguments and semicolon.  The expansion range is the exact
    // spelled token surface owned by the macro use.
    end = sm.getExpansionRange(end).getEnd();
  }
  if (!end.isValid()) {
    return range;
  }

  if (!canLexTrailingToken(end, sm)) {
    return range;
  }

  clang::Token end_tok;
  if (!clang::Lexer::getRawToken(end, end_tok, sm, lang_opts,
                                 /*IgnoreWhiteSpace=*/true) &&
      end_tok.is(clang::tok::r_brace)) {
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

std::string getLiteralTokenSpelling(const clang::Expr *literal,
                                    clang::SourceManager &sm,
                                    const clang::LangOptions &lang_opts) {
  ASSERT_not_null(literal);
  clang::tok::TokenKind expected_kind;
  if (const auto *character =
          llvm::dyn_cast<clang::CharacterLiteral>(literal)) {
    switch (character->getKind()) {
    case clang::CharacterLiteralKind::Ascii:
      expected_kind = clang::tok::char_constant;
      break;
    case clang::CharacterLiteralKind::Wide:
      expected_kind = clang::tok::wide_char_constant;
      break;
    case clang::CharacterLiteralKind::UTF8:
      expected_kind = clang::tok::utf8_char_constant;
      break;
    case clang::CharacterLiteralKind::UTF16:
      expected_kind = clang::tok::utf16_char_constant;
      break;
    case clang::CharacterLiteralKind::UTF32:
      expected_kind = clang::tok::utf32_char_constant;
      break;
    default:
      failExactExpressionTranslation("literal-character-kind", literal);
    }
  } else {
    ROSE_ASSERT(
        (llvm::isa<clang::IntegerLiteral, clang::FloatingLiteral>(literal)));
    expected_kind = clang::tok::numeric_constant;
  }

  const clang::SourceLocation location =
      sm.getSpellingLoc(literal->getExprLoc());
  clang::Token token;
  if (location.isInvalid() ||
      clang::Lexer::getRawToken(location, token, sm, lang_opts))
    failExactExpressionTranslation("literal-token-source", literal);
  if (!token.is(expected_kind))
    return {};

  bool invalid = false;
  std::string spelling =
      clang::Lexer::getSpelling(token, sm, lang_opts, &invalid);
  if (invalid || spelling.empty())
    failExactExpressionTranslation("literal-token-spelling", literal);
  return spelling;
}

bool integerLiteralHasExactSubstitutionAnchor(
    const clang::IntegerLiteral *literal, clang::SourceManager &sourceManager,
    const clang::LangOptions &languageOptions) {
  if (literal == nullptr || literal->getLocation().isInvalid() ||
      literal->getLocation().isMacroID() ||
      !literal->getSourceRange().isValid()) {
    return false;
  }
  const clang::SourceLocation location =
      sourceManager.getSpellingLoc(literal->getLocation());
  const clang::SourceLocation begin =
      sourceManager.getSpellingLoc(literal->getSourceRange().getBegin());
  const clang::SourceLocation end =
      sourceManager.getSpellingLoc(literal->getSourceRange().getEnd());
  if (location.isInvalid() || begin != location || end != location) {
    return false;
  }
  clang::Token token;
  if (clang::Lexer::getRawToken(location, token, sourceManager, languageOptions,
                                /*IgnoreWhiteSpace=*/true)) {
    return false;
  }
  return token.isOneOf(clang::tok::identifier, clang::tok::raw_identifier);
}

SgSymbol *symbolForExactThisPointerType(SgType *exact_this_type) {
  SgPointerType *pointer_type = isSgPointerType(exact_this_type);
  SgType *pointee_type =
      pointer_type != nullptr ? pointer_type->get_base_type() : nullptr;
  if (pointee_type != nullptr) {
    pointee_type = pointee_type->stripType(SgType::STRIP_MODIFIER_TYPE |
                                           SgType::STRIP_TYPEDEF_TYPE);
  }

  if (SgClassType *class_type = isSgClassType(pointee_type)) {
    SgClassDeclaration *declaration =
        isSgClassDeclaration(class_type->get_declaration());
    SgClassDeclaration *first =
        declaration != nullptr
            ? isSgClassDeclaration(
                  declaration->get_firstNondefiningDeclaration())
            : nullptr;
    for (SgClassDeclaration *candidate : {declaration, first}) {
      SgSymbol *symbol = candidate != nullptr
                             ? candidate->get_symbol_from_symbol_table()
                             : nullptr;
      if (isSgClassSymbol(symbol) != nullptr) {
        return symbol;
      }
    }
    return nullptr;
  }

  if (SgNonrealType *nonreal_type = isSgNonrealType(pointee_type)) {
    SgNonrealDecl *declaration =
        isSgNonrealDecl(nonreal_type->get_declaration());
    return declaration != nullptr
               ? isSgNonrealSymbol(declaration->get_symbol_from_symbol_table())
               : nullptr;
  }

  return nullptr;
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

template <typename T>
void setExplicitQualifierOnRef(T *ref, const SourceQualification &info) {
  if (ref == nullptr) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[explicit-qualification]: exact source "
            "qualifier has no Sage reference owner\n");
    ROSE_ABORT();
  }
  ref->set_explicit_name_qualification_length(
      static_cast<int>(info.tokens.size()));
  ref->set_explicit_global_qualification(info.global);
  ref->set_explicit_name_qualification_tokens(info.tokens);
}

template <typename T> void setStructuralQualifierDepthOnRef(T *ref, int depth) {
  if (ref == nullptr || depth <= 0) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[structural-qualification]: semantic "
            "macro qualifier has no Sage reference or positive depth\n");
    ROSE_ABORT();
  }
  ref->set_explicit_name_qualification_length(depth);
  ref->set_explicit_global_qualification(false);
  ref->set_explicit_name_qualification_tokens({});
}

void setStructuralQualifierDepthOnExpr(SgExpression *expr, int depth) {
  if (SgVarRefExp *var_ref = isSgVarRefExp(expr)) {
    setStructuralQualifierDepthOnRef(var_ref, depth);
  } else if (SgTemplateMemberFunctionRefExp *tmpl_member =
                 isSgTemplateMemberFunctionRefExp(expr)) {
    setStructuralQualifierDepthOnRef(tmpl_member, depth);
  } else if (SgMemberFunctionRefExp *member_ref =
                 isSgMemberFunctionRefExp(expr)) {
    setStructuralQualifierDepthOnRef(member_ref, depth);
  } else if (SgTemplateFunctionRefExp *tmpl_func =
                 isSgTemplateFunctionRefExp(expr)) {
    setStructuralQualifierDepthOnRef(tmpl_func, depth);
  } else if (SgFunctionRefExp *func_ref = isSgFunctionRefExp(expr)) {
    setStructuralQualifierDepthOnRef(func_ref, depth);
  } else if (SgNonrealRefExp *nonreal_ref = isSgNonrealRefExp(expr)) {
    setStructuralQualifierDepthOnRef(nonreal_ref, depth);
  } else if (SgEnumVal *enum_val = isSgEnumVal(expr)) {
    setStructuralQualifierDepthOnRef(enum_val, depth);
  } else {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[structural-qualification]: expression=%p "
            "type=%s cannot own semantic macro qualifier depth\n",
            static_cast<void *>(expr),
            expr != nullptr ? expr->class_name().c_str() : "<null>");
    ROSE_ABORT();
  }
}

void transferWrittenNonrealQualifierToUseSite(
    SgType *source_type, const SgStringList &qualifier_tokens,
    bool global_qualification, const char *producer) {
  ASSERT_not_null(source_type);
  ASSERT_not_null(producer);
  SgType *stripped_source_type = source_type->stripType(
      SgType::STRIP_MODIFIER_TYPE | SgType::STRIP_REFERENCE_TYPE |
      SgType::STRIP_RVALUE_REFERENCE_TYPE | SgType::STRIP_POINTER_TYPE |
      SgType::STRIP_ARRAY_TYPE);
  SgNonrealType *source_nonreal = isSgNonrealType(stripped_source_type);
  SgNonrealDecl *source_declaration = isSgNonrealDecl(
      source_nonreal != nullptr ? source_nonreal->get_declaration() : nullptr);
  if (source_declaration == nullptr) {
    return;
  }
  if (source_declaration->get_source_name_qualification_present() ||
      source_declaration->get_source_name_global_qualification() ||
      !source_declaration->get_source_name_qualification_tokens().empty()) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[expression-type-source-owner]: "
            "producer=%s type=%p terminal=%p/%s role=%d scope=%p/%s "
            "present=%d global=%d tokens=%zu shared semantic identity owns "
            "use-site qualifier state\n",
            producer, static_cast<void *>(source_type),
            static_cast<void *>(source_declaration),
            source_declaration->get_name().getString().c_str(),
            static_cast<int>(source_declaration->get_nonreal_template_role()),
            static_cast<void *>(source_declaration->get_scope()),
            source_declaration->get_scope() != nullptr
                ? source_declaration->get_scope()->class_name().c_str()
                : "<null>",
            source_declaration->get_source_name_qualification_present() ? 1 : 0,
            source_declaration->get_source_name_global_qualification() ? 1 : 0,
            source_declaration->get_source_name_qualification_tokens().size());
    ROSE_ABORT();
  }
  if (SageInterface::typeCarriesWrittenNonrealQualification(source_type)) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[expression-type-source-owner]: "
            "producer=%s failed to make its nonreal qualifier semantic-only\n",
            producer);
    ROSE_ABORT();
  }
}

void setExplicitQualifierOnExpr(SgExpression *expr,
                                const SourceQualification &info) {
  if (expr == nullptr || (info.tokens.empty() && !info.global)) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[explicit-qualification]: exact source "
            "qualifier has no expression or qualifier components\n");
    ROSE_ABORT();
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
  } else {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[explicit-qualification]: expression=%p "
            "type=%s cannot own an exact source qualifier\n",
            static_cast<void *>(expr), expr->class_name().c_str());
    ROSE_ABORT();
  }
}

void attachExplicitQualifierFromNestedName(
    SgExpression *expr, clang::NestedNameSpecifier qualifier,
    const clang::NestedNameSpecifierLoc *qualifier_loc,
    clang::CompilerInstance *compiler_instance,
    clang::SourceLocation exact_source_begin = clang::SourceLocation()) {

  if (!qualifier) {
    return;
  }
  if (expr == nullptr || compiler_instance == nullptr ||
      qualifier_loc == nullptr || !qualifier_loc->getNestedNameSpecifier()) {
    fprintf(stderr, "REX_FRONTEND_INVARIANT[explicit-qualification]: semantic "
                    "qualifier has no exact expression, compiler, and source "
                    "location identity\n");
    ROSE_ABORT();
  }

  const clang::NestedNameSpecifierLoc defined_qualifier_loc = *qualifier_loc;
  const SourceQualification info =
      sourceQualificationFromNestedNameSpecifierLoc(
          defined_qualifier_loc, compiler_instance,
          "attach-explicit-expression-qualification", exact_source_begin);
  if (info.semantic_identity != qualifier) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[explicit-qualification]: exact source "
            "qualifier disagrees with its Clang semantic identity\n");
    ROSE_ABORT();
  }
  if (info.surface ==
      SourceQualificationSurface::semantic_macro_expansion_fragment) {
    if (!info.tokens.empty() || !info.components.empty()) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[explicit-qualification]: semantic "
              "macro fragment acquired an independent source payload\n");
      ROSE_ABORT();
    }
    int semantic_depth = 0;
    for (clang::NestedNameSpecifier current = qualifier; current;
         current = nestedNameSpecifierPrefix(current)) {
      if (current.getKind() != clang::NestedNameSpecifier::Kind::Global) {
        ++semantic_depth;
      }
    }
    if (info.global) {
      ++semantic_depth;
    }
    setStructuralQualifierDepthOnExpr(expr, semantic_depth);
    return;
  }
  setExplicitQualifierOnExpr(expr, info);
}

std::string extractPragmaDirective(const std::string &pragma_text) {
  ROSE_ASSERT(!pragma_text.empty() && pragma_text.front() == '#');
  const size_t pragma_pos =
      RoseOpenMPPragmaCallback::skipWhitespace(pragma_text, 1);
  ROSE_ASSERT(pragma_text.compare(pragma_pos, 6, "pragma") == 0);
  const size_t after_pragma = pragma_pos + 6;
  ROSE_ASSERT(
      after_pragma < pragma_text.size() &&
      RoseOpenMPPragmaCallback::isWhitespace(pragma_text[after_pragma]));
  return std::string(pragma_pos - 1, ' ') +
         pragma_text.substr(after_pragma + 1);
}

} // namespace

void ClangToSageTranslator::applySourceRangeWithTrailingSemicolon(
    SgNode *rose_node, const clang::Stmt *clang_stmt) {
  if (rose_node == nullptr || clang_stmt == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[trailing-semicolon-source]: exact "
                 "Sage node and Clang statement are required"
              << std::endl;
    ROSE_ABORT();
  }

  clang::SourceRange range = clang_stmt->getSourceRange();
  range = extendSourceRangeWithTrailingSemicolon(
      range, p_compiler_instance->getSourceManager(),
      p_compiler_instance->getLangOpts());
  applySourceRange(rose_node, range);
}

SgNonrealRefExp *
ClangToSageTranslator::buildNonrealRefExpFromNestedNameSpecifier(
    clang::NestedNameSpecifier qualifier, SgScopeStatement *scope,
    const SgName &terminalName, bool terminalHasTemplateKeyword,
    const SgTemplateArgumentPtrList *terminalSemanticTemplateArgs,
    const SgTemplateArgumentPtrList *terminalReferenceTemplateArgs,
    SgDeclarationStatement *terminalDeclaration) {
  SgNonrealType *nrtype = buildSemanticNonrealTypeFromNestedNameSpecifier(
      qualifier, scope, terminalName, terminalSemanticTemplateArgs,
      terminalDeclaration);
  ROSE_ASSERT(nrtype != nullptr);

  SgNonrealDecl *nrdecl = isSgNonrealDecl(nrtype->get_declaration());
  ROSE_ASSERT(nrdecl != nullptr);

  if (terminalHasTemplateKeyword) {
    nrdecl->set_has_template_keyword(true);
  }

  SgNonrealSymbol *sym =
      isSgNonrealSymbol(nrdecl->get_symbol_from_symbol_table());
  ROSE_ASSERT(sym != nullptr);
  SgNonrealRefExp *ref = SageBuilder::buildNonrealRefExp_nfi(sym);
  if (terminalReferenceTemplateArgs != nullptr) {
    ref->get_templateArguments() = *terminalReferenceTemplateArgs;
    SageBuilder::setTemplateArgumentParents(ref);
  }
  return ref;
}

SgNode *ClangToSageTranslator::Traverse(clang::Stmt *stmt) {

  if (stmt == nullptr)
    return nullptr;

  struct ScopedClangStatementTraversal {
    std::vector<const clang::Stmt *> &stack;
    const clang::Stmt *statement;

    ScopedClangStatementTraversal(std::vector<const clang::Stmt *> &stack,
                                  const clang::Stmt *statement)
        : stack(stack), statement(statement) {
      stack.push_back(statement);
    }

    ~ScopedClangStatementTraversal() {
      if (stack.empty() || stack.back() != statement) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[statement-traversal-producer]: "
                "Clang statement traversal stack changed before scope exit\n");
        ROSE_ABORT();
      }
      stack.pop_back();
    }
  } scoped_clang_statement_traversal(p_clang_statement_traversal_stack, stmt);

  const bool semantic_expression_construction =
      llvm::isa<clang::Expr>(stmt) &&
      hasExactSemanticExpressionConstructionRole(
          "ClangToSageTranslator::Traverse(clang::Stmt*):cache-role");
  auto it = p_stmt_translation_map.find(stmt);
  if (!semantic_expression_construction &&
      p_rebuild_translation_cache_depth == 0 &&
      it != p_stmt_translation_map.end()) {
    if (SgExpression *expr = isSgExpression(it->second)) {
      // Clang can reuse expression subtrees across template patterns,
      // instantiations, and other lowered wrapper nodes. ROSE expressions
      // must remain a tree with a single owning parent, so cached expressions
      // need copy-on-reuse rather than pointer reuse.
      return copyExpressionForFrontendReuse(expr);
    }
    if (SgStatement *sg_stmt = isSgStatement(it->second)) {
      switch (stmt->getStmtClass()) {
      case clang::Stmt::LabelStmtClass:
      case clang::Stmt::CaseStmtClass:
      case clang::Stmt::DefaultStmtClass:
        // These statements are referenced from elsewhere in the AST (e.g.,
        // goto/switch targets), so lookups must continue to resolve to the
        // original node identity.
        return sg_stmt;
      default:
        break;
      }

      if (sg_stmt->get_parent() != nullptr) {
        // Clang deliberately reuses statement objects between template
        // patterns and instantiated function bodies.  The first translation
        // can therefore belong to a hidden semantic body while this
        // occurrence is the lexical template definition (or vice versa).
        // A generic Sage deep copy classifies the result as transformation
        // output and destroys that exact frontend role.  Re-run the statement
        // producer under the current traversal transaction instead: every
        // source/generated decision is then published from the current Clang
        // context, and function-local declarations are rebuilt with the same
        // ownership transaction.
        struct ScopedStatementTranslationCacheRebuild {
          unsigned &depth;

          explicit ScopedStatementTranslationCacheRebuild(unsigned &depth_ref)
              : depth(depth_ref) {
            if (depth == std::numeric_limits<unsigned>::max()) {
              std::cerr << "REX_FRONTEND_INVARIANT[statement-cache-rebuild]: "
                           "translation-cache rebuild depth overflow"
                        << std::endl;
              ROSE_ABORT();
            }
            ++depth;
          }

          ~ScopedStatementTranslationCacheRebuild() {
            if (depth == 0) {
              std::cerr << "REX_FRONTEND_INVARIANT[statement-cache-rebuild]: "
                           "translation-cache rebuild depth underflow"
                        << std::endl;
              ROSE_ABORT();
            }
            --depth;
          }
        } rebuild(p_rebuild_translation_cache_depth);

        SgNode *rebuilt = Traverse(stmt);
        SgNode *rebuilt_parent =
            rebuilt != nullptr ? rebuilt->get_parent() : nullptr;
        SgNode *exact_construction_owner =
            rebuilt_parent != nullptr
                ? statementConstructionOwner(stmt,
                                             "statement-cache rebuild result")
                : nullptr;
        if (rebuilt == nullptr || rebuilt == sg_stmt ||
            rebuilt->variantT() != sg_stmt->variantT() ||
            (rebuilt_parent != nullptr &&
             rebuilt_parent != exact_construction_owner)) {
          std::cerr << "REX_FRONTEND_INVARIANT[statement-cache-rebuild]: Clang "
                    << stmt->getStmtClassName() << "="
                    << static_cast<void *>(stmt)
                    << " begin=" << stmt->getBeginLoc().getRawEncoding()
                    << " end=" << stmt->getEndLoc().getRawEncoding()
                    << " cached Sage statement=" << static_cast<void *>(sg_stmt)
                    << "/" << sg_stmt->class_name()
                    << " produced rebuilt node=" << static_cast<void *>(rebuilt)
                    << "/"
                    << (rebuilt != nullptr ? rebuilt->class_name() : "<null>")
                    << " parent="
                    << static_cast<void *>(
                           rebuilt != nullptr ? rebuilt->get_parent() : nullptr)
                    << "/"
                    << (rebuilt != nullptr && rebuilt->get_parent() != nullptr
                            ? rebuilt->get_parent()->class_name()
                            : "<null>")
                    << " traversal-depth="
                    << p_clang_statement_traversal_stack.size()
                    << " exact-construction-owner="
                    << static_cast<void *>(exact_construction_owner)
                    << std::endl;
          for (const clang::Stmt *active : p_clang_statement_traversal_stack) {
            std::cerr << "  active " << active->getStmtClassName() << "="
                      << static_cast<const void *>(active)
                      << " begin=" << active->getBeginLoc().getRawEncoding()
                      << " end=" << active->getEndLoc().getRawEncoding()
                      << std::endl;
          }
          ROSE_ABORT();
        }
        return rebuilt;
      }
    }
    return it->second;
  }

  SgNode *result = nullptr;
  bool ret_status = false;

  switch (stmt->getStmtClass()) {
  case clang::Stmt::GCCAsmStmtClass:
    ret_status = VisitGCCAsmStmt((clang::GCCAsmStmt *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::MSAsmStmtClass:
    ret_status = VisitMSAsmStmt((clang::MSAsmStmt *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::BreakStmtClass:
    ret_status = VisitBreakStmt((clang::BreakStmt *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::CapturedStmtClass:
    ret_status = VisitCapturedStmt((clang::CapturedStmt *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::CompoundStmtClass:
    ret_status = VisitCompoundStmt((clang::CompoundStmt *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::ContinueStmtClass:
    ret_status = VisitContinueStmt((clang::ContinueStmt *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::CoreturnStmtClass:
    ret_status = VisitCoreturnStmt((clang::CoreturnStmt *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::CoroutineBodyStmtClass:
    ret_status =
        VisitCoroutineBodyStmt((clang::CoroutineBodyStmt *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::CXXCatchStmtClass:
    ret_status = VisitCXXCatchStmt((clang::CXXCatchStmt *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::CXXForRangeStmtClass:
    ret_status = VisitCXXForRangeStmt((clang::CXXForRangeStmt *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::CXXTryStmtClass:
    ret_status = VisitCXXTryStmt((clang::CXXTryStmt *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::DeclStmtClass:
    ret_status = VisitDeclStmt((clang::DeclStmt *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::DoStmtClass:
    ret_status = VisitDoStmt((clang::DoStmt *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::ForStmtClass:
    ret_status = VisitForStmt((clang::ForStmt *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::GotoStmtClass:
    ret_status = VisitGotoStmt((clang::GotoStmt *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::IfStmtClass:
    ret_status = VisitIfStmt((clang::IfStmt *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::IndirectGotoStmtClass:
    ret_status =
        VisitIndirectGotoStmt((clang::IndirectGotoStmt *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::MSDependentExistsStmtClass:
    ret_status = VisitMSDependentExistsStmt(
        (clang::MSDependentExistsStmt *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::NullStmtClass:
    ret_status = VisitNullStmt((clang::NullStmt *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::ReturnStmtClass:
    ret_status = VisitReturnStmt((clang::ReturnStmt *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::SEHExceptStmtClass:
    ret_status = VisitSEHExceptStmt((clang::SEHExceptStmt *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::SEHFinallyStmtClass:
    ret_status = VisitSEHFinallyStmt((clang::SEHFinallyStmt *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::SEHLeaveStmtClass:
    ret_status = VisitSEHLeaveStmt((clang::SEHLeaveStmt *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::SEHTryStmtClass:
    ret_status = VisitSEHTryStmt((clang::SEHTryStmt *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::CaseStmtClass:
    ret_status = VisitCaseStmt((clang::CaseStmt *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::DefaultStmtClass:
    ret_status = VisitDefaultStmt((clang::DefaultStmt *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::SwitchStmtClass:
    ret_status = VisitSwitchStmt((clang::SwitchStmt *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::AttributedStmtClass:
    ret_status = VisitAttributedStmt((clang::AttributedStmt *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::BinaryConditionalOperatorClass:
    ret_status = VisitBinaryConditionalOperator(
        (clang::BinaryConditionalOperator *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::ConditionalOperatorClass:
    ret_status =
        VisitConditionalOperator((clang::ConditionalOperator *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::AddrLabelExprClass:
    ret_status = VisitAddrLabelExpr((clang::AddrLabelExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::ArrayInitIndexExprClass:
    ret_status =
        VisitArrayInitIndexExpr((clang::ArrayInitIndexExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::CXXInheritedCtorInitExprClass:
    failExactExpressionTranslation("inherited-constructor-context",
                                   llvm::cast<clang::Expr>(stmt));
  case clang::Stmt::AsTypeExprClass:
  case clang::Stmt::CXXUuidofExprClass:
  case clang::Stmt::FixedPointLiteralClass:
  case clang::Stmt::FunctionParmPackExprClass:
    failExactExpressionTranslation("unsupported-source-expression",
                                   llvm::cast<clang::Expr>(stmt));
  case clang::Stmt::ArrayInitLoopExprClass:
    ret_status =
        VisitArrayInitLoopExpr((clang::ArrayInitLoopExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::ArraySubscriptExprClass:
    ret_status =
        VisitArraySubscriptExpr((clang::ArraySubscriptExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::ArrayTypeTraitExprClass:
    ret_status =
        VisitArrayTypeTraitExpr((clang::ArrayTypeTraitExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::AtomicExprClass:
    ret_status = VisitAtomicExpr((clang::AtomicExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::CompoundAssignOperatorClass:
    ret_status = VisitCompoundAssignOperator(
        (clang::CompoundAssignOperator *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::BlockExprClass:
    ret_status = VisitBlockExpr((clang::BlockExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::CUDAKernelCallExprClass:
    ret_status =
        VisitCUDAKernelCallExpr((clang::CUDAKernelCallExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::CXXMemberCallExprClass:
    ret_status =
        VisitCXXMemberCallExpr((clang::CXXMemberCallExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::CXXOperatorCallExprClass:
    ret_status =
        VisitCXXOperatorCallExpr((clang::CXXOperatorCallExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::UserDefinedLiteralClass:
    ret_status =
        VisitUserDefinedLiteral((clang::UserDefinedLiteral *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::BuiltinBitCastExprClass:
    ret_status =
        VisitBuiltinBitCastExpr((clang::BuiltinBitCastExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::CStyleCastExprClass:
    ret_status = VisitCStyleCastExpr((clang::CStyleCastExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::CXXFunctionalCastExprClass:
    ret_status = VisitCXXFunctionalCastExpr(
        (clang::CXXFunctionalCastExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::CXXConstCastExprClass:
  case clang::Stmt::CXXDynamicCastExprClass:
  case clang::Stmt::CXXReinterpretCastExprClass:
  case clang::Stmt::CXXStaticCastExprClass:
    ret_status = VisitExplicitCastExpr(
        llvm::cast<clang::CXXNamedCastExpr>(stmt), &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::CXXAddrspaceCastExprClass:
    ret_status =
        VisitCXXAddrspaceCastExpr((clang::CXXAddrspaceCastExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::ImplicitCastExprClass:
    ret_status =
        VisitImplicitCastExpr((clang::ImplicitCastExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::CharacterLiteralClass:
    ret_status =
        VisitCharacterLiteral((clang::CharacterLiteral *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::ChooseExprClass:
    ret_status = VisitChooseExpr((clang::ChooseExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::CompoundLiteralExprClass:
    ret_status =
        VisitCompoundLiteralExpr((clang::CompoundLiteralExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::ConceptSpecializationExprClass:
    ret_status = VisitConceptSpecializationExpr(
        (clang::ConceptSpecializationExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::ConvertVectorExprClass:
    ret_status =
        VisitConvertVectorExpr((clang::ConvertVectorExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::CoawaitExprClass:
    ret_status = VisitCoawaitExpr((clang::CoawaitExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::CoyieldExprClass:
    ret_status = VisitCoyieldExpr((clang::CoyieldExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::CXXBindTemporaryExprClass:
    ret_status =
        VisitCXXBindTemporaryExpr((clang::CXXBindTemporaryExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::CXXBoolLiteralExprClass:
    ret_status =
        VisitCXXBoolLiteralExpr((clang::CXXBoolLiteralExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::CXXConstructExprClass:
    ret_status =
        VisitCXXConstructExpr((clang::CXXConstructExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::CXXTemporaryObjectExprClass:
    ret_status = VisitCXXTemporaryObjectExpr(
        (clang::CXXTemporaryObjectExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::CXXParenListInitExprClass:
    ret_status =
        VisitCXXParenListInitExpr((clang::CXXParenListInitExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::CXXDefaultArgExprClass:
    ret_status =
        VisitCXXDefaultArgExpr((clang::CXXDefaultArgExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::CXXDefaultInitExprClass:
    ret_status =
        VisitCXXDefaultInitExpr((clang::CXXDefaultInitExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::CXXDeleteExprClass:
    ret_status = VisitCXXDeleteExpr((clang::CXXDeleteExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::CXXDependentScopeMemberExprClass:
    ret_status = VisitCXXDependentScopeMemberExpr(
        (clang::CXXDependentScopeMemberExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::CXXFoldExprClass:
    ret_status = VisitCXXFoldExpr((clang::CXXFoldExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::CXXNewExprClass:
    ret_status = VisitCXXNewExpr((clang::CXXNewExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::CXXNoexceptExprClass:
    ret_status = VisitCXXNoexceptExpr((clang::CXXNoexceptExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::CXXNullPtrLiteralExprClass:
    ret_status = VisitCXXNullPtrLiteralExpr(
        (clang::CXXNullPtrLiteralExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::CXXPseudoDestructorExprClass:
    ret_status = VisitCXXPseudoDestructorExpr(
        (clang::CXXPseudoDestructorExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::CXXRewrittenBinaryOperatorClass:
    ret_status = VisitCXXRewrittenBinaryOperator(
        (clang::CXXRewrittenBinaryOperator *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::CXXScalarValueInitExprClass:
    ret_status = VisitCXXScalarValueInitExpr(
        (clang::CXXScalarValueInitExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::CXXStdInitializerListExprClass:
    ret_status = VisitCXXStdInitializerListExpr(
        (clang::CXXStdInitializerListExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::CXXThisExprClass:
    ret_status = VisitCXXThisExpr((clang::CXXThisExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::CXXThrowExprClass:
    ret_status = VisitCXXThrowExpr((clang::CXXThrowExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::CXXTypeidExprClass:
    ret_status = VisitCXXTypeidExpr((clang::CXXTypeidExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::CXXUnresolvedConstructExprClass:
    ret_status = VisitCXXUnresolvedConstructExpr(
        (clang::CXXUnresolvedConstructExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::DeclRefExprClass:
    ret_status = VisitDeclRefExpr((clang::DeclRefExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::DependentCoawaitExprClass:
    ret_status =
        VisitDependentCoawaitExpr((clang::DependentCoawaitExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::DependentScopeDeclRefExprClass:
    ret_status = VisitDependentScopeDeclRefExpr(
        (clang::DependentScopeDeclRefExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::DesignatedInitExprClass:
    ret_status =
        VisitDesignatedInitExpr((clang::DesignatedInitExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::DesignatedInitUpdateExprClass:
    ret_status = VisitDesignatedInitUpdateExpr(
        (clang::DesignatedInitUpdateExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::ExpressionTraitExprClass:
    ret_status =
        VisitExpressionTraitExpr((clang::ExpressionTraitExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::ExtVectorElementExprClass:
    ret_status =
        VisitExtVectorElementExpr((clang::ExtVectorElementExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::FloatingLiteralClass:
    ret_status = VisitFloatingLiteral((clang::FloatingLiteral *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::ConstantExprClass:
    ret_status = VisitConstantExpr((clang::ConstantExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::ExprWithCleanupsClass:
    ret_status =
        VisitExprWithCleanups((clang::ExprWithCleanups *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::GenericSelectionExprClass:
    ret_status =
        VisitGenericSelectionExpr((clang::GenericSelectionExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::GNUNullExprClass:
    ret_status = VisitGNUNullExpr((clang::GNUNullExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::ImaginaryLiteralClass:
    ret_status =
        VisitImaginaryLiteral((clang::ImaginaryLiteral *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::ImplicitValueInitExprClass:
    ret_status = VisitImplicitValueInitExpr(
        (clang::ImplicitValueInitExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::InitListExprClass:
    ret_status = VisitInitListExpr((clang::InitListExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::IntegerLiteralClass:
    ret_status = VisitIntegerLiteral((clang::IntegerLiteral *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::LambdaExprClass:
    ret_status = VisitLambdaExpr((clang::LambdaExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::MaterializeTemporaryExprClass:
    ret_status = VisitMaterializeTemporaryExpr(
        (clang::MaterializeTemporaryExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::MemberExprClass:
    ret_status = VisitMemberExpr((clang::MemberExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::MSPropertyRefExprClass:
    ret_status =
        VisitMSPropertyRefExpr((clang::MSPropertyRefExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::MSPropertySubscriptExprClass:
    ret_status = VisitMSPropertySubscriptExpr(
        (clang::MSPropertySubscriptExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::NoInitExprClass:
    ret_status = VisitNoInitExpr((clang::NoInitExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::OffsetOfExprClass:
    ret_status = VisitOffsetOfExpr((clang::OffsetOfExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::OpaqueValueExprClass:
    ret_status = VisitOpaqueValueExpr((clang::OpaqueValueExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::UnresolvedLookupExprClass:
    ret_status =
        VisitUnresolvedLookupExpr((clang::UnresolvedLookupExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::UnresolvedMemberExprClass:
    ret_status =
        VisitUnresolvedMemberExpr((clang::UnresolvedMemberExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::PackExpansionExprClass:
    ret_status =
        VisitPackExpansionExpr((clang::PackExpansionExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::ParenExprClass:
    ret_status = VisitParenExpr((clang::ParenExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::ParenListExprClass:
    ret_status = VisitParenListExpr((clang::ParenListExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::PredefinedExprClass:
    ret_status = VisitPredefinedExpr((clang::PredefinedExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::PseudoObjectExprClass:
    ret_status =
        VisitPseudoObjectExpr((clang::PseudoObjectExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::ShuffleVectorExprClass:
    ret_status =
        VisitShuffleVectorExpr((clang::ShuffleVectorExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::SizeOfPackExprClass:
    ret_status = VisitSizeOfPackExpr((clang::SizeOfPackExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::SourceLocExprClass:
    ret_status = VisitSourceLocExpr((clang::SourceLocExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::StmtExprClass:
    ret_status = VisitStmtExpr((clang::StmtExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::StringLiteralClass:
    ret_status = VisitStringLiteral((clang::StringLiteral *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::SubstNonTypeTemplateParmPackExprClass:
    ret_status = VisitSubstNonTypeTemplateParmPackExpr(
        (clang::SubstNonTypeTemplateParmPackExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::SubstNonTypeTemplateParmExprClass:
    ret_status = VisitSubstNonTypeTemplateParmExpr(
        (clang::SubstNonTypeTemplateParmExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::TypeTraitExprClass:
    ret_status = VisitTypeTraitExpr((clang::TypeTraitExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::UnaryExprOrTypeTraitExprClass:
    ret_status = VisitUnaryExprOrTypeTraitExpr(
        (clang::UnaryExprOrTypeTraitExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::VAArgExprClass:
    ret_status = VisitVAArgExpr((clang::VAArgExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::LabelStmtClass:
    ret_status = VisitLabelStmt((clang::LabelStmt *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::WhileStmtClass:
    ret_status = VisitWhileStmt((clang::WhileStmt *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::UnaryOperatorClass:
    ret_status = VisitUnaryOperator((clang::UnaryOperator *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::CallExprClass:
    ret_status = VisitCallExpr((clang::CallExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::BinaryOperatorClass:
    ret_status = VisitBinaryOperator((clang::BinaryOperator *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::RecoveryExprClass:
    failExactExpressionTranslation("clang-recovery-expression",
                                   static_cast<clang::RecoveryExpr *>(stmt));
  case clang::Stmt::RequiresExprClass:
    ret_status = VisitRequiresExpr((clang::RequiresExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;

  default:
    std::cerr << "Unknown statement kind: " << stmt->getStmtClassName() << " !"
              << std::endl;
    ROSE_ABORT();
  }

  if (!ret_status) {
    std::cerr << "REX_FRONTEND_INVARIANT[statement-visitor-status]: Clang "
              << stmt->getStmtClassName() << " visitor reported failure"
              << std::endl;
    ROSE_ABORT();
  }
  ROSE_ASSERT(result != nullptr);

  if (semantic_expression_construction) {
    SgExpression *semantic_expression = isSgExpression(result);
    if (semantic_expression == nullptr ||
        p_synthesized_source_nodes.count(semantic_expression) != 1) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[semantic-expression-provenance]: "
              "Clang expression=%s produced Sage=%p/%s with lexical=%zu "
              "semantic=%zu roles instead of one exact semantic "
              "expression\n",
              stmt->getStmtClassName(), static_cast<void *>(result),
              result != nullptr ? result->class_name().c_str() : "<null>",
              result != nullptr ? p_lexical_source_nodes.count(result) : 0,
              result != nullptr ? p_synthesized_source_nodes.count(result) : 0);
      ROSE_ABORT();
    }
  } else if (p_rebuild_translation_cache_depth != 0) {
    p_stmt_translation_map[stmt] = result;
  } else {
    p_stmt_translation_map.insert(
        std::pair<clang::Stmt *, SgNode *>(stmt, result));
  }

  return result;
}

bool ClangToSageTranslator::VisitStmt(clang::Stmt *stmt, SgNode **node) {
  if (stmt == nullptr || node == nullptr || *node == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[statement-node]: exact Clang "
                 "statement, Sage result slot, and Sage node are required"
              << std::endl;
    if (stmt != nullptr) {
      stmt->dump();
    }
    ROSE_ABORT();
  }

  auto has_real_stmt_source_info = [](SgLocatedNode *located) {
    auto has_real_file_info = [](Sg_File_Info *fi) {
      return fi != nullptr && fi->get_line() > 0 &&
             !fi->isCompilerGenerated() && !fi->isFrontendSpecific() &&
             !fi->isSourcePositionUnavailableInFrontend();
    };

    return located != nullptr && has_real_file_info(located->get_file_info()) &&
           has_real_file_info(located->get_startOfConstruct()) &&
           has_real_file_info(located->get_endOfConstruct());
  };

  auto require_synthesized_statement_provenance = [&]() {
    if (p_synthesized_source_nodes.count(*node) != 1 ||
        p_lexical_source_nodes.count(*node) != 0) {
      std::cerr << "REX_FRONTEND_INVARIANT[synthesized-provenance]: Sage "
                   "statement result has contradictory source roles"
                << std::endl;
      ROSE_ABORT();
    }
    SgLocatedNode *located = isSgLocatedNode(*node);
    if (located == nullptr) {
      std::cerr << "REX_FRONTEND_INVARIANT[synthesized-provenance]: registered "
                   "statement result is not an exact located node"
                << std::endl;
      ROSE_ABORT();
    }
    std::vector<Sg_File_Info *> file_infos = {located->get_file_info(),
                                              located->get_startOfConstruct(),
                                              located->get_endOfConstruct()};
    if (SgExpression *expression = isSgExpression(located)) {
      file_infos.push_back(expression->get_operatorPosition());
    }
    for (Sg_File_Info *file_info : file_infos) {
      if (file_info == nullptr || !file_info->isCompilerGenerated() ||
          !file_info->isOutputInCodeGeneration()) {
        std::cerr << "REX_FRONTEND_INVARIANT[synthesized-provenance]: node="
                  << static_cast<void *>(*node)
                  << " type=" << (*node)->class_name()
                  << " has inconsistent registered file information"
                  << std::endl;
        ROSE_ABORT();
      }
    }
  };

  if (p_synthesized_source_nodes.count(*node) != 0) {
    require_synthesized_statement_provenance();
    return true;
  }

  if (SgLocatedNode *located = isSgLocatedNode(*node)) {
    clang::SourceRange range = stmt->getSourceRange();
    if (p_lexical_source_nodes.count(*node) == 0) {
      range = extendSourceRangeWithTrailingSemicolon(
          range, p_compiler_instance->getSourceManager(),
          p_compiler_instance->getLangOpts());
      applySourceRange(*node, range);
    }
    // Exact semantic-expression and imported-external traversal contexts
    // deliberately classify their result during applySourceRange.  The base
    // visitor must validate that producer-selected role after publication;
    // requiring a lexical role here would contradict the construction
    // context that just published the node as semantic.
    if (p_synthesized_source_nodes.count(*node) != 0) {
      require_synthesized_statement_provenance();
      return true;
    }
    if (p_lexical_source_nodes.count(*node) != 1 ||
        !has_real_stmt_source_info(located)) {
      auto describe_file_info = [](Sg_File_Info *file_info) {
        if (file_info == nullptr) {
          return std::string("<null>");
        }
        std::ostringstream out;
        out << file_info->get_line() << ":" << file_info->get_col()
            << " physical=" << file_info->get_physical_file_id()
            << " compiler=" << file_info->isCompilerGenerated()
            << " frontend=" << file_info->isFrontendSpecific()
            << " unavailable="
            << file_info->isSourcePositionUnavailableInFrontend();
        return out.str();
      };
      std::cerr << "REX_FRONTEND_INVARIANT[statement-source-publication]: "
                   "Clang statement="
                << stmt->getStmtClassName()
                << " Sage node=" << (*node)->class_name()
                << " did not publish one complete lexical source range; "
                   "roles="
                << p_lexical_source_nodes.count(*node) << "/"
                << p_synthesized_source_nodes.count(*node)
                << " primary=" << describe_file_info(located->get_file_info())
                << " start="
                << describe_file_info(located->get_startOfConstruct())
                << " end=" << describe_file_info(located->get_endOfConstruct())
                << std::endl;
      ROSE_ABORT();
    }
  }

  return true;
}

namespace {
[[noreturn]] void rejectUnimplementedStatement(const clang::Stmt *statement) {
  std::cerr << "REX_FRONTEND_UNSUPPORTED[statement]: "
            << (statement != nullptr ? statement->getStmtClassName() : "<null>")
            << " has no typed AST implementation" << std::endl;
  ROSE_ABORT();
}
} // namespace

bool ClangToSageTranslator::VisitGCCAsmStmt(clang::GCCAsmStmt *gcc_asm_stmt,
                                            SgNode **node) {
  unsigned asmNumInput = gcc_asm_stmt->getNumInputs();
  unsigned asmNumOutput = gcc_asm_stmt->getNumOutputs();
  unsigned asmClobber = gcc_asm_stmt->getNumClobbers();

  std::string AsmString = gcc_asm_stmt->getAsmString();

  const clang::TargetInfo &target =
      p_compiler_instance->getASTContext().getTargetInfo();
  std::vector<clang::TargetInfo::ConstraintInfo> constraints;
  constraints.reserve(asmNumOutput + asmNumInput);
  for (unsigned i = 0; i < asmNumOutput; ++i) {
    constraints.emplace_back(gcc_asm_stmt->getOutputConstraint(i),
                             gcc_asm_stmt->getOutputName(i));
    const bool valid = target.validateOutputConstraint(constraints.back());
    ROSE_ASSERT(valid);
  }
  for (unsigned i = 0; i < asmNumInput; ++i) {
    constraints.emplace_back(gcc_asm_stmt->getInputConstraint(i),
                             gcc_asm_stmt->getInputName(i));
    const bool valid = target.validateInputConstraint(
        llvm::MutableArrayRef<clang::TargetInfo::ConstraintInfo>(
            constraints.data(), asmNumOutput),
        constraints.back());
    ROSE_ASSERT(valid);
  }

  SgAsmStmt *asmStmt = SageBuilder::buildAsmStatement(AsmString);
  asmStmt->set_firstNondefiningDeclaration(asmStmt);
  asmStmt->set_definingDeclaration(asmStmt);
  asmStmt->set_parent(
      statementConstructionOwner(gcc_asm_stmt, "GNU asm statement"));
  asmStmt->set_useGnuExtendedFormat(!gcc_asm_stmt->isSimple());
  asmStmt->set_isVolatile(gcc_asm_stmt->isVolatile());

  for (unsigned i = 0; i < asmClobber; ++i) {
    std::string clobberStr =
        static_cast<std::string>(gcc_asm_stmt->getClobber(i));
    if (clobberStr.empty()) {
      std::cerr << "REX_CFE_ASM_INVARIANT[clobber]: Clang produced an empty "
                   "clobber spelling\n";
      ROSE_ABORT();
    }
    asmStmt->get_clobberRegisterList().push_back(clobberStr);
  }

  for (bool output : {true, false}) {
    const unsigned count = output ? asmNumOutput : asmNumInput;
    for (unsigned i = 0; i < count; ++i) {
      auto &validated = constraints[output ? i : asmNumOutput + i];
      SgAsmOp::constraint_info constraint;
      constraint.spelling = validated.getConstraintStr();
      constraint.allows_memory = validated.allowsMemory();
      constraint.allows_register = validated.allowsRegister();
      constraint.requires_immediate = validated.requiresImmediateConstant();
      constraint.has_matching_input = validated.hasMatchingInput();
      if (validated.hasTiedOperand()) {
        ROSE_ASSERT(validated.getTiedOperand() < asmNumOutput);
        constraint.tied_output = validated.getTiedOperand();
      }
      if (validated.ImmRange.isConstrained)
        constraint.immediate_range =
            std::make_pair(validated.ImmRange.Min, validated.ImmRange.Max);
      constraint.immediate_values.insert(validated.ImmSet.begin(),
                                         validated.ImmSet.end());
      constraint.output_bounds = validated.getOutputOperandBounds();
      const size_t prefix_end =
          constraint.spelling.find_first_not_of("=+&%*#?!");
      ROSE_ASSERT(prefix_end != std::string::npos);
      unsigned modifiers = output
                               ? (validated.isReadWrite() ? SgAsmOp::e_modify
                                                          : SgAsmOp::e_output)
                               : SgAsmOp::e_input;
      if (output && validated.earlyClobber())
        modifiers |= SgAsmOp::e_earlyclobber;
      modifiers |=
          get_sgAsmOperandModifier(constraint.spelling.substr(0, prefix_end));

      clang::Expr *clang_operand = output ? gcc_asm_stmt->getOutputExpr(i)
                                          : gcc_asm_stmt->getInputExpr(i);
      SgExpression *expression = isSgExpression(Traverse(clang_operand));
      ROSE_ASSERT(expression != nullptr);
      auto *operand = new SgAsmOp(
          constraint,
          static_cast<SgAsmOp::asm_operand_modifier_enum>(modifiers),
          expression);
      expression->set_parent(operand);
      operand->set_name((output ? gcc_asm_stmt->getOutputName(i)
                                : gcc_asm_stmt->getInputName(i))
                            .str());
      // Clang gives no combined source range for the constraint and expression.
      // Their written syntax belongs to the enclosing SgAsmStmt.
      setSynthesizedFileInfo(operand);
      asmStmt->get_operands().push_back(operand);
      operand->set_parent(asmStmt);
    }
  }
  *node = asmStmt;

  const bool result = VisitStmt(gcc_asm_stmt, node);
  if (!result) {
    return false;
  }
  if (p_sage_preprocessor_recorder == nullptr) {
    std::cerr << "REX_CFE_ASM_INVARIANT[preprocessing-owner]: GNU asm "
                 "translation has no preprocessing recorder\n";
    ROSE_ABORT();
  }
  for (clang::SourceLocation include_location :
       p_sage_preprocessor_recorder->includeDirectiveLocationsStrictlyInside(
           gcc_asm_stmt->getSourceRange())) {
    p_sage_preprocessor_recorder->consumeIncludeDirectiveInFlattenedSyntax(
        include_location, asmStmt, "GNU-asm-string");
  }
  return true;
}

bool ClangToSageTranslator::VisitMSAsmStmt(clang::MSAsmStmt *ms_asm_stmt,
                                           SgNode **node) {
  return VisitStmt(ms_asm_stmt, node);
}

bool ClangToSageTranslator::VisitBreakStmt(clang::BreakStmt *break_stmt,
                                           SgNode **node) {
  *node = SageBuilder::buildBreakStmt_nfi();
  return VisitStmt(break_stmt, node);
}

bool ClangToSageTranslator::VisitCapturedStmt(
    clang::CapturedStmt *captured_stmt, SgNode **node) {
  clang::CapturedDecl *captured_decl = captured_stmt->getCapturedDecl();
  clang::Stmt *clang_body = captured_stmt->getCapturedStmt();
  if (captured_decl == nullptr || clang_body == nullptr ||
      captured_decl->getBody() != clang_body) {
    std::cerr << "REX_FRONTEND_INVARIANT[captured-statement-owner]: Clang "
                 "CapturedStmt does not own one exact CapturedDecl/body pair"
              << std::endl;
    ROSE_ABORT();
  }
  if (Traverse(captured_decl) != nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[captured-statement-owner]: semantic "
                 "CapturedDecl produced a standalone Sage surface"
              << std::endl;
    ROSE_ABORT();
  }
  SgStatement *body = translateStatement(clang_body, "captured statement body");

  *node = body;
  const size_t captured_source_roles = p_lexical_source_nodes.count(body) +
                                       p_synthesized_source_nodes.count(body);
  if (captured_source_roles != 1) {
    std::cerr << "REX_FRONTEND_INVARIANT[captured-statement-provenance]: "
                 "exact body did not publish one source/generated role"
              << std::endl;
    ROSE_ABORT();
  }
  return true;
}

void ClangToSageTranslator::prepareOpenMPAttributePragmas() {
  if (!p_compiler_instance->getLangOpts().CPlusPlus)
    return;
  auto &sm = p_compiler_instance->getSourceManager();
  const auto &options = p_compiler_instance->getLangOpts();
  const auto &tokens = p_expanded_tokens;
  auto kind = [&](size_t index, clang::tok::TokenKind expected) {
    return index < tokens.size() && tokens[index].kind == expected;
  };
  auto name = [&](size_t index, llvm::StringRef expected) {
    return index < tokens.size() &&
           tokens[index].identifier_spelling == expected;
  };
  auto attributeEnd = [&](size_t begin) {
    if (!kind(begin, clang::tok::l_square) ||
        !kind(begin + 1, clang::tok::l_square))
      return begin;
    unsigned depth = 0;
    for (size_t index = begin; index < tokens.size(); ++index) {
      if (kind(index, clang::tok::l_square))
        ++depth;
      else if (kind(index, clang::tok::r_square) && --depth == 0)
        return kind(index - 1, clang::tok::r_square) ? index + 1 : begin;
    }
    return begin;
  };
  for (size_t begin = 0; begin < tokens.size(); ++begin) {
    const size_t end = attributeEnd(begin);
    if (end == begin)
      continue;
    size_t anchor = end;
    while (attributeEnd(anchor) != anchor)
      anchor = attributeEnd(anchor);
    ROSE_ASSERT(anchor < tokens.size());
    size_t position = begin + 2;
    const size_t limit = end - 2;
    bool using_omp = false;
    if (kind(position, clang::tok::kw_using) && name(position + 1, "omp") &&
        kind(position + 2, clang::tok::colon)) {
      using_omp = true;
      position += 3;
    }
    std::vector<std::string> directives;
    std::function<void(size_t &, unsigned)> parse = [&](size_t &cursor,
                                                        unsigned depth) {
      ROSE_ASSERT(depth < 64 && cursor < limit);
      if (name(cursor, "omp") && kind(cursor + 1, clang::tok::coloncolon))
        cursor += 2;
      const bool sequence = name(cursor, "sequence");
      ROSE_ASSERT(sequence || name(cursor, "directive"));
      ++cursor;
      ROSE_ASSERT(kind(cursor, clang::tok::l_paren));
      ++cursor;
      if (sequence) {
        parse(cursor, depth + 1);
        while (kind(cursor, clang::tok::comma)) {
          ++cursor;
          parse(cursor, depth + 1);
        }
      } else {
        unsigned parentheses = 1;
        std::string directive = "omp";
        const size_t first = cursor;
        while (cursor < limit) {
          if (kind(cursor, clang::tok::l_paren))
            ++parentheses;
          else if (kind(cursor, clang::tok::r_paren) && --parentheses == 0)
            break;
          bool invalid = false;
          const std::string spelling = clang::Lexer::getSpelling(
              tokens[cursor].token, sm, options, &invalid);
          ROSE_ASSERT(!invalid && !spelling.empty());
          directive += " " + spelling;
          ++cursor;
        }
        ROSE_ASSERT(cursor != first && parentheses == 0);
        directives.push_back(std::move(directive));
      }
      ROSE_ASSERT(cursor < limit && kind(cursor, clang::tok::r_paren));
      ++cursor;
    };
    while (position < limit) {
      if (using_omp || (name(position, "omp") &&
                        kind(position + 1, clang::tok::coloncolon))) {
        parse(position, 0);
      } else {
        unsigned parentheses = 0;
        while (position < limit) {
          if (kind(position, clang::tok::l_paren))
            ++parentheses;
          else if (kind(position, clang::tok::r_paren)) {
            ROSE_ASSERT(parentheses != 0);
            --parentheses;
          } else if (parentheses == 0 && kind(position, clang::tok::comma))
            break;
          ++position;
        }
        ROSE_ASSERT(parentheses == 0);
      }
      if (position != limit) {
        ROSE_ASSERT(kind(position, clang::tok::comma));
        ++position;
      }
    }
    if (!directives.empty()) {
      const clang::SourceLocation start =
          sm.getExpansionRange(tokens[begin].location).getBegin();
      const clang::SourceLocation finish =
          sm.getExpansionRange(tokens[end - 1].location).getEnd();
      ROSE_ASSERT(start.isFileID() && finish.isFileID() &&
                  sm.getFileID(start) == sm.getFileID(finish));
      const auto first = sm.getPresumedLoc(start);
      const auto last = sm.getPresumedLoc(finish);
      ROSE_ASSERT(first.isValid() && last.isValid() &&
                  std::string(first.getFilename()) == last.getFilename());
      for (const auto &directive : directives) {
        p_openmp_attribute_pragmas[static_cast<unsigned>(anchor + 1)].push_back(
            {sm.getFileID(start), sm.getSpellingLineNumber(start),
             first.getFilename(), first.getLine(), first.getColumn(),
             last.getLine(), last.getColumn(), directive,
             "[[omp::directive(" + directive.substr(4) + ")]]", true, false,
             tokens[begin].location.isMacroID(),
             clang::SourceRange(start, finish)});
      }
    }
    begin = end - 1;
  }
}

void ClangToSageTranslator::collectOpenMPAttributePragmas(
    clang::Stmt *stmt, std::vector<CapturedPragma> &pragmas) {
  if (p_openmp_attribute_pragmas.empty() ||
      p_semantic_function_body_traversal_depth != 0 ||
      p_semantic_expression_subtree_depth != 0)
    return;
  while (auto *attributed = llvm::dyn_cast<clang::AttributedStmt>(stmt))
    stmt = attributed->getSubStmt();
  const unsigned order = requireExpandedTokenSourceOrder(
      stmt->getBeginLoc(), "OpenMP attribute target",
      ExpandedTokenBoundary::first);
  const auto found = p_openmp_attribute_pragmas.find(order);
  if (found == p_openmp_attribute_pragmas.end())
    return;
  pragmas.insert(pragmas.end(), found->second.begin(), found->second.end());
  p_openmp_attribute_pragmas.erase(found);
}

bool ClangToSageTranslator::collectPragmas(
    clang::Stmt *stmt, std::vector<CapturedPragma> &pragmas) {
  pragmas.clear();

  if (p_semantic_function_body_traversal_depth != 0 ||
      p_semantic_expression_subtree_depth != 0)
    return false;

  if (p_openmp_pragma_callback == nullptr || stmt == nullptr) {
    return false;
  }
  const auto &pragma_map = p_openmp_pragma_callback->getPragmaMap();
  if (pragma_map.empty()) {
    return false;
  }

  clang::SourceLocation loc = stmt->getBeginLoc();
  if (!loc.isValid()) {
    return false;
  }

  clang::SourceManager &source_manager =
      p_compiler_instance->getSourceManager();
  clang::SourceLocation file_loc = source_manager.getFileLoc(loc);
  ROSE_ASSERT(file_loc.isValid());

  clang::FileID file_id = source_manager.getFileID(file_loc);
  // Presumed-file line markers can classify a range inside the physical main
  // file as system-header text. Such statements are semantic compiler input,
  // not source surfaces emitted by this translation unit.
  if (source_manager.isInSystemHeader(file_loc)) {
    return false;
  }
  std::set<std::pair<clang::FileID, unsigned>>
      semantically_owned_loop_pragma_lines;
  clang::Stmt *token_owner = stmt;
  for (clang::Stmt *current = stmt; current != nullptr;) {
    clang::AttributedStmt *attributed =
        llvm::dyn_cast<clang::AttributedStmt>(current);
    if (attributed == nullptr) {
      break;
    }

    for (const clang::Attr *attribute : attributed->getAttrs()) {
      const clang::LoopHintAttr *loop_hint =
          llvm::dyn_cast_or_null<clang::LoopHintAttr>(attribute);
      if (loop_hint == nullptr ||
          loop_hint->getSyntax() != clang::AttributeCommonInfo::AS_Pragma ||
          !loop_hint->getRange().isValid()) {
        continue;
      }

      clang::SourceLocation pragma_loc =
          source_manager.getFileLoc(loop_hint->getRange().getBegin());
      ROSE_ASSERT(pragma_loc.isValid());
      const clang::FileID pragma_file_id = source_manager.getFileID(pragma_loc);
      const unsigned pragma_line =
          source_manager.getSpellingLineNumber(pragma_loc);
      if (pragma_file_id.isInvalid() || pragma_line == 0 ||
          pragma_map.find({pragma_file_id, pragma_line}) == pragma_map.end()) {
        std::cerr << "REX_CFE_PRAGMA_INVARIANT[semantic-owner]: source-written "
                     "Clang loop hint has no exact captured pragma surface"
                  << std::endl;
        ROSE_ABORT();
      }
      semantically_owned_loop_pragma_lines.insert(
          std::make_pair(pragma_file_id, pragma_line));
    }

    if (token_owner == attributed &&
        std::all_of(attributed->getAttrs().begin(),
                    attributed->getAttrs().end(), [](const clang::Attr *attr) {
                      return attr->getSyntax() ==
                             clang::AttributeCommonInfo::AS_Pragma;
                    })) {
      token_owner = attributed->getSubStmt();
      ROSE_ASSERT(token_owner != nullptr);
    }
    current = attributed->getSubStmt();
  }

  if (!pragmaFileHasLexicalOwner(file_id)) {
    return false;
  }
  const unsigned statement_order = requireExpandedTokenSourceOrder(
      token_owner->getBeginLoc(), "leading pragma statement",
      ExpandedTokenBoundary::first);
  ROSE_ASSERT(statement_order > 0 &&
              statement_order <= p_expanded_tokens.size());
  const clang::SourceLocation statement_token =
      p_expanded_tokens[statement_order - 1].location;
  const clang::SourceLocation previous_token =
      statement_order == 1 ? clang::SourceLocation()
                           : p_expanded_tokens[statement_order - 2].location;

  for (auto captured = pragma_map.lower_bound({file_id, 1});
       captured != pragma_map.end() && captured->first.first == file_id;
       ++captured) {
    const auto &physical_key = captured->first;
    const auto &pragma = captured->second;
    ROSE_ASSERT(pragma.physical_range.isValid());
    if (!source_manager.isBeforeInTranslationUnit(
            pragma.physical_range.getBegin(), statement_token) ||
        (previous_token.isValid() &&
         !source_manager.isBeforeInTranslationUnit(
             previous_token, pragma.physical_range.getBegin()))) {
      continue;
    }
    if (pragma.is_in_system_header ||
        p_consumed_pragma_lines.count(physical_key) != 0 ||
        semantically_owned_loop_pragma_lines.count(physical_key) != 0) {
      continue;
    }

    std::string directive_text = extractPragmaDirective(pragma.text);
    std::string source_directive_text =
        extractPragmaDirective(pragma.source_text);
    if (directive_text.empty() || source_directive_text.empty()) {
      std::cerr << "REX_CFE_PRAGMA_INVARIANT[source-text]: captured pragma "
                   "has no parser or source directive payload"
                << std::endl;
      ROSE_ABORT();
    }
    if (pragma.is_structured_openmp) {
      switch (
          p_openmp_pragma_callback->requireStructuredOpenMPDirectiveKindAtLine(
              physical_key.first, physical_key.second)) {
      case OMPD_end_declare_target:
      case OMPD_end_declare_variant:
      case OMPD_end_assume:
      case OMPD_end_assumes:
        pragmas.clear();
        continue;
      default:
        break;
      }
    }

    pragmas.push_back(
        {file_id, physical_key.second, pragma.logical_filename,
         pragma.logical_line, pragma.logical_column, pragma.logical_end_line,
         pragma.logical_end_column, directive_text, source_directive_text,
         pragma.is_structured_openmp || pragma.is_structured_openacc,
         pragma.is_rose_semantic, pragma.has_top_level_macro_expansion});
  }

  for (const CapturedPragma &pragma : pragmas) {
    p_consumed_pragma_lines.insert(
        {pragma.physical_file_id, pragma.physical_line});
  }
  p_consumed_pragma_lines.insert(semantically_owned_loop_pragma_lines.begin(),
                                 semantically_owned_loop_pragma_lines.end());

  return !pragmas.empty();
}

bool ClangToSageTranslator::pragmaFileHasLexicalOwner(
    clang::FileID file_id) const {
  clang::SourceManager &sm = p_compiler_instance->getSourceManager();
  ROSE_ASSERT(file_id.isValid());
  const clang::SourceLocation start = sm.getLocForStartOfFile(file_id);
  ROSE_ASSERT(start.isValid());
  if (sm.isWrittenInBuiltinFile(start) ||
      sm.isWrittenInCommandLineFile(start)) {
    return false;
  }
  if (file_id == sm.getMainFileID()) {
    return true;
  }
  const auto file = sm.getFileEntryRefForID(file_id);
  ROSE_ASSERT(file.has_value() && p_sage_preprocessor_recorder != nullptr);
  return p_sage_preprocessor_recorder->includeOwnershipForPath(
             file->getName().str()) ==
         SagePreprocessorRecord::IncludeOwnership::application_textual;
}

void ClangToSageTranslator::planLexicalPragmas(
    clang::TranslationUnitDecl *translation_unit) {
  ROSE_ASSERT(translation_unit != nullptr && p_lexical_pragmas.empty());
  if (p_openmp_pragma_callback == nullptr) {
    return;
  }
  clang::SourceManager &sm = p_compiler_instance->getSourceManager();
  std::vector<PhysicalPragmaKey> keys;
  const auto &captures = p_openmp_pragma_callback->getPragmaMap();
  for (const auto &capture : captures) {
    if (!capture.second.is_in_system_header &&
        pragmaFileHasLexicalOwner(capture.first.first)) {
      ROSE_ASSERT(capture.first.second > 0 &&
                  capture.second.physical_range.isValid());
      keys.push_back(capture.first);
    }
  }
  if (keys.empty()) {
    return;
  }
  std::sort(keys.begin(), keys.end(), [&](const auto &lhs, const auto &rhs) {
    return sm.isBeforeInTranslationUnit(
        captures.at(lhs).physical_range.getBegin(),
        captures.at(rhs).physical_range.getBegin());
  });

  struct Scope {
    clang::SourceRange braces;
    LexicalPragmaOwner owner;
  };
  std::vector<Scope> scopes;
  auto has_pragma = [&](clang::SourceRange braces) {
    ROSE_ASSERT(braces.isValid());
    const auto first = std::upper_bound(
        keys.begin(), keys.end(), braces.getBegin(),
        [&](clang::SourceLocation begin, const PhysicalPragmaKey &key) {
          return sm.isBeforeInTranslationUnit(
              begin, captures.at(key).physical_range.getBegin());
        });
    return first != keys.end() &&
           sm.isBeforeInTranslationUnit(
               captures.at(*first).physical_range.getEnd(), braces.getEnd());
  };
  auto add_scope = [&](clang::SourceRange braces, LexicalPragmaOwner owner) {
    if (has_pragma(braces)) {
      scopes.push_back({braces, owner});
    }
  };
  struct ScopeCollector : clang::RecursiveASTVisitor<ScopeCollector> {
    std::vector<clang::Stmt *> statements;
    std::function<void(clang::SourceRange, LexicalPragmaOwner)> add;
    std::function<void(const clang::Expr *, const clang::Stmt *)>
        add_expression;
    std::function<clang::SourceRange(const clang::NamespaceDecl *)>
        namespace_braces;
    bool dataTraverseStmtPre(clang::Stmt *statement) {
      statements.push_back(statement);
      return true;
    }
    bool dataTraverseStmtPost(clang::Stmt *statement) {
      ROSE_ASSERT(!statements.empty() && statements.back() == statement);
      statements.pop_back();
      return true;
    }
    bool VisitExpr(clang::Expr *expression) {
      if (statements.size() > 1) {
        auto *parent = statements[statements.size() - 2];
        bool value_child = llvm::isa<clang::Expr, clang::ReturnStmt>(parent);
        if (auto *declarations = llvm::dyn_cast<clang::DeclStmt>(parent)) {
          for (auto *declaration : declarations->decls()) {
            if (auto *variable = llvm::dyn_cast<clang::VarDecl>(declaration))
              value_child |=
                  !variable->isImplicit() && variable->getInit() == expression;
          }
        } else if (auto *selector = llvm::dyn_cast<clang::SwitchStmt>(parent)) {
          value_child = selector->getInit() == expression ||
                        (selector->getConditionVariable() == nullptr &&
                         selector->getCond() == expression);
        } else if (auto *label = llvm::dyn_cast<clang::CaseStmt>(parent)) {
          value_child =
              label->getLHS() == expression || label->getRHS() == expression;
        }
        if (value_child) {
          if (auto *call = llvm::dyn_cast<clang::CXXOperatorCallExpr>(parent);
              call != nullptr && call->getCallee() == expression)
            return true;
          if (std::find(parent->child_begin(), parent->child_end(),
                        expression) != parent->child_end())
            add_expression(expression, parent);
        }
      }
      return true;
    }
    bool VisitCompoundStmt(clang::CompoundStmt *body) {
      if (body->getSourceRange().isValid()) {
        add(body->getSourceRange(), body);
      }
      return true;
    }
    bool VisitRecordDecl(clang::RecordDecl *record) {
      if (!record->isImplicit() && !record->isFromASTFile() &&
          record->isThisDeclarationADefinition() &&
          record->getBraceRange().isValid()) {
        add(record->getBraceRange(),
            static_cast<const clang::DeclContext *>(record));
      }
      return true;
    }
    bool VisitNamespaceDecl(clang::NamespaceDecl *ns) {
      if (!ns->isImplicit() && !ns->isFromASTFile() &&
          ns->getBeginLoc().isValid() && ns->getRBraceLoc().isValid()) {
        add(namespace_braces(ns), static_cast<const clang::DeclContext *>(ns));
      }
      return true;
    }
  } collector;
  collector.add = add_scope;
  collector.add_expression = [&](const clang::Expr *expression,
                                 const clang::Stmt *parent) {
    const clang::Expr *source = expression->IgnoreUnlessSpelledInSource();
    const auto range = source->getSourceRange();
    const auto parent_range = parent->getSourceRange();
    if (range.isInvalid() || parent_range.isInvalid() ||
        sm.isBeforeInTranslationUnit(range.getBegin(),
                                     parent_range.getBegin()) ||
        sm.isBeforeInTranslationUnit(parent_range.getEnd(), range.getEnd()) ||
        !has_pragma(parent_range))
      return;
    const unsigned first = requireExpandedTokenSourceOrder(
        range.getBegin(), "pragma expression begin",
        ExpandedTokenBoundary::first);
    const unsigned last = requireExpandedTokenSourceOrder(
        range.getEnd(), "pragma expression end", ExpandedTokenBoundary::last);
    if (first > 1) {
      const auto previous = p_expanded_tokens.at(first - 2).location;
      if (!sm.isBeforeInTranslationUnit(previous, parent_range.getBegin()))
        add_scope({previous, p_expanded_tokens.at(first - 1).location},
                  ExpressionPragmaOwner{source, PreprocessingInfo::before});
    }
    if (last < p_expanded_tokens.size()) {
      const auto next = p_expanded_tokens.at(last).location;
      if (!sm.isBeforeInTranslationUnit(parent_range.getEnd(), next))
        add_scope({p_expanded_tokens.at(last - 1).location, next},
                  ExpressionPragmaOwner{source, PreprocessingInfo::after});
    }
  };
  collector.namespace_braces = [&](const clang::NamespaceDecl *ns) {
    const NamespaceSourceRanges ranges = namespaceSourceRanges(ns);
    return clang::SourceRange(
        p_expanded_tokens.at(ranges.opening_brace_source_order - 1).location,
        p_expanded_tokens.at(ranges.closing_brace_source_order - 1).location);
  };
  ROSE_ASSERT(collector.TraverseDecl(translation_unit));

  for (const PhysicalPragmaKey &key : keys) {
    const clang::SourceRange pragma = captures.at(key).physical_range;
    const Scope *owner = nullptr;
    for (const Scope &candidate : scopes) {
      if (!sm.isBeforeInTranslationUnit(candidate.braces.getBegin(),
                                        pragma.getBegin()) ||
          !sm.isBeforeInTranslationUnit(pragma.getEnd(),
                                        candidate.braces.getEnd())) {
        continue;
      }
      if (owner == nullptr || owner->owner == candidate.owner) {
        owner = &candidate;
        continue;
      }
      if (candidate.braces == owner->braces) {
        const auto *inner =
            std::get_if<const clang::DeclContext *>(&candidate.owner);
        const auto *outer =
            std::get_if<const clang::DeclContext *>(&owner->owner);
        ROSE_ASSERT(inner != nullptr && outer != nullptr);
        bool nested = false;
        for (const clang::DeclContext *parent = (*inner)->getLexicalParent();
             parent != nullptr; parent = parent->getLexicalParent()) {
          if (parent == *outer) {
            nested = true;
            break;
          }
        }
        if (nested) {
          owner = &candidate;
        } else {
          bool enclosing = false;
          for (const clang::DeclContext *parent = (*outer)->getLexicalParent();
               parent != nullptr; parent = parent->getLexicalParent()) {
            enclosing = enclosing || parent == *inner;
          }
          ROSE_ASSERT(enclosing);
        }
        continue;
      }
      const bool candidate_inside =
          !sm.isBeforeInTranslationUnit(candidate.braces.getBegin(),
                                        owner->braces.getBegin()) &&
          !sm.isBeforeInTranslationUnit(owner->braces.getEnd(),
                                        candidate.braces.getEnd());
      const bool owner_inside =
          !sm.isBeforeInTranslationUnit(owner->braces.getBegin(),
                                        candidate.braces.getBegin()) &&
          !sm.isBeforeInTranslationUnit(candidate.braces.getEnd(),
                                        owner->braces.getEnd());
      ROSE_ASSERT(candidate_inside != owner_inside);
      if (candidate_inside) {
        owner = &candidate;
      }
    }
    const LexicalPragmaOwner lexical_owner =
        owner != nullptr
            ? owner->owner
            : LexicalPragmaOwner(
                  static_cast<const clang::DeclContext *>(translation_unit));
    if (std::holds_alternative<ExpressionPragmaOwner>(lexical_owner)) {
      const auto &capture = captures.at(key);
      if (capture.is_structured_openmp || capture.is_structured_openacc ||
          capture.is_rose_semantic) {
        std::cerr << "REX_CFE_PRAGMA_INVARIANT[expression-owner]: structured "
                     "directive cannot appear inside an expression\n";
        ROSE_ABORT();
      }
    }
    p_lexical_pragmas[lexical_owner].push_back(key);
  }
}

void ClangToSageTranslator::constructLexicalPragmas(LexicalPragmaOwner owner,
                                                    SgScopeStatement *scope) {
  const auto planned = p_lexical_pragmas.find(owner);
  if (planned == p_lexical_pragmas.end()) {
    return;
  }
  ROSE_ASSERT(scope != nullptr && p_openmp_pragma_callback != nullptr);
  clang::SourceManager &sm = p_compiler_instance->getSourceManager();
  const auto &captures = p_openmp_pragma_callback->getPragmaMap();
  for (const PhysicalPragmaKey &key : planned->second) {
    if (p_consumed_pragma_lines.count(key) != 0) {
      continue;
    }
    const auto &capture = captures.at(key);
    SgPragmaDeclaration *pragma = buildCapturedPragmaDeclaration(
        extractPragmaDirective(capture.text),
        extractPragmaDirective(capture.source_text), key.first, key.second,
        capture.logical_filename, capture.logical_line, capture.logical_column,
        capture.logical_end_line, capture.logical_end_column, scope,
        capture.has_top_level_macro_expansion,
        isSgGlobal(scope) != nullptr && key.first != sm.getMainFileID());
    auto follows = [&](SgStatement *child) {
      ROSE_ASSERT(child != nullptr && child->get_parent() == scope);
      const auto source = p_lexical_source_nodes.find(child);
      if (source == p_lexical_source_nodes.end()) {
        if (auto *ns = isSgNamespaceDeclarationStatement(child);
            ns != nullptr && ns->has_source_fragments() &&
            ns->get_opening_source_fragment()->get_source_form() ==
                SgNamespaceSourceFragment::
                    e_namespace_source_fragment_canonical_generated) {
          ns->validate_source_fragments();
          return false;
        }
        if (p_synthesized_source_nodes.count(child) != 0 ||
            p_semantic_auxiliary_source_nodes.count(child) != 0) {
          return false;
        }
        std::cerr << "REX_CFE_PRAGMA_INVARIANT[child-source]: direct child "
                  << child->class_name()
                  << " has no typed construction provenance" << std::endl;
        ROSE_ABORT();
      }
      return sm.isBeforeInTranslationUnit(capture.physical_range.getEnd(),
                                          source->second.getBegin());
    };
    if (auto *declarations = getScopeDeclarationList(scope)) {
      declarations->insert(
          std::find_if(declarations->begin(), declarations->end(), follows),
          pragma);
      p_decl_attachment_session.recordInsertion(declarations, pragma);
    } else {
      SgBasicBlock *block = isSgBasicBlock(scope);
      ROSE_ASSERT(block != nullptr);
      auto &statements = block->get_statements();
      statements.insert(
          std::find_if(statements.begin(), statements.end(), follows), pragma);
    }
    pragma->set_parent(scope);
    pragma->set_scope(scope);
    ROSE_ASSERT(p_consumed_pragma_lines.insert(key).second);
  }
}

void ClangToSageTranslator::constructExpressionPragmas(
    const clang::Expr *source, SgExpression *expression) {
  for (auto position : {PreprocessingInfo::before, PreprocessingInfo::after}) {
    const auto planned =
        p_lexical_pragmas.find(ExpressionPragmaOwner{source, position});
    if (planned == p_lexical_pragmas.end())
      continue;
    std::vector<CapturedPragma> pragmas;
    const auto &captures = p_openmp_pragma_callback->getPragmaMap();
    for (const PhysicalPragmaKey &key : planned->second) {
      if (p_consumed_pragma_lines.count(key) != 0)
        continue;
      const auto &capture = captures.at(key);
      pragmas.push_back({key.first, key.second, capture.logical_filename,
                         capture.logical_line, capture.logical_column,
                         capture.logical_end_line, capture.logical_end_column,
                         extractPragmaDirective(capture.text),
                         extractPragmaDirective(capture.source_text), false,
                         false, capture.has_top_level_macro_expansion});
    }
    attachCapturedPragmas(expression, pragmas, position,
                          "expression-attachment");
    for (const CapturedPragma &pragma : pragmas)
      ROSE_ASSERT(p_consumed_pragma_lines
                      .insert({pragma.physical_file_id, pragma.physical_line})
                      .second);
  }
}

void ClangToSageTranslator::attachCapturedPragmas(
    SgLocatedNode *owner, const std::vector<CapturedPragma> &pragmas,
    PreprocessingInfo::RelativePositionType position, const char *context) {
  ASSERT_not_null(owner);
  for (const CapturedPragma &pragma : pragmas) {
    if (pragma.filename.empty() || pragma.line == 0 || pragma.column == 0 ||
        pragma.end_line < pragma.line || pragma.end_column == 0) {
      std::cerr << "REX_CFE_PRAGMA_INVARIANT[" << context
                << "]: pragma has an invalid source interval" << std::endl;
      ROSE_ABORT();
    }
    std::string source_text = "#pragma";
    if (!pragma.source_text.empty())
      source_text += " " + pragma.source_text;
    if (source_text.back() != '\n')
      source_text.push_back('\n');
    auto *record = new PreprocessingInfo(
        PreprocessingInfo::CpreprocessorPragmaDeclaration, source_text,
        pragma.filename, pragma.line, pragma.column,
        pragma.end_line - pragma.line, position);
    ASSERT_not_null(record);
    ASSERT_not_null(record->get_file_info());
    record->get_file_info()->set_physical_file_occurrence_id(
        p_sage_preprocessor_recorder->requirePhysicalFileOccurrence(
            pragma.physical_file_id, context));
    owner->attachPreprocessingInfo(record, position);
  }
}

SgPragmaDeclaration *ClangToSageTranslator::buildCapturedPragmaDeclaration(
    const std::string &directive, const std::string &source_directive,
    clang::FileID physical_file_id, unsigned physical_line,
    const std::string &filename, unsigned pragma_line, unsigned pragma_column,
    unsigned pragma_end_line, unsigned pragma_end_column,
    SgScopeStatement *scope, bool has_top_level_macro_expansion,
    bool source_file_only_owner,
    std::optional<clang::SourceRange> attribute_range) {
  if (scope == nullptr || directive.empty() || source_directive.empty() ||
      filename.empty() || physical_file_id.isInvalid() || physical_line == 0 ||
      pragma_line == 0 || pragma_column == 0 || pragma_end_line < pragma_line ||
      pragma_end_column == 0 ||
      (pragma_end_line == pragma_line && pragma_end_column < pragma_column)) {
    std::cerr << "REX_CFE_PRAGMA_INVARIANT[construction]: invalid pragma "
                 "text, scope, or source interval"
              << std::endl;
    ROSE_ABORT();
  }

  SgPragmaDeclaration *pragma_decl =
      SageBuilder::buildPragmaDeclaration_nfi(directive, scope);
  SgPragma *pragma =
      pragma_decl != nullptr ? pragma_decl->get_pragma() : nullptr;
  if (pragma_decl == nullptr || pragma == nullptr) {
    std::cerr << "REX_CFE_PRAGMA_INVARIANT[construction]: captured pragma "
                 "builder returned an incomplete declaration"
              << std::endl;
    ROSE_ABORT();
  }

  const std::string &pragma_filename = filename;
  clang::SourceManager &source_manager =
      p_compiler_instance->getSourceManager();
  const std::string physical_filename =
      p_sage_preprocessor_recorder->getPhysicalFilename(physical_file_id);
  ROSE_ASSERT(p_openmp_pragma_callback != nullptr);
  const clang::SourceRange physical_range =
      attribute_range.has_value() ? *attribute_range
                                  : p_openmp_pragma_callback->getPragmaMap()
                                        .at({physical_file_id, physical_line})
                                        .physical_range;
  ROSE_ASSERT(physical_range.isValid() &&
              source_manager.getFileID(physical_range.getBegin()) ==
                  physical_file_id &&
              source_manager.getSpellingLineNumber(physical_range.getBegin()) ==
                  physical_line);
  const unsigned physical_end_line =
      source_manager.getSpellingLineNumber(physical_range.getEnd());
  ROSE_ASSERT(physical_end_line >= physical_line);
  if (p_sage_preprocessor_recorder == nullptr) {
    std::cerr << "REX_CFE_PRAGMA_INVARIANT[physical-source]: captured pragma "
                 "has no parse-session physical occurrence allocator"
              << std::endl;
    ROSE_ABORT();
  }
  const unsigned physical_file_occurrence =
      p_sage_preprocessor_recorder->requirePhysicalFileOccurrence(
          physical_file_id, "captured-pragma");
  if (physical_file_occurrence == 0) {
    std::cerr << "REX_CFE_PRAGMA_INVARIANT[physical-source]: captured pragma "
                 "has no exact parse-session physical occurrence"
              << std::endl;
    ROSE_ABORT();
  }
  // The frontend supplies the complete source interval once. The no-file-info
  // builder leaves no transformation positions to overwrite or abandon.
  ROSE_ASSERT(pragma_decl->get_startOfConstruct() == nullptr &&
              pragma_decl->get_endOfConstruct() == nullptr &&
              pragma->get_startOfConstruct() == nullptr &&
              pragma->get_endOfConstruct() == nullptr);
  Sg_File_Info *start_fi =
      new Sg_File_Info(pragma_filename, pragma_line, pragma_column);
  Sg_File_Info *end_fi =
      new Sg_File_Info(pragma_filename, pragma_end_line, pragma_end_column);
  Sg_File_Info *pragma_start_fi =
      new Sg_File_Info(pragma_filename, pragma_line, pragma_column);
  Sg_File_Info *pragma_end_fi =
      new Sg_File_Info(pragma_filename, pragma_end_line, pragma_end_column);
  auto set_physical_source = [&](Sg_File_Info *info, unsigned line) {
    ROSE_ASSERT(info != nullptr);
    info->set_physical_filename(physical_filename);
    info->set_physical_line(static_cast<int>(line));
    info->set_physical_file_occurrence_id(physical_file_occurrence);
  };
  set_physical_source(start_fi, physical_line);
  set_physical_source(end_fi, physical_end_line);
  set_physical_source(pragma_start_fi, physical_line);
  set_physical_source(pragma_end_fi, physical_end_line);
  start_fi->unsetCompilerGenerated();
  start_fi->setOutputInCodeGeneration();
  end_fi->unsetCompilerGenerated();
  end_fi->setOutputInCodeGeneration();
  pragma_start_fi->unsetCompilerGenerated();
  pragma_start_fi->setOutputInCodeGeneration();
  pragma_end_fi->unsetCompilerGenerated();
  pragma_end_fi->setOutputInCodeGeneration();

  pragma_decl->set_startOfConstruct(start_fi);
  pragma_decl->set_endOfConstruct(end_fi);
  start_fi->set_parent(pragma_decl);
  end_fi->set_parent(pragma_decl);
  pragma->set_startOfConstruct(pragma_start_fi);
  pragma->set_endOfConstruct(pragma_end_fi);
  pragma_start_fi->set_parent(pragma);
  pragma_end_fi->set_parent(pragma);
  pragma_decl->unsetCompilerGenerated();
  pragma_decl->setOutputInCodeGeneration();
  pragma_decl->set_parent(scope);
  if (source_directive.empty() || !pragma_decl->get_cxx_source_text().empty() ||
      pragma_decl->get_cxx_top_level_macro_expansion() ||
      pragma_decl->get_cxx_pragma_payload_kind() !=
          SgPragmaDeclaration::e_cxx_pragma_generated_semantic) {
    std::cerr << "REX_CFE_OMP_INVARIANT[source-provenance]: captured pragma "
                 "has empty, duplicate, or stale typed source provenance\n";
    ROSE_ABORT();
  }
  pragma_decl->set_cxx_pragma_payload_kind(
      attribute_range.has_value()
          ? SgPragmaDeclaration::e_cxx_pragma_source_attribute
      : source_file_only_owner
          ? SgPragmaDeclaration::e_cxx_pragma_source_file_only
          : SgPragmaDeclaration::e_cxx_pragma_source_spelled);
  pragma_decl->set_cxx_source_text(source_directive);
  pragma_decl->set_cxx_top_level_macro_expansion(has_top_level_macro_expansion);
  ROSE_ASSERT(
      p_lexical_source_nodes.emplace(pragma_decl, physical_range).second);
  ROSE_ASSERT(p_lexical_source_nodes.emplace(pragma, physical_range).second);

  if (source_file_only_owner) {
    if (isSgGlobal(scope) == nullptr) {
      std::cerr << "REX_CFE_PRAGMA_INVARIANT[source-file-only-owner]: "
                   "preprocessing-only pragma has a semantic scope\n";
      ROSE_ABORT();
    }
    return pragma_decl;
  }

  const bool is_structured_openmp =
      attribute_range.has_value() ||
      (p_openmp_pragma_callback != nullptr &&
       p_openmp_pragma_callback->isStructuredOpenMPPragmaAtLine(
           physical_file_id, physical_line));
  const bool is_structured_openacc =
      p_openmp_pragma_callback != nullptr &&
      p_openmp_pragma_callback->isStructuredOpenACCPragmaAtLine(
          physical_file_id, physical_line);
  if (is_structured_openmp && is_structured_openacc) {
    std::cerr << "REX_CFE_PRAGMA_INVARIANT[directive-family]: captured pragma "
                 "has both OpenMP and OpenACC semantic identities\n";
    ROSE_ABORT();
  }
  const auto *clang_bindings =
      is_structured_openacc && p_openmp_pragma_callback != nullptr
          ? p_openmp_pragma_callback
                ->materializeOpenACCCxxExactSemanticBindingsAtLine(
                    physical_file_id, physical_line)
          : nullptr;
  if (is_structured_openacc && clang_bindings == nullptr) {
    std::cerr << "REX_CFE_ACC_INVARIANT[semantic-capture]: structured OpenACC "
                 "pragma has no producer binding record\n";
    ROSE_ABORT();
  }
  if (clang_bindings != nullptr) {
    auto symbol_from_translated_node = [](SgNode *translated) -> SgSymbol * {
      if (SgVariableDeclaration *variable_decl =
              isSgVariableDeclaration(translated)) {
        if (variable_decl->get_variables().size() != 1 ||
            variable_decl->get_variables().front() == nullptr) {
          std::cerr << "REX_CFE_ACC_INVARIANT[semantic-materialization]: "
                       "translated variable does not own one name\n";
          ROSE_ABORT();
        }
        SgInitializedName *name = variable_decl->get_variables().front();
        if (SgSymbol *symbol = name->get_symbol_from_symbol_table()) {
          return symbol;
        }
        return name->get_scope() != nullptr
                   ? name->get_scope()->find_symbol_from_declaration(name)
                   : nullptr;
      }
      if (SgInitializedName *name = isSgInitializedName(translated)) {
        if (SgSymbol *symbol = name->get_symbol_from_symbol_table()) {
          return symbol;
        }
        return name->get_scope() != nullptr
                   ? name->get_scope()->find_symbol_from_declaration(name)
                   : nullptr;
      }
      if (SgDeclarationStatement *declaration =
              isSgDeclarationStatement(translated)) {
        if (SgSymbol *symbol = declaration->get_symbol_from_symbol_table()) {
          return symbol;
        }
        if (SgDeclarationStatement *first_nondefining =
                declaration->get_firstNondefiningDeclaration()) {
          if (SgSymbol *symbol =
                  first_nondefining->get_symbol_from_symbol_table()) {
            return symbol;
          }
        }
        return declaration->get_scope() != nullptr
                   ? declaration->get_scope()->find_symbol_from_declaration(
                         declaration)
                   : nullptr;
      }
      return nullptr;
    };

    auto translate_exact_declaration = [&](clang::NamedDecl *clang_decl,
                                           SgNode **translated,
                                           SgSymbol **symbol) {
      ROSE_ASSERT(clang_decl != nullptr);
      ROSE_ASSERT(translated != nullptr);
      ROSE_ASSERT(symbol != nullptr);
      auto found = p_decl_translation_map.find(clang_decl);
      *translated = found != p_decl_translation_map.end()
                        ? found->second
                        : TraverseOnDemand(clang_decl);
      *symbol = symbol_from_translated_node(*translated);
      if (*translated == nullptr || *symbol == nullptr) {
        clang::NamedDecl *canonical =
            llvm::dyn_cast<clang::NamedDecl>(clang_decl->getCanonicalDecl());
        if (canonical != nullptr && canonical != clang_decl) {
          auto canonical_found = p_decl_translation_map.find(canonical);
          *translated = canonical_found != p_decl_translation_map.end()
                            ? canonical_found->second
                            : TraverseOnDemand(canonical);
          *symbol = symbol_from_translated_node(*translated);
        }
      }
    };

    OpenACCCxxExactSemanticBindings::BindingSequence sage_bindings;
    sage_bindings.reserve(clang_bindings->size());
    for (const auto &expression : *clang_bindings) {
      std::vector<OmpExactSubexpressionType> sage_subexpressions;
      sage_subexpressions.reserve(expression.subexpressions.size());
      for (const auto &subexpression : expression.subexpressions) {
        if (subexpression.kind == OmpExactSubexpressionKind::invalid ||
            subexpression.result_type.isNull()) {
          std::cerr << "REX_CFE_ACC_INVARIANT[semantic-expression-type]: "
                       "producer published an invalid subexpression type\n";
          ROSE_ABORT();
        }
        SgType *result_type =
            buildTypeFromQualifiedType(subexpression.result_type);
        if (SageInterface::containsUnknownType(result_type)) {
          std::cerr << "REX_CFE_ACC_INVARIANT[semantic-expression-type]: "
                       "Clang subexpression has no exact Sage result type\n";
          ROSE_ABORT();
        }
        sage_subexpressions.emplace_back(subexpression.kind, result_type);
      }
      std::vector<OpenACCCxxExactSemanticBindings::Binding> sage_identifiers;
      sage_identifiers.reserve(expression.identifiers.size());
      for (const auto &binding : expression.identifiers) {
        if (binding.spelling.empty()) {
          std::cerr << "REX_CFE_ACC_INVARIANT[semantic-materialization]: "
                       "captured binding has no spelling\n";
          ROSE_ABORT();
        }
        clang::NamedDecl *clang_decl = binding.declaration;
        if (clang_decl == nullptr) {
          if (!binding.isCurrentThis()) {
            std::cerr << "REX_CFE_ACC_INVARIANT[semantic-materialization]: "
                         "OpenACC binding '"
                      << binding.spelling << "' has no exact Clang identity\n";
            ROSE_ABORT();
          }
          sage_identifiers.emplace_back(
              binding.spelling,
              OpenACCCxxExactSemanticBindings::BindingKind::current_this,
              nullptr, nullptr);
          continue;
        }

        const bool is_value =
            llvm::isa<clang::ValueDecl>(clang_decl) ||
            llvm::isa<clang::FunctionTemplateDecl>(clang_decl);
        const bool captured_as_value = binding.isValue();
        const bool captured_as_qualifier = binding.isQualifier();
        if ((is_value && !captured_as_value) ||
            (!is_value && !captured_as_qualifier)) {
          std::cerr << "REX_CFE_ACC_INVARIANT[semantic-materialization]: "
                       "captured binding kind changed before Sage "
                       "materialization\n";
          ROSE_ABORT();
        }
        const OpenACCCxxExactSemanticBindings::BindingKind binding_kind =
            is_value ? OpenACCCxxExactSemanticBindings::BindingKind::value
                     : OpenACCCxxExactSemanticBindings::BindingKind::qualifier;
        SgNode *semantic_node = nullptr;
        SgSymbol *symbol = nullptr;
        translate_exact_declaration(clang_decl, &semantic_node, &symbol);
        if (semantic_node == nullptr) {
          std::cerr << "REX_CFE_ACC_INVARIANT[semantic-materialization]: exact "
                       "Clang declaration '"
                    << binding.spelling << "' has no Sage declaration\n";
          ROSE_ABORT();
        }
        if (is_value &&
            (symbol == nullptr ||
             (isSgVariableSymbol(symbol) == nullptr &&
              isSgEnumFieldSymbol(symbol) == nullptr &&
              isSgFunctionSymbol(symbol) == nullptr &&
              isSgTemplateFunctionSymbol(symbol) == nullptr &&
              isSgMemberFunctionSymbol(symbol) == nullptr &&
              isSgTemplateMemberFunctionSymbol(symbol) == nullptr) ||
             symbol->get_name().getString() != binding.spelling)) {
          std::cerr << "REX_CFE_ACC_INVARIANT[semantic-materialization]: exact "
                       "Clang value '"
                    << binding.spelling
                    << "' has no matching Sage value/function symbol\n";
          ROSE_ABORT();
        }
        if (symbol != nullptr) {
          semantic_node = symbol->get_symbol_basis();
        }
        sage_identifiers.emplace_back(binding.spelling, binding_kind,
                                      semantic_node, symbol);
      }
      sage_bindings.emplace_back(expression.parse_mode, expression.expression,
                                 std::move(sage_identifiers),
                                 std::move(sage_subexpressions));
    }
    OmpSupport::registerOpenACCCxxExactSemanticBindings(
        p_sage_source_file, pragma_decl,
        OpenACCCxxExactSemanticBindings(std::move(sage_bindings)));
  }

  return pragma_decl;
}

void ClangToSageTranslator::appendPragmasBefore(clang::Stmt *stmt,
                                                SgScopeStatement *scope) {
  if (scope == nullptr) {
    return;
  }

  std::vector<CapturedPragma> pragmas;
  collectPragmas(stmt, pragmas);
  collectOpenMPAttributePragmas(stmt, pragmas);
  if (pragmas.empty()) {
    return;
  }

  for (const auto &entry : pragmas) {
    SgPragmaDeclaration *pragma_decl = buildCapturedPragmaDeclaration(
        entry.text, entry.source_text, entry.physical_file_id,
        entry.physical_line, entry.filename, entry.line, entry.column,
        entry.end_line, entry.end_column, scope,
        entry.has_top_level_macro_expansion, false, entry.attribute_range);
    if (pragma_decl == nullptr) {
      std::cerr << "REX_CFE_PRAGMA_INVARIANT[construction]: captured pragma "
                   "materialization returned null\n";
      ROSE_ABORT();
    }

    if (SgBasicBlock *block = isSgBasicBlock(scope)) {
      block->append_statement(pragma_decl);
    } else {
      SageInterface::appendStatement(pragma_decl, scope);
    }
  }
}

SgStatement *
ClangToSageTranslator::wrapStatementWithPragmas(clang::Stmt *stmt,
                                                SgStatement *statement) {
  if (statement == nullptr) {
    return nullptr;
  }

  std::vector<CapturedPragma> pragmas;
  collectPragmas(stmt, pragmas);
  collectOpenMPAttributePragmas(stmt, pragmas);
  if (pragmas.empty()) {
    return statement;
  }

  const bool has_structured_directive = std::any_of(
      pragmas.begin(), pragmas.end(), [](const CapturedPragma &pragma) {
        return pragma.is_structured_directive || pragma.is_rose_semantic;
      });
  if (!has_structured_directive) {
    attachCapturedPragmas(statement, pragmas, PreprocessingInfo::before,
                          "attachment");
    return statement;
  }

  SgBasicBlock *wrapper_block = SageBuilder::buildBasicBlock_nfi();
  if (stmt != nullptr) {
    applySourceRangeWithTrailingSemicolon(wrapper_block, stmt);
  } else {
    setSynthesizedFileInfo(wrapper_block);
  }

  for (const auto &entry : pragmas) {
    SgPragmaDeclaration *pragma_decl = buildCapturedPragmaDeclaration(
        entry.text, entry.source_text, entry.physical_file_id,
        entry.physical_line, entry.filename, entry.line, entry.column,
        entry.end_line, entry.end_column, wrapper_block,
        entry.has_top_level_macro_expansion, false, entry.attribute_range);
    if (pragma_decl == nullptr) {
      std::cerr << "REX_CFE_PRAGMA_INVARIANT[construction]: captured pragma "
                   "materialization returned null\n";
      ROSE_ABORT();
    }
    wrapper_block->append_statement(pragma_decl);
  }

  wrapper_block->append_statement(statement);
  return wrapper_block;
}

SgStatement *ClangToSageTranslator::translateStatement(
    clang::Stmt *stmt, const char *context, bool expression_owns_terminator) {
  SgNode *translated = Traverse(stmt);
  if (llvm::isa<clang::Expr>(stmt)) {
    auto *expression = isSgExpression(translated);
    if (expression == nullptr || expression->get_parent() != nullptr) {
      std::cerr << "REX_FRONTEND_INVARIANT[statement-expression]: " << context
                << " requires one unowned translated expression" << std::endl;
      ROSE_ABORT();
    }
    auto *statement = SageBuilder::buildExprStatement_nfi(expression);
    if (expression_owns_terminator)
      applySourceRangeWithTrailingSemicolon(statement, stmt);
    else
      applySourceRange(statement, stmt->getSourceRange());
    return statement;
  }
  auto *statement = isSgStatement(translated);
  if (statement == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[statement-translation]: " << context
              << " Clang " << stmt->getStmtClassName() << " translated to "
              << (translated != nullptr ? translated->class_name() : "<null>")
              << " instead of SgStatement" << std::endl;
    ROSE_ABORT();
  }
  return statement;
}

SgStatement *ClangToSageTranslator::translateControlledSubstatement(
    clang::Stmt *stmt, SgScopeStatement *typed_owner, const char *context) {

  if (stmt == nullptr || typed_owner == nullptr || context == nullptr ||
      SageBuilder::topScopeStack() != typed_owner) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[controlled-substatement-scope]: "
            "context=%s Clang statement=%p typed-owner=%p active-scope=%p is "
            "not one exact construction transaction\n",
            context != nullptr ? context : "<null>", static_cast<void *>(stmt),
            static_cast<void *>(typed_owner),
            static_cast<void *>(SageBuilder::topScopeStack()));
    ROSE_ABORT();
  }

  clang::Stmt *scope_requiring_statement = stmt;
  while (clang::AttributedStmt *attributed =
             llvm::dyn_cast<clang::AttributedStmt>(scope_requiring_statement)) {
    scope_requiring_statement = attributed->getSubStmt();
    if (scope_requiring_statement == nullptr) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[controlled-substatement-scope]: "
              "context=%s attributed statement has no exact child\n",
              context);
      ROSE_ABORT();
    }
  }
  if (!llvm::isa<clang::DeclStmt>(scope_requiring_statement)) {
    return translateStatement(stmt, context);
  }

  SgBasicBlock *implicit_scope = SageBuilder::buildBasicBlock_nfi();
  if (implicit_scope == nullptr || implicit_scope->get_parent() != nullptr ||
      !implicit_scope->get_statements().empty()) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[controlled-substatement-scope]: "
            "context=%s could not construct one empty implicit block scope\n",
            context);
    ROSE_ABORT();
  }
  implicit_scope->set_is_implicit_control_flow_scope(true);
  setSynthesizedFileInfo(implicit_scope);
  mark_compiler_generated_frontend_specific(implicit_scope);
  implicit_scope->set_parent(typed_owner);

  SageBuilder::pushScopeStack(implicit_scope);
  SgNode *translated = Traverse(stmt);
  SageBuilder::popScopeStack();

  SgStatement *statement = isSgStatement(translated);
  const size_t occurrences =
      statement != nullptr
          ? static_cast<size_t>(
                std::count(implicit_scope->get_statements().begin(),
                           implicit_scope->get_statements().end(), statement))
          : 0;
  if (statement == nullptr || statement == implicit_scope ||
      (statement->get_parent() == implicit_scope
           ? occurrences != 1
           : statement->get_parent() != nullptr || occurrences != 0)) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[controlled-substatement-scope]: "
            "context=%s translated child=%p/%s does not have one exact "
            "implicit-block ownership state\n",
            context, static_cast<void *>(statement),
            statement != nullptr ? statement->class_name().c_str() : "<null>");
    ROSE_ABORT();
  }
  if (statement->get_parent() == nullptr) {
    implicit_scope->append_statement(statement);
  }
  if (statement->get_parent() != implicit_scope ||
      std::count(implicit_scope->get_statements().begin(),
                 implicit_scope->get_statements().end(), statement) != 1) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[controlled-substatement-scope]: "
            "context=%s implicit block did not acquire its exact statement\n",
            context);
    ROSE_ABORT();
  }
  return implicit_scope;
}

SgStatement *ClangToSageTranslator::translateTypedStatementChild(
    clang::Stmt *stmt, SgStatement *typed_owner, const char *context) {

  if (stmt == nullptr || typed_owner == nullptr || context == nullptr) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[typed-statement-child]: context=%s "
            "Clang statement=%p typed-owner=%p is not one exact "
            "construction transaction\n",
            context != nullptr ? context : "<null>", static_cast<void *>(stmt),
            static_cast<void *>(typed_owner));
    ROSE_ABORT();
  }

  if (activeTypedStatementChildConstructionFor(stmt) != nullptr) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[typed-statement-child]: context=%s "
            "statement=%p/%s already has an active typed owner\n",
            context, static_cast<void *>(stmt), stmt->getStmtClassName());
    ROSE_ABORT();
  }

  p_typed_statement_child_constructions.push_back({stmt, typed_owner, context});
  struct TypedStatementChildConstructionGuard {
    std::vector<TypedStatementChildConstruction> &frames;
    const clang::Stmt *statement;
    SgStatement *owner;
    ~TypedStatementChildConstructionGuard() {
      if (frames.empty() || frames.back().statement != statement ||
          frames.back().owner != owner) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[typed-statement-child]: statement=%p "
                "owner=%p lost its exact construction frame\n",
                static_cast<const void *>(statement),
                static_cast<void *>(owner));
        ROSE_ABORT();
      }
      frames.pop_back();
    }
  } guard{p_typed_statement_child_constructions, stmt, typed_owner};
  SgStatement *translated = translateStatement(stmt, context);
  SgDeclarationStatement *declaration = isSgDeclarationStatement(translated);
  SgAuxiliaryDeclarationList *auxiliary =
      declaration != nullptr
          ? isSgAuxiliaryDeclarationList(declaration->get_parent())
          : nullptr;
  if (auxiliary != nullptr) {
    SgScopeStatement *lexical_scope = SageBuilder::topScopeStack();
    SgScopeStatement *auxiliary_owner =
        isSgScopeStatement(auxiliary->get_parent());
    auto already_owned_by = [declaration](SgNode *owner) {
      const auto children = owner->returnDataMemberPointers();
      return std::any_of(children.begin(), children.end(),
                         [declaration](const auto &child) {
                           return child.first == declaration;
                         });
    };
    if (!llvm::isa<clang::DeclStmt>(stmt) || lexical_scope == nullptr ||
        auxiliary_owner != lexical_scope ||
        declaration->get_scope() != lexical_scope ||
        lexical_scope->get_auxiliary_declarations() != auxiliary ||
        std::count(auxiliary->get_declarations().begin(),
                   auxiliary->get_declarations().end(), declaration) != 1 ||
        already_owned_by(lexical_scope) || already_owned_by(typed_owner)) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[typed-declaration-child-owner]: "
              "context=%s declaration=%p/%s auxiliary=%p owner=%p "
              "lexical-scope=%p lacks one exact pending typed-child "
              "transaction\n",
              context, static_cast<void *>(declaration),
              declaration->class_name().c_str(), static_cast<void *>(auxiliary),
              static_cast<void *>(auxiliary_owner),
              static_cast<void *>(lexical_scope));
      ROSE_ABORT();
    }
    if (!SageBuilder::detachAuxiliaryDeclaration(lexical_scope, declaration) ||
        declaration->get_parent() != nullptr) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[typed-declaration-child-owner]: "
              "context=%s declaration=%p/%s could not detach from its exact "
              "semantic owner before typed adoption\n",
              context, static_cast<void *>(declaration),
              declaration->class_name().c_str());
      ROSE_ABORT();
    }
    declaration->set_parent(typed_owner);
  }
  return translated;
}

SgNode *
ClangToSageTranslator::statementConstructionOwner(const clang::Stmt *statement,
                                                  const char *context) const {
  if (statement == nullptr || context == nullptr) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[statement-construction-owner]: "
            "context=%s statement=%p is incomplete\n",
            context != nullptr ? context : "<null>",
            static_cast<const void *>(statement));
    ROSE_ABORT();
  }
  if (const TypedStatementChildConstruction *typed_child =
          activeTypedStatementChildConstructionFor(statement)) {
    if (typed_child->owner == nullptr || typed_child->context == nullptr) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[statement-construction-owner]: "
              "context=%s statement=%p has an incomplete typed owner\n",
              context, static_cast<const void *>(statement));
      ROSE_ABORT();
    }
    return typed_child->owner;
  }
  SgScopeStatement *lexical_scope = SageBuilder::topScopeStack();
  if (lexical_scope == nullptr) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[statement-construction-owner]: "
            "context=%s statement=%p has no exact lexical scope\n",
            context, static_cast<const void *>(statement));
    ROSE_ABORT();
  }
  return lexical_scope;
}

bool ClangToSageTranslator::VisitCompoundStmt(
    clang::CompoundStmt *compound_stmt, SgNode **node) {
  SgScopeStatement *construction_scope = SageBuilder::topScopeStack();
  SgFunctionDefinition *function_definition =
      isSgFunctionDefinition(construction_scope);
  const TypedStatementChildConstruction *typed_child =
      activeTypedStatementChildConstructionFor(compound_stmt);
  SgTryStmt *try_owner =
      typed_child != nullptr ? isSgTryStmt(typed_child->owner) : nullptr;
  SgBasicBlock *block = nullptr;
  if (try_owner != nullptr) {
    block = isSgBasicBlock(try_owner->get_body());
    if (block == nullptr || block->get_parent() != try_owner ||
        !block->get_statements().empty()) {
      std::cerr << "REX_FRONTEND_INVARIANT[try-body]: try statement has no "
                   "empty, exactly owned compound body"
                << std::endl;
      ROSE_ABORT();
    }
  }
  if (block == nullptr && function_definition != nullptr &&
      !p_function_body_translation_owner_stack.empty()) {
    const clang::FunctionDecl *body_owner =
        p_function_body_translation_owner_stack.back().clang_declaration;
    auto pending = p_pending_function_body_scopes.find(body_owner);
    if (pending != p_pending_function_body_scopes.end()) {
      block = pending->second;
      if (block == nullptr || function_definition->get_body() != block ||
          block->get_parent() != function_definition ||
          block->get_function_body_construction_scope() != nullptr ||
          !block->get_statements().empty()) {
        std::cerr
            << "REX_FRONTEND_INVARIANT[function-body-preconstruction]: "
               "compound statement did not receive its exact adopted pending "
               "body scope"
            << std::endl;
        ROSE_ABORT();
      }
      p_pending_function_body_scopes.erase(pending);
    }
  }
  if (block == nullptr && function_definition != nullptr) {
    block = function_definition->get_body();
    if (block == nullptr || block->get_parent() != function_definition ||
        block->get_function_body_construction_scope() != nullptr ||
        !block->get_statements().empty() ||
        block->get_auxiliary_declarations() != nullptr) {
      std::cerr << "REX_FRONTEND_INVARIANT[function-body-construction]: "
                   "function definition does not own one empty, "
                   "declaration-free body scope"
                << std::endl;
      ROSE_ABORT();
    }
  }
  if (block == nullptr) {
    // The Clang producer assigns either lexical or compiler-generated semantic
    // provenance below.  A transformation-classified builder would publish a
    // false mutation role before that exact source classification is known.
    block = SageBuilder::buildBasicBlock_nfi();
  }
  const bool statement_expression_body =
      !p_statement_expression_body_construction_stack.empty() &&
      p_statement_expression_body_construction_stack.back().clang_body ==
          compound_stmt;
  if (statement_expression_body) {
    StatementExpressionBodyConstruction &transaction =
        p_statement_expression_body_construction_stack.back();
    if (construction_scope == nullptr ||
        transaction.semantic_scope != construction_scope ||
        transaction.sage_body != nullptr || block->get_parent() != nullptr ||
        block->get_statement_expression_construction_scope() != nullptr ||
        block->get_statement_expression_semantic_scope() != nullptr) {
      std::cerr << "REX_FRONTEND_INVARIANT[statement-expression-scope]: "
                   "GNU statement-expression body does not have one exact "
                   "detached semantic-scope construction transaction"
                << std::endl;
      ROSE_ABORT();
    }
    block->begin_statement_expression_construction(construction_scope);
    transaction.sage_body = block;
    if (block->get_scope() != construction_scope ||
        block->get_statement_expression_semantic_scope() != nullptr) {
      std::cerr << "REX_FRONTEND_INVARIANT[statement-expression-scope]: "
                   "GNU statement-expression body did not publish its exact "
                   "construction-time semantic scope"
                << std::endl;
      ROSE_ABORT();
    }
  }
  if (function_definition == nullptr && !statement_expression_body &&
      try_owner == nullptr) {
    block->set_parent(statementConstructionOwner(
        compound_stmt, "compound statement structural owner"));
  }

  // Publish the compound statement's source identity before translating any
  // child. Child expressions can construct auxiliary semantic scopes whose
  // exact physical output owner is this block; deferring VisitStmt until after
  // the body walk leaves a source-written block with builder-default file
  // information during that construction transaction.
  *node = block;
  const clang::SourceRange compound_source_range =
      compound_stmt->getSourceRange();
  const bool has_lexical_compound_range =
      compound_source_range.isValid() &&
      compound_source_range.getBegin().isValid() &&
      compound_source_range.getEnd().isValid();
  if (!has_lexical_compound_range) {
    SgFunctionDeclaration *function_declaration =
        function_definition != nullptr ? function_definition->get_declaration()
                                       : nullptr;
    SgAuxiliaryDeclarationList *auxiliary_owner =
        function_declaration != nullptr
            ? isSgAuxiliaryDeclarationList(function_declaration->get_parent())
            : nullptr;
    const bool exact_semantic_owner =
        auxiliary_owner != nullptr &&
        auxiliary_owner->get_parent() == function_declaration->get_scope() &&
        std::count(auxiliary_owner->get_declarations().begin(),
                   auxiliary_owner->get_declarations().end(),
                   function_declaration) == 1;
    auto exact_semantic_file_info = [](Sg_File_Info *file_info) {
      return file_info != nullptr && file_info->isCompilerGenerated() &&
             file_info->isFrontendSpecific() && !file_info->isTransformation();
    };
    const bool exact_semantic_function =
        function_declaration != nullptr && exact_semantic_owner &&
        function_definition->get_parent() == function_declaration &&
        exact_semantic_file_info(function_declaration->get_file_info()) &&
        exact_semantic_file_info(
            function_declaration->get_startOfConstruct()) &&
        exact_semantic_file_info(function_declaration->get_endOfConstruct());
    if (!exact_semantic_function) {
      std::cerr << "REX_FRONTEND_INVARIANT[compound-statement-source-role]: "
                   "source-less CompoundStmt is not the exact body of one "
                   "semantic-only frontend function; definition="
                << static_cast<void *>(function_definition)
                << " declaration=" << static_cast<void *>(function_declaration)
                << " auxiliary=" << static_cast<void *>(auxiliary_owner)
                << std::endl;
      ROSE_ABORT();
    }
    setSynthesizedFileInfo(block);
    mark_compiler_generated_frontend_specific(block);
  }
  if (!VisitStmt(compound_stmt, node) || *node != block) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[compound-statement-provenance]: Clang "
            "compound statement did not publish its exact Sage block source "
            "identity before child translation\n");
    ROSE_ABORT();
  }
  SageBuilder::pushScopeStack(block);

  // A directive between a control header (including a function declarator)
  // and its opening brace belongs to the controlled compound statement's
  // leading preprocessing surface.  It must be consumed here: appending it to
  // the enclosing declaration scope later gives it a false structural owner
  // and can reverse its order with the containing declaration.
  //
  // Generic pragmas remain preprocessing surfaces.  A structured OpenMP
  // directive cannot legally occupy the gap between a function declarator and
  // its body, so reject that malformed input instead of moving the directive
  // into the body and changing its meaning.
  std::vector<CapturedPragma> block_pragmas;
  const bool is_function_body =
      isSgFunctionDefinition(block->get_parent()) != nullptr;
  if (is_function_body && collectPragmas(compound_stmt, block_pragmas)) {
    for (const CapturedPragma &pragma : block_pragmas) {
      if (pragma.is_structured_directive) {
        std::cerr << "REX_CFE_PRAGMA_INVARIANT[function-body-boundary]: "
                     "structured OpenMP directive cannot appear between a "
                     "function declarator and its body"
                  << std::endl;
        ROSE_ABORT();
      }
    }
    attachCapturedPragmas(block, block_pragmas, PreprocessingInfo::before,
                          "block-attachment");
  }

  std::unordered_set<const clang::Stmt *> seen_clang_stmts;
  std::unordered_set<SgStatement *> seen_sg_stmts;

  auto prepare_auxiliary_declaration_for_statement_adoption =
      [&](clang::Stmt *source_statement, SgStatement *statement) {
        SgDeclarationStatement *declaration =
            isSgDeclarationStatement(statement);
        SgAuxiliaryDeclarationList *container =
            declaration != nullptr
                ? isSgAuxiliaryDeclarationList(declaration->get_parent())
                : nullptr;
        if (container == nullptr) {
          return;
        }

        SgScopeStatement *semantic_scope = declaration->get_scope();
        SgScopeStatement *auxiliary_owner =
            isSgScopeStatement(container->get_parent());
        const SgDeclarationStatementPtrList &auxiliary_declarations =
            container->get_declarations();
        if (!llvm::isa<clang::DeclStmt>(source_statement) ||
            auxiliary_owner != block || semantic_scope != auxiliary_owner ||
            auxiliary_owner->get_auxiliary_declarations() != container ||
            std::count(auxiliary_declarations.begin(),
                       auxiliary_declarations.end(), declaration) != 1 ||
            std::count(block->get_statements().begin(),
                       block->get_statements().end(), declaration) != 0) {
          std::cerr
              << "REX_FRONTEND_INVARIANT[compound-declaration-adoption]: "
                 "declaration="
              << declaration << " type=" << declaration->class_name()
              << " name=" << SageInterface::get_name(declaration)
              << " source-statement=" << source_statement->getStmtClassName()
              << " source-location="
              << source_statement->getBeginLoc().printToString(
                     p_compiler_instance->getSourceManager())
              << " auxiliary-owner=" << auxiliary_owner
              << " semantic-scope=" << semantic_scope
              << " active-block=" << block
              << " does not have one exact auxiliary owner before statement "
                 "adoption"
              << std::endl;
          if (clang::DeclStmt *decl_statement =
                  llvm::dyn_cast<clang::DeclStmt>(source_statement)) {
            for (clang::Decl *clang_declaration :
                 decl_statement->getDeclGroup()) {
              auto mapped = p_decl_translation_map.find(clang_declaration);
              SgScopeStatement *active_function =
                  activeFunctionBodyOwnerForDeclaration(clang_declaration);
              std::cerr << "  Clang declaration=" << clang_declaration << "/"
                        << clang_declaration->getDeclKindName() << " mapped="
                        << (mapped != p_decl_translation_map.end()
                                ? mapped->second
                                : nullptr)
                        << " active-function=" << active_function << std::endl;
            }
          }
          std::cerr << "  top-scope=" << SageBuilder::topScopeStack()
                    << " rebuild-depth=" << p_rebuild_translation_cache_depth
                    << " function-body-frames="
                    << p_function_body_translation_owner_stack.size()
                    << std::endl;
          for (const FunctionBodyTranslationOwner &frame :
               p_function_body_translation_owner_stack) {
            std::cerr
                << "  function-body-frame clang=" << frame.clang_declaration
                << "/"
                << (frame.clang_declaration != nullptr
                        ? frame.clang_declaration->getQualifiedNameAsString()
                        : std::string("<null>"))
                << " sage=" << frame.sage_owner << std::endl;
          }
          std::cerr << "  auxiliary-function="
                    << SageInterface::getEnclosingFunctionDefinition(
                           auxiliary_owner, true)
                    << " active-block-function="
                    << SageInterface::getEnclosingFunctionDefinition(block,
                                                                     true)
                    << std::endl;
          ROSE_ABORT();
        }

        if (!SageBuilder::detachAuxiliaryDeclaration(auxiliary_owner,
                                                     declaration) ||
            declaration->get_parent() != nullptr) {
          std::cerr << "REX_FRONTEND_INVARIANT[compound-declaration-adoption]: "
                       "declaration="
                    << declaration << " type=" << declaration->class_name()
                    << " was not detached from its exact auxiliary owner"
                    << std::endl;
          ROSE_ABORT();
        }
        SgAuxiliaryDeclarationList *remaining_container =
            auxiliary_owner->get_auxiliary_declarations();
        if (remaining_container != nullptr &&
            std::find(remaining_container->get_declarations().begin(),
                      remaining_container->get_declarations().end(),
                      declaration) !=
                remaining_container->get_declarations().end()) {
          std::cerr << "REX_FRONTEND_INVARIANT[compound-declaration-adoption]: "
                       "declaration="
                    << declaration << " type=" << declaration->class_name()
                    << " retained an auxiliary ownership edge after detachment"
                    << std::endl;
          ROSE_ABORT();
        }
      };

  clang::CompoundStmt::body_iterator it;
  for (it = compound_stmt->body_begin(); it != compound_stmt->body_end();
       it++) {
    clang::Stmt *child_stmt = *it;

    if (child_stmt == nullptr) {
      std::cerr << "REX_FRONTEND_INVARIANT[compound-statement-child]: Clang "
                   "CompoundStmt contains a null child"
                << std::endl;
      ROSE_ABORT();
    }
    if (!seen_clang_stmts.insert(child_stmt).second) {
      std::cerr << "REX_FRONTEND_INVARIANT[compound-statement-child]: "
                   "duplicate Clang child "
                << child_stmt->getStmtClassName() << "@" << child_stmt
                << std::endl;
      ROSE_ABORT();
    }

    appendPragmasBefore(child_stmt, block);

    SgStatement *stmt =
        translateStatement(child_stmt, "compound statement child");

    SgClassDeclaration *class_decl = isSgClassDeclaration(stmt);
    if (class_decl != nullptr &&
        (class_decl->get_name() == "" || class_decl->get_isUnNamed())) {
      Sg_File_Info *class_fi = class_decl->get_file_info();
      if (!class_decl->get_isAutonomousDeclaration() || class_fi == nullptr ||
          class_fi->isCompilerGenerated() ||
          !class_fi->isOutputInCodeGeneration()) {
        std::cerr << "REX_FRONTEND_INVARIANT[compound-anonymous-tag-owner]: "
                     "anonymous class returned as a direct CompoundStmt child "
                     "without one autonomous source-emission contract"
                  << std::endl;
        ROSE_ABORT();
      }
    }
    SgEnumDeclaration *enum_decl = isSgEnumDeclaration(stmt);
    if (enum_decl != nullptr &&
        (enum_decl->get_name() == "" || enum_decl->get_isUnNamed())) {
      Sg_File_Info *enum_fi = enum_decl->get_file_info();
      if (!enum_decl->get_isAutonomousDeclaration() || enum_fi == nullptr ||
          enum_fi->isCompilerGenerated() ||
          !enum_fi->isOutputInCodeGeneration()) {
        std::cerr << "REX_FRONTEND_INVARIANT[compound-anonymous-tag-owner]: "
                     "anonymous enum returned as a direct CompoundStmt child "
                     "without one autonomous source-emission contract"
                  << std::endl;
        ROSE_ABORT();
      }
    }

    prepare_auxiliary_declaration_for_statement_adoption(child_stmt, stmt);
    const size_t block_occurrences = std::count(
        block->get_statements().begin(), block->get_statements().end(), stmt);
    if (stmt->get_parent() == block) {
      if (block_occurrences > 1) {
        std::cerr << "REX_FRONTEND_INVARIANT[compound-statement-owner]: "
                     "statement owned by the active basic block occurs "
                  << block_occurrences << " times in its statement list"
                  << std::endl;
        ROSE_ABORT();
      }
      if (!seen_sg_stmts.insert(stmt).second) {
        std::cerr << "REX_FRONTEND_INVARIANT[compound-statement-owner]: "
                     "translated statement was returned more than once"
                  << std::endl;
        ROSE_ABORT();
      }
      if (block_occurrences == 1) {
        continue;
      }
      block->append_statement(stmt);
      continue;
    }
    if (block_occurrences != 0) {
      std::cerr
          << "REX_FRONTEND_INVARIANT[compound-statement-owner]: statement "
             "is present in the active basic block without exact ownership"
          << std::endl;
      ROSE_ABORT();
    }
    if (!seen_sg_stmts.insert(stmt).second) {
      std::cerr << "VisitCompoundStmt: duplicate sg stmt " << stmt->class_name()
                << "@" << stmt << std::endl;
      ROSE_ABORT();
    }
    block->append_statement(stmt);
    SgAuxiliaryDeclarationList *remaining_container =
        block->get_auxiliary_declarations();
    const bool retained_auxiliary_edge =
        remaining_container != nullptr &&
        std::find(remaining_container->get_declarations().begin(),
                  remaining_container->get_declarations().end(),
                  stmt) != remaining_container->get_declarations().end();
    if (stmt->get_parent() != block ||
        std::count(block->get_statements().begin(),
                   block->get_statements().end(), stmt) != 1 ||
        retained_auxiliary_edge) {
      std::cerr << "REX_FRONTEND_INVARIANT[compound-declaration-adoption]: "
                   "statement="
                << stmt << " type=" << stmt->class_name()
                << " did not acquire one exact basic-block owner" << std::endl;
      ROSE_ABORT();
    }
  }

  if (p_lexical_source_nodes.count(block) != 0) {
    constructLexicalPragmas(compound_stmt, block);
  }
  SageBuilder::popScopeStack();

  if (statement_expression_body) {
    if (p_statement_expression_body_construction_stack.empty()) {
      std::cerr << "REX_FRONTEND_INVARIANT[statement-expression-body-owner]: "
                   "GNU statement-expression construction stack was emptied "
                   "before its body completed"
                << std::endl;
      ROSE_ABORT();
    }
    const StatementExpressionBodyConstruction &transaction =
        p_statement_expression_body_construction_stack.back();
    if (transaction.clang_body != compound_stmt ||
        transaction.sage_body != block || block->get_parent() != nullptr ||
        block->get_statement_expression_construction_scope() !=
            transaction.semantic_scope ||
        block->get_statement_expression_semantic_scope() != nullptr ||
        block->get_scope() != transaction.semantic_scope) {
      std::cerr << "REX_FRONTEND_INVARIANT[statement-expression-body-owner]: "
                   "exact GNU statement-expression body lost its detached "
                   "semantic-scope construction contract"
                << std::endl;
      ROSE_ABORT();
    }
  }

  return true;
}

bool ClangToSageTranslator::VisitContinueStmt(
    clang::ContinueStmt *continue_stmt, SgNode **node) {
  *node = SageBuilder::buildContinueStmt_nfi();
  return VisitStmt(continue_stmt, node);
}

bool ClangToSageTranslator::VisitCoreturnStmt(
    clang::CoreturnStmt *core_turn_stmt, SgNode **node) {
  SgExpression *return_expr = nullptr;
  if (clang::Expr *operand = core_turn_stmt->getOperand()) {
    SgNode *tmp_expr = Traverse(operand);
    return_expr = isSgExpression(tmp_expr);
    if (return_expr == nullptr) {
      std::cerr << "REX_FRONTEND_INVARIANT[coreturn-operand]: Clang "
                   "co_return operand translated to "
                << (tmp_expr == nullptr ? std::string("<null>")
                                        : tmp_expr->class_name())
                << " instead of SgExpression" << std::endl;
      ROSE_ABORT();
    }
  }

  SgReturnStmt *return_stmt = SageBuilder::buildReturnStmt(return_expr);
  applySourceRangeWithTrailingSemicolon(return_stmt, core_turn_stmt);
  return_stmt->set_return_keyword_kind(
      SgReturnStmt::e_return_keyword_co_return);
  *node = return_stmt;

  return VisitStmt(core_turn_stmt, node);
}

bool ClangToSageTranslator::VisitCoroutineBodyStmt(
    clang::CoroutineBodyStmt *coroutine_body_stmt, SgNode **node) {
  clang::CompoundStmt *clang_body = coroutine_body_stmt->getBody();
  if (clang_body == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[coroutine-body]: Clang "
                 "CoroutineBodyStmt has no body"
              << std::endl;
    ROSE_ABORT();
  }
  SgNode *tmp_body = Traverse(clang_body);
  SgBasicBlock *body = isSgBasicBlock(tmp_body);

  if (body == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[coroutine-body]: exact body "
                 "translated to "
              << (tmp_body == nullptr ? std::string("<null>")
                                      : tmp_body->class_name())
              << " instead of SgBasicBlock" << std::endl;
    ROSE_ABORT();
  }

  *node = body;
  const size_t coroutine_body_source_roles =
      p_lexical_source_nodes.count(body) +
      p_synthesized_source_nodes.count(body);
  if (coroutine_body_source_roles != 1) {
    std::cerr << "REX_FRONTEND_INVARIANT[coroutine-body-provenance]: exact "
                 "body did not publish one source/generated role"
              << std::endl;
    ROSE_ABORT();
  }
  return true;
}

bool ClangToSageTranslator::VisitCXXCatchStmt(
    clang::CXXCatchStmt *cxx_catch_stmt, SgNode **node) {
  clang::Stmt *clang_body = cxx_catch_stmt->getHandlerBlock();
  if (clang_body == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[catch-handler-body]: Clang catch "
                 "statement has no handler body"
              << std::endl;
    ROSE_ABORT();
  }
  SgCatchStatementSeq *catch_sequence = isSgCatchStatementSeq(
      statementConstructionOwner(cxx_catch_stmt, "C++ catch handler"));
  SgTryStmt *try_stmt = catch_sequence != nullptr
                            ? isSgTryStmt(catch_sequence->get_parent())
                            : nullptr;
  if (catch_sequence == nullptr || try_stmt == nullptr ||
      try_stmt->get_catch_statement_seq_root() != catch_sequence) {
    std::cerr << "REX_FRONTEND_INVARIANT[catch-construction]: typed catch "
                 "statement has no exact try-owned sequence"
              << std::endl;
    ROSE_ABORT();
  }
  SgCatchOptionStmt *catch_stmt =
      SageBuilder::buildCatchOptionStmt_nfi(nullptr, nullptr);
  ROSE_ASSERT(catch_stmt != nullptr && catch_stmt->get_parent() == nullptr);
  try_stmt->append_catch_statement(catch_stmt);
  catch_stmt->set_trystmt(try_stmt);

  SgScopeStatement *catch_scope = isSgScopeStatement(catch_stmt);
  ROSE_ASSERT(catch_scope != nullptr);
  SageBuilder::pushScopeStack(catch_scope);

  SgVariableDeclaration *condition_decl = nullptr;
  if (clang::VarDecl *exception_decl = cxx_catch_stmt->getExceptionDecl()) {
    SgNode *tmp_condition = Traverse(exception_decl);
    condition_decl = isSgVariableDeclaration(tmp_condition);
    if (condition_decl == nullptr) {
      std::cerr << "REX_FRONTEND_INVARIANT[catch-condition]: Clang exception "
                   "declaration translated to "
                << (tmp_condition == nullptr ? "a null node"
                                             : tmp_condition->class_name())
                << std::endl;
      ROSE_ABORT();
    }
    SgInitializedName *init_name =
        SageInterface::getFirstInitializedName(condition_decl);
    if (condition_decl->get_parent() != nullptr || init_name == nullptr ||
        init_name->get_parent() != condition_decl ||
        init_name->get_scope() != catch_scope ||
        catch_stmt->get_condition() != nullptr) {
      std::cerr
          << "REX_FRONTEND_INVARIANT[catch-condition]: exception declaration="
          << condition_decl << " parent=" << condition_decl->get_parent() << "/"
          << (condition_decl->get_parent() != nullptr
                  ? condition_decl->get_parent()->class_name()
                  : std::string("<null>"))
          << " scope=" << condition_decl->get_scope()
          << " initialized-name=" << init_name << " name-parent="
          << (init_name != nullptr ? init_name->get_parent() : nullptr)
          << " name-scope="
          << (init_name != nullptr ? init_name->get_scope() : nullptr)
          << " catch=" << catch_stmt << " catch-scope=" << catch_scope
          << " existing-condition=" << catch_stmt->get_condition()
          << " clang-exception="
          << (exception_decl->isExceptionVariable() ? 1 : 0)
          << " did not preserve its detached typed owner and exact "
             "construction-time semantic scope"
          << std::endl;
      ROSE_ABORT();
    }
    condition_decl->set_parent(catch_stmt);
    catch_stmt->set_condition(condition_decl);
    if (catch_stmt->get_condition() != condition_decl ||
        condition_decl->get_parent() != catch_stmt ||
        condition_decl->get_scope() != catch_scope ||
        init_name->get_scope() != catch_scope) {
      std::cerr << "REX_FRONTEND_INVARIANT[catch-condition]: typed catch "
                   "condition publication did not establish one exact "
                   "structural and semantic owner"
                << std::endl;
      ROSE_ABORT();
    }
  }

  SgNode *tmp_body = Traverse(clang_body);
  SgStatement *body_stmt = isSgStatement(tmp_body);
  if (body_stmt == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[catch-handler-body]: Clang handler "
                 "body translated to "
              << (tmp_body == nullptr ? std::string("<null>")
                                      : tmp_body->class_name())
              << " instead of SgStatement" << std::endl;
    ROSE_ABORT();
  }

  SageBuilder::popScopeStack();
  ROSE_ASSERT(body_stmt->get_parent() == catch_stmt);
  catch_stmt->set_body(body_stmt);

  *node = catch_stmt;
  return VisitStmt(cxx_catch_stmt, node);
}

bool ClangToSageTranslator::VisitCXXForRangeStmt(
    clang::CXXForRangeStmt *cxx_for_range_stmt, SgNode **node) {
  clang::Stmt *clang_body = cxx_for_range_stmt->getBody();
  if (clang_body == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[range-for-body]: Clang range-for has "
                 "no body"
              << std::endl;
    ROSE_ABORT();
  }
  clang::Stmt *clang_range = cxx_for_range_stmt->getRangeStmt();
  clang::Stmt *clang_loop_var = cxx_for_range_stmt->getLoopVarStmt();
  if (clang_range == nullptr || clang_loop_var == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[range-for-child]: Clang range-for "
                 "has no exact range or loop-variable declaration"
              << std::endl;
    ROSE_ABORT();
  }
  auto translate_for_init_child =
      [&](clang::Stmt *clang_stmt, const char *child_label, bool required,
          bool canonical_generated = false,
          bool semantic_declaration_shell = false,
          SgRangeBasedForStatement *declaration_owner =
              nullptr) -> SgStatement * {
    if (clang_stmt == nullptr) {
      if (required) {
        std::cerr << "REX_FRONTEND_INVARIANT[range-for-child]: required "
                  << child_label << " is absent" << std::endl;
        ROSE_ABORT();
      }
      return nullptr;
    }

    clang::VarDecl *implicit_variable = nullptr;
    if (canonical_generated || semantic_declaration_shell) {
      clang::DeclStmt *declaration_statement =
          llvm::dyn_cast<clang::DeclStmt>(clang_stmt);
      implicit_variable = declaration_statement != nullptr &&
                                  declaration_statement->isSingleDecl()
                              ? llvm::dyn_cast<clang::VarDecl>(
                                    declaration_statement->getSingleDecl())
                              : nullptr;
      if (implicit_variable == nullptr || !implicit_variable->isImplicit()) {
        std::cerr << "REX_FRONTEND_INVARIANT[range-for-semantic-child]: "
                  << child_label
                  << " is not one exact implicit Clang variable declaration"
                  << std::endl;
        ROSE_ABORT();
      }
    }

    bool prev_in_for_init = p_in_for_init_translation;
    p_in_for_init_translation = true;
    if (canonical_generated || semantic_declaration_shell) {
      p_semantic_range_for_declaration_shell_stack.push_back(implicit_variable);
    }
    if (canonical_generated) {
      ++p_semantic_expression_subtree_depth;
    }
    if (canonical_generated) {
      ++p_canonical_generated_literal_depth;
    }
    SgStatement *stmt = declaration_owner != nullptr
                            ? translateTypedStatementChild(
                                  clang_stmt, declaration_owner, child_label)
                            : translateStatement(clang_stmt, child_label);
    if (canonical_generated) {
      if (p_canonical_generated_literal_depth == 0) {
        std::cerr << "REX_FRONTEND_INVARIANT[range-for-semantic-child]: "
                     "canonical literal translation depth underflowed"
                  << std::endl;
        ROSE_ABORT();
      }
      --p_canonical_generated_literal_depth;
    }
    if (canonical_generated) {
      if (p_semantic_expression_subtree_depth == 0) {
        std::cerr << "REX_FRONTEND_INVARIANT[range-for-semantic-child]: "
                  << child_label
                  << " semantic subtree translation depth underflowed"
                  << std::endl;
        ROSE_ABORT();
      }
      --p_semantic_expression_subtree_depth;
    }
    if (canonical_generated || semantic_declaration_shell) {
      if (p_semantic_range_for_declaration_shell_stack.empty() ||
          p_semantic_range_for_declaration_shell_stack.back() !=
              implicit_variable) {
        std::cerr << "REX_FRONTEND_INVARIANT[range-for-semantic-child]: "
                  << child_label
                  << " semantic declaration producer stack was corrupted"
                  << std::endl;
        ROSE_ABORT();
      }
      p_semantic_range_for_declaration_shell_stack.pop_back();
    }
    p_in_for_init_translation = prev_in_for_init;

    return stmt;
  };

  SgBasicBlock *wrapper_block = nullptr;
  SgStatement *init_stmt = nullptr;
  SgNode *outer_owner =
      statementConstructionOwner(cxx_for_range_stmt, "C++ range-for statement");
  clang::Stmt *clang_init = cxx_for_range_stmt->getInit();
  if (clang_init != nullptr) {
    wrapper_block = SageBuilder::buildBasicBlock_nfi();
    if (wrapper_block == nullptr || outer_owner == nullptr ||
        wrapper_block->get_parent() != nullptr) {
      std::cerr << "REX_FRONTEND_INVARIANT[range-for-init-owner]: wrapper "
                   "block was not constructed detached beneath one exact "
                   "active scope"
                << std::endl;
      ROSE_ABORT();
    }
    wrapper_block->set_parent(outer_owner);
    applySourceRange(wrapper_block, cxx_for_range_stmt->getSourceRange());

    SageBuilder::pushScopeStack(wrapper_block);
    init_stmt = translate_for_init_child(clang_init, "init statement", true);
    if (SgDeclarationGroupStatement *init_group =
            isSgDeclarationGroupStatement(init_stmt)) {
      if (init_group->get_parent() != nullptr ||
          init_group->get_scope() != wrapper_block ||
          std::count(wrapper_block->get_statements().begin(),
                     wrapper_block->get_statements().end(), init_group) != 0) {
        std::cerr << "REX_FRONTEND_INVARIANT[range-for-init-owner]: "
                     "declaration group did not complete as one detached "
                     "wrapper-scoped init surface"
                  << std::endl;
        ROSE_ABORT();
      }
    }
    if (SgDeclarationStatement *init_decl =
            isSgDeclarationStatement(init_stmt)) {
      if (init_decl->get_scope() != wrapper_block) {
        std::cerr << "REX_FRONTEND_INVARIANT[range-for-init-owner]: "
                     "declaration init has the wrong wrapper scope"
                  << std::endl;
        ROSE_ABORT();
      }
    }
    if (SgVariableDeclaration *init_decl = isSgVariableDeclaration(init_stmt)) {
      for (SgInitializedName *init_name : init_decl->get_variables()) {
        if (init_name == nullptr || init_name->get_parent() != init_decl ||
            init_name->get_scope() != wrapper_block) {
          std::cerr << "REX_FRONTEND_INVARIANT[range-for-init-owner]: "
                       "variable init has no exact wrapper scope"
                    << std::endl;
          ROSE_ABORT();
        }
      }
    }
    if (init_stmt->get_parent() != nullptr &&
        init_stmt->get_parent() != wrapper_block) {
      std::cerr << "REX_FRONTEND_INVARIANT[range-for-init-owner]: init "
                   "statement has another structural owner"
                << std::endl;
      ROSE_ABORT();
    }
    SgStatementPtrList &stmts = wrapper_block->get_statements();
    if (std::count(stmts.begin(), stmts.end(), init_stmt) == 0) {
      wrapper_block->append_statement(init_stmt);
    }
    if (init_stmt->get_parent() != wrapper_block ||
        std::count(stmts.begin(), stmts.end(), init_stmt) != 1) {
      std::cerr << "REX_FRONTEND_INVARIANT[range-for-init-owner]: init "
                   "statement did not join its exact wrapper list"
                << std::endl;
      ROSE_ABORT();
    }
    SageBuilder::popScopeStack();
  }

  SgRangeBasedForStatement *sg_for_stmt =
      SageBuilder::buildRangeBasedForStatement_nfi(
          (SgVariableDeclaration *)nullptr, (SgVariableDeclaration *)nullptr,
          (SgVariableDeclaration *)nullptr, (SgVariableDeclaration *)nullptr,
          (SgExpression *)nullptr, (SgExpression *)nullptr,
          (SgStatement *)nullptr);

  SgNode *range_for_parent = wrapper_block != nullptr
                                 ? static_cast<SgNode *>(wrapper_block)
                                 : outer_owner;
  if (sg_for_stmt == nullptr || range_for_parent == nullptr ||
      sg_for_stmt->get_parent() != nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[range-for-owner]: typed range-for "
                 "statement was not constructed detached beneath one exact "
                 "owner"
              << std::endl;
    ROSE_ABORT();
  }
  sg_for_stmt->set_parent(range_for_parent);
  applySourceRange(sg_for_stmt, cxx_for_range_stmt->getSourceRange());
  SageBuilder::pushScopeStack(sg_for_stmt);

  auto translate_var_decl_child =
      [&](clang::Stmt *clang_stmt, const char *child_label, bool required,
          bool canonical_generated = false,
          bool semantic_declaration_shell = false) -> SgVariableDeclaration * {
    if (clang_stmt != nullptr) {
      const auto *decl_stmt = llvm::dyn_cast<clang::DeclStmt>(clang_stmt);
      const struct {
        clang::Stmt *source;
        SgVariableDeclaration *declaration;
      } declaration_slots[] = {
          {clang_range, sg_for_stmt->get_range_declaration()},
          {cxx_for_range_stmt->getBeginStmt(),
           sg_for_stmt->get_begin_declaration()},
          {cxx_for_range_stmt->getEndStmt(),
           sg_for_stmt->get_end_declaration()},
          {clang_loop_var, sg_for_stmt->get_iterator_declaration()}};
      unsigned matching_slots = 0;
      bool exact_pending_slot = true;
      for (const auto &slot : declaration_slots) {
        if (slot.source == clang_stmt) {
          ++matching_slots;
        }
        const bool completed = matching_slots == 0 && slot.source != nullptr;
        if ((slot.declaration != nullptr) != completed ||
            (slot.declaration != nullptr &&
             slot.declaration->get_parent() != sg_for_stmt)) {
          exact_pending_slot = false;
        }
      }
      if (decl_stmt == nullptr || !decl_stmt->isSingleDecl() ||
          !llvm::isa<clang::VarDecl>(decl_stmt->getSingleDecl()) ||
          matching_slots != 1 || !exact_pending_slot ||
          sg_for_stmt->get_not_equal_expression() != nullptr ||
          sg_for_stmt->get_increment_expression() != nullptr ||
          sg_for_stmt->get_loop_body() != nullptr) {
        std::cerr
            << "REX_FRONTEND_INVARIANT[range-for-declaration-transaction]: "
            << child_label
            << " does not belong to its exact empty typed declaration slot"
            << std::endl;
        ROSE_ABORT();
      }
    }
    SgStatement *stmt = translate_for_init_child(
        clang_stmt, child_label, required, canonical_generated,
        semantic_declaration_shell, sg_for_stmt);
    if (stmt == nullptr) {
      return nullptr;
    }

    SgVariableDeclaration *var_decl = isSgVariableDeclaration(stmt);
    if (var_decl == nullptr) {
      std::cerr << "REX_FRONTEND_INVARIANT[range-for-declaration]: "
                << child_label << " translated to " << stmt->class_name()
                << " instead of SgVariableDeclaration" << std::endl;
      ROSE_ABORT();
    }

    return var_decl;
  };

  // SgForStatement for-init unparsing merges declarations and rewrites type
  // deduction semantics, so C++ range-for uses its own typed AST node.
  auto attach_range_declaration_owner = [&](SgVariableDeclaration *decl,
                                            const char *child_label) {
    if (decl == nullptr) {
      return;
    }
    SgInitializedName *init_name = decl->get_variables().size() == 1
                                       ? decl->get_variables().front()
                                       : nullptr;
    if ((decl->get_parent() != nullptr && decl->get_parent() != sg_for_stmt) ||
        decl == sg_for_stmt->get_range_declaration() ||
        decl == sg_for_stmt->get_begin_declaration() ||
        decl == sg_for_stmt->get_end_declaration() ||
        decl == sg_for_stmt->get_iterator_declaration() ||
        init_name == nullptr || init_name->get_parent() != decl ||
        init_name->get_scope() != sg_for_stmt) {
      std::cerr << "REX_FRONTEND_INVARIANT[range-for-declaration-owner]: "
                << child_label
                << " did not produce one unclaimed declaration in its exact "
                   "typed range-for construction"
                << std::endl;
      ROSE_ABORT();
    }
    // Later children refer to this declaration, so publish its final owner
    // and typed slot before translating the next child.
    decl->set_parent(sg_for_stmt);
  };
  SgVariableDeclaration *range_decl = translate_var_decl_child(
      clang_range, "CXXForRangeStmt::range", true, false, true);
  attach_range_declaration_owner(range_decl, "range declaration");
  sg_for_stmt->set_range_declaration(range_decl);
  SgVariableDeclaration *begin_decl =
      translate_var_decl_child(cxx_for_range_stmt->getBeginStmt(),
                               "CXXForRangeStmt::begin", false, true);
  attach_range_declaration_owner(begin_decl, "begin declaration");
  if (begin_decl != nullptr) {
    sg_for_stmt->set_begin_declaration(begin_decl);
  }
  SgVariableDeclaration *end_decl = translate_var_decl_child(
      cxx_for_range_stmt->getEndStmt(), "CXXForRangeStmt::end", false, true);
  attach_range_declaration_owner(end_decl, "end declaration");
  if (end_decl != nullptr) {
    sg_for_stmt->set_end_declaration(end_decl);
  }
  SgVariableDeclaration *iterator_decl = translate_var_decl_child(
      clang_loop_var, "CXXForRangeStmt::loop variable", true);
  attach_range_declaration_owner(iterator_decl, "loop variable");
  sg_for_stmt->set_iterator_declaration(iterator_decl);

  SgExpression *not_equal_expr = nullptr;
  clang::Expr *clang_cond = cxx_for_range_stmt->getCond();
  if (clang_cond != nullptr) {
    SgNode *tmp_cond = Traverse(clang_cond);
    not_equal_expr = isSgExpression(tmp_cond);
    if (not_equal_expr == nullptr) {
      std::cerr << "REX_FRONTEND_INVARIANT[range-for-condition]: Clang "
                   "condition translated to "
                << (tmp_cond == nullptr ? std::string("<null>")
                                        : tmp_cond->class_name())
                << " instead of SgExpression" << std::endl;
      ROSE_ABORT();
    }
    not_equal_expr->set_parent(sg_for_stmt);
    sg_for_stmt->set_not_equal_expression(not_equal_expr);
  }

  SgExpression *increment_expr = nullptr;
  clang::Expr *clang_inc = cxx_for_range_stmt->getInc();
  if (clang_inc != nullptr) {
    SgNode *tmp_inc = Traverse(clang_inc);
    increment_expr = isSgExpression(tmp_inc);
    if (increment_expr == nullptr) {
      std::cerr << "REX_FRONTEND_INVARIANT[range-for-increment]: Clang "
                   "increment translated to "
                << (tmp_inc == nullptr ? std::string("<null>")
                                       : tmp_inc->class_name())
                << " instead of SgExpression" << std::endl;
      ROSE_ABORT();
    }
    increment_expr->set_parent(sg_for_stmt);
    sg_for_stmt->set_increment_expression(increment_expr);
  }

  SgStatement *body = translateControlledSubstatement(clang_body, sg_for_stmt,
                                                      "range-for body");
  body = wrapStatementWithPragmas(clang_body, body);
  body->set_parent(sg_for_stmt);
  sg_for_stmt->set_loop_body(body);

  SageBuilder::popScopeStack();

  // Enforce structural invariants expected by AST tests and CFG support.
  ROSE_ASSERT(sg_for_stmt->get_iterator_declaration() != nullptr);
  ROSE_ASSERT(sg_for_stmt->get_range_declaration() != nullptr);
  ROSE_ASSERT(sg_for_stmt->get_loop_body() != nullptr);
  auto validate_range_declaration = [&](SgVariableDeclaration *decl,
                                        const char *child_label) {
    if (decl == nullptr) {
      return;
    }
    SgInitializedName *init_name = decl->get_variables().size() == 1
                                       ? decl->get_variables().front()
                                       : nullptr;
    if (decl->get_parent() != sg_for_stmt || decl->get_scope() != sg_for_stmt ||
        init_name == nullptr || init_name->get_scope() != sg_for_stmt) {
      std::cerr << "REX_FRONTEND_INVARIANT[range-for-declaration-owner]: "
                << child_label
                << " did not acquire the exact typed structural and semantic "
                   "range-for owner"
                << std::endl;
      ROSE_ABORT();
    }
  };
  validate_range_declaration(iterator_decl, "loop variable");
  validate_range_declaration(range_decl, "range declaration");
  validate_range_declaration(begin_decl, "begin declaration");
  validate_range_declaration(end_decl, "end declaration");

  if (wrapper_block != nullptr) {
    wrapper_block->append_statement(sg_for_stmt);
    SgStatementPtrList &stmts = wrapper_block->get_statements();
    if (init_stmt == nullptr || init_stmt->get_parent() != wrapper_block ||
        std::count(stmts.begin(), stmts.end(), init_stmt) != 1 ||
        sg_for_stmt->get_parent() != wrapper_block ||
        std::count(stmts.begin(), stmts.end(), sg_for_stmt) != 1) {
      std::cerr << "REX_FRONTEND_INVARIANT[range-for-init-owner]: wrapper "
                   "block did not acquire exactly one init and range-for "
                   "statement"
                << std::endl;
      ROSE_ABORT();
    }
    *node = wrapper_block;
  } else {
    *node = sg_for_stmt;
  }

  return VisitStmt(cxx_for_range_stmt, node);
}

bool ClangToSageTranslator::VisitCXXTryStmt(clang::CXXTryStmt *cxx_try_stmt,
                                            SgNode **node) {
  SgNode *try_owner =
      statementConstructionOwner(cxx_try_stmt, "C++ try statement");
  clang::CompoundStmt *try_block = cxx_try_stmt->getTryBlock();
  SgBasicBlock *try_body = SageBuilder::buildBasicBlock_nfi();
  SgTryStmt *try_stmt = SageBuilder::buildTryStmt(try_body);
  const TypedStatementChildConstruction *typed_child =
      activeTypedStatementChildConstructionFor(cxx_try_stmt);
  SgBasicBlock *function_body =
      typed_child != nullptr ? isSgBasicBlock(typed_child->owner) : nullptr;
  if (function_body != nullptr) {
    try_stmt->set_is_function_try_block(true);
    function_body->append_statement(try_stmt);
  } else {
    try_stmt->set_parent(try_owner);
  }
  *node = try_stmt;
  if (!VisitStmt(cxx_try_stmt, node) || *node != try_stmt) {
    return false;
  }

  SgNode *tmp_body =
      translateTypedStatementChild(try_block, try_stmt, "C++ try body");
  if (tmp_body != try_body || try_body->get_parent() != try_stmt ||
      try_stmt->get_body() != try_body) {
    std::cerr << "REX_FRONTEND_INVARIANT[try-body]: Clang try body did not "
                 "retain its exact typed owner"
              << std::endl;
    ROSE_ABORT();
  }
  SgCatchStatementSeq *catch_sequence =
      try_stmt->get_catch_statement_seq_root();
  if (catch_sequence == nullptr || catch_sequence->get_parent() != try_stmt) {
    std::cerr << "REX_FRONTEND_INVARIANT[try-catch-sequence]: try statement "
                 "has no exact typed catch sequence"
              << std::endl;
    ROSE_ABORT();
  }
  // The catch sequence is a structural list container, not an independently
  // spelled source construct.  Giving it the CXXTryStmt range makes it compete
  // with its emitting SgTryStmt for every token in the try/catch surface.
  setSynthesizedFileInfo(catch_sequence);
  mark_compiler_generated_frontend_specific(catch_sequence);

  for (unsigned i = 0; i < cxx_try_stmt->getNumHandlers(); ++i) {
    clang::CXXCatchStmt *catch_stmt = cxx_try_stmt->getHandler(i);

    SgNode *tmp_handler = translateTypedStatementChild(
        catch_stmt, catch_sequence, "C++ catch handler");
    SgCatchOptionStmt *handler_stmt = isSgCatchOptionStmt(tmp_handler);
    const SgStatementPtrList &handlers =
        catch_sequence->get_catch_statement_seq();
    if (handler_stmt == nullptr ||
        handler_stmt->get_parent() != catch_sequence ||
        handler_stmt->get_trystmt() != try_stmt || handlers.empty() ||
        handlers.back() != handler_stmt) {
      std::cerr << "REX_FRONTEND_INVARIANT[catch-handler]: Clang catch handler "
                   "did not publish one exact typed owner: "
                << (tmp_handler == nullptr ? "a null node"
                                           : tmp_handler->class_name())
                << std::endl;
      ROSE_ABORT();
    }
  }

  return true;
}

bool ClangToSageTranslator::VisitDeclStmt(clang::DeclStmt *decl_stmt,
                                          SgNode **node) {
  std::vector<clang::Decl *> visible_decls;
  for (clang::Decl *decl : decl_stmt->decls()) {

    if (decl == nullptr) {
      continue;
    }
    if (llvm::isa<clang::BindingDecl>(decl)) {
      continue;
    }
    visible_decls.push_back(decl);
  }

  DeclStatementConstruction construction;
  construction.statement = decl_stmt;
  construction.lexical_scope = SageBuilder::topScopeStack();
  if (construction.lexical_scope == nullptr) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[declaration-statement-owner]: "
            "DeclStmt=%p has no exact active lexical scope\n",
            static_cast<void *>(decl_stmt));
    ROSE_ABORT();
  }
  if (const TypedStatementChildConstruction *typed_child =
          activeTypedStatementChildConstructionFor(decl_stmt)) {
    if (typed_child->owner == nullptr || typed_child->context == nullptr) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[declaration-statement-owner]: "
              "DeclStmt=%p has an incomplete typed-child construction "
              "frame\n",
              static_cast<void *>(decl_stmt));
      ROSE_ABORT();
    }
    construction.typed_child_owner = typed_child->owner;
  }
  construction.direct_declarations.insert(visible_decls.begin(),
                                          visible_decls.end());
  p_decl_statement_constructions.push_back(std::move(construction));
  struct DeclStatementConstructionGuard {
    std::deque<DeclStatementConstruction> &frames;
    const clang::DeclStmt *statement;
    ~DeclStatementConstructionGuard() {
      if (frames.empty() || frames.back().statement != statement) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[declaration-statement-owner]: "
                "DeclStmt=%p lost its exact construction frame\n",
                static_cast<const void *>(statement));
        ROSE_ABORT();
      }
      frames.pop_back();
    }
  } construction_guard{p_decl_statement_constructions, decl_stmt};

  auto detach_block_scope_function_decl_from_semantic_scope =
      [](clang::Decl *clang_decl, SgNode *translated_node) {
        SgFunctionDeclaration *function_decl =
            isSgFunctionDeclaration(translated_node);
        if (function_decl == nullptr || clang_decl == nullptr ||
            llvm::isa<clang::CXXMethodDecl>(clang_decl) ||
            !llvm::isa<clang::FunctionDecl>(clang_decl)) {
          return;
        }

        SgScopeStatement *statement_scope = SageBuilder::topScopeStack();
        if (statement_scope == nullptr ||
            !scope_supports_statement_list_for_instantiation_ref(
                statement_scope)) {
          return;
        }

        SgScopeStatement *parent_scope =
            isSgScopeStatement(function_decl->get_parent());
        SgScopeStatement *decl_scope = function_decl->get_scope();
        if (parent_scope == statement_scope || decl_scope == statement_scope) {
          return;
        }

        if (SgAuxiliaryDeclarationList *auxiliary_owner =
                isSgAuxiliaryDeclarationList(function_decl->get_parent())) {
          SgScopeStatement *semantic_owner =
              isSgScopeStatement(auxiliary_owner->get_parent());
          if (decl_scope == nullptr || semantic_owner != decl_scope ||
              decl_scope->get_auxiliary_declarations() != auxiliary_owner ||
              std::count(auxiliary_owner->get_declarations().begin(),
                         auxiliary_owner->get_declarations().end(),
                         function_decl) != 1 ||
              is_decl_attached_to_scope_child_list_for_instantiation_ref(
                  decl_scope, function_decl)) {
            std::cerr
                << "REX_FRONTEND_INVARIANT[block-function-source-owner]: "
                   "function="
                << function_decl << " name=" << function_decl->get_name()
                << " does not have one exact semantic auxiliary owner before "
                   "its lexical declaration-statement adoption"
                << std::endl;
            ROSE_ABORT();
          }
          if (!SageBuilder::detachAuxiliaryDeclaration(decl_scope,
                                                       function_decl) ||
              function_decl->get_parent() != nullptr) {
            std::cerr
                << "REX_FRONTEND_INVARIANT[block-function-source-owner]: "
                   "function="
                << function_decl << " name=" << function_decl->get_name()
                << " could not detach from its exact semantic auxiliary owner"
                << std::endl;
            ROSE_ABORT();
          }
          return;
        }

        if (parent_scope != nullptr) {
          (void)detach_decl_from_scope_child_list_for_instantiation_ref(
              function_decl, parent_scope);
          function_decl->set_parent(nullptr);
        }
        if (decl_scope != nullptr && decl_scope != parent_scope) {
          (void)detach_decl_from_scope_child_list_for_instantiation_ref(
              function_decl, decl_scope);
        }
      };

  if (visible_decls.empty()) {
    *node = nullptr;
  } else if (visible_decls.size() == 1) {
    clang::Decl *decl = visible_decls.front();
    *node = Traverse(decl);
    detach_block_scope_function_decl_from_semantic_scope(decl, *node);
    if (llvm::isa<clang::VarDecl>(decl) || llvm::isa<clang::FieldDecl>(decl)) {
      (void)requireExactVariableDeclaratorSurface(decl, *node,
                                                  "VisitDeclStmt(single)");
    }
  } else {
    SgScopeStatement *scope = SageBuilder::topScopeStack();
    std::vector<SgDeclarationStatement *> source_declaration_surfaces;
    SgDeclarationGroupStatement *source_declaration_group = nullptr;
    const size_t initial_source_group_depth =
        p_source_declaration_group_constructions.size();

    for (clang::Decl *decl : visible_decls) {

      ASSERT_not_null(decl);
      if (clang::TagDecl *tag_decl = llvm::dyn_cast<clang::TagDecl>(decl)) {
        if (tagIsSemanticTemplateInstantiation(tag_decl) ||
            tag_decl->isEmbeddedInDeclarator() ||
            tag_decl->getTypedefNameForAnonDecl() != nullptr) {
          // This is not an independent declaration-statement surface. Clang
          // places it before the variable/typedef that owns the spelling; the
          // typed owner recursively constructs and adopts the tag definition.
          continue;
        }
      }
      SgDeclarationGroupStatement *source_group_construction =
          prepareExactSourceDeclarationGroup(decl, scope);
      SgNode *child = Traverse(decl);
      detach_block_scope_function_decl_from_semantic_scope(decl, child);
      if (llvm::isa<clang::VarDecl>(decl) ||
          llvm::isa<clang::FieldDecl>(decl)) {
        (void)requireExactVariableDeclaratorSurface(decl, child,
                                                    "VisitDeclStmt(group)");
      }

      SgDeclarationStatement *sub_decl_stmt = isSgDeclarationStatement(child);
      if (sub_decl_stmt == nullptr) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[declaration-statement]: Clang %s "
                "did not translate to a Sage declaration statement\n",
                decl->getDeclKindName());
        ROSE_ABORT();
      }
      if (source_group_construction != nullptr) {
        if (SgDeclarationGroupStatement *completed_group =
                completeExactSourceDeclarationGroupMember(decl,
                                                          sub_decl_stmt)) {
          source_declaration_group = completed_group;
        }
      }
      source_declaration_surfaces.push_back(sub_decl_stmt);
    }

    if (p_source_declaration_group_constructions.size() !=
        initial_source_group_depth) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[source-declaration-group-complete]: "
              "DeclStmt=%p exited with an incomplete atomic declaration "
              "group\n",
              static_cast<void *>(decl_stmt));
      ROSE_ABORT();
    }

    if (source_declaration_surfaces.empty()) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[declaration-statement]: Clang DeclStmt "
              "has no source declaration surface\n");
      ROSE_ABORT();
    }
    ASSERT_not_null(scope);
    if (source_declaration_surfaces.size() == 1) {
      *node = source_declaration_surfaces.front();
    } else {
      if (source_declaration_group == nullptr ||
          source_declaration_group->get_declarations() !=
              SgDeclarationStatementPtrList(
                  source_declaration_surfaces.begin(),
                  source_declaration_surfaces.end())) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[declaration-statement]: multiple "
                "source surfaces have no exact typed declaration group\n");
        ROSE_ABORT();
      }
      source_declaration_group->validate();
      *node = source_declaration_group;
    }
  }

  return true;
}

bool ClangToSageTranslator::VisitDoStmt(clang::DoStmt *do_stmt, SgNode **node) {
  clang::Expr *cond_expr = do_stmt->getCond();
  ROSE_ASSERT(cond_expr != nullptr);
  clang::Stmt *body_stmt = do_stmt->getBody();
  ROSE_ASSERT(body_stmt != nullptr);

  SgDoWhileStmt *sg_do_stmt =
      SageBuilder::buildDoWhileStmt_nfi(nullptr, nullptr);
  sg_do_stmt->set_parent(statementConstructionOwner(do_stmt, "do statement"));
  *node = sg_do_stmt;
  if (!VisitStmt(do_stmt, node)) {
    return false;
  }
  SageBuilder::pushScopeStack(sg_do_stmt);

  SgNode *tmp_cond = Traverse(cond_expr);
  SgExpression *cond = isSgExpression(tmp_cond);
  ROSE_ASSERT(cond != nullptr);

  SgStatement *expr_stmt = SageBuilder::buildExprStatement_nfi(cond);
  applySourceRange(expr_stmt, cond_expr->getSourceRange());
  expr_stmt->set_parent(sg_do_stmt);
  sg_do_stmt->set_condition(expr_stmt);

  SgStatement *body =
      translateControlledSubstatement(body_stmt, sg_do_stmt, "do body");
  ROSE_ASSERT(body != nullptr);
  body = wrapStatementWithPragmas(body_stmt, body);

  body->set_parent(sg_do_stmt);
  sg_do_stmt->set_body(body);

  SageBuilder::popScopeStack();

  return true;
}

bool ClangToSageTranslator::VisitForStmt(clang::ForStmt *for_stmt,
                                         SgNode **node) {
  clang::Stmt *clang_body = for_stmt->getBody();
  if (clang_body == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[for-body]: Clang for-statement has "
                 "no body"
              << std::endl;
    ROSE_ABORT();
  }
  SgForStatement *sg_for_stmt = new SgForStatement(
      static_cast<SgStatement *>(nullptr), static_cast<SgExpression *>(nullptr),
      static_cast<SgStatement *>(nullptr));

  sg_for_stmt->set_parent(
      statementConstructionOwner(for_stmt, "for statement"));
  ROSE_ASSERT(sg_for_stmt->get_parent() != nullptr);
  *node = sg_for_stmt;
  if (!VisitStmt(for_stmt, node)) {
    return false;
  }

  SageBuilder::pushScopeStack(sg_for_stmt);

  // Initialization

  SgForInitStatement *for_init_stmt = nullptr;

  {
    SgStatementPtrList for_init_stmt_list;
    SgDeclarationGroupStatement *source_declaration_group = nullptr;
    clang::Stmt *clang_init = for_stmt->getInit();
    bool prev_in_for_init = p_in_for_init_translation;
    p_in_for_init_translation = true;

    if (clang::DeclStmt *decl_stmt =
            llvm::dyn_cast_or_null<clang::DeclStmt>(clang_init)) {
      std::set<const clang::TagDecl *> inline_tag_decls;
      auto get_tag_decl_from_type =
          [&](clang::QualType qual_type) -> clang::TagDecl * {
        const clang::Type *type = qual_type.getTypePtrOrNull();
        while (type != nullptr) {
          if (auto *record_type = llvm::dyn_cast<clang::RecordType>(type)) {
            return llvm::dyn_cast_or_null<clang::TagDecl>(
                record_type->getDecl());
          }
          if (auto *enum_type = llvm::dyn_cast<clang::EnumType>(type)) {
            return llvm::dyn_cast_or_null<clang::TagDecl>(enum_type->getDecl());
          }

          clang::QualType next_qual_type;
          if (qualifiedTypeHasQualifier(type)) {
            next_qual_type = qual_type.getCanonicalType();
          } else if (auto *ptr = llvm::dyn_cast<clang::PointerType>(type)) {
            next_qual_type = ptr->getPointeeType();
          } else if (auto *array = llvm::dyn_cast<clang::ArrayType>(type)) {
            next_qual_type = array->getElementType();
          } else {
            break;
          }

          const clang::Type *next_type = next_qual_type.getTypePtrOrNull();
          if (next_type == nullptr || next_type == type) {
            break;
          }
          qual_type = next_qual_type;
          type = next_type;
        }
        return nullptr;
      };
      std::set<const clang::TagDecl *> tag_decls_in_stmt;
      std::unordered_map<const clang::TagDecl *, clang::TagDecl *>
          pending_inline_tag_surfaces;
      const size_t initial_source_group_depth =
          p_source_declaration_group_constructions.size();
      for (auto it = decl_stmt->decl_begin(); it != decl_stmt->decl_end();
           ++it) {
        if (auto *tag_decl = llvm::dyn_cast<clang::TagDecl>(*it)) {
          tag_decls_in_stmt.insert(tag_decl->getCanonicalDecl());
        }
      }
      if (!tag_decls_in_stmt.empty()) {
        for (auto it = decl_stmt->decl_begin(); it != decl_stmt->decl_end();
             ++it) {
          if (auto *decl = llvm::dyn_cast<clang::DeclaratorDecl>(*it)) {
            if (clang::TagDecl *tag = get_tag_decl_from_type(decl->getType())) {
              if (tag_decls_in_stmt.find(tag->getCanonicalDecl()) !=
                  tag_decls_in_stmt.end()) {
                inline_tag_decls.insert(tag->getCanonicalDecl());
              }
            }
          }
        }
      }
      for (auto it = decl_stmt->decl_begin(); it != decl_stmt->decl_end();
           ++it) {
        clang::Decl *decl = *it;
        if (decl == nullptr) {
          std::cerr << "REX_FRONTEND_INVARIANT[for-init-declaration]: Clang "
                       "DeclStmt contains a null declaration"
                    << std::endl;
          ROSE_ABORT();
        }
        if (clang::TagDecl *tag_decl = llvm::dyn_cast<clang::TagDecl>(decl)) {
          bool treat_as_embedded =
              tag_decl->isEmbeddedInDeclarator() ||
              tag_decl->getTypedefNameForAnonDecl() != nullptr ||
              inline_tag_decls.find(tag_decl->getCanonicalDecl()) !=
                  inline_tag_decls.end();
          if (treat_as_embedded) {
            const clang::TagDecl *canonical_tag = tag_decl->getCanonicalDecl();
            if (!pending_inline_tag_surfaces.emplace(canonical_tag, tag_decl)
                     .second) {
              std::cerr << "REX_FRONTEND_INVARIANT[for-init-inline-tag]: "
                           "one canonical tag has multiple pending source "
                           "surfaces"
                        << std::endl;
              ROSE_ABORT();
            }
            continue;
          }
        }
        SgDeclarationGroupStatement *source_group_construction =
            prepareExactSourceDeclarationGroup(decl, sg_for_stmt);
        SgNode *child = Traverse(decl);
        if (llvm::isa<clang::VarDecl>(decl) ||
            llvm::isa<clang::FieldDecl>(decl)) {
          (void)requireExactVariableDeclaratorSurface(decl, child,
                                                      "VisitForStmt(init)");
        }
        if (clang::DeclaratorDecl *declarator_decl =
                llvm::dyn_cast<clang::DeclaratorDecl>(decl)) {
          clang::TagDecl *type_tag =
              get_tag_decl_from_type(declarator_decl->getType());
          const clang::TagDecl *canonical_tag =
              type_tag != nullptr ? type_tag->getCanonicalDecl() : nullptr;
          auto pending = canonical_tag != nullptr
                             ? pending_inline_tag_surfaces.find(canonical_tag)
                             : pending_inline_tag_surfaces.end();
          if (pending != pending_inline_tag_surfaces.end()) {
            SgVariableDeclaration *variable_decl =
                isSgVariableDeclaration(child);
            SgDeclarationStatement *tag_surface =
                lookupExactSgDeclarationForClangDecl(pending->second,
                                                     /*allow_on_demand=*/false);
            if (variable_decl == nullptr || tag_surface == nullptr ||
                variable_decl->get_variables().empty()) {
              std::cerr << "REX_FRONTEND_INVARIANT[for-init-inline-tag]: "
                           "exact inline tag has no variable-declarator owner"
                        << std::endl;
              ROSE_ABORT();
            }

            SgDeclarationStatement *defining_tag = nullptr;
            SgInitializedName *variable_name =
                variable_decl->get_variables().front();
            if (variable_name == nullptr ||
                variable_name->get_type() == nullptr) {
              std::cerr << "REX_FRONTEND_INVARIANT[for-init-inline-tag]: "
                           "variable-declarator owner has no exact type"
                        << std::endl;
              ROSE_ABORT();
            }
            SgType *base_type = variable_name->get_type()->findBaseType();
            if (base_type == nullptr) {
              std::cerr << "REX_FRONTEND_INVARIANT[for-init-inline-tag]: "
                           "variable-declarator type has no base type"
                        << std::endl;
              ROSE_ABORT();
            }
            if (SgClassDeclaration *class_surface =
                    isSgClassDeclaration(tag_surface)) {
              SgClassType *class_type = isSgClassType(base_type);
              SgClassDeclaration *type_decl =
                  class_type != nullptr
                      ? isSgClassDeclaration(class_type->get_declaration())
                      : nullptr;
              SgClassDeclaration *surface_first = isSgClassDeclaration(
                  class_surface->get_firstNondefiningDeclaration());
              SgClassDeclaration *type_first =
                  type_decl != nullptr
                      ? isSgClassDeclaration(
                            type_decl->get_firstNondefiningDeclaration())
                      : nullptr;
              if (type_decl == nullptr ||
                  (type_decl != class_surface && type_first != class_surface &&
                   surface_first != type_decl &&
                   (surface_first == nullptr || type_first == nullptr ||
                    surface_first != type_first))) {
                std::cerr
                    << "REX_FRONTEND_INVARIANT[for-init-inline-tag]: exact "
                       "class source surface does not match the declarator "
                       "base type"
                    << std::endl;
                ROSE_ABORT();
              }
              defining_tag = isSgClassDeclaration(
                  class_surface->get_definingDeclaration());
              if (defining_tag == nullptr &&
                  class_surface->get_definition() != nullptr) {
                defining_tag = class_surface;
              }
            } else if (SgEnumDeclaration *enum_surface =
                           isSgEnumDeclaration(tag_surface)) {
              SgEnumType *enum_type = isSgEnumType(base_type);
              SgEnumDeclaration *type_decl =
                  enum_type != nullptr
                      ? isSgEnumDeclaration(enum_type->get_declaration())
                      : nullptr;
              SgEnumDeclaration *surface_first = isSgEnumDeclaration(
                  enum_surface->get_firstNondefiningDeclaration());
              SgEnumDeclaration *type_first =
                  type_decl != nullptr
                      ? isSgEnumDeclaration(
                            type_decl->get_firstNondefiningDeclaration())
                      : nullptr;
              if (type_decl == nullptr ||
                  (type_decl != enum_surface && type_first != enum_surface &&
                   surface_first != type_decl &&
                   (surface_first == nullptr || type_first == nullptr ||
                    surface_first != type_first))) {
                std::cerr
                    << "REX_FRONTEND_INVARIANT[for-init-inline-tag]: exact "
                       "enum source surface does not match the declarator "
                       "base type"
                    << std::endl;
                ROSE_ABORT();
              }
              defining_tag =
                  isSgEnumDeclaration(enum_surface->get_definingDeclaration());
              if (defining_tag == nullptr && !enum_surface->isForward()) {
                defining_tag = enum_surface;
              }
            }
            if (defining_tag == nullptr) {
              std::cerr << "REX_FRONTEND_INVARIANT[for-init-inline-tag]: "
                           "source-defined tag has no exact defining Sage "
                           "declaration"
                        << std::endl;
              ROSE_ABORT();
            }

            if (defining_tag->get_parent() != variable_decl ||
                variable_decl->get_baseTypeDefiningDeclaration() !=
                    defining_tag) {
              std::cerr << "REX_FRONTEND_INVARIANT[for-init-inline-tag]: exact "
                           "defining tag was not adopted by its variable "
                           "declarator during declaration translation"
                        << std::endl;
              ROSE_ABORT();
            }

            if (SgClassDeclaration *class_tag =
                    isSgClassDeclaration(defining_tag)) {
              class_tag->set_isAutonomousDeclaration(false);
            } else if (SgEnumDeclaration *enum_tag =
                           isSgEnumDeclaration(defining_tag)) {
              enum_tag->set_isAutonomousDeclaration(false);
            } else {
              std::cerr << "REX_FRONTEND_INVARIANT[for-init-inline-tag]: typed "
                           "owner is neither a class nor enum declaration"
                        << std::endl;
              ROSE_ABORT();
            }
            require_inline_tag_source_provenance(defining_tag, "VisitForStmt",
                                                 "defining declaration");
            if (SgClassDeclaration *class_defining =
                    isSgClassDeclaration(defining_tag)) {
              require_inline_tag_source_provenance(
                  class_defining->get_definition(), "VisitForStmt",
                  "class definition");
            }
            if (variable_decl->get_baseTypeDefiningDeclaration() !=
                    defining_tag ||
                defining_tag->get_parent() != variable_decl ||
                defining_tag->get_scope() != sg_for_stmt) {
              std::cerr << "REX_FRONTEND_INVARIANT[for-init-inline-tag]: "
                           "variable declarator did not adopt one exact tag "
                           "definition"
                        << std::endl;
              ROSE_ABORT();
            }
            pending_inline_tag_surfaces.erase(pending);
          }
        }
        SgStatement *stmt = isSgStatement(child);
        if (stmt == nullptr) {
          std::cerr << "REX_FRONTEND_INVARIANT[for-init-declaration]: Clang "
                    << decl->getDeclKindName() << " translated to "
                    << (child == nullptr ? std::string("<null>")
                                         : child->class_name())
                    << " instead of SgStatement" << std::endl;
          ROSE_ABORT();
        }
        if (SgDeclarationStatement *source_group_surface =
                isSgDeclarationStatement(stmt)) {
          if (source_group_construction != nullptr) {
            if (SgDeclarationGroupStatement *completed_group =
                    completeExactSourceDeclarationGroupMember(
                        decl, source_group_surface)) {
              source_declaration_group = completed_group;
            }
          }
        }
        if (stmt != nullptr) {
          for_init_stmt_list.push_back(stmt);
        }
      }
      if (p_source_declaration_group_constructions.size() !=
          initial_source_group_depth) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[source-declaration-group-complete]: "
                "for-init DeclStmt=%p exited with an incomplete atomic "
                "declaration group\n",
                static_cast<void *>(decl_stmt));
        ROSE_ABORT();
      }
      if (!pending_inline_tag_surfaces.empty()) {
        std::cerr << "REX_FRONTEND_INVARIANT[for-init-inline-tag]: exact "
                     "source tag was not consumed by its variable declarator"
                  << std::endl;
        ROSE_ABORT();
      }
    } else if (clang_init != nullptr) {
      SgStatement *init_stmt =
          translateStatement(clang_init, "for initializer", false);
      ASSERT_not_null(init_stmt);
      for_init_stmt_list.push_back(init_stmt);
    } else {
      // The first semicolon in `for (; condition; increment)` is source
      // syntax, but Clang represents the absent initializer with a null edge.
      // Sage's typed ownership contract represents that syntax explicitly:
      // every SgForInitStatement owns at least one statement, with an
      // SgNullStatement denoting syntactic absence.
      SgNullStatement *absent_initializer =
          SageBuilder::buildNullStatement_nfi();
      setSynthesizedFileInfo(absent_initializer);
      mark_compiler_generated_frontend_specific(absent_initializer);
      for_init_stmt_list.push_back(absent_initializer);
    }

    p_in_for_init_translation = prev_in_for_init;

    if (source_declaration_group != nullptr) {
      SgStatementPtrList grouped_init;
      bool inserted_group = false;
      for (SgStatement *statement : for_init_stmt_list) {
        SgDeclarationStatement *declaration =
            isSgDeclarationStatement(statement);
        if (declaration != nullptr &&
            declaration->get_parent() == source_declaration_group) {
          if (!inserted_group) {
            grouped_init.push_back(source_declaration_group);
            inserted_group = true;
          }
          continue;
        }
        grouped_init.push_back(statement);
      }
      if (!inserted_group) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[for-init-declaration-group]: typed "
                "group has no member in the exact for-init surface list\n");
        ROSE_ABORT();
      }
      source_declaration_group->validate();
      for_init_stmt_list.swap(grouped_init);
    }

    for_init_stmt = sg_for_stmt->get_for_init_stmt();
    if (for_init_stmt == nullptr ||
        for_init_stmt->get_parent() != sg_for_stmt ||
        !for_init_stmt->get_init_stmt().empty()) {
      std::cerr << "REX_FRONTEND_INVARIANT[for-init-transaction]: fresh "
                   "for-statement does not own one empty typed initializer "
                   "list"
                << std::endl;
      ROSE_ABORT();
    }
    for (SgStatement *init_stmt : for_init_stmt_list) {
      if (init_stmt == nullptr) {
        std::cerr << "REX_FRONTEND_INVARIANT[for-init-statement]: exact "
                     "initializer list contains a null Sage statement"
                  << std::endl;
        ROSE_ABORT();
      }
      if (init_stmt->get_parent() != nullptr) {
        std::cerr << "REX_FRONTEND_INVARIANT[for-init-statement]: exact "
                     "initializer already has a structural owner before its "
                     "typed for-init attachment"
                  << std::endl;
        ROSE_ABORT();
      }
      for_init_stmt->append_init_stmt(init_stmt);
      if (init_stmt->get_parent() != for_init_stmt ||
          std::count(for_init_stmt->get_init_stmt().begin(),
                     for_init_stmt->get_init_stmt().end(), init_stmt) != 1) {
        std::cerr << "REX_FRONTEND_INVARIANT[for-init-statement]: typed "
                     "initializer attachment did not publish one exact owner "
                     "edge"
                  << std::endl;
        ROSE_ABORT();
      }
    }

    if (for_stmt->getInit() != nullptr) {
      const bool semantic_initializer =
          p_semantic_function_body_traversal_depth != 0 ||
          p_semantic_expression_subtree_depth != 0;
      applySourceRange(for_init_stmt, for_stmt->getInit()->getSourceRange());
      if (!semantic_initializer) {
        Sg_File_Info *for_init_fi = for_init_stmt->get_file_info();
        if (for_init_stmt->get_startOfConstruct() == nullptr ||
            for_init_stmt->get_endOfConstruct() == nullptr ||
            for_init_fi == nullptr || for_init_fi->isCompilerGenerated()) {
          std::cerr
              << "REX_FRONTEND_INVARIANT[for-init-source]: source-written "
                 "initializer has no exact Sage source interval"
              << std::endl;
          ROSE_ABORT();
        }
      }
    } else {
      setSynthesizedFileInfo(for_init_stmt);
      mark_compiler_generated_frontend_specific(for_init_stmt);
    }

    // Ensure for-init statements are parented by the SgForInitStatement while
    // keeping their scope on the enclosing for-statement.
    for (SgStatement *init_stmt : for_init_stmt_list) {
      if (init_stmt == nullptr) {
        std::cerr << "REX_FRONTEND_INVARIANT[for-init-statement]: exact "
                     "initializer list contains a null Sage statement"
                  << std::endl;
        ROSE_ABORT();
      }
      if (SgDeclarationStatement *decl = isSgDeclarationStatement(init_stmt)) {
        if (decl->get_scope() != sg_for_stmt) {
          std::cerr << "REX_FRONTEND_INVARIANT[for-init-owner]: declaration "
                    << decl->class_name()
                    << " is not owned by the exact for-statement scope"
                    << std::endl;
          ROSE_ABORT();
        }
        if (SgVariableDeclaration *var_decl = isSgVariableDeclaration(decl)) {
          for (SgInitializedName *init_name : var_decl->get_variables()) {
            if (init_name == nullptr || init_name->get_scope() != sg_for_stmt) {
              std::cerr << "REX_FRONTEND_INVARIANT[for-init-owner]: variable "
                           "declarator is absent or has the wrong semantic "
                           "scope"
                        << std::endl;
              ROSE_ABORT();
            }
          }
        }
      }
    }
  }

  // Condition

  SgStatement *cond_stmt = nullptr;

  {
    if (clang::DeclStmt *cond_decl = for_stmt->getConditionVariableDeclStmt()) {
      SgNode *tmp_cond = nullptr;
      {
        PendingConditionDeclarationScopeGuard condition_scope(
            p_pending_condition_declaration_scope_stack, cond_decl,
            sg_for_stmt);
        tmp_cond = Traverse(cond_decl);
      }
      cond_stmt = isSgStatement(tmp_cond);
      if (cond_stmt == nullptr) {
        std::cerr << "REX_FRONTEND_INVARIANT[for-condition-declaration]: "
                     "Clang condition translated to "
                  << (tmp_cond == nullptr ? std::string("<null>")
                                          : tmp_cond->class_name())
                  << " instead of SgStatement" << std::endl;
        ROSE_ABORT();
      }
      requireExactConditionDeclarationResult(cond_decl, cond_stmt, sg_for_stmt,
                                             "for");
      sg_for_stmt->set_condition_value(
          buildDeclarationConditionValue(for_stmt->getCond(), sg_for_stmt));
    } else {
      clang::Expr *clang_cond = for_stmt->getCond();
      SgNode *tmp_cond = Traverse(clang_cond);
      SgExpression *cond = isSgExpression(tmp_cond);
      if (clang_cond != nullptr) {
        if (cond == nullptr) {
          std::cerr << "REX_FRONTEND_INVARIANT[for-condition-expression]: "
                       "Clang condition translated to "
                    << (tmp_cond == nullptr ? std::string("<null>")
                                            : tmp_cond->class_name())
                    << " instead of SgExpression" << std::endl;
          ROSE_ABORT();
        }
        cond_stmt = SageBuilder::buildExprStatement_nfi(cond);
        applySourceRange(cond_stmt, clang_cond->getSourceRange());
      } else {
        cond_stmt = SageBuilder::buildNullStatement_nfi();
        setSynthesizedFileInfo(cond_stmt);
        mark_compiler_generated_frontend_specific(cond_stmt);
      }
    }

    cond_stmt->set_parent(sg_for_stmt);
    sg_for_stmt->set_test(cond_stmt);
    requireExactAttachedConditionDeclaration(cond_stmt, sg_for_stmt, "for");
  }

  // Increment

  SgExpression *inc = nullptr;

  {
    clang::Expr *clang_inc = for_stmt->getInc();
    SgNode *tmp_inc = Traverse(clang_inc);
    inc = isSgExpression(tmp_inc);
    if (clang_inc != nullptr && inc == nullptr) {
      std::cerr << "REX_FRONTEND_INVARIANT[for-increment]: Clang increment "
                   "translated to "
                << (tmp_inc == nullptr ? std::string("<null>")
                                       : tmp_inc->class_name())
                << " instead of SgExpression" << std::endl;
      ROSE_ABORT();
    }
    if (clang_inc == nullptr) {
      inc = SageBuilder::buildNullExpression_nfi(
          SgNullExpression::e_null_expression_syntactic_absence);
      setSynthesizedFileInfo(inc);
      mark_compiler_generated_frontend_specific(inc);
    }
  }

  // Body

  SgStatement *body =
      translateControlledSubstatement(clang_body, sg_for_stmt, "for body");
  body = wrapStatementWithPragmas(clang_body, body);

  SageBuilder::popScopeStack();
  SageBuilder::buildForStatement_nfi(sg_for_stmt, for_init_stmt, cond_stmt, inc,
                                     body);

  return true;
}

bool ClangToSageTranslator::VisitGotoStmt(clang::GotoStmt *goto_stmt,
                                          SgNode **node) {
  clang::LabelDecl *label_decl = goto_stmt->getLabel();
  ROSE_ASSERT(label_decl != nullptr);
  SgLabelSymbol *symbol = isSgLabelSymbol(GetSymbolFromSymbolTable(label_decl));
  if (symbol == nullptr || symbol->get_declaration() == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[goto-label]: exact label "
                 "translation has no label symbol"
              << std::endl;
    ROSE_ABORT();
  }
  *node = SageBuilder::buildGotoStatement_nfi(symbol->get_declaration());
  return VisitStmt(goto_stmt, node);
}

bool ClangToSageTranslator::VisitIfStmt(clang::IfStmt *if_stmt, SgNode **node) {
  const bool is_consteval = if_stmt->isConsteval();
  clang::Stmt *then_clang_stmt = if_stmt->getThen();
  if (then_clang_stmt == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[if-then-body]: Clang IfStmt has no "
                 "then body"
              << std::endl;
    ROSE_ABORT();
  }
  clang::Expr *cond_expr_clang = if_stmt->getCond();
  if (!is_consteval && if_stmt->getConditionVariableDeclStmt() == nullptr &&
      cond_expr_clang == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[if-condition-expression]: Clang "
                 "IfStmt has no condition"
              << std::endl;
    ROSE_ABORT();
  }
  if (is_consteval && (cond_expr_clang != nullptr ||
                       if_stmt->getConditionVariableDeclStmt() != nullptr ||
                       if_stmt->getInit() != nullptr ||
                       !llvm::isa<clang::CompoundStmt>(then_clang_stmt) ||
                       (if_stmt->getElse() != nullptr &&
                        !llvm::isa<clang::CompoundStmt>(if_stmt->getElse())))) {
    std::cerr << "REX_FRONTEND_INVARIANT[consteval-if-shape]: consteval IF "
                 "has a condition/init or a noncompound body\n";
    ROSE_ABORT();
  }

  // C++17 if-init statements (if (init; cond)) are represented by getInit().
  // Since SgIfStmt does not directly model init-statements, lower this shape to
  // an equivalent block:
  //   { init; if (cond) ... }
  clang::Stmt *clang_if_init_stmt = if_stmt->getInit();
  SgBasicBlock *if_init_wrapper = nullptr;
  SgNode *if_outer_owner = statementConstructionOwner(if_stmt, "if statement");
  if (clang_if_init_stmt != nullptr) {
    if (!clang_if_init_stmt->getBeginLoc().isValid() ||
        !if_stmt->getEndLoc().isValid()) {
      std::cerr << "REX_FRONTEND_INVARIANT[if-init-source]: source-written "
                   "if-init has no valid Clang source interval"
                << std::endl;
      ROSE_ABORT();
    }
    if_init_wrapper = SageBuilder::buildBasicBlock_nfi();
    if_init_wrapper->set_parent(if_outer_owner);
    applySourceRange(if_init_wrapper,
                     clang::SourceRange(clang_if_init_stmt->getBeginLoc(),
                                        if_stmt->getEndLoc()));
    SageBuilder::pushScopeStack(if_init_wrapper);

    SgStatement *sg_init_stmt =
        translateStatement(clang_if_init_stmt, "if initializer", false);
    if (llvm::isa<clang::Expr>(clang_if_init_stmt)) {
      if_init_wrapper->append_statement(sg_init_stmt);
    } else if (sg_init_stmt->get_parent() != if_init_wrapper ||
               std::count(if_init_wrapper->get_statements().begin(),
                          if_init_wrapper->get_statements().end(),
                          sg_init_stmt) != 1) {
      std::cerr << "REX_FRONTEND_INVARIANT[if-init-owner]: translated "
                   "statement was not published exactly once by its active "
                   "wrapper scope"
                << std::endl;
      ROSE_ABORT();
    }
  }

  *node = SageBuilder::buildIfStmt_nfi(nullptr, nullptr, nullptr);
  (*node)->set_parent(if_init_wrapper != nullptr ? if_init_wrapper
                                                 : if_outer_owner);
  // Publish the source identity of the typed scope shell before translating
  // its condition and bodies.  Nested declarative regions can legitimately
  // use the active SgIfStmt as their exact physical output owner; leaving the
  // shell unclassified until VisitStmt returns makes that bottom-up ownership
  // transaction impossible to validate.
  clang::SourceRange if_source_range = extendSourceRangeWithTrailingSemicolon(
      if_stmt->getSourceRange(), p_compiler_instance->getSourceManager(),
      p_compiler_instance->getLangOpts());
  applySourceRange(*node, if_source_range);
  auto *sage_if = isSgIfStmt(*node);
  switch (if_stmt->getStatementKind()) {
  case clang::IfStatementKind::Ordinary:
    sage_if->set_if_statement_kind(SgIfStmt::e_if_ordinary);
    break;
  case clang::IfStatementKind::Constexpr:
    sage_if->set_if_statement_kind(SgIfStmt::e_if_constexpr);
    break;
  case clang::IfStatementKind::ConstevalNonNegated:
    sage_if->set_if_statement_kind(SgIfStmt::e_if_consteval);
    break;
  case clang::IfStatementKind::ConstevalNegated:
    sage_if->set_if_statement_kind(SgIfStmt::e_if_not_consteval);
    break;
  default:
    ROSE_ABORT();
  }
  if (if_stmt->isConstexpr()) {
    const auto selected =
        if_stmt->getNondiscardedCase(p_compiler_instance->getASTContext());
    if (!selected.has_value() && !cond_expr_clang->isValueDependent())
      failExactExpressionTranslation("constexpr-if-selection", cond_expr_clang);
    sage_if->set_if_constexpr_value_known(selected.has_value());
    sage_if->set_if_constexpr_value(selected.has_value() &&
                                    *selected == then_clang_stmt);
  }
  SageBuilder::pushScopeStack(isSgScopeStatement(*node));

  SgStatement *cond_stmt = nullptr;
  if (clang::DeclStmt *cond_decl = llvm::dyn_cast_or_null<clang::DeclStmt>(
          if_stmt->getConditionVariableDeclStmt())) {
    SgNode *tmp_cond = nullptr;
    {
      PendingConditionDeclarationScopeGuard condition_scope(
          p_pending_condition_declaration_scope_stack, cond_decl,
          isSgScopeStatement(*node));
      tmp_cond = Traverse(cond_decl);
    }
    cond_stmt = isSgStatement(tmp_cond);
    if (cond_stmt == nullptr) {
      std::cerr << "REX_FRONTEND_INVARIANT[if-condition-declaration]: Clang "
                   "condition translated to "
                << (tmp_cond == nullptr ? std::string("<null>")
                                        : tmp_cond->class_name())
                << " instead of SgStatement" << std::endl;
      ROSE_ABORT();
    }
    requireExactConditionDeclarationResult(cond_decl, cond_stmt,
                                           isSgScopeStatement(*node), "if");
    sage_if->set_condition_value(
        buildDeclarationConditionValue(cond_expr_clang, sage_if));
  } else if (!is_consteval) {
    SgNode *tmp_cond = Traverse(cond_expr_clang);
    SgExpression *cond_expr = isSgExpression(tmp_cond);
    if (cond_expr == nullptr) {
      std::cerr << "REX_FRONTEND_INVARIANT[if-condition-expression]: Clang "
                   "condition translated to "
                << (tmp_cond == nullptr ? std::string("<null>")
                                        : tmp_cond->class_name())
                << " instead of SgExpression" << std::endl;
      ROSE_ABORT();
    }
    cond_stmt = SageBuilder::buildExprStatement_nfi(cond_expr);
    applySourceRange(cond_stmt, cond_expr_clang->getSourceRange());
  }

  if (cond_stmt != nullptr) {
    cond_stmt->set_parent(*node);
    sage_if->set_conditional(cond_stmt);
    requireExactAttachedConditionDeclaration(cond_stmt, sage_if, "if");
  }

  SgStatement *then_stmt = translateControlledSubstatement(
      then_clang_stmt, isSgScopeStatement(*node), "if true body");
  then_stmt = wrapStatementWithPragmas(then_clang_stmt, then_stmt);

  clang::Stmt *else_clang_stmt = if_stmt->getElse();
  SgStatement *else_stmt =
      else_clang_stmt != nullptr
          ? translateControlledSubstatement(
                else_clang_stmt, isSgScopeStatement(*node), "if false body")
          : nullptr;
  if (else_stmt != nullptr) {
    else_stmt = wrapStatementWithPragmas(else_clang_stmt, else_stmt);
  }

  SageBuilder::popScopeStack();

  then_stmt->set_parent(*node);
  isSgIfStmt(*node)->set_true_body(then_stmt);
  if (else_stmt != nullptr) {
    else_stmt->set_parent(*node);
    isSgIfStmt(*node)->set_false_body(else_stmt);
  }
  auto validate_direct_declaration_body =
      [&](SgStatement *body, SgStatement *typed_edge, const char *edge_name) {
        SgDeclarationStatement *declaration = isSgDeclarationStatement(body);
        if (declaration == nullptr) {
          return;
        }
        SgIfStmt *owner = isSgIfStmt(*node);
        if (owner == nullptr || typed_edge != declaration ||
            declaration->get_parent() != owner ||
            declaration->get_scope() != owner) {
          fprintf(stderr,
                  "REX_FRONTEND_INVARIANT[if-declaration-body-owner]: "
                  "%s declaration=%p did not acquire the exact SgIfStmt "
                  "typed edge\n",
                  edge_name, static_cast<void *>(declaration));
          ROSE_ABORT();
        }
      };
  validate_direct_declaration_body(then_stmt,
                                   isSgIfStmt(*node)->get_true_body(), "then");
  if (else_stmt != nullptr) {
    validate_direct_declaration_body(
        else_stmt, isSgIfStmt(*node)->get_false_body(), "else");
  }

  if (if_init_wrapper != nullptr) {
    if_init_wrapper->append_statement(isSgIfStmt(*node));
    SageBuilder::popScopeStack();
    *node = if_init_wrapper;
    // Avoid VisitStmt(if_stmt, node) here: it would overwrite the wrapper's
    // explicit init-to-end source range with if_stmt->getSourceRange().
    return true;
  }

  return VisitStmt(if_stmt, node);
}

bool ClangToSageTranslator::VisitIndirectGotoStmt(
    clang::IndirectGotoStmt *indirect_goto_stmt, SgNode **node) {
  SgNode *tmp_target = Traverse(indirect_goto_stmt->getTarget());
  SgExpression *target_expr = isSgExpression(tmp_target);

  if (target_expr == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[indirect-goto-target]: Clang target "
                 "translated to "
              << (tmp_target == nullptr ? std::string("<null>")
                                        : tmp_target->class_name())
              << " instead of SgExpression" << std::endl;
    ROSE_ABORT();
  }
  target_expr = prepareExpressionForAttachment(target_expr);
  if (target_expr->get_parent() != nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[indirect-goto-target]: translated "
                 "target cannot be attached with exact ownership"
              << std::endl;
    ROSE_ABORT();
  }
  SgGotoStatement *sg_goto_stmt =
      SageBuilder::buildGotoStatement_nfi(target_expr);
  ASSERT_not_null(sg_goto_stmt);
  *node = sg_goto_stmt;

  return VisitStmt(indirect_goto_stmt, node);
}

bool ClangToSageTranslator::VisitMSDependentExistsStmt(
    clang::MSDependentExistsStmt *ms_dependent_exists_stmt, SgNode **node) {
  rejectUnimplementedStatement(ms_dependent_exists_stmt);
}

bool ClangToSageTranslator::VisitNullStmt(clang::NullStmt *null_stmt,
                                          SgNode **node) {
  // This node corresponds to a real Clang statement even when it belongs to
  // a semantic template-instantiation subtree.  Leave its source role
  // unclassified until VisitStmt atomically publishes either lexical or
  // synthesized frontend provenance.
  *node = SageBuilder::buildNullStatement_nfi();

  return VisitStmt(null_stmt, node);
}

bool ClangToSageTranslator::VisitReturnStmt(clang::ReturnStmt *return_stmt,
                                            SgNode **node) {
  clang::Expr *clang_return_expr = return_stmt->getRetValue();
  SgNode *tmp_expr = Traverse(clang_return_expr);
  SgExpression *expr = isSgExpression(tmp_expr);
  if (clang_return_expr != nullptr && expr == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[return-expression]: Clang return "
                 "operand translated to "
              << (tmp_expr == nullptr ? std::string("<null>")
                                      : tmp_expr->class_name())
              << " instead of SgExpression" << std::endl;
    ROSE_ABORT();
  }
  if (SgExprListExp *expr_list = isSgExprListExp(expr)) {
    const clang::Expr *ret_value = return_stmt->getRetValue();
    const clang::Expr *written_ret_value =
        ret_value != nullptr ? ret_value->IgnoreImplicit() : nullptr;
    if (written_ret_value != nullptr &&
        (llvm::isa<clang::InitListExpr>(written_ret_value) ||
         llvm::isa<clang::CXXStdInitializerListExpr>(written_ret_value))) {
      SgScopeStatement *scope = SageBuilder::topScopeStack();
      SgFunctionDeclaration *enclosing_function =
          SageInterface::getEnclosingFunctionDeclaration(scope);
      SgFunctionType *function_type = enclosing_function != nullptr
                                          ? enclosing_function->get_type()
                                          : nullptr;
      SgType *ret_type =
          function_type != nullptr ? function_type->get_return_type() : nullptr;
      if (ret_type == nullptr || SageInterface::containsUnknownType(ret_type)) {
        std::cerr << "REX_FRONTEND_INVARIANT[return-destination-type]: "
                     "braced return has no exact declared function result type"
                  << std::endl;
        ROSE_ABORT();
      }
      SgBracedInitializer *braced_return =
          SageBuilder::buildBracedInitializer_nfi(expr_list, ret_type);
      applySourceRange(braced_return, written_ret_value->getSourceRange());
      expr = braced_return;
    }
  }
  if (expr == nullptr) {
    expr = SageBuilder::buildNullExpression_nfi(
        SgNullExpression::e_null_expression_syntactic_absence);
    SageBuilder::initializeSemanticExpressionSourceProvenance(expr);
  }
  const auto *candidate = return_stmt->getNRVOCandidate();
  SgInitializedName *nrvo_candidate = nullptr;
  if (candidate != nullptr) {
    nrvo_candidate = exactInitializedNameFromTranslatedVariableNode(
        Traverse(const_cast<clang::VarDecl *>(candidate)));
    ASSERT_not_null(nrvo_candidate);
  }
  *node = SageBuilder::buildReturnStmt_nfi(expr, nrvo_candidate);

  return VisitStmt(return_stmt, node);
}

bool ClangToSageTranslator::VisitSEHExceptStmt(
    clang::SEHExceptStmt *seh_except_stmt, SgNode **node) {
  rejectUnimplementedStatement(seh_except_stmt);
}

bool ClangToSageTranslator::VisitSEHFinallyStmt(
    clang::SEHFinallyStmt *seh_finally_stmt, SgNode **node) {
  rejectUnimplementedStatement(seh_finally_stmt);
}

bool ClangToSageTranslator::VisitSEHLeaveStmt(
    clang::SEHLeaveStmt *seh_leave_stmt, SgNode **node) {
  rejectUnimplementedStatement(seh_leave_stmt);
}

bool ClangToSageTranslator::VisitSEHTryStmt(clang::SEHTryStmt *seh_try_stmt,
                                            SgNode **node) {
  rejectUnimplementedStatement(seh_try_stmt);
}

bool ClangToSageTranslator::VisitSwitchCase(clang::SwitchCase *switch_case,
                                            SgNode **node) {
  return VisitStmt(switch_case, node);
}

bool ClangToSageTranslator::VisitCaseStmt(clang::CaseStmt *case_stmt,
                                          SgNode **node) {
  SgNode *tmp_lhs = Traverse(case_stmt->getLHS());
  SgExpression *lhs = isSgExpression(tmp_lhs);
  if (lhs == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[case-value]: Clang case value "
                 "translated to "
              << (tmp_lhs == nullptr ? std::string("<null>")
                                     : tmp_lhs->class_name())
              << " instead of SgExpression" << std::endl;
    ROSE_ABORT();
  }

  SgExpression *rhs = nullptr;
  if (case_stmt->getRHS() != nullptr) {
    SgNode *tmp_rhs = Traverse(case_stmt->getRHS());
    rhs = isSgExpression(tmp_rhs);
    if (rhs == nullptr) {
      std::cerr << "REX_FRONTEND_INVARIANT[case-range-end]: Clang case range "
                   "end translated to "
                << (tmp_rhs == nullptr ? std::string("<null>")
                                       : tmp_rhs->class_name())
                << " instead of SgExpression" << std::endl;
      ROSE_ABORT();
    }
  }

  SgCaseOptionStmt *caseOptionStmt =
      SageBuilder::buildCaseOptionStmt_nfi(lhs, nullptr);
  caseOptionStmt->set_parent(
      statementConstructionOwner(case_stmt, "case statement"));

  if (rhs != nullptr) {
    caseOptionStmt->set_key_range_end(rhs);
    rhs->set_parent(caseOptionStmt);
  }

  clang::Stmt *clang_body = case_stmt->getSubStmt();
  SgStatement *stmt =
      translateTypedStatementChild(clang_body, caseOptionStmt, "case body");
  stmt = wrapStatementWithPragmas(clang_body, stmt);
  if (stmt->get_parent() != nullptr && stmt->get_parent() != caseOptionStmt) {
    std::cerr << "REX_FRONTEND_INVARIANT[case-body-owner]: translated body="
              << static_cast<void *>(stmt) << "/" << stmt->class_name()
              << " already has owner="
              << static_cast<void *>(stmt->get_parent()) << std::endl;
    ROSE_ABORT();
  }
  caseOptionStmt->set_body(stmt);
  if (stmt->get_parent() == nullptr)
    stmt->set_parent(caseOptionStmt);

  *node = caseOptionStmt;

  return VisitSwitchCase(case_stmt, node);
}

bool ClangToSageTranslator::VisitDefaultStmt(clang::DefaultStmt *default_stmt,
                                             SgNode **node) {
  SgDefaultOptionStmt *sage_default =
      SageBuilder::buildDefaultOptionStmt_nfi(nullptr);
  sage_default->set_parent(
      statementConstructionOwner(default_stmt, "default statement"));
  clang::Stmt *clang_body = default_stmt->getSubStmt();
  SgStatement *stmt =
      translateTypedStatementChild(clang_body, sage_default, "default body");
  stmt = wrapStatementWithPragmas(clang_body, stmt);
  if (stmt->get_parent() != nullptr && stmt->get_parent() != sage_default) {
    std::cerr << "REX_FRONTEND_INVARIANT[default-body-owner]: translated body="
              << static_cast<void *>(stmt) << "/" << stmt->class_name()
              << " already has owner="
              << static_cast<void *>(stmt->get_parent()) << std::endl;
    ROSE_ABORT();
  }
  sage_default->set_body(stmt);
  if (stmt->get_parent() == nullptr)
    stmt->set_parent(sage_default);

  *node = sage_default;

  return VisitSwitchCase(default_stmt, node);
}

bool ClangToSageTranslator::VisitSwitchStmt(clang::SwitchStmt *switch_stmt,
                                            SgNode **node) {
  clang::Stmt *clang_body = switch_stmt->getBody();
  if (clang_body == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[switch-body]: Clang SwitchStmt has no "
                 "body"
              << std::endl;
    ROSE_ABORT();
  }
  clang::Expr *clang_cond =
      llvm::dyn_cast_or_null<clang::Expr>(switch_stmt->getCond());
  if (switch_stmt->getConditionVariableDeclStmt() == nullptr &&
      clang_cond == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[switch-condition-expression]: "
                 "Clang SwitchStmt has no condition"
              << std::endl;
    ROSE_ABORT();
  }
  clang::Stmt *clang_switch_init_stmt = switch_stmt->getInit();
  // SgSwitchStatement has no dedicated child for a C++17 init-statement.
  // Lower it into an enclosing block so the translated AST remains
  // structurally valid while preserving the initializer's scope for the switch
  // body.
  SgBasicBlock *switch_init_wrapper = nullptr;
  SgNode *switch_outer_owner =
      statementConstructionOwner(switch_stmt, "switch statement");
  if (clang_switch_init_stmt != nullptr) {
    clang::SourceLocation init_begin = clang_switch_init_stmt->getBeginLoc();
    clang::SourceLocation switch_end = switch_stmt->getEndLoc();
    if (!init_begin.isValid() || !switch_end.isValid()) {
      std::cerr << "REX_FRONTEND_INVARIANT[switch-init-source]: "
                   "source-written switch-init has no valid Clang interval"
                << std::endl;
      ROSE_ABORT();
    }
    switch_init_wrapper = SageBuilder::buildBasicBlock_nfi();
    switch_init_wrapper->set_parent(switch_outer_owner);
    applySourceRange(switch_init_wrapper,
                     clang::SourceRange(init_begin, switch_end));
    SageBuilder::pushScopeStack(switch_init_wrapper);
  }

  auto require_decl_scope = [](SgStatement *stmt, SgScopeStatement *scope) {
    if (SgDeclarationStatement *decl = isSgDeclarationStatement(stmt)) {
      if (scope == nullptr || decl->get_scope() != scope) {
        std::cerr << "REX_FRONTEND_INVARIANT[switch-init-owner]: declaration "
                     "does not have the exact switch-init wrapper scope"
                  << std::endl;
        ROSE_ABORT();
      }
      if (SgVariableDeclaration *var_decl = isSgVariableDeclaration(decl)) {
        for (SgInitializedName *init_name : var_decl->get_variables()) {
          if (init_name == nullptr || init_name->get_scope() != scope) {
            std::cerr << "REX_FRONTEND_INVARIANT[switch-init-owner]: "
                         "declarator does not have the exact switch-init "
                         "wrapper scope"
                      << std::endl;
            ROSE_ABORT();
          }
        }
      }
    }
  };

  auto translate_wrapper_stmt = [&](clang::Stmt *clang_stmt) {
    SgStatement *sg_stmt = translateStatement(clang_stmt, "switch initializer");
    if (llvm::isa<clang::Expr>(clang_stmt)) {
      switch_init_wrapper->append_statement(sg_stmt);
    } else {
      require_decl_scope(sg_stmt, switch_init_wrapper);
      if (sg_stmt->get_parent() != switch_init_wrapper ||
          std::count(switch_init_wrapper->get_statements().begin(),
                     switch_init_wrapper->get_statements().end(),
                     sg_stmt) != 1) {
        std::cerr << "REX_FRONTEND_INVARIANT[switch-init-owner]: translated "
                     "statement was not published exactly once by its active "
                     "wrapper scope"
                  << std::endl;
        ROSE_ABORT();
      }
    }
  };

  if (clang_switch_init_stmt != nullptr) {
    translate_wrapper_stmt(clang_switch_init_stmt);
  }

  SgSwitchStatement *sg_switch_stmt =
      SageBuilder::buildSwitchStatement_nfi(nullptr, nullptr);
  sg_switch_stmt->set_parent(switch_init_wrapper != nullptr
                                 ? switch_init_wrapper
                                 : switch_outer_owner);
  *node = sg_switch_stmt;
  if (!VisitStmt(switch_stmt, node)) {
    return false;
  }

  SageBuilder::pushScopeStack(sg_switch_stmt);

  SgStatement *cond_stmt = nullptr;
  if (clang::DeclStmt *cond_decl = llvm::dyn_cast_or_null<clang::DeclStmt>(
          switch_stmt->getConditionVariableDeclStmt())) {
    SgNode *tmp_cond = nullptr;
    {
      PendingConditionDeclarationScopeGuard condition_scope(
          p_pending_condition_declaration_scope_stack, cond_decl,
          sg_switch_stmt);
      tmp_cond = Traverse(cond_decl);
    }
    cond_stmt = isSgStatement(tmp_cond);
    if (cond_stmt == nullptr) {
      std::cerr << "REX_FRONTEND_INVARIANT[switch-condition-declaration]: "
                   "Clang condition translated to "
                << (tmp_cond == nullptr ? std::string("<null>")
                                        : tmp_cond->class_name())
                << " instead of SgStatement" << std::endl;
      ROSE_ABORT();
    }
    requireExactConditionDeclarationResult(cond_decl, cond_stmt, sg_switch_stmt,
                                           "switch");
    sg_switch_stmt->set_condition_value(
        buildDeclarationConditionValue(clang_cond, sg_switch_stmt));
  } else {
    SgNode *tmp_cond = Traverse(clang_cond);
    SgExpression *cond = isSgExpression(tmp_cond);
    if (cond == nullptr) {
      std::cerr << "REX_FRONTEND_INVARIANT[switch-condition-expression]: "
                   "Clang condition translated to "
                << (tmp_cond == nullptr ? std::string("<null>")
                                        : tmp_cond->class_name())
                << " instead of SgExpression" << std::endl;
      ROSE_ABORT();
    }
    cond_stmt = SageBuilder::buildExprStatement_nfi(cond);
    applySourceRange(cond_stmt, clang_cond->getSourceRange());
  }

  cond_stmt->set_parent(sg_switch_stmt);
  sg_switch_stmt->set_item_selector(cond_stmt);
  requireExactAttachedConditionDeclaration(cond_stmt, sg_switch_stmt, "switch");

  SgStatement *body = translateControlledSubstatement(
      clang_body, sg_switch_stmt, "switch body");

  SageBuilder::popScopeStack();

  sg_switch_stmt->set_body(body);
  body->set_parent(sg_switch_stmt);

  if (switch_init_wrapper != nullptr) {
    switch_init_wrapper->append_statement(sg_switch_stmt);
    SageBuilder::popScopeStack();
    *node = switch_init_wrapper;
    return true;
  }

  return true;
}

bool ClangToSageTranslator::VisitValueStmt(clang::ValueStmt *value_stmt,
                                           SgNode **node) {
  return VisitStmt(value_stmt, node);
}

bool ClangToSageTranslator::VisitAttributedStmt(
    clang::AttributedStmt *attributed_stmt, SgNode **node) {
  if (attributed_stmt == nullptr || node == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[attributed-statement]: exact Clang "
                 "statement and Sage result slot are required"
              << std::endl;
    ROSE_ABORT();
  }

  const auto &clang_attributes = attributed_stmt->getAttrs();
  if (clang_attributes.empty()) {
    std::cerr << "REX_FRONTEND_INVARIANT[attributed-statement]: Clang "
                 "AttributedStmt has no attributes"
              << std::endl;
    ROSE_ABORT();
  }

  clang::Stmt *sub_stmt = attributed_stmt->getSubStmt();
  if (sub_stmt == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[attributed-statement]: Clang "
                 "AttributedStmt has no wrapped statement"
              << std::endl;
    ROSE_ABORT();
  }
  SgStatement *sage_sub_statement =
      translateStatement(sub_stmt, "attributed statement body");

  // Several statement builders publish the current scope as a provisional
  // parent before VisitCompoundStmt installs the translated statement in any
  // structural child slot.  An attributed statement is the structural owner,
  // so transfer only that exact uncommitted scope parent.  Never steal a node
  // that an AST edge already owns.
  if (SgNode *provisional_parent = sage_sub_statement->get_parent()) {
    SgScopeStatement *current_scope = SageBuilder::topScopeStack();
    bool is_structural_child = false;
    for (SgNode *successor :
         provisional_parent->get_traversalSuccessorContainer()) {
      if (successor == sage_sub_statement) {
        is_structural_child = true;
        break;
      }
    }
    if (provisional_parent != current_scope || is_structural_child) {
      std::cerr << "REX_FRONTEND_INVARIANT[attributed-statement-owner]: "
                   "wrapped statement="
                << static_cast<void *>(sage_sub_statement)
                << " has non-transferable owner="
                << static_cast<void *>(provisional_parent)
                << " current-scope=" << static_cast<void *>(current_scope)
                << " structural-child=" << is_structural_child << std::endl;
      ROSE_ABORT();
    }
    sage_sub_statement->set_parent(nullptr);
  }

  SgAttributedStatement *sage_statement =
      new SgAttributedStatement(sage_sub_statement);
  ROSE_ASSERT(sage_statement != nullptr);

  auto translate_attribute_expression =
      [&](clang::Expr *expression,
          const char *attribute_name) -> SgExpression * {
    if (expression == nullptr || attribute_name == nullptr) {
      std::cerr << "REX_FRONTEND_INVARIANT[statement-attribute-expression]: "
                   "attribute="
                << (attribute_name != nullptr ? attribute_name : "<null>")
                << " has no exact Clang expression" << std::endl;
      ROSE_ABORT();
    }
    SgExpression *sage_expression = isSgExpression(Traverse(expression));
    if (sage_expression == nullptr ||
        sage_expression->get_parent() != nullptr) {
      std::cerr << "REX_FRONTEND_INVARIANT[statement-attribute-expression]: "
                   "attribute="
                << attribute_name << " translated expression="
                << static_cast<void *>(sage_expression) << " owner="
                << static_cast<void *>(sage_expression != nullptr
                                           ? sage_expression->get_parent()
                                           : nullptr)
                << std::endl;
      ROSE_ABORT();
    }
    return sage_expression;
  };

  using SageAttribute = SgStatementAttribute;
  using SageKind = SageAttribute::statement_attribute_kind_enum;
  using SageSpelling = SageAttribute::statement_attribute_spelling_enum;
  using SageLoopOption = SageAttribute::loop_hint_option_enum;
  using SageLoopState = SageAttribute::loop_hint_state_enum;

  auto make_attribute = [&](SageKind kind, SageSpelling spelling,
                            SgExpression *expression, unsigned long integer,
                            SageLoopOption option,
                            SageLoopState state) -> SgStatementAttribute * {
    ROSE_ASSERT(SageAttribute::fields_are_valid(
        kind, spelling, expression != nullptr, integer, option, state));
    SgStatementAttribute *result = new SgStatementAttribute(
        kind, spelling, expression, integer, option, state);
    ROSE_ASSERT(result != nullptr);
    result->validate();
    return result;
  };

  auto loop_option =
      [](clang::LoopHintAttr::OptionType option) -> SageLoopOption {
    switch (option) {
    case clang::LoopHintAttr::Vectorize:
      return SageAttribute::e_loop_hint_option_vectorize;
    case clang::LoopHintAttr::VectorizeWidth:
      return SageAttribute::e_loop_hint_option_vectorize_width;
    case clang::LoopHintAttr::Interleave:
      return SageAttribute::e_loop_hint_option_interleave;
    case clang::LoopHintAttr::InterleaveCount:
      return SageAttribute::e_loop_hint_option_interleave_count;
    case clang::LoopHintAttr::Unroll:
      return SageAttribute::e_loop_hint_option_unroll;
    case clang::LoopHintAttr::UnrollCount:
      return SageAttribute::e_loop_hint_option_unroll_count;
    case clang::LoopHintAttr::UnrollAndJam:
      return SageAttribute::e_loop_hint_option_unroll_and_jam;
    case clang::LoopHintAttr::UnrollAndJamCount:
      return SageAttribute::e_loop_hint_option_unroll_and_jam_count;
    case clang::LoopHintAttr::PipelineDisabled:
      return SageAttribute::e_loop_hint_option_pipeline_disabled;
    case clang::LoopHintAttr::PipelineInitiationInterval:
      return SageAttribute::e_loop_hint_option_pipeline_initiation_interval;
    case clang::LoopHintAttr::Distribute:
      return SageAttribute::e_loop_hint_option_distribute;
    case clang::LoopHintAttr::VectorizePredicate:
      return SageAttribute::e_loop_hint_option_vectorize_predicate;
    }
    std::cerr << "REX_FRONTEND_INVARIANT[statement-loop-hint]: unsupported "
                 "Clang loop option="
              << static_cast<int>(option) << std::endl;
    ROSE_ABORT();
  };

  auto loop_state =
      [](clang::LoopHintAttr::LoopHintState state) -> SageLoopState {
    switch (state) {
    case clang::LoopHintAttr::Enable:
      return SageAttribute::e_loop_hint_state_enable;
    case clang::LoopHintAttr::Disable:
      return SageAttribute::e_loop_hint_state_disable;
    case clang::LoopHintAttr::Numeric:
      return SageAttribute::e_loop_hint_state_numeric;
    case clang::LoopHintAttr::FixedWidth:
      return SageAttribute::e_loop_hint_state_fixed_width;
    case clang::LoopHintAttr::ScalableWidth:
      return SageAttribute::e_loop_hint_state_scalable_width;
    case clang::LoopHintAttr::AssumeSafety:
      return SageAttribute::e_loop_hint_state_assume_safety;
    case clang::LoopHintAttr::Full:
      return SageAttribute::e_loop_hint_state_full;
    }
    std::cerr << "REX_FRONTEND_INVARIANT[statement-loop-hint]: unsupported "
                 "Clang loop state="
              << static_cast<int>(state) << std::endl;
    ROSE_ABORT();
  };

  for (const clang::Attr *clang_attribute : clang_attributes) {

    if (clang_attribute == nullptr) {
      std::cerr << "REX_FRONTEND_INVARIANT[statement-attribute-explicit]: "
                   "AttributedStmt contains a null attribute"
                << std::endl;
      ROSE_ABORT();
    }

    // Clang marks source-written loop pragmas implicit even though each one
    // owns an exact AS_Pragma spelling and a valid source range.  That flag is
    // an implementation detail of pragma lowering, not an absent source
    // attribute.  No other implicit statement attribute is accepted.
    const bool source_written_loop_pragma =
        llvm::isa<clang::LoopHintAttr>(clang_attribute) &&
        clang_attribute->getSyntax() == clang::AttributeCommonInfo::AS_Pragma &&
        clang_attribute->getRange().isValid();
    if ((clang_attribute->isImplicit() && !source_written_loop_pragma) ||
        clang_attribute->isInherited() || clang_attribute->isPackExpansion() ||
        !clang_attribute->getRange().isValid() ||
        clang_attribute->getSyntax() ==
            clang::AttributeCommonInfo::AS_Implicit) {
      std::cerr << "REX_FRONTEND_INVARIANT[statement-attribute-explicit]: "
                   "AttributedStmt contains null, implicit, inherited, "
                   "pack-expanded, or source-less attribute kind="
                << static_cast<int>(clang_attribute->getKind()) << std::endl;
      ROSE_ABORT();
    }

    const unsigned spelling = clang_attribute->getSpellingListIndex();
    SgStatementAttribute *sage_attribute = nullptr;

    if (llvm::isa<clang::FallThroughAttr>(clang_attribute)) {
      SageSpelling exact_spelling;
      switch (spelling) {
      case clang::FallThroughAttr::CXX11_fallthrough:
        exact_spelling =
            SageAttribute::e_statement_attribute_spelling_cxx11_unscoped;
        break;
      case clang::FallThroughAttr::C23_fallthrough:
        exact_spelling =
            SageAttribute::e_statement_attribute_spelling_c23_unscoped;
        break;
      case clang::FallThroughAttr::CXX11_clang_fallthrough:
        exact_spelling =
            SageAttribute::e_statement_attribute_spelling_cxx11_clang;
        break;
      case clang::FallThroughAttr::GNU_fallthrough:
        exact_spelling = SageAttribute::e_statement_attribute_spelling_gnu;
        break;
      case clang::FallThroughAttr::CXX11_gnu_fallthrough:
        exact_spelling =
            SageAttribute::e_statement_attribute_spelling_cxx11_gnu;
        break;
      case clang::FallThroughAttr::C23_gnu_fallthrough:
        exact_spelling = SageAttribute::e_statement_attribute_spelling_c23_gnu;
        break;
      default:
        std::cerr << "REX_FRONTEND_INVARIANT[statement-attribute-spelling]: "
                     "fallthrough spelling="
                  << spelling << std::endl;
        ROSE_ABORT();
      }
      sage_attribute = make_attribute(
          SageAttribute::e_statement_attribute_fallthrough, exact_spelling,
          nullptr, 0, SageAttribute::e_loop_hint_option_none,
          SageAttribute::e_loop_hint_state_none);
    } else if (llvm::isa<clang::LikelyAttr>(clang_attribute)) {
      SageSpelling exact_spelling;
      switch (spelling) {
      case clang::LikelyAttr::CXX11_likely:
        exact_spelling =
            SageAttribute::e_statement_attribute_spelling_cxx11_unscoped;
        break;
      case clang::LikelyAttr::C23_clang_likely:
        exact_spelling =
            SageAttribute::e_statement_attribute_spelling_c23_clang;
        break;
      default:
        std::cerr << "REX_FRONTEND_INVARIANT[statement-attribute-spelling]: "
                     "likely spelling="
                  << spelling << std::endl;
        ROSE_ABORT();
      }
      sage_attribute = make_attribute(
          SageAttribute::e_statement_attribute_likely, exact_spelling, nullptr,
          0, SageAttribute::e_loop_hint_option_none,
          SageAttribute::e_loop_hint_state_none);
    } else if (llvm::isa<clang::UnlikelyAttr>(clang_attribute)) {
      SageSpelling exact_spelling;
      switch (spelling) {
      case clang::UnlikelyAttr::CXX11_unlikely:
        exact_spelling =
            SageAttribute::e_statement_attribute_spelling_cxx11_unscoped;
        break;
      case clang::UnlikelyAttr::C23_clang_unlikely:
        exact_spelling =
            SageAttribute::e_statement_attribute_spelling_c23_clang;
        break;
      default:
        std::cerr << "REX_FRONTEND_INVARIANT[statement-attribute-spelling]: "
                     "unlikely spelling="
                  << spelling << std::endl;
        ROSE_ABORT();
      }
      sage_attribute = make_attribute(
          SageAttribute::e_statement_attribute_unlikely, exact_spelling,
          nullptr, 0, SageAttribute::e_loop_hint_option_none,
          SageAttribute::e_loop_hint_state_none);
    } else if (const auto *assume =
                   llvm::dyn_cast<clang::CXXAssumeAttr>(clang_attribute)) {
      SageSpelling exact_spelling;
      switch (spelling) {
      case clang::CXXAssumeAttr::CXX11_assume:
        exact_spelling =
            SageAttribute::e_statement_attribute_spelling_cxx11_unscoped;
        break;
      case clang::CXXAssumeAttr::GNU_assume:
        exact_spelling = SageAttribute::e_statement_attribute_spelling_gnu;
        break;
      case clang::CXXAssumeAttr::CXX11_clang_assume:
        exact_spelling =
            SageAttribute::e_statement_attribute_spelling_cxx11_clang;
        break;
      case clang::CXXAssumeAttr::C23_clang_assume:
        exact_spelling =
            SageAttribute::e_statement_attribute_spelling_c23_clang;
        break;
      default:
        std::cerr << "REX_FRONTEND_INVARIANT[statement-attribute-spelling]: "
                     "assume spelling="
                  << spelling << std::endl;
        ROSE_ABORT();
      }
      SgExpression *argument =
          translate_attribute_expression(assume->getAssumption(), "assume");
      sage_attribute = make_attribute(
          SageAttribute::e_statement_attribute_assume, exact_spelling, argument,
          0, SageAttribute::e_loop_hint_option_none,
          SageAttribute::e_loop_hint_state_none);
    } else if (llvm::isa<clang::NoMergeAttr>(clang_attribute)) {
      SageSpelling exact_spelling;
      switch (spelling) {
      case clang::NoMergeAttr::GNU_nomerge:
        exact_spelling = SageAttribute::e_statement_attribute_spelling_gnu;
        break;
      case clang::NoMergeAttr::CXX11_clang_nomerge:
        exact_spelling =
            SageAttribute::e_statement_attribute_spelling_cxx11_clang;
        break;
      case clang::NoMergeAttr::C23_clang_nomerge:
        exact_spelling =
            SageAttribute::e_statement_attribute_spelling_c23_clang;
        break;
      default:
        std::cerr << "REX_FRONTEND_INVARIANT[statement-attribute-spelling]: "
                     "nomerge spelling="
                  << spelling << std::endl;
        ROSE_ABORT();
      }
      sage_attribute = make_attribute(
          SageAttribute::e_statement_attribute_nomerge, exact_spelling, nullptr,
          0, SageAttribute::e_loop_hint_option_none,
          SageAttribute::e_loop_hint_state_none);
    } else if (llvm::isa<clang::MustTailAttr>(clang_attribute)) {
      SageSpelling exact_spelling;
      switch (spelling) {
      case clang::MustTailAttr::GNU_musttail:
        exact_spelling = SageAttribute::e_statement_attribute_spelling_gnu;
        break;
      case clang::MustTailAttr::CXX11_clang_musttail:
        exact_spelling =
            SageAttribute::e_statement_attribute_spelling_cxx11_clang;
        break;
      case clang::MustTailAttr::C23_clang_musttail:
        exact_spelling =
            SageAttribute::e_statement_attribute_spelling_c23_clang;
        break;
      default:
        std::cerr << "REX_FRONTEND_INVARIANT[statement-attribute-spelling]: "
                     "musttail spelling="
                  << spelling << std::endl;
        ROSE_ABORT();
      }
      sage_attribute = make_attribute(
          SageAttribute::e_statement_attribute_musttail, exact_spelling,
          nullptr, 0, SageAttribute::e_loop_hint_option_none,
          SageAttribute::e_loop_hint_state_none);
    } else if (llvm::isa<clang::AlwaysInlineAttr>(clang_attribute)) {
      SageSpelling exact_spelling;
      switch (spelling) {
      case clang::AlwaysInlineAttr::GNU_always_inline:
        exact_spelling = SageAttribute::e_statement_attribute_spelling_gnu;
        break;
      case clang::AlwaysInlineAttr::CXX11_gnu_always_inline:
        exact_spelling =
            SageAttribute::e_statement_attribute_spelling_cxx11_gnu;
        break;
      case clang::AlwaysInlineAttr::C23_gnu_always_inline:
        exact_spelling = SageAttribute::e_statement_attribute_spelling_c23_gnu;
        break;
      case clang::AlwaysInlineAttr::CXX11_clang_always_inline:
        exact_spelling =
            SageAttribute::e_statement_attribute_spelling_cxx11_clang;
        break;
      case clang::AlwaysInlineAttr::C23_clang_always_inline:
        exact_spelling =
            SageAttribute::e_statement_attribute_spelling_c23_clang;
        break;
      default:
        std::cerr << "REX_FRONTEND_INVARIANT[statement-attribute-spelling]: "
                     "always_inline statement spelling="
                  << spelling << std::endl;
        ROSE_ABORT();
      }
      sage_attribute = make_attribute(
          SageAttribute::e_statement_attribute_always_inline, exact_spelling,
          nullptr, 0, SageAttribute::e_loop_hint_option_none,
          SageAttribute::e_loop_hint_state_none);
    } else if (const auto *unroll = llvm::dyn_cast<clang::OpenCLUnrollHintAttr>(
                   clang_attribute)) {
      if (clang_attribute->getSyntax() != clang::AttributeCommonInfo::AS_GNU) {
        std::cerr << "REX_FRONTEND_INVARIANT[statement-attribute-spelling]: "
                     "OpenCL unroll hint is not GNU syntax"
                  << std::endl;
        ROSE_ABORT();
      }
      sage_attribute = make_attribute(
          SageAttribute::e_statement_attribute_opencl_unroll_hint,
          SageAttribute::e_statement_attribute_spelling_gnu, nullptr,
          unroll->getUnrollHint(), SageAttribute::e_loop_hint_option_none,
          SageAttribute::e_loop_hint_state_none);
    } else if (const auto *loop =
                   llvm::dyn_cast<clang::LoopHintAttr>(clang_attribute)) {
      SageSpelling exact_spelling;
      switch (spelling) {
      case clang::LoopHintAttr::Pragma_clang_loop:
        exact_spelling =
            SageAttribute::e_statement_attribute_spelling_pragma_clang_loop;
        break;
      case clang::LoopHintAttr::Pragma_unroll:
        exact_spelling =
            SageAttribute::e_statement_attribute_spelling_pragma_unroll;
        break;
      case clang::LoopHintAttr::Pragma_nounroll:
        exact_spelling =
            SageAttribute::e_statement_attribute_spelling_pragma_nounroll;
        break;
      case clang::LoopHintAttr::Pragma_unroll_and_jam:
        exact_spelling =
            SageAttribute::e_statement_attribute_spelling_pragma_unroll_and_jam;
        break;
      case clang::LoopHintAttr::Pragma_nounroll_and_jam:
        exact_spelling = SageAttribute::
            e_statement_attribute_spelling_pragma_nounroll_and_jam;
        break;
      default:
        std::cerr << "REX_FRONTEND_INVARIANT[statement-attribute-spelling]: "
                     "loop hint spelling="
                  << spelling << std::endl;
        ROSE_ABORT();
      }
      SgExpression *argument =
          loop->getValue() != nullptr
              ? translate_attribute_expression(loop->getValue(), "loop-hint")
              : nullptr;
      sage_attribute = make_attribute(
          SageAttribute::e_statement_attribute_loop_hint, exact_spelling,
          argument, 0, loop_option(loop->getOption()),
          loop_state(loop->getState()));
    } else {
      std::cerr << "REX_FRONTEND_INVARIANT[statement-attribute-kind]: "
                   "unsupported explicit Clang statement attribute kind="
                << static_cast<int>(clang_attribute->getKind()) << std::endl;
      ROSE_ABORT();
    }

    ROSE_ASSERT(sage_attribute != nullptr);
    const clang::SourceRange attribute_range = clang_attribute->getRange();
    applySourceRange(sage_attribute, attribute_range);
    sage_statement->append_attribute(sage_attribute);
  }

  const clang::SourceRange first_attribute_range =
      clang_attributes.front()->getRange();
  const clang::SourceRange last_attribute_range =
      clang_attributes.back()->getRange();
  applySourceRange(sage_statement->get_attribute_list(),
                   clang::SourceRange(first_attribute_range.getBegin(),
                                      last_attribute_range.getEnd()));
  sage_statement->validate();
  *node = sage_statement;
  if (!VisitStmt(attributed_stmt, node)) {
    return false;
  }
  const size_t attributed_source_roles =
      p_lexical_source_nodes.count(sage_statement) +
      p_synthesized_source_nodes.count(sage_statement);
  if (attributed_source_roles != 1) {
    std::cerr << "REX_FRONTEND_INVARIANT[attributed-statement-provenance]: "
                 "wrapped statement did not retain one source/generated role"
              << std::endl;
    ROSE_ABORT();
  }
  return true;
}

SgExpression *ClangToSageTranslator::wrapMacroExpansionExpression(
    clang::Expr *clang_expression, SgExpression *expanded_expression) {
  if (clang_expression == nullptr || expanded_expression == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[macro-expansion-expression]: exact "
                 "Clang and Sage expressions are required"
              << std::endl;
    ROSE_ABORT();
  }
  if (p_synthesized_source_nodes.count(expanded_expression) != 0) {
    // The producer classified this semantic wrapper before VisitExpr. Its
    // source-spelled child, not the wrapper, owns any macro invocation.
    return expanded_expression;
  }
  if (p_exact_literal_operand_spellings.count(clang_expression) != 0) {
    // A cooked scalar component shares its enclosing imaginary/UDL token.
    // That enclosing expression owns the complete macro invocation; placing
    // it on this component would change the component's scalar semantics.
    return expanded_expression;
  }
  const bool semantic_expression_construction =
      p_semantic_function_body_traversal_depth != 0 ||
      p_semantic_template_argument_expression_depth != 0 ||
      p_semantic_expression_subtree_depth != 0 ||
      p_user_defined_literal_semantic_expression_depth != 0 ||
      (!p_imported_external_semantic_traversal_stack.empty() &&
       p_imported_external_semantic_traversal_stack.back());
  if (semantic_expression_construction) {
    // Clang retains pattern source locations on instantiated bodies and other
    // semantic expression trees.  Those locations are construction evidence,
    // not a second lexical occurrence: applySourceRange publishes the whole
    // tree as generated semantic state after VisitExpr returns.
    return expanded_expression;
  }
  if (llvm::isa<clang::FullExpr, clang::CXXBindTemporaryExpr,
                clang::MaterializeTemporaryExpr>(clang_expression)) {
    // These Clang nodes are semantic-only boundaries around the written
    // child. Their typed role is recorded on SgExpression; the child remains
    // the sole owner of any enclosing macro invocation surface.
    return expanded_expression;
  }
  clang::SourceRange source_range = expressionSourceRange(
      *clang_expression, p_compiler_instance->getASTContext());
  if (!source_range.isValid()) {
    // Clang intentionally gives some implicit expression wrappers no written
    // range. Such a node cannot own a source macro invocation.
    return expanded_expression;
  }

  clang::SourceLocation begin = source_range.getBegin();
  clang::SourceLocation end = source_range.getEnd();
  if (!begin.isMacroID() && !end.isMacroID()) {
    return expanded_expression;
  }

  clang::SourceManager &sm = p_compiler_instance->getSourceManager();
  if (!((begin.isMacroID() && sm.isMacroBodyExpansion(begin)) ||
        (end.isMacroID() && sm.isMacroBodyExpansion(end)))) {
    // A macro-argument expansion alone is not a macro invocation surface.
    return expanded_expression;
  }
  const clang::LangOptions &lang_opts = p_compiler_instance->getLangOpts();
  auto ends_macro_expression_surface = [&](clang::SourceLocation end) {
    if (!end.isMacroID()) {
      return true;
    }
    if (clang::Lexer::isAtEndOfMacroExpansion(end, sm, lang_opts)) {
      return true;
    }

    // A legacy expression macro can own a trailing semicolon, which Clang
    // excludes from the Expr source range. Accept that exact non-expression
    // tail, but no other replacement tokens.
    clang::FileID expansion_file = sm.getFileID(end);
    const unsigned expansion_size = sm.getFileIDSize(expansion_file);
    const unsigned expression_offset = sm.getDecomposedLoc(end).second;
    const unsigned token_length =
        clang::Lexer::MeasureTokenLength(sm.getSpellingLoc(end), sm, lang_opts);
    if (token_length == 0 ||
        expression_offset + token_length > expansion_size) {
      std::cerr << "REX_FRONTEND_INVARIANT[macro-expansion-expression]: "
                   "cannot determine the exact macro replacement tail"
                << std::endl;
      ROSE_ABORT();
    }
    for (unsigned offset = expression_offset + token_length;
         offset < expansion_size; ++offset) {
      clang::SourceLocation tail_loc =
          end.getLocWithOffset(static_cast<int>(offset - expression_offset));
      bool invalid = false;
      const char *tail =
          sm.getCharacterData(sm.getImmediateSpellingLoc(tail_loc), &invalid);
      if (invalid || tail == nullptr) {
        std::cerr << "REX_FRONTEND_INVARIANT[macro-expansion-expression]: "
                     "macro replacement tail has no exact spelling"
                  << std::endl;
        ROSE_ABORT();
      }
      const unsigned char ch = static_cast<unsigned char>(*tail);
      if (ch != ';' && std::isspace(ch) == 0) {
        return false;
      }
    }
    return true;
  };
  if ((begin.isMacroID() &&
       !clang::Lexer::isAtStartOfMacroExpansion(begin, sm, lang_opts)) ||
      !ends_macro_expression_surface(end)) {
    // This expression is a semantic fragment inside a larger macro
    // replacement. The enclosing expression owns the invocation surface.
    return expanded_expression;
  }

  if (clang::CallExpr *call =
          llvm::dyn_cast<clang::CallExpr>(clang_expression)) {
    if (call->getRParenLoc().isInvalid()) {
      // An implicit conversion operator call has no written call surface.
      return expanded_expression;
    }
  }
  if (llvm::isa<clang::ImplicitCastExpr>(clang_expression)) {
    // An implicit cast is a typed semantic wrapper around its source-spelled
    // child. The child, not the cast, owns any enclosing macro invocation.
    return expanded_expression;
  }

  clang::CharSourceRange expansion_range = sm.getExpansionRange(source_range);
  const auto invocation_range =
      sm.getExpansionRange(begin.isMacroID() ? begin : end);
  const bool exact_invocation =
      expansion_range.getBegin() == invocation_range.getBegin() &&
      expansion_range.getEnd() == invocation_range.getEnd() &&
      expansion_range.isTokenRange() == invocation_range.isTokenRange();
  auto child_owns_invocation = [&](clang::SourceLocation boundary) {
    if (!boundary.isMacroID() || !sm.isMacroBodyExpansion(boundary))
      return true;
    const auto invocation = sm.getExpansionRange(boundary);
    for (const clang::Stmt *child : clang_expression->children()) {
      const auto *expression = llvm::dyn_cast_or_null<clang::Expr>(child);
      if (expression == nullptr)
        continue;
      const auto range = expressionSourceRange(
          *expression, p_compiler_instance->getASTContext());
      if (range.isInvalid() ||
          (range.getBegin().isMacroID() &&
           !clang::Lexer::isAtStartOfMacroExpansion(range.getBegin(), sm,
                                                    lang_opts)) ||
          !ends_macro_expression_surface(range.getEnd()))
        continue;
      const auto surface = sm.getExpansionRange(range);
      if (sm.getFileID(surface.getBegin()) ==
              sm.getFileID(invocation.getBegin()) &&
          sm.getFileID(surface.getEnd()) == sm.getFileID(invocation.getEnd()) &&
          sm.getFileOffset(surface.getBegin()) <=
              sm.getFileOffset(invocation.getBegin()) &&
          sm.getFileOffset(surface.getEnd()) >=
              sm.getFileOffset(invocation.getEnd()))
        return true;
    }
    return false;
  };
  if (!exact_invocation && child_owns_invocation(begin) &&
      child_owns_invocation(end)) {
    // The immediate children already cover both boundary invocations without
    // cutting a replacement token sequence. Do not add an ancestor wrapper
    // for MACRO * 2. A fragment such as object.member, where member expands to
    // embedded.member, instead needs this minimal enclosing expression: its
    // inner MemberExpr ends before the replacement's final token.
    return expanded_expression;
  }
  begin = expansion_range.getBegin();
  end = expansion_range.getEnd();
  if (!expansion_range.isValid() || !begin.isValid() || !end.isValid() ||
      !begin.isFileID() || !end.isFileID()) {
    std::cerr << "REX_FRONTEND_INVARIANT[macro-expansion-expression]: Clang "
              << clang_expression->getStmtClassName()
              << " does not map to an exact file spelling range" << std::endl;
    ROSE_ABORT();
  }

  clang::FileID begin_file = sm.getFileID(begin);
  clang::FileID end_file = sm.getFileID(end);
  if (begin_file.isInvalid() || end_file.isInvalid() ||
      begin_file != end_file ||
      sm.getFileOffset(begin) > sm.getFileOffset(end)) {
    std::cerr << "REX_FRONTEND_INVARIANT[macro-expansion-expression]: Clang "
              << clang_expression->getStmtClassName()
              << " expansion does not own one ordered file range" << std::endl;
    ROSE_ABORT();
  }

  bool invalid = false;
  llvm::StringRef source_text =
      clang::Lexer::getSourceText(expansion_range, sm, lang_opts, &invalid);
  std::string spelling =
      invalid ? std::string() : trimWhitespace(source_text.str());
  if (spelling.empty()) {
    std::cerr << "REX_FRONTEND_INVARIANT[macro-expansion-expression]: Clang "
              << clang_expression->getStmtClassName()
              << " expansion has no exact source spelling" << std::endl;
    ROSE_ABORT();
  }

  if (SgMacroExpansionExp *macro = isSgMacroExpansionExp(expanded_expression)) {
    macro->get_expanded_expression_checked();
    if (macro->get_spelling() != spelling) {
      std::cerr << "REX_FRONTEND_INVARIANT[macro-expansion-expression]: "
                   "transparent Clang wrapper disagrees with the existing "
                   "macro expression"
                << std::endl;
      ROSE_ABORT();
    }
    return macro;
  }

  expanded_expression->set_lvalue(clang_expression->isLValue());
  if (p_lexical_source_nodes.count(expanded_expression) == 0) {
    applySourceRange(expanded_expression, source_range);
  }
  if (p_lexical_source_nodes.count(expanded_expression) != 1 ||
      p_synthesized_source_nodes.count(expanded_expression) != 0) {
    std::cerr << "REX_FRONTEND_INVARIANT[macro-expansion-expression]: "
                 "expanded expression="
              << static_cast<void *>(expanded_expression) << "/"
              << expanded_expression->class_name()
              << " did not publish one exact lexical source role" << std::endl;
    ROSE_ABORT();
  }
  for (Sg_File_Info *file_info :
       {expanded_expression->get_file_info(),
        expanded_expression->get_startOfConstruct(),
        expanded_expression->get_endOfConstruct(),
        expanded_expression->get_operatorPosition()}) {
    if (file_info == nullptr || file_info->get_line() <= 0 ||
        file_info->get_col() <= 0 || file_info->get_physical_file_id() < 0 ||
        file_info->isCompilerGenerated() || file_info->isFrontendSpecific() ||
        file_info->isSourcePositionUnavailableInFrontend() ||
        file_info->get_parent() != expanded_expression) {
      std::cerr << "REX_FRONTEND_INVARIANT[macro-expansion-expression]: "
                   "expanded expression="
                << static_cast<void *>(expanded_expression) << "/"
                << expanded_expression->class_name()
                << " has incomplete lexical file provenance" << std::endl;
      ROSE_ABORT();
    }
  }

  SgMacroExpansionExp *macro_expression =
      new SgMacroExpansionExp(spelling, expanded_expression);
  return macro_expression;
}

bool ClangToSageTranslator::VisitExpr(clang::Expr *expr, SgNode **node) {
  if (expr == nullptr || node == nullptr || *node == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[expression-value-category]: "
                 "expression translation has no exact Clang/Sage node"
              << std::endl;
    ROSE_ABORT();
  }
  SgExpression *sage_expr = isSgExpression(*node);
  if (sage_expr == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[expression-value-category]: Clang "
              << expr->getStmtClassName() << " translated to "
              << (*node)->class_name() << " instead of SgExpression"
              << std::endl;
    ROSE_ABORT();
  }
  sage_expr->set_lvalue(expr->isLValue());
  sage_expr = wrapMacroExpansionExpression(expr, sage_expr);
  sage_expr->set_lvalue(expr->isLValue());
  *node = sage_expr;

  const bool translated = VisitValueStmt(expr, node);
  if (translated && p_lexical_source_nodes.count(*node) == 1)
    constructExpressionPragmas(expr, isSgExpression(*node));
  return translated;
}

bool ClangToSageTranslator::VisitAbstractConditionalOperator(
    clang::AbstractConditionalOperator *abstract_conditional_operator,
    SgNode **node) {
  return VisitExpr(abstract_conditional_operator, node);
}

bool ClangToSageTranslator::VisitBinaryConditionalOperator(
    clang::BinaryConditionalOperator *binary_conditional_operator,
    SgNode **node) {
  clang::Expr *common_child = binary_conditional_operator->getCommon();
  clang::Expr *false_child = binary_conditional_operator->getFalseExpr();
  if (common_child == nullptr || false_child == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[binary-conditional-operands]: GNU "
                 "binary conditional has no exact common or false operand"
              << std::endl;
    ROSE_ABORT();
  }

  SgExpression *common_expr = isSgExpression(Traverse(common_child));
  SgExpression *false_expr = isSgExpression(Traverse(false_child));
  if (common_expr == nullptr || false_expr == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[binary-conditional-operands]: GNU "
                 "binary conditional operands did not translate to exact "
                 "expressions"
              << std::endl;
    ROSE_ABORT();
  }

  SgType *result_type =
      buildTypeFromQualifiedType(binary_conditional_operator->getType());
  if (SageInterface::containsUnknownType(result_type)) {
    std::cerr << "REX_FRONTEND_INVARIANT[binary-conditional-type]: GNU "
                 "binary conditional has no exact semantic result type"
              << std::endl;
    ROSE_ABORT();
  }

  SgConditionalExp *result_expr = SageBuilder::buildBinaryConditionalExp_nfi(
      common_expr, false_expr, result_type);
  applySourceRange(result_expr, binary_conditional_operator->getSourceRange());
  *node = result_expr;

  return VisitAbstractConditionalOperator(binary_conditional_operator, node);
}

bool ClangToSageTranslator::VisitConditionalOperator(
    clang::ConditionalOperator *conditional_operator, SgNode **node) {
  if (conditional_operator == nullptr) {
    failExactExpressionTranslation("conditional-input", nullptr);
  }
  clang::Expr *cond_child = conditional_operator->getCond();
  clang::Expr *true_child = conditional_operator->getTrueExpr();
  clang::Expr *false_child = conditional_operator->getFalseExpr();
  if (cond_child == nullptr || true_child == nullptr ||
      false_child == nullptr) {
    failExactExpressionTranslation("conditional-operands",
                                   conditional_operator);
  }

  SgExpression *cond_expr = isSgExpression(Traverse(cond_child));
  SgExpression *true_expr = isSgExpression(Traverse(true_child));
  SgExpression *false_expr = isSgExpression(Traverse(false_child));
  if (cond_expr == nullptr || true_expr == nullptr || false_expr == nullptr) {
    failExactExpressionTranslation("conditional-operand-translation",
                                   conditional_operator);
  }

  SgType *cond_type =
      buildTypeFromQualifiedType(conditional_operator->getType());
  if (SageInterface::containsUnknownType(cond_type)) {
    failExactExpressionTranslation("conditional-result-type",
                                   conditional_operator);
  }
  SgConditionalExp *cond_exp = SageBuilder::buildConditionalExp_nfi(
      cond_expr, true_expr, false_expr, cond_type);
  applySourceRange(cond_exp, conditional_operator->getSourceRange());
  *node = cond_exp;

  return VisitAbstractConditionalOperator(conditional_operator, node);
}

bool ClangToSageTranslator::VisitAddrLabelExpr(
    clang::AddrLabelExpr *addr_label_expr, SgNode **node) {
  clang::LabelDecl *label_decl = addr_label_expr->getLabel();
  if (label_decl == nullptr) {
    failExactExpressionTranslation("address-of-label-missing-declaration",
                                   addr_label_expr);
  }
  SgLabelSymbol *label_sym =
      isSgLabelSymbol(GetSymbolFromSymbolTable(label_decl));
  if (label_sym == nullptr) {
    failExactExpressionTranslation("address-of-label-missing-symbol",
                                   addr_label_expr);
  }

  SgLabelRefExp *label_ref = SageBuilder::buildLabelRefExp(label_sym);
  applySourceRange(label_ref, addr_label_expr->getSourceRange());
  *node = label_ref;

  return VisitExpr(addr_label_expr, node);
}

bool ClangToSageTranslator::VisitArrayInitLoopExpr(
    clang::ArrayInitLoopExpr *array_init_loop_expr, SgNode **node) {
  clang::OpaqueValueExpr *common_expr = array_init_loop_expr->getCommonExpr();
  ROSE_ASSERT(common_expr != nullptr);
  clang::Expr *common_source = common_expr->getSourceExpr();
  ROSE_ASSERT(common_source != nullptr);
  ROSE_ASSERT(p_compiler_instance->getASTContext().hasSameType(
      common_expr->getType(), common_source->getType()));
  ROSE_ASSERT(common_expr->getValueKind() == common_source->getValueKind());
  SgExpression *common = isSgExpression(Traverse(common_source));
  ROSE_ASSERT(common != nullptr);
  common = prepareExpressionForAttachment(common);
  auto *binding = new SgCxxArrayInitBinding(
      common, translateClangValueCategory(common_expr->getValueKind()));

  SemanticExpressionConstruction semantic_construction(
      p_semantic_expression_subtree_depth, "array-init-loop");
  SgExpression *element = nullptr;
  {
    p_array_init_binding_stack.emplace_back(common_expr, binding);
    struct BindingScope {
      decltype(p_array_init_binding_stack) &stack;
      ~BindingScope() { stack.pop_back(); }
    } binding_scope{p_array_init_binding_stack};
    element = isSgExpression(Traverse(array_init_loop_expr->getSubExpr()));
    ROSE_ASSERT(p_array_init_binding_stack.back().first == common_expr);
    ROSE_ASSERT(p_array_init_binding_stack.back().second == binding);
  }
  ROSE_ASSERT(element != nullptr);
  element = prepareExpressionForAttachment(element);
  const llvm::APInt count = array_init_loop_expr->getArraySize();
  ROSE_ASSERT(count.getActiveBits() <= 64);
  SgType *type = buildTypeFromQualifiedType(array_init_loop_expr->getType());
  auto *loop =
      new SgCxxArrayInitLoop(binding, element, type, count.getZExtValue());
  publishSemanticExpressionSourceProvenance(
      loop, array_init_loop_expr->getSourceRange(), "array-init-loop");
  loop->validate();
  if (p_array_init_binding_stack.empty())
    loop->validate_bindings();
  *node = loop;
  return VisitExpr(array_init_loop_expr, node);
}

bool ClangToSageTranslator::VisitArrayInitIndexExpr(
    clang::ArrayInitIndexExpr *expression, SgNode **node) {
  ROSE_ASSERT(!p_array_init_binding_stack.empty());
  auto *reference = new SgCxxArrayInitReference(
      p_array_init_binding_stack.back().second,
      SgCxxArrayInitReference::e_current_index,
      buildTypeFromQualifiedType(expression->getType()),
      translateClangValueCategory(expression->getValueKind()));
  publishSemanticExpressionSourceProvenance(
      reference, expression->getSourceRange(), "array-init-index");
  reference->validate();
  *node = reference;
  return VisitExpr(expression, node);
}

bool ClangToSageTranslator::VisitArraySubscriptExpr(
    clang::ArraySubscriptExpr *array_subscript_expr, SgNode **node) {
  clang::Expr *clang_base = array_subscript_expr->getBase();
  SgNode *tmp_base = Traverse(clang_base);
  SgExpression *base = isSgExpression(tmp_base);
  if (base == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[array-subscript-base]: Clang base "
                 "translated to "
              << (tmp_base == nullptr ? std::string("<null>")
                                      : tmp_base->class_name())
              << " instead of SgExpression" << std::endl;
    ROSE_ABORT();
  }

  clang::Expr *clang_idx = array_subscript_expr->getIdx();
  SgNode *tmp_idx = Traverse(clang_idx);
  SgExpression *idx = isSgExpression(tmp_idx);
  if (idx == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[array-subscript-index]: Clang index "
                 "translated to "
              << (tmp_idx == nullptr ? std::string("<null>")
                                     : tmp_idx->class_name())
              << " instead of SgExpression" << std::endl;
    ROSE_ABORT();
  }

  const clang::QualType clang_result_type = array_subscript_expr->getType();
  SgType *result_type = buildTypeFromQualifiedType(clang_result_type);
  if (SageInterface::containsUnknownType(result_type)) {
    std::cerr << "REX_FRONTEND_INVARIANT[array-subscript-result-type]: Clang "
                 "subscript has no exact semantic result type"
              << std::endl;
    ROSE_ABORT();
  }
  const auto layout =
      measureSubscriptLayout(p_compiler_instance->getASTContext(),
                             clang_result_type, clang_base->getType());
  *node = SageBuilder::buildPntrArrRefExp_nfi(base, idx, result_type,
                                              layout.result_storage_size_bytes,
                                              {layout.stride_bits});

  return VisitExpr(array_subscript_expr, node);
}

bool ClangToSageTranslator::VisitArrayTypeTraitExpr(
    clang::ArrayTypeTraitExpr *array_type_trait_expr, SgNode **node) {
  // Array type traits map to builtins like __array_rank and __array_extent.
  const char *trait_spelling =
      clang::getTraitSpelling(array_type_trait_expr->getTrait());
  ROSE_ASSERT(trait_spelling != nullptr);

  SgExpressionPtrList args;
  clang::TypeSourceInfo *queried_type_info =
      array_type_trait_expr->getQueriedTypeSourceInfo();
  if (queried_type_info == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[type-trait-argument]: array type "
                 "trait has no exact queried TypeSourceInfo"
              << std::endl;
    ROSE_ABORT();
  }
  clang::TypeLoc queried_type_loc = queried_type_info->getTypeLoc();

  SgType *queried_type = buildTypeFromTypeLoc(queried_type_loc);
  if (queried_type == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[type-trait-argument]: array type "
                 "trait queried TypeLoc has no exact translated type"
              << std::endl;
    ROSE_ABORT();
  }
  SgTypeExpression *queried_type_operand =
      SageBuilder::buildTypeExpression_nfi(queried_type);
  applySourceRange(queried_type_operand, queried_type_loc.getSourceRange());
  args.push_back(queried_type_operand);

  if (clang::Expr *dimension =
          array_type_trait_expr->getDimensionExpression()) {
    SgNode *tmp_dim = Traverse(dimension);
    SgExpression *dim_expr = isSgExpression(tmp_dim);
    ROSE_ASSERT(dim_expr != nullptr);
    args.push_back(dim_expr);
  }

  SgType *result_type =
      buildTypeFromQualifiedType(array_type_trait_expr->getType());

  *node = SageBuilder::buildTypeTraitBuiltinOperator_nfi(
      trait_spelling, SgTypeTraitBuiltinOperator::e_type_trait_builtin,
      result_type, args);

  return VisitExpr(array_type_trait_expr, node);
}

bool ClangToSageTranslator::VisitAtomicExpr(clang::AtomicExpr *atomic_expr,
                                            SgNode **node) {
  SgExprListExp *args = SageBuilder::buildExprListExp_nfi();

  auto append_atomic_arg = [&](auto read_clang_arg) -> void {
    clang::Expr *clang_arg = (read_clang_arg)();
    if (clang_arg == nullptr) {
      std::cerr << "REX_FRONTEND_INVARIANT[atomic-expression]: missing "
                   "required operand"
                << std::endl;
      ROSE_ABORT();
    }

    SgNode *tmp_arg = Traverse(clang_arg);
    SgExpression *arg = isSgExpression(tmp_arg);
    if (arg == nullptr) {
      std::cerr << "REX_FRONTEND_INVARIANT[atomic-expression]: translated "
                   "operand is not an expression"
                << std::endl;
      ROSE_ABORT();
    }
    args->append_expression(arg);
  };

  auto append_atomic_scope_arg = [&]() {
    const bool has_scope_model = atomic_expr->getScopeModel() != nullptr;
    if (has_scope_model) {
      append_atomic_arg([&]() { return atomic_expr->getScope(); });
    }
  };

  const clang::AtomicExpr::AtomicOp atomic_op = atomic_expr->getOp();
  switch (atomic_op) {
  case clang::AtomicExpr::AO__atomic_compare_exchange:
  case clang::AtomicExpr::AO__atomic_compare_exchange_n:
  case clang::AtomicExpr::AO__scoped_atomic_compare_exchange:
  case clang::AtomicExpr::AO__scoped_atomic_compare_exchange_n:
    append_atomic_arg([&]() { return atomic_expr->getPtr(); });
    append_atomic_arg([&]() { return atomic_expr->getVal1(); });
    append_atomic_arg([&]() { return atomic_expr->getVal2(); });
    append_atomic_arg([&]() { return atomic_expr->getWeak(); });
    append_atomic_arg([&]() { return atomic_expr->getOrder(); });
    append_atomic_arg([&]() { return atomic_expr->getOrderFail(); });
    append_atomic_scope_arg();
    break;

  case clang::AtomicExpr::AO__c11_atomic_compare_exchange_strong:
  case clang::AtomicExpr::AO__c11_atomic_compare_exchange_weak:
  case clang::AtomicExpr::AO__opencl_atomic_compare_exchange_strong:
  case clang::AtomicExpr::AO__opencl_atomic_compare_exchange_weak:
  case clang::AtomicExpr::AO__hip_atomic_compare_exchange_strong:
  case clang::AtomicExpr::AO__hip_atomic_compare_exchange_weak:
    append_atomic_arg([&]() { return atomic_expr->getPtr(); });
    append_atomic_arg([&]() { return atomic_expr->getVal1(); });
    append_atomic_arg([&]() { return atomic_expr->getVal2(); });
    append_atomic_arg([&]() { return atomic_expr->getOrder(); });
    append_atomic_arg([&]() { return atomic_expr->getOrderFail(); });
    append_atomic_scope_arg();
    break;

  case clang::AtomicExpr::AO__c11_atomic_init:
  case clang::AtomicExpr::AO__opencl_atomic_init:
    append_atomic_arg([&]() { return atomic_expr->getPtr(); });
    append_atomic_arg([&]() { return atomic_expr->getVal1(); });
    break;

  case clang::AtomicExpr::AO__atomic_exchange:
  case clang::AtomicExpr::AO__scoped_atomic_exchange:
    append_atomic_arg([&]() { return atomic_expr->getPtr(); });
    append_atomic_arg([&]() { return atomic_expr->getVal1(); });
    append_atomic_arg([&]() { return atomic_expr->getVal2(); });
    append_atomic_arg([&]() { return atomic_expr->getOrder(); });
    append_atomic_scope_arg();
    break;

  case clang::AtomicExpr::AO__atomic_add_fetch:
  case clang::AtomicExpr::AO__atomic_and_fetch:
  case clang::AtomicExpr::AO__atomic_clear:
  case clang::AtomicExpr::AO__atomic_exchange_n:
  case clang::AtomicExpr::AO__atomic_fetch_add:
  case clang::AtomicExpr::AO__atomic_fetch_and:
  case clang::AtomicExpr::AO__atomic_fetch_max:
  case clang::AtomicExpr::AO__atomic_fetch_min:
  case clang::AtomicExpr::AO__atomic_fetch_nand:
  case clang::AtomicExpr::AO__atomic_fetch_or:
  case clang::AtomicExpr::AO__atomic_fetch_sub:
  case clang::AtomicExpr::AO__atomic_fetch_udec:
  case clang::AtomicExpr::AO__atomic_fetch_uinc:
  case clang::AtomicExpr::AO__atomic_fetch_xor:
  case clang::AtomicExpr::AO__atomic_load:
  case clang::AtomicExpr::AO__atomic_load_n:
  case clang::AtomicExpr::AO__atomic_max_fetch:
  case clang::AtomicExpr::AO__atomic_min_fetch:
  case clang::AtomicExpr::AO__atomic_nand_fetch:
  case clang::AtomicExpr::AO__atomic_or_fetch:
  case clang::AtomicExpr::AO__atomic_store:
  case clang::AtomicExpr::AO__atomic_store_n:
  case clang::AtomicExpr::AO__atomic_sub_fetch:
  case clang::AtomicExpr::AO__atomic_test_and_set:
  case clang::AtomicExpr::AO__atomic_xor_fetch:
  case clang::AtomicExpr::AO__c11_atomic_exchange:
  case clang::AtomicExpr::AO__c11_atomic_fetch_add:
  case clang::AtomicExpr::AO__c11_atomic_fetch_and:
  case clang::AtomicExpr::AO__c11_atomic_fetch_max:
  case clang::AtomicExpr::AO__c11_atomic_fetch_min:
  case clang::AtomicExpr::AO__c11_atomic_fetch_nand:
  case clang::AtomicExpr::AO__c11_atomic_fetch_or:
  case clang::AtomicExpr::AO__c11_atomic_fetch_sub:
  case clang::AtomicExpr::AO__c11_atomic_fetch_xor:
  case clang::AtomicExpr::AO__c11_atomic_load:
  case clang::AtomicExpr::AO__c11_atomic_store:
  case clang::AtomicExpr::AO__hip_atomic_exchange:
  case clang::AtomicExpr::AO__hip_atomic_fetch_add:
  case clang::AtomicExpr::AO__hip_atomic_fetch_and:
  case clang::AtomicExpr::AO__hip_atomic_fetch_max:
  case clang::AtomicExpr::AO__hip_atomic_fetch_min:
  case clang::AtomicExpr::AO__hip_atomic_fetch_or:
  case clang::AtomicExpr::AO__hip_atomic_fetch_sub:
  case clang::AtomicExpr::AO__hip_atomic_fetch_xor:
  case clang::AtomicExpr::AO__hip_atomic_load:
  case clang::AtomicExpr::AO__hip_atomic_store:
  case clang::AtomicExpr::AO__opencl_atomic_exchange:
  case clang::AtomicExpr::AO__opencl_atomic_fetch_add:
  case clang::AtomicExpr::AO__opencl_atomic_fetch_and:
  case clang::AtomicExpr::AO__opencl_atomic_fetch_max:
  case clang::AtomicExpr::AO__opencl_atomic_fetch_min:
  case clang::AtomicExpr::AO__opencl_atomic_fetch_or:
  case clang::AtomicExpr::AO__opencl_atomic_fetch_sub:
  case clang::AtomicExpr::AO__opencl_atomic_fetch_xor:
  case clang::AtomicExpr::AO__opencl_atomic_load:
  case clang::AtomicExpr::AO__opencl_atomic_store:
  case clang::AtomicExpr::AO__scoped_atomic_add_fetch:
  case clang::AtomicExpr::AO__scoped_atomic_and_fetch:
  case clang::AtomicExpr::AO__scoped_atomic_exchange_n:
  case clang::AtomicExpr::AO__scoped_atomic_fetch_add:
  case clang::AtomicExpr::AO__scoped_atomic_fetch_and:
  case clang::AtomicExpr::AO__scoped_atomic_fetch_max:
  case clang::AtomicExpr::AO__scoped_atomic_fetch_min:
  case clang::AtomicExpr::AO__scoped_atomic_fetch_nand:
  case clang::AtomicExpr::AO__scoped_atomic_fetch_or:
  case clang::AtomicExpr::AO__scoped_atomic_fetch_sub:
  case clang::AtomicExpr::AO__scoped_atomic_fetch_udec:
  case clang::AtomicExpr::AO__scoped_atomic_fetch_uinc:
  case clang::AtomicExpr::AO__scoped_atomic_fetch_xor:
  case clang::AtomicExpr::AO__scoped_atomic_load:
  case clang::AtomicExpr::AO__scoped_atomic_load_n:
  case clang::AtomicExpr::AO__scoped_atomic_max_fetch:
  case clang::AtomicExpr::AO__scoped_atomic_min_fetch:
  case clang::AtomicExpr::AO__scoped_atomic_nand_fetch:
  case clang::AtomicExpr::AO__scoped_atomic_or_fetch:
  case clang::AtomicExpr::AO__scoped_atomic_store:
  case clang::AtomicExpr::AO__scoped_atomic_store_n:
  case clang::AtomicExpr::AO__scoped_atomic_sub_fetch:
  case clang::AtomicExpr::AO__scoped_atomic_xor_fetch:
    append_atomic_arg([&]() { return atomic_expr->getPtr(); });
    if (atomic_expr->hasVal1Operand()) {
      append_atomic_arg([&]() { return atomic_expr->getVal1(); });
    }
    append_atomic_arg([&]() { return atomic_expr->getOrder(); });
    append_atomic_scope_arg();
    break;

  default:
    std::cerr << "REX_FRONTEND_INVARIANT[atomic-expression]: unsupported "
                 "Clang atomic operation "
              << static_cast<unsigned>(atomic_op) << std::endl;
    ROSE_ABORT();
  }

  std::string builtin_name = atomic_expr->getOpAsString().str();

  // Use the actual return type from Clang, not void - many atomics return
  // values Example: __atomic_fetch_add returns the old value before addition
  SgType *return_type = buildTypeFromQualifiedType(atomic_expr->getType());
  ASSERT_not_null(return_type);
  ASSERT_not_null(SageBuilder::topScopeStack());
  const clang::SourceRange atomic_range = atomic_expr->getSourceRange();
  applySourceRange(args, atomic_range);

  SgFunctionCallExp *call = SageBuilder::buildFunctionCallExp_nfi(
      builtin_name, return_type, args, SageBuilder::topScopeStack());
  SgExpression *callee = call != nullptr ? call->get_function() : nullptr;
  const clang::SourceLocation builtin_location = atomic_expr->getBuiltinLoc();
  if (call == nullptr || callee == nullptr || !builtin_location.isValid()) {
    std::cerr << "REX_FRONTEND_INVARIANT[atomic-expression-source]: atomic "
                 "builtin has no exact call, callee, or source location"
              << std::endl;
    ROSE_ABORT();
  }
  applySourceRange(callee,
                   clang::SourceRange(builtin_location, builtin_location));
  *node = call;

  return VisitExpr(atomic_expr, node);
}

bool ClangToSageTranslator::VisitBinaryOperator(
    clang::BinaryOperator *binary_operator, SgNode **node) {
  SgNode *tmp_lhs = Traverse(binary_operator->getLHS());
  SgExpression *lhs = isSgExpression(tmp_lhs);
  if (lhs == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[binary-left-operand]: Clang "
                 "operand translated to "
              << (tmp_lhs == nullptr ? std::string("<null>")
                                     : tmp_lhs->class_name())
              << " instead of SgExpression" << std::endl;
    ROSE_ABORT();
  }

  SgNode *tmp_rhs = Traverse(binary_operator->getRHS());
  SgExpression *rhs = isSgExpression(tmp_rhs);
  if (rhs == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[binary-right-operand]: Clang "
                 "operand translated to "
              << (tmp_rhs == nullptr ? std::string("<null>")
                                     : tmp_rhs->class_name())
              << " instead of SgExpression" << std::endl;
    ROSE_ABORT();
  }

  const clang::BinaryOperatorKind opcode = binary_operator->getOpcode();
  const clang::QualType clang_result_type = binary_operator->getType();
  const clang::Type *clang_result_type_ptr =
      clang_result_type.getTypePtrOrNull();
  const clang::BuiltinType *builtin_result_type =
      llvm::dyn_cast_or_null<clang::BuiltinType>(clang_result_type_ptr);
  const bool has_bound_member_pseudo_type =
      builtin_result_type != nullptr &&
      builtin_result_type->getKind() == clang::BuiltinType::BoundMember;

  SgType *result_type = nullptr;
  if (has_bound_member_pseudo_type) {
    if (opcode != clang::BO_PtrMemD && opcode != clang::BO_PtrMemI) {
      std::cerr << "REX_FRONTEND_INVARIANT[pointer-member-result-type]: "
                   "Clang BoundMember pseudo-type reached non-member-pointer "
                   "binary operator "
                << binary_operator->getOpcodeStr().str() << std::endl;
      ROSE_ABORT();
    }

    const clang::QualType rhs_type = binary_operator->getRHS()->getType();
    const clang::Type *rhs_type_ptr = rhs_type.getTypePtrOrNull();
    const clang::MemberPointerType *member_pointer_type =
        rhs_type_ptr != nullptr
            ? rhs_type_ptr->getAs<clang::MemberPointerType>()
            : nullptr;
    member_pointer_type =
        llvm::dyn_cast_or_null<clang::MemberPointerType>(member_pointer_type);
    if (member_pointer_type == nullptr ||
        !member_pointer_type->isMemberFunctionPointer()) {
      std::cerr << "REX_FRONTEND_INVARIANT[pointer-member-result-type]: "
                   "Clang "
                << binary_operator->getOpcodeStr().str()
                << " BoundMember expression has no exact member-function "
                   "pointer RHS"
                << std::endl;
      ROSE_ABORT();
    }

    const clang::QualType callable_type = member_pointer_type->getPointeeType();
    const clang::Type *callable_type_ptr = callable_type.getTypePtrOrNull();
    if (callable_type_ptr == nullptr || !callable_type_ptr->isFunctionType()) {
      std::cerr << "REX_FRONTEND_INVARIANT[pointer-member-result-type]: "
                   "Clang member-pointer RHS does not point to an exact "
                   "function type"
                << std::endl;
      ROSE_ABORT();
    }

    // BoundMember is Clang's pseudo-type for obj.*member or ptr->*member.
    // The callable result uses the canonical function pointee. Translating a
    // source typedef here produces an alias node instead of a callable type;
    // its spelling belongs to the member-pointer operand's source type.
    result_type = buildSemanticTypeFromQualifiedType(
        callable_type.getCanonicalType(), "pointer-member-result-type");
    if (result_type == nullptr || isSgFunctionType(result_type) == nullptr) {
      std::cerr << "REX_FRONTEND_INVARIANT[pointer-member-result-type]: "
                   "canonical member-function pointee did not translate to "
                   "an exact callable Sage type"
                << std::endl;
      ROSE_ABORT();
    }
  } else {
    result_type = buildTypeFromQualifiedType(clang_result_type);
  }
  if (SageInterface::containsUnknownType(result_type)) {
    std::cerr << "REX_FRONTEND_INVARIANT[binary-result-type]: Clang "
              << binary_operator->getOpcodeStr().str()
              << " expression has no exact semantic result type" << std::endl;
    ROSE_ABORT();
  }

  switch (opcode) {
  case clang::BO_PtrMemD:
    *node = SageBuilder::buildDotStarOp_nfi(lhs, rhs, result_type);
    break; // obj.*ptr_to_member
  case clang::BO_PtrMemI:
    *node = SageBuilder::buildArrowStarOp_nfi(lhs, rhs, result_type);
    break; // ptr->*ptr_to_member
  case clang::BO_Mul:
    *node = SageBuilder::buildMultiplyOp_nfi(lhs, rhs, result_type);
    break;
  case clang::BO_Div:
    *node = SageBuilder::buildDivideOp_nfi(lhs, rhs, result_type);
    break;
  case clang::BO_Rem:
    *node = SageBuilder::buildModOp_nfi(lhs, rhs, result_type);
    break;
  case clang::BO_Add:
    *node = SageBuilder::buildAddOp_nfi(lhs, rhs, result_type);
    break;
  case clang::BO_Sub:
    *node = SageBuilder::buildSubtractOp_nfi(lhs, rhs, result_type);
    break;
  case clang::BO_Shl:
    *node = SageBuilder::buildLshiftOp_nfi(lhs, rhs, result_type);
    break;
  case clang::BO_Shr:
    *node = SageBuilder::buildRshiftOp_nfi(lhs, rhs, result_type);
    break;
  case clang::BO_Cmp:
    *node = SageBuilder::buildSpaceshipOp_nfi(lhs, rhs, result_type);
    break;
  case clang::BO_LT:
    *node = SageBuilder::buildLessThanOp_nfi(lhs, rhs, result_type);
    break;
  case clang::BO_GT:
    *node = SageBuilder::buildGreaterThanOp_nfi(lhs, rhs, result_type);
    break;
  case clang::BO_LE:
    *node = SageBuilder::buildLessOrEqualOp_nfi(lhs, rhs, result_type);
    break;
  case clang::BO_GE:
    *node = SageBuilder::buildGreaterOrEqualOp_nfi(lhs, rhs, result_type);
    break;
  case clang::BO_EQ:
    *node = SageBuilder::buildEqualityOp_nfi(lhs, rhs, result_type);
    break;
  case clang::BO_NE:
    *node = SageBuilder::buildNotEqualOp_nfi(lhs, rhs, result_type);
    break;
  case clang::BO_And:
    *node = SageBuilder::buildBitAndOp_nfi(lhs, rhs, result_type);
    break;
  case clang::BO_Xor:
    *node = SageBuilder::buildBitXorOp_nfi(lhs, rhs, result_type);
    break;
  case clang::BO_Or:
    *node = SageBuilder::buildBitOrOp_nfi(lhs, rhs, result_type);
    break;
  case clang::BO_LAnd:
    *node = SageBuilder::buildAndOp_nfi(lhs, rhs, result_type);
    break;
  case clang::BO_LOr:
    *node = SageBuilder::buildOrOp_nfi(lhs, rhs, result_type);
    break;
  case clang::BO_Assign:
    *node = SageBuilder::buildAssignOp_nfi(lhs, rhs, result_type);
    break;
  case clang::BO_MulAssign:
    *node = SageBuilder::buildMultAssignOp_nfi(lhs, rhs, result_type);
    break;
  case clang::BO_DivAssign:
    *node = SageBuilder::buildDivAssignOp_nfi(lhs, rhs, result_type);
    break;
  case clang::BO_RemAssign:
    *node = SageBuilder::buildModAssignOp_nfi(lhs, rhs, result_type);
    break;
  case clang::BO_AddAssign:
    *node = SageBuilder::buildPlusAssignOp_nfi(lhs, rhs, result_type);
    break;
  case clang::BO_SubAssign:
    *node = SageBuilder::buildMinusAssignOp_nfi(lhs, rhs, result_type);
    break;
  case clang::BO_ShlAssign:
    *node = SageBuilder::buildLshiftAssignOp_nfi(lhs, rhs, result_type);
    break;
  case clang::BO_ShrAssign:
    *node = SageBuilder::buildRshiftAssignOp_nfi(lhs, rhs, result_type);
    break;
  case clang::BO_AndAssign:
    *node = SageBuilder::buildAndAssignOp_nfi(lhs, rhs, result_type);
    break;
  case clang::BO_XorAssign:
    *node = SageBuilder::buildXorAssignOp_nfi(lhs, rhs, result_type);
    break;
  case clang::BO_OrAssign:
    *node = SageBuilder::buildIorAssignOp_nfi(lhs, rhs, result_type);
    break;
  case clang::BO_Comma:
    *node = SageBuilder::buildCommaOpExp_nfi(lhs, rhs, result_type);
    break;
  default:
    std::cerr << "REX_FRONTEND_INVARIANT[binary-operator-kind]: unsupported "
                 "Clang opcode "
              << binary_operator->getOpcodeStr().str() << std::endl;
    ROSE_ABORT();
  }

  return VisitExpr(binary_operator, node);
}

bool ClangToSageTranslator::VisitCompoundAssignOperator(
    clang::CompoundAssignOperator *compound_assign_operator, SgNode **node) {
  return VisitBinaryOperator(compound_assign_operator, node);
}

bool ClangToSageTranslator::VisitBlockExpr(clang::BlockExpr *block_expr,
                                           SgNode **node) {
  (void)node;
  std::cerr << "REX_FRONTEND_INVARIANT[unsupported-source-expression]: "
               "Clang BlockExpr has no lossless typed Sage representation"
            << std::endl;
  ROSE_ABORT();
}

bool ClangToSageTranslator::VisitCallExpr(clang::CallExpr *call_expr,
                                          SgNode **node) {
  if (p_user_defined_literal_semantic_call_depth > 1) {
    std::cerr << "REX_FRONTEND_INVARIANT[user-defined-literal-session]: "
                 "semantic call construction is nested"
              << std::endl;
    ROSE_ABORT();
  }
  const bool user_defined_literal_semantic_call =
      p_user_defined_literal_semantic_call_depth == 1;
  auto traverse_call_component = [&](clang::Expr *expression,
                                     const char *context) -> SgNode * {
    if (!user_defined_literal_semantic_call) {
      return Traverse(expression);
    }
    SemanticExpressionConstruction semantic_component(
        p_user_defined_literal_semantic_expression_depth, context);
    return Traverse(expression);
  };
  clang::Expr *callee_expr = call_expr->getCallee();

  clang::FunctionDecl *direct_callee = call_expr->getDirectCallee();
  clang::CXXConversionDecl *conversion_decl =
      llvm::dyn_cast_or_null<clang::CXXConversionDecl>(direct_callee);
  clang::MemberExpr *conversion_member =
      llvm::dyn_cast_or_null<clang::MemberExpr>(callee_expr);
  if (conversion_decl != nullptr && conversion_member == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[implicit-conversion-owner]: "
                 "conversion call callee is "
              << callee_expr->getStmtClassName()
              << " instead of the exact Clang MemberExpr owner" << std::endl;
    ROSE_ABORT();
  }
  clang::SourceRange conversion_member_name_range;
  if (conversion_member != nullptr) {
    conversion_member_name_range =
        conversion_member->getMemberNameInfo().getSourceRange();
  }
  // A CXXConversionDecl with no written MemberExpr name is Clang's exact
  // semantic conversion-call wrapper.  RParenLoc is not authoritative here:
  // Clang can reuse the operand's source extent even though no `operator T()`
  // token sequence exists.  The typed declaration plus absent member-name
  // surface distinguishes implicit conversion from an explicit operator call.
  const bool implicit_conversion_call = conversion_decl != nullptr &&
                                        conversion_member != nullptr &&
                                        !conversion_member_name_range.isValid();
  clang::CXXMethodDecl *direct_method =
      llvm::dyn_cast_or_null<clang::CXXMethodDecl>(direct_callee);
  const bool implicit_call_operator_callee =
      conversion_decl == nullptr && direct_method != nullptr &&
      conversion_member != nullptr && !conversion_member_name_range.isValid() &&
      direct_method->getOverloadedOperator() == clang::OO_Call;
  const bool synthesized_member_callee =
      implicit_conversion_call || implicit_call_operator_callee;
  struct SynthesizedCalleeContextGuard {
    std::unordered_set<const clang::Expr *> &expressions;
    const clang::Expr *expression;

    SynthesizedCalleeContextGuard(
        std::unordered_set<const clang::Expr *> &expressions,
        const clang::Expr *expression)
        : expressions(expressions), expression(expression) {
      if (expression != nullptr && !expressions.insert(expression).second) {
        std::cerr << "REX_FRONTEND_INVARIANT[synthesized-member-callee-"
                     "owner]: synthesized member callee was already active"
                  << std::endl;
        ROSE_ABORT();
      }
    }

    ~SynthesizedCalleeContextGuard() {
      if (expression != nullptr && expressions.erase(expression) != 1) {
        std::cerr << "REX_FRONTEND_INVARIANT[synthesized-member-callee-"
                     "owner]: active synthesized member callee was lost"
                  << std::endl;
        ROSE_ABORT();
      }
    }
  };

  // A call argument can be the source owner of semantic entities that also
  // occur in the instantiated callee signature. In particular, a lambda
  // argument is the sole construction owner of its closure record. Translate
  // every written argument before the callee so those source producers publish
  // their exact typed identities before signature translation consumes them.
  std::vector<SgExpression *> translated_arguments;
  for (clang::CallExpr::arg_iterator it = call_expr->arg_begin();
       it != call_expr->arg_end(); ++it) {
    clang::Expr *arg = *it;
    SgNode *translated_argument_node = traverse_call_component(
        arg, "VisitCallExpr:user-defined-literal-argument");
    SgExpression *translated_argument =
        isSgExpression(translated_argument_node);
    if (translated_argument == nullptr) {
      std::cerr << "REX_FRONTEND_INVARIANT[call-argument]: argument "
                   "translated to "
                << (translated_argument_node != nullptr
                        ? translated_argument_node->class_name()
                        : std::string("<null>"))
                << std::endl;
      ROSE_ABORT();
    }
    clang::Expr *ignored_arg = arg->IgnoreParenImpCasts();
    if (clang::isa<clang::InitListExpr>(ignored_arg)) {
      if (SgExprListExp *init_list = isSgExprListExp(translated_argument)) {
        SgType *init_type = buildTypeFromQualifiedType(arg->getType());
        if (init_type == nullptr) {
          std::cerr << "REX_FRONTEND_INVARIANT[call-argument-type]: "
                       "initializer-list argument type did not translate"
                    << std::endl;
          ROSE_ABORT();
        }
        SgAggregateInitializer *aggregate_init =
            SageBuilder::buildAggregateInitializer_nfi(
                init_list, init_type,
                SgAggregateInitializer::e_aggregate_initializer_source_braced);
        applySourceRange(aggregate_init, arg->getSourceRange());
        translated_argument = aggregate_init;
      }
    }
    translated_arguments.push_back(translated_argument);
  }

  SgNode *translated_callee = nullptr;
  {
    SynthesizedCalleeContextGuard synthesized_callee_context(
        p_synthesized_callee_expressions,
        synthesized_member_callee ? callee_expr : nullptr);
    translated_callee = traverse_call_component(
        callee_expr, "VisitCallExpr:user-defined-literal-callee");
  }
  SgExpression *expr = isSgExpression(translated_callee);
  if (expr == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[call-callee]: callee translated to "
              << (translated_callee != nullptr ? translated_callee->class_name()
                                               : std::string("<null>"))
              << std::endl;
    ROSE_ABORT();
  }

  auto unresolved_nonreal_callee =
      [](SgExpression *callee) -> SgNonrealRefExp * {
    if (SgNonrealRefExp *reference = isSgNonrealRefExp(callee)) {
      return reference;
    }
    if (SgDotExp *member_access = isSgDotExp(callee)) {
      return isSgNonrealRefExp(member_access->get_rhs_operand());
    }
    if (SgArrowExp *member_access = isSgArrowExp(callee)) {
      return isSgNonrealRefExp(member_access->get_rhs_operand());
    }
    return nullptr;
  };
  if (direct_callee == nullptr) {
    if (SgNonrealRefExp *dependent_callee = unresolved_nonreal_callee(expr)) {
      if (dependent_callee->get_resolved_function_declaration() == nullptr &&
          dependent_callee->get_resolved_variable_declaration() == nullptr) {
        // The call expression is the exact semantic context that turns an
        // otherwise ordinary dependent name into an unresolved callable.
        // Publish the role here instead of guessing from the name-expression
        // kind: the same DependentScopeDeclRefExpr also represents dependent
        // variables in non-call positions.
        dependent_callee->set_semantic_role(
            SgNonrealRefExp::e_dependent_callable);
      }
    }
  }

  SgExprListExp *param_list = SageBuilder::buildExprListExp_nfi();
  if (implicit_conversion_call || user_defined_literal_semantic_call) {
    setSynthesizedFileInfo(param_list);
  } else {
    applySourceRange(param_list, call_expr->getSourceRange());
  }

  for (SgExpression *argument : translated_arguments) {
    param_list->append_expression(argument);
  }

  const clang::FunctionDecl *direct_callee_for_result_type =
      llvm::dyn_cast_or_null<clang::FunctionDecl>(call_expr->getDirectCallee());
  const clang::ClassTemplateSpecializationDecl *member_call_specialization =
      nullptr;
  if (const clang::CXXMethodDecl *method =
          llvm::dyn_cast_or_null<clang::CXXMethodDecl>(
              direct_callee_for_result_type)) {
    member_call_specialization =
        llvm::dyn_cast_or_null<clang::ClassTemplateSpecializationDecl>(
            method->getParent());
  }
  struct MemberCallSpecializationContextGuard {
    std::vector<const clang::ClassTemplateSpecializationDecl *> &stack;
    bool active;

    MemberCallSpecializationContextGuard(
        std::vector<const clang::ClassTemplateSpecializationDecl *> &stack,
        const clang::ClassTemplateSpecializationDecl *specialization)
        : stack(stack), active(specialization != nullptr) {
      if (active) {
        stack.push_back(specialization);
      }
    }

    ~MemberCallSpecializationContextGuard() {
      if (active) {
        stack.pop_back();
      }
    }
  } member_call_specialization_guard(p_member_call_specialization_context_stack,
                                     member_call_specialization);
  clang::QualType native_result_type =
      nativeCallResultType(*call_expr, p_compiler_instance->getASTContext(),
                           direct_callee_for_result_type);
  SgType *call_type = buildTypeFromQualifiedType(native_result_type);
  if (SageInterface::containsUnknownType(call_type) ||
      isSgFunctionType(call_type) != nullptr ||
      isSgMemberFunctionType(call_type) != nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[call-result-type]: Clang "
              << call_expr->getStmtClassName() << " source='"
              << getSourceText(call_expr->getSourceRange())
              << "' expression-type='" << call_expr->getType().getAsString()
              << "' call-return='" << native_result_type.getAsString()
              << "' translated to "
              << (call_type != nullptr ? call_type->class_name()
                                       : std::string("<null>"))
              << " instead of one exact semantic value type" << std::endl;
    ROSE_ABORT();
  }

  SgFunctionCallExp *call_exp =
      new SgFunctionCallExp(expr, param_list, call_type);
  SageInterface::setOneSourcePositionNull(call_exp);
  *node = call_exp;
  call_exp->set_source_syntax(implicit_conversion_call
                                  ? SgFunctionCallExp::e_implicit_conversion
                                  : SgFunctionCallExp::e_source_function_call);

  std::vector<std::pair<SgLocatedNode *, const char *>>
      implicit_conversion_wrappers;
  if (implicit_conversion_call) {
    // The call/operator-reference path is a typed semantic wrapper around the
    // source-spelled object expression. Register those synthesized nodes before
    // the generic expression visitor so source-range processing preserves and
    // validates their semantic provenance instead of overwriting it.
    setSynthesizedFileInfo(call_exp);
    implicit_conversion_wrappers.emplace_back(call_exp, "call wrapper");
    implicit_conversion_wrappers.emplace_back(param_list, "argument list");
    if (SgDotExp *dot = isSgDotExp(expr)) {
      implicit_conversion_wrappers.emplace_back(dot, "member-access wrapper");
      implicit_conversion_wrappers.emplace_back(
          isSgLocatedNode(dot->get_rhs_operand()),
          "conversion-function reference");
    } else if (SgArrowExp *arrow = isSgArrowExp(expr)) {
      implicit_conversion_wrappers.emplace_back(arrow, "member-access wrapper");
      implicit_conversion_wrappers.emplace_back(
          isSgLocatedNode(arrow->get_rhs_operand()),
          "conversion-function reference");
    } else {
      implicit_conversion_wrappers.emplace_back(
          isSgLocatedNode(expr), "conversion-function reference");
    }
    for (const auto &wrapper : implicit_conversion_wrappers) {
      if (wrapper.first == nullptr) {
        std::cerr
            << "REX_FRONTEND_INVARIANT[implicit-conversion-owner]: missing "
            << wrapper.second << std::endl;
        ROSE_ABORT();
      }
      if (p_synthesized_source_nodes.count(wrapper.first) != 1) {
        std::cerr << "REX_FRONTEND_INVARIANT[implicit-conversion-owner]: "
                  << wrapper.second
                  << " was not classified by its exact producer" << std::endl;
        ROSE_ABORT();
      }
    }
  }

  // The literal producer publishes source ownership after its lexical operands.
  if (user_defined_literal_semantic_call)
    return true;

  const bool visit_ok = VisitExpr(call_expr, node);
  if (implicit_conversion_call) {
    if (*node != call_exp) {
      std::cerr << "REX_FRONTEND_INVARIANT[implicit-conversion-owner]: "
                   "generic expression translation replaced the registered "
                   "conversion-call wrapper"
                << std::endl;
      ROSE_ABORT();
    }
    for (const auto &wrapper : implicit_conversion_wrappers) {
      for (Sg_File_Info *file_info : {wrapper.first->get_file_info(),
                                      wrapper.first->get_startOfConstruct(),
                                      wrapper.first->get_endOfConstruct()}) {
        if (file_info == nullptr || !file_info->isCompilerGenerated() ||
            !file_info->isOutputInCodeGeneration()) {
          std::cerr << "REX_FRONTEND_INVARIANT[implicit-conversion-owner]: "
                       "generic expression translation changed synthesized "
                    << wrapper.second << " provenance" << std::endl;
          ROSE_ABORT();
        }
      }
    }
  }

  return visit_ok;
}

bool ClangToSageTranslator::VisitCUDAKernelCallExpr(
    clang::CUDAKernelCallExpr *cuda_kernel_call_expr, SgNode **node) {
  auto translate_expr = [&](clang::Expr *expr,
                            const char *label) -> SgExpression * {
    if (expr == nullptr) {
      std::cerr << "REX_FRONTEND_INVARIANT[cuda-kernel-expression]: required "
                << label << " is absent" << std::endl;
      ROSE_ABORT();
    }

    SgNode *tmp_expr = Traverse(expr);
    SgExpression *sg_expr = isSgExpression(tmp_expr);
    if (sg_expr == nullptr) {
      std::cerr << "REX_FRONTEND_INVARIANT[cuda-kernel-expression]: " << label
                << " translated to "
                << (tmp_expr == nullptr ? std::string("<null>")
                                        : tmp_expr->class_name())
                << " instead of SgExpression" << std::endl;
      ROSE_ABORT();
    }
    return sg_expr;
  };

  SgExpression *kernel =
      translate_expr(cuda_kernel_call_expr->getCallee(), "CUDA kernel callee");

  clang::CallExpr *config_call = cuda_kernel_call_expr->getConfig();
  if (config_call == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[cuda-launch-configuration]: Clang "
                 "CUDAKernelCallExpr has no launch configuration"
              << std::endl;
    ROSE_ABORT();
  }

  auto translate_config_arg = [&](unsigned index,
                                  const char *label) -> SgExpression * {
    if (index >= config_call->getNumArgs()) {
      return nullptr;
    }
    return translate_expr(config_call->getArg(index), label);
  };

  SgExpression *grid =
      translate_config_arg(0, "CUDA launch configuration grid");
  SgExpression *blocks =
      translate_config_arg(1, "CUDA launch configuration blocks");
  SgExpression *shared =
      translate_config_arg(2, "CUDA launch configuration shared memory");
  SgExpression *stream =
      translate_config_arg(3, "CUDA launch configuration stream");

  if (kernel == nullptr || grid == nullptr || blocks == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[cuda-launch-configuration]: exact "
                 "kernel, grid, or block expression is absent"
              << std::endl;
    ROSE_ABORT();
  }

  SgExprListExp *param_list = SageBuilder::buildExprListExp_nfi();
  applySourceRange(param_list, cuda_kernel_call_expr->getSourceRange());

  for (clang::CallExpr::arg_iterator it = cuda_kernel_call_expr->arg_begin();
       it != cuda_kernel_call_expr->arg_end(); ++it) {
    SgExpression *arg = translate_expr(*it, "CUDA kernel argument");
    ASSERT_not_null(arg);
    param_list->append_expression(arg);
  }

  SgCudaKernelExecConfig *exec_config =
      SageBuilder::buildCudaKernelExecConfig_nfi(grid, blocks, shared, stream);
  applySourceRange(exec_config, config_call->getSourceRange());

  SgCudaKernelCallExp *kernel_call =
      SageBuilder::buildCudaKernelCallExp_nfi(kernel, param_list, exec_config);
  applySourceRange(kernel_call, cuda_kernel_call_expr->getSourceRange());
  *node = kernel_call;

  return VisitExpr(cuda_kernel_call_expr, node);
}

bool ClangToSageTranslator::VisitCXXMemberCallExpr(
    clang::CXXMemberCallExpr *cxx_member_call_expr, SgNode **node) {
  // CXXMemberCallExpr represents calls to member functions (e.g., obj.method()
  // or ptr->method()) Delegate to CallExpr handler which will handle function
  // call expression generation
  return VisitCallExpr(cxx_member_call_expr, node);
}

bool ClangToSageTranslator::VisitCXXOperatorCallExpr(
    clang::CXXOperatorCallExpr *cxx_operator_call_expr, SgNode **node) {
  auto callee_has_explicit_template_args = [&]() -> bool {
    clang::Expr *callee = cxx_operator_call_expr->getCallee();
    if (callee == nullptr) {
      return false;
    }

    clang::Expr *base = callee->IgnoreParenImpCasts();
    if (clang::OverloadExpr *overload =
            llvm::dyn_cast_or_null<clang::OverloadExpr>(base)) {
      return overload->hasExplicitTemplateArgs();
    }
    if (clang::DeclRefExpr *decl_ref =
            llvm::dyn_cast_or_null<clang::DeclRefExpr>(base)) {
      return decl_ref->hasExplicitTemplateArgs();
    }
    if (clang::MemberExpr *member_expr =
            llvm::dyn_cast_or_null<clang::MemberExpr>(base)) {
      return member_expr->hasExplicitTemplateArgs();
    }
    return false;
  };

  const bool has_explicit_template_args = callee_has_explicit_template_args();
  const bool preserve_unary_deref_call =
      cxx_operator_call_expr->getOperator() == clang::OO_Star &&
      cxx_operator_call_expr->getNumArgs() == 1;

  std::vector<SgExpression *> translated_operator_args(
      cxx_operator_call_expr->getNumArgs(), nullptr);
  std::vector<bool> operator_arg_was_translated(
      cxx_operator_call_expr->getNumArgs(), false);
  auto traverse_operator_arg = [&](unsigned index) -> SgExpression * {
    if (index >= cxx_operator_call_expr->getNumArgs()) {
      return nullptr;
    }

    if (operator_arg_was_translated[index]) {
      return translated_operator_args[index];
    }
    operator_arg_was_translated[index] = true;

    clang::Expr *clang_arg = cxx_operator_call_expr->getArg(index);
    const bool postfix_semantic_dummy =
        index == 1 && cxx_operator_call_expr->getNumArgs() == 2 &&
        (cxx_operator_call_expr->getOperator() == clang::OO_PlusPlus ||
         cxx_operator_call_expr->getOperator() == clang::OO_MinusMinus);
    if (postfix_semantic_dummy) {
      clang::IntegerLiteral *dummy =
          llvm::dyn_cast<clang::IntegerLiteral>(clang_arg);
      const clang::BuiltinType *dummy_type =
          dummy != nullptr ? dummy->getType()->getAs<clang::BuiltinType>()
                           : nullptr;
      if (dummy == nullptr || dummy_type == nullptr ||
          dummy_type->getKind() != clang::BuiltinType::Int ||
          !dummy->getValue().isZero() || dummy->getSourceRange().isValid()) {
        std::cerr << "REX_FRONTEND_INVARIANT[postfix-operator-semantic-dummy]: "
                     "postfix increment/decrement requires Clang's exact "
                     "source-less integer-zero operand"
                  << std::endl;
        ROSE_ABORT();
      }
      ++p_canonical_generated_literal_depth;
    }

    SgNode *tmp_arg = Traverse(clang_arg);

    if (postfix_semantic_dummy) {
      if (p_canonical_generated_literal_depth == 0) {
        std::cerr << "REX_FRONTEND_INVARIANT[postfix-operator-semantic-dummy]: "
                     "canonical-literal construction transaction underflow"
                  << std::endl;
        ROSE_ABORT();
      }
      --p_canonical_generated_literal_depth;
    }
    SgExpression *arg = isSgExpression(tmp_arg);
    if (arg == nullptr) {
      std::cerr << "REX_FRONTEND_INVARIANT[overloaded-operator-argument]: "
                   "argument "
                << index << " translated to "
                << (tmp_arg != nullptr ? tmp_arg->class_name()
                                       : std::string("<null>"))
                << std::endl;
      ROSE_ABORT();
    }
    translated_operator_args[index] = arg;
    return arg;
  };

  auto translate_all_operator_args = [&]() {
    for (unsigned i = 0; i < cxx_operator_call_expr->getNumArgs(); ++i) {
      traverse_operator_arg(i);
    }
  };

  auto translated_operator_declaration =
      [&](clang::FunctionDecl *direct_callee) -> SgFunctionDeclaration * {
    if (direct_callee == nullptr) {
      std::cerr << "REX_FRONTEND_INVARIANT[overloaded-operator-callee]: "
                   "missing direct Clang function declaration"
                << std::endl;
      ROSE_ABORT();
    }

    clang::Decl *source_callable = direct_callee;
    clang::FunctionTemplateDecl *deduced_source_template =
        direct_callee->getPrimaryTemplate();
    if (deduced_source_template != nullptr) {
      // Operator syntax has no DeclRefExpr producer. Its source-name edge
      // names the primary template independently of the selected semantic
      // specialization, which is published before constructing the reference.
      source_callable = deduced_source_template;
    }

    clang::FunctionDecl *source_function =
        deduced_source_template != nullptr
            ? deduced_source_template->getTemplatedDecl()
            : direct_callee;
    SgNode *tmp_decl = nullptr;
    auto hidden_callable =
        p_hidden_friend_callable_decl_map.find(source_function);
    if (hidden_callable != p_hidden_friend_callable_decl_map.end()) {
      tmp_decl = hidden_callable->second;
    } else {
      auto it_decl = p_decl_translation_map.find(source_callable);
      if (it_decl != p_decl_translation_map.end()) {
        tmp_decl = it_decl->second;
      } else {
        tmp_decl = TraverseOnDemand(source_callable);
        hidden_callable =
            p_hidden_friend_callable_decl_map.find(source_function);
        if (hidden_callable != p_hidden_friend_callable_decl_map.end()) {
          tmp_decl = hidden_callable->second;
        }
      }
    }
    if (SgTemplateInstantiationDirectiveStatement *directive =
            isSgTemplateInstantiationDirectiveStatement(tmp_decl)) {
      tmp_decl = directive->get_declaration();
    }

    SgFunctionDeclaration *function_decl = isSgFunctionDeclaration(tmp_decl);
    if (function_decl == nullptr) {
      std::cerr << "REX_FRONTEND_INVARIANT[overloaded-operator-callee]: exact "
                   "Clang declaration '"
                << direct_callee->getQualifiedNameAsString()
                << "' translated to "
                << (tmp_decl != nullptr ? tmp_decl->class_name()
                                        : std::string("<null>"))
                << std::endl;
      ROSE_ABORT();
    }
    return function_decl;
  };

  auto declarations_share_redeclaration_chain =
      [](SgFunctionDeclaration *lhs, SgFunctionDeclaration *rhs) -> bool {
    if (lhs == nullptr || rhs == nullptr) {
      return false;
    }
    if (lhs == rhs) {
      return true;
    }
    std::array<SgFunctionDeclaration *, 3> lhs_family{
        lhs, isSgFunctionDeclaration(lhs->get_firstNondefiningDeclaration()),
        isSgFunctionDeclaration(lhs->get_definingDeclaration())};
    std::array<SgFunctionDeclaration *, 3> rhs_family{
        rhs, isSgFunctionDeclaration(rhs->get_firstNondefiningDeclaration()),
        isSgFunctionDeclaration(rhs->get_definingDeclaration())};
    for (SgFunctionDeclaration *lhs_decl : lhs_family) {
      for (SgFunctionDeclaration *rhs_decl : rhs_family) {
        if (lhs_decl != nullptr && lhs_decl == rhs_decl) {
          return true;
        }
      }
    }
    return false;
  };

  auto exact_operator_result_type =
      [&](clang::FunctionDecl *direct_callee) -> SgType * {
    if (direct_callee == nullptr) {
      std::cerr << "REX_FRONTEND_INVARIANT[overloaded-operator-result-type]: "
                   "operator call has no exact direct callee"
                << std::endl;
      ROSE_ABORT();
    }
    SgType *translated = buildTypeFromQualifiedType(nativeCallResultType(
        *cxx_operator_call_expr, p_compiler_instance->getASTContext(),
        direct_callee));
    if (SageInterface::containsUnknownType(translated)) {
      std::cerr << "REX_FRONTEND_INVARIANT[overloaded-operator-result-type]: "
                   "operator call result did not translate to an exact Sage "
                   "type"
                << std::endl;
      ROSE_ABORT();
    }
    return translated;
  };

  auto resolve_member_operator_symbol =
      [&](clang::FunctionDecl *direct_callee) -> SgMemberFunctionSymbol * {
    if (!llvm::isa_and_nonnull<clang::CXXMethodDecl>(direct_callee)) {
      std::cerr << "REX_FRONTEND_INVARIANT[overloaded-member-operator]: "
                   "direct callee is not a Clang method"
                << std::endl;
      ROSE_ABORT();
    }
    SgMemberFunctionDeclaration *member_decl = isSgMemberFunctionDeclaration(
        translated_operator_declaration(direct_callee));
    if (member_decl == nullptr) {
      std::cerr << "REX_FRONTEND_INVARIANT[overloaded-member-operator]: exact "
                   "callee is not a Sage member-function declaration"
                << std::endl;
      ROSE_ABORT();
    }
    auto *canonical =
        isSgMemberFunctionDeclaration(canonical_function_identity(member_decl));
    ASSERT_not_null(canonical);
    auto *member_sym =
        isSgMemberFunctionSymbol(canonical->get_symbol_from_symbol_table());
    requireOperatorSymbolBinding(member_decl, canonical, member_sym,
                                 "overloaded-member-operator-symbol");

    return member_sym;
  };

  auto build_member_operator_call_expr =
      [&](clang::FunctionDecl *direct_callee,
          const std::vector<SgExpression *> &translated_args,
          bool copy_translated_args = false) -> SgFunctionCallExp * {
    if (direct_callee == nullptr ||
        !llvm::isa<clang::CXXMethodDecl>(direct_callee)) {
      std::cerr << "REX_FRONTEND_INVARIANT[overloaded-member-operator]: "
                   "cannot build a member call without an exact method"
                << std::endl;
      ROSE_ABORT();
    }
    if (translated_args.size() != cxx_operator_call_expr->getNumArgs() ||
        translated_args.empty() || translated_args.front() == nullptr) {
      std::cerr << "REX_FRONTEND_INVARIANT[overloaded-member-operator]: "
                   "translated arguments do not contain the implicit object"
                << std::endl;
      ROSE_ABORT();
    }

    SgExpression *base = translated_args.front();
    if (copy_translated_args) {
      base = SageInterface::copyExpression(base);
    }
    SgMemberFunctionSymbol *member_sym =
        resolve_member_operator_symbol(direct_callee);
    if (base == nullptr || member_sym == nullptr) {
      std::cerr << "REX_FRONTEND_INVARIANT[overloaded-member-operator]: "
                   "failed to construct the exact member-call base"
                << std::endl;
      ROSE_ABORT();
    }

    clang::FunctionTemplateDecl *deduced_member_source_template =
        direct_callee->getPrimaryTemplate();

    const bool virtual_dispatch =
        llvm::cast<clang::CXXMethodDecl>(direct_callee)->isVirtual();
    SgExpression *member_ref = nullptr;
    if (deduced_member_source_template != nullptr) {
      SgTemplateMemberFunctionSymbol *template_member_symbol =
          isSgTemplateMemberFunctionSymbol(member_sym);
      if (template_member_symbol == nullptr) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[deduced-template-member-operator]: "
                "operator=%s source template has no exact template-member "
                "symbol\n",
                direct_callee->getQualifiedNameAsString().c_str());
        ROSE_ABORT();
      }
      SgTemplateMemberFunctionRefExp *template_reference =
          SageBuilder::buildTemplateMemberFunctionRefExp_nfi(
              template_member_symbol, virtual_dispatch, false);
      // Operator syntax does not contain a written member-function name.  The
      // typed reference is therefore part of the semantic dot/arrow scaffold
      // and must receive provenance at construction, just like an ordinary
      // semantic member-function reference.
      SageBuilder::initializeSemanticExpressionSourceProvenance(
          template_reference);
      SgMemberFunctionDeclaration *semantic_function =
          isSgMemberFunctionDeclaration(
              requireSelectedFunctionDeclaration(direct_callee));
      ROSE_ASSERT(semantic_function != nullptr);
      template_reference->set_semantic_member_function_declaration(
          semantic_function);
      member_ref = template_reference;
    } else {
      member_ref = SageBuilder::buildSemanticMemberFunctionRefExp(
          member_sym, virtual_dispatch, false);
    }
    SgExpression *callee = nullptr;
    if (SageInterface::isPointerType(base->get_type())) {
      SgArrowExp *arrow = SageBuilder::buildArrowExp_nfi(
          base, member_ref, member_ref->get_type());
      arrow->set_emission_role(SgArrowExp::e_emit_arrow_operator);
      callee = arrow;
    } else {
      callee = SageBuilder::buildDotExp_nfi(base, member_ref,
                                            member_ref->get_type());
    }
    // In operator syntax (`a == b`, `*it`, ...), Clang's direct callee is
    // semantic identity rather than a written member-access expression.  The
    // call owns the physical operator range; publish the typed dot/arrow
    // scaffold as semantic at construction instead of leaving transformation
    // file information for a later function-boundary pass to repair.
    SageBuilder::initializeSemanticExpressionSourceProvenance(callee);

    SgExprListExp *param_list = SageBuilder::buildExprListExp_nfi();
    applySourceRange(param_list, cxx_operator_call_expr->getSourceRange());

    for (size_t i = 1; i < translated_args.size(); ++i) {
      SgExpression *arg = translated_args[i];
      if (arg == nullptr) {
        std::cerr << "REX_FRONTEND_INVARIANT[overloaded-member-operator]: "
                     "missing translated argument "
                  << i << std::endl;
        ROSE_ABORT();
      }
      if (copy_translated_args) {
        arg = SageInterface::copyExpression(arg);
        if (arg == nullptr) {
          std::cerr << "REX_FRONTEND_INVARIANT[overloaded-member-operator]: "
                       "failed to copy argument "
                    << i << std::endl;
          ROSE_ABORT();
        }
      }
      param_list->append_expression(arg);
    }

    SgFunctionCallExp *call = SageBuilder::buildFunctionCallExp_nfi(
        callee, exact_operator_result_type(direct_callee), param_list);
    if (call == nullptr) {
      std::cerr << "REX_FRONTEND_INVARIANT[overloaded-member-operator]: "
                   "function-call builder returned null"
                << std::endl;
      ROSE_ABORT();
    }
    call->set_uses_operator_syntax(true);
    applySourceRange(call, cxx_operator_call_expr->getSourceRange());
    return call;
  };

  auto build_nonmember_operator_call_expr =
      [&](clang::FunctionDecl *direct_callee,
          const std::vector<SgExpression *> &translated_args,
          bool copy_translated_args = false) -> SgFunctionCallExp * {
    if (direct_callee == nullptr ||
        llvm::isa<clang::CXXMethodDecl>(direct_callee)) {
      std::cerr << "REX_FRONTEND_INVARIANT[overloaded-nonmember-operator]: "
                   "cannot build a nonmember call without an exact function"
                << std::endl;
      ROSE_ABORT();
    }
    if (translated_args.size() != cxx_operator_call_expr->getNumArgs()) {
      std::cerr << "REX_FRONTEND_INVARIANT[overloaded-nonmember-operator]: "
                   "translated argument count does not match Clang"
                << std::endl;
      ROSE_ABORT();
    }

    SgFunctionDeclaration *function_decl =
        translated_operator_declaration(direct_callee);
    if (isSgMemberFunctionDeclaration(function_decl) != nullptr) {
      std::cerr << "REX_FRONTEND_INVARIANT[overloaded-nonmember-operator]: "
                   "exact declaration unexpectedly denotes a member"
                << std::endl;
      ROSE_ABORT();
    }
    clang::FunctionTemplateDecl *deduced_operator_source_template =
        direct_callee->getPrimaryTemplate();
    auto *canonical_symbol_declaration =
        canonical_function_identity(function_decl);
    ASSERT_not_null(canonical_symbol_declaration);
    SgFunctionSymbol *function_symbol = isSgFunctionSymbol(
        canonical_symbol_declaration->get_symbol_from_symbol_table());
    SgTemplateFunctionSymbol *template_function_symbol =
        isSgTemplateFunctionSymbol(
            canonical_symbol_declaration->get_symbol_from_symbol_table());
    SgSymbol *exact_function_symbol =
        deduced_operator_source_template != nullptr
            ? static_cast<SgSymbol *>(template_function_symbol)
            : static_cast<SgSymbol *>(function_symbol);
    requireOperatorSymbolBinding(function_decl, canonical_symbol_declaration,
                                 exact_function_symbol,
                                 "overloaded-nonmember-operator-symbol");
    SgExpression *callee = nullptr;
    if (deduced_operator_source_template != nullptr) {
      SgTemplateFunctionRefExp *template_reference =
          SageBuilder::buildTemplateFunctionRefExp_nfi(
              template_function_symbol);
      template_reference->set_semantic_function_declaration(
          requireSelectedFunctionDeclaration(direct_callee));
      callee = template_reference;
    } else {
      callee = buildFunctionReference(function_symbol, direct_callee->getType(),
                                      true);
    }
    ROSE_ASSERT(callee != nullptr);
    // A nonmember operator's function name is likewise absent from operator
    // syntax.  Preserve the exact function symbol while classifying the
    // synthesized reference as semantic from birth.
    SageBuilder::initializeSemanticExpressionSourceProvenance(callee);

    SgExprListExp *param_list = SageBuilder::buildExprListExp_nfi();
    applySourceRange(param_list, cxx_operator_call_expr->getSourceRange());
    for (size_t i = 0; i < translated_args.size(); ++i) {
      SgExpression *arg = translated_args[i];
      if (arg == nullptr) {
        std::cerr << "REX_FRONTEND_INVARIANT[overloaded-nonmember-operator]: "
                     "missing translated argument "
                  << i << std::endl;
        ROSE_ABORT();
      }
      if (copy_translated_args) {
        arg = SageInterface::copyExpression(arg);
        if (arg == nullptr) {
          std::cerr << "REX_FRONTEND_INVARIANT[overloaded-nonmember-operator]: "
                       "failed to copy argument "
                    << i << std::endl;
          ROSE_ABORT();
        }
      }
      param_list->append_expression(arg);
    }

    SgFunctionCallExp *call = SageBuilder::buildFunctionCallExp_nfi(
        callee, exact_operator_result_type(direct_callee), param_list);
    if (call == nullptr) {
      std::cerr << "REX_FRONTEND_INVARIANT[overloaded-nonmember-operator]: "
                   "function-call builder returned null"
                << std::endl;
      ROSE_ABORT();
    }
    call->set_uses_operator_syntax(true);
    applySourceRange(call, cxx_operator_call_expr->getSourceRange());
    return call;
  };

  auto validate_exact_operator_call = [&](SgFunctionCallExp *call,
                                          clang::FunctionDecl *direct_callee,
                                          const char *path) {
    if (call == nullptr || call->get_function() == nullptr ||
        call->get_args() == nullptr) {
      std::cerr << "REX_FRONTEND_INVARIANT[overloaded-operator-semantic-"
                   "path]: incomplete "
                << path << " call" << std::endl;
      ROSE_ABORT();
    }
    SgFunctionDeclaration *expected = nullptr;
    auto semantic_mapping = p_decl_translation_map.find(direct_callee);
    if (semantic_mapping != p_decl_translation_map.end()) {
      SgNode *semantic_node = semantic_mapping->second;
      if (SgTemplateInstantiationDirectiveStatement *directive =
              isSgTemplateInstantiationDirectiveStatement(semantic_node)) {
        semantic_node = directive->get_declaration();
      }
      SgFunctionDeclaration *semantic_function =
          isSgFunctionDeclaration(semantic_node);
      if (semantic_function != nullptr &&
          isSgTemplateFunctionDeclaration(semantic_function) == nullptr) {
        expected = semantic_function;
      }
    }
    if (expected == nullptr) {
      expected = translated_operator_declaration(direct_callee);
    }
    SgFunctionDeclaration *actual = call->getAssociatedFunctionDeclaration();
    if (!declarations_share_redeclaration_chain(expected, actual)) {
      std::cerr << "REX_FRONTEND_INVARIANT[overloaded-operator-semantic-"
                   "path]: "
                << path << " call for Clang declaration '"
                << direct_callee->getQualifiedNameAsString() << "' resolves to "
                << (actual != nullptr ? actual->get_name().getString()
                                      : std::string("<null>"))
                << std::endl;
      ROSE_ABORT();
    }
  };

  auto build_exact_operator_call =
      [&](clang::FunctionDecl *direct_callee) -> SgFunctionCallExp * {
    if (direct_callee == nullptr) {
      std::cerr << "REX_FRONTEND_INVARIANT[lowered-operator-semantics]: "
                   "missing exact operator declaration"
                << std::endl;
      ROSE_ABORT();
    }
    translate_all_operator_args();
    SgFunctionCallExp *semantic_call =
        llvm::isa<clang::CXXMethodDecl>(direct_callee)
            ? build_member_operator_call_expr(direct_callee,
                                              translated_operator_args, false)
            : build_nonmember_operator_call_expr(
                  direct_callee, translated_operator_args, false);
    validate_exact_operator_call(semantic_call, direct_callee,
                                 "direct-lowered");
    return semantic_call;
  };

  auto operator_result_is_callable = [&]() -> bool {
    clang::QualType result_type = cxx_operator_call_expr->getType();
    if (result_type.isNull()) {
      return false;
    }

    const clang::Type *type = result_type.getTypePtrOrNull();
    while (type != nullptr) {
      if (type->isFunctionType() || type->isFunctionPointerType() ||
          type->isMemberFunctionPointerType()) {
        return true;
      }

      if (const clang::ReferenceType *reference_type =
              llvm::dyn_cast<clang::ReferenceType>(type)) {
        reference_type =
            llvm::dyn_cast_or_null<clang::ReferenceType>(reference_type);
        clang::QualType pointee_type = reference_type->getPointeeType();
        type = pointee_type.getTypePtrOrNull();
        continue;
      }

      if (const clang::ParenType *paren_type =
              llvm::dyn_cast<clang::ParenType>(type)) {
        clang::QualType inner_type = paren_type->getInnerType();
        type = inner_type.getTypePtrOrNull();
        continue;
      }

      break;
    }

    return false;
  };

  clang::FunctionDecl *cached_direct_callee = nullptr;
  bool cached_direct_callee_known = false;
  auto direct_operator_callee = [&]() -> clang::FunctionDecl * {
    if (!cached_direct_callee_known) {
      cached_direct_callee = llvm::dyn_cast_or_null<clang::FunctionDecl>(
          cxx_operator_call_expr->getDirectCallee());
      cached_direct_callee_known = true;
    }
    return cached_direct_callee;
  };

  auto exact_operator_surface =
      [&]() -> SgFunctionCallExp::source_operator_surface_enum {
    const unsigned operand_count = cxx_operator_call_expr->getNumArgs();
    auto require_arity = [&](unsigned expected, const char *surface) {
      if (operand_count != expected) {
        std::cerr << "REX_FRONTEND_INVARIANT[operator-source-surface]: "
                  << surface << " requires " << expected
                  << " Clang operands, found " << operand_count << std::endl;
        ROSE_ABORT();
      }
    };
    auto unary_or_binary =
        [&](SgFunctionCallExp::source_operator_surface_enum unary_surface,
            SgFunctionCallExp::source_operator_surface_enum binary_surface,
            const char *surface)
        -> SgFunctionCallExp::source_operator_surface_enum {
      if (operand_count == 1) {
        return unary_surface;
      }
      if (operand_count == 2) {
        return binary_surface;
      }
      std::cerr << "REX_FRONTEND_INVARIANT[operator-source-surface]: "
                << surface << " requires one or two Clang operands, found "
                << operand_count << std::endl;
      ROSE_ABORT();
    };

    switch (cxx_operator_call_expr->getOperator()) {
    case clang::OO_Plus:
      return unary_or_binary(SgFunctionCallExp::e_prefix_plus,
                             SgFunctionCallExp::e_binary_plus, "+");
    case clang::OO_Minus:
      return unary_or_binary(SgFunctionCallExp::e_prefix_minus,
                             SgFunctionCallExp::e_binary_minus, "-");
    case clang::OO_Star:
      return unary_or_binary(SgFunctionCallExp::e_dereference,
                             SgFunctionCallExp::e_binary_multiply, "*");
    case clang::OO_Amp:
      return unary_or_binary(SgFunctionCallExp::e_address_of,
                             SgFunctionCallExp::e_binary_and, "&");
    case clang::OO_PlusPlus:
      if (operand_count == 1) {
        return SgFunctionCallExp::e_prefix_increment;
      }
      require_arity(2, "postfix ++");
      return SgFunctionCallExp::e_postfix_increment;
    case clang::OO_MinusMinus:
      if (operand_count == 1) {
        return SgFunctionCallExp::e_prefix_decrement;
      }
      require_arity(2, "postfix --");
      return SgFunctionCallExp::e_postfix_decrement;
    case clang::OO_Tilde:
      require_arity(1, "~");
      return SgFunctionCallExp::e_bitwise_not;
    case clang::OO_Exclaim:
      require_arity(1, "!");
      return SgFunctionCallExp::e_logical_not;
    case clang::OO_Coawait:
      require_arity(1, "co_await");
      return SgFunctionCallExp::e_co_await;
    case clang::OO_Slash:
      require_arity(2, "/");
      return SgFunctionCallExp::e_binary_divide;
    case clang::OO_Percent:
      require_arity(2, "%");
      return SgFunctionCallExp::e_binary_remainder;
    case clang::OO_Caret:
      require_arity(2, "^");
      return SgFunctionCallExp::e_binary_xor;
    case clang::OO_Pipe:
      require_arity(2, "|");
      return SgFunctionCallExp::e_binary_or;
    case clang::OO_Equal:
      require_arity(2, "=");
      return SgFunctionCallExp::e_binary_assign;
    case clang::OO_Less:
      require_arity(2, "<");
      return SgFunctionCallExp::e_binary_less;
    case clang::OO_Greater:
      require_arity(2, ">");
      return SgFunctionCallExp::e_binary_greater;
    case clang::OO_PlusEqual:
      require_arity(2, "+=");
      return SgFunctionCallExp::e_binary_plus_assign;
    case clang::OO_MinusEqual:
      require_arity(2, "-=");
      return SgFunctionCallExp::e_binary_minus_assign;
    case clang::OO_StarEqual:
      require_arity(2, "*=");
      return SgFunctionCallExp::e_binary_multiply_assign;
    case clang::OO_SlashEqual:
      require_arity(2, "/=");
      return SgFunctionCallExp::e_binary_divide_assign;
    case clang::OO_PercentEqual:
      require_arity(2, "%=");
      return SgFunctionCallExp::e_binary_remainder_assign;
    case clang::OO_CaretEqual:
      require_arity(2, "^=");
      return SgFunctionCallExp::e_binary_xor_assign;
    case clang::OO_AmpEqual:
      require_arity(2, "&=");
      return SgFunctionCallExp::e_binary_and_assign;
    case clang::OO_PipeEqual:
      require_arity(2, "|=");
      return SgFunctionCallExp::e_binary_or_assign;
    case clang::OO_LessLess:
      require_arity(2, "<<");
      return SgFunctionCallExp::e_binary_left_shift;
    case clang::OO_GreaterGreater:
      require_arity(2, ">>");
      return SgFunctionCallExp::e_binary_right_shift;
    case clang::OO_LessLessEqual:
      require_arity(2, "<<=");
      return SgFunctionCallExp::e_binary_left_shift_assign;
    case clang::OO_GreaterGreaterEqual:
      require_arity(2, ">>=");
      return SgFunctionCallExp::e_binary_right_shift_assign;
    case clang::OO_EqualEqual:
      require_arity(2, "==");
      return SgFunctionCallExp::e_binary_equal;
    case clang::OO_ExclaimEqual:
      require_arity(2, "!=");
      return SgFunctionCallExp::e_binary_not_equal;
    case clang::OO_LessEqual:
      require_arity(2, "<=");
      return SgFunctionCallExp::e_binary_less_equal;
    case clang::OO_GreaterEqual:
      require_arity(2, ">=");
      return SgFunctionCallExp::e_binary_greater_equal;
    case clang::OO_Spaceship:
      require_arity(2, "<=>");
      return SgFunctionCallExp::e_binary_spaceship;
    case clang::OO_AmpAmp:
      require_arity(2, "&&");
      return SgFunctionCallExp::e_binary_logical_and;
    case clang::OO_PipePipe:
      require_arity(2, "||");
      return SgFunctionCallExp::e_binary_logical_or;
    case clang::OO_Comma:
      require_arity(2, ",");
      return SgFunctionCallExp::e_binary_comma;
    case clang::OO_ArrowStar:
      require_arity(2, "->*");
      return SgFunctionCallExp::e_binary_arrow_star;
    case clang::OO_Call:
      if (operand_count == 0) {
        std::cerr << "REX_FRONTEND_INVARIANT[operator-source-surface]: call "
                     "operator has no source object"
                  << std::endl;
        ROSE_ABORT();
      }
      return SgFunctionCallExp::e_call_operator_surface;
    case clang::OO_Subscript:
      if (operand_count < 2) {
        std::cerr << "REX_FRONTEND_INVARIANT[operator-source-surface]: "
                     "subscript operator lacks an object or subscript"
                  << std::endl;
        ROSE_ABORT();
      }
      return SgFunctionCallExp::e_subscript_operator_surface;
    case clang::OO_Arrow:
      require_arity(1, "->");
      return SgFunctionCallExp::e_arrow_operator_surface;
    case clang::OO_None:
    case clang::OO_New:
    case clang::OO_Delete:
    case clang::OO_Array_New:
    case clang::OO_Array_Delete:
    case clang::OO_Conditional:
    case clang::NUM_OVERLOADED_OPERATORS:
      std::cerr << "REX_FRONTEND_INVARIANT[operator-source-surface]: "
                   "unsupported CXXOperatorCallExpr operator kind="
                << static_cast<int>(cxx_operator_call_expr->getOperator())
                << std::endl;
      ROSE_ABORT();
    }
    ROSE_ABORT();
  };

  auto publish_operator_surface = [&](SgFunctionCallExp *call,
                                      clang::FunctionDecl *direct_callee) {
    if (call == nullptr || call->get_function() == nullptr ||
        call->get_args() == nullptr) {
      std::cerr << "REX_FRONTEND_INVARIANT[operator-source-surface]: "
                   "operator call has incomplete Sage ownership"
                << std::endl;
      ROSE_ABORT();
    }

    if (has_explicit_template_args) {
      call->set_uses_operator_syntax(false);
      call->set_source_operator_surface(
          SgFunctionCallExp::e_no_operator_surface);
      call->set_source_operator_callee_form(
          SgFunctionCallExp::e_no_operator_callee_form);
      call->set_source_operator_operand_roles(SgUnsignedCharList{});
      return;
    }

    const auto surface = exact_operator_surface();
    const bool member_callee = isSgDotExp(call->get_function()) != nullptr ||
                               isSgArrowExp(call->get_function()) != nullptr;
    if (direct_callee != nullptr &&
        member_callee != llvm::isa<clang::CXXMethodDecl>(direct_callee)) {
      std::cerr << "REX_FRONTEND_INVARIANT[operator-source-surface]: exact "
                   "Clang and Sage callee forms disagree"
                << std::endl;
      ROSE_ABORT();
    }

    if (cxx_operator_call_expr->getNumArgs() < (member_callee ? 1u : 0u)) {
      std::cerr << "REX_FRONTEND_INVARIANT[operator-source-operands]: member "
                   "operator has no exact source object"
                << std::endl;
      ROSE_ABORT();
    }
    const size_t expected_sage_arguments =
        cxx_operator_call_expr->getNumArgs() - (member_callee ? 1u : 0u);
    if (call->get_args()->get_expressions().size() != expected_sage_arguments) {
      std::cerr << "REX_FRONTEND_INVARIANT[operator-source-operands]: Sage "
                   "argument ownership does not match exact Clang operands"
                << std::endl;
      ROSE_ABORT();
    }

    call->set_uses_operator_syntax(true);
    call->set_source_operator_surface(surface);
    call->set_source_operator_callee_form(
        member_callee ? SgFunctionCallExp::e_member_operator_callee
                      : SgFunctionCallExp::e_nonmember_operator_callee);
    SgUnsignedCharList roles;
    const bool postfix = surface == SgFunctionCallExp::e_postfix_increment ||
                         surface == SgFunctionCallExp::e_postfix_decrement;
    for (size_t i = 0; i < expected_sage_arguments; ++i) {
      const bool semantic_operand =
          (postfix && i + 1 == expected_sage_arguments) ||
          isSgCxxDefaultInitExp(call->get_args()->get_expressions()[i]) !=
              nullptr;
      roles.push_back(semantic_operand
                          ? SgFunctionCallExp::e_semantic_operator_operand
                          : SgFunctionCallExp::e_source_operator_operand);
    }
    call->set_source_operator_operand_roles(roles);
  };

  // Clang inserts an OO_Arrow call as the base of the surrounding MemberExpr.
  // Keep that exact call in the Sage tree: dropping it loses both recursive
  // operator-> semantics and the call-graph edge.  The operator-syntax flag
  // lets the unparser retain the source spelling.
  if (cxx_operator_call_expr->getOperator() == clang::OO_Arrow) {
    if (cxx_operator_call_expr->getNumArgs() != 1) {
      std::cerr << "REX_FRONTEND_INVARIANT[overloaded-arrow-arity]: expected "
                   "exactly one implicit object argument, got "
                << cxx_operator_call_expr->getNumArgs() << std::endl;
      ROSE_ABORT();
    }
    clang::FunctionDecl *direct_callee = direct_operator_callee();
    if (!llvm::isa_and_nonnull<clang::CXXMethodDecl>(direct_callee)) {
      std::cerr << "REX_FRONTEND_INVARIANT[overloaded-arrow-callee]: no exact "
                   "member operator declaration"
                << std::endl;
      ROSE_ABORT();
    }
    translate_all_operator_args();
    SgFunctionCallExp *call = build_member_operator_call_expr(
        direct_callee, translated_operator_args, false);
    if (!call->get_uses_operator_syntax()) {
      std::cerr << "REX_FRONTEND_INVARIANT[overloaded-arrow-syntax]: exact "
                   "call lacks operator-syntax metadata"
                << std::endl;
      ROSE_ABORT();
    }
    validate_exact_operator_call(call, direct_callee, "operator-arrow");
    publish_operator_surface(call, direct_callee);
    *node = call;
    return VisitExpr(cxx_operator_call_expr, node);
  }

  if (!has_explicit_template_args && !preserve_unary_deref_call &&
      !operator_result_is_callable()) {
    if (clang::FunctionDecl *direct_callee = direct_operator_callee()) {
      SgFunctionCallExp *call = build_exact_operator_call(direct_callee);
      publish_operator_surface(call, direct_callee);
      *node = call;
      return VisitExpr(cxx_operator_call_expr, node);
    }
  }

  // C++ overloaded operators (operator+, operator[], etc.) are represented as
  // function calls. For member operators, Clang models the implicit object as
  // the first argument, while ROSE represents it as a dot/arrow expression in
  // the call's callee. Convert member-operator calls into the ROSE form to
  // preserve correct operator arity and unparsing.
  if (clang::FunctionDecl *direct_callee = direct_operator_callee()) {
    if (llvm::isa<clang::CXXMethodDecl>(direct_callee) &&
        cxx_operator_call_expr->getNumArgs() > 0) {
      translate_all_operator_args();
      SgFunctionCallExp *call = build_member_operator_call_expr(
          direct_callee, translated_operator_args, false);
      validate_exact_operator_call(call, direct_callee, "direct-member");
      publish_operator_surface(call, direct_callee);
      *node = call;
      return VisitExpr(cxx_operator_call_expr, node);
    }
  }

  // Non-member overloaded operators are modeled as regular calls where all
  // operands appear as explicit arguments.
  if (!VisitCallExpr(cxx_operator_call_expr, node))
    return false;

  if (*node != nullptr) {
    SgFunctionCallExp *funcCall = isSgFunctionCallExp(*node);
    if (funcCall != nullptr) {
      clang::FunctionDecl *direct_callee = direct_operator_callee();
      publish_operator_surface(funcCall, direct_callee);
      if (direct_callee != nullptr) {
        validate_exact_operator_call(funcCall, direct_callee,
                                     "direct-nonmember");
      }
    } else if (direct_operator_callee() != nullptr) {
      std::cerr << "REX_FRONTEND_INVARIANT[overloaded-operator-semantic-path]: "
                   "direct callee did not produce a function call"
                << std::endl;
      ROSE_ABORT();
    }
  }

  return true;
}

bool ClangToSageTranslator::VisitUserDefinedLiteral(
    clang::UserDefinedLiteral *user_defined_literal, SgNode **node) {
  std::string suffix_spelling;
  if (const clang::IdentifierInfo *suffix =
          user_defined_literal->getUDSuffix()) {
    suffix_spelling = suffix->getName().str();
  }
  if (suffix_spelling.empty()) {
    std::cerr << "REX_FRONTEND_INVARIANT[user-defined-literal-spelling]: "
                 "literal has no exact suffix identity"
              << std::endl;
    ROSE_ABORT();
  }

  const clang::UserDefinedLiteral::LiteralOperatorKind literal_kind =
      user_defined_literal->getLiteralOperatorKind();
  std::size_t first_token =
      requireExpandedTokenSourceOrder(user_defined_literal->getBeginLoc(),
                                      "user-defined-literal-begin") -
      1;
  std::size_t last_token =
      requireExpandedTokenSourceOrder(user_defined_literal->getEndLoc(),
                                      "user-defined-literal-end") -
      1;
  ROSE_ASSERT(first_token <= last_token &&
              last_token < p_expanded_tokens.size());
  SgCxxLiteralToken::token_kind_enum token_kind;
  switch (literal_kind) {
  case clang::UserDefinedLiteral::LOK_String:
    token_kind = SgCxxLiteralToken::e_string_token;
    break;
  case clang::UserDefinedLiteral::LOK_Template:
    token_kind = clang::tok::isStringLiteral(p_expanded_tokens[last_token].kind)
                     ? SgCxxLiteralToken::e_string_token
                     : SgCxxLiteralToken::e_numeric_token;
    break;
  case clang::UserDefinedLiteral::LOK_Character:
    token_kind = SgCxxLiteralToken::e_character_token;
    break;
  case clang::UserDefinedLiteral::LOK_Raw:
  case clang::UserDefinedLiteral::LOK_Floating:
  case clang::UserDefinedLiteral::LOK_Integer:
    token_kind = SgCxxLiteralToken::e_numeric_token;
    break;
  default:
    ROSE_ABORT();
  }
  if (token_kind == SgCxxLiteralToken::e_string_token) {
    while (first_token > 0 &&
           clang::tok::isStringLiteral(p_expanded_tokens[first_token - 1].kind))
      --first_token;
    while (last_token + 1 < p_expanded_tokens.size() &&
           clang::tok::isStringLiteral(p_expanded_tokens[last_token + 1].kind))
      ++last_token;
  } else {
    ROSE_ASSERT(first_token == last_token);
  }

  // Preserve normal CallExpr semantic translation, but do not let Clang's
  // reused UDL token range turn the call's internal callee or arguments into
  // lexical owners.  The enclosing SgFunctionCallExp and its separate source
  // operand list own the written literal; every ordinary CallExpr component is
  // constructed as canonical frontend semantic state.
  clang::Expr *cooked_literal = nullptr;
  if (literal_kind != clang::UserDefinedLiteral::LOK_Raw &&
      literal_kind != clang::UserDefinedLiteral::LOK_Template) {
    cooked_literal = user_defined_literal->getCookedLiteral();
    if (cooked_literal == nullptr) {
      std::cerr << "REX_FRONTEND_INVARIANT[user-defined-literal-operand]: "
                   "cooked literal kind has no semantic source operand"
                << std::endl;
      ROSE_ABORT();
    }
    cooked_literal = cooked_literal->IgnoreParenImpCasts();
  }

  std::vector<const clang::Expr *> active_udl_literals;
  auto publish_udl_literal = [&](const clang::Expr *literal,
                                 const std::string &spelling,
                                 bool compiler_generated) {
    if (literal == nullptr ||
        !p_exact_literal_operand_spellings
             .emplace(literal, ExactLiteralOperandSpelling{spelling, false,
                                                           compiler_generated})
             .second) {
      std::cerr << "REX_FRONTEND_INVARIANT[user-defined-literal-session]: "
                   "literal already has an active operand role"
                << std::endl;
      ROSE_ABORT();
    }
    active_udl_literals.push_back(literal);
  };
  bool published_cooked_literal = cooked_literal == nullptr;
  for (auto argument = user_defined_literal->arg_begin();
       argument != user_defined_literal->arg_end(); ++argument) {
    clang::Expr *semantic_literal = (*argument)->IgnoreParenImpCasts();
    if (semantic_literal == cooked_literal) {
      published_cooked_literal = true;
    }
    if (!llvm::isa<clang::IntegerLiteral, clang::FloatingLiteral,
                   clang::CharacterLiteral, clang::StringLiteral>(
            semantic_literal)) {
      std::cerr << "REX_FRONTEND_INVARIANT[user-defined-literal-session]: "
                   "synthesized argument is not an exact literal value"
                << std::endl;
      ROSE_ABORT();
    }
    publish_udl_literal(semantic_literal, "", true);
  }
  if (!published_cooked_literal) {
    std::cerr << "REX_FRONTEND_INVARIANT[user-defined-literal-session]: "
                 "cooked literal is not owned by the semantic call arguments"
              << std::endl;
    ROSE_ABORT();
  }
  {
    SemanticExpressionConstruction semantic_call(
        p_user_defined_literal_semantic_call_depth,
        "VisitUserDefinedLiteral:semantic-call");
    if (!VisitCallExpr(user_defined_literal, node)) {
      std::cerr << "REX_FRONTEND_INVARIANT[user-defined-literal-call]: "
                   "semantic call translation failed"
                << std::endl;
      ROSE_ABORT();
    }
  }
  for (const clang::Expr *literal : active_udl_literals) {
    auto active = p_exact_literal_operand_spellings.find(literal);
    if (active == p_exact_literal_operand_spellings.end() ||
        !active->second.consumed) {
      std::cerr << "REX_FRONTEND_INVARIANT[user-defined-literal-session]: "
                   "active operand role was not consumed deterministically"
                << std::endl;
      ROSE_ABORT();
    }
    p_exact_literal_operand_spellings.erase(active);
  }

  SgFunctionCallExp *call = isSgFunctionCallExp(*node);
  if (call == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[user-defined-literal-call]: Clang "
                 "literal did not translate to SgFunctionCallExp"
              << std::endl;
    ROSE_ABORT();
  }

  SgFunctionDeclaration *literal_operator =
      call->getAssociatedFunctionDeclaration();
  if (literal_operator == nullptr ||
      !literal_operator->get_specialFunctionModifier().isUldOperator()) {
    SgFunctionDeclaration *first =
        literal_operator != nullptr
            ? isSgFunctionDeclaration(
                  literal_operator->get_firstNondefiningDeclaration())
            : nullptr;
    SgFunctionDeclaration *defining =
        literal_operator != nullptr
            ? isSgFunctionDeclaration(
                  literal_operator->get_definingDeclaration())
            : nullptr;
    std::cerr
        << "REX_FRONTEND_INVARIANT[user-defined-literal-declaration]: "
           "literal call declaration="
        << static_cast<void *>(literal_operator) << " name="
        << (literal_operator != nullptr
                ? literal_operator->get_name().getString()
                : std::string("<null>"))
        << " first=" << static_cast<void *>(first)
        << " defining=" << static_cast<void *>(defining) << " UDL roles="
        << (literal_operator != nullptr &&
                    literal_operator->get_specialFunctionModifier()
                        .isUldOperator()
                ? 1
                : 0)
        << "/"
        << (first != nullptr &&
                    first->get_specialFunctionModifier().isUldOperator()
                ? 1
                : 0)
        << "/"
        << (defining != nullptr &&
                    defining->get_specialFunctionModifier().isUldOperator()
                ? 1
                : 0)
        << " was not classified by the declaration translator" << std::endl;
    ROSE_ABORT();
  }

  // These are lexical tokens, not value expressions. Raw numeric spellings
  // have no builtin value, and concatenated string tokens do not individually
  // have the cooked aggregate's type or decoded payload.
  SgCxxLiteralTokenList *lexical_operands = new SgCxxLiteralTokenList();
  applySourceRange(lexical_operands, user_defined_literal->getSourceRange());
  SgUnsignedCharList suffix_roles;

  auto &source_manager = p_compiler_instance->getSourceManager();
  const auto &language = p_compiler_instance->getLangOpts();
  for (std::size_t index = first_token; index <= last_token; ++index) {
    const clang::Token &source_token = p_expanded_tokens[index].token;
    const bool exact_kind =
        token_kind == SgCxxLiteralToken::e_string_token
            ? clang::tok::isStringLiteral(source_token.getKind())
        : token_kind == SgCxxLiteralToken::e_numeric_token
            ? source_token.is(clang::tok::numeric_constant)
            : source_token.isOneOf(clang::tok::char_constant,
                                   clang::tok::wide_char_constant,
                                   clang::tok::utf8_char_constant,
                                   clang::tok::utf16_char_constant,
                                   clang::tok::utf32_char_constant);
    if (!exact_kind)
      failExactExpressionTranslation("user-defined-literal-token-kind",
                                     user_defined_literal);
    bool invalid = false;
    std::string spelling = clang::Lexer::getSpelling(
        source_token, source_manager, language, &invalid);
    if (invalid || spelling.empty())
      failExactExpressionTranslation("user-defined-literal-token-spelling",
                                     user_defined_literal);
    const bool owns_suffix = token_kind == SgCxxLiteralToken::e_numeric_token ||
                             source_token.hasUDSuffix();
    ROSE_ASSERT(token_kind == SgCxxLiteralToken::e_string_token || owns_suffix);
    auto *lexical_token = new SgCxxLiteralToken(token_kind, spelling);
    lexical_token->validate_literal_token();
    applySourceRange(lexical_token,
                     clang::SourceRange(source_token.getLocation(),
                                        source_token.getLocation()));
    lexical_operands->get_tokens().push_back(lexical_token);
    lexical_token->set_parent(lexical_operands);
    suffix_roles.push_back(
        owns_suffix
            ? SgFunctionCallExp::e_user_defined_literal_token_with_suffix
            : SgFunctionCallExp::e_user_defined_literal_token_without_suffix);
  }

  if (lexical_operands->get_tokens().empty() ||
      lexical_operands->get_tokens().size() != suffix_roles.size() ||
      std::none_of(
          suffix_roles.begin(), suffix_roles.end(), [](unsigned char role) {
            return role ==
                   SgFunctionCallExp::e_user_defined_literal_token_with_suffix;
          })) {
    std::cerr << "REX_FRONTEND_INVARIANT[user-defined-literal-surface]: "
                 "lexical token/suffix ownership is incomplete"
              << std::endl;
    ROSE_ABORT();
  }

  SgExprListExp *final_arguments = call->get_args();
  if (final_arguments == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[user-defined-literal-arguments]: "
                 "literal call has no semantic argument list"
              << std::endl;
    ROSE_ABORT();
  }
  auto require_exact_semantic_component = [&](SgNode *root, const char *role) {
    if (root == nullptr || role == nullptr) {
      std::cerr << "REX_FRONTEND_INVARIANT[user-defined-literal-semantic-"
                   "owner]: semantic component has no exact producer"
                << std::endl;
      ROSE_ABORT();
    }
    std::unordered_set<SgNode *> visited;
    std::vector<SgNode *> worklist{root};
    while (!worklist.empty()) {
      SgNode *current = worklist.back();
      worklist.pop_back();
      if (current == nullptr || !visited.insert(current).second) {
        continue;
      }
      if (SgLocatedNode *located = isSgLocatedNode(current)) {
        std::vector<Sg_File_Info *> positions{located->get_file_info(),
                                              located->get_startOfConstruct(),
                                              located->get_endOfConstruct()};
        if (SgExpression *expression = isSgExpression(located)) {
          positions.push_back(expression->get_operatorPosition());
        }
        const bool exact_semantic =
            p_synthesized_source_nodes.count(current) == 1 &&
            p_lexical_source_nodes.count(current) == 0 &&
            std::all_of(
                positions.begin(), positions.end(),
                [&](Sg_File_Info *position) {
                  return position != nullptr &&
                         position->get_parent() == current &&
                         !position->isShared() &&
                         position->isCompilerGenerated() &&
                         position->isFrontendSpecific() &&
                         !position->isTransformation() &&
                         !position->isSourcePositionUnavailableInFrontend() &&
                         position->isOutputInCodeGeneration() &&
                         position->get_file_id() ==
                             Sg_File_Info::COMPILER_GENERATED_FILE_ID &&
                         position->get_physical_file_id() ==
                             Sg_File_Info::COMPILER_GENERATED_FILE_ID;
                });
        if (!exact_semantic) {
          std::cerr
              << "REX_FRONTEND_INVARIANT[user-defined-literal-semantic-owner]: "
              << role << " component=" << static_cast<void *>(current) << "/"
              << current->class_name()
              << " was not generated by one exact semantic producer"
              << std::endl;
          ROSE_ABORT();
        }
        if (SgValueExp *value = isSgValueExp(located);
            value != nullptr && value->get_literal_spelling_form() !=
                                    SgValueExp::e_literal_canonical_generated) {
          std::cerr << "REX_FRONTEND_INVARIANT[user-defined-literal-semantic-"
                       "spelling]: semantic value retained a lexical literal "
                       "surface"
                    << std::endl;
          ROSE_ABORT();
        }
      }
      for (SgNode *child : current->get_traversalSuccessorContainer()) {
        if (child != nullptr && child->get_parent() == current) {
          worklist.push_back(child);
        }
      }
    }
  };
  require_exact_semantic_component(call->get_function(), "callee");
  require_exact_semantic_component(final_arguments, "arguments");

  call->set_source_user_defined_literal_suffix(SgName(suffix_spelling));
  call->set_uses_operator_syntax(true);
  call->set_source_operator_surface(
      SgFunctionCallExp::e_user_defined_literal_surface);
  call->set_source_operator_callee_form(
      SgFunctionCallExp::e_nonmember_operator_callee);
  call->set_source_user_defined_literal_operands(lexical_operands);
  lexical_operands->set_parent(call);
  call->set_source_user_defined_literal_suffix_roles(suffix_roles);
  SgUnsignedCharList operand_roles;
  for (size_t i = 0; i < final_arguments->get_expressions().size(); ++i) {
    operand_roles.push_back(SgFunctionCallExp::e_semantic_operator_operand);
  }
  call->set_source_operator_operand_roles(operand_roles);

  return VisitExpr(user_defined_literal, node);
}

SgCastExp::semantic_conversion_kind_enum
ClangToSageTranslator::translateClangCastKind(clang::CastKind kind) const {
  switch (kind) {
#define CAST_OPERATION(Name)                                                   \
  case clang::CK_##Name:                                                       \
    return SgCastExp::e_semantic_conversion_##Name;
#include <clang/AST/OperationKinds.def>
#undef CAST_OPERATION
  }
  fprintf(stderr,
          "REX_FRONTEND_INVARIANT[semantic-conversion-kind]: unsupported "
          "LLVM 22 CastKind=%d\n",
          static_cast<int>(kind));
  ROSE_ABORT();
}

SgCastExp::value_category_enum
ClangToSageTranslator::translateClangValueCategory(
    clang::ExprValueKind kind) const {
  switch (kind) {
  case clang::VK_LValue:
    return SgCastExp::e_value_category_lvalue;
  case clang::VK_XValue:
    return SgCastExp::e_value_category_xvalue;
  case clang::VK_PRValue:
    return SgCastExp::e_value_category_prvalue;
  }
  fprintf(stderr,
          "REX_FRONTEND_INVARIANT[cast-value-category]: unsupported Clang "
          "ExprValueKind=%d\n",
          static_cast<int>(kind));
  ROSE_ABORT();
}

SgTypePtrList
ClangToSageTranslator::translateCastBasePath(const clang::CastExpr &cast) {
  SgTypePtrList path;
  for (const clang::CXXBaseSpecifier *base : cast.path()) {
    if (base == nullptr || base->getType().isNull()) {
      std::cerr << "REX_FRONTEND_INVARIANT[cast-base-path]: cast kind="
                << cast.getCastKindName() << " has no exact base type"
                << std::endl;
      ROSE_ABORT();
    }
    // Conversion paths identify semantic classes, including typedef bases.
    const clang::QualType canonical_type = base->getType().getCanonicalType();
    SgType *type = buildTypeFromQualifiedType(canonical_type);
    SgType *stripped =
        type != nullptr ? type->stripTypedefsAndModifiers() : nullptr;
    if (SageInterface::containsUnknownType(type) ||
        isSgClassType(stripped) == nullptr) {
      std::cerr << "REX_FRONTEND_INVARIANT[cast-base-path]: cast kind="
                << cast.getCastKindName()
                << " base=" << base->getType().getAsString()
                << " canonical=" << canonical_type.getAsString()
                << " does not translate to one exact class identity"
                << std::endl;
      ROSE_ABORT();
    }
    path.push_back(type);
  }
  return path;
}

bool ClangToSageTranslator::VisitExplicitCastExpr(
    clang::ExplicitCastExpr *explicit_cast_expr, SgNode **node) {
  if (auto *functional =
          llvm::dyn_cast<clang::CXXFunctionalCastExpr>(explicit_cast_expr)) {
    return VisitCXXFunctionalCastExpr(functional, node);
  }

  const bool semantic_cast =
      hasExactSemanticExpressionConstructionRole("VisitExplicitCastExpr");
  clang::TypeLoc written_type_loc;
  SgStringList qualifier_tokens;
  bool global_qualification = false;
  SgNonrealDecl::source_elaboration_kind_enum source_elaboration_kind =
      SgNonrealDecl::e_source_elaboration_unspecified;
  {
    clang::TypeSourceInfo *written_type_info =
        explicit_cast_expr->getTypeInfoAsWritten();
    if (written_type_info == nullptr) {
      std::cerr
          << "REX_FRONTEND_INVARIANT[cast-source-qualification]: explicit "
             "cast has no exact written TypeLoc"
          << std::endl;
      ROSE_ABORT();
    }
    written_type_loc = written_type_info->getTypeLoc();
    (void)exactTypeLocSourceQualification(written_type_loc, qualifier_tokens,
                                          global_qualification);
    source_elaboration_kind =
        exactTypeLocSourceElaborationKind(written_type_loc);
  }

  SgType *source_type = nullptr;
  clang::TagDecl *cast_defining_tag_decl = nullptr;
  SgDeclarationStatement *cast_tag_definition = nullptr;
  if (!semantic_cast) {
    cast_defining_tag_decl = definingTagFromTypeLoc(written_type_loc);

    if (cast_defining_tag_decl != nullptr) {
      beginTypedOwnerTagConstruction(cast_defining_tag_decl,
                                     SageBuilder::topScopeStack(),
                                     "VisitExplicitCastExpr");
    }
  }
  source_type = semantic_cast ? buildSemanticTypeFromQualifiedType(
                                    written_type_loc.getType(),
                                    "explicit-cast-semantic-written-type")
                              : buildTypeFromTypeLoc(written_type_loc);
  if (cast_defining_tag_decl != nullptr) {
    cast_tag_definition = finishTypedOwnerTagConstruction(
        cast_defining_tag_decl, "VisitExplicitCastExpr");
  }
  if (source_type == nullptr ||
      SageInterface::containsUnknownType(source_type)) {
    std::cerr << "REX_FRONTEND_INVARIANT[cast-written-type]: explicit cast "
                 "has no exact translated TypeLoc"
              << std::endl;
    ROSE_ABORT();
  }
  SgType *semantic_type = buildSemanticTypeFromQualifiedType(
      explicit_cast_expr->getType().getCanonicalType(),
      "explicit-cast-semantic-type");
  if (SageInterface::containsUnknownType(semantic_type)) {
    std::cerr << "REX_FRONTEND_INVARIANT[cast-semantic-type]: explicit cast "
                 "has no exact semantic result type"
              << std::endl;
    ROSE_ABORT();
  }
  clang::Expr *sub_expr = explicit_cast_expr->getSubExpr();
  SgNode *tmp_expr = Traverse(sub_expr);
  SgExpression *sg_expr = isSgExpression(tmp_expr);
  ROSE_ASSERT(sg_expr != nullptr);

  SgCastExp::cast_type_enum cast_kind = SgCastExp::e_C_style_cast;
  if (llvm::isa<clang::BuiltinBitCastExpr>(explicit_cast_expr)) {
    cast_kind = SgCastExp::e_builtin_bit_cast;
  } else if (llvm::isa<clang::CXXStaticCastExpr>(explicit_cast_expr)) {
    cast_kind = SgCastExp::e_static_cast;
  } else if (llvm::isa<clang::CXXConstCastExpr>(explicit_cast_expr)) {
    cast_kind = SgCastExp::e_const_cast;
  } else if (llvm::isa<clang::CXXDynamicCastExpr>(explicit_cast_expr)) {
    cast_kind = SgCastExp::e_dynamic_cast;
  } else if (llvm::isa<clang::CXXReinterpretCastExpr>(explicit_cast_expr)) {
    cast_kind = SgCastExp::e_reinterpret_cast;
  }

  const SgCastExp::semantic_conversion_kind_enum semantic_kind =
      translateClangCastKind(explicit_cast_expr->getCastKind());
  const SgCastExp::value_category_enum value_category =
      translateClangValueCategory(explicit_cast_expr->getValueKind());
  const SgTypePtrList conversion_base_path =
      translateCastBasePath(*explicit_cast_expr);

  SgCastExp *cast_exp = SageBuilder::buildCastExp_nfi(
      sg_expr, semantic_type, cast_kind, semantic_kind, value_category,
      conversion_base_path, source_type);
  cast_exp->set_explicit_name_qualification_present(true);
  cast_exp->set_explicit_global_qualification(global_qualification);
  cast_exp->set_explicit_name_qualification_tokens(qualifier_tokens);
  cast_exp->set_source_type_elaboration_kind(source_elaboration_kind);
  cast_exp->set_type_elaboration_required(
      source_elaboration_kind != SgNonrealDecl::e_source_elaboration_none);
  if (cast_defining_tag_decl != nullptr) {
    SgDeclarationStatement *defining_tag = adopt_typed_owner_tag_definition(
        cast_exp, cast_tag_definition, source_type, "VisitExplicitCastExpr");
    cast_exp->set_type_defining_declaration(defining_tag);
  } else if (cast_exp->get_type_defining_declaration() != nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[cast-inline-tag-owner]: "
                 "non-defining cast has stale defining-tag ownership"
              << std::endl;
    ROSE_ABORT();
  }
  if (semantic_cast)
    publishCanonicalSemanticExpressionSourceProvenance(
        cast_exp, "VisitExplicitCastExpr:semantic-conversion");
  *node = cast_exp;

  return VisitExpr(explicit_cast_expr, node);
}

bool ClangToSageTranslator::VisitBuiltinBitCastExpr(
    clang::BuiltinBitCastExpr *builtin_bit_cast_expr, SgNode **node) {
  return VisitExplicitCastExpr(builtin_bit_cast_expr, node);
}

bool ClangToSageTranslator::VisitCStyleCastExpr(
    clang::CStyleCastExpr *c_style_cast, SgNode **node) {
  return VisitExplicitCastExpr(c_style_cast, node);
}

bool ClangToSageTranslator::VisitCXXFunctionalCastExpr(
    clang::CXXFunctionalCastExpr *cxx_functional_cast_expr, SgNode **node) {
  const bool semantic_cast =
      hasExactSemanticExpressionConstructionRole("VisitCXXFunctionalCastExpr");
  clang::TypeLoc source_type_loc;
  {
    clang::TypeSourceInfo *written_type_info =
        cxx_functional_cast_expr->getTypeInfoAsWritten();
    source_type_loc = written_type_info != nullptr
                          ? written_type_info->getTypeLoc()
                          : clang::TypeLoc();
    if (source_type_loc.isNull()) {
      std::cerr
          << "REX_FRONTEND_INVARIANT[functional-cast-source-"
             "qualification]: source-written functional cast has no exact "
             "TypeLoc"
          << std::endl;
      ROSE_ABORT();
    }
  }

  clang::Expr *sub_expr = cxx_functional_cast_expr->getSubExpr();
  if (sub_expr == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[functional-cast-operand]: Clang "
                 "functional cast has no exact semantic subexpression"
              << std::endl;
    ROSE_ABORT();
  }
  SgExpression *semantic_operand = isSgExpression(Traverse(sub_expr));
  if (semantic_operand == nullptr ||
      semantic_operand->get_parent() != nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[functional-cast-operand]: translated "
                 "semantic subexpression is missing or already owned"
              << std::endl;
    ROSE_ABORT();
  }

  SgType *source_type =
      semantic_cast
          ? buildSemanticTypeFromQualifiedType(
                source_type_loc.getType(),
                "functional-cast-semantic-written-type")
          : buildExplicitTemplateIdTypeUseFromTypeLoc(
                source_type_loc, "VisitCXXFunctionalCastExpr:source-type");
  if (SageInterface::containsUnknownType(source_type)) {
    std::cerr << "REX_FRONTEND_INVARIANT[functional-cast-type]: functional "
                 "cast has no exact written result type"
              << std::endl;
    ROSE_ABORT();
  }
  SgType *semantic_type = buildSemanticTypeFromQualifiedType(
      cxx_functional_cast_expr->getType().getCanonicalType(),
      "functional-cast-semantic-type");
  if (SageInterface::containsUnknownType(semantic_type)) {
    std::cerr << "REX_FRONTEND_INVARIANT[functional-cast-type]: functional "
                 "cast has no exact semantic result type"
              << std::endl;
    ROSE_ABORT();
  }

  const SgCastExp::semantic_conversion_kind_enum semantic_kind =
      translateClangCastKind(cxx_functional_cast_expr->getCastKind());
  const SgCastExp::value_category_enum value_category =
      translateClangValueCategory(cxx_functional_cast_expr->getValueKind());
  const SgTypePtrList conversion_base_path =
      translateCastBasePath(*cxx_functional_cast_expr);

  SgStringList qualifier_tokens;
  bool global_qualification = false;
  SgNonrealDecl::source_elaboration_kind_enum source_elaboration_kind =
      SgNonrealDecl::e_source_elaboration_unspecified;
  (void)exactTypeLocSourceQualification(source_type_loc, qualifier_tokens,
                                        global_qualification);
  if (!semantic_cast) {
    transferWrittenNonrealQualifierToUseSite(source_type, qualifier_tokens,
                                             global_qualification,
                                             "VisitCXXFunctionalCastExpr");
  }
  source_elaboration_kind = exactTypeLocSourceElaborationKind(source_type_loc);

  const SgCastExp::cast_type_enum source_surface =
      cxx_functional_cast_expr->isListInitialization()
          ? SgCastExp::e_functional_list_cast
          : SgCastExp::e_functional_cast;

  ROSE_ASSERT(isSgExprListExp(semantic_operand) == nullptr);

  SgCastExp *cast = SageBuilder::buildCastExp_nfi(
      semantic_operand, semantic_type, source_surface, semantic_kind,
      value_category, conversion_base_path, source_type);
  cast->set_explicit_name_qualification_present(true);
  cast->set_explicit_global_qualification(global_qualification);
  cast->set_explicit_name_qualification_tokens(qualifier_tokens);
  cast->set_source_type_elaboration_kind(source_elaboration_kind);
  cast->set_type_elaboration_required(source_elaboration_kind !=
                                      SgNonrealDecl::e_source_elaboration_none);
  if (semantic_cast)
    publishCanonicalSemanticExpressionSourceProvenance(
        cast, "VisitCXXFunctionalCastExpr:semantic-conversion");
  *node = cast;

  return VisitExpr(cxx_functional_cast_expr, node);
}

bool ClangToSageTranslator::VisitCXXAddrspaceCastExpr(
    clang::CXXAddrspaceCastExpr *cxx_addrspace_cast_expr, SgNode **node) {
  (void)node;
  fprintf(stderr,
          "REX_FRONTEND_INVARIANT[unsupported-cast-surface]: "
          "CXXAddrspaceCastExpr cast-kind=%s has no exact REX "
          "addrspace_cast source surface\n",
          cxx_addrspace_cast_expr != nullptr
              ? cxx_addrspace_cast_expr->getCastKindName()
              : "<null>");
  ROSE_ABORT();
}

bool ClangToSageTranslator::VisitImplicitCastExpr(
    clang::ImplicitCastExpr *implicit_cast_expr, SgNode **node) {
  SgNode *tmp_expr = Traverse(implicit_cast_expr->getSubExpr());
  SgExpression *expr = isSgExpression(tmp_expr);

  ROSE_ASSERT(expr != nullptr);

  // Every Clang implicit conversion has semantic target-type and value-category
  // meaning. Materialize every CastKind with the dedicated transparent role;
  // passing through selected kinds leaves a child with the wrong type and asks
  // downstream analyses to reconstruct frontend semantics.
  const SgCastExp::semantic_conversion_kind_enum conversion_kind =
      translateClangCastKind(implicit_cast_expr->getCastKind());
  SgType *target_type = nullptr;
  if (conversion_kind ==
      SgCastExp::e_semantic_conversion_FunctionToPointerDecay) {
    const clang::QualType clang_target = implicit_cast_expr->getType();
    const clang::Type *clang_target_type = clang_target.getTypePtrOrNull();
    const clang::PointerType *clang_pointer =
        llvm::dyn_cast_or_null<clang::PointerType>(clang_target_type);
    SgType *source_type = expr->get_type();
    SgType *source_value_type = source_type != nullptr
                                    ? source_type->stripTypedefsAndModifiers()
                                    : nullptr;
    if (SgReferenceType *reference = isSgReferenceType(source_value_type)) {
      source_value_type = reference->get_base_type();
    } else if (SgRvalueReferenceType *reference =
                   isSgRvalueReferenceType(source_value_type)) {
      source_value_type = reference->get_base_type();
    }
    SgFunctionType *source_function =
        source_value_type != nullptr
            ? isSgFunctionType(source_value_type->stripTypedefsAndModifiers())
            : nullptr;
    const clang::QualType clang_source =
        implicit_cast_expr->getSubExpr()->getType();
    const clang::QualType clang_pointee = clang_pointer != nullptr
                                              ? clang_pointer->getPointeeType()
                                              : clang::QualType();
    const bool exact_clang_decay =
        clang_pointer != nullptr && !clang_pointee.isNull() &&
        clang_pointee->isFunctionType() && !clang_source.isNull() &&
        clang_source->isFunctionType() &&
        p_compiler_instance->getASTContext().hasSameType(clang_source,
                                                         clang_pointee);
    if (clang_pointer == nullptr || !exact_clang_decay ||
        source_function == nullptr) {
      std::cerr
          << "REX_FRONTEND_INVARIANT[function-to-pointer-decay-type]: "
             "Clang cast kind "
          << implicit_cast_expr->getCastKindName()
          << " has no exact function operand and function-pointer "
             "target; subexpression="
          << implicit_cast_expr->getSubExpr()->getStmtClassName()
          << " clang-source="
          << implicit_cast_expr->getSubExpr()->getType().getAsString()
          << " clang-target=" << clang_target.getAsString()
          << " Sage-expression=" << expr->class_name() << " Sage-source="
          << (source_type != nullptr ? source_type->class_name()
                                     : std::string("<null>"))
          << " Sage-stripped="
          << (source_type != nullptr &&
                      source_type->stripTypedefsAndModifiers() != nullptr
                  ? source_type->stripTypedefsAndModifiers()->class_name()
                  : std::string("<null>"))
          << " Sage-value="
          << (source_value_type != nullptr ? source_value_type->class_name()
                                           : std::string("<null>"))
          << " Sage-function=" << (source_function != nullptr ? "yes" : "no")
          << " clang-pointer=" << (clang_pointer != nullptr ? "yes" : "no")
          << " clang-source-function="
          << (!clang_source.isNull() && clang_source->isFunctionType() ? "yes"
                                                                       : "no")
          << " clang-pointee-function="
          << (!clang_pointee.isNull() && clang_pointee->isFunctionType() ? "yes"
                                                                         : "no")
          << " clang-same-type="
          << (!clang_source.isNull() && !clang_pointee.isNull()
                  ? (p_compiler_instance->getASTContext().hasSameType(
                         clang_source, clang_pointee)
                         ? "yes"
                         : "no")
                  : "unavailable")
          << std::endl;
      ROSE_ABORT();
    }

    // The operand already names the exact translated declaration signature.
    // REX represents a call whose declaration returns a function reference
    // with that reference type even though Clang Expr::getType() exposes its
    // referred-to function type. Function-to-pointer decay removes that
    // value-category wrapper; a pointer-to-reference would be malformed.
    //
    // Preserve source typedef spelling when it wraps the function itself.
    // Retranslating the independently canonicalized Clang pointer pointee can
    // manufacture a second template-parameter identity after the declaration's
    // source-parameter frame has ended.  The implicit cast has no source type
    // spelling, so its semantic result must point at the operand's one exact
    // function type.
    target_type = SageBuilder::buildPointerType(source_value_type);
  } else if (conversion_kind ==
             SgCastExp::e_semantic_conversion_BuiltinFnToFnPtr) {
    const clang::QualType clang_target = implicit_cast_expr->getType();
    const clang::PointerType *clang_pointer =
        clang_target->getAs<clang::PointerType>();
    const bool clang_function_result = clang_target->isFunctionType();
    const bool clang_function_pointer_result =
        clang_pointer != nullptr &&
        clang_pointer->getPointeeType()->isFunctionType();
    SgType *source_type = expr->get_type();
    SgType *source_callable = source_type != nullptr
                                  ? source_type->stripTypedefsAndModifiers()
                                  : nullptr;
    if (isSgFunctionType(source_callable) == nullptr ||
        clang_function_result == clang_function_pointer_result) {
      std::cerr << "REX_FRONTEND_INVARIANT[builtin-function-decay-type]: "
                   "Clang BuiltinFnToFnPtr has no exact translated function "
                   "operand and function/function-pointer result; "
                   "subexpression="
                << implicit_cast_expr->getSubExpr()->getStmtClassName()
                << " clang-target=" << clang_target.getAsString()
                << " Sage-expression=" << expr->class_name() << " Sage-source="
                << (source_type != nullptr ? source_type->class_name()
                                           : std::string("<null>"))
                << std::endl;
      ROSE_ABORT();
    }

    // Builtin overload resolution can leave Clang's cast result expressed in
    // terms of an unresolved overload pseudo-type even after the selected
    // Sage reference has acquired its concrete declaration.  The operand is
    // the exact selected callable identity; preserve only Clang's result shape
    // and derive the semantic result from that operand in one transaction.
    target_type = clang_function_pointer_result
                      ? SageBuilder::buildPointerType(source_callable)
                      : source_callable;
  } else {
    target_type = buildTypeFromQualifiedType(implicit_cast_expr->getType());
  }
  if (target_type == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[implicit-cast-type]: Clang cast kind "
              << implicit_cast_expr->getCastKindName()
              << " has no exact translated target type" << std::endl;
    ROSE_ABORT();
  }

  const SgCastExp::value_category_enum value_category =
      translateClangValueCategory(implicit_cast_expr->getValueKind());
  if (conversion_kind == SgCastExp::e_semantic_conversion_BuiltinFnToFnPtr) {
    const clang::QualType clang_target = implicit_cast_expr->getType();
    SgType *stripped_target = target_type->stripTypedefsAndModifiers();
    const clang::PointerType *clang_pointer =
        clang_target->getAs<clang::PointerType>();
    const bool clang_function_result = clang_target->isFunctionType();
    const bool clang_function_pointer_result =
        clang_pointer != nullptr &&
        clang_pointer->getPointeeType()->isFunctionType();
    SgPointerType *sage_pointer = isSgPointerType(stripped_target);
    const bool sage_function_result =
        isSgFunctionType(stripped_target) != nullptr;
    const bool sage_function_pointer_result =
        sage_pointer != nullptr && sage_pointer->get_base_type() != nullptr &&
        isSgFunctionType(
            sage_pointer->get_base_type()->stripTypedefsAndModifiers()) !=
            nullptr;
    // LLVM's Sema initially creates a pointer result, but builtin overload
    // resolution can replace that with the selected function type (for example,
    // operator new). Preserve which of those two exact shapes Clang produced.
    if (clang_function_result == clang_function_pointer_result ||
        sage_function_result == sage_function_pointer_result ||
        clang_function_result != sage_function_result ||
        clang_function_pointer_result != sage_function_pointer_result) {
      std::string clang_target_text = clang_target.getAsString();
      std::cerr << "REX_FRONTEND_INVARIANT[builtin-function-decay-type]: Clang "
                   "BuiltinFnToFnPtr target="
                << clang_target_text << "/" << clang_target->getTypeClassName()
                << " translated to " << target_type->class_name()
                << " (stripped "
                << (stripped_target != nullptr ? stripped_target->class_name()
                                               : std::string("<null>"))
                << ") with function/pointer categories "
                << clang_function_result << "/" << clang_function_pointer_result
                << " -> " << sage_function_result << "/"
                << sage_function_pointer_result << std::endl;
      ROSE_ABORT();
    }
  }
  const SgTypePtrList conversion_base_path =
      translateCastBasePath(*implicit_cast_expr);
  SgCastExp *cast_expr = SageBuilder::buildCastExp_nfi(
      expr, target_type, SgCastExp::e_implicit_cast, conversion_kind,
      value_category, conversion_base_path, nullptr);
  ROSE_ASSERT(cast_expr != nullptr);
  *node = cast_expr;

  // This cast is a semantic node with a source-spelled child, not a
  // source-spelled cast surface. Register its synthesized provenance and
  // implicit-cast role before generic expression translation so that the
  // source-range visitor preserves and validates the classification.
  setSynthesizedFileInfo(cast_expr);
  for (Sg_File_Info *file_info :
       {cast_expr->get_file_info(), cast_expr->get_startOfConstruct(),
        cast_expr->get_endOfConstruct(), cast_expr->get_operatorPosition()}) {
    if (file_info == nullptr || !file_info->isCompilerGenerated() ||
        !file_info->isOutputInCodeGeneration()) {
      std::cerr << "REX_FRONTEND_INVARIANT[implicit-cast-owner]: typed "
                   "implicit cast has inconsistent synthesized provenance"
                << std::endl;
      ROSE_ABORT();
    }
    file_info->setImplicitCast();
  }

  const bool semantic_only_context =
      p_semantic_function_body_traversal_depth != 0 ||
      p_semantic_template_argument_expression_depth != 0 ||
      p_semantic_expression_subtree_depth != 0 ||
      p_user_defined_literal_semantic_expression_depth != 0 ||
      (!p_imported_external_semantic_traversal_stack.empty() &&
       p_imported_external_semantic_traversal_stack.back());
  std::set<std::pair<int, unsigned int>> source_physical_owners;
  bool saw_located_operand = false;
  bool operand_is_exact_semantic_subtree = true;
  SgLocatedNode *first_nonsemantic_operand = nullptr;
  for (SgNode *candidate : RoseAst(expr)) {
    if (SgAuxiliaryDeclarationList *auxiliary =
            isSgAuxiliaryDeclarationList(candidate)) {
      auxiliary->validate_semantic_non_output_role();
      continue;
    }
    SgLocatedNode *located = isSgLocatedNode(candidate);
    if (located == nullptr) {
      continue;
    }
    saw_located_operand = true;
    std::vector<Sg_File_Info *> positions{located->get_file_info(),
                                          located->get_startOfConstruct(),
                                          located->get_endOfConstruct()};
    if (SgExpression *located_expression = isSgExpression(located)) {
      positions.push_back(located_expression->get_operatorPosition());
    }
    for (Sg_File_Info *position : positions) {
      const bool exact_semantic =
          SageInterface::hasExactSemanticFrontendSourcePosition(located,
                                                                position);
      operand_is_exact_semantic_subtree &= exact_semantic;
      if (!exact_semantic && first_nonsemantic_operand == nullptr) {
        first_nonsemantic_operand = located;
      }
      // A semantic node can carry a typed file association for analyses while
      // still owning no token occurrence.  Its zero source coordinates and
      // frontend-specific classification distinguish that state from a
      // physical operand; do not reinterpret the associated file ID as a
      // source-token owner.
      if (exact_semantic) {
        continue;
      }
      if (position == nullptr || position->get_physical_file_id() < 0) {
        continue;
      }
      if (position->isShared()) {
        std::cerr << "REX_FRONTEND_INVARIANT[implicit-cast-physical-owner]: "
                     "source operand contains shared physical provenance"
                  << std::endl;
        ROSE_ABORT();
      }
      const unsigned int physical_occurrence =
          position->get_physical_file_occurrence_id();
      if (physical_occurrence == 0) {
        fprintf(
            stderr,
            "REX_FRONTEND_INVARIANT[implicit-cast-physical-owner]: cast=%p "
            "kind=%s operand-root=%p/%s candidate=%p/%s position=%p "
            "position-parent=%p file=%d physical=%d line=%d col=%d "
            "raw-line=%d raw-col=%d compiler=%d frontend=%d "
            "transformation=%d unavailable=%d output=%d implicit=%d "
            "source operand has no exact parse-session occurrence\n",
            static_cast<void *>(implicit_cast_expr),
            implicit_cast_expr->getCastKindName(), static_cast<void *>(expr),
            expr->class_name().c_str(), static_cast<void *>(located),
            located->class_name().c_str(), static_cast<void *>(position),
            static_cast<void *>(position->get_parent()),
            position->get_file_id(), position->get_physical_file_id(),
            position->get_line(), position->get_col(), position->get_raw_line(),
            position->get_raw_col(), position->isCompilerGenerated() ? 1 : 0,
            position->isFrontendSpecific() ? 1 : 0,
            position->isTransformation() ? 1 : 0,
            position->isSourcePositionUnavailableInFrontend() ? 1 : 0,
            position->isOutputInCodeGeneration() ? 1 : 0,
            position->isImplicitCast() ? 1 : 0);
        ROSE_ABORT();
      }
      source_physical_owners.emplace(position->get_physical_file_id(),
                                     physical_occurrence);
    }
  }
  if (!saw_located_operand) {
    std::cerr << "REX_FRONTEND_INVARIANT[implicit-cast-operand-provenance]: "
                 "implicit cast has no located operand producer"
              << std::endl;
    ROSE_ABORT();
  }

  if (source_physical_owners.empty() && operand_is_exact_semantic_subtree) {
    // A substituted template value or an explicitly semantic expression has
    // no physical token surface.  Its transparent conversion belongs to the
    // same semantic subtree and is published as such at this producer.
    mark_compiler_generated_frontend_specific(cast_expr);
    SageBuilder::initializeSemanticExpressionSourceProvenance(cast_expr);
  } else if (!semantic_only_context && source_physical_owners.size() == 1) {
    const auto [physical_owner, physical_occurrence] =
        *source_physical_owners.begin();
    for (Sg_File_Info *file_info :
         {cast_expr->get_file_info(), cast_expr->get_startOfConstruct(),
          cast_expr->get_endOfConstruct(), cast_expr->get_operatorPosition()}) {
      // This transparent conversion is owned by one physical source operand,
      // but has no independently written cast syntax.  Publish that distinct
      // provenance explicitly: compiler-generated implicit-cast metadata with
      // one physical owner, never the frontend-specific identity reserved for
      // wholly semantic subtrees.
      file_info->unsetFrontendSpecific();
      file_info->set_physical_file_id(physical_owner);
      file_info->set_physical_file_occurrence_id(physical_occurrence);
      if (file_info->get_physical_file_id() != physical_owner ||
          file_info->get_physical_file_occurrence_id() != physical_occurrence ||
          file_info->get_physical_filename().empty() ||
          !file_info->isCompilerGenerated() ||
          file_info->isFrontendSpecific() ||
          !file_info->isOutputInCodeGeneration() ||
          !file_info->isImplicitCast()) {
        std::cerr << "REX_FRONTEND_INVARIANT[implicit-cast-physical-owner]: "
                     "typed implicit cast did not retain its exact synthesized "
                     "role and physical source owner"
                  << std::endl;
        ROSE_ABORT();
      }
    }
  } else {
    std::cerr << "REX_FRONTEND_INVARIANT[implicit-cast-physical-owner]: "
                 "implicit cast context="
              << (semantic_only_context ? "semantic" : "source") << " has "
              << source_physical_owners.size()
              << " physical owner(s) and exact-semantic-operand="
              << (operand_is_exact_semantic_subtree ? 1 : 0)
              << " first-nonsemantic=" << first_nonsemantic_operand << "/"
              << (first_nonsemantic_operand != nullptr
                      ? first_nonsemantic_operand->class_name()
                      : std::string("<null>"))
              << std::endl;
    ROSE_ABORT();
  }

  const bool visit_ok = VisitExpr(implicit_cast_expr, node);
  if (*node != cast_expr) {
    std::cerr << "REX_FRONTEND_INVARIANT[implicit-cast-owner]: generic "
                 "expression translation replaced the registered implicit "
                 "cast"
              << std::endl;
    ROSE_ABORT();
  }
  if (cast_expr->get_semantic_conversion_kind() != conversion_kind ||
      cast_expr->get_value_category() != value_category ||
      cast_expr->get_conversion_base_path() != conversion_base_path) {
    std::cerr << "REX_FRONTEND_INVARIANT[semantic-conversion-kind]: generic "
                 "expression translation changed the exact conversion kind"
              << std::endl;
    ROSE_ABORT();
  }
  for (Sg_File_Info *file_info :
       {cast_expr->get_file_info(), cast_expr->get_startOfConstruct(),
        cast_expr->get_endOfConstruct(), cast_expr->get_operatorPosition()}) {
    if (file_info == nullptr || !file_info->isCompilerGenerated() ||
        !file_info->isOutputInCodeGeneration() ||
        !file_info->isImplicitCast()) {
      std::cerr << "REX_FRONTEND_INVARIANT[implicit-cast-owner]: generic "
                   "expression translation changed the registered "
                   "implicit-cast provenance"
                << std::endl;
      ROSE_ABORT();
    }
  }
  return visit_ok;
}

bool ClangToSageTranslator::VisitCharacterLiteral(
    clang::CharacterLiteral *character_literal, SgNode **node) {
  std::string spelling;
  bool udl_compiler_generated = false;
  auto exact_udl = p_exact_literal_operand_spellings.find(character_literal);
  if (exact_udl != p_exact_literal_operand_spellings.end()) {
    if (exact_udl->second.consumed || !exact_udl->second.compiler_generated ||
        !exact_udl->second.spelling.empty()) {
      std::cerr << "REX_FRONTEND_INVARIANT[user-defined-literal-session]: "
                   "character operand has no exact unconsumed generated role"
                << std::endl;
      ROSE_ABORT();
    }
    udl_compiler_generated = true;
    exact_udl->second.consumed = true;
  } else {
    spelling = getLiteralTokenSpelling(character_literal,
                                       p_compiler_instance->getSourceManager(),
                                       p_compiler_instance->getLangOpts());
  }
  if (spelling.empty() && !udl_compiler_generated) {
    std::cerr << "REX_FRONTEND_INVARIANT[character-literal-spelling]: source "
                 "character literal has no exact token spelling"
              << std::endl;
    ROSE_ABORT();
  }

  SgType *literal_type =
      buildTypeFromQualifiedType(character_literal->getType());
  ASSERT_not_null(literal_type);
  ROSE_ASSERT(!SageInterface::containsUnknownType(literal_type));
  const clang::QualType type = character_literal->getType();
  const uint64_t width = p_compiler_instance->getASTContext().getTypeSize(type);
  ROSE_ASSERT(width > 0 && width <= 64);
  ROSE_ASSERT(type->isSignedIntegerType() || type->isUnsignedIntegerType());
  const llvm::APInt bits(static_cast<unsigned>(width),
                         character_literal->getValue());
  const int64_t value = type->isSignedIntegerType()
                            ? bits.getSExtValue()
                            : static_cast<int64_t>(bits.getZExtValue());
  switch (character_literal->getKind()) {
  case clang::CharacterLiteralKind::Ascii:
  case clang::CharacterLiteralKind::UTF8:
    *node = SageBuilder::buildCharVal_nfi(value, spelling);
    break;
  case clang::CharacterLiteralKind::Wide:
    *node = SageBuilder::buildWcharVal_nfi(value, spelling);
    break;
  case clang::CharacterLiteralKind::UTF16:
    ROSE_ASSERT(value >= 0 && static_cast<uint64_t>(value) <=
                                  std::numeric_limits<unsigned short>::max());
    *node = SageBuilder::buildChar16Val_nfi(static_cast<unsigned short>(value),
                                            spelling);
    break;
  case clang::CharacterLiteralKind::UTF32:
    ROSE_ASSERT(value >= 0 && static_cast<uint64_t>(value) <=
                                  std::numeric_limits<unsigned int>::max());
    *node = SageBuilder::buildChar32Val_nfi(static_cast<unsigned int>(value),
                                            spelling);
    break;
  }

  auto *literal = isSgValueExp(*node);
  ASSERT_not_null(literal);
  literal->set_literal_type(literal_type);
  ROSE_ASSERT(literal->get_type() == literal_type);
  if (udl_compiler_generated) {
    literal->set_literal_spelling_form(
        SgValueExp::e_literal_canonical_generated);
    setSynthesizedFileInfo(*node);
  } else {
    literal->set_literal_spelling_form(SgValueExp::e_literal_source_spelled);
    applySourceRange(*node, character_literal->getSourceRange());
  }
  return VisitExpr(character_literal, node);
}

bool ClangToSageTranslator::VisitChooseExpr(clang::ChooseExpr *choose_expr,
                                            SgNode **node) {
  const Rose::ClangFrontend::ChooseExprOperands operands =
      Rose::ClangFrontend::requireChooseExprOperands(choose_expr);
  auto selection = SgChooseExpression::e_choose_selection_dependent;
  if (!choose_expr->isConditionDependent())
    selection = choose_expr->isConditionTrue()
                    ? SgChooseExpression::e_choose_selection_true
                    : SgChooseExpression::e_choose_selection_false;
  clang::Expr *cond_sub_expr = operands.condition;
  clang::Expr *true_sub_expr = operands.true_expression;
  clang::Expr *false_sub_expr = operands.false_expression;

  SgExpression *cond_expr = isSgExpression(Traverse(cond_sub_expr));
  SgExpression *true_expr = isSgExpression(Traverse(true_sub_expr));
  SgExpression *false_expr = isSgExpression(Traverse(false_sub_expr));
  SgType *result_type = buildTypeFromQualifiedType(choose_expr->getType());
  SgChooseExpression *result = SageBuilder::buildChooseExpression_nfi(
      cond_expr, true_expr, false_expr, result_type, selection);
  applySourceRange(result, choose_expr->getSourceRange());
  *node = result;

  return VisitExpr(choose_expr, node);
}

bool ClangToSageTranslator::VisitCompoundLiteralExpr(
    clang::CompoundLiteralExpr *compound_literal, SgNode **node) {
  if (compound_literal == nullptr || node == nullptr) {
    std::cerr
        << "REX_FRONTEND_INVARIANT[compound-literal]: exact Clang expression "
           "and Sage result slot are required"
        << std::endl;
    ROSE_ABORT();
  }

  clang::TypeSourceInfo *type_info = compound_literal->getTypeSourceInfo();
  if (type_info == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[compound-literal-type]: "
                 "source-written compound literal has no exact Clang "
                 "TypeSourceInfo"
              << std::endl;
    ROSE_ABORT();
  }
  clang::TypeLoc type_loc = type_info->getTypeLoc();
  if (type_loc.isNull()) {
    std::cerr << "REX_FRONTEND_INVARIANT[compound-literal-type]: "
                 "source-written compound literal has a null Clang TypeLoc"
              << std::endl;
    ROSE_ABORT();
  }

  clang::TagDecl *inline_tag_definition = definingTagFromTypeLoc(type_loc);
  SgDeclarationStatement *inline_tag_sage_definition = nullptr;
  if (inline_tag_definition != nullptr) {
    beginTypedOwnerTagConstruction(inline_tag_definition,
                                   SageBuilder::topScopeStack(),
                                   "VisitCompoundLiteralExpr");
  }

  SgType *source_type = buildExplicitTemplateIdTypeUseFromTypeLoc(
      type_loc, "VisitCompoundLiteralExpr:source-type");
  if (source_type == nullptr ||
      SageInterface::containsUnknownType(source_type)) {
    std::cerr << "REX_FRONTEND_INVARIANT[compound-literal-type]: exact source "
                 "TypeLoc translated to "
              << (source_type != nullptr ? source_type->class_name()
                                         : std::string("<null>"))
              << std::endl;
    ROSE_ABORT();
  }
  if (inline_tag_definition != nullptr) {
    inline_tag_sage_definition = finishTypedOwnerTagConstruction(
        inline_tag_definition, "VisitCompoundLiteralExpr");
  }
  SgType *type = buildSemanticTypeFromQualifiedType(
      compound_literal->getType(), "VisitCompoundLiteralExpr:object-type");

  clang::Expr *clang_initializer = compound_literal->getInitializer();
  if (clang_initializer == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[compound-literal-initializer]: "
                 "source-written compound literal has no Clang initializer"
              << std::endl;
    ROSE_ABORT();
  }
  SgNode *translated_initializer = Traverse(clang_initializer);
  auto *initializer = isSgAggregateInitializer(translated_initializer);
  if (initializer == nullptr || initializer->get_parent() != nullptr ||
      initializer->get_source_form() !=
          SgAggregateInitializer::
              e_aggregate_initializer_source_compound_literal ||
      !SageInterface::isEquivalentType(
          initializer->get_type(),
          buildTypeFromQualifiedType(clang_initializer->getType())) ||
      !SageInterface::cxxCompoundLiteralInitializerTypeMatchesObjectType(
          initializer->get_type(), type)) {
    std::cerr
        << "REX_FRONTEND_INVARIANT[compound-literal-initializer]: Clang "
           "initializer translated to "
        << (translated_initializer != nullptr
                ? translated_initializer->class_name()
                : std::string("<null>"))
        << " instead of a detached compound-literal aggregate with its exact "
           "native initializer type"
        << std::endl;
    ROSE_ABORT();
  }
  SgScopeStatement *scope = SageBuilder::topScopeStack();
  if (scope == nullptr || scope->get_symbol_table() == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[compound-literal-scope]: "
                 "compound literal has no exact active Sage scope and symbol "
                 "table"
              << std::endl;
    ROSE_ABORT();
  }

  SgName name = std::string("compound_literal_") +
                Rose::StringUtility::numberToString(compound_literal);
  if (scope->lookup_variable_symbol(name) != nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[compound-literal-symbol]: hidden "
                 "compound-literal name collides in its exact Sage scope"
              << std::endl;
    ROSE_ABORT();
  }

  SgVariableDeclaration *var_decl =
      new SgVariableDeclaration(name, type, initializer);
  if (var_decl == nullptr || var_decl->get_variables().size() != 1) {
    std::cerr << "REX_FRONTEND_INVARIANT[compound-literal-owner]: hidden "
                 "declaration did not produce exactly one initialized name"
              << std::endl;
    ROSE_ABORT();
  }
  var_decl->set_firstNondefiningDeclaration(var_decl);
  var_decl->set_definingDeclaration(nullptr);
  setSynthesizedFileInfo(var_decl);
  mark_compiler_generated_frontend_specific(var_decl);

  SgInitializedName *initialized_name = var_decl->get_variables().front();
  if (initialized_name == nullptr || initialized_name->get_name() != name ||
      initialized_name->get_type() != type ||
      initialized_name->get_initializer() != initializer ||
      initialized_name->get_parent() != var_decl ||
      initializer->get_parent() != initialized_name) {
    std::cerr << "REX_FRONTEND_INVARIANT[compound-literal-owner]: hidden "
                 "initialized name disagrees with its exact declaration, "
                 "type, initializer, or parent edges"
              << std::endl;
    ROSE_ABORT();
  }
  initialized_name->set_cxx_source_type(source_type);
  initialized_name->set_scope(scope);
  setSynthesizedFileInfo(initialized_name);
  mark_compiler_generated_frontend_specific(initialized_name);

  SgVariableDefinition *variable_definition =
      initialized_name->get_definition();
  if (variable_definition == nullptr ||
      variable_definition->get_vardefn() != initialized_name ||
      variable_definition->get_parent() != initialized_name) {
    std::cerr << "REX_FRONTEND_INVARIANT[compound-literal-owner]: hidden "
                 "initialized name has no exact structural variable "
                 "definition"
              << std::endl;
    ROSE_ABORT();
  }
  setSynthesizedFileInfo(variable_definition);
  mark_compiler_generated_frontend_specific(variable_definition);

  SageBuilder::attachAuxiliaryDeclaration(scope, var_decl);
  SgAuxiliaryDeclarationList *hidden_owner =
      isSgAuxiliaryDeclarationList(var_decl->get_parent());
  if (hidden_owner == nullptr || hidden_owner->get_parent() != scope ||
      var_decl->get_scope() != scope) {
    std::cerr << "REX_FRONTEND_INVARIANT[compound-literal-owner]: hidden "
                 "declaration has no exact auxiliary scope ownership"
              << std::endl;
    ROSE_ABORT();
  }

  SgVariableSymbol *symbol = new SgVariableSymbol(initialized_name);
  attachSymbolToExactScope(symbol, scope);
  if (scope->lookup_variable_symbol(name) != symbol ||
      scope->find_symbol_from_declaration(initialized_name) != symbol ||
      symbol->get_declaration() != initialized_name) {
    std::cerr << "REX_FRONTEND_INVARIANT[compound-literal-symbol]: hidden "
                 "declaration has no exact Sage variable symbol"
              << std::endl;
    ROSE_ABORT();
  }

  if (inline_tag_definition != nullptr) {
    SgDeclarationStatement *defining_tag = adopt_variable_owner_tag_definition(
        var_decl, inline_tag_sage_definition, type, "VisitCompoundLiteralExpr");
    if (defining_tag->get_parent() != var_decl ||
        var_decl->get_baseTypeDefiningDeclaration() != defining_tag) {
      std::cerr << "REX_FRONTEND_INVARIANT[compound-literal-inline-tag-owner]: "
                   "hidden declaration did not publish exact defining-tag "
                   "ownership metadata"
                << std::endl;
      ROSE_ABORT();
    }
  } else if (var_decl->get_baseTypeDefiningDeclaration() != nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[compound-literal-inline-tag-owner]: "
                 "non-defining compound-literal type has defining-tag metadata"
              << std::endl;
    ROSE_ABORT();
  }

  SgCompoundLiteralExp *compound_expression =
      SageBuilder::buildCompoundLiteralExp_nfi(symbol);
  if (compound_expression == nullptr ||
      compound_expression->get_symbol() != symbol) {
    std::cerr << "REX_FRONTEND_INVARIANT[compound-literal]: exact Sage "
                 "compound-literal expression construction failed"
              << std::endl;
    ROSE_ABORT();
  }
  *node = compound_expression;
  return VisitExpr(compound_literal, node);
}

SgNonrealRefExp *ClangToSageTranslator::buildConceptReference(
    const clang::ConceptReference &reference,
    const clang::ConceptSpecializationExpr *specialization) {
  SgTemplateArgumentPtrList template_args;
  const clang::ASTTemplateArgumentListInfo *args_written =
      reference.getTemplateArgsAsWritten();
  const bool explicit_arguments =
      args_written != nullptr && args_written->getLAngleLoc().isValid();
  if (args_written != nullptr)
    ROSE_ASSERT(explicit_arguments == args_written->getRAngleLoc().isValid());
  if (explicit_arguments) {
    for (const clang::TemplateArgumentLoc &arg_loc :
         args_written->arguments()) {
      appendTemplateArguments(template_args, arg_loc, true);
    }
  } else if (args_written != nullptr) {
    // Immediately-declared constraints carry implicit specialization arguments
    // without angle tokens. Preserve their typed semantic arguments.
    if (specialization != nullptr) {
      ROSE_ASSERT(specialization->getConceptReference() == &reference);
      for (const clang::TemplateArgument &argument :
           specialization->getTemplateArguments())
        appendTemplateArguments(template_args, argument, false);
    } else {
      ROSE_ASSERT(args_written->arguments().empty());
    }
  }

  SgNonrealRefExp *ref = nullptr;
  clang::TemplateDecl *concept_decl = reference.getNamedConcept();
  if (concept_decl != nullptr) {
    auto it = p_decl_translation_map.find(concept_decl);
    if (it == p_decl_translation_map.end() &&
        p_decl_translation_in_progress.find(concept_decl) ==
            p_decl_translation_in_progress.end() &&
        p_decl_translation_on_demand.find(concept_decl) ==
            p_decl_translation_on_demand.end()) {
      TraverseOnDemand(concept_decl);
      it = p_decl_translation_map.find(concept_decl);
    }
    if (it != p_decl_translation_map.end()) {
      if (SgNonrealDecl *nrdecl = isSgNonrealDecl(it->second)) {
        if (SgNonrealSymbol *sym =
                isSgNonrealSymbol(nrdecl->get_symbol_from_symbol_table())) {
          ROSE_ASSERT(sym->get_declaration() == nrdecl);
          ref = SageBuilder::buildNonrealRefExp_nfi(sym);
        }
      }
    }
  }

  if (ref == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[concept-reference]: Clang concept="
              << (concept_decl != nullptr ? concept_decl->getNameAsString()
                                          : std::string("<null>"))
              << " has no exact translated SgNonrealSymbol" << std::endl;
    ROSE_ABORT();
  }

  ROSE_ASSERT(ref->get_symbol()->get_declaration()->get_is_concept());
  ref->get_templateArguments() = template_args;
  SageBuilder::setTemplateArgumentParents(ref);
  ref->set_explicit_template_argument_list(explicit_arguments);
  clang::NestedNameSpecifierLoc qualifier_loc =
      reference.getNestedNameSpecifierLoc();
  attachExplicitQualifierFromNestedName(
      ref, qualifier_loc.getNestedNameSpecifier(),
      qualifier_loc ? &qualifier_loc : nullptr, p_compiler_instance);

  applySourceRange(ref, reference.getSourceRange());
  return ref;
}

bool ClangToSageTranslator::VisitConceptSpecializationExpr(
    clang::ConceptSpecializationExpr *concept_specialization_expr,
    SgNode **node) {
  ROSE_ASSERT(concept_specialization_expr->getConceptReference() != nullptr);
  SgNonrealRefExp *ref =
      buildConceptReference(*concept_specialization_expr->getConceptReference(),
                            concept_specialization_expr);
  if (!concept_specialization_expr->isValueDependent()) {
    const clang::ASTConstraintSatisfaction &satisfaction =
        concept_specialization_expr->getSatisfaction();
    ConstraintSatisfactionResult result;
    result.evaluated = true;
    result.contains_errors = satisfaction.ContainsErrors;
    result.satisfied = satisfaction.IsSatisfied && !result.contains_errors;
    attachConstraintSatisfaction(ref, result);
  }

  *node = ref;

  return VisitExpr(concept_specialization_expr, node);
}

bool ClangToSageTranslator::VisitConvertVectorExpr(
    clang::ConvertVectorExpr *convert_vector_expr, SgNode **node) {
  SgNode *tmp_src = Traverse(convert_vector_expr->getSrcExpr());
  SgExpression *src_expr = isSgExpression(tmp_src);
  if (src_expr == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[convert-vector-source]: Clang "
                 "source translated to "
              << (tmp_src == nullptr ? std::string("<null>")
                                     : tmp_src->class_name())
              << " instead of SgExpression" << std::endl;
    ROSE_ABORT();
  }

  clang::TypeSourceInfo *target_type_info =
      convert_vector_expr->getTypeSourceInfo();
  if (target_type_info == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[convert-vector-target]: Clang "
                 "expression has no exact target TypeSourceInfo"
              << std::endl;
    ROSE_ABORT();
  }
  clang::TypeLoc target_type_loc = target_type_info->getTypeLoc();

  SgType *target_type = buildTypeFromTypeLoc(target_type_loc);
  if (target_type == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[convert-vector-target]: target "
                 "TypeLoc has no exact translated type"
              << std::endl;
    ROSE_ABORT();
  }
  SgTypeExpression *target_type_operand =
      SageBuilder::buildTypeExpression_nfi(target_type);
  applySourceRange(target_type_operand, target_type_loc.getSourceRange());

  SgExpressionPtrList args;
  args.push_back(src_expr);
  args.push_back(target_type_operand);

  SgTypeTraitBuiltinOperator *builtin_expr =
      SageBuilder::buildTypeTraitBuiltinOperator_nfi(
          "__builtin_convertvector",
          SgTypeTraitBuiltinOperator::e_convert_vector_builtin, target_type,
          args);
  ROSE_ASSERT(builtin_expr != nullptr);
  applySourceRange(builtin_expr, convert_vector_expr->getSourceRange());
  *node = builtin_expr;

  return VisitExpr(convert_vector_expr, node);
}

void ClangToSageTranslator::buildCoroutineAwaitExpression(
    clang::Expr *operand, clang::QualType result_type,
    clang::SourceRange source_range,
    SgAwaitExpression::coroutine_keyword_kind_enum keyword,
    const char *operand_description, SgNode **node) {
  ROSE_ASSERT(node != nullptr && operand_description != nullptr);

  if (operand == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[coroutine-await-operand]: required "
              << operand_description << " is absent" << std::endl;
    ROSE_ABORT();
  }
  SgNode *tmp_operand = Traverse(operand);
  SgExpression *operand_expr = isSgExpression(tmp_operand);
  if (operand_expr == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[coroutine-await-operand]: "
              << operand_description << " translated to "
              << (tmp_operand == nullptr ? std::string("<null>")
                                         : tmp_operand->class_name())
              << " instead of SgExpression" << std::endl;
    ROSE_ABORT();
  }

  SgType *sage_result_type = buildTypeFromQualifiedType(result_type);
  SgAwaitExpression *await_expr = SageBuilder::buildAwaitExpression_nfi(
      operand_expr, sage_result_type, keyword);
  applySourceRange(await_expr, source_range);
  *node = await_expr;
}

bool ClangToSageTranslator::VisitCoawaitExpr(clang::CoawaitExpr *coawait_expr,
                                             SgNode **node) {
  buildCoroutineAwaitExpression(coawait_expr->getOperand(),
                                coawait_expr->getType(),
                                coawait_expr->getSourceRange(),
                                SgAwaitExpression::e_coroutine_keyword_co_await,
                                "co_await operand", node);
  return VisitExpr(coawait_expr, node);
}

bool ClangToSageTranslator::VisitCoyieldExpr(clang::CoyieldExpr *coyield_expr,
                                             SgNode **node) {
  if (p_function_body_translation_owner_stack.empty()) {
    std::cerr << "REX_FRONTEND_INVARIANT[coyield-promise]: no exact "
                 "function body owner"
              << std::endl;
    ROSE_ABORT();
  }
  const auto *function =
      p_function_body_translation_owner_stack.back().clang_declaration;
  const auto *coroutine_body =
      llvm::dyn_cast_or_null<clang::CoroutineBodyStmt>(function->getBody());
  if (coroutine_body == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[coyield-promise]: no exact "
                 "coroutine body"
              << std::endl;
    ROSE_ABORT();
  }

  clang::Expr *awaitable = coyield_expr->getOperand();
  if (awaitable == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[coyield-operand]: missing awaitable"
              << std::endl;
    ROSE_ABORT();
  }
  awaitable = awaitable->IgnoreParenImpCasts();
  if (auto *operator_call =
          llvm::dyn_cast<clang::CXXOperatorCallExpr>(awaitable)) {
    if (operator_call->getOperator() == clang::OO_Coawait &&
        operator_call->getNumArgs() == 1) {
      awaitable = operator_call->getArg(0)->IgnoreParenImpCasts();
    }
  } else if (auto *unary = llvm::dyn_cast<clang::UnaryOperator>(awaitable)) {
    if (unary->getOpcode() == clang::UO_Coawait) {
      awaitable = unary->getSubExpr()->IgnoreParenImpCasts();
    }
  }

  auto *promise_call = llvm::dyn_cast<clang::CallExpr>(awaitable);
  clang::Expr *promise_object = nullptr;
  if (auto *member_call =
          llvm::dyn_cast_or_null<clang::CXXMemberCallExpr>(promise_call)) {
    promise_object = member_call->getImplicitObjectArgument();
  } else if (promise_call != nullptr) {
    if (auto *dependent_member =
            llvm::dyn_cast<clang::CXXDependentScopeMemberExpr>(
                promise_call->getCallee()->IgnoreParenImpCasts())) {
      promise_object = dependent_member->getBase();
    }
  }
  auto *promise_reference = promise_object != nullptr
                                ? llvm::dyn_cast<clang::DeclRefExpr>(
                                      promise_object->IgnoreParenImpCasts())
                                : nullptr;
  if (promise_call == nullptr || promise_call->getNumArgs() == 0 ||
      promise_reference == nullptr ||
      promise_reference->getDecl() != coroutine_body->getPromiseDecl()) {
    std::cerr << "REX_FRONTEND_INVARIANT[coyield-operand]: awaitable is not "
                 "the exact promise call"
              << std::endl;
    ROSE_ABORT();
  }
  for (unsigned index = 1; index < promise_call->getNumArgs(); ++index) {
    if (!llvm::isa<clang::CXXDefaultArgExpr>(promise_call->getArg(index))) {
      std::cerr << "REX_FRONTEND_INVARIANT[coyield-operand]: promise call "
                   "has another source argument"
                << std::endl;
      ROSE_ABORT();
    }
  }
  clang::Expr *operand = promise_call->getArg(0);

  buildCoroutineAwaitExpression(operand, coyield_expr->getType(),
                                coyield_expr->getSourceRange(),
                                SgAwaitExpression::e_coroutine_keyword_co_yield,
                                "co_yield operand", node);

  return VisitExpr(coyield_expr, node);
}

bool ClangToSageTranslator::VisitCXXBindTemporaryExpr(
    clang::CXXBindTemporaryExpr *cxx_bind_temporary_expr, SgNode **node) {
  clang::Expr *sub_expr = cxx_bind_temporary_expr->getSubExpr();
  SgNode *translated = Traverse(sub_expr);
  SgExpression *expression = isSgExpression(translated);
  if (sub_expr == nullptr || expression == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[bound-temporary]: exact operand "
                 "translated to "
              << (translated == nullptr ? std::string("<null>")
                                        : translated->class_name())
              << " instead of SgExpression" << std::endl;
    ROSE_ABORT();
  }
  auto *destructor = isSgMemberFunctionDeclaration(
      Traverse(const_cast<clang::CXXDestructorDecl *>(
          cxx_bind_temporary_expr->getTemporary()->getDestructor())));
  ROSE_ASSERT(SgExpression::is_valid_bound_temporary_destructor(
      expression->get_type(), destructor));
  expression->set_semantic_wrapper_mask(
      static_cast<SgExpression::semantic_wrapper_mask_enum>(
          expression->get_semantic_wrapper_mask() |
          SgExpression::e_bound_temporary),
      destructor, expression->get_materialized_storage_duration(),
      expression->get_materialized_lifetime_owner());
  *node = expression;
  return VisitExpr(cxx_bind_temporary_expr, node);
}

bool ClangToSageTranslator::VisitCXXBoolLiteralExpr(
    clang::CXXBoolLiteralExpr *cxx_bool_literal_expr, SgNode **node) {
  // C++ boolean literals (true/false)
  bool value = cxx_bool_literal_expr->getValue();
  const bool semantic_expression =
      hasExactSemanticExpressionConstructionRole("VisitCXXBoolLiteralExpr");
  SgBoolValExp *literal = SageBuilder::buildBoolValExp_nfi(value);
  if (literal == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[bool-literal-provenance]: exact "
                 "boolean literal construction produced no Sage node"
              << std::endl;
    ROSE_ABORT();
  }
  literal->set_literal_spelling_form(
      semantic_expression ? SgValueExp::e_literal_canonical_generated
                          : SgValueExp::e_literal_source_spelled);
  *node = literal;
  applySourceRange(*node, cxx_bool_literal_expr->getSourceRange());

  return VisitExpr(cxx_bool_literal_expr, node);
}

bool ClangToSageTranslator::VisitCXXConstructExpr(
    clang::CXXConstructExpr *cxx_construct_expr, SgNode **node) {
  clang::CXXConstructorDecl *ctor_decl = cxx_construct_expr->getConstructor();

  if (ctor_decl != nullptr) {
    const clang::TemplateSpecializationKind constructor_kind =
        ctor_decl->getTemplateSpecializationKind();
    if (ctor_decl->doesThisDeclarationHaveABody() &&
        (constructor_kind == clang::TSK_ImplicitInstantiation ||
         constructor_kind == clang::TSK_ExplicitInstantiationDeclaration ||
         constructor_kind == clang::TSK_ExplicitInstantiationDefinition)) {
      if (p_pending_function_body_completions_set.insert(ctor_decl).second) {
        p_pending_function_body_completions.push_back(ctor_decl);
      }
    }

    const bool semantic_constructor =
        hasExactSemanticExpressionConstructionRole("VisitCXXConstructExpr");
    auto *temporary_object_expr =
        llvm::dyn_cast<clang::CXXTemporaryObjectExpr>(cxx_construct_expr);
    SgType *constructed_type = nullptr;
    if (temporary_object_expr != nullptr && !semantic_constructor) {
      clang::TypeSourceInfo *type_info =
          temporary_object_expr->getTypeSourceInfo();
      if (type_info == nullptr || type_info->getType().isNull())
        failExactExpressionTranslation("temporary-object-written-type",
                                       cxx_construct_expr);
      clang::TypeLoc written_type_loc = type_info->getTypeLoc();
      if (written_type_loc.isNull())
        failExactExpressionTranslation("temporary-object-written-type",
                                       cxx_construct_expr);
      const clang::Type *written_type = written_type_loc.getTypePtr();
      if (!qualifiedTypeHasQualifier(written_type)) {
        SgScopeStatement *scope =
            !p_type_loc_semantic_owner_scope_stack.empty()
                ? p_type_loc_semantic_owner_scope_stack.back()
                : SageBuilder::topScopeStack();
        if (scope == nullptr)
          failExactExpressionTranslation("temporary-object-type-owner",
                                         cxx_construct_expr);
        constructed_type =
            buildUnqualifiedWrittenRecordMemberType(written_type, scope);
      }
      if (constructed_type == nullptr)
        constructed_type = buildTypeFromTypeLoc(written_type_loc);
    } else {
      constructed_type = buildSemanticTypeFromQualifiedType(
          cxx_construct_expr->getType().getCanonicalType(),
          "constructor-semantic-type");
    }
    if (constructed_type == nullptr ||
        SageInterface::containsUnknownType(constructed_type))
      failExactExpressionTranslation("constructor-constructed-type",
                                     cxx_construct_expr);

    SgExprListExp *args = SageBuilder::buildExprListExp_nfi();
    clang::SourceRange written_delimiter_range =
        cxx_construct_expr->getParenOrBraceRange();
    if (!written_delimiter_range.isValid() &&
        cxx_construct_expr->isListInitialization()) {
      // In copy-list initialization Clang assigns the written braces to the
      // exact InitListExpr argument rather than to the surrounding
      // CXXConstructExpr.  Recover that typed syntax edge only when its brace
      // coordinates are identical to the complete construct range; nested
      // initializer lists cannot satisfy this contract accidentally.
      const clang::SourceRange construct_range =
          cxx_construct_expr->getSourceRange();
      unsigned exact_list_owner_count = 0;
      for (clang::Expr *argument : cxx_construct_expr->arguments()) {
        clang::Expr *current = argument;
        std::set<clang::Expr *> visited;
        while (current != nullptr && visited.insert(current).second) {
          current = current->IgnoreParenImpCasts();
          if (clang::CXXStdInitializerListExpr *standard_list =
                  llvm::dyn_cast<clang::CXXStdInitializerListExpr>(current)) {
            current = standard_list->getSubExpr();
            continue;
          }
          if (clang::MaterializeTemporaryExpr *materialized =
                  llvm::dyn_cast<clang::MaterializeTemporaryExpr>(current)) {
            current = materialized->getSubExpr();
            continue;
          }
          if (clang::CXXBindTemporaryExpr *bound =
                  llvm::dyn_cast<clang::CXXBindTemporaryExpr>(current)) {
            current = bound->getSubExpr();
            continue;
          }
          if (clang::ExprWithCleanups *cleanups =
                  llvm::dyn_cast<clang::ExprWithCleanups>(current)) {
            current = cleanups->getSubExpr();
            continue;
          }
          clang::InitListExpr *list =
              llvm::dyn_cast<clang::InitListExpr>(current);
          if (list != nullptr && list->getSyntacticForm() != nullptr) {
            list = list->getSyntacticForm();
          }
          if (list != nullptr) {
            const clang::SourceRange list_range(list->getLBraceLoc(),
                                                list->getRBraceLoc());
            if (list_range.isValid() && construct_range.isValid() &&
                list_range.getBegin() == construct_range.getBegin() &&
                list_range.getEnd() == construct_range.getEnd()) {
              written_delimiter_range = list_range;
              ++exact_list_owner_count;
            }
          }
          break;
        }
      }
      if (exact_list_owner_count != 1) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[constructor-list-source-owner]: "
                "constructor=%p has %u exact top-level written initializer "
                "list owners\n",
                static_cast<void *>(cxx_construct_expr),
                exact_list_owner_count);
        ROSE_ABORT();
      }
    }
    if (written_delimiter_range.isValid()) {
      applySourceRange(args, written_delimiter_range);
    } else {
      // A default-initialized object (`T value;`) has a real construct
      // expression but no written argument delimiters. Its empty/default-only
      // argument list is semantic structure, not a source range to infer from
      // the surrounding declarator.
      bool has_only_implicit_default_arguments = true;
      unsigned explicit_argument_count = 0;
      clang::Expr *explicit_argument = nullptr;
      for (clang::Expr *argument : cxx_construct_expr->arguments()) {
        if (!llvm::isa_and_nonnull<clang::CXXDefaultArgExpr>(argument)) {
          has_only_implicit_default_arguments = false;
          ++explicit_argument_count;
          explicit_argument = argument;
        }
      }

      // Copy-initialization and implicit converting-constructor calls have one
      // source-written operand but do not own parentheses or braces.  Clang
      // represents that syntax as a plain CXXConstructExpr whose source
      // interval contains the sole explicit operand.  This is distinct from a
      // CXXTemporaryObjectExpr (`T(arg)`), which must own an exact delimiter
      // range.  Publish the delimiter-free form only after proving that exact
      // producer relationship; malformed explicit construction still fails
      // here rather than being repaired by the unparser.
      bool has_exact_delimiter_free_conversion = false;
      if (!cxx_construct_expr->isListInitialization() &&
          !llvm::isa<clang::CXXTemporaryObjectExpr>(cxx_construct_expr) &&
          explicit_argument_count == 1 && explicit_argument != nullptr) {
        clang::SourceManager &source_manager =
            p_compiler_instance->getSourceManager();
        const clang::SourceRange construct_range =
            cxx_construct_expr->getSourceRange();
        const clang::SourceRange argument_range =
            explicit_argument->getSourceRange();
        const clang::SourceLocation construct_begin =
            source_manager.getSpellingLoc(construct_range.getBegin());
        const clang::SourceLocation construct_end =
            source_manager.getSpellingLoc(construct_range.getEnd());
        const clang::SourceLocation argument_begin =
            source_manager.getSpellingLoc(argument_range.getBegin());
        const clang::SourceLocation argument_end =
            source_manager.getSpellingLoc(argument_range.getEnd());
        auto ordered = [&source_manager](clang::SourceLocation lhs,
                                         clang::SourceLocation rhs) {
          return lhs == rhs ||
                 source_manager.isBeforeInTranslationUnit(lhs, rhs);
        };
        has_exact_delimiter_free_conversion =
            construct_begin.isValid() && construct_end.isValid() &&
            argument_begin.isValid() && argument_end.isValid() &&
            ordered(construct_begin, argument_begin) &&
            ordered(argument_end, construct_end);
      }

      if (cxx_construct_expr->isListInitialization() ||
          (!has_only_implicit_default_arguments &&
           !has_exact_delimiter_free_conversion) ||
          !cxx_construct_expr->getSourceRange().isValid()) {
        clang::SourceManager &source_manager =
            p_compiler_instance->getSourceManager();
        const clang::SourceRange construct_range =
            cxx_construct_expr->getSourceRange();
        const std::string construct_begin =
            construct_range.getBegin().printToString(source_manager);
        const std::string construct_end =
            construct_range.getEnd().printToString(source_manager);
        const std::string constructor_name =
            ctor_decl->getQualifiedNameAsString();
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[constructor-argument-source]: "
                "constructor=%p declaration=%s arguments=%u list=%d "
                "elidable=%d construct-range=%s..%s has explicit arguments "
                "or list syntax without one exact delimiter range\n",
                static_cast<void *>(cxx_construct_expr),
                constructor_name.c_str(), cxx_construct_expr->getNumArgs(),
                cxx_construct_expr->isListInitialization() ? 1 : 0,
                cxx_construct_expr->isElidable() ? 1 : 0,
                construct_begin.c_str(), construct_end.c_str());
        ROSE_ABORT();
      }
      setSynthesizedFileInfo(args);
    }

    auto is_std_init_list_arg = [](clang::Expr *arg) {
      clang::Expr *base = arg;
      while (base != nullptr) {
        base = base->IgnoreParenImpCasts();
        if (llvm::isa<clang::CXXStdInitializerListExpr>(base)) {
          return true;
        }
        if (auto *materialize =
                llvm::dyn_cast<clang::MaterializeTemporaryExpr>(base)) {
          base = materialize->getSubExpr();
          continue;
        }
        if (auto *bind = llvm::dyn_cast<clang::CXXBindTemporaryExpr>(base)) {
          base = bind->getSubExpr();
          continue;
        }
        break;
      }
      return false;
    };

    for (unsigned i = 0; i < cxx_construct_expr->getNumArgs(); ++i) {
      clang::Expr *arg = cxx_construct_expr->getArg(i);
      if (arg == nullptr) {
        failExactExpressionTranslation("constructor-argument-missing",
                                       cxx_construct_expr);
      }
      SgExpression *sg_expr = isSgExpression(Traverse(arg));
      if (sg_expr == nullptr) {
        failExactExpressionTranslation("constructor-argument-translation", arg);
      }
      if (cxx_construct_expr->isListInitialization() &&
          is_std_init_list_arg(arg)) {
        if (SgAggregateInitializer *agg_init =
                isSgAggregateInitializer(sg_expr)) {
          agg_init->set_source_form(
              SgAggregateInitializer::e_aggregate_initializer_source_unbraced);
        }
      }
      args->append_expression(sg_expr);
    }

    const bool class_unknown =
        constructorInitializerAssociatedClassUnknown(constructed_type);
    SgMemberFunctionDeclaration *sg_ctor_decl =
        isSgMemberFunctionDeclaration(Traverse(ctor_decl));
    if (sg_ctor_decl == nullptr) {
      failExactExpressionTranslation("constructor-declaration",
                                     cxx_construct_expr);
    }

    SgConstructorInitializer *ctor_init =
        SageBuilder::buildConstructorInitializer_nfi(
            sg_ctor_decl, args, constructed_type,
            false, // need_name
            false, // need_qualifier
            written_delimiter_range.isValid() &&
                !cxx_construct_expr->isListInitialization(),
            // need_parenthesis_after_name
            class_unknown // associated_class_unknown
        );
    // The CXXConstructExpr is the exact expression surface, independently of
    // whether its selected constructor declaration is implicit or belongs to
    // an unnamed class.  Classifying the expression from those declaration
    // properties discarded real source ranges and later forced callers to
    // overwrite synthesized provenance.  Delimiter-free default construction
    // is represented by the generated argument list above; the enclosing
    // expression still owns Clang's exact construct range.
    applySourceRange(ctor_init, cxx_construct_expr->getSourceRange());

    // Preserve brace-init vs paren-init. Without this, list-initialization like
    // `T t{};` may be unparsed as `T t;`, which can change semantics (e.g.,
    // value-initialization vs default-initialization).
    ctor_init->set_is_braced_initialized(
        cxx_construct_expr->isListInitialization());
    ctor_init->set_is_elidable(cxx_construct_expr->isElidable());
    ctor_init->set_elision_permitted(
        cxx_construct_expr->isElidable() &&
        p_compiler_instance->getLangOpts().ElideConstructors);
    ctor_init->validate_elision();

    *node = ctor_init;
  } else {
    failExactExpressionTranslation("constructor-declaration",
                                   cxx_construct_expr);
  }

  return VisitExpr(cxx_construct_expr, node);
}

bool ClangToSageTranslator::VisitCXXTemporaryObjectExpr(
    clang::CXXTemporaryObjectExpr *cxx_temporary_object_expr, SgNode **node) {
  if (!VisitCXXConstructExpr(cxx_temporary_object_expr, node)) {
    return false;
  }
  if (SgConstructorInitializer *ctor_init = isSgConstructorInitializer(*node)) {
    if (!hasExactSemanticExpressionConstructionRole(
            "VisitCXXTemporaryObjectExpr:source-type")) {
      const clang::TypeLoc type_loc =
          cxx_temporary_object_expr->getTypeSourceInfo()->getTypeLoc();
      SgStringList qualifier_tokens;
      bool global_qualification = false;
      (void)exactTypeLocSourceQualification(type_loc, qualifier_tokens,
                                            global_qualification);
      transferWrittenNonrealQualifierToUseSite(
          ctor_init->get_type(), qualifier_tokens, global_qualification,
          "VisitCXXTemporaryObjectExpr");
      ctor_init->set_explicit_name_qualification_present(true);
      ctor_init->set_explicit_global_qualification(global_qualification);
      ctor_init->set_explicit_name_qualification_tokens(qualifier_tokens);
      const auto elaboration = exactTypeLocSourceElaborationKind(type_loc);
      ctor_init->set_source_type_elaboration_kind(elaboration);
      ctor_init->set_type_elaboration_required(
          elaboration != SgNonrealDecl::e_source_elaboration_none);
    }
    // Ensure temporaries print as `Type()` even with no args, since they can
    // be used as implicit objects (e.g., conversion operators).
    ctor_init->set_need_name(true);
    ctor_init->set_need_parenthesis_after_name(
        !ctor_init->get_is_braced_initialized());
  }

  return true;
}

SgExpression *
ClangToSageTranslator::buildDeclarationConditionValue(clang::Expr *condition,
                                                      SgScopeStatement *owner) {
  ROSE_ASSERT(condition != nullptr && owner != nullptr);
  SemanticExpressionConstruction semantic_condition(
      p_semantic_expression_subtree_depth, "declaration-condition-value");
  auto *value = isSgExpression(Traverse(condition));
  if (value == nullptr)
    failExactExpressionTranslation("declaration-condition-value", condition);
  value = prepareExpressionForAttachment(value);
  ROSE_ASSERT(value->get_parent() == nullptr && value->get_type() != nullptr);
  value->set_parent(owner);
  return value;
}

bool ClangToSageTranslator::VisitCXXDefaultArgExpr(
    clang::CXXDefaultArgExpr *cxx_default_arg_expr, SgNode **node) {
  if (cxx_default_arg_expr->getExpr() == nullptr)
    failExactExpressionTranslation("default-argument-missing-expression",
                                   cxx_default_arg_expr);
  SgInitializedName *parameter =
      lookupFunctionParameterFromOwner(cxx_default_arg_expr->getParam());
  SemanticExpressionConstruction semantic_argument(
      p_semantic_expression_subtree_depth, "default-argument-use");
  auto *expression = isSgExpression(Traverse(cxx_default_arg_expr->getExpr()));
  if (expression == nullptr)
    failExactExpressionTranslation("default-argument-null-expression",
                                   cxx_default_arg_expr);
  expression = prepareExpressionForAttachment(expression);
  if (auto *initializers = isSgExprListExp(expression)) {
    expression = SageBuilder::buildAggregateInitializer_nfi(
        initializers,
        buildTypeFromQualifiedType(cxx_default_arg_expr->getType()),
        SgAggregateInitializer::e_aggregate_initializer_source_braced);
    publishSemanticExpressionSourceProvenance(
        expression, cxx_default_arg_expr->getExpr()->getSourceRange(),
        "default-argument-use");
  }
  if (!SgCxxDefaultInitExp::is_valid_use(parameter, expression))
    failExactExpressionTranslation("default-argument-parameter-identity",
                                   cxx_default_arg_expr);
  auto *use = new SgCxxDefaultInitExp(parameter, expression,
                                      SgCxxDefaultInitExp::e_parameter);
  publishSemanticExpressionSourceProvenance(
      use, cxx_default_arg_expr->getExpr()->getSourceRange(),
      "default-argument-use");
  *node = use;
  return VisitExpr(cxx_default_arg_expr, node);
}

bool ClangToSageTranslator::VisitCXXDefaultInitExpr(
    clang::CXXDefaultInitExpr *cxx_default_init_expr, SgNode **node) {
  auto *field =
      isSgVariableDeclaration(Traverse(cxx_default_init_expr->getField()));
  if (field == nullptr || field->get_variables().size() != 1 ||
      cxx_default_init_expr->getExpr() == nullptr) {
    failExactExpressionTranslation("default-initializer-input",
                                   cxx_default_init_expr);
  }

  SemanticExpressionConstruction semantic_use(
      p_semantic_expression_subtree_depth, "default-member-use");
  SgNode *tmp_expr = Traverse(cxx_default_init_expr->getExpr());
  SgExpression *expr = isSgExpression(tmp_expr);
  if (expr == nullptr) {
    failExactExpressionTranslation("default-initializer-translation",
                                   cxx_default_init_expr);
  }

  expr = prepareExpressionForAttachment(expr);
  if (auto *initializers = isSgExprListExp(expr)) {
    expr = SageBuilder::buildAggregateInitializer_nfi(
        initializers,
        buildTypeFromQualifiedType(cxx_default_init_expr->getType()),
        SgAggregateInitializer::e_aggregate_initializer_source_braced);
    publishSemanticExpressionSourceProvenance(
        expr, cxx_default_init_expr->getExpr()->getSourceRange(),
        "default-member-use");
  }
  auto *source = field->get_variables().front();
  if (!SgCxxDefaultInitExp::is_valid_use(source, expr,
                                         SgCxxDefaultInitExp::e_member))
    failExactExpressionTranslation("default-member-field-identity",
                                   cxx_default_init_expr);
  auto *use =
      new SgCxxDefaultInitExp(source, expr, SgCxxDefaultInitExp::e_member);
  publishSemanticExpressionSourceProvenance(
      use, cxx_default_init_expr->getExpr()->getSourceRange(),
      "default-member-use");
  *node = use;
  return VisitExpr(cxx_default_init_expr, node);
}

bool ClangToSageTranslator::VisitCXXDeleteExpr(
    clang::CXXDeleteExpr *cxx_delete_expr, SgNode **node) {
  clang::Expr *arg_expr = cxx_delete_expr->getArgument();
  SgNode *tmp_arg = arg_expr ? Traverse(arg_expr) : nullptr;
  SgExpression *arg = isSgExpression(tmp_arg);

  if (arg == nullptr) {
    failExactExpressionTranslation("delete-argument", arg_expr);
  }

  SgFunctionDeclaration *delete_operator = nullptr;
  if (clang::FunctionDecl *selected = cxx_delete_expr->getOperatorDelete()) {
    delete_operator = isSgFunctionDeclaration(Traverse(selected));
    if (delete_operator == nullptr)
      failExactExpressionTranslation("delete-operator", cxx_delete_expr);
  }
  *node = SageBuilder::buildDeleteExp_nfi(arg, cxx_delete_expr->isArrayForm(),
                                          cxx_delete_expr->isGlobalDelete(),
                                          delete_operator);

  return VisitExpr(cxx_delete_expr, node);
}

void ClangToSageTranslator::buildConversionFunctionId(
    SgExpression *reference, const clang::DeclarationNameInfo &name,
    bool source_written) {
  ROSE_ASSERT(reference != nullptr);
  if (name.getName().getNameKind() !=
      clang::DeclarationName::CXXConversionFunctionName) {
    return;
  }
  clang::TypeSourceInfo *type_info = name.getNamedTypeInfo();
  if (source_written && type_info == nullptr) {
    llvm::errs() << "REX_FRONTEND_INVARIANT[conversion-function-id]: written "
                    "conversion name has no TypeSourceInfo\n";
    ROSE_ABORT();
  }
  SgType *target =
      source_written
          ? buildTypeFromTypeLoc(type_info->getTypeLoc())
          : buildTypeFromQualifiedType(name.getName().getCXXNameType());
  ROSE_ASSERT(!SageInterface::containsUnknownType(target));
  SgTypeExpression *type_id = SageBuilder::buildTypeExpression_nfi(target);
  if (source_written) {
    applySourceRange(type_id, type_info->getTypeLoc().getSourceRange());
  } else {
    SageBuilder::initializeSemanticExpressionSourceProvenance(type_id);
  }
  if (auto *nonreal = isSgNonrealRefExp(reference)) {
    ROSE_ASSERT(nonreal->get_name_kind() == SgNonrealRefExp::e_ordinary_name);
    ROSE_ASSERT(nonreal->get_conversion_type_id() == nullptr);
    nonreal->set_name_kind(SgNonrealRefExp::e_conversion_function_id);
    nonreal->set_conversion_type_id(type_id);
  } else if (auto *member = isSgMemberFunctionRefExp(reference)) {
    ROSE_ASSERT(member->get_conversion_type_id() == nullptr);
    member->set_conversion_type_id(type_id);
  } else if (auto *member = isSgTemplateMemberFunctionRefExp(reference)) {
    ROSE_ASSERT(member->get_conversion_type_id() == nullptr);
    member->set_conversion_type_id(type_id);
  } else {
    llvm::errs() << "REX_FRONTEND_INVARIANT[conversion-function-id]: name has "
                    "an invalid reference node kind\n";
    ROSE_ABORT();
  }
  type_id->set_parent(reference);
}

bool ClangToSageTranslator::VisitCXXDependentScopeMemberExpr(
    clang::CXXDependentScopeMemberExpr *cxx_dependent_scope_member_expr,
    SgNode **node) {
  SgType *result_type =
      buildTypeFromQualifiedType(cxx_dependent_scope_member_expr->getType());
  if (SageInterface::containsUnknownType(result_type)) {
    std::cerr << "REX_FRONTEND_INVARIANT[dependent-member-result-type]: "
                 "Clang expression has no exact semantic result type"
              << std::endl;
    ROSE_ABORT();
  }

  // CXXDependentScopeMemberExpr represents member access on a
  // template-dependent type (e.g., obj.begin(), obj->data()).

  clang::DeclarationName member_name_decl =
      cxx_dependent_scope_member_expr->getMember();

  std::string member_name = member_name_decl.getAsString();
  clang::SourceRange member_name_range =
      cxx_dependent_scope_member_expr->getMemberNameInfo().getSourceRange();
  if (!member_name_range.isValid()) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[dependent-member-range]: member=%s has "
            "no exact source range\n",
            member_name.c_str());
    ROSE_ABORT();
  }

  SgScopeStatement *current_scope = SageBuilder::topScopeStack();
  ROSE_ASSERT(current_scope != nullptr);

  SgExpression *base_expr = nullptr;
  const bool has_implicit_access =
      cxx_dependent_scope_member_expr->isImplicitAccess();
  if (!has_implicit_access) {
    clang::Expr *base =
        static_cast<clang::Expr *>(cxx_dependent_scope_member_expr->getBase());
    SgNode *tmp_base = Traverse(base);
    base_expr = isSgExpression(tmp_base);
    ROSE_ASSERT(base_expr != nullptr);
  }

  clang::NestedNameSpecifierLoc qualifier_loc =
      cxx_dependent_scope_member_expr->getQualifierLoc();
  const clang::NestedNameSpecifierLoc *qualifier_loc_ptr =
      qualifier_loc.getNestedNameSpecifier() ? &qualifier_loc : nullptr;
  clang::SourceLocation exact_qualifier_begin;
  if (qualifier_loc_ptr != nullptr && qualifier_loc.getBeginLoc().isInvalid()) {
    if (has_implicit_access) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[dependent-member-qualifier-range]: "
              "member=%s has no qualifier begin and no explicit member "
              "operator owner\n",
              member_name.c_str());
      ROSE_ABORT();
    }
    clang::SourceManager &source_manager =
        p_compiler_instance->getSourceManager();
    const clang::LangOptions &lang_options = p_compiler_instance->getLangOpts();
    std::optional<clang::Token> qualifier_begin_token =
        clang::Lexer::findNextToken(
            cxx_dependent_scope_member_expr->getOperatorLoc(), source_manager,
            lang_options);
    if (qualifier_begin_token &&
        qualifier_begin_token->is(clang::tok::kw_template)) {
      qualifier_begin_token = clang::Lexer::findNextToken(
          qualifier_begin_token->getLocation(), source_manager, lang_options);
    }
    if (!qualifier_begin_token ||
        qualifier_begin_token->getLocation().isInvalid()) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[dependent-member-qualifier-range]: "
              "member=%s has no exact token after its explicit member "
              "operator\n",
              member_name.c_str());
      ROSE_ABORT();
    }
    exact_qualifier_begin = qualifier_begin_token->getLocation();
  }

  SgTemplateArgumentPtrList template_args;
  const SgTemplateArgumentPtrList *template_args_ptr = nullptr;
  SgTemplateArgumentPtrList semantic_template_args;
  const SgTemplateArgumentPtrList *semantic_template_args_ptr = nullptr;
  if (cxx_dependent_scope_member_expr->hasExplicitTemplateArgs()) {
    clang::TemplateArgumentListInfo arg_info;
    cxx_dependent_scope_member_expr->copyTemplateArgumentsInto(arg_info);
    template_args = buildTemplateArguments(arg_info, true);
    template_args_ptr = &template_args;
    semantic_template_args = buildSemanticTemplateArguments(
        arg_info, true, "dependent-scope-member-template-id");
    semantic_template_args_ptr = &semantic_template_args;
  }

  SgNonrealRefExp *member_ref = buildNonrealRefExpFromNestedNameSpecifier(
      cxx_dependent_scope_member_expr->getQualifier(), current_scope,
      SgName(member_name),
      cxx_dependent_scope_member_expr->hasTemplateKeyword(),
      semantic_template_args_ptr, template_args_ptr);
  ROSE_ASSERT(member_ref != nullptr);
  buildConversionFunctionId(
      member_ref, cxx_dependent_scope_member_expr->getMemberNameInfo());
  member_ref->set_explicit_template_argument_list(
      cxx_dependent_scope_member_expr->hasExplicitTemplateArgs());
  attachExplicitQualifierFromNestedName(
      member_ref, cxx_dependent_scope_member_expr->getQualifier(),
      qualifier_loc_ptr, p_compiler_instance, exact_qualifier_begin);
  if (!cxx_dependent_scope_member_expr->getQualifier())
    setExplicitQualifierOnRef(member_ref, SourceQualification{});

  if (base_expr != nullptr) {
    applySourceRange(member_ref, member_name_range);
    if (cxx_dependent_scope_member_expr->isArrow()) {
      SgArrowExp *arrow =
          SageBuilder::buildArrowExp_nfi(base_expr, member_ref, result_type);
      arrow->set_emission_role(SgArrowExp::e_emit_arrow_operator);
      *node = arrow;
    } else {
      *node = SageBuilder::buildDotExp_nfi(base_expr, member_ref, result_type);
    }
  } else {
    *node = member_ref;
    applySourceRange(member_ref,
                     cxx_dependent_scope_member_expr->getSourceRange());
  }

  if (base_expr != nullptr) {
    applySourceRange(isSgExpression(*node),
                     cxx_dependent_scope_member_expr->getSourceRange());
  }

  return VisitExpr(cxx_dependent_scope_member_expr, node);
}

bool ClangToSageTranslator::VisitCXXFoldExpr(clang::CXXFoldExpr *cxx_fold_expr,
                                             SgNode **node) {
  ASSERT_not_null(cxx_fold_expr);
  const auto fold_operator = [&]() -> SgFoldExpression::operator_kind_enum {
    switch (cxx_fold_expr->getOperator()) {
    case clang::BO_Add:
      return SgFoldExpression::e_fold_add;
    case clang::BO_Sub:
      return SgFoldExpression::e_fold_subtract;
    case clang::BO_Mul:
      return SgFoldExpression::e_fold_multiply;
    case clang::BO_Div:
      return SgFoldExpression::e_fold_divide;
    case clang::BO_Rem:
      return SgFoldExpression::e_fold_remainder;
    case clang::BO_Xor:
      return SgFoldExpression::e_fold_bit_xor;
    case clang::BO_And:
      return SgFoldExpression::e_fold_bit_and;
    case clang::BO_Or:
      return SgFoldExpression::e_fold_bit_or;
    case clang::BO_Assign:
      return SgFoldExpression::e_fold_assign;
    case clang::BO_LT:
      return SgFoldExpression::e_fold_less;
    case clang::BO_GT:
      return SgFoldExpression::e_fold_greater;
    case clang::BO_Shl:
      return SgFoldExpression::e_fold_shift_left;
    case clang::BO_Shr:
      return SgFoldExpression::e_fold_shift_right;
    case clang::BO_AddAssign:
      return SgFoldExpression::e_fold_add_assign;
    case clang::BO_SubAssign:
      return SgFoldExpression::e_fold_subtract_assign;
    case clang::BO_MulAssign:
      return SgFoldExpression::e_fold_multiply_assign;
    case clang::BO_DivAssign:
      return SgFoldExpression::e_fold_divide_assign;
    case clang::BO_RemAssign:
      return SgFoldExpression::e_fold_remainder_assign;
    case clang::BO_XorAssign:
      return SgFoldExpression::e_fold_xor_assign;
    case clang::BO_AndAssign:
      return SgFoldExpression::e_fold_and_assign;
    case clang::BO_OrAssign:
      return SgFoldExpression::e_fold_or_assign;
    case clang::BO_ShlAssign:
      return SgFoldExpression::e_fold_shift_left_assign;
    case clang::BO_ShrAssign:
      return SgFoldExpression::e_fold_shift_right_assign;
    case clang::BO_EQ:
      return SgFoldExpression::e_fold_equal;
    case clang::BO_NE:
      return SgFoldExpression::e_fold_not_equal;
    case clang::BO_LE:
      return SgFoldExpression::e_fold_less_equal;
    case clang::BO_GE:
      return SgFoldExpression::e_fold_greater_equal;
    case clang::BO_LAnd:
      return SgFoldExpression::e_fold_logical_and;
    case clang::BO_LOr:
      return SgFoldExpression::e_fold_logical_or;
    case clang::BO_Comma:
      return SgFoldExpression::e_fold_comma;
    case clang::BO_PtrMemD:
      return SgFoldExpression::e_fold_member_pointer;
    case clang::BO_PtrMemI:
      return SgFoldExpression::e_fold_indirect_member_pointer;
    default:
      std::cerr << "REX_FRONTEND_INVARIANT[fold-operator]: invalid Clang fold "
                   "operator"
                << std::endl;
      ROSE_ABORT();
    }
  }();
  clang::Expr *lhs = cxx_fold_expr->getLHS();
  clang::Expr *rhs = cxx_fold_expr->getRHS();
  if (lhs == nullptr && rhs == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[fold-operands]: CXXFoldExpr has no "
                 "exact left or right operand"
              << std::endl;
    ROSE_ABORT();
  }
  if (cxx_fold_expr->getType().isNull()) {
    std::cerr << "REX_FRONTEND_INVARIANT[fold-result-type]: CXXFoldExpr has "
                 "no Clang result type"
              << std::endl;
    ROSE_ABORT();
  }
  SgType *result_type = buildTypeFromQualifiedType(cxx_fold_expr->getType());
  if (SageInterface::containsUnknownType(result_type)) {
    std::cerr << "REX_FRONTEND_INVARIANT[fold-result-type]: CXXFoldExpr has "
                 "no exact translated result type"
              << std::endl;
    ROSE_ABORT();
  }
  const bool is_left_fold = cxx_fold_expr->isLeftFold();
  auto translate_fold_operand = [&](clang::Expr *operand,
                                    const char *role) -> SgExpression * {
    if (operand == nullptr) {
      return nullptr;
    }
    SgNode *translated = Traverse(operand);
    SgExpression *result = isSgExpression(translated);
    if (result == nullptr) {
      std::cerr << "REX_FRONTEND_INVARIANT[fold-operand]: " << role
                << " translated to "
                << (translated != nullptr ? translated->class_name()
                                          : std::string("<null>"))
                << " instead of SgExpression" << std::endl;
      ROSE_ABORT();
    }
    return result;
  };

  SgExpression *lhs_expr = translate_fold_operand(lhs, "left operand");
  SgExpression *rhs_expr = translate_fold_operand(rhs, "right operand");

  SgExpression *operands = nullptr;
  if (lhs_expr != nullptr && rhs_expr != nullptr) {
    SgExprListExp *list = SageBuilder::buildExprListExp_nfi();
    publishCanonicalSemanticExpressionSourceProvenance(
        list, "VisitCXXFoldExpr:operand-list");
    list->append_expression(lhs_expr);
    list->append_expression(rhs_expr);
    operands = list;
  } else if (lhs_expr != nullptr) {
    operands = lhs_expr;
  } else if (rhs_expr != nullptr) {
    operands = rhs_expr;
  }
  ROSE_ASSERT(operands != nullptr);
  SgFoldExpression *fold_expr = SageBuilder::buildFoldExpression_nfi(
      operands, fold_operator, is_left_fold, result_type);
  applySourceRange(fold_expr, cxx_fold_expr->getSourceRange());
  *node = fold_expr;

  return VisitExpr(cxx_fold_expr, node);
}

bool ClangToSageTranslator::VisitCXXNewExpr(clang::CXXNewExpr *cxx_new_expr,
                                            SgNode **node) {
  const auto source_range = expressionSourceRange(
      *cxx_new_expr, p_compiler_instance->getASTContext());
  clang::TypeSourceInfo *allocated_type_info =
      cxx_new_expr->getAllocatedTypeSourceInfo();
  if (allocated_type_info == nullptr) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[new-allocated-type-source]: "
            "new-expression=%p has no exact allocated TypeLoc\n",
            static_cast<void *>(cxx_new_expr));
    ROSE_ABORT();
  }
  clang::TypeLoc allocated_type_loc = allocated_type_info->getTypeLoc();
  SgType *allocated_value_type =
      buildTypeFromQualifiedType(cxx_new_expr->getAllocatedType());
  SgType *allocated_source_type =
      hasExactSemanticExpressionConstructionRole("new-allocated-type")
          ? buildTypeFromQualifiedType(allocated_type_info->getType())
          : buildTypeFromTypeLoc(allocated_type_loc);
  if (allocated_source_type == nullptr) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[new-allocated-type-source]: "
            "new-expression=%p allocated TypeLoc did not translate\n",
            static_cast<void *>(cxx_new_expr));
    ROSE_ABORT();
  }
  auto *result_type =
      isSgPointerType(buildTypeFromQualifiedType(cxx_new_expr->getType()));
  ROSE_ASSERT(result_type != nullptr);
  auto bound_source_form = SgNewExp::e_no_array_bound;
  if (cxx_new_expr->isArray()) {
    bound_source_form = SgNewExp::e_written_array_bound;
    auto &context = p_compiler_instance->getASTContext();
    if (!context.hasSameType(allocated_type_info->getType(),
                             cxx_new_expr->getAllocatedType())) {
      const auto *array =
          context.getAsConstantArrayType(allocated_type_info->getType());
      ROSE_ASSERT(array != nullptr &&
                  context.hasSameType(array->getElementType(),
                                      cxx_new_expr->getAllocatedType()));
      bound_source_form = SgNewExp::e_type_array_bound;
    }
  }
  if (!SgNewExp::specifiedTypeMatchesResult(allocated_source_type, result_type,
                                            bound_source_form)) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[new-result-type]: source=%s pointee=%s "
            "bound-source-form=%d has no exact typed allocation result\n",
            allocated_source_type->class_name().c_str(),
            result_type->get_base_type()->class_name().c_str(),
            static_cast<int>(bound_source_form));
    cxx_new_expr->dump();
    ROSE_ABORT();
  }

  auto classTypeUnknown = [allocated_value_type]() {
    return constructorInitializerAssociatedClassUnknown(allocated_value_type);
  };

  SgExprListExp *placementArgs = nullptr;
  const unsigned num_placement_args = cxx_new_expr->getNumPlacementArgs();
  if (num_placement_args > 0) {
    unsigned num_written_args = num_placement_args;
    while (num_written_args > 0 &&
           llvm::isa<clang::CXXDefaultArgExpr>(
               cxx_new_expr->getPlacementArg(num_written_args - 1)))
      --num_written_args;
    placementArgs = SageBuilder::buildExprListExp_nfi();
    for (unsigned i = 0; i < num_placement_args; ++i) {
      clang::Expr *placementArg = cxx_new_expr->getPlacementArg(i);
      ROSE_ASSERT(llvm::isa<clang::CXXDefaultArgExpr>(placementArg) ==
                  (i >= num_written_args));
      SgNode *tmpArg = Traverse(placementArg);
      SgExpression *expr = isSgExpression(tmpArg);
      if (expr == nullptr) {
        std::cerr << "REX_FRONTEND_INVARIANT[new-placement-argument]: Clang "
                     "argument "
                  << i << " translated to "
                  << (tmpArg == nullptr ? std::string("<null>")
                                        : tmpArg->class_name())
                  << " instead of SgExpression" << std::endl;
        ROSE_ABORT();
      }
      placementArgs->append_expression(expr);
    }
    if (num_written_args == 0) {
      SageBuilder::initializeSemanticExpressionSourceProvenance(placementArgs);
    } else {
      clang::SourceManager &source_manager =
          p_compiler_instance->getSourceManager();
      const clang::LangOptions &language_options =
          p_compiler_instance->getLangOpts();
      clang::Expr *first_placement_argument = cxx_new_expr->getPlacementArg(0);
      clang::Expr *last_placement_argument =
          cxx_new_expr->getPlacementArg(num_written_args - 1);
      const clang::SourceRange first_argument_range =
          first_placement_argument->getSourceRange();
      const clang::SourceRange last_argument_range =
          last_placement_argument->getSourceRange();
      const std::optional<clang::Token> left_delimiter =
          clang::Lexer::findPreviousToken(first_argument_range.getBegin(),
                                          source_manager, language_options,
                                          /*IncludeComments=*/false);
      const std::optional<clang::Token> right_delimiter =
          clang::Lexer::findNextToken(last_argument_range.getEnd(),
                                      source_manager, language_options,
                                      /*IncludeComments=*/false);
      if (!first_argument_range.isValid() || !last_argument_range.isValid() ||
          !left_delimiter.has_value() || !right_delimiter.has_value() ||
          !left_delimiter->is(clang::tok::l_paren) ||
          !right_delimiter->is(clang::tok::r_paren)) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[new-placement-source]: new-expression="
                "%p has %u placement arguments without one exact "
                "parenthesized source range\n",
                static_cast<void *>(cxx_new_expr), num_placement_args);
        ROSE_ABORT();
      }
      applySourceRange(placementArgs,
                       clang::SourceRange(left_delimiter->getLocation(),
                                          right_delimiter->getLocation()));
    }
  }

  SgConstructorInitializer *constructor_args = nullptr;
  if (const clang::CXXConstructExpr *construct_expr =
          llvm::dyn_cast_or_null<clang::CXXConstructExpr>(
              cxx_new_expr->getConstructExpr())) {
    SgNode *constructorInitializer =
        Traverse(const_cast<clang::CXXConstructExpr *>(construct_expr));
    constructor_args = isSgConstructorInitializer(constructorInitializer);
    if (constructor_args == nullptr) {
      std::cerr << "REX_FRONTEND_INVARIANT[new-constructor-initializer]: "
                   "Clang construct expression translated to "
                << (constructorInitializer == nullptr
                        ? std::string("<null>")
                        : constructorInitializer->class_name())
                << " instead of SgConstructorInitializer" << std::endl;
      ROSE_ABORT();
    }
  }

  const bool has_initializer = cxx_new_expr->hasInitializer();
  if (constructor_args == nullptr && has_initializer) {
    clang::Expr *initializer = cxx_new_expr->getInitializer();
    if (initializer == nullptr) {
      std::cerr << "REX_FRONTEND_INVARIANT[new-initializer]: Clang reports an "
                   "initializer but provides no expression"
                << std::endl;
      ROSE_ABORT();
    }
    const clang::CXXNewInitializationStyle initialization_style =
        cxx_new_expr->getInitializationStyle();
    clang::SourceRange initializer_source_range;
    switch (initialization_style) {
    case clang::CXXNewInitializationStyle::Parens:
      // An empty scalar value-initializer has no Expr source range.  Clang's
      // direct-initializer range is the authoritative spelling of the owned
      // parenthesized surface, including the empty `()` case.
      initializer_source_range = cxx_new_expr->getDirectInitRange();
      break;
    case clang::CXXNewInitializationStyle::Braces:
      initializer_source_range = initializer->getSourceRange();
      break;
    case clang::CXXNewInitializationStyle::None:
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[new-initializer-source]: "
              "new-expression=%p reports an initializer with no written "
              "initialization style\n",
              static_cast<void *>(cxx_new_expr));
      ROSE_ABORT();
    }
    if (!initializer_source_range.isValid()) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[new-initializer-source]: "
              "new-expression=%p style=%d has no exact written initializer "
              "range\n",
              static_cast<void *>(cxx_new_expr),
              static_cast<int>(initialization_style));
      ROSE_ABORT();
    }
    const bool uses_braces =
        llvm::isa<clang::InitListExpr>(initializer) ||
        llvm::isa<clang::CXXStdInitializerListExpr>(initializer);
    if (initialization_style == clang::CXXNewInitializationStyle::Parens &&
        llvm::isa<clang::ImplicitValueInitExpr>(initializer)) {
      // Clang publishes a semantic zero-initializer for `new T()`, but that
      // expression has no source spelling.  The CXXNewExpr's Parens style and
      // direct-init range are the exact source transaction: retain an empty
      // argument list instead of inventing `(T)0` as a written argument.
      SgExprListExp *args = SageBuilder::buildExprListExp_nfi();
      applySourceRange(args, initializer_source_range);
      constructor_args = SageBuilder::buildConstructorInitializer_nfi(
          nullptr, args, allocated_value_type,
          false,             // need_name
          false,             // need_qualifier
          true,              // need_parenthesis_after_name
          classTypeUnknown() // associated_class_unknown
      );
      constructor_args->set_is_braced_initialized(false);
      applySourceRange(constructor_args, initializer_source_range);
    } else {
      SgNode *translated_initializer = Traverse(initializer);
      if (SgConstructorInitializer *ctor_init =
              isSgConstructorInitializer(translated_initializer)) {
        constructor_args = ctor_init;
      } else if (SgExpression *expr = isSgExpression(translated_initializer)) {
        SgExprListExp *args = isSgExprListExp(expr);
        if (args == nullptr) {
          args = SageBuilder::buildExprListExp_nfi();
          args->append_expression(expr);
          applySourceRange(args, initializer_source_range);
        }

        constructor_args = SageBuilder::buildConstructorInitializer_nfi(
            nullptr, args, allocated_value_type,
            false,             // need_name
            false,             // need_qualifier
            !uses_braces,      // need_parenthesis_after_name
            classTypeUnknown() // associated_class_unknown
        );
        constructor_args->set_is_braced_initialized(uses_braces);
        applySourceRange(constructor_args, initializer_source_range);
      } else {
        std::cerr << "REX_FRONTEND_INVARIANT[new-initializer]: Clang "
                     "initializer translated to "
                  << (translated_initializer == nullptr
                          ? std::string("<null>")
                          : translated_initializer->class_name())
                  << " instead of SgExpression" << std::endl;
        ROSE_ABORT();
      }
    }
  }

  SgExpression *array_size = nullptr;
  if (cxx_new_expr->isArray()) {
    if (const std::optional<clang::Expr *> size_expr_opt =
            cxx_new_expr->getArraySize()) {
      if (clang::Expr *size_expr = *size_expr_opt) {
        clang::SourceManager &source_manager =
            p_compiler_instance->getSourceManager();
        const clang::IntegerLiteral *inferred_size =
            llvm::dyn_cast<clang::IntegerLiteral>(size_expr);
        clang::Token size_token;
        // LLVM 22 anchors inferred sizes at the TypeLoc end but records no
        // source-role flag. Preserve that role from exact tokens in the AST.
        const bool written_literal =
            inferred_size != nullptr &&
            !clang::Lexer::getRawToken(
                source_manager.getSpellingLoc(size_expr->getExprLoc()),
                size_token, source_manager,
                p_compiler_instance->getLangOpts()) &&
            size_token.is(clang::tok::numeric_constant);
        if (bound_source_form != SgNewExp::e_type_array_bound &&
            inferred_size != nullptr && !written_literal)
          bound_source_form = SgNewExp::e_inferred_array_bound;
        const bool semantic_bound =
            bound_source_form == SgNewExp::e_inferred_array_bound ||
            bound_source_form == SgNewExp::e_type_array_bound;
        if (semantic_bound) {
          const clang::SourceLocation size_location =
              source_manager.getSpellingLoc(size_expr->getExprLoc());
          const clang::SourceLocation allocated_type_end =
              source_manager.getSpellingLoc(allocated_type_loc.getEndLoc());
          if (inferred_size == nullptr || size_location.isInvalid() ||
              size_location != allocated_type_end ||
              (bound_source_form == SgNewExp::e_inferred_array_bound &&
               !has_initializer)) {
            std::cerr
                << "REX_FRONTEND_INVARIANT[new-inferred-array-bound]: Clang "
                   "array size has no exact type-anchored integral bound"
                << std::endl;
            ROSE_ABORT();
          }
          ++p_semantic_expression_subtree_depth;
          ++p_canonical_generated_literal_depth;
        }
        SgNode *translated_size = Traverse(size_expr);
        if (semantic_bound) {
          if (p_semantic_expression_subtree_depth == 0 ||
              p_canonical_generated_literal_depth == 0) {
            std::cerr << "REX_FRONTEND_INVARIANT[new-inferred-array-bound]: "
                         "semantic bound translation depth underflowed"
                      << std::endl;
            ROSE_ABORT();
          }
          --p_canonical_generated_literal_depth;
          --p_semantic_expression_subtree_depth;
        }
        array_size = isSgExpression(translated_size);
        if (array_size == nullptr) {
          std::cerr << "REX_FRONTEND_INVARIANT[new-array-bound]: Clang array "
                       "bound translated to "
                    << (translated_size == nullptr
                            ? std::string("<null>")
                            : translated_size->class_name())
                    << " instead of SgExpression" << std::endl;
          ROSE_ABORT();
        }
      }
    }

    ROSE_ASSERT(array_size != nullptr);
  }

  clang::FunctionDecl *allocation_function = cxx_new_expr->getOperatorNew();
  SgFunctionDeclaration *allocation_declaration =
      allocation_function != nullptr
          ? isSgFunctionDeclaration(Traverse(allocation_function))
          : nullptr;
  ROSE_ASSERT(allocation_function != nullptr
                  ? allocation_declaration != nullptr
                  : cxx_new_expr->isInstantiationDependent());
  const unsigned implicit_argument_count = cxx_new_expr->getNumImplicitArgs();
  ROSE_ASSERT(placementArgs == nullptr ||
              SgCxxDefaultInitExp::is_valid_binding(
                  allocation_declaration, placementArgs->get_expressions(),
                  implicit_argument_count));
  if (constructor_args != nullptr) {
    constructor_args->set_need_name(false);
  }

  SgExpression *builtin_args = nullptr;
  short int need_global_specifier = (short int)cxx_new_expr->isGlobalNew();

  SgNewExp *newExp = SageBuilder::buildNewExp_nfi(
      allocated_source_type, result_type, array_size, bound_source_form,
      placementArgs, constructor_args, builtin_args, need_global_specifier,
      allocation_declaration, implicit_argument_count);
  newExp->set_type_id_is_parenthesized(cxx_new_expr->isParenTypeId());
  SgStringList qualifier_tokens;
  bool global_qualification = false;
  (void)exactTypeLocSourceQualification(allocated_type_loc, qualifier_tokens,
                                        global_qualification);
  newExp->set_explicit_name_qualification_present(true);
  newExp->set_explicit_global_qualification(global_qualification);
  newExp->set_explicit_name_qualification_tokens(qualifier_tokens);
  applySourceRange(newExp, source_range);
  *node = newExp;

  return VisitExpr(cxx_new_expr, node);
}

bool ClangToSageTranslator::VisitCXXNoexceptExpr(
    clang::CXXNoexceptExpr *cxx_noexcept_expr, SgNode **node) {
  SgExpression *operand = nullptr;
  if (clang::Expr *arg = cxx_noexcept_expr->getOperand()) {
    SgNode *tmp = Traverse(arg);
    operand = isSgExpression(tmp);
  }
  if (operand == nullptr) {
    failExactExpressionTranslation("noexcept-operand", cxx_noexcept_expr);
  }

  SgNoexceptOp *noexcept_op = SageBuilder::buildNoexceptOp_nfi(operand);
  *node = noexcept_op;

  if (SgExpression *expr = isSgExpression(*node)) {
    applySourceRange(expr, cxx_noexcept_expr->getSourceRange());
  }

  return VisitExpr(cxx_noexcept_expr, node);
}

bool ClangToSageTranslator::VisitRequiresExpr(
    clang::RequiresExpr *requires_expr, SgNode **node) {
  struct PriorRequiresParameterTranslation {
    clang::ParmVarDecl *parameter;
    SgNode *translation;
    bool was_present;
  };
  std::vector<PriorRequiresParameterTranslation>
      prior_requires_parameter_translations;
  prior_requires_parameter_translations.reserve(
      requires_expr->getLocalParameters().size());
  for (clang::ParmVarDecl *parameter : requires_expr->getLocalParameters()) {
    auto prior = p_decl_translation_map.find(parameter);
    prior_requires_parameter_translations.push_back(
        {parameter,
         prior != p_decl_translation_map.end() ? prior->second : nullptr,
         prior != p_decl_translation_map.end()});
    if (prior != p_decl_translation_map.end()) {
      p_decl_translation_map.erase(prior);
    }
  }
  auto restore_requires_parameter_translations = [&]() {
    for (const PriorRequiresParameterTranslation &prior :
         prior_requires_parameter_translations) {
      p_decl_translation_map.erase(prior.parameter);
      if (prior.was_present) {
        p_decl_translation_map.emplace(prior.parameter, prior.translation);
      }
    }
  };
  struct RequiresParameterConstructionGuard {
    std::vector<RequiresParameterConstructionFrame> &frames;
    const clang::RequiresExpr *expression = nullptr;
    SgFunctionParameterScope *scope = nullptr;
    std::size_t frame_index = std::numeric_limits<std::size_t>::max();

    void requireActive(const char *context) const {
      const bool active =
          expression != nullptr && scope != nullptr &&
          frame_index != std::numeric_limits<std::size_t>::max() &&
          frame_index < frames.size() && frame_index + 1 == frames.size() &&
          frames[frame_index].expression == expression &&
          frames[frame_index].scope == scope;
      if (!active) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[requires-local-parameter-owner]: "
                "requires-expression=%p scope=%p frame-index=%zu "
                "frame-count=%zu lost its exact construction transaction "
                "during %s\n",
                static_cast<const void *>(expression),
                static_cast<void *>(scope), frame_index, frames.size(),
                context);
        ROSE_ABORT();
      }
    }

    void activate(const clang::RequiresExpr *new_expression,
                  SgFunctionParameterScope *new_scope) {
      if (expression != nullptr || scope != nullptr ||
          frame_index != std::numeric_limits<std::size_t>::max() ||
          new_expression == nullptr || new_scope == nullptr || frames.empty()) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[requires-local-parameter-owner]: "
                "requires-expression=%p scope=%p cannot activate one exact "
                "construction transaction\n",
                static_cast<const void *>(new_expression),
                static_cast<void *>(new_scope));
        ROSE_ABORT();
      }
      expression = new_expression;
      scope = new_scope;
      frame_index = frames.size() - 1;
      requireActive("activation");
    }

    void finish() {
      if (expression == nullptr) {
        if (scope != nullptr ||
            frame_index != std::numeric_limits<std::size_t>::max()) {
          fprintf(stderr,
                  "REX_FRONTEND_INVARIANT[requires-local-parameter-owner]: "
                  "inactive construction guard retains partial state\n");
          ROSE_ABORT();
        }
        return;
      }
      requireActive("completion");
      frames.pop_back();
      expression = nullptr;
      scope = nullptr;
      frame_index = std::numeric_limits<std::size_t>::max();
    }

    ~RequiresParameterConstructionGuard() { finish(); }
  } requires_parameter_guard{p_requires_parameter_construction_stack};

  SgFunctionParameterList *local_parameters = nullptr;
  SgFunctionParameterList *written_parameters = nullptr;
  SgFunctionParameterScope *local_parameter_scope = nullptr;
  if (requires_expr->getLParenLoc().isValid()) {
    local_parameter_scope = new SgFunctionParameterScope();
    ROSE_ASSERT(local_parameter_scope != nullptr);
    SgScopeStatement *enclosing_scope = SageBuilder::topScopeStack();
    if (enclosing_scope == nullptr) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[requires-local-parameter-scope]: "
              "requires-expression has no exact enclosing semantic scope\n");
      ROSE_ABORT();
    }
    setSynthesizedFileInfo(local_parameter_scope);
    SageInterface::beginDetachedFunctionParameterScopeConstruction(
        local_parameter_scope, enclosing_scope, enclosing_scope);
    SageBuilder::pushScopeStack(local_parameter_scope);

    local_parameters = SageBuilder::buildFunctionParameterList_nfi();
    ROSE_ASSERT(local_parameters != nullptr);
    applySourceRange(local_parameters,
                     clang::SourceRange(requires_expr->getLParenLoc(),
                                        requires_expr->getRParenLoc()));
    RequiresParameterConstructionFrame requires_parameter_frame;
    requires_parameter_frame.expression = requires_expr;
    requires_parameter_frame.scope = local_parameter_scope;
    for (clang::ParmVarDecl *parameter : requires_expr->getLocalParameters()) {
      if (parameter == nullptr ||
          !requires_parameter_frame.parameters.insert(parameter).second) {
        std::cerr
            << "REX_FRONTEND_INVARIANT[requires-local-parameter-owner]: "
               "requires-expression contains a null or duplicate exact local "
               "parameter identity"
            << std::endl;
        ROSE_ABORT();
      }
    }
    p_requires_parameter_construction_stack.push_back(
        std::move(requires_parameter_frame));
    requires_parameter_guard.activate(requires_expr, local_parameter_scope);
    std::vector<SgInitializedName *> written_parameter_nodes;
    for (clang::ParmVarDecl *parameter : requires_expr->getLocalParameters()) {
      p_requires_parameter_construction_stack.back().written_parameter =
          nullptr;
      SgInitializedName *translated_parameter =
          isSgInitializedName(Traverse(parameter));
      if (translated_parameter == nullptr) {
        std::cerr << "REX_FRONTEND_INVARIANT[requires-local-parameter]: Clang "
                  << (parameter != nullptr ? parameter->getNameAsString()
                                           : std::string("<null>"))
                  << " did not translate to an exact SgInitializedName"
                  << std::endl;
        ROSE_ABORT();
      }
      local_parameters->get_args().push_back(translated_parameter);
      translated_parameter->set_parent(local_parameters);
      RequiresParameterConstructionFrame &active_requires_frame =
          p_requires_parameter_construction_stack.back();
      written_parameter_nodes.push_back(
          active_requires_frame.written_parameter);
      if (active_requires_frame.expression != requires_expr ||
          active_requires_frame.scope != local_parameter_scope ||
          active_requires_frame.parameters.count(parameter) != 1 ||
          !active_requires_frame.translated_parameters
               .emplace(parameter, translated_parameter)
               .second) {
        std::cerr << "REX_FRONTEND_INVARIANT[requires-local-parameter-owner]: "
                     "translated parameter could not publish one exact "
                     "bound-variable identity"
                  << std::endl;
        ROSE_ABORT();
      }
      if (translated_parameter->get_scope() != local_parameter_scope) {
        std::cerr << "REX_FRONTEND_INVARIANT[requires-local-parameter-scope]: "
                  << "parameter="
                  << translated_parameter->get_name().getString()
                  << " was not published in the exact requires-expression scope"
                  << std::endl;
        ROSE_ABORT();
      }
      if (!translated_parameter->get_name().getString().empty() &&
          local_parameter_scope->find_symbol_from_declaration(
              translated_parameter) == nullptr) {
        std::cerr << "REX_FRONTEND_INVARIANT[requires-local-parameter-symbol]: "
                  << "parameter="
                  << translated_parameter->get_name().getString()
                  << " has no symbol in the exact requires-expression scope"
                  << std::endl;
        ROSE_ABORT();
      }
    }
    if (std::any_of(written_parameter_nodes.begin(),
                    written_parameter_nodes.end(),
                    [](auto *parameter) { return parameter != nullptr; })) {
      written_parameters = SageBuilder::buildFunctionParameterList_nfi();
      applySourceRange(written_parameters,
                       clang::SourceRange(requires_expr->getLParenLoc(),
                                          requires_expr->getRParenLoc()));
      size_t index = 0;
      for (clang::ParmVarDecl *parameter :
           requires_expr->getLocalParameters()) {
        auto *written = written_parameter_nodes[index];
        if (written == nullptr) {
          auto *semantic = local_parameters->get_args()[index];
          written = cloneParameter(semantic, parameter, semantic->get_type(),
                                   ParameterCloneRole::written);
        }
        ROSE_ASSERT(written->get_parent() == nullptr &&
                    written->get_scope() == local_parameter_scope);
        written->set_parent(written_parameters);
        written_parameters->get_args().push_back(written);
        ++index;
      }
    }
    requires_parameter_guard.requireActive("local-parameter construction");
  }

  SgExprListExp *requirements = SageBuilder::buildExprListExp_nfi();
  ROSE_ASSERT(requirements != nullptr);
  applySourceRange(requirements, requires_expr->getSourceRange());

  auto build_substitution_failure =
      [&](clang::concepts::Requirement::SubstitutionDiagnostic *diagnostic,
          SgRequirementSubstitutionFailure::failure_kind_enum failure_kind)
      -> SgRequirementSubstitutionFailure * {
    if (diagnostic == nullptr) {
      std::cerr << "REX_FRONTEND_INVARIANT[requirement-substitution-"
                   "failure]: Clang reported a substitution failure without "
                   "its exact diagnostic record"
                << std::endl;
      ROSE_ABORT();
    }
    const std::string substituted_entity = diagnostic->SubstitutedEntity.str();
    const std::string diagnostic_message = diagnostic->DiagMessage.str();
    const clang::SourceLocation diagnostic_location = diagnostic->DiagLoc;
    if (substituted_entity.empty() || diagnostic_location.isInvalid()) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[requirement-substitution-failure]: "
              "kind=%d has empty substituted entity or invalid diagnostic "
              "location\n",
              static_cast<int>(failure_kind));
      ROSE_ABORT();
    }
    SgRequirementSubstitutionFailure *failure =
        SageBuilder::buildRequirementSubstitutionFailure_nfi(
            failure_kind, substituted_entity, diagnostic_message);
    applySourceRange(
        failure, clang::SourceRange(diagnostic_location, diagnostic_location));
    return failure;
  };

  for (clang::concepts::Requirement *requirement :
       requires_expr->getRequirements()) {
    if (local_parameter_scope != nullptr) {
      requires_parameter_guard.requireActive("requirement translation");
    }
    if (requirement == nullptr) {
      std::cerr << "REX_FRONTEND_INVARIANT[requires-requirement]: null Clang "
                   "requirement"
                << std::endl;
      ROSE_ABORT();
    }

    SgExpression *translated_requirement = nullptr;
    switch (requirement->getKind()) {
    case clang::concepts::Requirement::RK_Type: {
      clang::concepts::TypeRequirement *type_requirement =
          llvm::cast<clang::concepts::TypeRequirement>(requirement);
      if (type_requirement->isSubstitutionFailure()) {
        translated_requirement = build_substitution_failure(
            type_requirement->getSubstitutionDiagnostic(),
            SgRequirementSubstitutionFailure::e_type_requirement_failure);
        break;
      }
      if (type_requirement->getType() == nullptr) {
        std::cerr << "REX_FRONTEND_INVARIANT[type-requirement]: Clang type "
                     "requirement has neither an exact source type nor an "
                     "explicit substitution failure"
                  << std::endl;
        ROSE_ABORT();
      }
      clang::TypeLoc type_loc = type_requirement->getType()->getTypeLoc();
      SgType *required_type = buildTypeFromTypeLoc(type_loc);
      if (required_type == nullptr) {
        std::cerr << "REX_FRONTEND_INVARIANT[type-requirement]: exact TypeLoc "
                     "translation failed"
                  << std::endl;
        ROSE_ABORT();
      }
      SgTypeRequirement *sage_requirement =
          SageBuilder::buildTypeRequirement_nfi(required_type);
      SgStringList qualifier_tokens;
      bool global_qualification = false;
      (void)exactTypeLocSourceQualification(type_loc, qualifier_tokens,
                                            global_qualification);
      sage_requirement->set_explicit_name_qualification_present(true);
      sage_requirement->set_explicit_global_qualification(global_qualification);
      sage_requirement->set_explicit_name_qualification_tokens(
          qualifier_tokens);
      applySourceRange(sage_requirement, type_loc.getSourceRange());
      translated_requirement = sage_requirement;
      break;
    }

    case clang::concepts::Requirement::RK_Simple:
    case clang::concepts::Requirement::RK_Compound: {
      clang::concepts::ExprRequirement *expr_requirement =
          llvm::cast<clang::concepts::ExprRequirement>(requirement);
      if (expr_requirement->isExprSubstitutionFailure()) {
        translated_requirement = build_substitution_failure(
            expr_requirement->getExprSubstitutionDiagnostic(),
            expr_requirement->isSimple()
                ? SgRequirementSubstitutionFailure::e_simple_expression_failure
                : SgRequirementSubstitutionFailure::
                      e_compound_expression_failure);
        break;
      }
      clang::Expr *clang_expression = expr_requirement->getExpr();
      SgExpression *sage_expression =
          isSgExpression(Traverse(clang_expression));
      if (sage_expression == nullptr) {
        std::cerr << "REX_FRONTEND_INVARIANT[expression-requirement]: Clang "
                  << (clang_expression != nullptr
                          ? clang_expression->getStmtClassName()
                          : "<null>")
                  << " did not translate to an exact SgExpression" << std::endl;
        ROSE_ABORT();
      }

      if (expr_requirement->isSimple()) {
        translated_requirement =
            SageBuilder::buildSimpleRequirement_nfi(sage_expression);
      } else {
        SgExpression *type_constraint = nullptr;
        const clang::concepts::ExprRequirement::ReturnTypeRequirement
            &return_requirement = expr_requirement->getReturnTypeRequirement();
        if (!return_requirement.isEmpty()) {
          if (return_requirement.isSubstitutionFailure()) {
            type_constraint = build_substitution_failure(
                return_requirement.getSubstitutionDiagnostic(),
                SgRequirementSubstitutionFailure::
                    e_compound_return_type_failure);
          } else if (!return_requirement.isTypeConstraint()) {
            std::cerr << "REX_FRONTEND_INVARIANT[compound-requirement]: "
                         "return type requirement has neither an exact type "
                         "constraint nor an explicit substitution failure"
                      << std::endl;
            ROSE_ABORT();
          } else {
            const clang::TypeConstraint *clang_constraint =
                return_requirement.getTypeConstraint();
            const clang::ConceptReference *concept_reference =
                clang_constraint != nullptr
                    ? clang_constraint->getConceptReference()
                    : nullptr;
            if (concept_reference == nullptr) {
              std::cerr << "REX_FRONTEND_INVARIANT[compound-requirement]: "
                           "type constraint has no exact named concept"
                        << std::endl;
              ROSE_ABORT();
            }

            type_constraint = buildConceptReference(*concept_reference);
          }
        }
        translated_requirement = SageBuilder::buildCompoundRequirement_nfi(
            sage_expression, expr_requirement->hasNoexceptRequirement(),
            type_constraint);
      }
      applySourceRange(translated_requirement,
                       clang_expression->getSourceRange());
      break;
    }

    case clang::concepts::Requirement::RK_Nested: {
      clang::concepts::NestedRequirement *nested_requirement =
          llvm::cast<clang::concepts::NestedRequirement>(requirement);
      if (nested_requirement->hasInvalidConstraint()) {
        std::cerr << "REX_FRONTEND_INVARIANT[nested-requirement]: Clang "
                     "requirement has no exact constraint expression"
                  << std::endl;
        ROSE_ABORT();
      }
      clang::Expr *constraint = nested_requirement->getConstraintExpr();
      SgExpression *sage_constraint = translateConstraintExpression(constraint);
      SgNestedRequirement *sage_requirement =
          SageBuilder::buildNestedRequirement_nfi(sage_constraint);
      applySourceRange(sage_requirement, constraint->getSourceRange());
      translated_requirement = sage_requirement;
      break;
    }
    }

    if (translated_requirement == nullptr) {
      std::cerr << "REX_FRONTEND_INVARIANT[requires-requirement]: unsupported "
                   "Clang requirement kind="
                << static_cast<int>(requirement->getKind()) << std::endl;
      ROSE_ABORT();
    }
    requirements->append_expression(translated_requirement);
  }
  if (local_parameter_scope != nullptr) {
    requires_parameter_guard.requireActive("completed requirement list");
  }

  SgRequiresExpr *sage_requires = SageBuilder::buildRequiresExpr_nfi(
      local_parameters, requirements, local_parameter_scope,
      written_parameters);
  ROSE_ASSERT(sage_requires != nullptr);
  if (local_parameter_scope != nullptr) {
    ROSE_ASSERT(SageBuilder::topScopeStack() == local_parameter_scope);
    SageBuilder::popScopeStack();
  }
  applySourceRange(sage_requires, requires_expr->getSourceRange());
  *node = sage_requires;
  const bool result = VisitExpr(requires_expr, node);
  requires_parameter_guard.finish();
  restore_requires_parameter_translations();
  return result;
}

bool ClangToSageTranslator::VisitCXXNullPtrLiteralExpr(
    clang::CXXNullPtrLiteralExpr *cxx_null_ptr_literal_expr, SgNode **node) {
  // Create the nullptr value expression without forcing a nonreal
  // std::nullptr_t declaration.
  const bool semantic_expression =
      hasExactSemanticExpressionConstructionRole("VisitCXXNullPtrLiteralExpr");
  *node = SageBuilder::buildNullptrValExp_nfi();
  isSgValueExp(*node)->set_literal_spelling_form(
      semantic_expression ? SgValueExp::e_literal_canonical_generated
                          : SgValueExp::e_literal_source_spelled);

  return VisitExpr(cxx_null_ptr_literal_expr, node);
}

bool ClangToSageTranslator::VisitCXXPseudoDestructorExpr(
    clang::CXXPseudoDestructorExpr *cxx_pseudo_destructor_expr, SgNode **node) {
  clang::TypeSourceInfo *type_info =
      cxx_pseudo_destructor_expr->getDestroyedTypeInfo();
  if (type_info != nullptr && type_info->getType().isNull())
    failExactExpressionTranslation("pseudo-destructor-written-type",
                                   cxx_pseudo_destructor_expr);
  clang::SourceLocation tilde_loc = cxx_pseudo_destructor_expr->getTildeLoc();
  clang::SourceLocation name_end_loc =
      type_info != nullptr ? type_info->getTypeLoc().getEndLoc()
                           : cxx_pseudo_destructor_expr->getDestroyedTypeLoc();
  clang::SourceRange expr_range = cxx_pseudo_destructor_expr->getSourceRange();
  if (!tilde_loc.isValid() || !name_end_loc.isValid() || !expr_range.isValid())
    failExactExpressionTranslation("pseudo-destructor-source",
                                   cxx_pseudo_destructor_expr);

  SgExpression *destructor_reference = nullptr;
  SgType *callee_type = nullptr;
  if (type_info != nullptr) {
    // LLVM 22 can give the destructor identifier trivial TypeLocs containing
    // its lookup qualifier. That qualifier is written before '~', not after it.
    SgType *destroyed_type =
        llvm::isa<clang::TypedefType>(type_info->getType().getTypePtr())
            ? buildTypeFromQualifiedType(type_info->getType())
            : buildTypeFromTypeLoc(type_info->getTypeLoc());
    if (destroyed_type == nullptr ||
        SageInterface::containsUnknownType(destroyed_type))
      failExactExpressionTranslation("pseudo-destructor-written-type",
                                     cxx_pseudo_destructor_expr);
    callee_type = new SgMemberFunctionType(SageBuilder::buildVoidType(), false,
                                           new SgFunctionParameterTypeList(),
                                           destroyed_type);
    destructor_reference =
        new SgPseudoDestructorRefExp(destroyed_type, callee_type);
  } else {
    const clang::IdentifierInfo *id =
        cxx_pseudo_destructor_expr->getDestroyedTypeIdentifier();
    ROSE_ASSERT(id != nullptr);
    SgScopeStatement *scope = SageBuilder::topScopeStack();
    ROSE_ASSERT(scope != nullptr);
    auto *reference = buildNonrealRefExpFromNestedNameSpecifier(
        cxx_pseudo_destructor_expr->getQualifier(), scope,
        SgName(id->getName().str()), false, nullptr, nullptr);
    ROSE_ASSERT(reference != nullptr);
    reference->set_name_kind(SgNonrealRefExp::e_destructor_id);
    reference->set_semantic_role(SgNonrealRefExp::e_dependent_callable);
    auto qualifier_loc = cxx_pseudo_destructor_expr->getQualifierLoc();
    attachExplicitQualifierFromNestedName(
        reference, cxx_pseudo_destructor_expr->getQualifier(), &qualifier_loc,
        p_compiler_instance);
    destructor_reference = reference;
    callee_type = reference->get_type();
  }
  ROSE_ASSERT(destructor_reference != nullptr && callee_type != nullptr);

  clang::Expr *clang_base = cxx_pseudo_destructor_expr->getBase();
  SgNode *tmp_base = Traverse(clang_base);
  SgExpression *base = isSgExpression(tmp_base);
  ROSE_ASSERT(base != nullptr);

  applySourceRange(destructor_reference,
                   clang::SourceRange(tilde_loc, name_end_loc));

  if (clang::TypeSourceInfo *scope_info =
          cxx_pseudo_destructor_expr->getScopeTypeInfo()) {
    const clang::TypeLoc scope_loc = scope_info->getTypeLoc();
    SgType *scope_type = buildTypeFromTypeLoc(scope_loc);
    if (scope_type == nullptr || SageInterface::containsUnknownType(scope_type))
      failExactExpressionTranslation("pseudo-destructor-scope-type",
                                     cxx_pseudo_destructor_expr);
    auto *scope = SageBuilder::buildTypeExpression_nfi(scope_type);
    applySourceRange(scope, scope_loc.getSourceRange());
    destructor_reference =
        SageBuilder::buildScopeOp_nfi(scope, destructor_reference, callee_type);
    applySourceRange(destructor_reference,
                     clang::SourceRange(scope_loc.getBeginLoc(), name_end_loc));
  }

  SgExpression *callee = nullptr;
  if (cxx_pseudo_destructor_expr->isArrow()) {
    callee = SageBuilder::buildBinaryExpression_nfi<SgArrowExp>(
        base, destructor_reference, callee_type);
  } else {
    callee = SageBuilder::buildBinaryExpression_nfi<SgDotExp>(
        base, destructor_reference, callee_type);
  }
  ROSE_ASSERT(callee != nullptr);
  applySourceRange(callee, expr_range);

  *node = callee;

  return VisitExpr(cxx_pseudo_destructor_expr, node);
}

bool ClangToSageTranslator::VisitCXXRewrittenBinaryOperator(
    clang::CXXRewrittenBinaryOperator *cxx_rewrite_binary_operator,
    SgNode **node) {
  auto translate_operand = [&](const clang::Expr *operand) -> SgExpression * {
    if (operand == nullptr) {
      std::cerr << "REX_FRONTEND_INVARIANT[rewritten-binary-operand]: required "
                   "Clang operand is absent"
                << std::endl;
      ROSE_ABORT();
    }
    SgNode *tmp_operand = Traverse(const_cast<clang::Expr *>(operand));
    SgExpression *expr = isSgExpression(tmp_operand);
    if (expr == nullptr) {
      std::cerr << "REX_FRONTEND_INVARIANT[rewritten-binary-operand]: Clang "
                   "operand translated to "
                << (tmp_operand == nullptr ? std::string("<null>")
                                           : tmp_operand->class_name())
                << " instead of SgExpression" << std::endl;
      ROSE_ABORT();
    }
    return expr;
  };

  SgType *result_type =
      buildTypeFromQualifiedType(cxx_rewrite_binary_operator->getType());
  if (SageInterface::containsUnknownType(result_type)) {
    std::cerr << "REX_FRONTEND_INVARIANT[rewritten-binary-result-type]: Clang "
              << cxx_rewrite_binary_operator->getOpcodeStr().str()
              << " expression has no exact semantic result type" << std::endl;
    ROSE_ABORT();
  }
  SgExpression *lhs = translate_operand(cxx_rewrite_binary_operator->getLHS());
  SgExpression *rhs = translate_operand(cxx_rewrite_binary_operator->getRHS());
  switch (cxx_rewrite_binary_operator->getOpcode()) {
  case clang::BO_Cmp:
    *node = SageBuilder::buildSpaceshipOp_nfi(lhs, rhs, result_type);
    break;
  case clang::BO_LT:
    *node = SageBuilder::buildLessThanOp_nfi(lhs, rhs, result_type);
    break;
  case clang::BO_GT:
    *node = SageBuilder::buildGreaterThanOp_nfi(lhs, rhs, result_type);
    break;
  case clang::BO_LE:
    *node = SageBuilder::buildLessOrEqualOp_nfi(lhs, rhs, result_type);
    break;
  case clang::BO_GE:
    *node = SageBuilder::buildGreaterOrEqualOp_nfi(lhs, rhs, result_type);
    break;
  case clang::BO_EQ:
    *node = SageBuilder::buildEqualityOp_nfi(lhs, rhs, result_type);
    break;
  case clang::BO_NE:
    *node = SageBuilder::buildNotEqualOp_nfi(lhs, rhs, result_type);
    break;
  default:
    std::cerr << "REX_FRONTEND_INVARIANT[rewritten-binary-kind]: unsupported "
                 "Clang opcode "
              << cxx_rewrite_binary_operator->getOpcodeStr().str() << std::endl;
    ROSE_ABORT();
  }
  ASSERT_not_null(*node);
  applySourceRange(*node, cxx_rewrite_binary_operator->getSourceRange());

  return VisitExpr(cxx_rewrite_binary_operator, node);
}

bool ClangToSageTranslator::VisitCXXScalarValueInitExpr(
    clang::CXXScalarValueInitExpr *cxx_scalar_value_init_expr, SgNode **node) {
  clang::TypeSourceInfo *type_info =
      cxx_scalar_value_init_expr->getTypeSourceInfo();
  if (type_info == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[scalar-value-init-type-source]: "
                 "source-written scalar value-initialization has no TypeLoc"
              << std::endl;
    ROSE_ABORT();
  }
  clang::TypeLoc type_loc = type_info->getTypeLoc();
  if (type_loc.isNull() || type_loc.getSourceRange().isInvalid()) {
    std::cerr << "REX_FRONTEND_INVARIANT[scalar-value-init-type-source]: "
                 "source-written scalar value-initialization has an invalid "
                 "TypeLoc"
              << std::endl;
    ROSE_ABORT();
  }
  SgType *type = buildTypeFromTypeLoc(type_loc);
  if (type == nullptr || SageInterface::containsUnknownType(type)) {
    std::cerr << "REX_FRONTEND_INVARIANT[scalar-value-init-type]: exact "
                 "source TypeLoc did not produce a semantic Sage type"
              << std::endl;
    ROSE_ABORT();
  }

  clang::SourceLocation right_paren =
      cxx_scalar_value_init_expr->getRParenLoc();
  if (right_paren.isInvalid()) {
    std::cerr << "REX_FRONTEND_INVARIANT[scalar-value-init-source]: "
                 "source-written scalar value-initialization has no closing "
                 "parenthesis"
              << std::endl;
    ROSE_ABORT();
  }
  SgExprListExp *arguments = SageBuilder::buildExprListExp_nfi();
  applySourceRange(arguments, clang::SourceRange(right_paren, right_paren));
  SgConstructorInitializer *value_init =
      SageBuilder::buildConstructorInitializer_nfi(
          nullptr, arguments, type,
          true,  // source explicitly names the value-initialized type
          false, // qualification is computed for this exact type use site
          true,  // `T()` owns an explicit empty parenthesized argument list
          constructorInitializerAssociatedClassUnknown(type));
  value_init->set_is_braced_initialized(false);
  applySourceRange(value_init, cxx_scalar_value_init_expr->getSourceRange());
  *node = value_init;

  return VisitExpr(cxx_scalar_value_init_expr, node);
}

bool ClangToSageTranslator::VisitCXXStdInitializerListExpr(
    clang::CXXStdInitializerListExpr *cxx_std_initializer_list_expr,
    SgNode **node) {
  clang::Expr *sub_expr = cxx_std_initializer_list_expr->getSubExpr();
  if (sub_expr == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[std-initializer-list-subexpression]: "
                 "Clang expression has no subexpression"
              << std::endl;
    ROSE_ABORT();
  }

  SgNode *tmp_sub = Traverse(sub_expr);
  if (tmp_sub == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[std-initializer-list-subexpression]: "
                 "Clang subexpression translated to <null>"
              << std::endl;
    ROSE_ABORT();
  }

  SgExprListExp *expr_list = isSgExprListExp(tmp_sub);
  if (expr_list == nullptr) {
    if (SgAggregateInitializer *agg_init = isSgAggregateInitializer(tmp_sub)) {
      ROSE_ASSERT(sub_expr->getType()->isArrayType());
      agg_init->set_source_form(
          SgAggregateInitializer::e_aggregate_initializer_source_unbraced);
    }

    SgExpression *expr = isSgExpression(tmp_sub);
    if (expr == nullptr) {
      std::cerr << "REX_FRONTEND_INVARIANT[std-initializer-list-"
                   "subexpression]: translated node "
                << tmp_sub->class_name() << " is not an expression"
                << std::endl;
      ROSE_ABORT();
    }

    expr_list = SageBuilder::buildExprListExp_nfi();
    expr_list->append_expression(expr);
    applySourceRange(expr_list,
                     cxx_std_initializer_list_expr->getSourceRange());
  }

  SgType *init_list_type =
      buildTypeFromQualifiedType(cxx_std_initializer_list_expr->getType());
  SgAggregateInitializer *agg_init = SageBuilder::buildAggregateInitializer_nfi(
      expr_list, init_list_type,
      SgAggregateInitializer::e_aggregate_initializer_source_braced);
  applySourceRange(agg_init, cxx_std_initializer_list_expr->getSourceRange());
  *node = agg_init;

  return VisitExpr(cxx_std_initializer_list_expr, node);
}

bool ClangToSageTranslator::VisitCXXThisExpr(clang::CXXThisExpr *cxx_this_expr,
                                             SgNode **node) {
  SgType *exact_this_type =
      buildTypeFromQualifiedType(cxx_this_expr->getType());
  SgPointerType *this_pointer_type = isSgPointerType(exact_this_type);
  if (this_pointer_type == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[this-result-type]: Clang this "
                 "expression has no exact cv-qualified pointer type"
              << std::endl;
    ROSE_ABORT();
  }
  SgSymbol *this_symbol = symbolForExactThisPointerType(exact_this_type);
  if (this_symbol == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[this-symbol]: Clang this expression "
                 "result type has no exact published class/nonreal symbol"
              << std::endl;
    ROSE_ABORT();
  }
  SgThisExp *this_exp =
      SageBuilder::buildThisExp_nfi(this_symbol, exact_this_type);
  *node = this_exp;

  return VisitExpr(cxx_this_expr, node);
}

bool ClangToSageTranslator::VisitCXXThrowExpr(
    clang::CXXThrowExpr *cxx_throw_expr, SgNode **node) {
  SgExpression *throw_operand = nullptr;
  SgThrowOp::e_throw_kind throw_kind;

  clang::Expr *sub_expr = cxx_throw_expr->getSubExpr();

  if (sub_expr != nullptr) {
    SgNode *tmp_expr = Traverse(sub_expr);
    throw_operand = isSgExpression(tmp_expr);
    if (throw_operand == nullptr) {
      std::cerr << "REX_FRONTEND_INVARIANT[throw-operand]: exact operand "
                   "translated to "
                << (tmp_expr == nullptr ? std::string("<null>")
                                        : tmp_expr->class_name())
                << " instead of SgExpression" << std::endl;
      ROSE_ABORT();
    }
    throw_kind = SgThrowOp::throw_expression;
  } else {
    // Rethrow (bare "throw;")
    throw_kind = SgThrowOp::rethrow;
  }

  SgType *result_type = buildTypeFromQualifiedType(cxx_throw_expr->getType());
  if (isSgTypeVoid(result_type) == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[throw-result-type]: Clang throw has "
                 "no exact semantic result type"
              << std::endl;
    ROSE_ABORT();
  }
  SgThrowOp *throw_op =
      SageBuilder::buildThrowOp_nfi(throw_operand, result_type, throw_kind);
  ROSE_ASSERT(throw_op != nullptr);

  *node = throw_op;

  return VisitExpr(cxx_throw_expr, node);
}

bool ClangToSageTranslator::VisitCXXTypeidExpr(
    clang::CXXTypeidExpr *cxx_typeid_expr, SgNode **node) {
  ROSE_ASSERT(cxx_typeid_expr != nullptr);

  SgType *result_type = buildTypeFromQualifiedType(cxx_typeid_expr->getType());
  if (result_type == nullptr ||
      SageInterface::containsUnknownType(result_type) ||
      !SageInterface::isConstType(result_type) ||
      isSgReferenceType(result_type) != nullptr)
    failExactExpressionTranslation("typeid-result-type", cxx_typeid_expr);
  SgType *expression_type = SageBuilder::buildReferenceType(result_type);

  SgType *operand_type = nullptr;
  SgExpression *operand = nullptr;
  if (cxx_typeid_expr->isTypeOperand()) {
    clang::TypeSourceInfo *type_info =
        cxx_typeid_expr->getTypeOperandSourceInfo();
    if (type_info == nullptr || type_info->getTypeLoc().isNull())
      failExactExpressionTranslation("typeid-written-type", cxx_typeid_expr);
    operand_type = buildTypeFromTypeLoc(type_info->getTypeLoc());
    if (operand_type == nullptr ||
        SageInterface::containsUnknownType(operand_type)) {
      failExactExpressionTranslation("typeid-type-operand", cxx_typeid_expr);
    }
  } else {
    clang::Expr *expr_operand = cxx_typeid_expr->getExprOperand();
    operand = isSgExpression(Traverse(expr_operand));
    if (operand == nullptr) {
      failExactExpressionTranslation("typeid-operand", expr_operand);
    }
  }
  *node =
      SageBuilder::buildTypeIdOp_nfi(operand, operand_type, expression_type,
                                     cxx_typeid_expr->isPotentiallyEvaluated());

  return VisitExpr(cxx_typeid_expr, node);
}

bool ClangToSageTranslator::VisitCXXUnresolvedConstructExpr(
    clang::CXXUnresolvedConstructExpr *cxx_unresolved_construct_expr,
    SgNode **node) {
  const bool semantic_constructor = hasExactSemanticExpressionConstructionRole(
      "VisitCXXUnresolvedConstructExpr");
  clang::TypeSourceInfo *type_info =
      cxx_unresolved_construct_expr->getTypeSourceInfo();
  if (type_info == nullptr || type_info->getType().isNull())
    failExactExpressionTranslation("unresolved-constructor-type-surface",
                                   cxx_unresolved_construct_expr);
  clang::TypeLoc type_loc = type_info->getTypeLoc();
  if (type_loc.isNull())
    failExactExpressionTranslation("unresolved-constructor-type-surface",
                                   cxx_unresolved_construct_expr);

  SgType *type = nullptr;
  const clang::Type *written_type = type_loc.getTypePtr();
  if (semantic_constructor) {
    type = buildSemanticTypeFromQualifiedType(
        cxx_unresolved_construct_expr->getType(),
        "unresolved-constructor-semantic-type");
  } else if (!qualifiedTypeHasQualifier(written_type)) {
    SgScopeStatement *scope = !p_type_loc_semantic_owner_scope_stack.empty()
                                  ? p_type_loc_semantic_owner_scope_stack.back()
                                  : SageBuilder::topScopeStack();
    if (scope == nullptr)
      failExactExpressionTranslation("unresolved-constructor-type-owner",
                                     cxx_unresolved_construct_expr);
    type = buildUnqualifiedWrittenRecordMemberType(written_type, scope);
  }
  if (!semantic_constructor && type == nullptr)
    type = buildTypeFromTypeLoc(type_loc);
  if (type == nullptr || SageInterface::containsUnknownType(type))
    failExactExpressionTranslation("unresolved-constructor-type-surface",
                                   cxx_unresolved_construct_expr);

  clang::SourceLocation left_delimiter =
      cxx_unresolved_construct_expr->getLParenLoc();
  clang::SourceLocation right_delimiter =
      cxx_unresolved_construct_expr->getRParenLoc();
  if (!left_delimiter.isValid() || !right_delimiter.isValid()) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[unresolved-constructor-delimiters]: "
            "source-written unresolved constructor has no exact argument "
            "delimiters\n");
    ROSE_ABORT();
  }
  SgExprListExp *args = SageBuilder::buildExprListExp_nfi();
  applySourceRange(args, clang::SourceRange(left_delimiter, right_delimiter));
  for (unsigned i = 0; i < cxx_unresolved_construct_expr->getNumArgs(); i++) {
    clang::Expr *clang_arg = cxx_unresolved_construct_expr->getArg(i);
    if (clang_arg == nullptr) {
      failExactExpressionTranslation("unresolved-constructor-argument-missing",
                                     cxx_unresolved_construct_expr);
    }
    SgExpression *arg = isSgExpression(Traverse(clang_arg));
    if (arg == nullptr) {
      failExactExpressionTranslation(
          "unresolved-constructor-argument-translation", clang_arg);
    }
    args->append_expression(arg);
  }

  const bool class_unknown = constructorInitializerAssociatedClassUnknown(type);

  SgConstructorInitializer *ctor_init =
      SageBuilder::buildConstructorInitializer_nfi(
          nullptr, // declaration will be nullptr for unresolved/dependent
                   // constructors
          args, type,
          true,  // need_name
          false, // need_qualifier
          !cxx_unresolved_construct_expr->isListInitialization(),
          // need_parenthesis_after_name
          class_unknown // associated_class_unknown for non class-like types
      );
  SgStringList qualifier_tokens;
  bool global_qualification = false;
  (void)exactTypeLocSourceQualification(type_loc, qualifier_tokens,
                                        global_qualification);
  transferWrittenNonrealQualifierToUseSite(type, qualifier_tokens,
                                           global_qualification,
                                           "VisitCXXUnresolvedConstructExpr");
  ctor_init->set_explicit_name_qualification_present(true);
  ctor_init->set_explicit_global_qualification(global_qualification);
  ctor_init->set_explicit_name_qualification_tokens(qualifier_tokens);
  const SgNonrealDecl::source_elaboration_kind_enum source_elaboration_kind =
      exactTypeLocSourceElaborationKind(type_loc);
  ctor_init->set_source_type_elaboration_kind(source_elaboration_kind);
  ctor_init->set_type_elaboration_required(
      source_elaboration_kind != SgNonrealDecl::e_source_elaboration_none);

  // Preserve list-initialization form for unresolved constructor expressions
  // so template definitions keep braces where required (e.g., aggregate
  // initialization through dependent base packs in C++17).
  ctor_init->set_is_braced_initialized(
      cxx_unresolved_construct_expr->isListInitialization());

  *node = ctor_init;

  return VisitExpr(cxx_unresolved_construct_expr, node);
}

bool ClangToSageTranslator::VisitDeclRefExpr(clang::DeclRefExpr *decl_ref_expr,
                                             SgNode **node) {
  clang::ValueDecl *decl_ref_decl = decl_ref_expr->getDecl();
  ROSE_ASSERT(decl_ref_decl != nullptr);
  if (auto *function = llvm::dyn_cast<clang::FunctionDecl>(decl_ref_decl))
    decl_ref_decl = openMPVariantBase(function);
  const bool semantic_expression =
      hasExactSemanticExpressionConstructionRole("VisitDeclRefExpr");

  if (auto *decomposition =
          llvm::dyn_cast<clang::DecompositionDecl>(decl_ref_decl)) {
    ROSE_ASSERT(semantic_expression);
    auto *owner = exactInitializedNameFromTranslatedVariableNode(
        lookupExactSgDeclarationForClangDecl(decomposition, false));
    ASSERT_not_null(owner);
    auto *type = buildSemanticTypeFromQualifiedType(
        decl_ref_expr->getType(), "VisitDeclRefExpr:decomposition");
    *node = new SgCxxDecompositionRefExp(owner, type);
    return VisitExpr(decl_ref_expr, node);
  }

  if (auto *parameter = llvm::dyn_cast<clang::ParmVarDecl>(decl_ref_decl);
      parameter != nullptr && semantic_expression &&
      p_function_exception_construction != nullptr &&
      p_function_exception_construction->semantic &&
      p_function_body_translation_owner_stack.size() ==
          p_function_exception_construction->body_depth) {
    const auto &exception = *p_function_exception_construction;
    const auto *parameter_owner =
        llvm::dyn_cast<clang::FunctionDecl>(parameter->getDeclContext());
    bool owns_parameter = parameter_owner != nullptr &&
                          parameter_owner->getType().getCanonicalType() ==
                              exception.type->getCanonicalTypeInternal();
    for (const auto &prototype : p_function_type_parameter_construction_stack)
      owns_parameter |= prototype.type->getCanonicalTypeInternal() ==
                            exception.type->getCanonicalTypeInternal() &&
                        prototype.parameters.count(parameter) != 0;
    if (owns_parameter) {
      const unsigned index = parameter->getFunctionScopeIndex();
      ROSE_ASSERT(index < exception.type->getNumParams());
      if (parameter_owner != nullptr)
        ROSE_ASSERT(parameter_owner->getParamDecl(index) == parameter);
      auto *reference = SageBuilder::buildFunctionParameterRefExp_nfi(
          index, 0,
          buildSemanticTypeFromQualifiedType(
              parameter->getType(), "function-exception-parameter-reference"));
      publishSemanticExpressionSourceProvenance(
          reference, decl_ref_expr->getSourceRange(), "VisitDeclRefExpr");
      *node = reference;
      return VisitExpr(decl_ref_expr, node);
    }
  }

  auto read_decl_context =
      [](const clang::Decl *decl) -> const clang::DeclContext * {
    if (decl == nullptr) {
      return nullptr;
    }
    return decl->getDeclContext();
  };
  auto read_lexical_decl_context =
      [](const clang::Decl *decl) -> const clang::DeclContext * {
    if (decl == nullptr) {
      return nullptr;
    }
    return decl->getLexicalDeclContext();
  };
  auto read_decl_context_parent =
      [](const clang::DeclContext *ctx) -> const clang::DeclContext * {
    if (ctx == nullptr) {
      return nullptr;
    }
    return ctx->getParent();
  };
  auto context_decl = [](const clang::DeclContext *ctx) -> const clang::Decl * {
    if (ctx == nullptr) {
      return nullptr;
    }
    return clang::Decl::castFromDeclContext(ctx);
  };
  auto strip_linkage_context =
      [&](const clang::DeclContext *ctx) -> const clang::DeclContext * {
    while (const clang::Decl *decl = context_decl(ctx)) {
      if (!llvm::isa<clang::LinkageSpecDecl>(decl)) {
        break;
      }
      ctx = read_decl_context_parent(ctx);
    }
    return ctx;
  };
  auto context_is_translation_unit = [](const clang::DeclContext *ctx) -> bool {
    return ctx != nullptr && ctx->isTranslationUnit();
  };
  auto context_is_namespace = [](const clang::DeclContext *ctx) -> bool {
    return ctx != nullptr && ctx->isNamespace();
  };
  auto context_is_record = [](const clang::DeclContext *ctx) -> bool {
    return ctx != nullptr && ctx->isRecord();
  };

  if (clang::NonTypeTemplateParmDecl *non_type_param =
          llvm::dyn_cast<clang::NonTypeTemplateParmDecl>(decl_ref_decl)) {
    if (p_canonical_template_argument_context.has_value())
      non_type_param =
          resolveCanonicalNonTypeTemplateParameterUse(non_type_param);
    SgTemplateParameter *sg_param = lookupActiveTemplateParameterSurface(
        non_type_param, "VisitDeclRefExpr:non-type-template-parameter");
    if (sg_param == nullptr) {
      SgDeclarationStatement *owning_template = resolveTemplateParameterOwner(
          non_type_param, /*allow_on_demand=*/true);
      sg_param = requirePublishedTemplateParameter(
          non_type_param, owning_template, "VisitDeclRefExpr");
    }

    SgTemplateParameterVal *param_val =
        SageBuilder::buildTemplateParameterVal_nfi(non_type_param->getIndex(),
                                                   sg_param);
    if (semantic_expression) {
      publishSemanticExpressionSourceProvenance(
          param_val, decl_ref_expr->getSourceRange(), "VisitDeclRefExpr:NTTP");
    } else {
      param_val->set_literal_spelling_form(
          SgValueExp::e_literal_source_spelled);
      applySourceRange(param_val, decl_ref_expr->getSourceRange());
    }
    const SgValueExp::literal_spelling_form_enum expected_spelling =
        semantic_expression ? SgValueExp::e_literal_canonical_generated
                            : SgValueExp::e_literal_source_spelled;
    if (param_val->get_literal_spelling_form() != expected_spelling) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[template-parameter-literal-"
              "provenance]: expression=%p semantic=%d spelling=%d does "
              "not identify its exact producer role\n",
              static_cast<void *>(param_val), semantic_expression ? 1 : 0,
              static_cast<int>(param_val->get_literal_spelling_form()));
      ROSE_ABORT();
    }
    *node = param_val;
    return true;
  }

  clang::NestedNameSpecifierLoc decl_ref_qualifier_loc =
      decl_ref_expr->getQualifierLoc();
  const clang::NestedNameSpecifierLoc *decl_ref_qualifier_loc_ptr =
      decl_ref_qualifier_loc.getNestedNameSpecifier() ? &decl_ref_qualifier_loc
                                                      : nullptr;
  const clang::NestedNameSpecifier decl_ref_qualifier =
      decl_ref_expr->getQualifier();

  auto qualifier_has_type_component =
      [](clang::NestedNameSpecifier qualifier) -> bool {
    for (clang::NestedNameSpecifier nns = qualifier; nns;
         nns = nestedNameSpecifierPrefix(nns)) {

      switch (nns.getKind()) {
      case clang::NestedNameSpecifier::Kind::Type:
      case clang::NestedNameSpecifier::Kind::MicrosoftSuper:
        return true;
      default:
        break;
      }
    }
    return false;
  };

  auto clang_context_is_namespace_or_tu =
      [&](const clang::DeclContext *ctx) -> bool {
    ctx = strip_linkage_context(ctx);
    return ctx != nullptr &&
           (context_is_namespace(ctx) || context_is_translation_unit(ctx));
  };
  auto clang_context_is_class_like =
      [&](const clang::DeclContext *ctx) -> bool {
    ctx = strip_linkage_context(ctx);
    if (ctx == nullptr) {
      return false;
    }
    if (context_is_record(ctx)) {
      return true;
    }
    if (const clang::Decl *ctx_decl = context_decl(ctx)) {
      return llvm::isa<clang::ClassTemplateDecl>(ctx_decl) ||
             llvm::isa<clang::ClassTemplateSpecializationDecl>(ctx_decl) ||
             llvm::isa<clang::ClassTemplatePartialSpecializationDecl>(ctx_decl);
    }
    return false;
  };
  auto is_adl_only_friend_function =
      [&](const clang::FunctionDecl *clang_func) -> bool {
    if (clang_func == nullptr || llvm::isa<clang::CXXMethodDecl>(clang_func)) {
      return false;
    }

    auto has_namespace_scope_primary_template =
        [&](const clang::FunctionDecl *func) -> bool {
      clang::FunctionTemplateDecl *primary_template =
          func->getPrimaryTemplate();
      clang::FunctionDecl *templated_decl =
          primary_template != nullptr ? primary_template->getTemplatedDecl()
                                      : nullptr;
      if (templated_decl == nullptr) {
        return false;
      }

      return clang_context_is_namespace_or_tu(
                 read_decl_context(templated_decl)) &&
             clang_context_is_namespace_or_tu(
                 read_lexical_decl_context(templated_decl));
    };

    if (has_namespace_scope_primary_template(clang_func)) {
      return false;
    }

    if (clang_func->getFriendObjectKind() != clang::Decl::FOK_None) {
      return true;
    }

    const clang::DeclContext *semantic_ctx = read_decl_context(clang_func);
    const clang::DeclContext *lexical_ctx =
        read_lexical_decl_context(clang_func);
    if (clang_context_is_namespace_or_tu(semantic_ctx) &&
        (clang_context_is_class_like(lexical_ctx) ||
         clang_context_is_class_like(semantic_ctx))) {
      return true;
    }

    const clang::FunctionDecl *first_decl =
        static_cast<const clang::FunctionDecl *>(clang_func->getFirstDecl());
    return first_decl != nullptr && first_decl != clang_func &&
           (clang_context_is_class_like(read_decl_context(first_decl)) ||
            clang_context_is_class_like(read_lexical_decl_context(first_decl)));
  };

  auto attach_explicit_qualifier = [&](SgExpression *expr) {
    if (expr == nullptr || decl_ref_expr == nullptr) {
      return;
    }
    if (semantic_expression) {
      // A semantic expression retains declaration identity through its exact
      // symbol edge.  Preserve a qualifier only when its outermost source
      // component owns the mandatory trailing `::`.  Clang can attach the
      // semantic `S::` qualifier of a substituted `&S::m` argument to the
      // instantiated expression whose source token is only `PTM`; that token
      // has no delimiter and therefore has no qualifier source surface.  An
      // instantiated reference originating in `std::forward_as_tuple`, by
      // contrast, retains the exact `std` token followed by `::`.
      if (!decl_ref_qualifier || decl_ref_qualifier_loc_ptr == nullptr) {
        return;
      }
      clang::SourceManager &source_manager =
          p_compiler_instance->getSourceManager();
      const clang::LangOptions &lang_options =
          p_compiler_instance->getLangOpts();
      const clang::SourceRange local_range =
          decl_ref_qualifier_loc_ptr->getLocalSourceRange();
      const clang::SourceLocation source_begin =
          sourceQualificationFileLoc(source_manager, local_range.getBegin());
      const clang::SourceLocation source_end =
          sourceQualificationFileLoc(source_manager, local_range.getEnd(),
                                     /*expansion_end=*/true);
      if (local_range.isInvalid() || source_begin.isInvalid() ||
          source_end.isInvalid() ||
          source_manager.getFileID(source_begin) !=
              source_manager.getFileID(source_end)) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[declaration-reference-qualification]: "
                "semantic declaration=%s expression=%s qualifier-range=%s--%s "
                "spelling-range=%s--%s has no exact single-file source "
                "component\n",
                decl_ref_decl != nullptr
                    ? decl_ref_decl->getQualifiedNameAsString().c_str()
                    : "<null>",
                decl_ref_expr->getStmtClassName(),
                local_range.getBegin().printToString(source_manager).c_str(),
                local_range.getEnd().printToString(source_manager).c_str(),
                source_begin.printToString(source_manager).c_str(),
                source_end.printToString(source_manager).c_str());
        ROSE_ABORT();
      }
      clang::Token end_token;
      if (clang::Lexer::getRawToken(source_end, end_token, source_manager,
                                    lang_options, /*IgnoreWhiteSpace=*/false)) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[declaration-reference-qualification]: "
                "semantic qualifier has no exact final token\n");
        ROSE_ABORT();
      }
      bool owns_delimiter = end_token.is(clang::tok::coloncolon);
      if (!owns_delimiter) {
        const std::optional<clang::Token> delimiter =
            clang::Lexer::findNextToken(source_end, source_manager,
                                        lang_options);
        owns_delimiter =
            delimiter.has_value() && delimiter->is(clang::tok::coloncolon);
      }
      if (!owns_delimiter) {
        return;
      }
    }
    auto set_explicit_unqualified_free_function = [](SgExpression *expr) {
      if (SgFunctionRefExp *function_ref = isSgFunctionRefExp(expr)) {
        function_ref->set_explicit_name_qualification_length(0);
        function_ref->set_explicit_global_qualification(false);
        function_ref->set_explicit_name_qualification_tokens(SgStringList());
      } else if (SgTemplateFunctionRefExp *template_ref =
                     isSgTemplateFunctionRefExp(expr)) {
        template_ref->set_explicit_name_qualification_length(0);
        template_ref->set_explicit_global_qualification(false);
        template_ref->set_explicit_name_qualification_tokens(SgStringList());
      }
    };
    if (!decl_ref_qualifier) {
      if (clang::FunctionDecl *clang_func =
              llvm::dyn_cast<clang::FunctionDecl>(decl_ref_decl)) {
        if (is_adl_only_friend_function(clang_func)) {
          expr->set_name_qualification_length(0);
          expr->set_global_qualification_required(false);
        }
        if (!llvm::isa<clang::CXXMethodDecl>(clang_func)) {
          set_explicit_unqualified_free_function(expr);
        }
      }
      return;
    }
    if (decl_ref_qualifier_loc_ptr == nullptr) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[declaration-reference-qualification]: "
              "semantic qualifier has no exact source location\n");
      ROSE_ABORT();
    }
    attachExplicitQualifierFromNestedName(expr, decl_ref_qualifier,
                                          decl_ref_qualifier_loc_ptr,
                                          p_compiler_instance);
  };

  if (clang::FunctionDecl *func_decl =
          llvm::dyn_cast<clang::FunctionDecl>(decl_ref_decl)) {
    clang::TemplateSpecializationKind kind =
        func_decl->getTemplateSpecializationKind();
    if (func_decl->doesThisDeclarationHaveABody() &&
        (kind == clang::TSK_ImplicitInstantiation ||
         kind == clang::TSK_ExplicitInstantiationDeclaration ||
         kind == clang::TSK_ExplicitInstantiationDefinition)) {
      if (p_pending_function_body_completions_set.insert(func_decl).second) {
        p_pending_function_body_completions.push_back(func_decl);
      }
    }
  }

  // Explicit template-id spelling is a property of this reference, not of the
  // shared declaration. Represent it on the reference itself so one use such
  // as `f<int>` cannot synthesize or mutate a declaration that is also used by
  // a deduced call to `f`.
  const clang::TemplateArgumentList *reference_semantic_arguments = nullptr;
  if (clang::FunctionDecl *function_decl =
          llvm::dyn_cast<clang::FunctionDecl>(decl_ref_decl)) {
    reference_semantic_arguments =
        function_decl->getTemplateSpecializationArgs();
  } else if (clang::VarTemplateSpecializationDecl *variable_decl =
                 llvm::dyn_cast<clang::VarTemplateSpecializationDecl>(
                     decl_ref_decl)) {

    reference_semantic_arguments = &variable_decl->getTemplateArgs();
  }

  // Clang's per-reference written-template metadata is authoritative.  Source
  // text immediately after this expression may begin a CUDA launch (`<<<`),
  // which is not a template argument list.
  const bool reference_has_explicit_template_id =
      !semantic_expression && decl_ref_expr->hasExplicitTemplateArgs();
  size_t reference_explicit_argument_count = 0;
  if (reference_has_explicit_template_id) {
    clang::TemplateArgumentListInfo argument_info;
    decl_ref_expr->copyTemplateArgumentsInto(argument_info);
    reference_explicit_argument_count =
        countExpandedTemplateArguments(argument_info);
    SgTemplateArgumentPtrList template_arguments;
    if (argument_info.size() != 0) {
      std::vector<clang::TemplateArgument::ArgKind> semantic_argument_kinds;
      if (reference_semantic_arguments != nullptr) {
        auto append_semantic_kinds =
            [&](auto &&self, const clang::TemplateArgument &argument) -> void {
          const clang::TemplateArgument &defined = argument;
          const clang::TemplateArgument::ArgKind kind = defined.getKind();
          if (kind == clang::TemplateArgument::Pack) {
            llvm::ArrayRef<clang::TemplateArgument> elements =
                defined.pack_elements();
            for (const clang::TemplateArgument &element : elements) {
              self(self, element);
            }
            return;
          }
          semantic_argument_kinds.push_back(kind);
        };
        for (const clang::TemplateArgument &argument :
             reference_semantic_arguments->asArray()) {
          append_semantic_kinds(append_semantic_kinds, argument);
        }
        if (reference_explicit_argument_count >
            semantic_argument_kinds.size()) {
          fprintf(
              stderr,
              "REX_FRONTEND_INVARIANT[explicit-template-reference-"
              "arguments]: reference=%s owns %zu expanded written arguments "
              "but its selected specialization owns only %zu semantic "
              "arguments\n",
              decl_ref_decl->getQualifiedNameAsString().c_str(),
              reference_explicit_argument_count,
              semantic_argument_kinds.size());
          ROSE_ABORT();
        }
      }
      struct ResolvedReferenceArgumentContext {
        ClangToSageTranslator &translator;
        const clang::TemplateArgumentListInfo *prior_owner;
        std::vector<clang::TemplateArgument::ArgKind> prior_kinds;
        size_t prior_cursor;
        std::optional<clang::TemplateArgument::ArgKind> prior_current;

        ResolvedReferenceArgumentContext(
            ClangToSageTranslator &translator,
            const clang::TemplateArgumentListInfo *owner,
            std::vector<clang::TemplateArgument::ArgKind> kinds)
            : translator(translator),
              prior_owner(
                  translator.p_resolved_reference_written_argument_owner),
              prior_kinds(std::move(
                  translator.p_resolved_reference_semantic_argument_kinds)),
              prior_cursor(
                  translator.p_resolved_reference_semantic_argument_cursor),
              prior_current(
                  translator
                      .p_resolved_reference_current_semantic_argument_kind) {
          translator.p_resolved_reference_written_argument_owner = owner;
          translator.p_resolved_reference_semantic_argument_kinds =
              std::move(kinds);
          translator.p_resolved_reference_semantic_argument_cursor = 0;
          translator.p_resolved_reference_current_semantic_argument_kind
              .reset();
        }

        ~ResolvedReferenceArgumentContext() {
          translator.p_resolved_reference_written_argument_owner = prior_owner;
          translator.p_resolved_reference_semantic_argument_kinds =
              std::move(prior_kinds);
          translator.p_resolved_reference_semantic_argument_cursor =
              prior_cursor;
          translator.p_resolved_reference_current_semantic_argument_kind =
              prior_current;
        }
      } resolved_argument_context(*this, &argument_info,
                                  std::move(semantic_argument_kinds));
      template_arguments = buildTemplateArguments(argument_info, true);
    }
    if (template_arguments.size() != reference_explicit_argument_count) {
      std::cerr
          << "REX_FRONTEND_INVARIANT[explicit-template-reference-arguments]: "
             "Clang explicit template-id for '"
          << decl_ref_decl->getQualifiedNameAsString() << "' owns "
          << reference_explicit_argument_count
          << " written argument(s), but exact source translation produced "
          << template_arguments.size() << std::endl;
      ROSE_ABORT();
    }
    for (SgTemplateArgument *argument : template_arguments) {
      if (argument == nullptr || !argument->get_explicitlySpecified()) {
        std::cerr << "REX_FRONTEND_INVARIANT[explicit-template-reference-"
                     "arguments]: translated written argument for '"
                  << decl_ref_decl->getQualifiedNameAsString()
                  << "' lost its explicit source identity" << std::endl;
        ROSE_ABORT();
      }
    }

    const std::string template_name = decl_ref_decl->getNameAsString();
    ROSE_ASSERT(!template_name.empty());

    if (reference_semantic_arguments == nullptr &&
        !decl_ref_expr->isInstantiationDependent()) {
      std::cerr
          << "REX_FRONTEND_INVARIANT[explicit-template-reference-semantic-"
             "arguments]: resolved template-id '"
          << decl_ref_decl->getQualifiedNameAsString()
          << "' has no exact semantic template-argument list" << std::endl;
      ROSE_ABORT();
    }

    SgTemplateArgumentPtrList semantic_template_arguments;
    if (reference_semantic_arguments != nullptr) {
      semantic_template_arguments = buildTemplateArguments(
          *reference_semantic_arguments, reference_explicit_argument_count,
          /*reconstruct_explicit_source_surface=*/false);
    } else {
      semantic_template_arguments = buildSemanticTemplateArguments(
          argument_info, true, "declaration-reference-template-id");
    }

    SgScopeStatement *scope = SageBuilder::topScopeStack();
    if (scope == nullptr) {
      std::cerr << "REX_FRONTEND_INVARIANT[explicit-template-reference-"
                   "scope]: template-id '"
                << decl_ref_decl->getQualifiedNameAsString()
                << "' has no active lexical scope" << std::endl;
      ROSE_ABORT();
    }
    ROSE_ASSERT(scope != nullptr);

    SgDeclarationStatement *template_target = nullptr;
    if (clang::FunctionDecl *function_decl =
            llvm::dyn_cast<clang::FunctionDecl>(decl_ref_decl)) {
      SgDeclarationStatement *translated = nullptr;
      auto hidden_callable =
          p_hidden_friend_callable_decl_map.find(function_decl);
      if (hidden_callable != p_hidden_friend_callable_decl_map.end()) {
        translated = hidden_callable->second;
      } else {
        translated = lookupSgDeclarationForClangDecl(function_decl,
                                                     /*allow_on_demand=*/true);
        hidden_callable = p_hidden_friend_callable_decl_map.find(function_decl);
        if (hidden_callable != p_hidden_friend_callable_decl_map.end()) {
          translated = hidden_callable->second;
        }
      }
      if (SgTemplateInstantiationDirectiveStatement *directive =
              isSgTemplateInstantiationDirectiveStatement(translated)) {
        translated = directive->get_declaration();
      }
      SgFunctionDeclaration *resolved_function =
          isSgFunctionDeclaration(translated);
      if (resolved_function == nullptr) {
        // Ordinary lookup deliberately withholds an in-progress declaration.
        // A recursive template-id inside that declaration's own body is the
        // one exact consumer allowed to use the already-published family
        // identity. Prove both the Clang and Sage body transactions before
        // exposing that edge.
        auto exact_mapping = p_decl_translation_map.find(function_decl);
        SgNode *published_node = exact_mapping != p_decl_translation_map.end()
                                     ? exact_mapping->second
                                     : nullptr;
        SgDeclarationStatement *published =
            isSgDeclarationStatement(published_node);
        if (SgTemplateInstantiationDirectiveStatement *directive =
                isSgTemplateInstantiationDirectiveStatement(published)) {
          published = directive->get_declaration();
        }
        SgFunctionDeclaration *published_function =
            isSgFunctionDeclaration(published);
        if (published_function != nullptr &&
            p_decl_translation_in_progress.count(function_decl) == 1) {
          if (p_function_body_translation_owner_stack.empty()) {
            std::cerr
                << "REX_FRONTEND_INVARIANT[explicit-template-id-callable-"
                   "transaction]: in-progress function '"
                << function_decl->getQualifiedNameAsString()
                << "' has a published Sage declaration but no exact active "
                   "function-body owner"
                << std::endl;
            ROSE_ABORT();
          }
          const FunctionBodyTranslationOwner &active =
              p_function_body_translation_owner_stack.back();
          SgFunctionDefinition *active_definition =
              isSgFunctionDefinition(active.sage_owner);
          SgFunctionDeclaration *active_declaration =
              active_definition != nullptr
                  ? active_definition->get_declaration()
                  : nullptr;
          const clang::FunctionDecl *function_canonical =
              function_decl->getCanonicalDecl();
          const clang::FunctionDecl *active_canonical =
              active.clang_declaration != nullptr
                  ? active.clang_declaration->getCanonicalDecl()
                  : nullptr;
          SgFunctionDeclaration *published_first = isSgFunctionDeclaration(
              published_function->get_firstNondefiningDeclaration());
          SgFunctionDeclaration *active_first =
              active_declaration != nullptr
                  ? isSgFunctionDeclaration(
                        active_declaration->get_firstNondefiningDeclaration())
                  : nullptr;
          const bool exact_clang_family =
              function_canonical != nullptr &&
              function_canonical == active_canonical;
          const bool exact_sage_family =
              active_definition != nullptr && active_declaration != nullptr &&
              active_definition->get_declaration() == active_declaration &&
              active_declaration->get_definition() == active_definition &&
              published_first != nullptr && active_first != nullptr &&
              published_first == active_first &&
              published_function->get_definingDeclaration() ==
                  active_declaration->get_definingDeclaration();
          if (!exact_clang_family || !exact_sage_family) {
            fprintf(
                stderr,
                "REX_FRONTEND_INVARIANT[explicit-template-id-callable-"
                "transaction]: Clang function=%p canonical=%p active=%p "
                "canonical=%p published Sage function=%p first=%p active "
                "declaration=%p first=%p definition=%p do not name one exact "
                "active function-body family\n",
                static_cast<void *>(function_decl),
                static_cast<const void *>(function_canonical),
                static_cast<const void *>(active.clang_declaration),
                static_cast<const void *>(active_canonical),
                static_cast<void *>(published_function),
                static_cast<void *>(published_first),
                static_cast<void *>(active_declaration),
                static_cast<void *>(active_first),
                static_cast<void *>(active_definition));
            ROSE_ABORT();
          }
          resolved_function = published_function;
        }
      }
      if (resolved_function == nullptr) {
        auto exact_mapping = p_decl_translation_map.find(function_decl);
        clang::FunctionDecl *pattern =
            llvm::dyn_cast_or_null<clang::FunctionDecl>(
                function_decl->getTemplateInstantiationPattern());
        clang::FunctionDecl *first =
            llvm::dyn_cast_or_null<clang::FunctionDecl>(
                function_decl->getFirstDecl());
        clang::FunctionDecl *canonical =
            llvm::dyn_cast_or_null<clang::FunctionDecl>(
                function_decl->getCanonicalDecl());
        fprintf(
            stderr,
            "REX_FRONTEND_INVARIANT[explicit-template-id-callable]: "
            "resolved Clang function=%p/%s translated=%p/%s exact-map=%p/%s "
            "in-progress=%d on-demand=%d first=%p canonical=%p pattern=%p "
            "has no exact Sage function declaration\n",
            static_cast<void *>(function_decl),
            function_decl->getQualifiedNameAsString().c_str(),
            static_cast<void *>(translated),
            translated != nullptr ? translated->class_name().c_str() : "<null>",
            static_cast<void *>(exact_mapping != p_decl_translation_map.end()
                                    ? exact_mapping->second
                                    : nullptr),
            exact_mapping != p_decl_translation_map.end() &&
                    exact_mapping->second != nullptr
                ? exact_mapping->second->class_name().c_str()
                : "<null>",
            p_decl_translation_in_progress.count(function_decl) != 0 ? 1 : 0,
            p_decl_translation_on_demand.count(function_decl) != 0 ? 1 : 0,
            static_cast<void *>(first), static_cast<void *>(canonical),
            static_cast<void *>(pattern));
        for (clang::FunctionDecl *redecl : function_decl->redecls()) {
          auto redecl_mapping = p_decl_translation_map.find(redecl);
          fprintf(
              stderr,
              "REX_FRONTEND_INVARIANT[explicit-template-id-callable-"
              "redecl]: function=%p redecl=%p/%s exact-map=%p/%s "
              "in-progress=%d on-demand=%d\n",
              static_cast<void *>(function_decl), static_cast<void *>(redecl),
              redecl->getQualifiedNameAsString().c_str(),
              static_cast<void *>(redecl_mapping != p_decl_translation_map.end()
                                      ? redecl_mapping->second
                                      : nullptr),
              redecl_mapping != p_decl_translation_map.end() &&
                      redecl_mapping->second != nullptr
                  ? redecl_mapping->second->class_name().c_str()
                  : "<null>",
              p_decl_translation_in_progress.count(redecl) != 0 ? 1 : 0,
              p_decl_translation_on_demand.count(redecl) != 0 ? 1 : 0);
        }
        ROSE_ABORT();
      }
      template_target = resolved_function;
    } else if (clang::VarTemplateSpecializationDecl *variable_decl =
                   llvm::dyn_cast<clang::VarTemplateSpecializationDecl>(
                       decl_ref_decl)) {
      SgDeclarationStatement *translated = lookupSgDeclarationForClangDecl(
          variable_decl, /*allow_on_demand=*/true);
      if (SgTemplateInstantiationDirectiveStatement *directive =
              isSgTemplateInstantiationDirectiveStatement(translated)) {
        translated = directive->get_declaration();
      }
      SgTemplateVariableDeclaration *resolved_variable =
          isSgTemplateVariableDeclaration(translated);
      if (resolved_variable == nullptr ||
          resolved_variable->get_variables().size() != 1 ||
          resolved_variable->get_variables().front() == nullptr ||
          resolved_variable->get_variables().front()->get_parent() !=
              resolved_variable ||
          resolved_variable->get_variables().front()->get_declptr() !=
              resolved_variable ||
          resolved_variable->get_variables().front()->get_scope() == nullptr ||
          resolved_variable->get_variables().front()->get_type() == nullptr) {
        std::cerr
            << "REX_FRONTEND_INVARIANT[explicit-variable-template-reference]: "
               "resolved Clang variable template '"
            << variable_decl->getQualifiedNameAsString()
            << "' has no exact typed Sage specialization declaration; "
               "translated="
            << (translated != nullptr ? translated->class_name()
                                      : std::string("<null>"))
            << std::endl;
        ROSE_ABORT();
      }
      template_target = resolved_variable;
    }
    ASSERT_not_null(template_target);
    SgNonrealRefExp *reference = buildNonrealRefExpFromNestedNameSpecifier(
        decl_ref_qualifier, scope, SgName(template_name),
        decl_ref_expr->hasTemplateKeyword(), &semantic_template_arguments,
        &template_arguments, template_target);
    ROSE_ASSERT(reference != nullptr);
    reference->set_resolved_function_declaration(
        isSgFunctionDeclaration(template_target));
    reference->set_resolved_variable_declaration(
        isSgTemplateVariableDeclaration(template_target));
    reference->set_explicit_template_argument_list(true);
    if (reference->get_resolved_function_declaration() != nullptr) {
      SageInterface::requireResolvedFunctionTemplateReference(
          reference, "Clang DeclRefExpr producer");
    }
    if (reference->get_resolved_variable_declaration() != nullptr) {
      reference->set_is_non_odr_use(decl_ref_expr->isNonOdrUse() !=
                                    clang::NOUR_None);
      SageInterface::requireResolvedVariableTemplateReference(
          reference, "Clang DeclRefExpr producer");
      ASSERT_not_null(reference->get_type());
    }
    attach_explicit_qualifier(reference);
    applySourceRange(reference, decl_ref_expr->getSourceRange());
    *node = reference;
    return VisitExpr(decl_ref_expr, node);
  }

  SgNode *translated_node = nullptr;
  clang::FunctionDecl *referenced_function =
      llvm::dyn_cast<clang::FunctionDecl>(decl_ref_decl);
  clang::FunctionTemplateDecl *deduced_source_template = nullptr;
  if (!reference_has_explicit_template_id && referenced_function != nullptr) {
    deduced_source_template = referenced_function->getPrimaryTemplate();
  }

  // A deduced call spells the primary template name, not the semantic
  // specialization name (which may contain an unspellable lambda type).  Keep
  // the implicit specialization queued for semantic/body materialization, but
  // bind the source reference to the exact primary template declaration.
  if (deduced_source_template != nullptr) {
    translated_node = lookupSgDeclarationForClangDecl(deduced_source_template,
                                                      /*allow_on_demand=*/true);
    if (translated_node == nullptr ||
        (isSgTemplateFunctionDeclaration(translated_node) == nullptr &&
         isSgTemplateMemberFunctionDeclaration(translated_node) == nullptr)) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[deduced-template-reference]: "
              "function=%s primary-template=%p translated to %p/%s\n",
              referenced_function->getQualifiedNameAsString().c_str(),
              static_cast<void *>(deduced_source_template),
              static_cast<void *>(translated_node),
              translated_node != nullptr ? translated_node->class_name().c_str()
                                         : "<null>");
      ROSE_ABORT();
    }
  }
  auto hidden_callable =
      referenced_function != nullptr
          ? p_hidden_friend_callable_decl_map.find(referenced_function)
          : p_hidden_friend_callable_decl_map.end();
  auto exact_translation = p_decl_translation_map.find(decl_ref_decl);
  if (translated_node != nullptr) {
    // The primary template surface selected above is authoritative.
  } else if (hidden_callable != p_hidden_friend_callable_decl_map.end()) {
    translated_node = hidden_callable->second;
  } else if (clang::ParmVarDecl *parameter =
                 llvm::dyn_cast<clang::ParmVarDecl>(decl_ref_decl)) {
    translated_node = lookupParameterFromExactOwner(parameter);
  } else if (exact_translation != p_decl_translation_map.end()) {
    translated_node = exact_translation->second;
  } else {
    translated_node = TraverseOnDemand(decl_ref_decl);
    hidden_callable =
        referenced_function != nullptr
            ? p_hidden_friend_callable_decl_map.find(referenced_function)
            : p_hidden_friend_callable_decl_map.end();
    exact_translation = p_decl_translation_map.find(decl_ref_decl);
    if (hidden_callable != p_hidden_friend_callable_decl_map.end()) {
      translated_node = hidden_callable->second;
    } else if (exact_translation != p_decl_translation_map.end()) {
      translated_node = exact_translation->second;
    }
  }

  if (SgTemplateInstantiationDirectiveStatement *directive =
          isSgTemplateInstantiationDirectiveStatement(translated_node)) {
    translated_node = directive->get_declaration();
  }

  auto symbol_from_translated_node = [](SgNode *translated) -> SgSymbol * {
    if (SgVariableDeclaration *variable_decl =
            isSgVariableDeclaration(translated)) {
      if (variable_decl->get_variables().size() != 1 ||
          variable_decl->get_variables().front() == nullptr) {
        std::cerr << "REX_FRONTEND_INVARIANT[declaration-reference]: "
                     "translated variable declaration does not own exactly "
                     "one initialized name"
                  << std::endl;
        ROSE_ABORT();
      }
      SgInitializedName *name = variable_decl->get_variables().front();
      if (SgSymbol *symbol = name->get_symbol_from_symbol_table()) {
        return symbol;
      }
      return name->get_scope() != nullptr
                 ? name->get_scope()->find_symbol_from_declaration(name)
                 : nullptr;
    }
    if (SgInitializedName *name = isSgInitializedName(translated)) {
      if (SgSymbol *symbol = name->get_symbol_from_symbol_table()) {
        return symbol;
      }
      return name->get_scope() != nullptr
                 ? name->get_scope()->find_symbol_from_declaration(name)
                 : nullptr;
    }
    if (SgDeclarationStatement *declaration =
            isSgDeclarationStatement(translated)) {
      if (SgSymbol *symbol = declaration->get_symbol_from_symbol_table()) {
        return symbol;
      }
      if (SgDeclarationStatement *first_nondefining =
              declaration->get_firstNondefiningDeclaration()) {
        return first_nondefining->get_symbol_from_symbol_table();
      }
    }
    return nullptr;
  };

  SgSymbol *sym = symbol_from_translated_node(translated_node);
  SgNode *canonical_translated_node = nullptr;
  if (sym == nullptr) {
    // Clang may bind a reference to a later redeclaration whose lexical Sage
    // node intentionally owns no symbol (notably a friend redeclaration),
    // while the canonical namespace declaration owns the entity's symbol.
    // Resolve through Clang's canonical identity rather than guessing by name.
    clang::NamedDecl *canonical_decl = llvm::dyn_cast_or_null<clang::NamedDecl>(
        decl_ref_decl->getCanonicalDecl());
    if (canonical_decl != nullptr && canonical_decl != decl_ref_decl) {
      auto canonical_translation = p_decl_translation_map.find(canonical_decl);
      if (canonical_translation != p_decl_translation_map.end()) {
        canonical_translated_node = canonical_translation->second;
      } else {
        canonical_translated_node = TraverseOnDemand(canonical_decl);
        canonical_translation = p_decl_translation_map.find(canonical_decl);
        if (canonical_translation != p_decl_translation_map.end()) {
          canonical_translated_node = canonical_translation->second;
        }
      }
      sym = symbol_from_translated_node(canonical_translated_node);
    }
  }
  if (sym == nullptr) {
    if (clang::FunctionDecl *function_decl =
            llvm::dyn_cast<clang::FunctionDecl>(decl_ref_decl)) {
      // A function entity can have a source declaration at namespace scope
      // and a distinct lexical friend redeclaration inside a class.  Sage
      // keeps both declaration surfaces, but exactly one surface owns the
      // semantic function symbol.  Resolve that owner through Clang's exact
      // redeclaration identity; never guess by name or signature.
      for (clang::FunctionDecl *redecl : function_decl->redecls()) {
        if (redecl == nullptr) {
          continue;
        }
        auto mapped = p_decl_translation_map.find(redecl);
        if (mapped == p_decl_translation_map.end()) {
          continue;
        }
        SgSymbol *redecl_symbol = symbol_from_translated_node(mapped->second);
        if (redecl_symbol == nullptr) {
          continue;
        }
        if (sym != nullptr && sym != redecl_symbol) {
          std::cerr << "REX_FRONTEND_INVARIANT[function-redeclaration-symbol]: "
                       "Clang redeclaration family for '"
                    << function_decl->getQualifiedNameAsString()
                    << "' owns multiple distinct Sage symbols" << std::endl;
          ROSE_ABORT();
        }
        sym = redecl_symbol;
      }
    }
  }
  if (sym == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[declaration-reference]: Clang "
              << decl_ref_decl->getDeclKindName() << " '"
              << decl_ref_decl->getQualifiedNameAsString()
              << "' has no exact translated symbol; translated="
              << (translated_node != nullptr ? translated_node->class_name()
                                             : std::string("<null>"))
              << " canonicalTranslated="
              << (canonical_translated_node != nullptr
                      ? canonical_translated_node->class_name()
                      : std::string("<null>"));
    if (SgVariableDeclaration *variable =
            isSgVariableDeclaration(translated_node)) {
      std::cerr << " declarationParent=" << variable->get_parent()
                << " variableCount=" << variable->get_variables().size();
      if (variable->get_variables().size() == 1 &&
          variable->get_variables().front() != nullptr) {
        SgInitializedName *name = variable->get_variables().front();
        std::cerr << " initializedName=" << name->get_name()
                  << " initializedScope=" << name->get_scope()
                  << " initializedParent=" << name->get_parent();
      }
    } else if (SgInitializedName *name = isSgInitializedName(translated_node)) {
      std::cerr << " initializedName=" << name->get_name()
                << " initializedScope=" << name->get_scope()
                << " initializedScopeType="
                << (name->get_scope() != nullptr
                        ? name->get_scope()->class_name()
                        : std::string("<null>"))
                << " initializedParent=" << name->get_parent()
                << " initializedParentType="
                << (name->get_parent() != nullptr
                        ? name->get_parent()->class_name()
                        : std::string("<null>"))
                << " declaration=" << name->get_declptr();
      if (const clang::ParmVarDecl *parameter =
              llvm::dyn_cast<clang::ParmVarDecl>(decl_ref_decl)) {
        if (const clang::FunctionDecl *owner =
                llvm::dyn_cast_or_null<clang::FunctionDecl>(
                    parameter->getDeclContext())) {
          std::cerr << " clangOwner=" << owner->getQualifiedNameAsString();
        }
      }
    }
    std::cerr << std::endl;
    ROSE_ABORT();
  }

  SgFunctionDeclaration *implicit_c_function_declaration = nullptr;
  if (clang::FunctionDecl *clang_func =
          llvm::dyn_cast<clang::FunctionDecl>(decl_ref_decl)) {
    if (is_implicit_c_function_decl_for_ref(clang_func, p_compiler_instance,
                                            p_sage_preprocessor_recorder)) {
      SgFunctionDeclaration *referenced_decl = nullptr;
      if (SgFunctionSymbol *function_symbol = isSgFunctionSymbol(sym)) {
        referenced_decl = function_symbol->get_declaration();
      } else if (SgMemberFunctionSymbol *member_symbol =
                     isSgMemberFunctionSymbol(sym)) {
        referenced_decl = member_symbol->get_declaration();
      } else if (SgTemplateFunctionSymbol *template_symbol =
                     isSgTemplateFunctionSymbol(sym)) {
        referenced_decl = template_symbol->get_declaration();
      } else if (SgTemplateMemberFunctionSymbol *template_member_symbol =
                     isSgTemplateMemberFunctionSymbol(sym)) {
        referenced_decl = template_member_symbol->get_declaration();
      }
      if (referenced_decl == nullptr) {
        std::cerr << "REX_FRONTEND_INVARIANT[implicit-c-function-reference]: "
                     "translated function has no function declaration symbol"
                  << std::endl;
        ROSE_ABORT();
      }
      validate_implicit_c_function_reference(referenced_decl);
      implicit_c_function_declaration = referenced_decl;
    }
  }

  if (auto *alias = isSgAliasSymbol(sym))
    sym = alias->get_base();

  const auto *referenced_method =
      llvm::dyn_cast<clang::CXXMethodDecl>(decl_ref_expr->getDecl());
  const bool virtual_dispatch = referenced_method != nullptr &&
                                referenced_method->isVirtual() &&
                                (p_compiler_instance->getLangOpts().AppleKext ||
                                 !decl_ref_expr->hasQualifier());

  if (SgVariableSymbol *variable_symbol = isSgVariableSymbol(sym)) {
    SgVarRefExp *reference =
        semantic_expression ? SageBuilder::buildVarRefExp_nfi(variable_symbol)
                            : SageBuilder::buildVarRefExp(variable_symbol);
    ROSE_ASSERT(reference != nullptr);
    reference->set_is_non_odr_use(decl_ref_expr->isNonOdrUse() !=
                                  clang::NOUR_None);
    attach_explicit_qualifier(reference);
    *node = reference;
  } else if (SgTemplateMemberFunctionSymbol *template_member_symbol =
                 isSgTemplateMemberFunctionSymbol(sym)) {
    SgTemplateMemberFunctionRefExp *reference =
        SageBuilder::buildTemplateMemberFunctionRefExp_nfi(
            template_member_symbol, virtual_dispatch,
            decl_ref_expr->hasQualifier());
    ROSE_ASSERT(reference != nullptr);
    if (deduced_source_template != nullptr) {
      SgMemberFunctionDeclaration *semantic_function =
          isSgMemberFunctionDeclaration(
              requireSelectedFunctionDeclaration(referenced_function));
      ROSE_ASSERT(semantic_function != nullptr);
      reference->set_semantic_member_function_declaration(semantic_function);
    }
    attach_explicit_qualifier(reference);
    *node = reference;
  } else if (SgMemberFunctionSymbol *member_symbol =
                 isSgMemberFunctionSymbol(sym)) {
    ROSE_ASSERT(referenced_method != nullptr);
    SgExpression *reference =
        referenced_method->isStatic()
            ? static_cast<SgExpression *>(buildFunctionReference(
                  member_symbol, referenced_method->getType(),
                  semantic_expression))
            : SageBuilder::buildMemberFunctionRefExp_nfi(
                  member_symbol, virtual_dispatch,
                  decl_ref_expr->hasQualifier());
    ROSE_ASSERT(reference != nullptr);
    attach_explicit_qualifier(reference);
    *node = reference;
  } else if (SgTemplateFunctionSymbol *template_function_symbol =
                 isSgTemplateFunctionSymbol(sym)) {
    SgTemplateFunctionRefExp *reference =
        SageBuilder::buildTemplateFunctionRefExp_nfi(template_function_symbol);
    ROSE_ASSERT(reference != nullptr);
    if (deduced_source_template != nullptr) {
      reference->set_semantic_function_declaration(
          requireSelectedFunctionDeclaration(referenced_function));
    }
    attach_explicit_qualifier(reference);
    *node = reference;
  } else if (SgFunctionSymbol *function_symbol = isSgFunctionSymbol(sym)) {
    SgFunctionRefExp *reference = buildFunctionReference(
        function_symbol, decl_ref_expr->getDecl()->getType(),
        semantic_expression);
    ROSE_ASSERT(reference != nullptr);
    reference->set_c_implicit_declaration(implicit_c_function_declaration);
    attach_explicit_qualifier(reference);
    *node = reference;
  } else if (SgEnumFieldSymbol *enum_symbol = isSgEnumFieldSymbol(sym)) {
    SgInitializedName *initialized_name = enum_symbol->get_declaration();
    SgEnumDeclaration *enum_decl =
        initialized_name != nullptr
            ? isSgEnumDeclaration(initialized_name->get_declptr())
            : nullptr;
    if (enum_decl == nullptr) {
      std::cerr << "REX_FRONTEND_INVARIANT[enum-reference]: translated enum "
                   "constant has no exact enum declaration"
                << std::endl;
      ROSE_ABORT();
    }

    const clang::EnumConstantDecl *enum_constant =
        llvm::dyn_cast<clang::EnumConstantDecl>(decl_ref_decl);
    if (enum_constant == nullptr) {
      std::cerr << "REX_FRONTEND_INVARIANT[enum-reference]: enum symbol does "
                   "not correspond to a Clang EnumConstantDecl"
                << std::endl;
      ROSE_ABORT();
    }

    ROSE_ASSERT(initialized_name->get_enum_constant_value().has_value());
    SgExpression *reference = SageBuilder::buildEnumVal_nfi(
        *initialized_name->get_enum_constant_value(), enum_decl,
        initialized_name->get_name());
    ROSE_ASSERT(reference != nullptr);
    attach_explicit_qualifier(reference);
    *node = reference;
  } else {
    std::cerr << "REX_FRONTEND_INVARIANT[declaration-reference]: exact "
                 "translated symbol has unsupported type "
              << sym->class_name() << std::endl;
    ROSE_ABORT();
  }

  if (implicit_c_function_declaration != nullptr) {
    SgFunctionRefExp *reference = isSgFunctionRefExp(*node);
    if (reference == nullptr || reference->get_c_implicit_declaration() !=
                                    implicit_c_function_declaration) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[implicit-c-function-reference]: "
              "implicit declaration=%p was not retained by one exact "
              "function-reference edge\n",
              static_cast<void *>(implicit_c_function_declaration));
      ROSE_ABORT();
    }
  }

  if (semantic_expression) {
    SgExpression *expression = isSgExpression(*node);
    if (expression == nullptr) {
      std::cerr << "REX_FRONTEND_INVARIANT[semantic-expression-provenance]: "
                   "VisitDeclRefExpr did not produce an exact expression"
                << std::endl;
      ROSE_ABORT();
    }
    publishSemanticExpressionSourceProvenance(
        expression, decl_ref_expr->getSourceRange(), "VisitDeclRefExpr");
  } else {
    applySourceRange(*node, decl_ref_expr->getSourceRange());
  }
  return VisitExpr(decl_ref_expr, node);
}

bool ClangToSageTranslator::VisitDependentCoawaitExpr(
    clang::DependentCoawaitExpr *dependent_coawait_expr, SgNode **node) {
  buildCoroutineAwaitExpression(dependent_coawait_expr->getOperand(),
                                dependent_coawait_expr->getType(),
                                dependent_coawait_expr->getSourceRange(),
                                SgAwaitExpression::e_coroutine_keyword_co_await,
                                "dependent co_await operand", node);

  return VisitExpr(dependent_coawait_expr, node);
}

bool ClangToSageTranslator::VisitDependentScopeDeclRefExpr(
    clang::DependentScopeDeclRefExpr *dependent_scope_decl_ref_expr,
    SgNode **node) {
  // DependentScopeDeclRefExpr represents a reference to a declaration that
  // depends on template parameters (e.g., variable references like 'x' or 'y'
  // in template-dependent contexts).

  std::string decl_name =
      dependent_scope_decl_ref_expr->getDeclName().getAsString();

  SgScopeStatement *current_scope = SageBuilder::topScopeStack();
  ROSE_ASSERT(current_scope != nullptr);

  SgTemplateArgumentPtrList template_args;
  const SgTemplateArgumentPtrList *template_args_ptr = nullptr;
  SgTemplateArgumentPtrList semantic_template_args;
  const SgTemplateArgumentPtrList *semantic_template_args_ptr = nullptr;
  if (dependent_scope_decl_ref_expr->hasExplicitTemplateArgs()) {
    clang::TemplateArgumentListInfo arg_info;
    dependent_scope_decl_ref_expr->copyTemplateArgumentsInto(arg_info);
    template_args = buildTemplateArguments(arg_info, true);
    template_args_ptr = &template_args;
    semantic_template_args = buildSemanticTemplateArguments(
        arg_info, true, "dependent-scope-declaration-reference-template-id");
    semantic_template_args_ptr = &semantic_template_args;
  }

  clang::NestedNameSpecifierLoc qualifier_loc =
      dependent_scope_decl_ref_expr->getQualifierLoc();
  const clang::NestedNameSpecifierLoc *qualifier_loc_ptr =
      qualifier_loc.getNestedNameSpecifier() ? &qualifier_loc : nullptr;

  clang::NestedNameSpecifier lookup_qualifier =
      dependent_scope_decl_ref_expr->getQualifier();

  SgNonrealRefExp *reference = buildNonrealRefExpFromNestedNameSpecifier(
      lookup_qualifier, current_scope, SgName(decl_name),
      dependent_scope_decl_ref_expr->hasTemplateKeyword(),
      semantic_template_args_ptr, template_args_ptr);
  ROSE_ASSERT(reference != nullptr);
  buildConversionFunctionId(reference,
                            dependent_scope_decl_ref_expr->getNameInfo());
  reference->set_explicit_template_argument_list(
      dependent_scope_decl_ref_expr->hasExplicitTemplateArgs());
  attachExplicitQualifierFromNestedName(reference, lookup_qualifier,
                                        qualifier_loc_ptr, p_compiler_instance);
  applySourceRange(reference, dependent_scope_decl_ref_expr->getSourceRange());
  *node = reference;

  return VisitExpr(dependent_scope_decl_ref_expr, node);
}

bool ClangToSageTranslator::VisitDesignatedInitExpr(
    clang::DesignatedInitExpr *designated_init_expr, SgNode **node) {
  clang::Expr *init_expr =
      llvm::dyn_cast_or_null<clang::Expr>(designated_init_expr->getInit());
  if (init_expr == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[designated-member-initializer]: "
                 "source designated initializer has no member initializer"
              << std::endl;
    ROSE_ABORT();
  }
  // Clang assigns the DesignatedInitExpr the selected field/element type after
  // resolving the designator chain.  The child initializer's type is only the
  // source operand/conversion and is not the destination contract.
  SgType *destination_type =
      buildTypeFromQualifiedType(designated_init_expr->getType());
  if (destination_type == nullptr ||
      SageInterface::containsUnknownType(destination_type)) {
    std::cerr << "REX_FRONTEND_INVARIANT[designated-member-type]: member "
                 "initializer has no exact designated destination type"
              << std::endl;
    ROSE_ABORT();
  }

  SgNode *translated_init_node = Traverse(init_expr);
  SgExpression *translated_init = isSgExpression(translated_init_node);
  if (translated_init == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[designated-member-initializer]: "
                 "Clang member initializer translated to "
              << (translated_init_node == nullptr
                      ? std::string("<null>")
                      : translated_init_node->class_name())
              << " instead of SgExpression" << std::endl;
    ROSE_ABORT();
  }
  translated_init = prepareExpressionForAttachment(translated_init);

  SgInitializer *member_init = isSgInitializer(translated_init);
  if (member_init == nullptr) {
    if (SgExprListExp *member_list = isSgExprListExp(translated_init)) {
      member_init = SageBuilder::buildAggregateInitializer_nfi(
          member_list, destination_type,
          SgAggregateInitializer::e_aggregate_initializer_source_braced);
    } else {
      member_init = SageBuilder::buildAssignInitializer_nfi(translated_init,
                                                            destination_type);
    }
  }
  if (member_init == nullptr || member_init->get_type() == nullptr ||
      SageInterface::containsUnknownType(member_init->get_type())) {
    std::cerr << "REX_FRONTEND_INVARIANT[designated-member-initializer]: "
                 "member initializer has no exact typed representation"
              << std::endl;
    ROSE_ABORT();
  }
  applySourceRange(member_init, init_expr->getSourceRange());

  SgExprListExp *designator_list = SageBuilder::buildExprListExp_nfi();
  const unsigned designator_count = designated_init_expr->size();
  if (designator_count == 0) {
    std::cerr << "REX_FRONTEND_INVARIANT[designator-list]: source designated "
                 "initializer has no designators"
              << std::endl;
    ROSE_ABORT();
  }

  for (unsigned index = 0; index < designator_count; ++index) {
    SgExpression *first = nullptr;
    SgExpression *second = nullptr;
    SgDesignator::designator_kind_enum kind =
        SgDesignator::e_designator_unclassified;
    clang::DesignatedInitExpr::Designator *D =
        designated_init_expr->getDesignator(index);
    if (D == nullptr) {
      std::cerr << "REX_FRONTEND_INVARIANT[designator-list]: null Clang "
                   "designator at index "
                << index << std::endl;
      ROSE_ABORT();
    }
    if (D->isFieldDesignator()) {
      clang::FieldDecl *field_decl = D->getFieldDecl();
      if (field_decl == nullptr) {
        std::cerr << "REX_FRONTEND_INVARIANT[designated-field]: field "
                     "designator has no exact Clang field declaration"
                  << std::endl;
        ROSE_ABORT();
      }
      SgSymbol *symbol = GetSymbolFromSymbolTable(field_decl);
      SgVariableSymbol *var_sym = isSgVariableSymbol(symbol);
      if (var_sym == nullptr) {
        std::cerr << "REX_FRONTEND_INVARIANT[designated-field]: exact field "
                     "translation has a non-variable symbol"
                  << std::endl;
        ROSE_ABORT();
      }
      first = SageBuilder::buildVarRefExp_nfi(var_sym);
      const clang::SourceLocation field_location = D->getFieldLoc();
      if (first == nullptr || !field_location.isValid()) {
        std::cerr << "REX_FRONTEND_INVARIANT[designated-field-provenance]: "
                     "field designator has no exact Sage reference or source "
                     "identifier location"
                  << std::endl;
        ROSE_ABORT();
      }
      // The SgDesignator owns the complete `.field` source surface, while its
      // SgVarRefExp payload owns the exact field-identifier token.  This
      // reference is built directly from the resolved symbol rather than by
      // Traverse(), so publish that source token at the producer instead of
      // leaving an unclassified located node for post-processing to repair.
      applySourceRange(first,
                       clang::SourceRange(field_location, field_location));
      kind = SgDesignator::e_designator_field;
    } else if (D->isArrayDesignator()) {
      SgNode *tmp_expr = Traverse(designated_init_expr->getArrayIndex(*D));
      first = isSgExpression(tmp_expr);
      if (first == nullptr) {
        std::cerr << "REX_FRONTEND_INVARIANT[designated-array]: array index "
                     "translated to a non-expression"
                  << std::endl;
        ROSE_ABORT();
      }
      first = prepareExpressionForAttachment(first);
      kind = SgDesignator::e_designator_array;
    } else if (D->isArrayRangeDesignator()) {
      first = isSgExpression(
          Traverse(designated_init_expr->getArrayRangeStart(*D)));
      second =
          isSgExpression(Traverse(designated_init_expr->getArrayRangeEnd(*D)));
      if (first == nullptr || second == nullptr) {
        std::cerr << "REX_FRONTEND_INVARIANT[designated-array-range]: range "
                     "bound translated to a non-expression"
                  << std::endl;
        ROSE_ABORT();
      }
      first = prepareExpressionForAttachment(first);
      second = prepareExpressionForAttachment(second);
      kind = SgDesignator::e_designator_array_range;
    } else {
      std::cerr << "REX_FRONTEND_INVARIANT[designator-kind]: unsupported "
                   "Clang designator at index "
                << index << std::endl;
      ROSE_ABORT();
    }
    if (first->get_parent() != nullptr ||
        (second != nullptr && second->get_parent() != nullptr)) {
      std::cerr << "REX_FRONTEND_INVARIANT[designator-owner]: designator "
                   "payload is already owned at index "
                << index << std::endl;
      ROSE_ABORT();
    }
    SgDesignator *designator = new SgDesignator(kind, first, second);
    if (designator == nullptr) {
      std::cerr << "REX_FRONTEND_INVARIANT[designator-construction]: failed "
                   "to build designator at index "
                << index << std::endl;
      ROSE_ABORT();
    }
    first->set_parent(designator);
    if (second != nullptr) {
      second->set_parent(designator);
    }
    applySourceRange(designator, D->getSourceRange());
    designator->validate_designator();
    designator->set_parent(designator_list);
    designator_list->append_expression(designator);
  }

  applySourceRange(designator_list,
                   designated_init_expr->getDesignatorsSourceRange());
  SgDesignatedInitializer *designated_init =
      new SgDesignatedInitializer(designator_list, member_init);
  if (designated_init->get_designatorList() != designator_list ||
      designated_init->get_memberInit() != member_init ||
      designator_list->get_parent() != designated_init ||
      member_init->get_parent() != designated_init) {
    std::cerr << "REX_FRONTEND_INVARIANT[designated-initializer-owner]: "
                 "designated initializer lacks exact child ownership"
              << std::endl;
    ROSE_ABORT();
  }
  applySourceRange(designated_init, designated_init_expr->getSourceRange());

  *node = designated_init;

  return true;
}

bool ClangToSageTranslator::VisitDesignatedInitUpdateExpr(
    clang::DesignatedInitUpdateExpr *designated_init_update, SgNode **node) {
  (void)designated_init_update;
  (void)node;
  std::cerr << "REX_FRONTEND_UNSUPPORTED[designated-init-update]: no exact "
               "Sage representation is implemented"
            << std::endl;
  ROSE_ABORT();
}

bool ClangToSageTranslator::VisitExpressionTraitExpr(
    clang::ExpressionTraitExpr *expression_trait_expr, SgNode **node) {
  (void)expression_trait_expr;
  (void)node;
  std::cerr << "REX_FRONTEND_UNSUPPORTED[expression-trait]: no exact Sage "
               "representation is implemented"
            << std::endl;
  ROSE_ABORT();
}

bool ClangToSageTranslator::VisitExtVectorElementExpr(
    clang::ExtVectorElementExpr *expression, SgNode **node) {
  SgExpression *base = isSgExpression(Traverse(expression->getBase()));
  ASSERT_not_null(base);
  SgType *type = buildTypeFromQualifiedType(expression->getType());
  ASSERT_not_null(type);

  llvm::SmallVector<uint32_t, 16> indices;
  expression->getEncodedElementAccess(indices);
  ASSERT_require(!indices.empty());
  ASSERT_require(indices.size() == expression->getNumElements());

  auto selector = SgVectorElementExp::e_components;
  const llvm::StringRef accessor = expression->getAccessor().getName();
  if (accessor == "hi")
    selector = SgVectorElementExp::e_hi;
  else if (accessor == "lo")
    selector = SgVectorElementExp::e_lo;
  else if (accessor == "even")
    selector = SgVectorElementExp::e_even;
  else if (accessor == "odd")
    selector = SgVectorElementExp::e_odd;
  if (selector == SgVectorElementExp::e_components) {
    for (unsigned index : indices)
      ASSERT_require(index < 16);
  }

  auto *result = new SgVectorElementExp(
      base, type, SgUnaryOp::prefix,
      std::vector<unsigned int>(indices.begin(), indices.end()), selector,
      expression->isArrow());
  setSynthesizedFileInfo(result);
  *node = result;
  return VisitExpr(expression, node);
}

bool ClangToSageTranslator::VisitFloatingLiteral(
    clang::FloatingLiteral *floating_literal, SgNode **node) {
  std::string spelling;
  bool udl_compiler_generated = false;
  auto exact_udl = p_exact_literal_operand_spellings.find(floating_literal);
  if (exact_udl != p_exact_literal_operand_spellings.end()) {
    if (exact_udl->second.consumed) {
      std::cerr << "REX_FRONTEND_INVARIANT[user-defined-literal-session]: "
                   "floating operand was consumed more than once"
                << std::endl;
      ROSE_ABORT();
    }
    spelling = exact_udl->second.spelling;
    udl_compiler_generated = exact_udl->second.compiler_generated;
    exact_udl->second.consumed = true;
  } else {
    spelling = getLiteralTokenSpelling(floating_literal,
                                       p_compiler_instance->getSourceManager(),
                                       p_compiler_instance->getLangOpts());
  }
  if (spelling.empty() && !udl_compiler_generated) {
    std::cerr << "REX_FRONTEND_INVARIANT[floating-literal-spelling]: source "
                 "floating literal has no exact token spelling"
              << std::endl;
    ROSE_ABORT();
  }

  const clang::BuiltinType *literal_type =
      floating_literal->getType()->getAs<clang::BuiltinType>();
  if (literal_type == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[floating-literal-type]: Clang "
                 "floating literal has non-builtin type "
              << floating_literal->getType().getAsString() << std::endl;
    ROSE_ABORT();
  }

  switch (literal_type->getKind()) {
  case clang::BuiltinType::Half:
  case clang::BuiltinType::Float16:
    *node = SageBuilder::buildFloat16Val_nfi(
        floating_literal->getValue().convertToFloat(), spelling);
    break;
  case clang::BuiltinType::BFloat16:
    *node = SageBuilder::buildBFloat16Val_nfi(
        floating_literal->getValue().convertToFloat(), spelling);
    break;
  case clang::BuiltinType::Float:
    *node = SageBuilder::buildFloatVal_nfi(
        floating_literal->getValue().convertToFloat(), spelling);
    break;
  case clang::BuiltinType::Double:
    *node = SageBuilder::buildDoubleVal_nfi(
        floating_literal->getValue().convertToDouble(), spelling);
    break;
  case clang::BuiltinType::LongDouble:
    *node = SageBuilder::buildLongDoubleVal_nfi(
        floatingValueFromClang(floating_literal->getValue()), spelling);
    break;
  case clang::BuiltinType::Float128:
    *node = SageBuilder::buildFloat128Val_nfi(
        floatingValueFromClang(floating_literal->getValue()), spelling);
    break;
  case clang::BuiltinType::Ibm128:
    std::cerr << "REX_FRONTEND_INVARIANT[floating-literal-format]: "
                 "__ibm128 has no exact SAGE value representation"
              << std::endl;
    ROSE_ABORT();
  default:
    std::cerr << "REX_FRONTEND_INVARIANT[floating-literal-kind]: unsupported "
                 "Clang builtin kind "
              << literal_type
                     ->getName(p_compiler_instance->getASTContext()
                                   .getPrintingPolicy())
                     .str()
              << std::endl;
    ROSE_ABORT();
  }

  if (udl_compiler_generated) {
    isSgValueExp(*node)->set_literal_spelling_form(
        SgValueExp::e_literal_canonical_generated);
    setSynthesizedFileInfo(*node);
  } else {
    isSgValueExp(*node)->set_literal_spelling_form(
        SgValueExp::e_literal_source_spelled);
    applySourceRange(*node, floating_literal->getSourceRange());
  }
  return VisitExpr(floating_literal, node);
}

bool ClangToSageTranslator::VisitFullExpr(clang::FullExpr *full_expr,
                                          SgNode **node) {
  clang::Expr *sub_expr = full_expr->getSubExpr();
  SgNode *tmp_expr = Traverse(sub_expr);
  SgExpression *expr = isSgExpression(tmp_expr);
  if (sub_expr == nullptr || expr == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[full-expression]: exact operand "
                 "translated to "
              << (tmp_expr == nullptr ? std::string("<null>")
                                      : tmp_expr->class_name())
              << " instead of SgExpression" << std::endl;
    ROSE_ABORT();
  }

  expr->add_semantic_wrapper(SgExpression::e_full_expression_wrapper);
  *node = expr;
  return VisitExpr(full_expr, node);
}

bool ClangToSageTranslator::VisitConstantExpr(
    clang::ConstantExpr *constant_expr, SgNode **node) {
  const bool result = VisitFullExpr(constant_expr, node);
  SgExpression *expression = node == nullptr ? nullptr : isSgExpression(*node);
  if (expression == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[constant-expression-wrapper]: "
                 "full-expression translation produced no expression"
              << std::endl;
    ROSE_ABORT();
  }
  expression->add_semantic_wrapper(SgExpression::e_constant_evaluated_wrapper);
  return result;
}

bool ClangToSageTranslator::VisitExprWithCleanups(
    clang::ExprWithCleanups *expr_with_cleanups, SgNode **node) {
  const bool result = VisitFullExpr(expr_with_cleanups, node);
  SgExpression *expression = node == nullptr ? nullptr : isSgExpression(*node);
  if (expression == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[cleanup-expression-wrapper]: "
                 "full-expression translation produced no expression"
              << std::endl;
    ROSE_ABORT();
  }
  expression->add_semantic_wrapper(SgExpression::e_cleanup_wrapper);
  return result;
}

bool ClangToSageTranslator::VisitGenericSelectionExpr(
    clang::GenericSelectionExpr *generic_Selection_expr, SgNode **node) {
  SgNode *result = Traverse(generic_Selection_expr->getResultExpr());
  SgExpression *result_expr = isSgExpression(result);
  ROSE_ASSERT(result_expr != nullptr);

  *node = result_expr;

  return VisitExpr(generic_Selection_expr, node);
}

bool ClangToSageTranslator::VisitGNUNullExpr(clang::GNUNullExpr *gnu_null_expr,
                                             SgNode **node) {
  clang::SourceManager &source_manager =
      p_compiler_instance->getSourceManager();
  clang::SourceLocation spelling_location =
      source_manager.getSpellingLoc(gnu_null_expr->getTokenLocation());
  llvm::SmallString<16> buffer;
  bool invalid = false;
  llvm::StringRef token;
  if (spelling_location.isValid()) {
    token =
        clang::Lexer::getSpelling(spelling_location, buffer, source_manager,
                                  p_compiler_instance->getLangOpts(), &invalid);
  } else {
    invalid = true;
  }
  if (invalid || token != "__null") {
    std::cerr << "REX_FRONTEND_INVARIANT[gnu-null-spelling]: source GNU null "
                 "expression has no exact __null token spelling"
              << std::endl;
    ROSE_ABORT();
  }

  const clang::QualType clang_type = gnu_null_expr->getType();
  const clang::BuiltinType *builtin_type =
      clang_type->getAs<clang::BuiltinType>();
  if (builtin_type == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[gnu-null-type]: GNU null expression "
                 "has non-builtin semantic type "
              << clang_type.getAsString() << std::endl;
    ROSE_ABORT();
  }

  switch (builtin_type->getKind()) {
  case clang::BuiltinType::Int:
    *node = SageBuilder::buildIntVal_nfi(0, "__null");
    break;
  case clang::BuiltinType::Long:
    *node = SageBuilder::buildLongIntVal_nfi(0, "__null");
    break;
  case clang::BuiltinType::LongLong:
    *node = SageBuilder::buildLongLongIntVal_nfi(0, "__null");
    break;
  default:
    std::cerr << "REX_FRONTEND_INVARIANT[gnu-null-type]: unsupported GNU null "
                 "semantic type "
              << builtin_type
                     ->getName(p_compiler_instance->getASTContext()
                                   .getPrintingPolicy())
                     .str()
              << std::endl;
    ROSE_ABORT();
  }

  applySourceRange(*node, gnu_null_expr->getSourceRange());
  return VisitExpr(gnu_null_expr, node);
}

bool ClangToSageTranslator::VisitImaginaryLiteral(
    clang::ImaginaryLiteral *imaginary_literal, SgNode **node) {
  clang::Expr *component = imaginary_literal->getSubExpr();
  ROSE_ASSERT(
      (llvm::isa<clang::IntegerLiteral, clang::FloatingLiteral>(component)));
  auto &source_manager = p_compiler_instance->getSourceManager();
  const auto &language = p_compiler_instance->getLangOpts();
  llvm::SmallString<64> buffer;
  bool invalid = false;
  std::string spelling =
      clang::Lexer::getSpelling(
          source_manager.getSpellingLoc(imaginary_literal->getBeginLoc()),
          buffer, source_manager, language, &invalid)
          .str();
  if (invalid || spelling.empty()) {
    fprintf(stderr, "REX_FRONTEND_INVARIANT[imaginary-literal-token]: missing "
                    "exact token\n");
    ROSE_ABORT();
  }
  clang::NumericLiteralParser parsed(
      spelling, imaginary_literal->getBeginLoc(), source_manager, language,
      p_compiler_instance->getTarget(), p_compiler_instance->getDiagnostics());
  // In C++, an imaginary suffix is also a candidate literal-operator suffix.
  // Clang's ImaginaryLiteral identity records the completed lookup decision;
  // hasUDSuffix alone cannot distinguish it from a UserDefinedLiteral.
  if (parsed.hadError || !parsed.isImaginary) {
    fprintf(stderr, "REX_FRONTEND_INVARIANT[imaginary-literal-token]: token is "
                    "not an imaginary numeric literal\n");
    ROSE_ABORT();
  }
  // Clang's scalar child carries the whole imaginary token range. Remove
  // only its validated imaginary suffix before constructing the scalar node;
  // the SgComplexVal owner represents that semantic operation exactly once.
  const size_t suffix_begin = parsed.getLiteralDigits().end() - spelling.data();
  const size_t imaginary_suffix = spelling.find_first_of("iIjJ", suffix_begin);
  ROSE_ASSERT(imaginary_suffix != std::string::npos);
  ROSE_ASSERT(spelling.find_first_of("iIjJ", imaginary_suffix + 1) ==
              std::string::npos);
  spelling.erase(imaginary_suffix, 1);
  ROSE_ASSERT(p_exact_literal_operand_spellings
                  .emplace(component,
                           ExactLiteralOperandSpelling{spelling, false, false})
                  .second);

  SgNode *tmp_imag_val = Traverse(component);
  const auto operand = p_exact_literal_operand_spellings.find(component);
  ROSE_ASSERT(operand != p_exact_literal_operand_spellings.end() &&
              operand->second.consumed);
  p_exact_literal_operand_spellings.erase(operand);
  SgExpression *imag_val = isSgExpression(tmp_imag_val);
  ROSE_ASSERT(imag_val != nullptr);

  SgComplexVal *comp_val =
      new SgComplexVal(nullptr, imag_val, imag_val->get_type());

  *node = comp_val;

  return VisitExpr(imaginary_literal, node);
}

bool ClangToSageTranslator::VisitImplicitValueInitExpr(
    clang::ImplicitValueInitExpr *implicit_value_init_expr, SgNode **node) {
  SgType *type =
      buildTypeFromQualifiedType(implicit_value_init_expr->getType());
  ROSE_ASSERT(!SageInterface::containsUnknownType(type));
  ROSE_ASSERT(isSgReferenceType(type) == nullptr);

  SgType *stripped =
      type->stripType(SgType::STRIP_MODIFIER_TYPE | SgType::STRIP_TYPEDEF_TYPE);
  SgExpression *expr = nullptr;

  if (isSgTypeNullptr(stripped) != nullptr) {
    expr = SageBuilder::buildNullptrValExp_nfi();
  } else if (SageInterface::isScalarType(stripped) ||
             SageInterface::isPointerType(stripped) ||
             isSgEnumType(stripped) != nullptr) {
    expr = SageBuilder::buildTransformationCastExp_nfi(
        SageBuilder::buildIntVal_nfi(0), type, SgCastExp::e_C_style_cast);
  } else {
    bool class_unknown = constructorInitializerAssociatedClassUnknown(type);
    SgExprListExp *args = SageBuilder::buildExprListExp_nfi();
    expr = SageBuilder::buildConstructorInitializer_nfi(
        nullptr, args, type,
        false,        // need_name
        false,        // need_qualifier
        false,        // need_parenthesis_after_name
        class_unknown // associated_class_unknown
    );
  }
  ROSE_ASSERT(expr != nullptr);

  // ImplicitValueInitExpr has no lexical spelling of its own.  Every Sage
  // expression that materializes the semantic zero/default initialization is
  // therefore a generated output node, including builder-created operands and
  // argument lists that are never visited through an independent Clang Expr.
  for (SgNode *semantic_node : RoseAst(expr)) {
    if (isSgLocatedNode(semantic_node) == nullptr) {
      continue;
    }
    if (SgValueExp *value = isSgValueExp(semantic_node)) {
      value->set_literal_spelling_form(
          SgValueExp::e_literal_canonical_generated);
    }
    setSynthesizedFileInfo(semantic_node);
  }

  *node = expr;

  return VisitExpr(implicit_value_init_expr, node);
}

bool ClangToSageTranslator::VisitInitListExpr(
    clang::InitListExpr *init_list_expr, SgNode **node) {
  clang::InitListExpr *semantic = init_list_expr->isSemanticForm()
                                      ? init_list_expr
                                      : init_list_expr->getSemanticForm();
  clang::InitListExpr *source = init_list_expr->getSyntacticForm();
  if (source == nullptr)
    source = init_list_expr;

  SgExprListExp *expr_list_expr = SageBuilder::buildExprListExp_nfi();
  applySourceRange(expr_list_expr, source->getSourceRange());

  for (unsigned index = 0; index < source->getNumInits(); ++index) {
    clang::Expr *clang_init = source->getInit(index);
    if (clang_init == nullptr) {
      std::cerr << "REX_FRONTEND_INVARIANT[initializer-list-element]: Clang "
                   "initializer list contains a null element"
                << std::endl;
      ROSE_ABORT();
    }
    SgNode *tmp_expr = Traverse(clang_init);
    SgExpression *expr = isSgExpression(tmp_expr);
    ROSE_ASSERT(expr != nullptr);

    expr_list_expr->append_expression(expr);
  }

  // A dependent braced-init-list can be syntax only. A semantic initializer
  // must publish its native destination type before any value/lifetime use.
  if (semantic == nullptr || semantic->getType()->isVoidType()) {
    ROSE_ASSERT(init_list_expr->getType()->isVoidType());
    expr_list_expr->set_source_form(SgExprListExp::e_expression_list_braced);
    *node = expr_list_expr;
  } else {
    SgType *type = buildTypeFromQualifiedType(semantic->getType());
    ROSE_ASSERT(type != nullptr && !SageInterface::containsUnknownType(type));
    const auto parents =
        p_compiler_instance->getASTContext().getParents(*init_list_expr);
    if (parents.size() == 1 && parents[0].get<clang::ReturnStmt>() != nullptr) {
      *node = SageBuilder::buildBracedInitializer_nfi(expr_list_expr, type);
    } else if (parents.size() == 1 &&
               parents[0].get<clang::CXXNewExpr>() != nullptr) {
      const auto *allocation = parents[0].get<clang::CXXNewExpr>();
      ROSE_ASSERT(allocation->getInitializer() == init_list_expr);
      SgType *allocated_type =
          buildTypeFromQualifiedType(allocation->getAllocatedType());
      auto *initializer = SageBuilder::buildConstructorInitializer_nfi(
          nullptr, expr_list_expr, allocated_type, false, false, false,
          constructorInitializerAssociatedClassUnknown(allocated_type));
      initializer->set_is_braced_initialized(true);
      *node = initializer;
    } else {
      const auto source_form =
          parents.size() == 1 &&
                  parents[0].get<clang::CompoundLiteralExpr>() != nullptr
              ? SgAggregateInitializer::
                    e_aggregate_initializer_source_compound_literal
              : SgAggregateInitializer::e_aggregate_initializer_source_braced;
      *node = SageBuilder::buildAggregateInitializer_nfi(expr_list_expr, type,
                                                         source_form);
    }
    applySourceRange(*node, source->getSourceRange());
  }

  return VisitExpr(init_list_expr, node);
}

bool ClangToSageTranslator::VisitIntegerLiteral(
    clang::IntegerLiteral *integer_literal, SgNode **node) {
  SgValueExp *value_exp = nullptr;
  const clang::QualType literal_type = integer_literal->getType();
  const clang::BuiltinType *builtin_type =
      literal_type->getAs<clang::BuiltinType>();
  const llvm::APInt value = integer_literal->getValue();
  std::string spelling;
  const bool semantic_expression_construction =
      p_semantic_function_body_traversal_depth != 0 ||
      p_semantic_template_argument_expression_depth != 0 ||
      p_semantic_expression_subtree_depth != 0 ||
      p_user_defined_literal_semantic_expression_depth != 0 ||
      (!p_imported_external_semantic_traversal_stack.empty() &&
       p_imported_external_semantic_traversal_stack.back());
  bool canonical_generated = semantic_expression_construction ||
                             p_canonical_generated_literal_depth != 0;
  auto exact_udl = p_exact_literal_operand_spellings.find(integer_literal);
  if (exact_udl != p_exact_literal_operand_spellings.end()) {
    if (exact_udl->second.consumed) {
      std::cerr << "REX_FRONTEND_INVARIANT[user-defined-literal-session]: "
                   "integer operand was consumed more than once"
                << std::endl;
      ROSE_ABORT();
    }
    spelling = exact_udl->second.spelling;
    canonical_generated = semantic_expression_construction ||
                          exact_udl->second.compiler_generated;
    exact_udl->second.consumed = true;
  } else if (!canonical_generated) {
    spelling = getLiteralTokenSpelling(integer_literal,
                                       p_compiler_instance->getSourceManager(),
                                       p_compiler_instance->getLangOpts());
    if (spelling.empty() &&
        integerLiteralHasExactSubstitutionAnchor(
            integer_literal, p_compiler_instance->getSourceManager(),
            p_compiler_instance->getLangOpts())) {
      // Clang materializes a concrete IntegerLiteral for a substituted
      // non-type template parameter while retaining the identifier token as
      // its source anchor. That node has no integer-token source surface: its
      // semantic value is the exact producer payload.
      canonical_generated = true;
    }
  }
  if (spelling.empty() && !canonical_generated) {
    clang::SourceManager &source_manager =
        p_compiler_instance->getSourceManager();
    std::cerr << "REX_FRONTEND_INVARIANT[integer-literal-spelling]: source "
                 "integer literal has no exact token spelling at "
              << integer_literal->getLocation().printToString(source_manager)
              << " raw-location="
              << integer_literal->getLocation().getRawEncoding()
              << " range-valid=" << integer_literal->getSourceRange().isValid()
              << " macro=" << integer_literal->getLocation().isMacroID()
              << " semantic-depth=" << p_semantic_expression_subtree_depth
              << " template-argument-depth="
              << p_semantic_template_argument_expression_depth
              << " canonical-depth=" << p_canonical_generated_literal_depth
              << std::endl;
    integer_literal->dump();
    ROSE_ABORT();
  }

  if (builtin_type != nullptr) {
    switch (builtin_type->getKind()) {
    case clang::BuiltinType::Int128:
    case clang::BuiltinType::UInt128:
      value_exp = new SgInteger128Val(integer128ValueFromClang(
          llvm::APSInt(value, literal_type->isUnsignedIntegerType())));
      SageInterface::setOneSourcePositionNull(value_exp);
      break;

    case clang::BuiltinType::UChar:
      value_exp = SageBuilder::buildUnsignedCharVal_nfi(
          static_cast<unsigned char>(value.getZExtValue()), spelling);
      break;
    case clang::BuiltinType::UShort:
      value_exp = SageBuilder::buildUnsignedShortVal_nfi(
          static_cast<unsigned short>(value.getZExtValue()), spelling);
      break;
    case clang::BuiltinType::UInt:
      value_exp = SageBuilder::buildUnsignedIntVal_nfi(
          static_cast<unsigned int>(value.getZExtValue()), spelling);
      break;
    case clang::BuiltinType::ULong:
      value_exp = SageBuilder::buildUnsignedLongVal_nfi(
          static_cast<unsigned long>(value.getZExtValue()), spelling);
      break;
    case clang::BuiltinType::ULongLong:
      value_exp = SageBuilder::buildUnsignedLongLongIntVal_nfi(
          static_cast<unsigned long long>(value.getZExtValue()), spelling);
      break;
    case clang::BuiltinType::SChar:
    case clang::BuiltinType::Char_S:
      value_exp = SageBuilder::buildCharVal_nfi(value.getSExtValue(), spelling);
      break;
    case clang::BuiltinType::Char_U:
      value_exp = SageBuilder::buildCharVal_nfi(
          static_cast<int64_t>(value.getZExtValue()), spelling);
      break;
    case clang::BuiltinType::Short:
      value_exp = SageBuilder::buildShortVal_nfi(
          static_cast<short>(value.getSExtValue()), spelling);
      break;
    case clang::BuiltinType::Int:
      value_exp = SageBuilder::buildIntVal_nfi(
          static_cast<int>(value.getSExtValue()), spelling);
      break;
    case clang::BuiltinType::Long:
      value_exp = SageBuilder::buildLongIntVal_nfi(
          static_cast<long>(value.getSExtValue()), spelling);
      break;
    case clang::BuiltinType::LongLong:
      value_exp = SageBuilder::buildLongLongIntVal_nfi(
          static_cast<long long>(value.getSExtValue()), spelling);
      break;
    default:
      break;
    }
  }

  if (value_exp == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[integer-literal-type]: unsupported "
              << literal_type.getAsString() << std::endl;
    ROSE_ABORT();
  }

  *node = value_exp;

  if (semantic_expression_construction) {
    value_exp->set_literal_spelling_form(
        SgValueExp::e_literal_canonical_generated);
    // A literal reached through a canonical semantic QualType is an owned
    // template-value component.  Clang may retain a range from the pattern or
    // substitution that produced it, but that range is construction evidence,
    // not an integer-token surface.  Publish the semantic role while the exact
    // type-construction transaction is active and validate the resulting
    // frontend-specific provenance at this producer boundary.
    applySourceRange(*node, integer_literal->getSourceRange());
  } else if (canonical_generated) {
    value_exp->set_literal_spelling_form(
        SgValueExp::e_literal_canonical_generated);
    setSynthesizedFileInfo(*node);
    mark_compiler_generated_frontend_specific(value_exp);
    SageBuilder::initializeSemanticExpressionSourceProvenance(value_exp);
  } else {
    value_exp->set_literal_spelling_form(SgValueExp::e_literal_source_spelled);
    applySourceRange(*node, integer_literal->getSourceRange());
  }
  const bool visit_ok = VisitExpr(integer_literal, node);
  return visit_ok;
}

bool ClangToSageTranslator::VisitLambdaExpr(clang::LambdaExpr *lambda_expr,
                                            SgNode **node) {
  (void)hasExactSemanticExpressionConstructionRole("VisitLambdaExpr");

  const clang::CXXRecordDecl *clang_lambda_class =
      lambda_expr->getLambdaClass();

  const clang::CXXMethodDecl *clang_call_operator =
      llvm::cast_or_null<clang::CXXMethodDecl>(lambda_expr->getCallOperator());
  clang::Stmt *clang_lambda_body = lambda_expr->getBody();

  const clang::CXXRecordDecl *clang_operator_parent =
      clang_call_operator != nullptr ? llvm::cast_or_null<clang::CXXRecordDecl>(
                                           clang_call_operator->getParent())
                                     : nullptr;
  clang::Stmt *clang_operator_body =
      clang_call_operator != nullptr ? clang_call_operator->getBody() : nullptr;
  if (clang_lambda_class == nullptr || clang_call_operator == nullptr ||
      clang_lambda_body == nullptr || clang_operator_parent == nullptr ||
      clang_operator_body != clang_lambda_body ||
      clang_call_operator->getOverloadedOperator() != clang::OO_Call ||
      !clang_lambda_class->isLambda() ||
      clang_operator_parent->getCanonicalDecl() !=
          clang_lambda_class->getCanonicalDecl()) {
    std::cerr << "REX_FRONTEND_INVARIANT[lambda-producer]: LambdaExpr lacks "
                 "one exact closure class, call operator, or shared body"
              << std::endl;
    ROSE_ABORT();
  }

  // Convert Clang lambda class to ROSE class declaration
  SgClassDeclaration *lambda_closure_class = nullptr;
  SgScopeStatement *lambda_expression_parent = SageBuilder::topScopeStack();
  SgScopeStatement *lambda_closure_semantic_owner = lambda_expression_parent;
  if (SgFunctionDefinition *function_def =
          isSgFunctionDefinition(lambda_closure_semantic_owner)) {
    lambda_closure_semantic_owner = function_def->get_body();
  }

  const clang::ParmVarDecl *default_argument_parameter =
      defaultArgumentParameterForLambda(clang_lambda_class);
  const clang::FunctionDecl *default_argument_function =
      default_argument_parameter != nullptr
          ? llvm::cast<clang::FunctionDecl>(
                default_argument_parameter->getDeclContext())
          : nullptr;

  if (default_argument_function != nullptr) {
    SgFunctionDeclaration *mapped_function =
        isSgFunctionDeclaration(lookupExactSgDeclarationForClangDecl(
            const_cast<clang::FunctionDecl *>(default_argument_function),
            /*allow_on_demand=*/false));
    SgDeclarationScope *parameter_scope =
        mapped_function != nullptr
            ? mapped_function->get_function_declarator_scope()
            : nullptr;
    if (mapped_function == nullptr || parameter_scope == nullptr ||
        SageBuilder::getDeclarationScopeOwner(parameter_scope) !=
            mapped_function) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[lambda-closure-owner]: "
              "default-argument function=%p maps to Sage declaration=%p "
              "without one exact parameter-scope owner\n",
              static_cast<const void *>(default_argument_function),
              static_cast<void *>(mapped_function));
      ROSE_ABORT();
    }
    lambda_closure_semantic_owner = parameter_scope;
  }

  SgClassDeclaration *canonical_closure =
      isSgClassDeclaration(lookupExactSgDeclarationForClangDecl(
          const_cast<clang::CXXRecordDecl *>(clang_lambda_class), false));
  if (canonical_closure != nullptr) {
    auto *canonical_scope =
        isSgDeclarationScope(canonical_closure->get_scope());
    auto *outer = canonical_scope != nullptr
                      ? canonical_scope->get_semantic_outer_scope()
                      : nullptr;
    if (outer == nullptr ||
        (outer != lambda_closure_semantic_owner &&
         (default_argument_function != nullptr ||
          !SageInterface::isAncestor(outer, lambda_expression_parent)))) {
      std::cerr << "REX_FRONTEND_INVARIANT[lambda-closure-owner]: canonical "
                   "closure does not own the exact expression scope"
                << std::endl;
      ROSE_ABORT();
    }
    lambda_closure_semantic_owner = canonical_scope;
  } else {
    auto *context =
        SageBuilder::buildDeclarationScope(lambda_closure_semantic_owner);
    SageBuilder::attachSemanticDeclarationScope(lambda_closure_semantic_owner,
                                                context);
    lambda_closure_semantic_owner = context;
  }

  auto validate_lambda_closure_class_chain = [&](SgClassDeclaration
                                                     *class_decl) {
    if (class_decl == nullptr) {
      std::cerr << "REX_FRONTEND_INVARIANT[lambda-closure-producer]: Clang "
                   "closure did not translate to SgClassDeclaration"
                << std::endl;
      ROSE_ABORT();
    }

    SgClassDeclaration *defining_decl =
        isSgClassDeclaration(class_decl->get_definingDeclaration());
    ROSE_ASSERT(defining_decl == class_decl);

    SgScopeStatement *target_scope = lambda_closure_semantic_owner;
    if (target_scope == nullptr) {
      std::cerr << "REX_FRONTEND_INVARIANT[lambda-closure-owner]: closure "
                   "class has no exact semantic scope"
                << std::endl;
      ROSE_ABORT();
    }

    SgClassDeclaration *first =
        isSgClassDeclaration(class_decl->get_firstNondefiningDeclaration());
    ROSE_ASSERT(first != nullptr && first != class_decl &&
                first->get_firstNondefiningDeclaration() == first &&
                first->get_definingDeclaration() == class_decl);
    for (SgClassDeclaration *current : {first, class_decl}) {
      auto require_synthesized_provenance = [&](SgLocatedNode *located,
                                                const char *role) {
        if (located == nullptr ||
            p_synthesized_source_nodes.count(located) != 1) {
          std::cerr << "REX_FRONTEND_INVARIANT[lambda-closure-provenance]: "
                    << role
                    << " was not classified exactly once by the record "
                       "producer"
                    << std::endl;
          ROSE_ABORT();
        }
        for (Sg_File_Info *file_info :
             {located->get_file_info(), located->get_startOfConstruct(),
              located->get_endOfConstruct()}) {
          if (file_info == nullptr || file_info->get_parent() != located ||
              !file_info->isCompilerGenerated() ||
              !file_info->isFrontendSpecific() ||
              !file_info->isOutputInCodeGeneration() ||
              file_info->isTransformation()) {
            std::cerr << "REX_FRONTEND_INVARIANT[lambda-closure-provenance]: "
                      << role
                      << " has incomplete, shared, or non-synthesized file "
                         "information"
                      << std::endl;
            ROSE_ABORT();
          }
        }
      };

      require_synthesized_provenance(current, "closure declaration");
      if (current->get_isAutonomousDeclaration()) {
        std::cerr << "REX_FRONTEND_INVARIANT[lambda-closure-provenance]: "
                     "closure declaration remains autonomous"
                  << std::endl;
        ROSE_ABORT();
      }
      if (current->get_scope() != target_scope) {
        std::cerr << "REX_FRONTEND_INVARIANT[lambda-closure-owner]: closure="
                  << current << "/" << current->class_name()
                  << " current-scope=" << current->get_scope() << "/"
                  << (current->get_scope() != nullptr
                          ? current->get_scope()->class_name()
                          : std::string("<null>"))
                  << " target-scope=" << target_scope << "/"
                  << target_scope->class_name()
                  << " defining=" << (current == defining_decl ? 1 : 0)
                  << " first="
                  << (current ==
                              defining_decl->get_firstNondefiningDeclaration()
                          ? 1
                          : 0)
                  << " definition=" << current->get_definition()
                  << " construction-owner="
                  << (defining_decl->get_definition() != nullptr
                          ? defining_decl->get_definition()
                                ->get_construction_physical_output_owner()
                          : nullptr)
                  << " does not have its exact construction-time semantic "
                     "scope"
                  << std::endl;
        ROSE_ABORT();
      }

      SgAuxiliaryDeclarationList *owner =
          isSgAuxiliaryDeclarationList(current->get_parent());
      if (owner == nullptr || owner->get_parent() != target_scope ||
          target_scope->get_auxiliary_declarations() != owner ||
          std::count(owner->get_declarations().begin(),
                     owner->get_declarations().end(), current) != 1) {
        std::cerr << "REX_FRONTEND_INVARIANT[lambda-closure-owner]: canonical "
                     "closure has no exact auxiliary owner"
                  << std::endl;
        ROSE_ABORT();
      }

      if (SgSymbol *symbol = current->get_symbol_from_symbol_table()) {
        requireSymbolInExactScope(symbol, target_scope,
                                  "lambda-closure-symbol");
      }
      if (SgClassDefinition *class_def = current->get_definition()) {
        require_synthesized_provenance(class_def, "closure definition");
      }
    }
  };

  if (lambda_expression_parent == nullptr ||
      lambda_closure_semantic_owner == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[lambda-closure-construction-owner]: "
                 "lambda expression has no exact attached source scope"
              << std::endl;
    ROSE_ABORT();
  }
  auto construction_owner = p_lambda_closure_construction_output_owners.emplace(
      clang_lambda_class, lambda_closure_semantic_owner);
  if (!construction_owner.second) {
    std::cerr << "REX_FRONTEND_INVARIANT[lambda-closure-construction-owner]: "
                 "closure record already has an active construction "
                 "transaction"
              << std::endl;
    ROSE_ABORT();
  }
  SgNode *tmp_class =
      Traverse(const_cast<clang::CXXRecordDecl *>(clang_lambda_class));
  if (p_lambda_closure_construction_output_owners.erase(clang_lambda_class) !=
      1) {
    std::cerr << "REX_FRONTEND_INVARIANT[lambda-closure-construction-owner]: "
                 "closure construction transaction was not consumed exactly "
                 "once"
              << std::endl;
    ROSE_ABORT();
  }
  lambda_closure_class = isSgClassDeclaration(tmp_class);
  if (lambda_closure_class != nullptr) {
    lambda_closure_class =
        isSgClassDeclaration(lambda_closure_class->get_definingDeclaration());
  }
  validate_lambda_closure_class_chain(lambda_closure_class);

  const unsigned total_captures = lambda_expr->capture_size();
  auto capture_begin = lambda_expr->capture_begin();
  auto capture_end = lambda_expr->capture_end();
  auto publish_lambda_capture_declaration =
      [&](clang::VarDecl *captured_var) -> SgVariableSymbol * {
    if (captured_var == nullptr) {
      std::cerr << "REX_FRONTEND_INVARIANT[lambda-capture-symbol]: "
                   "captured variable is null"
                << std::endl;
      ROSE_ABORT();
    }
    SgSymbol *captured_symbol = GetSymbolFromSymbolTable(captured_var);
    SgVariableSymbol *var_symbol = isSgVariableSymbol(captured_symbol);
    if (var_symbol == nullptr || var_symbol->get_declaration() == nullptr) {
      std::cerr << "REX_FRONTEND_INVARIANT[lambda-capture-symbol]: "
                   "captured variable has no exact Sage symbol"
                << std::endl;
      ROSE_ABORT();
    }
    auto capture_declaration = p_lambda_capture_declaration_map.emplace(
        std::make_pair(
            static_cast<const clang::FunctionDecl *>(clang_call_operator),
            static_cast<const clang::ValueDecl *>(captured_var)),
        var_symbol->get_declaration());
    if (!capture_declaration.second &&
        capture_declaration.first->second != var_symbol->get_declaration()) {
      std::cerr << "REX_FRONTEND_INVARIANT[lambda-capture-declaration]: "
                   "call operator="
                << clang_call_operator << " captured=" << captured_var << "/"
                << captured_var->getQualifiedNameAsString()
                << " changed its exact Sage declaration from "
                << capture_declaration.first->second << " to "
                << var_symbol->get_declaration() << std::endl;
      ROSE_ABORT();
    }
    return var_symbol;
  };

  // The call-operator body can reference an enclosing captured parameter
  // whose original function cache is retired before a semantic instantiation
  // of this body. Resolve non-init captures while the enclosing body
  // transaction is still active, before recursively translating the call
  // operator. An init-capture is instead a declaration owned by the new
  // call-operator body and cannot be constructed until that body has been
  // published.
  std::unordered_set<const clang::VarDecl *> init_capture_declarations;
  for (auto capture_it = capture_begin; capture_it != capture_end;
       ++capture_it) {
    const clang::LambdaCapture &capture = *capture_it;
    if (!capture.capturesVariable()) {
      continue;
    }
    clang::ValueDecl *captured_value =
        llvm::cast_or_null<clang::ValueDecl>(capture.getCapturedVar());
    clang::VarDecl *captured_var =
        llvm::dyn_cast_or_null<clang::VarDecl>(captured_value);
    if (captured_var == nullptr) {
      std::cerr << "REX_FRONTEND_INVARIANT[lambda-capture-declaration]: "
                   "variable capture has no VarDecl"
                << std::endl;
      ROSE_ABORT();
    }
    if (captured_var->isInitCapture()) {
      if (!init_capture_declarations.insert(captured_var).second) {
        std::cerr << "REX_FRONTEND_INVARIANT[lambda-capture-owner]: "
                     "LambdaExpr contains the same init-capture declaration "
                     "more than once"
                  << std::endl;
        ROSE_ABORT();
      }
      continue;
    }
    (void)publish_lambda_capture_declaration(captured_var);
  }

  struct LambdaInitCaptureOwnerGuard {
    std::vector<LambdaInitCaptureOwnerFrame> &stack;
    const clang::FunctionDecl *call_operator;
    bool active;

    LambdaInitCaptureOwnerGuard(
        std::vector<LambdaInitCaptureOwnerFrame> &owner_stack,
        const clang::FunctionDecl *owner,
        std::unordered_set<const clang::VarDecl *> declarations)
        : stack(owner_stack), call_operator(owner),
          active(!declarations.empty()) {
      if (active) {
        if (call_operator == nullptr) {
          std::cerr << "REX_FRONTEND_INVARIANT[lambda-capture-owner]: "
                       "init-capture transaction has no exact call operator"
                    << std::endl;
          ROSE_ABORT();
        }
        stack.push_back({call_operator, std::move(declarations)});
      }
    }

    ~LambdaInitCaptureOwnerGuard() {
      if (!active) {
        return;
      }
      if (stack.empty() || stack.back().call_operator != call_operator) {
        std::cerr << "REX_FRONTEND_INVARIANT[lambda-capture-owner]: "
                     "init-capture transaction changed before LambdaExpr "
                     "completion"
                  << std::endl;
        ROSE_ABORT();
      }
      stack.pop_back();
    }
  } lambda_init_capture_owner_guard(p_lambda_init_capture_owner_stack,
                                    clang_call_operator,
                                    std::move(init_capture_declarations));

  SgClassDefinition *lambda_closure_definition =
      lambda_closure_class->get_definition();
  for (auto *method : clang_lambda_class->methods()) {
    if (!llvm::isa<clang::CXXConstructorDecl, clang::CXXDestructorDecl>(
            method) &&
        !method->isCopyAssignmentOperator() &&
        !method->isMoveAssignmentOperator())
      continue;
    ROSE_ASSERT(method->isImplicit());
    auto *member = isSgMemberFunctionDeclaration(
        Traverse(const_cast<clang::CXXMethodDecl *>(method)));
    ROSE_ASSERT(member != nullptr &&
                member->get_scope() == lambda_closure_definition);
    auto *owner = isSgAuxiliaryDeclarationList(member->get_parent());
    ROSE_ASSERT(owner != nullptr &&
                owner ==
                    lambda_closure_definition->get_auxiliary_declarations());
  }

  // Convert Clang call operator to ROSE function declaration
  auto call_operator_definition =
      p_lambda_call_operator_definition_transactions.emplace(
          clang_call_operator);
  if (!call_operator_definition.second) {
    std::cerr << "REX_FRONTEND_INVARIANT[lambda-call-operator-producer]: "
                 "call operator="
              << clang_call_operator
              << " already has an active definition transaction" << std::endl;
    ROSE_ABORT();
  }
  SgNode *tmp_func =
      Traverse(const_cast<clang::CXXMethodDecl *>(clang_call_operator));
  if (p_lambda_call_operator_definition_transactions.erase(
          clang_call_operator) != 1) {
    std::cerr << "REX_FRONTEND_INVARIANT[lambda-call-operator-producer]: "
                 "call operator="
              << clang_call_operator
              << " did not consume exactly one definition transaction"
              << std::endl;
    ROSE_ABORT();
  }
  SgFunctionDeclaration *translated_lambda_function =
      isSgFunctionDeclaration(tmp_func);
  SgFunctionDeclaration *lambda_function =
      translated_lambda_function != nullptr
          ? isSgFunctionDeclaration(
                translated_lambda_function->get_definingDeclaration())
          : nullptr;
  SgFunctionDefinition *lambda_function_definition =
      lambda_function != nullptr ? lambda_function->get_definition() : nullptr;
  SgBasicBlock *lambda_function_body =
      lambda_function_definition != nullptr
          ? lambda_function_definition->get_body()
          : nullptr;
  SgAuxiliaryDeclarationList *lambda_function_owner =
      lambda_function != nullptr
          ? isSgAuxiliaryDeclarationList(lambda_function->get_parent())
          : nullptr;
  if (lambda_closure_definition == nullptr || lambda_function == nullptr ||
      lambda_function_definition == nullptr ||
      lambda_function_body == nullptr ||
      lambda_function->get_scope() != lambda_closure_definition ||
      lambda_function_definition->get_declaration() != lambda_function ||
      lambda_function_definition->get_parent() != lambda_function ||
      lambda_function_body->get_parent() != lambda_function_definition ||
      lambda_function_owner == nullptr ||
      lambda_function_owner->get_parent() != lambda_closure_definition ||
      lambda_closure_definition->get_auxiliary_declarations() !=
          lambda_function_owner ||
      std::count(lambda_function_owner->get_declarations().begin(),
                 lambda_function_owner->get_declarations().end(),
                 lambda_function) != 1 ||
      lambda_closure_definition->statementExistsInScope(lambda_function)) {
    std::cerr
        << "REX_FRONTEND_INVARIANT[lambda-call-operator-producer]: closure="
        << lambda_closure_definition << " function=" << lambda_function
        << " translated=" << translated_lambda_function << "/"
        << (translated_lambda_function != nullptr
                ? translated_lambda_function->class_name()
                : std::string("<null>"))
        << " translated-first="
        << (translated_lambda_function != nullptr
                ? translated_lambda_function->get_firstNondefiningDeclaration()
                : nullptr)
        << " translated-defining="
        << (translated_lambda_function != nullptr
                ? translated_lambda_function->get_definingDeclaration()
                : nullptr)
        << " translated-definition="
        << (translated_lambda_function != nullptr
                ? translated_lambda_function->get_definition()
                : nullptr)
        << " translated-parent="
        << (translated_lambda_function != nullptr
                ? translated_lambda_function->get_parent()
                : nullptr)
        << " definition=" << lambda_function_definition
        << " body=" << lambda_function_body << " function-scope="
        << (lambda_function != nullptr ? lambda_function->get_scope() : nullptr)
        << " function-parent="
        << (lambda_function != nullptr ? lambda_function->get_parent()
                                       : nullptr)
        << "/"
        << (lambda_function != nullptr &&
                    lambda_function->get_parent() != nullptr
                ? lambda_function->get_parent()->class_name()
                : std::string("<null>"))
        << " auxiliary=" << lambda_function_owner << " auxiliary-parent="
        << (lambda_function_owner != nullptr
                ? lambda_function_owner->get_parent()
                : nullptr)
        << " direct-count="
        << (lambda_function_owner != nullptr && lambda_function != nullptr
                ? std::count(lambda_function_owner->get_declarations().begin(),
                             lambda_function_owner->get_declarations().end(),
                             lambda_function)
                : 0)
        << " lexical-edge="
        << (lambda_closure_definition != nullptr && lambda_function != nullptr
                ? lambda_closure_definition->statementExistsInScope(
                      lambda_function)
                : false)
        << " lacks one exact defining declaration, body, semantic scope, or "
           "auxiliary construction owner"
        << std::endl;
    ROSE_ABORT();
  }

  SgLambdaCaptureList *lambda_capture_list =
      SageBuilder::buildLambdaCaptureList_nfi();
  applySourceRange(lambda_capture_list, lambda_expr->getIntroducerRange());
  SgScopeStatement *lambda_capture_scope = lambda_function_body;
  const bool semantic_lambda_construction =
      p_semantic_function_body_traversal_depth != 0 ||
      p_semantic_expression_subtree_depth != 0;
  unsigned capture_index = 0;
  auto require_source_expression = [&](SgExpression *expression,
                                       SgNode *expected_parent,
                                       const char *role) {
    if (expression == nullptr || expression->get_parent() != expected_parent ||
        p_synthesized_source_nodes.count(expression) != 0) {
      std::cerr << "REX_FRONTEND_INVARIANT[lambda-capture-provenance]: " << role
                << "=" << expression << "/"
                << (expression != nullptr ? expression->class_name()
                                          : std::string("<null>"))
                << " parent="
                << (expression != nullptr ? expression->get_parent() : nullptr)
                << "/"
                << (expression != nullptr && expression->get_parent() != nullptr
                        ? expression->get_parent()->class_name()
                        : std::string("<null>"))
                << " synthesized-count="
                << (expression != nullptr
                        ? p_synthesized_source_nodes.count(expression)
                        : 0)
                << " is absent, synthesized, or already structurally owned"
                << std::endl;
      ROSE_ABORT();
    }
    for (Sg_File_Info *file_info :
         {expression->get_file_info(), expression->get_startOfConstruct(),
          expression->get_endOfConstruct()}) {
      if (file_info == nullptr || file_info->get_parent() != expression ||
          file_info->isCompilerGenerated() || file_info->isFrontendSpecific() ||
          file_info->isTransformation() ||
          !file_info->isOutputInCodeGeneration() ||
          file_info->get_physical_file_id() < 0) {
        std::cerr << "REX_FRONTEND_INVARIANT[lambda-capture-provenance]: "
                  << role << " lacks one exact source-backed file interval"
                  << std::endl;
        ROSE_ABORT();
      }
    }
  };
  auto require_detached_source_expression = [&](SgExpression *expression,
                                                const char *role) {
    require_source_expression(expression, nullptr, role);
  };
  auto require_semantic_capture_provenance = [&](SgLocatedNode *located,
                                                 const char *role,
                                                 bool require_detached) {
    if (located == nullptr ||
        (require_detached && located->get_parent() != nullptr) ||
        p_synthesized_source_nodes.count(located) != 1 ||
        p_lexical_source_nodes.count(located) != 0) {
      std::cerr << "REX_FRONTEND_INVARIANT[lambda-capture-provenance]: " << role
                << " is absent, source-classified, or already structurally "
                   "owned"
                << std::endl;
      ROSE_ABORT();
    }
    for (Sg_File_Info *file_info :
         {located->get_file_info(), located->get_startOfConstruct(),
          located->get_endOfConstruct()}) {
      if (file_info == nullptr || file_info->get_parent() != located ||
          !file_info->isCompilerGenerated() ||
          !file_info->isFrontendSpecific() ||
          !file_info->isOutputInCodeGeneration() ||
          file_info->isTransformation() ||
          file_info->isSourcePositionUnavailableInFrontend() ||
          file_info->get_file_id() !=
              Sg_File_Info::COMPILER_GENERATED_FILE_ID ||
          file_info->get_physical_file_id() !=
              Sg_File_Info::COMPILER_GENERATED_FILE_ID) {
        std::cerr << "REX_FRONTEND_INVARIANT[lambda-capture-provenance]: "
                  << role << " has incomplete semantic file information"
                  << std::endl;
        ROSE_ABORT();
      }
    }
  };
  auto require_detached_semantic_expression = [&](SgExpression *expression,
                                                  const char *role) {
    require_semantic_capture_provenance(expression, role,
                                        /*require_detached=*/true);
  };
  auto require_detached_source_expression_tree = [&](SgExpression *expression,
                                                     const char *role) {
    if (expression == nullptr || expression->get_parent() != nullptr) {
      std::cerr << "REX_FRONTEND_INVARIANT[lambda-capture-provenance]: " << role
                << " has no detached expression-tree root" << std::endl;
      ROSE_ABORT();
    }

    SgExpression *source_expression = expression;
    SgNode *source_parent = nullptr;
    while (SgCastExp *implicit_cast = isSgCastExp(source_expression)) {
      if (implicit_cast->get_cast_type() != SgCastExp::e_implicit_cast ||
          implicit_cast->get_parent() != source_parent ||
          implicit_cast->get_operand_i() == nullptr ||
          implicit_cast->get_operand_i()->get_parent() != implicit_cast) {
        std::cerr << "REX_FRONTEND_INVARIANT[lambda-capture-provenance]: "
                  << role
                  << " contains a synthesized cast without one exact implicit "
                     "conversion operand"
                  << std::endl;
        ROSE_ABORT();
      }
      if (p_synthesized_source_nodes.count(implicit_cast) != 1 ||
          p_lexical_source_nodes.count(implicit_cast) != 0) {
        std::cerr << "REX_FRONTEND_INVARIANT[lambda-capture-provenance]: "
                  << role
                  << " contains an implicit conversion without one exact "
                     "synthesized role"
                  << std::endl;
        ROSE_ABORT();
      }
      for (Sg_File_Info *file_info : {implicit_cast->get_file_info(),
                                      implicit_cast->get_startOfConstruct(),
                                      implicit_cast->get_endOfConstruct(),
                                      implicit_cast->get_operatorPosition()}) {
        if (file_info == nullptr || file_info->get_parent() != implicit_cast ||
            !file_info->isCompilerGenerated() ||
            file_info->isFrontendSpecific() || !file_info->isImplicitCast() ||
            !file_info->isOutputInCodeGeneration() ||
            file_info->isTransformation() ||
            file_info->isSourcePositionUnavailableInFrontend() ||
            file_info->get_physical_file_id() < 0) {
          std::cerr << "REX_FRONTEND_INVARIANT[lambda-capture-provenance]: "
                    << role
                    << " contains an implicit conversion without exact "
                       "source-bound semantic provenance"
                    << std::endl;
          ROSE_ABORT();
        }
      }
      implicit_cast->validate_semantic_conversion();
      source_parent = implicit_cast;
      source_expression = implicit_cast->get_operand_i();
    }

    require_source_expression(source_expression, source_parent, role);
  };
  auto capture_init_begin = lambda_expr->capture_init_begin();
  if (total_captures > 0 && capture_init_begin == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[lambda-capture-initializer]: capture "
                 "array is absent"
              << std::endl;
    ROSE_ABORT();
  }

  for (auto capture_it = capture_begin; capture_it != capture_end;
       ++capture_it, ++capture_index) {
    const clang::LambdaCapture &capture = *capture_it;
    SgExpression *capture_expression = nullptr;
    bool handled_capture = false;
    const bool is_init_capture = lambda_expr->isInitCapture(&capture);
    const clang::LambdaCaptureKind capture_kind = capture.getCaptureKind();
    if (capture.capturesVLAType())
      continue;
    ROSE_ASSERT(capture_index < total_captures);
    clang::Expr *clang_init_expr = capture_init_begin[capture_index];
    ROSE_ASSERT(clang_init_expr != nullptr);

    if (capture.capturesThis()) {
      if (is_init_capture) {
        std::cerr << "REX_FRONTEND_INVARIANT[lambda-capture-symbol]: this "
                     "capture is incorrectly represented as an init-capture"
                  << std::endl;
        ROSE_ABORT();
      }
      clang::Expr *capture_init = clang_init_expr;
      clang::Expr *semantic_capture_init =
          capture_init != nullptr ? capture_init->IgnoreParenImpCasts()
                                  : nullptr;
      if (auto *direct_init = llvm::dyn_cast_or_null<clang::ParenListExpr>(
              semantic_capture_init)) {
        const unsigned direct_init_argument_count = direct_init->getNumExprs();
        if (direct_init_argument_count != 1) {
          std::cerr << "REX_FRONTEND_INVARIANT[lambda-this-capture-shape]: "
                       "dependent direct-initialization envelope has "
                    << direct_init_argument_count
                    << " arguments instead of exactly one" << std::endl;
          ROSE_ABORT();
        }
        semantic_capture_init = direct_init->getExpr(0)->IgnoreParenImpCasts();
      }
      clang::CXXThisExpr *clang_this = nullptr;
      if (capture_kind == clang::LCK_This) {
        clang_this =
            llvm::dyn_cast_or_null<clang::CXXThisExpr>(semantic_capture_init);
      } else if (capture_kind == clang::LCK_StarThis) {
        clang::CXXConstructExpr *copy_construction =
            llvm::dyn_cast_or_null<clang::CXXConstructExpr>(
                semantic_capture_init);
        ROSE_ASSERT(copy_construction != nullptr &&
                    copy_construction->getConstructor()->isCopyConstructor() &&
                    copy_construction->getNumArgs() != 0);
        for (unsigned index = 1; index < copy_construction->getNumArgs();
             ++index)
          ROSE_ASSERT(llvm::isa<clang::CXXDefaultArgExpr>(
              copy_construction->getArg(index)));
        clang::Expr *copy_argument =
            copy_construction->getArg(0)->IgnoreParenImpCasts();
        clang::UnaryOperator *deref =
            llvm::dyn_cast_or_null<clang::UnaryOperator>(copy_argument);
        clang::Expr *subexpression =
            deref != nullptr && deref->getOpcode() == clang::UO_Deref
                ? deref->getSubExpr()->IgnoreParenImpCasts()
                : nullptr;
        clang_this = llvm::dyn_cast_or_null<clang::CXXThisExpr>(subexpression);
        clang::QualType copied_object_type = copy_construction != nullptr
                                                 ? copy_construction->getType()
                                                 : clang::QualType();
        clang::QualType this_object_type =
            clang_this != nullptr ? clang_this->getType()->getPointeeType()
                                  : clang::QualType();
        if (copied_object_type.isNull() || this_object_type.isNull() ||
            !clang_lambda_class->getASTContext().hasSameUnqualifiedType(
                copied_object_type, this_object_type)) {
          std::cerr << "REX_FRONTEND_INVARIANT[lambda-this-capture-shape]: "
                       "*this capture does not copy the exact pointed-to object"
                    << std::endl;
          ROSE_ABORT();
        }
      } else {
        std::cerr << "REX_FRONTEND_INVARIANT[lambda-this-capture-shape]: "
                     "this capture has a non-this Clang capture kind"
                  << std::endl;
        ROSE_ABORT();
      }
      if (clang_this == nullptr) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[lambda-this-capture-shape]: this "
                "capture initializer has no exact CXXThisExpr shape "
                "capture-index=%u kind=%d implicit=%d initializer=%p/%s "
                "semantic=%p/%s\n",
                capture_index, static_cast<int>(capture_kind),
                capture.isImplicit() ? 1 : 0, static_cast<void *>(capture_init),
                capture_init != nullptr ? capture_init->getStmtClassName()
                                        : "<null>",
                static_cast<void *>(semantic_capture_init),
                semantic_capture_init != nullptr
                    ? semantic_capture_init->getStmtClassName()
                    : "<null>");
        ROSE_ABORT();
      }
      SgType *exact_this_type =
          buildTypeFromQualifiedType(clang_this->getType());
      if (isSgPointerType(exact_this_type) == nullptr) {
        std::cerr << "REX_FRONTEND_INVARIANT[lambda-this-result-type]: this "
                     "capture has no exact cv-qualified Clang pointer type"
                  << std::endl;
        ROSE_ABORT();
      }
      SgSymbol *this_symbol = symbolForExactThisPointerType(exact_this_type);
      if (this_symbol == nullptr) {
        std::cerr << "REX_FRONTEND_INVARIANT[lambda-capture-symbol]: this "
                     "capture result type has no exact published class/nonreal "
                     "symbol"
                  << std::endl;
        ROSE_ABORT();
      }
      capture_expression =
          SageBuilder::buildThisExp_nfi(this_symbol, exact_this_type);
      handled_capture = true;
    } else if (capture.capturesVariable()) {
      clang::ValueDecl *captured_val =
          llvm::cast_or_null<clang::ValueDecl>(capture.getCapturedVar());
      clang::VarDecl *captured_var =
          llvm::dyn_cast_or_null<clang::VarDecl>(captured_val);
      if (captured_var != nullptr) {
        const bool is_init_capture_decl = captured_var->isInitCapture();
        if (is_init_capture_decl != is_init_capture) {
          std::cerr << "REX_FRONTEND_INVARIANT[lambda-capture-declaration]: "
                       "LambdaExpr and captured VarDecl disagree about the "
                       "init-capture role"
                    << std::endl;
          ROSE_ABORT();
        }
        SgVariableSymbol *var_symbol =
            publish_lambda_capture_declaration(captured_var);
        if (is_init_capture_decl) {
          SgInitializedName *capture_name = var_symbol->get_declaration();
          SgVariableDeclaration *capture_decl =
              isSgVariableDeclaration(capture_name->get_declaration());
          SgAuxiliaryDeclarationList *container =
              capture_decl != nullptr
                  ? isSgAuxiliaryDeclarationList(capture_decl->get_parent())
                  : nullptr;
          if (isSgBasicBlock(lambda_capture_scope) == nullptr ||
              capture_name->get_scope() != lambda_capture_scope ||
              capture_decl == nullptr ||
              capture_decl->get_scope() != lambda_capture_scope ||
              container == nullptr ||
              container->get_parent() != lambda_capture_scope ||
              lambda_capture_scope->get_auxiliary_declarations() != container ||
              capture_name->get_parent() != capture_decl ||
              capture_decl->get_variables().size() != 1 ||
              std::count(container->get_declarations().begin(),
                         container->get_declarations().end(),
                         capture_decl) != 1 ||
              lambda_capture_scope->statementExistsInScope(capture_decl) ||
              lambda_capture_scope->find_symbol_from_declaration(
                  capture_name) != var_symbol) {
            fprintf(
                stderr,
                "REX_FRONTEND_INVARIANT[lambda-capture-scope]: "
                "capture=%p declaration=%p declaration-parent=%p/%s "
                "name-parent=%p scope=%p declaration-scope=%p expected=%p/%s "
                "container=%p container-parent=%p body-container=%p "
                "variables=%zu auxiliary-count=%zu lexical-edge=%d "
                "symbol=%p expected-symbol=%p is not owned by the exact "
                "lambda body; translated-function=%p translated-definition=%p "
                "translated-body=%p defining-function=%p "
                "defining-definition=%p defining-body=%p actual-body-parent=%p"
                "/%s actual-function=%p\n",
                static_cast<void *>(capture_name),
                static_cast<void *>(capture_decl),
                capture_decl != nullptr
                    ? static_cast<void *>(capture_decl->get_parent())
                    : nullptr,
                capture_decl != nullptr && capture_decl->get_parent() != nullptr
                    ? capture_decl->get_parent()->class_name().c_str()
                    : "<null>",
                static_cast<void *>(capture_name->get_parent()),
                static_cast<void *>(capture_name->get_scope()),
                capture_decl != nullptr
                    ? static_cast<void *>(capture_decl->get_scope())
                    : nullptr,
                static_cast<void *>(lambda_capture_scope),
                lambda_capture_scope != nullptr
                    ? lambda_capture_scope->class_name().c_str()
                    : "<null>",
                static_cast<void *>(container),
                container != nullptr
                    ? static_cast<void *>(container->get_parent())
                    : nullptr,
                lambda_capture_scope != nullptr
                    ? static_cast<void *>(
                          lambda_capture_scope->get_auxiliary_declarations())
                    : nullptr,
                capture_decl != nullptr ? capture_decl->get_variables().size()
                                        : 0,
                container != nullptr
                    ? std::count(container->get_declarations().begin(),
                                 container->get_declarations().end(),
                                 capture_decl)
                    : 0,
                capture_decl != nullptr && lambda_capture_scope != nullptr
                    ? (lambda_capture_scope->statementExistsInScope(
                           capture_decl)
                           ? 1
                           : 0)
                    : -1,
                lambda_capture_scope != nullptr
                    ? static_cast<void *>(
                          lambda_capture_scope->find_symbol_from_declaration(
                              capture_name))
                    : nullptr,
                static_cast<void *>(var_symbol),
                static_cast<void *>(translated_lambda_function),
                translated_lambda_function != nullptr
                    ? static_cast<void *>(
                          translated_lambda_function->get_definition())
                    : nullptr,
                translated_lambda_function != nullptr &&
                        translated_lambda_function->get_definition() != nullptr
                    ? static_cast<void *>(
                          translated_lambda_function->get_definition()
                              ->get_body())
                    : nullptr,
                static_cast<void *>(lambda_function),
                static_cast<void *>(lambda_function_definition),
                static_cast<void *>(lambda_function_body),
                capture_name->get_scope() != nullptr
                    ? static_cast<void *>(
                          capture_name->get_scope()->get_parent())
                    : nullptr,
                capture_name->get_scope() != nullptr &&
                        capture_name->get_scope()->get_parent() != nullptr
                    ? capture_name->get_scope()
                          ->get_parent()
                          ->class_name()
                          .c_str()
                    : "<null>",
                capture_name->get_scope() != nullptr &&
                        isSgFunctionDefinition(
                            capture_name->get_scope()->get_parent()) != nullptr
                    ? static_cast<void *>(
                          isSgFunctionDefinition(
                              capture_name->get_scope()->get_parent())
                              ->get_declaration())
                    : nullptr);
            ROSE_ABORT();
          }
        }
        capture_expression = SageBuilder::buildVarRefExp_nfi(var_symbol);
        handled_capture = true;
      } else {
        std::cerr << "REX_FRONTEND_INVARIANT[lambda-capture-declaration]: "
                     "variable capture has no VarDecl"
                  << std::endl;
        ROSE_ABORT();
      }
    }

    if (!handled_capture || capture_expression == nullptr) {
      std::cerr << "REX_FRONTEND_INVARIANT[lambda-capture-expression]: "
                   "lambda capture did not translate to an exact expression"
                << std::endl;
      ROSE_ABORT();
    }

    const bool is_implicit = capture.isImplicit();
    const clang::SourceLocation capture_location = capture.getLocation();
    if (is_implicit || semantic_lambda_construction) {
      setSynthesizedFileInfo(capture_expression);
      mark_compiler_generated_frontend_specific(capture_expression);
      require_detached_semantic_expression(capture_expression,
                                           "semantic capture variable");
    } else {
      if (!capture_location.isValid()) {
        std::cerr << "REX_FRONTEND_INVARIANT[lambda-capture-provenance]: "
                     "explicit capture has no exact source location"
                  << std::endl;
        ROSE_ABORT();
      }
      applySourceRange(capture_expression,
                       clang::SourceRange(capture_location, capture_location));
      require_detached_source_expression(capture_expression,
                                         "capture variable");
    }

    const bool semantic_initializer =
        !is_init_capture || semantic_lambda_construction;
    SgExpression *capture_initializer =
        semantic_initializer
            ? translateSemanticExpressionSubtree(
                  clang_init_expr, "VisitLambdaExpr:capture-initializer")
            : isSgExpression(Traverse(clang_init_expr));
    ROSE_ASSERT(capture_initializer != nullptr);
    capture_initializer = prepareExpressionForAttachment(capture_initializer);
    if (semantic_initializer) {
      require_detached_semantic_expression(capture_initializer,
                                           "semantic capture initializer");
    } else {
      require_detached_source_expression_tree(capture_initializer,
                                              "init-capture initializer");
    }

    bool capture_by_reference =
        (capture_kind == clang::LCK_ByRef || capture_kind == clang::LCK_This);

    SgLambdaCapture *sg_capture = SageBuilder::buildLambdaCapture_nfi(
        capture_expression, capture_initializer, nullptr, capture_by_reference,
        is_implicit, capture.isPackExpansion(), is_init_capture);
    if (is_implicit || semantic_lambda_construction) {
      setSynthesizedFileInfo(sg_capture);
      mark_compiler_generated_frontend_specific(sg_capture);
      require_semantic_capture_provenance(sg_capture,
                                          "VisitLambdaExpr:semantic-capture",
                                          /*require_detached=*/true);
    } else {
      clang::SourceLocation capture_end_location = capture_location;
      if (is_init_capture) {
        capture_end_location = clang_init_expr->getSourceRange().getEnd();
      } else if (capture.isPackExpansion()) {
        capture_end_location = capture.getEllipsisLoc();
      }
      if (!capture_end_location.isValid()) {
        std::cerr << "REX_FRONTEND_INVARIANT[lambda-capture-provenance]: "
                     "explicit capture has no exact source end"
                  << std::endl;
        ROSE_ABORT();
      }
      applySourceRange(sg_capture, clang::SourceRange(capture_location,
                                                      capture_end_location));
    }

    lambda_capture_list->get_capture_list().push_back(sg_capture);
    sg_capture->set_parent(lambda_capture_list);
  }
  if (capture_index != total_captures) {
    std::cerr << "REX_FRONTEND_INVARIANT[lambda-capture-count]: capture "
                 "iterator and capture array disagree"
              << std::endl;
    ROSE_ABORT();
  }

  SgLambdaExp *built_lambda = SageBuilder::buildLambdaExp_nfi(
      lambda_capture_list, lambda_closure_class, lambda_function);
  if (built_lambda == nullptr ||
      built_lambda->get_lambda_capture_list() != lambda_capture_list ||
      lambda_capture_list->get_parent() != built_lambda ||
      built_lambda->get_lambda_closure_class() != lambda_closure_class ||
      isSgAuxiliaryDeclarationList(lambda_closure_class->get_parent()) ==
          nullptr ||
      built_lambda->get_lambda_function() != lambda_function ||
      lambda_function->get_parent() != lambda_function_owner) {
    std::cerr << "REX_FRONTEND_INVARIANT[lambda-owner]: lambda expression "
                 "did not retain its exact captures and canonical declarations"
              << std::endl;
    ROSE_ABORT();
  }

  clang::LambdaCaptureDefault capture_default =
      lambda_expr->getCaptureDefault();
  bool has_default_capture = (capture_default != clang::LCD_None);
  built_lambda->set_capture_default(has_default_capture);
  built_lambda->set_default_is_by_reference(capture_default ==
                                            clang::LCD_ByRef);
  built_lambda->set_has_parameter_decl(lambda_expr->hasExplicitParameters());
  built_lambda->set_is_mutable(lambda_expr->isMutable());
  const bool has_explicit_result_type = lambda_expr->hasExplicitResultType();
  built_lambda->set_explicit_return_type(has_explicit_result_type);
  if (has_explicit_result_type) {
    clang::TypeSourceInfo *call_operator_type_info =
        clang_call_operator->getTypeSourceInfo();
    clang::FunctionTypeLoc written_function_type_loc;
    if (call_operator_type_info != nullptr) {
      for (clang::TypeLoc current = call_operator_type_info->getTypeLoc();
           !current.isNull(); current = current.getNextTypeLoc()) {

        if (clang::FunctionTypeLoc function_loc =
                current.getAs<clang::FunctionTypeLoc>()) {

          written_function_type_loc = function_loc;
          break;
        }
      }
    }
    if (written_function_type_loc.isNull()) {
      std::cerr
          << "REX_FRONTEND_INVARIANT[lambda-return-type-qualification]: "
             "explicit lambda result has no exact written function TypeLoc"
          << std::endl;
      ROSE_ABORT();
    }
    clang::TypeLoc return_type_loc = written_function_type_loc.getReturnLoc();
    preserveExplicitFunctionReturnTypeQualifier(lambda_function,
                                                return_type_loc);
  }

  applySourceRange(built_lambda, lambda_expr->getSourceRange());
  ROSE_ASSERT(
      lambda_closure_definition->get_construction_physical_output_owner() ==
      nullptr);

  auto require_source_backed_output = [](SgLocatedNode *located,
                                         const char *role) {
    if (located == nullptr) {
      std::cerr << "REX_FRONTEND_INVARIANT[lambda-source-owner]: missing "
                << role << std::endl;
      ROSE_ABORT();
    }
    for (Sg_File_Info *file_info :
         {located->get_file_info(), located->get_startOfConstruct(),
          located->get_endOfConstruct()}) {
      if (file_info == nullptr || file_info->get_parent() != located ||
          file_info->isCompilerGenerated() || file_info->isFrontendSpecific() ||
          file_info->isTransformation() ||
          !file_info->isOutputInCodeGeneration() ||
          file_info->get_file_id() < 0 ||
          file_info->get_physical_file_id() < 0 || file_info->get_line() <= 0 ||
          file_info->get_col() <= 0) {
        std::cerr << "REX_FRONTEND_INVARIANT[lambda-source-owner]: " << role
                  << " is not classified by its exact source interval"
                  << std::endl;
        ROSE_ABORT();
      }
    }
  };
  auto require_semantic_output = [&](SgLocatedNode *located, const char *role) {
    if (located == nullptr) {
      std::cerr << "REX_FRONTEND_INVARIANT[lambda-semantic-owner]: missing "
                << role << std::endl;
      ROSE_ABORT();
    }
    for (Sg_File_Info *file_info :
         {located->get_file_info(), located->get_startOfConstruct(),
          located->get_endOfConstruct()}) {
      if (file_info == nullptr || file_info->get_parent() != located ||
          !file_info->isCompilerGenerated() ||
          !file_info->isFrontendSpecific() || file_info->isTransformation() ||
          !file_info->isOutputInCodeGeneration() ||
          file_info->get_file_id() !=
              Sg_File_Info::COMPILER_GENERATED_FILE_ID ||
          file_info->get_physical_file_id() !=
              Sg_File_Info::COMPILER_GENERATED_FILE_ID) {
        std::cerr << "REX_FRONTEND_INVARIANT[lambda-semantic-owner]: " << role
                  << " has incomplete semantic file provenance" << std::endl;
        ROSE_ABORT();
      }
    }
  };
  require_semantic_output(lambda_function_definition,
                          "call-operator definition");
  ROSE_ASSERT(p_synthesized_source_nodes.count(lambda_function_body) +
                  p_lexical_source_nodes.count(lambda_function_body) ==
              1);
  if (p_synthesized_source_nodes.count(lambda_function_body) == 1) {
    require_semantic_output(lambda_function_body, "lambda body");
  } else {
    require_source_backed_output(lambda_function_body, "lambda body");
  }
  for (Sg_File_Info *file_info : {lambda_function->get_file_info(),
                                  lambda_function->get_startOfConstruct(),
                                  lambda_function->get_endOfConstruct()}) {
    if (file_info == nullptr || file_info->get_parent() != lambda_function ||
        !file_info->isCompilerGenerated() || !file_info->isFrontendSpecific() ||
        file_info->isTransformation() ||
        !file_info->isOutputInCodeGeneration()) {
      std::cerr << "REX_FRONTEND_INVARIANT[lambda-call-operator-provenance]: "
                   "semantic call operator lacks exact synthesized "
                   "provenance"
                << std::endl;
      ROSE_ABORT();
    }
  }

  *node = built_lambda;

  return VisitExpr(lambda_expr, node);
}

bool ClangToSageTranslator::VisitMaterializeTemporaryExpr(
    clang::MaterializeTemporaryExpr *materialize_temporary_expr,
    SgNode **node) {
  clang::Expr *sub_expr = materialize_temporary_expr->getSubExpr();
  SgNode *translated = Traverse(sub_expr);
  SgExpression *expression = isSgExpression(translated);
  if (sub_expr == nullptr || expression == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[materialized-temporary]: exact "
                 "operand translated to "
              << (translated == nullptr ? std::string("<null>")
                                        : translated->class_name())
              << " instead of SgExpression" << std::endl;
    ROSE_ABORT();
  }
  const clang::ValueDecl *extending =
      materialize_temporary_expr->getExtendingDecl();
  if (auto *binding = llvm::dyn_cast_or_null<clang::BindingDecl>(extending))
    extending = binding->getHoldingVar() != nullptr
                    ? binding->getHoldingVar()
                    : binding->getDecomposedDecl();
  // Field defaults have their containing object's lifetime. Decomposition
  // storage belongs to the decomposed declaration, not its lexical context.
  clang::StorageDuration duration = clang::SD_FullExpression;
  if (extending != nullptr && !llvm::isa<clang::FieldDecl>(extending)) {
    const auto *variable = llvm::dyn_cast<clang::VarDecl>(extending);
    ASSERT_not_null(variable);
    const auto binding_storage = p_binding_storage_constructions.find(variable);
    duration = binding_storage != p_binding_storage_constructions.end()
                   ? binding_storage->second.decomposition->getStorageDuration()
                   : variable->getStorageDuration();
  }
  SgExpression::temporary_storage_duration_enum storage =
      SgExpression::e_temporary_storage_none;
  switch (duration) {
  case clang::SD_FullExpression:
    storage = SgExpression::e_temporary_storage_full_expression;
    break;
  case clang::SD_Automatic:
    storage = SgExpression::e_temporary_storage_automatic;
    break;
  case clang::SD_Thread:
    storage = SgExpression::e_temporary_storage_thread;
    break;
  case clang::SD_Static:
    storage = SgExpression::e_temporary_storage_static;
    break;
  case clang::SD_Dynamic:
    ROSE_ABORT();
  }
  if (llvm::isa_and_nonnull<clang::FieldDecl>(extending))
    storage = SgExpression::e_temporary_storage_containing_object;
  SgInitializedName *owner = nullptr;
  const auto *field = llvm::dyn_cast_or_null<clang::FieldDecl>(extending);
  if (field != nullptr && p_decl_translation_in_progress.count(
                              const_cast<clang::FieldDecl *>(field)) != 0) {
    const auto construction =
        std::find_if(p_field_initializer_constructions.rbegin(),
                     p_field_initializer_constructions.rend(),
                     [field](const FieldInitializerConstruction &entry) {
                       return entry.declaration == field;
                     });
    ROSE_ASSERT(construction != p_field_initializer_constructions.rend());
    owner = construction->initialized_name;
  } else if (extending != nullptr) {
    owner = exactInitializedNameFromTranslatedVariableNode(
        Traverse(const_cast<clang::ValueDecl *>(extending)));
  }
  ROSE_ASSERT(extending == nullptr || owner != nullptr);
  ROSE_ASSERT(SgExpression::is_valid_temporary_materialization(storage, owner));
  expression->set_semantic_wrapper_mask(
      static_cast<SgExpression::semantic_wrapper_mask_enum>(
          expression->get_semantic_wrapper_mask() |
          SgExpression::e_materialized_temporary),
      expression->get_bound_temporary_destructor(), storage, owner);
  *node = expression;
  return VisitExpr(materialize_temporary_expr, node);
}

bool ClangToSageTranslator::VisitMemberExpr(clang::MemberExpr *member_expr,
                                            SgNode **node) {

  const clang::QualType clang_result_type = member_expr->getType();
  const clang::Type *clang_result_type_ptr =
      clang_result_type.getTypePtrOrNull();
  const clang::BuiltinType *builtin_result_type =
      llvm::dyn_cast_or_null<clang::BuiltinType>(clang_result_type_ptr);
  const bool has_bound_member_pseudo_type =
      builtin_result_type != nullptr &&
      builtin_result_type->getKind() == clang::BuiltinType::BoundMember;
  SgType *result_type = has_bound_member_pseudo_type
                            ? nullptr
                            : buildTypeFromQualifiedType(clang_result_type);
  auto require_exact_result_type = [&]() {
    if (SageInterface::containsUnknownType(result_type)) {
      std::cerr << "REX_FRONTEND_INVARIANT[member-result-type]: Clang member "
                   "expression has no exact semantic result type"
                << std::endl;
      ROSE_ABORT();
    }
  };
  if (!has_bound_member_pseudo_type) {
    require_exact_result_type();
  }

  const bool synthesized_member_callee =
      p_synthesized_callee_expressions.count(member_expr) == 1;
  const bool semantic_member_expression =
      hasExactSemanticExpressionConstructionRole("VisitMemberExpr");
  const bool member_uses_synthesized_provenance =
      synthesized_member_callee || semantic_member_expression;

  clang::Expr *member_base = member_expr->getBase();
  clang::ValueDecl *raw_member_decl = member_expr->getMemberDecl();
  std::optional<bool> stripped_anonymous_member_operator_is_arrow;
  std::vector<clang::NamedDecl *> stripped_anonymous_member_fields;
  if (llvm::isa<clang::FieldDecl>(raw_member_decl)) {
    while (clang::MemberExpr *promoted_base =
               llvm::dyn_cast_or_null<clang::MemberExpr>(member_base)) {

      clang::FieldDecl *promoted_field =
          llvm::dyn_cast_or_null<clang::FieldDecl>(
              promoted_base->getMemberDecl());
      if (promoted_field == nullptr ||
          !promoted_field->isAnonymousStructOrUnion()) {
        break;
      }

      clang::Expr *surviving_base = promoted_base->getBase();
      if (surviving_base == nullptr || surviving_base == promoted_base) {
        std::cerr << "REX_FRONTEND_INVARIANT[anonymous-member-owner]: Clang "
                     "promoted member hop has no distinct structural base"
                  << std::endl;
        ROSE_ABORT();
      }
      if (stripped_anonymous_member_operator_is_arrow.value_or(false)) {
        std::cerr << "REX_FRONTEND_INVARIANT[anonymous-member-operator]: "
                     "non-innermost promoted anonymous member hop uses an "
                     "arrow operator"
                  << std::endl;
        ROSE_ABORT();
      }
      stripped_anonymous_member_operator_is_arrow = promoted_base->isArrow();
      stripped_anonymous_member_fields.push_back(promoted_field);
      member_base = surviving_base;
    }
  }

  auto strip_implicit_base_wrappers = [](clang::Expr *expr) -> clang::Expr * {
    while (expr != nullptr) {
      if (clang::ParenExpr *paren = llvm::dyn_cast<clang::ParenExpr>(expr)) {
        expr = llvm::dyn_cast_or_null<clang::Expr>(paren->getSubExpr());
        continue;
      }
      if (clang::ImplicitCastExpr *cast =
              llvm::dyn_cast<clang::ImplicitCastExpr>(expr)) {
        expr = llvm::dyn_cast_or_null<clang::Expr>(cast->getSubExpr());
        continue;
      }
      return expr;
    }
    return nullptr;
  };

  clang::VarDecl *implicit_anonymous_base_variable = nullptr;
  if (clang::DeclRefExpr *base_reference =
          llvm::dyn_cast_or_null<clang::DeclRefExpr>(
              strip_implicit_base_wrappers(member_base))) {
    clang::VarDecl *candidate =
        llvm::dyn_cast_or_null<clang::VarDecl>(base_reference->getDecl());
    const clang::RecordType *candidate_record_type =
        candidate != nullptr ? candidate->getType()->getAs<clang::RecordType>()
                             : nullptr;
    clang::RecordDecl *candidate_record = candidate_record_type != nullptr
                                              ? candidate_record_type->getDecl()
                                              : nullptr;
    if (llvm::isa<clang::FieldDecl>(raw_member_decl) && candidate != nullptr &&
        candidate->isImplicit() && candidate->getName().empty() &&
        candidate_record != nullptr &&
        candidate_record->isAnonymousStructOrUnion()) {
      implicit_anonymous_base_variable = candidate;
    }
  }

  // Clang spells access through anonymous aggregates as a MemberExpr chain
  // whose hidden hops name the anonymous fields or namespace-scope backing
  // variable.  The independently stored IndirectFieldDecl is the exact lookup
  // identity injected into the outer scope.  Bind the Sage reference to that
  // declaration instead of constructing `(<unnamed>).member`: the unnamed
  // base owns no source token, and retaining its dot/arrow creates a malformed
  // AST that only token-based unparsing can hide.
  const bool has_anonymous_member_path =
      implicit_anonymous_base_variable != nullptr ||
      !stripped_anonymous_member_fields.empty();
  if (has_anonymous_member_path) {
    clang::FieldDecl *terminal_field =
        llvm::dyn_cast_or_null<clang::FieldDecl>(raw_member_decl);
    if (terminal_field == nullptr) {
      std::cerr << "REX_FRONTEND_INVARIANT[anonymous-member-identity]: "
                   "promoted member path does not terminate in a field"
                << std::endl;
      ROSE_ABORT();
    }

    std::vector<clang::NamedDecl *> exact_chain;
    exact_chain.reserve(stripped_anonymous_member_fields.size() + 2);
    if (implicit_anonymous_base_variable != nullptr) {
      exact_chain.push_back(implicit_anonymous_base_variable);
    }
    for (auto field = stripped_anonymous_member_fields.rbegin();
         field != stripped_anonymous_member_fields.rend(); ++field) {
      exact_chain.push_back(*field);
    }
    exact_chain.push_back(terminal_field);

    clang::DeclContext *injection_context =
        exact_chain.front()->getDeclContext();
    clang::IndirectFieldDecl *exact_indirect_field = nullptr;
    if (injection_context != nullptr) {
      for (clang::Decl *candidate_decl : injection_context->decls()) {
        clang::IndirectFieldDecl *candidate =
            llvm::dyn_cast_or_null<clang::IndirectFieldDecl>(candidate_decl);
        if (candidate == nullptr ||
            candidate->getName() != terminal_field->getName()) {
          continue;
        }
        llvm::ArrayRef<clang::NamedDecl *> candidate_chain = candidate->chain();
        if (candidate_chain.size() != exact_chain.size() ||
            !std::equal(candidate_chain.begin(), candidate_chain.end(),
                        exact_chain.begin())) {
          continue;
        }
        if (exact_indirect_field != nullptr &&
            exact_indirect_field != candidate) {
          std::cerr << "REX_FRONTEND_INVARIANT[anonymous-member-identity]: "
                       "anonymous member path names multiple injected fields"
                    << std::endl;
          ROSE_ABORT();
        }
        exact_indirect_field = candidate;
      }
    }
    if (exact_indirect_field == nullptr) {
      std::cerr << "REX_FRONTEND_INVARIANT[anonymous-member-identity]: "
                   "Clang anonymous member path has no exact injected field"
                << std::endl;
      ROSE_ABORT();
    }
    raw_member_decl = exact_indirect_field;
  }
  const bool member_has_qualifier = member_expr->hasQualifier();
  const bool injected_member_without_explicit_base =
      implicit_anonymous_base_variable != nullptr;
  bool implicit_access =
      injected_member_without_explicit_base || isImplicitCxxThis(member_base);
  auto is_instance_member_access = [&](clang::ValueDecl *decl) {
    if (decl == nullptr) {
      return false;
    }
    if (llvm::isa<clang::FieldDecl>(decl) ||
        llvm::isa<clang::IndirectFieldDecl>(decl)) {
      return true;
    }
    if (clang::CXXMethodDecl *method_decl =
            llvm::dyn_cast<clang::CXXMethodDecl>(decl)) {
      return !method_decl->isStatic();
    }
    return false;
  };

  bool materialize_implicit_base = implicit_access &&
                                   !injected_member_without_explicit_base &&
                                   is_instance_member_access(raw_member_decl);

  SgExpression *base = nullptr;
  if (!implicit_access || materialize_implicit_base) {
    SgNode *tmp_base = Traverse(member_base);
    base = isSgExpression(tmp_base);
    ROSE_ASSERT(base != nullptr);
  }

  if (raw_member_decl == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[member-reference]: MemberExpr has no "
                 "member declaration"
              << std::endl;
    ROSE_ABORT();
  }

  if (clang::FunctionDecl *member_function =
          llvm::dyn_cast<clang::FunctionDecl>(raw_member_decl)) {
    const clang::TemplateSpecializationKind member_kind =
        member_function->getTemplateSpecializationKind();
    if (member_function->doesThisDeclarationHaveABody() &&
        (member_kind == clang::TSK_ImplicitInstantiation ||
         member_kind == clang::TSK_ExplicitInstantiationDeclaration ||
         member_kind == clang::TSK_ExplicitInstantiationDefinition)) {
      if (p_pending_function_body_completions_set.insert(member_function)
              .second) {
        p_pending_function_body_completions.push_back(member_function);
      }
    }
  }

  const auto *method = llvm::dyn_cast<clang::CXXMethodDecl>(raw_member_decl);
  const bool virtual_dispatch =
      method != nullptr && method->isVirtual() &&
      member_expr->performsVirtualDispatch(p_compiler_instance->getLangOpts());

  SgNode *translated_member = nullptr;
  auto exact_translation = p_decl_translation_map.find(raw_member_decl);
  if (exact_translation != p_decl_translation_map.end()) {
    translated_member = exact_translation->second;
  } else {
    translated_member = TraverseOnDemand(raw_member_decl);
    exact_translation = p_decl_translation_map.find(raw_member_decl);
    if (exact_translation != p_decl_translation_map.end()) {
      translated_member = exact_translation->second;
    }
  }
  if (SgTemplateInstantiationDirectiveStatement *directive =
          isSgTemplateInstantiationDirectiveStatement(translated_member)) {
    translated_member = directive->get_declaration();
  }
  if (translated_member == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[member-reference]: Clang "
              << raw_member_decl->getDeclKindName() << " '"
              << raw_member_decl->getQualifiedNameAsString()
              << "' has no exact translated declaration"
              << " declaration=" << raw_member_decl;
    if (clang::FunctionDecl *function_decl =
            llvm::dyn_cast<clang::FunctionDecl>(raw_member_decl)) {
      clang::FunctionDecl *canonical = function_decl->getCanonicalDecl();
      clang::DeclContext *owner_context = function_decl->getDeclContext();
      clang::RecordDecl *owner =
          owner_context != nullptr
              ? llvm::dyn_cast<clang::RecordDecl>(
                    clang::Decl::castFromDeclContext(owner_context))
              : nullptr;
      auto owner_translation = owner != nullptr
                                   ? p_decl_translation_map.find(owner)
                                   : p_decl_translation_map.end();
      std::cerr << " canonical=" << canonical << " owner=" << owner
                << " declaration-in-progress="
                << (p_decl_translation_in_progress.count(function_decl) != 0)
                << " declaration-on-demand="
                << (p_decl_translation_on_demand.count(function_decl) != 0)
                << " owner-mapped="
                << (owner_translation != p_decl_translation_map.end()
                        ? owner_translation->second
                        : nullptr)
                << " owner-in-progress="
                << (owner != nullptr &&
                    p_decl_translation_in_progress.count(owner) != 0)
                << " owner-on-demand="
                << (owner != nullptr &&
                    p_decl_translation_on_demand.count(owner) != 0);
      for (clang::FunctionDecl *redecl : function_decl->redecls()) {
        auto mapping = p_decl_translation_map.find(redecl);
        std::cerr << " [redecl=" << redecl << " mapped="
                  << (mapping != p_decl_translation_map.end() ? mapping->second
                                                              : nullptr)
                  << " lexical=" << redecl->getLexicalDeclContext()
                  << " semantic=" << redecl->getDeclContext() << "]";
      }
    }
    std::cerr << std::endl;
    ROSE_ABORT();
  }

  if (has_bound_member_pseudo_type) {
    clang::FunctionDecl *clang_function =
        llvm::dyn_cast<clang::FunctionDecl>(raw_member_decl);
    SgFunctionDeclaration *sage_function =
        isSgFunctionDeclaration(translated_member);
    if (clang_function == nullptr || sage_function == nullptr) {
      std::cerr << "REX_FRONTEND_INVARIANT[bound-member-result-type]: Clang "
                << raw_member_decl->getDeclKindName() << " '"
                << raw_member_decl->getQualifiedNameAsString()
                << "' has a bound-member pseudo-type but no exact translated "
                   "function declaration"
                << std::endl;
      ROSE_ABORT();
    }
    // BoundMember is a Clang-only placeholder for an instance method selected
    // through an object expression; it is not a source or semantic C++ type.
    // The translated method declaration owns the exact callable type used by
    // the Sage dot/arrow expression.
    result_type = sage_function->get_type();
    require_exact_result_type();
  }

  const clang::TemplateArgumentList *semantic_arguments = nullptr;
  if (clang::FunctionDecl *function_decl =
          llvm::dyn_cast<clang::FunctionDecl>(raw_member_decl)) {
    semantic_arguments = function_decl->getTemplateSpecializationArgs();
  } else if (clang::VarTemplateSpecializationDecl *variable_decl =
                 llvm::dyn_cast<clang::VarTemplateSpecializationDecl>(
                     raw_member_decl)) {
    semantic_arguments = &variable_decl->getTemplateArgs();
  }

  // As with DeclRefExpr, do not infer a template-id from trailing source text:
  // a member expression may also be followed by CUDA launch configuration.
  clang::TemplateArgumentListInfo written_arguments;
  const bool has_explicit_template_id = member_expr->hasExplicitTemplateArgs();
  size_t explicit_argument_count = 0;
  if (has_explicit_template_id) {
    member_expr->copyTemplateArgumentsInto(written_arguments);
    explicit_argument_count = countExpandedTemplateArguments(written_arguments);
  }

  SgExpression *sg_member_expr = nullptr;
  if (has_explicit_template_id) {
    SgTemplateArgumentPtrList template_arguments;
    if (written_arguments.size() != 0) {
      template_arguments = buildTemplateArguments(written_arguments, true);
    }
    if (template_arguments.size() != explicit_argument_count) {
      std::cerr
          << "REX_FRONTEND_INVARIANT[explicit-member-template-reference-"
             "arguments]: Clang explicit template-id for '"
          << raw_member_decl->getQualifiedNameAsString() << "' owns "
          << explicit_argument_count
          << " written argument(s), but exact source translation produced "
          << template_arguments.size() << std::endl;
      ROSE_ABORT();
    }
    for (SgTemplateArgument *argument : template_arguments) {
      if (argument == nullptr || !argument->get_explicitlySpecified()) {
        std::cerr
            << "REX_FRONTEND_INVARIANT[explicit-member-template-reference-"
               "arguments]: translated written argument for '"
            << raw_member_decl->getQualifiedNameAsString()
            << "' lost its explicit source identity" << std::endl;
        ROSE_ABORT();
      }
    }

    const std::string template_name =
        member_expr->getMemberNameInfo().getAsString();
    if (template_name.empty()) {
      std::cerr << "REX_FRONTEND_INVARIANT[member-template-reference]: "
                   "explicit template-id has no member name"
                << std::endl;
      ROSE_ABORT();
    }

    if (semantic_arguments == nullptr &&
        !member_expr->isInstantiationDependent()) {
      std::cerr << "REX_FRONTEND_INVARIANT[explicit-member-template-"
                   "reference-semantic-arguments]: resolved template-id '"
                << raw_member_decl->getQualifiedNameAsString()
                << "' has no exact semantic template-argument list"
                << std::endl;
      ROSE_ABORT();
    }

    SgTemplateArgumentPtrList semantic_template_arguments;
    if (semantic_arguments != nullptr) {
      semantic_template_arguments =
          buildTemplateArguments(*semantic_arguments, explicit_argument_count,
                                 /*reconstruct_explicit_source_surface=*/false);
    } else {
      semantic_template_arguments = buildSemanticTemplateArguments(
          written_arguments, true, "member-reference-template-id");
    }

    SgScopeStatement *reference_scope = SageBuilder::topScopeStack();
    if (reference_scope == nullptr) {
      std::cerr << "REX_FRONTEND_INVARIANT[explicit-member-template-reference-"
                   "scope]: template-id '"
                << raw_member_decl->getQualifiedNameAsString()
                << "' has no active lexical scope" << std::endl;
      ROSE_ABORT();
    }
    clang::NestedNameSpecifier qualifier = member_expr->getQualifier();
    SgDeclarationStatement *template_target = nullptr;
    if (clang::FunctionDecl *function_decl =
            llvm::dyn_cast<clang::FunctionDecl>(raw_member_decl)) {
      SgFunctionDeclaration *resolved_function =
          isSgFunctionDeclaration(translated_member);
      if (resolved_function == nullptr) {
        std::cerr
            << "REX_FRONTEND_INVARIANT[explicit-member-template-callable]: "
               "resolved Clang function '"
            << function_decl->getQualifiedNameAsString()
            << "' has no exact Sage function declaration" << std::endl;
        ROSE_ABORT();
      }
      template_target = resolved_function;
    } else if (clang::VarTemplateSpecializationDecl *variable_decl =
                   llvm::dyn_cast<clang::VarTemplateSpecializationDecl>(
                       raw_member_decl)) {
      SgTemplateVariableDeclaration *resolved_variable =
          isSgTemplateVariableDeclaration(translated_member);
      if (resolved_variable == nullptr ||
          resolved_variable->get_variables().size() != 1 ||
          resolved_variable->get_variables().front() == nullptr ||
          resolved_variable->get_variables().front()->get_parent() !=
              resolved_variable ||
          resolved_variable->get_variables().front()->get_declptr() !=
              resolved_variable ||
          resolved_variable->get_variables().front()->get_scope() == nullptr ||
          resolved_variable->get_variables().front()->get_type() == nullptr) {
        std::cerr << "REX_FRONTEND_INVARIANT[explicit-variable-template-member-"
                     "reference]: resolved Clang variable template '"
                  << variable_decl->getQualifiedNameAsString()
                  << "' has no exact typed Sage specialization declaration; "
                     "translated="
                  << translated_member->class_name() << std::endl;
        ROSE_ABORT();
      }
      template_target = resolved_variable;
    }
    ASSERT_not_null(template_target);
    sg_member_expr = buildNonrealRefExpFromNestedNameSpecifier(
        qualifier, reference_scope, SgName(template_name),
        member_expr->hasTemplateKeyword(), &semantic_template_arguments,
        &template_arguments, template_target);
    SgNonrealRefExp *nonreal_reference = isSgNonrealRefExp(sg_member_expr);
    ASSERT_not_null(nonreal_reference);
    nonreal_reference->set_explicit_template_argument_list(true);
    nonreal_reference->set_resolved_function_declaration(
        isSgFunctionDeclaration(template_target));
    nonreal_reference->set_resolved_variable_declaration(
        isSgTemplateVariableDeclaration(template_target));
    if (nonreal_reference->get_resolved_function_declaration() != nullptr) {
      SageInterface::requireResolvedFunctionTemplateReference(
          nonreal_reference, "Clang MemberExpr producer");
    }
    if (nonreal_reference->get_resolved_variable_declaration() != nullptr) {
      nonreal_reference->set_is_non_odr_use(member_expr->isNonOdrUse() !=
                                            clang::NOUR_None);
      SageInterface::requireResolvedVariableTemplateReference(
          nonreal_reference, "Clang MemberExpr producer");
      ASSERT_not_null(nonreal_reference->get_type());
    }
  } else {
    clang::FunctionDecl *deduced_member_function =
        llvm::dyn_cast<clang::FunctionDecl>(raw_member_decl);
    clang::FunctionTemplateDecl *deduced_member_source_template =
        deduced_member_function != nullptr
            ? deduced_member_function->getPrimaryTemplate()
            : nullptr;

    if (deduced_member_source_template != nullptr) {
      SgNode *source_node = nullptr;
      auto source_mapping =
          p_decl_translation_map.find(deduced_member_source_template);
      if (source_mapping != p_decl_translation_map.end()) {
        source_node = source_mapping->second;
      } else {
        source_node = TraverseOnDemand(deduced_member_source_template);
      }
      if (SgTemplateInstantiationDirectiveStatement *directive =
              isSgTemplateInstantiationDirectiveStatement(source_node)) {
        source_node = directive->get_declaration();
      }
      SgTemplateMemberFunctionDeclaration *source_member =
          isSgTemplateMemberFunctionDeclaration(source_node);
      SgTemplateMemberFunctionDeclaration *canonical_source =
          source_member != nullptr
              ? isSgTemplateMemberFunctionDeclaration(
                    source_member->get_firstNondefiningDeclaration())
              : nullptr;
      SgTemplateMemberFunctionSymbol *source_symbol =
          canonical_source != nullptr
              ? isSgTemplateMemberFunctionSymbol(
                    canonical_source->get_symbol_from_symbol_table())
              : nullptr;
      if (source_member == nullptr || canonical_source == nullptr ||
          source_symbol == nullptr ||
          source_symbol->get_declaration() != canonical_source) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[deduced-template-member-access]: "
                "Clang member=%p source-template=%p translated=%p/%s "
                "source=%p/%s canonical=%p symbol=%p has no exact "
                "source-template identity\n",
                static_cast<void *>(deduced_member_function),
                static_cast<void *>(deduced_member_source_template),
                static_cast<void *>(translated_member),
                translated_member != nullptr
                    ? translated_member->class_name().c_str()
                    : "<null>",
                static_cast<void *>(source_member),
                source_member != nullptr ? source_member->class_name().c_str()
                                         : "<null>",
                static_cast<void *>(canonical_source),
                static_cast<void *>(source_symbol));
        ROSE_ABORT();
      }
      SgTemplateMemberFunctionRefExp *reference =
          SageBuilder::buildTemplateMemberFunctionRefExp_nfi(
              source_symbol, virtual_dispatch, member_has_qualifier);

      SgNode *semantic_node =
          requireSelectedFunctionDeclaration(deduced_member_function);
      SgTemplateInstantiationMemberFunctionDecl *semantic_member =
          isSgTemplateInstantiationMemberFunctionDecl(semantic_node);
      if (semantic_member != nullptr) {
        SgTemplateMemberFunctionDeclaration *semantic_source =
            semantic_member->get_templateDeclaration();
        SgTemplateMemberFunctionDeclaration *canonical_semantic_source =
            semantic_source != nullptr
                ? isSgTemplateMemberFunctionDeclaration(
                      semantic_source->get_firstNondefiningDeclaration())
                : nullptr;
        if (canonical_semantic_source != canonical_source ||
            semantic_member->get_type() == nullptr) {
          fprintf(stderr,
                  "REX_FRONTEND_INVARIANT[deduced-template-member-access]: "
                  "Clang member=%p source-template=%p semantic=%p source="
                  "%p canonical=%p/%p has no exact concrete semantic "
                  "identity\n",
                  static_cast<void *>(deduced_member_function),
                  static_cast<void *>(deduced_member_source_template),
                  static_cast<void *>(semantic_member),
                  static_cast<void *>(semantic_source),
                  static_cast<void *>(canonical_source),
                  static_cast<void *>(canonical_semantic_source));
          ROSE_ABORT();
        }
        reference->set_semantic_member_function_declaration(semantic_member);
      } else {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[deduced-template-member-access]: "
                "selected Clang member=%p published %p/%s without an exact "
                "typed member specialization\n",
                static_cast<void *>(deduced_member_function),
                static_cast<void *>(semantic_node),
                semantic_node->class_name().c_str());
        ROSE_ABORT();
      }

      SgMemberFunctionDeclaration *associated_member =
          reference->getAssociatedMemberFunctionDeclaration();
      if (associated_member == nullptr || reference->get_type() == nullptr ||
          (associated_member != semantic_member ||
           reference->get_type() != semantic_member->get_type())) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[deduced-template-member-access]: "
                "reference=%p did not retain its exact concrete "
                "member=%p/%p and type=%p\n",
                static_cast<void *>(reference),
                static_cast<void *>(semantic_member),
                static_cast<void *>(associated_member),
                static_cast<void *>(reference->get_type()));
        ROSE_ABORT();
      }
      sg_member_expr = reference;
    } else {
      auto symbol_from_translated_node = [](SgNode *translated) -> SgSymbol * {
        if (SgVariableDeclaration *variable_decl =
                isSgVariableDeclaration(translated)) {
          if (variable_decl->get_variables().size() != 1 ||
              variable_decl->get_variables().front() == nullptr) {
            std::cerr << "REX_FRONTEND_INVARIANT[member-reference]: translated "
                         "variable declaration does not own exactly one "
                         "initialized name"
                      << std::endl;
            ROSE_ABORT();
          }
          SgInitializedName *name = variable_decl->get_variables().front();
          SgScopeStatement *semantic_scope = name->get_scope();
          if (semantic_scope == nullptr) {
            std::cerr << "REX_FRONTEND_INVARIANT[member-reference]: translated "
                         "variable redeclaration has no semantic symbol scope"
                      << std::endl;
            ROSE_ABORT();
          }
          SgSymbol *family_symbol = nullptr;
          std::unordered_set<SgInitializedName *> seen;
          for (SgInitializedName *candidate = name; candidate != nullptr;
               candidate = candidate->get_prev_decl_item()) {
            if (!seen.insert(candidate).second ||
                candidate->get_name() != name->get_name() ||
                candidate->get_scope() != semantic_scope) {
              std::cerr
                  << "REX_FRONTEND_INVARIANT[member-reference]: translated "
                     "variable has a cyclic or cross-scope redeclaration family"
                  << std::endl;
              ROSE_ABORT();
            }
            SgSymbol *candidate_symbol =
                semantic_scope->find_symbol_from_declaration(candidate);
            if (candidate_symbol == nullptr) {
              continue;
            }
            if (isSgVariableSymbol(candidate_symbol) == nullptr &&
                isSgTemplateVariableSymbol(candidate_symbol) == nullptr) {
              std::cerr << "REX_FRONTEND_INVARIANT[member-reference]: variable "
                           "redeclaration family owns a non-variable symbol"
                        << std::endl;
              ROSE_ABORT();
            }
            if (family_symbol != nullptr && family_symbol != candidate_symbol) {
              std::cerr << "REX_FRONTEND_INVARIANT[member-reference]: variable "
                           "redeclaration family owns conflicting symbols"
                        << std::endl;
              ROSE_ABORT();
            }
            family_symbol = candidate_symbol;
          }
          return family_symbol;
        }
        if (SgInitializedName *name = isSgInitializedName(translated)) {
          if (SgSymbol *symbol = name->get_symbol_from_symbol_table()) {
            return symbol;
          }
          return name->get_scope() != nullptr
                     ? name->get_scope()->find_symbol_from_declaration(name)
                     : nullptr;
        }
        if (SgDeclarationStatement *declaration =
                isSgDeclarationStatement(translated)) {
          if (SgSymbol *symbol = declaration->get_symbol_from_symbol_table()) {
            return symbol;
          }
          if (SgDeclarationStatement *first_nondefining =
                  declaration->get_firstNondefiningDeclaration()) {
            return first_nondefining->get_symbol_from_symbol_table();
          }
        }
        return nullptr;
      };

      SgSymbol *symbol = symbol_from_translated_node(translated_member);
      if (symbol == nullptr) {
        std::cerr
            << "REX_FRONTEND_INVARIANT[member-reference]: exact translated "
            << raw_member_decl->getDeclKindName() << " '"
            << raw_member_decl->getQualifiedNameAsString()
            << "' has no symbol; translated=" << translated_member->class_name()
            << std::endl;
        ROSE_ABORT();
      }

      if (auto *alias = isSgAliasSymbol(symbol))
        symbol = alias->get_base();

      if (SgVariableSymbol *variable_symbol = isSgVariableSymbol(symbol)) {
        sg_member_expr = member_uses_synthesized_provenance
                             ? SageBuilder::buildVarRefExp_nfi(variable_symbol)
                             : SageBuilder::buildVarRefExp(variable_symbol);
      } else if (SgTemplateMemberFunctionSymbol *template_member_symbol =
                     isSgTemplateMemberFunctionSymbol(symbol)) {
        sg_member_expr = SageBuilder::buildTemplateMemberFunctionRefExp_nfi(
            template_member_symbol, virtual_dispatch, member_has_qualifier);
      } else if (SgMemberFunctionSymbol *member_symbol =
                     isSgMemberFunctionSymbol(symbol)) {
        sg_member_expr = SageBuilder::buildMemberFunctionRefExp_nfi(
            member_symbol, virtual_dispatch, member_has_qualifier);
      } else if (SgTemplateFunctionSymbol *template_function_symbol =
                     isSgTemplateFunctionSymbol(symbol)) {
        sg_member_expr = SageBuilder::buildTemplateFunctionRefExp_nfi(
            template_function_symbol);
      } else if (SgFunctionSymbol *function_symbol =
                     isSgFunctionSymbol(symbol)) {
        sg_member_expr = buildFunctionReference(
            function_symbol, member_expr->getMemberDecl()->getType(),
            member_uses_synthesized_provenance);
      } else if (SgEnumFieldSymbol *enum_symbol = isSgEnumFieldSymbol(symbol)) {
        SgInitializedName *initialized_name = enum_symbol->get_declaration();
        SgEnumDeclaration *enum_decl =
            initialized_name != nullptr
                ? isSgEnumDeclaration(initialized_name->get_declptr())
                : nullptr;
        clang::EnumConstantDecl *enum_constant =
            llvm::dyn_cast<clang::EnumConstantDecl>(raw_member_decl);
        if (enum_decl == nullptr || enum_constant == nullptr) {
          std::cerr << "REX_FRONTEND_INVARIANT[enum-member-reference]: exact "
                       "enum member lacks its Clang or Sage declaration"
                    << std::endl;
          ROSE_ABORT();
        }

        ROSE_ASSERT(initialized_name->get_enum_constant_value().has_value());
        sg_member_expr = SageBuilder::buildEnumVal_nfi(
            *initialized_name->get_enum_constant_value(), enum_decl,
            initialized_name->get_name());
      } else {
        std::cerr
            << "REX_FRONTEND_INVARIANT[member-reference]: exact translated "
               "symbol has unsupported type "
            << symbol->class_name() << std::endl;
        ROSE_ABORT();
      }
    }
  }
  ROSE_ASSERT(sg_member_expr != nullptr);
  if (auto *reference = isSgVarRefExp(sg_member_expr))
    reference->set_is_non_odr_use(member_expr->isNonOdrUse() !=
                                  clang::NOUR_None);
  buildConversionFunctionId(sg_member_expr, member_expr->getMemberNameInfo(),
                            !synthesized_member_callee);
  if (synthesized_member_callee) {
    publishCanonicalSemanticExpressionSourceProvenance(
        sg_member_expr, "VisitMemberExpr:synthesized-callee-reference");
  } else {
    applySourceRange(sg_member_expr,
                     member_expr->getMemberNameInfo().getSourceRange());
  }

  if (member_has_qualifier && !member_uses_synthesized_provenance) {
    clang::NestedNameSpecifierLoc qualifier_loc =
        member_expr->getQualifierLoc();
    const clang::NestedNameSpecifier qualifier =
        qualifier_loc.getNestedNameSpecifier();
    ROSE_ASSERT(qualifier);
    attachExplicitQualifierFromNestedName(sg_member_expr, qualifier,
                                          &qualifier_loc, p_compiler_instance);
  }

  bool use_arrow = member_expr->isArrow();
  if (stripped_anonymous_member_operator_is_arrow.has_value()) {
    // Clang lowers member access through anonymous aggregates as a chain like
    // `this->anon.field`, where the promoted field appears as a dot access on
    // the synthetic anonymous member.  Once the hidden hops are removed, the
    // innermost hidden hop owns the exact effective source operator.
    use_arrow = *stripped_anonymous_member_operator_is_arrow;
  }
  if (implicit_access && !materialize_implicit_base) {
    *node = sg_member_expr;
  } else if (use_arrow) {
    SgArrowExp *arrow =
        SageBuilder::buildArrowExp_nfi(base, sg_member_expr, result_type);
    arrow->set_emission_role(implicit_access && materialize_implicit_base
                                 ? SgArrowExp::e_implicit_object_access
                                 : SgArrowExp::e_emit_arrow_operator);
    *node = arrow;
  } else {
    *node = SageBuilder::buildDotExp_nfi(base, sg_member_expr, result_type);
  }

  if (synthesized_member_callee && *node != sg_member_expr) {
    publishCanonicalSemanticExpressionSourceProvenance(
        isSgExpression(*node),
        "VisitMemberExpr:synthesized-callee-access-expression");
  }

  return VisitExpr(member_expr, node);
}

bool ClangToSageTranslator::VisitMSPropertyRefExpr(
    clang::MSPropertyRefExpr *ms_property_expr, SgNode **node) {
  if (ms_property_expr == nullptr || node == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[unsupported-ms-property]: Clang "
                 "property-reference translation has no exact expression or "
                 "result slot"
              << std::endl;
    ROSE_ABORT();
  }
  std::cerr
      << "REX_FRONTEND_INVARIANT[unsupported-ms-property]: source-written "
         "Microsoft property reference has no lossless typed Sage "
         "representation"
      << std::endl;
  ROSE_ABORT();
}

bool ClangToSageTranslator::VisitMSPropertySubscriptExpr(
    clang::MSPropertySubscriptExpr *ms_property_subscript_expr, SgNode **node) {
  if (ms_property_subscript_expr == nullptr || node == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[unsupported-ms-property]: Clang "
                 "property-subscript translation has no exact expression or "
                 "result slot"
              << std::endl;
    ROSE_ABORT();
  }
  std::cerr
      << "REX_FRONTEND_INVARIANT[unsupported-ms-property]: source-written "
         "Microsoft property subscript has no lossless typed Sage "
         "representation"
      << std::endl;
  ROSE_ABORT();
}

bool ClangToSageTranslator::VisitNoInitExpr(clang::NoInitExpr *no_init_expr,
                                            SgNode **node) {
  (void)no_init_expr;
  (void)node;
  std::cerr << "REX_FRONTEND_UNSUPPORTED[no-init-expression]: no exact Sage "
               "representation is implemented"
            << std::endl;
  ROSE_ABORT();
}

bool ClangToSageTranslator::VisitOffsetOfExpr(
    clang::OffsetOfExpr *offset_of_expr, SgNode **node) {
  SgExpressionPtrList nodePtrList;

  clang::TypeSourceInfo *type_info = offset_of_expr->getTypeSourceInfo();
  if (type_info == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[offsetof-type]: builtin has no "
                 "exact written TypeSourceInfo"
              << std::endl;
    ROSE_ABORT();
  }
  clang::TypeLoc type_loc = type_info->getTypeLoc();

  SgType *type = buildTypeFromTypeLoc(type_loc);
  if (type == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[offsetof-type]: written type did not "
                 "translate from its exact TypeLoc"
              << std::endl;
    ROSE_ABORT();
  }

  SgTypeExpression *type_operand = SageBuilder::buildTypeExpression_nfi(type);
  applySourceRange(type_operand, type_loc.getSourceRange());
  nodePtrList.push_back(type_operand);

  SgExpression *topExp = nullptr;
  clang::SourceLocation designator_begin;
  clang::QualType component_type = type_info->getType();

  auto build_field_reference = [&](clang::FieldDecl *field) -> SgExpression * {
    if (field == nullptr) {
      std::cerr << "REX_FRONTEND_INVARIANT[offsetof-field]: null Clang field"
                << std::endl;
      ROSE_ABORT();
    }
    SgNode *field_node = Traverse(field);
    SgInitializedName *initialized_name = nullptr;
    if (SgVariableDeclaration *declaration =
            isSgVariableDeclaration(field_node)) {
      initialized_name = SageInterface::getFirstInitializedName(declaration);
    } else {
      initialized_name = isSgInitializedName(field_node);
    }
    if (initialized_name == nullptr ||
        SageInterface::containsUnknownType(initialized_name->get_type())) {
      std::cerr << "REX_FRONTEND_INVARIANT[offsetof-field]: field '"
                << field->getNameAsString() << "' has no typed Sage declaration"
                << std::endl;
      ROSE_ABORT();
    }
    SgVariableSymbol *symbol = isSgVariableSymbol(
        initialized_name->search_for_symbol_from_symbol_table());
    if (symbol == nullptr) {
      SgScopeStatement *scope = initialized_name->get_scope();
      if (scope == nullptr) {
        std::cerr << "REX_FRONTEND_INVARIANT[offsetof-field]: field '"
                  << field->getNameAsString() << "' has no Sage scope"
                  << std::endl;
        ROSE_ABORT();
      }
      symbol = new SgVariableSymbol(initialized_name);
      attachSymbolToExactScope(symbol, scope);
    }
    return SageBuilder::buildVarRefExp(symbol);
  };

  auto append_designator = [&](SgExpression *component,
                               clang::SourceRange component_range) {
    ASSERT_not_null(component);
    if (!component_range.isValid()) {
      std::cerr << "REX_FRONTEND_INVARIANT[offsetof-designator-source]: "
                   "written designator component has no exact source range"
                << std::endl;
      ROSE_ABORT();
    }
    if (topExp == nullptr) {
      designator_begin = component_range.getBegin();
      applySourceRange(component, component_range);
      topExp = component;
    } else {
      SgType *component_type = component->get_type();
      if (SageInterface::containsUnknownType(component_type)) {
        std::cerr << "REX_FRONTEND_INVARIANT[offsetof-component-type]: "
                     "field designator has no exact semantic type"
                  << std::endl;
        ROSE_ABORT();
      }
      applySourceRange(component, component_range);
      topExp = SageBuilder::buildDotExp(topExp, component, component_type);
      applySourceRange(topExp, clang::SourceRange(designator_begin,
                                                  component_range.getEnd()));
    }
  };

  for (unsigned i = 0, n = offset_of_expr->getNumComponents(); i < n; ++i) {
    clang::OffsetOfNode ON = offset_of_expr->getComponent(i);

    switch (ON.getKind()) {
    case clang::OffsetOfNode::Array: {
      if (topExp == nullptr) {
        std::cerr << "REX_FRONTEND_INVARIANT[offsetof-designator]: array "
                     "component has no preceding field designator"
                  << std::endl;
        ROSE_ABORT();
      }
      SgExpression *arrayIdx = isSgExpression(
          Traverse(offset_of_expr->getIndexExpr(ON.getArrayExprIndex())));
      if (arrayIdx == nullptr) {
        std::cerr << "REX_FRONTEND_INVARIANT[offsetof-designator]: array "
                     "component has no exact typed index expression"
                  << std::endl;
        ROSE_ABORT();
      }
      SgType *element_type = SageInterface::getElementType(topExp->get_type());
      if (SageInterface::containsUnknownType(element_type)) {
        std::cerr << "REX_FRONTEND_INVARIANT[offsetof-component-type]: "
                     "array designator has no exact element type"
                  << std::endl;
        ROSE_ABORT();
      }
      const auto &context = p_compiler_instance->getASTContext();
      SubscriptLayout layout;
      if (const auto *array = context.getAsArrayType(component_type)) {
        const auto base_type = component_type;
        component_type = array->getElementType();
        layout = measureSubscriptLayout(context, component_type, base_type);
      } else {
        ROSE_ASSERT(component_type->isDependentType());
      }
      SgPntrArrRefExp *pntrArrRefExp = SageBuilder::buildPntrArrRefExp_nfi(
          topExp, arrayIdx, element_type, layout.result_storage_size_bytes,
          {layout.stride_bits});
      clang::SourceRange component_range = ON.getSourceRange();
      if (!component_range.isValid() || designator_begin.isInvalid()) {
        std::cerr << "REX_FRONTEND_INVARIANT[offsetof-designator-source]: "
                     "array component has no exact composite source range"
                  << std::endl;
        ROSE_ABORT();
      }
      applySourceRange(
          pntrArrRefExp,
          clang::SourceRange(designator_begin, component_range.getEnd()));
      topExp = isSgExpression(pntrArrRefExp);
      break;
    }
    case clang::OffsetOfNode::Field: {
      component_type = ON.getField()->getType();
      append_designator(build_field_reference(ON.getField()),
                        ON.getSourceRange());
      break;
    }
    case clang::OffsetOfNode::Identifier: {
      SgName fieldName(ON.getFieldName()->getName().str());
      SgScopeStatement *scope = SageBuilder::topScopeStack();
      ASSERT_not_null(scope);
      SgNonrealRefExp *field_ref = buildNonrealRefExpFromNestedNameSpecifier(
          clang::NestedNameSpecifier(), scope, fieldName, false, nullptr,
          nullptr);
      append_designator(field_ref, ON.getSourceRange());
      break;
    }
    case clang::OffsetOfNode::Base:
      // Clang inserts base-class components to describe semantic lookup. They
      // have no source spelling in the offsetof member designator.
      component_type = ON.getBase()->getType();
      break;
    }
  }
  if (type == nullptr || topExp == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[offsetof-designator]: builtin has "
                 "no exact queried type or source member designator"
              << std::endl;
    ROSE_ABORT();
  }
  nodePtrList.push_back(topExp);

  SgType *result_type = buildTypeFromQualifiedType(offset_of_expr->getType());

  SgTypeTraitBuiltinOperator *typeTraitBuiltinOperator =
      SageBuilder::buildTypeTraitBuiltinOperator_nfi(
          "__builtin_offsetof", SgTypeTraitBuiltinOperator::e_offsetof_builtin,
          result_type, nodePtrList);

  *node = typeTraitBuiltinOperator;

  return VisitExpr(offset_of_expr, node);
}

bool ClangToSageTranslator::VisitOpaqueValueExpr(
    clang::OpaqueValueExpr *opaque_value_expr, SgNode **node) {
  for (auto binding = p_array_init_binding_stack.rbegin();
       binding != p_array_init_binding_stack.rend(); ++binding) {
    if (binding->first != opaque_value_expr)
      continue;
    SgCxxArrayInitBinding *target = binding->second;
    auto *reference = new SgCxxArrayInitReference(
        target, SgCxxArrayInitReference::e_common_value,
        buildTypeFromQualifiedType(opaque_value_expr->getType()),
        translateClangValueCategory(opaque_value_expr->getValueKind()));
    publishSemanticExpressionSourceProvenance(
        reference, opaque_value_expr->getSourceRange(), "array-init-common");
    reference->validate();
    *node = reference;
    return VisitExpr(opaque_value_expr, node);
  }

  for (auto frame = p_pseudo_object_construction_frames.rbegin();
       frame != p_pseudo_object_construction_frames.rend(); ++frame) {
    const auto found = frame->bindings.find(opaque_value_expr);
    if (found == frame->bindings.end())
      continue;
    if (p_pseudo_object_construction_frames.back().written) {
      clang::Expr *source = opaque_value_expr->getSourceExpr();
      ROSE_ASSERT(source != nullptr);
      *node = isSgExpression(Traverse(source));
      ROSE_ASSERT(*node != nullptr);
      return true;
    }
    auto *binding = found->second;
    for (auto captured = p_pseudo_object_construction_frames.rbegin();
         captured != frame; ++captured) {
      auto &captures = captured->captures;
      if (std::find(captures.begin(), captures.end(), binding) ==
          captures.end())
        captures.push_back(binding);
    }
    auto *reference = new SgOpaqueValueReference(binding);
    publishCanonicalSemanticExpressionSourceProvenance(
        reference, "pseudo-object-bound-value");
    *node = reference;
    return VisitExpr(opaque_value_expr, node);
  }

  // Expanding a nonunique source would repeat its evaluation.
  if (!opaque_value_expr->isUnique())
    failExactExpressionTranslation("opaque-value-binding", opaque_value_expr);

  clang::Expr *source_expr = opaque_value_expr->getSourceExpr();
  if (source_expr == nullptr)
    failExactExpressionTranslation("opaque-value-source", opaque_value_expr);
  if (!opaque_value_expr->getSourceRange().isValid() ||
      !p_compiler_instance->getASTContext().hasSameType(
          opaque_value_expr->getType(), source_expr->getType())) {
    std::cerr << "REX_FRONTEND_INVARIANT[opaque-value-source]: Clang "
                 "opaque value is not source- and type-transparent to its "
                 "source expression"
              << std::endl;
    ROSE_ABORT();
  }
  SgExpression *source = isSgExpression(Traverse(source_expr));
  ROSE_ASSERT(source != nullptr);
  *node = prepareExpressionForAttachment(source);

  return VisitExpr(opaque_value_expr, node);
}

bool ClangToSageTranslator::VisitUnresolvedLookupExpr(
    clang::UnresolvedLookupExpr *unresolved_lookup_expr, SgNode **node) {
  ASSERT_not_null(unresolved_lookup_expr);
  ASSERT_not_null(p_compiler_instance);
  auto should_materialize_overload_candidates = [&]() -> bool {
    clang::SourceManager &sm = p_compiler_instance->getSourceManager();
    clang::SourceLocation loc = unresolved_lookup_expr->getExprLoc();
    if (loc.isMacroID()) {
      clang::SourceLocation expansion_loc = sm.getExpansionLoc(loc);
      if (expansion_loc.isValid() && !sm.isInSystemHeader(expansion_loc) &&
          !sm.isWrittenInBuiltinFile(expansion_loc)) {
        return true;
      }
      loc = sm.getSpellingLoc(loc);
    }
    if (!loc.isValid()) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[unresolved-lookup-source]: "
              "expression=%p has no valid source location\n",
              static_cast<void *>(unresolved_lookup_expr));
      ROSE_ABORT();
    }

    return !sm.isInSystemHeader(loc) && !sm.isWrittenInBuiltinFile(loc);
  };
  const bool materialize_overload_candidates =
      should_materialize_overload_candidates();

  // UnresolvedLookupExpr represents a reference to a name that couldn't be
  // resolved during parsing (e.g., template-dependent function names like
  // `foo(t)` that will be resolved during instantiation/ADL).

  std::string function_name = unresolved_lookup_expr->getName().getAsString();

  SgScopeStatement *current_scope = SageBuilder::topScopeStack();
  ROSE_ASSERT(current_scope != nullptr);

  clang::NestedNameSpecifierLoc qualifier_loc =
      unresolved_lookup_expr->getQualifierLoc();
  const clang::NestedNameSpecifierLoc *qualifier_loc_ptr =
      qualifier_loc.getNestedNameSpecifier() ? &qualifier_loc : nullptr;

  clang::NestedNameSpecifier lookup_qualifier =
      unresolved_lookup_expr->getQualifier();

  auto build_single_overload_function_ref = [&]() -> SgExpression * {
    if (!materialize_overload_candidates) {
      return nullptr;
    }

    // Preserve explicit qualification (e.g., std::distance) by keeping the
    // call target in nonreal form with qualifier metadata.
    if (lookup_qualifier) {
      return nullptr;
    }

    // An explicit template-id must retain its source spelling. Collapsing it
    // to a plain template symbol loses the written `<...>` argument list.
    if (unresolved_lookup_expr->hasExplicitTemplateArgs()) {
      return nullptr;
    }

    // Keep unresolved ADL/dependent lookups in nonreal form. Even when Clang
    // exposes a single ordinary-lookup candidate, ADL or template-dependent
    // context can still change the selected callee at instantiation time.
    if (unresolved_lookup_expr->requiresADL() ||
        unresolved_lookup_expr->isTypeDependent() ||
        unresolved_lookup_expr->isValueDependent()) {
      return nullptr;
    }

    if (unresolved_lookup_expr->getName().getCXXOverloadedOperator() !=
        clang::OO_None) {
      return nullptr;
    }

    clang::NamedDecl *candidate_decl = nullptr;
    clang::Decl *candidate_canon = nullptr;

    for (auto it = unresolved_lookup_expr->decls_begin();
         it != unresolved_lookup_expr->decls_end(); ++it) {
      clang::NamedDecl *named_decl =
          llvm::dyn_cast_or_null<clang::NamedDecl>(it.getDecl());
      if (named_decl == nullptr) {
        continue;
      }
      if (clang::UsingShadowDecl *shadow =
              llvm::dyn_cast<clang::UsingShadowDecl>(named_decl)) {
        named_decl =
            llvm::dyn_cast_or_null<clang::NamedDecl>(shadow->getTargetDecl());
      }
      if (named_decl == nullptr) {
        continue;
      }
      clang::Decl *canon_decl = named_decl->getCanonicalDecl();
      if (candidate_canon == nullptr) {
        candidate_canon = canon_decl;
        candidate_decl = named_decl;
        continue;
      }
      if (canon_decl != candidate_canon) {
        return nullptr;
      }
    }

    if (candidate_decl == nullptr) {
      return nullptr;
    }

    SgScopeStatement *current_class_like_scope = SageBuilder::topScopeStack();
    auto member_ref_needs_qualifier =
        [&](clang::CXXMethodDecl *method_decl) -> bool {
      if (method_decl == nullptr || !method_decl->isStatic()) {
        return false;
      }

      SgScopeStatement *member_scope =
          resolveScopeFromDeclContext(method_decl->getParent());
      return member_scope != nullptr &&
             member_scope != current_class_like_scope;
    };

    if (clang::FunctionTemplateDecl *tmpl_decl =
            llvm::dyn_cast<clang::FunctionTemplateDecl>(candidate_decl)) {
      clang::CXXMethodDecl *method_decl =
          llvm::dyn_cast<clang::CXXMethodDecl>(tmpl_decl->getTemplatedDecl());
      SgNode *tmpl_node = nullptr;
      auto it_decl = p_decl_translation_map.find(tmpl_decl);
      if (it_decl != p_decl_translation_map.end()) {
        tmpl_node = it_decl->second;
      } else {
        tmpl_node = TraverseOnDemand(tmpl_decl);
      }
      if (SgTemplateMemberFunctionDeclaration *sg_tmpl_member_decl =
              isSgTemplateMemberFunctionDeclaration(tmpl_node)) {
        if (sg_tmpl_member_decl->get_symbol_from_symbol_table() == nullptr) {
          registerDeclarationSymbol(sg_tmpl_member_decl);
        }
        if (SgTemplateMemberFunctionSymbol *sym =
                isSgTemplateMemberFunctionSymbol(
                    sg_tmpl_member_decl->get_symbol_from_symbol_table())) {
          return SageBuilder::buildTemplateMemberFunctionRefExp_nfi(
              sym, false, member_ref_needs_qualifier(method_decl));
        }
      } else if (SgTemplateFunctionDeclaration *sg_tmpl_decl =
                     isSgTemplateFunctionDeclaration(tmpl_node)) {
        if (sg_tmpl_decl->get_symbol_from_symbol_table() == nullptr) {
          registerDeclarationSymbol(sg_tmpl_decl);
        }
        if (SgTemplateFunctionSymbol *sym = isSgTemplateFunctionSymbol(
                sg_tmpl_decl->get_symbol_from_symbol_table())) {
          return SageBuilder::buildTemplateFunctionRefExp_nfi(sym);
        }
      }
      return nullptr;
    }

    if (clang::FunctionDecl *func_decl =
            llvm::dyn_cast<clang::FunctionDecl>(candidate_decl)) {
      SgNode *func_node = nullptr;
      auto hidden_callable = p_hidden_friend_callable_decl_map.find(func_decl);
      if (hidden_callable != p_hidden_friend_callable_decl_map.end()) {
        func_node = hidden_callable->second;
      } else {
        auto it_decl = p_decl_translation_map.find(func_decl);
        if (it_decl != p_decl_translation_map.end()) {
          func_node = it_decl->second;
        } else {
          func_node = TraverseOnDemand(func_decl);
          hidden_callable = p_hidden_friend_callable_decl_map.find(func_decl);
          if (hidden_callable != p_hidden_friend_callable_decl_map.end()) {
            func_node = hidden_callable->second;
          }
        }
      }
      if (SgMemberFunctionDeclaration *sg_member_decl =
              isSgMemberFunctionDeclaration(func_node)) {
        if (sg_member_decl->get_symbol_from_symbol_table() == nullptr) {
          registerDeclarationSymbol(sg_member_decl);
        }
        if (SgMemberFunctionSymbol *sym = isSgMemberFunctionSymbol(
                sg_member_decl->get_symbol_from_symbol_table())) {
          return SageBuilder::buildMemberFunctionRefExp_nfi(
              sym, false,
              member_ref_needs_qualifier(
                  llvm::dyn_cast<clang::CXXMethodDecl>(func_decl)));
        }
      } else if (SgFunctionDeclaration *sg_func_decl =
                     isSgFunctionDeclaration(func_node)) {
        if (sg_func_decl->get_symbol_from_symbol_table() == nullptr) {
          registerDeclarationSymbol(sg_func_decl);
        }
        if (SgFunctionSymbol *sym = isSgFunctionSymbol(
                sg_func_decl->get_symbol_from_symbol_table())) {
          return buildFunctionReference(sym, func_decl->getType(), false);
        }
      }
    }

    return nullptr;
  };

  auto resolve_member_lookup_scope = [&]() -> SgScopeStatement * {
    const clang::CXXRecordDecl *member_record = nullptr;

    for (auto it = unresolved_lookup_expr->decls_begin();
         it != unresolved_lookup_expr->decls_end(); ++it) {
      clang::NamedDecl *named_decl =
          llvm::dyn_cast_or_null<clang::NamedDecl>(it.getDecl());
      if (named_decl == nullptr) {
        continue;
      }
      if (clang::UsingShadowDecl *shadow =
              llvm::dyn_cast<clang::UsingShadowDecl>(named_decl)) {
        named_decl =
            llvm::dyn_cast_or_null<clang::NamedDecl>(shadow->getTargetDecl());
      }
      if (named_decl == nullptr) {
        continue;
      }

      const clang::CXXMethodDecl *method_decl = nullptr;
      if (const clang::FunctionTemplateDecl *tmpl_decl =
              llvm::dyn_cast<clang::FunctionTemplateDecl>(named_decl)) {
        method_decl = llvm::dyn_cast_or_null<clang::CXXMethodDecl>(
            tmpl_decl->getTemplatedDecl());
      } else {
        method_decl = llvm::dyn_cast<clang::CXXMethodDecl>(named_decl);
      }
      if (method_decl == nullptr) {
        return nullptr;
      }
      const clang::CXXRecordDecl *candidate_record = method_decl->getParent();
      if (candidate_record == nullptr) {
        return nullptr;
      }

      if (const clang::CXXRecordDecl *definition =
              candidate_record->getDefinition()) {
        candidate_record = definition;
      }
      candidate_record = candidate_record->getCanonicalDecl();

      if (member_record == nullptr) {
        member_record = candidate_record;
      } else if (member_record != candidate_record) {
        return nullptr;
      }
    }

    if (member_record == nullptr) {
      return nullptr;
    }

    return resolveScopeFromDeclContext(
        const_cast<clang::CXXRecordDecl *>(member_record));
  };

  if (*node == nullptr) {
    if (SgExpression *func_ref = build_single_overload_function_ref()) {
      attachExplicitQualifierFromNestedName(
          func_ref, lookup_qualifier, qualifier_loc_ptr, p_compiler_instance);
      applySourceRange(func_ref, unresolved_lookup_expr->getSourceRange());
      *node = func_ref;
    }
  }

  if (*node != nullptr) {
    return VisitExpr(unresolved_lookup_expr, node);
  }

  SgTemplateArgumentPtrList template_args;
  const SgTemplateArgumentPtrList *template_args_ptr = nullptr;
  SgTemplateArgumentPtrList semantic_template_args;
  const SgTemplateArgumentPtrList *semantic_template_args_ptr = nullptr;
  if (unresolved_lookup_expr->hasExplicitTemplateArgs()) {
    clang::TemplateArgumentListInfo arg_info;
    unresolved_lookup_expr->copyTemplateArgumentsInto(arg_info);
    template_args = buildTemplateArguments(arg_info, true);
    template_args_ptr = &template_args;
    semantic_template_args = buildSemanticTemplateArguments(
        arg_info, true, "unresolved-lookup-template-id");
    semantic_template_args_ptr = &semantic_template_args;
  }

  SgScopeStatement *lookup_scope = current_scope;
  if (!lookup_qualifier) {
    if (SgScopeStatement *member_scope = resolve_member_lookup_scope()) {
      lookup_scope = member_scope;
    }
  }

  clang::VarTemplateDecl *variable_template = nullptr;
  bool has_non_variable_candidate = false;
  for (auto candidate = unresolved_lookup_expr->decls_begin();
       candidate != unresolved_lookup_expr->decls_end(); ++candidate) {
    clang::NamedDecl *declaration = candidate.getDecl();
    ROSE_ASSERT(declaration != nullptr);
    if (auto *shadow = llvm::dyn_cast<clang::UsingShadowDecl>(declaration))
      declaration = shadow->getTargetDecl();
    auto *variable = llvm::dyn_cast<clang::VarTemplateDecl>(declaration);
    if (variable == nullptr) {
      has_non_variable_candidate = true;
      continue;
    }
    auto *canonical = variable->getCanonicalDecl();
    ROSE_ASSERT(variable_template == nullptr || variable_template == canonical);
    variable_template = canonical;
  }
  SgTemplateVariableDeclaration *variable_identity = nullptr;
  if (variable_template != nullptr) {
    ROSE_ASSERT(!has_non_variable_candidate);
    ROSE_ASSERT(!unresolved_lookup_expr->requiresADL());
    variable_identity = isSgTemplateVariableDeclaration(
        lookupSgDeclarationForClangDecl(variable_template,
                                        /*allow_on_demand=*/true));
    ROSE_ASSERT(variable_identity != nullptr);
  }

  SgNonrealRefExp *reference = buildNonrealRefExpFromNestedNameSpecifier(
      lookup_qualifier, lookup_scope, SgName(function_name),
      unresolved_lookup_expr->hasTemplateKeyword(), semantic_template_args_ptr,
      template_args_ptr, variable_identity);
  buildConversionFunctionId(reference, unresolved_lookup_expr->getNameInfo());
  reference->set_explicit_template_argument_list(
      unresolved_lookup_expr->hasExplicitTemplateArgs());
  reference->set_semantic_role(variable_identity != nullptr
                                   ? SgNonrealRefExp::e_nonreal_reference
                                   : SgNonrealRefExp::e_dependent_callable);

  attachExplicitQualifierFromNestedName(reference, lookup_qualifier,
                                        qualifier_loc_ptr, p_compiler_instance);
  applySourceRange(reference, unresolved_lookup_expr->getSourceRange());
  *node = reference;

  return VisitExpr(unresolved_lookup_expr, node);
}

bool ClangToSageTranslator::VisitUnresolvedMemberExpr(
    clang::UnresolvedMemberExpr *unresolved_member_expr, SgNode **node) {
  const clang::QualType clang_result_type = unresolved_member_expr->getType();
  const clang::BuiltinType *builtin_result_type =
      llvm::dyn_cast_or_null<clang::BuiltinType>(
          clang_result_type.getTypePtrOrNull());
  const bool has_unresolved_callable_pseudo_type =
      builtin_result_type != nullptr &&
      (builtin_result_type->getKind() == clang::BuiltinType::Overload ||
       builtin_result_type->getKind() == clang::BuiltinType::BoundMember);
  SgType *result_type = has_unresolved_callable_pseudo_type
                            ? nullptr
                            : buildTypeFromQualifiedType(clang_result_type);
  if (!has_unresolved_callable_pseudo_type &&
      SageInterface::containsUnknownType(result_type)) {
    std::cerr << "REX_FRONTEND_INVARIANT[unresolved-member-result-type]: "
                 "Clang expression has no exact semantic result type"
              << std::endl;
    ROSE_ABORT();
  }

  std::string member_name =
      unresolved_member_expr->getMemberName().getAsString();

  SgScopeStatement *current_scope = SageBuilder::topScopeStack();
  ROSE_ASSERT(current_scope != nullptr);

  // Handle the base expression (the object/pointer being accessed)
  SgExpression *base_expr = nullptr;
  if (!unresolved_member_expr->isImplicitAccess()) {
    SgNode *tmp_base = Traverse(unresolved_member_expr->getBase());
    base_expr = isSgExpression(tmp_base);
    ROSE_ASSERT(base_expr != nullptr);
  }

  SgTemplateArgumentPtrList template_args;
  const SgTemplateArgumentPtrList *template_args_ptr = nullptr;
  SgTemplateArgumentPtrList semantic_template_args;
  const SgTemplateArgumentPtrList *semantic_template_args_ptr = nullptr;
  if (unresolved_member_expr->hasExplicitTemplateArgs()) {
    clang::TemplateArgumentListInfo arg_info;
    {
      unresolved_member_expr->copyTemplateArgumentsInto(arg_info);
    }
    template_args = buildTemplateArguments(arg_info, true);
    template_args_ptr = &template_args;
    semantic_template_args = buildSemanticTemplateArguments(
        arg_info, true, "unresolved-member-template-id");
    semantic_template_args_ptr = &semantic_template_args;
  }

  clang::NestedNameSpecifier lookup_qualifier =
      unresolved_member_expr->getQualifier();
  SgNonrealRefExp *member_ref = buildNonrealRefExpFromNestedNameSpecifier(
      lookup_qualifier, current_scope, SgName(member_name),
      unresolved_member_expr->hasTemplateKeyword(), semantic_template_args_ptr,
      template_args_ptr);
  ROSE_ASSERT(member_ref != nullptr);
  buildConversionFunctionId(member_ref,
                            unresolved_member_expr->getMemberNameInfo());
  if (has_unresolved_callable_pseudo_type) {
    member_ref->set_semantic_role(SgNonrealRefExp::e_dependent_callable);
    // Overload and BoundMember are Clang-only placeholder types, not C++
    // semantic types.  An unresolved Sage member is represented by its exact
    // nonreal declaration/type identity until overload resolution selects a
    // callable; publishing the placeholder as an ordinary type loses that
    // identity.
    result_type = member_ref->get_type();
    if (isSgNonrealType(result_type) == nullptr ||
        SageInterface::containsUnknownType(result_type)) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[unresolved-member-result-type]: "
              "member=%s Clang pseudo-kind=%u has no exact nonreal Sage "
              "identity\n",
              member_name.c_str(),
              static_cast<unsigned>(builtin_result_type->getKind()));
      ROSE_ABORT();
    }
  }
  clang::SourceRange member_name_range =
      unresolved_member_expr->getMemberNameInfo().getSourceRange();
  if (!member_name_range.isValid()) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[unresolved-member-range]: member=%s has "
            "no exact source range\n",
            member_name.c_str());
    ROSE_ABORT();
  }
  member_ref->set_explicit_template_argument_list(
      unresolved_member_expr->hasExplicitTemplateArgs());
  clang::NestedNameSpecifierLoc qualifier_loc =
      unresolved_member_expr->getQualifierLoc();
  const clang::NestedNameSpecifierLoc *qualifier_loc_ptr =
      qualifier_loc.getNestedNameSpecifier() ? &qualifier_loc : nullptr;
  attachExplicitQualifierFromNestedName(member_ref, lookup_qualifier,
                                        qualifier_loc_ptr, p_compiler_instance);
  if (!lookup_qualifier)
    setExplicitQualifierOnRef(member_ref, SourceQualification{});

  if (base_expr != nullptr) {
    // A dot/arrow node owns the complete member-access surface, so its child
    // reference owns only the terminal member name.  With implicit access the
    // reference itself owns the complete UnresolvedMemberExpr range and is
    // published exactly once below.
    applySourceRange(member_ref, member_name_range);
    if (unresolved_member_expr->isArrow()) {
      SgArrowExp *arrow =
          SageBuilder::buildArrowExp_nfi(base_expr, member_ref, result_type);
      arrow->set_emission_role(SgArrowExp::e_emit_arrow_operator);
      *node = arrow;
    } else {
      *node = SageBuilder::buildDotExp_nfi(base_expr, member_ref, result_type);
    }
  } else {
    *node = member_ref;
  }

  if (SgExpression *expr = isSgExpression(*node)) {
    applySourceRange(expr, unresolved_member_expr->getSourceRange());
  }

  return VisitExpr(unresolved_member_expr, node);
}

bool ClangToSageTranslator::VisitPackExpansionExpr(
    clang::PackExpansionExpr *pack_expansion_expr, SgNode **node) {
  clang::Expr *pattern = pack_expansion_expr->getPattern();
  if (pattern == nullptr) {
    failExactExpressionTranslation("pack-expansion-pattern",
                                   pack_expansion_expr);
  }
  SgNode *tmp_node = Traverse(pattern);
  SgExpression *pattern_expr = isSgExpression(tmp_node);
  if (pattern_expr == nullptr) {
    failExactExpressionTranslation("pack-expansion-pattern-translation",
                                   pack_expansion_expr);
  }
  ROSE_ASSERT(pack_expansion_expr->getType() == pattern->getType());
  SgType *result_type = pattern_expr->get_type();
  SgPackExpansionExpr *pack_expr =
      SageBuilder::buildPackExpansionExpr_nfi(pattern_expr, result_type);
  pack_expr->set_need_paren(false);
  applySourceRange(pack_expr, pack_expansion_expr->getSourceRange());
  *node = pack_expr;
  return VisitExpr(pack_expansion_expr, node);
}

bool ClangToSageTranslator::VisitParenExpr(clang::ParenExpr *paren_expr,
                                           SgNode **node) {
  SgNode *tmp_subexpr = Traverse(paren_expr->getSubExpr());
  SgExpression *subexpr = isSgExpression(tmp_subexpr);
  if (subexpr == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[parenthesized-expression]: operand "
                 "did not translate to an expression"
              << std::endl;
    ROSE_ABORT();
  }

  // Preserve explicitly written parentheses as expression metadata.  Sage
  // does not have a ParenExpr node, so contexts without an expression parent
  // (for example non-type template arguments) need this bit to recover source
  // grouping without inventing parentheses from operator class alone.
  subexpr->set_need_paren(true);

  *node = subexpr;

  return VisitExpr(paren_expr, node);
}

bool ClangToSageTranslator::VisitParenListExpr(
    clang::ParenListExpr *paran_list_expr, SgNode **node) {
  unsigned num_exprs = paran_list_expr->getNumExprs();

  if (num_exprs == 1) {
    *node = Traverse(paran_list_expr->getExpr(0));
    if (isSgExpression(*node) == nullptr) {
      std::cerr << "REX_FRONTEND_INVARIANT[parenthesized-expression-list]: "
                   "single element did not translate to an expression"
                << std::endl;
      ROSE_ABORT();
    }
  } else {
    SgExprListExp *expr_list = SageBuilder::buildExprListExp_nfi();
    for (unsigned i = 0; i < num_exprs; i++) {
      SgNode *tmp_expr = Traverse(paran_list_expr->getExpr(i));
      SgExpression *expr = isSgExpression(tmp_expr);
      if (expr == nullptr) {
        std::cerr << "REX_FRONTEND_INVARIANT[parenthesized-expression-list]: "
                     "element "
                  << i << " did not translate to an expression" << std::endl;
        ROSE_ABORT();
      }
      expr_list->append_expression(expr);
    }
    *node = expr_list;
  }

  return VisitExpr(paran_list_expr, node);
}

bool ClangToSageTranslator::VisitCXXParenListInitExpr(
    clang::CXXParenListInitExpr *cxx_paren_list_init_expr, SgNode **node) {
  SgType *constructed_type =
      buildTypeFromQualifiedType(cxx_paren_list_init_expr->getType());

  SgExprListExp *args = SageBuilder::buildExprListExp_nfi();
  applySourceRange(args, cxx_paren_list_init_expr->getSourceRange());
  llvm::ArrayRef<clang::Expr *> init_exprs =
      cxx_paren_list_init_expr->getInitExprs();
  for (clang::Expr *clang_arg : init_exprs) {
    if (clang_arg == nullptr) {
      std::cerr << "REX_FRONTEND_INVARIANT[paren-list-init-argument]: Clang "
                   "initializer list contains a null argument"
                << std::endl;
      ROSE_ABORT();
    }
    SgNode *tmp_arg = Traverse(clang_arg);
    SgExpression *arg_expr = isSgExpression(tmp_arg);
    if (arg_expr == nullptr) {
      std::cerr << "REX_FRONTEND_INVARIANT[paren-list-init-argument]: Clang "
                   "argument translated to "
                << (tmp_arg == nullptr ? std::string("<null>")
                                       : tmp_arg->class_name())
                << " instead of SgExpression" << std::endl;
      ROSE_ABORT();
    }
    args->append_expression(arg_expr);
  }

  const bool class_unknown =
      constructorInitializerAssociatedClassUnknown(constructed_type);

  SgConstructorInitializer *ctor_init =
      SageBuilder::buildConstructorInitializer_nfi(
          nullptr, args, constructed_type,
          false,        // need_name
          false,        // need_qualifier
          true,         // need_parenthesis_after_name
          class_unknown // associated_class_unknown
      );
  applySourceRange(ctor_init, cxx_paren_list_init_expr->getSourceRange());
  *node = ctor_init;

  return VisitExpr(cxx_paren_list_init_expr, node);
}

bool ClangToSageTranslator::VisitPredefinedExpr(
    clang::PredefinedExpr *predefined_expr, SgNode **node) {
  SgFunctionDefinition *func_def = nullptr;
  std::list<SgScopeStatement *>::reverse_iterator it =
      SageBuilder::ScopeStack.rbegin();
  while (it != SageBuilder::ScopeStack.rend() && func_def == nullptr) {
    func_def = isSgFunctionDefinition(*it);
    it++;
  }
  if (func_def == nullptr || func_def->get_symbol_table() == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[predefined-identifier-owner]: "
                 "Clang PredefinedExpr has no exact active function "
                 "definition and symbol table"
              << std::endl;
    ROSE_ABORT();
  }

  SgName name;
  const clang::PredefinedIdentKind ident_kind = predefined_expr->getIdentKind();
  switch (ident_kind) {
  case clang::PredefinedIdentKind::Func:
    name = "__func__";
    break;
  case clang::PredefinedIdentKind::Function:
    name = "__FUNCTION__";
    break;
  case clang::PredefinedIdentKind::LFunction:
    name = "L__FUNCTION__";
    break;
  case clang::PredefinedIdentKind::FuncDName:
    name = "__FUNCDNAME__";
    break;
  case clang::PredefinedIdentKind::FuncSig:
    name = "__FUNCSIG__";
    break;
  case clang::PredefinedIdentKind::LFuncSig:
    name = "L__FUNCSIG__";
    break;
  case clang::PredefinedIdentKind::PrettyFunction:
  case clang::PredefinedIdentKind::PrettyFunctionNoVirtual:
    name = "__PRETTY_FUNCTION__";
    break;
  default:
    std::cerr << "REX_FRONTEND_INVARIANT[predefined-identifier-kind]: "
                 "unsupported clang::PredefinedIdentKind="
              << static_cast<unsigned int>(ident_kind) << std::endl;
    ROSE_ABORT();
  }

  // A predefined identifier is an implicit function-local variable even when
  // the reference appears in a nested block.  Translate its type in that exact
  // owner scope so Clang's one dependent builtin type cannot acquire a
  // different SgNonrealType identity for each nested statement scope.
  const bool push_function_scope = SageBuilder::topScopeStack() != func_def;
  if (push_function_scope) {
    SageBuilder::pushScopeStack(func_def);
  }
  SgType *identifier_type =
      buildTypeFromQualifiedType(predefined_expr->getType());
  if (push_function_scope) {
    if (SageBuilder::topScopeStack() != func_def) {
      std::cerr << "REX_FRONTEND_INVARIANT[predefined-identifier-owner]: "
                   "predefined-identifier type translation changed its exact "
                   "function scope"
                << std::endl;
      ROSE_ABORT();
    }
    SageBuilder::popScopeStack();
  }
  if (identifier_type == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[predefined-identifier-type]: "
                 "exact Clang predefined-identifier type did not translate"
              << std::endl;
    ROSE_ABORT();
  }

  SgVariableSymbol *symbol = func_def->lookup_variable_symbol(name);
  if (symbol == nullptr) {
    SgVariableDeclaration *implicit_declaration =
        SageBuilder::buildSemanticAuxiliaryVariableDeclaration(
            name, identifier_type, nullptr, func_def);
    if (implicit_declaration == nullptr ||
        implicit_declaration->get_variables().size() != 1) {
      std::cerr << "REX_FRONTEND_INVARIANT[predefined-identifier-owner]: "
                   "failed to build one exact semantic variable declaration"
                << std::endl;
      ROSE_ABORT();
    }
    SgInitializedName *init_name =
        implicit_declaration->get_variables().front();
    if (init_name == nullptr) {
      std::cerr << "REX_FRONTEND_INVARIANT[predefined-identifier-owner]: "
                   "semantic variable declaration has no initialized name"
                << std::endl;
      ROSE_ABORT();
    }
    init_name->set_is_predefined_identifier(true);
    symbol =
        isSgVariableSymbol(func_def->find_symbol_from_declaration(init_name));
  }

  SgInitializedName *init_name =
      symbol != nullptr ? symbol->get_declaration() : nullptr;
  Rose::ClangFrontend::requireExactPredefinedIdentifierType(init_name,
                                                            identifier_type);
  SgSymbolTable *symbol_table = func_def->get_symbol_table();
  SgVariableDeclaration *implicit_declaration =
      init_name != nullptr ? isSgVariableDeclaration(init_name->get_parent())
                           : nullptr;
  SgAuxiliaryDeclarationList *auxiliary_owner =
      implicit_declaration != nullptr
          ? isSgAuxiliaryDeclarationList(implicit_declaration->get_parent())
          : nullptr;
  auto is_exact_semantic_file_info = [](Sg_File_Info *file_info,
                                        SgNode *owner) {
    return owner != nullptr && file_info != nullptr &&
           file_info->get_parent() == owner &&
           file_info->isCompilerGenerated() &&
           file_info->isFrontendSpecific() && !file_info->isTransformation() &&
           !file_info->isSourcePositionUnavailableInFrontend() &&
           file_info->isOutputInCodeGeneration() &&
           file_info->get_file_id() ==
               Sg_File_Info::COMPILER_GENERATED_FILE_ID &&
           file_info->get_physical_file_id() ==
               Sg_File_Info::COMPILER_GENERATED_FILE_ID;
  };
  if (symbol == nullptr || init_name == nullptr ||
      !init_name->get_is_predefined_identifier() ||
      init_name->get_name() != name || implicit_declaration == nullptr ||
      auxiliary_owner == nullptr || auxiliary_owner->get_parent() != func_def ||
      func_def->get_auxiliary_declarations() != auxiliary_owner ||
      implicit_declaration->get_scope() != func_def ||
      init_name->get_parent() != implicit_declaration ||
      init_name->get_scope() != func_def ||
      implicit_declaration->get_variables().size() != 1 ||
      implicit_declaration->get_variables().front() != init_name ||
      symbol->get_symbol_basis() != init_name ||
      symbol->get_type() != identifier_type ||
      symbol->get_parent() != symbol_table || !symbol_table->exists(symbol) ||
      func_def->lookup_variable_symbol(name) != symbol ||
      func_def->find_symbol_from_declaration(init_name) != symbol ||
      !is_exact_semantic_file_info(init_name->get_file_info(), init_name) ||
      !is_exact_semantic_file_info(init_name->get_startOfConstruct(),
                                   init_name) ||
      !is_exact_semantic_file_info(init_name->get_endOfConstruct(),
                                   init_name) ||
      !is_exact_semantic_file_info(implicit_declaration->get_file_info(),
                                   implicit_declaration) ||
      !is_exact_semantic_file_info(implicit_declaration->get_startOfConstruct(),
                                   implicit_declaration) ||
      !is_exact_semantic_file_info(implicit_declaration->get_endOfConstruct(),
                                   implicit_declaration)) {
    std::cerr << "REX_FRONTEND_INVARIANT[predefined-identifier-identity]: "
                 "predefined identifier '"
              << name
              << "' has no exact typed function-definition owner, semantic "
                 "provenance, or symbol identity"
              << std::endl;
    ROSE_ABORT();
  }
  *node = SageBuilder::buildVarRefExp_nfi(symbol);

  return VisitExpr(predefined_expr, node);
}

bool ClangToSageTranslator::VisitPseudoObjectExpr(
    clang::PseudoObjectExpr *pseudo_object_expr, SgNode **node) {
  clang::Expr *syntactic_expr = pseudo_object_expr->getSyntacticForm();
  if (syntactic_expr == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[pseudo-object]: no translatable form"
              << std::endl;
    ROSE_ABORT();
  }

  auto object_kind = [](clang::ExprObjectKind kind) {
    using Binding = SgOpaqueValueBinding;
    switch (kind) {
    case clang::OK_Ordinary:
      return Binding::e_ordinary_object;
    case clang::OK_BitField:
      return Binding::e_bit_field;
    case clang::OK_VectorComponent:
      return Binding::e_vector_component;
    case clang::OK_ObjCProperty:
      return Binding::e_objc_property;
    case clang::OK_ObjCSubscript:
      return Binding::e_objc_subscript;
    case clang::OK_MatrixComponent:
      return Binding::e_matrix_component;
    }
    ROSE_ABORT();
  };
  struct ConstructionScope {
    std::deque<PseudoObjectConstructionFrame> &frames;
    size_t depth;
    explicit ConstructionScope(std::deque<PseudoObjectConstructionFrame> &input)
        : frames(input), depth(input.size()) {
      frames.emplace_back();
    }
    ~ConstructionScope() {
      ROSE_ASSERT(frames.size() == depth + 1);
      frames.pop_back();
    }
  } construction(p_pseudo_object_construction_frames);
  auto &frame = p_pseudo_object_construction_frames.back();
  auto *evaluations = SageBuilder::buildExprListExp_nfi();
  publishCanonicalSemanticExpressionSourceProvenance(
      evaluations, "pseudo-object-evaluations");
  SgExpression *result = nullptr;
  {
    SemanticExpressionConstruction semantic(p_semantic_expression_subtree_depth,
                                            "pseudo-object-evaluation");
    SemanticExpressionConstruction literals(p_canonical_generated_literal_depth,
                                            "pseudo-object-generated-literal");
    for (clang::Expr *expression : pseudo_object_expr->semantics()) {
      SgExpression *evaluation = nullptr;
      if (auto *opaque = llvm::dyn_cast<clang::OpaqueValueExpr>(expression)) {
        if (opaque->isUnique()) {
          ROSE_ASSERT(expression != pseudo_object_expr->getResultExpr());
          continue;
        }
        clang::Expr *source = opaque->getSourceExpr();
        if (source == nullptr)
          failExactExpressionTranslation("pseudo-object-binding", opaque);
        auto *translated = isSgExpression(Traverse(source));
        ROSE_ASSERT(translated != nullptr);
        auto *binding = new SgOpaqueValueBinding(
            translated, buildTypeFromQualifiedType(opaque->getType()),
            translateClangValueCategory(opaque->getValueKind()),
            object_kind(opaque->getObjectKind()));
        ROSE_ASSERT(frame.bindings.emplace(opaque, binding).second);
        publishCanonicalSemanticExpressionSourceProvenance(
            binding, "pseudo-object-binding");
        evaluation = binding;
      } else {
        evaluation = isSgExpression(Traverse(expression));
        ROSE_ASSERT(evaluation != nullptr);
      }
      evaluations->append_expression(evaluation);
      if (expression == pseudo_object_expr->getResultExpr())
        result = evaluation;
    }
  }
  frame.written = true;
  auto *written = isSgExpression(Traverse(syntactic_expr));
  ROSE_ASSERT(written != nullptr);
  *node = new SgPseudoObjectExp(
      written, evaluations, result, frame.captures,
      buildTypeFromQualifiedType(pseudo_object_expr->getType()),
      translateClangValueCategory(pseudo_object_expr->getValueKind()),
      object_kind(pseudo_object_expr->getObjectKind()));
  applySourceRange(*node, pseudo_object_expr->getSourceRange());

  return VisitExpr(pseudo_object_expr, node);
}

bool ClangToSageTranslator::VisitShuffleVectorExpr(
    clang::ShuffleVectorExpr *shuffle_vector_expr, SgNode **node) {
  SgExprListExp *args = SageBuilder::buildExprListExp_nfi();
  ROSE_ASSERT(args != nullptr);
  applySourceRange(args, shuffle_vector_expr->getSourceRange());

  for (clang::Stmt *child : shuffle_vector_expr->children()) {
    if (child == nullptr) {
      std::cerr << "REX_FRONTEND_INVARIANT[shuffle-vector-argument]: Clang "
                   "shuffle expression contains a null child"
                << std::endl;
      ROSE_ABORT();
    }

    SgNode *tmp_arg = Traverse(child);
    SgExpression *arg = isSgExpression(tmp_arg);
    if (arg == nullptr) {
      std::cerr << "REX_FRONTEND_INVARIANT[shuffle-vector-argument]: Clang "
                   "child translated to "
                << (tmp_arg == nullptr ? std::string("<null>")
                                       : tmp_arg->class_name())
                << " instead of SgExpression" << std::endl;
      ROSE_ABORT();
    }
    args->append_expression(arg);
  }

  SgScopeStatement *scope = SageBuilder::topScopeStack();
  if (scope == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[shuffle-vector-scope]: Clang "
                 "expression has no active Sage scope"
              << std::endl;
    ROSE_ABORT();
  }

  SgType *return_type =
      buildTypeFromQualifiedType(shuffle_vector_expr->getType());
  SgFunctionCallExp *call = SageBuilder::buildFunctionCallExp_nfi(
      "__builtin_shufflevector", return_type, args, scope);
  SgFunctionRefExp *builtin_reference =
      call != nullptr ? isSgFunctionRefExp(call->get_function()) : nullptr;
  if (builtin_reference == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[shuffle-vector-callee]: Sage "
                 "construction did not produce one exact builtin function "
                 "reference"
              << std::endl;
    ROSE_ABORT();
  }
  SageBuilder::initializeSemanticExpressionSourceProvenance(builtin_reference);
  *node = call;
  applySourceRange(*node, shuffle_vector_expr->getSourceRange());

  return VisitExpr(shuffle_vector_expr, node);
}

bool ClangToSageTranslator::VisitSizeOfPackExpr(
    clang::SizeOfPackExpr *size_of_pack_expr, SgNode **node) {
  clang::NamedDecl *pack_decl =
      llvm::dyn_cast_or_null<clang::NamedDecl>(size_of_pack_expr->getPack());
  if (pack_decl == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[sizeof-pack-declaration]: "
                 "sizeof... expression has no parameter-pack declaration"
              << std::endl;
    ROSE_ABORT();
  }
  if (auto *parameter =
          llvm::dyn_cast<clang::NonTypeTemplateParmDecl>(pack_decl);
      parameter != nullptr && p_canonical_template_argument_context.has_value())
    pack_decl = resolveCanonicalNonTypeTemplateParameterUse(parameter);
  SgExpression *pack_expr = nullptr;
  SgType *pack_type = nullptr;
  if (auto *parameter = llvm::dyn_cast<clang::ParmVarDecl>(pack_decl)) {
    SgInitializedName *name = lookupFunctionParameterFromOwner(parameter);
    SgVariableSymbol *symbol =
        name != nullptr && name->get_scope() != nullptr
            ? isSgVariableSymbol(
                  name->get_scope()->find_symbol_from_declaration(name))
            : nullptr;
    if (name == nullptr || !name->get_is_parameter_pack() ||
        symbol == nullptr || symbol->get_declaration() != name) {
      std::cerr << "REX_FRONTEND_INVARIANT[sizeof-pack-parameter]: function "
                   "pack has no exact translated symbol"
                << std::endl;
      ROSE_ABORT();
    }
    pack_expr = SageBuilder::buildVarRefExp_nfi(symbol);
  } else if (llvm::isa<clang::TemplateTypeParmDecl>(pack_decl) ||
             llvm::isa<clang::NonTypeTemplateParmDecl>(pack_decl) ||
             llvm::isa<clang::TemplateTemplateParmDecl>(pack_decl)) {
    SgTemplateParameter *parameter =
        lookupActiveTemplateParameterSurface(pack_decl, "VisitSizeOfPackExpr");
    if (parameter == nullptr) {
      SgDeclarationStatement *owner =
          resolveTemplateParameterOwner(pack_decl, /*allow_on_demand=*/true);
      parameter = requirePublishedTemplateParameter(pack_decl, owner,
                                                    "VisitSizeOfPackExpr");
    }
    if (parameter == nullptr || !parameter->get_is_parameter_pack()) {
      std::cerr << "REX_FRONTEND_INVARIANT[sizeof-pack-parameter]: template "
                   "pack has no exact translated parameter"
                << std::endl;
      ROSE_ABORT();
    }
    if (auto *value =
            llvm::dyn_cast<clang::NonTypeTemplateParmDecl>(pack_decl)) {
      SgTemplateParameterVal *pack_value =
          SageBuilder::buildTemplateParameterVal_nfi(value->getIndex(),
                                                     parameter);
      pack_value->set_literal_spelling_form(
          SgValueExp::e_literal_canonical_generated);
      pack_expr = pack_value;
    } else {
      SgTemplateType *type = isSgTemplateType(parameter->get_type());
      if (type == nullptr || type->get_template_parameter() != parameter ||
          type->get_name().is_null()) {
        std::cerr << "REX_FRONTEND_INVARIANT[sizeof-pack-type]: type pack "
                     "has no exact named template parameter type"
                  << std::endl;
        ROSE_ABORT();
      }
      pack_type = type;
    }
  } else {
    std::cerr << "REX_FRONTEND_INVARIANT[sizeof-pack-declaration]: unsupported "
                 "parameter-pack declaration kind"
              << std::endl;
    ROSE_ABORT();
  }
  if (pack_expr != nullptr) {
    setSynthesizedFileInfo(pack_expr);
    mark_compiler_generated_frontend_specific(pack_expr);
  }

  SgType *result_type =
      buildTypeFromQualifiedType(size_of_pack_expr->getType());
  if (result_type == nullptr ||
      result_type != SageInterface::requireTargetSizeType(p_sage_source_file)) {
    std::cerr << "REX_FRONTEND_INVARIANT[sizeof-pack-result-type]: Clang "
                 "sizeof... result does not match the translation unit's "
                 "exact target size_t type"
              << std::endl;
    ROSE_ABORT();
  }
  std::optional<Rose::Integer128Value> evaluated_value;
  if (!size_of_pack_expr->isValueDependent()) {
    auto value = size_of_pack_expr->getIntegerConstantExpr(
        p_compiler_instance->getASTContext());
    ROSE_ASSERT(value && value->isUnsigned());
    evaluated_value = integer128ValueFromClang(*value);
  }
  SgSizeOfOp *sizeof_op =
      pack_expr != nullptr
          ? SageBuilder::buildSizeOfOp_nfi(pack_expr, result_type,
                                           SgExpression::e_sizeof_pack,
                                           evaluated_value)
          : SageBuilder::buildSizeOfOp_nfi(pack_type, result_type,
                                           SgExpression::e_sizeof_pack,
                                           evaluated_value);
  *node = sizeof_op;

  return VisitExpr(size_of_pack_expr, node);
}

bool ClangToSageTranslator::VisitSourceLocExpr(
    clang::SourceLocExpr *source_loc_expr, SgNode **node) {
  using SageKind =
      SgSourceLocationBuiltinExp::source_location_builtin_kind_enum;
  SageKind sage_kind;
  switch (source_loc_expr->getIdentKind()) {
  case clang::SourceLocIdentKind::Function:
    sage_kind = SgSourceLocationBuiltinExp::e_function;
    break;
  case clang::SourceLocIdentKind::FuncSig:
    sage_kind = SgSourceLocationBuiltinExp::e_function_signature;
    break;
  case clang::SourceLocIdentKind::File:
    sage_kind = SgSourceLocationBuiltinExp::e_file;
    break;
  case clang::SourceLocIdentKind::FileName:
    sage_kind = SgSourceLocationBuiltinExp::e_file_name;
    break;
  case clang::SourceLocIdentKind::Line:
    sage_kind = SgSourceLocationBuiltinExp::e_line;
    break;
  case clang::SourceLocIdentKind::Column:
    sage_kind = SgSourceLocationBuiltinExp::e_column;
    break;
  case clang::SourceLocIdentKind::SourceLocStruct:
    sage_kind = SgSourceLocationBuiltinExp::e_source_location;
    break;
  default:
    failExactExpressionTranslation("source-location-builtin", source_loc_expr);
  }
  SgType *expression_type =
      buildTypeFromQualifiedType(source_loc_expr->getType());
  if (SageInterface::containsUnknownType(expression_type)) {
    failExactExpressionTranslation("source-location-builtin-type",
                                   source_loc_expr);
  }
  *node = new SgSourceLocationBuiltinExp(sage_kind, expression_type);

  return VisitExpr(source_loc_expr, node);
}

bool ClangToSageTranslator::VisitStmtExpr(clang::StmtExpr *stmt_expr,
                                          SgNode **node) {
  clang::CompoundStmt *clang_substmt = stmt_expr->getSubStmt();
  if (clang_substmt == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[statement-expression-body]: Clang "
                 "StmtExpr has no CompoundStmt body"
              << std::endl;
    ROSE_ABORT();
  }
  const auto active_body = std::find_if(
      p_statement_expression_body_construction_stack.begin(),
      p_statement_expression_body_construction_stack.end(),
      [clang_substmt](const StatementExpressionBodyConstruction &transaction) {
        return transaction.clang_body == clang_substmt;
      });
  if (active_body != p_statement_expression_body_construction_stack.end()) {
    std::cerr << "REX_FRONTEND_INVARIANT[statement-expression-body]: Clang "
                 "CompoundStmt is already being constructed for another "
                 "statement-expression owner"
              << std::endl;
    ROSE_ABORT();
  }
  SgScopeStatement *semantic_scope = SageBuilder::topScopeStack();
  if (semantic_scope == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[statement-expression-scope]: GNU "
                 "statement-expression has no exact enclosing semantic scope"
              << std::endl;
    ROSE_ABORT();
  }
  p_statement_expression_body_construction_stack.push_back(
      {clang_substmt, semantic_scope, nullptr});
  SgNode *tmp_substmt = Traverse(clang_substmt);
  if (p_statement_expression_body_construction_stack.empty() ||
      p_statement_expression_body_construction_stack.back().clang_body !=
          clang_substmt) {
    std::cerr << "REX_FRONTEND_INVARIANT[statement-expression-body]: exact "
                 "detached-body construction transaction was corrupted"
              << std::endl;
    ROSE_ABORT();
  }
  const StatementExpressionBodyConstruction &transaction =
      p_statement_expression_body_construction_stack.back();
  SgBasicBlock *substmt = isSgBasicBlock(tmp_substmt);
  if (substmt == nullptr || transaction.sage_body != substmt ||
      transaction.semantic_scope != semantic_scope) {
    std::cerr << "REX_FRONTEND_INVARIANT[statement-expression-body]: Clang "
                 "body translated to "
              << (tmp_substmt == nullptr ? std::string("<null>")
                                         : tmp_substmt->class_name())
              << " without its exact SgBasicBlock construction transaction"
              << std::endl;
    ROSE_ABORT();
  }
  if (substmt->get_parent() != nullptr ||
      substmt->get_statement_expression_construction_scope() !=
          semantic_scope ||
      substmt->get_statement_expression_semantic_scope() != nullptr ||
      substmt->get_scope() != semantic_scope) {
    std::cerr << "REX_FRONTEND_INVARIANT[statement-expression-body-owner]: "
                 "translated GNU statement-expression body was not left "
                 "detached with its exact construction-time semantic scope"
              << std::endl;
    ROSE_ABORT();
  }

  // Clang has already applied the GNU statement-expression rules, including
  // void results and array/function/lvalue conversions.  Store that semantic
  // type directly; inspecting the final Sage statement would lose it.
  SgType *result_type = buildTypeFromQualifiedType(stmt_expr->getType());
  SgStatementExpression *statement_expression =
      SageBuilder::buildStatementExpression_nfi(substmt, result_type,
                                                semantic_scope);
  if (statement_expression->get_statement() != substmt ||
      substmt->get_parent() != statement_expression ||
      substmt->get_statement_expression_construction_scope() != nullptr ||
      substmt->get_statement_expression_semantic_scope() != semantic_scope ||
      substmt->get_scope() != semantic_scope) {
    std::cerr << "REX_FRONTEND_INVARIANT[statement-expression-body-owner]: "
                 "typed statement-expression owner did not consume the exact "
                 "body construction transaction"
              << std::endl;
    ROSE_ABORT();
  }
  p_statement_expression_body_construction_stack.pop_back();
  applySourceRange(statement_expression, stmt_expr->getSourceRange());
  *node = statement_expression;

  return VisitExpr(stmt_expr, node);
}

bool ClangToSageTranslator::VisitStringLiteral(
    clang::StringLiteral *string_literal, SgNode **node) {
  const unsigned token_count = string_literal->getNumConcatenated();
  const bool canonical_generated = p_canonical_generated_literal_depth != 0;
  if (token_count == 0 && !canonical_generated) {
    std::cerr << "REX_FRONTEND_INVARIANT[cxx-string-surface]: Clang string "
                 "literal has no source tokens"
              << std::endl;
    ROSE_ABORT();
  }
  bool udl_compiler_generated = false;
  auto exact_udl = p_exact_literal_operand_spellings.find(string_literal);
  if (exact_udl != p_exact_literal_operand_spellings.end()) {
    if (exact_udl->second.consumed || !exact_udl->second.compiler_generated ||
        !exact_udl->second.spelling.empty()) {
      std::cerr << "REX_FRONTEND_INVARIANT[user-defined-literal-session]: "
                   "string operand has no exact unconsumed generated role"
                << std::endl;
      ROSE_ABORT();
    }
    udl_compiler_generated = true;
    exact_udl->second.consumed = true;
  }
  SgStringCodeUnitList code_units;
  code_units.reserve(string_literal->getLength());
  for (unsigned index = 0; index < string_literal->getLength(); ++index)
    code_units.push_back(string_literal->getCodeUnit(index));
  SgStringVal *sgStrVal = new SgStringVal(code_units);
  SageInterface::setOneSourcePositionNull(sgStrVal);
  const SgStringVal::string_literal_encoding_enum encoding =
      cxxStringLiteralEncoding(string_literal);
  sgStrVal->set_literal_encoding(encoding);
  sgStrVal->set_cxx_unevaluated(string_literal->isUnevaluated());
  if (!sgStrVal->get_cxx_unevaluated()) {
    SgType *literal_type =
        buildTypeFromQualifiedType(string_literal->getType());
    if (SageInterface::containsUnknownType(literal_type) ||
        sgStrVal->get_literal_type() != nullptr) {
      std::cerr << "REX_FRONTEND_INVARIANT[cxx-string-semantic-type]: Clang "
                   "string literal has no exact unpublished result type"
                << std::endl;
      ROSE_ABORT();
    }
    sgStrVal->set_literal_type(literal_type);
    if (sgStrVal->get_type() != literal_type) {
      std::cerr << "REX_FRONTEND_INVARIANT[cxx-string-semantic-type]: Sage "
                   "string literal did not retain Clang's exact result type"
                << std::endl;
      ROSE_ABORT();
    }
  } else if (!string_literal->getType().isNull()) {
    std::cerr << "REX_FRONTEND_INVARIANT[cxx-unevaluated-string-type]: Clang "
                 "unevaluated string unexpectedly owns a semantic value type"
              << std::endl;
    ROSE_ABORT();
  }
  if (token_count == 1 && !udl_compiler_generated && !canonical_generated) {
    auto &source_manager = p_compiler_instance->getSourceManager();
    const auto &language = p_compiler_instance->getLangOpts();
    clang::Token token;
    if (clang::Lexer::getRawToken(
            source_manager.getSpellingLoc(string_literal->getStrTokenLoc(0)),
            token, source_manager, language))
      failExactExpressionTranslation("string-literal-source-token",
                                     string_literal);
    bool invalid = false;
    const std::string spelling =
        clang::Lexer::getSpelling(token, source_manager, language, &invalid);
    if (invalid || spelling.empty())
      failExactExpressionTranslation("string-literal-token-spelling",
                                     string_literal);
    const SgStringVal::string_literal_encoding_enum source_encoding =
        cxxStringLiteralTokenEncoding(token.getKind());
    if (!string_literal->isUnevaluated() && source_encoding != encoding) {
      std::cerr << "REX_FRONTEND_INVARIANT[cxx-string-surface]: source "
                   "encoding="
                << static_cast<int>(source_encoding)
                << " disagrees with exact Clang encoding="
                << static_cast<int>(encoding) << std::endl;
      ROSE_ABORT();
    }
    setCxxStringLiteralSurface(sgStrVal, source_encoding, spelling);
  } else {
    // Generated operands and concatenated literals retain their semantic
    // encoding and payload without inventing an independent source token.
    sgStrVal->set_isRawString(false);
    sgStrVal->set_raw_string_delimiter("");
    sgStrVal->set_raw_string_payload("");
    sgStrVal->validate_cxx_literal_state();
  }
  *node = sgStrVal;

  if (udl_compiler_generated || canonical_generated) {
    sgStrVal->set_literal_spelling_form(
        SgValueExp::e_literal_canonical_generated);
    setSynthesizedFileInfo(*node);
  } else {
    sgStrVal->set_literal_spelling_form(SgValueExp::e_literal_source_spelled);
  }
  return VisitExpr(string_literal, node);
}

bool ClangToSageTranslator::VisitSubstNonTypeTemplateParmExpr(
    clang::SubstNonTypeTemplateParmExpr *subst_non_type_template_parm_expr,
    SgNode **node) {
  clang::Expr *replacement =
      subst_non_type_template_parm_expr->getReplacement();
  if (replacement == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[substituted-template-argument]: exact "
                 "semantic replacement expression is absent"
              << std::endl;
    ROSE_ABORT();
  }
  // The replacement is Clang's instantiated semantic expression, not the
  // lexical spelling at the parameter-use site.  Its source range can be
  // anchored to the identifier token while its children retain semantic
  // details from the substituted argument (for example, the `S::` qualifier
  // in a pointer-to-member value substituted for `PTM`).  Translate the whole
  // replacement under the semantic-expression transaction so those typed
  // details cannot be published as source tokens owned by the anchor.
  ++p_semantic_expression_subtree_depth;
  ++p_canonical_generated_literal_depth;
  *node = Traverse(replacement);
  if (p_semantic_expression_subtree_depth == 0 ||
      p_canonical_generated_literal_depth == 0) {
    std::cerr
        << "REX_FRONTEND_INVARIANT[substituted-template-argument]: canonical "
           "semantic-expression translation depth underflowed"
        << std::endl;
    ROSE_ABORT();
  }
  --p_canonical_generated_literal_depth;
  --p_semantic_expression_subtree_depth;

  return VisitExpr(subst_non_type_template_parm_expr, node);
}

bool ClangToSageTranslator::VisitSubstNonTypeTemplateParmPackExpr(
    clang::SubstNonTypeTemplateParmPackExpr
        *subst_non_type_template_parm_pack_expr,
    SgNode **node) {
  auto build_expr_from_template_arg =
      [&](const clang::TemplateArgument &arg) -> SgExpression * {
    std::unique_ptr<SgTemplateArgument> sg_arg(
        translateTemplateArgument(arg, false));
    ROSE_ASSERT(sg_arg->get_parent() == nullptr);
    ROSE_ASSERT(sg_arg->get_argumentType() ==
                SgTemplateArgument::nontype_argument);
    if (SgExpression *expr = sg_arg->get_expression()) {
      ROSE_ASSERT(expr->get_parent() == sg_arg.get());
      sg_arg->set_expression(nullptr);
      expr->set_parent(nullptr);
      return expr;
    }
    failExactExpressionTranslation("substituted-nontype-template-pack-argument",
                                   subst_non_type_template_parm_pack_expr);
  };

  std::function<void(const clang::TemplateArgument &, SgExprListExp *)>
      append_pack_expr;
  append_pack_expr = [&](const clang::TemplateArgument &arg,
                         SgExprListExp *list) {
    if (arg.getKind() == clang::TemplateArgument::Pack) {
      for (const clang::TemplateArgument &pack_arg : arg.pack_elements()) {
        append_pack_expr(pack_arg, list);
      }
      return;
    }
    SgExpression *expr = build_expr_from_template_arg(arg);
    list->get_expressions().push_back(expr);
    expr->set_parent(list);
  };

  SgExprListExp *pack_list = SageBuilder::buildExprListExp_nfi();
  clang::TemplateArgument pack_arg =
      subst_non_type_template_parm_pack_expr->getArgumentPack();
  append_pack_expr(pack_arg, pack_list);

  *node = pack_list;
  return VisitExpr(subst_non_type_template_parm_pack_expr, node);
}

bool ClangToSageTranslator::VisitTypeTraitExpr(clang::TypeTraitExpr *type_trait,
                                               SgNode **node) {
  const char *trait_name = clang::getTraitSpelling(type_trait->getTrait());
  ROSE_ASSERT(trait_name != nullptr);

  SgExpressionPtrList args;
  for (unsigned i = 0; i < type_trait->getNumArgs(); ++i) {
    clang::TypeSourceInfo *arg_info = type_trait->getArg(i);
    if (arg_info == nullptr) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[type-trait-argument]: trait=%s "
              "argument=%u has no exact TypeSourceInfo\n",
              trait_name, i);
      ROSE_ABORT();
    }
    clang::TypeLoc arg_type_loc = arg_info->getTypeLoc();

    SgType *arg_type = buildTypeFromTypeLoc(arg_type_loc);
    if (arg_type == nullptr) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[type-trait-argument]: trait=%s "
              "argument=%u has no exact translated TypeLoc\n",
              trait_name, i);
      ROSE_ABORT();
    }
    SgTypeExpression *arg_operand =
        SageBuilder::buildTypeExpression_nfi(arg_type);
    applySourceRange(arg_operand, arg_type_loc.getSourceRange());
    args.push_back(arg_operand);
  }
  if (args.empty()) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[type-trait-argument]: trait=%s has no "
            "exact typed arguments\n",
            trait_name);
    ROSE_ABORT();
  }
  SgType *result_type = buildTypeFromQualifiedType(type_trait->getType());
  *node = SageBuilder::buildTypeTraitBuiltinOperator_nfi(
      trait_name, SgTypeTraitBuiltinOperator::e_type_trait_builtin, result_type,
      args);

  return VisitExpr(type_trait, node);
}

bool ClangToSageTranslator::VisitUnaryExprOrTypeTraitExpr(
    clang::UnaryExprOrTypeTraitExpr *unary_expr_or_type_trait_expr,
    SgNode **node) {
  ROSE_ASSERT(unary_expr_or_type_trait_expr != nullptr && node != nullptr);
  SgExpression::unary_type_trait_kind_enum operator_kind;
  switch (unary_expr_or_type_trait_expr->getKind()) {
  case clang::UETT_SizeOf:
    operator_kind = SgExpression::e_sizeof;
    break;
  case clang::UETT_DataSizeOf:
    operator_kind = SgExpression::e_datasizeof;
    break;
  case clang::UETT_AlignOf:
    operator_kind = SgExpression::e_alignof;
    break;
  case clang::UETT_PreferredAlignOf:
    operator_kind = SgExpression::e_preferred_alignof;
    break;
  default:
    std::cerr << "REX_FRONTEND_INVARIANT[unary-trait-kind]: unsupported Clang "
                 "trait kind "
              << static_cast<unsigned>(unary_expr_or_type_trait_expr->getKind())
              << std::endl;
    ROSE_ABORT();
  }
  SgType *result_type =
      buildTypeFromQualifiedType(unary_expr_or_type_trait_expr->getType());
  if (result_type == nullptr ||
      result_type != SageInterface::requireTargetSizeType(p_sage_source_file)) {
    std::cerr << "REX_FRONTEND_INVARIANT[unary-trait-result-type]: Clang trait "
                 "result does not match the translation unit's exact target "
                 "size_t type"
              << std::endl;
    ROSE_ABORT();
  }

  SgExpression *expr = nullptr;
  SgType *type = nullptr;
  SgNonrealDecl::source_elaboration_kind_enum source_elaboration_kind =
      SgNonrealDecl::e_source_elaboration_unspecified;
  SgDeclarationStatement *embedded_tag_definition = nullptr;

  if (unary_expr_or_type_trait_expr->isArgumentType() &&
      hasExactSemanticExpressionConstructionRole(
          "VisitUnaryExprOrTypeTraitExpr")) {
    type = buildSemanticTypeFromQualifiedType(
        unary_expr_or_type_trait_expr->getArgumentType(),
        "VisitUnaryExprOrTypeTraitExpr:semantic-operand");
    source_elaboration_kind = SgNonrealDecl::e_source_elaboration_none;
  } else if (unary_expr_or_type_trait_expr->isArgumentType()) {
    clang::TypeSourceInfo *argument_type_info =
        unary_expr_or_type_trait_expr->getArgumentTypeInfo();
    if (argument_type_info == nullptr) {
      std::cerr << "REX_FRONTEND_INVARIANT[unary-trait-type-source]: Clang "
                   "type operand has no TypeSourceInfo"
                << std::endl;
      ROSE_ABORT();
    }
    clang::TypeLoc argument_type_loc = argument_type_info->getTypeLoc();
    clang::TagDecl *embedded_tag_declaration_in_type_operand =
        definingTagFromTypeLoc(argument_type_loc);
    if (embedded_tag_declaration_in_type_operand != nullptr) {
      beginTypedOwnerTagConstruction(embedded_tag_declaration_in_type_operand,
                                     SageBuilder::topScopeStack(),
                                     "VisitUnaryExprOrTypeTraitExpr");
    }
    source_elaboration_kind =
        exactTypeLocSourceElaborationKind(argument_type_loc);
    type = buildTypeFromTypeLoc(argument_type_loc);
    if (type == nullptr || SageInterface::containsUnknownType(type)) {
      std::cerr << "REX_FRONTEND_INVARIANT[unary-trait-type]: exact written "
                   "type operand did not translate"
                << std::endl;
      ROSE_ABORT();
    }
    if (embedded_tag_declaration_in_type_operand != nullptr) {
      embedded_tag_definition = finishTypedOwnerTagConstruction(
          embedded_tag_declaration_in_type_operand,
          "VisitUnaryExprOrTypeTraitExpr");
    }
  } else {
    clang::Expr *argument_expr =
        unary_expr_or_type_trait_expr->getArgumentExpr();
    if (argument_expr == nullptr) {
      std::cerr << "REX_FRONTEND_INVARIANT[unary-trait-expression]: Clang "
                   "expression operand is absent"
                << std::endl;
      ROSE_ABORT();
    }
    SgNode *tmp_expr = Traverse(argument_expr);
    expr = isSgExpression(tmp_expr);
    if (expr == nullptr) {
      std::cerr << "REX_FRONTEND_INVARIANT[unary-trait-expression]: Clang "
                   "operand translated to "
                << (tmp_expr == nullptr ? std::string("<null>")
                                        : tmp_expr->class_name())
                << " instead of SgExpression" << std::endl;
      ROSE_ABORT();
    }
  }

  std::optional<Rose::Integer128Value> evaluated_value;
  const bool runtime_size =
      (operator_kind == SgExpression::e_sizeof ||
       operator_kind == SgExpression::e_datasizeof) &&
      unary_expr_or_type_trait_expr->getTypeOfArgument()->isVariableArrayType();
  if (!unary_expr_or_type_trait_expr->isValueDependent() && !runtime_size) {
    auto value = unary_expr_or_type_trait_expr->getIntegerConstantExpr(
        p_compiler_instance->getASTContext());
    if (!value || !value->isUnsigned()) {
      std::cerr << "REX_FRONTEND_INVARIANT[unary-trait-value]: constant "
                   "trait has no exact unsigned Clang value"
                << std::endl;
      ROSE_ABORT();
    }
    evaluated_value = integer128ValueFromClang(*value);
  }

  auto complete_operator = [&](auto *operation) -> SgExpression * {
    if (type != nullptr) {
      operation->set_type_elaboration_required(
          source_elaboration_kind != SgNonrealDecl::e_source_elaboration_none);
      operation->set_source_type_elaboration_kind(source_elaboration_kind);
      if (embedded_tag_definition != nullptr) {
        SgDeclarationStatement *defining_tag = adopt_typed_owner_tag_definition(
            operation, embedded_tag_definition, type,
            "VisitUnaryExprOrTypeTraitExpr");
        operation->set_type_defining_declaration(defining_tag);
      }
    }
    return operation;
  };
  if (operator_kind == SgExpression::e_sizeof ||
      operator_kind == SgExpression::e_datasizeof) {
    *node = complete_operator(
        type != nullptr
            ? SageBuilder::buildSizeOfOp_nfi(type, result_type, operator_kind,
                                             evaluated_value)
            : SageBuilder::buildSizeOfOp_nfi(expr, result_type, operator_kind,
                                             evaluated_value));
  } else {
    *node = complete_operator(
        type != nullptr
            ? SageBuilder::buildAlignOfOp_nfi(type, result_type, operator_kind,
                                              evaluated_value)
            : SageBuilder::buildAlignOfOp_nfi(expr, result_type, operator_kind,
                                              evaluated_value));
  }
  return VisitExpr(unary_expr_or_type_trait_expr, node);
}

bool ClangToSageTranslator::VisitUnaryOperator(
    clang::UnaryOperator *unary_operator, SgNode **node) {

  SgNode *tmp_subexpr = Traverse(unary_operator->getSubExpr());
  SgExpression *subexpr = isSgExpression(tmp_subexpr);
  if (subexpr == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[unary-operand]: Clang operand "
                 "translated to "
              << (tmp_subexpr == nullptr ? std::string("<null>")
                                         : tmp_subexpr->class_name())
              << " instead of SgExpression" << std::endl;
    ROSE_ABORT();
  }

  SgType *result_type = buildTypeFromQualifiedType(unary_operator->getType());
  if (SageInterface::containsUnknownType(result_type)) {
    std::cerr
        << "REX_FRONTEND_INVARIANT[unary-result-type]: Clang "
        << clang::UnaryOperator::getOpcodeStr(unary_operator->getOpcode()).str()
        << " expression has no exact semantic result type" << std::endl;
    ROSE_ABORT();
  }

  switch (unary_operator->getOpcode()) {
  case clang::UO_PostInc:
    *node = SageBuilder::buildPlusPlusOp_nfi(subexpr, result_type,
                                             SgUnaryOp::postfix);
    break;
  case clang::UO_PostDec:
    *node = SageBuilder::buildMinusMinusOp_nfi(subexpr, result_type,
                                               SgUnaryOp::postfix);
    break;
  case clang::UO_PreInc:
    *node = SageBuilder::buildPlusPlusOp_nfi(subexpr, result_type,
                                             SgUnaryOp::prefix);
    break;
  case clang::UO_PreDec:
    *node = SageBuilder::buildMinusMinusOp_nfi(subexpr, result_type,
                                               SgUnaryOp::prefix);
    break;
  case clang::UO_AddrOf: {
    // C++ member-pointer formation depends on an unparenthesized qualified-id,
    // even when Clang's result type is still dependent. Read the typed Clang
    // operand directly; no spelling or enclosing-class inference is involved.
    const clang::Expr *operand = unary_operator->getSubExpr();
    bool qualified_id = false;
    if (const auto *reference = llvm::dyn_cast<clang::DeclRefExpr>(operand))
      qualified_id = reference->hasQualifier();
    else if (const auto *reference =
                 llvm::dyn_cast<clang::DependentScopeDeclRefExpr>(operand))
      qualified_id = static_cast<bool>(reference->getQualifier());
    else if (const auto *reference =
                 llvm::dyn_cast<clang::UnresolvedLookupExpr>(operand))
      qualified_id = static_cast<bool>(reference->getQualifier());
    *node = SageBuilder::buildAddressOfOp_nfi(
        subexpr, result_type,
        qualified_id ? SgAddressOfOp::e_address_operand_qualified_id
                     : SgAddressOfOp::e_address_operand_object);
    break;
  }
  case clang::UO_Deref:
    *node = SageBuilder::buildPointerDerefExp_nfi(subexpr, result_type);
    break;
  case clang::UO_Plus:
    *node = SageBuilder::buildUnaryAddOp_nfi(subexpr, result_type);
    break;
  case clang::UO_Minus:
    *node = SageBuilder::buildMinusOp_nfi(subexpr, result_type);
    break;
  // Def. in Clang: UNARY_OPERATION(Not, "~")
  case clang::UO_Not:
    *node = SageBuilder::buildBitComplementOp_nfi(subexpr, result_type);
    break;
  // Def. in UNARY_OPERATION(LNot, "!")
  case clang::UO_LNot:
    *node = SageBuilder::buildNotOp_nfi(subexpr, result_type);
    break;
  case clang::UO_Real:
    *node = SageBuilder::buildRealPartOp_nfi(subexpr, result_type);
    break;
  case clang::UO_Imag:
    *node = SageBuilder::buildImagPartOp_nfi(subexpr, result_type);
    break;
  case clang::UO_Extension:
    *node = subexpr;
    break;
  default:
    std::cerr
        << "REX_FRONTEND_INVARIANT[unary-operator-kind]: unsupported "
           "Clang opcode "
        << clang::UnaryOperator::getOpcodeStr(unary_operator->getOpcode()).str()
        << std::endl;
    ROSE_ABORT();
  }

  return VisitExpr(unary_operator, node);
}

bool ClangToSageTranslator::VisitVAArgExpr(clang::VAArgExpr *va_arg_expr,
                                           SgNode **node) {
  clang::TypeSourceInfo *written_type_info = va_arg_expr->getWrittenTypeInfo();
  ROSE_ASSERT(written_type_info != nullptr);
  const clang::TypeLoc type_loc = written_type_info->getTypeLoc();
  ROSE_ASSERT(!type_loc.isNull());
  SgType *type = buildTypeFromTypeLoc(type_loc);
  if (type == nullptr || SageInterface::containsUnknownType(type))
    failExactExpressionTranslation("vararg-written-type", va_arg_expr);

  clang::Expr *sub_expr = va_arg_expr->getSubExpr();
  SgNode *tmp_expr = Traverse(sub_expr);
  SgExpression *expr = isSgExpression(tmp_expr);
  ROSE_ASSERT(expr != nullptr);

  *node = SageBuilder::buildVarArgOp_nfi(expr, type);

  return VisitExpr(va_arg_expr, node);
}
bool ClangToSageTranslator::VisitLabelStmt(clang::LabelStmt *label_stmt,
                                           SgNode **node) {
  if (label_stmt == nullptr || node == nullptr) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[label-statement]: translation requires "
            "an exact Clang statement and Sage result slot\n");
    ROSE_ABORT();
  }
  clang::LabelDecl *label_decl =
      llvm::cast_or_null<clang::LabelDecl>(label_stmt->getDecl());
  if (label_decl == nullptr) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[label-statement]: label has no exact "
            "Clang declaration identity\n");
    ROSE_ABORT();
  }
  SgName name(label_stmt->getName());
  auto published = p_decl_translation_map.find(label_decl);
  SgLabelStatement *sg_label_stmt = published != p_decl_translation_map.end()
                                        ? isSgLabelStatement(published->second)
                                        : nullptr;
  if (published != p_decl_translation_map.end() && sg_label_stmt == nullptr) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[label-statement]: Clang label maps to a "
            "non-label Sage node\n");
    ROSE_ABORT();
  }
  if (sg_label_stmt == nullptr) {
    sg_label_stmt = SageBuilder::buildLabelStatement_nfi(
        name, nullptr, SageBuilder::topScopeStack());
  }
  if (sg_label_stmt->get_label() != name ||
      sg_label_stmt->get_statement() != nullptr ||
      sg_label_stmt->get_parent() != nullptr) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[label-statement]: label '%s' did not "
            "enter lexical completion as one detached empty shell\n",
            name.getString().c_str());
    ROSE_ABORT();
  }
  sg_label_stmt->set_parent(
      statementConstructionOwner(label_stmt, "label statement"));
  *node = sg_label_stmt;

  SgFunctionDefinition *label_scope = nullptr;
  std::list<SgScopeStatement *>::reverse_iterator it =
      SageBuilder::ScopeStack.rbegin();
  while (it != SageBuilder::ScopeStack.rend() && label_scope == nullptr) {
    label_scope = isSgFunctionDefinition(*it);
    it++;
  }
  if (label_scope == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[label-scope]: label '" << name
              << "' has no surrounding function definition" << std::endl;
    ROSE_ABORT();
  }
  if (sg_label_stmt->get_scope() != label_scope) {
    std::cerr << "REX_FRONTEND_INVARIANT[label-scope]: label '" << name
              << "' was not published in its exact function scope" << std::endl;
    ROSE_ABORT();
  }
  SgLabelSymbol *label_sym = label_scope->lookup_label_symbol(name);
  if (label_sym == nullptr || label_sym->get_declaration() != sg_label_stmt) {
    std::cerr << "REX_FRONTEND_INVARIANT[label-symbol-publication]: label '"
              << name << "' has no exact construction-time symbol" << std::endl;
    ROSE_ABORT();
  }

  auto existing = p_decl_translation_map.find(label_decl);
  if (existing != p_decl_translation_map.end() &&
      existing->second != sg_label_stmt) {
    std::cerr << "REX_FRONTEND_INVARIANT[label-declaration]: one Clang label "
                 "maps to multiple Sage label statements"
              << std::endl;
    ROSE_ABORT();
  }
  p_decl_translation_map[label_decl] = sg_label_stmt;

  SgStatement *sg_sub_stmt = translateTypedStatementChild(
      label_stmt->getSubStmt(), sg_label_stmt, "label body");

  ROSE_ASSERT(sg_sub_stmt != nullptr);
  sg_sub_stmt = wrapStatementWithPragmas(label_stmt->getSubStmt(), sg_sub_stmt);

  if (sg_sub_stmt->get_parent() != nullptr &&
      sg_sub_stmt->get_parent() != sg_label_stmt) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[label-body-owner]: translated body=%p/%s "
            "already has owner=%p\n",
            static_cast<void *>(sg_sub_stmt), sg_sub_stmt->class_name().c_str(),
            static_cast<void *>(sg_sub_stmt->get_parent()));
    ROSE_ABORT();
  }
  if (sg_sub_stmt->get_parent() == nullptr)
    sg_sub_stmt->set_parent(sg_label_stmt);
  sg_label_stmt->set_statement(sg_sub_stmt);

  return VisitStmt(label_stmt, node);
}

bool ClangToSageTranslator::VisitWhileStmt(clang::WhileStmt *while_stmt,
                                           SgNode **node) {
  clang::Stmt *clang_body = while_stmt->getBody();
  if (clang_body == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[while-body]: Clang WhileStmt has no "
                 "body"
              << std::endl;
    ROSE_ABORT();
  }
  clang::Expr *cond_expr = while_stmt->getCond();
  if (while_stmt->getConditionVariableDeclStmt() == nullptr &&
      cond_expr == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[while-condition-expression]: "
                 "Clang WhileStmt has no condition"
              << std::endl;
    ROSE_ABORT();
  }
  SgWhileStmt *sg_while_stmt =
      SageBuilder::buildWhileStmt_nfi(nullptr, nullptr);
  sg_while_stmt->set_parent(
      statementConstructionOwner(while_stmt, "while statement"));
  *node = sg_while_stmt;
  if (!VisitStmt(while_stmt, node)) {
    return false;
  }

  SageBuilder::pushScopeStack(sg_while_stmt);

  SgStatement *cond_stmt = nullptr;
  if (clang::DeclStmt *cond_decl = while_stmt->getConditionVariableDeclStmt()) {
    SgNode *tmp_cond = nullptr;
    {
      PendingConditionDeclarationScopeGuard condition_scope(
          p_pending_condition_declaration_scope_stack, cond_decl,
          sg_while_stmt);
      tmp_cond = Traverse(cond_decl);
    }
    cond_stmt = isSgStatement(tmp_cond);
    if (cond_stmt == nullptr) {
      std::cerr << "REX_FRONTEND_INVARIANT[while-condition-declaration]: "
                   "Clang condition translated to "
                << (tmp_cond == nullptr ? std::string("<null>")
                                        : tmp_cond->class_name())
                << " instead of SgStatement" << std::endl;
      ROSE_ABORT();
    }
    requireExactConditionDeclarationResult(cond_decl, cond_stmt, sg_while_stmt,
                                           "while");
    sg_while_stmt->set_condition_value(
        buildDeclarationConditionValue(cond_expr, sg_while_stmt));
  } else {
    SgNode *tmp_cond = Traverse(cond_expr);
    SgExpression *cond = isSgExpression(tmp_cond);
    if (cond == nullptr) {
      std::cerr << "REX_FRONTEND_INVARIANT[while-condition-expression]: "
                   "Clang condition translated to "
                << (tmp_cond == nullptr ? std::string("<null>")
                                        : tmp_cond->class_name())
                << " instead of SgExpression" << std::endl;
      ROSE_ABORT();
    }

    cond_stmt = SageBuilder::buildExprStatement_nfi(cond);
    applySourceRange(cond_stmt, cond_expr->getSourceRange());
  }

  cond_stmt->set_parent(sg_while_stmt);
  sg_while_stmt->set_condition(cond_stmt);
  requireExactAttachedConditionDeclaration(cond_stmt, sg_while_stmt, "while");

  SgStatement *body =
      translateControlledSubstatement(clang_body, sg_while_stmt, "while body");
  body = wrapStatementWithPragmas(clang_body, body);

  body->set_parent(sg_while_stmt);

  SageBuilder::popScopeStack();

  sg_while_stmt->set_body(body);

  return true;
}
