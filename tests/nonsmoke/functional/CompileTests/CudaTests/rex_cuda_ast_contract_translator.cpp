#include "nodeQuery.h"
#include "rose.h"

#include <string>

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
    ROSE_ASSERT(isSgNonrealRefExp(callee) == nullptr);

    SgFunctionDeclaration *declaration = directFunctionDeclaration(callee);
    ROSE_ASSERT(declaration != nullptr);
    ROSE_ASSERT(declaration->get_name() == "rex_cuda_deduced_kernel");
    ROSE_ASSERT(declaration->get_functionModifier().isCudaKernel());

    SgCudaKernelExecConfig *config =
        isSgCudaKernelExecConfig(launch->get_exec_config());
    ROSE_ASSERT(config != nullptr);
    ROSE_ASSERT(config->get_grid() != nullptr);
    ROSE_ASSERT(config->get_blocks() != nullptr);
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
    ROSE_ASSERT(isSgTypeBool(logical->get_type()) != nullptr);

    if (isSgAndOp(logical) != nullptr) {
      ++and_count;
    } else {
      ++or_count;
    }
  }
  ROSE_ASSERT(and_count == 1);
  ROSE_ASSERT(or_count == 1);
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
  } else {
    ROSE_ABORT();
  }

  return backend(project);
}
