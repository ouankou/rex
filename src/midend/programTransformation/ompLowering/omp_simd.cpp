
#include <cstdio>

#include <iostream>

#include <limits>

#include <memory>

#include <stack>

#include <unordered_map>

#include <unordered_set>

#include "omp_lowering.h"

#include "omp_simd.h"

#include "sage3basic.h"

#include "sageBuilder.h"

using namespace Rose;
using namespace SageInterface;
using namespace SageBuilder;

SimdType simd_arch = Intel_AVX512;

namespace {
[[noreturn]] void failSimdContract(const char *contract, const SgNode *node,
                                   const char *reason) {
  fprintf(stderr, "REX_OMP_LOWERING_INVARIANT[%s]: node=%p kind=%s %s\n",
          contract, static_cast<const void *>(node),
          node != nullptr ? node->sage_class_name() : "null", reason);
  ROSE_ABORT();
}

void requireSimdEdge(SgNode *owner, SgNode *child, const char *contract) {
  if (owner == nullptr || child == nullptr || child->get_parent() != owner)
    failSimdContract(contract, child,
                     "does not have its required exact parent edge");
  const std::vector<SgNode *> successors =
      owner->get_traversalSuccessorContainer();
  if (std::count(successors.begin(), successors.end(), child) != 1)
    failSimdContract(contract, child,
                     "is not published on one unique structural edge");
}

bool isSupportedSimdScalarType(SgType *type) {
  if (type == nullptr)
    return false;
  type =
      type->stripType(SgType::STRIP_MODIFIER_TYPE | SgType::STRIP_TYPEDEF_TYPE);
  return isSgTypeInt(type) != nullptr || isSgTypeFloat(type) != nullptr ||
         isSgTypeDouble(type) != nullptr;
}

bool sameSimdScalarType(SgType *left, SgType *right) {
  if (!isSupportedSimdScalarType(left) || !isSupportedSimdScalarType(right))
    return false;
  left =
      left->stripType(SgType::STRIP_MODIFIER_TYPE | SgType::STRIP_TYPEDEF_TYPE);
  right = right->stripType(SgType::STRIP_MODIFIER_TYPE |
                           SgType::STRIP_TYPEDEF_TYPE);
  return SageInterface::isEquivalentType(left, right);
}

bool sameVariableIdentity(SgVariableSymbol *left, SgVariableSymbol *right) {
  return left != nullptr && right != nullptr &&
         (left == right || left->get_declaration() == right->get_declaration());
}

struct AffineIndexPlan {
  int induction_coefficient = 0;
  bool exact = false;
};

bool containsInductionReference(SgExpression *expression,
                                SgVariableSymbol *induction_symbol) {
  if (expression == nullptr || induction_symbol == nullptr)
    failSimdContract("omp-simd-address", expression,
                     "cannot inspect an incomplete induction expression");
  Rose_STL_Container<SgNode *> references =
      NodeQuery::querySubTree(expression, V_SgVarRefExp);
  for (SgNode *node : references) {
    SgVarRefExp *reference = isSgVarRefExp(node);
    if (reference == nullptr || reference->get_symbol() == nullptr)
      failSimdContract("omp-simd-address", node,
                       "contains a variable reference without exact identity");
    if (sameVariableIdentity(isSgVariableSymbol(reference->get_symbol()),
                             induction_symbol))
      return true;
  }
  return false;
}

AffineIndexPlan requireExactAffineIndex(SgExpression *expression,
                                        SgVariableSymbol *induction_symbol) {
  if (expression == nullptr || induction_symbol == nullptr)
    failSimdContract("omp-simd-address", expression,
                     "has no exact induction-aware index plan");

  if (SgMacroExpansionExp *macro = isSgMacroExpansionExp(expression)) {
    SgExpression *expanded = macro->get_expanded_expression_checked();
    requireSimdEdge(macro, expanded, "omp-simd-address");
    return requireExactAffineIndex(expanded, induction_symbol);
  }

  // An index subtree that does not mention this exact induction symbol is
  // affine only when it is a genuine typed integral constant expression.
  if (!containsInductionReference(expression, induction_symbol)) {
    if (!SageInterface::isStrictIntegerType(expression->get_type()) &&
        isSgEnumType(expression->get_type()) == nullptr)
      failSimdContract("omp-simd-address", expression,
                       "constant index does not have integral type");
    OmpSupport::requireExactIntegralConstantExpression(expression,
                                                       "omp-simd-address");
    return {0, true};
  }

  if (SgCastExp *cast = isSgCastExp(expression)) {
    SgExpression *operand = cast->get_operand_i();
    requireSimdEdge(cast, operand, "omp-simd-address");
    SgType *source_type = operand->get_type();
    SgType *target_type = cast->get_type();
    if (source_type == nullptr || target_type == nullptr ||
        !SageInterface::isStrictIntegerType(source_type) ||
        !SageInterface::isStrictIntegerType(target_type) ||
        !SageInterface::isEquivalentType(
            source_type->stripType(SgType::STRIP_MODIFIER_TYPE |
                                   SgType::STRIP_TYPEDEF_TYPE),
            target_type->stripType(SgType::STRIP_MODIFIER_TYPE |
                                   SgType::STRIP_TYPEDEF_TYPE)))
      failSimdContract("omp-simd-address", cast,
                       "casts the induction expression through a different "
                       "width or signedness");
    return requireExactAffineIndex(operand, induction_symbol);
  }
  if (SgVarRefExp *reference = isSgVarRefExp(expression)) {
    SgVariableSymbol *symbol = isSgVariableSymbol(reference->get_symbol());
    if (symbol == nullptr)
      failSimdContract("omp-simd-address", reference,
                       "index reference has no exact variable identity");
    if (!sameVariableIdentity(symbol, induction_symbol))
      failSimdContract("omp-simd-address", reference,
                       "runtime index is not the exact induction symbol");
    return {1, true};
  }
  if (isSgValueExp(expression) != nullptr)
    failSimdContract("omp-simd-address", expression,
                     "induction-bearing value node is malformed");
  if (SgUnaryOp *unary = isSgUnaryOp(expression)) {
    SgExpression *operand = unary->get_operand_i();
    requireSimdEdge(unary, operand, "omp-simd-address");
    AffineIndexPlan plan = requireExactAffineIndex(operand, induction_symbol);
    if (isSgUnaryAddOp(unary) != nullptr)
      return plan;
    if (isSgMinusOp(unary) != nullptr) {
      plan.induction_coefficient = -plan.induction_coefficient;
      return plan;
    }
    failSimdContract("omp-simd-address", unary,
                     "uses an unsupported unary index operation");
  }
  if (SgBinaryOp *binary = isSgBinaryOp(expression)) {
    if (isSgAddOp(binary) == nullptr && isSgSubtractOp(binary) == nullptr)
      failSimdContract("omp-simd-address", binary,
                       "uses a non-affine index operation");
    SgExpression *lhs = binary->get_lhs_operand();
    SgExpression *rhs = binary->get_rhs_operand();
    requireSimdEdge(binary, lhs, "omp-simd-address");
    requireSimdEdge(binary, rhs, "omp-simd-address");
    AffineIndexPlan left = requireExactAffineIndex(lhs, induction_symbol);
    AffineIndexPlan right = requireExactAffineIndex(rhs, induction_symbol);
    const int coefficient =
        isSgAddOp(binary) != nullptr
            ? left.induction_coefficient + right.induction_coefficient
            : left.induction_coefficient - right.induction_coefficient;
    if (coefficient < -1 || coefficient > 1)
      failSimdContract("omp-simd-address", binary,
                       "has an unsupported induction coefficient");
    return {coefficient, left.exact && right.exact};
  }
  failSimdContract("omp-simd-address", expression,
                   "is not an exact side-effect-free affine index");
}

SgVarRefExp *requireExactIndexedBaseIdentity(SgExpression *base) {
  if (base == nullptr)
    failSimdContract("omp-simd-address", base,
                     "indexed base has no exact expression");
  while (SgCastExp *cast = isSgCastExp(base)) {
    if (cast->cast_type() != SgCastExp::e_implicit_cast)
      failSimdContract("omp-simd-address", cast,
                       "indexed base uses an explicit pointer conversion");
    SgExpression *operand = cast->get_operand_i();
    requireSimdEdge(cast, operand, "omp-simd-address");
    SgType *source_type = operand->get_type();
    SgType *target_type = cast->get_type();
    if (source_type == nullptr || target_type == nullptr)
      failSimdContract("omp-simd-address", cast,
                       "implicit indexed-base conversion has no exact type");
    source_type = source_type->stripType(
        SgType::STRIP_MODIFIER_TYPE | SgType::STRIP_TYPEDEF_TYPE |
        SgType::STRIP_REFERENCE_TYPE | SgType::STRIP_RVALUE_REFERENCE_TYPE);
    target_type = target_type->stripType(
        SgType::STRIP_MODIFIER_TYPE | SgType::STRIP_TYPEDEF_TYPE |
        SgType::STRIP_REFERENCE_TYPE | SgType::STRIP_RVALUE_REFERENCE_TYPE);
    bool exact_conversion =
        SageInterface::isEquivalentType(source_type, target_type);
    if (!exact_conversion) {
      SgArrayType *array_type = isSgArrayType(source_type);
      SgPointerType *pointer_type = isSgPointerType(target_type);
      exact_conversion =
          array_type != nullptr && pointer_type != nullptr &&
          SageInterface::isEquivalentType(array_type->get_base_type(),
                                          pointer_type->get_base_type());
    }
    if (!exact_conversion)
      failSimdContract("omp-simd-address", cast,
                       "indexed base implicit conversion changes pointee "
                       "identity or representation");
    base = operand;
  }
  SgVarRefExp *reference = isSgVarRefExp(base);
  if (reference == nullptr || reference->get_symbol() == nullptr)
    failSimdContract("omp-simd-address", base,
                     "indexed base has no exact variable identity");
  return reference;
}

void requireExactContiguousAccess(SgExpression *expression,
                                  SgVariableSymbol *induction_symbol) {
  SgPntrArrRefExp *access = isSgPntrArrRefExp(expression);
  if (access == nullptr)
    failSimdContract("omp-simd-address", expression,
                     "is not one exact indexed access");

  size_t dimension = 0;
  SgExpression *cursor = access;
  while (SgPntrArrRefExp *current = isSgPntrArrRefExp(cursor)) {
    ++dimension;
    if (dimension > 2)
      failSimdContract("omp-simd-address", expression,
                       "has more dimensions than exact SIMD lowering supports");
    SgExpression *base = current->get_lhs_operand();
    SgExpression *index = current->get_rhs_operand();
    requireSimdEdge(current, base, "omp-simd-address");
    requireSimdEdge(current, index, "omp-simd-address");
    const AffineIndexPlan plan =
        requireExactAffineIndex(index, induction_symbol);
    const int required_coefficient = dimension == 1 ? 1 : 0;
    if (!plan.exact || plan.induction_coefficient != required_coefficient)
      failSimdContract(
          "omp-simd-address", index,
          dimension == 1
              ? "is not a coefficient-one contiguous induction index"
              : "outer array dimension depends on the SIMD induction variable");
    cursor = base;
  }
  (void)requireExactIndexedBaseIdentity(cursor);
}

void preflightSimdRvalue(SgExpression *expression,
                         SgVariableSymbol *induction_symbol,
                         SgType *assignment_type) {
  if (expression == nullptr)
    failSimdContract("omp-simd-expression", expression,
                     "is missing from the SIMD assignment");
  if (SgBinaryOp *binary = isSgBinaryOp(expression)) {
    if (isSgAddOp(binary) == nullptr && isSgSubtractOp(binary) == nullptr &&
        isSgMultiplyOp(binary) == nullptr && isSgDivideOp(binary) == nullptr &&
        isSgPntrArrRefExp(binary) == nullptr)
      failSimdContract("omp-simd-expression", binary,
                       "is an unsupported binary SIMD operation");
    if (!sameSimdScalarType(binary->get_type(), assignment_type))
      failSimdContract("omp-simd-type", binary,
                       "changes the homogeneous assignment scalar type");
    SgExpression *lhs = binary->get_lhs_operand();
    SgExpression *rhs = binary->get_rhs_operand();
    requireSimdEdge(binary, lhs, "omp-simd-expression");
    requireSimdEdge(binary, rhs, "omp-simd-expression");
    if (isSgPntrArrRefExp(binary) != nullptr) {
      if (!isSupportedSimdScalarType(binary->get_type()))
        failSimdContract("omp-simd-expression", binary,
                         "has an unsupported array-element type");
      requireExactContiguousAccess(binary, induction_symbol);
      return;
    }
    preflightSimdRvalue(lhs, induction_symbol, assignment_type);
    preflightSimdRvalue(rhs, induction_symbol, assignment_type);
    return;
  }
  if (SgCastExp *cast = isSgCastExp(expression)) {
    requireSimdEdge(cast, cast->get_operand_i(), "omp-simd-expression");
    SgExpression *operand = cast->get_operand_i();
    if (!sameSimdScalarType(cast->get_type(), operand->get_type()) ||
        !sameSimdScalarType(cast->get_type(), assignment_type))
      failSimdContract("omp-simd-type", cast,
                       "is not an exact no-op scalar cast");
    preflightSimdRvalue(operand, induction_symbol, assignment_type);
    return;
  }
  if (SgVarRefExp *reference = isSgVarRefExp(expression)) {
    SgVariableSymbol *symbol = isSgVariableSymbol(reference->get_symbol());
    if (symbol == nullptr ||
        !sameSimdScalarType(reference->get_type(), assignment_type))
      failSimdContract("omp-simd-expression", reference,
                       "has no exact homogeneous scalar symbol and type");
    if (sameVariableIdentity(symbol, induction_symbol))
      failSimdContract("omp-simd-expression", reference,
                       "uses the induction variable as a scalar broadcast");
    return;
  }
  if (isSgIntVal(expression) != nullptr ||
      isSgFloatVal(expression) != nullptr ||
      isSgDoubleVal(expression) != nullptr) {
    if (!sameSimdScalarType(expression->get_type(), assignment_type))
      failSimdContract("omp-simd-type", expression,
                       "changes the homogeneous assignment scalar type");
    return;
  }
  failSimdContract("omp-simd-expression", expression,
                   "is not supported by exact SIMD lowering");
}

bool hasExactReduction(SgOmpSimdStatement *target, SgVariableSymbol *symbol) {
  if (target == nullptr || symbol == nullptr)
    return false;
  for (SgOmpClause *clause : target->get_clauses()) {
    SgOmpReductionClause *reduction = isSgOmpReductionClause(clause);
    if (reduction == nullptr)
      continue;
    SgExprListExp *variables = reduction->get_variables();
    if (variables == nullptr)
      failSimdContract("omp-simd-reduction", reduction,
                       "has no exact variable list");
    for (SgExpression *expression : variables->get_expressions()) {
      SgVarRefExp *reference = isSgVarRefExp(expression);
      if (reference != nullptr && reference->get_symbol() == symbol)
        return true;
    }
  }
  return false;
}

struct ExactSimdLengthClauses {
  std::optional<unsigned int> simdlen;
  std::optional<unsigned int> safelen;
};

ExactSimdLengthClauses
requireExactSimdLengthClauses(SgOmpSimdStatement *target,
                              SgOmpClauseList *clause_list) {
  if (target == nullptr || clause_list == nullptr ||
      clause_list->get_parent() != target)
    failSimdContract("omp-simd-length", target,
                     "has no exact clause-list owner");

  ExactSimdLengthClauses result;
  for (SgOmpClause *clause : clause_list->get_clauses()) {
    const bool is_simdlen = isSgOmpSimdlenClause(clause) != nullptr;
    const bool is_safelen = isSgOmpSafelenClause(clause) != nullptr;
    if (!is_simdlen && !is_safelen)
      continue;
    SgOmpExpressionClause *expression_clause = isSgOmpExpressionClause(clause);
    SgExpression *expression = expression_clause != nullptr
                                   ? expression_clause->get_expression()
                                   : nullptr;
    requireSimdEdge(expression_clause, expression, "omp-simd-length");
    const unsigned long long exact_value =
        OmpSupport::requireExactPositiveIntegralConstant(
            expression,
            static_cast<unsigned long long>(
                std::numeric_limits<unsigned int>::max()),
            "omp-simd-length");
    const unsigned int value = static_cast<unsigned int>(exact_value);
    std::optional<unsigned int> &slot =
        is_simdlen ? result.simdlen : result.safelen;
    if (slot.has_value())
      failSimdContract("omp-simd-length", clause,
                       "duplicates an exact SIMD length clause");
    slot = value;
  }

  if (result.simdlen.has_value() && result.safelen.has_value() &&
      *result.simdlen > *result.safelen)
    failSimdContract("omp-simd-length", target,
                     "has simdlen greater than safelen");

  if ((result.simdlen.has_value() || result.safelen.has_value()) &&
      simd_arch != Intel_AVX512)
    failSimdContract("omp-simd-length", target,
                     "cannot apply fixed SIMD lengths to the selected exact "
                     "backend");

  if (simd_arch == Intel_AVX512 && result.simdlen.has_value() &&
      *result.simdlen != 4 && *result.simdlen != 8 && *result.simdlen != 16)
    failSimdContract("omp-simd-length", target,
                     "requests a width unsupported by the exact Intel SIMD "
                     "backend");

  if (simd_arch == Intel_AVX512 && !result.simdlen.has_value() &&
      result.safelen.has_value() && *result.safelen < 4)
    failSimdContract("omp-simd-length", target,
                     "has no Intel SIMD width that satisfies safelen");
  return result;
}

struct SimdRegionPlan {
  SageInterface::CheckedCanonicalLoopPlan loop;
  std::vector<SgExprStatement *> statements;
};

SimdRegionPlan preflightSimdRegion(SgOmpSimdStatement *target,
                                   SgForStatement *loop) {
  if (target == nullptr || loop == nullptr || target->get_scope() == nullptr)
    failSimdContract("omp-simd-region", target,
                     "is not one scoped exact canonical SIMD loop");
  const SageInterface::CheckedCanonicalLoopPlan canonical_loop =
      SageInterface::requireCheckedCanonicalLoopPlan(loop, "omp-simd-loop");

  SgOmpClauseList *clause_list = OmpSupport::getOmpClauseList(target);
  if (clause_list == nullptr || clause_list->get_parent() != target)
    failSimdContract("omp-simd-clause", target,
                     "has no exact clause-list owner");
  static_cast<void>(requireExactSimdLengthClauses(target, clause_list));
  std::unordered_set<SgOmpClause *> seen_clauses;
  for (SgOmpClause *clause : clause_list->get_clauses()) {
    if (clause == nullptr || clause->get_parent() != clause_list ||
        !seen_clauses.insert(clause).second)
      failSimdContract("omp-simd-clause", clause,
                       "does not have one unique clause-list edge");
    if (isSgOmpPrivateClause(clause) == nullptr &&
        isSgOmpReductionClause(clause) == nullptr &&
        isSgOmpSimdlenClause(clause) == nullptr &&
        isSgOmpSafelenClause(clause) == nullptr)
      failSimdContract("omp-simd-clause", clause,
                       "is not implemented by exact SIMD lowering");
    if (SgOmpVariablesClause *variables_clause =
            isSgOmpVariablesClause(clause)) {
      SgExprListExp *variables = variables_clause->get_variables();
      requireSimdEdge(variables_clause, variables, "omp-simd-clause");
      if (variables->get_expressions().empty())
        failSimdContract("omp-simd-clause", variables_clause,
                         "has an empty exact variable list");
      std::unordered_set<SgVariableSymbol *> seen_symbols;
      for (SgExpression *expression : variables->get_expressions()) {
        SgVarRefExp *reference = isSgVarRefExp(expression);
        SgVariableSymbol *symbol =
            reference != nullptr ? isSgVariableSymbol(reference->get_symbol())
                                 : nullptr;
        if (symbol == nullptr || expression->get_parent() != variables ||
            !seen_symbols.insert(symbol).second)
          failSimdContract("omp-simd-clause", expression,
                           "is not one unique exact clause variable");
      }
    }
    if (SgOmpExpressionClause *expression_clause =
            isSgOmpExpressionClause(clause)) {
      SgExpression *expression = expression_clause->get_expression();
      requireSimdEdge(expression_clause, expression, "omp-simd-clause");
    }
    if (SgOmpReductionClause *reduction = isSgOmpReductionClause(clause)) {
      switch (reduction->get_identifier()) {
      case SgOmpClause::e_omp_reduction_plus:
      case SgOmpClause::e_omp_reduction_minus:
      case SgOmpClause::e_omp_reduction_mul:
        break;
      default:
        failSimdContract("omp-simd-reduction", reduction,
                         "uses an unsupported reduction operator");
      }
    }
  }
  SgVariableSymbol *induction_symbol = &canonical_loop.inductionSymbol();
  if (induction_symbol->get_declaration() != &canonical_loop.induction())
    failSimdContract("omp-simd-region", loop,
                     "has no checked canonical induction identity");

  SgStatement *body = loop->get_loop_body();
  requireSimdEdge(loop, body, "omp-simd-body");
  std::vector<SgExprStatement *> statements;
  if (SgExprStatement *statement = isSgExprStatement(body)) {
    statements.push_back(statement);
  } else if (SgBasicBlock *block = isSgBasicBlock(body)) {
    if (block->get_statements().empty())
      failSimdContract("omp-simd-body", block, "has no SIMD assignments");
    for (SgStatement *child : block->get_statements()) {
      requireSimdEdge(block, child, "omp-simd-body");
      SgExprStatement *statement = isSgExprStatement(child);
      if (statement == nullptr)
        failSimdContract("omp-simd-body", child,
                         "is not a direct SIMD assignment statement");
      statements.push_back(statement);
    }
  } else {
    failSimdContract("omp-simd-body", body,
                     "is not one assignment or an exact assignment block");
  }

  for (SgExprStatement *statement : statements) {
    SgBinaryOp *assignment = isSgBinaryOp(statement->get_expression());
    if (assignment == nullptr || (isSgAssignOp(assignment) == nullptr &&
                                  isSgPlusAssignOp(assignment) == nullptr &&
                                  isSgMinusAssignOp(assignment) == nullptr &&
                                  isSgMultAssignOp(assignment) == nullptr &&
                                  isSgDivAssignOp(assignment) == nullptr))
      failSimdContract("omp-simd-assignment", statement,
                       "does not own one supported assignment operator");
    requireSimdEdge(statement, assignment, "omp-simd-assignment");
    SgExpression *lhs = assignment->get_lhs_operand();
    SgExpression *rhs = assignment->get_rhs_operand();
    requireSimdEdge(assignment, lhs, "omp-simd-assignment");
    requireSimdEdge(assignment, rhs, "omp-simd-assignment");
    if (SgVarRefExp *reference = isSgVarRefExp(lhs)) {
      SgVariableSymbol *symbol = isSgVariableSymbol(reference->get_symbol());
      if (!hasExactReduction(target, symbol))
        failSimdContract("omp-simd-reduction", reference,
                         "scalar assignment has no exact reduction clause");
    } else if (isSgPntrArrRefExp(lhs) == nullptr) {
      failSimdContract("omp-simd-assignment", lhs,
                       "left operand is neither an array element nor a "
                       "reduction variable");
    }
    if (isSgPntrArrRefExp(lhs) != nullptr)
      requireExactContiguousAccess(lhs, induction_symbol);
    if (!isSupportedSimdScalarType(lhs->get_type()))
      failSimdContract("omp-simd-assignment", lhs,
                       "has an unsupported destination type");
    preflightSimdRvalue(rhs, induction_symbol, lhs->get_type());
  }
  return {canonical_loop, std::move(statements)};
}

struct DeleteDetachedSimdSemanticAst {
  void operator()(SgProject *project) const {
    if (project == nullptr)
      return;
    if (project->get_parent() != nullptr)
      failSimdContract("omp-simd-raii", project,
                       "semantic sandbox unexpectedly became attached");
    SageInterface::deleteAST(project);
  }
};

class DetachedSimdBackendPlan {
public:
  explicit DetachedSimdBackendPlan(SgBasicBlock *region) : region_(region) {
    if (region_ == nullptr || region_->get_parent() != nullptr)
      failSimdContract("omp-simd-raii", region_,
                       "detached backend plan is not exactly owned");
  }

