#include "AstNodeClass.h"

#include "ROSETTA_macros.h"

#include "grammar.h"

#include "grammarString.h"

#include <memory>

using namespace std;
using namespace Rose;

#define BOOL2STR(b) ((b) ? "true" : "false")

SubclassListBuilder &SubclassListBuilder::operator|(const AstNodeClass &t) {
  children.push_back(const_cast<AstNodeClass *>(&t));
  return *this;
}

const std::vector<AstNodeClass *> &SubclassListBuilder::getList() const {
  return children;
}

namespace {
void clearGrammarStringList(std::vector<GrammarString *> &list) {
  for (GrammarString *entry : list) {
    delete entry;
  }
  list.clear();
}
} // namespace

AstNodeClass::~AstNodeClass() {
  for (int i = 0; i < 2; ++i) {
    for (int j = 0; j < 2; ++j) {
      clearGrammarStringList(memberFunctionPrototypeList[i][j]);
      clearGrammarStringList(memberDataPrototypeList[i][j]);
      clearGrammarStringList(memberFunctionSourceList[i][j]);
      clearGrammarStringList(editSubstituteTargetList[i][j]);
      clearGrammarStringList(editSubstituteSourceList[i][j]);
    }
  }
}

AstNodeClass::AstNodeClass(const string &lexemeString, Grammar &X,
                           const string &stringVar, const string &tagString,
                           bool canHaveInstances,
                           const SubclassListBuilder &builder)
    : name((stringVar.empty() ? lexemeString : stringVar)),
      baseName((stringVar.empty() ? lexemeString : stringVar)), baseClass(NULL),
      subclasses(builder.getList()), canHaveInstances(canHaveInstances),
      tag(tagString.empty() ? (name + "Tag") : tagString),
      predeclarationString(""), postdeclarationString(""),
      automaticGenerationOfDestructor(true),
      automaticGenerationOfConstructor(true),
      automaticGenerationOfCopyFunction(true), associatedGrammar(&X) {
  for (AstNodeClass *subclass : this->subclasses) {
    ROSE_ASSERT(subclass != nullptr);
    ROSE_ASSERT(subclass->getBaseClass() == nullptr);
    subclass->setBaseClass(this);
  }
  X.addGrammarElement(*this);
  ROSE_ASSERT(associatedGrammar != NULL);
}

void AstNodeClass::appendPreDefinitionText(const std::string &text) {
  preDefinitionText += text;
}

void AstNodeClass::setBaseClass(AstNodeClass *bc) { baseClass = bc; }

AstNodeClass *AstNodeClass::getBaseClass() const { return baseClass; }

void AstNodeClass::setTraversalCardinalityOverride(
    const std::string &memberName,
    TraversalCardinalityEnum traversalCardinality) {
  if (memberName.empty() ||
      !traversalCardinalityOverrides.emplace(memberName, traversalCardinality)
           .second) {
    fprintf(stderr,
            "REX_ROSETTA_INVARIANT[traversal-cardinality-override]: node=%s "
            "has an empty or duplicate override for member=%s\n",
            getName().c_str(), memberName.c_str());
    ROSE_ABORT();
  }
}

const std::map<std::string, TraversalCardinalityEnum> &
AstNodeClass::getTraversalCardinalityOverrides() const {
  return traversalCardinalityOverrides;
}

bool AstNodeClass::getCanHaveInstances() const { return canHaveInstances; }

void AstNodeClass::setMemberDestructor(const std::string &member,
                                       const std::string &operation) {
  ROSE_ASSERT(!member.empty() && !operation.empty());
  ROSE_ASSERT(memberDestructors.emplace(member, operation).second);
}

void AstNodeClass::setDestructorPrologue(const std::string &operation) {
  ROSE_ASSERT(!operation.empty() && destructorPrologue.empty());
  destructorPrologue = operation;
}

string AstNodeClass::buildDestructorOwnershipBody() {
  vector<GrammarString *> members =
      getMemberDataPrototypeList(LOCAL_LIST, INCLUDE_LIST);
  vector<GrammarString *> excluded =
      getMemberDataPrototypeList(LOCAL_LIST, EXCLUDE_LIST);
  Grammar::editStringList(members, excluded);
  string body;
  if (AstNodeClass *base = getBaseClass())
    body += "  " + base->getName() + "::appendDestructorOwnedNodes(nodes);\n";
  for (GrammarString *member : members) {
    if (member->getToBeDeleted() == DELETE_IF_OWNED) {
      const bool container =
          member->getSchemaStorage() == VALUE_CONTAINER_SCHEMA_STORAGE;
      const string field =
          container ? "owned" : "p_" + member->getVariableNameString();
      const string indent = container ? "    " : "  ";
      if (container)
        body += "  for (auto *owned : p_" + member->getVariableNameString() +
                ") {\n";
      body += indent + "ROSE_ASSERT(" + field +
              " == nullptr || SgNode::isLiveNode(" + field + "));\n";
      body += indent + "if (" + field + " != nullptr && " + field +
              "->get_parent() == this)\n";
      body += indent + "  SgNode::appendOwnedAstNode(nodes, " + field + ");\n";
      if (container)
        body += "  }\n";
    }
    if (member->getToBeDeleted() == DEF_DELETE)
      body += "  SgNode::appendOwnedAstNode(nodes, p_" +
              member->getVariableNameString() + ");\n";
  }
  return body;
}

