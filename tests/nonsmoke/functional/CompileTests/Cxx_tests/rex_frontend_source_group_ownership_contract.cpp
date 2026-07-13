#include "clang-frontend-private.hpp"

namespace {

void publishExactSourcePosition(SgLocatedNode *node, int line) {
  ROSE_ASSERT(node != nullptr);
  const char *filename = "rex_frontend_source_group_ownership_contract.cpp";
  Sg_File_Info *primary = new Sg_File_Info(filename, line, 1);
  Sg_File_Info *start = new Sg_File_Info(filename, line, 1);
  Sg_File_Info *end = new Sg_File_Info(filename, line, 2);
  ROSE_ASSERT(primary != nullptr);
  ROSE_ASSERT(start != nullptr);
  ROSE_ASSERT(end != nullptr);
  node->set_file_info(primary);
  node->set_startOfConstruct(start);
  node->set_endOfConstruct(end);
  primary->set_parent(node);
  start->set_parent(node);
  end->set_parent(node);
}

} // namespace

int main() {
  SgBasicBlock *scope = SageBuilder::buildBasicBlock();
  ROSE_ASSERT(scope != nullptr);
  SgVariableDeclaration *first = SageBuilder::buildVariableDeclaration(
      "rex_detached_first", SageBuilder::buildIntType(), nullptr, scope);
  SgVariableDeclaration *second = SageBuilder::buildVariableDeclaration(
      "rex_detached_second", SageBuilder::buildIntType(), nullptr, scope);
  ROSE_ASSERT(first != nullptr);
  ROSE_ASSERT(second != nullptr);
  ROSE_ASSERT(first->get_parent() == nullptr);
  ROSE_ASSERT(second->get_parent() == nullptr);
  publishExactSourcePosition(first, 1);
  publishExactSourcePosition(second, 1);

  SgDeclarationGroupStatement *group = new SgDeclarationGroupStatement();
  ROSE_ASSERT(group != nullptr);
  group->set_scope(scope);
  group->append_declaration(first);
  group->append_declaration(second);

  // A structurally plausible but detached group has no completed lexical
  // source owner. Only the frontend translator's active typed transaction may
  // recognize this state while constructing a real declaration group.
  ROSE_ASSERT(
      !clangFrontendDeclarationHasExactCompletedSourceSurfaceOwnership(first));
  ROSE_ASSERT(
      !clangFrontendDeclarationHasExactCompletedSourceSurfaceOwnership(second));

  publishExactSourcePosition(group, 1);
  scope->append_statement(group);
  ROSE_ASSERT(group->get_parent() == scope);
  ROSE_ASSERT(
      clangFrontendDeclarationHasExactCompletedSourceSurfaceOwnership(first));
  ROSE_ASSERT(
      clangFrontendDeclarationHasExactCompletedSourceSurfaceOwnership(second));
  return 0;
}
