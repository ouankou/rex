#include "ompAstConstruction.h"
#include "rose.h"
#include "sageAstJsonPrivate.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace {
namespace fs = std::filesystem;

struct Options {
  std::string family;
  std::string layout;
  fs::path directory;
  bool generic = false;
  bool negatives = false;
};

Options takeOptions(int &argc, char **argv) {
  Options result;
  int retained = 1;
  for (int index = 1; index < argc; ++index) {
    const std::string argument = argv[index];
    if (argument.rfind("--include-family=", 0) == 0)
      result.family = argument.substr(17);
    else if (argument.rfind("--include-case=", 0) == 0)
      result.layout = argument.substr(15);
    else if (argument.rfind("--include-directory=", 0) == 0)
      result.directory = fs::canonical(argument.substr(20));
    else if (argument == "--include-generic")
      result.generic = true;
    else if (argument == "--include-negatives")
      result.negatives = true;
    else
      argv[retained++] = argv[index];
  }
  argc = retained;
  argv[argc] = nullptr;
  ROSE_ASSERT(result.family == "omp" || result.family == "acc");
  ROSE_ASSERT(result.layout == "empty" || result.layout == "nonempty" ||
              result.layout == "repeat" || result.layout == "cpp_state" ||
              result.layout == "macro_native" ||
              result.layout == "selected_scope");
  ROSE_ASSERT(!result.directory.empty());
  ROSE_ASSERT(!result.negatives || result.layout == "selected_scope");
  return result;
}

bool within(const SgNode *node, const SgNode *ancestor) {
  for (; node != nullptr; node = node->get_parent())
    if (node == ancestor)
      return true;
  return false;
}

void requirePoint(const Sg_File_Info *point, const fs::path &path, int line) {
  ROSE_ASSERT(point != nullptr &&
              !point->isSourcePositionUnavailableInFrontend());
  ROSE_ASSERT(fs::canonical(point->get_physical_filename()) == path);
  ROSE_ASSERT(point->get_physical_line() == line && point->get_col() > 0);
  ROSE_ASSERT(point->get_source_sequence_number() > 0);
  ROSE_ASSERT(point->get_physical_file_occurrence_id() > 0);
}

struct Marker {
  SgLocatedNode *owner;
  const PreprocessingInfo *comment;

  const Sg_File_Info *point() const { return comment->get_file_info(); }
  unsigned order() const { return point()->get_source_sequence_number(); }
  unsigned occurrence() const {
    return point()->get_physical_file_occurrence_id();
  }
};

using Markers = std::map<std::string, std::vector<Marker>>;

Markers collectMarkers(SgSourceFile *file) {
  const std::vector<std::string> names{"BEFORE",
                                       "BEGIN_INLINE",
                                       "FROM_INCLUDE",
                                       "BODY_MAIN",
                                       "END_BEFORE",
                                       "END_INLINE",
                                       "AFTER",
                                       "OUTER_BEGIN",
                                       "FIRST",
                                       "REPEAT_END",
                                       "REPEAT_OUTSIDE",
                                       "REPEAT_BEGIN",
                                       "MIDDLE",
                                       "LAST",
                                       "OUTER_END",
                                       "T",
                                       "COMMENT_CHILD_FIRST",
                                       "COMMENT_CHILD_LAST"};
  Markers result;
  std::unordered_set<const PreprocessingInfo *> owners;
  for (SgNode *node : NodeQuery::querySubTree(file, V_SgLocatedNode)) {
    auto *located = isSgLocatedNode(node);
    const auto *comments = located->getAttachedPreprocessingInfo();
    if (comments == nullptr)
      continue;
    for (const PreprocessingInfo *comment : *comments) {
      ROSE_ASSERT(comment != nullptr && owners.insert(comment).second);
      for (const std::string &name : names) {
        if (comment->getString().find("REX_INCLUDE_" + name) !=
            std::string::npos)
          result[name].push_back({located, comment});
      }
    }
  }
  return result;
}

void describeMarker(const std::string &name, size_t index,
                    const Marker &marker) {
  std::cerr << "  marker=REX_INCLUDE_" << name << " index=" << index
            << " owner=" << marker.owner->class_name() << '@' << marker.owner
            << " parent=";
  if (const SgNode *parent = marker.owner->get_parent())
    std::cerr << parent->class_name() << '@' << parent;
  else
    std::cerr << "<null>";
  std::cerr << " relative=" << marker.comment->getRelativePosition()
            << " directive=" << marker.comment->getTypeOfDirective()
            << " spelling=" << std::quoted(marker.comment->getString()) << '\n';
  const auto describePoint = [](const char *role, const Sg_File_Info *point) {
    std::cerr << "    " << role << '=';
    if (point != nullptr) {
      std::cerr << (point->get_physical_file_id() >= 0
                        ? point->get_physical_filename()
                        : "<no-physical-file>")
                << ':' << point->get_physical_line() << ':' << point->get_col()
                << " order=" << point->get_source_sequence_number()
                << " occurrence=" << point->get_physical_file_occurrence_id();
    } else
      std::cerr << "<null>";
    std::cerr << '\n';
  };
  describePoint("comment",
                marker.comment->has_file_info() ? marker.point() : nullptr);
  describePoint("owner-start", marker.owner->get_startOfConstruct());
  describePoint("owner-end", marker.owner->get_endOfConstruct());
}

