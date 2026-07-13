#include "sage3basic.h"
#include "sageBuilder.h"
#include "sageInterface.h"

#include <Cxx_GrammarTreeTraversalAccessEnums.h>

#include <cstdio>
#include <cstring>
#include <type_traits>
#include <utility>

static_assert(!std::is_convertible_v<E_SgConditionalExp, size_t>);
static_assert(rosettaTraversalIndex(
                  E_SgConditionalExp::SgConditionalExp_conditional_exp) == 0);

template <typename Node, typename = void>
struct HasHeuristicChildIndex : std::false_type {};

template <typename Node>
struct HasHeuristicChildIndex<
    Node, std::void_t<decltype(std::declval<const Node &>().getChildIndex(
              static_cast<SgNode *>(nullptr)))>> : std::true_type {};

static_assert(!HasHeuristicChildIndex<SgNode>::value);

namespace {
class TraversalConsistencyVisitor final : public ROSE_VisitTraversal {
public:
  void visit(SgNode *node) override {
    ROSE_ASSERT(node != nullptr);

    const SgNodePtrList successors = node->get_traversalSuccessorContainer();
    const std::vector<std::string> names =
        node->get_traversalSuccessorNamesContainer();
    const size_t count = node->get_numberOfTraversalSuccessors();
    ++visited_;

    if (successors.size() != count || names.size() != count) {
      std::fprintf(stderr,
                   "REX_TEST_ERROR: traversal cardinality mismatch for %s: "
                   "successors=%zu names=%zu count=%zu\n",
                   node->sage_class_name(), successors.size(), names.size(),
                   count);
      failed_ = true;
      return;
    }

    for (size_t index = 0; index < count; ++index) {
      SgNode *const indexed = node->get_traversalSuccessorByIndex(index);
      if (indexed != successors[index]) {
        std::fprintf(stderr,
                     "REX_TEST_ERROR: traversal indexed successor mismatch "
                     "for %s at %zu\n",
                     node->sage_class_name(), index);
        failed_ = true;
        continue;
      }

      if (indexed == nullptr) {
        std::fprintf(stderr,
                     "REX_TEST_ERROR: traversal exposed null successor for %s "
                     "at %zu\n",
                     node->sage_class_name(), index);
        failed_ = true;
        continue;
      }
      if (!node->isChild(indexed)) {
        std::fprintf(stderr,
                     "REX_TEST_ERROR: structural child predicate rejected "
                     "%s successor at %zu\n",
                     node->sage_class_name(), index);
        failed_ = true;
      }
      const size_t reverseIndex = node->get_childIndex(indexed);
      if (reverseIndex != index) {
        std::fprintf(stderr,
                     "REX_TEST_ERROR: traversal reverse child index mismatch "
                     "for %s at %zu: reverse=%zu\n",
                     node->sage_class_name(), index, reverseIndex);
        failed_ = true;
      }
    }
  }

