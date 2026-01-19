// ROSE is a tool for building preprocessors, this file is an example
// preprocessor built with ROSE. rose.C: Example (default) ROSE Preprocessor:
// used for testing ROSE infrastructure
#include "rose.h"
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <iomanip>

#include <string>

#include <AstTests.h>

#include <algorithm>

// Build an inherited attribute for the tree traversal to test the rewrite
// mechanism
class dqInheritedAttribute {
public:
  //! Specific constructors are required
  dqInheritedAttribute() {};
  dqInheritedAttribute(const dqInheritedAttribute &X) {};
};

// Build a synthesized attribute for the tree traversal to test the rewrite
// mechanism
class dqSynthesizedAttribute {
public:
  dqSynthesizedAttribute() {};
};

// tree traversal to test the rewrite mechanism
/*! A specific AST processing class is used (built from
 * SgTopDownBottomUpProcessing)
 */
class dqTraversal : public SgTopDownBottomUpProcessing<dqInheritedAttribute,
                                                       dqSynthesizedAttribute> {
public:
  // This value is a temporary data member to allow us to output the number of
  // nodes traversed so that we can relate this number to the numbers printed
  // in the AST graphs output via DOT.
  int traversalNodeCounter;

  // list of types that have been traversed
  static list<SgNode *> listOfTraversedTypes;

  dqTraversal() : traversalNodeCounter(0) {};

  // Functions required by the rewrite mechanism
  dqInheritedAttribute
  evaluateInheritedAttribute(SgNode *astNode,
                             dqInheritedAttribute inheritedAttribute);

  dqSynthesizedAttribute evaluateSynthesizedAttribute(
      SgNode *astNode, dqInheritedAttribute inheritedAttribute,
      SubTreeSynthesizedAttributes synthesizedAttributeList);
};

// Allocation of space for listOfTraversedTypes declared in dqTraversal
list<SgNode *> dqTraversal::listOfTraversedTypes;

// Functions required by the tree traversal mechanism
dqInheritedAttribute dqTraversal::evaluateInheritedAttribute(
    SgNode *astNode, dqInheritedAttribute inheritedAttribute) {

  traversalNodeCounter++;

  switch (astNode->variantT()) {
  case V_SgTypedefDeclaration: {
    //             printf ("Found a SgTypedefDeclaration \n");
    break;
  }
  case V_SgTypedefType: {
    //             printf ("Found a SgTypedefType \n");
    SgTypedefType *typedefType = isSgTypedefType(astNode);
    ROSE_ASSERT(typedefType != NULL);
    //             printf ("typedefType->get_name() = %s \n",
    //                  (typedefType->get_name().str() != NULL) ?
    //                  typedefType->get_name().str() : "<Null String>");

    SgType *baseType = typedefType->get_base_type();
    //             printf ("typedefType->get_base_type()->sage_class_name() = %s
    //             \n",
    //                  (baseType != NULL) ? baseType->sage_class_name() :
    //                  "<Null Pointer>");

    if (baseType != NULL) {
      //                  printf ("listOfTraversedTypes.size() = %zu
      //                  \n",listOfTraversedTypes.size());
      list<SgNode *>::iterator previouslyTraversedType = find(
          listOfTraversedTypes.begin(), listOfTraversedTypes.end(), baseType);
      bool traverseBaseType =
          (previouslyTraversedType == listOfTraversedTypes.end());
      //                  printf ("traverseBaseType = %s \n",(traverseBaseType)
      //                  ? "true" : "false");
      if (traverseBaseType == true) {
        // Add to list of traversed types
        listOfTraversedTypes.push_back(baseType);

        //                       printf
        //                       ("##########################################################
        //                       \n"); printf ("Traverse the base type \n");
        // traverse the base type (skipped by the traversal mechanism, seems to
        // be a bug!)
        dqTraversal treeTraversal;
        dqInheritedAttribute inheritedAttribute;

        // Ignore the return value since we don't need it
        treeTraversal.traverse(baseType, inheritedAttribute);

        //                       printf
        //                       ("##########################################################
        //                       \n");

        // Add to list of traversed types
        //                       listOfTraversedTypes.push_back(baseType);
      }
    }

    break;
  }
  case V_SgTypedefSeq: {
    //             printf ("Found a SgTypedefSeq \n");
    SgTypedefSeq *typedefSequence = isSgTypedefSeq(astNode);
    ROSE_ASSERT(typedefSequence != NULL);
    break;
  }
  case V_SgClassType: {
    //             printf ("Found a SgClassType \n");
    SgClassType *classType = isSgClassType(astNode);
    ROSE_ASSERT(classType != NULL);
    //             printf ("classType->get_name() = %s \n",
    //                  (classType->get_name().str() != NULL) ?
    //                  classType->get_name().str() : "<Null String>");
    break;
  }
  case V_SgVariableDeclaration: {
    //             printf ("Found a SgVariableDeclaration \n");
    SgVariableDeclaration *variableDeclaration =
        isSgVariableDeclaration(astNode);
    ROSE_ASSERT(variableDeclaration != NULL);
    //             variableDeclaration->get_file_info()->display("Called from
    //             SgVariableDeclaration case ... ");
    break;
  }
  }

  return inheritedAttribute;
}

dqSynthesizedAttribute dqTraversal::evaluateSynthesizedAttribute(
    SgNode *astNode, dqInheritedAttribute inheritedAttribute,
    SubTreeSynthesizedAttributes synthesizedAttributeList) {
  dqSynthesizedAttribute returnAttribute;

  switch (astNode->variantT()) {
  case V_SgTypedefType: {
    break;
  }
  }

  return returnAttribute;
}

#include <sageCommonSourceHeader.h>
extern an_il_header il_header;

int main(int argc, char *argv[]) {
  // Main Function for default example ROSE Preprocessor
  // This is an example of a preprocessor that can be built with ROSE
  // This example can be used to test the ROSE infrastructure

  ios::sync_with_stdio(); // Syncs C++ and C I/O subsystems!

  printf("In preprocessor.C: main() \n");

  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != NULL);

  // DQ (2/6/2004): These tests fail in Coco for test2004_14.C
  AstTests::runAllTests(const_cast<SgProject *>(project));

  // printf ("Generate the pdf output of the SAGE III AST \n");
  // generatePDF ( project );

  printf("Generate the DOT output of the SAGE III AST \n");
  generateDOT(*project);
  printf("DONE: Generate the DOT output of the SAGE III AST \n");

  printf("Calling the backend() \n");

  return backend(project);
  // return backend(frontend(argc,argv));
}
