

template <typename T>
class X
   {
     public:
          friend X<T> & operator+( X<T> & i, X<T> & j)
             {
               return i;
             }
   };

   template <typename T> T &operator+(T &ii, T &jj) { return ii; }

   // template X<int> & operator+( X<int> & ii, X<int> & jj);

   int main() {
     X<int> y,z;

  // Error used to be unparses as: "x = (+< X< int > > (y,z));"
     y + z;

     return 0;
   }
