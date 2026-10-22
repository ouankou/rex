#include "sage3basic.h"

#include "tokenStreamMapping.h"
using namespace std;

namespace {
bool suppressedCompilerGeneratedNode(SgLocatedNode *node) {
  if (node == nullptr || node->isOutputInCodeGeneration()) {
    return false;
  }

  Sg_File_Info *fileInfo = node->get_file_info();
  if (fileInfo == nullptr) {
    return false;
  }

  return fileInfo->isCompilerGenerated() || fileInfo->isFrontendSpecific();
}

bool contributesVisibleTransformation(SgLocatedNode *node) {
  if (node == nullptr) {
    return false;
  }

  if (!node->isTransformation() && !node->get_containsTransformation()) {
    return false;
  }

  return !suppressedCompilerGeneratedNode(node);
}

bool isExactSemanticAuxiliaryStatement(
    SgNode *node, std::unordered_map<SgNode *, bool> &classifications,
    std::unordered_map<SgNode *, std::unordered_map<SgNode *, size_t>>
        &childCounts) {
  ROSE_ASSERT(node != nullptr);
  const auto known = classifications.find(node);
  if (known != classifications.end())
    return known->second;
  std::vector<SgNode *> path{node};
  auto publish = [&](bool result) {
    for (auto *ancestor : path) {
      const auto entry = classifications.emplace(ancestor, result);
      ROSE_ASSERT(entry.second || entry.first->second == result);
    }
    return result;
  };
  std::unordered_set<SgNode *> visited{node};
  SgNode *child = node;
  for (SgNode *parent = node != nullptr ? node->get_parent() : nullptr;
       parent != nullptr; child = parent, parent = parent->get_parent()) {
    if (!visited.insert(parent).second) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[token-frontier-auxiliary-owner]: "
              "node=%p/%s has a parent cycle\n",
              static_cast<void *>(node),
              node != nullptr ? node->class_name().c_str() : "<null>");
      ROSE_ABORT();
    }
    const auto index = childCounts.try_emplace(parent);
    if (index.second) {
      for (auto *successor : parent->get_traversalSuccessorContainer())
        ++index.first->second[successor];
    }
    const auto occurrence = index.first->second.find(child);
    if (child == nullptr || child->get_parent() != parent ||
        occurrence == index.first->second.end() || occurrence->second != 1) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[token-frontier-auxiliary-owner]: "
              "node=%p/%s has malformed structural ancestry at child=%p/%s "
              "parent=%p/%s\n",
              static_cast<void *>(node),
              node != nullptr ? node->class_name().c_str() : "<null>",
              static_cast<void *>(child),
              child != nullptr ? child->class_name().c_str() : "<null>",
              static_cast<void *>(parent), parent->class_name().c_str());
      ROSE_ABORT();
    }

    SgAuxiliaryDeclarationList *auxiliary =
        isSgAuxiliaryDeclarationList(parent);
    if (auxiliary == nullptr) {
      const auto ancestor = classifications.find(parent);
      if (ancestor != classifications.end())
        return publish(ancestor->second);
      path.push_back(parent);
      continue;
    }
    SgDeclarationStatement *declaration = isSgDeclarationStatement(child);
    SgScopeStatement *scope = isSgScopeStatement(auxiliary->get_parent());
    if (declaration == nullptr || scope == nullptr ||
        scope->get_auxiliary_declarations() != auxiliary ||
        std::count(auxiliary->get_declarations().begin(),
                   auxiliary->get_declarations().end(), declaration) != 1) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[token-frontier-auxiliary-owner]: "
              "node=%p/%s has malformed auxiliary ownership\n",
              static_cast<void *>(node),
              node != nullptr ? node->class_name().c_str() : "<null>");
      ROSE_ABORT();
    }

    auto exactSemanticPosition = [declaration](Sg_File_Info *position) {
      return SageInterface::hasExactSemanticFrontendSourcePosition(declaration,
                                                                   position);
    };
    Sg_File_Info *start = declaration->get_startOfConstruct();
    Sg_File_Info *end = declaration->get_endOfConstruct();
    if (!exactSemanticPosition(start) || !exactSemanticPosition(end) ||
        start->get_file_id() != end->get_file_id() ||
        start->get_physical_file_id() != end->get_physical_file_id()) {
      fprintf(
          stderr,
          "REX_UNPARSE_INVARIANT[token-frontier-auxiliary-provenance]: "
          "declaration=%p/%s name=%s parent=%p/%s scope=%p/%s "
          "start=%p parent=%p file=%d physical=%d line=%d col=%d "
          "raw-line=%d raw-col=%d "
          "compiler=%d frontend=%d transformation=%d unavailable=%d "
          "output=%d; end=%p parent=%p file=%d physical=%d line=%d "
          "col=%d raw-line=%d raw-col=%d compiler=%d frontend=%d "
          "transformation=%d "
          "unavailable=%d output=%d does not have exact semantic "
          "structural provenance\n",
          static_cast<void *>(declaration), declaration->class_name().c_str(),
          SageInterface::get_name(declaration).c_str(),
          static_cast<void *>(declaration->get_parent()),
          declaration->get_parent() != nullptr
              ? declaration->get_parent()->class_name().c_str()
              : "<null>",
          static_cast<void *>(declaration->get_scope()),
          declaration->get_scope() != nullptr
              ? declaration->get_scope()->class_name().c_str()
              : "<null>",
          static_cast<void *>(start),
          static_cast<void *>(start != nullptr ? start->get_parent() : nullptr),
          start != nullptr ? start->get_file_id() : Sg_File_Info::BAD_FILE_ID,
          start != nullptr ? start->get_physical_file_id()
                           : Sg_File_Info::BAD_FILE_ID,
          start != nullptr ? start->get_line() : 0,
          start != nullptr ? start->get_col() : 0,
          start != nullptr ? start->get_raw_line() : 0,
          start != nullptr ? start->get_raw_col() : 0,
          start != nullptr && start->isCompilerGenerated() ? 1 : 0,
          start != nullptr && start->isFrontendSpecific() ? 1 : 0,
          start != nullptr && start->isTransformation() ? 1 : 0,
          start != nullptr && start->isSourcePositionUnavailableInFrontend()
              ? 1
              : 0,
          start != nullptr && start->isOutputInCodeGeneration() ? 1 : 0,
          static_cast<void *>(end),
          static_cast<void *>(end != nullptr ? end->get_parent() : nullptr),
          end != nullptr ? end->get_file_id() : Sg_File_Info::BAD_FILE_ID,
          end != nullptr ? end->get_physical_file_id()
                         : Sg_File_Info::BAD_FILE_ID,
          end != nullptr ? end->get_line() : 0,
          end != nullptr ? end->get_col() : 0,
          end != nullptr ? end->get_raw_line() : 0,
          end != nullptr ? end->get_raw_col() : 0,
          end != nullptr && end->isCompilerGenerated() ? 1 : 0,
          end != nullptr && end->isFrontendSpecific() ? 1 : 0,
          end != nullptr && end->isTransformation() ? 1 : 0,
          end != nullptr && end->isSourcePositionUnavailableInFrontend() ? 1
                                                                         : 0,
          end != nullptr && end->isOutputInCodeGeneration() ? 1 : 0);
      ROSE_ABORT();
    }
    return publish(true);
  }
  return publish(false);
}
} // namespace

