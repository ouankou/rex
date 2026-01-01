/*
bug 369: template instantiation function declaration's forward declaration should not be unparsed.
https://outreach.scidac.gov/tracker/

namespace namespace1
{
template < class T > void foo ( T t ) { }
}

void namespace1::foo(int t); // WRONG !!!!

class className 
{
  public: inline void bar(int value)
  {
    namespace1::foo(value);
  }
} ;
*/
namespace namespace1 {
  template <class T> void foo(T t) {}
}

class className {
  public:
    void bar(int value) {
      namespace1::foo<int>(value);
    }
};
