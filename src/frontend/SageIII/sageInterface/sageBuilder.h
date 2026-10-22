#ifndef ROSE_SAGE_BUILDER_INTERFACE
#define ROSE_SAGE_BUILDER_INTERFACE

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>

#include "Rose/FloatingValue.h"
#include "sageInterface.h"

// forward declarations required for templated functions using those functions
namespace SageInterface {
ROSE_DLL_API void setOneSourcePositionForTransformation(SgNode *root);
ROSE_DLL_API void setSourcePosition(SgNode *node);
ROSE_DLL_API bool containsUnknownType(const SgType *type);
} // namespace SageInterface

/** @brief High level SAGE III AST node and subtree builders.
 *
 * Building AST trees using raw SgNode constructors is tedious and error-prone.
 * It becomes even more difficult with the presence of symbol tables. This
 * namespace contains major AST node builders on top of the constructors to take
 * care of symbol tables, various edges to scope, parent relationships, and so
 * on.
 *
 * See @ref frontendSageHighLevelInterface.
 */
namespace SageBuilder {

//----------------------------------------------------------
//@{
/*! @name Scope stack interfaces
    \brief  a global data structure to store current scope and parent scopes.

Scope stack is provided as an alternative to manually passing scope parameters
to builder functions. It is not required to be used. Please use the
recommendeded operation functions for maintaining the scope stack. Don't use raw
container access functions to ScopeStack.  e.g. avoid ScopeStack.push_back(),
using pushScopeStack() instead.

 \todo consider complex cases:
        - how many scope stacks to keep? one. examine only one transparently
        - regular: push a child scope of current scope, add symbols etc.
        - irregular: push an arbitrary scope temporarily,  add some symbol, then
pop
        - even push a chain of scopes
        - restore scopes
*/

/*! \brief intended to be a private member, don't access it directly. could be
 * changed any time
 */
extern std::list<SgScopeStatement *> ScopeStack;

//! Support for construction of case sensitive/insensitive symbol table handling
//! in scopes.
extern bool symbol_table_case_insensitive_semantics;

//! Public interfaces of the scope stack, should be stable
ROSE_DLL_API void pushScopeStack(SgScopeStatement *stmt);

ROSE_DLL_API void popScopeStack();
ROSE_DLL_API SgScopeStatement *topScopeStack();
ROSE_DLL_API bool emptyScopeStack();
ROSE_DLL_API void clearScopeStack();

//! Support to retrive the SgGlobal from the internal scope stack (error if not
//! present in a non-empty list, return null for empty list).
SgScopeStatement *getGlobalScopeFromScopeStack();

bool inSwitchScope();

//@}

enum SourcePositionClassification {
  e_sourcePositionError,                //! Error value for enum.
  e_sourcePositionDefault,              //! Default source position.
  e_sourcePositionTransformation,       //! Classify as a transformation.
  e_sourcePositionCompilerGenerated,    //! Classify as compiler generated code
                                        //! (e.g. template instantiation).
  e_sourcePositionFrontendConstruction, //! Specify as source position to be
                                        //! filled in as part of AST
                                        //! construction in the front-end.
  e_sourcePosition_last                 //! Last entry in enum.
};

//! C++ SageBuilder namespace specific state for storage of the source code
//! position state (used to control how the source code positon is defined for
//! IR nodes built within the SageBuilder interface).
extern SourcePositionClassification SourcePositionClassificationMode;

//! Get the current source position classification (defines how IR nodes built
//! by the SageBuilder interface will be classified).
ROSE_DLL_API SourcePositionClassification getSourcePositionClassificationMode();

//! display function for debugging
ROSE_DLL_API std::string display(SourcePositionClassification &scp);

//! Set the current source position classification (defines how IR nodes built
//! by the SageBuilder interface will be classified).
ROSE_DLL_API void
setSourcePositionClassificationMode(SourcePositionClassification X);

//! Enforce the construction-time contract shared by all expression builders
//! whose names end in _nfi: the newly allocated root must not already own any
//! source-position object.  This validator never clears or repairs provenance.
ROSE_DLL_API void requireFreshNfiLocatedNodeSourceState(SgLocatedNode *node,
                                                        const char *producer);
ROSE_DLL_API void requireFreshNfiExpressionSourceState(SgExpression *expression,
                                                       const char *producer);

// *************************************************************************************************************

//--------------------------------------------------------------
//@{
/*! @name Builders for SgType
  \brief Builders for simple and complex SgType nodes, such as integer type,
  function type, array type, struct type, etc.

*/

//! Built in simple types
ROSE_DLL_API SgTypeBool *buildBoolType();
ROSE_DLL_API SgTypeNullptr *buildNullptrType();
ROSE_DLL_API SgTypeChar *buildCharType();
ROSE_DLL_API SgTypeDouble *buildDoubleType();
ROSE_DLL_API SgTypeFloat *buildFloatType();
ROSE_DLL_API SgTypeInt *buildIntType();
ROSE_DLL_API SgTypeLong *buildLongType();
ROSE_DLL_API SgTypeLongDouble *buildLongDoubleType();
ROSE_DLL_API SgTypeLongLong *buildLongLongType();
ROSE_DLL_API SgTypeShort *buildShortType();
ROSE_DLL_API SgTypeFloat80 *buildFloat80Type();
ROSE_DLL_API SgTypeFloat128 *buildFloat128Type();
ROSE_DLL_API SgTypeFloat16 *buildFloat16Type();
ROSE_DLL_API SgTypeFp16 *buildFp16Type();
ROSE_DLL_API SgTypeBFloat16 *buildBFloat16Type();
ROSE_DLL_API SgTypeFloat32x *buildFloat32xType();
ROSE_DLL_API SgTypeFloat64x *buildFloat64xType();
ROSE_DLL_API SgTypeFloat32 *buildFloat32Type();
ROSE_DLL_API SgTypeFloat64 *buildFloat64Type();
ROSE_DLL_API SgTypeTargetBuiltin *
buildTargetBuiltinType(const SgName &spelling,
                       SgTypeTargetBuiltin::target_family_enum target_family);

//! Build a Fortran CHARACTER type from its typed length expression.
ROSE_DLL_API SgTypeString *
buildStringType(SgExpression *stringLengthExpression);

ROSE_DLL_API SgTypeVoid *buildVoidType();
ROSE_DLL_API SgTypeWchar *buildWcharType();

ROSE_DLL_API SgTypeChar8 *buildChar8Type();
ROSE_DLL_API SgTypeChar16 *buildChar16Type();
ROSE_DLL_API SgTypeChar32 *buildChar32Type();

ROSE_DLL_API SgTypeSignedChar *buildSignedCharType();
ROSE_DLL_API SgTypeSignedInt *buildSignedIntType();
ROSE_DLL_API SgTypeSignedLong *buildSignedLongType();
ROSE_DLL_API SgTypeSignedLongLong *buildSignedLongLongType();
ROSE_DLL_API SgTypeSignedShort *buildSignedShortType();

ROSE_DLL_API SgTypeSigned128bitInteger *buildSigned128bitIntegerType();
ROSE_DLL_API SgTypeUnsigned128bitInteger *buildUnsigned128bitIntegerType();

ROSE_DLL_API SgTypeUnsignedChar *buildUnsignedCharType();
ROSE_DLL_API SgTypeUnsignedInt *buildUnsignedIntType();
ROSE_DLL_API SgTypeUnsignedLong *buildUnsignedLongType();
ROSE_DLL_API SgTypeUnsignedLongLong *buildUnsignedLongLongType();
ROSE_DLL_API SgTypeUnsignedShort *buildUnsignedShortType();
ROSE_DLL_API SgTypeUnknown *buildUnknownType();

ROSE_DLL_API SgAutoType *buildAutoType();

//! Builder functions for primitive types with type size (kind) expressions
ROSE_DLL_API SgTypeBool *buildBoolType(SgExpression *kind_expr);
ROSE_DLL_API SgTypeInt *buildIntType(SgExpression *kind_expr);
ROSE_DLL_API SgTypeFloat *buildFloatType(SgExpression *kind_expr);
ROSE_DLL_API SgTypeUnsignedInt *buildUnsignedIntType(SgExpression *kind_expr);

//! Build a pointer type
ROSE_DLL_API SgPointerType *buildPointerType(SgType *base_type = nullptr);

//! Build a reference type
ROSE_DLL_API SgReferenceType *buildReferenceType(SgType *base_type = nullptr);

//! Build a rvalue reference type
ROSE_DLL_API SgRvalueReferenceType *buildRvalueReferenceType(SgType *base_type);

//! Build a decltype reference type
ROSE_DLL_API SgDeclType *buildDeclType(SgExpression *base_expression,
                                       SgType *base_type,
                                       bool is_gnu_decltype = false);

//! Build a GNU typeof operator
ROSE_DLL_API SgTypeOfType *buildTypeOfType(SgExpression *base_expression,
                                           SgType *base_type);

//! Build a modifier type.
ROSE_DLL_API SgModifierType *buildModifierType(SgType *base_type,
                                               const SgTypeModifier &modifier);

//! Build a const type.
ROSE_DLL_API SgModifierType *buildConstType(SgType *base_type = nullptr);

//! Build a volatile type.
ROSE_DLL_API SgModifierType *buildVolatileType(SgType *base_type = nullptr);

//! Build a const volatile type.
ROSE_DLL_API SgModifierType *
buildConstVolatileType(SgType *base_type = nullptr);

//! Build a restrict type.
ROSE_DLL_API SgModifierType *buildRestrictType(SgType *base_type);

//! Build ArrayType
ROSE_DLL_API SgArrayType *buildArrayType(SgType *base_type = nullptr,
                                         SgExpression *index = nullptr);

//! Build an ArrayType based on dimension information.
//!
//! \param base_type The base type of the array.
//!        Note that if the base type is itself an array type, the shape of the
//!        array may be changed.
//! \param dim_info A list of expressions describing the shape of the array.
//!        The rank of the array is set from the length of this list.
ROSE_DLL_API SgArrayType *buildArrayType(SgType *base_type,
                                         SgExprListExp *dim_info);

//! Build a type based on the Fortran kind mechanism
ROSE_DLL_API SgModifierType *buildFortranKindType(SgType *base_type,
                                                  SgExpression *kindExpression);

//! Build a canonical function type from a return type and exact parameter
//! types. A final SgTypeEllipse atomically publishes variadic identity;
//! ellipses in any other position are malformed and abort.
ROSE_DLL_API SgFunctionType *
buildFunctionType(SgType *return_type, SgFunctionParameterTypeList *typeList,
                  bool hasPrototype = true);

//! Intern a complete semantic signature, consuming its owned children even
//! when an identical canonical type already exists.
ROSE_DLL_API SgFunctionType *
internFunctionType(std::unique_ptr<SgFunctionType> candidate);

ROSE_DLL_API std::unique_ptr<SgFunctionType>
copyFunctionSignature(const SgFunctionType *specification, SgType *returnType,
                      std::unique_ptr<SgFunctionParameterTypeList> arguments,
                      SgType *classType, unsigned int memberFlags,
                      SgCopyHelp &bindings, SgFunctionType *semanticSignature);

//! Build function type from return type and parameter list
ROSE_DLL_API SgFunctionType *
buildFunctionType(SgType *return_type, SgFunctionParameterList *argList);

ROSE_DLL_API SgMemberFunctionType *buildMemberFunctionType(
    SgType *return_type, SgFunctionParameterTypeList *typeList,
    SgScopeStatement *struct_name, unsigned int mfunc_specifier);

ROSE_DLL_API SgMemberFunctionType *
buildMemberFunctionType(SgType *return_type,
                        SgFunctionParameterTypeList *typeList,
                        SgType *classType, unsigned int mfunc_specifier);

//! Bind a complete semantic function signature to its member class.
ROSE_DLL_API SgMemberFunctionType *
buildMemberFunctionType(const SgFunctionType *type, SgType *classType,
                        unsigned int mfunc_specifier);
ROSE_DLL_API SgMemberFunctionType *
buildMemberFunctionType(const SgFunctionType *type, SgScopeStatement *scope,
                        unsigned int mfunc_specifier);

ROSE_DLL_API SgPointerMemberType *buildPointerMemberType(SgType *base_type,
                                                         SgType *classType);

//! Build a complex type
ROSE_DLL_API SgTypeComplex *buildComplexType(SgType *base_type = nullptr);

//! Build an imaginary type
ROSE_DLL_API SgTypeImaginary *buildImaginaryType(SgType *base_type = nullptr);

//! Build a const/volatile type qualifier
ROSE_DLL_API SgConstVolatileModifier *
buildConstVolatileModifier(SgConstVolatileModifier::cv_modifier_enum mtype =
                               SgConstVolatileModifier::e_unknown);

//! Build a semantic non-real dependent type (optionally with template
//! arguments).  Every structurally owned located template-argument node must
//! be unclassified or already have exact compiler-generated semantic
//! provenance. The optional template declaration is linked before the
//! internal SgNonrealDecl is published.
ROSE_DLL_API SgNonrealType *
buildSemanticNonrealType(const SgName &name, SgScopeStatement *scope,
                         const SgTemplateArgumentPtrList *tplArgs,
                         SgDeclarationStatement *templateDeclaration = nullptr,
                         SgType *aliasedType = nullptr);
ROSE_DLL_API SgNonrealType *
buildSemanticNonrealType(const SgNonrealDecl *source,
                         const SgTemplateArgumentPtrList &arguments,
                         SgType *aliasedType);

//@}

//--------------------------------------------------------------
//@{
/*! @name Builders for expressions */

ROSE_DLL_API SgVariantExpression *buildVariantExpression(SgType *anchor_type);

//! Build a typed null-expression placeholder, set file info as the default one.
ROSE_DLL_API SgNullExpression *
buildNullExpression(SgNullExpression::null_expression_role_enum role);
//! No file info version of buildNullExpression(). File info is to be set later
//! on.
ROSE_DLL_API SgNullExpression *
buildNullExpression_nfi(SgNullExpression::null_expression_role_enum role);

//! Build a Fortran colon-shape expression, set file info as the default one.
ROSE_DLL_API SgColonShapeExp *buildColonShapeExp();
//! No file info version of buildColonShapeExp(). File info is to be set later
//! on.
ROSE_DLL_API SgColonShapeExp *buildColonShapeExp_nfi();

//! Build a bool value expression.
ROSE_DLL_API SgBoolValExp *buildBoolValExp(int value = 0);
ROSE_DLL_API SgBoolValExp *buildBoolValExp(bool value = 0);
ROSE_DLL_API SgBoolValExp *buildBoolValExp_nfi(int value);

ROSE_DLL_API SgCharVal *buildCharVal(int64_t value = 0);
ROSE_DLL_API SgCharVal *buildCharVal_nfi(int64_t value, const std::string &str);

ROSE_DLL_API SgNullptrValExp *buildNullptrValExp();
ROSE_DLL_API SgNullptrValExp *buildNullptrValExp_nfi();

ROSE_DLL_API SgVoidVal *buildVoidVal();
ROSE_DLL_API SgVoidVal *buildVoidVal_nfi();

ROSE_DLL_API SgWcharVal *buildWcharVal(int64_t value = 0);
ROSE_DLL_API SgWcharVal *buildWcharVal_nfi(int64_t value,
                                           const std::string &str);

ROSE_DLL_API SgChar16Val *buildChar16Val(unsigned short value = 0);
ROSE_DLL_API SgChar16Val *buildChar16Val_nfi(unsigned short value,
                                             const std::string &str);
ROSE_DLL_API SgChar32Val *buildChar32Val(unsigned int value = 0);
ROSE_DLL_API SgChar32Val *buildChar32Val_nfi(unsigned int value,
                                             const std::string &str);

ROSE_DLL_API SgComplexVal *buildComplexVal(SgExpression *real_value,
                                           SgExpression *imaginary_value,
                                           SgType *precision_type);
ROSE_DLL_API SgComplexVal *buildComplexVal_nfi(SgExpression *real_value,
                                               SgExpression *imaginary_value,
                                               SgType *precision_type);
ROSE_DLL_API SgComplexVal *
buildImaginaryVal(Rose::FloatingValue imaginary_value);
ROSE_DLL_API SgComplexVal *buildImaginaryVal(SgExpression *imaginary_value,
                                             SgType *precision_type);
ROSE_DLL_API SgComplexVal *buildImaginaryVal_nfi(SgExpression *imaginary_value,
                                                 SgType *precision_type);

//! Build a double value expression
ROSE_DLL_API SgDoubleVal *buildDoubleVal(double value = 0.0);
ROSE_DLL_API SgDoubleVal *buildDoubleVal_nfi(double value,
                                             const std::string &str);

ROSE_DLL_API SgFloatVal *buildFloatVal(float value = 0.0);
ROSE_DLL_API SgFloatVal *buildFloatVal_nfi(float value = 0.0);
ROSE_DLL_API SgFloatVal *buildFloatVal_nfi(float value, const std::string &str);

//! Build an integer value expression
ROSE_DLL_API SgIntVal *buildIntVal(int value = 0);
ROSE_DLL_API SgIntVal *buildIntValHex(int value = 0);
ROSE_DLL_API SgIntVal *buildIntVal_nfi(int value = 0);
ROSE_DLL_API SgIntVal *buildIntVal_nfi(int value, const std::string &str);
//! Build an integer value expression by converting the string
ROSE_DLL_API SgIntVal *buildIntVal_nfi(const std::string &str);

//! Build a long integer value expression
ROSE_DLL_API SgLongIntVal *buildLongIntVal(long value = 0);
ROSE_DLL_API SgLongIntVal *buildLongIntVal_nfi(long value,
                                               const std::string &str);

//! Build a long long integer value expression
ROSE_DLL_API SgLongLongIntVal *buildLongLongIntVal(long long value = 0);
ROSE_DLL_API SgLongLongIntVal *buildLongLongIntValHex(long long value = 0);
ROSE_DLL_API SgLongLongIntVal *buildLongLongIntVal_nfi(long long value,
                                                       const std::string &str);
// !Build enum val without file info: nfi
ROSE_DLL_API SgEnumVal *buildEnumVal_nfi(long long int value,
                                         SgEnumDeclaration *decl, SgName name);
// !Build enum val with transformation file info
ROSE_DLL_API SgEnumVal *buildEnumVal(long long int value,
                                     SgEnumDeclaration *decl, SgName name);
ROSE_DLL_API SgEnumVal *buildEnumVal(SgEnumFieldSymbol *sym);

ROSE_DLL_API SgLongDoubleVal *
buildLongDoubleVal(Rose::FloatingValue value = 0.0);
ROSE_DLL_API SgLongDoubleVal *buildLongDoubleVal_nfi(Rose::FloatingValue value,
                                                     const std::string &str);

ROSE_DLL_API SgFloat80Val *buildFloat80Val(Rose::FloatingValue value = 0.0);
ROSE_DLL_API SgFloat80Val *buildFloat80Val_nfi(Rose::FloatingValue value,
                                               const std::string &str);

ROSE_DLL_API SgFloat128Val *buildFloat128Val(Rose::FloatingValue value = 0.0);
ROSE_DLL_API SgFloat128Val *buildFloat128Val_nfi(Rose::FloatingValue value,
                                                 const std::string &str);

//! Build a bfloat16
ROSE_DLL_API SgBFloat16Val *buildBFloat16Val(float v = 0);
ROSE_DLL_API SgBFloat16Val *buildBFloat16Val_nfi(float v,
                                                 const std::string &str);

//! Build a float16
ROSE_DLL_API SgFloat16Val *buildFloat16Val(float v = 0);
ROSE_DLL_API SgFloat16Val *buildFloat16Val_nfi(float v, const std::string &str);

//! Build a float32
ROSE_DLL_API SgFloat32Val *buildFloat32Val(float v = 0);
ROSE_DLL_API SgFloat32Val *buildFloat32Val_nfi(float v, const std::string &str);

//! Build a float64
ROSE_DLL_API SgFloat64Val *buildFloat64Val(double v = 0);
ROSE_DLL_API SgFloat64Val *buildFloat64Val_nfi(double v,
                                               const std::string &str);

ROSE_DLL_API SgShortVal *buildShortVal(short value = 0);
ROSE_DLL_API SgShortVal *buildShortValHex(short value = 0);
ROSE_DLL_API SgShortVal *buildShortVal_nfi(short value, const std::string &str);

ROSE_DLL_API SgStringVal *buildStringVal(std::string value = "");
ROSE_DLL_API SgStringVal *buildStringVal_nfi(std::string value);

//! Build an unsigned char
ROSE_DLL_API SgUnsignedCharVal *buildUnsignedCharVal(unsigned char v = 0);
ROSE_DLL_API SgUnsignedCharVal *buildUnsignedCharValHex(unsigned char v = 0);
ROSE_DLL_API SgUnsignedCharVal *
buildUnsignedCharVal_nfi(unsigned char v, const std::string &str);

//! Build a signed char
ROSE_DLL_API SgSignedCharVal *buildSignedCharVal(signed char v = 0);
ROSE_DLL_API SgSignedCharVal *buildSignedCharValHex(signed char v = 0);
ROSE_DLL_API SgSignedCharVal *buildSignedCharVal_nfi(signed char v,
                                                     const std::string &str);

//! Build an unsigned short integer
ROSE_DLL_API SgUnsignedShortVal *buildUnsignedShortVal(unsigned short v = 0);
ROSE_DLL_API SgUnsignedShortVal *buildUnsignedShortValHex(unsigned short v = 0);
ROSE_DLL_API SgUnsignedShortVal *
buildUnsignedShortVal_nfi(unsigned short v, const std::string &str);

//! Build an unsigned integer
ROSE_DLL_API SgUnsignedIntVal *buildUnsignedIntVal(unsigned int v = 0);
ROSE_DLL_API SgUnsignedIntVal *buildUnsignedIntValHex(unsigned int v = 0);
ROSE_DLL_API SgUnsignedIntVal *buildUnsignedIntVal_nfi(unsigned int v,
                                                       const std::string &str);

//! Build a unsigned long integer
ROSE_DLL_API SgUnsignedLongVal *buildUnsignedLongVal(unsigned long v = 0);
ROSE_DLL_API SgUnsignedLongVal *buildUnsignedLongValHex(unsigned long v = 0);
ROSE_DLL_API SgUnsignedLongVal *
buildUnsignedLongVal_nfi(unsigned long v, const std::string &str);

//! Build an unsigned long long integer
ROSE_DLL_API SgUnsignedLongLongIntVal *
buildUnsignedLongLongIntVal(unsigned long long v = 0);
ROSE_DLL_API SgUnsignedLongLongIntVal *
buildUnsignedLongLongIntValHex(unsigned long long v = 0);
ROSE_DLL_API SgUnsignedLongLongIntVal *
buildUnsignedLongLongIntVal_nfi(unsigned long long v, const std::string &str);

//! Build an template parameter value expression
ROSE_DLL_API SgTemplateParameterVal *
buildTemplateParameterVal(int template_parameter_position,
                          SgTemplateParameter *parameter);
ROSE_DLL_API SgTemplateParameterVal *
buildTemplateParameterVal_nfi(int template_parameter_position,
                              SgTemplateParameter *parameter);

//! Build a template type, used for template parameter and later argument
ROSE_DLL_API SgTemplateType *buildTemplateType(SgName name = "");

//! Build a template parameter, passing enum kind and SgTemplateType
//! template_parameter_enum { parameter_undefined = 0, type_parameter = 1,
//! nontype_parameter = 2,  template_parameter = 3}
ROSE_DLL_API SgTemplateParameter *buildTemplateParameter(
    SgTemplateParameter::template_parameter_enum parameterType, SgType *,
    SgTemplateParameter::template_parameter_keyword_enum keyword,
    SgInitializedName *initializedName = nullptr,
    SgTemplateParameter *canonicalNonTypeParameter = nullptr,
    SgTemplateDeclaration *templateDeclaration = nullptr);
ROSE_DLL_API SgTemplateParameter *buildTemplateParameter(
    SgTemplateParameter::template_parameter_enum parameterType, SgType *,
    const SgName &parameterName, SgScopeStatement *scope,
    SgTemplateParameter::template_parameter_keyword_enum keyword,
    SgTemplateDeclaration *templateDeclaration = nullptr);

//! Build a declaration of a non-real class or class-member in an explicit,
//! non-null lexical declaration scope. A null child scope requests creation of
//! a new owned child declaration scope.
ROSE_DLL_API SgNonrealDecl *
buildNonrealDecl(const SgName &name, SgDeclarationScope *scope,
                 SgDeclarationScope *child_scope = NULL);

//! Build a reference to the non-real declaration of a member of a non-real
//! class
ROSE_DLL_API SgNonrealRefExp *buildNonrealRefExp_nfi(SgNonrealSymbol *sym);

//! Build this pointer
ROSE_DLL_API SgThisExp *buildThisExp(SgSymbol *sym, SgType *expression_type);
ROSE_DLL_API SgThisExp *buildThisExp_nfi(SgSymbol *sym,
                                         SgType *expression_type);

#define BUILD_UNARY_PROTO(suffix)                                              \
  ROSE_DLL_API Sg##suffix *build##suffix(SgExpression *op,                     \
                                         SgType *result_type);                 \
  ROSE_DLL_API Sg##suffix *build##suffix##_nfi(SgExpression *op,               \
                                               SgType *result_type);

