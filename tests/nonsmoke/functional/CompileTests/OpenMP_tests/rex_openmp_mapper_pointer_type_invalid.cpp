struct MapperRecord {
  int value;
};

#pragma omp declare mapper(pointer_type : MapperRecord *v) map(tofrom : v)

int main() { return 0; }