void SimpleFrontierDetectionForTokenStreamMapping::atTraversalStart() {
  semanticAuxiliaryClassification.clear();
  structuralChildCounts.clear();
}

void SimpleFrontierDetectionForTokenStreamMapping::atTraversalEnd() {
  semanticAuxiliaryClassification.clear();
  structuralChildCounts.clear();
}

SimpleFrontierDetectionForTokenStreamMapping_InheritedAttribute::
    SimpleFrontierDetectionForTokenStreamMapping_InheritedAttribute() {
  sourceFile = nullptr;
  processChildNodes = false;

  isFrontier = false;

  unparseUsingTokenStream = false;
  unparseFromTheAST = false;
  containsNodesToBeUnparsedFromTheAST = false;

  // DQ (11/13/2018): I want to use the other constructor that will always at
  // least set the SgSourceFile pointer.
  printf("Exitng as a test! \n");
  ROSE_ABORT();
}

SimpleFrontierDetectionForTokenStreamMapping_InheritedAttribute::
    SimpleFrontierDetectionForTokenStreamMapping_InheritedAttribute(
        SgSourceFile *input_sourceFile) {
  sourceFile = input_sourceFile;
  processChildNodes = false;

  isFrontier = false;

  unparseUsingTokenStream = false;
  unparseFromTheAST = false;
  containsNodesToBeUnparsedFromTheAST = false;
}

