
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
// This and example showing the static const integer data member in-class
// initialization! It does not work for non-iteger types (like float or double),
// but it does work for char, long, etc. Under some non-standard conditions
// doubles and floats can be initialized in-class though I think it is handled
// sufficiently differently in different compilers o make it painful. More info
// at:
//      http://www.cqf.info/forum/
//      C++ static double in class
// #include<stdlib.h>

class X {
public:
  static const int maxIntValue = 3;
  static const long maxLongValue = 3;
  static const char maxCharValue = 'Z'
      // Not allowed for float, double, or pointers (interesting!)
      ;
  // static const float  maxFloatValue  = 3.0;
  // static const double maxDoubleValue = 3.0;
  // static const size_t maxSizeTValue = 3;
  // static const int   *maxIntPointerValue = 0L;
  // types of data members that can't be initalized in-class
  static const double pi;

  const int tuncatedPi;
  const double e
      // this is the only way to initalize a non-static const variable (integer
      // or double)
      ;

  X()
      : tuncatedPi(3), e(2.71
                         // This is the only way to initialize a static const
                         // data member which is non-integer based
                       ) {}
}

// Notice that we can initialize static constants within the class!
;

const double X::pi = 3.14
    // #if SWIG
    ;
#define STORAGE

class Y {
// static const double pi = 3.141592653589793238462643383279; // Pi to 30
// places
// This is allowed by legacy frontend, but not by g++ (g++ needs constant
// to be static) const double pi = 3.141592653589793238462643383279; //
// Pi to 30 places
// Code that will compile with legacy frontend
// const double pi = 3.141592653589793238462643383279; // Pi to 30 places
// Code that we should generate so that we can compile with g++
// static const double pi = 3.141592653589793238462643383279; // Pi to 30
// places
#ifdef USE_ROSE
  // DQ (3/23/2014): When ROSE is using legacy frontend 4.7 we can specify
  // const double pi = 3.141... but when we use legacy frontend 4.8, we
  // have to follow GNU more closely. STORAGE const double pi
  // = 3.141592653589793238462643383279; // Pi to 30 places
  // Version for when ROSE is using legacy frontend 4.8.
public:
  double pi;

#else
#endif
}

// This is allowed by GNU but not by legacy frontend
;

void foo() {
  class X x;
  class X *xptr;
  const double gamma = 7.89;
  const int integerConst = 42;
#ifdef USE_ROSE
  double var1 = X::pi;

#else
#endif
  double var2 = x.e;

  double var3 = gamma;
  int var4 = integerConst
      // Access via static qualifier works fine but access via data member filed
      ;
  // generates error: "x->3;" The fix (to legacy frontend/Sage translation)
  // was to make these generate the same code (as it should be)
  int var5 = X::maxIntValue;

  int var6 = x.X::maxIntValue;
  int var7 = x.X::maxLongValue;
  char var8 = x.X::maxCharValue;
  int var9 = xptr->X::maxIntValue;
  double var10 =
      (double)(double)xptr
          // ROSE can properly handle integer constants but we need to handle
          ->pi;
  // floating point constants as well (which is non-standard in C++).
  // Since we are close this is likely worth fixing.
  class Y y;

  class Y *yptr;
#ifdef USE_ROSE
  // This is the version that works for ROSE legacy frontend 4.7 and before,
  // but with legacy frontend 4.8... double var11 = Y::pi;     // This works
  // Version for when ROSE is using legacy frontend 4.8.
  double var11 = y
                     // This works
                     .pi;

#else
#endif
  double var12 = y
                     // This does not work presently
                     .pi;

  double var13 = yptr->pi;
  // This does not work presently
}
