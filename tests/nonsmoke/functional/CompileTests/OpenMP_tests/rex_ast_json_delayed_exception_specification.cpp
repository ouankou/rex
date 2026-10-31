// RexDelayedHolder<RexNoDefault>'s defaulted default constructor is never
// used, so its exception specification stays unevaluated; evaluating it would
// instantiate an ill-formed default member initializer.  The static assertion
// needs RexNeededHolder's specification, so that one is evaluated.  The AST
// JSON checkpoint must carry both forms exactly.

struct RexNoDefault {
  explicit RexNoDefault(int) {}
};

template <class T> struct RexDelayedHolder {
  T value{};
  RexDelayedHolder() = default;
  explicit RexDelayedHolder(T initial) : value(initial) {}
};

struct RexNeededHolder {
  RexNeededHolder() = default;
  int value = 0;
};

static_assert(noexcept(RexNeededHolder()),
              "the needed specification is evaluated");

void rex_ast_json_delayed_exception_specification(int *out) {
  RexDelayedHolder<RexNoDefault> holder(RexNoDefault(1));
  RexNeededHolder needed;
#pragma omp parallel
  {
    *out = static_cast<int>(sizeof(holder)) + needed.value;
  }
}
