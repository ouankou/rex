#include "astJson/sageAstJson.h"
#include "rose.h"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

std::string takeJsonDir(int &argc, char **argv) {
  constexpr char prefix[] = "--rex-ast-json-dir=";
  std::string result;
  std::vector<char *> filtered{argv[0]};
  for (int index = 1; index < argc; ++index) {
    if (std::strncmp(argv[index], prefix, sizeof(prefix) - 1) == 0) {
      result = argv[index] + sizeof(prefix) - 1;
    } else {
      filtered.push_back(argv[index]);
    }
  }
  argc = static_cast<int>(filtered.size());
  for (int index = 0; index < argc; ++index) {
    argv[index] = filtered[index];
  }
  argv[argc] = nullptr;
  return result;
}

SgSourceFile *firstSourceFile(SgProject *project) {
  ROSE_ASSERT(project != nullptr && project->numberOfFiles() == 1);
  SgSourceFile *file = isSgSourceFile(&project->get_file(0));
  ROSE_ASSERT(file != nullptr);
  return file;
}

SgType *stripFortranTypeWrappers(SgType *type) {
  while (type != nullptr) {
    if (SgModifierType *modifier = isSgModifierType(type)) {
      type = modifier->get_base_type();
    } else if (SgPointerType *pointer = isSgPointerType(type)) {
      type = pointer->get_base_type();
    } else if (SgArrayType *array = isSgArrayType(type)) {
      type = array->get_base_type();
    } else {
      return type;
    }
  }
  return nullptr;
}

void verifyFortranSemanticAndSourceTypes(SgSourceFile *file) {
  const Rose_STL_Container<SgNode *> initializedNames =
      NodeQuery::querySubTree(file, V_SgInitializedName);
  size_t verified = 0;
  for (SgNode *node : initializedNames) {
    SgInitializedName *name = isSgInitializedName(node);
    ROSE_ASSERT(name != nullptr);
    if (name->get_name() != "main_value" && name->get_name() != "block_value") {
      continue;
    }
    SgVariableDeclaration *declaration =
        isSgVariableDeclaration(name->get_parent());
    ROSE_ASSERT(declaration != nullptr);
    ROSE_ASSERT(declaration->get_fortran_declaration_origin() ==
                SgVariableDeclaration::e_fortran_source_declaration);

    SgType *semantic = stripFortranTypeWrappers(name->get_type());
    SgType *source = stripFortranTypeWrappers(name->get_fortran_source_type());
    ROSE_ASSERT(isSgTypeInt(semantic) != nullptr);
    ROSE_ASSERT(isSgTypeInt(source) != nullptr);
    ROSE_ASSERT(semantic != source);
    SgIntVal *semanticKind = isSgIntVal(semantic->get_type_kind());
    ROSE_ASSERT(semanticKind != nullptr && semanticKind->get_value() == 4);
    ROSE_ASSERT(source->get_type_kind() == nullptr);
    ++verified;
  }
  ROSE_ASSERT(verified == 2);
}

void verifyAnonymousProgramUnits(SgSourceFile *file) {
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
  ROSE_ASSERT(canonical->get_definition() == nullptr);
  ROSE_ASSERT(canonical->get_firstNondefiningDeclaration() == canonical);
  ROSE_ASSERT(canonical->get_definingDeclaration() == program);
  SgAuxiliaryDeclarationList *auxiliary =
      isSgAuxiliaryDeclarationList(canonical->get_parent());
  ROSE_ASSERT(auxiliary != nullptr);
  ROSE_ASSERT(auxiliary->get_parent() == program->get_scope());
  ROSE_ASSERT(program->get_scope()->get_auxiliary_declarations() == auxiliary);
  ROSE_ASSERT(program->get_name().is_null());
  ROSE_ASSERT(program->get_mangled_name().is_null());
  ROSE_ASSERT(SageInterface::isFortranProgramUnitWithoutSourceName(program));
  const SgName programKey =
      SageInterface::getFortranProgramUnitSymbolTableKey(program);
  ROSE_ASSERT(programKey.getString().find("__rex_internal_implicit_program_") ==
              0);
  SgFunctionSymbol *programSymbol =
      program->get_scope()->lookup_function_symbol(programKey);
  ROSE_ASSERT(programSymbol != nullptr);
  ROSE_ASSERT(programSymbol->get_declaration() == canonical);
  ROSE_ASSERT(canonical->get_symbol_from_symbol_table() == programSymbol);

  const Rose_STL_Container<SgNode *> procedures =
      NodeQuery::querySubTree(file, V_SgProcedureHeaderStatement);
  SgProcedureHeaderStatement *blockData = nullptr;
  for (SgNode *node : procedures) {
    SgProcedureHeaderStatement *candidate = isSgProcedureHeaderStatement(node);
    if (candidate != nullptr && candidate->isBlockData() &&
        candidate->get_definition() != nullptr) {
      ROSE_ASSERT(blockData == nullptr);
      blockData = candidate;
    }
  }
  ROSE_ASSERT(blockData != nullptr);
  ROSE_ASSERT(blockData->get_name().is_null());
  ROSE_ASSERT(blockData->get_mangled_name().is_null());
  ROSE_ASSERT(SageInterface::isFortranProgramUnitWithoutSourceName(blockData));
  const SgName blockDataKey =
      SageInterface::getFortranProgramUnitSymbolTableKey(blockData);
  ROSE_ASSERT(
      blockDataKey.getString().find("__rex_internal_unnamed_block_data_") == 0);
  ROSE_ASSERT(blockData->get_scope()->lookup_function_symbol(blockDataKey) !=
              nullptr);
}

void verifyJsonDoesNotExposeInternalNames(const std::filesystem::path &dir) {
  for (const auto &entry : std::filesystem::recursive_directory_iterator(dir)) {
    if (!entry.is_regular_file()) {
      continue;
    }
    std::ifstream input(entry.path());
    std::ostringstream contents;
    contents << input.rdbuf();
    ROSE_ASSERT(contents.str().find("__rex_internal_implicit_program_") ==
                std::string::npos);
    ROSE_ASSERT(contents.str().find("__rex_internal_unnamed_block_data_") ==
                std::string::npos);
  }
}

} // namespace

int main(int argc, char **argv) {
  const std::filesystem::path jsonDir = takeJsonDir(argc, argv);
  ROSE_ASSERT(!jsonDir.empty());
  std::filesystem::remove_all(jsonDir);
  std::filesystem::create_directories(jsonDir);

  SgProject *project = frontend(argc, argv);
  project->skipfinalCompileStep(true);
  SgSourceFile *file = firstSourceFile(project);
  verifyAnonymousProgramUnits(file);
  verifyFortranSemanticAndSourceTypes(file);

  file->get_originalCommandLineArgumentList().push_back(
      "-rex:ast-json-checkpoint=pre-omp-construction");
  Rose::AstJson::Options options;
  options.outputDirectory = jsonDir.string();
  file = Rose::AstJson::roundTripSourceFile(
      file, Rose::AstJson::Checkpoint::PreOmpConstruction, options);
  verifyAnonymousProgramUnits(file);
  verifyFortranSemanticAndSourceTypes(file);
  verifyJsonDoesNotExposeInternalNames(jsonDir);
  return backend(project);
}
