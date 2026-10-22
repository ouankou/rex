#include "featureTests.h"
#ifdef ROSE_ENABLE_SOURCE_ANALYSIS

#include "cfgUtils.h"

#include "rwAccessLabeler.h"

#include "sageInterface.h"
#include "variables.h"

#include <algorithm>
#include <functional>
#include <set>

using namespace std;
using namespace SageInterface;

#define SgDefaultFile                                                          \
  Sg_File_Info::generateDefaultFileInfoForTransformationNode()

// given a SgInitializedName, returns a SgVarRefExp to the variable declared in
// the SgInitializedName
SgVarRefExp *varRefFromInitName(SgInitializedName *initName) {
  SgVariableSymbol *sym = new SgVariableSymbol(initName);
  ROSE_ASSERT(sym);
  SgVarRefExp *refExp = new SgVarRefExp(SgDefaultFile, sym);
  ROSE_ASSERT(refExp);
  sym->set_parent(refExp);

  return refExp;
}

/******************************
 ********* varID CODE *********
 ******************************/

bool varID::init(const varID &that) {
  components = that.components;
  varType = that.varType;
  expression = that.expression;
  name = that.name;

  return true;
}

// initializes this object from the given expression (assumed that
// isValidVarExp(n) is true) returns true on success, false on failure
bool varID::init(SgNode *n) {
  ROSE_ASSERT(isValidVarExp(n));
  if (isSgExpression(n))
    return init(isSgExpression(n));
  else if (isSgInitializedName(n))
    return init(isSgInitializedName(n));
  return false;
}

// initializes this object from the given expression (assumed that
// isValidVarExp(exp) is true) returns true on success, false on failure
bool varID::init(const SgExpression *exp) {
  exp = cfgUtils::unwrapCasts(exp);
  ROSE_ASSERT(exp != nullptr);
  components.clear();
  name.clear();
  expression = nullptr;
  varType = exp->get_type();
  if (isValidVarExp(exp)) {
    const SgVarRefExp *refExp;
    const SgDotExp *dotExp;
    if ((refExp = isSgVarRefExp(exp))) {
      components.push_back(refExp->get_symbol()->get_declaration());
      return true;
    } else if ((dotExp = isSgDotExp(exp))) {
      collectDotComponents(dotExp);
      return true;
    }
  }
  return false;
}

// initializes this object from the given SgInitializedName (assumed that
// isValidVarExp(name) is true)
bool varID::init(SgInitializedName *name) {
  components.clear();
  this->name.clear();
  components.push_back(name);
  varType = name->get_type();
  expression = nullptr;
  return true;
}

varID varID::forExpression(const SgExpression *exp) {
  ROSE_ASSERT(exp != nullptr);
  varID result;
  result.expression = exp;
  return result;
}

void varID::collectDotComponents(const SgDotExp *dotExp) {
  SgExpression *lhs = dotExp->get_lhs_operand();
  ROSE_ASSERT(lhs);
  if (SgVarRefExp *reference = isSgVarRefExp(lhs)) {
    components.push_back(reference->get_symbol()->get_declaration());
  } else {
    SgDotExp *nested = isSgDotExp(lhs);
    ROSE_ASSERT(nested != nullptr);
    collectDotComponents(nested);
  }

  SgVarRefExp *rhs = isSgVarRefExp(dotExp->get_rhs_operand());
  ROSE_ASSERT(rhs);
  components.push_back(rhs->get_symbol()->get_declaration());
}

// returns the scope in which this variable was declared
// for compound variables (i.e. those composed of dot expressions), it is the
// scope of the leftmost name
SgScopeStatement *varID::getScope() const {
  ROSE_ASSERT(components.size() > 0);
  return components[0]->get_scope();
}

// returns a SgExpression that corresponds to an access of this variable
SgExpression *varID::toSgExpression() const {
  ROSE_ASSERT(components.size() > 0 && varType != NULL);
  return toSgExpression_rec(components.begin());
}

// returns a SgExpression that corresponds to an access of this variable,
// including only the components at or after the iterator rest into the
// components vector
SgExpression *varID::toSgExpression_rec(
    vector<SgInitializedName *>::const_iterator rest) const {
  SgInitializedName *curName = (*rest);

  rest++;
  // if this is the last component
  if (rest == components.end()) {
    return varRefFromInitName(curName);
  } else {
    SgVarRefExp *lhs = varRefFromInitName(curName);
    SgExpression *rhs = toSgExpression_rec(rest);
    SgDotExp *de = new SgDotExp(SgDefaultFile, lhs, rhs, varType);
    lhs->set_parent(de);
    rhs->set_parent(de);
    return de;
  }
}

