#include "astJson/sageAstJson.h"
#include "rose.h"

#include <cstring>
#include <filesystem>
#include <iostream>
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

SgPointerType *semanticPointer(SgType *type) {
  while (SgModifierType *modifier = isSgModifierType(type)) {
    type = modifier->get_base_type();
  }
  return isSgPointerType(type);
}

SgArrayType *ordinaryArray(SgType *type) {
  while (type != nullptr) {
    if (SgModifierType *modifier = isSgModifierType(type)) {
      type = modifier->get_base_type();
    } else if (SgPointerType *pointer = isSgPointerType(type)) {
      type = pointer->get_base_type();
    } else if (SgArrayType *array = isSgArrayType(type)) {
      return array->get_isCoArray() ? nullptr : array;
    } else {
      return nullptr;
    }
  }
  return nullptr;
}

void verifyContract(SgNode *root, const char *phase) {
  ROSE_ASSERT(phase != nullptr);
  SgInitializedName *scalar = exactName(root, "scalar_pointer");
  SgInitializedName *shaped = exactName(root, "shaped_pointer");
  SgAttributeSpecificationStatement *pointer =
      isSgAttributeSpecificationStatement(
          scalar->get_fortran_separate_pointer_declaration());
  ROSE_ASSERT(pointer != nullptr);
  ROSE_ASSERT(pointer->get_attribute_kind() ==
              SgAttributeSpecificationStatement::e_pointerStatement);
  ROSE_ASSERT(shaped->get_fortran_separate_pointer_declaration() == pointer);
  ROSE_ASSERT(pointer->get_scope() == scalar->get_scope());
  ROSE_ASSERT(pointer->get_scope() == shaped->get_scope());
  ROSE_ASSERT(pointer->get_file_info() != nullptr);
  ROSE_ASSERT(!pointer->get_file_info()->isCompilerGenerated());
  ROSE_ASSERT(!pointer->get_file_info()->isTransformation());

  SgExprListExp *parameters = pointer->get_parameter_list();
  ROSE_ASSERT(parameters != nullptr);
  if (parameters->get_parent() != pointer) {
    std::cerr << "REX_TEST_INVARIANT[fortran-separate-pointer-owner]: phase="
              << phase << " parameter-list=" << parameters
              << " parent=" << parameters->get_parent()
              << " pointer=" << pointer << "\n";
    ROSE_ABORT();
  }
  ROSE_ASSERT(parameters->get_expressions().size() == 2);
  SgVarRefExp *scalarReference =
      isSgVarRefExp(parameters->get_expressions()[0]);
  SgPntrArrRefExp *shapedReference =
      isSgPntrArrRefExp(parameters->get_expressions()[1]);
  ROSE_ASSERT(scalarReference != nullptr);
  ROSE_ASSERT(scalarReference->get_symbol()->get_declaration() == scalar);
  ROSE_ASSERT(shapedReference != nullptr);
  SgVarRefExp *shapedBase = isSgVarRefExp(shapedReference->get_lhs_operand());
  SgExprListExp *shape = isSgExprListExp(shapedReference->get_rhs_operand());
  ROSE_ASSERT(shapedBase != nullptr);
  ROSE_ASSERT(shapedBase->get_symbol()->get_declaration() == shaped);
  ROSE_ASSERT(shape != nullptr);
  ROSE_ASSERT(shape->get_parent() == shapedReference);
  ROSE_ASSERT(shape->get_expressions().size() == 1);
  ROSE_ASSERT(isSgColonShapeExp(shape->get_expressions().front()) != nullptr);

  ROSE_ASSERT(semanticPointer(scalar->get_type()) != nullptr);
  ROSE_ASSERT(semanticPointer(shaped->get_type()) != nullptr);
  ROSE_ASSERT(semanticPointer(scalar->get_fortran_source_type()) == nullptr);
  ROSE_ASSERT(semanticPointer(shaped->get_fortran_source_type()) == nullptr);
  ROSE_ASSERT(ordinaryArray(scalar->get_type()) == nullptr);
  SgArrayType *semanticShape = ordinaryArray(shaped->get_type());
  ROSE_ASSERT(semanticShape != nullptr);
  ROSE_ASSERT(semanticShape->get_rank() == 1);
  ROSE_ASSERT(ordinaryArray(shaped->get_fortran_source_type()) == nullptr);
  ROSE_ASSERT(shaped->get_fortran_separate_shape_declaration() == pointer);
  ROSE_ASSERT(shaped->get_shapeDeferred());
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
  verifyContract(original, "frontend");

  const Rose_STL_Container<SgNode *> bodies =
      NodeQuery::querySubTree(original, V_SgBasicBlock);
  ROSE_ASSERT(bodies.size() == 1);
  SgTreeCopy copyHelp;
  SgBasicBlock *copy = isSgBasicBlock(bodies.front()->copy(copyHelp));
  ROSE_ASSERT(copy != nullptr);
  verifyContract(copy, "copy");

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
  verifyContract(restored, "json-roundtrip");

  return backend(project);
}
