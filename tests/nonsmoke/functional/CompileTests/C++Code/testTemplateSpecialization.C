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

int main()
   {
     ABC<int> object1;
     object1.xyz = 7;
     object1.xyz = object1.foo();
     return 0;
   }

