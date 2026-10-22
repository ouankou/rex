#include "AstNodeClass.h"

#include "Rose/StringUtility/FileUtility.h"

#include "Rose/StringUtility.h"

#include "grammarString.h"

#include <cstdio>
#include <string>

using namespace std;
using namespace Rose;

const string &GrammarString::getTypeNameString() const {
  return typeNameString;
}

const string &GrammarString::getVariableNameString() const {
  return variableNameString;
}

const string &GrammarString::getDefaultInitializerString() const {
  return defaultInitializerString;
}

ConstructParamEnum GrammarString::getIsInConstructorParameterList() const {
  return p_isInConstructorParameterList;
}

TraversalEnum GrammarString::getToBeTraversed() const { return toBeTraversed; }

TraversalCardinalityEnum GrammarString::getTraversalCardinality() const {
  if (!p_traversalCardinality.has_value()) {
    fprintf(stderr,
            "REX_ROSETTA_INVARIANT[traversal-cardinality]: member %s has no "
            "scalar traversal cardinality\n",
            variableNameString.c_str());
    ROSE_ABORT();
    __builtin_unreachable();
  }
  return *p_traversalCardinality;
}

TraversalAccessorEnum GrammarString::getTraversalAccessor() const {
  return p_traversalAccessor;
}

TraversalStorageEnum GrammarString::getTraversalStorage() const {
  return p_traversalStorage;
}

string GrammarString::getRawString() const { return functionNameString; }

string listIteratorInitialization(string typeName, string iteratorName,
                                  string listName, string accessOperator) {
  string returnString;

  if (accessOperator == ".") {
    string tempListName = iteratorName + "_container";
    returnString = "     const " + typeName + "& " + tempListName + " = " +
                   listName + "; \n";
    returnString += "     " + typeName + "::const_iterator " + iteratorName +
                    " = " + tempListName + ".begin(); \n";
  } else {
    returnString = "     " + typeName + "::const_iterator " + iteratorName +
                   " = " + listName + accessOperator + "begin(); \n";
  }

  return returnString;
}

string forLoopOpening(string iteratorName, string listName,
                      string accessOperator) {
  string endTarget =
      accessOperator == "." ? iteratorName + "_container" : listName;

  string returnString = "     for ( /* empty by design */; " + iteratorName +
                        " != " + endTarget + accessOperator + "end(); ++" +
                        iteratorName + ") \n        { \n";
  return returnString;
}

string forLoopBody(string typeName, string variableName, string iteratorName) {
  string returnString = "          " + typeName + " " + variableName + " = *" +
                        iteratorName + "; \n";
  return returnString;
}

string conditionalToSetParent(string variableName, bool ownsType = false) {
  // Shared types retain their parent; owned type syntax receives its copied
  // owner.
  string returnString =
      "          if ( (" + variableName + " != NULL) && (" + variableName +
      "->get_parent() == NULL)" +
      (ownsType ? " ) \n"
                : " && (isSgType(" + variableName + ") == NULL) ) \n") +
      "             { \n" + "               " + variableName +
      "->set_parent(result); \n" + "             } \n";

  return returnString;
}

string conditionalToCopyVariable(string typeName, string variableNameSource,
                                 string variableNameCopy, string iteratorName,
                                 bool exclusiveOwner) {
  string returnString =
      "          if (" + variableNameSource + " != NULL) \n" +
      "             { \n" + "               " + variableNameCopy +
      " = static_cast<" + typeName + ">(help." +
      (exclusiveOwner ? "copyOrLookupOwnedAst(" : "copyOrLookupAst(") +
      iteratorName + ")); \n" + "             } \n" + "            else \n" +
      "             { \n" + "               " + variableNameCopy +
      " = NULL; \n" + "             } \n";
  return returnString;
}

string forLoopClosing() {
  string returnString = "        } \n";
  return returnString;
}

string variableInitialization(string copyOfVariableName,
                              string sourceVariableName) {
  string returnString =
      "     " + copyOfVariableName + " = " + sourceVariableName + "; \n";
  return returnString;
}

string variableDeclaration(string typeName, string variableName) {
  string returnString = "     " + typeName + " " + variableName + " = NULL; \n";
  return returnString;
}

string conditionalToBuildNewVariable(string typeName, string variableNameSource,
                                     string newVariableName) {
  const string rhs = "          " + newVariableName + " = new " + typeName +
                     "( *" + variableNameSource + "); \n";
  string returnString = "     if ( " + variableNameSource + " != NULL ) \n" +
                        "        { \n" + rhs + "        } \n" +
                        "       else \n" + "        { \n" + "          " +
                        newVariableName + " = NULL; \n" + "        } \n";

  return returnString;
}

