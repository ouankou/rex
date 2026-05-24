// tps (01/14/2010) : Switching from rose.h to sage3.
#include "sage3basic.h"

#include "markCompilerGenerated.h"

#include "rose_config.h"

#include <cstdio>

namespace {

SgDeclarationStatement *templateDeclarationStatement(SgNode *node) {
  SgDeclarationStatement *declaration = isSgDeclarationStatement(node);
  if (declaration == NULL) {
    return NULL;
  }

  switch (declaration->variantT()) {
  case V_SgTemplateClassDeclaration:
  case V_SgTemplateFunctionDeclaration:
  case V_SgTemplateMemberFunctionDeclaration:
  case V_SgTemplateTypedefDeclaration:
  case V_SgTemplateVariableDeclaration:
  case V_SgTemplateDeclaration:
    return declaration;
  default:
    return NULL;
  }
}

bool hasRealSourceFileInfo(SgLocatedNode *node) {
  if (node == NULL || node->get_file_info() == NULL) {
    return false;
  }

  Sg_File_Info *fi = node->get_file_info();
  return fi->get_line() > 0 && fi->isCompilerGenerated() == false &&
         fi->isFrontendSpecific() == false &&
         fi->isSourcePositionUnavailableInFrontend() == false;
}

bool isInSourceBackedLambdaFunction(SgNode *node) {
  for (SgNode *current = node; current != NULL;
       current = current->get_parent()) {
    SgFunctionDeclaration *function_decl = isSgFunctionDeclaration(current);
    if (function_decl == NULL) {
      continue;
    }

    if (function_decl->get_parent() != NULL &&
        SageInterface::isLambdaFunction(function_decl) &&
        hasRealSourceFileInfo(function_decl)) {
      return true;
    }
  }

  return false;
}

} // namespace

void markAsCompilerGenerated(SgNode *node) {
  // This simplifies how the traversal is called!
  MarkAsCompilerGenerated astFixupTraversal;

  // printf ("In markAsCompilerGenerated(): What sort of IR node is being
  // resursively marked as compiler generated? node = %p = %s
  // \n",node,node->class_name().c_str());

  // I think the default should be preorder so that the interfaces would be more
  // uniform
  astFixupTraversal.traverse(node, preorder);
}

// DQ (12/23/2011): Template declarations are now derived from there
// associated non-template declarations (e.g. SgTemplateClassDeclaration is
// derived from SgClassDeclaration). It was previously the case that
// SgTemplateClassDeclaration was derived from SgTemplateDeclaration in the
// first attempt to put the template declarations into the AST.  Previous to
// this (within the legacy frontend 3.3 support within ROSE there was only a
// SgTemplateDeclaration and there was no SgTemplateClassDeclaration.  The
// same story was the case for functions, member functions, and variable
// template declrations.
bool MarkAsCompilerGenerated::templateDeclarationCanBeMarkedAsCompilerGenerated(
    SgDeclarationStatement *templateDeclaration) {
  // Note that this function uses and requires parent pointers to be
  // previously set.

  bool markAsCompilerGenerated = true;

  ROSE_ASSERT(templateDeclaration != NULL);

  // We should not be marking template declarations as compiler generated (I
  // think) if (isSgTemplateDeclaration(node) != NULL) SgTemplateDeclaration*
  // templateDeclaration = isSgTemplateDeclaration(node);
  if (templateDeclaration != NULL) {
    // Get the structural representation from the parent since in the case of a
    // global function marked as friend we want to ignore the scope.

    SgScopeStatement *parentScope =
        isSgScopeStatement(templateDeclaration->get_parent());
    SgGlobal *globalScope = isSgGlobal(parentScope);
    SgNamespaceDefinitionStatement *namespaceScope =
        isSgNamespaceDefinitionStatement(parentScope);
    if (globalScope != NULL || namespaceScope != NULL) {
      // This is an error case
      bool isFriend = templateDeclaration->get_declarationModifier().isFriend();
      if (isFriend == true) {
        // This is OK case too!
      } else {
        // printf ("Warning: detected attempt to mark a template declaration in
        // scope = %s as compiler generated! (will not be
        // marked)\n",parentScope->class_name().c_str());
        // templateDeclaration->get_file_info()->display("template declaration
        // being marked as compiler generated"); ROSE_ABORT();

        markAsCompilerGenerated = false;
      }
    } else {
      // This is an OK case
    }
  }
  // ROSE_ASSERT(isSgTemplateDeclaration(node) == NULL);

  return markAsCompilerGenerated;
}

void MarkAsCompilerGenerated::visit(SgNode *node) {
  ROSE_ASSERT(node != NULL);

  // Only make statements (skip expressions since compiler generated casts are
  // not output!) We should not be borrowing the compiler generated flag to mark
  // IR statement nodes for output by the unparser!
  SgStatement *statement = isSgStatement(node);
  if (statement != NULL) {
    // printf ("Attempting to mark %s as compiler generated
    // \n",node->class_name().c_str());

    bool couldBeCompilerGenerated = true;
    SgDeclarationStatement *templateDeclaration =
        templateDeclarationStatement(node);
    if (templateDeclaration != NULL) {
      // DQ (8/12/2005): There are non-trivial cases where a template
      // declaration can be compiler generated (e.g. when it is a nested class)
      couldBeCompilerGenerated = MarkAsCompilerGenerated::
          templateDeclarationCanBeMarkedAsCompilerGenerated(
              templateDeclaration);
    }

    // DQ (8/17/2005): Mark any compiler generated member function
    // instatiations as non-specialied. legacy frontend marks functions
    // as specialization when they are just defined outside of their
    // parent scope.
    SgTemplateInstantiationMemberFunctionDecl
        *memberFunctionTemplateInstantiation =
            isSgTemplateInstantiationMemberFunctionDecl(node);
    if (memberFunctionTemplateInstantiation != NULL) {
      // printf ("Found a memberFunctionTemplateInstantiation \n");
      if (memberFunctionTemplateInstantiation->isSpecialization() == true) {
        // printf ("In AST Fixup: resetting memberFunctionTemplateInstantiation
        // to be marked as non-specialized \n");
        memberFunctionTemplateInstantiation->set_specialization(
            SgDeclarationStatement::e_no_specialization);
      }
    }

    Sg_File_Info *fileInfo = node->get_file_info();
    if (fileInfo != NULL) {
      // Mark the file info object as being compiler generated instead of part
      // of a transformation. If it were part of a transformation the the
      // unparser would be forced to output the associated code. if
      // (markAsCompilerGenerated == true)
      if (couldBeCompilerGenerated == true &&
          hasRealSourceFileInfo(statement) == false &&
          isInSourceBackedLambdaFunction(statement) == false) {
        // DQ (12/21/2006): Modified to make the settings uniform over all
        // possible source position (there are two for statements).
        // fileInfo->setCompilerGenerated();
        statement->setCompilerGenerated();
      }
    }
  } else {
    // DQ (3/31/2006): SgInitializedName IR nodes need to be marked (and they
    // are not SgStatements)
    SgInitializedName *initializedName = isSgInitializedName(node);
    if (initializedName != NULL) {
      Sg_File_Info *fileInfo = node->get_file_info();
      if (fileInfo != NULL) {
        // Mark the file info object as being compiler generated instead of part
        // of a transformation. If it were part of a transformation the the
        // unparser would be forced to output the associated code.
        if (hasRealSourceFileInfo(initializedName) == false &&
            isInSourceBackedLambdaFunction(initializedName) == false) {
          fileInfo->setCompilerGenerated();
        }
      }
    }
  }
}
