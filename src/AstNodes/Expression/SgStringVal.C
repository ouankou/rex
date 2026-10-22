#include "sage3basic.h"

#include <cctype>
#include <iomanip>
#include <limits>
#include <sstream>

namespace {
std::string escapedCxxCharacter(uint32_t value) {
  switch (value) {
  case '\0':
    return "\\0";
  case '\a':
    return "\\a";
  case '\b':
    return "\\b";
  case '\f':
    return "\\f";
  case '\n':
    return "\\n";
  case '\r':
    return "\\r";
  case '\t':
    return "\\t";
  case '\v':
    return "\\v";
  case '\\':
    return "\\\\";
  case '"':
    return "\\\"";
  default:
    return {};
  }
}
static std::string escapeOrdinaryStringLiteralContentsForUnparse(
    const SgStringCodeUnitList &contents) {
  std::string escaped;
  escaped.reserve(contents.size());
  bool last_hex_escape = false;

  for (uint32_t ch : contents) {
    switch (ch) {
    case '\\':
      escaped.append("\\\\");
      last_hex_escape = false;
      break;
    case '\n':
      escaped.append("\\n");
      last_hex_escape = false;
      break;
    case '\r':
      escaped.append("\\r");
      last_hex_escape = false;
      break;
    case '"':
      escaped.append("\\\"");
      last_hex_escape = false;
      break;
    default:
      if (!(ch >= 0x20 && ch <= 0x7e) ||
          (last_hex_escape && std::isxdigit(ch))) {
        std::stringstream escape;
        escape << "\\x" << std::uppercase << std::hex << std::setw(2)
               << std::setfill('0') << static_cast<unsigned>(ch);
        escaped.append(escape.str());
        last_hex_escape = true;
      } else {
        escaped.push_back(static_cast<char>(ch));
        last_hex_escape = false;
      }
      break;
    }
  }

  return escaped;
}

static void appendUnicodeEscape(std::string &escaped, uint32_t code_point) {
  std::stringstream escape;
  escape << std::uppercase << std::hex << std::setfill('0');
  if (code_point <= 0xFFFF) {
    escape << "\\u" << std::setw(4) << code_point;
  } else {
    escape << "\\U" << std::setw(8) << code_point;
  }
  escaped.append(escape.str());
}

static void appendHexEscape(std::string &escaped, uint32_t code_unit) {
  std::stringstream escape;
  escape << "\\x" << std::uppercase << std::hex << code_unit;
  escaped.append(escape.str());
}

static void appendOctalEscape(std::string &escaped, uint32_t code_unit) {
  ROSE_ASSERT(code_unit <= 0xFF);
  std::stringstream escape;
  escape << '\\' << std::setfill('0') << std::setw(3) << std::oct << code_unit;
  escaped.append(escape.str());
}

static std::string escapeWideStringLiteralCodeUnitsForUnparse(
    const SgStringCodeUnitList &code_units, bool is_wide, bool is_utf16) {
  std::string escaped;
  bool last_hex_escape = false;

  for (size_t ii = 0; ii < code_units.size(); ++ii) {
    uint32_t content_val = code_units[ii];

    if (is_utf16 && content_val >= 0xD800 && content_val <= 0xDBFF &&
        ii + 1 < code_units.size()) {
      uint32_t low_surrogate = code_units[ii + 1];
      if (low_surrogate >= 0xDC00 && low_surrogate <= 0xDFFF) {
        content_val =
            0x10000 + (((content_val & 0x3FF) << 10) | (low_surrogate & 0x3FF));
        ++ii;
      }
    }

    if ((content_val >= 0xD800 && content_val <= 0xDFFF) ||
        content_val > 0x10FFFF) {
      appendHexEscape(escaped, content_val);
      last_hex_escape = true;
    } else if (!is_wide && content_val > 0xFF) {
      appendUnicodeEscape(escaped, content_val);
      last_hex_escape = false;
      continue;
    } else if (content_val == 0 && last_hex_escape) {
      appendOctalEscape(escaped, content_val);
      last_hex_escape = false;
      continue;
    } else if (auto mapped = escapedCxxCharacter(content_val);
               !mapped.empty()) {
      escaped.append(mapped);
      last_hex_escape = false;
      continue;
    } else if (content_val > 0xFF ||
               (last_hex_escape && content_val <= 0xFF &&
                std::isxdigit(static_cast<unsigned char>(content_val)))) {
      appendHexEscape(escaped, content_val);
      last_hex_escape = true;
    } else if (!(content_val >= 0x20 && content_val <= 0x7e)) {
      appendOctalEscape(escaped, content_val);
      last_hex_escape = false;
      continue;
    } else {
      escaped.push_back(static_cast<char>(content_val));
      last_hex_escape = false;
      continue;
    }
  }

  return escaped;
}

} // namespace