string GrammarString::buildCopyMemberFunctionSetParentSource() {
  string returnString;

  string variableName = getVariableNameString();
  string typeName = getTypeNameString();

  ROSE_ASSERT(typeName.empty() == false);
  ROSE_ASSERT(variableName.empty() == false);

  if (typeName == "SgFunctionParameterScope*" &&
      variableName == "functionParameterScope")
    return "";

  if (toBeDeleted == DELETE_IF_OWNED &&
      p_schemaStorage == VALUE_CONTAINER_SCHEMA_STORAGE) {
    const string source = "p_" + variableName;
    const string target = "result->" + source;
    return "     ROSE_ASSERT(" + source + ".size() == " + target +
           ".size());\n" + "     for (size_t index = 0; index < " + source +
           ".size(); ++index) {\n" + "       if (" + source +
           "[index] != nullptr && " + source +
           "[index]->get_parent() == this) {\n" + "         ROSE_ASSERT(" +
           target + "[index] != nullptr && " + target + "[index] != " + source +
           "[index] && " + target + "[index]->get_parent() == result);\n" +
           "       } else {\n" + "         help.bindCopiedReference(" + target +
           "[index], " + source + "[index]);\n       }\n     }\n";
  }

  if (toBeTraversed == NO_TRAVERSAL && toBeCopied == CLONE_TREE &&
      toBeDeleted == NO_DELETE && p_schemaStorage == SCALAR_SCHEMA_STORAGE &&
      p_schemaElement == POINTER_SCHEMA_ELEMENT) {
    const string source = "p_" + variableName;
    const string copy = variableName + "_copy";
    return "     if (" + source + " != nullptr && " + source +
           "->get_parent() == this) {\n" + "       ROSE_ASSERT(" + copy +
           " != nullptr && " + copy + " != " + source + ");\n" +
           "       ROSE_ASSERT(" + copy + "->get_parent() == nullptr || " +
           copy + "->get_parent() == result);\n" + "       " + copy +
           "->set_parent(result);\n     }\n";
  }

  // Traversed and explicitly destroyed subtrees have structural parents.
  const bool ownsCopiedAstSubtree =
      toBeTraversed == DEF_TRAVERSAL ||
      (toBeDeleted == DEF_DELETE && toBeCopied == CLONE_TREE);
  if (ownsCopiedAstSubtree) {
    const bool typeIsPointerToList =
        p_schemaStorage == POINTER_CONTAINER_SCHEMA_STORAGE;
    const bool typeIsList = p_schemaStorage != SCALAR_SCHEMA_STORAGE;
    const bool typeIsSgNode = p_schemaStorage == SCALAR_SCHEMA_STORAGE &&
                              p_schemaElement == POINTER_SCHEMA_ELEMENT;

    ROSE_ASSERT(typeIsList == true || typeIsSgNode == true);
    ROSE_ASSERT(typeIsList == false || typeIsSgNode == false);

    if (typeIsList == true) {
      string listElementName = "list_element";

      string iteratorBaseType;
      string needPointer;
      string listElementType;
      string copyOfList;
      string iteratorName;

      string accessOperator = "error string for access operator";

      if (typeIsPointerToList == true) {
        if (p_schemaElement == POINTER_SCHEMA_ELEMENT) {
          needPointer = "*";
          accessOperator = "->";
        } else {
          ROSE_ASSERT(p_schemaElement == VALUE_SCHEMA_ELEMENT);
          accessOperator = ".";
        }

        iteratorBaseType = typeName.substr(0, typeName.size() - 3);

        copyOfList = variableName + "_copy";
        iteratorName = copyOfList + "_iterator";
      } else {
        ROSE_ASSERT(p_schemaStorage == VALUE_CONTAINER_SCHEMA_STORAGE &&
                    p_schemaElement == POINTER_SCHEMA_ELEMENT);
        iteratorBaseType = typeName;
        needPointer = "*";
        accessOperator = ".";

        // The copied children do not acquire their new structural owner until
        // the loop emitted below.  Calling a public getter here is therefore
        // invalid for members whose getter checks ownership.  This code is
        // emitted inside the node's copy member function, so inspect the
        // freshly assigned data member directly while repairing that transient
        // construction state.
        copyOfList = string("result->p_") + variableName;
        iteratorName = variableName + "_iterator";
      }

      listElementType = p_schemaElementTypeName + needPointer;

      returnString += listIteratorInitialization(iteratorBaseType, iteratorName,
                                                 copyOfList, accessOperator);

      returnString += forLoopOpening(iteratorName, copyOfList, accessOperator);

      returnString +=
          forLoopBody(listElementType, listElementName, iteratorName);

      returnString +=
          conditionalToSetParent(listElementName, toBeDeleted == DEF_DELETE &&
                                                      toBeCopied == CLONE_TREE);

      returnString += forLoopClosing();
    } else {
      ROSE_ASSERT(typeIsSgNode == true);

      string copyOfVariable = variableName + "_copy";
      returnString +=
          conditionalToSetParent(copyOfVariable, toBeDeleted == DEF_DELETE &&
                                                     toBeCopied == CLONE_TREE);
    }
  }

  if (getTraversalAccessor() == COMPUTED_BASE_TYPE_DECLARATION_ACCESS) {
    returnString +=
        "     if (compute_baseTypeDefiningDeclaration() == nullptr)\n";
    returnString += "       help.bindCopiedReference(result->p_" +
                    variableName + ", p_" + variableName + ");\n";
  }

  return returnString;
}

