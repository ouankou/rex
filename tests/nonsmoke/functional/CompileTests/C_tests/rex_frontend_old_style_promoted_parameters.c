/* Old-style definitions with no earlier declaration: each function's callable
   type takes the promoted parameter types (int, double, unsigned int) while
   the parameter objects keep their declared types. */
int rex_old_style_scalars(c, f)
char c;
float f;
{
  return c + (int)f;
}

int rex_old_style_enumeration(e) enum rex_kind { rex_first, rex_second } e;
{
  return (int)e;
}

int main(void) {
  return rex_old_style_scalars(1, 2.0) + rex_old_style_enumeration(1) == 4 ? 0
                                                                           : 1;
}
