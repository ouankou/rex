#include <rose.h>

#include "Rose/FileSystem.h"
#include "SageTreeBuilder.h"
#include "astJson/sageAstJson.h"
#include "astJson/sageAstJsonPrivate.h"

#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {
using Rose::builder::SageTreeBuilder;
using Rose::builder::SourcePosition;

const SourcePosition kStart{
    "/tmp/rex_flang_source_position.f90",         2, 3, 0, std::nullopt,
    Rose::builder::FlangSourcePointIdentity{1, 1}};
const SourcePosition kEnd{"/tmp/rex_flang_source_position.f90",
                          2,
                          8,
                          std::nullopt,
                          std::nullopt,
                          Rose::builder::FlangSourcePointIdentity{2, 1}};

SgNullStatement *buildSourceNode() {
  SgNullStatement *node = SageBuilder::buildNullStatement_nfi();
  ROSE_ASSERT(node != nullptr);
  ROSE_ASSERT(node->get_startOfConstruct() == nullptr);
  ROSE_ASSERT(node->get_endOfConstruct() == nullptr);
  return node;
}

bool hasExactRange(const SgLocatedNode *node) {
  const Sg_File_Info *start = node->get_startOfConstruct();
  const Sg_File_Info *end = node->get_endOfConstruct();
  return start != nullptr && end != nullptr && start->get_parent() == node &&
         end->get_parent() == node &&
         start->get_raw_filename() == kStart.path &&
         end->get_raw_filename() == kEnd.path &&
         start->get_raw_line() == kStart.line &&
         start->get_raw_col() == kStart.column &&
         end->get_raw_line() == kEnd.line &&
         end->get_raw_col() == kEnd.column - 1 &&
         start->get_source_sequence_number() ==
             kStart.lexical_identity->order &&
         end->get_source_sequence_number() == kEnd.lexical_identity->order &&
         start->get_physical_file_occurrence_id() ==
             kStart.lexical_identity->physical_occurrence &&
         end->get_physical_file_occurrence_id() ==
             kEnd.lexical_identity->physical_occurrence &&
         !start->isSourcePositionUnavailableInFrontend() &&
         !end->isSourcePositionUnavailableInFrontend();
}

bool hasExactExpressionRange(const SgExpression *expression) {
  ROSE_ASSERT(expression != nullptr);
  const Sg_File_Info *start = expression->get_startOfConstruct();
  const Sg_File_Info *end = expression->get_endOfConstruct();
  const Sg_File_Info *operatorPosition = expression->get_operatorPosition();
  return hasExactRange(expression) && operatorPosition != nullptr &&
         operatorPosition != start && operatorPosition != end &&
         expression->get_file_info() == operatorPosition &&
         operatorPosition->get_parent() == expression &&
         operatorPosition->get_raw_filename() == kStart.path &&
         operatorPosition->get_raw_line() == kStart.line &&
         operatorPosition->get_raw_col() == kStart.column &&
         operatorPosition->get_source_sequence_number() ==
             kStart.lexical_identity->order &&
         operatorPosition->get_physical_file_occurrence_id() ==
             kStart.lexical_identity->physical_occurrence;
}

void checkRecordedSourceIdentity() {
  namespace fs = std::filesystem;
  const auto directory = Rose::FileSystem::createTemporaryDirectory();
  const auto original = directory / "snapshot.f90";
  const auto replacement = directory / "replacement.f90";
  for (const auto &path : {original, replacement}) {
    std::ofstream output(path);
    output << "program snapshot\n  continue\nend\n";
    ROSE_ASSERT(output.good());
  }
  SourcePosition start = kStart;
  SourcePosition end = kEnd;
  start.path = end.path = original.string();
  SgSourceFile source;
  source.set_file_info(new Sg_File_Info(start.path, 1, 1));
  source.set_sourceFileNameWithPath(start.path);
  source.set_sourceFileNameWithoutPath(original.filename().string());
  source.set_Fortran_only(true);
  SageTreeBuilder builder;
  builder.setSourceFile(&source);
  auto *before = buildSourceNode();
  builder.setSourcePosition(before, start, end);
  ROSE_ASSERT(fs::remove(original));
  fs::create_symlink(replacement.filename(), original);
  auto *after = buildSourceNode();
  builder.setSourcePosition(after, start, end);
  for (auto *node : {before, after}) {
    ROSE_ASSERT(node->get_startOfConstruct()->get_raw_filename() == start.path);
    ROSE_ASSERT(node->get_endOfConstruct()->get_raw_filename() == end.path);
    ROSE_ASSERT(node->get_file_info()->get_physical_file_id() ==
                source.get_file_info()->get_physical_file_id());
    SageInterface::deepDelete(node);
  }
  fs::remove_all(directory);
}

struct CompilerInlinePragma {
  int line;
  const char *raw;
  const char *syntax;
  unsigned member;
  unsigned count;
};

