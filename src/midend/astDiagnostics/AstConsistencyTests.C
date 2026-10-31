// Author: Markus Schordan
// $Id: AstConsistencyTests.C,v 1.8 2008/01/25 02:25:46 dquinlan Exp $

// tps (01/14/2010) : Switching from rose.h to sage3.
#include "AstNodes/Expression/OpenMPModifierValidation.h"
#include "sage3basic.h"

#include "rose_config.h"
// tps : needed to define this here as it is defined in rose.h
#include "AstDiagnostics.h"

#include "roseInternal.h"

#include "AstConsistencyTests.h"

#include "AstTraversal.h"

#include <sstream>

using namespace std;
using namespace Rose;

namespace {
void rosePhaseTrace(const char *phase) {
  if (getenv("ROSE_PHASE_TRACE") != nullptr) {
    fprintf(stderr, "ROSE_PHASE %s\n", phase);
    fflush(stderr);
  }
}

bool isOpenmpNonentityDirective(const SgNode *node) {
  return isSgOmpBeginDeclareVariantStatement(node) != NULL ||
         isSgOmpEndDeclareVariantStatement(node) != NULL ||
         isSgOmpDeclareTargetStatement(node) != NULL ||
         isSgOmpEndDeclareTargetStatement(node) != NULL ||
         isSgOmpBeginDeclareTargetStatement(node) != NULL ||
         isSgOmpAssumesStatement(node) != NULL ||
         isSgOmpBeginAssumesStatement(node) != NULL ||
         isSgOmpEndAssumesStatement(node) != NULL ||
         isSgOmpGroupprivateStatement(node) != NULL ||
         isSgOmpRequiresStatement(node) != NULL;
}

SgArrayType *ordinaryFortranArrayType(SgType *type) {
  while (type != nullptr) {
    if (SgModifierType *modifier = isSgModifierType(type)) {
      type = modifier->get_base_type();
      continue;
    }
    if (SgPointerType *pointer = isSgPointerType(type)) {
      type = pointer->get_base_type();
      continue;
    }
    if (SgArrayType *array = isSgArrayType(type)) {
      if (!array->get_isCoArray()) {
        return array;
      }
      type = array->get_base_type();
      continue;
    }
    return nullptr;
  }
  return nullptr;
}

bool exactFortranRedeclarationIdentity(const SgInitializedName *source,
                                       const SgInitializedName *canonical) {
  std::unordered_set<const SgInitializedName *> visited;
  for (const SgInitializedName *current = source; current != nullptr;
       current = current->get_prev_decl_item()) {
    if (!visited.insert(current).second) {
      fprintf(stderr,
              "REX_AST_INVARIANT[fortran-redeclaration-chain]: object=%p "
              "name=%s has a cyclic initialized-name chain\n",
              static_cast<const void *>(source), source->get_name().str());
      ROSE_ABORT();
    }
    if (current == canonical) {
      return true;
    }
  }
  return false;
}

struct ChildPointerCollector : SimpleReferenceToPointerHandler {
  explicit ChildPointerCollector(std::unordered_set<SgNode *> &children)
      : children(children) {}

  void operator()(SgNode *&child, const SgName &, bool) override {
    if (child != nullptr) {
      children.insert(child);
    }
  }

  std::unordered_set<SgNode *> &children;
};

bool symbolBasisMatchesNode(SgSymbol *symbol, const SgNode *node) {
  return symbol != nullptr && node != nullptr &&
         isSgAliasSymbol(symbol) == nullptr &&
         symbol->get_symbol_basis() == node;
}

SgSymbol *localSymbolForCurrentEntry(SgSymbol *symbol,
                                     SgDeclarationStatement *declaration,
                                     SgScopeStatement *scope) {
  if (declaration != nullptr && declaration->get_scope() == scope &&
      symbolBasisMatchesNode(symbol, declaration)) {
    return symbol;
  }

  return declaration != nullptr ? declaration->get_symbol_from_symbol_table()
                                : nullptr;
}

SgSymbol *localSymbolForCurrentEntry(SgSymbol *symbol,
                                     SgInitializedName *initializedName) {
  if (initializedName != nullptr &&
      symbolBasisMatchesNode(symbol, initializedName)) {
    return symbol;
  }

  return initializedName != nullptr
             ? initializedName->get_symbol_from_symbol_table()
             : nullptr;
}

bool declarationRequiresExactSourceSurfaceOwner(
    SgDeclarationStatement *declaration) {
  return isSgClassDeclaration(declaration) != nullptr ||
         isSgTemplateFunctionDeclaration(declaration) != nullptr ||
         isSgTemplateMemberFunctionDeclaration(declaration) != nullptr ||
         isSgTemplateVariableDeclaration(declaration) != nullptr ||
         isSgTemplateTypedefDeclaration(declaration) != nullptr;
}

SgSymbol *localSymbolForCurrentEntry(SgSymbol *symbol,
                                     SgLabelStatement *labelStatement) {
  if (labelStatement != nullptr &&
      symbolBasisMatchesNode(symbol, labelStatement)) {
    return symbol;
  }

  return labelStatement != nullptr
             ? labelStatement->get_symbol_from_symbol_table()
             : nullptr;
}

SgDeclarationStatement *
canonicalTagDeclaration(SgDeclarationStatement *declaration) {
  if (declaration == nullptr) {
    return nullptr;
  }
  SgDeclarationStatement *first =
      declaration->get_firstNondefiningDeclaration();
  return first != nullptr ? first : declaration;
}

void validateExpressionTypeDefinitionOwner(
    SgExpression *expression, SgType *operandType,
    SgExpression *operandExpression,
    SgDeclarationStatement *typeDefiningDeclaration,
    bool requireExclusiveTypeOperand, const char *context) {
  ROSE_ASSERT(expression != nullptr);
  if (requireExclusiveTypeOperand &&
      ((operandType == nullptr) == (operandExpression == nullptr))) {
    fprintf(stderr,
            "REX_AST_INVARIANT[expression-type-definition-owner]: "
            "context=%s expression=%p/%s does not have exactly one type or "
            "expression operand\n",
            context, static_cast<void *>(expression),
            expression->class_name().c_str());
    ROSE_ABORT();
  }

  if (typeDefiningDeclaration == nullptr) {
    for (SgNode *successor : expression->get_traversalSuccessorContainer()) {
      if (isSgClassDeclaration(successor) != nullptr ||
          isSgEnumDeclaration(successor) != nullptr) {
        fprintf(stderr,
                "REX_AST_INVARIANT[expression-type-definition-owner]: "
                "context=%s expression=%p/%s traverses a tag declaration "
                "without publishing its exact typed field\n",
                context, static_cast<void *>(expression),
                expression->class_name().c_str());
        ROSE_ABORT();
      }
    }
    return;
  }

  if (operandType == nullptr ||
      typeDefiningDeclaration->get_parent() != expression) {
    fprintf(stderr,
            "REX_AST_INVARIANT[expression-type-definition-owner]: "
            "context=%s expression=%p/%s has a defining declaration without "
            "an exact type operand/target and parent edge\n",
            context, static_cast<void *>(expression),
            expression->class_name().c_str());
    ROSE_ABORT();
  }

  SgNamedType *namedType = isSgNamedType(operandType->findBaseType());
  SgDeclarationStatement *typeDeclaration =
      namedType != nullptr ? namedType->get_declaration() : nullptr;
  bool exactDefinition = false;
  bool autonomousDeclaration = true;
  if (SgClassDeclaration *classDeclaration =
          isSgClassDeclaration(typeDefiningDeclaration)) {
    exactDefinition =
        classDeclaration->get_definition() != nullptr &&
        classDeclaration->get_definingDeclaration() == classDeclaration;
    autonomousDeclaration = classDeclaration->get_isAutonomousDeclaration();
  } else if (SgEnumDeclaration *enumDeclaration =
                 isSgEnumDeclaration(typeDefiningDeclaration)) {
    exactDefinition =
        !enumDeclaration->isForward() &&
        enumDeclaration->get_definingDeclaration() == enumDeclaration;
    autonomousDeclaration = enumDeclaration->get_isAutonomousDeclaration();
  }
  if (typeDeclaration == nullptr || !exactDefinition ||
      canonicalTagDeclaration(typeDeclaration) !=
          canonicalTagDeclaration(typeDefiningDeclaration) ||
      autonomousDeclaration) {
    fprintf(stderr,
            "REX_AST_INVARIANT[expression-type-definition-owner]: "
            "context=%s expression=%p/%s owns a declaration that is not the "
            "nonautonomous definition of its exact type\n",
            context, static_cast<void *>(expression),
            expression->class_name().c_str());
    ROSE_ABORT();
  }

  size_t traversalOccurrences = 0;
  for (SgNode *successor : expression->get_traversalSuccessorContainer()) {
    if (successor == typeDefiningDeclaration) {
      ++traversalOccurrences;
    }
  }
  if (traversalOccurrences != 1) {
    fprintf(stderr,
            "REX_AST_INVARIANT[expression-type-definition-owner]: "
            "context=%s expression=%p/%s has %zu traversal edges to its "
            "defining declaration\n",
            context, static_cast<void *>(expression),
            expression->class_name().c_str(), traversalOccurrences);
    ROSE_ABORT();
  }
}

void validateVariableBaseTypeDefinitionOwner(
    SgVariableDeclaration *variableDeclaration) {
  ROSE_ASSERT(variableDeclaration != nullptr);
  SgDeclarationStatement *baseTypeNondefiningDeclaration =
      variableDeclaration->get_baseTypeNondefiningDeclaration();
  SgDeclarationStatement *baseTypeDefiningDeclaration =
      variableDeclaration->get_baseTypeDefiningDeclaration();
  if (baseTypeNondefiningDeclaration != nullptr &&
      baseTypeDefiningDeclaration != nullptr) {
    fprintf(stderr,
            "REX_AST_INVARIANT[variable-base-type-tag-owner]: variable=%p "
            "owns both nondefining=%p and defining=%p tag edges\n",
            static_cast<void *>(variableDeclaration),
            static_cast<void *>(baseTypeNondefiningDeclaration),
            static_cast<void *>(baseTypeDefiningDeclaration));
    ROSE_ABORT();
  }
  SgDeclarationStatement *ownedTag = baseTypeNondefiningDeclaration != nullptr
                                         ? baseTypeNondefiningDeclaration
                                         : baseTypeDefiningDeclaration;
  if (ownedTag == nullptr) {
    for (SgNode *successor :
         variableDeclaration->get_traversalSuccessorContainer()) {
      if (isSgClassDeclaration(successor) != nullptr ||
          isSgEnumDeclaration(successor) != nullptr) {
        fprintf(stderr,
                "REX_AST_INVARIANT[variable-base-type-tag-owner]: "
                "variable=%p traverses a tag declaration without publishing "
                "its exact typed field\n",
                static_cast<void *>(variableDeclaration));
        ROSE_ABORT();
      }
    }
    return;
  }

  bool exactDefinition = false;
  bool exactNondefining = false;
  bool autonomousDeclaration = true;
  if (SgClassDeclaration *classDeclaration = isSgClassDeclaration(ownedTag)) {
    exactDefinition =
        classDeclaration->get_definition() != nullptr &&
        classDeclaration->get_definingDeclaration() == classDeclaration;
    exactNondefining =
        classDeclaration->get_definition() == nullptr &&
        classDeclaration->get_firstNondefiningDeclaration() ==
            classDeclaration &&
        classDeclaration->get_definingDeclaration() != classDeclaration;
    autonomousDeclaration = classDeclaration->get_isAutonomousDeclaration();
  } else if (SgEnumDeclaration *enumDeclaration =
                 isSgEnumDeclaration(ownedTag)) {
    exactDefinition =
        !enumDeclaration->isForward() &&
        enumDeclaration->get_definingDeclaration() == enumDeclaration;
    exactNondefining =
        enumDeclaration->isForward() &&
        enumDeclaration->get_firstNondefiningDeclaration() == enumDeclaration &&
        enumDeclaration->get_definingDeclaration() != enumDeclaration;
    autonomousDeclaration = enumDeclaration->get_isAutonomousDeclaration();
  }
  const bool exactFieldRole = baseTypeNondefiningDeclaration != nullptr
                                  ? exactNondefining
                                  : exactDefinition;
  if (!exactFieldRole || autonomousDeclaration ||
      ownedTag->get_parent() != variableDeclaration ||
      variableDeclaration->get_variables().empty()) {
    fprintf(stderr,
            "REX_AST_INVARIANT[variable-base-type-tag-owner]: variable=%p "
            "does not own one nonautonomous tag with the exact field role\n",
            static_cast<void *>(variableDeclaration));
    ROSE_ABORT();
  }

  for (SgInitializedName *initializedName :
       variableDeclaration->get_variables()) {
    SgType *declaredType =
        initializedName != nullptr ? initializedName->get_type() : nullptr;
    SgNamedType *namedBaseType =
        declaredType != nullptr ? isSgNamedType(declaredType->findBaseType())
                                : nullptr;
    SgDeclarationStatement *typeDeclaration =
        namedBaseType != nullptr ? namedBaseType->get_declaration() : nullptr;
    if (!SageInterface::isExactTagTypeIdentity(declaredType, ownedTag)) {
      fprintf(stderr,
              "REX_AST_INVARIANT[variable-base-type-tag-owner]: "
              "variable=%p initialized-name=%p declared-type=%p/%s "
              "base-type=%p/%s type-declaration=%p/%s canonical=%p "
              "owned-tag=%p/%s canonical=%p does not name the owned tag as "
              "its exact base type\n",
              static_cast<void *>(variableDeclaration),
              static_cast<void *>(initializedName),
              static_cast<void *>(declaredType),
              declaredType != nullptr ? declaredType->class_name().c_str()
                                      : "<null>",
              static_cast<void *>(namedBaseType),
              namedBaseType != nullptr ? namedBaseType->class_name().c_str()
                                       : "<null>",
              static_cast<void *>(typeDeclaration),
              typeDeclaration != nullptr ? typeDeclaration->class_name().c_str()
                                         : "<null>",
              static_cast<void *>(canonicalTagDeclaration(typeDeclaration)),
              static_cast<void *>(ownedTag), ownedTag->class_name().c_str(),
              static_cast<void *>(canonicalTagDeclaration(ownedTag)));
      ROSE_ABORT();
    }
  }

  size_t traversalOccurrences = 0;
  size_t ownedTagPosition = 0;
  bool sawOwnedTag = false;
  const SgNodePtrList successors =
      variableDeclaration->get_traversalSuccessorContainer();
  for (size_t index = 0; index < successors.size(); ++index) {
    SgNode *successor = successors[index];
    if (successor == ownedTag) {
      ++traversalOccurrences;
      ownedTagPosition = index;
      sawOwnedTag = true;
    } else if ((isSgClassDeclaration(successor) != nullptr ||
                isSgEnumDeclaration(successor) != nullptr) &&
               successor->get_parent() == variableDeclaration) {
      fprintf(stderr,
              "REX_AST_INVARIANT[variable-base-type-tag-owner]: variable=%p "
              "traverses undeclared owned tag=%p/%s\n",
              static_cast<void *>(variableDeclaration),
              static_cast<void *>(successor), successor->class_name().c_str());
      ROSE_ABORT();
    }
  }
  bool tagPrecedesDeclarators = sawOwnedTag;
  for (SgInitializedName *initializedName :
       variableDeclaration->get_variables()) {
    const auto position =
        std::find(successors.begin(), successors.end(), initializedName);
    tagPrecedesDeclarators =
        tagPrecedesDeclarators && position != successors.end() &&
        ownedTagPosition < static_cast<size_t>(position - successors.begin());
  }
  if (traversalOccurrences != 1 || !tagPrecedesDeclarators) {
    fprintf(stderr,
            "REX_AST_INVARIANT[variable-base-type-tag-owner]: variable=%p "
            "has %zu traversal edges to its tag or does not traverse it "
            "before every declarator\n",
            static_cast<void *>(variableDeclaration), traversalOccurrences);
    ROSE_ABORT();
  }
}
} // namespace

/*! \file

    This file contains the test run on the AST after construction (option).

*/

/*! \page AstProperties AST Properties (Consistency Tests)

    This documentation describes the properties of the AST. There are a number
of consistency tests of the AST, when all these pass the AST is verified to have
specifi properties.  This documentation lays out these properties of the AST
(e.g. which pointers are always valid, etc.).

*/

void AstTests::validateTraversalSuccessors(SgNode *node) {
  ROSE_ASSERT(node != nullptr);
  const SgNodePtrList successors = node->get_traversalSuccessorContainer();
  const size_t count = node->get_numberOfTraversalSuccessors();
  const size_t nameCount = node->get_traversalSuccessorNamesContainer().size();
  if (successors.size() != count || nameCount != count) {
    fprintf(stderr,
            "REX_AST_INVARIANT[traversal-cardinality]: node=%s has %zu "
            "successors, %zu names and a reported count of %zu\n",
            node->sage_class_name(), successors.size(), nameCount, count);
    ROSE_ABORT();
  }
  std::unordered_set<SgNode *> identities;
  for (SgNode *successor : successors) {
    if (successor == nullptr || !identities.insert(successor).second) {
      fprintf(stderr,
              "REX_AST_INVARIANT[traversal-identity]: node=%s exposes a null "
              "or duplicate successor\n",
              node->sage_class_name());
      ROSE_ABORT();
    }
  }
}

// DQ (10/6/2004): This function should be designed to take a SgNode
// as input so that any part of the AST could be tested!
bool AstTests::isCorrectAst(SgProject *sageProject) {
  TimingPerformance timer(
      "AST check for IR nodes without source position information:");

  TestAstProperties t;
  bool returnValue = t.traverse(sageProject).val;

  if (SgProject::get_verbose() >= DIAGNOSTICS_VERBOSE_LEVEL) {
    printf("\nAST Diagnostics: Fequency of IR nodes missing ending source "
           "postion info: \n");
    bool found = false;
    for (int i = V_SgModifier; i != V_SgNumVariants; i++) {
      if (t.nodeWithoutFileInfoFrequencyCount[i] > 0) {
        found = true;
        printf("     Frequency at %s = %d \n",
               getVariantName(VariantT(i)).c_str(),
               t.nodeWithoutFileInfoFrequencyCount[i]);
      }
    }

    if (found == false) {
      printf("PASSED: All IR nodes have both starting and ending position "
             "information \n");
    }
  }

  return returnValue;
}

void AstTests::runAllTests(SgProject *sageProject) {
  rosePhaseTrace("AstTests.runAllTests.begin");

  if (SgProject::get_verbose() >= DIAGNOSTICS_VERBOSE_LEVEL) {
    printf("At TOP of AstTests::runAllTests() \n");
  }

  if (sageProject->get_frontendErrorCode() != 0) {
    rosePhaseTrace("AstTests.runAllTests.end");
    return;
  }

  TimingPerformance timer("AST Consistency Tests:");

  // DQ (2/17/2013): Added support to skip AST consistancy tests for performance
  // testing. The skipAstConsistancyTests variable is on the SgFile, not the
  // SgProject.
  if (sageProject->get_fileList().empty() == false &&
      sageProject->get_fileList()[0]->get_skipAstConsistancyTests() == true) {
    printf("Note: In AstTests::runAllTests(): command line option used to skip "
           "AST consistancy tests \n");
    rosePhaseTrace("AstTests.runAllTests.end");
    return;
  }

  // DQ (2/23/2014): Adding support for gathering statistics from hash
  // tables.
  if (SgProject::get_verbose() >= DIAGNOSTICS_VERBOSE_LEVEL) {
    for (size_t i = 0; i < sageProject->get_fileList().size(); i++) {
      SgSourceFile *sourceFile = isSgSourceFile(sageProject->get_fileList()[i]);
      if (sourceFile != NULL) {
        SgGlobal *globalScope = sourceFile->get_globalScope();
        ROSE_ASSERT(globalScope != NULL);
        size_t maxCollisions = globalScope->get_symbol_table()->maxCollisions();
        printf("Symbol Table Statistics: sourceFile = %" PRIuPTR
               " maxCollisions = %" PRIuPTR " \n",
               i, maxCollisions);

        float load_factor =
            globalScope->get_symbol_table()->get_table()->load_factor();
        printf("Symbol Table Statistics: sourceFile = %" PRIuPTR
               " load_factor = %f \n",
               i, load_factor);

        float max_load_factor =
            globalScope->get_symbol_table()->get_table()->max_load_factor();
        printf("Symbol Table Statistics: sourceFile = %" PRIuPTR
               " max_load_factor = %f \n",
               i, max_load_factor);
      }
    }
  }

  // test properties of AST
  if (SgProject::get_verbose() >= DIAGNOSTICS_VERBOSE_LEVEL)
    cout << "\nAST initial correctness test ... " << flush;
  if (isCorrectAst(sageProject)) {
    if (SgProject::get_verbose() >= DIAGNOSTICS_VERBOSE_LEVEL)
      cout << "succeeded." << endl;
  } else {
    if (SgProject::get_verbose() >= DIAGNOSTICS_VERBOSE_LEVEL)
      cout << "failed." << endl;
    else
      cout << "AST Consistancy Tests have failed." << endl;
    ROSE_ABORT();
  }

  // Output an extra CR
  if (SgProject::get_verbose() >= DIAGNOSTICS_VERBOSE_LEVEL)
    cout << endl;

  if (SgProject::get_verbose() >= DIAGNOSTICS_VERBOSE_LEVEL)
    cout << "Redundent Statement Test started (tests only single scopes for "
            "redundent statements)."
         << endl;
  {
    TimingPerformance timer(
        "AST check for unique IR nodes in each scope (excludes IR nodes marked "
        "explicitly as shared by AST merge):");

    TestAstForUniqueStatementsInScopes redundentStatementTest;
    redundentStatementTest.traverse(sageProject, preorder);
  }
  if (SgProject::get_verbose() >= DIAGNOSTICS_VERBOSE_LEVEL)
    cout << "Redundent Statement Test finished." << endl;

  {
    TimingPerformance timer("AST check for unique structural nodes:");
    TestAstForUniqueNodesInAST::test(sageProject);
  }

  // DQ (4/27/2005): Test of mangled names
  if (SgProject::get_verbose() >= DIAGNOSTICS_VERBOSE_LEVEL)
    cout << "Mangled Name Test on AST started (tests properties of mangled "
            "names)."
         << endl;
  {
    TimingPerformance timer("AST mangle name test:");

    TestAstForProperlyMangledNames mangledNameTest;
    mangledNameTest.traverse(sageProject, preorder);

    if (SgProject::get_verbose() >= DIAGNOSTICS_VERBOSE_LEVEL) {
      cout << "Mangled Name Test finished: (number of mangled name size = "
           << mangledNameTest.saved_numberOfMangledNames << ") " << endl;
      cout << "Mangled Name Test finished: (max mangled name size       = "
           << mangledNameTest.saved_maxMangledNameSize << ") " << endl;
      cout << "Mangled Name Test finished: (total mangled name size     = "
           << mangledNameTest.saved_totalMangledNameSize << ") " << endl;
    }
  }

  if (SgProject::get_verbose() >= DIAGNOSTICS_VERBOSE_LEVEL)
    cout << "Cycle test started." << endl;

  {
    TimingPerformance timer("AST cycle test:");

    AstCycleTest cycTest;
    cycTest.traverse(sageProject);
  }

  if (SgProject::get_verbose() >= DIAGNOSTICS_VERBOSE_LEVEL)
    cout << "Cycle test finished. No cycle found." << endl;

  if (SgProject::get_verbose() >= DIAGNOSTICS_VERBOSE_LEVEL)
    cout << "Template Test started." << endl;
  {
    TimingPerformance timer("AST template properties test:");

    TestAstTemplateProperties templateTest;
    templateTest.traverse(sageProject, preorder);
  }
  if (SgProject::get_verbose() >= DIAGNOSTICS_VERBOSE_LEVEL)
    cout << "Template Test finished." << endl;

  // DQ (6/24/2005): Test setup of defining and non-defining declaration
  // pointers for each SgDeclarationStatement
  if (SgProject::get_verbose() >= DIAGNOSTICS_VERBOSE_LEVEL)
    cout << "Defining and Non-Defining Declaration  Test started." << endl;
  {
    TimingPerformance timer("AST defining and non-defining declaration test:");

    TestAstForProperlySetDefiningAndNondefiningDeclarations declarationTest;
    declarationTest.traverse(sageProject, preorder);
  }
  if (SgProject::get_verbose() >= DIAGNOSTICS_VERBOSE_LEVEL)
    cout << "Defining and Non-Defining Declaration Test finished." << endl;

  // DQ (6/24/2005): Test setup of defining and non-defining declaration
  // pointers for each SgDeclarationStatement
  if (SgProject::get_verbose() >= DIAGNOSTICS_VERBOSE_LEVEL)
    cout << "Symbol Table Test started." << endl;
  {
    TimingPerformance timer("AST symbol table test:");

    TestAstSymbolTables symbolTableTest;
    symbolTableTest.traverse(sageProject, preorder);
  }
  if (SgProject::get_verbose() >= DIAGNOSTICS_VERBOSE_LEVEL)
    cout << "Symbol Table Test finished." << endl;

  // DQ (6/24/2005): Test setup of defining and non-defining declaration
  // pointers for each SgDeclarationStatement
  if (SgProject::get_verbose() >= DIAGNOSTICS_VERBOSE_LEVEL)
    cout << "Test return value of get_declaration() member functions started."
         << endl;
  {
    TimingPerformance timer("AST test member function access functions:");

    TestAstAccessToDeclarations getDeclarationMemberFunctionTest;
    getDeclarationMemberFunctionTest.traverse(sageProject, preorder);
  }
  if (SgProject::get_verbose() >= DIAGNOSTICS_VERBOSE_LEVEL)
    cout << "Test return value of get_declaration() member functions finished."
         << endl;

  // DQ (2/21/2006): Test the type of all expressions and where ever a get_type
  // function is implemented.
  if (SgProject::get_verbose() >= DIAGNOSTICS_VERBOSE_LEVEL)
    cout << "Test return value of get_type() member functions started." << endl;
  {
    TimingPerformance timer("AST expression type test:");
    TestExpressionTypes expressionTypeTest;
    expressionTypeTest.traverse(sageProject, preorder);
  }
  if (SgProject::get_verbose() >= DIAGNOSTICS_VERBOSE_LEVEL)
    cout << "Test return value of get_type() member functions finished."
         << endl;

  // DQ (5/22/2006): Test the generation of mangled names.
  if (SgProject::get_verbose() >= DIAGNOSTICS_VERBOSE_LEVEL)
    cout << "Test generation of mangled names started." << endl;
  {
    TimingPerformance timer(
        "AST mangled names test (exhaustive test using memory pool):");

    TestMangledNames::test();
  }
  if (SgProject::get_verbose() >= DIAGNOSTICS_VERBOSE_LEVEL)
    cout << "Test generation of mangled names finished." << endl;

  // DQ (6/26/2006): Test the parent pointers of IR nodes in memory pool.
  if (SgProject::get_verbose() >= DIAGNOSTICS_VERBOSE_LEVEL)
    cout << "Test parent pointers of IR nodes in memory pool started." << endl;
  {
    TimingPerformance timer("AST IR node parent pointers test:");

    TestParentPointersInMemoryPool::test();
  }
  if (SgProject::get_verbose() >= DIAGNOSTICS_VERBOSE_LEVEL)
    cout << "Test parent pointers of IR nodes in memory pool finished." << endl;

  // DQ (6/26/2006): Test the parent pointers of IR nodes in memory pool.
  if (SgProject::get_verbose() >= DIAGNOSTICS_VERBOSE_LEVEL)
    cout << "Test parents child pointers of IR nodes in memory pool started."
         << endl;
  {
    TimingPerformance timer("AST IR node child pointers test:");

    TestChildPointersInMemoryPool::test();
  }
  if (SgProject::get_verbose() >= DIAGNOSTICS_VERBOSE_LEVEL)
    cout << "Test parents child pointers of IR nodes in memory pool finished."
         << endl;

  // DQ (6/26/2006): Test expressions for l-value flags
  if (SgProject::get_verbose() >= DIAGNOSTICS_VERBOSE_LEVEL)
    cout << "Test expressions for properly set l-values started." << endl;
  {
    TimingPerformance timer("Test expressions for properly set l-values:");

    TestLValueExpressions lvalueTest;
    lvalueTest.traverse(sageProject, preorder);

    // King84 (7/29/2010): Uncomment this to enable checking of the corrected
    // LValues
  }
  if (SgProject::get_verbose() >= DIAGNOSTICS_VERBOSE_LEVEL)
    cout << "Test expressions for properly set l-values finished." << endl;

  // DQ (11/28/2010): Test to make sure that Fortran is using case insensitive
  // symbol tables and that C/C++ is using case sensitive symbol tables.
  TestForProperLanguageAndSymbolTableCaseSensitivity::test(sageProject);

  {
    TimingPerformance timer("AST check for references to deleted IR nodes:");

    // DQ (9/26/2011): Test for references to deleted IR nodes in the AST.
    TestForReferencesToDeletedNodes::test(sageProject);
  }

  if (SgProject::get_verbose() >= DIAGNOSTICS_VERBOSE_LEVEL)
    cout << "Test typedef types for cycles." << endl;

  {
    TimingPerformance timer("AST check for typedef type cycles:");

    // DQ (9/26/2011): Test for references to deleted IR nodes in the AST.
    TestAstForCyclesInTypedefs::test();
  }

  Rose_STL_Container<SgNode *> typeList;
  if (SgProject::get_verbose() >= DIAGNOSTICS_VERBOSE_LEVEL)
    printf("Calling NodeQuery::generateListOfTypes(SgProject) ... \n");
  {
    TimingPerformance timer("AST generate type list test:");

    typeList = NodeQuery::generateListOfTypes(sageProject);
  }
  if (SgProject::get_verbose() >= DIAGNOSTICS_VERBOSE_LEVEL)
    printf("AST Type Test: typeList.size() = %ld \n", (long)typeList.size());

  if (SgProject::get_verbose() >= DIAGNOSTICS_VERBOSE_LEVEL)
    printf("Type Test Started \n");

  {
    TimingPerformance timer("AST type test:");

    Rose_STL_Container<SgNode *>::iterator i = typeList.begin();
    while (i != typeList.end()) {
      TestAstAccessToDeclarations::test(*i);

      switch ((*i)->variantT()) {
      case V_SgClassType: {
        SgNamedType *namedType = isSgNamedType(*i);
        ROSE_ASSERT(namedType != NULL);
        SgDeclarationStatement *declarationStatement =
            namedType->get_declaration();
        ROSE_ASSERT(declarationStatement != NULL);
        if (declarationStatement->get_parent() == NULL) {
          if (SgProject::get_verbose() >= DIAGNOSTICS_VERBOSE_LEVEL) {
            printf("     Warning: Found a NULL "
                   "declarationStatement->get_parent() value within "
                   "SgClassType ... \n");
          }
        }

        // DQ (10/17/2004): Acturally the defining declaration need not exist
        // (for code to compile and not link). So it is OK to have a NULL
        // value for the definingDeclaration.
        if (declarationStatement->get_firstNondefiningDeclaration() == NULL) {
          if (SgProject::get_verbose() >= DIAGNOSTICS_VERBOSE_LEVEL) {
            printf("     Warning: At %p = %s found unset definingDeclaration "
                   "= %p or firstNondefiningDeclaration = %p \n",
                   namedType, namedType->sage_class_name(),
                   declarationStatement->get_definingDeclaration(),
                   declarationStatement->get_firstNondefiningDeclaration());
          }
        }

        const bool allow_missing_first_nondef =
            isSgTemplateInstantiationDecl(declarationStatement) != NULL ||
            isSgTemplateInstantiationFunctionDecl(declarationStatement) !=
                NULL ||
            isSgTemplateInstantiationMemberFunctionDecl(declarationStatement) !=
                NULL;

        if (declarationStatement->get_firstNondefiningDeclaration() == NULL &&
            !allow_missing_first_nondef) {
          printf("Error: declarationStatement = %p \n", declarationStatement);
          declarationStatement->get_file_info()->display(
              "Error: "
              "declarationStatement->get_firstNondefiningDeclaration() == "
              "NULL");
        }
        ROSE_ASSERT(allow_missing_first_nondef ||
                    declarationStatement->get_firstNondefiningDeclaration() !=
                        NULL);
        // Liao 10/30/2009, We enforce a unique SgClassType node for
        // SgClassDeclaration and its derived classes SgClassType should be
        // associated with the first nondefining class declaration All other
        // relevant declarations (defining and other nondefining declarations)
        // should share this type node
        SgClassType *cls_type = isSgClassType(*i);
        if (cls_type != NULL) {
          ROSE_ASSERT(cls_type->get_declaration() != NULL);
          SgDeclarationStatement *cls_decl =
              isSgDeclarationStatement(cls_type->get_declaration());
          ROSE_ASSERT(cls_decl != NULL);
          if (cls_decl != NULL) {
            if (cls_decl->get_firstNondefiningDeclaration() != NULL) {
              if (cls_decl->get_firstNondefiningDeclaration() != cls_decl) {
                printf("   Warning: cls_decl = %p = %s \n", cls_decl,
                       cls_decl->class_name().c_str());
                printf(
                    "   Warning: cls_decl->get_firstNondefiningDeclaration() "
                    "= %p = %s \n",
                    cls_decl->get_firstNondefiningDeclaration(),
                    cls_decl->get_firstNondefiningDeclaration()
                        ->class_name()
                        .c_str());

                SgClassDeclaration *local_cls_decl =
                    isSgClassDeclaration(cls_decl);
                SgClassDeclaration *local_cls_decl_firstNondefining =
                    isSgClassDeclaration(
                        cls_decl->get_firstNondefiningDeclaration());
                SgClassDeclaration *local_cls_decl_defining =
                    isSgClassDeclaration(cls_decl->get_definingDeclaration());

                printf("local_cls_decl->get_name()                  = %s \n",
                       local_cls_decl->get_name().str());
                printf("local_cls_decl_firstNondefining->get_name() = %s \n",
                       local_cls_decl_firstNondefining->get_name().str());

                local_cls_decl->get_startOfConstruct()->display(
                    "Error in AST Consistancy tests: "
                    "cls_decl->get_firstNondefiningDeclaration() != "
                    "cls_decl");
                local_cls_decl_firstNondefining->get_startOfConstruct()
                    ->display("Error in AST Consistancy tests: "
                              "cls_decl->get_firstNondefiningDeclaration() "
                              "!= cls_decl");

                if (local_cls_decl_defining != NULL) {
                  printf("local_cls_decl_defining->get_name()         = %s \n",
                         local_cls_decl_defining->get_name().str());
                  local_cls_decl_defining->get_startOfConstruct()->display(
                      "Error in AST Consistancy tests: "
                      "cls_decl->get_definingDeclaration() != cls_decl");
                }

                printf("    Warning: found a SgClassType which is NOT "
                       "associated with the first nondefining class "
                       "declaration\n");
                printf("    Warning: SgClassType = %p name = %s associated "
                       "with SgClassDeclaration =%p\n",
                       cls_type, cls_type->get_name().getString().c_str(),
                       cls_decl);
                ROSE_ABORT();
              }
            }
          }
        }

        SgTemplateInstantiationDecl *templateInstantiationDeclaration =
            isSgTemplateInstantiationDecl(declarationStatement);
        if (templateInstantiationDeclaration != NULL) {
          ROSE_ASSERT(!templateInstantiationDeclaration->get_name().is_null());
          ROSE_ASSERT(
              templateInstantiationDeclaration->get_templateDeclaration() !=
              nullptr);
          ROSE_ASSERT(
              !templateInstantiationDeclaration->get_templateName().is_null());
        }
        break;
      }

      default: {
        // Ignore other sorts of types for now!
        break;
      }
      }
      i++;
    }

    // DQ (3/7/2007): This is the end of the timer scope
  }

  if (SgProject::get_verbose() >= DIAGNOSTICS_VERBOSE_LEVEL)
    printf("Type Test finished. \n");

  TestForParentsMatchingASTStructure::test(sageProject);

  // DQ (12/3/2012): Test source position information.
  if (SgProject::get_verbose() >= DIAGNOSTICS_VERBOSE_LEVEL)
    cout << "Test source position information started." << endl;
  {
    TimingPerformance timer("Test source position information:");

    TestForSourcePosition sourcePositionTest;
    sourcePositionTest.traverse(sageProject, preorder);
  }
  if (SgProject::get_verbose() >= DIAGNOSTICS_VERBOSE_LEVEL)
    cout << "Test source position information finished." << endl;

  // DQ (12/11/2012): Test source position information.
  if (SgProject::get_verbose() >= DIAGNOSTICS_VERBOSE_LEVEL)
    cout << "Test restrict keyword information started." << endl;
  {
    TimingPerformance timer("Test restrict keyword:");

    TestForMultipleWaysToSpecifyRestrictKeyword restrictKeywordTest;
    restrictKeywordTest.traverse(sageProject, preorder);
  }
  if (SgProject::get_verbose() >= DIAGNOSTICS_VERBOSE_LEVEL)
    cout << "Test restrict keyword information finished." << endl;

  if (SgProject::get_verbose() >= DIAGNOSTICS_VERBOSE_LEVEL) {
    printf("At BOTTOM of AstTests::runAllTests() \n");
  }
  rosePhaseTrace("AstTests.runAllTests.end");
}

