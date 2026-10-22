#include "sageAstJsonPrivate.h"

#include <cstring>
#include <limits>
#include <memory>
#include <string>
#include <type_traits>

namespace {
template <typename Float>
using Bits =
    std::conditional_t<std::is_same_v<Float, float>, uint32_t, uint64_t>;

template <typename Float> Bits<Float> bitsOf(Float value) {
  Bits<Float> bits;
  std::memcpy(&bits, &value, sizeof(bits));
  return bits;
}

template <typename Literal, typename Float>
void check(Float value, const std::string &spelling, bool edit = false) {
  using namespace Rose::AstJson;
  auto *literal = new Literal(value, spelling);
  SageInterface::setSourcePosition(literal);
  literal->set_literal_spelling_form(
      spelling.empty() ? SgValueExp::e_literal_canonical_generated
                       : SgValueExp::e_literal_source_spelled);
  if (edit) {
    literal->set_value(value);
    ROSE_ASSERT(literal->get_valueString().empty());
  }
  const JsonValue owned = parseJson(rawTypeOwnedExpressionRef(literal, {}));
  ROSE_ASSERT(owned.at("properties").find("value") == nullptr);
  auto *restored = dynamic_cast<Literal *>(expressionFromRef(owned, {}));
  ROSE_ASSERT(restored != nullptr &&
              bitsOf(restored->get_value()) == bitsOf(value));
  ROSE_ASSERT(restored->get_valueString() == literal->get_valueString());
  ROSE_ASSERT(restored->get_literal_spelling_form() ==
              literal->get_literal_spelling_form());
  ROSE_ASSERT(restored->get_isModified() == literal->get_isModified());

  SgTreeCopy copy;
  auto *copied = dynamic_cast<Literal *>(restored->copy(copy));
  ROSE_ASSERT(copied != nullptr &&
              bitsOf(copied->get_value()) == bitsOf(value));
  ROSE_ASSERT(copied->get_valueString() == literal->get_valueString());

  for (const char *invalid :
       {"null", "{}", "\"1.0\"", "[]", "[0,0,0]", "[-1]", "[4294967296]",
        "[true]", "[1.5]", "[-1,0]", "[4294967296,0]", "[0,false]"}) {
    JsonValue malformed = owned;
    malformed.object.at("properties").object.at("value_bits") =
        parseJson(invalid);
    bool rejected = false;
    try {
      (void)expressionFromRef(malformed, {});
    } catch (const std::runtime_error &) {
      rejected = true;
    }
    ROSE_ASSERT(rejected);
  }
  for (const char *field : {"value_bits", "value_string"}) {
    JsonValue malformed = owned;
    malformed.object.at("properties").object.erase(field);
    bool rejected = false;
    try {
      (void)expressionFromRef(malformed, {});
    } catch (const std::runtime_error &error) {
      rejected = std::string(error.what()).find(field) != std::string::npos;
    }
    ROSE_ASSERT(rejected);
  }
  SageInterface::deleteAST(copied);
  SageInterface::deleteAST(restored);
  SageInterface::deleteAST(literal);
}

template <typename Literal, typename Float> void checkGenerated() {
  for (Float value :
       {Float(0), -Float(0), Float(42.5), Float(-42.5),
        std::numeric_limits<Float>::min(), std::numeric_limits<Float>::max(),
        std::numeric_limits<Float>::denorm_min(),
        std::numeric_limits<Float>::infinity(),
        -std::numeric_limits<Float>::infinity()}) {
    check<Literal>(value, "");
  }
  // A nondefault quiet-NaN payload must survive without numeric conversion.
  Bits<Float> payload;
  if constexpr (std::is_same_v<Float, float>)
    payload = UINT32_C(0xffc01234);
  else
    payload = UINT64_C(0xfff8000000001234);
  Float nan;
  std::memcpy(&nan, &payload, sizeof(nan));
  check<Literal>(nan, "");
}

template <class Literal> void checkExtended(Rose::FloatingValue value) {
  using namespace Rose::AstJson;
  auto *literal = new Literal(value, "");
  SageInterface::setSourcePosition(literal);
  literal->set_literal_spelling_form(SgValueExp::e_literal_canonical_generated);
  const JsonValue owned = parseJson(rawTypeOwnedExpressionRef(literal, {}));
  auto *restored = dynamic_cast<Literal *>(expressionFromRef(owned, {}));
  ROSE_ASSERT(restored != nullptr);
  ROSE_ASSERT(restored->get_value().format() == value.format());
  ROSE_ASSERT(restored->get_value().words() == value.words());
  SgTreeCopy copier;
  auto *copied = dynamic_cast<Literal *>(restored->copy(copier));
  ROSE_ASSERT(copied != nullptr);
  ROSE_ASSERT(copied->get_value().format() == value.format());
  ROSE_ASSERT(copied->get_value().words() == value.words());
  restored->set_value(2);
  ROSE_ASSERT(restored->get_value().format() == value.format());
  ROSE_ASSERT(restored->get_value() == 2);
  ROSE_ASSERT(copied->get_value().words() == value.words());

  for (const char *invalid :
       {"null", "{}", "{\"format\":999,\"words\":[0,0,0,0]}",
        "{\"format\":0,\"words\":[0,0,0]}",
        "{\"format\":0,\"words\":[0,0,0,1]}",
        "{\"format\":1,\"words\":[0,0,65536,0]}",
        "{\"format\":1,\"words\":[0,0,1,0]}",
        "{\"format\":1,\"words\":[0,2147483648,0,0]}",
        "{\"format\":2,\"words\":[-1,0,0,0]}",
        "{\"format\":2,\"words\":[4294967296,0,0,0]}",
        "{\"format\":2,\"words\":[true,0,0,0]}",
        "{\"format\":2,\"words\":[1.5,0,0,0]}"}) {
    JsonValue malformed = owned;
    malformed.object.at("properties").object.at("value_bits") =
        parseJson(invalid);
    bool rejected = false;
    try {
      (void)expressionFromRef(malformed, {});
    } catch (const std::runtime_error &) {
      rejected = true;
    }
    ROSE_ASSERT(rejected);
  }
  SageInterface::deleteAST(copied);
  SageInterface::deleteAST(restored);
  SageInterface::deleteAST(literal);
}

void checkExtendedFormats() {
  using Value = Rose::FloatingValue;
  using Format = Value::Format;
  for (const auto words :
       {Value::Words{0, 0}, Value::Words{0, 0x8000}, Value::Words{1, 0},
        Value::Words{0x8000000000000001, 0x3fff},
        Value::Words{0x8000000000000000, 0x7fff},
        Value::Words{0xc000000000001234, 0xffff},
        Value::Words{0x8000000000001234, 0x7fff}}) {
    const auto value = Value::fromBits(Format::x87_binary80, words);
    checkExtended<SgLongDoubleVal>(value);
    checkExtended<SgFloat80Val>(value);
  }
  for (const auto words :
       {Value::Words{0, 0}, Value::Words{0, 0x8000000000000000},
        Value::Words{1, 0}, Value::Words{1, 0x3fff000000000000},
        Value::Words{0, 0x7fff000000000000},
        Value::Words{0x1234, 0xffff800000000000},
        Value::Words{0x1234, 0x7fff000000000000}}) {
    const auto value = Value::fromBits(Format::binary128, words);
    checkExtended<SgLongDoubleVal>(value);
    checkExtended<SgFloat128Val>(value);
  }
  checkExtended<SgLongDoubleVal>(
      Value::fromBits(Format::binary64, {0x3ff0000000000001, 0}));
}

void checkComplexMutation() {
  using namespace Rose::AstJson;
  auto *imaginary = SageBuilder::buildDoubleVal_nfi(2.0, "2.0");
  SageInterface::setSourcePosition(imaginary);
  auto *value =
      SageBuilder::buildComplexVal(SageBuilder::buildDoubleVal(-0.0), imaginary,
                                   SageBuilder::buildDoubleType());
  imaginary->set_value(3.0);
  const JsonValue owned = parseJson(rawTypeOwnedExpressionRef(value, {}));
  ROSE_ASSERT(owned.at("properties").find("value_string") == nullptr);
  auto *restored = isSgComplexVal(expressionFromRef(owned, {}));
  ROSE_ASSERT(restored != nullptr);
  ROSE_ASSERT(restored->get_real_value()->get_parent() == restored);
  ROSE_ASSERT(restored->get_imaginary_value()->get_parent() == restored);
  ROSE_ASSERT(bitsOf(isSgDoubleVal(restored->get_real_value())->get_value()) ==
              bitsOf(-0.0));
  auto *restored_imaginary = isSgDoubleVal(restored->get_imaginary_value());
  ROSE_ASSERT(restored_imaginary->get_value() == 3.0);
  ROSE_ASSERT(restored_imaginary->get_valueString().empty());
  SgTreeCopy copy;
  auto *copied = isSgComplexVal(restored->copy(copy));
  ROSE_ASSERT(copied != nullptr &&
              copied->get_imaginary_value() != restored_imaginary);
  restored_imaginary->set_value(4.0);
  ROSE_ASSERT(isSgDoubleVal(copied->get_imaginary_value())->get_value() == 3.0);
  SageInterface::deleteAST(copied);
  SageInterface::deleteAST(restored);
  SageInterface::deleteAST(value);
}
} // namespace

int main() {
  checkExtendedFormats();
  checkComplexMutation();
  checkGenerated<SgFloatVal, float>();
  checkGenerated<SgDoubleVal, double>();
  check<SgFloatVal>(1024.0f, "1'024.0f");
  check<SgDoubleVal>(1000.0, "1'000.0");
  check<SgDoubleVal>(100.0, "1.0d+2");
  check<SgFloatVal>(42.5f, "0x1.54p+5f");
  check<SgDoubleVal>(42.5, "0x1.54p+5");
  check<SgFloatVal>(42.5f, "1.0f", true);
  check<SgDoubleVal>(42.5, "1.0", true);
}
