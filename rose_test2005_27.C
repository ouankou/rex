
struct __NSConstantString_tag {
  const int *isa;
  int flags;
  const char *str;
  long length;
};

struct __va_list_tag {
  unsigned int gp_offset;
  unsigned int fp_offset;
  void *overflow_arg_area;
  void *reg_save_area;
};
/*
   This demonstrates a error in the unparsing of #include directives relative to
   the "{" and "}" of a block, class definition, enum, namespace, etc.  The
   level of detail was not previously implemented in ROSE.  It was not always
   required, but since I was fixing test2005_26.C and that test code made the
   problem more visible, I decided to fix it correctly so that we generate
   better quality code now and we avoid potential pathological problems related
   to the position of commented relative to the closing brace of a block, enum,
   namespace or class definition. I believe that all possible cases are not
   handled.
 */
#include "test2005_26.h"

void foo() {
  double pi;
#include "test2005_26.h"
}

void foobar() {
  double pi;
  if (true) {
#include "test2005_26.h"
  }

  else {
#include "test2005_26.h"
  }
  {
#include "test2005_26.h"
  }

  enum enumType {
    unknownValue = 0
    /* Comment inside of enum! */
  };
  for (int i = 0; i < 10; i++) {
#include "test2005_26.h"
  }

  switch (true) {
  case false: {
#include "test2005_26.h"
  }

    /* comment in case */
    /* comment in between case and default */
  default: {
#include "test2005_26.h"
  }

    /* comment in default case */
  }

/* Comment in switch (top) */
#include "test2005_26.h"
}

/* Comment in switch (bottom) */
namespace X1 {}

namespace X2 {
namespace XX {}

} // namespace X2

namespace X3 {

class XX {
#include "test2005_26.h"
};
// Example of "}" in a namespace (test that we correctly jump over it when
// searching for the ending brace associated with the namespace).
enum numbers { zero, one, two };

enum more_numbers { three, four, five } more_numbers_variable;
} // namespace X3

class Y {
#include "test2005_26.h"
};

// DQ (5/11/2016): GNU 6.1 does not allow a global variable to be named "main".
// int main;
