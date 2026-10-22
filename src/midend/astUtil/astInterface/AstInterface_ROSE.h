#ifndef AST_INTERFACE_IMPL_H
#define AST_INTERFACE_IMPL_H

#include "AstInterface.h"

#include "ObserveObject.h"

#include <list>

#include <string>

class SgNode;
class SgType;
class SgVarRefExp;
class SgExpression;
class SgDotExp;

typedef AstNodePtr AstNodePtrImpl;

class AstNodeTypeImpl : public AstNodeType {
public:
  AstNodeTypeImpl(SgType *n = 0) : AstNodeType(n) {}
  AstNodeTypeImpl(const AstNodeType &that) : AstNodeType(that) {}
  AstNodeType &operator=(const AstNodeType &that) {
    AstNodeType::operator=(that);
    return *this;
  }
  SgType *get_ptr() const {
    return static_cast<SgType *>(AstNodeType::get_ptr());
  }
  SgType *operator->() const {
    return static_cast<SgType *>(AstNodeType::get_ptr());
  }
};

class SgScopeStatement;
class SgBasicBlock;
class SgGlobal;
class SgFunctionSymbol;
class SgVariableSymbol;
class SgStatement;
class SgMemberFunctionSymbol;
class SgClassDeclaration;
class SgVariableDeclaration;
class SgInitializedName;
class SgFunctionDeclaration;
class AstInterfaceImpl : public ObserveObject<AstObserver> {
public:
  AstInterfaceImpl(SgNode *_top) : newVarIndex(0) { set_top(_top); }

  typedef AstInterface::AstNodeList AstNodeList;
  typedef AstInterface::AstTypeList AstTypeList;

  SgScopeStatement *get_scope(SgNode *l) {
    if (scope == 0)
      set_top(l);
    return scope;
  }
  void set_top(SgNode *_top);

  static void GetTypeInfo(SgType *t, std::string *name = 0,
                          std::string *stripname = 0,
                          bool use_global_unique_name = false);
  static SgType *GetTypeInt();

  SgVarRefExp *CreateFieldRef(SgNode *decl, std::string fieldname);

  SgFunctionSymbol *LookupFunction(const char *start, SgScopeStatement *loc);

  SgFunctionSymbol *GetFunc(const std::string &name);
  SgFunctionSymbol *NewFunc(const std::string &name, SgType *rtype,
                            const std::list<SgInitializedName *> &args);

  SgNode *CreateFunction(std::string name, int numOfPars);
  SgNode *CreateFunctionCall(SgNode *func, AstNodeList::const_iterator,
                             AstNodeList::const_iterator e);
  static bool IsFunctionCall(SgNode *s, SgNode **func, AstNodeList *args);

  /*QY: if yes, set ivar,lb,ub,step, and body accordingly */
  static bool IsFortranLoop(const SgNode *s, SgNode **ivar = 0, SgNode **lb = 0,
                            SgNode **ub = 0, SgNode **step = 0,
                            SgNode **body = 0);

  /*QY: if yes, set lhs, rhs and readlhs accordingly */
  static bool IsAssignment(const SgNode *s, SgNode **lhs = 0, SgNode **rhs = 0,
                           bool *readlhs = 0);

  /*QY: if yes, set vartype, varname,scope, and isglobal accordingly*/
  static bool IsVarRef(SgNode *exp, SgType **vartype = 0,
                       std::string *varname = 0, SgNode **scope = 0,
                       bool *isglobal = 0, bool use_global_unique_name = false,
                       bool *has_ptr_deref = 0);

  SgMemberFunctionSymbol *
  NewMemberFunc(SgClassDeclaration *decl, const std::string &name,
                SgType *rtype, const std::list<SgInitializedName *> &args);
  /* Create a new member function if not found */
  SgMemberFunctionSymbol *GetMemberFunc(SgClassDeclaration *c,
                                        const std::string &funcname,
                                        std::vector<SgExpression *> *args = 0);

  SgVariableSymbol *NewVar(SgType *type, const std::string &_name,
                           bool makeunique, bool delayDecl,
                           SgExpression *initexp = 0,
                           SgScopeStatement *loc = 0);
  void CopyNewVarDecls(SgBasicBlock *blk);

  static SgNode *LookupNestedDeclaration(const std::string &name, SgNode *loc);
  SgVariableSymbol *LookupVar(const std::string &name, SgNode *loc);
  SgVariableSymbol *LookupVar(const std::string &name);

  SgExpression *CreateVarRef(std::string varname, SgNode *loc = 0);

  SgDotExp *CreateVarMemberRef(std::string varname, std::string field,
                               SgNode *loc = 0);

  bool ReplaceAst(SgNode *orig, SgNode *n);
  static SgStatement *GetScope(SgNode *loc);

private:
  SgGlobal *global;
  SgScopeStatement *scope;
  int newVarIndex;
  struct PendingVariable {
    SgScopeStatement *scope;
    SgVariableDeclaration *declaration;
    SgVariableSymbol *symbol;
  };
  std::vector<PendingVariable> newVarList;
  friend class AstInterface;
};

#endif
