using RexSize = decltype(sizeof(0));
void *operator new(RexSize, void *) noexcept;

template <typename T>
using RexTypeOwnedNewResult = decltype(new (static_cast<void *>(nullptr)) T());

struct RexTypeOwnedNewValue {};
RexTypeOwnedNewResult<RexTypeOwnedNewValue> rex_type_owned_new_pointer;

void rex_ast_json_type_owned_new_expression(int *value) {
#pragma omp parallel
  {
    *value += 1;
  }
}
