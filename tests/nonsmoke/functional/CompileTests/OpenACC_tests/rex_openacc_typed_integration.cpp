#include "rose.h"

#include "OpenACCParser.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

namespace {

void requireTypedForm(const std::string &input, openacc::InputForm form) {
  openacc::ParseResult result =
      openacc::parseDirective(input, {openacc::Language::Fortran, form});
  ROSE_ASSERT(result.diagnostics.empty());
  ROSE_ASSERT(result.succeeded() && result.directive.has_value());
  ROSE_ASSERT(result.directive->language() == openacc::Language::Fortran);
  ROSE_ASSERT(result.directive->inputForm() == form);
  ROSE_ASSERT(result.directive->kind() == openacc::DirectiveKind::Parallel);
  const openacc::GeneralDirective *general =
      std::get_if<openacc::GeneralDirective>(&result.directive->payload());
  ROSE_ASSERT(general != nullptr && general->deviceGroups.empty());
}

std::size_t countVariant(SgNode *root, VariantT variant) {
  return NodeQuery::querySubTree(root, variant).size();
}

std::string removeWhitespace(std::string text) {
  text.erase(
      std::remove_if(text.begin(), text.end(),
                     [](unsigned char ch) { return std::isspace(ch) != 0; }),
      text.end());
  return text;
}

SgSourceFile *findSource(SgProject *project, const std::string &suffix) {
  ROSE_ASSERT(project != nullptr);
  for (SgFile *file : project->get_fileList()) {
    SgSourceFile *source = isSgSourceFile(file);
    if (source != nullptr && source->getFileName().size() >= suffix.size() &&
        source->getFileName().compare(source->getFileName().size() -
                                          suffix.size(),
                                      suffix.size(), suffix) == 0) {
      return source;
    }
  }
  return nullptr;
}

void requireSupportedConversionAndIsolation(SgProject *project) {
  SgSourceFile *first = findSource(project, "rex_openacc_session_first.c");
  SgSourceFile *second = findSource(project, "rex_openacc_session_second.c");
  ROSE_ASSERT(first != nullptr && second != nullptr && first != second);

  ROSE_ASSERT(countVariant(first, V_SgAccParallelStatement) == 1);
  ROSE_ASSERT(countVariant(first, V_SgAccDataStatement) == 0);
  ROSE_ASSERT(countVariant(first, V_SgAccCacheStatement) == 0);
  ROSE_ASSERT(countVariant(first, V_SgAccWaitStatement) == 0);
  ROSE_ASSERT(countVariant(first, V_SgAccDefaultClause) == 1);
  ROSE_ASSERT(countVariant(first, V_SgAccReductionClause) == 1);

  SgAccDefaultClause *defaultClause = isSgAccDefaultClause(
      NodeQuery::querySubTree(first, V_SgAccDefaultClause).front());
  SgAccReductionClause *reductionClause = isSgAccReductionClause(
      NodeQuery::querySubTree(first, V_SgAccReductionClause).front());
  ROSE_ASSERT(defaultClause != nullptr &&
              defaultClause->get_default_kind() ==
                  static_cast<int>(openacc::DefaultKind::Present));
  ROSE_ASSERT(reductionClause != nullptr &&
              reductionClause->get_reduction_operator() ==
                  static_cast<int>(openacc::ReductionOperator::Add));

  ROSE_ASSERT(countVariant(second, V_SgAccParallelStatement) == 0);
  ROSE_ASSERT(countVariant(second, V_SgAccDataStatement) == 1);
  ROSE_ASSERT(countVariant(second, V_SgAccCacheStatement) == 1);
  ROSE_ASSERT(countVariant(second, V_SgAccWaitStatement) == 1);

  SgAccCacheStatement *cache = isSgAccCacheStatement(
      NodeQuery::querySubTree(second, V_SgAccCacheStatement).front());
  SgAccWaitStatement *wait = isSgAccWaitStatement(
      NodeQuery::querySubTree(second, V_SgAccWaitStatement).front());
  ROSE_ASSERT(cache != nullptr && cache->get_modifier() == 1 &&
              cache->get_variables() != nullptr &&
              cache->get_variables()->get_expressions().size() == 1);
  ROSE_ASSERT(wait != nullptr && wait->get_queues() &&
              wait->get_devnum() != nullptr &&
              wait->get_wait_list() != nullptr &&
              wait->get_wait_list()->get_expressions().size() == 2);
}

std::vector<SgAccWaitStatement *> requireWaitForms(SgProject *project) {
  SgSourceFile *source = findSource(project, "rex_openacc_wait_forms.c");
  ROSE_ASSERT(source != nullptr);
  const std::vector<SgNode *> waitNodes =
      NodeQuery::querySubTree(source, V_SgAccWaitStatement);
  ROSE_ASSERT(waitNodes.size() == 5);

  std::vector<SgAccWaitStatement *> waits;
  waits.reserve(waitNodes.size());
  for (SgNode *node : waitNodes) {
    SgAccWaitStatement *wait = isSgAccWaitStatement(node);
    ROSE_ASSERT(wait != nullptr);
    waits.push_back(wait);
  }

  ROSE_ASSERT(waits[0]->get_wait_list() == nullptr &&
              waits[0]->get_devnum() == nullptr && !waits[0]->get_queues());
  ROSE_ASSERT(waits[1]->get_wait_list() != nullptr &&
              waits[1]->get_wait_list()->get_expressions().size() == 1 &&
              waits[1]->get_devnum() == nullptr && !waits[1]->get_queues());
  ROSE_ASSERT(waits[2]->get_wait_list() == nullptr &&
              waits[2]->get_devnum() != nullptr && !waits[2]->get_queues());
  ROSE_ASSERT(waits[3]->get_wait_list() != nullptr &&
              waits[3]->get_wait_list()->get_expressions().size() == 1 &&
              waits[3]->get_devnum() == nullptr && waits[3]->get_queues());
  ROSE_ASSERT(waits[4]->get_wait_list() != nullptr &&
              waits[4]->get_wait_list()->get_expressions().size() == 2 &&
              waits[4]->get_devnum() != nullptr && waits[4]->get_queues());

  const std::vector<std::string> expected = {
      "#pragmaaccwait", "#pragmaaccwait(queue)", "#pragmaaccwait(devnum:queue)",
      "#pragmaaccwait(queues:queue)",
      "#pragmaaccwait(devnum:queue:queues:queue,queue+1)"};
  for (std::size_t index = 0; index < waits.size(); ++index) {
    ROSE_ASSERT(removeWhitespace(waits[index]->unparseToString()) ==
                expected[index]);
  }
  return waits;
}

enum class MalformedWaitMode { None, QueuesWithoutList, EmptyList };

MalformedWaitMode removeMalformedWaitMode(int &argc, char **argv) {
  MalformedWaitMode mode = MalformedWaitMode::None;
  int destination = 1;
  for (int source = 1; source < argc; ++source) {
    const std::string argument = argv[source];
    if (argument == "--reject-queues-without-list") {
      ROSE_ASSERT(mode == MalformedWaitMode::None);
      mode = MalformedWaitMode::QueuesWithoutList;
      continue;
    }
    if (argument == "--reject-empty-wait-list") {
      ROSE_ASSERT(mode == MalformedWaitMode::None);
      mode = MalformedWaitMode::EmptyList;
      continue;
    }
    argv[destination++] = argv[source];
  }
  argc = destination;
  return mode;
}

[[noreturn]] void exerciseMalformedWait(SgProject *project,
                                        MalformedWaitMode mode) {
  std::vector<SgAccWaitStatement *> waits = requireWaitForms(project);
  switch (mode) {
  case MalformedWaitMode::QueuesWithoutList:
    waits[3]->set_wait_list(nullptr);
    waits[3]->unparseToString();
    break;
  case MalformedWaitMode::EmptyList: {
    SgExprListExp *empty = SageBuilder::buildExprListExp();
    ROSE_ASSERT(empty != nullptr);
    waits[1]->set_wait_list(empty);
    empty->set_parent(waits[1]);
    waits[1]->unparseToString();
    break;
  }
  case MalformedWaitMode::None:
    break;
  }
  ROSE_ABORT();
}

} // namespace

int main(int argc, char **argv) {
  if (argc == 2 && std::string(argv[1]) == "--forms") {
    requireTypedForm("!$acc parallel\n", openacc::InputForm::FortranFree);
    requireTypedForm("c$acc parallel\n", openacc::InputForm::FortranFixed);
    return 0;
  }

  const MalformedWaitMode malformedWaitMode =
      removeMalformedWaitMode(argc, argv);
  SgProject *project = frontend(argc, argv);
  requireSupportedConversionAndIsolation(project);
  requireWaitForms(project);
  if (malformedWaitMode != MalformedWaitMode::None) {
    exerciseMalformedWait(project, malformedWaitMode);
  }
  return 0;
}
