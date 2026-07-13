#include "rose.h"

namespace {
SgNonrealDecl *nonrealDeclaration(SgType *type) {
  SgNonrealType *nonreal = isSgNonrealType(type);
  return isSgNonrealDecl(nonreal != nullptr ? nonreal->get_declaration()
                                            : nullptr);
}

SgNonrealDecl *structuralOwner(SgNonrealDecl *declaration) {
  ROSE_ASSERT(declaration != nullptr);
  SgDeclarationScope *scope = isSgDeclarationScope(declaration->get_parent());
  ROSE_ASSERT(scope != nullptr);
  return isSgNonrealDecl(scope->get_parent());
}

SgTemplateArgument *onlyTypeArgument(SgNonrealDecl *declaration) {
  ROSE_ASSERT(declaration != nullptr);
  ROSE_ASSERT(declaration->get_tpl_args().size() == 1);
  SgTemplateArgument *argument = declaration->get_tpl_args().front();
  ROSE_ASSERT(argument != nullptr);
  ROSE_ASSERT(argument->get_argumentType() ==
              SgTemplateArgument::type_argument);
  ROSE_ASSERT(argument->get_type() != nullptr);
  ROSE_ASSERT(argument->get_sourceSpelledType() != nullptr);
  return argument;
}
} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(frontendExitStatus(project) == 0);

  size_t qualified_inner_argument_count = 0;
  size_t qualified_outer_type_count = 0;
  for (SgNode *node : NodeQuery::querySubTree(project, V_SgInitializedName)) {
    SgInitializedName *name = isSgInitializedName(node);
    ROSE_ASSERT(name != nullptr);
    if (name->get_name() != "a" && name->get_name() != "c") {
      continue;
    }

    SgType *source_type = name->get_cxx_source_type();
    SgType *semantic_type = name->get_type();
    ROSE_ASSERT(source_type != nullptr);
    ROSE_ASSERT(semantic_type != nullptr);
    ROSE_ASSERT(source_type != semantic_type);
    ROSE_ASSERT(isSgClassType(semantic_type) != nullptr);

    SgNonrealDecl *source_declaration = nonrealDeclaration(source_type);
    ROSE_ASSERT(source_declaration != nullptr);
    ROSE_ASSERT(source_declaration->get_templateDeclaration() != nullptr);
    ROSE_ASSERT(!source_declaration->get_source_name_qualification_present());
    ROSE_ASSERT(!source_declaration->get_source_name_global_qualification());
    ROSE_ASSERT(
        source_declaration->get_source_name_qualification_tokens().empty());
    ROSE_ASSERT(name->get_source_type_qualification_present());
    ROSE_ASSERT(!name->get_source_type_global_qualification());

    if (name->get_name() == "a") {
      ROSE_ASSERT(source_declaration->get_name() == "A");
      ROSE_ASSERT(name->get_source_type_qualification_tokens().empty());

      SgTemplateArgument *argument = onlyTypeArgument(source_declaration);
      ROSE_ASSERT(argument->get_source_type_qualification_present());
      ROSE_ASSERT(!argument->get_source_type_global_qualification());
      ROSE_ASSERT(argument->get_source_type_qualification_tokens() ==
                  SgStringList{"B < 3 > ::"});

      SgNonrealDecl *inner =
          nonrealDeclaration(argument->get_sourceSpelledType());
      ROSE_ASSERT(inner != nullptr);
      ROSE_ASSERT(inner->get_name() == "C");
      ROSE_ASSERT(structuralOwner(inner) != nullptr);
      ROSE_ASSERT(structuralOwner(inner)->get_name() == "B");
      ++qualified_inner_argument_count;
    } else {
      ROSE_ASSERT(source_declaration->get_name() == "C");
      ROSE_ASSERT(name->get_source_type_qualification_tokens() ==
                  SgStringList{"A < B < 3 > ::"});
      ROSE_ASSERT(structuralOwner(source_declaration) != nullptr);
      ROSE_ASSERT(structuralOwner(source_declaration)->get_name() == "A");

      SgTemplateArgument *argument = onlyTypeArgument(source_declaration);
      SgNonrealDecl *inner =
          nonrealDeclaration(argument->get_sourceSpelledType());
      ROSE_ASSERT(inner != nullptr);
      ROSE_ASSERT(inner->get_name() == "D");
      ++qualified_outer_type_count;
    }
  }

  ROSE_ASSERT(qualified_inner_argument_count == 1);
  ROSE_ASSERT(qualified_outer_type_count == 1);
  return backend(project);
}
