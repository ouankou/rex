// A translator to dump AST terms within an input source file
// Liao
#include "rose.h"

#include "AstTerm.h"

#include <iostream>

#include <fstream>

#include <cstdlib>
#include <filesystem>
#include <stdlib.h>
using namespace std;

namespace {
std::string resolveTestOutputPath(const std::string &filename) {
  const char *output_dir = std::getenv("ROSE_TEST_OUTPUT_DIR");
  if (output_dir == nullptr || output_dir[0] == '\0') {
    return filename;
  }

  std::filesystem::path output_dir_path(output_dir);
  std::error_code ec;
  std::filesystem::create_directories(output_dir_path, ec);
  if (ec) {
    return filename;
  }

  return (output_dir_path / filename).string();
}
} // namespace

class visitorTraversal : public AstSimpleProcessing {
public:
  virtual void visit(SgNode *n);
};

void
// visitorTraversal::visit (SgNode * n)
visit(SgNode *n, ofstream &out) {
  if (SgDeclarationStatement *decl = isSgDeclarationStatement(n)) {

    string filename = decl->get_file_info()->get_filename();
    string suffix = Rose::StringUtility ::fileNameSuffix(filename);

    // vector.tcc: This is an internal header file, included by other library
    // headers
    if (suffix == "h" || suffix == "hpp" || suffix == "hh" || suffix == "H" ||
        suffix == "hxx" || suffix == "h++" || suffix == "tcc")
      return;

    // also check if it is compiler generated. Not from user code
    // skip compiler generated codes, mostly from template headers
    if (decl->get_file_info()->isCompilerGenerated())
      return;

    out << AstTerm::astTermWithNullValuesToString(decl) << endl;
  }
}

int main(int argc, char *argv[]) {
  SgProject *project = frontend(argc, argv);
  //  visitorTraversal exampleTraversal;
  //  exampleTraversal.traverse (project, preorder);
  if (project->get_fileList().size() >= 1) {
    SgFilePtrList file_list = project->get_fileList();
    std::string firstFileName =
        Rose::StringUtility::stripPathFromFileName(file_list[0]->getFileName());

    ofstream out;
    out.open(resolveTestOutputPath(firstFileName + ".astTerm.txt"));
    // out<< AstTerm::astTermWithNullValuesToString(project)<<endl; // too much
    // from headers
    SgGlobal *global = isSgSourceFile(file_list[0])->get_globalScope();
    SgDeclarationStatementPtrList &decl_list = global->get_declarations();

    for (int i = 0; i < decl_list.size(); i++) {
      visit(decl_list[i], out);
    }

    out.close();
  }

  //  visitorTraversal exampleTraversal;
  //  exampleTraversal.traverse (project, preorder);

  // skip unparsing, avoiding unparsing errors
  //  return backend (project);
  return 0;
}
