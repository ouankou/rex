#include "nodeQuery.h"
#include "rose.h"

#include <algorithm>
#include <array>
#include <fstream>
#include <string>

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(frontendExitStatus(project) == 0);

  using Attribute = SgStatementAttribute;
  using Kind = Attribute::statement_attribute_kind_enum;
  std::array<size_t, Attribute::e_last_statement_attribute_kind> counts{};

  const Rose_STL_Container<SgNode *> statements =
      NodeQuery::querySubTree(project, V_SgAttributedStatement);
  ROSE_ASSERT(statements.size() == 10);
  size_t gnuFallthroughCount = 0;
  for (SgNode *node : statements) {
    SgAttributedStatement *statement = isSgAttributedStatement(node);
    ROSE_ASSERT(statement != nullptr);
    statement->validate();
    ROSE_ASSERT(statement->get_statement()->get_parent() == statement);
    ROSE_ASSERT(statement->get_attribute_list()->get_parent() == statement);

    using VirtualCFG::CFGNode;
    SgStatement *body = statement->get_statement();
    ROSE_ASSERT(statement->cfgIndexForEnd() == 1);
    ROSE_ASSERT(statement->cfgFindChildIndex(body) == 0);
    ROSE_ASSERT(statement->cfgFindNextChildIndex(body) == 1);
    const auto entry = CFGNode(statement, 0).outEdges();
    ROSE_ASSERT(entry.size() == 1);
    ROSE_ASSERT(entry.front().target() == body->cfgForBeginning());
    const auto incoming = body->cfgForBeginning().inEdges();
    ROSE_ASSERT(std::find(incoming.begin(), incoming.end(), entry.front()) !=
                incoming.end());
    const auto exit = CFGNode(statement, 1).inEdges();
    if (isSgReturnStmt(body) || isSgBreakStmt(body) || isSgContinueStmt(body) ||
        isSgGotoStatement(body)) {
      ROSE_ASSERT(exit.empty());
    } else {
      ROSE_ASSERT(exit.size() == 1);
      ROSE_ASSERT(exit.front().source() == body->cfgForEnd());
      const auto outgoing = body->cfgForEnd().outEdges();
      ROSE_ASSERT(std::find(outgoing.begin(), outgoing.end(), exit.front()) !=
                  outgoing.end());
    }

    for (Attribute *attribute : statement->get_attributes()) {
      ROSE_ASSERT(attribute != nullptr);
      attribute->validate();
      ROSE_ASSERT(attribute->get_parent() == statement->get_attribute_list());
      const Kind kind = attribute->get_kind();
      ROSE_ASSERT(kind >= Attribute::e_statement_attribute_fallthrough &&
                  kind < Attribute::e_last_statement_attribute_kind);
      ++counts[static_cast<size_t>(kind)];

      if (kind == Attribute::e_statement_attribute_fallthrough &&
          attribute->get_spelling() ==
              Attribute::e_statement_attribute_spelling_gnu) {
        ++gnuFallthroughCount;
      }
      if (kind == Attribute::e_statement_attribute_assume) {
        ROSE_ASSERT(attribute->get_expression_argument() != nullptr);
        ROSE_ASSERT(attribute->get_expression_argument()->get_parent() ==
                    attribute);
      }
    }
  }

  ROSE_ASSERT(counts[Attribute::e_statement_attribute_fallthrough] == 2);
  ROSE_ASSERT(gnuFallthroughCount == 1);
  ROSE_ASSERT(counts[Attribute::e_statement_attribute_likely] == 1);
  ROSE_ASSERT(counts[Attribute::e_statement_attribute_unlikely] == 1);
  ROSE_ASSERT(counts[Attribute::e_statement_attribute_assume] == 1);
  ROSE_ASSERT(counts[Attribute::e_statement_attribute_nomerge] == 1);
  ROSE_ASSERT(counts[Attribute::e_statement_attribute_musttail] == 1);
  ROSE_ASSERT(counts[Attribute::e_statement_attribute_always_inline] == 1);
  ROSE_ASSERT(counts[Attribute::e_statement_attribute_loop_hint] == 3);

  SgAttributedStatement *copy = isSgAttributedStatement(
      SageInterface::deepCopy(isSgAttributedStatement(statements.front())));
  ROSE_ASSERT(copy != nullptr);
  copy->validate();
  ROSE_ASSERT(
      copy->get_attribute_list() !=
      isSgAttributedStatement(statements.front())->get_attribute_list());
  ROSE_ASSERT(copy->get_statement() !=
              isSgAttributedStatement(statements.front())->get_statement());

  size_t controlledPragmaCount = 0;
  for (SgNode *node : NodeQuery::querySubTree(project, V_SgIfStmt)) {
    SgIfStmt *conditional = isSgIfStmt(node);
    SgStatement *body = conditional->get_true_body();
    ROSE_ASSERT(body != nullptr && body->get_parent() == conditional);
    if (auto *records = body->getAttachedPreprocessingInfo()) {
      for (PreprocessingInfo *record : *records) {
        if (record->getString() == "#pragma rex_controlled_marker\n") {
          ROSE_ASSERT(record->getTypeOfDirective() ==
                      PreprocessingInfo::CpreprocessorPragmaDeclaration);
          ROSE_ASSERT(record->getRelativePosition() ==
                      PreprocessingInfo::before);
          ++controlledPragmaCount;
        }
      }
    }
  }
  ROSE_ASSERT(controlledPragmaCount == 1);

  AstTests::runAllTests(project);
  const int status = backend(project);
  if (status != 0)
    return status;

  ROSE_ASSERT(project->get_fileList().size() == 1);
  SgFile *file = project->get_fileList().front();
  ROSE_ASSERT(file != nullptr);
  std::ifstream output(file->get_unparse_output_filename());
  ROSE_ASSERT(output.is_open());

  const std::array<const char *, 13> patterns = {
      "[[assume(",
      "[[clang::nomerge]]",
      "[[clang::always_inline]]",
      "[[likely]]",
      "[[unlikely]]",
      "[[fallthrough]]",
      "__attribute__((fallthrough))",
      "[[clang::musttail]]",
      "#pragma clang loop vectorize_width",
      "#pragma clang loop interleave",
      "#pragma unroll 4",
      "for (int index",
      "#pragma rex_raw_literal R\"rex(\"/*)rex\""};
  std::array<size_t, patterns.size()> matchingLines{};
  std::array<size_t, patterns.size()> firstLines{};
  size_t lineNumber = 0;
  for (std::string line; std::getline(output, line);) {
    ++lineNumber;
    for (size_t index = 0; index < patterns.size(); ++index) {
      if (line.find(patterns[index]) != std::string::npos) {
        ++matchingLines[index];
        if (firstLines[index] == 0)
          firstLines[index] = lineNumber;
      }
    }
  }
  ROSE_ASSERT(output.eof() && !output.bad());
  for (size_t count : matchingLines)
    ROSE_ASSERT(count != 0);
  ROSE_ASSERT(matchingLines[12] == 1);
  for (size_t index = 8; index < 11; ++index)
    ROSE_ASSERT(matchingLines[index] == 1);
  ROSE_ASSERT(firstLines[8] < firstLines[9]);
  ROSE_ASSERT(firstLines[9] < firstLines[11]);
  return 0;
}
