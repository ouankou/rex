

#ifndef ARTIFICIAL_FRONTIER_HEADER
#define ARTIFICIAL_FRONTIER_HEADER

// DQ (5/31/2021): This is a fix for the testing support to add an articial test
// mode and mark the AST via the frontier nodes. It is better to makr the AST
// directly, and then use the mechanisms to interprete the frontier from the AST
// directly.

class ArtificialFrontier_InheritedAttribute {
public:
  // Save a reference to the associated source file so that we can get the
  // filename to compare against.
  SgSourceFile *sourceFile;

  // DQ (12/1/2013): Support specific restrictions in where frontiers can be
  // placed. For now: avoid class declarations in typedefs using a mixture of
  // unparsing from tokens and unparsing from the AST. bool
  // isPartOfTypedefDeclaration; For now: avoid unparing the SgIfStmt from the
  // AST and the conditional expression/statement from the token stream. bool
  // isPartOfConditionalStatement;

  // Specific constructors are required
  ArtificialFrontier_InheritedAttribute();

  // DQ (11/13/2018): Added constructor.
  ArtificialFrontier_InheritedAttribute(SgSourceFile *input_sourceFile);

  ArtificialFrontier_InheritedAttribute(SgSourceFile *input_sourceFile,
                                        int start, int end, bool processed);

  ArtificialFrontier_InheritedAttribute(
      const ArtificialFrontier_InheritedAttribute &X);
};

class ArtificialFrontier_SynthesizedAttribute {
public:
  SgStatement *node;

  // std::vector<SgStatement*> frontierNodes;
  // std::vector<FrontierNode*> frontierNodes;

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

  // virtual function must be defined
  ArtificialFrontier_InheritedAttribute evaluateInheritedAttribute(
      SgNode *n, ArtificialFrontier_InheritedAttribute inheritedAttribute);

  // virtual function must be defined
  ArtificialFrontier_SynthesizedAttribute evaluateSynthesizedAttribute(
      SgNode *n, ArtificialFrontier_InheritedAttribute inheritedAttribute,
      SubTreeSynthesizedAttributes synthesizedAttributeList);

private:
  TokenUnparseFrontierFileContext &frontierContext;
};

void buildArtificialFrontier(SgSourceFile *sourceFile, bool traverseHeaderFiles,
                             TokenUnparseFrontierFileContext &frontierContext);

#endif
