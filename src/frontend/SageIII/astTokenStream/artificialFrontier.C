#include "sage3basic.h"

#include "tokenStreamMapping.h"

using namespace std;

ArtificialFrontier_InheritedAttribute::ArtificialFrontier_InheritedAttribute() {
  sourceFile = nullptr;

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
    SgStatement *statement = SageInterface::getEnclosingStatement(locatedNode);

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
