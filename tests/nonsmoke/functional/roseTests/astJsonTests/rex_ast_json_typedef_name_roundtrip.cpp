#include "rose.h"
#include "sageAstJsonPrivate.h"

#include <stdexcept>
#include <string>

namespace {

SgSourceFile *sourceFile(SgProject *project) {
  if (project == nullptr || project->numberOfFiles() != 1) {
    throw std::runtime_error(
        "AST JSON typedef/name round trip requires one source file");
  }
  SgSourceFile *file = isSgSourceFile(&project->get_file(0));
  if (file == nullptr) {
    throw std::runtime_error(
        "AST JSON typedef/name round trip input is not a source file");
  }
  return file;
}

SgTypedefDeclaration *findTypedef(SgSourceFile *file, const std::string &name) {
  SgTypedefDeclaration *result = nullptr;
  for (SgNode *node : NodeQuery::querySubTree(file, V_SgTypedefDeclaration)) {
    SgTypedefDeclaration *declaration = isSgTypedefDeclaration(node);
    if (declaration == nullptr || declaration->get_name().getString() != name) {
      continue;
    }
    if (result != nullptr) {
      throw std::runtime_error(
          "AST JSON typedef/name round trip has duplicate typedef targets");
    }
    result = declaration;
  }
  if (result == nullptr) {
    throw std::runtime_error(
        "AST JSON typedef/name round trip has no requested typedef");
  }
  return result;
}

SgInitializedName *findName(SgSourceFile *file, const std::string &name) {
  SgInitializedName *result = nullptr;
  for (SgNode *node : NodeQuery::querySubTree(file, V_SgInitializedName)) {
    SgInitializedName *initialized_name = isSgInitializedName(node);
    if (initialized_name == nullptr ||
        initialized_name->get_name().getString() != name) {
      continue;
    }
    if (result != nullptr) {
      throw std::runtime_error(
          "AST JSON typedef/name round trip has duplicate name targets");
    }
    result = initialized_name;
  }
  if (result == nullptr) {
    throw std::runtime_error(
        "AST JSON typedef/name round trip has no requested name");
  }
  return result;
}

SgTemplateClassDeclaration *
findDefiningClassTemplate(SgNode *root, const std::string &name,
                          const std::string &context) {
  // A class template is a SgTemplateClassDeclaration, never a bare
  // SgTemplateDeclaration: the latter only identifies template-template
  // parameter surfaces (clang-frontend-decl.cpp template-identity clones),
  // while NodeQuery matches exact variants only.
  SgTemplateClassDeclaration *defining = nullptr;
  size_t matches = 0;
  for (SgNode *node :
       NodeQuery::querySubTree(root, V_SgTemplateClassDeclaration)) {
    SgTemplateClassDeclaration *declaration =
        isSgTemplateClassDeclaration(node);
    if (declaration == nullptr || declaration->get_name().getString() != name) {
      continue;
    }
    ++matches;
    if (declaration->get_definingDeclaration() == declaration) {
      if (defining != nullptr) {
        throw std::runtime_error(context +
                                 " has duplicate defining class templates");
      }
      defining = declaration;
    }
  }
  if (defining == nullptr) {
    throw std::runtime_error(context + " has no defining class template " +
                             name + " (" + std::to_string(matches) +
                             " matches)");
  }
  return defining;
}

void requireNttpHolderTemplate(SgNode *root, const std::string &context) {
  // No bare SgTemplateDeclaration can appear for this input: only
  // template-template parameters own one, and the specimen declares none.
  // Its presence would prove a spurious synthesis; its absence is required.
  if (!NodeQuery::querySubTree(root, V_SgTemplateDeclaration).empty()) {
    throw std::runtime_error(
        context + " synthesized an unexpected bare SgTemplateDeclaration");
  }
  SgTemplateClassDeclaration *defining =
      findDefiningClassTemplate(root, "RexJsonNttpHolder", context);
  const SgTemplateParameterPtrList &parameters =
      defining->get_templateParameters();
  if (parameters.size() != 1) {
    throw std::runtime_error(context +
                             " class template does not own exactly one NTTP");
  }
  SgTemplateParameter *parameter = isSgTemplateParameter(parameters.front());
  if (parameter == nullptr || parameter->get_parameterType() !=
                                  SgTemplateParameter::nontype_parameter) {
    throw std::runtime_error(context +
                             " class template parameter is not an NTTP");
  }
  if (!isSgTypeInt(parameter->get_type())) {
    throw std::runtime_error(context +
                             " NTTP does not carry the exact int type");
  }
  const auto *first =
      isSgTemplateClassDeclaration(defining->get_firstNondefiningDeclaration());
  ROSE_ASSERT(first != nullptr && first->get_templateParameters().size() == 1);
  ROSE_ASSERT(
      parameter->get_canonicalNonTypeParameter() ==
      first->get_templateParameters().front()->get_canonicalNonTypeParameter());
  SgInitializedName *nttp = parameter->get_initializedName();
  if (nttp == nullptr || nttp->get_name().getString() != "rex_json_n" ||
      !isSgTypeInt(nttp->get_typeptr())) {
    throw std::runtime_error(
        context + " NTTP identity is not the exact rex_json_n of int type");
  }
  if (parameter->get_defaultExpressionParameter() != nullptr) {
    throw std::runtime_error(context +
                             " NTTP owns an unexpected default expression");
  }
  SgClassDefinition *class_definition = defining->get_definition();
  SgTemplateClassDefinition *definition =
      isSgTemplateClassDefinition(class_definition);
  if (definition == nullptr || definition->get_declaration() != defining) {
    throw std::runtime_error(
        context + " class template definition does not name its declaration");
  }
  SgTemplateInstantiationDecl *found = nullptr;
  size_t instantiation_matches = 0;
  for (SgNode *node :
       NodeQuery::querySubTree(root, V_SgTemplateInstantiationDecl)) {
    SgTemplateInstantiationDecl *instantiation =
        isSgTemplateInstantiationDecl(node);
    if (instantiation == nullptr ||
        instantiation->get_name().getString() != "RexJsonNttpHolder<7>") {
      continue;
    }
    ++instantiation_matches;
    // The frontend owns a defining/nondefining instantiation pair; identity
    // checks run on the defining copy, never on an assumed single node.
    if (instantiation->get_definingDeclaration() == instantiation) {
      if (found != nullptr) {
        throw std::runtime_error(
            context + " has duplicate defining RexJsonNttpHolder<7>");
      }
      found = instantiation;
    }
  }
  if (found == nullptr) {
    throw std::runtime_error(
        context + " lost the defining RexJsonNttpHolder<7> instantiation (" +
        std::to_string(instantiation_matches) + " matches)");
  }
  SgTemplateClassDeclaration *linked =
      isSgTemplateClassDeclaration(found->get_templateDeclaration());
  // The instantiation must reference the same template family present in
  // this tree: the defining declaration, or the first nondefining one that
  // ROSE cross-references point at. A foreign or dangling pointer fails.
  SgTemplateClassDeclaration *first_nondefining =
      isSgTemplateClassDeclaration(defining->get_firstNondefiningDeclaration());
  if (linked == nullptr ||
      linked->get_name().getString() != "RexJsonNttpHolder" ||
      (linked != defining && linked != first_nondefining)) {
    throw std::runtime_error(
        context + " instantiation does not name the exact template family");
  }
  const auto &arguments = found->get_semanticTemplateArguments();
  if (arguments.size() != 1) {
    throw std::runtime_error(
        context + " instantiation does not own exactly one template argument");
  }
  SgTemplateArgument *argument = arguments.front();
  SgIntVal *value =
      isSgIntVal(argument != nullptr ? argument->get_expression() : nullptr);
  if (argument == nullptr || argument->get_parent() != found ||
      value == nullptr || value->get_parent() != argument ||
      value->get_value() != 7 || !isSgTypeInt(argument->get_type())) {
    throw std::runtime_error(
        context + " instantiation argument is not the exact int constant 7");
  }
}

void requireAliasTemplate(SgSourceFile *file) {
  auto *base = findTypedef(file, "rex_json_alias_t");
  bool found = false;
  for (SgNode *node :
       NodeQuery::querySubTree(file, V_SgTemplateTypedefDeclaration)) {
    auto *declaration = isSgTemplateTypedefDeclaration(node);
    if (declaration->get_name().getString() != "RexJsonAlias")
      continue;
    found = true;
    if (declaration->get_base_type() != base->get_type() ||
        declaration->get_type() == nullptr ||
        declaration->get_type()->get_declaration() != declaration) {
      throw std::runtime_error(
          "AST JSON alias template lost exact type identity");
    }
  }
  if (!found)
    throw std::runtime_error("AST JSON alias template is missing");
}

} // namespace

