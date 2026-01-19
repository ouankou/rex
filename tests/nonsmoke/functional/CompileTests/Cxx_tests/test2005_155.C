
#define DEFINE_IN_CLASS 1

class X
   {
     public:
          void foobar()
             {
               foo<int>(1,2,3);
             }

#if DEFINE_IN_CLASS
          template <typename T>
          void foo(int i, int j, int k) {}
#else
          template <typename T>
          void foo(int i, int j, int k);
#endif
   };

#if !DEFINE_IN_CLASS
template <typename T>
void X::foo() {}
#endif

// Template instantiation Directive for a member function
template void X::foo<int>(int i, int j, int k);
