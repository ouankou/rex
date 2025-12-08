// Verifies function template definitions keep their template parameter lists.
// Explicit instantiation should fail to compile if the template information is
// dropped.

template <int N> void foo() {}
template void foo<1>();

int main() { return 0; }
