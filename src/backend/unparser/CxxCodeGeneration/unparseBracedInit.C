
#include "unparser.h"

#include "sage3basic.h"

#include "rose_config.h"

void Unparse_ExprStmt::unparseBracedInit(SgExpression *expr,
                                         SgUnparse_Info &info) {
  SgBracedInitializer *braced_init = isSgBracedInitializer(expr);
  ASSERT_not_null(braced_init);
  SgUnparse_Info newinfo(info);

  SgExprListExp *initializers = braced_init->get_initializers();
  if (initializers == nullptr) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[braced-initializer-list]: initializer "
            "has no exact expression list\n");
    ROSE_ABORT();
  }
  const SgExpressionPtrList &args = initializers->get_expressions();

  curprint("{");
  for (size_t index = 0; index < args.size(); ++index) {
    if (args[index] == nullptr) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[braced-initializer-element]: "
              "initializer element %zu is null\n",
              index);
      ROSE_ABORT();
    }
    unparseExpression(args[index], newinfo);
    if (index != args.size() - 1)
      curprint(", ");
  }
  unparseAttachedPreprocessingInfo(braced_init, info,
                                   PreprocessingInfo::inside);
  curprint("}");
}
