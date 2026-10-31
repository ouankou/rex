#ifndef ROSE_FloatingValue_H
#define ROSE_FloatingValue_H

#include "rosedll.h"

#include <array>
#include <cstdint>
#include <iosfwd>
#include <string>

namespace Rose {
// Exact target floating data. Words are least significant first, independent
// of the host's byte order and long-double representation.
class ROSE_DLL_API FloatingValue {
public:
  enum class Format { binary64, x87_binary80, binary128 };
  using Words = std::array<std::uint64_t, 2>;

  FloatingValue();
  // Import storage without an FP load/store; see EXT-VALGRIND-X87-EMULATION
  // in https://github.com/ouankou/rex/issues/885.
  FloatingValue(const long double &value);
  static FloatingValue fromBits(Format format, Words words);
  static Format nativeFormat();
  static unsigned bitWidth(Format format);

  Format format() const { return format_; }
  const Words &words() const { return words_; }
  FloatingValue converted(Format format) const;
  bool isNegative() const;
  bool isZero() const;
  bool isInfinite() const;
  bool isNaN() const;
  bool isSignaling() const;
  explicit operator bool() const { return !isZero(); }
  std::string decimal() const;
  FloatingValue operator-() const;

  friend FloatingValue operator+(FloatingValue, FloatingValue);
  friend FloatingValue operator-(FloatingValue, FloatingValue);
  friend FloatingValue operator*(FloatingValue, FloatingValue);
  friend FloatingValue operator/(FloatingValue, FloatingValue);
  friend bool operator==(FloatingValue, FloatingValue);
  friend bool operator!=(FloatingValue lhs, FloatingValue rhs) {
    return !(lhs == rhs);
  }
  friend bool operator<(FloatingValue, FloatingValue);
  friend bool operator>(FloatingValue lhs, FloatingValue rhs) {
    return rhs < lhs;
  }
  friend bool operator<=(FloatingValue lhs, FloatingValue rhs) {
    return lhs < rhs || lhs == rhs;
  }
  friend bool operator>=(FloatingValue lhs, FloatingValue rhs) {
    return rhs <= lhs;
  }
  friend std::ostream &operator<<(std::ostream &, FloatingValue);

private:
  Format format_;
  Words words_;
};
} // namespace Rose
#endif
