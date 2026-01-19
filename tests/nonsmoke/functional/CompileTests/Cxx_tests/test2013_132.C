

class X {};

void foo ( X a, X b );
void foo ( X a, X b );

void foo(X a, X b = X());

void foo ( X a, X b );
void foo ( X a, X b );

void foobar()
   {
  X x;
  foo(x);
   }
