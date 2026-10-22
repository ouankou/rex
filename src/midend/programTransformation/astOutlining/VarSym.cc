/*!
 *  \file ASTtools/VarSym.cc
 *
 *  \brief Implements routines to assist in variable symbol analysis
 *  and manipulation.
 */
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
                                     const SgInitializedName *name) {
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
  VariantT declaration_kind = V_SgNode;
};

static VarSymOrderKey makeVarSymOrderKey(const SgVariableSymbol *sym) {
  VarSymOrderKey key;
  ROSE_ASSERT(sym != nullptr && sym->get_declaration() != nullptr);
  const SgInitializedName *decl = sym->get_declaration();
  key.normalized_name = normalizedSymbolName(sym->get_name());

  const SgFunctionDeclaration *function =
      getEnclosingFunctionForSymbolOrder(decl);
  const int parameter_index = getFunctionParameterIndex(function, decl);
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
    key.declaration_kind = decl->get_declaration()->variantT();
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

static bool isPredefinedIdentifier(const SgVariableSymbol *symbol) {
  ROSE_ASSERT(symbol != nullptr && symbol->get_declaration() != nullptr);
  return symbol->get_declaration()->get_is_predefined_identifier();
}

// Follow the explicit redeclaration and alias identities established by the
// frontend or builder. Variable analysis must never manufacture missing
// symbols.
static const SgVariableSymbol *
getVarSymFromName_const(const SgInitializedName *name) {
  ROSE_ASSERT(name != nullptr);
  const SgFunctionDeclaration *parameterOwner =
      getDirectParameterOwnerFunctionDeclaration(name);
  SgSymbol *symbol = nullptr;
  if (parameterOwner != nullptr) {
    // A definition's dummy is a local binding. Its predecessor can be the
    // corresponding canonical signature parameter, which has no body-local
    // symbol. Resolve the exact defining parameter instead of treating that
    // signature relationship as a variable's canonical storage declaration.
    ROSE_ASSERT(parameterOwner->get_definition() != nullptr);
    ROSE_ASSERT(name->get_scope() == parameterOwner->get_definition());
    symbol = name->get_symbol_from_symbol_table();
  } else {
    symbol = name->search_for_symbol_from_symbol_table();
  }
  if (auto *alias = isSgAliasSymbol(symbol))
    symbol = alias->get_base();
  if (isSgEnumFieldSymbol(symbol) != nullptr)
    return nullptr;
  auto *variable = isSgVariableSymbol(symbol);
  if (variable == nullptr || variable->get_declaration() == nullptr) {
    fprintf(stderr,
            "REX_AST_INVARIANT[outliner-variable-symbol]: declaration=%p "
            "name=%s scope=%p has no canonical variable symbol\n",
            static_cast<const void *>(name), name->get_name().str(),
            static_cast<void *>(name->get_scope()));
    ROSE_ABORT();
  }
  return variable;
}

static SgVariableSymbol *getVarSymFromName(SgInitializedName *name) {
  return const_cast<SgVariableSymbol *>(getVarSymFromName_const(name));
}

/*!
 *  \brief Returns the SgVariableSymbol associated with an SgVarRefExp
 *  or SgInitializedName, or 0 if none.
 */
static const SgVariableSymbol *getVarSym_const(const SgNode *n) {
  const SgVariableSymbol *v_sym = 0;
  switch (n->variantT()) {
  case V_SgVarRefExp: {
    SgInitializedName *iname = SageInterface::convertRefToInitializedName(
        isSgVarRefExp(const_cast<SgNode *>(n)));
    return getVarSym_const(iname);
  }
  case V_SgInitializedName: {
    const SgInitializedName *iname = isSgInitializedName(n);
    ROSE_ASSERT(iname != NULL);
    v_sym = getVarSymFromName_const(iname);
    if (v_sym != nullptr && isPredefinedIdentifier(v_sym))
      v_sym = nullptr;
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
    if (!isPredefinedIdentifier(v_sym))
      syms.insert(v_sym);
  } break;
  case V_SgVariableDeclaration: {
    SgVariableDeclaration *v_decl = isSgVariableDeclaration(n);
    ROSE_ASSERT(v_decl);
    SgInitializedNamePtrList &names = v_decl->get_variables();
    for (SgInitializedName *name : names) {
      SgVariableSymbol *symbol = getVarSymFromName(name);
      if (symbol != nullptr && !isPredefinedIdentifier(symbol))
        syms.insert(symbol);
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

bool ASTtools::requiresVariableCapture(const SgVarRefExp *reference) {
  ROSE_ASSERT(reference != nullptr && reference->get_symbol() != nullptr);
  const SgInitializedName *name = reference->get_symbol()->get_declaration();
  ROSE_ASSERT(name != nullptr);
  if (!reference->get_is_non_odr_use() ||
      isSgClassDefinition(name->get_scope()) == nullptr)
    return true;
  auto *declaration = isSgVariableDeclaration(name->get_declptr());
  ROSE_ASSERT(declaration != nullptr);
  // A non-ODR member reference needs its declaration, not a captured address.
  return !SageInterface::isStatic(declaration);
}

void ASTtools::collectRefdVarSyms(const SgNode *s, VarSymSet_t &syms) {
  // First, collect all variable reference expressions, {e}
  typedef Rose_STL_Container<SgNode *> NodeList_t;
  NodeList_t var_refs =
      NodeQuery::querySubTree(const_cast<SgNode *>(s), V_SgVarRefExp);
  SageInterface::addVarRefExpFromArrayDimInfo(const_cast<SgNode *>(s),
                                              var_refs);
  for (SgNode *node : var_refs) {
    auto *reference = isSgVarRefExp(node);
    ROSE_ASSERT(reference != nullptr && reference->get_symbol() != nullptr);
    if (isPredefinedIdentifier(reference->get_symbol()) ||
        !requiresVariableCapture(reference))
      continue;
    SgVariableSymbol *symbol = getVarSym(reference);
    ROSE_ASSERT(symbol != nullptr);
    syms.insert(symbol);
  }
}

// ========================================================================

void ASTtools::collectDefdVarSyms(const SgNode *s, VarSymSet_t &syms) {
  typedef Rose_STL_Container<SgNode *> NodeList_t;
  NodeList_t vars_local =
      NodeQuery::querySubTree(const_cast<SgNode *>(s), V_SgVariableDeclaration);
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
          getDirectParameterOwnerFunctionDeclaration(name);
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