  ~DetachedSimdBackendPlan() {
    if (!released_ && region_ != nullptr) {
      if (region_->get_parent() != nullptr)
        failSimdContract("omp-simd-raii", region_,
                         "unreleased backend region became attached");
      SageInterface::deleteAST(region_);
    }
  }

  DetachedSimdBackendPlan(const DetachedSimdBackendPlan &) = delete;
  DetachedSimdBackendPlan &operator=(const DetachedSimdBackendPlan &) = delete;

  SgBasicBlock *releaseRegion() {
    if (released_ || region_ == nullptr || region_->get_parent() != nullptr)
      failSimdContract("omp-simd-raii", region_,
                       "backend region cannot be transferred exactly once");
    released_ = true;
    return region_;
  }

private:
  SgBasicBlock *region_ = nullptr;
  bool released_ = false;
};

bool isStructurallyRetained(const SgNode *node, const SgNode *region) {
  std::unordered_set<const SgNode *> seen;
  for (const SgNode *current = node; current != nullptr;
       current = current->get_parent()) {
    if (!seen.insert(current).second)
      failSimdContract("omp-simd-rehome", current,
                       "has a cyclic structural owner chain");
    if (current == region)
      return true;
  }
  return false;
}

bool isLiveSimdAstNode(SgNode *node) {
  return node != nullptr &&
         SgNode::variantFromPool(node) != static_cast<VariantT>(0) &&
         SgNode::isLiveNode(node);
}

std::vector<SgNode *> collectRetainedSimdGraph(SgBasicBlock *region) {
  if (region == nullptr)
    failSimdContract("omp-simd-rehome", region,
                     "cannot collect a null backend region");
  std::vector<SgNode *> retained;
  std::vector<SgNode *> worklist{region};
  std::unordered_set<SgNode *> seen;
  while (!worklist.empty()) {
    SgNode *node = worklist.back();
    worklist.pop_back();
    if (!isLiveSimdAstNode(node) || !isStructurallyRetained(node, region) ||
        !seen.insert(node).second)
      continue;
    retained.push_back(node);
    for (const auto &edge : node->returnDataMemberPointers()) {
      if (isLiveSimdAstNode(edge.first) &&
          isStructurallyRetained(edge.first, region))
        worklist.push_back(edge.first);
    }
  }
  return retained;
}

void requireDetachedSimdTypeOwner(SgType *type, SgBasicBlock *region,
                                  SgProject *destination_project,
                                  SgProject *semantic_project,
                                  std::unordered_set<SgType *> &seen_types) {
  if (type == nullptr || !seen_types.insert(type).second)
    return;
  if (SageInterface::getProject(type) == semantic_project)
    failSimdContract("omp-simd-rehome", type,
                     "retains a type owned by the semantic sandbox");
  if (SgNamedType *named = isSgNamedType(type)) {
    SgDeclarationStatement *declaration = named->get_declaration();
    if (declaration == nullptr ||
        (!isStructurallyRetained(declaration, region) &&
         SageInterface::getProject(declaration) != destination_project))
      failSimdContract("omp-simd-rehome", type,
                       "has no retained or destination-owned named-type "
                       "declaration");
  }
  for (const auto &edge : type->returnDataMemberPointers()) {
    if (SgType *child = isSgType(edge.first))
      requireDetachedSimdTypeOwner(child, region, destination_project,
                                   semantic_project, seen_types);
  }
}

void rehomeAndValidateDetachedSimdGraph(
    SgBasicBlock *region, SgProject *destination_project,
    SgGlobal *destination_global, SgProject *semantic_project,
    const std::unordered_map<SgNode *, const SgNode *> &original_by_copy) {
  if (region == nullptr || destination_project == nullptr ||
      destination_global == nullptr ||
      SageInterface::getProject(destination_global) != destination_project ||
      semantic_project == nullptr || destination_project == semantic_project)
    failSimdContract("omp-simd-rehome", region,
                     "cannot rehome an incomplete backend ownership graph");

  std::unordered_map<SgNode *, SgNode *> replacements;
  for (const auto &entry : original_by_copy) {
    SgNode *copied = entry.first;
    SgNode *original = const_cast<SgNode *>(entry.second);
    if (!isLiveSimdAstNode(copied) || !isLiveSimdAstNode(original) ||
        copied == original || isStructurallyRetained(copied, region))
      continue;
    if (SageInterface::getProject(original) == destination_project)
      replacements.emplace(copied, original);
  }

  // Named-call construction inside the private semantic translation unit
  // publishes exact auxiliary prototypes there.  Commit an equivalent typed
  // ABI identity to the destination global scope, then redirect every retained
  // call edge before the sandbox is destroyed.  No raw function name or
  // unresolved symbol is allowed to cross the transaction boundary.
  const std::vector<SgNode *> provisional_retained =
      collectRetainedSimdGraph(region);
  std::unordered_set<SgFunctionSymbol *> sandbox_function_symbols;
  for (SgNode *node : provisional_retained) {
    for (const auto &member : node->returnDataMemberPointers()) {
      SgFunctionSymbol *symbol = isSgFunctionSymbol(member.first);
      if (symbol != nullptr &&
          SageInterface::getProject(symbol) == semantic_project &&
          !isStructurallyRetained(symbol, region))
        sandbox_function_symbols.insert(symbol);
    }
  }
  for (SgFunctionSymbol *sandbox_symbol : sandbox_function_symbols) {
    SgFunctionDeclaration *sandbox_declaration = isSgFunctionDeclaration(
        sandbox_symbol != nullptr ? sandbox_symbol->get_declaration()
                                  : nullptr);
    SgFunctionParameterList *sandbox_parameters =
        sandbox_declaration != nullptr
            ? sandbox_declaration->get_parameterList()
            : nullptr;
    SgFunctionType *sandbox_type = sandbox_declaration != nullptr
                                       ? sandbox_declaration->get_type()
                                       : nullptr;
    if (sandbox_declaration == nullptr || sandbox_parameters == nullptr ||
        sandbox_type == nullptr ||
        !SageInterface::hasExactSemanticAuxiliaryOwnership(sandbox_declaration))
      failSimdContract("omp-simd-rehome", sandbox_symbol,
                       "has no exact sandbox ABI declaration");

    SgFunctionSymbol *destination_symbol =
        SageInterface::lookupFunctionSymbolInParentScopes(
            sandbox_declaration->get_name(), sandbox_type, destination_global);
    if (destination_symbol == nullptr) {
      SgFunctionParameterList *destination_parameters =
          SageBuilder::buildSemanticFunctionParameterList(sandbox_parameters);
      SgFunctionDeclaration *destination_declaration =
          SageBuilder::buildNondefiningFunctionDeclaration(
              SageBuilder::function_declaration_ownership::semanticAuxiliary(),
              sandbox_declaration->get_name(), sandbox_type->get_return_type(),
              destination_parameters, destination_global);
      if (destination_declaration != nullptr)
        destination_declaration->get_declarationModifier()
            .get_storageModifier()
            .setExtern();
      destination_symbol = SageInterface::lookupFunctionSymbolInParentScopes(
          sandbox_declaration->get_name(), sandbox_type, destination_global);
      if (destination_declaration == nullptr || destination_symbol == nullptr ||
          destination_symbol->get_declaration() != destination_declaration ||
          !SageInterface::hasExactSemanticAuxiliaryOwnership(
              destination_declaration))
        failSimdContract("omp-simd-rehome", destination_declaration,
                         "could not publish the exact destination ABI "
                         "declaration");
      replacements.emplace(sandbox_declaration, destination_declaration);
    }
    if (destination_symbol == nullptr ||
        SageInterface::getProject(destination_symbol) != destination_project)
      failSimdContract("omp-simd-rehome", destination_symbol,
                       "did not resolve to the destination ABI identity");
    replacements.emplace(sandbox_symbol, destination_symbol);
  }

  struct ExactExternalEdgeRewriter : public SimpleReferenceToPointerHandler {
    const std::unordered_map<SgNode *, SgNode *> &replacements;
    explicit ExactExternalEdgeRewriter(
        const std::unordered_map<SgNode *, SgNode *> &map)
        : replacements(map) {}
    void operator()(SgNode *&edge, const SgName &, bool) override {
      auto replacement = replacements.find(edge);
      if (replacement != replacements.end())
        edge = replacement->second;
    }
  } rewriter(replacements);

  std::vector<SgNode *> retained = collectRetainedSimdGraph(region);
  for (SgNode *node : retained)
    node->processDataMemberReferenceToPointers(&rewriter);

  retained = collectRetainedSimdGraph(region);
  std::unordered_set<SgType *> seen_types;
  for (SgNode *node : retained) {
    // An expression list is an owned syntax container for argument or
    // initializer elements; it deliberately has no standalone semantic value
    // type. Validate the types of its retained element expressions instead.
    if (SgExpression *expression = isSgExpression(node);
        expression != nullptr && isSgExprListExp(expression) == nullptr)
      requireDetachedSimdTypeOwner(expression->get_type(), region,
                                   destination_project, semantic_project,
                                   seen_types);
    if (SgInitializedName *name = isSgInitializedName(node)) {
      requireDetachedSimdTypeOwner(name->get_type(), region,
                                   destination_project, semantic_project,
                                   seen_types);
      SgScopeStatement *scope = name->get_scope();
      if (scope == nullptr ||
          (!isStructurallyRetained(scope, region) &&
           SageInterface::getProject(scope) != destination_project))
        failSimdContract("omp-simd-rehome", name,
                         "has no retained or destination-owned semantic scope");
    }
    if (SgDeclarationStatement *declaration = isSgDeclarationStatement(node)) {
      SgScopeStatement *scope = declaration->get_scope();
      if (declaration->hasExplicitScope() &&
          (scope == nullptr ||
           (!isStructurallyRetained(scope, region) &&
            SageInterface::getProject(scope) != destination_project)))
        failSimdContract("omp-simd-rehome", declaration,
                         "has no retained or destination-owned declaration "
                         "scope");
    }

    for (const auto &member : node->returnDataMemberPointers()) {
      SgNode *edge = member.first;
      if (!isLiveSimdAstNode(edge) || isStructurallyRetained(edge, region))
        continue;
      if (SageInterface::getProject(edge) == semantic_project)
        failSimdContract("omp-simd-rehome", edge,
                         "is an escaping edge into the semantic sandbox");
      if (SageInterface::getProject(edge) == destination_project)
        continue;
      if (SgType *type = isSgType(edge)) {
        requireDetachedSimdTypeOwner(type, region, destination_project,
                                     semantic_project, seen_types);
        continue;
      }
      if (isSg_File_Info(edge) != nullptr)
        continue;
      failSimdContract("omp-simd-rehome", edge,
                       "is neither retained nor destination-owned");
    }
  }
}

void requireCommittedSimdPosition(Sg_File_Info *position, SgNode *owner,
                                  int destination_file_id, const char *role) {
  if (position == nullptr || owner == nullptr ||
      position->get_parent() != owner || position->isShared() ||
      position->get_physical_file_id() != destination_file_id) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[omp-simd-file-owner]: role=%s "
            "owner=%p position=%p parent=%p physical=%d expected=%d\n",
            role, static_cast<void *>(owner), static_cast<void *>(position),
            position != nullptr ? static_cast<void *>(position->get_parent())
                                : nullptr,
            position != nullptr ? position->get_physical_file_id() : -1,
            destination_file_id);
    ROSE_ABORT();
  }
}

