#include "FortranSourceContext.h"
#include "astJson/sageAstJson.h"
#include "astJson/sageAstJsonPrivate.h"
#include "nodeQuery.h"
#include "rose.h"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <set>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

namespace {
namespace fs = std::filesystem;
using Context = SgSourceFile::fortran_source_context;

std::string read(const fs::path &path) {
  std::ifstream stream(path, std::ios::binary);
  ROSE_ASSERT(stream.good());
  return {std::istreambuf_iterator<char>(stream), {}};
}

struct Snapshot {
  Context context;
  std::vector<std::string> locations;
  std::vector<std::string> preprocessing;

  bool operator==(const Snapshot &other) const {
    return context == other.context && locations == other.locations &&
           preprocessing == other.preprocessing;
  }
};

Snapshot snapshot(SgSourceFile *file) {
  Snapshot result{file->get_fortran_source_context(), {}, {}};
  Rose::FortranSource::validate(file);
  for (auto *node : NodeQuery::querySubTree(file, V_SgNode)) {
    result.locations.push_back(node->class_name() +
                               Rose::AstJson::rawLocationJson(node));
    auto *located = isSgLocatedNode(node);
    if (located == nullptr)
      continue;
    located->validateAttachedPreprocessingInfoOwnership();
    if (const auto *records = located->getAttachedPreprocessingInfo())
      for (const auto *record : *records) {
        ROSE_ASSERT(record != nullptr &&
                    record->getAttachedOwner() == located &&
                    record->get_file_info()->get_parent() == located);
        std::ostringstream out;
        out << record->getTypeOfDirective() << ':'
            << record->getRelativePosition() << ':'
            << record->getOutputPlacement() << ':'
            << static_cast<int>(record->getFortranSourceRole()) << ':'
            << record->getNumberOfLines() << ':' << record->isTransformation()
            << ':' << record->getString() << ':'
            << Rose::AstJson::rawFileInfoJson(record->get_file_info());
        result.preprocessing.push_back(out.str());
      }
  }
  return result;
}

void checkPoint(const Sg_File_Info *before, const Sg_File_Info *after,
                const Context &source, const std::string &destination) {
  if (before == nullptr || after == nullptr) {
    ROSE_ASSERT(before == nullptr && after == nullptr);
    return;
  }
  ROSE_ASSERT(before != after &&
              before->get_source_sequence_number() ==
                  after->get_source_sequence_number() &&
              before->get_physical_file_occurrence_id() ==
                  after->get_physical_file_occurrence_id() &&
              before->get_raw_line() == after->get_raw_line() &&
              before->get_raw_col() == after->get_raw_col() &&
              before->get_physical_line() == after->get_physical_line() &&
              before->get_classificationBitField() ==
                  after->get_classificationBitField());
  const bool root =
      before->get_physical_filename() == source.root_path &&
      (before->get_physical_file_occurrence_id() == 0 ||
       before->get_physical_file_occurrence_id() == source.root_occurrence);
  ROSE_ASSERT(after->get_physical_filename() ==
              (root ? destination : before->get_physical_filename()));
}

struct StructuralNodes : AstSimpleProcessing {
  std::vector<SgNode *> nodes;

  void visit(SgNode *node) override { nodes.push_back(node); }
};

void checkTransfer(SgSourceFile *original, SgSourceFile *copy,
                   const SgTreeCopy &copier, const std::string &requested,
                   const std::string &destination, bool relocated) {
  const auto before = original->get_fortran_source_context();
  auto expected = before;
  expected.root_path = destination;
  // A relocation may need selected emission to preserve include semantics.
  // It must still retain the same immutable lexical occurrence tree.
  if (relocated)
    expected.output_surface = copy->get_fortran_output_surface();
  ROSE_ASSERT(copy->get_fortran_source_context() == expected &&
              copy->get_sourceFileNameWithPath() ==
                  original->get_sourceFileNameWithPath() &&
              copy->get_file_info()->get_filenameString() == requested &&
              copy->get_file_info()->get_physical_filename() == destination);
  const auto &nodes = copier.get_copiedNodeMap();
  StructuralNodes structural;
  structural.traverse(original, preorder);
  for (auto *node : structural.nodes) {
    const auto found = nodes.find(node);
    ROSE_ASSERT(found != nodes.end() && found->second != node);
    auto *cloned = found->second;
    checkPoint(node->get_startOfConstruct(), cloned->get_startOfConstruct(),
               before, destination);
    checkPoint(node->get_endOfConstruct(), cloned->get_endOfConstruct(), before,
               destination);
    if (auto *expression = isSgExpression(node))
      checkPoint(expression->get_operatorPosition(),
                 isSgExpression(cloned)->get_operatorPosition(), before,
                 destination);
    auto *located = isSgLocatedNode(node);
    if (located == nullptr)
      continue;
    const auto *records = located->getAttachedPreprocessingInfo();
    const auto *copies =
        isSgLocatedNode(cloned)->getAttachedPreprocessingInfo();
    if (records == nullptr || copies == nullptr) {
      ROSE_ASSERT(records == nullptr && copies == nullptr);
      continue;
    }
    ROSE_ASSERT(records->size() == copies->size());
    for (size_t index = 0; index < records->size(); ++index) {
      const auto *record = records->at(index);
      const auto *clonedRecord = copies->at(index);
      ROSE_ASSERT(record != clonedRecord &&
                  record->getString() == clonedRecord->getString() &&
                  record->getFortranSourceRole() ==
                      clonedRecord->getFortranSourceRole() &&
                  record->getOutputPlacement() ==
                      clonedRecord->getOutputPlacement());
      checkPoint(record->get_file_info(), clonedRecord->get_file_info(), before,
                 destination);
    }
  }
  Rose::FortranSource::validate(copy);
}

void checkOccurrences(SgSourceFile *file, const std::string &kind) {
  const auto context = file->get_fortran_source_context();
  ROSE_ASSERT(context.root_occurrence != 0 && context.inclusions.size() == 3 &&
              context.output_surface ==
                  SgSourceFile::fortran_output_surface::preserve_source);
  std::set<unsigned> children;
  for (const auto &edge : context.inclusions) {
    ROSE_ASSERT(edge.parent_occurrence == context.root_occurrence &&
                edge.child_occurrence != context.root_occurrence &&
                children.insert(edge.child_occurrence).second);
    if (kind == "self")
      ROSE_ASSERT(edge.kind ==
                      SgSourceFile::fortran_include_kind::cpp_include &&
                  edge.physical_path == context.root_path);
    else
      ROSE_ASSERT(edge.kind ==
                      SgSourceFile::fortran_include_kind::native_include &&
                  edge.physical_path != context.root_path);
  }
  if (kind == "native")
    ROSE_ASSERT(context.inclusions[1].physical_path ==
                context.inclusions[2].physical_path);
}
} // namespace

