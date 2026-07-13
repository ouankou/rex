#include "rose.h"

#include <map>
#include <set>
#include <string>

namespace {
constexpr const char *headerName =
    "rex_frontend_preprocessing_only_header_owner.h";

bool belongsToHeader(const PreprocessingInfo *record) {
  if (record == nullptr || record->get_file_info() == nullptr) {
    return false;
  }
  const std::string filename = record->get_file_info()->get_filenameString();
  return filename.size() >= std::char_traits<char>::length(headerName) &&
         filename.compare(
             filename.size() - std::char_traits<char>::length(headerName),
             std::char_traits<char>::length(headerName), headerName) == 0;
}

SgSourceFile *mainSourceFile(SgProject *project) {
  for (SgFile *file : project->get_fileList()) {
    if (SgSourceFile *sourceFile = isSgSourceFile(file)) {
      if (!sourceFile->get_isHeaderFile()) {
        return sourceFile;
      }
    }
  }
  return nullptr;
}
} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(frontendExitStatus(project) == 0);

  SgSourceFile *sourceFile = mainSourceFile(project);
  ROSE_ASSERT(sourceFile != nullptr);
  SgGlobal *global = sourceFile->get_globalScope();
  ROSE_ASSERT(global != nullptr);

  for (SgDeclarationStatement *declaration : global->get_declarations()) {
    if (SgEmptyDeclaration *empty = isSgEmptyDeclaration(declaration)) {
      Sg_File_Info *fileInfo = empty->get_file_info();
      ROSE_ASSERT(fileInfo == nullptr || fileInfo->get_filenameString().find(
                                             headerName) == std::string::npos);
    }
  }

  std::map<PreprocessingInfo *, size_t> ownerCount;
  std::set<std::string> exactHeaderDirectives;
  for (SgNode *node : NodeQuery::querySubTree(project, V_SgLocatedNode)) {
    SgLocatedNode *located = isSgLocatedNode(node);
    ROSE_ASSERT(located != nullptr);
    AttachedPreprocessingInfoType *records =
        located->getAttachedPreprocessingInfo();
    if (records == nullptr) {
      continue;
    }
    for (PreprocessingInfo *record : *records) {
      ROSE_ASSERT(record != nullptr);
      if (!belongsToHeader(record)) {
        continue;
      }
      ++ownerCount[record];
      ROSE_ASSERT(located == global);
      ROSE_ASSERT(record->getRelativePosition() == PreprocessingInfo::before);
      exactHeaderDirectives.insert(record->getString());
    }
  }

  ROSE_ASSERT(!ownerCount.empty());
  for (const auto &[record, count] : ownerCount) {
    ROSE_ASSERT(record != nullptr);
    ROSE_ASSERT(count == 1);
  }
  ROSE_ASSERT(exactHeaderDirectives.count(
                  "#ifndef REX_FRONTEND_PREPROCESSING_ONLY_HEADER_OWNER_H\n") ==
              1);
  ROSE_ASSERT(exactHeaderDirectives.count(
                  "#define REX_FRONTEND_PREPROCESSING_ONLY_HEADER_OWNER_H\n") ==
              1);
  ROSE_ASSERT(exactHeaderDirectives.count(
                  "#define REX_FRONTEND_PREPROCESSING_ONLY_VALUE 7\n") == 1);
  ROSE_ASSERT(exactHeaderDirectives.count("#endif\n") == 1);

  return 0;
}
