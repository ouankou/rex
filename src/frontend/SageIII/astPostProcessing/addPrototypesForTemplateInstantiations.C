#include "addPrototypesForTemplateInstantiations.h"

#include "sage3basic.h"

void addPrototypesForTemplateInstantiations(SgNode *node) {
  // This function must operate on the SgFile or SgProject, becauae based on the
  // existence of a template instantation being marked as output, we have to
  // find the first location of the call to the template instantiation, and then
  // insert a prototype declaration of the instantiated template.  This is only
  // required for template instantiations that do not already have a template
  // instantion prototype declaration.

  // DQ (7/7/2005): Introduce tracking of performance of ROSE.
  TimingPerformance timer("Add Template Instantiation Prototypes for output:");

  CollectTemplateInstantiationsMarkedForOutput collectDeclarationsTraversal;
  collectDeclarationsTraversal.traverse(node, preorder);

  // Save a reference to the set of defining template instantiations.
  std::set<SgDeclarationStatement *> &definingTemplateInstantiationSet =
      collectDeclarationsTraversal.definingTemplateInstantiationSet;

  AddPrototypesForTemplateInstantiations declarationFixupTraversal(
      definingTemplateInstantiationSet);

  // This inherited attribute is used for all traversals (within the iterative
  // approach we define)
  AddPrototypesForTemplateInstantiationsInheritedAttribute inheritedAttribute;

  // This will be called iteratively so that we can do a fixed point iteration
  declarationFixupTraversal.traverse(node, inheritedAttribute);
}

// We need to record all of the requirements for template instantiation
// prototypes (that don't have prototypes). Then we need to find all of the
// template instantiatons that are marked for output (at the end of the file).

// Then we need to add template instantiation prototypes in a reasonable scope
// before the location where they are required, so that they will be properly
// defined.

// So this requires two traversals, one to collect the template instantiations
// to be output, and a second to evaluate if the first call to the template
// instantiation is proceeded by a template instantiation prototype.

// Note that template instantiations that are used in header files and thus for
// which we can't support the addition of a template instantiation prototype,
// can not be makred to be unparsed in the source file.

// DQ (6/21/2005): This class controls the output of template declarations in
// the generated code (by the unparser).
void CollectTemplateInstantiationsMarkedForOutput::visit(SgNode *node) {

  // Collect all defining template instantiations that are marked to be output
  // in the ROSE Generated code.
  SgTemplateInstantiationFunctionDecl *functionDeclaration =
      isSgTemplateInstantiationFunctionDecl(node);
  if (functionDeclaration != NULL) {
    SgTemplateInstantiationFunctionDecl *definingFunctionDeclaration =
        isSgTemplateInstantiationFunctionDecl(
            functionDeclaration->get_definingDeclaration());
    if (functionDeclaration == definingFunctionDeclaration) {
      // Add function declaration for defining template instantitation.
      // Declarations can be reachable multiple times (e.g., re-entrant
      // namespaces). The set handles de-duplication.
      definingTemplateInstantiationSet.insert(definingFunctionDeclaration);
    }
  }
}

AddPrototypesForTemplateInstantiations::AddPrototypesForTemplateInstantiations(
    std::set<SgDeclarationStatement *> &definingTemplateInstantiationSet)
    : definingTemplateInstantiationSet(definingTemplateInstantiationSet) {
  // We need to setup a new set of the first non-defining declarations
  // associated with the input set of defining declarations, so that we can test
  // uses (e.g. function calls) of instantiated templates (which as only
  // associated with the non-defining declarations).  Or we always only set the
  // set by first finding the defining declarations.
}

AddPrototypesForTemplateInstantiationsInheritedAttribute
AddPrototypesForTemplateInstantiations::evaluateInheritedAttribute(
    SgNode *node, AddPrototypesForTemplateInstantiationsInheritedAttribute
                      inheritedAttribute) {

  // Record the defining and or nondefining template instantiations so that we
  // can know if they have been declared ahead of the uses detected below.
  SgTemplateInstantiationFunctionDecl *functionDeclaration =
      isSgTemplateInstantiationFunctionDecl(node);
  if (functionDeclaration != NULL) {
    // Removed unused variable declaration [Rasmussen 2019.01.29]
    // SgTemplateInstantiationFunctionDecl* definingFunctionDeclaration =
    // isSgTemplateInstantiationFunctionDecl(functionDeclaration->get_definingDeclaration());
    SgTemplateInstantiationFunctionDecl *firstNondefiningFunctionDeclaration =
        isSgTemplateInstantiationFunctionDecl(
            functionDeclaration->get_firstNondefiningDeclaration());
    ROSE_ASSERT(firstNondefiningFunctionDeclaration != NULL);

    // Save the non defining declaration.
    if (prototypeTemplateInstantiationSet.find(
            firstNondefiningFunctionDeclaration) ==
        prototypeTemplateInstantiationSet.end()) {
      prototypeTemplateInstantiationSet.insert(
          firstNondefiningFunctionDeclaration);
    }
  }

  SgFunctionRefExp *functionRefExp = isSgFunctionRefExp(node);
  if (functionRefExp != NULL) {
    SgFunctionSymbol *functionSymbol =
        isSgFunctionSymbol(functionRefExp->get_symbol());
    ROSE_ASSERT(functionSymbol != NULL);
    SgFunctionDeclaration *functionDeclaration =
        functionSymbol->get_declaration();
    ROSE_ASSERT(functionDeclaration != NULL);
    SgFunctionDeclaration *definingFunctionDeclaration =
        isSgFunctionDeclaration(functionDeclaration->get_definingDeclaration());
    if (definingFunctionDeclaration != NULL) {
      if (definingTemplateInstantiationSet.find(definingFunctionDeclaration) !=
          definingTemplateInstantiationSet.end()) {
        if (usedTemplateInstantiationSet.find(functionRefExp) ==
            usedTemplateInstantiationSet.end()) {
          usedTemplateInstantiationSet.insert(functionRefExp);

          SgFunctionDeclaration *firstNondefiningFunctionDeclaration =
              isSgFunctionDeclaration(
                  functionDeclaration->get_firstNondefiningDeclaration());
          ROSE_ASSERT(firstNondefiningFunctionDeclaration != NULL);

          if (prototypeTemplateInstantiationSet.find(
                  firstNondefiningFunctionDeclaration) !=
              prototypeTemplateInstantiationSet.end()) {
          } else {
          }
        }
      }
    }
  }

  return inheritedAttribute;
}