int main(int argc, char **argv) {
  using namespace Rose::AstJson;

  SgProject *project = frontend(argc, argv);
  if (project == nullptr || frontendExitStatus(project) != 0) {
    throw std::runtime_error(
        "AST JSON typedef/name round trip frontend failed");
  }
  SgSourceFile *file = sourceFile(project);
  requireAliasTemplate(file);
  constexpr Checkpoint checkpoint = Checkpoint::PreOmpConstruction;
  std::string json_text = buildJson(file, checkpoint, file);
  AstFileRecord ast = parseAstFileJson(json_text, checkpointName(checkpoint));
  SgSourceFile *copy = reconstructSourceFile(ast, file);
  if (copy == nullptr) {
    throw std::runtime_error(
        "AST JSON typedef/name round trip reconstructed as null");
  }

  // Typedef chains reconstruct with exact bases and wired canonical wrappers.
  SgTypedefDeclaration *alias = findTypedef(copy, "rex_json_alias_t");
  SgTypedefDeclaration *base = findTypedef(copy, "rex_json_base_t");
  SgTypedefType *alias_base = isSgTypedefType(alias->get_base_type());
  if (alias_base == nullptr || alias_base->get_declaration() != base) {
    throw std::runtime_error(
        "AST JSON typedef chain base does not name its exact declaration");
  }
  if (!isSgTypeInt(base->get_base_type())) {
    throw std::runtime_error(
        "AST JSON typedef chain does not bottom out in the exact int type");
  }
  if (alias->get_type() == nullptr ||
      alias->get_type()->get_declaration() != alias ||
      base->get_type() == nullptr ||
      base->get_type()->get_declaration() != base) {
    throw std::runtime_error(
        "AST JSON reconstructed typedefs lack wired canonical wrappers");
  }

  // Initialized names reconstruct with exact types, and their scope symbol
  // tables resolve to the reconstructed nodes.
  SgInitializedName *plain = findName(copy, "rex_json_plain");
  if (plain->get_typeptr() != alias->get_type()) {
    throw std::runtime_error(
        "AST JSON variable does not own the exact chained typedef type");
  }
  SgScopeStatement *scope = plain->get_scope();
  if (scope == nullptr) {
    throw std::runtime_error("AST JSON reconstructed variable has no scope");
  }
  SgVariableSymbol *symbol = scope->lookup_variable_symbol(plain->get_name());
  if (symbol == nullptr || symbol->get_symbol_basis() != plain) {
    throw std::runtime_error(
        "AST JSON scope symbol table does not resolve the reconstructed name");
  }

  // The variable-length array bound survives as an exact array type.
  SgInitializedName *vla = findName(copy, "rex_json_vla");
  SgArrayType *array_type = isSgArrayType(vla->get_typeptr());
  if (array_type == nullptr) {
    throw std::runtime_error(
        "AST JSON variable-length array did not reconstruct an array type");
  }

  // The decltype variable reconstructs its exact decltype type.
  SgInitializedName *deduced = findName(copy, "rex_json_deduced");
  if (isSgDeclType(deduced->get_typeptr()) == nullptr) {
    throw std::runtime_error(
        "AST JSON decltype variable did not reconstruct a decltype type");
  }

  // The non-type template parameter declaration round-trips intact: the
  // class template, its NTTP identity, its definition edge, and the <7>
  // instantiation survive with exact identities. Checked on the original
  // frontend AST first so the reconstruction comparison cannot pass
  // vacuously.
  requireNttpHolderTemplate(file, "AST JSON original frontend");
  requireNttpHolderTemplate(copy, "AST JSON reconstructed");
  requireAliasTemplate(copy);
  return 0;
}