/*! \page AstProperties AST Properties (Consistency Tests)

\section section6 General AST Properties

    All IR nodes are tests and verified to name a number of specific properties.

\subsection subsection6a Source Code Position
    All SgLocatedNode and SgInitializedName IR nodes (and those derieved from
them) are verified to have a valid pointer to a Sg_File_Info object.  Each IR
nodes has a Sg_File_Info object for the start of the language construct and the
end of the language construct.  Note that for scope statements the start and end
positions identify the opening "{" and closing "}" were appropriate.
    - All nodes have a vaild Sg_File_Info object for the front and end of the
subtree.
    - All Sg_File_Info objects are uniques (never shared) \n
      This is important to support classification of IR nodes within
transformations (marked as transformed, shared, or compiler generated).
    - Any nodes not passing this tests are recorded internall (output with
-rose-verbose >= 2)

\subsection subsection6b SgFunctionCallExp Properties

    The SgFunctionCallExp is tested to verify that the get_function() member
function returns only a specific subset of SgExpressions:
       - SgDotExp
       - SgDotStarOp
       - SgArrowExp
       - SgArrowStarOp
       - SgPointerDerefExp
       - SgFunctionRefExp
       - SgMemberFunctionRefExp

    The SgType returned from these SgExpression objects is also restricted to
the following subset of SgType objects:
       - SgTypedefType \n
         This is the case of a function call from a pointer to a function
"(object->*(mFieldAccessorMethod))();"
       - SgMemberFunctionType
       - SgFunctionType \n
         These are the more common cases
       - SgPointerMemberType \n
         This is an unusual case.

    Scopes are stored explicitly on some IR nodes (where the parent pointers in
the AST could provide incorrect evaluation of the scope.  The following IR nodes
are tested to verify that their explicitly represented scope is a valid pointer:
       - SgInitializedName
       - SgClassDeclaration
       - SgTemplateInstantiationDecl
       - SgFunctionDeclaration
       - SgMemberFunctionDeclaration
       - SgTemplateInstantiationFunctionDecl
       - SgTemplateInstantiationMemberFunctionDecl
       - SgTemplateDeclaration
       - SgTypedefDeclaration

*/

TestAstProperties::TestAstProperties() {
  for (int i = V_SgModifier; i != V_SgNumVariants; i++)
    nodeWithoutFileInfoFrequencyCount[i] = 0;
}

TestAstPropertiesSA
TestAstProperties::evaluateSynthesizedAttribute(SgNode *node,
                                                SynthesizedAttributesList l) {
  ROSE_ASSERT(node);
  TestAstPropertiesSA syn;

  for (SynthesizedAttributesList::iterator i = l.begin(); i != l.end(); i++)
    syn.val = syn.val && (*i).val;
  if (!syn.val) {
    cout << "Children are incorrect@" << node->sage_class_name() << endl;
    for (SynthesizedAttributesList::iterator i = l.begin(); i != l.end(); i++)
      cout << (*i).val << ", ";
    cout << endl;
  }

  AstTests::validateTraversalSuccessors(node);

  if (SgImplicitStatement *implicit = isSgImplicitStatement(node))
    implicit->validate_implicit_statement();
  if (auto *initializer = isSgFortranDataInitializer(node)) {
    initializer->validate_initializer();
    auto *owner = isSgInitializedName(initializer->get_parent());
    ROSE_ASSERT(owner != nullptr && owner->get_initializer() == initializer);
    ROSE_ASSERT(initializer->get_type() == owner->get_type());
  }
  if (auto *data = isSgAttributeSpecificationStatement(node))
    data->validate_data_groups();
  if (auto *group = isSgDataStatementGroup(node))
    group->validate_group();
  if (auto *object = isSgDataStatementObject(node))
    object->validate_object();
  if (auto *value = isSgDataStatementValue(node))
    value->validate_value();
  if (SgFortranImplicitRange *range = isSgFortranImplicitRange(node)) {
    range->validate_range();
    SgImplicitStatement *owner = isSgImplicitStatement(range->get_parent());
    ROSE_ASSERT(owner != nullptr);
    ROSE_ASSERT(std::count(owner->get_ranges().begin(),
                           owner->get_ranges().end(), range) == 1);
  }

  if (SgUsingDeclarationStatement *usingDeclaration =
          isSgUsingDeclarationStatement(node)) {
    SgAuxiliaryDeclarationList *auxiliary =
        isSgAuxiliaryDeclarationList(usingDeclaration->get_parent());
    if (auxiliary != nullptr) {
      SgScopeStatement *semanticOwner =
          isSgScopeStatement(auxiliary->get_parent());
      const bool hasDeclarationTarget =
          usingDeclaration->get_declaration() != nullptr;
      const bool hasInitializedNameTarget =
          usingDeclaration->get_initializedName() != nullptr;
      auto isExactSemanticPosition =
          [usingDeclaration](const Sg_File_Info *position) {
            return position != nullptr &&
                   position->get_parent() == usingDeclaration &&
                   position->isCompilerGenerated() &&
                   position->isFrontendSpecific() &&
                   !position->isTransformation() &&
                   !position->isSourcePositionUnavailableInFrontend() &&
                   position->isOutputInCodeGeneration();
          };
      if (semanticOwner == nullptr ||
          semanticOwner->get_auxiliary_declarations() != auxiliary ||
          usingDeclaration->get_scope() != semanticOwner ||
          std::count(auxiliary->get_declarations().begin(),
                     auxiliary->get_declarations().end(),
                     usingDeclaration) != 1 ||
          semanticOwner->statementExistsInScope(usingDeclaration) ||
          hasDeclarationTarget == hasInitializedNameTarget ||
          usingDeclaration->get_definingDeclaration() != usingDeclaration ||
          usingDeclaration->get_firstNondefiningDeclaration() !=
              usingDeclaration ||
          !usingDeclaration->get_source_terminal_name().getString().empty() ||
          usingDeclaration->get_source_name_qualification_present() ||
          usingDeclaration->get_source_name_global_qualification() ||
          !usingDeclaration->get_source_name_qualification_tokens().empty() ||
          usingDeclaration->get_name_qualification_length() != 0 ||
          usingDeclaration->get_global_qualification_required() ||
          usingDeclaration->get_type_elaboration_required() ||
          usingDeclaration->get_translation_unit_source_order().has_value() ||
          !isExactSemanticPosition(usingDeclaration->get_file_info()) ||
          !isExactSemanticPosition(usingDeclaration->get_startOfConstruct()) ||
          !isExactSemanticPosition(usingDeclaration->get_endOfConstruct())) {
        fprintf(stderr,
                "REX_AST_INVARIANT[using-semantic-instantiation]: "
                "declaration=%p parent=%p scope=%p does not have one exact "
                "source-less auxiliary contract\n",
                static_cast<void *>(usingDeclaration),
                static_cast<void *>(usingDeclaration->get_parent()),
                static_cast<void *>(usingDeclaration->get_scope()));
        ROSE_ABORT();
      }
    } else if (usingDeclaration->get_source_terminal_name()
                   .getString()
                   .empty()) {
      SgDeclarationStatement *targetDeclaration =
          usingDeclaration->get_declaration();
      SgInitializedName *targetName = usingDeclaration->get_initializedName();
      Sg_File_Info *fileInfo = usingDeclaration->get_file_info();
      fprintf(stderr,
              "REX_AST_INVARIANT[using-terminal-name]: declaration=%p has no "
              "exact terminal token\n",
              static_cast<void *>(usingDeclaration));
      fprintf(
          stderr,
          "REX_AST_DETAIL[using-terminal-name]: declaration=%p "
          "parent=%p/%s scope=%p target-declaration=%p/%s "
          "target-name=%p/%s file=%s:%d:%d compiler-generated=%d "
          "frontend-specific=%d transformation=%d unavailable=%d "
          "output=%d source-order=%d\n",
          static_cast<void *>(usingDeclaration),
          static_cast<void *>(usingDeclaration->get_parent()),
          usingDeclaration->get_parent() != nullptr
              ? usingDeclaration->get_parent()->class_name().c_str()
              : "<null>",
          static_cast<void *>(usingDeclaration->get_scope()),
          static_cast<void *>(targetDeclaration),
          targetDeclaration != nullptr ? targetDeclaration->class_name().c_str()
                                       : "<null>",
          static_cast<void *>(targetName),
          targetName != nullptr ? targetName->get_name().getString().c_str()
                                : "<null>",
          fileInfo != nullptr ? fileInfo->get_filenameString().c_str()
                              : "<null>",
          fileInfo != nullptr ? fileInfo->get_line() : -1,
          fileInfo != nullptr ? fileInfo->get_col() : -1,
          fileInfo != nullptr && fileInfo->isCompilerGenerated(),
          fileInfo != nullptr && fileInfo->isFrontendSpecific(),
          fileInfo != nullptr && fileInfo->isTransformation(),
          fileInfo != nullptr &&
              fileInfo->isSourcePositionUnavailableInFrontend(),
          fileInfo != nullptr && fileInfo->isOutputInCodeGeneration(),
          usingDeclaration->get_translation_unit_source_order().has_value());
      ROSE_ABORT();
    }
  }

  if (SgCastExp *cast = isSgCastExp(node)) {
    cast->validate_semantic_conversion();
  }

  if (auto *declaration = isSgTemplateClassDeclaration(node)) {
    std::set<SgTemplateClassDeclaration *> patterns{declaration};
    for (auto *pattern = declaration->get_instantiatedFromMemberTemplate();
         pattern != nullptr;
         pattern = pattern->get_instantiatedFromMemberTemplate()) {
      ROSE_ASSERT(patterns.insert(pattern).second);
      ROSE_ASSERT(pattern->get_firstNondefiningDeclaration() == pattern);
    }
  }
  if (SgInitializedName *initializedName = isSgInitializedName(node)) {
    if (auto *type = initializedName->get_cxx_source_type();
        type != nullptr && type->has_source_prototype()) {
      ROSE_ASSERT(type != initializedName->get_type());
      ROSE_ASSERT(type->get_parent() == initializedName);
    }
    if (initializedName->get_deduction_guide_default_available()) {
      auto *parameters =
          isSgFunctionParameterList(initializedName->get_parent());
      auto *guide = parameters != nullptr
                        ? isSgFunctionDeclaration(parameters->get_parent())
                        : nullptr;
      ROSE_ASSERT(guide != nullptr && guide->get_is_deduction_guide());
      ROSE_ASSERT(guide->get_parameterList() == parameters);
      ROSE_ASSERT(initializedName->get_initializer() == nullptr);
    }
    std::set<SgInitializedName *> redeclarationChain;
    for (SgInitializedName *current = initializedName; current != nullptr;
         current = current->get_prev_decl_item()) {
      if (!redeclarationChain.insert(current).second) {
        fprintf(stderr,
                "REX_AST_INVARIANT[variable-redeclaration-chain]: "
                "declaration=%p name=%s contains a cycle at %p\n",
                static_cast<void *>(initializedName),
                initializedName->get_name().str(),
                static_cast<void *>(current));
        ROSE_ABORT();
      }
    }

    const bool isCrayPointer =
        isSgTypeCrayPointer(initializedName->get_fortran_source_type()) !=
        nullptr;
    SgInitializedName *pointee = initializedName->get_cray_pointer_pointee();
    SgExprListExp *crayPointeeShape =
        initializedName->get_fortran_cray_pointer_pointee_shape();
    SgType *semanticPointerBase = initializedName->get_type();
    while (SgModifierType *modifier = isSgModifierType(semanticPointerBase)) {
      semanticPointerBase = modifier->get_base_type();
    }
    SgVariableDeclaration *pointerDeclaration =
        isSgVariableDeclaration(initializedName->get_parent());
    SgArrayType *semanticPointeeArray =
        pointee != nullptr ? ordinaryFortranArrayType(pointee->get_type())
                           : nullptr;
    if (isCrayPointer != (pointee != nullptr) || pointee == initializedName ||
        (pointee != nullptr &&
         isSgTypeCrayPointer(pointee->get_fortran_source_type()) != nullptr) ||
        (isCrayPointer &&
         (pointerDeclaration == nullptr || semanticPointerBase == nullptr ||
          !initializedName->get_fortran_source_type()
               ->get_fortran_source_syntax() ||
          (isSgTypeInt(semanticPointerBase) == nullptr &&
           isSgTypeSignedInt(semanticPointerBase) == nullptr))) ||
        (!isCrayPointer && crayPointeeShape != nullptr) ||
        (crayPointeeShape != nullptr &&
         (semanticPointeeArray == nullptr ||
          semanticPointeeArray->get_rank() <= 0 ||
          crayPointeeShape->get_parent() != initializedName ||
          crayPointeeShape->get_expressions().empty() ||
          crayPointeeShape->get_expressions().size() !=
              static_cast<size_t>(semanticPointeeArray->get_rank()) ||
          pointee->get_fortran_separate_shape_declaration() !=
              pointerDeclaration ||
          !pointee->get_shapeDeferred()))) {
      fprintf(stderr,
              "REX_AST_INVARIANT[cray-pointer-pair]: pointer=%p name=%s "
              "type=%s pointee=%p pointee-type=%s shape=%p\n",
              static_cast<void *>(initializedName),
              initializedName->get_name().str(),
              initializedName->get_type() != nullptr
                  ? initializedName->get_type()->class_name().c_str()
                  : "<null>",
              static_cast<void *>(pointee),
              pointee != nullptr && pointee->get_type() != nullptr
                  ? pointee->get_type()->class_name().c_str()
                  : "<null>",
              static_cast<void *>(crayPointeeShape));
      ROSE_ABORT();
    }

    SgStatement *shapeOwner =
        initializedName->get_fortran_separate_shape_declaration();
    if (shapeOwner != nullptr) {
      SgAttributeSpecificationStatement *attributeOwner =
          isSgAttributeSpecificationStatement(shapeOwner);
      SgCommonBlock *commonOwner = isSgCommonBlock(shapeOwner);
      SgVariableDeclaration *crayPointerOwner =
          isSgVariableDeclaration(shapeOwner);
      if (shapeOwner->get_scope() == nullptr ||
          shapeOwner->get_scope() != initializedName->get_scope() ||
          (attributeOwner == nullptr && commonOwner == nullptr &&
           crayPointerOwner == nullptr)) {
        fprintf(stderr,
                "REX_AST_INVARIANT[fortran-separate-shape-owner]: object=%p "
                "name=%s has malformed owner=%p type=%s\n",
                static_cast<void *>(initializedName),
                initializedName->get_name().str(),
                static_cast<void *>(shapeOwner),
                shapeOwner->class_name().c_str());
        ROSE_ABORT();
      }

      size_t exactReferences = 0;
      if (crayPointerOwner != nullptr) {
        if (crayPointerOwner->get_variables().size() != 1) {
          fprintf(stderr,
                  "REX_AST_INVARIANT[fortran-cray-pointer-shape-owner]: "
                  "object=%p name=%s has malformed Cray POINTER owner=%p\n",
                  static_cast<void *>(initializedName),
                  initializedName->get_name().str(),
                  static_cast<void *>(crayPointerOwner));
          ROSE_ABORT();
        }
        SgInitializedName *pointer = crayPointerOwner->get_variables().front();
        SgExprListExp *shape =
            pointer != nullptr
                ? pointer->get_fortran_cray_pointer_pointee_shape()
                : nullptr;
        SgArrayType *semanticArray =
            ordinaryFortranArrayType(initializedName->get_type());
        if (pointer == nullptr || pointer->get_parent() != crayPointerOwner ||
            isSgTypeCrayPointer(pointer->get_fortran_source_type()) ==
                nullptr ||
            pointer->get_cray_pointer_pointee() != initializedName ||
            shape == nullptr || shape->get_parent() != pointer ||
            semanticArray == nullptr || semanticArray->get_rank() <= 0 ||
            shape->get_expressions().size() !=
                static_cast<size_t>(semanticArray->get_rank())) {
          fprintf(stderr,
                  "REX_AST_INVARIANT[fortran-cray-pointer-shape-owner]: "
                  "object=%p name=%s has malformed Cray POINTER owner=%p\n",
                  static_cast<void *>(initializedName),
                  initializedName->get_name().str(),
                  static_cast<void *>(crayPointerOwner));
          ROSE_ABORT();
        }
        exactReferences = 1;
      } else if (commonOwner != nullptr) {
        for (SgCommonBlockObject *block : commonOwner->get_block_list()) {
          SgExprListExp *objects =
              block != nullptr ? block->get_variable_reference_list() : nullptr;
          if (block == nullptr || block->get_parent() != commonOwner ||
              objects == nullptr || objects->get_parent() != block) {
            fprintf(stderr,
                    "REX_AST_INVARIANT[fortran-common-shape-owner]: "
                    "object=%p name=%s has malformed COMMON owner=%p\n",
                    static_cast<void *>(initializedName),
                    initializedName->get_name().str(),
                    static_cast<void *>(commonOwner));
            ROSE_ABORT();
          }
          for (SgExpression *object : objects->get_expressions()) {
            SgPntrArrRefExp *arrayReference = isSgPntrArrRefExp(object);
            SgVarRefExp *variableReference =
                arrayReference != nullptr
                    ? isSgVarRefExp(arrayReference->get_lhs_operand())
                    : nullptr;
            SgVariableSymbol *symbol = variableReference != nullptr
                                           ? variableReference->get_symbol()
                                           : nullptr;
            if (symbol != nullptr &&
                symbol->get_declaration() == initializedName) {
              ++exactReferences;
            }
          }
        }
      } else {
        SgExprListExp *parameters = attributeOwner->get_parameter_list();
        const auto attributeKind = attributeOwner->get_attribute_kind();
        if ((attributeKind !=
                 SgAttributeSpecificationStatement::e_dimensionStatement &&
             attributeKind !=
                 SgAttributeSpecificationStatement::e_allocatableStatement &&
             attributeKind !=
                 SgAttributeSpecificationStatement::e_pointerStatement) ||
            parameters == nullptr ||
            parameters->get_parent() != attributeOwner) {
          fprintf(stderr,
                  "REX_AST_INVARIANT[fortran-attribute-shape-owner]: "
                  "object=%p name=%s has malformed attribute owner=%p\n",
                  static_cast<void *>(initializedName),
                  initializedName->get_name().str(),
                  static_cast<void *>(attributeOwner));
          ROSE_ABORT();
        }

        for (SgExpression *parameter : parameters->get_expressions()) {
          SgPntrArrRefExp *arrayReference = isSgPntrArrRefExp(parameter);
          SgVarRefExp *variableReference =
              arrayReference != nullptr
                  ? isSgVarRefExp(arrayReference->get_lhs_operand())
                  : nullptr;
          SgVariableSymbol *symbol = variableReference != nullptr
                                         ? variableReference->get_symbol()
                                         : nullptr;
          if (symbol != nullptr &&
              symbol->get_declaration() == initializedName) {
            ++exactReferences;
          }
        }
      }
      if (exactReferences != 1) {
        fprintf(stderr,
                "REX_AST_INVARIANT[fortran-separate-shape-owner]: object=%p "
                "name=%s is referenced %zu times by shape owner=%p\n",
                static_cast<void *>(initializedName),
                initializedName->get_name().str(), exactReferences,
                static_cast<void *>(shapeOwner));
        ROSE_ABORT();
      }
    }

    SgStatement *pointerOwner =
        initializedName->get_fortran_separate_pointer_declaration();
    if (pointerOwner != nullptr) {
      SgAttributeSpecificationStatement *pointerStatement =
          isSgAttributeSpecificationStatement(pointerOwner);
      SgExprListExp *parameters = pointerStatement != nullptr
                                      ? pointerStatement->get_parameter_list()
                                      : nullptr;
      SgType *semanticType = initializedName->get_type();
      SgType *sourceType = initializedName->get_fortran_source_type();
      while (SgModifierType *modifier = isSgModifierType(semanticType)) {
        semanticType = modifier->get_base_type();
      }
      while (SgModifierType *modifier = isSgModifierType(sourceType)) {
        sourceType = modifier->get_base_type();
      }
      if (pointerStatement == nullptr ||
          pointerStatement->get_attribute_kind() !=
              SgAttributeSpecificationStatement::e_pointerStatement ||
          pointerStatement->get_scope() == nullptr ||
          pointerStatement->get_scope() != initializedName->get_scope() ||
          parameters == nullptr ||
          parameters->get_parent() != pointerStatement ||
          parameters->get_expressions().empty() ||
          isSgPointerType(semanticType) == nullptr ||
          isSgPointerType(sourceType) != nullptr) {
        fprintf(stderr,
                "REX_AST_INVARIANT[fortran-pointer-owner]: object=%p "
                "name=%s has malformed POINTER owner=%p\n",
                static_cast<void *>(initializedName),
                initializedName->get_name().str(),
                static_cast<void *>(pointerOwner));
        ROSE_ABORT();
      }
      size_t exactReferences = 0;
      for (SgExpression *item : parameters->get_expressions()) {
        SgPntrArrRefExp *arrayReference = isSgPntrArrRefExp(item);
        SgVarRefExp *variableReference =
            arrayReference != nullptr
                ? isSgVarRefExp(arrayReference->get_lhs_operand())
                : isSgVarRefExp(item);
        SgVariableSymbol *symbol = variableReference != nullptr
                                       ? variableReference->get_symbol()
                                       : nullptr;
        SgInitializedName *declaration =
            symbol != nullptr ? symbol->get_declaration() : nullptr;
        if (item == nullptr || item->get_parent() != parameters ||
            declaration == nullptr ||
            declaration->get_fortran_separate_pointer_declaration() !=
                pointerStatement ||
            (arrayReference != nullptr &&
             declaration->get_fortran_separate_shape_declaration() !=
                 pointerStatement)) {
          fprintf(stderr, "REX_AST_INVARIANT[fortran-pointer-owner]: POINTER "
                          "statement contains a malformed or unowned item\n");
          ROSE_ABORT();
        }
        if (exactFortranRedeclarationIdentity(initializedName, declaration)) {
          ++exactReferences;
        }
      }
      if (exactReferences != 1) {
        fprintf(stderr,
                "REX_AST_INVARIANT[fortran-pointer-owner]: object=%p "
                "name=%s is referenced %zu times by POINTER owner=%p\n",
                static_cast<void *>(initializedName),
                initializedName->get_name().str(), exactReferences,
                static_cast<void *>(pointerOwner));
        ROSE_ABORT();
      }
    }
  }

  // Source positions are a typed AST contract. SgLocatedNode owns source
  // positions directly, while SgPragma is the one legacy support node that
  // explicitly owns them. Other SgSupport nodes are semantic structure and
  // must not be forced into a source classification merely because they are
  // reachable through an exact traversal edge.
  const bool isFileNode = isSgFile(node) || isSgProject(node) ||
                          isSgFileList(node) || isSgDirectory(node);
  const bool requiresFileInfo =
      isSgLocatedNode(node) != nullptr || isSgPragma(node) != nullptr;
  Sg_File_Info *fileInfo = node->get_file_info();
  if (!isFileNode && requiresFileInfo) {
    if (fileInfo == NULL) {
      fprintf(stderr,
              "REX_AST_INVARIANT[source-position-owner]: node=%p type=%s "
              "is a source-position-owning AST category but has no file "
              "information\n",
              static_cast<void *>(node), node->sage_class_name());
      ROSE_ABORT();
    } else {
      // A file info object can only be tested for invalid line number
      // if it is not compiler generated or part of a transformation.
      if (fileInfo->isCompilerGenerated() == false &&
          fileInfo->isTransformation() == false) {
        if (fileInfo->get_line() == 0 && !isSgGlobal(node)) {
          listOfNodesWithoutValidFileInfo.push_back(node);
        }
      }
    }
  } else if (!isFileNode && fileInfo != nullptr) {
    fprintf(stderr,
            "REX_AST_INVARIANT[source-position-owner]: node=%p type=%s is "
            "not a source-position-owning AST category but publishes file "
            "information=%p\n",
            static_cast<void *>(node), node->sage_class_name(),
            static_cast<void *>(fileInfo));
    ROSE_ABORT();
  }

  if (SgDeclarationStatement *declaration = isSgDeclarationStatement(node)) {
    Sg_File_Info *declarationInfo = declaration->get_file_info();
    SgScopeStatement *directOwner =
        isSgScopeStatement(declaration->get_parent());
    if (declarationRequiresExactSourceSurfaceOwner(declaration) &&
        declarationInfo != nullptr && directOwner != nullptr &&
        !declarationInfo->isCompilerGenerated() &&
        !declarationInfo->isTransformation() &&
        declarationInfo->get_line() > 0 &&
        declarationInfo->get_physical_file_id() >= 0) {
      SgSourceFile *sourceFile =
          SageInterface::getEnclosingSourceFile(declaration, true);
      Sg_File_Info *sourceInfo =
          sourceFile != nullptr ? sourceFile->get_file_info() : nullptr;
      const bool belongsToPrimarySource =
          sourceInfo != nullptr && sourceInfo->get_physical_file_id() >= 0 &&
          sourceInfo->get_physical_file_id() ==
              declarationInfo->get_physical_file_id();
      if (belongsToPrimarySource) {
        if (!declarationInfo->isOutputInCodeGeneration()) {
          fprintf(stderr,
                  "REX_AST_INVARIANT[source-surface-ownership]: "
                  "declaration=%p type=%s file=%s line=%d col=%d direct "
                  "scope owner=%p is hidden from code generation\n",
                  static_cast<void *>(declaration),
                  declaration->class_name().c_str(),
                  declarationInfo->get_filenameString().c_str(),
                  declarationInfo->get_line(), declarationInfo->get_col(),
                  static_cast<void *>(directOwner));
          ROSE_ABORT();
        }

        Sg_File_Info *start = declaration->get_startOfConstruct();
        Sg_File_Info *end = declaration->get_endOfConstruct();
        if (start == nullptr || end == nullptr || start->get_line() <= 0 ||
            end->get_line() <= 0 ||
            start->get_physical_file_id() !=
                declarationInfo->get_physical_file_id() ||
            end->get_physical_file_id() !=
                declarationInfo->get_physical_file_id()) {
          fprintf(stderr,
                  "REX_AST_INVARIANT[source-surface-range]: declaration=%p "
                  "type=%s file=%s has no exact primary-source range\n",
                  static_cast<void *>(declaration),
                  declaration->class_name().c_str(),
                  declarationInfo->get_filenameString().c_str());
          ROSE_ABORT();
        }

        const auto key = std::make_tuple(
            declarationInfo->get_physical_file_id(), start->get_line(),
            start->get_col(), end->get_line(), end->get_col(),
            static_cast<int>(declaration->variantT()),
            start->get_source_sequence_number(),
            end->get_source_sequence_number());
        auto inserted = directSourceSurfaceOwners.emplace(key, declaration);
        if (!inserted.second && inserted.first->second != declaration) {
          fprintf(
              stderr,
              "REX_AST_INVARIANT[duplicate-source-surface]: first=%p "
              "first-type=%s first-parent=%p/%s first-scope=%p/%s "
              "first-canonical=%p first-defining=%p first-file-flags="
              "%d/%d/%d duplicate=%p duplicate-type=%s "
              "duplicate-parent=%p/%s duplicate-scope=%p/%s "
              "duplicate-canonical=%p duplicate-defining=%p "
              "duplicate-file-flags=%d/%d/%d file=%s "
              "range=%d:%d-%d:%d\n",
              static_cast<void *>(inserted.first->second),
              inserted.first->second->class_name().c_str(),
              static_cast<void *>(inserted.first->second->get_parent()),
              inserted.first->second->get_parent() != nullptr
                  ? inserted.first->second->get_parent()->class_name().c_str()
                  : "<null>",
              static_cast<void *>(inserted.first->second->get_scope()),
              inserted.first->second->get_scope() != nullptr
                  ? inserted.first->second->get_scope()->class_name().c_str()
                  : "<null>",
              static_cast<void *>(
                  inserted.first->second->get_firstNondefiningDeclaration()),
              static_cast<void *>(
                  inserted.first->second->get_definingDeclaration()),
              inserted.first->second->get_file_info()->isCompilerGenerated()
                  ? 1
                  : 0,
              inserted.first->second->get_file_info()->isFrontendSpecific() ? 1
                                                                            : 0,
              inserted.first->second->get_file_info()
                      ->isOutputInCodeGeneration()
                  ? 1
                  : 0,
              static_cast<void *>(declaration),
              declaration->class_name().c_str(),
              static_cast<void *>(declaration->get_parent()),
              declaration->get_parent() != nullptr
                  ? declaration->get_parent()->class_name().c_str()
                  : "<null>",
              static_cast<void *>(declaration->get_scope()),
              declaration->get_scope() != nullptr
                  ? declaration->get_scope()->class_name().c_str()
                  : "<null>",
              static_cast<void *>(
                  declaration->get_firstNondefiningDeclaration()),
              static_cast<void *>(declaration->get_definingDeclaration()),
              declaration->get_file_info()->isCompilerGenerated() ? 1 : 0,
              declaration->get_file_info()->isFrontendSpecific() ? 1 : 0,
              declaration->get_file_info()->isOutputInCodeGeneration() ? 1 : 0,
              declarationInfo->get_filenameString().c_str(), start->get_line(),
              start->get_col(), end->get_line(), end->get_col());
          ROSE_ABORT();
        }
      }
    }
  }

  // Record the file info objects in all IR nodes traversed (so that we can test
  // for uniqueness)
  listOfNodesFileInfo.push_back(node);

  // Test the use of file info objects to define the start AND end of language
  // constructs (statements for now and expressions later on)
  SgStatement *statement = isSgStatement(node);
  if (statement != NULL) {
    ROSE_ASSERT(statement->get_file_info() != NULL);
    Sg_File_Info *startingSourcePosition = statement->get_startOfConstruct();
    if (startingSourcePosition == NULL) {
      if (SgProject::get_verbose() >= DIAGNOSTICS_VERBOSE_LEVEL)
        printf("AST Diagnostics: Lacking starting postion info on %s \n",
               statement->sage_class_name());
    }
    Sg_File_Info *endingSourcePosition = statement->get_endOfConstruct();
    if (endingSourcePosition == NULL) {
      nodeWithoutFileInfoFrequencyCount[statement->variantT()]++;
    }
  }

  switch (node->variantT()) {
    // DQ (7/11/2004): Test that the function in a function call has a valid
    // function type
  case V_SgFunctionCallExp: {
    SgFunctionCallExp *fc = isSgFunctionCallExp(node);
    ROSE_ASSERT(fc != NULL);
    SgExpression *functionExpression = fc->get_function();
    ROSE_ASSERT(functionExpression != NULL);

    // The type of expression is restricted to a subset of all possible
    // expression (check this)
    SgNode *functionExpressionOwner = fc;
    while (true) {
      if (SgMacroExpansionExp *macro =
              isSgMacroExpansionExp(functionExpression)) {
        SgExpression *expanded = macro->get_expanded_expression_checked();
        if (macro->get_parent() != functionExpressionOwner) {
          fprintf(stderr,
                  "REX_AST_INVARIANT[macro-call-callee]: macro=%p owner=%p "
                  "spelling='%s' expanded=%p expanded-parent=%p\n",
                  static_cast<void *>(macro),
                  static_cast<void *>(functionExpressionOwner),
                  macro->get_spelling().c_str(), static_cast<void *>(expanded),
                  expanded != nullptr
                      ? static_cast<void *>(expanded->get_parent())
                      : nullptr);
          ROSE_ABORT();
        }
        functionExpressionOwner = macro;
        functionExpression = expanded;
        continue;
      }
      if (SgCommaOpExp *comma = isSgCommaOpExp(functionExpression)) {
        SgExpression *rhs = comma->get_rhs_operand();
        if (rhs == nullptr || rhs->get_parent() != comma) {
          fprintf(stderr,
                  "REX_AST_INVARIANT[comma-call-callee]: comma=%p rhs=%p "
                  "rhs-parent=%p\n",
                  static_cast<void *>(comma), static_cast<void *>(rhs),
                  rhs != nullptr ? static_cast<void *>(rhs->get_parent())
                                 : nullptr);
          ROSE_ABORT();
        }
        functionExpressionOwner = comma;
        functionExpression = rhs;
        continue;
      }
      break;
    }

    switch (functionExpression->variantT()) {
      // these are the acceptable cases
    case V_SgDotExp:
    case V_SgDotStarOp:
    case V_SgArrowExp:
    case V_SgArrowStarOp:
    case V_SgPointerDerefExp:
      // these are the acceptable cases
      break;

    case V_SgFunctionRefExp: {
      SgFunctionRefExp *functionRefExp = isSgFunctionRefExp(functionExpression);
      ROSE_ASSERT(functionRefExp != NULL);
      SgFunctionSymbol *functionSymbol = functionRefExp->get_symbol();
      ROSE_ASSERT(functionSymbol != NULL);

      // Make sure that the function has a valid declaration
      ROSE_ASSERT(functionSymbol->get_declaration() != NULL);
      break;
    }

      // DQ (2/23/2013): Added support for SgTemplateFunctionRefExp (required as
      // part of fix for test2013_69.C).
    case V_SgTemplateFunctionRefExp: {
      SgTemplateFunctionRefExp *functionRefExp =
          isSgTemplateFunctionRefExp(functionExpression);
      ROSE_ASSERT(functionRefExp != NULL);
      SgFunctionSymbol *functionSymbol = functionRefExp->get_symbol();
      ROSE_ASSERT(functionSymbol != NULL);

      // Make sure that the function has a valid declaration
      ROSE_ASSERT(functionSymbol->get_declaration() != NULL);
      break;
    }

    case V_SgMemberFunctionRefExp: {
      SgMemberFunctionRefExp *memberFunctionRefExp =
          isSgMemberFunctionRefExp(functionExpression);
      ROSE_ASSERT(memberFunctionRefExp != NULL);
      SgMemberFunctionSymbol *memberFunctionSymbol =
          memberFunctionRefExp->get_symbol();
      ROSE_ASSERT(memberFunctionSymbol != NULL);

      // Make sure that the function has a valid declaration
      ROSE_ASSERT(memberFunctionSymbol->get_declaration() != NULL);
      SgMemberFunctionDeclaration *memberFunctionDeclaration =
          memberFunctionSymbol->get_declaration();

      // DQ (7/29/2005): If there is a non-defining declaration available
      // then make sure that it is used for all function calls.
      if (memberFunctionDeclaration->get_firstNondefiningDeclaration() !=
          NULL) {
        SgMemberFunctionDeclaration *firstNondefiningDeclaration =
            isSgMemberFunctionDeclaration(
                memberFunctionDeclaration->get_firstNondefiningDeclaration());
        ROSE_ASSERT(firstNondefiningDeclaration != nullptr);
        ROSE_ASSERT(firstNondefiningDeclaration->get_definition() == nullptr);

        // If this is not a defining declaration then it should match the
        // firstNondefiningDeclaration
        if (memberFunctionDeclaration->get_definition() == NULL) {
          // Make sure that the first non-defining declaration is recorded
          // int the AST is the one referenced by the memberFunctionRefExp.
          if (memberFunctionDeclaration !=
              memberFunctionDeclaration->get_firstNondefiningDeclaration()) {
            printf("memberFunctionDeclaration = %p \n",
                   memberFunctionDeclaration);
            printf(
                "memberFunctionDeclaration->get_firstNondefiningDeclaration() "
                "= %p \n",
                memberFunctionDeclaration->get_firstNondefiningDeclaration());
            memberFunctionDeclaration->get_file_info()->display(
                "memberFunctionDeclaration");
            memberFunctionDeclaration->get_firstNondefiningDeclaration()
                ->get_file_info()
                ->display("memberFunctionDeclaration->get_"
                          "firstNondefiningDeclaration()");
            memberFunctionDeclaration->get_firstNondefiningDeclaration()
                ->get_file_info()
                ->display("debug");
          }
          ROSE_ASSERT(
              memberFunctionDeclaration ==
              memberFunctionDeclaration->get_firstNondefiningDeclaration());
        }

        // Check the scope of the member function, make sure that it is a
        // SgClassDefinition (could also be a SgTemplateInstantiationDefn).
        if (isSgClassDefinition(memberFunctionDeclaration->get_scope()) ==
            NULL) {
          Sg_File_Info *info = memberFunctionDeclaration->get_file_info();
          fprintf(stderr, "AstConsistency member function scope failure:\n");
          fprintf(stderr, "  decl=%p type=%s name=%s\n",
                  memberFunctionDeclaration,
                  memberFunctionDeclaration->class_name().c_str(),
                  memberFunctionDeclaration->get_name().str());
          fprintf(stderr, "  file=%s line=%d col=%d\n",
                  info != NULL ? info->get_filenameString().c_str() : "<null>",
                  info != NULL ? info->get_line() : -1,
                  info != NULL ? info->get_col() : -1);
          fprintf(
              stderr, "  scope=%p type=%s\n",
              memberFunctionDeclaration->get_scope(),
              memberFunctionDeclaration->get_scope() != NULL
                  ? memberFunctionDeclaration->get_scope()->class_name().c_str()
                  : "<null>");
          fprintf(stderr, "  parent=%p type=%s\n",
                  memberFunctionDeclaration->get_parent(),
                  memberFunctionDeclaration->get_parent() != NULL
                      ? memberFunctionDeclaration->get_parent()
                            ->class_name()
                            .c_str()
                      : "<null>");
          fprintf(stderr, "  firstNondef=%p def=%p associatedClass=%p\n",
                  memberFunctionDeclaration->get_firstNondefiningDeclaration(),
                  memberFunctionDeclaration->get_definingDeclaration(),
                  memberFunctionDeclaration->get_associatedClassDeclaration());
          fflush(stderr);
        }
        ROSE_ASSERT(isSgClassDefinition(
                        memberFunctionDeclaration->get_scope()) != NULL);
      }
      break;
    }

    case V_SgIntVal:
    case V_SgFunctionCallExp:
    case V_SgConstructorInitializer:
    case V_SgCastExp:
    case V_SgConditionalExp:
    case V_SgTemplateParameterVal:
    case V_SgAddressOfOp:
    case V_SgPntrArrRefExp:
    case V_SgVarRefExp:
    case V_SgTemplateMemberFunctionRefExp:
    case V_SgPseudoDestructorRefExp:
    case V_SgLambdaExp:
    case V_SgNonrealRefExp: {
      break;
    }

    default: {
      printf("Error case default in switch (functionExpression = %s) \n",
             functionExpression->class_name().c_str());
      ROSE_ABORT();
    }
    }

    ROSE_ASSERT(fc->get_type() != NULL);
    ROSE_ASSERT(fc->get_function()->get_type() != NULL);

    SgType *callType = fc->get_function()->get_type();
    switch (callType->variantT()) {
    case V_SgTypedefType:
    case V_SgMemberFunctionType:
    case V_SgFunctionType:
    case V_SgPointerMemberType:
    case V_SgPointerType:
    case V_SgTypeInt:
    case V_SgTemplateType:
    case V_SgClassType:
    case V_SgReferenceType:
    case V_SgModifierType:
    case V_SgTypeVoid:
    case V_SgTypeUnknown:
    case V_SgRvalueReferenceType:
    case V_SgNonrealType:
    case V_SgTypeDependent:
    case V_SgAutoType:
    case V_SgDeclType:
    case V_SgTypeBool:
    case V_SgTypeChar:
    case V_SgTypeLongLong: {
      break;
    }

    default: {
      printf("Error case default in switch (callType = %s) \n",
             callType->sage_class_name());
      ROSE_ABORT();
    }
    }

    break;
  }

    // DQ (8/3/2004): Added output of problem IR nodes (output when evaluating
    // the SgFile nodes near the end of the traversal).
  case V_SgFile: {
    // Report the number of IR nodes traversed
    if (SgProject::get_verbose() >= DIAGNOSTICS_VERBOSE_LEVEL)
      printf("\n     Total number of nodes traversed = %ld \n",
             (long)listOfNodesFileInfo.size());

    // Only need to output information if there are a nonzero number of nodes
    // without correct file info
    if (listOfNodesWithoutValidFileInfo.size() > 0) {
      listOfNodesWithoutValidFileInfo.sort();
      listOfNodesWithoutValidFileInfo.unique();
      if (SgProject::get_verbose() >= DIAGNOSTICS_VERBOSE_LEVEL)
        printf("\n     List of %ld IR nodes in the AST with default "
               "information in their file info objects: \n",
               (long)listOfNodesWithoutValidFileInfo.size());

      // DQ 12/17/2007): Commented these out since there are so many for Fortran
      // right now!
      printf("\n     COMMENTED OUT List of %ld IR nodes in the AST with "
             "default information in their file info objects: \n",
             (long)listOfNodesWithoutValidFileInfo.size());
    }

    // Report on number of shared Sg_File_Info objects within the AST (should be
    // none)
    listOfNodesFileInfo.sort();
    int sizeBeforeRemovingRedundantEntries = listOfNodesFileInfo.size();
    listOfNodesFileInfo.unique();
    int sizeAfterRemovingRedundantEntries = listOfNodesFileInfo.size();
    int redundantEntries =
        sizeBeforeRemovingRedundantEntries - sizeAfterRemovingRedundantEntries;
    if (redundantEntries != 0) {
      if (SgProject::get_verbose() >= DIAGNOSTICS_VERBOSE_LEVEL)
        printf("There are %d IR nodes that share a file info object \n",
               redundantEntries);
    }
    break;
  }

  case V_SgInitializedName: {
    // DQ (11/28/2004): Verify that newly added explicit scope data members are
    // initialized properly (should be a valid pointer)
    SgInitializedName *initializedName = isSgInitializedName(node);
    ROSE_ASSERT(initializedName != NULL);
    // Note that SgInitializedNames in function paremter lists can be NULL if
    // the function is not a defining declaration (or if no defining declaration
    // exists). Later work will set the scope to the existing defining
    // declaration's scope (later).
    SgFunctionParameterList *parentParameterList =
        isSgFunctionParameterList(initializedName->get_parent());
    if (parentParameterList != NULL) {
      SgFunctionDeclaration *functionDeclaration =
          isSgFunctionDeclaration(parentParameterList->get_parent());

      if (functionDeclaration != NULL) {
        bool isFunctionDefinition =
            (functionDeclaration->get_definition() != NULL);

        // Only enforce the scope to be valid if this is the defining function
        // declaration (later for any function prototype of a defining function
        // declaration!)
        if (isFunctionDefinition == true) {
          ROSE_ASSERT(initializedName->get_scope() != NULL);
        }

        // Cong (10/20/2010): Here we test if the parent of this initialized
        // name does have it as an argument. This is to detect if several
        // function parameter lists own the same initialized name.

        const SgInitializedNamePtrList &initNameList =
            parentParameterList->get_args();
        SgInitializedNamePtrList::const_iterator result = std::find(
            initNameList.begin(), initNameList.end(), initializedName);
        ROSE_ASSERT(result != initNameList.end());
      } else if (SgRequiresExpr *requiresExpression =
                     isSgRequiresExpr(parentParameterList->get_parent())) {
        SgFunctionParameterScope *parameterScope =
            requiresExpression->get_local_parameter_scope();
        SgSymbol *parameterSymbol =
            parameterScope != NULL
                ? parameterScope->find_symbol_from_declaration(initializedName)
                : NULL;
        const bool written =
            requiresExpression->get_local_parameter_list_syntax() ==
            parentParameterList;
        if ((!written && requiresExpression->get_local_parameter_list() !=
                             parentParameterList) ||
            parameterScope == NULL ||
            parameterScope->get_parent() != requiresExpression ||
            initializedName->get_scope() != parameterScope ||
            (written
                 ? parameterSymbol != NULL
                 : (parameterSymbol == NULL ||
                    parameterSymbol->get_symbol_basis() != initializedName ||
                    parameterSymbol->get_parent() !=
                        parameterScope->get_symbol_table()))) {
          fprintf(stderr,
                  "REX_AST_INVARIANT[requires-local-parameter-owner]: "
                  "parameter=%p/%s list=%p requires-expression=%p does not "
                  "have one exact parameter-list, scope, and symbol owner\n",
                  static_cast<void *>(initializedName),
                  initializedName->get_name().getString().c_str(),
                  static_cast<void *>(parentParameterList),
                  static_cast<void *>(requiresExpression));
          ROSE_ABORT();
        }
      } else if (isSgTemplateDeclaration(parentParameterList->get_parent())) {
        // DQ (12/6/2011): Now that we have the template declarations in the
        // AST, this could be a SgTemplateDeclaration.
        printf("WARNING: There are tests missing for the case of a "
               "parentParameterList->get_parent() that is a "
               "SgTemplateDeclaration \n");
      } else {
        ROSE_ASSERT(NULL);
      }
    } else {
      // DQ (2/12/2012): Implement better error diagnostics.
      if (initializedName->get_scope() == NULL) {
        printf("Error: initializedName->get_scope() == NULL \n");
        ROSE_ASSERT(initializedName->get_parent() != NULL);
        // DQ (2/12/2012): Refactoring disagnostic support for detecting where
        // we are when something fails.
        SageInterface::whereAmI(initializedName);
      }
      ROSE_ASSERT(initializedName->get_scope() != NULL);
    }
    break;
  }

  case V_SgClassDeclaration:
  case V_SgDerivedTypeStatement:
  case V_SgTemplateInstantiationDecl: {
    // DQ (11/28/2004): Verify that newly added explicit scope data members are
    // initialized properly (should be a valid pointer)
    SgClassDeclaration *classDeclaration = isSgClassDeclaration(node);
    ROSE_ASSERT(classDeclaration != NULL);
    if (classDeclaration->get_scope() == NULL) {
      printf(
          "Error: SgClassDeclaration with explicit scope unset at %p = %s \n",
          classDeclaration, classDeclaration->sage_class_name());
    }
    ROSE_ASSERT(classDeclaration->get_scope() != NULL);
    break;
  }

  case V_SgFunctionDeclaration:
  case V_SgMemberFunctionDeclaration:
  case V_SgTemplateInstantiationFunctionDecl:
  case V_SgTemplateInstantiationMemberFunctionDecl: {
    // DQ (11/28/2004): Verify that newly added explicit scope data members are
    // initialized properly (should be a valid pointer)
    SgFunctionDeclaration *functionDeclaration = isSgFunctionDeclaration(node);
    ROSE_ASSERT(functionDeclaration != NULL);
    if (functionDeclaration->get_scope() == NULL) {
      printf("Error: SgFunctionDeclaration with explicit scope unset at %p = "
             "%s \n",
             functionDeclaration, functionDeclaration->sage_class_name());
    }
    ROSE_ASSERT(functionDeclaration->get_scope() != NULL);
    if (SgFunctionDeclaration *pattern =
            functionDeclaration->get_templateInstantiationPattern()) {
      ROSE_ASSERT(pattern != functionDeclaration);
      ROSE_ASSERT(isSgTemplateInstantiationFunctionDecl(functionDeclaration) ==
                  NULL);
      ROSE_ASSERT(isSgTemplateInstantiationMemberFunctionDecl(
                      functionDeclaration) == NULL);
      ROSE_ASSERT(pattern->get_templateInstantiationPattern() == NULL);
    }
    break;
  }

  case V_SgTemplateDeclaration: {
    // DQ (11/28/2004): Verify that newly added explicit scope data members are
    // initialized properly (should be a valid pointer)
    SgTemplateDeclaration *templateDeclaration = isSgTemplateDeclaration(node);
    ROSE_ASSERT(templateDeclaration != NULL);
    if (templateDeclaration->get_scope() == NULL) {
      printf("Error: SgTemplateDeclaration with explicit scope unset at %p = "
             "%s \n",
             templateDeclaration, templateDeclaration->sage_class_name());
    }
    ROSE_ASSERT(templateDeclaration->get_scope() != NULL);
    break;
  }

  case V_SgTypedefDeclaration: {
    // DQ (11/28/2004): Verify that newly added explicit scope data members are
    // initialized properly (should be a valid pointer)
    SgTypedefDeclaration *typedefDeclaration = isSgTypedefDeclaration(node);
    ROSE_ASSERT(typedefDeclaration != NULL);
    if (typedefDeclaration->get_scope() == NULL) {
      printf(
          "Error: SgTypedefDeclaration with explicit scope unset at %p = %s \n",
          typedefDeclaration, typedefDeclaration->sage_class_name());
    }
    ROSE_ASSERT(typedefDeclaration->get_scope() != NULL);
    break;
  }

  case V_SgFunctionDefinition: {
    // DQ (8/9/2005): Test for existence of body in function definition
    SgFunctionDefinition *functionDefinition = isSgFunctionDefinition(node);
    if (functionDefinition->get_body() == NULL) {
      printf("AST Consistency Test: This function definition violates the rule "
             "that every function definition has a function body (even if it "
             "is empty) \n");
      functionDefinition->get_file_info()->display("debug");
    }
    ROSE_ASSERT(functionDefinition->get_body() != NULL);

    // DQ (3/16/2006): Verify that this is true, it should always be true (for
    // Yarden)
    if (functionDefinition->get_body()->get_parent() != functionDefinition) {
      printf("Error: functionDefinition = %p functionDefinition->get_body() = "
             "%p functionDefinition->get_body()->get_parent() = %p \n",
             functionDefinition, functionDefinition->get_body(),
             functionDefinition->get_body()->get_parent());
    }
    ROSE_ASSERT(functionDefinition->get_body()->get_parent() ==
                functionDefinition);
    break;
  }

  default:
    break;
  }

  return syn;
}

