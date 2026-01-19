#ifndef ROSE_StringUtility_Predicate_H
#define ROSE_StringUtility_Predicate_H

#include "rosedll.h"

#include <string>

namespace Rose {
namespace StringUtility {

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                      Predicates
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/** Determines whether one string contains another.
 *
 *  Returns true if `longString` contains `shortString` as a subsequence.
 *
 * @param longString String to search.
 * @param shortString Subsequence to find.
 * @return True if contained. */
inline bool isContainedIn(const std::string &longString,
                          const std::string &shortString) {
  return longString.find(shortString) != std::string::npos;
}

/** Returns true if the string ends with line termination.
 *
 *  Only common ASCII-based line terminations are recognized: CR+LF, LF+CR, CR
 * (only), or LF (only).
 *
 * @param s Input string.
 * @return True if the string ends with a line terminator. */
ROSE_UTIL_API bool isLineTerminated(const std::string &s);

} // namespace StringUtility
} // namespace Rose

#endif
