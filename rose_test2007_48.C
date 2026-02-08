
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
// This test code demonstates the use of forward declarations
// it is designed so that an error in how the source position
// will cause a lagitimate erro in the compilation.
// This will build a SgClassDeclaration and a SgClassDefinition
// where the class definition is marked as compiler generated.
#include "test2007_48.h"
template <typename T> class Y;

template <> class Y<int>;
// forward declaration of template specialization
#if defined(__clang__)
template <> class Y<int> {};

#endif
// Redundant forward class declaration
#if defined(__clang__)
#endif
#if defined(__clang__)
#endif
// This will build a new SgClassDeclaration, and reuse the existing
// SgClassDefinition (???).
// DQ (2/20/2010): This is a error for g++ 4.x compilers (at least g++ 4.2).
// #if (__GNUC__ >= 3)
// #if ( defined(__clang__) || (defined(__clang__) == 0 && __GNUC__ >= 3) )
// #if ( defined(__clang__) || (defined(__clang__) == 0 && __GNUC__ >= 3) )
#if (defined(__clang__) || __GNUC__ == 4)
#endif
// Redundant forward class declaration
#if defined(__clang__)
#endif
#if defined(__clang__)
#endif