const Marker &requireMarker(const Markers &markers, const std::string &name,
                            size_t index, size_t count, const fs::path &path,
                            int line) {
  const auto found = markers.find(name);
  if (found == markers.end() || found->second.size() != count) {
    std::cerr << "REX_TEST_INVARIANT[include-marker-count]: marker=REX_INCLUDE_"
              << name << " expected=" << count << " actual="
              << (found == markers.end() ? 0 : found->second.size())
              << " requested-index=" << index << " expected-source=" << path
              << ':' << line << "\nAvailable marker owners:\n";
    for (const auto &entry : markers)
      for (size_t item = 0; item < entry.second.size(); ++item)
        describeMarker(entry.first, item, entry.second[item]);
  }
  ROSE_ASSERT(found != markers.end() && found->second.size() == count);
  const Marker &marker = found->second.at(index);
  requirePoint(marker.point(), path, line);
  std::ifstream input(path);
  ROSE_ASSERT(input.good());
  std::string physical;
  for (int row = 0; row < line; ++row)
    ROSE_ASSERT(static_cast<bool>(std::getline(input, physical)));
  const size_t position = physical.find("REX_INCLUDE_" + name);
  ROSE_ASSERT(position != std::string::npos);
  const size_t begin = physical.rfind('!', position);
  ROSE_ASSERT(begin != std::string::npos);
  ROSE_ASSERT(marker.point()->get_col() == static_cast<int>(begin + 1));
  ROSE_ASSERT(marker.comment->getString() == physical.substr(begin));
  ROSE_ASSERT(marker.comment->getFortranSourceRole() ==
              PreprocessingInfo::FortranSourceRole::comment);
  if (index != 0)
    ROSE_ASSERT(found->second[index - 1].order() < marker.order());
  return marker;
}

void requireInterior(const Marker &marker, SgStatement *body) {
  ROSE_ASSERT(within(marker.owner, body));
  if (isSgBasicBlock(marker.owner) != nullptr) {
    ROSE_ASSERT(marker.comment->getRelativePosition() ==
                PreprocessingInfo::inside);
  } else {
    const unsigned start =
        marker.owner->get_startOfConstruct()->get_source_sequence_number();
    const unsigned end =
        marker.owner->get_endOfConstruct()->get_source_sequence_number();
    if (marker.order() < start)
      ROSE_ASSERT(marker.comment->getRelativePosition() ==
                  PreprocessingInfo::before);
    else {
      ROSE_ASSERT(marker.order() > end);
      ROSE_ASSERT(marker.comment->getRelativePosition() ==
                  PreprocessingInfo::after);
    }
  }
}

void requireAfter(const Marker &marker, SgStatement *region) {
  ROSE_ASSERT(marker.owner == region);
  ROSE_ASSERT(marker.comment->getRelativePosition() ==
              PreprocessingInfo::after);
}

void requireOrder(const std::vector<unsigned> &points) {
  ROSE_ASSERT(!points.empty() && points.front() > 0);
  for (size_t index = 1; index < points.size(); ++index)
    ROSE_ASSERT(points[index - 1] < points[index]);
}

void requireSelectedTokens(const SgPragmaDeclaration *pragma) {
  const auto &text = pragma->get_fortran_selected_output_text();
  const auto &tokens = pragma->get_fortran_selected_output_tokens();
  if (!text) {
    ROSE_ASSERT(tokens.empty());
    return;
  }
  std::string joined;
  for (const auto &token : tokens)
    joined += token.spelling;
  ROSE_ASSERT(joined == *text);
  if (pragma->get_fortran_directive_group_id().empty() ||
      pragma->get_fortran_directive_primary())
    ROSE_ASSERT(!text->empty() && !tokens.empty());
  else
    ROSE_ASSERT(text->empty() && tokens.empty());
}

