
#ifndef TOKEN_STREAM_SEQUENCE_MAPPING_HEADER
#define TOKEN_STREAM_SEQUENCE_MAPPING_HEADER

#include "tokenStreamInterval.h"

#include <optional>

class TokenStreamSequenceToNodeMapping_key {
  // The purpose of this class is to support when to share the
  // TokenStreamSequenceToNodeMapping objects across multiple IR nodes of the
  // AST.  Token sequences of IR nodes in the AST that are the same (excluding
  // leading a trailing tokens subsequences) should share the same
  // TokenStreamSequenceToNodeMapping objects.

public:
  SgNode *node;
  SgNode *share_owner;
  int lower_bound, upper_bound;

  // DQ (4/21/2021): We need to include the SgSourceFile to allow header files
  // to be supported.
  SgSourceFile *sourceFile;

  // TokenStreamSequenceToNodeMapping_key(SgNode* n, int input_lower_bound, int
  // input_upper_bound);
  TokenStreamSequenceToNodeMapping_key(SgSourceFile *sourceFile, SgNode *n,
                                       int input_lower_bound,
                                       int input_upper_bound);
  TokenStreamSequenceToNodeMapping_key(
      const TokenStreamSequenceToNodeMapping_key &X);

  bool operator==(const TokenStreamSequenceToNodeMapping_key &X) const;
  bool operator<(const TokenStreamSequenceToNodeMapping_key &X) const;
};

class TokenStreamMappingConstructionAccess;
class TokenStreamMappingContractTestAccess;

class TokenStreamMappingConstructionKey {
  TokenStreamMappingConstructionKey() = default;
  friend class TokenStreamMappingConstructionAccess;
  friend class TokenStreamMappingContractTestAccess;
};

class TokenStreamSequenceToNodeMapping {
  // This is the principal data structure used in the token mapping.

  // This class is used to make the token sequence to each IR node (or nodes).
  // It is used as an element in a list to report all mapping of
  // subsequences to IR nodes in the AST.

public:
  // To allow sharing of token stream sbsequences across multiple nodes
  // we need to permit this to be a collection of SgNode's. Likely a
  // vector would be a good choice since it would preserve order.
  // Pointer to the AST IR node.
  SgNode *node;

  const TokenStreamHalfOpenInterval &
  halfOpenInterval(TokenStreamIntervalKind kind) const;

  static TokenStreamSequenceToNodeMapping *createPublished(
      SgNode *node, const TokenStreamHalfOpenInterval &leading_whitespace,
      const TokenStreamHalfOpenInterval &token_subsequence,
      const TokenStreamHalfOpenInterval &trailing_whitespace,
      const TokenStreamHalfOpenInterval &else_whitespace, size_t token_count);

  // Currently some normalized parts of the ROSE AST can share the same
  // TokenStreamSequenceToNodeMapping data structure.  The best example
  // of this is the case of a variable declaration using multiple variables
  // (e.g. "int a,b,c;").  This will currently be normalized to be three
  // separate variable declarations (though this will be fixed in the future).
  // Since each of the variable declarations will have the same source
  // position in the generated AST, the same token sequence will map to
  // each of the separate (normalized) variable declarations.  Thus the
  // TokenStreamSequenceToNodeMapping can be shared all of the separate
  // variable declarations.  Now that we have a container of SgNodes,
  // this boolean should be true iff the container has more than 1 node.
  bool shared;

  bool constructedInEvaluationOfSynthesizedAttribute;

  // Use a vector as a container for the associated IR nodes for this token
  // sequence when it is shared.
  std::vector<SgNode *> nodeVector;

  // Static date for generating unique keys into the tokenSequencePool
  static size_t tokenStreamSize;

  // A map of unique subsequences (intervals). An interval map could
  // also work here. static
  // map<size_t,TokenStreamSequenceToNodeMapping*> tokenSequencePool;
  // static
  // map<size_t,TokenStreamSequenceToNodeMapping*,TokenStreamSequenceToNodeMapping_key>
  // tokenSequencePool;
  static std::map<TokenStreamSequenceToNodeMapping_key,
                  TokenStreamSequenceToNodeMapping *>
      tokenSequencePool;

private:
  class TokenMappingDraft {
  public:
    TokenMappingDraft(
        const TokenStreamHalfOpenInterval &token_subsequence,
        std::optional<TokenStreamHalfOpenInterval> leading_whitespace,
        std::optional<TokenStreamHalfOpenInterval> trailing_whitespace,
        std::optional<TokenStreamHalfOpenInterval> else_whitespace);

    const TokenStreamHalfOpenInterval &tokenSubsequence() const;
    const std::optional<TokenStreamHalfOpenInterval> &leadingWhitespace() const;
    const std::optional<TokenStreamHalfOpenInterval> &
    trailingWhitespace() const;
    const std::optional<TokenStreamHalfOpenInterval> &elseWhitespace() const;

    void replaceTokenSubsequence(
        const TokenStreamHalfOpenInterval &token_subsequence);
    void replaceLeadingWhitespace(
        std::optional<TokenStreamHalfOpenInterval> leading_whitespace);
    void replaceTrailingWhitespace(
        std::optional<TokenStreamHalfOpenInterval> trailing_whitespace);
    void replaceElseWhitespace(
        std::optional<TokenStreamHalfOpenInterval> else_whitespace);

    bool active() const;
    void finish();

  private:
    TokenStreamHalfOpenInterval token_subsequence_;
    std::optional<TokenStreamHalfOpenInterval> leading_whitespace_;
    std::optional<TokenStreamHalfOpenInterval> trailing_whitespace_;
    std::optional<TokenStreamHalfOpenInterval> else_whitespace_;
    bool active_ = true;
  };