string
GrammarString::buildCopyMemberFunctionSource(bool buildConstructorArgument) {
  string returnString;

  string variableName = getVariableNameString();
  string typeName = getTypeNameString();

  ROSE_ASSERT(typeName.empty() == false);
  ROSE_ASSERT(variableName.empty() == false);

  if (typeName == "SgFunctionParameterScope*" &&
      variableName == "functionParameterScope") {
    ROSE_ASSERT(!buildConstructorArgument &&
                p_isInConstructorParameterList == NO_CONSTRUCTOR_PARAMETER &&
                toBeTraversed == DEF_TRAVERSAL && toBeCopied == COPY_DATA);
    return R"(     if (p_functionParameterScope != nullptr &&
         p_functionParameterScope->get_parent() == this) {
       auto *scope = isSgFunctionParameterScope(
           help.copyOrLookupOwnedAst(p_functionParameterScope));
       ROSE_ASSERT(scope != nullptr && scope != p_functionParameterScope);
       SageInterface::completeDetachedFunctionParameterScopeConstruction(
           isSgFunctionDeclaration(result), scope);
     } else {
       help.bindCopiedReference(result->p_functionParameterScope,
                                p_functionParameterScope);
     }
)";
  }

  if (toBeCopied == COPY_REFERENCE ||
      (toBeDeleted == DELETE_IF_OWNED &&
       p_schemaStorage == SCALAR_SCHEMA_STORAGE)) {
    if (p_schemaStorage == VALUE_CONTAINER_SCHEMA_STORAGE) {
      ROSE_ASSERT(toBeCopied == COPY_REFERENCE &&
                  p_schemaElement == POINTER_SCHEMA_ELEMENT &&
                  !buildConstructorArgument);
      const string source = "p_" + variableName;
      const string target = "result->" + source;
      return "     " + target + ".resize(" + source + ".size());\n" +
             "     for (size_t index = 0; index < " + source +
             ".size(); ++index)\n" + "       help.bindCopiedReference(" +
             target + "[index], " + source + "[index]);\n";
    }
    const bool conditional_owner = toBeDeleted == DELETE_IF_OWNED;
    ROSE_ASSERT(conditional_owner ? toBeCopied == CLONE_TREE
                                  : toBeTraversed == NO_TRAVERSAL &&
                                        toBeDeleted == NO_DELETE);
    const string source = "p_" + variableName;
    const string copy = variableName + "_copy";
    if (buildConstructorArgument) {
      returnString = "     " + typeName + " " + copy + " = " + source + ";\n";
      returnString += "     if (" + source + " != nullptr) {\n";
      if (conditional_owner) {
        returnString += "       if (" + source + "->get_parent() == this) {\n";
        returnString += "         " + copy + " = static_cast<" + typeName +
                        ">(help.copyOrLookupOwnedAst(" + source + "));\n";
        returnString += "       } else {\n";
      }
      const string indent = conditional_owner ? "         " : "       ";
      returnString += indent +
                      "const auto reference = help.get_copiedNodeMap().find(" +
                      source + ");\n";
      returnString +=
          indent + "if (reference != help.get_copiedNodeMap().end()) {\n";
      returnString += indent + "  " + copy + " = dynamic_cast<" + typeName +
                      ">(reference->second);\n";
      returnString += indent + "  ASSERT_not_null(" + copy + ");\n";
      returnString += indent + "}\n";
      if (conditional_owner)
        returnString += "       }\n";
      returnString += "     }\n";
      return returnString;
    }
    if (conditional_owner) {
      returnString = "     if (" + source + " != nullptr && " + source +
                     "->get_parent() == this) {\n";
      if (p_isInConstructorParameterList == NO_CONSTRUCTOR_PARAMETER)
        returnString += "       result->" + source + " = dynamic_cast<" +
                        typeName + ">(help.copyOrLookupOwnedAst(" + source +
                        "));\n";
      return returnString + "       ASSERT_not_null(result->" + source +
             ");\n" + "       ROSE_ASSERT(result->" + source + " != " + source +
             " &&\n" + "                   (result->" + source +
             "->get_parent() == nullptr ||\n" + "                    result->" +
             source + "->get_parent() == result));\n" + "       result->" +
             source + "->set_parent(result);\n" + "     } else {\n" +
             "       help.bindCopiedReference(result->" + source + ", " +
             source + ");\n" + "     }\n";
    }
    return returnString + "     help.bindCopiedReference(result->" + source +
           ", " + source + ");\n";
  }

  if (typeName == "SgInitializedName*" &&
      variableName == "source_declaration") {
    ROSE_ASSERT(buildConstructorArgument && toBeCopied == COPY_DATA &&
                toBeTraversed == NO_TRAVERSAL);
    return "     ASSERT_not_null(p_source_declaration);\n"
           "     SgInitializedName *source_declaration_copy = "
           "p_source_declaration;\n"
           "     const auto parameter_entry = "
           "help.get_copiedNodeMap().find(p_source_declaration);\n"
           "     if (parameter_entry != help.get_copiedNodeMap().end()) {\n"
           "       source_declaration_copy = "
           "isSgInitializedName(parameter_entry->second);\n"
           "       ASSERT_not_null(source_declaration_copy);\n"
           "     } else {\n"
           "       ROSE_ASSERT(!help.isCopyingOriginal(p_source_declaration) "
           "&&\n"
           "                   !help.isCopyingOriginal("
           "p_source_declaration->get_parent()) &&\n"
           "                   !help.isCopyingOriginal("
           "p_source_declaration->get_declptr()));\n"
           "     }\n";
  }

  if (typeName == "SgCxxArrayInitBinding*" && toBeCopied == COPY_DATA) {
    ROSE_ASSERT(buildConstructorArgument && toBeTraversed == NO_TRAVERSAL);
    return "     ROSE_ASSERT(p_binding != nullptr &&\n"
           "                 SgNode::isLiveNode(p_binding));\n"
           "     const auto binding_entry = "
           "help.get_copiedNodeMap().find(p_binding);\n"
           "     SgCxxArrayInitBinding *binding_copy = p_binding;\n"
           "     if (binding_entry != help.get_copiedNodeMap().end()) {\n"
           "       binding_copy = "
           "isSgCxxArrayInitBinding(binding_entry->second);\n"
           "       ROSE_ASSERT(binding_copy != nullptr);\n"
           "       ROSE_ASSERT(\n"
           "           !help.isCopyingOriginal(p_binding->get_parent()) ||\n"
           "           binding_copy != p_binding);\n"
           "     } else {\n"
           "       const auto *binding_owner = "
           "isSgCxxArrayInitLoop(p_binding->get_parent());\n"
           "       ROSE_ASSERT(binding_owner != nullptr &&\n"
           "                   binding_owner->get_binding() == p_binding &&\n"
           "                   !help.isCopyingOriginal(binding_owner));\n"
           "     }\n";
  }

  if (typeName == "AttachedPreprocessingInfoType*" &&
      variableName == "attachedPreprocessingInfoPtr") {
    if (buildConstructorArgument == false) {
      returnString +=
          "     result->cloneAttachedPreprocessingInfoFrom(this, &help); \n";
    }

    return returnString;
  }

  // Pointer members in the grammar are Sage IR nodes.
  const bool typeIsSgNode = p_schemaStorage == SCALAR_SCHEMA_STORAGE &&
                            p_schemaElement == POINTER_SCHEMA_ELEMENT;

  if (toBeTraversed == DEF_TRAVERSAL || toBeCopied == CLONE_TREE) {
    const bool typeIsPointerToList =
        p_schemaStorage == POINTER_CONTAINER_SCHEMA_STORAGE;
    const bool typeIsList = p_schemaStorage != SCALAR_SCHEMA_STORAGE;

    ROSE_ASSERT(typeIsList == true || typeIsSgNode == true);
    ROSE_ASSERT(typeIsList == false || typeIsSgNode == false);

    string listElementType = "default-error-type";

    returnString += "     " + typeName + " " + variableName + "_copy; \n";

    if (typeIsList == true) {
      string listElementName = "source_list_element";
      string copyOfListElementName = "copy_list_element";

      string iteratorBaseType;
      string needPointer;
      string originalList;
      string iteratorName;

      string accessOperator = "error string for access operator";

      if (typeIsPointerToList == true) {
        if (p_schemaElement == POINTER_SCHEMA_ELEMENT) {
          needPointer = "*";
          accessOperator = "->";
        } else {
          ROSE_ASSERT(p_schemaElement == VALUE_SCHEMA_ELEMENT);
          accessOperator = ".";
        }

        iteratorBaseType = typeName.substr(0, typeName.size() - 3);

        originalList = string("get_") + variableName + "()";
        iteratorName = variableName + "_iterator";

        returnString += "     " + variableName + "_copy" + " = new " +
                        iteratorBaseType +
                        "; // initialize the pointer to the list \n";
      } else {
        ROSE_ASSERT(p_schemaStorage == VALUE_CONTAINER_SCHEMA_STORAGE &&
                    p_schemaElement == POINTER_SCHEMA_ELEMENT);
        iteratorBaseType = typeName;
        needPointer = "*";
        accessOperator = ".";

        originalList = string("get_") + variableName + "()";
        iteratorName = string("source_") + variableName + "_iterator";
      }

      listElementType = p_schemaElementTypeName + needPointer;

      returnString += listIteratorInitialization(iteratorBaseType, iteratorName,
                                                 originalList, accessOperator);

      returnString +=
          forLoopOpening(iteratorName, originalList, accessOperator);

      returnString +=
          forLoopBody(listElementType, listElementName, iteratorName);

      returnString +=
          "     " + variableDeclaration(listElementType, copyOfListElementName);

      if (toBeDeleted == DELETE_IF_OWNED) {
        ROSE_ASSERT(buildConstructorArgument);
        returnString += "          " + copyOfListElementName + " = " +
                        listElementName + ";\n";
        returnString += "          if (" + listElementName + " != nullptr) {\n";
        returnString += "            if (" + listElementName +
                        "->get_parent() == this) {\n";
        returnString += "              " + copyOfListElementName +
                        " = static_cast<" + listElementType +
                        ">(help.copyOrLookupOwnedAst(" + listElementName +
                        "));\n";
        returnString += "            } else {\n";
        returnString +=
            "              const auto entry = help.get_copiedNodeMap().find(" +
            listElementName + ");\n";
        returnString +=
            "              if (entry != help.get_copiedNodeMap().end()) {\n";
        returnString += "                " + copyOfListElementName +
                        " = dynamic_cast<" + listElementType +
                        ">(entry->second);\n";
        returnString += "                ASSERT_not_null(" +
                        copyOfListElementName +
                        ");\n              }\n            }\n          }\n";
      } else {
        string dereferencedIteratorName = string("*") + iteratorName;
        returnString += conditionalToCopyVariable(
            listElementType, listElementName, copyOfListElementName,
            dereferencedIteratorName,
            toBeDeleted == DEF_DELETE && toBeCopied == CLONE_TREE);
      }

      returnString += "          " + variableName + "_copy" + accessOperator +
                      "push_back(" + copyOfListElementName + "); \n";

      returnString += forLoopClosing();
    } else {
      ROSE_ASSERT(typeIsSgNode == true);

      string variableType = typeName;
      string copyOfVariableName = variableName + "_copy";
      string sourceVariableName;
      switch (getTraversalAccessor()) {
      case DIRECT_TRAVERSAL_ACCESS:
        sourceVariableName = string("p_") + variableName;
        break;
      case COMPUTED_BASE_TYPE_DECLARATION_ACCESS:
        sourceVariableName = "compute_baseTypeDefiningDeclaration()";
        break;
      case COMPUTED_CLASS_DEFINITION_ACCESS:
        sourceVariableName = "compute_classDefinition()";
        break;
      default:
        fprintf(stderr,
                "REX_ROSETTA_INVARIANT[copy-traversal-accessor]: member=%s "
                "has unknown traversal accessor\n",
                variableName.c_str());
        ROSE_ABORT();
      }
      returnString += conditionalToCopyVariable(
          variableType, sourceVariableName, copyOfVariableName,
          sourceVariableName,
          toBeDeleted == DEF_DELETE && toBeCopied == CLONE_TREE);

      if (buildConstructorArgument == false) {
        if (p_copyConstructionOrder == COPY_AFTER_OWNER_CONSTRUCTION) {
          returnString +=
              "     ROSE_ASSERT(" + copyOfVariableName + " == nullptr ||\n" +
              "                 " + copyOfVariableName +
              "->get_parent() == nullptr ||\n" + "                 " +
              copyOfVariableName + "->get_parent() == result);\n";
        }
        const string destination = "result->p_" + variableName;
        returnString += "     if (" + destination + " != nullptr &&\n";
        returnString +=
            "         " + destination + " != " + copyOfVariableName + ") {\n";
        returnString += "       ROSE_ASSERT(help.get_originalNodesForCopy(" +
                        destination + ").empty());\n";
        returnString += "       delete " + destination + ";\n     }\n";
      }
    }
    if (buildConstructorArgument == false) {
      returnString += "     result->p_" + variableName + " = " + variableName +
                      "_copy;" + " \n";
    }
  } else {
    string sourceVariableName = variableName + "_copy";

    if (toBeCopied == COPY_DATA || toBeCopied == COPY_CONSTRUCTOR_REFERENCE) {
      returnString += "     " + typeName + " " + variableName + "_copy = p_" +
                      variableName + "; \n";
      const bool constructorReference =
          buildConstructorArgument &&
          (toBeCopied == COPY_CONSTRUCTOR_REFERENCE ||
           (toBeTraversed == NO_TRAVERSAL && toBeDeleted == NO_DELETE &&
            p_schemaStorage == SCALAR_SCHEMA_STORAGE &&
            p_schemaElement == POINTER_SCHEMA_ELEMENT));
      if (constructorReference) {
        const string copy = variableName + "_copy";
        const string entry = variableName + "_entry";
        const string &nodeType = p_schemaElementTypeName;
        const bool container =
            p_schemaStorage == VALUE_CONTAINER_SCHEMA_STORAGE;
        const string reference = container ? variableName + "_element" : copy;
        if (container)
          returnString +=
              "     for (auto *&" + reference + " : " + copy + ") {\n";
        returnString += "     if (" + reference + " != nullptr) {\n";
        returnString +=
            "       prepareConstructorReference(" + reference + ", help);\n";
        returnString += "       const auto " + entry +
                        " = help.get_copiedNodeMap().find(" + reference +
                        ");\n";
        returnString +=
            "       if (" + entry + " != help.get_copiedNodeMap().end()) {\n";
        returnString += "         " + reference + " = is" + nodeType + "(" +
                        entry + "->second);\n";
        returnString += "         ASSERT_not_null(" + reference + ");\n";
        returnString += "       } else {\n";
        returnString += "         ROSE_ASSERT(!help.isCopyingOriginal(" +
                        reference + "));\n";
        returnString += "       }\n     }\n";
        if (container)
          returnString += "     }\n";
      }
      string copyOfVariableName = "result->p_" + variableName;
      if (buildConstructorArgument == false) {
        returnString +=
            variableInitialization(copyOfVariableName, sourceVariableName);
      }
    } else {
      if (toBeCopied == CLONE_PTR) {
        returnString +=
            "     " + typeName + " " + variableName + "_copy = NULL; \n";
        string copyOfVariableName = "p_" + variableName;
        ROSE_ASSERT(typeIsSgNode == true);
        returnString += conditionalToBuildNewVariable(
            p_schemaElementTypeName, copyOfVariableName, sourceVariableName);
        if (buildConstructorArgument == false) {
          const string destination = "result->p_" + variableName;
          returnString += "     if (" + destination + " != nullptr &&\n";
          returnString +=
              "         " + destination + " != " + sourceVariableName + ")\n";
          returnString += "       delete " + destination + ";\n";

          returnString += "     result->p_" + variableName + " = " +
                          variableName + "_copy;" + " \n";
        }
      }
    }
  }

  return returnString;
}