//! Build the exact semantic result type of applying unary '&' to an
//! expression of operand_type.  C++ reference declarations denote their
//! referred-to object in an expression, so this never constructs the illegal
//! pointer-to-reference type.
ROSE_DLL_API SgPointerType *buildAddressOfResultType(SgType *operand_type);

ROSE_DLL_API SgAddressOfOp *
buildAddressOfOp(SgExpression *operand, SgType *type,
                 SgAddressOfOp::address_operand_form_enum form);
ROSE_DLL_API SgAddressOfOp *
buildAddressOfOp_nfi(SgExpression *operand, SgType *type,
                     SgAddressOfOp::address_operand_form_enum form);

BUILD_UNARY_PROTO(BitComplementOp)
BUILD_UNARY_PROTO(MinusOp)
BUILD_UNARY_PROTO(NotOp)
BUILD_UNARY_PROTO(PointerDerefExp)
BUILD_UNARY_PROTO(UnaryAddOp)
BUILD_UNARY_PROTO(MinusMinusOp)
BUILD_UNARY_PROTO(PlusPlusOp)
BUILD_UNARY_PROTO(RealPartOp)
BUILD_UNARY_PROTO(ImagPartOp)
BUILD_UNARY_PROTO(ConjugateOp)
BUILD_UNARY_PROTO(VarArgStartOneOperandOp)
BUILD_UNARY_PROTO(VarArgEndOp)

//! Build a fully checked type conversion.  All semantic fields are mandatory.
ROSE_DLL_API SgCastExp *
buildCastExp(SgExpression *operand_i, SgType *expression_type,
             SgCastExp::cast_type_enum cast_type,
             SgCastExp::semantic_conversion_kind_enum semantic_kind,
             SgCastExp::value_category_enum value_category,
             const SgTypePtrList &conversion_base_path, SgType *source_type);
ROSE_DLL_API SgCastExp *
buildCastExp_nfi(SgExpression *operand_i, SgType *expression_type,
                 SgCastExp::cast_type_enum cast_type,
                 SgCastExp::semantic_conversion_kind_enum semantic_kind,
                 SgCastExp::value_category_enum value_category,
                 const SgTypePtrList &conversion_base_path,
                 SgType *source_type);

//! Build a source-emitting transformation cast whose semantics are completely
//! determined by its exact operand and result types.  Ambiguous conversions
//! are rejected instead of being assigned a guessed conversion kind.
ROSE_DLL_API SgCastExp *
buildTransformationCastExp(SgExpression *operand_i, SgType *expression_type,
                           SgCastExp::cast_type_enum cast_type);
ROSE_DLL_API SgCastExp *
buildTransformationCastExp(SgExpression *operand_i, SgType *expression_type,
                           SgCastExp::cast_type_enum cast_type,
                           SgType *source_type);
ROSE_DLL_API SgCastExp *
buildTransformationCastExp_nfi(SgExpression *operand_i, SgType *expression_type,
                               SgCastExp::cast_type_enum cast_type);

//! Concise operation-specific forms for transformation-authored C-style or
//! explicitly selected source surfaces.  These are overloads, not defaulted
//! parameters; both operands remain mandatory and undergo exact classification.
ROSE_DLL_API SgCastExp *buildCastExp(SgExpression *operand_i,
                                     SgType *expression_type);
ROSE_DLL_API SgCastExp *buildCastExp(SgExpression *operand_i,
                                     SgType *expression_type,
                                     SgCastExp::cast_type_enum cast_type);
ROSE_DLL_API SgCastExp *buildCastExp_nfi(SgExpression *operand_i,
                                         SgType *expression_type);
ROSE_DLL_API SgCastExp *buildCastExp_nfi(SgExpression *operand_i,
                                         SgType *expression_type,
                                         SgCastExp::cast_type_enum cast_type);

//! Build vararg op expression
ROSE_DLL_API SgVarArgOp *buildVarArgOp_nfi(SgExpression *operand_i,
                                           SgType *expression_type);

//! Build -- expression, Sgop_mode is a value of either SgUnaryOp::prefix or
//! SgUnaryOp::postfix
ROSE_DLL_API SgMinusOp *buildMinusOp(SgExpression *operand_i,
                                     SgType *result_type,
                                     SgUnaryOp::Sgop_mode a_mode);
ROSE_DLL_API SgMinusOp *buildMinusOp_nfi(SgExpression *operand_i,
                                         SgType *result_type,
                                         SgUnaryOp::Sgop_mode a_mode);
ROSE_DLL_API SgMinusMinusOp *buildMinusMinusOp(SgExpression *operand_i,
                                               SgType *result_type,
                                               SgUnaryOp::Sgop_mode a_mode);
ROSE_DLL_API SgMinusMinusOp *buildMinusMinusOp_nfi(SgExpression *operand_i,
                                                   SgType *result_type,
                                                   SgUnaryOp::Sgop_mode a_mode);

//! Build ++x or x++ , specify prefix or postfix using either SgUnaryOp::prefix
//! or SgUnaryOp::postfix
ROSE_DLL_API SgPlusPlusOp *buildPlusPlusOp(SgExpression *operand_i,
                                           SgType *result_type,
                                           SgUnaryOp::Sgop_mode a_mode);
ROSE_DLL_API SgPlusPlusOp *buildPlusPlusOp_nfi(SgExpression *operand_i,
                                               SgType *result_type,
                                               SgUnaryOp::Sgop_mode a_mode);

//! Build a ThrowOp expression
ROSE_DLL_API SgThrowOp *buildThrowOp(SgExpression *, SgType *result_type,
                                     SgThrowOp::e_throw_kind);
ROSE_DLL_API SgThrowOp *buildThrowOp_nfi(SgExpression *, SgType *result_type,
                                         SgThrowOp::e_throw_kind);

ROSE_DLL_API SgNewExp *
buildNewExp(SgType *type, SgPointerType *result_type, SgExpression *array_bound,
            SgNewExp::array_bound_source_form_enum source_form,
            SgExprListExp *exprListExp, SgConstructorInitializer *constInit,
            SgExpression *expr, short int val, SgFunctionDeclaration *funcDecl,
            unsigned int allocation_implicit_argument_count);
ROSE_DLL_API SgNewExp *buildNewExp_nfi(
    SgType *type, SgPointerType *result_type, SgExpression *array_bound,
    SgNewExp::array_bound_source_form_enum source_form,
    SgExprListExp *exprListExp, SgConstructorInitializer *constInit,
    SgExpression *expr, short int val, SgFunctionDeclaration *funcDecl,
    unsigned int allocation_implicit_argument_count);

ROSE_DLL_API SgDeleteExp *
buildDeleteExp(SgExpression *variable, short is_array,
               short need_global_specifier,
               SgFunctionDeclaration *deleteOperatorDeclaration);

ROSE_DLL_API SgTypeIdOp *buildTypeIdOp(SgExpression *operand_expr,
                                       SgType *operand_type,
                                       SgType *expression_type,
                                       bool operand_is_evaluated);
ROSE_DLL_API SgTypeIdOp *buildTypeIdOp_nfi(SgExpression *operand_expr,
                                           SgType *operand_type,
                                           SgType *expression_type,
                                           bool operand_is_evaluated);

#undef BUILD_UNARY_PROTO

/*! The instantiated functions' prototypes are not shown since they are expanded
 * using macros. Documentation tools do not expand these macros.
 */

#define BUILD_BINARY_PROTO(suffix)                                             \
  ROSE_DLL_API Sg##suffix *build##suffix(SgExpression *lhs, SgExpression *rhs, \
                                         SgType *result_type);                 \
  ROSE_DLL_API Sg##suffix *build##suffix##_nfi(                                \
      SgExpression *lhs, SgExpression *rhs, SgType *result_type);

BUILD_BINARY_PROTO(AddOp)
BUILD_BINARY_PROTO(AndAssignOp)
BUILD_BINARY_PROTO(AndOp)
BUILD_BINARY_PROTO(ArrowExp)
BUILD_BINARY_PROTO(ArrowStarOp)
BUILD_BINARY_PROTO(AssignOp)
BUILD_BINARY_PROTO(BitAndOp)
BUILD_BINARY_PROTO(BitOrOp)
BUILD_BINARY_PROTO(BitXorOp)

BUILD_BINARY_PROTO(CommaOpExp)
BUILD_BINARY_PROTO(ConcatenationOp)
BUILD_BINARY_PROTO(DivAssignOp)
BUILD_BINARY_PROTO(DivideOp)
BUILD_BINARY_PROTO(DotExp)
BUILD_BINARY_PROTO(DotStarOp)
BUILD_BINARY_PROTO(EqualityOp)

BUILD_BINARY_PROTO(ExponentiationOp)
BUILD_BINARY_PROTO(ExponentiationAssignOp)
BUILD_BINARY_PROTO(GreaterOrEqualOp)
BUILD_BINARY_PROTO(GreaterThanOp)
BUILD_BINARY_PROTO(IntegerDivideOp)
BUILD_BINARY_PROTO(IntegerDivideAssignOp)
BUILD_BINARY_PROTO(IorAssignOp)

BUILD_BINARY_PROTO(LessOrEqualOp)
BUILD_BINARY_PROTO(LessThanOp)
BUILD_BINARY_PROTO(LshiftAssignOp)
BUILD_BINARY_PROTO(LshiftOp)

