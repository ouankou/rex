#ifndef _CREATESLICE_H_
#define _CREATESLICE_H_

// Retain the requested AST nodes and remove other statements from the slice.

#include "AstProcessing.h"

#include "rosedll.h"

#include <list>

#include <set>

#include <stack>

#include <vector>

class BooleanSafeKeeper {
public:
  BooleanSafeKeeper() : boolean(false) {}
  BooleanSafeKeeper(bool boolVal) : boolean(boolVal) {}
  bool boolean;
};

class ROSE_DLL_API CreateSlice
    : public AstTopDownBottomUpProcessing<BooleanSafeKeeper,
                                          BooleanSafeKeeper> {

public:
  CreateSlice(std::set<SgNode *> saveNodes) : _toSave(saveNodes) {}

  bool traverse(SgNode *node) {
    currentFile = NULL;
    statementsPendingRemoval.clear();
    // No explicit targets means there is no slice to materialize.
    // Keep the AST unchanged instead of attempting destructive pruning.
    if (_toSave.empty()) {
      return false;
    }
    bool traversalResult =
        AstTopDownBottomUpProcessing<BooleanSafeKeeper, BooleanSafeKeeper>::
            traverse(node, BooleanSafeKeeper(false))
                .boolean;
    applyPendingRemovals();
    return traversalResult;
  }

protected:
  SgFile *currentFile;

  virtual BooleanSafeKeeper
  evaluateInheritedAttribute(SgNode *node, BooleanSafeKeeper partOfSlice);

  virtual BooleanSafeKeeper
  evaluateSynthesizedAttribute(SgNode *node, BooleanSafeKeeper inherited,
                               SubTreeSynthesizedAttributes atts);
  BooleanSafeKeeper defaultSynthesizedAttribute(BooleanSafeKeeper inh) {
    return inh;
  }
  std::stack<std::list<SgNode *>> delayedRemoveListStack;
  void queueStatementForRemoval(SgStatement *statement);
  void applyPendingRemovals();
  std::vector<SgStatement *> statementsPendingRemoval;

private:
  std::set<SgNode *> _toSave;
};

#endif
