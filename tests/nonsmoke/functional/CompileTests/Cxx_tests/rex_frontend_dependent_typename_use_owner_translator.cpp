#include "RoseAst.h"
#include "rose.h"

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(frontendExitStatus(project) == 0);

  SgFunctionDeclaration *function = nullptr;
  for (SgNode *node : RoseAst(project)) {
    if (SgFunctionDeclaration *candidate = isSgFunctionDeclaration(node)) {
      if (candidate->get_name() == "rex_dependent_typename_use" &&
          candidate->get_definition() != nullptr &&
          candidate->get_file_info() != nullptr &&
          candidate->get_file_info()->get_line() > 0) {
        function = candidate;
      }
    }
  }

  ROSE_ASSERT(function != nullptr);
  ROSE_ASSERT(function->get_type_elaboration_required_for_return_type());

  SgTypedefDeclaration *dependentTypedef = nullptr;
  SgTypedefDeclaration *functionTypeAlias = nullptr;
  for (SgNode *node : RoseAst(project)) {
    if (SgTypedefDeclaration *candidate = isSgTypedefDeclaration(node)) {
      if (candidate->get_name() == "RexDependentTypedef") {
        ROSE_ASSERT(dependentTypedef == nullptr);
        dependentTypedef = candidate;
      } else if (candidate->get_name() == "RexDependentFunctionType") {
        ROSE_ASSERT(functionTypeAlias == nullptr);
        functionTypeAlias = candidate;
      }
    }
  }
  ROSE_ASSERT(dependentTypedef != nullptr);
  ROSE_ASSERT(dependentTypedef->get_type_elaboration_required_for_base_type());
  ROSE_ASSERT(functionTypeAlias != nullptr);
  SgFunctionType *functionType =
      isSgFunctionType(functionTypeAlias->get_base_type());
  ROSE_ASSERT(functionType != nullptr);
  SgFunctionParameterTypeList *functionArguments =
      functionType->get_argument_list();
  ROSE_ASSERT(functionArguments != nullptr);
  ROSE_ASSERT(
      functionArguments->get_argument_qualification_use_sites().size() == 1);
  SgFunctionTypeArgument *functionTypeArgument =
      functionArguments->get_argument_qualification_use_sites().front();
  ROSE_ASSERT(functionTypeArgument != nullptr);
  ROSE_ASSERT(functionTypeArgument->get_source_type_elaboration_required());
  ROSE_ASSERT(functionTypeArgument->get_source_type_qualification_present());

  size_t nestedTemplateTypeArguments = 0;
  for (SgNode *node : RoseAst(project)) {
    SgTemplateArgument *argument = isSgTemplateArgument(node);
    SgNonrealDecl *owner =
        argument != nullptr ? isSgNonrealDecl(argument->get_parent()) : nullptr;
    if (owner == nullptr || owner->get_name() != "RexDependentPair" ||
        argument->get_sourceSpelledType() == nullptr ||
        !argument->get_type_elaboration_required()) {
      continue;
    }
    ROSE_ASSERT(argument->get_argumentType() ==
                SgTemplateArgument::type_argument);
    ROSE_ASSERT(argument->get_explicitlySpecified());
    ROSE_ASSERT(argument->get_source_type_qualification_present());
    SgNonrealType *sourceNonreal =
        isSgNonrealType(argument->get_sourceSpelledType());
    ROSE_ASSERT(sourceNonreal != nullptr);
    SgNonrealDecl *sourceNonrealDeclaration =
        isSgNonrealDecl(sourceNonreal->get_declaration());
    ROSE_ASSERT(sourceNonrealDeclaration != nullptr);
    ROSE_ASSERT(sourceNonrealDeclaration->get_source_elaboration_kind() ==
                SgNonrealDecl::e_source_elaboration_typename);
    ++nestedTemplateTypeArguments;
  }
  ROSE_ASSERT(nestedTemplateTypeArguments == 1);

  SgFunctionParameterList *parameters = function->get_parameterList();
  ROSE_ASSERT(parameters != nullptr);
  ROSE_ASSERT(parameters->get_args().size() == 1);
  SgFunctionParameterList *parameterSyntax =
      function->get_parameterList_syntax();
  ROSE_ASSERT(parameterSyntax != nullptr);
  ROSE_ASSERT(parameterSyntax != parameters);
  ROSE_ASSERT(parameterSyntax->get_args().size() == 1);
  SgInitializedName *parameter = parameterSyntax->get_args().front();
  ROSE_ASSERT(parameter != nullptr);
  ROSE_ASSERT(parameter->get_name() == "value");
  ROSE_ASSERT(parameter->get_type_elaboration_required());
  ROSE_ASSERT(parameter->get_type_elaboration_required_for_type());

  SgInitializedName *local = nullptr;
  for (SgNode *node : RoseAst(function->get_definition())) {
    SgInitializedName *candidate = isSgInitializedName(node);
    if (candidate != nullptr && candidate->get_name() == "local") {
      ROSE_ASSERT(local == nullptr);
      local = candidate;
    }
  }
  ROSE_ASSERT(local != nullptr);
  ROSE_ASSERT(local->get_type_elaboration_required());
  ROSE_ASSERT(local->get_type_elaboration_required_for_type());

  size_t sourceElaboratedDependentConstructions = 0;
  SgCastExp *dependentTypenameCast = nullptr;
  for (SgNode *node : RoseAst(project)) {
    SgConstructorInitializer *constructor = isSgConstructorInitializer(node);
    if (constructor == nullptr || !constructor->get_need_name() ||
        !constructor->get_type_elaboration_required()) {
      continue;
    }
    ROSE_ASSERT(constructor->get_expression_type() != nullptr);
    ROSE_ASSERT(constructor->get_explicit_name_qualification_present());
    ROSE_ASSERT(constructor->get_source_type_elaboration_kind() ==
                SgNonrealDecl::e_source_elaboration_typename);
    ++sourceElaboratedDependentConstructions;
  }
  ROSE_ASSERT(sourceElaboratedDependentConstructions == 1);

  size_t sourceElaboratedDependentCasts = 0;
  for (SgNode *node : RoseAst(project)) {
    SgCastExp *cast = isSgCastExp(node);
    if (cast == nullptr || cast->get_source_type() == nullptr ||
        cast->get_source_type_elaboration_kind() !=
            SgNonrealDecl::e_source_elaboration_typename) {
      continue;
    }
    ROSE_ASSERT(cast->get_cast_type() == SgCastExp::e_static_cast);
    ROSE_ASSERT(cast->get_explicit_name_qualification_present());
    ROSE_ASSERT(cast->get_type_elaboration_required());
    ROSE_ASSERT(dependentTypenameCast == nullptr);
    dependentTypenameCast = cast;
    ++sourceElaboratedDependentCasts;
  }
  ROSE_ASSERT(sourceElaboratedDependentCasts == 1);
  ROSE_ASSERT(dependentTypenameCast != nullptr);

  SgInitializedName *qualifiedElaboratedTag = nullptr;
  for (SgNode *node : RoseAst(project)) {
    SgInitializedName *candidate = isSgInitializedName(node);
    if (candidate != nullptr &&
        candidate->get_name() == "rex_qualified_elaborated_tag") {
      ROSE_ASSERT(qualifiedElaboratedTag == nullptr);
      qualifiedElaboratedTag = candidate;
    }
  }
  ROSE_ASSERT(qualifiedElaboratedTag != nullptr);
  ROSE_ASSERT(qualifiedElaboratedTag->get_type_elaboration_required());
  ROSE_ASSERT(qualifiedElaboratedTag->get_type_elaboration_required_for_type());
  ROSE_ASSERT(qualifiedElaboratedTag->get_source_type_qualification_present());
  ROSE_ASSERT(qualifiedElaboratedTag->get_source_type_global_qualification());
  ROSE_ASSERT(
      qualifiedElaboratedTag->get_source_type_qualification_tokens().empty());
  ROSE_ASSERT(qualifiedElaboratedTag->get_cxx_source_type() == nullptr);
  ROSE_ASSERT(
      isSgClassType(qualifiedElaboratedTag->get_type()->findBaseType()) !=
      nullptr);

  SgInitializedName *elaboratedTemplateTag = nullptr;
  for (SgNode *node : RoseAst(project)) {
    SgInitializedName *candidate = isSgInitializedName(node);
    if (candidate != nullptr &&
        candidate->get_name() == "rex_elaborated_template_tag") {
      ROSE_ASSERT(elaboratedTemplateTag == nullptr);
      elaboratedTemplateTag = candidate;
    }
  }
  ROSE_ASSERT(elaboratedTemplateTag != nullptr);
  ROSE_ASSERT(elaboratedTemplateTag->get_type_elaboration_required());
  ROSE_ASSERT(elaboratedTemplateTag->get_source_type_qualification_present());
  SgNonrealType *elaboratedTemplateType =
      isSgNonrealType((elaboratedTemplateTag->get_cxx_source_type() != nullptr
                           ? elaboratedTemplateTag->get_cxx_source_type()
                           : elaboratedTemplateTag->get_type())
                          ->findBaseType());
  ROSE_ASSERT(elaboratedTemplateType != nullptr);
  SgNonrealDecl *elaboratedTemplateDeclaration =
      isSgNonrealDecl(elaboratedTemplateType->get_declaration());
  ROSE_ASSERT(elaboratedTemplateDeclaration != nullptr);
  ROSE_ASSERT(elaboratedTemplateDeclaration->get_source_elaboration_kind() ==
              SgNonrealDecl::e_source_elaboration_class);

  size_t dependentStorageDeclarators = 0;
  for (SgNode *node : RoseAst(project)) {
    SgInitializedName *initializedName = isSgInitializedName(node);
    if (initializedName == nullptr ||
        initializedName->get_name() != "storage") {
      continue;
    }
    if (initializedName->get_file_info() == nullptr ||
        initializedName->get_file_info()->get_line() <= 0) {
      continue;
    }
    SgArrayType *arrayType = isSgArrayType(initializedName->get_type());
    ROSE_ASSERT(arrayType != nullptr);
    SgSizeOfOp *bound = isSgSizeOfOp(arrayType->get_index());
    ROSE_ASSERT(bound != nullptr);
    ROSE_ASSERT(!bound->get_type_elaboration_required());
    ROSE_ASSERT(bound->get_source_type_elaboration_kind() ==
                SgNonrealDecl::e_source_elaboration_none);
    ++dependentStorageDeclarators;
  }
  ROSE_ASSERT(dependentStorageDeclarators == 2);

  size_t plainSizeof = 0;
  size_t elaboratedSizeof = 0;
  size_t plainAlignof = 0;
  size_t elaboratedAlignof = 0;
  size_t plainCasts = 0;
  size_t elaboratedCasts = 0;
  SgCastExp *plainTagCast = nullptr;
  SgCastExp *elaboratedTagCast = nullptr;
  for (SgNode *node : RoseAst(project)) {
    if (SgSizeOfOp *operation = isSgSizeOfOp(node)) {
      SgClassType *operandType =
          isSgClassType(operation->get_operand_type()->findBaseType());
      if (operandType == nullptr ||
          operandType->get_name() != "RexTypeOperandElaboration") {
        continue;
      }
      if (operation->get_type_elaboration_required()) {
        ROSE_ASSERT(operation->get_source_type_elaboration_kind() ==
                    SgNonrealDecl::e_source_elaboration_struct);
        ++elaboratedSizeof;
      } else {
        ROSE_ASSERT(operation->get_source_type_elaboration_kind() ==
                    SgNonrealDecl::e_source_elaboration_none);
        ++plainSizeof;
      }
    } else if (SgAlignOfOp *operation = isSgAlignOfOp(node)) {
      SgClassType *operandType =
          isSgClassType(operation->get_operand_type()->findBaseType());
      if (operandType == nullptr ||
          operandType->get_name() != "RexTypeOperandElaboration") {
        continue;
      }
      if (operation->get_type_elaboration_required()) {
        ROSE_ASSERT(operation->get_source_type_elaboration_kind() ==
                    SgNonrealDecl::e_source_elaboration_struct);
        ++elaboratedAlignof;
      } else {
        ROSE_ASSERT(operation->get_source_type_elaboration_kind() ==
                    SgNonrealDecl::e_source_elaboration_none);
        ++plainAlignof;
      }
    } else if (SgCastExp *operation = isSgCastExp(node)) {
      SgClassType *sourceType =
          operation->get_source_type() != nullptr
              ? isSgClassType(operation->get_source_type()->findBaseType())
              : nullptr;
      if (sourceType == nullptr ||
          sourceType->get_name() != "RexTypeOperandElaboration") {
        continue;
      }
      if (operation->get_type_elaboration_required()) {
        ROSE_ASSERT(operation->get_source_type_elaboration_kind() ==
                    SgNonrealDecl::e_source_elaboration_struct);
        ROSE_ASSERT(elaboratedTagCast == nullptr);
        elaboratedTagCast = operation;
        ++elaboratedCasts;
      } else {
        ROSE_ASSERT(operation->get_source_type_elaboration_kind() ==
                    SgNonrealDecl::e_source_elaboration_none);
        ROSE_ASSERT(plainTagCast == nullptr);
        plainTagCast = operation;
        ++plainCasts;
      }
    }
  }
  ROSE_ASSERT(plainSizeof == 1 && elaboratedSizeof == 1);
  ROSE_ASSERT(plainAlignof == 1 && elaboratedAlignof == 1);
  ROSE_ASSERT(plainCasts == 1 && elaboratedCasts == 1);
  ROSE_ASSERT(plainTagCast != nullptr && elaboratedTagCast != nullptr);

  auto directSourceTypeSpelling = [](SgCastExp *cast) {
    ROSE_ASSERT(cast != nullptr);
    ROSE_ASSERT(cast->get_source_type() != nullptr);
    SgStatement *useSite = SageInterface::getEnclosingStatement(cast);
    SgSourceFile *sourceFile = SageInterface::getEnclosingSourceFile(cast);
    SgScopeStatement *scope = SageInterface::getScope(useSite);
    ROSE_ASSERT(useSite != nullptr && sourceFile != nullptr &&
                scope != nullptr);
    SgUnparse_Info info;
    info.set_current_source_file(sourceFile);
    info.set_current_scope(scope);
    info.set_template_argument_qualification_context(useSite);
    info.set_reference_node_for_qualification(cast);
    info.set_SkipClassDefinition();
    info.set_SkipEnumDefinition();
    return Rose::StringUtility::trim(
        cast->get_source_type()->unparseToString(&info));
  };
  const std::string dependentTypenameSpelling =
      directSourceTypeSpelling(dependentTypenameCast);
  const std::string plainTagSpelling = directSourceTypeSpelling(plainTagCast);
  const std::string elaboratedTagSpelling =
      directSourceTypeSpelling(elaboratedTagCast);
  if (dependentTypenameSpelling !=
          "typename RexDependentTypenameOwner<T>::type" ||
      plainTagSpelling != "RexTypeOperandElaboration*" ||
      elaboratedTagSpelling != "struct RexTypeOperandElaboration*") {
    fprintf(stderr,
            "REX_TEST_INVARIANT[direct-source-type-unparse]: "
            "dependent=%s plain=%s elaborated=%s\n",
            dependentTypenameSpelling.c_str(), plainTagSpelling.c_str(),
            elaboratedTagSpelling.c_str());
    ROSE_ABORT();
  }

  AstTests::runAllTests(project);
  return backend(project);
}
