#include "astJson/sageAstJson.h"
#include "rose.h"
#include "tokenStreamMapping.h"
#include "utility_functions.h"

#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>

static std::string contents(const std::string &path) {
  std::ifstream input(path);
  ROSE_ASSERT(input.good());
  return {std::istreambuf_iterator<char>(input),
          std::istreambuf_iterator<char>()};
}

static void validateIncludeTreeCopy(SgSourceFile *original,
                                    SgSourceFile *copy) {
  auto *sourceRoot = original->get_associated_include_file();
  auto *copyRoot = copy->get_associated_include_file();
  ROSE_ASSERT((sourceRoot == nullptr) == (copyRoot == nullptr));
  if (sourceRoot == nullptr)
    return;
  ROSE_ASSERT(copyRoot != sourceRoot && copyRoot->get_parent() == copy);
  std::vector<std::pair<SgIncludeFile *, SgIncludeFile *>> pending{
      {sourceRoot, copyRoot}};
  while (!pending.empty()) {
    auto [source, cloned] = pending.back();
    pending.pop_back();
    source->validateIncludeOwnership();
    cloned->validateIncludeOwnership();
    ROSE_ASSERT(cloned != source &&
                cloned->get_filename() == source->get_filename());
    ROSE_ASSERT(cloned->get_isSystemInclude() == source->get_isSystemInclude());
    ROSE_ASSERT(cloned->get_isApplicationFile() ==
                source->get_isApplicationFile());
    ROSE_ASSERT(cloned->get_source_file_of_translation_unit() == copy);
    SgSourceFile *file = cloned->get_source_file();
    ROSE_ASSERT(file != source->get_source_file());
    if (cloned == copyRoot) {
      ROSE_ASSERT(file == copy);
    } else {
      ROSE_ASSERT(file->get_isHeaderFile() &&
                  file->get_associated_include_file() == cloned);
      ROSE_ASSERT(file->get_parent() == copy->get_frontendExternalFileList());
    }
    const auto &sourceChildren = source->get_include_file_list();
    const auto &copyChildren = cloned->get_include_file_list();
    ROSE_ASSERT(sourceChildren.size() == copyChildren.size());
    for (size_t index = 0; index < sourceChildren.size(); ++index)
      pending.emplace_back(sourceChildren[index], copyChildren[index]);
  }
}

