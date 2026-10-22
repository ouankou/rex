// Original Author (AstProcessing classes): Markus Schordan
// Rewritten by: Gergo Barany

#ifndef ROSE_ASTPROCESSING_H
#define ROSE_ASTPROCESSING_H
#include "featureTests.h"

#include "mlog.h"

#include <algorithm>

#include <iostream>

#include "rosedefs.h"

#include "rose_attributes_list.h"

#include "Cxx_Grammar.h"

#include <utility>

#include <vector>

// Non-templated helper function in AstProcessing.C

ROSE_DLL_API bool SgTreeTraversal_inFileToTraverse(SgNode *node,
                                                   bool traversalConstraint,
                                                   SgFile *fileToVisit);

#include "AstSuccessorsSelectors.h"

#include "StackFrameVector.h"

// This type is used as a dummy template parameter for those traversals
// that do not use inherited or synthesized attributes.
typedef void *DummyAttribute;
// We initialize DummyAttributes to this value to avoid "may not be
// initialized" warnings. If you change the typedef, adjust the constant...
static const DummyAttribute defaultDummyAttribute = nullptr;
// The attribute _DummyAttribute is reserved for the implementation, so we
// should deprecate it, but there is code using it explicitly. Which it
// shouldn't.
typedef DummyAttribute _DummyAttribute;

template <class InheritedAttributeType, class SynthesizedAttributeType>
class SgCombinedTreeTraversal;

// Attribute-bearing traversal base; use the Ast*Processing classes directly.
template <class InheritedAttributeType, class SynthesizedAttributeType>
class SgTreeTraversal {
public:
  typedef StackFrameVector<SynthesizedAttributeType> SynthesizedAttributesList;

  SynthesizedAttributeType
  traverse(SgNode *basenode, InheritedAttributeType inheritedValue,
           t_traverseOrder travOrder = preandpostorder);

  SynthesizedAttributeType
  traverseWithinFile(SgNode *basenode, InheritedAttributeType inheritedValue,
                     t_traverseOrder travOrder = preandpostorder);

  void traverseInputFiles(SgProject *projectNode,
                          InheritedAttributeType inheritedValue,
                          t_traverseOrder travOrder = preandpostorder);

  // Default destructor/constructor
  virtual ~SgTreeTraversal();
  SgTreeTraversal();

  // Copy operations
  SgTreeTraversal(const SgTreeTraversal &);
  const SgTreeTraversal &operator=(const SgTreeTraversal &);

  friend class SgCombinedTreeTraversal<InheritedAttributeType,
                                       SynthesizedAttributeType>;

#include <Cxx_GrammarTreeTraversalAccessEnums.h>
protected:
  virtual InheritedAttributeType
  evaluateInheritedAttribute(SgNode *astNode,
                             InheritedAttributeType inheritedValue) = 0;

  virtual SynthesizedAttributeType
  evaluateSynthesizedAttribute(SgNode *n, InheritedAttributeType in,
                               SynthesizedAttributesList l) = 0;

  typedef
      typename AstSuccessorsSelectors::SuccessorsContainer SuccessorsContainer;
  typedef SuccessorsContainer &SuccessorsContainerRef;

  // GB (09/25/2007): Feel free to override this to implement custom traversals,
  // but see the comment for set_useDefaultIndexBasedTraversal() below!
  virtual void setNodeSuccessors(SgNode *node,
                                 SuccessorsContainer &succContainer);

  virtual SynthesizedAttributeType
  defaultSynthesizedAttribute(InheritedAttributeType inh);

  // GB (06/04/2007): A new virtual function called at the start of the
  // traversal, before any node is actually visited; can be used to
  // compute attributes that may have changed since the constructor was
  // executed, but are constant during the traversal itself. A no-op by
  // default. If you don't know what you would use this for, ignore it.
  virtual void atTraversalStart();
  // GB (06/13/2007): Added this just for symmetry, not sure if it is
  // useful, but it won't hurt to have it.
  virtual void atTraversalEnd();

