#include "astJson/sageAstJson.h"
#include "rose.h"

#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

namespace {

std::string takeJsonDir(int &argc, char **argv) {
  const char prefix[] = "--rex-ast-json-dir=";
  std::string result;
  std::vector<char *> filtered;
  filtered.reserve(argc);
  filtered.push_back(argv[0]);

  for (int i = 1; i < argc; ++i) {
    if (std::strncmp(argv[i], prefix, sizeof(prefix) - 1) == 0) {
      result = argv[i] + sizeof(prefix) - 1;
      continue;
    }
    filtered.push_back(argv[i]);
  }

  argc = static_cast<int>(filtered.size());
  for (int i = 0; i < argc; ++i) {
    argv[i] = filtered[i];
  }
  argv[argc] = nullptr;
  return result;
}

SgSourceFile *firstSourceFile(SgProject *project) {
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(project->numberOfFiles() == 1);
  SgSourceFile *file = isSgSourceFile(&project->get_file(0));
  ROSE_ASSERT(file != nullptr);
  return file;
}

size_t verifySourceSpelledStaticMembers(SgNode *root) {
  size_t count = 0;
  size_t dependent_argument_count = 0;
  size_t concrete_argument_count = 0;
  size_t newline_header_count = 0;
  size_t space_header_count = 0;
  for (SgNode *node : NodeQuery::querySubTree(root, V_SgVariableDeclaration)) {
    SgVariableDeclaration *decl = isSgVariableDeclaration(node);
    ROSE_ASSERT(decl != nullptr);
    if (decl->get_variables().size() != 1 ||
        decl->get_variables().front()->get_name() != "value" ||
        decl->get_sourceSpelledTemplateHeaders().empty()) {
      continue;
    }

    ROSE_ASSERT(decl->get_sourceSpelledTemplateHeaders().size() == 1);
    SgTemplateParameterList *header =
        decl->get_sourceSpelledTemplateHeaders().front();
    ROSE_ASSERT(header != nullptr);
    if (header->get_source_header_separator() ==
        SgTemplateParameterList::e_source_header_separator_newline) {
      ++newline_header_count;
    } else {
      ROSE_ASSERT(header->get_source_header_separator() ==
                  SgTemplateParameterList::e_source_header_separator_space);
      ++space_header_count;
    }
    SgNamedType *source_owner = decl->get_sourceSpelledTemplateOwnerType();
    ROSE_ASSERT(source_owner != nullptr);
    SgNonrealType *source_surface = isSgNonrealType(source_owner);
    ROSE_ASSERT(source_surface != nullptr);
    SgNonrealDecl *source_declaration =
        isSgNonrealDecl(source_surface->get_declaration());
    ROSE_ASSERT(source_declaration != nullptr);
    ROSE_ASSERT(source_declaration->get_tpl_args().size() == 1);
    SgTemplateArgument *argument = source_declaration->get_tpl_args().front();
    ROSE_ASSERT(argument != nullptr);
    ROSE_ASSERT(argument->get_argumentType() ==
                SgTemplateArgument::nontype_argument);
    SgExpression *argument_expression = argument->get_expression();
    ROSE_ASSERT(argument_expression != nullptr);
    if (isSgIntVal(argument_expression) != nullptr) {
      ++concrete_argument_count;
    } else {
      ROSE_ASSERT(isSgTemplateParameterVal(argument_expression) != nullptr);
      ++dependent_argument_count;
    }
    SgInitializedName *name = decl->get_variables().front();
    ROSE_ASSERT(name->get_source_name_qualification_present());
    ROSE_ASSERT(!name->get_source_name_global_qualification());
    const SgStringList &name_tokens =
        name->get_source_name_qualification_tokens();
    ROSE_ASSERT(name_tokens.size() == 2);
    ROSE_ASSERT(name_tokens.front() == "rex_ast_json_scope::");
    ++count;
  }
  ROSE_ASSERT(count == 2);
  ROSE_ASSERT(dependent_argument_count == 1);
  ROSE_ASSERT(concrete_argument_count == 1);
  ROSE_ASSERT(newline_header_count == 1);
  ROSE_ASSERT(space_header_count == 1);
  return count;
}

void verifyTypedStaticMemberParameterOwners(SgNode *root) {
  size_t count = 0;
  for (SgNode *node : NodeQuery::querySubTree(root, V_SgVariableDeclaration)) {
    SgVariableDeclaration *declaration = isSgVariableDeclaration(node);
    ROSE_ASSERT(declaration != nullptr);
    if (declaration->get_variables().size() != 1 ||
        declaration->get_variables().front()->get_name() != "typed_value" ||
        declaration->get_sourceSpelledTemplateHeaders().empty()) {
      continue;
    }

    ROSE_ASSERT(declaration->get_sourceSpelledTemplateHeaders().size() == 1);
    SgTemplateParameterList *header =
        declaration->get_sourceSpelledTemplateHeaders().front();
    ROSE_ASSERT(header != nullptr);
    ROSE_ASSERT(header->get_parent() == declaration);
    ROSE_ASSERT(header->get_args().size() == 1);
    SgTemplateParameter *parameter = header->get_args().front();
    ROSE_ASSERT(parameter != nullptr);
    ROSE_ASSERT(parameter->get_parent() == header);
    ROSE_ASSERT(parameter->get_parameterType() ==
                SgTemplateParameter::type_parameter);

    SgTemplateType *parameter_type = isSgTemplateType(parameter->get_type());
    ROSE_ASSERT(parameter_type != nullptr);
    ROSE_ASSERT(parameter_type->get_name() == "WrittenType");
    ROSE_ASSERT(parameter_type->get_template_parameter() == parameter);

    SgNonrealType *owner_type =
        isSgNonrealType(declaration->get_sourceSpelledTemplateOwnerType());
    ROSE_ASSERT(owner_type != nullptr);
    SgNonrealDecl *owner_declaration =
        isSgNonrealDecl(owner_type->get_declaration());
    ROSE_ASSERT(owner_declaration != nullptr);
    ROSE_ASSERT(owner_declaration->get_tpl_args().size() == 1);
    SgTemplateArgument *owner_argument =
        owner_declaration->get_tpl_args().front();
    ROSE_ASSERT(owner_argument != nullptr);
    ROSE_ASSERT(owner_argument->get_argumentType() ==
                SgTemplateArgument::type_argument);
    SgTemplateType *owner_use_type =
        isSgTemplateType(owner_argument->get_type());
    ROSE_ASSERT(owner_use_type != nullptr);
    ROSE_ASSERT(owner_use_type->get_name() == "WrittenType");
    ROSE_ASSERT(owner_use_type->get_template_parameter() == parameter);
    ROSE_ASSERT(owner_use_type->get_template_parameter_position() ==
                parameter_type->get_template_parameter_position());
    ROSE_ASSERT(owner_use_type->get_template_parameter_depth() ==
                parameter_type->get_template_parameter_depth());
    ROSE_ASSERT(owner_use_type->get_canonical_source_identity() ==
                parameter_type->get_canonical_source_identity());
    ++count;
  }
  ROSE_ASSERT(count == 1);
}

void verifySourceSpelledTemplateArgumentType(SgNode *root) {
  size_t count = 0;
  size_t unqualified_count = 0;
  size_t qualified_count = 0;
  for (SgNode *node : NodeQuery::querySubTree(root, V_SgTemplateArgument)) {
    SgTemplateArgument *argument = isSgTemplateArgument(node);
    ROSE_ASSERT(argument != nullptr);
    SgTypedefType *source_type =
        isSgTypedefType(argument->get_sourceSpelledType());
    if (source_type == nullptr ||
        source_type->get_name() != "SourceSpelledAlias") {
      continue;
    }

    ROSE_ASSERT(isSgClassType(argument->get_type()) != nullptr);
    ROSE_ASSERT(argument->get_type() != argument->get_sourceSpelledType());
    ROSE_ASSERT(argument->get_source_type_qualification_present());
    ROSE_ASSERT(!argument->get_source_type_global_qualification());
    const SgStringList &tokens =
        argument->get_source_type_qualification_tokens();
    if (tokens.empty()) {
      ++unqualified_count;
    } else {
      ROSE_ASSERT(tokens.size() == 1);
      ROSE_ASSERT(tokens.front() == "rex_ast_json_scope::");
      ++qualified_count;
    }
    ++count;
  }
  ROSE_ASSERT(count >= 2);
  ROSE_ASSERT(unqualified_count >= 1);
  ROSE_ASSERT(qualified_count >= 1);
}

void verifySourceSpelledVariableInstantiation(SgNode *root) {
  size_t count = 0;
  for (SgNode *node : NodeQuery::querySubTree(root, V_SgInitializedName)) {
    SgInitializedName *name = isSgInitializedName(node);
    ROSE_ASSERT(name != nullptr);
    if (name->get_name() != "source_spelled_holder") {
      continue;
    }

    SgType *base_type = name->get_type() != nullptr
                            ? name->get_type()->findBaseType()
                            : nullptr;
    SgClassType *class_type = isSgClassType(base_type);
    ROSE_ASSERT(class_type != nullptr);
    SgTemplateInstantiationDecl *instantiation =
        isSgTemplateInstantiationDecl(class_type->get_declaration());
    ROSE_ASSERT(instantiation != nullptr);
    ROSE_ASSERT(instantiation->get_templateArguments().size() == 1);

    SgTemplateArgument *argument =
        instantiation->get_templateArguments().front();
    ROSE_ASSERT(argument != nullptr);
    SgClassType *semantic_type = isSgClassType(argument->get_type());
    ROSE_ASSERT(semantic_type != nullptr);
    SgClassDeclaration *semantic_declaration =
        isSgClassDeclaration(semantic_type->get_declaration());
    ROSE_ASSERT(semantic_declaration != nullptr);
    ROSE_ASSERT(semantic_declaration->get_isUnNamed());

    // Canonical template arguments are semantic identities shared by every
    // use of the instantiation and therefore must not acquire one use site's
    // typedef spelling.  The initialized name owns an independent, typed
    // source graph for the exact template-id written at this declaration.
    ROSE_ASSERT(argument->get_sourceSpelledType() == nullptr);
    SgNonrealType *written_type = isSgNonrealType(name->get_cxx_source_type());
    ROSE_ASSERT(written_type != nullptr);
    ROSE_ASSERT(written_type != name->get_type());
    SgNonrealDecl *written_declaration =
        isSgNonrealDecl(written_type->get_declaration());
    ROSE_ASSERT(written_declaration != nullptr);
    ROSE_ASSERT(written_declaration->get_tpl_args().size() == 1);
    SgTemplateArgument *written_argument =
        written_declaration->get_tpl_args().front();
    ROSE_ASSERT(written_argument != nullptr);
    ROSE_ASSERT(
        SageInterface::templateArgumentEquivalence(written_argument, argument));
    SgTypedefType *source_type =
        isSgTypedefType(written_argument->get_sourceSpelledType());
    ROSE_ASSERT(source_type != nullptr);
    ROSE_ASSERT(source_type->get_name() == "SourceSpelledAlias");
    ++count;
  }
  ROSE_ASSERT(count == 1);
}

void verifySourceSpelledDeclarationHeaders(SgNode *root) {
  auto verify_header = [](SgDeclarationStatement *owner,
                          SgNode *header_node) -> size_t {
    SgTemplateParameterList *header = isSgTemplateParameterList(header_node);
    ROSE_ASSERT(header != nullptr);
    ROSE_ASSERT(header->get_parent() == owner);
    ROSE_ASSERT(
        header->get_source_header_separator() ==
        (header->get_args().empty()
             ? SgTemplateParameterList::e_source_header_separator_space
             : SgTemplateParameterList::e_source_header_separator_newline));
    for (SgTemplateParameter *parameter : header->get_args()) {
      ROSE_ASSERT(parameter != nullptr);
      ROSE_ASSERT(parameter->get_parent() == header);
    }
    return header->get_args().size();
  };

  size_t function_nonempty = 0;
  size_t function_empty = 0;
  size_t function_immediate = 0;
  size_t function_source_owned_specialization = 0;
  for (SgNode *node : NodeQuery::querySubTree(root, V_SgFunctionDeclaration)) {
    SgFunctionDeclaration *decl = isSgFunctionDeclaration(node);
    ROSE_ASSERT(decl != nullptr);
    if (decl->get_name() != "function") {
      continue;
    }
    if (decl->get_sourceSpelledTemplateHeaders().empty()) {
      SgTemplateMemberFunctionDeclaration *template_member =
          isSgTemplateMemberFunctionDeclaration(decl);
      if (template_member != nullptr &&
          template_member->get_templateParameters().size() == 1) {
        SgTemplateParameter *parameter =
            template_member->get_templateParameters().front();
        ROSE_ASSERT(parameter != nullptr);
        ROSE_ASSERT(parameter->get_parent() == template_member);
        ++function_immediate;
      }
      continue;
    }
    ROSE_ASSERT(decl->get_sourceSpelledTemplateHeaders().size() == 1);
    if (verify_header(decl, decl->get_sourceSpelledTemplateHeaders().front()) ==
        0) {
      SgTemplateInstantiationMemberFunctionDecl *specialization =
          isSgTemplateInstantiationMemberFunctionDecl(decl);
      if (specialization != nullptr &&
          specialization->get_specialization() ==
              SgDeclarationStatement::e_specialization &&
          specialization->get_templateDeclaration() == nullptr &&
          specialization->get_definition() != nullptr) {
        ++function_source_owned_specialization;
      }
      ++function_empty;
    } else {
      ++function_nonempty;
    }
  }
  // A class-template member pattern owns its immediate typed parameter list.
  // An ordinary member specialization owns one exact empty source header and
  // a typed specialization role; the source header emits its sole surface,
  // while the typed role validates its semantics without duplicating it.
  ROSE_ASSERT(function_immediate == 1);
  ROSE_ASSERT(function_source_owned_specialization == 1);
  ROSE_ASSERT(function_nonempty == 0);
  ROSE_ASSERT(function_empty == 1);

  size_t class_nonempty = 0;
  size_t class_empty = 0;
  for (SgNode *node : NodeQuery::querySubTree(root, V_SgClassDeclaration)) {
    SgClassDeclaration *decl = isSgClassDeclaration(node);
    ROSE_ASSERT(decl != nullptr);
    if (decl->get_name() != "Nested" ||
        decl->get_sourceSpelledTemplateHeaders().empty()) {
      continue;
    }
    ROSE_ASSERT(decl->get_sourceSpelledTemplateHeaders().size() == 1);
    if (verify_header(decl, decl->get_sourceSpelledTemplateHeaders().front()) ==
        0) {
      ++class_empty;
    } else {
      ++class_nonempty;
    }
  }
  ROSE_ASSERT(class_nonempty == 2);
  ROSE_ASSERT(class_empty == 1);
}

void verifySourceSpelledTemplateTemplateParameter(SgNode *root) {
  size_t count = 0;
  for (SgNode *node : NodeQuery::querySubTree(root, V_SgClassDeclaration)) {
    SgClassDeclaration *declaration = isSgClassDeclaration(node);
    ROSE_ASSERT(declaration != nullptr);
    if (declaration->get_name() != "Nested") {
      continue;
    }
    for (SgTemplateParameterList *header :
         declaration->get_sourceSpelledTemplateHeaders()) {
      ROSE_ASSERT(header != nullptr);
      ROSE_ASSERT(header->get_parent() == declaration);
      if (header->get_args().size() != 1 ||
          header->get_args().front()->get_parameterType() !=
              SgTemplateParameter::template_parameter) {
        continue;
      }

      SgTemplateParameter *parameter = header->get_args().front();
      SgTemplateDeclaration *semantic_declaration =
          isSgTemplateDeclaration(parameter->get_templateDeclaration());
      SgTemplateDeclaration *source_declaration =
          parameter->get_sourceSpelledTemplateDeclaration();
      SgDeclarationScope *semantic_owner =
          semantic_declaration != nullptr
              ? isSgDeclarationScope(semantic_declaration->get_parent())
              : nullptr;
      ROSE_ASSERT(semantic_declaration != nullptr);
      ROSE_ASSERT(semantic_owner != nullptr);
      ROSE_ASSERT(semantic_declaration->get_scope() == semantic_owner);
      ROSE_ASSERT(source_declaration != nullptr);
      ROSE_ASSERT(source_declaration != semantic_declaration);
      ROSE_ASSERT(source_declaration->get_parent() == parameter);
      ROSE_ASSERT(source_declaration->get_scope() != nullptr);

      const SgTemplateParameterPtrList &semantic_parameters =
          semantic_declaration->get_templateParameters();
      const SgTemplateParameterPtrList &source_parameters =
          source_declaration->get_templateParameters();
      ROSE_ASSERT(semantic_parameters.size() == 2);
      ROSE_ASSERT(source_parameters.size() == 2);
      SgTemplateType *semantic_type =
          isSgTemplateType(semantic_parameters[0]->get_type());
      SgTemplateType *source_type =
          isSgTemplateType(source_parameters[0]->get_type());
      ROSE_ASSERT(semantic_type != nullptr);
      ROSE_ASSERT(source_type != nullptr);
      ROSE_ASSERT(semantic_type->get_name() == "SemanticType");
      ROSE_ASSERT(source_type->get_name() == "WrittenType");
      SgInitializedName *semantic_count =
          semantic_parameters[1]->get_initializedName();
      SgInitializedName *source_count =
          source_parameters[1]->get_initializedName();
      ROSE_ASSERT(semantic_count != nullptr);
      ROSE_ASSERT(source_count != nullptr);
      ROSE_ASSERT(semantic_count->get_name() == "SemanticCount");
      ROSE_ASSERT(source_count->get_name() == "WrittenCount");
      ROSE_ASSERT(source_count->get_parent() == source_parameters[1]);
      ROSE_ASSERT(source_count->get_scope() ==
                  SageBuilder::getNonrealDeclarationScope(source_declaration));
      ++count;
    }
  }
  ROSE_ASSERT(count == 1);
}

void verifyStructuralSourceQualifications(SgNode *root) {
  size_t qualified_functions = 0;
  for (SgNode *node : NodeQuery::querySubTree(root, V_SgFunctionDeclaration)) {
    SgFunctionDeclaration *decl = isSgFunctionDeclaration(node);
    ROSE_ASSERT(decl != nullptr);
    if (decl->get_name() != "function" ||
        !decl->get_source_name_qualification_present()) {
      continue;
    }
    ROSE_ASSERT(!decl->get_source_name_global_qualification());
    const SgStringList &tokens = decl->get_source_name_qualification_tokens();
    ROSE_ASSERT(tokens.size() == 2);
    ROSE_ASSERT(tokens.front() == "rex_ast_json_scope::");
    ++qualified_functions;
  }
  ROSE_ASSERT(qualified_functions == 2);

  size_t qualified_aliases = 0;
  for (SgNode *node : NodeQuery::querySubTree(root, V_SgTypedefDeclaration)) {
    SgTypedefDeclaration *decl = isSgTypedefDeclaration(node);
    ROSE_ASSERT(decl != nullptr);
    if (decl->get_name() != "RexAstJsonQualifiedAlias") {
      continue;
    }
    ROSE_ASSERT(decl->get_source_base_type_qualification_present());
    ROSE_ASSERT(!decl->get_source_base_type_global_qualification());
    const SgStringList &tokens =
        decl->get_source_base_type_qualification_tokens();
    ROSE_ASSERT(tokens.size() == 1);
    ROSE_ASSERT(tokens.front() == "rex_ast_json_scope::");
    ++qualified_aliases;
  }
  ROSE_ASSERT(qualified_aliases == 1);

  size_t qualified_values = 0;
  for (SgNode *node : NodeQuery::querySubTree(root, V_SgInitializedName)) {
    SgInitializedName *name = isSgInitializedName(node);
    ROSE_ASSERT(name != nullptr);
    if (name->get_name() != "rex_ast_json_qualified_value") {
      continue;
    }
    ROSE_ASSERT(name->get_source_type_qualification_present());
    ROSE_ASSERT(!name->get_source_type_global_qualification());
    const SgStringList &tokens = name->get_source_type_qualification_tokens();
    ROSE_ASSERT(tokens.size() == 1);
    ROSE_ASSERT(tokens.front() == "rex_ast_json_scope::");
    ROSE_ASSERT(name->get_source_name_qualification_present());
    ROSE_ASSERT(!name->get_source_name_global_qualification());
    ROSE_ASSERT(name->get_source_name_qualification_tokens().empty());
    ++qualified_values;
  }
  ROSE_ASSERT(qualified_values == 1);
}

} // namespace

