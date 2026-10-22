#include "rose.h"

#include <algorithm>
#include <memory>

int main() {
  auto *file = SageBuilder::buildGeneratedSourceFile(
      "rex_preprocessing_destruction.cpp");
  std::unique_ptr<SgProject> project(file->get_project());
  auto *original = file->get_globalScope();
  SageInterface::attachComment(original, "retained analysis comment",
                               PreprocessingInfo::CplusplusStyleComment);
  original->validateAttachedPreprocessingInfoOwnership();
  std::unique_ptr<SgProject> copiedProject(
      SageInterface::deepCopy(project.get()));
  auto *copiedFile = isSgSourceFile(&copiedProject->get_file(0));
  ROSE_ASSERT(copiedFile != nullptr);
  auto *copy = copiedFile->get_globalScope();
  ROSE_ASSERT(copy != nullptr && copy != original);
  auto *originalRecords = original->getAttachedPreprocessingInfo();
  auto *copiedRecords = copy->getAttachedPreprocessingInfo();
  ROSE_ASSERT(originalRecords != nullptr && copiedRecords != nullptr &&
              originalRecords != copiedRecords);
  ROSE_ASSERT(originalRecords->size() == 1 && copiedRecords->size() == 1);
  ROSE_ASSERT(originalRecords->front() != copiedRecords->front());
  auto *record = originalRecords->front();
  ROSE_ASSERT(record->get_file_info()->get_parent() == original);
  ROSE_ASSERT(copiedRecords->front()->get_file_info()->get_parent() == copy);
  {
    std::unique_ptr<PreprocessingInfo> detachedCopy(
        new PreprocessingInfo(*record));
    ROSE_ASSERT(detachedCopy->getAttachedOwner() == nullptr);
    ROSE_ASSERT(detachedCopy->get_file_info()->get_parent() == nullptr);
    ROSE_ASSERT(detachedCopy->get_file_info() != record->get_file_info());
  }
  const auto position = record->getRelativePosition();
  ROSE_ASSERT(original->detachPreprocessingInfo(record) == record);
  ROSE_ASSERT(record->getAttachedOwner() == nullptr);
  ROSE_ASSERT(record->get_file_info()->get_parent() == nullptr);
  original->attachPreprocessingInfo(
      record, position, SgLocatedNode::PreprocessingInfoInsertion::back);
  originalRecords = original->getAttachedPreprocessingInfo();
  ROSE_ASSERT(record->getAttachedOwner() == original);
  ROSE_ASSERT(record->get_file_info()->get_parent() == original);
  {
    std::unique_ptr<Sg_File_Info> previous(record->get_file_info());
    std::unique_ptr<Sg_File_Info> replacement(
        Sg_File_Info::generateDefaultFileInfoForTransformationNode());
    replacement->setCommentOrDirective();
    ROSE_ASSERT(replacement->get_parent() == nullptr);
    record->set_file_info(replacement.release());
    ROSE_ASSERT(previous->get_parent() == nullptr);
    ROSE_ASSERT(record->get_file_info()->get_parent() == original);
    original->validateAttachedPreprocessingInfoOwnership();
  }
  const std::string originalText = originalRecords->front()->getString();
  copiedProject.reset();
  original->validateAttachedPreprocessingInfoOwnership();
  ROSE_ASSERT(originalRecords->front()->getString() == originalText);
  auto relative = std::make_unique<PreprocessingInfo>(*record);
  auto *inserted = relative.get();
  original->attachPreprocessingInfoRelative(inserted, PreprocessingInfo::after,
                                            record, true);
  relative.release();
  ROSE_ASSERT(originalRecords->size() == 2 &&
              originalRecords->back() == inserted);
  ROSE_ASSERT(inserted->getAttachedOwner() == original &&
              inserted->get_file_info()->get_parent() == original);
  ROSE_ASSERT(inserted->getRelativePosition() == PreprocessingInfo::after);
  original->validateAttachedPreprocessingInfoOwnership();
  original->clearAttachedPreprocessingInfo();
  ROSE_ASSERT(original->getAttachedPreprocessingInfo() == nullptr);

  auto *function = SageBuilder::buildDefiningFunctionDeclaration(
      SageBuilder::function_declaration_ownership::sourceLexical(),
      "rex_directive_owner", SageBuilder::buildVoidType(),
      SageBuilder::buildFunctionParameterList(), original);
  auto *block = function->get_definition()->get_body();
  auto *child = SageBuilder::buildBasicBlock();
  SageInterface::appendStatement(child, block);
  auto attach = [](SgLocatedNode *owner, PreprocessingInfo::DirectiveType kind,
                   const char *text) {
    auto *info = new PreprocessingInfo(kind, text, "rex_directives.cpp", 1, 1,
                                       1, PreprocessingInfo::before);
    owner->attachPreprocessingInfo(
        info, PreprocessingInfo::before,
        SgLocatedNode::PreprocessingInfoInsertion::back);
    return info;
  };
  auto *outer = attach(block, PreprocessingInfo::CpreprocessorIfDeclaration,
                       "#if OUTER\n");
  auto *end = attach(child, PreprocessingInfo::CpreprocessorEndifDeclaration,
                     "#endif\n");
  auto *balanced = attach(child, PreprocessingInfo::CpreprocessorIfDeclaration,
                          "#if BALANCED\n");
  auto *balancedEnd = attach(
      child, PreprocessingInfo::CpreprocessorEndifDeclaration, "#endif\n");
  auto *begin = attach(child, PreprocessingInfo::CpreprocessorIfDeclaration,
                       "#if INNER\n");
  auto *middle = attach(child, PreprocessingInfo::CpreprocessorElifDeclaration,
                        "#elif OTHER\n");
  auto *otherwise =
      attach(child, PreprocessingInfo::CpreprocessorElseDeclaration, "#else\n");
  std::vector<PreprocessingInfo *> ordered;
  SageInterface::preOrderCollectPreprocessingInfo(block, ordered, 0);
  ROSE_ASSERT(ordered == (std::vector<PreprocessingInfo *>{outer, end, balanced,
                                                           balancedEnd, begin,
                                                           middle, otherwise}));
  ROSE_ASSERT(SageInterface::moveUpInnerDanglingIfEndifDirective(block) == 4);
  ROSE_ASSERT(
      *block->getAttachedPreprocessingInfo() ==
      (AttachedPreprocessingInfoType{outer, end, begin, middle, otherwise}));
  ROSE_ASSERT(*child->getAttachedPreprocessingInfo() ==
              (AttachedPreprocessingInfoType{balanced, balancedEnd}));
  block->validateAttachedPreprocessingInfoOwnership();
  child->validateAttachedPreprocessingInfoOwnership();
  for (PreprocessingInfo *info : {end, begin, middle, otherwise}) {
    ROSE_ASSERT(info->getRelativePosition() == PreprocessingInfo::after);
    ROSE_ASSERT(info->getOutputPlacement() ==
                PreprocessingInfo::attached_output_boundary);
  }
  ROSE_ASSERT(SageInterface::moveUpInnerDanglingIfEndifDirective(block) == 0);
  std::vector<PreprocessingInfo *> transferred;
  for (int index = 0; index != 64; ++index)
    transferred.push_back(attach(
        child, PreprocessingInfo::CplusplusStyleComment, "// rex_transfer\n"));
  size_t capacityChanges = 0;
  auto capacity = block->getAttachedPreprocessingInfo()->capacity();
  for (auto *info : transferred) {
    child->transferPreprocessingInfo(info, block, PreprocessingInfo::after);
    ROSE_ASSERT(info->getAttachedOwner() == block);
    ROSE_ASSERT(info->get_file_info()->get_parent() == block);
    ROSE_ASSERT(info->getRelativePosition() == PreprocessingInfo::after);
    const auto currentCapacity =
        block->getAttachedPreprocessingInfo()->capacity();
    if (currentCapacity != capacity) {
      ++capacityChanges;
      capacity = currentCapacity;
    }
  }
  ROSE_ASSERT(capacityChanges <= 7);
  ROSE_ASSERT(std::equal(transferred.begin(), transferred.end(),
                         block->getAttachedPreprocessingInfo()->end() -
                             transferred.size()));
  block->validateAttachedPreprocessingInfoOwnership();
  child->validateAttachedPreprocessingInfoOwnership();
  return 0;
}
