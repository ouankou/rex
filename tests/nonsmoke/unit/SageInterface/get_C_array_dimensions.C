#include "rose.h"

#include <iostream>

#include <string>

#include <vector>

#include <fstream>

#include "test_output_path.h"

using namespace std;
using namespace SageInterface;

ofstream ofile;

class visitorTraversal : public AstSimpleProcessing {
protected:
  virtual void visit(SgNode *n);
};

void visitorTraversal::visit(SgNode *node) {
  if (SgLocatedNode *lnode = isSgLocatedNode(node)) {
    // skip system headers
    if (insideSystemHeader(lnode))
      return;

    if (SgVariableDeclaration *decl = isSgVariableDeclaration(node)) {
      ofile << "variable declaration at line "
            << decl->get_file_info()->get_line() << endl;
      SgInitializedName *iname = getFirstInitializedName(decl);

      if (iname != NULL) {
        SgArrayType *atype = isSgArrayType(iname->get_type());
        if (atype != NULL) {
          vector<SgExpression *> dims = get_C_array_dimensions(*atype, *iname);
          for (size_t i = 0; i < dims.size(); i++) {
            // Must redirect to a .output file to enable diff-based correctness
            // checking and avoid screen spewing interruptions.
            // The returned expression is intentionally detached and owned by
            // this caller.  Supply its exact source use-site explicitly; a
            // detached expression must never guess an unparse context.
            SgUnparse_Info info;
            SgStatement *useSite = SageInterface::getEnclosingStatement(iname);
            ROSE_ASSERT(useSite != NULL);
            info.set_template_argument_qualification_context(useSite);
            info.set_reference_node_for_qualification(iname);
            ofile << dims[i]->unparseToString(&info) << endl;
          }
        }
      }
    }
  }
}

int main(int argc, char *argv[]) {
  SgProject *project = frontend(argc, argv);

  SgFilePtrList fl = project->get_files();
  SgFile *firstfile = fl[0];
  ROSE_ASSERT(firstfile != NULL);

  string filename =
      Rose::StringUtility::stripPathFromFileName(firstfile->getFileName());
  string ofilename = filename + ".get_C_array_dimensions.output";
  ofile.open(resolveTestOutputPath(ofilename).c_str());
  visitorTraversal myvisitor;
  myvisitor.traverseInputFiles(project, preorder);
  ofile.close();

  return backend(project);
}
