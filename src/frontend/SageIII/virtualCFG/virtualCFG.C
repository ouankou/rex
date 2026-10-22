#include "sage3basic.h"

#include <err.h>

using namespace std;

namespace VirtualCFG {

unsigned int cfgIndexForEndWrapper(SgNode *n) { return n->cfgIndexForEnd(); }

static inline string variableName(SgInitializedName *in) {
  string s = in->get_name().str();
  if (s.empty())
    s = "<anon>";
  return s;
}

CFGNode::CFGNode(SgNode *node, unsigned int index) : node(node), index(index) {
  ROSE_ASSERT(!node || isSgStatement(node) || isSgExpression(node) ||
              isSgInitializedName(node));

  if (node && (index > node->cfgIndexForEnd())) {
    warnx("created CFGNode with illegal index:");
    cout << "index = " << index
         << " while max index is:" << node->cfgIndexForEnd() << endl;
    printf("Error: node = %p = %s \n", node, node->class_name().c_str());
    SageInterface::dumpInfo(node);
    ROSE_ABORT();
  }
}

string CFGNode::toString() const {
  ostringstream s;
  if (auto *definition = isSgFunctionDefinition(node)) {
    switch (index) {
    case 0:
      s << "Start(";
      break;
    case 1:
      s << "After parameters(";
      break;
    case 2:
      s << "After pre-initialization(";
      break;
    case 3:
      if (definition->get_destructor_epilogue_dependent())
        s << "Dependent cleanup(";
      else if (definition->get_destructor_epilogue() != nullptr)
        s << "After body(";
      else
        s << "End(";
      break;
    case 4:
      ROSE_ASSERT(definition->cfgIndexForEnd() == index);
      s << "End(";
      break;
    default: {
      ROSE_ASSERT(!"Bad index");
      return "error";
    }
    }
    s << definition->get_declaration()->get_qualified_name().str() << ")"
      << std::endl;
  } else {
    s << SageInterface::get_name(node) << endl;
  }
  s << toStringForDebugging();
  return s.str();
}

string CFGNode::toStringForDebugging() const {
  ostringstream s;
  if (node == NULL) {
    s << "End of procedure";
  } else {
    string nodeText;
    if (isSgInitializedName(node)) {
      nodeText = variableName(isSgInitializedName(node));
    } else {
      // nodeText = node->unparseToString(); -- Fortran CFG nodes can't always
      // be unparsed
      ostringstream nt;
      nt << "@line=" << node->get_startOfConstruct()->get_line()
         << ", col=" << node->get_startOfConstruct()->get_col();
      nodeText += nt.str();
    }
    if (nodeText.length() > 20) {
      nodeText.resize(20);
      nodeText += "...";
    }
    s << node << "<" << node->class_name() << "> " << nodeText
      << " :idx=" << index;
    // s << node->class_name() << " @" << hex << uintptr_t(node) << " " << dec
    // << index;
  }
  return s.str();
}

string CFGNode::id() const {
  ostringstream s;
  s << "n_" << hex << uintptr_t(node) << "_" << dec << index;
  return s.str();
}

string CFGEdge::toString() const { return toStringForDebugging(); }

string CFGEdge::toStringForDebugging() const {
  ostringstream s;
  EdgeConditionKind cond = condition();

  if (cond != eckUnconditional) {
    s << "key(";
    switch (cond) {
    case eckTrue:
      s << "true";
      break;
    case eckFalse:
      s << "false";
      break;
    case eckCaseLabel: {
      SgExpression *cseLbl = caseLabel();
      ROSE_ASSERT(cseLbl != NULL);
      s << cseLbl->unparseToString();
    } break;
    case eckDefault:
      s << "default";
      break;
    case eckDoConditionPassed:
      s << "do passed";
      break;
    case eckDoConditionFailed:
      s << "do failed";
      break;
    case eckForallIndicesInRange:
      s << "indices in range";
      break;
    case eckForallIndicesNotInRange:
      s << "indices not in range";
      break;
    case eckComputedGotoCaseLabel:
      s << computedGotoCaseIndex();
      break;
    case eckArithmeticIfLess:
      s << "less";
      break;
    case eckArithmeticIfEqual:
      s << "equal";
      break;
    case eckArithmeticIfGreater:
      s << "greater";
      break;
    case eckInterprocedural:
      s << "interprocedural";
      break;
    case eckCompleteObjectConstruction:
      s << "complete-object construction";
      break;
    case eckBaseObjectConstruction:
      s << "base-object construction";
      break;
    case eckCompleteObjectDestruction:
      s << "complete-object destruction";
      break;
    case eckBaseObjectDestruction:
      s << "base-object destruction";
      break;

    case eckError:
      s << "error (debugging)";
      break;

    default:
      s << "unknown";
      break;
    }
    s << ")";
  }
  return s.str();
}

string CFGEdge::id() const {
  ostringstream s;
  s << src.id() << "__" << tgt.id();
  return s.str();
}

string CFGPath::toString() const {
  ostringstream s;
  bool anyNonEmpty = false;
  for (unsigned int i = 0; i < edges.size(); ++i) {
    string str = edges[i].toString();
    if (anyNonEmpty && !str.empty())
      s << "; ";
    if (!str.empty())
      anyNonEmpty = true;
    s << str;
  }
  return s.str();
}

string CFGPath::toStringForDebugging() const {
  ostringstream s;
  bool anyNonEmpty = false;
  for (unsigned int i = 0; i < edges.size(); ++i) {
    string str = edges[i].toStringForDebugging();
    if (anyNonEmpty && !str.empty())
      s << "; ";
    if (!str.empty())
      anyNonEmpty = true;
    s << str;
  }
  return s.str();
}

string CFGPath::id() const {
  ostringstream s;
  ROSE_ASSERT(!edges.empty());
  for (unsigned int i = 0; i < edges.size(); ++i) {
    s << edges[i].source().id() << "___";
  }
  s << edges.back().target().id();
  return s.str();
}

static vector<SgInitializedName *>
findVariablesDirectlyInScope(SgStatement *sc) {
  // This can't be done with a traversal because it needs to avoid entering
  // inner scope statements
  vector<SgInitializedName *> variablesInScope;
  vector<SgNode *> succs = sc->get_traversalSuccessorContainer();
  for (size_t i = succs.size(); i > 0; --i) {
    SgNode *child = succs[i - 1];
    if (isSgInitializedName(child)) {
      variablesInScope.push_back(isSgInitializedName(child));
    } else if (isSgVariableDeclaration(child)) {
      SgVariableDeclaration *vd = isSgVariableDeclaration(child);
      ROSE_ASSERT(vd != NULL);
      const SgInitializedNamePtrList &vars = vd->get_variables();
      variablesInScope.insert(variablesInScope.end(), vars.rbegin(),
                              vars.rend());
    } else if (isSgForInitStatement(child) ||
               isSgDeclarationGroupStatement(child)) {
      SgStatement *declarations = isSgStatement(child);
      ROSE_ASSERT(declarations != nullptr);
      vector<SgInitializedName *> initVars =
          findVariablesDirectlyInScope(declarations);
      variablesInScope.insert(variablesInScope.end(), initVars.begin(),
                              initVars.end());
    }
  }
  return variablesInScope;
}

bool CFGNode::isInteresting() const {
  ROSE_ASSERT(node);
  return node->cfgIsIndexInteresting(index);
}

static SgNode *leastCommonAncestor(SgNode *a, SgNode *b) {
  // Find the closest node which is an ancestor of both a and b
  vector<SgNode *> ancestorsOfA;
  for (SgNode *p = a; p; p = p->get_parent())
    ancestorsOfA.push_back(p);
  while (b) {
    vector<SgNode *>::const_iterator i =
        std::find(ancestorsOfA.begin(), ancestorsOfA.end(), b);
    if (i != ancestorsOfA.end())
      return *i;
    b = b->get_parent();
  }
  return NULL;
}

EdgeConditionKind CFGEdge::condition() const {
  SgNode *srcNode = src.getNode();
  unsigned int srcIndex = src.getIndex();
  SgNode *tgtNode = tgt.getNode();
  unsigned int tgtIndex = tgt.getIndex();

  if (auto *choice = isSgChooseExpression(srcNode);
      choice != nullptr && srcIndex == 0) {
    choice->validate();
    ROSE_ASSERT(tgtNode == choice->get_true_expression() ||
                tgtNode == choice->get_false_expression());
    const bool true_arm = tgtNode == choice->get_true_expression();
    if (choice->get_selection_kind() !=
        SgChooseExpression::e_choose_selection_dependent) {
      ROSE_ASSERT(true_arm == (choice->get_selection_kind() ==
                               SgChooseExpression::e_choose_selection_true));
      return eckUnconditional;
    }
    return true_arm ? eckTrue : eckFalse;
  }
  if (auto *sequence = isSgCatchStatementSeq(srcNode);
      sequence != nullptr &&
      srcIndex < sequence->get_catch_statement_seq().size()) {
    auto *handler =
        isSgCatchOptionStmt(sequence->get_catch_statement_seq()[srcIndex]);
    ASSERT_not_null(handler);
    if (tgt == handler->cfgForBeginning())
      return eckTrue;
    ROSE_ASSERT(handler->get_condition() != nullptr && tgtNode == srcNode &&
                tgtIndex == srcIndex + 1);
    return eckFalse;
  }
  if (auto *initializers = isSgCtorInitializerList(srcNode);
      initializers != nullptr && srcIndex < initializers->get_ctors().size()) {
    auto *entry = initializers->get_ctors()[srcIndex];
    ROSE_ASSERT(entry != nullptr && entry->get_parent() == initializers);
    if (entry->get_preinitialization() ==
        SgInitializedName::e_virtual_base_class) {
      if (tgt == entry->cfgForBeginning())
        return eckCompleteObjectConstruction;
      ROSE_ASSERT(tgtNode == srcNode && tgtIndex == srcIndex + 1);
      return eckBaseObjectConstruction;
    }
  }
  if (auto *destruction = isSgCxxSubobjectDestruction(srcNode);
      destruction != nullptr && srcIndex == 0 &&
      destruction->get_target_kind() ==
          SgCxxSubobjectDestruction::e_virtual_base) {
    ROSE_ASSERT(tgtNode == srcNode && (tgtIndex == 1 || tgtIndex == 2));
    return tgtIndex == 1 ? eckCompleteObjectDestruction
                         : eckBaseObjectDestruction;
  }
  if (isSgIfStmt(srcNode) && srcIndex == 1) {
    SgIfStmt *ifs = isSgIfStmt(srcNode);
    if (ifs->get_is_if_consteval_statement())
      return eckUnconditional;
    if (ifs->get_true_body() == tgtNode) {
      return eckTrue;
    } else if (tgtNode != NULL && ifs->get_false_body() == tgtNode) {
      return eckFalse;
    } else if (ifs->get_false_body() == NULL && tgtNode == ifs &&
               tgtIndex == 2) {
      return eckFalse;
    } else
      ROSE_ASSERT(!"Bad successor in if statement");

  } else if (isSgArithmeticIfStatement(srcNode) && srcIndex == 1) {
    SgArithmeticIfStatement *aif = isSgArithmeticIfStatement(srcNode);
    if (getCFGTargetOfFortranLabelRef(aif->get_less_label()) == tgt) {
      return eckArithmeticIfLess;
    } else if (getCFGTargetOfFortranLabelRef(aif->get_equal_label()) == tgt) {
      return eckArithmeticIfEqual;
    } else if (getCFGTargetOfFortranLabelRef(aif->get_greater_label()) == tgt) {
      return eckArithmeticIfGreater;
    } else {
      ROSE_ASSERT(srcNode != NULL);
      ROSE_ASSERT(tgtNode != NULL);
      // DQ (1/19/2018): This is an error for Fortran (test2007_151.f) when the
      // CFG graph is outputing the inEdges (must be turned on explicit in the
      // souce code: filteredCFGImpl.h.
      ROSE_ASSERT(!"Bad successor in arithmetic if statement");
    }
  } else if (isSgCxxArrayInitLoop(srcNode) &&
             (srcIndex == 1 || srcIndex == 3)) {
    ROSE_ASSERT(srcNode == tgtNode && (tgtIndex == 2 || tgtIndex == 4));
    return tgtIndex == 2 ? eckTrue : eckFalse;
  } else if (isSgWhileStmt(srcNode) && srcIndex == 1) {
    if (srcNode == tgtNode) {
      // False case for while test
      return eckFalse;
    } else {
      return eckTrue;
    }
  } else if (isSgDoWhileStmt(srcNode) && srcIndex == 2) {
    // tgtIndex values are 0 for true branch and 3 for false branch
    if (tgtIndex == 0) {
      return eckTrue;
    } else {
      return eckFalse;
    }
  } else if (isSgForStatement(srcNode) && srcIndex == 2) {
    if (srcNode == tgtNode) {
      // False case for test
      return eckFalse;
    } else {
      return eckTrue;
    }
  } else if (isSgFortranDo(srcNode) && srcIndex == 3) {
    if (tgtIndex == 4) {
      return eckDoConditionPassed;
    } else if (tgtIndex == 6) {
      return eckDoConditionFailed;
    } else
      ROSE_ASSERT(!"Bad successor in do loop");
  } else if (isSgForAllStatement(srcNode) && srcIndex == 1) {
    if (tgtIndex == 2) {
      return eckForallIndicesInRange;
    } else if (tgtIndex == 7) {
      return eckForallIndicesNotInRange;
    } else
      ROSE_ASSERT(!"Bad successor in forall loop");
  } else if (isSgForAllStatement(srcNode) && srcIndex == 3) {
    if (SageInterface::forallMaskExpression(isSgForAllStatement(srcNode))) {
      if (tgtIndex == 4) {
        return eckTrue;
      } else if (tgtIndex == 6) {
        return eckFalse;
      } else
        ROSE_ASSERT(!"Bad successor in forall loop");
    } else {
      return eckUnconditional;
    }
  } else if (isSgSwitchStatement(srcNode) && isSgCaseOptionStmt(tgtNode)) {
    return eckCaseLabel;
  } else if (isSgSwitchStatement(srcNode) && isSgDefaultOptionStmt(tgtNode)) {
    return eckDefault;
  } else if (isSgComputedGotoStatement(srcNode) && srcIndex == 1) {
    if (tgtNode == srcNode) {
      return eckDefault;
    } else {
      return eckComputedGotoCaseLabel;
    }
  } else if (isSgConditionalExp(srcNode) && srcIndex == 1) {
    SgConditionalExp *ce = isSgConditionalExp(srcNode);
    ce->validate();
    if ((ce->get_operator_kind() ==
             SgConditionalExp::e_conditional_operator_standard &&
         ce->get_true_exp() == tgtNode) ||
        (ce->get_operator_kind() ==
             SgConditionalExp::e_conditional_operator_gnu_binary &&
         tgtNode == ce && tgtIndex == 2)) {
      return eckTrue;
    } else if (ce->get_false_exp() == tgtNode) {
      return eckFalse;
    } else
      ROSE_ASSERT(!"Bad successor in conditional expression");
  } else if (isSgAndOp(srcNode) && srcIndex == 1) {
    if (srcNode == tgtNode) {
      // Short-circuited false case
      return eckFalse;
    } else {
      return eckTrue;
    }
  } else if (isSgOrOp(srcNode) && srcIndex == 1) {
    if (srcNode == tgtNode) {
      // Short-circuited true case
      return eckTrue;
    } else {
      return eckFalse;
    }
  } else if ((isSgCxxSubobjectDestruction(srcNode) && srcIndex == 1 &&
              isSgFunctionDefinition(tgtNode)) ||
             (isSgCxxSubobjectDestruction(tgtNode) && tgtIndex == 2 &&
              isSgFunctionDefinition(srcNode))) {
    return eckInterprocedural;
  } else if (isSgFunctionCallExp(srcNode) &&
             srcIndex == srcNode->cfgIndexForEnd() - 1 &&
             !isSgFunctionCallExp(tgtNode)) {
    return eckInterprocedural;
  } else if (isSgFunctionCallExp(tgtNode) &&
             tgtIndex == tgtNode->cfgIndexForEnd() &&
             !isSgFunctionCallExp(srcNode)) {
    return eckInterprocedural;
  } else if (isSgConstructorInitializer(srcNode) &&
             srcIndex == SGCONSTRUCTORINITIALIZER_INTERPROCEDURAL_INDEX &&
             !isSgConstructorInitializer(tgtNode)) {
    return eckInterprocedural;
  } else if (isSgConstructorInitializer(tgtNode) &&
             tgtIndex == SGCONSTRUCTORINITIALIZER_INTERPROCEDURAL_INDEX + 1 &&
             !isSgConstructorInitializer(srcNode)) {
    return eckInterprocedural;
  } else {
    // No key
    return eckUnconditional;
  }

  // DQ (11/29/2009): It should be an error to reach this point in the function.
  ROSE_ABORT();
}

SgExpression *CFGEdge::caseLabel() const {
  SgNode *srcNode = src.getNode();
  SgNode *tgtNode = tgt.getNode();
  if (isSgSwitchStatement(srcNode) && isSgCaseOptionStmt(tgtNode)) {
    return isSgCaseOptionStmt(tgtNode)->get_key();
  } else {
    return NULL;
  }
}

unsigned int CFGEdge::computedGotoCaseIndex() const {
  SgNode *srcNode = src.getNode();
  SgNode *tgtNode = tgt.getNode();
  if (isSgComputedGotoStatement(srcNode) && srcNode != tgtNode) {
    const Rose_STL_Container<SgExpression *> &labels =
        isSgComputedGotoStatement(srcNode)->get_labelList()->get_expressions();
    for (Rose_STL_Container<SgExpression *>::const_iterator j = labels.begin();
         j != labels.end(); ++j) {
      SgLabelRefExp *lRef = isSgLabelRefExp(*j);
      if (getCFGTargetOfFortranLabelRef(lRef) == tgt) {
        return j - labels.begin() + 1;
      }
    }
    return 0;
  } else {
    return 0;
  }
}

static SgExpression *getExpressionForTest(SgStatement *t) {
  auto *statement = isSgExprStatement(t);
  ROSE_ASSERT(statement != nullptr && statement->get_expression() != nullptr);
  return statement->get_expression();
}

SgExpression *CFGEdge::conditionBasedOn() const {
  SgNode *srcNode = src.getNode();
  unsigned int srcIndex = src.getIndex();
  if (auto *choice = isSgChooseExpression(srcNode);
      choice != nullptr && srcIndex == 0) {
    (void)condition();
    return choice->get_selection_kind() ==
                   SgChooseExpression::e_choose_selection_dependent
               ? choice->get_condition()
               : nullptr;
  }
  if (isSgIfStmt(srcNode) && srcIndex == 1) {
    SgIfStmt *ifs = isSgIfStmt(srcNode);
    if (ifs->get_is_if_consteval_statement())
      return nullptr;
    return SageInterface::getConditionValue(ifs);
  } else if (isSgArithmeticIfStatement(srcNode) && srcIndex == 1) {
    return isSgArithmeticIfStatement(srcNode)->get_conditional();
  } else if (isSgWhileStmt(srcNode) && srcIndex == 1) {
    return SageInterface::getConditionValue(isSgWhileStmt(srcNode));
  } else if (isSgDoWhileStmt(srcNode) && srcIndex == 2) {
    return getExpressionForTest(isSgDoWhileStmt(srcNode)->get_condition());
  } else if (isSgForStatement(srcNode) && srcIndex == 2) {
    return SageInterface::getConditionValue(isSgForStatement(srcNode));
  } else if (isSgForAllStatement(srcNode) && srcIndex == 1) {
    return isSgForAllStatement(srcNode)->get_forall_header();
  } else if (isSgForAllStatement(srcNode) && srcIndex == 3) {
    return SageInterface::forallMaskExpression(isSgForAllStatement(srcNode));
  } else if (isSgFortranDo(srcNode) && srcIndex == 3) {
    return isSgFortranDo(srcNode)->get_bound();
  } else if (isSgSwitchStatement(srcNode) && srcIndex == 1) {
    return SageInterface::getConditionValue(isSgSwitchStatement(srcNode));
  } else if (isSgComputedGotoStatement(srcNode) && srcIndex == 1) {
    return isSgComputedGotoStatement(srcNode)->get_label_index();
  } else if (isSgConditionalExp(srcNode) && srcIndex == 1) {
    SgConditionalExp *ce = isSgConditionalExp(srcNode);
    ce->validate();
    return ce->get_conditional_exp();
  } else if (isSgAndOp(srcNode) && srcIndex == 1) {
    return isSgAndOp(srcNode)->get_lhs_operand();
  } else if (isSgOrOp(srcNode) && srcIndex == 1) {
    return isSgOrOp(srcNode)->get_lhs_operand();
  } else {
    return NULL;
  }
}

template <typename DoEffectsForwards, typename DoEffectsBackwards>
vector<SgInitializedName *>
genericWindUnwind(SgNode *src, SgNode *tgt, DoEffectsForwards doEffectsForwards,
                  DoEffectsBackwards doEffectsBackwards) {
  ROSE_ASSERT(src != nullptr);
  ROSE_ASSERT(tgt != nullptr);
  vector<SgInitializedName *> data;
  vector<SgNode *> ancestorsOfTo, ancestorsOfFrom;
  SgNode *lca = leastCommonAncestor(src, tgt);
  ROSE_ASSERT(lca != nullptr);
  for (SgNode *n = tgt; n != lca; n = n->get_parent())
    ancestorsOfTo.push_back(n);
  ancestorsOfTo.push_back(lca);
  reverse(ancestorsOfTo.begin(),
          ancestorsOfTo.end()); // So outermost scopes are at the beginning
  for (SgNode *n = src; n != lca; n = n->get_parent())
    ancestorsOfFrom.push_back(n);
  ancestorsOfFrom.push_back(lca);
  reverse(ancestorsOfFrom.begin(),
          ancestorsOfFrom.end()); // So outermost scopes are at the beginning

  // Find positions of ancestors of "from" and "to" in children of "lca"
  size_t positionOfChild1, positionOfChild2;
  {
    vector<SgNode *> children = lca->get_traversalSuccessorContainer();
    positionOfChild1 = src == lca ? 0
                                  : find(children.begin(), children.end(),
                                         ancestorsOfFrom[1]) -
                                        children.begin();
    positionOfChild2 =
        tgt == lca ? 0
                   : find(children.begin(), children.end(), ancestorsOfTo[1]) -
                         children.begin();
    ROSE_ASSERT(positionOfChild1 <= children.size());
    ROSE_ASSERT(positionOfChild2 <= children.size());
    ROSE_ASSERT(src == lca || positionOfChild1 != children.size());
    ROSE_ASSERT(tgt == lca || positionOfChild2 != children.size());
  }

  // Walk from "src" to "lca", but not "lca" itself
  for (size_t i = ancestorsOfFrom.size(); i > 2; --i) {
    SgNode *anc = ancestorsOfFrom[i - 2];
    vector<SgNode *> children = anc->get_traversalSuccessorContainer();
    // This doesn't use findParent because here we care about lexical
    // children, not CFG children
    size_t positionOfChild =
        std::find(children.begin(), children.end(), ancestorsOfFrom[i - 1]) -
        children.begin();
    ROSE_ASSERT(positionOfChild != children.size());
    for (size_t j = positionOfChild; j > 0; --j) {
      doEffectsBackwards(children[j - 1], data);
    }
  }
  // Walk "lca"'s children
  {
    vector<SgNode *> children = lca->get_traversalSuccessorContainer();
    if (positionOfChild1 < positionOfChild2) {
      for (size_t j = positionOfChild1 + 1; j < positionOfChild2; ++j) {
        doEffectsForwards(children[j], data);
      }
    } else if (positionOfChild1 > positionOfChild2) {
      for (size_t j = positionOfChild1; j > positionOfChild2 + 1; --j) {
        doEffectsBackwards(children[j - 1], data);
      }
    }
  }
  // Walk from "lca" to "tgt", but not "lca" itself
  for (size_t i = 1; i + 1 < ancestorsOfTo.size(); ++i) {
    SgNode *anc = ancestorsOfTo[i];
    vector<SgNode *> children = anc->get_traversalSuccessorContainer();
    // This doesn't use findParent because here we care about lexical
    // children, not CFG children
    size_t positionOfChild =
        std::find(children.begin(), children.end(), ancestorsOfTo[i + 1]) -
        children.begin();
    ROSE_ASSERT(positionOfChild != children.size());
    for (size_t j = 0; j < positionOfChild; ++j) {
      doEffectsForwards(children[j], data);
    }
  }
  return data;
}

void getEntriesNull(SgNode *, vector<SgInitializedName *> &) {}

template <bool Reverse>
void getEntriesForScope(SgNode *n, vector<SgInitializedName *> &entries) {
  SgStatement *s = isSgStatement(n);
  if (!s || isSgScopeStatement(s))
    return;
  vector<SgInitializedName *> names = findVariablesDirectlyInScope(s);
  if (Reverse) {
    entries.insert(entries.end(), names.begin(), names.end());
  } else {
    entries.insert(entries.end(), names.rbegin(), names.rend());
  }
}

vector<SgInitializedName *> windToLocation(CFGNode src, CFGNode tgt) {
  SgNode *srcNode = src.getNode();
  SgNode *tgtNode = tgt.getNode();
  ROSE_ASSERT(srcNode != nullptr);
  ROSE_ASSERT(tgtNode != nullptr);
  if (isSgExpression(srcNode) && isSgExpression(tgtNode)) {
    return vector<SgInitializedName *>(); // Common case, since these cannot be
                                          // arbitrary jumps
  }
  if (srcNode->get_parent() == tgtNode || tgtNode->get_parent() == srcNode) {
    return vector<SgInitializedName *>(); // We assume that these are
                                          // consecutive program points
  }
  vector<SgInitializedName *> scopesEntering = genericWindUnwind(
      srcNode, tgtNode, getEntriesForScope<false>, getEntriesNull);

  return scopesEntering;
}

vector<SgInitializedName *> unwindToLocation(CFGNode src, CFGNode tgt) {
  SgNode *srcNode = src.getNode();
  SgNode *tgtNode = tgt.getNode();
  ROSE_ASSERT(srcNode != nullptr);
  ROSE_ASSERT(tgtNode != nullptr);
  unsigned int tgtIndex = tgt.getIndex();
  if ((isSgExpression(srcNode) != NULL) && (isSgExpression(tgtNode) != NULL)) {
    return vector<SgInitializedName *>(); // Common case, since these cannot be
                                          // arbitrary jumps
  }
  vector<SgInitializedName *> scopesLeaving;
  SgScopeStatement *scope = isSgScopeStatement(srcNode);
  if (scope != nullptr && src.getIndex() == scope->cfgIndexForEnd())
    scopesLeaving = findVariablesDirectlyInScope(scope);
  if (srcNode->get_parent() != tgtNode && tgtNode->get_parent() != srcNode) {
    const auto enclosing = genericWindUnwind(srcNode, tgtNode, getEntriesNull,
                                             getEntriesForScope<true>);
    scopesLeaving.insert(scopesLeaving.end(), enclosing.begin(),
                         enclosing.end());
  }

  SgVariableDeclaration *iterationCondition = nullptr;
  if (auto *loop = isSgWhileStmt(tgtNode);
      loop != nullptr && tgtIndex == 0 &&
      (srcNode == loop->get_body() || isSgContinueStmt(srcNode)))
    iterationCondition = isSgVariableDeclaration(loop->get_condition());
  if (auto *loop = isSgForStatement(tgtNode);
      loop != nullptr && tgtIndex == 1 && srcNode == loop->get_increment())
    iterationCondition = isSgVariableDeclaration(loop->get_test());
  if (iterationCondition != nullptr) {
    const auto &variables = iterationCondition->get_variables();
    scopesLeaving.insert(scopesLeaving.end(), variables.rbegin(),
                         variables.rend());
  }

  bool isEndOfProcedure =
      isSgFunctionDefinition(tgtNode) && tgtIndex == tgtNode->cfgIndexForEnd();
  if (isEndOfProcedure) {
    // Special case: add in procedure parameters
    SgFunctionDefinition *fd = isSgFunctionDefinition(tgtNode);
    SgFunctionParameterList *pl = fd->get_declaration()->get_parameterList();
    const SgInitializedNamePtrList &params = pl->get_args();
    scopesLeaving.insert(scopesLeaving.end(), params.rbegin(), params.rend());
  }
  return scopesLeaving;
}

vector<SgInitializedName *> CFGEdge::scopesBeingExited() const {
  return unwindToLocation(source(), target());
}

vector<SgInitializedName *> CFGEdge::scopesBeingEntered() const {
  vector<SgInitializedName *> scopesEntering =
      windToLocation(source(), target());
  if (isSgInitializedName(src.getNode()) && src.getIndex() == 0) {
    scopesEntering.push_back(isSgInitializedName(src.getNode()));
  }
  return scopesEntering;
}

vector<CFGEdge> CFGNode::outEdges() const {
  ROSE_ASSERT(node);
  vector<CFGEdge> result = node->cfgOutEdges(index);
  for (const CFGEdge &edge : result)
    ROSE_ASSERT(edge.source().getNode() != NULL &&
                edge.target().getNode() != NULL);
  return result;
}

vector<CFGEdge> CFGNode::inEdges() const {
  ROSE_ASSERT(node);

  vector<CFGEdge> result = node->cfgInEdges(index);
  for (const CFGEdge &edge : result)
    ROSE_ASSERT(edge.source().getNode() != NULL &&
                edge.target().getNode() != NULL);
  return result;
}

vector<InterestingEdge> makeClosure(const vector<CFGEdge> &orig,
                                    vector<CFGEdge> (CFGNode::*closure)() const,
                                    CFGNode (CFGPath::*otherSide)() const,
                                    CFGPath (*merge)(const CFGPath &,
                                                     const CFGPath &)) {
  vector<CFGPath> rawEdges(orig.begin(), orig.end());
  while (true) {
  top:

    // CH (5/27/2010): 'push_back' may invalidate iterators of a vector.
    // Using index instead to fix this subtle bug.
    for (size_t i = 0; i < rawEdges.size(); ++i) {
      if (!(rawEdges[i].*otherSide)().isInteresting()) {
        unsigned int oldSize = rawEdges.size();
        vector<CFGEdge> rawEdges2 = ((rawEdges[i].*otherSide)().*closure)();
        for (unsigned int j = 0; j < rawEdges2.size(); ++j) {
          CFGPath merged = (*merge)(rawEdges[i], rawEdges2[j]);
          if (std::find(rawEdges.begin(), rawEdges.end(), merged) ==
              rawEdges.end()) {
            rawEdges.push_back(merged);
          }
        }
        if (rawEdges.size() != oldSize)
          goto top; // To restart iteration
      }
    }
    break; // If the iteration got all of the way through
  }
  vector<InterestingEdge> edges;
  for (vector<CFGPath>::const_iterator i = rawEdges.begin();
       i != rawEdges.end(); ++i) {
    if (((*i).*otherSide)().isInteresting())
      edges.push_back(InterestingEdge(*i));
  }
  return edges;
}

vector<InterestingEdge> InterestingNode::outEdges() const {
  return makeClosure(n.outEdges(), &CFGNode::outEdges, &CFGPath::target,
                     &mergePaths);
}

vector<InterestingEdge> InterestingNode::inEdges() const {
  return makeClosure(n.inEdges(), &CFGNode::inEdges, &CFGPath::source,
                     &mergePathsReversed);
}

CFGNode getCFGTargetOfFortranLabelSymbol(SgLabelSymbol *sym) {
  SgStatement *st = sym->get_fortran_statement();
  ROSE_ASSERT(st);

  switch (sym->get_label_type()) {
  case SgLabelSymbol::e_non_numeric_label_type:
  case SgLabelSymbol::e_start_label_type:
    return st->cfgForBeginning();

  case SgLabelSymbol::e_else_label_type: {
    SgIfStmt *ifs = isSgIfStmt(st);
    ROSE_ASSERT(ifs);
    ROSE_ASSERT(ifs->get_false_body() != NULL);
    return ifs->get_false_body()->cfgForBeginning();
  }

  case SgLabelSymbol::e_end_label_type: {
    if (auto *physicalLabel = st->get_numeric_label();
        physicalLabel != nullptr && physicalLabel->get_symbol() == sym) {
      return st->cfgForBeginning();
    } else if (auto *loop = isSgFortranDo(st)) {
      return CFGNode(loop, loop->cfgFindNextChildIndex(loop->get_body()));
    } else if (auto *loop = isSgWhileStmt(st)) {
      return CFGNode(loop, loop->cfgFindNextChildIndex(loop->get_body()));
    } else if (isSgProgramHeaderStatement(st)) {
      return isSgProgramHeaderStatement(st)->get_definition()->cfgForEnd();
    } else if (isSgProcedureHeaderStatement(st)) {
      return isSgProcedureHeaderStatement(st)->get_definition()->cfgForEnd();
    } else {
      return st->cfgForEnd();
    }
  }

  default: {
    ROSE_ASSERT(!"Invalid Fortran label type");
    return st->cfgForEnd();
  }
  }
}

CFGNode getCFGTargetOfFortranLabelRef(SgLabelRefExp *lRef) {
  ROSE_ASSERT(lRef);
  SgLabelSymbol *sym = lRef->get_symbol();
  ROSE_ASSERT(sym);
  return getCFGTargetOfFortranLabelSymbol(sym);
}

} // namespace VirtualCFG