void TestAstTemplateProperties::visit(SgNode *astNode) {
  switch (astNode->variantT()) {
  case V_SgTemplateInstantiationDecl: {
    SgTemplateInstantiationDecl *s = isSgTemplateInstantiationDecl(astNode);
    ROSE_ASSERT(s != NULL);

    SgDeclarationStatement *templateDeclaration = s->get_templateDeclaration();
    ROSE_ASSERT(templateDeclaration != nullptr);
    if (templateDeclaration->get_definingDeclaration()) {
      templateDeclaration = templateDeclaration->get_definingDeclaration();
    }

    const bool isAuxiliarySemanticDeclaration =
        isSgAuxiliaryDeclarationList(templateDeclaration->get_parent()) != NULL;
    if (!isAuxiliarySemanticDeclaration) {
      // Source-owned template declarations must retain their frontend source
      // identity. Only declarations in the explicit auxiliary ownership
      // channel may be compiler-generated semantic shells.
      if (templateDeclaration->get_file_info()->isCompilerGenerated() == true) {
        fprintf(
            stderr,
            "REX_AST_INVARIANT[template-source-ownership]: template=%p "
            "type=%s name=%s parent=%p parent_type=%s parent_name=%s "
            "first=%p defining=%p "
            "line=%d output=%d is compiler-generated without auxiliary "
            "ownership\n",
            static_cast<void *>(templateDeclaration),
            templateDeclaration->class_name().c_str(),
            SageInterface::get_name(templateDeclaration).c_str(),
            static_cast<void *>(templateDeclaration->get_parent()),
            templateDeclaration->get_parent() != NULL
                ? templateDeclaration->get_parent()->class_name().c_str()
                : "<null>",
            isSgClassDefinition(templateDeclaration->get_parent()) != NULL
                ? isSgClassDefinition(templateDeclaration->get_parent())
                      ->get_declaration()
                      ->get_name()
                      .getString()
                      .c_str()
                : "<none>",
            static_cast<void *>(
                templateDeclaration->get_firstNondefiningDeclaration()),
            static_cast<void *>(templateDeclaration->get_definingDeclaration()),
            templateDeclaration->get_file_info()->get_line(),
            templateDeclaration->get_file_info()->isOutputInCodeGeneration());
        ROSE_ABORT();
      }
    }
    break;
  }

  case V_SgTemplateInstantiationFunctionDecl: {
    break;
  }

  case V_SgTemplateInstantiationMemberFunctionDecl: {
    SgTemplateInstantiationMemberFunctionDecl *s =
        isSgTemplateInstantiationMemberFunctionDecl(astNode);
    // The specialization bit is semantic: it is shared by a written explicit
    // specialization and by the auxiliary canonical declaration that backs
    // its redeclaration chain.  Source provenance must therefore be checked
    // against the declaration's typed owner, not inferred from that bit.
    if (s->isSpecialization() == true || s->isPartialSpecialization() == true) {
      Sg_File_Info *fileInfo = s->get_file_info();
      const bool auxiliaryOwned =
          isSgAuxiliaryDeclarationList(s->get_parent()) != nullptr;
      const bool exactAuxiliaryProvenance =
          fileInfo != nullptr && fileInfo->isCompilerGenerated() &&
          fileInfo->isFrontendSpecific() && !fileInfo->isTransformation() &&
          fileInfo->isOutputInCodeGeneration();
      const bool exactSourceProvenance =
          fileInfo != nullptr && !fileInfo->isCompilerGenerated() &&
          !fileInfo->isFrontendSpecific() && !fileInfo->isTransformation() &&
          fileInfo->isOutputInCodeGeneration();
      if ((auxiliaryOwned && !exactAuxiliaryProvenance) ||
          (!auxiliaryOwned && !exactSourceProvenance)) {
        fprintf(stderr,
                "REX_AST_INVARIANT[member-specialization-ownership]: "
                "declaration=%p name=%s parent=%p/%s auxiliary=%d "
                "compiler-generated=%d frontend=%d transformation=%d "
                "output=%d has contradictory typed ownership and source "
                "provenance\n",
                static_cast<void *>(s), s->get_name().str(),
                static_cast<void *>(s->get_parent()),
                s->get_parent() != nullptr
                    ? s->get_parent()->class_name().c_str()
                    : "<null>",
                auxiliaryOwned ? 1 : 0,
                fileInfo != nullptr && fileInfo->isCompilerGenerated(),
                fileInfo != nullptr && fileInfo->isFrontendSpecific(),
                fileInfo != nullptr && fileInfo->isTransformation(),
                fileInfo != nullptr && fileInfo->isOutputInCodeGeneration());
        ROSE_ABORT();
      }
    }

    break;
  }

  case V_SgClassDefinition:
  case V_SgTemplateInstantiationDefn: {
    // DQ (6/22/2005): templated class declarations can be hidden in base class
    // specifications
    SgClassDefinition *classDefinition = isSgClassDefinition(astNode);
    ROSE_ASSERT(classDefinition != NULL);

    SgBaseClassPtrList::iterator i =
        classDefinition->get_inheritances().begin();
    for (; i != classDefinition->get_inheritances().end(); ++i) {
      // Check the parent pointer to make sure it is properly set
      ROSE_ASSERT((*i)->get_parent() != NULL);
      ROSE_ASSERT((*i)->get_parent() == classDefinition);
      const bool sourceQualifierOwnsTerminal =
          (*i)->get_source_type_qualification_owns_terminal_name();
      const bool sourceQualifierOwnsArguments =
          (*i)->get_source_type_qualification_owns_template_arguments();
      ROSE_ASSERT(!sourceQualifierOwnsArguments || sourceQualifierOwnsTerminal);
      ROSE_ASSERT(!sourceQualifierOwnsTerminal ||
                  ((*i)->get_source_type_qualification_present() &&
                   !(*i)->get_source_type_qualification_tokens().empty()));

      if (isSgExpBaseClass(*i)) {
        SgDeclType *sourceType = isSgDeclType((*i)->get_source_type());
        ROSE_ASSERT(sourceType != nullptr);
        ROSE_ASSERT(sourceType->get_base_expression() != nullptr);
        ROSE_ASSERT((*i)->get_base_class() == nullptr);
        continue;
      }
      if (SgNonrealBaseClass *nonrealBaseClass = isSgNonrealBaseClass(*i)) {
        ROSE_ASSERT(nonrealBaseClass->get_base_class_nonreal() != NULL);
        continue;
      }

      SgClassDeclaration *baseClassDeclaration = (*i)->get_base_class();
      ROSE_ASSERT(baseClassDeclaration != NULL);
      SgTemplateInstantiationDecl *templateInstantiation =
          isSgTemplateInstantiationDecl(baseClassDeclaration);
      if (templateInstantiation != NULL) {
        ROSE_ASSERT(!templateInstantiation->get_name().is_null());
        ROSE_ASSERT(templateInstantiation->get_templateDeclaration() !=
                    nullptr);
        ROSE_ASSERT(!templateInstantiation->get_templateName().is_null());
      }
    }
    break;
  }

  default: {
    SgVariableDeclaration *variableDeclaration =
        isSgVariableDeclaration(astNode);
    if (variableDeclaration != NULL) {
      SgScopeStatement *scope = variableDeclaration->get_scope();
      SgClassDefinition *classDefinition = isSgClassDefinition(scope);
      if (classDefinition != NULL &&
          isSgTemplateClassDefinition(classDefinition) == NULL) {
        SgClassDeclaration *classDeclaration =
            classDefinition->get_declaration();
        ROSE_ASSERT(classDeclaration != NULL);
        if (isSgTemplateInstantiationDecl(classDeclaration) != NULL) {
          if (isSgTemplateInstantiationDefn(classDefinition) == NULL) {
            printf("Warning: classDefinition = %p = %s \n", classDefinition,
                   classDefinition->class_name().c_str());
          }
          ROSE_ASSERT(isSgClassDefinition(classDefinition) != NULL);
        }
      }
    }

    // DQ (5/9/2005): Make sure that all non-templated IR nodes are marked as
    // SgDeclarationStatement::e_no_specialization.
    SgClassDeclaration *classDeclaration = isSgClassDeclaration(astNode);
    if (classDeclaration != NULL) {
      if (isSgTemplateInstantiationDecl(classDeclaration) == NULL &&
          isSgTemplateClassDeclaration(classDeclaration) == NULL) {
        const bool explicit_member_class_specialization =
            classDeclaration->get_specialization() ==
                SgClassDeclaration::e_specialization &&
            isSgTemplateInstantiationDefn(classDeclaration->get_scope()) !=
                NULL;
        // this is NOT a data member of the templated class (or nested class of
        // a templated class)
        if (!explicit_member_class_specialization &&
            classDeclaration->get_specialization() !=
                SgClassDeclaration::e_no_specialization) {
          printf("AST ConsistancyTest: classDeclaration = %p = %s "
                 "classDeclaration->get_specialization() = %d != "
                 "SgClassDeclaration::e_no_specialization \n",
                 classDeclaration, classDeclaration->get_name().str(),
                 classDeclaration->get_specialization());
          printf("     classDeclaration at file %s line = %d \n",
                 classDeclaration->get_file_info()->get_raw_filename().c_str(),
                 classDeclaration->get_file_info()->get_raw_line());
        }
        ROSE_ASSERT(explicit_member_class_specialization ||
                    classDeclaration->get_specialization() ==
                        SgClassDeclaration::e_no_specialization);
      }
    }

    // DQ (5/9/2005): Make sure that all non-templated IR nodes are marked as
    // SgDeclarationStatement::e_no_specialization.
    SgFunctionDeclaration *functionDeclaration =
        isSgFunctionDeclaration(astNode);
    if (functionDeclaration != NULL) {
      if (isSgTemplateInstantiationFunctionDecl(functionDeclaration) == NULL &&
          isSgTemplateInstantiationMemberFunctionDecl(functionDeclaration) ==
              NULL) {
        // this is NOT a data member of the templated class (or nested class of
        // a templated class)
        ROSE_ASSERT(functionDeclaration->get_specialization() ==
                    SgFunctionDeclaration::e_no_specialization);
      }
    }
  }
  }
}

