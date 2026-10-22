// ################################################################
// #                           Header Files                       #
// ################################################################

#include "AstNodeClass.h"

#include "featureTests.h"

#include "grammar.h"

#include "grammarString.h"

#include "Rose/FileSystem.h"
#include "mlog.h"

#include <cctype>

#include <fstream>

#include <iostream>

#include <map>

#include <algorithm>

#include "rose_config.h"

#include <sstream>
#include <utility>

using namespace std;
using namespace Rose;

// MS: temporary (TODO: move to processing class)
static string RTIContainerName =
    "rtiContainer"; // put this into the respective processing class as private
                    // member
static string RTIreturnType = "RTIReturnType"; // typedef in Grammar/Common.code

// ################################################################
// #                 Grammar Static Data Members                  #
// ################################################################

map<string, StringUtility::FileWithLineNumbers> Grammar::fileCache;

// ################################################################
// #                   Grammar Member Functions                   #
// ################################################################

Grammar::~Grammar() {
  for (AstNodeClass *node : terminalList) {
    delete node;
  }
  terminalList.clear();
  astVariantToTerminalMap.clear();
  astNodeToVariantMap.clear();
  astVariantToNodeMap.clear();
  fileCache.clear();
}

Grammar::Grammar(const string &inputGrammarName, const string &inputPrefixName,
                 const string &t_directory, const string &smallHeadersDir)
    : smallHeadersDir(smallHeadersDir) {

  target_directory = t_directory;

  // Principle constructor
  grammarName = inputGrammarName;
  grammarPrefixName = inputPrefixName;

  // JJW 2-12-2008 Use a file for this list so the numbers will be more stable
  {

    std::string astNodeListFilename =
        std::string(ROSE_SOURCE_TREE) + "/src/ROSETTA/astNodeList";
    std::ifstream astNodeList(astNodeListFilename.c_str());
    ROSE_ASSERT(astNodeList.is_open());
    size_t c = 1;
    std::string name;
    while (astNodeList >> name) {
      const auto inserted = astNodeToVariantMap.emplace(name, c);
      ROSE_ASSERT(inserted.second);
      astVariantToNodeMap.emplace(c, name);
      ++c;
    }
    ROSE_ASSERT(!astNodeList.bad());
    ROSE_ASSERT(astNodeList.eof());
  }
  ROSE_ASSERT(this->astNodeToVariantMap.size() >= 10); // A reasonable count

  // Build up the terminals and nonTerminals defined within the default C++
  // grammar (using SAGE)
  setUpSupport();
  setUpTypes();
  setUpStatements();
  setUpExpressions();
  setUpSymbols();

  // Setup of Node requires previous definition of types,
  // expressions, statements, symbols within the grammar
  setUpNodes();

  // Specify additional global declarations required for this grammar
  setFilenameForGlobalDeclarations("../Grammar/Cxx_GlobalDeclarations.macro");

  // Check the consistency of the data that we just built
  consistencyCheck();
}

void Grammar::consistencyCheck() const {
  // Call the consistencyCheck function on the list object
  for (size_t i = 0; i < terminalList.size(); ++i) {
    terminalList[i]->consistencyCheck();
  }
}

void Grammar::setRootOfGrammar(AstNodeClass *RootNodeForGrammar) {
  rootNode = RootNodeForGrammar;
}

AstNodeClass *Grammar::getRootOfGrammar() { return rootNode; }

void Grammar::addGrammarElement(AstNodeClass &X) {
  X.setGrammar(this);
  const auto inserted =
      astVariantToTerminalMap.emplace(getVariantForTerminal(X), &X);
  ROSE_ASSERT(inserted.second);
  terminalList.push_back(&X);
}

const std::string &Grammar::getGrammarPrefixName() const {
  // This function returns the name of the grammar and is used to substitute for
  // "$GRAMMAR_PREFIX_" in the name of the generated classes that represent the
  // terminals and nonterminals of the grammar.
  return grammarPrefixName;
}

const std::string &Grammar::getGrammarName() const {
  // This function returns the name of the grammar and is used to substitute for
  // "$GRAMMAR_PREFIX_" in the name of the generated classes that represent the
  // terminals and nonterminals of the grammar.
  return grammarName;
}

/**********************************
 * AstNodeClass/Nonterminal functions *
 **********************************/

AstNodeClass &Grammar::terminalConstructor(const string &lexeme, Grammar &X,
                                           const string &stringVar,
                                           const string &tagString) {
  // These functions build AstNodeClass and nonterminal objects to be associated
  // with this grammar Using a member function to construct these serves several
  // purposes: 1) organizes terminals and nonterminals with there respective
  // grammar (without ambiguity) 2) avoids or deferes the implementation of the
  // envelop/letter interface mechanism so
  //    that the letter will have a scope longer than the envelope

  AstNodeClass *t = new AstNodeClass(lexeme, X, stringVar, tagString, true);
  ROSE_ASSERT(t);
  return *(t);
}

AstNodeClass &Grammar::nonTerminalConstructor(
    const string &lexeme, Grammar &X, const string &stringVar,
    const string &tagString, const SubclassListBuilder &builder,
    bool canHaveInstances) {
  // These functions build AstNodeClass and nonterminal objects to be associated
  // with this grammar Using a member function to construct these serves several
  // purposes: 1) organizes terminals and nonterminals with there respective
  // grammar (without ambiguity) 2) avoids or deferes the implementation of the
  // envelop/letter interface mechanism so
  //    that the letter will have a scope longer than the envelope

  AstNodeClass *nt = new AstNodeClass(lexeme, X, stringVar, tagString,
                                      canHaveInstances, builder);
  ROSE_ASSERT(nt);
  return *(nt);
}

StringUtility::FileWithLineNumbers
Grammar::readFileWithPos(const string &inputFileName) {
  auto cached = fileCache.find(inputFileName);
  if (cached == fileCache.end()) {
    auto content = StringUtility::readFileWithPos(inputFileName);
    cached = fileCache.emplace(inputFileName, std::move(content)).first;
  }
  return cached->second;
}

namespace {
void writeGeneratedFile(const std::string &filename,
                        const std::string &content) {
  // A generator relink does not imply an AST change. Preserve unchanged output
  // timestamps so build systems do not recompile every consumer after
  // configure.
  if (std::filesystem::exists(filename) &&
      Rose::FileSystem::readFile<std::string>(filename) == content) {
    return;
  }
  std::ofstream output;
  output.exceptions(std::ios::failbit | std::ios::badbit);
  output.open(filename, std::ios::binary | std::ios::trunc);
  ROSE_ASSERT(output.good());
  output << content;
  ROSE_ASSERT(output.good());
  output.close();
  ROSE_ASSERT(output.good());
}
} // namespace

void Grammar::writeFile(const StringUtility::FileWithLineNumbers &outputString,
                        const string &directoryName, const string &className,
                        const string &fileExtension) {
  const string outputFilename =
      (directoryName == "." ? "" : directoryName + "/") + className +
      fileExtension;
  std::string content;
  for (const auto &fragment : outputString)
    content += fragment.str + '\n';
  writeGeneratedFile(outputFilename, content);
}

static void
collectInheritedGrammarStrings(AstNodeClass &node,
                               vector<GrammarString *> &includeList,
                               vector<GrammarString *> &excludeList,
                               Grammar::FunctionPointerType listFunction,
                               AstNodeClass::locationInTree location) {
  if (auto *base = node.getBaseClass())
    collectInheritedGrammarStrings(*base, includeList, excludeList,
                                   listFunction, location);
  const auto &includes =
      (node.*listFunction)(location, AstNodeClass::INCLUDE_LIST);
  const auto &excludes =
      (node.*listFunction)(location, AstNodeClass::EXCLUDE_LIST);
  includeList.insert(includeList.end(), includes.begin(), includes.end());
  excludeList.insert(excludeList.end(), excludes.begin(), excludes.end());
}

void Grammar::editStringList(vector<GrammarString *> &targetList,
                             const vector<GrammarString *> &excludeList) {
  vector<GrammarString *> newList;
  for (vector<GrammarString *>::const_iterator i = targetList.begin();
       i != targetList.end(); ++i) {
    for (vector<GrammarString *>::const_iterator j = i + 1;
         j != targetList.end(); ++j) {
      if (**i == **j)
        goto skipThisElement;
    }
    for (vector<GrammarString *>::const_iterator j = excludeList.begin();
         j != excludeList.end(); ++j) {
      if (**i == **j)
        goto skipThisElement;
    }
    newList.push_back(*i);
  skipThisElement:;
  }
  targetList.swap(newList);
}

string Grammar::buildStringFromLists(
    AstNodeClass &node, FunctionPointerType listFunction,
    StringGeneratorFunctionPointerType stringGeneratorFunction) {
  // This method builds a string (according to the parameter
  // stringGeneratorFunction) from the local lists of the current node, from all
  // of its parents' subtree lists, and from its own subtree lists

  vector<GrammarString *> sourceList = buildListFromLists(node, listFunction);
  vector<GrammarString *>::iterator sourceListIterator;

  string editStringMiddle;

  for (sourceListIterator = sourceList.begin();
       sourceListIterator != sourceList.end(); sourceListIterator++) {
    GrammarString &memberFunction = **sourceListIterator;
    string memberFunctionString = "// Start of memberFunctionString\n" +
                                  (memberFunction.*stringGeneratorFunction)() +
                                  "\n// End of memberFunctionString\n";
    editStringMiddle += memberFunctionString;
  }

  return editStringMiddle;
}

vector<GrammarString *>
Grammar::buildListFromLists(AstNodeClass &node,
                            FunctionPointerType listFunction)
// This method builds a list from the local lists of the current node,
// from all of its parents' subtree lists, and from its own subtree lists
{
  vector<GrammarString *> includeList;
  vector<GrammarString *> excludeList;

  // Initialize with local node data
  includeList = (node.*listFunction)(AstNodeClass::LOCAL_LIST,
                                     AstNodeClass::INCLUDE_LIST);
  excludeList = (node.*listFunction)(AstNodeClass::LOCAL_LIST,
                                     AstNodeClass::EXCLUDE_LIST);

  // Now generate the additions to the lists from the parent node subtree lists
  // and the subtree lists of the current node
  if (node.getBaseClass())
    collectInheritedGrammarStrings(*node.getBaseClass(), includeList,
                                   excludeList, listFunction,
                                   AstNodeClass::SUBTREE_LIST);

  // Now edit the list to remove elements appearing within the exclude list
  editStringList(includeList, excludeList);

  return includeList;
}

string Grammar::buildStringForPrototypes(AstNodeClass &node) {
  // This function adds in the source code specific to a node in the
  // tree that represents the hierachy of the grammer's implementation.

  return buildStringFromLists(node,
                              &AstNodeClass::getMemberFunctionPrototypeList,
                              &GrammarString::getRawString);
}

StringUtility::FileWithLineNumbers
Grammar::supportForBuildStringForIsClassNameFunctionSource(
    AstNodeClass &node,
    const StringUtility::FileWithLineNumbers &accumulationStringOrig) {
  // This function forms support for the
  // Grammar::buildStringForIsClassNameFunctionSource function. If a node is a
  // part of the subtree represented by this grammar then is is by definition of
  // the type represented by the root of the subtree.

  StringUtility::FileWithLineNumbers accumulationString =
      accumulationStringOrig;

  string tempString = "               case ";

  tempString += node.getTagName();
  tempString += ":\n";
  accumulationString.push_back(StringUtility::StringWithLineNumber(
      tempString, "" /* "<supportForBuildStringForIsClassNameFunctionSource on "
                        + node.getToken().getTagName() + ">" */
      ,
      1));

  vector<AstNodeClass *>::iterator nodeListIterator;
  // Loop through the children
  for (nodeListIterator = node.subclasses.begin();
       nodeListIterator != node.subclasses.end(); nodeListIterator++) {
    ROSE_ASSERT((*nodeListIterator) != NULL);
    ROSE_ASSERT((*nodeListIterator)->getBaseClass() == &node);

    accumulationString = supportForBuildStringForIsClassNameFunctionSource(
        **nodeListIterator, accumulationString);
  }

  return accumulationString;
}

StringUtility::FileWithLineNumbers
Grammar::buildStringForIsClassNameFunctionSource(AstNodeClass &node) {
  // This function builds the source code for a friend function, each class has
  // a member function that casts a pointer to any derived class to type
  // represented by this level of the subtree  (e.g. at the Statement level of
  // the grammar the function would cast a pointer to any type of function to
  // the Statement base class type).

  string isClassNameFunctionTemplateFileName =
      "../Grammar/grammarIsClassNameFunctionDefinitionMacros.macro";
  StringUtility::FileWithLineNumbers returnString =
      readFileWithPos(isClassNameFunctionTemplateFileName);

  StringUtility::FileWithLineNumbers accumulationString =
      supportForBuildStringForIsClassNameFunctionSource(
          node, StringUtility::FileWithLineNumbers());
  returnString = StringUtility::copyEdit(returnString, "$ROOT_NODE_OF_GRAMMAR",
                                         getRootOfGrammar()->getName());
  returnString = StringUtility::copyEdit(returnString, "$ACCUMULATION_STRING",
                                         accumulationString);
  return returnString;
}

StringUtility::FileWithLineNumbers
Grammar::buildStringForNewAndDeleteOperatorSource(AstNodeClass &node) {
  string isClassNameFunctionTemplateFileName =
      "../Grammar/grammarNewDeleteOperatorMacros.macro";
  StringUtility::FileWithLineNumbers returnString =
      readFileWithPos(isClassNameFunctionTemplateFileName);

  returnString =
      StringUtility::copyEdit(returnString, "$CLASSNAME", node.getName());

  return returnString;
}

void Grammar::buildNewAndDeleteOperators(
    AstNodeClass &node, StringUtility::FileWithLineNumbers &outputFile) {
  StringUtility::FileWithLineNumbers editString =
      buildStringForNewAndDeleteOperatorSource(node);

  outputFile += editString;

  // Call this function recursively on the children of this node in the tree
  vector<AstNodeClass *>::iterator treeNodeIterator;
  for (treeNodeIterator = node.subclasses.begin();
       treeNodeIterator != node.subclasses.end(); treeNodeIterator++) {
    ROSE_ASSERT((*treeNodeIterator) != NULL);
    ROSE_ASSERT((*treeNodeIterator)->getBaseClass() != NULL);

    buildNewAndDeleteOperators(**treeNodeIterator, outputFile);
  }
}

StringUtility::FileWithLineNumbers
Grammar::buildStringForTraverseMemoryPoolSource(AstNodeClass &node) {
  string isClassNameFunctionTemplateFileName =
      "../Grammar/grammarTraverseMemoryPool.macro";
  StringUtility::FileWithLineNumbers returnString =
      readFileWithPos(isClassNameFunctionTemplateFileName);

  returnString =
      StringUtility::copyEdit(returnString, "$CLASSNAME", node.getName());

  return returnString;
}

void Grammar::buildTraverseMemoryPoolSupport(
    AstNodeClass &node, StringUtility::FileWithLineNumbers &outputFile) {
  StringUtility::FileWithLineNumbers editString =
      buildStringForTraverseMemoryPoolSource(node);

  outputFile += editString;

  // Call this function recursively on the children of this node in the tree
  vector<AstNodeClass *>::iterator treeNodeIterator;
  for (treeNodeIterator = node.subclasses.begin();
       treeNodeIterator != node.subclasses.end(); treeNodeIterator++) {
    ROSE_ASSERT((*treeNodeIterator) != NULL);
    ROSE_ASSERT((*treeNodeIterator)->getBaseClass() != NULL);

    buildTraverseMemoryPoolSupport(**treeNodeIterator, outputFile);
  }
}

StringUtility::FileWithLineNumbers
Grammar::buildStringForReturnDataMemberPointersSource(AstNodeClass &node) {
  // DQ & JH (1/17/2006): Added support for testing data members pointers if
  // they point to IR nodes

  string isClassNameFunctionTemplateFileName =
      "../Grammar/grammarReturnDataMemberPointers.macro";
  StringUtility::FileWithLineNumbers returnString =
      readFileWithPos(isClassNameFunctionTemplateFileName);

  string dataMemberSpecificString = node.buildReturnDataMemberPointers();

  returnString = StringUtility::copyEdit(returnString, "$CODE_STRING",
                                         dataMemberSpecificString.c_str());
  returnString =
      StringUtility::copyEdit(returnString, "$CLASSNAME", node.getName());
  returnString =
      StringUtility::copyEdit(returnString, "$GRAMMAR_PREFIX_", "Sg");

  return returnString;
}

