#include "sage3basic.h"

SgName SgTemplateParameterVal::get_template_parameter_name() const {
  // A value can be nested inside a type argument whose nearest lexical
  // template declaration is unrelated to the non-type parameter it denotes.
  // Recovering identity by walking parents therefore selects the wrong
  // parameter list.  The producer owns the exact spelling, position, and type;
  // require that identity instead of guessing from lexical context.
  if (get_template_parameter_position() < 0 || get_valueString().empty() ||
      get_valueType() == NULL) {
    std::cerr << "REX_AST_INVARIANT[template-parameter-value-identity]: "
                 "value has no exact spelling, position, or type"
              << std::endl;
    ROSE_ABORT();
  }
  return SgName(get_valueString());
}

SgType *SgTemplateParameterVal::get_type() const {
  // DQ (8/6/2013): The correct type is now saved explicitly so that it can be
  // used to disambiguate template functions overloaded on template parameters.
  // See test2013_303.C for an example.
  ROSE_ASSERT(this->get_valueType() != NULL);

  if (get_literal_type() != NULL && get_literal_type() != get_valueType()) {
    std::cerr << "REX_AST_INVARIANT[template-parameter-literal-type]: "
                 "explicit literal type differs from the exact non-type "
                 "template parameter type"
              << std::endl;
    ROSE_ABORT();
  }

  return get_literal_type() != NULL ? get_literal_type() : get_valueType();
}
