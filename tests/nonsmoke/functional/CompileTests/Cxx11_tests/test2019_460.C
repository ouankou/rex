#include <memory>

template < typename T >
class XXX
   {
     public :
       // std::shared_ptr<T> m_data;
       // int x;
       void foobar();
   };

void error()
   {
     XXX<int> a;
  // This line is required to demonstrate the bug in ROSE.
     a.foobar();
}