  bool failed() const { return failed_; }
  size_t visited() const { return visited_; }

private:
  bool failed_ = false;
  size_t visited_ = 0;
};

void buildTraversalFixtures() {
  // Materialize a builtin type whose former static builtin_type edge exposed
  // the successor-container/count disagreement.
  (void)SageBuilder::buildIntType();

  SgOmpContextSelector *selector =
      new SgOmpContextSelector(SgOmpClause::e_omp_context_trait_construct);
  SgOmpContextSelectorSet *selectorSet = new SgOmpContextSelectorSet(
      SgOmpClause::e_omp_context_selector_set_construct);
  SgIntVal *score = SageBuilder::buildIntVal(7);
  SgNullStatement *construct = SageBuilder::buildNullStatement();
  SgOmpContextSelectorProperty *property = new SgOmpContextSelectorProperty();
  selector->set_score(score);
  selector->set_construct_directive(construct);
  selector->get_properties().push_back(property);
  score->set_parent(selector);
  construct->set_parent(selector);
  property->set_parent(selector);
  selectorSet->get_selectors().push_back(selector);
  selector->set_parent(selectorSet);

  ROSE_ASSERT(selectorSet->isChild(selector));
  ROSE_ASSERT(selector->isChild(score));
  ROSE_ASSERT(selector->isChild(construct));
  ROSE_ASSERT(selector->isChild(property));
  ROSE_ASSERT(!selector->isChild(nullptr));
  ROSE_ASSERT(!selector->isChild(SageBuilder::buildIntVal(9)));
  ROSE_ASSERT(!selector->isChild(selectorSet));

  SgTemplateVariableDeclaration *declaration =
      new SgTemplateVariableDeclaration(SgName("rex_traversal_consistency"),
                                        SageBuilder::buildIntType(), nullptr);
  SgTemplateParameter *firstParameter = new SgTemplateParameter();
  SgTemplateParameter *secondParameter = new SgTemplateParameter();
  declaration->get_templateParameters().push_back(firstParameter);
  declaration->get_templateParameters().push_back(secondParameter);
  firstParameter->set_parent(declaration);
  secondParameter->set_parent(declaration);
}

int verifyAllLiveNodes() {
  buildTraversalFixtures();
  TraversalConsistencyVisitor visitor;
  visitor.traverseMemoryPool();
  if (visitor.visited() == 0) {
    std::fprintf(stderr,
                 "REX_TEST_ERROR: traversal consistency sweep visited no "
                 "memory-pool nodes\n");
    return 2;
  }
  return visitor.failed() ? 1 : 0;
}

int verifyOptionalNullIsOmitted() {
  SgOmpContextSelector *selector =
      new SgOmpContextSelector(SgOmpClause::e_omp_context_trait_construct);
  const SgNodePtrList successors = selector->get_traversalSuccessorContainer();
  const std::vector<std::string> names =
      selector->get_traversalSuccessorNamesContainer();
  if (!successors.empty() || !names.empty() ||
      selector->get_numberOfTraversalSuccessors() != 0) {
    std::fprintf(stderr,
                 "REX_TEST_ERROR: optional null selector edges were not "
                 "omitted\n");
    return 1;
  }
  return 0;
}

int verifyAlignofConditionalOperandTraversal() {
  SgType *const operandType = SageBuilder::buildIntType();
  SgType *const resultType = SageBuilder::buildUnsignedLongType();
  SgAlignOfOp *const typeForm =
      SageBuilder::buildAlignOfOp_nfi(operandType, resultType);
  const SgNodePtrList typeSuccessors =
      typeForm->get_traversalSuccessorContainer();
  const std::vector<std::string> typeNames =
      typeForm->get_traversalSuccessorNamesContainer();
  if (typeForm->get_operand_expr() != nullptr ||
      typeForm->get_operand_type() != operandType ||
      typeForm->get_type() != resultType || !typeSuccessors.empty() ||
      !typeNames.empty() || typeForm->get_numberOfTraversalSuccessors() != 0) {
    std::fprintf(stderr,
                 "REX_TEST_ERROR: alignof(type) did not preserve its exact "
                 "typed operand without an expression traversal edge\n");
    return 1;
  }

  SgIntVal *const operand = SageBuilder::buildIntVal(1);
  SgAlignOfOp *const expressionForm =
      SageBuilder::buildAlignOfOp_nfi(operand, resultType);
  const SgNodePtrList expressionSuccessors =
      expressionForm->get_traversalSuccessorContainer();
  const std::vector<std::string> expressionNames =
      expressionForm->get_traversalSuccessorNamesContainer();
  if (expressionForm->get_operand_expr() != operand ||
      expressionForm->get_operand_type() != nullptr ||
      expressionForm->get_type() != resultType ||
      operand->get_parent() != expressionForm ||
      expressionSuccessors.size() != 1 ||
      expressionSuccessors.front() != operand || expressionNames.size() != 1 ||
      expressionForm->get_numberOfTraversalSuccessors() != 1) {
    std::fprintf(stderr,
                 "REX_TEST_ERROR: alignof(expression) did not expose its "
                 "exact owned expression traversal edge\n");
    return 1;
  }
  return 0;
}

int verifyTemplateArgumentSemanticDeclarationCopy() {
  SgGlobal *const declarationOwner = new SgGlobal();
  SgTemplateDeclaration *const semanticDeclaration =
      new SgTemplateDeclaration(SgName("rex_semantic_template"));
  semanticDeclaration->set_definingDeclaration(semanticDeclaration);
  semanticDeclaration->set_firstNondefiningDeclaration(semanticDeclaration);
  semanticDeclaration->set_scope(declarationOwner);
  semanticDeclaration->set_parent(declarationOwner);
  declarationOwner->get_declarations().push_back(semanticDeclaration);

  SgTemplateArgument *const argument =
      new SgTemplateArgument(SgTemplateArgument::template_template_argument,
                             /*isArrayBoundUnknownType=*/false,
                             /*type=*/nullptr,
                             /*expression=*/nullptr, semanticDeclaration,
                             /*explicitlySpecified=*/true);
  if (semanticDeclaration->get_parent() != declarationOwner ||
      !argument->get_traversalSuccessorContainer().empty() ||
      argument->get_numberOfTraversalSuccessors() != 0 ||
      argument->isChild(semanticDeclaration)) {
    std::fprintf(stderr,
                 "REX_TEST_ERROR: template argument exposed its semantic "
                 "declaration as an owned traversal child\n");
    return 1;
  }

  SgTemplateArgument *const copiedArgument = SageInterface::deepCopy(argument);
  if (copiedArgument == nullptr || copiedArgument == argument ||
      copiedArgument->get_argumentType() !=
          SgTemplateArgument::template_template_argument ||
      copiedArgument->get_templateDeclaration() != semanticDeclaration ||
      semanticDeclaration->get_parent() != declarationOwner ||
      !copiedArgument->get_traversalSuccessorContainer().empty() ||
      copiedArgument->get_numberOfTraversalSuccessors() != 0 ||
      copiedArgument->isChild(semanticDeclaration)) {
    std::fprintf(stderr,
                 "REX_TEST_ERROR: template argument copy cloned or adopted "
                 "its external semantic declaration\n");
    return 1;
  }
  return 0;
}

[[noreturn]] void rejectInvalidSuccessorIndex() {
  SgOmpContextSelector *selector =
      new SgOmpContextSelector(SgOmpClause::e_omp_context_trait_construct);
  selector->get_traversalSuccessorByIndex(
      selector->get_numberOfTraversalSuccessors());
  ROSE_ABORT();
}

[[noreturn]] void rejectNullChildIndex() {
  SgOmpContextSelector *selector =
      new SgOmpContextSelector(SgOmpClause::e_omp_context_trait_construct);
  selector->get_childIndex(nullptr);
  ROSE_ABORT();
}

[[noreturn]] void rejectForeignChildIndex() {
  SgOmpContextSelector *selector =
      new SgOmpContextSelector(SgOmpClause::e_omp_context_trait_construct);
  selector->get_childIndex(SageBuilder::buildIntVal(1));
  ROSE_ABORT();
}

[[noreturn]] void rejectRequiredNullSuccessor() {
  SgAddOp *expression = SageBuilder::buildAddOp(SageBuilder::buildIntVal(1),
                                                SageBuilder::buildIntVal(2),
                                                SageBuilder::buildIntType());
  expression->set_lhs_operand(nullptr);
  expression->get_traversalSuccessorContainer();
  ROSE_ABORT();
}

[[noreturn]] void rejectAlignofMissingTypedOperand() {
  SgAlignOfOp *const malformed =
      new SgAlignOfOp(nullptr, nullptr, SageBuilder::buildUnsignedLongType());
  (void)malformed->get_type();
  ROSE_ABORT();
}

[[noreturn]] void rejectAlignofConflictingTypedOperands() {
  SgIntVal *const expressionOperand = SageBuilder::buildIntVal(1);
  SgAlignOfOp *const malformed =
      new SgAlignOfOp(expressionOperand, SageBuilder::buildIntType(),
                      SageBuilder::buildUnsignedLongType());
  expressionOperand->set_parent(malformed);
  (void)malformed->get_type();
  ROSE_ABORT();
}

[[noreturn]] void rejectNullContainerSuccessor() {
  SgExprListExp *expressions = SageBuilder::buildExprListExp();
  expressions->get_expressions().push_back(nullptr);
  expressions->get_traversalSuccessorContainer();
  ROSE_ABORT();
}

[[noreturn]] void rejectDuplicateSuccessorIdentity() {
  SgExprListExp *expressions = SageBuilder::buildExprListExp();
  SgIntVal *child = SageBuilder::buildIntVal(1);
  expressions->get_expressions().push_back(child);
  expressions->get_expressions().push_back(child);
  child->set_parent(expressions);
  expressions->get_traversalSuccessorContainer();
  ROSE_ABORT();
}
} // namespace

