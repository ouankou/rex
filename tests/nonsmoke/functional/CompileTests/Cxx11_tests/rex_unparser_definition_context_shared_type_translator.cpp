#include "RoseAst.h"
#include "rose.h"

#include <string>

namespace {

  struct InlineDefinition {
    SgTypedefDeclaration *owner = nullptr;
    SgDeclarationStatement *definition = nullptr;
    SgNamedType *type = nullptr;
  };

  InlineDefinition findInlineDefinition(SgSourceFile *file,
                                        const std::string &alias_name) {
    InlineDefinition result;
    for (SgNode *node : RoseAst(file)) {
      SgTypedefDeclaration *declaration = isSgTypedefDeclaration(node);
      if (declaration == nullptr ||
          declaration->get_name().getString() != alias_name) {
        continue;
      }
      ROSE_ASSERT(result.owner == nullptr);
      result.owner = declaration;
      result.definition = declaration->get_baseTypeDefiningDeclaration();
      result.type = isSgNamedType(declaration->get_base_type()->findBaseType());
    }

    ROSE_ASSERT(result.owner != nullptr && result.definition != nullptr &&
                result.type != nullptr);
    ROSE_ASSERT(result.owner->get_declaration() == result.definition);
    ROSE_ASSERT(result.definition->get_parent() == result.owner);
    ROSE_ASSERT(result.definition->get_definingDeclaration() ==
                result.definition);
    ROSE_ASSERT(result.definition->get_firstNondefiningDeclaration() !=
                nullptr);
    ROSE_ASSERT(result.definition->get_firstNondefiningDeclaration()
                    ->get_firstNondefiningDeclaration() ==
                result.definition->get_firstNondefiningDeclaration());
    return result;
  }

  SgNamedType *definitionType(SgDeclarationStatement *declaration) {
    if (SgClassDeclaration *class_declaration =
            isSgClassDeclaration(declaration)) {
      SgClassDefinition *body = class_declaration->get_definition();
      ROSE_ASSERT(body != nullptr && body->get_declaration() == declaration &&
                  body->get_parent() == declaration);
      return class_declaration->get_type();
    }
    SgEnumDeclaration *enum_declaration = isSgEnumDeclaration(declaration);
    ROSE_ASSERT(enum_declaration != nullptr);
    for (SgInitializedName *enumerator : enum_declaration->get_enumerators()) {
      ROSE_ASSERT(enumerator != nullptr &&
                  enumerator->get_parent() == enum_declaration);
    }
    return enum_declaration->get_type();
  }

  void
  requireDistinctTranslationUnitTypeIdentities(const InlineDefinition &first,
                                               const InlineDefinition &second) {
    ROSE_ASSERT(first.owner != second.owner);
    ROSE_ASSERT(first.definition != second.definition);
    ROSE_ASSERT(first.type != second.type);
    ROSE_ASSERT(definitionType(first.definition) == first.type);
    ROSE_ASSERT(definitionType(second.definition) == second.type);
    ROSE_ASSERT(first.definition->get_firstNondefiningDeclaration() !=
                second.definition->get_firstNondefiningDeclaration());
    ROSE_ASSERT(first.type->get_declaration() != nullptr);
    ROSE_ASSERT(second.type->get_declaration() != nullptr);
    ROSE_ASSERT(first.type->get_declaration() !=
                second.type->get_declaration());
    ROSE_ASSERT(first.type->get_declaration()->get_definingDeclaration() ==
                first.definition);
    ROSE_ASSERT(second.type->get_declaration()->get_definingDeclaration() ==
                second.definition);
  }

} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr && project->get_fileList().size() == 2);
  SgSourceFile *first = isSgSourceFile(project->get_fileList()[0]);
  SgSourceFile *second = isSgSourceFile(project->get_fileList()[1]);
  ROSE_ASSERT(first != nullptr && second != nullptr);

  requireDistinctTranslationUnitTypeIdentities(
      findInlineDefinition(first, "RexDefinitionContextClassAlias"),
      findInlineDefinition(second, "RexDefinitionContextClassAlias"));
  requireDistinctTranslationUnitTypeIdentities(
      findInlineDefinition(first, "RexDefinitionContextEnumAlias"),
      findInlineDefinition(second, "RexDefinitionContextEnumAlias"));
  return backend(project);
}
