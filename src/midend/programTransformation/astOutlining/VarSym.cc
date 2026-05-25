/*!
 *  \file ASTtools/VarSym.hh
 *
 *  \brief Implements routines to assist in variable symbol analysis
 *  and manipulation.
 */
// tps (01/14/2010) : Switching from rose.h to sage3.
#include "sage3basic.h"

#include <algorithm>
#include <sstream>

#include "VarSym.hh"

// ========================================================================

using namespace std;

// ========================================================================

static bool isCompilerGeneratedOnly(const SgLocatedNode *node) {
  if (node == NULL)
    return false;
  const Sg_File_Info *fi = node->get_file_info();
  return fi != NULL && fi->isCompilerGenerated() && !fi->isTransformation();
}

static bool isCompilerGeneratedPlaceholder(const SgInitializedName *name) {
  if (name == NULL)
    return false;
  if (name->get_symbol_from_symbol_table() != NULL)
    return false;

  const SgDeclarationStatement *decl = name->get_declaration();
  return isCompilerGeneratedOnly(name) || isCompilerGeneratedOnly(decl);
}

static SgVariableSymbol *
findVariableSymbolByDeclaration(SgScopeStatement *scope,
                                const SgInitializedName *name) {
  if (scope == NULL || name == NULL)
    return NULL;

  SgSymbolTable *table = scope->get_symbol_table();
  if (table == NULL)
    return NULL;

  std::set<SgNode *> symbols = table->get_symbolSet();
  for (std::set<SgNode *>::iterator i = symbols.begin(); i != symbols.end();
       ++i) {
    if (SgVariableSymbol *sym = isSgVariableSymbol(*i)) {
      if (sym->get_declaration() == name)
        return sym;
    } else if (SgAliasSymbol *alias = isSgAliasSymbol(*i)) {
      if (SgVariableSymbol *base = isSgVariableSymbol(alias->get_alias())) {
        if (base->get_declaration() == name)
          return base;
      }
    }
  }

  return NULL;
}

static bool canSynthesizeVariableSymbol(const SgInitializedName *name) {
  if (name == NULL)
    return false;
  if (name->get_scope() == NULL)
    return false;
  if (isCompilerGeneratedPlaceholder(name))
    return false;
  return isSgVariableDeclaration(name->get_declaration()) != NULL;
}

static bool isBuiltinFunctionMacroName(const std::string &name) {
  return name == "__PRETTY_FUNCTION__" || name == "__FUNCTION__" ||
         name == "__func__";
}

static const SgFunctionDeclaration *
getParameterOwnerFunctionDeclaration(const SgInitializedName *name) {
  if (name == NULL)
    return NULL;

  const SgFunctionParameterList *param_list =
      isSgFunctionParameterList(name->get_parent());
  if (param_list == NULL)
    return NULL;

  return isSgFunctionDeclaration(param_list->get_parent());
}

//! Converts a set of variable symbols into a string for debugging.
string ASTtools::toString(const VarSymSet_t &syms) {
  stringstream s;
  ASTtools::VarSymSet_t::const_iterator v = syms.begin();
  bool is_first = true;
  while (v != syms.end()) {
    if (is_first)
      is_first = false;
    else
      s << ", ";
    const SgVariableSymbol *sym = *v;
    if (sym) {
      const SgInitializedName *n = sym->get_declaration();
      ROSE_ASSERT(n);
      s << n->get_name().str();
    } else // !sym
      s << "(nil?)";
    ++v;
  }
  return s.str();
}

// ========================================================================

/*!
 *  \brief Return an existing variable symbol for the given
 *  initialized name.
 *
 *  This routine checks various scopes in trying to find a suitable
 *  variable symbol for the given initialized name.
 */