int main(int argc, char **argv) {
  if (argc != 2) {
    std::fprintf(stderr,
                 "REX_TEST_ERROR: expected one traversal consistency mode\n");
    return 2;
  }
  if (std::strcmp(argv[1], "all-live-nodes") == 0)
    return verifyAllLiveNodes();
  if (std::strcmp(argv[1], "optional-null") == 0)
    return verifyOptionalNullIsOmitted();
  if (std::strcmp(argv[1], "alignof-conditional-operand") == 0)
    return verifyAlignofConditionalOperandTraversal();
  if (std::strcmp(argv[1], "template-argument-semantic-declaration") == 0)
    return verifyTemplateArgumentSemanticDeclarationCopy();
  if (std::strcmp(argv[1], "invalid-successor-index") == 0)
    rejectInvalidSuccessorIndex();
  if (std::strcmp(argv[1], "null-child-index") == 0)
    rejectNullChildIndex();
  if (std::strcmp(argv[1], "foreign-child-index") == 0)
    rejectForeignChildIndex();
  if (std::strcmp(argv[1], "required-null-successor") == 0)
    rejectRequiredNullSuccessor();
  if (std::strcmp(argv[1], "alignof-missing-operand") == 0)
    rejectAlignofMissingTypedOperand();
  if (std::strcmp(argv[1], "alignof-conflicting-operands") == 0)
    rejectAlignofConflictingTypedOperands();
  if (std::strcmp(argv[1], "null-container-successor") == 0)
    rejectNullContainerSuccessor();
  if (std::strcmp(argv[1], "duplicate-successor") == 0)
    rejectDuplicateSuccessorIdentity();

  std::fprintf(stderr,
               "REX_TEST_ERROR: unknown traversal consistency mode %s\n",
               argv[1]);
  return 2;
}