BUILD_BINARY_PROTO(MinusAssignOp)
BUILD_BINARY_PROTO(ModAssignOp)
BUILD_BINARY_PROTO(ModOp)
BUILD_BINARY_PROTO(MultAssignOp)
BUILD_BINARY_PROTO(MultiplyOp)

BUILD_BINARY_PROTO(NotEqualOp)
BUILD_BINARY_PROTO(OrOp)
BUILD_BINARY_PROTO(PlusAssignOp)
BUILD_BINARY_PROTO(PntrArrRefExp)
ROSE_DLL_API SgPntrArrRefExp *buildPntrArrRefExp_nfi(
    SgExpression *lhs, SgExpression *rhs, SgType *result_type,
    std::optional<long long> result_storage_size_bytes,
    const std::vector<std::optional<long long>> &subscript_stride_bits = {});
BUILD_BINARY_PROTO(RshiftAssignOp)

BUILD_BINARY_PROTO(RshiftOp)
BUILD_BINARY_PROTO(ScopeOp)
BUILD_BINARY_PROTO(SubtractOp)
BUILD_BINARY_PROTO(XorAssignOp)

BUILD_BINARY_PROTO(VarArgCopyOp)
BUILD_BINARY_PROTO(VarArgStartOp)

BUILD_BINARY_PROTO(SpaceshipOp)

#undef BUILD_BINARY_PROTO

//! Build a conditional expression ?:
ROSE_DLL_API SgConditionalExp *buildConditionalExp(SgExpression *test,
                                                   SgExpression *a,
                                                   SgExpression *b,
                                                   SgType *result_type);
SgConditionalExp *buildConditionalExp_nfi(SgExpression *test, SgExpression *a,
                                          SgExpression *b, SgType *t);
//! Build the GNU omitted-middle conditional `common ?: false_expression`.
//! The common operand is represented once and therefore evaluated once.
ROSE_DLL_API SgConditionalExp *
buildBinaryConditionalExp(SgExpression *common, SgExpression *false_expression,
                          SgType *result_type);
SgConditionalExp *buildBinaryConditionalExp_nfi(SgExpression *common,
                                                SgExpression *false_expression,
                                                SgType *result_type);

//! Build a SgExprListExp, used for function call parameter list etc.
ROSE_DLL_API SgExprListExp *
buildExprListExp(SgExpression *expr1 = NULL, SgExpression *expr2 = NULL,
                 SgExpression *expr3 = NULL, SgExpression *expr4 = NULL,
                 SgExpression *expr5 = NULL, SgExpression *expr6 = NULL,
                 SgExpression *expr7 = NULL, SgExpression *expr8 = NULL,
                 SgExpression *expr9 = NULL, SgExpression *expr10 = NULL);
ROSE_DLL_API SgExprListExp *
buildExprListExp(const std::vector<SgExpression *> &exprs);
SgExprListExp *buildExprListExp_nfi();
SgExprListExp *buildExprListExp_nfi(const std::vector<SgExpression *> &exprs);

//! Build a SgSubscriptExpression, used for array shape expressions.  The lower
//! bound and stride may be nullptrs
SgSubscriptExpression *buildSubscriptExpression_nfi(SgExpression *lower_bound,
                                                    SgExpression *upper_bound,
                                                    SgExpression *stride);

//! Build SgVarRefExp for an existing variable symbol found from scope.
//! Missing declarations are malformed AST construction and abort immediately.
ROSE_DLL_API SgVarRefExp *buildVarRefExp(const SgName &name,
                                         SgScopeStatement *scope = NULL);

//! Build SgVarRefExp based on a variable's name. It will lookup symbol table
//! internally starting from scope. A variable is unique so type can be
//! inferred.
ROSE_DLL_API SgVarRefExp *buildVarRefExp(const std::string &varName,
                                         SgScopeStatement *scope = NULL);

//! Build a variable reference using a C style char array
ROSE_DLL_API SgVarRefExp *buildVarRefExp(const char *varName,
                                         SgScopeStatement *scope = NULL);

//! Build a variable reference from an existing symbol
ROSE_DLL_API SgVarRefExp *buildVarRefExp(SgVariableSymbol *varSymbol);
ROSE_DLL_API SgVarRefExp *buildVarRefExp_nfi(SgVariableSymbol *varSymbol);

//! Build a variable reference from an existing variable declaration. The
//! assumption is a SgVariableDeclartion only declares one variable in the ROSE
//! AST.
ROSE_DLL_API SgVarRefExp *buildVarRefExp(SgVariableDeclaration *vardecl);

//! Build a variable reference from an initialized name's exact symbol.
ROSE_DLL_API SgVarRefExp *buildVarRefExp(SgInitializedName *initname,
                                         SgScopeStatement *scope = NULL);

//! Build function for compound literals (uses a SgVariableSymbol and is similar
//! to buildVarRefExp_nfi()).
SgCompoundLiteralExp *buildCompoundLiteralExp_nfi(SgVariableSymbol *varSymbol);
SgCompoundLiteralExp *buildCompoundLiteralExp(SgVariableSymbol *varSymbol);

//! Build a Fortran numeric label ref exp
ROSE_DLL_API SgLabelRefExp *buildLabelRefExp(SgLabelSymbol *s);

//! Build SgFunctionRefExp based on a C++ function's name and function type. It
//! will lookup symbol table internally starting from scope. A hidden prototype
//! will be created internally to introduce a new function symbol if the
//! function symbol cannot be found.
ROSE_DLL_API SgFunctionRefExp *
buildFunctionRefExp(const SgName &name, const SgType *func_type,
                    SgScopeStatement *scope = NULL);

ROSE_DLL_API SgFunctionRefExp *
buildFunctionRefExp(const char *name, const SgType *func_type,
                    SgScopeStatement *scope = NULL);

//! Build SgFunctionRefExp based on a C function's name. It will lookup symbol
//! table internally starting from scope and return the first matching function.
ROSE_DLL_API SgFunctionRefExp *
buildFunctionRefExp(const SgName &name, SgScopeStatement *scope = NULL);

ROSE_DLL_API SgFunctionRefExp *
buildFunctionRefExp(const char *name, SgScopeStatement *scope = NULL);

//! Build SgFunctionRefExp based on a function's declaration.
ROSE_DLL_API SgFunctionRefExp *
buildFunctionRefExp(const SgFunctionDeclaration *func_decl);

//! Build SgFunctionRefExp based on a function's symbol.
ROSE_DLL_API SgFunctionRefExp *buildFunctionRefExp(SgFunctionSymbol *sym);
ROSE_DLL_API SgFunctionRefExp *buildFunctionRefExp(SgFunctionSymbol *sym,
                                                   SgType *referenceType);

SgFunctionRefExp *buildFunctionRefExp_nfi(const SgName &name,
                                          const SgType *func_type,
                                          SgScopeStatement *scope = NULL);
SgFunctionRefExp *buildFunctionRefExp_nfi(SgFunctionSymbol *sym);
ROSE_DLL_API SgFunctionRefExp *buildFunctionRefExp_nfi(SgFunctionSymbol *sym,
                                                       SgType *referenceType);

//! Build a Fortran function reference with separate resolved-procedure and
//! source-visible binding identities.
ROSE_DLL_API SgFunctionRefExp *buildFortranFunctionRefExp(
    SgFunctionSymbol *semantic_symbol, SgFunctionSymbol *source_visible_symbol,
    SgFunctionRefExp::fortran_source_visible_binding_kind_enum kind);
ROSE_DLL_API SgFunctionRefExp *buildFortranFunctionRefExp_nfi(
    SgFunctionSymbol *semantic_symbol, SgFunctionSymbol *source_visible_symbol,
    SgFunctionRefExp::fortran_source_visible_binding_kind_enum kind);

SgTemplateFunctionRefExp *
buildTemplateFunctionRefExp_nfi(SgTemplateFunctionSymbol *sym);

SgTemplateMemberFunctionRefExp *
buildTemplateMemberFunctionRefExp_nfi(SgTemplateMemberFunctionSymbol *sym,
                                      bool virtual_call, bool need_qualifier);

SgMemberFunctionRefExp *
buildMemberFunctionRefExp_nfi(SgMemberFunctionSymbol *sym, bool virtual_call,
                              bool need_qualifier);
ROSE_DLL_API SgMemberFunctionRefExp *
buildSemanticMemberFunctionRefExp(SgMemberFunctionSymbol *sym,
                                  bool virtual_call, bool need_qualifier);
ROSE_DLL_API SgMemberFunctionRefExp *
buildMemberFunctionRefExp(SgMemberFunctionSymbol *sym, bool virtual_call,
                          bool need_qualifier);
SgClassNameRefExp *buildClassNameRefExp_nfi(SgClassSymbol *sym);
ROSE_DLL_API SgClassNameRefExp *buildClassNameRefExp(SgClassSymbol *sym);

//! Build a function call expression
ROSE_DLL_API SgFunctionCallExp *
buildFunctionCallExp(SgFunctionSymbol *sym, SgExprListExp *parameters = NULL);
SgFunctionCallExp *buildFunctionCallExp_nfi(SgExpression *f,
                                            SgType *result_type,
                                            SgExprListExp *parameters = NULL);
SgFunctionCallExp *buildFunctionCallExp_nfi(const SgName &name,
                                            SgType *return_type,
                                            SgExprListExp *parameters = NULL,
                                            SgScopeStatement *scope = NULL);
ROSE_DLL_API SgFunctionCallExp *
buildFunctionCallExp(SgExpression *f, SgType *result_type,
                     SgExprListExp *parameters = NULL);

//! Build a function call expression,it will automatically search for function
//! symbols internally to build a right function reference etc. It tolerates the
//! lack of the function symbol to support generating calls to library functions
//! whose headers have not yet been inserted.
ROSE_DLL_API SgFunctionCallExp *
buildFunctionCallExp(const SgName &name, SgType *return_type,
                     SgExprListExp *parameters = NULL,
                     SgScopeStatement *scope = NULL);

//! Build member function calls
/*!
 *Create a member function call
 *  This function looks for the function symbol in the given className
 *  The function should exist in the class
 *  The class should be #included or present in the source file parsed by
 *frontend
 *
 * Parameters:
 *  className: template class name, e.g. vector
 *  objectExpression: the variable reference expression to an object of template
 *class instantiation:  vector<int> var1; functionName: member function name:
 *size params: function parameter list scope: the scope this function call
 *expression will be inserted into. Credit to prior transformer prototypes.
 */
ROSE_DLL_API SgFunctionCallExp *
buildMemberFunctionCall(std::string className, SgExpression *objectExpression,
                        std::string functionName, SgExprListExp *params,
                        SgScopeStatement *scope);

//! Build member function calls. objectExpression: the variable reference
//! expression to an object of template class instantiation:  vector<int> var1;
ROSE_DLL_API SgFunctionCallExp *
buildMemberFunctionCall(SgExpression *objectExpression,
                        SgMemberFunctionSymbol *functionSymbol,
                        SgExprListExp *params);

SgTypeTraitBuiltinOperator *buildTypeTraitBuiltinOperator(
    SgName functionName,
    SgTypeTraitBuiltinOperator::builtin_operator_kind_enum builtinKind,
    SgType *result_type, SgExpressionPtrList parameters);
ROSE_DLL_API SgTypeTraitBuiltinOperator *buildTypeTraitBuiltinOperator_nfi(
    SgName functionName,
    SgTypeTraitBuiltinOperator::builtin_operator_kind_enum builtinKind,
    SgType *result_type, SgExpressionPtrList parameters);

//! Build a CUDA kernel call expression (kernel<<<config>>>(parameters))
SgCudaKernelCallExp *
buildCudaKernelCallExp_nfi(SgExpression *kernel,
                           SgExprListExp *parameters = NULL,
                           SgCudaKernelExecConfig *config = NULL);

//! Build a CUDA kernel execution configuration (<<<grid, blocks, shared,
//! stream>>>)
SgCudaKernelExecConfig *buildCudaKernelExecConfig_nfi(
    SgExpression *grid = NULL, SgExpression *blocks = NULL,
    SgExpression *shared = NULL, SgExpression *stream = NULL);

//! Build the rhs of a variable declaration which includes an assignment
ROSE_DLL_API SgAssignInitializer *
buildAssignInitializer(SgExpression *operand_i, SgType *expression_type);
ROSE_DLL_API SgAssignInitializer *
buildAssignInitializer_nfi(SgExpression *operand_i, SgType *expression_type);

//! Build an aggregate initializer
ROSE_DLL_API SgAggregateInitializer *buildAggregateInitializer(
    SgExprListExp *initializers, SgType *type,
    SgAggregateInitializer::aggregate_initializer_source_form_enum source_form);
ROSE_DLL_API SgAggregateInitializer *buildAggregateInitializer_nfi(
    SgExprListExp *initializers, SgType *type,
    SgAggregateInitializer::aggregate_initializer_source_form_enum source_form);

ROSE_DLL_API SgConstructorInitializer *buildConstructorInitializer(
    SgMemberFunctionDeclaration *declaration, SgExprListExp *args,
    SgType *expression_type, bool need_name, bool need_qualifier,
    bool need_parenthesis_after_name, bool associated_class_unknown);
ROSE_DLL_API SgConstructorInitializer *buildConstructorInitializer_nfi(
    SgMemberFunctionDeclaration *declaration, SgExprListExp *args,
    SgType *expression_type, bool need_name, bool need_qualifier,
    bool need_parenthesis_after_name, bool associated_class_unknown,
    SgConstructorInitializer::argument_source_enum argument_source =
        SgConstructorInitializer::e_constructor_argument_expressions);

//! Build an braced initializer
ROSE_DLL_API SgBracedInitializer *
buildBracedInitializer(SgExprListExp *initializers, SgType *expression_type);
ROSE_DLL_API SgBracedInitializer *
buildBracedInitializer_nfi(SgExprListExp *initializers,
                           SgType *expression_type);

//! Build sizeof() expression with an expression parameter
ROSE_DLL_API SgSizeOfOp *buildSizeOfOp(
    SgExpression *exp, SgType *expression_type,
    SgExpression::unary_type_trait_kind_enum operator_kind =
        SgExpression::e_sizeof,
    std::optional<Rose::Integer128Value> evaluated_value = std::nullopt);
ROSE_DLL_API SgSizeOfOp *buildSizeOfOp_nfi(
    SgExpression *exp, SgType *expression_type,
    SgExpression::unary_type_trait_kind_enum operator_kind =
        SgExpression::e_sizeof,
    std::optional<Rose::Integer128Value> evaluated_value = std::nullopt);

//! Build sizeof() expression with a type parameter
ROSE_DLL_API SgSizeOfOp *buildSizeOfOp(
    SgType *type, SgType *expression_type,
    SgExpression::unary_type_trait_kind_enum operator_kind =
        SgExpression::e_sizeof,
    std::optional<Rose::Integer128Value> evaluated_value = std::nullopt);
ROSE_DLL_API SgSizeOfOp *buildSizeOfOp_nfi(
    SgType *type, SgType *expression_type,
    SgExpression::unary_type_trait_kind_enum operator_kind =
        SgExpression::e_sizeof,
    std::optional<Rose::Integer128Value> evaluated_value = std::nullopt);

//! Build __alignof__() expression with an expression parameter
ROSE_DLL_API SgAlignOfOp *buildAlignOfOp(
    SgExpression *exp, SgType *expression_type,
    SgExpression::unary_type_trait_kind_enum operator_kind =
        SgExpression::e_preferred_alignof,
    std::optional<Rose::Integer128Value> evaluated_value = std::nullopt);
ROSE_DLL_API SgAlignOfOp *buildAlignOfOp_nfi(
    SgExpression *exp, SgType *expression_type,
    SgExpression::unary_type_trait_kind_enum operator_kind =
        SgExpression::e_preferred_alignof,
    std::optional<Rose::Integer128Value> evaluated_value = std::nullopt);

//! Build __alignof__() expression with a type parameter
ROSE_DLL_API SgAlignOfOp *buildAlignOfOp(
    SgType *type, SgType *expression_type,
    SgExpression::unary_type_trait_kind_enum operator_kind =
        SgExpression::e_preferred_alignof,
    std::optional<Rose::Integer128Value> evaluated_value = std::nullopt);
ROSE_DLL_API SgAlignOfOp *buildAlignOfOp_nfi(
    SgType *type, SgType *expression_type,
    SgExpression::unary_type_trait_kind_enum operator_kind =
        SgExpression::e_preferred_alignof,
    std::optional<Rose::Integer128Value> evaluated_value = std::nullopt);

//! Build noexcept operator expression with an expression parameter
ROSE_DLL_API SgNoexceptOp *buildNoexceptOp(SgExpression *exp);
ROSE_DLL_API SgNoexceptOp *buildNoexceptOp_nfi(SgExpression *exp);

ROSE_DLL_API SgTypeExpression *buildTypeExpression(SgType *type);
ROSE_DLL_API SgTypeExpression *buildTypeExpression_nfi(SgType *type);

ROSE_DLL_API SgFunctionParameterRefExp *
buildFunctionParameterRefExp(int parameter_number, int parameter_level,
                             SgType *parameter_type);
ROSE_DLL_API SgFunctionParameterRefExp *
buildFunctionParameterRefExp_nfi(int parameter_number, int parameter_level,
                                 SgType *parameter_type);

