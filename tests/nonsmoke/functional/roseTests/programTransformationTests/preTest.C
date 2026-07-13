#include "rose.h"

#include "RoseAst.h"
#include "pre.h"

#include <vector>

#include <string>

#include "VectorCommandOptions.h"
using namespace std;

void requireGeneratedPreprocessingOwnership(SgNode *root,
                                            const std::string &marker) {
  size_t matches = 0;
  RoseAst ast(root);
  for (RoseAst::iterator current = ast.begin(); current != ast.end();
       ++current) {
    SgLocatedNode *owner = isSgLocatedNode(*current);
    if (owner == nullptr || owner->getAttachedPreprocessingInfo() == nullptr) {
      continue;
    }
    for (PreprocessingInfo *record : *owner->getAttachedPreprocessingInfo()) {
      if (record == nullptr ||
          record->getString().find(marker) == std::string::npos) {
        continue;
      }
      ++matches;
      Sg_File_Info *ownerInfo = owner->get_file_info();
      Sg_File_Info *recordInfo = record->get_file_info();
      if (SageInterface::getEnclosingSourceFile(owner, true) == nullptr ||
          ownerInfo == nullptr || recordInfo == nullptr ||
          ownerInfo->isShared() || recordInfo->isShared() ||
          ownerInfo->get_physical_file_id() < 0 ||
          recordInfo->get_physical_file_id() !=
              ownerInfo->get_physical_file_id() ||
          recordInfo->get_physical_filename() !=
              Sg_File_Info::getFilenameFromID(
                  ownerInfo->get_physical_file_id()) ||
          recordInfo->get_filenameString() != "Compiler-Generated in PRE" ||
          !record->isTransformation() || recordInfo->isTransformation() ||
          recordInfo->isCompilerGenerated() ||
          !recordInfo->isOutputInCodeGeneration()) {
        fprintf(stderr,
                "REX_TEST_ERROR: generated partial-redundancy preprocessing "
                "has no exact physical owner\n");
        ROSE_ABORT();
      }
    }
  }
  if (matches == 0) {
    fprintf(stderr,
            "REX_TEST_ERROR: partial redundancy elimination generated no "
            "owned preprocessing record\n");
    ROSE_ABORT();
  }
}

int main(int argc, char *argv[]) {
  // Main Function for default example ROSE Preprocessor
  // This is an example of a preprocessor that can be built with ROSE

  // Build the project object (AST) which we will fill up with multiple files
  // and use as a handle for all processing of the AST(s) associated with one or
  // more source files.
  vector<string> argvList(argv, argv + argc);
  VectorCmdOptions::GetInstance()->SetOptions(argvList);
  SgProject *sageProject = frontend(argvList);
  // FixSgProject(sageProject);

  // AstTests::runAllTests(const_cast<SgProject*>(project));
  AstTests::runAllTests(sageProject);

  legacy::PRE::partialRedundancyElimination(sageProject);
  requireGeneratedPreprocessingOwnership(sageProject,
                                         "Partial redundancy elimination:");

  // AstPDFGeneration().generateInputFiles(sageProject);

  AstTests::runAllTests(sageProject);

  // AstPDFGeneration().generateInputFiles(sageProject);

  // Generate the final C++ source code from the potentially modified SAGE AST
  sageProject->unparse();

  // What remains is to run the specified compiler (typically the C++ compiler)
  // using the generated output file (unparsed and transformed application code)
  // to generate an object file. int finalCombinedExitStatus =
  // sageProject.compileOutput();

  // return exit code from complilation of generated (unparsed) code
  return 0;
}