string AstNodeClass::buildDestructorBody() {
  string returnString;
  vector<GrammarString *> localList;
  vector<GrammarString *> localExcludeList;
  vector<GrammarString *>::iterator stringListIterator;

  // Initialize with local node data
  localList = getMemberDataPrototypeList(AstNodeClass::LOCAL_LIST,
                                         AstNodeClass::INCLUDE_LIST);
  localExcludeList = getMemberDataPrototypeList(AstNodeClass::LOCAL_LIST,
                                                AstNodeClass::EXCLUDE_LIST);

  // Now edit the list to remove elements appearing within the exclude list
  Grammar::editStringList(localList, localExcludeList);

  returnString += "\n" + destructorPrologue;

  size_t customDestructors = 0;
  for (stringListIterator = localList.begin();
       stringListIterator != localList.end(); stringListIterator++) {
    // DEF_DELETE describes ownership independently of the spelling of a
    // member's C++ type. Let C++ distinguish pooled AST nodes from ordinary
    // owned objects; name-prefix whitelists silently leaked other classes.
    const std::string &varName = (*stringListIterator)->getVariableNameString();
    const auto custom = memberDestructors.find(varName);
    if (custom != memberDestructors.end()) {
      returnString += "    " + custom->second + "\n";
      ++customDestructors;
    } else if ((*stringListIterator)->getToBeDeleted() == DEF_DELETE) {
      returnString += "    SgNode::destroyOwnedData(p_" + varName + ");\n";
    } else if ((*stringListIterator)->getToBeDeleted() == DELETE_IF_OWNED) {
      const bool container = (*stringListIterator)->getSchemaStorage() ==
                             VALUE_CONTAINER_SCHEMA_STORAGE;
      const string field = container ? "owned" : "p_" + varName;
      const string indent = container ? "      " : "    ";
      if (container)
        returnString += "    for (auto *owned : p_" + varName + ") {\n";
      returnString += indent + "if (" + field +
                      " != nullptr && SgNode::isLiveNode(" + field + ") && " +
                      field + "->get_parent() == this)\n";
      returnString += indent + "  SgNode::destroyOwnedData(" + field + ");\n";
      if (container)
        returnString += "    }\n";
    }
  }
  ROSE_ASSERT(customDestructors == memberDestructors.size());

  returnString += "\n";

  ROSE_ASSERT(localExcludeList.size() == 0);

  // Ensure deleted nodes no longer look live when stale pointers are inspected.
  if (getName() == "SgNode") {
    returnString += "  get_globalMangledNameMap().erase(this);\n";
    returnString += "  TransformationTracking::clearId(this);\n";
    returnString += "  p_freepointer = nullptr;\n";
  }

  returnString =
      "  if (p_freepointer == ASTMemoryPool::validNodeMarker()) {\n" +
      returnString + "\n  }";
  returnString += "\n";

  return returnString;
}

string AstNodeClass::buildConstructorBody() {
  string returnString;

  vector<GrammarString *> localList;
  vector<GrammarString *> localExcludeList;
  vector<GrammarString *>::iterator stringListIterator;

  // Initialize with local node data
  localList = getMemberDataPrototypeList(AstNodeClass::LOCAL_LIST,
                                         AstNodeClass::INCLUDE_LIST);
  localExcludeList = getMemberDataPrototypeList(AstNodeClass::LOCAL_LIST,
                                                AstNodeClass::EXCLUDE_LIST);

  // Now edit the list to remove elements appearing within the exclude list
  Grammar::editStringList(localList, localExcludeList);

  for (stringListIterator = localList.begin();
       stringListIterator != localList.end(); stringListIterator++) {
    // QY 11/9/04  added additional types of constructor parameters
    string variableNameString = (*stringListIterator)->getVariableNameString();
    switch ((*stringListIterator)->getIsInConstructorParameterList()) {
    case NO_CONSTRUCTOR_PARAMETER:
      if (string((*stringListIterator)->getDefaultInitializerString()) != "") {
        returnString = returnString + "     p_" + variableNameString + " " +
                       (*stringListIterator)->getDefaultInitializerString() +
                       ";\n";
      }
      break;

    case CONSTRUCTOR_PARAMETER:
      returnString = returnString + "     p_" + variableNameString + " = " +
                     variableNameString + ";\n";
      break;
    default:
      ROSE_ABORT();
    }
  }

  ROSE_ASSERT(localExcludeList.size() == 0);

  return returnString;
}

StringUtility::FileWithLineNumbers
AstNodeClass::buildCopyMemberFunctionHeader() {
  StringUtility::FileWithLineNumbers returnString;
  if (automaticGenerationOfCopyFunction) {
    if (baseName == "Node") {
      returnString.push_back(StringUtility::StringWithLineNumber(
          "          virtual SgNode* copy ( SgCopyHelp& help) const;",
          "" /* "<copy member function>" */, 1));
    } else {
      returnString.push_back(StringUtility::StringWithLineNumber(
          "          virtual SgNode* copy ( SgCopyHelp& help) const override;",
          "" /* "<copy member function>" */, 1));
    }
  } else {
    returnString.push_back(StringUtility::StringWithLineNumber(
        "       // copy functions omitted for $CLASSNAME",
        "" /* "<copy member function>" */, 1));
  }

  return returnString;
}

