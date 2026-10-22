#include "NumberToString.h"
#include "mlog.h"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <iomanip>
#include <sstream>

namespace Rose {
namespace StringUtility {

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                      Functions for converting numbers to
//                                      strings
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

std::string numberToString(long long x) { return std::to_string(x); }

std::string numberToString(unsigned long long x) { return std::to_string(x); }

std::string numberToString(long x) { return std::to_string(x); }

std::string numberToString(unsigned long x) { return std::to_string(x); }

std::string numberToString(int x) { return std::to_string(x); }

std::string numberToString(unsigned int x) { return std::to_string(x); }

std::string numberToString(const void *x) {
  char numberString[128];
  sprintf(numberString, "%p", x);
  return std::string(numberString);
}

std::string numberToString(double x) {
  const int length = std::snprintf(nullptr, 0, "%2.2f", x);
  ROSE_ASSERT(length >= 0);
  std::string result(static_cast<std::size_t>(length) + 1, '\0');
  const int written = std::snprintf(result.data(), result.size(), "%2.2f", x);
  ROSE_ASSERT(written == length);
  result.resize(static_cast<std::size_t>(length));
  return result;
}

#if defined(__SIZEOF_INT128__)
std::string numberToString(__int128 x) {
  if (x >= 0)
    return numberToString(static_cast<unsigned __int128>(x));
  const unsigned __int128 magnitude =
      static_cast<unsigned __int128>(-(x + 1)) + 1;
  return "-" + numberToString(magnitude);
}

std::string numberToString(unsigned __int128 x) {
  std::string result;
  do {
    result.push_back('0' + static_cast<unsigned>(x % 10));
    x /= 10;
  } while (x != 0);
  std::reverse(result.begin(), result.end());
  return result;
}
#endif

std::string intToHex(uint64_t i) {
  std::ostringstream os;
  os << "0x" << std::hex << i;
  return os.str();
}

std::string appendAsmComment(const std::string &s, const std::string &comment) {
  if (comment.empty())
    return s;
  if (s.empty())
    return "<" + comment + ">";
  if (s[s.size() - 1] == '>')
    return s.substr(0, s.size() - 1) + "," + comment + ">";
  return s + "<" + comment + ">";
}

std::string toHex2(uint64_t value, size_t nbits, bool show_unsigned_decimal,
                   bool show_signed_decimal, uint64_t decimal_threshold) {
  assert(nbits > 0 && nbits <= 8 * sizeof value);
  uint64_t sign_bit = ((uint64_t)1 << (nbits - 1));

  // Hexadecimal value

  std::stringstream ss;
  ss << "0x" << std::setw((nbits + 3) / 4) << std::setfill('0') << std::hex
     << value;
  std::string retval = ss.str();

  // unsigned decimal
  bool showed_unsigned = false;
  if (show_unsigned_decimal && value >= decimal_threshold) {
    retval = appendAsmComment(retval, numberToString(value));
    showed_unsigned = true;
  }

  // signed decimal
  if (show_signed_decimal) {
    if (0 == (value & sign_bit)) {
      // This is a positive value. Don't show it if we did already.
      if (!showed_unsigned && value >= decimal_threshold)
        retval = appendAsmComment(retval, numberToString(value));
    } else {
      // This is a negative value, so show it as negative.  We have to manually
      // sign extend it first.
      int64_t signed_value = (value | (-1ll & ~(sign_bit - 1)));
      retval = appendAsmComment(retval, numberToString(signed_value));
    }
  }
  return retval;
}

std::string signedToHex2(uint64_t value, size_t nbits) {
  return toHex2(value, nbits, false, true);
}

std::string unsignedToHex2(uint64_t value, size_t nbits) {
  return toHex2(value, nbits, true, false);
}

std::string addrToString(uint64_t value, size_t nbits) {
  if (0 == nbits)
    nbits = value > 0xffffffff ? 64 : 32;
  return toHex2(value, nbits, false, false);
}

} // namespace StringUtility
} // namespace Rose
