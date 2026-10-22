
#ifndef FRONTIER_DETECTION_HEADER
#define FRONTIER_DETECTION_HEADER

#include <map>
#include <memory>
#include <set>
#include <utility>
#include <vector>

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
  bool isInCurrentFile;
  SgNode *node;

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

  FrontierDetectionForTokenStreamMapping_InheritedAttribute();
  FrontierDetectionForTokenStreamMapping_InheritedAttribute(SgSourceFile *file,
                                                            SgNode *n);

  FrontierDetectionForTokenStreamMapping_InheritedAttribute(SgSourceFile *file);

  FrontierDetectionForTokenStreamMapping_InheritedAttribute(
      const FrontierDetectionForTokenStreamMapping_InheritedAttribute &X);

  FrontierDetectionForTokenStreamMapping_InheritedAttribute
  operator=(const FrontierDetectionForTokenStreamMapping_InheritedAttribute &X);

  bool isNodeFromCurrentFile(SgStatement *statement);
};

class FrontierNode {
  // These objects represent the frontier in the AST of where we have to unparse
  // using either the token stream or the AST.
public:
  SgStatement *node;

  bool unparseUsingTokenStream;
  bool unparseFromTheAST;

  FrontierNode(SgStatement *n, bool unparseUsingTokenStream,
               bool unparseFromTheAST);

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
  SgSourceFile *sourceFile;

  SgStatement *node;

  bool isFrontier;

  bool unparseUsingTokenStream;
  bool unparseFromTheAST;
  bool containsNodesToBeUnparsedFromTheAST;
  bool containsNodesToBeUnparsedFromTheTokenStream;

  FrontierDetectionForTokenStreamMapping_SynthesizedAttribute();

  FrontierDetectionForTokenStreamMapping_SynthesizedAttribute(
      SgNode *n, SgSourceFile *file);

  FrontierDetectionForTokenStreamMapping_SynthesizedAttribute(
      const FrontierDetectionForTokenStreamMapping_SynthesizedAttribute &X);

  FrontierDetectionForTokenStreamMapping_SynthesizedAttribute operator=(
      const FrontierDetectionForTokenStreamMapping_SynthesizedAttribute &X);
};

class FrontierDetectionForTokenStreamMapping
    : public SgTopDownBottomUpProcessing<
          FrontierDetectionForTokenStreamMapping_InheritedAttribute,
          FrontierDetectionForTokenStreamMapping_SynthesizedAttribute> {
public:
  std::map<int, std::map<SgStatement *, FrontierNode *> *> frontierNodes;

  void addFrontierNode(SgStatement *statement, FrontierNode *frontierNode);
  FrontierNode *getFrontierNode(SgStatement *statement);

  FrontierDetectionForTokenStreamMapping(
      SgSourceFile *sourceFile,
      const TokenUnparseFrontierFileContext &frontierContext);
  ~FrontierDetectionForTokenStreamMapping();

  FrontierDetectionForTokenStreamMapping_InheritedAttribute
  evaluateInheritedAttribute(
      SgNode *n, FrontierDetectionForTokenStreamMapping_InheritedAttribute
                     inheritedAttribute);

  FrontierDetectionForTokenStreamMapping_SynthesizedAttribute
  evaluateSynthesizedAttribute(
      SgNode *n,
      FrontierDetectionForTokenStreamMapping_InheritedAttribute
          inheritedAttribute,
      SubTreeSynthesizedAttributes synthesizedAttributeList);

  bool isChildNodeFromSameFileAsCurrentNode(SgNode *currentNode,
                                            SgStatement *child_statement);

private:
  SgSourceFile *sourceFile;
  const TokenUnparseFrontierFileContext &frontierContext;
};

void frontierDetectionForTokenStreamMapping(
    SgSourceFile *sourceFile, bool traverseHeaderFiles,
    TokenUnparseFrontierContext &context, SgNode *traversalRoot = nullptr);

#endif
