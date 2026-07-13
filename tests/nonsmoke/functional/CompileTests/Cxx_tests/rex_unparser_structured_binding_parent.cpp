struct binding_pair {
  int first;
  long second;
};

long structured_binding_value() {
  auto [first, second] = binding_pair{2, 3};
  return first + second;
}
