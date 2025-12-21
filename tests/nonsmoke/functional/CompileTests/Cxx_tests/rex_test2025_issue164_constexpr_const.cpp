// Verify explicit vs implicit const for constexpr variables.

typedef const int ConstInt;

constexpr int implicit_scalar = 1;
constexpr const int explicit_scalar = 2;

constexpr int *implicit_ptr = nullptr;
constexpr int *const explicit_ptr = nullptr;

constexpr const int *implicit_pointee = nullptr;
constexpr const int *const explicit_pointee_ptr = nullptr;

constexpr ConstInt implicit_typedef = 3;
constexpr const ConstInt explicit_typedef = 4;

int use_all() {
  return implicit_scalar + explicit_scalar + implicit_typedef +
         explicit_typedef;
}
