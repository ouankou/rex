#ifndef __ROSETTA_GRAMMAR_H__
#define __ROSETTA_GRAMMAR_H__

#include "fileoffsetbits.h"
#include "rose_paths.h"

#include <stdio.h>

#include <stdlib.h>

#include <assert.h>

#include "Rose/StringUtility/FileUtility.h"

#include "ROSETTA_macros.h"

#include <map>

#include <set>

#include <sstream>

#include <string>

#include <vector>

// Switch to control whether CxxGrammarMetaProgram is noisy on standard output.
extern bool verbose;

/*
 ROSETTA substitution variables: used in *.code files:
 $GRAMMAR_NAME
 $GRAMMAR_PREFIX_
 $CLASSNAME
 $BASECLASS
 $BASE_CLASS_CONSTRUCTOR_CALL
 $BASE_CLASS_CONSTRUCTOR_PARAMETER
 $CONSTRUCTOR_PARAMETER_LIST
 $CONSTRUCTOR_BODY
 $CLASSTAG
 $CLASS_BASE_NAME
 $PURE_VIRTUAL_MARKER
 $MARKER // see enum $MARKERVariants
 $MARKER_UNKNOWN_GRAMMAR
 $MARKER_LAST_TAG
 $LIST_LENGTH // see TerminalNamesType $MARKERTerminalNames[$LIST_LENGTH] =
 $MEMBER_FUNCTION_DEFINITIONS
 $CODE_STRING
 $ASSOCIATED_MEMORY_POOL_TEST

 $IFDEF_MARKER_H
 $IFDEF_MARKER

*/

// MS 2015: rewrote ROSETTA to use enums instead of wrapper classes around
// enums. enums: ConstructParamEnum, BuildAccessEnum, CopyConfigEnum
enum ConstructParamEnum { NO_CONSTRUCTOR_PARAMETER, CONSTRUCTOR_PARAMETER };

enum BuildAccessEnum {
  NO_ACCESS_FUNCTIONS,
  BUILD_ACCESS_FUNCTIONS,
  // Typed literal mutations invalidate the original source spelling.
  BUILD_LITERAL_VALUE_ACCESS_FUNCTIONS,
  // Just like TAG_BUILD_ACCESS_FUNCTIONS except doesn't set p_isModified
  BUILD_FLAG_ACCESS_FUNCTIONS,
  BUILD_LIST_ACCESS_FUNCTIONS
};

enum CopyConfigEnum {
  /* This skips the generation of any code to copy the
     pointer (deep or shallow)
  */
  NO_COPY_DATA,
  /* This copies the data (if a pointer this copies the
     pointer, else calls the operator= for any object)
  */
  COPY_DATA,
  /* This copies the data by building a new object using
     the copy constructor
  */
  CLONE_PTR,
  /* This builds a new object dependent on the use of the
     SgCopyHelp input object (deep or shallow) using the object's copy member
     function
  */
  CLONE_TREE,
  // Bind a borrowed AST edge before copying dependent owned children.
  COPY_REFERENCE,
  // Resolve a constructor reference before publishing the copied node.
  COPY_CONSTRUCTOR_REFERENCE
};

enum CopyConstructionOrderEnum {
  DEFAULT_COPY_CONSTRUCTION_ORDER,
  COPY_BEFORE_CONSTRUCTOR_DEPENDENCIES,
  COPY_AFTER_OWNER_CONSTRUCTION,
  COPY_BEFORE_OTHER_MEMBERS,
};

enum TraversalEnum {
  DEF_TRAVERSAL,
  NO_TRAVERSAL,
};

enum TraversalCardinalityEnum {
  REQUIRED_TRAVERSAL_MEMBER,
  OPTIONAL_TRAVERSAL_MEMBER,
};

enum TraversalAccessorEnum {
  DIRECT_TRAVERSAL_ACCESS,
  COMPUTED_BASE_TYPE_DECLARATION_ACCESS,
  COMPUTED_CLASS_DEFINITION_ACCESS,
};

enum TraversalStorageEnum {
  NOT_A_TRAVERSAL_MEMBER,
  NODE_POINTER_TRAVERSAL_MEMBER,
  NODE_POINTER_CONTAINER_TRAVERSAL_MEMBER,
};

