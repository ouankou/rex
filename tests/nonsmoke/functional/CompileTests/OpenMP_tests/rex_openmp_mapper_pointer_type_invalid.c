typedef struct MapperRecord {
  int value;
} MapperRecord;

#pragma omp declare mapper(pointer_type : MapperRecord *v) map(tofrom : v)

int main(void) { return 0; }
