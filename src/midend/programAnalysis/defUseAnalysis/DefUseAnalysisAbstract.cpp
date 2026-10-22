#include "DefUseAnalysisAbstract.h"

#include "sage3basic.h"

using namespace std;

bool DefUseAnalysisAbstract::isDoubleExactEntry(const multitype *multi,
                                                SgInitializedName *name,
                                                SgNode *sgNode) {
  for (const auto &entry : *multi) {
    if (entry.first == name && entry.second == sgNode) {
      return true;
    }
  }
  return false;
}

bool DefUseAnalysisAbstract::searchMulti(const multitype *multi,
                                         SgInitializedName *initName) {
  for (const auto &entry : *multi) {
    if (entry.first == initName) {
      return true;
    }
  }
  return false;
}

bool DefUseAnalysisAbstract::checkElementsForChange(const multitype *t1,
                                                    const multitype *t2) {
  typedef set<pair<SgInitializedName *, SgNode *>> st;
  st s1(t1->begin(), t1->end());
  st s2(t2->begin(), t2->end());
  ROSE_ASSERT(s1.size() == t1->size());

  if (s2.size() != t2->size()) {
    cerr << "s2.size()=" << s2.size() << endl;
    cerr << "t2.size()=" << t2->size() << endl;
    dfa->printMultiMap(t2);
    ROSE_ABORT();
  }

  return s1 != s2;
}
