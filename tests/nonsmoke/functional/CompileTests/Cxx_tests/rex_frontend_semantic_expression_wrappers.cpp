struct RexLifetimeValue {
  RexLifetimeValue();
  ~RexLifetimeValue();
};

RexLifetimeValue rex_make_lifetime_value();
int rex_consume_lifetime_value(const RexLifetimeValue &);

enum { rex_constant_extent = 1 + 2 };

int rex_semantic_wrapper_probe() {
  rex_make_lifetime_value();
  const RexLifetimeValue &value = rex_make_lifetime_value();
  return rex_consume_lifetime_value(value) + rex_constant_extent;
}

struct RexImplicitObjectOwner {
  int member;

  int rex_implicit_object_probe() {
    auto qualified = [this] { return RexImplicitObjectOwner::member; };
    return this->member + qualified();
  }
};
