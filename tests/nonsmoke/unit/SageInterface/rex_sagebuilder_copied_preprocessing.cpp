#include "rose.h"
#include <memory>
#include <set>
#include <string>
#include <vector>

struct Records : AstSimpleProcessing {
  std::multiset<std::string> text;
  std::set<PreprocessingInfo *> identities;
  void visit(SgNode *node) override {
    auto *located = isSgLocatedNode(node);
    if (located == nullptr)
      return;
    located->validateAttachedPreprocessingInfoOwnership();
    if (auto *records = located->getAttachedPreprocessingInfo()) {
      for (auto *record : *records) {
        ROSE_ASSERT(record != nullptr);
        ROSE_ASSERT(identities.insert(record).second);
        text.insert(record->getString());
      }
    }
  }
};

void checkStructuralComparison() {
  auto *empty = SageBuilder::buildBasicBlock();
  auto *flat = SageBuilder::buildBasicBlock(SageBuilder::buildBasicBlock(),
                                            SageBuilder::buildBasicBlock());
  auto *inner = SageBuilder::buildBasicBlock(SageBuilder::buildBasicBlock());
  auto *nested = SageBuilder::buildBasicBlock(inner);
  auto *copy = SageInterface::deepCopy(flat);
  ROSE_ASSERT(SageInterface::isStructurallyEquivalentAST(flat, copy));
  ROSE_ASSERT(!SageInterface::isStructurallyEquivalentAST(flat, nested));
  ROSE_ASSERT(!SageInterface::isStructurallyEquivalentAST(nested, flat));
  ROSE_ASSERT(!SageInterface::isStructurallyEquivalentAST(flat, empty));
  ROSE_ASSERT(!SageInterface::isStructurallyEquivalentAST(empty, flat));
  SageInterface::deleteAST(copy);
  SageInterface::deleteAST(nested);
  SageInterface::deleteAST(flat);
  SageInterface::deleteAST(empty);
}

int main(int argc, char **argv) {
  if (argc != 3)
    return 2;
  std::vector<std::string> arguments{argv[0], "-c", argv[1]};
  std::unique_ptr<SgProject> project(frontend(arguments));
  ROSE_ASSERT(project != nullptr && project->get_fileList().size() == 1);
  checkStructuralComparison();
  auto *original = isSgSourceFile(project->get_fileList().front());
  ROSE_ASSERT(original != nullptr);
  ROSE_ASSERT(SageInterface::collectModifiedLocatedNodes(original).empty());
  original->get_globalScope()->markAsModified();
  const auto sequence = original->get_globalScope()->get_lastMutationSequence();
  Records before;
  before.traverse(original, preorder);
  auto *copy = SageBuilder::buildSourceFile(argv[1], argv[2], project.get());
  ROSE_ASSERT(copy != nullptr && copy != original);
  ROSE_ASSERT(copy->get_parent() == project->get_fileList_ptr());
  ROSE_ASSERT(copy->get_sourceFileNameWithPath() ==
              original->get_sourceFileNameWithPath());
  ROSE_ASSERT(copy->get_file_info()->get_filenameString() == argv[2]);
  ROSE_ASSERT(original->get_globalScope()->get_isModified());
  ROSE_ASSERT(original->get_globalScope()->get_lastMutationSequence() ==
              sequence);
  ROSE_ASSERT(SageInterface::collectModifiedLocatedNodes(copy).empty());
  Records after, copied;
  after.traverse(original, preorder);
  copied.traverse(copy, preorder);
  ROSE_ASSERT(before.identities == after.identities &&
              before.text == after.text);
  ROSE_ASSERT(before.text == copied.text);
  for (auto *record : copied.identities)
    ROSE_ASSERT(before.identities.count(record) == 0);
  AstTests::runAllTests(project.get());
  project->unparse();
}
