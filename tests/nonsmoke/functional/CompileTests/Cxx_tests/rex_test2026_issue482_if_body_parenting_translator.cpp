#include "nodeQuery.h"

#include "rose.h"

#include <utility>
#include <vector>

namespace {
SgSourceFile *findMainFile(SgProject *project) {
  SgSourceFile *first_source_file = NULL;
  for (SgFile *file : project->get_fileList()) {
    if (SgSourceFile *source = isSgSourceFile(file)) {
      if (!source->get_isHeaderFile()) {
        return source;
      }
      if (first_source_file == NULL) {
        first_source_file = source;
      }
    }
  }

  return first_source_file;
}

bool isFromFile(SgLocatedNode *node, SgSourceFile *source_file) {
  Sg_File_Info *info = node != NULL ? node->get_file_info() : NULL;
  return info != NULL && info->isSameFile(source_file);
}

bool parentHasChild(SgNode *parent, SgNode *child) {
  std::vector<std::pair<SgNode *, std::string>> children =
      parent->returnDataMemberPointers();
  for (const std::pair<SgNode *, std::string> &entry : children) {
    if (entry.first == child) {
      return true;
    }
  }

  return false;
}

SgIfStmt *findTargetIfStmt(SgSourceFile *source_file) {
  for (SgNode *node : NodeQuery::querySubTree(source_file, V_SgIfStmt)) {
    SgIfStmt *if_stmt = isSgIfStmt(node);
    if (if_stmt == NULL || !isFromFile(if_stmt, source_file)) {
      continue;
    }

    SgStatement *true_body = if_stmt->get_true_body();
    if (isSgBasicBlock(true_body) != NULL) {
      return if_stmt;
    }
  }

  return NULL;
}
} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != NULL);
  ROSE_ASSERT(frontendExitStatus(project) == 0);

  SgSourceFile *source_file = findMainFile(project);
  ROSE_ASSERT(source_file != NULL);

  SgIfStmt *if_stmt = findTargetIfStmt(source_file);
  ROSE_ASSERT(if_stmt != NULL);

  SgStatement *true_body = if_stmt->get_true_body();
  SgBasicBlock *true_block = isSgBasicBlock(true_body);
  ROSE_ASSERT(true_block != NULL);
  ROSE_ASSERT(true_block->get_parent() == if_stmt);
  ROSE_ASSERT(if_stmt->isChild(true_block));
  ROSE_ASSERT(parentHasChild(if_stmt, true_block));

  return backend(project);
}
