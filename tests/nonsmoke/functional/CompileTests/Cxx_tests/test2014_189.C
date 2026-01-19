template <typename T>
class X
   {
public:
  static void free(X *ptr) { delete ptr; }
   };

void foo()
   {
      X<int>* a = 0L;
      X<int>::free(a);
   }

