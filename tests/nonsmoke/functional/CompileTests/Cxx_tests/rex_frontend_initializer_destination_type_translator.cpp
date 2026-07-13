#include "RoseAst.h"
#include "rose.h"
#include "sageAstJsonPrivate.h"

#include <string>

namespace {

SgInitializedName *findInitializedName(SgProject *project,
                                       const std::string &name) {
  SgInitializedName *result = nullptr;
  for (SgNode *node : RoseAst(project)) {
    SgInitializedName *candidate = isSgInitializedName(node);
    if (candidate == nullptr || candidate->get_name() != name) {
      continue;
    }
    ROSE_ASSERT(result == nullptr);
    result = candidate;
  }
  ROSE_ASSERT(result != nullptr);
  return result;
}

void requireExactAssignDestination(SgProject *project,
                                   const std::string &name) {
  SgInitializedName *destination = findInitializedName(project, name);
  SgAssignInitializer *initializer =
      isSgAssignInitializer(destination->get_initializer());
  ROSE_ASSERT(initializer != nullptr);
  ROSE_ASSERT(initializer->get_parent() == destination);
  ROSE_ASSERT(initializer->get_operand() != nullptr);
  ROSE_ASSERT(initializer->get_operand()->get_parent() == initializer);
  ROSE_ASSERT(initializer->get_type() != nullptr);
  ROSE_ASSERT(isSgTypeUnknown(initializer->get_type()) == nullptr);
  ROSE_ASSERT(isSgTypeDefault(initializer->get_type()) == nullptr);
  ROSE_ASSERT(SageInterface::isEquivalentType(initializer->get_type(),
                                              destination->get_type()));
}

void validateInitializerContracts(SgProject *project) {
  requireExactAssignDestination(project, "rex_initializer_text");
  requireExactAssignDestination(project, "rex_initializer_array_reference");
  requireExactAssignDestination(project, "rex_initializer_qualified");

  SgInitializedName *text =
      findInitializedName(project, "rex_initializer_text");
  ROSE_ASSERT(isSgArrayType(text->get_type()) != nullptr);
  ROSE_ASSERT(isSgArrayType(text->get_initializer()->get_type()) != nullptr);
  ROSE_ASSERT(isSgPointerType(text->get_initializer()->get_type()) == nullptr);

  SgInitializedName *array_reference =
      findInitializedName(project, "rex_initializer_array_reference");
  SgReferenceType *reference_type =
      isSgReferenceType(array_reference->get_type());
  ROSE_ASSERT(reference_type != nullptr);
  ROSE_ASSERT(isSgArrayType(reference_type->get_base_type()) != nullptr);
  ROSE_ASSERT(isSgPointerType(array_reference->get_initializer()->get_type()) ==
              nullptr);

  SgFunctionDeclaration *factory = nullptr;
  for (SgNode *node : RoseAst(project)) {
    SgFunctionDeclaration *candidate = isSgFunctionDeclaration(node);
    if (candidate != nullptr &&
        candidate->get_name() == "rex_initializer_make_record" &&
        candidate->get_definition() != nullptr) {
      ROSE_ASSERT(factory == nullptr);
      factory = candidate;
    }
  }
  ROSE_ASSERT(factory != nullptr);
  SgBracedInitializer *braced_return = nullptr;
  for (SgNode *node : RoseAst(factory->get_definition())) {
    SgReturnStmt *return_statement = isSgReturnStmt(node);
    if (return_statement == nullptr) {
      continue;
    }
    ROSE_ASSERT(braced_return == nullptr);
    braced_return = isSgBracedInitializer(return_statement->get_expression());
  }
  ROSE_ASSERT(braced_return != nullptr);
  ROSE_ASSERT(braced_return->get_initializers() != nullptr);
  ROSE_ASSERT(braced_return->get_initializers()->get_parent() == braced_return);
  ROSE_ASSERT(braced_return->get_type() != nullptr);
  ROSE_ASSERT(isSgTypeUnknown(braced_return->get_type()) == nullptr);
  ROSE_ASSERT(isSgTypeDefault(braced_return->get_type()) == nullptr);
  ROSE_ASSERT(SageInterface::isEquivalentType(
      braced_return->get_type(), factory->get_type()->get_return_type()));
}

void roundTrip(SgProject *project) {
  SgSourceFile *source = nullptr;
  for (SgFile *file : project->get_fileList()) {
    SgSourceFile *candidate = isSgSourceFile(file);
    if (candidate != nullptr && !candidate->get_isHeaderFile()) {
      ROSE_ASSERT(source == nullptr);
      source = candidate;
    }
  }
  ROSE_ASSERT(source != nullptr);

  using namespace Rose::AstJson;
  constexpr Checkpoint checkpoint = Checkpoint::PreOmpConstruction;
  AstFileRecord ast = parseAstFileJson(buildJson(source, checkpoint, source),
                                       checkpointName(checkpoint));
  SgSourceFile *copy = reconstructSourceFile(ast, source);
  replaceFileInProject(source, copy);
}

} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(frontendExitStatus(project) == 0);

  validateInitializerContracts(project);
  roundTrip(project);
  validateInitializerContracts(project);

  return backend(project);
}
