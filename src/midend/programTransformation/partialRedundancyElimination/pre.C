#include "pre.h"

#include "sage3basic.h"
#include "sageInterface.h"

#include <algorithm>
#include <queue>

#include "constantFolding.h"

namespace legacy {

using namespace std;

// Are any variables in syms modified anywhere within n, or is n a declaration
// of one of them?
bool anyOfListPotentiallyModifiedIn(const vector<SgVariableSymbol *> &syms,
                                    SgNode *n) {
  for (SgVariableSymbol *symbol : syms) {
    ROSE_ASSERT(symbol != nullptr);
    if (isPotentiallyModified(symbol, n))
      return true;
    if (isSgVariableDeclaration(n)) {
      SgVariableDeclaration *decl = isSgVariableDeclaration(n);
      for (SgInitializedName *name : decl->get_variables())
        if (name == symbol->get_declaration())
          return true;
    }
  }
  return false;
}

namespace {
bool isIndependentObject(SgInitializedName *declaration) {
  ROSE_ASSERT(declaration != nullptr && declaration->get_type() != nullptr);
  SgType *type = declaration->get_type()->stripTypedefsAndModifiers();
  if (isSgReferenceType(type) || isSgRvalueReferenceType(type) ||
      declaration->get_structured_binding_owner() != nullptr ||
      isSgClassDefinition(declaration->get_scope()) != nullptr)
    return false;
  auto *file = SageInterface::getEnclosingSourceFile(declaration);
  if (file == nullptr || (file->get_inputLanguage() != SgFile::e_C_language &&
                          file->get_inputLanguage() != SgFile::e_Cxx_language))
    return false;
  if (isSgFunctionParameterList(declaration->get_parent()))
    return true;
  auto *variable = isSgVariableDeclaration(declaration->get_parent());
  if (variable == nullptr ||
      SageInterface::getEnclosingFunctionDefinition(variable) == nullptr)
    return false;
  const auto &storage =
      variable->get_declarationModifier().get_storageModifier();
  return !storage.isStatic() && !storage.isExtern() &&
         !storage.get_thread_local_storage() &&
         !variable->get_is_thread_local();
}

SgInitializedName *writtenObject(SgExpression *expression);

SgInitializedName *pointedObject(SgExpression *pointer) {
  ROSE_ASSERT(pointer != nullptr && pointer->get_type() != nullptr);
  if (!isSgPointerType(pointer->get_type()->stripTypedefsAndModifiers()))
    return nullptr;
  if (auto *address = isSgAddressOfOp(pointer)) {
    if (address->get_address_operand_form() ==
        SgAddressOfOp::e_address_operand_object)
      return writtenObject(address->get_operand());
  }
  if (isSgAddOp(pointer) || isSgSubtractOp(pointer)) {
    auto *binary = isSgBinaryOp(pointer);
    SgExpression *base = binary->get_lhs_operand();
    if (isSgAddOp(pointer) &&
        !isSgPointerType(base->get_type()->stripTypedefsAndModifiers()))
      base = binary->get_rhs_operand();
    return pointedObject(base);
  }
  return nullptr;
}

SgInitializedName *writtenObject(SgExpression *expression) {
  ROSE_ASSERT(expression != nullptr);
  if (auto *reference = isSgVarRefExp(expression)) {
    auto *declaration = reference->get_symbol()->get_declaration();
    return isIndependentObject(declaration) ? declaration : nullptr;
  }
  if (auto *member = isSgDotExp(expression))
    return writtenObject(member->get_lhs_operand());
  if (auto *dereference = isSgPointerDerefExp(expression))
    return pointedObject(dereference->get_operand());
  if (expression->get_lvalue()) {
    if (isSgAssignOp(expression) || isSgCompoundAssignOp(expression))
      return writtenObject(isSgBinaryOp(expression)->get_lhs_operand());
    if (isSgPlusPlusOp(expression) || isSgMinusMinusOp(expression))
      return writtenObject(isSgUnaryOp(expression)->get_operand());
  }
  return nullptr;
}

bool readOnlyIntegralExpression(SgExpression *expression) {
  ROSE_ASSERT(expression != nullptr);
  if (!expressionTreeEqual(expression, expression))
    return false;
  for (SgNode *node : NodeQuery::querySubTree(expression, V_SgExpression)) {
    auto *operand = isSgExpression(node);
    ROSE_ASSERT(operand != nullptr && operand->get_type() != nullptr);
    if (!operand->get_type()->isIntegerType() || isSgSizeOfOp(operand) ||
        isSgAlignOfOp(operand))
      return false;
  }
  return true;
}

bool readOnlyIntegralStatement(SgStatement *statement) {
  ROSE_ASSERT(statement != nullptr);
  if (auto *block = isSgBasicBlock(statement))
    return std::all_of(block->get_statements().begin(),
                       block->get_statements().end(),
                       readOnlyIntegralStatement);
  if (auto *returned = isSgReturnStmt(statement))
    return returned->get_return_keyword_kind() ==
               SgReturnStmt::e_return_keyword_return &&
           (returned->get_expression() == nullptr ||
            readOnlyIntegralExpression(returned->get_expression()));
  if (auto *expression = isSgExprStatement(statement))
    return readOnlyIntegralExpression(expression->get_expression());
  if (auto *conditional = isSgIfStmt(statement))
    return readOnlyIntegralStatement(conditional->get_conditional()) &&
           readOnlyIntegralStatement(conditional->get_true_body()) &&
           (conditional->get_false_body() == nullptr ||
            readOnlyIntegralStatement(conditional->get_false_body()));
  return isSgNullStatement(statement) != nullptr;
}

bool readOnlyIntegralCall(SgFunctionCallExp *call) {
  auto *declaration = call->getAssociatedFunctionDeclaration();
  if (declaration == nullptr || isSgMemberFunctionDeclaration(declaration))
    return false;
  auto *defining =
      isSgFunctionDeclaration(declaration->get_definingDeclaration());
  if (defining == nullptr || defining->get_definition() == nullptr)
    return false;
  for (auto *candidate : {declaration, defining}) {
    const auto &modifier = candidate->get_functionModifier();
    if (modifier.isGnuAttributeWeak() ||
        modifier.isGnuAttributeWeakReference() ||
        candidate->get_declarationModifier()
            .get_typeModifier()
            .isGnuAttributeNoReturn())
      return false;
  }
  auto *type = defining->get_type();
  ROSE_ASSERT(type != nullptr && call->get_args() != nullptr);
  SgType *result = type->get_return_type()->stripTypedefsAndModifiers();
  if ((!result->isIntegerType() && !isSgTypeVoid(result)) ||
      type->get_has_ellipses() ||
      type->get_arguments().size() !=
          call->get_args()->get_expressions().size())
    return false;
  for (SgType *parameter : type->get_arguments())
    if (!parameter->isIntegerType())
      return false;
  return std::all_of(call->get_args()->get_expressions().begin(),
                     call->get_args()->get_expressions().end(),
                     readOnlyIntegralExpression) &&
         readOnlyIntegralStatement(defining->get_definition()->get_body());
}

bool consumesObjectValue(SgVarRefExp *reference) {
  SgExpression *value = reference;
  while (true) {
    SgNode *parent = value->get_parent();
    ROSE_ASSERT(parent != nullptr);
    if (isSgExprStatement(parent))
      return true;
    if (auto *loop = isSgForStatement(parent))
      return loop->get_increment() == value;
    if (auto *cast = isSgCastExp(parent)) {
      cast->validate_semantic_conversion();
      return cast->get_value_category() == SgCastExp::e_value_category_prvalue;
    }
    if (auto *initializer = isSgAssignInitializer(parent)) {
      auto *name = isSgInitializedName(initializer->get_parent());
      return name != nullptr && name->get_type()->isIntegerType();
    }
    if (auto *binary = isSgBinaryOp(parent)) {
      ROSE_ASSERT(binary->get_lhs_operand() == value ||
                  binary->get_rhs_operand() == value);
      if (isSgAssignOp(binary) || isSgCompoundAssignOp(binary)) {
        if (binary->get_rhs_operand() == value)
          return true;
        value = binary;
        continue;
      }
      if (isSgCommaOpExp(binary)) {
        if (binary->get_lhs_operand() == value)
          return true;
        value = binary;
        continue;
      }
      return !isSgMemberAccessExp(binary) && !isSgDotStarOp(binary) &&
             !isSgArrowStarOp(binary) && !isSgUserDefinedBinaryOp(binary) &&
             (!isSgPntrArrRefExp(binary) || binary->get_rhs_operand() == value);
    }
    if (auto *unary = isSgUnaryOp(parent)) {
      if (isSgPlusPlusOp(unary) || isSgMinusMinusOp(unary)) {
        value = unary;
        continue;
      }
      return isSgMinusOp(unary) || isSgUnaryAddOp(unary) || isSgNotOp(unary) ||
             isSgBitComplementOp(unary);
    }
    return false;
  }
}

bool inputsHaveOnlyValueUses(const vector<SgVariableSymbol *> &inputs,
                             SgBasicBlock *root) {
  auto *function = SageInterface::getEnclosingFunctionDefinition(root);
  auto *file = SageInterface::getEnclosingSourceFile(root);
  if (function == nullptr || file == nullptr || file->get_Cuda_only() ||
      file->get_OpenCL_only())
    return false;
  for (SgVariableSymbol *symbol : inputs) {
    auto *declaration = symbol->get_declaration();
    if (!isIndependentObject(declaration))
      return false;
    SgScopeStatement *scope = declaration->get_scope();
    if (scope != function &&
        SageInterface::getEnclosingFunctionDefinition(scope) != function)
      return false;
  }
  for (SgNode *node :
       NodeQuery::querySubTree(function->get_declaration(), V_SgNode)) {
    if (isSgAsmStmt(node) || isSgAwaitExpression(node) || isSgLambdaExp(node) ||
        isSgOmpBodyStatement(node) || isSgPragmaDeclaration(node) ||
        isSgTryStmt(node) || (isSgFunctionDefinition(node) && node != function))
      return false;
    if (auto *returned = isSgReturnStmt(node))
      if (returned->get_return_keyword_kind() ==
          SgReturnStmt::e_return_keyword_co_return)
        return false;
    if (auto *reference = isSgVarRefExp(node))
      for (SgVariableSymbol *symbol : inputs)
        if (reference->get_symbol()->get_declaration() ==
                symbol->get_declaration() &&
            !consumesObjectValue(reference))
          return false;
  }
  return true;
}

bool ordinaryScalarStore(SgExpression *target) {
  ROSE_ASSERT(target != nullptr && target->get_type() != nullptr);
  SgType *type = target->get_type()->stripType(SgType::STRIP_TYPEDEF_TYPE);
  while (auto *modifier = isSgModifierType(type)) {
    const auto &properties = modifier->get_typeModifier();
    if (properties.isAtomic() || properties.haveAddressSpace() ||
        properties.get_constVolatileModifier().isVolatile())
      return false;
    type = modifier->get_base_type()->stripType(SgType::STRIP_TYPEDEF_TYPE);
  }
  return type->isIntegerType() || type->isFloatType() || isSgPointerType(type);
}

bool hasOrdinaryCallOperands(SgFunctionCallExp *call) {
  ROSE_ASSERT(call->get_function() != nullptr && call->get_args() != nullptr);
  auto *cast = isSgCastExp(call->get_function());
  if (cast == nullptr)
    return true;
  cast->validate_semantic_conversion();
  return cast->get_semantic_conversion_kind() !=
         SgCastExp::e_semantic_conversion_BuiltinFnToFnPtr;
}

struct ExpressionEffects {
  bool mayModifyInputs = false;
  bool mayInterrupt = false;
  bool mayInterruptBeforeComputation = false;
};

class ExpressionEffectsVisitor : public AstSimpleProcessing {
  const vector<SgVariableSymbol *> &inputs;
  bool unexposedInputs;
  SgExpression *computation;

public:
  ExpressionEffectsVisitor(const vector<SgVariableSymbol *> &inputs,
                           bool unexposedInputs, SgExpression *computation)
      : inputs(inputs), unexposedInputs(unexposedInputs),
        computation(computation) {}

