#include "rose.h"

#include <cctype>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

static void checkOwned(SgExpression *operand, SgStatement *owner,
                       bool expected) {
  ROSE_ASSERT((operand != nullptr) == expected);
  if (operand != nullptr)
    ROSE_ASSERT(operand->get_parent() == owner);
}

static void checkOperand(SgExpression *operand, SgStatement *owner,
                         bool expected) {
  checkOwned(operand, owner, expected);
  if (operand != nullptr)
    ROSE_ASSERT(isSgVarRefExp(operand) != nullptr);
}

static std::size_t countEmitted(const std::filesystem::path &source,
                                const std::string &firstWord,
                                const std::string &secondWord) {
  std::ifstream generated("rose_" + source.filename().string());
  ROSE_ASSERT(generated.is_open());
  std::size_t count = 0;
  std::string line;
  while (std::getline(generated, line)) {
    std::istringstream words(line);
    std::string first;
    std::string second;
    words >> first >> second;
    if (!first.empty() &&
        std::isdigit(static_cast<unsigned char>(first.front()))) {
      first = second;
      words >> second;
    }
    for (char &letter : first)
      letter =
          static_cast<char>(std::toupper(static_cast<unsigned char>(letter)));
    for (char &letter : second)
      letter =
          static_cast<char>(std::toupper(static_cast<unsigned char>(letter)));
    if (first == firstWord && (secondWord.empty() || second == secondWord ||
                               second == secondWord + "("))
      ++count;
  }
  ROSE_ASSERT(generated.eof());
  return count;
}

int main(int argc, char **argv) {
  ROSE_ASSERT(argc >= 4);
  const std::string mode = argv[1];
  std::vector<char *> frontendArgs{argv[0]};
  for (int i = 2; i < argc; ++i)
    frontendArgs.push_back(argv[i]);
  std::unique_ptr<SgProject> project(
      frontend(static_cast<int>(frontendArgs.size()), frontendArgs.data()));
  ROSE_ASSERT(project != nullptr && project->get_frontendErrorCode() == 0);

  auto all = NodeQuery::querySubTree(project.get(), V_SgSyncAllStatement);
  auto memory = NodeQuery::querySubTree(project.get(), V_SgSyncMemoryStatement);
  auto images = NodeQuery::querySubTree(project.get(), V_SgSyncImagesStatement);
  auto teams = NodeQuery::querySubTree(project.get(), V_SgSyncTeamStatement);
  auto locks = NodeQuery::querySubTree(project.get(), V_SgLockStatement);
  auto unlocks = NodeQuery::querySubTree(project.get(), V_SgUnlockStatement);
  if (mode == "R858") {
    ROSE_ASSERT(all.size() == 2 && memory.empty());
    for (SgNode *node : all) {
      auto *stmt = isSgSyncAllStatement(node);
      ROSE_ASSERT(stmt->get_parent() != nullptr);
      ROSE_ASSERT(stmt->get_image_control_statement() ==
                  SgImageControlStatement::e_sync_all);
      checkOperand(stmt->get_stat(), stmt, false);
      checkOperand(stmt->get_err_msg(), stmt, false);
    }
  } else if (mode == "R859") {
    ROSE_ASSERT(all.size() == 1 && memory.empty());
    auto *stmt = isSgSyncAllStatement(all.front());
    ROSE_ASSERT(stmt->get_parent() != nullptr);
    ROSE_ASSERT(stmt->get_image_control_statement() ==
                SgImageControlStatement::e_sync_all);
    checkOperand(stmt->get_stat(), stmt, true);
    checkOperand(stmt->get_err_msg(), stmt, true);
  } else if (mode == "R862") {
    ROSE_ASSERT(all.empty() && memory.size() == 3);
    for (std::size_t index = 0; index < memory.size(); ++index) {
      auto *stmt = isSgSyncMemoryStatement(memory[index]);
      ROSE_ASSERT(stmt->get_parent() != nullptr);
      ROSE_ASSERT(stmt->get_image_control_statement() ==
                  SgImageControlStatement::e_sync_memory);
      checkOperand(stmt->get_stat(), stmt, index == 2);
      checkOperand(stmt->get_err_msg(), stmt, index != 0);
    }
  } else {
    ROSE_ASSERT(mode == "image_control" && all.empty() && memory.empty());
    ROSE_ASSERT(images.size() == 3 && teams.size() == 1 && locks.size() == 1 &&
                unlocks.size() == 1);
    for (std::size_t index = 0; index < images.size(); ++index) {
      auto *stmt = isSgSyncImagesStatement(images[index]);
      ROSE_ASSERT(stmt->get_image_control_statement() ==
                  SgImageControlStatement::e_sync_images);
      ROSE_ASSERT(stmt->get_all_images() == (index == 1));
      checkOwned(stmt->get_image_set(), stmt, index != 1);
      checkOperand(stmt->get_stat(), stmt, index == 2);
      checkOperand(stmt->get_err_msg(), stmt, index == 2);
    }
    auto *team = isSgSyncTeamStatement(teams.front());
    ROSE_ASSERT(team->get_image_control_statement() ==
                SgImageControlStatement::e_sync_team);
    checkOwned(team->get_team_value(), team, true);
    checkOperand(team->get_stat(), team, true);
    checkOperand(team->get_err_msg(), team, true);

    auto *lock = isSgLockStatement(locks.front());
    ROSE_ASSERT(lock->get_image_control_statement() ==
                SgImageControlStatement::e_lock);
    checkOwned(lock->get_lock_variable(), lock, true);
    checkOperand(lock->get_acquired_lock(), lock, true);
    checkOperand(lock->get_stat(), lock, true);
    checkOperand(lock->get_err_msg(), lock, true);

    auto *unlock = isSgUnlockStatement(unlocks.front());
    ROSE_ASSERT(unlock->get_image_control_statement() ==
                SgImageControlStatement::e_unlock);
    checkOwned(unlock->get_lock_variable(), unlock, true);
    checkOperand(unlock->get_stat(), unlock, true);
    checkOperand(unlock->get_err_msg(), unlock, true);
  }

  if (int status = backend(project.get()); status != 0)
    return status;
  const std::filesystem::path source(argv[argc - 1]);
  ROSE_ASSERT(countEmitted(source, "SYNC", "ALL") == all.size());
  ROSE_ASSERT(countEmitted(source, "SYNC", "MEMORY") == memory.size());
  ROSE_ASSERT(countEmitted(source, "SYNC", "IMAGES") == images.size());
  ROSE_ASSERT(countEmitted(source, "SYNC", "TEAM") == teams.size());
  ROSE_ASSERT(countEmitted(source, "LOCK", "") == locks.size());
  ROSE_ASSERT(countEmitted(source, "UNLOCK", "") == unlocks.size());
  return 0;
}
