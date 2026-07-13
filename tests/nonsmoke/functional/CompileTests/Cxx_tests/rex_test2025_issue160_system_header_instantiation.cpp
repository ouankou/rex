#include "rex_test2025_issue160_system_header_instantiation.h"

template int rex_test2025_issue160::scale_and_shift<int>(int, int);
template struct rex_test2025_issue160::DefaultedWrapper<int>;

namespace rex_test2025_issue160 {

template int namespace_adjust<int>(int);

} // namespace rex_test2025_issue160

int main() {
  rex_test2025_issue160::Wrapper wrapper;
  wrapper.value = 41;
  int stored = 7;
  int other = 9;
  rex_test2025_issue160::DefaultedWrapper<int> defaulted{&stored};
  rex_test2025_issue160::SystemHeaderIterator<int> begin{&stored};
  rex_test2025_issue160::SystemHeaderIterator<int> end{&other};
  rex_test2025_issue160::ForwardOnly<int> *forward = nullptr;
  bool distinct = begin != end;

  int scaled = rex_test2025_issue160::scale_and_shift(2, 3);
  int adjusted = rex_test2025_issue160::namespace_adjust(8);
  return forward == nullptr && distinct &&
                 (wrapper.bump() + scaled + adjusted + *defaulted.value) > 0
             ? 0
             : 1;
}
