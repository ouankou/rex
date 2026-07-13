#include "sage3basic.h"

#include <cctype>

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
                            isSgTypeInt(elementType) != nullptr;
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

void SgStringVal::post_construction_initialization() {
  // We can't initialize this to NULL since it might have just been set!
  // p_value = (char*)0L;
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
  return prefix + "\"" + get_value() + "\"";
}