int main(int argc, char **argv) {
  ROSE_ASSERT(argc == 4 || argc == 5);
  const bool rejectSymlink = argc == 5;
  ROSE_ASSERT(!rejectSymlink || std::string(argv[4]) == "reject-symlink");
  const fs::path input = fs::canonical(argv[1]);
  const fs::path work = fs::absolute(argv[2]);
  const std::string kind = argv[3];
  ROSE_ASSERT(kind == "native" || kind == "self");
  fs::current_path(input.parent_path());
  const auto sourceBytes = read(input);
  auto *project = frontend(std::vector<std::string>{
      argv[0], "-rose:skipfinalCompileStep", "-c", input.string()});
  ROSE_ASSERT(project != nullptr && project->numberOfFiles() == 1);
  auto *original = isSgSourceFile(project->get_fileList().front());
  checkOccurrences(original, kind);
  const auto originalState = snapshot(original);
  const std::vector<std::string> modes =
      rejectSymlink
          ? std::vector<std::string>{"symlink"}
          : std::vector<std::string>{"absolute", "relative", "missing"};
  for (const std::string &mode : modes) {
    const fs::path output = mode == "missing"
                                ? work / "absent" / "nested" / "missing.F90"
                                : input.parent_path() / (mode + ".F90");
    const auto requested =
        mode == "relative" ? output.filename().string() : output.string();
    const auto physical =
        (fs::weakly_canonical(output.parent_path()) / output.filename())
            .string();
    if (mode == "missing")
      ROSE_ASSERT(!fs::exists(output.parent_path()));
    if (rejectSymlink) {
      const auto target = input.parent_path() / "rex_rebind_symlink_target.txt";
      ROSE_ASSERT(fs::is_symlink(output) &&
                  fs::read_symlink(output) == target.filename() &&
                  read(target) == "REX_REBIND_DO_NOT_OVERWRITE\n");
    }
    SgTreeCopy copier(
        SgCopyHelp::ProvenancePolicy::PreserveExactFrontendSemantic);
    auto *clonedProject = isSgProject(project->copy(copier));
    ROSE_ASSERT(clonedProject != nullptr &&
                clonedProject->numberOfFiles() == 1);
    auto *copy = isSgSourceFile(clonedProject->get_fileList().front());
    checkTransfer(original, copy, copier,
                  original->get_file_info()->get_filenameString(),
                  original->get_file_info()->get_physical_filename(), false);
    SageBuilder::rebindCopiedSourceFilePhysicalIdentity(copy, requested);
    checkTransfer(original, copy, copier, requested, physical,
                  mode == "missing");
    AstTests::runAllTests(clonedProject);
    const auto transferred = snapshot(copy);
    copy->set_unparse_output_filename(requested);
    copy->unparse();
    if (rejectSymlink) {
      // The output-permissions invariant must reject the destination entry.
      SageInterface::deleteAST(clonedProject);
      SageInterface::deleteAST(project);
      return 1;
    }
    const auto emitted = read(output);
    Rose::AstJson::Options options;
    options.outputDirectory = (work / ("json-" + mode)).string();
    copy = Rose::AstJson::roundTripSourceFile(
        copy, Rose::AstJson::Checkpoint::PreOmpConstruction, options);
    ROSE_ASSERT(snapshot(copy) == transferred);
    AstTests::runAllTests(clonedProject);
    copy->set_unparse_output_filename(requested);
    copy->unparse();
    ROSE_ASSERT(read(output) == emitted &&
                snapshot(original) == originalState &&
                read(input) == sourceBytes);
    SageInterface::deleteAST(clonedProject);
  }
  SageInterface::deleteAST(project);
}
