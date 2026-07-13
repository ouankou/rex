
#include "unparser.h"

#include "sage3basic.h"

#include "rose_config.h"

#define DEBUG__unparseAssnInit 0

void Unparse_ExprStmt::unparseAssnInit(SgExpression *expr,
                                       SgUnparse_Info &info) {
  SgAssignInitializer *assn_init = isSgAssignInitializer(expr);
  ASSERT_not_null(assn_init);
#if DEBUG__unparseAssnInit
  printf("Enter unparseAssnInit()\n");
  printf("  assn_init = %p = %s\n", assn_init, assn_init->class_name().c_str());
#endif
  SgExpression *operand = assn_init->get_operand_i();
  if (operand == nullptr) {
    fprintf(stderr, "REX_UNPARSE_INVARIANT[assignment-initializer-operand]: "
                    "initializer has no exact operand\n");
    ROSE_ABORT();
  }
  const auto source_form = assn_init->get_source_form();
  const bool allow_include_at_interval_start =
      source_form ==
      SgAssignInitializer::
          e_assignment_initializer_source_include_complete_expansion;
  size_t inside_include_count = 0;
  if (AttachedPreprocessingInfoType *attached =
          assn_init->getAttachedPreprocessingInfo()) {
    for (PreprocessingInfo *record : *attached) {
      if (record == nullptr) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[assignment-initializer-include]: "
                "initializer has a null preprocessing record\n");
        ROSE_ABORT();
      }
      const PreprocessingInfo::DirectiveType type =
          record->getTypeOfDirective();
      if ((type == PreprocessingInfo::CpreprocessorIncludeDeclaration ||
           type == PreprocessingInfo::CpreprocessorIncludeNextDeclaration) &&
          record->getRelativePosition() == PreprocessingInfo::inside) {
        ++inside_include_count;
        Sg_File_Info *initializer_start = assn_init->get_startOfConstruct();
        Sg_File_Info *initializer_end = assn_init->get_endOfConstruct();
        Sg_File_Info *include_location = record->get_file_info();
        auto source_less = [](Sg_File_Info *lhs, Sg_File_Info *rhs) {
          return lhs->get_line() < rhs->get_line() ||
                 (lhs->get_line() == rhs->get_line() &&
                  lhs->get_col() < rhs->get_col());
        };
        if (initializer_start == nullptr || initializer_end == nullptr ||
            include_location == nullptr ||
            initializer_start->get_physical_file_id() < 0 ||
            initializer_end->get_physical_file_id() < 0 ||
            include_location->get_physical_file_id() < 0 ||
            !initializer_start->isSameFile(*initializer_end) ||
            !initializer_start->isSameFile(*include_location) ||
            (!source_less(initializer_start, include_location) &&
             !(allow_include_at_interval_start &&
               initializer_start->get_line() == include_location->get_line() &&
               initializer_start->get_col() == include_location->get_col())) ||
            !source_less(include_location, initializer_end)) {
          fprintf(stderr,
                  "REX_UNPARSE_INVARIANT[assignment-initializer-include]: "
                  "include is not strictly inside its exact initializer "
                  "source interval\n");
          ROSE_ABORT();
        }
      }
    }
  }
  switch (source_form) {
  case SgAssignInitializer::e_assignment_initializer_source_ast:
    if (inside_include_count != 0) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[assignment-initializer-include]: "
              "AST-owned initializer carries %zu include-owned source "
              "boundaries\n",
              inside_include_count);
      ROSE_ABORT();
    }
    break;
  case SgAssignInitializer::
      e_assignment_initializer_source_include_operand_expansion:
  case SgAssignInitializer::
      e_assignment_initializer_source_include_complete_expansion:
    if (inside_include_count != 1) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[assignment-initializer-include]: "
              "include-owned initializer carries %zu exact source "
              "boundaries instead of one\n",
              inside_include_count);
      ROSE_ABORT();
    }
    unparseAttachedPreprocessingInfo(assn_init, info,
                                     PreprocessingInfo::inside);
    return;
  default:
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[assignment-initializer-source-form]: "
            "initializer has invalid source form=%d\n",
            static_cast<int>(assn_init->get_source_form()));
    ROSE_ABORT();
  }
  // Assignment initialization does not determine whether an operand cast was
  // source-written.  SgCastExp::cast_type is the closed source-surface
  // contract: explicit casts emit their syntax and e_implicit_cast validates
  // its synthesized role before emitting only its operand.
  unparseExpression(operand, info);
#if DEBUG__unparseAssnInit
  printf("Leave unparseAssnInit()\n");
#endif
}
