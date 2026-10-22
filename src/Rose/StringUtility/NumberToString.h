#ifndef ROSE_StringUtility_NumberToString_H
#define ROSE_StringUtility_NumberToString_H

#include "rosedll.h"

#include <cstdint>

#include <string>

namespace Rose {
namespace StringUtility {

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                      Functions for converting numbers to
//                                      strings
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/** Convert an integer to a string.
 *
 * @return String representation of the input value.
 *
 *  @{ */
ROSE_UTIL_API std::string numberToString(long long);
ROSE_UTIL_API std::string numberToString(unsigned long long);
ROSE_UTIL_API std::string numberToString(long);
ROSE_UTIL_API std::string numberToString(unsigned long);
ROSE_UTIL_API std::string numberToString(int);
ROSE_UTIL_API std::string numberToString(unsigned int);

#if defined(__SIZEOF_INT128__)
ROSE_UTIL_API std::string numberToString(__int128 x);
ROSE_UTIL_API std::string numberToString(unsigned __int128 x);
#endif

/** @} */

/** Convert a pointer to a string.
 *
 * @return String representation of the pointer. */
ROSE_UTIL_API std::string numberToString(const void *);

/** Convert a floating-point number to a string.
 *
 *  The returned string uses `printf` with "%2.2f" format.
 *
 * @return String representation of the input value. */
ROSE_UTIL_API std::string numberToString(double);

/** Convert an integer to a hexadecimal string.
 *
 * @return Hexadecimal string. */
ROSE_UTIL_API std::string intToHex(uint64_t);

/** Convert a number to a hexadecimal and decimal string.
 *
 *  The returned string starts with the hexadecimal representation of the number
 * and an optional decimal representation in angle brackets.  The decimal
 * portion will contain a signed and/or unsigned value depending on whether the
 * value is interpreted as signed and whether the sign bit is set. The
 * signedToHex versions print the decimal value for only the signed
 * interpretation; the unsignedToHex versions print only the decimal unsigned
 * interpretation, and toHex2 can print both without repetition.
 *
 * @param value Input value.
 * @param nbits Bit width to display.
 * @param show_unsigned_decimal Whether to show unsigned decimal.
 * @param show_signed_decimal Whether to show signed decimal.
 * @param decimal_threshold Threshold for showing decimal.
 * @return Formatted string.
 *
 *  @{ */
ROSE_UTIL_API std::string toHex2(uint64_t value, size_t nbits,
                                 bool show_unsigned_decimal = true,
                                 bool show_signed_decimal = true,
                                 uint64_t decimal_threshold = 256);
ROSE_UTIL_API std::string signedToHex2(uint64_t value, size_t nbits);
ROSE_UTIL_API std::string unsignedToHex2(uint64_t value, size_t nbits);

template <typename T> std::string signedToHex(T value) {
  return signedToHex2((uint64_t)value, 8 * sizeof(T));
}
template <typename T> std::string unsignedToHex(T value) {
  return unsignedToHex2((uint64_t)value, 8 * sizeof(T));
}
/** @} */

/** Convert a virtual address to a string.
 *
 *  Converts a virtual address to a hexadecimal string with a leading "0x". The
 * string is zero-padded so that it explicitly represents at least `nbits` bits
 * (four bits per hexadecimal digits). If `nbits` is zero then the function uses
 * 32 bits for values that fit in 32 bits, otherwise 64 bits.
 *
 * @param value Address value.
 * @param nbits Minimum bit width to display.
 * @return Formatted address string. */
ROSE_UTIL_API std::string addrToString(uint64_t value, size_t nbits = 0);

} // namespace StringUtility
} // namespace Rose

#endif
