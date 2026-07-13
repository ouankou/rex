#include "RoseAst.h"
#include "rose.h"
#include "sageAstJsonPrivate.h"

#include <string>
#include <vector>

namespace {

SgSourceFile *sourceFile(SgProject *project) {
  SgSourceFile *result = nullptr;
  for (SgFile *file : project->get_fileList()) {
    if (SgSourceFile *source = isSgSourceFile(file)) {
      if (!source->get_isHeaderFile()) {
        ROSE_ASSERT(result == nullptr);
        result = source;
      }
    }
  }
  ROSE_ASSERT(result != nullptr);
  return result;
}

void roundTrip(SgProject *project) {
  using namespace Rose::AstJson;
  SgSourceFile *source = sourceFile(project);
  constexpr Checkpoint checkpoint = Checkpoint::PreOmpConstruction;
  AstFileRecord ast = parseAstFileJson(buildJson(source, checkpoint, source),
                                       checkpointName(checkpoint));
  SgSourceFile *copy = reconstructSourceFile(ast, source);
  replaceFileInProject(source, copy);
}

std::string variableName(SgDeclarationStatement *declaration) {
  SgVariableDeclaration *variable = isSgVariableDeclaration(declaration);
  if (variable == nullptr || variable->get_variables().size() != 1 ||
      variable->get_variables().front() == nullptr) {
    return {};
  }
  return variable->get_variables().front()->get_name().getString();
}

std::string labelName(SgAccessLabelStatement::access_label_kind_enum label) {
  switch (label) {
  case SgAccessLabelStatement::e_access_label_private:
    return "private";
  case SgAccessLabelStatement::e_access_label_protected:
    return "protected";
  case SgAccessLabelStatement::e_access_label_public:
    return "public";
  }
  return "invalid";
}

SgClassDefinition *findDefinition(SgProject *project, const std::string &name) {
  for (SgNode *node : RoseAst(project)) {
    SgClassDefinition *definition = isSgClassDefinition(node);
    if (definition != nullptr && definition->get_declaration() != nullptr &&
        definition->get_declaration()->get_name().getString() == name) {
      return definition;
    }
  }
  return nullptr;
}

void requireSourceEmptyDeclarationRole(SgProject *project) {
  SgGlobal *global = sourceFile(project)->get_globalScope();
  ROSE_ASSERT(global != nullptr);
  size_t count = 0;
  for (SgDeclarationStatement *declaration : global->get_declarations()) {
    SgEmptyDeclaration *empty = isSgEmptyDeclaration(declaration);
    if (empty == nullptr) {
      continue;
    }
    empty->validate_lexical_role();
    ROSE_ASSERT(empty->get_lexical_role() ==
                SgEmptyDeclaration::e_empty_declaration_source_semicolon);
    ROSE_ASSERT(empty->get_parent() == global);
    ++count;
  }
  ROSE_ASSERT(count == 1);
}

void requireOrder(SgClassDefinition *definition,
                  const std::vector<std::string> &expected) {
  ROSE_ASSERT(definition != nullptr);
  std::vector<std::string> actual;
  for (SgDeclarationStatement *member : definition->get_members()) {
    ROSE_ASSERT(member != nullptr);
    ROSE_ASSERT(member->get_parent() == definition);
    ROSE_ASSERT(member->get_scope() == definition);
    if (SgAccessLabelStatement *label = isSgAccessLabelStatement(member)) {
      label->validate();
      actual.push_back("access:" + labelName(label->get_label_kind()));
      continue;
    }
    const std::string name = variableName(member);
    if (!name.empty()) {
      actual.push_back("variable:" + name);
    }
  }
  ROSE_ASSERT(actual == expected);
}

} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(frontendExitStatus(project) == 0);
  roundTrip(project);
  requireSourceEmptyDeclarationRole(project);

  requireOrder(findDefinition(project, "RexJsonAccessClass"),
               {"variable:implicit_private", "access:public",
                "variable:explicit_public", "access:protected",
                "variable:explicit_protected"});
  requireOrder(findDefinition(project, "RexJsonAccessStruct"),
               {"variable:implicit_public", "access:private",
                "variable:explicit_private", "access:public",
                "variable:explicit_public"});

  AstTests::runAllTests(project);
  return backend(project);
}
