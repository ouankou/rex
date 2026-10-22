#include "Rose/StringUtility/Escape.h"

#include "Rose/StringUtility/Replace.h"

#include "Rose/StringUtility/StringToNumber.h"

#include <cctype>

#include <cstdio>

#include <cstring>

#include <iomanip>

#include <sstream>

namespace Rose {
namespace StringUtility {

std::string htmlEscape(const std::string &s) {
  std::string s2;
  for (size_t i = 0; i < s.size(); ++i) {
    switch (s[i]) {
    case '<':
      s2 += "&lt;";
      break;
    case '>':
      s2 += "&gt;";
      break;
    case '&':
      s2 += "&amp;";
      break;
    default:
      s2 += s[i];
      break;
    }
  }
  return s2;
}

std::string cEscape(char ch, char context) {
  std::string result;
  switch (ch) {
  case '\a':
    result += "\\a";
    break;
  case '\b':
    result += "\\b";
    break;
  case '\t':
    result += "\\t";
    break;
  case '\n':
    result += "\\n";
    break;
  case '\v':
    result += "\\v";
    break;
  case '\f':
    result += "\\f";
    break;
  case '\r':
    result += "\\r";
    break;
  case '\"':
    if ('"' == context) {
      result += "\\\"";
    } else {
      result += ch;
    }
    break;
  case '\'':
    if ('\'' == context) {
      result += "\\'";
    } else {
      result += ch;
    }
    break;
  case '\\':
    result += "\\\\";
    break;
  default:
    if (isprint(static_cast<unsigned char>(ch))) {
      result += ch;
    } else {
      char buf[8];
      snprintf(buf, sizeof(buf), "\\%03o", (unsigned)(unsigned char)ch);
      result += buf;
    }
    break;
  }
  return result;
}

std::string cEscape(const std::string &s, char context) {
  std::string result;
  for (char ch : s)
    result += cEscape(ch, context);
  return result;
}

std::string cUnescape(const std::string &s) {
  std::string result;
  for (size_t i = 0; i < s.size(); ++i) {
    if ('\\' == s[i] && i + 1 < s.size()) {
      ++i;
      switch (s[i]) {
      case '?':
        result += '?';
        break;
      case '"':
        result += '"';
        break;
      case '\'':
        result += '\'';
        break;
      case 'a':
        result += '\a';
        break;
      case 'b':
        result += '\b';
        break;
      case 'f':
        result += '\f';
        break;
      case 'n':
        result += '\n';
        break;
      case 'r':
        result += '\r';
        break;
      case 't':
        result += '\t';
        break;
      case 'u':
        result += "\\u"; // Unicode is not supported; leave it escaped
        break;
      case 'U':
        result += "\\U"; // Unicode is not supported; leave it escaped
        break;
      case 'v':
        result += '\v';
        break;
      case 'x': {
        unsigned byte = 0;
        while (i + 1 < s.size() &&
               isxdigit(static_cast<unsigned char>(s[i + 1])))
          byte = ((16 * byte) + hexadecimalToInt(s[++i])) & 0xff;
        result += (char)byte;
        break;
      }
      case '0':
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7': {
        unsigned byte = 0, nchars = 0;
        --i;
        while (i + 1 < s.size() && nchars++ < 3 && s[i + 1] >= '0' &&
               s[i + 1] <= '7')
          byte = 8 * byte + s[++i] - '0';
        result += (char)byte;
        break;
      }
      default:
        result += std::string("\\") + s[i];
      }
    } else {
      result += s[i];
    }
  }
  return result;
}

// Escaping special characters in shells is difficult. There are many classes of
// characters:
//
//   1. Characters that can appear unescaped and outside quotes (bare). They can
//   also appear unescaped inside single or
//      double quotes.  Most characters fall into this category.
//
//   2. Characters that must be escaped when outside quotes or (not escaped)
//   inside double or single quotes.  These are
//      characters like "*", "?", "=", "{", "}", "[", "]", etc. and consist of
//      most printable non-alphanumeric characters.
//
//   3. Characters that cannot be escaped by some shells. These are things like
//   non-printable control characters
//      and characters above 0177. The Bash shell has a special syntax for
//      escaping these in a C-like manner.
std::string bourneEscape(const std::string &s) {
  // If the string is empty it needs to be quoted.
  if (s.empty())
    return "''";

  // The presence of non-printing characters or single quotes trumps all others
  // and requires C-style quoting
  for (char ch : s) {
    if (!::isprint(static_cast<unsigned char>(ch)) || '\'' == ch)
      return "$'" + cEscape(s, '\'') + "'";
  }

  // If the string contains any shell meta characters or white space that must
  // be quoted then single-quote the entire string and escape backslashes.
  for (char ch : s) {
    if (!::isalnum(static_cast<unsigned char>(ch)) && !strchr("_-+./", ch))
      return "'" + Rose::StringUtility::replaceAllCopy(s, "\\", "\\\\") + "'";
  }

  // No quoting or escaping necessary
  return s;
}

std::string jsonEscape(const std::string &s) {
  std::string result;
  result.reserve(s.size());
  for (unsigned char ch : s) {
    switch (ch) {
    case '"':
      result += "\\\"";
      break;
    case '\\':
      result += "\\\\";
      break;
    case '\b':
      result += "\\b";
      break;
    case '\f':
      result += "\\f";
      break;
    case '\n':
      result += "\\n";
      break;
    case '\r':
      result += "\\r";
      break;
    case '\t':
      result += "\\t";
      break;
    default:
      if (ch < 0x20) {
        char buffer[7];
        snprintf(buffer, sizeof(buffer), "\\u%04x", ch);
        result += buffer;
      } else {
        result += ch;
      }
      break;
    }
  }
  return result;
}

std::string csvEscape(const std::string &s) {
  const std::string quote =
      s.find_first_of(",\r\n\"") == std::string::npos ? "" : "\"";
  return quote + Rose::StringUtility::replaceAllCopy(s, "\"", "\"\"") + quote;
}

std::string escapeNewlineAndDoubleQuoteCharacters(const std::string &X) {
  std::string result;
  for (char ch : X) {
    if (ch == '\n') {
      result += "\\l";
    } else if (ch == '"') {
      result += "\\\"";
    } else {
      result += ch;
    }
  }
  return result;
}

} // namespace StringUtility
} // namespace Rose

