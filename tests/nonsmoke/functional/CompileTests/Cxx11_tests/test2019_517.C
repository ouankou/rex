#include <cstddef>

template <typename CharT, CharT... chars> constexpr int operator"" _tmpl() {
  return sizeof...(chars);
}

int operator"" _raw(const char *, std::size_t);
int operator"" _raw(const char16_t *, std::size_t);
int operator"" _raw(const char32_t *, std::size_t);

void f() {
  int a = R"(simple raw string)"_raw;
  int b = R"tag(raw "quotes" and \\ slash
next line)tag"_raw;
  int c = uR"wide(raw "u16" and \\ slash
next line)wide"_raw;
  int d = UR"utf32(raw "u32" and \\ slash)utf32"_raw;

  int e = "line\nbreak"_tmpl;
  int f = "quote\" slash\\ tab\t"_tmpl;
  int g = u"wide\ntext"_tmpl;
  int h = U"utf32\ntext"_tmpl;

  (void)a;
  (void)b;
  (void)c;
  (void)d;
  (void)e;
  (void)f;
  (void)g;
  (void)h;
}