void checkOrdinaryIncludes(SgSourceFile *file, const Options &options) {
  const bool selected = options.layout == "selected_scope";
  const auto &context = file->get_fortran_source_context();
  const fs::path parent = fs::canonical(file->get_sourceFileNameWithPath());
  ROSE_ASSERT(
      context.output_surface ==
      (selected && !options.generic
           ? SgSourceFile::fortran_output_surface::selected_translation_unit
           : SgSourceFile::fortran_output_surface::preserve_source));
  ROSE_ASSERT(context.root_occurrence > 0 && context.root_entry > 0 &&
              context.root_entry < context.root_exit);
  ROSE_ASSERT(fs::canonical(context.root_path) == parent);
  std::vector<std::string> names;
  std::vector<SgSourceFile::fortran_include_kind> kinds;
  using Kind = SgSourceFile::fortran_include_kind;
  if (selected) {
    names = {"rex_fortran_include_selected_native.inc",
             "rex_fortran_include_selected_cpp.inc",
             "rex_fortran_include_selected_execution.inc"};
    kinds = {Kind::native_include, Kind::cpp_include, Kind::native_include};
  } else {
    names = {"rex_fortran_include_" + options.layout + ".inc"};
    kinds = {options.layout == "cpp_state" ? Kind::cpp_include
                                           : Kind::native_include};
  }
  ROSE_ASSERT(context.inclusions.size() == names.size());
  std::unordered_set<unsigned> occurrences{context.root_occurrence};
  unsigned previous = context.root_entry;
  for (size_t index = 0; index < names.size(); ++index) {
    const auto &edge = context.inclusions[index];
    ROSE_ASSERT(edge.kind == kinds[index] &&
                edge.source_filename == names[index]);
    ROSE_ASSERT(fs::canonical(edge.physical_path) ==
                options.directory / names[index]);
    ROSE_ASSERT(edge.parent_occurrence == context.root_occurrence &&
                edge.child_occurrence > 0 &&
                occurrences.insert(edge.child_occurrence).second);
    requireOrder({previous, edge.directive_begin, edge.directive_end,
                  edge.child_entry, edge.child_exit, context.root_exit});
    previous = edge.child_exit;
  }
  ROSE_ASSERT(NodeQuery::querySubTree(file, V_SgFortranIncludeLine).size() ==
              (selected                        ? 2
               : options.layout == "cpp_state" ? 0
                                               : 1));
  size_t disabled = 0;
  size_t controls = 0;
  size_t inactive = 0;
  size_t lexical_controls = 0;
  size_t eof_comments = 0;
  for (SgNode *node : NodeQuery::querySubTree(file, V_SgLocatedNode)) {
    const auto *entries = isSgLocatedNode(node)->getAttachedPreprocessingInfo();
    if (entries == nullptr)
      continue;
    for (const auto *entry : *entries) {
      ROSE_ASSERT(entry != nullptr);
      using Role = PreprocessingInfo::FortranSourceRole;
      if (entry->getString() == "! REX_SELECTED_CONTEXT_EOF") {
        ROSE_ASSERT(selected && node == file->get_globalScope());
        ROSE_ASSERT(entry->getFortranSourceRole() == Role::comment);
        requirePoint(entry->get_file_info(), parent, 14);
        ROSE_ASSERT(entry->get_file_info()->get_col() == 1 &&
                    entry->get_file_info()->get_physical_file_occurrence_id() ==
                        context.root_occurrence);
        requireOrder({context.root_entry,
                      entry->get_file_info()->get_source_sequence_number(),
                      context.root_exit});
        ++eof_comments;
      }
      if (entry->getFortranSourceRole() == Role::cpp_control)
        ++controls;
      if (entry->getFortranSourceRole() == Role::cpp_inactive)
        ++inactive;
      if (entry->getFortranSourceRole() == Role::lexical_control) {
        ROSE_ASSERT(selected && entry->getString() == "!dir$ fixed\n");
        requirePoint(entry->get_file_info(),
                     options.directory /
                         "rex_fortran_include_selected_execution.inc",
                     4);
        const auto &inclusion = context.inclusions.at(2);
        ROSE_ASSERT(entry->get_file_info()->get_col() == 1 &&
                    entry->get_file_info()->get_physical_file_occurrence_id() ==
                        inclusion.child_occurrence);
        requireOrder({inclusion.child_entry,
                      entry->get_file_info()->get_source_sequence_number(),
                      inclusion.child_exit});
        ++lexical_controls;
      }
      if (entry->getFortranSourceRole() == Role::disabled_directive) {
        ROSE_ASSERT(options.layout == "cpp_state");
        ROSE_ASSERT(entry->getString() ==
                    "!$omp barrier ! REX_SELECTED_DISABLED\n");
        requirePoint(entry->get_file_info(), parent, 3);
        ROSE_ASSERT(entry->get_file_info()->get_col() == 1 &&
                    entry->get_file_info()->get_physical_file_occurrence_id() ==
                        context.root_occurrence);
        requireOrder({context.root_entry,
                      entry->get_file_info()->get_source_sequence_number(),
                      context.root_exit});
        ++disabled;
      }
    }
  }
  ROSE_ASSERT(controls > 0);
  ROSE_ASSERT(disabled == (options.layout == "cpp_state" ? 1 : 0));
  ROSE_ASSERT(inactive > 0 || options.layout == "macro_native");
  ROSE_ASSERT(lexical_controls == (selected ? 1 : 0));
  ROSE_ASSERT(eof_comments == (selected ? 1 : 0));
  if (!selected)
    return;
  ROSE_ASSERT(NodeQuery::querySubTree(file, V_SgOmpParallelStatement).size() ==
              (options.generic ? 0 : 1));
  size_t opaque = 0;
  size_t parallel = 0;
  for (SgNode *node : NodeQuery::querySubTree(file, V_SgPragmaDeclaration)) {
    auto *pragma = isSgPragmaDeclaration(node);
    requireSelectedTokens(pragma);
    const auto &raw = pragma->get_fortran_directive_raw_text();
    const auto &text = pragma->get_fortran_selected_output_text();
    if (raw.find("REX_SCOPE_OPAQUE_PAYLOAD") != std::string::npos) {
      ROSE_ASSERT(pragma->get_fortran_directive_family() ==
                  SgPragmaDeclaration::e_fortran_directive_ompx);
      ROSE_ASSERT(text &&
                  *text == "ompx rex_selected_opaque(\"space payload\")");
      size_t quoted = 0;
      for (const auto &token : pragma->get_fortran_selected_output_tokens())
        if (token.quoted_literal_offset) {
          ROSE_ASSERT(*token.quoted_literal_offset == 0 &&
                      token.spelling == "\"space payload\"");
          ++quoted;
        }
      ROSE_ASSERT(quoted == 1);
      ++opaque;
    }
    if (raw.find("num_threads(REX_SCOPE_THREADS)") != std::string::npos) {
      ROSE_ASSERT(options.generic && text &&
                  *text == "omp parallel num_threads(1) shared(total)");
      ++parallel;
    }
  }
  ROSE_ASSERT(opaque == 1 && parallel == (options.generic ? 1 : 0));
}

