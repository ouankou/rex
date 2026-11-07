#include "FortranSourceContext.h"
#include "astJson/sageAstJson.h"
#include "astJson/sageAstJsonPrivate.h"
#include "nodeQuery.h"
#include "rose.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace {
namespace fs = std::filesystem;
const std::array<std::string, 5> markers{"REX_UNIT_BEFORE", "REX_UNIT_HEADER",
                                         "REX_UNIT_INSIDE", "REX_UNIT_CLOSING",
                                         "REX_UNIT_AFTER"};

std::string read(const fs::path &path) {
  std::ifstream input(path, std::ios::binary);
  ROSE_ASSERT(input.good());
  return {std::istreambuf_iterator<char>(input), {}};
}

std::string lower(std::string text) {
  for (auto &ch : text)
    ch = std::tolower(static_cast<unsigned char>(ch));
  return text;
}

std::string trim(const std::string &text) {
  const auto first = text.find_first_not_of(" \t\r");
  return first == std::string::npos ? "" : text.substr(first);
}

// This oracle accepts only the literal INCLUDE edges in these finite fixtures.
// It uses the output's actual relative lookup, never a repair search path.
std::string logicalInput(const fs::path &path,
                         const std::set<std::string> &allowed,
                         std::set<std::string> &active) {
  const auto actual = fs::canonical(path).string();
  ROSE_ASSERT(active.insert(actual).second);
  std::istringstream input(read(path));
  std::string result, line;
  while (std::getline(input, line)) {
    const auto text = trim(line);
    if (lower(text).rfind("include ", 0) != 0) {
      result += line + '\n';
      continue;
    }
    const auto filename = trim(text.substr(8));
    ROSE_ASSERT(filename.size() > 2 &&
                (filename.front() == '\'' || filename.front() == '"'));
    const auto end = filename.find(filename.front(), 1);
    ROSE_ASSERT(end != std::string::npos &&
                trim(filename.substr(end + 1)).empty());
    const auto child =
        fs::canonical(path.parent_path() / filename.substr(1, end - 1));
    ROSE_ASSERT(allowed.count(child.string()) == 1);
    result += logicalInput(child, allowed, active);
  }
  active.erase(actual);
  return result;
}

struct Snapshot {
  SgSourceFile::fortran_source_context context;
  std::vector<std::string> units;
  std::vector<std::string> preprocessing;
  bool operator==(const Snapshot &other) const {
    return context == other.context && units == other.units &&
           preprocessing == other.preprocessing;
  }
};