int main(int argc, char **argv) {
  {
    auto file = std::make_unique<SgSourceFile>();
    SgSourceFile *identity = file.get();
    ROSE_ASSERT(file->get_tokenSubsequenceMap().empty());
    ROSE_ASSERT(Rose::tokenSubsequenceMapOfMapsBySourceFile.at(identity) ==
                &file->get_tokenSubsequenceMap());
    file.reset();
    ROSE_ASSERT(Rose::tokenSubsequenceMapOfMapsBySourceFile.count(identity) ==
                0);
  }

  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr && project->numberOfFiles() == 1);
  auto *original = isSgSourceFile(&project->get_file(0));
  ROSE_ASSERT(original != nullptr);
  SgProject *copyProject = SageInterface::deepCopySemantic(project);
  auto *copy = isSgSourceFile(&copyProject->get_file(0));
  ROSE_ASSERT(copy != nullptr && copy != original);
  validateIncludeTreeCopy(original, copy);
  const auto *sourceRecords =
      original->get_globalScope()->getAttachedPreprocessingInfo();
  const auto *copyRecords =
      copy->get_globalScope()->getAttachedPreprocessingInfo();
  ROSE_ASSERT((sourceRecords == nullptr) == (copyRecords == nullptr));
  if (sourceRecords != nullptr) {
    ROSE_ASSERT(sourceRecords->size() == copyRecords->size());
    for (size_t index = 0; index < sourceRecords->size(); ++index) {
      const auto *sourceRecord = (*sourceRecords)[index];
      const auto *copyRecord = (*copyRecords)[index];
      ROSE_ASSERT(copyRecord != sourceRecord);
      ROSE_ASSERT(copyRecord->getOutputPlacement() ==
                  sourceRecord->getOutputPlacement());
      ROSE_ASSERT(copyRecord->get_file_info()->get_physical_file_id() ==
                  sourceRecord->get_file_info()->get_physical_file_id());
    }
  }
  ROSE_ASSERT(copy->get_startOfConstruct() != original->get_startOfConstruct());
  ROSE_ASSERT(copy->get_startOfConstruct()->get_parent() == copy);
  ROSE_ASSERT(copy->get_preprocessorDirectivesAndCommentsList() !=
              original->get_preprocessorDirectivesAndCommentsList());
  ROSE_ASSERT(&copy->get_tokenSubsequenceMap() !=
              &original->get_tokenSubsequenceMap());
  ROSE_ASSERT(copy->get_tokenSubsequenceMap().size() ==
              original->get_tokenSubsequenceMap().size());
  ROSE_ASSERT(copy->get_token_list().size() ==
              original->get_token_list().size());
  const auto &originalIncludes =
      original->get_frontendResolvedIncludeDirectivesMap();
  const auto &copiedIncludes = copy->get_frontendResolvedIncludeDirectivesMap();
  ROSE_ASSERT(!originalIncludes.empty());
  ROSE_ASSERT(copiedIncludes.size() == originalIncludes.size());
  for (const auto &[path, originals] : originalIncludes) {
    const auto &copies = copiedIncludes.at(path);
    ROSE_ASSERT(copies.size() == originals.size());
    for (PreprocessingInfo *record : copies) {
      ROSE_ASSERT(record != nullptr && originals.count(record) == 0);
      SgLocatedNode *owner = record->getAttachedOwner();
      ROSE_ASSERT(owner != nullptr);
      owner->validateAttachedPreprocessingInfoOwnership();
      ROSE_ASSERT(SageInterface::getEnclosingSourceFile(owner) == copy);
    }
  }
  for (size_t i = 0; i < original->get_token_list().size(); ++i) {
    auto *token = copy->get_token_list()[i];
    ROSE_ASSERT(token != original->get_token_list()[i]);
    ROSE_ASSERT(token->get_parent() == copy);
    ROSE_ASSERT(token->get_classification_code() ==
                original->get_token_list()[i]->get_classification_code());
    ROSE_ASSERT(token->get_lexeme_string() ==
                original->get_token_list()[i]->get_lexeme_string());
  }
  if (original->get_unparse_tokens()) {
    ROSE_ASSERT(!original->get_token_list().empty());
    ROSE_ASSERT(!original->get_tokenSubsequenceMap().empty());
    for (const auto &entry : copy->get_tokenSubsequenceMap()) {
      ROSE_ASSERT(original->get_tokenSubsequenceMap().count(entry.first) == 0);
      ROSE_ASSERT(entry.second != nullptr);
      ROSE_ASSERT(
          entry.second
              ->halfOpenInterval(TokenStreamIntervalKind::token_subsequence)
              .end <= copy->get_token_list().size());
    }
  }
  AstTests::runAllTests(project);
  AstTests::runAllTests(copyProject);
  original->set_unparse_output_filename("original.c");
  original->unparse();
  const std::string expected = contents("original.c");
  copy->set_unparse_output_filename("copied.c");
  copy->unparse();
  ROSE_ASSERT(contents("copied.c") == expected);
  if (copy->get_unparse_tokens()) {
    copy->get_originalCommandLineArgumentList().push_back(
        "-rex:ast-json-checkpoint=pre-omp-construction");
    Rose::AstJson::Options options;
    options.outputDirectory = "copy-json";
    SgSourceFile *restored = Rose::AstJson::roundTripSourceFile(
        copy, Rose::AstJson::Checkpoint::PreOmpConstruction, options);
    ROSE_ASSERT(restored != nullptr && restored != copy);
    validateIncludeTreeCopy(original, restored);
    ROSE_ASSERT(restored->get_frontendResolvedIncludeDirectivesMap().size() ==
                originalIncludes.size());
    copy = restored;
    copy->set_unparse_output_filename("restored.c");
    copy->unparse();
    ROSE_ASSERT(contents("restored.c") == expected);
  }
  SageInterface::deleteAST(project);
  copy->set_unparse_output_filename("surviving.c");
  copy->unparse();
  ROSE_ASSERT(contents("surviving.c") == expected);
  // A later user edit must still select AST emission; construction must not
  // disable modification tracking after the copy transaction returns.
  SgIntVal *editable = nullptr;
  for (SgNode *node : NodeQuery::querySubTree(copy, V_SgIntVal)) {
    auto *value = isSgIntVal(node);
    if (value->get_value() == 41) {
      ROSE_ASSERT(editable == nullptr);
      editable = value;
    }
  }
  ROSE_ASSERT(editable != nullptr);
  editable->set_value(43);
  ROSE_ASSERT(editable->get_isModified());
  copy->set_unparse_output_filename("edited.c");
  copy->unparse();
  ROSE_ASSERT(contents("edited.c") != expected);
  ROSE_ASSERT(contents("edited.c").find("43") != std::string::npos);
  const std::string edited = contents("edited.c");
  const int previousFileId = copy->get_file_info()->get_physical_file_id();
  SageBuilder::rebindCopiedSourceFilePhysicalIdentity(copy, "rebased.c");
  const int rebasedFileId = copy->get_file_info()->get_physical_file_id();
  ROSE_ASSERT(rebasedFileId != previousFileId);
  for (auto *token : copy->get_token_list())
    ROSE_ASSERT(token->get_file_info()->get_physical_file_id() ==
                rebasedFileId);
  if (auto *root = copy->get_associated_include_file()) {
    root->validateIncludeOwnership();
    ROSE_ASSERT(root->get_filename() == "rebased.c");
  }
  if (copy->get_unparse_tokens())
    ROSE_ASSERT(!getTokenStream(copy).empty());
  copy->set_unparse_output_filename("rebased.c");
  copy->unparse();
  ROSE_ASSERT(contents("rebased.c") == edited);
  if (copy->get_unparse_tokens()) {
    Rose::AstJson::Options options;
    options.outputDirectory = "edited-copy-json";
    SgSourceFile *restored = Rose::AstJson::roundTripSourceFile(
        copy, Rose::AstJson::Checkpoint::PreOmpConstruction, options);
    ROSE_ASSERT(restored != nullptr && restored != copy);
    copy = restored;
    SgIntVal *restoredEdit = nullptr;
    for (SgNode *node : NodeQuery::querySubTree(copy, V_SgIntVal)) {
      if (isSgIntVal(node)->get_value() == 43) {
        ROSE_ASSERT(restoredEdit == nullptr);
        restoredEdit = isSgIntVal(node);
      }
    }
    ROSE_ASSERT(restoredEdit != nullptr && restoredEdit->get_isModified());
    copy->set_unparse_output_filename("restored-edit.c");
    copy->unparse();
    ROSE_ASSERT(contents("restored-edit.c") == edited);
    ROSE_ASSERT(copy->get_file_info()->get_physical_file_id() == rebasedFileId);
    ROSE_ASSERT(!getTokenStream(copy).empty());
    if (auto *root = copy->get_associated_include_file()) {
      root->validateIncludeOwnership();
      ROSE_ASSERT(root->get_filename() == "rebased.c");
    }
  }
  AstTests::runAllTests(copyProject);
  SageInterface::deleteAST(copyProject);
  return 0;
}
