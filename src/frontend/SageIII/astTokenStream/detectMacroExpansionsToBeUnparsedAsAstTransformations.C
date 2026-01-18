#include "previousAndNextNode.h"

#include "sage3basic.h"

#include "tokenStreamMapping.h"

using namespace std;

namespace {
class TransformationFinder : public SgSimpleProcessing {
public:
  bool found;
  TransformationFinder() : found(false) {}

  void visit(SgNode *n) override {
    if (found || n == NULL) {
      return;
    }
    if (n->get_containsTransformation()) {
      found = true;
      return;
    }
    SgLocatedNode *located = isSgLocatedNode(n);
    if (located != NULL && located->isTransformation()) {
      found = true;
    }
  }
};

static bool subtreeHasTransformation(SgStatement *statement) {
  if (statement == NULL) {
    return false;
  }
  if (statement->get_containsTransformation() ||
      statement->isTransformation()) {
    return true;
  }

  TransformationFinder finder;
  finder.traverse(statement, preorder);
  return finder.found;
}

static void
markStatementsForMacro(MacroExpansion *macroExpansion,
                       const std::vector<SgStatement *> &statements) {
  if (macroExpansion == NULL) {
    return;
  }
  for (SgStatement *statement : statements) {
    if (statement == NULL) {
      continue;
    }
    if (statement->get_file_info() == NULL) {
      continue;
    }
    statement->setTransformation();
    statement->setOutputInCodeGeneration();
  }
  macroExpansion->isTransformed = true;
}
} // namespace

void detectMacroExpansionsToBeUnparsedAsAstTransformations(
    SgSourceFile *sourceFile) {
  // If a statement associated with a macro expansion contains a transformation,
  // mark all statements for that macro expansion as transformations so that the
  // unparser falls back to AST unparsing for the expanded macro region.
  if (sourceFile == NULL) {
    return;
  }

  std::map<SgStatement *, MacroExpansion *> &macroExpansionMap =
      sourceFile->get_macroExpansionMap();
  if (macroExpansionMap.empty()) {
    return;
  }

  std::map<MacroExpansion *, std::vector<SgStatement *>> expansionToStatements;
  for (std::map<SgStatement *, MacroExpansion *>::const_iterator it =
           macroExpansionMap.begin();
       it != macroExpansionMap.end(); ++it) {
    if (it->second != NULL) {
      expansionToStatements[it->second].push_back(it->first);
    }
  }

  for (std::map<MacroExpansion *, std::vector<SgStatement *>>::const_iterator
           it = expansionToStatements.begin();
       it != expansionToStatements.end(); ++it) {
    MacroExpansion *macroExpansion = it->first;
    const std::vector<SgStatement *> &statementsFromMap = it->second;

    bool needsTransformation = macroExpansion->isTransformed;
    const std::vector<SgStatement *> &associatedStatements =
        macroExpansion->associatedStatementVector;

    if (!needsTransformation) {
      const std::vector<SgStatement *> &statementsToCheck =
          !associatedStatements.empty() ? associatedStatements
                                        : statementsFromMap;
      for (SgStatement *statement : statementsToCheck) {
        if (subtreeHasTransformation(statement)) {
          needsTransformation = true;
          break;
        }
      }
    }

    if (needsTransformation) {
      const std::vector<SgStatement *> &statementsToMark =
          !associatedStatements.empty() ? associatedStatements
                                        : statementsFromMap;
      markStatementsForMacro(macroExpansion, statementsToMark);
    }
  }
}
