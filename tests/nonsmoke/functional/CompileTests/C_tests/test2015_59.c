typedef struct {
  void *p;
} __guest_handle_void;
typedef struct {
  const void *p;
} __guest_handle_const_void;

struct mmuext_op {
    unsigned int cmd;
    union {
        unsigned long linear_addr;
    } arg1;
    union {
        unsigned int nr_ents;
        __guest_handle_const_void vcpumask;
    } arg2;
};