void Grammar::buildStringForReturnDataMemberPointersSupport(
    AstNodeClass &node, StringUtility::FileWithLineNumbers &outputFile) {
  StringUtility::FileWithLineNumbers editString =
      buildStringForReturnDataMemberPointersSource(node);

  outputFile += editString;

  // Call this function recursively on the children of this node in the tree
  vector<AstNodeClass *>::iterator treeNodeIterator;
  for (treeNodeIterator = node.subclasses.begin();
       treeNodeIterator != node.subclasses.end(); treeNodeIterator++) {
    ROSE_ASSERT((*treeNodeIterator) != NULL);
    ROSE_ASSERT((*treeNodeIterator)->getBaseClass() != NULL);

    buildStringForReturnDataMemberPointersSupport(**treeNodeIterator,
                                                  outputFile);
  }
}

StringUtility::FileWithLineNumbers
Grammar::buildStringForProcessDataMemberReferenceToPointersSource(
    AstNodeClass &node) {
  // DQ & JH (1/17/2006): Added support for testing data members pointers if
  // they point to IR nodes

  string isClassNameFunctionTemplateFileName =
      "../Grammar/grammarProcessDataMemberReferenceToPointers.macro";
  StringUtility::FileWithLineNumbers returnString =
      readFileWithPos(isClassNameFunctionTemplateFileName);

  string dataMemberSpecificString =
      node.buildProcessDataMemberReferenceToPointers();

  returnString = StringUtility::copyEdit(returnString, "$CODE_STRING",
                                         dataMemberSpecificString.c_str());
  returnString =
      StringUtility::copyEdit(returnString, "$CLASSNAME", node.getName());
  returnString =
      StringUtility::copyEdit(returnString, "$GRAMMAR_PREFIX_", "Sg");

  return returnString;
}

void Grammar::buildStringForProcessDataMemberReferenceToPointersSupport(
    AstNodeClass &node, StringUtility::FileWithLineNumbers &outputFile) {
  StringUtility::FileWithLineNumbers editString =
      buildStringForProcessDataMemberReferenceToPointersSource(node);

  outputFile += editString;

  // Call this function recursively on the children of this node in the tree
  vector<AstNodeClass *>::iterator treeNodeIterator;
  for (treeNodeIterator = node.subclasses.begin();
       treeNodeIterator != node.subclasses.end(); treeNodeIterator++) {
    ROSE_ASSERT((*treeNodeIterator) != NULL);
    ROSE_ASSERT((*treeNodeIterator)->getBaseClass() != NULL);

    buildStringForProcessDataMemberReferenceToPointersSupport(
        **treeNodeIterator, outputFile);
  }
}

StringUtility::FileWithLineNumbers
Grammar::buildStringForSource(AstNodeClass &node) {
  string beginString =
      buildStringFromLists(node, &AstNodeClass::getMemberFunctionSourceList,
                           &GrammarString::getRawString);

  StringUtility::FileWithLineNumbers isClassnameFunctionDefinition =
      buildStringForIsClassNameFunctionSource(node);

  StringUtility::FileWithLineNumbers returnString =
      StringUtility::FileWithLineNumbers(
          1, StringUtility::StringWithLineNumber(beginString, "", 1)) +
      isClassnameFunctionDefinition;

  return returnString;
}

StringUtility::FileWithLineNumbers
Grammar::buildStringForDataDeclaration(AstNodeClass &node) {
  string returnString =
      buildStringFromLists(node, &AstNodeClass::getMemberDataPrototypeList,
                           &GrammarString::getDataPrototypeString);
  return StringUtility::FileWithLineNumbers(
      1, StringUtility::StringWithLineNumber(returnString, "", 1));
}

bool generate_override_keyword(AstNodeClass &node, GrammarString &data) {
  bool returnResult = true;

  string variableNameString = string(data.variableNameString);
  string nodeName = node.baseName;

  if ((nodeName == "InitializedName" && variableNameString == "scope") ||
      (nodeName == "TemplateParameter" && variableNameString == "type") ||
      (nodeName == "TemplateArgument" && variableNameString == "type") ||
      (nodeName == "EnumDeclaration" && variableNameString == "type") ||
      (nodeName == "TypedefDeclaration" && variableNameString == "type") ||
      (nodeName == "ClassDeclaration" && variableNameString == "type") ||
      (nodeName == "FunctionDeclaration" && variableNameString == "type") ||
      (nodeName == "SymbolTable" && variableNameString == "name") ||
      (nodeName == "Attribute" && variableNameString == "name") ||
      (nodeName == "Directory" && variableNameString == "name") ||
      (nodeName == "Graph" && variableNameString == "name") ||
      (nodeName == "GraphNode" && variableNameString == "name") ||
      (nodeName == "GraphEdge" && variableNameString == "name") ||
      (nodeName == "TemplateType" && variableNameString == "name") ||
      (nodeName == "TypeDefault" && variableNameString == "name") ||
      (nodeName == "TypeLabel" && variableNameString == "name") ||
      (nodeName == "InitializedName" && variableNameString == "name") ||
      (nodeName == "EnumDeclaration" && variableNameString == "name") ||
      (nodeName == "TemplateDeclaration" && variableNameString == "name") ||
      (nodeName == "UseStatement" && variableNameString == "name") ||
      (nodeName == "NamespaceDeclarationStatement" &&
       variableNameString == "name") ||
      (nodeName == "InterfaceStatement" && variableNameString == "name") ||
      (nodeName == "NamespaceAliasDeclarationStatement" &&
       variableNameString == "name") ||
      (nodeName == "TypedefDeclaration" && variableNameString == "name") ||
      (nodeName == "ClassDeclaration" && variableNameString == "name") ||
      (nodeName == "FunctionDeclaration" && variableNameString == "name") ||
      (nodeName == "InquireStatement" && variableNameString == "name") ||
      (nodeName == "OmpCriticalStatement" && variableNameString == "name") ||
      (nodeName == "EnumVal" && variableNameString == "name") ||
      (nodeName == "AsmOp" && variableNameString == "name") ||
      (nodeName == "TypeTraitBuiltinOperator" &&
       variableNameString == "name")) {
    returnResult = false;
  }

  return returnResult;
}

bool generate_override_keyword_for_set_functions(AstNodeClass &node,
                                                 GrammarString &data) {
  if (!generate_override_keyword(node, data))
    return false;
  const string &member = data.variableNameString;
  const string &kind = node.baseName;
  if ((member == "startOfConstruct" || member == "endOfConstruct") &&
      (kind == "Pragma" || kind == "File" || kind == "LocatedNode"))
    return false;
  if (member == "type" && (kind == "FunctionTypeSymbol" ||
                           kind == "DefaultSymbol" || kind == "TypeExpression"))
    return false;
  return !(kind == "FunctionTypeSymbol" && member == "name");
}

StringUtility::FileWithLineNumbers
Grammar::buildStringForDataAccessFunctionDeclaration(AstNodeClass &node) {
  vector<GrammarString *> dataMemberList =
      buildListFromLists(node, &AstNodeClass::getMemberDataPrototypeList);

  StringUtility::FileWithLineNumbers returnString;
  vector<GrammarString *>::iterator dataMemberIterator;

  for (dataMemberIterator = dataMemberList.begin();
       dataMemberIterator != dataMemberList.end(); dataMemberIterator++) {
    GrammarString &data = **dataMemberIterator;

    string codeString = data.getDataAccessFunctionPrototypeString();

    bool use_override_keyword = generate_override_keyword(node, data);
    if (use_override_keyword == false) {
      codeString =
          StringUtility::copyEdit(codeString, " $ROSE_OVERRIDE_GET", "");
    }

    bool use_override_keyword_for_set_functions =
        generate_override_keyword_for_set_functions(node, data);
    if (use_override_keyword_for_set_functions == false) {
      codeString =
          StringUtility::copyEdit(codeString, " $ROSE_OVERRIDE_SET", "");
    }

    // And surviving references to $ROSE_OVERRIDE_GET and $ROSE_OVERRIDE_SET
    // should be edited to be override.
    codeString =
        StringUtility::copyEdit(codeString, " $ROSE_OVERRIDE_GET", " override");
    codeString =
        StringUtility::copyEdit(codeString, " $ROSE_OVERRIDE_SET", " override");

    StringUtility::FileWithLineNumbers tempString(
        1,
        StringUtility::StringWithLineNumber(
            codeString, "" /* "<getDataAccessFunctionPrototypeString>" */, 1));
    returnString += tempString;
  }

  return returnString;
}

void Grammar::buildConstructorParameterList(
    AstNodeClass &node, vector<GrammarString *> &constructorParameterList) {
  // This function is called by the buildConstructorParameterListString(node)
  // function and builds the list of parameters that are used by a constructor.
  // The data exclusiion mechanism implies that excluded data within parents
  // will not appear in the constructor parameters of child grammar elements.

  // The input list shoud be empty
  ROSE_ASSERT(constructorParameterList.size() == 0);

  // Now build the constructor using the data specified at the node and its
  // parents! Use an exclusion mechanism to exclude data before being used
  // within the constructor parameter list (use flag in GrammarString).

  vector<GrammarString *> includeList = classMemberIncludeList(node);
  vector<GrammarString *>::iterator gIt;

  for (gIt = includeList.begin(); gIt != includeList.end(); gIt++) {
    GrammarString *memberFunctionCopy = *gIt;
    ROSE_ASSERT(memberFunctionCopy != NULL);

    if (memberFunctionCopy->getIsInConstructorParameterList() ==
        CONSTRUCTOR_PARAMETER) {
      constructorParameterList.push_back(memberFunctionCopy);
    }
  }
}

string Grammar::buildConstructorParameterListString(AstNodeClass &node,
                                                    bool withInitializers,
                                                    bool withTypes) {
  // This function returns the string used to build the parameters within the
  // constructor.
  int i = 0;
  vector<GrammarString *> constructorParameterList;
  vector<GrammarString *>::iterator stringListIterator;

  buildConstructorParameterList(node, constructorParameterList);

  // Build the parameter string for the constructor
  string constructorParameterString;

  int listSize = constructorParameterList.size();
  // Put the constructor paramteres into the parameter string
  for (stringListIterator = constructorParameterList.begin();
       stringListIterator != constructorParameterList.end();
       stringListIterator++) {
    GrammarString &constructorParameter = **stringListIterator;
    string tempConstructorParameterString = "";
    if (withTypes == true) {
      if (withInitializers == true)
        tempConstructorParameterString =
            constructorParameter.getConstructorPrototypeParameterString();
      else
        tempConstructorParameterString =
            constructorParameter.getConstructorSourceParameterString();
    } else {
      ROSE_ASSERT(withInitializers == false);
      tempConstructorParameterString =
          constructorParameter.getBaseClassConstructorSourceParameterString();
    }

    constructorParameterString += tempConstructorParameterString;

    // If there is another parameter to add in then separate them with a ","
    if (i < listSize - 1) {
      // BP : 10/24/2001, delete extra memory
      constructorParameterString += ", ";
    }
    i++;
  }

  return constructorParameterString;
}

StringUtility::FileWithLineNumbers
Grammar::buildDataMemberVariableDeclarations(AstNodeClass &node) {
  // This function builds a single string containing:
  //    1) Data prototype  (e.g. "int data; $Data* someSageData;")

  // This builds the data declaration (the easy part) e.g. "int dataField; \n
  // char* charField; \n" Spaces and CR's have been added to simplify the final
  // formatting
  StringUtility::FileWithLineNumbers result;
  result.push_back(StringUtility::StringWithLineNumber(
      "    protected:", "" /* "<buildDataMemberVariableDeclarations on " +
                              node.getToken().getName() + ">" */
      ,
      1));
  result += buildStringForDataDeclaration(node);

  return result;
}

StringUtility::FileWithLineNumbers
Grammar::buildMemberAccessFunctionPrototypesAndConstuctorPrototype(
    AstNodeClass &node) {

  // This function builds a single string containing:
  //    1) Data Access function prototypes (e.g. "void set_data( int data ); int
  //    get_data(void); ..." ) 2) Constructor prototype (e.g. "$CLASSNAME ( data
  //    = 0, $Data* someSageData = NULL );" )
  StringUtility::FileWithLineNumbers dataAccessFunctionPrototypeString =
      buildStringForDataAccessFunctionDeclaration(node);

  string className = node.getName();

  // Build the constructor prototype and then edit the names!
  dataAccessFunctionPrototypeString.push_back(
      StringUtility::StringWithLineNumber(
          "\n public: virtual void appendDestructorOwnedNodes("
          "std::vector<SgNode*>& nodes) const" +
              std::string(node.getBaseClass() != nullptr ? " override" : "") +
              ";\n",
          "", 1));
  string destructorPrototype =
      "\n     public: \n         virtual ~" + string(className) + "();\n";

  if (node.generateDestructor() == true)
    dataAccessFunctionPrototypeString.push_back(
        StringUtility::StringWithLineNumber(destructorPrototype,
                                            "" /* "<destructor>" */, 1));

  // Now build the constructor and put in the constructorParameterString
  if (node.generateConstructor() == true) {
    string constructorPrototype = "\n     public: \n";
    bool withInitializers = true;
    bool withTypes = true;

    // Get the SgLocatedNode so that we can set the data member as not being a
    // constructor parameter so that we can reuse the same code generation
    // source code.
    AstNodeClass *parentNode = getNamedNode(node, "SgLocatedNode");
    if (parentNode != NULL) {
      GrammarString *returnValue = getNamedDataMember(*parentNode);
      ROSE_ASSERT(returnValue != NULL);

      // DQ (11/7/2006): Mark it temporarily as NOT a constructor parameter.
      string defaultInitializer = returnValue->getDefaultInitializerString();
      returnValue->defaultInitializerString = "";

      string constructorParameterString_1 = buildConstructorParameterListString(
          node, withInitializers, withTypes);
      constructorPrototype = constructorPrototype + "         " +
                             string(className) + "(" +
                             constructorParameterString_1 + "); \n";

      // Reset "withInitializers" to false and generate a new string for the
      // constructor parameters.
      withInitializers = false;

      // DQ (11/7/2006): Mark it temporarily as NOT a constructor parameter.
      returnValue->setIsInConstructorParameterList(NO_CONSTRUCTOR_PARAMETER);

      string constructorParameterString_2 = buildConstructorParameterListString(
          node, withInitializers, withTypes);
      constructorPrototype = constructorPrototype + "         " +
                             string(className) + "(" +
                             constructorParameterString_2 + "); \n";

      // DQ (11/7/2006): Turn it back on as a constructor parameter (and reset
      // the defaultInitializerString)
      returnValue->setIsInConstructorParameterList(CONSTRUCTOR_PARAMETER);
      returnValue->defaultInitializerString = defaultInitializer;
    } else {
      // If not a SgLocatedNode then output the normal constructor prototype
      // (with all the default arguments).
      string constructorParameterString = buildConstructorParameterListString(
          node, withInitializers, withTypes);
      constructorPrototype = constructorPrototype + "         " +
                             string(className) + "(" +
                             constructorParameterString + "); \n";
    }

    dataAccessFunctionPrototypeString.push_back(
        StringUtility::StringWithLineNumber(constructorPrototype,
                                            "" /* "<constructor>" */, 1));
  }

  return dataAccessFunctionPrototypeString;
}

