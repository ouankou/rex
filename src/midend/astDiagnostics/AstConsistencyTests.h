// Author: Markus Schordan
// $Id: AstConsistencyTests.h,v 1.8 2008/01/25 02:25:46 dquinlan Exp $

#ifndef ASTTESTS_H
#define ASTTESTS_H

#include <iostream>
#include <list>
#include <map>
#include <set>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>

#include "AstStatistics.h"

#include "rosedll.h"

#include "AstProcessing.h"

class TestAstPropertiesSA {
public:
  TestAstPropertiesSA() : val(true), node(NULL) {}
  bool val;
  SgNode *node;
};

class TestAstProperties : public AstBottomUpProcessing<TestAstPropertiesSA> {
  //! Propagate subtree consistency and validate each node.

private:
  // DQ (8/3/2004): This list is used to record and report any IR nodes that
  // have missing file info the file info object is present, but the information
  // is it is default information (e.g. filename = NULL_FILE, line = 0, column =
  // 0)
  std::list<SgNode *> listOfNodesWithoutValidFileInfo;
  std::list<SgNode *> listOfNodesFileInfo;
  std::map<std::tuple<int, int, int, int, int, int, unsigned int, unsigned int>,
           SgDeclarationStatement *>
      directSourceSurfaceOwners;

public:
  // DQ (10/24/2004): Track count of nodes without proper ending file info
  // object
  int nodeWithoutFileInfoFrequencyCount[V_SgNumVariants];

  TestAstProperties();

private:
  TestAstPropertiesSA
  evaluateSynthesizedAttribute(SgNode *node, SubTreeSynthesizedAttributes l);
};

class ROSE_DLL_API AstTests {
public:
  // Validate actual successor cardinality and identity. Dynamic containers
  // and scalar edges may coexist; their names do not encode AST validity.
  static void validateTraversalSuccessors(SgNode *node);

  //! Test codes that traverse the AST
  static void runAllTests(SgProject *sageProject);
  static bool isCorrectAst(SgProject *sageProject);
};

// DQ (3/30/2004): Added tests on templates!
// class TestAstTemplateProperties : public AstPreOrderTraversal
class TestAstTemplateProperties : public AstSimpleProcessing {
  // This class uses a traversal to test properties of template
  // specific IR nodes in the AST.
public:
  void visit(SgNode *node);
};

class TestAstForUniqueStatementsInScopes : public AstSimpleProcessing {
  // This class uses a traversal to test properties of AST.
  // We look for redundent entries in any single scope (not
  // redundent entries in the AST!).
public:
  void visit(SgNode *node);
};

// Structural traversal edges must visit each node once. Shared semantic type
// references are not structural children and are outside this check.
class TestAstForUniqueNodesInAST : public AstSimpleProcessing {
  std::set<SgNode *> astNodeSet;

public:
  void visit(SgNode *node);

  static void test(SgNode *node);
};

// Check structural uniqueness, including expression lists built by SageBuilder.
void testAstForUniqueNodes(SgNode *node);

class TestAstForProperlyMangledNames : public AstSimpleProcessing {
public:
  unsigned long saved_maxMangledNameSize;
  unsigned long saved_totalMangledNameSize;
  unsigned long saved_numberOfMangledNames;
  void visit(SgNode *node);

  TestAstForProperlyMangledNames();
};

class TestAstForProperlySetDefiningAndNondefiningDeclarations
    : public AstSimpleProcessing {
  // This class uses a traversal to test the values of the definingDeclaration
  // and firstNondefiningDeclaration pointers in each SgDeclarationStatement.
  // See code for details, since both of these pointers are not always set.

public:
  void visit(SgNode *node);
};

class TestAstSymbolTables : public AstSimpleProcessing {
  // This class uses a traversal to test properties of symbol tables
  // (global function type symbol table and local symbol tables in each scope).

public:
  void visit(SgNode *node);
};

class TestAstAccessToDeclarations : public AstSimpleProcessing {
  // This class uses a traversal to test the get_declaration() member function
  // on each IR node where relavant.  The goal is to verify that each member
  // function, were appropriate, returns a valid declaration.

public:
  //! static function to do test on any IR node
  static void test(SgNode *node);

  //! visit function required for traversal
  void visit(SgNode *node);
};