void TestAstForProperlyMangledNames::visit(SgNode *node) {
  SgClassDeclaration *classDeclaration = isSgClassDeclaration(node);
  SgFunctionDeclaration *functionDeclaration = isSgFunctionDeclaration(node);
  if (classDeclaration == nullptr && functionDeclaration == nullptr)
    return;

  string mangledName;
  if (classDeclaration != NULL) {
    mangledName = classDeclaration->get_mangled_name().getString();

    unsigned long mangledNameSize = mangledName.size();
    saved_totalMangledNameSize += mangledNameSize;
    saved_numberOfMangledNames++;
    if (saved_maxMangledNameSize < mangledNameSize) {
      saved_maxMangledNameSize = mangledNameSize;
      if (SgProject::get_verbose() >= DIAGNOSTICS_VERBOSE_LEVEL) {
        printf("saved_maxMangledNameSize = %lu average size = %lu \n",
               saved_maxMangledNameSize,
               saved_totalMangledNameSize / saved_numberOfMangledNames);
      }
    }

    if (mangledName.find('<') != string::npos) {
      string name = classDeclaration->get_name().str();
      printf("In TestAstForProperlyMangledNames::visit(SgNode*): \n           "
             "unmangled name = %s \n check mangled class name = %s \n",
             name.c_str(), mangledName.c_str());
    }
    ROSE_ASSERT(mangledName.find('<') == string::npos);
    if (mangledName.find('>') != string::npos) {
      string name = classDeclaration->get_name().str();
      printf("In TestAstForProperlyMangledNames::visit(SgNode*): \n           "
             "unmangled name = %s \n check mangled class name = %s \n",
             name.c_str(), mangledName.c_str());
    }
    ROSE_ASSERT(mangledName.find('>') == string::npos);
  }

  if (functionDeclaration != NULL) {
    mangledName = functionDeclaration->get_mangled_name().str();
    ROSE_ASSERT(mangledName.find('<') == string::npos);
    ROSE_ASSERT(mangledName.find('>') == string::npos);
  }
  // Make sure that there is no template specific syntax included in the mangled
  // name
  ROSE_ASSERT(mangledName.find('`') == string::npos);
  ROSE_ASSERT(mangledName.find('~') == string::npos);
  ROSE_ASSERT(mangledName.find('!') == string::npos);

  if (mangledName.find('@') != string::npos) {
    printf("Error: mangled name contains '@' node = %p = %s --- "
           "mangledName = %s \n",
           node, node->class_name().c_str(), mangledName.c_str());
  }
  ROSE_ASSERT(mangledName.find('@') == string::npos);

  ROSE_ASSERT(mangledName.find('#') == string::npos);

  // DQ (8/13/2005): this is an error in KULL (use of siloswigtypecheck.cc)
  // Commented out this tests so that I can defer it to later!
  if (mangledName.find('(') != string::npos) {
    printf("AST Consistency Test: found \"(\" in mangledName = %s \n",
           mangledName.c_str());

    SgDeclarationStatement *declaration = isSgDeclarationStatement(node);
    ROSE_ASSERT(declaration != NULL);
    declaration->get_file_info()->display("debug");
  }
  // DQ (8/13/2005): this is an error in KULL (use of siloswigtypecheck.cc)
  // Commented out this tests so that I can defer it to later!
  if (mangledName.find(')') != string::npos) {
    printf("AST Consistency Test: found \")\" in mangledName = %s \n",
           mangledName.c_str());

    SgDeclarationStatement *declaration = isSgDeclarationStatement(node);
    ROSE_ASSERT(declaration != NULL);
    declaration->get_file_info()->display("debug");
  }
  // DQ (8/9/2005): this is an error in KULL (use of template-heavy
  // headers)
  if (mangledName.find('-') != string::npos) {
    printf("AST Consistency Test: found \"-\" in mangledName = %s \n",
           mangledName.c_str());

    SgDeclarationStatement *declaration = isSgDeclarationStatement(node);
    ROSE_ASSERT(declaration != NULL);
    declaration->get_file_info()->display("debug");
  }
  ROSE_ASSERT(mangledName.find('-') == string::npos);

  ROSE_ASSERT(mangledName.find('+') == string::npos);
  ROSE_ASSERT(mangledName.find('=') == string::npos);
  ROSE_ASSERT(mangledName.find('{') == string::npos);
  ROSE_ASSERT(mangledName.find('}') == string::npos);
  ROSE_ASSERT(mangledName.find('[') == string::npos);
  ROSE_ASSERT(mangledName.find(']') == string::npos);
  ROSE_ASSERT(mangledName.find('|') == string::npos);
  ROSE_ASSERT(mangledName.find('\\') == string::npos);

  // DQ (2/22/2007): Added error checking to report problems found in mangled
  if (mangledName.find("::") != string::npos) {
    printf("AST Consistency Test: found \"::\" in mangledName = %s \n",
           mangledName.c_str());

    SgDeclarationStatement *declaration = isSgDeclarationStatement(node);
    ROSE_ASSERT(declaration != NULL);
    declaration->get_file_info()->display("debug");
  }
  ROSE_ASSERT(mangledName.find("::") == string::npos);

  ROSE_ASSERT(mangledName.find(':') == string::npos);
  ROSE_ASSERT(mangledName.find(';') == string::npos);
  ROSE_ASSERT(mangledName.find('\"') == string::npos);
  ROSE_ASSERT(mangledName.find('?') == string::npos);
  ROSE_ASSERT(mangledName.find('.') == string::npos);
  ROSE_ASSERT(mangledName.find('/') == string::npos);
  ROSE_ASSERT(mangledName.find(',') == string::npos);

  ROSE_ASSERT(mangledName.find('<') == string::npos);
  ROSE_ASSERT(mangledName.find('>') == string::npos);
}

TestAstForProperlyMangledNames::TestAstForProperlyMangledNames()
    : saved_maxMangledNameSize(0), saved_totalMangledNameSize(0),
      saved_numberOfMangledNames(0) {}

/*! \page AstProperties AST Properties (Consistency Tests)

\section section2 Unique Statements in Scope

     This test verifies each statement in a scope is unique. This catches
rewrite and general transformation errors that might insert a statement twice or
relocate in by forget to delete it.  Since it only works within a single scope
it is not very robust.

*/

void TestAstForUniqueStatementsInScopes::visit(SgNode *node) {
  SgScopeStatement *scope = isSgScopeStatement(node);
  if (scope != NULL) {
    // Generate a list of statements in the scope (even if they are really
    // declaration statements)
    SgStatementPtrList statementList;
    switch (scope->variantT()) {
    case V_SgIfStmt: {
      SgIfStmt *ifStatement = isSgIfStmt(scope);
      ROSE_ASSERT(ifStatement != NULL);
      ROSE_ASSERT(
          ifStatement->get_if_statement_kind() >= SgIfStmt::e_if_ordinary &&
          ifStatement->get_if_statement_kind() <= SgIfStmt::e_if_not_consteval);
      ROSE_ASSERT(!ifStatement->get_if_constexpr_value_known() ||
                  ifStatement->get_is_if_constexpr_statement());
      ROSE_ASSERT(!ifStatement->get_if_constexpr_value() ||
                  ifStatement->get_if_constexpr_value_known());
      ROSE_ASSERT((ifStatement->get_conditional() == nullptr) ==
                  ifStatement->get_is_if_consteval_statement());
      if (ifStatement->get_is_if_consteval_statement()) {
        ROSE_ASSERT(isSgBasicBlock(ifStatement->get_true_body()) != nullptr);
        ROSE_ASSERT(ifStatement->get_false_body() == nullptr ||
                    isSgBasicBlock(ifStatement->get_false_body()) != nullptr);
      }
      statementList = isSgBasicBlock(ifStatement->get_true_body())
                          ? isSgBasicBlock(ifStatement->get_true_body())
                                ->generateStatementList()
                          : SgStatementPtrList(1, ifStatement->get_true_body());
      SgStatementPtrList falseStatementList =
          isSgBasicBlock(ifStatement->get_false_body())
              ? isSgBasicBlock(ifStatement->get_false_body())
                    ->generateStatementList()
          : ifStatement->get_false_body()
              ? SgStatementPtrList(1, ifStatement->get_false_body())
              : SgStatementPtrList();

      statementList.insert(statementList.end(), falseStatementList.begin(),
                           falseStatementList.end());

      break;
    }
    default:
      statementList = scope->generateStatementList();
    }
    // MK (8/3/05) : Rewrote this test to ignore statements which are marked as
    // shared. These may actually exist multiple times in a scope

    set<SgStatement *> seenStatements;
    list<SgStatement *> duplicateStatements;

    // This gives us the total number of statements checked, useful for
    // reporting
    int totalStatements = statementList.size();
    int numberOfDuplicates = 0;
    int numberOfUniques = 0;
    int numberOfShared = 0;

    // Go through the statements, and put any duplicates found into
    // duplicateStatements
    for (SgStatementPtrList::iterator i = statementList.begin();
         i != statementList.end(); i++) {
      SgStatement *currStatement = *i;

      // ignore shared nodes
      if (currStatement->get_file_info()->isShared()) {
        numberOfShared++;
      } else {
        // DQ (2/22/2007): It is likely that count is more expensive than find
        // since we only want existence.
        if (seenStatements.count(currStatement) == 0) {
          seenStatements.insert(currStatement);
          numberOfUniques++;
        } else {
          duplicateStatements.push_back(currStatement);
          numberOfDuplicates++;
        }
      }
    }

    if (numberOfDuplicates != 0) {
      cout << "Problematic Node: " << node->sage_class_name()
           << " found. (a statement appears more than once in this scope)"
           << endl;

      printf("Error: number of [non-shared] statements = %d  number of "
             "unique statements = %d \n",
             totalStatements - numberOfShared, numberOfUniques);

      printf("Number of duplicate statements = %d \n", numberOfDuplicates);
      int counter = 0;
      for (list<SgStatement *>::iterator j = duplicateStatements.begin();
           j != duplicateStatements.end(); j++) {
        SgStatement *currDuplicate = *j;
        ROSE_ASSERT(currDuplicate != NULL);

        // DQ (6/26/2016): Debugging a special case that appears with ROSE
        // compiles "rose.h" header file.
        SgTemplateInstantiationDefn *templateInstantiationDefn =
            isSgTemplateInstantiationDefn(node);
        if (templateInstantiationDefn != NULL) {
          SgTemplateInstantiationDecl *templateInstantiationDecl =
              isSgTemplateInstantiationDecl(
                  templateInstantiationDefn->get_declaration());
          ROSE_ASSERT(templateInstantiationDecl != NULL);
          printf("ERROR: problem declaration: templateInstantiationDecl = %p "
                 "= %s \n",
                 templateInstantiationDecl,
                 templateInstantiationDecl->get_name().str());
        }

        Sg_File_Info *location = currDuplicate->get_file_info();
        ROSE_ASSERT(location != NULL);
        printf("Error: node (%d/%d) = %p = %s at: \n", counter,
               numberOfDuplicates, currDuplicate,
               currDuplicate->sage_class_name());

        // DQ (3/21/2011): Added more detail to debug duplicate entries...
        printf("currDuplicate name = %s \n",
               SageInterface::get_name(currDuplicate).c_str());

        location->display("redundant IR node");
        counter++;
      }
      printf("Error: duplicate statements in scope = %p = %s \n", scope,
             scope->class_name().c_str());
      scope->get_file_info()->display("Error: duplicate statements in scope");
      ROSE_ABORT();
    }
  }
}

/*! \page AstProperties AST Properties (Consistency Tests)

\section section2 Unique IR nodes in the AST

     This test verifies each IR nodes visited in the AST is only visited once.
This is a more robust version of the previous test which checked for shared
IR nodes in the same scope (which is a more common problem).  This test is
more expensive in memory since it has to save a reference to ever IR node
and test if it has been previously seen.
*/

void TestAstForUniqueNodesInAST::visit(SgNode *node) {
  ROSE_ASSERT(node != nullptr);
  if (!astNodeSet.insert(node).second) {
    std::cerr << "REX_AST_INVARIANT[unique-structural-node]: "
              << node->class_name() << " " << node
              << " appears more than once in the structural traversal\n";
    ROSE_ABORT();
  }
}

void TestAstForUniqueNodesInAST::test(SgNode *node) {
  // DQ (4/3/2012): Added test to make sure that the pointers are unique.
  TestAstForUniqueNodesInAST redundentNodeTest;
  redundentNodeTest.traverse(node, preorder);
}

void testAstForUniqueNodes(SgNode *node) {
  // DQ (4/3/2012): Added test to make sure that the pointers are unique.
  TestAstForUniqueNodesInAST::test(node);
}

/*! \page AstProperties AST Properties (Consistency Tests)

\section section3 Rules for Defining and Nondefining Declarations

    We separate defining and non-defining declarations so that many aspects of
analysis and transformation are simplified, along with code generation. For
example, if from any function declaration the function definition is sought, it
is available from the defining declaration (this applied uniformly to all
declarations). These tests verify that the handling of defining and non-defining
declaration follow specific rules (with a goal toward uniformity and intuitive
behavior).

    Nondefining declarations appear as forward declarations and references to
declarations within types.  These many appear many times within the source code
and as a result are non unique within the AST.  For example, each forward
declaration of a function or class within a source code becomes a non-defining
declaration.

    Defining declarations contain their definition, and function appearing with
its body (implementation) is a defined declaration containing a function
definition (the scope of the function).  The defining declaration should appear
only once within the source code, by the One Time Definition rule, (OTD).  Each
forward declaration, clearly becomes a separate declaration but it may be shared
as needed to reduce the total number of non-defining declarations, which are
also referenced in types.

   Within SAGE III, every declaration has a reference to its first non-defining
declaration and its defined declaration if it exists (is defined within the
current translation unit).  If in processing a defining declaration an reference
is required, get_declaration() always returns the non-defined declaration.  The
defined declaration is only available explicitly (via a function call) and is
never returned through any other mechanism.  Thus non-defining declarations are
shared and defining declaration are never shared within the AST.

\subsection subsection3a When defining and non-defining declarations are the
same SgEnumDeclaration declarations are not allowed to forward reference their
definitions, this they are the same and the defining and non-defining
declaration for a SgEnumDeclaration are pointer values which are the same.

\example The following assertion is true for all SgEnumDeclaration objects: \n
     assert (declaration->get_definingDeclaration() ==
declaration->get_firstNondefiningDeclaration());

\subsection subsection3b Scopes of defining and nondefining declarations match
(same pointer value) For all defining and nondefining declarations the scopes
are the same, however for those that are in namespaces the actual
SgNamespaceDefinition of a defining and non-defining declaration could be
different.  To simplify analysis, the namespaces of defining and non-defining
declarations are set to the SgNamespaceDefinition of the defined declaration.
These test verify the equality of the pointers for all scopes of defining and
non-defining declarations.

\example The following assertion is always true: \n
     assert (declaration->get_definingDeclaration()->get_scope() ==
declaration->get_firstNondefiningDeclaration()->get_scope());

\subsection subsection3c Defining and nondefining declarations are non-null
pointers which never match (different pointer values)

   The following SgDeclarationStatement IR nodes never share the same
declaration and are always valid (non-null) pointers.
       -# SgAsmStmt \n
          This is a not well tested declaration within Sage III (but I think
that any declaration must be a defining declaration)
       -# SgFunctionParameterList
       -# SgCtorInitializerList \n
               These are special case declarations.
       -# SgVariableDefinition \n
          A variable definition appears with a variable declaration, but a
variable declaration can be a forward reference to the variable declaration
containing the variable definitions (e.g. "extern int x;", is a forward
declaration to the declaration of "x").
       -# SgPragmaDeclaration \n
          A pragam can contain no references to it and so it's declaration is
also it's definition
       -# SgUsingDirectiveStatement
       -# SgUsingDeclarationStatement
       -# SgNamespaceAliasDeclarationStatement
       -# SgTemplateInstantiationDirectiveStatement \n
          These can appear multiple times and are not really associated with
definitions (but for consistancy they are consired to be their own defining
declaration).
       -# SgNamespaceDeclarationStatement \n
          Namespaces can't appear without their definitions (or so it seems, and
it is tested).

\example The following assertion is true for all the above listed cases: \n
     assert (declaration->get_definingDeclaration() != NULL); \n
     assert (declaration->get_firstNondefiningDeclaration() != NULL); \n
     assert (declaration->get_definingDeclaration() !=
declaration->get_firstNondefiningDeclaration());

\subsection subsection3d Defining and nondefining declarations which never match
(non-defining declaration may be NULL)

     This case is similar to \ref subsection3c but the non-defining declaration
can be a null values pointer. This is because a non-defining declaration may not
exist (as in the case of a function defined with its definition and without any
function prototype)  The following cases are tested for this properly:

       -# SgVariableDeclaration \n
          This case is a bit special.
       -# SgTemplateDeclaration
       -# SgFunctionDeclaration
          The non-defining declaration is always a valid pointer.
       -# SgClassDeclaration
       -# SgTypedefDeclaration
       -# SgMemberFunctionDeclaration
       -# SgTemplateInstantiationFunctionDecl
       -# SgTemplateInstantiationDecl
       -# SgTemplateInstantiationMemberFunctionDecl \n
          These can have forward declarations separated from their definitions
          so a declaration may be either a defining or non-defining declaration.
          All declarations, except the defining declaration, are the same object
          as the non-defining declaration if it is non-null.

\example The following assertion is true for all the above listed cases: \n
     assert (declaration->get_definingDeclaration() != NULL); \n
     assert (declaration->get_definingDeclaration() !=
declaration->get_firstNondefiningDeclaration());

 */
void TestAstForProperlySetDefiningAndNondefiningDeclarations::visit(
    SgNode *node) {
  // DQ (6/24/2005): Test setup of defining and non-defining declaration
  // pointers for each SgDeclarationStatement

  SgDeclarationStatement *declaration = isSgDeclarationStatement(node);
  SgDeclarationStatement *definingDeclaration = NULL;
  SgDeclarationStatement *firstNondefiningDeclaration = NULL;
  if (declaration != NULL) {
    if (SgAccessLabelStatement *label = isSgAccessLabelStatement(declaration)) {
      label->validate();
      SgClassDefinition *owner = isSgClassDefinition(label->get_parent());
      ROSE_ASSERT(owner != NULL);
      ROSE_ASSERT(label->get_scope() == owner);
      ROSE_ASSERT(std::count(owner->get_members().begin(),
                             owner->get_members().end(), label) == 1);
      return;
    }
    if (SgDeclarationGroupStatement *group =
            isSgDeclarationGroupStatement(declaration)) {
      group->validate();
      return;
    }
    if (declaration->get_scope() == NULL) {
      printf("Error: declaration with NULL scope encountered: %p = %s = %s \n",
             declaration, declaration->class_name().c_str(),
             SageInterface::get_name(declaration).c_str());
    }
    ROSE_ASSERT(declaration->get_scope() != NULL);

    ROSE_ASSERT(declaration != NULL);

    definingDeclaration = declaration->get_definingDeclaration();
    firstNondefiningDeclaration =
        declaration->get_firstNondefiningDeclaration();

    if (definingDeclaration == NULL && firstNondefiningDeclaration == NULL) {
      fprintf(stderr,
              "REX_AST_INVARIANT[declaration-chain]: node=%p type=%s name=%s "
              "has neither a defining nor a first nondefining declaration\n",
              declaration, declaration->class_name().c_str(),
              SageInterface::get_name(declaration).c_str());
    }
    ROSE_ASSERT(definingDeclaration != NULL ||
                firstNondefiningDeclaration != NULL);

    // DQ (7/23/2005): The scopes should match!
    if (definingDeclaration != NULL && firstNondefiningDeclaration != NULL) {
      // DQ (5/5/2007): A merged AST can have many SgGlobal (global scope)
      // objects and we have to allow this. Thus a function with a prototype
      // declaration in one file but and both a prototype and function
      // definition in another file, will have references to different scopes
      // (from get_scope()).  I think this is OK, but output a warning for now.
      // And it applys more broadly to all declarations with secondary forms
      // (defining and non-defining declarations).

      SgScopeStatement *definingDeclarationScope =
          definingDeclaration->get_scope();
      SgScopeStatement *firstNondefiningDeclarationScope =
          firstNondefiningDeclaration->get_scope();

      // DQ (10/22/2016): If these are a namespace definition then we want to
      // check the global definition.
      SgNamespaceDefinitionStatement *namespaceDefinition_defining =
          isSgNamespaceDefinitionStatement(definingDeclarationScope);
      SgNamespaceDefinitionStatement *namespaceDefinition_firstNondefining =
          isSgNamespaceDefinitionStatement(firstNondefiningDeclarationScope);
      if (namespaceDefinition_defining != NULL &&
          namespaceDefinition_firstNondefining != NULL) {
        definingDeclarationScope =
            namespaceDefinition_defining->get_global_definition();
        firstNondefiningDeclarationScope =
            namespaceDefinition_firstNondefining->get_global_definition();
      }

      if (definingDeclarationScope != firstNondefiningDeclarationScope) {
        // DQ (5/10/2007): With a merged AST the primary and secondary
        // declarations can be in different global scopes since we don't merge
        // to a unique global scope.  So when we are checking a merged AST this
        // is OK.
        if (isSgGlobal(definingDeclarationScope) != NULL &&
            isSgGlobal(firstNondefiningDeclarationScope) != NULL) {
          // This is a case of the primary and secondary declaration in the
          // global scope, but from different files (associated with a result of
          // a merged AST). printf ("Note: Case of definingDeclaration and
          // firstNondefiningDeclarationScope in different SgGlobal scopes \n");
        } else {
          // But if they are not global scopes then we want to report this
          // because it could be an error.
          printf(
              "\n\n******************************************************\n");
          printf("Error in scope: declaration = %p = %s \n", declaration,
                 declaration->class_name().c_str());
          printf("definingDeclaration ========= %p = %s = %s: get_scope() = %p "
                 "= %s = %s \n",
                 definingDeclaration,
                 SageInterface::get_name(definingDeclaration).c_str(),
                 definingDeclaration->class_name().c_str(),
                 definingDeclaration->get_scope(),
                 definingDeclaration->get_scope()->class_name().c_str(),
                 SageInterface::get_name(definingDeclaration->get_scope())
                     .c_str());
          printf(
              "firstNondefiningDeclaration = %p = %s = %s: get_scope() = %p = "
              "%s = %s \n",
              firstNondefiningDeclaration,
              SageInterface::get_name(firstNondefiningDeclaration).c_str(),
              firstNondefiningDeclaration->class_name().c_str(),
              firstNondefiningDeclaration->get_scope(),
              firstNondefiningDeclaration->get_scope()->class_name().c_str(),
              SageInterface::get_name(firstNondefiningDeclaration->get_scope())
                  .c_str());

          printf("definingDeclaration parent = %p = %s = %s \n",
                 definingDeclaration->get_parent(),
                 definingDeclaration->get_parent()->class_name().c_str(),
                 SageInterface::get_name(definingDeclaration->get_parent())
                     .c_str());

          declaration->get_file_info()->display(
              "location of problem declaaration");

          // DQ (3/4/3009): This test fails for test2005_118.C when run from the
          // copyAST_tests directory (AST Copy tests).
          if (definingDeclarationScope != firstNondefiningDeclarationScope) {
            // I think this might be reasonable to fail for this test of the AST
            // Copy mechanism, but it needs to be looked into deeper.
            printf("This test fails for test2005_118.C when run from the "
                   "copyAST_tests directory (AST Copy tests) \n");
          }
        }
      }

      // DQ (3/1/2013): Adding test for access specification (public, protected,
      // private).  First we need to argue that these should be the same for the
      // non-defining and defining declaration.  I am not clear that they should
      // be the same.
      SgAccessModifier::access_modifier_enum
          definingDeclaration_access_modifier =
              definingDeclaration->get_declarationModifier()
                  .get_accessModifier()
                  .get_modifier();
      SgAccessModifier::access_modifier_enum
          firstNondefiningDeclaration_access_modifier =
              firstNondefiningDeclaration->get_declarationModifier()
                  .get_accessModifier()
                  .get_modifier();
      if (definingDeclaration_access_modifier !=
          firstNondefiningDeclaration_access_modifier) {
        // DQ (6/30/2014): I think this is not an error for
        // SgTemplateInstantiationDecl.
        if (isSgTemplateInstantiationDecl(definingDeclaration) != NULL) {
          // DQ (3/11/2017): Fixed to use message streams.
          // MLOG_WARN_C("astDiagnostics", "(different access modifiers used)
          // definingDeclaration = %p firstNondefiningDeclaration = %p = %s
          // \n",definingDeclaration,firstNondefiningDeclaration,firstNondefiningDeclaration->class_name().c_str());
          MLOG_KEY_C("astDiagnostics",
                     "(different access modifiers used) definingDeclaration = "
                     "%p firstNondefiningDeclaration = %p = %s  \n",
                     definingDeclaration, firstNondefiningDeclaration,
                     firstNondefiningDeclaration->class_name().c_str());
          MLOG_KEY_MORE_C("definingDeclaration_access_modifier         = %d \n",
                          definingDeclaration_access_modifier);
          MLOG_KEY_MORE_C("firstNondefiningDeclaration_access_modifier = %d \n",
                          firstNondefiningDeclaration_access_modifier);
        }
      }
    }
  }

  switch (node->variantT()) {
  case V_SgEnumDeclaration: {
    break;
  }

    // DQ (9/6/2005): Ignoring this case!
  case V_SgFunctionParameterList: {
    break;
  }

    // DQ (6/26/2005): These are likely somewhat special and such that they
    // should be their own defining declarations, I think

    // This is a not well tested declaration within Sage III (but I think that
    // any declaration must be a defining declaration)
  case V_SgAsmStmt:

    // These are special case declarations
    // case V_SgFunctionParameterList:
  case V_SgCtorInitializerList:
  case V_SgFortranIncludeLine:

    // A variable definition appears with a variable declaration, but a variable
    // declaration can be a forward reference to the variable declaration
    // containing the variable definitions (e.g. "extern int x;", is a forward
    // declaration to the declaration of "x").
  case V_SgVariableDefinition:

    // A pragam can contain no references to it and so it's declaration is also
    // it's definition
  case V_SgPragmaDeclaration:

    // These can appear multiple times and are not really associated with
    // definitions (but for consistency they are considered to be their own
    // defining declaration).
  case V_SgUsingDirectiveStatement:
  case V_SgUsingDeclarationStatement:
  case V_SgNamespaceAliasDeclarationStatement:
  case V_SgTemplateInstantiationDirectiveStatement:

    // Shared by all the above cases!
    {
      // DQ (6/26/2005): I think that the C++ standard does not allow forward
      // declarations for these either! So the defining declaration should be
      // the declaration itself (I think).  either that or we need to build a
      // special non-defining declaration for these declarations.
      if (declaration != definingDeclaration) {
        printf("declaration = %p (%s)\n", declaration,
               declaration->class_name().c_str());
        if (definingDeclaration) {
          printf("definingDeclaration = %p (%s)\n", definingDeclaration,
                 definingDeclaration->class_name().c_str());
        }
        if (declaration->get_definingDeclaration()) {
          printf("declaration->get_definingDeclaration() = %p (%s)\n",
                 declaration->get_definingDeclaration(),
                 declaration->get_definingDeclaration()->class_name().c_str());
        }
        if (declaration->get_firstNondefiningDeclaration()) {
          printf("declaration->get_firstNondefiningDeclaration() = %p (%s)\n",
                 declaration->get_firstNondefiningDeclaration(),
                 declaration->get_firstNondefiningDeclaration()
                     ->class_name()
                     .c_str());
        }
      }
      ROSE_ASSERT(declaration == definingDeclaration);
      break;
    }

    // Namespaces can't appear without their definitions (or so it seems,
    // tested). Since there can be many declarations of the same namespace the
    // definingDeclaration is always NULL (only for
    // SgNamespaceDeclarationStatement)
  case V_SgNamespaceDeclarationStatement: {
    ROSE_ASSERT(declaration != NULL);
    ROSE_ASSERT(definingDeclaration == NULL);
    ROSE_ASSERT(firstNondefiningDeclaration != NULL);
    break;
  }

    // This case is a bit special
  case V_SgVariableDeclaration:

    // These can have forward declarations separated from their definitions
    // so a declaration may be either a defining or non-defining declaration.
  case V_SgTemplateDeclaration:
  case V_SgFunctionDeclaration:
  case V_SgClassDeclaration:
  case V_SgDerivedTypeStatement:
  case V_SgTypedefDeclaration:
  case V_SgMemberFunctionDeclaration:
  case V_SgTemplateInstantiationFunctionDecl:
  case V_SgTemplateInstantiationDecl:
  case V_SgTemplateInstantiationMemberFunctionDecl:
  // Liao 12/2/2010, add new Fortran function nodes
  case V_SgProcedureHeaderStatement:
  case V_SgProgramHeaderStatement: {
    // For some declarations, the only declaration is a defining declaration, in
    // which case the non-defining declaration is NULL (except in the case of
    // SgClassDeclarations, where a non-defining declarations in generated
    // internally).
    if (firstNondefiningDeclaration == NULL) {
      ROSE_ASSERT(declaration != NULL);
      switch (declaration->variantT()) {
        // These nodes should have a non-defining declaration even if only a
        // defining declaration is present in the source code.  It may be that
        // the other IR nodes below should be treated similarly.
      case V_SgTemplateInstantiationDecl:
      case V_SgTemplateInstantiationFunctionDecl:
      case V_SgTemplateInstantiationMemberFunctionDecl: {
        // Defining template instantiations do not necessarily have a distinct
        // nondefining declaration.
        break;
      }

      case V_SgClassDeclaration:
      case V_SgDerivedTypeStatement: {
        printf("Warning AST Consistancy Test: declaration %p = %s has no "
               "firstNondefiningDeclaration = %p \n",
               declaration, declaration->sage_class_name(),
               firstNondefiningDeclaration);
        declaration->get_file_info()->display(
            "has no firstNondefiningDeclaration");
        break;
      }

      case V_SgTemplateDeclaration:
      case V_SgTypedefDeclaration: {
        // These are special cases
        break;
      }

      case V_SgFunctionDeclaration:
      case V_SgMemberFunctionDeclaration:
      case V_SgProcedureHeaderStatement:
      case V_SgProgramHeaderStatement: {
        // This is the reasonable case, where a function or template or typedef
        // is declared once (and only once and contains its definition).  Verify
        // that the definition is present!
        SgFunctionDeclaration *functionDeclaration =
            isSgFunctionDeclaration(declaration);
        ROSE_ASSERT(functionDeclaration != NULL);
        break;
      }

      default: {
        // Nothing to do here!
      }

        // And the defining declaration is the current definition
        ROSE_ASSERT(declaration == definingDeclaration);
      }
    }
    if (firstNondefiningDeclaration == definingDeclaration) {
      if (SgFunctionDeclaration *function =
              isSgFunctionDeclaration(declaration)) {
        fprintf(stderr,
                "REX_AST_INVARIANT[function-declaration-chain]: "
                "function=%p name=%s first=%p defining=%p must have distinct "
                "canonical and defining declarations\n",
                static_cast<void *>(function), function->get_name().str(),
                static_cast<void *>(firstNondefiningDeclaration),
                static_cast<void *>(definingDeclaration));
        ROSE_ABORT();
      }

      // DQ (12/12/2009): Suppress the warning about this for the case of a
      // SgTypedefDeclaration if not set to verbose mode. This is important to
      // reducing the output from the tests of AST merge in the mergeAST_tests
      // directory.
      SgTypedefDeclaration *typedefDeclaration =
          isSgTypedefDeclaration(declaration);
      if (typedefDeclaration == NULL || (SgProject::get_verbose() > 0)) {
        printf(
            "Warning AST Consistency Test: declaration %p = %s = %s has equal "
            "firstNondefiningDeclaration and definingDeclaration = %p \n",
            declaration, declaration->class_name().c_str(),
            SageInterface::get_name(declaration).c_str(),
            firstNondefiningDeclaration);
        printf("declaration->get_definingDeclaration() = %p "
               "declaration->get_firstNondefiningDeclaration() = %p \n",
               declaration->get_definingDeclaration(),
               declaration->get_firstNondefiningDeclaration());
        declaration->get_file_info()->display(
            "firstNondefiningDeclaration == definingDeclaration: debug");
      }
    } // end if nondefining == defining

    // DQ (8/6/2007): Comment this out, at least for SgTypedefDeclaration it
    // should be OK, MAYBE. DQ (3/4/2007): Temporarily commented out (now
    // uncommented) ROSE_ASSERT(firstNondefiningDeclaration !=
    // definingDeclaration);
    break;
  }

  default: {
    // Nothing to do here!
  }
  }
}

