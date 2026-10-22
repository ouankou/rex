void foo() {
  int a[100];
  int *b, c = 0;
  void *p1;

  int &r = c;
}

class A {
public:
  int a;
  void member();
};

class B {
  int a;
  int b[100];
};

struct RexPublicAssignment {
  RexPublicAssignment &operator=(const RexPublicAssignment &);
};

class RexPrivateAssignment {
  RexPrivateAssignment &operator=(const RexPrivateAssignment &);
};

struct RexDeletedAssignment {
  RexDeletedAssignment &operator=(const RexDeletedAssignment &) = delete;
};

class RexPointerAssignment {
  RexPointerAssignment &operator=(RexPointerAssignment *);
};

struct RexMoveAssignment {
  RexMoveAssignment &operator=(RexMoveAssignment &&) = default;
};

class rex_traits_private_copy_default {
  rex_traits_private_copy_default(const rex_traits_private_copy_default &,
                                  int = 0);
};

struct rex_traits_move_constructor {
  rex_traits_move_constructor(rex_traits_move_constructor &&) = default;
};

struct rex_traits_constructor_template {
  rex_traits_constructor_template(const rex_traits_constructor_template &) =
      delete;
  template <class T = void>
  rex_traits_constructor_template(const rex_traits_constructor_template &);
};

struct rex_traits_assignment_template {
  rex_traits_assignment_template &
  operator=(const rex_traits_assignment_template &) = delete;
  template <class T = void>
  rex_traits_assignment_template &
  operator=(const rex_traits_assignment_template &);
};

static_assert(__is_constructible(B, const B &) &&
              __is_assignable(B &, const B &));
static_assert(!__is_constructible(rex_traits_private_copy_default,
                                  const rex_traits_private_copy_default &));
static_assert(!__is_constructible(rex_traits_move_constructor,
                                  const rex_traits_move_constructor &) &&
              !__is_assignable(rex_traits_move_constructor &,
                               const rex_traits_move_constructor &));
static_assert(!__is_constructible(rex_traits_constructor_template,
                                  const rex_traits_constructor_template &));
static_assert(!__is_assignable(rex_traits_assignment_template &,
                               const rex_traits_assignment_template &));
