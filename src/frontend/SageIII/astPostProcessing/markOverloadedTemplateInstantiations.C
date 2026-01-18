// tps (01/14/2010) : Switching from rose.h to sage3.
#include "astPostProcessing/markOverloadedTemplateInstantiations.h"

#include "sage3basic.h"

// DQ (10/14/2010):  This should only be included by source files that require
// it. This fixed a reported bug which caused conflicts with configure-time
// macros (e.g. PACKAGE_BUGREPORT). Interestingly it must be at the top of the
// list of include files.
#include "rose_config.h"
// DQ (12/31/2005): This is OK if not declared in a header file
using namespace std;

void markOverloadedTemplateInstantiations(SgNode *node) {
  // This simplifies how the traversal is called!
  MarkOverloadedTemplateInstantiations astTraversal;

  // I think the default should be preorder so that the interfaces would be more
  // uniform
  astTraversal.traverse(node, preorder);
}

void MarkOverloadedTemplateInstantiations::visit(SgNode *node) {
  // This function marks member functions that are overloaded with templated
  // member function the output of the non-template member function will bring
  // out a bug in GNU g++. This processing needs to be done after all template
  // instatiations are marked for output so that it can turn off the output of
  // specific overloaded member function template instantiations that would
  // confuse g++.

  // Note that this bug in g++ prevents the transformation of non-template
  // member functions in template classes that are overloaded with template
  // member function in the same templated class.

  // printf ("MarkOverloadedTemplateInstantiations::visit(%p = %s)
  // \n",node,node->class_name().c_str());

  SgTemplateInstantiationMemberFunctionDecl *memberFunctionInstantiation =
      isSgTemplateInstantiationMemberFunctionDecl(node);
  if (memberFunctionInstantiation != NULL) {
    bool searchForOverloadedMemberFunctions = false;
    if (memberFunctionInstantiation->get_file_info()
            ->isOutputInCodeGeneration() == true) {
      searchForOverloadedMemberFunctions = true;
    }

    if (searchForOverloadedMemberFunctions == true) {
      bool isOverloaded =
          SageInterface::isOverloaded(memberFunctionInstantiation);
      if (isOverloaded == true) {
        // DQ (11/6/2007): The code above is equivalent to this simpler code
        // below.
        if (memberFunctionInstantiation->isSpecialization() == false) {
          // printf ("*** Calling unsetOutputInCodeGeneration on
          // memberFunctionInstantiation = %p \n",memberFunctionInstantiation);
          memberFunctionInstantiation->get_file_info()
              ->unsetOutputInCodeGeneration();
        }
      }
    }
  }
}