void Grammar::constructorLoopBody(
    const StringUtility::FileWithLineNumbers &constructorSourceCodeTemplate,
    AstNodeClass &node, StringUtility::FileWithLineNumbers &returnString) {
  StringUtility::FileWithLineNumbers constructorSource =
      constructorSourceCodeTemplate;
  if (node.getBaseClass() != NULL) {
    string parentClassName = node.getBaseClass()->getName();
    string baseClassParameterString;
    bool withInitializers = false;
    bool withTypes = false;
    baseClassParameterString = buildConstructorParameterListString(
        *node.getBaseClass(), withInitializers, withTypes);
    string preInitializationString =
        parentClassName + "($BASECLASS_PARAMETERS)";
    preInitializationString = ": " + preInitializationString;
    preInitializationString = StringUtility::copyEdit(preInitializationString,
                                                      "$BASECLASS_PARAMETERS",
                                                      baseClassParameterString);
    constructorSource = StringUtility::copyEdit(
        constructorSource, "$PRE_INITIALIZATION_LIST", preInitializationString);
  } else {
    constructorSource = StringUtility::copyEdit(constructorSource,
                                                "$PRE_INITIALIZATION_LIST", "");
  }

  bool withInitializers = false;
  bool withTypes = true;
  string constructorParameterString =
      buildConstructorParameterListString(node, withInitializers, withTypes);
  constructorSource =
      StringUtility::copyEdit(constructorSource, "$CONSTRUCTOR_PARAMETER_LIST",
                              constructorParameterString);
  constructorSource =
      StringUtility::copyEdit(constructorSource, "$CLASSNAME", node.getName());

  string constructorFunctionBody = node.buildConstructorBody();
  constructorSource = StringUtility::copyEdit(
      constructorSource, "$CONSTRUCTOR_BODY", constructorFunctionBody);

  constructorSource = StringUtility::copyEdit(
      constructorSource, "$CONSTRUCTOR_ESSENTIAL_DATA_MEMBERS", "");

  returnString.insert(returnString.end(), constructorSource.begin(),
                      constructorSource.end());
}

StringUtility::FileWithLineNumbers
Grammar::buildConstructor(AstNodeClass &node) {
  string className = node.getName();

  StringUtility::FileWithLineNumbers returnString;

  returnString.push_back(StringUtility::StringWithLineNumber(
      "void " + className +
          "::appendDestructorOwnedNodes(std::vector<SgNode*>& nodes) const "
          "{\n" +
          node.buildDestructorOwnershipBody() + "}\n",
      "", 1));

  if (node.generateDestructor() == true) {
    // Build the string representing the constructor text (with macro variables)
    string destructorTemplateFileName =
        "../Grammar/grammarDestructorDefinitionMacros.macro";
    StringUtility::FileWithLineNumbers destructorSourceCodeTemplate =
        readFileWithPos(destructorTemplateFileName);

    // edit the string to customize it for this node in the grammar!
    StringUtility::FileWithLineNumbers destructorSource =
        StringUtility::copyEdit(destructorSourceCodeTemplate, "$CLASSNAME",
                                className);

    // For now make the descructor function body empty
    // AJ (10/27/2004) - Added the destructor body generation
    string destructorFunctionBody = node.buildDestructorBody();
    destructorSource = StringUtility::copyEdit(
        destructorSource, "$DESTRUCTOR_BODY", destructorFunctionBody);

    returnString.insert(returnString.end(), destructorSource.begin(),
                        destructorSource.end());
  }

  if (node.generateConstructor() == true) {
    string constructorTemplateFileName =
        "../Grammar/grammarConstructorDefinitionMacros.macro";
    StringUtility::FileWithLineNumbers constructorSourceCodeTemplate =
        readFileWithPos(constructorTemplateFileName);

    constructorLoopBody(constructorSourceCodeTemplate, node, returnString);
  }

  return returnString;
}

StringUtility::FileWithLineNumbers
Grammar::buildCopyMemberFunctionSource(AstNodeClass &node) {
  // This function builds the copy function within each class defined by the
  // grammar
  StringUtility::FileWithLineNumbers returnString =
      node.buildCopyMemberFunctionSource();

  returnString =
      StringUtility::copyEdit(returnString, "$CLASSNAME", node.getName());
  returnString = StringUtility::copyEdit(returnString, "$GRAMMAR_PREFIX_",
                                         getGrammarPrefixName());

  return returnString;
}

void Grammar::buildCopyMemberFunctions(
    AstNodeClass &node, StringUtility::FileWithLineNumbers &outputFile) {
  StringUtility::FileWithLineNumbers editString =
      buildCopyMemberFunctionSource(node);

  outputFile += editString;

  // Call this function recursively on the children of this node in the tree
  vector<AstNodeClass *>::iterator treeNodeIterator;
  for (treeNodeIterator = node.subclasses.begin();
       treeNodeIterator != node.subclasses.end(); treeNodeIterator++) {
    ROSE_ASSERT((*treeNodeIterator) != NULL);
    ROSE_ASSERT((*treeNodeIterator)->getBaseClass() != NULL);

    buildCopyMemberFunctions(**treeNodeIterator, outputFile);
  }
}

string Grammar::getDerivedClassDeclaration(AstNodeClass &node) {
  string derivedClassString;

  if (node.getBaseClass() != NULL)
    derivedClassString = string(": public ") + node.getBaseClass()->getName();

  return derivedClassString;
}

StringUtility::FileWithLineNumbers
Grammar::buildHeaderStringBeforeMarker(const string &marker,
                                       const string &fileName) {
  string headerFileInsertionSeparator = marker;
  StringUtility::FileWithLineNumbers headerFileTemplate =
      readFileWithPos(fileName);

  for (unsigned int i = 0; i < headerFileTemplate.size(); ++i) {
    std::string::size_type pos =
        headerFileTemplate[i].str.find(headerFileInsertionSeparator);
    if (pos != string::npos) {
      headerFileTemplate.erase(headerFileTemplate.begin() + i + 1,
                               headerFileTemplate.end());
      headerFileTemplate[i].str = headerFileTemplate[i].str.substr(0, pos);
      headerFileTemplate.insert(
          headerFileTemplate.begin(),
          StringUtility::StringWithLineNumber(
              "", "" /* "<before output of buildHeaderStringBeforeMarker " +
                        marker + " " + fileName + ">" */
              ,
              1));
      headerFileTemplate.insert(
          headerFileTemplate.end(),
          StringUtility::StringWithLineNumber(
              "", "" /* "<after output of buildHeaderStringBeforeMarker " +
                        marker + " " + fileName + ">" */
              ,
              1));
      return headerFileTemplate;
    }
  }
  ROSE_ASSERT(!"Marker not found");

  // Unreachable
  return headerFileTemplate;
}

StringUtility::FileWithLineNumbers
Grammar::buildHeaderStringAfterMarker(const string &marker,
                                      const string &fileName) {
  string headerFileInsertionSeparator = marker;
  StringUtility::FileWithLineNumbers headerFileTemplate =
      readFileWithPos(fileName);

  for (unsigned int i = 0; i < headerFileTemplate.size(); ++i) {
    std::string::size_type pos =
        headerFileTemplate[i].str.find(headerFileInsertionSeparator);
    if (pos != string::npos) {
      headerFileTemplate.erase(headerFileTemplate.begin(),
                               headerFileTemplate.begin() + i);
      headerFileTemplate[0].str = headerFileTemplate[0].str.substr(
          pos + headerFileInsertionSeparator.size());
      headerFileTemplate.insert(
          headerFileTemplate.begin(),
          StringUtility::StringWithLineNumber(
              "", "" /* "<before output of buildHeaderStringAfterMarker " +
                        marker + " " + fileName + ">" */
              ,
              1));
      headerFileTemplate.insert(
          headerFileTemplate.end(),
          StringUtility::StringWithLineNumber(
              "", "" /* "<after output of buildHeaderStringAfterMarker " +
                        marker + " " + fileName + ">" */
              ,
              1));
      return headerFileTemplate;
    }
  }
  ROSE_ASSERT(!"Marker not found");

  // Unreachable
  return headerFileTemplate;
}

void Grammar::buildClassDefinition(
    AstNodeClass &node, StringUtility::FileWithLineNumbers &outputFile) {
  string marker = "MEMBER_FUNCTION_DECLARATIONS";
  string fileName = "../Grammar/grammarClassDeclarationMacros.macro";

  StringUtility::FileWithLineNumbers headerBeforeInsertion;
  headerBeforeInsertion << node.preDefinitionText;
  headerBeforeInsertion += buildHeaderStringBeforeMarker(marker, fileName);
  StringUtility::FileWithLineNumbers headerAfterInsertion =
      buildHeaderStringAfterMarker(marker, fileName);

  // DQ (3/24/2006): Have this be generated from the CommonCode.code file
  // so that we can better control how the documentation is done.
  // Here is where the virtual copy function is added to the header file!
  StringUtility::FileWithLineNumbers copyString =
      node.buildCopyMemberFunctionHeader();

  headerBeforeInsertion += copyString;

  // Edit the $CLASSNAME
  string className = node.getName();

  string derivedClassString = getDerivedClassDeclaration(node);

  // Likely this must happen here since the substitution for BASECLASS is
  // different within the calls to StringUtility::copyEdit() now centralized in
  // editSubstitution() This should be fixed!
  StringUtility::FileWithLineNumbers editStringStart = StringUtility::copyEdit(
      headerBeforeInsertion, "$BASECLASS", derivedClassString);

  // calls to StringUtility::copyEdit() now centralized in editSubstitution()
  // BP : 10/24/2001, keep track of memory being freed
  editStringStart =
      StringUtility::copyEdit(editStringStart, "$CLASSNAME", className);
  StringUtility::FileWithLineNumbers editStringEnd =
      StringUtility::copyEdit(headerAfterInsertion, "$CLASSNAME", className);

  StringUtility::FileWithLineNumbers editedStringMiddle;

  // Each of these functions should return a null terminated string
  // (even if there are no code strings associated with this node).

  StringUtility::FileWithLineNumbers editStringMiddleNodeMemberFunctions(
      1, StringUtility::StringWithLineNumber(
             buildStringForPrototypes(node),
             "" /* "<buildStringForPrototypes " + node.getToken().getName() +
                   ">" */
             ,
             1));

  editedStringMiddle += editStringMiddleNodeMemberFunctions;

  // DQ (3/24/2006): Separated these functions so that we could position the
  // data member variable declaration at the base of the class and the generated
  // access functions at the top.  This permist us to present the documentation
  // better. string
  // buildDataPrototypesAndAccessFunctionPrototypesAndConstuctorPrototype (
  // GrammarTreeNode & node );

  // Using the data prototypes we also want to build the constructor
  // call (with the data types and variables as prototypes)
  StringUtility::FileWithLineNumbers
      editStringMiddleNodeMemberFunctionsPrototypes =
          buildMemberAccessFunctionPrototypesAndConstuctorPrototype(node);

  editedStringMiddle += editStringMiddleNodeMemberFunctionsPrototypes;

  // DQ (3/24/2006): Add the data members to the end of the class in the
  // generated code.
  StringUtility::FileWithLineNumbers editStringMiddleNodeData =
      buildDataMemberVariableDeclarations(node);
  editedStringMiddle += editStringMiddleNodeData;

  // increment the final string with the node specific string
  StringUtility::FileWithLineNumbers editedHeaderFileStringTemp =
      editStringStart + editedStringMiddle + editStringEnd;

  // Specification of declarations that will appear before or after the node
  // associated class (this issue comes up in SAGE where the Name class must
  // have the postdeclaration string "extern Name defaultName;" so that other
  // classes which follow it can provide default initialization of function
  // parameters).
  StringUtility::FileWithLineNumbers predeclarationString(
      1,
      StringUtility::StringWithLineNumber(
          node.getPredeclarationString(),
          "" /* "<getPredeclarationString " + node.getToken().getName() + ">" */
          ,
          1));

  StringUtility::FileWithLineNumbers editedHeaderFileString =
      StringUtility::copyEdit(editedHeaderFileStringTemp, "$PREDECLARATIONS",
                              predeclarationString);

  StringUtility::FileWithLineNumbers postdeclarationString(
      1, StringUtility::StringWithLineNumber(
             node.getPostdeclarationString(),
             "" /* "<getPostdeclarationString " + node.getToken().getName() +
                   ">" */
             ,
             1));
  editedHeaderFileString = StringUtility::copyEdit(
      editedHeaderFileString, "$POSTDECLARATIONS", postdeclarationString);

  // DQ (3/21/2017): Modify code generation to eliminate Clang C++11 override
  // warning.
  if (className == "SgNode") {
    editedHeaderFileString =
        StringUtility::copyEdit(editedHeaderFileString, " override", "");
  }

  editedHeaderFileString = editSubstitution(node, editedHeaderFileString);

  // Output strings to single file (this outputs everything to a single file)
  outputFile += editedHeaderFileString;

  if (!smallHeadersDir.empty()) {
    // Now write out the header file (each class in its own file)!
    editedHeaderFileString.insert(
        editedHeaderFileString.begin(),
        StringUtility::StringWithLineNumber(
            "#ifndef ROSE_" + node.getName() +
                "_H\n"
                "#define ROSE_" +
                node.getName() +
                "_H\n"
                "\n"
                "#include <RoseFirst.h>\n"
                "\n"
                "#include <Cxx_GrammarDeclarations.h>\n",
            __FILE__, __LINE__));
    editedHeaderFileString.push_back(StringUtility::StringWithLineNumber(
        "#endif // ROSE_" + node.getName() + "_H", __FILE__, __LINE__));
    writeFile(editedHeaderFileString, smallHeadersDir, node.getName(), ".h");
  }
}

void Grammar::buildHeaderFiles(AstNodeClass &node,
                               StringUtility::FileWithLineNumbers &outputFile) {
  buildClassDefinition(node, outputFile);
  vector<AstNodeClass *>::iterator treeListIterator;
  for (treeListIterator = node.subclasses.begin();
       treeListIterator != node.subclasses.end(); treeListIterator++) {
    ROSE_ASSERT((*treeListIterator) != NULL);
    ROSE_ASSERT((*treeListIterator)->getBaseClass() != NULL);
    buildHeaderFiles(**treeListIterator, outputFile);
  }
}

