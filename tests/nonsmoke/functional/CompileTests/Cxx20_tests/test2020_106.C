// DQ (7/22/2020): legacy frontend 6.0 can't handl the STL string header file.
#include <compare>
#include <string>

class Base {
public:
  auto operator<=>(const Base &) const = default;
};

class TotallyOrdered : Base {
  std::string tax_id;
  std::string first_name;
  std::string last_name;

public:
  // custom operator<=> because we want to compare last names first:
  std::strong_ordering operator<=>(const TotallyOrdered &that) const {
    if (auto cmp = static_cast<const Base &>(*this) <=>
                   static_cast<const Base &>(that);
        cmp != 0)
      return cmp;
    if (auto cmp = last_name <=> that.last_name; cmp != 0)
      return cmp;
    if (auto cmp = first_name <=> that.first_name; cmp != 0)
      return cmp;
    return tax_id <=> that.tax_id;
  }
  // ... non-comparison functions ...
};

// DQ (7/21/2020): Moved function calls into a function.
void foobar1() {
  // compiler generates all four relational operators
  TotallyOrdered to1, to2;

  // std::set<TotallyOrdered> s; // ok
  // s.insert(to1); // ok

  if (to1 <= to2) { /*...*/
  } // ok, single call to <=>
}
