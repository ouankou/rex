#include "clang-frontend-private.hpp"

#include "sageInterface.h"

#include <cstring>

namespace {

constexpr const char *kSourceFilename =
    "rex_frontend_ordered_declaration_provenance_contract.cpp";

SgVariableDeclaration *makeOrdinaryDeclaration() {
  return new SgVariableDeclaration();
}

SgTypedefDeclaration *makeGeneratedTypedefDependency() {
  return new SgTypedefDeclaration(SgName("rex_generated_dependency"),
                                  SageBuilder::buildIntType(), nullptr, nullptr,
                                  nullptr);
}

void installGeneratedProvenance(SgLocatedNode *node) {
  ROSE_ASSERT(node != nullptr);
  Sg_File_Info *start =
      Sg_File_Info::generateDefaultFileInfoForCompilerGeneratedNode();
  Sg_File_Info *end =
      Sg_File_Info::generateDefaultFileInfoForCompilerGeneratedNode();
  ROSE_ASSERT(start != nullptr);
  ROSE_ASSERT(end != nullptr);
  node->set_file_info(start);
  node->set_startOfConstruct(start);
  node->set_endOfConstruct(end);
  start->set_parent(node);
  end->set_parent(node);
  mark_compiler_generated_frontend_specific(node);
}

Sg_File_Info *installSourceStart(SgDeclarationStatement *declaration,
                                 unsigned int occurrence) {
  ROSE_ASSERT(declaration != nullptr);
  Sg_File_Info *start = new Sg_File_Info(kSourceFilename, 4, 3);
  Sg_File_Info *end = new Sg_File_Info(kSourceFilename, 4, 17);
  start->set_source_sequence_number(occurrence);
  declaration->set_file_info(start);
  declaration->set_startOfConstruct(start);
  declaration->set_endOfConstruct(end);
  start->set_parent(declaration);
  end->set_parent(declaration);
  return start;
}

SgNamespaceDeclarationStatement *makeCanonicalNamespaceShell() {
  SgNamespaceDefinitionStatement *definition =
      SageBuilder::buildNamespaceDefinition();
  SgNamespaceDeclarationStatement *declaration =
      new SgNamespaceDeclarationStatement("rex_canonical", definition, false);
  definition->set_parent(declaration);
  definition->set_namespaceDeclaration(declaration);
  SageInterface::setOneSourcePositionForTransformation(declaration);
  SageInterface::setOneSourcePositionForTransformation(definition);

  SgNamespaceSourceFragment *opening = new SgNamespaceSourceFragment(
      SgNamespaceSourceFragment::e_namespace_source_fragment_opening,
      SgNamespaceSourceFragment::
          e_namespace_source_fragment_canonical_generated);
  SgNamespaceSourceFragment *closing = new SgNamespaceSourceFragment(
      SgNamespaceSourceFragment::e_namespace_source_fragment_closing,
      SgNamespaceSourceFragment::
          e_namespace_source_fragment_canonical_generated);
  SageInterface::setOneSourcePositionForTransformation(opening);
  SageInterface::setOneSourcePositionForTransformation(closing);
  declaration->attach_source_fragments(nullptr, opening, closing);
  return declaration;
}

} // namespace

int main(int argc, char **argv) {
  if (argc == 2) {
    SgDeclarationStatement *invalid = makeOrdinaryDeclaration();
    if (std::strcmp(argv[1], "order-without-start") == 0) {
      invalid->initialize_translation_unit_source_order(31);
    } else if (std::strcmp(argv[1], "start-without-order") == 0) {
      installSourceStart(invalid, 31);
    } else if (std::strcmp(argv[1], "mismatched-occurrence") == 0) {
      installSourceStart(invalid, 32);
      invalid->initialize_translation_unit_source_order(31);
    } else if (std::strcmp(argv[1], "generated-without-order") == 0) {
      invalid = makeGeneratedTypedefDependency();
      installGeneratedProvenance(invalid);
    } else if (std::strcmp(argv[1], "generated-wrong-kind") == 0) {
      installGeneratedProvenance(invalid);
      invalid->initialize_translation_unit_source_order(31);
    } else if (std::strcmp(argv[1], "generated-transformation") == 0) {
      invalid = makeGeneratedTypedefDependency();
      installGeneratedProvenance(invalid);
      invalid->get_endOfConstruct()->setTransformation();
      invalid->initialize_translation_unit_source_order(31);
    } else if (std::strcmp(argv[1], "generated-lexical-dependency") == 0) {
      invalid = makeGeneratedTypedefDependency();
      installGeneratedProvenance(invalid);
      invalid->initialize_translation_unit_source_order(31);
    } else if (std::strcmp(argv[1], "unpositioned-ordinary") != 0) {
      return 2;
    }
    requireClangOrderedDeclarationProvenanceForFrontend(invalid, argv[1]);
    return 3;
  }
  if (argc != 1) {
    return 2;
  }

  SgVariableDeclaration *source = makeOrdinaryDeclaration();
  Sg_File_Info *sourceStart = installSourceStart(source, 41);
  source->initialize_translation_unit_source_order(41);
  const ClangOrderedDeclarationProvenance sourceProvenance =
      requireClangOrderedDeclarationProvenanceForFrontend(source,
                                                          "positive-source");
  if (sourceProvenance.kind !=
          ClangOrderedDeclarationProvenance::Kind::e_source_lexical ||
      sourceProvenance.source_start != sourceStart ||
      sourceProvenance.source_order != 41) {
    return 1;
  }

  SgNamespaceDeclarationStatement *canonical = makeCanonicalNamespaceShell();
  const ClangOrderedDeclarationProvenance canonicalProvenance =
      requireClangOrderedDeclarationProvenanceForFrontend(canonical,
                                                          "positive-canonical");
  if (canonicalProvenance.kind != ClangOrderedDeclarationProvenance::Kind::
                                      e_canonical_generated_namespace_shell ||
      canonicalProvenance.source_start != nullptr ||
      canonicalProvenance.source_order != 0) {
    return 1;
  }

  return 0;
}