void checkFile(SgSourceFile *file, const Options &options) {
  if (options.layout == "cpp_state" || options.layout == "macro_native" ||
      options.layout == "selected_scope") {
    checkOrdinaryIncludes(file, options);
    return;
  }
  const fs::path parent = fs::canonical(file->get_sourceFileNameWithPath());
  const std::string language = options.family == "omp" ? "openmp" : "openacc";
  const fs::path begin =
      options.directory / ("rex_fortran_" + language + "_include_begin.inc");
  const fs::path turn =
      options.directory / ("rex_fortran_" + language + "_include_turnover.inc");
  const fs::path comments =
      options.directory / ("rex_fortran_" + language + "_include_comments.inc");
  const bool repeated = options.layout == "repeat";
  const bool nonempty = options.layout == "nonempty";
  const auto &context = file->get_fortran_source_context();
  ROSE_ASSERT(context.output_surface ==
              SgSourceFile::fortran_output_surface::selected_translation_unit);
  ROSE_ASSERT(context.root_occurrence > 0 && context.root_entry > 0 &&
              context.root_entry < context.root_exit);
  ROSE_ASSERT(fs::canonical(context.root_path) == parent);
  ROSE_ASSERT(context.inclusions.size() == (nonempty ? 1 : 2));
  std::unordered_set<unsigned> occurrences{context.root_occurrence};
  unsigned previous_exit = context.root_entry;
  for (size_t index = 0; index < context.inclusions.size(); ++index) {
    const auto &inclusion = context.inclusions[index];
    const fs::path expected = repeated ? turn : index == 0 ? begin : comments;
    ROSE_ASSERT(inclusion.kind ==
                SgSourceFile::fortran_include_kind::native_include);
    ROSE_ASSERT(inclusion.parent_occurrence == context.root_occurrence);
    ROSE_ASSERT(inclusion.child_occurrence > 0 &&
                occurrences.insert(inclusion.child_occurrence).second);
    ROSE_ASSERT(fs::canonical(inclusion.physical_path) == expected);
    ROSE_ASSERT(inclusion.source_filename == expected.filename().string());
    requireOrder({previous_exit, inclusion.directive_begin,
                  inclusion.directive_end, inclusion.child_entry,
                  inclusion.child_exit, context.root_exit});
    previous_exit = inclusion.child_exit;
  }
  const auto nodes = NodeQuery::querySubTree(
      file, options.family == "omp" ? V_SgOmpParallelStatement
                                    : V_SgAccParallelStatement);
  ROSE_ASSERT(nodes.size() == (repeated ? 3 : 1));
  std::vector<SgStatement *> regions;
  std::vector<SgStatement *> bodies;
  std::vector<unsigned> starts;
  for (size_t index = 0; index < nodes.size(); ++index) {
    auto *region = isSgStatement(nodes[index]);
    auto *omp = isSgOmpParallelStatement(region);
    auto *acc = isSgAccParallelStatement(region);
    auto *body = omp != nullptr ? omp->get_body() : acc->get_body();
    ROSE_ASSERT(body != nullptr && body->get_parent() == region);
    ROSE_ASSERT(omp != nullptr ? omp->get_clauses().empty()
                               : acc->get_clauses().empty());
    requirePoint(region->get_startOfConstruct(),
                 repeated && index == 0 ? parent
                 : repeated             ? turn
                                        : begin,
                 repeated && index == 0 ? 6
                 : repeated             ? 3
                                        : 1);
    const unsigned start =
        region->get_startOfConstruct()->get_source_sequence_number();
    if (!starts.empty())
      ROSE_ASSERT(starts.back() < start);
    starts.push_back(start);
    regions.push_back(region);
    bodies.push_back(body);

    size_t includes = 0;
    size_t continues = 0;
    for (SgNode *statement : NodeQuery::querySubTree(body, V_SgStatement)) {
      if (isSgBasicBlock(statement) != nullptr)
        continue;
      if (auto *include = isSgFortranIncludeLine(statement)) {
        ROSE_ASSERT((repeated && index < 2) || options.layout == "empty");
        requirePoint(include->get_startOfConstruct(), parent,
                     index == 0 ? 8 : 10);
        requirePoint(include->get_endOfConstruct(), parent,
                     index == 0 ? 8 : 10);
        ROSE_ASSERT(include->get_filename() ==
                    (repeated ? turn : comments).filename().string());
        ++includes;
      } else if (auto *continuation = isSgFortranContinueStmt(statement)) {
        ROSE_ASSERT(nonempty);
        requirePoint(continuation->get_startOfConstruct(), parent, 8);
        requirePoint(continuation->get_endOfConstruct(), parent, 8);
        ++continues;
      } else {
        ROSE_ASSERT(!"include-region body contains an invented or unexpected "
                     "executable statement");
      }
    }
    ROSE_ASSERT(includes ==
                ((repeated && index < 2) || options.layout == "empty" ? 1 : 0));
    ROSE_ASSERT(continues == (nonempty ? 1 : 0));
    if (includes == 0 && continues == 0) {
      auto *block = isSgBasicBlock(body);
      ROSE_ASSERT(block != nullptr && block->get_statements().empty());
      requirePoint(block->get_startOfConstruct(), repeated ? turn : begin,
                   repeated ? 3 : 1);
      requirePoint(block->get_endOfConstruct(), parent, repeated ? 12 : 10);
    }
  }

  const auto all_includes =
      NodeQuery::querySubTree(file, V_SgFortranIncludeLine);
  ROSE_ASSERT(all_includes.size() == (nonempty ? 1 : 2));
  const Markers markers = collectMarkers(file);
  const Marker &before = requireMarker(markers, "BEFORE", 0, 1, parent, 5);
  const Marker &after =
      requireMarker(markers, "AFTER", 0, 1, parent, repeated ? 13 : 11);
  for (auto *body : bodies) {
    ROSE_ASSERT(!within(before.owner, body) && !within(after.owner, body));
  }
  if (!repeated) {
    const Marker &opening =
        requireMarker(markers, "BEGIN_INLINE", 0, 1, begin, 1);
    const Marker &included =
        requireMarker(markers, "FROM_INCLUDE", 0, 1, begin, 2);
    const Marker &main = requireMarker(markers, "BODY_MAIN", 0, 1, parent, 7);
    const Marker &closing =
        requireMarker(markers, "END_BEFORE", 0, 1, parent, 9);
    const Marker &end = requireMarker(markers, "END_INLINE", 0, 1, parent, 10);
    for (const Marker *marker : {&opening, &included, &main, &closing})
      requireInterior(*marker, bodies.front());
    requireAfter(end, regions.front());
    ROSE_ASSERT(opening.occurrence() == included.occurrence());
    ROSE_ASSERT(opening.occurrence() ==
                regions.front()
                    ->get_startOfConstruct()
                    ->get_physical_file_occurrence_id());
    ROSE_ASSERT(main.occurrence() == end.occurrence() &&
                end.occurrence() == after.occurrence());
    ROSE_ASSERT(opening.occurrence() != end.occurrence());
    if (nonempty) {
      requireOrder({before.order(), starts.front(), opening.order(),
                    included.order(), main.order(), closing.order(),
                    end.order(), after.order()});
    } else {
      const Marker &trailing = requireMarker(markers, "T", 0, 1, parent, 8);
      const Marker &child_first =
          requireMarker(markers, "COMMENT_CHILD_FIRST", 0, 1, comments, 1);
      const Marker &child_last =
          requireMarker(markers, "COMMENT_CHILD_LAST", 0, 1, comments, 2);
      requireInterior(child_first, bodies.front());
      requireInterior(child_last, bodies.front());
      ROSE_ASSERT(within(trailing.owner, bodies.front()));
      ROSE_ASSERT(trailing.comment->getRelativePosition() ==
                  (isSgBasicBlock(trailing.owner) != nullptr
                       ? PreprocessingInfo::inside
                       : PreprocessingInfo::after));
      ROSE_ASSERT(trailing.occurrence() == main.occurrence());
      ROSE_ASSERT(child_first.occurrence() == child_last.occurrence());
      ROSE_ASSERT(child_first.occurrence() != main.occurrence() &&
                  child_first.occurrence() != opening.occurrence());
      requireOrder({before.order(), starts.front(), opening.order(),
                    included.order(), main.order(), trailing.order(),
                    child_first.order(), child_last.order(), closing.order(),
                    end.order(), after.order()});
    }
  } else {
    const Marker &opening =
        requireMarker(markers, "OUTER_BEGIN", 0, 1, parent, 6);
    const Marker &first = requireMarker(markers, "FIRST", 0, 1, parent, 7);
    const Marker &middle = requireMarker(markers, "MIDDLE", 0, 1, parent, 9);
    const Marker &last = requireMarker(markers, "LAST", 0, 1, parent, 11);
    const Marker &end = requireMarker(markers, "OUTER_END", 0, 1, parent, 12);
    const Marker &end1 = requireMarker(markers, "REPEAT_END", 0, 2, turn, 1);
    const Marker &end2 = requireMarker(markers, "REPEAT_END", 1, 2, turn, 1);
    const Marker &outside1 =
        requireMarker(markers, "REPEAT_OUTSIDE", 0, 2, turn, 2);
    const Marker &outside2 =
        requireMarker(markers, "REPEAT_OUTSIDE", 1, 2, turn, 2);
    const Marker &begin1 =
        requireMarker(markers, "REPEAT_BEGIN", 0, 2, turn, 3);
    const Marker &begin2 =
        requireMarker(markers, "REPEAT_BEGIN", 1, 2, turn, 3);
    requireInterior(opening, bodies[0]);
    requireInterior(first, bodies[0]);
    requireInterior(begin1, bodies[1]);
    requireInterior(middle, bodies[1]);
    requireInterior(begin2, bodies[2]);
    requireInterior(last, bodies[2]);
    requireAfter(end1, regions[0]);
    requireAfter(end2, regions[1]);
    requireAfter(end, regions[2]);
    for (const Marker *outside : {&outside1, &outside2})
      for (auto *body : bodies)
        ROSE_ASSERT(!within(outside->owner, body));
    ROSE_ASSERT(end1.occurrence() == outside1.occurrence() &&
                end1.occurrence() == begin1.occurrence());
    ROSE_ASSERT(end2.occurrence() == outside2.occurrence() &&
                end2.occurrence() == begin2.occurrence());
    ROSE_ASSERT(end1.occurrence() != end2.occurrence());
    ROSE_ASSERT(
        begin1.occurrence() ==
        regions[1]->get_startOfConstruct()->get_physical_file_occurrence_id());
    ROSE_ASSERT(
        begin2.occurrence() ==
        regions[2]->get_startOfConstruct()->get_physical_file_occurrence_id());
    requireOrder({before.order(), starts[0], opening.order(), first.order(),
                  end1.order(), outside1.order(), starts[1], begin1.order(),
                  middle.order(), end2.order(), outside2.order(), starts[2],
                  begin2.order(), last.order(), end.order(), after.order()});
  }
}

