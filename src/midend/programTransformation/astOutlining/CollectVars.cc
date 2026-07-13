/**
 *  \file Transform/CollectVars.cc
 *
 *  \brief Collect variable references that need to be passed through
 *  the outlined-function call interface.  SgBasicBlock.
 */
// tps (01/14/2010) : Switching from rose.h to sage3.
#include "sage3basic.h"

#include <iostream>

#include <list>

#include <sstream>

#include <string>

#include <vector>

#include "Outliner.hh"
// #include "Transform.hh"

#include "ASTtools.hh"

#include "VarSym.hh"

// =====================================================================

using namespace std;

// =====================================================================
static void dump(const ASTtools::VarSymSet_t &V, const std::string &tag) {
  //  if (SgProject::get_verbose () >= 2)
  if (Outliner::enable_debug) {
    std::ostringstream out;
    out << tag << '{';
    bool first = true;
    for (ASTtools::VarSymSet_t::const_iterator i = V.begin(); i != V.end();
         ++i) {
      const SgVariableSymbol *sym = *i;
      if (!first)
        out << ", ";
      first = false;
      if (sym == NULL) {
        out << "(null)";
        continue;
      }
      const SgInitializedName *decl = sym->get_declaration();
      out << sym->get_name().getString() << "@sym=" << sym << "/decl=" << decl;
    }
    out << '}';
    MLOG_DEBUG_CXX("Outliner") << out.str();
  }
}

