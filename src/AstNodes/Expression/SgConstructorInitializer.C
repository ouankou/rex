#include "sage3basic.h"

/** Obtain the class declaration for a constructor initializer if possible.
 *
 * If this is a constructor initializer for a class type then the first
 * non-defining declaration of the constructor's class is returned; the null
 * pointer is returned for constructor initializers of primitive types.
 *
 * The SgConstructorInitializer::p_declaration points to a
 * SgMemberFunctionDeclaration when the constructor is declared, but is the null
 * pointer when the constructor is not declared or when the constructor is for a
 * non-class type.
 *
 * When the p_declaration is the null pointer then the
 * SgConstructorInitializer::p_expression_type can be used to determine whether
 * the constructor initializer is for a class type without a declared
 * constructor, or for a non-class type.  In the former case, the
 * p_expression_type points to a SgClassType.  In the latter case it points to
 * either a SgFunctionType or null depending on frontend behavior. */
SgClassDeclaration *SgConstructorInitializer::get_class_decl() const {
  SgClassDeclaration *class_decl = NULL;

  auto normalize_class_decl =
      [](SgClassDeclaration *decl) -> SgClassDeclaration * {
    if (decl == NULL) {
      return NULL;
    }

    if (SgClassDeclaration *first_nondef =
            isSgClassDeclaration(decl->get_firstNondefiningDeclaration())) {
      return first_nondef;
    }

    return decl;
  };

  if (SgMemberFunctionDeclaration *fdecl = get_declaration()) {
    // The constructor is declared (and is a member of a class type)
    // DQ (11/11/2014): Modified version of code to address the fact that
    // Fortran is organized a bit differently. That it is different from C++
    // might be something to revisit later in the Fortran support.
    ROSE_ASSERT(get_declaration() != NULL);
    ROSE_ASSERT(fdecl->get_scope() != NULL);

    SgClassDefinition *cdef = isSgClassDefinition(fdecl->get_scope());
    if (cdef != NULL) {
      assert(cdef != NULL);
      class_decl = isSgClassDeclaration(cdef->get_parent());
      assert(class_decl != NULL);
    }
  } else if (SgType *expr_type = get_expression_type()) {
    SgType *stripped_type = expr_type->stripType(
        SgType::STRIP_MODIFIER_TYPE | SgType::STRIP_TYPEDEF_TYPE |
        SgType::STRIP_REFERENCE_TYPE | SgType::STRIP_RVALUE_REFERENCE_TYPE);

    if (SgClassType *class_type = isSgClassType(stripped_type)) {
      // Constructor initializer is for a class type with a non-declared
      // default constructor. We can follow the expression type's declaration.
      class_decl = isSgClassDeclaration(class_type->get_declaration());
      assert(class_decl != NULL);
    } else if (SgNonrealType *nonreal_type = isSgNonrealType(stripped_type)) {
      // Qualified class names can be preserved as SgNonrealType. Recover the
      // associated class declaration when the frontend attached it to the
      // nonreal declaration.
      if (SgNonrealDecl *nonreal_decl =
              isSgNonrealDecl(nonreal_type->get_declaration())) {
        class_decl =
            isSgClassDeclaration(nonreal_decl->get_templateDeclaration());
      }
    }
  } else {
    // Constructor is for a primitive type.  There is no class declaration to
    // return.
  }

  return normalize_class_decl(class_decl);
}

void SgConstructorInitializer::post_construction_initialization() {
  if (p_expression_type == nullptr ||
      isSgTypeUnknown(p_expression_type) != nullptr ||
      isSgTypeDefault(p_expression_type) != nullptr) {
    fprintf(stderr,
            "REX_AST_INVARIANT[exact-expression-type]: "
            "node=SgConstructorInitializer has no exact semantic result "
            "type\n");
    ROSE_ABORT();
  }
  if (p_declaration == NULL) {
    // This can be NULL for the case of an undeclared constructor.

    SgType *associated_type =
        p_expression_type != NULL
            ? p_expression_type->stripType(SgType::STRIP_MODIFIER_TYPE |
                                           SgType::STRIP_TYPEDEF_TYPE |
                                           SgType::STRIP_REFERENCE_TYPE |
                                           SgType::STRIP_RVALUE_REFERENCE_TYPE)
            : NULL;
    bool has_associated_class = false;
    if (SgClassType *class_type = isSgClassType(associated_type)) {
      has_associated_class = class_type->get_declaration() != NULL;
    } else if (SgNonrealType *nonreal_type = isSgNonrealType(associated_type)) {
      if (SgNonrealDecl *nonreal_decl =
              isSgNonrealDecl(nonreal_type->get_declaration())) {
        has_associated_class =
            isSgClassDeclaration(nonreal_decl->get_templateDeclaration()) !=
            NULL;
      }
    }
    ROSE_ASSERT(has_associated_class || (p_associated_class_unknown == true));
  } else {
    ROSE_ASSERT(p_expression_type != NULL);
  }

  if (p_args == NULL) {
    fprintf(stderr, "REX_AST_INVARIANT[constructor-initializer-arguments]: "
                    "constructor initializer has no exact argument list\n");
    ROSE_ABORT();
  }
  if (p_args->get_parent() != nullptr && p_args->get_parent() != this) {
    fprintf(stderr, "REX_AST_INVARIANT[constructor-initializer-arguments]: "
                    "constructor initializer argument list is already owned\n");
    ROSE_ABORT();
  }
  p_args->set_parent(this);
}

SgExpression *SgConstructorInitializer::get_next(int &n) const {
  if (n == 0) {
    n++;
    return get_args();
  }
  return 0;
}

int SgConstructorInitializer::replace_expression(SgExpression *o,
                                                 SgExpression *n) {
  // DQ (12/17/2006): This function should have the semantics that it will
  // represent a structural change to the AST, thus it is free to set the parent
  // of the new expression.

  ROSE_ASSERT(o != NULL);
  ROSE_ASSERT(n != NULL);

  if (get_args() == o) {
    set_args(isSgExprListExp(n));
    n->set_parent(this);
    return 1;
  } else
    return 0;
}

// DQ (6/11/2015): Moved these six access functions, they should not be
// generated by ROSETTA so that we could avoid them setting the isModified flag
// which is a problem in the name qualification support for C++ (interfering
// with the token-based unparsing).
int SgConstructorInitializer::get_name_qualification_length() const {
  ROSE_ASSERT(this != NULL);
  return p_name_qualification_length;
}

void SgConstructorInitializer::set_name_qualification_length(
    int name_qualification_length) {
  ROSE_ASSERT(this != NULL);
  // This can't be called by the name qualification API (see test2015_26.C).
  // set_isModified(true);

  p_name_qualification_length = name_qualification_length;
}

bool SgConstructorInitializer::get_type_elaboration_required() const {
  ROSE_ASSERT(this != NULL);

  return p_type_elaboration_required;
}

void SgConstructorInitializer::set_type_elaboration_required(
    bool type_elaboration_required) {
  ROSE_ASSERT(this != NULL);
  // This can't be called by the name qualification API (see test2015_26.C).
  // set_isModified(true);

  p_type_elaboration_required = type_elaboration_required;
}

bool SgConstructorInitializer::get_global_qualification_required() const {
  ROSE_ASSERT(this != NULL);
  return p_global_qualification_required;
}

void SgConstructorInitializer::set_global_qualification_required(
    bool global_qualification_required) {
  ROSE_ASSERT(this != NULL);

  // This can't be called by the name qualification API (see test2015_26.C).
  // set_isModified(true);

  p_global_qualification_required = global_qualification_required;
}