void validateCommittedSimdGraph(SgBasicBlock *region, SgSourceFile *file) {
  if (region == nullptr || file == nullptr ||
      SageInterface::getEnclosingSourceFile(region, true) != file ||
      SageInterface::getProject(region) != file->get_project())
    failSimdContract("omp-simd-file-owner", region,
                     "is not attached to its exact destination source file");
  Sg_File_Info *source_position = file->get_file_info();
  if (source_position == nullptr || source_position->get_physical_file_id() < 0)
    failSimdContract("omp-simd-file-owner", file,
                     "destination source file has no physical identity");
  const int destination_file_id = source_position->get_physical_file_id();

  const std::vector<SgNode *> retained = collectRetainedSimdGraph(region);
  const auto &token_map = file->get_tokenSubsequenceMap();
  const auto &macro_map = file->get_macroExpansionMap();
  const auto &whitespace_map = file->get_representativeWhitespaceStatementMap();
  for (SgNode *node : retained) {
    SgStatement *statement = isSgStatement(node);
    SgScopeStatement *scope = isSgScopeStatement(node);
    const bool appears_in_whitespace_map =
        (scope != nullptr && whitespace_map.count(scope) != 0) ||
        (statement != nullptr &&
         std::any_of(whitespace_map.begin(), whitespace_map.end(),
                     [statement](const auto &entry) {
                       return entry.second == statement;
                     }));
    if (token_map.count(node) != 0 ||
        (statement != nullptr && macro_map.count(statement) != 0) ||
        appears_in_whitespace_map)
      failSimdContract(
          "omp-simd-file-owner", node,
          "retains a source token, macro, or whitespace side-table "
          "identity after commit");
    if (SgLocatedNode *located = isSgLocatedNode(node)) {
      requireCommittedSimdPosition(located->get_file_info(), located,
                                   destination_file_id, "file");
      requireCommittedSimdPosition(located->get_startOfConstruct(), located,
                                   destination_file_id, "start");
      requireCommittedSimdPosition(located->get_endOfConstruct(), located,
                                   destination_file_id, "end");
      if (SgExpression *expression = isSgExpression(located)) {
        if (expression->get_operatorPosition() != nullptr)
          requireCommittedSimdPosition(expression->get_operatorPosition(),
                                       expression, destination_file_id,
                                       "operator");
      }
    }
    if (SgInitializedName *name = isSgInitializedName(node)) {
      requireCommittedSimdPosition(name->get_file_info(), name,
                                   destination_file_id, "name-file");
      requireCommittedSimdPosition(name->get_startOfConstruct(), name,
                                   destination_file_id, "name-start");
      requireCommittedSimdPosition(name->get_endOfConstruct(), name,
                                   destination_file_id, "name-end");
    }
  }
}

