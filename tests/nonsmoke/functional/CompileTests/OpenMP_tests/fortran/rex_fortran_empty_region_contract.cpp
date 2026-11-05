#include "rose.h"

#include <string>

int main(int argc, char **argv) {
  bool openacc = false;
  int retained = 1;
  for (int index = 1; index < argc; ++index) {
    const std::string argument = argv[index];
    if (argument == "--empty-region-family=acc")
      openacc = true;
    else
      argv[retained++] = argv[index];
  }
  argc = retained;
  argv[argc] = nullptr;
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr && project->numberOfFiles() == 1);
  auto *file = isSgSourceFile(&project->get_file(0));
  ROSE_ASSERT(file != nullptr);
  const auto regions = NodeQuery::querySubTree(
      file, openacc ? V_SgAccParallelStatement : V_SgOmpParallelStatement);
  ROSE_ASSERT(regions.size() == 2);
  size_t plain_bodies = 0;
  size_t commented_bodies = 0;
  size_t end_inline_comments = 0;
  for (SgNode *node : regions) {
    auto *omp = isSgOmpParallelStatement(node);
    auto *acc = isSgAccParallelStatement(node);
    ROSE_ASSERT((omp != nullptr) != (acc != nullptr));
    ROSE_ASSERT(omp != nullptr ? omp->get_clauses().empty()
                               : acc->get_clauses().empty());
    auto *body =
        isSgBasicBlock(omp != nullptr ? omp->get_body() : acc->get_body());
    ROSE_ASSERT(body != nullptr && body->get_parent() == node);
    // Trivia does not create executable placeholder statements. The source
    // anchors must survive reconstruction even though there is no first child.
    ROSE_ASSERT(body->get_statements().empty());
    for (const Sg_File_Info *position :
         {body->get_startOfConstruct(), body->get_endOfConstruct()}) {
      ROSE_ASSERT(position != nullptr && position->get_line() > 0 &&
                  !position->isSourcePositionUnavailableInFrontend());
      ROSE_ASSERT(
          position->get_filenameString() ==
          isSgStatement(node)->get_startOfConstruct()->get_filenameString());
    }
    const int begin_line =
        isSgStatement(node)->get_startOfConstruct()->get_line();
    ROSE_ASSERT(begin_line == 6 || begin_line == 8);
    ROSE_ASSERT(body->get_startOfConstruct()->get_line() == begin_line);
    ROSE_ASSERT(body->get_endOfConstruct()->get_line() ==
                (begin_line == 6 ? 7 : 12));
    if (const auto *comments =
            isSgStatement(node)->getAttachedPreprocessingInfo()) {
      for (const PreprocessingInfo *comment : *comments) {
        ROSE_ASSERT(comment != nullptr);
        if (comment->getString().find("REX_EMPTY_END_INLINE") !=
            std::string::npos) {
          ROSE_ASSERT(comment->getRelativePosition() ==
                      PreprocessingInfo::after);
          ++end_inline_comments;
        }
      }
    }
    const auto *comments = body->getAttachedPreprocessingInfo();
    if (comments == nullptr || comments->empty()) {
      ++plain_bodies;
      continue;
    }
    size_t first = 0;
    size_t last = 0;
    size_t begin_inline = 0;
    for (const PreprocessingInfo *comment : *comments) {
      ROSE_ASSERT(comment != nullptr);
      const std::string &text = comment->getString();
      ROSE_ASSERT(text.find("REX_EMPTY_BEFORE") == std::string::npos &&
                  text.find("REX_EMPTY_AFTER") == std::string::npos &&
                  text.find("REX_EMPTY_END_INLINE") == std::string::npos);
      if (text.find("REX_EMPTY_BEGIN_INLINE") != std::string::npos) {
        ROSE_ASSERT(comment->getRelativePosition() ==
                    PreprocessingInfo::inside);
        ++begin_inline;
      }
      if (text.find("REX_EMPTY_INSIDE_FIRST") != std::string::npos) {
        ROSE_ASSERT(comment->getRelativePosition() ==
                    PreprocessingInfo::inside);
        ++first;
      }
      if (text.find("REX_EMPTY_INSIDE_LAST") != std::string::npos) {
        ROSE_ASSERT(comment->getRelativePosition() ==
                    PreprocessingInfo::inside);
        ++last;
      }
    }
    ROSE_ASSERT(first == 1 && last == 1 && begin_inline == 1);
    ++commented_bodies;
  }
  ROSE_ASSERT(plain_bodies == 1 && commented_bodies == 1 &&
              end_inline_comments == 1);
  AstTests::runAllTests(project);
  return backend(project);
}
