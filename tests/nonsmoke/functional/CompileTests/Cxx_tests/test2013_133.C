

class X {};

void foo ( X & a, const X & b );
void foo ( X & a, const X & b );

void foo(X &a, const X &b = X());

void foo ( X & a, const X & b );
void foo ( X & a, const X & b );

void foobar()
   {
  X x;
  foo(x);
   }
