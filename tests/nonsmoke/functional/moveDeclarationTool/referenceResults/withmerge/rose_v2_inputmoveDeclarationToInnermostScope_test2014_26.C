namespace std {
template <typename _CharT> class basic_string;
typedef basic_string<char> string;
template <typename _CharT> class basic_string {
public:
  basic_string(const _CharT *__s);
  ~basic_string() {}
};

string grouping() { return ""; }

void foo() {
  if (1) {
    int x = 4;
  }
}
} // closing brace for namespace statement
