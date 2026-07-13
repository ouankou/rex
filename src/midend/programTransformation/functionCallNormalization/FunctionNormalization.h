#ifndef FUNCTION_CALL_NORMALIZATION_H
#define FUNCTION_CALL_NORMALIZATION_H

#include "AstSimpleProcessing.h"

#include <iostream>

#include <list>

#include <map>

#include <sstream>

#include <string>

#include <utility>
// #include "rose.h"

struct Declaration {
  SgStatement *initVarDeclaration, *nonInitVarDeclaration, *assignment;
  SgVariableSymbol *symbol;
  SgName name;
};

typedef std::list<struct Declaration *> DeclarationPtrList;

class FunctionCallNormalization : public SgSimpleProcessing {
public:
  // normalizes function calls of statements within a basic block
  void visit(SgNode *astNode);

private:
  struct FunctionCallReplacement {
    SgVariableSymbol *symbol;
    SgType *expressionType;
    bool dereference;
  };
  typedef std::map<SgFunctionCallExp *, FunctionCallReplacement>
      FunctionCallReplacementMap;

  void replaceFunctionCallsInExpression(SgNode *,
                                        const FunctionCallReplacementMap &);

  // BFS query on an AST
  std::list<SgNode *> BFSQueryForNodes(SgNode *root, VariantT type);

  // function evaluation order query on an AST (first eval args, then function;
  // for other nodes, it's postorder)
  std::list<SgNode *> FEOQueryForNodes(SgNode *root, VariantT type);

  std::list<SgNode *> createTraversalList(SgNode *root);
};

#endif
