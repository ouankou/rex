
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
template <class type> class QStack_alt {
  // void push( const type *d ) {};
  // When this function is built as an instantiation, it will cause the template
  // type to get name qualification. Where the template argument will be name
  // qualified, this effects all uses of the template type, even in the variable
  // declaration: "QStack_alt<State> m_stateStack_alt;".  If this is not good
  // enough then we need to keep maps of name qualifications so that they can be
  // organized more specific to the declarations there the types are being
  // referenced (since types are shared but statements are not).  Note clear if
  // we really need that level of complexity.
public:
  void push() { type x; }
};
enum State { Invalid };

struct TagFileParser {
  enum State { Invalid };

  void startMember() { m_stateStack_alt.push(); }

  // m_stateStack_alt.push(0L);
  // the type we have to allow some over qualification of the template type
  // names here.
  class QStack_alt<enum TagFileParser::State> m_stateStack_alt;
};
