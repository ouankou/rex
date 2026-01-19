// Similar to test2014_143.C (this code fails when
// the contant folding is applied to the array type size).

// const int const_size = 4;

template< class T >
class Y
   {
  // T* array[X::const_size];
   };

class Z {};

namespace X
   {
     const int const_size = 5;

  // Type syntax not yet supported for variabel declarations.
     Y<Z[const_size]> a;
   }

void foobar (int array[X::const_size]);