std::unique_ptr<DetachedSimdBackendPlan>
buildDetachedSimdBackendPlan(SgOmpSimdStatement *source_target) {
  if (source_target == nullptr)
    failSimdContract("omp-simd-backend", source_target,
                     "cannot plan a null SIMD directive");

  SgSourceFile *source_file =
      SageInterface::getEnclosingSourceFile(source_target);
  if (source_file == nullptr ||
      (!source_file->get_C_only() && !source_file->get_C99_only() &&
       !source_file->get_Cxx_only()))
    failSimdContract("omp-simd-backend", source_target,
                     "has no exact supported source-language owner");

  SgProject *destination_project = SageInterface::getProject(source_target);
  if (destination_project == nullptr)
    failSimdContract("omp-simd-backend", source_target,
                     "has no exact destination project");

  std::unique_ptr<SgProject, DeleteDetachedSimdSemanticAst> semantic_owner(
      new SgProject());
  semantic_owner->get_fileList().clear();
  semantic_owner->set_compileOnly(true);
  const bool cxx_source = source_file->get_Cxx_only();
  semantic_owner->get_originalCommandLineArgumentList() = {
      cxx_source ? "c++" : "cc", "-c"};
  SgSourceFile *semantic_source = SageBuilder::buildGeneratedSourceFile(
      cxx_source ? "__rex_detached_simd_backend.cpp"
                 : "__rex_detached_simd_backend.c",
      semantic_owner.get());
  SgGlobal *semantic_global =
      semantic_source != nullptr ? semantic_source->get_globalScope() : nullptr;
  if (semantic_source == nullptr || semantic_global == nullptr ||
      semantic_source->get_project() != semantic_owner.get() ||
      SageInterface::getProject(semantic_global) != semantic_owner.get())
    failSimdContract("omp-simd-backend", semantic_source,
                     "could not construct an owned semantic translation unit");

  SgFunctionDeclaration *source_function =
      SageInterface::getEnclosingFunctionDeclaration(source_target, true);
  SgFunctionParameterList *source_parameters =
      source_function != nullptr ? source_function->get_parameterList()
                                 : nullptr;
  if (source_function == nullptr || source_parameters == nullptr ||
      source_parameters->get_parent() != source_function)
    failSimdContract("omp-simd-backend", source_target,
                     "has no exact enclosing function parameter identity");

  // The copied function body is normalized inside a private semantic
  // translation unit before it is committed.  Publish an exact shadow of the
  // enclosing parameter environment in that unit so ordinary lexical lookup
  // remains valid while statements acquire new block scopes.  These identities
  // are transaction-local and are remapped to their source counterparts before
  // the transformed region leaves the sandbox.
  SgFunctionParameterList *parameters =
      buildSemanticFunctionParameterList(source_parameters);
  SgFunctionDeclaration *sandbox_function = buildDefiningFunctionDeclaration(
      function_declaration_ownership::sourceLexicalIn(semantic_global),
      "__rex_detached_simd_backend", buildVoidType(), parameters,
      semantic_global);
  SgFunctionDefinition *definition = sandbox_function != nullptr
                                         ? sandbox_function->get_definition()
                                         : nullptr;
  SgBasicBlock *region =
      definition != nullptr ? definition->get_body() : nullptr;
  if (sandbox_function == nullptr ||
      sandbox_function->get_parent() != semantic_global ||
      definition == nullptr || definition->get_parent() != sandbox_function ||
      region == nullptr || region->get_parent() != definition)
    failSimdContract("omp-simd-backend", sandbox_function,
                     "could not construct an exact detached semantic sandbox");

  SgBasicBlock *source_scope = isSgBasicBlock(source_target->get_scope());
  if (source_scope == nullptr ||
      !isStructurallyRetained(source_target, source_scope))
    failSimdContract("omp-simd-backend", source_target,
                     "has no exact lexical block to copy atomically");

  SgTreeCopy copy_help;
  SgBasicBlock *copied_scope = isSgBasicBlock(source_scope->copy(copy_help));
  auto copied_target_entry = copy_help.get_copiedNodeMap().find(source_target);
  SgOmpSimdStatement *target =
      copied_target_entry != copy_help.get_copiedNodeMap().end()
          ? isSgOmpSimdStatement(copied_target_entry->second)
          : nullptr;
  if (copied_scope == nullptr || copied_scope->get_parent() != nullptr ||
      target == nullptr || !isStructurallyRetained(target, copied_scope))
    failSimdContract("omp-simd-backend", target,
                     "could not copy the exact SIMD lexical scope");

  std::unordered_map<SgNode *, const SgNode *> original_by_copy;
  for (const auto &entry : copy_help.get_copiedNodeMap()) {
    if (entry.first != nullptr && entry.second != nullptr &&
        entry.first != entry.second)
      original_by_copy.emplace(entry.second, entry.first);
  }
  const SgInitializedNamePtrList &source_parameter_names =
      source_parameters->get_args();
  const SgInitializedNamePtrList &sandbox_parameter_names =
      parameters->get_args();
  if (source_parameter_names.size() != sandbox_parameter_names.size())
    failSimdContract("omp-simd-backend", parameters,
                     "did not preserve the exact parameter cardinality");
  for (size_t index = 0; index < source_parameter_names.size(); ++index) {
    SgInitializedName *source_name = source_parameter_names[index];
    SgInitializedName *sandbox_name = sandbox_parameter_names[index];
    SgVariableSymbol *source_symbol =
        source_name != nullptr
            ? isSgVariableSymbol(
                  source_name->search_for_symbol_from_symbol_table())
            : nullptr;
    SgVariableSymbol *sandbox_symbol =
        sandbox_name != nullptr
            ? isSgVariableSymbol(
                  sandbox_name->search_for_symbol_from_symbol_table())
            : nullptr;
    if (source_name == nullptr || sandbox_name == nullptr ||
        source_name->get_name() != sandbox_name->get_name() ||
        source_name->get_type() != sandbox_name->get_type() ||
        source_symbol == nullptr || sandbox_symbol == nullptr ||
        source_symbol->get_declaration() != source_name ||
        sandbox_symbol->get_declaration() != sandbox_name)
      failSimdContract("omp-simd-backend", sandbox_name,
                       "did not publish one exact shadow parameter identity");
    original_by_copy.emplace(sandbox_name, source_name);
    original_by_copy.emplace(sandbox_symbol, source_symbol);
  }

  definition->set_body(nullptr);
  region->set_parent(nullptr);
  definition->set_body(copied_scope);
  copied_scope->set_parent(definition);
  SageInterface::deleteAST(region);
  region = copied_scope;
  if (definition->get_body() != region || region->get_parent() != definition ||
      SageInterface::getProject(region) != semantic_owner.get())
    failSimdContract("omp-simd-backend", region,
                     "could not attach the copied lexical scope to its "
                     "semantic project");

  // The lexical copy is a private transformation product owned by the
  // semantic sandbox, not source text owned by the original translation unit.
  // Publish that provenance before invoking any mutation API so ownership is
  // decided from the copy's actual project instead of its copied source path.
  // Its final physical output owner is already exact: the source directive
  // that this all-or-nothing plan will replace.
  SageInterface::setSourcePositionForTransformation(region);
  SageInterface::publishGeneratedSubtreeOutputOwner(region, source_target);

  SgForStatement *loop = OmpSupport::requireExactAssociatedForLoop(
      target, target->get_body(), OmpSupport::AssociatedLoopPathContract::Simd,
      "omp-simd-backend-loop");
  const SimdRegionPlan copied_plan = preflightSimdRegion(target, loop);

  std::unique_ptr<OmpSimdCompiler> compiler =
      std::make_unique<OmpSimdCompiler>();
  compiler->setTarget(target);
  compiler->setForLoop(loop);

  // Every backend operates only on this detached copy.  Writer rejection,
  // normalization, generated declarations, and increment rewrites therefore
  // cannot partially mutate the source tree.
  SageInterface::forLoopNormalization(loop);
  const SageInterface::CheckedCanonicalLoopPlan normalized_loop =
      SageInterface::requireCheckedCanonicalLoopPlan(
          loop, "omp-simd-normalized-loop");
  if (&normalized_loop.inductionSymbol() ==
          &copied_plan.loop.inductionSymbol() ||
      normalized_loop.induction().get_parent() == nullptr)
    failSimdContract("omp-simd-normalized-loop", loop,
                     "did not publish one distinct normalized induction "
                     "identity");
  compiler->setInductionSymbol(&normalized_loop.inductionSymbol());
  compiler->omp_simd_pass1();
  compiler->omp_simd_pass2();
  if (simd_arch == Addr3 || simd_arch == ArmAddr3) {
    SgStatement *loop_body = getLoopBody(loop);
    replaceStatement(loop_body, compiler->releaseBlock(), true);
  } else if (simd_arch == Intel_AVX512) {
    omp_simd_write_intel(target, loop, compiler->getIR(),
                         compiler->omp_simd_get_length());
  } else if (simd_arch == Arm_SVE2) {
    omp_simd_write_arm(target, loop, compiler->getIR());
  } else {
    failSimdContract("omp-simd-architecture", source_target,
                     "selected an unsupported exact SIMD backend");
  }

  SgStatement *replacement = target->get_body();
  if (replacement == nullptr || replacement->get_parent() != target)
    failSimdContract("omp-simd-backend", target,
                     "detached directive lost its exact transformed body");
  target->set_body(nullptr);
  replacement->set_parent(nullptr);
  replaceStatement(target, replacement, true);
  if (target->get_parent() != nullptr) {
    failSimdContract("omp-simd-backend", target,
                     "copied directive remained attached after replacement");
  }
  SageInterface::deleteAST(target,
                           SageInterface::DeleteAstMode::kRequireIsolated);
  if (SgNode::isLiveNode(target))
    failSimdContract("omp-simd-backend", target,
                     "copied directive remained live after replacement");
  SgStatement *replacement_owner = isSgStatement(replacement->get_parent());
  requireSimdEdge(replacement_owner, replacement, "omp-simd-backend");

  SgNullStatement *placeholder = buildNullStatement();
  replacement_owner->replace_statement(replacement, placeholder);
  if (placeholder->get_parent() == nullptr)
    placeholder->set_parent(replacement_owner);
  replacement->set_parent(nullptr);
  requireSimdEdge(replacement_owner, placeholder, "omp-simd-backend");
  const std::vector<SgNode *> remaining_children =
      replacement_owner->get_traversalSuccessorContainer();
  if (std::count(remaining_children.begin(), remaining_children.end(),
                 replacement) != 0)
    failSimdContract("omp-simd-backend", replacement,
                     "remains published in the semantic sandbox");

  SgBasicBlock *output_region = buildBasicBlock(replacement);
  requireSimdEdge(output_region, replacement, "omp-simd-backend");
  rehomeAndValidateDetachedSimdGraph(output_region, destination_project,
                                     source_file->get_globalScope(),
                                     semantic_owner.get(), original_by_copy);

  return std::make_unique<DetachedSimdBackendPlan>(output_region);
}
} // namespace

