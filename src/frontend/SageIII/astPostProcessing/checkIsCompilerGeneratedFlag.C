#include "checkIsCompilerGeneratedFlag.h"

#include "sage3basic.h"

using namespace Rose;

// documented in header file
size_t checkIsCompilerGeneratedFlag(SgNode *ast) {
  struct T1 : public AstSimpleProcessing {
    size_t nviolations;
    T1() : nviolations(0) {}

    void visit(SgNode *node) {
      SgLocatedNode *located = isSgLocatedNode(node);
      if (located) {
        fix(located, located->get_file_info());
        fix(located, located->generateMatchingFileInfo());
        fix(located, located->get_startOfConstruct());
        fix(located, located->get_endOfConstruct());
      }
    }

    // Mark node as compiler generated and emit a warning if it wasn't already
    // so marked.
    void fix(SgNode *node, Sg_File_Info *finfo) {
      if (finfo && finfo->isFrontendSpecific() &&
          !finfo->isCompilerGenerated()) {
        finfo->setCompilerGenerated();
        ++nviolations;
      }
    }
  } t1;

  t1.traverse(ast, preorder);
  return t1.nviolations;
}