const std::vector<CompilerInlinePragma> kCompilerInlinePragmas{
    {5, "!dir$ unroll ! REX_COMPILER_INLINE_COMMENT", "!dir$ unroll", 0, 1},
    {10, "!dir$ unroll REX_EMPTY(\"!\") ! REX_COMPILER_EMPTY_INLINE_COMMENT",
     "!dir$ unroll REX_EMPTY(\"!\")", 0, 1},
    {14, "!dir$ un&", "!dir$ un&", 0, 2},
    {17, "!dir$ &roll ! REX_COMPILER_CONTINUED_INLINE_COMMENT", "!dir$ &roll",
     1, 2},
    {22, "!dir$ un&", "!dir$ un&", 0, 2},
    {23, "REX_REST ! REX_COMPILER_MACRO_INLINE_COMMENT", "REX_REST", 1, 2}};

struct CompilerInlineComment {
  int line;
  int column;
  const char *text;
};

const std::vector<CompilerInlineComment> kCompilerInlineComments{
    {5, 14, "! REX_COMPILER_INLINE_COMMENT"},
    {10, 29, "! REX_COMPILER_EMPTY_INLINE_COMMENT"},
    {15, 1, "! REX_COMPILER_GAP_COMMENT"},
    {17, 13, "! REX_COMPILER_CONTINUED_INLINE_COMMENT"},
    {23, 10, "! REX_COMPILER_MACRO_INLINE_COMMENT"}};

struct CompilerPragmaSnapshot {
  std::string pragma_location;
  std::string payload_location;
  Rose::AstJson::JsonValue pragma_properties;

  bool operator==(const CompilerPragmaSnapshot &other) const {
    return pragma_location == other.pragma_location &&
           payload_location == other.payload_location &&
           pragma_properties.exactlyEquals(other.pragma_properties);
  }
};

struct CompilerInlineSnapshot {
  SgSourceFile::fortran_source_context context;
  std::vector<CompilerPragmaSnapshot> pragmas;
  std::vector<Rose::AstJson::JsonValue> trivia;

  bool operator==(const CompilerInlineSnapshot &other) const {
    if (!(context == other.context) || pragmas != other.pragmas ||
        trivia.size() != other.trivia.size())
      return false;
    for (size_t index = 0; index < trivia.size(); ++index)
      if (!trivia[index].exactlyEquals(other.trivia[index]))
        return false;
    return true;
  }
};

