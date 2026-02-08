
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
#define NULL 0L

class Descriptor {};

class FieldDescriptor {};

struct Symbol {
  enum Type {
    NULL_SYMBOL,
    MESSAGE,
    FIELD,
    ENUM,
    ENUM_VALUE,
    SERVICE,
    METHOD,
    PACKAGE
  };
  enum Type type;
  union {
    const class Descriptor *descriptor;
    const class FieldDescriptor *field_descriptor;
  };

  Symbol()
      : type(NULL_SYMBOL
             // inline bool IsNull() const { return type == NULL_SYMBOL; }
        ) {
    .descriptor = 0L;
  }
// inline bool IsType() const { return type == MESSAGE || type == ENUM; }
// inline bool IsAggregate() const { return type == MESSAGE || type == PACKAGE
// || type == ENUM || type == SERVICE; }
#define CONSTRUCTOR(TYPE, TYPE_CONSTANT, FIELD)                                \
  inline explicit Symbol(const TYPE *value) {                                  \
    type = TYPE_CONSTANT;                                                      \
    this->FIELD = value;                                                       \
  }

  Symbol(const class Descriptor *value) {
    type = MESSAGE;
    (this).descriptor = value;
  }

// CONSTRUCTOR(FieldDescriptor    , FIELD     , field_descriptor       )
#undef CONSTRUCTOR
};
