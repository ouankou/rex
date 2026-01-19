// DQ (12/17/2013): This test code is not valid C++, but it is valid C code.

struct sockaddr{};
struct sockaddr_in{};

#ifndef __cplusplus
  #warning "This is not valid C++ code!"

enum XXX {
    LSA_LEN_SIZE = 0,
    LSA_SIZEOF_SA = sizeof(
        union NEW_UNION_TYPE_NAME {
            struct sockaddr sa;
            struct sockaddr_in sin;
        };
    ),

// NEW_ENUM_ITEM = sizeof(NEW_UNION_TYPE_NAME)    // If this is legal then this is extra ugly
};
#endif

// NEW_UNION_TYPE_NAME x;