bool varID::isValidVarExp(const SgNode *n) {
  if (isSgExpression(n))
    return isValidVarExp(isSgExpression(n));
  else if (isSgInitializedName(n))
    return isValidVarExp(isSgInitializedName(n));
  else
    return false;
}

// returns true if the given expression is one that can be represented as a
// variable in our representation
bool varID::isValidVarExp(const SgExpression *exp) {
  ROSE_ASSERT(exp->get_parent());

  exp = cfgUtils::unwrapCasts(exp);

  return // exp is valid if it is not part of a larger dot expression
      (!isSgDotExp(exp->get_parent())) &&
      // exp is a variable reference expression OR
      ((isSgVarRefExp(exp) && !isSgDotExp(exp->get_parent())) ||
       // exp is a dot expression that is a valid expression (evaluated
       // recursively using isValidVarExp_rec())
       (isSgDotExp(exp) && isValidVarExp_rec(exp)));
}

bool varID::isValidVarExp(const SgInitializedName *exp) { return true; }

bool varID::isValidVarExp_rec(const SgExpression *exp) {
  ROSE_ASSERT(exp->get_parent());

  return // exp is a variable reference expression
      isSgVarRefExp(exp) ||
      // exp is a dot expression with an lhs that is a reference expression and
      // an rhs that is a valid expression
      (isSgDotExp(exp) &&
       isValidVarExp_rec(((SgDotExp *)exp)->get_lhs_operand()) &&
       isSgVarRefExp(((SgDotExp *)exp)->get_rhs_operand()));
}

/******************
 *** COMPARISON ***
 ******************/

bool varID::equal(const varID &two) const {
  return components == two.components && name == two.name &&
         expression == two.expression;
}

bool varID::lessThan(const varID &two) const {
  if (components != two.components)
    return std::lexicographical_compare(
        components.begin(), components.end(), two.components.begin(),
        two.components.end(), std::less<SgInitializedName *>{});
  if (name != two.name)
    return name < two.name;
  return std::less<const SgExpression *>{}(expression, two.expression);
}

bool varID::operator==(const varID &that) const { return equal(that); }

// two variables are equal if they're not unequal
bool varID::operator!=(const varID &that) const { return !(*this == that); }

// ORDERING OPERATORS
bool varID::operator<(const varID &that) const { return lessThan(that); }

bool varID::operator>(const varID &that) const {
  return !(*this < that) && !(*this == that);
}

bool varID::operator<=(const varID &that) const {
  return *this == that || *this < that;
}

bool varID::operator>=(const varID &that) const {
  return *this == that || *this > that;
}

bool operator==(const varID &var, SgInitializedName *iName) {
  return (var.components.size() == 1 && var.components[0] == iName);
}
bool operator!=(const varID &var, SgInitializedName *iName) {
  return !(var == iName);
}
bool operator==(SgInitializedName *iName, const varID &var) {
  return (var == iName);
}
bool operator!=(SgInitializedName *iName, const varID &var) {
  return !(var == iName);
}

bool operator==(const varID &var, SgExpression *expr) {
  return var == SgExpr2Var(expr);
}
bool operator!=(const varID &var, SgExpression *expr) { return !(var == expr); }
bool operator==(SgExpression *expr, const varID &var) { return var == expr; }
bool operator!=(SgExpression *expr, const varID &var) { return !(var == expr); }

/**************
 *** OUTPUT ***
 **************/

// string representation of the variable reference
string varID::str() const {
  ostringstream outs;

  // if this is a regular variable
  if (components.size() > 0) {
    for (vector<SgInitializedName *>::const_iterator it = components.begin();
         it != components.end();) {
      outs << (*it)->get_name().getString();
      it++;
      if (it != components.end())
        outs << ".";
    }
  } else if (expression != nullptr)
    outs << "__expression_" << static_cast<const void *>(expression);
  else
    outs << name;

  return outs.str();
}

ostream &operator<<(ostream &stream, varID v) {
  stream << v.str();
  return stream;
}

ostream &operator<<(ostream &stream, const set<varID>::iterator &v) {
  stream << v->str();
  return stream;
}

// returns true if the last field in this variable has an array or pointer type
bool varID::isArrayType() const {
  // if this is a regular variable
  if (components.size() > 0)
    return (isSgArrayType(components[components.size() - 1]->get_typeptr())) ||
           (isSgPointerType(components[components.size() - 1]->get_typeptr()));
  else
    return false;
}