StringUtility::FileWithLineNumbers
AstNodeClass::buildCopyMemberFunctionSource() {
  StringUtility::FileWithLineNumbers returnString;
  if (automaticGenerationOfCopyFunction == true) {
    string dependencyCopy = "", constructArgCopy = "", constructArgList = "",
           postConstructCopy = "", referenceCopy = "", stateCopy = "",
           earlyMemberCopy = "";
    string filename = "../Grammar/copyMemberFunction.macro";
    StringUtility::FileWithLineNumbers functionTemplateString =
        Grammar::readFileWithPos(filename);
    const bool emptyConstructorArg = !generateConstructor();

    for (AstNodeClass *t = this; t != NULL; t = t->getBaseClass()) {
      string constructArgCopy1 = "", constructArgList1 = "",
             postConstructCopy1 = "", stateCopy1 = "", referenceCopy1 = "";
      vector<GrammarString *> copyList;
      vector<GrammarString *>::iterator stringListIterator;

      copyList = t->getMemberDataPrototypeList(AstNodeClass::LOCAL_LIST,
                                               AstNodeClass::INCLUDE_LIST);

      for (stringListIterator = copyList.begin();
           stringListIterator != copyList.end(); stringListIterator++) {
        GrammarString *data = *stringListIterator;
        if (data->getToBeCopied() == NO_COPY_DATA) {
          continue;
        }

        if (data->getToBeCopied() == COPY_REFERENCE ||
            (data->getToBeDeleted() == DELETE_IF_OWNED &&
             data->getSchemaStorage() == SCALAR_SCHEMA_STORAGE)) {
          if (!emptyConstructorArg && data->getIsInConstructorParameterList() !=
                                          NO_CONSTRUCTOR_PARAMETER) {
            constructArgCopy1 += data->buildCopyMemberFunctionSource(true);
            constructArgList1 += ", " + data->getVariableNameString() + "_copy";
          }
          referenceCopy1 += data->buildCopyMemberFunctionSource(false);
          continue;
        }

        if (!emptyConstructorArg && data->getIsInConstructorParameterList() !=
                                        NO_CONSTRUCTOR_PARAMETER) {
          const string typeName = data->getTypeNameString();
          const bool deferOwnedMember =
              data->getCopyConstructionOrder() ==
                  COPY_AFTER_OWNER_CONSTRUCTION ||
              (data->getCopyConstructionOrder() ==
                   DEFAULT_COPY_CONSTRUCTION_ORDER &&
               data->getToBeTraversed() == DEF_TRAVERSAL &&
               data->getToBeDeleted() == DEF_DELETE &&
               data->getToBeCopied() == CLONE_TREE &&
               data->getTraversalStorage() == NODE_POINTER_TRAVERSAL_MEMBER);
          if (deferOwnedMember) {
            // Construct the owner before recursively copying its children.
            if (data->getTraversalStorage() ==
                NODE_POINTER_CONTAINER_TRAVERSAL_MEMBER)
              constructArgList1 += ", " + typeName + "()";
            else
              constructArgList1 += ", static_cast<" + typeName + ">(nullptr)";

            bool buildConstructorArgument = false;
            postConstructCopy1 +=
                data->buildCopyMemberFunctionSource(buildConstructorArgument);
            postConstructCopy1 +=
                data->buildCopyMemberFunctionSetParentSource();
          } else {
            bool buildConstructorArgument = true;
            const string copy =
                data->buildCopyMemberFunctionSource(buildConstructorArgument);
            if (data->getCopyConstructionOrder() ==
                COPY_BEFORE_CONSTRUCTOR_DEPENDENCIES)
              dependencyCopy += copy;
            else
              constructArgCopy1 += copy;
            constructArgList1 +=
                ", " + string(data->getVariableNameString()) + "_copy";

            if (data->getCopyConstructionOrder() !=
                COPY_BEFORE_CONSTRUCTOR_DEPENDENCIES)
              postConstructCopy +=
                  data->buildCopyMemberFunctionSetParentSource();
          }
        } else {
          bool buildConstructorArgument = false;

          switch (data->automaticGenerationOfDataAccessFunctions) {
          case NO_ACCESS_FUNCTIONS:
          case BUILD_LIST_ACCESS_FUNCTIONS:
          case BUILD_ACCESS_FUNCTIONS:
          case BUILD_LITERAL_VALUE_ACCESS_FUNCTIONS:
          case BUILD_FLAG_ACCESS_FUNCTIONS: {
            const string memberCopy =
                data->buildCopyMemberFunctionSource(buildConstructorArgument) +
                data->buildCopyMemberFunctionSetParentSource();
            if (data->getCopyConstructionOrder() == COPY_BEFORE_OTHER_MEMBERS)
              earlyMemberCopy += memberCopy;
            else if (data->getToBeTraversed() == NO_TRAVERSAL &&
                (data->getToBeCopied() == COPY_DATA ||
                 data->getToBeCopied() == COPY_CONSTRUCTOR_REFERENCE))
              stateCopy1 += memberCopy;
            else
              postConstructCopy1 += memberCopy;
            break;
          }

          default: {
            printf("Default reached in "
                   "AstNodeClass::buildCopyMemberFunctionSource \n");
            ROSE_ABORT();
          }
          }
        }
      }

      constructArgCopy = constructArgCopy1 + constructArgCopy;
      constructArgList = constructArgList1 + constructArgList;
      postConstructCopy = postConstructCopy1 + postConstructCopy;
      stateCopy = stateCopy1 + stateCopy;
      referenceCopy = referenceCopy1 + referenceCopy;
    }

    if (!constructArgList.empty())
      constructArgList.erase(0, 1);

    returnString = functionTemplateString;
    returnString = StringUtility::copyEdit(returnString, "virtual ", "");
    returnString = StringUtility::copyEdit(returnString, "$DEPENDENCY_COPY",
                                           dependencyCopy);
    returnString = StringUtility::copyEdit(returnString, "$CONSTRUCT_ARG_COPY",
                                           constructArgCopy);
    returnString = StringUtility::copyEdit(returnString, "$CONSTRUCT_ARG_LIST",
                                           constructArgList);
    returnString = StringUtility::copyEdit(returnString, "$POST_CONSTRUCT_COPY",
                                           earlyMemberCopy + postConstructCopy);
    returnString =
        StringUtility::copyEdit(returnString, "$REFERENCE_COPY", referenceCopy);
    returnString =
        StringUtility::copyEdit(returnString, "$STATE_COPY", stateCopy);
  }

  return returnString;
}