SgType *SgStringVal::get_type(void) const {
  if (get_cxx_unevaluated()) {
    fprintf(stderr,
            "REX_AST_INVARIANT[syntax-expression-type]: unevaluated C++ "
            "string-literal syntax has no standalone semantic value type\n");
    ROSE_ABORT();
  }
  SgType *literalType = get_literal_type();
  if (literalType == nullptr || isSgTypeUnknown(literalType) != nullptr ||
      isSgTypeDefault(literalType) != nullptr) {
    fprintf(stderr,
            "REX_AST_INVARIANT[string-literal-semantic-type]: string literal "
            "has no exact producer-owned semantic type\n");
    ROSE_ABORT();
  }

  SgType *baseType = literalType->stripType(SgType::STRIP_TYPEDEF_TYPE |
                                            SgType::STRIP_MODIFIER_TYPE);
  if (get_stringDelimiter() != 0) {
    if (isSgTypeString(baseType) == nullptr) {
      fprintf(stderr,
              "REX_AST_INVARIANT[string-literal-semantic-type]: Fortran "
              "literal does not own an exact CHARACTER semantic type\n");
      ROSE_ABORT();
    }
    return literalType;
  }

  validate_cxx_literal_state();
  SgArrayType *arrayType = isSgArrayType(baseType);
  SgType *elementType =
      arrayType != nullptr ? arrayType->get_base_type() : nullptr;
  if (elementType != nullptr) {
    elementType = elementType->stripType(SgType::STRIP_TYPEDEF_TYPE |
                                         SgType::STRIP_MODIFIER_TYPE);
  }
  // The encoding is a lexical role, while Clang's exact semantic code-unit
  // type also depends on the language and initialization context.  C exposes
  // wchar_t/char16_t/char32_t through their target integer carrier types, and
  // Clang preserves its accepted character-array initialization extensions by
  // assigning an ordinary literal the destination signed/unsigned-char
  // element type.  C++20 UTF-8 literals instead use char8_t.  Keep these
  // producer-owned distinctions; do not rewrite them to one guessed code-unit
  // type.
  const bool ordinaryCodeUnit = isSgTypeChar(elementType) != nullptr ||
                                isSgTypeSignedChar(elementType) != nullptr ||
                                isSgTypeUnsignedChar(elementType) != nullptr;
  const bool wideCodeUnit = isSgTypeWchar(elementType) != nullptr ||
                            isSgTypeShort(elementType) != nullptr ||
                            isSgTypeUnsignedShort(elementType) != nullptr ||
                            isSgTypeInt(elementType) != nullptr ||
                            isSgTypeUnsignedInt(elementType) != nullptr;
  const bool utf16CodeUnit = isSgTypeChar16(elementType) != nullptr ||
                             isSgTypeUnsignedShort(elementType) != nullptr;
  const bool utf32CodeUnit = isSgTypeChar32(elementType) != nullptr ||
                             isSgTypeUnsignedInt(elementType) != nullptr;
  const bool elementMatches =
      (get_literal_encoding() == e_string_encoding_ordinary &&
       ordinaryCodeUnit) ||
      (get_literal_encoding() == e_string_encoding_utf8 &&
       (ordinaryCodeUnit || isSgTypeChar8(elementType) != nullptr)) ||
      (get_literal_encoding() == e_string_encoding_wide && wideCodeUnit) ||
      (get_literal_encoding() == e_string_encoding_utf16 && utf16CodeUnit) ||
      (get_literal_encoding() == e_string_encoding_utf32 && utf32CodeUnit);
  if (arrayType == nullptr || arrayType->get_index() == nullptr ||
      !elementMatches) {
    fprintf(stderr,
            "REX_AST_INVARIANT[string-literal-semantic-type]: C/C++ literal "
            "does not own an exact array type matching encoding=%d "
            "literal-type=%s stripped-type=%s element-type=%s index=%p\n",
            static_cast<int>(get_literal_encoding()),
            literalType->class_name().c_str(),
            baseType != nullptr ? baseType->class_name().c_str() : "<null>",
            elementType != nullptr ? elementType->class_name().c_str()
                                   : "<null>",
            static_cast<void *>(arrayType != nullptr ? arrayType->get_index()
                                                     : nullptr));
    ROSE_ABORT();
  }
  return literalType;
}

