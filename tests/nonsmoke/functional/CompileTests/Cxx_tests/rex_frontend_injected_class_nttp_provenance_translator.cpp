#include "rose.h"

#include <array>
#include <set>

namespace {
bool hasExactSemanticProvenance(SgExpression *expression) {
  if (expression == nullptr) {
    return false;
  }
  const std::array<Sg_File_Info *, 4> positions = {
      expression->get_file_info(), expression->get_startOfConstruct(),
      expression->get_endOfConstruct(), expression->get_operatorPosition()};
  for (Sg_File_Info *position : positions) {
    if (position == nullptr || position->get_parent() != expression ||
        position->isShared() || !position->isCompilerGenerated() ||
        !position->isFrontendSpecific() || position->isTransformation() ||
        position->isSourcePositionUnavailableInFrontend() ||
        !position->isOutputInCodeGeneration() ||
        position->get_file_id() != Sg_File_Info::COMPILER_GENERATED_FILE_ID ||
        position->get_physical_file_id() !=
            Sg_File_Info::COMPILER_GENERATED_FILE_ID) {
      return false;
    }
  }
  return true;
}

class ContractTraversal : public AstSimpleProcessing {
public:
  std::set<SgTemplateParameterVal *> values;
  std::set<SgNonrealDecl *> syntaxDeclarations;
  std::set<SgTemplateMemberFunctionDeclaration *> methods;

