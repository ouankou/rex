#include "sage3basic.h"

using namespace std;

VariantVector::VariantVector(const VariantT &X) {

  size_t i = 0;

  this->push_back(X);

  do {
    SgNode::getClassHierarchySubTreeFunction((*this)[i], *this);
    i++;
  } while (i < this->size());

}

VariantVector::VariantVector(const VariantVector &X) : vector<VariantT>(X) {
}

VariantVector VariantVector::operator=(const VariantVector &X) {
  vector<VariantT>::operator=(X);
  return *this;
}

VariantVector operator+(VariantT lhs, VariantT rhs) {
  VariantVector returnVector(lhs);
  returnVector.push_back(rhs);
  return returnVector;
}

VariantVector operator+(VariantT lhs, const VariantVector &rhs) {
  VariantVector returnVector(lhs);
  returnVector.insert(returnVector.end(), rhs.begin(), rhs.end());
  return returnVector;
}

VariantVector operator+(const VariantVector &lhs, VariantT rhs) {
  VariantVector returnVector(lhs);
  returnVector.push_back(rhs);
  return returnVector;
}

VariantVector operator+(const VariantVector &lhs, const VariantVector &rhs) {
  VariantVector returnVector(lhs);
  returnVector.insert(returnVector.end(), rhs.begin(), rhs.end());
  return returnVector;
}