void AstNodeClass::setGrammar(Grammar *grammarPointer) {
  ROSE_ASSERT(grammarPointer != NULL);
  associatedGrammar = grammarPointer;
}

Grammar *AstNodeClass::getGrammar() const {
  ROSE_ASSERT(associatedGrammar != NULL);
  return associatedGrammar;
}

const string &AstNodeClass::getName() const {
  ROSE_ASSERT(!name.empty());
  return name;
}

const string &AstNodeClass::getBaseName() const { return baseName; }

void AstNodeClass::addGrammarPrefixToName() {
  string grammarName = "";
  ROSE_ASSERT(getGrammar() != NULL);
  grammarName = getGrammar()->getGrammarPrefixName();

  if (GrammarString::isContainedIn(name, grammarName)) {
    std::cerr << "Grammar's name already exists in the AstNodeClass's name"
              << "; name=" << name << ", grammarName=" << grammarName << "\n";
  }
  ROSE_ASSERT(!GrammarString::isContainedIn(name, grammarName));

  string newNameWithPrefix = grammarName + name;
  this->name = newNameWithPrefix;
}

const string &AstNodeClass::getTagName() const { return tag; }

void AstNodeClass::setFunctionPrototype(
    const GrammarString &inputMemberFunction) {
  AstNodeClass::addElementToList(
      getMemberFunctionPrototypeList(AstNodeClass::LOCAL_LIST,
                                     AstNodeClass::INCLUDE_LIST),
      inputMemberFunction);
}

std::unique_ptr<GrammarString>
AstNodeClass::setupMarkerStrings(string markerString, string filename) {
  string functionString;
  for (const auto &fragment : Grammar::extractStringFromFile(
           markerString + "_START", markerString + "_END", filename)) {
    functionString += fragment.str + '\n';
  }
  auto codeString = std::make_unique<GrammarString>(
      functionString, std::make_pair(filename, markerString));
  return codeString;
}

void AstNodeClass::setFunctionPrototype(const string &markerString,
                                        const string &filename) {
  auto codeString = setupMarkerStrings(markerString, filename);
  setFunctionPrototype(*codeString);
}

void AstNodeClass::setFunctionSource(const string &markerString,
                                     const string &filename) {
  auto codeString = setupMarkerStrings(markerString, filename);
  AstNodeClass::addElementToList(
      getMemberFunctionSourceList(AstNodeClass::LOCAL_LIST,
                                  AstNodeClass::INCLUDE_LIST),
      *codeString);
}

void AstNodeClass::setSubTreeFunctionPrototype(
    const GrammarString &inputMemberFunction) {
  AstNodeClass::addElementToList(
      getMemberFunctionPrototypeList(AstNodeClass::SUBTREE_LIST,
                                     AstNodeClass::INCLUDE_LIST),
      inputMemberFunction);
  ROSE_ASSERT(getMemberFunctionPrototypeList(AstNodeClass::SUBTREE_LIST,
                                             AstNodeClass::INCLUDE_LIST)
                  .size() > 0);
}

void AstNodeClass::setSubTreeFunctionPrototype(const string &markerString,
                                               const string &filename) {
  auto codeString = setupMarkerStrings(markerString, filename);
  setSubTreeFunctionPrototype(*codeString);
}

void AstNodeClass::setSubTreeFunctionSource(const string &markerString,
                                            const string &filename) {
  auto codeString = setupMarkerStrings(markerString, filename);
  AstNodeClass::addElementToList(
      getMemberFunctionSourceList(AstNodeClass::SUBTREE_LIST,
                                  AstNodeClass::INCLUDE_LIST),
      *codeString);
}

