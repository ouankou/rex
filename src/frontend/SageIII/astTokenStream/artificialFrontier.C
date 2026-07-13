#include "sage3basic.h"

#include "tokenStreamMapping.h"

using namespace std;

ArtificialFrontier_InheritedAttribute::ArtificialFrontier_InheritedAttribute() {
  sourceFile = nullptr;

  // DQ (11/13/2018): I want to use the other constructor that will always at
  // least set the SgSourceFile pointer.
  printf("Exitng as a test! \n");
  ROSE_ABORT();
}

ArtificialFrontier_InheritedAttribute::ArtificialFrontier_InheritedAttribute(
    SgSourceFile *input_sourceFile) {
  sourceFile = input_sourceFile;
}

ArtificialFrontier_InheritedAttribute::ArtificialFrontier_InheritedAttribute(
    SgSourceFile *input_sourceFile, int /*start*/, int /*end*/,
    bool /*processed*/) {
  sourceFile = input_sourceFile;
}

ArtificialFrontier_InheritedAttribute::ArtificialFrontier_InheritedAttribute(
    const ArtificialFrontier_InheritedAttribute &X) {
  sourceFile = X.sourceFile;
}

ArtificialFrontier_SynthesizedAttribute::
    ArtificialFrontier_SynthesizedAttribute() {
  node = nullptr;
}

ArtificialFrontier_SynthesizedAttribute::
    ArtificialFrontier_SynthesizedAttribute(SgNode *n) {
  node = isSgStatement(n);
}

ArtificialFrontier_SynthesizedAttribute::
    ArtificialFrontier_SynthesizedAttribute(
        const ArtificialFrontier_SynthesizedAttribute &X) {
  node = X.node;
}

ArtificialFrontierTraversal::ArtificialFrontierTraversal(
    TokenUnparseFrontierFileContext &frontierContext)
    : frontierContext(frontierContext) {}

ArtificialFrontier_InheritedAttribute
ArtificialFrontierTraversal::evaluateInheritedAttribute(
    SgNode *n, ArtificialFrontier_InheritedAttribute inheritedAttribute) {
  ASSERT_not_null(inheritedAttribute.sourceFile);
  ArtificialFrontier_InheritedAttribute returnAttribute(
      inheritedAttribute.sourceFile);

  if (isSgGlobal(n) != nullptr) {
    SgGlobal *globalScope = isSgGlobal(n);
    ASSERT_not_null(globalScope->get_parent());
  }

  SgLocatedNode *locatedNode = isSgLocatedNode(n);
  if (locatedNode != nullptr) {
    // DQ (4/14/2015): We need to detect modified IR nodes and then set there
    // coresponding parent statement as being transformed.
    SgStatement *statement = SageInterface::getEnclosingStatement(locatedNode);

    // DQ (4/16/2015): I want to verify that we have not returned a statement at
    // a higher position in the AST than the locatedNode if it was a
    // SgStatement.
    if (isSgStatement(locatedNode) != nullptr) {
      ASSERT_require(statement == locatedNode);
    }
    ASSERT_require(isSgStatement(locatedNode) == nullptr ||
                   statement == locatedNode);
  }

  return returnAttribute;
}

ArtificialFrontier_SynthesizedAttribute
ArtificialFrontierTraversal::evaluateSynthesizedAttribute(
    SgNode *n, ArtificialFrontier_InheritedAttribute inheritedAttribute,
    SubTreeSynthesizedAttributes synthesizedAttributeList) {
  // DQ (4/14/2015): This function does not appear to do anything, because the
  // pointers to the attributes in the synthesizedAttributeList are always NULL.

  // The goal of this function is to identify the node ranges in the frontier
  // that are associated with tokens stream unparsing, and AST node unparsing.
  // There ranges are saved and concatinated as we proceed in the evaluation of
  // the synthesized attributes up the AST.

  // We want to generate a IR node range in each node which contains children so
  // that we can concatinate the lists across the whole AST and define the
  // frontier in terms of IR nodes which will then be converted into token
  // ranges to be unparsed and specific IR nodes to be unparsed from the AST
  // directly.

  ASSERT_not_null(n);

  ArtificialFrontier_SynthesizedAttribute returnAttribute(n);

  SgStatement *statement = isSgStatement(n);
  if (statement != nullptr) {
    SgBasicBlock *basicBlockParent = isSgBasicBlock(statement->get_parent());
    SgExprStatement *exprStatement = isSgExprStatement(statement);
    if (exprStatement != nullptr && basicBlockParent != nullptr) {
      Sg_File_Info *fileInfo = statement->get_file_info();
      if (fileInfo == nullptr) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[artificial-frontier]: statement=%p "
                "type=%s has no file information\n",
                static_cast<void *>(statement),
                statement->class_name().c_str());
        ROSE_ABORT();
      }
      if (!statement->isOutputInCodeGeneration()) {
        if (fileInfo->isCompilerGenerated() || fileInfo->isFrontendSpecific()) {
          return returnAttribute;
        }
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[artificial-frontier]: source "
                "statement=%p type=%s is suppressed from code generation\n",
                static_cast<void *>(statement),
                statement->class_name().c_str());
        ROSE_ABORT();
      }
      frontierContext.markStatementForAstUnparse(statement);
    }
  }

  return returnAttribute;
}

void buildArtificialFrontier(SgSourceFile *sourceFile, bool traverseHeaderFiles,
                             TokenUnparseFrontierFileContext &frontierContext) {
  TimingPerformance timer(
      "AST Build Artificial Frontier For Testing Token Stream Mapping:");

  // DQ (11/8/2015): This function sets the nodes as containing transforamtions
  // (which is essential). DQ (4/14/2015): After an more detailed evaluation of
  // this function it does not acomplish it's objectives.

  // This frontier detection happens before we associate token subsequences to
  // the AST (in a separate map).
  ASSERT_not_null(sourceFile);
  ArtificialFrontier_InheritedAttribute inheritedAttribute(sourceFile);
  ArtificialFrontierTraversal fdTraversal(frontierContext);

  if (traverseHeaderFiles == true) {
    fdTraversal.traverse(sourceFile, inheritedAttribute);
  } else {
    fdTraversal.traverseWithinFile(sourceFile, inheritedAttribute);
  }
}