static const SgVariableSymbol *
getVarSymFromName_const(const SgInitializedName *name) {
  SgVariableSymbol *v_sym = 0;
  if (name) {
    // Keep the initialized-name symbol as a fallback only. We prefer symbols
    // recovered from scope tables so ref/visible-set intersections use one
    // canonical symbol object.
    SgVariableSymbol *direct_sym =
        isSgVariableSymbol(name->get_symbol_from_symbol_table());

    SgScopeStatement *s = name->get_scope();
    ROSE_ASSERT(s);
    v_sym = s->lookup_var_symbol(name->get_name());
    if (v_sym != NULL && v_sym->get_declaration() != name) {
      if (SgVariableSymbol *by_decl = findVariableSymbolByDeclaration(s, name))
        v_sym = by_decl;
    }
    if (v_sym == NULL) {
      SgVariableSymbol *parent_scope_sym =
          SageInterface::lookupVariableSymbolInParentScopes(name->get_name(),
                                                            s);
      if (parent_scope_sym != NULL &&
          parent_scope_sym->get_declaration() == name) {
        v_sym = parent_scope_sym;
      }
    }

    if (!v_sym) // E.g., might be part of an 'extern' declaration.
    {
      // Try the declaration's scope.
      SgDeclarationStatement *decl = name->get_declaration();
      ROSE_ASSERT(decl);

      SgScopeStatement *decl_scope = decl->get_scope();
      if (decl_scope)
        v_sym = decl_scope->lookup_var_symbol(name->get_name());
      if (v_sym != NULL && v_sym->get_declaration() != name) {
        if (SgVariableSymbol *by_decl =
                findVariableSymbolByDeclaration(decl_scope, name))
          v_sym = by_decl;
      }

      if (!v_sym && decl_scope) {
        SgVariableSymbol *decl_scope_sym =
            SageInterface::lookupVariableSymbolInParentScopes(name->get_name(),
                                                              decl_scope);
        if (decl_scope_sym != NULL &&
            decl_scope_sym->get_declaration() == name) {
          v_sym = decl_scope_sym;
        }
      }

      if (!v_sym && decl_scope)
        v_sym = findVariableSymbolByDeclaration(decl_scope, name);
      if (!v_sym)
        v_sym = findVariableSymbolByDeclaration(s, name);

      if (!v_sym && direct_sym)
        v_sym = direct_sym->get_declaration() == name ? direct_sym : NULL;

      // Transformation-introduced declarations can exist in the AST before
      // their scope tables are fully populated. Recover by inserting the
      // declaration's variable symbol into the owning scope.
      if (!v_sym && canSynthesizeVariableSymbol(name)) {
        SgScopeStatement *owner_scope = name->get_scope();
        ROSE_ASSERT(owner_scope);
        SgVariableSymbol *new_sym =
            new SgVariableSymbol(const_cast<SgInitializedName *>(name));
        owner_scope->insert_symbol(name->get_name(), new_sym);
        v_sym = new_sym;
      }

      if (!v_sym)
        MLOG_WARN_CXX("Outliner")
            << "getVarSymFromName_const(): cannot find symbol for '"
            << name->get_name().str() << "'";
    }
  }
  return v_sym;
}

/*!
 *  \brief Return an existing variable symbol for the given
 *  initialized name.
 *
 *  This routine checks various scopes in trying to find a suitable
 *  variable symbol for the given initialized name.
 */
static SgVariableSymbol *getVarSymFromName(SgInitializedName *name) {
  const SgVariableSymbol *v_sym = getVarSymFromName_const(name);
  if (v_sym == NULL) {
    // cerr<<"Warning: VarSym.cc getVarSymFromName() fails to find a symbol
    // for:"<<name->get_name().getString()<<endl; ROSE_ASSERT (v_sym != NULL);
  }
  return const_cast<SgVariableSymbol *>(v_sym);
}

/*!
 *  \brief Returns the SgVariableSymbol associated with an SgVarRefExp
 *  or SgInitializedName, or 0 if none.
 */
static const SgVariableSymbol *getVarSym_const(const SgNode *n) {
  const SgVariableSymbol *v_sym = 0;
  switch (n->variantT()) {
  case V_SgVarRefExp: {
    // We want to handle a->b  case and return a instead of b
    // Converge to SgInitializedName
    //  v_sym = isSgVarRefExp(n)->get_symbol ();
    SgInitializedName *iname = SageInterface::convertRefToInitializedName(
        isSgVarRefExp(const_cast<SgNode *>(n)));
    return getVarSym_const(iname);
    break;
  }
  case V_SgInitializedName: {
    const SgInitializedName *iname = isSgInitializedName(n);
    ROSE_ASSERT(iname != NULL);
    v_sym = getVarSymFromName_const(iname);
    // TODO: support references to enumerate types in a code block
    SgSymbol *symbol = iname->get_symbol_from_symbol_table();
    SgEnumFieldSymbol *efs = isSgEnumFieldSymbol(symbol);
    if (efs != NULL) {
      MLOG_WARN_CXX("Outliner")
          << "getVarSym_const(): unsupported SgEnumFieldSymbol";
      return NULL;
    }

    if (v_sym == NULL) {
      MLOG_WARN_CXX("Outliner") << "getVarSym_const(): did not find symbol for "
                                << n->unparseToString();
      // Cannot find symbol for omp runtime functions in Fortran code right now
      bool placeholder = isCompilerGeneratedPlaceholder(iname);
      if (!SageInterface::is_Fortran_language() && !placeholder)
        ROSE_ASSERT(v_sym != NULL);
      // GCC macros __FUNCTION__ and __PRETTY_FUNCTION__ have no symbols in ROSE
      // for some reason
    } else { // Liao, 12/18/2012. We should ignore built in variables since they
             // should not be passed (by value/ref) into the outlined functions
      string name = v_sym->get_name().getString();
      if (isBuiltinFunctionMacroName(name))
        v_sym = NULL;
    }
    break;
  }
  default:
    break;
  }
  return v_sym;
}