CompilerInlineSnapshot checkCompilerInlineComment(SgSourceFile *file) {
  using namespace Rose::AstJson;
  ROSE_ASSERT(file != nullptr);
  const auto path =
      std::filesystem::canonical(file->get_sourceFileNameWithPath()).string();
  const auto &context = file->get_fortran_source_context();
  ROSE_ASSERT(context.root_occurrence > 0 && context.root_entry > 0 &&
              context.root_entry < context.root_exit &&
              context.root_path == path);
  const auto point = [&](const Sg_File_Info *position, int line, int column) {
    ROSE_ASSERT(
        position != nullptr && position->get_physical_filename() == path &&
        position->get_physical_line() == line &&
        position->get_raw_line() == line && position->get_col() == column &&
        position->get_raw_col() == column);
    ROSE_ASSERT(position->get_source_sequence_number() > context.root_entry &&
                position->get_source_sequence_number() < context.root_exit &&
                position->get_physical_file_occurrence_id() ==
                    context.root_occurrence);
  };
  const auto pragmas = NodeQuery::querySubTree(file, V_SgPragmaDeclaration);
  ROSE_ASSERT(pragmas.size() == kCompilerInlinePragmas.size());
  for (size_t index = 0; index < pragmas.size(); ++index) {
    auto *pragma = isSgPragmaDeclaration(pragmas[index]);
    const auto &expected = kCompilerInlinePragmas[index];
    ROSE_ASSERT(pragma->get_fortran_directive_family() ==
                SgPragmaDeclaration::e_fortran_directive_compiler);
    ROSE_ASSERT(
        pragma->get_fortran_directive_member_count() == expected.count &&
        pragma->get_fortran_directive_member_index() == expected.member &&
        pragma->get_fortran_directive_primary() == (expected.member == 0));
    ROSE_ASSERT(pragma->get_fortran_compiler_directive_sentinel() == "dir$");
    ROSE_ASSERT(pragma->get_fortran_directive_raw_text() == expected.raw);
    ROSE_ASSERT(pragma->get_pragma() != nullptr &&
                pragma->get_pragma()->get_pragma() == expected.syntax);
    ROSE_ASSERT(pragma->get_fortran_selected_output_text() ==
                (expected.member == 0 ? "!dir$ unroll" : ""));
    ROSE_ASSERT(SgPragmaDeclaration::fortran_selected_output_is_valid(
        pragma->get_fortran_selected_output_text(),
        pragma->get_fortran_selected_output_tokens()));
    if (expected.member != 0)
      ROSE_ASSERT(pragma->get_fortran_directive_group_id() ==
                  isSgPragmaDeclaration(pragmas[index - 1])
                      ->get_fortran_directive_group_id());
    const auto checkRange = [&](const auto *node) {
      point(node->get_startOfConstruct(), expected.line, 1);
      point(node->get_endOfConstruct(), expected.line,
            static_cast<int>(std::strlen(expected.raw)));
    };
    checkRange(pragma);
    checkRange(pragma->get_pragma());
  }

  std::vector<const PreprocessingInfo *> comments(
      kCompilerInlineComments.size());
  const PreprocessingInfo *blank = nullptr;
  std::map<const PreprocessingInfo *, SgLocatedNode *> owners;
  for (SgNode *node : NodeQuery::querySubTree(file, V_SgLocatedNode)) {
    auto *located = isSgLocatedNode(node);
    const auto *entries = located->getAttachedPreprocessingInfo();
    if (entries == nullptr)
      continue;
    for (const auto *entry : *entries) {
      ROSE_ASSERT(entry != nullptr);
      if (entry->getFortranSourceRole() ==
              PreprocessingInfo::FortranSourceRole::layout &&
          entry->get_file_info()->get_physical_line() == 16) {
        ROSE_ASSERT(blank == nullptr && entry->getString() == "\n");
        ROSE_ASSERT(entry->getTypeOfDirective() ==
                    PreprocessingInfo::CSkippedToken);
        ROSE_ASSERT(entry->getAttachedOwner() == located &&
                    entry->get_file_info()->get_parent() == located);
        point(entry->get_file_info(), 16, 1);
        blank = entry;
        owners.emplace(entry, located);
        continue;
      }
      if (entry->getString().find("REX_COMPILER_") == std::string::npos)
        continue;
      const auto found = std::find_if(
          kCompilerInlineComments.begin(), kCompilerInlineComments.end(),
          [&](const auto &expected) {
            return entry->getString() == expected.text;
          });
      ROSE_ASSERT(found != kCompilerInlineComments.end());
      const size_t index =
          std::distance(kCompilerInlineComments.begin(), found);
      ROSE_ASSERT(comments[index] == nullptr);
      ROSE_ASSERT(entry->getFortranSourceRole() ==
                  PreprocessingInfo::FortranSourceRole::comment);
      ROSE_ASSERT(entry->getTypeOfDirective() ==
                  PreprocessingInfo::F90StyleComment);
      ROSE_ASSERT(entry->getAttachedOwner() == located &&
                  entry->get_file_info()->get_parent() == located);
      point(entry->get_file_info(), found->line, found->column);
      comments[index] = entry;
      owners.emplace(entry, located);
    }
  }
  ROSE_ASSERT(blank != nullptr &&
              std::all_of(comments.begin(), comments.end(),
                          [](const auto *p) { return p != nullptr; }));
  ROSE_ASSERT(isSgPragmaDeclaration(pragmas[2])
                      ->get_endOfConstruct()
                      ->get_source_sequence_number() <
                  comments[2]->get_file_info()->get_source_sequence_number() &&
              comments[2]->get_file_info()->get_source_sequence_number() <
                  blank->get_file_info()->get_source_sequence_number() &&
              blank->get_file_info()->get_source_sequence_number() <
                  isSgPragmaDeclaration(pragmas[3])
                      ->get_startOfConstruct()
                      ->get_source_sequence_number());
  NodeMap nodes;
  const auto ast = parseAstFileJson(
      buildJson(file, Checkpoint::PreOmpConstruction, file, nullptr, &nodes),
      checkpointName(Checkpoint::PreOmpConstruction));
  CompilerInlineSnapshot result{context, {}, {}};
  for (SgNode *node : pragmas) {
    const auto record =
        std::find_if(nodes.begin(), nodes.end(),
                     [&](const auto &entry) { return entry.second == node; });
    ROSE_ASSERT(record != nodes.end());
    auto *pragma = isSgPragmaDeclaration(node);
    result.pragmas.push_back({rawLocationJson(pragma),
                              rawLocationJson(pragma->get_pragma()),
                              ast.node(record->first).properties});
  }
  comments.push_back(blank);
  for (const auto *comment : comments) {
    const auto owner =
        std::find_if(nodes.begin(), nodes.end(), [&](const auto &entry) {
          return entry.second == owners.at(comment);
        });
    ROSE_ASSERT(owner != nodes.end());
    const auto &entries = ast.node(owner->first).preprocessing.array;
    const auto record =
        std::find_if(entries.begin(), entries.end(), [&](const auto &entry) {
          return entry.at("text").asString() == comment->getString() &&
                 entry.at("file_info").at("physical_line").asInt() ==
                     comment->get_file_info()->get_physical_line();
        });
    ROSE_ASSERT(record != entries.end());
    result.trivia.push_back(*record);
  }
  return result;
}

