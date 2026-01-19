
struct sockaddr{};
struct sockaddr_in{};

enum XXX {
    LSA_LEN_SIZE = 0,
    LSA_SIZEOF_SA = sizeof(
        struct NEW_UNION_TYPE_NAME {
            struct sockaddr sa;
            struct sockaddr_in sin;
        }
    ),

   NEW_ENUM_ITEM = sizeof(int)    // If this is legal then this is extra ugly
// NEW_ENUM_ITEM = sizeof(NEW_UNION_TYPE_NAME)    // If this is legal then this is extra ugly
};

// NEW_UNION_TYPE_NAME x;
