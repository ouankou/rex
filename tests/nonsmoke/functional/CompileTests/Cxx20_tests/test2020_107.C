#include <cctype>
#include <compare>
#include <string>

#include <set>

static std::weak_ordering case_insensitive_compare(const char *lhs,
                                                   const char *rhs) {
  while (*lhs != '\0' && *rhs != '\0') {
    const unsigned char lhs_char = static_cast<unsigned char>(*lhs);
    const unsigned char rhs_char = static_cast<unsigned char>(*rhs);
    const unsigned char lhs_lower =
        static_cast<unsigned char>(std::tolower(lhs_char));
    const unsigned char rhs_lower =
        static_cast<unsigned char>(std::tolower(rhs_char));
    if (lhs_lower < rhs_lower) {
      return std::weak_ordering::less;
    }
    if (lhs_lower > rhs_lower) {
      return std::weak_ordering::greater;
    }

    ++lhs;
    ++rhs;
  }

  if (*lhs == '\0' && *rhs == '\0') {
    return std::weak_ordering::equivalent;
  }

  return (*lhs == '\0') ? std::weak_ordering::less
                        : std::weak_ordering::greater;
}

class CaseInsensitiveString {
  std::string s;

public:
  std::weak_ordering operator<=>(const CaseInsensitiveString &b) const {
    return case_insensitive_compare(s.c_str(), b.s.c_str());
  }
  std::weak_ordering operator<=>(const char *b) const {
    return case_insensitive_compare(s.c_str(), b);
  }
  // ... non-comparison functions ...
};

// DQ (7/21/2020): Moved function calls into a function.
void foobar1() {
  // Compiler generates all four relational operators
  CaseInsensitiveString cis1, cis2;
  std::set<CaseInsensitiveString> s; // ok
  s.insert(cis1);                    // ok
  if (cis1 <= cis2) {                /*...*/
  } // ok, performs one comparison operation

  // Compiler also generates all eight heterogeneous relational operators
  if (cis1 <= "xyzzy") { /*...*/
  } // ok, performs one comparison operation
  if ("xyzzy" >= cis1) { /*...*/
  } // ok, identical semantics
}
