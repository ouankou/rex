#include "rose.h"

#include "unparse_format.h"

#include <sstream>
#include <streambuf>
#include <string>

namespace {
class RejectingStreamBuffer final : public std::streambuf {
protected:
  int_type overflow(int_type) override { return traits_type::eof(); }
  int sync() override { return -1; }
};
} // namespace

int main(int argc, char **argv) {
  if (argc == 2 && std::string(argv[1]) == "--pragma") {
    SgSourceFile *sourceFile = SageBuilder::buildGeneratedSourceFile(
        "rex_unparser_compact_output_pragma.cpp");
    ASSERT_not_null(sourceFile);
    sourceFile->set_Cxx_only(true);
    sourceFile->set_outputLanguage(SgFile::e_Cxx_language);
    SgGlobal *global = sourceFile->get_globalScope();
    ASSERT_not_null(global);
    SgPragmaDeclaration *pragma =
        SageBuilder::buildPragmaDeclaration("omp parallel", global);
    ASSERT_not_null(pragma);
    SageInterface::appendStatement(pragma, global);
    SgUnparse_Info info;
    info.set_current_source_file(sourceFile);
    info.set_current_scope(global);
    info.set_language(SgFile::e_Cxx_language);
    info.set_SkipComments();
    info.set_SkipWhitespaces();
    info.set_SkipCPPDirectives();
    return pragma->unparseToString(&info) == "#pragma omp parallel\n" ? 0 : 11;
  }
  if (argc == 2 && std::string(argv[1]) == "--missing-finalize") {
    std::ostringstream output;
    UnparseFormat format(&output);
    format.set_compact_output(true);
    format << "unfinished";
    return 0;
  }
  if (argc == 2 && std::string(argv[1]) == "--inactive-finalize") {
    std::ostringstream output;
    UnparseFormat format(&output);
    format.finalize_compact_output();
    return 0;
  }
  if (argc == 2 && std::string(argv[1]) == "--failed-compact-output") {
    RejectingStreamBuffer buffer;
    std::ostream output(&buffer);
    UnparseFormat format(&output);
    format.set_compact_output(true);
    format << "payload";
    return 0;
  }
  if (argc == 2 && std::string(argv[1]) == "--failed-compact-finalize") {
    RejectingStreamBuffer buffer;
    std::ostream output(&buffer);
    UnparseFormat format(&output);
    format.set_compact_output(true);
    format.finalize_compact_output();
    return 0;
  }
  if (argc == 2 && std::string(argv[1]) == "--invalid-string-encoding") {
    SgStringVal *literal = SageBuilder::buildStringVal("invalid");
    literal->set_literal_encoding(
        static_cast<SgStringVal::string_literal_encoding_enum>(999));
    literal->unparseToString();
    return 0;
  }
  if (argc == 2 && std::string(argv[1]) == "--invalid-cxx-string-delimiter") {
    SgStringVal *literal = SageBuilder::buildStringVal("invalid");
    literal->set_stringDelimiter('"');
    literal->unparseToString();
    return 0;
  }
  if (argc == 2 && std::string(argv[1]) == "--invalid-raw-string-state") {
    SgStringVal *literal = SageBuilder::buildStringVal("invalid");
    literal->set_raw_string_delimiter("tag");
    literal->unparseToString();
    return 0;
  }
  if (argc == 2 && std::string(argv[1]) == "--invalid-raw-string-delimiter") {
    SgStringVal *literal = SageBuilder::buildStringVal("invalid");
    literal->set_isRawString(true);
    literal->set_raw_string_delimiter("bad delimiter");
    literal->set_raw_string_payload("payload");
    literal->unparseToString();
    return 0;
  }
  if (argc == 2 &&
      std::string(argv[1]) == "--invalid-raw-string-control-delimiter") {
    SgStringVal *literal = SageBuilder::buildStringVal("invalid");
    literal->set_isRawString(true);
    literal->set_raw_string_delimiter(std::string(1, '\x01'));
    literal->set_raw_string_payload("payload");
    literal->unparseToString();
    return 0;
  }
  if (argc == 2 && std::string(argv[1]) == "--invalid-raw-string-payload") {
    SgStringVal *literal = SageBuilder::buildStringVal("invalid");
    literal->set_isRawString(true);
    literal->set_raw_string_delimiter("tag");
    literal->set_raw_string_payload("payload)tag\"suffix");
    literal->unparseToString();
    return 0;
  }

  std::ostringstream compactOutput;
  UnparseFormat compactFormat(&compactOutput);
  compactFormat.set_compact_output(true);
  if (argc == 2 && std::string(argv[1]) == "--raw-text") {
    compactFormat.emit_raw_text("raw");
    return 0;
  }
  if (argc == 2 && std::string(argv[1]) == "--double-finalize") {
    compactFormat.finalize_compact_output();
    compactFormat.finalize_compact_output();
    return 0;
  }
  if (argc == 2 && std::string(argv[1]) == "--post-finalize") {
    compactFormat.finalize_compact_output();
    compactFormat << "late";
    return 0;
  }
  if (argc == 2 && std::string(argv[1]) == "--directive-newline") {
    compactFormat.emit_compact_directive("#pragma rex\ncontinued");
    return 0;
  }
  if (argc == 2 && std::string(argv[1]) == "--directive-carriage-return") {
    compactFormat.emit_compact_directive("#pragma rex\rcontinued");
    return 0;
  }
  if (argc == 2 && std::string(argv[1]) == "--directive-line-splice") {
    compactFormat.emit_compact_directive("#pragma rex \\");
    return 0;
  }
  if (argc == 2 && std::string(argv[1]) == "--nested-directive") {
    compactFormat.begin_compact_directive();
    compactFormat.begin_compact_directive();
    return 0;
  }
  if (argc == 2 && std::string(argv[1]) == "--unterminated-directive") {
    compactFormat.begin_compact_directive();
    compactFormat << "#pragma rex";
    compactFormat.finalize_compact_output();
    return 0;
  }

  compactFormat << "for " << "(" << "int i = 0; " << "i < 2; " << "++i"
                << ")" << " {";
  compactFormat.insert_newline();
  compactFormat.emit_compact_directive("#pragma rex compact");
  compactFormat.insert_newline();
  compactFormat << "printf(";
  compactFormat.emit_literal("\"alpha  beta\"");
  compactFormat << "); ";
  compactFormat.insert_newline();
  compactFormat << "return 1'000; ";
  compactFormat.insert_newline();
  compactFormat << "}";
  compactFormat.finalize_compact_output();
  if (compactOutput.str() !=
      "for(int i = 0;i < 2;++i) {\n#pragma rex compact\nprintf(\"alpha  "
      "beta\");return 1'000;}") {
    return 1;
  }

  std::ostringstream compactDirectiveTransitionsOutput;
  UnparseFormat compactDirectiveTransitions(&compactDirectiveTransitionsOutput);
  compactDirectiveTransitions.set_compact_output(true);
  compactDirectiveTransitions << "token ";
  compactDirectiveTransitions.begin_compact_directive();
  compactDirectiveTransitions << "#pragma rex assembled";
  compactDirectiveTransitions.insert_newline();
  compactDirectiveTransitions.emit_compact_directive("#pragma rex second");
  compactDirectiveTransitions.emit_compact_directive("!$omp parallel");
  compactDirectiveTransitions << "next";
  if (compactDirectiveTransitionsOutput.str() !=
          "token\n#pragma rex assembled\n#pragma rex second\n!$omp "
          "parallel\nnext" ||
      compactDirectiveTransitions.current_line() != 5 ||
      compactDirectiveTransitions.current_col() != 4) {
    return 15;
  }
  compactDirectiveTransitions.finalize_compact_output();

  std::ostringstream compactLayoutOutput;
  UnparseFormat compactLayout(&compactLayoutOutput);
  compactLayout.set_compact_output(true);
  compactLayout << "case 1:";
  compactLayout.insert_newline();
  compactLayout << "value;";
  compactLayout.finalize_compact_output();
  if (compactLayoutOutput.str() != "case 1:value;") {
    return 16;
  }

  SgStringVal *literal = SageBuilder::buildStringVal("alpha  beta gamma");
  ASSERT_not_null(literal);
  const std::string output = literal->unparseToString();
  if (output.find("alpha  beta gamma") == std::string::npos) {
    return 2;
  }
  if (output.find("alpha beta gamma") != std::string::npos ||
      output.find("alphabetagamma") != std::string::npos) {
    return 3;
  }

  SgIntVal *digitSeparated = SageBuilder::buildIntVal(1000);
  ASSERT_not_null(digitSeparated);
  digitSeparated->set_valueString("1'000");
  if (digitSeparated->unparseToString() != "1'000") {
    return 4;
  }

  SgCharVal *space = SageBuilder::buildCharVal(' ');
  ASSERT_not_null(space);
  if (space->unparseToString() != "' '") {
    return 5;
  }

  std::ostringstream compactWhitespaceOutput;
  UnparseFormat compactWhitespaceFormat(&compactWhitespaceOutput);
  compactWhitespaceFormat.set_compact_output(true);
  compactWhitespaceFormat << "unsigned\nidentifier \t\r";
  compactWhitespaceFormat.finalize_compact_output();
  if (compactWhitespaceOutput.str() != "unsigned identifier") {
    return 6;
  }

  SgIntVal *expanded = SageBuilder::buildIntVal(7);
  ASSERT_not_null(expanded);
  const std::string macroSpelling = "REX_EXACT(alpha  +\n beta)";
  SgMacroExpansionExp *macro = new SgMacroExpansionExp(macroSpelling, expanded);
  expanded->set_parent(macro);
  SageInterface::setSourcePositionForTransformation(macro);
  if (macro->unparseToString() != macroSpelling) {
    return 7;
  }

  const std::string ompSpelling = "alpha  +\n beta";
  SgOmpSourceExpression *ompSource = new SgOmpSourceExpression(ompSpelling);
  SageInterface::setSourcePositionForTransformation(ompSource);
  if (ompSource->unparseToString() != ompSpelling) {
    return 8;
  }

  struct EncodingCase {
    SgStringVal::string_literal_encoding_enum encoding;
    const char *expected;
  };
  const EncodingCase encodingCases[] = {
      {SgStringVal::e_string_encoding_ordinary, "\"payload\""},
      {SgStringVal::e_string_encoding_wide, "L\"payload\""},
      {SgStringVal::e_string_encoding_utf8, "u8\"payload\""},
      {SgStringVal::e_string_encoding_utf16, "u\"payload\""},
      {SgStringVal::e_string_encoding_utf32, "U\"payload\""},
  };
  for (const EncodingCase &testCase : encodingCases) {
    SgStringVal *encoded = SageBuilder::buildStringVal("payload");
    encoded->set_literal_encoding(testCase.encoding);
    if (encoded->unparseToString() != testCase.expected) {
      return 9;
    }
  }

  SgStringVal *raw = SageBuilder::buildStringVal("decoded payload");
  raw->set_literal_encoding(SgStringVal::e_string_encoding_utf8);
  raw->set_isRawString(true);
  raw->set_raw_string_delimiter("rex_tag");
  raw->set_raw_string_payload("first line  \n\nthird line");
  const std::string expectedRaw =
      "u8R\"rex_tag(first line  \n\nthird line)rex_tag\"";
  if (raw->unparseToString() != expectedRaw) {
    return 10;
  }

  SgTreeCopy treeCopy;
  SgStringVal *rawCopy = isSgStringVal(raw->copy(treeCopy));
  if (rawCopy == nullptr ||
      rawCopy->get_literal_encoding() != raw->get_literal_encoding() ||
      rawCopy->get_isRawString() != raw->get_isRawString() ||
      rawCopy->get_raw_string_delimiter() != raw->get_raw_string_delimiter() ||
      rawCopy->get_raw_string_payload() != raw->get_raw_string_payload() ||
      rawCopy->unparseToString() != expectedRaw) {
    return 11;
  }

  const std::string exactMultilineLiteral =
      "R\"rex_exact(first line  \n\nthird line)rex_exact\"";
  std::ostringstream exactMultilineOutput;
  {
    UnparseFormat exactMultilineFormat(&exactMultilineOutput);
    exactMultilineFormat << "const char *value = ";
    exactMultilineFormat.emit_literal(exactMultilineLiteral);
    exactMultilineFormat << ";";
    if (exactMultilineOutput.str() !=
        "const char *value = " + exactMultilineLiteral + ";") {
      return 12;
    }
    if (exactMultilineFormat.current_line() != 3 ||
        exactMultilineFormat.current_col() != 22) {
      return 13;
    }
    exactMultilineFormat.insert_newline();
  }
  if (exactMultilineOutput.str() !=
      "const char *value = " + exactMultilineLiteral + ";\n") {
    return 14;
  }
  return 0;
}