/*!
 *  \brief Returns the SgVariableSymbol associated with an SgVarRefExp
 *  or SgInitializedName, or 0 if none.
 */
static SgVariableSymbol *getVarSym(SgNode *n) {
  const SgVariableSymbol *v_sym = getVarSym_const(n);
  return const_cast<SgVariableSymbol *>(v_sym);
}

/*!
 *  Collect all SgVariableSymbols associated with an SgVarRefExp node
 *  a SgVariableDeclaration node,  or a SgInitializedName (function parameters)
 */
static void getVarSyms(SgNode *n, ASTtools::VarSymSet_t *p_syms) {
  if (!p_syms || !n)
    return;

  ASTtools::VarSymSet_t &syms = *p_syms;

  switch (n->variantT()) {
  case V_SgVariableSymbol: {
    SgVariableSymbol *v_sym = isSgVariableSymbol(n);
    ROSE_ASSERT(v_sym);
    string name = v_sym->get_name().getString();
    if (!isBuiltinFunctionMacroName(name)) {
      // cout<<"debug: L181 inserting "<<v_sym->get_name() <<endl;
      syms.insert(v_sym);
    }
  } break;
  case V_SgVariableDeclaration: {
    SgVariableDeclaration *v_decl = isSgVariableDeclaration(n);
    ROSE_ASSERT(v_decl);
    SgInitializedNamePtrList &names = v_decl->get_variables();
    //        transform (names.begin (), names.end (),
    //                   inserter (syms, syms.begin ()),
    //                   getVarSymFromName);
    for (SgInitializedNamePtrList::iterator iter = names.begin();
         iter != names.end(); iter++) {
      SgVariableSymbol *v_sym = getVarSymFromName(*iter);
      // We relax for Fortran since the external function_name is not well
      // implemented right now Liao 1/21/2010
      // TODO
      if (v_sym == NULL) {
        // if (SageInterface::is_Fortran_language() )
        MLOG_WARN_CXX("Outliner") << "getVarSyms(): cannot find symbol for "
                                  << (*iter)->get_name().getString();
        // else
        // ROSE_ASSERT (v_sym != NULL);
      }

      if (v_sym) {
        string name = v_sym->get_name().getString();
        if (!isBuiltinFunctionMacroName(name)) {
          // cout<<"debug: L209 inserting "<<v_sym->get_name() <<endl;
          syms.insert(v_sym);
        }
      }
    }
  } break;
  case V_SgInitializedName: {
    SgInitializedName *name = isSgInitializedName(n);
    ROSE_ASSERT(name);
    getVarSyms(getVarSym(name), p_syms);
  }
  default:
    break;
  }
}

// ========================================================================