StringUtility::FileWithLineNumbers Grammar::editSubstitution(
    AstNodeClass &node,
    const StringUtility::FileWithLineNumbers &editStringOrig) {
  // Setup default edit variables (locate them here to centralize the process)
  string className = node.getName();
  string derivedClassString = getDerivedClassDeclaration(node);
  string parentClassName = (node.getBaseClass() != NULL)
                               ? node.getBaseClass()->getName()
                               : "//"; //"NO PARENT AVAILABLE";
  string baseClassConstructorParameterString = "";
  string constructorParameterListString = "";
  string constructorBodyString = "";

  StringUtility::FileWithLineNumbers editString = editStringOrig;
  editString = StringUtility::copyEdit(editString, "$CLASSNAME", className);
  editString = StringUtility::copyEdit(
      editString, "$GRAMMAR_NAME",
      getGrammarName()); // grammarName string defined in Grammar class
  editString =
      StringUtility::copyEdit(editString, "$BASECLASS", parentClassName);
  editString = StringUtility::copyEdit(
      editString, "$BASE_CLASS_CONSTRUCTOR_CALL", derivedClassString);
  // Set these to NULL strings if they are still present within the string
  editString =
      StringUtility::copyEdit(editString, "$BASE_CLASS_CONSTRUCTOR_PARAMETER",
                              baseClassConstructorParameterString);
  editString =
      StringUtility::copyEdit(editString, "$CONSTRUCTOR_PARAMETER_LIST",
                              constructorParameterListString);
  editString = StringUtility::copyEdit(editString, "$CONSTRUCTOR_BODY",
                                       constructorBodyString);
  editString =
      StringUtility::copyEdit(editString, "$CLASSTAG", node.getTagName());
  editString = StringUtility::copyEdit(
      editString, "$CONSTRUCTOR_ESSENTIAL_DATA_MEMBERS", "");

  // Edit the suffix of the $CLASSNAME (separate from the $GRAMMAR_PREFIX_)
  editString = StringUtility::copyEdit(editString, "$CLASS_BASE_NAME",
                                       node.getBaseName());

  // Fixup the declaration of pure virtual functions (so that they are defined
  // properly at the leaves)
  std::string emptyString = "";
  std::string pureVirtualMarker = " = 0";

  if (node.getCanHaveInstances()) {
    editString = StringUtility::copyEdit(editString, "$PURE_VIRTUAL_MARKER",
                                         emptyString);
  } else {
    editString = StringUtility::copyEdit(editString, "$PURE_VIRTUAL_MARKER",
                                         pureVirtualMarker);
  }

  // Now do final editing/substitution as specified by the user
  ROSE_ASSERT(node.getEditSubstituteTargetList(AstNodeClass::LOCAL_LIST,
                                               AstNodeClass::INCLUDE_LIST)
                  .size() ==
              node.getEditSubstituteSourceList(AstNodeClass::LOCAL_LIST,
                                               AstNodeClass::INCLUDE_LIST)
                  .size());
  ROSE_ASSERT(node.getEditSubstituteTargetList(AstNodeClass::SUBTREE_LIST,
                                               AstNodeClass::INCLUDE_LIST)
                  .size() ==
              node.getEditSubstituteSourceList(AstNodeClass::SUBTREE_LIST,
                                               AstNodeClass::INCLUDE_LIST)
                  .size());
  ROSE_ASSERT(node.getEditSubstituteTargetList(AstNodeClass::LOCAL_LIST,
                                               AstNodeClass::EXCLUDE_LIST)
                  .size() ==
              node.getEditSubstituteSourceList(AstNodeClass::LOCAL_LIST,
                                               AstNodeClass::EXCLUDE_LIST)
                  .size());
  ROSE_ASSERT(node.getEditSubstituteTargetList(AstNodeClass::SUBTREE_LIST,
                                               AstNodeClass::EXCLUDE_LIST)
                  .size() ==
              node.getEditSubstituteSourceList(AstNodeClass::SUBTREE_LIST,
                                               AstNodeClass::EXCLUDE_LIST)
                  .size());

  // Local lists that we will accumulate elements into
  // (traversing up through the parents in the grammar tree)
  vector<GrammarString *> targetList;
  vector<GrammarString *> targetExcludeList;
  vector<GrammarString *> sourceList;
  vector<GrammarString *> sourceExcludeList;

  // Initialize with local node data
  targetList = node.getEditSubstituteTargetList(AstNodeClass::LOCAL_LIST,
                                                AstNodeClass::INCLUDE_LIST);
  targetExcludeList = node.getEditSubstituteTargetList(
      AstNodeClass::LOCAL_LIST, AstNodeClass::EXCLUDE_LIST);
  sourceList = node.getEditSubstituteSourceList(AstNodeClass::LOCAL_LIST,
                                                AstNodeClass::INCLUDE_LIST);
  sourceExcludeList = node.getEditSubstituteSourceList(
      AstNodeClass::LOCAL_LIST, AstNodeClass::EXCLUDE_LIST);

  // Now generate the additions to the lists from the parent node subtree lists
  collectInheritedGrammarStrings(node, targetList, targetExcludeList,
                                 &AstNodeClass::getEditSubstituteTargetList,
                                 AstNodeClass::SUBTREE_LIST);
  collectInheritedGrammarStrings(node, sourceList, sourceExcludeList,
                                 &AstNodeClass::getEditSubstituteSourceList,
                                 AstNodeClass::SUBTREE_LIST);

  ROSE_ASSERT(sourceList.size() == targetList.size());
  ROSE_ASSERT(sourceExcludeList.size() == targetExcludeList.size());

  // Apply substitutions in their specified order; list sizes match above.
  for (size_t i = 0; i < sourceList.size(); ++i) {
    editString =
        StringUtility::copyEdit(editString, targetList[i]->getRawString(),
                                sourceList[i]->getRawString());
  }

  editString =
      StringUtility::copyEdit(editString, "$GRAMMAR_PREFIX_",
                              node.getGrammar()->getGrammarPrefixName());
  editString = StringUtility::copyEdit(editString, "$CLASSNAME", className);

  return editString;
}

void Grammar::buildVariantsStringPrototype(
    StringUtility::FileWithLineNumbers &outputFile) {
  // DQ (10/26/2007): Add the protytype for the Cxx_GrammarTerminalNames
  // This has been changed to use the newer V_SgNode form of the IR node names.

  string startString =
      "typedef struct \n"
      "   { \n"
      "     VariantT variant; \n"
      "     std::string name; \n"
      "   } TerminalNamesType; \n\n"
      "extern TerminalNamesType $MARKERTerminalNames[$LIST_LENGTH]; \n\n";

  // Set the type name using the grammarName variable contained within the
  // grammar
  startString =
      StringUtility::copyEdit(startString, "$MARKER", getGrammarName());

  size_t maxVariant = this->astVariantToNodeMap.rbegin()->first;

  string listLengthString = StringUtility::numberToString(maxVariant + 2);

  // COPY the length into the string at "LIST_LENGTH"
  startString =
      StringUtility::copyEdit(startString, "$LIST_LENGTH", listLengthString);

  string finalString = startString;

  outputFile.push_back(StringUtility::StringWithLineNumber(finalString, "", 1));
}

void Grammar::buildVariantsStringDataBase(
    StringUtility::FileWithLineNumbers &outputFile) {
  string startString =
      "TerminalNamesType $MARKERTerminalNames[$LIST_LENGTH] = {  \n";

  // Set the type name using the grammarName variable contained within the
  // grammar
  startString =
      StringUtility::copyEdit(startString, "$MARKER", getGrammarName());

  size_t maxVariant = this->astVariantToNodeMap.rbegin()->first;

  string listLengthString = StringUtility::numberToString(maxVariant + 2U);

  // COPY the length into the string at "LIST_LENGTH"
  startString =
      StringUtility::copyEdit(startString, "$LIST_LENGTH", listLengthString);

  // BP : 10/25/2001, this new version of the code only performs one new memory
  // allocation, uses list iterators instead of the operator[], and should be a
  // lot faster
  string openString = "          {";
  string separatorString = ", \"";
  string closeString = "\"}, \n";
  string middleString;

  vector<string> variantNames;
  for (map<size_t, string>::const_iterator i =
           this->astVariantToNodeMap.begin();
       i != this->astVariantToNodeMap.end(); ++i) {
    if (i->first + 1 > variantNames.size()) {
      variantNames.resize(i->first + 1, "<ERROR: unknown VariantT>");
    }
    variantNames[i->first] = i->second;
  }
  for (size_t i = 0; i < variantNames.size(); i++) {
    middleString += openString + "(VariantT)" +
                    StringUtility::numberToString(i) + separatorString +
                    variantNames[i] + closeString;
  }

  string endString =
      "          {V_SgNumVariants, \"last tag\" } \n   }; \n\n\n";

  endString = StringUtility::copyEdit(endString, "$MARKER", getGrammarName());

  string finalString = startString + middleString + endString;

  outputFile.push_back(StringUtility::StringWithLineNumber(finalString, "", 1));
}

void Grammar::buildSourceFiles(AstNodeClass &node,
                               StringUtility::FileWithLineNumbers &outputFile) {
  string sourceFileInsertionSeparator = "MEMBER_FUNCTION_DEFINITIONS";
  string fileName = "../Grammar/grammarClassDefinitionMacros.macro";
  StringUtility::FileWithLineNumbers sourceFileTemplate =
      readFileWithPos(fileName);

  StringUtility::FileWithLineNumbers sourceBeforeInsertion =
      buildHeaderStringBeforeMarker(sourceFileInsertionSeparator, fileName);

  StringUtility::FileWithLineNumbers sourceAfterInsertion =
      buildHeaderStringAfterMarker(sourceFileInsertionSeparator, fileName);

  string derivedClassString;

  StringUtility::FileWithLineNumbers editedStringMiddle;
  // Each of these functions should return a null terminated string
  // (even if there are no code strings associated with this node).

  // At this point data access functions have already been built and placed into
  // the source code lists all that is left is the construction of the code
  // specific to the constructor to build the constructor we require all the
  // data variables (which is why we could not have build it with the access
  // functions)

  StringUtility::FileWithLineNumbers editStringMiddleNodeDataMemberFunctions =
      buildConstructor(node);
  StringUtility::FileWithLineNumbers editStringMiddleNodeMemberFunctions =
      buildStringForSource(node);

  // Place the constructor at the top of the node specific code for this element
  // of grammar BP : 10/24/2001, keep track of memory

  editedStringMiddle += editStringMiddleNodeMemberFunctions;
  editedStringMiddle += editStringMiddleNodeDataMemberFunctions;

  // increment the final string with the node specific string

  StringUtility::FileWithLineNumbers editedSourceFileString =
      sourceBeforeInsertion + editedStringMiddle;
  editedSourceFileString += sourceAfterInsertion;
  // Now apply the edit/subsitution specified within the grammar (by the user)
  editedSourceFileString = editSubstitution(node, editedSourceFileString);

  // Also output strings to single file
  outputFile += editedSourceFileString;

  // Call this function recursively on the children of this node in the tree
  vector<AstNodeClass *>::iterator treeNodeIterator;
  for (treeNodeIterator = node.subclasses.begin();
       treeNodeIterator != node.subclasses.end(); treeNodeIterator++) {
    ROSE_ASSERT((*treeNodeIterator) != NULL);
    ROSE_ASSERT((*treeNodeIterator)->getBaseClass() != NULL);

    buildSourceFiles(**treeNodeIterator, outputFile);
  }
}

size_t Grammar::getVariantForNode(const std::string &name) const {
  std::map<std::string, size_t>::const_iterator it =
      this->astNodeToVariantMap.find(name);
  if (it == this->astNodeToVariantMap.end()) {
    it = this->astNodeToVariantMap.find(this->grammarPrefixName + name);
  }
  if (it == this->astNodeToVariantMap.end()) {
    std::cerr << "No variant for " << name << "; add it to " << ROSE_SOURCE_TREE
              << "/src/ROSETTA/astNodeList" << std::endl;
    ROSE_ABORT();
  }
  return it->second;
}

size_t Grammar::getVariantForTerminal(const AstNodeClass &t) const {
  return this->getVariantForNode(t.getName());
}

AstNodeClass &Grammar::getTerminalForVariant(size_t var) {
  std::map<size_t, AstNodeClass *>::const_iterator it =
      this->astVariantToTerminalMap.find(var);

  ROSE_ASSERT(it != this->astVariantToTerminalMap.end());

  ROSE_ASSERT(it->second);
  return *(it->second);
}

StringUtility::FileWithLineNumbers Grammar::buildVariants() {
  string header =
      "//! Variants used to identify elements of the grammar used in ROSE \n"
      "/*! Each element is assigned a unique value defined by this enumerated "
      "type \n"
      "    the values are used to generate the casts from one type toanother "
      "were permitted. \n"
      "    This is a technique borrowed from the design of SAGE II. \n"
      "*/ \n"
      "enum $MARKERVariants \n"
      "   { \n";

  string footer =
      "     $MARKER_UNKNOWN_GRAMMAR = " +
      StringUtility::numberToString(this->astNodeToVariantMap.size() + 1) +
      ",\n"
      "     $MARKER_LAST_TAG \n"
      "   }; \n";

  string separatorString = "     ";
  string newlineString = ",\n";

  unsigned int i = 0;

  // now allocate the necessary memory
  StringUtility::FileWithLineNumbers returnString;
  returnString.push_back(StringUtility::StringWithLineNumber(
      header, "" /* "<buildVariants header>" */, 1));

  for (i = 0; i < terminalList.size(); i++) {
    returnString.push_back(StringUtility::StringWithLineNumber(
        separatorString + terminalList[i]->getTagName() + " = " +
            StringUtility::numberToString(
                this->getVariantForTerminal(*terminalList[i])) +
            ", ",
        "" /* "<variant for node type " + terminalList[i].getTagName() + ">" */,
        1));
  }

  returnString.push_back(StringUtility::StringWithLineNumber(
      footer, "" /* "<buildVariants footer>" */, 1));

  return returnString;
}

// Emit a forward declaration for each class of the grammar.
void Grammar::emitForwardDeclarations(std::ostream &s) const {
  std::vector<std::string> names;
  names.reserve(terminalList.size());
  for (const auto &terminal : terminalList)
    names.push_back(terminal->name);

  std::sort(names.begin(), names.end());
  for (const std::string &name : names)
    s << "class " << name << ";\n";
}

StringUtility::FileWithLineNumbers Grammar::buildForwardDeclarations() {
  // DQ (4/23/2006): Need to add forward declarations of "Sg[CLASSNAME]*
  // isSg[CLASSNAME](SgNode*)" friend functions

  string header = "\n\n\n//! Forward Declarations used to represent the "
                  "grammar used in ROSE \n";

  StringUtility::FileWithLineNumbers returnString;
  returnString.push_back(StringUtility::StringWithLineNumber(
      header, "" /* "<buildForwardDeclarations header>" */, 1));

  for (unsigned int i = 0; i < terminalList.size(); i++) {
    returnString.push_back(StringUtility::StringWithLineNumber(
        "class " + terminalList[i]->name + ";",
        "" /* "<forward decl for " + terminalList[i].name + ">" */, 1));
  }

  returnString.push_back(StringUtility::StringWithLineNumber(
      "\n\n// Forward declaration of \"<classname> is<classname> (SgNode* n)\" "
      "friend functions.\n",
      "" /* "<unknown>" */, 1));
  returnString.push_back(StringUtility::StringWithLineNumber(
      "// GNU g++ 4.1.0 requires these be declared outside of the class "
      "(because the friend declaration in the class is not enough).\n\n",
      "" /* "<unknown>" */, 2));

  returnString.push_back(StringUtility::StringWithLineNumber(
      "\n\n#include \"rosedll.h\"\n", "" /* "<unknown>" */, 1));
  returnString.push_back(StringUtility::StringWithLineNumber(
      "#include \"mlog.h\"\n", "" /* "<unknown>" */, 1));
  returnString.push_back(StringUtility::StringWithLineNumber(
      "#include \"rosedefs.h\"\n", "" /* "<unknown>" */, 1));
  returnString.push_back(StringUtility::StringWithLineNumber(
      "#include \"rose_attributes_list.h\"\n", "" /* "<unknown>" */, 1));
  // Milind Chabbi (8/28/2013): Performance refactoring: make
  // rose_ClassHierarchyCastTable accessible for IS_SgXXX_FAST_MACRO()s
  size_t maxRows = getRowsInClassHierarchyCastTable();
  size_t maxCols = getColumnsInClassHierarchyCastTable();
  string externDeclarationForClassHierarchyCastTable =
      "\nextern const uint8_t rose_ClassHierarchyCastTable[" +
      StringUtility::numberToString(maxRows) + "][" +
      StringUtility::numberToString(maxCols) + "] ;\n";
  returnString.push_back(StringUtility::StringWithLineNumber(
      externDeclarationForClassHierarchyCastTable, "", 1));
  for (unsigned int i = 0; i < terminalList.size(); i++) {
    string className = terminalList[i]->name;
    returnString.push_back(StringUtility::StringWithLineNumber(
        "ROSE_DLL_API " + className + "* is" + className + "(SgNode* node);",
        "" /* "<downcast function for " + className + ">" */, 1));
    returnString.push_back(StringUtility::StringWithLineNumber(
        "ROSE_DLL_API const " + className + "* is" + className +
            "(const SgNode* node);",
        "" /* "<downcast function for " + className + ">" */, 2));
    // Milind Chabbi (8/28/2013): Performance refactoring.
    // Providing additional MACRO for each isSgXXX() function.
    // One can substitue each isSgXXX() function with IS_SgXXX_FAST_MACRO()
    // function. Being a macro, IS_SgXXX_FAST_MACRO() is unsafe and should be
    // used with care. Using IS_SgXXX_FAST_MACRO() with side effect expressions.
    // e.g., IS_SgXXX_FAST_MACRO(*it++) will cause undefined effects. However,
    // it can be used safely for side effect free expressions e.g.,
    // IS_SgXXX_FAST_MACRO(node) and this will improve performance. A good use
    // case of using IS_SgXXX_FAST_MACRO() is in places where isSgXXX() is very
    // heavily used. Substituting all isSgXXX() with IS_SgXXX_FAST_MACRO()
    // worked fine for entire of rose but failed in unsafe uses in tests e.g.
    // src/optimizer/programAnalysis/StencilAnalysis.C
    string fromVariantString = "(node)->variantT()";
    string toVariantString = className + "::static_variant";
    string toVariantBytePositionString = toVariantString + " >> 3";
    string toVariantbitMaskPositionString =
        "(1 << (" + toVariantString + " & 7))";
    string rose_ClassHierarchyCastTableAccessString =
        " (rose_ClassHierarchyCastTable[" + fromVariantString + "][" +
        toVariantBytePositionString + "] & " + toVariantbitMaskPositionString +
        ")";
    returnString.push_back(StringUtility::StringWithLineNumber(
        "#define IS_" + className + "_FAST_MACRO(node) ( (node) ? ((" +
            rose_ClassHierarchyCastTableAccessString + ") ? ((" + className +
            "*) (node)) : NULL) : NULL)",
        "" /* "<downcast MACRO for " + className + ">" */, 1));
  }

  return returnString;
}

