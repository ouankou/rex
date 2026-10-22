#include "rose_config.h"

#include "Rose/StringUtility/FileUtility.h"

#include "Rose/StringUtility.h"

#include <utility>

using namespace std;
using namespace Rose;

int main() {
  const pair<string, string> filenames[] = {
      {"./.", "./."},
      {"logical/../source.cpp", "logical/../source.cpp"},
      {"", ""},
      {"quote\"name.cpp", "quote\\\"name.cpp"},
      {"back\\slash.cpp", "back\\\\slash.cpp"},
      {"line\nname.cpp", "line\\nname.cpp"},
      {"?"
       "?/name.cpp",
       "\\?\\?/name.cpp"},
#ifdef SKIP_HASH_LINE_NUMBER_DECLARATIONS_IN_GENERATED_FILES
      {"end*/name.cpp", "end*\\057name.cpp"},
      {"begin/*name.cpp", "begin\\057*name.cpp"},
      {"edge/*/name.cpp", "edge\\057*\\057name.cpp"},
#else
      {"end*/name.cpp", "end*/name.cpp"},
      {"begin/*name.cpp", "begin/*name.cpp"},
      {"edge/*/name.cpp", "edge/*/name.cpp"},
#endif
  };
  for (const auto &[filename, escaped] : filenames) {
    StringUtility::StringWithLineNumber line("int value;", filename, 37);
#ifdef SKIP_HASH_LINE_NUMBER_DECLARATIONS_IN_GENERATED_FILES
    const string directive = "/* #line 37 \"" + escaped + "\" */\n";
#else
    const string directive = "#line 37 \"" + escaped + "\"\n";
#endif
    ROSE_ASSERT(line.toString() == directive + "int value;\n");
    if (!filename.empty())
      ROSE_ASSERT(StringUtility::toString({line}) == line.toString());
  }
  const StringUtility::FileWithLineNumbers lines{
      {"one", "./.", 37}, {"two", "./.", 38}, {"generated", "", 1}};
  ROSE_ASSERT(
      StringUtility::toString(lines, "emitted.cpp", 1) ==
      StringUtility::StringWithLineNumber("one\ntwo", "./.", 37).toString() +
          StringUtility::StringWithLineNumber("generated", "emitted.cpp", 5)
              .toString());

  return 0;
}
