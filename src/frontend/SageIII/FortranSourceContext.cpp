#include "FortranSourceContext.h"
#include "../Flang/lexical/SourceWhitespace.h"
#include "sage3basic.h"

#include <algorithm>
#include <cctype>
#include <exception>
#include <filesystem>
#include <iterator>
#include <map>
#include <stdexcept>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>

namespace Rose::FortranSource {
namespace {
using Kind = SgSourceFile::fortran_include_kind;
using Surface = SgSourceFile::fortran_output_surface;
using Role = PreprocessingInfo::FortranSourceRole;
using Inclusion = SgSourceFile::fortran_source_inclusion;
using Key = std::tuple<unsigned, unsigned, Kind>;

[[noreturn]] void fail(const std::string &message) {
  throw std::runtime_error("Fortran source context: " + message);
}

bool isFortran(const SgSourceFile *file) {
  return file != nullptr && (file->get_Fortran_only() || file->get_F77_only() ||
                             file->get_F90_only() || file->get_F95_only() ||
                             file->get_F2003_only());
}

bool physicalPath(const std::string &path) {
  return !path.empty() && path.find('\0') == std::string::npos &&
         std::filesystem::path(path).is_absolute();
}

bool generatedInclude(const SgFortranIncludeLine *line,
                      const SgSourceFile *file) {
  if (line->get_parent() == nullptr ||
      SageInterface::getEnclosingSourceFile(
          const_cast<SgFortranIncludeLine *>(line)) != file)
    return false;
  for (const auto *point :
       {line->get_startOfConstruct(), line->get_endOfConstruct()})
    if (point == nullptr || point->get_parent() != line || point->isShared() ||
        point->get_source_sequence_number() != 0 ||
        (!point->isTransformation() && !point->isCompilerGenerated()) ||
        !point->isOutputInCodeGeneration())
      return false;
  return true;
}

bool generatedPreprocessing(const PreprocessingInfo *record,
                            const SgSourceFile *file) {
  if (record->getFortranSourceRole() != Role::none ||
      !record->isTransformation())
    return false;
  const auto *owner = record->getAttachedOwner();
  const auto *point = record->get_file_info();
  if (owner == nullptr || point == nullptr || point->get_parent() != owner ||
      point->isShared() || point->get_physical_file_id() < 0 ||
      !point->isOutputInCodeGeneration() ||
      SageInterface::getEnclosingSourceFile(
          const_cast<SgLocatedNode *>(owner)) != file ||
      (record->getOutputPlacement() !=
           PreprocessingInfo::attached_output_boundary &&
       record->getOutputPlacement() !=
           PreprocessingInfo::attached_output_trailing_line))
    fail("generated preprocessing has no exact output owner");
  for (const auto *anchor :
       {owner->get_file_info(), owner->get_startOfConstruct(),
        owner->get_endOfConstruct()})
    if (anchor == nullptr || anchor->get_parent() != owner ||
        anchor->isShared() || !anchor->isOutputInCodeGeneration() ||
        anchor->get_physical_file_id() != point->get_physical_file_id() ||
        anchor->get_physical_file_occurrence_id() !=
            point->get_physical_file_occurrence_id())
      fail("generated preprocessing contradicts its attached output owner");
  return true;
}

bool isComment(const PreprocessingInfo *record) {
  switch (record->getTypeOfDirective()) {
  case PreprocessingInfo::FortranStyleComment:
  case PreprocessingInfo::F90StyleComment:
  case PreprocessingInfo::C_StyleComment:
  case PreprocessingInfo::CplusplusStyleComment:
    return true;
  default:
    return false;
  }
}

void requireRole(const PreprocessingInfo *record, bool sourceOwned) {
  if (record->getFortranSourceRole() != Role::none && !sourceOwned)
    fail("lexical preprocessing role has no written source identity");
  switch (record->getFortranSourceRole()) {
  case Role::none:
    if (sourceOwned && !record->isTransformation())
      fail("source preprocessing record has no lexical role");
    return;
  case Role::comment:
    if (!isComment(record))
      fail("comment lexical role has incompatible preprocessing syntax");
    return;
  case Role::layout: {
    const std::string spelling = record->getString();
    for (std::size_t offset = 0; offset < spelling.size();) {
      if (const int blank =
              Rose::FortranLex::BlankBytes(spelling.c_str() + offset))
        offset += blank;
      else if (std::isspace(static_cast<unsigned char>(spelling[offset])))
        ++offset;
      else
        fail("layout lexical role contains non-whitespace source");
    }
    if (record->getTypeOfDirective() != PreprocessingInfo::CSkippedToken)
      fail("layout lexical role has incompatible preprocessing syntax");
    return;
  }
  case Role::disabled_directive:
  case Role::lexical_control:
    if (record->getTypeOfDirective() != PreprocessingInfo::CSkippedToken)
      fail("retained lexical role has incompatible preprocessing syntax");
    return;
  case Role::cpp_control:
    switch (record->getTypeOfDirective()) {
    case PreprocessingInfo::CpreprocessorIncludeDeclaration:
    case PreprocessingInfo::CpreprocessorIncludeNextDeclaration:
    case PreprocessingInfo::CpreprocessorDefineDeclaration:
    case PreprocessingInfo::CpreprocessorUndefDeclaration:
    case PreprocessingInfo::CpreprocessorIfdefDeclaration:
    case PreprocessingInfo::CpreprocessorIfndefDeclaration:
    case PreprocessingInfo::CpreprocessorIfDeclaration:
    case PreprocessingInfo::CpreprocessorDeadIfDeclaration:
    case PreprocessingInfo::CpreprocessorElseDeclaration:
    case PreprocessingInfo::CpreprocessorElifDeclaration:
    case PreprocessingInfo::CpreprocessorEndifDeclaration:
    case PreprocessingInfo::CpreprocessorLineDeclaration:
    case PreprocessingInfo::CpreprocessorPragmaDeclaration:
    case PreprocessingInfo::CpreprocessorErrorDeclaration:
    case PreprocessingInfo::CpreprocessorWarningDeclaration:
    case PreprocessingInfo::CpreprocessorEmptyDeclaration:
    case PreprocessingInfo::CpreprocessorIdentDeclaration:
    case PreprocessingInfo::CpreprocessorCompilerGeneratedLinemarker:
    case PreprocessingInfo::CpreprocessorEnd_ifDeclaration:
      return;
    default:
      fail("CPP lexical role has incompatible preprocessing syntax");
    }
  case Role::cpp_inactive:
    if (record->getTypeOfDirective() != PreprocessingInfo::CSkippedToken &&
        !isComment(record))
      fail("inactive lexical role has incompatible preprocessing syntax");
    return;
  }
  fail("preprocessing record has an invalid lexical role");
}
} // namespace

struct Context::Impl {
  struct Occurrence {
    std::string path;
    unsigned begin;
    unsigned end;
    unsigned parent;
  };
  const SgSourceFile *file;
  SgSourceFile::fortran_source_context state;
  std::unordered_map<unsigned, Occurrence> occurrences;
  std::unordered_set<std::string> physical_paths;
  std::map<Key, const Inclusion *> includes;
  std::unordered_map<unsigned, std::vector<std::pair<unsigned, unsigned>>>
      child_intervals;
  mutable std::unordered_map<const SgLocatedNode *,
                             std::unordered_set<const PreprocessingInfo *>>
      retired_owner_records;

