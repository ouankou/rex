#include "omp_lowering.h"

#include "rose.h"

#include <algorithm>
#include <string>
#include <vector>

namespace {
class DevicePlaceholderCollector : public ROSE_VisitTraversal {
public:
  void visit(SgNode *node) override {
    SgInitializedName *name = isSgInitializedName(node);
    SgVariableDeclaration *declaration =
        name != nullptr ? isSgVariableDeclaration(name->get_parent()) : nullptr;
    if (name == nullptr || name->get_name() != "_dev_value" ||
        declaration == nullptr ||
        isSgAuxiliaryDeclarationList(declaration->get_parent()) == nullptr) {
      return;
    }
    placeholders.push_back(name);
  }

  std::vector<SgInitializedName *> placeholders;
};

SgType *requireExactRuntimeArrayElement(SgInitializedName *name) {
  ROSE_ASSERT(name != nullptr && name->get_type() != nullptr);
  SgType *element = nullptr;
  if (SgArrayType *array = isSgArrayType(name->get_type()))
    element = array->get_base_type();
  else if (SgPointerType *pointer = isSgPointerType(name->get_type()))
    element = pointer->get_base_type();
  ROSE_ASSERT(element != nullptr);
  ROSE_ASSERT(isSgTypedefType(element) == nullptr);
  ROSE_ASSERT(isSgModifierType(element) == nullptr);
  return element;
}
} // namespace

