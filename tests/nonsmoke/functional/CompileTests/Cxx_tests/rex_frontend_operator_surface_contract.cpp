using RexSize = __SIZE_TYPE__;

struct RexOperatorTarget {
  int field;
};

struct RexOrdering {
  int value;
};

int operator+(RexOrdering ordering, int value) {
  return ordering.value + value;
}

int rex_operator_argument();

int rex_explicit_named_operator() {
  return operator+(RexOrdering{1}, rex_operator_argument()) +
         operator+(RexOrdering{2}, rex_operator_argument() + 1);
}

struct RexOperatorSurface {
  int value;

  RexOperatorSurface operator+() const;
  RexOperatorSurface operator+(const RexOperatorSurface &) const;
  RexOperatorSurface &operator++();
  RexOperatorSurface operator++(int);
  int operator()(int, int) const;
  int &operator[](int);
  RexOperatorTarget *operator->() const;
};

RexOperatorSurface operator-(const RexOperatorSurface &);
RexOperatorSurface operator*(const RexOperatorSurface &,
                             const RexOperatorSurface &);
RexOperatorSurface &operator--(RexOperatorSurface &);
RexOperatorSurface operator--(RexOperatorSurface &, int);

template <typename T> RexOrdering operator<=>(const T &, const T &) {
  return RexOrdering{0};
}

struct RexTemplateMemberSurface {
  template <typename T> RexOrdering convert(const T &) const {
    return RexOrdering{1};
  }
};

unsigned long operator""_rex_surface(const char *, RexSize);

int rex_frontend_operator_surface_contract(RexOperatorSurface &lhs,
                                           RexOperatorSurface rhs) {
  RexOperatorSurface member_unary = +lhs;
  RexOperatorSurface nonmember_unary = -lhs;
  RexOperatorSurface member_binary = lhs + rhs;
  RexOperatorSurface nonmember_binary = lhs * rhs;
  ++lhs;
  lhs++;
  --lhs;
  lhs--;
  int called = lhs(1, 2);
  int indexed = lhs[3];
  int arrow = lhs->field;
  unsigned long literal = "rex"_rex_surface;
  int grouped_spaceship = (lhs <=> rhs) + 1;
  RexOrdering explicit_spaceship = operator<=> <RexOperatorSurface>(lhs, rhs);
  RexTemplateMemberSurface member_template;
  RexOrdering explicit_member =
      member_template.convert<RexOperatorSurface>(lhs);
  RexOrdering explicit_arrow_member =
      (&member_template)->convert<RexOperatorSurface>(rhs);
  return member_unary.value + nonmember_unary.value + member_binary.value +
         nonmember_binary.value + called + indexed + arrow + literal +
         grouped_spaceship + explicit_spaceship.value + explicit_member.value +
         explicit_arrow_member.value;
}
struct RexDispatch {
  virtual int evaluate() { return 7; }
  virtual int operator()(int value) { return value; }
};

template <class T> void rex_destroy_pointer(T *object) {
  object->~value_type();
}
template <class T> void rex_destroy_reference(T &object) {
  object.~value_type();
}

struct RexDestructorTarget {
  using value_type = RexDestructorTarget;
};

void rex_instantiate_destructors(RexDestructorTarget *first,
                                RexDestructorTarget &second) {
  rex_destroy_pointer(first);
  rex_destroy_reference(second);
}

int rex_dispatch_operator(RexDispatch &object) { return object(1); }
int rex_dispatch_lambda() { return [] { return 3; }(); }

int rex_dispatch_reference(RexDispatch &object) { return object.evaluate(); }
int rex_dispatch_pointer(RexDispatch *object) { return object->evaluate(); }
int rex_dispatch_dereference(RexDispatch *object) {
  return (*object).evaluate();
}
int rex_dispatch_qualified_reference(RexDispatch &object) {
  return object.RexDispatch::evaluate();
}
int rex_dispatch_qualified_pointer(RexDispatch *object) {
  return object->RexDispatch::evaluate();
}