class TestExpressionTypes : public AstSimpleProcessing {
  // This class uses a traversal to test the get_type() member function on each
  // SgExpression IR node. The goal is to verify that each member function, were
  // appropriate, returns a valid type.

public:
  //! static function to do test on any IR node
  // static void test(SgNode* node);

  // DQ (10/31/2016): Added constructor so that we could test using
  // set_useDefaultIndexBasedTraversal(false);
  TestExpressionTypes();

  //! visit function required for traversal
  void visit(SgNode *node);
};

class TestMangledNames : public ROSE_VisitTraversal {
public:
  virtual ~TestMangledNames() {};
  static void test();

  unsigned long saved_maxMangledNameSize;
  unsigned long saved_totalMangledNameSize;
  unsigned long saved_numberOfMangledNames;
  unsigned long totalLongMangledNameSize;
  unsigned long totalNumberOfLongMangledNames;

  TestMangledNames();

  void visit(SgNode *node);
};

// DQ (6/26/2006): Added test of parents of IR nodes using memory pool!
// void testParentPointersInMemoryPool();

/*! \brief This traversal uses the Memory Pool traversal to test parent
   pointers.

    This traversal uses the Memory Pool traversal to test parent pointers
    (e.g. declarations that are hidden from the AST traversal). This traversal
   traverses the whole AST using the memory pool traversal.
 */
class TestParentPointersInMemoryPool : public ROSE_VisitTraversal {
public:
  virtual ~TestParentPointersInMemoryPool() {};
  //! static function to do test on any IR node
  static void test();

  //! Required traversal function
  void visit(SgNode *node);
};

/*! \brief This traversal uses the Memory Pool traversal to test parent
   pointers.

    This traversal uses the Memory Pool traversal to test parent pointers
    (e.g. declarations that are hidden from the AST traversal). This traversal
   traverses the whole AST using the memory pool traversal.
 */
class TestChildPointersInMemoryPool : public ROSE_VisitTraversal {
  struct ParentEdge {
    SgNode *parent;
    SgNode *child;
    size_t order;
  };
  std::vector<ParentEdge> edges;

  static void reportMissingChild(SgNode *node, SgNode *parent);

public:
  virtual ~TestChildPointersInMemoryPool() {};
  //! static function to do test on any IR node
  static void test();

  virtual void visit(SgNode *);
};

/*! \brief This tests the first non-defining, referenced from each declaration,
   to make sure that it is not a forward declaration (a non-defining declaration
   marked to be a forward declaration).

    \internal This is an issue specific to name qualification.  For functions,
   there are different types of non-defining declarations.  Those marked forward
   are represented in the AST traversal (the AST as defined by the traversal)
   and cannot be shared, while those marked as non-forward are sharable
   non-defining declarations.  Even the forward non-defining declarations are
   not all the same (non created equal) since those appearing in scopes where
   the associated defining declaration could be placed are \bstronger and force
   name qualification to be used subsequently (those appearing before this
   stronger sort fo forward declaration are not allowed to have name
   qualification.  This behavior is most dramatic for functions, but applied in
   what seems to be a slightly reduced form to classes and structs.  This needs
   to be tested!
 */
class TestFirstNondefiningDeclarationsForForwardMarking
    : public ROSE_VisitTraversal {
public:
  virtual ~TestFirstNondefiningDeclarationsForForwardMarking() {};
  //! static function to do test on any IR node
  static void test();

  virtual void visit(SgNode *);
};

class TestLValueExpressions : public AstSimpleProcessing {
  // This class uses a traversal to test expressions that should be marked
  // as lvalues and makes sure that other expressions are not marked as lvalues.

public:
  //! static function to do test on any IR node
  //  static void test(SgNode* node);

  //! visit function required for traversal
  void visit(SgNode *node);
};

class TestForProperLanguageAndSymbolTableCaseSensitivity_InheritedAttribute
    : AstInheritedAttribute {
public:
  // Use the source file as a way to report better quality errors.
  SgSourceFile *sourceFile;

  // This will be set as we encounter the SgSourceFile at the top of the AST
  // within the traversal.
  bool caseInsensitive;

  // Required constructor.
  TestForProperLanguageAndSymbolTableCaseSensitivity_InheritedAttribute(bool b);

  // Required copy constructor.
  TestForProperLanguageAndSymbolTableCaseSensitivity_InheritedAttribute(
      const TestForProperLanguageAndSymbolTableCaseSensitivity_InheritedAttribute
          &X);
  TestForProperLanguageAndSymbolTableCaseSensitivity_InheritedAttribute &
  operator=(
      const TestForProperLanguageAndSymbolTableCaseSensitivity_InheritedAttribute
          &) = default;
};

