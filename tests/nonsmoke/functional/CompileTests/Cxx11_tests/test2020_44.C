struct A;
template<class T> int foo(T t, A a);
template<class T> int foo(T *t, A a);
struct A 
   {
          A() : x(42) { }
     private:
          int x;

       // This was output with "template<>" syntax (which is a mistake for friend functions).
          friend int foo<>(int *, A);
   };

template<class T> int foo(T t, A a)  { return a.x + 43; }
template<class T> int foo(T *t, A a) { return a.x + 44; }

void foobar()
   {
     int *x = 0L;
     A a;
     foo(x, a);
   }
