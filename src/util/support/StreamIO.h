#ifndef STREAM_IO_H
#define STREAM_IO_H

#include <assert.h>

#include <iostream>

#include <string>

inline bool is_space(char c) { return c == ' ' || c == '\t' || c == '\n'; }

inline bool is_num(char c) { return c >= '0' && c <= '9'; }

inline bool is_id(char c) {
  return c == '_' || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

inline char getch(std::istream &in) {
  char c = in.get();
  return c;
}

inline char read_ch(std::istream &in) {
  char c = 0;
  do {
    c = getch(in);
  } while (in.good() && is_space(c));
  if (!in.good())
    return 0;
  return c;
}

inline void read_ch(std::istream &in, char c) {
  char c1 = read_ch(in);
  if (c1 != c) {
    std::cerr << "read char error: expecting '" << c << "' instead of '" << c1
              << "'\n";
    ROSE_ABORT();
  }
}

inline std::string read_id(std::istream &in) {
  char c = read_ch(in);
  std::string buf = "";
  while (in.good() && (is_id(c) || is_num(c))) {
    buf.push_back(c);
    c = getch(in);
  }
  if (in.good())
    in.unget();
  return buf;
}

inline void read_id(std::istream &in, const std::string &id) {
  std::string id1 = read_id(in);
  if (id1 != id) {
    std::cerr << "read id error: expecting '" << id << "' instead of '" << id1
              << "'\n";
    ROSE_ABORT();
  }
}

inline std::string read_num(std::istream &in) {
  char c = read_ch(in);
  if (!in.good())
    return "";
  assert(is_num(c));
  std::string buf = "";
  buf.push_back(c);
  for (;;) {
    c = getch(in);
    if (!is_num(c) && c != '.') {
      in.unget();
      break;
    }
    buf.push_back(c);
  }
  return buf;
}

template <class ItemGen> void read_list(std::istream &in, ItemGen &op) {
  op.read(in);
  while (in.good()) {
    char c = read_ch(in);
    if (c != ',')
      break;
    op.read(in);
  }
  if (in.good())
    in.unget();
}

template <class ItemGen, class Container> class ReadContainerWrap {
  ItemGen &op;
  Container &c;

public:
  ReadContainerWrap(ItemGen &_op, Container &_c) : op(_op), c(_c) {}
  void read(std::istream &in) { c.push_back(op.read(in)); }
};

#endif
