#include "Rose/FloatingValue.h"
#include <cfloat>
#include <cstring>
#include <limits>
#include <llvm/ADT/APFloat.h>
#include <llvm/ADT/SmallString.h>
#include <ostream>
#include <stdexcept>

namespace Rose {
namespace {
using Format = FloatingValue::Format;
const llvm::fltSemantics &semantics(Format format) {
  switch (format) {
  case Format::binary64:
    return llvm::APFloat::IEEEdouble();
  case Format::x87_binary80:
    return llvm::APFloat::x87DoubleExtended();
  case Format::binary128:
    return llvm::APFloat::IEEEquad();
  }
  throw std::invalid_argument("Unknown floating value format");
}
llvm::APFloat unpack(FloatingValue value) {
  const unsigned width = FloatingValue::bitWidth(value.format());
  return llvm::APFloat(
      semantics(value.format()),
      llvm::APInt(width, llvm::ArrayRef<uint64_t>(value.words().data(),
                                                  (width + 63) / 64)));
}
FloatingValue pack(Format format, const llvm::APFloat &value) {
  if (&value.getSemantics() != &semantics(format))
    throw std::logic_error(
        "Floating value format differs from its arithmetic semantics");
  const auto bits = value.bitcastToAPInt();
  FloatingValue::Words words{};
  for (unsigned i = 0; i < bits.getNumWords(); ++i)
    words[i] = bits.getRawData()[i];
  return FloatingValue::fromBits(format, words);
}
Format commonFormat(FloatingValue lhs, FloatingValue rhs) {
  return FloatingValue::bitWidth(lhs.format()) >=
                 FloatingValue::bitWidth(rhs.format())
             ? lhs.format()
             : rhs.format();
}
template <class Operation>
FloatingValue arithmetic(FloatingValue lhs, FloatingValue rhs,
                         Operation operation) {
  const auto format = commonFormat(lhs, rhs);
  auto left = unpack(lhs.converted(format));
  operation(left, unpack(rhs.converted(format)));
  return pack(format, left);
}
llvm::APFloat::cmpResult compare(FloatingValue lhs, FloatingValue rhs) {
  const auto format = commonFormat(lhs, rhs);
  return unpack(lhs.converted(format)).compare(unpack(rhs.converted(format)));
}
} // namespace

unsigned FloatingValue::bitWidth(Format format) {
  return llvm::APFloat::semanticsSizeInBits(semantics(format));
}
FloatingValue::Format FloatingValue::nativeFormat() {
  static_assert(std::numeric_limits<long double>::radix == 2);
  if constexpr (LDBL_MANT_DIG == 53 && LDBL_MAX_EXP == 1024)
    return Format::binary64;
  else if constexpr (LDBL_MANT_DIG == 64 && LDBL_MAX_EXP == 16384)
    return Format::x87_binary80;
  else if constexpr (LDBL_MANT_DIG == 113 && LDBL_MAX_EXP == 16384)
    return Format::binary128;
  else
    throw std::logic_error("Native long double has no supported binary format");
}
FloatingValue::FloatingValue() : format_(nativeFormat()), words_{} {}
FloatingValue::FloatingValue(const long double &value) : FloatingValue() {
  const unsigned bytes = bitWidth(format_) / 8;
  unsigned char representation[sizeof(long double)];
  // Copy only value bytes, excluding the indeterminate x87 ABI tail padding.
  std::memcpy(representation, &value, bytes);
  const uint16_t endian = 1;
  const bool little = *reinterpret_cast<const unsigned char *>(&endian) == 1;
  if (!little && format_ == Format::x87_binary80)
    throw std::logic_error(
        "Native big-endian x87 storage requires an explicit ABI layout");
  for (unsigned i = 0; i < bytes; ++i) {
    const unsigned significance = little ? i : bytes - 1 - i;
    words_[significance / 8] |= uint64_t(representation[i])
                                << (8 * (significance % 8));
  }
}
FloatingValue FloatingValue::fromBits(Format format, Words words) {
  const unsigned width = bitWidth(format);
  if ((width == 64 && words[1] != 0) || (width == 80 && words[1] >> 16 != 0))
    throw std::invalid_argument(
        "Floating value contains bits outside its format");
  if (format == Format::x87_binary80) {
    const unsigned exponent = words[1] & 0x7fff;
    const bool integer = (words[0] >> 63) != 0;
    if ((exponent != 0) != integer)
      throw std::invalid_argument(
          "Floating value contains a noncanonical x87 encoding");
  }
  FloatingValue result;
  result.format_ = format;
  result.words_ = words;
  return result;
}
FloatingValue FloatingValue::converted(Format format) const {
  if (format_ == format)
    return *this;
  auto value = unpack(*this);
  bool losesInfo = false;
  value.convert(semantics(format), llvm::APFloat::rmNearestTiesToEven,
                &losesInfo);
  return pack(format, value);
}
bool FloatingValue::isNegative() const { return unpack(*this).isNegative(); }
bool FloatingValue::isZero() const { return unpack(*this).isZero(); }
bool FloatingValue::isInfinite() const { return unpack(*this).isInfinity(); }
bool FloatingValue::isNaN() const { return unpack(*this).isNaN(); }
bool FloatingValue::isSignaling() const { return unpack(*this).isSignaling(); }
std::string FloatingValue::decimal() const {
  llvm::SmallString<64> buffer;
  unpack(*this).toString(buffer);
  return buffer.str().str();
}
FloatingValue FloatingValue::operator-() const {
  auto value = unpack(*this);
  value.changeSign();
  return pack(format_, value);
}
FloatingValue operator+(FloatingValue lhs, FloatingValue rhs) {
  return arithmetic(lhs, rhs, [](auto &left, const auto &right) {
    left.add(right, llvm::APFloat::rmNearestTiesToEven);
  });
}
FloatingValue operator-(FloatingValue lhs, FloatingValue rhs) {
  return arithmetic(lhs, rhs, [](auto &left, const auto &right) {
    left.subtract(right, llvm::APFloat::rmNearestTiesToEven);
  });
}
FloatingValue operator*(FloatingValue lhs, FloatingValue rhs) {
  return arithmetic(lhs, rhs, [](auto &left, const auto &right) {
    left.multiply(right, llvm::APFloat::rmNearestTiesToEven);
  });
}
FloatingValue operator/(FloatingValue lhs, FloatingValue rhs) {
  return arithmetic(lhs, rhs, [](auto &left, const auto &right) {
    left.divide(right, llvm::APFloat::rmNearestTiesToEven);
  });
}
bool operator==(FloatingValue lhs, FloatingValue rhs) {
  return compare(lhs, rhs) == llvm::APFloat::cmpEqual;
}
bool operator<(FloatingValue lhs, FloatingValue rhs) {
  return compare(lhs, rhs) == llvm::APFloat::cmpLessThan;
}
std::ostream &operator<<(std::ostream &out, FloatingValue value) {
  return out << value.decimal();
}
} // namespace Rose
