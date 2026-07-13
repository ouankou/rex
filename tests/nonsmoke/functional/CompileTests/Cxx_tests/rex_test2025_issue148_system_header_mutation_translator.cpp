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
const char kFirstNamespaceFunctionName[] = "first";
const char kSecondNamespaceFunctionName[] = "second";

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

void assertDetachedTransformationProvenance(SgLocatedNode *node) {
  ROSE_ASSERT(node != NULL);
  ROSE_ASSERT(node->get_parent() == NULL);
  for (Sg_File_Info *position :
       {node->get_file_info(), node->get_startOfConstruct(),
        node->get_endOfConstruct()}) {
    ROSE_ASSERT(position != NULL);
    ROSE_ASSERT(position->get_parent() == node);
    ROSE_ASSERT(!position->isShared());
    ROSE_ASSERT(position->isTransformation());
    ROSE_ASSERT(position->get_physical_file_id() == Sg_File_Info::NULL_FILE_ID);
  }
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

SgClassDeclaration *
findPublishedAuxiliaryFirstNondefining(SgProject *project,
                                       const std::string &name) {
  SgClassDeclaration *result = NULL;
  for (SgNode *node : NodeQuery::querySubTree(project, V_SgClassDeclaration)) {
    SgClassDeclaration *declaration = isSgClassDeclaration(node);
    if (declaration == NULL || declaration->get_name().getString() != name ||
        declaration->get_firstNondefiningDeclaration() != declaration ||
        declaration->get_definingDeclaration() != NULL ||
        isSgAuxiliaryDeclarationList(declaration->get_parent()) == NULL) {
      continue;
    }
    ROSE_ASSERT(result == NULL);
    result = declaration;
  }
  return result;
}

SgFunctionDeclaration *findSystemHeaderFunction(SgProject *project,
                                                const std::string &name) {
  std::vector<SgNode *> declarations =
      NodeQuery::querySubTree(project, V_SgFunctionDeclaration);
  for (SgNode *node : declarations) {
    SgFunctionDeclaration *declaration = isSgFunctionDeclaration(node);
    if (declaration == NULL || declaration->get_name().getString() != name ||
        declaration->get_definingDeclaration() != declaration ||
        !hasHeaderFileInfo(declaration)) {
      continue;
    }

    SgNamespaceDefinitionStatement *namespace_scope =
        isSgNamespaceDefinitionStatement(declaration->get_parent());
    if (namespace_scope != NULL &&
        namespace_scope->get_namespaceDeclaration() != NULL &&
        namespace_scope->get_namespaceDeclaration()->get_name().getString() ==
            "RexIssue148SystemHeaderNamespace") {
      return declaration;
    }
  }
  return NULL;
}
} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != NULL);

  bool has_include_ownership = false;
  bool has_system_include_ownership = false;
  bool has_external_ownership = false;
  for (SgFile *file : project->get_fileList()) {
    SgSourceFile *source_file = isSgSourceFile(file);
    if (source_file == NULL) {
      continue;
    }
    for (const std::string &path :
         source_file->get_frontendIncludeOwnershipPathList()) {
      if (path.find(kHeaderName) != std::string::npos) {
        has_include_ownership = true;
      }
    }
    for (const std::string &path :
         source_file->get_frontendSystemIncludeOwnershipPathList()) {
      if (path.find(kHeaderName) != std::string::npos) {
        has_system_include_ownership = true;
      }
    }
    for (const std::string &path :
         source_file->get_frontendExternalOwnershipPathList()) {
      if (path.find(kHeaderName) != std::string::npos) {
        has_external_ownership = true;
      }
    }
  }
  ROSE_ASSERT(has_include_ownership);
  ROSE_ASSERT(has_system_include_ownership);
  ROSE_ASSERT(has_external_ownership);

  SgClassDeclaration *target = findSystemHeaderType(project);
  ROSE_ASSERT(target != NULL);

  SgClassDeclaration *target_first =
      isSgClassDeclaration(target->get_firstNondefiningDeclaration());
  ROSE_ASSERT(target_first != NULL);
  ROSE_ASSERT(target_first != target);
  SgAuxiliaryDeclarationList *target_first_owner =
      isSgAuxiliaryDeclarationList(target_first->get_parent());
  ROSE_ASSERT(target_first_owner != NULL);
  ROSE_ASSERT(target_first_owner->get_parent() == target_first->get_scope());
  ROSE_ASSERT(std::count(target_first_owner->get_declarations().begin(),
                         target_first_owner->get_declarations().end(),
                         target_first) == 1);

  SgScopeStatement *scope = isSgScopeStatement(target->get_parent());
  if (scope == NULL) {
    scope = target->get_scope();
  }
  ROSE_ASSERT(scope != NULL);
  ROSE_ASSERT(scopeContainsDecl(scope, target));
  const int target_physical_file_id =
      target->get_file_info()->get_physical_file_id();
  ROSE_ASSERT(target_physical_file_id >= 0);

  SgAssignInitializer *initializer = SageBuilder::buildAssignInitializer(
      SageBuilder::buildIntVal(1), SageBuilder::buildIntType());
  SgVariableDeclaration *injected = SageBuilder::buildVariableDeclaration(
      "rex_issue148_inserted", SageBuilder::buildIntType(), initializer, scope);
  ROSE_ASSERT(injected != NULL);

  SageInterface::insertStatement(target, injected, false);
  ROSE_ASSERT(scopeContainsDecl(scope, injected));

  SgClassDeclaration *replacement = SageBuilder::buildStructDeclaration(
      SageBuilder::declaration_ownership::transformationDetached(),
      "RexIssue148ReplacementType", scope);
  ROSE_ASSERT(replacement != NULL);
  assertDetachedTransformationProvenance(replacement);
  SgClassDefinition *replacement_definition = replacement->get_definition();
  ROSE_ASSERT(replacement_definition != NULL);
  ROSE_ASSERT(replacement_definition->get_parent() == replacement);
  for (Sg_File_Info *position :
       {replacement_definition->get_file_info(),
        replacement_definition->get_startOfConstruct(),
        replacement_definition->get_endOfConstruct()}) {
    ROSE_ASSERT(position != NULL);
    ROSE_ASSERT(position->get_parent() == replacement_definition);
    ROSE_ASSERT(!position->isShared());
    ROSE_ASSERT(position->isTransformation());
    ROSE_ASSERT(position->get_physical_file_id() == Sg_File_Info::NULL_FILE_ID);
  }
  SgClassDeclaration *replacement_first =
      isSgClassDeclaration(replacement->get_firstNondefiningDeclaration());
  ROSE_ASSERT(replacement_first != NULL);
  ROSE_ASSERT(replacement_first != replacement);
  ROSE_ASSERT(replacement_first->get_parent() ==
              scope->get_auxiliary_declarations());
  ROSE_ASSERT(
      SageInterface::hasExactSemanticAuxiliaryOwnership(replacement_first));
  SageInterface::replaceStatement(target, replacement, true);
  ROSE_ASSERT(scopeContainsDecl(scope, replacement));
  ROSE_ASSERT(replacement->get_parent() == scope);
  ROSE_ASSERT(replacement->get_file_info()->get_physical_file_id() ==
              target_physical_file_id);
  ROSE_ASSERT(replacement_definition->get_file_info()->get_physical_file_id() ==
              target_physical_file_id);
  ROSE_ASSERT(target_first->get_parent() == target_first_owner);
  ROSE_ASSERT(target_first->get_definingDeclaration() == NULL);
  SgClassDeclaration *published_first =
      findPublishedAuxiliaryFirstNondefining(project, kTargetTypeName);
  ROSE_ASSERT(published_first != NULL);
  ROSE_ASSERT(published_first == target_first);
  SgAuxiliaryDeclarationList *published_first_owner =
      isSgAuxiliaryDeclarationList(published_first->get_parent());
  ROSE_ASSERT(published_first_owner != NULL);
  ROSE_ASSERT(published_first_owner->get_parent() ==
              published_first->get_scope());
  ROSE_ASSERT(std::count(published_first_owner->get_declarations().begin(),
                         published_first_owner->get_declarations().end(),
                         published_first) == 1);

  // The first insertion clones the complete external namespace fragment.  A
  // second mutation through an original sibling handle must resolve to the
  // exact sibling in that already-materialized clone, not walk through the
  // detached original namespace declaration.
  SgFunctionDeclaration *first_function =
      findSystemHeaderFunction(project, kFirstNamespaceFunctionName);
  SgFunctionDeclaration *second_function =
      findSystemHeaderFunction(project, kSecondNamespaceFunctionName);
  ROSE_ASSERT(first_function != NULL);
  ROSE_ASSERT(second_function != NULL);
  ROSE_ASSERT(first_function->get_parent() == second_function->get_parent());

  SgScopeStatement *namespace_scope = first_function->get_scope();
  ROSE_ASSERT(namespace_scope != NULL);
  SgVariableDeclaration *first_namespace_insertion =
      SageBuilder::buildVariableDeclaration(
          "rex_issue148_first_namespace_insertion", SageBuilder::buildIntType(),
          NULL, namespace_scope);
  SageInterface::insertStatementAfter(first_function, first_namespace_insertion,
                                      false);
  SgScopeStatement *cloned_namespace_scope =
      isSgScopeStatement(first_namespace_insertion->get_parent());
  ROSE_ASSERT(cloned_namespace_scope != NULL);
  ROSE_ASSERT(
      scopeContainsDecl(cloned_namespace_scope, first_namespace_insertion));

  SgVariableDeclaration *second_namespace_insertion =
      SageBuilder::buildVariableDeclaration(
          "rex_issue148_second_namespace_insertion",
          SageBuilder::buildIntType(), NULL, namespace_scope);
  SageInterface::insertStatementAfter(second_function,
                                      second_namespace_insertion, false);
  ROSE_ASSERT(second_namespace_insertion->get_parent() ==
              cloned_namespace_scope);
  ROSE_ASSERT(
      scopeContainsDecl(cloned_namespace_scope, second_namespace_insertion));

  SgClassDeclaration *aux_decl = findSystemHeaderClass(project, kAuxTypeName);
  ROSE_ASSERT(aux_decl != NULL);
  SgScopeStatement *var_scope = isSgScopeStatement(aux_decl->get_parent());
  if (var_scope == NULL) {
    var_scope = aux_decl->get_scope();
  }
  ROSE_ASSERT(var_scope != NULL);

  SageInterface::removeStatement(aux_decl);
  ROSE_ASSERT(!scopeContainsDecl(var_scope, aux_decl));
  ROSE_ASSERT(aux_decl->get_parent() == NULL);
  AstTests::runAllTests(project);

  ROSE_ASSERT(findSystemHeaderClass(project, kAuxTypeName) == NULL);

  AstTests::runAllTests(project);

  return backend(project);
}