  TokenMappingDraft construction_;
  TokenStreamHalfOpenInterval leading_whitespace_interval_;
  TokenStreamHalfOpenInterval token_subsequence_interval_;
  TokenStreamHalfOpenInterval trailing_whitespace_interval_;
  TokenStreamHalfOpenInterval else_whitespace_interval_;
  bool published_;

  TokenStreamSequenceToNodeMapping(
      SgNode *n, const TokenStreamHalfOpenInterval &token_subsequence,
      std::optional<TokenStreamHalfOpenInterval> leading_whitespace,
      std::optional<TokenStreamHalfOpenInterval> trailing_whitespace,
      std::optional<TokenStreamHalfOpenInterval> else_whitespace);

  TokenStreamSequenceToNodeMapping(const TokenStreamSequenceToNodeMapping &) =
      delete;
  TokenStreamSequenceToNodeMapping &
  operator=(const TokenStreamSequenceToNodeMapping &) = delete;

  // Intern token mappings by their required core interval. Construction keeps
  // optional whitespace as complete ranges and publishes all intervals once.
  static TokenStreamSequenceToNodeMapping *createTokenInterval(
      SgSourceFile *sourceFile, SgNode *n,
      const TokenStreamHalfOpenInterval &token_subsequence,
      std::optional<TokenStreamHalfOpenInterval> leading_whitespace,
      std::optional<TokenStreamHalfOpenInterval> trailing_whitespace,
      std::optional<TokenStreamHalfOpenInterval> else_whitespace);

  static TokenStreamHalfOpenInterval
  requiredInclusiveDraftInterval(SgNode *node, const char *name, int start,
                                 int inclusive_end);
  static std::optional<TokenStreamHalfOpenInterval>
  optionalInclusiveDraftInterval(SgNode *node, const char *name, int start,
                                 int inclusive_end);
  static void requireDirectOwnerInterval(
      SgNode *node, const char *source_file,
      std::optional<TokenStreamHalfOpenInterval> interval);
  static void retireDetachedMapping(TokenStreamSequenceToNodeMapping *mapping);

  void publishHalfOpenIntervals(size_t token_count);

public:
  TokenMappingDraft &constructionState(TokenStreamMappingConstructionKey);
  const TokenMappingDraft &
      constructionState(TokenStreamMappingConstructionKey) const;

private:
  friend class TokenStreamMappingConstructionAccess;
  friend class TokenStreamMappingContractTestAccess;

public:
  void display(std::string label) const;
};

// An empty token core is a first-class state only for the two published roots
// of a translation unit whose token stream is empty. Consumers use this
// predicate instead of independently weakening their nonempty-surface checks.
bool isExactEmptyTranslationUnitTokenMapping(
    SgSourceFile *sourceFile, SgStatement *statement,
    TokenStreamSequenceToNodeMapping *mapping);

// Remove the one bidirectional token association owned by a statement that is
// leaving the AST.  Generated/unmapped nodes are valid no-op inputs; a partial
// map/reverse-map association is always malformed.
void detachTokenMappingForRemovedNode(SgSourceFile *sourceFile, SgNode *node);

class Graph_TokenMappingTraversal : public AstSimpleProcessing {
public:
  // File for output for generated graph.
  static std::ofstream file;

  // The map is stored so that we can lookup the token subsequence information
  // using the SgNode pointer as a key.
  std::map<SgNode *, TokenStreamSequenceToNodeMapping *>
      &tokenStreamSequenceMap;

  // The vector is stored so that we can build the list of nodes with edges
  // (edges are missing the the token information, which might be better to
  // support there).
  std::vector<stream_element *> &tokenList;

  Graph_TokenMappingTraversal(
      std::vector<stream_element *> &input_tokenList,
      std::map<SgNode *, TokenStreamSequenceToNodeMapping *> &tokenMap);

  void visit(SgNode *n);

  // static void graph_ast_and_token_stream(SgSourceFile* file,
  // vector<stream_element*> & tokenList);
  static void graph_ast_and_token_stream(
      SgSourceFile *file, std::vector<stream_element *> &tokenList,
      std::map<SgNode *, TokenStreamSequenceToNodeMapping *>
          &tokenStreamSequenceMap);

  static void graph_ast_and_token_stream(SgSourceFile *file);

  // Map the toke_id to a string.
  static std::string getTokenIdString(int i);

  static int *first_leading_whitespace_start;
};

#include "frontierDetection.h"

// DQ (5/31/2021): Added to better support the testing mode for the token-based
// unparsing regression tests.
#include "artificialFrontier.h"

// DQ (12/4/2014): Added alternative form of detection where to switch
// between unparsing from the AST and unparsing from the token stream.
#include "simpleFrontierDetection.h"

// DQ (11/8/2015): We need a separate traversal to recognise from the
// token stream mapping, what subtrees are a part of macro expansions
// that are transformations.  These macro eexpansions must be unparsed
// as a single unit (we can't just unparse parts of them from the token
// stream and parts from the AST; because there representation in the
// token stream is only as the unexpanded macro).
#include "detectMacroExpansionsToBeUnparsedAsAstTransformations.h"

#include "detectMacroOrIncludeFileExpansions.h"

// DQ (1/7/2021): Adding function to header so that I can call it elsewhere for
// testing.
std::vector<stream_element *> getTokenStream(SgSourceFile *file);

// DQ (1/18/2021): Token stream mapping entry points.
void buildTokenStreamMappingForSourceFile(SgSourceFile *sourceFile);
void buildTokenStreamMapping(SgSourceFile *sourceFile,
                             std::vector<stream_element *> &tokenVector);
void buildTokenStreamMappingForRoot(SgSourceFile *sourceFile,
                                    SgNode *traversalRoot,
                                    std::vector<stream_element *> &tokenVector);

#endif
