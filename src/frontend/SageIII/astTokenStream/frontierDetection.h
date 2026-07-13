
#ifndef FRONTIER_DETECTION_HEADER
#define FRONTIER_DETECTION_HEADER

#include <map>
#include <memory>
#include <set>
#include <utility>
#include <vector>

// The support for unparsing from the token stream is a feature in
// ROSE to provide a new level of portability for the generated code.

// This support for frontier detection on the AST distinguishes
// the subtrees that must be unparsed from the AST vs. those that
// may be unparsed from the token stream.  The solution is not to
// simplipy unparse the whole file from the token stream starting
// at the SgSourceFile, since this would not reflect transformations
// and might not reflect parts of the AST for which there is some
// lack of precise enough information in the mapping of the token
// stream to the AST  (to support this feature of unparsing from
// the token stream).

class FrontierDetectionForTokenStreamMapping_InheritedAttribute {
public:
  // Save a reference to the associated source file so that we can get the
  // filename to compare against.
  SgSourceFile *sourceFile;

  // Detect when to stop processing deeper into the AST.
  bool processChildNodes;

  bool isFrontier;

  bool unparseUsingTokenStream;
  bool unparseFromTheAST;
  // bool containsNodesToBeUnparsedFromTheAST;

  // DQ (5/11/2021): Added to support header file unparsing.
  bool isInCurrentFile;
  SgNode *node;

  // DQ (5/23/2021): Added to support C++.
  bool isPartOfTemplateInstantiation;

  // Semantic declarations have exact structural ownership through an
  // SgAuxiliaryDeclarationList, but they do not describe a lexical source
  // surface.  Keep that typed traversal role distinct from physical token
  // frontier ownership.
  bool isPartOfAuxiliaryDeclarationSubtree;

  // Function parameter and constructor-initializer lists are typed structural
  // children of one function declaration, not independent source statements.
  // This flag is true only while visiting the wrapper itself; its payload is
  // still traversed normally so a real child transformation cannot disappear.
  bool isFunctionDeclarationStructuralWrapper;

  // A non-function declaration can own a typed source-declarator scope whose
  // children mix semantic name infrastructure with source-written tag
  // surfaces.  The scope is not an independent source statement; its exact
  // declaration owner is the only lexical frontier boundary.
  bool isSourceDeclaratorStructuralWrapper;

  // The range, begin, and end declarations beneath a C++ range-for are
  // semantic lowering containers.  Their expression children remain visible
  // to transformation analysis, but the declaration shells are not independent
  // lexical statement surfaces.
  bool isRangeForSemanticDeclarationWrapper;

  // Every traversal descendant of a semantic range/begin/end declaration is
  // payload of that typed lowering container, not an independent statement
  // surface.  The enclosing SgRangeBasedForStatement remains the sole lexical
  // owner, including for the source expression nested below the range shell.
  bool isPartOfRangeForSemanticDeclarationSubtree;

  // An unbraced declaration used as a controlled substatement needs a lexical
  // scope even though no braces were written. The frontend represents that
  // scope with a typed implicit SgBasicBlock owned by the control statement.
  bool isImplicitControlFlowStructuralWrapper;

  // A declaration-form SgForInitStatement is a typed structural list whose
  // sole SgDeclarationGroupStatement child owns the source declaration
  // surface.  Keep the wrapper role explicit so frontier synthesis can
  // delegate to that one lexical owner without publishing two owners for the
  // same token interval.
  bool isForInitDeclarationGroupWrapper;

  // Omitted `for` initializer and condition fields have real separator syntax
  // but no Clang statement node.  The frontend represents each absence with
  // an exact semantic SgNullStatement child.  This role keeps those children
  // traversable without inventing another physical source surface.
  bool isSourceLessForStructuralPayload;

  // DQ (5/12/2021): Added to support header file unparsing.
  // int subtree_color_index;
  // std::string subtree_color;

  // DQ (12/1/2013): Support specific restrictions in where frontiers can be
  // placed. For now: avoid class declarations in typedefs using a mixture of
  // unparsing from tokens and unparsing from the AST. bool
  // isPartOfTypedefDeclaration; For now: avoid unparing the SgIfStmt from the
  // AST and the conditional expression/statement from the token stream. bool
  // isPartOfConditionalStatement;

