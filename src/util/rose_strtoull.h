#ifndef ROSE_strtoull_H
#define ROSE_strtoull_H

#include "rosedll.h"
#include <inttypes.h>

/** Convert a string to an unsigned long integer.
 *
 *  This function is the same as the system strtoull() except it also allows
 * `base` to be two, in which case it parses a binary literal consisting of '0'
 * and '1' bits.  If `base` is zero and the first non-whitespace characters of
 * the string are '0b' then bits follow.
 *
 * @param nptr Input string to parse.
 * @param endptr Receives pointer to the first unparsed character.
 * @param base Numeric base for parsing.
 * @return Parsed unsigned value. */
ROSE_UTIL_API uint64_t rose_strtoull(const char *nptr, char **endptr, int base);

#endif
