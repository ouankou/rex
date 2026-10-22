// Dump source declarations from the first input translation unit.
#include "rose.h"

#include "AstTerm.h"
#include "rose_test_output_path.h"

#include <fstream>
#include <iostream>

int main(int argc, char *argv[]) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr && project->numberOfFiles() > 0);
  SgSourceFile *file = isSgSourceFile(&project->get_file(0));
  ROSE_ASSERT(file != nullptr && file->get_globalScope() != nullptr);
  const std::string output = Rose::TestOutput::resolvePath(
      Rose::StringUtility::stripPathFromFileName(file->getFileName()) +
      ".astTerm.txt");
  std::ofstream out(output);
  if (!out) {
    std::cerr << "Cannot open AST term output: " << output << '\n';
    return 1;
  }
  const int sourceId = file->get_file_info()->get_physical_file_id();
  ROSE_ASSERT(sourceId >= 0);
  for (SgDeclarationStatement *decl :
       file->get_globalScope()->get_declarations()) {
    ROSE_ASSERT(decl != nullptr && decl->get_file_info() != nullptr);
    const Sg_File_Info *position = decl->get_file_info();
    if (!position->isCompilerGenerated() &&
        position->get_physical_file_id() == sourceId)
      out << AstTerm::astTermWithNullValuesToString(decl) << '\n';
  }
  out.close();
  if (!out) {
    std::cerr << "Cannot write AST term output: " << output << '\n';
    return 1;
  }
  return 0;
}
