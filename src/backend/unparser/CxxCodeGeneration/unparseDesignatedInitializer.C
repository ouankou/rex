
#include "unparser.h"

#include "sage3basic.h"

#include "rose_config.h"

#define DEBUG__unparseDesignatedInitializer 0

void Unparse_ExprStmt::unparseDesignatedInitializer(SgExpression *expr,
                                                    SgUnparse_Info &info) {
  SgDesignatedInitializer *design_init = isSgDesignatedInitializer(expr);
#if DEBUG__unparseDesignatedInitializer
  printf("Enter unparseDesignatedInitializer()\n");
  printf("  design_init = %p = %s\n", design_init,
         design_init->class_name().c_str());
#endif
  ASSERT_not_null(design_init);
  SgExprListExp *designator_list = design_init->get_designatorList();
  if (designator_list == nullptr ||
      designator_list->get_parent() != design_init ||
      designator_list->get_expressions().empty()) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[designator-list]: designated initializer "
            "has no exact nonempty designator list\n");
    ROSE_ABORT();
  }
  SgInitializer *initializer = design_init->get_memberInit();
  if (initializer == nullptr || initializer->get_parent() != design_init ||
      initializer->get_type() == nullptr ||
      isSgTypeUnknown(initializer->get_type()) != nullptr ||
      isSgTypeDefault(initializer->get_type()) != nullptr) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[designated-member-initializer]: designated "
            "initializer has no exact owned typed member initializer\n");
    ROSE_ABORT();
  }
#if DEBUG__unparseDesignatedInitializer
  printf("  initializer = %p = %s\n", initializer,
         initializer ? initializer->class_name().c_str() : "");
#endif
  const SgExpressionPtrList &designators = designator_list->get_expressions();
  for (size_t index = 0; index < designators.size(); ++index) {
    SgDesignator *designator = isSgDesignator(designators[index]);
    if (designator == nullptr || designator->get_parent() != designator_list) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[designator-element]: designator %zu is "
              "not an exactly owned SgDesignator\n",
              index);
      ROSE_ABORT();
    }
    designator->validate_designator();
    SgExpression *first = designator->get_first_expression();
    switch (designator->get_kind()) {
    case SgDesignator::e_designator_field: {
      SgVarRefExp *field = isSgVarRefExp(first);
      SgVariableSymbol *field_symbol =
          field != nullptr ? field->get_symbol() : nullptr;
      SgInitializedName *field_declaration =
          field_symbol != nullptr ? field_symbol->get_declaration() : nullptr;
      if (field_declaration == nullptr ||
          field_declaration->get_name().is_null() ||
          field_symbol->get_name() != field_declaration->get_name()) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[designator-field]: field designator "
                "has no exact named variable-symbol identity\n");
        ROSE_ABORT();
      }
      curprint(".");
      // C and C++ field-designator grammar accepts an identifier, never a
      // qualified-id.  The exact field symbol is the typed spelling source;
      // ordinary variable-reference qualification is invalid in this role.
      curprint(field_symbol->get_name().str());
      break;
    }
    case SgDesignator::e_designator_array:
      curprint("[");
      unparseExpression(first, info);
      curprint("]");
      break;
    case SgDesignator::e_designator_array_range:
      curprint("[");
      unparseExpression(first, info);
      curprint(" ... ");
      unparseExpression(designator->get_second_expression(), info);
      curprint("]");
      break;
    case SgDesignator::e_designator_unclassified:
    default:
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[designator-kind]: designator %zu has "
              "invalid kind=%d\n",
              index, static_cast<int>(designator->get_kind()));
      ROSE_ABORT();
    }
  }

  curprint(" = ");
  unparseExpression(initializer, info);
#if DEBUG__unparseDesignatedInitializer
  printf("Leave unparseDesignatedInitializer()\n");
#endif
}
