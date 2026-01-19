

void foo_B (int x);

void foobar() {

  int x;
  //   while (0)
  for (;;)
  //      if ( ({ union { int i; } u; u.i = 42; }) == 0)
  {
    foo_B(({
      union {
        int i;
      } u;
      u.i = 42;
    }));
    foo_B(({
      union {
        int i;
      } u;
      u.i = 42;
    }));
    foo_B(({
      union {
        int i;
      } u;
      u.i = 42;
    }));
    foo_B(({
      union {
        int i;
      } u;
      u.i = 42;
    }));
    foo_B(({
      union {
        int i;
      } u;
      u.i = 42;
    }));
    foo_B(({
      union {
        int i;
      } u;
      u.i = 42;
    }));
    break;
  }
}
