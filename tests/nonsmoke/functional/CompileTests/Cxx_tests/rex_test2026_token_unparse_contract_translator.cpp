#include "sage3basic.h"

#include "utility_functions.h"

#include <map>

namespace {
SgSourceFile *findMainFile(SgProject *project) {
  for (SgFile *file : project->get_fileList()) {
    SgSourceFile *source = isSgSourceFile(file);
    if (source == NULL) {
      continue;
    }
    if (!source->get_isHeaderFile()) {
      return source;
    }
  }

  if (!project->get_fileList().empty()) {
    return isSgSourceFile(project->get_fileList().front());
  }

  return NULL;
}
} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != NULL);

  SgSourceFile *source_file = findMainFile(project);
  ROSE_ASSERT(source_file != NULL);

  source_file->set_unparse_tokens(true);
  std::map<SgNode *, TokenStreamSequenceToNodeMapping *> &token_map =
      source_file->get_tokenSubsequenceMap();
  token_map.clear();

  return backend(project);
}
