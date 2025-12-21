#ifndef REX_EXPLICIT_QUALIFIER_ATTRIBUTE_H
#define REX_EXPLICIT_QUALIFIER_ATTRIBUTE_H

#include "sage3basic.h"

class RexExplicitQualifierAttribute : public AstAttribute {
public:
  RexExplicitQualifierAttribute(int qualification_depth, bool has_global)
      : qualification_depth_(qualification_depth), has_global_(has_global) {}

  int qualification_depth() const { return qualification_depth_; }
  bool has_global() const { return has_global_; }

  OwnershipPolicy getOwnershipPolicy() const override {
    return CONTAINER_OWNERSHIP;
  }

  AstAttribute *copy() const override {
    return new RexExplicitQualifierAttribute(qualification_depth_, has_global_);
  }

  std::string attribute_class_name() const override {
    return "RexExplicitQualifierAttribute";
  }

  std::string toString() override { return ""; }

private:
  int qualification_depth_ = 0;
  bool has_global_ = false;
};

static const char kRexExplicitQualifierAttr[] = "rex_explicit_qualifier";

inline const RexExplicitQualifierAttribute *
getRexExplicitQualifier(const SgNode *node) {
  if (node == nullptr) {
    return nullptr;
  }
  AstAttribute *attr = node->getAttribute(kRexExplicitQualifierAttr);
  return dynamic_cast<RexExplicitQualifierAttribute *>(attr);
}

inline void setRexExplicitQualifier(SgNode *node, int qualification_depth,
                                    bool has_global) {
  if (node == nullptr) {
    return;
  }
  if (qualification_depth <= 0 && !has_global) {
    return;
  }
  node->setAttribute(
      kRexExplicitQualifierAttr,
      new RexExplicitQualifierAttribute(qualification_depth, has_global));
}

#endif
