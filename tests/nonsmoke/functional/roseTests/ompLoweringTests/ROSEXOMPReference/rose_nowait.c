/*
 */
#include <stdio.h>
#ifdef _OPENMP
#include "libxomp.h"

#include <omp.h>
#endif
int a[20];

void foo() {
  int i;
  {
    int _p_i;
    long p_index_;
    long p_lower_;
    long p_upper_;
    XOMP_loop_default(0, 19, 1, &p_lower_, &p_upper_);
    for (p_index_ = p_lower_; p_index_ <= p_upper_; p_index_ += 1) {
      a[p_index_] = p_index_ * 2;
    }
  }
}