  explicit Impl(const SgSourceFile *input) : file(input) {
    if (file == nullptr)
      fail("missing source file");
    state = file->get_fortran_source_context();
    const auto &context = state;
    if (context.output_surface != Surface::preserve_source &&
        context.output_surface != Surface::selected_translation_unit)
      fail("invalid output surface");
    if (context.root_occurrence == 0) {
      if (context.root_entry != 0 || context.root_exit != 0 ||
          !context.root_path.empty() || !context.inclusions.empty() ||
          context.output_surface != Surface::preserve_source)
        fail("incomplete root ownership");
      return;
    }
    if (!isFortran(file) || context.root_entry == 0 ||
        context.root_exit <= context.root_entry ||
        !physicalPath(context.root_path) || file->get_file_info() == nullptr ||
        file->get_file_info()->get_physical_filename() != context.root_path)
      fail("invalid root ownership");
    occurrences.emplace(context.root_occurrence,
                        Occurrence{context.root_path, context.root_entry,
                                   context.root_exit, 0});
    physical_paths.insert(context.root_path);
    unsigned previousDirective = 0;
    for (const auto &include : context.inclusions) {
      if (include.kind != Kind::native_include &&
          include.kind != Kind::cpp_include &&
          include.kind != Kind::cpp_include_next)
        fail("invalid include kind");
      if (include.parent_occurrence == 0 || include.child_occurrence == 0 ||
          include.directive_begin == 0 ||
          include.directive_begin > include.directive_end ||
          include.directive_end >= include.child_entry ||
          include.child_entry >= include.child_exit ||
          !physicalPath(include.physical_path) ||
          include.source_filename.empty() ||
          include.source_filename.find('\0') != std::string::npos ||
          include.directive_begin <= previousDirective)
        fail("invalid or unordered include boundaries");
      previousDirective = include.directive_begin;
      physical_paths.insert(include.physical_path);
      if (!occurrences
               .emplace(include.child_occurrence,
                        Occurrence{include.physical_path, include.child_entry,
                                   include.child_exit,
                                   include.parent_occurrence})
               .second ||
          !includes
               .emplace(Key{include.parent_occurrence, include.directive_begin,
                            include.kind},
                        &include)
               .second)
        fail("duplicate include occurrence or source owner");
    }
    std::unordered_map<unsigned, std::vector<const Inclusion *>> children;
    for (const auto &include : context.inclusions) {
      const auto parent = occurrences.find(include.parent_occurrence);
      if (parent == occurrences.end() ||
          parent->second.begin >= include.directive_begin ||
          include.child_exit >= parent->second.end)
        fail("include escapes its parent occurrence");
      children[include.parent_occurrence].push_back(&include);
    }
    std::unordered_map<unsigned, unsigned> visited;
    visited[context.root_occurrence] = 2;
    for (const auto &include : context.inclusions) {
      unsigned current = include.child_occurrence;
      std::vector<unsigned> path;
      while (visited[current] != 2) {
        if (visited[current] == 1)
          fail("cyclic include occurrence tree");
        visited[current] = 1;
        path.push_back(current);
        const auto found = occurrences.find(current);
        if (found == occurrences.end() || found->second.parent == 0)
          fail("disconnected include occurrence tree");
        current = found->second.parent;
      }
      for (const unsigned occurrence : path)
        visited[occurrence] = 2;
    }
    for (auto &[parent, list] : children) {
      static_cast<void>(parent);
      std::sort(list.begin(), list.end(), [](const auto *a, const auto *b) {
        return a->child_entry < b->child_entry;
      });
      for (std::size_t index = 1; index < list.size(); ++index)
        if (list[index - 1]->child_exit >= list[index]->child_entry)
          fail("sibling include occurrences overlap");
      auto &intervals = child_intervals[parent];
      for (const auto *child : list)
        intervals.emplace_back(child->child_entry, child->child_exit);
    }
  }

  bool isRootBoundaryPoint(const Sg_File_Info *point) const {
    const auto *global = file->get_globalScope();
    const auto *filePoint = file->get_file_info();
    return global != nullptr && global->get_parent() == file &&
           point != nullptr && point->get_parent() == global &&
           (point == global->get_file_info() ||
            point == global->get_startOfConstruct() ||
            point == global->get_endOfConstruct()) &&
           point->get_source_sequence_number() == 0 &&
           point->get_physical_file_occurrence_id() == 0 &&
           !point->isShared() && point->isOutputInCodeGeneration() &&
           filePoint != nullptr && filePoint->get_physical_file_id() >= 0 &&
           point->get_physical_file_id() == filePoint->get_physical_file_id() &&
           point->get_physical_filename() == state.root_path;
  }

  bool ownsPoint(const Sg_File_Info *point) const {
    if (point == nullptr)
      fail("missing source point");
    if (occurrences.empty())
      return false;
    if (isRootBoundaryPoint(point))
      return false;
    const unsigned occurrence = point->get_physical_file_occurrence_id();
    const unsigned order = point->get_source_sequence_number();
    if (order != 0 && occurrence == 0)
      fail("written source point has no exact lexical identity");
    if (occurrence == 0 || order == 0) {
      if (point->isTransformation() || point->isCompilerGenerated() ||
          point->isSourcePositionUnavailableInFrontend())
        return false;
      const auto &external = file->get_frontendExternalOwnershipPathList();
      if (std::find(external.begin(), external.end(),
                    point->get_physical_filename()) != external.end())
        return false;
      fail("written source point has no exact lexical identity");
    }
    const auto found = occurrences.find(occurrence);
    // Generated semantic aliases can retain exact written source identities.
    // get_col() hides their location; this proof uses the stored coordinates.
    if (found == occurrences.end() || point->get_physical_file_id() < 0 ||
        point->isShared() || point->get_physical_line() <= 0 ||
        point->get_raw_col() <= 0 ||
        point->get_physical_filename() != found->second.path ||
        order <= found->second.begin || order >= found->second.end)
      fail("source point escapes its exact occurrence");
    if (const auto children = child_intervals.find(occurrence);
        children != child_intervals.end()) {
      const auto next =
          std::upper_bound(children->second.begin(), children->second.end(),
                           order, [](unsigned value, const auto &interval) {
                             return value < interval.first;
                           });
      if (next != children->second.begin() && order <= std::prev(next)->second)
        fail("parent source point lies inside a child occurrence");
    }
    return true;
  }

