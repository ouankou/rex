/*! \brief  Demonstrate how to build a new file
 */
#include "rose.h"

#include <iostream>

using namespace std;

using namespace SageInterface;
using namespace SageBuilder;

int main(int argc, char *argv[]) {
  // Read in the input program (construct initial AST).
  SgProject *project = frontend(argc, argv);

  // Construct a new translation unit without parsing or creating dummy input.
  string newFilename_output = "new_source_file";
  SgSourceFile *sourceFile =
      buildGeneratedSourceFile(newFilename_output + ".C", project);
  ROSE_ASSERT(sourceFile != NULL);

  // There should now be 2 SgFile objects in the SgProject.
  ROSE_ASSERT(project->get_fileList().size() == 2);

  // Find the pointer to the global scope.
  SgGlobal *globalScopeOfNewFile = sourceFile->get_globalScope();
  ROSE_ASSERT(globalScopeOfNewFile != NULL);

  // Build a new function in the global scope (specify the name).
  string functionName = "foobar";

  // Build the new function's return type.
  SgType *return_type = buildVoidType();

  // Build a simple empty function parameter list.
  SgFunctionParameterList *parameter_list = buildFunctionParameterList();

  // Note that specification of scope determins the scope of the function and
  // not where it could be located (must be explicitly added to the target
  // scope).
  SgFunctionDeclaration *functionDeclaration = buildDefiningFunctionDeclaration(
      function_declaration_ownership::sourceLexical(), functionName,
      return_type, parameter_list, globalScopeOfNewFile);
  ROSE_ASSERT(functionDeclaration != NULL);

  // Build an explicit source prototype for the header. The defining function's
  // canonical nondefining declaration is semantic metadata, not source syntax.
  SgFunctionDeclaration *nondefiningFunctionDeclaration =
      buildNondefiningFunctionDeclaration(
          function_declaration_ownership::sourceLexicalRedeclarationIn(
              globalScopeOfNewFile, functionDeclaration),
          functionDeclaration, globalScopeOfNewFile);
  ROSE_ASSERT(nondefiningFunctionDeclaration != NULL);

  // Generate the string for the function prototype.
  string functionPrototypeString =
      nondefiningFunctionDeclaration->unparseToString();
  printf("functionPrototypeString = %s \n", functionPrototypeString.c_str());

  // Generate the header file and output the function prototype into the header
  // file.
  string header_suffix = ".h";
  string headerFileName = newFilename_output + header_suffix;
  ofstream outputfile(headerFileName.c_str(), ios::out);
  outputfile << "// Output for generated header file" << endl;
  outputfile << functionPrototypeString << endl;
  outputfile.close();

  // Run all diabnostics tests on the generated AST.
  AstTests::runAllTests(project);

  // Output an optional graph of the AST (just the tree, when active)
  generateDOT(*project);

  // Output an optional graph of the AST (the whole graph, of bounded
  // complexity, when active)
  const int MAX_NUMBER_OF_IR_NODES_TO_GRAPH_FOR_WHOLE_GRAPH = 10000;
  generateAstGraph(project, MAX_NUMBER_OF_IR_NODES_TO_GRAPH_FOR_WHOLE_GRAPH,
                   "");

  // Outout the unparsed code from the input AST.
  project->unparse();
}
