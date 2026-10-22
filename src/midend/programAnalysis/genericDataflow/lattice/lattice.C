#include "featureTests.h"
#ifdef ROSE_ENABLE_SOURCE_ANALYSIS

#include "latticeFull.h"

#include <sstream>
using namespace std;

/********************************************
 ************** BoolAndLattice **************
 ********************************************/

// returns a copy of this lattice
Lattice *BoolAndLattice::copy() const { return new BoolAndLattice(state); }

// overwrites the state of this Lattice with that of that Lattice
void BoolAndLattice::copy(Lattice *that) {
  state = dynamic_cast<BoolAndLattice *>(that)->state;
}

// computes the meet of this and that and saves the result in this
// returns true if this causes this to change and false otherwise
bool BoolAndLattice::meetUpdate(Lattice *that) {
  int newState = (state > dynamic_cast<BoolAndLattice *>(that)->state
                      ? state
                      : dynamic_cast<BoolAndLattice *>(that)->state);
  bool ret = newState != state;
  state = newState;
  return ret;
}

bool BoolAndLattice::operator==(Lattice *that) {
  return state == dynamic_cast<BoolAndLattice *>(that)->state;
}

// returns the current state of this object
bool BoolAndLattice::get() const {
  if (state < 0)
    return false;
  else
    return state;
}

// sets the state of this BoolAndLattice to the given value
// returns true if this causes the BoolAndLattice state to change, false
// otherwise
bool BoolAndLattice::set(bool state) {
  const int previous = this->state;
  this->state = state;
  return previous != this->state;
}

// sets the state of this lattice to the conjunction of the BoolAndLattice's
// current state and the given value returns true if this causes the
// BoolAndLattice state to change, false otherwise
bool BoolAndLattice::andUpd(bool state) {
  const int previous = this->state;
  this->state = previous == -1 ? state : previous && state;
  return previous != this->state;
}

string BoolAndLattice::str(string indent) {
  ostringstream outs;
  if (state == -1)
    outs << indent << "?";
  else
    outs << indent << (state ? "true" : "false");
  return outs.str();
}

/**********************
 *** ProductLattice ***
 **********************/

ProductLattice::ProductLattice() { level = uninitialized; }

ProductLattice::ProductLattice(const vector<Lattice *> &lattices) {
  level = uninitialized;
  init(lattices);
}

ProductLattice::ProductLattice(const ProductLattice &that) : Lattice(that) {
  level = that.level;
  that.copy_lattices(lattices);
}

ProductLattice::~ProductLattice() {
  // Deallocate all the lattices
  for (vector<Lattice *>::iterator lat = lattices.begin();
       lat != lattices.end(); lat++)
    delete *lat;
}

void ProductLattice::init(const vector<Lattice *> &lattices) {
  this->lattices = lattices;
}

// initializes this Lattice to its default state
void ProductLattice::initialize() {
  if (level != initialized) {
    level = initialized;
    for (vector<Lattice *>::iterator it = lattices.begin();
         it != lattices.end(); it++) {
      (*it)->initialize();
    }
  }
}

const vector<Lattice *> &ProductLattice::getLattices() { return lattices; }

// initializes the given vector with a copy of the lattices vector
void ProductLattice::copy_lattices(vector<Lattice *> &newLattices) const {
  for (vector<Lattice *>::const_iterator it = lattices.begin();
       it != lattices.end(); it++)
    newLattices.push_back((*it)->copy());
}

// overwrites the state of this Lattice with that of that Lattice
void ProductLattice::copy(Lattice *that_arg) {
  ProductLattice *that = dynamic_cast<ProductLattice *>(that_arg);
  ROSE_ASSERT(that);
  if (that == this)
    return;
  level = that->level;
  // Deallocate all the Lattices
  for (vector<Lattice *>::iterator lat = lattices.begin();
       lat != lattices.end(); lat++)
    delete *lat;
  lattices.clear();
  that->copy_lattices(lattices);
}

// computes the meet of this and that and saves the result in this
// returns true if this causes this to change and false otherwise
bool ProductLattice::meetUpdate(Lattice *that_arg) {
  ProductLattice *that = dynamic_cast<ProductLattice *>(that_arg);
  ASSERT_not_null(that);
  ROSE_ASSERT(lattices.size() == that->lattices.size());

  int newLevel = max(level, that->level);
  bool modified = newLevel != level;
  level = newLevel;

  for (size_t i = 0; i < lattices.size(); ++i)
    modified = lattices[i]->meetUpdate(that->lattices[i]) || modified;

  return modified;
}

bool ProductLattice::operator==(Lattice *that_arg) {
  ProductLattice *that = dynamic_cast<ProductLattice *>(that_arg);
  ASSERT_not_null(that);
  if (level != that->level || lattices.size() != that->lattices.size())
    return false;

  for (size_t i = 0; i < lattices.size(); ++i)
    if (*lattices[i] != *that->lattices[i])
      return false;
  return true;
}

// The string that represents this object
// If indent!="", every line of this string must be prefixed by indent
// The last character of the returned string should not be '\n', even if it is a
// multi-line string.
string ProductLattice::str(string indent) {
  ostringstream outs;
  outs << indent << "[ProductLattice: level="
       << (level == uninitialized ? "uninitialized" : "initialized") << "\n";
  for (vector<Lattice *>::const_iterator it = lattices.begin();
       it != lattices.end(); it++)
    outs << (*it)->str(indent + "    ") << "\n";
  outs << indent << "]\n";
  return outs.str();
}

#endif
