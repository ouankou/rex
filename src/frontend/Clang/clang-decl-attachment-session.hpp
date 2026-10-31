#ifndef ROSE_CLANG_DECL_ATTACHMENT_SESSION_HPP
#define ROSE_CLANG_DECL_ATTACHMENT_SESSION_HPP

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class SgDeclarationStatement;
class SgGlobal;
class SgNamespaceDefinitionStatement;
class SgNode;
class SgScopeStatement;
class Sg_File_Info;

class DeclAttachmentSession {
public:
  using DeclarationList = std::vector<SgDeclarationStatement *>;

  DeclAttachmentSession() = default;
  DeclAttachmentSession(const DeclAttachmentSession &) = delete;
  DeclAttachmentSession &operator=(const DeclAttachmentSession &) = delete;

  bool contains(DeclarationList *declarations, SgScopeStatement *owner,
                SgDeclarationStatement *declaration);
  bool containsExactly(DeclarationList *declarations, SgScopeStatement *owner,
                       SgDeclarationStatement *declaration,
                       const char *operation);
  bool containsExactlyOwned(DeclarationList *declarations,
                            SgNode *structural_owner,
                            SgScopeStatement *semantic_owner,
                            SgDeclarationStatement *declaration,
                            const char *operation);
  void eraseExactly(DeclarationList *declarations, SgScopeStatement *owner,
                    SgDeclarationStatement *declaration, const char *operation);
  void replaceExactly(DeclarationList *declarations, SgScopeStatement *owner,
                      SgDeclarationStatement *existing_declaration,
                      SgDeclarationStatement *replacement_declaration,
                      const char *operation);
  void recordInsertion(DeclarationList *declarations,
                       SgDeclarationStatement *declaration);
  void recordErasure(DeclarationList *declarations,
                     SgDeclarationStatement *declaration,
                     bool declaration_remains);
  void recordReplacement(DeclarationList *declarations,
                         SgDeclarationStatement *existing_declaration,
                         SgDeclarationStatement *replacement_declaration);
  void invalidate(DeclarationList *declarations);
  void retire(DeclarationList *declarations);
  void validateAll();
  void clear();
  const std::vector<SgDeclarationStatement *> *
  lookupDeclarationsOnSameSourceLine(DeclarationList *declarations,
                                     const Sg_File_Info *location);

  //! One source-spelled namespace fragment: the expanded-token occurrences of
  //! its opening and its closing brace.
  struct NamespaceFragment {
    unsigned int opening = 0;
    unsigned int closing = 0;
    SgNamespaceDefinitionStatement *definition = nullptr;

    bool operator==(const NamespaceFragment &other) const {
      return opening == other.opening && closing == other.closing &&
             definition == other.definition;
    }
  };
  //! The source-spelled fragments of one reopening chain within one
  //! translation unit, ordered by opening.  They are current only while the
  //! chain revision they were collected at is unchanged.
  struct NamespaceFragmentIndex {
    std::optional<std::uint64_t> chain_revision;
    std::vector<NamespaceFragment> fragments;
  };
  NamespaceFragmentIndex &
  namespaceFragmentIndex(SgNamespaceDefinitionStatement *chain,
                         SgGlobal *global);

private:
  struct MembershipIndex {
    SgNode *structural_owner = nullptr;
    SgScopeStatement *semantic_owner = nullptr;
    std::size_t indexed_count = 0;
    SgDeclarationStatement *last_indexed_declaration = nullptr;
    // Exact occurrence counts are the authoritative construction-time
    // cardinality index.  An unordered_set cannot distinguish a valid single
    // attachment from a malformed duplicate and forced containsExactly() to
    // rescan the complete declaration list after every append.
    std::unordered_map<SgDeclarationStatement *, std::size_t> occurrences;
  };

  std::unordered_map<DeclarationList *, MembershipIndex> p_membership_indices;

  struct SourceLineKey {
    int file_id = -1;
    int line = -1;

    bool operator==(const SourceLineKey &other) const {
      return file_id == other.file_id && line == other.line;
    }
  };

  struct SourceLineKeyHash {
    std::size_t operator()(const SourceLineKey &key) const {
      std::size_t seed = static_cast<std::size_t>(key.file_id);
      seed ^= static_cast<std::size_t>(key.line) + 0x9e3779b9 + (seed << 6) +
              (seed >> 2);
      return seed;
    }
  };

  struct SourceLineIndex {
    std::size_t indexed_count = 0;
    std::unordered_map<SourceLineKey, std::vector<SgDeclarationStatement *>,
                       SourceLineKeyHash>
        declarations_by_line;
  };

  std::unordered_map<DeclarationList *, SourceLineIndex> p_source_line_indices;

  struct NamespaceChainKey {
    SgNamespaceDefinitionStatement *chain = nullptr;
    SgGlobal *global = nullptr;

    bool operator==(const NamespaceChainKey &other) const {
      return chain == other.chain && global == other.global;
    }
  };

  struct NamespaceChainKeyHash {
    std::size_t operator()(const NamespaceChainKey &key) const {
      std::size_t seed =
          std::hash<SgNamespaceDefinitionStatement *>()(key.chain);
      seed ^= std::hash<SgGlobal *>()(key.global) + 0x9e3779b9 + (seed << 6) +
              (seed >> 2);
      return seed;
    }
  };

  std::unordered_map<NamespaceChainKey, NamespaceFragmentIndex,
                     NamespaceChainKeyHash>
      p_namespace_fragment_indices;

  static void rebuild(MembershipIndex &index, DeclarationList *declarations,
                      SgNode *structural_owner,
                      SgScopeStatement *semantic_owner);
  MembershipIndex &indexFor(DeclarationList *declarations,
                            SgNode *structural_owner,
                            SgScopeStatement *semantic_owner);
  void invalidateSourceCaches(DeclarationList *declarations);
};

#endif
