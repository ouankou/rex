#include "RoseAst.h"
#include "rose.h"

#include <map>
#include <set>
#include <string>

namespace {

bool exactSemanticInfo(Sg_File_Info *info, SgNode *owner) {
  return info != nullptr && owner != nullptr && info->get_parent() == owner &&
         info->isCompilerGenerated() && info->isFrontendSpecific() &&
         !info->isTransformation() &&
         !info->isSourcePositionUnavailableInFrontend() &&
         info->isOutputInCodeGeneration() &&
         info->get_file_id() == Sg_File_Info::COMPILER_GENERATED_FILE_ID &&
         info->get_physical_file_id() ==
             Sg_File_Info::COMPILER_GENERATED_FILE_ID;
}

} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(frontendExitStatus(project) == 0);

  const std::set<std::string> expected = {"__func__", "__FUNCTION__",
                                          "__PRETTY_FUNCTION__"};
  std::map<std::string, std::set<SgInitializedName *>> identities;
  std::map<SgFunctionDefinition *,
           std::map<std::string, std::set<SgInitializedName *>>>
      ownerIdentities;
  std::size_t predefinedReferences = 0;
  for (SgNode *node : RoseAst(project)) {
    SgVarRefExp *reference = isSgVarRefExp(node);
    if (reference == nullptr || reference->get_symbol() == nullptr) {
      continue;
    }
    SgVariableSymbol *symbol = reference->get_symbol();
    SgInitializedName *name = symbol->get_declaration();
    if (name == nullptr || !name->get_is_predefined_identifier()) {
      continue;
    }
    ++predefinedReferences;
    const std::string spelling = name->get_name().getString();
    ROSE_ASSERT(expected.count(spelling) == 1);
    identities[spelling].insert(name);

    SgVariableDeclaration *declaration =
        isSgVariableDeclaration(name->get_parent());
    SgAuxiliaryDeclarationList *auxiliary =
        declaration != nullptr
            ? isSgAuxiliaryDeclarationList(declaration->get_parent())
            : nullptr;
    SgFunctionDefinition *owner =
        auxiliary != nullptr ? isSgFunctionDefinition(auxiliary->get_parent())
                             : nullptr;
    ROSE_ASSERT(owner != nullptr);
    ownerIdentities[owner][spelling].insert(name);
    ROSE_ASSERT(name->get_scope() == owner);
    ROSE_ASSERT(owner->get_auxiliary_declarations() == auxiliary);
    ROSE_ASSERT(declaration->get_scope() == owner);
    ROSE_ASSERT(declaration->get_variables().size() == 1);
    ROSE_ASSERT(declaration->get_variables().front() == name);
    ROSE_ASSERT(name->get_type() != nullptr);
    ROSE_ASSERT(isSgPointerType(name->get_type()) == nullptr);
    ROSE_ASSERT(symbol->get_declaration() == name);
    ROSE_ASSERT(symbol->get_symbol_basis() == name);
    ROSE_ASSERT(symbol->get_type() == name->get_type());
    ROSE_ASSERT(symbol->get_parent() == owner->get_symbol_table());
    ROSE_ASSERT(owner->get_symbol_table()->exists(symbol));
    ROSE_ASSERT(owner->lookup_variable_symbol(name->get_name()) == symbol);
    ROSE_ASSERT(owner->find_symbol_from_declaration(name) == symbol);
    ROSE_ASSERT(exactSemanticInfo(name->get_file_info(), name));
    ROSE_ASSERT(exactSemanticInfo(name->get_startOfConstruct(), name));
    ROSE_ASSERT(exactSemanticInfo(name->get_endOfConstruct(), name));
    ROSE_ASSERT(exactSemanticInfo(declaration->get_file_info(), declaration));
    ROSE_ASSERT(
        exactSemanticInfo(declaration->get_startOfConstruct(), declaration));
    ROSE_ASSERT(
        exactSemanticInfo(declaration->get_endOfConstruct(), declaration));
  }

  ROSE_ASSERT(predefinedReferences == 9);
  ROSE_ASSERT(identities.size() == expected.size());
  ROSE_ASSERT(identities.at("__func__").size() == 3);
  ROSE_ASSERT(identities.at("__FUNCTION__").size() == 1);
  ROSE_ASSERT(identities.at("__PRETTY_FUNCTION__").size() == 1);
  for (const auto &ownerEntry : ownerIdentities) {
    for (const auto &spellingEntry : ownerEntry.second) {
      ROSE_ASSERT(spellingEntry.second.size() == 1);
    }
  }

  AstTests::runAllTests(project);
  return backend(project);
}
