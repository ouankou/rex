

#include "AnnotIO.h"

#include "CommandOptions.h"

#include <string.h>
using namespace std;

namespace {
struct Lookahead {
  string buffer;
  size_t index = 0;
};

void releaseLookahead(ios_base::event event, ios_base &stream, int index) {
  void *&value = stream.pword(index);
  if (event == ios_base::erase_event) {
    delete static_cast<Lookahead *>(value);
    value = nullptr;
  } else if (event == ios_base::copyfmt_event) {
    value = new Lookahead;
  }
}

Lookahead &lookahead(istream &in) {
  static const int index = ios_base::xalloc();
  void *&value = in.pword(index);
  if (!value) {
    in.register_callback(releaseLookahead, index);
    value = new Lookahead;
  }
  return *static_cast<Lookahead *>(value);
}
} // namespace

bool DebugAnnotRead() {
  static int debugread = 0;
  if (debugread == 0) {
    if (CmdOptions::GetInstance()->HasOption("-debugannotread"))
      debugread = 1;
    else
      debugread = -1;
  }
  return debugread == 1;
}

bool DebugAnnot() {
  static int debug = 0;
  if (debug == 0) {
    if (CmdOptions::GetInstance()->HasOption("-debugannot"))
      debug = 1;
    else
      debug = -1;
  }
  return debug == 1;
}

bool is_space(char c) { return c == ' ' || c == '\t' || c == '\n'; }

//! Check if a character is a numerical character: 0 to 9
bool is_num(char c) { return c >= '0' && c <= '9'; }

//! Check if a character is part of an indentifier: _ or a to z or A to Z
bool is_id(char c) {
  return c == '_' || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

int is_operator(char c) {
  string op = "<>+-*/&.$";
  const char *p = strchr(op.c_str(), c);
  if (p == 0)
    return 0;
  return p - op.c_str() + 1;
}

char getch(istream &in) {
  Lookahead &state = lookahead(in);
  if (state.index < state.buffer.size()) {
    char c = state.buffer[state.index++];
    if (state.index == state.buffer.size()) {
      state.buffer.clear();
      state.index = 0;
    }
    return c;
  }
  int c = in.get();
  if (c == char_traits<char>::eof())
    return 0;
  if (DebugAnnotRead())
    cerr << static_cast<char>(c);
  return static_cast<char>(c);
}

char peek_ch(istream &in) {
  char c = peek_next(in);
  while (is_space(c)) {
    getch(in);
    c = peek_next(in);
  }
  return c;
}
char peek_next(istream &in) {
  Lookahead &state = lookahead(in);
  if (state.index < state.buffer.size())
    return state.buffer[state.index];
  int c = in.peek();
  return c == char_traits<char>::eof() ? 0 : static_cast<char>(c);
}
string peek_next(istream &in, unsigned size) {
  Lookahead &state = lookahead(in);
  while (state.buffer.size() - state.index < size) {
    int c = in.get();
    if (c == char_traits<char>::eof())
      break;
    state.buffer.push_back(static_cast<char>(c));
    if (DebugAnnotRead())
      cerr << static_cast<char>(c);
  }
  return state.buffer.substr(state.index, size);
}
// read a non-space character from the input stream
char read_ch(istream &in) {
  char c = 0;
  do {
    c = getch(in);
  } while (is_space(c));
  return c;
}
// Expect to read a char c from the in stream.
void read_ch(istream &in, char c) {
  char c1 = read_ch(in);
  if (c1 != c) {
    ReadError m("read char error: expecting '");
    m.msg.push_back(c);
    m.msg = m.msg + "' instead of '";
    m.msg.push_back(c1);
    m.msg.push_back('\'');
    throw m;
  }
}
//! Grab the next identifier from an input stream
string read_id(istream &in) {
  char c = peek_ch(in);
  string buf = "";
  while (is_id(c) || is_num(c)) {
    c = getch(in);
    buf.push_back(c);
    c = peek_next(in);
  }
  return buf;
}
//! Grab the identifier s from an input stream 'in'
void read_id(istream &in, const string &s) {
  string r = read_id(in);
  if (r != s) {
    throw ReadError("read identifier error: expecting '" + s +
                    "' instead of '" + r + "'");
  }
}

string peek_id(istream &in) {
  Lookahead &state = lookahead(in);
  if (state.buffer.empty()) {
    state.buffer = read_id(in);
    state.index = 0;
  }
  return state.buffer.substr(state.index);
}

string read_num(istream &in) {
  char c = read_ch(in);
  if (c == 0)
    throw ReadError("annotation number ended unexpectedly");
  if (!is_num(c)) {
    throw ReadError("read number error: expecting numerics instead of " +
                    string(1, c));
  }
  string buf = "";
  buf.push_back(c);
  bool saw_decimal_point = false;
  for (;;) {
    c = peek_next(in);
    if (!is_num(c) && c != '.')
      break;
    if (c == '.') {
      if (saw_decimal_point)
        throw ReadError("annotation number has multiple decimal points");
      saw_decimal_point = true;
    }
    getch(in);
    buf.push_back(c);
  }
  return buf;
}

string read_operator(istream &in) {
  char c = peek_ch(in);
  string buf = "";
  while (is_operator(c)) {
    read_ch(in, c);
    buf.push_back(c);
    c = peek_next(in);
  }
  return buf;
}