std::string escapeString(const std::string &s) {

  // DQ (2/4/2014): Note that this function can not be used on the asm strings
  // for the unparser.  When it can frequently work, the present of "\n" and
  // "\t" substrings causes the transformation to "\\n" and \\t" which is a bug
  // when used with the asmembler.  So a special version of this function that
  // does not process "\n" to "\\n" is defined in the unparser.  Note that we
  // still need to process '\n' to "\n" and this behavior is still preserved.
  // Test codes demonstrating this are in test2014_83-87.c.

  std::string result;
  for (size_t i = 0; i < s.length(); ++i) {
    switch (s[i]) {
    case '\\':
      result += "\\\\";
      break;
    case '"':
      result += "\\\"";
      break;
    case '\a':
      result += "\\a";
      break;
    case '\f':
      result += "\\f";
      break;
    case '\n':
      result += "\\n";
      break;
    case '\r':
      result += "\\r";
      break;
    case '\t':
      result += "\\t";
      break;
    case '\v':
      result += "\\v";
      break;
    default:
      if (isprint(static_cast<unsigned char>(s[i]))) {
        result.push_back(s[i]);
      } else {
        std::ostringstream stream;
        stream << '\\';
        stream << std::setw(3) << std::setfill('0') << std::oct
               << (unsigned)(unsigned char)(s[i]);
        result += stream.str();
      }
      break;
    }
  }
  return result;
}