  const Inclusion *find(const Sg_File_Info *start, Kind kind) const {
    if (!ownsPoint(start))
      fail("include owner has no written source identity");
    const auto found =
        includes.find(Key{start->get_physical_file_occurrence_id(),
                          start->get_source_sequence_number(), kind});
    return found == includes.end() ? nullptr : found->second;
  }

  bool ownsRetiredRecord(const SgLocatedNode *owner,
                         const PreprocessingInfo *record) const {
    auto cached = retired_owner_records.find(owner);
    if (cached == retired_owner_records.end()) {
      const auto *records = owner->get_attachedPreprocessingInfoPtr();
      if (records == nullptr)
        return false;
      std::unordered_set<const PreprocessingInfo *> snapshot;
      for (const auto *entry : *records)
        if (entry == nullptr || !entry->has_file_info() ||
            entry->getAttachedOwner() != owner ||
            entry->get_file_info()->get_parent() != owner ||
            !snapshot.insert(entry).second)
          fail("retired preprocessing owner has no unique attachment list");
      cached = retired_owner_records.emplace(owner, std::move(snapshot)).first;
    }
    return cached->second.count(record) == 1;
  }

  void
  validateSourcePreprocessingAnchor(const SgEmptyDeclaration *anchor) const {
    if (anchor == nullptr ||
        anchor->get_lexical_role() !=
            SgEmptyDeclaration::
                e_empty_declaration_source_preprocessing_anchor ||
        SageInterface::getEnclosingSourceFile(anchor) != file)
      fail("CPP include has no exact source preprocessing anchor");
    const auto *start = anchor->get_startOfConstruct();
    const auto *end = anchor->get_endOfConstruct();
    if (!ownsPoint(start) || !ownsPoint(end) || start->get_parent() != anchor ||
        end->get_parent() != anchor || !start->isOutputInCodeGeneration() ||
        !end->isOutputInCodeGeneration())
      fail("CPP include has no exact source preprocessing anchor");
    const auto *edge = find(start, Kind::cpp_include);
    if (edge == nullptr)
      edge = find(start, Kind::cpp_include_next);
    if (edge == nullptr ||
        end->get_physical_file_occurrence_id() != edge->parent_occurrence ||
        end->get_source_sequence_number() != edge->directive_end)
      fail("CPP include has no exact source preprocessing anchor");
    const auto *records = anchor->get_attachedPreprocessingInfoPtr();
    if (records == nullptr || records->empty())
      fail("CPP include has no exact source preprocessing anchor");
    unsigned eventOwners = 0;
    unsigned previous = 0;
    std::unordered_set<const PreprocessingInfo *> seen;
    for (const auto *record : *records) {
      if (record == nullptr || !record->has_file_info() ||
          record->getAttachedOwner() != anchor ||
          record->get_file_info()->get_parent() != anchor ||
          !seen.insert(record).second)
        fail("preprocessing record has no unique exact AST owner");
      const auto *point = record->get_file_info();
      if (!ownsPoint(point) || !point->isOutputInCodeGeneration() ||
          record->isTransformation() || point->isTransformation() ||
          point->isCompilerGenerated())
        fail("CPP anchor owns preprocessing without an exact written identity");
      requireRole(record, true);
      const unsigned order = point->get_source_sequence_number();
      if (order <= previous ||
          record->getRelativePosition() != PreprocessingInfo::before)
        fail("CPP anchor preprocessing has invalid source order");
      previous = order;
      if (order == edge->directive_begin) {
        const auto expected =
            edge->kind == Kind::cpp_include
                ? PreprocessingInfo::CpreprocessorIncludeDeclaration
                : PreprocessingInfo::CpreprocessorIncludeNextDeclaration;
        if (record->getTypeOfDirective() != expected ||
            record->getFortranSourceRole() != Role::cpp_control ||
            point->get_physical_file_occurrence_id() !=
                edge->parent_occurrence ||
            point->get_physical_file_id() != start->get_physical_file_id() ||
            point->get_physical_line() != start->get_physical_line() ||
            point->get_raw_col() != start->get_raw_col())
          fail("CPP include has no exact source preprocessing anchor");
        ++eventOwners;
      } else if (order > edge->directive_begin) {
        fail("CPP anchor preprocessing does not precede its include event");
      }
    }
    if (eventOwners != 1)
      fail("CPP include has no exact source preprocessing anchor");
  }

