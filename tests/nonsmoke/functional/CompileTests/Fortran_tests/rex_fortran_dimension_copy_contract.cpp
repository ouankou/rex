#include "rose.h"

#include <algorithm>

namespace {
SgInitializedName *findExactObject(SgNode *root) {
  ROSE_ASSERT(root != nullptr);
  SgInitializedName *result = nullptr;
  for (SgNode *node : NodeQuery::querySubTree(root, V_SgInitializedName)) {
    SgInitializedName *name = isSgInitializedName(node);
    ROSE_ASSERT(name != nullptr);
    if (name->get_name() != "dimension_owned_object") {
      continue;
    }
    ROSE_ASSERT(result == nullptr);
    result = name;
  }
  ROSE_ASSERT(result != nullptr);
  return result;
}

SgAttributeSpecificationStatement *findExactDimension(SgNode *root) {
  ROSE_ASSERT(root != nullptr);
  SgAttributeSpecificationStatement *result = nullptr;
  for (SgNode *node :
       NodeQuery::querySubTree(root, V_SgAttributeSpecificationStatement)) {
    SgAttributeSpecificationStatement *attribute =
        isSgAttributeSpecificationStatement(node);
    ROSE_ASSERT(attribute != nullptr);
    if (attribute->get_attribute_kind() !=
        SgAttributeSpecificationStatement::e_dimensionStatement) {
      continue;
    }
    ROSE_ASSERT(result == nullptr);
    result = attribute;
  }
  ROSE_ASSERT(result != nullptr);
  return result;
}
} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);

  SgInitializedName *originalName = findExactObject(project);
  SgAttributeSpecificationStatement *originalDimension =
      findExactDimension(project);
  ROSE_ASSERT(originalName->get_fortran_separate_shape_declaration() ==
              originalDimension);

  SgVariableDeclaration *originalDeclaration =
      isSgVariableDeclaration(originalName->get_declaration());
  ROSE_ASSERT(originalDeclaration != nullptr);
  SgTreeCopy declarationCopyHelp;
  SgVariableDeclaration *declarationCopy =
      isSgVariableDeclaration(originalDeclaration->copy(declarationCopyHelp));
  ROSE_ASSERT(declarationCopy != nullptr);
  SgInitializedName *isolatedNameCopy = findExactObject(declarationCopy);
  ROSE_ASSERT(isolatedNameCopy->get_fortran_separate_shape_declaration() ==
              nullptr);

  const Rose_STL_Container<SgNode *> bodies =
      NodeQuery::querySubTree(project, V_SgBasicBlock);
  ROSE_ASSERT(bodies.size() == 1);
  SgBasicBlock *originalBody = isSgBasicBlock(bodies.front());
  ROSE_ASSERT(originalBody != nullptr);
  SgTreeCopy bodyCopyHelp;
  SgBasicBlock *bodyCopy = isSgBasicBlock(originalBody->copy(bodyCopyHelp));
  ROSE_ASSERT(bodyCopy != nullptr);

  SgInitializedName *bodyNameCopy = findExactObject(bodyCopy);
  SgAttributeSpecificationStatement *bodyDimensionCopy =
      findExactDimension(bodyCopy);
  ROSE_ASSERT(bodyNameCopy != originalName);
  ROSE_ASSERT(bodyDimensionCopy != originalDimension);
  ROSE_ASSERT(bodyNameCopy->get_fortran_separate_shape_declaration() ==
              bodyDimensionCopy);

  return 0;
}
