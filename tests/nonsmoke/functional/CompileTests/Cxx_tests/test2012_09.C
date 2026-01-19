#include <list>

#include <vector>

#include <map>

using namespace std;

int main() {
  // DQ (11/19/2004): Temporarily commented out since this is a demonstrated bug
  // now that we qualify everything!
  list<int> integerList;

  integerList.push_back(1);

  int sumOverList = 0;
  for (list<int>::iterator i = integerList.begin(); i != integerList.end();
       i++) {
    sumOverList += *i;
  }

  integerList.sort();

  return 0;
}
