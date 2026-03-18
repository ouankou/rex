typedef struct MapperRecord_ {
  int value;
} MapperRecord;

#pragma omp declare mapper(base_const : MapperRecord const v)
#pragma omp declare mapper(ptr_to_const : const MapperRecord *v)
#pragma omp declare mapper(const_ptr : MapperRecord *const v)
#pragma omp declare mapper(ptr_to_const_ptr : MapperRecord *const *v)

int main() { return 0; }