void AstNodeClass::setPredeclarationString(const string &markerString,
                                           const string &filename) {
  auto codeString = setupMarkerStrings(markerString, filename);

  setPredeclarationString(codeString->getRawString());
}

void AstNodeClass::setPostdeclarationString(const string &markerString,
                                            const string &filename) {
  auto codeString = setupMarkerStrings(markerString, filename);
  setPostdeclarationString(codeString->getRawString());
}

void AstNodeClass::setPredeclarationString(const string &declarationString) {
  predeclarationString = declarationString;
}

void AstNodeClass::setPostdeclarationString(const string &declarationString) {
  postdeclarationString = declarationString;
}

const string &AstNodeClass::getPredeclarationString() const {
  return predeclarationString;
}

const string &AstNodeClass::getPostdeclarationString() const {
  return postdeclarationString;
}

void AstNodeClass::setDataPrototype(const GrammarString &inputMemberData) {
  AstNodeClass::addElementToList(
      getMemberDataPrototypeList(AstNodeClass::LOCAL_LIST,
                                 AstNodeClass::INCLUDE_LIST),
      inputMemberData);

  string accessFunctionString = buildDataAccessFunctions(inputMemberData);
  auto sourceCodeString = std::make_unique<GrammarString>(accessFunctionString);

  vector<GrammarString *> &l = getMemberFunctionSourceList(
      AstNodeClass::LOCAL_LIST, AstNodeClass::INCLUDE_LIST);
  // Data access "functions" should be placed into the LOCAL_LIST since
  // they are accessable though the base classes by definition (of C++)
  AstNodeClass::addElementToList(l, *sourceCodeString);
}

string
AstNodeClass::buildDataAccessFunctions(const GrammarString &inputMemberData) {
  string returnString = "";
  string filename = "";
  BuildAccessEnum config = inputMemberData.generateDataAccessFunctions();
  if (config == NO_ACCESS_FUNCTIONS) {
    return "\n";
  }
  switch (config) {
  case BUILD_ACCESS_FUNCTIONS:
    filename = "../Grammar/dataMemberAccessFunctions.macro";
    break;
  case BUILD_LITERAL_VALUE_ACCESS_FUNCTIONS:
    filename = "../Grammar/literalValueAccessFunctions.macro";
    break;
  case BUILD_FLAG_ACCESS_FUNCTIONS:
    filename = "../Grammar/flagMemberAccessFunctions.macro";
    break;
  case BUILD_LIST_ACCESS_FUNCTIONS:
    filename = "../Grammar/listMemberAccessFunctions.macro";
    break;
  default:
    ROSE_ABORT();
  }

  string markerString = "DATA_ACCESS_FUNCTION";

  auto codeString = setupMarkerStrings(markerString, filename);

  // We can't edit anything that would be tree position dependent be
  // we need to get the correct variable names into place so do just
  // that editing here!
  string functionString = codeString->getRawString();
  string typeName = inputMemberData.getTypeNameString();
  string variableName = inputMemberData.getVariableNameString();

  // DQ (12/20/2005): strip the "static " substring from the typeName
  // so that we generate non-static member access functions and non-static
  // parameter variable types (which are not legal C++).
  if (typeName.find("static ") != string::npos) {
    typeName = typeName.substr(7 /* strlen("static ") */);
  }

  functionString =
      StringUtility::copyEdit(functionString, "$DATA_TYPE", typeName);
  functionString =
      StringUtility::copyEdit(functionString, "$DATA", variableName);

  return functionString;
}

void AstNodeClass::setDataPrototype(
    const string &inputTypeNameString, const string &inputVariableNameString,
    const string &inputDefaultInitializer,
    const ConstructParamEnum &constructorParameter,
    const BuildAccessEnum &buildAccessDataFunctions,
    const TraversalEnum &toBeTraversedDuringTreeTraversal,
    const DeleteEnum &delete_flag, const CopyConfigEnum &toBeCopied) {
  GrammarString temp(inputTypeNameString, inputVariableNameString,
                     inputDefaultInitializer, constructorParameter,
                     buildAccessDataFunctions, toBeTraversedDuringTreeTraversal,
                     delete_flag, toBeCopied);
  setDataPrototype(temp);
}

void AstNodeClass::setDataPrototype(
    const string &inputTypeNameString, const string &inputVariableNameString,
    const string &inputDefaultInitializer,
    const ConstructParamEnum &constructorParameter,
    const BuildAccessEnum &buildAccessDataFunctions,
    const TraversalEnum &toBeTraversedDuringTreeTraversal,
    const DeleteEnum &delete_flag, const CopyConfigEnum &toBeCopied,
    std::optional<TraversalCardinalityEnum> traversalCardinality,
    const TraversalAccessorEnum &traversalAccessor,
    CopyConstructionOrderEnum copyConstructionOrder) {
  GrammarString temp(inputTypeNameString, inputVariableNameString,
                     inputDefaultInitializer, constructorParameter,
                     buildAccessDataFunctions, toBeTraversedDuringTreeTraversal,
                     delete_flag, toBeCopied, traversalCardinality,
                     traversalAccessor, copyConstructionOrder);
  setDataPrototype(temp);
}

