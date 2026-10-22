#include "rose.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "Outliner.hh"
#include "RoseAst.h"

namespace {

[[noreturn]] void fail(const std::string &reason) {
  std::cerr << "REX_TEST_ERROR[outliner-dependent-namespace-target-table]: "
            << reason << "\n";
  std::exit(1);
}

SgNamespaceSymbol *directNamespace(SgScopeStatement *scope,
                                   const SgName &name) {
  SgSymbolTable *table = scope != nullptr ? scope->get_symbol_table() : nullptr;
  SgNamespaceSymbol *symbol =
      table != nullptr ? table->find_namespace(name) : nullptr;
  if (symbol == nullptr || symbol->get_parent() != table ||
      !table->exists(symbol)) {
    fail("namespace is not published in the requested exact symbol table");
  }
  return symbol;
}

SgNamespaceDefinitionStatement *namespaceDefinition(SgNamespaceSymbol *symbol) {
  SgNamespaceDeclarationStatement *declaration =
      symbol != nullptr ? symbol->get_declaration() : nullptr;
  SgNamespaceDeclarationStatement *canonical =
      declaration != nullptr
          ? isSgNamespaceDeclarationStatement(
                declaration->get_firstNondefiningDeclaration())
          : nullptr;
  SgNamespaceDefinitionStatement *definition =
      canonical != nullptr ? canonical->get_definition() : nullptr;
  if (canonical == nullptr || symbol->get_symbol_basis() != canonical ||
      definition == nullptr) {
    fail("namespace symbol has no exact canonical declaration family");
  }
  return definition;
}

struct DependencySymbols {
  SgNamespaceSymbol *nameSpace;
  SgClassSymbol *value;
  SgFunctionSymbol *compute;
};

DependencySymbols directDependencySymbols(SgScopeStatement *owner) {
  SgNamespaceSymbol *namespaceSymbol =
      directNamespace(owner, SgName("rex_copy_dependency"));
  SgNamespaceDefinitionStatement *definition =
      namespaceDefinition(namespaceSymbol);
  SgSymbolTable *table = definition->get_symbol_table();
  SgClassSymbol *value =
      table != nullptr ? table->find_class(SgName("rex_copy_value")) : nullptr;
  SgFunctionSymbol *compute =
      table != nullptr ? table->find_function(SgName("rex_copy_compute"))
                       : nullptr;
  if (table == nullptr || table->get_parent() != definition ||
      value == nullptr || compute == nullptr || value->get_parent() != table ||
      compute->get_parent() != table || !table->exists(value) ||
      !table->exists(compute) || value->get_declaration() == nullptr ||
      compute->get_declaration() == nullptr ||
      value->get_declaration()->get_scope() != definition ||
      compute->get_declaration()->get_scope() != definition) {
    fail("class/function dependency is not published in the namespace's exact "
         "symbol table");
  }
  return {namespaceSymbol, value, compute};
}

void checkReopenedNamespaceLookup(SgProject *project) {
  SgFunctionDeclaration *ordinary[2];
  SgFunctionDeclaration *internal[2];
  SgTypedefDeclaration *aliases[2];
  SgEnumDeclaration *enumerations[2];
  SgInitializedName *variables[2];
  SgInitializedName *statics[2];
  SgGlobal *globals[2];
  const char *outputs[] = {"rex_namespace_source.cpp",
                           "rex_namespace_target.cpp"};
  for (size_t index = 0; index < 2; ++index) {
    globals[index] =
        SageBuilder::buildGeneratedSourceFile(outputs[index], project)
            ->get_globalScope();
    auto *first =
        SageBuilder::buildNamespaceDeclaration("rex_namespace", globals[index]);
    auto *reopened =
        SageBuilder::buildNamespaceDeclaration("rex_namespace", globals[index]);
    auto *fragment = reopened->get_definition();
    ROSE_ASSERT(fragment != first->get_definition() &&
                fragment->get_global_definition() == first->get_definition());
    auto *record = SageBuilder::buildNondefiningClassDeclaration(
        SageBuilder::declaration_ownership::semanticAuxiliary(), "rex_record",
        fragment);
    auto *definitionFragment =
        SageBuilder::buildNamespaceDeclaration("rex_namespace", globals[index])
            ->get_definition();
    auto *nested =
        SageBuilder::buildNamespaceDeclaration("rex_nested", fragment);
    auto *nestedReopened = SageBuilder::buildNamespaceDeclaration(
        "rex_nested", definitionFragment);
    auto *nestedSymbol =
        first->get_definition()->get_symbol_table()->find_namespace(
            SgName("rex_nested"));
    ROSE_ASSERT(nestedSymbol != nullptr &&
                nestedSymbol->get_parent() ==
                    first->get_definition()->get_symbol_table() &&
                nestedSymbol->get_declaration() == nested &&
                nested->get_scope() == fragment &&
                nested->get_parent() == fragment &&
                nestedReopened->get_scope() == definitionFragment &&
                nestedReopened->get_parent() == definitionFragment &&
                nestedReopened->get_firstNondefiningDeclaration() == nested &&
                nestedReopened->get_definition()->get_global_definition() ==
                    nested->get_definition());
    auto *definition = SageBuilder::buildDefiningClassDeclaration(
        SageBuilder::declaration_ownership::semanticAuxiliary(), "rex_record",
        definitionFragment, record);
    ROSE_ASSERT(definition->get_firstNondefiningDeclaration() == record &&
                record->get_definingDeclaration() == definition &&
                record->get_scope() == fragment &&
                definition->get_scope() == definitionFragment &&
                record->get_symbol_from_symbol_table()->get_parent() ==
                    first->get_definition()->get_symbol_table());
    aliases[index] = SageBuilder::buildTypedefDeclaration(
        SageBuilder::typedef_declaration_ownership::sourceLexical(),
        SgTypedefDeclaration::e_using, "rex_alias", record->get_type(),
        fragment);
    ROSE_ASSERT(aliases[index]->get_scope() == fragment &&
                aliases[index]->get_parent() == fragment &&
                aliases[index]->get_symbol_from_symbol_table()->get_parent() ==
                    first->get_definition()->get_symbol_table());
    auto *enumForward = SageBuilder::buildNondefiningEnumDeclaration_nfi(
        "rex_enum", false, fragment,
        SageBuilder::declaration_ownership::semanticAuxiliary(), nullptr);
    enumerations[index] = SageBuilder::buildEnumDeclaration_nfi(
        SageBuilder::declaration_ownership::sourceLexical(), "rex_enum", false,
        definitionFragment, enumForward);
    ROSE_ASSERT(enumerations[index]->get_scope() == definitionFragment &&
                enumerations[index]->get_parent() == definitionFragment &&
                enumerations[index]->get_firstNondefiningDeclaration() ==
                    enumForward &&
                enumForward->get_definingDeclaration() == enumerations[index] &&
                enumForward->get_scope() == fragment &&
                enumForward->get_symbol_from_symbol_table()->get_parent() ==
                    first->get_definition()->get_symbol_table());
    auto *variable = SageBuilder::buildSemanticAuxiliaryVariableDeclaration(
        "rex_variable", aliases[index]->get_type(), nullptr, fragment);
    variables[index] = SageInterface::getFirstInitializedName(variable);
    ROSE_ASSERT(
        variables[index]->get_scope() == fragment &&
        variables[index]->get_symbol_from_symbol_table()->get_parent() ==
            first->get_definition()->get_symbol_table());
    auto *redeclaration = SageBuilder::buildVariableRedeclaration_nfi(
        "rex_variable", aliases[index]->get_type(), nullptr, definitionFragment,
        variables[index], SgStorageModifier::e_extern);
    SageInterface::setSourcePositionForTransformation(redeclaration);
    SageInterface::appendStatement(redeclaration, definitionFragment);
    auto *redeclared = SageInterface::getFirstInitializedName(redeclaration);
    ROSE_ASSERT(redeclared->get_scope() == definitionFragment &&
                redeclared->get_prev_decl_item() == variables[index] &&
                redeclared->search_for_symbol_from_symbol_table() ==
                    variables[index]->get_symbol_from_symbol_table());
    auto *internalVariable = SageBuilder::buildVariableDeclaration_nfi(
        "rex_static", enumerations[index]->get_type(), nullptr, fragment,
        SgStorageModifier::e_static);
    SageInterface::setSourcePositionForTransformation(internalVariable);
    SageInterface::appendStatement(internalVariable, fragment);
    statics[index] = SageInterface::getFirstInitializedName(internalVariable);
    ROSE_ASSERT(statics[index]->get_scope() == fragment &&
                statics[index]->get_symbol_from_symbol_table()->get_parent() ==
                    first->get_definition()->get_symbol_table());
    ordinary[index] = SageBuilder::buildNondefiningFunctionDeclaration(
        SageBuilder::function_declaration_ownership::semanticAuxiliary(),
        "rex_ordinary",
        SageBuilder::buildPointerType(aliases[index]->get_type()),
        SageBuilder::buildFunctionParameterList_nfi(), fragment);
    internal[index] = SageBuilder::buildNondefiningFunctionDeclaration(
        SageBuilder::function_declaration_ownership::semanticAuxiliary(),
        "rex_internal", enumerations[index]->get_type(),
        SageBuilder::buildFunctionParameterList_nfi(), fragment, false, nullptr,
        SgStorageModifier::e_static);
    for (auto *declaration : {ordinary[index], internal[index]}) {
      auto *symbol = declaration->get_symbol_from_symbol_table();
      ROSE_ASSERT(declaration->get_scope() == fragment && symbol != nullptr &&
                  symbol->get_parent() ==
                      first->get_definition()->get_symbol_table());
    }
  }
  for (auto *source : {ordinary[0], internal[0]}) {
    auto *target = source == ordinary[0] ? ordinary[1] : internal[1];
    ROSE_ASSERT(
        SageBuilder::findAssociatedSymbolInTargetAST(source, globals[1]) ==
        target->get_symbol_from_symbol_table());
  }
  ROSE_ASSERT(
      SageBuilder::findAssociatedSymbolInTargetAST(aliases[0], globals[1]) ==
      aliases[1]->get_symbol_from_symbol_table());
  ROSE_ASSERT(SageBuilder::findAssociatedSymbolInTargetAST(enumerations[0],
                                                           globals[1]) ==
              enumerations[1]->get_symbol_from_symbol_table());
  ROSE_ASSERT(
      SageBuilder::findAssociatedSymbolInTargetAST(variables[0], globals[1]) ==
      variables[1]->get_symbol_from_symbol_table());
  ROSE_ASSERT(
      SageBuilder::findAssociatedSymbolInTargetAST(statics[0], globals[1]) ==
      statics[1]->get_symbol_from_symbol_table());
}

} // namespace

