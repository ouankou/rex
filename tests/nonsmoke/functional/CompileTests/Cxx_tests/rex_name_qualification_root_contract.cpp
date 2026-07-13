namespace rex_nq_ctor {
struct Base {
  struct Nested {};
};

struct Derived : Base, Base::Nested {
  Derived() : Base::Nested() {}
};
} // namespace rex_nq_ctor

namespace rex_nq_enum_outer {
namespace a {
enum Type { first = 0 };
}

namespace b {
enum Type { second = 0 };
}
} // namespace rex_nq_enum_outer

using namespace rex_nq_enum_outer::a;
using namespace rex_nq_enum_outer::b;

rex_nq_enum_outer::a::Type rex_nq_enum_value = rex_nq_enum_outer::a::first;

template <class T> struct RexNqTemplate {};

namespace rex_nq_template_argument {
struct Payload {};
} // namespace rex_nq_template_argument

template struct RexNqTemplate<rex_nq_template_argument::Payload>;

struct RexNqMemberContext {
  int extent() const { return 1; }

  void consume() {
    RexNqMemberContext self;
    decltype(self.extent()) value = 0;
    (void)value;
  }
};

int main() { return rex_nq_enum_value == rex_nq_enum_outer::a::first ? 0 : 1; }
