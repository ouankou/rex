
#define uint32 unsigned int
#define uint8  unsigned char

// ===================================================================
// emulates google3/util/endian/endian.h
//
// TODO(xiaofeng): PROTOBUF_LITTLE_ENDIAN is unfortunately defined in
// google/protobuf/io/coded_stream.h and therefore can not be used here.
// Maybe move that macro definition here in the furture.
uint32 ghtonl(uint32 x) {
  union {
    uint32 result;
    uint8 result_array[4];
  };

  return result;
}
