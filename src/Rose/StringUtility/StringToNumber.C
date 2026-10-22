#include "StringToNumber.h"

#include <ctype.h>
namespace Rose {
namespace StringUtility {

unsigned hexadecimalToInt(char ch) {
  const unsigned char byte = static_cast<unsigned char>(ch);
  if (isxdigit(byte)) {
    if (isdigit(byte))
      return ch - '0';
    if (isupper(byte))
      return ch - 'A' + 10;
    return ch - 'a' + 10;
  }
  return 0;
}

} // namespace StringUtility
} // namespace Rose