ROSE_DLL_API SgLambdaExp *
buildLambdaExp_nfi(SgLambdaCaptureList *lambda_capture_list,
                   SgClassDeclaration *lambda_closure_class,
                   SgFunctionDeclaration *lambda_function);

ROSE_DLL_API SgLambdaCapture *buildLambdaCapture_nfi(
    SgExpression *capture_variable, SgExpression *capture_initializer,
    SgExpression *closure_variable, bool capture_by_reference, bool implicit,
    bool pack_expansion, bool is_init_capture);

ROSE_DLL_API SgLambdaCaptureList *buildLambdaCaptureList_nfi();

ROSE_DLL_API SgFoldExpression *
buildFoldExpression_nfi(SgExpression *operands,
                        SgFoldExpression::operator_kind_enum operator_kind,
                        bool is_left_associative, SgType *result_type);

ROSE_DLL_API SgAwaitExpression *buildAwaitExpression_nfi(
    SgExpression *value, SgType *result_type,
    SgAwaitExpression::coroutine_keyword_kind_enum keyword);

ROSE_DLL_API SgPackExpansionExpr *
buildPackExpansionExpr_nfi(SgExpression *pattern, SgType *result_type);

ROSE_DLL_API SgChooseExpression *
buildChooseExpression_nfi(SgExpression *condition,
                          SgExpression *true_expression,
                          SgExpression *false_expression, SgType *result_type,
                          SgChooseExpression::selection_kind_enum selection);
ROSE_DLL_API SgRequiresExpr *buildRequiresExpr(
    SgFunctionParameterList *local_parameter_list, SgExprListExp *requirements,
    SgFunctionParameterScope *local_parameter_scope = nullptr,
    SgFunctionParameterList *local_parameter_list_syntax = nullptr);
ROSE_DLL_API SgRequiresExpr *buildRequiresExpr_nfi(
    SgFunctionParameterList *local_parameter_list, SgExprListExp *requirements,
    SgFunctionParameterScope *local_parameter_scope = nullptr,
    SgFunctionParameterList *local_parameter_list_syntax = nullptr);
ROSE_DLL_API SgSimpleRequirement *
buildSimpleRequirement_nfi(SgExpression *expression);
ROSE_DLL_API SgTypeRequirement *buildTypeRequirement_nfi(SgType *required_type);
ROSE_DLL_API SgCompoundRequirement *
buildCompoundRequirement_nfi(SgExpression *expression, bool noexcept_required,
                             SgExpression *type_constraint);
ROSE_DLL_API SgRequirementSubstitutionFailure *
buildRequirementSubstitutionFailure_nfi(
    SgRequirementSubstitutionFailure::failure_kind_enum failure_kind,
    const std::string &substituted_entity,
    const std::string &diagnostic_message);
ROSE_DLL_API SgNestedRequirement *
buildNestedRequirement_nfi(SgExpression *constraint);

//@}

//@{
/*! @name Builders for range expressions
 */
ROSE_DLL_API SgRangeExp *buildRangeExp(SgExpression *start, SgExpression *end,
                                       SgExpression *stride);
SgRangeExp *buildRangeExp_nfi(SgExpression *start, SgExpression *end,
                              SgExpression *stride);

//@}
//
//--------------------------------------------------------------
//@{
/*! @name Builders for support nodes
  \brief AST high level builders for SgSupport nodes

*/
//! Initialized names are tricky, their scope vary depending on context, so
//! scope and symbol information are not needed until the initialized name is
//! being actually used somewhere.

/*!e.g the scope of arguments of functions are different for defining and
 * nondefining functions.
 */
ROSE_DLL_API SgInitializedName *
buildInitializedName(const SgName &name, SgType *type,
                     SgInitializer *init = NULL);
ROSE_DLL_API SgInitializedName *buildInitializedName(const std::string &name,
                                                     SgType *type);
ROSE_DLL_API SgInitializedName *buildInitializedName(const char *name,
                                                     SgType *type);
ROSE_DLL_API SgInitializedName *
buildInitializedName_nfi(const SgName &name, SgType *type, SgInitializer *init,
                         SgVariableDeclaration *declptr = NULL);
//! Build an initialized name that has no independently written source surface.
//! The result is classified and validated as exact frontend semantic output at
//! this producer; callers must not repair its provenance after attachment.
ROSE_DLL_API SgInitializedName *
buildSemanticInitializedName(const SgName &name, SgType *type,
                             SgInitializer *init = NULL,
                             SgVariableDeclaration *declptr = NULL);

//! Derive a callable parameter type from a validated declared type without
//! mutating the declaration's type.
ROSE_DLL_API SgType *functionParameterTypeFromDeclaredType(SgType *type);

//! Build SgFunctionParameterTypeList from SgFunctionParameterList
ROSE_DLL_API SgFunctionParameterTypeList *
buildFunctionParameterTypeList(SgFunctionParameterList *paralist);

//! Build SgFunctionParameterTypeList from an expression list, useful when
//! building a function call
ROSE_DLL_API SgFunctionParameterTypeList *
buildFunctionParameterTypeList(SgExprListExp *expList);
SgFunctionParameterTypeList *
buildFunctionParameterTypeList_nfi(SgExprListExp *expList);

//! Build an SgFunctionParameterTypeList from SgTypes. To build an
ROSE_DLL_API SgFunctionParameterTypeList *
buildFunctionParameterTypeList(SgType *type0 = NULL, SgType *type1 = NULL,
                               SgType *type2 = NULL, SgType *type3 = NULL,
                               SgType *type4 = NULL, SgType *type5 = NULL,
                               SgType *type6 = NULL, SgType *type7 = NULL);

//--------------------------------------------------------------
//@{
/*! @name Builders for statements
  \brief AST high level builders for SgStatement, explicit scope parameters are
  allowed for flexibility. Please use SageInterface::appendStatement(),
  prependStatement(), and insertStatement() to attach the newly built statements
  into an AST tree. Calling member functions like
  SgScopeStatement::prepend_statement() or using container functions such as
  pushback() is discouraged since they don't handle many side effects for symbol
  tables, source file information, scope and parent pointers etc.

*/

//! Build a fresh, structurally detached variable declaration in an exact
//! semantic scope.  A same-scope declaration with the same name is a hard
//! error; use buildVariableRedeclaration() when the prior declaration is known.
//! An explicit C/C++ written type must be distinct and semantically equivalent;
//! it is attached before the declaration's variable symbol is published.
ROSE_DLL_API SgVariableDeclaration *
buildVariableDeclaration(const SgName &name, SgType *type,
                         SgInitializer *varInit, SgScopeStatement *scope,
                         SgType *cxxSourceType = nullptr);

ROSE_DLL_API SgVariableDeclaration *
buildVariableDeclaration(const std::string &name, SgType *type,
                         SgInitializer *varInit, SgScopeStatement *scope,
                         SgType *cxxSourceType = nullptr);

ROSE_DLL_API SgVariableDeclaration *
buildVariableDeclaration(const char *name, SgType *type, SgInitializer *varInit,
                         SgScopeStatement *scope,
                         SgType *cxxSourceType = nullptr);

//! Build one source-positioned variable declaration without publishing a
//! symbol.  This is an internal transaction primitive: the caller must
//! establish exact lexical ownership and publish the symbol before returning
//! the declaration to application code.
ROSE_DLL_API SgVariableDeclaration *
buildUnpublishedVariableDeclaration(const SgName &name, SgType *type,
                                    SgInitializer *varInit,
                                    SgScopeStatement *scope);

//! Build a fresh variable declaration with null frontend source positions.
ROSE_DLL_API SgVariableDeclaration *buildVariableDeclaration_nfi(
    const SgName &name, SgType *type, SgInitializer *varInit,
    SgScopeStatement *scope,
    SgStorageModifier::storage_modifier_enum sm = SgStorageModifier::e_default);

//! Classify a freshly built semantic-only variable declaration and every
//! structurally owned located child with exact compiler-generated provenance.
//! Source-positioned or partially positioned input is malformed and aborts.
ROSE_DLL_API void initializeSemanticVariableDeclarationSourceProvenance(
    SgVariableDeclaration *declaration);

//! Build and publish one named semantic-only variable declaration under the
//! scope's exact auxiliary owner.  Structural ownership is established before
//! the variable symbol is inserted, so copied scope symbol tables can map the
//! symbol only to an attached declaration basis.
ROSE_DLL_API SgVariableDeclaration *
buildSemanticAuxiliaryVariableDeclaration(const SgName &name, SgType *type,
                                          SgInitializer *varInit,
                                          SgScopeStatement *scope);

//! Complete the provenance of a detached frontend source declaration before
//! its exact declaration range is known. Fresh structural wrappers receive
//! semantic generated provenance while already source-positioned expression
//! operands retain their exact immutable provenance.
ROSE_DLL_API void initializePendingSourceVariableDeclarationProvenance(
    SgVariableDeclaration *declaration);
//! Classify one fresh semantic-only expression with complete compiler-generated
//! provenance. Source-positioned or partially positioned input is malformed.
ROSE_DLL_API void
initializeSemanticExpressionSourceProvenance(SgExpression *expression);

//! Build a fresh, detached redeclaration with an explicit prior identity.
ROSE_DLL_API SgVariableDeclaration *
buildVariableRedeclaration(const SgName &name, SgType *type,
                           SgInitializer *varInit, SgScopeStatement *scope,
                           SgInitializedName *priorDeclaration);

//! Build a fresh, detached redeclaration with null frontend source positions.
ROSE_DLL_API SgVariableDeclaration *buildVariableRedeclaration_nfi(
    const SgName &name, SgType *type, SgInitializer *varInit,
    SgScopeStatement *scope, SgInitializedName *priorDeclaration,
    SgStorageModifier::storage_modifier_enum sm = SgStorageModifier::e_default);

//! Build a fresh variable declaration which is the exact physical owner of a
//! source-spelled class/enum definition embedded in its declarator.
ROSE_DLL_API SgVariableDeclaration *buildVariableDeclarationWithEmbeddedTag(
    const SgName &name, SgType *type, SgInitializer *varInit,
    SgScopeStatement *scope, SgInitializedName *priorDeclaration,
    SgDeclarationStatement *embeddedDefinition);

//! Build an embedded-tag variable declaration with null frontend positions.
ROSE_DLL_API SgVariableDeclaration *buildVariableDeclarationWithEmbeddedTag_nfi(
    const SgName &name, SgType *type, SgInitializer *varInit,
    SgScopeStatement *scope, SgInitializedName *priorDeclaration,
    SgDeclarationStatement *embeddedDefinition,
    SgStorageModifier::storage_modifier_enum sm = SgStorageModifier::e_default);

//! Build an embedded-tag variable subtree without publishing its symbol.
//! The frontend must attach exact redeclaration identity and publish the symbol
//! in the same construction transaction.
ROSE_DLL_API SgVariableDeclaration *
buildUnpublishedVariableDeclarationWithEmbeddedTag_nfi(
    const SgName &name, SgType *type, SgInitializer *varInit,
    SgScopeStatement *scope, SgDeclarationStatement *embeddedDefinition,
    SgStorageModifier::storage_modifier_enum sm = SgStorageModifier::e_default);

//! Build variable definition
ROSE_DLL_API SgVariableDefinition *
buildVariableDefinition_nfi(SgVariableDeclaration *decl,
                            SgInitializedName *init_name, SgInitializer *init);

class declaration_ownership;
ROSE_DLL_API SgTemplateVariableDeclaration *
buildTemplateVariableDeclaration_nfi(
    const SgName &name, SgType *type, SgInitializer *varInit,
    SgScopeStatement *scope,
    SgTemplateVariableDeclaration *specializedTemplateDeclaration,
    const SgTemplateArgumentPtrList &semanticArguments,
    SgInitializedName *priorDeclaration = nullptr);

//! Build and atomically publish a template variable declaration with explicit
//! structural ownership. The semantic scope is supplied separately;
//! sourceLexicalIn() represents an out-of-class definition without changing
//! the variable entity's semantic class scope.
ROSE_DLL_API SgTemplateVariableDeclaration *buildTemplateVariableDeclaration(
    const declaration_ownership &ownership, const SgName &name, SgType *type,
    SgInitializer *varInit, SgScopeStatement *scope,
    SgTemplateVariableDeclaration *specializedTemplateDeclaration,
    const SgTemplateArgumentPtrList &semanticArguments,
    SgInitializedName *priorDeclaration = nullptr);

//! Build a typedef declaration, such as: typedef int myint;  typedef struct A
//! {..} s_A;
class ROSE_DLL_API typedef_declaration_ownership {
public:
  enum class kind {
    source_lexical,
    source_lexical_pending_publication,
    source_typed_child,
    semantic_lexical,
    semantic_auxiliary,
    source_auxiliary_pending_exact_source,
    source_group_member,
    semantic_group_member
  };

  static typedef_declaration_ownership sourceLexical();
  //! Construct and publish the semantic identity of a transformation-owned
  //! source typedef while leaving its lexical root detached for an enclosing
  //! insertion transaction.
  static typedef_declaration_ownership sourceLexicalPendingPublication();
  //! Keep a source declaration detached while its enclosing statement
  //! producer constructs the declaration's sole typed child edge.  The
  //! semantic scope passed to the builder still owns the declaration symbol.
  static typedef_declaration_ownership sourceTypedChild();
  //! Attach a semantic-only declaration to a semantic body in statement order.
  //! Unlike semanticAuxiliary(), this is body structure rather than a
  //! scope-visible dependency with no lexical child edge.
  static typedef_declaration_ownership semanticLexical();
  static typedef_declaration_ownership semanticAuxiliary();
  //! Publish a source-written typedef's semantic identity before its ordinary
  //! lexical traversal.  The declaration remains auxiliary-owned and has no
  //! source provenance until the exact Clang source occurrence completes the
  //! transaction.
  static typedef_declaration_ownership sourceAuxiliaryPendingExactSource();
  static typedef_declaration_ownership
  sourceGroupMember(SgDeclarationGroupStatement *owner);
  static typedef_declaration_ownership
  semanticGroupMember(SgDeclarationGroupStatement *owner);

  kind getKind() const;
  SgDeclarationGroupStatement *getDeclarationGroupOwner() const;

private:
  typedef_declaration_ownership(kind ownership_kind,
                                SgDeclarationGroupStatement *group_owner);

  kind ownership_kind_;
  SgDeclarationGroupStatement *group_owner_;
};

ROSE_DLL_API SgTypedefDeclaration *
buildTypedefDeclaration(const typedef_declaration_ownership &ownership,
                        SgTypedefDeclaration::typedef_type_enum declarationForm,
                        const std::string &name, SgType *base_type,
                        SgScopeStatement *scope);

ROSE_DLL_API SgTypedefDeclaration *buildTypedefDeclaration_nfi(
    const typedef_declaration_ownership &ownership,
    SgTypedefDeclaration::typedef_type_enum declarationForm,
    const std::string &name, SgType *base_type, SgScopeStatement *scope);

ROSE_DLL_API SgTypedefDeclaration *buildTypedefDeclarationWithEmbeddedTag(
    const typedef_declaration_ownership &ownership,
    SgTypedefDeclaration::typedef_type_enum declarationForm,
    const std::string &name, SgType *base_type, SgScopeStatement *scope,
    SgDeclarationStatement *embeddedDefinition);

ROSE_DLL_API SgTypedefDeclaration *buildTypedefDeclarationWithEmbeddedTag_nfi(
    const typedef_declaration_ownership &ownership,
    SgTypedefDeclaration::typedef_type_enum declarationForm,
    const std::string &name, SgType *base_type, SgScopeStatement *scope,
    SgDeclarationStatement *embeddedDefinition);

ROSE_DLL_API SgTemplateTypedefDeclaration *buildTemplateTypedefDeclaration_nfi(
    const typedef_declaration_ownership &ownership,
    SgTypedefDeclaration::typedef_type_enum declarationForm, const SgName &name,
    SgType *base_type, SgScopeStatement *scope);

ROSE_DLL_API SgTemplateInstantiationTypedefDeclaration *
buildTemplateInstantiationTypedefDeclaration_nfi(
    const typedef_declaration_ownership &ownership,
    SgTypedefDeclaration::typedef_type_enum declarationForm, SgName &name,
    SgType *base_type, SgScopeStatement *scope,
    SgTemplateTypedefDeclaration *templateTypedefDeclaration,
    SgTemplateArgumentPtrList &templateArgumentsList,
    const SgName &semanticInstantiationName);

//! Build an empty SgFunctionParameterList, possibly with some initialized names
//! filled in
ROSE_DLL_API SgFunctionParameterList *buildFunctionParameterList(
    SgInitializedName *in1 = NULL, SgInitializedName *in2 = NULL,
    SgInitializedName *in3 = NULL, SgInitializedName *in4 = NULL,
    SgInitializedName *in5 = NULL, SgInitializedName *in6 = NULL,
    SgInitializedName *in7 = NULL, SgInitializedName *in8 = NULL,
    SgInitializedName *in9 = NULL, SgInitializedName *in10 = NULL);
SgFunctionParameterList *buildFunctionParameterList_nfi();

//! Build an independent semantic-only signature from an existing parameter
//! list. Default arguments remain owned by the source declaration; the result
//! contains fresh parameter identities with the same names, types, pack roles,
//! and qualification metadata.
ROSE_DLL_API SgFunctionParameterList *buildSemanticFunctionParameterList(
    const SgFunctionParameterList *sourceParameters);