  // GB (09/25/2007): This flag determines whether the new index-based traversal
  // mechanism or the more general mechanism based on successor containers is to
  // be used. Indexing should be faster, but it would be quite hard to adapt it
  // to the reverse traversal and other specialized traversals. Thus: This is
  // true by default, and anybody who overrides setNodeSuccessors() *must*
  // change this to false to force the traversal to use their custom successor
  // container.
  void set_useDefaultIndexBasedTraversal(bool);

private:
  void performTraversal(SgNode *basenode, InheritedAttributeType inheritedValue,
                        t_traverseOrder travOrder);
  SynthesizedAttributeType traversalResult();

  bool useDefaultIndexBasedTraversal;
  bool traversalConstraint;
  SgFile *fileToVisit;

  // stack of synthesized attributes; evaluateSynthesizedAttribute() is
  // automagically called with the appropriate stack frame, which
  // behaves like a non-resizable std::vector
  SynthesizedAttributesList *synthesizedAttributes;
};

template <class InheritedAttributeType, class SynthesizedAttributeType>
class AstCombinedTopDownBottomUpProcessing;

template <class InheritedAttributeType, class SynthesizedAttributeType>
/** @brief Attribute evaluator for inherited and synthesized attributes.
 *
 * In general, this class combines the classes TopDownProcessing and
 * BottomUpProcessing and also allows to use the inherited attribute of a node
 * in the computation of the synthesized attribute at the same node.
 *
 * This class allows computation of inherited and synthesized attributes on the
 * AST. It requires an inherited attribute type and a synthesized attribute type
 * as template parameters and the implementation of the functions
 * evaluateInheritedAttribute and evaluateSynthesizedAttribute. The function
 * evaluateInheritedAttribute is invoked in pre-order, the function
 * evaluateSynthesizedAttribute is invoked in post-order while the AST is
 * traversed. The function evaluateSynthesizedAttribute gets an additional
 * parameter: the inheritedAttribute value which is computed at the respective
 * node. It can be used to make the computation of the synthesized attribute at
 * a node dependent on the value of the inherited attribute of the same node.
 *
 * Internal: This class is derived from the SgTreeTraversal class.
 */
class AstTopDownBottomUpProcessing
    : public SgTreeTraversal<InheritedAttributeType, SynthesizedAttributeType> {
public:
  typedef typename SgTreeTraversal<InheritedAttributeType,
                                   SynthesizedAttributeType>::
      SynthesizedAttributesList SynthesizedAttributesList;

  // deprecated
  typedef SynthesizedAttributesList SubTreeSynthesizedAttributes;

  //! evaluates attributes on the entire AST
  SynthesizedAttributeType traverse(SgNode *node,
                                    InheritedAttributeType inheritedValue);

  //! evaluates attributes only at nodes which represent the same file as where
  //! the evaluation was started
  SynthesizedAttributeType
  traverseWithinFile(SgNode *node, InheritedAttributeType inheritedValue);

  friend class AstCombinedTopDownBottomUpProcessing<InheritedAttributeType,
                                                    SynthesizedAttributeType>;

protected:
  //! pure virtual function which must be implemented to compute the inherited
  //! attribute at a node
  virtual InheritedAttributeType
  evaluateInheritedAttribute(SgNode *astNode,
                             InheritedAttributeType inheritedValue) = 0;

  //! pure virtual function which must be implemented to compute the synthesized
  //! attribute at a node. The list of synthesized attributes consists of the
  //! synthesized attributes computed at the children node of the current node.
  //! The inherited attribute value is computed by evaluateInheritedAttribute at
  //! the same node and simply passed to this function. Use the typedef
  //! SynthesizedAttributeList as type for the synthesized attributes list.
  virtual SynthesizedAttributeType
  evaluateSynthesizedAttribute(SgNode *, InheritedAttributeType,
                               SynthesizedAttributesList) = 0;

  //! Function called at the start of the traversal, before any node is
  //! visited; override if necessary, the default implementation is a
  //! no-op.
  virtual void atTraversalStart();
  virtual void atTraversalEnd();
};

template <class InheritedAttributeType> class AstCombinedTopDownProcessing;

template <class InheritedAttributeType>
class DistributedMemoryAnalysisPreTraversal;

