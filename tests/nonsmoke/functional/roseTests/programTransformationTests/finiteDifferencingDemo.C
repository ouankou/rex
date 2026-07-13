#include "rose.h"

#include "RoseAst.h"
#include "finiteDifferencing.h"

#include <memory>

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
          !record->isTransformation() || recordInfo->isTransformation() ||
          recordInfo->isCompilerGenerated() ||
          !recordInfo->isOutputInCodeGeneration()) {
        fprintf(stderr,
                "REX_TEST_ERROR: generated finite-differencing preprocessing "
                "has no exact physical owner owner=%p record=%p "
                "owner-shared=%d record-shared=%d owner-physical=%d "
                "record-physical=%d owner-filename=%s record-filename=%s "
                "record-transformation=%d info-transformation=%d "
                "info-compiler=%d info-output=%d\n",
                static_cast<void *>(ownerInfo), static_cast<void *>(recordInfo),
                ownerInfo != nullptr && ownerInfo->isShared() ? 1 : 0,
                recordInfo != nullptr && recordInfo->isShared() ? 1 : 0,
                ownerInfo != nullptr ? ownerInfo->get_physical_file_id() : -1,
                recordInfo != nullptr ? recordInfo->get_physical_file_id() : -1,
                ownerInfo != nullptr ? Sg_File_Info::getFilenameFromID(
                                           ownerInfo->get_physical_file_id())
                                           .c_str()
                                     : "<null>",
                recordInfo != nullptr ? recordInfo->get_filenameString().c_str()
                                      : "<null>",
                record->isTransformation() ? 1 : 0,
                recordInfo != nullptr && recordInfo->isTransformation() ? 1 : 0,
                recordInfo != nullptr && recordInfo->isCompilerGenerated() ? 1
                                                                           : 0,
                recordInfo != nullptr && recordInfo->isOutputInCodeGeneration()
                    ? 1
                    : 0);
        ROSE_ABORT();
      }
    }
  }
  if (matches == 0) {
    fprintf(stderr, "REX_TEST_ERROR: finite differencing generated no owned "
                    "preprocessing record\n");
    ROSE_ABORT();
  }
}

int main(int argc, char *argv[]) {
  // Main Function for default example ROSE Preprocessor
  // This is an example of a preprocessor that can be built with ROSE

  // Build the project object (AST) which we will fill up with multiple files
  // and use as a handle for all processing of the AST(s) associated with one or
  // more source files.
  SgProject *sageProject = frontend(argc, argv);

  // Exercise the production finite-differencing entry point.  The former
  // driver inferred candidate expressions from incidental SgMinusOp/SgNotOp
  // nodes and therefore depended on malformed frontend AST shapes instead of
  // the transformation's typed candidate analysis.
  simpleIndexFiniteDifferencing(sageProject);

  requireGeneratedPreprocessingOwnership(sageProject, "Finite differencing:");

  // AstPDFGeneration().generateInputFiles(sageProject);

  // Generate the final C++ source code from the potentially modified SAGE AST
  sageProject->unparse();

  // What remains is to run the specified compiler (typically the C++ compiler)
  // using the generated output file (unparsed and transformed application code)
  // to generate an object file.
  int finalCombinedExitStatus = sageProject->compileOutput();

  // return exit code from complilation of generated (unparsed) code
  return finalCombinedExitStatus;
}
