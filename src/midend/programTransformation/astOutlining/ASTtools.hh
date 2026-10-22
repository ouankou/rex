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

//! Returns 'true' if the specified type is a 'const' object.
bool isConstObj(const SgType *type);

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

//! Convert a node's location to a string-friendly form.
std::string toStringFileLoc(const SgLocatedNode *n);

} // namespace ASTtools

#endif // !defined(INC_ASTTOOLS_HH)

// eof