string GrammarString::getDataPrototypeString() const {
  return typeNameString + " p_" + variableNameString + ";\n          ";
}

bool GrammarString::generate_override_keyword(string variableNameString) const {
  bool returnResult = false;
  if ((variableNameString == "startOfConstruct") ||
      (variableNameString == "endOfConstruct") ||
      (variableNameString == "end_numeric_label") ||
      (variableNameString == "scope") ||
      (variableNameString == "originalExpressionTree") ||
      (variableNameString == "type") || (variableNameString == "name") ||
      (variableNameString == "attributeMechanism")) {
    returnResult = true;
  }

  return returnResult;
}

string GrammarString::getDataAccessFunctionPrototypeString() const {
  string typeNameStringTmp = typeNameString;

  // DQ (12/20/2005): strip the "static " substring from the typeName
  // so that we generate non-static member access functions and non-static
  // parameter variable types (which are not legal C++).
  string::size_type positionOfSubstring = typeNameStringTmp.find("static ");
  if (positionOfSubstring != string::npos) {
    typeNameStringTmp.erase(positionOfSubstring, 7 /* strlen("static ") */);
  }

  string variableNameStringTmp = string(variableNameString);
  bool use_override_keyword = generate_override_keyword(variableNameStringTmp);

  string returnString;
  switch (automaticGenerationOfDataAccessFunctions) {
  case NO_ACCESS_FUNCTIONS:
    break;

  case BUILD_ACCESS_FUNCTIONS:
  case BUILD_FLAG_ACCESS_FUNCTIONS:
  case BUILD_LITERAL_VALUE_ACCESS_FUNCTIONS:
    if (use_override_keyword == true) {
      returnString =
          "     public: \n         " + typeNameStringTmp + " get_" +
          variableNameStringTmp +
          "() const $ROSE_OVERRIDE_GET /* "
          "(getDataAccessFunctionPrototypeString) */;\n         void set_" +
          variableNameStringTmp + "(" + typeNameStringTmp + " " +
          variableNameStringTmp +
          ") $ROSE_OVERRIDE_SET /* (getDataAccessFunctionPrototypeString) "
          "*/;\n";
    } else {
      returnString = "     public: \n         " + typeNameStringTmp + " get_" +
                     variableNameStringTmp + "() const;\n         void set_" +
                     variableNameStringTmp + "(" + typeNameStringTmp + " " +
                     variableNameStringTmp + ");\n";
    }
    break;

  case BUILD_LIST_ACCESS_FUNCTIONS:
    returnString = "     public: \n         const " + typeNameStringTmp + "& " +
                   " get_" + variableNameStringTmp + "() const;\n         " +
                   typeNameStringTmp + "& " + "get_" + variableNameStringTmp +
                   "(); \n";
    break;
  default:
    ROSE_ABORT();
  }

  return returnString;
}

