void test_nested_memory_refs(int cond, int *p, int *q, int *r) {
  (cond ? *p : *q) = 1;
  (cond, *r) = *p;
  (++cond, *r) = *q;
}
