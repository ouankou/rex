// tps (01/14/2010) : Switching from rose.h to sage3.
#include "sage3basic.h"

// DQ (10/14/2010):  This should only be included by source files that require
// it. This fixed a reported bug which caused conflicts with configure-time
// macros (e.g. PACKAGE_BUGREPORT).
#include "rose_config.h"
#include "rtiHelpers.h"

using namespace std;

std::ostream &operator<<(std::ostream &os, const SgName &n) {
  return os << "\"" << n.str() << "\"";
}
std::ostream &operator<<(std::ostream &os,
                         const SgAsmStmt::AsmRegisterNameList &bv) {
  for (unsigned int i = 0; i < bv.size(); i++) {
    if (i != 0)
      os << ", ";
    os << ((long)(bv[i]));
  }
  return os;
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

void doRTI(const char *fieldNameBase, void *fieldPtr, size_t fieldSize,
           void *thisPtr, const char *className, const char *typeString,
           const char *fieldName, const std::string &fieldContents,
           RTIMemberData &memberData) {
#if ROSE_USE_VALGRIND
  auto shouldCheckDefinedForType = [](const char *typeName) {
    if (typeName == nullptr) {
      return false;
    }
    const std::string type(typeName);
    if (type.find('<') != std::string::npos ||
        type.find("std::") != std::string::npos ||
        type.find("Rose_STL_Container") != std::string::npos ||
        type.find("List") != std::string::npos ||
        type.find("Set") != std::string::npos ||
        type.find("Map") != std::string::npos ||
        type.find("SgName") != std::string::npos) {
      return false;
    }
    if (type.find('*') != std::string::npos) {
      return true;
    }
    if (type.find("enum") != std::string::npos) {
      return true;
    }
    return type == "bool" || type == "char" || type == "signed char" ||
           type == "unsigned char" || type == "short" ||
           type == "unsigned short" || type == "int" ||
           type == "unsigned int" || type == "long" ||
           type == "unsigned long" || type == "long long" ||
           type == "unsigned long long" || type == "size_t";
  };

  if (shouldCheckDefinedForType(typeString)) {
    doUninitializedFieldCheck(fieldNameBase, fieldPtr, fieldSize, thisPtr,
                              className);
  }
#else
  (void)fieldNameBase;
  (void)fieldPtr;
  (void)fieldSize;
  (void)thisPtr;
  (void)className;
#endif
  memberData = RTIMemberData(typeString, fieldName, fieldContents);
}