//! Build an independent generated prototype signature from an existing
//! parameter list. Default arguments remain owned by the source declaration;
//! the result contains fresh transformation identities with the same names,
//! types, pack roles, and qualification metadata. This is the generated-source
//! counterpart to buildSemanticFunctionParameterList().
ROSE_DLL_API SgFunctionParameterList *buildGeneratedFunctionParameterList(
    const SgFunctionParameterList *sourceParameters);

//! Build an SgFunctionParameterList from SgFunctionParameterTypeList, like
//! (int, float,...), used for parameter list of prototype functions when
//! function type( including parameter type list) is known.
ROSE_DLL_API SgFunctionParameterList *
buildFunctionParameterList(SgFunctionParameterTypeList *paraTypeList);

ROSE_DLL_API SgFunctionParameterList *
buildFunctionParameterList_nfi(SgFunctionParameterTypeList *paraTypeList);

SgCtorInitializerList *buildCtorInitializerList_nfi();
ROSE_DLL_API void
setTemplateNameInTemplateInstantiations(SgFunctionDeclaration *func,
                                        const SgName &name);

ROSE_DLL_API void setTemplateArgumentParents(SgDeclarationStatement *decl);
ROSE_DLL_API void setTemplateArgumentParents(SgNonrealRefExp *ref);
ROSE_DLL_API void testTemplateArgumentParents(SgDeclarationStatement *decl);
ROSE_DLL_API SgTemplateArgumentPtrList *
getTemplateArgumentList(SgDeclarationStatement *decl);

ROSE_DLL_API void testTemplateParameterParents(SgDeclarationStatement *decl);
ROSE_DLL_API void setTemplateParameterParents(SgDeclarationStatement *decl);
ROSE_DLL_API SgTemplateParameterPtrList *
getTemplateParameterList(SgDeclarationStatement *decl);

ROSE_DLL_API void setTemplateArgumentsInDeclaration(
    SgDeclarationStatement *decl,
    SgTemplateArgumentPtrList *templateArgumentsList_input);
ROSE_DLL_API void setTemplateSpecializationArgumentsInDeclaration(
    SgDeclarationStatement *decl,
    SgTemplateArgumentPtrList *templateSpecializationArgumentsList_input);
ROSE_DLL_API void setTemplateParametersInDeclaration(
    SgDeclarationStatement *decl,
    SgTemplateParameterPtrList *templateParametersList_input);

//! Exact, typed structural ownership established by function-declaration
//! builders.  Invalid kind/owner combinations cannot be constructed.
class ROSE_DLL_API function_declaration_ownership {
public:
  enum class semantic_lookup_publication {
    required,
    prohibited_for_rejected_template_instantiation
  };

  //! Function identity known before declaration and symbol publication.
  enum class special_member_kind {
    none,
    constructor,
    destructor,
    overloaded_operator,
    conversion,
    literal_operator,
    deduction_guide
  };

  struct explicit_specifier {
    bool is_explicit;
    SgExpression *condition;
  };

  struct special_member_properties {
    bool is_default;
    bool is_trivial;
    bool is_ineligible;
  };

  struct deletion_properties {
    bool is_deleted;
    bool is_written;
  };

  struct method_properties {
    bool is_virtual;
    bool is_pure;
    bool is_final;
    bool is_override;
    SgFunctionModifier::copy_move_kind_enum copy_move_kind =
        SgFunctionModifier::e_copy_move_none;
  };

  enum class kind {
    source_lexical,
    source_lexical_redeclaration,
    source_lexical_pending_exact_source,
    source_lexical_pending_exact_source_redeclaration,
    source_lexical_pending_friend_specialization,
    source_lexical_canonical_replacement,
    source_lexical_generated_canonical_replacement,
    semantic_auxiliary,
    semantic_auxiliary_redeclaration,
    semantic_auxiliary_canonical_replacement,
    semantic_auxiliary_pending_exact_source,
    source_replacement,
    source_group_member,
    source_group_redeclaration,
    semantic_group_member,
    source_instantiation_directive_member,
    source_interface_body,
    source_statement_function
  };

  static function_declaration_ownership sourceLexical();
  static function_declaration_ownership
  sourceLexicalIn(SgScopeStatement *lexicalOwner);
  //! Publish a distinct source declaration in its exact lexical owner while
  //! joining an already published canonical function family.  The target
  //! remains the canonical declaration and its one direct symbol is reused;
  //! no name/type lookup is permitted to select another overload family.
  static function_declaration_ownership
  sourceLexicalRedeclarationIn(SgScopeStatement *lexicalOwner,
                               SgFunctionDeclaration *target);
  //! Construct a frontend source declaration with its exact semantic scope,
  //! but keep it physically detached until the frontend has published the
  //! immutable source range and translation-unit order.  This transaction is
  //! valid only in frontend-construction mode.
  static function_declaration_ownership
  sourceLexicalPendingExactSourceIn(SgScopeStatement *lexicalOwner);
  //! Construct a distinct source redeclaration in its exact lexical owner,
  //! join an already published canonical function family, and leave the
  //! declaration subtree unclassified until the frontend publishes its exact
  //! source ranges.  This is the source/redeclaration transaction for a
  //! declaration whose lexical and semantic owners differ, such as a
  //! qualified friend member declaration.
  static function_declaration_ownership
  sourceLexicalPendingExactSourceRedeclarationIn(SgScopeStatement *lexicalOwner,
                                                 SgFunctionDeclaration *target);
  //! Construct one dependent friend template-id source surface in its exact
  //! class lexical owner and namespace semantic scope.  Clang has not selected
  //! a concrete specialization declaration in this state, so the node carries
  //! its typed primary-template identity and deliberately publishes no
  //! independent function symbol or redeclaration-chain edge.
  static function_declaration_ownership
  sourceLexicalPendingFriendSpecializationIn(SgScopeStatement *lexicalOwner);
  //! Publish a source declaration in its exact lexical owner while replacing
  //! a later source redeclaration that was necessarily constructed first by a
  //! re-entrant frontend translation.  The replacement target remains in its
  //! own lexical source position; only the canonical declaration/symbol basis
  //! is re-rooted to the earlier source declaration.
  static function_declaration_ownership
  sourceLexicalCanonicalReplacementIn(SgScopeStatement *lexicalOwner,
                                      SgFunctionDeclaration *target);
  //! Publish a generated declaration immediately before an exact source
  //! anchor and make it the canonical declaration/symbol basis of an existing
  //! declaration family.  The target remains in its original lexical owner.
  static function_declaration_ownership
  sourceLexicalCanonicalReplacementBefore(SgScopeStatement *lexicalOwner,
                                          SgStatement *anchor,
                                          SgFunctionDeclaration *target);
  //! Publish a generated declaration at the beginning of an exact lexical
  //! owner and make it the canonical declaration/symbol basis of an existing
  //! declaration family.
  static function_declaration_ownership
  sourceLexicalCanonicalReplacementAtTop(SgScopeStatement *lexicalOwner,
                                         SgFunctionDeclaration *target);
  static function_declaration_ownership
  sourceLexicalAtTop(SgScopeStatement *lexicalOwner);
  static function_declaration_ownership
  sourceLexicalBefore(SgScopeStatement *lexicalOwner, SgStatement *anchor);
  static function_declaration_ownership semanticAuxiliary();
  //! Publish a distinct semantic declaration in the exact auxiliary owner of
  //! an already published function family.  The target remains canonical and
  //! its direct symbol is reused without name/type lookup.
  static function_declaration_ownership
  semanticAuxiliaryRedeclarationOf(SgFunctionDeclaration *target);
  //! Publish a distinct semantic declaration while atomically re-rooting an
  //! already published semantic declaration family and its one direct symbol.
  //! The replacement target must be exactly auxiliary-owned in the same
  //! semantic scope.
  static function_declaration_ownership
  semanticAuxiliaryCanonicalReplacementOf(SgFunctionDeclaration *target);
  //! Publish through semantic auxiliary ownership while leaving the complete
  //! function declaration subtree unclassified for one exact frontend source
  //! publication. This transaction is valid only in frontend-construction
  //! mode and must not be used for defining declarations.
  static function_declaration_ownership semanticAuxiliaryPendingExactSource();
  static function_declaration_ownership
  sourceReplacementOf(SgFunctionDeclaration *target);
  static function_declaration_ownership
  sourceGroupMember(SgDeclarationGroupStatement *owner);
  //! Publish a source declaration-group member while joining an already
  //! published exact function family.  The group remains the structural owner
  //! and the target supplies the canonical declaration/symbol identity.
  static function_declaration_ownership
  sourceGroupRedeclaration(SgDeclarationGroupStatement *owner,
                           SgFunctionDeclaration *target);
  static function_declaration_ownership
  semanticGroupMember(SgDeclarationGroupStatement *owner);
  static function_declaration_ownership sourceInstantiationDirectiveMember(
      SgTemplateInstantiationDirectiveStatement *owner);
  static function_declaration_ownership
  sourceInterfaceBody(SgInterfaceBody *owner);
  static function_declaration_ownership
  sourceStatementFunction(SgStatementFunctionStatement *owner);

  //! Preserve a source function declarator whose complete type comes from a
  //! typeof specifier and therefore owns semantic parameters but no written
  //! parameter-list child.  This is a construction contract, not an unparser
  //! recovery mode.
  function_declaration_ownership
  withSemanticParameterListForWrappedFunctionDeclarator() const;
  //! Make a translated trailing requires-clause part of the function overload
  //! identity before any declaration or symbol is constructed.  The expression
  //! must be detached; the builder transfers its ownership to the declaration
  //! it constructs.
  function_declaration_ownership
  withTrailingRequiresClause(SgExpression *constraint) const;
  //! Make a translated template-head requires-clause part of a function
  //! template's overload identity before its declaration and symbol are
  //! constructed.  The expression must be detached and is transferred to the
  //! newly constructed template function declaration.
  function_declaration_ownership
  withTemplateRequiresClause(SgExpression *constraint) const;
  //! Retain a semantically rejected template instantiation in the auxiliary
  //! AST while prohibiting construction of a lookup symbol.  The builder
  //! validates that this contract is used only for semantic template
  //! instantiations; it never publishes and then removes a symbol.
  function_declaration_ownership
  withoutSemanticLookupForRejectedTemplateInstantiation() const;
  //! Add exact special-member identity to a function construction
  //! transaction.  This is valid only for member-function builders and cannot
  //! be changed once present.
  function_declaration_ownership
  withSpecialMemberKind(special_member_kind specialKind) const;
  function_declaration_ownership
  withMethodProperties(method_properties properties) const;
  std::optional<method_properties> getMethodProperties() const;
  function_declaration_ownership withDeletionProperties(bool is_deleted,
                                                        bool is_written) const;
  std::optional<deletion_properties> getDeletionProperties() const;
  function_declaration_ownership
  withSpecialMemberProperties(bool is_default, bool is_trivial,
                              bool is_ineligible) const;
  std::optional<special_member_properties> getSpecialMemberProperties() const;
  function_declaration_ownership withDestructorProperties(
      SgFunctionModifier::destructor_properties_enum properties) const;
  SgFunctionModifier::destructor_properties_enum
  getDestructorProperties() const;
  //! Supply an independent, detached written parameter list to a function
  //! builder; it constructs and owns the syntax signature before publication.
  function_declaration_ownership
  withSourceParameterList(SgFunctionParameterList *parameters) const;
  SgFunctionParameterList *getSourceParameterList() const;
  //! Transfer the declarator scope and source signature during construction.
  //! Written parameters must already be bound to this declarator scope.
  function_declaration_ownership withFunctionDeclaratorScope(
      SgDeclarationScope *scope, SgFunctionType *source_type = nullptr,
      SgFunctionParameterList *source_parameters = nullptr,
      bool old_style_definition = false) const;
  SgDeclarationScope *getFunctionDeclaratorScope() const;
  SgFunctionType *getSourceFunctionType() const;
  bool getOldStyleDefinition() const;
  //! Supply the specifier before publication. Without an explicit value,
  //! redeclarations inherit their exact canonical declaration's specifier.
  function_declaration_ownership withConstexprSpecifier(
      SgFunctionDeclaration::constexpr_specifier_enum specifier) const;
  std::optional<SgFunctionDeclaration::constexpr_specifier_enum>
  getConstexprSpecifier() const;
  //! Transfer a detached condition and its resolved semantic value before
  //! publication. Absence of an input inherits the exact canonical specifier.
  function_declaration_ownership
  withExplicitSpecifier(bool isExplicit, SgExpression *condition) const;
  std::optional<explicit_specifier> getExplicitSpecifier() const;
  //! Supply a complete canonical signature before declaration publication.
  function_declaration_ownership withFunctionType(SgFunctionType *type) const;
  SgFunctionType *getFunctionType() const;
  function_declaration_ownership
  withDeclaredExceptionSignature(SgFunctionType *signature) const;
  SgFunctionType *getDeclaredExceptionSignature() const;
  //! Retarget an in-progress redeclaration or canonical-replacement
  //! transaction after re-entrant construction has re-rooted the same exact
  //! function family.  All other typed construction properties are preserved.
  //! The replacement must be the family's self-canonical declaration.
  function_declaration_ownership
  withCanonicalFunctionTarget(SgFunctionDeclaration *target) const;
  //! Return the exact symbol-table key for this construction transaction.
  SgName semanticSymbolTableKey(const SgName &sourceName) const;

  kind getKind() const;
  semantic_lookup_publication getSemanticLookupPublication() const;
  special_member_kind getSpecialMemberKind() const;
  bool sourceDeclaratorUsesWrappedFunctionType() const;
  SgExpression *getTrailingRequiresClause() const;
  SgExpression *getTemplateRequiresClause() const;
  SgScopeStatement *getSourceLexicalOwner() const;
  SgStatement *getSourceInsertionAnchor() const;
  bool getSourceInsertionAtTop() const;
  SgFunctionDeclaration *getSourceReplacementTarget() const;
  SgDeclarationGroupStatement *getDeclarationGroupOwner() const;
  SgTemplateInstantiationDirectiveStatement *
  getSourceInstantiationDirectiveOwner() const;
  SgInterfaceBody *getSourceInterfaceBodyOwner() const;
  SgStatementFunctionStatement *getSourceStatementFunctionOwner() const;

private:
  void validate() const;
  function_declaration_ownership(
      kind ownership_kind, SgScopeStatement *lexical_owner,
      SgStatement *insertion_anchor, bool insertion_at_top,
      SgFunctionDeclaration *replacement_target,
      SgDeclarationGroupStatement *group_owner,
      SgTemplateInstantiationDirectiveStatement *instantiation_owner,
      SgInterfaceBody *interface_body_owner,
      SgStatementFunctionStatement *statement_function_owner,
      bool source_declarator_uses_wrapped_function_type = false,
      SgExpression *trailing_requires_clause = nullptr,
      SgExpression *template_requires_clause = nullptr,
      semantic_lookup_publication lookup_publication =
          semantic_lookup_publication::required,
      special_member_kind special_kind = special_member_kind::none,
      std::optional<SgFunctionDeclaration::constexpr_specifier_enum>
          constexpr_specifier = std::nullopt,
      SgFunctionParameterList *source_parameter_list = nullptr,
      std::optional<explicit_specifier> explicit_specifier = std::nullopt);

  kind ownership_kind_;
  SgScopeStatement *lexical_owner_;
  SgStatement *insertion_anchor_;
  bool insertion_at_top_;
  SgFunctionDeclaration *replacement_target_;
  SgDeclarationGroupStatement *group_owner_;
  SgTemplateInstantiationDirectiveStatement *instantiation_owner_;
  SgInterfaceBody *interface_body_owner_;
  SgStatementFunctionStatement *statement_function_owner_;
  bool source_declarator_uses_wrapped_function_type_;
  SgExpression *trailing_requires_clause_;
  SgExpression *template_requires_clause_;
  semantic_lookup_publication lookup_publication_;
  special_member_kind special_kind_;
  std::optional<method_properties> method_properties_;
  std::optional<deletion_properties> deletion_properties_;
  std::optional<special_member_properties> special_member_properties_;
  SgFunctionModifier::destructor_properties_enum destructor_properties_ =
      SgFunctionModifier::e_destructor_unclassified;
  std::optional<SgFunctionDeclaration::constexpr_specifier_enum>
      constexpr_specifier_;
  SgFunctionParameterList *source_parameter_list_;
  SgDeclarationScope *function_declarator_scope_ = nullptr;
  SgFunctionType *source_function_type_ = nullptr;
  bool old_style_definition_ = false;
  std::optional<explicit_specifier> explicit_specifier_;
  SgFunctionType *function_type_ = nullptr;
  SgFunctionType *declared_exception_signature_ = nullptr;
};

//! Build a prototype for a function, handle function type, symbol etc
//! transparently
ROSE_DLL_API SgFunctionDeclaration *buildNondefiningFunctionDeclaration(
    const function_declaration_ownership &ownership, const SgName &name,
    SgType *return_type, SgFunctionParameterList *parlist,
    SgScopeStatement *scope, bool buildTemplateInstantiation = false,
    SgTemplateArgumentPtrList *templateArgumentsList = NULL,
    SgStorageModifier::storage_modifier_enum sm = SgStorageModifier::e_default,
    bool forceFreeFunctionScope = false,
    const SgName *templateInstantiationName = NULL,
    SgDeclarationStatement *expectedTemplateDeclaration = NULL,
    SgTemplateArgumentPtrList *semanticTemplateArguments = nullptr);

