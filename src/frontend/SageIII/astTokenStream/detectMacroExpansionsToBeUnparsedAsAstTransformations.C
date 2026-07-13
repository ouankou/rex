#include "sage3basic.h"

#include "tokenStreamMapping.h"

#include <tuple>

using namespace std;

namespace {
class TransformationFinder : public SgSimpleProcessing {
public:
  TokenUnparseFrontierFileContext &frontierContext;
  bool found;
  explicit TransformationFinder(
      TokenUnparseFrontierFileContext &frontierContext)
      : frontierContext(frontierContext), found(false) {}

  void visit(SgNode *n) override {
    if (found) {
      return;
    }
    if (n == nullptr) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[macro-frontier]: traversal produced a "
              "null AST node\n");
      ROSE_ABORT();
    }
    if (n->get_containsTransformation()) {
      found = true;
      return;
    }
    if (SgStatement *statement = isSgStatement(n)) {
      if (frontierContext.isStatementMarkedForAstUnparse(statement)) {
        found = true;
        return;
      }
    }
    SgLocatedNode *located = isSgLocatedNode(n);
    if (located != nullptr && located->isTransformation()) {
      found = true;
      return;
    }
    if (located == nullptr || !located->get_isModified()) {
      return;
    }

    SgStatement *statement = SageInterface::getEnclosingStatement(located);
    if (statement == nullptr) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[macro-frontier]: modified node=%p "
              "type=%s in a macro expansion has no enclosing statement\n",
              static_cast<void *>(located), located->class_name().c_str());
      ROSE_ABORT();
    }
    Sg_File_Info *fileInfo = statement->get_file_info();
    if (fileInfo == nullptr) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[macro-frontier]: statement=%p type=%s "
              "in a modified macro expansion has no file information\n",
              static_cast<void *>(statement), statement->class_name().c_str());
      ROSE_ABORT();
    }
    if (statement->isOutputInCodeGeneration()) {
      found = true;
      return;
    }
    if (!fileInfo->isCompilerGenerated() && !fileInfo->isFrontendSpecific()) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[macro-frontier]: modified source "
              "statement=%p type=%s is suppressed from code generation\n",
              static_cast<void *>(statement), statement->class_name().c_str());
      ROSE_ABORT();
    }
  }
};

static bool
subtreeHasTransformation(SgStatement *statement,
                         TokenUnparseFrontierFileContext &frontierContext) {
  ASSERT_not_null(statement);
  if (statement->get_containsTransformation() ||
      statement->isTransformation() ||
      frontierContext.isStatementMarkedForAstUnparse(statement)) {
    return true;
  }

  TransformationFinder finder(frontierContext);
  finder.traverse(statement, preorder);
  return finder.found;
}

static void
markStatementsForMacro(MacroExpansion *macroExpansion,
                       const std::vector<SgStatement *> &statements,
                       TokenUnparseFrontierFileContext &frontierContext) {
  ASSERT_not_null(macroExpansion);
  if (statements.empty()) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[macro-frontier]: transformed macro=%p has "
            "no associated statements\n",
            static_cast<void *>(macroExpansion));
    ROSE_ABORT();
  }
  std::set<SgStatement *> expansionStatements(statements.begin(),
                                              statements.end());
  std::set<SgStatement *> atomicOwners;
  for (SgStatement *statement : expansionStatements) {
    if (statement == nullptr || statement->get_file_info() == nullptr) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[macro-frontier]: transformed macro=%p "
              "has a null statement or missing statement file information\n",
              static_cast<void *>(macroExpansion));
      ROSE_ABORT();
    }

    // SgForInitStatement is the structural container for the written
    // initializer payload.  It has no independent C/C++ source surface, so a
    // macro transformation must never publish it as an AST-emission owner.
    // The payload and enclosing SgForStatement are both present in the exact
    // expansion statement set and retain their lexical roles below.
    if (SgForInitStatement *forInit = isSgForInitStatement(statement)) {
      SgForStatement *owner = isSgForStatement(forInit->get_parent());
      const SgStatementPtrList &initializers = forInit->get_init_stmt();
      const SgNodePtrList ownerSuccessors =
          owner != nullptr ? owner->get_traversalSuccessorContainer()
                           : SgNodePtrList();
      if (owner == nullptr || owner->get_for_init_stmt() != forInit ||
          std::count(ownerSuccessors.begin(), ownerSuccessors.end(), forInit) !=
              1 ||
          initializers.size() != 1 || initializers.front() == nullptr ||
          initializers.front()->get_parent() != forInit) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[macro-frontier-owner]: macro=%p "
                "for-init=%p has no exact lexical payload and loop owner\n",
                static_cast<void *>(macroExpansion),
                static_cast<void *>(forInit));
        ROSE_ABORT();
      }
    } else {
      frontierContext.markStatementForAstUnparse(statement);
    }

    // A written macro invocation is one token-stream unit even when it expands
    // to several AST statements.  Once any expanded statement is transformed,
    // the first enclosing statement outside that expansion owns the complete
    // invocation boundary.  Mark that owner atomically so the unparser cannot
    // concatenate a surviving macro-name token with AST-emitted expansion
    // fragments.
    SgStatement *cursor = statement;
    SgStatement *parentStatement =
        SageInterface::getEnclosingStatement(cursor->get_parent());
    while (parentStatement != nullptr &&
           expansionStatements.find(parentStatement) !=
               expansionStatements.end()) {
      cursor = parentStatement;
      parentStatement =
          SageInterface::getEnclosingStatement(cursor->get_parent());
    }
    if (parentStatement == nullptr ||
        parentStatement->get_file_info() == nullptr) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[macro-frontier-owner]: transformed "
              "macro=%p statement=%p/%s has no exact enclosing statement "
              "outside the expansion\n",
              static_cast<void *>(macroExpansion),
              static_cast<void *>(statement), statement->class_name().c_str());
      ROSE_ABORT();
    }
    atomicOwners.insert(parentStatement);
  }

  for (SgStatement *owner : atomicOwners) {
    frontierContext.markStatementForAstUnparse(owner);
  }
}
} // namespace

