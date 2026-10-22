void f() {
  typedef int T;
  int x = 0;
  x.~T();
  int y = 0;
  y.T::~T();
  int z = 0;
  int *p = &z;
  p->T::~T();
}