string GrammarString::getConstructorPrototypeParameterString() {
  string startString = getConstructorSourceParameterString();
  string endString = getDefaultInitializerString();
  string returnString = startString + " " + endString;

  return returnString;
}

string GrammarString::getConstructorSourceParameterString() {
  string returnString = getTypeNameString() + " " + getVariableNameString();

  return returnString;
}

string GrammarString::getBaseClassConstructorSourceParameterString() {
  return getVariableNameString();
}

GrammarString::~GrammarString() {}

namespace {
string trimTypeName(string typeName) {
  const string whitespace = " \t\n\r";
  const size_t first = typeName.find_first_not_of(whitespace);
  if (first == string::npos)
    return "";
  const size_t last = typeName.find_last_not_of(whitespace);
  return typeName.substr(first, last - first + 1);
}

bool stripExactSuffix(string &value, const string &suffix) {
  if (value.size() < suffix.size() ||
      value.compare(value.size() - suffix.size(), suffix.size(), suffix) != 0)
    return false;
  value.erase(value.size() - suffix.size());
  return true;
}
} // namespace

void GrammarString::initializeSchemaMetadata() {
  p_schemaStorage = SCALAR_SCHEMA_STORAGE;
  p_schemaElement = VALUE_SCHEMA_ELEMENT;
  p_schemaElementTypeName = trimTypeName(typeNameString);

  string elementType = p_schemaElementTypeName;
  if (stripExactSuffix(elementType, "PtrListPtr") ||
      stripExactSuffix(elementType, "PtrVectorPtr")) {
    p_schemaStorage = POINTER_CONTAINER_SCHEMA_STORAGE;
    p_schemaElement = POINTER_SCHEMA_ELEMENT;
  } else {
    elementType = p_schemaElementTypeName;
    if (stripExactSuffix(elementType, "PtrList") ||
        stripExactSuffix(elementType, "PtrVector")) {
      p_schemaStorage = VALUE_CONTAINER_SCHEMA_STORAGE;
      p_schemaElement = POINTER_SCHEMA_ELEMENT;
    } else {
      elementType = p_schemaElementTypeName;
      if (stripExactSuffix(elementType, "ListPtr") ||
          stripExactSuffix(elementType, "VectorPtr")) {
        p_schemaStorage = POINTER_CONTAINER_SCHEMA_STORAGE;
      } else {
        elementType = p_schemaElementTypeName;
        if (p_schemaElementTypeName.size() < 9 ||
            p_schemaElementTypeName.compare(p_schemaElementTypeName.size() - 9,
                                            9, "BitVector") != 0) {
          if (stripExactSuffix(elementType, "List") ||
              stripExactSuffix(elementType, "Vector"))
            p_schemaStorage = VALUE_CONTAINER_SCHEMA_STORAGE;
        }
      }
    }
  }

  if (p_schemaStorage != SCALAR_SCHEMA_STORAGE) {
    p_schemaElementTypeName = trimTypeName(elementType);
    if (p_schemaElementTypeName.empty()) {
      fprintf(stderr,
              "REX_ROSETTA_INVARIANT[schema-container-element]: member %s "
              "has no container element type\n",
              variableNameString.c_str());
      ROSE_ABORT();
    }
  } else if (stripExactSuffix(elementType, "*")) {
    p_schemaElement = POINTER_SCHEMA_ELEMENT;
    p_schemaElementTypeName = trimTypeName(elementType);
  }
}

