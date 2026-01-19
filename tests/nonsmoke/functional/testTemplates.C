// Example ROSE Translator used for testing ROSE infrastructure
#include "rose.h"

#include "RoseAst.h"

void markNodeToBeUnparsed(SgNode *node) {
  Sg_File_Info *fileInfo = node->get_file_info();
  if (fileInfo != NULL) {
    fileInfo->setTransformation();
    fileInfo->setOutputInCodeGeneration();

    SgLocatedNode *locatedNode = isSgLocatedNode(node);
    if (locatedNode != NULL) {
      // DQ (7/7/2015): Make the subtree as transformed.
      locatedNode->setTransformation();
      locatedNode->setOutputInCodeGeneration();
      markTransformationsForOutput(node);
    } else {
    }
  } else {
  }
}

int markAllTemplateInstantiationsToBeUnparsed(SgProject *root) {
  RoseAst ast(root);
  int n = 0;
  for (RoseAst::iterator i = ast.begin(); i != ast.end(); ++i) {
    // if (isTemplateInstantiationNode(*i))
    if (SageInterface::isTemplateInstantiationNode(*i)) {
      markNodeToBeUnparsed(*i);
      n++;
    }
  }

  return n;
}

int main(int argc, char *argv[]) {

  // Generate the ROSE AST.
  SgProject *project = frontend(argc, argv);

  // AST consistency tests (optional for users, but this enforces more of our
  // tests)
  AstTests::runAllTests(project);

  markAllTemplateInstantiationsToBeUnparsed(project);

  // DQ (9/17/2015): Call fixup function for template instantiations so that
  // they can be unparsed with the GNU g++ backend compiler.
  SageInterface::wrapAllTemplateInstantiationsInAssociatedNamespaces(project);

  // regenerate the source code and call the vendor
  // compiler, only backend error code is reported.
  return backend(project);
}
