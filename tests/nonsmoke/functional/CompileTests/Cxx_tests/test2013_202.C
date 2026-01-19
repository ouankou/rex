// This test code demonstrates an un-terminated #if..#endif pair.

int ghtonl(int x) {
  union {
    int result;
  };

  return result;
}