void TestAstSymbolTables::visit(SgNode *node) {

  SgScopeStatement *scope = isSgScopeStatement(node);
  if (scope != NULL) {
    SgSymbolTable *symbolTable = scope->get_symbol_table();
    if (symbolTable == NULL) {
      printf("Error: symbolTable == NULL, no symbol table found at node = %p = "
             "%s \n",
             node, node->class_name().c_str());
    }
    ROSE_ASSERT(symbolTable != NULL);
    if (symbolTable->get_table() == NULL) {
      printf("Error: symbolTable->get_table() == NULL, no symbol table found "
             "at node = %p = %s \n",
             node, node->class_name().c_str());
    }
    ROSE_ASSERT(symbolTable->get_table() != NULL);

    SgSymbolTable::BaseHashType *internalTable = symbolTable->get_table();

    // Iterate over each symbol in the table and test it separately!
    SgSymbolTable::hash_iterator i = internalTable->begin();
    while (i != internalTable->end()) {
      SgSymbol *symbol = i->second;
      ROSE_ASSERT(symbol != nullptr);

      ROSE_ASSERT(symbol->get_parent() == symbolTable);

      // DQ (8/6/2005): Test the get_declaration() function on all symbols
      TestAstAccessToDeclarations::test(symbol);

      // DQ (7/3/2007): This is a common point of failure in the AST merge
      // mechanism.
      SgNode *declarationNode = symbol->get_symbol_basis();
      ROSE_ASSERT(declarationNode != NULL);
      SgDeclarationStatement *declarationStatement =
          isSgDeclarationStatement(declarationNode);

      // DQ (12/9/2007): Skip symbols that come from labels since they are often
      // numeric labels and need to be tested in a Fortran specific way.
      if (declarationStatement != nullptr &&
          isSgLabelSymbol(symbol) == nullptr) {
        SgSymbol *local_symbol =
            localSymbolForCurrentEntry(symbol, declarationStatement, scope);
        SgMemberFunctionDeclaration *memberFunctionDeclaration =
            isSgMemberFunctionDeclaration(declarationStatement);
        if (memberFunctionDeclaration != NULL &&
            memberFunctionDeclaration->get_associatedClassDeclaration() !=
                NULL) {
          ROSE_ASSERT(memberFunctionDeclaration->get_scope() != nullptr);
        } else if (local_symbol == nullptr) {
          ROSE_ASSERT(declarationStatement->get_firstNondefiningDeclaration() !=
                      nullptr);
        }
      } else if (auto *group = isSgNameGroup(declarationNode)) {
        auto *namelist = SgNamelistSymbol::resolve(symbol);
        ROSE_ASSERT(namelist->get_symbol_basis() == group);
        ROSE_ASSERT(
            namelist->get_scope()->get_symbol_table()->exists(namelist));
      } else {
        SgInitializedName *initializedName =
            isSgInitializedName(declarationNode);
        if (initializedName != NULL) {
          SgSymbol *local_symbol =
              localSymbolForCurrentEntry(symbol, initializedName);
          if (local_symbol == nullptr) {
            printf("Error: initializedName->get_symbol_from_symbol_table() == "
                   "NULL initializedName = %p = %s \n",
                   initializedName, initializedName->get_name().str());
            printf("Error: initializedName parent = %p = %s \n",
                   initializedName->get_parent(),
                   initializedName->get_parent() != NULL
                       ? initializedName->get_parent()->class_name().c_str()
                       : "NULL");
            printf("Error: initializedName scope = %p = %s \n",
                   initializedName->get_scope(),
                   initializedName->get_scope() != NULL
                       ? initializedName->get_scope()->class_name().c_str()
                       : "NULL");
            printf("Error: initializedName declptr = %p = %s \n",
                   initializedName->get_declptr(),
                   initializedName->get_declptr() != NULL
                       ? initializedName->get_declptr()->class_name().c_str()
                       : "NULL");
            printf("Error: initializedName prev_decl_item = %p = %s \n",
                   initializedName->get_prev_decl_item(),
                   initializedName->get_prev_decl_item() != NULL
                       ? initializedName->get_prev_decl_item()->get_name().str()
                       : "NULL");
            initializedName->get_startOfConstruct()->display(
                "Error: initializedName->get_symbol_from_symbol_table() == "
                "NULL");
            SgLocatedNode *locatedNodeParent =
                isSgLocatedNode(initializedName->get_parent());
            ROSE_ASSERT(locatedNodeParent != NULL);
            locatedNodeParent->get_startOfConstruct()->display(
                "Error: initializedName->get_symbol_from_symbol_table() == "
                "NULL: locatedNodeParent");

            SgScopeStatement *scope = initializedName->get_scope();
            ROSE_ASSERT(scope != NULL);
            ROSE_ASSERT(scope->get_symbol_table() != NULL);

            printf("In TestAstSymbolTables::visit(): output the symbol tables "
                   "for the scope = %p = %s \n",
                   scope, scope->class_name().c_str());
            scope->get_symbol_table()->print();
          }
          ROSE_ASSERT(local_symbol != NULL);
        } else {
          if (isSgLabelStatement(declarationNode)) {
            SgLabelStatement *labelStatement =
                (SgLabelStatement *)declarationNode;
            SgSymbol *local_symbol =
                localSymbolForCurrentEntry(symbol, labelStatement);
            if (local_symbol == nullptr) {
              printf("Error: labelStatement->get_symbol_from_symbol_table() == "
                     "NULL labelStatement = %p = %s \n",
                     labelStatement, labelStatement->get_label().str());
              ROSE_ASSERT(labelStatement->get_scope() != NULL);
              labelStatement->get_scope()->get_symbol_table()->print(
                  "debug labelStatement scope");
            }
            ROSE_ASSERT(local_symbol != NULL);
          } else {
            // DQ (12/9/2007): Added support for fortran in SgLabelSymbol.
            SgLabelSymbol *labelSymbol = isSgLabelSymbol(symbol);
            if (labelSymbol != NULL) {
              ROSE_ASSERT(labelSymbol->get_fortran_statement() != NULL);
            } else {
              printf("Error: declaration found in symbol not handled "
                     "(declarationNode = %s) \n",
                     declarationNode->class_name().c_str());
              ROSE_ABORT();
            }
          }
        }
      }

      // DQ (12/16/2007): Added test
      ASSERT_not_null(symbol);

      // We have to look at each type of symbol separately!  This is because
      // there is no virtual function, the reason for this is that each
      // get_declaration() function returns a different type! ROSE_ASSERT (
      // symbol->get_declaration() != NULL );
      switch (symbol->variantT()) {
      case V_SgClassSymbol: {
        SgClassSymbol *classSymbol = isSgClassSymbol(symbol);
        ROSE_ASSERT(classSymbol != NULL);
        ROSE_ASSERT(classSymbol->get_declaration() != NULL);

        // DQ (12/27/2011): Make sure this is not a SgClassSymbol that is
        // incorrectly associated with a SgTemplateClassDeclaration.
        ROSE_ASSERT(isSgTemplateClassDeclaration(
                        classSymbol->get_declaration()) == NULL);
        break;
      }

        // DQ (12/27/2011): Added new symbol (and required support).
      case V_SgTemplateClassSymbol: {
        SgTemplateClassSymbol *templateClassSymbol =
            isSgTemplateClassSymbol(symbol);
        ROSE_ASSERT(templateClassSymbol != NULL);
        ROSE_ASSERT(templateClassSymbol->get_declaration() != NULL);

        // DQ (12/27/2011): Make sure this is correctly associated with a
        // SgTemplateClassDeclaration.
        ASSERT_not_null(isSgTemplateClassDeclaration(
            templateClassSymbol->get_declaration()));
        break;
      }

      case V_SgDefaultSymbol: {
        printf("The SgDefaultSymbol should not be present in the AST \n");
        ROSE_ABORT();
      }

      case V_SgEnumFieldSymbol: {
        // Note that the type returned by get_declaration is SgInitializedName
        // and not any sort of SgDeclaration
        SgEnumFieldSymbol *enumFieldSymbol = isSgEnumFieldSymbol(symbol);
        ROSE_ASSERT(enumFieldSymbol != NULL);
        ROSE_ASSERT(enumFieldSymbol->get_declaration() != NULL);
        break;
      }

      case V_SgEnumSymbol: {
        SgEnumSymbol *enumSymbol = isSgEnumSymbol(symbol);
        ROSE_ASSERT(enumSymbol != NULL);
        ROSE_ASSERT(enumSymbol->get_declaration() != NULL);
        break;
      }

        // DQ (12/28/2011): These can be handled using the same case.
      case V_SgTemplateFunctionSymbol:
      case V_SgTemplateMemberFunctionSymbol:

        // These can be handled by the same case
      case V_SgFunctionSymbol:
      case V_SgMemberFunctionSymbol: {
        SgFunctionSymbol *functionSymbol = isSgFunctionSymbol(symbol);
        ROSE_ASSERT(functionSymbol != NULL);
        ROSE_ASSERT(functionSymbol->get_declaration() != NULL);
        break;
      }

        // DQ (10/11/2008): Added to support renaming of functions using the
        // Fortran 90 interface statement. Note that the SgRenameSymbol is
        // derived from the SgFunctionSymbol. Unclear if any other symbols
        // should have a similar renamed version (e.g. SgMemberFunctionSymbol).
        // If this technique is uesd for C++ then the target of C++ aliases
        // might be supported.  It is not clear if this technique will be used
        // outside of the Fortran support.
      case V_SgRenameSymbol: {
        // This is an alias for a symbol injected from another scope as part of
        // a Fortran "use" statement (or perhaps eventually a C++ using
        // declaration or using directive).
        SgRenameSymbol *renameSymbol = isSgRenameSymbol(symbol);
        ROSE_ASSERT(renameSymbol != NULL);
        ROSE_ASSERT(renameSymbol->get_original_symbol() != NULL);
        ROSE_ASSERT(renameSymbol->get_declaration() != NULL);
        break;
      }

      case V_SgFunctionTypeSymbol: {
        // Note that we check the get_type() function here and not
        // get_declaration()
        SgFunctionTypeSymbol *functionTypeSymbol =
            isSgFunctionTypeSymbol(symbol);
        ROSE_ASSERT(functionTypeSymbol != NULL);
        ROSE_ASSERT(functionTypeSymbol->get_type() != NULL);
        break;
      }

      case V_SgLabelSymbol: {
        SgLabelSymbol *labelSymbol = isSgLabelSymbol(symbol);
        ROSE_ASSERT(labelSymbol != NULL);

        // DQ (12/9/2007): Modified to reflect added support for fortran
        // statement numeric labels. ROSE_ASSERT(labelSymbol->get_declaration()
        // != NULL);
        if (labelSymbol->get_declaration() == NULL) {
          // DQ (2/2/2011): Added support in SgLabelSymbol for Fortran
          // alternative return parameters (see test2010_164.f90).
          if (labelSymbol->get_fortran_statement() == NULL) {
            ROSE_ASSERT(labelSymbol->get_fortran_alternate_return_parameter() !=
                        NULL);
          }
        }
        break;
      }

      case V_SgInterfaceSymbol: {
        SgInterfaceSymbol *interfaceSymbol = isSgInterfaceSymbol(symbol);
        ROSE_ASSERT(interfaceSymbol != NULL);
        ROSE_ASSERT(interfaceSymbol->get_declaration() != NULL);
        break;
      }

      case V_SgModuleSymbol: {
        SgModuleSymbol *moduleSymbol = isSgModuleSymbol(symbol);
        ROSE_ASSERT(moduleSymbol != NULL);
        ROSE_ASSERT(moduleSymbol->get_declaration() != NULL);
        break;
      }

      case V_SgNamespaceSymbol: {
        SgNamespaceSymbol *namespaceSymbol = isSgNamespaceSymbol(symbol);
        ROSE_ASSERT(namespaceSymbol != NULL);
        ROSE_ASSERT(namespaceSymbol->get_symbol_basis() != NULL);
        break;
      }

      case V_SgTemplateSymbol: {
        SgTemplateSymbol *templateSymbol = isSgTemplateSymbol(symbol);
        ROSE_ASSERT(templateSymbol != NULL);
        ROSE_ASSERT(templateSymbol->get_declaration() != NULL);
        break;
      }

        // DQ (11/4/2014): Adding support for template typedef declarations.
      case V_SgTemplateTypedefSymbol:
      case V_SgTypedefSymbol: {
        SgTypedefSymbol *typedefSymbol = isSgTypedefSymbol(symbol);
        ROSE_ASSERT(typedefSymbol != NULL);
        ROSE_ASSERT(typedefSymbol->get_declaration() != NULL);
        break;
      }
      case V_SgTemplateVariableSymbol:
      case V_SgVariableSymbol: {
        // Note that the type returned by get_declaration is SgInitializedName
        // and not any sort of SgDeclaration
        SgVariableSymbol *variableSymbol = isSgVariableSymbol(symbol);
        ROSE_ASSERT(variableSymbol != NULL);
        ROSE_ASSERT(variableSymbol->get_declaration() != NULL);
        break;
      }

        // DQ (9/29/2008): Added support for symbols that are alias for other
        // symbol from othe scopes.
      case V_SgNamelistSymbol: {
        auto *namelist = isSgNamelistSymbol(symbol);
        ROSE_ASSERT(namelist != nullptr);
        namelist->get_scope();
        break;
      }
      case V_SgAliasSymbol: {
        // This is an alias for a symbol injected from another scope as part of
        // a Fortran "use" statement (or perhaps eventually a C++ using
        // declaration or using directive).
        SgAliasSymbol *aliasSymbol = isSgAliasSymbol(symbol);
        ROSE_ASSERT(aliasSymbol != NULL);
        ROSE_ASSERT(aliasSymbol->get_alias() != NULL);
        break;
      }

      case V_SgNonrealSymbol: {
        SgNonrealSymbol *nrsymbol = isSgNonrealSymbol(symbol);
        ROSE_ASSERT(nrsymbol != NULL);
        ROSE_ASSERT(nrsymbol->get_declaration() != NULL);
        break;
      }

      default: {
        printf("Error: default reached in switch (AstFixes.C) symbol = %s \n",
               symbol->class_name().c_str());
        ROSE_ABORT();
      }
      }

      // DQ (12/16/2007): Added test
      ASSERT_not_null(symbol);

      // DQ (6/10/2007): Test if the scopes match!
      // This also test the SgSymbol::get_symbol_basis() member function
      SgScopeStatement *decl_scope = symbol->get_scope();
      if (decl_scope == NULL) {
        // DQ (7/26/2007): Test for the special case of a member function
        // pointer to a class without a definition see test2007_116.C
        SgMemberFunctionSymbol *memberFunctionSymbol =
            isSgMemberFunctionSymbol(symbol);
        if (memberFunctionSymbol != NULL) {
          ROSE_ASSERT(memberFunctionSymbol->get_declaration() != NULL);
          SgMemberFunctionDeclaration *memberFunctionDeclaration =
              isSgMemberFunctionDeclaration(
                  memberFunctionSymbol->get_declaration());
          ROSE_ASSERT(
              memberFunctionDeclaration->get_associatedClassDeclaration() !=
              NULL);
        } else {
          printf("Error: symbol declaration's scope not set! symbol = %p = "
                 "%s \n",
                 symbol, symbol->class_name().c_str());
          ROSE_ABORT();
        }
      }

      // Increment iterator!
      i++;
    }
  }
}

void TestAstAccessToDeclarations::visit(SgNode *node) {
  // DQ (8/5/2005): Test the get_declaration() member function which is common
  // on many IR nodes printf ("node = %s \n",node->sage_class_name());

  TestAstAccessToDeclarations::test(node);
}

void TestAstAccessToDeclarations::test(SgNode *node) {
  // DQ (8/5/2005): Test the get_declaration() member function which is common
  // on many IR nodes printf ("node = %s \n",node->sage_class_name());

  switch (node->variantT()) {
    // IR nodes that have a get_declaration member function
  case V_SgInitializedName: {
    SgInitializedName *initializedName = isSgInitializedName(node);
    // Every variable should have a declaration (but initialized name objects
    // are used in many ways, so it is not clear if this is always true)

    // DQ (9/6/2005): This can be NULL when in a function parameter list of a
    // forward function declaration (but only if the parent has not been set)
    if (initializedName->get_declaration() == NULL) {
      printf("initializedName with NULL declaration at %p name = %s (seems to "
             "apply to function parameters) \n",
             initializedName, initializedName->get_name().str());
      initializedName->get_file_info()->display(
          "initializedName with NULL declaration");
    }
    break;
  }

  case V_SgVariableDefinition: {
    SgVariableDefinition *variableDefinition = isSgVariableDefinition(node);
    ROSE_ASSERT(variableDefinition->get_declaration() != NULL);
    break;
  }

  case V_SgTemplateInstantiationDirectiveStatement: {
    SgTemplateInstantiationDirectiveStatement *templateInstantiationDirective =
        isSgTemplateInstantiationDirectiveStatement(node);
    ROSE_ASSERT(templateInstantiationDirective->get_declaration() != NULL);
    break;
  }

  case V_SgUsingDeclarationStatement: {
    SgUsingDeclarationStatement *tmp = isSgUsingDeclarationStatement(node);
    // struct T { enum c { i }; }; struct U : T { using T::i; };
    // causes get_declaration() to return NULL, since the object in the using
    // declaration is a SgInitializedName.
    if (tmp->get_declaration() == NULL && tmp->get_initializedName() == NULL) {
      printf("SgUsingDeclarationStatement::get_declaration() and "
             "get_initializedName() both return NULL \n");
    }
    ROSE_ASSERT((tmp->get_declaration() != NULL) ||
                (tmp->get_initializedName() != NULL));
    break;
  }

  case V_SgFunctionDefinition: {
    SgFunctionDefinition *tmp = isSgFunctionDefinition(node);
    ROSE_ASSERT(tmp != NULL);
    ROSE_ASSERT(tmp->get_declaration() != NULL);
    break;
  }

  case V_SgNamespaceDefinitionStatement: {
    SgNamespaceDefinitionStatement *tmp =
        isSgNamespaceDefinitionStatement(node);
    // Note that this class uses a different name since it has another member
    // function "get_declarations()" which returns a list of the declarations in
    // the namespace. the issue was that two member function should not be
    // differentiated by a single letter "s" at the end of the name!
    ROSE_ASSERT(tmp->get_namespaceDeclaration() != NULL);
    break;
  }

  case V_SgClassDefinition: {
    SgClassDefinition *tmp = isSgClassDefinition(node);
    ROSE_ASSERT(tmp->get_declaration() != NULL);
    break;
  }

  case V_SgConstructorInitializer: {
    SgConstructorInitializer *tmp = isSgConstructorInitializer(node);
    ROSE_ASSERT(tmp->get_declaration() != nullptr ||
                tmp->get_class_decl() != nullptr ||
                tmp->get_associated_class_unknown());
    break;
  }

  case V_SgEnumVal: {
    SgEnumVal *tmp = isSgEnumVal(node);
    if (tmp->get_declaration() == NULL) {
      printf(
          "### Error: Need to fixup declaration in SgEnumVal node = %p ### \n",
          node);
    }
    break;
  }

    // This function is called from the symbol table tests (so that symbols are
    // tested)
  case V_SgVariableSymbol: {
    SgVariableSymbol *tmp = isSgVariableSymbol(node);
    ROSE_ASSERT(tmp->get_declaration() != NULL);
    break;
  }

    // This function is called from the symbol table tests (so that symbols are
    // tested)
  case V_SgClassSymbol: {
    SgClassSymbol *tmp = isSgClassSymbol(node);
    ROSE_ASSERT(tmp->get_declaration() != NULL);
    break;
  }

    // This function is called from the symbol table tests (so that symbols are
    // tested)
  case V_SgTemplateSymbol: {
    SgTemplateSymbol *tmp = isSgTemplateSymbol(node);
    ROSE_ASSERT(tmp->get_declaration() != NULL);
    break;
  }

    // This function is called from the symbol table tests (so that symbols are
    // tested)
  case V_SgEnumSymbol: {
    SgEnumSymbol *tmp = isSgEnumSymbol(node);
    ROSE_ASSERT(tmp->get_declaration() != NULL);
    break;
  }

    // This function is called from the symbol table tests (so that symbols are
    // tested)
  case V_SgEnumFieldSymbol: {
    SgEnumFieldSymbol *tmp = isSgEnumFieldSymbol(node);
    ROSE_ASSERT(tmp->get_declaration() != NULL);
    break;
  }

    // This function is called from the symbol table tests (so that symbols are
    // tested)
  case V_SgTypedefSymbol: {
    SgTypedefSymbol *tmp = isSgTypedefSymbol(node);
    if (tmp->get_declaration() == NULL) {
      printf("SgTypedefSymbol::get_declaration returns NULL \n");
    }
    ROSE_ASSERT(tmp->get_declaration() != NULL);
    break;
  }

    // This function is called from the symbol table tests (so that symbols are
    // tested)
  case V_SgLabelSymbol: {
    SgLabelSymbol *tmp = isSgLabelSymbol(node);

    // DQ (12/9/2007): Modified to reflect added support for fortran statement
    // numeric labels. ROSE_ASSERT(tmp->get_declaration() != NULL);
    if (tmp->get_declaration() == NULL) {
      // DQ (2/2/2011): Added support in SgLabelSymbol for Fortran alternative
      // return parameters (see test2010_164.f90).
      if (tmp->get_fortran_statement() == NULL) {
        ROSE_ASSERT(tmp->get_fortran_alternate_return_parameter() != NULL);
      }
    }
    break;
  }

    // This function is called from the symbol table tests (so that symbols are
    // tested)
  case V_SgNamespaceSymbol: {
    SgNamespaceSymbol *tmp = isSgNamespaceSymbol(node);
    ROSE_ASSERT(tmp->get_symbol_basis() != NULL);
    break;
  }

    // This function is called from the symbol table tests (so that symbols are
    // tested)
  case V_SgFunctionSymbol: {
    SgFunctionSymbol *tmp = isSgFunctionSymbol(node);
    ROSE_ASSERT(tmp->get_declaration() != NULL);
    break;
  }

    // This function is called from the symbol table tests (so that symbols are
    // tested)
  case V_SgMemberFunctionSymbol: {
    SgMemberFunctionSymbol *tmp = isSgMemberFunctionSymbol(node);
    ROSE_ASSERT(tmp->get_declaration() != NULL);
    break;
  }

    // This function is called from the type tests (so that these types are
    // tested)
  case V_SgNamedType:
  case V_SgClassType:
  case V_SgEnumType:
  case V_SgTypedefType: {
    SgNamedType *tmp = isSgNamedType(node);
    ROSE_ASSERT(tmp->get_declaration() != NULL);
    break;
  }

  default: {
    // Numerous IR nodes don't have a get_declaration member function and these
    // are ignored!
  }
  }
}

TestExpressionTypes::TestExpressionTypes() {
  // DQ (10/31/2016): Use this mechanism to make traversals transformation-safe
  // (it works). myProcessingObject->set_useDefaultIndexBasedTraversal(false);
  set_useDefaultIndexBasedTraversal(false);
}

