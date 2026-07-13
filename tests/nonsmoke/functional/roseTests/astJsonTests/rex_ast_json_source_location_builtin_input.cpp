const char *rex_json_file = __builtin_FILE();
const char *rex_json_function = __builtin_FUNCTION();
unsigned rex_json_line = __builtin_LINE();

int main() {
  return rex_json_file[0] == '\0' || rex_json_function == nullptr ||
         rex_json_line == 0;
}