SimpleFrontierDetectionForTokenStreamMapping_InheritedAttribute::
    SimpleFrontierDetectionForTokenStreamMapping_InheritedAttribute(
        SgSourceFile *input_sourceFile, int /*start*/, int /*end*/,
        bool processed) {
  sourceFile = input_sourceFile;
  processChildNodes = processed;

  isFrontier = false;

  unparseUsingTokenStream = false;
  unparseFromTheAST = false;
  containsNodesToBeUnparsedFromTheAST = false;
}

SimpleFrontierDetectionForTokenStreamMapping_InheritedAttribute::
    SimpleFrontierDetectionForTokenStreamMapping_InheritedAttribute(
        const SimpleFrontierDetectionForTokenStreamMapping_InheritedAttribute
            &X) {
  sourceFile = X.sourceFile;
  processChildNodes = X.processChildNodes;

  isFrontier = X.isFrontier;

  unparseUsingTokenStream = X.unparseUsingTokenStream;
  unparseFromTheAST = X.unparseFromTheAST;
  containsNodesToBeUnparsedFromTheAST = X.containsNodesToBeUnparsedFromTheAST;
}

SimpleFrontierDetectionForTokenStreamMapping_SynthesizedAttribute::
    SimpleFrontierDetectionForTokenStreamMapping_SynthesizedAttribute() {
  node = nullptr;
  isFrontier = false;

  unparseUsingTokenStream = false;
  unparseFromTheAST = false;
  containsNodesToBeUnparsedFromTheAST = false;

  containsNodesToBeUnparsedFromTheTokenStream = false;
}

SimpleFrontierDetectionForTokenStreamMapping_SynthesizedAttribute::
    SimpleFrontierDetectionForTokenStreamMapping_SynthesizedAttribute(
        SgNode *n) {
  node = isSgStatement(n);
  isFrontier = false;

  unparseUsingTokenStream = false;
  unparseFromTheAST = false;
  containsNodesToBeUnparsedFromTheAST = false;

  containsNodesToBeUnparsedFromTheTokenStream = false;
}

SimpleFrontierDetectionForTokenStreamMapping_SynthesizedAttribute::
    SimpleFrontierDetectionForTokenStreamMapping_SynthesizedAttribute(
        const SimpleFrontierDetectionForTokenStreamMapping_SynthesizedAttribute
            &X) {
  node = X.node;
  isFrontier = X.isFrontier;

  unparseUsingTokenStream = X.unparseUsingTokenStream;
  unparseFromTheAST = X.unparseFromTheAST;
  containsNodesToBeUnparsedFromTheAST = X.containsNodesToBeUnparsedFromTheAST;

  containsNodesToBeUnparsedFromTheTokenStream =
      X.containsNodesToBeUnparsedFromTheTokenStream;
}

SimpleFrontierDetectionForTokenStreamMapping::
    SimpleFrontierDetectionForTokenStreamMapping(
        TokenUnparseFrontierFileContext &frontierContext)
    : frontierContext(frontierContext) {}

