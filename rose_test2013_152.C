
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

#define uint32 unsigned int
unsigned int ghtonl(unsigned int x)
#define uint8 unsigned char
// ===================================================================
// emulates google3/util/endian/endian.h
//
// TODO(xiaofeng): PROTOBUF_LITTLE_ENDIAN is unfortunately defined in
// google/protobuf/io/coded_stream.h and therefore can not be used here.
// Maybe move that macro definition here in the furture.
{
  return result;
}
