int rex_gnu_binary_source();

int rex_frontend_gnu_binary_conditional() {
  return rex_gnu_binary_source() ?: 17;
}
