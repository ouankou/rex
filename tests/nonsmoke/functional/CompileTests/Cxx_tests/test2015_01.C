template <typename T>
class X
   {
public:
  template <typename S> void foobar(S t) {};
   };

void foo()
   {
     X<int> a;
   }


