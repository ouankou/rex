#include "rose.h"

#include <string>

int main(int argc, char *argv[]) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);

  SgSourceFile *source_file = isSgSourceFile(project->get_fileList()[0]);
  ROSE_ASSERT(source_file != nullptr);

  SgUnparse_Info info;
  info.unset_SkipComments();
  info.unset_SkipCPPDirectives();
  info.set_current_source_file(source_file);

  std::string unparsed = globalUnparseToString(source_file, &info);

  ROSE_ASSERT(unparsed.find("#include") != std::string::npos);
  ROSE_ASSERT(unparsed.find("vector") != std::string::npos);
  ROSE_ASSERT(unparsed.find("#define REX_MAGIC") != std::string::npos);
  ROSE_ASSERT(unparsed.find("REX comment") != std::string::npos);

  return 0;
}
