#ifndef ROSE_CLANG_DECL_ATTACHMENT_SESSION_HPP
#define ROSE_CLANG_DECL_ATTACHMENT_SESSION_HPP

#include <cstddef>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class SgDeclarationStatement;
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
  void clear();
  const std::vector<SgDeclarationStatement *> *
  lookupDeclarationsOnSameSourceLine(DeclarationList *declarations,
                                     const Sg_File_Info *location);

private:
  struct MembershipIndex {
    SgScopeStatement *owner = nullptr;
    std::size_t indexed_count = 0;
    SgDeclarationStatement *last_indexed_declaration = nullptr;
    std::unordered_set<SgDeclarationStatement *> members;
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

  static void rebuild(MembershipIndex &index, DeclarationList *declarations,
                      SgScopeStatement *owner);
  MembershipIndex &indexFor(DeclarationList *declarations,
                            SgScopeStatement *owner);
  void invalidateSourceCaches(DeclarationList *declarations);
};

#endif
