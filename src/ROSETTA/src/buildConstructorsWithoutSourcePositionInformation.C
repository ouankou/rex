#include "AstNodeClass.h"
#include "ROSETTA_macros.h"
#include "grammar.h"
#include "grammarString.h"

using namespace std;
using namespace Rose;

void Grammar::markNodeForConstructorWithoutSourcePositionInformation(
    AstNodeClass &node) {
  if (node.name == "SgLocatedNode") {
    GrammarString *returnValue = getNamedDataMember(node);
    ROSE_ASSERT(returnValue != NULL);
    returnValue->setIsInConstructorParameterList(NO_CONSTRUCTOR_PARAMETER);
  }
}

void Grammar::markNodeForConstructorWithoutSourcePositionInformationSupport(
    AstNodeClass &node) {
  markNodeForConstructorWithoutSourcePositionInformation(node);

  // Call this function recursively on the children of this node in the tree
  vector<AstNodeClass *>::const_iterator treeNodeIterator;
  for (treeNodeIterator = node.subclasses.begin();
       treeNodeIterator != node.subclasses.end(); treeNodeIterator++) {
    ROSE_ASSERT((*treeNodeIterator) != NULL);
    ROSE_ASSERT((*treeNodeIterator)->getBaseClass() != NULL);

    markNodeForConstructorWithoutSourcePositionInformationSupport(
        **treeNodeIterator);
  }
}

GrammarString *Grammar::getNamedDataMember(AstNodeClass &node) {
  GrammarString *returnValue = NULL;
  const vector<GrammarString *> &terminalList = node.getMemberDataPrototypeList(
      AstNodeClass::LOCAL_LIST, AstNodeClass::INCLUDE_LIST);
  vector<GrammarString *>::const_iterator i = terminalList.begin();
  while (i != terminalList.end()) {
    if ((*i)->variableNameString == "startOfConstruct") {
      returnValue = *i;
    }
    i++;
  }

  return returnValue;
}

AstNodeClass *Grammar::getNamedNode(AstNodeClass &node, const string &name) {
  AstNodeClass *parentNode = node.getBaseClass();

  while (parentNode != NULL && string(parentNode->getName()) != name) {
    parentNode = parentNode->getBaseClass();
  }

  return parentNode;
}

StringUtility::FileWithLineNumbers
Grammar::buildConstructorWithoutSourcePositionInformation(AstNodeClass &node) {
  string className = node.getName();

  StringUtility::FileWithLineNumbers returnString;

  // Only located nodes need constructors without source-position arguments.
  AstNodeClass *parentNode = getNamedNode(node, "SgLocatedNode");

  if (parentNode != NULL && node.generateConstructor() == true) {
    string constructorTemplateFileName =
        "../Grammar/grammarConstructorDefinitionMacros.macro";
    StringUtility::FileWithLineNumbers constructorSourceCodeTemplate =
        Grammar::readFileWithPos(constructorTemplateFileName);

    StringUtility::FileWithLineNumbers constructorSource =
        constructorSourceCodeTemplate;
    if (node.getBaseClass() != NULL) {
      string parentClassName = node.getBaseClass()->getName();

      bool withInitializers = false;
      bool withTypes = false;
      string baseClassParameterString = buildConstructorParameterListString(
          *node.getBaseClass(), withInitializers, withTypes);
      string preInitializationString =
          ": " + parentClassName + "($BASECLASS_PARAMETERS)";
      preInitializationString = StringUtility::copyEdit(
          preInitializationString, "$BASECLASS_PARAMETERS",
          baseClassParameterString);
      constructorSource =
          StringUtility::copyEdit(constructorSource, "$PRE_INITIALIZATION_LIST",
                                  preInitializationString);
    } else {
      constructorSource = StringUtility::copyEdit(
          constructorSource, "$PRE_INITIALIZATION_LIST", "");
    }

    bool withInitializers = false;
    bool withTypes = true;
    string constructorParameterString =
        buildConstructorParameterListString(node, withInitializers, withTypes);
    constructorSource = StringUtility::copyEdit(constructorSource,
                                                "$CONSTRUCTOR_PARAMETER_LIST",
                                                constructorParameterString);
    constructorSource =
        StringUtility::copyEdit(constructorSource, "$CLASSNAME", className);

    string constructorFunctionBody = node.buildConstructorBody();
    constructorSource = StringUtility::copyEdit(
        constructorSource, "$CONSTRUCTOR_BODY", constructorFunctionBody);

    constructorSource = StringUtility::copyEdit(
        constructorSource, "$CONSTRUCTOR_ESSENTIAL_DATA_MEMBERS", "");

    returnString.insert(returnString.end(), constructorSource.begin(),
                        constructorSource.end());
  }

  returnString =
      StringUtility::copyEdit(returnString, "$GRAMMAR_PREFIX_", "Sg");

  return returnString;
}

void Grammar::buildConstructorWithoutSourcePositionInformationSupport(
    AstNodeClass &node, StringUtility::FileWithLineNumbers &outputFile) {
  StringUtility::FileWithLineNumbers editString =
      buildConstructorWithoutSourcePositionInformation(node);

  editString =
      StringUtility::copyEdit(editString, "$CLASSNAME", node.getName());
  editString = StringUtility::copyEdit(
      editString, "$GRAMMAR_NAME",
      getGrammarName()); // grammarName string defined in Grammar class
  // Set these to NULL strings if they are still present within the string
  editString =
      StringUtility::copyEdit(editString, "$CLASSTAG", node.getTagName());

  // Also output strings to single file
  outputFile += editString;

  // Call this function recursively on the children of this node in the tree
  vector<AstNodeClass *>::const_iterator treeNodeIterator;
  for (treeNodeIterator = node.subclasses.begin();
       treeNodeIterator != node.subclasses.end(); treeNodeIterator++) {
    ROSE_ASSERT((*treeNodeIterator) != NULL);
    ROSE_ASSERT((*treeNodeIterator)->getBaseClass() != NULL);

    buildConstructorWithoutSourcePositionInformationSupport(**treeNodeIterator,
                                                            outputFile);
  }
}
