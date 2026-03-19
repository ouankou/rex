//
// Main program illustrating basic index set creation and traversal.
//

#include <cstdlib>

#include <string>

#include <iostream>

template <typename T> T *assume_aligned_32(T *ptr) {
#if defined(__INTEL_COMPILER)
  __assume_aligned(ptr, 32);
  return ptr;
#elif defined(__clang__) || defined(__GNUC__)
  return static_cast<T *>(__builtin_assume_aligned(ptr, 32));
#elif defined(__IBMCPP__) || defined(__ibmxl__)
  __alignx(32, ptr);
  return ptr;
#else
#error                                                                         \
    "test2012_139.C requires compiler support for an alignment-assumption builtin"
#endif
}

/*!
 ******************************************************************************
 *
 * \brief  Simple range traversal template method.
 *
 ******************************************************************************
 */
template <typename LOOP_BODY>
void IndexSet_forall(unsigned begin, unsigned length, LOOP_BODY loop_body) {
  for (unsigned ii = 0; ii < length; ++ii) {
    loop_body(ii + begin);
  }
}

/*!
 ******************************************************************************
 *
 * \brief  Function to check result.
 *
 ******************************************************************************
 */
void ResultCheck(const std::string &name, double *ref_result, double *to_check,
                 unsigned iset_len) {
  bool is_correct = true;
  for (unsigned i = 0; i < iset_len; ++i) {
    is_correct &= ref_result[i] == to_check[i];
  }

  std::cout << name << "is " << (is_correct ? "CORRECT" : "WRONG") << std::endl;
}

/*!
 ******************************************************************************
 *
 * \brief  Function to check result.
 *
 ******************************************************************************
 */
class TestOp {
public:
  TestOp(double *__restrict__ parent, double *__restrict__ child)
      : m_parent(parent), m_child(child) {
    ;
  }

  void operator()(unsigned idx) {
    double *__restrict__ parent = assume_aligned_32(m_parent);
    double *__restrict__ child = assume_aligned_32(m_child);
    child[idx] = parent[idx] * parent[idx];
  }

  double *__restrict__ m_parent;
  double *__restrict__ m_child;
};

int main(int argc, char *argv[]) {
  //
  // Allocate and initialize arrays for tests...
  //
  const unsigned array_length = 320;

  double *parent;
  double *child;
  double *child_ref;
  posix_memalign((void **)&parent, 32, array_length * sizeof(double));
  posix_memalign((void **)&child, 32, array_length * sizeof(double));
  posix_memalign((void **)&child_ref, 32, array_length * sizeof(double));

  for (int i = 0; i < array_length; ++i) {
    parent[i] = (double)(rand() % 65536);
    child[i] = 0.0;
    child_ref[i] = 0.0;
  }

  //
  // Generate reference result to check correctness
  //

  auto ref_op = [&](int idx) {
    double *aligned_parent = assume_aligned_32(parent);
    double *aligned_child_ref = assume_aligned_32(child_ref);
    aligned_child_ref[idx] = aligned_parent[idx] * aligned_parent[idx];
  };

  // Execute full array traversal as correct result...
  IndexSet_forall(0, array_length, ref_op);

  TestOp test_op(parent, child);

  // Execute half array traversal and check result...
  IndexSet_forall(0, array_length / 2, test_op);
  ResultCheck("half array ", child_ref, child, array_length / 2);

  free(parent);
  free(child);
  free(child_ref);

  std::cout << "\n DONE!!! " << std::endl;

  return 0;
}
