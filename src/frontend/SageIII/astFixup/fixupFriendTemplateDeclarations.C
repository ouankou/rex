// tps (01/14/2010) : Switching from rose.h to sage3.
#include "fixupFriendTemplateDeclarations.h"

#include "sage3basic.h"
// DQ (12/31/2005): This is OK if not declared in a header file
using namespace std;

void fixupFriendTemplateDeclarations() {
  // DQ (3/10/2007): Introduce tracking of performance of ROSE.
  TimingPerformance timer("Fixup friend template function declarations:");

  FixupFriendTemplateDeclarations t1;

  // Traverse just a specific memory pool (optimal).
  // t.traverseMemoryPool();
  SgTemplateDeclaration::traverseMemoryPoolNodes(t1);

  fixupFriendDeclarations();
}

void fixupFriendDeclarations() {
  // DQ (3/10/2007): Introduce tracking of performance of ROSE.
  TimingPerformance timer("Fixup friend declarations:");

  FixupFriendDeclarations t2;

  // The visitor only rewrites template-instantiation function declarations.
  SgTemplateInstantiationFunctionDecl::traverseMemoryPoolNodes(t2);
}

void FixupFriendTemplateDeclarations::visit(SgNode *node) {
  // This function marks template declarations that are declared to be friends
  // with the ROSE internal marking as friends.  Specifically it sets the firend
  // flag in the declaration modifier (the organization of modifiers follows the
  // C++ grammar as laid out in the back of Bjarne's book). legacy frontend does
  // not always record friend functions when they are templates (perhaps because
  // we use the template string implementation at present), however the friend
  // keyward is in the string if it is declared as such.  We determine if a
  // declaration is a friend by checking the scope (stored explicitly) against
  // the location of the declaration (using the parent information).

  SgTemplateDeclaration *templateDeclaration = isSgTemplateDeclaration(node);
  ROSE_ASSERT(templateDeclaration != NULL);
  if (templateDeclaration != NULL) {
    // DQ (10/21/2007): Need to make friend function declarations in classes as
    // friend in the declarationModifier However, the only clude that they are
    // friend is that the scope is SgGlobal and parent is SgClassDefinition.
    // Thus when they are different we can safely mark the declaration as being
    // a friend, I think!
    if (templateDeclaration->get_scope() != templateDeclaration->get_parent()) {
      if (templateDeclaration->get_parent() == NULL) {
        printf("templateDeclaration = %p \n", templateDeclaration);
      }

      ROSE_ASSERT(templateDeclaration->get_parent() != NULL);
      ROSE_ASSERT(templateDeclaration->get_scope() != NULL);

      // This should be a friend declaration, verify this.
      // ROSE_ASSERT(templateDeclaration->get_template_kind() ==
      // SgTemplateDeclaration::e_template_function); Make sure this is a
      // template function (not a class or template member function)
      if (templateDeclaration->get_template_kind() ==
          SgTemplateDeclaration::e_template_function) {
        // Only make the declarations that are defined in the class scope
        if (isSgClassDefinition(templateDeclaration->get_parent()) != NULL) {
          // Make sure the scope is not a class definition, should be global
          // scope or a namespace scope.
          // ROSE_ASSERT(isSgClassDefinition(templateDeclaration->get_scope())
          // == NULL);
          if (isSgClassDefinition(templateDeclaration->get_scope()) != NULL) {
            printf("templateDeclaration = %p templateDeclaration->get_scope() "
                   "= %p = %s \n",
                   templateDeclaration, templateDeclaration->get_scope(),
                   templateDeclaration->get_scope()->class_name().c_str());
            templateDeclaration->get_startOfConstruct()->display(
                "templateDeclaration->get_scope() is a SgClassDefinition");
          }
          ROSE_ASSERT(isSgClassDefinition(templateDeclaration->get_scope()) ==
                      NULL);

          templateDeclaration->get_declarationModifier().setFriend();
        }
      }
    }
  }
}

void FixupFriendDeclarations::visit(SgNode *node) {
  SgTemplateInstantiationFunctionDecl *inst_decl =
      isSgTemplateInstantiationFunctionDecl(node);
  if (inst_decl == NULL ||
      inst_decl->get_declarationModifier().isFriend() == false) {
    return;
  }

  SgScopeStatement *lexical_parent =
      isSgScopeStatement(inst_decl->get_parent());
  SgDeclarationStatementPtrList *decls = NULL;
  if (SgClassDefinition *class_def = isSgClassDefinition(lexical_parent)) {
    decls = &class_def->get_members();
  } else if (SgTemplateClassDefinition *template_class_def =
                 isSgTemplateClassDefinition(lexical_parent)) {
    decls = &template_class_def->get_members();
  } else if (SgTemplateInstantiationDefn *inst_def =
                 isSgTemplateInstantiationDefn(lexical_parent)) {
    decls = &inst_def->get_members();
  }
  if (decls == NULL) {
    return;
  }

  auto same_source_position = [](SgLocatedNode *lhs, SgLocatedNode *rhs) {
    if (lhs == NULL || rhs == NULL || lhs->get_file_info() == NULL ||
        rhs->get_file_info() == NULL) {
      return false;
    }

    Sg_File_Info *lhs_fi = lhs->get_file_info();
    Sg_File_Info *rhs_fi = rhs->get_file_info();
    return lhs_fi->get_line() == rhs_fi->get_line() &&
           lhs_fi->get_col() == rhs_fi->get_col() &&
           lhs_fi->get_filenameString() == rhs_fi->get_filenameString();
  };

  auto same_parameter_arity = [](SgFunctionDeclaration *lhs,
                                 SgFunctionDeclaration *rhs) {
    if (lhs == NULL || rhs == NULL) {
      return false;
    }
    SgFunctionParameterList *lhs_params = lhs->get_parameterList();
    SgFunctionParameterList *rhs_params = rhs->get_parameterList();
    if (lhs_params == NULL || rhs_params == NULL) {
      return lhs_params == rhs_params;
    }
    return lhs_params->get_args().size() == rhs_params->get_args().size();
  };
  SgName inst_base_name = inst_decl->get_templateName().is_null()
                              ? inst_decl->get_name()
                              : inst_decl->get_templateName();

  for (SgDeclarationStatementPtrList::iterator it = decls->begin();
       it != decls->end();) {
    SgFunctionDeclaration *candidate = isSgFunctionDeclaration(*it);
    if (candidate == NULL || candidate == inst_decl ||
        isSgTemplateInstantiationFunctionDecl(candidate) != NULL ||
        candidate->get_declarationModifier().isFriend() == false) {
      ++it;
      continue;
    }
    if (candidate->get_name() != inst_base_name) {
      ++it;
      continue;
    }
    if (same_parameter_arity(candidate, inst_decl) == false) {
      ++it;
      continue;
    }
    if (same_source_position(candidate, inst_decl) == false) {
      ++it;
      continue;
    }

    if (candidate->get_file_info() != NULL) {
      candidate->get_file_info()->unsetOutputInCodeGeneration();
    }
    if (SgFunctionParameterList *params = candidate->get_parameterList()) {
      if (params->get_file_info() != NULL) {
        params->get_file_info()->unsetOutputInCodeGeneration();
      }
      for (SgInitializedName *param : params->get_args()) {
        if (param != NULL && param->get_file_info() != NULL) {
          param->get_file_info()->unsetOutputInCodeGeneration();
        }
      }
    }
    it = decls->erase(it);
    continue;
  }
}
