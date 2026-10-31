namespace rex_outer {
namespace rex_inner {
enum class rex_color { red, green };
struct rex_point {
  int x;
  int y;
};
template <typename T> struct rex_box {
  T value;
};
int rex_value = 3;
} // namespace rex_inner
namespace rex_alias = rex_inner;
} // namespace rex_outer

namespace rex_other {
struct rex_point {
  double x;
};
} // namespace rex_other

namespace rex_outer {
namespace rex_inner {
int rex_reopened(rex_point point) { return point.x + rex_value; }
} // namespace rex_inner
} // namespace rex_outer

using namespace rex_outer;

int rex_member_pointer_user(rex_inner::rex_point &point) {
  int rex_inner::rex_point::*member = &rex_inner::rex_point::y;
  return point.*member;
}

int rex_switch(rex_alias::rex_color color) {
  switch (color) {
  case rex_alias::rex_color::red:
    return 1;
  case rex_inner::rex_color::green:
    return 2;
  }
  return 0;
}

struct {
  int rex_anonymous_field;
} rex_anonymous_object;

int main() {
  rex_outer::rex_inner::rex_box<rex_other::rex_point> box{};
  decltype(box) copy = box;
  rex_inner::rex_point point{1, 2};
  auto lambda = [&](rex_inner::rex_point p) {
    return p.x + rex_inner::rex_value;
  };
  return rex_member_pointer_user(point) +
         rex_switch(rex_inner::rex_color::red) + lambda(point) +
         rex_inner::rex_reopened(point) + static_cast<int>(copy.value.x) +
         rex_anonymous_object.rex_anonymous_field;
}
