
namespace std {
template <typename _CharT> class basic_string;
typedef basic_string<char> string;

template <typename _CharT> class basic_string {
public:
  basic_string(const _CharT *__s);
  ~basic_string() {}
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
