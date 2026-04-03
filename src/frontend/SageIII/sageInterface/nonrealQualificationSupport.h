#ifndef ROSE_NONREAL_QUALIFICATION_SUPPORT_H
#define ROSE_NONREAL_QUALIFICATION_SUPPORT_H

#include "rosedll.h"

class SgNonrealType;
class SgType;

namespace SageInterface {

ROSE_DLL_API bool
nonrealTypeCarriesWrittenQualification(const SgNonrealType *nonreal_type);

ROSE_DLL_API bool typeCarriesWrittenNonrealQualification(const SgType *type);

} // namespace SageInterface

#endif
