#include "rose.h"

#include <algorithm>
#include <string>
#include <vector>

namespace {
class LinkageCollector : public AstSimpleProcessing {
public:
  std::vector<SgVariableDeclaration *> variables;
  std::vector<SgTypedefDeclaration *> typedefs;
  std::vector<SgClassDeclaration *> classes;
  std::vector<SgClinkageStartStatement *> starts;
  std::vector<SgClinkageEndStatement *> ends;

  void visit(SgNode *node) override {
    if (SgVariableDeclaration *variable = isSgVariableDeclaration(node)) {
      variables.push_back(variable);
    }
    if (SgTypedefDeclaration *typedefDeclaration =
            isSgTypedefDeclaration(node)) {
      typedefs.push_back(typedefDeclaration);
    }
    if (SgClassDeclaration *classDeclaration = isSgClassDeclaration(node)) {
      classes.push_back(classDeclaration);
    }
    if (SgClinkageStartStatement *start = isSgClinkageStartStatement(node)) {
      starts.push_back(start);
    }
    if (SgClinkageEndStatement *end = isSgClinkageEndStatement(node)) {
      ends.push_back(end);
    }
  }
};

SgVariableDeclaration *findVariable(const LinkageCollector &collector,
                                    const std::string &name) {
  SgVariableDeclaration *result = nullptr;
  for (SgVariableDeclaration *variable : collector.variables) {
    if (variable == nullptr || variable->get_variables().size() != 1 ||
        variable->get_variables().front() == nullptr ||
        variable->get_variables().front()->get_name().getString() != name) {
      continue;
    }
    ROSE_ASSERT(result == nullptr);
    result = variable;
  }
  ROSE_ASSERT(result != nullptr);
  return result;
}

SgTypedefDeclaration *findTypedef(const LinkageCollector &collector,
                                  const std::string &name) {
  SgTypedefDeclaration *result = nullptr;
  for (SgTypedefDeclaration *declaration : collector.typedefs) {
    if (declaration == nullptr || declaration->get_name().getString() != name) {
      continue;
    }
    ROSE_ASSERT(result == nullptr);
    result = declaration;
  }
  ROSE_ASSERT(result != nullptr);
  return result;
}

SgClassDeclaration *findDefiningClass(const LinkageCollector &collector,
                                      const std::string &name) {
  SgClassDeclaration *result = nullptr;
  for (SgClassDeclaration *declaration : collector.classes) {
    if (declaration == nullptr || declaration->get_name().getString() != name ||
        declaration->get_definition() == nullptr) {
      continue;
    }
    ROSE_ASSERT(result == nullptr);
    result = declaration;
  }
  ROSE_ASSERT(result != nullptr);
  return result;
}

void requireEmbeddedDefinition(SgVariableDeclaration *variable) {
  ROSE_ASSERT(variable != nullptr);
  ROSE_ASSERT(variable->get_baseTypeNondefiningDeclaration() == nullptr);
  SgDeclarationStatement *definition =
      variable->get_baseTypeDefiningDeclaration();
  ROSE_ASSERT(definition != nullptr);
  ROSE_ASSERT(definition->get_parent() == variable);
  if (SgClassDeclaration *classDeclaration = isSgClassDeclaration(definition)) {
    ROSE_ASSERT(!classDeclaration->get_isAutonomousDeclaration());
    ROSE_ASSERT(classDeclaration->get_definingDeclaration() == definition);
    ROSE_ASSERT(classDeclaration->get_definition() != nullptr);
    ROSE_ASSERT(classDeclaration->get_definition()->get_parent() == definition);
    return;
  }
  SgEnumDeclaration *enumDeclaration = isSgEnumDeclaration(definition);
  ROSE_ASSERT(enumDeclaration != nullptr);
  ROSE_ASSERT(!enumDeclaration->get_isAutonomousDeclaration());
  ROSE_ASSERT(enumDeclaration->get_definingDeclaration() == definition);
  ROSE_ASSERT(!enumDeclaration->isForward());
}

void requireEmbeddedNondefiningTag(SgVariableDeclaration *variable) {
  ROSE_ASSERT(variable != nullptr);
  ROSE_ASSERT(variable->get_baseTypeDefiningDeclaration() == nullptr);
  SgDeclarationStatement *introduction =
      variable->get_baseTypeNondefiningDeclaration();
  SgClassDeclaration *classDeclaration = isSgClassDeclaration(introduction);
  ROSE_ASSERT(classDeclaration != nullptr);
  ROSE_ASSERT(classDeclaration->get_parent() == variable);
  ROSE_ASSERT(!classDeclaration->get_isAutonomousDeclaration());
  ROSE_ASSERT(classDeclaration->get_firstNondefiningDeclaration() ==
              classDeclaration);
  ROSE_ASSERT(classDeclaration->get_definingDeclaration() != classDeclaration);
  ROSE_ASSERT(classDeclaration->get_definition() == nullptr);

  ROSE_ASSERT(variable->get_variables().size() == 1);
  SgInitializedName *initializedName = variable->get_variables().front();
  ROSE_ASSERT(initializedName != nullptr);
  SgNamedType *namedType =
      isSgNamedType(initializedName->get_type()->findBaseType());
  ROSE_ASSERT(namedType != nullptr);
  SgDeclarationStatement *typeDeclaration = namedType->get_declaration();
  ROSE_ASSERT(typeDeclaration != nullptr);
  ROSE_ASSERT(typeDeclaration == classDeclaration ||
              typeDeclaration->get_firstNondefiningDeclaration() ==
                  classDeclaration);

  const SgNodePtrList successors = variable->get_traversalSuccessorContainer();
  auto tagPosition =
      std::find(successors.begin(), successors.end(), classDeclaration);
  auto namePosition =
      std::find(successors.begin(), successors.end(), initializedName);
  ROSE_ASSERT(tagPosition != successors.end());
  ROSE_ASSERT(namePosition != successors.end());
  ROSE_ASSERT(
      std::count(successors.begin(), successors.end(), classDeclaration) == 1);
  ROSE_ASSERT(tagPosition < namePosition);
}

void requireEmbeddedDefinition(SgTypedefDeclaration *declaration) {
  ROSE_ASSERT(declaration != nullptr);
  ROSE_ASSERT(declaration->get_typedefBaseTypeContainsDefiningDeclaration());
  SgDeclarationStatement *definition = declaration->get_declaration();
  ROSE_ASSERT(definition != nullptr);
  ROSE_ASSERT(definition->get_parent() == declaration);
  if (SgClassDeclaration *classDeclaration = isSgClassDeclaration(definition)) {
    ROSE_ASSERT(!classDeclaration->get_isAutonomousDeclaration());
    ROSE_ASSERT(classDeclaration->get_definingDeclaration() == definition);
    ROSE_ASSERT(classDeclaration->get_definition() != nullptr);
    return;
  }
  SgEnumDeclaration *enumDeclaration = isSgEnumDeclaration(definition);
  ROSE_ASSERT(enumDeclaration != nullptr);
  ROSE_ASSERT(!enumDeclaration->get_isAutonomousDeclaration());
  ROSE_ASSERT(enumDeclaration->get_definingDeclaration() == definition);
}

bool filenameEndsWith(SgLocatedNode *node, const std::string &suffix) {
  Sg_File_Info *info = node != nullptr ? node->get_file_info() : nullptr;
  if (info == nullptr) {
    return false;
  }
  const std::string filename = info->get_filenameString();
  return filename.size() >= suffix.size() &&
         filename.compare(filename.size() - suffix.size(), suffix.size(),
                          suffix) == 0;
}

template <class Marker>
Marker *findMarkerInFile(const std::vector<Marker *> &markers,
                         const std::string &suffix) {
  Marker *result = nullptr;
  for (Marker *marker : markers) {
    if (!filenameEndsWith(marker, suffix)) {
      continue;
    }
    ROSE_ASSERT(result == nullptr);
    result = marker;
  }
  ROSE_ASSERT(result != nullptr);
  return result;
}

void requireMarkerPair(SgClinkageStartStatement *start,
                       SgClinkageEndStatement *end) {
  ROSE_ASSERT(start != nullptr);
  ROSE_ASSERT(end != nullptr);
  ROSE_ASSERT(start->get_languageSpecifier() == "C");
  ROSE_ASSERT(end->get_languageSpecifier() == "C");
  ROSE_ASSERT(isSgGlobal(start->get_parent()) != nullptr);
  ROSE_ASSERT(start->get_parent() == end->get_parent());
  ROSE_ASSERT(start->get_scope() == start->get_parent());
  ROSE_ASSERT(end->get_scope() == end->get_parent());
  ROSE_ASSERT(start->get_startOfConstruct() != nullptr);
  ROSE_ASSERT(start->get_endOfConstruct() != nullptr);
  ROSE_ASSERT(end->get_startOfConstruct() != nullptr);
  ROSE_ASSERT(end->get_endOfConstruct() != nullptr);
  ROSE_ASSERT(start->get_translation_unit_source_order().has_value());
  ROSE_ASSERT(end->get_translation_unit_source_order().has_value());
  ROSE_ASSERT(*start->get_translation_unit_source_order() <
              *end->get_translation_unit_source_order());
  ROSE_ASSERT(start->get_startOfConstruct()->get_source_sequence_number() ==
              *start->get_translation_unit_source_order());
  ROSE_ASSERT(end->get_startOfConstruct()->get_source_sequence_number() ==
              *end->get_translation_unit_source_order());

  SgGlobal *global = isSgGlobal(start->get_parent());
  const SgDeclarationStatementPtrList &declarations =
      global->get_declarations();
  auto startPosition =
      std::find(declarations.begin(), declarations.end(), start);
  auto endPosition = std::find(declarations.begin(), declarations.end(), end);
  ROSE_ASSERT(startPosition != declarations.end());
  ROSE_ASSERT(endPosition != declarations.end());
  ROSE_ASSERT(startPosition < endPosition);
}
} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(frontendExitStatus(project) == 0);

  LinkageCollector collector;
  collector.traverse(project, preorder);

  for (const char *name :
       {"rex_nonbraced_inline", "rex_nonbraced_enum", "rex_braced_inline",
        "rex_braced_enum", "rex_macro_inline"}) {
    SgVariableDeclaration *variable = findVariable(collector, name);
    requireEmbeddedDefinition(variable);
    ROSE_ASSERT(variable->get_linkage() == "C");
  }
  requireEmbeddedNondefiningTag(
      findVariable(collector, "rex_nonbraced_forward"));

  for (const char *name :
       {"RexNonbracedTypedef", "RexBracedTypedef", "RexMacroTypedef"}) {
    SgTypedefDeclaration *declaration = findTypedef(collector, name);
    requireEmbeddedDefinition(declaration);
    ROSE_ASSERT(declaration->get_linkage() == "C");
  }

  SgClassDeclaration *standaloneTag =
      findDefiningClass(collector, "RexBracedStandaloneTag");
  SgTypedefDeclaration *standaloneAlias =
      findTypedef(collector, "RexBracedStandaloneAlias");
  ROSE_ASSERT(standaloneTag->get_parent() == standaloneTag->get_scope());
  ROSE_ASSERT(isSgGlobal(standaloneTag->get_parent()) != nullptr);
  ROSE_ASSERT(standaloneTag->get_isAutonomousDeclaration());
  ROSE_ASSERT(standaloneTag->isOutputInCodeGeneration());
  ROSE_ASSERT(standaloneTag->get_linkage() == "C");
  ROSE_ASSERT(
      !standaloneAlias->get_typedefBaseTypeContainsDefiningDeclaration());
  ROSE_ASSERT(standaloneAlias->get_parent() == standaloneTag->get_parent());
  SgClassDeclaration *aliasTag =
      isSgClassDeclaration(standaloneAlias->get_declaration());
  ROSE_ASSERT(aliasTag != nullptr);
  ROSE_ASSERT(aliasTag->get_parent() != standaloneAlias);
  ROSE_ASSERT(aliasTag == standaloneTag ||
              aliasTag->get_definingDeclaration() == standaloneTag);
  ROSE_ASSERT(standaloneAlias->get_linkage() == "C");

  ROSE_ASSERT(collector.starts.size() == 2);
  ROSE_ASSERT(collector.ends.size() == 2);
  SgClinkageStartStatement *directStart = findMarkerInFile(
      collector.starts, "rex_frontend_linkage_container_contract.cpp");
  SgClinkageEndStatement *directEnd = findMarkerInFile(
      collector.ends, "rex_frontend_linkage_container_contract.cpp");
  requireMarkerPair(directStart, directEnd);

  SgClinkageStartStatement *macroStart = findMarkerInFile(
      collector.starts, "rex_frontend_linkage_macro_header.hpp");
  SgClinkageEndStatement *macroEnd =
      findMarkerInFile(collector.ends, "rex_frontend_linkage_macro_header.hpp");
  requireMarkerPair(macroStart, macroEnd);
  ROSE_ASSERT(macroStart->get_startOfConstruct()->get_line() == 7);
  ROSE_ASSERT(macroEnd->get_startOfConstruct()->get_line() == 17);

  return backend(project);
}
