#include "featureTests.h"
#ifdef ROSE_ENABLE_SOURCE_ANALYSIS

#include "Cxx_Grammar.h"
#include "VirtualCFGIterator.h"

#include "variables.h"

#include "virtualCFG.h"

#include <list>
#include <set>
#include <sstream>
#include <string>
#include <vector>

using std::list;
using std::ostringstream;
using std::set;
using std::string;
using std::vector;

namespace VirtualCFG {

/******************************
 ********** ITERATOR **********
 ******************************/

iterator::iterator() { initialized = false; }

iterator::iterator(const DataflowNode &start) {
  initialized = true;
  remainingNodes.push_front(start);
  visited.insert(start);
}

void iterator::init(const DataflowNode &start) {
  initialized = true;
  remainingNodes.push_front(start);
  visited.insert(start);
}

// returns true if the given DataflowNode is in the remainingNodes list and
// false otherwise
bool iterator::isRemaining(DataflowNode n) {
  ROSE_ASSERT(initialized);
  for (list<DataflowNode>::const_iterator it = remainingNodes.begin();
       it != remainingNodes.end(); it++) {
    // if in is currently n remainingNodes, say so
    if (*it == n)
      return true;
  }
  // n is not in remainingNodes
  return false;
}

// advances this iterator in the given direction. Forwards if fwDir=true and
// backwards if fwDir=false. if pushAllChildren=true, all of the current node's
// unvisited children (predecessors or successors,
//    depending on fwDir) are pushed onto remainingNodes
void iterator::advance(bool fwDir, bool pushAllChildren) {
  ROSE_ASSERT(initialized);
  if (remainingNodes.size() > 0) {
    // pop the next CFG node from the front of the list
    DataflowNode cur = remainingNodes.front();
    remainingNodes.pop_front();

    if (pushAllChildren) {
      // find its followers (either successors or predecessors, depending on
      // value of fwDir), push back those that have not yet been visited
      vector<DataflowEdge> nextE;
      if (fwDir)
        nextE = cur.outEdges();
      else
        nextE = cur.inEdges();
      for (vector<DataflowEdge>::iterator it = nextE.begin(); it != nextE.end();
           it++) {
        DataflowNode nextN = fwDir ? it->target() : it->source();

        // if we haven't yet visited this node and don't yet have it on the
        // remainingNodes list
        if (visited.find(nextN) == visited.end() && !isRemaining(nextN)) {
          remainingNodes.push_back(nextN);
        }
      }
    }

    // if we still have any nodes left remaining
    if (remainingNodes.size() > 0) {
      // take the next node from the front of the list and mark it as visited
      visited.insert(remainingNodes.front());
    }
  }
}

void iterator::operator++(int) {
  ROSE_ASSERT(initialized);
  advance(true, true);
}

bool iterator::eq(const iterator &other_it) const {
  if (initialized != other_it.initialized)
    return false;

  // If both iterators are not initialized, they're equal
  if (!initialized)
    return true;

  if (remainingNodes.size() != other_it.remainingNodes.size())
    return false;

  list<DataflowNode>::const_iterator it1, it2;
  // look to ensure that every CFG node in other_it.remainingNodes appears in
  // remainingNodes

  for (it1 = remainingNodes.begin(); it1 != remainingNodes.end(); it1++) {
    for (it2 = other_it.remainingNodes.begin();
         it2 != other_it.remainingNodes.end(); it2++) {
      // if we found *it1 inside other_it.remainingNodes
      if (*it1 == *it2) {
        break;
      }
    }

    // the two iterators are not equal if ...

    // the current node in remainingNodes was not found in
    // other_it.remainingNodes
    if (it2 == other_it.remainingNodes.end()) {
      return false;
    }

    // or the two nodes do not have the same visited status in both iterators
    if ((visited.find(*it1) == visited.end()) !=
        (other_it.visited.find(*it1) == other_it.visited.end())) {
      return false;
    }
  }

  return true;
}

bool iterator::operator==(const iterator &other_it) const {
  return eq(other_it);
}

bool iterator::operator!=(const iterator &it) const { return !(*this == it); }

DataflowNode &iterator::operator*() {
  ROSE_ASSERT(initialized);
  return remainingNodes.front();
}

iterator iterator::begin(DataflowNode n) {
  iterator newIter(n);
  return newIter;
}

iterator iterator::end() {
  iterator blank;
  // Make sure that this iterator is initialized even though it is empty
  blank.initialized = true;
  return blank;
}

iterator::checkpoint::checkpoint(const list<DataflowNode> &remainingNodes,
                                 const set<DataflowNode> &visited) {
  this->remainingNodes = remainingNodes;
  this->visited = visited;
}

iterator::checkpoint::checkpoint(const iterator::checkpoint &that) {
  this->remainingNodes = that.remainingNodes;
  this->visited = that.visited;
}

string iterator::checkpoint::str(string indent) {
  ostringstream outs;
  outs << indent << "[VirtualCFG::iterator::checkpoint : \n";
  for (list<DataflowNode>::iterator it = remainingNodes.begin();
       it != remainingNodes.end();) {
    outs << indent << "    <" << (*it).getNode()->class_name() << " | "
         << (*it).getNode()->unparseToString() << ">";
    it++;
    if (it != remainingNodes.end())
      outs << "\n";
  }
  outs << "]";
  return outs.str();
}

// Returns a checkpoint of this iterator's progress.
iterator::checkpoint iterator::getChkpt() {
  ROSE_ASSERT(initialized);
  iterator::checkpoint chkpt(remainingNodes, visited);
  return chkpt;
}

// Loads this iterator's state from the given checkpoint.
void iterator::restartFromChkpt(iterator::checkpoint &chkpt) {
  remainingNodes = chkpt.remainingNodes;
  visited = chkpt.visited;
  // The iterator must become initialized because it is only possible to take a
  // checkpoint of an initialized iterator.
  initialized = true;
}

string iterator::str(string indent) {

  ostringstream outs;

  if (initialized) {
    outs << "[iterator:\n";
    outs << "    remainingNodes = \n";
    for (list<DataflowNode>::iterator it = remainingNodes.begin();
         it != remainingNodes.end(); it++) {
      outs << "        <" << (*it).getNode()->class_name() << " | "
           << (*it).getNode()->unparseToString() << " | " << (*it).getIndex()
           << "\n";
    }

    outs << "    visited = \n";
    for (set<DataflowNode>::iterator it = visited.begin(); it != visited.end();
         it++) {
      outs << "        <" << (*it).getNode()->class_name() << " | "
           << (*it).getNode()->unparseToString() << " | " << (*it).getIndex()
           << "\n";
    }

    outs << "]";
  } else {
    outs << "[iterator: Uninitialized]";
  }

  return outs.str();
}

/*****************************
 ******* BACK_ITERATOR *******
 *****************************/

void back_iterator::operator++(int) {
  ROSE_ASSERT(initialized);
  advance(false, true);
}

/******************************
 ********** DATAFLOW **********
 ******************************/

dataflow::dataflow(const DataflowNode &terminator_arg)
    : terminator(terminator_arg) {
  initialized = true;
  // Record that the terminator has been visited to ensure that it is never
  // analyzed
  visited.insert(terminator);
}

dataflow::dataflow(const DataflowNode &start,
                   const DataflowNode &terminator_arg)
    : iterator(start), terminator(terminator_arg) {
  // Record that the terminator has been visited to ensure that it is never
  // analyzed
  visited.insert(terminator);

  ROSE_ASSERT(start != terminator);
}

void dataflow::init(const DataflowNode &start_arg,
                    const DataflowNode &terminator_arg) {
  ROSE_ASSERT(start_arg != terminator_arg);
  // Use the init method to initialize the starting point to make sure that the
  // object is recorded as being initialized
  iterator::init(start_arg);
  terminator = terminator_arg;
}

void dataflow::add(const DataflowNode &next) {
  // If this dataflow iterator is not initialized, initialize it now since it
  // will now have real state
  if (!initialized)
    initialized = true;

  // never add the terminator node
  if (next == terminator)
    return;

  // if next is not currently in remainingNodes, add it
  if (!isRemaining(next)) {
    set<DataflowNode>::iterator nextLoc = visited.find(next);
    if (nextLoc != visited.end())
      visited.erase(nextLoc);
    remainingNodes.push_back(next);
  }
}

dataflow::checkpoint::checkpoint(const iterator::checkpoint &iChkpt,
                                 const DataflowNode &terminator)
    : iChkpt(iChkpt), terminator(terminator) {}

dataflow::checkpoint::checkpoint(const dataflow::checkpoint &that)
    : iChkpt(that.iChkpt), terminator(that.terminator) {}

string dataflow::checkpoint::str(string indent) {
  ostringstream outs;
  outs << indent << "[VirtualCFG::dataflow::checkpoint : \n";
  outs << indent << "    iterator = \n" << iChkpt.str(indent + "    ") << "\n";
  outs << indent << "    terminator = <" << terminator.getNode()->class_name()
       << " | " << terminator.getNode()->unparseToString() << ">]";
  return outs.str();
}

// Returns a checkpoint of this iterator's progress.
dataflow::checkpoint dataflow::getChkpt() {
  ROSE_ASSERT(initialized);
  dataflow::checkpoint chkpt(iterator::getChkpt(), terminator);
  return chkpt;
}

// Loads this iterator's state from the given checkpoint.
void dataflow::restartFromChkpt(dataflow::checkpoint &chkpt) {
  iterator::restartFromChkpt(chkpt.iChkpt);
  terminator = chkpt.terminator;
}

string dataflow::str(string indent) {
  ostringstream outs;

  if (initialized) {
    outs << "[dataflow:\n";
    outs << "    iterator = " << iterator::str(indent + "    ") << "\n";
    outs << "    terminator = " << terminator.getNode()->class_name() << " | "
         << terminator.getNode()->unparseToString() << " | "
         << terminator.getIndex() << "]";
  } else {
    outs << "[dataflow: Uninitialized]";
  }

  return outs.str();
}

/*****************************
******* BACK_DATAFLOW *******
*****************************/
back_dataflow::back_dataflow(const DataflowNode &end,
                             const DataflowNode &terminator_arg)
    : iterator(end), dataflow(end, terminator_arg) {}

void back_dataflow::operator++(int) { advance(false, true); }

} // namespace VirtualCFG

#endif