void detectMacroExpansionsToBeUnparsedAsAstTransformations(
    SgSourceFile *sourceFile,
    TokenUnparseFrontierFileContext &frontierContext) {
  // If a statement associated with a macro expansion contains a transformation,
  // assign the complete expanded region to AST emission as one indivisible
  // source unit.
  ASSERT_not_null(sourceFile);

  std::map<SgStatement *, MacroExpansion *> &macroExpansionMap =
      sourceFile->get_macroExpansionMap();
  if (macroExpansionMap.empty()) {
    return;
  }
  Sg_File_Info *sourceFileInfo = sourceFile->get_file_info();
  if (sourceFileInfo == nullptr) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[macro-frontier-identity]: source file=%s "
            "has no file information\n",
            sourceFile->getFileName().c_str());
    ROSE_ABORT();
  }
  const int sourcePhysicalFileId = sourceFileInfo->get_physical_file_id();
  if (sourcePhysicalFileId < 0) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[macro-frontier-identity]: file=%s source "
            "file has invalid physical file id=%d\n",
            sourceFile->getFileName().c_str(), sourcePhysicalFileId);
    ROSE_ABORT();
  }

  std::map<MacroExpansion *, std::vector<SgStatement *>> expansionToStatements;
  for (std::map<SgStatement *, MacroExpansion *>::const_iterator it =
           macroExpansionMap.begin();
       it != macroExpansionMap.end(); ++it) {
    if (it->first == nullptr || it->second == nullptr) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[macro-frontier]: file=%s has a null "
              "statement or macro in its expansion map\n",
              sourceFile->getFileName().c_str());
      ROSE_ABORT();
    }
    Sg_File_Info *statementFileInfo = it->first->get_file_info();
    if (statementFileInfo == nullptr) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[macro-frontier-identity]: file=%s has "
              "a macro statement without file information\n",
              sourceFile->getFileName().c_str());
      ROSE_ABORT();
    }
    if (statementFileInfo->get_physical_file_id() < 0) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[macro-frontier-identity]: file=%s "
              "macro=%s mapped statement=%p/%s has invalid physical file "
              "id=%d\n",
              sourceFile->getFileName().c_str(), it->second->macro_name.c_str(),
              static_cast<void *>(it->first), it->first->class_name().c_str(),
              statementFileInfo->get_physical_file_id());
      ROSE_ABORT();
    }
    if (statementFileInfo->get_physical_file_id() != sourcePhysicalFileId) {
      continue;
    }
    expansionToStatements[it->second].push_back(it->first);
  }
  if (expansionToStatements.empty()) {
    return;
  }

  // Some expanded statements have no token mapping of their own (a macro can
  // expand to a declaration followed by an expression, for example).  The
  // frontend still assigns every such statement the exact physical file,
  // line, and column of the written invocation.  Complete each expansion from
  // that source identity so transforming any one expanded statement transfers
  // the entire invocation from token ownership to AST ownership.
  using MacroSourceKey = std::tuple<int, int, int>;
  std::map<MacroSourceKey, MacroExpansion *> expansionsBySourceLocation;
  for (const auto &entry : expansionToStatements) {
    MacroExpansion *macroExpansion = entry.first;
    if (macroExpansion == nullptr) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[macro-frontier-identity]: file=%s "
              "macro=<null>\n",
              sourceFile->getFileName().c_str());
      ROSE_ABORT();
    }
    if (macroExpansion->macro_name.empty() || macroExpansion->line <= 0 ||
        macroExpansion->column < 0 ||
        macroExpansion->token_interval.begin < 0 ||
        macroExpansion->token_interval.end <=
            macroExpansion->token_interval.begin) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[macro-frontier-identity]: file=%s "
              "macro=%p name=%s source=%d:%d tokens=[%d,%d)\n",
              sourceFile->getFileName().c_str(),
              static_cast<void *>(macroExpansion),
              macroExpansion->macro_name.c_str(), macroExpansion->line,
              macroExpansion->column, macroExpansion->token_interval.begin,
              macroExpansion->token_interval.end);
      ROSE_ABORT();
    }

    if (entry.second.empty()) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[macro-frontier-identity]: file=%s "
              "macro=%s has no mapped source statement\n",
              sourceFile->getFileName().c_str(),
              macroExpansion->macro_name.c_str());
      ROSE_ABORT();
    }
    SgStatement *firstStatement = entry.second.front();
    Sg_File_Info *firstFileInfo =
        firstStatement != nullptr ? firstStatement->get_file_info() : nullptr;
    if (firstFileInfo == nullptr || firstFileInfo->get_physical_file_id() < 0) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[macro-frontier-identity]: file=%s "
              "macro=%s has a first mapped statement without exact physical "
              "file information\n",
              sourceFile->getFileName().c_str(),
              macroExpansion->macro_name.c_str());
      ROSE_ABORT();
    }
    const int physicalFileId = firstFileInfo->get_physical_file_id();
    for (SgStatement *statement : entry.second) {
      Sg_File_Info *fileInfo =
          statement != nullptr ? statement->get_file_info() : nullptr;
      if (fileInfo == nullptr || fileInfo->get_physical_file_id() < 0) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[macro-frontier-identity]: file=%s "
                "macro=%s has a mapped statement without exact physical file "
                "information\n",
                sourceFile->getFileName().c_str(),
                macroExpansion->macro_name.c_str());
        ROSE_ABORT();
      }
      if (physicalFileId != fileInfo->get_physical_file_id()) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[macro-frontier-identity]: file=%s "
                "macro=%s spans physical files %d and %d\n",
                sourceFile->getFileName().c_str(),
                macroExpansion->macro_name.c_str(), physicalFileId,
                fileInfo->get_physical_file_id());
        ROSE_ABORT();
      }
    }

    MacroSourceKey key(physicalFileId, macroExpansion->line,
                       macroExpansion->column);
    auto inserted = expansionsBySourceLocation.emplace(key, macroExpansion);
    if (!inserted.second && inserted.first->second != macroExpansion) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[macro-frontier-identity]: file=%s has "
              "multiple macro expansions at physical-file=%d source=%d:%d\n",
              sourceFile->getFileName().c_str(), physicalFileId,
              macroExpansion->line, macroExpansion->column);
      ROSE_ABORT();
    }
  }

  for (SgNode *candidate : NodeQuery::querySubTree(sourceFile, V_SgStatement)) {
    SgStatement *statement = isSgStatement(candidate);
    Sg_File_Info *fileInfo =
        statement != nullptr ? statement->get_file_info() : nullptr;
    if (fileInfo == nullptr) {
      continue;
    }
    MacroSourceKey key(fileInfo->get_physical_file_id(), fileInfo->get_line(),
                       fileInfo->get_col());
    auto expansion = expansionsBySourceLocation.find(key);
    if (expansion == expansionsBySourceLocation.end()) {
      continue;
    }
    std::vector<SgStatement *> &statements =
        expansionToStatements[expansion->second];
    if (std::find(statements.begin(), statements.end(), statement) ==
        statements.end()) {
      statements.push_back(statement);
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

    std::vector<SgStatement *> allStatements = statementsFromMap;
    for (SgStatement *statement : associatedStatements) {
      if (std::find(allStatements.begin(), allStatements.end(), statement) ==
          allStatements.end()) {
        allStatements.push_back(statement);
      }
    }

    if (!needsTransformation) {
      for (SgStatement *statement : allStatements) {
        if (subtreeHasTransformation(statement, frontierContext)) {
          needsTransformation = true;
          break;
        }
      }
    }

    if (needsTransformation) {
      markStatementsForMacro(macroExpansion, allStatements, frontierContext);
    }
  }
}
