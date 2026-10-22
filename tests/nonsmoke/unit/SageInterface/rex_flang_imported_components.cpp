#include "astJson/sageAstJson.h"
#include "rose.h"

#include <cstdlib>
#include <set>
#include <utility>

namespace {
void checkComponents(SgNode *root) {
  const auto references = NodeQuery::querySubTree(root, V_SgDotExp);
  ROSE_ASSERT(references.size() == 2);
  for (SgNode *node : references) {
    auto *reference = isSgDotExp(node);
    auto *member = isSgVarRefExp(reference->get_rhs_operand());
    ROSE_ASSERT(member != nullptr && member->get_symbol() != nullptr);
    auto *type =
        isSgClassType(reference->get_lhs_operand()->get_type()->stripType(
            SgType::STRIP_MODIFIER_TYPE | SgType::STRIP_ARRAY_TYPE |
            SgType::STRIP_POINTER_TYPE));
    ROSE_ASSERT(type != nullptr);
    auto *declaration = isSgClassDeclaration(type->get_declaration());
    ROSE_ASSERT(declaration != nullptr);
    auto *defining =
        isSgClassDeclaration(declaration->get_definingDeclaration());
    ROSE_ASSERT(defining != nullptr && defining->get_definition() != nullptr);
    SgClassDefinition *owner = defining->get_definition();
    SgInitializedName *component = member->get_symbol()->get_declaration();
    ROSE_ASSERT(component != nullptr && component->get_scope() == owner);
    ROSE_ASSERT(owner->find_symbol_from_declaration(component) ==
                member->get_symbol());
    ROSE_ASSERT(reference->get_type() == component->get_type());
  }
  const auto accesses = NodeQuery::querySubTree(root, V_SgPntrArrRefExp);
  ROSE_ASSERT(accesses.size() == 16);
  std::multiset<std::pair<int, int>> characterLengths;
  std::multiset<std::pair<int, int>> characterBaseLengths;
  for (SgNode *node : accesses) {
    auto *access = isSgPntrArrRefExp(node);
    if (auto *string = isSgTypeString(access->get_type())) {
      ROSE_ASSERT(!string->get_fortran_source_syntax());
      auto *length = isSgIntVal(string->get_lengthExpression());
      auto *kind = isSgIntVal(string->get_type_kind());
      ROSE_ASSERT(length != nullptr && kind != nullptr);
      ROSE_ASSERT(kind->get_value() == 1 || kind->get_value() == 4);
      const long long characterBytes = kind->get_value() == 4 ? 4 : 1;
      ROSE_ASSERT(isSgSubscriptExpression(access->get_rhs_operand()) !=
                  nullptr);
      ROSE_ASSERT(access->get_result_storage_size_bytes() ==
                  length->get_value() * characterBytes);
      ROSE_ASSERT(access->get_subscript_stride_bits() ==
                  std::vector<std::optional<long long>>{characterBytes * 8});
      characterLengths.emplace(kind->get_value(), length->get_value());
      auto *base = isSgStringVal(access->get_lhs_operand());
      ROSE_ASSERT(base != nullptr);
      auto *baseType = isSgTypeString(base->get_type());
      ROSE_ASSERT(baseType != nullptr &&
                  !baseType->get_fortran_source_syntax());
      auto *baseLength = isSgIntVal(baseType->get_lengthExpression());
      auto *baseKind = isSgIntVal(baseType->get_type_kind());
      ROSE_ASSERT(baseLength != nullptr && baseKind != nullptr &&
                  baseKind->get_value() == kind->get_value());
      characterBaseLengths.emplace(baseKind->get_value(),
                                   baseLength->get_value());
      continue;
    }
    auto *indices = isSgExprListExp(access->get_rhs_operand());
    ROSE_ASSERT(indices != nullptr && (indices->get_expressions().size() == 1 ||
                                       indices->get_expressions().size() == 2));
    auto *array = isSgArrayType(access->get_type()->stripType(
        SgType::STRIP_TYPEDEF_TYPE | SgType::STRIP_MODIFIER_TYPE));
    if (indices->get_expressions().size() == 2) {
      ROSE_ASSERT(array == nullptr);
      ROSE_ASSERT(access->get_result_storage_size_bytes() == 1);
      ROSE_ASSERT((access->get_subscript_stride_bits() ==
                   std::vector<std::optional<long long>>{8, 24}));
    } else if (isSgSubscriptExpression(indices->get_expressions().front())) {
      ROSE_ASSERT(array != nullptr && array->get_rank() == 1);
      ROSE_ASSERT(access->get_result_storage_size_bytes() == 3);
    } else {
      ROSE_ASSERT(array == nullptr);
      ROSE_ASSERT(access->get_result_storage_size_bytes() == 1);
    }
    if (indices->get_expressions().size() == 1) {
      const auto *pointer = isSgPointerType(
          access->get_lhs_operand()->get_type()->stripTypedefsAndModifiers());
      ROSE_ASSERT(access->get_subscript_stride_bits() ==
                  std::vector<std::optional<long long>>{
                      pointer ? std::nullopt : std::optional<long long>{8}});
    }
  }
  const std::multiset<std::pair<int, int>> expectedCharacterLengths{
      {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2},
      {1, 3}, {1, 4}, {1, 5}, {4, 1}, {4, 3}};
  ROSE_ASSERT(characterLengths == expectedCharacterLengths);
  const std::multiset<std::pair<int, int>> expectedCharacterBaseLengths{
      {1, 6}, {1, 6}, {1, 6}, {1, 2}, {1, 3},
      {1, 4}, {1, 6}, {1, 6}, {4, 4}, {4, 2}};
  ROSE_ASSERT(characterBaseLengths == expectedCharacterBaseLengths);
}
} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr && project->numberOfFiles() == 1);
  checkComponents(project);
  SgTreeCopy copy;
  auto *copied = isSgProject(project->copy(copy));
  ROSE_ASSERT(copied != nullptr && copied != project);
  checkComponents(copied);
  auto *copied_files = copied->get_fileList_ptr();
  SageInterface::deleteAST(copied,
                           SageInterface::DeleteAstMode::kRequireIsolated);
  ROSE_ASSERT(!SgNode::isLiveNode(copied_files));

  auto *file = isSgSourceFile(&project->get_file(0));
  ROSE_ASSERT(file != nullptr);
  Rose::AstJson::Options options;
  options.outputDirectory = "rex_flang_imported_components_json";
  ROSE_ASSERT(setenv("REX_AST_JSON_CHECKPOINT", "pre-omp-construction", 1) ==
              0);
  SgFileList *originalModules = file->get_frontendExternalFileList();
  ROSE_ASSERT(originalModules != nullptr);
  SgSourceFile *restored = Rose::AstJson::roundTripSourceFile(
      file, Rose::AstJson::Checkpoint::PreOmpConstruction, options);
  ROSE_ASSERT(restored != nullptr && restored != file);
  ROSE_ASSERT(restored->get_frontendExternalFileList() != nullptr &&
              restored->get_frontendExternalFileList() != originalModules);
  checkComponents(restored);
  const int status = backend(project);
  auto *project_files = project->get_fileList_ptr();
  SageInterface::deleteAST(project,
                           SageInterface::DeleteAstMode::kRequireIsolated);
  ROSE_ASSERT(!SgNode::isLiveNode(project_files));
  return status;
}
