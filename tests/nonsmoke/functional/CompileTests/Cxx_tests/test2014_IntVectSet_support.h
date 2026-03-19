#pragma once

class Box {
public:
  int numPts() const;
};

class DenseIntVectSet {
public:
  Box box() const;
  DenseIntVectSet &operator|=(const DenseIntVectSet &);
};

class SparseIntVectSet {
public:
  SparseIntVectSet &operator|=(const SparseIntVectSet &);
};

class TreeNodePool {
public:
  void clear();
};

class StaticVector {
public:
  void clear();
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
  int numPts() const;
};

MinBoxResult minBox(const Box &, const Box &);
} // namespace CH_XD

class IntVectSet {
public:
  IntVectSet &operator|=(const IntVectSet &ivs);
  static void clearStaticMemory();
  void convert() const;

private:
  bool m_isdense;
  DenseIntVectSet m_dense;
  SparseIntVectSet m_ivs;
};
