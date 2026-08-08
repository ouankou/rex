static void broadcast_automatic_local(void) {
  int value;

#pragma omp single copyprivate(value)
  {
    value = 42;
  }
}

int main(void) {
#pragma omp parallel
  {
    broadcast_automatic_local();
  }
  return 0;
}