template <class InheritedAttributeType>
/** @brief Attribute evaluator for inherited attributes.
 *
 * This class allows computation of inherited attributes on the AST. It requires
 * an inherited attribute type as a template parameter and the implementation of
 * the function evaluateInheritedAttribute. This function is invoked in
 * pre-order while the AST is traversed. It can be used for passing context
 * information down the AST.
 *
 * Internal: This class is derived from the SgTreeTraversal class.
 */
class AstTopDownProcessing
    : public SgTreeTraversal<InheritedAttributeType, DummyAttribute> {
public:
  typedef typename SgTreeTraversal<InheritedAttributeType,
                                   DummyAttribute>::SynthesizedAttributesList
      SynthesizedAttributesList;

  //! evaluates attributes on the entire AST
  void traverse(SgNode *node, InheritedAttributeType inheritedValue);

  //! evaluates attributes only at nodes which represent the same file as where
  //! the evaluation was started
  void traverseWithinFile(SgNode *node, InheritedAttributeType inheritedValue);

  friend class AstCombinedTopDownProcessing<InheritedAttributeType>;
  friend class DistributedMemoryAnalysisPreTraversal<InheritedAttributeType>;

protected:
  //! pure virtual function which must be implemented to compute the inherited
  //! attribute at a node
  virtual InheritedAttributeType
  evaluateInheritedAttribute(SgNode *astNode,
                             InheritedAttributeType inheritedValue) = 0;

  //! Function called at the start of the traversal, before any node is
  //! visited; override if necessary, the default implementation is a
  //! no-op.
  virtual void atTraversalStart();
  virtual void atTraversalEnd();
  // GB (06/04/2007): This is a new virtual function, a no-op by
  // default. It is called for every node, after its successors have
  // been visited, with the inherited attribute computed at this node.
  // The intention is to be able to free any memory (or other resources)
  // allocated by evaluateInheritedAttribute().
  virtual void destroyInheritedValue(SgNode *, InheritedAttributeType);

private:
  DummyAttribute
  evaluateSynthesizedAttribute(SgNode *astNode,
                               InheritedAttributeType inheritedValue,
                               SynthesizedAttributesList l);

  DummyAttribute defaultSynthesizedAttribute(InheritedAttributeType inh);
};

template <class SynthesizedAttributeType> class AstCombinedBottomUpProcessing;

template <class InheritedAttributeType>
class DistributedMemoryAnalysisPostTraversal;

template <class SynthesizedAttributeType>
/** @brief Attribute evaluator for synthesized attributes.
 *
 * This class allows computation of synthesized attributes on the AST. It
 * requires a synthesized attribute type as a template parameter and the
 * implementation of the function evaluateSynthesizedAttribute. This function is
 * invoked in post-order while the AST is traversed. It can be used for passing
 * information up the AST and for computing a synthesized attribute at each node
 * based on the results of its children in the AST.
 *
 * Initialization of synthesized attributes is necessary for values which
 * represent results of non-existent nodes (null pointers in the AST) or skipped
 * nodes (e.g. traverseWithinFile skips all nodes which do not represent the
 * same file as from where the evaluation of attributes started).
 * - Class as synthesized attribute type. The default constructor is sufficient
 * to initialize the object representing an attribute value.
 * - Primitive type as synthesized attribute type (e.g. int, bool, etc.). The
 * method defaultSynthesizedAttribute must be implemented to initialize the
 * synthesized attribute. This function is automatically called during attribute
 * evaluation whenever necessary.
 *
 * Internal: This class is derived from the SgTreeTraversal class.
 */
