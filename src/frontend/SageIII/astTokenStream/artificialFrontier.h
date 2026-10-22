

#ifndef ARTIFICIAL_FRONTIER_HEADER
#define ARTIFICIAL_FRONTIER_HEADER

class ArtificialFrontier_InheritedAttribute {
public:
  // Save a reference to the associated source file so that we can get the
  // filename to compare against.
  SgSourceFile *sourceFile;

  ArtificialFrontier_InheritedAttribute();

  ArtificialFrontier_InheritedAttribute(SgSourceFile *input_sourceFile);

  ArtificialFrontier_InheritedAttribute(SgSourceFile *input_sourceFile,
                                        int start, int end, bool processed);

  ArtificialFrontier_InheritedAttribute(
      const ArtificialFrontier_InheritedAttribute &X);
};

class ArtificialFrontier_SynthesizedAttribute {
public:
  SgStatement *node;

  ArtificialFrontier_SynthesizedAttribute();
  ArtificialFrontier_SynthesizedAttribute(SgNode *n);

  ArtificialFrontier_SynthesizedAttribute(
      const ArtificialFrontier_SynthesizedAttribute &X);
};

class ArtificialFrontierTraversal
    : public SgTopDownBottomUpProcessing<
          ArtificialFrontier_InheritedAttribute,
          ArtificialFrontier_SynthesizedAttribute> {
public:
  explicit ArtificialFrontierTraversal(
      TokenUnparseFrontierFileContext &frontierContext);

  ArtificialFrontier_InheritedAttribute evaluateInheritedAttribute(
      SgNode *n, ArtificialFrontier_InheritedAttribute inheritedAttribute);

  ArtificialFrontier_SynthesizedAttribute evaluateSynthesizedAttribute(
      SgNode *n, ArtificialFrontier_InheritedAttribute inheritedAttribute,
      SubTreeSynthesizedAttributes synthesizedAttributeList);

private:
  TokenUnparseFrontierFileContext &frontierContext;
};

void buildArtificialFrontier(SgSourceFile *sourceFile, bool traverseHeaderFiles,
                             TokenUnparseFrontierFileContext &frontierContext);

#endif