void GrammarString::initializeTraversalMetadata() {
  p_traversalStorage = NOT_A_TRAVERSAL_MEMBER;
  p_traversalElementTypeName.clear();

  if (toBeTraversed == NO_TRAVERSAL) {
    if (p_traversalCardinality.has_value() ||
        p_traversalAccessor != DIRECT_TRAVERSAL_ACCESS) {
      fprintf(stderr,
              "REX_ROSETTA_INVARIANT[traversal-metadata]: non-traversed "
              "member %s cannot declare traversal behavior\n",
              variableNameString.c_str());
      ROSE_ABORT();
    }
    return;
  }

  string typeName = trimTypeName(typeNameString);
  if (typeName.compare(0, 7, "static ") == 0) {
    fprintf(stderr,
            "REX_ROSETTA_INVARIANT[traversal-static-member]: static member "
            "%s must be excluded from traversal in the schema\n",
            variableNameString.c_str());
    ROSE_ABORT();
  }

  if (p_schemaStorage == POINTER_CONTAINER_SCHEMA_STORAGE) {
    fprintf(stderr,
            "REX_ROSETTA_INVARIANT[traversal-container-storage]: traversed "
            "container %s must use non-null value storage\n",
            variableNameString.c_str());
    ROSE_ABORT();
  }

  if (p_schemaStorage == VALUE_CONTAINER_SCHEMA_STORAGE &&
      p_schemaElement == POINTER_SCHEMA_ELEMENT) {
    p_traversalStorage = NODE_POINTER_CONTAINER_TRAVERSAL_MEMBER;
    p_traversalElementTypeName = p_schemaElementTypeName;
    if (p_traversalCardinality.has_value()) {
      fprintf(stderr,
              "REX_ROSETTA_INVARIANT[traversal-cardinality]: traversed "
              "container %s cannot declare scalar traversal cardinality\n",
              variableNameString.c_str());
      ROSE_ABORT();
    }
    if (p_traversalAccessor != DIRECT_TRAVERSAL_ACCESS) {
      fprintf(stderr,
              "REX_ROSETTA_INVARIANT[traversal-accessor]: traversed "
              "container %s cannot use a computed scalar accessor\n",
              variableNameString.c_str());
      ROSE_ABORT();
    }
  } else {
    if (p_schemaStorage == VALUE_CONTAINER_SCHEMA_STORAGE) {
      fprintf(stderr,
              "REX_ROSETTA_INVARIANT[traversal-container-element]: "
              "traversed container %s must contain AST pointers\n",
              variableNameString.c_str());
      ROSE_ABORT();
    }

    if (p_schemaElement != POINTER_SCHEMA_ELEMENT) {
      fprintf(stderr,
              "REX_ROSETTA_INVARIANT[traversal-scalar-storage]: traversed "
              "member %s must be an AST pointer or pointer container\n",
              variableNameString.c_str());
      ROSE_ABORT();
    }
    p_traversalStorage = NODE_POINTER_TRAVERSAL_MEMBER;
    p_traversalElementTypeName = p_schemaElementTypeName;
    if (!p_traversalCardinality.has_value()) {
      fprintf(stderr,
              "REX_ROSETTA_INVARIANT[traversal-cardinality]: traversed "
              "scalar member %s must explicitly declare REQUIRED or "
              "OPTIONAL cardinality\n",
              variableNameString.c_str());
      ROSE_ABORT();
    }
  }

  if (p_traversalElementTypeName.empty() ||
      p_traversalElementTypeName.back() == '*') {
    fprintf(stderr,
            "REX_ROSETTA_INVARIANT[traversal-element-type]: traversal member "
            "%s has no exact AST element type\n",
            variableNameString.c_str());
    ROSE_ABORT();
  }
  if (p_traversalAccessor != DIRECT_TRAVERSAL_ACCESS &&
      p_traversalStorage != NODE_POINTER_TRAVERSAL_MEMBER) {
    fprintf(stderr,
            "REX_ROSETTA_INVARIANT[traversal-accessor]: computed traversal "
            "access requires a scalar AST pointer member\n");
    ROSE_ABORT();
  }
}