Snapshot check(SgSourceFile *file, const std::string &kind) {
  const auto state = file->get_fortran_source_context();
  ROSE_ASSERT(state.root_occurrence != 0 && state.inclusions.size() == 1);
  const auto edge = state.inclusions.front();
  ROSE_ASSERT(edge.kind == SgSourceFile::fortran_include_kind::native_include &&
              edge.parent_occurrence == state.root_occurrence &&
              edge.child_occurrence != state.root_occurrence);
  Rose::FortranSource::validate(file);
  Rose::FortranSource::Context context(file);
  Snapshot result{state, {}, {}};
  size_t units = 0;
  SgFunctionDeclaration *target = nullptr;
  for (auto *node : NodeQuery::querySubTree(file, V_SgFunctionDeclaration)) {
    auto *unit = isSgFunctionDeclaration(node);
    auto *definition = unit->get_definition();
    if (definition == nullptr)
      continue;
    ++units;
    auto *body = definition->get_body();
    ROSE_ASSERT(context.ownsProgramUnitContainer(unit) &&
                context.ownsProgramUnitContainer(definition) &&
                context.ownsProgramUnitContainer(body));
    for (SgLocatedNode *part : {static_cast<SgLocatedNode *>(unit),
                                static_cast<SgLocatedNode *>(definition),
                                static_cast<SgLocatedNode *>(body)})
      result.units.push_back(Rose::AstJson::rawLocationJson(part));
    if (kind != "procedure" || unit->get_name() == "rex_set_total")
      target = unit;
  }
  ROSE_ASSERT(units == (kind == "procedure" ? 2 : 1) && target != nullptr);
  const auto *first = target->get_startOfConstruct();
  const auto *last = target->get_endOfConstruct();
  const bool childFirst =
      kind == "implicit" || kind == "header" || kind == "whole";
  const bool childLast =
      kind == "closing" || kind == "procedure" || kind == "whole";
  const int firstLine = kind == "procedure" ? 7 : kind == "header" ? 1 : 2;
  const int lastLine = childLast && kind != "whole" ? 3 : 7;
  ROSE_ASSERT(
      first->get_physical_filename() ==
          (childFirst ? edge.physical_path : state.root_path) &&
      last->get_physical_filename() ==
          (childLast ? edge.physical_path : state.root_path) &&
      first->get_physical_line() == firstLine &&
      last->get_physical_line() == lastLine && first->get_raw_col() == 1 &&
      first->get_physical_file_occurrence_id() ==
          (childFirst ? edge.child_occurrence : state.root_occurrence) &&
      last->get_physical_file_occurrence_id() ==
          (childLast ? edge.child_occurrence : state.root_occurrence) &&
      first->get_source_sequence_number() > 0 &&
      first->get_source_sequence_number() < last->get_source_sequence_number());
  if (auto *program = isSgProgramHeaderStatement(target))
    ROSE_ASSERT(program->get_program_statement_kind() ==
                (kind == "implicit" || kind == "whole"
                     ? SgProgramHeaderStatement::e_implicit_program_statement
                     : SgProgramHeaderStatement::e_explicit_program_statement));
  std::istringstream closingSource(read(last->get_physical_filename()));
  std::string closingLine;
  for (int line = 0; line < lastLine; ++line)
    ROSE_ASSERT(static_cast<bool>(std::getline(closingSource, closingLine)));
  ROSE_ASSERT(last->get_raw_col() == static_cast<int>(closingLine.size()));
  std::array<const PreprocessingInfo *, 5> comments{};
  size_t ownerIndex = 0;
  for (auto *node : NodeQuery::querySubTree(file, V_SgLocatedNode)) {
    auto *owner = isSgLocatedNode(node);
    const size_t identity = ownerIndex++;
    owner->validateAttachedPreprocessingInfoOwnership();
    const auto *records = owner->getAttachedPreprocessingInfo();
    if (records == nullptr)
      continue;
    for (const auto *record : *records) {
      std::ostringstream raw;
      raw << identity << ':' << owner->class_name() << ':'
          << record->getRelativePosition() << ':'
          << record->getOutputPlacement() << ':'
          << static_cast<int>(record->getFortranSourceRole()) << ':'
          << record->getTypeOfDirective() << ':' << record->getNumberOfLines()
          << ':' << record->isTransformation() << ':' << record->getString()
          << Rose::AstJson::rawFileInfoJson(record->get_file_info());
      result.preprocessing.push_back(raw.str());
      for (size_t index = 0; index < markers.size(); ++index)
        if (record->getString().find(markers[index]) != std::string::npos) {
          ROSE_ASSERT(comments[index] == nullptr &&
                      record->getFortranSourceRole() ==
                          PreprocessingInfo::FortranSourceRole::comment &&
                      record->getAttachedOwner() == owner &&
                      record->get_file_info()->get_parent() == owner &&
                      context.ownsSourcePoint(record->get_file_info()));
          comments[index] = record;
        }
    }
  }
  unsigned previous = 0;
  for (const auto *record : comments) {
    ROSE_ASSERT(record != nullptr &&
                record->get_file_info()->get_source_sequence_number() >
                    previous);
    previous = record->get_file_info()->get_source_sequence_number();
  }
  for (size_t index = 0; index < markers.size(); ++index) {
    unsigned matches = 0;
    for (const auto &path : {state.root_path, edge.physical_path}) {
      std::istringstream source(read(path));
      std::string line;
      for (int number = 1; std::getline(source, line); ++number)
        if (line.find(markers[index]) != std::string::npos) {
          ++matches;
          const auto *point = comments[index]->get_file_info();
          ROSE_ASSERT(point->get_physical_filename() == path &&
                      point->get_physical_line() == number &&
                      point->get_raw_line() == number &&
                      point->get_raw_col() ==
                          static_cast<int>(line.find('!') + 1) &&
                      point->get_physical_file_occurrence_id() ==
                          (path == state.root_path ? state.root_occurrence
                                                   : edge.child_occurrence));
        }
    }
    ROSE_ASSERT(matches == 1);
  }
  return result;
}

