template <typename T>
class X
   {
public:
  template <typename S> void free(X *ptr) {
    S s_value;
    delete ptr;
  }
   };

void foo()
   {
      X<int>* a = 0L;
      a->free<long>(a);
   }