  ExpressionEffects effects;

  void visit(SgNode *node) override {
    if (auto *call = isSgFunctionCallExp(node)) {
      const bool barrier = !readOnlyIntegralCall(call);
      effects.mayModifyInputs |= barrier && !unexposedInputs;
      effects.mayInterrupt |= barrier;
      if (barrier) {
        effects.mayInterruptBeforeComputation |=
            computation == nullptr || !hasOrdinaryCallOperands(call) ||
            (!expressionComputedIn(computation, call->get_function()) &&
             !expressionComputedIn(computation, call->get_args()));
      }
      return;
    }
    if (isSgConstructorInitializer(node) || isSgNewExp(node) ||
        isSgDeleteExp(node) || isSgAsmStmt(node) || isSgThrowOp(node) ||
        isSgAwaitExpression(node) || isSgUserDefinedBinaryOp(node) ||
        isSgUserDefinedUnaryOp(node)) {
      effects.mayModifyInputs = true;
      effects.mayInterrupt = true;
      effects.mayInterruptBeforeComputation = true;
      return;
    }

    SgExpression *target = nullptr;
    if (isSgAssignOp(node) || isSgCompoundAssignOp(node))
      target = isSgBinaryOp(node)->get_lhs_operand();
    else if (isSgPlusPlusOp(node) || isSgMinusMinusOp(node))
      target = isSgUnaryOp(node)->get_operand();
    if (target == nullptr)
      return;

    SgInitializedName *object = writtenObject(target);
    if (object == nullptr) {
      effects.mayModifyInputs |=
          !unexposedInputs || !ordinaryScalarStore(target);
      return;
    }
    for (SgVariableSymbol *symbol : inputs) {
      SgInitializedName *input = symbol->get_declaration();
      if (!isIndependentObject(input) || input == object) {
        effects.mayModifyInputs = true;
        return;
      }
    }
  }
};

ExpressionEffects
getExpressionEffects(const vector<SgVariableSymbol *> &symbols, SgNode *node,
                     bool unexposedInputs = false,
                     SgExpression *computation = nullptr) {
  ExpressionEffectsVisitor visitor(symbols, unexposedInputs, computation);
  visitor.traverse(node, preorder);
  visitor.effects.mayModifyInputs =
      !symbols.empty() && (visitor.effects.mayModifyInputs ||
                           anyOfListPotentiallyModifiedIn(symbols, node));
  return visitor.effects;
}
} // namespace

int countComputationsOfExpressionIn(SgExpression *expr, SgNode *root) {
  ROSE_ASSERT(expr != nullptr && root != nullptr);
  if (auto *call = isSgFunctionCallExp(root))
    if (!hasOrdinaryCallOperands(call))
      return 0;
  int count = 0;
  if (auto *expression = isSgExpression(root))
    count = expressionTreeEqual(expression, expr) ? 1 : 0;
  if (auto *conditional = isSgConditionalExp(root)) {
    conditional->validate();
    count += countComputationsOfExpressionIn(
        expr, conditional->get_conditional_exp());
    if (conditional->get_operator_kind() ==
        SgConditionalExp::e_conditional_operator_standard)
      count += std::min(
          countComputationsOfExpressionIn(expr, conditional->get_true_exp()),
          countComputationsOfExpressionIn(expr, conditional->get_false_exp()));
  } else if (auto *choice = isSgChooseExpression(root)) {
    choice->validate();
    if (choice->get_selection_kind() ==
        SgChooseExpression::e_choose_selection_dependent)
      count += std::min(
          countComputationsOfExpressionIn(expr, choice->get_true_expression()),
          countComputationsOfExpressionIn(expr,
                                          choice->get_false_expression()));
    else
      count += countComputationsOfExpressionIn(
          expr, choice->get_selection_kind() ==
                        SgChooseExpression::e_choose_selection_true
                    ? choice->get_true_expression()
                    : choice->get_false_expression());
  } else if (auto *conditional = isSgIfStmt(root)) {
    ROSE_ASSERT(conditional->get_true_body() != nullptr);
    const bool known = conditional->runtime_branch_is_known();
    if (!conditional->get_is_if_consteval_statement() &&
        (!conditional->get_is_if_constexpr_statement() ||
         !isSgExprStatement(conditional->get_conditional())))
      count +=
          countComputationsOfExpressionIn(expr, conditional->get_conditional());
    if (known) {
      auto *selected = conditional->runtime_branch_is_true()
                           ? conditional->get_true_body()
                           : conditional->get_false_body();
      if (selected != nullptr)
        count += countComputationsOfExpressionIn(expr, selected);
    } else if (conditional->get_false_body() != nullptr) {
      count += std::min(
          countComputationsOfExpressionIn(expr, conditional->get_true_body()),
          countComputationsOfExpressionIn(expr, conditional->get_false_body()));
    }
  } else if (auto *loop = isSgWhileStmt(root)) {
    count += countComputationsOfExpressionIn(expr, loop->get_condition());
  } else if (auto *loop = isSgForStatement(root)) {
    count += countComputationsOfExpressionIn(expr, loop->get_for_init_stmt());
    count += countComputationsOfExpressionIn(expr, loop->get_test());
  } else if (isSgAndOp(root) || isSgOrOp(root)) {
    count += countComputationsOfExpressionIn(
        expr, isSgBinaryOp(root)->get_lhs_operand());
  } else if (auto *operation = isSgSizeOfOp(root)) {
    if (operation->expression_operand_is_evaluated())
      count +=
          countComputationsOfExpressionIn(expr, operation->get_operand_expr());
  } else if (auto *operation = isSgTypeIdOp(root)) {
    (void)operation->get_type();
    if (operation->get_operand_is_evaluated())
      count +=
          countComputationsOfExpressionIn(expr, operation->get_operand_expr());
  } else if (!isSgNoexceptOp(root) && !isSgAlignOfOp(root) &&
             !isSgFunctionDeclaration(root) && !isSgFunctionDefinition(root) &&
             !isSgLambdaExp(root)) {
    for (SgNode *child : root->get_traversalSuccessorContainer())
      if (child != nullptr)
        count += countComputationsOfExpressionIn(expr, child);
  }
  return count;
}

bool expressionComputedIn(SgExpression *expr, SgNode *root) {
  return countComputationsOfExpressionIn(expr, root) != 0;
}

#define FIXPOINT_BEGIN(cfg)                                                    \
  {                                                                            \
    VertexIter i = cfg.graph.vertices().begin(),                               \
               end = cfg.graph.vertices().end();                               \
    queue<Vertex> Q;                                                           \
    for (; i != end; ++i) {                                                    \
      Q.push(*i);                                                              \
    }                                                                          \
    while (!Q.empty()) {                                                       \
      Vertex v = Q.front();                                                    \
      Q.pop();                                                                 \
      bool changed = false;

#define FIXPOINT_END(cfg, get_outflow, outflow_iter)                           \
  if (changed) {                                                               \
    outflow_iter j = cfg.graph.get_outflow(v).begin(),                         \
                 jend = cfg.graph.get_outflow(v).end();                        \
    for (; j != jend; ++j) {                                                   \
      if (cfg.graph.source(*j) != v)                                           \
        Q.push(cfg.graph.source(*j));                                          \
      else                                                                     \
        Q.push(cfg.graph.target(*j));                                          \
      /* Includes case when j->source = j->target = v */                       \
    }                                                                          \
  }                                                                            \
  }                                                                            \
  }

#define FIXPOINT_OUTPUT_BEGIN(pmap) bool old_##pmap = pmap[v]

#define FIXPOINT_OUTPUT_END(pmap, leq)                                         \
  ROSE_ASSERT(pmap[v] leq old_##pmap);                                         \
  changed |= (pmap[v] != old_##pmap)

// Accumulate a certain property of all neighbors of a graph node.  Used
// internally by PRE.
template <class PropMap, class BinOp>
using NeighborAccumulationResult =
    std::invoke_result_t<BinOp, typename PropMap::value_type,
                         typename PropMap::value_type>;

template <class PropMap, class BinOp>
NeighborAccumulationResult<PropMap, BinOp>
accumulate_neighbors(const PRE::myControlFlowGraph &cfg, const PropMap &prop,
                     const vector<int> &edges,
                     int (PRE::simpleGraph::*edge_to_neighbor)(int) const,
                     const BinOp &combine,
                     NeighborAccumulationResult<PropMap, BinOp> init,
                     NeighborAccumulationResult<PropMap, BinOp> ifnone) {
  std::vector<int>::const_iterator i = edges.begin(), end = edges.end();
  if (i == end)
    return ifnone;
  else {
    for (; i != end; ++i)
      init = combine(init, prop[(cfg.graph.*edge_to_neighbor)(*i)]);
    return init;
  }
}

class ReplaceExpressionWithVarrefVisitor : public AstSimpleProcessing {
  SgExpression *expr;
  SgVarRefExp *vr;
  std::vector<SgExpression *> &retired_expressions;

public:
  ReplaceExpressionWithVarrefVisitor(
      SgExpression *expr, SgVarRefExp *vr,
      std::vector<SgExpression *> &retired_expressions)
      : expr(expr), vr(vr), retired_expressions(retired_expressions) {
    set_useDefaultIndexBasedTraversal(false);
  }

  void setNodeSuccessors(SgNode *node,
                         SuccessorsContainer &successors) override {
    if (auto *choice = isSgChooseExpression(node)) {
      choice->validate();
      successors.clear();
      if (choice->get_selection_kind() !=
          SgChooseExpression::e_choose_selection_false)
        successors.push_back(choice->get_true_expression());
      if (choice->get_selection_kind() !=
          SgChooseExpression::e_choose_selection_true)
        successors.push_back(choice->get_false_expression());
    } else {
      AstSimpleProcessing::setNodeSuccessors(node, successors);
    }
  }

  void visit(SgNode *n) override {
    SgExpression *n2 = isSgExpression(n);
    if (n2 && expressionTreeEqual(n2, expr) &&
        isSgExpression(n->get_parent())) {
      SgTreeCopy copyHelp;
      SgVarRefExp *replacement = isSgVarRefExp(vr->copy(copyHelp));
      if (replacement == NULL || replacement->get_parent() != NULL) {
        fprintf(stderr,
                "REX_AST_INVARIANT[partial-redundancy-replacement]: cache "
                "reference copy is not one detached expression\n");
        ROSE_ABORT();
      }
      SageInterface::replaceExpression(n2, replacement, true);
      ROSE_ASSERT(n2->get_parent() == nullptr);
      retired_expressions.push_back(n2);
    }
  }
};

// Do partial redundancy elimination, looking for copies of one expression expr
// within the basic block root.  A control flow graph for root must be provided
// in cfg, with a map from nodes to their statements in node_statements, a map
// from edges to their CFG edge types in edge_type, and a map from edges to
// their insertion points in edge_insertion_point.  The algorithm used is that
// of Paleri, Srikant, and Shankar ("Partial redundancy elimination: a simple,
// pragmatic, and provably correct algorithm", Science of Computer Programming
// 48 (2003) 1--20).
void PRE::partialRedundancyEliminationOne(
    SgExpression *expr, SgBasicBlock *root, const myControlFlowGraph &cfg,
    std::vector<SgExpression *> &retired_expressions) {
  ROSE_ASSERT(expr != NULL);
  ROSE_ASSERT(root != NULL);

  if (SgCastExp *cast = isSgCastExp(expr)) {
    cast->validate_semantic_conversion();
    if (cast->cast_type() == SgCastExp::e_implicit_cast) {
      // The typed frontend represents value-category and scalar conversions
      // explicitly.  They are semantic edges of their operand computation,
      // not independent source computations for PRE to cache.
      return;
    }
  }

  vector<SgVariableSymbol *> symbols_in_expression =
      SageInterface::getSymbolsUsedInExpression(expr);

  if (getExpressionEffects(symbols_in_expression, expr).mayModifyInputs) {
    // This expression updates its own arguments, and so is not idempotent
    return;
  }

  // Lvalue designators name storage.  Without alias and memory-version
  // analysis, PRE cannot prove that repeated reads from that storage are
  // equivalent computations, even when their index syntax is identical.
  if (expr->get_lvalue())
    return;
  if (isSgVarRefExp(expr))
    return;
  if (isSgValueExp(expr))
    return;
  if (isSgFunctionRefExp(expr))
    return;
  if (isSgExprListExp(expr))
    return;
  if (isSgInitializer(expr))
    return;

  // Expressions which do not keep a consistent value each time they are used
  if (!expressionTreeEqual(expr, expr))
    return;

  const bool unexposedInputs =
      readOnlyIntegralExpression(expr) &&
      inputsHaveOnlyValueUses(symbols_in_expression, root);

  VertexIter i = cfg.graph.vertices().begin(), end = cfg.graph.vertices().end();

  bool needToMakeCachevar = false;
  set<SgNode *> replacements;
  vector<pair<SgNode *, bool /* before */>> insertions;

  vector<bool> transp(cfg.graph.vertices().size()),
      anttransp(cfg.graph.vertices().size()), comp(cfg.graph.vertices().size()),
      antloc(cfg.graph.vertices().size());

  vector<SgNode *> first_computation(cfg.graph.vertices().size()),
      last_computation(cfg.graph.vertices().size());

  // Set values of local node properties
  for (i = cfg.graph.vertices().begin(); i != end; ++i) {
    const vector<SgNode *> &stmts = cfg.node_statements[*i];

    // Precompute test values for each statement
    vector<bool> argumentsModifiedInStatement(stmts.size());
    vector<bool> evaluationInterruptedInStatement(stmts.size());
    vector<bool> computationInterruptedInStatement(stmts.size());
    vector<int> expressionComputedInStatement(stmts.size());
    for (unsigned int j = 0; j < stmts.size(); ++j) {
      const auto effects = getExpressionEffects(symbols_in_expression, stmts[j],
                                                unexposedInputs, expr);
      argumentsModifiedInStatement[j] = effects.mayModifyInputs;
      evaluationInterruptedInStatement[j] = effects.mayInterrupt;
      computationInterruptedInStatement[j] =
          effects.mayInterruptBeforeComputation;
      expressionComputedInStatement[j] =
          countComputationsOfExpressionIn(expr, stmts[j]);
    }

    // Value preservation does not imply safe anticipation across a call.
    transp[*i] = true;
    anttransp[*i] = true;
    for (unsigned int j = 0; j < stmts.size(); ++j) {
      if (argumentsModifiedInStatement[j])
        transp[*i] = false;
      if (argumentsModifiedInStatement[j] ||
          evaluationInterruptedInStatement[j])
        anttransp[*i] = false;
    }

    // Compute comp and do local redundancy elimination
    comp[*i] = false;
    SgNode *firstComputationInChain = 0;
    bool needToInsertComputation = false;
    bool computationInsertedOrUsed = false;
    for (unsigned int j = 0; j < stmts.size(); ++j) {
      if (expressionComputedInStatement[j] &&
          !argumentsModifiedInStatement[j] &&
          !computationInterruptedInStatement[j] && comp[*i]) {
        // Do local redundancy elimination
        if (firstComputationInChain && needToInsertComputation) {
          insertions.push_back(make_pair(firstComputationInChain, true));
          replacements.insert(firstComputationInChain);
          needToInsertComputation = false;
        }
        replacements.insert(stmts[j]);
        computationInsertedOrUsed = true;
        needToMakeCachevar = true;
      }
      if (expressionComputedInStatement[j] &&
          !argumentsModifiedInStatement[j] &&
          !computationInterruptedInStatement[j]) {
        comp[*i] = true;
        if (!firstComputationInChain) {
          firstComputationInChain = stmts[j];
          needToInsertComputation = true;
          if (expressionComputedInStatement[j] >= 2) {
            insertions.push_back(make_pair(stmts[j], true));
            needToMakeCachevar = true;
            needToInsertComputation = false;
            computationInsertedOrUsed = true;
            replacements.insert(stmts[j]);
          }
        }
        last_computation[*i] = stmts[j];
      }
      if (argumentsModifiedInStatement[j]) {
        comp[*i] = false; // Must come after expressionComputedInStatement check
        firstComputationInChain = 0;
        needToInsertComputation = false;
      }
    }

    ASSERT_require(!computationInsertedOrUsed || needToMakeCachevar);

    // Compute antloc
    antloc[*i] = false;
    for (unsigned int j = 0; j < stmts.size(); ++j) {
      if (expressionComputedInStatement[j] &&
          !argumentsModifiedInStatement[j] &&
          !computationInterruptedInStatement[j]) {
        antloc[*i] = true;
        first_computation[*i] = stmts[j];
        break;
      }
      if (argumentsModifiedInStatement[j] ||
          evaluationInterruptedInStatement[j]) {
        antloc[*i] = false;
        break;
      }
    }
  }

  int (simpleGraph::*source_ptr)(int) const = &simpleGraph::source;
  int (simpleGraph::*target_ptr)(int) const = &simpleGraph::target;

  vector<bool> avin(cfg.graph.vertices().size(), true);
  vector<bool> avout(cfg.graph.vertices().size(), true);
  FIXPOINT_BEGIN(cfg)
  FIXPOINT_OUTPUT_BEGIN(avin);
  FIXPOINT_OUTPUT_BEGIN(avout);
  avin[v] = accumulate_neighbors(cfg, avout, cfg.graph.in_edges(v),
                                 &simpleGraph::source, logical_and<bool>(),
                                 true, false);
  avout[v] = comp[v] || (avin[v] && transp[v]);
  FIXPOINT_OUTPUT_END(avout, <=);
  FIXPOINT_OUTPUT_END(avin, <=);
  FIXPOINT_END(cfg, out_edges, OutEdgeIter)

  vector<bool> antin(cfg.graph.vertices().size(), true);
  vector<bool> antout(cfg.graph.vertices().size(), true);
  FIXPOINT_BEGIN(cfg)
  FIXPOINT_OUTPUT_BEGIN(antin);
  FIXPOINT_OUTPUT_BEGIN(antout);
  antout[v] =
      accumulate_neighbors(cfg, antin, cfg.graph.out_edges(v), target_ptr,
                           logical_and<bool>(), true, false);
  antin[v] = antloc[v] || (antout[v] && anttransp[v]);
  FIXPOINT_OUTPUT_END(antout, <=);
  FIXPOINT_OUTPUT_END(antin, <=);
  FIXPOINT_END(cfg, in_edges, InEdgeIter)

  vector<bool> safein(cfg.graph.vertices().size()),
      safeout(cfg.graph.vertices().size());

  for (i = cfg.graph.vertices().begin(); i != end; ++i) {
    safein[*i] = avin[*i] || antin[*i];
    safeout[*i] = avout[*i] || antout[*i];
  }

  vector<bool> spavin(cfg.graph.vertices().size(), false);
  vector<bool> spavout(cfg.graph.vertices().size(), false);
  FIXPOINT_BEGIN(cfg)
  FIXPOINT_OUTPUT_BEGIN(spavin);
  FIXPOINT_OUTPUT_BEGIN(spavout);
  spavin[v] = safein[v] && accumulate_neighbors(
                               cfg, spavout, cfg.graph.in_edges(v), source_ptr,
                               logical_or<bool>(), false, false);
  spavout[v] = safeout[v] && (comp[v] || (spavin[v] && transp[v]));
  FIXPOINT_OUTPUT_END(spavout, >=);
  FIXPOINT_OUTPUT_END(spavin, >=);
  FIXPOINT_END(cfg, out_edges, OutEdgeIter)

  vector<bool> spantin(cfg.graph.vertices().size(), false);
  vector<bool> spantout(cfg.graph.vertices().size(), false);
  FIXPOINT_BEGIN(cfg)
  FIXPOINT_OUTPUT_BEGIN(spantin);
  FIXPOINT_OUTPUT_BEGIN(spantout);
  spantout[v] = safeout[v] && accumulate_neighbors(
                                  cfg, spantin, cfg.graph.out_edges(v),
                                  target_ptr, logical_or<bool>(), false, false);
  // A computation already available on every incoming path permits cache
  // demand across an interruption without introducing a new evaluation.
  const bool demandTransparent = anttransp[v] || (avin[v] && transp[v]);
  spantin[v] = safein[v] && (antloc[v] || (spantout[v] && demandTransparent));
  FIXPOINT_OUTPUT_END(spantout, >=);
  FIXPOINT_OUTPUT_END(spantin, >=);
  FIXPOINT_END(cfg, in_edges, InEdgeIter)

  vector<bool> node_insert(cfg.graph.vertices().size());
  vector<bool> replacef(cfg.graph.vertices().size());
  vector<bool> replacel(cfg.graph.vertices().size());

  for (i = cfg.graph.vertices().begin(); i != end; ++i) {
    node_insert[*i] = comp[*i] && spantout[*i] && (!transp[*i] || !spavin[*i]);
    replacef[*i] = antloc[*i] && (spavin[*i] || (transp[*i] && spantout[*i]));
    replacel[*i] = comp[*i] && (spantout[*i] || (transp[*i] && spavin[*i]));

    if (node_insert[*i]) {
      needToMakeCachevar = true;
      insertions.push_back(make_pair(last_computation[*i], true));
    }

    if (replacef[*i]) {
      needToMakeCachevar = true;
      replacements.insert(first_computation[*i]);
    }

    if (replacel[*i]) {
      needToMakeCachevar = true;
      replacements.insert(last_computation[*i]);
    }
  }

  vector<bool> edge_insert(cfg.graph.edges().size());

  EdgeIter j = cfg.graph.edges().begin(), jend = cfg.graph.edges().end();
  for (; j != jend; ++j) {
    edge_insert[*j] = !spavout[cfg.graph.source(*j)] &&
                      spavin[cfg.graph.target(*j)] &&
                      spantin[cfg.graph.target(*j)];

    if (edge_insert[*j]) {
      needToMakeCachevar = true;
    }
  }

  // Add cache variable if necessary
  SgVarRefExp *cachevar = 0;
  if (needToMakeCachevar) {
    SgType *type = expr->get_type();
    if (isSgArrayType(type))
      type =
          SageBuilder::buildPointerType(isSgArrayType(type)->get_base_type());
    if (!SageInterface::isDefaultConstructible(type) ||
        !SageInterface::isAssignable(type))
      return;

    SgName cachevarname =
        SageInterface::generateUniqueVariableName(root, "pre_cache");
    SgVariableDeclaration *decl =
        SageBuilder::buildVariableDeclaration(cachevarname, type, NULL, root);
    SgInitializedName *initname = decl->get_variables().back();

    ROSE_ASSERT(initname != NULL);

    SageInterface::prependStatement(decl, root);
    SgVariableSymbol *cachevarsym = root->lookup_variable_symbol(cachevarname);
    if (cachevarsym == NULL || cachevarsym->get_declaration() != initname) {
      fprintf(stderr,
              "REX_AST_INVARIANT[partial-redundancy-cache-owner]: generated "
              "cache declaration has no exact published symbol\n");
      ROSE_ABORT();
    }

    PreprocessingInfo *cacheComment = new PreprocessingInfo(
        PreprocessingInfo::CplusplusStyleComment,
        (string("// Partial redundancy elimination: ") + cachevarname.str() +
         " is a cache of " + expr->unparseToString())
            .c_str(),
        "Compiler-Generated in PRE", 0, 0, 0, PreprocessingInfo::before);
    ROSE_ASSERT(cacheComment->get_file_info() != NULL);
    cacheComment->get_file_info()->set_physical_file_id(
        Sg_File_Info::NULL_FILE_ID);
    SageInterface::publishGeneratedPreprocessingInfo(cacheComment, decl);
    decl->addToAttachedPreprocessingInfo(cacheComment);
    cachevar = SageBuilder::buildVarRefExp(cachevarsym);
  }

  if (!needToMakeCachevar) {
    return;
  }
  ROSE_ASSERT(cachevar != NULL);

  SgTreeCopy expressionTemplateCopy;
  SgExpression *insertionExpressionTemplate =
      isSgExpression(expr->copy(expressionTemplateCopy));
  if (insertionExpressionTemplate == NULL ||
      insertionExpressionTemplate->get_parent() != NULL) {
    fprintf(stderr,
            "REX_AST_INVARIANT[partial-redundancy-expression-template]: "
            "source expression copy is not one detached transaction root\n");
    ROSE_ABORT();
  }

  // Do expression computation replacements
  for (set<SgNode *>::iterator i = replacements.begin();
       i != replacements.end(); ++i) {
    ReplaceExpressionWithVarrefVisitor(insertionExpressionTemplate, cachevar,
                                       retired_expressions)
        .traverse(*i, postorder);
  }

  // Do edge insertions
  for (j = cfg.graph.edges().begin(); j != jend; ++j) {
    if (edge_insert[*j]) {
      pair<SgNode *, bool> insert_point = cfg.edge_insertion_point[*j];
      if (insert_point.first) {
        insertions.push_back(insert_point);
      } else {
        fprintf(stderr,
                "REX_AST_INVARIANT[partial-redundancy-edge-insertion]: "
                "edge=%d has no insertion point\n",
                *j);
        ROSE_ABORT();
      }
    }
  }

  // Do within-node insertions
  for (vector<pair<SgNode *, bool>>::iterator i = insertions.begin();
       i != insertions.end(); ++i) {
    SgTreeCopy tc1, tc2;
    SgVarRefExp *cachevarCopy = isSgVarRefExp(cachevar->copy(tc1));
    ROSE_ASSERT(cachevarCopy);
    cachevarCopy->set_lvalue(true);
    SgExpression *expressionCopy =
        isSgExpression(insertionExpressionTemplate->copy(tc2));
    ROSE_ASSERT(expressionCopy);
    SgExpression *operation = SageBuilder::buildAssignOp(
        cachevarCopy, expressionCopy, cachevarCopy->get_type());
    if (isSgExpression(i->first) && !i->second) {
      SgNode *ifp = i->first->get_parent();
      SgStatement *physicalOwner =
          SageInterface::getEnclosingStatement(i->first);
      SgExpression *original = isSgExpression(i->first);
      SgForStatement *forParent = isSgForStatement(ifp);
      SgBinaryOp *binaryParent = isSgBinaryOp(ifp);
      SgUnaryOp *unaryParent = isSgUnaryOp(ifp);
      const bool binaryLhs = binaryParent != nullptr &&
                             binaryParent->get_lhs_operand() == original;
      const bool binaryRhs = binaryParent != nullptr &&
                             binaryParent->get_rhs_operand() == original;
      ROSE_ASSERT(original->get_parent() == ifp);
      ROSE_ASSERT(!(binaryLhs && binaryRhs));
      if (forParent != nullptr && forParent->get_increment() == original) {
        forParent->set_increment(nullptr);
      } else if (binaryLhs) {
        binaryParent->set_lhs_operand(nullptr);
      } else if (binaryRhs) {
        binaryParent->set_rhs_operand(nullptr);
      } else if (unaryParent != nullptr &&
                 unaryParent->get_operand() == original) {
        unaryParent->set_operand_i(nullptr);
      } else {
        fprintf(stderr,
                "REX_AST_INVARIANT[partial-redundancy-comma-parent]: "
                "unsupported parent %s\n",
                ifp != nullptr ? ifp->class_name().c_str() : "<null>");
        ROSE_ABORT();
      }
      original->set_parent(nullptr);
      SgCommaOpExp *comma = SageBuilder::buildCommaOpExp(original, operation,
                                                         operation->get_type());
      SageInterface::publishGeneratedSubtreeOutputOwner(comma, physicalOwner);
      if (forParent != nullptr) {
        forParent->set_increment(comma);
        comma->set_parent(ifp);
      } else if (binaryLhs) {
        binaryParent->set_lhs_operand(comma);
      } else if (binaryRhs) {
        binaryParent->set_rhs_operand(comma);
      } else {
        unaryParent->set_operand(comma);
      }
      size_t ownerEdges = 0;
      for (SgNode *successor : ifp->get_traversalSuccessorContainer()) {
        ownerEdges += successor == comma ? 1 : 0;
      }
      if (comma->get_parent() != ifp || ownerEdges != 1) {
        fprintf(stderr,
                "REX_AST_INVARIANT[partial-redundancy-comma-owner]: "
                "inserted comma has parent=%p expected=%p edges=%zu\n",
                static_cast<void *>(comma->get_parent()),
                static_cast<void *>(ifp), ownerEdges);
        ROSE_ABORT();
      }
    } else {
      SgForStatement *forStatement = isSgForStatement(i->first->get_parent());
      SgExprStatement *forTest = isSgExprStatement(i->first);
      if (forStatement != nullptr && forTest != nullptr &&
          forStatement->get_test() == forTest) {
        SgExpression *originalCondition = forTest->get_expression();
        if (originalCondition == nullptr ||
            originalCondition->get_parent() != forTest ||
            SageInterface::containsUnknownType(originalCondition->get_type())) {
          fprintf(stderr, "REX_AST_INVARIANT[partial-redundancy-for-test]: "
                          "loop test has no exact typed condition\n");
          ROSE_ABORT();
        }
        SgTreeCopy conditionCopyHelp;
        SgExpression *condition =
            isSgExpression(originalCondition->copy(conditionCopyHelp));
        if (condition == nullptr || condition->get_parent() != nullptr ||
            SageInterface::containsUnknownType(condition->get_type())) {
          fprintf(stderr, "REX_AST_INVARIANT[partial-redundancy-for-test]: "
                          "condition copy is not detached\n");
          ROSE_ABORT();
        }
        SgExpression *replacement = nullptr;
        if (i->second) {
          replacement = SageBuilder::buildCommaOpExp(operation, condition,
                                                     condition->get_type());
        } else {
          SgTreeCopy operationCopyHelp;
          SgExpression *otherOperation =
              isSgExpression(operation->copy(operationCopyHelp));
          if (otherOperation == nullptr ||
              otherOperation->get_parent() != nullptr ||
              otherOperation->get_type() != operation->get_type()) {
            fprintf(stderr, "REX_AST_INVARIANT[partial-redundancy-for-test]: "
                            "cache assignment copy is not detached\n");
            ROSE_ABORT();
          }
          SgType *boolType = SageBuilder::buildBoolType();
          SgExpression *onTrue = SageBuilder::buildCommaOpExp(
              operation, SageBuilder::buildBoolValExp(true), boolType);
          SgExpression *onFalse = SageBuilder::buildCommaOpExp(
              otherOperation, SageBuilder::buildBoolValExp(false), boolType);
          replacement = SageBuilder::buildConditionalExp(condition, onTrue,
                                                         onFalse, boolType);
        }
        SageInterface::publishGeneratedSubtreeOutputOwner(replacement, forTest);
        forTest->set_expression(replacement);
        replacement->set_parent(forTest);
        originalCondition->set_parent(nullptr);
        retired_expressions.push_back(originalCondition);
        continue;
      }
      SgStatement *the_computation = SageBuilder::buildExprStatement(operation);
      if (auto *initializer = isSgForInitStatement(i->first)) {
        SageInterface::appendStatement(the_computation, initializer);
      } else if (isSgBasicBlock(i->first) && i->second) {
        SageInterface::prependStatement(the_computation,
                                        isSgBasicBlock(i->first));
      } else {

        if (forStatement != NULL) {
          if (forStatement->get_loop_body() == i->first) {
            SageInterface::insertStatement(forStatement->get_loop_body(),
                                           the_computation, i->second, true);
          } else {
            fprintf(stderr,
                    "REX_AST_INVARIANT[partial-redundancy-for-insertion]: "
                    "insertion point=%p is not the loop body\n",
                    static_cast<void *>(i->first));
            ROSE_ABORT();
          }
        } else {
          SgForInitStatement *forInitStatement =
              isSgForInitStatement(i->first->get_parent());
          if (forInitStatement != NULL) {
            SgStatementPtrList &statementList =
                forInitStatement->get_init_stmt();
            if ((isSgVariableDeclaration(i->first) == nullptr &&
                 isSgDeclarationGroupStatement(i->first) == nullptr &&
                 isSgExprStatement(i->first) == nullptr) ||
                std::count(statementList.begin(), statementList.end(),
                           i->first) != 1) {
              fprintf(stderr,
                      "REX_AST_INVARIANT[partial-redundancy-for-init]: "
                      "insertion point=%p is not exactly owned as a "
                      "declaration or expression by the initializer list\n",
                      static_cast<void *>(i->first));
              ROSE_ABORT();
            }
            auto *declaration = isSgVariableDeclaration(i->first);
            auto *group = isSgDeclarationGroupStatement(i->first);
            if (declaration != nullptr || group != nullptr) {
              auto *loop = isSgForStatement(forInitStatement->get_parent());
              ROSE_ASSERT(loop != nullptr &&
                          loop->get_for_init_stmt() == forInitStatement &&
                          statementList.size() == 1);
              auto *scope = SageBuilder::buildBasicBlock();
              SageInterface::replaceStatement(loop, scope);
              SageInterface::appendStatement(loop, scope);
              if (declaration != nullptr)
                SageInterface::moveVariableDeclaration(declaration, scope);
              else
                SageInterface::moveVariableDeclarationGroup(group, scope);
            }
            SageInterface::insertStatement(isSgStatement(i->first),
                                           the_computation, i->second, true);
          } else {
            SageInterface::insertStatement(
                SageInterface::getEnclosingStatement(i->first), the_computation,
                i->second, true);
          }
        }
      }
    }
  }

  SageInterface::deepDelete(insertionExpressionTemplate);
  SageInterface::deepDelete(cachevar);
}

class FindExpressionsVisitor : public AstSimpleProcessing {
public:
  vector<SgExpression *> exprs;

  virtual void visit(SgNode *n) {
    if (isSgExpression(n))
      exprs.push_back(isSgExpression(n));
  }
};

// Do partial redundancy for all expressions within a given function, whose
// body is given in n.
void PRE::partialRedundancyEliminationFunction(SgFunctionDefinition *n) {
  ConstantFolding::constantFoldingOptimization(n);

  FindExpressionsVisitor vis;
  vis.traverse(n, postorder);
  std::vector<SgExpression *> expressions_done;
  std::vector<SgExpression *> retired_expressions;
  // The worklist observes child replacements in still-live parent expressions.
  // Keep removed roots owned by this transaction until every worklist and
  // equality scan has finished; then retire them without retaining AST leaks.
  for (SgExpression *expression : vis.exprs) {
    if (std::any_of(expressions_done.begin(), expressions_done.end(),
                    [expression](SgExpression *candidate) {
                      return expressionTreeEqual(candidate, expression);
                    }))
      continue;
    expressions_done.push_back(expression);
    myControlFlowGraph controlflow;
    makeCfg(n, controlflow);
    addEdgeInsertionPoints(controlflow);
    partialRedundancyEliminationOne(expression, n->get_body(), controlflow,
                                    retired_expressions);
  }
  for (SgExpression *expression : retired_expressions)
    SageInterface::deleteAST(expression,
                             SageInterface::DeleteAstMode::kRequireIsolated);
}

class DoPreOnEachFunctionVisitor : public AstSimpleProcessing {
public:
  virtual void visit(SgNode *n) {
    if (isSgFunctionDefinition(n)) {
      PRE::partialRedundancyEliminationFunction(isSgFunctionDefinition(n));
    }
  }
};

// Do partial redundancy elimination on all functions within n.
void PRE::partialRedundancyElimination(SgNode *n) {
  DoPreOnEachFunctionVisitor().traverse(n, preorder);
}

} // namespace legacy
