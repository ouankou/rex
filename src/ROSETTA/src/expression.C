#include "AstNodeClass.h"

#include "ROSETTA_macros.h"

#include "rosetta_template_prototype_helpers.h"

#include "grammar.h"

void Grammar::setUpExpressions() {
  // ROSETTA requires derived classes to be declared before their base class.

  //                                  Terminal classes in alphabetical order

  // DQ (12/31/2007): This expression represents constructions such as "DIM=N"
  // which binds a function parameter name with a function argument as in:
  // "sum(array,DIM=1)". DQ (12/31/2007): Support for named actual arguments to
  // functions (fortran specific).
  NEW_TERMINAL_MACRO(ActualArgumentExpression, "ActualArgumentExpression",
                     "ACTUAL_ARGUMENT_EXPRESSION");
  ActualArgumentExpression.editSubstitute("PRECEDENCE_VALUE", " 2");
  ActualArgumentExpression.setFunctionPrototype(
      "HEADER_ACTUAL_ARGUMENT_EXPRESSION", "../Grammar/Expression.code");
  ActualArgumentExpression.setDataPrototype(
      "SgName", "argument_name", "= \"\"", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  ActualArgumentExpression.setDataPrototype(
      "SgExpression*", "expression", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);

  NEW_TERMINAL_MACRO(AddOp, "AddOp", "ADD_OP");
  AddOp.setFunctionSource("SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
                          "../Grammar/Expression.code");
  AddOp.editSubstitute("PRECEDENCE_VALUE", "12");

  NEW_TERMINAL_MACRO(AddressOfOp, "AddressOfOp", "ADDRESS_OP");
  AddressOfOp.setDataPrototype(
      "SgAddressOfOp::address_operand_form_enum", "address_operand_form", "",
      CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  AddressOfOp.editSubstitute("PRECEDENCE_VALUE", "15");
  AddressOfOp.setFunctionPrototype("HEADER_ADDRESS_OF_OPERATOR",
                                   "../Grammar/Expression.code");

  NEW_TERMINAL_MACRO(AggregateInitializer, "AggregateInitializer",
                     "AGGREGATE_INIT");
  AggregateInitializer.setFunctionPrototype(
      "HEADER_AGGREGATE_INITIALIZER_EXPRESSION", "../Grammar/Expression.code");

  AggregateInitializer.editSubstitute("HEADER_LIST_DECLARATIONS",
                                      "HEADER_LIST_FUNCTIONS",
                                      "../Grammar/Expression.code");
  AggregateInitializer.editSubstitute("LIST_NAME", "initializer");
  AggregateInitializer.setDataPrototype(
      "SgExprListExp*", "initializers", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);
  AggregateInitializer.setDataPrototype(
      "SgType*", "expression_type", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  // Fortran array constructors have two independent type facts: the resolved
  // semantic type of the expression above, and whether the source explicitly
  // spelled an element type before "::".  Do not encode absent source syntax
  // with a null/default/unknown semantic expression type.
  AggregateInitializer.setDataPrototype(
      "bool", "fortran_has_source_explicit_type", "= false",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);
  AggregateInitializer.setDataPrototype(
      "SgType*", "fortran_source_explicit_type", "= NULL",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);
  // An explicitly typed Fortran array constructor can name a local derived
  // type alias.  Record that exact source binding independently of the
  // canonical expression and element types.
  AggregateInitializer.setDataPrototype(
      "SgSymbol*", "fortran_source_derived_type_symbol", "= NULL",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE,
      COPY_DATA);
  // The producer owns the exact source surface.  The backend must never infer
  // braces or a type prefix from parent/child ancestry.
  AggregateInitializer.setDataPrototype(
      "SgAggregateInitializer::aggregate_initializer_source_form_enum",
      "source_form",
      "= SgAggregateInitializer::e_aggregate_initializer_source_unclassified",
      CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  // A preprocessor include can own one or more direct source elements of a
  // braced initializer.  Preserve that producer decision explicitly: the
  // unparser must consume the include directive instead of emitting the
  // expanded elements a second time.
  AggregateInitializer.setDataPrototype(
      "SgUnsignedCharList", "source_element_roles", "",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE,
      COPY_DATA);

  // Qualification accessors must not mark the AST as modified.
  AggregateInitializer.setDataPrototype(
      "bool", "requiresGlobalNameQualificationOnType", "= false",
      NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  AggregateInitializer.setDataPrototype(
      "int", "name_qualification_length_for_type", "= 0",
      NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  AggregateInitializer.setDataPrototype(
      "bool", "type_elaboration_required_for_type", "= false",
      NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  AggregateInitializer.setDataPrototype(
      "bool", "global_qualification_required_for_type", "= false",
      NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  NEW_TERMINAL_MACRO(AlignOfOp, "AlignOfOp", "ALIGNOF_OP");
  AlignOfOp.setFunctionSource("SOURCE_UNARY_TYPE_TRAIT_POST_CONSTRUCTION",
                              "../Grammar/Expression.code");
  AlignOfOp.setFunctionPrototype("HEADER_UNARY_TYPE_TRAIT_SEMANTICS",
                                 "../Grammar/Expression.code");
  AlignOfOp.editSubstitute("PRECEDENCE_VALUE", "16");
  AlignOfOp.setFunctionPrototype("HEADER_ALIGNOF_OPERATOR",
                                 "../Grammar/Expression.code");
  // alignof has exactly one of an expression operand or a type operand.  The
  // expression edge is therefore conditional for traversal;
  // SgAlignOfOp::get_type enforces the cross-field XOR contract directly.
  AlignOfOp.setDataPrototype("SgExpression*", "operand_expr", "",
                             CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                             DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
                             OPTIONAL_TRAVERSAL_MEMBER);
  AlignOfOp.setDataPrototype("SgType*", "operand_type", "",
                             CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                             NO_TRAVERSAL, NO_DELETE, COPY_REFERENCE);
  AlignOfOp.setDataPrototype(
      "SgDeclarationStatement*", "type_defining_declaration", "= NULL",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL,
      NO_DELETE, COPY_DATA, OPTIONAL_TRAVERSAL_MEMBER);
  // alignof has the target's size_t type.  The operand does not carry that ABI
  // fact, so the frontend or transformation producer must supply it exactly.
  AlignOfOp.setDataPrototype("SgType*", "expression_type", "",
                             CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                             NO_TRAVERSAL, NO_DELETE);
  AlignOfOp.setDataPrototype(
      "SgExpression::unary_type_trait_kind_enum", "operator_kind",
      "= SgExpression::e_preferred_alignof", CONSTRUCTOR_PARAMETER,
      NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  AlignOfOp.setDataPrototype("std::optional<Rose::Integer128Value>",
                             "evaluated_value", "= std::nullopt",
                             CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                             NO_TRAVERSAL, NO_DELETE, COPY_DATA);
  AlignOfOp.setDataPrototype("int", "name_qualification_length", "= 0",
                             NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                             NO_TRAVERSAL, NO_DELETE);

  AlignOfOp.setDataPrototype("bool", "type_elaboration_required", "= false",
                             NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                             NO_TRAVERSAL, NO_DELETE);
  AlignOfOp.setDataPrototype(
      "SgNonrealDecl::source_elaboration_kind_enum",
      "source_type_elaboration_kind",
      "= SgNonrealDecl::e_source_elaboration_unspecified",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);

  AlignOfOp.setDataPrototype("bool", "global_qualification_required", "= false",
                             NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                             NO_TRAVERSAL, NO_DELETE);

  NEW_TERMINAL_MACRO(AndAssignOp, "AndAssignOp", "AND_ASSIGN_OP");
  AndAssignOp.setFunctionSource("SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
                                "../Grammar/Expression.code");
  AndAssignOp.editSubstitute("PRECEDENCE_VALUE", " 2");

  NEW_TERMINAL_MACRO(AndOp, "AndOp", "AND_OP");
  AndOp.setFunctionSource("SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
                          "../Grammar/Expression.code");
  AndOp.editSubstitute("PRECEDENCE_VALUE", " 5");
  AndOp.setFunctionPrototype("HEADER_AND_OPERATOR",
                             "../Grammar/Expression.code");

  NEW_TERMINAL_MACRO(ArrowExp, "ArrowExp", "POINTST_OP");
  ArrowExp.setFunctionSource("SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
                             "../Grammar/Expression.code");
  ArrowExp.editSubstitute("PRECEDENCE_VALUE", "16");
  ArrowExp.setFunctionPrototype("HEADER_ARROW_EXPRESSION",
                                "../Grammar/Expression.code");
  ArrowExp.setDataPrototype("SgArrowExp::emission_role_enum", "emission_role",
                            "= SgArrowExp::e_emit_arrow_operator",
                            NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                            NO_TRAVERSAL, NO_DELETE);

  NEW_TERMINAL_MACRO(ArrowStarOp, "ArrowStarOp", "ARROWSTAR_OP");
  ArrowStarOp.setFunctionSource("SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
                                "../Grammar/Expression.code");
  ArrowStarOp.editSubstitute("PRECEDENCE_VALUE", "14");
  ArrowStarOp.setFunctionPrototype("HEADER_ARROW_STAR_OPERATOR",
                                   "../Grammar/Expression.code");

  // DQ (7/22/2006): Support for ASM operands that are contained in an SgAsmStmt
  NEW_TERMINAL_MACRO(AsmOp, "AsmOp", "ASM_OP");
  AsmOp.setFunctionSource("SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
                          "../Grammar/Expression.code");
  AsmOp.editSubstitute("PRECEDENCE_VALUE", "16");
  AsmOp.setFunctionPrototype("HEADER_ASM_OP", "../Grammar/Expression.code");
  AsmOp.setDataPrototype("SgAsmOp::constraint_info", "constraint",
                         "= SgAsmOp::constraint_info()", CONSTRUCTOR_PARAMETER,
                         BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  AsmOp.setDataPrototype("SgAsmOp::asm_operand_modifier_enum", "modifiers",
                         "= SgAsmOp::e_unknown", CONSTRUCTOR_PARAMETER,
                         BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  AsmOp.setDataPrototype("SgExpression*", "expression", "= NULL",
                         CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                         DEF_TRAVERSAL, DEF_DELETE, CLONE_TREE,
                         OPTIONAL_TRAVERSAL_MEMBER);

  AsmOp.setDataPrototype("std::string", "name", "= \"\"",
                         NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                         NO_TRAVERSAL, NO_DELETE);

  NEW_TERMINAL_MACRO(AssignInitializer, "AssignInitializer", "ASSIGN_INIT");
  AssignInitializer.setFunctionPrototype(
      "HEADER_ASSIGNMENT_INITIALIZER_EXPRESSION", "../Grammar/Expression.code");
  AssignInitializer.setDataPrototype(
      "SgExpression*", "operand_i", "", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      REQUIRED_TRAVERSAL_MEMBER);
  // Assignment initialization is a destination conversion.  Its exact result
  // type is frontend/producer data and cannot be inferred from the operand.
  AssignInitializer.setDataPrototype("SgType*", "expression_type", "",
                                     CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                                     NO_TRAVERSAL, NO_DELETE);
  // A preprocessing include can own either the operand after a source-written
  // '=' or the complete '= operand' initializer.  Preserve that exact source
  // grammar so the backend neither emits the semantic expansion nor invents
  // or duplicates the assignment introducer.
  AssignInitializer.setDataPrototype(
      "SgAssignInitializer::assignment_initializer_source_form_enum",
      "source_form",
      "= SgAssignInitializer::e_assignment_initializer_source_ast",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);

  NEW_TERMINAL_MACRO(AssignOp, "AssignOp", "ASSIGN_OP");
  AssignOp.setFunctionSource("SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
                             "../Grammar/Expression.code");
  AssignOp.editSubstitute("PRECEDENCE_VALUE", " 2");
  AssignOp.setFunctionPrototype("HEADER_ASSIGNMENT_OPERATOR",
                                "../Grammar/Expression.code");

  NEW_TERMINAL_MACRO(AsteriskShapeExp, "AsteriskShapeExp",
                     "TEMP_AsteriskShapeExp");
  AsteriskShapeExp.editSubstitute("PRECEDENCE_VALUE", " 2");
  AsteriskShapeExp.setFunctionPrototype("HEADER_ASTERISK_SHAPE_EXPRESSION",
                                        "../Grammar/Expression.code");

  NEW_TERMINAL_MACRO(AssumedRankExp, "AssumedRankExp", "TEMP_AssumedRankExp");
  AssumedRankExp.editSubstitute("PRECEDENCE_VALUE", " 2");
  AssumedRankExp.setFunctionPrototype("HEADER_ASSUMED_RANK_EXPRESSION",
                                      "../Grammar/Expression.code");

  NEW_TERMINAL_MACRO(AwaitExpression, "AwaitExpression", "AWAIT_EXPR");
  AwaitExpression.setFunctionSource(
      "SOURCE_AWAIT_EXPRESSION_POST_CONSTRUCTION_INITIALIZATION",
      "../Grammar/Expression.code");
  AwaitExpression.setFunctionPrototype("HEADER_AWAIT_EXPRESSION",
                                       "../Grammar/Expression.code");
  AwaitExpression.setDataPrototype(
      "SgExpression*", "value", "", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);
  // Coroutine lowering computes the value category and semantic result type.
  // The awaiter operand alone cannot recover await_resume conversions.
  AwaitExpression.setDataPrototype(
      "SgType*", "expression_type", "", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  AwaitExpression.setDataPrototype(
      "SgAwaitExpression::coroutine_keyword_kind_enum",
      "coroutine_keyword_kind", "", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  NEW_TERMINAL_MACRO(BitAndOp, "BitAndOp", "BITAND_OP");
  BitAndOp.setFunctionSource("SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
                             "../Grammar/Expression.code");
  BitAndOp.editSubstitute("PRECEDENCE_VALUE", " 8");

  NEW_TERMINAL_MACRO(BitComplementOp, "BitComplementOp", "BIT_COMPLEMENT_OP");
  BitComplementOp.setFunctionSource(
      "SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
      "../Grammar/Expression.code");
  BitComplementOp.editSubstitute("PRECEDENCE_VALUE", "15");

  NEW_TERMINAL_MACRO(BitEqvOp, "BitEqvOp", "BITEQV_OP");
  BitEqvOp.setFunctionSource("SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
                             "../Grammar/Expression.code");
  BitEqvOp.editSubstitute("PRECEDENCE_VALUE", " 6");

  NEW_TERMINAL_MACRO(BitOrOp, "BitOrOp", "BITOR_OP");
  BitOrOp.setFunctionSource("SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
                            "../Grammar/Expression.code");
  BitOrOp.editSubstitute("PRECEDENCE_VALUE", " 6");

  NEW_TERMINAL_MACRO(BitXorOp, "BitXorOp", "BITXOR_OP");
  BitXorOp.setFunctionSource("SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
                             "../Grammar/Expression.code");
  BitXorOp.editSubstitute("PRECEDENCE_VALUE", " 7");

  NEW_TERMINAL_MACRO(BoolValExp, "BoolValExp", "BOOL_VAL");
  BoolValExp.setFunctionSource("SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
                               "../Grammar/Expression.code");
  BoolValExp.setDataPrototype("int", "value", "= 0", CONSTRUCTOR_PARAMETER,
                              BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  BoolValExp.setFunctionSource("SOURCE_GET_TYPE_VALUE",
                               "../Grammar/Expression.code");
  BoolValExp.editSubstitute("GENERIC_TYPE", "SgTypeBool");

  NEW_TERMINAL_MACRO(BracedInitializer, "BracedInitializer", "BRACED_INIT");
  BracedInitializer.setFunctionPrototype("HEADER_BRACED_INITIALIZER_EXPRESSION",
                                         "../Grammar/Expression.code");
  BracedInitializer.editSubstitute("HEADER_LIST_DECLARATIONS",
                                   "HEADER_LIST_FUNCTIONS",
                                   "../Grammar/Expression.code");
  BracedInitializer.editSubstitute("LIST_NAME", "initializer");
  BracedInitializer.setDataPrototype(
      "SgExprListExp*", "initializers", "", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      REQUIRED_TRAVERSAL_MEMBER);
  BracedInitializer.setDataPrototype("SgType*", "expression_type", "",
                                     CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                                     NO_TRAVERSAL, NO_DELETE);

  // FMZ (2/6/2009): Added CoArray Reference Expression
  NEW_TERMINAL_MACRO(CAFCoExpression, "CAFCoExpression", "COARRAY_REF_EXPR");
  // FMZ (2/6/2009): Added for SgCAFCoExpression--following SgPntrArrRefExp
  CAFCoExpression.editSubstitute("PRECEDENCE_VALUE", " 16");
  // FMZ (2/5/2009): Added for CAFCoExpression => change teamId from "SgName" to
  // "SgVarRefExp*"

  CAFCoExpression.setDataPrototype(
      "SgVarRefExp*", "teamId", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);
  CAFCoExpression.setDataPrototype(
      "SgExpression*", "teamRank", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);
  CAFCoExpression.setDataPrototype(
      "SgExpression*", "referData", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);
  CAFCoExpression.setDataPrototype(
      "SgType*", "expression_type", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  CAFCoExpression.setFunctionSource(
      "SOURCE_CAF_CO_EXPRESSION_POST_CONSTRUCTION_INITIALIZATION",
      "../Grammar/Expression.code");

  // OpenACC/CAF image selector expression for coarray references
  NEW_TERMINAL_MACRO(CAFImageSelectorExp, "CAFImageSelectorExp",
                     "COARRAY_IMAGE_SELECTOR");
  CAFImageSelectorExp.editSubstitute("PRECEDENCE_VALUE", " 16");
  CAFImageSelectorExp.setDataPrototype(
      "SgExprListExp*", "cosubscripts", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);
  CAFImageSelectorExp.setDataPrototype(
      "SgExpression*", "stat_expression", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);
  CAFImageSelectorExp.setDataPrototype(
      "SgExpression*", "team_expression", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);
  CAFImageSelectorExp.setDataPrototype(
      "SgExpression*", "team_number_expression", "= NULL",
      CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE,
      COPY_DATA, OPTIONAL_TRAVERSAL_MEMBER);
  CAFImageSelectorExp.setFunctionSource(
      "SOURCE_CAF_IMAGE_SELECTOR_POST_CONSTRUCTION_INITIALIZATION",
      "../Grammar/Expression.code");

  NEW_TERMINAL_MACRO(CastExp, "CastExp", "CAST_OP");
  CastExp.editSubstitute("PRECEDENCE_VALUE", "15");
  CastExp.setFunctionPrototype("HEADER_CAST_EXPRESSION",
                               "../Grammar/Expression.code");

  CastExp.setDataPrototype("SgCastExp::cast_type_enum", "cast_type", "",
                           CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                           NO_TRAVERSAL, NO_DELETE);

  CastExp.setDataPrototype("SgCastExp::semantic_conversion_kind_enum",
                           "semantic_conversion_kind", "",
                           CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                           NO_TRAVERSAL, NO_DELETE, COPY_DATA);

  CastExp.setDataPrototype("SgCastExp::value_category_enum", "value_category",
                           "", CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                           NO_TRAVERSAL, NO_DELETE, COPY_DATA);

  // The exact sequence of semantic base types traversed by Clang for
  // base/derived and member-pointer conversions.  The cast owns the vector;
  // the canonical types themselves are shared, as all SgType objects are.
  CastExp.setDataPrototype("SgTypePtrList", "conversion_base_path", "",
                           CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                           NO_TRAVERSAL, NO_DELETE, COPY_DATA);

  // The expression type is the exact semantic result published by Clang.
  // Explicit cast syntax can name a source-distinct typedef, alias template,
  // elaborated type, or dependent type surface, so preserve that written type
  // on the use site instead of replacing the semantic expression type.
  CastExp.setDataPrototype("SgType*", "source_type", "", CONSTRUCTOR_PARAMETER,
                           BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE,
                           CLONE_TREE);

  // Qualification accessors do not mark the node modified.
  CastExp.setDataPrototype("int", "name_qualification_length", "= 0",
                           NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                           NO_TRAVERSAL, NO_DELETE);

  CastExp.setDataPrototype("bool", "type_elaboration_required", "= false",
                           NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                           NO_TRAVERSAL, NO_DELETE);

  CastExp.setDataPrototype("bool", "global_qualification_required", "= false",
                           NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                           NO_TRAVERSAL, NO_DELETE);

  // Preserve the exact nested-name-specifier written on the cast TypeLoc.
  // The target SgType is shared semantic identity and cannot own per-use
  // spelling.
  CastExp.setDataPrototype("bool", "explicit_name_qualification_present",
                           "= false", NO_CONSTRUCTOR_PARAMETER,
                           BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  CastExp.setDataPrototype("bool", "explicit_global_qualification", "= false",
                           NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                           NO_TRAVERSAL, NO_DELETE);
  CastExp.setDataPrototype("SgStringList", "explicit_name_qualification_tokens",
                           "", NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                           NO_TRAVERSAL, NO_DELETE);
  CastExp.setDataPrototype("SgNonrealDecl::source_elaboration_kind_enum",
                           "source_type_elaboration_kind",
                           "= SgNonrealDecl::e_source_elaboration_unspecified",
                           NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                           NO_TRAVERSAL, NO_DELETE);

  CastExp.setDataPrototype(
      "SgDeclarationStatement*", "type_defining_declaration", "= NULL",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL,
      NO_DELETE, COPY_DATA, OPTIONAL_TRAVERSAL_MEMBER);

  CastExp.setDataPrototype(
      "int", "name_qualification_for_pointer_to_member_class_length", "= 0",
      NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  CastExp.setDataPrototype(
      "bool", "type_elaboration_for_pointer_to_member_class_required",
      "= false", NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);

  CastExp.setDataPrototype(
      "bool", "global_qualification_for_pointer_to_member_class_required",
      "= false", NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);

  NEW_TERMINAL_MACRO(Char16Val, "Char16Val", "CHAR16_VAL");
  Char16Val.setFunctionSource("SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
                              "../Grammar/Expression.code");
  Char16Val.setDataPrototype(
      "unsigned short", "value", "= 0", CONSTRUCTOR_PARAMETER,
      BUILD_LITERAL_VALUE_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  Char16Val.setDataPrototype("std::string", "valueString", "= \"\"",
                             CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                             NO_TRAVERSAL, NO_DELETE);
  Char16Val.setFunctionSource("SOURCE_GET_TYPE_VALUE",
                              "../Grammar/Expression.code");
  Char16Val.editSubstitute("GENERIC_TYPE", "SgTypeChar16");

  NEW_TERMINAL_MACRO(Char32Val, "Char32Val", "CHAR32_VAL");
  Char32Val.setFunctionSource("SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
                              "../Grammar/Expression.code");
  Char32Val.setDataPrototype(
      "unsigned int", "value", "= 0", CONSTRUCTOR_PARAMETER,
      BUILD_LITERAL_VALUE_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  Char32Val.setDataPrototype("std::string", "valueString", "= \"\"",
                             CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                             NO_TRAVERSAL, NO_DELETE);
  Char32Val.setFunctionSource("SOURCE_GET_TYPE_VALUE",
                              "../Grammar/Expression.code");
  Char32Val.editSubstitute("GENERIC_TYPE", "SgTypeChar32");

  NEW_TERMINAL_MACRO(CharVal, "CharVal", "CHAR_VAL");
  CharVal.setFunctionSource("SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
                            "../Grammar/Expression.code");
  CharVal.setDataPrototype("int64_t", "value", "= 0", CONSTRUCTOR_PARAMETER,
                           BUILD_LITERAL_VALUE_ACCESS_FUNCTIONS, NO_TRAVERSAL,
                           NO_DELETE);
  CharVal.setDataPrototype("std::string", "valueString", "= \"\"",
                           CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                           NO_TRAVERSAL, NO_DELETE);
  CharVal.setFunctionSource("SOURCE_GET_TYPE_VALUE",
                            "../Grammar/Expression.code");
  CharVal.editSubstitute("GENERIC_TYPE", "SgTypeChar");

  NEW_TERMINAL_MACRO(ChooseExpression, "ChooseExpression", "CHOOSE_EXPR");
  ChooseExpression.setFunctionPrototype("HEADER_CHOOSE_EXPRESSION",
                                        "../Grammar/Expression.code");
  ChooseExpression.setFunctionSource(
      "SOURCE_CHOOSE_EXPRESSION_POST_CONSTRUCTION_INITIALIZATION",
      "../Grammar/Expression.code");
  ChooseExpression.setDataPrototype(
      "SgExpression*", "condition", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);
  ChooseExpression.setDataPrototype(
      "SgExpression*", "true_expression", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);
  ChooseExpression.setDataPrototype(
      "SgExpression*", "false_expression", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);
  ChooseExpression.setDataPrototype(
      "SgType*", "expression_type", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  ChooseExpression.setFunctionSource("SOURCE_CHOOSE_EXPRESSION_GET_TYPE",
                                     "../Grammar/Expression.code");
  ChooseExpression.setDataPrototype(
      "SgChooseExpression::selection_kind_enum", "selection_kind",
      "= SgChooseExpression::e_choose_selection_unclassified",
      CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  NEW_TERMINAL_MACRO(ClassNameRefExp, "ClassNameRefExp", "CLASSNAME_REF");
  ClassNameRefExp.excludeFunctionPrototype("HEADER_GET_TYPE",
                                           "../Grammar/Expression.code");
  ClassNameRefExp.setFunctionSource(
      "SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
      "../Grammar/Expression.code");
  ClassNameRefExp.setDataPrototype(
      "SgClassSymbol*", "symbol", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  NEW_TERMINAL_MACRO(ColonShapeExp, "ColonShapeExp", "TEMP_ColonShapeExp");
  // Note that SgColonExp and SgAsteriskExp are used in array shape
  // specification, but not indexing, so they are not part of subscript
  // expressions.
  ColonShapeExp.editSubstitute("PRECEDENCE_VALUE", " 17");

  NEW_TERMINAL_MACRO(CommaOpExp, "CommaOpExp", "COMMA_OP");
  CommaOpExp.setFunctionSource("SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
                               "../Grammar/Expression.code");
  // The PRECEDENCE_VALUE of "6" was chosen so as not to have to change other
  // precedence values.
  CommaOpExp.editSubstitute("PRECEDENCE_VALUE", " 1"); // lowest precedence
  CommaOpExp.setFunctionPrototype("HEADER_COMMA_OPERATOR_EXPRESSION",
                                  "../Grammar/Expression.code");

  // Complex values own their components; a null real component denotes a
  // pure imaginary value. Source spelling belongs to the scalar components.
  NEW_TERMINAL_MACRO(ComplexVal, "ComplexVal", "COMPLEX_VAL");
  ComplexVal.setFunctionSource(
      "SOURCE_COMPLEX_VALUE_POST_CONSTRUCTION_INITIALIZATION",
      "../Grammar/Expression.code");
  // A Fortran complex literal can use a
  // named constant for either component, so the typed operands are arbitrary
  // expressions rather than only immediate value nodes. real_value can be
  // NULL for a pure imaginary value.
  ComplexVal.setDataPrototype("SgExpression*", "real_value", "= NULL",
                              CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                              DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
                              OPTIONAL_TRAVERSAL_MEMBER);
  ComplexVal.setDataPrototype("SgExpression*", "imaginary_value", "= NULL",
                              CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                              DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
                              OPTIONAL_TRAVERSAL_MEMBER);
  ComplexVal.setDataPrototype("SgType*", "precisionType", "= NULL",
                              CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                              NO_TRAVERSAL, NO_DELETE);
  ComplexVal.editSubstitute("GENERIC_TYPE", "SgTypeComplex");

  NEW_TERMINAL_MACRO(CompoundLiteralExp, "CompoundLiteralExp",
                     "COMPOUND_LITERAL");
  CompoundLiteralExp.setFunctionSource(
      "SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
      "../Grammar/Expression.code");
  CompoundLiteralExp.setFunctionPrototype("HEADER_COMPOUND_LITERAL_EXPRESSION",
                                          "../Grammar/Expression.code");

  // DQ (9/4/2013): This better matches the concept of unnamed variable and
  // follows the design of a variable reference. Note that a SgVariableSymbol is
  // used which means that the declaration will be a SgInitializedName which
  // will contain a internally generated name used as a key to add the
  // SgVariableSymbol to the symbol table.
  CompoundLiteralExp.setDataPrototype("SgVariableSymbol*", "symbol", "= NULL",
                                      CONSTRUCTOR_PARAMETER,
                                      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
                                      NO_DELETE, COPY_CONSTRUCTOR_REFERENCE);

  // DQ (9/4/2013): This follows the design of CompoundLiteralExp to be similar
  // to a variable reference.
  CompoundLiteralExp.setFunctionSource("SOURCE_GET_TYPE_FROM_SYMBOL",
                                       "../Grammar/Expression.code");

  NEW_TERMINAL_MACRO(ConcatenationOp, "ConcatenationOp", "CONCATENATION_OP");
  ConcatenationOp.setFunctionSource(
      "SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
      "../Grammar/Expression.code");
  ConcatenationOp.editSubstitute("PRECEDENCE_VALUE", " 3");

  NEW_TERMINAL_MACRO(ConjugateOp, "ConjugateOp", "CONJUGATE_OP");
  ConjugateOp.setFunctionSource("SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
                                "../Grammar/Expression.code");
  ConjugateOp.editSubstitute("PRECEDENCE_VALUE", "15");

  NEW_TERMINAL_MACRO(ConditionalExp, "ConditionalExp", "EXPR_CONDITIONAL");
  ConditionalExp.setFunctionPrototype("HEADER_GET_NEXT_EXPRESSION",
                                      "../Grammar/Expression.code");
  ConditionalExp.setFunctionPrototype("HEADER_CONDITIONAL_EXPRESSION",
                                      "../Grammar/Expression.code");
  ConditionalExp.setDataPrototype("SgExpression*", "conditional_exp", "",
                                  CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                                  DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
                                  REQUIRED_TRAVERSAL_MEMBER);
  ConditionalExp.setDataPrototype("SgExpression*", "true_exp", "",
                                  CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                                  DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
                                  OPTIONAL_TRAVERSAL_MEMBER);
  ConditionalExp.setDataPrototype("SgExpression*", "false_exp", "",
                                  CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                                  DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
                                  REQUIRED_TRAVERSAL_MEMBER);
  ConditionalExp.setDataPrototype("SgType*", "expression_type", "",
                                  CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                                  NO_TRAVERSAL, NO_DELETE);
  ConditionalExp.setDataPrototype(
      "SgConditionalExp::conditional_operator_kind_enum", "operator_kind", "",
      CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE,
      COPY_DATA);

  NEW_TERMINAL_MACRO(ConstructorInitializer, "ConstructorInitializer",
                     "CONSTRUCTOR_INIT");
  ConstructorInitializer.setFunctionPrototype(
      "HEADER_CONSTRUCTOR_INITIALIZER_EXPRESSION",
      "../Grammar/Expression.code");
  ConstructorInitializer.setDataPrototype(
      "SgMemberFunctionDeclaration*", "declaration", "= NULL",
      CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE,
      COPY_CONSTRUCTOR_REFERENCE);
  ConstructorInitializer.setDataPrototype(
      "SgExprListExp*", "args", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER, DIRECT_TRAVERSAL_ACCESS,
      COPY_BEFORE_CONSTRUCTOR_DEPENDENCIES);
  ConstructorInitializer.setDataPrototype(
      "SgType*", "expression_type", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  ConstructorInitializer.setDataPrototype(
      "bool", "need_name", "= false", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  ConstructorInitializer.setDataPrototype(
      "bool", "need_qualifier", "= false", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  ConstructorInitializer.setDataPrototype(
      "bool", "need_parenthesis_after_name", "= false", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  ConstructorInitializer.setDataPrototype(
      "bool", "associated_class_unknown", "= false", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  // Inherited calls forward the enclosing constructor's incoming arguments,
  // including unnamed variadic arguments, without reevaluating expressions.
  ConstructorInitializer.setDataPrototype(
      "SgConstructorInitializer::argument_source_enum", "argument_source",
      "= SgConstructorInitializer::e_constructor_argument_expressions",
      CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  ConstructorInitializer.setDataPrototype(
      "bool", "is_elidable", "= false", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  ConstructorInitializer.setDataPrototype(
      "bool", "elision_permitted", "= false", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  // Qualification accessors do not mark the node modified.
  ConstructorInitializer.setDataPrototype(
      "int", "name_qualification_length", "= 0", NO_CONSTRUCTOR_PARAMETER,
      NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  ConstructorInitializer.setDataPrototype(
      "bool", "type_elaboration_required", "= false", NO_CONSTRUCTOR_PARAMETER,
      NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  ConstructorInitializer.setDataPrototype(
      "bool", "global_qualification_required", "= false",
      NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  // A named constructor expression owns the exact TypeLoc spelling at its use
  // site. The expression type is semantic identity and can be shared across
  // differently qualified uses, so it cannot carry this source payload.
  ConstructorInitializer.setDataPrototype(
      "bool", "explicit_name_qualification_present", "= false",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);
  ConstructorInitializer.setDataPrototype(
      "bool", "explicit_global_qualification", "= false",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);
  ConstructorInitializer.setDataPrototype(
      "SgStringList", "explicit_name_qualification_tokens", "",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);
  ConstructorInitializer.setDataPrototype(
      "SgNonrealDecl::source_elaboration_kind_enum",
      "source_type_elaboration_kind",
      "= SgNonrealDecl::e_source_elaboration_unspecified",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);

  ConstructorInitializer.setDataPrototype(
      "bool", "is_used_in_conditional", "= false", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  ConstructorInitializer.setFunctionSource("SOURCE_EXACT_EXPRESSION_GET_TYPE",
                                           "../Grammar/Expression.code");

  NEW_TERMINAL_MACRO(CudaKernelCallExp, "CudaKernelCallExp", "KERN_CALL");
  CudaKernelCallExp.editSubstitute("PRECEDENCE_VALUE", " 0");
  CudaKernelCallExp.editSubstitute("HEADER_LIST_DECLARATIONS",
                                   "HEADER_LIST_FUNCTIONS",
                                   "../Grammar/Expression.code");
  CudaKernelCallExp.setDataPrototype(
      "SgCudaKernelExecConfig*", "exec_config", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);

  NEW_TERMINAL_MACRO(CudaKernelExecConfig, "CudaKernelExecConfig", "EXEC_CONF");
  CudaKernelExecConfig.editSubstitute("PRECEDENCE_VALUE", " 0");
  CudaKernelExecConfig.setDataPrototype(
      "SgExpression*", "grid", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);
  CudaKernelExecConfig.setDataPrototype(
      "SgExpression*", "blocks", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);
  CudaKernelExecConfig.setDataPrototype(
      "SgExpression*", "shared", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);
  CudaKernelExecConfig.setDataPrototype(
      "SgExpression*", "stream", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);
  CudaKernelExecConfig.setFunctionSource("SOURCE_SYNTAX_ONLY_GET_TYPE",
                                         "../Grammar/Expression.code");

  NEW_TERMINAL_MACRO(DeleteExp, "DeleteExp", "DELETE_OP");
  DeleteExp.setFunctionPrototype("HEADER_GET_NEXT_EXPRESSION",
                                 "../Grammar/Expression.code");
  DeleteExp.setFunctionPrototype("HEADER_DELETE_OPERATOR_EXPRESSION",
                                 "../Grammar/Expression.code");
  DeleteExp.setDataPrototype("SgExpression*", "variable", "= NULL",
                             CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                             DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
                             OPTIONAL_TRAVERSAL_MEMBER);
  DeleteExp.setDataPrototype("short", "is_array", "= 0", CONSTRUCTOR_PARAMETER,
                             BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  DeleteExp.setDataPrototype("short", "need_global_specifier", "= 0",
                             CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                             NO_TRAVERSAL, NO_DELETE);
  DeleteExp.setDataPrototype(
      "SgFunctionDeclaration*", "deleteOperatorDeclaration", "= NULL",
      CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE,
      COPY_CONSTRUCTOR_REFERENCE);

  // An expression with a designator, used for designated initialization in
  // SgAggregateInitializer
  NEW_TERMINAL_MACRO(DesignatedInitializer, "DesignatedInitializer",
                     "DESIGNATED_INITIALIZER");
  DesignatedInitializer.setFunctionPrototype("HEADER_DESIGNATED_INITIALIZER",
                                             "../Grammar/Expression.code");

  DesignatedInitializer.setDataPrototype(
      "SgExprListExp*", "designatorList", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);
  DesignatedInitializer.setDataPrototype(
      "SgInitializer*", "memberInit", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);

  // One source designator in a designated initializer.  Its explicit kind
  // distinguishes `.field`, `[index]`, and `[lower ... upper]` without asking
  // the backend to infer syntax from the payload expression's dynamic type.
  NEW_TERMINAL_MACRO(Designator, "Designator", "DESIGNATOR");
  Designator.setFunctionSource("SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
                               "../Grammar/Expression.code");
  Designator.setFunctionPrototype("HEADER_DESIGNATOR",
                                  "../Grammar/Expression.code");
  Designator.setDataPrototype("SgDesignator::designator_kind_enum", "kind",
                              "= SgDesignator::e_designator_unclassified",
                              CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                              NO_TRAVERSAL, NO_DELETE);
  Designator.setDataPrototype("SgExpression*", "first_expression", "= NULL",
                              CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                              DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
                              OPTIONAL_TRAVERSAL_MEMBER);
  Designator.setDataPrototype("SgExpression*", "second_expression", "= NULL",
                              CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                              DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
                              OPTIONAL_TRAVERSAL_MEMBER);

  NEW_TERMINAL_MACRO(DivAssignOp, "DivAssignOp", "DIV_ASSIGN_OP");
  DivAssignOp.setFunctionSource("SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
                                "../Grammar/Expression.code");
  DivAssignOp.editSubstitute("PRECEDENCE_VALUE", " 2");

  NEW_TERMINAL_MACRO(DivideOp, "DivideOp", "DIV_OP");
  DivideOp.setFunctionSource("SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
                             "../Grammar/Expression.code");
  DivideOp.editSubstitute("PRECEDENCE_VALUE", "13");

  NEW_TERMINAL_MACRO(DotExp, "DotExp", "RECORD_REF");
  DotExp.setFunctionSource("SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
                           "../Grammar/Expression.code");
  DotExp.editSubstitute("PRECEDENCE_VALUE", "16");
  DotExp.setFunctionPrototype("HEADER_DOT_EXPRESSION",
                              "../Grammar/Expression.code");

  NEW_TERMINAL_MACRO(DotStarOp, "DotStarOp", "DOTSTAR_OP");
  DotStarOp.setFunctionSource("SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
                              "../Grammar/Expression.code");
  DotStarOp.editSubstitute("PRECEDENCE_VALUE", "14");
  DotStarOp.setFunctionPrototype("HEADER_DOT_STAR_OPERATOR",
                                 "../Grammar/Expression.code");

  NEW_TERMINAL_MACRO(DoubleVal, "DoubleVal", "DOUBLE_VAL");
  DoubleVal.setFunctionSource("SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
                              "../Grammar/Expression.code");
  DoubleVal.setDataPrototype("double", "value", "= 0.0", CONSTRUCTOR_PARAMETER,
                             BUILD_LITERAL_VALUE_ACCESS_FUNCTIONS, NO_TRAVERSAL,
                             NO_DELETE);
  DoubleVal.setDataPrototype("std::string", "valueString", "= \"\"",
                             CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                             NO_TRAVERSAL, NO_DELETE);
  DoubleVal.setFunctionSource("SOURCE_GET_TYPE_VALUE",
                              "../Grammar/Expression.code");
  DoubleVal.editSubstitute("GENERIC_TYPE", "SgTypeDouble");

  NEW_TERMINAL_MACRO(EnumVal, "EnumVal", "ENUM_VAL");
  EnumVal.setFunctionSource("SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
                            "../Grammar/Expression.code");
  EnumVal.setFunctionPrototype("HEADER_ENUM_VALUE_EXPRESSION",
                               "../Grammar/Expression.code");

  EnumVal.setDataPrototype("long long int", "value", "= 0",
                           CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                           NO_TRAVERSAL, NO_DELETE);
  EnumVal.setDataPrototype("SgEnumDeclaration*", "declaration", "= NULL",
                           CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                           NO_TRAVERSAL, NO_DELETE);
  // We do not traverse the following data member for the moment!
  EnumVal.setDataPrototype("SgName", "name", "= \"\"", CONSTRUCTOR_PARAMETER,
                           BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  // DQ (12/22/2006): Record if name qualification is required where used as a
  // constant. See test2003_01.C for an example of where this is required.
  EnumVal.setDataPrototype("bool", "requiresNameQualification", "= false",
                           NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                           NO_TRAVERSAL, NO_DELETE);

  EnumVal.setDataPrototype("int", "name_qualification_length", "= 0",
                           NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                           NO_TRAVERSAL, NO_DELETE);

  EnumVal.setDataPrototype("bool", "type_elaboration_required", "= false",
                           NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                           NO_TRAVERSAL, NO_DELETE);

  EnumVal.setDataPrototype("bool", "global_qualification_required", "= false",
                           NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                           NO_TRAVERSAL, NO_DELETE);
  EnumVal.setDataPrototype("int", "explicit_name_qualification_length", "= -1",
                           NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                           NO_TRAVERSAL, NO_DELETE);
  EnumVal.setDataPrototype("bool", "explicit_global_qualification", "= false",
                           NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                           NO_TRAVERSAL, NO_DELETE);
  EnumVal.setDataPrototype("SgStringList", "explicit_name_qualification_tokens",
                           "", NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                           NO_TRAVERSAL, NO_DELETE);
  NEW_TERMINAL_MACRO(EqualityOp, "EqualityOp", "EQ_OP");
  EqualityOp.setFunctionSource("SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
                               "../Grammar/Expression.code");
  EqualityOp.editSubstitute("PRECEDENCE_VALUE", " 9");
  EqualityOp.setFunctionPrototype("HEADER_EQUALITY_OPERATOR",
                                  "../Grammar/Expression.code");
  // DQ (6/20/2006): Relational Operators must return bool type
  EqualityOp.editSubstitute("HEADER_BOOLEAN_GET_TYPE_MEMBER_FUNCTION",
                            "HEADER_BOOLEAN_GET_TYPE",
                            "../Grammar/Expression.code");
  EqualityOp.setFunctionSource("SOURCE_EQUALITY_OPERATOR_EXPRESSION",
                               "../Grammar/Expression.code");
  // DQ (6/20/2006): Relational Operators must return bool type
  EqualityOp.editSubstitute("SOURCE_BOOLEAN_GET_TYPE_MEMBER_FUNCTION",
                            "SOURCE_BOOLEAN_GET_TYPE",
                            "../Grammar/Expression.code");

  NEW_TERMINAL_MACRO(ExponentiationAssignOp, "ExponentiationAssignOp",
                     "EXP_ASSIGN_OP");
  ExponentiationAssignOp.setFunctionSource(
      "SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
      "../Grammar/Expression.code");
  ExponentiationAssignOp.editSubstitute("PRECEDENCE_VALUE", " 2");

  // Binary operator
  NEW_TERMINAL_MACRO(ExponentiationOp, "ExponentiationOp", "EXPONENTIATION_OP");
  ExponentiationOp.editSubstitute("PRECEDENCE_VALUE", " 2");

  NEW_TERMINAL_MACRO(ExpressionRoot, "ExpressionRoot", "EXPRESSION_ROOT");
  ExpressionRoot.setFunctionSource(
      "SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
      "../Grammar/Expression.code");
  ExpressionRoot.excludeFunctionPrototype("HEADER_PRECEDENCE",
                                          "../Grammar/Expression.code");

  NEW_TERMINAL_MACRO(Float128Val, "Float128Val", "FLOAT_128_VAL");
  Float128Val.setFunctionSource(
      "SOURCE_EXTENDED_FLOAT_POST_CONSTRUCTION_INITIALIZATION",
      "../Grammar/Expression.code");
  Float128Val.setDataPrototype("Rose::FloatingValue", "value",
                               "= Rose::FloatingValue()", CONSTRUCTOR_PARAMETER,
                               NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  Float128Val.setDataPrototype("std::string", "valueString", "= \"\"",
                               CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                               NO_TRAVERSAL, NO_DELETE);
  Float128Val.setFunctionSource("SOURCE_GET_TYPE_VALUE",
                                "../Grammar/Expression.code");
  Float128Val.editSubstitute("GENERIC_TYPE", "SgTypeFloat128");

  Float128Val.setFunctionPrototype("HEADER_EXTENDED_FLOAT_LITERAL",
                                   "../Grammar/Expression.code");
  Float128Val.setFunctionSource("SOURCE_EXTENDED_FLOAT_LITERAL",
                                "../Grammar/Expression.code");
  Float128Val.editSubstitute(
      "NORMALIZE_FLOATING_FORMAT",
      "p_value = p_value.converted(Rose::FloatingValue::Format::binary128);");

  NEW_TERMINAL_MACRO(Float80Val, "Float80Val", "FLOAT_80_VAL");
  Float80Val.setFunctionSource(
      "SOURCE_EXTENDED_FLOAT_POST_CONSTRUCTION_INITIALIZATION",
      "../Grammar/Expression.code");
  Float80Val.setDataPrototype("Rose::FloatingValue", "value",
                              "= Rose::FloatingValue()", CONSTRUCTOR_PARAMETER,
                              NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  Float80Val.setDataPrototype("std::string", "valueString", "= \"\"",
                              CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                              NO_TRAVERSAL, NO_DELETE);
  Float80Val.setFunctionSource("SOURCE_GET_TYPE_VALUE",
                               "../Grammar/Expression.code");
  Float80Val.editSubstitute("GENERIC_TYPE", "SgTypeFloat80");

  Float80Val.setFunctionPrototype("HEADER_EXTENDED_FLOAT_LITERAL",
                                  "../Grammar/Expression.code");
  Float80Val.setFunctionSource("SOURCE_EXTENDED_FLOAT_LITERAL",
                               "../Grammar/Expression.code");
  Float80Val.editSubstitute(
      "NORMALIZE_FLOATING_FORMAT",
      "p_value = "
      "p_value.converted(Rose::FloatingValue::Format::x87_binary80);");

  NEW_TERMINAL_MACRO(BFloat16Val, "BFloat16Val", "BFLOAT_16_VAL");
  BFloat16Val.setFunctionSource("SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
                                "../Grammar/Expression.code");
  BFloat16Val.setDataPrototype("float", "value", "= 0", CONSTRUCTOR_PARAMETER,
                               BUILD_LITERAL_VALUE_ACCESS_FUNCTIONS,
                               NO_TRAVERSAL, NO_DELETE);
  BFloat16Val.setDataPrototype("std::string", "valueString", "= \"\"",
                               CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                               NO_TRAVERSAL, NO_DELETE);
  BFloat16Val.setFunctionSource("SOURCE_GET_TYPE_VALUE",
                                "../Grammar/Expression.code");
  BFloat16Val.editSubstitute("GENERIC_TYPE", "SgTypeBFloat16");

  NEW_TERMINAL_MACRO(Float16Val, "Float16Val", "FLOAT_16_VAL");
  Float16Val.setFunctionSource("SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
                               "../Grammar/Expression.code");
  Float16Val.setDataPrototype("float", "value", "= 0", CONSTRUCTOR_PARAMETER,
                              BUILD_LITERAL_VALUE_ACCESS_FUNCTIONS,
                              NO_TRAVERSAL, NO_DELETE);
  Float16Val.setDataPrototype("std::string", "valueString", "= \"\"",
                              CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                              NO_TRAVERSAL, NO_DELETE);
  Float16Val.setFunctionSource("SOURCE_GET_TYPE_VALUE",
                               "../Grammar/Expression.code");
  Float16Val.editSubstitute("GENERIC_TYPE", "SgTypeFloat16");

  NEW_TERMINAL_MACRO(Float32Val, "Float32Val", "FLOAT_32_VAL");
  Float32Val.setFunctionSource("SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
                               "../Grammar/Expression.code");
  Float32Val.setDataPrototype("float", "value", "= 0", CONSTRUCTOR_PARAMETER,
                              BUILD_LITERAL_VALUE_ACCESS_FUNCTIONS,
                              NO_TRAVERSAL, NO_DELETE);
  Float32Val.setDataPrototype("std::string", "valueString", "= \"\"",
                              CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                              NO_TRAVERSAL, NO_DELETE);
  Float32Val.setFunctionSource("SOURCE_GET_TYPE_VALUE",
                               "../Grammar/Expression.code");
  Float32Val.editSubstitute("GENERIC_TYPE", "SgTypeFloat32");

  NEW_TERMINAL_MACRO(Float64Val, "Float64Val", "FLOAT_64_VAL");
  Float64Val.setFunctionSource("SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
                               "../Grammar/Expression.code");
  Float64Val.setDataPrototype("double", "value", "= 0", CONSTRUCTOR_PARAMETER,
                              BUILD_LITERAL_VALUE_ACCESS_FUNCTIONS,
                              NO_TRAVERSAL, NO_DELETE);
  Float64Val.setDataPrototype("std::string", "valueString", "= \"\"",
                              CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                              NO_TRAVERSAL, NO_DELETE);
  Float64Val.setFunctionSource("SOURCE_GET_TYPE_VALUE",
                               "../Grammar/Expression.code");
  Float64Val.editSubstitute("GENERIC_TYPE", "SgTypeFloat64");

  NEW_TERMINAL_MACRO(FloatVal, "FloatVal", "FLOAT_VAL");
  FloatVal.setFunctionSource("SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
                             "../Grammar/Expression.code");
  FloatVal.setDataPrototype("float", "value", "= 0.0", CONSTRUCTOR_PARAMETER,
                            BUILD_LITERAL_VALUE_ACCESS_FUNCTIONS, NO_TRAVERSAL,
                            NO_DELETE);
  FloatVal.setDataPrototype("std::string", "valueString", "= \"\"",
                            CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                            NO_TRAVERSAL, NO_DELETE);
  FloatVal.setFunctionSource("SOURCE_GET_TYPE_VALUE",
                             "../Grammar/Expression.code");
  FloatVal.editSubstitute("GENERIC_TYPE", "SgTypeFloat");

  NEW_TERMINAL_MACRO(FoldExpression, "FoldExpression", "FOLD_EXPR");
  FoldExpression.setFunctionPrototype("HEADER_FOLD_EXPRESSION",
                                      "../Grammar/Expression.code");
  FoldExpression.setFunctionSource(
      "SOURCE_FOLD_EXPRESSION_POST_CONSTRUCTION_INITIALIZATION",
      "../Grammar/Expression.code");
  FoldExpression.setDataPrototype("SgExpression*", "operands", "= NULL",
                                  CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                                  DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
                                  OPTIONAL_TRAVERSAL_MEMBER);
  FoldExpression.setDataPrototype(
      "SgFoldExpression::operator_kind_enum", "operator_kind",
      "= SgFoldExpression::e_fold_operator_unspecified", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  FoldExpression.setDataPrototype("bool", "is_left_associative", "= false",
                                  CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                                  NO_TRAVERSAL, NO_DELETE);
  // Fold operands do not determine the final type after operator resolution,
  // promotions, and value-category conversion.
  FoldExpression.setDataPrototype("SgType*", "expression_type", "= NULL",
                                  CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                                  NO_TRAVERSAL, NO_DELETE);

  NEW_TERMINAL_MACRO(PackExpansionExpr, "PackExpansionExpr",
                     "PACK_EXPANSION_EXPR");
  PackExpansionExpr.setFunctionSource(
      "SOURCE_PACK_EXPANSION_POST_CONSTRUCTION_INITIALIZATION",
      "../Grammar/Expression.code");
  PackExpansionExpr.setDataPrototype(
      "SgExpression*", "pattern_expression", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);
  // Expansion type is a Clang semantic property.  It is not reconstructed
  // from a pattern that may be dependent or transformed later.
  PackExpansionExpr.setDataPrototype(
      "SgType*", "expression_type", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  NEW_TERMINAL_MACRO(FunctionParameterRefExp, "FunctionParameterRefExp",
                     "FUNCTION_PARAMETER_REF_EXP");
  FunctionParameterRefExp.setFunctionSource(
      "SOURCE_FUNCTION_PARAMETER_REF_POST_CONSTRUCTION_INITIALIZATION",
      "../Grammar/Expression.code");
  FunctionParameterRefExp.editSubstitute("PRECEDENCE_VALUE", "16");
  FunctionParameterRefExp.setDataPrototype(
      "int", "parameter_number", "", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  FunctionParameterRefExp.setDataPrototype(
      "int", "parameter_levels_up", "", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  FunctionParameterRefExp.setDataPrototype(
      "SgType*", "parameter_type", "", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  NEW_TERMINAL_MACRO(FunctionRefExp, "FunctionRefExp", "FUNCTION_REF");
  FunctionRefExp.setFunctionPrototype("HEADER_FUNCTION_REF_EXPRESSION",
                                      "../Grammar/Expression.code");
  FunctionRefExp.setDataPrototype("SgFunctionSymbol*", "symbol_i", "",
                                  CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                                  NO_TRAVERSAL, NO_DELETE);
  FunctionRefExp.setDataPrototype("SgType*", "function_type", "",
                                  CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                                  NO_TRAVERSAL, NO_DELETE, COPY_REFERENCE);
  // In pre-C23 C, a call can introduce an implicit function declaration
  // before the entity acquires a later lexical declaration.  The shared
  // function symbol is deliberately re-rooted when that source declaration is
  // published, so retain the exact implicit declaration selected at this use
  // site as a separate temporal binding edge.
  FunctionRefExp.setDataPrototype(
      "SgFunctionDeclaration*", "c_implicit_declaration", "= NULL",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE,
      COPY_REFERENCE);
  // Fortran semantic resolution and source visibility are distinct.  The
  // semantic symbol above determines the exact procedure identity;
  // this optional edge records the exact symbol whose lexical binding supplies
  // the source-spelled unqualified name at the use site.
  FunctionRefExp.setDataPrototype(
      "SgFunctionSymbol*", "fortran_source_visible_symbol", "= NULL",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);
  FunctionRefExp.setDataPrototype(
      "SgFunctionRefExp::fortran_source_visible_binding_kind_enum",
      "fortran_source_visible_binding_kind",
      "= SgFunctionRefExp::e_fortran_source_visible_binding_not_applicable",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);

  FunctionRefExp.setDataPrototype("int", "name_qualification_length", "= 0",
                                  NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                                  NO_TRAVERSAL, NO_DELETE);

  FunctionRefExp.setDataPrototype("bool", "type_elaboration_required",
                                  "= false", NO_CONSTRUCTOR_PARAMETER,
                                  NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  FunctionRefExp.setDataPrototype("bool", "global_qualification_required",
                                  "= false", NO_CONSTRUCTOR_PARAMETER,
                                  NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  FunctionRefExp.setDataPrototype("int", "explicit_name_qualification_length",
                                  "= -1", NO_CONSTRUCTOR_PARAMETER,
                                  NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  FunctionRefExp.setDataPrototype("bool", "explicit_global_qualification",
                                  "= false", NO_CONSTRUCTOR_PARAMETER,
                                  NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  FunctionRefExp.setDataPrototype(
      "SgStringList", "explicit_name_qualification_tokens", "",
      NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  NEW_TERMINAL_MACRO(GreaterOrEqualOp, "GreaterOrEqualOp", "GE_OP");
  GreaterOrEqualOp.setFunctionSource(
      "SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
      "../Grammar/Expression.code");
  GreaterOrEqualOp.editSubstitute("PRECEDENCE_VALUE", "10");
  GreaterOrEqualOp.setFunctionPrototype("HEADER_GREATER_OR_EQUAL_OPERATOR",
                                        "../Grammar/Expression.code");
  GreaterOrEqualOp.editSubstitute("HEADER_BOOLEAN_GET_TYPE_MEMBER_FUNCTION",
                                  "HEADER_BOOLEAN_GET_TYPE",
                                  "../Grammar/Expression.code");
  GreaterOrEqualOp.setFunctionSource(
      "SOURCE_GREATER_OR_EQUAL_OPERATOR_EXPRESSION",
      "../Grammar/Expression.code");
  GreaterOrEqualOp.editSubstitute("SOURCE_BOOLEAN_GET_TYPE_MEMBER_FUNCTION",
                                  "SOURCE_BOOLEAN_GET_TYPE",
                                  "../Grammar/Expression.code");

  NEW_TERMINAL_MACRO(GreaterThanOp, "GreaterThanOp", "GT_OP");
  GreaterThanOp.setFunctionSource(
      "SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
      "../Grammar/Expression.code");
  GreaterThanOp.editSubstitute("PRECEDENCE_VALUE", "10");
  GreaterThanOp.setFunctionPrototype("HEADER_GREATER_THAN_OPERATOR",
                                     "../Grammar/Expression.code");
  GreaterThanOp.editSubstitute("HEADER_BOOLEAN_GET_TYPE_MEMBER_FUNCTION",
                               "HEADER_BOOLEAN_GET_TYPE",
                               "../Grammar/Expression.code");
  GreaterThanOp.setFunctionSource("SOURCE_GREATER_THAN_OPERATOR_EXPRESSION",
                                  "../Grammar/Expression.code");
  GreaterThanOp.editSubstitute("SOURCE_BOOLEAN_GET_TYPE_MEMBER_FUNCTION",
                               "SOURCE_BOOLEAN_GET_TYPE",
                               "../Grammar/Expression.code");

  NEW_TERMINAL_MACRO(ImagPartOp, "ImagPartOp", "IMAG_PART_OP");
  ImagPartOp.setFunctionSource("SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
                               "../Grammar/Expression.code");
  ImagPartOp.editSubstitute("PRECEDENCE_VALUE", "15");

  NEW_TERMINAL_MACRO(ImpliedDo, "ImpliedDo", "IMPLIED_DO");
  ImpliedDo.editSubstitute("PRECEDENCE_VALUE", " 2");
  ImpliedDo.setFunctionPrototype("HEADER_IMPLIED_DO",
                                 "../Grammar/Expression.code");
  // The loop initialization can contain an expression in the index, as in
  // "(product(localCount(:j)), j = 1, numDims - 1)" in test2010_49.f90.
  ImpliedDo.setDataPrototype("SgExpression*", "do_var_initialization", "= NULL",
                             CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                             DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
                             OPTIONAL_TRAVERSAL_MEMBER);
  ImpliedDo.setDataPrototype("SgExpression*", "last_val", "= NULL",
                             CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                             DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
                             OPTIONAL_TRAVERSAL_MEMBER);
  ImpliedDo.setDataPrototype("SgExpression*", "increment", "= NULL",
                             CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                             DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
                             OPTIONAL_TRAVERSAL_MEMBER);
  // For "((A(i),B(i,j),i=0,10,2),j=0,20,3)" A(i) and B(i,j) are the objects in
  // the object_list for the inner most implided do loop.  The inner most
  // implied do loop is in the object list for the outer implied do loop.
  ImpliedDo.setDataPrototype("SgExprListExp*", "object_list", "= NULL",
                             CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                             DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
                             OPTIONAL_TRAVERSAL_MEMBER);
  // DQ (10/2/2010): Added scope to hold the SgVariableDeclaration and support
  // nested scopes of multi-dimensional implied do loops. However this scope
  // should not be traversed since its purpose is to hold a symbol table, but we
  // don't want just have a symbol table directly.
  ImpliedDo.setDataPrototype("SgScopeStatement*", "implied_do_scope", "= NULL",
                             CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                             NO_TRAVERSAL, NO_DELETE);

  NEW_TERMINAL_MACRO(IntegerDivideAssignOp, "IntegerDivideAssignOp",
                     "IDIV_ASSIGN_OP");
  IntegerDivideAssignOp.setFunctionSource(
      "SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
      "../Grammar/Expression.code");
  IntegerDivideAssignOp.editSubstitute("PRECEDENCE_VALUE", " 2");

  NEW_TERMINAL_MACRO(IntegerDivideOp, "IntegerDivideOp", "INTEGER_DIV_OP");
  IntegerDivideOp.setFunctionSource(
      "SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
      "../Grammar/Expression.code");
  IntegerDivideOp.editSubstitute("PRECEDENCE_VALUE", "13");

  NEW_TERMINAL_MACRO(IntVal, "IntVal", "INT_VAL");
  IntVal.setFunctionSource("SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
                           "../Grammar/Expression.code");
  IntVal.setDataPrototype("int", "value", "= 0", CONSTRUCTOR_PARAMETER,
                          BUILD_LITERAL_VALUE_ACCESS_FUNCTIONS, NO_TRAVERSAL,
                          NO_DELETE);
  IntVal.setDataPrototype("std::string", "valueString", "= \"\"",
                          CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                          NO_TRAVERSAL, NO_DELETE);
  IntVal.setFunctionSource("SOURCE_GET_TYPE_VALUE",
                           "../Grammar/Expression.code");
  IntVal.editSubstitute("GENERIC_TYPE", "SgTypeInt");

  NEW_TERMINAL_MACRO(IorAssignOp, "IorAssignOp", "IOR_ASSIGN_OP");
  IorAssignOp.setFunctionSource("SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
                                "../Grammar/Expression.code");
  IorAssignOp.editSubstitute("PRECEDENCE_VALUE", " 2");

  NEW_TERMINAL_MACRO(LabelRefExp, "LabelRefExp", "LABEL_REF");
  LabelRefExp.setFunctionSource("SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
                                "../Grammar/Expression.code");
  LabelRefExp.setFunctionPrototype("HEADER_LABEL_REF_EXPRESSION",
                                   "../Grammar/Expression.code");
  LabelRefExp.setDataPrototype("SgLabelSymbol*", "symbol", "= NULL",
                               CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                               NO_TRAVERSAL, NO_DELETE);
  LabelRefExp.setFunctionSource("SOURCE_GET_TYPE_FROM_SYMBOL",
                                "../Grammar/Expression.code");

  NEW_TERMINAL_MACRO(LambdaExp, "LambdaExp", "LAMBDA_EXP");
  LambdaExp.setFunctionSource(
      "SOURCE_LAMBDA_EXPRESSION_POST_CONSTRUCTION_INITIALIZATION",
      "../Grammar/Expression.code");
  LambdaExp.editSubstitute("PRECEDENCE_VALUE", "16");
  LambdaExp.setDataPrototype("SgLambdaCaptureList*", "lambda_capture_list", "",
                             CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                             DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
                             OPTIONAL_TRAVERSAL_MEMBER);

  LambdaExp.setDataPrototype("SgClassDeclaration*", "lambda_closure_class", "",
                             CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                             NO_TRAVERSAL, NO_DELETE, COPY_REFERENCE);
  LambdaExp.setDataPrototype("SgFunctionDeclaration*", "lambda_function", "",
                             CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                             NO_TRAVERSAL, NO_DELETE, COPY_REFERENCE);
  LambdaExp.setDataPrototype(
      "SgDeclarationScope*", "closure_scope", "= nullptr",
      CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE,
      COPY_DATA, OPTIONAL_TRAVERSAL_MEMBER, DIRECT_TRAVERSAL_ACCESS,
      COPY_BEFORE_CONSTRUCTOR_DEPENDENCIES);
  LambdaExp.setFunctionPrototype("HEADER_LAMBDA_EXPRESSION",
                                 "../Grammar/Expression.code");
  LambdaExp.setDataPrototype("bool", "is_mutable", "= false",
                             NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                             NO_TRAVERSAL, NO_DELETE);
  LambdaExp.setDataPrototype("bool", "capture_default", "= false",
                             NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                             NO_TRAVERSAL, NO_DELETE);
  LambdaExp.setDataPrototype("bool", "default_is_by_reference", "= false",
                             NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                             NO_TRAVERSAL, NO_DELETE);
  LambdaExp.setDataPrototype("bool", "explicit_return_type", "= false",
                             NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                             NO_TRAVERSAL, NO_DELETE);
  LambdaExp.setDataPrototype("bool", "has_parameter_decl", "= false",
                             NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                             NO_TRAVERSAL, NO_DELETE);

  LambdaExp.setDataPrototype("bool", "is_device", "= false",
                             NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                             NO_TRAVERSAL, NO_DELETE);

  NEW_TERMINAL_MACRO(LessOrEqualOp, "LessOrEqualOp", "LE_OP");
  LessOrEqualOp.setFunctionSource(
      "SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
      "../Grammar/Expression.code");
  LessOrEqualOp.editSubstitute("PRECEDENCE_VALUE", "10");
  LessOrEqualOp.setFunctionPrototype("HEADER_LESS_OR_EQUAL_OPERATOR",
                                     "../Grammar/Expression.code");
  LessOrEqualOp.editSubstitute("HEADER_BOOLEAN_GET_TYPE_MEMBER_FUNCTION",
                               "HEADER_BOOLEAN_GET_TYPE",
                               "../Grammar/Expression.code");
  LessOrEqualOp.setFunctionSource("SOURCE_LESS_OR_EQUAL_OPERATOR_EXPRESSION",
                                  "../Grammar/Expression.code");
  LessOrEqualOp.editSubstitute("SOURCE_BOOLEAN_GET_TYPE_MEMBER_FUNCTION",
                               "SOURCE_BOOLEAN_GET_TYPE",
                               "../Grammar/Expression.code");

  NEW_TERMINAL_MACRO(LessThanOp, "LessThanOp", "LT_OP");
  LessThanOp.setFunctionSource("SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
                               "../Grammar/Expression.code");
  LessThanOp.editSubstitute("PRECEDENCE_VALUE", "10");
  LessThanOp.setFunctionPrototype("HEADER_LESS_THAN_OPERATOR",
                                  "../Grammar/Expression.code");
  LessThanOp.editSubstitute("HEADER_BOOLEAN_GET_TYPE_MEMBER_FUNCTION",
                            "HEADER_BOOLEAN_GET_TYPE",
                            "../Grammar/Expression.code");
  LessThanOp.setFunctionSource("SOURCE_LESS_THAN_OPERATOR_EXPRESSION",
                               "../Grammar/Expression.code");
  LessThanOp.editSubstitute("SOURCE_BOOLEAN_GET_TYPE_MEMBER_FUNCTION",
                            "SOURCE_BOOLEAN_GET_TYPE",
                            "../Grammar/Expression.code");

  NEW_TERMINAL_MACRO(LongDoubleVal, "LongDoubleVal", "LONG_DOUBLE_VAL");
  LongDoubleVal.setFunctionSource(
      "SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
      "../Grammar/Expression.code");
  LongDoubleVal.setDataPrototype(
      "Rose::FloatingValue", "value", "= Rose::FloatingValue()",
      CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  LongDoubleVal.setDataPrototype("std::string", "valueString", "= \"\"",
                                 CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                                 NO_TRAVERSAL, NO_DELETE);
  LongDoubleVal.setFunctionSource("SOURCE_GET_TYPE_VALUE",
                                  "../Grammar/Expression.code");
  LongDoubleVal.editSubstitute("GENERIC_TYPE", "SgTypeLongDouble");

  LongDoubleVal.setFunctionPrototype("HEADER_EXTENDED_FLOAT_LITERAL",
                                     "../Grammar/Expression.code");
  LongDoubleVal.setFunctionSource("SOURCE_EXTENDED_FLOAT_LITERAL",
                                  "../Grammar/Expression.code");

  NEW_TERMINAL_MACRO(LongIntVal, "LongIntVal", "LONG_INT_VAL");
  LongIntVal.setFunctionSource("SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
                               "../Grammar/Expression.code");
  LongIntVal.setDataPrototype("long int", "value", "= 0", CONSTRUCTOR_PARAMETER,
                              BUILD_LITERAL_VALUE_ACCESS_FUNCTIONS,
                              NO_TRAVERSAL, NO_DELETE);
  LongIntVal.setDataPrototype("std::string", "valueString", "= \"\"",
                              CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                              NO_TRAVERSAL, NO_DELETE);
  LongIntVal.setFunctionSource("SOURCE_GET_TYPE_VALUE",
                               "../Grammar/Expression.code");
  LongIntVal.editSubstitute("GENERIC_TYPE", "SgTypeLong");

  NEW_TERMINAL_MACRO(LongLongIntVal, "LongLongIntVal", "LONG_LONG_INT_VAL");
  LongLongIntVal.setFunctionSource(
      "SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
      "../Grammar/Expression.code");
  LongLongIntVal.setDataPrototype(
      "long long int", "value", "= 0", CONSTRUCTOR_PARAMETER,
      BUILD_LITERAL_VALUE_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  LongLongIntVal.setDataPrototype("std::string", "valueString", "= \"\"",
                                  CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                                  NO_TRAVERSAL, NO_DELETE);
  LongLongIntVal.setFunctionSource("SOURCE_GET_TYPE_VALUE",
                                   "../Grammar/Expression.code");
  LongLongIntVal.editSubstitute("GENERIC_TYPE", "SgTypeLongLong");

  NEW_TERMINAL_MACRO(Integer128Val, "Integer128Val", "INTEGER_128_VAL");
  Integer128Val.setFunctionSource(
      "SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
      "../Grammar/Expression.code");
  Integer128Val.setDataPrototype(
      "Rose::Integer128Value", "value", "= Rose::Integer128Value()",
      CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  Integer128Val.setFunctionPrototype("HEADER_INTEGER128_LITERAL",
                                     "../Grammar/Expression.code");
  Integer128Val.setFunctionSource("SOURCE_INTEGER128_LITERAL",
                                  "../Grammar/Expression.code");

  NEW_TERMINAL_MACRO(FortranBOZVal, "FortranBOZVal", "FORTRAN_BOZ_VAL");
  FortranBOZVal.setDataPrototype("Rose::Integer128Value", "bits",
                                 "= Rose::Integer128Value::fromBits({}, true)",
                                 CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                                 NO_TRAVERSAL, NO_DELETE);
  FortranBOZVal.setFunctionPrototype("HEADER_FORTRAN_BOZ_LITERAL",
                                     "../Grammar/Expression.code");
  FortranBOZVal.setFunctionSource("SOURCE_FORTRAN_BOZ_LITERAL",
                                  "../Grammar/Expression.code");

  NEW_TERMINAL_MACRO(LshiftAssignOp, "LshiftAssignOp", "LSHIFT_ASSIGN_OP");
  LshiftAssignOp.setFunctionSource(
      "SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
      "../Grammar/Expression.code");
  LshiftAssignOp.editSubstitute("PRECEDENCE_VALUE", " 2");

  NEW_TERMINAL_MACRO(LshiftOp, "LshiftOp", "LSHIFT_OP");
  LshiftOp.setFunctionSource("SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
                             "../Grammar/Expression.code");
  LshiftOp.editSubstitute("PRECEDENCE_VALUE", "11");

  NEW_TERMINAL_MACRO(MemberFunctionRefExp, "MemberFunctionRefExp",
                     "MEMBER_FUNCTION_REF");
  MemberFunctionRefExp.setFunctionPrototype(
      "HEADER_MEMBER_FUNCTION_REF_EXPRESSION", "../Grammar/Expression.code");
  MemberFunctionRefExp.setDataPrototype(
      "SgMemberFunctionSymbol*", "symbol_i", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  MemberFunctionRefExp.setDataPrototype(
      "SgTypeExpression*", "conversion_type_id", "= NULL",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL,
      NO_DELETE, COPY_DATA, OPTIONAL_TRAVERSAL_MEMBER);
  MemberFunctionRefExp.setDataPrototype(
      "bool", "virtual_call", "= false", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  // The exact member symbol supplies the semantic function type.

  MemberFunctionRefExp.setDataPrototype(
      "bool", "need_qualifier", "= true", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  MemberFunctionRefExp.setDataPrototype(
      "int", "name_qualification_length", "= 0", NO_CONSTRUCTOR_PARAMETER,
      NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  MemberFunctionRefExp.setDataPrototype(
      "bool", "type_elaboration_required", "= false", NO_CONSTRUCTOR_PARAMETER,
      NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  MemberFunctionRefExp.setDataPrototype(
      "bool", "global_qualification_required", "= false",
      NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  MemberFunctionRefExp.setDataPrototype(
      "int", "explicit_name_qualification_length", "= -1",
      NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  MemberFunctionRefExp.setDataPrototype(
      "bool", "explicit_global_qualification", "= false",
      NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  MemberFunctionRefExp.setDataPrototype(
      "SgStringList", "explicit_name_qualification_tokens", "",
      NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  MemberFunctionRefExp.setFunctionSource("SOURCE_GET_TYPE_FROM_SYMBOL",
                                         "../Grammar/Expression.code");

  NEW_TERMINAL_MACRO(MinusAssignOp, "MinusAssignOp", "MINUS_ASSIGN_OP");
  MinusAssignOp.setFunctionSource(
      "SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
      "../Grammar/Expression.code");
  MinusAssignOp.editSubstitute("PRECEDENCE_VALUE", " 2");

  NEW_TERMINAL_MACRO(MinusMinusOp, "MinusMinusOp", "MINUSMINUS_OP");
  MinusMinusOp.setFunctionSource(
      "SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
      "../Grammar/Expression.code");
  MinusMinusOp.editSubstitute("PRECEDENCE_VALUE", "15");
  MinusMinusOp.setFunctionPrototype("HEADER_MINUS_MINUS_OPERATOR",
                                    "../Grammar/Expression.code");

  NEW_TERMINAL_MACRO(MinusOp, "MinusOp", "UNARY_MINUS_OP");
  MinusOp.setFunctionSource("SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
                            "../Grammar/Expression.code");
  MinusOp.editSubstitute("PRECEDENCE_VALUE", "15");

  NEW_TERMINAL_MACRO(ModAssignOp, "ModAssignOp", "MOD_ASSIGN_OP");
  ModAssignOp.setFunctionSource("SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
                                "../Grammar/Expression.code");
  ModAssignOp.editSubstitute("PRECEDENCE_VALUE", " 2");

  NEW_TERMINAL_MACRO(ModOp, "ModOp", "MOD_OP");
  ModOp.setFunctionSource("SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
                          "../Grammar/Expression.code");
  ModOp.editSubstitute("PRECEDENCE_VALUE", "13");

  NEW_TERMINAL_MACRO(MultAssignOp, "MultAssignOp", "MULT_ASSIGN_OP");
  MultAssignOp.setFunctionSource(
      "SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
      "../Grammar/Expression.code");
  MultAssignOp.editSubstitute("PRECEDENCE_VALUE", " 2");

  NEW_TERMINAL_MACRO(MultiplyOp, "MultiplyOp", "MULT_OP");
  MultiplyOp.setFunctionSource("SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
                               "../Grammar/Expression.code");
  MultiplyOp.editSubstitute("PRECEDENCE_VALUE", "13");

  NEW_TERMINAL_MACRO(NewExp, "NewExp", "NEW_OP");
  NewExp.setFunctionPrototype("HEADER_GET_NEXT_EXPRESSION",
                              "../Grammar/Expression.code");
  NewExp.setFunctionPrototype("HEADER_NEW_OPERATOR_EXPRESSION",
                              "../Grammar/Expression.code");
  NewExp.setDataPrototype("SgType*", "specified_type", "= NULL",
                          CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                          NO_TRAVERSAL, NO_DELETE);
  NewExp.setDataPrototype("SgPointerType*", "expression_type", "= NULL",
                          CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                          NO_TRAVERSAL, NO_DELETE);
  NewExp.setDataPrototype("SgExpression*", "array_bound", "= NULL",
                          CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                          DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
                          OPTIONAL_TRAVERSAL_MEMBER);
  NewExp.setDataPrototype("SgNewExp::array_bound_source_form_enum",
                          "array_bound_source_form", "= e_no_array_bound",
                          CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                          NO_TRAVERSAL, NO_DELETE);
  NewExp.setDataPrototype("SgExprListExp*", "placement_args", "= NULL",
                          CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                          DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
                          OPTIONAL_TRAVERSAL_MEMBER);
  NewExp.setDataPrototype("SgConstructorInitializer*", "constructor_args",
                          "= NULL", CONSTRUCTOR_PARAMETER,
                          BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE,
                          COPY_DATA, OPTIONAL_TRAVERSAL_MEMBER);
  NewExp.setDataPrototype("SgExpression*", "builtin_args", "= NULL",
                          CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                          DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
                          OPTIONAL_TRAVERSAL_MEMBER);
  NewExp.setDataPrototype("short", "need_global_specifier", "= 0",
                          CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                          NO_TRAVERSAL, NO_DELETE);
  NewExp.setDataPrototype("bool", "type_id_is_parenthesized", "= false",
                          NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                          NO_TRAVERSAL, NO_DELETE);
  NewExp.setDataPrototype("SgFunctionDeclaration*", "newOperatorDeclaration",
                          "= NULL", CONSTRUCTOR_PARAMETER,
                          BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE,
                          COPY_CONSTRUCTOR_REFERENCE);
  NewExp.setDataPrototype("unsigned int", "allocation_implicit_argument_count",
                          "= 1", CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                          NO_TRAVERSAL, NO_DELETE, COPY_DATA);

  NewExp.setDataPrototype("int", "name_qualification_length", "= 0",
                          NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                          NO_TRAVERSAL, NO_DELETE);

  NewExp.setDataPrototype("bool", "type_elaboration_required", "= false",
                          NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                          NO_TRAVERSAL, NO_DELETE);

  NewExp.setDataPrototype("bool", "global_qualification_required", "= false",
                          NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                          NO_TRAVERSAL, NO_DELETE);

  // Preserve the exact nested-name-specifier written on the allocated TypeLoc.
  // The SgType is shared semantic identity and cannot own per-use spelling.
  NewExp.setDataPrototype("bool", "explicit_name_qualification_present",
                          "= false", NO_CONSTRUCTOR_PARAMETER,
                          BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  NewExp.setDataPrototype("bool", "explicit_global_qualification", "= false",
                          NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                          NO_TRAVERSAL, NO_DELETE);
  NewExp.setDataPrototype("SgStringList", "explicit_name_qualification_tokens",
                          "", NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                          NO_TRAVERSAL, NO_DELETE);

  NewExp.setDataPrototype(
      "int", "name_qualification_for_pointer_to_member_class_length", "= 0",
      NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  NewExp.setDataPrototype(
      "bool", "type_elaboration_for_pointer_to_member_class_required",
      "= false", NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);

  NewExp.setDataPrototype(
      "bool", "global_qualification_for_pointer_to_member_class_required",
      "= false", NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);

  NEW_TERMINAL_MACRO(NoexceptOp, "NoexceptOp", "NOEXCEPT_OP");
  NoexceptOp.setFunctionSource(
      "SOURCE_NOEXCEPT_OPERATOR_POST_CONSTRUCTION_INITIALIZATION",
      "../Grammar/Expression.code");
  NoexceptOp.editSubstitute("PRECEDENCE_VALUE", "16");
  NoexceptOp.setFunctionPrototype("HEADER_NOEXCEPT_OPERATOR",
                                  "../Grammar/Expression.code");
  NoexceptOp.setDataPrototype("SgExpression*", "operand_expr", "",
                              CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                              DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
                              REQUIRED_TRAVERSAL_MEMBER);

  // C++20 requires-expressions are structural AST.  A raw source string here
  // used to hide failed frontend translation and made every downstream pass
  // blind to local parameters, requirements, and referenced declarations.
  NEW_TERMINAL_MACRO(SimpleRequirement, "SimpleRequirement",
                     "SIMPLE_REQUIREMENT");
  SimpleRequirement.setFunctionSource(
      "SOURCE_SIMPLE_REQUIREMENT_POST_CONSTRUCTION_INITIALIZATION",
      "../Grammar/Expression.code");
  SimpleRequirement.setFunctionSource("SOURCE_SYNTAX_ONLY_GET_TYPE",
                                      "../Grammar/Expression.code");
  SimpleRequirement.setDataPrototype(
      "SgExpression*", "expression", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);

  NEW_TERMINAL_MACRO(TypeRequirement, "TypeRequirement", "TYPE_REQUIREMENT");
  TypeRequirement.setFunctionSource(
      "SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
      "../Grammar/Expression.code");
  TypeRequirement.setFunctionSource("SOURCE_SYNTAX_ONLY_GET_TYPE",
                                    "../Grammar/Expression.code");
  TypeRequirement.setFunctionPrototype("HEADER_TYPE_REQUIREMENT",
                                       "../Grammar/Expression.code");
  TypeRequirement.setDataPrototype(
      "SgType*", "required_type", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  TypeRequirement.setDataPrototype(
      "int", "name_qualification_length", "= 0", NO_CONSTRUCTOR_PARAMETER,
      NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  TypeRequirement.setDataPrototype(
      "bool", "type_elaboration_required", "= false", NO_CONSTRUCTOR_PARAMETER,
      NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  TypeRequirement.setDataPrototype(
      "bool", "global_qualification_required", "= false",
      NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  TypeRequirement.setDataPrototype(
      "bool", "explicit_name_qualification_present", "= false",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);
  TypeRequirement.setDataPrototype("bool", "explicit_global_qualification",
                                   "= false", NO_CONSTRUCTOR_PARAMETER,
                                   BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
                                   NO_DELETE);
  TypeRequirement.setDataPrototype(
      "SgStringList", "explicit_name_qualification_tokens", "",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);

  NEW_TERMINAL_MACRO(CompoundRequirement, "CompoundRequirement",
                     "COMPOUND_REQUIREMENT");
  CompoundRequirement.setFunctionSource(
      "SOURCE_COMPOUND_REQUIREMENT_POST_CONSTRUCTION_INITIALIZATION",
      "../Grammar/Expression.code");
  CompoundRequirement.setFunctionSource("SOURCE_SYNTAX_ONLY_GET_TYPE",
                                        "../Grammar/Expression.code");
  CompoundRequirement.setDataPrototype(
      "SgExpression*", "expression", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);
  CompoundRequirement.setDataPrototype(
      "bool", "noexcept_required", "= false", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  CompoundRequirement.setDataPrototype(
      "SgExpression*", "type_constraint", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);

  // A failed template substitution has no Clang expression/type node to
  // translate.  Preserve that semantic state as its own typed requirement
  // record instead of manufacturing a null child or recovering source text.
  NEW_TERMINAL_MACRO(RequirementSubstitutionFailure,
                     "RequirementSubstitutionFailure",
                     "REQUIREMENT_SUBSTITUTION_FAILURE");
  RequirementSubstitutionFailure.setFunctionSource(
      "SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
      "../Grammar/Expression.code");
  RequirementSubstitutionFailure.setFunctionSource(
      "SOURCE_SYNTAX_ONLY_GET_TYPE", "../Grammar/Expression.code");
  RequirementSubstitutionFailure.setFunctionPrototype(
      "HEADER_REQUIREMENT_SUBSTITUTION_FAILURE", "../Grammar/Expression.code");
  RequirementSubstitutionFailure.setDataPrototype(
      "SgRequirementSubstitutionFailure::failure_kind_enum", "failure_kind",
      "= SgRequirementSubstitutionFailure::e_simple_expression_failure",
      CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  RequirementSubstitutionFailure.setDataPrototype(
      "std::string", "substituted_entity", "= \"\"", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  RequirementSubstitutionFailure.setDataPrototype(
      "std::string", "diagnostic_message", "= \"\"", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  NEW_TERMINAL_MACRO(NestedRequirement, "NestedRequirement",
                     "NESTED_REQUIREMENT");
  NestedRequirement.setFunctionSource(
      "SOURCE_NESTED_REQUIREMENT_POST_CONSTRUCTION_INITIALIZATION",
      "../Grammar/Expression.code");
  NestedRequirement.setFunctionSource("SOURCE_SYNTAX_ONLY_GET_TYPE",
                                      "../Grammar/Expression.code");
  NestedRequirement.setDataPrototype(
      "SgExpression*", "constraint", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);

  NEW_TERMINAL_MACRO(RequiresExpr, "RequiresExpr", "REQUIRES_EXPR");
  RequiresExpr.setFunctionSource(
      "SOURCE_REQUIRES_EXPRESSION_POST_CONSTRUCTION_INITIALIZATION",
      "../Grammar/Expression.code");
  RequiresExpr.setFunctionSource("SOURCE_GET_TYPE_GENERIC",
                                 "../Grammar/Expression.code");
  RequiresExpr.editSubstitute("GENERIC_TYPE", "SgTypeBool");
  RequiresExpr.setDataPrototype(
      "SgFunctionParameterList*", "local_parameter_list", "= NULL",
      CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE,
      COPY_DATA, OPTIONAL_TRAVERSAL_MEMBER);
  RequiresExpr.setDataPrototype(
      "SgFunctionParameterScope*", "local_parameter_scope", "= NULL",
      CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE,
      COPY_DATA, OPTIONAL_TRAVERSAL_MEMBER);
  RequiresExpr.setDataPrototype("SgExprListExp*", "requirements", "= NULL",
                                CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                                DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
                                OPTIONAL_TRAVERSAL_MEMBER);
  RequiresExpr.setDataPrototype("SgFunctionParameterList*",
                                "local_parameter_list_syntax", "= NULL",
                                CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                                NO_TRAVERSAL, DEF_DELETE, CLONE_TREE);

  NEW_TERMINAL_MACRO(NonrealRefExp, "NonrealRefExp", "NONREAL_REF");
  NonrealRefExp.setFunctionSource(
      "SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
      "../Grammar/Expression.code");
  NonrealRefExp.setFunctionPrototype("HEADER_NONREAL_REF_EXPRESSION",
                                     "../Grammar/Expression.code");
  // A dependent conversion-function-id owns its written type syntax. It is
  // not an identifier containing a printable approximation of a Clang type.
  NonrealRefExp.setDataPrototype(
      "SgNonrealRefExp::name_kind_enum", "name_kind",
      "= SgNonrealRefExp::e_ordinary_name", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  NonrealRefExp.setDataPrototype(
      "SgTypeExpression*", "conversion_type_id", "= NULL",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL,
      NO_DELETE, COPY_DATA, OPTIONAL_TRAVERSAL_MEMBER);
  NonrealRefExp.setDataPrototype(
      "SgNonrealSymbol*", "symbol", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_FLAG_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  // A nonreal reference can preserve source-only syntax (for example, the
  // explicitly written arguments in `f<int>`) even when Clang has resolved
  // the reference to an exact callable.  Keep that semantic identity on the
  // use site instead of mutating the shared function declaration.
  NonrealRefExp.setDataPrototype(
      "SgFunctionDeclaration*", "resolved_function_declaration", "= NULL",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);
  // An explicitly spelled variable-template reference still needs the
  // SgNonrealRefExp source surface to own its written template arguments, but
  // its semantic identity and value type come from the exact translated
  // specialization declaration.  Keep that declaration as a non-owning
  // semantic edge; never substitute the synthetic SgNonrealSymbol's type for
  // a resolved variable template.
  NonrealRefExp.setDataPrototype(
      "SgTemplateVariableDeclaration*", "resolved_variable_declaration",
      "= NULL", NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);
  // Storage-use classification belongs to each reference, not its shared
  // variable-template specialization.
  NonrealRefExp.setDataPrototype("bool", "is_non_odr_use", "= false",
                                 CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                                 NO_TRAVERSAL, NO_DELETE, COPY_DATA);
  // Distinguish an ordinary source-only name from a callable whose target is
  // intentionally dependent on template instantiation and ADL. Consumers
  // must not infer this semantic fact merely from a missing resolved edge.
  NonrealRefExp.setDataPrototype(
      "SgNonrealRefExp::semantic_role_enum", "semantic_role",
      "= SgNonrealRefExp::e_nonreal_reference", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  NonrealRefExp.setDataPrototype("int", "name_qualification_length", "= 0",
                                 NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                                 NO_TRAVERSAL, NO_DELETE);
  NonrealRefExp.setDataPrototype("bool", "type_elaboration_required", "= false",
                                 NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                                 NO_TRAVERSAL, NO_DELETE);
  NonrealRefExp.setDataPrototype("bool", "global_qualification_required",
                                 "= false", NO_CONSTRUCTOR_PARAMETER,
                                 NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  NonrealRefExp.setDataPrototype("int", "explicit_name_qualification_length",
                                 "= -1", NO_CONSTRUCTOR_PARAMETER,
                                 NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  NonrealRefExp.setDataPrototype("bool", "explicit_global_qualification",
                                 "= false", NO_CONSTRUCTOR_PARAMETER,
                                 NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  NonrealRefExp.setDataPrototype(
      "SgStringList", "explicit_name_qualification_tokens", "",
      NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  NonrealRefExp.setDataPrototype(
      "SgTemplateArgumentPtrList", "templateArguments",
      "= SgTemplateArgumentPtrList()", NO_CONSTRUCTOR_PARAMETER,
      BUILD_LIST_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, CLONE_TREE);
  NonrealRefExp.setDataPrototype("bool", "explicit_template_argument_list",
                                 "= false", NO_CONSTRUCTOR_PARAMETER,
                                 BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
                                 NO_DELETE);
  // REX: Constraint satisfaction + SFINAE results.
  addConstraintSatisfactionPrototypes(NonrealRefExp);
  addSFINAEPrototypes(NonrealRefExp);
  NonrealRefExp.setFunctionSource("SOURCE_NONREAL_REFERENCE_GET_TYPE",
                                  "../Grammar/Expression.code");

  NEW_TERMINAL_MACRO(NotOp, "NotOp", "NOT_OP");
  NotOp.setFunctionSource("SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
                          "../Grammar/Expression.code");
  NotOp.editSubstitute("PRECEDENCE_VALUE", "15");
  NotOp.setFunctionPrototype("HEADER_NOT_OPERATOR",
                             "../Grammar/Expression.code");
  NotOp.setFunctionSource("SOURCE_NOT_OPERATOR_EXPRESSION",
                          "../Grammar/Expression.code");
  NotOp.editSubstitute("SOURCE_BOOLEAN_GET_TYPE_MEMBER_FUNCTION",
                       "SOURCE_BOOLEAN_GET_TYPE", "../Grammar/Expression.code");

  NEW_TERMINAL_MACRO(NotEqualOp, "NotEqualOp", "NE_OP");
  NotEqualOp.setFunctionSource("SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
                               "../Grammar/Expression.code");
  NotEqualOp.editSubstitute("PRECEDENCE_VALUE", " 9");
  NotEqualOp.setFunctionPrototype("HEADER_NOT_EQUAL_OPERATOR",
                                  "../Grammar/Expression.code");
  NotEqualOp.editSubstitute("HEADER_BOOLEAN_GET_TYPE_MEMBER_FUNCTION",
                            "HEADER_BOOLEAN_GET_TYPE",
                            "../Grammar/Expression.code");
  NotEqualOp.setFunctionSource("SOURCE_NOT_EQUAL_OPERATOR_EXPRESSION",
                               "../Grammar/Expression.code");
  NotEqualOp.editSubstitute("SOURCE_BOOLEAN_GET_TYPE_MEMBER_FUNCTION",
                            "SOURCE_BOOLEAN_GET_TYPE",
                            "../Grammar/Expression.code");

  // DQ (12/13/2005): Added support for empty expression (and empty statement).
  NEW_TERMINAL_MACRO(NullExpression, "NullExpression", "NULL_EXPR");
  NullExpression.setFunctionSource(
      "SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
      "../Grammar/Expression.code");
  NullExpression.editSubstitute("PRECEDENCE_VALUE", "16");
  NullExpression.setFunctionPrototype("HEADER_NULL_EXPRESSION",
                                      "../Grammar/Expression.code");
  NullExpression.setDataPrototype(
      "SgNullExpression::null_expression_role_enum", "role",
      "= SgNullExpression::e_null_expression_unclassified",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);
  NullExpression.setFunctionSource("SOURCE_SYNTAX_ONLY_GET_TYPE",
                                   "../Grammar/Expression.code");

  // An OpenMP grammar identifier is syntax carried by a directive rather than
  // a C, C++, or Fortran variable reference.  Keep it explicit so parser
  // clients never fabricate unresolved symbols merely to preserve spelling.
  NEW_TERMINAL_MACRO(OmpNameExpression, "OmpNameExpression",
                     "OMP_NAME_EXPRESSION");
  OmpNameExpression.setFunctionSource(
      "SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
      "../Grammar/Expression.code");
  OmpNameExpression.editSubstitute("PRECEDENCE_VALUE", "16");
  OmpNameExpression.setDataPrototype(
      "std::string", "spelling", "= \"\"", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  // A frontend-owned semantic reference to a variable declared by OpenMP
  // directive syntax (for example a mapper variable or iterator).  Such a
  // variable has an exact semantic type before the directive parser creates
  // its final declaration scope, but deliberately has no ordinary program
  // symbol.  Keep that state distinct from both SgVarRefExp and the syntax-only
  // SgOmpNameExpression.
  NEW_TERMINAL_MACRO(OmpDirectiveLocalRefExp, "OmpDirectiveLocalRefExp",
                     "OMP_DIRECTIVE_LOCAL_REF_EXP");
  OmpDirectiveLocalRefExp.setFunctionSource(
      "SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
      "../Grammar/Expression.code");
  OmpDirectiveLocalRefExp.editSubstitute("PRECEDENCE_VALUE", "16");
  OmpDirectiveLocalRefExp.setDataPrototype(
      "std::string", "spelling", "= \"\"", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  OmpDirectiveLocalRefExp.setDataPrototype(
      "SgType*", "expression_type", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  // Exact source spelling for an OpenMP clause expression whose semantic AST
  // was built from macro-expanded tokens.  This is deliberately distinct from
  // SgOmpNameExpression: the spelling can be a complete expression or locator
  // list, not merely an OpenMP grammar identifier.
  NEW_TERMINAL_MACRO(OmpSourceExpression, "OmpSourceExpression",
                     "OMP_SOURCE_EXPRESSION");
  OmpSourceExpression.setFunctionSource(
      "SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
      "../Grammar/Expression.code");
  OmpSourceExpression.editSubstitute("PRECEDENCE_VALUE", "16");
  OmpSourceExpression.setDataPrototype(
      "std::string", "spelling", "= \"\"", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  // One semantic induction-clause item.  The enum distinguishes the closed
  // grammar alternatives; labels and expressions are data, never magic tag
  // expressions interpreted by the unparser.
  NEW_TERMINAL_MACRO(OmpInductionItem, "OmpInductionItem",
                     "OMP_INDUCTION_ITEM");
  OmpInductionItem.setFunctionSource(
      "SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
      "../Grammar/Expression.code");
  OmpInductionItem.setFunctionPrototype("HEADER_OMP_TYPED_EXPRESSION_OWNER",
                                        "../Grammar/Expression.code");
  OmpInductionItem.editSubstitute("PRECEDENCE_VALUE", "16");
  OmpInductionItem.setDataPrototype(
      "SgOmpClause::omp_induction_item_kind_enum", "kind",
      "= SgOmpClause::e_omp_induction_item_unknown", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  OmpInductionItem.setDataPrototype(
      "std::string", "label", "= \"\"", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  OmpInductionItem.setDataPrototype(
      "SgExpression*", "expression", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, CLONE_PTR,
      OPTIONAL_TRAVERSAL_MEMBER);

  // One apply-clause transformation.  Standard transformations use the kind
  // enum. Extension transformations use the dedicated transformation-name
  // field. A nested apply is represented by its clause subtree rather than
  // reparsed text.
  NEW_TERMINAL_MACRO(OmpApplyTransformation, "OmpApplyTransformation",
                     "OMP_APPLY_TRANSFORMATION");
  OmpApplyTransformation.setFunctionSource(
      "SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
      "../Grammar/Expression.code");
  OmpApplyTransformation.setFunctionPrototype(
      "HEADER_OMP_TYPED_EXPRESSION_OWNER", "../Grammar/Expression.code");
  OmpApplyTransformation.editSubstitute("PRECEDENCE_VALUE", "16");
  OmpApplyTransformation.setDataPrototype(
      "SgOmpClause::omp_apply_transform_kind_enum", "kind",
      "= SgOmpClause::e_omp_apply_transform_unknown", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  OmpApplyTransformation.setDataPrototype(
      "SgOmpClause::omp_clause_separator_enum", "separator",
      "= SgOmpClause::e_omp_clause_separator_unknown", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  OmpApplyTransformation.setDataPrototype(
      "std::string", "transformation_name", "= \"\"", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  OmpApplyTransformation.setDataPrototype(
      "SgExpression*", "argument", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, CLONE_PTR,
      OPTIONAL_TRAVERSAL_MEMBER);
  OmpApplyTransformation.setDataPrototype(
      "SgOmpApplyClause*", "nested_apply", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, CLONE_PTR,
      OPTIONAL_TRAVERSAL_MEMBER);

  // One init/append_args modifier. Closed alternatives use the enum and only
  // modifiers defined by the OpenMP grammar may own an expression.
  NEW_TERMINAL_MACRO(OmpInitModifier, "OmpInitModifier", "OMP_INIT_MODIFIER");
  OmpInitModifier.setFunctionSource(
      "SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
      "../Grammar/Expression.code");
  OmpInitModifier.setFunctionPrototype("HEADER_OMP_TYPED_EXPRESSION_OWNER",
                                       "../Grammar/Expression.code");
  OmpInitModifier.editSubstitute("PRECEDENCE_VALUE", "16");
  OmpInitModifier.setDataPrototype(
      "SgOmpClause::omp_init_modifier_kind_enum", "kind",
      "= SgOmpClause::e_omp_init_modifier_unknown", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  OmpInitModifier.setDataPrototype(
      "SgExpression*", "expression", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, CLONE_PTR,
      OPTIONAL_TRAVERSAL_MEMBER);

  // Typed ownership boundary for an init clause's ordered modifier sequence.
  // Keeping the container in a dedicated node lets SgOmpInitClause own only
  // singleton traversal successors, as required by ROSETTA's clean model.
  NEW_TERMINAL_MACRO(OmpInitModifierList, "OmpInitModifierList",
                     "OMP_INIT_MODIFIER_LIST");
  OmpInitModifierList.setFunctionSource(
      "SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
      "../Grammar/Expression.code");
  OmpInitModifierList.setFunctionPrototype("HEADER_OMP_TYPED_EXPRESSION_OWNER",
                                           "../Grammar/Expression.code");
  OmpInitModifierList.editSubstitute("PRECEDENCE_VALUE", "16");
  OmpInitModifierList.setDataPrototype(
      "SgOmpInitModifierPtrList", "modifiers", "", NO_CONSTRUCTOR_PARAMETER,
      BUILD_LIST_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE);

  // One typed append_args interop operation. The operation node owns the
  // modifier-list wrapper so operation and modifier boundaries cannot be
  // flattened or inferred during unparsing.
  NEW_TERMINAL_MACRO(OmpAppendArgsOperation, "OmpAppendArgsOperation",
                     "OMP_APPEND_ARGS_OPERATION");
  OmpAppendArgsOperation.setFunctionSource(
      "SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
      "../Grammar/Expression.code");
  OmpAppendArgsOperation.setFunctionPrototype(
      "HEADER_OMP_TYPED_EXPRESSION_OWNER", "../Grammar/Expression.code");
  OmpAppendArgsOperation.editSubstitute("PRECEDENCE_VALUE", "16");
  OmpAppendArgsOperation.setDataPrototype(
      "SgOmpInitModifierList*", "modifier_list", "= NULL",
      CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE,
      CLONE_TREE, OPTIONAL_TRAVERSAL_MEMBER);

  // One typed experimental dist-data policy attached to exactly one map
  // item. The optional expression is the block/cyclic argument.
  NEW_TERMINAL_MACRO(OmpMapDistDataPolicy, "OmpMapDistDataPolicy",
                     "OMP_MAP_DIST_DATA_POLICY");
  OmpMapDistDataPolicy.setFunctionSource(
      "SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
      "../Grammar/Expression.code");
  OmpMapDistDataPolicy.setFunctionPrototype("HEADER_OMP_TYPED_EXPRESSION_OWNER",
                                            "../Grammar/Expression.code");
  OmpMapDistDataPolicy.editSubstitute("PRECEDENCE_VALUE", "16");
  OmpMapDistDataPolicy.setDataPrototype(
      "SgOmpClause::omp_map_dist_data_enum", "policy",
      "= SgOmpClause::e_omp_map_dist_data_unknown", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  OmpMapDistDataPolicy.setDataPrototype(
      "SgExpression*", "expression", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, CLONE_PTR,
      OPTIONAL_TRAVERSAL_MEMBER);

  // Canonical ordered map-clause item. It owns the exact locator expression
  // and the policies that follow that item in source; no symbol-keyed side
  // channel is permitted.
  NEW_TERMINAL_MACRO(OmpMapItem, "OmpMapItem", "OMP_MAP_ITEM");
  OmpMapItem.setFunctionSource("SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
                               "../Grammar/Expression.code");
  OmpMapItem.setFunctionPrototype("HEADER_OMP_TYPED_EXPRESSION_OWNER",
                                  "../Grammar/Expression.code");
  OmpMapItem.editSubstitute("PRECEDENCE_VALUE", "16");
  OmpMapItem.setDataPrototype("SgExpression*", "expression", "= NULL",
                              CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                              DEF_TRAVERSAL, NO_DELETE, CLONE_PTR,
                              OPTIONAL_TRAVERSAL_MEMBER);
  OmpMapItem.setDataPrototype(
      "SgOmpMapDistDataPolicyPtrList", "policies", "", NO_CONSTRUCTOR_PARAMETER,
      BUILD_LIST_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE);

  // Exact source syntax for a C/C++ macro invocation together with the
  // expanded semantic expression produced by Clang. The spelling is the
  // original unparse surface; the traversed child preserves semantic identity
  // and records any later changes that require emission of the expansion.
  NEW_TERMINAL_MACRO(MacroExpansionExp, "MacroExpansionExp",
                     "MACRO_EXPANSION_EXPR");
  MacroExpansionExp.setFunctionSource(
      "SOURCE_MACRO_EXPANSION_POST_CONSTRUCTION_INITIALIZATION",
      "../Grammar/Expression.code");
  MacroExpansionExp.editSubstitute("PRECEDENCE_VALUE", "16");
  MacroExpansionExp.setFunctionPrototype("HEADER_MACRO_EXPANSION_EXPRESSION",
                                         "../Grammar/Expression.code");
  MacroExpansionExp.setDataPrototype(
      "std::string", "spelling", "", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  MacroExpansionExp.setDataPrototype("SgExpression*", "expanded_expression", "",
                                     CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                                     DEF_TRAVERSAL, NO_DELETE, CLONE_PTR,
                                     OPTIONAL_TRAVERSAL_MEMBER);
  MacroExpansionExp.setDataPrototype(
      "bool", "expansion_was_modified", "= false", CONSTRUCTOR_PARAMETER,
      NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  MacroExpansionExp.setDataPrototype(
      "uint64_t", "expansion_source_sequence", "= 0", NO_CONSTRUCTOR_PARAMETER,
      NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  NEW_TERMINAL_MACRO(OpaqueValueBinding, "OpaqueValueBinding",
                     "OPAQUE_VALUE_BINDING");
  OpaqueValueBinding.setFunctionPrototype("HEADER_OPAQUE_VALUE_BINDING",
                                          "../Grammar/Expression.code");
  OpaqueValueBinding.setDataPrototype(
      "SgExpression*", "source_expression", "= nullptr", CONSTRUCTOR_PARAMETER,
      NO_ACCESS_FUNCTIONS, DEF_TRAVERSAL, DEF_DELETE, CLONE_TREE,
      REQUIRED_TRAVERSAL_MEMBER, DIRECT_TRAVERSAL_ACCESS,
      COPY_BEFORE_CONSTRUCTOR_DEPENDENCIES);
  OpaqueValueBinding.setDataPrototype(
      "SgType*", "expression_type", "= nullptr", CONSTRUCTOR_PARAMETER,
      NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  OpaqueValueBinding.setDataPrototype(
      "SgCastExp::value_category_enum", "value_category",
      "= SgCastExp::e_value_category_unclassified", CONSTRUCTOR_PARAMETER,
      NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  OpaqueValueBinding.setDataPrototype(
      "SgOpaqueValueBinding::object_kind_enum", "object_kind",
      "= SgOpaqueValueBinding::e_ordinary_object", CONSTRUCTOR_PARAMETER,
      NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  NEW_TERMINAL_MACRO(OpaqueValueReference, "OpaqueValueReference",
                     "OPAQUE_VALUE_REFERENCE");
  OpaqueValueReference.setFunctionPrototype("HEADER_OPAQUE_VALUE_REFERENCE",
                                            "../Grammar/Expression.code");
  OpaqueValueReference.setDataPrototype(
      "SgOpaqueValueBinding*", "binding", "= nullptr", CONSTRUCTOR_PARAMETER,
      NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE, COPY_CONSTRUCTOR_REFERENCE);

  NEW_TERMINAL_MACRO(PseudoObjectExp, "PseudoObjectExp", "PSEUDO_OBJECT_EXPR");
  PseudoObjectExp.setFunctionPrototype("HEADER_PSEUDO_OBJECT_EXPRESSION",
                                       "../Grammar/Expression.code");
  PseudoObjectExp.setDataPrototype(
      "SgExpression*", "written_expression", "= nullptr", CONSTRUCTOR_PARAMETER,
      NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, DEF_DELETE, CLONE_TREE);
  PseudoObjectExp.setDataPrototype(
      "SgExprListExp*", "semantic_evaluations", "= nullptr",
      CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, DEF_TRAVERSAL, DEF_DELETE,
      CLONE_TREE, REQUIRED_TRAVERSAL_MEMBER, DIRECT_TRAVERSAL_ACCESS,
      COPY_BEFORE_CONSTRUCTOR_DEPENDENCIES);
  PseudoObjectExp.setDataPrototype(
      "SgExpression*", "result_expression", "= nullptr", CONSTRUCTOR_PARAMETER,
      NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE, COPY_CONSTRUCTOR_REFERENCE);
  PseudoObjectExp.setDataPrototype(
      "SgOpaqueValueBindingPtrList", "captured_bindings",
      "= SgOpaqueValueBindingPtrList()", CONSTRUCTOR_PARAMETER,
      NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE, COPY_CONSTRUCTOR_REFERENCE);
  PseudoObjectExp.setDataPrototype("SgType*", "expression_type", "= nullptr",
                                   CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                                   NO_TRAVERSAL, NO_DELETE);
  PseudoObjectExp.setDataPrototype(
      "SgCastExp::value_category_enum", "value_category",
      "= SgCastExp::e_value_category_unclassified", CONSTRUCTOR_PARAMETER,
      NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  PseudoObjectExp.setDataPrototype(
      "SgOpaqueValueBinding::object_kind_enum", "object_kind",
      "= SgOpaqueValueBinding::e_ordinary_object", CONSTRUCTOR_PARAMETER,
      NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  PseudoObjectExp.setDataPrototype(
      "uint64_t", "source_sequence", "= 0", NO_CONSTRUCTOR_PARAMETER,
      NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  NEW_TERMINAL_MACRO(CxxDecompositionRefExp, "CxxDecompositionRefExp",
                     "CXX_DECOMPOSITION_REF");
  CxxDecompositionRefExp.setFunctionPrototype(
      "HEADER_CXX_DECOMPOSITION_REFERENCE", "../Grammar/Expression.code");
  CxxDecompositionRefExp.setFunctionSource("SOURCE_CXX_DECOMPOSITION_REFERENCE",
                                           "../Grammar/Expression.code");
  CxxDecompositionRefExp.setDataPrototype(
      "SgInitializedName*", "decomposition", "", CONSTRUCTOR_PARAMETER,
      NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE, COPY_CONSTRUCTOR_REFERENCE);
  CxxDecompositionRefExp.setDataPrototype(
      "SgType*", "expression_type", "", CONSTRUCTOR_PARAMETER,
      NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  CxxDecompositionRefExp.setFunctionSource("SOURCE_EXACT_EXPRESSION_GET_TYPE",
                                           "../Grammar/Expression.code");

  NEW_TERMINAL_MACRO(CxxSubobjectDestruction, "CxxSubobjectDestruction",
                     "CXX_SUBOBJECT_DESTRUCTION");
  CxxSubobjectDestruction.setFunctionPrototype(
      "HEADER_CXX_SUBOBJECT_DESTRUCTION", "../Grammar/Expression.code");
  CxxSubobjectDestruction.setDataPrototype(
      "SgMemberFunctionDeclaration*", "destructor_declaration", "= nullptr",
      CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE,
      COPY_CONSTRUCTOR_REFERENCE);
  CxxSubobjectDestruction.setDataPrototype(
      "SgInitializedName*", "member_target", "= nullptr", CONSTRUCTOR_PARAMETER,
      NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE, COPY_CONSTRUCTOR_REFERENCE);
  CxxSubobjectDestruction.setDataPrototype(
      "SgClassDeclaration*", "base_target", "= nullptr", CONSTRUCTOR_PARAMETER,
      NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE, COPY_CONSTRUCTOR_REFERENCE);
  CxxSubobjectDestruction.setDataPrototype(
      "SgCxxSubobjectDestruction::target_kind_enum", "target_kind",
      "= SgCxxSubobjectDestruction::e_unclassified", CONSTRUCTOR_PARAMETER,
      NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  CxxSubobjectDestruction.setDataPrototype(
      "SgType*", "expression_type", "= nullptr", CONSTRUCTOR_PARAMETER,
      NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  CxxSubobjectDestruction.setFunctionSource("SOURCE_EXACT_EXPRESSION_GET_TYPE",
                                            "../Grammar/Expression.code");

  NEW_TERMINAL_MACRO(CxxArrayInitLoop, "CxxArrayInitLoop",
                     "CXX_ARRAY_INIT_LOOP");
  CxxArrayInitLoop.setFunctionPrototype("HEADER_CXX_ARRAY_INIT_LOOP",
                                        "../Grammar/Expression.code");
  CxxArrayInitLoop.setDataPrototype(
      "SgCxxArrayInitBinding*", "binding", "= nullptr", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, CLONE_PTR,
      REQUIRED_TRAVERSAL_MEMBER);
  CxxArrayInitLoop.setDataPrototype(
      "SgExpression*", "element_initializer", "= nullptr",
      CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE,
      CLONE_PTR, REQUIRED_TRAVERSAL_MEMBER);
  CxxArrayInitLoop.setDataPrototype(
      "SgType*", "expression_type", "= nullptr", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  CxxArrayInitLoop.setDataPrototype(
      "uint64_t", "element_count", "= 0", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  CxxArrayInitLoop.setFunctionSource("SOURCE_EXACT_EXPRESSION_GET_TYPE",
                                     "../Grammar/Expression.code");

  NEW_TERMINAL_MACRO(CxxArrayInitReference, "CxxArrayInitReference",
                     "CXX_ARRAY_INIT_REFERENCE");
  CxxArrayInitReference.setFunctionPrototype("HEADER_CXX_ARRAY_INIT_REFERENCE",
                                             "../Grammar/Expression.code");
  CxxArrayInitReference.setDataPrototype(
      "SgCxxArrayInitBinding*", "binding", "= nullptr", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE, COPY_DATA);
  CxxArrayInitReference.setDataPrototype(
      "SgCxxArrayInitReference::reference_kind_enum", "reference_kind",
      "= SgCxxArrayInitReference::e_common_value", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  CxxArrayInitReference.setDataPrototype(
      "SgType*", "expression_type", "= nullptr", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  CxxArrayInitReference.setDataPrototype(
      "SgCastExp::value_category_enum", "value_category",
      "= SgCastExp::e_value_category_unclassified", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  CxxArrayInitReference.setFunctionSource("SOURCE_EXACT_EXPRESSION_GET_TYPE",
                                          "../Grammar/Expression.code");

  NEW_TERMINAL_MACRO(CxxDefaultInitExp, "CxxDefaultInitExp",
                     "CXX_DEFAULT_INIT_EXP");
  CxxDefaultInitExp.setFunctionPrototype("HEADER_CXX_DEFAULT_INIT_EXP",
                                         "../Grammar/Expression.code");
  CxxDefaultInitExp.setDataPrototype("SgInitializedName*", "source_declaration",
                                     "= nullptr", CONSTRUCTOR_PARAMETER,
                                     BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
                                     NO_DELETE, COPY_DATA);
  CxxDefaultInitExp.setDataPrototype(
      "SgExpression*", "expression", "= nullptr", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, CLONE_PTR,
      REQUIRED_TRAVERSAL_MEMBER);
  CxxDefaultInitExp.setDataPrototype(
      "SgCxxDefaultInitExp::source_kind_enum", "source_kind",
      "= SgCxxDefaultInitExp::e_parameter", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  // A Clang source-location builtin invocation. Preserve the builtin as a
  // typed semantic expression rather than replacing it with a guessed value.
  NEW_TERMINAL_MACRO(SourceLocationBuiltinExp, "SourceLocationBuiltinExp",
                     "SOURCE_LOCATION_BUILTIN_EXP");
  SourceLocationBuiltinExp.setFunctionSource(
      "SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
      "../Grammar/Expression.code");
  SourceLocationBuiltinExp.editSubstitute("PRECEDENCE_VALUE", "16");
  SourceLocationBuiltinExp.setFunctionPrototype(
      "HEADER_SOURCE_LOCATION_BUILTIN_EXPRESSION",
      "../Grammar/Expression.code");
  SourceLocationBuiltinExp.setDataPrototype(
      "SgSourceLocationBuiltinExp::source_location_builtin_kind_enum", "kind",
      "= SgSourceLocationBuiltinExp::e_last_source_location_builtin",
      CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  SourceLocationBuiltinExp.setDataPrototype(
      "SgType*", "expression_type", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  SourceLocationBuiltinExp.setFunctionSource("SOURCE_EXACT_EXPRESSION_GET_TYPE",
                                             "../Grammar/Expression.code");

  // A Fortran common-block designator in an OpenMP or OpenACC variable list.
  // Keep the exact use-site spelling separate from the semantic common-block
  // object. Expanding the designator into member variables loses source syntax
  // and changes the clause's meaning.
  NEW_TERMINAL_MACRO(FortranCommonBlockRefExp, "FortranCommonBlockRefExp",
                     "FORTRAN_COMMON_BLOCK_REF_EXP");
  FortranCommonBlockRefExp.setFunctionSource(
      "SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
      "../Grammar/Expression.code");
  FortranCommonBlockRefExp.editSubstitute("PRECEDENCE_VALUE", "16");
  FortranCommonBlockRefExp.setDataPrototype(
      "SgName", "use_name", "= \"\"", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  FortranCommonBlockRefExp.setDataPrototype(
      "SgCommonBlockObject*", "common_block", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  // DQ (7/31/2014): Added support for C++11 nullptr constant value expression
  // (using type nullptr_t).
  NEW_TERMINAL_MACRO(NullptrValExp, "NullptrValExp", "NULLPTR_VAL");
  NullptrValExp.setFunctionSource(
      "SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
      "../Grammar/Expression.code");
  NullptrValExp.setFunctionSource("SOURCE_GET_TYPE_VALUE",
                                  "../Grammar/Expression.code");
  NullptrValExp.editSubstitute("GENERIC_TYPE", "SgTypeNullptr");

  NEW_TERMINAL_MACRO(OrOp, "OrOp", "OR_OP");
  OrOp.setFunctionSource("SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
                         "../Grammar/Expression.code");
  OrOp.editSubstitute("PRECEDENCE_VALUE", " 4");
  OrOp.setFunctionPrototype("HEADER_OR_OPERATOR", "../Grammar/Expression.code");

  NEW_TERMINAL_MACRO(PlusAssignOp, "PlusAssignOp", "PLUS_ASSIGN_OP");
  PlusAssignOp.setFunctionSource(
      "SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
      "../Grammar/Expression.code");
  PlusAssignOp.editSubstitute("PRECEDENCE_VALUE", " 2");

  NEW_TERMINAL_MACRO(PlusPlusOp, "PlusPlusOp", "PLUSPLUS_OP");
  PlusPlusOp.setFunctionSource("SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
                               "../Grammar/Expression.code");
  PlusPlusOp.editSubstitute("PRECEDENCE_VALUE", "15");
  PlusPlusOp.setFunctionPrototype("HEADER_PLUS_PLUS_OPERATOR",
                                  "../Grammar/Expression.code");

  NEW_TERMINAL_MACRO(PntrArrRefExp, "PntrArrRefExp", "ARRAY_OP");
  PntrArrRefExp.setFunctionSource(
      "SOURCE_POINTER_ARRAY_REFERENCE_POST_CONSTRUCTION",
      "../Grammar/Expression.code");
  PntrArrRefExp.setDataPrototype("std::optional<long long>",
                                 "result_storage_size_bytes", "= std::nullopt",
                                 CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                                 NO_TRAVERSAL, NO_DELETE, COPY_DATA);
  PntrArrRefExp.setDataPrototype("std::vector<std::optional<long long>>",
                                 "subscript_stride_bits", "= {}",
                                 CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                                 NO_TRAVERSAL, NO_DELETE, COPY_DATA);
  PntrArrRefExp.editSubstitute("PRECEDENCE_VALUE", "16");
  PntrArrRefExp.setFunctionPrototype(
      "HEADER_POINTER_ARRAY_REFERENCE_EXPRESSION",
      "../Grammar/Expression.code");

  // DQ (1/31/2009): Added Fortran pointer assignment operator (to support
  // pointer assignment statement).
  NEW_TERMINAL_MACRO(PointerAssignOp, "PointerAssignOp", "POINTER_ASSIGN_OP");
  PointerAssignOp.setFunctionSource(
      "SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
      "../Grammar/Expression.code");
  PointerAssignOp.editSubstitute("PRECEDENCE_VALUE", " 2");
  PointerAssignOp.setFunctionPrototype("HEADER_POINTER_ASSIGNMENT_OPERATOR",
                                       "../Grammar/Expression.code");

  NEW_TERMINAL_MACRO(PointerDerefExp, "PointerDerefExp", "DEREF_OP");
  PointerDerefExp.setFunctionSource(
      "SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
      "../Grammar/Expression.code");
  PointerDerefExp.editSubstitute("PRECEDENCE_VALUE", "15");
  PointerDerefExp.setFunctionPrototype("HEADER_POINTER_DEREF_EXPRESSION",
                                       "../Grammar/Expression.code");

  NEW_TERMINAL_MACRO(PseudoDestructorRefExp, "PseudoDestructorRefExp",
                     "PSEUDO_DESTRUCTOR_REF");
  PseudoDestructorRefExp.editSubstitute("PRECEDENCE_VALUE", " 2");
  PseudoDestructorRefExp.setFunctionPrototype("HEADER_PSEUDO_DESTRUCTOR_REF",
                                              "../Grammar/Expression.code");
  PseudoDestructorRefExp.setDataPrototype(
      "SgType*", "object_type", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  PseudoDestructorRefExp.setDataPrototype(
      "SgType*", "expression_type", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  PseudoDestructorRefExp.setDataPrototype(
      "int", "name_qualification_length", "= 0", NO_CONSTRUCTOR_PARAMETER,
      NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  PseudoDestructorRefExp.setDataPrototype(
      "bool", "type_elaboration_required", "= false", NO_CONSTRUCTOR_PARAMETER,
      NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  PseudoDestructorRefExp.setDataPrototype(
      "bool", "global_qualification_required", "= false",
      NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  PseudoDestructorRefExp.setFunctionSource("SOURCE_EXACT_EXPRESSION_GET_TYPE",
                                           "../Grammar/Expression.code");

  // SgRangeExp for representing a range like 1:5 or 1:2:5.
  NEW_TERMINAL_MACRO(RangeExp, "RangeExp", "RANGE_EXP");
  RangeExp.setFunctionSource(
      "SOURCE_RANGE_EXP_POST_CONSTRUCTION_INITIALIZATION",
      "../Grammar/Expression.code");
  RangeExp.setFunctionPrototype("HEADER_RANGE_EXP",
                                "../Grammar/Expression.code");
  RangeExp.setDataPrototype("SgExpression*", "start", "= NULL",
                            CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                            DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
                            REQUIRED_TRAVERSAL_MEMBER);
  RangeExp.setDataPrototype("SgExpression*", "end", "= NULL",
                            CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                            DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
                            REQUIRED_TRAVERSAL_MEMBER);
  RangeExp.setDataPrototype("SgExpression*", "stride", "= NULL",
                            CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                            DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
                            REQUIRED_TRAVERSAL_MEMBER);
  RangeExp.setFunctionSource("SOURCE_SYNTAX_ONLY_GET_TYPE",
                             "../Grammar/Expression.code");

  NEW_TERMINAL_MACRO(RealPartOp, "RealPartOp", "REAL_PART_OP");
  RealPartOp.setFunctionSource("SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
                               "../Grammar/Expression.code");
  RealPartOp.editSubstitute("PRECEDENCE_VALUE", "15");

  NEW_TERMINAL_MACRO(VectorElementExp, "VectorElementExp",
                     "VECTOR_ELEMENT_EXPR");
  VectorElementExp.setFunctionPrototype("HEADER_VECTOR_ELEMENT_EXPRESSION",
                                        "../Grammar/Expression.code");
  VectorElementExp.setFunctionSource(
      "SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
      "../Grammar/Expression.code");
  VectorElementExp.editSubstitute("PRECEDENCE_VALUE", "16");
  VectorElementExp.setDataPrototype(
      "std::vector<unsigned int>", "components", "", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  VectorElementExp.setDataPrototype(
      "SgVectorElementExp::selector_kind_enum", "selector_kind",
      "= SgVectorElementExp::e_components", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  VectorElementExp.setDataPrototype(
      "bool", "is_arrow", "= false", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  NEW_TERMINAL_MACRO(RefExp, "RefExp", "TYPE_REF");
  RefExp.excludeFunctionPrototype("HEADER_GET_TYPE",
                                  "../Grammar/Expression.code");
  RefExp.setFunctionSource("SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
                           "../Grammar/Expression.code");
  RefExp.setDataPrototype("SgType*", "type_name", "= NULL",
                          CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                          NO_TRAVERSAL, NO_DELETE);

  NEW_TERMINAL_MACRO(RshiftAssignOp, "RshiftAssignOp", "RSHIFT_ASSIGN_OP");
  RshiftAssignOp.setFunctionSource(
      "SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
      "../Grammar/Expression.code");
  RshiftAssignOp.editSubstitute("PRECEDENCE_VALUE", " 2");

  NEW_TERMINAL_MACRO(RshiftOp, "RshiftOp", "RSHIFT_OP");
  RshiftOp.setFunctionSource("SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
                             "../Grammar/Expression.code");
  RshiftOp.editSubstitute("PRECEDENCE_VALUE", "11");

  NEW_TERMINAL_MACRO(ScopeOp, "ScopeOp", "SCOPE_OP");
  ScopeOp.setFunctionSource("SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
                            "../Grammar/Expression.code");
  ScopeOp.editSubstitute("PRECEDENCE_VALUE", "17"); // highest precedence
  ScopeOp.setFunctionPrototype("HEADER_SCOPE_OPERATOR",
                               "../Grammar/Expression.code");

  NEW_TERMINAL_MACRO(ShortVal, "ShortVal", "SHORT_VAL");
  ShortVal.setFunctionSource("SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
                             "../Grammar/Expression.code");
  // URK (08/22/2006): Added string to hold source code constants of integer and
  // character types precisely.
  ShortVal.setDataPrototype("short", "value", "= 0", CONSTRUCTOR_PARAMETER,
                            BUILD_LITERAL_VALUE_ACCESS_FUNCTIONS, NO_TRAVERSAL,
                            NO_DELETE);
  ShortVal.setDataPrototype("std::string", "valueString", "= \"\"",
                            CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                            NO_TRAVERSAL, NO_DELETE);
  ShortVal.setFunctionSource("SOURCE_GET_TYPE_VALUE",
                             "../Grammar/Expression.code");
  ShortVal.editSubstitute("GENERIC_TYPE", "SgTypeShort");

  NEW_TERMINAL_MACRO(SizeOfOp, "SizeOfOp", "SIZEOF_OP");
  SizeOfOp.setFunctionSource("SOURCE_UNARY_TYPE_TRAIT_POST_CONSTRUCTION",
                             "../Grammar/Expression.code");
  SizeOfOp.setFunctionPrototype("HEADER_UNARY_TYPE_TRAIT_SEMANTICS",
                                "../Grammar/Expression.code");
  // Now set the precedence values for each leaf of the grammar
  // (where the precedence member function is defined)
  SizeOfOp.editSubstitute("PRECEDENCE_VALUE", "16");
  SizeOfOp.setFunctionPrototype("HEADER_SIZEOF_OPERATOR",
                                "../Grammar/Expression.code");
  // sizeof has exactly one of an expression operand or a type operand.  The
  // expression edge is therefore optional for traversal; SgSizeOfOp::get_type
  // enforces the cross-field XOR contract directly.
  SizeOfOp.setDataPrototype("SgExpression*", "operand_expr", "",
                            CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                            DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
                            OPTIONAL_TRAVERSAL_MEMBER);
  SizeOfOp.setDataPrototype("SgType*", "operand_type", "",
                            CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                            NO_TRAVERSAL, NO_DELETE, COPY_REFERENCE);
  SizeOfOp.setDataPrototype(
      "SgDeclarationStatement*", "type_defining_declaration", "= NULL",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL,
      NO_DELETE, COPY_DATA, OPTIONAL_TRAVERSAL_MEMBER);
  // sizeof and sizeof... have the target's size_t type.  The operand does not
  // carry that ABI fact, so the frontend or transformation producer must
  // supply it exactly.
  SizeOfOp.setDataPrototype("SgType*", "expression_type", "",
                            CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                            NO_TRAVERSAL, NO_DELETE);
  SizeOfOp.setDataPrototype("SgExpression::unary_type_trait_kind_enum",
                            "operator_kind", "= SgExpression::e_sizeof",
                            CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                            NO_TRAVERSAL, NO_DELETE);
  SizeOfOp.setDataPrototype("std::optional<Rose::Integer128Value>",
                            "evaluated_value", "= std::nullopt",
                            CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                            NO_TRAVERSAL, NO_DELETE, COPY_DATA);

  SizeOfOp.setDataPrototype("int", "name_qualification_length", "= 0",
                            NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                            NO_TRAVERSAL, NO_DELETE);

  SizeOfOp.setDataPrototype("bool", "type_elaboration_required", "= false",
                            NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                            NO_TRAVERSAL, NO_DELETE);
  SizeOfOp.setDataPrototype("SgNonrealDecl::source_elaboration_kind_enum",
                            "source_type_elaboration_kind",
                            "= SgNonrealDecl::e_source_elaboration_unspecified",
                            NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                            NO_TRAVERSAL, NO_DELETE);

  SizeOfOp.setDataPrototype("bool", "global_qualification_required", "= false",
                            NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                            NO_TRAVERSAL, NO_DELETE);

  SizeOfOp.setDataPrototype("bool",
                            "is_objectless_nonstatic_data_member_reference",
                            "= false", NO_CONSTRUCTOR_PARAMETER,
                            BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  SizeOfOp.setDataPrototype(
      "int", "name_qualification_for_pointer_to_member_class_length", "= 0",
      NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  SizeOfOp.setDataPrototype(
      "bool", "type_elaboration_for_pointer_to_member_class_required",
      "= false", NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);

  SizeOfOp.setDataPrototype(
      "bool", "global_qualification_for_pointer_to_member_class_required",
      "= false", NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);

  NEW_TERMINAL_MACRO(SpaceshipOp, "SpaceshipOp", "SPACESHIP_OP");
  SpaceshipOp.setFunctionSource("SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
                                "../Grammar/Expression.code");
  // DQ (7/25/2020): Adding C++20 support (need to lookup the correct operator
  // precedence, made it the same as AddOp for now).
  SpaceshipOp.editSubstitute("PRECEDENCE_VALUE", "12");

  NEW_TERMINAL_MACRO(StatementExpression, "StatementExpression", "STMT_EXPR");
  StatementExpression.editSubstitute("PRECEDENCE_VALUE", "16");
  StatementExpression.setFunctionPrototype("HEADER_STATEMENT_EXPRESSION",
                                           "../Grammar/Expression.code");
  StatementExpression.setDataPrototype(
      "SgStatement*", "statement", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER, DIRECT_TRAVERSAL_ACCESS,
      COPY_BEFORE_CONSTRUCTOR_DEPENDENCIES);
  // GNU statement-expression typing includes Clang's void and value-category
  // conversions.  Walking the final Sage statement loses those semantics.
  StatementExpression.setDataPrototype(
      "SgType*", "expression_type", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  NEW_TERMINAL_MACRO(StringVal, "StringVal", "STRING_VAL");
  StringVal.setFunctionPrototype("HEADER_STRING_VALUE_EXPRESSION",
                                 "../Grammar/Expression.code");
  // Store decoded target code units. Source escaping belongs to emission.
  StringVal.setDataPrototype("SgStringCodeUnitList", "value",
                             "= SgStringCodeUnitList{}", CONSTRUCTOR_PARAMETER,
                             NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE,
                             COPY_DATA);
  StringVal.setDataPrototype(
      "SgStringVal::string_literal_encoding_enum", "literal_encoding",
      "= SgStringVal::e_string_encoding_ordinary", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  // Clang 23 represents strings in unevaluated language contexts (for
  // example, static-assert diagnostic messages) as a distinct semantic kind
  // and may normalize away a non-ordinary source prefix.  literal_encoding
  // records the exact lexical prefix, while cxx_unevaluated independently
  // preserves Clang's semantic kind.
  StringVal.setDataPrototype("bool", "cxx_unevaluated", "= false",
                             NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                             NO_TRAVERSAL, NO_DELETE);

  // Fortran FORMAT items retain a quote, Hollerith, or unquoted delimiter.
  StringVal.setDataPrototype("char", "stringDelimiter", "= 0",
                             NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                             NO_TRAVERSAL, NO_DELETE);

  StringVal.setDataPrototype("bool", "isRawString", "= false",
                             NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                             NO_TRAVERSAL, NO_DELETE);
  StringVal.setDataPrototype("std::string", "raw_string_delimiter", "= \"\"",
                             NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                             NO_TRAVERSAL, NO_DELETE);
  StringVal.setDataPrototype("std::string", "raw_string_payload", "= \"\"",
                             NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                             NO_TRAVERSAL, NO_DELETE);
  StringVal.editSubstitute("GENERIC_TYPE", "SgTypeString");

  NEW_TERMINAL_MACRO(SubscriptExpression, "SubscriptExpression",
                     "SUBSCRIPT_EXPR");
  SubscriptExpression.editSubstitute("PRECEDENCE_VALUE", " 2");
  SubscriptExpression.setFunctionPrototype("HEADER_SUBSCRIPT_EXPR_EXPRESSION",
                                           "../Grammar/Expression.code");
  SubscriptExpression.setDataPrototype(
      "SgExpression*", "lowerBound", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);
  SubscriptExpression.setDataPrototype(
      "SgExpression*", "upperBound", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);
  SubscriptExpression.setDataPrototype(
      "SgExpression*", "stride", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);
  NEW_TERMINAL_MACRO(SubtractOp, "SubtractOp", "SUBT_OP");
  SubtractOp.setFunctionSource("SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
                               "../Grammar/Expression.code");
  SubtractOp.editSubstitute("PRECEDENCE_VALUE", "12");

  NEW_TERMINAL_MACRO(TemplateFunctionRefExp, "TemplateFunctionRefExp",
                     "TEMPLATE_FUNCTION_REF");
  TemplateFunctionRefExp.setFunctionPrototype(
      "HEADER_TEMPLATE_FUNCTION_REF_EXPRESSION", "../Grammar/Expression.code");
  TemplateFunctionRefExp.setDataPrototype(
      "SgTemplateFunctionSymbol*", "symbol_i", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  // A deduced template call spells the primary template's source name while
  // semantically selecting a concrete function specialization.  Preserve the
  // selected callable as an explicit non-owning edge on the use site instead
  // of forcing analyses to infer it from the spelling symbol.
  TemplateFunctionRefExp.setDataPrototype(
      "SgFunctionDeclaration*", "semantic_function_declaration", "= NULL",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);
  // The function declaration supplies the function type.

  TemplateFunctionRefExp.setDataPrototype(
      "int", "name_qualification_length", "= 0", NO_CONSTRUCTOR_PARAMETER,
      NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  TemplateFunctionRefExp.setDataPrototype(
      "bool", "type_elaboration_required", "= false", NO_CONSTRUCTOR_PARAMETER,
      NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  TemplateFunctionRefExp.setDataPrototype(
      "bool", "global_qualification_required", "= false",
      NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  TemplateFunctionRefExp.setDataPrototype(
      "int", "explicit_name_qualification_length", "= -1",
      NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  TemplateFunctionRefExp.setDataPrototype(
      "bool", "explicit_global_qualification", "= false",
      NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  TemplateFunctionRefExp.setDataPrototype(
      "SgStringList", "explicit_name_qualification_tokens", "",
      NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  // DQ (12/15/2011): Adding template declaration support to the AST.
  TemplateFunctionRefExp.setFunctionSource(
      "SOURCE_TEMPLATE_FUNCTION_REFERENCE_GET_TYPE",
      "../Grammar/Expression.code");

  NEW_TERMINAL_MACRO(TemplateMemberFunctionRefExp,
                     "TemplateMemberFunctionRefExp",
                     "TEMPLATE_MEMBER_FUNCTION_REF");
  // DQ (12/15/2011) Added template declaration support and so we need support
  // for calling template functions and template member functions as
  // expressions. NOTE: These have a similar interface to the SgFunctionRefExp
  // and SgTemplateMemberFunctionRefExp.
  TemplateMemberFunctionRefExp.setFunctionPrototype(
      "HEADER_TEMPLATE_MEMBER_FUNCTION_REF_EXPRESSION",
      "../Grammar/Expression.code");
  TemplateMemberFunctionRefExp.setDataPrototype(
      "SgTypeExpression*", "conversion_type_id", "= NULL",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL,
      NO_DELETE, COPY_DATA, OPTIONAL_TRAVERSAL_MEMBER);
  TemplateMemberFunctionRefExp.setDataPrototype(
      "SgTemplateMemberFunctionSymbol*", "symbol_i", "= NULL",
      CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  // A deduced member-template call spells the primary member template while
  // selecting a concrete member-function specialization.  Keep those two
  // identities separate, just as SgTemplateFunctionRefExp does for nonmember
  // templates.
  TemplateMemberFunctionRefExp.setDataPrototype(
      "SgMemberFunctionDeclaration*", "semantic_member_function_declaration",
      "= NULL", NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);
  TemplateMemberFunctionRefExp.setDataPrototype(
      "bool", "virtual_call", "= false", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  TemplateMemberFunctionRefExp.setDataPrototype(
      "bool", "need_qualifier", "= true", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  TemplateMemberFunctionRefExp.setDataPrototype(
      "int", "name_qualification_length", "= 0", NO_CONSTRUCTOR_PARAMETER,
      NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  TemplateMemberFunctionRefExp.setDataPrototype(
      "bool", "type_elaboration_required", "= false", NO_CONSTRUCTOR_PARAMETER,
      NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  TemplateMemberFunctionRefExp.setDataPrototype(
      "bool", "global_qualification_required", "= false",
      NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  TemplateMemberFunctionRefExp.setDataPrototype(
      "int", "explicit_name_qualification_length", "= -1",
      NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  TemplateMemberFunctionRefExp.setDataPrototype(
      "bool", "explicit_global_qualification", "= false",
      NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  TemplateMemberFunctionRefExp.setDataPrototype(
      "SgStringList", "explicit_name_qualification_tokens", "",
      NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  TemplateMemberFunctionRefExp.setFunctionSource(
      "SOURCE_TEMPLATE_MEMBER_FUNCTION_REFERENCE_GET_TYPE",
      "../Grammar/Expression.code");

  NEW_TERMINAL_MACRO(TemplateParameterVal, "TemplateParameterVal",
                     "TEMPLATE_PARAMETER_VAL");
  TemplateParameterVal.excludeFunctionPrototype("HEADER_GET_TYPE",
                                                "../Grammar/Expression.code");
  TemplateParameterVal.setFunctionPrototype(
      "HEADER_TEMPLATE_PARAMETER_VALUE_EXPRESSION",
      "../Grammar/Expression.code");
  TemplateParameterVal.setDataPrototype(
      "int", "template_parameter_position", "= -1", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  TemplateParameterVal.setDataPrototype(
      "SgTemplateParameter*", "template_parameter", "= NULL",
      CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE,
      COPY_REFERENCE);

  NEW_TERMINAL_MACRO(ThisExp, "ThisExp", "THIS_NODE");
  ThisExp.setFunctionSource("SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
                            "../Grammar/Expression.code");
  ThisExp.setFunctionPrototype("HEADER_THIS_EXPRESSION",
                               "../Grammar/Expression.code");
  ThisExp.setDataPrototype("SgClassSymbol*", "class_symbol", "",
                           CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                           NO_TRAVERSAL, NO_DELETE);
  ThisExp.setDataPrototype("SgNonrealSymbol*", "nonreal_symbol", "",
                           CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                           NO_TRAVERSAL, NO_DELETE);
  // The pointee cv-qualifiers come from the enclosing member function and are
  // not recoverable from the class symbol.  Store the exact Clang/producer
  // result type instead of synthesizing a fresh unqualified pointer in
  // get_type().
  ThisExp.setDataPrototype("SgType*", "expression_type", "",
                           CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                           NO_TRAVERSAL, NO_DELETE);

  NEW_TERMINAL_MACRO(ThrowOp, "ThrowOp", "THROW_OP");
  ThrowOp.setFunctionSource("SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
                            "../Grammar/Expression.code");
  ThrowOp.editSubstitute("PRECEDENCE_VALUE", "15");
  ThrowOp.setFunctionPrototype("HEADER_THROW_OPERATOR",
                               "../Grammar/Expression.code");
  // A rethrow has no operand.
  ThrowOp.setTraversalCardinalityOverride("operand_i",
                                          OPTIONAL_TRAVERSAL_MEMBER);
  ThrowOp.setDataPrototype("SgThrowOp::e_throw_kind", "throwKind",
                           "= SgThrowOp::unknown_throw", CONSTRUCTOR_PARAMETER,
                           BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  NEW_TERMINAL_MACRO(TypeExpression, "TypeExpression", "TYPE_EXPRESSION");
  TypeExpression.setFunctionPrototype("HEADER_TYPE_EXPRESSION",
                                      "../Grammar/Expression.code");
  TypeExpression.setDataPrototype("SgType*", "represented_type", "= NULL",
                                  CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                                  NO_TRAVERSAL, DELETE_IF_OWNED, CLONE_TREE);
  TypeExpression.setDataPrototype("int", "name_qualification_length", "= 0",
                                  NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                                  NO_TRAVERSAL, NO_DELETE);
  TypeExpression.setDataPrototype("bool", "type_elaboration_required",
                                  "= false", NO_CONSTRUCTOR_PARAMETER,
                                  NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  TypeExpression.setDataPrototype("bool", "global_qualification_required",
                                  "= false", NO_CONSTRUCTOR_PARAMETER,
                                  NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  TypeExpression.setFunctionSource(
      "SOURCE_TYPE_EXPRESSION_POST_CONSTRUCTION_INITIALIZATION",
      "../Grammar/Expression.code");
  TypeExpression.setFunctionSource("SOURCE_SYNTAX_ONLY_GET_TYPE",
                                   "../Grammar/Expression.code");
  TypeExpression.editSubstitute("PRECEDENCE_VALUE", "16");

  NEW_TERMINAL_MACRO(TypeIdOp, "TypeIdOp", "TYPEID_OP");
  TypeIdOp.setFunctionSource(
      "SOURCE_TYPE_ID_OPERATOR_POST_CONSTRUCTION_INITIALIZATION",
      "../Grammar/Expression.code");
  TypeIdOp.editSubstitute("PRECEDENCE_VALUE", "16");
  TypeIdOp.setFunctionPrototype("HEADER_TYPE_ID_OPERATOR",
                                "../Grammar/Expression.code");
  TypeIdOp.setDataPrototype("SgExpression*", "operand_expr", "",
                            CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                            DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
                            OPTIONAL_TRAVERSAL_MEMBER);
  TypeIdOp.setDataPrototype("SgType*", "operand_type", "",
                            CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                            NO_TRAVERSAL, NO_DELETE);
  // Store the typeid expression result type explicitly (const std::type_info&).
  // This avoids name-based lookups and does not depend on operand typing.
  TypeIdOp.setDataPrototype("SgType*", "expression_type", "",
                            CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                            NO_TRAVERSAL, NO_DELETE);
  TypeIdOp.setDataPrototype("bool", "operand_is_evaluated", "",
                            CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                            NO_TRAVERSAL, NO_DELETE, COPY_DATA);

  TypeIdOp.setDataPrototype("int", "name_qualification_length", "= 0",
                            NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                            NO_TRAVERSAL, NO_DELETE);

  TypeIdOp.setDataPrototype("bool", "type_elaboration_required", "= false",
                            NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                            NO_TRAVERSAL, NO_DELETE);

  TypeIdOp.setDataPrototype("bool", "global_qualification_required", "= false",
                            NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                            NO_TRAVERSAL, NO_DELETE);

  TypeIdOp.setDataPrototype(
      "int", "name_qualification_for_pointer_to_member_class_length", "= 0",
      NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  TypeIdOp.setDataPrototype(
      "bool", "type_elaboration_for_pointer_to_member_class_required",
      "= false", NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);

  TypeIdOp.setDataPrototype(
      "bool", "global_qualification_for_pointer_to_member_class_required",
      "= false", NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);

  NEW_TERMINAL_MACRO(TypeTraitBuiltinOperator, "TypeTraitBuiltinOperator",
                     "TYPE_TRAIT_BUILTIN_OPERATOR");
  TypeTraitBuiltinOperator.setFunctionPrototype(
      "HEADER_TYPE_TRAIT_BUILTIN_OPERATOR", "../Grammar/Expression.code");
  TypeTraitBuiltinOperator.setDataPrototype(
      "SgName", "name", "= \"\"", CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
      NO_TRAVERSAL, NO_DELETE);
  TypeTraitBuiltinOperator.setDataPrototype(
      "SgTypeTraitBuiltinOperator::builtin_operator_kind_enum",
      "builtin_operator_kind",
      "= SgTypeTraitBuiltinOperator::e_type_trait_builtin",
      CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  TypeTraitBuiltinOperator.setDataPrototype(
      "SgType*", "expression_type", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  TypeTraitBuiltinOperator.setDataPrototype(
      "SgExpressionPtrList", "args", "", NO_CONSTRUCTOR_PARAMETER,
      BUILD_LIST_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE);

  NEW_TERMINAL_MACRO(UnaryAddOp, "UnaryAddOp", "UNARY_ADD_OP");
  UnaryAddOp.setFunctionSource("SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
                               "../Grammar/Expression.code");
  UnaryAddOp.editSubstitute("PRECEDENCE_VALUE", "15");

  // PL (1/21/2025): Despite C++ not having a signed char literal (only char and
  // unsigned char literals), signed chars can come out of the frontend constant
  // folding. For example, an expression like `(signed char)0` can get constant
  // folded into a signed char literal with value 0, despite there not being a
  // syntactic construct in C++ that can directly generate such a node. We don't
  // want to just emit an SgCharVal when encountering this, because the C++
  // standard does not specify the signedness of the native `char` type. We need
  // to emit an SgSignedCharVal to enforce signedness.
  //
  // This solution was discussed with and agreed by DQ.
  NEW_TERMINAL_MACRO(SignedCharVal, "SignedCharVal", "SIGNED_CHAR_VAL");
  SignedCharVal.setFunctionSource(
      "SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
      "../Grammar/Expression.code");
  SignedCharVal.setDataPrototype(
      "signed char", "value", "= 0", CONSTRUCTOR_PARAMETER,
      BUILD_LITERAL_VALUE_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  SignedCharVal.setDataPrototype("std::string", "valueString", "= \"\"",
                                 CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                                 NO_TRAVERSAL, NO_DELETE);
  SignedCharVal.setFunctionSource("SOURCE_GET_TYPE_VALUE",
                                  "../Grammar/Expression.code");
  SignedCharVal.editSubstitute("GENERIC_TYPE", "SgTypeSignedChar");

  NEW_TERMINAL_MACRO(UnsignedCharVal, "UnsignedCharVal", "UNSIGNED_CHAR_VAL");
  UnsignedCharVal.setFunctionSource(
      "SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
      "../Grammar/Expression.code");
  UnsignedCharVal.setDataPrototype(
      "unsigned char", "value", "= 0", CONSTRUCTOR_PARAMETER,
      BUILD_LITERAL_VALUE_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  UnsignedCharVal.setDataPrototype(
      "std::string", "valueString", "= \"\"", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  UnsignedCharVal.setFunctionSource("SOURCE_GET_TYPE_VALUE",
                                    "../Grammar/Expression.code");
  UnsignedCharVal.editSubstitute("GENERIC_TYPE", "SgTypeUnsignedChar");

  NEW_TERMINAL_MACRO(UnsignedIntVal, "UnsignedIntVal", "UNSIGNED_INT_VAL");
  UnsignedIntVal.setFunctionSource(
      "SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
      "../Grammar/Expression.code");
  UnsignedIntVal.setDataPrototype(
      "unsigned int", "value", "= 0", CONSTRUCTOR_PARAMETER,
      BUILD_LITERAL_VALUE_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  UnsignedIntVal.setDataPrototype("std::string", "valueString", "= \"\"",
                                  CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                                  NO_TRAVERSAL, NO_DELETE);
  UnsignedIntVal.setFunctionSource("SOURCE_GET_TYPE_VALUE",
                                   "../Grammar/Expression.code");
  UnsignedIntVal.editSubstitute("GENERIC_TYPE", "SgTypeUnsignedInt");

  NEW_TERMINAL_MACRO(UnsignedLongLongIntVal, "UnsignedLongLongIntVal",
                     "UNSIGNED_LONG_LONG_INT_VAL");
  UnsignedLongLongIntVal.setFunctionSource(
      "SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
      "../Grammar/Expression.code");
  UnsignedLongLongIntVal.setDataPrototype(
      "unsigned long long int", "value", "= 0", CONSTRUCTOR_PARAMETER,
      BUILD_LITERAL_VALUE_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  UnsignedLongLongIntVal.setDataPrototype(
      "std::string", "valueString", "= \"\"", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  UnsignedLongLongIntVal.setFunctionSource("SOURCE_GET_TYPE_VALUE",
                                           "../Grammar/Expression.code");
  UnsignedLongLongIntVal.editSubstitute("GENERIC_TYPE",
                                        "SgTypeUnsignedLongLong");

  NEW_TERMINAL_MACRO(UnsignedLongVal, "UnsignedLongVal",
                     "UNSIGNED_LONG_INT_VAL");
  UnsignedLongVal.setFunctionSource(
      "SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
      "../Grammar/Expression.code");
  UnsignedLongVal.setDataPrototype(
      "unsigned long", "value", "= 0", CONSTRUCTOR_PARAMETER,
      BUILD_LITERAL_VALUE_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  UnsignedLongVal.setDataPrototype(
      "std::string", "valueString", "= \"\"", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  UnsignedLongVal.setFunctionSource("SOURCE_GET_TYPE_VALUE",
                                    "../Grammar/Expression.code");
  UnsignedLongVal.editSubstitute("GENERIC_TYPE", "SgTypeUnsignedLong");

  NEW_TERMINAL_MACRO(UnsignedShortVal, "UnsignedShortVal",
                     "UNSIGNED_SHORT_VAL");
  UnsignedShortVal.setFunctionSource(
      "SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
      "../Grammar/Expression.code");
  UnsignedShortVal.setDataPrototype(
      "unsigned short", "value", "= 0", CONSTRUCTOR_PARAMETER,
      BUILD_LITERAL_VALUE_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  UnsignedShortVal.setDataPrototype(
      "std::string", "valueString", "= \"\"", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  UnsignedShortVal.setFunctionSource("SOURCE_GET_TYPE_VALUE",
                                     "../Grammar/Expression.code");
  UnsignedShortVal.editSubstitute("GENERIC_TYPE", "SgTypeUnsignedShort");

  // User defined operator for Fortran named operators.
  NEW_TERMINAL_MACRO(UserDefinedBinaryOp, "UserDefinedBinaryOp",
                     "USER_DEFINED_BINARY_OP");
  UserDefinedBinaryOp.setFunctionSource(
      "SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
      "../Grammar/Expression.code");
  UserDefinedBinaryOp.editSubstitute("PRECEDENCE_VALUE", " 2");
  UserDefinedBinaryOp.setFunctionPrototype(
      "HEADER_USER_DEFINED_BINARY_EXPRESSION", "../Grammar/Expression.code");
  UserDefinedBinaryOp.setDataPrototype(
      "SgName", "operator_name", "= \"\"", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  UserDefinedBinaryOp.setDataPrototype(
      "SgFunctionSymbol*", "symbol", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  // User defined operator for Fortran named operators.
  NEW_TERMINAL_MACRO(UserDefinedUnaryOp, "UserDefinedUnaryOp",
                     "USER_DEFINED_UNARY_OP");
  UserDefinedUnaryOp.setFunctionSource(
      "SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
      "../Grammar/Expression.code");
  UserDefinedUnaryOp.editSubstitute("PRECEDENCE_VALUE", " 2");
  UserDefinedUnaryOp.setFunctionPrototype(
      "HEADER_USER_DEFINED_UNARY_EXPRESSION", "../Grammar/Expression.code");
  UserDefinedUnaryOp.setDataPrototype(
      "SgName", "operator_name", "= \"\"", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  UserDefinedUnaryOp.setDataPrototype(
      "SgFunctionSymbol*", "symbol", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  NEW_TERMINAL_MACRO(VarArgCopyOp, "VarArgCopyOp", "VA_COPY_OP");
  VarArgCopyOp.setFunctionSource(
      "SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
      "../Grammar/Expression.code");
  VarArgCopyOp.editSubstitute("PRECEDENCE_VALUE", "16");
  VarArgCopyOp.setDataPrototype("SgExpression*", "lhs_operand", "= NULL",
                                CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                                DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
                                OPTIONAL_TRAVERSAL_MEMBER);
  VarArgCopyOp.setDataPrototype("SgExpression*", "rhs_operand", "= NULL",
                                CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                                DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
                                OPTIONAL_TRAVERSAL_MEMBER);
  VarArgCopyOp.setDataPrototype("SgType*", "expression_type", "= NULL",
                                CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                                NO_TRAVERSAL, NO_DELETE);
  VarArgCopyOp.setFunctionSource("SOURCE_EXACT_EXPRESSION_GET_TYPE",
                                 "../Grammar/Expression.code");

  NEW_TERMINAL_MACRO(VarArgEndOp, "VarArgEndOp", "VA_END_OP");
  VarArgEndOp.setFunctionSource("SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
                                "../Grammar/Expression.code");
  VarArgEndOp.editSubstitute("PRECEDENCE_VALUE", "16");
  VarArgEndOp.setFunctionPrototype("HEADER_VARARG_END_OPERATOR",
                                   "../Grammar/Expression.code");
  VarArgEndOp.setDataPrototype("SgExpression*", "operand_expr", "= NULL",
                               CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                               DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
                               OPTIONAL_TRAVERSAL_MEMBER);
  VarArgEndOp.setDataPrototype("SgType*", "expression_type", "= NULL",
                               CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                               NO_TRAVERSAL, NO_DELETE);
  VarArgEndOp.setFunctionSource("SOURCE_EXACT_EXPRESSION_GET_TYPE",
                                "../Grammar/Expression.code");

  NEW_TERMINAL_MACRO(VarArgOp, "VarArgOp", "VA_OP");
  VarArgOp.setFunctionSource("SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
                             "../Grammar/Expression.code");
  VarArgOp.editSubstitute("PRECEDENCE_VALUE", "16");
  VarArgOp.setFunctionPrototype("HEADER_VARARG_OPERATOR",
                                "../Grammar/Expression.code");
  VarArgOp.setDataPrototype("SgExpression*", "operand_expr", "= NULL",
                            CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                            DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
                            OPTIONAL_TRAVERSAL_MEMBER);
  VarArgOp.setDataPrototype("SgType*", "expression_type", "= NULL",
                            CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                            NO_TRAVERSAL, NO_DELETE);
  VarArgOp.setDataPrototype("int", "name_qualification_length", "= 0",
                            NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                            NO_TRAVERSAL, NO_DELETE);
  VarArgOp.setDataPrototype("bool", "type_elaboration_required", "= false",
                            NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                            NO_TRAVERSAL, NO_DELETE);
  VarArgOp.setDataPrototype("bool", "global_qualification_required", "= false",
                            NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                            NO_TRAVERSAL, NO_DELETE);
  VarArgOp.setDataPrototype(
      "int", "name_qualification_for_pointer_to_member_class_length", "= 0",
      NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  VarArgOp.setDataPrototype(
      "bool", "type_elaboration_for_pointer_to_member_class_required",
      "= false", NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);
  VarArgOp.setDataPrototype(
      "bool", "global_qualification_for_pointer_to_member_class_required",
      "= false", NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);
  NEW_TERMINAL_MACRO(VarArgStartOneOperandOp, "VarArgStartOneOperandOp",
                     "VA_START_ONE_OPERAND_OP");
  VarArgStartOneOperandOp.setFunctionSource(
      "SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
      "../Grammar/Expression.code");
  VarArgStartOneOperandOp.editSubstitute("PRECEDENCE_VALUE", "16");
  VarArgStartOneOperandOp.setDataPrototype(
      "SgExpression*", "operand_expr", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
      OPTIONAL_TRAVERSAL_MEMBER);
  VarArgStartOneOperandOp.setDataPrototype(
      "SgType*", "expression_type", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  VarArgStartOneOperandOp.setFunctionSource("SOURCE_EXACT_EXPRESSION_GET_TYPE",
                                            "../Grammar/Expression.code");

  NEW_TERMINAL_MACRO(VarArgStartOp, "VarArgStartOp", "VA_START_OP");
  VarArgStartOp.setFunctionSource(
      "SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
      "../Grammar/Expression.code");
  VarArgStartOp.editSubstitute("PRECEDENCE_VALUE", "16");
  VarArgStartOp.setFunctionPrototype("HEADER_VARARG_START_OPERATOR",
                                     "../Grammar/Expression.code");
  VarArgStartOp.setDataPrototype("SgExpression*", "lhs_operand", "= NULL",
                                 CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                                 DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
                                 OPTIONAL_TRAVERSAL_MEMBER);
  VarArgStartOp.setDataPrototype("SgExpression*", "rhs_operand", "= NULL",
                                 CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                                 DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
                                 OPTIONAL_TRAVERSAL_MEMBER);
  VarArgStartOp.setDataPrototype("SgType*", "expression_type", "= NULL",
                                 CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                                 NO_TRAVERSAL, NO_DELETE);
  VarArgStartOp.setFunctionSource("SOURCE_EXACT_EXPRESSION_GET_TYPE",
                                  "../Grammar/Expression.code");

  NEW_TERMINAL_MACRO(VariantExpression, "VariantExpression", "VARIANT_EXPR");
  VariantExpression.setFunctionSource(
      "SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
      "../Grammar/Expression.code");
  VariantExpression.editSubstitute("PRECEDENCE_VALUE", "16");
  VariantExpression.setDataPrototype(
      "SgType*", "expression_type", "", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  VariantExpression.setFunctionSource("SOURCE_EXACT_EXPRESSION_GET_TYPE",
                                      "../Grammar/Expression.code");

  // This function sets up the type system for the grammar.  In this case it
  // implements the C++ grammar, but this will be modified to permit all
  // grammars to contain elements of the C++ grammar.  Modified grammars will
  // add and subtract elements from this default C++ grammar.
  NEW_TERMINAL_MACRO(VarRefExp, "VarRefExp", "VAR_REF");
  // DQ (1/14/2006): We should be using
  // SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION instead of
  // SOURCE_POST_CONSTRUCTION_INITIALIZATION_USING_SET_TYPE since we don't want
  // to have a set_type function on certain types of expressions (because the
  // type should be computed from the operands or the value types directly).
  VarRefExp.setFunctionSource("SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
                              "../Grammar/Expression.code");
  VarRefExp.setFunctionPrototype("HEADER_VAR_REF_EXPRESSION",
                                 "../Grammar/Expression.code");

  // Changing the symbol must not mark the AST as modified.
  VarRefExp.setDataPrototype("SgVariableSymbol*", "symbol", "= NULL",
                             CONSTRUCTOR_PARAMETER, BUILD_FLAG_ACCESS_FUNCTIONS,
                             NO_TRAVERSAL, NO_DELETE,
                             COPY_CONSTRUCTOR_REFERENCE);
  VarRefExp.setDataPrototype("bool", "is_non_odr_use", "= false",
                             CONSTRUCTOR_PARAMETER, BUILD_FLAG_ACCESS_FUNCTIONS,
                             NO_TRAVERSAL, NO_DELETE);

  VarRefExp.setDataPrototype("int", "name_qualification_length", "= 0",
                             NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                             NO_TRAVERSAL, NO_DELETE);

  VarRefExp.setDataPrototype("bool", "type_elaboration_required", "= false",
                             NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                             NO_TRAVERSAL, NO_DELETE);

  VarRefExp.setDataPrototype("bool", "global_qualification_required", "= false",
                             NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                             NO_TRAVERSAL, NO_DELETE);
  VarRefExp.setDataPrototype("int", "explicit_name_qualification_length",
                             "= -1", NO_CONSTRUCTOR_PARAMETER,
                             NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  VarRefExp.setDataPrototype("bool", "explicit_global_qualification", "= false",
                             NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                             NO_TRAVERSAL, NO_DELETE);
  VarRefExp.setDataPrototype(
      "SgStringList", "explicit_name_qualification_tokens", "",
      NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  VarRefExp.setFunctionSource("SOURCE_GET_TYPE_FROM_SYMBOL",
                              "../Grammar/Expression.code");

  NEW_TERMINAL_MACRO(VoidVal, "VoidVal", "VOID_VAL");
  VoidVal.setFunctionSource("SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
                            "../Grammar/Expression.code");
  VoidVal.setFunctionSource("SOURCE_GET_TYPE_VALUE",
                            "../Grammar/Expression.code");
  VoidVal.editSubstitute("GENERIC_TYPE", "SgTypeVoid");

  NEW_TERMINAL_MACRO(WcharVal, "WcharVal", "WCHAR_VAL");
  WcharVal.setFunctionSource("SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
                             "../Grammar/Expression.code");
  WcharVal.setDataPrototype("int64_t", "value", "= 0", CONSTRUCTOR_PARAMETER,
                            BUILD_LITERAL_VALUE_ACCESS_FUNCTIONS, NO_TRAVERSAL,
                            NO_DELETE);
  WcharVal.setDataPrototype("std::string", "valueString", "= \"\"",
                            CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                            NO_TRAVERSAL, NO_DELETE);
  WcharVal.setFunctionSource("SOURCE_GET_TYPE_VALUE",
                             "../Grammar/Expression.code");
  WcharVal.editSubstitute("GENERIC_TYPE", "SgTypeWchar");

  NEW_TERMINAL_MACRO(XorAssignOp, "XorAssignOp", "XOR_ASSIGN_OP");
  XorAssignOp.setFunctionSource("SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
                                "../Grammar/Expression.code");
  XorAssignOp.editSubstitute("PRECEDENCE_VALUE", " 2");

  // SIMD operators
  NEW_TERMINAL_MACRO(SIMDLoad, "SIMDLoad", "SIMD_LOAD");
  NEW_TERMINAL_MACRO(SIMDBroadcast, "SIMDBroadcast", "SIMD_BROADCAST");
  NEW_TERMINAL_MACRO(SIMDStore, "SIMDStore", "SIMD_STORE");
  NEW_TERMINAL_MACRO(SIMDPartialStore, "SIMDPartialStore",
                     "SIMD_PARTIAL_STORE");
  NEW_TERMINAL_MACRO(SIMDScalarStore, "SIMDScalarStore", "SIMD_SCALAR_STORE");

  NEW_TERMINAL_MACRO(SIMDGather, "SIMDGather", "SIMD_GATHER");
  NEW_TERMINAL_MACRO(SIMDExplicitGather, "SIMDExplicitGather",
                     "SIMD_EXPLICIT_GATHER");
  NEW_TERMINAL_MACRO(SIMDScatter, "SIMDScatter", "SIMD_SCATTER");

  NEW_TERMINAL_MACRO(SIMDAddOp, "SIMDAddOp", "SIMD_ADD_OP");
  NEW_TERMINAL_MACRO(SIMDSubOp, "SIMDSubOp", "SIMD_SUB_OP");
  NEW_TERMINAL_MACRO(SIMDMulOp, "SIMDMulOp", "SIMD_MUL_OP");
  NEW_TERMINAL_MACRO(SIMDDivOp, "SIMDDivOp", "SIMD_DIV_OP");
  NEW_TERMINAL_MACRO(SIMDFmaOp, "SIMDFmaOp", "SIMD_FMA_OP");

  NEW_NONTERMINAL_MACRO(
      SIMDBinaryOp, SIMDAddOp | SIMDSubOp | SIMDMulOp | SIMDDivOp | SIMDFmaOp,
      "SIMDBinaryOp", "SIMD_BINARY_OP", false);

  SIMDBinaryOp.editSubstitute("PRECEDENCE_VALUE", "16");
  SIMDAddOp.editSubstitute("PRECEDENCE_VALUE", "16");
  SIMDSubOp.editSubstitute("PRECEDENCE_VALUE", "16");
  SIMDMulOp.editSubstitute("PRECEDENCE_VALUE", "16");
  SIMDDivOp.editSubstitute("PRECEDENCE_VALUE", "16");
  SIMDFmaOp.editSubstitute("PRECEDENCE_VALUE", "16");
  SIMDLoad.editSubstitute("PRECEDENCE_VALUE", "16");
  SIMDBroadcast.editSubstitute("PRECEDENCE_VALUE", "16");
  SIMDStore.editSubstitute("PRECEDENCE_VALUE", "16");
  SIMDPartialStore.editSubstitute("PRECEDENCE_VALUE", "16");
  SIMDScalarStore.editSubstitute("PRECEDENCE_VALUE", "16");
  SIMDGather.editSubstitute("PRECEDENCE_VALUE", "16");
  SIMDExplicitGather.editSubstitute("PRECEDENCE_VALUE", "16");
  SIMDScatter.editSubstitute("PRECEDENCE_VALUE", "16");

  //                                  Nonterminal classes in reverse
  //                                  hierarchical order

  NEW_NONTERMINAL_MACRO(
      CompoundAssignOp,
      PlusAssignOp | MinusAssignOp | AndAssignOp | IorAssignOp | MultAssignOp |
          DivAssignOp | ModAssignOp | XorAssignOp | LshiftAssignOp |
          RshiftAssignOp | IntegerDivideAssignOp | ExponentiationAssignOp,
      "CompoundAssignOp", "COMPOUND_ASSIGN_OP", false);
  CompoundAssignOp.setFunctionSource(
      "SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
      "../Grammar/Expression.code");
  CompoundAssignOp.excludeFunctionPrototype("HEADER_PRECEDENCE",
                                            "../Grammar/Expression.code");
  CompoundAssignOp.setFunctionPrototype("HEADER_COMPOUND_ASSIGNMENT_OPERATOR",
                                        "../Grammar/Expression.code");

  NEW_NONTERMINAL_MACRO(MemberAccessExp,
                        ArrowExp | DotExp | DotStarOp | ArrowStarOp,
                        "MemberAccessExp", "MEMBER_ACCESS_EXP", false);
  MemberAccessExp.setFunctionSource(
      "SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
      "../Grammar/Expression.code");
  MemberAccessExp.editSubstitute("PRECEDENCE_VALUE", "16");

  NEW_NONTERMINAL_MACRO(
      BinaryOp,
      MemberAccessExp | EqualityOp | LessThanOp | GreaterThanOp | NotEqualOp |
          LessOrEqualOp | GreaterOrEqualOp | AddOp | SubtractOp | MultiplyOp |
          DivideOp | IntegerDivideOp | ModOp | AndOp | OrOp | BitXorOp |
          BitAndOp | BitOrOp | BitEqvOp | CommaOpExp | LshiftOp | RshiftOp |
          PntrArrRefExp | ScopeOp | AssignOp | ExponentiationOp |
          ConcatenationOp | PointerAssignOp | UserDefinedBinaryOp |
          CompoundAssignOp | SpaceshipOp | SIMDBinaryOp | SIMDLoad |
          SIMDBroadcast | SIMDStore | SIMDPartialStore | SIMDScalarStore |
          SIMDGather | SIMDExplicitGather | SIMDScatter,
      "BinaryOp", "BINARY_EXPRESSION", false);
  BinaryOp.setFunctionPrototype("HEADER_EXTRA_FUNCTIONS",
                                "../Grammar/Expression.code");
  BinaryOp.excludeFunctionPrototype("HEADER_GET_TYPE",
                                    "../Grammar/Expression.code");
  BinaryOp.excludeSubTreeFunctionPrototype("HEADER_GET_TYPE",
                                           "../Grammar/Expression.code");
  BinaryOp.setSubTreeFunctionPrototype("HEADER_PRECEDENCE",
                                       "../Grammar/Expression.code");
  BinaryOp.excludeFunctionPrototype("HEADER_PRECEDENCE",
                                    "../Grammar/Expression.code");
  BinaryOp.setFunctionPrototype("HEADER_GET_NEXT_EXPRESSION",
                                "../Grammar/Expression.code");
  BinaryOp.setFunctionPrototype("HEADER_BINARY_EXPRESSION",
                                "../Grammar/Expression.code");
  BinaryOp.setDataPrototype("SgExpression*", "lhs_operand_i", "",
                            CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                            DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
                            REQUIRED_TRAVERSAL_MEMBER);
  BinaryOp.setDataPrototype("SgExpression*", "rhs_operand_i", "",
                            CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                            DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
                            REQUIRED_TRAVERSAL_MEMBER);
  // Every semantic binary expression owns its exact frontend-computed result
  // type.  Operand types are insufficient: promotions, comparisons, pointer
  // subtraction, member-pointer application, and language rules can all change
  // the result independently of either operand.
  BinaryOp.setDataPrototype("SgType*", "expression_type", "",
                            CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                            NO_TRAVERSAL, NO_DELETE);

  // TV (06/06/13) : CudaKernelCall are now considered to be a FunctionCall
  NEW_NONTERMINAL_MACRO(FunctionCallExp, CudaKernelCallExp, "FunctionCallExp",
                        "FUNC_CALL", true);
  FunctionCallExp.setFunctionPrototype("HEADER_FUNCTION_CALL_EXPRESSION",
                                       "../Grammar/Expression.code");
  // Support specification of operator vs.
  // non-operator syntax ("x+y" instead of "operator+(x,y)"). This is relevant
  // for generated code in some cases (has different function evaluation rules).
  // See test2013_100.C. ROSE has historically defaulted to using the operator
  // syntax ("x+y") in generated code, but sometimes this is an error as
  // test2013_100.C demonstrates.
  FunctionCallExp.setDataPrototype(
      "bool", "uses_operator_syntax", "= false", NO_CONSTRUCTOR_PARAMETER,
      NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  FunctionCallExp.setDataPrototype(
      "SgFunctionCallExp::source_syntax_enum", "source_syntax",
      "= SgFunctionCallExp::e_source_function_call", NO_CONSTRUCTOR_PARAMETER,
      NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  FunctionCallExp.setDataPrototype(
      "SgFunctionCallExp::source_operator_surface_enum",
      "source_operator_surface", "= SgFunctionCallExp::e_no_operator_surface",
      NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  FunctionCallExp.setDataPrototype(
      "SgFunctionCallExp::source_operator_callee_form_enum",
      "source_operator_callee_form",
      "= SgFunctionCallExp::e_no_operator_callee_form",
      NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  FunctionCallExp.setDataPrototype(
      "SgUnsignedCharList", "source_operator_operand_roles", "",
      NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  // A source user-defined literal is not an ordinary operator call.  Preserve
  // its exact identifier token on the call rather than reconstructing the
  // suffix from a function declaration name in the backend.  An empty suffix
  // means that this call was not written with literal syntax.
  FunctionCallExp.setDataPrototype(
      "SgName", "source_user_defined_literal_suffix", "= \"\"",
      NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  // Source operands of a UDL are lexical syntax, not the semantic parameters
  // passed to its literal-operator function.  Keep them under a separate edge
  // so raw and template UDLs do not corrupt the semantic call arity.
  FunctionCallExp.setDataPrototype(
      "SgCxxLiteralTokenList*", "source_user_defined_literal_operands",
      "= NULL", NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, DEF_TRAVERSAL,
      NO_DELETE, COPY_DATA, OPTIONAL_TRAVERSAL_MEMBER);
  FunctionCallExp.setDataPrototype(
      "SgUnsignedCharList", "source_user_defined_literal_suffix_roles", "",
      NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  FunctionCallExp.setDataPrototype(
      "uint64_t", "source_user_defined_literal_sequence", "= 0",
      NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  FunctionCallExp.setDataPrototype(
      "bool", "source_user_defined_literal_was_modified", "= false",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);

  //               x.init(1,2,3) and init(x, 1, 2, 4)
  //               the flag indicates if the call was parsed/ should be unparsed
  //               using the first argument as prefix notation.
  FunctionCallExp.setDataPrototype(
      "bool", "usesObjectCallSyntax", "= false", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  NEW_NONTERMINAL_MACRO(CallExpression, FunctionCallExp, "CallExpression",
                        "CALL_EXPRESSION", true);
  CallExpression.setFunctionPrototype("HEADER_GET_NEXT_EXPRESSION",
                                      "../Grammar/Expression.code");
  CallExpression.setFunctionPrototype("HEADER_CALL_EXPRESSION",
                                      "../Grammar/Expression.code");
  CallExpression.editSubstitute("HEADER_LIST_DECLARATIONS",
                                "HEADER_LIST_FUNCTIONS",
                                "../Grammar/Expression.code");
  CallExpression.editSubstitute("LIST_NAME", "arg");
  CallExpression.setDataPrototype("SgExpression*", "function", "= NULL",
                                  CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                                  DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
                                  OPTIONAL_TRAVERSAL_MEMBER);
  CallExpression.setDataPrototype("SgExprListExp*", "args", "= NULL",
                                  CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                                  DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
                                  OPTIONAL_TRAVERSAL_MEMBER);

  CallExpression.setDataPrototype("SgType*", "expression_type", "= NULL",
                                  CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                                  NO_TRAVERSAL, NO_DELETE);

  NEW_TERMINAL_MACRO(ExprListExp, "ExprListExp", "EXPR_LIST");
  ExprListExp.setFunctionSource("SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
                                "../Grammar/Expression.code");
  ExprListExp.setFunctionPrototype("HEADER_EXPRESSION_LIST_EXPRESSION",
                                   "../Grammar/Expression.code");

  // MK: I moved the following data member declaration from
  // ../Grammar/Expression.code to this position:
  ExprListExp.setDataPrototype("SgExpressionPtrList", "expressions", "",
                               NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                               DEF_TRAVERSAL, NO_DELETE);
  ExprListExp.setDataPrototype(
      "source_form_enum", "source_form", "= e_expression_list_plain",
      NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  ExprListExp.editSubstitute("HEADER_LIST_DECLARATIONS",
                             "HEADER_LIST_FUNCTIONS",
                             "../Grammar/Expression.code");
  ExprListExp.editSubstitute("LIST_NAME", "expression");
  ExprListExp.setFunctionSource("SOURCE_SYNTAX_ONLY_GET_TYPE",
                                "../Grammar/Expression.code");

  NEW_TERMINAL_MACRO(FortranDataInitializer, "FortranDataInitializer",
                     "FORTRAN_DATA_INIT");
  FortranDataInitializer.setFunctionPrototype("HEADER_FORTRAN_DATA_INITIALIZER",
                                              "../Grammar/Expression.code");
  FortranDataInitializer.setFunctionSource("SOURCE_FORTRAN_DATA_INITIALIZER",
                                           "../Grammar/Expression.code");
  FortranDataInitializer.setDataPrototype(
      "SgDataStatementValuePtrList", "values", "", CONSTRUCTOR_PARAMETER,
      NO_ACCESS_FUNCTIONS, DEF_TRAVERSAL, NO_DELETE, CLONE_TREE);
  FortranDataInitializer.setDataPrototype(
      "SgType*", "expression_type", "", CONSTRUCTOR_PARAMETER,
      NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE, COPY_DATA);
  NEW_NONTERMINAL_MACRO(Initializer,
                        AggregateInitializer | ConstructorInitializer |
                            AssignInitializer | DesignatedInitializer |
                            BracedInitializer | FortranDataInitializer,
                        "Initializer", "EXPR_INIT", false);
  Initializer.excludeFunctionPrototype("HEADER_GET_TYPE",
                                       "../Grammar/Expression.code");
  Initializer.setFunctionSource("SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
                                "../Grammar/Expression.code");
  Initializer.setSubTreeFunctionPrototype("HEADER_GET_NEXT_EXPRESSION",
                                          "../Grammar/Expression.code");
  Initializer.excludeFunctionPrototype("HEADER_GET_NEXT_EXPRESSION",
                                       "../Grammar/Expression.code");
  FortranDataInitializer.excludeFunctionPrototype("HEADER_GET_NEXT_EXPRESSION",
                                                  "../Grammar/Expression.code");
  Initializer.setFunctionPrototype("HEADER_INITIALIZER_EXPRESSION",
                                   "../Grammar/Expression.code");
  // DQ (11/6/2014): This is C++11 syntax for direct brace initalization (e.g.
  // int n{}).
  Initializer.setDataPrototype("bool", "is_braced_initialized", "= false",
                               NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                               NO_TRAVERSAL, NO_DELETE);

  NEW_NONTERMINAL_MACRO(UnaryOp,
                        ExpressionRoot | MinusOp | UnaryAddOp | NotOp |
                            PointerDerefExp | AddressOfOp | MinusMinusOp |
                            PlusPlusOp | BitComplementOp | CastExp | ThrowOp |
                            RealPartOp | ImagPartOp | ConjugateOp |
                            UserDefinedUnaryOp | VectorElementExp,
                        "UnaryOp", "UNARY_EXPRESSION", false);
  UnaryOp.setFunctionPrototype("HEADER_EXTRA_FUNCTIONS",
                               "../Grammar/Expression.code");
  // get_type is included directly within UnaryOp and BinaryOp
  UnaryOp.excludeFunctionPrototype("HEADER_GET_TYPE",
                                   "../Grammar/Expression.code");
  UnaryOp.excludeSubTreeFunctionPrototype("HEADER_GET_TYPE",
                                          "../Grammar/Expression.code");
  // It is almost true that the presendence operators exist only on classes
  // derived from UnaryOp and BinaryOp plus the SizeOfOp and TypeIdOp, but in
  // fact the ExpressionRoot is derived from UnaryOp and does not have a
  // precedence member function.
  UnaryOp.setSubTreeFunctionPrototype("HEADER_PRECEDENCE",
                                      "../Grammar/Expression.code");
  UnaryOp.excludeFunctionPrototype("HEADER_PRECEDENCE",
                                   "../Grammar/Expression.code");
  UnaryOp.setFunctionPrototype("HEADER_GET_NEXT_EXPRESSION",
                               "../Grammar/Expression.code");
  UnaryOp.setFunctionPrototype("HEADER_UNARY_EXPRESSION",
                               "../Grammar/Expression.code");
  UnaryOp.setDataPrototype("SgExpression*", "operand_i", "",
                           CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                           DEF_TRAVERSAL, NO_DELETE, COPY_DATA,
                           REQUIRED_TRAVERSAL_MEMBER);
  // Every semantic unary expression owns its exact frontend-computed result
  // type.  This preserves promotions and qualifiers and avoids reconstructing
  // address-of or dereference semantics from a potentially transformed
  // operand.
  UnaryOp.setDataPrototype("SgType*", "expression_type", "",
                           CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                           NO_TRAVERSAL, NO_DELETE);
  UnaryOp.setDataPrototype("SgUnaryOp::Sgop_mode", "mode", "",
                           CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                           NO_TRAVERSAL, NO_DELETE);

  NEW_NONTERMINAL_MACRO(
      ValueExp,
      BoolValExp | StringVal | ShortVal | CharVal | SignedCharVal |
          UnsignedCharVal | WcharVal | UnsignedShortVal | IntVal | EnumVal |
          UnsignedIntVal | LongIntVal | LongLongIntVal | Integer128Val |
          FortranBOZVal | UnsignedLongLongIntVal | UnsignedLongVal | FloatVal |
          DoubleVal | LongDoubleVal | ComplexVal | TemplateParameterVal |
          NullptrValExp | Char16Val | Char32Val | Float80Val | Float128Val |
          BFloat16Val | Float16Val | Float32Val | Float64Val | VoidVal,
      "ValueExp", "ValueExpTag", false);

  ValueExp.excludeFunctionPrototype("HEADER_GET_TYPE",
                                    "../Grammar/Expression.code");
  ValueExp.setFunctionSource("SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
                             "../Grammar/Expression.code");
  ValueExp.setFunctionPrototype("HEADER_VALUE_EXPRESSION",
                                "../Grammar/Expression.code");
  ValueExp.setDataPrototype(
      "SgValueExp::literal_spelling_form_enum", "literal_spelling_form",
      "= SgValueExp::e_literal_spelling_unclassified", NO_CONSTRUCTOR_PARAMETER,
      NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  // Most C and C++ literal node classes encode their type directly.  Other
  // source languages, notably Fortran, attach semantic selectors such as KIND
  // that cannot be reconstructed from the payload carrier class.  Preserve
  // that exact frontend type independently from the carrier and its spelling.
  ValueExp.setDataPrototype("SgType*", "literal_type", "= NULL",
                            NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                            NO_TRAVERSAL, NO_DELETE, COPY_REFERENCE);
  NEW_NONTERMINAL_MACRO(
      Expression,
      UnaryOp | BinaryOp | ExprListExp | VarRefExp | ClassNameRefExp |
          FunctionRefExp | MemberFunctionRefExp | ValueExp | CallExpression |
          SizeOfOp | TypeIdOp | ConditionalExp | NewExp | DeleteExp | ThisExp |
          RefExp | Initializer | VarArgStartOp | VarArgOp | VarArgEndOp |
          VarArgCopyOp | VarArgStartOneOperandOp | NullExpression |
          OmpNameExpression | OmpDirectiveLocalRefExp | OmpSourceExpression |
          OmpInductionItem | OmpApplyTransformation | OmpInitModifier |
          OmpInitModifierList | OmpAppendArgsOperation | OmpMapDistDataPolicy |
          OmpMapItem | MacroExpansionExp | OpaqueValueBinding |
          OpaqueValueReference | PseudoObjectExp | CxxSubobjectDestruction |
          CxxDecompositionRefExp | CxxArrayInitLoop | CxxArrayInitReference |
          CxxDefaultInitExp | SourceLocationBuiltinExp |
          FortranCommonBlockRefExp | VariantExpression | SubscriptExpression |
          ColonShapeExp | AsteriskShapeExp | AssumedRankExp | ImpliedDo |
          StatementExpression | AsmOp | LabelRefExp | ActualArgumentExpression |
          PseudoDestructorRefExp | CAFCoExpression | CAFImageSelectorExp |
          CudaKernelExecConfig | TemplateFunctionRefExp |
          TemplateMemberFunctionRefExp | AlignOfOp | RangeExp |
          TypeTraitBuiltinOperator | CompoundLiteralExp | TypeExpression |
          FunctionParameterRefExp | LambdaExp | NoexceptOp | RequiresExpr |
          SimpleRequirement | TypeRequirement | CompoundRequirement |
          RequirementSubstitutionFailure | NestedRequirement | NonrealRefExp |
          FoldExpression | Designator | PackExpansionExpr | AwaitExpression |
          ChooseExpression,
      "Expression", "ExpressionTag", false);

  // DQ (5/20/2004): Add need_paren to all expression objects so that we can
  // trigger it for any expression and use the value as set by the frontend.
  // See how this works! Added here to fix required paren in SgDotExp where
  // it could not be set properly See test code abstract_op.C line 418:
  //      Rhs.Array_Descriptor.Array_Domain.Push_Array_ID(rhsArrayID);
  // which is translated to
  //      (*(&(Rhs.Array_Descriptor).Array_Domain)).Push_Array_ID(rhsArrayID);
  // That we have build generated the addess operator followed by the deref
  // operator is a separate problem, I think!
  Expression.setDataPrototype("bool", "need_paren", "= false",
                              NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                              NO_TRAVERSAL, NO_DELETE);

  // DQ (8/21/2004): Allow expressions to be marked as lvalues
  Expression.setDataPrototype("bool", "lvalue", "= false",
                              NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                              NO_TRAVERSAL, NO_DELETE);

  // DQ (12/5/2006): Added back boolean to record use of explicit global
  // qualifier. This avoids some over qualification, this only applies to use of
  // "::" prefix not more complex (longer) forms of name qualification.
  Expression.setDataPrototype("bool", "global_qualified_name", "= false",
                              NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                              NO_TRAVERSAL, NO_DELETE);

  Expression.setDataPrototype(
      "SgExpression::semantic_wrapper_mask_enum", "semantic_wrapper_mask",
      "= SgExpression::e_no_semantic_wrapper", NO_CONSTRUCTOR_PARAMETER,
      NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  Expression.setDataPrototype("SgMemberFunctionDeclaration*",
                              "bound_temporary_destructor", "= nullptr",
                              NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                              NO_TRAVERSAL, NO_DELETE, COPY_REFERENCE);
  Expression.setDataPrototype("SgExpression::temporary_storage_duration_enum",
                              "materialized_storage_duration",
                              "= SgExpression::e_temporary_storage_none",
                              NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                              NO_TRAVERSAL, NO_DELETE);
  Expression.setDataPrototype("SgInitializedName*",
                              "materialized_lifetime_owner", "= nullptr",
                              NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                              NO_TRAVERSAL, NO_DELETE, COPY_REFERENCE);

  // Every expression kind can be the canonical semantic owner of a distinct
  // source-spelled expression tree.  Keeping this edge only on a historical
  // subset of subclasses made producer correctness depend on the folded
  // expression's incidental node kind and forced late fallback behavior.
  // The source tree is an exclusively owned, non-primary traversal edge:
  // consumers that choose source spelling validate it explicitly.
  Expression.setDataPrototype("SgExpression*", "originalExpressionTree",
                              "= NULL", NO_CONSTRUCTOR_PARAMETER,
                              NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, DEF_DELETE,
                              CLONE_TREE);

  // Flang folds Fortran KIND/LEN selectors and DATA repeat expressions during
  // semantic analysis. Preserve that exact integer meaning on the source root
  // while retaining the original typed expression tree for direct emission.
  Expression.setDataPrototype("bool",
                              "fortran_integer_constant_value_is_available",
                              "= false", NO_CONSTRUCTOR_PARAMETER,
                              BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  Expression.setDataPrototype("int64_t", "fortran_integer_constant_value",
                              "= 0", NO_CONSTRUCTOR_PARAMETER,
                              BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  Expression.setFunctionPrototype("HEADER", "../Grammar/Expression.code");

  Expression.setDataPrototype("Sg_File_Info*", "operatorPosition", "= NULL",
                              NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                              NO_TRAVERSAL, DEF_DELETE, CLONE_PTR);

  Expression.setDataPrototype("SgExpression*", "alternativeExpr", "= nullptr",
                              NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                              NO_TRAVERSAL, DEF_DELETE, CLONE_TREE);

  Expression.setSubTreeFunctionPrototype("HEADER_GET_TYPE",
                                         "../Grammar/Expression.code");

  // Within Expression get_type() is specific directly (as a virtual function)
  Expression.excludeFunctionPrototype("HEADER_GET_TYPE",
                                      "../Grammar/Expression.code");

  Expression.setFunctionPrototype("HEADER_POST_CONSTRUCTION_INITIALIZATION",
                                  "../Grammar/Expression.code");
  Expression.setSubTreeFunctionPrototype(
      "HEADER_POST_CONSTRUCTION_INITIALIZATION", "../Grammar/Expression.code");

  // SIMD operators
  SIMDBinaryOp.setFunctionPrototype("HEADER_EXTRA_FUNCTIONS",
                                    "../Grammar/Expression.code");
  SIMDBinaryOp.excludeFunctionPrototype("HEADER_GET_TYPE",
                                        "../Grammar/Expression.code");
  SIMDBinaryOp.excludeSubTreeFunctionPrototype("HEADER_GET_TYPE",
                                               "../Grammar/Expression.code");
  SIMDBinaryOp.setFunctionSource(
      "SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
      "../Grammar/Expression.code");

  SIMDAddOp.setFunctionSource("SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
                              "../Grammar/Expression.code");

  SIMDSubOp.setFunctionSource("SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
                              "../Grammar/Expression.code");

  SIMDMulOp.setFunctionSource("SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
                              "../Grammar/Expression.code");

  SIMDDivOp.setFunctionSource("SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
                              "../Grammar/Expression.code");

  SIMDFmaOp.setFunctionSource("SOURCE_EMPTY_POST_CONSTRUCTION_INITIALIZATION",
                              "../Grammar/Expression.code");

  SIMDLoad.setFunctionSource("SOURCE_SIMD_LOAD", "../Grammar/Expression.code");

  SIMDBroadcast.setFunctionSource("SOURCE_SIMD_BROADCAST",
                                  "../Grammar/Expression.code");

  SIMDStore.setFunctionSource("SOURCE_SIMD_STORE",
                              "../Grammar/Expression.code");

  SIMDPartialStore.setFunctionSource("SOURCE_SIMD_PARTIAL_STORE",
                                     "../Grammar/Expression.code");

  SIMDScalarStore.setFunctionSource("SOURCE_SIMD_SCALAR_STORE",
                                    "../Grammar/Expression.code");

  SIMDGather.setFunctionSource("SOURCE_SIMD_GATHER",
                               "../Grammar/Expression.code");

  SIMDExplicitGather.setFunctionSource("SOURCE_SIMD_EXPLICIT_GATHER",
                                       "../Grammar/Expression.code");

  SIMDScatter.setFunctionSource("SOURCE_SIMD_SCATTER",
                                "../Grammar/Expression.code");
}
