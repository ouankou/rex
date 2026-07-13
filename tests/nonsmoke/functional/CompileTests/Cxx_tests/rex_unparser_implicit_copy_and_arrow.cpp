struct RexArrowTarget {
  int value;
};

struct RexArrowProxy {
  RexArrowTarget *target;

  RexArrowTarget *operator->() const { return target; }
};

struct RexArrowChain {
  RexArrowProxy proxy;

  RexArrowProxy operator->() const { return proxy; }
};

struct RexImplicitCopy {
  int value;

  RexImplicitCopy clone() const { return *this; }
};

int rex_unparser_implicit_copy_and_arrow() {
  RexArrowTarget target{7};
  RexArrowProxy proxy{&target};
  RexArrowChain chain{proxy};
  RexImplicitCopy copy{5};
  return proxy->value + chain->value + copy.clone().value;
}