void writeCompilerInlineSurface(SgSourceFile *file,
                                const std::filesystem::path &output) {
  std::filesystem::remove(output);
  file->set_unparse_output_filename(output.string());
  file->unparse();
  const auto text = Rose::StringUtility::readFile(output.string());
  const auto count = [&](const std::string &needle) {
    size_t matches = 0;
    for (size_t at = text.find(needle); at != std::string::npos;
         at = text.find(needle, at + needle.size()))
      ++matches;
    return matches;
  };
  for (const auto &comment : kCompilerInlineComments)
    ROSE_ASSERT(count(comment.text) == 1);
  const std::string directive = "!dir$ unroll";
  const bool selected =
      file->get_fortran_source_context().output_surface ==
      SgSourceFile::fortran_output_surface::selected_translation_unit;
  ROSE_ASSERT(count(directive) == (selected ? 4 : 2));
  if (selected) {
    ROSE_ASSERT(text.find("REX_EMPTY") == std::string::npos);
    ROSE_ASSERT(text.find("REX_REST") == std::string::npos);
    ROSE_ASSERT(count("!dir$ un&") == 0 && count("!dir$ &roll") == 0);
    const auto first = text.find(directive);
    const auto second = text.find(directive, first + directive.size());
    const auto third = text.find(directive, second + directive.size());
    ROSE_ASSERT(third != std::string::npos &&
                third < text.find("! REX_COMPILER_GAP_COMMENT"));
  } else {
    ROSE_ASSERT(count("!dir$ un&") == 2 && count("!dir$ &roll") == 2);
    ROSE_ASSERT(
        text.find("!dir$ un&") < text.find("! REX_COMPILER_GAP_COMMENT") &&
        text.find("! REX_COMPILER_GAP_COMMENT") < text.find("!dir$ &roll"));
    ROSE_ASSERT(count("REX_EMPTY(\"!\")") == 1);
    ROSE_ASSERT(count("REX_REST") == 2);
  }
}

struct ExactSourceRecord {
  uint64_t id;
  std::string kind;
  Rose::AstJson::JsonValue location;
  Rose::AstJson::JsonValue preprocessing;

  bool operator==(const ExactSourceRecord &other) const {
    return id == other.id && kind == other.kind &&
           location.exactlyEquals(other.location) &&
           preprocessing.exactlyEquals(other.preprocessing);
  }
};

struct ExactSourceSnapshot {
  SgSourceFile::fortran_source_context context;
  std::vector<ExactSourceRecord> records;

  bool operator==(const ExactSourceSnapshot &other) const {
    if (!(context == other.context)) {
      std::cerr << "source snapshot context mismatch\n";
      return false;
    }
    if (records.size() != other.records.size()) {
      std::cerr << "source snapshot node-count mismatch: actual="
                << records.size() << " expected=" << other.records.size()
                << '\n';
      return false;
    }
    for (size_t index = 0; index < records.size(); ++index) {
      const auto &actual = records[index];
      const auto &expected = other.records[index];
      if (!(actual == expected)) {
        std::cerr << "source snapshot record mismatch at " << index
                  << ": actual=" << actual.id << '/' << actual.kind
                  << " expected=" << expected.id << '/' << expected.kind
                  << " location_equal="
                  << actual.location.exactlyEquals(expected.location)
                  << " preprocessing_equal="
                  << actual.preprocessing.exactlyEquals(expected.preprocessing)
                  << '\n';
        return false;
      }
    }
    return true;
  }
};

const std::vector<std::string> kEntryComments{"! REX_PROCEDURE_HEADER_COMMENT",
                                              "! REX_ENTRY_INLINE_COMMENT",
                                              "! REX_PROCEDURE_END_COMMENT"};

ExactSourceSnapshot checkSourcePredicate(SgSourceFile *file, bool entry) {
  using namespace Rose::AstJson;
  ROSE_ASSERT(file != nullptr);
  const auto path =
      std::filesystem::canonical(file->get_sourceFileNameWithPath()).string();
  const auto &context = file->get_fortran_source_context();
  ROSE_ASSERT(context.root_path == path && context.root_occurrence > 0 &&
              context.root_entry > 0 && context.root_entry < context.root_exit);
  if (entry) {
    const auto original = Rose::StringUtility::readFile(path);
    std::vector<unsigned> counts(kEntryComments.size());
    for (SgNode *node : NodeQuery::querySubTree(file, V_SgLocatedNode)) {
      auto *owner = isSgLocatedNode(node);
      const auto *comments = owner->getAttachedPreprocessingInfo();
      if (comments == nullptr)
        continue;
      for (const auto *comment : *comments) {
        ROSE_ASSERT(comment != nullptr);
        for (size_t index = 0; index < kEntryComments.size(); ++index) {
          const auto &text = kEntryComments[index];
          if (comment->getString().find(text) == std::string::npos)
            continue;
          ++counts[index];
          ROSE_ASSERT(comment->getString() == text && counts[index] == 1);
          const size_t at = original.find(text);
          ROSE_ASSERT(at != std::string::npos &&
                      original.find(text, at + text.size()) ==
                          std::string::npos);
          const int line =
              1 + std::count(original.begin(), original.begin() + at, '\n');
          const size_t previous = original.rfind('\n', at);
          const int column =
              at - (previous == std::string::npos ? 0 : previous + 1) + 1;
          const auto *position = comment->get_file_info();
          ROSE_ASSERT(comment->getFortranSourceRole() ==
                          PreprocessingInfo::FortranSourceRole::comment &&
                      comment->getTypeOfDirective() ==
                          PreprocessingInfo::F90StyleComment &&
                      comment->getAttachedOwner() == owner &&
                      position != nullptr && position->get_parent() == owner);
          ROSE_ASSERT(
              position->get_raw_filename() == path &&
              position->get_physical_filename() == path &&
              position->get_raw_line() == line &&
              position->get_physical_line() == line &&
              position->get_raw_col() == column &&
              position->get_col() == column &&
              position->get_physical_file_occurrence_id() ==
                  context.root_occurrence &&
              position->get_source_sequence_number() > context.root_entry &&
              position->get_source_sequence_number() < context.root_exit);
          if (index == 1) {
            ROSE_ASSERT(
                isSgEntryStatement(owner) != nullptr &&
                comment->getRelativePosition() == PreprocessingInfo::after &&
                comment->getOutputPlacement() ==
                    PreprocessingInfo::source_position &&
                owner->get_startOfConstruct() != nullptr &&
                owner->get_endOfConstruct() != nullptr &&
                owner->get_startOfConstruct()->get_physical_line() == line &&
                owner->get_endOfConstruct()->get_physical_line() == line &&
                owner->get_endOfConstruct()->get_raw_col() < column);
          }
        }
      }
    }
    ROSE_ASSERT(std::all_of(counts.begin(), counts.end(),
                            [](unsigned count) { return count == 1; }));
  } else {
    const std::string expected_text = "hello world     !";
    const SgStringCodeUnitList expected(expected_text.begin(),
                                        expected_text.end());
    unsigned literals = 0;
    for (SgNode *node : NodeQuery::querySubTree(file, V_SgStringVal))
      if (isSgStringVal(node)->get_value() == expected)
        ++literals;
    ROSE_ASSERT(literals == 2);
  }
  // roundTripSourceFile snapshots the owning file list and preserves its IDs.
  // Use that same graph boundary before, after copying, and after JSON so IDs
  // describe equivalent graphs while every raw location/PP check stays exact.
  auto *files = isSgFileList(file->get_parent());
  ROSE_ASSERT(files != nullptr);
  const auto ast = parseAstFileJson(
      buildJson(file, Checkpoint::PreOmpConstruction, file, files),
      checkpointName(Checkpoint::PreOmpConstruction));
  ExactSourceSnapshot result{context, {}};
  for (const auto &record : ast.nodes)
    result.records.push_back(
        {record.id, record.kind, record.location, record.preprocessing});
  return result;
}

