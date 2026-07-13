// tps (01/14/2010) : Switching from rose.h to sage3.
#include "sage3basic.h"

// Original Author (AstProcessing classes): Markus Schordan
// Rewritten by: Gergo Barany
// $Id: AstProcessing.C,v 1.10 2008/01/25 02:25:48 dquinlan Exp $

// For information about the changes introduced during the rewrite, see
// the comment in AstProcessing.h

// GB (05/30/2007): This was completely rewritten from the old version to
// use much more efficient comparisons now available to us; it also never
// causes visits to included files, something that happened from time to
// time with the old version.
bool SgTreeTraversal_inFileToTraverse(SgNode *node, bool traversalConstraint,
                                      SgFile *fileToVisit) {

  // If traversing without constraint, just continue.
  // if (!traversalConstraint)
  if (traversalConstraint == false) {
    return true;
  }

  // Non-located semantic nodes (for example SgTemplateParameter) inherit file
  // membership only through their exact structural owner.  Never guess a file
  // or continue after printing a diagnostic: validate every parent/traversal
  // edge until a located owner publishes the physical identity.
  SgNode *ownershipNode = node;
  std::set<SgNode *> ownershipPath;
  while (ownershipNode->get_file_info() == NULL) {
    if (!ownershipPath.insert(ownershipNode).second) {
      fprintf(stderr,
              "REX_AST_INVARIANT[traversal-physical-owner]: node=%p type=%s "
              "has a cycle in its structural ownership path\n",
              static_cast<void *>(node), node->class_name().c_str());
      ROSE_ABORT();
    }
    if (isSgProject(ownershipNode) != NULL) {
      if (ownershipNode != node) {
        fprintf(stderr,
                "REX_AST_INVARIANT[traversal-physical-owner]: node=%p "
                "type=%s reaches the project without one located owner\n",
                static_cast<void *>(node), node->class_name().c_str());
        ROSE_ABORT();
      }
      return true;
    }

    SgNode *parent = ownershipNode->get_parent();
    if (parent == NULL) {
      fprintf(stderr,
              "REX_AST_INVARIANT[traversal-physical-owner]: node=%p type=%s "
              "has no located structural owner\n",
              static_cast<void *>(node), node->class_name().c_str());
      ROSE_ABORT();
    }
    const SgNodePtrList successors = parent->get_traversalSuccessorContainer();
    const size_t traversalEdgeCount =
        std::count(successors.begin(), successors.end(), ownershipNode);
    if (traversalEdgeCount != 1) {
      size_t writtenArgumentEdges = 0;
      size_t deducedArgumentEdges = 0;
      if (SgTemplateInstantiationDecl *instantiation =
              isSgTemplateInstantiationDecl(parent)) {
        writtenArgumentEdges =
            std::count(instantiation->get_templateArguments().begin(),
                       instantiation->get_templateArguments().end(),
                       isSgTemplateArgument(ownershipNode));
        deducedArgumentEdges =
            std::count(instantiation->get_deducedTemplateArguments().begin(),
                       instantiation->get_deducedTemplateArguments().end(),
                       isSgTemplateArgument(ownershipNode));
      }
      fprintf(stderr,
              "REX_AST_INVARIANT[traversal-physical-owner]: node=%p type=%s "
              "owner=%p type=%s publishes traversal=%zu written=%zu "
              "deduced=%zu edges instead of one\n",
              static_cast<void *>(ownershipNode),
              ownershipNode->class_name().c_str(), static_cast<void *>(parent),
              parent->class_name().c_str(), traversalEdgeCount,
              writtenArgumentEdges, deducedArgumentEdges);
      ROSE_ABORT();
    }
    ownershipNode = parent;
  }
  Sg_File_Info *ownershipInfo = ownershipNode->get_file_info();
  ROSE_ASSERT(ownershipInfo != NULL);

  // Traverse compiler generated code and code generated from
  // transformations, unless it is "frontend specific" like the stuff in
  // rose_required_macros_and_functions.h.
  bool isFrontendSpecific = ownershipInfo->isFrontendSpecific();
  bool isCompilerGeneratedOrPartOfTransformation;
  if (isFrontendSpecific) {
    isCompilerGeneratedOrPartOfTransformation = false;
  } else {
    // DQ (11/14/2008): Implicitly defined functions in Fortran are not
    // marked as compiler generated (the function body is at least
    // explicit in the source file), but the function declaration IR nodes
    // is marked as coming from file == NULL_FILE and it is also marked as
    // "outputInCodeGeneration" So it should be traversed so that we can
    // see the function body and so that it can be a proper part of the
    // definition of the AST. isCompilerGeneratedOrPartOfTransformation =
    // node->get_file_info()->isCompilerGenerated() ||
    // node->get_file_info()->isTransformation();
    bool isOutputInCodeGeneration = ownershipInfo->isOutputInCodeGeneration();
    isCompilerGeneratedOrPartOfTransformation =
        ownershipInfo->isCompilerGenerated() ||
        ownershipInfo->isTransformation() || isOutputInCodeGeneration;
  }

  // Traverse this node if it is in the file we want to visit.
  bool isRightFile = ownershipInfo->isSameFile(fileToVisit);

  // This function is meant to traverse input files in the sense of not
  // visiting "header" files (a fuzzy concept). But not every #included file
  // is a header, "code" (another fuzzy concept) can also be #included; see
  // test2005_157.C for an example. We want to traverse such code, so we
  // cannot rely on just comparing files.
  // The following heuristic is therefore used: If a node is included from
  // global scope or from within a namespace definition, we guess that it is
  // a header and don't traverse it. Otherwise, we guess that it is "code"
  // and do traverse it.
  bool isCode = node->get_parent() != NULL && !isSgGlobal(node->get_parent()) &&
                !isSgNamespaceDefinitionStatement(node->get_parent());

  bool traverseNode;
  if (isCompilerGeneratedOrPartOfTransformation || isRightFile || isCode)
    traverseNode = true;
  else
    traverseNode = false;

  return traverseNode;
}