void AstNodeClass::excludeFunctionPrototype(const string &markerString,
                                            const string &filename) {
  auto codeString = setupMarkerStrings(markerString, filename);
  AstNodeClass::addElementToList(
      getMemberFunctionPrototypeList(AstNodeClass::LOCAL_LIST,
                                     AstNodeClass::EXCLUDE_LIST),
      *codeString);
}

void AstNodeClass::excludeSubTreeFunctionPrototype(const string &markerString,
                                                   const string &filename) {
  auto codeString = setupMarkerStrings(markerString, filename);
  AstNodeClass::addElementToList(
      getMemberFunctionPrototypeList(AstNodeClass::SUBTREE_LIST,
                                     AstNodeClass::EXCLUDE_LIST),
      *codeString);
}

void AstNodeClass::excludeFunctionSource(const string &markerString,
                                         const string &filename) {
  auto codeString = setupMarkerStrings(markerString, filename);
  AstNodeClass::addElementToList(
      getMemberFunctionSourceList(AstNodeClass::LOCAL_LIST,
                                  AstNodeClass::EXCLUDE_LIST),
      *codeString);
}

void AstNodeClass::addElementToList(vector<GrammarString *> &targetList,
                                    const GrammarString &element) {
  int i = 0;
  bool duplicateEntryFound = false;
  vector<GrammarString *>::iterator stringListIterator;
  for (stringListIterator = targetList.begin();
       stringListIterator != targetList.end(); stringListIterator++) {
    if (*stringListIterator == &element) {
      printf("Duplicate entry found in list! (targetList[%d] == element) \n",
             i);
      duplicateEntryFound = true;

      // If any of the GrammarString objects is marked to skip construction
      // of access functions (for all the data at a node!) then mark it in the
      // list's copy
      if (element.generateDataAccessFunctions() == NO_ACCESS_FUNCTIONS)
        (*stringListIterator)
            ->setAutomaticGenerationOfDataAccessFunctions(NO_ACCESS_FUNCTIONS);
    }

    i++;
  }

  // Since the grammar tree is not build yet we cannot search the parents
  // so this abstraction for adding elements to the lists is not used to
  // exclude any elements (this is post processed).
  if (duplicateEntryFound == false) {
    targetList.push_back(new GrammarString(element));
  }
}

vector<GrammarString *> &
AstNodeClass::getMemberFunctionPrototypeList(int i, int j) const {
  return (vector<GrammarString *> &)memberFunctionPrototypeList[i][j];
}

vector<GrammarString *> &AstNodeClass::getMemberDataPrototypeList(int i,
                                                                  int j) const {
  return (vector<GrammarString *> &)memberDataPrototypeList[i][j];
}

vector<GrammarString *> &
AstNodeClass::getMemberFunctionSourceList(int i, int j) const {
  return (vector<GrammarString *> &)memberFunctionSourceList[i][j];
}

vector<GrammarString *> &
AstNodeClass::getEditSubstituteTargetList(int i, int j) const {
  return (vector<GrammarString *> &)editSubstituteTargetList[i][j];
}

vector<GrammarString *> &
AstNodeClass::getEditSubstituteSourceList(int i, int j) const {
  return (vector<GrammarString *> &)editSubstituteSourceList[i][j];
}

// This marco adds the oldString and newString to lists that are later used to
// drive the substitution mechanism.
#define EDIT_SUBSTITUTE_MACRO(X, Y)                                            \
  GrammarString *targetString = new GrammarString(oldString);                  \
  GrammarString *sourceString = new GrammarString(newString);                  \
  getEditSubstituteTargetList(AstNodeClass::X, AstNodeClass::Y)                \
      .push_back(targetString);                                                \
  getEditSubstituteSourceList(AstNodeClass::X, AstNodeClass::Y)                \
      .push_back(sourceString);                                                \
  ROSE_ASSERT(                                                                 \
      getEditSubstituteTargetList(AstNodeClass::X, AstNodeClass::Y).size() ==  \
      getEditSubstituteSourceList(AstNodeClass::X, AstNodeClass::Y).size());

void AstNodeClass::editSubstitute(const string &oldString,
                                  const string &newString) {
  EDIT_SUBSTITUTE_MACRO(LOCAL_LIST, INCLUDE_LIST)
}

void AstNodeClass::editSubstituteSubTree(const string &oldString,
                                         const string &newString) {
  EDIT_SUBSTITUTE_MACRO(SUBTREE_LIST, INCLUDE_LIST)
}

void AstNodeClass::editSubstitute(const string &oldString,
                                  const string &markerString,
                                  const string &filename) {

  auto codeString = setupMarkerStrings(markerString, filename);
  string newString = codeString->getRawString();
  editSubstitute(oldString, newString);
}

void AstNodeClass::editSubstituteSubTree(const string &oldString,
                                         const string &markerString,
                                         const string &filename) {

  auto codeString = setupMarkerStrings(markerString, filename);
  string newString = codeString->getRawString();
  editSubstitute(oldString, newString);
}