  // Specific constructors are required
  FrontierDetectionForTokenStreamMapping_InheritedAttribute();
  // FrontierDetectionForTokenStreamMapping_InheritedAttribute( SgSourceFile*
  // file, SgNode* n, int color_index );
  FrontierDetectionForTokenStreamMapping_InheritedAttribute(SgSourceFile *file,
                                                            SgNode *n);

  // DQ (5/11/2021): This is used to start the traversal.
  FrontierDetectionForTokenStreamMapping_InheritedAttribute(SgSourceFile *file);

  FrontierDetectionForTokenStreamMapping_InheritedAttribute(
      const FrontierDetectionForTokenStreamMapping_InheritedAttribute &X);

  // DQ (5/15/2021): Added operator=() support.
  FrontierDetectionForTokenStreamMapping_InheritedAttribute
  operator=(const FrontierDetectionForTokenStreamMapping_InheritedAttribute &X);

  // DQ (5/11/2021): More appropriate function to determine when statements are
  // in the source file.
  bool isNodeFromCurrentFile(SgStatement *statement);
};

class FrontierNode {
  // These objects represent the frontier in the AST of where we have to unparse
  // using either the token stream or the AST.
public:
  SgStatement *node;

  bool unparseUsingTokenStream;
  bool unparseFromTheAST;

  // FrontierNode(SgStatement* n,bool unparseUsingTokenStream,bool
  // unparseFromTheAST) : node(node),
  // unparseUsingTokenStream(unparseUsingTokenStream),
  // unparseFromTheAST(unparseFromTheAST)
  FrontierNode(SgStatement *n, bool unparseUsingTokenStream,
               bool unparseFromTheAST);

  std::string display();

  FrontierNode(const FrontierNode &X);
  FrontierNode operator=(const FrontierNode &X);
};

struct TokenUnparseFrontierFileContext {
  std::map<SgStatement *, FrontierNode *> frontierNodes;
  std::map<SgNode *, std::pair<SgNode *, SgNode *>> frontierAdjacency;
  std::set<SgStatement *> statementsToUnparseFromAst;
  std::set<SgStatement *> statementsContainingAstUnparse;
  std::vector<std::unique_ptr<FrontierNode>> ownedFrontierNodes;
  bool transformationAnalysisComplete = false;

  void adoptFrontierNode(SgStatement *statement, FrontierNode *frontierNode);
  void markStatementForAstUnparse(SgStatement *statement);
  void markStatementAsContainingAstUnparse(SgStatement *statement);
  bool isStatementMarkedForAstUnparse(SgStatement *statement) const;
  bool statementContainsAstUnparse(SgStatement *statement) const;
  bool statementRequiresAstUnparse(SgStatement *statement) const;
  void finishTransformationAnalysis();
};

class TokenUnparseFrontierContext {
  std::map<SgSourceFile *, TokenUnparseFrontierFileContext> files;

public:
  // Include ownership is part of one unparse invocation. Keeping this index in
  // the invocation context prevents one project from affecting another and
  // makes duplicate physical ownership an immediate contract violation.
  std::map<std::string, SgIncludeFile *> includeFilesByPath;
  std::map<std::string, std::vector<SgIncludeFile *>>
      includeFileOccurrencesByPath;
  std::map<SgIncludeFile *, std::pair<SgStatement *, SgStatement *>>
      includeFileStatementBounds;
  std::map<SgSourceFile *, std::pair<SgStatement *, SgStatement *>>
      sourceFileStatementBounds;
  std::map<SgSourceFile *, std::map<SgScopeStatement *,
                                    std::pair<SgStatement *, SgStatement *>>>
      scopeStatementBoundsBySourceFile;

  TokenUnparseFrontierContext() = default;
  TokenUnparseFrontierContext(const TokenUnparseFrontierContext &) = delete;
  TokenUnparseFrontierContext &
  operator=(const TokenUnparseFrontierContext &) = delete;

  TokenUnparseFrontierFileContext &beginFile(SgSourceFile *sourceFile);
  TokenUnparseFrontierFileContext &file(SgSourceFile *sourceFile);
  const TokenUnparseFrontierFileContext &file(SgSourceFile *sourceFile) const;
  bool hasFile(SgSourceFile *sourceFile) const;
};

