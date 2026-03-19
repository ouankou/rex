#pragma once

// Reduced support scaffold so these compile tests remain standalone.
class Box {
public:
  int numPts() const { return 0; }
};

class DenseIntVectSet {
public:
  Box box() const { return Box(); }
  DenseIntVectSet &operator|=(const DenseIntVectSet &) { return *this; }
};

class SparseIntVectSet {
public:
  SparseIntVectSet &operator|=(const SparseIntVectSet &) { return *this; }
};

class TreeNodePool {
public:
  void clear() {}
};

class StaticVector {
public:
  void clear() {}
};

class TreeIntVectSet {
public:
  static TreeNodePool *treeNodePool;
  static StaticVector index;
  static StaticVector parents;
  static StaticVector boxes;
  static StaticVector bufferOffset;
};

namespace CH_XD {
class MinBoxResult {
public:
  int numPts() const { return 0; }
};

inline MinBoxResult minBox(const Box &, const Box &) { return MinBoxResult(); }
} // namespace CH_XD

class IntVectSet {
public:
  IntVectSet &operator|=(const IntVectSet &ivs);
  static void clearStaticMemory();
  void convert() const {}

private:
  bool m_isdense = false;
  DenseIntVectSet m_dense;
  SparseIntVectSet m_ivs;
};

namespace test2014_intvectset_support {
static TreeNodePool treeNodePoolInstance;
}

TreeNodePool *TreeIntVectSet::treeNodePool =
    &test2014_intvectset_support::treeNodePoolInstance;
StaticVector TreeIntVectSet::index;
StaticVector TreeIntVectSet::parents;
StaticVector TreeIntVectSet::boxes;
StaticVector TreeIntVectSet::bufferOffset;
