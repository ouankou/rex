#include "featureTests.h"
#ifdef ROSE_ENABLE_SOURCE_ANALYSIS

#ifndef LATTICE_FULL_H
#define LATTICE_FULL_H

#include "lattice.h"

#include <string>

#include <vector>

/******************************
 *** Commonly used lattices ***
 ******************************/

class BoolAndLattice : public FiniteLattice {
  // state can be:
  // -1 : unset (default value)
  // 0 : false
  // 1 : true
  int state;

public:
  BoolAndLattice() { state = -1; }

private:
  BoolAndLattice(int state) { this->state = state; }

public:
  BoolAndLattice(bool state) { this->state = state; }

  // initializes this Lattice to its default state
  void initialize() { state = -1; }

  // returns a copy of this lattice
  Lattice *copy() const;

  // overwrites the state of this Lattice with that of that Lattice
  void copy(Lattice *that);

  // computes the meet of this and that and saves the result in this
  // returns true if this causes this to change and false otherwise
  bool meetUpdate(Lattice *that);

  bool operator==(Lattice *that);

  // returns the current state of this object
  bool get() const;

  // sets the state of this BoolAndLattice to the given value
  // returns true if this causes the BoolAndLattice state to change, false
  // otherwise
  bool set(bool state);

  // sets the state of this lattice to the conjunction of the BoolAndLattice's
  // current state and the given value returns true if this causes the
  // BoolAndLattice state to change, false otherwise
  bool andUpd(bool state);

  std::string str(std::string indent = "");
};

/*########################
  ### Utility lattices ###
  ########################*/

// A container lattice to store other lattices
class ProductLattice : public virtual Lattice {
public:
  // The different levels of this lattice
  static const int uninitialized = 0;
  static const int initialized = 1;
  // This object's current level in the lattice: (uninitialized or initialized)
  short level;

protected:
  std::vector<Lattice *> lattices;

public:
  ProductLattice();
  ProductLattice(const std::vector<Lattice *> &lattices);
  ProductLattice(const ProductLattice &that);
  ~ProductLattice();

  void init(const std::vector<Lattice *> &lattices);

  // initializes this Lattice to its default state
  void initialize();

  const std::vector<Lattice *> &getLattices();

  // initializes the given vector with a copy of the lattices vector
  void copy_lattices(std::vector<Lattice *> &newLattices) const;

  // overwrites the state of this Lattice with that of that Lattice
  virtual void copy(Lattice *that);

  // computes the meet of this and that and saves the result in this
  // returns true if this causes this to change and false otherwise
  virtual bool meetUpdate(Lattice *that);

  virtual bool operator==(Lattice *that);

  int getLevel() { return level; }

  // The string that represents this object
  // If indent!="", every line of this string must be prefixed by indent
  // The last character of the returned string should not be '\n', even if it is
  // a multi-line string.
  virtual std::string str(std::string indent = "");
};

class FiniteProductLattice : public virtual ProductLattice,
                             public virtual FiniteLattice {
public:
  FiniteProductLattice() : ProductLattice(), FiniteLattice() {}

  FiniteProductLattice(const std::vector<Lattice *> &lattices)
      : ProductLattice(lattices), FiniteLattice() {
    verifyFinite();
  }

  FiniteProductLattice(const FiniteProductLattice &that)
      : Lattice(that), ProductLattice(that), FiniteLattice() {
    verifyFinite();
  }

  void verifyFinite() {
    for (std::vector<Lattice *>::iterator it = lattices.begin();
         it != lattices.end(); it++)
      ROSE_ASSERT((*it)->finiteLattice());
  }

  // Returns a copy of this lattice
  Lattice *copy() const { return new FiniteProductLattice(*this); }
};

#endif
#endif