  void validateOwners() const {
    struct CompilerGroup {
      std::string sentinel;
      unsigned long count;
      std::unordered_set<unsigned long> members;
    };
    std::unordered_map<std::string, CompilerGroup> compilerGroups;
    std::map<Key, unsigned> owners;
    std::unordered_set<const PreprocessingInfo *> seenRecords;
    for (SgNode *node : NodeQuery::querySubTree(
             const_cast<SgSourceFile *>(file), V_SgLocatedNode)) {
      auto *located = isSgLocatedNode(node);
      for (const auto *point :
           {located->get_file_info(), located->get_startOfConstruct(),
            located->get_endOfConstruct()})
        if (point != nullptr && point->get_source_sequence_number() != 0 &&
            !ownsPoint(point))
          fail("written source point has no exact lexical identity");
      if (const auto *anchor = isSgEmptyDeclaration(located);
          anchor != nullptr &&
          anchor->get_lexical_role() ==
              SgEmptyDeclaration::
                  e_empty_declaration_source_preprocessing_anchor)
        validateSourcePreprocessingAnchor(anchor);
      if (auto *pragma = isSgPragmaDeclaration(located)) {
        validateSelectedPragma(pragma);
        if (pragma->get_fortran_directive_family() ==
                SgPragmaDeclaration::e_fortran_directive_compiler &&
            !pragma->get_fortran_directive_group_id().empty()) {
          const auto count = pragma->get_fortran_directive_member_count();
          const auto index = pragma->get_fortran_directive_member_index();
          const bool primary = pragma->get_fortran_directive_primary();
          if (count == 0 || index >= count || primary != (index == 0) ||
              pragma->get_fortran_directive_raw_text().empty() ||
              (primary
                   ? pragma->get_fortran_directive_logical_text().empty() ||
                         pragma->get_fortran_directive_semantic_text().empty()
                   : !pragma->get_fortran_directive_logical_text().empty() ||
                         !pragma->get_fortran_directive_semantic_text()
                              .empty()))
            fail("compiler directive has invalid group membership");
          auto [group, inserted] = compilerGroups.try_emplace(
              pragma->get_fortran_directive_group_id(),
              CompilerGroup{pragma->get_fortran_compiler_directive_sentinel(),
                            count,
                            {}});
          static_cast<void>(inserted);
          if (group->second.count != count ||
              group->second.sentinel !=
                  pragma->get_fortran_compiler_directive_sentinel() ||
              !group->second.members.insert(index).second)
            fail("compiler directive group has conflicting member ownership");
        }
        if (state.output_surface == Surface::selected_translation_unit &&
            pragma->get_fortran_directive_family() !=
                SgPragmaDeclaration::e_fortran_directive_none) {
          const auto *point = pragma->get_file_info();
          if (point == nullptr)
            fail("source pragma has no source position");
          if (ownsPoint(point)) {
            const auto text = pragma->get_fortran_selected_output_text();
            if (!text.has_value() ||
                (pragma->get_fortran_directive_group_id().empty()
                     ? text->empty()
                     : pragma->get_fortran_directive_primary() ==
                           text->empty()))
              fail("source pragma has no valid selected output surface");
          }
        }
      }
      if (auto *line = isSgFortranIncludeLine(located)) {
        if (!generatedInclude(line, file)) {
          if (SageInterface::getEnclosingSourceFile(line) != file)
            fail("native INCLUDE has a different structural source owner");
          const auto *edge =
              find(line->get_startOfConstruct(), Kind::native_include);
          if (edge == nullptr ||
              line->get_filename() != edge->source_filename ||
              !ownsPoint(line->get_endOfConstruct()) ||
              line->get_endOfConstruct()->get_source_sequence_number() !=
                  edge->directive_end)
            fail("native INCLUDE has no exact inclusion event");
          ++owners[Key{edge->parent_occurrence, edge->directive_begin,
                       edge->kind}];
        }
      }
      const auto *records = located->getAttachedPreprocessingInfo();
      if (records == nullptr)
        continue;
      for (const auto *record : *records) {
        if (record == nullptr || !record->has_file_info())
          fail("preprocessing source owner has no position");
        const auto *point = record->get_file_info();
        if (record->getAttachedOwner() != located ||
            point->get_parent() != located ||
            !seenRecords.insert(record).second)
          fail("preprocessing record has no unique exact AST owner");
        if (generatedPreprocessing(record, file))
          continue;
        const bool sourceOwned = ownsPoint(point);
        if (sourceOwned &&
            SageInterface::getEnclosingSourceFile(located) != file)
          fail("preprocessing record has a different structural source owner");
        requireRole(record, sourceOwned);
        Kind kind;
        if (record->getTypeOfDirective() ==
            PreprocessingInfo::CpreprocessorIncludeDeclaration)
          kind = Kind::cpp_include;
        else if (record->getTypeOfDirective() ==
                 PreprocessingInfo::CpreprocessorIncludeNextDeclaration)
          kind = Kind::cpp_include_next;
        else
          continue;
        if (!sourceOwned)
          continue;
        const auto *edge = find(point, kind);
        // A directive in an inactive CPP branch has no include event.
        if (edge == nullptr)
          continue;
        if (record->getFortranSourceRole() != Role::cpp_control)
          fail("active CPP include has an incompatible lexical role");
        const auto *anchor = isSgEmptyDeclaration(located);
        if (anchor == nullptr ||
            anchor->get_lexical_role() !=
                SgEmptyDeclaration::
                    e_empty_declaration_source_preprocessing_anchor ||
            !ownsPoint(anchor->get_startOfConstruct()) ||
            !ownsPoint(anchor->get_endOfConstruct()) ||
            anchor->get_startOfConstruct()->get_source_sequence_number() !=
                edge->directive_begin ||
            anchor->get_endOfConstruct()->get_source_sequence_number() !=
                edge->directive_end)
          fail("CPP include has no exact source preprocessing anchor");
        ++owners[Key{edge->parent_occurrence, edge->directive_begin,
                     edge->kind}];
      }
    }
    for (const auto &[key, edge] : includes) {
      static_cast<void>(edge);
      if (owners[key] != 1)
        fail("include event does not have exactly one live source owner");
    }
    for (const auto &[id, group] : compilerGroups) {
      static_cast<void>(id);
      if (group.members.size() != group.count)
        fail("compiler directive group has missing source members");
    }
  }
};

Context::Context(const SgSourceFile *file)
    : impl_(std::make_unique<Impl>(file)) {}
Context::~Context() = default;

struct Context::PendingSubtreePublication::Impl {
  struct Node {
    const SgNode *parent;
    std::size_t children;
  };
  struct Record {
    const SgLocatedNode *owner;
    std::size_t slot;
    std::size_t count;
    const Sg_File_Info *point;
    std::string text;
    std::string filename;
    int raw_file;
    int line;
    int column;
    int physical_file;
    int raw_physical_file;
    int physical_line;
    unsigned order;
    unsigned occurrence;
    unsigned classification;
    Role role;
    PreprocessingInfo::DirectiveType directive;
    PreprocessingInfo::RelativePositionType relative;
    PreprocessingInfo::OutputPlacementType placement;
    int lines;
    bool transformation;
  };
  const Context &context;
  const SgNode *root;
  const SgNode *root_parent;
  const SgLocatedNode *boundary;
  const int uncaught;
  bool completed{false};
  std::unordered_map<const SgNode *, Node> parents;
  std::unordered_map<const PreprocessingInfo *, Record> records;

  static bool excluded(const SgNode *node) {
    return isSgAuxiliaryDeclarationList(node) != nullptr ||
           isSgDeclarationScope(node) != nullptr ||
           isSgDeclarationScopeList(node) != nullptr;
  }

  Impl(const Context &input, const SgNode *subtree, const SgLocatedNode *target)
      : context(input), root(subtree),
        root_parent(subtree == nullptr ? nullptr : subtree->get_parent()),
        boundary(target), uncaught(std::uncaught_exceptions()) {
    if (root == nullptr || root == boundary ||
        !context.ownsOutputBoundary(boundary))
      fail("pending subtree has no exact attached output boundary");
    const auto *rootFile = SageInterface::getEnclosingSourceFile(root, true);
    if (rootFile != nullptr && rootFile != context.impl_->file)
      fail("pending subtree belongs to another source file");
    if (root_parent != nullptr) {
      if (rootFile != context.impl_->file)
        fail("pending subtree has an unproved external parent");
      const auto children = root_parent->get_traversalSuccessorContainer();
      const auto count = std::count(children.begin(), children.end(), root);
      if (count > 1 || (count == 0 && root_parent != boundary &&
                        root_parent != boundary->get_parent()))
        fail("pending subtree has no exact attached or staged parent");
    }
    std::vector<const SgNode *> pending{root};
    while (!pending.empty()) {
      const auto *node = pending.back();
      pending.pop_back();
      if (excluded(node))
        continue;
      auto [identity, inserted] =
          parents.emplace(node, Node{node->get_parent(), 0});
      if (!inserted)
        fail("pending subtree has duplicate structural ownership");
      if (node == boundary)
        fail("pending subtree contains its own output boundary");
      if (const auto *owner = isSgLocatedNode(node)) {
        owner->validateAttachedPreprocessingInfoOwnership();
        if (const auto *entries = owner->get_attachedPreprocessingInfoPtr()) {
          for (std::size_t index = 0; index < entries->size(); ++index) {
            const auto *entry = (*entries)[index];
            if (entry == nullptr || !entry->has_file_info() ||
                entry->getAttachedOwner() != owner ||
                entry->get_file_info()->get_parent() != owner)
              fail("pending preprocessing has no exact unique owner");
            if (entry->getFortranSourceRole() == Role::none)
              continue;
            const auto *point = entry->get_file_info();
            if (!context.ownsSourcePoint(point))
              fail("pending preprocessing has no canonical source point");
            requireRole(entry, true);
            const Record snapshot{owner,
                                  index,
                                  entries->size(),
                                  point,
                                  entry->getString(),
                                  point->get_raw_filename(),
                                  point->get_raw_file_id(),
                                  point->get_raw_line(),
                                  point->get_raw_col(),
                                  point->get_physical_file_id(),
                                  point->get_raw_physical_file_id(),
                                  point->get_physical_line(),
                                  point->get_source_sequence_number(),
                                  point->get_physical_file_occurrence_id(),
                                  point->get_classificationBitField(),
                                  entry->getFortranSourceRole(),
                                  entry->getTypeOfDirective(),
                                  entry->getRelativePosition(),
                                  entry->getOutputPlacement(),
                                  entry->getNumberOfLines(),
                                  entry->isTransformation()};
            if (!records.emplace(entry, snapshot).second)
              fail("pending preprocessing has duplicate subtree ownership");
          }
        }
      }
      for (const auto *child : node->get_traversalSuccessorContainer())
        if (child != nullptr && child->get_parent() == node &&
            !excluded(child)) {
          ++identity->second.children;
          pending.push_back(child);
        }
    }
  }

