#include "CallGraph.h"
#include "nodeQuery.h"
#include "rose.h"

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <vector>

extern bool virtualInterproceduralControlFlowGraphs;

namespace {
bool pathEndsWith(const std::string &path, const std::string &suffix) {
  return path.size() >= suffix.size() &&
         path.compare(path.size() - suffix.size(), suffix.size(), suffix) == 0;
}

bool isFromSpecimen(SgLocatedNode *node, const std::string &specimen) {
  if (node == nullptr) {
    return false;
  }
  for (Sg_File_Info *file_info :
       {node->get_file_info(), node->get_startOfConstruct(),
        node->get_endOfConstruct()}) {
    if (file_info != nullptr &&
        pathEndsWith(file_info->get_filenameString(), specimen)) {
      return true;
    }
  }
  return false;
}

SgFunctionDeclaration *directFunctionDeclaration(SgExpression *expression) {
  auto *conversion = isSgCastExp(expression);
  ROSE_ASSERT(conversion != nullptr);
  ROSE_ASSERT(conversion->get_cast_type() == SgCastExp::e_implicit_cast);
  ROSE_ASSERT(conversion->get_semantic_conversion_kind() ==
              SgCastExp::e_semantic_conversion_FunctionToPointerDecay);
  conversion->validate_semantic_conversion();
  expression = conversion->get_operand();
  if (SgFunctionRefExp *function_ref = isSgFunctionRefExp(expression)) {
    SgFunctionSymbol *symbol = function_ref->get_symbol_i();
    return symbol != nullptr ? symbol->get_declaration() : nullptr;
  }
  if (SgTemplateFunctionRefExp *template_ref =
          isSgTemplateFunctionRefExp(expression)) {
    SgTemplateFunctionSymbol *symbol = template_ref->get_symbol_i();
    return symbol != nullptr ? symbol->get_declaration() : nullptr;
  }
  return nullptr;
}

void checkEdge(VirtualCFG::CFGNode source, VirtualCFG::CFGNode target) {
  const VirtualCFG::CFGEdge expected(source, target);
  const auto outgoing = source.outEdges();
  const auto incoming = target.inEdges();
  ROSE_ASSERT(std::count(outgoing.begin(), outgoing.end(), expected) == 1);
  ROSE_ASSERT(std::count(incoming.begin(), incoming.end(), expected) == 1);
}

void checkLaunchCFG(SgCudaKernelCallExp *launch) {
  using VirtualCFG::CFGNode;
  auto *config = launch->get_exec_config();
  ROSE_ASSERT(config != nullptr && config->get_parent() == launch);
  checkEdge(launch->cfgForBeginning(),
            launch->get_function()->cfgForBeginning());
  checkEdge(launch->get_function()->cfgForEnd(), CFGNode(launch, 1));
  checkEdge(CFGNode(launch, 1), config->cfgForBeginning());
  checkEdge(config->cfgForEnd(), CFGNode(launch, 2));
  checkEdge(CFGNode(launch, 2), launch->get_args()->cfgForBeginning());
  checkEdge(launch->get_args()->cfgForEnd(), CFGNode(launch, 3));
  checkEdge(CFGNode(launch, 3), launch->cfgForEnd());

  const std::vector<SgExpression *> operands = {
      config->get_grid(), config->get_blocks(), config->get_shared(),
      config->get_stream()};
  for (unsigned int index = 0; index < operands.size(); ++index) {
    SgExpression *operand = operands[index];
    if (operand != nullptr) {
      ROSE_ASSERT(operand->get_parent() == config);
      checkEdge(CFGNode(config, index), operand->cfgForBeginning());
      checkEdge(operand->cfgForEnd(), CFGNode(config, index + 1));
    } else {
      checkEdge(CFGNode(config, index), CFGNode(config, index + 1));
    }
  }
}

void checkDeducedTemplateKernelLaunch(SgProject *project,
                                      const std::string &specimen) {
  size_t launch_count = 0;
  for (SgNode *node : NodeQuery::querySubTree(project, V_SgCudaKernelCallExp)) {
    SgCudaKernelCallExp *launch = isSgCudaKernelCallExp(node);
    ROSE_ASSERT(launch != nullptr);
    if (!isFromSpecimen(launch, specimen)) {
      continue;
    }

    ++launch_count;
    SgExpression *callee = launch->get_function();
    ROSE_ASSERT(callee != nullptr);
    ROSE_ASSERT(callee->get_parent() == launch);
    ROSE_ASSERT(launch->get_args() != nullptr);
    ROSE_ASSERT(launch->get_args()->get_parent() == launch);
    ROSE_ASSERT(isSgNonrealRefExp(callee) == nullptr);

    SgFunctionDeclaration *declaration = directFunctionDeclaration(callee);
    ROSE_ASSERT(declaration != nullptr);
    ROSE_ASSERT(declaration->get_name() == "rex_cuda_deduced_kernel");
    ROSE_ASSERT(declaration->get_functionModifier().isCudaKernel());

    SgCudaKernelExecConfig *config =
        isSgCudaKernelExecConfig(launch->get_exec_config());
    ROSE_ASSERT(config != nullptr);
    ROSE_ASSERT(config->get_parent() == launch);
    ROSE_ASSERT(config->get_grid() != nullptr);
    ROSE_ASSERT(config->get_blocks() != nullptr);
    checkLaunchCFG(launch);

    ClassHierarchyWrapper hierarchy(project);
    Rose_STL_Container<SgFunctionDefinition *> definitions;
    CallTargetSet::getDefinitionsForExpression(launch, &hierarchy, definitions);
    ROSE_ASSERT(definitions.size() == 1);
    virtualInterproceduralControlFlowGraphs = true;
    checkEdge(VirtualCFG::CFGNode(launch, 3),
              definitions.front()->cfgForBeginning());
    checkEdge(definitions.front()->cfgForEnd(), launch->cfgForEnd());
    virtualInterproceduralControlFlowGraphs = false;

    auto *copy = isSgCudaKernelCallExp(SageInterface::copyExpression(launch));
    ROSE_ASSERT(copy != nullptr && copy->get_exec_config() != config);
    checkLaunchCFG(copy);
    SageInterface::deleteAST(copy);

    for (unsigned int optional = 0; optional <= 2; ++optional) {
      auto *generated_config = SageBuilder::buildCudaKernelExecConfig_nfi(
          SageBuilder::buildIntVal_nfi(1, "1"),
          SageBuilder::buildIntVal_nfi(2, "2"),
          optional >= 1 ? SageBuilder::buildIntVal_nfi(3, "3") : nullptr,
          optional >= 2 ? SageBuilder::buildIntVal_nfi(4, "4") : nullptr);
      auto *generated = SageBuilder::buildCudaKernelCallExp_nfi(
          SageInterface::copyExpression(callee),
          SageBuilder::buildExprListExp_nfi(), generated_config);
      checkLaunchCFG(generated);
      SageInterface::deleteAST(generated);
    }
  }
  ROSE_ASSERT(launch_count == 1);
}

void checkDependentLogicalResultTypes(SgProject *project,
                                      const std::string &specimen) {
  size_t and_count = 0;
  size_t or_count = 0;
  for (SgNode *node : NodeQuery::querySubTree(project, V_SgBinaryOp)) {
    SgBinaryOp *logical = isSgBinaryOp(node);
    if (logical == nullptr || !isFromSpecimen(logical, specimen) ||
        (isSgAndOp(logical) == nullptr && isSgOrOp(logical) == nullptr)) {
      continue;
    }

    ROSE_ASSERT(logical->get_lhs_operand() != nullptr);
    ROSE_ASSERT(logical->get_rhs_operand() != nullptr);
    ROSE_ASSERT(isSgNonrealType(logical->get_lhs_operand()->get_type()) !=
                nullptr);
    ROSE_ASSERT(isSgNonrealType(logical->get_rhs_operand()->get_type()) !=
                nullptr);
    // Instantiation may select an overloaded operator with a non-bool result.
    ROSE_ASSERT(isSgTypeDependent(logical->get_type()) != nullptr);

    if (isSgAndOp(logical) != nullptr) {
      ++and_count;
    } else {
      ++or_count;
    }
  }
  ROSE_ASSERT(and_count == 1);
  ROSE_ASSERT(or_count == 1);
}
void checkLaunchBounds(SgProject *project, const std::string &specimen) {
  const std::map<std::string, size_t> expected{{"rex_cuda_single", 1},
                                               {"rex_cuda_pair", 2},
                                               {"rex_cuda_cluster", 3},
                                               {"rex_cuda_dependent", 2}};
  std::set<std::string> seen;
  for (SgNode *node :
       NodeQuery::querySubTree(project, V_SgFunctionDeclaration)) {
    auto *declaration = isSgFunctionDeclaration(node);
    if (!isFromSpecimen(declaration, specimen) ||
        declaration->get_definition() == nullptr)
      continue;
    const std::string name = declaration->get_name().str();
    ROSE_ASSERT(expected.count(name) == 1 && seen.insert(name).second);
    auto *arguments =
        isSgExprListExp(declaration->get_cuda_launch_bounds_expression());
    ROSE_ASSERT(arguments != nullptr);
    ROSE_ASSERT(arguments->get_expressions().size() == expected.at(name));
    declaration->validate_cuda_launch_bounds_expression();
    if (name == "rex_cuda_dependent") {
      ROSE_ASSERT(isSgIntVal(arguments->get_expressions().front()) == nullptr);
    } else {
      for (size_t index = 0; index < arguments->get_expressions().size();
           ++index) {
        auto *value = isSgIntVal(arguments->get_expressions()[index]);
        ROSE_ASSERT(value != nullptr);
        ROSE_ASSERT(value->get_value() == (index == 0   ? 128
                                           : index == 1 ? 2
                                                        : 4));
      }
    }
  }
  ROSE_ASSERT(seen.size() == expected.size());
}
} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(frontendExitStatus(project) == 0);
  ROSE_ASSERT(project->get_fileList().size() == 1);

  SgSourceFile *source_file = isSgSourceFile(project->get_fileList().front());
  ROSE_ASSERT(source_file != nullptr);
  const std::string specimen = source_file->get_sourceFileNameWithoutPath();
  if (specimen == "rex_cuda_deduced_template_kernel_launch.cu") {
    checkDeducedTemplateKernelLaunch(project, specimen);
  } else if (specimen == "rex_cuda_dependent_logical_result_type.cu") {
    checkDependentLogicalResultTypes(project, specimen);
  } else if (specimen == "rex_cuda_launch_bounds.cu") {
    checkLaunchBounds(project, specimen);
  } else {
    ROSE_ABORT();
  }

  return backend(project);
}
