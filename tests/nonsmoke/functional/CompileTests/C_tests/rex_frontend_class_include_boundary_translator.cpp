#include "rose.h"

#include <algorithm>
#include <string>

namespace {
SgClassDefinition *findClassDefinition(SgProject *project, const SgName &name) {
  for (SgNode *node : NodeQuery::querySubTree(project, V_SgClassDeclaration)) {
    SgClassDeclaration *declaration = isSgClassDeclaration(node);
    ROSE_ASSERT(declaration != nullptr);
    if (declaration->get_name() == name &&
        declaration->get_definition() != nullptr) {
      return declaration->get_definition();
    }
  }
  return nullptr;
}

bool isClassMemberInclude(const PreprocessingInfo *info) {
  return info != nullptr &&
         info->getTypeOfDirective() ==
             PreprocessingInfo::CpreprocessorIncludeDeclaration &&
         info->getString().find("rex_frontend_class_include_members.def") !=
             std::string::npos;
}
} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(frontendExitStatus(project) == 0);

  SgClassDefinition *definition =
      findClassDefinition(project, "rex_class_include_contract");
  ROSE_ASSERT(definition != nullptr);
  const SgDeclarationStatementPtrList &members = definition->get_members();
  ROSE_ASSERT(members.size() == 4);
  const SgUnsignedCharList &roles = definition->get_source_member_roles();
  ROSE_ASSERT(roles.size() == members.size());
  ROSE_ASSERT(roles[0] == SgClassDefinition::e_source_member_ast);
  ROSE_ASSERT(roles[1] == SgClassDefinition::e_source_member_include_expansion);
  ROSE_ASSERT(roles[2] == SgClassDefinition::e_source_member_include_expansion);
  ROSE_ASSERT(roles[3] == SgClassDefinition::e_source_member_ast);

  size_t includeCount = 0;
  for (SgNode *node : NodeQuery::querySubTree(project, V_SgLocatedNode)) {
    SgLocatedNode *located = isSgLocatedNode(node);
    ROSE_ASSERT(located != nullptr);
    AttachedPreprocessingInfoType *attached =
        located->getAttachedPreprocessingInfo();
    if (attached == nullptr) {
      continue;
    }
    for (PreprocessingInfo *info : *attached) {
      if (!isClassMemberInclude(info)) {
        continue;
      }
      ++includeCount;
      ROSE_ASSERT(located == members[1]);
      ROSE_ASSERT(info->getRelativePosition() == PreprocessingInfo::before);
    }
  }
  ROSE_ASSERT(includeCount == 1);
  ROSE_ASSERT(definition->getAttachedPreprocessingInfo() == nullptr ||
              std::none_of(definition->getAttachedPreprocessingInfo()->begin(),
                           definition->getAttachedPreprocessingInfo()->end(),
                           isClassMemberInclude));

  return backend(project);
}
