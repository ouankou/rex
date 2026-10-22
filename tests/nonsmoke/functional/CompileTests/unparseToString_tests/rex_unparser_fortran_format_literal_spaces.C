#include "astJson/sageAstJson.h"
#include "rose.h"

#include <string>

namespace {
void verify(SgProject *project) {
  const Rose_STL_Container<SgNode *> statements =
      NodeQuery::querySubTree(project, V_SgFormatStatement);
  ROSE_ASSERT(statements.size() == 6);
  SgFormatStatement *format = isSgFormatStatement(statements.front());
  ASSERT_not_null(format);
  ASSERT_not_null(format->get_format_item_list());
  const std::size_t sizes[] = {6, 1, 1, 1, 3, 13};
  for (std::size_t index = 0; index < statements.size(); ++index) {
    auto *statement = isSgFormatStatement(statements[index]);
    auto *list = statement->get_format_item_list();
    ROSE_ASSERT(list != nullptr && list->get_parent() == statement);
    list->validate();
    ROSE_ASSERT(list->get_format_item_list().size() == sizes[index]);
  }

  const std::string text = format->unparseToString();
  if (text.find("'single   spaces'") == std::string::npos) {
    ROSE_ABORT();
  }
  if (text.find("\"double  spaces\"") == std::string::npos) {
    ROSE_ABORT();
  }
  if (text.find("'single spaces'") != std::string::npos ||
      text.find("\"double spaces\"") != std::string::npos) {
    ROSE_ABORT();
  }
  auto *repeated = isSgFormatStatement(statements[1])
                       ->get_format_item_list()
                       ->get_format_item_list()
                       .front();
  ROSE_ASSERT(repeated->get_repeat_count() == 2);
  const auto &integer =
      std::get<SgFormatItem::Intrinsic>(repeated->get_descriptor().value);
  ROSE_ASSERT(integer.kind == SgFormatItem::Intrinsic::Kind::I &&
              integer.width == 1);
  auto *unlimited = isSgFormatStatement(statements[2])
                        ->get_format_item_list()
                        ->get_format_item_list()
                        .front();
  ROSE_ASSERT(
      std::get<SgFormatItem::Group>(unlimited->get_descriptor().value) ==
      SgFormatItem::Group::unlimited);
  const auto &nested =
      unlimited->get_format_item_list()->get_format_item_list();
  ROSE_ASSERT(nested.size() == 3);
  ROSE_ASSERT(
      std::get<SgFormatItem::Character>(nested.back()->get_descriptor().value)
          .value == "x  y");
  const auto &derived = isSgFormatStatement(statements[5])
                            ->get_format_item_list()
                            ->get_format_item_list();
  ROSE_ASSERT(
      !std::get<SgFormatItem::Derived>(derived[0]->get_descriptor().value).tag);
  const auto &empty =
      std::get<SgFormatItem::Derived>(derived[1]->get_descriptor().value).tag;
  ROSE_ASSERT(empty && empty->value.empty() &&
              empty->delimiter ==
                  SgFormatItem::Character::Delimiter::single_quote);
  const auto &tagged =
      std::get<SgFormatItem::Derived>(derived[2]->get_descriptor().value);
  ROSE_ASSERT(tagged.tag && tagged.tag->value == "tag" &&
              tagged.parameters == std::vector<std::int64_t>({-2, 3}));
}
} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ASSERT_not_null(project);
  verify(project);
  SgTreeCopy copier;
  auto *copied = isSgProject(project->copy(copier));
  ASSERT_not_null(copied);
  verify(copied);
  const auto originals = NodeQuery::querySubTree(project, V_SgFormatItem);
  const auto copies = NodeQuery::querySubTree(copied, V_SgFormatItem);
  ROSE_ASSERT(originals.size() == copies.size());
  for (std::size_t index = 0; index < originals.size(); ++index)
    ROSE_ASSERT(originals[index] != copies[index]);
  SageInterface::deleteAST(copied,
                           SageInterface::DeleteAstMode::kRequireIsolated);
  auto *file = isSgSourceFile(&project->get_file(0));
  Rose::AstJson::Options options;
  options.outputDirectory = "rex_format_json";
  ROSE_ASSERT(setenv("REX_AST_JSON_CHECKPOINT", "pre-omp-construction", 1) ==
              0);
  auto *restored = Rose::AstJson::roundTripSourceFile(
      file, Rose::AstJson::Checkpoint::PreOmpConstruction, options);
  ROSE_ASSERT(restored != nullptr && restored != file);
  verify(project);
  project->skipfinalCompileStep(true);
  const int status = backend(project);
  SageInterface::deleteAST(project,
                           SageInterface::DeleteAstMode::kRequireIsolated);
  return status;
}