// returns true if any of its constituent SgInitializedNames are
// compiler-generated
bool varID::isCompilerGenerated() const {
  for (vector<SgInitializedName *>::const_iterator it = components.begin();
       it != components.end(); it++) {
    if ((*it)->get_file_info()->isCompilerGenerated())
      return true;
  }
  return false;
}

// returns true if this variable is global and false otherwise
bool varID::isGlobal() const {
  if (components.size() == 0)
    return false;

  SgScopeStatement *nameScope = components[0]->get_scope();

  // this variable is global if...
  // components[0] refers to a variable declaration with a globally visible
  // scope if ...
  return
      // initName refers to a variable rather than a function AND
      !isSgFunctionType(components[0]->get_type()) &&
      // this SgInitializedName is part of a variable declaration rather than a
      // function argument list AND
      isSgVariableDeclaration(components[0]->get_parent()) &&
      // and it has file-scope
      (isSgGlobal(nameScope) || isSgNamespaceDefinitionStatement(nameScope));
}

// Returns the varID that corresponds to the given SgExpression
varID SgExpr2Var(const SgExpression *expr) {
  if (const SgMacroExpansionExp *macro = isSgMacroExpansionExp(expr)) {
    // A macro-expansion expression owns the source spelling of an expression
    // whose semantics are represented by its expanded child.  Dataflow
    // identities must therefore be transparent across this syntax envelope.
    return SgExpr2Var(macro->get_expanded_expression_checked());
  }

  if (varID::isValidVarExp(expr))
    return varID(expr);
  return varID::forExpression(expr);
}

// Returns true if the given expression can be interepreted as a concrete
// variable
bool isVarExpr(SgExpression *expr) { return varID::isValidVarExp(expr); }

varID zeroVar("ZERO");
varID oneVar("ONE");
varID allVar("ALL");

// Add the zero variable to the set.
void addPredefinedVars(varIDSet &vars) { vars.insert(zeroVar); }

class varIDCollector : public AstBottomUpProcessing<varIDSet> {
  set<rwAccessLabeler::accessType> access;

public:
  varIDCollector() {
    // by default collect all access types
    access.insert(rwAccessLabeler::readAccess);
    access.insert(rwAccessLabeler::writeAccess);
    access.insert(rwAccessLabeler::rwAccess);
  }

  varIDCollector(rwAccessLabeler::accessType access) {
    this->access.insert(access);
  }

  varIDCollector(set<rwAccessLabeler::accessType> &access) {
    this->access = access;
  }

  varIDSet evaluateSynthesizedAttribute(SgNode *n,
                                        SynthesizedAttributesList children) {
    // list of variables inside this subtree
    varIDSet res;

    SgExpression *expr;
    if ((expr = isSgExpression(n)) && varID::isValidVarExp(expr)) {
      // if we're capturing accesses of the type being performed at node n
      if (access.find(rwAccessLabeler::getAccessType(n)) != access.end()) {
        // add this variable to the list
        varID newVar(expr);
        res.insert(newVar);
      }
    }
    // if n is not a variable node, res = union of children lists
    else {
      for (SynthesizedAttributesList::iterator it = children.begin();
           it != children.end(); it++) {
        for (varIDSet::iterator it2 = (*it).begin(); it2 != (*it).end();
             it2++) {
          res.insert(*it2);
        }
      }
    }

    return res;
  }
};

// returns a set of varIDs that correspond to all the variables (of
// SgDotExp/SgVarRefExp form) that were referenced in the given subtree
varIDSet getVarRefsInSubtree(SgNode *root) {
  rwAccessLabeler::addRWAnnotations(root);
  varIDCollector c;
  return c.traverse(root);
}

// returns a set of varIDs that correspond to all the variables (of
// SgDotExp/SgVarRefExp form) that were read from in the given subtree
varIDSet getReadVarRefsInSubtree(SgNode *root) {
  rwAccessLabeler::addRWAnnotations(root);
  set<rwAccessLabeler::accessType> access;
  access.insert(rwAccessLabeler::readAccess);
  access.insert(rwAccessLabeler::rwAccess);
  varIDCollector c(access);
  return c.traverse(root);
}

// returns a set of varIDs that correspond to all the variables (of
// SgDotExp/SgVarRefExp form) that were written to in the given subtree
varIDSet getWriteVarRefsInSubtree(SgNode *root) {
  rwAccessLabeler::addRWAnnotations(root);
  set<rwAccessLabeler::accessType> access;
  access.insert(rwAccessLabeler::writeAccess);
  access.insert(rwAccessLabeler::rwAccess);
  varIDCollector c(access);
  return c.traverse(root);
}

#endif
