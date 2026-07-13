template <typename T> struct rex_using_template_base {
  template <typename U> void invoke(U) {}
};

struct rex_using_template_derived : rex_using_template_base<char> {
  using rex_using_template_base<char>::invoke;
};

struct rex_using_hidden_base {
  void hidden(int) {}
};

struct rex_using_hidden_derived : rex_using_hidden_base {
  using rex_using_hidden_base::hidden;
  void hidden(int) {}
};

template <typename T> struct rex_using_dependent_base {
  struct nested {};
};

template <typename T>
struct rex_using_dependent_derived : rex_using_dependent_base<T> {
  using rex_using_dependent_base<T>::nested;
};

int main() {
  rex_using_template_derived template_derived;
  template_derived.invoke(1);

  rex_using_hidden_derived hidden_derived;
  hidden_derived.hidden(1);
}