class FrontierDetectionForTokenStreamMapping_SynthesizedAttribute {
public:
  // DQ (5/14/2021): I think that we need the sourceFile information to support
  // header file unparsing.
  SgSourceFile *sourceFile;

  SgStatement *node;

  bool isFrontier;

  bool unparseUsingTokenStream;
  bool unparseFromTheAST;
  bool containsNodesToBeUnparsedFromTheAST;
  bool containsNodesToBeUnparsedFromTheTokenStream;

  // std::vector<SgStatement*> frontierNodes;
  // std::vector<FrontierNode*> frontierNodes;

  FrontierDetectionForTokenStreamMapping_SynthesizedAttribute();

  // FrontierDetectionForTokenStreamMapping_SynthesizedAttribute(SgNode* n);
  FrontierDetectionForTokenStreamMapping_SynthesizedAttribute(
      SgNode *n, SgSourceFile *file);

  FrontierDetectionForTokenStreamMapping_SynthesizedAttribute(
      const FrontierDetectionForTokenStreamMapping_SynthesizedAttribute &X);

  // DQ (5/15/2021): Added operator=() support.
  FrontierDetectionForTokenStreamMapping_SynthesizedAttribute operator=(
      const FrontierDetectionForTokenStreamMapping_SynthesizedAttribute &X);
};

class FrontierDetectionForTokenStreamMapping
    : public SgTopDownBottomUpProcessing<
          FrontierDetectionForTokenStreamMapping_InheritedAttribute,
          FrontierDetectionForTokenStreamMapping_SynthesizedAttribute> {
public:
  // DQ (5/13/2021): Adding accumulator attribute.
  // int filenameToColorCodeMap;

  // DQ (5/13/2021): Using a vector is not sufficient to support the most
  // general cases (need a map). DQ (5/13/2021): Save the set of supported
  // filenames so that we can use the size of the set to distinquish the number
  // of files supported (and the color code for different header files).
  // std::vector<std::string> supportedFilesList;
  // std::map<std::string,int> filenameToColorCodeMap;

  // DQ (5/16/2021): Moved the collection of FrontierNode to here from the
  // FrontierDetectionForTokenStreamMapping_SynthesizedAttribute.
  // std::vector<FrontierNode*> frontierNodes;
  // std::map<int,std::vector<FrontierNode*> > frontierNodes;
  // std::map<int,std::map<SgStatement*,FrontierNode*> > frontierNodes;
  std::map<int, std::map<SgStatement *, FrontierNode *> *> frontierNodes;

  void addFrontierNode(SgStatement *statement, FrontierNode *frontierNode);
  FrontierNode *getFrontierNode(SgStatement *statement);
  void outputFrontierNodes();

  FrontierDetectionForTokenStreamMapping(
      SgSourceFile *sourceFile,
      const TokenUnparseFrontierFileContext &frontierContext);
  ~FrontierDetectionForTokenStreamMapping();

  // virtual function must be defined
  FrontierDetectionForTokenStreamMapping_InheritedAttribute
  evaluateInheritedAttribute(
      SgNode *n, FrontierDetectionForTokenStreamMapping_InheritedAttribute
                     inheritedAttribute);

  // virtual function must be defined
  FrontierDetectionForTokenStreamMapping_SynthesizedAttribute
  evaluateSynthesizedAttribute(
      SgNode *n,
      FrontierDetectionForTokenStreamMapping_InheritedAttribute
          inheritedAttribute,
      SubTreeSynthesizedAttributes synthesizedAttributeList);

  // DQ (5/11/2021): Added to better organize code (and support for unparsing
  // headers).
  bool isChildNodeFromSameFileAsCurrentNode(SgNode *currentNode,
                                            SgStatement *child_statement);

private:
  SgSourceFile *sourceFile;
  const TokenUnparseFrontierFileContext &frontierContext;
};

// DQ (5/10/2021): Activate this code.
void frontierDetectionForTokenStreamMapping(
    SgSourceFile *sourceFile, bool traverseHeaderFiles,
    TokenUnparseFrontierContext &context, SgNode *traversalRoot = nullptr);

#endif