void writeSourcePredicateSurface(SgSourceFile *file,
                                 const std::filesystem::path &output,
                                 bool entry) {
  std::filesystem::remove(output);
  file->set_unparse_output_filename(output.string());
  file->unparse();
  if (!entry)
    return;
  const auto text = Rose::StringUtility::readFile(output.string());
  for (const auto &comment : kEntryComments) {
    const size_t at = text.find(comment);
    ROSE_ASSERT(at != std::string::npos &&
                text.find(comment, at + comment.size()) == std::string::npos);
  }
  const size_t at = text.find(kEntryComments[1]);
  const size_t previous = text.rfind('\n', at);
  auto line =
      text.substr(previous == std::string::npos ? 0 : previous + 1,
                  at - (previous == std::string::npos ? 0 : previous + 1));
  std::transform(line.begin(), line.end(), line.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  ROSE_ASSERT(line.find("entry rex_alternate(") != std::string::npos);
}
} // namespace

int main(int argc, char **argv) {
  if (argc == 4 &&
      (std::string(argv[1]) == "source-predicate-entry-preserve" ||
       std::string(argv[1]) == "source-predicate-entry-selected" ||
       std::string(argv[1]) == "source-predicate-character-preserve" ||
       std::string(argv[1]) == "source-predicate-character-selected")) {
    const std::string mode = argv[1];
    const bool entry = mode.find("-entry-") != std::string::npos;
    const auto directory = std::filesystem::absolute(argv[3]);
    std::filesystem::create_directories(directory);
    const std::vector<std::string> arguments{
        argv[0],      "-rose:skipfinalCompileStep",
        "-outputdir", directory.string(),
        "-c",         argv[2]};
    SgProject *project = frontend(arguments);
    ROSE_ASSERT(project != nullptr && project->numberOfFiles() == 1);
    auto *file = isSgSourceFile(project->get_fileList().front());
    ROSE_ASSERT(file != nullptr);
    if (mode.find("-selected") != std::string::npos) {
      auto context = file->get_fortran_source_context();
      context.output_surface =
          SgSourceFile::fortran_output_surface::selected_translation_unit;
      file->set_fortran_source_context(context);
    }
    const auto snapshot = checkSourcePredicate(file, entry);
    writeSourcePredicateSurface(file, directory / "live.f90", entry);
    SgTreeCopy copier(
        SgCopyHelp::ProvenancePolicy::PreserveExactFrontendSemantic);
    auto *copy = isSgProject(project->copy(copier));
    ROSE_ASSERT(copy != nullptr && copy->numberOfFiles() == 1);
    auto *copied_file = isSgSourceFile(copy->get_fileList().front());
    ROSE_ASSERT(checkSourcePredicate(copied_file, entry) == snapshot);
    writeSourcePredicateSurface(copied_file, directory / "copied.f90", entry);
    SageInterface::deleteAST(copy);
    Rose::AstJson::Options options;
    options.outputDirectory = (directory / "json").string();
    file = Rose::AstJson::roundTripSourceFile(
        file, Rose::AstJson::Checkpoint::PreOmpConstruction, options);
    ROSE_ASSERT(checkSourcePredicate(file, entry) == snapshot);
    writeSourcePredicateSurface(file, directory / "restored.f90", entry);
    SageInterface::deleteAST(project);
    return 0;
  }
  if (argc == 4 && (std::string(argv[1]) == "compiler-inline-preserve" ||
                    std::string(argv[1]) == "compiler-inline-selected")) {
    const auto directory = std::filesystem::absolute(argv[3]);
    std::filesystem::create_directories(directory);
    const std::vector<std::string> arguments{
        argv[0],      "-rose:skipfinalCompileStep",
        "-outputdir", directory.string(),
        "-c",         argv[2]};
    SgProject *project = frontend(arguments);
    ROSE_ASSERT(project != nullptr && project->numberOfFiles() == 1);
    auto *file = isSgSourceFile(project->get_fileList().front());
    ROSE_ASSERT(file != nullptr);
    if (std::string(argv[1]) == "compiler-inline-selected") {
      auto context = file->get_fortran_source_context();
      context.output_surface =
          SgSourceFile::fortran_output_surface::selected_translation_unit;
      file->set_fortran_source_context(context);
    }
    const auto snapshot = checkCompilerInlineComment(file);
    writeCompilerInlineSurface(file, directory / "live.F90");
    SgTreeCopy copier(
        SgCopyHelp::ProvenancePolicy::PreserveExactFrontendSemantic);
    auto *copy = isSgProject(project->copy(copier));
    ROSE_ASSERT(copy != nullptr && copy->numberOfFiles() == 1);
    auto *copied_file = isSgSourceFile(copy->get_fileList().front());
    ROSE_ASSERT(checkCompilerInlineComment(copied_file) == snapshot);
    writeCompilerInlineSurface(copied_file, directory / "copied.F90");
    SageInterface::deleteAST(copy);
    Rose::AstJson::Options options;
    options.outputDirectory = (directory / "json").string();
    file = Rose::AstJson::roundTripSourceFile(
        file, Rose::AstJson::Checkpoint::PreOmpConstruction, options);
    ROSE_ASSERT(checkCompilerInlineComment(file) == snapshot);
    writeCompilerInlineSurface(file, directory / "restored.F90");
    SageInterface::deleteAST(project);
    return 0;
  }
  if (argc == 3 && std::string(argv[1]) == "directive-source") {
    const std::string path = std::filesystem::canonical(argv[2]).string();
    const std::vector<std::string> arguments{
        argv[0], "-rose:skipfinalCompileStep", "-c", argv[2]};
    SgProject *project = frontend(arguments);
    const auto checkRange = [&](const auto *node) {
      const auto *start = node->get_startOfConstruct();
      const auto *end = node->get_endOfConstruct();
      ROSE_ASSERT(start != nullptr && end != nullptr);
      ROSE_ASSERT(start->get_parent() == node && end->get_parent() == node);
      ROSE_ASSERT(start->get_raw_filename() == path &&
                  end->get_raw_filename() == path);
      ROSE_ASSERT(start->get_physical_filename() == path &&
                  end->get_physical_filename() == path);
      ROSE_ASSERT(start->get_raw_line() == 6 && end->get_raw_line() == 6);
      ROSE_ASSERT(start->get_raw_col() == 1 && end->get_raw_col() == 12);
      ROSE_ASSERT(!start->isCompilerGenerated() && !end->isCompilerGenerated());
    };
    const auto checkProject = [&](SgProject *candidate) {
      const auto directives =
          NodeQuery::querySubTree(candidate, V_SgPragmaDeclaration);
      ROSE_ASSERT(directives.size() == 1);
      auto *directive = isSgPragmaDeclaration(directives.front());
      ROSE_ASSERT(directive->get_fortran_directive_family() ==
                  SgPragmaDeclaration::e_fortran_directive_compiler);
      ROSE_ASSERT(directive->get_pragma()->get_parent() == directive);
      ROSE_ASSERT(directive->get_pragma()->get_pragma() == "!dir$ unroll");
      checkRange(directive);
      checkRange(directive->get_pragma());
    };
    checkProject(project);
    SgTreeCopy copier;
    auto *copy = isSgProject(project->copy(copier));
    ROSE_ASSERT(copy != nullptr);
    checkProject(copy);
    SageInterface::deleteAST(copy);
    const auto directory = Rose::FileSystem::createTemporaryDirectory();
    Rose::AstJson::Options options;
    options.outputDirectory = directory.string();
    auto *source = isSgSourceFile(project->get_fileList().front());
    source = Rose::AstJson::roundTripSourceFile(
        source, Rose::AstJson::Checkpoint::PreOmpConstruction, options);
    checkProject(project);
    const auto output = directory / "directive.F90";
    source->set_unparse_output_filename(output.string());
    source->unparse();
    ROSE_ASSERT(Rose::StringUtility::readFile(output.string())
                    .find("\n!dir$ unroll\n") != std::string::npos);
    SageInterface::deleteAST(project);
    std::filesystem::remove_all(directory);
    return 0;
  }
  if (argc != 2) {
    std::cerr << "usage: " << argv[0] << " MODE\n";
    return 2;
  }

  SageBuilder::setSourcePositionClassificationMode(
      SageBuilder::e_sourcePositionFrontendConstruction);
  SageTreeBuilder builder;
  const std::string mode = argv[1];

  if (mode == "valid-source") {
    checkRecordedSourceIdentity();
    SgNullStatement *node = buildSourceNode();
    builder.setSourcePosition(node, kStart, kEnd);
    return hasExactRange(node) ? 0 : 1;
  }
  if (mode == "valid-pending-source") {
    SgNullStatement *node = buildSourceNode();
    SageInterface::setSourcePosition(node);
    ROSE_ASSERT(node->get_startOfConstruct() != nullptr);
    ROSE_ASSERT(node->get_endOfConstruct() != nullptr);
    ROSE_ASSERT(
        node->get_startOfConstruct()->isSourcePositionUnavailableInFrontend());
    ROSE_ASSERT(
        node->get_endOfConstruct()->isSourcePositionUnavailableInFrontend());
    builder.setSourcePosition(node, kStart, kEnd);
    return hasExactRange(node) ? 0 : 1;
  }
  if (mode == "valid-source-expression") {
    SgExpression *expression = SageBuilder::buildIntVal_nfi(7);
    ROSE_ASSERT(expression != nullptr);
    ROSE_ASSERT(expression->get_startOfConstruct() == nullptr);
    ROSE_ASSERT(expression->get_endOfConstruct() == nullptr);
    ROSE_ASSERT(expression->get_operatorPosition() == nullptr);
    builder.setSourcePosition(expression, kStart, kEnd);
    return hasExactExpressionRange(expression) ? 0 : 1;
  }
  if (mode == "valid-pending-source-expression") {
    SgExpression *expression = SageBuilder::buildIntVal_nfi(7);
    ROSE_ASSERT(expression != nullptr);
    SageInterface::setSourcePosition(expression);
    ROSE_ASSERT(expression->get_startOfConstruct() != nullptr);
    ROSE_ASSERT(expression->get_endOfConstruct() != nullptr);
    ROSE_ASSERT(expression->get_operatorPosition() != nullptr);
    builder.setSourcePosition(expression, kStart, kEnd);
    return hasExactExpressionRange(expression) ? 0 : 1;
  }
  if (mode == "valid-source-attribute") {
    SgAttributeSpecificationStatement *statement =
        SageBuilder::buildAttributeSpecificationStatement_nfi(
            SgAttributeSpecificationStatement::e_parameterStatement);
    ROSE_ASSERT(statement != nullptr);
    ROSE_ASSERT(statement->get_startOfConstruct() == nullptr);
    ROSE_ASSERT(statement->get_endOfConstruct() == nullptr);
    SgExprListExp *parameters = statement->get_parameter_list();
    ROSE_ASSERT(parameters != nullptr);
    ROSE_ASSERT(parameters->get_parent() == statement);
    builder.setSourcePosition(statement, kStart, kEnd);
    ROSE_ASSERT(hasExactRange(statement));
    ROSE_ASSERT(statement->get_translation_unit_source_order() == 1);
    ROSE_ASSERT(parameters->get_startOfConstruct() != nullptr);
    ROSE_ASSERT(parameters->get_endOfConstruct() != nullptr);
    ROSE_ASSERT(parameters->get_startOfConstruct()->isCompilerGenerated());
    ROSE_ASSERT(parameters->get_endOfConstruct()->isCompilerGenerated());
    ROSE_ASSERT(!parameters->get_startOfConstruct()->isTransformation());
    ROSE_ASSERT(!parameters->get_endOfConstruct()->isTransformation());
    return 0;
  }
  if (mode == "valid-physical-output-owner") {
    SgSourceFile source;
    source.set_file_info(new Sg_File_Info(kStart.path, 1, 1));
    source.set_sourceFileNameWithPath(kStart.path);
    source.set_sourceFileNameWithoutPath("rex_flang_source_position.f90");
    source.set_Fortran_only(true);
    builder.setSourceFile(&source);

    SgNullStatement *primary = buildSourceNode();
    builder.setSourcePosition(primary, kStart, kEnd);
    const SourcePosition includeStart{
        "/tmp/rex_flang_include.inc",
        4,
        2,
        std::nullopt,
        std::nullopt,
        Rose::builder::FlangSourcePointIdentity{3, 2}};
    const SourcePosition includeEnd{
        "/tmp/rex_flang_include.inc",
        4,
        7,
        std::nullopt,
        std::nullopt,
        Rose::builder::FlangSourcePointIdentity{4, 2}};
    SgNullStatement *included = buildSourceNode();
    builder.setSourcePosition(included, includeStart, includeEnd);
    return primary->get_file_info()->get_physical_file_id() ==
                       source.get_file_info()->get_physical_file_id() &&
                   included->get_file_info()->get_physical_file_id() !=
                       source.get_file_info()->get_physical_file_id() &&
                   primary->get_file_info()->isOutputInCodeGeneration() &&
                   included->get_file_info()->isOutputInCodeGeneration()
               ? 0
               : 1;
  }
  if (mode == "valid-generated-object") {
    SgSourceFile source;
    source.set_file_info(new Sg_File_Info(kStart.path, 1, 1));
    source.set_sourceFileNameWithPath(kStart.path);
    source.set_sourceFileNameWithoutPath("rex_flang_source_position.f90");
    source.set_Fortran_only(true);
    builder.setSourceFile(&source);

    SgVariableDeclaration *declaration = new SgVariableDeclaration(
        SgName("generated_object"), SageBuilder::buildIntType(), nullptr);
    ROSE_ASSERT(declaration != nullptr);
    SageInterface::setOneSourcePositionNull(declaration);
    builder.setGeneratedSourcePosition(
        declaration, kStart, kEnd,
        SageTreeBuilder::GeneratedSourceAnchorKind::
            use_associated_object_declaration);
    const Sg_File_Info *start = declaration->get_startOfConstruct();
    const Sg_File_Info *end = declaration->get_endOfConstruct();
    const bool valid =
        hasExactRange(declaration) && start->isCompilerGenerated() &&
        end->isCompilerGenerated() && start->isOutputInCodeGeneration() &&
        end->isOutputInCodeGeneration();
    builder.setTokens({Rose::builder::Token(
        Rose::builder::TokenKind::comment,
        Rose::builder::PreprocessingDirectiveKind::none, kStart.path, 3, 1, 3,
        21, "! rex_source_comment\n",
        Rose::builder::FlangSourcePointIdentity{3, 1},
        Rose::builder::FortranSourceTokenRole::comment)});
    builder.attachRemainingComments(declaration);
    const auto *generatedComments = declaration->getAttachedPreprocessingInfo();
    ROSE_ASSERT(generatedComments == nullptr || generatedComments->empty());
    SgNullStatement *written = buildSourceNode();
    builder.setSourcePosition(written, kStart, kEnd);
    builder.attachRemainingComments(written);
    const auto *writtenComments = written->getAttachedPreprocessingInfo();
    ROSE_ASSERT(writtenComments != nullptr && writtenComments->size() == 1);
    ROSE_ASSERT(writtenComments->front()->getString() ==
                "! rex_source_comment\n");
    ROSE_ASSERT(writtenComments->front()
                    ->get_file_info()
                    ->get_source_sequence_number() == 3);
    ROSE_ASSERT(writtenComments->front()
                    ->get_file_info()
                    ->get_physical_file_occurrence_id() == 1);
    SageInterface::deepDelete(written);
    SageInterface::deepDelete(declaration);
    return valid ? 0 : 1;
  }
  if (mode == "missing-start") {
    builder.setSourcePosition(buildSourceNode(), SourcePosition{}, kEnd);
  } else if (mode == "partial-start") {
    builder.setSourcePosition(
        buildSourceNode(),
        SourcePosition{"/tmp/rex_flang_source_position.f90", 0, 3}, kEnd);
  } else if (mode == "different-files") {
    builder.setSourcePosition(buildSourceNode(), kStart,
                              SourcePosition{"/tmp/rex_flang_other.f90", 2, 8});
  } else if (mode == "reversed-range") {
    builder.setSourcePosition(buildSourceNode(), kEnd, kStart);
  } else if (mode == "preclassified-statement") {
    builder.setSourcePosition(SageBuilder::buildNullStatement(), kStart, kEnd);
  } else if (mode == "preclassified-expression-operator") {
    SgExpression *expression = SageBuilder::buildIntVal_nfi(7);
    ROSE_ASSERT(expression != nullptr);
    Sg_File_Info *operatorPosition =
        Sg_File_Info::generateDefaultFileInfoForTransformationNode();
    ROSE_ASSERT(operatorPosition != nullptr);
    expression->set_operatorPosition(operatorPosition);
    operatorPosition->set_parent(expression);
    builder.setSourcePosition(expression, kStart, kEnd);
  } else if (mode == "wrong-generated-kind") {
    builder.setGeneratedSourcePosition(
        buildSourceNode(), kStart, kEnd,
        SageTreeBuilder::GeneratedSourceAnchorKind::
            use_associated_object_declaration);
  } else if (mode == "missing-comment-owner" ||
             mode == "pending-comment-owner") {
    SgSourceFile source;
    source.set_file_info(new Sg_File_Info(kStart.path, 1, 1));
    builder.setSourceFile(&source);
    SgNullStatement *node = buildSourceNode();
    if (mode == "pending-comment-owner") {
      SageInterface::setSourcePosition(node);
    }
    builder.attachRemainingComments(node);
  } else if (mode == "missing-output-owner" ||
             mode == "ambiguous-output-owner") {
    SgSourceFile source;
    source.set_file_info(mode == "missing-output-owner"
                             ? Sg_File_Info::generateDefaultFileInfo()
                             : new Sg_File_Info(kStart.path, 1, 1));
    if (mode == "ambiguous-output-owner") {
      source.get_file_info()->setShared();
    }
    source.set_sourceFileNameWithPath(kStart.path);
    source.set_sourceFileNameWithoutPath("rex_flang_source_position.f90");
    source.set_Fortran_only(true);
    builder.setSourceFile(&source);
    builder.setSourcePosition(buildSourceNode(), kStart, kEnd);
  } else {
    std::cerr << "unknown mode: " << mode << '\n';
    return 2;
  }

  std::cerr << "source-position contract unexpectedly returned\n";
  return 1;
}
