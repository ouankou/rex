// Liao, 3/21/2022
// Demonstrate how to build a tempalte class
//
// SageBuilder contains the AST nodes/subtrees builders
// SageInterface contains any other AST utility tools
//-------------------------------------------------------------------
#include "rose.h"

using namespace SageBuilder;
using namespace SageInterface;

SgProject *tester_init(int argc, char *argv[], SgGlobal *&global_scope) {
  SgProject *project = frontend(argc, argv);

  global_scope = SageInterface::getFirstGlobalScope(project);
  ROSE_ASSERT(global_scope);

  SgSourceFile *sfile = SageInterface::getEnclosingSourceFile(global_scope);
  sfile->set_unparse_template_ast(true);

  return project;
}

SgTemplateClassDeclaration *
build_template_class(SgScopeStatement *scope, SgType *&t_par1_type,
                     SgTemplateClassDefinition *&t_class_defn) {
  // Each declaration surface owns an independent template parameter.  The
  // defining surface's type is also used by the members in its class body.
  SgTemplateType *nondefining_t_par1_type =
      SageBuilder::buildTemplateType(SgName("T"));
  t_par1_type = SageBuilder::buildTemplateType(SgName("T"));
  SgTemplateParameterPtrList tpl_param_list{SageBuilder::buildTemplateParameter(
      SgTemplateParameter::type_parameter, nondefining_t_par1_type,
      SgTemplateParameter::keyword_class)};
  SgTemplateParameterPtrList defining_tpl_param_list{
      SageBuilder::buildTemplateParameter(SgTemplateParameter::type_parameter,
                                          t_par1_type,
                                          SgTemplateParameter::keyword_class)};

  // 3 - build the template declaration
  SgTemplateArgumentPtrList alist;
  const SgName template_name("Element");
  SgTemplateClassDeclaration *canonical =
      SageBuilder::buildNondefiningTemplateClassDeclaration(
          SageBuilder::declaration_ownership::semanticAuxiliary(),
          template_name, SgClassDeclaration::e_class,
          SageBuilder::template_class_declaration_scopes::
              fromExactSemanticScope(scope),
          &tpl_param_list, &alist, &template_name);
  SgTemplateClassDeclaration *t_class_decl =
      SageBuilder::buildTemplateClassDeclaration(
          SageBuilder::declaration_ownership::sourceLexical(), template_name,
          SgClassDeclaration::e_class,
          SageBuilder::template_class_declaration_scopes::
              fromExactSemanticScope(scope),
          canonical, &tpl_param_list, &defining_tpl_param_list, &alist,
          &template_name);

  t_class_defn = isSgTemplateClassDefinition(t_class_decl->get_definition());

  return t_class_decl;
}

#if defined(CODE_TO_GENERATE)
template <class T> class Element {
private:
  static T Value;
  static bool Valid;

public:
  static void Set(T E) {
    Value = E;
    Valid = true;
  }
  static void Reset();
};

template <class T> bool Element<T>::Valid = false;

template <class T> T Element<T>::Value;

template <class T> void Element<T>::Reset() { Valid = false; }
#endif

