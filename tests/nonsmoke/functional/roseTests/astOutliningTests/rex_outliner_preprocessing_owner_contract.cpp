#include "RoseAst.h"
#include "rose.h"

#include "PreprocessingInfo.hh"

#include <string>

namespace {

struct RecordOwner {
  SgLocatedNode *node = nullptr;
  PreprocessingInfo *record = nullptr;
};

RecordOwner findOwnedRecord(SgNode *root, const std::string &marker) {
  RecordOwner result;
  size_t matches = 0;
  RoseAst ast(root);
  for (RoseAst::iterator current = ast.begin(); current != ast.end();
       ++current) {
    SgLocatedNode *owner = isSgLocatedNode(*current);
    if (owner == nullptr || owner->getAttachedPreprocessingInfo() == nullptr) {
      continue;
    }
    for (PreprocessingInfo *record : *owner->getAttachedPreprocessingInfo()) {
      if (record != nullptr &&
          record->getString().find(marker) != std::string::npos) {
        ++matches;
        result = {owner, record};
      }
    }
  }
  if (matches != 1) {
    fprintf(stderr,
            "REX_TEST_ERROR: expected exactly one generated preprocessing "
            "record for marker %s, found %zu\n",
            marker.c_str(), matches);
    ROSE_ABORT();
  }
  return result;
}

void requireExactPhysicalOwner(const RecordOwner &owned,
                               SgSourceFile *sourceFile) {
  ROSE_ASSERT(owned.node != nullptr);
  ROSE_ASSERT(owned.record != nullptr);
  ROSE_ASSERT(sourceFile != nullptr);
  Sg_File_Info *ownerInfo = owned.node->get_file_info();
  Sg_File_Info *recordInfo = owned.record->get_file_info();
  if (SageInterface::getEnclosingSourceFile(owned.node, true) != sourceFile ||
      ownerInfo == nullptr || recordInfo == nullptr || ownerInfo->isShared() ||
      recordInfo->isShared() || ownerInfo->get_physical_file_id() < 0 ||
      recordInfo->get_physical_file_id() != ownerInfo->get_physical_file_id() ||
      recordInfo->get_physical_filename() !=
          Sg_File_Info::getFilenameFromID(ownerInfo->get_physical_file_id()) ||
      !owned.record->isTransformation() || recordInfo->isTransformation() ||
      recordInfo->isCompilerGenerated() ||
      !recordInfo->isOutputInCodeGeneration()) {
    fprintf(stderr,
            "REX_TEST_ERROR: generated outliner preprocessing has no exact "
            "physical owner\n");
    ROSE_ABORT();
  }
}

} // namespace

int main(int argc, char **argv) {
  if (argc == 2) {
    const std::string mode = argv[1];
    if (mode == "null-comment-owner") {
      ASTtools::attachComment("comment with no owner", nullptr);
    } else if (mode == "null-comment-spelling") {
      ASTtools::attachComment(static_cast<const char *>(nullptr), nullptr);
    } else if (mode == "null-header-project") {
      ASTtools::insertHeader("rex_header.hpp", nullptr);
    } else if (mode == "empty-header-filename") {
      ASTtools::insertHeader("", nullptr);
    } else {
      return 2;
    }
    return 3;
  }
  if (argc != 1) {
    return 2;
  }

  constexpr const char *filename =
      "rex_outliner_preprocessing_owner_contract.cpp";
  SgSourceFile *sourceFile = SageBuilder::buildGeneratedSourceFile(filename);
  ROSE_ASSERT(sourceFile != nullptr);
  sourceFile->set_Cxx_only(true);
  sourceFile->set_outputLanguage(SgFile::e_Cxx_language);
  SgProject *project = sourceFile->get_project();
  SgGlobal *global = sourceFile->get_globalScope();
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(global != nullptr);

  ASTtools::attachComment("rex exact outliner comment", global);
  requireExactPhysicalOwner(
      findOwnedRecord(project, "rex exact outliner comment"), sourceFile);

  ASTtools::insertHeader("rex_outliner_exact_owner.hpp", project);
  requireExactPhysicalOwner(
      findOwnedRecord(project, "rex_outliner_exact_owner.hpp"), sourceFile);
  return 0;
}
