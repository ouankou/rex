// This is a copy of test2005_150.C (with some modifications).

// Debug ambiguous template specialization if template <> X :: X() {}

template <typename T>
class X
   {
     public:
//        X(int t) {};
          X(int tXXX) {};
   };

   int main() { X<int> c(42); }
