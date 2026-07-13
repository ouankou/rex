#include "sage3basic.h"

#include "tokenStreamMapping.h"
#include "unparser.h"

#include <sstream>
#include <string>

namespace {

constexpr const char *kFilename = "switch-label-token-boundary.cpp";

void setSourcePosition(SgLocatedNode *node, int line, int column,
                       bool compiler_generated = false) {
  ROSE_ASSERT(node != nullptr);
  node->set_startOfConstruct(new Sg_File_Info(kFilename, line, column));
  node->set_endOfConstruct(new Sg_File_Info(kFilename, line, column));
  if (compiler_generated) {
    node->setCompilerGenerated();
  }
}

TokenStreamSequenceToNodeMapping *publishMapping(SgLocatedNode *node,
                                                 int core_begin, int core_end,
                                                 int token_count) {
  return TokenStreamSequenceToNodeMapping::createPublished(
      node, TokenStreamHalfOpenInterval(core_begin, core_begin),
      TokenStreamHalfOpenInterval(core_begin, core_end),
      TokenStreamHalfOpenInterval(core_end, core_end),
      TokenStreamHalfOpenInterval(core_end, core_end), token_count);
}

struct Fixture {
  SgSourceFile *source_file = new SgSourceFile();
  SgCaseOptionStmt *case_statement = nullptr;
  SgNullStatement *case_body = nullptr;
  SgDefaultOptionStmt *default_statement = nullptr;
  SgNullStatement *default_body = nullptr;

  explicit Fixture(const std::string &mode) {
    source_file->set_file_info(new Sg_File_Info(kFilename, 1, 1));
    source_file->set_sourceFileNameWithPath(kFilename);
    source_file->set_sourceFileNameWithoutPath(kFilename);
    source_file->set_Cxx_only(true);
    source_file->set_unparse_tokens(true);
    source_file->set_tokenSubsequenceMap(
        new std::map<SgNode *, TokenStreamSequenceToNodeMapping *>());

    int token_index = 0;
    for (const char *lexeme :
         {"case", "  ", "7", " :\n", ";", "default", "  ", ":\n", ";"}) {
      SgToken *token = new SgToken(lexeme, 0);
      const int line = token_index < 5 ? 1 : 3;
      setSourcePosition(token, line, token_index + 1);
      token->set_parent(source_file);
      source_file->get_token_list().push_back(token);
      ++token_index;
    }

    SgIntVal *key = SageBuilder::buildIntVal_nfi(7, "7");
    setSourcePosition(key, 1, 7);
    case_body = new SgNullStatement();
    setSourcePosition(case_body, 2, 1, true);
    case_statement = new SgCaseOptionStmt(key, case_body);
    setSourcePosition(case_statement, 1, 1);
    key->set_parent(case_statement);
    case_body->set_parent(case_statement);

    default_body = new SgNullStatement();
    setSourcePosition(default_body, 4, 1, true);
    default_statement = new SgDefaultOptionStmt(default_body);
    setSourcePosition(default_statement, 3, 1);
    default_body->set_parent(default_statement);

    ROSE_ASSERT(case_body->isCompilerGenerated());
    ROSE_ASSERT(default_body->isCompilerGenerated());
    ROSE_ASSERT(!case_body->isTransformation());
    ROSE_ASSERT(!default_body->isTransformation());

    const int token_count =
        static_cast<int>(source_file->get_token_list().size());
    auto &token_map = source_file->get_tokenSubsequenceMap();
    token_map[case_statement] =
        publishMapping(case_statement, 0, 5, token_count);
    token_map[default_statement] =
        publishMapping(default_statement, 5, 9, token_count);
    if (mode != "case-missing-body-map") {
      const int body_begin = mode == "case-invalid-boundary" ? 0 : 4;
      token_map[case_body] =
          publishMapping(case_body, body_begin, body_begin + 1, token_count);
    }
    if (mode != "default-missing-body-map") {
      token_map[default_body] = publishMapping(default_body, 8, 9, token_count);
    }
    if (mode == "aliased-token-owner") {
      token_map[case_statement] = token_map[default_statement];
    }
  }
};

void unparseLabel(Unparser &unparser, SgSourceFile *source_file,
                  SgStatement *label) {
  SgUnparse_Info info;
  info.set_language(SgFile::e_Cxx_language);
  info.set_current_source_file(source_file);
  info.set_unparsedPartiallyUsingTokenStream();
  info.set_SkipBasicBlock();
  if (SgCaseOptionStmt *case_statement = isSgCaseOptionStmt(label)) {
    unparser.u_exprStmt->unparseCaseStmt(case_statement, info);
  } else {
    SgDefaultOptionStmt *default_statement = isSgDefaultOptionStmt(label);
    ROSE_ASSERT(default_statement != nullptr);
    unparser.u_exprStmt->unparseDefaultStmt(default_statement, info);
  }
}

} // namespace

int main(int argc, char **argv) {
  if (argc > 2) {
    return 2;
  }
  const std::string mode = argc == 2 ? argv[1] : std::string();
  Fixture fixture(mode);

  std::ostringstream output;
  Unparser_Opt options;
  Unparser unparser(&output, kFilename, options);
  unparser.currentFile = fixture.source_file;

  if (mode == "token-missing-end-range") {
    fixture.source_file->get_token_list().front()->set_endOfConstruct(nullptr);
  }
  if (mode == "token-missing-end-range") {
    SgUnparse_Info info;
    info.set_language(SgFile::e_Cxx_language);
    info.set_current_source_file(fixture.source_file);
    bool last_statement_unparsed_using_tokens = false;
    unparser.u_exprStmt->unparseStatementFromTokenStream(
        fixture.source_file, fixture.case_statement, info,
        last_statement_unparsed_using_tokens);
    return 0;
  }

  if (mode == "case-missing-body-map" || mode == "case-invalid-boundary") {
    unparseLabel(unparser, fixture.source_file, fixture.case_statement);
    return 0;
  }
  if (mode == "default-missing-body-map") {
    unparseLabel(unparser, fixture.source_file, fixture.default_statement);
    return 0;
  }
  if (mode == "aliased-token-owner") {
    unparseLabel(unparser, fixture.source_file, fixture.case_statement);
    return 0;
  }
  if (!mode.empty()) {
    return 2;
  }

  unparseLabel(unparser, fixture.source_file, fixture.case_statement);
  unparseLabel(unparser, fixture.source_file, fixture.default_statement);
  const std::string expected = "case  7 :\ndefault  :\n";
  if (output.str() != expected) {
    fprintf(stderr,
            "REX_TEST_FAILURE[switch-label-token-boundary]: expected=%s "
            "actual=%s\n",
            expected.c_str(), output.str().c_str());
    return 1;
  }
  return 0;
}
