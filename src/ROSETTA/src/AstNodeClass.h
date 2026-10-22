#ifndef __TERMINAL_H__
#define __TERMINAL_H__

#include "ROSETTA_macros.h"

#include "Rose/StringUtility/FileUtility.h"

#include "grammar.h"

#include <map>
#include <memory>
#include <optional>

#include <string>

class Grammar;
class GrammarString;
class AstNodeClass;

class SubclassListBuilder {
  std::vector<AstNodeClass *> children;

public:
  SubclassListBuilder() {}
  SubclassListBuilder &operator|(const AstNodeClass &t);
  const std::vector<AstNodeClass *> &getList() const;
};

class AstNodeClass {
public:
  std::string
      name; // The name we assign to the token (e.g. "BinaryOperatorEquals")

  std::string baseName;

  AstNodeClass *baseClass;

  // Subclasses of this AST node type
  std::vector<AstNodeClass *> subclasses;

  // Can there be objects of this type (as opposed to just subclasses)?
  bool canHaveInstances;

  std::string tag;

  std::string predeclarationString;
  std::string postdeclarationString;

  bool automaticGenerationOfDestructor;
  bool automaticGenerationOfConstructor;
  bool automaticGenerationOfCopyFunction;

  // Explicit cleanup operations for owned members with reciprocal ownership.
  std::map<std::string, std::string> memberDestructors;
  std::string destructorPrologue;

  Grammar *associatedGrammar;

  // Header file class definition function prototypes (local and subtree
  // definitions) Use an array of lists since every string has a local
  // node/subtree representation and a local node/subtree exclude list
  std::vector<GrammarString *> memberFunctionPrototypeList[2][2];

  // Header file class definition data declarations (local and subtree
  // definitions)
  std::vector<GrammarString *> memberDataPrototypeList[2][2];

  // Source code string lists (local and subtree definitions)
  std::vector<GrammarString *> memberFunctionSourceList[2][2];

  // Content that should appear before the start of the class definition, such
  // as CPP include directives
  std::string preDefinitionText;

  // A derived grammar node can narrow the traversal cardinality of an inherited
  // scalar edge without redeclaring the C++ data member.  This is part of the
  // typed schema: it is not a code-generation exception keyed on a node name.
  std::map<std::string, TraversalCardinalityEnum> traversalCardinalityOverrides;

  enum locationInTree { LOCAL_LIST = 0, SUBTREE_LIST = 1 };

  enum includeExcludeMechanism { INCLUDE_LIST = 0, EXCLUDE_LIST = 1 };

  std::vector<GrammarString *> &getMemberFunctionPrototypeList(int i,
                                                               int j) const;
  std::vector<GrammarString *> &getMemberDataPrototypeList(int i, int j) const;
  std::vector<GrammarString *> &getMemberFunctionSourceList(int i, int j) const;

  void setTraversalCardinalityOverride(
      const std::string &memberName,
      TraversalCardinalityEnum traversalCardinality);
  const std::map<std::string, TraversalCardinalityEnum> &
  getTraversalCardinalityOverrides() const;

  static void addElementToList(std::vector<GrammarString *> &list,
                               const GrammarString &element);

  // Storage of editing strings used for substitution of final output strings
  std::vector<GrammarString *> editSubstituteTargetList[2][2];
  std::vector<GrammarString *> editSubstituteSourceList[2][2];

  std::vector<GrammarString *> &getEditSubstituteTargetList(int i, int j) const;
  std::vector<GrammarString *> &getEditSubstituteSourceList(int i, int j) const;

  // Add text that should appear before the beginning of the class definition.
  void appendPreDefinitionText(const std::string &);

  void setFunctionPrototype(const GrammarString &memberFunction);
  void setFunctionPrototype(const std::string &markerString,
                            const std::string &filename);

  void setSubTreeFunctionPrototype(const GrammarString &memberFunction);
  void setSubTreeFunctionPrototype(const std::string &markerString,
                                   const std::string &filename);

  void setFunctionSource(const std::string &markerString,
                         const std::string &filename);
  void setSubTreeFunctionSource(const std::string &markerString,
                                const std::string &filename);

  // Mechanism for excluding code from specific node or subtrees
  void excludeFunctionPrototype(const std::string &markerString,
                                const std::string &filename);
  void excludeSubTreeFunctionPrototype(const std::string &markerString,
                                       const std::string &filename);

