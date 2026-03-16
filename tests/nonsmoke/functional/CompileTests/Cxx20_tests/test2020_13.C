#include <string>
#include <utility>
using namespace std;

// C++17 example should difficulty: see
// https://en.cppreference.com/w/cpp/language/range-for

// struct cow_string { /* ... */ }; // a copy-on-write string
struct cow_string {
  string x;

  auto begin() { return x.begin(); }
  auto end() { return x.end(); }
  auto begin() const { return x.begin(); }
  auto end() const { return x.end(); }
}; // a copy-on-write string

// cow_string str = "abcdefg" /* ... */;
cow_string str = {.x = "abcdefg"};

// for(auto x : str) { /* ... */ } // may cause deep copy

void foobar() {
  for (auto x : std::as_const(str)) { /* ... */
  }
}
