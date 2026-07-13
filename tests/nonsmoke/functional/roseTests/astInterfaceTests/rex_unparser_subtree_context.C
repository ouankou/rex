#include "rose.h"

#include <iostream>
#include <string>

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  const std::vector<SgNode *> definitions =
      NodeQuery::querySubTree(project, V_SgFunctionDefinition);
  if (definitions.size() != 1) {
    std::cerr << "expected one function definition, found "
              << definitions.size() << '\n';
    return 1;
  }

  SgFunctionDefinition *definition =
      isSgFunctionDefinition(definitions.front());
  if (definition == nullptr) {
    std::cerr << "function-definition query returned the wrong node kind\n";
    return 1;
  }
  const std::string output = definition->unparseToString();
  if (output.find("new") == std::string::npos ||
      output.find("value") == std::string::npos) {
    std::cerr << "isolated function-definition unparse lost its nested "
                 "constructor expression\n";
    return 1;
  }

  const std::vector<SgNode *> constructors =
      NodeQuery::querySubTree(definition, V_SgConstructorInitializer);
  if (constructors.size() != 1) {
    std::cerr << "expected one constructor initializer, found "
              << constructors.size() << '\n';
    return 1;
  }
  SgConstructorInitializer *constructor =
      isSgConstructorInitializer(constructors.front());
  if (constructor == nullptr) {
    std::cerr << "constructor query returned the wrong node kind\n";
    return 1;
  }
  // An expression-root diagnostic must use the structurally enclosing
  // declaration as the exact qualification use site.  The constructor owned
  // by `new` intentionally has no standalone name in its output, so successful
  // completion is the contract here.
  (void)constructor->unparseToString();

  const std::vector<SgNode *> functionReferences =
      NodeQuery::querySubTree(definition, V_SgFunctionRefExp);
  SgFunctionRefExp *sinkReference = nullptr;
  for (SgNode *node : functionReferences) {
    SgFunctionRefExp *reference = isSgFunctionRefExp(node);
    if (reference != nullptr &&
        reference->getAssociatedFunctionDeclaration() != nullptr &&
        reference->getAssociatedFunctionDeclaration()->get_name() ==
            "rex_subtree_sink") {
      if (sinkReference != nullptr) {
        std::cerr << "found duplicate rex_subtree_sink references\n";
        return 1;
      }
      sinkReference = reference;
    }
  }
  if (sinkReference == nullptr ||
      sinkReference->unparseToString() != "rex_subtree_sink") {
    std::cerr << "isolated source-owned function reference did not consume "
                 "its preceding declaration visibility\n";
    return 1;
  }

  const std::vector<SgNode *> loops =
      NodeQuery::querySubTree(definition, V_SgForStatement);
  if (loops.size() != 1) {
    std::cerr << "expected one for statement, found " << loops.size() << '\n';
    return 1;
  }
  SgForStatement *loop = isSgForStatement(loops.front());
  if (loop == nullptr) {
    std::cerr << "for-statement query returned the wrong node kind\n";
    return 1;
  }
  const std::string loopOutput = loop->unparseToString();
  if (loopOutput.find("index") == std::string::npos ||
      loopOutput.find("result") == std::string::npos ||
      loopOutput.find("#pragma rex_subtree_marker") == std::string::npos) {
    std::cerr << "isolated for-statement unparse lost a nested statement use "
                 "site\n";
    return 1;
  }
  if (loopOutput.find("\n#pragma rex_subtree_marker\n") == std::string::npos ||
      loopOutput.find("#pragma rex_subtree_markerresult") !=
          std::string::npos) {
    std::cerr << "isolated statement diagnostic did not preserve the typed "
                 "dense pragma boundary\n";
    return 1;
  }
  return 0;
}