StringUtility::FileWithLineNumbers
Grammar::extractStringFromFile(const string &startMarker,
                               const string &endMarker,
                               const string &filename) {
  // Open file
  StringUtility::FileWithLineNumbers fileString =
      Grammar::readFileWithPos(filename);

  auto isMarker = [](const string &line, const string &marker) {
    const size_t first = line.find_first_not_of(" \t\r\n");
    if (first == string::npos)
      return false;
    const size_t last = line.find_last_not_of(" \t\r\n");
    return line.compare(first, last - first + 1, marker) == 0;
  };
  size_t start = fileString.size();
  size_t end = fileString.size();
  for (size_t i = 0; i < fileString.size(); ++i) {
    for (const auto &boundary : {std::make_pair(&startMarker, &start),
                                 std::make_pair(&endMarker, &end)}) {
      if (!isMarker(fileString[i].str, *boundary.first))
        continue;
      if (*boundary.second != fileString.size())
        throw std::runtime_error("duplicate grammar marker " + *boundary.first +
                                 " in " + filename);
      *boundary.second = i;
    }
  }
  if (start == fileString.size() || end == fileString.size() || start >= end)
    throw std::runtime_error("missing or unordered grammar fragment " +
                             startMarker + " / " + endMarker + " in " +
                             filename);

  fileString[end].str =
      fileString[end].str.substr(0, fileString[end].str.find(endMarker));
  fileString.erase(fileString.begin() + end + 1, fileString.end());
  fileString.erase(fileString.begin(), fileString.begin() + start);
  fileString.front().str = fileString.front().str.substr(
      fileString.front().str.find(startMarker) + startMarker.size());

  return fileString;
}

string Grammar::getFilenameForGlobalDeclarations() {
  return filenameForGlobalDeclarations;
}

void Grammar::setFilenameForGlobalDeclarations(const string &filename) {
  filenameForGlobalDeclarations = filename;
}

StringUtility::FileWithLineNumbers Grammar::buildMiscSupportDeclarations() {
  // This function allows the introduction of support classes for the grammar
  StringUtility::FileWithLineNumbers returnString;

  string fileName = getFilenameForGlobalDeclarations();
  returnString = readFileWithPos(fileName);

  // Finally, Edit into place the name of the grammar
  returnString = StringUtility::copyEdit(returnString, "$GRAMMAR_PREFIX_",
                                         getGrammarPrefixName());

  return returnString;
}

// MS: new automatically generated variant. Replaces variant().
// used in variantT()
string Grammar::buildVariantEnums() {
  string s = string("enum VariantT \n{\n");
  unsigned int i;
  bool notFirst = false;
  for (i = 0; i < terminalList.size(); i++) {
    if (notFirst) {
      s += string(",\n");
    }
    notFirst = true;
    size_t varNum = this->getVariantForNode(terminalList[i]->name);
    s += (string("V_") + terminalList[i]->name + " = " +
          StringUtility::numberToString(varNum));
  }
  // add an ENUM to get the number of enums declared.
  s += string(", V_SgNumVariants = ") +
       StringUtility::numberToString(this->astNodeToVariantMap.size() + 1);
  s += "};\n";
  return s;
}

// Milind Chabbi (8/28/2013): Performance refactoring
// classHierarchyCastTable is a table where each row represents a SgXXX node and
// each column represents if the node can be dynamically casted to the other
// SgXXX node. Sample classHierarchyCastTable
//                   | SgNode | SgStatement | SgBreakStmt |
// --------------------------------------------------------
// SgNode            | true   |  false      | false       |
// SgStatement       | true   |  true       | false       |
// SgBreakStmt       | true   |  true       | true        |
// --------------------------------------------------------
// The table, however, instead of storing booleans, is represented using bits in
// a byte.

static uint8_t **classHierarchyCastTable;

// Set the correct bit in classHierarchyCastTable correspinding to the given row
// and column
static void SetBitInClassHierarchyCastTable(size_t row, size_t col) {
  size_t bytePosition = col >> 3;
  uint8_t bitPosition = col & 7;
  classHierarchyCastTable[row][bytePosition] |= (1 << bitPosition);
}

// Gets the number of rows in classHierarchyCastTable
size_t Grammar::getRowsInClassHierarchyCastTable() {
  return this->astVariantToNodeMap.rbegin()->first + 1;
}

// Gets the number of columns in classHierarchyCastTable
size_t Grammar::getColumnsInClassHierarchyCastTable() {
  return (this->astVariantToNodeMap.rbegin()->first / 8) + 1;
}

// Generates a table (classHierarchyCatTable) populated with information
// about whether a given SgXXX node can be casted to other SgYYY node.
// The technique has constant lookup time.
string Grammar::generateClassHierarchyCastTable() {
  // First allocate the classHierarchyCastTable table
  size_t maxRows = getRowsInClassHierarchyCastTable();
  size_t maxCols = getColumnsInClassHierarchyCastTable();
  classHierarchyCastTable = new uint8_t *[maxRows];
  for (size_t i = 0; i < maxRows; i++) {
    classHierarchyCastTable[i] = new uint8_t[maxCols]();
  }

  // Populate classHierarchyCastTable by visiting the Sg node tree
  vector<AstNodeClass *> myParentsDescendents;
  buildClassHierarchyCastTable(getRootOfGrammar(), myParentsDescendents);

  // Output the table as a constant in the generated code
  string s = "\nconst uint8_t rose_ClassHierarchyCastTable[" +
             StringUtility::numberToString(maxRows) + "][" +
             StringUtility::numberToString(maxCols) + "] = {";
  bool outerLoopFirst = true;
  for (size_t i = 0; i < maxRows; i++) {
    if (!outerLoopFirst) {
      s += ",";
    } else {
      outerLoopFirst = false;
    }
    bool innerLoopFirst = true;
    for (size_t j = 0; j < maxCols; j++) {
      if (innerLoopFirst) {
        s += "{";
        innerLoopFirst = false;
      } else {
        s += ",";
      }
      s += StringUtility::numberToString(classHierarchyCastTable[i][j]);
    }
    s += "}\n";
  }
  s += "};\n";

  for (size_t i = 0; i < maxRows; i++) {
    delete[] classHierarchyCastTable[i];
  }
  delete[] classHierarchyCastTable;

  return s;
}

// Populates the classHierarchyCastTable with all the types that can be casted
// to AstNodeClass type
void Grammar::buildClassHierarchyCastTable(
    AstNodeClass *astNodeClass, vector<AstNodeClass *> &myParentsDescendents) {
  // obtain the immediate derived classes of the given AstNodeClass.
  vector<AstNodeClass *> myImmediateDescendents = astNodeClass->subclasses;
  vector<AstNodeClass *> myDescendents;

  // recur on the immediate derived classes to obtain the entire class hierarchy
  // rooted at the given node.
  for (vector<AstNodeClass *>::iterator it = myImmediateDescendents.begin(),
                                        e = myImmediateDescendents.end();
       it != e; it++) {
    buildClassHierarchyCastTable(*it, myDescendents);
  }

  // add self to the vector
  myDescendents.push_back(astNodeClass);

  size_t toVariant = getVariantForTerminal(*astNodeClass);
  ROSE_ASSERT(toVariant < getRowsInClassHierarchyCastTable());

  // Set the bits in classHierarchyCastTable indicating all derived types in
  // this subtree can be casted to the type of the AstNodeClass.
  for (vector<AstNodeClass *>::iterator it = myDescendents.begin(),
                                        e = myDescendents.end();
       it != e; it++) {
    size_t fromVariant = getVariantForTerminal(*(*it));
    ROSE_ASSERT(fromVariant < getRowsInClassHierarchyCastTable());
    SetBitInClassHierarchyCastTable(fromVariant, toVariant);
  }
  // return the list of all types rooted at this subtree to the caller
  myParentsDescendents.insert(myParentsDescendents.end(), myDescendents.begin(),
                              myDescendents.end());
}

string Grammar::buildClassHierarchySubTreeFunction() {
  string s = "void SgNode::getClassHierarchySubTreeFunction( VariantT v, "
             "std::vector<VariantT>& subTreeVariants){\n";
  s += "ROSE_ASSERT(static_cast<unsigned>(v) < "
       "static_cast<unsigned>(V_SgNumVariants));\n";

  s += "switch(v){\n ";
  for (size_t i = 0; i < terminalList.size(); i++) {
    if (terminalList[i]->subclasses.empty())
      continue;

    s += "case " + string("V_") + string(terminalList[i]->name) + ":\n";

    s += "{\n";

    for (vector<AstNodeClass *>::iterator iItr =
             terminalList[i]->subclasses.begin();
         iItr != terminalList[i]->subclasses.end(); ++iItr) {
      s += "subTreeVariants.push_back(V_" + string((*iItr)->getName()) + ");\n";
    }
    s += "break;\n";
    s += "}\n";
  }

  s += "default:\n{ }\n";
  s += "}\n\n";

  s += "};\n";

  s += "\n\n\n\n";

  s += "std::vector<VariantT> SgNode::getClassHierarchySubTreeFunction( "
       "VariantT v){\n";
  s += "std::vector<VariantT> subTreeVariants;\n";
  s += "getClassHierarchySubTreeFunction(v, subTreeVariants);\n";
  s += "return subTreeVariants;\n";
  s += "}\n";

  return s;
}

// AS: new automatically generated variant. Replaces variant().
// used in variantT()
string Grammar::buildMemoryPoolBasedVariantVectorTraversalSupport() {

  // The first function which takes a vector reference which is used to
  // return the result.
  string s = "template <class FunctionalType>\n";
  s += "void "
       "AstQueryNamespace::queryMemoryPool(AstQuery<ROSE_VisitTraversal,"
       "FunctionalType>& astQuery,";
  s += " VariantVector* variantsToTraverse)\n";
  s += "  {\n";

  s += "for (VariantVector::iterator it = variantsToTraverse->begin(); it != "
       "variantsToTraverse->end(); ++it)\n";
  s += "  {\n";
  s += "switch(*it){\n ";

  unsigned int i;

  for (i = 0; i < terminalList.size(); i++) {
    s += "case " + string("V_") + terminalList[i]->name + ": {\n";
    s +=
        "  " + terminalList[i]->name + "::traverseMemoryPoolNodes(astQuery);\n";
    s += "  break;\n";
    s += "}\n";
  }

  // Add default case
  s += "default:\n{\n";
  s += "  // This is a common error after adding a new IR node (because this "
       "function should have been automatically generated).\n";
  s += "  std::cout << \"Case not implemented in queryMemoryPool(..). "
       "Exiting.\" << std::endl;\n";
  s += "  ROSE_ABORT();\n";
  s += "  break;\n";
  s += "}\n";
  s += "}\n";
  s += "}\n\n";
  s += "};\n";

  return s;
}

// MS: new automatically generated variantnames as variantEnum->string mapping
string Grammar::buildVariantEnumNames() {
  vector<string> variantNames;
  for (map<size_t, string>::const_iterator i =
           this->astVariantToNodeMap.begin();
       i != this->astVariantToNodeMap.end(); ++i) {
    if (i->first + 1 > variantNames.size()) {
      variantNames.resize(i->first + 1, "<ERROR: unknown VariantT>");
    }
    variantNames[i->first] = i->second;
  }
  bool first = true;
  string s = "";
  for (size_t i = 0; i < variantNames.size(); i++) {
    s += (first ? "" : string(",\n")) + "\"" + variantNames[i] + string("\"");
    first = false;
  }
  return s;
}

// PC: new implementation of ReferenceToPointerHandler.  This implementation
// allows you to use a subclass to override the template function
// ReferenceToPointerHandler::apply, which is not usually possible.
// This is done by defining an overloaded set of virtual functions each
// corresponding to a particular node.  The SimpleReferenceToPointerHandler
// class provides ReferenceToPointerHandler's old behaviour.
// SimpleReferenceToPointerHandler also acts as an example of how to use
// ReferenceToPointerHandlerImpl to write a "virtual template function".
string Grammar::buildReferenceToPointerHandlerCode() {
  // DQ (3/9/2013): Adding support to exclude some code from SWIG.
  string s;
  s += string("#ifndef SWIG \n\n");
  s += "#ifndef REFERENCETOPOINTERHANDLER_DEFINED\n"
       "#define REFERENCETOPOINTERHANDLER_DEFINED\n\n"

       "struct ReferenceToPointerHandler\n"
       "   {\n";

  for (size_t i = 0; i < terminalList.size(); i++) {
    s += "     virtual void apply(" + terminalList[i]->name +
         " *&r, const SgName &n, bool traverse) = 0;\n";
  }

  s += "\n     virtual ~ReferenceToPointerHandler() {}\n"
       "   };\n\n";

  s += "template <class ImplClass>\n"
       "struct ReferenceToPointerHandlerImpl : ReferenceToPointerHandler\n"
       "   {\n";

  for (size_t i = 0; i < terminalList.size(); i++) {
    s += "     void apply(" + terminalList[i]->name +
         " *&r, const SgName &n, bool traverse)\n"
         "        {\n"
         "          static_cast<ImplClass *>(this)->genericApply(r, n, "
         "traverse);\n"
         "        }\n\n";
  }

  s += "};\n\n"

       "struct SimpleReferenceToPointerHandler : "
       "ReferenceToPointerHandlerImpl<SimpleReferenceToPointerHandler>\n"
       "   {\n"
       "     template <typename NodeSubclass>\n"
       "     void genericApply(NodeSubclass*& r, const SgName& n, bool "
       "traverse)\n"
       "        {\n"
       "          SgNode* sgn = r;\n"
       "          (*this)(sgn, n, traverse);\n\n"
       "          NodeSubclass* replacement = "
       "dynamic_cast<NodeSubclass*>(sgn);\n"
       "          ROSE_ASSERT(sgn == nullptr || replacement != nullptr);\n"
       "          r = replacement;\n"
       "        }\n\n"

       "     virtual void operator()(SgNode*&, const SgName&, bool) = 0;\n"
       "   };\n\n"

       "#endif // REFERENCETOPOINTERHANDLER_DEFINED\n\n";

  // DQ (3/9/2013): Adding support to exclude some code from SWIG.
  s += string("#endif // endif for ifndef SWIG \n\n");

  return s;
}

