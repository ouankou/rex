#include "VectorBoolTemplate.h"

enum { rex_test19_anchor = 0 };

void VectorBoolTemplate<int>::touch() { bits.push_back(true); }

int main() {
  VectorBoolTemplate<int> value;
  value.touch();
  return value.bits.empty() ? 1 : 0;
}
