#include "RoseAst.h"
#include "rose.h"

#include <cstdio>
#include <cstring>

namespace {

SgFunctionDeclaration *findFunction(SgNode *root, const char *name,
                                    bool requireDefinition) {
  for (SgNode *node : RoseAst(root)) {
    SgFunctionDeclaration *declaration = isSgFunctionDeclaration(node);
    if (declaration != nullptr && declaration->get_name() == name &&
        (!requireDefinition || declaration->get_definition() != nullptr)) {
      return declaration;
    }
  }
  return nullptr;
}

class MissingInternalScopeMapCopy final : public SgTreeCopy {
public:
  explicit MissingInternalScopeMapCopy(const SgScopeStatement *hiddenScope)
      : hiddenScope_(hiddenScope) {
    ROSE_ASSERT(hiddenScope_ != nullptr);
  }

  void prepareRootCopyForFixup() override {
    SgTreeCopy::prepareRootCopyForFixup();
    if (eraseCopiedNodePair(hiddenScope_) != 1) {
      std::fprintf(stderr,
                   "REX_TEST_ERROR: internal scope was not copied before "
                   "fixup\n");
      ROSE_ABORT();
    }
  }

private:
  const SgScopeStatement *hiddenScope_;
};

void checkCopyIdentityIndex(const SgCopyHelp &help) {
  SgCopyHelp::originalNodeMapType expected;
  for (const auto &mapping : help.get_copiedNodeMap()) {
    expected[mapping.second].insert(mapping.first);
  }
  for (const auto &mapping : expected) {
    ROSE_ASSERT(help.get_originalNodesForCopy(mapping.first) == mapping.second);
  }
}

void checkCopyIdentityUpdates(const SgNode *original, SgNode *other,
                              SgNode *copy) {
  SgTreeCopy help;
  help.insertCopiedNodePair(original, copy);
  // Insertion preserves a previously published identity.
  help.insertCopiedNodePair(original, other);
  ROSE_ASSERT(help.get_copiedNodeMap().at(original) == copy);
  ROSE_ASSERT(help.get_originalNodesForCopy(other).empty());

  // Canonicalization may merge targets, then rebind a source without changing
  // the number of forward entries. A size-based reverse cache would go stale.
  help.assignCopiedNodePair(other, copy);
  ROSE_ASSERT(help.get_originalNodesForCopy(copy).size() == 2);
  help.assignCopiedNodePair(original, other);
  ROSE_ASSERT(help.get_originalNodesForCopy(copy).size() == 1);
  checkCopyIdentityIndex(help);
  const size_t erased = help.eraseCopiedNodePair(other);
  const size_t erasedAgain = help.eraseCopiedNodePair(other);
  ROSE_ASSERT(erased == 1);
  ROSE_ASSERT(erasedAgain == 0);
  ROSE_ASSERT(help.get_originalNodesForCopy(copy).empty());

  // Shared identities and transaction reset must also remove obsolete values.
  help.assignCopiedNodePair(original, const_cast<SgNode *>(original));
  ROSE_ASSERT(help.get_originalNodesForCopy(other).empty());
  checkCopyIdentityIndex(help);
  help.clearState();
  ROSE_ASSERT(help.get_copiedNodeMap().empty());
  ROSE_ASSERT(help.get_originalNodesForCopy(original).empty());
  help.insertCopiedNodePair(original, copy);
  checkCopyIdentityIndex(help);
}

} // namespace

int main(int argc, char **argv) {
  const bool rejectInternalMissingMap =
      argc == 3 &&
      std::strcmp(argv[1], "--reject-internal-missing-scope-map") == 0;
  if (argc != (rejectInternalMissingMap ? 3 : 2)) {
    return 2;
  }

  char *frontendArguments[] = {argv[0], argv[argc - 1]};
  SgProject *project = frontend(2, frontendArguments);
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(frontendExitStatus(project) == 0);

  SgFunctionDeclaration *target =
      findFunction(project, "rex_copy_scope_target", false);
  SgFunctionDeclaration *owner =
      findFunction(project, "rex_copy_scope_owner", true);
  ROSE_ASSERT(target != nullptr);
  ROSE_ASSERT(owner != nullptr);
  ROSE_ASSERT(owner->get_definition() != nullptr);

  if (!rejectInternalMissingMap) {
    SgScopeStatement *externalScope = target->get_scope();
    ROSE_ASSERT(externalScope != nullptr);
    SgTreeCopy copyHelp;
    SgFunctionDeclaration *copy =
        isSgFunctionDeclaration(target->copy(copyHelp));
    ROSE_ASSERT(copy != nullptr);
    ROSE_ASSERT(copy != target);
    ROSE_ASSERT(copy->get_scope() == externalScope);
    checkCopyIdentityIndex(copyHelp);
    checkCopyIdentityUpdates(target, owner, copy);
    return 0;
  }

  // Deliberately inject an internal semantic edge, then remove that internal
  // target's copy-map entry after the structural copy is complete.  The copy
  // boundary must reject the missing map: retaining the source scope would
  // cross-link the copied declaration into its source transaction.
  SgFunctionDefinition *internalScope = owner->get_definition();
  SgGlobal *global = SageInterface::getGlobalScope(target);
  ROSE_ASSERT(global != nullptr);
  target->set_scope(internalScope);
  MissingInternalScopeMapCopy copyHelp(internalScope);
  (void)global->copy(copyHelp);
  return 1;
}