int main(int argc, char **argv) {
  const std::string json_dir = takeJsonDir(argc, argv);
  ROSE_ASSERT(!json_dir.empty());
  std::filesystem::remove_all(json_dir);
  std::filesystem::create_directories(json_dir);

  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  project->skipfinalCompileStep(true);

  SgSourceFile *file = firstSourceFile(project);
  verifySourceSpelledStaticMembers(file);
  verifyTypedStaticMemberParameterOwners(file);
  verifySourceSpelledTemplateArgumentType(file);
  verifySourceSpelledVariableInstantiation(file);
  verifySourceSpelledDeclarationHeaders(file);
  verifySourceSpelledTemplateTemplateParameter(file);
  verifyStructuralSourceQualifications(file);

  file = Rose::AstJson::roundTripSourceFile(
      file, Rose::AstJson::Checkpoint::PreOmpConstruction);
  ROSE_ASSERT(file != nullptr);
  verifySourceSpelledStaticMembers(file);
  verifyTypedStaticMemberParameterOwners(file);
  verifySourceSpelledTemplateArgumentType(file);
  verifySourceSpelledVariableInstantiation(file);
  verifySourceSpelledDeclarationHeaders(file);
  verifySourceSpelledTemplateTemplateParameter(file);
  verifyStructuralSourceQualifications(file);

  return backend(project);
}
