
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
// This test code demonstrates the required use of name qualification in the
// base type of a typedef declaration.

struct A {

  struct B {};

  struct AB {};
};

void foobar() {

  struct A {};

  struct B {};

  struct C {};
  typedef struct A x1_type;
  typedef ::A x2_type;
  typedef struct B x3_type;
  typedef :: ::A ::B x4_type;

  // This will generate an error if the name qualification is not used
  // (even the global qualification "::" is required), since class "AB"
  // does not exit in the local version of class "A"!
  typedef :: ::A ::AB x5_type;

  // This would be an error if it used global qualification
  typedef struct C x6_type;

  // Note that if the class specifier "class" or "struct" is used here
  // then the class is built in the typedef and it is valid code. In
  // this case the "class" keyword is required. Since we can't determine
  // if the class specifier is required (not set properly in legacy frontend)
  // we have to always output the class specifier.  This can mark errors in
  // name qualification.
  class D;

  typedef class D x7_type;
  typedef struct {
    int x;
  } y;
  y yyy
      // copied from test2001_24.C
      ;
}
typedef struct {
  int x;
} y;
y yyy
    // copied from test2003_08.C
    ;
// recursive reference to the tag of an autonomous type in a typedef
typedef struct Atag3 {
  struct Atag3 *a
      // Forward declaration of __IO_FILE (name qualification not allowed here).
      ;
} A3;

// This is a case where we have to search the AST to see if this declaration
// preceeds the defining declaration.
typedef struct _IO_FILE __FILE;

struct _IO_FILE {
  int _flags;
};
namespace X {

struct _IO_FILE {
  int _flags;
};
} // namespace X

// Defining declaration for __IO_FILE
typedef X ::_IO_FILE namespace__FILE;

// Name qualification is allowed here
typedef struct _IO_FILE another__FILE;
// Name qualification is allowed here (though we always output a class specifier
// in the generated code so this is no different than the previous case).
typedef struct _IO_FILE yet_another__FILE;
