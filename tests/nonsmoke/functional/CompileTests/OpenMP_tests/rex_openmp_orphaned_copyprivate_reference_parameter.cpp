static void broadcast_lvalue_reference(int &value) {
#pragma omp single copyprivate(value)
  {
    value = 42;
  }
}

static void broadcast_rvalue_reference(int &&value) {
#pragma omp single copyprivate(value)
  {
    value = 84;
  }
}

int main() {
#pragma omp parallel
  {
    int value = 0;
    broadcast_lvalue_reference(value);
    broadcast_rvalue_reference(static_cast<int &&>(value));
  }
  return 0;
}