void SgStringVal::post_construction_initialization() {}

const SgStringCodeUnitList &SgStringVal::get_value() const { return p_value; }

SgStringCodeUnitList
SgStringVal::code_units_from_bytes(const std::string &bytes) {
  SgStringCodeUnitList result;
  result.reserve(bytes.size());
  for (unsigned char byte : bytes)
    result.push_back(byte);
  return result;
}

std::string SgStringVal::get_byte_value() const {
  std::string result;
  result.reserve(p_value.size());
  for (uint32_t unit : p_value) {
    if (unit > 0xff) {
      fprintf(stderr,
              "REX_AST_INVARIANT[string-byte-value]: code unit=%u cannot "
              "be represented as one byte\n",
              unit);
      ROSE_ABORT();
    }
    result.push_back(static_cast<char>(unit));
  }
  return result;
}

void SgStringVal::set_value(const SgStringCodeUnitList &value) {
  if (value.size() != p_value.size() && get_literal_type() != nullptr) {
    ROSE_ASSERT(value.size() <
                static_cast<size_t>(std::numeric_limits<int>::max()));
    SgType *type = get_type()->stripTypedefsAndModifiers();
    const bool cxx = get_stringDelimiter() == 0;
    SgIntVal *length = SageBuilder::buildIntVal_nfi(
        static_cast<int>(value.size() + (cxx ? 1 : 0)), "");
    SageBuilder::initializeSemanticExpressionSourceProvenance(length);
    if (cxx) {
      auto *array = isSgArrayType(type);
      ROSE_ASSERT(array != nullptr);
      set_literal_type(
          SageBuilder::buildArrayType(array->get_base_type(), length));
    } else {
      auto *string = isSgTypeString(type);
      ROSE_ASSERT(string != nullptr);
      SgExpression *kind = string->get_type_kind();
      set_literal_type(SgTypeString::createType(
          length,
          kind != nullptr ? SageInterface::copyExpression(kind) : nullptr));
    }
  }
  p_value = value;
  p_isRawString = false;
  p_raw_string_delimiter.clear();
  p_raw_string_payload.clear();
  markAsModified();
}

std::string SgStringVal::get_cxx_literal_contents() const {
  validate_cxx_literal_state();
  const bool wide = get_literal_encoding() == e_string_encoding_wide;
  const bool utf16 = get_literal_encoding() == e_string_encoding_utf16;
  if (wide || utf16 || get_literal_encoding() == e_string_encoding_utf32)
    return escapeWideStringLiteralCodeUnitsForUnparse(p_value, wide, utf16);
  return escapeOrdinaryStringLiteralContentsForUnparse(p_value);
}

void SgStringVal::set_usesSingleQuotes(bool usesSingleQuotes) {
  if (usesSingleQuotes) {
    set_stringDelimiter('\'');
  } else if (get_usesSingleQuotes()) {
    // unset only if the current delimiter uses single quotes
    set_stringDelimiter(0);
  }
}

