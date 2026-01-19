
// This test code demonstrates the use of a private type in the template argument list.

#include <map>

// This works fine
// std::map<int, int>::iterator it;

class foo
   {
     public:
       // This works fine
       // std::map<int, int>::iterator it;
       void doSomething() {
         // This fails
         std::map<int, int>::iterator it;
       }
};

void doSomething()
   {
  // This works fine
  // std::map<int, int>::iterator it;
}
