#include <rose.h>

#include <map>
#include <string>

namespace {

struct ExpectedRecord {
  PreprocessingInfo::DirectiveType type;
  int numberOfLines;
  std::string spelling;
  int occurrences{0};
};

bool IsComment(PreprocessingInfo::DirectiveType type) {
  return type == PreprocessingInfo::FortranStyleComment ||
         type == PreprocessingInfo::F90StyleComment ||
         type == PreprocessingInfo::C_StyleComment ||
         type == PreprocessingInfo::CplusplusStyleComment;
}

class PreprocessingProvenanceChecker : public AstSimpleProcessing {
public:
  void visit(SgNode *node) override {
    SgLocatedNode *located = isSgLocatedNode(node);
    if (located == nullptr) {
      return;
    }
    AttachedPreprocessingInfoType *records =
        located->getAttachedPreprocessingInfo();
    if (records == nullptr) {
      return;
    }
    for (PreprocessingInfo *record : *records) {
      ROSE_ASSERT(record != nullptr);
      if (record->getFilename().find(
              "rex_fortran_source_token_provenance.F90") == std::string::npos) {
        continue;
      }

      const PreprocessingInfo::DirectiveType type =
          record->getTypeOfDirective();
      ROSE_ASSERT(type != PreprocessingInfo::CpreprocessorUnknownDeclaration);
      if (IsComment(type)) {
        continue;
      }

      auto expected = records_.find(record->getLineNumber());
      ROSE_ASSERT(expected != records_.end());
      ROSE_ASSERT(type == expected->second.type);
      ROSE_ASSERT(record->getNumberOfLines() == expected->second.numberOfLines);
      ROSE_ASSERT(record->getString() == expected->second.spelling);
      ++expected->second.occurrences;
    }
  }

  void verify() const {
    for (const auto &entry : records_) {
      ROSE_ASSERT(entry.second.occurrences == 1);
    }
  }

private:
  // Enabled OpenMP-family directives are typed pragma groups, never skipped
  // preprocessing tokens.  Any such downgraded record reaches visit() with a
  // line absent from this exact preprocessing-only table and fails hard.
  std::map<int, ExpectedRecord> records_{
      {1,
       {PreprocessingInfo::CpreprocessorDefineDeclaration, 1,
        "#define REX_SOURCE_TOKEN_VALUE 7\n"}},
      {2,
       {PreprocessingInfo::CpreprocessorDefineDeclaration, 1,
        "#define REX_SOURCE_TOKEN_TEXT \"macro bang ! remains preprocessing "
        "text\"\n"}},
      {3,
       {PreprocessingInfo::CpreprocessorDefineDeclaration, 2,
        "#define REX_SOURCE_TOKEN_SUM(lhs, rhs) \\\n"
        "  ((lhs) + (rhs))\n"}},
      {5, {PreprocessingInfo::CpreprocessorIfDeclaration, 1, "#if 0\n"}},
      {6,
       {PreprocessingInfo::CSkippedToken, 1,
        "this inactive source is intentionally not valid Fortran\n"}},
      {7,
       {PreprocessingInfo::CpreprocessorDefineDeclaration, 1,
        "#define REX_SOURCE_TOKEN_INACTIVE 1\n"}},
      {8, {PreprocessingInfo::CpreprocessorEndifDeclaration, 1, "#endif\n"}},
  };
};

} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  project->skipfinalCompileStep(true);

  PreprocessingProvenanceChecker checker;
  checker.traverse(project, preorder);
  checker.verify();
  AstTests::runAllTests(project);
  return 0;
}
