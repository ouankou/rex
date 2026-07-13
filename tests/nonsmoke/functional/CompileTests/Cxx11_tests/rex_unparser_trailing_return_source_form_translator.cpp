#include "rose.h"

#include <string>

namespace {
  bool isSourceDecltypeReturn(SgFunctionDeclaration *declaration) {
    if (declaration == nullptr || declaration->get_file_info() == nullptr ||
        declaration->get_file_info()->isCompilerGenerated() ||
        declaration->get_file_info()->isFrontendSpecific()) {
      return false;
    }
    SgType *returnType = declaration->get_orig_return_type();
    return returnType != nullptr &&
           isSgDeclType(returnType->stripType(
               SgType::STRIP_MODIFIER_TYPE | SgType::STRIP_REFERENCE_TYPE |
               SgType::STRIP_RVALUE_REFERENCE_TYPE)) != nullptr;
  }
} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(frontendExitStatus(project) == 0);

  size_t prefixCount = 0;
  size_t trailingCount = 0;
  for (SgNode *node :
       NodeQuery::querySubTree(project, V_SgFunctionDeclaration)) {
    SgFunctionDeclaration *declaration = isSgFunctionDeclaration(node);
    if (!isSourceDecltypeReturn(declaration)) {
      continue;
    }
    const std::string name = declaration->get_name().str();
    if (name == "rex_prefix_decltype_return") {
      ROSE_ASSERT(!declaration->get_using_new_function_return_type_syntax());
      ++prefixCount;
    } else if (name == "rex_trailing_decltype_return") {
      ROSE_ASSERT(declaration->get_using_new_function_return_type_syntax());
      ++trailingCount;
    }
  }

  ROSE_ASSERT(prefixCount == 1);
  ROSE_ASSERT(trailingCount == 1);
  return backend(project);
}
