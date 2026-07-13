struct MapperBase {
  int value;
};

struct MapperDerived : virtual MapperBase {};

#pragma omp declare mapper(virtual_base : MapperDerived v) map(to : v)

int main() { return 0; }
