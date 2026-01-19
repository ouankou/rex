struct A;
template<class T> int foo(T *t, A a);
struct A 
   {
     private:
          int x;

       // This was output with "template<>" syntax (which is a mistake for friend functions).
          friend int foo<>(int *, A);
   };

template<class T> int foo(T *t, A a) { return 44; }

void foobar()
   {
     int *x = 0L;
     A a;
     foo(x, a);
   }
