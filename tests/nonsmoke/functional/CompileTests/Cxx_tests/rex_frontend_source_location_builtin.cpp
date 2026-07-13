const char *rex_source_file = __builtin_FILE();
const char *rex_source_file_name = __builtin_FILE_NAME();
const char *rex_source_function = __builtin_FUNCTION();
unsigned rex_source_line = __builtin_LINE();
unsigned rex_source_column = __builtin_COLUMN();

int main() {
  return rex_source_file[0] == '\0' || rex_source_file_name[0] == '\0' ||
         rex_source_function == nullptr || rex_source_line == 0 ||
         rex_source_column == 0;
}
