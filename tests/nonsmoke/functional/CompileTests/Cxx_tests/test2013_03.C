// Note that the class "X" must be templated and the "operator+" must be a friend function.

template <typename T>
class X
   {
public:
  // friend X<T> & operator+( X<T> & i, X<T> & j)
  friend void foo(X<T> &i)
  // friend X & operator+( X & i, X & j)
  {
    // return i;
    i;
  }
   };

int main()
   {
     X<int> y;
  // X y,z;

  // y + z;
     foo(y);

     return 0;
   }

