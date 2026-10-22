#ifndef ROSE_Integer128Value_H
#define ROSE_Integer128Value_H

#include <algorithm>
#include <array>
#include <cstdint>
#include <ostream>
#include <string>

namespace Rose {
// Exact two's-complement target data, independent of host integer extensions.
class Integer128Value {
public:
  using Words = std::array<std::uint64_t, 2>;
  Integer128Value() = default;
  static Integer128Value fromBits(Words words, bool is_unsigned) {
    Integer128Value value;
    value.words_ = words;
    value.unsigned_ = is_unsigned;
    return value;
  }
  const Words &words() const { return words_; }
  bool isUnsigned() const { return unsigned_; }
  bool isNegative() const { return !unsigned_ && (words_[1] >> 63); }
  bool operator==(Integer128Value other) const {
    return words_ == other.words_ && unsigned_ == other.unsigned_;
  }
  bool operator!=(Integer128Value other) const { return !(*this == other); }
  friend std::ostream &operator<<(std::ostream &out, Integer128Value value) {
    return out << value.decimal();
  }

  // Neither C nor C++ provides a 128-bit integer token suffix. Both the
  // frontend's canonical declaration-name formatter and source emission use
  // this exact constant expression; stored semantics remain the numeric words.
  std::string cxxExpression() const {
    std::string result = unsigned_ ? "(" : "((__int128)(";
    result += "((unsigned __int128)" + std::to_string(words_[1]) +
              "ULL << 64) | " + std::to_string(words_[0]) + "ULL";
    result += unsigned_ ? ")" : "))";
    return result;
  }

  std::string decimal() const {
    Words magnitude = words_;
    if (isNegative()) {
      magnitude[0] = ~magnitude[0] + 1;
      magnitude[1] = ~magnitude[1] + (magnitude[0] == 0);
    }
    std::array<std::uint32_t, 4> digits{
        static_cast<std::uint32_t>(magnitude[0]),
        static_cast<std::uint32_t>(magnitude[0] >> 32),
        static_cast<std::uint32_t>(magnitude[1]),
        static_cast<std::uint32_t>(magnitude[1] >> 32)};
    std::string result;
    do {
      std::uint64_t remainder = 0;
      for (std::size_t i = digits.size(); i > 0; --i) {
        const std::uint64_t dividend = (remainder << 32) | digits[i - 1];
        digits[i - 1] = static_cast<std::uint32_t>(dividend / 10);
        remainder = dividend % 10;
      }
      result.push_back(static_cast<char>('0' + remainder));
    } while (std::any_of(digits.begin(), digits.end(),
                         [](std::uint32_t word) { return word != 0; }));
    if (isNegative())
      result.push_back('-');
    std::reverse(result.begin(), result.end());
    return result;
  }

private:
  Words words_{};
  bool unsigned_ = false;
};
} // namespace Rose
#endif