int main(int argc, char *argv[]) {
  SgGlobal *global_scope;
  SgProject *project = tester_init(argc, argv, global_scope);

  SgScopeStatement *scope = global_scope;

  // 1 - Build the (defining) template class declaration

  SgType *t_par1_type = nullptr;
  SgTemplateClassDefinition *t_class_defn = nullptr;
  SgTemplateClassDeclaration *t_class_decl =
      build_template_class(scope, t_par1_type, t_class_defn);

  // 2 - Build member variables with initializers outside the class

  SgTemplateVariableDeclaration *var0_decl =
      SageBuilder::buildTemplateVariableDeclaration(
          SageBuilder::declaration_ownership::sourceLexical(), SgName("Value"),
          t_par1_type, nullptr, t_class_defn);
  var0_decl->get_declarationModifier().get_storageModifier().setStatic();
  ROSE_ASSERT(var0_decl->get_parent() == t_class_defn);

  SgTemplateVariableDeclaration *var1_decl =
      SageBuilder::buildTemplateVariableDeclaration(
          SageBuilder::declaration_ownership::sourceLexicalIn(scope),
          SgName("Value"), t_par1_type, nullptr, t_class_defn,
          SageBuilder::template_variable_entity_kind::primary_template,
          var0_decl->get_variables().front());
  ROSE_ASSERT(var1_decl->get_parent() == scope);
  ROSE_ASSERT(var1_decl->get_variables().front()->get_scope() == t_class_defn);

  SgTemplateVariableDeclaration *var2_decl =
      SageBuilder::buildTemplateVariableDeclaration(
          SageBuilder::declaration_ownership::sourceLexical(), SgName("Valid"),
          SageBuilder::buildBoolType(), nullptr, t_class_defn);
  var2_decl->get_declarationModifier().get_storageModifier().setStatic();
  ROSE_ASSERT(var2_decl->get_parent() == t_class_defn);

  SgType *var3_type = SageBuilder::buildBoolType();
  SgTemplateVariableDeclaration *var3_decl =
      SageBuilder::buildTemplateVariableDeclaration(
          SageBuilder::declaration_ownership::sourceLexicalIn(scope),
          SgName("Valid"), var3_type,
          SageBuilder::buildAssignInitializer(
              SageBuilder::buildBoolValExp(false), var3_type),
          t_class_defn,
          SageBuilder::template_variable_entity_kind::primary_template,
          var2_decl->get_variables().front());
  ROSE_ASSERT(var3_decl->get_parent() == scope);
  ROSE_ASSERT(var3_decl->get_variables().front()->get_scope() == t_class_defn);

  // 3 - Build a member functions defined inside the class (need to add a hidden
  // 1st non-defining)

  SgTemplateParameterPtrList mfnc0_tpl_params;
  SgFunctionParameterList *mfnc0_source_params =
      SageBuilder::buildFunctionParameterList(
          SageBuilder::buildInitializedName(SgName("E"), t_par1_type));
  SgFunctionParameterList *mfnc0_semantic_params =
      SageBuilder::buildSemanticFunctionParameterList(mfnc0_source_params);
  SgTemplateMemberFunctionDeclaration *mfnc0_decl =
      SageBuilder::buildNondefiningTemplateMemberFunctionDeclaration(
          SageBuilder::function_declaration_ownership::semanticAuxiliary(),
          "Set", buildVoidType(), mfnc0_semantic_params, t_class_defn, 0,
          &mfnc0_tpl_params, &mfnc0_tpl_params);

  mfnc0_decl->get_declarationModifier().get_storageModifier().setStatic();
  ROSE_ASSERT(mfnc0_decl->get_parent() ==
              t_class_defn->get_auxiliary_declarations());

  SgTemplateMemberFunctionDeclaration *mfnc1_decl =
      SageBuilder::buildDefiningTemplateMemberFunctionDeclaration(
          SageBuilder::function_declaration_ownership::sourceLexical(), "Set",
          buildVoidType(), mfnc0_source_params, t_class_defn, 0, mfnc0_decl,
          &mfnc0_tpl_params);

  mfnc1_decl->get_declarationModifier().get_storageModifier().setStatic();

  // TODO mfnc1_decl->get_definition() ...

  // 4 - Build a member functions defined outside the class

  SgTemplateParameterPtrList mfnc2_tpl_params;
  SgTemplateMemberFunctionDeclaration *mfnc2_decl =
      SageBuilder::buildNondefiningTemplateMemberFunctionDeclaration(
          SageBuilder::function_declaration_ownership::sourceLexical(), "Reset",
          buildVoidType(), SageBuilder::buildFunctionParameterList(),
          t_class_defn, 0, &mfnc2_tpl_params, &mfnc2_tpl_params);
  mfnc2_decl->get_declarationModifier().get_storageModifier().setStatic();

  SgTemplateMemberFunctionDeclaration *mfnc3_decl =
      SageBuilder::buildDefiningTemplateMemberFunctionDeclaration(
          SageBuilder::function_declaration_ownership::sourceLexicalIn(scope),
          "Reset", buildVoidType(), SageBuilder::buildFunctionParameterList(),
          t_class_defn, 0, mfnc2_decl, &mfnc2_tpl_params);
  ROSE_ASSERT(mfnc3_decl->get_parent() == scope);
  ROSE_ASSERT(mfnc3_decl->get_scope() == t_class_defn);

  // TODO mfnc3_decl->get_definition() ...

  AstTests::runAllTests(project);

  return backend(project);
}
