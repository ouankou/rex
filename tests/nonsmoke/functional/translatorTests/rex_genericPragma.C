// Validate that generic pragmas translated from Clang do not retain the leading
// "#pragma" text (which would unparse as "#pragma #pragma ...").
#include "rose.h"

using namespace SageInterface;

int main(int argc, char* argv[]) {
    SgProject* project = frontend(argc, argv);
    ROSE_ASSERT(project != NULL);

    SgFunctionDeclaration* main_func = findMain(project);
    ROSE_ASSERT(main_func != NULL);

    SgBasicBlock* body = main_func->get_definition()->get_body();
    ROSE_ASSERT(body != NULL);

    auto pragmas = querySubTree<SgPragmaDeclaration>(body, V_SgPragmaDeclaration);
    ROSE_ASSERT(pragmas.size() == 1);

    SgPragma* pragma = pragmas[0]->get_pragma();
    ROSE_ASSERT(pragma != NULL);

    const std::string directive = pragma->get_pragma();
    ROSE_ASSERT(directive == "rose test");
    ROSE_ASSERT(directive.find('#') == std::string::npos);

    return 0;
}