std::string OmpSimdCompiler::simdGenName(int type) {
  std::string prefix = "__vec";
  if (type == 1)
    prefix = "__ptr";
  else if (type == 2)
    prefix = "__part";

  if (next_name_id_ == std::numeric_limits<unsigned int>::max())
    failSimdContract("omp-simd-name", nullptr,
                     "temporary-name counter overflowed");
  std::string name = prefix + std::to_string(next_name_id_);
  ++next_name_id_;
  return name;
}

SgPntrArrRefExp *OmpSimdCompiler::omp_simd_convert_ptr(SgExpression *pntr_exp) {
  // Perform a check and convert a multi-dimensional array to a 1-D array
  // reference
  SgPntrArrRefExp *pntr = isSgPntrArrRefExp(pntr_exp);
  if (pntr == nullptr)
    failSimdContract("omp-simd-pointer", pntr_exp,
                     "is not one exact array reference");
  SgType *result_type = pntr->get_type();
  SgExpression *lval = pntr->get_lhs_operand();
  requireSimdEdge(pntr, lval, "omp-simd-pointer");
  requireSimdEdge(pntr, pntr->get_rhs_operand(), "omp-simd-pointer");

  // A stack is used to hold the index expressions
  std::stack<SgExpression *> stack;
  SgPntrArrRefExp *array;

  // Iterate down the tree and store the index expressions
  while (isSgPntrArrRefExp(lval) != nullptr) {
    SgExpression *current_rval = copyExpression(pntr->get_rhs_operand());
    stack.push(current_rval);

    pntr = isSgPntrArrRefExp(lval);
    requireSimdEdge(pntr, pntr->get_lhs_operand(), "omp-simd-pointer");
    requireSimdEdge(pntr, pntr->get_rhs_operand(), "omp-simd-pointer");
    lval = pntr->get_lhs_operand();
  }

  SgExpression *current_rval = copyExpression(pntr->get_rhs_operand());
  stack.push(current_rval);

  // There will always be at least one value on the stack; if there's only
  // one, we can just copy the expression and be done it with it. Otherwise,
  // we have to build a reference
  if (stack.size() == 1) {
    array = static_cast<SgPntrArrRefExp *>(copyExpression(pntr));
  } else {
    SgVarRefExp *lastPntr = NULL;

    // Iterate down the stack
    while (stack.size() > 1) {
      SgExpression *index = stack.top();
      stack.pop();

      // SgType *baseType = buildFloatType();
      SgType *baseType = pntr->get_type()->findBaseType();
      SgType *type = buildPointerType(baseType);

      std::string name = simdGenName(1);
      SgVariableDeclaration *vd =
          buildVariableDeclaration(name, type, NULL, new_block);
      appendStatement(vd, new_block);

      SgVarRefExp *va = buildVarRefExp(name, new_block);
      lastPntr = buildVarRefExp(name, new_block);

      SgPntrArrRefExp *pntr =
          buildPntrArrRefExp(copyExpression(lval), index, type);
      SgExprStatement *expr = buildAssignStatement(va, pntr);
      appendStatement(expr, new_block);
    }

    SgExpression *last_index = stack.top();
    stack.pop();

    array = buildPntrArrRefExp(lastPntr, last_index, result_type);
  }

  return array;
}

