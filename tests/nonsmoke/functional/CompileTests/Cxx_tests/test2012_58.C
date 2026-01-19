// This code is similar to test2012_57.C.
template <typename T>
class X
   {
public:
  // This function is not called so it will be listed internally in legacy
  // frontend as not "defined" and this causes some problems for the legacy
  // frontend/ROSE translation.
  friend void foo(X<T> &i) { int a; }
   };

   template <typename T> void foo(T &j) { int b; }

   void foobar() {
     X<int> x;

     // This causes the legacy frontend/ROSE connection to fail (this will cause
     // the foo explicitly defined in the global scope to be referenced).
     ::foo(x);
   }
