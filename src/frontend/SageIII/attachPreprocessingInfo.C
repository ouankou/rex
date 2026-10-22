#include "attachPreprocessingInfo.h"

#include "sage3basic.h"

#include "rose_config.h"

using namespace std;

namespace {
void rosePhaseTrace(const char *phase) {
  if (getenv("ROSE_PHASE_TRACE") != nullptr) {
    fprintf(stderr, "ROSE_PHASE %s\n", phase);
    fflush(stderr);
  }
}

static SgSourceFile *
getPreprocessingAttachmentTraversalRoot(SgSourceFile *source_file) {
  if (source_file->get_isHeaderFile() == false) {
    return source_file;
  }

  SgIncludeFile *include_file = source_file->get_associated_include_file();
  SgSourceFile *translation_unit =
      include_file != nullptr
          ? include_file->get_source_file_of_translation_unit()
          : nullptr;
  if (include_file == nullptr ||
      include_file->get_source_file() != source_file ||
      translation_unit == nullptr) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[header-preprocessing-owner]: header=%s "
            "has no exact include-file and translation-unit owner\n",
            source_file->getFileName().c_str());
    ROSE_ABORT();
  }
  return translation_unit;
}

} // namespace

void attachPreprocessingInfo(SgSourceFile *sageFilePtr,
                             const std::string &new_filename,
                             bool attach_to_ast) {
  ROSE_ASSERT(sageFilePtr != NULL);
  Sg_File_Info *sourceInfo = sageFilePtr->get_file_info();
  ROSE_ASSERT(sourceInfo != NULL);
  ROSE_ASSERT(sourceInfo->get_physical_file_id() >= 0);
  ROSE_ASSERT(!sageFilePtr->get_processedToIncludeCppDirectivesAndComments());
  ROSEAttributesListContainerPtr filePreprocInfo =
      sageFilePtr->get_preprocessorDirectivesAndCommentsList();
  ROSE_ASSERT(filePreprocInfo != NULL);

  TimingPerformance timer_1("AST attachPreprocessingInfo:");

  string filename = sageFilePtr->get_sourceFileNameWithPath();
  ROSEAttributesList *commentAndCppDirectiveList = NULL;

  rosePhaseTrace("attachPreprocessingInfo.buildList.begin");
  commentAndCppDirectiveList =
      AttachPreprocessingInfoTreeTrav::buildCommentAndCppDirectiveList(
          sageFilePtr, filename, new_filename);
  rosePhaseTrace("attachPreprocessingInfo.buildList.end");

  ROSE_ASSERT(commentAndCppDirectiveList != NULL);

  const std::string attributes_filename =
      new_filename.empty() ? filename : new_filename;
  filePreprocInfo->addList(attributes_filename, commentAndCppDirectiveList);

  // We should at least have the current files CPP/Comment/Token information
  // (even if it is an empty file).
  ROSE_ASSERT(!filePreprocInfo->getList().empty());

  TimingPerformance timer_2("AST Comment and CPP Directive Processing:");

  if (attach_to_ast) {
    AttachPreprocessingInfoTreeTraversalInheritedAttrribute inh;

    AttachPreprocessingInfoTreeTrav tt(sageFilePtr, commentAndCppDirectiveList);
    SgSourceFile *attachment_traversal_root =
        getPreprocessingAttachmentTraversalRoot(sageFilePtr);

    rosePhaseTrace("attachPreprocessingInfo.attachTraversal.begin");
    tt.traverse(attachment_traversal_root, inh);
    rosePhaseTrace("attachPreprocessingInfo.attachTraversal.end");
  }

  sageFilePtr->set_processedToIncludeCppDirectivesAndComments(true);
}