void OmpSimdCompiler::omp_simd_build_ptr_assign(SgExpression *pntr_exp,
                                                SgType *type) {
  if (induction_symbol == nullptr)
    failSimdContract("omp-simd-address", for_loop,
                     "has no checked induction-symbol plan");
  requireExactContiguousAccess(pntr_exp, induction_symbol);
  SgPntrArrRefExp *array = omp_simd_convert_ptr(pntr_exp);

  // Now that we are done, build the assignment
  std::string name = simdGenName();
  nameStack.push(name);

  SgVariableDeclaration *vd =
      buildVariableDeclaration(name, type, NULL, new_block);
  appendStatement(vd, new_block);

  SgVarRefExp *va = buildVarRefExp(name, new_block);
  SgExprStatement *expr = buildAssignStatement(va, array);
  appendStatement(expr, new_block);
}

void OmpSimdCompiler::omp_simd_build_scalar_assign(SgExpression *node,
                                                   SgType *type) {
  if (node->variantT() == V_SgVarRefExp) {
    SgVarRefExp *ref = static_cast<SgVarRefExp *>(node);
    std::string name = ref->get_symbol()->get_name().getString();

    if (name.rfind("__part", 0) == 0) {
      nameStack.push(name);
      return;
    }
  }

  std::string name = simdGenName();
  nameStack.push(name);

  // Build the assignment
  SgExpression *expr = NULL;

  switch (node->variantT()) {
  case V_SgIntVal: {
    SgIntVal *val = static_cast<SgIntVal *>(node);

    if (type->variantT() == V_SgTypeInt)
      expr = buildIntVal(val->get_value());
    else if (type->variantT() == V_SgTypeDouble)
      expr = buildDoubleVal(val->get_value());
    else
      expr = buildFloatVal(val->get_value());
  } break;

  case V_SgFloatVal: {
    SgFloatVal *val = static_cast<SgFloatVal *>(node);

    if (type->variantT() == V_SgTypeInt)
      expr = buildIntVal(val->get_value());
    else if (type->variantT() == V_SgTypeDouble)
      expr = buildDoubleVal(val->get_value());
    else
      expr = buildFloatVal(val->get_value());
  } break;

  case V_SgDoubleVal: {
    SgDoubleVal *val = static_cast<SgDoubleVal *>(node);

    if (type->variantT() == V_SgTypeInt)
      expr = buildIntVal(val->get_value());
    else if (type->variantT() == V_SgTypeDouble)
      expr = buildDoubleVal(val->get_value());
    else
      expr = buildFloatVal(val->get_value());
  } break;

  case V_SgVarRefExp: {
    expr = copyExpression(node);
  } break;

  default: { // expr = copyExpression(node);
    failSimdContract("omp-simd-scalar", node,
                     "is an unsupported scalar operand");
  }
  }

  // Build the variable declaration
  SgVariableDeclaration *vd =
      buildVariableDeclaration(name, type, NULL, new_block);
  appendStatement(vd, new_block);

  // Build the assignment
  SgVarRefExp *va = buildVarRefExp(name, new_block);
  SgExprStatement *assign = buildAssignStatement(va, expr);
  appendStatement(assign, new_block);
}

// Builds the math operations
void OmpSimdCompiler::omp_simd_build_math(VariantT op_type, SgType *type) {
  if (nameStack.size() < 2)
    failSimdContract("omp-simd-three-address", nullptr,
                     "math operation has fewer than two exact operands");
  std::string name = simdGenName();

  SgVariableDeclaration *vd =
      buildVariableDeclaration(name, type, NULL, new_block);
  appendStatement(vd, new_block);

  // Build the assignment
  // The variables
  const std::string rhs_name = nameStack.top();
  nameStack.pop();

  const std::string lhs_name = nameStack.top();
  nameStack.pop();

  SgVarRefExp *lhs = buildVarRefExp(lhs_name, new_block);
  SgVarRefExp *rhs = buildVarRefExp(rhs_name, new_block);

  // The operators
  SgBinaryOp *op = NULL;

  switch (op_type) {
  case V_SgAddOp:
    op = buildAddOp(lhs, rhs, type);
    break;
  case V_SgSubtractOp:
    op = buildSubtractOp(lhs, rhs, type);
    break;
  case V_SgMultiplyOp:
    op = buildMultiplyOp(lhs, rhs, type);
    break;
  case V_SgDivideOp:
    op = buildDivideOp(lhs, rhs, type);
    break;
  default: {
    failSimdContract("omp-simd-three-address", nullptr,
                     "received an unsupported math operator");
  }
  }

  // The rest of the assignment
  SgExprListExp *exprList = buildExprListExp(op);
  SgVarRefExp *va = buildVarRefExp(name, new_block);
  SgExprStatement *expr = buildAssignStatement(va, exprList);
  appendStatement(expr, new_block);

  nameStack.push(name);
}

void OmpSimdCompiler::omp_simd_build_3addr(SgExpression *rval, SgType *type) {
  if (rval == nullptr || !isSupportedSimdScalarType(type))
    failSimdContract("omp-simd-three-address", rval,
                     "has a missing expression or unsupported scalar type");
  switch (rval->variantT()) {
  case V_SgAddOp:
  case V_SgSubtractOp:
  case V_SgMultiplyOp:
  case V_SgDivideOp: {
    // Build math
    SgBinaryOp *op = static_cast<SgBinaryOp *>(rval);
    omp_simd_build_3addr(op->get_lhs_operand(), type);
    omp_simd_build_3addr(op->get_rhs_operand(), type);
    omp_simd_build_math(rval->variantT(), type);
  } break;

  case V_SgPntrArrRefExp: {
    omp_simd_build_ptr_assign(rval, type);
  } break;

  case V_SgVarRefExp:
  case V_SgIntVal:
  case V_SgFloatVal:
  case V_SgDoubleVal: {
    omp_simd_build_scalar_assign(rval, type);
  } break;

  case V_SgCastExp: {
    SgCastExp *cast = static_cast<SgCastExp *>(rval);
    omp_simd_build_3addr(cast->get_operand(), type);
  } break;

  default: {
    failSimdContract("omp-simd-three-address", rval,
                     "contains an unsupported expression kind");
  }
  }
}

// This scans an OMP SIMD statement for a reduction clause containing a variable
// matching that of the parameter. If it is found, the modifier (operator) is
// converted to a char for easier processing, and returned
//
char OmpSimdCompiler::omp_simd_get_reduction_mod(SgVarRefExp *var) {
  if (target == nullptr || var == nullptr || var->get_symbol() == nullptr)
    failSimdContract("omp-simd-reduction", var,
                     "has no exact directive or variable symbol");
  SgOmpClausePtrList clauses = target->get_clauses();
  for (size_t i = 0; i < clauses.size(); i++) {
    if (clauses.at(i)->variantT() != V_SgOmpReductionClause)
      continue;

    SgOmpReductionClause *rc =
        static_cast<SgOmpReductionClause *>(clauses.at(i));
    SgExpressionPtrList vars = rc->get_variables()->get_expressions();
    bool found = false;

    for (size_t j = 0; j < vars.size(); j++) {
      SgVarRefExp *v_current = isSgVarRefExp(vars.at(j));
      if (v_current == nullptr || v_current->get_symbol() == nullptr)
        failSimdContract("omp-simd-reduction", vars.at(j),
                         "is not one exact reduction variable reference");
      if (v_current->get_symbol() == var->get_symbol()) {
        found = true;
        break;
      }
    }

    if (!found)
      continue;

    SgOmpClause::omp_reduction_identifier_enum omp_op = rc->get_identifier();
    switch (omp_op) {
    case SgOmpClause::e_omp_reduction_plus:
      return '+';
    case SgOmpClause::e_omp_reduction_minus:
      return '-';
    case SgOmpClause::e_omp_reduction_mul:
      return '*';
    default:
      failSimdContract("omp-simd-reduction", rc,
                       "uses an unsupported reduction operator");
    }
  }

  failSimdContract("omp-simd-reduction", var,
                   "does not have an exact matching reduction clause");
}

