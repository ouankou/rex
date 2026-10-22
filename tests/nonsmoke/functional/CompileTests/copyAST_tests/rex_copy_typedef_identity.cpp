#include "rose.h"

namespace {
SgTypedefType *requireExactAliases(SgProject *project) {
  SgTypedefType *alias = nullptr;
  for (SgNode *node :
       NodeQuery::querySubTree(project, V_SgTypedefDeclaration)) {
    auto *declaration = isSgTypedefDeclaration(node);
    if (declaration->get_name() == "rex_extent") {
      ROSE_ASSERT(alias == nullptr);
      alias = declaration->get_type();
      ROSE_ASSERT(alias != nullptr && alias->get_declaration() == declaration);
    }
  }
  ROSE_ASSERT(alias != nullptr);

  size_t arguments = 0;
  for (SgNode *node : NodeQuery::querySubTree(project, V_SgTemplateArgument)) {
    auto *argument = isSgTemplateArgument(node);
    if (argument->get_argumentType() != SgTemplateArgument::nontype_argument)
      continue;
    ROSE_ASSERT(argument->get_expression() != nullptr);
    SgType *payload = argument->get_expression()->get_type();
    // Check explicitly in optimized builds too: the general AstTests entry
    // point omits its expression traversal when the library defines NDEBUG.
    ROSE_ASSERT(SageInterface::cxxNonTypeTemplateArgumentTypeConversionIsExact(
        payload, argument->get_type()));
    ++arguments;
  }
  ROSE_ASSERT(arguments >= 2);
  return alias;
}

class RecursiveCopy : public SgCopyHelp {
public:
  SgNode *copyAst(const SgNode *node) override {
    SgNode *parent = node->get_parent();
    const auto *function = isSgFunctionDeclaration(parent);
    const auto *definition = isSgFunctionDefinition(parent);
    const auto *name = isSgInitializedName(parent);
    if ((function != nullptr && function->get_definition() == node) ||
        (definition != nullptr && definition->get_body() == node) ||
        (name != nullptr && name->get_initptr() == node)) {
      const auto owner = get_copiedNodeMap().find(parent);
      ROSE_ASSERT(owner != get_copiedNodeMap().end());
      ROSE_ASSERT(owner->second != parent);
    }
    return node->copy(*this);
  }
};

template <class Copy> void requireCopy(SgProject *project) {
  SgTypedefType *original = requireExactAliases(project);
  Copy help;
  auto *copy = isSgProject(project->copy(help));
  ROSE_ASSERT(copy != nullptr && copy != project);
  SgTypedefType *copied = requireExactAliases(copy);
  ROSE_ASSERT(copied != original);
  ROSE_ASSERT(help.get_copiedNodeMap().at(original) == copied);
  ROSE_ASSERT(requireExactAliases(project) == original);
  size_t signatures = 0;
  for (const auto &mapping : help.get_copiedNodeMap()) {
    const auto *sourceFunction = isSgFunctionDeclaration(mapping.first);
    auto *copyFunction = isSgFunctionDeclaration(mapping.second);
    if (sourceFunction == nullptr ||
        sourceFunction->get_type_syntax() == nullptr)
      continue;
    const SgFunctionType *sourceType = sourceFunction->get_type_syntax();
    const SgFunctionType *copyType = copyFunction->get_type_syntax();
    ROSE_ASSERT(copyType != nullptr && copyType != sourceType);
    ROSE_ASSERT(copyType->get_parent() == copyFunction);
    auto requireType = [&](SgType *source, SgType *target) {
      const auto mapped = help.get_copiedNodeMap().find(source);
      ROSE_ASSERT(
          target ==
          (mapped == help.get_copiedNodeMap().end() ? source : mapped->second));
    };
    requireType(sourceType->get_return_type(), copyType->get_return_type());
    ROSE_ASSERT(sourceType->get_arguments().size() ==
                copyType->get_arguments().size());
    for (size_t index = 0; index < sourceType->get_arguments().size(); ++index)
      requireType(sourceType->get_arguments()[index],
                  copyType->get_arguments()[index]);
    if (const auto *sourceMember = isSgMemberFunctionType(sourceType)) {
      const auto *copyMember = isSgMemberFunctionType(copyType);
      ROSE_ASSERT(copyMember != nullptr);
      requireType(sourceMember->get_class_type(), copyMember->get_class_type());
    }
    ++signatures;
  }
  ROSE_ASSERT(signatures >= 2);
  SageInterface::deleteAST(copy);
  ROSE_ASSERT(requireExactAliases(project) == original);
}
} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  requireCopy<SgTreeCopy>(project);
  requireCopy<RecursiveCopy>(project);
  SageInterface::deleteAST(project);
}
