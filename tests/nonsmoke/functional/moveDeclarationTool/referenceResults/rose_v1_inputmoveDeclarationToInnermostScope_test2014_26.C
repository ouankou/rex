namespace std {
template <typename _CharT> class basic_string;
typedef class basic_string<char> string;
template <typename _CharT> class basic_string {
public:
  ~basic_string() {}
  basic_string(const _CharT *__s);
};
;

string grouping() { return basic_string<char>(""); }

void foo() {
  if (1) {
    int x;
    x = 4;
  }
}
} // namespace std
// closing brace for namespace statement