void TestExpressionTypes::visit(SgNode *node) {
  // DQ (2/21/2006): Test the get_type() member function which is common on many
  // IR nodes printf ("In TestExpressionTypes::visit(): node = %s
  // \n",node->class_name().c_str());
  if (auto *reference = isSgNonrealRefExp(node);
      reference != nullptr &&
      reference->get_resolved_variable_declaration() != nullptr) {
    SageInterface::requireResolvedVariableTemplateReference(
        reference, "AST variable-template reference validation");
  }
  if (SgVariableDeclaration *variableDeclaration =
          isSgVariableDeclaration(node)) {
    validateVariableBaseTypeDefinitionOwner(variableDeclaration);
  }
  if (SgSizeOfOp *sizeOf = isSgSizeOfOp(node)) {
    validateExpressionTypeDefinitionOwner(
        sizeOf, sizeOf->get_operand_type(), sizeOf->get_operand_expr(),
        sizeOf->get_type_defining_declaration(),
        /*requireExclusiveTypeOperand=*/true, "SgSizeOfOp");
  }
  if (SgAlignOfOp *alignOf = isSgAlignOfOp(node)) {
    validateExpressionTypeDefinitionOwner(
        alignOf, alignOf->get_operand_type(), alignOf->get_operand_expr(),
        alignOf->get_type_defining_declaration(),
        /*requireExclusiveTypeOperand=*/true, "SgAlignOfOp");
  }
  if (SgCastExp *cast = isSgCastExp(node)) {
    validateExpressionTypeDefinitionOwner(
        cast, cast->get_type(), cast->get_operand(),
        cast->get_type_defining_declaration(),
        /*requireExclusiveTypeOperand=*/false, "SgCastExp");
  }
  if (SgAttributedStatement *statement = isSgAttributedStatement(node)) {
    statement->validate();
  }
  if (SgDeclarationGroupStatement *group =
          isSgDeclarationGroupStatement(node)) {
    group->validate();
    SgNode *owner = group->get_parent();
    const SgNodePtrList owner_successors =
        owner != NULL ? owner->get_traversalSuccessorContainer()
                      : SgNodePtrList();
    if (isSgStatement(owner) == NULL ||
        std::count(owner_successors.begin(), owner_successors.end(), group) !=
            1) {
      fprintf(stderr,
              "REX_AST_INVARIANT[declaration-group-owner]: group=%p has "
              "no exact typed statement owner=%p type=%s\n",
              static_cast<void *>(group), static_cast<void *>(owner),
              owner != NULL ? owner->class_name().c_str() : "<null>");
      ROSE_ABORT();
    }
  }
  if (SgAccessLabelStatement *label = isSgAccessLabelStatement(node)) {
    label->validate();
    SgClassDefinition *owner = isSgClassDefinition(label->get_parent());
    if (owner == NULL || label->get_scope() != owner ||
        std::count(owner->get_members().begin(), owner->get_members().end(),
                   label) != 1) {
      fprintf(stderr,
              "REX_AST_INVARIANT[access-label-owner]: label=%p has no exact "
              "lexical class-member owner\n",
              static_cast<void *>(label));
      ROSE_ABORT();
    }
  }
  if (SgClassDefinition *definition = isSgClassDefinition(node)) {
    SgClassDeclaration *class_declaration = definition->get_declaration();
    if (class_declaration == NULL) {
      fprintf(stderr,
              "REX_AST_INVARIANT[class-definition-declaration]: class=%p "
              "has no declaration\n",
              static_cast<void *>(definition));
      ROSE_ABORT();
    }

    const bool is_fortran_module =
        isSgModuleStatement(class_declaration) != NULL;
    const bool is_fortran_derived_type =
        isSgDerivedTypeStatement(class_declaration) != NULL;
    if (is_fortran_module || is_fortran_derived_type) {
      const SgClassDeclaration::class_types expected_kind =
          is_fortran_module ? SgClassDeclaration::e_fortran_module
                            : SgClassDeclaration::e_struct;
      if (class_declaration->get_class_type() != expected_kind ||
          !definition->isCaseInsensitive()) {
        fprintf(stderr,
                "REX_AST_INVARIANT[fortran-definition-kind]: class=%p "
                "declaration=%p type=%s class-key=%d expected=%d "
                "case-insensitive=%d\n",
                static_cast<void *>(definition),
                static_cast<void *>(class_declaration),
                class_declaration->class_name().c_str(),
                static_cast<int>(class_declaration->get_class_type()),
                static_cast<int>(expected_kind),
                definition->isCaseInsensitive() ? 1 : 0);
        ROSE_ABORT();
      }

      // Fortran MODULE and derived-type accessibility is expressed by typed
      // PUBLIC/PRIVATE attribute statements and declaration attributes.  It
      // does not have C++'s lexical access-label state machine.
      for (SgDeclarationStatement *member : definition->get_members()) {
        if (member == NULL || isSgAccessLabelStatement(member) != NULL) {
          fprintf(stderr,
                  "REX_AST_INVARIANT[fortran-member-access]: class=%p "
                  "member=%p type=%s cannot use a C++ access label\n",
                  static_cast<void *>(definition), static_cast<void *>(member),
                  member != NULL ? member->class_name().c_str() : "<null>");
          ROSE_ABORT();
        }
        const SgAccessModifier::access_modifier_enum access =
            member->get_declarationModifier()
                .get_accessModifier()
                .get_modifier();
        switch (access) {
        case SgAccessModifier::e_private:
        case SgAccessModifier::e_public:
        case SgAccessModifier::e_default:
        case SgAccessModifier::e_not_applicable:
        case SgAccessModifier::e_undefined:
          break;
        default:
          fprintf(stderr,
                  "REX_AST_INVARIANT[fortran-member-access]: class=%p "
                  "member=%p type=%s has invalid access=%d\n",
                  static_cast<void *>(definition), static_cast<void *>(member),
                  member->class_name().c_str(), static_cast<int>(access));
          ROSE_ABORT();
        }
      }
    } else {
      const bool semantic_template_instantiation =
          isSgTemplateInstantiationDefn(definition) != NULL &&
          class_declaration->get_specialization() !=
              SgDeclarationStatement::e_specialization;
      const bool semantic_auxiliary_class_definition =
          SageInterface::hasExactSemanticAuxiliaryOwnership(
              class_declaration) &&
          SageInterface::hasSemanticOnlyFrontendSourcePosition(
              class_declaration) &&
          SageInterface::hasSemanticOnlyFrontendSourcePosition(definition);
      // A source-less semantic class has no lexical class body in the Sage
      // tree.  Its on-demand members still retain Clang's exact effective
      // access, but manufacturing access-label statements would invent a
      // second source surface.  Validate that typed effective access directly,
      // just as for semantic template instantiations.  Source-backed
      // auxiliary declarations remain on the lexical access-label path.
      const bool effective_access_only = semantic_template_instantiation ||
                                         semantic_auxiliary_class_definition;
      SgAccessModifier::access_modifier_enum current_access;
      switch (class_declaration->get_class_type()) {
      case SgClassDeclaration::e_class:
      case SgClassDeclaration::e_lambda_closure:
        current_access = SgAccessModifier::e_private;
        break;
      case SgClassDeclaration::e_struct:
      case SgClassDeclaration::e_union:
        current_access = SgAccessModifier::e_public;
        break;
      default:
        fprintf(
            stderr,
            "REX_AST_INVARIANT[class-member-access]: class=%p has invalid "
            "class-key=%d\n",
            static_cast<void *>(definition),
            static_cast<int>(definition->get_declaration()->get_class_type()));
        ROSE_ABORT();
      }
      for (SgDeclarationStatement *member : definition->get_members()) {
        if (SgAccessLabelStatement *label = isSgAccessLabelStatement(member)) {
          if (effective_access_only) {
            Sg_File_Info *class_info = class_declaration->get_file_info();
            Sg_File_Info *label_info = label->get_file_info();
            fprintf(
                stderr,
                "REX_AST_INVARIANT[class-member-access]: effective-access-only "
                "class=%p name=%s source=%s:%d owns "
                "lexical access label=%p source=%s:%d\n",
                static_cast<void *>(definition),
                class_declaration->get_name().str(),
                class_info != nullptr ? class_info->get_filenameString().c_str()
                                      : "<null>",
                class_info != nullptr ? class_info->get_line() : 0,
                static_cast<void *>(label),
                label_info != nullptr ? label_info->get_filenameString().c_str()
                                      : "<null>",
                label_info != nullptr ? label_info->get_line() : 0);
            ROSE_ABORT();
          }
          label->validate();
          switch (label->get_label_kind()) {
          case SgAccessLabelStatement::e_access_label_private:
            current_access = SgAccessModifier::e_private;
            break;
          case SgAccessLabelStatement::e_access_label_protected:
            current_access = SgAccessModifier::e_protected;
            break;
          case SgAccessLabelStatement::e_access_label_public:
            current_access = SgAccessModifier::e_public;
            break;
          }
          continue;
        }
        const SgAccessModifier &access =
            member->get_declarationModifier().get_accessModifier();
        if (isSgPragmaDeclaration(member) != nullptr ||
            isOpenmpNonentityDirective(member)) {
          if (!access.isNotApplicable() || access.get_is_explicit()) {
            fprintf(stderr,
                    "REX_AST_INVARIANT[class-nonentity-access]: member=%p "
                    "type=%s effective=%d explicit=%d\n",
                    static_cast<void *>(member), member->class_name().c_str(),
                    static_cast<int>(access.get_modifier()),
                    access.get_is_explicit() ? 1 : 0);
            ROSE_ABORT();
          }
          continue;
        }
        if (member->get_declarationModifier().isFriend()) {
          if (!access.isNotApplicable() || access.get_is_explicit()) {
            fprintf(stderr,
                    "REX_AST_INVARIANT[class-friend-access]: member=%p "
                    "type=%s effective=%d explicit=%d\n",
                    static_cast<void *>(member), member->class_name().c_str(),
                    static_cast<int>(access.get_modifier()),
                    access.get_is_explicit() ? 1 : 0);
            ROSE_ABORT();
          }
          continue;
        }
        if (effective_access_only) {
          if (access.get_is_explicit() || access.isUnknown() ||
              access.isDefault() || access.isNotApplicable()) {
            fprintf(stderr,
                    "REX_AST_INVARIANT[class-member-access]: "
                    "effective-access-only class=%p member=%p/%s has invalid "
                    "effective "
                    "access=%d explicit=%d\n",
                    static_cast<void *>(definition),
                    static_cast<void *>(member), member->class_name().c_str(),
                    static_cast<int>(access.get_modifier()),
                    access.get_is_explicit() ? 1 : 0);
            ROSE_ABORT();
          }
          continue;
        }
        if (access.get_is_explicit() ||
            access.get_modifier() != current_access) {
          SgClassDeclaration *namedMember = isSgClassDeclaration(member);
          Sg_File_Info *memberInfo = member->get_file_info();
          Sg_File_Info *classInfo = class_declaration->get_file_info();
          fprintf(
              stderr,
              "REX_AST_INVARIANT[class-member-access]: class=%p type=%s "
              "name=%s class-key=%d specialization=%d source=%s:%d "
              "compiler-generated=%d member=%p type=%s name=%s effective=%d "
              "explicit=%d lexical=%d source=%s:%d compiler-generated=%d\n",
              static_cast<void *>(class_declaration),
              class_declaration->class_name().c_str(),
              class_declaration->get_name().str(),
              static_cast<int>(class_declaration->get_class_type()),
              static_cast<int>(class_declaration->get_specialization()),
              classInfo != nullptr ? classInfo->get_filenameString().c_str()
                                   : "<null>",
              classInfo != nullptr ? classInfo->get_line() : 0,
              classInfo != nullptr && classInfo->isCompilerGenerated() ? 1 : 0,
              static_cast<void *>(member), member->class_name().c_str(),
              namedMember != nullptr ? namedMember->get_name().str() : "",
              static_cast<int>(access.get_modifier()),
              access.get_is_explicit() ? 1 : 0,
              static_cast<int>(current_access),
              memberInfo != nullptr ? memberInfo->get_filenameString().c_str()
                                    : "<null>",
              memberInfo != nullptr ? memberInfo->get_line() : 0,
              memberInfo != nullptr && memberInfo->isCompilerGenerated() ? 1
                                                                         : 0);
          size_t memberIndex = 0;
          for (SgDeclarationStatement *orderedMember :
               definition->get_members()) {
            Sg_File_Info *orderedInfo = orderedMember != nullptr
                                            ? orderedMember->get_file_info()
                                            : nullptr;
            fprintf(stderr,
                    "REX_AST_INVARIANT[class-member-access-order]: "
                    "index=%zu member=%p type=%s source=%s:%d\n",
                    memberIndex++, static_cast<void *>(orderedMember),
                    orderedMember != nullptr
                        ? orderedMember->class_name().c_str()
                        : "<null>",
                    orderedInfo != nullptr
                        ? orderedInfo->get_filenameString().c_str()
                        : "<null>",
                    orderedInfo != nullptr ? orderedInfo->get_line() : 0);
          }
          ROSE_ABORT();
        }
      }
    }
  }
  if (SgNamespaceDeclarationStatement *declaration =
          isSgNamespaceDeclarationStatement(node)) {
    declaration->validate_source_fragments();
  }
  if (SgNamespaceSourceFragment *fragment = isSgNamespaceSourceFragment(node)) {
    SgNamespaceDeclarationStatement *owner =
        isSgNamespaceDeclarationStatement(fragment->get_parent());
    const bool is_introducer =
        owner != NULL &&
        owner->get_opening_introducer_source_fragment() == fragment;
    const bool is_opening =
        owner != NULL && owner->get_opening_source_fragment() == fragment;
    const bool is_closing =
        owner != NULL && owner->get_closing_source_fragment() == fragment;
    if (static_cast<unsigned int>(is_introducer) +
            static_cast<unsigned int>(is_opening) +
            static_cast<unsigned int>(is_closing) !=
        1) {
      fprintf(stderr,
              "REX_AST_INVARIANT[namespace-source-fragment-owner]: "
              "fragment=%p has no unique namespace declaration slot\n",
              static_cast<void *>(fragment));
      ROSE_ABORT();
    }
    fragment->validate();
  }
  if (SgStatementAttributeList *attribute_list =
          isSgStatementAttributeList(node)) {
    SgAttributedStatement *owner =
        isSgAttributedStatement(attribute_list->get_parent());
    if (owner == NULL || owner->get_attribute_list() != attribute_list) {
      fprintf(stderr,
              "REX_AST_INVARIANT[statement-attribute-list-owner]: list=%p "
              "has no exact attributed-statement owner\n",
              static_cast<void *>(attribute_list));
      ROSE_ABORT();
    }
    attribute_list->validate();
  }
  if (SgStatementAttribute *attribute = isSgStatementAttribute(node)) {
    SgStatementAttributeList *owner =
        isSgStatementAttributeList(attribute->get_parent());
    bool found = false;
    if (owner != NULL) {
      for (SgStatementAttribute *candidate : owner->get_attributes()) {
        if (candidate == attribute) {
          found = true;
          break;
        }
      }
    }
    if (!found) {
      fprintf(stderr,
              "REX_AST_INVARIANT[statement-attribute-list-owner]: "
              "attribute=%p has no exact structural list owner\n",
              static_cast<void *>(attribute));
      ROSE_ABORT();
    }
    attribute->validate();
  }

  if (SgOmpDeclareSimdStatement *stmt = isSgOmpDeclareSimdStatement(node)) {
    static_cast<void>(stmt->get_mangled_name());
  }
  if (SgOmpDeclareVariantStatement *stmt =
          isSgOmpDeclareVariantStatement(node)) {
    static_cast<void>(stmt->get_mangled_name());
  }
  if (isSgOmpTaskwaitStatement(node) != NULL ||
      isSgOmpEndAssumeStatement(node) != NULL) {
    if (isSgOmpExecStatement(node) == NULL ||
        isSgDeclarationStatement(node) != NULL) {
      fprintf(stderr,
              "REX_AST_INVARIANT[openmp-executable-classification]: "
              "statement=%p type=%s is not exclusively an OpenMP executable "
              "statement\n",
              static_cast<void *>(node), node->class_name().c_str());
      ROSE_ABORT();
    }
  }

  SgOmpClauseList *owned_clause_list = NULL;
  bool requires_clause_list = false;
  if (SgOmpClauseBodyStatement *stmt = isSgOmpClauseBodyStatement(node)) {
    owned_clause_list = stmt->get_clause_list();
    requires_clause_list = true;
  } else if (SgOmpClauseStatement *stmt = isSgOmpClauseStatement(node)) {
    owned_clause_list = stmt->get_clause_list();
    requires_clause_list = true;
  } else if (SgOmpGroupprivateStatement *stmt =
                 isSgOmpGroupprivateStatement(node)) {
    owned_clause_list = stmt->get_clause_list();
    requires_clause_list = true;
  }
  if (requires_clause_list && owned_clause_list == NULL) {
    fprintf(stderr,
            "REX_AST_INVARIANT[openmp-clause-list]: statement=%p type=%s has "
            "no structural clause-list wrapper\n",
            static_cast<void *>(node), node->class_name().c_str());
    ROSE_ABORT();
  }
  if (owned_clause_list != NULL) {
    if (owned_clause_list->get_parent() != node) {
      fprintf(stderr,
              "REX_AST_INVARIANT[openmp-clause-list]: statement=%p type=%s "
              "does not exclusively own its clause-list wrapper\n",
              static_cast<void *>(node), node->class_name().c_str());
      ROSE_ABORT();
    }
    (void)owned_clause_list->get_clauses();
  }

  if (SgOmpBodyStatement *outer = isSgOmpBodyStatement(node)) {
    const bool combined = outer->get_source_form_is_combined();
    SgOmpBodyStatement *combined_owner =
        isSgOmpBodyStatement(outer->get_parent());
    const bool nested_combined_component =
        combined_owner != NULL &&
        combined_owner->get_source_form_is_combined() &&
        combined_owner->get_body() == outer;

    if (combined) {
      SgOmpBodyStatement *inner = isSgOmpBodyStatement(outer->get_body());
      if (isSgOmpParallelStatement(outer) == NULL || inner == NULL ||
          inner->get_parent() != outer) {
        fprintf(stderr,
                "REX_AST_INVARIANT[omp-clause-source-order]: combined "
                "directive=%p has no exact nested semantic component\n",
                static_cast<void *>(outer));
        ROSE_ABORT();
      }

      std::vector<SgOmpClause *> combined_clauses;
      auto append_clauses = [&](SgStatement *statement) {
        SgOmpClauseList *list = NULL;
        if (SgOmpClauseBodyStatement *body =
                isSgOmpClauseBodyStatement(statement)) {
          list = body->get_clause_list();
        } else if (SgOmpClauseStatement *clause_statement =
                       isSgOmpClauseStatement(statement)) {
          list = clause_statement->get_clause_list();
        }
        if (list == NULL || list->get_parent() != statement) {
          fprintf(stderr,
                  "REX_AST_INVARIANT[omp-clause-source-order]: combined "
                  "component=%p has no exact clause-list owner\n",
                  static_cast<void *>(statement));
          ROSE_ABORT();
        }
        for (SgOmpClause *clause : list->get_clauses()) {
          if (clause == NULL || clause->get_parent() != list) {
            fprintf(stderr,
                    "REX_AST_INVARIANT[omp-clause-source-order]: combined "
                    "component=%p owns a null or misparented clause\n",
                    static_cast<void *>(statement));
            ROSE_ABORT();
          }
          combined_clauses.push_back(clause);
        }
      };
      append_clauses(outer);
      append_clauses(inner);

      std::vector<bool> seen(combined_clauses.size(), false);
      for (SgOmpClause *clause : combined_clauses) {
        const std::optional<std::size_t> &source_order =
            clause->get_combined_source_order();
        if (!source_order.has_value() ||
            *source_order >= combined_clauses.size() || seen[*source_order]) {
          fprintf(stderr,
                  "REX_AST_INVARIANT[omp-clause-source-order]: combined "
                  "directive=%p has absent, duplicate, or out-of-range "
                  "clause provenance\n",
                  static_cast<void *>(outer));
          ROSE_ABORT();
        }
        seen[*source_order] = true;
      }
    } else if (!nested_combined_component && owned_clause_list != NULL) {
      for (SgOmpClause *clause : owned_clause_list->get_clauses()) {
        if (clause != NULL && clause->get_combined_source_order().has_value()) {
          fprintf(stderr,
                  "REX_AST_INVARIANT[omp-clause-source-order]: non-combined "
                  "directive=%p owns combined clause provenance\n",
                  static_cast<void *>(outer));
          ROSE_ABORT();
        }
      }
    }
  }

  if (SgOmpClauseList *clause_list = isSgOmpClauseList(node)) {
    SgNode *owner = clause_list->get_parent();
    const bool valid_owner =
        (isSgOmpClauseBodyStatement(owner) != NULL &&
         isSgOmpClauseBodyStatement(owner)->get_clause_list() == clause_list) ||
        (isSgOmpClauseStatement(owner) != NULL &&
         isSgOmpClauseStatement(owner)->get_clause_list() == clause_list) ||
        (isSgOmpGroupprivateStatement(owner) != NULL &&
         isSgOmpGroupprivateStatement(owner)->get_clause_list() == clause_list);
    if (!valid_owner) {
      fprintf(stderr,
              "REX_AST_INVARIANT[openmp-clause-list]: list=%p has no exact "
              "structural statement owner\n",
              static_cast<void *>(clause_list));
      ROSE_ABORT();
    }
    (void)clause_list->get_clauses();
  }

  if (SgOmpClause *clause = isSgOmpClause(node)) {
    SgNode *parent = clause->get_parent();
    if (isSgOmpClauseBodyStatement(parent) != NULL ||
        isSgOmpClauseStatement(parent) != NULL ||
        isSgOmpGroupprivateStatement(parent) != NULL) {
      fprintf(stderr,
              "REX_AST_INVARIANT[openmp-clause-list]: clause=%p type=%s is "
              "parented directly to a wrapper-based statement\n",
              static_cast<void *>(clause), clause->class_name().c_str());
      ROSE_ABORT();
    }
    SgOmpClauseList *list = isSgOmpClauseList(parent);
    SgOmpBodyStatement *owner =
        list != NULL ? isSgOmpBodyStatement(list->get_parent()) : NULL;
    SgOmpBodyStatement *combined_owner =
        owner != NULL ? isSgOmpBodyStatement(owner->get_parent()) : NULL;
    const bool combined_participant =
        owner != NULL && (owner->get_source_form_is_combined() ||
                          (combined_owner != NULL &&
                           combined_owner->get_source_form_is_combined() &&
                           combined_owner->get_body() == owner));
    if (!combined_participant &&
        clause->get_combined_source_order().has_value()) {
      fprintf(stderr,
              "REX_AST_INVARIANT[omp-clause-source-order]: non-combined "
              "clause=%p owns combined source-order provenance\n",
              static_cast<void *>(clause));
      ROSE_ABORT();
    }
  }

  auto validate_omp_iterator_definitions =
      [](SgOmpClause *owner, const SgOmpIteratorDefinitionPtrList &definitions,
         bool required) {
        if (owner == NULL || required != !definitions.empty()) {
          fprintf(stderr,
                  "REX_AST_INVARIANT[openmp-iterator-definitions]: "
                  "clause=%p required=%d has invalid definition cardinality\n",
                  static_cast<void *>(owner), required);
          ROSE_ABORT();
        }
        for (SgOmpIteratorDefinition *definition : definitions) {
          if (definition == NULL || definition->get_parent() != owner ||
              std::count(definitions.begin(), definitions.end(), definition) !=
                  1 ||
              definition->get_iterator_name() == NULL ||
              definition->get_iterator_name()->get_spelling().empty() ||
              definition->get_begin() == NULL ||
              definition->get_end() == NULL ||
              (definition->get_iterator_type() != NULL &&
               definition->get_iterator_type()->get_parent() != definition) ||
              definition->get_iterator_name()->get_parent() != definition ||
              definition->get_begin()->get_parent() != definition ||
              definition->get_end()->get_parent() != definition ||
              (definition->get_step() != NULL &&
               definition->get_step()->get_parent() != definition)) {
            fprintf(stderr,
                    "REX_AST_INVARIANT[openmp-iterator-definitions]: "
                    "clause=%p has a malformed or multiply owned typed "
                    "iterator definition\n",
                    static_cast<void *>(owner));
            ROSE_ABORT();
          }
          std::set<SgExpression *> fields = {definition->get_iterator_name(),
                                             definition->get_begin(),
                                             definition->get_end()};
          const size_t required_field_count =
              3 + (definition->get_iterator_type() != NULL) +
              (definition->get_step() != NULL);
          if (definition->get_iterator_type() != NULL) {
            fields.insert(definition->get_iterator_type());
          }
          if (definition->get_step() != NULL) {
            fields.insert(definition->get_step());
          }
          if (fields.size() != required_field_count) {
            fprintf(stderr,
                    "REX_AST_INVARIANT[openmp-iterator-definitions]: "
                    "clause=%p aliases one syntax node across iterator "
                    "roles\n",
                    static_cast<void *>(owner));
            ROSE_ABORT();
          }
        }
      };

  if (SgOmpReductionClause *clause = isSgOmpReductionClause(node)) {
    const bool requires_user_identifier =
        clause->get_identifier() ==
        SgOmpClause::e_omp_reduction_user_defined_identifier;
    SgOmpNameExpression *identifier = clause->get_user_defined_identifier();
    if (requires_user_identifier != (identifier != NULL) ||
        (identifier != NULL && (identifier->get_spelling().empty() ||
                                identifier->get_parent() != clause))) {
      fprintf(stderr,
              "REX_AST_INVARIANT[openmp-reduction-identifier]: clause=%p "
              "kind and exact user identifier ownership disagree\n",
              static_cast<void *>(clause));
      ROSE_ABORT();
    }
  }
  if (SgOmpInReductionClause *clause = isSgOmpInReductionClause(node)) {
    const bool requires_user_identifier =
        clause->get_identifier() ==
        SgOmpClause::e_omp_in_reduction_user_defined_identifier;
    SgOmpNameExpression *identifier = clause->get_user_defined_identifier();
    if (requires_user_identifier != (identifier != NULL) ||
        (identifier != NULL && (identifier->get_spelling().empty() ||
                                identifier->get_parent() != clause))) {
      fprintf(stderr,
              "REX_AST_INVARIANT[openmp-in-reduction-identifier]: clause=%p "
              "kind and exact user identifier ownership disagree\n",
              static_cast<void *>(clause));
      ROSE_ABORT();
    }
  }
  if (SgOmpTaskReductionClause *clause = isSgOmpTaskReductionClause(node)) {
    const bool requires_user_identifier =
        clause->get_identifier() ==
        SgOmpClause::e_omp_task_reduction_user_defined_identifier;
    SgOmpNameExpression *identifier = clause->get_user_defined_identifier();
    if (requires_user_identifier != (identifier != NULL) ||
        (identifier != NULL && (identifier->get_spelling().empty() ||
                                identifier->get_parent() != clause))) {
      fprintf(stderr,
              "REX_AST_INVARIANT[openmp-task-reduction-identifier]: "
              "clause=%p kind and exact user identifier ownership disagree\n",
              static_cast<void *>(clause));
      ROSE_ABORT();
    }
  }
  if (SgOmpMapClause *clause = isSgOmpMapClause(node)) {
    switch (clause->get_operation()) {
    case SgOmpClause::e_omp_map_unknown:
    case SgOmpClause::e_omp_map_alloc:
    case SgOmpClause::e_omp_map_to:
    case SgOmpClause::e_omp_map_from:
    case SgOmpClause::e_omp_map_tofrom:
    case SgOmpClause::e_omp_map_storage:
    case SgOmpClause::e_omp_map_release:
    case SgOmpClause::e_omp_map_delete:
    case SgOmpClause::e_omp_map_present:
    case SgOmpClause::e_omp_map_self:
      break;
    default:
      fprintf(stderr,
              "REX_AST_INVARIANT[openmp-map-operation]: clause=%p has an "
              "invalid typed operation\n",
              static_cast<void *>(clause));
      ROSE_ABORT();
    }
    const SgOmpClause::omp_map_modifier_enum modifiers[] = {
        clause->get_modifier1(), clause->get_modifier2(),
        clause->get_modifier3()};
    bool saw_unspecified_modifier = false;
    std::set<int> unique_modifiers;
    size_t mapper_modifier_count = 0;
    size_t iterator_modifier_count = 0;
    for (SgOmpClause::omp_map_modifier_enum modifier : modifiers) {
      switch (modifier) {
      case SgOmpClause::e_omp_map_modifier_unspecified:
        saw_unspecified_modifier = true;
        continue;
      case SgOmpClause::e_omp_map_modifier_always:
      case SgOmpClause::e_omp_map_modifier_close:
      case SgOmpClause::e_omp_map_modifier_present:
      case SgOmpClause::e_omp_map_modifier_self:
      case SgOmpClause::e_omp_map_modifier_mapper:
      case SgOmpClause::e_omp_map_modifier_iterator:
        break;
      default:
        fprintf(stderr,
                "REX_AST_INVARIANT[openmp-map-modifier]: clause=%p has an "
                "invalid typed modifier\n",
                static_cast<void *>(clause));
        ROSE_ABORT();
      }
      if (saw_unspecified_modifier ||
          !unique_modifiers.insert(static_cast<int>(modifier)).second) {
        fprintf(stderr,
                "REX_AST_INVARIANT[openmp-map-modifier]: clause=%p has a "
                "non-contiguous or duplicate modifier\n",
                static_cast<void *>(clause));
        ROSE_ABORT();
      }
      mapper_modifier_count +=
          modifier == SgOmpClause::e_omp_map_modifier_mapper ? 1 : 0;
      iterator_modifier_count +=
          modifier == SgOmpClause::e_omp_map_modifier_iterator ? 1 : 0;
    }
    SgOmpNameExpression *mapper_identifier = clause->get_mapper_identifier();
    if (mapper_modifier_count > 1 || iterator_modifier_count > 1 ||
        (mapper_modifier_count == 1) != (mapper_identifier != NULL) ||
        (mapper_identifier != NULL &&
         (mapper_identifier->get_spelling().empty() ||
          mapper_identifier->get_parent() != clause))) {
      fprintf(stderr,
              "REX_AST_INVARIANT[openmp-map-payload]: clause=%p has "
              "invalid mapper or iterator discriminator state\n",
              static_cast<void *>(clause));
      ROSE_ABORT();
    }
    validate_omp_iterator_definitions(clause,
                                      clause->get_iterator_definitions(),
                                      iterator_modifier_count == 1);
  }
  if (SgOmpDependClause *clause = isSgOmpDependClause(node)) {
    const bool requires_iterator = clause->get_depend_modifier() ==
                                   SgOmpClause::e_omp_depend_modifier_iterator;
    if (clause->get_depend_modifier() !=
            SgOmpClause::e_omp_depend_modifier_unspecified &&
        !requires_iterator) {
      fprintf(stderr,
              "REX_AST_INVARIANT[openmp-depend-iterator]: clause=%p has an "
              "invalid typed modifier\n",
              static_cast<void *>(clause));
      ROSE_ABORT();
    }
    switch (clause->get_dependence_type()) {
    case SgOmpClause::e_omp_depend_in:
    case SgOmpClause::e_omp_depend_out:
    case SgOmpClause::e_omp_depend_inout:
    case SgOmpClause::e_omp_depend_inoutset:
    case SgOmpClause::e_omp_depend_mutexinoutset:
    case SgOmpClause::e_omp_depend_depobj:
    case SgOmpClause::e_omp_depend_source:
    case SgOmpClause::e_omp_depend_sink:
      break;
    default:
      fprintf(stderr,
              "REX_AST_INVARIANT[openmp-depend-type]: clause=%p has an "
              "invalid typed dependence kind\n",
              static_cast<void *>(clause));
      ROSE_ABORT();
    }
    validate_omp_iterator_definitions(
        clause, clause->get_iterator_definitions(), requires_iterator);
    const bool requires_sink_vectors =
        clause->get_dependence_type() == SgOmpClause::e_omp_depend_sink;
    SgExprListExp *sink_vectors = clause->get_sink_vectors();
    if (requires_sink_vectors != (sink_vectors != NULL) ||
        (sink_vectors != NULL && (sink_vectors->get_parent() != clause ||
                                  sink_vectors->get_expressions().empty()))) {
      fprintf(stderr,
              "REX_AST_INVARIANT[openmp-depend-sink]: clause=%p kind and "
              "nonempty exact sink-vector ownership disagree\n",
              static_cast<void *>(clause));
      ROSE_ABORT();
    }
    if (sink_vectors != NULL) {
      const SgExpressionPtrList &vectors = sink_vectors->get_expressions();
      for (SgExpression *vector : vectors) {
        if (vector == NULL || vector->get_parent() != sink_vectors ||
            std::count(vectors.begin(), vectors.end(), vector) != 1) {
          fprintf(stderr,
                  "REX_AST_INVARIANT[openmp-depend-sink]: clause=%p has a "
                  "null, aliased, or incorrectly owned sink vector\n",
                  static_cast<void *>(clause));
          ROSE_ABORT();
        }
      }
    }
  }
  if (SgOmpAffinityClause *clause = isSgOmpAffinityClause(node)) {
    const bool requires_iterator =
        clause->get_affinity_modifier() ==
        SgOmpClause::e_omp_affinity_modifier_iterator;
    if (clause->get_affinity_modifier() !=
            SgOmpClause::e_omp_affinity_modifier_unspecified &&
        !requires_iterator) {
      fprintf(stderr,
              "REX_AST_INVARIANT[openmp-affinity-iterator]: clause=%p has "
              "an invalid typed modifier\n",
              static_cast<void *>(clause));
      ROSE_ABORT();
    }
    validate_omp_iterator_definitions(
        clause, clause->get_iterator_definitions(), requires_iterator);
  }
  if (SgOmpToClause *clause = isSgOmpToClause(node)) {
    if (clause->get_kind() != SgOmpClause::e_omp_to_kind_unknown &&
        clause->get_kind() != SgOmpClause::e_omp_to_kind_mapper &&
        clause->get_kind() != SgOmpClause::e_omp_to_kind_iterator &&
        clause->get_kind() != SgOmpClause::e_omp_to_kind_present) {
      fprintf(stderr,
              "REX_AST_INVARIANT[openmp-to-kind]: clause=%p has an invalid "
              "typed kind\n",
              static_cast<void *>(clause));
      ROSE_ABORT();
    }
    const bool requires_mapper =
        clause->get_kind() == SgOmpClause::e_omp_to_kind_mapper;
    SgOmpNameExpression *mapper_identifier = clause->get_mapper_identifier();
    if (requires_mapper != (mapper_identifier != NULL) ||
        (mapper_identifier != NULL &&
         (mapper_identifier->get_spelling().empty() ||
          mapper_identifier->get_parent() != clause)) ||
        (clause->get_declare_target_extended_list() &&
         (clause->get_kind() != SgOmpClause::e_omp_to_kind_unknown ||
          mapper_identifier != NULL ||
          !clause->get_iterator_definitions().empty()))) {
      fprintf(stderr,
              "REX_AST_INVARIANT[openmp-to-payload]: clause=%p has "
              "incompatible typed payloads\n",
              static_cast<void *>(clause));
      ROSE_ABORT();
    }
    validate_omp_iterator_definitions(
        clause, clause->get_iterator_definitions(),
        clause->get_kind() == SgOmpClause::e_omp_to_kind_iterator);
  }
  if (SgOmpFromClause *clause = isSgOmpFromClause(node)) {
    if (clause->get_kind() != SgOmpClause::e_omp_from_kind_unknown &&
        clause->get_kind() != SgOmpClause::e_omp_from_kind_mapper &&
        clause->get_kind() != SgOmpClause::e_omp_from_kind_iterator &&
        clause->get_kind() != SgOmpClause::e_omp_from_kind_present) {
      fprintf(stderr,
              "REX_AST_INVARIANT[openmp-from-kind]: clause=%p has an invalid "
              "typed kind\n",
              static_cast<void *>(clause));
      ROSE_ABORT();
    }
    const bool requires_mapper =
        clause->get_kind() == SgOmpClause::e_omp_from_kind_mapper;
    SgOmpNameExpression *mapper_identifier = clause->get_mapper_identifier();
    if (requires_mapper != (mapper_identifier != NULL) ||
        (mapper_identifier != NULL &&
         (mapper_identifier->get_spelling().empty() ||
          mapper_identifier->get_parent() != clause))) {
      fprintf(stderr,
              "REX_AST_INVARIANT[openmp-from-payload]: clause=%p has "
              "incompatible typed payloads\n",
              static_cast<void *>(clause));
      ROSE_ABORT();
    }
    validate_omp_iterator_definitions(
        clause, clause->get_iterator_definitions(),
        clause->get_kind() == SgOmpClause::e_omp_from_kind_iterator);
  }

  if (SgOmpFlushStatement *stmt = isSgOmpFlushStatement(node)) {
    if (stmt->get_variable_list() == NULL ||
        stmt->get_variable_list()->get_parent() != stmt) {
      fprintf(stderr,
              "REX_AST_INVARIANT[openmp-flush-variable-list]: statement=%p "
              "has invalid structural variable-list ownership\n",
              static_cast<void *>(stmt));
      ROSE_ABORT();
    }
    (void)stmt->get_variables();
  }
  if (SgOmpAllocateStatement *stmt = isSgOmpAllocateStatement(node)) {
    if (stmt->get_variable_list() == NULL ||
        stmt->get_variable_list()->get_parent() != stmt) {
      fprintf(stderr,
              "REX_AST_INVARIANT[openmp-allocate-variable-list]: statement=%p "
              "has invalid structural variable-list ownership\n",
              static_cast<void *>(stmt));
      ROSE_ABORT();
    }
    (void)stmt->get_variables();
  }

  if (SgOmpAllocateClause *clause = isSgOmpAllocateClause(node)) {
    SgExpression *user_defined_modifier = clause->get_user_defined_modifier();
    SgExpression *alignment = clause->get_alignment();
    const bool requires_user_defined_modifier =
        clause->get_modifier() ==
        SgOmpClause::e_omp_allocate_user_defined_modifier;
    if (requires_user_defined_modifier != (user_defined_modifier != NULL) ||
        (user_defined_modifier != NULL &&
         user_defined_modifier->get_parent() != clause) ||
        (alignment != NULL && alignment->get_parent() != clause) ||
        (clause->get_uses_allocator_modifier_syntax() &&
         clause->get_modifier() ==
             SgOmpClause::e_omp_allocate_modifier_unknown) ||
        (alignment != NULL &&
         clause->get_modifier() !=
             SgOmpClause::e_omp_allocate_modifier_unknown &&
         !clause->get_uses_allocator_modifier_syntax())) {
      fprintf(stderr,
              "REX_AST_INVARIANT[openmp-allocate-modifier]: clause=%p "
              "modifier=%d has invalid singular modifier ownership\n",
              static_cast<void *>(clause),
              static_cast<int>(clause->get_modifier()));
      ROSE_ABORT();
    }
  }

  if (SgOmpInductionClause *clause = isSgOmpInductionClause(node)) {
    for (SgOmpInductionItem *item : clause->get_items()) {
      if (item == NULL || item->get_parent() != clause) {
        fprintf(stderr,
                "REX_AST_INVARIANT[openmp-induction-items]: clause=%p has "
                "a null item or an item with the wrong parent\n",
                static_cast<void *>(clause));
        ROSE_ABORT();
      }
    }
  }
  if (SgOmpApplyClause *clause = isSgOmpApplyClause(node)) {
    if (clause->get_transformations().empty() && clause->get_label().empty()) {
      fprintf(stderr,
              "REX_AST_INVARIANT[openmp-apply-transformations]: clause=%p "
              "has neither a label nor a transformation\n",
              static_cast<void *>(clause));
      ROSE_ABORT();
    }
    for (SgOmpApplyTransformation *item : clause->get_transformations()) {
      if (item == NULL || item->get_parent() != clause) {
        fprintf(stderr,
                "REX_AST_INVARIANT[openmp-apply-transformations]: clause=%p "
                "has a null transformation or one with the wrong parent\n",
                static_cast<void *>(clause));
        ROSE_ABORT();
      }
    }
  }
  if (SgOmpInitClause *clause = isSgOmpInitClause(node)) {
    std::string detail;
    if (!Rose::OpenMP::Detail::validateInitClause(clause, &detail)) {
      fprintf(stderr,
              "REX_AST_INVARIANT[openmp-init]: clause=%p is malformed: %s\n",
              static_cast<void *>(clause), detail.c_str());
      ROSE_ABORT();
    }
  }
  if (SgOmpAdjustArgsClause *clause = isSgOmpAdjustArgsClause(node)) {
    std::string detail;
    if (!Rose::OpenMP::Detail::validateAdjustArgsClause(clause, &detail)) {
      fprintf(stderr,
              "REX_AST_INVARIANT[openmp-adjust-args]: clause=%p is "
              "malformed: %s\n",
              static_cast<void *>(clause), detail.c_str());
      ROSE_ABORT();
    }
  }
  if (SgOmpAppendArgsClause *clause = isSgOmpAppendArgsClause(node)) {
    std::string detail;
    if (!Rose::OpenMP::Detail::validateAppendArgsClause(clause, &detail)) {
      fprintf(stderr,
              "REX_AST_INVARIANT[openmp-append-args]: clause=%p is "
              "malformed: %s\n",
              static_cast<void *>(clause), detail.c_str());
      ROSE_ABORT();
    }
  }
  if (SgOmpVariablesClause *clause = isSgOmpVariablesClause(node)) {
    if (clause->get_has_source_variables_override() !=
        (clause->get_source_variables() != NULL)) {
      fprintf(stderr,
              "REX_AST_INVARIANT[openmp-source-variable-override]: "
              "clause=%p discriminator and owned source list disagree\n",
              static_cast<void *>(clause));
      ROSE_ABORT();
    }
    if (isSgOmpMapClause(clause) != NULL) {
      SgExprListExp *variables = clause->get_variables();
      if (variables == NULL || variables->get_parent() != clause ||
          variables->get_expressions().empty()) {
        fprintf(stderr,
                "REX_AST_INVARIANT[openmp-map-items]: clause=%p has no "
                "nonempty exactly owned variable list\n",
                static_cast<void *>(clause));
        ROSE_ABORT();
      }
      for (SgExpression *expression : variables->get_expressions()) {
        SgOmpMapItem *item = isSgOmpMapItem(expression);
        if (item == NULL || item->get_parent() != variables ||
            item->get_expression() == NULL ||
            item->get_expression()->get_parent() != item) {
          fprintf(stderr,
                  "REX_AST_INVARIANT[openmp-map-items]: clause=%p has a "
                  "non-wrapper item or invalid locator ownership\n",
                  static_cast<void *>(clause));
          ROSE_ABORT();
        }
        for (SgOmpMapDistDataPolicy *policy : item->get_policies()) {
          if (policy == NULL || policy->get_parent() != item) {
            fprintf(stderr,
                    "REX_AST_INVARIANT[openmp-map-policies]: item=%p has a "
                    "null policy or invalid policy ownership\n",
                    static_cast<void *>(item));
            ROSE_ABORT();
          }
        }
      }
    }
  }

  if (SgOmpIteratorDefinition *definition = isSgOmpIteratorDefinition(node)) {
    SgOmpClause *owner = isSgOmpClause(definition->get_parent());
    const SgOmpIteratorDefinitionPtrList *definitions = NULL;
    if (SgOmpMapClause *clause = isSgOmpMapClause(owner)) {
      definitions = &clause->get_iterator_definitions();
    } else if (SgOmpDependClause *clause = isSgOmpDependClause(owner)) {
      definitions = &clause->get_iterator_definitions();
    } else if (SgOmpAffinityClause *clause = isSgOmpAffinityClause(owner)) {
      definitions = &clause->get_iterator_definitions();
    } else if (SgOmpToClause *clause = isSgOmpToClause(owner)) {
      definitions = &clause->get_iterator_definitions();
    } else if (SgOmpFromClause *clause = isSgOmpFromClause(owner)) {
      definitions = &clause->get_iterator_definitions();
    }
    if (definitions == NULL ||
        std::count(definitions->begin(), definitions->end(), definition) != 1 ||
        definition->get_iterator_name() == NULL ||
        definition->get_begin() == NULL || definition->get_end() == NULL ||
        definition->get_iterator_name()->get_spelling().empty() ||
        (definition->get_iterator_type() != NULL &&
         definition->get_iterator_type()->get_parent() != definition) ||
        definition->get_iterator_name()->get_parent() != definition ||
        definition->get_begin()->get_parent() != definition ||
        definition->get_end()->get_parent() != definition ||
        (definition->get_step() != NULL &&
         definition->get_step()->get_parent() != definition)) {
      fprintf(stderr,
              "REX_AST_INVARIANT[openmp-iterator-definition]: "
              "definition=%p has invalid clause membership or child "
              "ownership\n",
              static_cast<void *>(definition));
      ROSE_ABORT();
    }
    std::set<SgExpression *> fields = {definition->get_iterator_name(),
                                       definition->get_begin(),
                                       definition->get_end()};
    const size_t required_field_count =
        3 + (definition->get_iterator_type() != NULL) +
        (definition->get_step() != NULL);
    if (definition->get_iterator_type() != NULL) {
      fields.insert(definition->get_iterator_type());
    }
    if (definition->get_step() != NULL) {
      fields.insert(definition->get_step());
    }
    if (fields.size() != required_field_count) {
      fprintf(stderr,
              "REX_AST_INVARIANT[openmp-iterator-definition]: "
              "definition=%p aliases one syntax node across iterator roles\n",
              static_cast<void *>(definition));
      ROSE_ABORT();
    }
  }

  SgExpression *expression = isSgExpression(node);

  if (SgOmpMapItem *item = isSgOmpMapItem(expression)) {
    SgExprListExp *variables = isSgExprListExp(item->get_parent());
    SgOmpMapClause *clause =
        variables != NULL ? isSgOmpMapClause(variables->get_parent()) : NULL;
    if (clause == NULL || clause->get_variables() != variables ||
        std::find(variables->get_expressions().begin(),
                  variables->get_expressions().end(),
                  item) == variables->get_expressions().end() ||
        item->get_expression() == NULL ||
        item->get_expression()->get_parent() != item) {
      fprintf(stderr,
              "REX_AST_INVARIANT[openmp-map-item]: item=%p has invalid map "
              "clause, list, or locator ownership\n",
              static_cast<void *>(item));
      ROSE_ABORT();
    }
    for (SgOmpMapDistDataPolicy *policy : item->get_policies()) {
      if (policy == NULL || policy->get_parent() != item) {
        fprintf(stderr,
                "REX_AST_INVARIANT[openmp-map-item]: item=%p has a null or "
                "incorrectly owned policy\n",
                static_cast<void *>(item));
        ROSE_ABORT();
      }
    }
    return;
  }

  if (SgOmpMapDistDataPolicy *policy = isSgOmpMapDistDataPolicy(expression)) {
    SgOmpMapItem *item = isSgOmpMapItem(policy->get_parent());
    const bool listed =
        item != NULL &&
        std::find(item->get_policies().begin(), item->get_policies().end(),
                  policy) != item->get_policies().end();
    SgExpression *argument = policy->get_expression();
    bool valid_payload = false;
    switch (policy->get_policy()) {
    case SgOmpClause::e_omp_map_dist_data_duplicate:
      valid_payload = argument == NULL;
      break;
    case SgOmpClause::e_omp_map_dist_data_block:
    case SgOmpClause::e_omp_map_dist_data_cyclic:
      valid_payload = argument == NULL || argument->get_parent() == policy;
      break;
    default:
      valid_payload = false;
      break;
    }
    if (!listed || !valid_payload) {
      fprintf(stderr,
              "REX_AST_INVARIANT[openmp-map-policy]: policy=%p has invalid "
              "item membership, kind, payload, or ownership\n",
              static_cast<void *>(policy));
      ROSE_ABORT();
    }
    return;
  }

  if (SgOmpInitModifierList *modifier_list =
          isSgOmpInitModifierList(expression)) {
    SgOmpInitClause *clause = isSgOmpInitClause(modifier_list->get_parent());
    SgOmpAppendArgsOperation *operation =
        isSgOmpAppendArgsOperation(modifier_list->get_parent());
    const bool owned_by_init =
        clause != NULL && clause->get_modifier_list() == modifier_list;
    const bool owned_by_append =
        operation != NULL && operation->get_modifier_list() == modifier_list;
    if (owned_by_init == owned_by_append) {
      fprintf(stderr,
              "REX_AST_INVARIANT[openmp-init-modifier-list]: list=%p has "
              "invalid init or append_args ownership\n",
              static_cast<void *>(modifier_list));
      ROSE_ABORT();
    }
    for (SgOmpInitModifier *modifier : modifier_list->get_modifiers()) {
      if (modifier == NULL || modifier->get_parent() != modifier_list) {
        fprintf(stderr,
                "REX_AST_INVARIANT[openmp-init-modifier-list]: list=%p has "
                "a null modifier or invalid modifier ownership\n",
                static_cast<void *>(modifier_list));
        ROSE_ABORT();
      }
    }
    return;
  }

  if (SgOmpAppendArgsOperation *operation =
          isSgOmpAppendArgsOperation(expression)) {
    SgOmpAppendArgsClause *clause =
        isSgOmpAppendArgsClause(operation->get_parent());
    if (clause == NULL ||
        std::count(clause->get_interop_operations().begin(),
                   clause->get_interop_operations().end(), operation) != 1 ||
        operation->get_modifier_list() == NULL ||
        operation->get_modifier_list()->get_parent() != operation) {
      fprintf(stderr,
              "REX_AST_INVARIANT[openmp-append-args-operation]: operation=%p "
              "has invalid clause membership or modifier-list ownership\n",
              static_cast<void *>(operation));
      ROSE_ABORT();
    }
    return;
  }

  if (SgOmpInductionItem *item = isSgOmpInductionItem(expression)) {
    SgOmpInductionClause *clause = isSgOmpInductionClause(item->get_parent());
    if (clause == NULL ||
        std::find(clause->get_items().begin(), clause->get_items().end(),
                  item) == clause->get_items().end() ||
        item->get_expression() == NULL ||
        item->get_expression()->get_parent() != item) {
      fprintf(stderr,
              "REX_AST_INVARIANT[openmp-induction-item]: item=%p has invalid "
              "clause ownership or semantic expression ownership\n",
              static_cast<void *>(item));
      ROSE_ABORT();
    }
    const bool has_label = !item->get_label().empty();
    if ((item->get_kind() == SgOmpClause::e_omp_induction_item_binding) !=
            has_label ||
        (item->get_kind() != SgOmpClause::e_omp_induction_item_step &&
         item->get_kind() != SgOmpClause::e_omp_induction_item_binding &&
         item->get_kind() != SgOmpClause::e_omp_induction_item_expression)) {
      fprintf(stderr,
              "REX_AST_INVARIANT[openmp-induction-item]: item=%p has an "
              "invalid kind/label combination\n",
              static_cast<void *>(item));
      ROSE_ABORT();
    }
    return;
  }

  if (SgOmpApplyTransformation *item = isSgOmpApplyTransformation(expression)) {
    SgOmpApplyClause *clause = isSgOmpApplyClause(item->get_parent());
    SgOmpApplyTransformationPtrList::const_iterator position;
    if (clause != NULL) {
      position = std::find(clause->get_transformations().begin(),
                           clause->get_transformations().end(), item);
    }
    if (clause == NULL || position == clause->get_transformations().end() ||
        (item->get_argument() != NULL &&
         item->get_argument()->get_parent() != item) ||
        (item->get_nested_apply() != NULL &&
         item->get_nested_apply()->get_parent() != item) ||
        (position == clause->get_transformations().begin()
             ? item->get_separator() != SgOmpClause::e_omp_clause_separator_none
             : item->get_separator() !=
                       SgOmpClause::e_omp_clause_separator_comma &&
                   item->get_separator() !=
                       SgOmpClause::e_omp_clause_separator_space)) {
      fprintf(stderr,
              "REX_AST_INVARIANT[openmp-apply-transformation]: item=%p has "
              "invalid ownership or separator\n",
              static_cast<void *>(item));
      ROSE_ABORT();
    }
    const bool plain =
        item->get_kind() == SgOmpClause::e_omp_apply_transform_unroll ||
        item->get_kind() == SgOmpClause::e_omp_apply_transform_unroll_full ||
        item->get_kind() == SgOmpClause::e_omp_apply_transform_reverse ||
        item->get_kind() == SgOmpClause::e_omp_apply_transform_interchange ||
        item->get_kind() == SgOmpClause::e_omp_apply_transform_nothing;
    const bool argument =
        item->get_kind() == SgOmpClause::e_omp_apply_transform_unroll_partial ||
        item->get_kind() == SgOmpClause::e_omp_apply_transform_tile_sizes;
    const bool nested =
        item->get_kind() == SgOmpClause::e_omp_apply_transform_nested_apply;
    const bool named =
        item->get_kind() == SgOmpClause::e_omp_apply_transform_named;
    if ((!plain && !argument && !nested && !named) ||
        (plain &&
         (!item->get_transformation_name().empty() ||
          item->get_argument() != NULL || item->get_nested_apply() != NULL)) ||
        (argument &&
         (!item->get_transformation_name().empty() ||
          item->get_argument() == NULL || item->get_nested_apply() != NULL)) ||
        (nested &&
         (!item->get_transformation_name().empty() ||
          item->get_argument() != NULL || item->get_nested_apply() == NULL)) ||
        (named &&
         (item->get_transformation_name().empty() ||
          item->get_argument() != NULL || item->get_nested_apply() != NULL))) {
      fprintf(stderr,
              "REX_AST_INVARIANT[openmp-apply-transformation]: item=%p has "
              "an invalid kind/payload combination\n",
              static_cast<void *>(item));
      ROSE_ABORT();
    }
    return;
  }

  if (SgOmpInitModifier *modifier = isSgOmpInitModifier(expression)) {
    SgOmpInitModifierList *modifier_list =
        isSgOmpInitModifierList(modifier->get_parent());
    SgOmpInitClause *clause =
        modifier_list != NULL ? isSgOmpInitClause(modifier_list->get_parent())
                              : NULL;
    SgOmpAppendArgsOperation *operation =
        modifier_list != NULL
            ? isSgOmpAppendArgsOperation(modifier_list->get_parent())
            : NULL;
    const bool owned_by_init =
        clause != NULL && clause->get_modifier_list() == modifier_list;
    const bool owned_by_append =
        operation != NULL && operation->get_modifier_list() == modifier_list;
    if (modifier_list == NULL || owned_by_init == owned_by_append ||
        std::find(modifier_list->get_modifiers().begin(),
                  modifier_list->get_modifiers().end(),
                  modifier) == modifier_list->get_modifiers().end() ||
        std::count(modifier_list->get_modifiers().begin(),
                   modifier_list->get_modifiers().end(), modifier) != 1 ||
        (modifier->get_expression() != NULL &&
         modifier->get_expression()->get_parent() != modifier)) {
      fprintf(stderr,
              "REX_AST_INVARIANT[openmp-init-modifier]: modifier=%p has "
              "invalid init or append_args membership or expression "
              "ownership\n",
              static_cast<void *>(modifier));
      ROSE_ABORT();
    }
    const bool expression_modifier =
        modifier->get_kind() == SgOmpClause::e_omp_init_modifier_prefer_type ||
        modifier->get_kind() == SgOmpClause::e_omp_init_modifier_depinfo_in ||
        modifier->get_kind() == SgOmpClause::e_omp_init_modifier_depinfo_out ||
        modifier->get_kind() ==
            SgOmpClause::e_omp_init_modifier_depinfo_inout ||
        modifier->get_kind() ==
            SgOmpClause::e_omp_init_modifier_depinfo_inoutset ||
        modifier->get_kind() ==
            SgOmpClause::e_omp_init_modifier_depinfo_mutexinoutset;
    const bool plain =
        modifier->get_kind() == SgOmpClause::e_omp_init_modifier_depobj ||
        modifier->get_kind() == SgOmpClause::e_omp_init_modifier_interop ||
        modifier->get_kind() == SgOmpClause::e_omp_init_modifier_target ||
        modifier->get_kind() == SgOmpClause::e_omp_init_modifier_targetsync;
    if ((!expression_modifier && !plain) ||
        (expression_modifier && modifier->get_expression() == NULL) ||
        (plain && modifier->get_expression() != NULL)) {
      fprintf(stderr,
              "REX_AST_INVARIANT[openmp-init-modifier]: modifier=%p has an "
              "invalid kind/payload combination\n",
              static_cast<void *>(modifier));
      ROSE_ABORT();
    }
    return;
  }

  if (expression != NULL) {
    SgOmpVariablesClause *clause =
        isSgOmpVariablesClause(expression->get_parent());
    size_t registeredDirectEdgeCount = 0;
    if (clause != NULL) {
      const std::vector<SgNode *> successors =
          clause->get_traversalSuccessorContainer();
      registeredDirectEdgeCount = static_cast<size_t>(
          std::count(successors.begin(), successors.end(), expression));
      if (registeredDirectEdgeCount > 1) {
        fprintf(stderr,
                "REX_AST_INVARIANT[openmp-direct-expression-owner]: "
                "expression=%p type=%s occurs %zu times as a direct "
                "structural child of clause=%p type=%s\n",
                static_cast<void *>(expression),
                expression->class_name().c_str(), registeredDirectEdgeCount,
                static_cast<void *>(clause), clause->class_name().c_str());
        ROSE_ABORT();
      }
    }
    SgOmpAllocateClause *allocate_clause = isSgOmpAllocateClause(clause);
    const bool registered_allocate_modifier =
        allocate_clause != NULL &&
        allocate_clause->get_user_defined_modifier() == expression;
    const bool registered_allocate_alignment =
        allocate_clause != NULL &&
        allocate_clause->get_alignment() == expression;
    if (registered_allocate_modifier) {
      if (allocate_clause->get_modifier() !=
          SgOmpClause::e_omp_allocate_user_defined_modifier) {
        fprintf(stderr,
                "REX_AST_INVARIANT[openmp-allocate-modifier]: clause=%p "
                "owns a user-defined modifier expression with modifier "
                "kind=%d\n",
                static_cast<void *>(allocate_clause),
                static_cast<int>(allocate_clause->get_modifier()));
        ROSE_ABORT();
      }
    } else if (registered_allocate_alignment) {
      if (expression->get_parent() != allocate_clause) {
        fprintf(stderr,
                "REX_AST_INVARIANT[openmp-allocate-alignment]: clause=%p "
                "has invalid alignment ownership\n",
                static_cast<void *>(allocate_clause));
        ROSE_ABORT();
      }
    } else if (clause != NULL && registeredDirectEdgeCount != 1 &&
               (isSgExprListExp(expression) == NULL ||
                (clause->get_variables() != expression &&
                 clause->get_source_variables() != expression))) {
      fprintf(stderr,
              "REX_AST_INVARIANT[openmp-variable-list-owner]: expression=%p "
              "type=%s is an unregistered direct child of clause=%p type=%s "
              "instead of a typed direct field or a member of one of its "
              "SgExprListExp wrappers\n",
              static_cast<void *>(expression), expression->class_name().c_str(),
              static_cast<void *>(clause), clause->class_name().c_str());
      ROSE_ABORT();
    }
  }
  if (expression != NULL) {
    SgExprListExp *list = isSgExprListExp(expression->get_parent());
    if (list != NULL && isSgOmpVariablesClause(list->get_parent()) != NULL &&
        std::find(list->get_expressions().begin(),
                  list->get_expressions().end(),
                  expression) == list->get_expressions().end()) {
      fprintf(stderr,
              "REX_AST_INVARIANT[openmp-variable-list-membership]: "
              "expression=%p type=%s names an OpenMP variable-list parent "
              "but is absent from that list\n",
              static_cast<void *>(expression),
              expression->class_name().c_str());
      ROSE_ABORT();
    }
  }

  // A common-block designator is syntax naming a Fortran storage entity in a
  // directive clause, not a value expression. Validate its typed semantic edge
  // and ownership without asking it for a fabricated value type.
  if (SgFortranCommonBlockRefExp *reference =
          isSgFortranCommonBlockRefExp(expression)) {
    SageInterface::validateFortranCommonBlockRef(reference);
    SgExprListExp *list = isSgExprListExp(reference->get_parent());
    SgNode *clause = list != NULL ? list->get_parent() : NULL;
    if (isSgOmpVariablesClause(clause) == NULL &&
        isSgAccVariablesClause(clause) == NULL &&
        isSgOmpThreadprivateStatement(reference->get_parent()) == NULL) {
      fprintf(stderr,
              "REX_AST_INVARIANT[fortran-common-block-context]: reference=%p "
              "name=/%s/ parent=%p clause=%p is not a directive "
              "variable-list operand\n",
              static_cast<void *>(reference), reference->get_use_name().str(),
              static_cast<void *>(reference->get_parent()),
              static_cast<void *>(clause));
      ROSE_ABORT();
    }
    return;
  }

  if (expression != NULL && expression->has_semantic_value_type()) {
    SgType *type = expression->get_type();
    ROSE_ASSERT(type != NULL);
    // PC (10/12/2009): The following test verifies that array types properly
    // decay to pointer types
    //  From C99 6.3.2.1p3:
    /* Except when it is the operand of the sizeof operator or the unary &
       operator, or is a string literal used to initialize an array, an
       expression that has type array of type is converted to an expression with
       type pointer to type that points to the initial element of the array
       object and is not an lvalue. */
    type = type->stripTypedefsAndModifiers();
    ROSE_ASSERT(type != NULL);
    if (type->variantT() == V_SgArrayType ||
        type->variantT() == V_SgTypeString) {
      SgExpression *parentExpr = isSgExpression(expression->get_parent());
      if (parentExpr != NULL && parentExpr->has_semantic_value_type() &&
          !(
              // DQ (10/31/2015): I think this is fixing the same issue (and
              // resulted in a conflict).
              (parentExpr->variantT() == V_SgAssignInitializer &&
               expression->variantT() == V_SgStringVal) ||
              parentExpr->variantT() == V_SgDotExp ||
              parentExpr->variantT() == V_SgArrowExp ||
              (isSgInitializer(parentExpr) && isSgInitializer(expression)))) {
        SgType *parentType = parentExpr->get_type();
        parentType = parentType->stripTypedefsAndModifiers();
        if (SageInterface::getDimensionCount(type) ==
                SageInterface::getDimensionCount(parentType) &&
            SageInterface::getArrayElementType(type) ==
                SageInterface::getArrayElementType(parentType)) {
          SgValueExp *parentValueExpr = isSgValueExp(parentExpr);
          if (parentValueExpr == NULL ||
              expression != parentValueExpr->get_originalExpressionTree()) {
            // DQ (9/14/2010): Make this a level 1 warning. It is common for
            // Fortran code but never see for C++ code.
            if (SgProject::get_verbose() > 0) {
              printf("Warning: encountered a case where an array type did not "
                     "decay to a pointer type\n"
                     "  parentExpr = %p = %s, expression = %p = %s, parentType "
                     "= %p = %s, type = %p = %s\n",
                     parentExpr, parentExpr->sage_class_name(), expression,
                     expression->sage_class_name(), parentType,
                     parentType->sage_class_name(), type,
                     type->sage_class_name());
            }
            // ROSE_ABORT();
          }
        }
      }
    }
  }

  // DQ (10/31/2016): Testing to debug mergeTest_04.C and mergeTest_111.C.

  SgType *type = NULL;
  switch (node->variantT()) {
    // IR nodes that have a get_type member function
  case V_SgInitializedName: {
    SgInitializedName *initializedName = isSgInitializedName(node);
    ROSE_ASSERT(initializedName->get_type() != NULL);
    type = initializedName->get_type();
    break;
  }
  case V_SgTemplateParameter: {
    SgTemplateParameter *x = isSgTemplateParameter(node);
    ROSE_ASSERT(x->get_type() != NULL);
    type = x->get_type();
    break;
  }
  case V_SgTemplateArgument: {
    SgTemplateArgument *argument = isSgTemplateArgument(node);
    ROSE_ASSERT(argument != nullptr);
    auto fail_malformed_argument = [&](const char *detail) {
      fprintf(stderr,
              "REX_AST_INVARIANT[template-argument-payload]: argument=%p "
              "kind=%d %s\n",
              static_cast<void *>(argument),
              static_cast<int>(argument->get_argumentType()), detail);
      ROSE_ABORT();
    };
    switch (argument->get_argumentType()) {
    case SgTemplateArgument::type_argument:
      if (argument->get_type() == nullptr ||
          argument->get_expression() != nullptr ||
          argument->get_templateDeclaration() != nullptr) {
        fail_malformed_argument(
            "type argument does not own exactly one type payload");
      }
      type = argument->get_type();
      break;
    case SgTemplateArgument::nontype_argument: {
      if (argument->get_expression() == nullptr ||
          argument->get_templateDeclaration() != nullptr) {
        fail_malformed_argument(
            "nontype argument does not own exactly one value payload");
      }
      SgType *payloadType = argument->get_expression()->get_type();
      if (argument->get_type() == nullptr || payloadType == nullptr ||
          !SageInterface::cxxNonTypeTemplateArgumentTypeConversionIsExact(
              payloadType, argument->get_type())) {
        fprintf(stderr,
                "REX_AST_INVARIANT[template-argument-payload-type]: "
                "argument=%p declared=%p/%s payload=%p/%s\n",
                static_cast<void *>(argument),
                static_cast<void *>(argument->get_type()),
                argument->get_type() != nullptr
                    ? argument->get_type()->class_name().c_str()
                    : "<null>",
                static_cast<void *>(payloadType),
                payloadType != nullptr ? payloadType->class_name().c_str()
                                       : "<null>");
        fail_malformed_argument(
            "nontype argument has no equivalent value-payload type");
      }
      type = payloadType;
      break;
    }
    case SgTemplateArgument::template_template_argument: {
      SgDeclarationStatement *templateDeclaration =
          argument->get_templateDeclaration();
      if (argument->get_type() != nullptr ||
          argument->get_expression() != nullptr ||
          templateDeclaration == nullptr) {
        fail_malformed_argument(
            "template argument does not own exactly one declaration payload");
      }
      if (SgTemplateClassDeclaration *classDeclaration =
              isSgTemplateClassDeclaration(templateDeclaration)) {
        type = classDeclaration->get_type();
      } else if (SgTemplateTypedefDeclaration *typedefDeclaration =
                     isSgTemplateTypedefDeclaration(templateDeclaration)) {
        type = typedefDeclaration->get_type();
      } else if (SgTemplateDeclaration *templateParameterDeclaration =
                     isSgTemplateDeclaration(templateDeclaration)) {
        // A template-template parameter owns an SgTemplateDeclaration as its
        // exact semantic identity.  That generic declaration deliberately has
        // no SgType (its get_type() is a hard-error API); validate the complete
        // declaration/symbol publication instead of manufacturing a type.
        SgDeclarationScope *scope =
            isSgDeclarationScope(templateParameterDeclaration->get_scope());
        SgTemplateSymbol *symbol =
            scope != nullptr
                ? isSgTemplateSymbol(templateParameterDeclaration
                                         ->get_symbol_from_symbol_table())
                : nullptr;
        if (templateParameterDeclaration->get_template_kind() !=
                SgTemplateDeclaration::e_template_class ||
            templateParameterDeclaration->get_name().getString().empty() ||
            scope == nullptr ||
            templateParameterDeclaration->get_parent() != scope ||
            templateParameterDeclaration->get_firstNondefiningDeclaration() !=
                templateParameterDeclaration ||
            templateParameterDeclaration->get_definingDeclaration() !=
                nullptr ||
            symbol == nullptr ||
            symbol->get_declaration() != templateParameterDeclaration ||
            symbol->get_parent() != scope->get_symbol_table() ||
            !scope->symbol_exists(symbol)) {
          fail_malformed_argument(
              "template-template parameter declaration lacks exact scope, "
              "symbol, or declaration-family identity");
        }
      } else if (SgNonrealDecl *nonrealDeclaration =
                     isSgNonrealDecl(templateDeclaration)) {
        type = nonrealDeclaration->get_type();
      } else {
        fail_malformed_argument(
            "declaration payload is not a template-bearing AST category");
      }
      if (type == nullptr &&
          isSgTemplateDeclaration(templateDeclaration) == nullptr) {
        fail_malformed_argument(
            "declaration payload has no exact associated template type");
      }
      break;
    }
    case SgTemplateArgument::start_of_pack_expansion_argument:
      fail_malformed_argument(
          "synthetic pack marker escaped template-argument construction");
      break;
    case SgTemplateArgument::argument_undefined:
    default:
      fail_malformed_argument("argument has an undefined payload kind");
      break;
    }
    break;
  }
  case V_SgVariableDefinition: {
    SgVariableDefinition *x = isSgVariableDefinition(node);
    ROSE_ASSERT(x->get_type() != NULL);
    type = x->get_type();
    break;
  }
  case V_SgEnumDeclaration: {
    SgEnumDeclaration *x = isSgEnumDeclaration(node);
    ROSE_ASSERT(x->get_type() != NULL);
    type = x->get_type();
    break;
  }
  case V_SgTypedefDeclaration: {
    SgTypedefDeclaration *x = isSgTypedefDeclaration(node);
    ROSE_ASSERT(x->get_type() != NULL);
    type = x->get_type();
    break;
  }
  case V_SgClassDeclaration:
  case V_SgDerivedTypeStatement: {
    SgClassDeclaration *x = isSgClassDeclaration(node);
    ROSE_ASSERT(x->get_type() != NULL);
    type = x->get_type();
    break;
  }
  case V_SgFunctionDeclaration: {
    SgFunctionDeclaration *x = isSgFunctionDeclaration(node);
    ROSE_ASSERT(x->get_type() != NULL);
    type = x->get_type();
    break;
  }

  default: {
    // noting to do
  }
  }

  SgNamedType *namedType = isSgNamedType(type);
  if (namedType != NULL) {
    SgDeclarationStatement *declaration = namedType->get_declaration();
    ROSE_ASSERT(declaration != NULL);
    SgDeclarationStatement *nondefiningDeclaration =
        declaration->get_firstNondefiningDeclaration();
    SgDeclarationStatement *definingDeclaration =
        declaration->get_definingDeclaration();
    if (definingDeclaration != NULL) {
      switch (definingDeclaration->variantT()) {
        // DQ (1/1/2012): Added support for templates in the AST.
      case V_SgTemplateClassDeclaration:

        // This case applies to template or non-template classes
      case V_SgClassDeclaration:
      case V_SgDerivedTypeStatement:
      case V_SgTemplateInstantiationDecl: {
        SgClassDeclaration *definingClassDeclaration =
            isSgClassDeclaration(definingDeclaration);
        ROSE_ASSERT(definingClassDeclaration->get_definition() != NULL);
        ROSE_ASSERT(nondefiningDeclaration != NULL);
        break;
      }

      case V_SgTypedefDeclaration: {
        SgTypedefDeclaration *typedefDeclaration =
            isSgTypedefDeclaration(definingDeclaration);
        ROSE_ASSERT(typedefDeclaration->get_base_type() != NULL);
        ROSE_ASSERT(typedefDeclaration->get_type() != NULL);
        ROSE_ASSERT(typedefDeclaration->get_scope() != NULL);

        // This is to handle declarations that appear directly in the typedef
        // (most often NULL)
        if (typedefDeclaration
                ->get_typedefBaseTypeContainsDefiningDeclaration() == true)
          ROSE_ASSERT(typedefDeclaration->get_declaration() != NULL);
        else
          ROSE_ASSERT(typedefDeclaration->get_declaration() == NULL);
        break;
      }

      case V_SgEnumDeclaration: {
        SgEnumDeclaration *enumDeclaration =
            isSgEnumDeclaration(definingDeclaration);
        ROSE_ASSERT(enumDeclaration->get_type() != NULL);
        ROSE_ASSERT(enumDeclaration->get_scope() != NULL);
        break;
      }

      default: {
        printf("Error: default reached in switch: definingDeclaration not "
               "tested = %p = %s \n",
               definingDeclaration, definingDeclaration->class_name().c_str());
        ROSE_ABORT();
      }
      }
    }
  }
}