class AstBottomUpProcessing
    : public SgTreeTraversal<DummyAttribute, SynthesizedAttributeType> {
public:
  typedef typename SgTreeTraversal<DummyAttribute, SynthesizedAttributeType>::
      SynthesizedAttributesList SynthesizedAttributesList;

  // deprecated
  typedef SynthesizedAttributesList SubTreeSynthesizedAttributes;

  //! evaluates attributes on the entire AST
  SynthesizedAttributeType traverse(SgNode *node);

  //! evaluates attributes only at nodes which represent the same file as where
  //! the evaluation was started
  SynthesizedAttributeType traverseWithinFile(SgNode *node);

  //! evaluates attributes only at nodes which represent files which were
  //! specified on the command line (=input files).
  void traverseInputFiles(SgProject *projectNode);

  friend class AstCombinedBottomUpProcessing<SynthesizedAttributeType>;
  friend class DistributedMemoryAnalysisPostTraversal<SynthesizedAttributeType>;

protected:
  //! pure virtual function which must be implemented to compute the synthesized
  //! attribute at a node. The list of synthesized attributes consists of the
  //! synthesized attributes computed at the children node of the current node.
  //! The inherited attribute value is computed by the function
  //! evaluateInheritedAttribute at the same node and simply passed to this
  //! function. Use the typedef SynthesizedAttributeList as type for the
  //! synthesized attributes list.
  virtual SynthesizedAttributeType
  evaluateSynthesizedAttribute(SgNode *, SynthesizedAttributesList) = 0;

  //! Allows to provide a default value for a synthesized attribute of primitive
  //! type (e.g. int, bool, etc.). If a class is used as type for a synthesized
  //! attribute the default constructor of this class is sufficient and this
  //! function does not have be implemented.
  virtual SynthesizedAttributeType defaultSynthesizedAttribute();
  //! Function called at the start of the traversal, before any node is
  //! visited; override if necessary, the default implementation is a
  //! no-op.
  virtual void atTraversalStart();
  virtual void atTraversalEnd();

private:
  virtual DummyAttribute
  evaluateInheritedAttribute(SgNode *astNode, DummyAttribute inheritedValue);

  virtual SynthesizedAttributeType
  evaluateSynthesizedAttribute(SgNode *astNode, DummyAttribute inheritedValue,
                               SynthesizedAttributesList l);

  virtual SynthesizedAttributeType
  defaultSynthesizedAttribute(DummyAttribute inheritedValue);
};

// deprecated classes (provided for compatibility with existing user code - will
// be removed at some point in future)
class AstSynthesizedAttribute {};
class AstInheritedAttribute {};

/** @deprecated Use AstSynthesizedAttribute instead. (provided for compatibility
 * with existing user code - will be removed at some point in future). */
class SgSynthesizedAttribute : public AstSynthesizedAttribute {};

/** @deprecated Use AstInheritedAttribute instead. (provided for compatibility
 * with existing user code - will be removed at some point in future). */
class SgInheritedAttribute : public AstInheritedAttribute {};

/** @deprecated Use AstTopDownBottomUpProcessing instead. (provided for
 * compatibility with existing user code - will be removed at some point in
 * future). */

template <class InheritedAttributeType, class SynthesizedAttributeType>
class SgTopDownBottomUpProcessing
    : public AstTopDownBottomUpProcessing<InheritedAttributeType,
                                          SynthesizedAttributeType> {};

/** @deprecated Use AstTopDownProcessing instead. (provided for compatibility
 * with existing user code - will be removed at some point in future). */
template <class InheritedAttributeType>
class SgTopDownProcessing
    : public AstTopDownProcessing<InheritedAttributeType> {};

/** @deprecated Use AstBottomUpProcessing instead. (provided for compatibility
 * with existing user code - will be removed at some point in future). */
template <class SynthesizedAttributeType>
class SgBottomUpProcessing
    : public AstBottomUpProcessing<SynthesizedAttributeType> {};

// Original Author (AstProcessing classes): Markus Schordan
// Rewritten by: Gergo Barany
// $Id: AstProcessing.C,v 1.10 2008/01/25 02:25:48 dquinlan Exp $

// For information about the changes introduced during the rewrite, see
// the comment in AstProcessing.h

template <class InheritedAttributeType, class SynthesizedAttributeType>
void SgTreeTraversal<InheritedAttributeType, SynthesizedAttributeType>::
    setNodeSuccessors(SgNode *node, SuccessorsContainer &succContainer) {
  AstSuccessorsSelectors::selectDefaultSuccessors(node, succContainer);
}

// The default constructor of the internal tree traversal class
template <class InheritedAttributeType, class SynthesizedAttributeType>
SgTreeTraversal<InheritedAttributeType,
                SynthesizedAttributeType>::SgTreeTraversal()
    : useDefaultIndexBasedTraversal(true), traversalConstraint(false),
      fileToVisit(nullptr),
      synthesizedAttributes(new SynthesizedAttributesList()) {}

