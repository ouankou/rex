// Example ROSE Translator used for testing ROSE infrastructure
#include "rose.h"

namespace {

bool isCudaTraversalMarkerType(SgType *type) {
  if (type == NULL) {
    return false;
  }

  type = type->stripType(
      SgType::STRIP_MODIFIER_TYPE | SgType::STRIP_REFERENCE_TYPE |
      SgType::STRIP_RVALUE_REFERENCE_TYPE | SgType::STRIP_TYPEDEF_TYPE);
  SgClassType *classType = isSgClassType(type);
  if (classType == NULL) {
    return false;
  }

  SgClassDeclaration *decl = isSgClassDeclaration(classType->get_declaration());
  if (decl == NULL) {
    return false;
  }

  return decl->get_name() == "cuda_traversal";
}

bool lambdaIsMarkedByTraversalCall(SgFunctionCallExp *functionCallExp,
                                   SgLambdaExp *lambdaExp) {
  if (functionCallExp == NULL || lambdaExp == NULL) {
    return false;
  }

  SgExprListExp *args = functionCallExp->get_args();
  if (args == NULL) {
    return false;
  }

  const SgExpressionPtrList &expressions = args->get_expressions();
  size_t lambdaIndex = 0;
  bool foundLambda = false;
  for (SgExpressionPtrList::const_iterator i = expressions.begin();
       i != expressions.end(); ++i, ++lambdaIndex) {
    if (*i == lambdaExp) {
      foundLambda = true;
      break;
    }
  }

  if (!foundLambda) {
    return false;
  }

  for (size_t i = 0; i < lambdaIndex; ++i) {
    if (isCudaTraversalMarkerType(expressions[i]->get_type())) {
      return true;
    }
  }

  return false;
}

} // namespace

// Build an inherited attribute for the tree traversal to test the rewrite mechanism
class InheritedAttribute
   {
     public:
         SgFunctionCallExp* functionCallExp;

       // Specific constructors are required
          InheritedAttribute () {};
          InheritedAttribute ( const InheritedAttribute & X ) : functionCallExp(X.functionCallExp) {};
   };

class visitorTraversal : public AstTopDownProcessing<InheritedAttribute>
   {
     public:
       // virtual function must be defined
          virtual InheritedAttribute evaluateInheritedAttribute(SgNode* n, InheritedAttribute inheritedAttribute);
   };

InheritedAttribute
visitorTraversal::evaluateInheritedAttribute(SgNode* n, InheritedAttribute inheritedAttribute)
   {
     SgFunctionCallExp* functionCallExp = isSgFunctionCallExp(n);
     if (functionCallExp != NULL)
        {
          inheritedAttribute.functionCallExp = functionCallExp;
        }

     SgLambdaExp* lambdaExp = isSgLambdaExp(n);
     if (lambdaExp != NULL) {
       SgExprListExp *exprListExp = isSgExprListExp(lambdaExp->get_parent());
       ROSE_ASSERT(exprListExp != NULL);

       if (inheritedAttribute.functionCallExp != NULL) {
         if (lambdaIsMarkedByTraversalCall(inheritedAttribute.functionCallExp,
                                           lambdaExp)) {
           lambdaExp->set_is_device(true);
         }
       } else {
         printf("Error: We should have seen a function call expression at this "
                "point \n");
         ROSE_ASSERT(false);
       }
     }

     return inheritedAttribute;
   }


int main( int argc, char * argv[] )
   {
  // Generate the ROSE AST.
     SgProject* project = frontend(argc,argv);

  // AST consistency tests (optional for users, but this enforces more of our tests)
     AstTests::runAllTests(project);

  // Build the inherited attribute
     InheritedAttribute inheritedAttribute;

  // Build the traversal object
     visitorTraversal exampleTraversal;

  // Call the traversal starting at the project node of the AST
     exampleTraversal.traverseInputFiles(project,inheritedAttribute);

  // Or the traversal over all AST IR nodes can be called!
     exampleTraversal.traverse(project,inheritedAttribute);

  // regenerate the source code and call the vendor 
  // compiler, only backend error code is reported.
     return backend(project);
   }
