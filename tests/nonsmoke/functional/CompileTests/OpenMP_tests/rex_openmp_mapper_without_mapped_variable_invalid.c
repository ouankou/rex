typedef struct MapperRecord {
  int value;
} MapperRecord;

int unrelated;

#pragma omp declare mapper(no_mapped_variable : MapperRecord v)                \
    map(to : unrelated)

int main(void) { return 0; }