SimpleFrontierDetectionForTokenStreamMapping_InheritedAttribute
SimpleFrontierDetectionForTokenStreamMapping::evaluateInheritedAttribute(
    SgNode *n, SimpleFrontierDetectionForTokenStreamMapping_InheritedAttribute
                   inheritedAttribute) {

  ASSERT_not_null(inheritedAttribute.sourceFile);
  SimpleFrontierDetectionForTokenStreamMapping_InheritedAttribute
      returnAttribute(inheritedAttribute.sourceFile);

  ASSERT_not_null(inheritedAttribute.sourceFile);

  if (isSgGlobal(n) != nullptr) {
    SgGlobal *globalScope = isSgGlobal(n);
    ASSERT_not_null(globalScope->get_parent());
  }

  const bool isStructuralFileContainer = isSgProject(n) != nullptr ||
                                         isSgFileList(n) != nullptr ||
                                         isSgFile(n) != nullptr;
  // SgCatchStatementSeq is the typed non-lexical owner of a try statement's
  // handler vector. Appending handlers changes that container during frontend
  // construction, but the sequence never owns an independent source or output
  // boundary. The strict frontier pass validates its exact try/handler edges
  // and propagates any real handler transformation to the SgTryStmt.
  if (isSgCatchStatementSeq(n) != nullptr) {
    return returnAttribute;
  }
  // Auxiliary declarations are semantic lookup/type infrastructure.  They are
  // traversable typed children of a scope, but they are never token-stream or
  // AST output frontiers.  Suppress only the complete reciprocal ownership and
  // semantic-provenance contract; reject malformed ownership and provenance.
  if (isExactSemanticAuxiliaryStatement(n, semanticAuxiliaryClassification,
                                        structuralChildCounts)) {
    return returnAttribute;
  }
  if (n->get_isModified() && !isStructuralFileContainer) {
    SgStatement *statement = SageInterface::getEnclosingStatement(n);
    if (statement == nullptr) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[token-frontier]: modified node=%p type=%s "
              "has no enclosing statement\n",
              static_cast<void *>(n), n->class_name().c_str());
      ROSE_ABORT();
    }
    if (isSgStatement(n) != nullptr && statement != n) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[token-frontier]: modified statement=%p "
              "resolved to a different enclosing statement=%p\n",
              static_cast<void *>(n), static_cast<void *>(statement));
      ROSE_ABORT();
    }

    Sg_File_Info *statementInfo = statement->get_file_info();
    if (statementInfo == nullptr) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[token-frontier]: modified node=%p type=%s "
              "maps to statement=%p without file information\n",
              static_cast<void *>(n), n->class_name().c_str(),
              static_cast<void *>(statement));
      ROSE_ABORT();
    }

    if (!statement->isOutputInCodeGeneration() &&
        (statementInfo->isCompilerGenerated() ||
         statementInfo->isFrontendSpecific())) {
      return returnAttribute;
    }

    if (!statement->isOutputInCodeGeneration()) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[token-frontier]: modified statement=%p "
              "type=%s is suppressed from code generation\n",
              static_cast<void *>(statement), statement->class_name().c_str());
      ROSE_ABORT();
    }

    if (contributesVisibleTransformation(statement)) {
      // Transformation nodes are deliberately detached from the immutable
      // token stream and carry transformation file information rather than a
      // physical source-file ID.  The synthesized pass classifies the exact
      // containing frontier from this transformation marker; forcing the
      // declaration itself into an atomic AST region would discard surrounding
      // token-owned preprocessing boundaries.
      return returnAttribute;
    }

    Sg_File_Info *sourceInfo = inheritedAttribute.sourceFile->get_file_info();
    if (sourceInfo == nullptr || sourceInfo->get_physical_file_id() < 0 ||
        statementInfo->get_physical_file_id() < 0) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[token-frontier]: source=%s or modified "
              "statement=%p type=%s name=%s parent=%p parent-type=%s has no "
              "physical file ownership "
              "(source-file-id=%d source-physical-file-id=%d "
              "statement-file-id=%d statement-physical-file-id=%d "
              "output=%s compiler-generated=%s frontend-specific=%s)\n",
              inheritedAttribute.sourceFile->getFileName().c_str(),
              static_cast<void *>(statement), statement->class_name().c_str(),
              SageInterface::get_name(statement).c_str(),
              static_cast<void *>(statement->get_parent()),
              statement->get_parent() == nullptr
                  ? "<null>"
                  : statement->get_parent()->class_name().c_str(),
              sourceInfo == nullptr ? Sg_File_Info::NULL_FILE_ID
                                    : sourceInfo->get_file_id(),
              sourceInfo == nullptr ? Sg_File_Info::NULL_FILE_ID
                                    : sourceInfo->get_physical_file_id(),
              statementInfo->get_file_id(),
              statementInfo->get_physical_file_id(),
              statementInfo->isOutputInCodeGeneration() ? "true" : "false",
              statementInfo->isCompilerGenerated() ? "true" : "false",
              statementInfo->isFrontendSpecific() ? "true" : "false");
      ROSE_ABORT();
    }
    if (sourceInfo->get_physical_file_id() !=
        statementInfo->get_physical_file_id()) {
      return returnAttribute;
    }

    frontierContext.markStatementForAstUnparse(statement);
  }

  return returnAttribute;
}

