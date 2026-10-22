#include "rtiHelpers.h"
#include "sage3basic.h"

std::ostream &operator<<(std::ostream &os, const SgName &n) {
  return os << "\"" << n.str() << "\"";
}
std::ostream &operator<<(std::ostream &os,
                         const SgDataStatementObjectPtrList &) {
  return os;
}
std::ostream &operator<<(std::ostream &os,
                         const SgDataStatementValuePtrList &) {
  return os;
}
std::ostream &operator<<(std::ostream &os, const SgCommonBlockObjectPtrList &) {
  return os;
}
std::ostream &operator<<(std::ostream &os, const SgDimensionObjectPtrList &) {
  return os;
}
std::ostream &operator<<(std::ostream &os, const SgLabelSymbolPtrList &) {
  return os;
}
std::ostream &operator<<(std::ostream &os, const SgFormatItemPtrList &) {
  return os;
}
