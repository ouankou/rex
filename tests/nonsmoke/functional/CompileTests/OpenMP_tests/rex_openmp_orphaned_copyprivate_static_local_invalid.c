static void broadcast_static_local(void) {
  static int value;

#pragma omp single copyprivate(value)
  {
    value = 42;
  }
}

int main(void) {
#pragma omp parallel
  {
    broadcast_static_local();
  }
  return 0;
}