int main(int argc, char **argv) {
  if (argc != 3)
    return 2;
  const std::string mode = argv[1];
  if (mode != "--lp64" && mode != "--ilp32")
    return 2;

  std::vector<std::string> frontend_args{argv[0], "-rose:openmp:ast_only",
                                         "-rose:skipfinalCompileStep"};
  if (mode == "--ilp32")
    frontend_args.push_back("-m32");
  frontend_args.insert(frontend_args.end(), {"-c", argv[2]});
  SgProject *project = frontend(frontend_args);
  ROSE_ASSERT(project != nullptr && project->get_fileList().size() == 1);
  SgSourceFile *source = isSgSourceFile(project->get_fileList().front());
  ROSE_ASSERT(source != nullptr);

  std::vector<SgTypedefDeclaration *> source_int64_typedefs;
  for (SgTypedefDeclaration *declaration :
       SageInterface::querySubTree<SgTypedefDeclaration>(
           source, V_SgTypedefDeclaration)) {
    if (declaration != nullptr && declaration->get_name() == "int64_t")
      source_int64_typedefs.push_back(declaration);
  }
  ROSE_ASSERT(source_int64_typedefs.size() == 1);
  ROSE_ASSERT(isSgTypeInt(source_int64_typedefs.front()->get_base_type()) !=
              nullptr);

  OmpSupport::lower_omp(source);

  std::vector<SgTypedefDeclaration *> lowered_int64_typedefs;
  for (SgTypedefDeclaration *declaration :
       SageInterface::querySubTree<SgTypedefDeclaration>(
           source, V_SgTypedefDeclaration)) {
    if (declaration != nullptr && declaration->get_name() == "int64_t")
      lowered_int64_typedefs.push_back(declaration);
  }
  ROSE_ASSERT(lowered_int64_typedefs.size() == 1);
  ROSE_ASSERT(lowered_int64_typedefs.front() == source_int64_typedefs.front());

  size_t runtime_array_count = 0;
  for (SgInitializedName *name : SageInterface::querySubTree<SgInitializedName>(
           source, V_SgInitializedName)) {
    if (name == nullptr || (name->get_name() != "__arg_sizes" &&
                            name->get_name() != "__arg_types"))
      continue;
    ++runtime_array_count;
    SgType *element = requireExactRuntimeArrayElement(name);
    if (mode == "--lp64") {
      ROSE_ASSERT(element == SageBuilder::buildLongType());
      ROSE_ASSERT(isSgTypeLong(element) != nullptr);
    } else {
      ROSE_ASSERT(element == SageBuilder::buildLongLongType());
      ROSE_ASSERT(isSgTypeLongLong(element) != nullptr);
    }
  }
  ROSE_ASSERT(runtime_array_count >= 2);

  // The rewritten device pointer is a transaction-local outlining identity,
  // not a lexical declaration. Auxiliary declarations are intentionally
  // outside ordinary project traversal, so inspect the memory pool and prove
  // that the identity was retired after every body reference was remapped to a
  // real kernel parameter.
  DevicePlaceholderCollector collector;
  collector.traverseMemoryPool();
  ROSE_ASSERT(collector.placeholders.empty());

  size_t device_parameter_count = 0;
  size_t defining_device_parameter_count = 0;
  for (SgInitializedName *name : SageInterface::querySubTree<SgInitializedName>(
           project, V_SgInitializedName)) {
    if (name == nullptr || name->get_name() != "_dev_value")
      continue;
    ++device_parameter_count;
    SgFunctionParameterList *parameters =
        isSgFunctionParameterList(name->get_parent());
    SgFunctionDeclaration *function =
        parameters != nullptr
            ? isSgFunctionDeclaration(parameters->get_parent())
            : nullptr;
    SgVariableSymbol *symbol =
        isSgVariableSymbol(name->get_symbol_from_symbol_table());
    ROSE_ASSERT(parameters != nullptr && function != nullptr &&
                function->get_parameterList() == parameters &&
                std::count(parameters->get_args().begin(),
                           parameters->get_args().end(), name) == 1 &&
                name->get_type() != nullptr && name->get_scope() != nullptr);

    if (SgFunctionDefinition *definition = function->get_definition()) {
      SgFunctionDeclaration *canonical =
          isSgFunctionDeclaration(function->get_firstNondefiningDeclaration());
      ROSE_ASSERT(function->get_definingDeclaration() == function &&
                  definition->get_parent() == function &&
                  definition->get_declaration() == function &&
                  canonical != nullptr &&
                  canonical->get_definingDeclaration() == function &&
                  name->get_scope() == definition->get_body() &&
                  symbol != nullptr && symbol->get_declaration() == name);
      ++defining_device_parameter_count;
      continue;
    }

    // A prototype owns a distinct semantic parameter scope.  Its named
    // parameters must resolve through that scope instead of leaking into the
    // enclosing global symbol table or borrowing the defining parameter
    // identity.
    SgScopeStatement *scope = function->get_scope();
    SgFunctionParameterScope *parameter_scope =
        isSgFunctionParameterScope(name->get_scope());
    SgFunctionSymbol *function_symbol =
        scope != nullptr
            ? isSgFunctionSymbol(scope->find_symbol_from_declaration(function))
            : nullptr;
    ROSE_ASSERT(parameter_scope != nullptr &&
                parameter_scope->get_parent() == function &&
                parameter_scope->get_scope() == scope && symbol != nullptr &&
                symbol->get_declaration() == name &&
                function->get_firstNondefiningDeclaration() == function &&
                function_symbol != nullptr &&
                function_symbol->get_symbol_basis() == function);

    SgFunctionDeclaration *defining =
        isSgFunctionDeclaration(function->get_definingDeclaration());
    if (defining == nullptr) {
      SgGlobal *global = isSgGlobal(scope);
      ROSE_ASSERT(
          global != nullptr && function->get_parent() == global &&
          global->statementExistsInScope(function) &&
          function->get_declarationModifier().get_storageModifier().isExtern());
      continue;
    }

    SgAuxiliaryDeclarationList *auxiliary =
        isSgAuxiliaryDeclarationList(function->get_parent());
    SgFunctionDefinition *definition = defining->get_definition();
    ROSE_ASSERT(auxiliary != nullptr && auxiliary->get_parent() == scope &&
                defining->get_parent() == scope &&
                defining->get_scope() == scope && definition != nullptr &&
                defining->get_firstNondefiningDeclaration() == function &&
                defining->get_definingDeclaration() == defining);
    const SgInitializedNamePtrList &prototype_args = parameters->get_args();
    const SgInitializedNamePtrList &defining_args =
        defining->get_parameterList()->get_args();
    const size_t index = static_cast<size_t>(std::distance(
        prototype_args.begin(),
        std::find(prototype_args.begin(), prototype_args.end(), name)));
    ROSE_ASSERT(index < defining_args.size() &&
                defining_args.size() == prototype_args.size() &&
                defining_args[index]->get_name() == name->get_name() &&
                defining_args[index]->get_type() == name->get_type());
  }
  ROSE_ASSERT(device_parameter_count > 0);
  ROSE_ASSERT(defining_device_parameter_count > 0);

  AstTests::runAllTests(project);
  SageInterface::tearDownAst(project);
  return 0;
}
