template <typename Character> struct basic_regex {
  Character value;
};

int main() {
  basic_regex<char> expression{'a'};
  return expression.value == 'a' ? 0 : 1;
}
