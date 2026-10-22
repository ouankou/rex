#include "astJson/sageAstJson.h"
#include "rose.h"

#include <memory>

namespace {
void verify(SgProject *project) {
  ROSE_ASSERT(SgNode::isLiveNode(project->get_globalScopeAcrossFiles()));
  ROSE_ASSERT(project->get_globalScopeAcrossFiles()->get_parent() == project);
  auto *file = isSgSourceFile(&project->get_file(0));
  ROSE_ASSERT(file != nullptr);
  unsigned int found = 0;
  SgClassDeclaration *receiverClass = nullptr;
  for (auto *declaration : file->get_globalScope()->get_declarations()) {
    if (auto *classDeclaration = isSgClassDeclaration(declaration)) {
      ROSE_ASSERT(receiverClass == nullptr);
      receiverClass = classDeclaration;
      ROSE_ASSERT(receiverClass->get_type()->get_declaration() ==
                  receiverClass->get_firstNondefiningDeclaration());
    }
    auto *alias = isSgTypedefDeclaration(declaration);
    if (alias == nullptr)
      continue;
    const auto name = alias->get_name();
    auto *base = alias->get_base_type()->stripTypedefsAndModifiers();
    if (name == "rex_const_signature" || name == "rex_ref_signature") {
      auto *function = isSgMemberFunctionType(base);
      ROSE_ASSERT(function != nullptr && function->get_class_type() == nullptr);
      const unsigned int expected =
          name == "rex_const_signature"
              ? SgMemberFunctionType::e_const
              : SgMemberFunctionType::e_ref_qualifier_lvalue;
      ROSE_ASSERT(function->get_mfunc_specifier() == expected);
      ROSE_ASSERT(function->get_arguments().size() == 1);
      ++found;
    } else if (name == "rex_bound_pointer") {
      auto *pointer = isSgPointerMemberType(base);
      ROSE_ASSERT(pointer != nullptr);
      ROSE_ASSERT(pointer->get_class_type() != nullptr);
      ROSE_ASSERT(!SgPointerMemberType::isCanonicalSemanticType(pointer));
      auto *pointeeAlias = isSgTypedefType(pointer->get_base_type());
      ROSE_ASSERT(pointeeAlias != nullptr);
      auto *pointeeDeclaration =
          isSgTypedefDeclaration(pointeeAlias->get_declaration());
      ROSE_ASSERT(pointeeDeclaration != nullptr &&
                  pointeeDeclaration->get_name() == "rex_const_signature");
      ++found;
    }
  }
  ROSE_ASSERT(found == 3);
  ROSE_ASSERT(receiverClass != nullptr);
  unsigned int receivers = 0;
  for (auto *node : NodeQuery::querySubTree(file, V_SgInitializedName)) {
    auto *name = isSgInitializedName(node);
    if (name->get_name() == "receiver") {
      ROSE_ASSERT(name->get_type()->stripTypedefsAndModifiers() ==
                  receiverClass->get_type());
      ++receivers;
    }
  }
  ROSE_ASSERT(receivers == 1);
  auto calls = NodeQuery::querySubTree(file, V_SgDotStarOp);
  ROSE_ASSERT(calls.size() == 1);
  auto *callable =
      isSgMemberFunctionType(isSgDotStarOp(calls.front())->get_type());
  ROSE_ASSERT(callable != nullptr);
  ROSE_ASSERT(callable->get_mfunc_specifier() == SgMemberFunctionType::e_const);
  ROSE_ASSERT(callable->get_arguments().size() == 1);
  AstTests::runAllTests(project);
}
} // namespace

int main(int argc, char **argv) {
  std::unique_ptr<SgProject> project(frontend(argc, argv));
  verify(project.get());
  std::unique_ptr<SgProject> copy(SageInterface::deepCopy(project.get()));
  ROSE_ASSERT(copy->get_globalScopeAcrossFiles() !=
              project->get_globalScopeAcrossFiles());
  ROSE_ASSERT(copy->get_directoryList() != project->get_directoryList());
  verify(copy.get());
  verify(project.get());
  SgProject *copied_project = copy.release();
  SgFileList *copied_files = copied_project->get_fileList_ptr();
  SageInterface::deleteAST(copied_project,
                           SageInterface::DeleteAstMode::kRequireIsolated);
  ROSE_ASSERT(!SgNode::isLiveNode(copied_project));
  ROSE_ASSERT(!SgNode::isLiveNode(copied_files));
  verify(project.get());
  auto *file = isSgSourceFile(&project->get_file(0));
  file->get_originalCommandLineArgumentList().push_back(
      "-rex:ast-json-checkpoint=pre-omp-construction");
  auto *restored = Rose::AstJson::roundTripSourceFile(
      file, Rose::AstJson::Checkpoint::PreOmpConstruction);
  ROSE_ASSERT(restored != nullptr && restored != file);
  verify(project.get());
  return backend(project.get());
}
