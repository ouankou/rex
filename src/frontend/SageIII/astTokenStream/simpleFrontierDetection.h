

#ifndef SIMPLE_FRONTIER_DETECTION_HEADER
#define SIMPLE_FRONTIER_DETECTION_HEADER

#include <unordered_map>

class SimpleFrontierDetectionForTokenStreamMapping_InheritedAttribute {
public:
  // Save a reference to the associated source file so that we can get the
  // filename to compare against.
  SgSourceFile *sourceFile;

  // Detect when to stop processing deeper into the AST.
  bool processChildNodes;

  bool isFrontier;

  bool unparseUsingTokenStream;
  bool unparseFromTheAST;
  bool containsNodesToBeUnparsedFromTheAST;

  SimpleFrontierDetectionForTokenStreamMapping_InheritedAttribute();

  SimpleFrontierDetectionForTokenStreamMapping_InheritedAttribute(
      SgSourceFile *input_sourceFile);

  SimpleFrontierDetectionForTokenStreamMapping_InheritedAttribute(
      SgSourceFile *input_sourceFile, int start, int end, bool processed);

  SimpleFrontierDetectionForTokenStreamMapping_InheritedAttribute(
      const SimpleFrontierDetectionForTokenStreamMapping_InheritedAttribute &X);
  SimpleFrontierDetectionForTokenStreamMapping_InheritedAttribute &operator=(
      const SimpleFrontierDetectionForTokenStreamMapping_InheritedAttribute &) =
      default;
};

class SimpleFrontierDetectionForTokenStreamMapping_SynthesizedAttribute {
public:
  SgStatement *node;

  bool isFrontier;

  bool unparseUsingTokenStream;
  bool unparseFromTheAST;
  bool containsNodesToBeUnparsedFromTheAST;
  bool containsNodesToBeUnparsedFromTheTokenStream;

  SimpleFrontierDetectionForTokenStreamMapping_SynthesizedAttribute();
  SimpleFrontierDetectionForTokenStreamMapping_SynthesizedAttribute(SgNode *n);

  SimpleFrontierDetectionForTokenStreamMapping_SynthesizedAttribute(
      const SimpleFrontierDetectionForTokenStreamMapping_SynthesizedAttribute
          &X);
  SimpleFrontierDetectionForTokenStreamMapping_SynthesizedAttribute &operator=(
      const SimpleFrontierDetectionForTokenStreamMapping_SynthesizedAttribute
          &) = default;
};

class SimpleFrontierDetectionForTokenStreamMapping
    : public SgTopDownBottomUpProcessing<
          SimpleFrontierDetectionForTokenStreamMapping_InheritedAttribute,
          SimpleFrontierDetectionForTokenStreamMapping_SynthesizedAttribute> {
public:
  explicit SimpleFrontierDetectionForTokenStreamMapping(
      TokenUnparseFrontierFileContext &frontierContext);

  SimpleFrontierDetectionForTokenStreamMapping_InheritedAttribute
  evaluateInheritedAttribute(
      SgNode *n, SimpleFrontierDetectionForTokenStreamMapping_InheritedAttribute
                     inheritedAttribute) override;

  SimpleFrontierDetectionForTokenStreamMapping_SynthesizedAttribute
  evaluateSynthesizedAttribute(
      SgNode *n,
      SimpleFrontierDetectionForTokenStreamMapping_InheritedAttribute
          inheritedAttribute,
      SubTreeSynthesizedAttributes synthesizedAttributeList) override;

private:
  void atTraversalStart() override;
  void atTraversalEnd() override;

  TokenUnparseFrontierFileContext &frontierContext;
  // Frontier traversal changes output flags, not structural ownership. These
  // checked facts live only for this traversal and never become AST metadata.
  std::unordered_map<SgNode *, bool> semanticAuxiliaryClassification;
  std::unordered_map<SgNode *, std::unordered_map<SgNode *, size_t>>
      structuralChildCounts;
};

void simpleFrontierDetectionForTokenStreamMapping(
    SgSourceFile *sourceFile, bool traverseHeaderFiles,
    TokenUnparseFrontierFileContext &frontierContext,
    SgNode *traversalRoot = nullptr);

#endif
