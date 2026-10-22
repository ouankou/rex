/*************************************************************
 * Author   : Markus Schordan                                *
 *************************************************************/

#include "RoseAst.h"

#include "sage3basic.h"

#include <stdexcept>

RoseAst::RoseAst(SgNode *astNode) : _startNode(astNode) {}

SgNode *RoseAst::iterator::parent() const { return _stack.top().node; }

RoseAst::iterator::iterator()
    : _skipChildrenOnForward(false),
      _withNullValues(false) // default: null values are not traversed
{}

RoseAst::iterator::iterator(SgNode *x, bool withNullValues)
    : _skipChildrenOnForward(false), _withNullValues(withNullValues) {
  stack_element e;
  e.node = x;
  e.index = ROOT_NODE_INDEX; // only root node has this index
  _stack.push(e);
}

int RoseAst::iterator::stack_size() const { return _stack.size(); }
bool RoseAst::iterator::is_past_the_end() const { return _stack.size() == 0; }

bool RoseAst::iterator::operator==(const iterator &x) const {
  if (is_past_the_end() != x.is_past_the_end())
    return false;
  if (is_past_the_end() && x.is_past_the_end())
    return true;
  // this check ensures that 0 values work for trees. For DAGs we would need to
  // compare the entire stack (i.e. context). Hence, this comparison is
  // guaranteed to work for trees, but not for DAGs. For DAGs the comparison
  // might be true although the traversal is at different positions at shared
  // nodes. This is not problematic for comparisons with the past-the-end
  // comparison, but may be problematic when different DAGs are compared.
  if (_stack.top().node != x._stack.top().node ||
      _stack.top().index != x._stack.top().index)
    return false;
  // mode must be the same otherwise iterators are different as
  // ++ may go to different elements. Therefore i==j -> *i==*j is not violated
  // because in case of different modes we define == such that i==j does not
  // hold.
  if (_skipChildrenOnForward != x._skipChildrenOnForward ||
      _withNullValues != x._withNullValues)
    return false;
  return true;
}

bool RoseAst::iterator::operator!=(const iterator &x) const {
  return !(*this == x);
}

SgNode *RoseAst::iterator::operator*() const {
  if (_stack.size() == 0)
    throw std::out_of_range("Ast::iterator: past-the-end access");
  // access node by index
  stack_element e = _stack.top();
  if (e.index == ROOT_NODE_INDEX) {
    return e.node;
  } else {
    return access_node_by_parent_and_index(e.node, e.index);
  }
}

RoseAst::iterator RoseAst::iterator::operator++(int) {
  iterator tmp = *this;
  ++*this;
  return tmp;
}

RoseAst::iterator RoseAst::begin() { return iterator(_startNode, false); }

RoseAst::iterator RoseAst::end() { return iterator(); }

RoseAst::iterator &RoseAst::iterator::operator++() {
  if (is_past_the_end())
    return *this;

  stack_element e = _stack.top();
  _stack.pop();

  SgNode *node = e.index == ROOT_NODE_INDEX
                     ? e.node
                     : access_node_by_parent_and_index(e.node, e.index);
  bool skipChildren = _skipChildrenOnForward;
  _skipChildrenOnForward = false;
  if (!node || skipChildren)
    return *this;

  for (int index = node->get_numberOfTraversalSuccessors() - 1; index >= 0;
       --index) {
    if (_withNullValues || access_node_by_parent_and_index(node, index))
      _stack.push({node, index});
  }
  return *this;
}

SgNode *RoseAst::iterator::access_node_by_parent_and_index(SgNode *p,
                                                           int index) const {
  if (index < 0 || index >= p->get_numberOfTraversalSuccessors())
    throw std::out_of_range("RoseAst iterator child index out of bounds");
  return p->get_traversalSuccessorByIndex(index);
}

void RoseAst::iterator::skipChildrenOnForward() {
  _skipChildrenOnForward = true;
}

RoseAst::iterator &RoseAst::iterator::withNullValues() {
  if (_stack.empty() || _stack.top().index != ROOT_NODE_INDEX)
    throw std::out_of_range("RoseAst iterator mode change after traversal");
  _withNullValues = true;
  return *this;
}
