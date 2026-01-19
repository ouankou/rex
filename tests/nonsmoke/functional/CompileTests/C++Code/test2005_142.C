// In a problem related to test2005_141.C from the following code:
//      const Face &FBC = *(S.faceBegin() + fid);
// we generated:
//      const Face &FBC = *(S.faceBegin())+fid;
// where "fid" is of integer type

#include <vector>

#include <list>

#include <map>

#include <string>

using namespace std;

vector<int>::iterator getIterator(const vector<int> &X);

class myVector {
public:
  const myVector &operator*() const;

  myVector();

  // Presence of explicit copy constructor causes code generation to be "value =
  // b-c.norm();"
  myVector(const class myVector &x);

  // An explicit operator= appears to have no effect!
  // myVector operator= (const class vector &x) const;
};

double min(double x, double y);

void foo() {
  myVector *a = NULL;

  // Problem code
  myVector &b = *(a + 1);

  int offset;
}
