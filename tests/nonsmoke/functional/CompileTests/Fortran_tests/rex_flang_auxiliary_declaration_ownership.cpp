#include "rose.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <string>

namespace {

void requireOrdinaryOutputClassification(SgLocatedNode *node) {
  ROSE_ASSERT(node != nullptr);
  for (Sg_File_Info *info :
       {node->get_file_info(), node->get_startOfConstruct(),
        node->get_endOfConstruct()}) {
    ROSE_ASSERT(info != nullptr);
    ROSE_ASSERT(info->get_parent() == node);
    ROSE_ASSERT(info->isOutputInCodeGeneration());
  }
}

SgAuxiliaryDeclarationList *requireAuxiliaryOwner(SgScopeStatement *scope) {
  ROSE_ASSERT(scope != nullptr);
  SgAuxiliaryDeclarationList *container = scope->get_auxiliary_declarations();
  ROSE_ASSERT(container != nullptr);
  ROSE_ASSERT(container->get_parent() == scope);
  ROSE_ASSERT(!container->get_declarations().empty());
  container->validate_semantic_non_output_role();
  for (SgDeclarationStatement *declaration : container->get_declarations()) {
    ROSE_ASSERT(declaration != nullptr);
    ROSE_ASSERT(declaration->get_parent() == container);
    ROSE_ASSERT(declaration->get_scope() == scope);
    ROSE_ASSERT(std::count(container->get_declarations().begin(),
                           container->get_declarations().end(),
                           declaration) == 1);
    ROSE_ASSERT(!scope->statementExistsInScope(declaration));
    requireOrdinaryOutputClassification(declaration);
  }
  return container;
}

SgInitializedName *requireVariable(const SgAuxiliaryDeclarationList *container,
                                   const SgName &name) {
  ROSE_ASSERT(container != nullptr);
  SgInitializedName *match = nullptr;
  for (SgDeclarationStatement *declaration : container->get_declarations()) {
    SgVariableDeclaration *variable = isSgVariableDeclaration(declaration);
    if (variable == nullptr) {
      continue;
    }
    for (SgInitializedName *initialized : variable->get_variables()) {
      if (initialized == nullptr || initialized->get_name() != name) {
        continue;
      }
      ROSE_ASSERT(match == nullptr);
      match = initialized;
    }
  }
  ROSE_ASSERT(match != nullptr);
  return match;
}

void requireCanonicalProducerDerivedType(SgInitializedName *object,
                                         const SgName &producerName) {
  ROSE_ASSERT(object != nullptr);
  SgClassType *type = isSgClassType(object->get_type()->stripType(
      SgType::STRIP_MODIFIER_TYPE | SgType::STRIP_ARRAY_TYPE |
      SgType::STRIP_POINTER_TYPE));
  ROSE_ASSERT(type != nullptr);
  SgDerivedTypeStatement *canonical =
      isSgDerivedTypeStatement(type->get_declaration());
  ROSE_ASSERT(canonical != nullptr);
  ROSE_ASSERT(canonical->get_name() == producerName);
  ROSE_ASSERT(canonical->get_firstNondefiningDeclaration() == canonical);
  ROSE_ASSERT(canonical->get_definingDeclaration() != nullptr);
  ROSE_ASSERT(SageInterface::hasExactSemanticAuxiliaryOwnership(canonical));
  SgClassDefinition *moduleDefinition =
      isSgClassDefinition(canonical->get_scope());
  ROSE_ASSERT(moduleDefinition != nullptr);
  SgModuleStatement *module =
      isSgModuleStatement(moduleDefinition->get_declaration());
  ROSE_ASSERT(module != nullptr);
  ROSE_ASSERT(module->get_name() == "__fortran_builtins");
}

std::size_t countWord(const std::string &text, const std::string &word) {
  auto isWordCharacter = [](char c) {
    const unsigned char value = static_cast<unsigned char>(c);
    return std::isalnum(value) != 0 || c == '_';
  };
  std::size_t count = 0;
  for (std::size_t offset = text.find(word); offset != std::string::npos;
       offset = text.find(word, offset + word.size())) {
    const bool beginsWord = offset == 0 || !isWordCharacter(text[offset - 1]);
    const std::size_t end = offset + word.size();
    const bool endsWord = end == text.size() || !isWordCharacter(text[end]);
    if (beginsWord && endsWord) {
      ++count;
    }
  }
  return count;
}

} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  project->skipfinalCompileStep(true);

  SgSourceFile *file = nullptr;
  for (SgFile *candidate : project->get_fileList()) {
    SgSourceFile *source = isSgSourceFile(candidate);
    if (source == nullptr ||
        source->getFileName().find(
            "rex_fortran_intrinsic_use_derived_type_publication.f90") ==
            std::string::npos) {
      continue;
    }
    ROSE_ASSERT(file == nullptr);
    file = source;
  }
  ROSE_ASSERT(file != nullptr);
  SgGlobal *global = file->get_globalScope();
  ROSE_ASSERT(global != nullptr);

  SgProgramHeaderStatement *program = nullptr;
  for (SgNode *node :
       NodeQuery::querySubTree(file, V_SgProgramHeaderStatement)) {
    SgProgramHeaderStatement *candidate = isSgProgramHeaderStatement(node);
    if (candidate != nullptr && candidate->get_definition() != nullptr) {
      ROSE_ASSERT(program == nullptr);
      program = candidate;
    }
  }
  ROSE_ASSERT(program != nullptr);
  SgBasicBlock *body = program->get_definition()->get_body();
  ROSE_ASSERT(body != nullptr);

  SgAuxiliaryDeclarationList *globalAuxiliary = requireAuxiliaryOwner(global);
  SgAuxiliaryDeclarationList *bodyAuxiliary = requireAuxiliaryOwner(body);
  ROSE_ASSERT(globalAuxiliary->get_declarations().size() == 1);
  requireCanonicalProducerDerivedType(
      requireVariable(bodyAuxiliary, SgName("c_null_ptr")),
      SgName("__builtin_c_ptr"));
  requireCanonicalProducerDerivedType(
      requireVariable(bodyAuxiliary, SgName("c_null_funptr")),
      SgName("__builtin_c_funptr"));

  AstTests::runAllTests(project);
  if (backend(project) != 0) {
    return 2;
  }

  std::ifstream output(file->get_unparse_output_filename());
  if (!output) {
    return 3;
  }
  std::string text{std::istreambuf_iterator<char>(output),
                   std::istreambuf_iterator<char>()};
  std::transform(text.begin(), text.end(), text.begin(), [](char c) {
    return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  });
  for (const std::string &name :
       {"c_ptr", "c_funptr", "c_null_ptr", "c_null_funptr",
        "rex_fortran_intrinsic_use_derived_type_publication"}) {
    if (countWord(text, name) != 2) {
      return 4;
    }
  }
  return 0;
}
