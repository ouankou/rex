#include "featureTests.h"
#ifdef ROSE_ENABLE_SOURCE_ANALYSIS

#ifndef VARIABLES_H
#define VARIABLES_H

#include "genericDataflowCommon.h"

#include <iosfwd>

#include <set>

#include <string>

#include <utility>

#include <vector>

class SgDotExp;
class SgScopeStatement;
class SgType;
class varID;

/* #########################
   ###### SHARED DATA ######
   ######################### */

extern varID zeroVar;
extern varID oneVar;
extern varID allVar;

/* ##############################
   ###### TYPE DEFINITIONS ######
   ############################## */

// type of variable ids, their sets and maps of sets
class varID {
public:
  std::vector<SgInitializedName *> components;
  SgType *varType = nullptr;
  const SgExpression *expression = nullptr;
  // special variables do not have associated SgInitializedName nodes and thus,
  // we just give them a name
  std::string name;

  varID() {}

  varID(std::string name) : name(std::move(name)) {}

  // creates a varID from an SgNode s.t. isValidVarExp(n) is true
  varID(SgNode *n) {
    bool ret = init(n);
    ROSE_ASSERT(ret);
  }

  varID(SgInitializedName *name) {
    bool ret = init(name);
    ROSE_ASSERT(ret);
  }

  // pre-condition: isValidVarExp(refExp) evaluates to true
  varID(const SgExpression *exp) {
    bool ret = init(exp);
    ROSE_ASSERT(ret);
  }

  varID(const varID &that) { init(that); }

  static varID forExpression(const SgExpression *exp);

  // initializes this object from the given varID
  bool init(const varID &that);

  // initializes this object from the given expression (assumed that
  // isValidVarExp(n) is true) returns true on success, false on failure
  bool init(SgNode *n);

  // initializes this object from the given expression (assumed that
  // isValidVarExp(exp) is true) returns true on success, false on failure
  bool init(const SgExpression *exp);

  // initializes this object from the given SgInitializedName (assumed that
  // isValidVarExp(name) is true)
  bool init(SgInitializedName *name);

  // returns the scope in which this variable was declared
  // for compound variables (i.e. those composed of dot expressions), it is the
  // scope of the leftmost name
  SgScopeStatement *getScope() const;

  // returns a SgExpression that corresponds to an access of this variable
  SgExpression *toSgExpression() const;

protected:
  void collectDotComponents(const SgDotExp *dotExp);

  // returns a SgExpression that corresponds to an access of this variable,
  // including only the components at or after the iterator rest into the
  // components vector
  SgExpression *toSgExpression_rec(
      std::vector<SgInitializedName *>::const_iterator rest) const;

public:
  // returns true if the given expression is one that can be represented as a
  // variable in our representation
  static bool isValidVarExp(const SgNode *exp);
  static bool isValidVarExp(const SgExpression *exp);
  static bool isValidVarExp(const SgInitializedName *exp);

protected:
  static bool isValidVarExp_rec(const SgExpression *exp);

  /******************
   *** COMPARISON ***
   ******************/

public:
  bool equal(const varID &two) const;
  bool lessThan(const varID &two) const;

  bool operator==(const varID &that) const;
  bool operator!=(const varID &that) const;
  bool operator<(const varID &that) const;
  bool operator>(const varID &that) const;
  bool operator<=(const varID &that) const;
  bool operator>=(const varID &that) const;

  /**************
   *** OUTPUT ***
   **************/

  // string representation of the variable reference.
  std::string str() const;

  /**********************
   *** SEMANTIC INFO ***
   **********************/

  // returns true if the last field in this variable has an array or pointer
  // type
  bool isArrayType() const;

  // returns true if any of its constituent SgInitializedNames are
  // compiler-generated
  bool isCompilerGenerated() const;

  // returns true if this variable is global and false otherwise
  bool isGlobal() const;
};

std::ostream &operator<<(std::ostream &stream, varID v);
std::ostream &operator<<(std::ostream &stream,
                         const std::set<varID>::iterator &v);
bool operator==(const varID &var, SgInitializedName *iName);
bool operator!=(const varID &var, SgInitializedName *iName);
bool operator==(SgInitializedName *iName, const varID &var);
bool operator!=(SgInitializedName *iName, const varID &var);

bool operator==(const varID &var, SgExpression *expr);
bool operator!=(const varID &var, SgExpression *expr);
bool operator==(SgExpression *expr, const varID &var);
bool operator!=(SgExpression *expr, const varID &var);

typedef std::set<varID, std::less<varID>> varIDSet;

/* #################################
   ###### FUNCTION PROTOTYPES ######
   ################################# */

// Returns the varID that corresponds to the given SgExpression
varID SgExpr2Var(const SgExpression *expr);

// Returns true if the given expression can be interepreted as a concrete
// variable
bool isVarExpr(SgExpression *expr);

// add the special variable Zero to the given set of variables
void addPredefinedVars(varIDSet &vars);

// returns a set of varIDs that correspond to all the variables (of
// SgDotExp/SgVarRefExp form) that were referenced in the given subtree
varIDSet getVarRefsInSubtree(SgNode *root);

// returns a set of varIDs that correspond to all the variables (of
// SgDotExp/SgVarRefExp form) that were read from in the given subtree
varIDSet getReadVarRefsInSubtree(SgNode *root);

// returns a set of varIDs that correspond to all the variables (of
// SgDotExp/SgVarRefExp form) that were written to in the given subtree
varIDSet getWriteVarRefsInSubtree(SgNode *root);

#endif
#endif
