#include "rose.h"

namespace si = SageInterface;
namespace sb = SageBuilder;

int main(int argc, char *argv[]) {
  SgProject *proj = frontend(argc, argv);
  proj->skipfinalCompileStep(true);

  SgFunctionDefinition *m_def =
      si::findFunctionDeclaration(proj, "foo", NULL, true)->get_definition();

  SgVariableSymbol *a_symbol =
      si::lookupVariableSymbolInParentScopes("a", m_def->get_body());
  ROSE_ASSERT(a_symbol != nullptr);

  SgVariableSymbol *x_symbol = nullptr;
  for (SgVarRefExp *reference :
       si::querySubTree<SgVarRefExp>(m_def, V_SgVarRefExp)) {
    if (reference->get_symbol()->get_name() == "x") {
      x_symbol = reference->get_symbol();
      break;
    }
  }
  ROSE_ASSERT(x_symbol != nullptr);

  si::appendStatement(sb::buildExprStatement(sb::buildVarRefExp(a_symbol)),
                      m_def->get_body());

  si::appendStatement(sb::buildExprStatement(sb::buildArrowExp(
                          sb::buildVarRefExp(a_symbol),
                          sb::buildVarRefExp(x_symbol), x_symbol->get_type())),
                      m_def->get_body());

  fprintf(stderr, "Testing\n");
  AstTests::runAllTests(proj);

  return backend(proj);
}
