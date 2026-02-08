
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
namespace N {

struct A {
  // struct B. struct B;
  // This is a declaration of a pointer to the global struct B (which is not
  // declared in global scope).
  struct B;

  struct N::B *bp;

  struct B {}

  // struct B {};
  ;
}

// Note that legacy frontend altomatically inserts a forward declaration for
;
} // namespace N

int main() {
  // struct B {};
  N ::A x
      // Note that struct B has a non-defining declaration as a struct in global
      // scope (though it was never declared in global scope).
      ;

  // N::B* y;
  N ::B *y;

  ::N::A ::B *y2
      // B z; // Of course this is an error, because the definition of b has not
      // been seen.
      ;
  // so this is allowed (because bp is a pointer to the global B not the one in
  // struct A.
  x.bp = y;

  // x.bp = y2; // This is an error since A::bp is a pointer to the global
  // struct B not the local one in this function.
}

// This will cause an error, since "x.bp = y;" will not type match.