  void excludeFunctionSource(const std::string &markerString,
                             const std::string &filename);
  // Data is handled slightly differently, there are no subtree mechanisms
  // for handling data because data in base classes is by definition included
  // within derived classes.
  void setDataPrototype(const GrammarString &memberData);

  void setMemberDestructor(const std::string &member,
                           const std::string &operation);
  void setDestructorPrologue(const std::string &operation);

  void setDataPrototype(const std::string &inputTypeNameString,
                        const std::string &inputVariableNameString,
                        const std::string &defaultInitializer,
                        const ConstructParamEnum &constructorParameter,
                        const BuildAccessEnum &buildAccessFunctions,
                        const TraversalEnum &toBeTraversedDuringTreeTraversal,
                        const DeleteEnum &delete_flag,
                        const CopyConfigEnum &toBeCopied = COPY_DATA);

  void setDataPrototype(
      const std::string &inputTypeNameString,
      const std::string &inputVariableNameString,
      const std::string &defaultInitializer,
      const ConstructParamEnum &constructorParameter,
      const BuildAccessEnum &buildAccessFunctions,
      const TraversalEnum &toBeTraversedDuringTreeTraversal,
      const DeleteEnum &delete_flag, const CopyConfigEnum &toBeCopied,
      std::optional<TraversalCardinalityEnum> traversalCardinality,
      const TraversalAccessorEnum &traversalAccessor = DIRECT_TRAVERSAL_ACCESS,
      CopyConstructionOrderEnum copyConstructionOrder =
          DEFAULT_COPY_CONSTRUCTION_ORDER);

  virtual ~AstNodeClass();

protected:
  std::unique_ptr<GrammarString> setupMarkerStrings(std::string markerString,
                                                    std::string fileName);

public:
  AstNodeClass(const std::string &lexeme, Grammar &X,
               const std::string &stringVar, const std::string &tagString,
               bool canHaveInstances = true,
               const SubclassListBuilder &childList = SubclassListBuilder());

  const std::string &getName() const;
  const std::string &getTagName() const;
  const std::string &getBaseName() const;

  void setGrammar(Grammar *grammarPointer);
  Grammar *getGrammar() const;

  void setBaseClass(AstNodeClass *baseClassPointer);
  AstNodeClass *getBaseClass() const;

  bool getCanHaveInstances() const;

  void addGrammarPrefixToName();

  std::string buildDataAccessFunctions(const GrammarString &inputMemberData);

  std::string buildConstructorBody();

  std::string buildDestructorBody();
  std::string buildDestructorOwnershipBody();

  // this stores the old and new string used for the substitution and applies
  // it before final copying out of a node of the grammar to a file!
  void editSubstitute(const std::string &oldString,
                      const std::string &newString);
  void editSubstituteSubTree(const std::string &oldString,
                             const std::string &newString);

  void editSubstitute(const std::string &oldString,
                      const std::string &markerName,
                      const std::string &filename);
  void editSubstituteSubTree(const std::string &oldString,
                             const std::string &markerName,
                             const std::string &filename);

  void setPredeclarationString(const std::string &declarationString);
  void setPostdeclarationString(const std::string &declarationString);
  void setPredeclarationString(const std::string &markerString,
                               const std::string &filename);
  void setPostdeclarationString(const std::string &markerString,
                                const std::string &filename);

  const std::string &getPredeclarationString() const;
  const std::string &getPostdeclarationString() const;

  void setAutomaticGenerationOfConstructor(bool X = true);
  void setAutomaticGenerationOfDestructor(bool X = true);
  void setAutomaticGenerationOfCopyFunction(bool X = true);

  bool generateDestructor() const;
  bool generateConstructor() const;

  Rose::StringUtility::FileWithLineNumbers buildCopyMemberFunctionSource();
  Rose::StringUtility::FileWithLineNumbers buildCopyMemberFunctionHeader();

  std::string buildReturnDataMemberPointers();
  std::string buildListIteratorString(const GrammarString &data);

  std::string buildProcessDataMemberReferenceToPointers();
  std::string
  buildListIteratorStringForReferenceToPointers(const GrammarString &data,
                                                bool traverse);

  void consistencyCheck() const;
};

#endif
