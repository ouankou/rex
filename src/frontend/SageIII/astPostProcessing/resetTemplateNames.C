// tps (01/14/2010) : Switching from rose.h to sage3.
#include "resetTemplateNames.h"

#include "sage3basic.h"
// DQ (12/31/2005): This is OK if not declared in a header file
using namespace std;

// DQ (2/19/2007): This is a simplified handling of template names using the
// memory pool traversal on specific IR nodes
// (SgTemplateInstantiationDecl,SgTemplateInstantiationFunctionDecl,and
// SgTemplateInstantiationMemberFunctionDecl) so that we can be certain that we
// visit ALL relavant IR nodes (the AST traversal forced us to hunt for them, as
// in the resetParent() traversal).
void ResetTemplateNamesOnMemoryPool::visit(SgNode *node) {
  ROSE_ASSERT(node);
  // cerr << "reset parent for node " << node->unparseToString();

  switch (node->variantT()) {
  case V_SgTemplateInstantiationDecl: {
    SgTemplateInstantiationDecl *templateClassDeclaration =
        isSgTemplateInstantiationDecl(node);
    ROSE_ASSERT(templateClassDeclaration != NULL);
    // Calling resetTemplateName()
    templateClassDeclaration->resetTemplateName();
    break;
  }

  case V_SgTemplateInstantiationFunctionDecl: {
    SgTemplateInstantiationFunctionDecl *templateFunctionDeclaration =
        isSgTemplateInstantiationFunctionDecl(node);
    ROSE_ASSERT(templateFunctionDeclaration != NULL);

    // DQ (2/16/2005): For now as a test, output the mangled name
    // string oldName     = templateFunctionDeclaration->get_name().str();
    // string mangledName =
    // templateFunctionDeclaration->get_mangled_name().str(); printf ("template
    // function: oldName = %s mangledName = %s
    // \n",oldName.c_str(),mangledName.c_str());
    templateFunctionDeclaration->resetTemplateName();
    // string newName     = templateFunctionDeclaration->get_name().str();
    // printf ("template function: oldName = %s mangledName = %s newName = %s
    // \n",oldName.c_str(),mangledName.c_str(),newName.c_str());
    break;
  }

    // DQ (10/20/2004): Not clear if this case will be a problem
  case V_SgTemplateInstantiationMemberFunctionDecl: {
    SgTemplateInstantiationMemberFunctionDecl
        *templateMemberFunctionDeclaration =
            isSgTemplateInstantiationMemberFunctionDecl(node);
    ROSE_ASSERT(templateMemberFunctionDeclaration != NULL);

    // It is better to set this properly than just have the constructor name
    // exclude the template parameters because the constructor name is used for
    // several purposes (e.g. case operator names) and here the template names
    // arguments are required even if they are not required for the constructors
    // function declaration.
    if (templateMemberFunctionDeclaration->get_specialFunctionModifier()
            .isConstructor() ||
        templateMemberFunctionDeclaration->get_specialFunctionModifier()
            .isDestructor() ||
        templateMemberFunctionDeclaration->get_specialFunctionModifier()
            .isConversion()) {
      // printf ("Case V_SgTemplateInstantiationMemberFunctionDecl: found a
      // constructor, destructor or conversion operator \n");
      // templateMemberFunctionDeclaration->set_name(fixedUpName);

      // DQ (7/26/2007): Modified to handle case where class scope is not
      // avialble (see test2007_116.C).
      // ROSE_ASSERT(templateMemberFunctionDeclaration->get_class_scope() !=
      // NULL);

      // DQ (7/28/2007): Modified to use get_scope(), it might still be too eary
      // to call get_class_scope(). printf ("Modified to use get_scope(), it
      // might still be too early to call get_class_scope() \n"); if
      // (templateMemberFunctionDeclaration->get_class_scope() != NULL)
      if (templateMemberFunctionDeclaration->get_scope() != NULL) {
        // ROSE_ASSERT(templateMemberFunctionDeclaration->get_class_scope()->get_declaration()
        // != NULL); SgName className =
        // templateMemberFunctionDeclaration->get_class_scope()->get_declaration()->get_name();
        // ROSE_ASSERT(templateMemberFunctionDeclaration->get_scope()->get_declaration()
        // != NULL); SgName className =
        // templateMemberFunctionDeclaration->get_scope()->get_declaration()->get_name();
        SgScopeStatement *classScope =
            templateMemberFunctionDeclaration->get_scope();
        SgClassDefinition *classDefinition = isSgClassDefinition(classScope);
        ROSE_ASSERT(classDefinition != NULL);
        SgClassDeclaration *classDeclaration =
            classDefinition->get_declaration();
        ROSE_ASSERT(classDeclaration != NULL);
        // ROSE_ASSERT(templateMemberFunctionDeclaration->get_scope()->get_declaration()
        // != NULL); SgName className =
        // templateMemberFunctionDeclaration->get_scope()->get_declaration()->get_name();
        SgName className = classDeclaration->get_name();
        SgName fixedUpName = className;
        // printf ("fixedUpName = %s \n",fixedUpName.str());
        if (templateMemberFunctionDeclaration->get_specialFunctionModifier()
                .isDestructor()) {
          // fixedUpName = string("~") + className;

          // DQ (2/10/2007): I am unclear why we don't reset the name here
          // (commented out)! Likely it is done in the code generation phase.
          // printf ("In ASTFixs: Destructor case fixedUpName = %s (original
          // name = %s) (why don't we call set_name for the
          // SgTemplateInstantiationMemberFunctionDecl?)
          // \n",fixedUpName.str(),templateMemberFunctionDeclaration->get_name().str());
          // templateMemberFunctionDeclaration->set_name(fixedUpName);
          // templateMemberFunctionDeclaration->set_name(fixedUpName);
        } else {
          if (templateMemberFunctionDeclaration->get_specialFunctionModifier()
                  .isConstructor() == true) {
            // DQ (2/19/2007): The symbol table will be handled by the
            // resetTemplateName() called below! printf ("WARNING: Need to
            // unload symbol for %s and load symbol for %s
            // \n",className.str(),fixedUpName.str());

            // I think that this should be commented out since the name will be
            // updated directly
            // templateMemberFunctionDeclaration->set_name(fixedUpName);

            // ROSE_ABORT();
          } else {
            // DQ (11/24/2004): Separate out the case of a conversion operator
            // (don't reset the name from the form "operator X&(result)"). See
            // test2004_141.C.
            ROSE_ASSERT(
                templateMemberFunctionDeclaration->get_specialFunctionModifier()
                    .isConversion() == true);
            // templateMemberFunctionDeclaration->set_name(fixedUpName);
          }
        }
      } else {
        printf(
            "Case where templateMemberFunctionDeclaration->get_class_scope() "
            "== NULL \n");
        ROSE_ABORT();
      }
    } else {
      // This was implemented as a test when there were problems finding the use
      // of the STL string class and changing all the names to include the
      // template arguments explicitly.
      // ROSE_ASSERT(templateMemberFunctionDeclaration->get_name() !=
      // "basic_string");

      // This is the case of a member function, if it is a template member
      // function then we should modify the name to include the template
      // parameters.

      // DQ (2/16/2005): For now as a test, output the mangled name
      // string mangledName =
      // templateMemberFunctionDeclaration->get_mangled_name().str(); printf
      // ("member function: mangledName = %s \n",mangledName.c_str());
    }

    // printf ("ResetTemplateNames::visit(): case
    // V_SgTemplateInstantiationMemberFunctionDecl: not implemented! \n");
    // ROSE_ABORT();

    // Should we be calling resetTemplateName()
    // templateClassDeclaration->resetTemplateName();
    // DQ (2/17/2005): For now as a test, output the mangled name

    // DQ (2/19/2007): This function will correctly unload/reload the symbol
    // table.
    templateMemberFunctionDeclaration->resetTemplateName();
    break;
  }

  default: {
    // Don't worry about any other nodes (since we use the memory pool we know
    // that we get all the template instantiations)!
    break;
  }
  }
}

