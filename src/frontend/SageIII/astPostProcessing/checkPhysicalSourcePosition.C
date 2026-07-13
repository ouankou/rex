#include "checkPhysicalSourcePosition.h"

#include "sage3basic.h"
#include "sageInterface.h"

using namespace Rose;

// documented in header file
size_t checkPhysicalSourcePosition(SgNode *ast) {
  struct T1 : public AstSimpleProcessing {
    size_t nviolations;
    T1() : nviolations(0) {}

    void visit(SgNode *node) {
      SgLocatedNode *located = isSgLocatedNode(node);
      if (located) {
        Sg_File_Info *primary = located->get_file_info();
        Sg_File_Info *start = located->get_startOfConstruct();
        Sg_File_Info *end = located->get_endOfConstruct();
        if (primary == nullptr || start == nullptr || end == nullptr) {
          SgNode *parent = located->get_parent();
          SgInitializedName *initialized = isSgInitializedName(parent);
          SgFunctionDeclaration *function = isSgFunctionDeclaration(parent);
          fprintf(
              stderr,
              "REX_AST_INVARIANT[physical-source-position]: "
              "node=%p/%s parent=%p/%s initialized-name=%s "
              "declaration=%p function-name=%s function-ownership=%d "
              "function-parent=%p function-scope=%p function-first=%p "
              "function-defining=%p function-primary=%p primary=%p "
              "start=%p end=%p\n",
              static_cast<void *>(located), located->class_name().c_str(),
              static_cast<void *>(parent),
              parent != nullptr ? parent->class_name().c_str() : "<null>",
              initialized != nullptr ? initialized->get_name().str() : "<none>",
              static_cast<void *>(initialized != nullptr
                                      ? initialized->get_declptr()
                                      : nullptr),
              function != nullptr ? function->get_name().str() : "<none>",
              function != nullptr
                  ? static_cast<int>(function->get_frontend_source_ownership())
                  : -1,
              static_cast<void *>(function != nullptr ? function->get_parent()
                                                      : nullptr),
              static_cast<void *>(function != nullptr ? function->get_scope()
                                                      : nullptr),
              static_cast<void *>(
                  function != nullptr
                      ? function->get_firstNondefiningDeclaration()
                      : nullptr),
              static_cast<void *>(function != nullptr
                                      ? function->get_definingDeclaration()
                                      : nullptr),
              static_cast<void *>(
                  function != nullptr ? function->get_file_info() : nullptr),
              static_cast<void *>(primary), static_cast<void *>(start),
              static_cast<void *>(end));
          ROSE_ABORT();
        }
        check(located, primary);
        check(located, start);
        check(located, end);
      }
    }

    void check(SgNode *node, Sg_File_Info *finfo) {
      if (finfo != NULL) {
        if (finfo->get_raw_physical_file_id() >= 0) {
          if (finfo->isFrontendSpecific()) {
            if (SageInterface::hasExactSemanticFrontendSourcePosition(node,
                                                                      finfo)) {
              return;
            }
            fprintf(stderr,
                    "REX_AST_INVARIANT[physical-source-position]: node=%p/%s "
                    "file-info=%p claims both frontend-semantic and physical "
                    "source ownership\n",
                    static_cast<void *>(node),
                    node != nullptr ? node->class_name().c_str() : "<null>",
                    static_cast<void *>(finfo));
            ROSE_ABORT();
          }
          return;
        }
        if (finfo->isTransformation() || finfo->isCompilerGenerated() ||
            finfo->isFrontendSpecific() ||
            finfo->isSourcePositionUnavailableInFrontend()) {
          return;
        }
        if (finfo->get_file_id() >= 0 && finfo->get_physical_file_id() < 0) {
          SgNode *parent = finfo->get_parent();
          ROSE_ASSERT(parent != NULL);
          printf("Detected inconsistant physical source position information: "
                 "%p parent = %p = %s \n",
                 finfo, parent, parent->class_name().c_str());
          finfo->display("checkPhysicalSourcePosition()");

          ROSE_ABORT();
        }
      }
    }
  } t1;

  t1.traverse(ast, preorder);
  return t1.nviolations;
}
