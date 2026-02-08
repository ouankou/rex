
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
template <class T1> class reverse_iterator {
private:
  T1 abc;
};
template <class T1, class T2, class T3> class XYZ {
public:
  typedef T1 *pointer;
  typedef pointer iterator;
  typedef ::reverse_iterator<XYZ<T1, T2 = int, T3 = long>::iterator>
      reverse_iterator;
};

/* This program generates an error in legacy frontend/SAGE connection
s->kind = template (kind = 51) iek_last = 52
Error: case not implemented in generateFileInfo ( a_source_sequence_entry_ptr s)
NOTE: Since these templates are not instantiated anywhere, no code is produced
      from this file within the unparsing phase.
 */