//! Collect the variables to be passed if 's' is to be outlined
//  The variables used (U) but not internally declared (L), and declared within
//  the function (not globally declared) should be passed as parameters to the
//  outlined function
//
// It classifies variables used in s as the following categories
//  * U: all used (referenced) variables within 's'
//  * L: locally (internally to be accurate) declared within the code block 's'
//  to be outlined
//  * Q: declared within the enclosing function surrounding 's'
//       but not globally declared beyond the function surrounding 's'  (global
//       variables should not be passed if the outlined function  is put within
//       the same file )
//
void Outliner::collectVars(
    const SgStatement *s,
    ASTtools::VarSymSet_t &syms) // return the symbols(variables) that need to
                                 // be passed into the outlined function
{
  // Determine the function definition surrounding 's'. The enclosing function
  // of 's'
  const SgFunctionDefinition *outer_func_s = ASTtools::findFirstFuncDef(s);
  ROSE_ASSERT(outer_func_s);

  // U = {symbols used within 's'}
  ASTtools::VarSymSet_t U;
  ASTtools::collectRefdVarSyms(s, U);
  dump(U, "U (variables used within s) = ");

  // L = {symbols defined within 's'}, local variables declared within 's'
  ASTtools::VarSymSet_t L;
  ASTtools::collectDefdVarSyms(s, L);
  dump(L, "L (local variables declared within s) = ");

  // U - L = {symbols used within 's' but not defined in 's'}
  // variable references to non-local-declared variables
  ASTtools::VarSymSet_t diff_U_L;
  set_difference(U.begin(), U.end(), L.begin(), L.end(),
                 inserter(diff_U_L, diff_U_L.begin()), ASTtools::VarSymLess());
  dump(diff_U_L, "U - L = ");

  ASTtools::VarSymSet_t Q;
  std::map<const SgInitializedName *, const SgVariableSymbol *> q_by_decl;

  // if the outlined function is put into a separated file
  // There are two choices for global variables
  // * pass them as parameters anyway to be lazy
  // * does not pass them as parameters, put extern xxx declarations in the
  // separated file
  if (Outliner::useNewFile) // lazy
  {
    syms = diff_U_L;
    // TODO  a better way is to find intersection of U-L and global variables
    // and
    //  add extern xxx, for them

  } else {
    // Q = {symbols defined within the function surrounding 's' that are visible
    // at 's'}, including function parameters
    ASTtools::collectLocalVisibleVarSyms(outer_func_s->get_declaration(), s, Q);
    dump(Q, "Q (variables defined within the function surrounding s that are "
            "visible at s) = ");

    // (U - L) \cap Q = {variables that need to be passed as parameters to the
    // outlined function}
    //
    // Use declaration-identity matching instead of raw symbol-pointer
    // intersection. Frontend/normalization paths may materialize distinct
    // symbol objects for the same declaration, which would otherwise drop valid
    // captures.
    for (ASTtools::VarSymSet_t::const_iterator i = Q.begin(); i != Q.end();
         ++i) {
      const SgVariableSymbol *sym = *i;
      if (sym == NULL)
        continue;
      const SgInitializedName *decl = sym->get_declaration();
      if (decl != NULL)
        q_by_decl[decl] = sym;
    }

    for (ASTtools::VarSymSet_t::const_iterator i = diff_U_L.begin();
         i != diff_U_L.end(); ++i) {
      const SgVariableSymbol *sym = *i;
      if (sym == NULL)
        continue;
      const SgInitializedName *decl = sym->get_declaration();
      if (decl == NULL)
        continue;
      std::map<const SgInitializedName *,
               const SgVariableSymbol *>::const_iterator where =
          q_by_decl.find(decl);
      if (where != q_by_decl.end())
        syms.insert(where->second);
    }
    dump(syms, "(U - L) InterSection Q [the variables to be passed into the "
               "outlined function]= ");
  }

  if (!SageInterface::is_Fortran_language())
    return;

  // A Fortran dummy's source declaration may depend on variables that are not
  // referenced by the outlined statements themselves.  For example, copying
  // ARRAY(N) into an external outlined procedure requires N in both the call
  // and the outlined signature.  Close the capture set over exact source-type
  // references before either interface is planned; assumed-shape conversion
  // would hide the missing dependency and produce an invalid external
  // procedure interface.
  std::vector<const SgVariableSymbol *> pending(syms.begin(), syms.end());
  std::set<const SgInitializedName *> processed;
  for (size_t index = 0; index < pending.size(); ++index) {
    const SgVariableSymbol *captured = pending[index];
    const SgInitializedName *captured_declaration =
        captured != NULL ? captured->get_declaration() : NULL;
    if (captured_declaration == NULL ||
        !processed.insert(captured_declaration).second)
      continue;

    SgType *source_type = captured_declaration->get_fortran_source_type();
    if (source_type == NULL)
      continue;

    Rose_STL_Container<SgNode *> references =
        NodeQuery::querySubTree(source_type, V_SgVarRefExp);
    for (SgNode *node : references) {
      SgVarRefExp *reference = isSgVarRefExp(node);
      SgVariableSymbol *dependency =
          reference != NULL ? reference->get_symbol() : NULL;
      SgInitializedName *dependency_declaration =
          dependency != NULL ? dependency->get_declaration() : NULL;
      if (reference == NULL || dependency == NULL ||
          dependency_declaration == NULL) {
        fprintf(stderr,
                "REX_OUTLINER_INVARIANT[fortran-type-dependency]: captured "
                "variable=%p/%s has an unresolved source-type reference\n",
                static_cast<const void *>(captured_declaration),
                captured_declaration->get_name().getString().c_str());
        ROSE_ABORT();
      }
      if (dependency_declaration == captured_declaration)
        continue;

      const SgVariableSymbol *dependency_to_capture = dependency;
      if (!Outliner::useNewFile) {
        const auto visible = q_by_decl.find(dependency_declaration);
        if (visible == q_by_decl.end()) {
          if (SageInterface::getEnclosingFunctionDefinition(
                  dependency_declaration) == outer_func_s) {
            fprintf(stderr,
                    "REX_OUTLINER_INVARIANT[fortran-type-dependency]: "
                    "captured variable=%p/%s depends on local variable=%p/%s "
                    "that is not visible at the outline site\n",
                    static_cast<const void *>(captured_declaration),
                    captured_declaration->get_name().getString().c_str(),
                    static_cast<void *>(dependency_declaration),
                    dependency_declaration->get_name().getString().c_str());
            ROSE_ABORT();
          }
          continue;
        }
        dependency_to_capture = visible->second;
      }

      const auto inserted = syms.insert(dependency_to_capture);
      if (inserted.second)
        pending.push_back(*inserted.first);
    }
  }
  dump(syms, "Fortran source-type dependency closure = ");
}

// eof