  bool contains(const PreprocessingInfo *record,
                const SgLocatedNode *target) const {
    const auto found = records.find(record);
    if (completed || target != boundary || root->get_parent() != root_parent ||
        found == records.end())
      return false;
    const auto &snapshot = found->second;
    const auto parent = parents.find(snapshot.owner);
    const auto *entries = snapshot.owner->get_attachedPreprocessingInfoPtr();
    if (parent == parents.end() ||
        snapshot.owner->get_parent() != parent->second.parent ||
        entries == nullptr || entries->size() != snapshot.count ||
        (*entries)[snapshot.slot] != record ||
        record->getAttachedOwner() != snapshot.owner ||
        !record->has_file_info() || record->get_file_info() != snapshot.point ||
        snapshot.point->get_parent() != snapshot.owner)
      return false;
    const auto *point = snapshot.point;
    const auto placement = record->getOutputPlacement();
    return record->getString() == snapshot.text &&
           point->get_raw_filename() == snapshot.filename &&
           point->get_raw_file_id() == snapshot.raw_file &&
           point->get_raw_line() == snapshot.line &&
           point->get_raw_col() == snapshot.column &&
           point->get_physical_file_id() == snapshot.physical_file &&
           point->get_raw_physical_file_id() == snapshot.raw_physical_file &&
           point->get_physical_line() == snapshot.physical_line &&
           point->get_source_sequence_number() == snapshot.order &&
           point->get_physical_file_occurrence_id() == snapshot.occurrence &&
           point->get_classificationBitField() == snapshot.classification &&
           record->getFortranSourceRole() == snapshot.role &&
           record->getTypeOfDirective() == snapshot.directive &&
           record->getRelativePosition() == snapshot.relative &&
           record->getNumberOfLines() == snapshot.lines &&
           record->isTransformation() == snapshot.transformation &&
           (placement == snapshot.placement ||
            (snapshot.placement == PreprocessingInfo::source_position &&
             placement == PreprocessingInfo::attached_output_boundary));
  }

