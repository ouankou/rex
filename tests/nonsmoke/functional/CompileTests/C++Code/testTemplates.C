template<class T>
class ABC
   {
     public:
          T xyz;
          T foo();
   };

template<class T>
T ABC<T>::foo ()
   {
     return xyz;
   }

// If uncommented this causes an error with g++
// class ABC< int >;
class ABC<int>
   {
     public:
          int xyz;
          int foo();
   };

   int ABC<int>::foo() { return xyz; }

   int main() {
     class ABC<int> object1;
     object1.xyz = 7;
     object1.xyz = object1.foo();
     return 0;
   }

