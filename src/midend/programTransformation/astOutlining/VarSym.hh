/*!
 *  \file ASTtools/VarSym.hh
 *
 *  \brief Implements routines to assist in variable symbol analysis
 *  and manipulation.
 */

#if !defined(INC_ASTTOOLS_VARSYM_HH)
//! ASTtools/VarSym.hh included.
#define INC_ASTTOOLS_VARSYM_HH

#include <set>

#include "Outliner.hh"

class SgVariableSymbol;
class SgStatement;

namespace ASTtools {
ROSE_DLL_API bool requiresVariableCapture(const SgVarRefExp *reference);
//! Collect var syms for all local variable declarations at 's'.
ROSE_DLL_API void collectDefdVarSyms(const SgNode *s, VarSymSet_t &syms);

//! Collect var syms for all variable references at 's'.
ROSE_DLL_API void collectRefdVarSyms(const SgNode *s, VarSymSet_t &syms);

/*!
 *  Collect var syms declared at 'root' or below that are visible to
 *  'target'.
 */
ROSE_DLL_API
void collectLocalVisibleVarSyms(const SgStatement *root,
                                const SgStatement *target, VarSymSet_t &syms);

//! Collect variable references using addresses for s
ROSE_DLL_API void collectVarRefsUsingAddress(const SgStatement *s,
                                             std::set<SgVarRefExp *> &varSetB);

//! Collect variable references with a type which does not support =operator or
//! copy construction. Those two support is essential for temp variables used to
//! copy and restore parameters
ROSE_DLL_API void
collectVarRefsOfTypeWithoutAssignmentSupport(const SgStatement *s,
                                             std::set<SgVarRefExp *> &varSetB);

//! Collect variables to be replaced by pointer dereferencing (pd)
ROSE_DLL_API void collectPointerDereferencingVarSyms(const SgStatement *s,
                                                     VarSymSet_t &pdSyms);

} // namespace ASTtools

#endif // !defined(INC_ASTTOOLS_VARSYM_HH)

// eof