void ASTtools::collectRefdVarSyms(const SgStatement *s, VarSymSet_t &syms) {
  // First, collect all variable reference expressions, {e}
  typedef Rose_STL_Container<SgNode *> NodeList_t;
  NodeList_t var_refs =
      NodeQuery::querySubTree(const_cast<SgStatement *>(s), V_SgVarRefExp);
  //  NodeList_t type_list = NodeQuery::querySubTree (const_cast<SgStatement *>
  //  (s), V_SgType,AstQueryNamespace::ExtractTypes);

  SageInterface::addVarRefExpFromArrayDimInfo(const_cast<SgStatement *>(s),
                                              var_refs);
  // Next, insert the variable symbol for each e into syms.
  for (NodeList_t::iterator iter = var_refs.begin(); iter != var_refs.end();
       iter++) {
    SgVarRefExp *vref = isSgVarRefExp(*iter);
    ROSE_ASSERT(vref != NULL);
    SgInitializedName *iname = NULL;
    SgVariableSymbol *symbol = NULL;

    if (vref->get_symbol() != NULL) {
      // SageInterface::convertRefToInitializedName() will ignore builtin
      // functions, getVarSym() calls it internally.
      // so we do builtin function ref check first, later we can safely assert
      // symbol != NULL
      string vname = vref->get_symbol()->get_name().getString();
      if (isBuiltinFunctionMacroName(vname))
        continue;

      symbol = getVarSym(*iter);
      iname = SageInterface::convertRefToInitializedName(vref);
    } else {
      // Some transformed references can temporarily lose direct symbols.
      // Recover via initialized-name mapping so outlining still captures them.
      iname = SageInterface::convertRefToInitializedName(vref);
      if (iname != NULL) {
        string iname_name = iname->get_name().getString();
        if (isBuiltinFunctionMacroName(iname_name))
          continue;

        symbol = getVarSym(iname);
      }
    }

    if (symbol) {
      syms.insert(symbol);
      continue;
    }

    if (iname != NULL && isCompilerGeneratedPlaceholder(iname))
      continue;
    MLOG_ERROR_CXX("Outliner")
        << "ASTtools::collectRefdVarSyms() found NULL symbol for SgVarRefExp: "
        << *iter;
    if (!SageInterface::is_Fortran_language())
      ROSE_ASSERT(symbol != NULL);
  }
}

// ========================================================================

void ASTtools::collectDefdVarSyms(const SgStatement *s, VarSymSet_t &syms) {
  typedef Rose_STL_Container<SgNode *> NodeList_t;
  NodeList_t vars_local = NodeQuery::querySubTree(const_cast<SgStatement *>(s),
                                                  V_SgVariableDeclaration);
  for_each(vars_local.begin(), vars_local.end(),
           [&syms](SgNode *n) { getVarSyms(n, &syms); });

  for (VarSymSet_t::iterator it = syms.begin(); it != syms.end(); it++)
    ROSE_ASSERT(*it != NULL);
}

void ASTtools::collectLocalVisibleVarSyms(const SgStatement *root,
                                          const SgStatement *target,
                                          VarSymSet_t &syms) {
  //! Traversal to collect variable symbols, with early stopping.
  class Collector : public AstSimpleProcessing {
  public:
    Collector(const SgStatement *target, VarSymSet_t &syms)
        : target_(target), syms_(syms), root_function_decl_(NULL) {}

    void setRoot(const SgStatement *root) {
      root_function_decl_ = isSgFunctionDeclaration(root);
    }

    virtual void visit(SgNode *n) {
      // Stop the traversal once target node is met.
      if (isSgStatement(n) == target_)
        throw string("done");
      if (isParameterOfNestedFunctionDeclaration(n))
        return;
      getVarSyms(n, &syms_);
      // Liao, 12/18/2007
      // for Fortran, variables without declarations are legal,but easy to miss
      // grab them all from symbol tables
      SgScopeStatement *scope = isSgScopeStatement(n);
      if (scope) {
        SgSymbolTable *table = scope->get_symbol_table();
        std::set<SgNode *> nodeset = table->get_symbolSet();
        for (std::set<SgNode *>::iterator i = nodeset.begin();
             i != nodeset.end(); i++) {
          SgVariableSymbol *varsymbol = isSgVariableSymbol(*i);
          if (varsymbol)
            getVarSyms(varsymbol, &syms_);
        }
      } // end if scope
    }

  private:
    bool isParameterOfNestedFunctionDeclaration(const SgNode *n) const {
      const SgInitializedName *name = isSgInitializedName(n);
      if (name == NULL)
        return false;

      const SgFunctionDeclaration *owner =
          getParameterOwnerFunctionDeclaration(name);
      return owner != NULL && owner != root_function_decl_;
    }

    const SgStatement *target_; //!< Node at which to stop search.
    VarSymSet_t &syms_;         //!< Container in which to collect symbols.
    const SgFunctionDeclaration *root_function_decl_;
  };

  // Do collection
  Collector collector(target, syms);
  collector.setRoot(root);
  try {
    collector.traverse(const_cast<SgStatement *>(root), preorder);
  } catch (string &stopped_early) {
    ROSE_ASSERT(stopped_early == "done");
  }
}

