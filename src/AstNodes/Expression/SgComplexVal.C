#include "sage3basic.h"

SgType *SgComplexVal::get_type(void) const {
  // Use the stored SgType to return the correct SgTypeComplex using the correct
  // precision.
  ROSE_ASSERT(p_precisionType != NULL);

  // returns a shared SgTypeComplex type
  SgType *carrierType = SgTypeComplex::createType(p_precisionType);
  ROSE_ASSERT(carrierType != NULL);
  if (get_literal_type() != NULL) {
    if (isSgTypeComplex(get_literal_type()) == NULL ||
        !SageInterface::isEquivalentType(get_literal_type(), carrierType)) {
      fprintf(stderr,
              "REX_AST_INVARIANT[complex-literal-semantic-type]: explicit "
              "literal type does not match its exact precision type\n");
      ROSE_ABORT();
    }
    return get_literal_type();
  }
  return carrierType;
}
