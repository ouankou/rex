

// template <typename T> T foo();
int foo();

template <typename T>
void foo2()
   {
     for (T thing = foo(); auto& x : thing.items()) { /* ... */ } // OK
   }