void AstNodeClass::setAutomaticGenerationOfConstructor(bool X) {
  // All terminals can have there constructors and destructors
  // automatically generated.  This function controls the automatic
  // generation of the constructor (one with the declared data input
  // as paramteres).
  automaticGenerationOfConstructor = X;
}

void AstNodeClass::setAutomaticGenerationOfDestructor(bool X) {
  // See description of setAutomaticGenerationOfConstructor(bool) above
  automaticGenerationOfDestructor = X;
}

bool AstNodeClass::generateDestructor() const {
  // See description of setAutomaticGenerationOfConstructor(bool) above
  return automaticGenerationOfDestructor;
}

bool AstNodeClass::generateConstructor() const {
  // See description of setAutomaticGenerationOfConstructor(bool) above
  return automaticGenerationOfConstructor;
}

void AstNodeClass::setAutomaticGenerationOfCopyFunction(bool X) {
  // These functions set the construction of access functions for all member
  // data of a AstNodeClass! See description of
  // setAutomaticGenerationOfConstructor(bool) above
  automaticGenerationOfCopyFunction = X;
}

void AstNodeClass::consistencyCheck() const {
  ROSE_ASSERT(associatedGrammar != NULL);

  // local index variable
  int j = 0, k = 0;

  // check the list for valid objects

  for (k = 0; k < 2; k++)
    for (j = 0; j < 2; j++) {
      // int i = 0;
      vector<GrammarString *>::const_iterator it;
      for (it = memberDataPrototypeList[j][k].begin();
           it != memberDataPrototypeList[j][k].end(); it++) {
        assert(*it != NULL);
        (*it)->consistencyCheck();
      }

      for (it = memberFunctionPrototypeList[j][k].begin();
           it != memberFunctionPrototypeList[j][k].end(); it++) {
        assert(*it != NULL);
        (*it)->consistencyCheck();
      }

      for (it = memberFunctionSourceList[j][k].begin();
           it != memberFunctionSourceList[j][k].end(); it++) {
        assert(*it != NULL);
        (*it)->consistencyCheck();
      }
    }

  // check the subclass list for valid objects
  for (vector<AstNodeClass *>::const_iterator terminalIterator =
           subclasses.begin();
       terminalIterator != subclasses.end(); terminalIterator++) {
    ROSE_ASSERT((*terminalIterator) != NULL);
    (*terminalIterator)->consistencyCheck();
    ROSE_ASSERT((*terminalIterator)->getBaseClass() == this);
  }
}

std::string AstNodeClass::buildListIteratorString(const GrammarString &data) {
  const auto storage = data.getSchemaStorage();
  const auto element = data.getSchemaElement();
  ROSE_ASSERT(storage == SCALAR_SCHEMA_STORAGE ||
              storage == VALUE_CONTAINER_SCHEMA_STORAGE ||
              storage == POINTER_CONTAINER_SCHEMA_STORAGE);
  ROSE_ASSERT(element == VALUE_SCHEMA_ELEMENT ||
              element == POINTER_SCHEMA_ELEMENT);
  if (storage == SCALAR_SCHEMA_STORAGE || element != POINTER_SCHEMA_ELEMENT)
    return "";
  const bool pointer = storage == POINTER_CONTAINER_SCHEMA_STORAGE;
  const string &variableName = data.getVariableNameString();
  const string originalList = "p_" + variableName;
  const string iteratorName =
      (pointer ? variableName : "source_" + variableName) + "_iterator";
  const string accessOperator = pointer ? "->" : ".";
  string returnString;
  if (pointer) {
    returnString += "     if(" + originalList + "==NULL)\n";
    returnString +=
        "        returnVector.push_back(pair<SgNode*,std::string>( NULL,\"" +
        variableName + "\"));\n";
    returnString += "     else\n";
  }
  returnString += "     for ( auto " + iteratorName + " = " + originalList +
                  accessOperator + "begin() \n" + "; " + iteratorName +
                  " != " + originalList + accessOperator + "end(); ++" +
                  iteratorName + ") \n        { \n";
  returnString +=
      "          returnVector.push_back(pair<SgNode*,std::string>( *" +
      iteratorName + ",\"" + variableName + "\"));\n";
  returnString += "        } \n";
  return returnString;
}

