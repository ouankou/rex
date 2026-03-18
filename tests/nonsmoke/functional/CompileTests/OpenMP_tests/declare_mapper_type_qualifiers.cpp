typedef struct MapperRecord_ {
  int value;
} MapperRecord;

template <int N> struct TemplateRecord {
  int value;
};

typedef TemplateRecord<123> TemplateRecord123;

#pragma omp declare mapper(base_const : MapperRecord const v)
#pragma omp declare mapper(ptr_to_const : const MapperRecord *v)
#pragma omp declare mapper(const_ptr : MapperRecord *const v)
#pragma omp declare mapper(ptr_to_const_ptr : MapperRecord *const *v)
#pragma omp declare mapper(template_int : TemplateRecord<123> v)

int main() { return 0; }
