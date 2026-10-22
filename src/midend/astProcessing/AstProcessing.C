#include "sage3basic.h"

bool SgTreeTraversal_inFileToTraverse(SgNode *node, bool traversalConstraint,
                                      SgFile *fileToVisit) {
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

  bool isFrontendSpecific = ownershipInfo->isFrontendSpecific();
  bool isCompilerGeneratedOrPartOfTransformation;
  if (isFrontendSpecific) {
    isCompilerGeneratedOrPartOfTransformation = false;
  } else {
    // Include source-visible generated declarations in constrained traversals.
    bool isOutputInCodeGeneration = ownershipInfo->isOutputInCodeGeneration();
    isCompilerGeneratedOrPartOfTransformation =
        ownershipInfo->isCompilerGenerated() ||
        ownershipInfo->isTransformation() || isOutputInCodeGeneration;
  }

  // Traverse this node if it is in the file we want to visit.
  bool isRightFile = ownershipInfo->isSameFile(fileToVisit);

  // Included code nested within a scope remains traversable. Classify the
  // located owner so non-located semantic children inherit the same decision.
  SgNode *lexicalParent = ownershipNode->get_parent();
  bool isCode = lexicalParent != NULL && !isSgGlobal(lexicalParent) &&
                !isSgNamespaceDefinitionStatement(lexicalParent);

  return isCompilerGeneratedOrPartOfTransformation || isRightFile || isCode;
}