void Grammar::buildCode() {
  // Build tree representing the type hierarchy
  // Get the root node (the only one without a parent)
  // Also, add the grammar prefix to each and every node
  this->setRootOfGrammar(NULL);
  for (vector<AstNodeClass *>::const_iterator i = terminalList.begin();
       i != terminalList.end(); ++i) {
    (*i)->addGrammarPrefixToName();
    if ((*i)->getBaseClass() == NULL) {
      this->setRootOfGrammar(*i);
    }
  }
  ROSE_ASSERT(this->getRootOfGrammar());
  ROSE_ASSERT(rootNode != NULL);

  // **************************************************************
  //                 AST HEADER FILE GENERATION
  // **************************************************************

  StringUtility::FileWithLineNumbers ROSE_ArrayGrammarHeaderFile;

  // Put in comment block so that autogenerated grammars can be automatically
  // documented.

  // DQ (12/28/2009): Removed references to files that should be elsewhere to
  // simplify splitting large files generated by ROSETTA..
  string headerString =
      "// MACHINE GENERATED HEADER FILE --- DO NOT MODIFY! \n\n\n"
      "//! AST implementation generated by ROSETTA \n"
      "//  (in many this is an object oriented IR based upon Sage II's "
      "implementation (Gannon et. al.). \n\n\n"
      "#ifndef $IFDEF_MARKER_H \n"
      "#define $IFDEF_MARKER_H \n\n"
      "\n"
      "#include <Rose/FloatingValue.h>\n"
      "#include <Rose/Integer128Value.h>\n"
      "#include <memory>\n"
      "#include <optional>\n"
      "#include <string>\n\n";

  string footerString = "\n\n\n#endif // ifndef IFDEF_MARKER_H \n\n\n";

  // Get the strings onto the heap so that copy edit can process it (is this
  // poor design? MS: yes)
  headerString =
      StringUtility::copyEdit(headerString, "$IFDEF_MARKER", getGrammarName());
  footerString =
      StringUtility::copyEdit(footerString, "$IFDEF_MARKER", getGrammarName());
  ROSE_ArrayGrammarHeaderFile << headerString;

  // Generate type variant enums
  {
    // Old enum
    StringUtility::FileWithLineNumbers variantsSource;
    variantsSource << "#ifndef ROSE_Cxx_GrammarVariants_H\n";
    variantsSource << "#define ROSE_Cxx_GrammarVariants_H\n";
    StringUtility::FileWithLineNumbers tmp = buildVariants();
    variantsSource += StringUtility::copyEdit(tmp, "$MARKER", getGrammarName());

    // New enum
    variantsSource << buildVariantEnums();
    variantsSource << "#endif\n";
    Grammar::writeFile(variantsSource, target_directory,
                       getGrammarName() + "Variants", ".h");
  }
  ROSE_ArrayGrammarHeaderFile
      << "\n\n#include <" + getGrammarName() + "Variants.h>\n";

  // DQ (10/26/2007): Add the protytype for the Cxx_GrammarTerminalNames
  buildVariantsStringPrototype(ROSE_ArrayGrammarHeaderFile);

  // Forward declarations
  {
    const std::string fileName =
        target_directory + "/" + getGrammarName() + "Declarations.h";
    std::ostringstream declarations;
    declarations << "#ifndef ROSE_Cxx_GrammarDeclarations_H\n"
                 << "#define ROSE_Cxx_GrammarDeclarations_H\n";
    emitForwardDeclarations(declarations);
    declarations << "#endif\n";
    writeGeneratedFile(fileName, declarations.str());
  }
  ROSE_ArrayGrammarHeaderFile
      << "\n\n#include <" + getGrammarName() + "Declarations.h>\n";

  // Is-a predicates
  {
    StringUtility::FileWithLineNumbers sourceLines = buildForwardDeclarations();
    sourceLines =
        StringUtility::copyEdit(sourceLines, "$MARKER", getGrammarName());
    ROSE_ArrayGrammarHeaderFile += sourceLines;
  }

  ROSE_ArrayGrammarHeaderFile << "\n\n";
  ROSE_ArrayGrammarHeaderFile
      << "ROSE_DLL_API std::ostream& operator<<(std::ostream&, const "
         "SgName&);\n\n";

  // DQ (12/6/2003): Added output function for SgBitVector objects
  ROSE_ArrayGrammarHeaderFile << "std::ostream& operator<<(std::ostream&, "
                                 "const std::vector<bool>&);\n\n";

  // DQ (8/3/2005): Added output function for STL set objects
  ROSE_ArrayGrammarHeaderFile
      << "std::ostream& operator<<(std::ostream&, const std::set<int>&);\n\n";

  // DQ (10/4/2006): Added output function for STL map objects
  ROSE_ArrayGrammarHeaderFile << "std::ostream& operator<<(std::ostream&, "
                                 "const std::map<SgNode*,int>&);\n\n";

  // DQ (10/6/2006): Added output function for STL map objects
  ROSE_ArrayGrammarHeaderFile << "std::ostream& operator<<(std::ostream&, "
                                 "const std::map<SgNode*,std::string>&);\n\n";

  // DQ (11/20/2007): Part of support for the Fortran data statement
  ROSE_ArrayGrammarHeaderFile
      << "std::ostream& operator<<(std::ostream&, const "
         "Rose_STL_Container<SgDataStatementObject*>&);\n\n";
  ROSE_ArrayGrammarHeaderFile
      << "std::ostream& operator<<(std::ostream&, const "
         "Rose_STL_Container<SgDataStatementValue*>&);\n\n";
  ROSE_ArrayGrammarHeaderFile
      << "std::ostream& operator<<(std::ostream&, const "
         "Rose_STL_Container<SgCommonBlockObject*>&);\n\n";
  ROSE_ArrayGrammarHeaderFile
      << "std::ostream& operator<<(std::ostream&, const "
         "Rose_STL_Container<SgDimensionObject*>&);\n\n";
  ROSE_ArrayGrammarHeaderFile
      << "std::ostream& operator<<(std::ostream&, const "
         "Rose_STL_Container<SgLabelSymbol*>&);\n\n";
  ROSE_ArrayGrammarHeaderFile
      << "std::ostream& operator<<(std::ostream&, const "
         "Rose_STL_Container<SgFormatItem*>&);\n\n";

  // Use bitmasks so traversal can include both pre- and postorder.
  ROSE_ArrayGrammarHeaderFile
      << "typedef enum \n{preorder = 1, postorder = 2, preandpostorder = "
         "preorder | postorder} t_traverseOrder;\n\n";

  // Now declare the classes representing the terminals and nonterminals within
  // the grammar
  ROSE_ASSERT(rootNode != NULL);
  buildHeaderFiles(*rootNode, ROSE_ArrayGrammarHeaderFile);

  // DQ (11/26/2005): Support for visitor pattern.
  string visitorSupport = buildVisitorBaseClass();
  ROSE_ArrayGrammarHeaderFile.push_back(
      StringUtility::StringWithLineNumber(visitorSupport, "", 1));

  ROSE_ArrayGrammarHeaderFile.push_back(
      StringUtility::StringWithLineNumber(footerString, "", 1));

  ROSE_ArrayGrammarHeaderFile << buildReferenceToPointerHandlerCode();

  // Now place all global declarations at the base of the
  // header file after all classes have been defined
  StringUtility::FileWithLineNumbers miscSupport =
      buildMiscSupportDeclarations();
  ROSE_ArrayGrammarHeaderFile += miscSupport;

  Grammar::writeFile(ROSE_ArrayGrammarHeaderFile, target_directory,
                     getGrammarName(), ".h");

  // **************************************************************
  //                 AST SOURCE FILE GENERATION
  // **************************************************************

  StringUtility::FileWithLineNumbers ROSE_ArrayGrammarSourceFile;
  // Now build the source files

  string includeHeaderFileName = "sage3basic.h";
  string includeHeaderString = "// MACHINE GENERATED SOURCE FILE WITH ROSE "
                               "(Grammar.h)--- DO NOT MODIFY!\n\n#include \"" +
                               includeHeaderFileName + "\"\n\n";
  string includeHeaderStringWithoutROSE =
      "// MACHINE GENERATED SOURCE FILE --- DO NOT MODIFY! (Grammar.C) \n\n";
  // DQ (10/14/2010):  This should only be included by source files that
  // require it. This fixed a reported bug which caused conflicts with
  // configure-time macros (e.g. PACKAGE_BUGREPORT). Interestingly it must be
  // at the top of the list of include files.
  includeHeaderString += "// The header file (\"rose_config.h\") should only "
                         "be included by source files that require it.\n";
  string includeHeader_rose_config = "#include \"rose_config.h\"\n\n";
  includeHeaderString += includeHeader_rose_config;

  includeHeaderString += "\nusing namespace std;\n";

  ROSE_ArrayGrammarSourceFile.push_back(
      StringUtility::StringWithLineNumber(includeHeaderString, "", 1));

  // Setup the data base of names (linking name strings to grammar element tags)
  buildVariantsStringDataBase(ROSE_ArrayGrammarSourceFile);

  // Now build the source code for the terminals and non-terminals in the
  // grammar
  ROSE_ASSERT(rootNode != NULL);
  buildSourceFiles(*rootNode, ROSE_ArrayGrammarSourceFile);
  if (verbose)
    cout << "DONE: buildSourceFiles()" << endl;

  // DQ (5/24/2005): Support for evaluation of memory sizes of IR nodes
  string memoryStorageEvaluationSupport = buildMemoryStorageEvaluationSupport();
  ROSE_ArrayGrammarSourceFile.push_back(StringUtility::StringWithLineNumber(
      memoryStorageEvaluationSupport, "", 1));

  string memoryPoolTraversalSupport = buildMemoryPoolBasedTraversalSupport();
  ROSE_ArrayGrammarSourceFile.push_back(
      StringUtility::StringWithLineNumber(memoryPoolTraversalSupport, "", 1));

  Grammar::writeFile(ROSE_ArrayGrammarSourceFile, target_directory,
                     getGrammarName(), ".C");

  StringUtility::FileWithLineNumbers ROSE_NewAndDeleteOperatorSourceFile;
  ROSE_NewAndDeleteOperatorSourceFile.push_back(
      StringUtility::StringWithLineNumber(includeHeaderString, "", 1));

  ROSE_ASSERT(rootNode != NULL);

  buildNewAndDeleteOperators(*rootNode, ROSE_NewAndDeleteOperatorSourceFile);
  if (verbose)
    cout << "DONE: buildNewAndDeletOperators()" << endl;

  Grammar::writeFile(ROSE_NewAndDeleteOperatorSourceFile, target_directory,
                     getGrammarName() + "NewAndDeleteOperators", ".C");

  StringUtility::FileWithLineNumbers ROSE_TraverseMemoryPoolSourceFile;

  ROSE_TraverseMemoryPoolSourceFile.push_back(
      StringUtility::StringWithLineNumber(includeHeaderString, "", 1));
  ROSE_ASSERT(rootNode != NULL);

  buildTraverseMemoryPoolSupport(*rootNode, ROSE_TraverseMemoryPoolSourceFile);
  if (verbose)
    cout << "DONE: buildTraverseMemoryPoolSupport()" << endl;

  Grammar::writeFile(ROSE_TraverseMemoryPoolSourceFile, target_directory,
                     getGrammarName() + "TraverseMemoryPool", ".C");

  string returnClassHierarchySubTreeFileName =
      string(getGrammarName()) + "ReturnClassHierarchySubTree.C";
  ostringstream ROSE_returnClassHierarchySubTreeSourceFile;
  ROSE_ASSERT(ROSE_returnClassHierarchySubTreeSourceFile.good() == true);

  ROSE_returnClassHierarchySubTreeSourceFile << includeHeaderString;
  ROSE_ASSERT(rootNode != NULL);

  ROSE_returnClassHierarchySubTreeSourceFile
      << buildClassHierarchySubTreeFunction();
  // Include the classHierarchyCastTable in the file for fast casting between
  // compatible types
  ROSE_returnClassHierarchySubTreeSourceFile
      << generateClassHierarchyCastTable();
  if (verbose)
    cout << "DONE: buildClassHierarchySubTreeFunction()" << endl;
  writeGeneratedFile(target_directory + "/" +
                         returnClassHierarchySubTreeFileName,
                     ROSE_returnClassHierarchySubTreeSourceFile.str());

  string memoryPoolTraversalFileName = "AstQueryMemoryPool.h";
  ostringstream ROSE_memoryPoolTraversalSourceFile;
  ROSE_ASSERT(ROSE_memoryPoolTraversalSourceFile.good() == true);

  ROSE_ASSERT(rootNode != NULL);

  ROSE_memoryPoolTraversalSourceFile
      << buildMemoryPoolBasedVariantVectorTraversalSupport();
  if (verbose)
    cout << "DONE: buildMemoryPoolBasedVariantVectorTraversalSupport()" << endl;
  writeGeneratedFile(target_directory + "/" + memoryPoolTraversalFileName,
                     ROSE_memoryPoolTraversalSourceFile.str());

  StringUtility::FileWithLineNumbers ROSE_ReturnDataMemberPointersSourceFile;

  ROSE_ReturnDataMemberPointersSourceFile.push_back(
      StringUtility::StringWithLineNumber(includeHeaderString, "", 1));
  ROSE_ASSERT(rootNode != NULL);

  buildStringForReturnDataMemberPointersSupport(
      *rootNode, ROSE_ReturnDataMemberPointersSourceFile);
  if (verbose)
    cout << "DONE: buildStringForReturnDataMemberPointersSupport()" << endl;

  Grammar::writeFile(ROSE_ReturnDataMemberPointersSourceFile, target_directory,
                     getGrammarName() + "ReturnDataMemberPointers", ".C");

  StringUtility::FileWithLineNumbers
      ROSE_ProcessDataMemberReferenceToPointersSourceFile;

  ROSE_ProcessDataMemberReferenceToPointersSourceFile.push_back(
      StringUtility::StringWithLineNumber(includeHeaderString, "", 1));
  ROSE_ASSERT(rootNode != NULL);

  buildStringForProcessDataMemberReferenceToPointersSupport(
      *rootNode, ROSE_ProcessDataMemberReferenceToPointersSourceFile);
  if (verbose)
    cout << "DONE: buildStringForProcessDataMemberReferenceToPointersSupport()"
         << endl;

  Grammar::writeFile(
      ROSE_ProcessDataMemberReferenceToPointersSourceFile, target_directory,
      getGrammarName() + "ProcessDataMemberReferenceToPointers", ".C");

  StringUtility::FileWithLineNumbers ROSE_CopyMemberFunctionsSourceFile;

  ROSE_CopyMemberFunctionsSourceFile << includeHeaderString;
  ROSE_CopyMemberFunctionsSourceFile << "#include \"fixupCopy.h\"\n";
  ROSE_CopyMemberFunctionsSourceFile << R"cpp(
namespace {
void requireClassDefinitionOwnerForCopy(const SgNode *original) {
  const SgClassDefinition *definition = isSgClassDefinition(original);
  if (const auto *declaration = isSgClassDeclaration(original))
    definition = declaration->get_definition();
  if (definition == nullptr)
    return;
  const auto *declaration = isSgClassDeclaration(definition->get_parent());
  if (declaration == nullptr || declaration->get_definition() != definition ||
      declaration->get_definingDeclaration() != declaration) {
    fprintf(stderr,
            "REX_COPY_INVARIANT[class-definition-owner]: "
            "definition=%p parent=%p/%s parent-definition=%p "
            "defining-declaration=%p\n",
            static_cast<const void *>(definition),
            static_cast<const void *>(definition->get_parent()),
            definition->get_parent() != nullptr
                ? definition->get_parent()->class_name().c_str()
                : "<null>",
            static_cast<const void *>(declaration != nullptr
                                          ? declaration->get_definition()
                                          : nullptr),
            static_cast<const void *>(
                declaration != nullptr ? declaration->get_definingDeclaration()
                                       : nullptr));
    ROSE_ABORT();
  }
}

void completeNodeCopy(const SgNode *original, SgNode *result, SgCopyHelp &help) {
  help.decrementDepth();
  if (help.get_depth() == 0) {
    help.prepareRootCopyForFixup();
    help.requireBoundCopiedReferences();
    original->fixupCopy_scopes(result, help);
    original->fixupCopy_symbols(result, help);
    original->fixupCopy_references(result, help);
    help.finalizeRootCopy();
  }
}

void prepareConstructorReference(SgNode *reference, SgCopyHelp &help) {
  if (auto *type = isSgType(reference)) {
    SageInterface::materializeTypeWithCopyMap(type, help, true);
    return;
  }
  if (auto *declaration = isSgDeclarationStatement(reference)) {
    auto *primary = declaration;
    std::set<const SgDeclarationStatement *> using_declarations;
    while (auto *using_declaration = isSgUsingDeclarationStatement(primary)) {
      ROSE_ASSERT(using_declarations.insert(using_declaration).second);
      primary = using_declaration->get_declaration();
      ASSERT_not_null(primary);
    }
    if (isSgTemplateTypedefDeclaration(primary) == nullptr)
      return;
    const auto &copies = help.get_copiedNodeMap();
    const auto existing = copies.find(declaration);
    if (existing != copies.end() && existing->second != declaration) {
      ROSE_ASSERT(existing->second != nullptr &&
                  existing->second->variantT() == declaration->variantT());
      return;
    }
    for (SgNode *owner = declaration; owner != nullptr;
         owner = owner->get_parent()) {
      const auto mapped_owner = copies.find(owner);
      if (!help.isCopyingOriginal(owner) &&
          (mapped_owner == copies.end() || mapped_owner->second == owner))
        continue;
      if (auto *using_declaration = isSgUsingDeclarationStatement(declaration))
        prepareConstructorReference(using_declaration->get_declaration(), help);
      ROSE_ASSERT(!help.isCopyingOriginal(declaration));
      auto *copy = isSgDeclarationStatement(
          help.copyOrLookupOwnedAst(declaration));
      ROSE_ASSERT(copy != nullptr && copy != declaration &&
                  copy->variantT() == declaration->variantT() &&
                  copies.at(declaration) == copy);
      return;
    }
    return;
  }
  if (auto *group = isSgNameGroup(reference)) {
    if (help.get_copiedNodeMap().count(group) != 0)
      return;
    auto *owner = isSgNamelistStatement(group->get_parent());
    ROSE_ASSERT(owner != nullptr);
    const auto scope = help.get_copiedNodeMap().find(owner->get_scope());
    if (scope != help.get_copiedNodeMap().end() &&
        scope->second != owner->get_scope())
      help.copyOrLookupOwnedAst(owner);
    return;
  }
  const auto *nonreal = isSgNonrealSymbol(reference);
  const bool nonrealClass =
      nonreal != nullptr &&
      isSgClassDeclaration(
          nonreal->get_declaration()->get_templateDeclaration()) != nullptr;
  auto *initialized_name = isSgInitializedName(reference);
  if (initialized_name == nullptr &&
      isSgVariableSymbol(reference) == nullptr &&
      isSgClassSymbol(reference) == nullptr &&
      !nonrealClass &&
      isSgFunctionSymbol(reference) == nullptr &&
      isSgNamelistSymbol(reference) == nullptr &&
      isSgAliasSymbol(reference) == nullptr)
    return;
  auto *symbol = isSgSymbol(reference);
  if (initialized_name == nullptr)
    ASSERT_not_null(symbol);
  SgNode *basis = initialized_name != nullptr
                      ? initialized_name
                      : symbol->get_symbol_basis();
  ASSERT_not_null(basis);
  auto *scope = initialized_name != nullptr ? initialized_name->get_scope()
                                            : symbol->get_scope();
  ASSERT_not_null(scope);
  const auto &copies = help.get_copiedNodeMap();
  const auto owner = copies.find(scope);
  const auto mapped_basis = copies.find(basis);
  if ((owner == copies.end() || owner->second == scope) &&
      (mapped_basis == copies.end() || mapped_basis->second == basis)) {
    ROSE_ASSERT(!help.isCopyingOriginal(scope));
    ROSE_ASSERT(!help.isCopyingOriginal(basis));
    return;
  }
  if (auto *alias = isSgAliasSymbol(symbol)) {
    ASSERT_not_null(alias->get_alias());
    prepareConstructorReference(alias->get_alias(), help);
  } else if (auto *rename = isSgRenameSymbol(symbol)) {
    ASSERT_not_null(rename->get_original_symbol());
    prepareConstructorReference(rename->get_original_symbol(), help);
  } else if (copies.count(basis) == 0) {
    auto *name = isSgInitializedName(basis);
    auto *group = isSgNameGroup(basis);
    SgNode *declaration = name != nullptr ? name->get_declptr()
                         : group != nullptr ? group->get_parent() : basis;
    ASSERT_not_null(declaration);
    help.copyOrLookupOwnedAst(declaration);
    ROSE_ASSERT(copies.count(basis) != 0 && copies.at(basis) != basis);
  }
  if (initialized_name != nullptr) {
    ROSE_ASSERT(copies.count(basis) != 0 && copies.at(basis) != basis);
    return;
  }
  auto *copy = isSgSymbol(help.copyOrLookupAst(symbol));
  auto expected = copies.find(basis);
  ROSE_ASSERT(copy != nullptr && copy != symbol &&
              copy->get_symbol_basis() ==
                  (expected != copies.end() ? expected->second : basis));
}
}
)cpp";
  ROSE_ASSERT(rootNode != NULL);

  buildCopyMemberFunctions(*rootNode, ROSE_CopyMemberFunctionsSourceFile);
  if (verbose)
    cout << "DONE: buildCopyMemberFunctions()" << endl;

  Grammar::writeFile(ROSE_CopyMemberFunctionsSourceFile, target_directory,
                     getGrammarName() + "CopyMemberFunctions", ".C");

  // ---------------------------------------------------------------------------------------------
  // generate a function for each node in the AST to return the node's
  // successors of the traversal
  // ---------------------------------------------------------------------------------------------
  StringUtility::FileWithLineNumbers ROSE_treeTraversalFunctionsSourceFile;
  if (verbose)
    cout << "Calling buildTreeTraversalFunctions() ..." << endl;
  // Write header string to file (it's the same string as above, we just reuse
  // it)
  ROSE_treeTraversalFunctionsSourceFile << includeHeaderString;
  ROSE_treeTraversalFunctionsSourceFile << "#include <set>\n\n";

  // DQ (12/31/2005): Insert "using namespace std;" into the source file (but
  // never into the header files!)
  ROSE_treeTraversalFunctionsSourceFile
      << "\n// Simplify code by using std namespace (never put into header "
         "files since it effects users) \nusing namespace std;\n\n";

  // Generate the implementations of the tree traversal functions
  buildTreeTraversalFunctions(*rootNode, ROSE_treeTraversalFunctionsSourceFile);
  if (verbose)
    cout << "DONE: buildTreeTraversalFunctions()" << endl;
  Grammar::writeFile(ROSE_treeTraversalFunctionsSourceFile, target_directory,
                     getGrammarName() + "TreeTraversalSuccessorContainer",
                     ".C");

  // ---------------------------------------------------------------------------------------------
  // generate what is necessary for SAGE support in AstProcessing classes
  // ---------------------------------------------------------------------------------------------
  if (verbose)
    cout << "building TreeTraversalAccessEnums ... ";
  string treeTraversalClassHeaderFileName = getGrammarName();
  treeTraversalClassHeaderFileName += "TreeTraversalAccessEnums.h";
  ostringstream ROSE_treeTraversalClassHeaderFile;
  ROSE_ASSERT(ROSE_treeTraversalClassHeaderFile.good() == true);
  ROSE_treeTraversalClassHeaderFile
      << "// GENERATED HEADER FILE --- DO NOT MODIFY!" << endl
      << "#include <cstddef>" << endl
      << endl;
  string traversalEnums;
  buildEnumForNode(*rootNode, traversalEnums);
  ROSE_treeTraversalClassHeaderFile << traversalEnums;
  writeGeneratedFile(target_directory + "/" + treeTraversalClassHeaderFileName,
                     ROSE_treeTraversalClassHeaderFile.str());
  if (verbose)
    cout << "finished." << endl;

  // --------------------------------------------
  // generate code for variantT enum names
  // --------------------------------------------
  string variantEnumNamesFileName =
      string(getGrammarName()) + "VariantEnumNames.C";
  ostringstream variantEnumNamesFile;
  ROSE_ASSERT(variantEnumNamesFile.good() == true);
  string variantEnumNames = buildVariantEnumNames();

  // DQ (4/8/2004): Maybe we need a more obscure name to prevent global name
  // space pollution?
  variantEnumNamesFile << "\n#include \"rosedll.h\"\n ROSE_DLL_API const char* "
                          "roseGlobalVariantNameList[] = { \n"
                       << variantEnumNames << "\n};\n\n";
  writeGeneratedFile(target_directory + "/" + variantEnumNamesFileName,
                     variantEnumNamesFile.str());

  // --------------------------------------------
  // generate code for RTI support
  // --------------------------------------------
  string rtiFunctionsSourceFileName = string(getGrammarName()) + "RTI.C";
  StringUtility::FileWithLineNumbers rtiFile;
  rtiFile << includeHeaderString;

  // DQ (12/31/2005): Insert "using namespace std;" into the source file (but
  // never into the header files!)
  rtiFile << "\n// Simplify code by using std namespace (never put into header "
             "files since it effects users) \nusing namespace std;\n\n";

  buildRTIFile(rootNode, rtiFile);
  if (verbose)
    cout << "DONE: buildRTIFile" << endl;
  Grammar::writeFile(rtiFile, target_directory, getGrammarName() + "RTI", ".C");

  // -----------------------------------------------------------------------------------------------------------------------
  // generate code for new form of constructor without source position
  // information (this code generation must be done LAST!)
  // -----------------------------------------------------------------------------------------------------------------------

  StringUtility::FileWithLineNumbers ROSE_NewConstructorsSourceFile;

  ROSE_NewConstructorsSourceFile << includeHeaderString;

  // Now build the source code for the terminals and non-terminals in the
  // grammar
  ROSE_ASSERT(rootNode != NULL);

  // Modify the tree to mark the Sg_File_Info* in the SgLocatedNode to NOT
  // be a constructor parameter. Then regenerate the code.
  markNodeForConstructorWithoutSourcePositionInformationSupport(*rootNode);

  buildConstructorWithoutSourcePositionInformationSupport(
      *rootNode, ROSE_NewConstructorsSourceFile);

  Grammar::writeFile(ROSE_NewConstructorsSourceFile, target_directory,
                     getGrammarName() + "NewConstructors", ".C");

  return;
}

