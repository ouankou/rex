// This test is from ElsaTestCases/std/13.1a.cc
class X {
  static void f();
  // ERROR(1): void f();                      // ill-formed
  // ERROR(2): void f() const;                // ill-formed
  // ERROR(3): void f() const volatile;       // ill-formed
  void g();
  void g() const;          // OK: no static g
  void g() const volatile; // OK: no static g
  void g() volatile;       // OK: no static g

  // DQ (12/11/2012): Test variations with the GNU/Clang __restrict__ extension.
  // int* h();
  // int* h() const;
  // int* h() const volatile;
  // int* h() volatile;
  int *h() __restrict__;       // Error if "int* h();" is declared
  int *h() const __restrict__; // Error if "int* h() const;" is declared
  int *h() const
      volatile __restrict__; // Error if "int* h() const volatile;" is declared
  int *h() volatile __restrict__; // Error if "int* h() volatile;" is declared
};
