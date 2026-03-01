// Examples of C++11 attributes

int x [[foo]];          // foo applies to variable x
void f [[foo, bar]] (); // foo and bar apply to function f

// An attribute name can be optionally qualified with a single-level attribute
// namespace and followed by attribute arguments enclosed in parenthesis. The
// format of attribute arguments is attribute-dependent. For example:

int y [[omp::shared]];

[[noreturn]] void terminate_now();

void foo() {
  int z [[foo]] = 0;

  if (z == 0) {
  }
}
