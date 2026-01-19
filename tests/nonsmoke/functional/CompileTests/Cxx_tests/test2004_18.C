// Only the class and constant integer cases seem to work!

template<class T>
class TestClassArgument
   {
     public:
          T xyz;
          T foo();
   };

   template <class T> T TestClassArgument<T>::foo() { return xyz; }

   template <int T> class TestIntegerArgument {
   public:
     int xyz;
     int foo();
   };

   template <int T> int TestIntegerArgument<T>::foo() { return T; }

   int main() {
     // It seems that only one of these can be turned on at a time!

     TestClassArgument<int> object1;
     object1.xyz = 7;
     object1.xyz = object1.foo();

     TestIntegerArgument<2> object2;
     object2.xyz = object2.foo();

     return 0;
   }