// Include node pointers even when they are not traversal children.
string AstNodeClass::buildReturnDataMemberPointers() {
  vector<GrammarString *> copyList;
  vector<GrammarString *>::iterator stringListIterator;

  string s("std::vector<std::pair<SgNode*,std::string> > returnVector;\n");

  for (AstNodeClass *t = this; t != NULL; t = t->getBaseClass()) {
    copyList = t->getMemberDataPrototypeList(AstNodeClass::LOCAL_LIST,
                                             AstNodeClass::INCLUDE_LIST);
    for (stringListIterator = copyList.begin();
         stringListIterator != copyList.end(); stringListIterator++) {
      GrammarString *data = *stringListIterator;
      string varNameString = string(data->getVariableNameString());
      const string &elementType = data->getSchemaElementTypeName();

      if (varNameString != "freepointer") {

        s += buildListIteratorString(*data);

        if (data->getSchemaStorage() == SCALAR_SCHEMA_STORAGE &&
            data->getSchemaElement() == POINTER_SCHEMA_ELEMENT &&
            associatedGrammar->astNodeToVariantMap.count(elementType) != 0) {
          s +=
              "          returnVector.push_back(pair<SgNode*,std::string>( p_" +
              varNameString + ",\"" + varNameString + "\"));\n";

        } else {
          if (elementType == "rose_hash_multimap") {
            ROSE_ASSERT(data->getSchemaStorage() == SCALAR_SCHEMA_STORAGE &&
                        data->getSchemaElement() == POINTER_SCHEMA_ELEMENT);
            s += "     if(p_" + varNameString + "==NULL)\n";
            s += "        returnVector.push_back(pair<SgNode*,std::string>( "
                 "NULL,\"" +
                 varNameString + "\"));\n";
            s += "     else\n";

            s += "     for ( rose_hash_multimap::const_iterator it_" +
                 varNameString + "= p_" + varNameString + "->begin();" + "it_" +
                 varNameString + "!= p_" + varNameString + "->end(); ++" +
                 "it_" + varNameString + ") \n        { \n";
            s += "          returnVector.push_back(pair<SgNode*,std::string>( "
                 "it_" +
                 varNameString + "->second,std::string(it_" + varNameString +
                 "->first.str()) ) );\n";

            s += "        } \n";
          }
        }
      }
    }
  }

  s += "     return returnVector;\n";

  return s;
}
string AstNodeClass::buildListIteratorStringForReferenceToPointers(
    const GrammarString &data, bool traverse) {
  const auto storage = data.getSchemaStorage();
  const auto element = data.getSchemaElement();
  ROSE_ASSERT(storage == SCALAR_SCHEMA_STORAGE ||
              storage == VALUE_CONTAINER_SCHEMA_STORAGE ||
              storage == POINTER_CONTAINER_SCHEMA_STORAGE);
  ROSE_ASSERT(element == VALUE_SCHEMA_ELEMENT ||
              element == POINTER_SCHEMA_ELEMENT);
  if (storage == SCALAR_SCHEMA_STORAGE || element != POINTER_SCHEMA_ELEMENT)
    return "";
  const bool pointer = storage == POINTER_CONTAINER_SCHEMA_STORAGE;
  const string &variableName = data.getVariableNameString();
  const string originalList = "p_" + variableName;
  const string iteratorName =
      (pointer ? variableName : "source_" + variableName) + "_iterator";
  const string accessOperator = pointer ? "->" : ".";
  string returnString;
  if (pointer)
    returnString += "     if (" + originalList + " != nullptr)\n";
  returnString += "     for ( auto " + iteratorName + " = " + originalList +
                  accessOperator + "begin() \n" + "; " + iteratorName +
                  " != " + originalList + accessOperator + "end(); ++" +
                  iteratorName + ") \n        { \n";
  returnString += "          handler->apply(*" + iteratorName + ",SgName(\"" +
                  variableName + "\"), " + BOOL2STR(traverse) + ");\n";
  returnString += "        } \n";
  return returnString;
}

string AstNodeClass::buildProcessDataMemberReferenceToPointers() {
  vector<GrammarString *> copyList;
  vector<GrammarString *>::iterator stringListIterator;

  string s;
  for (AstNodeClass *t = this; t != NULL; t = t->getBaseClass()) {
    copyList = t->getMemberDataPrototypeList(AstNodeClass::LOCAL_LIST,
                                             AstNodeClass::INCLUDE_LIST);
    for (stringListIterator = copyList.begin();
         stringListIterator != copyList.end(); stringListIterator++) {
      GrammarString *data = *stringListIterator;
      string varNameString = string(data->getVariableNameString());
      const string &elementType = data->getSchemaElementTypeName();

      if (varNameString != "freepointer") {
        bool traverse = data->getToBeTraversed() == DEF_TRAVERSAL;

        s += buildListIteratorStringForReferenceToPointers(*data, traverse);

        if (data->getSchemaStorage() == SCALAR_SCHEMA_STORAGE &&
            data->getSchemaElement() == POINTER_SCHEMA_ELEMENT &&
            associatedGrammar->astNodeToVariantMap.count(elementType) != 0) {
          s += "     handler->apply(p_" + varNameString + ",SgName(\"" +
               varNameString + "\"), " + BOOL2STR(traverse) + ");\n";
        } else {
          if (elementType == "rose_hash_multimap") {
            ROSE_ASSERT(data->getSchemaStorage() == SCALAR_SCHEMA_STORAGE &&
                        data->getSchemaElement() == POINTER_SCHEMA_ELEMENT);
            s += "     if (p_" + varNameString + " == NULL)\n";
            s += "        {\n";
            s += "        }\n";
            s += "     else\n";
            s += "        {\n";
            s += "          for ( rose_hash_multimap::iterator it_" +
                 varNameString + "= p_" + varNameString + "->begin(); it_" +
                 varNameString + "!= p_" + varNameString + "->end(); ++" +
                 "it_" + varNameString + ")\n";
            s += "             {\n";
            s += "               handler->apply(it_" + varNameString +
                 "->second, it_" + varNameString + "->first, " +
                 BOOL2STR(traverse) + ");\n";
            s += "             }\n";
            s += "        }\n";
          }
        }
      }
    }
  }

  return s;
}
