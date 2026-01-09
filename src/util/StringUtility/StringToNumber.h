#ifndef ROSE_StringUtility_StringToNumber_H
#define ROSE_StringUtility_StringToNumber_H

#include <rosedll.h>

#include <cctype>
#include <limits>
#include <optional>
#include <string>
#include <type_traits>
namespace Rose {
namespace StringUtility {

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                      Number parsing
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/** Convert an ASCII hexadecimal character to an integer.
 *
 *  Converts the characters 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, a, b, c, d, e, f, A, B, C, D, E, and F into their hexadecimal integer
 *  equivalents. Returns zero if the input character is not in this set.
 *
 * @return Integer value for the character, or zero on failure. */
ROSE_UTIL_API unsigned hexadecimalToInt(char);

template<typename T>
class NumberParseResult {
  bool ok_;
  T value_;
  std::string error_message_;

 public:
  explicit NumberParseResult(const T &value) : ok_(true), value_(value) {}
  explicit NumberParseResult(std::string message)
      : ok_(false), value_(), error_message_(std::move(message)) {}

  explicit operator bool() const { return ok_; }
  const T &operator*() const { return value_; }
  const T *operator->() const { return &value_; }

  std::optional<std::string> error() const {
    if (ok_) {
      return std::nullopt;
    }
    return error_message_;
  }
};

template<typename T>
inline NumberParseResult<T>
toNumber(const std::string &input) {
  static_assert(std::is_integral<T>::value,
                "Rose::StringUtility::toNumber requires an integral type");

  using Unsigned = typename std::make_unsigned<T>::type;
  const bool is_signed = std::numeric_limits<T>::is_signed;

  auto make_error = [](const std::string &message) {
    return NumberParseResult<T>(message);
  };

  if (input.empty()) {
    return make_error("syntax error: digits expected");
  }

  size_t idx = 0;
  bool negative = false;
  if (input[idx] == '+' || input[idx] == '-') {
    if (!is_signed) {
      return make_error("syntax error: sign not allowed for unsigned types");
    }
    negative = (input[idx] == '-');
    ++idx;
    if (idx >= input.size()) {
      return make_error("syntax error: digits expected");
    }
  }

  bool has_prefix = false;
  int base = 10;
  if (idx + 1 < input.size() && input[idx] == '0') {
    const char next = input[idx + 1];
    if (next == 'x' || next == 'X') {
      base = 16;
      idx += 2;
      has_prefix = true;
    } else if (next == 'b' || next == 'B') {
      base = 2;
      idx += 2;
      has_prefix = true;
    }
  }

  Unsigned max_allowed = static_cast<Unsigned>(std::numeric_limits<T>::max());
  if (is_signed && negative) {
    max_allowed = max_allowed + 1;
  }

  Unsigned value = 0;
  size_t digits = 0;
  bool last_sep = false;

  auto overflow_message = [&]() {
    return negative ? std::string("overflow error: less than minimum value for type")
                    : std::string("overflow error: greater than maximum value for type");
  };

  auto invalid_digit_message = [&]() {
    std::string msg = "syntax error: invalid digit after parsing ";
    msg += std::to_string(digits);
    msg += (digits == 1) ? " digit" : " digits";
    return msg;
  };

  for (; idx < input.size(); ++idx) {
    const char ch = input[idx];
    if (ch == '_') {
      if (digits == 0) {
        if (has_prefix && !last_sep) {
          last_sep = true;
          continue;
        }
        return make_error("syntax error: separator not allowed before first digit");
      }
      if (last_sep) {
        return make_error("syntax error: invalid use of digit separator");
      }
      last_sep = true;
      continue;
    }

    int digit = -1;
    if (base == 10) {
      if (ch >= '0' && ch <= '9') {
        digit = ch - '0';
      }
    } else if (base == 2) {
      if (ch == '0' || ch == '1') {
        digit = ch - '0';
      }
    } else {
      if (std::isxdigit(static_cast<unsigned char>(ch))) {
        digit = static_cast<int>(hexadecimalToInt(ch));
      }
    }

    if (digit < 0 || digit >= base) {
      return make_error(invalid_digit_message());
    }

    if (value > (max_allowed - static_cast<Unsigned>(digit)) /
                     static_cast<Unsigned>(base)) {
      return make_error(overflow_message());
    }

    value = value * static_cast<Unsigned>(base) +
            static_cast<Unsigned>(digit);
    ++digits;
    last_sep = false;
  }

  if (digits == 0) {
    return make_error("syntax error: digits expected");
  }
  if (last_sep) {
    return make_error("syntax error: invalid use of digit separator");
  }

  if (negative) {
    const Unsigned max_neg = max_allowed;
    if (value > max_neg) {
      return make_error("overflow error: less than minimum value for type");
    }
    if (value == max_neg) {
      return NumberParseResult<T>(std::numeric_limits<T>::min());
    }
    T signed_value = static_cast<T>(value);
    return NumberParseResult<T>(static_cast<T>(-signed_value));
  }

  if (value > static_cast<Unsigned>(std::numeric_limits<T>::max())) {
    return make_error("overflow error: greater than maximum value for type");
  }
  return NumberParseResult<T>(static_cast<T>(value));
}
} // namespace
} // namespace

#endif
