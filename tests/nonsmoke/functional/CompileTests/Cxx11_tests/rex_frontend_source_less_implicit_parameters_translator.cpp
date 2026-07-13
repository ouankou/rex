#include "nodeQuery.h"
#include "rose.h"

#include <algorithm>
#include <string>

namespace {
  bool isSemanticIdentity(SgFunctionDeclaration *declaration) {
    SgAuxiliaryDeclarationList *owner =
        declaration != nullptr
            ? isSgAuxiliaryDeclarationList(declaration->get_parent())
            : nullptr;
    SgScopeStatement *scope =
        owner != nullptr ? isSgScopeStatement(owner->get_parent()) : nullptr;
    return scope != nullptr && scope->get_auxiliary_declarations() == owner &&
           declaration->get_scope() == scope &&
           std::count(owner->get_declarations().begin(),
                      owner->get_declarations().end(), declaration) == 1;
  }

  void requireExactSemanticProvenance(SgLocatedNode *node) {
    ROSE_ASSERT(node != nullptr);
    Sg_File_Info *primary = node->get_file_info();
    Sg_File_Info *start = node->get_startOfConstruct();
    Sg_File_Info *end = node->get_endOfConstruct();
    ROSE_ASSERT(primary != nullptr && primary == start && end != nullptr);
    ROSE_ASSERT(start != end);
    for (Sg_File_Info *position : {primary, start, end}) {
      ROSE_ASSERT(position->get_parent() == node);
      ROSE_ASSERT(position->isCompilerGenerated());
      ROSE_ASSERT(position->isFrontendSpecific());
      ROSE_ASSERT(!position->isTransformation());
      ROSE_ASSERT(!position->isSourcePositionUnavailableInFrontend());
      ROSE_ASSERT(position->isOutputInCodeGeneration());
      ROSE_ASSERT(position->get_file_id() ==
                  Sg_File_Info::COMPILER_GENERATED_FILE_ID);
      ROSE_ASSERT(position->get_physical_file_id() ==
                  Sg_File_Info::COMPILER_GENERATED_FILE_ID);
    }
  }

  void requireSemanticParameters(SgFunctionDeclaration *function,
                                 bool expectEllipsis) {
    ROSE_ASSERT(function != nullptr && isSemanticIdentity(function));
    requireExactSemanticProvenance(function);
    SgFunctionParameterList *parameters = function->get_parameterList();
    ROSE_ASSERT(parameters != nullptr && parameters->get_parent() == function);
    requireExactSemanticProvenance(parameters);

    std::size_t ellipsisCount = 0;
    for (SgInitializedName *parameter : parameters->get_args()) {
      ROSE_ASSERT(parameter != nullptr &&
                  parameter->get_parent() == parameters);
      ROSE_ASSERT(parameter->get_scope() != nullptr);
      requireExactSemanticProvenance(parameter);
      if (isSgTypeEllipse(parameter->get_type()) != nullptr) {
        ++ellipsisCount;
      }
    }
    ROSE_ASSERT(ellipsisCount == (expectEllipsis ? 1 : 0));
  }
} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(frontendExitStatus(project) == 0);

  std::size_t operatorNewCount = 0;
  std::size_t builtinPrintfCount = 0;
  for (SgNode *node :
       NodeQuery::querySubTree(project, V_SgFunctionDeclaration)) {
    SgFunctionDeclaration *function = isSgFunctionDeclaration(node);
    if (function == nullptr || !isSemanticIdentity(function)) {
      continue;
    }
    const std::string name = function->get_name().getString();
    if (name == "operator new") {
      requireSemanticParameters(function, false);
      ROSE_ASSERT(function->get_parameterList()->get_args().size() == 1);
      ++operatorNewCount;
    } else if (name == "__builtin_printf") {
      requireSemanticParameters(function, true);
      ROSE_ASSERT(function->get_parameterList()->get_args().size() == 2);
      ++builtinPrintfCount;
    }
  }
  ROSE_ASSERT(operatorNewCount == 1);
  ROSE_ASSERT(builtinPrintfCount == 1);
  return 0;
}
