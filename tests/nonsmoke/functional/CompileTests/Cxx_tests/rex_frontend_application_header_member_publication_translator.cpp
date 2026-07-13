#include "RoseAst.h"
#include "rose.h"

#include <array>
#include <map>
#include <string>

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr && frontendExitStatus(project) == 0);

  std::map<std::string, SgFunctionDeclaration *> sourceMembers;
  std::map<std::string, size_t> semanticMembers;
  for (SgNode *node : RoseAst(project)) {
    SgFunctionDeclaration *declaration = isSgFunctionDeclaration(node);
    if (declaration == nullptr ||
        (declaration->get_name() != "first" &&
         declaration->get_name() != "later") ||
        declaration->get_frontend_source_ownership() !=
            SgFunctionDeclaration::e_frontend_source_application_header) {
      continue;
    }

    const bool semanticOnly =
        SageInterface::hasExactSemanticFrontendSourcePosition(
            declaration, declaration->get_file_info()) &&
        SageInterface::hasExactSemanticFrontendSourcePosition(
            declaration, declaration->get_startOfConstruct()) &&
        SageInterface::hasExactSemanticFrontendSourcePosition(
            declaration, declaration->get_endOfConstruct());
    if (semanticOnly) {
      SgAuxiliaryDeclarationList *owner =
          isSgAuxiliaryDeclarationList(declaration->get_parent());
      ROSE_ASSERT(owner != nullptr && owner->get_parent() != nullptr &&
                  declaration->get_scope() == owner->get_parent());
      ++semanticMembers[declaration->get_name().getString()];
      continue;
    }

    ROSE_ASSERT(
        sourceMembers.emplace(declaration->get_name().getString(), declaration)
            .second);
    SgTemplateClassDefinition *owner =
        isSgTemplateClassDefinition(declaration->get_parent());
    if (owner == nullptr || declaration->get_scope() != owner) {
      std::cerr << "REX_TEST_INVARIANT[application-header-member-publication]: "
                << "member=" << declaration->get_name().getString()
                << " declaration=" << declaration << "/"
                << declaration->class_name()
                << " parent=" << declaration->get_parent() << "/"
                << (declaration->get_parent() != nullptr
                        ? declaration->get_parent()->class_name()
                        : std::string("<null>"))
                << " scope=" << declaration->get_scope() << "/"
                << (declaration->get_scope() != nullptr
                        ? declaration->get_scope()->class_name()
                        : std::string("<null>"))
                << std::endl;
      ROSE_ABORT();
    }
    ROSE_ASSERT(owner->get_members().size() >= 2);
    ROSE_ASSERT(declaration->get_definition() != nullptr);
    ROSE_ASSERT(declaration->get_definingDeclaration() == declaration);
    ROSE_ASSERT(declaration->get_translation_unit_source_order().has_value());
    ROSE_ASSERT(declaration->get_file_info() != nullptr);
    ROSE_ASSERT(declaration->get_file_info()->get_filenameString().find(
                    "rex_frontend_application_header_member_publication.hpp") !=
                std::string::npos);
    ROSE_ASSERT(!SageInterface::insideSystemHeader(declaration));
  }

  if (sourceMembers.size() != 2) {
    std::cerr << "REX_TEST_INVARIANT[application-header-member-publication]: "
              << "expected source members first/later, found source="
              << sourceMembers.size()
              << " semantic-first=" << semanticMembers["first"]
              << " semantic-later=" << semanticMembers["later"] << std::endl;
    ROSE_ABORT();
  }
  ROSE_ASSERT(semanticMembers["first"] > 0 && semanticMembers["later"] > 0);
  ROSE_ASSERT(*sourceMembers.at("first")->get_translation_unit_source_order() <
              *sourceMembers.at("later")->get_translation_unit_source_order());

  SgFunctionDeclaration *sourceLater = sourceMembers.at("later");
  SgFunctionDefinition *sourceLaterDefinition = sourceLater->get_definition();
  SgBasicBlock *sourceLaterBody = sourceLaterDefinition != nullptr
                                      ? sourceLaterDefinition->get_body()
                                      : nullptr;
  ROSE_ASSERT(sourceLaterDefinition != nullptr && sourceLaterBody != nullptr &&
              sourceLaterDefinition->get_declaration() == sourceLater &&
              sourceLaterBody->get_parent() == sourceLaterDefinition);

  auto requireExactLocalSource = [&](SgDeclarationStatement *declaration,
                                     const char *expectedName) {
    ROSE_ASSERT(declaration != nullptr && expectedName != nullptr);
    std::array<Sg_File_Info *, 3> positions{declaration->get_file_info(),
                                            declaration->get_startOfConstruct(),
                                            declaration->get_endOfConstruct()};
    for (Sg_File_Info *position : positions) {
      if (position == nullptr || position->get_parent() != declaration ||
          position->get_file_id() < 0 || position->get_physical_file_id() < 0 ||
          position->isCompilerGenerated() || position->isFrontendSpecific() ||
          position->isTransformation() ||
          position->isSourcePositionUnavailableInFrontend() ||
          position->get_filenameString().find(
              "rex_frontend_application_header_member_publication.hpp") ==
              std::string::npos) {
        std::cerr
            << "REX_TEST_INVARIANT[application-header-local-publication]: "
            << "local=" << expectedName << " declaration=" << declaration << "/"
            << declaration->class_name() << " position=" << position
            << std::endl;
        ROSE_ABORT();
      }
    }
    ROSE_ASSERT(declaration->get_parent() == sourceLaterBody &&
                declaration->get_scope() == sourceLaterBody &&
                declaration->get_translation_unit_source_order().has_value());
  };

  std::map<std::string, SgDeclarationStatement *> sourceLocals;
  for (SgNode *node : RoseAst(sourceLaterDefinition)) {
    SgDeclarationStatement *declaration = isSgDeclarationStatement(node);
    if (declaration == nullptr || declaration == sourceLater) {
      continue;
    }

    std::string name;
    if (SgTypedefDeclaration *typedefDeclaration =
            isSgTypedefDeclaration(declaration)) {
      name = typedefDeclaration->get_name().getString();
    } else if (SgClassDeclaration *classDeclaration =
                   isSgClassDeclaration(declaration);
               classDeclaration != nullptr &&
               classDeclaration->get_definingDeclaration() ==
                   classDeclaration) {
      name = classDeclaration->get_name().getString();
    } else if (SgEnumDeclaration *enumDeclaration =
                   isSgEnumDeclaration(declaration);
               enumDeclaration != nullptr &&
               enumDeclaration->get_definingDeclaration() == enumDeclaration) {
      name = enumDeclaration->get_name().getString();
    } else if (SgVariableDeclaration *variableDeclaration =
                   isSgVariableDeclaration(declaration);
               variableDeclaration != nullptr &&
               variableDeclaration->get_variables().size() == 1) {
      name =
          variableDeclaration->get_variables().front()->get_name().getString();
    }

    if (name == "RexLocalTypedef" || name == "RexLocalAlias" ||
        name == "RexLocalRecord" || name == "RexLocalEnum" ||
        name == "rexLocalRecord") {
      ROSE_ASSERT(sourceLocals.emplace(name, declaration).second);
    }
  }

  for (const char *expected :
       {"RexLocalTypedef", "RexLocalAlias", "RexLocalRecord", "RexLocalEnum",
        "rexLocalRecord"}) {
    auto found = sourceLocals.find(expected);
    if (found == sourceLocals.end()) {
      std::cerr << "REX_TEST_INVARIANT[application-header-local-publication]: "
                << "missing exact local source declaration=" << expected
                << std::endl;
      ROSE_ABORT();
    }
    requireExactLocalSource(found->second, expected);
  }

  AstTests::runAllTests(project);
  return backend(project);
}
