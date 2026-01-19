// This is a translator that demonstrates (or demonstrated) a bug in ROSE.
// This is is provided as part of regression tests on translators that 
// demonstrate bugs in ROSE and is different from the tests/nonsmoke/functional/CompileTests
// directory which demonstrates input codes that demonstrate bugs in ROSE.

#include "rose.h"

using namespace std;
using namespace SageBuilder;
using namespace SageInterface;

int
main(int argc, char **argv)
   {
     SgFile *file = buildFile("blank.cpp", "out.cpp");
     ROSE_ASSERT(file != NULL);
     SgSourceFile *outputFile = isSgSourceFile(file);
     ROSE_ASSERT(outputFile != NULL);
     SgProject *project = outputFile->get_project();
     ROSE_ASSERT(project != NULL);
     SgGlobal *globalScope = outputFile->get_globalScope();
     ROSE_ASSERT(globalScope != NULL);

     SgFunctionDeclaration *func = buildDefiningFunctionDeclaration(
         "a_function", buildVoidType(), buildFunctionParameterList(),
         globalScope);
     appendStatement(func, globalScope);

     AstTests::runAllTests(project);
     project->unparse();

     return 0;
   }
