#include "sage3basic.h"

SgType *SgAddressOfOp::get_type() const {
  // DQ (1/14/2006): p_expression_type has been removed, we have to compute the
  // appropriate type (IR specific code) This function returns a pointer to the
  // type return from get_operand()->get_type().

  ROSE_ASSERT(get_operand() != NULL);
  SgType *baseType = get_operand()->get_type();
  ROSE_ASSERT(baseType != NULL);

  // DQ (7/31/2006): Suggested change by Jeremiah.
  SgClassDefinition *classDefinition = NULL;

  // DQ (7/31/2006): check if this is a data member of a class
  // (and save the class for the SgPointerMemberType::createType() function!)
  SgVarRefExp *varRefExp = isSgVarRefExp(get_operand());
  if (varRefExp != NULL) {
    ROSE_ASSERT(varRefExp->get_symbol() != NULL);
    ROSE_ASSERT(varRefExp->get_symbol()->get_declaration() != NULL);
    SgInitializedName *variable = varRefExp->get_symbol()->get_declaration();
    ROSE_ASSERT(variable != NULL);
    SgScopeStatement *scope = variable->get_scope();
    ROSE_ASSERT(scope != NULL);

    classDefinition = isSgClassDefinition(scope);
  }

  SgType *returnType = NULL;
  if (classDefinition != NULL) {
    SgClassType *classType = classDefinition->get_declaration()->get_type();
    returnType = SgPointerMemberType::createType(baseType, classType);
  } else {
    // Milind Chabbi (8/1/2013) We must strip the reference else we will get
    // pointer to a reference. See CompileTests/Cxx_tests/test2004_157.C run
    // under extractFunctionArgumentsNormalization
    // TODO: should we strip STRIP_TYPEDEF_TYPE? Not sure, need to talk to Dan.
    // DQ (7/16/2014): Dan says no. Don't strip the typedef or we could generate
    // references to types in the AST that might be private or inaccessible when
    // we compile the unparsed the source code). TV (11/13/2018): Added r-value
    // reference handling
    returnType = SgPointerType::createType(baseType->stripType(
        SgType::STRIP_REFERENCE_TYPE | SgType::STRIP_RVALUE_REFERENCE_TYPE));
  }

  //  ROSE_ASSERT(returnType != NULL);
  return returnType;
}
