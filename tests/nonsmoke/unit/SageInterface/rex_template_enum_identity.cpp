#include "rose.h"

#include "AstInterface_ROSE.h"
#include "astJson/sageAstJson.h"
#include "manglingSupport.h"

#include <cstdlib>
#include <map>
#include <set>
#include <string>

namespace {
SgEnumDeclaration *canonicalEnum(SgEnumDeclaration *declaration) {
  ROSE_ASSERT(declaration != nullptr);
  auto *canonical =
      isSgEnumDeclaration(declaration->get_firstNondefiningDeclaration());
  ROSE_ASSERT(canonical != nullptr);
  return canonical;
}

void checkEnumIdentity(SgNode *root) {
  unsigned fullSpecializations = 0, selectedPartials = 0, primaryInstances = 0;
  std::set<std::string> variableNames, declarationNames, variableSignatures;
  for (SgNode *node :
       NodeQuery::querySubTree(root, V_SgTemplateVariableDeclaration)) {
    auto *declaration = isSgTemplateVariableDeclaration(node);
    ROSE_ASSERT(declaration->get_variables().size() == 1);
    if (declaration->get_variables().front()->get_name() !=
        "rex_semantic_variable")
      continue;
    auto *name = declaration->get_variables().front();
    variableNames.insert(name->get_mangled_name().getString());
    declarationNames.insert(declaration->get_mangled_name().getString());
    variableSignatures.insert(AstInterface::GetVariableSignature(name));
    auto *selected = isSgTemplateVariableDeclaration(
        declaration->get_specializedTemplateDeclaration());
    const auto &semantic = declaration->get_semanticTemplateArguments();
    if (selected == nullptr) {
      ROSE_ASSERT(semantic.empty());
      continue;
    }
    ROSE_ASSERT(semantic.size() == 2);
    auto parameters = declaration->get_templateParameters();
    auto arguments = semantic;
    ROSE_ASSERT(name->get_scope()->lookup_template_variable_symbol(
                    name->get_name(), &parameters, &arguments) ==
                name->get_symbol_from_symbol_table());
    for (auto *argument : semantic) {
      ROSE_ASSERT(argument != nullptr && argument->get_parent() == declaration);
      ROSE_ASSERT(argument->get_argumentType() ==
                  SgTemplateArgument::type_argument);
    }
    if (declaration->get_specialization() ==
        SgDeclarationStatement::e_partial_specialization) {
      ROSE_ASSERT(declaration->get_deducedTemplateArguments().empty());
    } else if (declaration->get_specialization() ==
               SgDeclarationStatement::e_specialization) {
      const auto &written = declaration->get_templateSpecializationArguments();
      ROSE_ASSERT(written.size() == 1 && written.front() != semantic.front());
      ROSE_ASSERT(isSgTypedefType(written.front()->get_sourceSpelledType()));
      ROSE_ASSERT(isSgTypeInt(semantic[0]->get_type()));
      ROSE_ASSERT(semantic[0]->get_type() == semantic[1]->get_type());
      ++fullSpecializations;
    } else if (selected->get_specialization() ==
               SgDeclarationStatement::e_partial_specialization) {
      ROSE_ASSERT(declaration->get_deducedTemplateArguments().size() == 1);
      ROSE_ASSERT(isSgPointerType(semantic[0]->get_type())->get_base_type() ==
                  semantic[1]->get_type());
      ++selectedPartials;
    } else {
      ROSE_ASSERT(declaration->get_deducedTemplateArguments().size() == 2);
      ROSE_ASSERT(isSgTypeDouble(semantic[0]->get_type()));
      ROSE_ASSERT(semantic[0]->get_type() == semantic[1]->get_type());
      ++primaryInstances;
    }
  }
  ROSE_ASSERT(variableNames.size() == 5 && declarationNames.size() == 5 &&
              variableSignatures.size() == 5);
  ROSE_ASSERT(fullSpecializations && selectedPartials && primaryInstances);
  for (SgNode *node : NodeQuery::querySubTree(root, V_SgNonrealRefExp)) {
    auto *reference = isSgNonrealRefExp(node);
    if (reference->get_resolved_variable_declaration() != nullptr) {
      auto *name = SageInterface::requireResolvedVariableTemplateReference(
          reference, "rex_template_enum_identity");
      ROSE_ASSERT(AstInterface::GetVariableSignature(reference) ==
                  AstInterface::GetVariableSignature(name));
      ROSE_ASSERT(mangleExpression(reference) ==
                  name->get_mangled_name().getString());
    }
  }
  for (SgNode *node : NodeQuery::querySubTree(root, V_SgVarRefExp)) {
    auto *reference = isSgVarRefExp(node);
    auto *name = reference->get_symbol()->get_declaration();
    if (isSgTemplateVariableDeclaration(name->get_parent()) != nullptr)
      ROSE_ASSERT(mangleExpression(reference) ==
                  name->get_mangled_name().getString());
  }
  std::set<std::string> innerNamespaces, usingNamespaces, sharedAliases;
  unsigned aliasInstantiations = 0;
  for (SgNode *node :
       NodeQuery::querySubTree(root, V_SgNamespaceDeclarationStatement)) {
    auto *declaration = isSgNamespaceDeclarationStatement(node);
    if (declaration->get_name() == "inner")
      innerNamespaces.insert(AstInterface::GetVariableSignature(node));
  }
  for (SgNode *node :
       NodeQuery::querySubTree(root, V_SgUsingDirectiveStatement))
    usingNamespaces.insert(AstInterface::GetVariableSignature(node));
  for (SgNode *node : NodeQuery::querySubTree(root, V_SgTypedefDeclaration)) {
    auto *declaration = isSgTypedefDeclaration(node);
    const auto signature = AstInterface::GetVariableSignature(node);
    ROSE_ASSERT(!signature.empty());
    if (declaration->get_name() == "RexSharedAlias")
      sharedAliases.insert(signature);
    if (isSgTemplateInstantiationTypedefDeclaration(declaration))
      ++aliasInstantiations;
  }
  ROSE_ASSERT(innerNamespaces.size() == 2 && usingNamespaces.size() == 2 &&
              sharedAliases.size() == 2 && aliasInstantiations >= 2);
  std::map<SgClassType *, std::string> classSignatures;
  std::map<std::string, SgClassType *> signatureTypes;
  unsigned sharedRecords = 0;
  for (SgNode *node : NodeQuery::querySubTree(root, V_SgClassDeclaration)) {
    auto *declaration = isSgClassDeclaration(node);
    auto *type = isSgClassType(declaration->get_type());
    ROSE_ASSERT(type != nullptr);
    const std::string signature =
        AstInterface::GetVariableSignature(AstNodePtr(declaration));
    if (auto *definition = declaration->get_definition())
      ROSE_ASSERT(AstInterface::GetVariableSignature(definition) == signature);
    const auto [typeEntry, newType] = classSignatures.emplace(type, signature);
    ROSE_ASSERT(newType || typeEntry->second == signature);
    if (newType && declaration->get_name() == "RexSharedRecord")
      ++sharedRecords;
    const auto [signatureEntry, newSignature] =
        signatureTypes.emplace(signature, type);
    ROSE_ASSERT(newSignature || signatureEntry->second == type);
  }
  ROSE_ASSERT(sharedRecords == 2);
  unsigned maximumValues = 0;
  std::string anonymousSignatureA;
  std::string anonymousSignatureB;
  for (SgNode *node : NodeQuery::querySubTree(root, V_SgEnumVal)) {
    auto *value = isSgEnumVal(node);
    ROSE_ASSERT(value->get_name() != "REX_ENUM_WARM");
    ROSE_ASSERT(value->get_name() != "REX_ENUM_MAXIMUM");
    if (value->get_name() == "maximum") {
      ROSE_ASSERT(static_cast<unsigned long long>(value->get_value()) ==
                  0xffffffffffffffffULL);
      ++maximumValues;
    }
  }
  ROSE_ASSERT(maximumValues != 0);
  std::set<SgEnumDeclaration *> declarations;
  for (SgNode *node : NodeQuery::querySubTree(root, V_SgEnumDeclaration)) {
    auto *declaration = isSgEnumDeclaration(node);
    declarations.insert(canonicalEnum(declaration));
    if (declaration->get_isUnNamed()) {
      auto *scope = isSgNamespaceDefinitionStatement(declaration->get_scope());
      if (scope != nullptr) {
        const SgName &scopeName = scope->get_namespaceDeclaration()->get_name();
        if (scopeName == "rex_enum_signature_a") {
          anonymousSignatureA =
              AstInterface::GetVariableSignature(AstNodePtr(declaration));
        } else if (scopeName == "rex_enum_signature_b") {
          anonymousSignatureB =
              AstInterface::GetVariableSignature(AstNodePtr(declaration));
        }
      }
    }
    for (auto *enumerator : declaration->get_enumerators()) {
      ROSE_ASSERT(enumerator->get_enum_constant_value().has_value());
      SgEnumFieldSymbol symbol(enumerator);
      SgEnumVal *reference = SageBuilder::buildEnumVal(&symbol);
      ROSE_ASSERT(reference->get_value() ==
                  *enumerator->get_enum_constant_value());
      if (enumerator->get_name() == "after_boundary") {
        ROSE_ASSERT(static_cast<unsigned long long>(reference->get_value()) ==
                    0x8000000000000000ULL);
      }
      delete reference;
    }
  }
  ROSE_ASSERT(!anonymousSignatureA.empty() && !anonymousSignatureB.empty() &&
              anonymousSignatureA != anonymousSignatureB);
  const std::map<std::string, std::string> expected = {
      {"Shade", "Tone"},      {"Color", "Hue"},        {"AliasShade", "Tone"},
      {"Shades", "Tone"},     {"NestedShade", "Tone"}, {"SelectedRank", "Rank"},
      {"SelectedHue", "Hue"},
  };
  std::set<std::string> observed;
  for (SgNode *node : NodeQuery::querySubTree(root, V_SgTemplateParameter)) {
    auto *parameter = isSgTemplateParameter(node);
    if (parameter->get_parameterType() !=
        SgTemplateParameter::nontype_parameter) {
      continue;
    }
    SgInitializedName *name = parameter->get_initializedName();
    ROSE_ASSERT(name != nullptr && name->get_parent() == parameter);
    const std::string spelling = name->get_name().getString();
    ROSE_ASSERT(expected.count(spelling) == 1);
    ROSE_ASSERT(name->get_type() == parameter->get_type());
    SgType *type = name->get_type();
    if (spelling == "AliasShade") {
      auto *alias = isSgTypedefType(type);
      ROSE_ASSERT(alias != nullptr && alias->get_declaration() != nullptr);
      type = alias->get_base_type();
    }
    auto *enumType = isSgEnumType(type);
    ROSE_ASSERT(enumType != nullptr);
    auto *declaration =
        canonicalEnum(isSgEnumDeclaration(enumType->get_declaration()));
    ROSE_ASSERT(declarations.count(declaration) == 1);
    ROSE_ASSERT(declaration->get_name() == expected.at(spelling));
    if (spelling == "SelectedRank") {
      ROSE_ASSERT(isSgClassDefinition(declaration->get_scope()) != nullptr);
    } else {
      auto *scope = isSgNamespaceDefinitionStatement(declaration->get_scope());
      ROSE_ASSERT(scope != nullptr);
      ROSE_ASSERT(scope->get_namespaceDeclaration()->get_name() ==
                  "rex_palette");
    }
    ROSE_ASSERT(parameter->get_is_parameter_pack() == (spelling == "Shades"));
    observed.insert(spelling);
  }
  ROSE_ASSERT(observed.size() == expected.size());
}
} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr && project->numberOfFiles() == 1);
  checkEnumIdentity(project);
  SgTreeCopy copy;
  auto *copied = isSgProject(project->copy(copy));
  ROSE_ASSERT(copied != nullptr && copied != project);
  checkEnumIdentity(copied);
  auto *copied_files = copied->get_fileList_ptr();
  SageInterface::deleteAST(copied,
                           SageInterface::DeleteAstMode::kRequireIsolated);
  ROSE_ASSERT(!SgNode::isLiveNode(copied_files));

  auto *file = isSgSourceFile(&project->get_file(0));
  ROSE_ASSERT(file != nullptr);
  Rose::AstJson::Options options;
  options.outputDirectory = "rex_template_enum_identity_json";
  ROSE_ASSERT(setenv("REX_AST_JSON_CHECKPOINT", "pre-omp-construction", 1) ==
              0);
  SgSourceFile *restored = Rose::AstJson::roundTripSourceFile(
      file, Rose::AstJson::Checkpoint::PreOmpConstruction, options);
  ROSE_ASSERT(restored != nullptr && restored != file);
  checkEnumIdentity(restored);
  project->skipfinalCompileStep(true);
  const int status = backend(project);
  auto *project_files = project->get_fileList_ptr();
  SageInterface::deleteAST(project,
                           SageInterface::DeleteAstMode::kRequireIsolated);
  ROSE_ASSERT(!SgNode::isLiveNode(project_files));
  return status;
}
