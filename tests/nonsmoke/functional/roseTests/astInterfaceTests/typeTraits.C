// Test some type-related SageInterface functions
//  by traversing memory pools
//
// Liao, 3/3/2009
#include "rose.h"

#include <iostream>
using namespace std;

class RoseVisitor : public ROSE_VisitTraversal {
public:
  size_t checkedAssignmentClasses = 0;
  size_t checkedRoleClasses = 0;
  void visit(SgNode *node);
};

void RoseVisitor::visit(SgNode *node) {
  if (isSgType(node)) {
    const bool copyConstructible =
        SageInterface::isCopyConstructible(isSgType(node));
    if (copyConstructible)
      cout << "found a copy constructible type:\t" << node->class_name()
           << endl;
    else
      cout << "      found a non-copy constructible type:\t"
           << node->class_name() << endl;

    const size_t referenceTypes = SgReferenceType::numberOfNodes();
    const bool assignable = SageInterface::isAssignable(isSgType(node));
    ROSE_ASSERT(SgReferenceType::numberOfNodes() == referenceTypes);
    if (SgClassType *type = isSgClassType(node)) {
      const SgName name = type->get_name();
      const struct {
        const char *name;
        bool copyConstructible;
        bool assignable;
      } expectations[] = {{"B", true, true},
                          {"rex_traits_private_copy_default", false, true},
                          {"rex_traits_move_constructor", false, false},
                          {"rex_traits_constructor_template", false, true},
                          {"rex_traits_assignment_template", true, false}};
      for (const auto &expected : expectations)
        if (name == expected.name) {
          ROSE_ASSERT(copyConstructible == expected.copyConstructible);
          ROSE_ASSERT(assignable == expected.assignable);
          ++checkedRoleClasses;
        }
      if (name == "RexPublicAssignment" || name == "RexPointerAssignment") {
        ROSE_ASSERT(assignable);
        ++checkedAssignmentClasses;
      } else if (name == "RexPrivateAssignment" ||
                 name == "RexDeletedAssignment" ||
                 name == "RexMoveAssignment") {
        ROSE_ASSERT(!assignable);
        ++checkedAssignmentClasses;
      }
    }
    if (assignable)
      cout << "found an assignable type:\t" << node->class_name() << endl;
    else
      cout << "      found a non-assignable type:\t" << node->class_name()
           << endl;
  }
}

int main(int argc, char *argv[]) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != NULL);

  // ROSE visit traversal
  RoseVisitor visitor;
  visitor.traverseMemoryPool();
  ROSE_ASSERT(visitor.checkedAssignmentClasses == 5);
  ROSE_ASSERT(visitor.checkedRoleClasses == 5);

  return backend(project);
}