class TestForProperLanguageAndSymbolTableCaseSensitivity
    : public AstTopDownProcessing<
          TestForProperLanguageAndSymbolTableCaseSensitivity_InheritedAttribute> {
  // DQ (11/28/2010): This class is part of a test to verify consistancy of
  // symbol table case sensitivity with languge.  C/C++ codes should use only
  // case sensitive symbol tables, while Fortran codes should use only case
  // insensitive symbol table handling.

public:
  // Overloaded pure virtual function.
  TestForProperLanguageAndSymbolTableCaseSensitivity_InheritedAttribute
  evaluateInheritedAttribute(
      SgNode *node,
      TestForProperLanguageAndSymbolTableCaseSensitivity_InheritedAttribute
          inheritedAttribute);

  // Simple funtion to call to get the traversal started (sets up the inherited
  // attribute, etc.).
  static void test(SgNode *node);
};

class TestForReferencesToDeletedNodes : public ROSE_VisitTraversal {
  // DQ (9/26/2011): This class is part of a test to verify consistancy of
  // the AST by verifying that no IR node referenced in the AST is a SgNode.
  // Note that in a proper AST all IR nodes are derived from an SgNode, but
  // that there should be no IR nodes in the AST that are only SgNode types.
  // It can happen that a referenced IR nodes will only be able to report
  // that it is a SgNode, when this happens it is generally because it is
  // and dangling pointer to an IR node that was deleted (at which point
  // it can no longer be identified by type to be more than an SgNode).
  // So this is a test for references in the AST to IR nodes that have been
  // deleted.

private:
  int detect_dangling_pointers;
  std::string filename;

public:
  TestForReferencesToDeletedNodes(int input_detect_dangling_pointers,
                                  const std::string &s);

  // Overloaded pure virtual function.
  void visit(SgNode *node);

  // Simple funtion to call to get the traversal started...
  static void test(SgProject *project);
};

class TestForParentsMatchingASTStructure : public AstPrePostProcessing {
  // Compare structural traversal edges with each node's recorded parent.
  // check() reports validity to its caller; test() aborts on any mismatch.
  // The traversal path is cleared on every exit so a checker can be reused.

public:
  std::vector<SgNode *> stack; // current path within the AST
#ifndef USE_ROSE
  // DQ (3/6/2013): Disable code that is a problem for SWIG (vesion 2.0.9).
  // This data member is a problem for SWIG, so ignore it when
  // processing using USE_ROSE which we define when using SWIG.
  std::ostream &output; // where to emit warning/error messages
#endif
  size_t nproblems;   // number of problems detected
  size_t limit;       // number of errors to allow before exit
  std::string prefix; // line prefix

public:
  explicit TestForParentsMatchingASTStructure(std::ostream &output,
                                              const std::string &prefix = "");
  bool check(SgNode *ast, size_t limit = 0);
  void preOrderVisit(SgNode *node);
  void postOrderVisit(SgNode *node);
  void show_details_and_maybe_fail(SgNode *node);

  // Simple funtion to call to get the traversal started...
  static void test(SgProject *project);
};

class TestForSourcePosition : public AstSimpleProcessing {
  // DQ (12/3/2012): This tests for Sg_File_Info objects that have an empty
  // filename. These have been set at an early stage in development and now they
  // are a problem.  so it is time to detect them and get rid of them.

public:
  void testFileInfo(Sg_File_Info *fileInfo);

  void visit(SgNode *node);
};

class TestForMultipleWaysToSpecifyRestrictKeyword : public AstSimpleProcessing {
public:
  void visit(SgNode *node);
};

class TestAstForCyclesInTypedefs : public ROSE_VisitTraversal {
public:
  virtual ~TestAstForCyclesInTypedefs() {};

  void visit(SgNode *node);

  static void test();
};

#endif
