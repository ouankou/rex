struct RexNoThrowSpecialMembers {
  RexNoThrowSpecialMembers() noexcept(true) {}
  RexNoThrowSpecialMembers(const RexNoThrowSpecialMembers &) noexcept(true) {}
  RexNoThrowSpecialMembers &
  operator=(const RexNoThrowSpecialMembers &) noexcept(true) {
    return *this;
  }
};

struct RexThrowingSpecialMembers {
  RexThrowingSpecialMembers() noexcept(false) {}
  RexThrowingSpecialMembers(const RexThrowingSpecialMembers &) noexcept(false) {
  }
  RexThrowingSpecialMembers &
  operator=(const RexThrowingSpecialMembers &) noexcept(false) {
    return *this;
  }
};

struct RexImplicitNoThrowSpecialMembers : RexNoThrowSpecialMembers {};

// Nothing in the program needs these implicit exception specifications, so
// Clang never evaluates them ([except.spec]) and the AST cannot answer whether
// they are nothrow.
struct RexUnneededNoThrowSpecialMembers : RexNoThrowSpecialMembers {};

struct RexOverrideNoThrowSpecialMembers : RexThrowingSpecialMembers {
  RexOverrideNoThrowSpecialMembers() noexcept {}
  RexOverrideNoThrowSpecialMembers(
      const RexOverrideNoThrowSpecialMembers &) noexcept {}
  RexOverrideNoThrowSpecialMembers &
  operator=(const RexOverrideNoThrowSpecialMembers &) noexcept {
    return *this;
  }
};

static_assert(__has_nothrow_constructor(RexNoThrowSpecialMembers), "noexcept");
static_assert(__has_nothrow_copy(RexNoThrowSpecialMembers), "noexcept");
static_assert(__has_nothrow_assign(RexNoThrowSpecialMembers), "noexcept");
static_assert(!__has_nothrow_constructor(RexThrowingSpecialMembers),
              "throwing");
static_assert(!__has_nothrow_copy(RexThrowingSpecialMembers), "throwing");
static_assert(!__has_nothrow_assign(RexThrowingSpecialMembers), "throwing");
static_assert(__has_nothrow_constructor(RexImplicitNoThrowSpecialMembers),
              "implicit members inherit the base guarantee");
static_assert(__has_nothrow_copy(RexImplicitNoThrowSpecialMembers),
              "implicit members inherit the base guarantee");
static_assert(__has_nothrow_assign(RexImplicitNoThrowSpecialMembers),
              "implicit members inherit the base guarantee");
static_assert(__has_nothrow_constructor(RexOverrideNoThrowSpecialMembers),
              "derived declaration controls its exception guarantee");
static_assert(__has_nothrow_copy(RexOverrideNoThrowSpecialMembers), "noexcept");
static_assert(__has_nothrow_assign(RexOverrideNoThrowSpecialMembers),
              "noexcept");

struct RexProtectedDefault {
protected:
  RexProtectedDefault() = default;
};

struct RexImplicitDefault : RexProtectedDefault {};

struct RexVirtualDefault {
  virtual void method();
};

struct RexTrivialAssignment {
  RexTrivialAssignment &operator=(const RexTrivialAssignment &) = default;
  ~RexTrivialAssignment() {}
};

struct RexNontrivialMove {
  RexNontrivialMove() = default;
  RexNontrivialMove(const RexNontrivialMove &) = default;
  RexNontrivialMove &operator=(const RexNontrivialMove &) = default;
  RexNontrivialMove(RexNontrivialMove &&) {}
};

struct RexDeletedDestructor {
  ~RexDeletedDestructor() = delete;
};

struct RexMoveOnly {
  RexMoveOnly() = default;
  RexMoveOnly(const RexMoveOnly &) = delete;
  RexMoveOnly(RexMoveOnly &&) = default;
};

static_assert(__has_trivial_constructor(RexNontrivialMove), "trivial default");
static_assert(__has_trivial_copy(RexNontrivialMove), "trivial copy");
static_assert(__has_trivial_assign(RexNontrivialMove), "trivial assignment");
static_assert(!__is_trivial(RexNontrivialMove), "nontrivial move");
static_assert(__is_trivial(decltype(nullptr)), "trivial nullptr");
static_assert(__is_trivial(RexDeletedDestructor), "trivial deleted destructor");
static_assert(__is_trivial(RexMoveOnly), "trivial move-only class");

