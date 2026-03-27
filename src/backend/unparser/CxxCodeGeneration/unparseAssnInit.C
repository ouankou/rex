
#include "unparser.h"

#include "sage3basic.h"

#include "rose_config.h"

#define DEBUG__unparseAssnInit 0

namespace {

bool hasAttachedIncludeDirective(SgLocatedNode *node) {
  if (node == nullptr) {
    return false;
  }

  AttachedPreprocessingInfoType *infos = node->getAttachedPreprocessingInfo();
  if (infos == nullptr) {
    return false;
  }

  for (PreprocessingInfo *info : *infos) {
    if (info == nullptr) {
      continue;
    }
    PreprocessingInfo::DirectiveType type = info->getTypeOfDirective();
    if (type == PreprocessingInfo::CpreprocessorIncludeDeclaration ||
        type == PreprocessingInfo::CpreprocessorIncludeNextDeclaration) {
      return true;
    }
  }

  return false;
}

bool assignInitializerOperandComesFromAnotherFile(SgAssignInitializer *init) {
  if (init == nullptr) {
    return false;
  }

  SgExpression *operand = init->get_operand_i();
  while (SgCastExp *cast_op = isSgCastExp(operand)) {
    if (cast_op->get_originalExpressionTree() != nullptr) {
      operand = cast_op->get_originalExpressionTree();
    } else {
      operand = cast_op->get_operand_i();
    }
  }

  SgLocatedNode *located_operand = isSgLocatedNode(operand);
  if (located_operand == nullptr ||
      located_operand->get_file_info() == nullptr) {
    return false;
  }

  SgStatement *current_stmt = SageInterface::getEnclosingStatement(init);
  if (current_stmt == nullptr) {
    current_stmt = SageInterface::getEnclosingStatement(located_operand);
  }
  if (current_stmt == nullptr) {
    return false;
  }

  SgFile *current_file = SageInterface::getEnclosingFileNode(current_stmt);
  if (current_file == nullptr || current_file->get_file_info() == nullptr) {
    return false;
  }

  const std::string operand_file =
      located_operand->get_file_info()->get_physical_filename();
  const std::string current_filename =
      current_file->get_file_info()->get_physical_filename();

  return !operand_file.empty() && operand_file != "NULL_FILE" &&
         !current_filename.empty() && operand_file != current_filename;
}

} // namespace

void Unparse_ExprStmt::unparseAssnInit(SgExpression *expr,
                                       SgUnparse_Info &info) {
  SgAssignInitializer *assn_init = isSgAssignInitializer(expr);
  ASSERT_not_null(assn_init);
#if DEBUG__unparseAssnInit
  printf("Enter unparseAssnInit()\n");
  printf("  assn_init = %p = %s\n", assn_init, assn_init->class_name().c_str());
#endif
  const bool suppress_semantic_operand =
      hasAttachedIncludeDirective(assn_init) &&
      assignInitializerOperandComesFromAnotherFile(assn_init);
  if (assn_init->get_is_explicit_cast()) {
    if (!suppress_semantic_operand &&
        assn_init->get_operand()->get_originalExpressionTree() != NULL) {
      unparseExpression(assn_init->get_operand()->get_originalExpressionTree(),
                        info);
    } else if (!suppress_semantic_operand) {
      unparseExpression(assn_init->get_operand(), info);
    }
  } else {
    SgCastExp *castExp = isSgCastExp(assn_init->get_operand());
    if (!suppress_semantic_operand && castExp != NULL) {
      unparseExpression(castExp->get_operand(), info);
    } else if (!suppress_semantic_operand) {
      unparseExpression(assn_init->get_operand(), info);
    }
  }
#if DEBUG__unparseAssnInit
  printf("Leave unparseAssnInit()\n");
#endif
}