SimpleFrontierDetectionForTokenStreamMapping_SynthesizedAttribute
SimpleFrontierDetectionForTokenStreamMapping::evaluateSynthesizedAttribute(
    SgNode *n,
    SimpleFrontierDetectionForTokenStreamMapping_InheritedAttribute
        inheritedAttribute,
    SubTreeSynthesizedAttributes synthesizedAttributeList) {
  ASSERT_not_null(n);

  SimpleFrontierDetectionForTokenStreamMapping_SynthesizedAttribute
      returnAttribute(n);

  SgStatement *currentStatement = isSgStatement(n);
  bool containsAstUnparse =
      currentStatement != nullptr &&
      (frontierContext.isStatementMarkedForAstUnparse(currentStatement) ||
       contributesVisibleTransformation(currentStatement));

  for (const auto &childAttribute : synthesizedAttributeList) {
    if (!childAttribute.containsNodesToBeUnparsedFromTheAST) {
      continue;
    }
    // A lexical container can begin in an included file while still owning
    // later statements from the materialized header whose frontier is being
    // built. Preserve the file-context-specific transformation bit through
    // those mixed-physical-file containers; the inherited pass only seeds
    // statements owned by this exact SgSourceFile.
    containsAstUnparse = true;
  }

  returnAttribute.containsNodesToBeUnparsedFromTheAST = containsAstUnparse;
  returnAttribute.containsNodesToBeUnparsedFromTheTokenStream =
      !containsAstUnparse;
  returnAttribute.unparseFromTheAST = containsAstUnparse;
  returnAttribute.unparseUsingTokenStream = !containsAstUnparse;
  if (currentStatement != nullptr && containsAstUnparse) {
    frontierContext.markStatementAsContainingAstUnparse(currentStatement);
  }

  return returnAttribute;
}

void simpleFrontierDetectionForTokenStreamMapping(
    SgSourceFile *sourceFile, bool traverseHeaderFiles,
    TokenUnparseFrontierFileContext &frontierContext, SgNode *traversalRoot) {
  TimingPerformance timer(
      "AST Simple Frontier Detection For Token Stream Mapping:");

  ASSERT_not_null(sourceFile);
  SimpleFrontierDetectionForTokenStreamMapping_InheritedAttribute
      inheritedAttribute(sourceFile);
  SimpleFrontierDetectionForTokenStreamMapping fdTraversal(frontierContext);

  if (traversalRoot != nullptr) {
    fdTraversal.traverse(traversalRoot, inheritedAttribute);
  } else if (traverseHeaderFiles == true) {
    fdTraversal.traverse(sourceFile, inheritedAttribute);
  } else {
    fdTraversal.traverseWithinFile(sourceFile, inheritedAttribute);
  }

  // Function and range-for headers interleave several semantic children inside
  // one source-level construct.  A transformed header must therefore own its
  // source boundary atomically.  A transformation in a function body does not
  // make the function header an AST region: retaining the header token owner is
  // required for source spellings that the semantic AST cannot represent, such
  // as conditional directives splitting a return type from a function name.
  std::set<SgStatement *> atomicStatements;
  for (SgStatement *seed : frontierContext.statementsToUnparseFromAst) {
    bool crossedFunctionBody = false;
    for (SgNode *cursor = seed; cursor != nullptr;
         cursor = cursor->get_parent()) {
      if (SgRangeBasedForStatement *rangeFor =
              isSgRangeBasedForStatement(cursor)) {
        atomicStatements.insert(rangeFor);
      }
      if (isSgFunctionDefinition(cursor) != nullptr) {
        crossedFunctionBody = true;
      }
      if (SgFunctionDeclaration *function = isSgFunctionDeclaration(cursor)) {
        if (!crossedFunctionBody) {
          atomicStatements.insert(function);
        }
        break;
      }
    }
  }
  for (SgStatement *statement : atomicStatements) {
    frontierContext.markStatementForAstUnparse(statement);
  }

  frontierContext.finishTransformationAnalysis();
}
