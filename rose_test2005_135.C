
struct __NSConstantString_tag {
  const int *isa;
  int flags;
  const char *str;
  long length;
};

struct __va_list_tag {
  unsigned int gp_offset;
  unsigned int fp_offset;
  void *overflow_arg_area;
  void *reg_save_area;
};
// Test of template directive

class PolygonalMesh {}

// Templated class
;

template <class MeshType> class ArtificialViscosity {
public:
  void computeZonalLengthScale();
};
template <class T> class DereferenceVector {};
template <class MeshType>
class ArtificialViscosityList
    : public DereferenceVector<ArtificialViscosity<MeshType>> {
private:
  typedef DereferenceVector<ArtificialViscosity<MeshType>> VectorType;

  // Template Forward declaration
};
template <class MeshType>
void ArtificialViscosity<MeshType>::computeZonalLengthScale() {}

// DQ (2/20/2010): This is a error for g++ 4.x compilers (at least g++ 4.2).
#if (__GNUC__ == 4)
#endif
// Template specialization
// Template Instantiation Directive
template class ArtificialViscosity<PolygonalMesh>;

template <>
void ArtificialViscosity<::PolygonalMesh>::computeZonalLengthScale() {}