void resetTemplateNames(SgNode *node) {
  // DQ (7/7/2005): Introduce tracking of performance of ROSE.
  TimingPerformance resetTemplateNameTimer("Reset template names:");

  // DQ (7/29/2005): Added support with Qing for AST fragments that occure in
  // the ASTInterface classes. I gather that once the parent pointer is set then
  // that indicates that the class declaration is part of a valid AST?
  // setTemplateNamesTraversal.traverse(node,preorder);
  // GB (8/19/2009): Allowing SgProject nodes here, under the assumption
  // that a project node also represents a valid AST.
  if (node->get_parent() != NULL || isSgProject(node)) {
    // DQ (2/10/2007): Use the memory pool traversal so that we can get all
    // relavant IR nodes (missed to many previously, which made the AST merge a
    // problem) ResetTemplateNames setTemplateNamesTraversal;
    // setTemplateNamesTraversal.traverse(node,preorder);

    // DQ (2/19/2007): We can alternatively just traverse the types of IR nodes
    // that are relavant, instead of the whole memory pool.  This is a cool
    // feature of the memory pool traversal in this case this make the traversal
    // about 30% faster, since we visit such a small subset of IR nodes.
    ResetTemplateNamesOnMemoryPool t;
    SgTemplateInstantiationDecl::traverseMemoryPoolNodes(t);
    SgTemplateInstantiationFunctionDecl::traverseMemoryPoolNodes(t);
    SgTemplateInstantiationMemberFunctionDecl::traverseMemoryPoolNodes(t);
  } else {
    // DQ (2/10/2007): This should hopefully not happen on too many IR nodes of
    // a properly built AST!
    printf("Detected AST fragement not associated with primary AST, ignore "
           "template handling ... (premature point to call "
           "resetTemplateNames() function) \n");
  }
}