void requireInvalidOwnership(const Rose::AstJson::AstFileRecord &original,
                             SgSourceFile *file,
                             const Rose::AstJson::NodeMap &original_nodes) {
  using namespace Rose::AstJson;
  const std::vector<std::string> diagnostics{
      "duplicate include occurrence or source owner",
      "include escapes its parent occurrence",
      "include escapes its parent occurrence",
      "sibling include occurrences overlap",
      "invalid root ownership",
      "native INCLUDE has no exact inclusion event",
      "source preprocessing record has no lexical role",
      "CPP lexical role has incompatible preprocessing syntax",
      "source point escapes its exact occurrence",
      "written source point has no exact lexical identity",
      "layout lexical role contains non-whitespace source",
      "source pragma has no valid selected output surface",
      "written Fortran source has no rooted context",
      "selected pragma disagrees with its typed directive family",
      "source point escapes its exact occurrence"};
  const auto original_context = file->get_fortran_source_context();
  const auto original_json =
      buildJson(file, Checkpoint::PostOmpConstruction, file);
  std::vector<std::pair<SgNode *, uint64_t>> original_revisions;
  original_revisions.reserve(original_nodes.size());
  for (const auto &entry : original_nodes)
    original_revisions.emplace_back(entry.second,
                                    entry.second->get_lastMutationSequence());
  for (size_t mutation = 0; mutation < diagnostics.size(); ++mutation) {
    auto malformed = original;
    auto &context =
        malformed.nodes.at(malformed.index_by_id.at(malformed.root_id))
            .properties.at("fortran_source_context");
    auto &edges = context.at("inclusions").array;
    ROSE_ASSERT(edges.size() == 3);
    if (mutation == 0)
      edges[1].at("child_occurrence") = edges[0].at("child_occurrence");
    else if (mutation == 1)
      edges[0].at("parent_occurrence") = JsonValue::number("4294967295");
    else if (mutation == 2)
      edges[0].at("child_exit") = context.at("root_exit");
    else if (mutation == 3)
      edges[0].at("child_exit") = edges[1].at("child_entry");
    else if (mutation == 4)
      context.at("root_path") = JsonValue::string("/rex_foreign_root.f90");
    else if (mutation == 5)
      edges[0].at("source_filename") =
          JsonValue::string("rex_foreign_include.inc");
    else if (mutation == 12)
      context = parseJson(R"({"root_occurrence":0,"root_entry":0,"root_exit":0,
        "root_path":"","output_surface":0,"inclusions":[]})");
    else if (mutation == 14) {
      bool found = false;
      for (auto &record : malformed.nodes)
        if (record.kind == "SgExprStatement" &&
            record.location.at("start").at("physical_line").asInt() == 9) {
          ROSE_ASSERT(!found);
          for (const char *endpoint : {"start", "end"}) {
            auto &point = record.location.at(endpoint);
            ROSE_ASSERT(point.at("physical_file_occurrence_id")
                            .exactlyEquals(context.at("root_occurrence")) &&
                        point.at("source_sequence").asInt() > 0);
            point.at("physical_file_occurrence_id") =
                edges[0].at("child_occurrence");
          }
          found = true;
        }
      ROSE_ASSERT(found);
    } else if (mutation == 11 || mutation == 13) {
      bool found = false;
      for (auto &record : malformed.nodes)
        if (record.kind == "SgPragmaDeclaration" &&
            record.properties.at("fortran_directive_raw_text")
                    .asString()
                    .find("REX_SCOPE_OPAQUE_PAYLOAD") != std::string::npos) {
          ROSE_ASSERT(!found);
          if (mutation == 11) {
            record.properties.at("fortran_selected_output_text") =
                parseJson("null");
            record.properties.at("fortran_selected_output_tokens") =
                parseJson("[]");
          } else {
            auto &text = record.properties.at("fortran_selected_output_text");
            ROSE_ASSERT(text.asString().rfind("ompx ", 0) == 0);
            text = JsonValue::string("omp " + text.asString().substr(5));
            auto &tokens =
                record.properties.at("fortran_selected_output_tokens").array;
            ROSE_ASSERT(!tokens.empty() &&
                        tokens.front().at("spelling").asString() == "ompx");
            tokens.front().at("spelling") = JsonValue::string("omp");
          }
          found = true;
        }
      ROSE_ASSERT(found);
    } else {
      JsonValue *comment = nullptr;
      NodeRecord *owner = nullptr;
      for (auto &record : malformed.nodes)
        for (auto &entry : record.preprocessing.array)
          if (entry.at("text").asString() == "! REX_SELECTED_CONTEXT_COMMENT") {
            ROSE_ASSERT(comment == nullptr);
            comment = &entry;
            owner = &record;
          }
      ROSE_ASSERT(comment != nullptr && owner != nullptr);
      if (mutation == 6)
        comment->at("fortran_source_role") = JsonValue::number("0");
      else if (mutation == 7)
        comment->at("fortran_source_role") = JsonValue::number("3");
      else if (mutation == 8)
        comment->at("file_info").at("physical_file_occurrence_id") =
            edges[0].at("child_occurrence");
      else if (mutation == 9)
        comment->at("file_info").at("source_sequence") = JsonValue::number("0");
      else {
        comment->at("fortran_source_role") = JsonValue::number("2");
        comment->at("directive") = JsonValue::number(
            std::to_string(static_cast<int>(PreprocessingInfo::CSkippedToken)));
      }
      owner->properties.at("preprocessing") = owner->preprocessing;
    }
    const auto files = SgSourceFile::numberOfNodes();
    const auto positions = Sg_File_Info::numberOfNodes();
    const auto nodes = numberOfNodes();
    NodeMap restored;
    bool rejected = false;
    try {
      static_cast<void>(reconstructSourceFile(malformed, file, &restored));
      std::cerr << "Malformed Fortran source ownership was accepted: mutation="
                << mutation << '\n';
      std::_Exit(1);
    } catch (const std::runtime_error &error) {
      const std::string expected =
          "Fortran source context: " + diagnostics[mutation];
      if (std::string(error.what()) != expected)
        throw;
      rejected = true;
    }
    ROSE_ASSERT(rejected && restored.empty());
    ROSE_ASSERT(SgSourceFile::numberOfNodes() == files &&
                Sg_File_Info::numberOfNodes() == positions);
    ROSE_ASSERT(numberOfNodes() == nodes);
    for (const auto &[node, revision] : original_revisions) {
      ROSE_ASSERT(SgNode::isLiveNode(node));
      ROSE_ASSERT(node->get_lastMutationSequence() == revision);
    }
    ROSE_ASSERT(file->get_fortran_source_context() == original_context);
    ROSE_ASSERT(buildJson(file, Checkpoint::PostOmpConstruction, file) ==
                original_json);
  }
}

