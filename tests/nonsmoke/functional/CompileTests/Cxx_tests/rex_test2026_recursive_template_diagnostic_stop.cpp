template <class T, class U> void rex_recursive_template(T x, U y) {
  rex_recursive_template(&y, &x);
}

int main() { rex_recursive_template(3, 4); }