  void visit(SgNode *node) override {
    if (auto *method = isSgTemplateMemberFunctionDeclaration(node)) {
      auto *owner = isSgTemplateClassDefinition(method->get_scope());
      auto *record =
          owner != nullptr
              ? isSgTemplateClassDeclaration(owner->get_declaration())
              : nullptr;
      if (record != nullptr && record->get_name() == "RexInjectedClass" &&
          method->get_name() == "rex_clone") {
        ROSE_ASSERT(methods.insert(method).second);
      }
    }
    SgTemplateParameterVal *value = isSgTemplateParameterVal(node);
    if (value == nullptr ||
        value->get_template_parameter_name().getString() != "RexNttpValue") {
      return;
    }

    SgTemplateArgument *argument = isSgTemplateArgument(value->get_parent());
    SgNonrealDecl *syntax =
        argument != nullptr ? isSgNonrealDecl(argument->get_parent()) : nullptr;
    SgDeclarationScope *declarationScope =
        syntax != nullptr ? isSgDeclarationScope(syntax->get_parent())
                          : nullptr;
    if (declarationScope == nullptr) {
      return;
    }

    auto *method =
        isSgTemplateMemberFunctionDeclaration(declarationScope->get_parent());
    SgTemplateClassDeclaration *templateDeclaration =
        isSgTemplateClassDeclaration(syntax->get_templateDeclaration());
    ROSE_ASSERT(method != nullptr);
    ROSE_ASSERT(methods.count(method) == 1);
    ROSE_ASSERT(method->get_function_declarator_scope() == declarationScope);
    ROSE_ASSERT(declarationScope->get_semantic_outer_scope() == nullptr);
    ROSE_ASSERT(declarationScope->get_scope() == method->get_scope());
    ROSE_ASSERT(std::count(declarationScope->getDeclarationList().begin(),
                           declarationScope->getDeclarationList().end(),
                           syntax) == 1);
    ROSE_ASSERT(syntax->get_scope() == declarationScope);
    ROSE_ASSERT(templateDeclaration != nullptr);
    ROSE_ASSERT(templateDeclaration->get_name() == "RexInjectedClass");
    ROSE_ASSERT(argument->get_explicitlySpecified());
    ROSE_ASSERT(value->get_parent() == argument);
    ROSE_ASSERT(isSgTypeInt(value->get_type()) != nullptr);
    ROSE_ASSERT(hasExactSemanticProvenance(value));
    ROSE_ASSERT(values.insert(value).second);
    ROSE_ASSERT(syntaxDeclarations.insert(syntax).second);
  }
};
} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(frontendExitStatus(project) == 0);

  ContractTraversal contract;
  contract.traverse(project, preorder);
  ROSE_ASSERT(contract.methods.size() == 2);
  auto *first = isSgTemplateMemberFunctionDeclaration(
      (*contract.methods.begin())->get_firstNondefiningDeclaration());
  ROSE_ASSERT(first != nullptr);
  auto *defining =
      isSgTemplateMemberFunctionDeclaration(first->get_definingDeclaration());
  ROSE_ASSERT(defining != nullptr && defining != first);
  ROSE_ASSERT(contract.methods.count(first) == 1);
  ROSE_ASSERT(contract.methods.count(defining) == 1);
  ROSE_ASSERT(defining->get_firstNondefiningDeclaration() == first);
  auto *signature = isSgMemberFunctionType(first->get_type());
  ROSE_ASSERT(signature != nullptr && defining->get_type() == signature);
  ROSE_ASSERT(signature->get_arguments().size() == 1);
  auto *classType = isSgClassType(signature->get_class_type());
  ROSE_ASSERT(classType != nullptr);
  auto *record = isSgTemplateClassDeclaration(classType->get_declaration());
  ROSE_ASSERT(record != nullptr);
  ROSE_ASSERT(record->get_firstNondefiningDeclaration() == record);
  ROSE_ASSERT(first->get_parameterList() != defining->get_parameterList());
  for (auto *method : contract.methods) {
    ROSE_ASSERT(method->get_type_syntax() == nullptr);
    ROSE_ASSERT(!method->get_type_syntax_is_available());
    ROSE_ASSERT(method->get_parameterList_syntax() == nullptr);
    auto *parameters = method->get_parameterList();
    ROSE_ASSERT(parameters != nullptr && parameters->get_parent() == method);
    ROSE_ASSERT(parameters->get_args().size() == 1);
    ROSE_ASSERT(parameters->get_args()[0]->get_parent() == parameters);
    ROSE_ASSERT(parameters->get_args()[0]->get_type() ==
                signature->get_arguments()[0]);
  }

  ROSE_ASSERT(signature->get_return_type() == signature->get_arguments()[0]);
  auto *pointer = isSgPointerType(signature->get_return_type());
  ROSE_ASSERT(pointer != nullptr);
  auto *nonreal = isSgNonrealType(pointer->get_base_type());
  ROSE_ASSERT(nonreal != nullptr);
  auto *declaration = isSgNonrealDecl(nonreal->get_declaration());
  ROSE_ASSERT(declaration != nullptr);
  ROSE_ASSERT(declaration->get_type() == nonreal);
  ROSE_ASSERT(declaration->get_templateDeclaration() == record);
  ROSE_ASSERT(declaration->get_tpl_args().size() == 1);
  auto *argument = declaration->get_tpl_args()[0];
  ROSE_ASSERT(argument != nullptr && argument->get_parent() == declaration);
  auto *value = isSgTemplateParameterVal(argument->get_expression());
  ROSE_ASSERT(value != nullptr);
  auto *parameter = value->get_template_parameter();
  ROSE_ASSERT(parameter != nullptr);
  auto *owner = isSgTemplateClassDeclaration(parameter->get_parent());
  ROSE_ASSERT(owner != nullptr);
  ROSE_ASSERT(owner->get_firstNondefiningDeclaration() == record);
  ROSE_ASSERT(owner->get_templateParameters().size() == 1);
  ROSE_ASSERT(owner->get_templateParameters()[0] == parameter);
  // Every retained argument must belong to an adopted signature type use.
  ROSE_ASSERT(contract.values.size() == 1);
  ROSE_ASSERT(contract.values.count(value) == 1);
  ROSE_ASSERT(contract.syntaxDeclarations.size() == 1);
  ROSE_ASSERT(contract.syntaxDeclarations.count(declaration) == 1);
  return 0;
}