/////////////////////////
// RTI CODE GENERATION //
/////////////////////////
void Grammar::buildRTIFile(AstNodeClass *rootNode,
                           StringUtility::FileWithLineNumbers &rtiFile) {
  string result;
  result += "// generated file\n";
  result += "#include \"rtiHelpers.h\"\n";
  generateRTIImplementation(*rootNode, result);
  rtiFile.push_back(StringUtility::StringWithLineNumber(result, "", 1));
}

void Grammar::generateRTIImplementation(AstNodeClass &node, string &source) {
  vector<GrammarString *> includeList = classMemberIncludeList(node);
  includeList.erase(std::remove_if(includeList.begin(), includeList.end(),
                                   [](const GrammarString *member) {
                                     return !member->includeInRTI();
                                   }),
                    includeList.end());
  ostringstream ss;
  ss << RTIreturnType << endl << node.getName() << "::roseRTI() {" << endl;
  ss << RTIreturnType << " " << RTIContainerName << "(" << includeList.size()
     << ");\n\n";
  for (size_t index = 0; index < includeList.size(); ++index) {
    ss << generateRTICode(includeList[index], RTIContainerName, node.getName(),
                          index);
  }

  ss << "return " << RTIContainerName << ";\n}" << endl;

  source += ss.str();
  for (AstNodeClass *subclass : node.subclasses) {
    ROSE_ASSERT(subclass != nullptr);
    ROSE_ASSERT(subclass->getBaseClass() != nullptr);
    generateRTIImplementation(*subclass, source);
  }
}

// MS: 2002: generate source for adding RTI information to node (more detailed
// than C++ RTI info!) this info is used in PDF and dot output
string Grammar::generateRTICode(GrammarString *gs,
                                string dataMemberContainerName,
                                string className, size_t index) {
  string memberVariableName = gs->getVariableNameString();
  string typeString = string(gs->getTypeNameString());
  ostringstream ss;

  ss << "doRTI(\"" << memberVariableName << "\", &p_" << memberVariableName
     << ", (void*)this, \"" << className << "\", \"" << typeString << "\", \"p_"
     << memberVariableName << "\", makeRTIValueDescriptor(p_"
     << memberVariableName << "), " << dataMemberContainerName << "[" << index
     << "]);\n";
  return ss.str();
}

//======================================================================
// BUILD TRAVERSAL SUCCESSOR CONTAINER CREATION CODE
//======================================================================
namespace {
string traversalAccessExpression(const GrammarString *member) {
  if (member->getToBeDeleted() == DELETE_IF_OWNED) {
    ROSE_ASSERT(
        member->getTraversalStorage() == NODE_POINTER_TRAVERSAL_MEMBER &&
        member->getTraversalCardinality() == OPTIONAL_TRAVERSAL_MEMBER &&
        member->getTraversalAccessor() == DIRECT_TRAVERSAL_ACCESS);
    const string field = "p_" + member->getVariableNameString();
    return "(" + field + " != nullptr && " + field +
           "->get_parent() == this ? " + field + " : nullptr)";
  }
  switch (member->getTraversalAccessor()) {
  case DIRECT_TRAVERSAL_ACCESS:
    return "p_" + member->getVariableNameString();
  case COMPUTED_BASE_TYPE_DECLARATION_ACCESS:
    return "compute_baseTypeDefiningDeclaration()";
  case COMPUTED_CLASS_DEFINITION_ACCESS:
    return "compute_classDefinition()";
  }
  fprintf(stderr,
          "REX_ROSETTA_INVARIANT[traversal-accessor]: unknown typed traversal "
          "accessor for member %s\n",
          member->getVariableNameString().c_str());
  ROSE_ABORT();
}

TraversalCardinalityEnum
effectiveTraversalCardinality(const AstNodeClass &node,
                              const GrammarString *member) {
  ROSE_ASSERT(member != nullptr);
  const auto &overrides = node.getTraversalCardinalityOverrides();
  const auto override = overrides.find(member->getVariableNameString());
  if (member->getTraversalStorage() ==
      NODE_POINTER_CONTAINER_TRAVERSAL_MEMBER) {
    if (override != overrides.end()) {
      fprintf(stderr,
              "REX_ROSETTA_INVARIANT[traversal-cardinality-override]: node=%s "
              "member=%s is a container traversal edge\n",
              node.getName().c_str(), member->getVariableNameString().c_str());
      ROSE_ABORT();
    }
    // Containers have dynamic cardinality; this value is never consulted by
    // their generation path.
    return REQUIRED_TRAVERSAL_MEMBER;
  }
  if (override == overrides.end()) {
    return member->getTraversalCardinality();
  }
  if (member->getTraversalStorage() != NODE_POINTER_TRAVERSAL_MEMBER) {
    fprintf(stderr,
            "REX_ROSETTA_INVARIANT[traversal-cardinality-override]: node=%s "
            "member=%s is not one scalar traversal edge\n",
            node.getName().c_str(), member->getVariableNameString().c_str());
    ROSE_ABORT();
  }
  return override->second;
}
} // namespace