#if __cplusplus >= 202002L
template <class T> struct RexConstrainedMove {
  RexConstrainedMove() = default;
  RexConstrainedMove(const RexConstrainedMove &) = default;
  RexConstrainedMove &operator=(const RexConstrainedMove &) = default;
  RexConstrainedMove(RexConstrainedMove &&)
    requires(sizeof(T) == 0)
  {}
};
using RexIneligibleMove = RexConstrainedMove<int>;
static_assert(__is_trivial(RexIneligibleMove), "ineligible nontrivial move");

template <class T> struct RexConstrainedDefault {
  RexConstrainedDefault()
    requires(sizeof(T) == 0)
  = default;
};
using RexIneligibleDefault = RexConstrainedDefault<int>;
static_assert(!__is_constructible(RexIneligibleDefault),
              "ineligible default constructor");

template <class T> struct RexConstrainedCopy {
  RexConstrainedCopy() = default;
  RexConstrainedCopy(const RexConstrainedCopy &)
    requires(sizeof(T) == 0)
  {}
  RexConstrainedCopy(const RexConstrainedCopy &) = default;
  RexConstrainedCopy &operator=(const RexConstrainedCopy &)
    requires(sizeof(T) == 0);
  RexConstrainedCopy &operator=(const RexConstrainedCopy &) = default;
};
using RexIneligibleCopy = RexConstrainedCopy<int>;
static_assert(__has_trivial_copy(RexIneligibleCopy), "eligible trivial copy");
static_assert(__has_trivial_assign(RexIneligibleCopy),
              "eligible trivial assignment");
static_assert(__is_trivial(RexIneligibleCopy), "ineligible nontrivial copies");
static_assert(__is_constructible(RexIneligibleCopy, const RexIneligibleCopy &),
              "eligible copy constructor");
static_assert(__is_assignable(RexIneligibleCopy &, const RexIneligibleCopy &),
              "eligible assignment operator");

template <class T> struct RexOnlyIneligibleCopy {
  RexOnlyIneligibleCopy() = default;
  RexOnlyIneligibleCopy(const RexOnlyIneligibleCopy &)
    requires(sizeof(T) == 0)
  {}
  RexOnlyIneligibleCopy &operator=(const RexOnlyIneligibleCopy &)
    requires(sizeof(T) == 0);
};
using RexNoEligibleCopy = RexOnlyIneligibleCopy<int>;
static_assert(!__has_trivial_copy(RexNoEligibleCopy), "no eligible copy");
static_assert(!__has_trivial_assign(RexNoEligibleCopy),
              "no eligible assignment");
static_assert(!__is_constructible(RexNoEligibleCopy, const RexNoEligibleCopy &),
              "ineligible copy constructor");
static_assert(!__is_assignable(RexNoEligibleCopy &, const RexNoEligibleCopy &),
              "ineligible assignment operator");
#endif

static_assert(__has_trivial_assign(RexTrivialAssignment),
              "defaulted assignment with nontrivial destructor");

static_assert(!__is_constructible(RexProtectedDefault),
              "protected constructor");
static_assert(__is_constructible(RexImplicitDefault), "implicit constructor");
static_assert(__has_trivial_constructor(RexImplicitDefault), "trivial default");
static_assert(__has_trivial_constructor(RexProtectedDefault), "trivial base");
static_assert(__is_constructible(RexVirtualDefault), "concrete virtual class");
static_assert(!__has_trivial_constructor(RexVirtualDefault), "virtual class");
static_assert(__is_trivial(RexProtectedDefault), "trivial protected class");
static_assert(__is_trivial(RexImplicitDefault), "trivial implicit class");
static_assert(!__is_trivial(RexVirtualDefault), "nontrivial virtual class");

struct RexValue {
  int value;
  explicit RexValue(int v) : value(v) {}
  RexValue(const RexValue &) = default;
  ~RexValue() {}
};

template <class T> struct RexValueInitializedOwner {
  T value;
  RexValueInitializedOwner() : value() {}
};

