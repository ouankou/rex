#define __restrict__ 

#include<list>

using namespace std;

int main()
   {
     list<int> integerList;

     integerList.push_back(1);

  // DQ (2/6/2011): This causes a problem with the new support for sizeof.
     integerList.sort();

     return 0;
   }