// The destructor of the internal tree traversal class
template <class InheritedAttributeType, class SynthesizedAttributeType>
SgTreeTraversal<InheritedAttributeType,
                SynthesizedAttributeType>::~SgTreeTraversal() {
  ASSERT_not_null(synthesizedAttributes);
  delete synthesizedAttributes;
  synthesizedAttributes = nullptr;
}

template <class InheritedAttributeType, class SynthesizedAttributeType>
SgTreeTraversal<InheritedAttributeType, SynthesizedAttributeType>::
    SgTreeTraversal(const SgTreeTraversal &other)
    : useDefaultIndexBasedTraversal(other.useDefaultIndexBasedTraversal),
      traversalConstraint(other.traversalConstraint),
      fileToVisit(other.fileToVisit),
      synthesizedAttributes(other.synthesizedAttributes->deepCopy()) {}

template <class InheritedAttributeType, class SynthesizedAttributeType>
const SgTreeTraversal<InheritedAttributeType, SynthesizedAttributeType> &
SgTreeTraversal<InheritedAttributeType, SynthesizedAttributeType>::operator=(
    const SgTreeTraversal &other) {
  ASSERT_not_null(synthesizedAttributes);
  SynthesizedAttributesList *copiedAttributes =
      other.synthesizedAttributes->deepCopy();
  useDefaultIndexBasedTraversal = other.useDefaultIndexBasedTraversal;
  traversalConstraint = other.traversalConstraint;
  fileToVisit = other.fileToVisit;

  delete synthesizedAttributes;
  synthesizedAttributes = copiedAttributes;

  return *this;
}

template <class InheritedAttributeType, class SynthesizedAttributeType>
void SgTreeTraversal<InheritedAttributeType, SynthesizedAttributeType>::
    set_useDefaultIndexBasedTraversal(bool val) {
  useDefaultIndexBasedTraversal = val;
}

// MS: 03/22/02ROSE/tests/nonsmoke/functional/roseTests/astProcessingTests/
// function to traverse all ASTs representing inputfiles (excluding include
// files),
template <class InheritedAttributeType, class SynthesizedAttributeType>
void SgTreeTraversal<InheritedAttributeType, SynthesizedAttributeType>::
    traverseInputFiles(SgProject *projectNode,
                       InheritedAttributeType inheritedValue,
                       t_traverseOrder travOrder) {
  const SgFilePtrList &fList = projectNode->get_fileList();

  // DQ (9/1/2008): It is observed that this prevents a SgProject from being
  // built on the generated DOT file! We might want a better design to be used
  // here or call the evaluation directly to force the handling of inherited and
  // synthesized attributes on the SgProject.  This detail effect the handling
  // of multiple files on the command line (something we want to get a global
  // perspective on if possible).
  if (SgProject::get_verbose() > 0)
    printf("Warning: The traverseInputFiles() iteration over the file list "
           "prevents the evaluation of inherited and synthesized attributes on "
           "the SgProject IR node! \n");

  for (SgFilePtrList::const_iterator fl_iter = fList.begin();
       fl_iter != fList.end(); fl_iter++) {
    ASSERT_not_null(*fl_iter);
    traverseWithinFile((*fl_iter), inheritedValue, travOrder);
  }
}

//////////////////////////////////////////////////////
//// TOP DOWN BOTTOM UP PROCESSING IMPLEMENTATION ////
//////////////////////////////////////////////////////

// MS: 04/25/02
template <class InheritedAttributeType, class SynthesizedAttributeType>
SynthesizedAttributeType
AstTopDownBottomUpProcessing<InheritedAttributeType, SynthesizedAttributeType>::
    traverse(SgNode *node, InheritedAttributeType inheritedValue) {
  // this is now explicitly marked as a pre *and* post order traversal
  return SgTreeTraversal<InheritedAttributeType,
                         SynthesizedAttributeType>::traverse(node,
                                                             inheritedValue,
                                                             preandpostorder);
}

// MS: 04/25/02
template <class InheritedAttributeType, class SynthesizedAttributeType>
SynthesizedAttributeType
AstTopDownBottomUpProcessing<InheritedAttributeType, SynthesizedAttributeType>::
    traverseWithinFile(SgNode *node, InheritedAttributeType inheritedValue) {
  // this is now explicitly marked as a pre *and* post order traversal
  return SgTreeTraversal<InheritedAttributeType, SynthesizedAttributeType>::
      traverseWithinFile(node, inheritedValue, preandpostorder);
}

