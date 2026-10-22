#include "astJson/sageAstJson.h"
#include "rose.h"

#include <filesystem>
#include <map>
#include <memory>
#include <string>

namespace {
struct Binding {
  const char *identity;
  bool labelPresent;
  const char *label;
};

void verify(SgSourceFile *file) {
  SgSymbol *namelist_binding = nullptr;
  unsigned namelist_controls = 0;
  for (auto *node : NodeQuery::querySubTree(file, V_SgIOStatement)) {
    auto *read = isSgReadStatement(node);
    auto *write = isSgWriteStatement(node);
    auto *binding = read != nullptr    ? read->get_namelist()
                    : write != nullptr ? write->get_namelist()
                                       : nullptr;
    if (binding == nullptr)
      continue;
    ROSE_ASSERT(namelist_binding == nullptr || namelist_binding == binding);
    namelist_binding = binding;
    ++namelist_controls;
  }
  ROSE_ASSERT(namelist_controls == 2);
  auto *alias = isSgAliasSymbol(namelist_binding);
  ROSE_ASSERT(alias != nullptr && alias->get_name() == "rex_local_parameters");
  auto *producer = isSgAliasSymbol(alias->get_alias());
  ROSE_ASSERT(producer != nullptr &&
              producer->get_name() == "rex_exported_parameters");
  ROSE_ASSERT(producer->get_scope() != alias->get_scope());
  auto *namelist = SgNamelistSymbol::resolve(alias);
  ROSE_ASSERT(namelist->get_name() == "rex_parameters");
  ROSE_ASSERT(namelist->get_declarations().size() == 2);
  ROSE_ASSERT(alias->get_declaration() ==
              namelist->get_declarations().front()->get_parent());
  const char *names[] = {"rex_namelist_first", "rex_namelist_second"};
  for (size_t index = 0; index < 2; ++index) {
    auto *group = namelist->get_declarations()[index];
    group->validate_group();
    ROSE_ASSERT(group->get_objects().size() == 1);
    auto *object =
        group->get_objects().front()->get_symbol()->get_declaration();
    ROSE_ASSERT(object->get_name() == names[index]);
    ROSE_ASSERT(object->get_scope() == namelist->get_scope());
  }
  const std::map<std::string, Binding> expected = {
      {"rex_bind_default", {"rex_bind_default", false, ""}},
      {"rex_bind_named", {"rex_bound_Name", true, "  rex_bound_Name  "}},
      {"rex_bind_empty", {"", true, ""}},
      {"rex_bind_blanks", {"", true, "   "}},
      {"rex_bind_host", {"rex_bind_host", false, ""}},
      {"rex_bind_internal", {"", false, ""}},
      {"rex_bind_reference", {"rex_bind_reference", false, ""}},
      {"rex_bind_descriptor", {"rex_bind_descriptor", false, ""}}};
  const auto &abi = file->get_target_abi();
  ROSE_ASSERT(abi.available && abi.pointer_size_bits > 0);
  ROSE_ASSERT(!abi.scalars.empty());
  unsigned wideDeclarations = 0;
  for (auto *node : NodeQuery::querySubTree(file, V_SgInitializedName)) {
    auto *declaration = isSgInitializedName(node);
    if (declaration->get_name() != "rex_target_kind8")
      continue;
    bool is_unsigned;
    unsigned width;
    SageInterface::requireTargetIntegerDomain(declaration->get_type(),
                                              declaration, is_unsigned, width);
    ROSE_ASSERT(!is_unsigned && width == 64);
    ++wideDeclarations;
  }
  ROSE_ASSERT(wideDeclarations == 1);
  std::map<std::string, unsigned> definitions;
  for (SgNode *node :
       NodeQuery::querySubTree(file, V_SgProcedureHeaderStatement)) {
    auto *procedure = isSgProcedureHeaderStatement(node);
    ROSE_ASSERT(procedure != nullptr);
    const std::string name = procedure->get_name().getString();
    const auto found = expected.find(name);
    if (found == expected.end()) {
      continue;
    }
    const Binding &binding = found->second;
    ROSE_ASSERT(procedure->get_declarationModifier().isBind());
    ROSE_ASSERT(procedure->get_linkage() == "C");
    ROSE_ASSERT(procedure->get_fortran_binding_name() == binding.identity);
    auto *canonical = isSgProcedureHeaderStatement(
        procedure->get_firstNondefiningDeclaration());
    ROSE_ASSERT(canonical != nullptr);
    ROSE_ASSERT(canonical->get_declarationModifier().isBind());
    ROSE_ASSERT(canonical->get_linkage() == "C");
    ROSE_ASSERT(canonical->get_fortran_binding_name() == binding.identity);
    const auto passing =
        name == "rex_bind_reference"
            ? SgInitializedName::e_fortran_c_argument_reference
        : name == "rex_bind_descriptor"
            ? SgInitializedName::e_fortran_c_argument_descriptor
            : SgInitializedName::e_fortran_c_argument_value;
    ROSE_ASSERT(procedure->get_args().size() == 1);
    ROSE_ASSERT(
        procedure->get_args().front()->get_fortran_c_argument_passing() ==
        passing);
    ROSE_ASSERT(canonical->get_args().size() == 1);
    ROSE_ASSERT(
        canonical->get_args().front()->get_fortran_c_argument_passing() ==
        passing);
    if (procedure->get_definition() == nullptr) {
      continue;
    }
    ++definitions[name];
    ROSE_ASSERT(procedure->get_binding_label_present() == binding.labelPresent);
    ROSE_ASSERT(procedure->get_binding_label() == binding.label);
  }
  ROSE_ASSERT(definitions.size() == expected.size());
  for (const auto &entry : definitions) {
    ROSE_ASSERT(entry.second == 1);
  }
}
} // namespace

int main(int argc, char **argv) {
  std::unique_ptr<SgProject> project(frontend(argc, argv));
  ROSE_ASSERT(project != nullptr && project->numberOfFiles() == 1);
  auto *file = isSgSourceFile(&project->get_file(0));
  ROSE_ASSERT(file != nullptr);
  AstTests::runAllTests(project.get());
  verify(file);
  // Copy the complete project, including canonical declarations owned by its
  // auxiliary scopes; a defining-procedure subtree is not an isolated owner.
  SgTreeCopy copy;
  auto copied = std::unique_ptr<SgProject>(isSgProject(project->copy(copy)));
  ROSE_ASSERT(copied != nullptr && copied.get() != project.get());
  verify(isSgSourceFile(&copied->get_file(0)));
  AstTests::runAllTests(copied.get());
  SgProject *copied_project = copied.release();
  SgFileList *copied_files = copied_project->get_fileList_ptr();
  SageInterface::deleteAST(copied_project,
                           SageInterface::DeleteAstMode::kRequireIsolated);
  ROSE_ASSERT(!SgNode::isLiveNode(copied_project));
  ROSE_ASSERT(!SgNode::isLiveNode(copied_files));
  verify(file);
  Rose::AstJson::Options options;
  options.outputDirectory =
      (std::filesystem::current_path() / "checkpoint").string();
  auto &arguments = file->get_originalCommandLineArgumentList();
  arguments.push_back("-rex:ast-json-checkpoint=pre-omp-construction");
  file = Rose::AstJson::roundTripSourceFile(
      file, Rose::AstJson::Checkpoint::PreOmpConstruction, options);
  ROSE_ASSERT(file != nullptr);
  verify(file);
  AstTests::runAllTests(project.get());
  return backend(project.get());
}
