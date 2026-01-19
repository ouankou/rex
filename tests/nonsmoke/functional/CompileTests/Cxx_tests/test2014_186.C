template <typename T>
class X
   {
public:
  static void free(X *x);
   };

   template <typename T> void X<T>::free(X *ptr) {}

   void foo() {
     X<int> *a = 0L;
     X<int>::free(a);
   }

