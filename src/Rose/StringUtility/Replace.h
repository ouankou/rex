#ifndef ROSE_StringUtility_Replace_H
#define ROSE_StringUtility_Replace_H

#include "rosedll.h"

#include <string>

namespace Rose {
namespace StringUtility {

/** Replace every occurrence of a nonempty pattern in a string.
 *
 * @param input Input string.
 * @param search Substring to replace.
 * @param replacedWith Replacement text.
 * @return Updated string.
 */
ROSE_UTIL_API std::string replaceAllCopy(const std::string &input,
                                         const std::string &search,
                                         const std::string &replacedWith);

} // namespace StringUtility
} // namespace Rose

#endif
