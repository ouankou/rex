
#include "Cxx_Grammar.h"
#include "ROSE_ASSERT.h"

#include <utility>
#include <vector>

struct stmt_pos {
  stmt_pos(SgStatementPtrList *statements, SgStatementPtrList::size_type index,
           SgStatement *owner)
      : statements(statements), index(index), owner(owner) {}
  SgStatementPtrList *statements;
  SgStatementPtrList::size_type index;
  SgStatement *owner;
};

typedef std::vector<stmt_pos> stmt_pos_list;
stmt_pos findPosition(SgNode *n);
stmt_pos_list findFollowingPositions(SgInitializedName *in);
stmt_pos_list findFollowingPositions(SgStatement *stmt);

template <typename T, T *(isT)(SgNode *)>
inline std::pair<T *, SgNode *> findNearestParentOfType(SgNode *node) {
  while (true) {
    ROSE_ASSERT(node != NULL);
    SgNode *nodeParent = node->get_parent();

    T *typedNode = isT(nodeParent);
    if (typedNode != NULL) {
      return std::make_pair(typedNode, node);
    }
    node = nodeParent;
  }
}

SgStatement *findStatementForExpression(SgExpression *expr);
std::pair<SgBasicBlock *, SgNode *> findBasicBlockForStmt(SgStatement *stmt);