void TestMangledNames::visit(SgNode *node) {
  ROSE_ASSERT(SgNode::isLiveNode(node));

  string mangledName;
  SgDeclarationStatement *declarationStatement = isSgDeclarationStatement(node);
  if (declarationStatement != NULL) {
    SgNonrealDecl *nrdecl = isSgNonrealDecl(declarationStatement);
    ROSE_ASSERT(nrdecl != NULL || declarationStatement->get_scope() != NULL);
    if (nrdecl == NULL)
      ROSE_ASSERT(SgNode::isLiveNode(declarationStatement->get_scope()));
    if (!isOpenmpNonentityDirective(node))
      mangledName = declarationStatement->get_mangled_name().getString();
  }

  SgInitializedName *initializedName = isSgInitializedName(node);
  if (initializedName != NULL) {
    if (initializedName->get_scope() != NULL) {
      ROSE_ASSERT(SgNode::isLiveNode(initializedName->get_scope()));
      mangledName = initializedName->get_mangled_name().getString();
    }
  }
  SgFunctionDefinition *functionDefinition = isSgFunctionDefinition(node);
  if (functionDefinition != NULL) {
    SgTemplateFunctionDefinition *templateFunctionDefinition =
        isSgTemplateFunctionDefinition(functionDefinition);
    if (templateFunctionDefinition != NULL) {
      mangledName = templateFunctionDefinition->get_mangled_name().getString();
    } else {
      mangledName = functionDefinition->get_mangled_name().getString();
    }
  }
  SgClassDefinition *classDefinition = isSgClassDefinition(node);
  if (classDefinition != NULL) {
    mangledName = classDefinition->get_mangled_name().getString();
  }
  SgNamespaceDefinitionStatement *namespaceDefinition =
      isSgNamespaceDefinitionStatement(node);
  if (namespaceDefinition != NULL) {
    mangledName = namespaceDefinition->get_mangled_name().getString();
  }
  SgTemplateArgument *templateArgument = isSgTemplateArgument(node);
  if (templateArgument != NULL) {
    mangledName = templateArgument->get_mangled_name().getString();
  }
  SgType *type = isSgType(node);
  if (type != NULL) {
    SgDeclarationStatement *decl = type->getAssociatedDeclaration();
    if (decl != NULL) {
      ROSE_ASSERT(SgNode::isLiveNode(decl));
      ROSE_ASSERT(decl->get_scope() != NULL);
      ROSE_ASSERT(SgNode::isLiveNode(decl->get_scope()));
      mangledName = type->get_mangled().getString();
    }
  }

  if (mangledName.empty())
    return;

  unsigned long mangledNameSize = mangledName.size();
  saved_totalMangledNameSize += mangledNameSize;
  saved_numberOfMangledNames++;
  if (saved_maxMangledNameSize < mangledNameSize) {
    saved_maxMangledNameSize = mangledNameSize;
    if (SgProject::get_verbose() >= DIAGNOSTICS_VERBOSE_LEVEL) {
      printf("saved_maxMangledNameSize = %lu average size = %lu \n",
             saved_maxMangledNameSize,
             saved_totalMangledNameSize / saved_numberOfMangledNames);
    }
  }
}

TestMangledNames::TestMangledNames()
    : saved_maxMangledNameSize(0), saved_totalMangledNameSize(0),
      saved_numberOfMangledNames(0), totalLongMangledNameSize(0),
      totalNumberOfLongMangledNames(0) {}

void TestMangledNames::test() {
  TestMangledNames t;

  const std::map<std::string, uint64_t> &shortMangledNameCache =
      SgNode::get_shortMangledNameCache();
  for (std::map<std::string, uint64_t>::const_iterator i =
           shortMangledNameCache.begin();
       i != shortMangledNameCache.end(); ++i) {
    t.totalLongMangledNameSize += i->first.size();
    ++(t.totalNumberOfLongMangledNames);
  }

  // Only these node families contribute mangled names in this test.
  SgDeclarationStatement::traverseMemoryPoolNodes(t);
  SgInitializedName::traverseMemoryPoolNodes(t);
  SgFunctionDefinition::traverseMemoryPoolNodes(t);
  SgClassDefinition::traverseMemoryPoolNodes(t);
  SgNamespaceDefinitionStatement::traverseMemoryPoolNodes(t);
  SgTemplateArgument::traverseMemoryPoolNodes(t);
  SgType::traverseMemoryPoolNodes(t);
  if (SgProject::get_verbose() >= DIAGNOSTICS_VERBOSE_LEVEL) {
    printf("saved_numberOfMangledNames = %lu \n", t.saved_numberOfMangledNames);
    printf("saved_maxMangledNameSize   = %lu \n", t.saved_maxMangledNameSize);
    printf("saved_totalMangledNameSize = %lu", t.saved_totalMangledNameSize);
    if (t.saved_numberOfMangledNames != 0) {
      printf(" average size = %f",
             static_cast<double>(t.saved_totalMangledNameSize) /
                 t.saved_numberOfMangledNames);
    }
    printf("\n");
    printf("Total long mangled name size = %lu for %lu name(s)",
           t.totalLongMangledNameSize, t.totalNumberOfLongMangledNames);
    if (t.totalNumberOfLongMangledNames != 0) {
      printf("; average is %f",
             static_cast<double>(t.totalLongMangledNameSize) /
                 t.totalNumberOfLongMangledNames);
    }
    printf("\n");
  }
}

void TestParentPointersInMemoryPool::test() {
  TestParentPointersInMemoryPool t;
  SgSymbol::traverseMemoryPoolNodes(t);
  SgStatement::traverseMemoryPoolNodes(t);
  SgSupport::traverseMemoryPoolNodes(t);
}