int main(int argc, char **argv) {
  std::vector<std::string> arguments(argv, argv + argc);
  Outliner::commandLineProcessing(arguments);
  SgProject *project = frontend(arguments);
  if (project == nullptr || project->get_fileList().empty()) {
    fail("frontend did not construct the source project");
  }
  AstTests::runAllTests(project);

  SgSourceFile *sourceFile = isSgSourceFile(project->get_fileList().front());
  SgGlobal *sourceGlobal =
      sourceFile != nullptr ? sourceFile->get_globalScope() : nullptr;
  DependencySymbols source = directDependencySymbols(sourceGlobal);

  Outliner::Result result(nullptr, nullptr, sourceFile, {});
  Outliner::Result copiedResult(result);
  if (copiedResult.file_ != sourceFile) {
    fail("copying an outlining result lost its exact source-file identity");
  }
  Outliner::Result emptyResult;
  if (emptyResult.decl_ != nullptr || emptyResult.call_ != nullptr ||
      emptyResult.file_ != nullptr || emptyResult.isValid()) {
    fail("a default outlining result contains an unpublished identity");
  }

  SgNamespaceSymbol *decoyOwner =
      directNamespace(sourceGlobal, SgName("rex_copy_decoy"));
  DependencySymbols decoy =
      directDependencySymbols(namespaceDefinition(decoyOwner));

  Outliner::outlineAll(project);
  AstTests::runAllTests(project);

  SgFunctionDeclaration *outlined = nullptr;
  for (SgNode *node :
       NodeQuery::querySubTree(project, V_SgFunctionDeclaration)) {
    SgFunctionDeclaration *function = isSgFunctionDeclaration(node);
    if (function != nullptr && function->get_definition() != nullptr &&
        function->get_name().getString().find("OUT_") == 0 &&
        SageInterface::getEnclosingSourceFile(function) != sourceFile) {
      if (outlined != nullptr && outlined != function) {
        fail("copied-file outlining produced more than one defining target");
      }
      outlined = function;
    }
  }
  SgSourceFile *targetFile =
      outlined != nullptr ? SageInterface::getEnclosingSourceFile(outlined)
                          : nullptr;
  SgGlobal *targetGlobal =
      targetFile != nullptr ? targetFile->get_globalScope() : nullptr;
  if (targetGlobal == nullptr || targetGlobal == sourceGlobal) {
    fail("outlined function has no independent copied-file global scope");
  }

  DependencySymbols target = directDependencySymbols(targetGlobal);
  const auto &sourceDeclarations =
      namespaceDefinition(source.nameSpace)->get_declarations();
  const auto &targetDeclarations =
      namespaceDefinition(target.nameSpace)->get_declarations();
  if (sourceDeclarations.size() != targetDeclarations.size()) {
    fail("copied namespace lost a written declaration");
  }
  size_t checkedPrototypes = 0;
  auto checkPrototype = [&](SgDeclarationStatement *sourceDeclaration,
                            SgDeclarationStatement *targetDeclaration) {
    auto *sourceFunction = isSgFunctionDeclaration(sourceDeclaration);
    auto *targetFunction = isSgFunctionDeclaration(targetDeclaration);
    if (sourceFunction == nullptr ||
        sourceFunction->get_definition() != nullptr)
      return;
    if (targetFunction == nullptr || targetFunction == sourceFunction ||
        !sourceFunction->get_translation_unit_source_order().has_value() ||
        sourceFunction->get_translation_unit_source_order() !=
            targetFunction->get_translation_unit_source_order() ||
        sourceFunction->get_firstNondefiningDeclaration() !=
            source.compute->get_declaration() ||
        targetFunction->get_firstNondefiningDeclaration() !=
            target.compute->get_declaration()) {
      fail("copied prototype lost its producer or canonical family identity");
    }
    ++checkedPrototypes;
  };
  for (size_t index = 0; index < sourceDeclarations.size(); ++index) {
    auto *sourceGroup =
        isSgDeclarationGroupStatement(sourceDeclarations[index]);
    if (sourceGroup == nullptr) {
      checkPrototype(sourceDeclarations[index], targetDeclarations[index]);
      continue;
    }
    auto *targetGroup =
        isSgDeclarationGroupStatement(targetDeclarations[index]);
    if (targetGroup == nullptr ||
        sourceGroup->get_translation_unit_source_order() !=
            targetGroup->get_translation_unit_source_order() ||
        sourceGroup->get_declarations().size() != 2 ||
        targetGroup->get_declarations().size() != 2) {
      fail("copied declaration group lost its producer or member identities");
    }
    for (size_t member = 0; member < 2; ++member) {
      if (sourceGroup->get_declarations()[member]->get_parent() !=
              sourceGroup ||
          targetGroup->get_declarations()[member]->get_parent() !=
              targetGroup) {
        fail("copied grouped prototype lost its structural owner");
      }
      checkPrototype(sourceGroup->get_declarations()[member],
                     targetGroup->get_declarations()[member]);
    }
  }
  if (checkedPrototypes != 4) {
    fail(
        "prototype identity checks did not cover standalone and grouped peers");
  }
  size_t projectedOwners = 0;
  for (SgDeclarationStatement *declaration : sourceGlobal->get_declarations()) {
    SgTemplateInstantiationDecl *sourceOwner =
        isSgTemplateInstantiationDecl(declaration);
    if (sourceOwner == nullptr ||
        sourceOwner->get_templateName() != SgName("rex_copy_owner") ||
        sourceOwner->get_definition() == nullptr) {
      continue;
    }
    SgTemplateClassDeclaration *sourcePrimary =
        sourceOwner->get_templateDeclaration();
    if (sourcePrimary == nullptr) {
      fail("class specialization has no typed primary template");
    }
    SgTemplateClassSymbol *targetPrimary =
        targetGlobal->get_symbol_table()->find_template_class(
            sourcePrimary->get_name(), &sourcePrimary->get_templateParameters(),
            &sourcePrimary->get_templateSpecializationArguments());
    if (targetPrimary == nullptr) {
      fail("class specialization has no exact target primary template");
    }
    SgClassSymbol *targetOwner =
        targetGlobal->get_symbol_table()->find_class_instantiation(
            isSgTemplateClassDeclaration(targetPrimary->get_declaration()),
            sourceOwner->get_semanticTemplateArguments());
    if (targetOwner == nullptr ||
        targetOwner->get_parent() != targetGlobal->get_symbol_table()) {
      fail("class specialization has no exact target symbol");
    }
    SgClassDeclaration *targetDefinition = isSgClassDeclaration(
        targetOwner->get_declaration()->get_definingDeclaration());
    if (targetDefinition == nullptr ||
        targetDefinition->get_definition() == nullptr ||
        SageBuilder::getTargetFileType(sourceOwner->get_type(), targetGlobal) !=
            targetOwner->get_type()) {
      fail("class specialization projection lost its exact target identity");
    }
    SgClassSymbol *sourceNested =
        sourceOwner->get_definition()->get_symbol_table()->find_class("nested");
    SgClassSymbol *targetNested =
        targetDefinition->get_definition()->get_symbol_table()->find_class(
            "nested");
    if (sourceNested == nullptr || targetNested == nullptr ||
        sourceNested == targetNested ||
        SageBuilder::getTargetFileType(sourceNested->get_type(),
                                       targetGlobal) !=
            targetNested->get_type()) {
      fail("nested-type projection selected the wrong class specialization");
    }
    ++projectedOwners;
  }
  if (projectedOwners != 2) {
    fail("nested-type projection did not check both class specializations");
  }
  if (target.nameSpace == source.nameSpace ||
      target.nameSpace == decoy.nameSpace || target.value == source.value ||
      target.value == decoy.value || target.compute == source.compute ||
      target.compute == decoy.compute) {
    fail("target dependency reused a source or same-spelled decoy symbol");
  }

  bool foundFunctionReference = false;
  bool foundClassType = false;
  RoseAst body(outlined->get_definition()->get_body());
  for (RoseAst::iterator node = body.begin(); node != body.end(); ++node) {
    if (SgFunctionRefExp *reference = isSgFunctionRefExp(*node)) {
      if (reference->get_symbol() != nullptr &&
          reference->get_symbol()->get_name() == SgName("rex_copy_compute")) {
        if (reference->get_symbol() != target.compute) {
          fail("moved function reference does not use the exact target symbol");
        }
        foundFunctionReference = true;
      }
    }
    if (SgInitializedName *name = isSgInitializedName(*node)) {
      SgClassType *type = isSgClassType(name->get_type());
      SgClassDeclaration *declaration =
          type != nullptr ? isSgClassDeclaration(type->get_declaration())
                          : nullptr;
      if (declaration != nullptr &&
          declaration->get_name() == SgName("rex_copy_value")) {
        if (declaration != target.value->get_declaration()) {
          fail("moved class use does not use the exact target declaration");
        }
        foundClassType = true;
      }
    }
  }
  if (!foundFunctionReference || !foundClassType) {
    fail("outlined body did not retain both dependent namespace uses");
  }
  checkReopenedNamespaceLookup(project);
  AstTests::runAllTests(project);
  return 0;
}