GrammarString::GrammarString()
    : functionNameString(""), typeNameString(""), variableNameString(""),
      defaultInitializerString(""),
      p_isInConstructorParameterList(CONSTRUCTOR_PARAMETER),
      p_traversalCardinality(std::nullopt),
      p_traversalAccessor(DIRECT_TRAVERSAL_ACCESS),
      p_traversalStorage(NOT_A_TRAVERSAL_MEMBER),
      p_schemaStorage(SCALAR_SCHEMA_STORAGE),
      p_schemaElement(VALUE_SCHEMA_ELEMENT), toBeCopied(COPY_DATA),
      toBeTraversed(DEF_TRAVERSAL),
      automaticGenerationOfDataAccessFunctions(BUILD_ACCESS_FUNCTIONS),
      toBeDeleted(NO_DELETE) {}

GrammarString::GrammarString(
    const string &inputTypeNameString, const string &inputVariableNameString,
    const string &inputDefaultInitializerString,
    const ConstructParamEnum &isConstructorParameter,
    const BuildAccessEnum &inputAutomaticGenerationOfDataAccessFunctions,
    const TraversalEnum &toBeTraversedDuringTreeTraversal,
    const DeleteEnum &delete_flag, const CopyConfigEnum &_toBeCopied)
    : GrammarString(inputTypeNameString, inputVariableNameString,
                    inputDefaultInitializerString, isConstructorParameter,
                    inputAutomaticGenerationOfDataAccessFunctions,
                    toBeTraversedDuringTreeTraversal, delete_flag, _toBeCopied,
                    std::nullopt, DIRECT_TRAVERSAL_ACCESS) {}

GrammarString::GrammarString(
    const string &inputTypeNameString, const string &inputVariableNameString,
    const string &inputDefaultInitializerString,
    const ConstructParamEnum &isConstructorParameter,
    const BuildAccessEnum &inputAutomaticGenerationOfDataAccessFunctions,
    const TraversalEnum &toBeTraversedDuringTreeTraversal,
    const DeleteEnum &delete_flag, const CopyConfigEnum &_toBeCopied,
    std::optional<TraversalCardinalityEnum> traversalCardinality,
    const TraversalAccessorEnum &traversalAccessor,
    CopyConstructionOrderEnum copyConstructionOrder)
    : typeNameString(inputTypeNameString),
      variableNameString(inputVariableNameString),
      defaultInitializerString(inputDefaultInitializerString),
      p_isInConstructorParameterList(isConstructorParameter),
      p_traversalCardinality(traversalCardinality),
      p_traversalAccessor(traversalAccessor),
      p_traversalStorage(NOT_A_TRAVERSAL_MEMBER),
      p_schemaStorage(SCALAR_SCHEMA_STORAGE),
      p_schemaElement(VALUE_SCHEMA_ELEMENT), toBeCopied(_toBeCopied),
      toBeTraversed(toBeTraversedDuringTreeTraversal),
      toBeDeleted(delete_flag) {
  initializeSchemaMetadata();
  initializeTraversalMetadata();
  if (toBeCopied == COPY_CONSTRUCTOR_REFERENCE)
    ROSE_ASSERT(p_isInConstructorParameterList == CONSTRUCTOR_PARAMETER &&
                toBeTraversed == NO_TRAVERSAL && toBeDeleted == NO_DELETE &&
                (p_schemaStorage == SCALAR_SCHEMA_STORAGE ||
                 p_schemaStorage == VALUE_CONTAINER_SCHEMA_STORAGE) &&
                p_schemaElement == POINTER_SCHEMA_ELEMENT);
  ROSE_ASSERT(copyConstructionOrder == DEFAULT_COPY_CONSTRUCTION_ORDER ||
              copyConstructionOrder == COPY_BEFORE_CONSTRUCTOR_DEPENDENCIES ||
              copyConstructionOrder == COPY_AFTER_OWNER_CONSTRUCTION ||
              copyConstructionOrder == COPY_BEFORE_OTHER_MEMBERS);
  p_copyConstructionOrder = copyConstructionOrder;
  if (copyConstructionOrder == COPY_BEFORE_OTHER_MEMBERS) {
    ROSE_ASSERT(p_isInConstructorParameterList == NO_CONSTRUCTOR_PARAMETER &&
                toBeTraversed == DEF_TRAVERSAL && toBeCopied == COPY_DATA &&
                toBeDeleted == NO_DELETE &&
                p_schemaStorage == VALUE_CONTAINER_SCHEMA_STORAGE &&
                p_schemaElement == POINTER_SCHEMA_ELEMENT);
  }
  if (toBeDeleted == DELETE_IF_OWNED)
    ROSE_ASSERT((p_schemaStorage == SCALAR_SCHEMA_STORAGE ||
                 (p_schemaStorage == VALUE_CONTAINER_SCHEMA_STORAGE &&
                  p_isInConstructorParameterList == CONSTRUCTOR_PARAMETER)) &&
                p_schemaElement == POINTER_SCHEMA_ELEMENT &&
                toBeCopied == CLONE_TREE);
  if (copyConstructionOrder == COPY_BEFORE_CONSTRUCTOR_DEPENDENCIES) {
    const bool primaryOwner = toBeTraversed == DEF_TRAVERSAL &&
                              toBeCopied == COPY_DATA &&
                              toBeDeleted == NO_DELETE;
    ROSE_ASSERT(p_isInConstructorParameterList == CONSTRUCTOR_PARAMETER &&
                p_schemaStorage == SCALAR_SCHEMA_STORAGE &&
                p_schemaElement == POINTER_SCHEMA_ELEMENT &&
                ((toBeCopied == CLONE_TREE && toBeDeleted == DEF_DELETE) ||
                 primaryOwner));
  }
  if (copyConstructionOrder == COPY_AFTER_OWNER_CONSTRUCTION) {
    ROSE_ASSERT(
        p_isInConstructorParameterList == CONSTRUCTOR_PARAMETER &&
        toBeTraversed == DEF_TRAVERSAL &&
        ((p_traversalStorage == NODE_POINTER_TRAVERSAL_MEMBER) ||
         p_traversalStorage == NODE_POINTER_CONTAINER_TRAVERSAL_MEMBER) &&
        toBeCopied != NO_COPY_DATA);
  }
  functionNameString = inputTypeNameString + " " + inputVariableNameString +
                       " " + inputDefaultInitializerString + ";";

  ROSE_ASSERT(!functionNameString.empty());

  automaticGenerationOfDataAccessFunctions =
      inputAutomaticGenerationOfDataAccessFunctions;
}

