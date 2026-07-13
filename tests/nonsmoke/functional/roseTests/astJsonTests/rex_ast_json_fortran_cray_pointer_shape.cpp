#include "astJson/sageAstJson.h"
#include "rose.h"

#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

namespace {

std::string takeJsonDir(int &argc, char **argv) {
  constexpr char prefix[] = "--rex-ast-json-dir=";
  std::string result;
  std::vector<char *> filtered{argv[0]};
  for (int index = 1; index < argc; ++index) {
    if (std::strncmp(argv[index], prefix, sizeof(prefix) - 1) == 0) {
      ROSE_ASSERT(result.empty());
      result = argv[index] + sizeof(prefix) - 1;
    } else {
      filtered.push_back(argv[index]);
    }
  }
  argc = static_cast<int>(filtered.size());
  for (int index = 0; index < argc; ++index) {
    argv[index] = filtered[index];
  }
  argv[argc] = nullptr;
  return result;
}

SgSourceFile *firstSourceFile(SgProject *project) {
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(project->numberOfFiles() == 1);
  SgSourceFile *file = isSgSourceFile(&project->get_file(0));
  ROSE_ASSERT(file != nullptr);
  return file;
}

SgInitializedName *exactName(SgNode *root, const std::string &spelling) {
  SgInitializedName *result = nullptr;
  for (SgNode *node : NodeQuery::querySubTree(root, V_SgInitializedName)) {
    SgInitializedName *name = isSgInitializedName(node);
    ROSE_ASSERT(name != nullptr);
    if (name->get_name().getString() != spelling) {
      continue;
    }
    ROSE_ASSERT(result == nullptr);
    result = name;
  }
  ROSE_ASSERT(result != nullptr);
  return result;
}

SgArrayType *ordinaryArray(SgType *type) {
  while (type != nullptr) {
    if (SgModifierType *modifier = isSgModifierType(type)) {
      type = modifier->get_base_type();
    } else if (SgArrayType *array = isSgArrayType(type)) {
      if (!array->get_isCoArray()) {
        return array;
      }
      type = array->get_base_type();
    } else {
      return nullptr;
    }
  }
  return nullptr;
}

void verifyContract(SgNode *root) {
  SgInitializedName *pointer = exactName(root, "shape_address");
  SgInitializedName *pointee = exactName(root, "shaped_pointee");
  SgVariableDeclaration *pointerDeclaration =
      isSgVariableDeclaration(pointer->get_parent());
  SgExprListExp *shape = pointer->get_fortran_cray_pointer_pointee_shape();
  SgArrayType *semanticArray = ordinaryArray(pointee->get_type());

  ROSE_ASSERT(pointerDeclaration != nullptr);
  ROSE_ASSERT(pointerDeclaration->get_variables().size() == 1);
  ROSE_ASSERT(pointerDeclaration->get_variables().front() == pointer);
  ROSE_ASSERT(isSgTypeCrayPointer(pointer->get_fortran_source_type()) !=
              nullptr);
  ROSE_ASSERT(pointer->get_fortran_source_type()->get_fortran_source_syntax());
  ROSE_ASSERT(isSgTypeInt(pointer->get_type()) != nullptr ||
              isSgTypeSignedInt(pointer->get_type()) != nullptr);
  ROSE_ASSERT(pointer->get_cray_pointer_pointee() == pointee);
  ROSE_ASSERT(shape != nullptr);
  ROSE_ASSERT(shape->get_parent() == pointer);
  ROSE_ASSERT(shape->get_expressions().size() == 1);
  ROSE_ASSERT(semanticArray != nullptr);
  ROSE_ASSERT(semanticArray->get_rank() == 1);
  ROSE_ASSERT(pointee->get_fortran_separate_shape_declaration() ==
              pointerDeclaration);
  ROSE_ASSERT(pointee->get_shapeDeferred());
  ROSE_ASSERT(isSgSubscriptExpression(shape->get_expressions().front()) !=
              nullptr);
}

} // namespace

int main(int argc, char **argv) {
  const std::string jsonDir = takeJsonDir(argc, argv);
  ROSE_ASSERT(!jsonDir.empty());
  std::filesystem::remove_all(jsonDir);
  std::filesystem::create_directories(jsonDir);

  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  project->skipfinalCompileStep(true);
  SgSourceFile *original = firstSourceFile(project);
  verifyContract(original);

  const Rose_STL_Container<SgNode *> bodies =
      NodeQuery::querySubTree(original, V_SgBasicBlock);
  ROSE_ASSERT(bodies.size() == 1);
  SgTreeCopy copyHelp;
  SgBasicBlock *bodyCopy = isSgBasicBlock(bodies.front()->copy(copyHelp));
  ROSE_ASSERT(bodyCopy != nullptr);
  verifyContract(bodyCopy);

  std::vector<std::string> checkpointArguments =
      original->get_originalCommandLineArgumentList();
  checkpointArguments.push_back(
      "-rex:ast-json-checkpoint=pre-omp-construction");
  original->get_originalCommandLineArgumentList() = checkpointArguments;
  Rose::AstJson::Options options;
  options.outputDirectory = jsonDir;
  SgSourceFile *restored = Rose::AstJson::roundTripSourceFile(
      original, Rose::AstJson::Checkpoint::PreOmpConstruction, options);
  ROSE_ASSERT(restored != nullptr && restored != original);
  verifyContract(restored);

  return backend(project);
}
