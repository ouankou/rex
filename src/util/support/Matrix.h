
#ifndef MATRIX_TMPL_H
#define MATRIX_TMPL_H

#include <assert.h>

#include <limits>
#include <memory>
#include <stdexcept>

template <class T> class Matrix {
  unsigned nr, nc, num;
  std::unique_ptr<T[]> vec;

  static unsigned checkedCapacity(unsigned rows, unsigned cols,
                                  unsigned spare) {
    const unsigned max = std::numeric_limits<unsigned>::max();
    if (rows > max - spare || cols > max - spare)
      throw std::length_error("Matrix dimensions overflow capacity");
    rows += spare;
    cols += spare;
    if (rows != 0 && cols > max / rows)
      throw std::length_error("Matrix dimensions overflow capacity");
    return rows * cols;
  }

public:
  typedef T Elem;

  Matrix() : nr(0), nc(0), num(0) {}
  Matrix(unsigned _nr, unsigned _nc, unsigned spare = 1) {
    nr = _nr;
    nc = _nc;
    num = checkedCapacity(nr, nc, spare);
    if (num != 0)
      vec = std::make_unique<T[]>(num);
  }

  Matrix(const Matrix &that) {
    nr = that.nr;
    nc = that.nc;
    num = that.num;
    if (num != 0) {
      vec = std::make_unique<T[]>(num);
      for (size_t i = 0; i < num; ++i)
        vec[i] = that.vec[i];
    }
  }
  void operator=(const Matrix &that) {
    if (num < that.num) {
      vec = std::make_unique<T[]>(that.num);
      num = that.num;
    }
    nr = that.nr;
    nc = that.nc;
    if (that.num > 0) {
      for (unsigned i = 0; i < that.num; ++i)
        vec[i] = that.vec[i];
    }
  }

  unsigned cols() const { return nc; }
  unsigned rows() const { return nr; }
  T &Entry(unsigned index1, unsigned index2) const {
    unsigned index = index1 * nc + index2;
    return vec[index];
  }

  T &operator()(unsigned index1, unsigned index2) const {
    return Entry(index1, index2);
  }

  void Reset(unsigned index1, unsigned index2) {
    unsigned num1 = checkedCapacity(index1, index2, 0);
    if (num < num1) {
      vec = std::make_unique<T[]>(num1);
      num = num1;
    }
    nr = index1;
    nc = index2;
  }

  void Initialize(const T &init) {
    for (size_t i = 0; i < num; i++)
      vec[i] = init;
  }
};

template <class Mat, class ElemOp>
bool UpdateMatrix(Mat &d1, const Mat &d2, ElemOp op) {
  assert(d1.rows() == d2.rows() && d1.cols() == d2.cols());
  bool mod = false;
  for (int i = 0; i < d1.rows(); i++) {
    for (int j = 0; j < d1.cols(); j++) {
      typename Mat::Elem e1 = d1.Entry(i, j);
      if (op(e1, d2.Entry(i, j))) {
        mod = true;
        d1.Entry(i, j) = e1;
      }
    }
  }
  return mod;
}

#endif