void emit(SgSourceFile *file, const std::string &kind, const fs::path &output) {
  file->set_unparse_output_filename(output.string());
  file->unparse();
  std::set<std::string> allowed, active;
  for (const auto &edge : file->get_fortran_source_context().inclusions)
    allowed.insert(edge.physical_path);
  const auto text = logicalInput(output, allowed, active);
  size_t previous = 0;
  for (const auto &marker : markers) {
    const auto at = text.find(marker);
    ROSE_ASSERT(at != std::string::npos && at >= previous &&
                text.find(marker, at + marker.size()) == std::string::npos);
    previous = at + marker.size();
  }
  size_t programs = 0, endings = 0;
  std::istringstream lines(text);
  std::string line;
  while (std::getline(lines, line)) {
    const auto value = lower(trim(line));
    programs += value.rfind("program ", 0) == 0;
    endings += value == "end" || value.rfind("end program", 0) == 0 ||
               value.rfind("end subroutine", 0) == 0;
  }
  ROSE_ASSERT(programs == (kind == "implicit" || kind == "whole" ? 0 : 1) &&
              endings == (kind == "procedure" ? 2 : 1));
  size_t includes = 0;
  std::istringstream physical(read(output));
  while (std::getline(physical, line))
    includes += lower(trim(line)).rfind("include ", 0) == 0;
  const bool selected =
      file->get_fortran_output_surface() ==
      SgSourceFile::fortran_output_surface::selected_translation_unit;
  ROSE_ASSERT(includes == (selected ? 0 : 1));
}
} // namespace

int main(int argc, char **argv) {
  ROSE_ASSERT(argc == 5);
  const auto source = fs::canonical(argv[1]);
  const auto work = fs::absolute(argv[2]);
  const std::string kind = argv[3], surface = argv[4];
  ROSE_ASSERT((surface == "preserve" || surface == "selected") &&
              std::set<std::string>(
                  {"implicit", "closing", "header", "procedure", "whole"})
                      .count(kind) == 1);
  auto *project = frontend(std::vector<std::string>{
      argv[0], "-rose:skipfinalCompileStep", "-c", source.string()});
  ROSE_ASSERT(project != nullptr && project->numberOfFiles() == 1);
  auto *file = isSgSourceFile(project->get_fileList().front());
  if (surface == "selected") {
    auto state = file->get_fortran_source_context();
    state.output_surface =
        SgSourceFile::fortran_output_surface::selected_translation_unit;
    file->set_fortran_source_context(state);
  }
  const auto expected = check(file, kind);
  AstTests::runAllTests(project);
  emit(file, kind, source.parent_path() / ("live-" + surface + ".f90"));
  SgTreeCopy copier(
      SgCopyHelp::ProvenancePolicy::PreserveExactFrontendSemantic);
  auto *copy = isSgProject(project->copy(copier));
  auto *copiedFile = isSgSourceFile(copy->get_fileList().front());
  ROSE_ASSERT(check(copiedFile, kind) == expected);
  AstTests::runAllTests(copy);
  emit(copiedFile, kind, source.parent_path() / ("copied-" + surface + ".f90"));
  SageInterface::deleteAST(copy);
  Rose::AstJson::Options options;
  options.outputDirectory = (work / ("json-" + surface)).string();
  file = Rose::AstJson::roundTripSourceFile(
      file, Rose::AstJson::Checkpoint::PreOmpConstruction, options);
  ROSE_ASSERT(check(file, kind) == expected);
  AstTests::runAllTests(project);
  emit(file, kind, source.parent_path() / ("restored-" + surface + ".f90"));
  SageInterface::deleteAST(project);
}
