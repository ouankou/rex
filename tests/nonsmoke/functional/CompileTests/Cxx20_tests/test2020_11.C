#include <map>
using namespace std;

void foo1()
   {
     map<int,int> mymap;
     for (auto&& [first,second] : mymap) 
          { // use first and second
          }
   }

// (since C++17)
// Explanation
// The above syntax produces code equivalent to the following (__range, __begin and __end are for exposition only):
