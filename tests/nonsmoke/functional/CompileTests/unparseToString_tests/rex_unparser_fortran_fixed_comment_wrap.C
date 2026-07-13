#include "FortranLineWrapSupport.h"
#include "rose.h"
#include "unparser.h"

#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace {
struct FixedCommentFixture {
  Unparser_Opt options;
  std::ostringstream output;
  Unparser unparser;
  SgSourceFile file;

  FixedCommentFixture()
      : unparser(&output, "rex_unparser_fortran_fixed_comment_wrap.f",
                 options) {
    file.set_Fortran_only(true);
    file.set_Cxx_only(false);
    file.set_outputLanguage(SgFile::e_Fortran_language);
    file.set_outputFormat(SgFile::e_fixed_form_output_format);
    unparser.currentFile = &file;
  }
};
} // namespace

int main(int argc, char **argv) {
  if (argc > 2) {
    return 10;
  }
  const std::string mode = argc == 2 ? argv[1] : std::string();
  if (mode == "linewrap-state") {
    FixedCommentFixture fixture;
    fixture.unparser.cur.set_linewrap(32);

    SgNullStatement *statement = new SgNullStatement();
    SgUnparse_Info info;
    fixture.unparser.cur.format(statement, info, FORMAT_BEFORE_DIRECTIVE);
    fixture.unparser.emitFortranComment("C leading comment");
    fixture.unparser.cur.format(statement, info, FORMAT_AFTER_DIRECTIVE);

    const std::optional<int> restoredWidth =
        fixture.unparser.cur.get_linewrap();
    if (!restoredWidth.has_value() || *restoredWidth != 32) {
      return 12;
    }

    fixture.unparser.emitFortranText("      CALL ");
    fixture.unparser.emitFortranText("rex_runtime_operation");
    fixture.unparser.emitFortranText("(");
    fixture.unparser.emitFortranText("rex_long_argument");
    fixture.unparser.emitFortranText(")");
    if (fixture.output.str().find("\n     &") == std::string::npos) {
      return 13;
    }
    return 0;
  }
  if (mode == "invalid-marker") {
    FixedCommentFixture fixture;
    fixture.unparser.emitFortranComment("  C invalid fixed-form marker");
    return 0;
  }
  if (mode == "non-column-one") {
    FixedCommentFixture fixture;
    fixture.unparser.cur.emit_raw_text(" ");
    fixture.unparser.emitFortranComment("C misplaced fixed-form comment");
    return 0;
  }
  if (!mode.empty()) {
    return 11;
  }

  using Rose::FortranLineWrapSupport::isFixedFormatCommentLine;
  using Rose::FortranLineWrapSupport::wrapFixedFormatComment;

  if (!isFixedFormatCommentLine("C fixed comment") ||
      !isFixedFormatCommentLine("* fixed comment") ||
      !isFixedFormatCommentLine("  ! fixed comment") ||
      isFixedFormatCommentLine("!$omp parallel") ||
      isFixedFormatCommentLine("      value = 1")) {
    return 1;
  }

  const std::vector<std::string> words =
      wrapFixedFormatComment("C abc def ghi", 0, 10);
  const std::vector<std::string> expectedWords{"C abc def", "C ghi"};
  if (words != expectedWords) {
    return 2;
  }

  const std::vector<std::string> separator =
      wrapFixedFormatComment("!=============", 0, 8);
  const std::vector<std::string> expectedSeparator{"!=======", "!======"};
  if (separator != expectedSeparator) {
    return 3;
  }

  for (const std::string &line : separator) {
    if (line.empty() || line.front() != '!' || line.size() > 8) {
      return 4;
    }
  }

  FixedCommentFixture fixture;
  const std::string exact_comments =
      "C uppercase marker\nc lowercase marker\n* star marker\n! bang marker";
  fixture.unparser.emitFortranComment(exact_comments);
  if (fixture.output.str() != exact_comments) {
    return 5;
  }
  return 0;
}
