// Author: Dan Quinlan

// Report explicit and implicit captures of this in lambda expressions.

#include "rose.h"

class Traversal : public SgSimpleProcessing {
public:
  void visit(SgNode *astNode) override;
};

void Traversal::visit(SgNode *astNode) {
  SgLambdaCapture *lambdaCapture = isSgLambdaCapture(astNode);
  if (lambdaCapture != nullptr) {
    SgExpression *captureVariable = lambdaCapture->get_capture_variable();
    if (captureVariable != nullptr) {
      if (isSgThisExp(captureVariable) != nullptr) {
        printf("Identified use of C++ \"this\" expression in lambda capture "
               "list: \n");

        std::string filename =
            lambdaCapture->get_file_info()->get_filenameString();
        int lineNumber = lambdaCapture->get_file_info()->get_line();
        int columnNumber = lambdaCapture->get_file_info()->get_col();

        printf("   --- location: \n");
        printf("   --- --- file = %s \n", filename.c_str());
        printf("   --- --- line = %d column = %d \n", lineNumber, columnNumber);
      }
    }
  }
}

int main(int argc, char *argv[]) {
  SgProject *project = frontend(argc, argv);
  ASSERT_not_null(project);
  if (project->get_frontendErrorCode() != 0) {
    const int status = frontendExitStatus(project);
    SageInterface::deleteAST(project,
                             SageInterface::DeleteAstMode::kRequireIsolated);
    return status;
  }
  Traversal traversal;
  traversal.traverseInputFiles(project, preorder);

  const int status = backendCompilesUsingOriginalInputFile(project);
  SageInterface::deleteAST(project,
                           SageInterface::DeleteAstMode::kRequireIsolated);
  return status;
}