enum SchemaStorageEnum {
  SCALAR_SCHEMA_STORAGE,
  VALUE_CONTAINER_SCHEMA_STORAGE,
  POINTER_CONTAINER_SCHEMA_STORAGE,
};

enum SchemaElementEnum {
  VALUE_SCHEMA_ELEMENT,
  POINTER_SCHEMA_ELEMENT,
};

enum DeleteEnum { DEF_DELETE, NO_DELETE, DELETE_IF_OWNED };

class AstNodeClass;
class SubclassListBuilder;
class GrammarString;

AstNodeClass *lookupTerminal(const std::vector<AstNodeClass *> &tl,
                             const std::string &name);

class Grammar {
public:
  // Pointer to member function
  typedef std::vector<GrammarString *> &(AstNodeClass::*FunctionPointerType)(
      int i, int j) const;

  // Pointer to member function
  typedef std::string (
      GrammarString::*StringGeneratorFunctionPointerType)() const;

  Grammar(const Grammar &X);

  /**! The constructor builds a grammar.  The inputs are strings:
       inputGrammarName          -- The name of the grammar (used in the
     construction of filenames etc.) inputPrefixName           -- The name used
     for the prefix to all classes generated to implement the grammar.  This
     permits each class to have a unique name.  The prefix for the base level
     grammar should be "Sg" to match SAGE!
   */
  Grammar(const std::string &inputGrammarName,
          const std::string &inputPrefixName, const std::string &t_directory,
          const std::string &smallHeadersDir);
  ~Grammar();

public:
  std::vector<AstNodeClass *> terminalList;
  std::map<std::string, size_t> astNodeToVariantMap;
  std::map<size_t, std::string> astVariantToNodeMap;
  std::map<size_t, AstNodeClass *> astVariantToTerminalMap;

  // The directory name we should generate the files to
  std::string target_directory;

  // The optional directory for small headers
  std::string smallHeadersDir;

  AstNodeClass *rootNode;

  // filename of file containing declaration and source code for a grammar's
  // supporting classes
  std::string filenameForGlobalDeclarations;

  static void editStringList(std::vector<GrammarString *> &origList,
                             const std::vector<GrammarString *> &excludeList);

  std::string buildStringFromLists(
      AstNodeClass &node, FunctionPointerType listFunction,
      StringGeneratorFunctionPointerType stringGeneratorFunction);

  std::vector<GrammarString *>
  buildListFromLists(AstNodeClass &node, FunctionPointerType listFunction);

  void setRootOfGrammar(AstNodeClass *RootNodeForGrammar);
  AstNodeClass *getRootOfGrammar();

public:
  void consistencyCheck() const;

  // builds the header files and source code
  void buildCode();

  std::string getFilenameForGlobalDeclarations();
  void setFilenameForGlobalDeclarations(const std::string &filename);

  Rose::StringUtility::FileWithLineNumbers buildMiscSupportDeclarations();

  std::string grammarName;
  std::string grammarPrefixName;

  const std::string &getGrammarName() const;
  const std::string &getGrammarPrefixName() const;

  void addGrammarElement(AstNodeClass &X);

  // Get the variant for a node name
  size_t getVariantForNode(const std::string &name) const;
  size_t getVariantForTerminal(const AstNodeClass &name) const;
  // Get the node name for a variant, asserting if not found
  AstNodeClass &getTerminalForVariant(size_t var);

  std::string getDerivedClassDeclaration(AstNodeClass &node);

  Rose::StringUtility::FileWithLineNumbers
  buildHeaderStringAfterMarker(const std::string &marker,
                               const std::string &fileName);
  Rose::StringUtility::FileWithLineNumbers
  buildHeaderStringBeforeMarker(const std::string &marker,
                                const std::string &fileName);

  static Rose::StringUtility::FileWithLineNumbers
  readFileWithPos(const std::string &inputFileName);

  void writeFile(const Rose::StringUtility::FileWithLineNumbers &outputString,
                 const std::string &directoryName, const std::string &className,
                 const std::string &fileExtension);

