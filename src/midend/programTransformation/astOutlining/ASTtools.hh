/**
 *  \file ASTtools.hh
 *  \brief Higher-level AST manipulation support routines.
 *  \author Chunhua Liao <liaoch@cs.uh.edu>
 *
 *  This implementation was extracted by Liao from his ROSE
 *  OpenMP_Translator project.
 */

#if !defined(INC_ASTTOOLS_HH)
#define INC_ASTTOOLS_HH //! ASTtools.hh included.

#include "rosedll.h"

#include "Cxx_Grammar.h"

#include <cstddef>

#include <map>

#include <ostream>

#include <set>

#include <string>

namespace ASTtools {
//! Stable ordering for variable symbols used by outlining/lowering interfaces.
struct ROSE_DLL_API VarSymLess {
  bool operator()(const SgVariableSymbol *lhs,
                  const SgVariableSymbol *rhs) const;
};

//! Stores a collection of SgVariableSymbols (var syms).
typedef std::set<const SgVariableSymbol *, VarSymLess> VarSymSet_t;

//! Search for the first surrounding scope that may contain a function def.
ROSE_DLL_API const SgScopeStatement *
findFirstFuncDefScope(const SgStatement *s);

//! Search for the first surrounding function definition.
ROSE_DLL_API const SgFunctionDefinition *findFirstFuncDef(const SgStatement *s);

//! Return the canonical declaration that identifies a function family.
ROSE_DLL_API SgFunctionDeclaration *
canonicalFunctionFamilyDeclaration(SgFunctionDeclaration *declaration);

//! Return true when two declarations belong to the same function family.
ROSE_DLL_API bool sameFunctionFamily(SgFunctionDeclaration *lhs,
                                     SgFunctionDeclaration *rhs);

//! Return the function that lexically owns a function-local named type.
ROSE_DLL_API SgFunctionDeclaration *functionOwningHiddenNamedType(SgType *type);

//! Returns 'true' if the specific function is a 'const' member function.
bool isConstMemFunc(const SgFunctionDeclaration *decl);

//! Returns 'true' if the specific function is a 'const' member function.
bool isConstMemFunc(const SgFunctionDefinition *def);

//! Returns 'true' if the specified type is a 'const' object.
bool isConstObj(const SgType *type);

//! Build the exact semantic result type of applying unary '&' to an
//! expression of operand_type.  C++ reference declarations denote their
//! referred-to object in an expression, so this never constructs the illegal
//! pointer-to-reference type.
ROSE_DLL_API SgPointerType *buildAddressOfResultType(SgType *operand_type);

//! Publish the exact use-site pack-expansion surface for an argument that
//! references a template parameter.  A type parameter's SgTemplateType owns
//! the ellipsis in its declaration; the SgTemplateArgument owns the ellipsis
//! in a generated template-id use.
ROSE_DLL_API void
publishTemplateParameterPackExpansion(const SgTemplateParameter *parameter,
                                      SgTemplateArgument *argument);

//! Collect variables suitable for using pointer dereferencing
ROSE_DLL_API void collectPointerDereferencingVarSyms(const SgStatement *s,
                                                     VarSymSet_t &pdSyms);

/*!
 *  \brief Returns 'true' <==> 's' is the conditional selector of some
 *  'switch' statement.
 */
bool isSwitchCond(const SgStatement *s);

/*!
 *  \brief Returns 'true' <==> 's' is the condition of some 'if'
 *  statement.
 */
bool isIfCond(const SgStatement *s);

/*!
 *  \brief Returns 'true' <==> 's' is the condition of some 'while' or
 *  'do ... while' statement.
 */
bool isWhileCond(const SgStatement *s);

//! Returns 'true' if this node is contained in a C99-only project.
bool isC99(const SgNode *n);

//! Returns 'true' if the scope is the '::std' namespace.
bool isStdNamespace(const SgScopeStatement *scope);

//! Returns 'true' if the function is 'main'.
bool isMain(const SgFunctionDeclaration *decl);

//! Returns 'true' if the given declaration is a template instantiation.
bool isTemplateInst(const SgDeclarationStatement *decl);

//! Returns 'true' if the given scope is a template instantiation.
bool isTemplateInst(const SgScopeStatement *scope);

//! Returns a non-templated, unqualified name.
std::string getUnqualUntmplName(const SgDeclarationStatement *d);

//! Returns 'true' iff the given function is declared 'extern "C"'.
bool isExternC(const SgFunctionDeclaration *func);

/*!
 *  \brief Returns true iff the function's unqualified name equals the
 *  target name.
 */
bool isFuncName(const SgFunctionDeclaration *func, const std::string &target);

/*!
 *  \brief Returns true iff the function's unqualified name begins
 *  with the target prefix.
 */
bool isFuncNamePrefix(const SgFunctionDeclaration *func,
                      const std::string &target_prefix);

/*!
 *  \brief Returns the number of function arguments, including 'this'
 *  for member functions.
 *
 *  Examples:
 *    void foo (void);     // 0 args
 *    void A::foo (void);  // 1 arg, including 'this'
 *
 *    void foo (int);      // 1 arg
 *    void A::foo (int);   // 2 args, including 'this'
 */
size_t getNumArgs(const SgFunctionDeclaration *func);

//! Returns true if 'func' is a constructor.
bool isCtor(const SgFunctionDeclaration *func);

//! Returns true if 'func' is a destructor.
bool isDtor(const SgFunctionDeclaration *func);

//! Returns true iff the given function declaration is 'static'.
bool isStaticFunc(const SgFunctionDeclaration *func);

//! Returns the name of the class of a member function.
std::string getClassName(const SgMemberFunctionDeclaration *mem_func);

//! Returns the unqualified name of the member function.
std::string getMemFuncName(const SgMemberFunctionDeclaration *mem_func);

//! Convert a node's location to a string-friendly form.
std::string toStringFileLoc(const SgLocatedNode *n);

//! Returns a newly allocated file info object for transformation nodes.
ROSE_DLL_API Sg_File_Info *newFileInfo(void);

//! Dump a symbol table.
ROSE_DLL_API void dumpSymTab(const SgScopeStatement *s, const std::string &tag,
                             std::ostream &o);
//! Reset source position as transformation for the current node only
ROSE_DLL_API void setSourcePositionAsTransformation(SgNode *node);

//! Reset source position as transformation recursively
ROSE_DLL_API void
setSourcePositionAtRootAndAllChildrenAsTransformation(SgNode *node);

} // namespace ASTtools

#endif // !defined(INC_ASTTOOLS_HH)

// eof
