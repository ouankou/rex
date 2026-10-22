#include "rose.h"

#include <cstring>
#include <memory>
#include <sstream>
#include <stdexcept>

namespace {
class InterruptibleChecker : public TestForParentsMatchingASTStructure {
public:
  using TestForParentsMatchingASTStructure::TestForParentsMatchingASTStructure;
  int interruption = 0;
  const TestForParentsMatchingASTStructure *foreign = nullptr;
  void preOrderVisit(SgNode *node) override {
    if (isSgNullStatement(node) != nullptr) {
      if (interruption == 1)
        throw std::runtime_error("caller interruption");
      if (interruption == 2)
        throw foreign;
    }
    TestForParentsMatchingASTStructure::preOrderVisit(node);
  }
};

void checkReuse() {
  std::ostringstream bodyReport;
  TestForParentsMatchingASTStructure bodyChecker(bodyReport);
  for (bool located : {false, true}) {
    for (bool adoptName : {false, true}) {
      auto *info =
          located ? Sg_File_Info::generateDefaultFileInfoForTransformationNode()
                  : nullptr;
      auto *type = SageBuilder::buildIntType();
      SgVariableDeclaration *declaration;
      if (adoptName) {
        auto *name =
            SageBuilder::buildInitializedName_nfi("value", type, nullptr);
        declaration = located ? new SgVariableDeclaration(info, name)
                              : new SgVariableDeclaration(name);
      } else {
        declaration = located
                          ? new SgVariableDeclaration(info, "value", type)
                          : new SgVariableDeclaration("value", type, nullptr);
      }
      ROSE_ASSERT(declaration->get_startOfConstruct() == info);
      ROSE_ASSERT(info == nullptr || info->get_parent() == declaration);
      ROSE_ASSERT(declaration->get_inline_kind() ==
                  SgVariableDeclaration::inline_kind::none);
      declaration->set_inline_kind(SgVariableDeclaration::inline_kind::written);
      ROSE_ASSERT(bodyChecker.check(declaration, 1));
      SageInterface::deleteAST(declaration,
                               SageInterface::DeleteAstMode::kRequireIsolated);
    }
  }
  auto *selector = SageBuilder::buildCastExp(
      SageBuilder::buildIntVal(0),
      SageBuilder::buildPointerType(SageBuilder::buildVoidType()),
      SgCastExp::e_C_style_cast);
  auto *computed = SageBuilder::buildGotoStatement_nfi(selector);
  ROSE_ASSERT(selector->get_parent() == computed);
  ROSE_ASSERT(bodyChecker.check(computed, 1));
  SageInterface::setOneSourcePositionForTransformation(computed);
  auto *copy = isSgGotoStatement(SageInterface::deepCopy(computed));
  ROSE_ASSERT(copy != nullptr && copy->get_selector_expression() != selector);
  ROSE_ASSERT(copy->get_selector_expression()->get_parent() == copy);
  ROSE_ASSERT(bodyChecker.check(copy, 1));
  SageInterface::deleteAST(computed,
                           SageInterface::DeleteAstMode::kRequireIsolated);
  ROSE_ASSERT(bodyChecker.check(copy, 1));
  SageInterface::deleteAST(copy,
                           SageInterface::DeleteAstMode::kRequireIsolated);
  for (bool prepend : {false, true}) {
    auto *definition = new SgFunctionDefinition(
        Sg_File_Info::generateDefaultFileInfoForTransformationNode(), nullptr,
        nullptr);
    auto *statement = SageBuilder::buildNullStatement();
    if (prepend)
      definition->prepend_statement(statement);
    else
      definition->append_statement(statement);
    ROSE_ASSERT(definition->get_body()->get_parent() == definition);
    ROSE_ASSERT(statement->get_parent() == definition->get_body());
    ROSE_ASSERT(bodyChecker.check(definition, 1));
    SageInterface::deleteAST(definition,
                             SageInterface::DeleteAstMode::kRequireIsolated);
  }
  auto invalid = std::make_unique<SgBasicBlock>();
  auto invalidChild = std::make_unique<SgNullStatement>();
  invalid->append_statement(invalidChild.get());
  invalidChild->set_parent(nullptr);
  auto valid = std::make_unique<SgBasicBlock>();
  auto validChild = std::make_unique<SgNullStatement>();
  valid->append_statement(validChild.get());
  std::ostringstream report;
  InterruptibleChecker checker(report);
  TestForParentsMatchingASTStructure foreign(report);
  checker.foreign = &foreign;
  for (int repeat = 0; repeat != 100; ++repeat) {
    ROSE_ASSERT(!checker.check(invalid.get(), 1));
    ROSE_ASSERT(checker.nproblems == 1 && checker.stack.empty());
    ROSE_ASSERT(checker.check(valid.get(), 1));
    ROSE_ASSERT(checker.nproblems == 0 && checker.stack.empty());
    for (int interruption : {1, 2}) {
      checker.interruption = interruption;
      bool caught = false;
      try {
        (void)checker.check(valid.get(), 1);
      } catch (const std::runtime_error &) {
        ROSE_ASSERT(interruption == 1);
        caught = true;
      } catch (const TestForParentsMatchingASTStructure *failure) {
        ROSE_ASSERT(interruption == 2 && failure == &foreign);
        caught = true;
      }
      ROSE_ASSERT(caught && checker.stack.empty());
      checker.interruption = 0;
      ROSE_ASSERT(checker.check(valid.get(), 1));
      ROSE_ASSERT(checker.nproblems == 0 && checker.stack.empty());
    }
  }
}
} // namespace

int main(int argc, char **argv) {
  if (argc != 2)
    return 2;
  if (std::strcmp(argv[1], "reuse") == 0) {
    checkReuse();
    return 0;
  }
  const bool duplicate = std::strcmp(argv[1], "duplicate") == 0;
  const bool wrong = std::strcmp(argv[1], "wrong") == 0;
  const bool missing = std::strcmp(argv[1], "missing") == 0;
  if (!wrong && !missing && !duplicate && std::strcmp(argv[1], "valid") != 0)
    return 2;
  auto *file =
      SageBuilder::buildGeneratedSourceFile("rex_ast_parent_ownership.cpp");
  std::unique_ptr<SgProject> project(file->get_project());
  auto *scope = file->get_globalScope();
  auto *declaration = SageBuilder::buildVariableDeclaration(
      "value", SageBuilder::buildIntType(), nullptr, scope);
  SageInterface::appendStatement(declaration, scope);
  if (wrong)
    declaration->set_parent(project.get());
  else if (missing)
    declaration->set_parent(nullptr);
  if (duplicate) {
    TestAstForUniqueNodesInAST checker;
    checker.visit(project.get());
    checker.visit(project.get());
  } else {
    TestAstForUniqueNodesInAST::test(project.get());
    TestForParentsMatchingASTStructure::test(project.get());
  }
  return 0;
}