  std::string generateNumberOfSuccessorsComputation(
      std::vector<GrammarString *> &traverseDataMemberList);
  std::string generateTraverseSuccessor(GrammarString *gs,
                                        std::string successorContainerName,
                                        const std::string &nodeName,
                                        TraversalCardinalityEnum cardinality);
  std::string
  generateTraverseSuccessorNames(GrammarString *gs,
                                 std::string successorContainerName,
                                 TraversalCardinalityEnum cardinality);

  std::string generateRTICode(GrammarString *gs,
                              std::string successorContainerName,
                              std::string className, size_t index);
  void buildRTIFile(AstNodeClass *rootNode,
                    Rose::StringUtility::FileWithLineNumbers &rttiFile);
  Rose::StringUtility::FileWithLineNumbers buildVariants();
  void emitForwardDeclarations(std::ostream &) const;
  Rose::StringUtility::FileWithLineNumbers buildForwardDeclarations();

  void buildVariantsStringPrototype(
      Rose::StringUtility::FileWithLineNumbers &outputFile);
  void buildVariantsStringDataBase(
      Rose::StringUtility::FileWithLineNumbers &outputFile);

  void buildClassDefinition(AstNodeClass &,
                            Rose::StringUtility::FileWithLineNumbers &);
  void buildHeaderFiles(AstNodeClass &node,
                        Rose::StringUtility::FileWithLineNumbers &outputFile);

  std::string buildStringForPrototypes(AstNodeClass &node);
  Rose::StringUtility::FileWithLineNumbers
  buildStringForDataDeclaration(AstNodeClass &node);
  Rose::StringUtility::FileWithLineNumbers
  buildStringForDataAccessFunctionDeclaration(AstNodeClass &node);
  Rose::StringUtility::FileWithLineNumbers
  buildStringForSource(AstNodeClass &node);

  // Builds the "CLASSNAME::isCLASSNAME()" friend function
  Rose::StringUtility::FileWithLineNumbers
  supportForBuildStringForIsClassNameFunctionSource(
      AstNodeClass &node,
      const Rose::StringUtility::FileWithLineNumbers &accumulationString);
  Rose::StringUtility::FileWithLineNumbers
  buildStringForIsClassNameFunctionSource(AstNodeClass &node);

  Rose::StringUtility::FileWithLineNumbers
  buildStringForNewAndDeleteOperatorSource(AstNodeClass &node);

  Rose::StringUtility::FileWithLineNumbers
  buildStringForTraverseMemoryPoolSource(AstNodeClass &node);

  Rose::StringUtility::FileWithLineNumbers
  buildStringForReturnDataMemberPointersSource(AstNodeClass &node);

  Rose::StringUtility::FileWithLineNumbers
  buildStringForProcessDataMemberReferenceToPointersSource(AstNodeClass &node);

  void buildConstructorParameterList(
      AstNodeClass &node,
      std::vector<GrammarString *> &constructorParameterList);

  std::string buildConstructorParameterListString(AstNodeClass &node,
                                                  bool withInitializers,
                                                  bool withTypes);

  void
  markNodeForConstructorWithoutSourcePositionInformation(AstNodeClass &node);
  void markNodeForConstructorWithoutSourcePositionInformationSupport(
      AstNodeClass &node);
  Rose::StringUtility::FileWithLineNumbers
  buildConstructorWithoutSourcePositionInformation(AstNodeClass &node);
  void buildConstructorWithoutSourcePositionInformationSupport(
      AstNodeClass &node, Rose::StringUtility::FileWithLineNumbers &outputFile);
  void
  constructorLoopBody(const Rose::StringUtility::FileWithLineNumbers
                          &constructorSourceCodeTemplate,
                      AstNodeClass &node,
                      Rose::StringUtility::FileWithLineNumbers &returnString);

  AstNodeClass *getNamedNode(AstNodeClass &node, const std::string &name);
  GrammarString *getNamedDataMember(AstNodeClass &node);

  Rose::StringUtility::FileWithLineNumbers
  buildDataMemberVariableDeclarations(AstNodeClass &node);
  Rose::StringUtility::FileWithLineNumbers
  buildMemberAccessFunctionPrototypesAndConstuctorPrototype(AstNodeClass &node);

