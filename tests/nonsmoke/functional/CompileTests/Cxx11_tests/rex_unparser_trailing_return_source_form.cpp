int rex_trailing_return_global = 7;

decltype(rex_trailing_return_global) rex_prefix_decltype_return(int value) {
  return value;
}

auto rex_trailing_decltype_return(int value) -> decltype(value) {
  return value;
}
