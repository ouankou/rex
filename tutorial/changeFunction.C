// Redirect direct calls to the global socket-style send function. Resolve the
// signature through its typedef declarations and publish the callback through
// the declaration builder, which owns its parameters and symbol.
#include "rose.h"

#include <map>

namespace {
bool matchesSend(SgFunctionDeclaration *function) {
  auto *scope = isSgGlobal(function->get_scope());
  if (scope == nullptr || function->get_name() != "send")
    return false;
  auto *result = scope->lookup_typedef_symbol("ssize_t");
  auto *size = scope->lookup_typedef_symbol("size_t");
  if (result == nullptr || size == nullptr)
    return false;
  auto *type = function->get_type();
  ROSE_ASSERT(type != nullptr);
  const auto &arguments = type->get_arguments();
  if (arguments.size() != 4 || type->get_has_ellipses())
    return false;
  auto *buffer = SageBuilder::buildPointerType(
      SageBuilder::buildConstType(SageBuilder::buildVoidType()));
  return SageInterface::isEquivalentType(type->get_return_type(),
                                         result->get_type()) &&
         SageInterface::isEquivalentType(arguments[0],
                                         SageBuilder::buildIntType()) &&
         SageInterface::isEquivalentType(arguments[1], buffer) &&
         SageInterface::isEquivalentType(arguments[2], size->get_type()) &&
         SageInterface::isEquivalentType(arguments[3],
                                         SageBuilder::buildIntType());
}

class RedirectCalls : public SgSimpleProcessing {
  std::map<SgGlobal *, SgFunctionSymbol *> callbacks;

public:
  void visit(SgNode *node) override {
    auto *call = isSgFunctionCallExp(node);
    if (call == nullptr)
      return;
    SgExpression *callee = call->get_function();
    // The frontend represents function-to-pointer decay as an implicit cast.
    // Retain that typed conversion while replacing its direct function symbol.
    while (auto *cast = isSgCastExp(callee)) {
      if (cast->get_cast_type() != SgCastExp::e_implicit_cast)
        return;
      callee = cast->get_operand();
    }
    auto *reference = isSgFunctionRefExp(callee);
    if (reference == nullptr)
      return;
    auto *symbol = reference->get_symbol();
    ROSE_ASSERT(symbol != nullptr && symbol->get_declaration() != nullptr);
    auto *original = symbol->get_declaration();
    if (!matchesSend(original))
      return;
    auto *scope = SageInterface::getGlobalScope(call);
    ROSE_ASSERT(scope != nullptr);
    auto &callback = callbacks[scope];
    if (callback == nullptr) {
      auto *parameters = SageBuilder::buildFunctionParameterList();
      for (auto *type : original->get_type()->get_arguments())
        SageInterface::appendArg(parameters,
                                 SageBuilder::buildInitializedName("", type));
      SgNode *anchor = call;
      while (anchor->get_parent() != scope) {
        anchor = anchor->get_parent();
        ROSE_ASSERT(anchor != nullptr);
      }
      auto *statement = isSgStatement(anchor);
      ROSE_ASSERT(statement != nullptr);
      auto *declaration = SageBuilder::buildNondefiningFunctionDeclaration(
          SageBuilder::function_declaration_ownership::sourceLexicalBefore(
              scope, statement),
          "contest_call", original->get_type()->get_return_type(), parameters,
          scope);
      declaration->get_declarationModifier().get_storageModifier().setExtern();
      if (SageInterface::is_Cxx_language())
        declaration->set_linkage("C");
      callback =
          isSgFunctionSymbol(declaration->get_symbol_from_symbol_table());
      ROSE_ASSERT(callback != nullptr);
    }
    reference->set_symbol(callback);
  }
};
} // namespace

int main(int argc, char **argv) {
  auto *project = frontend(argc, argv);
  const int status = frontendExitStatus(project);
  if (status != 0)
    return status;
  RedirectCalls traversal;
  traversal.traverseInputFiles(project, preorder);
  AstTests::runAllTests(project);
  return backend(project);
}