void TestParentPointersInMemoryPool::visit(SgNode *node) {

  SgSymbol *symbol = isSgSymbol(node);
  SgSupport *support = isSgSupport(node);

  // Symbols can be shared within a single file but are not yet shared across
  // files in the AST merge
  if (symbol != NULL) {
    if (symbol->get_parent() == NULL)
      printf("Error: symbol named %s has no parent set\n",
             symbol->get_name().getString().c_str());
    ROSE_ASSERT(symbol->get_parent() != NULL);
  }

  // Some SgSupport IR nodes have a valid parent and others are always NULL
  if (support != NULL) {
    switch (support->variantT()) {
    case V_SgProject: {
      ROSE_ASSERT(support->get_parent() == NULL);
      break;
    }

    case V_SgStorageModifier:
    case V_SgSymbolTable:
    case V_SgSourceFile:
    case V_SgFunctionTypeArgument:
    case V_SgFunctionParameterTypeList:
    case V_SgPragma:
    case V_SgBaseClass:
    case V_SgExpBaseClass:
    case V_SgNonrealBaseClass:
    case V_Sg_File_Info:
    case V_SgTemplateParameter:
    case V_SgTypeTable:
    case V_SgIncludeFile:
      break;

      // DQ (6/11/2007): Added this case
    case V_SgInitializedName: {
      SgNode *parent = support->get_parent();
      if (parent == NULL) {
        printf("Warning: detected SgInitializedName without parent set "
               "properly at %p = %s parent is currently NULL \n",
               support, support->class_name().c_str());
      }
      break;
    }

      // DQ (6/11/2007): Added this case
    case V_SgTemplateArgument: {
      SgNode *parent = support->get_parent();
      if (parent == NULL) {
        printf("Warning: detected SgTemplateArgument without parent set "
               "properly at %p = %s parent is currently NULL \n",
               support, support->class_name().c_str());
      }
      break;
    }

      // DQ (6/11/2007): Added this case
    case V_SgCommonBlockObject: {
      SgNode *parent = support->get_parent();
      if (parent == NULL) {
        printf("Warning: detected SgCommonBlockObject without parent set "
               "properly at %p = %s parent is currently NULL \n",
               support, support->class_name().c_str());
      }
      break;
    }

      // DQ (1/23/2010): Added this case
    case V_SgFileList:
    case V_SgDirectoryList:

      // DQ (10/4/2008): Added this case
    case V_SgRenamePair:

      // DQ (12/23/2007): Added this case
    case V_SgFormatItem:
    case V_SgFormatItemList: {
      SgNode *parent = support->get_parent();
      if (parent == NULL) {
        printf("Warning: detected SgSupport IR node without parent set "
               "properly at %p = %s parent is currently NULL \n",
               support, support->class_name().c_str());
      }
      break;
    }

    // driscoll6 (01/03/2011): Added this case.
    case V_SgGraphEdge:
    case V_SgDirectedGraphEdge:
    case V_SgUndirectedGraphEdge:
    case V_SgGraphNode: {
      if (!isSgGraph(support->get_parent())) {
        std::cerr << "Graph component " << support->class_name()
                  << " requires SgGraph as parent" << std::endl;
        ROSE_ABORT();
      }
      break;
    }

    default: {
      if (support->get_parent() != NULL) {
        printf("##### TestParentPointersInMemoryPool::visit(node = %p = %s), "
               "support->get_parent() != NULL == %s\n",
               node, node->sage_class_name(),
               support->get_parent()->sage_class_name());
      }
      ROSE_ASSERT(support->get_parent() == NULL);
      break;
    }
    }
  }
}

void TestChildPointersInMemoryPool::test() {
  TestChildPointersInMemoryPool t;
  t.edges.reserve(numberOfNodes());
  t.traverseMemoryPool();

  // Each parent's child pointers are collected once, for all of its children
  // together, and released before the next parent's are collected.
  std::vector<ParentEdge> &edges = t.edges;
  std::sort(edges.begin(), edges.end(),
            [](const ParentEdge &lhs, const ParentEdge &rhs) {
              if (lhs.parent != rhs.parent)
                return std::less<SgNode *>()(lhs.parent, rhs.parent);
              return lhs.order < rhs.order;
            });
  std::vector<ParentEdge> missing;
  std::unordered_set<SgNode *> children;
  for (size_t begin = 0; begin < edges.size();) {
    SgNode *parent = edges[begin].parent;
    size_t end = begin;
    while (end < edges.size() && edges[end].parent == parent)
      ++end;

#if ROSE_USE_VALGRIND
    if (VALGRIND_CHECK_MEM_IS_ADDRESSABLE(parent, sizeof(SgNode))) {
      fprintf(stderr, "Parent %p of child %p (a %s) has been deleted.\n",
              parent, edges[begin].child,
              edges[begin].child->class_name().c_str());
    }
#endif

    children.clear();
    children.reserve(parent->get_numberOfTraversalSuccessors() + 8);
    ChildPointerCollector collector(children);
    parent->processDataMemberReferenceToPointers(&collector);
    for (size_t index = begin; index < end; ++index) {
      if (children.find(edges[index].child) == children.end())
        missing.push_back(edges[index]);
    }
    begin = end;
  }

  // Missing children are reported in memory-pool order.
  std::sort(missing.begin(), missing.end(),
            [](const ParentEdge &lhs, const ParentEdge &rhs) {
              return lhs.order < rhs.order;
            });
  for (const ParentEdge &edge : missing)
    reportMissingChild(edge.child, edge.parent);
}

void TestChildPointersInMemoryPool::visit(SgNode *node) {
  ROSE_ASSERT(SgNode::isLiveNode(node));

  SgNode *parent = node->get_parent();

#if ROSE_USE_VALGRIND
  VALGRIND_CHECK_VALUE_IS_DEFINED(parent);
#endif

  if (parent != NULL) {
    ROSE_ASSERT(SgNode::isLiveNode(parent));
    edges.push_back({parent, node, edges.size()});
  }
}

void TestChildPointersInMemoryPool::reportMissingChild(SgNode *node,
                                                       SgNode *parent) {

  switch (node->variantT()) {
  case V_SgTemplateInstantiationDecl:
  case V_SgClassDeclaration:
  case V_SgDerivedTypeStatement: {
    // Ignore this case, there a lots of cases where non-defining
    // declarations are built and they are assigned parents where they are
    // not children.
    break;
  }

  case V_SgTemplateDeclaration: {
    // Ignore this case, at least numerous non-defining declarations will
    // fail to appear in the parent' child list.
    break;
  }

  case V_SgFunctionTypeTable: {
    // Ignore this case, the pointer to the SgFunctionTypeTable is a static
    // data member and not returned by the returnDataMemberPointers()
    // function on the parent (SgNode).
    break;
  }

  case V_SgTypedefDeclaration: {
    SgTypedefDeclaration *typedefDeclaration = isSgTypedefDeclaration(node);
    SgTemplateArgument *templateArgument = isSgTemplateArgument(parent);
    ROSE_ASSERT(templateArgument != NULL);
    SgTypedefType *typedefType = isSgTypedefType(templateArgument->get_type());
    ROSE_ASSERT(typedefType != NULL);
    ROSE_ASSERT(typedefType->get_declaration() == typedefDeclaration);
    break;
  }

  case V_SgInitializedName: {
    SgInitializedName *initializedName = isSgInitializedName(node);
    if (initializedName->get_is_predefined_identifier()) {
      ROSE_ASSERT(isSgFunctionDefinition(parent) != NULL);
    } else {
      MLOG_KEY_C("astDiagnostics",
                 "TestChildPointersInMemoryPool::visit(): Node is not in "
                 "parent's child list, node: %p = %s = %s parent: %p = %s \n",
                 node, node->class_name().c_str(),
                 SageInterface::get_name(node).c_str(), parent,
                 parent->class_name().c_str());
    }
    break;
  }

  case V_SgVariableSymbol: {
    SgVariableSymbol *variableSymbol = isSgVariableSymbol(node);
    SgInitializedName *initializedName = variableSymbol->get_declaration();
    if (initializedName == NULL ||
        !initializedName->get_is_predefined_identifier()) {
      MLOG_KEY_C(
          "astDiagnostics",
          "TestChildPointersInMemoryPool::visit(). SgVariableSymbol is not "
          "in parent's child list, node: %p = %s = %s parent: %p = %s \n",
          node, node->class_name().c_str(),
          SageInterface::get_name(node).c_str(), parent,
          parent->class_name().c_str());
    }
    break;
  }

  case V_SgClassSymbol: {
    SgClassSymbol *classSymbol = isSgClassSymbol(node);
    SgClassDeclaration *classDeclaration = classSymbol->get_declaration();

    // DQ (3/3/2007): This is always a case we want to warn about!
    printf("SgClassSymbol is not in parent's child list, node: %p = %s = "
           "%s parent: %p = %s = %s \n",
           node, node->class_name().c_str(),
           SageInterface::get_name(node).c_str(), parent,
           parent->class_name().c_str(),
           SageInterface::get_name(parent).c_str());
    classDeclaration->get_file_info()->display(
        "case V_SgClassSymbol: location: debug");
    break;
  }

  case V_SgFunctionSymbol: {
    SgFunctionSymbol *functionSymbol = isSgFunctionSymbol(node);
    SgFunctionDeclaration *functionDeclaration =
        functionSymbol->get_declaration();

    // DQ (3/3/2007): This is always a case we want to warn about!
    printf("SgFunctionSymbol is not in parent's child list, node: %p = %s "
           "= %s parent: %p = %s = %s \n",
           node, node->class_name().c_str(),
           SageInterface::get_name(node).c_str(), parent,
           parent->class_name().c_str(),
           SageInterface::get_name(parent).c_str());
    functionDeclaration->get_file_info()->display(
        "case V_SgFunctionSymbol: location: debug");
    break;
  }

  case V_SgStorageModifier: {
    SgStorageModifier *storageModifier = isSgStorageModifier(node);
    SgInitializedName *initializedName =
        isSgInitializedName(storageModifier->get_parent());
    if (initializedName != NULL) {
      printf("SgStorageModifier is not in parent's child list, "
             "SgInitializedName: %p = %s = %s parent: %p = %s = %s \n",
             storageModifier, storageModifier->class_name().c_str(),
             SageInterface::get_name(storageModifier).c_str(), initializedName,
             initializedName->class_name().c_str(),
             SageInterface::get_name(initializedName).c_str());
      SgNode *parent1 = initializedName->get_parent();
      printf("parent1 IR node = %p = %s \n", parent1,
             parent1->class_name().c_str());
      SgNode *parent2 = parent1->get_parent();
      printf("parent2 IR node = %p = %s \n", parent2,
             parent2->class_name().c_str());
      SgNode *parent3 = parent2->get_parent();
      printf("parent3 IR node = %p = %s \n", parent3,
             parent3->class_name().c_str());
      SgNode *parent4 = parent3->get_parent();
      printf("parent4 IR node = %p = %s \n", parent4,
             parent4->class_name().c_str());
      initializedName->get_file_info()->display(
          "case V_SgStorageModifier: location: debug");
    } else {
      // DQ (3/3/2007): This is always a case we want to warn about!
      printf("SgStorageModifier is not in parent's child list, node: %p = "
             "%s = %s parent: %p = %s \n",
             node, node->class_name().c_str(),
             SageInterface::get_name(node).c_str(), parent,
             parent->class_name().c_str());
    }
    break;
  }

    // DQ (6/13/2007): Added case to avoid warnings where Sg_File_Info
    // objects are used in comments and cpp directives.
  case V_Sg_File_Info: {
    Sg_File_Info *fileInfo = isSg_File_Info(node);
    SgNode *parent = fileInfo->get_parent();
    if (!fileInfo->isCommentOrDirective())
      ROSE_ASSERT(parent != NULL);
    break;
  }

  default:
    break;
  }

  // Additional information!
  if (SgProject::get_verbose() >= DIAGNOSTICS_VERBOSE_LEVEL + 1) {
    // DQ (1/22/2008): Detect the case where node is a Sg_File_Info IR nodes
    // (which should not have a valid get_file_info() pointer)
    Sg_File_Info *fileInfo = isSg_File_Info(node);
    if (node->get_file_info() == NULL) {
      // DQ (5/17/2008): It is OK for a SgSymbol to have a NULL fileInfo
      // pointer DQ (1/22/2008): Ignore that the get_file_info() for a
      // Sg_File_Info IR node is NULL if (fileInfo == NULL && isSgType(node)
      // == NULL )
      if (fileInfo == NULL &&
          (isSgType(node) == NULL && isSgSymbol(node) == NULL))
        printf("Warning: node->get_file_info() is NULL on node = %p = %s \n",
               node, node->class_name().c_str());
    } else {
      // node->get_file_info()->display("node location: debug");
      ROSE_ASSERT(fileInfo == NULL);
    }

    if (parent != NULL) {
      if (parent->get_file_info() == NULL) {
        // DQ (5/17/2008): It is OK for a SgSymbol to have a parent which is
        // a symbol table without a valid fileInfo pointer DQ (1/22/2008):
        // Ignore SgType IR nodes (since they never have a valid
        // get_file_info()) if (isSgType(parent) == NULL )
        if (isSgType(parent) == NULL && isSgSymbolTable(parent) == NULL)
          printf("Warning: parent->get_file_info() is NULL on node = %p = "
                 "%s \n",
                 parent, parent->class_name().c_str());
      } else {
        // parent->get_file_info()->display("parent location: debug");
      }
    } else {
      printf("Warning: parent of node->get_file_info() is NULL on node = "
             "%p = %s \n",
             node, node->class_name().c_str());
    }
  }

  // DQ (9/14/2006): This causes the
  // ROSE/tests/nonsmoke/functional/roseTests/astMergeTests/testMerge.C test
  // to fail! cout << node->unparseToString() << endl; cout << endl;
}

void TestFirstNondefiningDeclarationsForForwardMarking::test() {
  TestFirstNondefiningDeclarationsForForwardMarking t;
  SgDeclarationStatement::traverseMemoryPoolNodes(t);
}

void TestFirstNondefiningDeclarationsForForwardMarking::visit(SgNode *node) {
  SgDeclarationStatement *declaration = isSgDeclarationStatement(node);
  if (declaration != NULL) {
    SgDeclarationStatement *firstDefiningDeclaration =
        declaration->get_firstNondefiningDeclaration();
    if (firstDefiningDeclaration->isForward() == true) {
      printf("Error: firstDefiningDeclaration->isForward() == true: node = %p "
             "= %s = %s \n",
             node, node->class_name().c_str(),
             SageInterface::get_name(node).c_str());
      firstDefiningDeclaration->get_file_info()->display(
          "Error: firstDefiningDeclaration->isForward() == true: debug");
    }
    ROSE_ASSERT(firstDefiningDeclaration->isForward() == false);
  }
}

void TestLValueExpressions::visit(SgNode *node) {
  SgExpression *expression = isSgExpression(node);
  if (expression != NULL) {
    SgBinaryOp *binaryOperator = isSgBinaryOp(expression);

    if (binaryOperator != NULL) {
      switch (expression->variantT()) {
        // IR nodes that have an l-value
      case V_SgAssignOp:
      case V_SgAndAssignOp:
      case V_SgDivAssignOp:
      case V_SgIorAssignOp:
      case V_SgLshiftAssignOp:
      case V_SgMinusAssignOp:
      case V_SgModAssignOp:
      case V_SgMultAssignOp:
      case V_SgPlusAssignOp:
      case V_SgRshiftAssignOp:
      case V_SgXorAssignOp: {
        SgExpression *lhs = binaryOperator->get_lhs_operand();
        ROSE_ASSERT(lhs != NULL);

        if (lhs->get_lvalue() == false) {
          printf("ERROR (lvalue for Assign operator not marked): for lhs = %p "
                 "= %s = %s in binary expression = %s \n",
                 lhs, lhs->class_name().c_str(),
                 SageInterface::get_name(lhs).c_str(),
                 expression->class_name().c_str());
          binaryOperator->get_startOfConstruct()->display(
              "Error for lhs: lhs->get_lvalue() == true: debug");
        }

        ROSE_ASSERT(lhs->get_lvalue() == true);
        break;
      }

      default: {
        SgExpression *lhs = binaryOperator->get_lhs_operand();
        ROSE_ASSERT(lhs != NULL);
      }
      }

      SgExpression *rhs = binaryOperator->get_rhs_operand();
      ROSE_ASSERT(rhs != NULL);
    }

    SgUnaryOp *unaryOperator = isSgUnaryOp(expression);
    if (unaryOperator != NULL) {
      switch (expression->variantT()) {
        // IR nodes that should have a valid lvalue
      case V_SgAddressOfOp: {
        SgExpression *operand = unaryOperator->get_operand();
        ROSE_ASSERT(operand != NULL);
        // Address-of accepts object lvalues, function designators, and
        // qualified member-function designators. The legacy p_lvalue cache is
        // contextual and therefore has no single valid value for this whole
        // operand family. Clang has already validated the operand category;
        // the AST contract here is exact structural ownership.
        ROSE_ASSERT(operand->get_parent() == unaryOperator);
        break;
      }

      case V_SgMinusMinusOp:
      case V_SgPlusPlusOp: {
        SgExpression *operand = unaryOperator->get_operand();
        ROSE_ASSERT(operand != NULL);
        ROSE_ASSERT(operand->get_lvalue() == true);
        break;
      }

      case V_SgThrowOp:
        break;

      case V_SgNotOp:
      case V_SgPointerDerefExp:
      case V_SgCastExp:
      case V_SgMinusOp:
      case V_SgBitComplementOp:
      case V_SgRealPartOp:
      case V_SgImagPartOp: {
        SgExpression *operand = unaryOperator->get_operand();
        ROSE_ASSERT(operand != NULL);
        break;
      }

      default: {
        SgExpression *operand = unaryOperator->get_operand();
        ROSE_ASSERT(operand != NULL);
        ROSE_ASSERT(operand->get_parent() == unaryOperator);
      }
      }
    }
  }
}

TestForProperLanguageAndSymbolTableCaseSensitivity_InheritedAttribute::
    TestForProperLanguageAndSymbolTableCaseSensitivity_InheritedAttribute(
        bool b)
    : sourceFile(NULL), caseInsensitive(b) {}

TestForProperLanguageAndSymbolTableCaseSensitivity_InheritedAttribute::
    TestForProperLanguageAndSymbolTableCaseSensitivity_InheritedAttribute(
        const TestForProperLanguageAndSymbolTableCaseSensitivity_InheritedAttribute
            &X) {
  sourceFile = X.sourceFile;
  caseInsensitive = X.caseInsensitive;
}

TestForProperLanguageAndSymbolTableCaseSensitivity_InheritedAttribute
TestForProperLanguageAndSymbolTableCaseSensitivity::evaluateInheritedAttribute(
    SgNode *node,
    TestForProperLanguageAndSymbolTableCaseSensitivity_InheritedAttribute
        inheritedAttribute) {
  TestForProperLanguageAndSymbolTableCaseSensitivity_InheritedAttribute
      return_inheritedAttribute(inheritedAttribute);

  SgSourceFile *sourceFile = isSgSourceFile(node);
  if (sourceFile != NULL) {
    return_inheritedAttribute.sourceFile = sourceFile;
    if (sourceFile->get_Fortran_only()) {
      return_inheritedAttribute.caseInsensitive = true;
    }
  }

  SgScopeStatement *scope = isSgScopeStatement(node);
  if (scope != NULL) {
    if (scope->isCaseInsensitive() !=
        return_inheritedAttribute.caseInsensitive) {
      printf("Error: scope->isCaseInsensitive() = %s "
             "inheritedAttribute.caseInsensitive = %s \n",
             scope->isCaseInsensitive() ? "true" : "false",
             return_inheritedAttribute.caseInsensitive ? "true" : "false");
      printf("       type of scope = %s\n", scope->class_name().c_str());
      scope->get_startOfConstruct()->display(
          "scope->isCaseInsensitive() incorrectly set");
      ROSE_ASSERT(return_inheritedAttribute.sourceFile != NULL);
      SgSourceFile *originFile = return_inheritedAttribute.sourceFile;

      if (originFile->get_Fortran_only()) {
        printf("Fortran file %s should have an AST with scopes marked as case "
               "insensitive \n",
               originFile->getFileName().c_str());
      } else {
        printf("Non-fortran file %s should have an AST with scopes marked as "
               "case sensitive \n",
               originFile->getFileName().c_str());
      }
    }
    ROSE_ASSERT(scope->isCaseInsensitive() ==
                return_inheritedAttribute.caseInsensitive);
  }

  return return_inheritedAttribute;
}

void TestForProperLanguageAndSymbolTableCaseSensitivity::test(SgNode *node) {
  // Inherited attribute with caseInsensitive marked as false.
  bool caseInsensitive = false;
  TestForProperLanguageAndSymbolTableCaseSensitivity_InheritedAttribute IH(
      caseInsensitive);

  TestForProperLanguageAndSymbolTableCaseSensitivity traversal;

  // This should be a SgProject or SgFile so that we can evaluate the language
  // type (obtained from the SgFile).
  ROSE_ASSERT(isSgProject(node) != NULL || isSgFile(node) != NULL);

  traversal.traverse(node, IH);
}

TestForReferencesToDeletedNodes::TestForReferencesToDeletedNodes(
    int input_detect_dangling_pointers, const string &s)
    : detect_dangling_pointers(input_detect_dangling_pointers), filename(s) {
  // Nothing to do here!
}

void TestForReferencesToDeletedNodes::visit(SgNode *node) {
  // DQ (9/26/2011): This is a new test for possible references to deleted IR
  // nodes in the AST (dangling pointers). It works as a byproduct of the way
  // that memory pool works.  It might work better if we also added an option to
  // prevent previously deleted IR nodes from reusing memory in the memory pool.
  // This can be considered for future work.

  ROSE_ASSERT(node != NULL);
  // Only output information about this test if set to value greater than zero
  // (this avoids anoying output for existing translators).
  if (detect_dangling_pointers > 0) {
    vector<pair<SgNode *, string>> v = node->returnDataMemberPointers();

    for (unsigned long i = 0; i < v.size(); i++) {
      SgNode *child = v[i].first;

      if (child != NULL && child->variantT() == V_SgNode) {
        // This is a deleted IR node
        printf("Error in AST consistancy detect_dangling_pointers test for "
               "file %s: Found a child = %p = %s child name = %s of node = %p "
               "= %s that was previously deleted \n",
               filename.c_str(), child, child->class_name().c_str(),
               v[i].second.c_str(), node, node->class_name().c_str());
      }

      // DQ (9/26/2011): Other failing tests include:
      //    projects/backstroke/tests/expNormalizationTest/test2006_74.C
      //    projects/backstroke/tests/expNormalizationTest/test2006_87.C
      // Large percentage of Fortran tests fail this test!
      if (detect_dangling_pointers > 1) {
        ROSE_ASSERT((child == NULL) ||
                    (child != NULL && child->variantT() != V_SgNode));
      }
    }
  }
}

void TestForReferencesToDeletedNodes::test(SgProject *project) {
  // See documentation in the header file for this test.

  // I was having trouble generating a file name for where the problem was
  // happening, so this should work better. might be that the problem IR nodes
  // were contained in expressions in the index of a SgArrayType, or something
  // like that.
  // Generate a name from all the files on the command line
  string filename =
      SageInterface::generateProjectName(project, /* supressSuffix = */ false);

  TestForReferencesToDeletedNodes traversal(
      project->get_detect_dangling_pointers(), filename);

  traversal.traverseMemoryPool();
}

// explicit
TestForParentsMatchingASTStructure::TestForParentsMatchingASTStructure(
    std::ostream &output, const std::string &prefix)
    : output(output), nproblems(0), limit(0), prefix(prefix) {}

bool TestForParentsMatchingASTStructure::check(SgNode *ast, size_t limit) {
  this->limit = limit;
  nproblems = 0;
  stack.clear();
  struct ResetPath {
    std::vector<SgNode *> &path;
    ~ResetPath() { path.clear(); }
  } reset{stack};
  try {
    traverse(ast);
  } catch (const TestForParentsMatchingASTStructure *failure) {
    if (failure != this)
      throw;
    return false;
  }

  return 0 == nproblems;
}

void TestForParentsMatchingASTStructure::preOrderVisit(SgNode *node) {
  if (!stack.empty()) {
    if (NULL == node->get_parent()) {
      // output << prefix << "node has null parent property but was reached by
      // AST traversal\n";
      printf("In TestForParentsMatchingASTStructure::preOrderVisit(): (NULL == "
             "node->get_parent()): node class_name = %s , prefix = %s node's "
             "parent property is NULL\n",
             node->class_name().c_str(), prefix.c_str());

      show_details_and_maybe_fail(node);
    } else {
      if (node->get_parent() != stack.back()) {
        // output << prefix << "node's parent property does not match traversal
        // parent\n";
        if (SgProject::get_verbose() >= DIAGNOSTICS_VERBOSE_LEVEL) {
          printf("In TestForParentsMatchingASTStructure::preOrderVisit(): "
                 "(node->get_parent() != stack.back()): prefix = %s node's "
                 "parent property does not match traversal parent\n",
                 prefix.c_str());
          printf("traversal parent = %p = %s \n", stack.back(),
                 stack.back()->class_name().c_str());
        }
        SgNamespaceDefinitionStatement *namespaceDefinition =
            isSgNamespaceDefinitionStatement(stack.back());

        if (namespaceDefinition != NULL) {
          if (SgProject::get_verbose() >= DIAGNOSTICS_VERBOSE_LEVEL) {
            printf("node = %p = %s node->get_parent() = %p = %s \n", node,
                   node->class_name().c_str(), node->get_parent(),
                   node->get_parent()->class_name().c_str());
          }

          SgNamespaceDeclarationStatement *namespaceDeclaration =
              namespaceDefinition->get_namespaceDeclaration();
          if (SgProject::get_verbose() >= DIAGNOSTICS_VERBOSE_LEVEL) {
            printf("Found a namespaceDefinition = %p = %s \n",
                   namespaceDefinition, namespaceDeclaration->get_name().str());
          }

          SgNamespaceDefinitionStatement *previousNamespaceDefinition =
              namespaceDefinition;
          while (previousNamespaceDefinition != NULL) {
            if (SgProject::get_verbose() >= DIAGNOSTICS_VERBOSE_LEVEL) {
              printf("previousNamespaceDefinition = %p \n",
                     previousNamespaceDefinition);
            }

            previousNamespaceDefinition =
                previousNamespaceDefinition->get_previousNamespaceDefinition();
          }
        }

        show_details_and_maybe_fail(node);
      }
    }
  }

  stack.push_back(node);
}

void TestForParentsMatchingASTStructure::postOrderVisit(SgNode *node) {
  ROSE_ASSERT(!stack.empty());
  ROSE_ASSERT(node == stack.back());
  stack.pop_back();
}

void TestForParentsMatchingASTStructure::show_details_and_maybe_fail(
    SgNode *node) {
  ROSE_ASSERT(node != nullptr);
  output << prefix << "AST path when parent inconsistency was detected:\n";
  for (size_t i = 0; i < stack.size(); ++i)
    output << prefix << "    #" << i << " " << stack[i]->class_name() << " "
           << stack[i] << "; parent=" << stack[i]->get_parent() << "\n";
  output << prefix << "    #" << stack.size() << " " << node->class_name()
         << " " << node << "; parent=" << node->get_parent() << "\n";
  if (++nproblems >= limit)
    throw this;
}

void TestForParentsMatchingASTStructure::test(SgProject *project) {
  std::ostringstream report;
  if (!TestForParentsMatchingASTStructure(report, "    ").check(project, 10)) {
    std::cerr << "REX_AST_INVARIANT[parent-ownership]: traversal and recorded "
                 "parents disagree\n"
              << report.str();
    ROSE_ABORT();
  }
}

void TestForSourcePosition::testFileInfo(Sg_File_Info *fileInfo) {
  // The get_file_info() function maps to get_operator_position() for
  // SgExpression IR nodes and to get_startOfConstruct() for SgStatement and
  // some other IR nodes in SgSupport that have not yet been moved to be in
  // SgLocatedNode.

  bool isCompilerGenerated = (fileInfo->isCompilerGenerated() == true);
  bool isFrontendSpecific = (fileInfo->isFrontendSpecific() == true);
  bool isTransformation = (fileInfo->isTransformation() == true);
  bool isShared = (fileInfo->isShared() == true);
  bool isSourcePositionUnavailableInFrontend =
      (fileInfo->isSourcePositionUnavailableInFrontend() == true);

  if (isCompilerGenerated == false && isFrontendSpecific == false &&
      isShared == false && isSourcePositionUnavailableInFrontend == false &&
      isTransformation == false) {
    if (fileInfo->get_filenameString() == "") {
      SgNode *node = fileInfo->get_parent();
      ROSE_ASSERT(node != NULL);
      fprintf(stderr,
              "REX_AST_INVARIANT[source-position]: node=%p type=%s has an "
              "empty filename line=%d column=%d physical-id=%d\n",
              static_cast<void *>(node), node->class_name().c_str(),
              fileInfo->get_line(), fileInfo->get_col(),
              fileInfo->get_physical_file_id());
      ROSE_ABORT();
    }

    if (fileInfo->get_physical_file_id() < 0) {
      SgNode *node = fileInfo->get_parent();
      ROSE_ASSERT(node != NULL);
      fprintf(
          stderr,
          "REX_AST_INVARIANT[source-position]: node=%p type=%s "
          "name=%s file=%s line=%d column=%d physical-id=%d "
          "compiler-generated=%d frontend-specific=%d "
          "transformation=%d output=%d\n",
          static_cast<void *>(node), node->class_name().c_str(),
          isSgDeclarationStatement(node) != nullptr
              ? SageInterface::get_name(isSgDeclarationStatement(node)).c_str()
              : "",
          fileInfo->get_filenameString().c_str(), fileInfo->get_line(),
          fileInfo->get_col(), fileInfo->get_physical_file_id(),
          fileInfo->isCompilerGenerated(), fileInfo->isFrontendSpecific(),
          fileInfo->isTransformation(), fileInfo->isOutputInCodeGeneration());
      ROSE_ABORT();
    }
  }
}

void TestForSourcePosition::visit(SgNode *node) {
  Sg_File_Info *fileInfo = node->get_file_info();
  if (fileInfo != NULL) {
    testFileInfo(fileInfo);
  }

  SgLocatedNode *locatedNode = isSgLocatedNode(node);
  if (locatedNode != NULL) {
    // This is a redundant test (with get_file_info()) for SgStatement, but not
    // for SgExpression.
    Sg_File_Info *startOfConstruct = node->get_startOfConstruct();
    if (startOfConstruct != NULL) {
      testFileInfo(startOfConstruct);
    }

    Sg_File_Info *endOfConstruct = node->get_endOfConstruct();
    if (endOfConstruct != NULL) {
      testFileInfo(endOfConstruct);
    }

    // This is a redundant test (with get_file_info()) for SgExpression.
    SgExpression *expressionNode = isSgExpression(locatedNode);
    if (expressionNode != NULL) {
      Sg_File_Info *operatorPosition = expressionNode->get_operatorPosition();
      if (operatorPosition != NULL) {
        testFileInfo(operatorPosition);
      }
    }
  }
}

void TestForMultipleWaysToSpecifyRestrictKeyword::visit(SgNode *node) {
  // DQ (12/11/2012): Added new consistancy test.
  // This consistancy test only tests for consistant setting of the restrict
  // keyword.  But it might be useful to enforce consistant setting of both
  // "const" and volatile" keywords as well.  I'm not clear on this design
  // point.  There are notes about this in the
  // Unparse_ExprStmt::unparseMFuncDeclStmt() hnadling of the "restrict" keywork
  // (in the unparser).

  SgDeclarationStatement *declarationStatement = isSgDeclarationStatement(node);
  if (declarationStatement != NULL) {
    SgDeclarationModifier &declarationModifier =
        declarationStatement->get_declarationModifier();
    SgTypeModifier &typeModifierFromDeclarationModifier =
        declarationModifier.get_typeModifier();

    SgFunctionDeclaration *functionDeclaration =
        isSgFunctionDeclaration(declarationStatement);
    if (functionDeclaration != NULL) {
      SgType *type = functionDeclaration->get_type();
      ROSE_ASSERT(type != NULL);
      // DQ (12/11/2012): This should never be a SgModifierType.
      SgModifierType *modifierType = isSgModifierType(type);
      ROSE_ASSERT(modifierType == NULL);

      SgMemberFunctionType *memberFunctionType = isSgMemberFunctionType(type);
      if (memberFunctionType != NULL) {
        if (memberFunctionType->isRestrictFunc() !=
            typeModifierFromDeclarationModifier.isRestrict()) {
          printf("Error: restrict is not consistantly set in the two "
                 "SgTypeModifier IR nodes memberFunctionType->isRestrict() = "
                 "%s typeModifierFromDeclarationModifier.isRestrict() = %s \n",
                 memberFunctionType->isRestrictFunc() ? "true" : "false",
                 typeModifierFromDeclarationModifier.isRestrict() ? "true"
                                                                  : "false");
          ROSE_ABORT();
        }
      }
    }
  }
}

void TestAstForCyclesInTypedefs::test() {
  TestAstForCyclesInTypedefs t;
  SgTypedefType::traverseMemoryPoolNodes(t);
}

void TestAstForCyclesInTypedefs::visit(SgNode *node) {
  ROSE_ASSERT(node != NULL);
  SgTypedefType *currentType = isSgTypedefType(node);
  if (currentType != NULL) {
    SgModifierType *modifier = isSgModifierType(currentType->get_base_type());
    ROSE_ASSERT(modifier == NULL || modifier->get_base_type() != currentType);
  }
}
