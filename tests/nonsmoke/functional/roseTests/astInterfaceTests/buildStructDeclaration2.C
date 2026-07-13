// Liao, 11/9/2009
// Demonstrate how to build a struct declaration embedded into a variable decl
//
// SageBuilder contains the AST nodes/subtrees builders
// SageInterface contains any other AST utility tools
//-------------------------------------------------------------------
#include "rose.h"

#include <cstdlib>

using namespace SageBuilder;
using namespace SageInterface;

int main(int argc, char *argv[]) {
  // grab the scope in which AST will be added
  SgProject *project = frontend(argc, argv);
  SgGlobal *globalScope = getFirstGlobalScope(project);
  ROSE_ASSERT(globalScope);

  if (std::getenv("REX_TEST_REJECT_EMPTY_NAMED_STRUCT") != nullptr) {
    buildStructDeclaration(declaration_ownership::sourceLexical(), "",
                           globalScope);
    ROSE_ABORT();
  }

  // Build an anonymous struct here
  // top-down construction, with scope ready
  SgClassDeclaration *decl = buildAnonymousStructDeclaration(
      declaration_ownership::embeddedDeclaratorChild(),
      "__rex_internal_anonymous_struct_build_struct_declaration_2",
      globalScope);
  ROSE_ASSERT(!decl->get_name().is_null());
  ROSE_ASSERT(decl->get_isUnNamed());

  // build a member variable inside the structure
  SgClassDefinition *def = decl->get_definition();
  SgVariableDeclaration *varDecl =
      buildVariableDeclaration(SgName("i"), buildIntType(), NULL, def);
  // Insert the  member variable
  appendStatement(varDecl, def);

  // insert the struct declaration
  //  This is not necessary and will cause the struct to be unparsed twice,
  //  both from global scope and from the later variable declaration
  //  but SageInterface::setBaseTypeDefiningDeclaration() will take care of this
  //  even if you accidentally inserted it here.
  // appendStatement (decl,globalScope);

  // Declare a struct variable
  SgVariableDeclaration *varDecl2 =
      SageBuilder::buildVariableDeclarationWithEmbeddedTag(
          "temp", decl->get_type(), NULL, globalScope, nullptr, decl);
  appendStatement(varDecl2, globalScope);

  AstTests::runAllTests(project);
  return backend(project);
}