template <class T> struct RexOwner {
  using value_type = T;
  using value_class = RexValue;
  T value;
  RexOwner() : value(11) {}
  template <class U> explicit RexOwner(U v) : value(v) {}
  ~RexOwner() {}
  value_type make_value(int input) { return value_type(input); }
  int make_class(int input) { return value_class{input}.value; }
  struct Nested {
    T value;
    Nested() : value(17) {}
    ~Nested() {}
  };
};

template <class T> struct RexOwner<T *> {
  T value;
  RexOwner() : value(19) {}
  ~RexOwner() {}
};

template <> struct RexOwner<void> {
  int value;
  RexOwner() : value(23) {}
  ~RexOwner() {}
};

struct RexAnonymous {
  RexAnonymous() : member{29}, integer(31) {}
  struct {
    int value;
  } member;
  union {
    int integer;
    double real;
  };
};

int rex_default_calls = 0;
int rex_default_value() {
  ++rex_default_calls;
  return 0;
}

struct RexElision {
  static int alive;
  static int copies;
  static int moves;
  int value;

  explicit RexElision(int input) : value(input) { ++alive; }
  RexElision(const RexElision &other, int = rex_default_value())
      : value(other.value) {
    ++alive;
    ++copies;
  }
  RexElision(RexElision &&other, int = rex_default_value())
      : value(other.value) {
    other.value = -1;
    ++alive;
    ++moves;
  }
  RexElision &operator=(const RexElision &) = default;
  RexElision &operator=(RexElision &&) = default;
  ~RexElision() { --alive; }
};

int RexElision::alive = 0;
int RexElision::copies = 0;
int RexElision::moves = 0;

const RexElision rex_return_copy() {
  const RexElision local(37);
  return local;
}

RexElision rex_return_move() {
  RexElision local(41);
  return local;
}

int main() {
  RexValueInitializedOwner<int> zero;
  if (zero.value != 0)
    return 1;
  {
    RexElision copied = rex_return_copy();
    RexElision moved = rex_return_move();
    RexElision temporary = RexElision(47);
    RexElision listed = {RexElision(53)};
    RexElision direct(RexElision{59});
    RexElision source(43);
    const int prior_copies = RexElision::copies;
    const int prior_moves = RexElision::moves;
    RexElision explicit_copy(source);
    RexElision explicit_move(static_cast<RexElision &&>(source));
    if (copied.value != 37 || moved.value != 41 || temporary.value != 47 ||
        listed.value != 53 || direct.value != 59 || explicit_copy.value != 43 ||
        explicit_move.value != 43 || source.value != -1 ||
        RexElision::alive != 8 || RexElision::copies != prior_copies + 1 ||
        RexElision::moves != prior_moves + 1)
      return 1;
  }
  if (RexElision::alive != 0 ||
      rex_default_calls != RexElision::copies + RexElision::moves)
    return 1;

  RexValue value(7);
  RexOwner<int> values[2][2];
  RexOwner<int> empty_values[0];
  RexOwner<int> single_value[1];
  auto lambda = [value, values, empty_values, single_value](auto extra) {
    if (values[0][0].value != 11 || values[0][1].value != 11 ||
        values[1][0].value != 11 || values[1][1].value != 11 ||
        single_value[0].value != 11 || sizeof(empty_values) != 0)
      return -1;
    return value.value + extra;
  };
  values[0][0].value = 13;
  single_value[0].value = 17;
  auto copied_lambda = lambda;
  static_assert(__has_trivial_copy(decltype(lambda)), "trivial closure copy");
  static_assert(__has_trivial_assign(decltype(lambda)),
                "trivial closure assignment");
  static_assert(!__is_assignable(decltype(lambda) &, const decltype(lambda) &),
                "deleted closure assignment");
  RexOwner<int> ordinary;
  if (ordinary.make_value(61) != 61 || ordinary.make_class(67) != 67)
    return 1;
  RexOwner<int> converted(13L);
  RexOwner<int>::Nested nested;
  RexOwner<int *> partial;
  RexOwner<void> specialized;
  RexAnonymous anonymous;
  return copied_lambda(3) != 10 || ordinary.value != 11 ||
         converted.value != 13 || nested.value != 17 || partial.value != 19 ||
         specialized.value != 23 || anonymous.member.value != 29 ||
         anonymous.integer != 31;
}