////////////////////////////////////////////
//// TOP DOWN PROCESSING IMPLEMENTATION ////
////////////////////////////////////////////

template <class InheritedAttributeType>
DummyAttribute
AstTopDownProcessing<InheritedAttributeType>::evaluateSynthesizedAttribute(
    SgNode *astNode, InheritedAttributeType inheritedValue,
    typename AstTopDownProcessing<
        InheritedAttributeType>::SynthesizedAttributesList) {
  destroyInheritedValue(astNode, inheritedValue);
  return defaultDummyAttribute;
}

template <class InheritedAttributeType>
DummyAttribute
AstTopDownProcessing<InheritedAttributeType>::defaultSynthesizedAttribute(
    InheritedAttributeType) {
  return defaultDummyAttribute;
}

// MS: 04/25/02
template <class InheritedAttributeType>
void AstTopDownProcessing<InheritedAttributeType>::traverse(
    SgNode *node, InheritedAttributeType inheritedValue) {
  // "top down" is now marked as a pre *and* post order traversal because
  // there is a post order component (the call to destroyInheritedAttribute)
  SgTreeTraversal<InheritedAttributeType, DummyAttribute>::traverse(
      node, inheritedValue, preandpostorder);
}

// MS: 09/30/02
template <class InheritedAttributeType>
void AstTopDownProcessing<InheritedAttributeType>::traverseWithinFile(
    SgNode *node, InheritedAttributeType inheritedValue) {
  // "top down" is now marked as a pre *and* post order traversal because
  // there is a post order component (the call to destroyInheritedAttribute)
  SgTreeTraversal<InheritedAttributeType, DummyAttribute>::traverseWithinFile(
      node, inheritedValue, preandpostorder);
}

/////////////////////////////////////////////
//// BOTTOM UP PROCESSING IMPLEMENTATION ////
/////////////////////////////////////////////

template <class SynthesizedAttributeType>
DummyAttribute
AstBottomUpProcessing<SynthesizedAttributeType>::evaluateInheritedAttribute(
    SgNode *, DummyAttribute /*inheritedValue*/) {
  return defaultDummyAttribute;
}

// MS: 30/07/04
template <class SynthesizedAttributeType>
SynthesizedAttributeType
AstBottomUpProcessing<SynthesizedAttributeType>::defaultSynthesizedAttribute() {
  SynthesizedAttributeType s = SynthesizedAttributeType();
  return s;
}

// MS: 30/07/04
template <class SynthesizedAttributeType>
SynthesizedAttributeType
AstBottomUpProcessing<SynthesizedAttributeType>::defaultSynthesizedAttribute(
    DummyAttribute /*inheritedValue*/) {
  return defaultSynthesizedAttribute();
}

template <class SynthesizedAttributeType>
SynthesizedAttributeType
AstBottomUpProcessing<SynthesizedAttributeType>::evaluateSynthesizedAttribute(
    SgNode *astNode, DummyAttribute /*inheritedValue*/,
    SynthesizedAttributesList l) {
  return evaluateSynthesizedAttribute(astNode, l);
}

template <class SynthesizedAttributeType>
SynthesizedAttributeType
AstBottomUpProcessing<SynthesizedAttributeType>::traverse(SgNode *node) {
  return SgTreeTraversal<DummyAttribute, SynthesizedAttributeType>::traverse(
      node, defaultDummyAttribute, postorder);
}

template <class SynthesizedAttributeType>
SynthesizedAttributeType
AstBottomUpProcessing<SynthesizedAttributeType>::traverseWithinFile(
    SgNode *node) {
  return SgTreeTraversal<DummyAttribute, SynthesizedAttributeType>::
      traverseWithinFile(node, defaultDummyAttribute, postorder);
}

