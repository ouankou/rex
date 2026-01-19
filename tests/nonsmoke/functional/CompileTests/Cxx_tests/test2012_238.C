template <typename T>
class X
   {
     public:
          friend void foo( X<T> & i )
             {
             }
   };

template <typename T>
void foo( T & j )
   {
   }

void foobar()
   {
     X<int> x;

     foo(x);
}
