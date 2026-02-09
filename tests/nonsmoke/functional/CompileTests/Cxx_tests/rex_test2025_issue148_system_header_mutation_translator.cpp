#include "AstConsistencyTests.h"

#include "nodeQuery.h"

#include "sage3basic.h"

#include "sageInterface.h"

#include <algorithm>

#include <string>

#include <vector>

namespace {
const char kHeaderName[] = "test_system_header.hpp";
const char kTargetTypeName[] = "RexIssue148SystemHeaderType";
const char kAuxTypeName[] = "RexIssue148SystemHeaderAuxType";

bool hasHeaderFileInfo(const SgLocatedNode *node) {
  if (node == NULL) {
    return false;
  }
  const Sg_File_Info *fi = node->get_file_info();
  if (fi == NULL) {
    return false;
  }
  const std::string filename = fi->get_filenameString();
  return filename.find(kHeaderName) != std::string::npos;
}

bool scopeContainsDecl(SgScopeStatement *scope, SgDeclarationStatement *decl) {
  if (scope == NULL || decl == NULL) {
    return false;
  }

  if (scope->containsOnlyDeclarations()) {
    const SgDeclarationStatementPtrList &decls = scope->getDeclarationList();
    return std::find(decls.begin(), decls.end(), decl) != decls.end();
  }

  const SgStatementPtrList &stmts = scope->getStatementList();
  return std::find(stmts.begin(), stmts.end(), decl) != stmts.end();
}

SgClassDeclaration *findSystemHeaderType(SgProject *project) {
  std::vector<SgNode *> decls =
      NodeQuery::querySubTree(project, V_SgClassDeclaration);
  for (SgNode *node : decls) {
    SgClassDeclaration *decl = isSgClassDeclaration(node);
    if (decl == NULL) {
      continue;
    }
    if (decl->get_name().getString() != kTargetTypeName) {
      continue;
    }
    if (decl->get_definingDeclaration() != decl) {
      continue;
    }
    if (!hasHeaderFileInfo(decl)) {
      continue;
    }
    return decl;
  }
  return NULL;
}

SgClassDeclaration *findSystemHeaderClass(SgProject *project,
                                          const std::string &name) {
  std::vector<SgNode *> decls =
      NodeQuery::querySubTree(project, V_SgClassDeclaration);
  for (SgNode *node : decls) {
    SgClassDeclaration *decl = isSgClassDeclaration(node);
    if (decl == NULL) {
      continue;
    }
    if (decl->get_definingDeclaration() != decl) {
      continue;
    }
    if (!hasHeaderFileInfo(decl)) {
      continue;
    }
    if (decl->get_name().getString() == name) {
      return decl;
    }
  }
  return NULL;
}
} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != NULL);

  SgClassDeclaration *target = findSystemHeaderType(project);
  ROSE_ASSERT(target != NULL);

  SgScopeStatement *scope = isSgScopeStatement(target->get_parent());
  if (scope == NULL) {
    scope = target->get_scope();
  }
  ROSE_ASSERT(scope != NULL);
  ROSE_ASSERT(scopeContainsDecl(scope, target));

  SgAssignInitializer *initializer = SageBuilder::buildAssignInitializer(
      SageBuilder::buildIntVal(1), SageBuilder::buildIntType());
  SgVariableDeclaration *injected = SageBuilder::buildVariableDeclaration(
      "rex_issue148_inserted", SageBuilder::buildIntType(), initializer, scope);
  ROSE_ASSERT(injected != NULL);

  SageInterface::insertStatement(target, injected, false);
  ROSE_ASSERT(scopeContainsDecl(scope, injected));

  SgClassDeclaration *replacement =
      SageBuilder::buildStructDeclaration("RexIssue148ReplacementType", scope);
  ROSE_ASSERT(replacement != NULL);
  SageInterface::replaceStatement(target, replacement, true);
  ROSE_ASSERT(scopeContainsDecl(scope, replacement));

  SgClassDeclaration *aux_decl = findSystemHeaderClass(project, kAuxTypeName);
  ROSE_ASSERT(aux_decl != NULL);
  SgScopeStatement *var_scope = isSgScopeStatement(aux_decl->get_parent());
  if (var_scope == NULL) {
    var_scope = aux_decl->get_scope();
  }
  ROSE_ASSERT(var_scope != NULL);

  SageInterface::removeStatement(aux_decl);
  AstTests::runAllTests(project);

  ROSE_ASSERT(findSystemHeaderClass(project, kAuxTypeName) == NULL);

  AstTests::runAllTests(project);

  return backend(project);
}