//! Build a prototype for an existing function declaration (defining or
//! nondefining is fine)
ROSE_DLL_API SgFunctionDeclaration *buildNondefiningFunctionDeclaration(
    const function_declaration_ownership &ownership,
    const SgFunctionDeclaration *funcdecl, SgScopeStatement *scope);

// DQ (8/11/2013): Even though template functions can't use partial
// specialization, they can be specialized, however the specialization does not
// define a template and instead defines a template instantiation, so we don't
// need the SgTemplateArgumentPtrList in this function.
ROSE_DLL_API SgTemplateFunctionDeclaration *
buildNondefiningTemplateFunctionDeclaration(
    const function_declaration_ownership &ownership, const SgName &name,
    SgType *return_type, SgFunctionParameterList *parlist,
    SgScopeStatement *scope,
    SgTemplateParameterPtrList *templateParameterList = NULL);

ROSE_DLL_API SgTemplateFunctionDeclaration *
buildDefiningTemplateFunctionDeclaration(
    const function_declaration_ownership &ownership, const SgName &name,
    SgType *return_type, SgFunctionParameterList *parlist,
    SgScopeStatement *scope,
    SgTemplateFunctionDeclaration *first_nondefining_declaration,
    SgTemplateParameterPtrList *definingTemplateParameterList);

//! Build a prototype member function declaration
ROSE_DLL_API SgMemberFunctionDeclaration *
buildNondefiningMemberFunctionDeclaration(
    const function_declaration_ownership &ownership, const SgName &name,
    SgType *return_type, SgFunctionParameterList *parlist,
    SgScopeStatement *scope, unsigned int functionConstVolatileFlags,
    bool buildTemplateInstantiation,
    SgTemplateArgumentPtrList *templateArgumentsList,
    const SgName *templateInstantiationName = NULL,
    SgDeclarationStatement *expectedTemplateDeclaration = NULL,
    SgTemplateArgumentPtrList *semanticTemplateArguments = nullptr);

ROSE_DLL_API SgTemplateMemberFunctionDeclaration *
buildNondefiningTemplateMemberFunctionDeclaration(
    const function_declaration_ownership &ownership, const SgName &name,
    SgType *return_type, SgFunctionParameterList *parlist,
    SgScopeStatement *scope, unsigned int functionConstVolatileFlags,
    SgTemplateParameterPtrList *templateParameterList,
    SgTemplateParameterPtrList *symbolIdentityTemplateParameterList);

ROSE_DLL_API SgTemplateMemberFunctionDeclaration *
buildDefiningTemplateMemberFunctionDeclaration(
    const function_declaration_ownership &ownership, const SgName &name,
    SgType *return_type, SgFunctionParameterList *parlist,
    SgScopeStatement *scope, unsigned int functionConstVolatileFlags,
    SgTemplateMemberFunctionDeclaration *first_nondefing_declaration,
    SgTemplateParameterPtrList *definingTemplateParameterList);

//! Build a defining ( non-prototype) member function declaration
ROSE_DLL_API SgMemberFunctionDeclaration *
buildDefiningMemberFunctionDeclaration(
    const function_declaration_ownership &ownership, const SgName &name,
    SgType *return_type, SgFunctionParameterList *parlist,
    SgScopeStatement *scope, bool buildTemplateInstantiation,
    unsigned int functionConstVolatileFlags,
    SgMemberFunctionDeclaration *first_nondefinng_declaration,
    SgTemplateArgumentPtrList *templateArgumentsList);

ROSE_DLL_API SgMemberFunctionDeclaration *
buildNondefiningMemberFunctionDeclaration(
    const function_declaration_ownership &ownership, const SgName &name,
    SgType *return_type, SgFunctionParameterList *parameter_list,
    SgScopeStatement *scope);

ROSE_DLL_API SgMemberFunctionDeclaration *
buildDefiningMemberFunctionDeclaration(
    const function_declaration_ownership &ownership, const SgName &name,
    SgType *return_type, SgFunctionParameterList *parameter_list,
    SgScopeStatement *scope);

//! Build a function declaration with a function body
ROSE_DLL_API SgFunctionDeclaration *buildDefiningFunctionDeclaration(
    const function_declaration_ownership &ownership, const SgName &name,
    SgType *return_type, SgFunctionParameterList *parlist,
    SgScopeStatement *scope, bool buildTemplateInstantiation,
    SgFunctionDeclaration *first_nondefinng_declaration,
    SgTemplateArgumentPtrList *templateArgumentsList,
    bool forceFreeFunctionScope);

ROSE_DLL_API SgFunctionDeclaration *buildDefiningFunctionDeclaration(
    const function_declaration_ownership &ownership, const SgName &name,
    SgType *return_type, SgFunctionParameterList *parameter_list,
    SgScopeStatement *scope, bool forceFreeFunctionScope = false);

//! Complete one source-less semantic function declaration as its own
//! definition when no distinct declaration precedes it.
ROSE_DLL_API SgFunctionDeclaration *
completeSemanticFunctionDeclarationAsDefinition(
    SgFunctionDeclaration *declaration);

//! Build a defining Fortran procedure with an explicit lexical source form.
ROSE_DLL_API SgProcedureHeaderStatement *buildProcedureHeaderStatement(
    const function_declaration_ownership &ownership, const SgName &name,
    SgType *return_type, SgFunctionParameterList *parameter_list,
    SgFunctionParameterList *canonical_parameter_list,
    SgProcedureHeaderStatement::subprogram_kind_enum,
    SgProcedureHeaderStatement::fortran_procedure_source_form_enum source_form,
    SgScopeStatement *scope);

//! Build a Fortran subroutine or procedure
struct FortranBlockDataBuilderIdentity {
  SgName symbol_table_key;
  SgProcedureHeaderStatement::block_data_name_kind_enum source_name_kind;
  std::string source_path;
  int start_line;
  int start_column;
  int end_line;
  int end_column;
};

ROSE_DLL_API SgProcedureHeaderStatement *buildProcedureHeaderStatement(
    const function_declaration_ownership &ownership, const char *name,
    SgType *return_type, SgFunctionParameterList *parlist,
    SgProcedureHeaderStatement::subprogram_kind_enum,
    SgProcedureHeaderStatement::fortran_procedure_source_form_enum source_form,
    SgScopeStatement *scope,
    SgProcedureHeaderStatement *first_nondefining_declaration,
    const FortranBlockDataBuilderIdentity *block_data_identity = nullptr);

//! Complete a bottom-up Fortran procedure construction around its exact
//! preconstructed definition and body. The definition is consumed exactly
//! once and must already carry final source classification and scope identity.
ROSE_DLL_API SgProcedureHeaderStatement *
buildProcedureHeaderStatementFromExactDefinition(
    const function_declaration_ownership &ownership,
    SgFunctionDefinition *exact_definition, const char *name,
    SgType *return_type, SgFunctionParameterList *parlist,
    SgProcedureHeaderStatement::subprogram_kind_enum,
    SgProcedureHeaderStatement::fortran_procedure_source_form_enum source_form,
    SgScopeStatement *scope,
    SgProcedureHeaderStatement *first_nondefining_declaration,
    const FortranBlockDataBuilderIdentity *block_data_identity = nullptr);

// Rasmussen (9/24/2020)
//! Build a nondefining SgProcedureHeaderStatement, handle function type, symbol
//! etc transparently
ROSE_DLL_API SgProcedureHeaderStatement *
buildNondefiningProcedureHeaderStatement(
    const function_declaration_ownership &ownership, const SgName &name,
    SgType *return_type, SgFunctionParameterList *param_list,
    SgProcedureHeaderStatement::subprogram_kind_enum,
    SgProcedureHeaderStatement::fortran_procedure_source_form_enum source_form,
    SgScopeStatement *scope,
    const FortranBlockDataBuilderIdentity *block_data_identity = nullptr);

//! Build a regular function call statement
ROSE_DLL_API SgExprStatement *
buildFunctionCallStmt(const SgName &name, SgType *return_type,
                      SgExprListExp *parameters = NULL,
                      SgScopeStatement *scope = NULL);

//! Build a function call statement using function expression and argument list
//! only, like (*funcPtr)(args);
ROSE_DLL_API SgExprStatement *
buildFunctionCallStmt(SgExpression *function, SgType *result_type,
                      SgExprListExp *parameters = NULL);

//! Build a label statement, name is the label's name. Handling label symbol and
//! scope internally.

//! Note that the scope of a label statement is special. It is
//! SgFunctionDefinition, not the closest scope statement such as SgBasicBlock.
ROSE_DLL_API SgLabelStatement *
buildLabelStatement(const SgName &name, SgStatement *stmt,
                    SgScopeStatement *scope = NULL);
// Frontend-internal construction entry point.  A null statement is permitted
// only while resolving recursive label references and must be replaced before
// the label is published into a completed AST.
SgLabelStatement *buildLabelStatement_nfi(const SgName &name, SgStatement *stmt,
                                          SgScopeStatement *scope);

//! Build a goto statement
ROSE_DLL_API SgGotoStatement *
buildGotoStatement(SgLabelStatement *label = NULL);
SgGotoStatement *buildGotoStatement_nfi(SgLabelStatement *label);

//! Build a goto statement from a label symbol, supporting both C/C++ and
//! Fortran cases
ROSE_DLL_API SgGotoStatement *buildGotoStatement(SgLabelSymbol *symbol);

//! Build a goto statement from a label expression, supporting only C/C++ and
//! not Fortran cases
SgGotoStatement *buildGotoStatement_nfi(SgExpression *expr);

//! Build a case option statement
ROSE_DLL_API SgCaseOptionStmt *buildCaseOptionStmt(SgExpression *key = NULL,
                                                   SgStatement *body = NULL);
SgCaseOptionStmt *buildCaseOptionStmt_nfi(SgExpression *key, SgStatement *body);

//! Build a default option statement
ROSE_DLL_API SgDefaultOptionStmt *
buildDefaultOptionStmt(SgStatement *body = NULL);
SgDefaultOptionStmt *buildDefaultOptionStmt_nfi(SgStatement *body);

//! Build a SgExprStatement, set File_Info automatically
ROSE_DLL_API SgExprStatement *buildExprStatement(SgExpression *exp = NULL);
SgExprStatement *buildExprStatement_nfi(SgExpression *exp);

//! Build a GNU statement expression
ROSE_DLL_API SgStatementExpression *
buildStatementExpression(SgStatement *exp, SgType *result_type,
                         SgScopeStatement *semantic_scope);
ROSE_DLL_API SgStatementExpression *
buildStatementExpression_nfi(SgStatement *exp, SgType *result_type,
                             SgScopeStatement *semantic_scope);

//! Build a switch statement
ROSE_DLL_API SgSwitchStatement *
buildSwitchStatement(SgStatement *item_selector = NULL,
                     SgStatement *body = NULL);
inline SgSwitchStatement *buildSwitchStatement(SgExpression *item_selector,
                                               SgStatement *body = NULL) {
  return buildSwitchStatement(buildExprStatement(item_selector), body);
}
ROSE_DLL_API SgSwitchStatement *
buildSwitchStatement_nfi(SgStatement *item_selector, SgStatement *body);

//! Build if statement
ROSE_DLL_API SgIfStmt *buildIfStmt(SgStatement *conditional,
                                   SgStatement *true_body,
                                   SgStatement *false_body);
inline SgIfStmt *buildIfStmt(SgExpression *conditional, SgStatement *true_body,
                             SgStatement *false_body) {
  return buildIfStmt(buildExprStatement(conditional), true_body, false_body);
}

ROSE_DLL_API SgIfStmt *buildIfStmt_nfi(SgStatement *conditional,
                                       SgStatement *true_body,
                                       SgStatement *false_body);

//! Build a Fortran do construct
ROSE_DLL_API SgFortranDo *buildFortranDo(SgExpression *initialization,
                                         SgExpression *bound,
                                         SgExpression *increment,
                                         SgBasicBlock *loop_body);
ROSE_DLL_API SgFortranDo *buildFortranDo_nfi(SgExpression *initialization,
                                             SgExpression *bound,
                                             SgExpression *increment,
                                             SgBasicBlock *loop_body);

//! Build a for init statement
ROSE_DLL_API SgForInitStatement *buildForInitStatement();
ROSE_DLL_API SgForInitStatement *
buildForInitStatement(const SgStatementPtrList &statements);
ROSE_DLL_API SgForInitStatement *
buildForInitStatement_nfi(SgStatementPtrList &statements);

ROSE_DLL_API SgForInitStatement *buildForInitStatement(SgStatement *statement);

//! Build a for statement, assume none of the arguments is NULL
ROSE_DLL_API SgForStatement *buildForStatement(SgStatement *initialize_stmt,
                                               SgStatement *test,
                                               SgExpression *increment,
                                               SgStatement *loop_body);
ROSE_DLL_API SgForStatement *buildForStatement_nfi(SgStatement *initialize_stmt,
                                                   SgStatement *test,
                                                   SgExpression *increment,
                                                   SgStatement *loop_body);
ROSE_DLL_API SgForStatement *
buildForStatement_nfi(SgForInitStatement *init_stmt, SgStatement *test,
                      SgExpression *increment, SgStatement *loop_body);
ROSE_DLL_API void buildForStatement_nfi(SgForStatement *result,
                                        SgForInitStatement *init_stmt,
                                        SgStatement *test,
                                        SgExpression *increment,
                                        SgStatement *loop_body);

ROSE_DLL_API SgRangeBasedForStatement *buildRangeBasedForStatement_nfi(
    SgVariableDeclaration *initializer, SgVariableDeclaration *range,
    SgVariableDeclaration *begin_declaration,
    SgVariableDeclaration *end_declaration, SgExpression *not_equal_expression,
    SgExpression *increment_expression, SgStatement *body);

//! Build while statement
ROSE_DLL_API SgWhileStmt *buildWhileStmt(SgStatement *condition,
                                         SgStatement *body);
inline SgWhileStmt *buildWhileStmt(SgExpression *condition, SgStatement *body) {
  return buildWhileStmt(buildExprStatement(condition), body);
}
SgWhileStmt *buildWhileStmt_nfi(SgStatement *condition, SgStatement *body);

//! Build do-while statement
ROSE_DLL_API SgDoWhileStmt *buildDoWhileStmt(SgStatement *body,
                                             SgStatement *condition);
inline SgDoWhileStmt *buildDoWhileStmt(SgStatement *body,
                                       SgExpression *condition) {
  return buildDoWhileStmt(body, buildExprStatement(condition));
}
SgDoWhileStmt *buildDoWhileStmt_nfi(SgStatement *body, SgStatement *condition);

//! Build pragma declaration, handle SgPragma and defining/nondefining pointers
//! internally
ROSE_DLL_API SgPragmaDeclaration *
buildPragmaDeclaration(const std::string &name, SgScopeStatement *scope = NULL);
SgPragmaDeclaration *buildPragmaDeclaration_nfi(const std::string &name,
                                                SgScopeStatement *scope);
SgPragmaDeclaration *buildPragmaDeclaration_nfi(
    const std::string &name, SgScopeStatement *scope,
    SgPragmaDeclaration::fortran_directive_family_enum family);

//! Build SgPragma
ROSE_DLL_API SgPragma *buildPragma(const std::string &name);

//! Build a detached empty declaration with one explicit lexical role.  This
//! builder never consults the ambient scope stack; callers must publish the
//! result through one explicit structural ownership transaction and must
//! distinguish a real `;` from a zero-spelling preprocessing/source anchor.
ROSE_DLL_API SgEmptyDeclaration *buildEmptyDeclaration(
    SgEmptyDeclaration::empty_declaration_role_enum lexicalRole);

//! Build a SgBasicBlock, setting file info internally
ROSE_DLL_API SgBasicBlock *
buildBasicBlock(SgStatement *stmt1 = NULL, SgStatement *stmt2 = NULL,
                SgStatement *stmt3 = NULL, SgStatement *stmt4 = NULL,
                SgStatement *stmt5 = NULL, SgStatement *stmt6 = NULL,
                SgStatement *stmt7 = NULL, SgStatement *stmt8 = NULL,
                SgStatement *stmt9 = NULL, SgStatement *stmt10 = NULL);
ROSE_DLL_API SgBasicBlock *buildBasicBlock_nfi();
SgBasicBlock *buildBasicBlock_nfi(const std::vector<SgStatement *> &);

// CR (7/24/2020): Added additional functionality
//! Build a SgBasicBlock and set its parent. This function does NOT link the
//! parent scope to the block.
SgBasicBlock *buildBasicBlock_nfi(SgScopeStatement *parent);

//! Build an assignment statement from lefthand operand and right hand operand
ROSE_DLL_API SgExprStatement *buildAssignStatement(SgExpression *lhs,
                                                   SgExpression *rhs);

//! Build a break statement
ROSE_DLL_API SgBreakStmt *buildBreakStmt();
SgBreakStmt *buildBreakStmt_nfi();

//! Build a continue statement
SgContinueStmt *buildContinueStmt_nfi();

//! Build a Fortran continue statement
ROSE_DLL_API SgFortranContinueStmt *buildFortranContinueStmt();
SgFortranContinueStmt *buildFortranContinueStmt_nfi();

