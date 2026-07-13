#include "astJson/sageAstJson.h"
#include "rose.h"

#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

namespace {

std::string takeJsonDir(int &argc, char **argv) {
  const char prefix[] = "--rex-ast-json-dir=";
  std::string result;
  std::vector<char *> filtered;
  filtered.reserve(argc);
  filtered.push_back(argv[0]);

  for (int index = 1; index < argc; ++index) {
    if (std::strncmp(argv[index], prefix, sizeof(prefix) - 1) == 0) {
      result = argv[index] + sizeof(prefix) - 1;
      continue;
    }
    filtered.push_back(argv[index]);
  }

  argc = static_cast<int>(filtered.size());
  for (int index = 0; index < argc; ++index) {
    argv[index] = filtered[index];
  }
  argv[argc] = nullptr;
  return result;
}

SgSourceFile *firstSourceFile(SgProject *project) {
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(project->numberOfFiles() == 1);
  SgSourceFile *file = isSgSourceFile(&project->get_file(0));
  ROSE_ASSERT(file != nullptr);
  return file;
}

void verifyProgramNames(SgSourceFile *file) {
  const Rose_STL_Container<SgNode *> programs =
      NodeQuery::querySubTree(file, V_SgProgramHeaderStatement);
  ROSE_ASSERT(programs.size() == 2);
  SgProgramHeaderStatement *program = nullptr;
  for (SgNode *node : programs) {
    SgProgramHeaderStatement *candidate = isSgProgramHeaderStatement(node);
    ROSE_ASSERT(candidate != nullptr);
    if (candidate->get_definition() != nullptr) {
      ROSE_ASSERT(program == nullptr);
      program = candidate;
    }
  }
  ROSE_ASSERT(program != nullptr);
  ROSE_ASSERT(program->get_definingDeclaration() == program);
  SgProgramHeaderStatement *canonical =
      isSgProgramHeaderStatement(program->get_firstNondefiningDeclaration());
  ROSE_ASSERT(canonical != nullptr);
  ROSE_ASSERT(canonical != program);
  ROSE_ASSERT(canonical->get_firstNondefiningDeclaration() == canonical);
  ROSE_ASSERT(canonical->get_definingDeclaration() == program);
  ROSE_ASSERT(isSgAuxiliaryDeclarationList(canonical->get_parent()) != nullptr);
  ROSE_ASSERT(program->get_program_statement_kind() ==
              SgProgramHeaderStatement::e_explicit_program_statement);
  ROSE_ASSERT(program->get_name().getString() == "JsonRoundTrip");
  ROSE_ASSERT(program->get_named_in_end_statement());
  ROSE_ASSERT(program->get_end_statement_name().getString() == "jSoNrOuNdTrIp");
  ROSE_ASSERT(program->get_name() != program->get_end_statement_name());
  ROSE_ASSERT(canonical->get_program_statement_kind() ==
              program->get_program_statement_kind());
  ROSE_ASSERT(canonical->get_name() == program->get_name());
  ROSE_ASSERT(canonical->get_named_in_end_statement() ==
              program->get_named_in_end_statement());
  ROSE_ASSERT(canonical->get_end_statement_name() ==
              program->get_end_statement_name());
}

} // namespace

int main(int argc, char **argv) {
  const std::string jsonDir = takeJsonDir(argc, argv);
  ROSE_ASSERT(!jsonDir.empty());
  std::filesystem::remove_all(jsonDir);
  std::filesystem::create_directories(jsonDir);

  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  project->skipfinalCompileStep(true);

  SgSourceFile *originalFile = firstSourceFile(project);
  verifyProgramNames(originalFile);

  std::vector<std::string> checkpointArguments =
      originalFile->get_originalCommandLineArgumentList();
  checkpointArguments.push_back(
      "-rex:ast-json-checkpoint=pre-omp-construction");
  originalFile->get_originalCommandLineArgumentList() = checkpointArguments;

  Rose::AstJson::Options options;
  options.outputDirectory = jsonDir;
  SgSourceFile *restoredFile = Rose::AstJson::roundTripSourceFile(
      originalFile, Rose::AstJson::Checkpoint::PreOmpConstruction, options);
  ROSE_ASSERT(restoredFile != nullptr);
  ROSE_ASSERT(restoredFile != originalFile);
  verifyProgramNames(restoredFile);

  return backend(project);
}
