#include "astJson/sageAstJson.h"
#include "rose.h"

#include <stdexcept>

namespace {

SgSourceFile *firstSourceFile(SgProject *project) {
  if (project == nullptr || project->numberOfFiles() != 1) {
    throw std::runtime_error(
        "function instantiation-pattern test requires one source file");
  }
  SgSourceFile *file = isSgSourceFile(&project->get_file(0));
  if (file == nullptr) {
    throw std::runtime_error(
        "function instantiation-pattern test input is not a source file");
  }
  return file;
}

void verifyFunctionInstantiationPatterns(SgSourceFile *file) {
  size_t hiddenFriendPatterns = 0;
  size_t nestedConstructorPatterns = 0;
  size_t unpublishedLocalConstructorPatterns = 0;
  for (SgNode *node : NodeQuery::querySubTree(file, V_SgFunctionDeclaration)) {
    SgFunctionDeclaration *semantic = isSgFunctionDeclaration(node);
    SgFunctionDeclaration *pattern =
        semantic != nullptr ? semantic->get_templateInstantiationPattern()
                            : nullptr;
    if (semantic != nullptr &&
        semantic->get_template_instantiation_pattern_is_unpublished()) {
      if (pattern != nullptr) {
        throw std::runtime_error(
            "unpublished local-body function pattern has contradictory typed "
            "state");
      }
      ++unpublishedLocalConstructorPatterns;
      continue;
    }
    if (pattern == nullptr) {
      continue;
    }

    Sg_File_Info *source = pattern->get_file_info();
    if (pattern == semantic ||
        isSgAuxiliaryDeclarationList(semantic->get_parent()) == nullptr ||
        pattern->get_templateInstantiationPattern() != nullptr ||
        source == nullptr || source->isTransformation()) {
      throw std::runtime_error(
          "function instantiation-pattern edge lost its exact typed source "
          "identity");
    }

    if (semantic->get_name() == "operator+") {
      if (isSgAuxiliaryDeclarationList(pattern->get_parent()) != nullptr ||
          source->get_physical_file_id() < 0 ||
          source->get_physical_filename().empty() ||
          source->get_raw_line() <= 0 || source->get_raw_col() < 0 ||
          source->isCompilerGenerated() || source->isFrontendSpecific()) {
        throw std::runtime_error(
            "source hidden-friend pattern lost its physical declaration");
      }
      ++hiddenFriendPatterns;
    } else if (semantic->get_name() == "Iterator") {
      if (isSgAuxiliaryDeclarationList(pattern->get_parent()) != nullptr ||
          source->get_physical_file_id() < 0 ||
          source->get_physical_filename().empty() ||
          source->get_raw_line() <= 0 || source->get_raw_col() < 0 ||
          source->isCompilerGenerated() || source->isFrontendSpecific()) {
        throw std::runtime_error(
            "source nested-constructor pattern lost its physical "
            "declaration");
      }
      ++nestedConstructorPatterns;
    }
  }

  if (hiddenFriendPatterns == 0 || nestedConstructorPatterns == 0 ||
      unpublishedLocalConstructorPatterns == 0) {
    throw std::runtime_error(
        "function instantiation-pattern test did not cover all ordinary "
        "instantiation forms");
  }
}

} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  if (project == nullptr) {
    throw std::runtime_error(
        "function instantiation-pattern frontend returned no project");
  }
  project->skipfinalCompileStep(true);

  SgSourceFile *file = firstSourceFile(project);
  verifyFunctionInstantiationPatterns(file);

  file = Rose::AstJson::roundTripSourceFile(
      file, Rose::AstJson::Checkpoint::PreOmpConstruction);
  if (file == nullptr) {
    throw std::runtime_error(
        "function instantiation-pattern AST JSON round trip returned no file");
  }
  verifyFunctionInstantiationPatterns(file);
  AstTests::runAllTests(project);

  return backend(project);
}