GrammarString::GrammarString(const string &inputFunctionNameString,
                             std::pair<std::string, std::string> fragment)
    : functionNameString(inputFunctionNameString),
      sourceFragment(std::move(fragment)), typeNameString(""),
      variableNameString(""), defaultInitializerString(""),
      p_isInConstructorParameterList(CONSTRUCTOR_PARAMETER),
      p_traversalCardinality(std::nullopt),
      p_traversalAccessor(DIRECT_TRAVERSAL_ACCESS),
      p_traversalStorage(NOT_A_TRAVERSAL_MEMBER),
      p_schemaStorage(SCALAR_SCHEMA_STORAGE),
      p_schemaElement(VALUE_SCHEMA_ELEMENT), toBeCopied(COPY_DATA),
      toBeTraversed(DEF_TRAVERSAL),
      automaticGenerationOfDataAccessFunctions(BUILD_ACCESS_FUNCTIONS),
      toBeDeleted(NO_DELETE) {
  ROSE_ASSERT(!functionNameString.empty());
}

GrammarString::GrammarString(const GrammarString &X)
    : functionNameString(""), typeNameString(""), variableNameString(""),
      defaultInitializerString(""),
      p_isInConstructorParameterList(CONSTRUCTOR_PARAMETER),
      p_traversalCardinality(std::nullopt),
      p_traversalAccessor(DIRECT_TRAVERSAL_ACCESS),
      p_traversalStorage(NOT_A_TRAVERSAL_MEMBER),
      p_schemaStorage(SCALAR_SCHEMA_STORAGE),
      p_schemaElement(VALUE_SCHEMA_ELEMENT), toBeCopied(X.toBeCopied),
      toBeTraversed(DEF_TRAVERSAL),
      automaticGenerationOfDataAccessFunctions(BUILD_ACCESS_FUNCTIONS),
      toBeDeleted(NO_DELETE) {
  *this = X;
}

GrammarString &GrammarString::operator=(const GrammarString &X) {
  functionNameString = X.functionNameString;
  sourceFragment = X.sourceFragment;

  typeNameString = X.typeNameString;
  variableNameString = X.variableNameString;
  defaultInitializerString = X.defaultInitializerString;

  automaticGenerationOfDataAccessFunctions =
      X.automaticGenerationOfDataAccessFunctions;
  p_isInConstructorParameterList = X.p_isInConstructorParameterList;
  p_copyConstructionOrder = X.p_copyConstructionOrder;
  p_traversalCardinality = X.p_traversalCardinality;
  p_traversalAccessor = X.p_traversalAccessor;
  p_traversalStorage = X.p_traversalStorage;
  p_traversalElementTypeName = X.p_traversalElementTypeName;
  p_schemaStorage = X.p_schemaStorage;
  p_schemaElement = X.p_schemaElement;
  p_schemaElementTypeName = X.p_schemaElementTypeName;
  p_includeInRTI = X.p_includeInRTI;
  toBeTraversed = X.toBeTraversed;
  toBeCopied = X.toBeCopied;
  toBeDeleted = X.toBeDeleted;

  return *this;
}

bool operator!=(const GrammarString &X, const GrammarString &Y) {
  return !(X == Y);
}

bool operator==(const GrammarString &X, const GrammarString &Y) {
  ROSE_ASSERT(!X.functionNameString.empty());
  ROSE_ASSERT(!Y.functionNameString.empty());
  return X.sourceFragment == Y.sourceFragment &&
         X.functionNameString == Y.functionNameString;
}

DeleteEnum GrammarString::getToBeDeleted() const { return toBeDeleted; }

void GrammarString::setAutomaticGenerationOfDataAccessFunctions(
    const BuildAccessEnum &X) {
  automaticGenerationOfDataAccessFunctions = X;
}

void GrammarString::setIsInConstructorParameterList(ConstructParamEnum X) {
  p_isInConstructorParameterList = X;
}

BuildAccessEnum GrammarString::generateDataAccessFunctions() const {
  return automaticGenerationOfDataAccessFunctions;
}

void GrammarString::consistencyCheck() const {
  ROSE_ASSERT(!functionNameString.empty());
}

bool GrammarString::isContainedIn(const string &longString,
                                  const string &shortString) {
  return (longString.find(shortString) != string::npos);
}
