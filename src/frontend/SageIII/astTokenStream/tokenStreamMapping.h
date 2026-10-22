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

  SgSourceFile *sourceFile;

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
  SgNode *node;

  const TokenStreamHalfOpenInterval &
  halfOpenInterval(TokenStreamIntervalKind kind) const;

  struct NodeAssociation {
    SgNode *owner;
    std::vector<SgNode *> nodes;
    bool shared;
    bool constructedInSynthesizedAttribute;
  };

  static TokenStreamSequenceToNodeMapping *
  createPublished(const NodeAssociation &association,
                  const TokenStreamHalfOpenInterval &leading_whitespace,
                  const TokenStreamHalfOpenInterval &token_subsequence,
                  const TokenStreamHalfOpenInterval &trailing_whitespace,
                  const TokenStreamHalfOpenInterval &else_whitespace,
                  size_t token_count);

  static TokenStreamSequenceToNodeMapping *createPublished(
      SgNode *node, const TokenStreamHalfOpenInterval &leading_whitespace,
      const TokenStreamHalfOpenInterval &token_subsequence,
      const TokenStreamHalfOpenInterval &trailing_whitespace,
      const TokenStreamHalfOpenInterval &else_whitespace, size_t token_count);

  bool shared;

  bool constructedInEvaluationOfSynthesizedAttribute;

  // Use a vector as a container for the associated IR nodes for this token
  // sequence when it is shared.
  std::vector<SgNode *> nodeVector;

  // Intern mappings by their source file and token interval.
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
      SgSourceFile *sourceFile, SgNode *n, size_t token_count,
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

#include "frontierDetection.h"

#include "artificialFrontier.h"

#include "simpleFrontierDetection.h"

#include "detectMacroExpansionsToBeUnparsedAsAstTransformations.h"

#include "detectMacroOrIncludeFileExpansions.h"

std::vector<stream_element *> getTokenStream(SgSourceFile *file);

void buildTokenStreamMapping(SgSourceFile *sourceFile,
                             std::vector<stream_element *> &tokenVector);
void buildTokenStreamMappingForRoot(SgSourceFile *sourceFile,
                                    SgNode *traversalRoot,
                                    std::vector<stream_element *> &tokenVector);

#endif