//! Collect variable reference a using addresses within s,
// including &a expression and foo(a) when type2 foo(Type& parameter) in C++
void ASTtools::collectVarRefsUsingAddress(const SgStatement *s,
                                          std::set<SgVarRefExp *> &varSetB) {
  SageInterface::collectUseByAddressVariableRefs(s, varSetB);
}

//! Collect variable references with a type which does not support =operator or
//! copy construction
// TODO this function can be merged with the one above for better performance,
// but separated out for clarity
void ASTtools::collectVarRefsOfTypeWithoutAssignmentSupport(
    const SgStatement *s, std::set<SgVarRefExp *> &varSetB) {
  Rose_STL_Container<SgNode *> var_refs =
      NodeQuery::querySubTree(const_cast<SgStatement *>(s), V_SgVarRefExp);
  Rose_STL_Container<SgNode *>::iterator iter = var_refs.begin();
  for (; iter != var_refs.end(); iter++) {
    SgVarRefExp *ref = isSgVarRefExp(*iter);
    SgType *vtype =
        isSgVariableSymbol(ref->get_symbol())->get_declaration()->get_type();
    if (!SageInterface::isCopyConstructible(vtype) ||
        (!SageInterface::isAssignable(vtype))) {
      if (Outliner::enable_debug)
        MLOG_DEBUG_CXX("Outliner")
            << "Reference does not support copy construction/assignment: "
            << ref->unparseToString();
      varSetB.insert(ref);
    }
  }
}

//! Collect variables to be replaced by pointer dereferencing (pd)
// We collect those used by address OR those do not support assignment
// We exclude C++ reference types since they do not support dereferencing
// We also collect structure or class types, passing by reference is more
// efficient for them ?PointerDereferenceingVars = ?  PassByRefParameters
// \intersection (UsingByAddress \union NotAssignableVars) -
// PointerDereferencedVars
//
// pdSyms = useByAddressVars + Non-assignableVars + Struct/ClassVars
//   Liao, 8/14/2009
void ASTtools::collectPointerDereferencingVarSyms(const SgStatement *s,
                                                  VarSymSet_t &pdSyms) {
  std::set<SgVarRefExp *>
      varSetB; // use by address (&a) or not assignable (a=..)
  std::set<SgVarRefExp *>::const_iterator iter;

  // use by address
  collectVarRefsUsingAddress(s, varSetB);
  // not assignable
  collectVarRefsOfTypeWithoutAssignmentSupport(s, varSetB);

  // Also collect structure or class types, passing by reference is more
  // efficient for them
  Rose_STL_Container<SgNode *> var_refs =
      NodeQuery::querySubTree(const_cast<SgStatement *>(s), V_SgVarRefExp);
  Rose_STL_Container<SgNode *>::iterator iter2 = var_refs.begin();
  for (; iter2 != var_refs.end(); iter2++) {
    SgVarRefExp *ref = isSgVarRefExp(*iter2);
    SgType *vtype =
        isSgVariableSymbol(ref->get_symbol())->get_declaration()->get_type();
    // cout<<"Debug: ASTtools::collectPointerDereferencingVarSyms() vtype is
    // :"<< vtype->class_name() <<endl;
    if (isSgClassType(vtype)) {
      if (Outliner::enable_debug)
        MLOG_DEBUG_CXX("Outliner")
            << "Found class/structure reference: " << ref->unparseToString();
      varSetB.insert(ref);
    }
  }

  // convert variable references to symbols
  for (iter = varSetB.begin(); iter != varSetB.end(); iter++) {
    SgVarRefExp *ref = *iter;
    ROSE_ASSERT(ref->get_symbol() != NULL);
    if (!isSgReferenceType(ref->get_type())) // exclude C++ reference type
      pdSyms.insert(ref->get_symbol());
  }

  if (Outliner::enable_debug) {
    std::ostringstream out;
    out << "collectPointerDereferencingVarSyms(): found " << pdSyms.size()
        << " symbols requiring pointer dereference rewriting: ";
    for (VarSymSet_t::const_iterator iter = pdSyms.begin();
         iter != pdSyms.end(); iter++)
      out << (*iter)->get_name().getString() << " ";
    MLOG_DEBUG_CXX("Outliner") << out.str();
  }
}

// eof