void requireRawRoundTrip(SgSourceFile *file, const Options &options) {
  using namespace Rose::AstJson;
  constexpr auto checkpoint = Checkpoint::PostOmpConstruction;
  NodeMap originals;
  const AstFileRecord ast =
      parseAstFileJson(buildJson(file, checkpoint, file, nullptr, &originals),
                       checkpointName(checkpoint));
  NodeMap restored;
  auto *copy = reconstructSourceFile(ast, file, &restored);
  ROSE_ASSERT(copy->get_fortran_source_context() ==
              file->get_fortran_source_context());
  NodeMap copied;
  const AstFileRecord second =
      parseAstFileJson(buildJson(copy, checkpoint, copy, nullptr, &copied),
                       checkpointName(checkpoint));
  std::unordered_map<const SgNode *, uint64_t> copied_ids;
  for (const auto &entry : copied)
    copied_ids.emplace(entry.second, entry.first);
  for (const auto &entry : originals) {
    SgNode *node = restored.at(entry.first);
    const auto &original_record = ast.node(entry.first);
    const auto &copy_record = second.node(copied_ids.at(node));
    // Deliberately compare raw records: normalized signatures erase the TU
    // point and physical include occurrence identities this regression needs.
    ROSE_ASSERT(original_record.location.exactlyEquals(copy_record.location));
    ROSE_ASSERT(
        original_record.preprocessing.exactlyEquals(copy_record.preprocessing));
    if (entry.second == file)
      ROSE_ASSERT(original_record.properties.at("fortran_source_context")
                      .exactlyEquals(
                          copy_record.properties.at("fortran_source_context")));
    if (auto *declaration = isSgDeclarationStatement(entry.second)) {
      auto *restored_declaration = isSgDeclarationStatement(node);
      ROSE_ASSERT(restored_declaration != nullptr);
      ROSE_ASSERT(declaration->get_translation_unit_source_order() ==
                  restored_declaration->get_translation_unit_source_order());
      if (declaration->get_translation_unit_source_order().has_value())
        ROSE_ASSERT(
            *declaration->get_translation_unit_source_order() ==
            declaration->get_startOfConstruct()->get_source_sequence_number());
    }
    if (auto *pragma = isSgPragmaDeclaration(entry.second)) {
      auto *restored_pragma = isSgPragmaDeclaration(node);
      ROSE_ASSERT(restored_pragma != nullptr &&
                  pragma->get_fortran_selected_output_text() ==
                      restored_pragma->get_fortran_selected_output_text());
      ROSE_ASSERT(pragma->get_fortran_selected_output_tokens() ==
                  restored_pragma->get_fortran_selected_output_tokens());
      ROSE_ASSERT(pragma->get_fortran_compiler_directive_sentinel() ==
                  restored_pragma->get_fortran_compiler_directive_sentinel());
      requireSelectedTokens(pragma);
      requireSelectedTokens(restored_pragma);
      ROSE_ASSERT(original_record.properties.at("fortran_selected_output_text")
                      .exactlyEquals(copy_record.properties.at(
                          "fortran_selected_output_text")));
      ROSE_ASSERT(
          original_record.properties.at("fortran_selected_output_tokens")
              .exactlyEquals(
                  copy_record.properties.at("fortran_selected_output_tokens")));
      ROSE_ASSERT(
          original_record.properties.at("fortran_compiler_directive_sentinel")
              .exactlyEquals(copy_record.properties.at(
                  "fortran_compiler_directive_sentinel")));
    }
  }
  checkFile(copy, options);
  if (options.negatives) {
    requireInvalidOwnership(ast, file, originals);
    checkFile(file, options);
  }
}
} // namespace

int main(int argc, char **argv) {
  const Options options = takeOptions(argc, argv);
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr && project->numberOfFiles() == 1);
  auto *file = isSgSourceFile(&project->get_file(0));
  ROSE_ASSERT(file != nullptr);
  checkFile(file, options);
  requireRawRoundTrip(file, options);
  AstTests::runAllTests(project);
  return backend(project);
}
