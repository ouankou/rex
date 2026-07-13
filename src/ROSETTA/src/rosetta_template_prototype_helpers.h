#ifndef ROSETTA_TEMPLATE_PROTOTYPE_HELPERS_H
#define ROSETTA_TEMPLATE_PROTOTYPE_HELPERS_H

#include "AstNodeClass.h"
#include "ROSETTA_macros.h"

inline void addConstraintSatisfactionPrototypes(AstNodeClass &target) {
  // REX: Constraint satisfaction results as core IR.
  target.setDataPrototype("bool", "constraintSatisfactionEvaluated", "= false",
                          NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                          NO_TRAVERSAL, NO_DELETE);
  target.setDataPrototype("bool", "constraintSatisfactionSatisfied", "= true",
                          NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                          NO_TRAVERSAL, NO_DELETE);
  target.setDataPrototype("bool", "constraintSatisfactionContainsErrors",
                          "= false", NO_CONSTRUCTOR_PARAMETER,
                          BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  target.setDataPrototype("bool", "constraintSatisfactionSubstitutionFailure",
                          "= false", NO_CONSTRUCTOR_PARAMETER,
                          BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  target.setDataPrototype("std::string", "constraintSatisfactionSummary",
                          "= \"\"", NO_CONSTRUCTOR_PARAMETER,
                          BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
}

inline void addSFINAEPrototypes(AstNodeClass &target) {
  // REX: SFINAE (non-constraint) substitution failure results.
  target.setDataPrototype("bool", "sfinaeEvaluated", "= false",
                          NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                          NO_TRAVERSAL, NO_DELETE);
  target.setDataPrototype("bool", "sfinaeSubstitutionFailure", "= false",
                          NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                          NO_TRAVERSAL, NO_DELETE);
  target.setDataPrototype("std::string", "sfinaeSummary", "= \"\"",
                          NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                          NO_TRAVERSAL, NO_DELETE);
}

inline void addTemplateInstantiationPrototypes(AstNodeClass &target) {
  // REX: Store deduced template arguments (distinct from written/pattern args).
  target.setDataPrototype(
      "SgTemplateArgumentPtrList", "deducedTemplateArguments",
      "= SgTemplateArgumentPtrList()", NO_CONSTRUCTOR_PARAMETER,
      BUILD_LIST_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE);
  // REX: Track the specialized template (primary or partial) chosen by Clang.
  target.setDataPrototype("SgDeclarationStatement*",
                          "specializedTemplateDeclaration", "= NULL",
                          NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                          NO_TRAVERSAL, NO_DELETE);
  addConstraintSatisfactionPrototypes(target);
  addSFINAEPrototypes(target);
}

#endif // ROSETTA_TEMPLATE_PROTOTYPE_HELPERS_H