// MS: This method is used to write the individual tree traversal functions to
// the specified output file. It only generates the code for creating a
// container of successors (of AST nodes) at run time.
void Grammar::buildTreeTraversalFunctions(
    AstNodeClass &node, StringUtility::FileWithLineNumbers &outputFile) {
  string successorContainerName = "traversalSuccessorContainer";

  if (node.getCanHaveInstances()) {
    vector<GrammarString *> includeList = classMemberIncludeList(node);
    vector<GrammarString *>::iterator stringListIterator;

    // MS: generate the reduced list of traversed data members
    vector<GrammarString *> traverseDataMemberList;
    for (stringListIterator = includeList.begin();
         stringListIterator != includeList.end(); stringListIterator++) {
      if ((*stringListIterator)->getToBeTraversed() == DEF_TRAVERSAL) {
        traverseDataMemberList.push_back(*stringListIterator);
      }
    }
    for (const auto &overrideEntry : node.getTraversalCardinalityOverrides()) {
      const std::string &memberName = overrideEntry.first;
      const size_t matchingMembers = std::count_if(
          traverseDataMemberList.begin(), traverseDataMemberList.end(),
          [&](const GrammarString *member) {
            return member != nullptr &&
                   member->getVariableNameString() == memberName;
          });
      if (matchingMembers != 1) {
        fprintf(stderr,
                "REX_ROSETTA_INVARIANT[traversal-cardinality-override]: "
                "node=%s member=%s matches %zu traversed schema edges\n",
                node.getName().c_str(), memberName.c_str(), matchingMembers);
        ROSE_ABORT();
      }
    }
    // start: generate get_traversalSuccessorContainer() method
    outputFile << "vector<" << grammarPrefixName << "Node*>\n"
               << node.getName()
               << "::get_traversalSuccessorContainer() const {\n"
               << "  vector<" << grammarPrefixName << "Node*> "
               << successorContainerName << ";\n";
    // GB (8/1/2007): Preallocating the memory needed for the traversal
    // successors to avoid frequent reallocations on push_back. This makes
    // things a little more efficient.
    if (traverseDataMemberList.size() > 0) {
      outputFile << "  " << successorContainerName << ".reserve("
                 << generateNumberOfSuccessorsComputation(
                        traverseDataMemberList)
                 << ");\n";
    }
    if (!traverseDataMemberList.empty())
      outputFile << "  set<SgNode*> traversalSuccessorIdentities;\n";
    for (GrammarString *member : traverseDataMemberList)
      outputFile << generateTraverseSuccessor(
          member, successorContainerName, node.getName(),
          effectiveTraversalCardinality(node, member));
    outputFile << "return " << successorContainerName << ";\n}\n";
    // end: generate get_traversalSuccessorContainer() method

    // start: generate get_traversalSuccessorNamesContainer() method
    outputFile << "vector<string>\n"
               << node.getName()
               << "::get_traversalSuccessorNamesContainer() const {\n"
               << "const size_t validatedSuccessorCount = "
                  "get_traversalSuccessorContainer().size();\n"
               << "vector<string> " << successorContainerName << ";\n";
    if (!traverseDataMemberList.empty())
      outputFile << "size_t i = 0;\n";
    for (vector<GrammarString *>::iterator iter =
             traverseDataMemberList.begin();
         iter != traverseDataMemberList.end(); iter++) {
      outputFile << generateTraverseSuccessorNames(
          *iter, successorContainerName,
          effectiveTraversalCardinality(node, *iter));
    }
    outputFile << "if (" << successorContainerName
               << ".size() != validatedSuccessorCount) {\n"
               << "  cerr << \"REX_AST_INVARIANT[traversal-successor-names]: "
                  "schema name count disagrees with validated successors for "
               << node.getName() << "\" << endl;\n"
               << "  ROSE_ABORT();\n"
               << "}\n"
               << "return " << successorContainerName
               << ";\n}\n"; // end of function
    // end: generate get_traversalSuccessorNamesContainer() method

    outputFile << "vector<bool>\n"
               << node.getName()
               << "::get_traversalSuccessorIsListElement() const {\n"
               << "const size_t validatedSuccessorCount = "
                  "get_traversalSuccessorContainer().size();\n"
               << "vector<bool> listElements;\n"
               << "listElements.reserve(validatedSuccessorCount);\n";
    for (GrammarString *member : traverseDataMemberList) {
      if (member->getTraversalStorage() ==
          NODE_POINTER_CONTAINER_TRAVERSAL_MEMBER) {
        outputFile << "listElements.insert(listElements.end(), p_"
                   << member->getVariableNameString() << ".size(), true);\n";
      } else if (effectiveTraversalCardinality(node, member) ==
                 OPTIONAL_TRAVERSAL_MEMBER) {
        outputFile << "if (" << traversalAccessExpression(member)
                   << " != NULL) listElements.push_back(false);\n";
      } else {
        outputFile << "listElements.push_back(false);\n";
      }
    }
    outputFile << "if (listElements.size() != validatedSuccessorCount) {\n"
               << "  cerr << \"REX_AST_INVARIANT[traversal-list-membership]: "
                  "schema cardinality disagrees with validated successors for "
               << node.getName() << "\" << endl;\n"
               << "  ROSE_ABORT();\n"
               << "}\n"
               << "return listElements;\n}\n";

    // start: generate get_numberOfTraversalSuccessors() method
    // GB (09/25/2007): Added this method.
    outputFile << "size_t\n"
               << node.getName()
               << "::get_numberOfTraversalSuccessors() const {\n";
    outputFile << "return get_traversalSuccessorContainer().size();\n";
    outputFile << "}\n";
    // end: generate get_numberOfTraversalSuccessors() method

    // start: generate get_traversalSuccessorByIndex() method
    // GB (09/25/2007): Added this method.
    outputFile << "SgNode *\n"
               << node.getName()
               << "::get_traversalSuccessorByIndex(size_t idx) const {\n";
    outputFile << "const SgNodePtrList successors = "
                  "get_traversalSuccessorContainer();\n"
               << "if (idx >= successors.size()) {\n"
               << "  cerr << \"REX_AST_INVARIANT[traversal-successor-index]: "
                  "invalid index \" << idx << \" for "
               << node.getName() << "\" << endl;\n"
               << "  ROSE_ABORT();\n"
               << "}\n"
               << "return successors[idx];\n";
    outputFile << "}\n";
    // end: generate get_traversalSuccessorByIndex() method

    // start: generate get_childIndex() method
    // GB (09/25/2007): Added this method.
    outputFile << "size_t\n"
               << node.getName() << "::get_childIndex(SgNode *child) const {\n";
    outputFile
        << "if (child == NULL) {\n"
        << "  cerr << \"REX_AST_INVARIANT[traversal-child-index]: "
           "null child query for "
        << node.getName() << "\" << endl;\n"
        << "  ROSE_ABORT();\n"
        << "}\n"
        << "const SgNodePtrList successors = "
           "get_traversalSuccessorContainer();\n"
        << "const auto childPosition = "
           "std::find(successors.begin(), successors.end(), child);\n"
        << "if (childPosition == successors.end()) {\n"
        << "  cerr << \"REX_AST_INVARIANT[traversal-child-index]: "
           "foreign child query for "
        << node.getName() << "\" << endl;\n"
        << "  ROSE_ABORT();\n"
        << "}\n"
        << "return static_cast<size_t>(std::distance(successors.begin(), "
           "childPosition));\n";
    outputFile << "}\n";
    // end: generate get_childIndex() method
  } else {
    // *** The tree traversal function for the current class issues an error
    // message
    // *** and causes the program to abort. Build this code string and write it
    // to
    // *** the outputFile

    outputFile << "vector<" << grammarPrefixName << "Node*>\n"
               << node.getName()
               << "::get_traversalSuccessorContainer() const {\n";
    outputFile
        << "   cerr << \"Internal error(!): called tree traversal mechanism "
           "for illegal object: \" << endl\n"
        << "<< \"static: " << node.getName()
        << "\" << endl << \"dynamic:  \" << this->sage_class_name() << endl;\n"
        << "cerr << \"Aborting ...\" << endl;\n"
        << "ROSE_ABORT();\n"
        << "__builtin_unreachable();\n }\n\n";

    outputFile << "vector<bool>\n"
               << node.getName()
               << "::get_traversalSuccessorIsListElement() const {\n"
               << "  cerr << \"REX_AST_INVARIANT[illegal-traversal]: "
               << node.getName() << "\" << endl;\n"
               << "  ROSE_ABORT();\n"
               << "  __builtin_unreachable();\n"
               << "}\n";

    outputFile << "vector<string>\n"
               << node.getName()
               << "::get_traversalSuccessorNamesContainer() const {\n";
    outputFile
        << "   cerr << \"Internal error(!): called tree traversal mechanism "
           "for illegal object: \" << endl\n"
        << "<< \"static: " << node.getName()
        << "\" << endl << \"dynamic:  \" << this->sage_class_name() << endl;\n"
        << "cerr << \"Aborting ...\" << endl;\n"
        << "ROSE_ABORT();\n"
        << "__builtin_unreachable();\n }\n\n";

    // GB (09/25/2007): Added implementations for the new methods
    // get_numberOfTraversalSuccessors, get_traversalSuccessorByIndex, and
    // get_childIndex.
    outputFile << "size_t\n"
               << node.getName()
               << "::get_numberOfTraversalSuccessors() const {\n";
    outputFile << "   cerr << \"Internal error(!): called tree traversal "
                  "mechanism for illegal object: \" << endl\n"
               << "<< \"static: " << node.getName()
               << "\" << endl << \"dynamic:  this = \" << this << \" = \" << "
                  "this->sage_class_name() << endl;\n"
               << "cerr << \"Aborting ...\" << endl;\n"
               << "ROSE_ABORT();\n"
               << "__builtin_unreachable();\n }\n\n";

    outputFile << "SgNode*\n"
               << node.getName()
               << "::get_traversalSuccessorByIndex(size_t) const {\n";
    outputFile
        << "   cerr << \"Internal error(!): called tree traversal mechanism "
           "for illegal object: \" << endl\n"
        << "<< \"static: " << node.getName()
        << "\" << endl << \"dynamic:  \" << this->sage_class_name() << endl;\n"
        << "cerr << \"Aborting ...\" << endl;\n"
        << "ROSE_ABORT();\n"
        << "__builtin_unreachable();\n }\n\n";

    outputFile << "size_t\n"
               << node.getName() << "::get_childIndex(SgNode *) const {\n";
    outputFile
        << "   cerr << \"Internal error(!): called tree traversal mechanism "
           "for illegal object: \" << endl\n"
        << "<< \"static: " << node.getName()
        << "\" << endl << \"dynamic:  \" << this->sage_class_name() << endl;\n"
        << "cerr << \"Aborting ...\" << endl;\n"
        << "ROSE_ABORT();\n"
        << "__builtin_unreachable();\n }\n\n";
  }

  // Traverse all nodes of the grammar recursively and build the tree traversal
  // function for each of them
  vector<AstNodeClass *>::iterator treeNodeIterator;
  for (treeNodeIterator = node.subclasses.begin();
       treeNodeIterator != node.subclasses.end(); treeNodeIterator++) {
    ROSE_ASSERT((*treeNodeIterator) != NULL);
    ROSE_ASSERT((*treeNodeIterator)->getBaseClass() != NULL);
    buildTreeTraversalFunctions(**treeNodeIterator, outputFile);
  }
}

/////////////////////////////////////////////////
// traversalSuccessorContainer Code Generation //
/////////////////////////////////////////////////

// GB (8/16/2007): Generate the pre-allocation of the traversal successor
// container. We know the size beforehand, so calling reserve saves some time
// as it avoids repeated reallocations on push_back. The size of the container
// is an upper bound: the number of scalar members plus the sizes of pointer
// containers. Optional null scalar members may leave unused capacity.
string Grammar::generateNumberOfSuccessorsComputation(
    vector<GrammarString *> &traverseDataMemberList) {
  stringstream travSuccSource;
  if (!traverseDataMemberList.empty()) {
    int singleSuccessors = 0;
    for (GrammarString *member : traverseDataMemberList) {
      const string memberVariableName = member->getVariableNameString();
      if (member->getTraversalStorage() ==
          NODE_POINTER_CONTAINER_TRAVERSAL_MEMBER) {
        travSuccSource << "p_" << memberVariableName << ".size() + ";
      } else {
        singleSuccessors++;
      }
    }

    // Sum all dynamic containers plus the fixed successors.  Traversal order
    // is the schema declaration order and is independent of container count.
    travSuccSource << singleSuccessors;
  }
  return travSuccSource.str();
}

// MS: 03/11/02: new TraversalMechanism
// generate source for adding successors of a node to the successors container.
string Grammar::generateTraverseSuccessor(
    GrammarString *gs, string successorContainerName, const string &nodeName,
    TraversalCardinalityEnum cardinality) {
  const string memberName = gs->getVariableNameString();
  const string expression = traversalAccessExpression(gs);
  const string successorName = "traversalSuccessor_" + memberName;
  const string identityCheck =
      "if (!traversalSuccessorIdentities.insert(" + successorName +
      ").second) {\n"
      "  cerr << \"REX_AST_INVARIANT[traversal-duplicate-successor]: "
      "duplicate successor identity in " +
      nodeName + ".p_" + memberName +
      "\" << endl;\n"
      "  ROSE_ABORT();\n"
      "}\n";
  if (gs->getTraversalStorage() == NODE_POINTER_CONTAINER_TRAVERSAL_MEMBER) {
    return "for (auto *" + successorName + " : p_" + memberName +
           ") {\n"
           "if (" +
           successorName +
           " == NULL) {\n"
           "  cerr << \"REX_AST_INVARIANT[traversal-null-"
           "successor]: null successor in " +
           nodeName + ".p_" + memberName +
           "\" << endl;\n"
           "  ROSE_ABORT();\n"
           "}\n" +
           identityCheck + "  " + successorContainerName + ".push_back(" +
           successorName + ");\n}\n";
  }
  if (cardinality == OPTIONAL_TRAVERSAL_MEMBER) {
    return "auto *const " + successorName + " = " + expression +
           ";\n"
           "if (" +
           successorName + " != NULL) {\n" + identityCheck + "  " +
           successorContainerName + ".push_back(" + successorName + ");\n}\n";
  }
  return "auto *const " + successorName + " = " + expression +
         ";\n"
         "if (" +
         successorName +
         " == NULL) {\n"
         "  cerr << \"REX_AST_INVARIANT[traversal-required-"
         "successor]: required successor " +
         nodeName + ".p_" + memberName +
         " is null\" << endl;\n"
         "  ROSE_ABORT();\n"
         "}\n" +
         identityCheck + successorContainerName + ".push_back(" +
         successorName + ");\n";
}

// MS: 06/28/02:
// generate source for adding successor names of a node to the successornames
// container.
string
Grammar::generateTraverseSuccessorNames(GrammarString *gs,
                                        string successorContainerName,
                                        TraversalCardinalityEnum cardinality) {
  const string memberName = gs->getVariableNameString();
  if (gs->getTraversalStorage() == NODE_POINTER_CONTAINER_TRAVERSAL_MEMBER) {
    return "for (size_t member_index = 0; member_index < p_" + memberName +
           ".size(); ++member_index, ++i) {\n"
           "  char buf[64];\n"
           "  snprintf(buf, sizeof(buf), \"*[%zu]\", i);\n  " +
           successorContainerName + ".push_back(buf);\n}\n";
  }
  const string statement =
      successorContainerName + ".push_back(\"p_" + memberName + "\"); ++i;\n";
  if (cardinality == OPTIONAL_TRAVERSAL_MEMBER)
    return "if (" + traversalAccessExpression(gs) + " != NULL) {\n  " +
           statement + "}\n";
  return statement;
}

void Grammar::buildEnumForNode(AstNodeClass &node, string &allEnumsString) {
  for (AstNodeClass *subclass : node.subclasses) {
    ROSE_ASSERT(subclass != nullptr);
    ROSE_ASSERT(subclass->getBaseClass() != nullptr);
    buildEnumForNode(*subclass, allEnumsString);
  }
  vector<GrammarString *> fixedPrefix;
  for (GrammarString *member : classMemberIncludeList(node)) {
    if (member->getToBeTraversed() != DEF_TRAVERSAL)
      continue;
    if (member->getTraversalStorage() ==
            NODE_POINTER_CONTAINER_TRAVERSAL_MEMBER ||
        effectiveTraversalCardinality(node, member) ==
            OPTIONAL_TRAVERSAL_MEMBER)
      break;
    fixedPrefix.push_back(member);
  }
  if (fixedPrefix.empty())
    return;

  const string enumName = "E_" + node.getName();
  allEnumsString += "enum class " + enumName + " : std::size_t\n{\n";
  for (size_t i = 0; i < fixedPrefix.size(); ++i) {
    if (i != 0)
      allEnumsString += ", ";
    allEnumsString +=
        node.getName() + "_" + fixedPrefix[i]->getVariableNameString();
  }
  allEnumsString += "\n};\n";
  allEnumsString += "static constexpr std::size_t rosettaTraversalIndex(" +
                    enumName +
                    " member) noexcept\n{\n  return "
                    "static_cast<std::size_t>(member);\n}\n";
}

/////////////////////////////////
// GRAMMAR AUXILIARY FUNCTIONS //
/////////////////////////////////
vector<GrammarString *> Grammar::classMemberIncludeList(AstNodeClass &node) {
  // Determine the data members to be investigated (starting at the root of the
  // grammar)
  vector<GrammarString *> includeList;
  vector<GrammarString *> excludeList;

  ROSE_ASSERT(includeList.size() == 0);
  ROSE_ASSERT(excludeList.size() == 0);

  collectInheritedGrammarStrings(node, includeList, excludeList,
                                 &AstNodeClass::getMemberDataPrototypeList,
                                 AstNodeClass::LOCAL_LIST);

  // Now edit the lists to remove elements appearing within the exclude list
  editStringList(includeList, excludeList);
  return includeList;
}

AstNodeClass *lookupTerminal(const vector<AstNodeClass *> &tl,
                             const std::string &name) {
  for (vector<AstNodeClass *>::const_iterator it = tl.begin(); it != tl.end();
       ++it) {
    if ((*it)->getName() == name) {
      return *it;
    }
  }
  cerr << "Reached end of AstNodeClass list in search for '" << name << "'"
       << endl;
  ROSE_ABORT();
}
