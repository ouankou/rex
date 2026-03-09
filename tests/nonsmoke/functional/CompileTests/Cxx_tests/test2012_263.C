// This test is from ElsaTestCases/std/13.1a.cc
class X {
  int *h() __restrict__; // Error if "int* h();" is declared
  // int* h() __restrict__ { return 0L; }; // Error if "int* h();" is declared
  int *h() const __restrict__; // Error if "int* h() const;" is declared
  int *h() const
      volatile __restrict__; // Error if "int* h() const volatile;" is declared
  int *h() volatile __restrict__; // Error if "int* h() volatile;" is declared
};

int *X::h() __restrict__ { return 0L; }

int *X::h() const __restrict__ { return 0L; }

int *X::h() const volatile __restrict__ { return 0L; }

int *X::h() volatile __restrict__ { return 0L; }
