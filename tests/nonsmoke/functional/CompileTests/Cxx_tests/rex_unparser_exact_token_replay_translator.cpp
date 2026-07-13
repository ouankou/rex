#include "sage3basic.h"

#include "RoseAst.h"
#include "frontierDetection.h"
#include "tokenStreamMapping.h"
#include "unparser.h"
#include "utility_functions.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <set>
#include <sstream>
#include <string>
#include <type_traits>

static_assert(
    !std::is_constructible<TokenStreamSequenceToNodeMapping, SgNode *, int, int,
                           int, int, int, int, int, int>::value,
    "mutable inclusive token mappings must not be publicly constructible");
static_assert(
    !std::is_copy_constructible<TokenStreamSequenceToNodeMapping>::value,
    "published token mappings must not be copyable mutable state");

class TokenStreamMappingContractTestAccess {
public:
  static void rejectHalfPresentWhitespace(SgNode *node) {
    TokenStreamSequenceToNodeMapping::optionalInclusiveDraftInterval(
        node, "leading-whitespace", -1, 0);
  }

  static void rejectMissingCore(SgNode *node) {
    TokenStreamSequenceToNodeMapping::requiredInclusiveDraftInterval(
        node, "token-subsequence", -1, -1);
  }

  static TokenStreamSequenceToNodeMapping *
  publishWithoutWhitespace(SgNode *node, size_t token_count) {
    ROSE_ASSERT(node != nullptr);
    ROSE_ASSERT(token_count > 0);
    auto *mapping = new TokenStreamSequenceToNodeMapping(
        node, TokenStreamHalfOpenInterval(0, 1), std::nullopt, std::nullopt,
        std::nullopt);
    mapping->publishHalfOpenIntervals(token_count);
    return mapping;
  }

  static void republish(TokenStreamSequenceToNodeMapping *mapping,
                        size_t token_count) {
    ROSE_ASSERT(mapping != nullptr);
    mapping->publishHalfOpenIntervals(token_count);
  }

  static void mutatePublishedCore(TokenStreamSequenceToNodeMapping *mapping) {
    ROSE_ASSERT(mapping != nullptr);
    mapping->construction_.replaceTokenSubsequence(
        TokenStreamHalfOpenInterval(0, 1));
  }

  static void rejectMissingChildInterval(SgNode *parent, SgNode *child,
                                         const std::string &source_file) {
    ROSE_ASSERT(parent != nullptr);
    ROSE_ASSERT(child != nullptr);
    ROSE_ASSERT(child->get_parent() == parent);
    TokenStreamSequenceToNodeMapping::requireDirectOwnerInterval(
        child, source_file.c_str(), std::nullopt);
  }

  static void rejectEmptyNonRoot(SgNode *node) {
    auto *mapping = new TokenStreamSequenceToNodeMapping(
        node, TokenStreamHalfOpenInterval(0, 0), std::nullopt, std::nullopt,
        std::nullopt);
    mapping->publishHalfOpenIntervals(0);
  }
};