template <class SynthesizedAttributeType>
void AstBottomUpProcessing<SynthesizedAttributeType>::traverseInputFiles(
    SgProject *projectNode) {
  SgTreeTraversal<DummyAttribute, SynthesizedAttributeType>::traverseInputFiles(
      projectNode, defaultDummyAttribute, postorder);
}
// MS: 07/29/04
template <class InheritedAttributeType, class SynthesizedAttributeType>
SynthesizedAttributeType
SgTreeTraversal<InheritedAttributeType, SynthesizedAttributeType>::
    defaultSynthesizedAttribute(InheritedAttributeType /*inh*/) {
  // we provide 'inh' but do not use it in the constructor of 's' to allow
  // primitive types
  SynthesizedAttributeType s = SynthesizedAttributeType();
  return s;
}

// MS: 09/30/02
template <class InheritedAttributeType, class SynthesizedAttributeType>
SynthesizedAttributeType
SgTreeTraversal<InheritedAttributeType, SynthesizedAttributeType>::
    traverseWithinFile(SgNode *node, InheritedAttributeType inheritedValue,
                       t_traverseOrder treeTraversalOrder) {
  traversalConstraint = true;

  SgFile *filenode = isSgFile(node);
  if (filenode == nullptr) {
    if (node == nullptr) {
      printf(
          "Error: traverseWithinFile(): (node should be non-null) node = %p \n",
          node);
    } else {
      // DQ (4/22/2014): This will fail if the input is specified as a
      // SgProject.
      printf("Error: traverseWithinFile(): (node should be type SgFile) node = "
             "%p = %s \n",
             node, node->class_name().c_str());
    }
  }
  ASSERT_not_null(
      filenode); // this function will be extended to work with all nodes soon

  // GB (05/30/2007): changed to a SgFile* instead of a file name,
  // comparisons are much cheaper this way
  fileToVisit = filenode;

  ROSE_ASSERT(SgTreeTraversal_inFileToTraverse(node, traversalConstraint,
                                               fileToVisit) == true);

  SynthesizedAttributeType synth =
      traverse(node, inheritedValue, treeTraversalOrder);

  traversalConstraint = false;

  return synth;
}

template <class InheritedAttributeType, class SynthesizedAttributeType>
SynthesizedAttributeType
SgTreeTraversal<InheritedAttributeType, SynthesizedAttributeType>::traverse(
    SgNode *node, InheritedAttributeType inheritedValue,
    t_traverseOrder treeTraversalOrder) {
  // make sure the stack is empty
  synthesizedAttributes->resetStack();
  ROSE_ASSERT(synthesizedAttributes->debugSize() == 0);

  // notify the concrete traversal class that a traversal is starting
  atTraversalStart();

  // perform the actual traversal
  performTraversal(node, inheritedValue, treeTraversalOrder);

  // notify the traversal that we are done
  atTraversalEnd();

  // get the result off the stack
  return traversalResult();
}

