
struct XXX {
  union {
    struct {
      struct YYY {
        int data2;
      } vendor_guid;
    } set_variable;
    struct {
      struct YYY vendor_guid;
    } get_next_variable_name;
  } u;
};
