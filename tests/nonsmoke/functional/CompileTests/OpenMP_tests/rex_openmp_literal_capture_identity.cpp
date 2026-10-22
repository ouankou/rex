#include "rose.h"

#include <memory>

static void verify(SgProject *project) {
  unsigned kernels = 0;
  unsigned shadows = 0;
  for (SgNode *node :
       NodeQuery::querySubTree(project, V_SgFunctionDeclaration)) {
    auto *function = isSgFunctionDeclaration(node);
    if (!function->get_functionModifier().isCudaKernel() ||
        function->get_definition() == nullptr)
      continue;
    ++kernels;
    auto *canonical =
        isSgFunctionDeclaration(function->get_firstNondefiningDeclaration());
    ROSE_ASSERT(canonical != nullptr && canonical != function);
    const auto &parameters = function->get_parameterList()->get_args();
    const auto &canonical_parameters =
        canonical->get_parameterList()->get_args();
    ROSE_ASSERT(!parameters.empty());
    unsigned literal_parameters = 0;
    for (SgInitializedName *parameter : parameters) {
      if (isSgIntegralType(parameter->get_type())) {
        ROSE_ASSERT(parameter->get_type() ==
                    SageInterface::requireTargetSizeType(function));
        ++literal_parameters;
      }
    }
    ROSE_ASSERT(literal_parameters == 1);
    ROSE_ASSERT(parameters.size() == canonical_parameters.size());
    ROSE_ASSERT(function->get_type() == canonical->get_type());
    auto *parameter_scope = canonical->get_functionParameterScope();
    ROSE_ASSERT(parameter_scope != nullptr &&
                parameter_scope->get_parent() == canonical);
    for (size_t index = 0; index < parameters.size(); ++index) {
      ROSE_ASSERT(parameters[index]->get_type() ==
                  canonical_parameters[index]->get_type());
      auto *parameter = canonical_parameters[index];
      ROSE_ASSERT(parameter->get_scope() == parameter_scope);
      ROSE_ASSERT(parameter->get_declptr() == canonical);
      if (!parameter->get_name().is_null())
        ROSE_ASSERT(parameter_scope->find_symbol_from_declaration(parameter) !=
                    nullptr);
    }

    auto *body = function->get_definition()->get_body();
    for (SgNode *declaration_node :
         NodeQuery::querySubTree(body, V_SgVariableDeclaration)) {
      auto *declaration = isSgVariableDeclaration(declaration_node);
      for (SgInitializedName *name : declaration->get_variables()) {
        auto *initializer = isSgAssignInitializer(name->get_initializer());
        auto *value = initializer != nullptr
                          ? isSgIntVal(initializer->get_operand())
                          : nullptr;
        if (value == nullptr ||
            (value->get_value() != 7 && value->get_value() != 11))
          continue;
        ++shadows;
        auto *symbol = isSgVariableSymbol(name->get_symbol_from_symbol_table());
        ROSE_ASSERT(symbol != nullptr && symbol->get_declaration() == name);
        unsigned uses = 0;
        for (SgNode *reference_node :
             NodeQuery::querySubTree(body, V_SgVarRefExp)) {
          auto *reference = isSgVarRefExp(reference_node);
          if (reference->get_symbol() == symbol)
            ++uses;
        }
        ROSE_ASSERT(uses == 1);
      }
    }
  }
  ROSE_ASSERT(kernels == 2 && shadows == 2);
  AstTests::runAllTests(project);
}

int main(int argc, char **argv) {
  std::unique_ptr<SgProject> project(frontend(argc, argv));
  ROSE_ASSERT(project->get_frontendErrorCode() == 0);
  verify(project.get());
  auto *copy = SageInterface::deepCopy(project.get());
  ROSE_ASSERT(copy != nullptr);
  verify(copy);
  SageInterface::deleteAST(copy);
  verify(project.get());
  return backend(project.get());
}