namespace {
SgSourceFile *mainSourceFile(SgProject *project) {
  ROSE_ASSERT(project != nullptr);
  for (SgFile *file : project->get_fileList()) {
    SgSourceFile *source_file = isSgSourceFile(file);
    if (source_file != nullptr && !source_file->get_isHeaderFile()) {
      return source_file;
    }
  }
  return nullptr;
}

SgTypedefDeclaration *
requireSemanticInt128Typedef(SgSourceFile *source_file,
                             SgAuxiliaryDeclarationList **owner_out) {
  ROSE_ASSERT(source_file != nullptr);
  ROSE_ASSERT(owner_out != nullptr);
  SgGlobal *global = source_file->get_globalScope();
  ROSE_ASSERT(global != nullptr);
  SgAuxiliaryDeclarationList *owner = global->get_auxiliary_declarations();
  ROSE_ASSERT(owner != nullptr);
  ROSE_ASSERT(owner->get_parent() == global);

  SgTypedefDeclaration *result = nullptr;
  for (SgDeclarationStatement *declaration : owner->get_declarations()) {
    SgTypedefDeclaration *candidate = isSgTypedefDeclaration(declaration);
    if (candidate == nullptr || candidate->get_name() != "__int128_t") {
      continue;
    }
    ROSE_ASSERT(result == nullptr);
    result = candidate;
  }
  ROSE_ASSERT(result != nullptr);
  ROSE_ASSERT(result->get_parent() == owner);
  ROSE_ASSERT(result->get_scope() == global);
  ROSE_ASSERT(std::count(owner->get_declarations().begin(),
                         owner->get_declarations().end(), result) == 1);
  ROSE_ASSERT(result->get_file_info() != nullptr);
  ROSE_ASSERT(result->get_file_info()->get_physical_file_id() < 0);
  ROSE_ASSERT(result->get_file_info()->isCompilerGenerated());
  ROSE_ASSERT(result->get_file_info()->isFrontendSpecific());
  *owner_out = owner;
  return result;
}

TokenUnparseFrontierFileContext &
runFrontierAnalysis(SgSourceFile *source_file,
                    TokenUnparseFrontierContext &frontier_context,
                    SgNode *traversal_root = nullptr) {
  TokenUnparseFrontierFileContext &file_context =
      frontier_context.beginFile(source_file);
  file_context.finishTransformationAnalysis();
  frontierDetectionForTokenStreamMapping(source_file, false, frontier_context,
                                         traversal_root);
  return file_context;
}

bool hasAuxiliaryDeclarationAncestor(SgNode *node) {
  std::set<SgNode *> visited;
  for (SgNode *cursor = node; cursor != nullptr;
       cursor = cursor->get_parent()) {
    ROSE_ASSERT(visited.insert(cursor).second);
    if (isSgAuxiliaryDeclarationList(cursor) != nullptr) {
      return true;
    }
  }
  return false;
}

void validatePublishedIntervals(SgSourceFile *source_file) {
  const int token_count =
      static_cast<int>(source_file->get_token_list().size());
  std::set<TokenStreamSequenceToNodeMapping *> visited;
  bool saw_empty_leading = false;
  bool saw_empty_trailing = false;
  for (const auto &entry : source_file->get_tokenSubsequenceMap()) {
    TokenStreamSequenceToNodeMapping *mapping = entry.second;
    ROSE_ASSERT(entry.first != nullptr);
    ROSE_ASSERT(mapping != nullptr);
    if (!visited.insert(mapping).second) {
      continue;
    }
    for (TokenStreamIntervalKind kind :
         {TokenStreamIntervalKind::leading_whitespace,
          TokenStreamIntervalKind::token_subsequence,
          TokenStreamIntervalKind::trailing_whitespace,
          TokenStreamIntervalKind::else_whitespace}) {
      const TokenStreamHalfOpenInterval &interval =
          mapping->halfOpenInterval(kind);
      ROSE_ASSERT(interval.begin >= 0);
      ROSE_ASSERT(interval.end >= interval.begin);
      ROSE_ASSERT(interval.end <= token_count);
    }
    const TokenStreamHalfOpenInterval &leading =
        mapping->halfOpenInterval(TokenStreamIntervalKind::leading_whitespace);
    const TokenStreamHalfOpenInterval &core =
        mapping->halfOpenInterval(TokenStreamIntervalKind::token_subsequence);
    const TokenStreamHalfOpenInterval &trailing =
        mapping->halfOpenInterval(TokenStreamIntervalKind::trailing_whitespace);
    const TokenStreamHalfOpenInterval &else_interval =
        mapping->halfOpenInterval(TokenStreamIntervalKind::else_whitespace);
    ROSE_ASSERT(token_count == 0 ? core.empty() : !core.empty());
    ROSE_ASSERT(leading.end == core.begin);
    ROSE_ASSERT(trailing.begin == core.end);
    if (else_interval.empty()) {
      ROSE_ASSERT(else_interval.begin == core.end);
    } else {
      ROSE_ASSERT(else_interval.begin >= core.begin);
      ROSE_ASSERT(else_interval.end <= core.end);
    }
    saw_empty_leading =
        saw_empty_leading ||
        mapping->halfOpenInterval(TokenStreamIntervalKind::leading_whitespace)
            .empty();
    saw_empty_trailing =
        saw_empty_trailing ||
        mapping->halfOpenInterval(TokenStreamIntervalKind::trailing_whitespace)
            .empty();
  }
  ROSE_ASSERT(!visited.empty());
  ROSE_ASSERT(saw_empty_leading);
  ROSE_ASSERT(saw_empty_trailing);
}

void validateSharedMacroIntervals(SgSourceFile *source_file) {
  if (Rose::utility_stripPathFromFileName(source_file->getFileName()) !=
      "rex_token_mapping_shared_macro.cpp") {
    return;
  }

  std::set<TokenStreamSequenceToNodeMapping *> visited;
  bool saw_shared_mapping = false;
  for (const auto &entry : source_file->get_tokenSubsequenceMap()) {
    TokenStreamSequenceToNodeMapping *mapping = entry.second;
    ROSE_ASSERT(mapping != nullptr);
    if (!visited.insert(mapping).second || !mapping->shared) {
      continue;
    }
    ROSE_ASSERT(mapping->nodeVector.size() > 1);
    std::set<SgNode *> associated(mapping->nodeVector.begin(),
                                  mapping->nodeVector.end());
    ROSE_ASSERT(associated.size() == mapping->nodeVector.size());
    for (SgNode *node : associated) {
      ROSE_ASSERT(node != nullptr);
    }
    saw_shared_mapping = true;
  }
  ROSE_ASSERT(saw_shared_mapping);
}

SgLocatedNode *explicitEmptyOwner(SgSourceFile *source_file,
                                  TokenStreamIntervalKind kind) {
  for (const auto &entry : source_file->get_tokenSubsequenceMap()) {
    SgLocatedNode *node = isSgLocatedNode(entry.first);
    TokenStreamSequenceToNodeMapping *mapping = entry.second;
    if (node != nullptr && mapping != nullptr &&
        mapping->halfOpenInterval(kind).empty()) {
      return node;
    }
  }
  return nullptr;
}

std::string replay(
    SgSourceFile *source_file, SgLocatedNode *node,
    UnparseLanguageIndependentConstructs::token_sequence_position_enum_type
        begin,
    UnparseLanguageIndependentConstructs::token_sequence_position_enum_type end,
    bool whitespace_only = false) {
  std::ostringstream output;
  Unparser_Opt options;
  Unparser unparser(&output, source_file->getFileName(), options);
  unparser.currentFile = source_file;
  SgUnparse_Info info;
  info.set_current_source_file(source_file);
  unparser.u_exprStmt->unparseStatementFromTokenStream(node, node, begin, end,
                                                       info, whitespace_only);
  return output.str();
}
} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  SgSourceFile *source_file = mainSourceFile(project);
  ROSE_ASSERT(source_file != nullptr);

  const char *corruption = std::getenv("REX_TEST_TOKEN_REPLAY_CORRUPTION");
  const std::string mode = corruption != nullptr ? corruption : "";
  if (mode == "frontier-auxiliary-positive" ||
      mode == "frontier-auxiliary-detached-container" ||
      mode == "frontier-auxiliary-detached-declaration") {
    SgAuxiliaryDeclarationList *auxiliary = nullptr;
    SgTypedefDeclaration *semantic_typedef =
        requireSemanticInt128Typedef(source_file, &auxiliary);

    TokenUnparseFrontierContext frontier_context;
    if (mode == "frontier-auxiliary-detached-container") {
      auxiliary->set_parent(nullptr);
      runFrontierAnalysis(source_file, frontier_context);
      ROSE_ABORT();
    }
    if (mode == "frontier-auxiliary-detached-declaration") {
      SgDeclarationStatementPtrList &declarations =
          auxiliary->get_declarations();
      const auto owned =
          std::find(declarations.begin(), declarations.end(), semantic_typedef);
      ROSE_ASSERT(owned != declarations.end());
      declarations.erase(owned);
      ROSE_ASSERT(semantic_typedef->get_parent() == auxiliary);
      runFrontierAnalysis(source_file, frontier_context, semantic_typedef);
      ROSE_ABORT();
    }

    TokenUnparseFrontierFileContext &file_context =
        runFrontierAnalysis(source_file, frontier_context);
    ROSE_ASSERT(!file_context.frontierNodes.empty());
    ROSE_ASSERT(file_context.frontierNodes.find(semantic_typedef) ==
                file_context.frontierNodes.end());
    for (const auto &entry : file_context.frontierNodes) {
      ROSE_ASSERT(entry.first != nullptr);
      ROSE_ASSERT(entry.second != nullptr);
      ROSE_ASSERT(!hasAuxiliaryDeclarationAncestor(entry.first));
    }
    return 0;
  }
  if (mode == "frontier-missing-typedef-definition") {
    SgTypedefDeclaration *target = nullptr;
    RoseAst ast(source_file);
    for (RoseAst::iterator node = ast.begin(); node != ast.end(); ++node) {
      SgTypedefDeclaration *candidate = isSgTypedefDeclaration(*node);
      if (candidate != nullptr &&
          candidate->get_name() == "rex_token_replay_plain_alias") {
        ROSE_ASSERT(target == nullptr);
        target = candidate;
      }
    }
    ROSE_ASSERT(target != nullptr);
    ROSE_ASSERT(!target->get_typedefBaseTypeContainsDefiningDeclaration());
    ROSE_ASSERT(target->get_baseTypeDefiningDeclaration() == nullptr);
    target->set_typedefBaseTypeContainsDefiningDeclaration(true);

    TokenUnparseFrontierContext frontier_context;
    runFrontierAnalysis(source_file, frontier_context);
    ROSE_ABORT();
  }
  const int token_count =
      static_cast<int>(source_file->get_token_list().size());
  if (mode == "draft-half-present") {
    TokenStreamMappingContractTestAccess::rejectHalfPresentWhitespace(
        source_file->get_globalScope());
    ROSE_ABORT();
  }
  if (mode == "draft-missing-core") {
    TokenStreamMappingContractTestAccess::rejectMissingCore(
        source_file->get_globalScope());
    ROSE_ABORT();
  }
  if (mode == "missing-child-interval" || mode == "empty-nonroot") {
    SgGlobal *global = source_file->get_globalScope();
    ROSE_ASSERT(global != nullptr);
    const SgDeclarationStatementPtrList &declarations =
        global->get_declarations();
    ROSE_ASSERT(!declarations.empty());
    SgDeclarationStatement *child = declarations.front();
    ROSE_ASSERT(child != nullptr);
    if (mode == "missing-child-interval") {
      TokenStreamMappingContractTestAccess::rejectMissingChildInterval(
          global, child, source_file->getFileName());
    } else {
      TokenStreamMappingContractTestAccess::rejectEmptyNonRoot(child);
    }
    ROSE_ABORT();
  }
  if (mode == "draft-optional-whitespace") {
    TokenStreamSequenceToNodeMapping *mapping =
        TokenStreamMappingContractTestAccess::publishWithoutWhitespace(
            source_file->get_globalScope(), token_count);
    ROSE_ASSERT(
        mapping->halfOpenInterval(TokenStreamIntervalKind::leading_whitespace)
            .empty());
    ROSE_ASSERT(
        mapping->halfOpenInterval(TokenStreamIntervalKind::trailing_whitespace)
            .empty());
    ROSE_ASSERT(
        mapping->halfOpenInterval(TokenStreamIntervalKind::else_whitespace)
            .empty());
    delete mapping;
    return 0;
  }
  if (mode == "draft-double-publication" ||
      mode == "draft-mutation-after-publication") {
    TokenStreamSequenceToNodeMapping *mapping =
        TokenStreamMappingContractTestAccess::publishWithoutWhitespace(
            source_file->get_globalScope(), token_count);
    if (mode == "draft-double-publication") {
      TokenStreamMappingContractTestAccess::republish(mapping, token_count);
    } else {
      TokenStreamMappingContractTestAccess::mutatePublishedCore(mapping);
    }
    ROSE_ABORT();
  }
  if (corruption != nullptr) {
    ROSE_ASSERT(token_count > 2);
  }
  if (mode == "producer-inverted") {
    TokenStreamSequenceToNodeMapping::createPublished(
        source_file->get_globalScope(), TokenStreamHalfOpenInterval(0, 0),
        TokenStreamHalfOpenInterval(1, 0), TokenStreamHalfOpenInterval(0, 0),
        TokenStreamHalfOpenInterval(0, 0), token_count);
    ROSE_ABORT();
  }
  if (mode == "producer-missing-core") {
    TokenStreamSequenceToNodeMapping::createPublished(
        source_file->get_globalScope(), TokenStreamHalfOpenInterval(0, 0),
        TokenStreamHalfOpenInterval(0, 0), TokenStreamHalfOpenInterval(0, 0),
        TokenStreamHalfOpenInterval(0, 0), token_count);
    ROSE_ABORT();
  }
  if (mode == "producer-nonadjacent") {
    TokenStreamSequenceToNodeMapping::createPublished(
        source_file->get_globalScope(), TokenStreamHalfOpenInterval(0, 1),
        TokenStreamHalfOpenInterval(2, 3), TokenStreamHalfOpenInterval(3, 3),
        TokenStreamHalfOpenInterval(3, 3), token_count);
    ROSE_ABORT();
  }

  validatePublishedIntervals(source_file);
  validateSharedMacroIntervals(source_file);
  if (Rose::utility_stripPathFromFileName(source_file->getFileName()) ==
      "rex_token_mapping_shared_macro.cpp") {
    return 0;
  }

  auto global_entry = source_file->get_tokenSubsequenceMap().find(
      source_file->get_globalScope());
  ROSE_ASSERT(global_entry != source_file->get_tokenSubsequenceMap().end());
  TokenStreamSequenceToNodeMapping *global_mapping = global_entry->second;
  ROSE_ASSERT(global_mapping != nullptr);
  if (token_count == 0) {
    for (TokenStreamIntervalKind kind :
         {TokenStreamIntervalKind::leading_whitespace,
          TokenStreamIntervalKind::token_subsequence,
          TokenStreamIntervalKind::trailing_whitespace,
          TokenStreamIntervalKind::else_whitespace}) {
      const TokenStreamHalfOpenInterval &interval =
          global_mapping->halfOpenInterval(kind);
      ROSE_ASSERT(interval.begin == 0 && interval.end == 0);
    }
    ROSE_ASSERT(
        replay(source_file, source_file->get_globalScope(),
               UnparseLanguageIndependentConstructs::e_leading_whitespace_start,
               UnparseLanguageIndependentConstructs::e_trailing_whitespace_end)
            .empty());
    return 0;
  }

  if (corruption != nullptr) {
    if (mode == "inverted") {
      const TokenStreamHalfOpenInterval &published_core =
          global_mapping->halfOpenInterval(
              TokenStreamIntervalKind::token_subsequence);
      TokenStreamHalfOpenInterval &corrupted_core =
          const_cast<TokenStreamHalfOpenInterval &>(published_core);
      corrupted_core.end = corrupted_core.begin - 1;
    } else if (mode == "out-of-bounds") {
      TokenStreamHalfOpenInterval &corrupted_core =
          const_cast<TokenStreamHalfOpenInterval &>(
              global_mapping->halfOpenInterval(
                  TokenStreamIntervalKind::token_subsequence));
      corrupted_core.end =
          static_cast<int>(source_file->get_token_list().size()) + 1;
    } else if (mode == "nonadjacent") {
      const TokenStreamHalfOpenInterval &core =
          global_mapping->halfOpenInterval(
              TokenStreamIntervalKind::token_subsequence);
      TokenStreamHalfOpenInterval &corrupted_leading =
          const_cast<TokenStreamHalfOpenInterval &>(
              global_mapping->halfOpenInterval(
                  TokenStreamIntervalKind::leading_whitespace));
      corrupted_leading.begin = 0;
      corrupted_leading.end = core.begin == 0 ? 1 : 0;
    } else if (mode == "non-whitespace-request") {
      // The replay call below must reject the exact interval; it must not scan
      // or filter the token text into a different interval.
    } else {
      ROSE_ABORT();
    }
  }

  if (!mode.empty()) {
    replay(source_file, source_file->get_globalScope(),
           UnparseLanguageIndependentConstructs::e_leading_whitespace_start,
           UnparseLanguageIndependentConstructs::e_trailing_whitespace_end,
           mode == "non-whitespace-request");
    ROSE_ABORT();
  }

  SgLocatedNode *empty_leading = explicitEmptyOwner(
      source_file, TokenStreamIntervalKind::leading_whitespace);
  SgLocatedNode *empty_trailing = explicitEmptyOwner(
      source_file, TokenStreamIntervalKind::trailing_whitespace);
  ROSE_ASSERT(empty_leading != nullptr);
  ROSE_ASSERT(empty_trailing != nullptr);
  ROSE_ASSERT(
      replay(source_file, empty_leading,
             UnparseLanguageIndependentConstructs::e_leading_whitespace_start,
             UnparseLanguageIndependentConstructs::e_leading_whitespace_end)
          .empty());
  ROSE_ASSERT(
      replay(source_file, empty_trailing,
             UnparseLanguageIndependentConstructs::e_trailing_whitespace_start,
             UnparseLanguageIndependentConstructs::e_trailing_whitespace_end)
          .empty());

  std::ifstream input(source_file->getFileName(), std::ios::binary);
  ROSE_ASSERT(input.good());
  const std::string expected((std::istreambuf_iterator<char>(input)),
                             std::istreambuf_iterator<char>());
  const std::string replayed =
      replay(source_file, source_file->get_globalScope(),
             UnparseLanguageIndependentConstructs::e_leading_whitespace_start,
             UnparseLanguageIndependentConstructs::e_trailing_whitespace_end);
  if (replayed != expected) {
    fprintf(stderr,
            "REX_TEST_FAILURE[token-replay-byte-exact]: expected-bytes=%zu "
            "replayed-bytes=%zu\n",
            expected.size(), replayed.size());
    ROSE_ABORT();
  }
  return 0;
}
