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
