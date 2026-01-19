template <typename T> bool operator== (T a,int b);

// #if 1
class X
   {
     public:
// #if 1
          template <typename T>
          friend bool operator== (T,int);
// #endif
   };
// #endif

   template <typename T> bool operator==(T a, int b) { return false; }
