#ifndef REX_FRONTEND_CLASS_SOURCE_RANGE_HPP
#define REX_FRONTEND_CLASS_SOURCE_RANGE_HPP

struct rex_range_header {
  int value;
};

struct rex_range_header_forward;
extern rex_range_header_forward *
rex_range_header_identity(rex_range_header_forward *);
struct rex_range_header_forward {
  int value;
};

#endif
