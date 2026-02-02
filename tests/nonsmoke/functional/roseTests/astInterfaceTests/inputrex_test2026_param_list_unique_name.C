int add_values(int lhs, double rhs) { return lhs + static_cast<int>(rhs); }

int main() { return add_values(1, 2.0); }
