struct RexExactThisTypes {
  RexExactThisTypes *mutable_this() { return this; }

  const volatile RexExactThisTypes *cv_this() const volatile { return this; }

  RexExactThisTypes *lambda_this() {
    return [this]() { return this; }();
  }

  RexExactThisTypes *implicit_lambda_this() {
    return [=]() { return this; }();
  }
};

using RexTargetSizeType = decltype(sizeof(int));

RexTargetSizeType rex_size_expression(int value) { return sizeof value; }

RexTargetSizeType rex_size_type() { return sizeof(long double); }

RexTargetSizeType rex_align_type() { return alignof(long double); }
