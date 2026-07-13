// A reserved-looking name is not frontend provenance.  This source-written
// declaration and definition must remain visible in the generated program.

int __builtin_rex_user_identity(int value) { return value; }

int rex_source_written_builtin_spelling(void) {
  return __builtin_rex_user_identity(17);
}
