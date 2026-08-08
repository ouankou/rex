static void broadcast_parameter(int value) {
#pragma omp single copyprivate(value)
  {
    value = 42;
  }
}

int main() {
#pragma omp parallel
  {
    broadcast_parameter(0);
  }
  return 0;
}
