#include "RoseAst.h"
#include "rose.h"

#include <string>

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(frontendExitStatus(project) == 0);
  ROSE_ASSERT(project->get_fileList().size() == 1);

  SgSourceFile *source_file = isSgSourceFile(project->get_fileList().front());
  ROSE_ASSERT(source_file != nullptr);
  const std::string source_name = source_file->get_sourceFileNameWithoutPath();

  size_t co_await_count = 0;
  size_t co_yield_count = 0;
  size_t await_void_type_count = 0;
  size_t await_nonvoid_type_count = 0;
  size_t co_return_count = 0;
  size_t semantic_size_declaration_count = 0;
  size_t semantic_size_field_count = 0;
  size_t semantic_size_first_parameter_count = 0;
  size_t semantic_size_defining_parameter_count = 0;
  for (SgNode *node : RoseAst(project)) {
    if (SgAwaitExpression *await_expression = isSgAwaitExpression(node)) {
      ROSE_ASSERT(await_expression->get_value() != nullptr);
      ROSE_ASSERT(await_expression->get_value()->get_parent() ==
                  await_expression);
      SgType *await_type = await_expression->get_type();
      ROSE_ASSERT(await_type != nullptr);
      ROSE_ASSERT(isSgTypeUnknown(await_type) == nullptr);
      ROSE_ASSERT(isSgTypeDefault(await_type) == nullptr);
      ROSE_ASSERT(await_expression->get_expression_type() == await_type);
      if (isSgTypeVoid(await_type->findBaseType()) != nullptr) {
        ++await_void_type_count;
      } else {
        ++await_nonvoid_type_count;
      }
      switch (await_expression->get_coroutine_keyword_kind()) {
      case SgAwaitExpression::e_coroutine_keyword_co_await:
        ++co_await_count;
        break;
      case SgAwaitExpression::e_coroutine_keyword_co_yield:
        ++co_yield_count;
        break;
      case SgAwaitExpression::e_coroutine_keyword_unspecified:
      default:
        ROSE_ABORT();
      }
    }

    if (SgReturnStmt *return_statement = isSgReturnStmt(node)) {
      switch (return_statement->get_return_keyword_kind()) {
      case SgReturnStmt::e_return_keyword_return:
        break;
      case SgReturnStmt::e_return_keyword_co_return:
        ++co_return_count;
        break;
      default:
        ROSE_ABORT();
      }
    }

    if (SgInitializedName *name = isSgInitializedName(node)) {
      const SgStringList &type_tokens =
          name->get_source_type_qualification_tokens();
      if (name->get_name() == "size") {
        Sg_File_Info *source = name->get_startOfConstruct();
        ROSE_ASSERT(source != nullptr);
        ROSE_ASSERT(source->isCompilerGenerated());
        ROSE_ASSERT(source->isFrontendSpecific());
        ROSE_ASSERT(name->get_source_type_qualification_present());
        ROSE_ASSERT(!name->get_source_type_global_qualification());
        ROSE_ASSERT(type_tokens.size() == 1);
        ROSE_ASSERT(type_tokens.front() == "std::");
        ROSE_ASSERT(name->get_source_name_qualification_present());
        ROSE_ASSERT(!name->get_source_name_global_qualification());
        ROSE_ASSERT(name->get_source_name_qualification_tokens().empty());
        ROSE_ASSERT(name->get_name_qualification_length() == 0);
        ROSE_ASSERT(!name->get_global_qualification_required());
        ++semantic_size_declaration_count;

        if (source_name == "test2020_42.C") {
          if (SgVariableDeclaration *field =
                  isSgVariableDeclaration(name->get_parent())) {
            ROSE_ASSERT(field->get_variables().size() == 1);
            ROSE_ASSERT(field->get_variables().front() == name);
            ROSE_ASSERT(isSgAuxiliaryDeclarationList(field->get_parent()) !=
                        nullptr);
            ++semantic_size_field_count;
          } else {
            SgFunctionParameterList *parameters =
                isSgFunctionParameterList(name->get_parent());
            ROSE_ASSERT(parameters != nullptr);
            SgFunctionDeclaration *function =
                isSgFunctionDeclaration(parameters->get_parent());
            ROSE_ASSERT(function != nullptr);
            ROSE_ASSERT(function->get_name() == "buffer");
            ROSE_ASSERT(function->get_firstNondefiningDeclaration() != nullptr);
            ROSE_ASSERT(function->get_definingDeclaration() != nullptr);
            ROSE_ASSERT(function->get_firstNondefiningDeclaration() !=
                        function->get_definingDeclaration());
            if (function == function->get_firstNondefiningDeclaration()) {
              ++semantic_size_first_parameter_count;
            } else {
              ROSE_ASSERT(function == function->get_definingDeclaration());
              ++semantic_size_defining_parameter_count;
            }
          }
        }
      }
    }
  }

  if (source_name == "test2020_42.C") {
    ROSE_ASSERT(co_await_count == 2);
    ROSE_ASSERT(co_yield_count == 0);
    ROSE_ASSERT(await_void_type_count == 1);
    ROSE_ASSERT(await_nonvoid_type_count == 1);
    ROSE_ASSERT(co_return_count == 0);
    ROSE_ASSERT(semantic_size_declaration_count == 3);
    ROSE_ASSERT(semantic_size_field_count == 1);
    ROSE_ASSERT(semantic_size_first_parameter_count == 1);
    ROSE_ASSERT(semantic_size_defining_parameter_count == 1);
  } else if (source_name == "test2020_43.C") {
    ROSE_ASSERT(co_await_count == 0);
    ROSE_ASSERT(co_yield_count == 1);
    ROSE_ASSERT(await_void_type_count == 1);
    ROSE_ASSERT(await_nonvoid_type_count == 0);
    ROSE_ASSERT(co_return_count == 0);
  } else if (source_name == "test2020_44.C") {
    ROSE_ASSERT(co_await_count == 0);
    ROSE_ASSERT(co_yield_count == 0);
    ROSE_ASSERT(await_void_type_count == 0);
    ROSE_ASSERT(await_nonvoid_type_count == 0);
    ROSE_ASSERT(co_return_count == 1);
  } else {
    ROSE_ABORT();
  }

  return backend(project);
}