//! Build an Actual Argument Expression
SgActualArgumentExpression *
buildActualArgumentExpression_nfi(SgName arg_name, SgExpression *arg);

//! Build a delete statement
ROSE_DLL_API SgDeleteExp *
buildDeleteExp(SgExpression *target, bool is_array = false,
               bool need_global_specifier = false,
               SgFunctionDeclaration *deleteOperatorDeclaration = NULL);
SgDeleteExp *
buildDeleteExp_nfi(SgExpression *target, bool is_array = false,
                   bool need_global_specifier = false,
                   SgFunctionDeclaration *deleteOperatorDeclaration = NULL);

//! Build a scope statement. Used to build SgNonrealDecl and SgNonrealType
ROSE_DLL_API SgDeclarationScope *
buildDeclarationScope(SgScopeStatement *semantic_outer_scope = nullptr);

//! Attach semantic-only declaration-scope infrastructure to its exact
//! structural owner.  Both the container and scope must already carry exact
//! semantic provenance; this operation never publishes a physical output
//! owner or reclassifies their subtree.
ROSE_DLL_API void attachSemanticDeclarationScope(SgScopeStatement *owner,
                                                 SgDeclarationScope *scope);

ROSE_DLL_API void detachDeclarationScope(SgScopeStatement *owner,
                                         SgDeclarationScope *scope);

//! Transfer one detached signature scope to its exact function owner.
ROSE_DLL_API void adoptFunctionDeclaratorScope(SgFunctionDeclaration *owner,
                                               SgDeclarationScope *scope);

ROSE_DLL_API SgNode *getDeclarationScopeOwner(const SgDeclarationScope *scope);

//! Own a semantic-only declaration without adding it to source emission.
ROSE_DLL_API void
attachAuxiliaryDeclaration(SgScopeStatement *owner,
                           SgDeclarationStatement *declaration);

//! Remove a semantic-only declaration ownership edge.
ROSE_DLL_API bool
detachAuxiliaryDeclaration(SgScopeStatement *owner,
                           SgDeclarationStatement *declaration);

//! Get the nonreal declaration scope associated with a declaration (if any).
ROSE_DLL_API SgDeclarationScope *
getNonrealDeclarationScope(SgDeclarationStatement *owner);

//! Attach a nonreal declaration scope to a declaration.
ROSE_DLL_API void setNonrealDeclarationScope(SgDeclarationStatement *owner,
                                             SgDeclarationScope *scope);

//! Get or create the nonreal declaration scope associated with a declaration.
ROSE_DLL_API SgDeclarationScope *
getOrCreateNonrealDeclarationScope(SgDeclarationStatement *owner);

//! Build a class definition scope statement
ROSE_DLL_API SgClassDefinition *
buildClassDefinition(SgClassDeclaration *d,
                     bool buildTemplateInstantiation = false);

//! Build a class definition scope statement
SgClassDefinition *
buildClassDefinition_nfi(SgClassDeclaration *d,
                         bool buildTemplateInstantiation = false);

//! Build a template class definition statement
SgTemplateClassDefinition *
buildTemplateClassDefinition(SgTemplateClassDeclaration *d);

//! Exact structural ownership selected by named declaration producers.
class ROSE_DLL_API declaration_ownership {
public:
  enum class kind {
    source_lexical,
    source_lexical_class_redeclaration,
    source_lexical_pending_exact_source,
    transformation_detached,
    semantic_auxiliary,
    semantic_auxiliary_pending_exact_source,
    source_group_member,
    semantic_group_member,
    source_declarator_scope_child,
    embedded_declarator_child,
    semantic_embedded_declarator_child,
    semantic_lexical
  };

  //! Publish in the semantic scope passed to the builder.
  static declaration_ownership sourceLexical();
  //! Publish in the semantic scope while leaving provenance unclassified for
  //! one later exact physical-source publication by a frontend.
  static declaration_ownership sourceLexicalPendingExactSource();
  //! Build one generated declaration surface with complete transformation
  //! provenance and semantic identity, but no lexical owner.  A later typed
  //! mutation transaction must publish its exact physical output owner.
  static declaration_ownership transformationDetached();
  //! Publish in a distinct lexical scope without changing semantic scope.
  static declaration_ownership sourceLexicalIn(SgScopeStatement *lexicalOwner);
  //! Publish a distinct class source surface in a lexical scope while joining
  //! an already published exact semantic class declaration family directly.
  //! No name or template-argument lookup is permitted for this transaction.
  static declaration_ownership
  sourceLexicalClassRedeclarationIn(SgScopeStatement *lexicalOwner,
                                    SgClassDeclaration *canonicalTarget);
  static declaration_ownership semanticAuxiliary();
  //! Publish a semantic declaration directly in its lexical scope.
  static declaration_ownership semanticLexical();
  //! Publish through semantic auxiliary ownership while leaving provenance
  //! unclassified for one exact frontend publication.  The declaration is
  //! structurally auxiliary from construction and can never acquire a lexical
  //! source-owner edge.
  static declaration_ownership semanticAuxiliaryPendingExactSource();
  static declaration_ownership
  sourceGroupMember(SgDeclarationGroupStatement *owner);
  static declaration_ownership
  semanticGroupMember(SgDeclarationGroupStatement *owner);
  //! Publish a source-written tag in the exact function declaration scope that
  //! owns its declarator spelling while preserving the tag's semantic scope.
  static declaration_ownership
  sourceDeclaratorScopeChildIn(SgDeclarationScope *owner);
  static declaration_ownership embeddedDeclaratorChild();
  //! Construct a detached tag with semantic provenance for typed adoption.
  static declaration_ownership semanticEmbeddedDeclaratorChild();

  kind getKind() const;
  SgDeclarationGroupStatement *getDeclarationGroupOwner() const;
  SgScopeStatement *getSourceLexicalOwner() const;
  SgClassDeclaration *getClassRedeclarationTarget() const;

private:
  declaration_ownership(kind ownership_kind,
                        SgDeclarationGroupStatement *group_owner,
                        SgScopeStatement *lexical_owner,
                        SgClassDeclaration *class_redeclaration_target);

  kind ownership_kind_;
  SgDeclarationGroupStatement *group_owner_;
  SgScopeStatement *lexical_owner_;
  SgClassDeclaration *class_redeclaration_target_;
};

//! Complete one source-lexical declaration transaction whose builder used
//! sourceLexicalPendingExactSource().  The declaration must still be detached
//! and must already own exact physical source provenance.  This operation
//! publishes the lexical owner exactly once and never synthesizes or repairs
//! source information.
ROSE_DLL_API void
publishExactSourceLexicalDeclaration(SgDeclarationStatement *declaration,
                                     SgScopeStatement *scope);

//! Attach a fresh source-written class/enum declaration to its exact function
//! declarator scope without changing its semantic symbol scope.  This is the
//! construction-time structural transaction used by Clang TypeLoc producers;
//! it does not synthesize source provenance or transfer an existing owner.
ROSE_DLL_API void
attachSourceDeclaratorTag(SgDeclarationScope *owner,
                          SgDeclarationStatement *declaration);

//! Transfer a detached declarator-local source scope to its exact non-function
//! declaration owner.  The scope's declarations keep their independent
//! semantic symbol scopes; only structural source ownership changes.
ROSE_DLL_API void adoptSourceDeclaratorScope(SgDeclarationStatement *owner,
                                             SgDeclarationScope *scope);

//! Begin construction of a non-function declaration whose TypeLoc owns nested
//! source-written tags.  Both nodes must be fresh and the scope must be empty;
//! finishSourceDeclaratorScopeConstruction() hard-validates the populated
//! source children before the declaration can be published.
ROSE_DLL_API void
beginSourceDeclaratorScopeConstruction(SgDeclarationStatement *owner,
                                       SgDeclarationScope *scope);

//! Complete a source-declarator scope begun by
//! beginSourceDeclaratorScopeConstruction().  Every child must have one exact
//! nonautonomous structural role and already-published physical provenance.
ROSE_DLL_API void
finishSourceDeclaratorScopeConstruction(SgDeclarationStatement *owner,
                                        SgDeclarationScope *scope);

//! Exact semantic and canonical symbol scopes for a template-class surface.
//!
//! Namespace reopenings are distinct semantic/source scopes that share one
//! canonical symbol table.  Class-like scopes, by contrast, are exact identity
//! owners and are never normalized.  Construct this descriptor from the exact
//! semantic scope of the declaration being built; callers cannot supply or
//! repair the canonical symbol scope independently.
class ROSE_DLL_API template_class_declaration_scopes final {
public:
  static template_class_declaration_scopes
  fromExactSemanticScope(SgScopeStatement *exactSemanticScope);

  SgScopeStatement *getExactSemanticScope() const;
  SgScopeStatement *getCanonicalSymbolScope() const;

private:
  template_class_declaration_scopes(SgScopeStatement *exactSemanticScope,
                                    SgScopeStatement *canonicalSymbolScope);

  SgScopeStatement *exact_semantic_scope_;
  SgScopeStatement *canonical_symbol_scope_;
};

//! Build a class first nondefining declaration, without file info.
//!
//! A non-null templateDeclaration selects SgTemplateInstantiationDecl and is
//! installed by its constructor. Template arguments without that exact
//! identity or its exact selected primary/partial declaration are malformed
//! input. The written list transfers detached arguments
//! and copies arguments owned elsewhere. A separate complete list preserves
//! identity when written arguments omit defaults. Both semantic lists are
//! required for instantiations, including when empty. The deduced list
//! transfers its detached children to the new node.
ROSE_DLL_API SgClassDeclaration *buildNondefiningClassDeclaration_nfi(
    const declaration_ownership &ownership, const SgName &name,
    SgClassDeclaration::class_types kind, SgScopeStatement *scope,
    SgTemplateClassDeclaration *templateDeclaration,
    SgTemplateClassDeclaration *specializedTemplateDeclaration,
    SgTemplateArgumentPtrList *templateArgumentsList,
    const SgName *templateInstantiationNameOverride,
    bool anonymousInternalIdentity = false,
    const SgTemplateArgumentPtrList *semanticTemplateArgumentsList = nullptr,
    bool sourceSpellsInjectedClassName = false,
    SgTemplateArgumentPtrList *deducedTemplateArgumentsList = nullptr);

// DQ (8/11/2013): We need to hand in both the SgTemplateParameterPtrList and
// the SgTemplateArgumentPtrList because class templates can be partially
// specialized.
ROSE_DLL_API SgTemplateClassDeclaration *
buildNondefiningTemplateClassDeclaration_nfi(
    const declaration_ownership &ownership, const SgName &name,
    SgClassDeclaration::class_types kind,
    const template_class_declaration_scopes &scopes,
    SgTemplateParameterPtrList *templateParameterList,
    SgTemplateArgumentPtrList *templateSpecializationArgumentList,
    const SgName *templateSpecializationNameOverride);

//! Build one nondefining redeclaration of an existing template class.  The
//! canonical declaration is required explicitly; the redeclaration reuses its
//! exact type and symbol and never publishes another symbol.
ROSE_DLL_API SgTemplateClassDeclaration *
buildNondefiningTemplateClassRedeclaration_nfi(
    const declaration_ownership &ownership, const SgName &name,
    SgClassDeclaration::class_types kind,
    const template_class_declaration_scopes &scopes,
    SgTemplateClassDeclaration *canonicalDeclaration,
    SgTemplateParameterPtrList *templateParameterList,
    SgTemplateArgumentPtrList *templateSpecializationArgumentList,
    const SgName *templateSpecializationNameOverride);

//! buildNondefiningTemplateClassDeclaration()
ROSE_DLL_API SgTemplateClassDeclaration *
buildNondefiningTemplateClassDeclaration(
    const declaration_ownership &ownership, const SgName &name,
    SgClassDeclaration::class_types kind,
    const template_class_declaration_scopes &scopes,
    SgTemplateParameterPtrList *templateParameterList,
    SgTemplateArgumentPtrList *templateSpecializationArgumentList,
    const SgName *templateSpecializationNameOverride);

ROSE_DLL_API SgClassDeclaration *
buildNondefiningClassDeclaration(const declaration_ownership &ownership,
                                 SgName name, SgScopeStatement *scope);
ROSE_DLL_API SgClassDeclaration *
buildDefiningClassDeclaration(const declaration_ownership &ownership,
                              SgName name, SgScopeStatement *scope,
                              SgClassDeclaration *nonDefiningDeclaration);

//! Build both nondefining and defining class declarations, in that order.
ROSE_DLL_API SgClassDeclaration *
buildClassDeclaration(const declaration_ownership &ownership, SgName name,
                      SgScopeStatement *scope);

//! Build an enum nondefining declaration, without file info
ROSE_DLL_API SgEnumDeclaration *
buildNondefiningEnumDeclaration_nfi(const SgName &name, bool isUnNamed,
                                    SgScopeStatement *scope,
                                    const declaration_ownership &ownership,
                                    SgEnumDeclaration *canonicalDeclaration);

//! Build a structure, It is also a declaration statement in SAGE III
ROSE_DLL_API SgClassDeclaration *
buildStructDeclaration(const declaration_ownership &ownership,
                       const SgName &name, SgScopeStatement *scope);
ROSE_DLL_API SgClassDeclaration *
buildStructDeclaration(const declaration_ownership &ownership,
                       const std::string &name, SgScopeStatement *scope);
ROSE_DLL_API SgClassDeclaration *
buildStructDeclaration(const declaration_ownership &ownership, const char *name,
                       SgScopeStatement *scope);

//! Build a source-anonymous structure with an explicit, non-empty semantic
//! identity.  The internal name is never emitted as source.
ROSE_DLL_API SgClassDeclaration *
buildAnonymousStructDeclaration(const declaration_ownership &ownership,
                                const SgName &internalName,
                                SgScopeStatement *scope);

//! tps (09/02/2009) : Added support for building namespaces
ROSE_DLL_API SgNamespaceDeclarationStatement *
buildNamespaceDeclaration(const SgName &name, SgScopeStatement *scope);

enum namespace_declaration_ownership_enum {
  e_namespace_declaration_source_lexical,
  e_namespace_declaration_canonical_generated_lexical,
  e_namespace_declaration_semantic_auxiliary
};

//! Build one namespace declaration with explicit semantic and lexical
//! ownership. Source-lexical declarations require source-spelled fragments and
//! a positive producer-wide namespace order in the final expanded token
//! stream; generated and semantic-only declarations require no order.
ROSE_DLL_API SgNamespaceDeclarationStatement *buildNamespaceDeclaration_nfi(
    const SgName &name, bool unnamednamespace, SgScopeStatement *scope,
    namespace_declaration_ownership_enum ownership,
    SgNamespaceSourceFragment *openingIntroducerFragment,
    SgNamespaceSourceFragment *openingFragment,
    SgNamespaceSourceFragment *closingFragment,
    std::optional<unsigned int> sourceOrder);
ROSE_DLL_API SgNamespaceDefinitionStatement *
buildNamespaceDefinition(SgNamespaceDeclarationStatement *d = NULL);
ROSE_DLL_API SgNamespaceDefinitionStatement *
buildNamespaceDefinition_nfi(SgNamespaceDeclarationStatement *d = NULL);

// Pei-Hung (09/14/2023) :added support for building namespace alias
ROSE_DLL_API SgNamespaceAliasDeclarationStatement *
buildNamespaceAliasDeclarationStatement(
    const declaration_ownership &ownership, const SgName &name,
    SgNamespaceDeclarationStatement *namespaceDeclaration,
    SgScopeStatement *scope);

ROSE_DLL_API SgClassDeclaration *buildClassDeclaration_nfi(
    const declaration_ownership &ownership, const SgName &name,
    SgClassDeclaration::class_types kind, SgScopeStatement *scope,
    SgClassDeclaration *nonDefiningDecl, bool buildTemplateInstantiation,
    SgTemplateArgumentPtrList *templateArgumentsList,
    const SgName *templateInstantiationName);

ROSE_DLL_API SgTemplateClassDeclaration *buildTemplateClassDeclaration_nfi(
    const declaration_ownership &ownership, const SgName &name,
    SgClassDeclaration::class_types kind,
    const template_class_declaration_scopes &scopes,
    SgTemplateClassDeclaration *nonDefiningDecl,
    SgTemplateParameterPtrList *nondefiningTemplateParameterList,
    SgTemplateParameterPtrList *definingTemplateParameterList,
    SgTemplateArgumentPtrList *templateSpecializationArgumentList,
    const SgName *templateSpecializationNameOverride);
//! Build tempplate class declaration
ROSE_DLL_API SgTemplateClassDeclaration *buildTemplateClassDeclaration(
    const declaration_ownership &ownership, const SgName &name,
    SgClassDeclaration::class_types kind,
    const template_class_declaration_scopes &scopes,
    SgTemplateClassDeclaration *nonDefiningDecl,
    SgTemplateParameterPtrList *nondefiningTemplateParameterList,
    SgTemplateParameterPtrList *definingTemplateParameterList,
    SgTemplateArgumentPtrList *templateSpecializationArgumentList,
    const SgName *templateSpecializationNameOverride);

//! Build an SgDerivedTypeStatement Fortran derived type declaration with a
//! class declaration and definition (creating both the defining and nondefining
//! declarations as required).
ROSE_DLL_API SgDerivedTypeStatement *
buildDerivedTypeStatement(const declaration_ownership &ownership,
                          const SgName &name, SgScopeStatement *scope);