// This runs the first pass of the SIMD lowering. This pass converts
// multi-dimensional arrays and then converts the statements to 3-address scalar
// code
//
// The main purpose of this function is to break each expression between the
// load and store
void OmpSimdCompiler::omp_simd_pass1() {
  // Get the loop body
  SgStatement *loop_body = getLoopBody(for_loop);
  Rose_STL_Container<SgNode *> bodyList =
      NodeQuery::querySubTree(loop_body, V_SgExprStatement);
  std::vector<SgAssignOp *> assign_list;
  std::vector<SgStatement *> reduction_statements;

  for (Rose_STL_Container<SgNode *>::iterator i = bodyList.begin();
       i != bodyList.end(); i++) {
    if (!nameStack.empty())
      failSimdContract("omp-simd-pass1", *i,
                       "started an assignment with stale temporary results");
    SgExprStatement *source_statement = isSgExprStatement(*i);
    SgExpression *expr = source_statement != nullptr
                             ? source_statement->get_expression()
                             : nullptr;
    requireSimdEdge(source_statement, expr, "omp-simd-pass1");
    SgBinaryOp *op = isSgBinaryOp(expr);
    if (op == nullptr)
      failSimdContract("omp-simd-pass1", expr,
                       "is not one exact assignment expression");

    // Copy the store expression and determine the proper type
    SgExpression *orig = static_cast<SgExpression *>(op->get_lhs_operand());
    SgExpression *dest;
    SgType *type;

    char reduction_mod = 0;
    bool need_partial = false;
    std::string reduction_name = "";

    // If we have a variable, we need to indicate a partial sum variable
    // These are prefixed with __part, and in this step, they are simply
    // assigned Like this: __part0 = scalar;
    if (orig->variantT() == V_SgVarRefExp) {
      SgVarRefExp *var = static_cast<SgVarRefExp *>(copyExpression(orig));
      type = var->get_type();

      // Make sure we have a valid reduction clause
      reduction_mod = omp_simd_get_reduction_mod(var);
      if (reduction_mod == 0)
        failSimdContract("omp-simd-pass1", var,
                         "has no exact reduction modifier");
      reduction_name = var->get_symbol()->get_name();
      need_partial = true;
      dest = var;

      // Otherwise, we just have a conventional store
    } else {
      SgPntrArrRefExp *array = isSgPntrArrRefExp(copyExpression(orig));
      if (array == nullptr)
        failSimdContract("omp-simd-pass1", orig,
                         "has an unsupported assignment destination");
      type = array->get_type();
      dest = omp_simd_convert_ptr(copyExpression(orig));
    }

    switch (type->variantT()) {
    case V_SgTypeInt:
      type = buildIntType();
      break;
    case V_SgTypeFloat:
      type = buildFloatType();
      break;
    case V_SgTypeDouble:
      type = buildDoubleType();
      break;
    default: {
      failSimdContract("omp-simd-pass1", orig,
                       "has an unsupported destination scalar type");
    }
    }

    std::string partial_vec = "";
    if (need_partial) {
      if (reduction_map_.find(reduction_name) == reduction_map_.end()) {
        partial_vec = simdGenName(2);
        SgVariableDeclaration *vd =
            buildVariableDeclaration(partial_vec, type, NULL, new_block);
        appendStatement(vd, new_block);
        reduction_map_[reduction_name] = partial_vec;
      } else {
        partial_vec = reduction_map_[reduction_name];
      }
    }

    // Expand compound assignments only in the detached pass-1 transaction.
    // The source loop remains untouched until the final replacement commits.
    SgExpression *rvalue = copyExpression(op->get_rhs_operand());
    SgExpression *lhs = nullptr;
    if (isSgPlusAssignOp(op) || isSgMinusAssignOp(op) || isSgMultAssignOp(op) ||
        isSgDivAssignOp(op)) {
      lhs = partial_vec.empty() ? copyExpression(op->get_lhs_operand())
                                : buildVarRefExp(partial_vec, new_block);
    }
    if (isSgPlusAssignOp(op)) {
      rvalue = buildAddOp(lhs, rvalue, type);

    } else if (isSgMinusAssignOp(op)) {
      rvalue = buildSubtractOp(lhs, rvalue, type);

    } else if (isSgMultAssignOp(op)) {
      rvalue = buildMultiplyOp(lhs, rvalue, type);

    } else if (isSgDivAssignOp(op)) {
      rvalue = buildDivideOp(lhs, rvalue, type);
    }

    // Build the rval (the expression)
    omp_simd_build_3addr(rvalue, type);
    if (rvalue->get_parent() != nullptr)
      failSimdContract("omp-simd-pass1", rvalue,
                       "retained an owner after three-address translation");
    SageInterface::deleteAST(rvalue,
                             SageInterface::DeleteAstMode::kRequireIsolated);

    // Build the lval (the store/assignment)
    if (nameStack.size() != 1)
      failSimdContract("omp-simd-pass1", op,
                       "did not produce exactly one three-address result");
    std::string name = nameStack.top();
    nameStack.pop();

    // If application, build the reduction assignment
    if (need_partial) {
      std::string dest_vec = name;
      name = partial_vec;

      SgVarRefExp *va = buildVarRefExp(partial_vec, new_block);
      SgVarRefExp *vec = buildVarRefExp(dest_vec, new_block);

      SgExprStatement *assign = buildAssignStatement(va, vec);
      appendStatement(assign, new_block);
    }

    SgVarRefExp *var = buildVarRefExp(name, new_block);
    SgExprStatement *storeExpr = buildAssignStatement(dest, var);
    appendStatement(storeExpr, new_block);

    if (need_partial) {
      SgAssignOp *reduction_assignment =
          isSgAssignOp(storeExpr->get_expression());
      if (reduction_assignment == nullptr)
        failSimdContract("omp-simd-pass1", storeExpr,
                         "did not publish one exact reduction assignment");
      assign_list.push_back(reduction_assignment);
      reduction_statements.push_back(storeExpr);
    }
  }

  std::map<std::string, std::string> reduction_map2;

  for (size_t i = assign_list.size(); i-- > 0;) {
    SgAssignOp *op = assign_list.at(i);
    if (!isSgVarRefExp(op->get_lhs_operand()) ||
        !isSgVarRefExp(op->get_rhs_operand())) {
      failSimdContract("omp-simd-pass1", op,
                       "generated a non-scalar reduction assignment");
    }

    SgVarRefExp *lval = isSgVarRefExp(op->get_lhs_operand());
    SgVarRefExp *rval = isSgVarRefExp(op->get_rhs_operand());
    std::string lval_name = lval->get_symbol()->get_name();
    std::string rval_name = rval->get_symbol()->get_name();

    if (reduction_map2.find(lval_name) == reduction_map2.end()) {
      reduction_map2[lval_name] = rval_name;
    } else {
      removeStatement(reduction_statements.at(i));
    }
  }
}

////////////////////////////////////////////////////////////////////////////////////
// Pass 2-> Convert to SIMD IR

// The main pass2 loop
void OmpSimdCompiler::omp_simd_pass2() {
  Rose_STL_Container<SgStatement *> bodyList = new_block->get_statements();

  for (Rose_STL_Container<SgStatement *>::iterator i = bodyList.begin();
       i != bodyList.end(); i++) {
    if (*i == nullptr)
      failSimdContract("omp-simd-pass2", *i,
                       "generated block contains a null statement");
    if ((*i)->variantT() != V_SgExprStatement) {
      if (isSgVariableDeclaration(*i) == nullptr)
        failSimdContract("omp-simd-pass2", *i,
                         "generated block contains an unsupported statement");
      continue;
    }

    SgExprStatement *expr_statement = static_cast<SgExprStatement *>(*i);
    if (!isSgBinaryOp(expr_statement->get_expression()))
      failSimdContract("omp-simd-pass2", expr_statement,
                       "does not own one exact binary expression");

    // Build the IR
    // The left-value will be the variable reference, which will be translated
    // to a vector variable declaration The right-value is either a pointer
    // reference or a math expression
    SgBinaryOp *assign_stmt =
        static_cast<SgBinaryOp *>(expr_statement->get_expression());
    SgExpression *lval = assign_stmt->get_lhs_operand();
    SgExpression *rval = assign_stmt->get_rhs_operand();

    // Load
    if (lval->variantT() == V_SgVarRefExp &&
        rval->variantT() == V_SgPntrArrRefExp) {
      SgVarRefExp *lvar = static_cast<SgVarRefExp *>(lval);
      if (lvar->get_type()->variantT() == V_SgPointerType) {
        ir_block->push_back(deepCopy(assign_stmt));
      } else {
        SgPntrArrRefExp *pntr = static_cast<SgPntrArrRefExp *>(rval);
        if (pntr->get_rhs_operand()->variantT() == V_SgPntrArrRefExp) {
          SgSIMDGather *ld = buildBinaryExpression<SgSIMDGather>(
              deepCopy(lval), deepCopy(rval), lval->get_type());
          ir_block->push_back(ld);
        } else if (pntr->get_lhs_operand()->variantT() == V_SgPntrArrRefExp) {
          SgSIMDExplicitGather *ld =
              buildBinaryExpression<SgSIMDExplicitGather>(
                  deepCopy(lval), deepCopy(rval), lval->get_type());
          ir_block->push_back(ld);
        } else {
          SgSIMDLoad *ld = buildBinaryExpression<SgSIMDLoad>(
              deepCopy(lval), deepCopy(rval), lval->get_type());
          ir_block->push_back(ld);
        }
      }

      // This could be a broadcast or a scalar store
    } else if (lval->variantT() == V_SgVarRefExp &&
               rval->variantT() == V_SgVarRefExp) {
      SgVarRefExp *lvar = static_cast<SgVarRefExp *>(lval);
      std::string name = lvar->get_symbol()->get_name().getString();

      // If the variable name starts with __part, we store to a partial sums
      // register
      if (name.rfind("__part", 0) == 0) {
        SgSIMDPartialStore *str = buildBinaryExpression<SgSIMDPartialStore>(
            deepCopy(lval), deepCopy(rval), lval->get_type());
        ir_block->push_back(str);

        // If the variable name starts with __vec, we broadcast
      } else if (name.rfind("__vec", 0) == 0) {
        SgSIMDBroadcast *ld = buildBinaryExpression<SgSIMDBroadcast>(
            deepCopy(lval), deepCopy(rval), lval->get_type());
        ir_block->push_back(ld);

        // Otherwise, we have a scalar store
      } else {
        SgSIMDScalarStore *str = buildBinaryExpression<SgSIMDScalarStore>(
            deepCopy(lval), deepCopy(rval), lval->get_type());
        ir_block->push_back(str);
      }

      // Broadcast
      // TODO: This is not the most elegent piece of code...
    } else if (lval->variantT() == V_SgVarRefExp &&
               (rval->variantT() == V_SgIntVal ||
                rval->variantT() == V_SgFloatVal ||
                rval->variantT() == V_SgDoubleVal)) {
      SgSIMDBroadcast *ld = buildBinaryExpression<SgSIMDBroadcast>(
          deepCopy(lval), deepCopy(rval), lval->get_type());
      ir_block->push_back(ld);

      // Store
    } else if (lval->variant() == V_SgPntrArrRefExp &&
               rval->variantT() == V_SgVarRefExp) {
      SgPntrArrRefExp *pntr = static_cast<SgPntrArrRefExp *>(lval);

      if (pntr->get_rhs_operand()->variantT() == V_SgPntrArrRefExp) {
        SgSIMDScatter *str = buildBinaryExpression<SgSIMDScatter>(
            deepCopy(lval), deepCopy(rval), lval->get_type());
        ir_block->push_back(str);
      } else {
        SgSIMDStore *str = buildBinaryExpression<SgSIMDStore>(
            deepCopy(lval), deepCopy(rval), lval->get_type());
        ir_block->push_back(str);
      }

      // Math
    } else if (lval->variantT() == V_SgVarRefExp &&
               rval->variantT() == V_SgExprListExp) {
      SgExprListExp *expr_list = static_cast<SgExprListExp *>(rval);
      if (expr_list->get_expressions().size() != 1 ||
          expr_list->get_expressions().front() == nullptr)
        failSimdContract("omp-simd-pass2", expr_list,
                         "does not own one exact math expression");
      SgExpression *first = expr_list->get_expressions().front();
      if (!isSgBinaryOp(first))
        failSimdContract("omp-simd-pass2", first,
                         "is not one exact binary math expression");

      SgVarRefExp *dest = static_cast<SgVarRefExp *>(lval);
      SgBinaryOp *math_stmt = static_cast<SgBinaryOp *>(first);
      lval = math_stmt->get_lhs_operand();
      rval = math_stmt->get_rhs_operand();

      if (!isSgVarRefExp(lval) || !isSgVarRefExp(rval)) {
        failSimdContract("omp-simd-pass2", math_stmt,
                         "has non-variable three-address operands");
      }

      // First, check to see if its a reduction assignment
      std::string name = dest->get_symbol()->get_name().getString();
      if (name.rfind("__part", 0) == 0) {
        SgSIMDPartialStore *str = buildBinaryExpression<SgSIMDPartialStore>(
            deepCopy(lval), NULL, lval->get_type());
        ir_block->push_back(str);
      }

      // Then, build math
      SgSIMDBinaryOp *math = NULL;
      SgExprListExp *parameters =
          buildExprListExp(deepCopy(lval), deepCopy(rval));

      switch (math_stmt->variantT()) {
      case V_SgAddOp:
        math = buildBinaryExpression<SgSIMDAddOp>(deepCopy(dest), parameters,
                                                  dest->get_type());
        break;
      case V_SgSubtractOp:
        math = buildBinaryExpression<SgSIMDSubOp>(deepCopy(dest), parameters,
                                                  dest->get_type());
        break;
      case V_SgMultiplyOp:
        math = buildBinaryExpression<SgSIMDMulOp>(deepCopy(dest), parameters,
                                                  dest->get_type());
        break;
      case V_SgDivideOp:
        math = buildBinaryExpression<SgSIMDDivOp>(deepCopy(dest), parameters,
                                                  dest->get_type());
        break;
      default:
        failSimdContract("omp-simd-pass2", math_stmt,
                         "has an unsupported math operation");
      }

      if (math == nullptr)
        failSimdContract("omp-simd-pass2", math_stmt,
                         "did not produce one exact SIMD IR operation");
      ir_block->push_back(math);
    } else {
      failSimdContract("omp-simd-pass2", assign_stmt,
                       "has an unsupported three-address assignment shape");
    }
  }
  if (ir_block->empty())
    failSimdContract("omp-simd-pass2", new_block,
                     "produced an empty SIMD IR transaction");

  if (new_block_owner == nullptr ||
      new_block->get_parent() != new_block_owner ||
      std::count(new_block_owner->get_statements().begin(),
                 new_block_owner->get_statements().end(), new_block) != 1)
    failSimdContract("omp-simd-scratch-owner", new_block,
                     "is not published exactly once in its semantic sandbox");
  SgStatementPtrList &scratch_statements = new_block_owner->get_statements();
  SgStatementPtrList::iterator scratch_position = std::find(
      scratch_statements.begin(), scratch_statements.end(), new_block);
  if (scratch_position == scratch_statements.end() ||
      std::find(std::next(scratch_position), scratch_statements.end(),
                new_block) != scratch_statements.end())
    failSimdContract("omp-simd-scratch-owner", new_block,
                     "is not uniquely removable from its semantic sandbox");
  scratch_statements.erase(scratch_position);
  new_block->set_parent(nullptr);
  if (new_block->get_parent() != nullptr ||
      std::count(new_block_owner->get_statements().begin(),
                 new_block_owner->get_statements().end(), new_block) != 0)
    failSimdContract("omp-simd-scratch-owner", new_block,
                     "could not finish its exact semantic sandbox transaction");
  new_block_owner = nullptr;
}

