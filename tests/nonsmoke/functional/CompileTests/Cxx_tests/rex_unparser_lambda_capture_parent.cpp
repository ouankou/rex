int nested_capture_value(int seed) {
  int offset = 2;
  auto outer = [seed, &offset](int scale) {
    auto inner = [seed, scale, &offset]() { return seed * scale + offset; };
    return inner();
  };
  return outer(3);
}
