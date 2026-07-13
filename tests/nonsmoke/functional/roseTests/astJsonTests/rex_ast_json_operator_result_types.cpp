#include "nodeQuery.h"
#include "rose.h"

#include <string>

namespace {

SgInitializedName *requireInitializedName(SgNode *root,
                                          const std::string &name) {
  SgInitializedName *result = nullptr;
  for (SgNode *node : NodeQuery::querySubTree(root, V_SgInitializedName)) {
    SgInitializedName *candidate = isSgInitializedName(node);
    ROSE_ASSERT(candidate != nullptr);
    if (candidate->get_name() != name) {
      continue;
    }
    ROSE_ASSERT(result == nullptr);
    result = candidate;
  }
  ROSE_ASSERT(result != nullptr);
  return result;
}

template <typename Operator>
Operator *requireSingleOperator(SgNode *root, VariantT variant) {
  const Rose_STL_Container<SgNode *> nodes =
      NodeQuery::querySubTree(root, variant);
  ROSE_ASSERT(nodes.size() == 1);
  Operator *result = dynamic_cast<Operator *>(nodes.front());
  ROSE_ASSERT(result != nullptr);
  return result;
}

void verifyExactOperatorResultTypes(SgProject *project) {
  SgAddOp *addition = requireSingleOperator<SgAddOp>(project, V_SgAddOp);
  ROSE_ASSERT(isSgTypeInt(addition->get_type()->stripType()) != nullptr);

  SgLessThanOp *comparison =
      requireSingleOperator<SgLessThanOp>(project, V_SgLessThanOp);
  ROSE_ASSERT(isSgTypeBool(comparison->get_type()->stripType()) != nullptr);

  SgSubtractOp *subtraction =
      requireSingleOperator<SgSubtractOp>(project, V_SgSubtractOp);
  SgType *declared_difference_type =
      requireInitializedName(project, "distance")->get_type();
  ROSE_ASSERT(declared_difference_type != nullptr);
  ROSE_ASSERT(subtraction->get_type()->stripType() ==
              declared_difference_type->stripType());

  SgPointerDerefExp *dereference =
      requireSingleOperator<SgPointerDerefExp>(project, V_SgPointerDerefExp);
  ROSE_ASSERT(SageInterface::isConstType(dereference->get_type()));
  ROSE_ASSERT(isSgTypeInt(dereference->get_type()->stripType()) != nullptr);

  SgDotStarOp *dot_star =
      requireSingleOperator<SgDotStarOp>(project, V_SgDotStarOp);
  SgArrowStarOp *arrow_star =
      requireSingleOperator<SgArrowStarOp>(project, V_SgArrowStarOp);
  ROSE_ASSERT(isSgTypeInt(dot_star->get_type()->stripType()) != nullptr);
  ROSE_ASSERT(isSgTypeInt(arrow_star->get_type()->stripType()) != nullptr);

  size_t checked_casts = 0;
  size_t builtin_bit_casts = 0;
  size_t functional_casts = 0;
  size_t two_edge_paths = 0;
  for (SgNode *node : NodeQuery::querySubTree(project, V_SgCastExp)) {
    SgCastExp *cast = isSgCastExp(node);
    ROSE_ASSERT(cast != nullptr);
    cast->validate_semantic_conversion();
    ++checked_casts;
    if (cast->get_cast_type() == SgCastExp::e_builtin_bit_cast) {
      ++builtin_bit_casts;
    }
    if (cast->get_cast_type() == SgCastExp::e_functional_cast) {
      ++functional_casts;
    }
    if (cast->get_conversion_base_path().size() == 2) {
      SgClassType *intermediate = isSgClassType(
          cast->get_conversion_base_path()[0]->stripTypedefsAndModifiers());
      SgClassType *base = isSgClassType(
          cast->get_conversion_base_path()[1]->stripTypedefsAndModifiers());
      ROSE_ASSERT(intermediate != nullptr && base != nullptr);
      SgClassDeclaration *intermediate_declaration =
          isSgClassDeclaration(intermediate->get_declaration());
      SgClassDeclaration *base_declaration =
          isSgClassDeclaration(base->get_declaration());
      ROSE_ASSERT(intermediate_declaration != nullptr &&
                  base_declaration != nullptr);
      ROSE_ASSERT(intermediate_declaration->get_name() ==
                  "RexJsonCastIntermediate");
      ROSE_ASSERT(base_declaration->get_name() == "RexJsonCastBase");
      ++two_edge_paths;
    }
  }
  ROSE_ASSERT(checked_casts > 0);
  ROSE_ASSERT(builtin_bit_casts == 1);
  ROSE_ASSERT(functional_casts >= 1);
  ROSE_ASSERT(two_edge_paths >= 1);
}

} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  verifyExactOperatorResultTypes(project);

  project->skipfinalCompileStep(true);
  AstTests::runAllTests(project);
  return backend(project);
}