unsigned int OmpSimdCompiler::omp_simd_get_length() const {
  if (target == nullptr || simd_arch != Intel_AVX512)
    failSimdContract("omp-simd-length", target,
                     "cannot select an Intel width without an exact Intel "
                     "SIMD target");
  if (simdlen_.has_value())
    return *simdlen_;
  if (!safelen_.has_value() || *safelen_ >= 16)
    return 16;
  if (*safelen_ >= 8)
    return 8;
  if (*safelen_ >= 4)
    return 4;
  failSimdContract("omp-simd-length", target,
                   "has no Intel SIMD width that satisfies safelen");
}

////////////////////////////////////////////////////////////////////////////////////
// The helper functions for the SIMD compiler
OmpSimdCompiler::OmpSimdCompiler() {
  this->new_block = SageBuilder::buildBasicBlock();
  this->ir_block = new Rose_STL_Container<SgNode *>();
}

OmpSimdCompiler::~OmpSimdCompiler() {
  if (new_block_owner != nullptr)
    failSimdContract("omp-simd-raii", new_block,
                     "scratch block retained a semantic sandbox owner");
  if (ir_block != nullptr) {
    for (SgNode *node : *ir_block) {
      if (node != nullptr) {
        if (node->get_parent() != nullptr)
          failSimdContract("omp-simd-raii", node,
                           "detached IR unexpectedly acquired an owner");
        SageInterface::deleteAST(
            node, SageInterface::DeleteAstMode::kRequireIsolated);
      }
    }
    delete ir_block;
  }
  if (!block_released && new_block != nullptr) {
    if (new_block->get_parent() != nullptr)
      failSimdContract("omp-simd-raii", new_block,
                       "owned detached block unexpectedly acquired an owner");
    SageInterface::deleteAST(new_block,
                             SageInterface::DeleteAstMode::kRequireIsolated);
  }
}

SgBasicBlock *OmpSimdCompiler::releaseBlock() {
  if (new_block == nullptr || block_released ||
      new_block->get_parent() != nullptr)
    failSimdContract("omp-simd-raii", new_block,
                     "block cannot be transferred exactly once");
  block_released = true;
  return new_block;
}

Rose_STL_Container<SgNode *> *OmpSimdCompiler::getIR() { return ir_block; }

void OmpSimdCompiler::setTarget(SgOmpSimdStatement *target) {
  if (this->target != nullptr || target == nullptr)
    failSimdContract("omp-simd-length", target,
                     "cannot publish the SIMD target more than once or as "
                     "null");
  SgOmpClauseList *clause_list = OmpSupport::getOmpClauseList(target);
  const ExactSimdLengthClauses lengths =
      requireExactSimdLengthClauses(target, clause_list);
  SgBasicBlock *semantic_scope = isSgBasicBlock(target->get_scope());
  if (new_block == nullptr || new_block->get_parent() != nullptr ||
      new_block_owner != nullptr || semantic_scope == nullptr ||
      !isStructurallyRetained(target, semantic_scope))
    failSimdContract("omp-simd-scratch-owner", target,
                     "has no exact semantic sandbox for three-address output");
  appendStatement(new_block, semantic_scope);
  if (new_block->get_parent() != semantic_scope ||
      std::count(semantic_scope->get_statements().begin(),
                 semantic_scope->get_statements().end(), new_block) != 1)
    failSimdContract("omp-simd-scratch-owner", new_block,
                     "failed exact semantic sandbox publication");
  new_block_owner = semantic_scope;
  this->target = target;
  simdlen_ = lengths.simdlen;
  safelen_ = lengths.safelen;
}

void OmpSimdCompiler::setForLoop(SgForStatement *for_loop) {
  if (this->for_loop != nullptr || for_loop == nullptr || target == nullptr)
    failSimdContract("omp-simd-loop-publication", for_loop,
                     "cannot install a null, repeated, or ownerless associated "
                     "loop identity");
  SgForStatement *associated = OmpSupport::requireExactAssociatedForLoop(
      target, target->get_body(), OmpSupport::AssociatedLoopPathContract::Simd,
      "omp-simd-loop-publication");
  if (associated != for_loop)
    failSimdContract("omp-simd-loop-publication", for_loop,
                     "does not match the target's exact associated loop");
  this->for_loop = for_loop;
}

void OmpSimdCompiler::setInductionSymbol(SgVariableSymbol *symbol) {
  if (induction_symbol != nullptr || for_loop == nullptr || symbol == nullptr ||
      symbol->get_declaration() == nullptr)
    failSimdContract("omp-simd-region", symbol,
                     "cannot install a null, repeated, or ownerless induction "
                     "identity");
  induction_symbol = symbol;
}

////////////////////////////////////////////////////////////////////////////////////
// The entry point to the SIMD analyzer

void OmpSupport::transOmpSimd(SgNode *node) {
  if (simd_arch == Nothing)
    failSimdContract("omp-simd-architecture", node,
                     "cannot lower SIMD with no selected architecture");

  // Insert the needed headers
  // insertHeader(file, "immintrin.h", true, true);
  // insertHeader(file, "arm_sve.h", true, true);

  // Make sure the tree is correct
  SgOmpSimdStatement *target = isSgOmpSimdStatement(node);
  if (target == nullptr || target->get_parent() == nullptr)
    failSimdContract("omp-simd-owner", node,
                     "is not one attached SIMD directive");
  requireSimdEdge(target->get_parent(), target, "omp-simd-owner");

  SgSourceFile *file = getEnclosingSourceFile(node);
  if (file == nullptr)
    failSimdContract("omp-simd-owner", target,
                     "has no exact enclosing source file");

  SgScopeStatement *p_scope = target->get_scope();
  if (p_scope == nullptr)
    failSimdContract("omp-simd-owner", target, "has no exact lexical scope");

  SgStatement *body = target->get_body();
  if (body == nullptr)
    failSimdContract("omp-simd-owner", target, "has no exact directive body");

  SgForStatement *for_loop = requireExactAssociatedForLoop(
      target, body, AssociatedLoopPathContract::Simd, "omp-simd-loop");
  (void)preflightSimdRegion(target, for_loop);

  // Complete normalization and backend generation in a separately owned
  // semantic sandbox.  No attached source node is changed until this returns.
  std::unique_ptr<DetachedSimdBackendPlan> backend_plan =
      buildDetachedSimdBackendPlan(target);

  SgNode *exact_owner = target->get_parent();
  requireSimdEdge(exact_owner, target, "omp-simd-commit");
  if (simd_arch == Intel_AVX512)
    insertHeader(file, "immintrin.h", true, true);
  else if (simd_arch == Arm_SVE2)
    insertHeader(file, "arm_sve.h", true, true);

  SgBasicBlock *replacement = backend_plan->releaseRegion();
  replaceStatement(target, replacement, true);
  if (replacement->get_parent() != exact_owner)
    failSimdContract("omp-simd-commit", replacement,
                     "replacement did not acquire the exact source owner");
  requireSimdEdge(exact_owner, replacement, "omp-simd-commit");
  if (target->get_parent() != nullptr)
    failSimdContract("omp-simd-commit", target,
                     "source directive remained attached after commit");
  SageInterface::deleteAST(target,
                           SageInterface::DeleteAstMode::kRequireIsolated);
  if (SgNode::isLiveNode(target))
    failSimdContract("omp-simd-commit", target,
                     "source directive remained live after commit");
  validateCommittedSimdGraph(replacement, file);
}