void SgStringVal::set_usesDoubleQuotes(bool usesDoubleQuotes) {
  if (usesDoubleQuotes) {
    set_stringDelimiter('"');
  } else if (get_usesDoubleQuotes()) {
    // unset only if the current delimiter uses double quotes
    set_stringDelimiter(0);
  }
}

void SgStringVal::validate_cxx_literal_state() const {
  switch (get_literal_encoding()) {
  case e_string_encoding_ordinary:
  case e_string_encoding_wide:
  case e_string_encoding_utf8:
  case e_string_encoding_utf16:
  case e_string_encoding_utf32:
    break;
  default:
    fprintf(stderr,
            "REX_AST_INVARIANT[cxx-string-encoding]: invalid encoding=%d\n",
            static_cast<int>(get_literal_encoding()));
    ROSE_ABORT();
  }

  if (get_stringDelimiter() != 0) {
    fprintf(stderr,
            "REX_AST_INVARIANT[cxx-string-delimiter]: C/C++ string literal "
            "carries a Fortran delimiter=%d\n",
            static_cast<int>(get_stringDelimiter()));
    ROSE_ABORT();
  }

  const uint32_t maximum =
      get_literal_encoding() == e_string_encoding_ordinary ||
              get_literal_encoding() == e_string_encoding_utf8
          ? 0xffu
      : get_literal_encoding() == e_string_encoding_utf16 ? 0xffffu
                                                          : 0xffffffffu;
  for (uint32_t unit : p_value) {
    if (unit > maximum) {
      fprintf(
          stderr,
          "REX_AST_INVARIANT[string-code-unit]: value=%u exceeds encoding=%d\n",
          unit, static_cast<int>(get_literal_encoding()));
      ROSE_ABORT();
    }
  }

  if (!get_isRawString()) {
    if (!get_raw_string_delimiter().empty() ||
        !get_raw_string_payload().empty()) {
      fprintf(stderr,
              "REX_AST_INVARIANT[cxx-raw-string-state]: non-raw literal "
              "carries raw delimiter or payload state\n");
      ROSE_ABORT();
    }
    return;
  }

  const std::string &delimiter = get_raw_string_delimiter();
  if (delimiter.size() > 16) {
    fprintf(stderr,
            "REX_AST_INVARIANT[cxx-raw-string-delimiter]: delimiter has "
            "length=%zu, maximum is 16\n",
            delimiter.size());
    ROSE_ABORT();
  }
  for (unsigned char ch : delimiter) {
    if (std::iscntrl(ch) || ch < 0x21 || ch > 0x7e || ch == '(' || ch == ')' ||
        ch == '\\') {
      fprintf(stderr,
              "REX_AST_INVARIANT[cxx-raw-string-delimiter]: delimiter "
              "contains forbidden byte=%u\n",
              static_cast<unsigned>(ch));
      ROSE_ABORT();
    }
  }

  const std::string terminator = ")" + delimiter + "\"";
  if (get_raw_string_payload().find(terminator) != std::string::npos) {
    fprintf(stderr,
            "REX_AST_INVARIANT[cxx-raw-string-payload]: payload contains its "
            "own closing delimiter\n");
    ROSE_ABORT();
  }
}

std::string SgStringVal::get_cxx_literal_spelling() const {
  validate_cxx_literal_state();

  std::string prefix;
  switch (get_literal_encoding()) {
  case e_string_encoding_ordinary:
    break;
  case e_string_encoding_wide:
    prefix = "L";
    break;
  case e_string_encoding_utf8:
    prefix = "u8";
    break;
  case e_string_encoding_utf16:
    prefix = "u";
    break;
  case e_string_encoding_utf32:
    prefix = "U";
    break;
  default:
    ROSE_ABORT();
  }

  if (get_isRawString()) {
    const std::string &delimiter = get_raw_string_delimiter();
    return prefix + "R\"" + delimiter + "(" + get_raw_string_payload() + ")" +
           delimiter + "\"";
  }
  return prefix + "\"" + get_cxx_literal_contents() + "\"";
}
