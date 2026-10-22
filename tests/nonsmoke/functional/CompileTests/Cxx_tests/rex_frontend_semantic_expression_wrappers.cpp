#include <utility>

struct RexLifetimeValue {
  RexLifetimeValue();
  ~RexLifetimeValue();
};

RexLifetimeValue rex_make_lifetime_value();
int rex_consume_lifetime_value(const RexLifetimeValue &);
struct RexBindingTuple {};
namespace std {
template <> struct tuple_size<RexBindingTuple> {
  static constexpr size_t value = 1;
};
template <> struct tuple_element<0, RexBindingTuple> {
  using type = RexLifetimeValue;
};
} // namespace std
template <std::size_t Index> RexLifetimeValue get(RexBindingTuple);
void rex_temporary_exception_probe() noexcept(
    noexcept(rex_make_lifetime_value()));

struct RexReferenceDefault {
  const RexLifetimeValue &member = rex_make_lifetime_value();
};

static RexReferenceDefault rex_static_reference_default{};
const RexLifetimeValue &rex_global_reference = rex_make_lifetime_value();
thread_local const RexLifetimeValue &rex_global_thread_reference =
    rex_make_lifetime_value();

struct RexStaticReferenceOwner {
  static const RexLifetimeValue &member;
};

const RexLifetimeValue &RexStaticReferenceOwner::member =
    rex_make_lifetime_value();

template <class T> struct RexSemanticAllocator {
  RexSemanticAllocator();
  ~RexSemanticAllocator();
};

template <class Alloc = RexSemanticAllocator<int>> struct RexSemanticDefault {
  explicit RexSemanticDefault(const Alloc & = Alloc());
};

RexSemanticDefault<> rex_semantic_default;

template <class T> struct RexSemanticIterator {
  T value;
  RexSemanticIterator operator++(int) {
    RexSemanticIterator saved = *this;
    ++value;
    return saved;
  }
};

int rex_semantic_iterator_use() {
  RexSemanticIterator<int> iterator{1};
  return (iterator++).value;
}

enum { rex_constant_extent = 1 + 2 };

int rex_semantic_wrapper_probe() {
  struct RexBindingFields {
    int first;
    int second;
  };
  static auto [rex_static_first, rex_static_second] = RexBindingFields{1, 2};
  thread_local auto [rex_thread_first, rex_thread_second] =
      RexBindingFields{3, 4};
  int array[2] = {5, 6};
  auto &[array_first, array_second] = array;
  auto [tuple_first, tuple_second] = std::pair<int, int>{7, 8};
  auto [temporary_binding] = RexBindingTuple{};
  static auto [static_temporary_binding] = RexBindingTuple{};
  thread_local auto [thread_temporary_binding] = RexBindingTuple{};
  RexLifetimeValue object_value = rex_make_lifetime_value();
  rex_make_lifetime_value();
  rex_consume_lifetime_value(rex_make_lifetime_value());
  const RexLifetimeValue &value = rex_make_lifetime_value();
  static const RexLifetimeValue &static_value = rex_make_lifetime_value();
  thread_local const RexLifetimeValue &thread_value = rex_make_lifetime_value();
  return rex_consume_lifetime_value(value) + rex_constant_extent;
}

struct RexImplicitObjectOwner {
  int member;

  int rex_implicit_object_probe() {
    auto qualified = [this] { return RexImplicitObjectOwner::member; };
    return this->member + qualified();
  }
};

void rex_cfg_uncaught_throw() {
  throw 701;
  int unreachable = 0;
  (void)unreachable;
}

int rex_cfg_nested_catches() {
  try {
    try {
      throw 702;
    } catch (long value) {
      (void)value;
      throw;
    } catch (...) {
    }
  } catch (int value) {
    return value;
  } catch (...) {
    return 2;
  }
  return 3;
}

int rex_cfg_conditional_throw(bool condition) {
  try {
    return condition ? (throw 703, 0) : 1;
  } catch (int value) {
    return value;
  }
}

void rex_cfg_nested_lambda() {
  try {
    auto uncalled = [] { throw 704; };
    (void)uncalled;
  } catch (...) {
  }
}

struct RexCfgFunctionTry {
  int member;
  RexCfgFunctionTry() try : member((throw 705, 0)) {
  } catch (...) {
  }
  ~RexCfgFunctionTry() noexcept(false) try { throw 706; } catch (...) {
  }
};

int rex_cfg_function_try() try { throw 707; } catch (...) {
  return 9;
}
