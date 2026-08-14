#ifndef REX_FRONTEND_LAZY_BOUNDED_SYSTEM_HEADER_FIELDS_HPP
#define REX_FRONTEND_LAZY_BOUNDED_SYSTEM_HEADER_FIELDS_HPP

#pragma GCC system_header

template <typename T> struct RexFrontendLazyBoundedSystemHeaderFields {
  struct Entry {
    T value;
  };

  Entry entry;
};

#endif
