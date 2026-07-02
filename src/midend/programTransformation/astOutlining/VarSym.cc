/*!
 *  \file ASTtools/VarSym.hh
 *
 *  \brief Implements routines to assist in variable symbol analysis
 *  and manipulation.
 */
// tps (01/14/2010) : Switching from rose.h to sage3.
#include "sage3basic.h"

#include <algorithm>
#include <functional>
#include <sstream>

#include "VarSym.hh"

// ========================================================================

using namespace std;

// ========================================================================

namespace {

static std::string normalizedSymbolName(const SgName &name) {
  return Rose::StringUtility::convertToLowerCase(name.getString());
}

static std::string normalizedSymbolName(const SgVariableSymbol *sym,
                                        const SgInitializedName *decl) {
  if (sym != NULL && !sym->get_name().getString().empty())
    return normalizedSymbolName(sym->get_name());
  if (decl != NULL)
    return normalizedSymbolName(decl->get_name());
  return std::string();
}

static const SgFunctionDeclaration *
getDirectParameterOwnerFunctionDeclaration(const SgInitializedName *name) {
  if (name == NULL)
    return NULL;

  const SgFunctionParameterList *param_list =
      isSgFunctionParameterList(name->get_parent());
  if (param_list == NULL)
    return NULL;

  return isSgFunctionDeclaration(param_list->get_parent());
}

static const SgFunctionDeclaration *
getEnclosingFunctionForSymbolOrder(const SgInitializedName *name) {
  if (name == NULL)
    return NULL;

  if (const SgFunctionDeclaration *owner =
          getDirectParameterOwnerFunctionDeclaration(name))
    return owner;

  if (SgDeclarationStatement *decl =
          const_cast<SgDeclarationStatement *>(name->get_declaration())) {
    if (SgFunctionDeclaration *enclosing =
            SageInterface::getEnclosingFunctionDeclaration(decl, false))
      return enclosing;
  }

  if (SgScopeStatement *scope =
          const_cast<SgScopeStatement *>(name->get_scope()))
    return SageInterface::getEnclosingFunctionDeclaration(scope, true);

  return NULL;
}

static int getFunctionParameterIndex(const SgFunctionDeclaration *function,
                                     const SgInitializedName *name,
                                     const std::string &normalized_name) {
  if (function == NULL || name == NULL)
    return -1;

  SgFunctionParameterList *params =
      const_cast<SgFunctionDeclaration *>(function)->get_parameterList();
  if (params == NULL)
    return -1;

  SgInitializedNamePtrList &args = params->get_args();
  for (SgInitializedNamePtrList::size_type i = 0; i < args.size(); ++i) {
    SgInitializedName *arg = args[i];
    if (arg == NULL)
      continue;
    if (arg == name)
      return static_cast<int>(i);
  }

  for (SgInitializedNamePtrList::size_type i = 0; i < args.size(); ++i) {
    SgInitializedName *arg = args[i];
    if (arg == NULL)
      continue;
    if (normalizedSymbolName(arg->get_name()) == normalized_name)
      return static_cast<int>(i);
  }

  return -1;
}

static int getInitializedNameListIndex(const SgInitializedName *name) {
  if (name == NULL)
    return -1;

  if (const SgVariableDeclaration *var_decl =
          isSgVariableDeclaration(name->get_declaration())) {
    SgInitializedNamePtrList &vars =
        const_cast<SgVariableDeclaration *>(var_decl)->get_variables();
    for (SgInitializedNamePtrList::size_type i = 0; i < vars.size(); ++i) {
      if (vars[i] == name)
        return static_cast<int>(i);
    }
  }

  if (const SgFunctionParameterList *param_list =
          isSgFunctionParameterList(name->get_parent())) {
    SgInitializedNamePtrList &args =
        const_cast<SgFunctionParameterList *>(param_list)->get_args();
    for (SgInitializedNamePtrList::size_type i = 0; i < args.size(); ++i) {
      if (args[i] == name)
        return static_cast<int>(i);
    }
  }

  return -1;
}

static const Sg_File_Info *
getBestFileInfoForSymbolOrder(const SgInitializedName *name) {
  if (name != NULL) {
    if (const Sg_File_Info *info = name->get_startOfConstruct())
      return info;
    if (const Sg_File_Info *info = name->get_file_info())
      return info;
    if (const SgDeclarationStatement *decl = name->get_declaration()) {
      if (const Sg_File_Info *info = decl->get_startOfConstruct())
        return info;
      if (const Sg_File_Info *info = decl->get_file_info())
        return info;
    }
  }
  return NULL;
}

struct VarSymOrderKey {
  bool has_parameter_index = false;
  int parameter_index = -1;
  bool has_source_position = false;
  int file_id = -1;
  int physical_file_id = -1;
  int line = -1;
  int column = -1;
  int declaration_list_index = -1;
  std::string normalized_name;
  std::string declaration_kind;
};

static VarSymOrderKey makeVarSymOrderKey(const SgVariableSymbol *sym) {
  VarSymOrderKey key;
  const SgInitializedName *decl = sym != NULL ? sym->get_declaration() : NULL;
  key.normalized_name = normalizedSymbolName(sym, decl);

  const SgFunctionDeclaration *function =
      getEnclosingFunctionForSymbolOrder(decl);
  const int parameter_index =
      getFunctionParameterIndex(function, decl, key.normalized_name);
  if (parameter_index >= 0) {
    key.has_parameter_index = true;
    key.parameter_index = parameter_index;
  }

  if (const Sg_File_Info *info = getBestFileInfoForSymbolOrder(decl)) {
    key.file_id = info->get_file_id();
    key.physical_file_id = info->get_physical_file_id();
    key.line = info->get_line();
    key.column = info->get_col();
    key.has_source_position = key.line > 0 || key.column > 0 ||
                              key.file_id >= 0 || key.physical_file_id >= 0;
  }

  key.declaration_list_index = getInitializedNameListIndex(decl);
  if (decl != NULL && decl->get_declaration() != NULL)
    key.declaration_kind = decl->get_declaration()->class_name();
  return key;
}

template <typename T> static int compareScalar(const T &lhs, const T &rhs) {
  if (lhs < rhs)
    return -1;
  if (rhs < lhs)
    return 1;
  return 0;
}

static int compareVarSymOrderKey(const VarSymOrderKey &lhs,
                                 const VarSymOrderKey &rhs) {
  if (int cmp = compareScalar(lhs.has_parameter_index, rhs.has_parameter_index))
    return -cmp; // symbols matching formal parameters sort first
  if (lhs.has_parameter_index) {
    if (int cmp = compareScalar(lhs.parameter_index, rhs.parameter_index))
      return cmp;
  }

  if (int cmp = compareScalar(lhs.has_source_position, rhs.has_source_position))
    return -cmp; // source-positioned declarations sort before generated ties
  if (lhs.has_source_position) {
    if (int cmp = compareScalar(lhs.file_id, rhs.file_id))
      return cmp;
    if (int cmp = compareScalar(lhs.physical_file_id, rhs.physical_file_id))
      return cmp;
    if (int cmp = compareScalar(lhs.line, rhs.line))
      return cmp;
    if (int cmp = compareScalar(lhs.column, rhs.column))
      return cmp;
  }

  if (int cmp =
          compareScalar(lhs.declaration_list_index, rhs.declaration_list_index))
    return cmp;
  if (int cmp = compareScalar(lhs.normalized_name, rhs.normalized_name))
    return cmp;
  if (int cmp = compareScalar(lhs.declaration_kind, rhs.declaration_kind))
    return cmp;
  return 0;
}

} // namespace

bool ASTtools::VarSymLess::operator()(const SgVariableSymbol *lhs,
                                      const SgVariableSymbol *rhs) const {
  if (lhs == rhs)
    return false;
  if (lhs == NULL)
    return rhs != NULL;
  if (rhs == NULL)
    return false;

  const VarSymOrderKey lhs_key = makeVarSymOrderKey(lhs);
  const VarSymOrderKey rhs_key = makeVarSymOrderKey(rhs);
  if (int cmp = compareVarSymOrderKey(lhs_key, rhs_key))
    return cmp < 0;

  return std::less<const SgVariableSymbol *>()(lhs, rhs);
}

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