  Rose::StringUtility::FileWithLineNumbers buildConstructor(AstNodeClass &node);
  Rose::StringUtility::FileWithLineNumbers
  buildCopyMemberFunctionSource(AstNodeClass &node);

  void buildSourceFiles(AstNodeClass &node,
                        Rose::StringUtility::FileWithLineNumbers &outputFile);

  void buildNewAndDeleteOperators(
      AstNodeClass &node, Rose::StringUtility::FileWithLineNumbers &outputFile);

  void buildTraverseMemoryPoolSupport(
      AstNodeClass &node, Rose::StringUtility::FileWithLineNumbers &outputFile);

  void buildStringForReturnDataMemberPointersSupport(
      AstNodeClass &node, Rose::StringUtility::FileWithLineNumbers &outputFile);

  void buildStringForProcessDataMemberReferenceToPointersSupport(
      AstNodeClass &node, Rose::StringUtility::FileWithLineNumbers &outputFile);

  void buildCopyMemberFunctions(
      AstNodeClass &node, Rose::StringUtility::FileWithLineNumbers &outputFile);

  // Uses list of targets and sources stored within each node to drive
  // substitutions
  Rose::StringUtility::FileWithLineNumbers
  editSubstitution(AstNodeClass &node,
                   const Rose::StringUtility::FileWithLineNumbers &editString);

  static Rose::StringUtility::FileWithLineNumbers
  extractStringFromFile(const std::string &startMarker,
                        const std::string &endMarker,
                        const std::string &filename);

  AstNodeClass &nonTerminalConstructor(const std::string &lexeme, Grammar &X,
                                       const std::string &stringVar,
                                       const std::string &tagString,
                                       const SubclassListBuilder &builder,
                                       bool canHaveInstances = false);
  AstNodeClass &terminalConstructor(const std::string &lexeme, Grammar &X,
                                    const std::string &stringVar = "",
                                    const std::string &tagString = "");

  // These functions build the C++ grammar
  void setUpTypes();
  void setUpSymbols();
  void setUpExpressions();
  void setUpStatements();
  void setUpSupport();
  void setUpNodes();

  void generateRTIImplementation(AstNodeClass &node, std::string &source);

  // MS: generates the code to implement the creation of the
  // treeTraversalSuccessorContainer in Sage
  void buildTreeTraversalFunctions(
      AstNodeClass &node, Rose::StringUtility::FileWithLineNumbers &outputFile);

  // MS: create source code for enums used in traversal functions to access
  // synthesized attribute values
  void buildEnumForNode(AstNodeClass &node, std::string &enumString);

  // MS: build Variants for use in tree traversal (access with getVariantT())
  std::string buildVariantEnums();

  // PC: build ReferenceToPointerHandler and related classes
  std::string buildReferenceToPointerHandlerCode();

  // Support for table-driven SgXXX node castability.

  // Generates a table (classHierarchyCatTable) populated with information
  // about whether a given SgXXX node can be casted to a SgYYY node.
  // The technique has constant lookup time.
  std::string generateClassHierarchyCastTable();

  // Populates the classHierarchyCastTable with all the types that can be casted
  // to AstNodeClass type
  void buildClassHierarchyCastTable(
      AstNodeClass *astNodeClass,
      std::vector<AstNodeClass *> &myParentsDescendents);

  // Gets the number of rows in classHierarchyCastTable
  size_t getRowsInClassHierarchyCastTable();

  // Gets the number of columns in classHierarchyCastTable
  size_t getColumnsInClassHierarchyCastTable();

  std::string buildClassHierarchySubTreeFunction();

  std::string buildMemoryPoolBasedVariantVectorTraversalSupport();

  std::string buildVariantEnumNames();

  std::vector<GrammarString *> classMemberIncludeList(AstNodeClass &node);

  std::string buildMemoryStorageEvaluationSupport();

  std::string buildVisitorBaseClass();

  std::string buildMemoryPoolBasedTraversalSupport();

private:
  static std::map<std::string, Rose::StringUtility::FileWithLineNumbers>
      fileCache;
};

#endif // endif for ROSETTA_GRAMMAR_H
