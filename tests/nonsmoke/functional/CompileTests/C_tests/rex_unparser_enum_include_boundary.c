enum RexExternalEnumeratorBoundary {
  rex_enum_direct_first = 1,
#define REX_EXTERNAL_ENUMERATOR(name) name,
#include "rex_unparser_enum_include_entries.def"
  rex_enum_direct_last
};

enum RexTerminalExternalEnumeratorBoundary {
  rex_enum_terminal_direct = 4,
#define REX_TERMINAL_EXTERNAL_ENUMERATOR(name) name,
#include "rex_unparser_enum_terminal_include_entries.def"
};

int main(void) {
  enum RexExternalEnumeratorBoundary value = rex_enum_direct_last;
  return value != rex_enum_direct_last;
}