  void complete() {
    if (completed || root->get_parent() != root_parent ||
        !context.ownsOutputBoundary(boundary) ||
        !(context.impl_->file->get_fortran_source_context() ==
          context.impl_->state))
      fail("pending subtree changed its publication phase");
    for (const auto &[node, identity] : parents) {
      if (node->get_parent() != identity.parent)
        fail("pending subtree changed a structural parent during publication");
      std::unordered_set<const SgNode *> children;
      for (const auto *child : node->get_traversalSuccessorContainer()) {
        if (child == nullptr || child->get_parent() != node || excluded(child))
          continue;
        const auto found = parents.find(child);
        if (found == parents.end() || found->second.parent != node)
          fail("pending subtree changed its owned forest during publication");
        if (!children.insert(child).second)
          fail("pending subtree duplicated an owned structural edge");
      }
      if (children.size() != identity.children)
        fail("pending subtree changed a unique owned structural edge");
    }
    for (const auto &[record, snapshot] : records) {
      static_cast<void>(snapshot);
      if (!contains(record, boundary))
        fail("pending preprocessing changed ownership or source provenance");
    }
    completed = true;
  }
};

Context::PendingSubtreePublication::PendingSubtreePublication(
    const Context &context, const SgNode *root, const SgLocatedNode *boundary)
    : impl_(std::make_unique<Impl>(context, root, boundary)) {}

Context::PendingSubtreePublication::~PendingSubtreePublication() {
  if (!impl_->completed && std::uncaught_exceptions() == impl_->uncaught) {
    fprintf(stderr,
            "REX_AST_INVARIANT[fortran-subtree-publication]: pending phase "
            "ended without completion\n");
    ROSE_ABORT();
  }
}

std::unique_ptr<Context::PendingSubtreePublication>
Context::prepareSubtreePublication(const SgNode *root,
                                   const SgLocatedNode *boundary) const {
  return std::unique_ptr<PendingSubtreePublication>(
      new PendingSubtreePublication(*this, root, boundary));
}

bool Context::PendingSubtreePublication::canAttachSourcePreprocessing(
    const PreprocessingInfo *record, const SgLocatedNode *boundary) const {
  return impl_->context.canAttachSourcePreprocessingImpl(record, boundary,
                                                         this);
}

void Context::PendingSubtreePublication::complete() { impl_->complete(); }

void validate(const SgSourceFile *file) {
  const Context context{file};
  if (!context.impl_->occurrences.empty()) {
    context.impl_->validateOwners();
    return;
  }
  if (!isFortran(file))
    return;
  auto writtenIdentity = [](const Sg_File_Info *point) {
    return point != nullptr &&
           (point->get_source_sequence_number() != 0 ||
            (!point->isTransformation() && !point->isCompilerGenerated() &&
             !point->isSourcePositionUnavailableInFrontend() &&
             point->get_physical_file_occurrence_id() != 0));
  };
  for (SgNode *node : NodeQuery::querySubTree(const_cast<SgSourceFile *>(file),
                                              V_SgLocatedNode)) {
    auto *located = isSgLocatedNode(node);
    bool claimsSource = writtenIdentity(located->get_file_info()) ||
                        writtenIdentity(located->get_startOfConstruct()) ||
                        writtenIdentity(located->get_endOfConstruct());
    if (auto *line = isSgFortranIncludeLine(located))
      claimsSource |= !generatedInclude(line, file);
    if (auto *pragma = isSgPragmaDeclaration(located))
      claimsSource |= !pragma->get_fortran_directive_group_id().empty() ||
                      !pragma->get_fortran_directive_raw_text().empty() ||
                      !pragma->get_fortran_directive_logical_text().empty() ||
                      !pragma->get_fortran_directive_semantic_text().empty();
    if (const auto *records = located->getAttachedPreprocessingInfo()) {
      for (const auto *record : *records) {
        if (record == nullptr || !record->has_file_info())
          fail("preprocessing source owner has no position");
        claimsSource |= record->getFortranSourceRole() != Role::none ||
                        (!record->isTransformation() &&
                         writtenIdentity(record->get_file_info()));
      }
    }
    if (claimsSource)
      fail("written Fortran source has no rooted context");
  }
}

void validateSelectedPragma(const SgPragmaDeclaration *pragma) {
  if (pragma == nullptr)
    fail("missing selected pragma");
  if (!SgPragmaDeclaration::fortran_selected_output_is_valid(
          pragma->get_fortran_selected_output_text(),
          pragma->get_fortran_selected_output_tokens()))
    fail("invalid selected pragma token surface");
  const auto typedFamily = pragma->get_fortran_directive_family();
  const auto &sentinel = pragma->get_fortran_compiler_directive_sentinel();
  const bool compiler =
      typedFamily == SgPragmaDeclaration::e_fortran_directive_compiler;
  if (!compiler && !sentinel.empty())
    fail("noncompiler pragma owns a compiler directive sentinel");
  if (!sentinel.empty() &&
      !std::all_of(sentinel.begin(), sentinel.end(), [](unsigned char ch) {
        return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
               ch == '$' || ch == '@';
      }))
    fail("compiler directive has an invalid registered sentinel");
  if (compiler && !pragma->get_fortran_directive_group_id().empty() &&
      sentinel.empty())
    fail("compiler directive group has no registered sentinel");
  if (compiler && pragma->get_fortran_directive_group_id().empty() &&
      (!sentinel.empty() || pragma->get_fortran_directive_member_index() != 0 ||
       pragma->get_fortran_directive_member_count() != 0 ||
       pragma->get_fortran_directive_primary() ||
       !pragma->get_fortran_directive_raw_text().empty() ||
       !pragma->get_fortran_directive_logical_text().empty() ||
       !pragma->get_fortran_directive_semantic_text().empty()))
    fail("compiler directive has invalid group membership");
  const auto &selected = pragma->get_fortran_selected_output_text();
  if (!selected)
    return;
  if (compiler &&
      (sentinel.empty() || pragma->get_fortran_directive_group_id().empty()))
    fail("selected compiler pragma has no exact sentinel owner");
  if (selected->empty())
    return;
  const auto equalPrefix = [](const std::string &text,
                              const std::string &prefix) {
    if (text.size() < prefix.size())
      return false;
    for (std::size_t at = 0; at < prefix.size(); ++at)
      if (std::tolower(static_cast<unsigned char>(text[at])) !=
          std::tolower(static_cast<unsigned char>(prefix[at])))
        return false;
    return true;
  };
  const auto first = pragma->get_fortran_selected_output_tokens().front();
  if (compiler) {
    if (first.quoted_literal_offset ||
        first.spelling.size() != sentinel.size() + 1 ||
        !equalPrefix(first.spelling, "!" + sentinel))
      fail("selected compiler pragma disagrees with its registered sentinel");
    return;
  }
  const char *family = nullptr;
  switch (typedFamily) {
  case SgPragmaDeclaration::e_fortran_directive_openmp:
    family = "omp";
    break;
  case SgPragmaDeclaration::e_fortran_directive_ompx:
    family = "ompx";
    break;
  case SgPragmaDeclaration::e_fortran_directive_openacc:
    family = "acc";
    break;
  case SgPragmaDeclaration::e_fortran_directive_cuda:
    family = "cuf";
    break;
  case SgPragmaDeclaration::e_fortran_directive_compiler:
    return;
  case SgPragmaDeclaration::e_fortran_directive_none:
    fail("selected pragma has no typed directive family");
  default:
    fail("selected pragma has an invalid typed directive family");
  }
  const auto *canonical = pragma->get_pragma();
  const std::string identity =
      canonical == nullptr ? std::string{} : canonical->get_pragma();
  const std::size_t length = std::char_traits<char>::length(family);
  if (first.quoted_literal_offset || first.spelling.size() != length ||
      !equalPrefix(first.spelling, family) || !equalPrefix(identity, family) ||
      (identity.size() > length &&
       (std::isalnum(static_cast<unsigned char>(identity[length])) ||
        identity[length] == '_')))
    fail("selected pragma disagrees with its typed directive family");
}

bool selectedOutput(const SgSourceFile *file) {
  if (file == nullptr)
    return false;
  switch (file->get_fortran_output_surface()) {
  case Surface::preserve_source:
    return false;
  case Surface::selected_translation_unit:
    return true;
  }
  fail("invalid output surface");
}

bool Context::selectedOutput() const {
  return FortranSource::selectedOutput(impl_->file);
}

bool Context::hasLexicalSource() const { return !impl_->occurrences.empty(); }

bool Context::ownsSourcePoint(const Sg_File_Info *point) const {
  return impl_->ownsPoint(point);
}

bool Context::ownsProgramUnitContainer(const SgLocatedNode *node) const {
  const SgFunctionDeclaration *declaration = isSgFunctionDeclaration(node);
  if (const auto *definition = isSgFunctionDefinition(node))
    declaration = definition->get_declaration();
  else if (const auto *body = isSgBasicBlock(node)) {
    const auto *definition = isSgFunctionDefinition(body->get_parent());
    if (definition == nullptr || definition->get_body() != body)
      return false;
    declaration = definition->get_declaration();
  }
  const auto *program = isSgProgramHeaderStatement(declaration);
  const auto *procedure = isSgProcedureHeaderStatement(declaration);
  if ((program == nullptr) == (procedure == nullptr))
    return false;
  const auto *definition = declaration->get_definition();
  if (definition == nullptr)
    return false;
  const auto *body = definition->get_body();
  if (node != declaration && node != definition && node != body)
    fail("program unit container is not its exact declaration, definition or "
         "body");
  const auto *start = declaration->get_startOfConstruct();
  const auto *end = declaration->get_endOfConstruct();
  if (start == nullptr || end == nullptr)
    return false;
  // Generated/manual units retain their existing physical-output contract.
  if (start->isTransformation() || end->isTransformation() ||
      start->isCompilerGenerated() || end->isCompilerGenerated() ||
      (start->get_source_sequence_number() == 0 &&
       end->get_source_sequence_number() == 0 &&
       start->get_physical_file_occurrence_id() == 0 &&
       end->get_physical_file_occurrence_id() == 0))
    return false;
  if (definition->get_parent() != declaration || body == nullptr ||
      body->get_parent() != definition || declaration->get_scope() == nullptr ||
      declaration->get_definingDeclaration() != declaration ||
      definition->get_construction_physical_output_owner() != nullptr ||
      !definition->get_fortran_construction_name().getString().empty() ||
      SageInterface::getEnclosingSourceFile(declaration) != impl_->file ||
      (procedure != nullptr &&
       procedure->get_fortran_procedure_source_form() !=
           SgProcedureHeaderStatement::e_fortran_procedure_source_form_header))
    fail("program unit has no exact structural source owner");
  for (const SgLocatedNode *container :
       {static_cast<const SgLocatedNode *>(declaration),
        static_cast<const SgLocatedNode *>(definition),
        static_cast<const SgLocatedNode *>(body)}) {
    const auto *first = container->get_startOfConstruct();
    const auto *last = container->get_endOfConstruct();
    if (first != nullptr && last != nullptr &&
        (first->isTransformation() || last->isTransformation() ||
         first->isCompilerGenerated() || last->isCompilerGenerated()))
      return false;
    if (container->get_file_info() != first || first == nullptr ||
        last == nullptr || first == last || first->get_parent() != container ||
        last->get_parent() != container || !first->isOutputInCodeGeneration() ||
        !last->isOutputInCodeGeneration() || !impl_->ownsPoint(first) ||
        !impl_->ownsPoint(last) ||
        first->get_source_sequence_number() >
            last->get_source_sequence_number())
      fail("program unit has no exact ordered source span");
  }
  return true;
}

bool Context::emitProgramUnitBoundary(const SgFunctionDeclaration *declaration,
                                      ProgramUnitBoundary boundary,
                                      unsigned activeOccurrence) const {
  if (declaration == nullptr)
    fail("missing program unit output boundary owner");
  const Sg_File_Info *point = nullptr;
  switch (boundary) {
  case ProgramUnitBoundary::header:
    point = declaration->get_startOfConstruct();
    break;
  case ProgramUnitBoundary::closing:
    point = declaration->get_endOfConstruct();
    break;
  default:
    fail("invalid program unit output boundary");
  }
  if (!ownsProgramUnitContainer(declaration))
    return true;
  if (selectedOutput())
    return true;
  const unsigned occurrence =
      activeOccurrence == 0 ? impl_->state.root_occurrence : activeOccurrence;
  if (impl_->occurrences.count(occurrence) == 0)
    fail("program unit output has no exact active occurrence");
  return point->get_physical_file_occurrence_id() == occurrence &&
         point->get_physical_file_id() ==
             impl_->file->get_file_info()->get_physical_file_id();
}

void Context::validateSourcePreprocessingAnchor(
    const SgEmptyDeclaration *anchor) const {
  impl_->validateSourcePreprocessingAnchor(anchor);
}

bool Context::ownsOutputBoundary(const SgLocatedNode *node) const {
  if (node == nullptr || impl_->occurrences.empty() ||
      SageInterface::getEnclosingSourceFile(
          const_cast<SgLocatedNode *>(node)) != impl_->file)
    return false;
  return ownsOutputBoundaryAnchors(node);
}

bool Context::ownsOutputBoundaryAnchors(const SgLocatedNode *node) const {
  if (node == nullptr || impl_->occurrences.empty())
    return false;
  const auto *start = node->get_startOfConstruct();
  const auto *end = node->get_endOfConstruct();
  const auto *owner = node->get_file_info();
  if (node == impl_->file->get_globalScope())
    return impl_->isRootBoundaryPoint(owner) &&
           impl_->isRootBoundaryPoint(start) && impl_->isRootBoundaryPoint(end);
  bool written = true;
  bool generated = true;
  for (const auto *point : {owner, start, end}) {
    if (point == nullptr || point->get_parent() != node || point->isShared() ||
        !point->isOutputInCodeGeneration() || point->get_physical_file_id() < 0)
      return false;
    written &= impl_->ownsPoint(point);
    generated &=
        point->get_source_sequence_number() == 0 &&
        (point->isTransformation() || point->isCompilerGenerated()) &&
        !point->isFrontendSpecific() &&
        !point->isSourcePositionUnavailableInFrontend() &&
        point->get_physical_file_id() == owner->get_physical_file_id() &&
        point->get_physical_file_occurrence_id() ==
            owner->get_physical_file_occurrence_id();
  }
  if (written)
    return start->get_source_sequence_number() <=
               end->get_source_sequence_number() &&
           (selectedOutput() ||
            start->get_physical_file_occurrence_id() ==
                end->get_physical_file_occurrence_id() ||
            ownsProgramUnitContainer(node));
  if (!generated ||
      impl_->physical_paths.count(owner->get_physical_filename()) == 0)
    return false;
  const unsigned occurrence = owner->get_physical_file_occurrence_id();
  if (occurrence == 0)
    return owner->get_physical_filename() == impl_->state.root_path;
  const auto found = impl_->occurrences.find(occurrence);
  return found != impl_->occurrences.end() &&
         found->second.path == owner->get_physical_filename();
}

bool Context::canAttachSourcePreprocessing(const PreprocessingInfo *record,
                                           const SgLocatedNode *node) const {
  return canAttachSourcePreprocessingImpl(record, node, nullptr);
}

bool Context::canAttachSourcePreprocessingImpl(
    const PreprocessingInfo *record, const SgLocatedNode *node,
    const PendingSubtreePublication *publication) const {
  if (publication != nullptr && (&publication->impl_->context != this ||
                                 !publication->impl_->contains(record, node)))
    return false;
  if (record == nullptr || !record->has_file_info() ||
      record->getFortranSourceRole() == Role::none ||
      !record->get_file_info()->isOutputInCodeGeneration() ||
      !impl_->ownsPoint(record->get_file_info()) || !ownsOutputBoundary(node))
    return false;
  const auto *point = record->get_file_info();
  if (point->get_parent() != record->getAttachedOwner())
    return false;
  if (const auto *prior = record->getAttachedOwner()) {
    const auto *priorFile = SageInterface::getEnclosingSourceFile(prior);
    const bool pending =
        publication != nullptr && ownsOutputBoundaryAnchors(prior);
    if (priorFile != impl_->file &&
        (priorFile != nullptr ||
         (!pending && (prior->get_parent() != nullptr ||
                       !ownsOutputBoundaryAnchors(prior) ||
                       !impl_->ownsRetiredRecord(prior, record)))))
      return false;
  }
  requireRole(record, true);
  // The exact source point, destination and prior owner belong to this
  // translation unit. Attachment can cross INCLUDE occurrences; preserve-source
  // emission still filters the record by its unchanged physical source owner.
  return true;
}

bool Context::emitPreprocessing(const PreprocessingInfo *record) const {
  if (!selectedOutput())
    return true;
  if (record == nullptr || !record->has_file_info())
    fail("missing preprocessing source record");
  if (generatedPreprocessing(record, impl_->file))
    return true;
  const bool sourceOwned = impl_->ownsPoint(record->get_file_info());
  requireRole(record, sourceOwned);
  switch (record->getFortranSourceRole()) {
  case Role::none:
  case Role::comment:
  case Role::layout:
  case Role::disabled_directive:
    return true;
  case Role::cpp_control:
  case Role::cpp_inactive:
  case Role::lexical_control:
    return false;
  }
  fail("invalid preprocessing lexical role");
}

bool Context::emitInclude(const SgFortranIncludeLine *line) const {
  if (!selectedOutput())
    return true;
  if (line != nullptr && generatedInclude(line, impl_->file))
    return true;
  if (line == nullptr || impl_->find(line->get_startOfConstruct(),
                                     Kind::native_include) == nullptr)
    fail("selected native INCLUDE has no exact inclusion event");
  return false;
}

void prepareEdit(SgSourceFile *file, EditReason reason,
                 const std::vector<const SgLocatedNode *> &syntax,
                 const std::vector<const SgStatement *> &consumedBodies) {
  switch (reason) {
  case EditReason::paired_boundary_absorption:
  case EditReason::directive_lowering:
  case EditReason::body_outlining:
  case EditReason::associated_statement_rewrite:
  case EditReason::loop_transformation:
  case EditReason::source_file_relocation:
    break;
  default:
    fail("invalid source edit reason");
  }
  if (syntax.empty() && consumedBodies.empty())
    fail("source edit has no exact operands");
  for (const auto *node : syntax)
    if (node == nullptr)
      fail("source edit has a null syntax operand");
  for (const auto *body : consumedBodies)
    if (body == nullptr)
      fail("source edit has a null body operand");
  if (!isFortran(file))
    return;
  const auto &state = file->get_fortran_source_context();
  if (state.root_occurrence == 0) {
    if (state.root_entry != 0 || state.root_exit != 0 ||
        !state.root_path.empty() || !state.inclusions.empty() ||
        state.output_surface != Surface::preserve_source)
      fail("incomplete root ownership");
    validate(file);
    // Manually constructed and generated ASTs have no lexical input tree.
    // Their existing transformation/output contracts remain authoritative.
    return;
  }
  const Context context{file};
  bool absorbed = false;
  auto inspectPoint = [&](const Sg_File_Info *point) {
    if (point == nullptr)
      fail("source edit operand has no position");
    if (!context.ownsSourcePoint(point)) {
      if (point->isTransformation() || point->isCompilerGenerated() ||
          point->isSourcePositionUnavailableInFrontend())
        return;
      const auto *global = file->get_globalScope();
      if (point->get_parent() == global && context.ownsOutputBoundary(global))
        return;
      const auto &external = file->get_frontendExternalOwnershipPathList();
      if (std::find(external.begin(), external.end(),
                    point->get_physical_filename()) != external.end())
        return;
      fail("written source edit operand has no exact lexical identity");
    }
    absorbed |=
        point->get_physical_file_occurrence_id() != state.root_occurrence;
  };
  auto inspectSyntax = [&](const SgLocatedNode *node) {
    if (node == nullptr)
      fail("source edit has a null syntax operand");
    if (SageInterface::getEnclosingSourceFile(
            const_cast<SgLocatedNode *>(node)) != file)
      fail("source edit operand has a different translation-unit owner");
    inspectPoint(node->get_startOfConstruct());
    inspectPoint(node->get_endOfConstruct());
  };
  auto inspectInclude = [&](const Sg_File_Info *point, Kind kind) {
    if (point == nullptr || point->get_source_sequence_number() == 0)
      return;
    const auto found =
        std::lower_bound(state.inclusions.begin(), state.inclusions.end(),
                         point->get_source_sequence_number(),
                         [](const Inclusion &edge, unsigned order) {
                           return edge.directive_begin < order;
                         });
    if (found != state.inclusions.end() &&
        found->directive_begin == point->get_source_sequence_number() &&
        found->parent_occurrence == point->get_physical_file_occurrence_id() &&
        found->kind == kind)
      absorbed = true;
  };
  for (const auto *node : syntax)
    inspectSyntax(node);
  for (const auto *body : consumedBodies)
    inspectSyntax(body);
  if (selectedOutput(file))
    return;
  for (const auto *body : consumedBodies) {
    if (body == nullptr)
      fail("source edit has a null body operand");
    for (SgNode *node : NodeQuery::querySubTree(const_cast<SgStatement *>(body),
                                                V_SgLocatedNode)) {
      auto *located = isSgLocatedNode(node);
      inspectSyntax(located);
      if (isSgFortranIncludeLine(located) != nullptr)
        inspectInclude(located->get_startOfConstruct(), Kind::native_include);
      const auto *records = located->getAttachedPreprocessingInfo();
      if (records == nullptr)
        continue;
      for (const auto *record : *records) {
        if (record == nullptr || !record->has_file_info())
          fail("source edit body has malformed preprocessing ownership");
        if (record->getTypeOfDirective() ==
            PreprocessingInfo::CpreprocessorIncludeDeclaration)
          inspectInclude(record->get_file_info(), Kind::cpp_include);
        else if (record->getTypeOfDirective() ==
                 PreprocessingInfo::CpreprocessorIncludeNextDeclaration)
          inspectInclude(record->get_file_info(), Kind::cpp_include_next);
      }
    }
  }
  if (!absorbed)
    return;
  validate(file);
  auto selected = state;
  selected.output_surface = Surface::selected_translation_unit;
  file->set_fortran_source_context(selected);
}

void preparePairedBoundary(SgLocatedNode *begin, SgLocatedNode *end) {
  if (begin == nullptr || end == nullptr)
    fail("paired source edit has a null boundary");
  auto *file = SageInterface::getEnclosingSourceFile(begin);
  if (!isFortran(file))
    return;
  if (SageInterface::getEnclosingSourceFile(end) != file)
    fail("paired source boundaries have different translation-unit owners");
  const auto *first = begin->get_startOfConstruct();
  const auto *last = end->get_endOfConstruct();
  if (first == nullptr || last == nullptr ||
      first->get_physical_file_occurrence_id() == 0 ||
      last->get_physical_file_occurrence_id() == 0 ||
      first->get_source_sequence_number() == 0 ||
      first->get_source_sequence_number() >= last->get_source_sequence_number())
    fail("paired source edit has no ordered lexical boundaries");
  if (first->get_physical_file_occurrence_id() !=
      last->get_physical_file_occurrence_id()) {
    if (file->get_fortran_source_context().root_occurrence == 0)
      fail("crossing source boundaries have no rooted context");
    prepareEdit(file, EditReason::paired_boundary_absorption, {begin, end});
  }
}

} // namespace Rose::FortranSource
