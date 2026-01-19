

#include <vector>

std::vector<int> intList;
// std::vector<int>::const_iterator intListInterator = intList.begin();

void foo()
   {
     std::vector<int>::const_iterator intListInteratorDirectAssignment = intList.begin();
     std::vector<int>::const_iterator intListInterator;
     intListInterator = intList.begin();
   }
 


