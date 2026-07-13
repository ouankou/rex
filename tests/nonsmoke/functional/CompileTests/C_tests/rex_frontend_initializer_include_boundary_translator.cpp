#include "rose.h"

#include <algorithm>
#include <map>
#include <string>

namespace {
SgSourceFile *mainSourceFile(SgProject *project) {
  for (SgFile *file : project->get_fileList()) {
    SgSourceFile *sourceFile = isSgSourceFile(file);
    if (sourceFile != nullptr && !sourceFile->get_isHeaderFile()) {
      return sourceFile;
    }
  }
  return nullptr;
}

SgVariableDeclaration *findVariable(SgGlobal *global, const SgName &name) {
  for (SgDeclarationStatement *declaration : global->get_declarations()) {
    SgVariableDeclaration *variable = isSgVariableDeclaration(declaration);
    if (variable == nullptr) {
      continue;
    }
    for (SgInitializedName *initializedName : variable->get_variables()) {
      if (initializedName != nullptr && initializedName->get_name() == name) {
        return variable;
      }
    }
  }
  return nullptr;
}

bool isInitializerInclude(const PreprocessingInfo *info) {
  return info != nullptr &&
         info->getTypeOfDirective() ==
             PreprocessingInfo::CpreprocessorIncludeDeclaration &&
         info->getString().find(
             "rex_frontend_initializer_include_entries.def") !=
             std::string::npos;
}

bool isScalarInitializerInclude(const PreprocessingInfo *info) {
  return info != nullptr &&
         info->getTypeOfDirective() ==
             PreprocessingInfo::CpreprocessorIncludeDeclaration &&
         info->getString().find("rex_frontend_scalar_initializer_value.def") !=
             std::string::npos;
}

bool isCompleteInitializerInclude(const PreprocessingInfo *info) {
  return info != nullptr &&
         info->getTypeOfDirective() ==
             PreprocessingInfo::CpreprocessorIncludeDeclaration &&
         info->getString().find(
             "rex_frontend_complete_initializer_value.def") !=
             std::string::npos;
}

bool sourceLess(Sg_File_Info *lhs, Sg_File_Info *rhs) {
  ROSE_ASSERT(lhs != nullptr && rhs != nullptr);
  ROSE_ASSERT(lhs->isSameFile(*rhs));
  return lhs->get_line() < rhs->get_line() ||
         (lhs->get_line() == rhs->get_line() &&
          lhs->get_col() < rhs->get_col());
}
} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(frontendExitStatus(project) == 0);

  SgSourceFile *sourceFile = mainSourceFile(project);
  ROSE_ASSERT(sourceFile != nullptr);
  SgGlobal *global = sourceFile->get_globalScope();
  ROSE_ASSERT(global != nullptr);
  SgVariableDeclaration *variable =
      findVariable(global, "rex_initializer_include_records");
  ROSE_ASSERT(variable != nullptr);
  ROSE_ASSERT(variable->get_variables().size() == 1);
  SgInitializedName *initializedName = variable->get_variables().front();
  ROSE_ASSERT(initializedName != nullptr);
  SgAggregateInitializer *aggregate =
      isSgAggregateInitializer(initializedName->get_initializer());
  ROSE_ASSERT(aggregate != nullptr);
  ROSE_ASSERT(aggregate->get_source_form() ==
              SgAggregateInitializer::e_aggregate_initializer_source_braced);
  SgExprListExp *initializerList = aggregate->get_initializers();
  ROSE_ASSERT(initializerList != nullptr);
  ROSE_ASSERT(initializerList->get_parent() == aggregate);
  ROSE_ASSERT(initializerList->get_expressions().size() == 3);
  const SgUnsignedCharList &sourceElementRoles =
      aggregate->get_source_element_roles();
  ROSE_ASSERT(sourceElementRoles.size() ==
              initializerList->get_expressions().size());
  ROSE_ASSERT(sourceElementRoles[0] ==
              SgAggregateInitializer::e_source_element_include_expansion);
  ROSE_ASSERT(sourceElementRoles[1] ==
              SgAggregateInitializer::e_source_element_include_expansion);
  ROSE_ASSERT(sourceElementRoles[2] ==
              SgAggregateInitializer::e_source_element_ast);
  SgExpression *firstElement = initializerList->get_expressions().front();
  ROSE_ASSERT(firstElement != nullptr);
  ROSE_ASSERT(firstElement->get_parent() == initializerList);

  std::map<PreprocessingInfo *, SgLocatedNode *> includeOwners;
  for (SgNode *node : NodeQuery::querySubTree(project, V_SgLocatedNode)) {
    SgLocatedNode *located = isSgLocatedNode(node);
    ROSE_ASSERT(located != nullptr);
    AttachedPreprocessingInfoType *attached =
        located->getAttachedPreprocessingInfo();
    if (attached == nullptr) {
      continue;
    }
    for (PreprocessingInfo *info : *attached) {
      if (!isInitializerInclude(info)) {
        continue;
      }
      ROSE_ASSERT(includeOwners.emplace(info, located).second);
      ROSE_ASSERT(located == firstElement);
      ROSE_ASSERT(info->getRelativePosition() == PreprocessingInfo::before);
      Sg_File_Info *includePosition = info->get_file_info();
      Sg_File_Info *aggregateStart = aggregate->get_startOfConstruct();
      Sg_File_Info *aggregateEnd = aggregate->get_endOfConstruct();
      ROSE_ASSERT(includePosition != nullptr);
      ROSE_ASSERT(aggregateStart != nullptr && aggregateEnd != nullptr);
      ROSE_ASSERT(aggregateStart->isSameFile(*aggregateEnd));
      ROSE_ASSERT(aggregateStart->isSameFile(*includePosition));
      ROSE_ASSERT(sourceLess(aggregateStart, includePosition));
      ROSE_ASSERT(sourceLess(includePosition, aggregateEnd));
    }
  }

  ROSE_ASSERT(includeOwners.size() == 1);
  ROSE_ASSERT(variable->getAttachedPreprocessingInfo() == nullptr ||
              std::none_of(variable->getAttachedPreprocessingInfo()->begin(),
                           variable->getAttachedPreprocessingInfo()->end(),
                           isInitializerInclude));
  ROSE_ASSERT(aggregate->getAttachedPreprocessingInfo() == nullptr ||
              std::none_of(aggregate->getAttachedPreprocessingInfo()->begin(),
                           aggregate->getAttachedPreprocessingInfo()->end(),
                           isInitializerInclude));

  SgVariableDeclaration *scalarVariable =
      findVariable(global, "rex_scalar_initializer_include_value");
  ROSE_ASSERT(scalarVariable != nullptr);
  ROSE_ASSERT(scalarVariable->get_variables().size() == 1);
  SgInitializedName *scalarName = scalarVariable->get_variables().front();
  ROSE_ASSERT(scalarName != nullptr);
  SgAssignInitializer *scalarInitializer =
      isSgAssignInitializer(scalarName->get_initializer());
  ROSE_ASSERT(scalarInitializer != nullptr);
  ROSE_ASSERT(scalarInitializer->get_source_form() ==
              SgAssignInitializer::
                  e_assignment_initializer_source_include_operand_expansion);
  SgExpression *scalarOperand = scalarInitializer->get_operand_i();
  ROSE_ASSERT(scalarOperand != nullptr);
  ROSE_ASSERT(scalarOperand->get_parent() == scalarInitializer);
  Sg_File_Info *scalarStart = scalarInitializer->get_startOfConstruct();
  Sg_File_Info *scalarEnd = scalarInitializer->get_endOfConstruct();
  Sg_File_Info *operandStart = scalarOperand->get_startOfConstruct();
  ROSE_ASSERT(scalarStart != nullptr && scalarEnd != nullptr);
  ROSE_ASSERT(operandStart != nullptr);
  ROSE_ASSERT(scalarStart->isSameFile(*scalarEnd));
  ROSE_ASSERT(!scalarStart->isSameFile(*operandStart));

  size_t scalarIncludeCount = 0;
  for (SgNode *node : NodeQuery::querySubTree(project, V_SgLocatedNode)) {
    SgLocatedNode *located = isSgLocatedNode(node);
    ROSE_ASSERT(located != nullptr);
    AttachedPreprocessingInfoType *attached =
        located->getAttachedPreprocessingInfo();
    if (attached == nullptr) {
      continue;
    }
    for (PreprocessingInfo *info : *attached) {
      if (!isScalarInitializerInclude(info)) {
        continue;
      }
      ++scalarIncludeCount;
      ROSE_ASSERT(located == scalarInitializer);
      ROSE_ASSERT(info->getRelativePosition() == PreprocessingInfo::inside);
      Sg_File_Info *includePosition = info->get_file_info();
      ROSE_ASSERT(includePosition != nullptr);
      ROSE_ASSERT(includePosition->isSameFile(*scalarStart));
      ROSE_ASSERT(sourceLess(scalarStart, includePosition));
      ROSE_ASSERT(sourceLess(includePosition, scalarEnd));
    }
  }
  ROSE_ASSERT(scalarIncludeCount == 1);

  SgVariableDeclaration *completeVariable =
      findVariable(global, "rex_complete_initializer_include_value");
  ROSE_ASSERT(completeVariable != nullptr);
  ROSE_ASSERT(completeVariable->get_variables().size() == 1);
  SgInitializedName *completeName = completeVariable->get_variables().front();
  ROSE_ASSERT(completeName != nullptr);
  SgAssignInitializer *completeInitializer =
      isSgAssignInitializer(completeName->get_initializer());
  ROSE_ASSERT(completeInitializer != nullptr);
  ROSE_ASSERT(completeInitializer->get_source_form() ==
              SgAssignInitializer::
                  e_assignment_initializer_source_include_complete_expansion);
  SgExpression *completeOperand = completeInitializer->get_operand_i();
  ROSE_ASSERT(completeOperand != nullptr);
  ROSE_ASSERT(completeOperand->get_parent() == completeInitializer);
  Sg_File_Info *completeStart = completeInitializer->get_startOfConstruct();
  Sg_File_Info *completeEnd = completeInitializer->get_endOfConstruct();
  Sg_File_Info *completeOperandStart = completeOperand->get_startOfConstruct();
  ROSE_ASSERT(completeStart != nullptr && completeEnd != nullptr);
  ROSE_ASSERT(completeOperandStart != nullptr);
  ROSE_ASSERT(completeStart->isSameFile(*completeEnd));
  ROSE_ASSERT(!completeStart->isSameFile(*completeOperandStart));

  size_t completeIncludeCount = 0;
  for (SgNode *node : NodeQuery::querySubTree(project, V_SgLocatedNode)) {
    SgLocatedNode *located = isSgLocatedNode(node);
    ROSE_ASSERT(located != nullptr);
    AttachedPreprocessingInfoType *attached =
        located->getAttachedPreprocessingInfo();
    if (attached == nullptr) {
      continue;
    }
    for (PreprocessingInfo *info : *attached) {
      if (!isCompleteInitializerInclude(info)) {
        continue;
      }
      ++completeIncludeCount;
      ROSE_ASSERT(located == completeInitializer);
      ROSE_ASSERT(info->getRelativePosition() == PreprocessingInfo::inside);
      Sg_File_Info *includePosition = info->get_file_info();
      ROSE_ASSERT(includePosition != nullptr);
      ROSE_ASSERT(includePosition->isSameFile(*completeStart));
      ROSE_ASSERT(includePosition->get_line() == completeStart->get_line());
      ROSE_ASSERT(includePosition->get_col() == completeStart->get_col());
      ROSE_ASSERT(sourceLess(includePosition, completeEnd));
    }
  }
  ROSE_ASSERT(completeIncludeCount == 1);

  return backend(project);
}