//! Build the canonical semantic forward declaration for a Fortran derived
//! type.  A derived-type forward has no source surface or definition and must
//! be completed later by exactly one buildDerivedTypeStatement() call.
ROSE_DLL_API SgDerivedTypeStatement *
buildNondefiningDerivedTypeStatement(const declaration_ownership &ownership,
                                     const SgName &name,
                                     SgScopeStatement *scope);

//! Build a Fortran module declaration.
ROSE_DLL_API SgModuleStatement *
buildModuleStatement(const declaration_ownership &ownership, const SgName &name,
                     SgScopeStatement *scope);

//! Build a generic class declaration statement (SgClassDeclaration or subclass)
//! with a class declaration and definition (creating both the defining and
//! nondefining declarations as required.
template <class DeclClass>
ROSE_DLL_API DeclClass *buildClassDeclarationStatement_nfi(
    const declaration_ownership &ownership, const SgName &name,
    SgClassDeclaration::class_types kind, SgScopeStatement *scope,
    SgClassDeclaration *nonDefiningDecl);

//! Build an enum, It is also a declaration statement in SAGE III
ROSE_DLL_API SgEnumDeclaration *
buildEnumDeclaration(const declaration_ownership &ownership, const SgName &name,
                     bool isUnNamed, SgScopeStatement *scope);

//! Build one enum definition over an explicit canonical type identity.
ROSE_DLL_API SgEnumDeclaration *buildEnumDeclaration_nfi(
    const declaration_ownership &ownership, const SgName &name, bool isUnNamed,
    SgScopeStatement *scope, SgEnumDeclaration *canonicalDeclaration);

//! Build a return statement
ROSE_DLL_API SgReturnStmt *
buildReturnStmt(SgExpression *expression = nullptr,
                SgInitializedName *nrvo_candidate = nullptr);
ROSE_DLL_API SgReturnStmt *
buildReturnStmt_nfi(SgExpression *expression,
                    SgInitializedName *nrvo_candidate = nullptr);

//! Build a NULL statement
ROSE_DLL_API SgNullStatement *buildNullStatement();
SgNullStatement *buildNullStatement_nfi();

//! Build Fortran attribute specification statement
ROSE_DLL_API SgAttributeSpecificationStatement *
buildAttributeSpecificationStatement(
    SgAttributeSpecificationStatement::attribute_spec_enum kind,
    const SgDataStatementGroupPtrList &groups = {});
//! Build a source-backed Fortran attribute specification statement whose exact
//! file information will be published by the frontend.
ROSE_DLL_API SgAttributeSpecificationStatement *
buildAttributeSpecificationStatement_nfi(
    SgAttributeSpecificationStatement::attribute_spec_enum kind,
    const SgDataStatementGroupPtrList &groups = {});

//! Build Fortran include line
ROSE_DLL_API SgFortranIncludeLine *
buildFortranIncludeLine(std::string filename);
ROSE_DLL_API SgFortranIncludeLine *
buildFortranIncludeLine_nfi(std::string filename);

//! Build a Fortran common block, possibly with a name
ROSE_DLL_API SgCommonBlockObject *
buildCommonBlockObject(std::string name = "", SgExprListExp *exp_list = NULL);

//! Build a Fortran Common statement
ROSE_DLL_API SgCommonBlock *
buildCommonBlock(SgCommonBlockObject *first_block = NULL);

// driscoll6 (6/9/2011): Adding support for try stmts.
//! Build a catch statement.
ROSE_DLL_API SgCatchOptionStmt *
buildCatchOptionStmt(SgVariableDeclaration *condition = NULL,
                     SgStatement *body = NULL);
ROSE_DLL_API SgCatchOptionStmt *
buildCatchOptionStmt_nfi(SgVariableDeclaration *condition = NULL,
                         SgStatement *body = NULL);

//! Build a try statement.
ROSE_DLL_API SgTryStmt *buildTryStmt(SgStatement *body,
                                     SgCatchOptionStmt *catch0 = NULL,
                                     SgCatchOptionStmt *catch1 = NULL,
                                     SgCatchOptionStmt *catch2 = NULL,
                                     SgCatchOptionStmt *catch3 = NULL,
                                     SgCatchOptionStmt *catch4 = NULL);

//! Build an asm statement
ROSE_DLL_API SgAsmStmt *buildAsmStatement(std::string s);

ROSE_DLL_API SgBaseClass *
buildBaseClass(SgClassDeclaration *classDeclaration, SgType *sourceType,
               SgClassDefinition *classDefinition, bool isVirtual,
               bool isDirect, SgSymbol *fortranSourceSymbol = nullptr);

ROSE_DLL_API SgNonrealBaseClass *
buildNonrealBaseClass(SgNonrealDecl *classDeclaration, SgType *sourceType,
                      SgClassDefinition *classDefinition, bool isVirtual,
                      bool isDirect);

ROSE_DLL_API SgStaticAssertionDeclaration *
buildStaticAssertionDeclaration_nfi(SgExpression *condition,
                                    SgExpression *message = nullptr);

//! Build a C++11 type-only friend declaration, such as `friend T;`.
ROSE_DLL_API SgFriendTypeDeclaration *
buildFriendTypeDeclaration(SgType *friend_type);

//! Build a using directive statement
ROSE_DLL_API SgUsingDirectiveStatement *
buildUsingDirectiveStatement(SgNamespaceDeclarationStatement *ns_decl);
ROSE_DLL_API SgUsingDirectiveStatement *
buildUsingDirectiveStatement_nfi(SgNamespaceDeclarationStatement *ns_decl);
//@}

//--------------------------------------------------------------
//@{
/*! @name Builders for others
  \brief AST high level builders for others

*/
//! Build a SgFile node and attach it to SgProject
/*! Parse an existing readable input file. An unreadable input or frontend
 * failure is a hard error. Use buildGeneratedSourceFile for output-only AST
 * construction. outputFileName selects the unparse destination. The file is
 * registered in project; buildFile creates a project when none is supplied.
 * Retrieve its owner with SgFile::get_project().
 */
ROSE_DLL_API SgFile *buildFile(const std::string &inputFileName,
                               const std::string &outputFileName,
                               SgProject *project = NULL);

//! Build an output-only SgSourceFile node and attach it to SgProject
/*! The file is constructed with an empty global scope without creating an
 * input file or invoking a frontend.  The output filename is canonicalized to
 * one absolute translation-unit identity before construction and determines
 * the source language.  The same identity owns the file command line, source
 * positions, and unparse output.  If project is null, a new SgProject is
 * created and can be retrieved with SgFile::get_project().
 */
ROSE_DLL_API SgSourceFile *
buildGeneratedSourceFile(const std::string &outputFileName,
                         SgProject *project = NULL);

//! Build a SgSourceFile node and attach it to SgProject
/*! Parse an existing readable input file. An unreadable input or frontend
 * failure is a hard error. Use buildGeneratedSourceFile for output-only AST
 * construction. outputFileName selects the unparse destination. The file is
 * registered in project; buildFile creates a project when none is supplied.
 * Retrieve its owner with SgFile::get_project().
 */
ROSE_DLL_API SgSourceFile *buildSourceFile(const std::string &inputFileName,
                                           const std::string &outputFileName,
                                           SgProject *project);

//! Commit the physical identity of a parsed source-file copy.
/*! The source file must still identify the primary file it was parsed from.
 * Positions owned by that primary file are rebound to newFileName. Positions
 * originating in included files retain their exact physical provenance.
 * Incomplete, shared, or mixed source ranges are hard producer errors.
 */
ROSE_DLL_API void
rebindCopiedSourceFilePhysicalIdentity(SgSourceFile *sourceFile,
                                       const std::string &newFileName);

//! Build and attach a comment with an explicit lexical comment style. It is a
//! wrapper of SageInterface::attachComment().
ROSE_DLL_API PreprocessingInfo *
buildComment(SgLocatedNode *target, const std::string &content,
             PreprocessingInfo::DirectiveType commentStyle,
             PreprocessingInfo::RelativePositionType position =
                 PreprocessingInfo::before);

//! Build and attach #define XX directives, pass "#define xxx xxx" as content.
ROSE_DLL_API PreprocessingInfo *buildCpreprocessorDefineDeclaration(
    SgLocatedNode *target, const std::string &content,
    PreprocessingInfo::RelativePositionType position =
        PreprocessingInfo::before);

ROSE_DLL_API SgEquivalenceStatement *
buildEquivalenceStatement(SgExpression *lhs, SgExpression *rhs);

ROSE_DLL_API void fixupCopyOfNodeFromSeparateFileInNewTargetAst(
    SgStatement *insertionPoint, bool insertionPointIsScope, SgNode *node_copy,
    SgNode *node_original);
ROSE_DLL_API SgType *getTargetFileType(SgType *snippet_type,
                                       SgScopeStatement *targetScope);

ROSE_DLL_API SgSymbol *
findAssociatedSymbolInTargetAST(SgDeclarationStatement *snippet_declaration,
                                SgScopeStatement *targetScope);
ROSE_DLL_API SgSymbol *
findAssociatedSymbolInTargetAST(SgNode *snippet_declaration,
                                SgScopeStatement *targetScope);

//! Error checking the inserted snippet AST.
ROSE_DLL_API void errorCheckingTargetAST(SgNode *node_copy,
                                         SgNode *node_original,
                                         SgFile *targetFile);

//@}

//----------------------build unary expressions----------------------
//!  Template function to build a unary expression of type T. Instantiated
//!  functions
//!  include:buildAddressOfOp(),buildBitComplementOp(),buildBitComplementOp(),buildMinusOp(),buildNotOp(),buildPointerDerefExp(),buildUnaryAddOp(),buildMinusMinusOp(),buildPlusPlusOp().
//!  They are also used for the unary vararg operators (which are not
//!  technically unary operators).
/*! The instantiated functions' prototypes are not shown since they are expanded
 * using macros. Documentation tools do not expand these macros.
 */

template <class T, class... ConstructorArgs>
T *buildUnaryExpression(SgExpression *operand, SgType *result_type,
                        ConstructorArgs &&...constructor_args) {
  if (operand == nullptr) {
    fprintf(stderr,
            "REX_AST_INVARIANT[unary-operand-producer]: builder requires one "
            "structural operand\n");
    ROSE_ABORT();
  }
  if (operand->get_parent() != nullptr) {
    fprintf(stderr,
            "REX_AST_INVARIANT[unary-operand-ownership]: builder requires a "
            "detached structural operand\n");
    ROSE_ABORT();
  }
  if (SageInterface::containsUnknownType(result_type)) {
    fprintf(stderr,
            "REX_AST_INVARIANT[unary-result-type-producer]: builder requires "
            "an exact semantic result type\n");
    ROSE_ABORT();
  }
  if constexpr (std::is_same_v<T, SgMinusMinusOp> ||
                std::is_same_v<T, SgPlusPlusOp>) {
    operand->set_lvalue(true);
  }
  T *result = new T(operand, result_type,
                    std::forward<ConstructorArgs>(constructor_args)...);
  ROSE_ASSERT(result);
  operand->set_parent(result);
  SageInterface::setOneSourcePositionForTransformation(result);
  return result;
}

//!  Template function to build a unary expression of type T with no file info.
//!  Instantiated functions
//!  include:buildAddressOfOp(),buildBitComplementOp(),buildBitComplementOp(),buildMinusOp(),buildNotOp(),buildPointerDerefExp(),buildUnaryAddOp(),buildMinusMinusOp(),buildPlusPlusOp().
//!  They are also used for the unary vararg operators (which are not
//!  technically unary operators).
/*! The instantiated functions' prototypes are not shown since they are expanded
 * using macros. Documentation tools do not expand these macros.
 */
template <class T, class... ConstructorArgs>
T *buildUnaryExpression_nfi(SgExpression *operand, SgType *result_type,
                            ConstructorArgs &&...constructor_args) {
  if (operand == nullptr) {
    fprintf(stderr,
            "REX_AST_INVARIANT[unary-operand-producer]: builder requires one "
            "structural operand\n");
    ROSE_ABORT();
  }
  if (operand->get_parent() != nullptr) {
    fprintf(stderr,
            "REX_AST_INVARIANT[unary-operand-ownership]: builder requires a "
            "detached structural operand\n");
    ROSE_ABORT();
  }
  if (SageInterface::containsUnknownType(result_type)) {
    fprintf(stderr,
            "REX_AST_INVARIANT[unary-result-type-producer]: builder requires "
            "an exact semantic result type\n");
    ROSE_ABORT();
  }
  if constexpr (std::is_same_v<T, SgMinusMinusOp> ||
                std::is_same_v<T, SgPlusPlusOp>) {
    operand->set_lvalue(true);
  }
  T *result = new T(operand, result_type,
                    std::forward<ConstructorArgs>(constructor_args)...);
  ROSE_ASSERT(result);
  operand->set_parent(result);
  requireFreshNfiExpressionSourceState(result, "buildUnaryExpression_nfi");

  result->set_need_paren(false);
  return result;
}

//---------------------binary expressions-----------------------

template <class T>
T *buildBinaryExpression(SgExpression *lhs, SgExpression *rhs,
                         SgType *result_type) {
  if (lhs == nullptr || rhs == nullptr) {
    fprintf(stderr,
            "REX_AST_INVARIANT[binary-operand-producer]: builder requires "
            "both structural operands\n");
    ROSE_ABORT();
  }
  if (lhs == rhs || lhs->get_parent() != nullptr ||
      rhs->get_parent() != nullptr) {
    fprintf(stderr,
            "REX_AST_INVARIANT[binary-operand-ownership]: builder requires "
            "two distinct detached structural operands\n");
    ROSE_ABORT();
  }
  if (SageInterface::containsUnknownType(result_type)) {
    fprintf(stderr,
            "REX_AST_INVARIANT[binary-result-type-producer]: builder requires "
            "an exact semantic result type\n");
    ROSE_ABORT();
  }
  if constexpr (std::is_same_v<T, SgAssignOp> ||
                std::is_base_of_v<SgCompoundAssignOp, T>) {
    lhs->set_lvalue(true);
  }
  T *result;
  if constexpr (std::is_same_v<T, SgPntrArrRefExp>)
    result = new T(lhs, rhs, result_type, std::nullopt,
                   std::vector<std::optional<long long>>{});
  else
    result = new T(lhs, rhs, result_type);
  ROSE_ASSERT(result);
  lhs->set_parent(result);
  rhs->set_parent(result);
  SageInterface::setOneSourcePositionForTransformation(result);
  return result;
}

template <class T, class... Args>
T *buildBinaryExpression_nfi(SgExpression *lhs, SgExpression *rhs,
                             SgType *result_type, Args &&...args) {
  if (lhs == nullptr || rhs == nullptr) {
    fprintf(stderr,
            "REX_AST_INVARIANT[binary-operand-producer]: builder requires "
            "both structural operands\n");
    ROSE_ABORT();
  }
  if (lhs == rhs || lhs->get_parent() != nullptr ||
      rhs->get_parent() != nullptr) {
    fprintf(stderr,
            "REX_AST_INVARIANT[binary-operand-ownership]: builder requires "
            "two distinct detached structural operands\n");
    ROSE_ABORT();
  }
  if (SageInterface::containsUnknownType(result_type)) {
    fprintf(stderr,
            "REX_AST_INVARIANT[binary-result-type-producer]: builder requires "
            "an exact semantic result type\n");
    ROSE_ABORT();
  }
  if constexpr (std::is_same_v<T, SgAssignOp> ||
                std::is_base_of_v<SgCompoundAssignOp, T>) {
    lhs->set_lvalue(true);
  }
  T *result;
  if constexpr (std::is_same_v<T, SgPntrArrRefExp> && sizeof...(Args) == 0)
    result = new T(lhs, rhs, result_type, std::nullopt,
                   std::vector<std::optional<long long>>{});
  else
    result = new T(lhs, rhs, result_type, std::forward<Args>(args)...);
  ROSE_ASSERT(result);
  lhs->set_parent(result);
  rhs->set_parent(result);
  requireFreshNfiExpressionSourceState(result, "buildBinaryExpression_nfi");
  result->set_need_paren(false);

  return result;
}

} // namespace SageBuilder

namespace Rose {
namespace Builder {
namespace Templates {

SgTemplateArgument *buildTemplateArgument(SgType *t);
SgTemplateArgument *buildTemplateArgument(SgExpression *e);
SgTemplateArgument *buildTemplateArgument(int v);
SgTemplateArgument *buildTemplateArgument(bool v);

SgExpression *
instantiateNonrealRefExps(SgExpression *expr,
                          std::vector<SgTemplateParameter *> &tpl_params,
                          std::vector<SgTemplateArgument *> &tpl_args);
SgType *instantiateNonrealTypes(SgType *type,
                                std::vector<SgTemplateParameter *> &tpl_params,
                                std::vector<SgTemplateArgument *> &tpl_args,
                                SgScopeStatement *semantic_receiver_scope);

} // namespace Templates
} // namespace Builder
} // namespace Rose

namespace SageBuilder {
using Rose::Builder::Templates::buildTemplateArgument;
using Rose::Builder::Templates::instantiateNonrealRefExps;
using Rose::Builder::Templates::instantiateNonrealTypes;
} // namespace SageBuilder

#endif // ROSE_SAGE_BUILDER_INTERFACE
