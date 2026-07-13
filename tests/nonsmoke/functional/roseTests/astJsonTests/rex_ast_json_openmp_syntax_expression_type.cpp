#include "rose.h"

#include <cstdio>
#include <string>

int main(int argc, char **argv) {
  if (argc != 2) {
    return 1;
  }
  const std::string mode = argv[1];
  if (mode == "name") {
    (new SgOmpNameExpression("rex_name_property"))->get_type();
  } else if (mode == "source") {
    (new SgOmpSourceExpression("nested(7)"))->get_type();
  } else if (mode == "classification") {
    SgStringVal *unevaluatedString = new SgStringVal("rex_unevaluated_message");
    unevaluatedString->set_cxx_unevaluated(true);
    SgExpression *syntaxExpressions[] = {
        new SgFortranCommonBlockRefExp(
            SgName("rex_common"), static_cast<SgCommonBlockObject *>(nullptr)),
        new SgOmpNameExpression("rex_name_property"),
        new SgOmpSourceExpression("nested(7)"),
        new SgOmpInductionItem(SgOmpClause::e_omp_induction_item_expression, "",
                               static_cast<SgExpression *>(nullptr)),
        new SgOmpApplyTransformation(SgOmpClause::e_omp_apply_transform_nothing,
                                     SgOmpClause::e_omp_clause_separator_none,
                                     "", static_cast<SgExpression *>(nullptr),
                                     static_cast<SgOmpApplyClause *>(nullptr)),
        new SgOmpInitModifier(SgOmpClause::e_omp_init_modifier_depobj,
                              static_cast<SgExpression *>(nullptr)),
        new SgOmpInitModifierList(),
        new SgOmpAppendArgsOperation(new SgOmpInitModifierList()),
        new SgOmpMapDistDataPolicy(SgOmpClause::e_omp_map_dist_data_duplicate,
                                   static_cast<SgExpression *>(nullptr)),
        new SgOmpMapItem(static_cast<SgExpression *>(nullptr)),
        unevaluatedString,
    };
    for (SgExpression *expression : syntaxExpressions) {
      if (expression->has_semantic_value_type()) {
        std::fprintf(stderr, "%s was classified as a value expression\n",
                     expression->sage_class_name());
        return 2;
      }
    }
    SgExpression *valueExpressions[] = {
        new SgIntVal(1, "1"),
        SageBuilder::buildStringVal("rex_value_expression"),
    };
    for (SgExpression *expression : valueExpressions) {
      if (!expression->has_semantic_value_type() ||
          expression->get_type() == nullptr) {
        std::fprintf(stderr,
                     "%s was not classified and typed as a value expression\n",
                     expression->sage_class_name());
        return 2;
      }
    }
  } else {
    return 1;
  }
  return 0;
}
