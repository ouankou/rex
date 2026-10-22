int shared;
extern int shared;

int rex_symbol_identity(int parameter) {
  int value = parameter;
  {
    int value = 3;
    shared += value;
  }
  return value + shared + __func__[0] + __FUNCTION__[0] +
         __PRETTY_FUNCTION__[0] + __FUNCSIG__[0];
}