template <class InheritedAttributeType, class SynthesizedAttributeType>
void SgTreeTraversal<InheritedAttributeType, SynthesizedAttributeType>::
    performTraversal(SgNode *node, InheritedAttributeType inheritedValue,
                     t_traverseOrder treeTraversalOrder) {
  if (node && SgTreeTraversal_inFileToTraverse(node, traversalConstraint,
                                               fileToVisit)) {
    // In case of a preorder traversal call the function to be applied to each
    // node of the AST GB (7/6/2007): Because AstPrePostProcessing was
    // introduced, a treeTraversalOrder can now be pre *and* post at the same
    // time! The
    // == comparison was therefore replaced by a bit mask check.
    if (treeTraversalOrder & preorder)
      inheritedValue = evaluateInheritedAttribute(node, inheritedValue);

    // Visit the traversable data members of this AST node.
    // GB (09/25/2007): Added support for index-based traversals. The
    // useDefaultIndexBasedTraversal flag tells us whether to use successor
    // containers or direct index-based access to the node's successors.
    AstSuccessorsSelectors::SuccessorsContainer succContainer;
    if (!useDefaultIndexBasedTraversal) {
      setNodeSuccessors(node, succContainer);
    } else {
      // The generated index accessor validates and materializes the complete
      // successor sequence.  Calling it once per index is quadratic for
      // high-cardinality typed owners such as SgAuxiliaryDeclarationList and
      // also repeats the same identity validation.  Materialize the exact
      // sequence once for this traversal step.
      succContainer = node->get_traversalSuccessorContainer();
    }
    const size_t numberOfSuccessors = succContainer.size();

    for (SgNode *child : succContainer) {
      if (child != nullptr) {
        performTraversal(child, inheritedValue, treeTraversalOrder);
      } else {
        // null pointer (not traversed): we put the default value(s) of
        // SynthesizedAttribute onto the stack
        if (treeTraversalOrder & postorder)
          synthesizedAttributes->push(
              defaultSynthesizedAttribute(inheritedValue));
      }
    }

    // In case of a postorder traversal call the function to be applied to each
    // node of the AST GB (7/6/2007): Because AstPrePostProcessing was
    // introduced, a treeTraversalOrder can now be pre *and* post at the same
    // time! The
    // == comparison was therefore replaced by a bit mask check.
    // The call to evaluateInheritedAttribute at this point also had to be
    // changed; it was never elegant anyway as we were not really
    // evaluating attributes here.
    if (treeTraversalOrder & postorder) {
      // Now that every child's synthesized attributes are on the stack:
      // Tell the stack how big the stack frame containing those
      // attributes is to be, and pass that frame to
      // evaluateSynthesizedAttribute(); then replace those results by
      // pushing the computed value onto the stack (which pops off the
      // previous stack frame).
      synthesizedAttributes->setFrameSize(numberOfSuccessors);
      ROSE_ASSERT(synthesizedAttributes->size() == numberOfSuccessors);
      synthesizedAttributes->push(evaluateSynthesizedAttribute(
          node, inheritedValue, *synthesizedAttributes));
    }
  } else {
    if (treeTraversalOrder & postorder)
      synthesizedAttributes->push(defaultSynthesizedAttribute(inheritedValue));
  }
} // function body

// GB (05/30/2007)
template <class InheritedAttributeType, class SynthesizedAttributeType>
SynthesizedAttributeType
SgTreeTraversal<InheritedAttributeType,
                SynthesizedAttributeType>::traversalResult() {
  // If the stack of synthesizedAttributes contains exactly one object, then
  // that one is the valid final result of the computation, so we should
  // return it. Otherwise, either there are no objects on the stack (because
  // the traversal didn't use any attributes), or there are more than one
  // (usually because the traversal exited prematurely by throwing an
  // exception); in this case, just return a default attribute.
  if (synthesizedAttributes->debugSize() == 1) {
    return synthesizedAttributes->pop();
  } else {
    static SynthesizedAttributeType sa;
    return sa;
  }
}

// GB (05/30/2007)
template <class InheritedAttributeType, class SynthesizedAttributeType>
void SgTreeTraversal<InheritedAttributeType,
                     SynthesizedAttributeType>::atTraversalStart() {}

template <class InheritedAttributeType, class SynthesizedAttributeType>
void SgTreeTraversal<InheritedAttributeType,
                     SynthesizedAttributeType>::atTraversalEnd() {}

template <class InheritedAttributeType, class SynthesizedAttributeType>
void AstTopDownBottomUpProcessing<
    InheritedAttributeType, SynthesizedAttributeType>::atTraversalStart() {}

template <class InheritedAttributeType, class SynthesizedAttributeType>
void AstTopDownBottomUpProcessing<InheritedAttributeType,
                                  SynthesizedAttributeType>::atTraversalEnd() {}

template <class InheritedAttributeType>
void AstTopDownProcessing<InheritedAttributeType>::destroyInheritedValue(
    SgNode *, InheritedAttributeType) {}

template <class InheritedAttributeType>
void AstTopDownProcessing<InheritedAttributeType>::atTraversalStart() {}

template <class InheritedAttributeType>
void AstTopDownProcessing<InheritedAttributeType>::atTraversalEnd() {}

template <class SynthesizedAttributeType>
void AstBottomUpProcessing<SynthesizedAttributeType>::atTraversalStart() {}

template <class SynthesizedAttributeType>
void AstBottomUpProcessing<SynthesizedAttributeType>::atTraversalEnd() {}

#include "AstSimpleProcessing.h" // that's a non-templated class which is put in a different file (for gcc to compile&link properly)

#include "AstCombinedProcessing.h"

#endif
