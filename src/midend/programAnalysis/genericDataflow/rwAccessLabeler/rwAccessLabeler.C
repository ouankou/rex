#include "featureTests.h"
#ifdef ROSE_ENABLE_SOURCE_ANALYSIS

#include "rwAccessLabeler.h"

#include "sage3basic.h"

using namespace std;

namespace rwAccessLabeler {

class lrRecord {
public:
  SgNode *readSubtree = nullptr;
  SgNode *writeSubtree = nullptr;
  SgNode *rwSubtree = nullptr;
  bool readAllChildren = false;
  accessType access = readAccess;

  lrRecord() = default;

  lrRecord(const lrRecord &parent, SgNode *node) : access(parent.access) {
    if (parent.readAllChildren || parent.readSubtree == node)
      access = readAccess;
    else if (parent.writeSubtree == node)
      access = writeAccess;
    else if (parent.rwSubtree == node)
      access = rwAccess;

    if (SgBinaryOp *binary = isSgBinaryOp(node)) {
      if (isSgAssignOp(binary)) {
        writeSubtree = binary->get_lhs_operand();
        readSubtree = binary->get_rhs_operand();
      } else if (isSgCompoundAssignOp(binary)) {
        rwSubtree = binary->get_lhs_operand();
        readSubtree = binary->get_rhs_operand();
      } else if (isSgPntrArrRefExp(binary)) {
        readAllChildren = true;
      }
    } else if (SgUnaryOp *unary = isSgUnaryOp(node)) {
      if (isSgMinusMinusOp(unary) || isSgPlusPlusOp(unary))
        rwSubtree = unary->get_operand();
      else if (isSgPointerDerefExp(unary))
        readSubtree = unary->get_operand();
    } else if (isSgFunctionCallExp(node)) {
      readAllChildren = true;
    }
  }
};

class rwAccessAttribute : public AstAttribute {
  accessType access;

public:
  rwAccessAttribute(lrRecord r) : access(r.access) {}

  AstAttribute *copy() const override { return new rwAccessAttribute(*this); }

  OwnershipPolicy getOwnershipPolicy() const override {
    return CONTAINER_OWNERSHIP;
  }

  void setAccess(accessType access) { this->access = access; }

  accessType getAccess() { return access; }
};

class rwAccessLabeler : public AstTopDownProcessing<lrRecord> {
public:
  lrRecord evaluateInheritedAttribute(SgNode *n, lrRecord inherited) {
    // pass this node's left-right record down to its children
    lrRecord r(inherited, n);

    if (AstAttribute *existing = n->getAttribute("rwAccess")) {
      auto *old = dynamic_cast<rwAccessAttribute *>(existing);
      if (old == nullptr) {
        fprintf(stderr,
                "REX_ANALYSIS_INVARIANT[rw-access]: node=%p has an "
                "incompatible access attribute\n",
                static_cast<void *>(n));
        ROSE_ABORT();
      }
      old->setAccess(r.access);
    } else {
      n->addNewAttribute("rwAccess", new rwAccessAttribute(r));
    }

    return r;
  }
};

// labels the portion of the AST rooted at root with annotations that identify
// the various portions the read/write sides of SgAssignOp nodes (if they are)
void addRWAnnotations(SgNode *root) {
  ASSERT_not_null(root);
  rwAccessLabeler labeler;
  lrRecord initial;
  labeler.traverse(root, initial);
}

// returns the type of access being performed at the given node
accessType getAccessType(SgNode *n) {
  ASSERT_not_null(n);
  auto *attribute =
      dynamic_cast<rwAccessAttribute *>(n->getAttribute("rwAccess"));
  if (attribute == nullptr) {
    fprintf(stderr,
            "REX_ANALYSIS_INVARIANT[rw-access]: node=%p has no typed access "
            "attribute\n",
            static_cast<void *>(n));
    ROSE_ABORT();
  }
  return attribute->getAccess();
}

// copies the access type annotation from src to tgt
void cloneAccessType(SgNode *tgt, SgNode *src) {
  ASSERT_not_null(tgt);
  ASSERT_not_null(src);
  AstAttribute *source = src->getAttribute("rwAccess");
  if (source == nullptr)
    return;
  AstAttribute *target = tgt->getAttribute("rwAccess");
  auto *source_access = dynamic_cast<rwAccessAttribute *>(source);
  auto *target_access = dynamic_cast<rwAccessAttribute *>(target);
  if (source_access == nullptr ||
      (target != nullptr && target_access == nullptr)) {
    fprintf(stderr, "REX_ANALYSIS_INVARIANT[rw-access]: incompatible access "
                    "attribute during clone\n");
    ROSE_ABORT();
  }
  if (target_access != nullptr)
    target_access->setAccess(source_access->getAccess());
  else
    tgt->addNewAttribute("rwAccess", new rwAccessAttribute(*source_access));
}

// returns a string representation of the given accessType
string accessTypeToStr(accessType t) {
  return (t == readAccess
              ? "readAccess"
              : (t == writeAccess ? "writeAccess"
                                  : (t == rwAccess ? "rwAccess" : "???")));
}

} // namespace rwAccessLabeler

#endif
