#include "sage3basic.h"

#include "transformationTracking.h"

#include "wholeAST.h"

#include "rose_config.h"

#include <fstream>
#include <functional>
#include <limits>
#include <memory>
#include <stdexcept>

using namespace std;
using namespace Rose;

CustomAstDOTGenerationEdgeType::CustomAstDOTGenerationEdgeType(
    SgNode *n1, SgNode *n2, std::string label, std::string options)
    : start(n1), end(n2), labelString(label), optionString(options) {}

bool CustomAstDOTGenerationEdgeType::operator!=(
    const CustomAstDOTGenerationEdgeType &edge) const {
  return !(*this == edge);
}

bool CustomAstDOTGenerationEdgeType::operator==(
    const CustomAstDOTGenerationEdgeType &edge) const {
  return start == edge.start && end == edge.end &&
         labelString == edge.labelString;
}

bool CustomAstDOTGenerationEdgeType::operator<(
    const CustomAstDOTGenerationEdgeType &edge) const {
  if (start != edge.start)
    return std::less<SgNode *>{}(edge.start, start);
  if (end != edge.end)
    return std::less<SgNode *>{}(edge.end, end);
  return edge.labelString < labelString;
}

CustomAstDOTGeneration::~CustomAstDOTGeneration() {}

void CustomAstDOTGenerationData::addNode(NodeType n) {
  ROSE_ASSERT(n.optionString.size() < 4000);
  nodeList.push_back(n);
}

void CustomAstDOTGenerationData::addEdge(EdgeType e) {
  edgeList.push_back(e);
}

void CustomAstDOTGenerationData::addEdges() {
  // Add the local list of edges to the dot graph

  for (std::list<EdgeType>::iterator i = edgeList.begin(); i != edgeList.end();
       i++) {
    ROSE_ASSERT((*i).start != NULL);
    ROSE_ASSERT((*i).end != NULL);
    dotrep.addEdge((*i).start, (*i).end, (*i).optionString);
  }
}

std::string CustomAstDOTGenerationData::additionalNodeInfo(SgNode *node) {
  // This virtual function only modifies the node label not the options list
  // that would follow the node's label in the final dot output file.
  std::string returnString;

  const NodeType tmp(node, "", "");
  std::list<NodeType>::iterator i = nodeList.end();
  i = find(nodeList.begin(), nodeList.end(), tmp);
  if (i != nodeList.end()) {
    returnString = (*i).labelString;
  }

  return returnString;
}

std::string CustomAstDOTGenerationData::additionalNodeOptions(SgNode *node) {

  std::string returnString;

  const NodeType tmp(node, "", "");
  std::list<NodeType>::iterator i = nodeList.begin();
  i = find(nodeList.begin(), nodeList.end(), tmp);
  if (i != nodeList.end()) {
    ROSE_ASSERT(i->target == node);
    returnString = (*i).optionString;
  }

  return returnString;
}

std::string CustomAstDOTGenerationData::additionalEdgeInfo(SgNode *from,
                                                           SgNode *to,
                                                           std::string label) {
  std::string returnString;

  const EdgeType tmp(from, to, label, "");
  std::list<EdgeType>::iterator i = edgeList.end();
  i = find(edgeList.begin(), edgeList.end(), tmp);
  if (i != edgeList.end()) {
    returnString = (*i).labelString;
  }

  return returnString;
}

string
CustomAstDOTGenerationData::unparseToCompleteStringForDOT(SgNode *astNode) {
  SgUnparse_Info info;
  info.unset_SkipComments();
  info.unset_SkipWhitespaces();
  return StringUtility::escapeNewlineAndDoubleQuoteCharacters(
      globalUnparseToString(astNode, &info));
}

void CustomAstDOTGenerationData::internalGenerateGraph(std::string filename,
                                                       SgProject *project) {
  std::string label =
      "CustomAstDOTGenerationData::internalGenerateGraph(" + filename + "):";
  TimingPerformance timer(label);

  // Required setup on base class
  init();

  traversal = AstDOTGeneration::TOPDOWNBOTTOMUP;

  // Build the DOT specific inherited attribute
  DOTInheritedAttribute ia;

  traverse(project, ia);

  addEdges();

  dotrep.writeToFileAsGraph(filename + ".dot");
}

void CustomAstDOTGeneration::addNode(NodeType n) {
  ROSE_ASSERT(n.optionString.size() < 4000);
  DOTgraph.addNode(n);
}

void CustomAstDOTGeneration::addEdge(EdgeType e) {
  DOTgraph.addEdge(e);
}

void CustomAstDOTGeneration::internalGenerateGraph(std::string filename,
                                                   SgProject *project) {
  DOTgraph.internalGenerateGraph(filename, project);
}

CustomMemoryPoolDOTGenerationData::~CustomMemoryPoolDOTGenerationData() {}

void CustomMemoryPoolDOTGenerationData::internalGenerateGraph(
    std::string dotfilename) {
  std::string label =
      "CustomMemoryPoolDOTGenerationData::internalGenerateGraph(" +
      dotfilename + "):";
  TimingPerformance timer(label);

  filename = dotfilename;

  traverseMemoryPool();
  dotrep.writeToFileAsGraph(filename + ".dot");
}

void CustomMemoryPoolDOTGenerationData::addNode(NodeType n) {
  nodeList.push_back(n);
}

void CustomMemoryPoolDOTGenerationData::addEdge(EdgeType e) {
  edgeList.push_back(e);
}

void CustomMemoryPoolDOTGenerationData::skipNode(SgNode *n) {
  // This is the normal (non-debugging) mode.
  skipNodeList.insert(n);
}

void CustomMemoryPoolDOTGenerationData::skipEdge(EdgeType e) {
  ROSE_ASSERT(e.start != nullptr);
  // Container entries can share the same source, target and display label.
  skipEdgeSet.insert(e);
}

std::string
CustomMemoryPoolDOTGenerationData::additionalNodeInfo(SgNode *node) {
  // This virtual function only modifies the node label not the options list
  // that would follow the node's label in the final dot output file.
  std::string returnString;

  const NodeType tmp(node, "", "");
  std::list<NodeType>::iterator i = nodeList.end();

  i = find(nodeList.begin(), nodeList.end(), tmp);
  if (i != nodeList.end()) {
    returnString = (*i).labelString;
  }

  return returnString;
}

std::string
CustomMemoryPoolDOTGenerationData::additionalNodeOptions(SgNode *node) {

  std::string returnString;

  const NodeType tmp(node, "", "");
  std::list<NodeType>::iterator i = nodeList.end();
  i = find(nodeList.begin(), nodeList.end(), tmp);
  if (i != nodeList.end()) {
    returnString = (*i).optionString;
  }

  return returnString;
}

std::string
CustomMemoryPoolDOTGenerationData::additionalEdgeInfo(SgNode *from, SgNode *to,
                                                      std::string label) {
  // This virtual function only modifies the node label not the options list
  // that would follow the node's label in the final dot output file.
  std::string returnString;

  const EdgeType tmp(from, to, label);
  std::list<EdgeType>::iterator i = edgeList.end();
  i = find(edgeList.begin(), edgeList.end(), tmp);
  if (i != edgeList.end()) {
    returnString = (*i).labelString;
  }

  return returnString;
}

std::string CustomMemoryPoolDOTGenerationData::additionalEdgeOptions(
    SgNode *from, SgNode *to, std::string label) {

  std::string returnString;
  // DQ (3/6/2007): This builds a default set of mappings of edge colors and
  // edge options to edges.

  // Color all edges that lead to a SgType (this is overwritten for the parent
  // edges)
  SgType *type = isSgType(to);
  if (type != NULL) {
    returnString = "color=\"gold1\" ";
  }

  // DQ (3/5/2007): color the parent edges blue
  if (from->get_parent() == to && label == "parent") {
    returnString = "color=\"blue\" ";
  }

  // DQ (3/5/2007): color the scope edges green (that are explicitly stored in
  // the AST) SgScopeStatement* scopeStatement = isSgScopeStatement(to);
  SgStatement *statement = isSgStatement(from);
  if (statement != NULL) {
    if (statement->hasExplicitScope() == true && statement->get_scope() == to &&
        label == "scope") {
      returnString = "color=\"green\" ";
    }
  }

  SgInitializedName *initializedName = isSgInitializedName(from);
  if (initializedName != NULL) {
    if (initializedName->get_scope() == to && label == "scope") {
      returnString = "color=\"green\" ";
    }
  }

  return returnString;
}

void CustomMemoryPoolDOTGenerationData::visit(SgNode *node) {
  ROSE_ASSERT(node != nullptr);
  if (!visitedNodes.insert(node).second || skipNodeList.count(node) != 0)
    return;

  std::string nodelabel = std::string("\\n") + node->class_name();
  nodelabel += additionalNodeInfo(node);
  dotrep.addNode(node, nodelabel, additionalNodeOptions(node));

  for (const auto &[target, label] : node->returnDataMemberPointers()) {
    EdgeType edge(node, target, label);
    auto skipped_edge = skipEdgeSet.find(edge);
    if (target == nullptr) {
      if (skipped_edge == skipEdgeSet.end())
        dotrep.addNullValue(node, "", label, "");
      continue;
    }
    if (skipNodeList.count(target) != 0)
      continue;
    if (skipped_edge != skipEdgeSet.end()) {
      ROSE_ASSERT(label == skipped_edge->labelString);
      continue;
    }

    std::string edge_option = additionalEdgeOptions(node, target, label);
    std::string edge_label = additionalEdgeInfo(node, target, label);
    dotrep.addEdge(node, label + edge_label, target,
                   edge_option + "dir=forward");
  }
}

CustomMemoryPoolDOTGeneration::CustomMemoryPoolDOTGeneration(
    s_Filter_Flags *f /*= NULL*/) {
  if (f == nullptr) {
    ownedFilterFlags = std::make_unique<s_Filter_Flags>();
    filterFlags = ownedFilterFlags.get();
  } else {
    filterFlags = f;
  }
}

void CustomMemoryPoolDOTGeneration::addNode(NodeType n) { DOTgraph.addNode(n); }

void CustomMemoryPoolDOTGeneration::addEdge(EdgeType e) { DOTgraph.addEdge(e); }

void CustomMemoryPoolDOTGeneration::skipNode(SgNode *n) {
  DOTgraph.skipNode(n);
}

void CustomMemoryPoolDOTGeneration::skipEdge(EdgeType e) {
  DOTgraph.skipEdge(e);
}

void CustomMemoryPoolDOTGeneration::internalGenerateGraph(
    std::string filename) {
  DOTgraph.internalGenerateGraph(filename);
}

void CustomMemoryPoolDOTGeneration::edgeFilter(SgNode *nodeSource,
                                               SgNode *nodeSink,
                                               std::string edgeName) {
  ROSE_ASSERT(nodeSource != NULL);
  if (edgeName != "parent" && nodeSink == NULL) {
    EdgeType edge(nodeSource, nodeSink, edgeName);
    skipEdge(edge);
  }

  if (DOTgraph.skipNodeList.find(nodeSource) != DOTgraph.skipNodeList.end()) {
    if (nodeSink != NULL &&
        DOTgraph.skipNodeList.find(nodeSink) != DOTgraph.skipNodeList.end()) {
      EdgeType edge(nodeSource, nodeSink, edgeName);
      skipEdge(edge);
    }
  }
}

void CustomMemoryPoolDOTGeneration::typeFilter(SgNode *node) {
  // This function skips the representation of types and IR nodes associated
  // with types
  if (isSgType(node) != NULL) {
    skipNode(node);
  }

  if (isSgFunctionParameterTypeList(node) != NULL) {
    skipNode(node);
  }
}

void CustomMemoryPoolDOTGeneration::symbolFilter(SgNode *node) {
  // This function skips the representation of types and IR nodes associated
  // with types
  if (isSgSymbol(node) != NULL) {
    skipNode(node);
  }
}

void CustomMemoryPoolDOTGeneration::emptySymbolTableFilter(SgNode *node) {
  // This function skips the representation of types and IR nodes associated
  // with types
  if (isSgSymbolTable(node) != NULL) {
    skipNode(node);
  }
}

void CustomMemoryPoolDOTGeneration::emptyBasicBlockFilter(SgNode *node) {
  // This function skips the representation of types and IR nodes associated
  // with types
  SgBasicBlock *block = isSgBasicBlock(node);
  if (block != NULL) {
    if (block->get_statements().empty() == true) {
      skipNode(node);
    }
  }
}

void CustomMemoryPoolDOTGeneration::emptyFunctionParameterListFilter(
    SgNode *node) {
  // This function skips the representation of types and IR nodes associated
  // with types
  SgFunctionParameterList *parameterList = isSgFunctionParameterList(node);
  if (parameterList != NULL) {
    if (parameterList->get_args().empty() == true) {
      skipNode(node);
    }
  }
}

void CustomMemoryPoolDOTGeneration::expressionFilter(SgNode *node) {
  // This function skips the representation of types and IR nodes associated
  // with types
  if (isSgExpression(node) != NULL) {
    skipNode(node);
  }
}

void CustomMemoryPoolDOTGeneration::variableDefinitionFilter(SgNode *node) {
  // This function skips the representation of types and IR nodes associated
  // with types
  if (isSgVariableDefinition(node) != NULL) {
    skipNode(node);
  }
}

void CustomMemoryPoolDOTGeneration::variableDeclarationFilter(SgNode *node) {
  // This function skips the representation of types and IR nodes associated
  // with types
  if (isSgVariableDeclaration(node) != NULL) {
    skipNode(node);
  }

  if (isSgInitializedName(node) != NULL) {
    skipNode(node);
  }

  if (isSgStorageModifier(node) != NULL) {
    skipNode(node);
  }

  if (isSgFunctionParameterList(node) != NULL) {
    skipNode(node);
  }

  variableDefinitionFilter(node);
}

void CustomMemoryPoolDOTGeneration::ctorInitializerListFilter(SgNode *node) {
  // This function skips the representation of types and IR nodes associated
  // with types
  if (isSgCtorInitializerList(node) != NULL) {
    skipNode(node);
  }
}

void CustomMemoryPoolDOTGeneration::commentAndDirectiveFilter(SgNode *node) {
  // Skip the Sg_File_Info objects associated with comments

  SgLocatedNode *locatedNode = isSgLocatedNode(node);
  if (locatedNode != NULL) {
    AttachedPreprocessingInfoType *comments =
        locatedNode->getAttachedPreprocessingInfo();
    if (comments != NULL) {
      AttachedPreprocessingInfoType::iterator i;
      for (i = comments->begin(); i != comments->end(); i++) {
        ROSE_ASSERT((*i) != NULL);
        ROSE_ASSERT((*i)->get_file_info() != NULL);
        skipNode((*i)->get_file_info());
      }
    }
  }
}

void CustomMemoryPoolDOTGeneration::fileInfoFilter(SgNode *node) {
  // Skip the Sg_File_Info objects associated with comments

  Sg_File_Info *fileInfoNode = isSg_File_Info(node);
  if (fileInfoNode != NULL) {
    skipNode(fileInfoNode);
  }
}

void CustomMemoryPoolDOTGeneration::frontendCompatibilityFilter(SgNode *node) {
  ROSE_ASSERT(node != NULL);

  SgFunctionSymbol *functionSymbol = isSgFunctionSymbol(node);
  if (functionSymbol != NULL) {
    SgDeclarationStatement *declaration = functionSymbol->get_declaration();
    ROSE_ASSERT(declaration != NULL);
    ROSE_ASSERT(declaration->get_file_info() != NULL);
    if (declaration != NULL &&
        declaration->get_file_info()->isFrontendSpecific() == true) {
      skipNode(functionSymbol);
    }
  }

  SgTypedefSymbol *typedefSymbol = isSgTypedefSymbol(node);
  if (typedefSymbol != NULL) {
    SgDeclarationStatement *declaration = typedefSymbol->get_declaration();
    if (declaration->get_file_info()->isFrontendSpecific() == true) {
      skipNode(typedefSymbol);
    }
  }

  SgVariableSymbol *variableSymbol = isSgVariableSymbol(node);
  if (variableSymbol != NULL) {
    SgInitializedName *declaration = variableSymbol->get_declaration();
    ROSE_ASSERT(declaration != NULL);
    if (declaration->get_file_info()->isFrontendSpecific() == true) {
      skipNode(variableSymbol);
    }

    SgStorageModifier &storageModifier = declaration->get_storageModifier();
    skipNode(&storageModifier);
  }

  SgFunctionParameterList *functionParateterList =
      isSgFunctionParameterList(node);
  if (functionParateterList != NULL) {
    SgInitializedNamePtrList::iterator i =
        functionParateterList->get_args().begin();
    while (i != functionParateterList->get_args().end()) {
      SgInitializedName *declaration = *i;
      ROSE_ASSERT(declaration != NULL);
      if (declaration->get_file_info()->isFrontendSpecific() == true) {
        SgStorageModifier &storageModifier = declaration->get_storageModifier();
        skipNode(&storageModifier);
      }

      i++;
    }
  }

  Sg_File_Info *fileInfo = node->get_file_info();
  if (fileInfo != NULL && fileInfo->isFrontendSpecific() == true) {
    skipNode(node);
  }

  Sg_File_Info *currentNodeFileInfo = isSg_File_Info(node);
  if (currentNodeFileInfo != NULL) {
    if (currentNodeFileInfo->isFrontendSpecific() == true) {
      skipNode(currentNodeFileInfo);
    }
  }
}

void CustomMemoryPoolDOTGeneration::defaultFilter(SgNode *node) {
  // Default fileter to simplify the whole AST graph

  if (filterFlags->m_commentAndDirective == 1)
    commentAndDirectiveFilter(node);

  if (filterFlags->m_edge == 1) {
    std::vector<std::pair<SgNode *, std::string>> listOfIRnodes =
        node->returnDataMemberPointers();
    std::vector<std::pair<SgNode *, std::string>>::iterator i =
        listOfIRnodes.begin();
    while (i != listOfIRnodes.end()) {
      // Might we want to test if this is node previously ignored (a gnu
      // compatibility IR node) and do something like include that IR node in
      // the AST?
      SgNode *n = (*i).first;
      std::string edgelabel = (*i).second;
      // Run all the edges through the filter (the filter will single out the
      // ones to be skipped)
      edgeFilter(node, n, edgelabel);

      i++;
    }
  }
}

void CustomMemoryPoolDOTGeneration::defaultColorFilter(SgNode *node) {
  ROSE_ASSERT(node != NULL);

  SgStatement *statement = isSgStatement(node);
  if (statement != NULL) {
    string additionalNodeOptions;

    // Make this statement different in the generated dot graph
    string labelWithSourceCode;

    switch (statement->variantT()) {
      // DQ (2/27/2012): Added template support (new template declaration IR
      // nodes).
    case V_SgTemplateFunctionDeclaration:
    case V_SgTemplateMemberFunctionDeclaration: {
      SgFunctionDeclaration *functionDeclaration =
          isSgFunctionDeclaration(node);
      additionalNodeOptions =
          "shape=polygon,regular=0,URL=\"\\N\",tooltip=\"more info at "
          "\\N\",sides=5,peripheries=1,color=\"blue\",fillcolor=blueviolet,"
          "fontname=\"7x13bold\",fontcolor=black,style=filled";
      string forwardFlagString = (functionDeclaration->isForward() == true)
                                     ? "isForward"
                                     : "!isForward";
      string friendFlagString =
          (functionDeclaration->get_declarationModifier().isFriend() == true)
              ? "isFriend"
              : "!isFriend";
      // DQ (5/10/2018): Added more debugging information now the we explicitly
      // mark when a function is implicit.
      string implicitFlagString =
          (functionDeclaration->get_is_implicit_function() == true)
              ? "is_implicit_function"
              : "!is_implicit_function";
      labelWithSourceCode =
          string("\\n  ") + functionDeclaration->get_name().getString() +
          "\\n  " + forwardFlagString + "\\n  " + friendFlagString + "\\n  " +
          implicitFlagString + "\\n  " +
          StringUtility::numberToString(functionDeclaration) + "  ";
      break;
    }

    case V_SgFunctionDeclaration:
    case V_SgProgramHeaderStatement:
    case V_SgProcedureHeaderStatement:
    case V_SgMemberFunctionDeclaration:
      {
        SgFunctionDeclaration *functionDeclaration =
            isSgFunctionDeclaration(node);
        additionalNodeOptions =
            "shape=polygon,regular=0,URL=\"\\N\",tooltip=\"more info at "
            "\\N\",sides=5,peripheries=1,color=\"blue\",fillcolor=royalblue,"
            "fontname=\"7x13bold\",fontcolor=black,style=filled";
        string forwardFlagString = (functionDeclaration->isForward() == true)
                                       ? "isForward"
                                       : "!isForward";
        string friendFlagString =
            (functionDeclaration->get_declarationModifier().isFriend() == true)
                ? "isFriend"
                : "!isFriend";
        // DQ (5/10/2018): Added more debugging information now the we
        // explicitly mark when a function is implicit.
        string implicitFlagString =
            (functionDeclaration->get_is_implicit_function() == true)
                ? "is_implicit_function"
                : "!is_implicit_function";
        labelWithSourceCode =
            string("\\n  ") + functionDeclaration->get_name().getString() +
            "\\n  " + forwardFlagString + "\\n  " + friendFlagString + "\\n  " +
            implicitFlagString + "\\n  " +
            StringUtility::numberToString(functionDeclaration) + "  ";
        break;
      }

      // DQ (7/22/2012): Distingish between these different types.
    case V_SgTemplateInstantiationFunctionDecl:       // functions
    case V_SgTemplateInstantiationMemberFunctionDecl: // functions
    {
      SgFunctionDeclaration *functionDeclaration =
          isSgFunctionDeclaration(node);
      additionalNodeOptions =
          "shape=polygon,regular=0,URL=\"\\N\",tooltip=\"more info at "
          "\\N\",sides=5,peripheries=1,color=\"blue\",fillcolor=lightseagreen,"
          "fontname=\"7x13bold\",fontcolor=black,style=filled";
      string forwardFlagString = (functionDeclaration->isForward() == true)
                                     ? "isForward"
                                     : "!isForward";
      string friendFlagString =
          (functionDeclaration->get_declarationModifier().isFriend() == true)
              ? "isFriend"
              : "!isFriend";
      // DQ (5/10/2018): Added more debugging information now the we explicitly
      // mark when a function is implicit.
      string implicitFlagString =
          (functionDeclaration->get_is_implicit_function() == true)
              ? "is_implicit_function"
              : "!is_implicit_function";
      labelWithSourceCode =
          string("\\n  ") + functionDeclaration->get_name().getString() +
          "\\n  " + forwardFlagString + "\\n  " + friendFlagString + "\\n  " +
          implicitFlagString + "\\n  " +
          StringUtility::numberToString(functionDeclaration) + "  ";
      break;
    }

    case V_SgTemplateDeclaration: {
      // This type is not supposed to be used in the new legacy
      // frontend 4.x work.
      SgTemplateDeclaration *templateDeclaration =
          isSgTemplateDeclaration(node);
      additionalNodeOptions =
          "shape=polygon,regular=0,URL=\"\\N\",tooltip=\"more info "
          "at "
          "\\N\",sides=8,peripheries=2,color=\"blue\",fillcolor="
          "green,fontname=\"7x13bold\",fontcolor=black,style=filled";
      labelWithSourceCode =
          "\\n  " + templateDeclaration->get_name().getString() + "\\n  " +
          StringUtility::numberToString(templateDeclaration) + "  ";
      break;
    }

      // DQ (7/22/2012): Distingish between these template class and non
      // template class IR nodes. DQ (2/27/2012): Added template support (new
      // template declaration IR nodes).
    case V_SgTemplateClassDeclaration: {
      SgClassDeclaration *classDeclaration = isSgClassDeclaration(node);
      additionalNodeOptions =
          "shape=polygon,regular=0,URL=\"\\N\",tooltip=\"more info at "
          "\\N\",sides=8,peripheries=2,color=\"blue\",fillcolor=blueviolet,"
          "fontname=\"7x13bold\",fontcolor=black,style=filled";
      string flagString =
          (classDeclaration->isForward() == true) ? "isForward" : "!isForward";
      string flagString2 =
          (classDeclaration->get_isAutonomousDeclaration() == true)
              ? "isAutonomousDeclaration"
              : "!isAutonomousDeclaration";
      labelWithSourceCode =
          "\\n  " + classDeclaration->get_name().getString() + "\\n  " +
          flagString + "\\n  " + flagString2 + "\\n  " +
          StringUtility::numberToString(classDeclaration) + "  ";
      break;
    }

    case V_SgClassDeclaration:
    case V_SgModuleStatement:
      {
        SgClassDeclaration *classDeclaration = isSgClassDeclaration(node);
        additionalNodeOptions =
            "shape=polygon,regular=0,URL=\"\\N\",tooltip=\"more info at "
            "\\N\",sides=8,peripheries=2,color=\"blue\",fillcolor=peru,"
            "fontname=\"7x13bold\",fontcolor=black,style=filled";
        string flagString = (classDeclaration->isForward() == true)
                                ? "isForward"
                                : "!isForward";
        string flagString2 =
            (classDeclaration->get_isAutonomousDeclaration() == true)
                ? "isAutonomousDeclaration"
                : "!isAutonomousDeclaration";
        labelWithSourceCode =
            "\\n  " + classDeclaration->get_name().getString() + "\\n  " +
            flagString + "\\n  " + flagString2 + "\\n  " +
            StringUtility::numberToString(classDeclaration) + "  ";
        break;
      }

    case V_SgTemplateInstantiationDecl: {
      SgClassDeclaration *classDeclaration = isSgClassDeclaration(node);
      additionalNodeOptions =
          "shape=polygon,regular=0,URL=\"\\N\",tooltip=\"more info at "
          "\\N\",sides=8,peripheries=2,color=\"blue\",fillcolor=green,fontname="
          "\"7x13bold\",fontcolor=black,style=filled";
      string flagString =
          (classDeclaration->isForward() == true) ? "isForward" : "!isForward";
      string flagString2 =
          (classDeclaration->get_isAutonomousDeclaration() == true)
              ? "isAutonomousDeclaration"
              : "!isAutonomousDeclaration";
      labelWithSourceCode =
          "\\n  " + classDeclaration->get_name().getString() + "\\n  " +
          flagString + "\\n  " + flagString2 + "\\n  " +
          StringUtility::numberToString(classDeclaration) + "  ";
      break;
    }

    case V_SgTypedefDeclaration: {
      SgTypedefDeclaration *typedefDeclaration = isSgTypedefDeclaration(node);
      additionalNodeOptions =
          "shape=polygon,regular=0,URL=\"\\N\",tooltip=\"more info at "
          "\\N\",sides=8,peripheries=2,color=\"blue\",fillcolor=peru,fontname="
          "\"7x13bold\",fontcolor=black,style=filled";
      string flagString =
          (typedefDeclaration
               ->get_typedefBaseTypeContainsDefiningDeclaration() == true)
              ? "typedefBaseTypeContainsDefiningDeclaration"
              : "!typedefBaseTypeContainsDefiningDeclaration";
      // string flagString2 = (typedefDeclaration->get_isAutonomousDeclaration()
      // == true) ? "isAutonomousDeclaration" : "!isAutonomousDeclaration";
      labelWithSourceCode =
          "\\n  " + typedefDeclaration->get_name().getString() + "\\n  " +
          flagString +
          // "\\n  " + flagString2 +
          "\\n  " + StringUtility::numberToString(typedefDeclaration) + "  ";
      break;
    }

      // DQ (11/4/2014): Added support for C++11 template typedefs.
    case V_SgTemplateTypedefDeclaration: {
      SgTemplateTypedefDeclaration *templateTypedefDeclaration =
          isSgTemplateTypedefDeclaration(node);
      additionalNodeOptions =
          "shape=polygon,regular=0,URL=\"\\N\",tooltip=\"more info at "
          "\\N\",sides=8,peripheries=2,color=\"blue\",fillcolor=blueviolet,"
          "fontname=\"7x13bold\",fontcolor=black,style=filled";
      string flagString = (templateTypedefDeclaration->isForward() == true)
                              ? "isForward"
                              : "!isForward";
      string flagString2 =
          (templateTypedefDeclaration->get_isAutonomousDeclaration() == true)
              ? "isAutonomousDeclaration"
              : "!isAutonomousDeclaration";
      labelWithSourceCode =
          "\\n  " + templateTypedefDeclaration->get_name().getString() +
          "\\n  " + flagString + "\\n  " + flagString2 + "\\n  " +
          StringUtility::numberToString(templateTypedefDeclaration) + "  ";
      break;
    }

      // DQ (11/5/2014): Added support for C++11 template typedefs.
    case V_SgTemplateInstantiationTypedefDeclaration: {
      SgTemplateInstantiationTypedefDeclaration
          *templateInstantiationTypedefDeclaration =
              isSgTemplateInstantiationTypedefDeclaration(node);
      additionalNodeOptions =
          "shape=polygon,regular=0,URL=\"\\N\",tooltip=\"more info at "
          "\\N\",sides=8,peripheries=2,color=\"blue\",fillcolor=green,fontname="
          "\"7x13bold\",fontcolor=black,style=filled";
      string flagString =
          (templateInstantiationTypedefDeclaration->isForward() == true)
              ? "isForward"
              : "!isForward";
      string flagString2 = (templateInstantiationTypedefDeclaration
                                ->get_isAutonomousDeclaration() == true)
                               ? "isAutonomousDeclaration"
                               : "!isAutonomousDeclaration";
      labelWithSourceCode =
          "\\n  " +
          templateInstantiationTypedefDeclaration->get_name().getString() +
          "\\n  " + flagString + "\\n  " + flagString2 + "\\n  " +
          StringUtility::numberToString(
              templateInstantiationTypedefDeclaration) +
          "  ";
      break;
    }

    case V_SgVariableDeclaration: {
      SgVariableDeclaration *variableDeclaration =
          isSgVariableDeclaration(node);
      additionalNodeOptions =
          "shape=polygon,regular=0,URL=\"\\N\",tooltip=\"more info at "
          "\\N\",sides=8,peripheries=2,color=\"blue\",fillcolor=peru,fontname="
          "\"7x13bold\",fontcolor=black,style=filled";
      string flagString =
          variableDeclaration->get_baseTypeDefiningDeclaration() != NULL
              ? "ownsBaseTypeDefiningDeclaration"
              : "referencesBaseTypeDeclaration";
      labelWithSourceCode = "\\n  " + flagString + "\\n  " +
                            StringUtility::numberToString(variableDeclaration) +
                            "  ";
      // DQ (6/15/2019): Debugging the outliner support and detection of
      // protected or private data members that trigger declarations to be
      // marked as friend declaration in the class definitions.

      string isPrivate = variableDeclaration->get_declarationModifier()
                                 .get_accessModifier()
                                 .isPrivate()
                             ? "true"
                             : "false";
      string isProtected = variableDeclaration->get_declarationModifier()
                                   .get_accessModifier()
                                   .isProtected()
                               ? "true"
                               : "false";
      string isPublic = variableDeclaration->get_declarationModifier()
                                .get_accessModifier()
                                .isPublic()
                            ? "true"
                            : "false";

      labelWithSourceCode += "\\n isPrivate   = " + isPrivate + "  ";
      labelWithSourceCode += "\\n isProtected = " + isProtected + "  ";
      labelWithSourceCode += "\\n isPublic    = " + isPublic + "  ";
      break;
    }

    case V_SgEnumDeclaration: {
      SgEnumDeclaration *enumDeclaration = isSgEnumDeclaration(node);
      additionalNodeOptions =
          "shape=polygon,regular=0,URL=\"\\N\",tooltip=\"more info at "
          "\\N\",sides=8,peripheries=2,color=\"blue\",fillcolor=green,fontname="
          "\"7x13bold\",fontcolor=black,style=filled";
      string flagString =
          (enumDeclaration->get_isAutonomousDeclaration() == true)
              ? "isAutonomousDeclaration"
              : "!isAutonomousDeclaration";
      labelWithSourceCode = "\\n  " + enumDeclaration->get_name().getString() +
                            "\\n  " + flagString + "\\n  " +
                            StringUtility::numberToString(enumDeclaration) +
                            "  ";
      break;
    }

    case V_SgNamespaceDeclarationStatement: {
      SgNamespaceDeclarationStatement *namespaceDeclaration =
          isSgNamespaceDeclarationStatement(node);
      additionalNodeOptions =
          "shape=polygon,regular=0,URL=\"\\N\",tooltip=\"more info at "
          "\\N\",sides=5,peripheries=2,color=\"blue\",fillcolor=green,fontname="
          "\"7x13bold\",fontcolor=black,style=filled";
      labelWithSourceCode =
          "\\n  " + namespaceDeclaration->get_name().getString() + "\\n  " +
          StringUtility::numberToString(namespaceDeclaration) + "  ";
      break;
    }

    case V_SgNamespaceDefinitionStatement: {
      additionalNodeOptions =
          "shape=polygon,regular=0,URL=\"\\N\",tooltip=\"more info at "
          "\\N\",sides=6,peripheries=2,color=\"blue\",fillcolor=green,fontname="
          "\"7x13bold\",fontcolor=black,style=filled";

      SgNamespaceDefinitionStatement *ns =
          isSgNamespaceDefinitionStatement(node);
      ROSE_ASSERT(ns != NULL);

      size_t index = ns->namespaceIndex() + 1;
      size_t count = ns->numberOfNamespaceDefinitions();

      if (ns->get_isUnionOfReentrantNamespaceDefinitions() == false) {
        labelWithSourceCode = "\\n  " + StringUtility::numberToString(index) +
                              " of " + StringUtility::numberToString(count) +
                              "  ";
      } else {
        labelWithSourceCode = "\\n  union of " +
                              StringUtility::numberToString(count) +
                              " reentrant namespace definitions ";
      }

      labelWithSourceCode +=
          "\\n  " + StringUtility::numberToString(node) + "  ";
      break;
    }

      // DQ (4/8/2018): Added support for the namespace alias.
    case V_SgNamespaceAliasDeclarationStatement: {
      SgNamespaceAliasDeclarationStatement *namespaceAliasDeclaration =
          isSgNamespaceAliasDeclarationStatement(node);
      additionalNodeOptions =
          "shape=polygon,regular=0,URL=\"\\N\",tooltip=\"more info at "
          "\\N\",sides=5,peripheries=2,color=\"blue\",fillcolor=green,fontname="
          "\"7x13bold\",fontcolor=black,style=filled";
      labelWithSourceCode =
          "\\n  " + namespaceAliasDeclaration->get_name().getString() +
          "\\n  " + StringUtility::numberToString(namespaceAliasDeclaration) +
          "  ";
      break;
    }

    default: {
      // It appears that we can't unparse one of these (not implemented)
      additionalNodeOptions =
          "shape=polygon,regular=0,URL=\"\\N\",tooltip=\"more info at "
          "\\N\",sides=5,peripheries=1,color=\"blue\",fillcolor=skyblue,"
          "fontname=\"7x13bold\",fontcolor=black,style=filled";
      labelWithSourceCode =
          string("\\n  ") + StringUtility::numberToString(node) + "  ";
      break;
    }
    }

    // Liao, 5/8/2014, display unique ID if set (non-zero)
    AST_NODE_ID id = TransformationTracking::getId(node);
    if (id != 0)
      labelWithSourceCode =
          string("\\n  ID: ") + StringUtility::numberToString(id) + "  ";
    NodeType graphNode(node, labelWithSourceCode, additionalNodeOptions);
    addNode(graphNode);
  }

  if (isSgExpression(node) != NULL) {
    string additionalNodeOptions =
        "shape=polygon,regular=0,URL=\"\\N\",tooltip=\"more info at "
        "\\N\",sides=6,peripheries=1,color=\"blue\",fillcolor=green,fontname="
        "\"7x13bold\",fontcolor=black,style=filled";
    // Make this statement different in the generated dot graph

    string labelWithSourceCode;
    labelWithSourceCode =
        string("\\n  ") + StringUtility::numberToString(node) + "  ";

    SgValueExp *valueExp = isSgValueExp(node);
    if (valueExp != NULL) {
      if (valueExp->get_parent() == NULL) {
        printf("Error: valueExp = %p = %s valueExp->get_parent() == NULL \n",
               valueExp, valueExp->class_name().c_str());
        valueExp->get_file_info()->display(
            "Error: valueExp->get_parent() == NULL");

        // In the case of a SgIntVal, we can try to output a little more
        // information
        SgIntVal *intVal = isSgIntVal(valueExp);
        if (intVal != NULL) {
          printf("Error: intVal = %d \n", intVal->get_value());
        }
        SgUnsignedLongVal *unsignedLongVal = isSgUnsignedLongVal(valueExp);
        if (unsignedLongVal != NULL) {
          printf("Error: unsignedLongVal = %lu \n",
                 unsignedLongVal->get_value());
        }
        SgCharVal *charVal = isSgCharVal(valueExp);
        if (charVal != NULL) {
          printf("Error: charVal = %s \n",
                 std::to_string(charVal->get_value()).c_str());
        }
      }

      if (valueExp->get_parent() == NULL) {
        printf("ERROR: valueExp->get_parent() == NULL in AST visualization: "
               "valueExp = %p = %s \n",
               valueExp, valueExp->class_name().c_str());
      }

      // DQ (9/24/2011): Added support to indicate non-printable characters.
      SgCharVal *charVal = isSgCharVal(valueExp);
      if (charVal != NULL) {
        const int64_t value = charVal->get_value();
        const bool alphanumeric =
            value >= 0 && value <= std::numeric_limits<unsigned char>::max() &&
            isalnum(static_cast<unsigned char>(value));
        labelWithSourceCode += string("\\n alpha/numeric value = ") +
                               (alphanumeric ? "true" : "false") + "  ";
      }

      // DQ (8/13/2014): Added debugging output to support C++11 unicode
      // specific details.
      SgStringVal *stringVal = isSgStringVal(valueExp);
      if (stringVal != NULL) {
        labelWithSourceCode += string("\\n literal_encoding = ") +
                               StringUtility::numberToString(static_cast<int>(
                                   stringVal->get_literal_encoding())) +
                               "  ";
        labelWithSourceCode +=
            string("\\n is_raw_string = ") +
            (stringVal->get_isRawString() ? "true" : "false") + "  ";
      }

      // DQ (10/4/2010): Output the value so that we can provide more
      // information.
      labelWithSourceCode += string("\\n value = ") +
                             valueExp->get_constant_folded_value_as_string() +
                             "  ";
    }

    // DQ (6/5/2011): Added support to output if this is an implicit or explicit
    // cast.
    SgCastExp *castExp = isSgCastExp(node);
    if (castExp != NULL) {
      // DQ (6/5/2011): Output if this is an implicit (compiler generated) or
      // explicit case (non-compiler generated).
      ROSE_ASSERT(castExp->get_startOfConstruct() != NULL);
      labelWithSourceCode +=
          string("\\n owns type defining declaration: ") +
          ((castExp->get_type_defining_declaration() != NULL) ? "true"
                                                              : "false") +
          "  ";
      labelWithSourceCode +=
          string("\\n cast is: ") +
          ((castExp->get_startOfConstruct()->isCompilerGenerated() == true)
               ? "implicit"
               : "explicit") +
          "  ";
    }

    // DQ (2/2/2011): Added support for fortran...
    SgActualArgumentExpression *actualArgumentExpression =
        isSgActualArgumentExpression(node);
    if (actualArgumentExpression != NULL) {
      labelWithSourceCode += string("\\n name = ") +
                             actualArgumentExpression->get_argument_name() +
                             "  ";
    }

    // DQ (4/8/2013): Added support to output if this function is using operator
    // syntax.
    SgFunctionCallExp *functionCallExp = isSgFunctionCallExp(node);
    if (functionCallExp != NULL) {
      // DQ (4/8/2013): Added support to output if this function is using
      // operator syntax.
      labelWithSourceCode +=
          string("\\n call uses operator syntax: ") +
          ((functionCallExp->get_uses_operator_syntax() == true) ? "true"
                                                                 : "false") +
          "  ";
    }

    // DQ (4/26/2013): Added support for marking as compiler generated.
    SgExpression *expression = isSgExpression(node);
    ROSE_ASSERT(expression != NULL);
    if (expression->get_file_info()->isCompilerGenerated() == true) {
      labelWithSourceCode += string("\\n compiler generated ");
    }

    if (expression->get_file_info()->isDefaultArgument() == true) {
      labelWithSourceCode += string("\\n default argument ");
    }

    if (expression->get_file_info()->isImplicitCast() == true) {
      labelWithSourceCode += string("\\n implicit cast ");

      // Make sure this is a cast sine otherwise this setting makes no sense.
      ROSE_ASSERT(isSgCastExp(node) != NULL);
    }

    AST_NODE_ID id = TransformationTracking::getId(node);
    if (id != 0)
      labelWithSourceCode =
          string("\\n  ID: ") + StringUtility::numberToString(id) + "  ";
    NodeType graphNode(node, labelWithSourceCode, additionalNodeOptions);
    addNode(graphNode);
  }

  if (isSgType(node) != NULL) {
    SgType *type = isSgType(node);
    ROSE_ASSERT(type != NULL);

    string additionalNodeOptions =
        "shape=polygon,regular=0,URL=\"\\N\",tooltip=\"more info at "
        "\\N\",sides=3,peripheries=1,color=\"blue\",fillcolor=yellow,fontname="
        "\"7x13bold\",fontcolor=black,style=filled";

    string labelWithSourceCode;

    SgTemplateType *templateType = isSgTemplateType(node);
    if (templateType != NULL) {
      labelWithSourceCode +=
          string("\\n  name = ") + templateType->get_name().str() + "  " +
          "position = " +
          StringUtility::numberToString(
              templateType->get_template_parameter_position()) +
          " ";
    }

    // DQ (2/28/2018): Added name to the graph node for all SgNamedType IR
    // nodes.
    SgNamedType *namedType = isSgNamedType(node);
    if (namedType != NULL) {
      labelWithSourceCode += string("\\n") + namedType->get_name().str();
    }

    SgModifierType *modifierType = isSgModifierType(node);
    if (modifierType != NULL) {
      SgTypeModifier tm = modifierType->get_typeModifier();
      if (tm.isRestrict())
        labelWithSourceCode += string("\\n restrict  ");
    }

    labelWithSourceCode +=
        string("\\n  ") + StringUtility::numberToString(node) + "  ";

    // DQ (5/4/2013): Added to make the formatting of the type information
    // better in the graph node.
    labelWithSourceCode += string("\\n   ");

    SgModifierType *mod_type = isSgModifierType(node);
    if (mod_type != NULL) {
      if (mod_type->get_typeModifier().get_constVolatileModifier().isConst() ==
          true) {
        labelWithSourceCode += string("\\n const ");
      }

      if (mod_type->get_typeModifier()
              .get_elaboratedTypeModifier()
              .get_modifier() != SgElaboratedTypeModifier::e_default) {
        stringstream ss;
        ss << mod_type->get_typeModifier()
                  .get_elaboratedTypeModifier()
                  .get_modifier();
        labelWithSourceCode +=
            string("\\n type modifier enum value = " + ss.str() + " ");
      }
    }

    // DQ (4/22/2014): Added to make the formatting of the type information
    // better in the graph node.
    labelWithSourceCode += string("\\n   ");

    AST_NODE_ID id = TransformationTracking::getId(node);
    if (id != 0)
      labelWithSourceCode =
          string("\\n  ID: ") + StringUtility::numberToString(id) + "  ";
    NodeType graphNode(node, labelWithSourceCode, additionalNodeOptions);
    addNode(graphNode);
  }

  SgSupport *supportNode = isSgSupport(node);
  if (supportNode != NULL) {
    string additionalNodeOptions;

    // Make this statement different in the generated dot graph
    string labelWithSourceCode;

    switch (supportNode->variantT()) {
    case V_SgProject: {
      SgProject *project = isSgProject(node);
      additionalNodeOptions =
          "shape=ellipse,regular=0,URL=\"\\N\",tooltip=\"more info at "
          "\\N\",sides=5,peripheries=3,color=\"blue\",fillcolor=pink,fontname="
          "\"7x13bold\",fontcolor=black,style=filled";
      labelWithSourceCode =
          string("\\n  ") + project->get_outputFileName() + "  ";
      labelWithSourceCode +=
          string("\\n  ") + StringUtility::numberToString(project) + "  ";
      break;
    }

    case V_SgSourceFile: {
      SgFile *file = isSgFile(node);
      additionalNodeOptions =
          "shape=ellipse,regular=0,URL=\"\\N\",tooltip=\"more info at "
          "\\N\",sides=5,peripheries=2,color=\"blue\",fillcolor=pink,fontname="
          "\"7x13bold\",fontcolor=black,style=filled";
      labelWithSourceCode =
          string("\\n  getFileName = ") + file->getFileName() + "  ";
      labelWithSourceCode += string("\\n  sourceFileNameWithPath = ") +
                             file->get_sourceFileNameWithPath() + "  ";

      labelWithSourceCode +=
          string("\\n  ") + StringUtility::numberToString(file) + "  ";
      ROSE_ASSERT(SgNode::get_globalFunctionTypeTable() != NULL);
      break;
    }

      // DQ (5/5/2020): Adding AST visualization support for SgIncludeFile.
    case V_SgIncludeFile: {
      SgIncludeFile *include_file = isSgIncludeFile(node);
      additionalNodeOptions =
          "shape=ellipse,regular=0,URL=\"\\N\",tooltip=\"more info at "
          "\\N\",sides=5,peripheries=2,color=\"blue\",fillcolor=green,fontname="
          "\"7x13bold\",fontcolor=black,style=filled";

      // DQ (5/6/2020): Trigger the root fo the include tree associated with the
      // input source file to be collored differently.
      if (include_file->get_isRootSourceFile() == true) {
        additionalNodeOptions =
            "shape=ellipse,regular=0,URL=\"\\N\",tooltip=\"more info at "
            "\\N\",sides=5,peripheries=2,color=\"blue\",fillcolor=green,"
            "fontname=\"7x13bold\",fontcolor=black,style=filled";
        labelWithSourceCode = string("INCLUDE TREE ROOT\\n");
      }

      labelWithSourceCode +=
          string("\\n  ") + include_file->get_filename() + "  ";
      labelWithSourceCode +=
          string("\\n  ") + StringUtility::numberToString(include_file) + "  ";
      break;
    }

    case V_Sg_File_Info: {
      Sg_File_Info *currentNodeFileInfo = isSg_File_Info(node);
      if (currentNodeFileInfo != NULL &&
          !currentNodeFileInfo->isFrontendSpecific()) {
        additionalNodeOptions =
            "shape=polygon,regular=0,URL=\"\\N\",tooltip=\"more info at "
            "\\N\",sides=6,peripheries=1,color=\"blue\",fillcolor=brown,"
            "fontname=\"7x13bold\",fontcolor=black,style=filled";
        // Make this statement different in the generated dot graph
        labelWithSourceCode =
            "\\n" + currentNodeFileInfo->get_filenameString() + "\\n line: " +
            StringUtility::numberToString(currentNodeFileInfo->get_line()) +
            " column: " +
            StringUtility::numberToString(currentNodeFileInfo->get_col()) +
            "  " + "\\n raw line: " +
            StringUtility::numberToString(currentNodeFileInfo->get_raw_line()) +
            " raw column: " +
            StringUtility::numberToString(currentNodeFileInfo->get_raw_col()) +
            "  " +
            // DQ (1/16/2012): Added more information to the output of the
            // source poisition information.
            "\\n hasPositionInSource() = " +
            (currentNodeFileInfo->hasPositionInSource() == true ? "T" : "F") +
            " isTransformation() = " +
            (currentNodeFileInfo->isTransformation() == true ? "T" : "F") +
            "  " + " isCompilerGenerated() = " +
            (currentNodeFileInfo->isCompilerGenerated() == true ? "T" : "F") +
            "  " + "\\n isOutputInCodeGeneration() = " +
            (currentNodeFileInfo->isOutputInCodeGeneration() == true ? "T"
                                                                     : "F") +
            "  " + " isShared() = " +
            (currentNodeFileInfo->isShared() == true ? "T" : "F") + "  " +
            " isFrontendSpecific() = " +
            (currentNodeFileInfo->isFrontendSpecific() == true ? "T" : "F") +
            "  " + "\\n isSourcePositionUnavailableInFrontend() = " +
            (currentNodeFileInfo->isSourcePositionUnavailableInFrontend() ==
                     true
                 ? "T"
                 : "F") +
            "  " + " isCommentOrDirective() = " +
            (currentNodeFileInfo->isCommentOrDirective() == true ? "T" : "F") +
            "  " + " isToken() = " +
            (currentNodeFileInfo->isToken() == true ? "T" : "F") + "  " +
            "\\n pointer value: " +
            StringUtility::numberToString(currentNodeFileInfo) + "  ";
      }
      break;
    }

    case V_SgSymbolTable: {
      SgSymbolTable *symbolTable = isSgSymbolTable(node);
      additionalNodeOptions =
          "shape=house,regular=0,URL=\"\\N\",tooltip=\"more info at "
          "\\N\",sides=5,peripheries=1,color=\"blue\",fillcolor=orange,"
          "fontname=\"7x13bold\",fontcolor=black,style=filled";
      string symbolTableName = "Unnamed Symbol Table";
      if (symbolTable->get_no_name() == true)
        symbolTableName = symbolTable->get_name().getString();
      labelWithSourceCode = string("\\n  ") + symbolTableName +
                            string("\\n  ") +
                            StringUtility::numberToString(symbolTable) + "  ";
      break;
    }

    case V_SgTemplateArgument: {
      SgTemplateArgument *templateArgument = isSgTemplateArgument(node);
      additionalNodeOptions =
          "shape=circle,regular=0,URL=\"\\N\",tooltip=\"more info at "
          "\\N\",sides=5,peripheries=1,color=\"blue\",fillcolor=yellow,"
          "fontname=\"7x13bold\",fontcolor=black,style=filled";
      // string typeString  = (templateArgument->get_argumentType() ==
      // SgTemplateArgument::argument_undefined) ? "argument_undefined" :
      // "!isForward";
      string typeString;
      switch (templateArgument->get_argumentType()) {
      case SgTemplateArgument::argument_undefined:
        typeString = "argument_undefined";
        break;

      case SgTemplateArgument::type_argument:
        typeString = "type_argument";

        // A memory-pool graph has no lexical source use site, so a source
        // spelling (and therefore name qualification) is undefined here.
        // Report the exact semantic type identity instead of invoking the
        // source unparser without a qualification context.
        ASSERT_not_null(templateArgument->get_type());
        typeString += string("\\n type class = ") +
                      templateArgument->get_type()->class_name();
        typeString += string("\\n type identity = ") +
                      templateArgument->get_type()->get_mangled().str();
        typeString +=
            string("\\n type = ") +
            StringUtility::numberToString(templateArgument->get_type()) + "  ";
        break;

      case SgTemplateArgument::nontype_argument:
        typeString = "nontype_argument";

        // DQ (8/25/2012): Fixed bug to output the value of the tepression
        // pointer.
        typeString +=
            string("\\n expression = ") +
            StringUtility::numberToString(templateArgument->get_expression()) +
            "  ";
        break;

      case SgTemplateArgument::template_template_argument:
        typeString = "template_template_argument";
        break;

      case SgTemplateArgument::start_of_pack_expansion_argument:
        typeString = "start_of_pack_expansion_argument";
        break;

      default: {
        printf("Error: default reached in case V_SgTemplateArgument: "
               "templateArgument->get_argumentType() \n");
        ROSE_ABORT();
      }
      }

      // DQ (1/21/2018): Add debugging information to output in each graph node.
      typeString +=
          string("\\n explicitlySpecified = ") +
          ((templateArgument->get_explicitlySpecified() == true) ? "true"
                                                                 : "false") +
          "  ";

      labelWithSourceCode = string("\\n  ") + typeString + string("\\n  ") +
                            StringUtility::numberToString(templateArgument) +
                            "  ";
      break;
    }

      // DQ (9/21/2012): Added visualization support for template parameters.
    case V_SgTemplateParameter: {
      SgTemplateParameter *templateParameter = isSgTemplateParameter(node);
      additionalNodeOptions =
          "shape=circle,regular=0,URL=\"\\N\",tooltip=\"more info at "
          "\\N\",sides=5,peripheries=1,color=\"blue\",fillcolor=blue,fontname="
          "\"7x13bold\",fontcolor=black,style=filled";

      string typeString;
      switch (templateParameter->get_parameterType()) {
      case SgTemplateParameter::parameter_undefined:
        typeString = "parameter_undefined";
        break;

      case SgTemplateParameter::type_parameter:
        typeString = "type_parameter";
        ASSERT_not_null(templateParameter->get_type());
        typeString += string("\\n type class = ") +
                      templateParameter->get_type()->class_name();
        typeString += string("\\n type identity = ") +
                      templateParameter->get_type()->get_mangled().str();
        typeString +=
            string("\\n type = ") +
            StringUtility::numberToString(templateParameter->get_type()) + "  ";
        break;

      case SgTemplateParameter::nontype_parameter:
        typeString = "nontype_parameter";
        break;

      case SgTemplateParameter::template_parameter:
        typeString = "template_parameter";
        break;

      default: {
        printf("Error: default reached in case V_SgTemplateParameter: "
               "templateParameter->get_argumentType() = %d \n",
               templateParameter->get_parameterType());
        ROSE_ABORT();
      }
      }

      labelWithSourceCode = string("\\n  ") + typeString + string("\\n  ") +
                            StringUtility::numberToString(templateParameter) +
                            "  ";
      break;
    }

    case V_SgBaseClass: {
      SgBaseClass *baseClass = isSgBaseClass(node);
      additionalNodeOptions =
          "shape=polygon,regular=0,URL=\"\\N\",tooltip=\"more info at "
          "\\N\",sides=8,peripheries=1,color=\"blue\",fillcolor=greenyellow,"
          "fontname=\"7x13bold\",fontcolor=black,style=filled";
      labelWithSourceCode =
          string("\\n  ") + StringUtility::numberToString(baseClass) + "  ";
      break;
    }

      // DQ (12/6/2012): Added support for SgFunctionParameterTypeList.
    case V_SgFunctionParameterTypeList: {
      SgFunctionParameterTypeList *functionParameterTypeList =
          isSgFunctionParameterTypeList(node);
      additionalNodeOptions =
          "shape=house,regular=0,URL=\"\\N\",tooltip=\"more info at "
          "\\N\",sides=5,peripheries=1,color=\"blue\",fillcolor=blue,fontname="
          "\"7x13bold\",fontcolor=black,style=filled";
      labelWithSourceCode =
          string("\\n  ") +
          StringUtility::numberToString(
              functionParameterTypeList->get_arguments().size()) +
          "  ";
      labelWithSourceCode +=
          string("\\n  ") +
          StringUtility::numberToString(functionParameterTypeList) + "  ";
      break;
    }

    default: {
      additionalNodeOptions =
          "shape=polygon,regular=0,URL=\"\\N\",tooltip=\"more info at "
          "\\N\",sides=4,peripheries=1,color=\"blue\",fillcolor=red,fontname="
          "\"7x13bold\",fontcolor=black,style=filled";
      break;
    }
    }

    AST_NODE_ID id = TransformationTracking::getId(node);
    if (id != 0) {
      labelWithSourceCode =
          string("\\n  ID: ") + StringUtility::numberToString(id) + "  ";
    }

    NodeType graphNode(node, labelWithSourceCode, additionalNodeOptions);
    addNode(graphNode);
  }

  if (isSgInitializedName(node) != NULL) {
    SgInitializedName *initializedName = isSgInitializedName(node);
    string additionalNodeOptions =
        "shape=house,regular=0,URL=\"\\N\",tooltip=\"more info at "
        "\\N\",sides=5,peripheries=1,color=\"blue\",fillcolor=darkturquoise,"
        "fontname=\"7x13bold\",fontcolor=black,style=filled";
    string labelWithSourceCode =
        string("\\n  ") + initializedName->get_name().getString() +
        string("\\n  ") + StringUtility::numberToString(initializedName) + "  ";

    AST_NODE_ID id = TransformationTracking::getId(node);
    if (id != 0)
      labelWithSourceCode =
          string("\\n  ID: ") + StringUtility::numberToString(id) + "  ";
    NodeType graphNode(node, labelWithSourceCode, additionalNodeOptions);
    addNode(graphNode);
  }

  if (isSgSymbol(node) != NULL) {
    string additionalNodeOptions =
        "shape=polygon,regular=0,URL=\"\\N\",tooltip=\"more info at "
        "\\N\",sides=8,peripheries=1,color=\"blue\",fillcolor=purple,fontname="
        "\"7x13bold\",fontcolor=black,style=filled";
    string labelWithSourceCode;

    SgSymbol *symbol = isSgSymbol(node);
    // DQ (2/28/2018): Added debugging code to make symbols in the graphs more
    // clear.
    SgAliasSymbol *aliasSymbol = isSgAliasSymbol(node);
    if (aliasSymbol != NULL) {
      labelWithSourceCode +=
          string("\\n alias to: ") + aliasSymbol->get_base()->class_name();
    }

    labelWithSourceCode += string("\\n name: ") + symbol->get_name();
    labelWithSourceCode +=
        string("\\n  ") + StringUtility::numberToString(node) + "  ";

    AST_NODE_ID id = TransformationTracking::getId(node);
    if (id != 0)
      labelWithSourceCode =
          string("\\n  ID: ") + StringUtility::numberToString(id) + "  ";
    NodeType graphNode(node, labelWithSourceCode, additionalNodeOptions);
    addNode(graphNode);
  }
}

/* Initialize the filters for the default case */
CustomMemoryPoolDOTGeneration::s_Filter_Flags::s_Filter_Flags() {
  setDefault();
}

void CustomMemoryPoolDOTGeneration::s_Filter_Flags::setDefault() {
  // Initial values for the filters
  m_commentAndDirective = 1; /* commentAndDirectiveFilter()*/
  m_ctorInitializer = 0;     /*ctorInitializerListFilter()*/

  m_default = 1;          /* defaultFilter ()*/
  m_defaultColor = 1;     /*defaultColorFilter()*/
  m_edge = 1;             /* edgeFilter ()*/
  m_emptySymbolTable = 0; /*emptySymbolTableFilter()*/

  // DQ (7/22/2012): Ignore some empty IR nodes.
  m_emptyBasicBlock = 0;            /*emptyBasicBlockFilter()*/
  m_emptyFunctionParameterList = 0; /*emptyFunctionParameterListFilter()*/

  m_expression = 0; /* expressionFilter ()*/

  m_fileInfo = 1; /* fileInfoFilter ()*/

  m_frontendCompatibility = 0; /* frontendCompatibilityFilter()*/

  m_symbol = 0;              /*symbolFilter ()*/
  m_type = 0;                /* typeFilter ()*/
  m_variableDeclaration = 0; /*variableDeclarationFilter()*/

  m_variableDefinition = 0; /*variableDefinitionFilter()*/

  // This is the normal (non-debugging) mode.
  m_noFilter = 0; /* no filtering */
}

void CustomMemoryPoolDOTGeneration::s_Filter_Flags::print_filter_flags() {
  printf("Current filter flags' values are: \n");

  printf("\t m_commentAndDirective = %d \n", m_commentAndDirective);
  printf("\t m_ctorInitializer = %d \n", m_ctorInitializer);

  printf("\t m_default = %d \n", m_default);
  printf("\t m_defaultColor = %d \n", m_defaultColor);
  printf("\t m_edge = %d \n", m_edge);
  printf("\t m_emptySymbolTable = %d \n", m_emptySymbolTable);
  printf("\t m_expression = %d \n", m_expression);

  printf("\t m_fileInfo = %d \n", m_fileInfo);
  printf("\t m_frontendCompatibility = %d \n", m_frontendCompatibility);
  printf("\t m_symbol = %d \n", m_symbol);
  printf("\t m_type = %d \n", m_type);
  printf("\t m_variableDeclaration = %d \n", m_variableDeclaration);

  printf("\t m_variableDefinition = %d \n", m_variableDefinition);

  printf("\t m_noFilter = %d \n", m_noFilter);
}

/* Construct an instance from */
CustomMemoryPoolDOTGeneration::s_Filter_Flags::s_Filter_Flags(
    std::vector<std::string> &argvList) {
  setDefault(); // set default options first

  // stop here if no arguments are specified at all
  if (argvList.size() == 0)
    return;

  CommandlineProcessing::isOptionWithParameter(
      argvList, "-rose:dotgraph:", "commentAndDirectiveFilter",
      m_commentAndDirective, true);
  CommandlineProcessing::isOptionWithParameter(
      argvList, "-rose:dotgraph:", "ctorInitializerListFilter",
      m_ctorInitializer, true);

  CommandlineProcessing::isOptionWithParameter(
      argvList, "-rose:dotgraph:", "defaultColorFilter", m_defaultColor, true);
  CommandlineProcessing::isOptionWithParameter(
      argvList, "-rose:dotgraph:", "defaultFilter", m_default, true);
  CommandlineProcessing::isOptionWithParameter(
      argvList, "-rose:dotgraph:", "edgeFilter", m_edge, true);

  CommandlineProcessing::isOptionWithParameter(
      argvList, "-rose:dotgraph:", "emptySymbolTableFilter", m_emptySymbolTable,
      true);

  // DQ (7/22/2012): Added support to ignore some empty IR nodes.
  CommandlineProcessing::isOptionWithParameter(
      argvList, "-rose:dotgraph:", "emptyBasicBlockFilter", m_emptyBasicBlock,
      true);
  CommandlineProcessing::isOptionWithParameter(
      argvList, "-rose:dotgraph:", "emptyFunctionParameterListFilter",
      m_emptyFunctionParameterList, true);

  CommandlineProcessing::isOptionWithParameter(
      argvList, "-rose:dotgraph:", "expressionFilter", m_expression, true);

  CommandlineProcessing::isOptionWithParameter(
      argvList, "-rose:dotgraph:", "fileInfoFilter", m_fileInfo, true);
  CommandlineProcessing::isOptionWithParameter(
      argvList, "-rose:dotgraph:", "frontendCompatibilityFilter",
      m_frontendCompatibility, true);
  CommandlineProcessing::isOptionWithParameter(
      argvList, "-rose:dotgraph:", "symbolFilter", m_symbol, true);
  CommandlineProcessing::isOptionWithParameter(
      argvList, "-rose:dotgraph:", "typeFilter", m_type, true);
  CommandlineProcessing::isOptionWithParameter(
      argvList, "-rose:dotgraph:", "variableDeclarationFilter",
      m_variableDeclaration, true);

  CommandlineProcessing::isOptionWithParameter(
      argvList, "-rose:dotgraph:", "variableDefinitionFilter",
      m_variableDefinition, true);

  CommandlineProcessing::isOptionWithParameter(
      argvList, "-rose:dotgraph:", "noFilter", m_noFilter, true);

  if (CommandlineProcessing::isOption(argvList, "-rose:", "help", false) ||
      CommandlineProcessing::isOption(argvList, "-help", "", false) ||
      CommandlineProcessing::isOption(argvList, "--help", "", false)) {
    print_commandline_help();
    print_filter_flags();
  }
}

void CustomMemoryPoolDOTGeneration::s_Filter_Flags::print_commandline_help() {
  cout << "   -rose:help                     show this help message" << endl;

  cout << "   -rose:dotgraph:commentAndDirectiveFilter        [0|1]  Disable "
          "or enable commentAndDirective filter"
       << endl;
  cout << "   -rose:dotgraph:ctorInitializerListFilter        [0|1]  Disable "
          "or enable ctorInitializerList filter"
       << endl;

  cout << "   -rose:dotgraph:defaultFilter                    [0|1]  Disable "
          "or enable default filter"
       << endl;
  cout << "   -rose:dotgraph:defaultColorFilter               [0|1]  Disable "
          "or enable defaultColor filter"
       << endl;
  cout << "   -rose:dotgraph:edgeFilter                       [0|1]  Disable "
          "or enable edge filter"
       << endl;
  cout << "   -rose:dotgraph:expressionFilter                 [0|1]  Disable "
          "or enable expression filter"
       << endl;
  cout << "   -rose:dotgraph:fileInfoFilter                   [0|1]  Disable "
          "or enable fileInfo filter"
       << endl;

  cout << "   -rose:dotgraph:frontendCompatibilityFilter      [0|1]  Disable "
          "or enable frontendCompatibility filter"
       << endl;
  cout << "   -rose:dotgraph:symbolFilter                     [0|1]  Disable "
          "or enable symbol filter"
       << endl;

  cout << "   -rose:dotgraph:emptySymbolTableFilter           [0|1]  Disable "
          "or enable emptySymbolTable filter"
       << endl;
  cout << "   -rose:dotgraph:emptyFunctionParameterListFilter [0|1]  Disable "
          "or enable emptyFunctionParameterList filter"
       << endl;
  cout << "   -rose:dotgraph:emptyBasicBlockFilter            [0|1]  Disable "
          "or enable emptyBasicBlock filter"
       << endl;

  cout << "   -rose:dotgraph:typeFilter                       [0|1]  Disable "
          "or enable type filter"
       << endl;
  cout << "   -rose:dotgraph:variableDeclarationFilter        [0|1]  Disable "
          "or enable variableDeclaration filter"
       << endl;

  cout << "   -rose:dotgraph:variableDefinitionFilter         [0|1]  Disable "
          "or enable variableDefinitionFilter filter"
       << endl;

  cout << "   -rose:dotgraph:noFilter                         [0|1]  Disable "
          "or enable no filtering"
       << endl;
  cout << "   -DSKIP_ROSE_BUILTIN_DECLARATIONS                Enable builtin "
          "function filtering, defaut is to show all of them"
       << endl;
}

class SimpleColorMemoryPoolTraversal : public CustomMemoryPoolDOTGeneration,
                                       public ROSE_VisitTraversal {
public:
  static const set<SgNode *> defaultSetOfIRnodes;
  const set<SgNode *> &setOfIRnodes;

  //! Required traversal function
  void visit(SgNode *node);

  static void
  generateGraph(string filename,
                const set<SgNode *> &firstAST = defaultSetOfIRnodes,
                CustomMemoryPoolDOTGeneration::s_Filter_Flags *flags = NULL);

  void buildExcludeList();

  SimpleColorMemoryPoolTraversal(
      const set<SgNode *> &s = defaultSetOfIRnodes,
      CustomMemoryPoolDOTGeneration::s_Filter_Flags *flags = NULL)
      : CustomMemoryPoolDOTGeneration(flags), setOfIRnodes(s) {};

  // Destructor defined because base class has virtual members
  virtual ~SimpleColorMemoryPoolTraversal();
};

const set<SgNode *> SimpleColorMemoryPoolTraversal::defaultSetOfIRnodes;

SimpleColorMemoryPoolTraversal::~SimpleColorMemoryPoolTraversal() {}

void SimpleColorMemoryPoolTraversal::generateGraph(
    string filename, const set<SgNode *> &firstAST,
    CustomMemoryPoolDOTGeneration::s_Filter_Flags *flags /*= NULL*/) {
  // DQ (2/2/2007): Introduce tracking of performance of within AST merge
  std::string label =
      "SimpleColorMemoryPoolTraversal::generateGraph(" + filename + "):";
  TimingPerformance timer(label);

  // Custom control over the coloring of the "whole" AST for a memory pool
  // traversal
  SimpleColorMemoryPoolTraversal traversal(firstAST, flags);
  traversal.traverseMemoryPool();

  traversal.buildExcludeList();

  traversal.internalGenerateGraph(filename);
}

void SimpleColorMemoryPoolTraversal::buildExcludeList() {
  for (SgNode *node : setOfIRnodes)
    skipNode(node);
}

void SimpleColorMemoryPoolTraversal::visit(SgNode *node) {
  ROSE_ASSERT(filterFlags != NULL);

  // DQ (3/2/2010): Test if we have turned off all filtering of the AST.
  if (filterFlags->m_noFilter == 0) {
    // We allow filitering of the AST.

    if (filterFlags->m_default == 1) {
      defaultFilter(node);
    }

    // DQ (3/1/2009): Uncommented to allow filtering of types.
    if (filterFlags->m_type == 1) {
      typeFilter(node);
    }

    // DQ (3/2/2009): Remove some more nodes to make the graphs more clear.
    if (filterFlags->m_expression == 1) {
      expressionFilter(node);
    }

    if (filterFlags->m_emptySymbolTable == 1) {
      emptySymbolTableFilter(node);
    }

    // DQ (7/22/2012): Added support to ignore some empty IR nodes.
    if (filterFlags->m_emptyBasicBlock == 1) {
      emptyBasicBlockFilter(node);
    }

    if (filterFlags->m_emptyFunctionParameterList == 1) {
      emptyFunctionParameterListFilter(node);
    }
    if (filterFlags->m_variableDefinition == 1) {
      variableDefinitionFilter(node);
    }

    if (filterFlags->m_variableDeclaration == 1) {
      variableDeclarationFilter(node);
    }

    if (filterFlags->m_ctorInitializer == 1) {
      ctorInitializerListFilter(node);
    }
    if (filterFlags->m_symbol == 1) {
      symbolFilter(node);
    }

    // Control output of Sg_File_Info object in graph of whole AST.
    if (filterFlags->m_fileInfo == 1) {
      fileInfoFilter(node);
    }
  }

  if (filterFlags->m_defaultColor == 1) {
    defaultColorFilter(node);
  }

  // Color this IR node differently if it in NOT in the original AST
  if (setOfIRnodes.find(node) == setOfIRnodes.end()) {
    string additionalNodeOptions =
        "shape=polygon,regular=0,URL=\"\\N\",tooltip=\"more info at "
        "\\N\",sides=6,peripheries=1,color=\"blue\",fillcolor=greenyellow,"
        "fontname=\"7x13bold\",fontcolor=black,style=filled";
    // Make this statement different in the generated dot graph

    string labelWithSourceCode =
        "\\n" + StringUtility::numberToString(node) + "  ";

    NodeType graphNode(node, labelWithSourceCode, additionalNodeOptions);
    addNode(graphNode);
  }
}

// Build an inherited attribute for the tree traversal to test the rewrite
// mechanism
class SimpleColorFilesInheritedAttribute {
public:
  //! Max number of different types of nodes that we define
  static const int maxSize;
  static const std::string additionalNodeOptionsArray[5];

  // Current options to use with DOT visualizations
  string currentNodeOptions;

  // True if subtree is shared
  bool sharedSubTree;

  void setNodeOptions(SgFile *file);

  // Default constructor
  SimpleColorFilesInheritedAttribute() : sharedSubTree(false) {
    ROSE_ASSERT(currentNodeOptions.size() < 4000);
  }

  //! Specific constructors are required
  //  SimpleColorFilesInheritedAttribute () {};
  //  SimpleColorFilesInheritedAttribute ( const
  //  SimpleColorFilesInheritedAttribute & X ) {};
};

const int SimpleColorFilesInheritedAttribute::maxSize = 5;

// colors that will be represented as "black": azure1
const string SimpleColorFilesInheritedAttribute::additionalNodeOptionsArray
    [SimpleColorFilesInheritedAttribute::maxSize] = {
        "shape=ellipse,regular=0,URL=\"\\N\",tooltip=\"more info at "
        "\\N\",sides=6,peripheries=1,color=\"blue\",fillcolor=blue1,fontname="
        "\"7x13bold\",fontcolor=black,style=filled",
        "shape=ellipse,regular=0,URL=\"\\N\",tooltip=\"more info at "
        "\\N\",sides=6,peripheries=1,color=\"blue\",fillcolor=greenyellow,"
        "fontname=\"7x13bold\",fontcolor=black,style=filled",
        "shape=ellipse,regular=0,URL=\"\\N\",tooltip=\"more info at "
        "\\N\",sides=6,peripheries=1,color=\"blue\",fillcolor=red,fontname="
        "\"7x13bold\",fontcolor=black,style=filled",
        "shape=ellipse,regular=0,URL=\"\\N\",tooltip=\"more info at "
        "\\N\",sides=6,peripheries=1,color=\"blue\",fillcolor=gold1,fontname="
        "\"7x13bold\",fontcolor=black,style=filled",
        "shape=ellipse,regular=0,URL=\"\\N\",tooltip=\"more info at "
        "\\N\",sides=6,peripheries=1,color=\"blue\",fillcolor=green,fontname="
        "\"7x13bold\",fontcolor=black,style=filled"};

void SimpleColorFilesInheritedAttribute::setNodeOptions(SgFile *file) {
  ROSE_ASSERT(file->get_file_info() != NULL);

  // DQ (3/22/2007): file_id's can be negative for defalut generated files
  // (NULL_FILE, Transforamtions, etc.)
  int file_id = file->get_file_info()->get_file_id();
  int index = 0;
  if (file_id > 0) {
    // Select from a small subset of optional setting strings.
    index = (file->get_file_info()->get_file_id() + 0) % maxSize;
  }
  currentNodeOptions = additionalNodeOptionsArray[index];
  ROSE_ASSERT(currentNodeOptions.size() < 4000);
}

class SimpleColorFilesTraversal
    : public CustomAstDOTGeneration,
      public SgTopDownProcessing<SimpleColorFilesInheritedAttribute> {
public:
  // Use this as an empty list to support the default argument option to
  // generateGraph
  static set<SgNode *> emptyNodeList;

  // Reference to list that will be colored differently
  set<SgNode *> &specialNodeList;

  //! Required traversal function
  SimpleColorFilesInheritedAttribute evaluateInheritedAttribute(
      SgNode *astNode, SimpleColorFilesInheritedAttribute inheritedAttribute);

  static void generateGraph(SgProject *project, string filename,
                            set<SgNode *> &specialNodeList =
                                SimpleColorFilesTraversal::emptyNodeList);

  // Constructor
  SimpleColorFilesTraversal(set<SgNode *> &s) : specialNodeList(s) {}

  // Destructor defined because base class has virtual members
  virtual ~SimpleColorFilesTraversal() {}
};

// Use this as an empty list to support the default argument option to
// generateGraph
set<SgNode *> SimpleColorFilesTraversal::emptyNodeList;

// Functions required by the tree traversal mechanism
SimpleColorFilesInheritedAttribute
SimpleColorFilesTraversal::evaluateInheritedAttribute(
    SgNode *node, SimpleColorFilesInheritedAttribute inheritedAttribute) {
  ROSE_ASSERT(node != NULL);
  ROSE_ASSERT(inheritedAttribute.currentNodeOptions.size() < 4000);

  SgProject *project = isSgProject(node);
  if (project != NULL) {
    inheritedAttribute.currentNodeOptions =
        "shape=circle,regular=0,URL=\"\\N\",tooltip=\"more info at "
        "\\N\",sides=6,peripheries=3,color=\"blue\",fillcolor=cyan4,fontname="
        "\"12x24bold\",fontcolor=black,style=filled";
  }

  ROSE_ASSERT(inheritedAttribute.currentNodeOptions.size() < 4000);
  SgFile *file = isSgFile(node);
  if (file != NULL) {
    inheritedAttribute.setNodeOptions(file);
  }

  ROSE_ASSERT(inheritedAttribute.currentNodeOptions.size() < 4000);
  Sg_File_Info *fileInfo = node->get_file_info();
  if (fileInfo != NULL) {
    if (fileInfo->isShared() == true)
      inheritedAttribute.sharedSubTree = true;
  }

  ROSE_ASSERT(inheritedAttribute.currentNodeOptions.size() < 4000);
  if (inheritedAttribute.sharedSubTree == true) {
    inheritedAttribute.currentNodeOptions =
        "shape=diamond,regular=0,URL=\"\\N\",tooltip=\"more info at "
        "\\N\",sides=6,peripheries=1,color=\"blue\",fillcolor=magenta1,"
        "fontname=\"12x24bold\",fontcolor=black,style=filled";
  }
  ROSE_ASSERT(inheritedAttribute.currentNodeOptions.size() < 4000);

  string additionalNodeOptions = inheritedAttribute.currentNodeOptions;
  string labelWithSourceCode;

  SgClassDeclaration *classDeclaration = isSgClassDeclaration(node);
  if (classDeclaration != NULL) {
    labelWithSourceCode = "\\n" + classDeclaration->get_name().getString() +
                          "\\n" + StringUtility::numberToString(node) + "  ";
  } else {
    SgFunctionDeclaration *functionDeclaration = isSgFunctionDeclaration(node);
    if (functionDeclaration != NULL) {
      labelWithSourceCode = "\\n" +
                            functionDeclaration->get_name().getString() +
                            "\\n" + StringUtility::numberToString(node) + "  ";
    } else {
      labelWithSourceCode = "\\n" + StringUtility::numberToString(node) + "  ";
    }
  }
  ROSE_ASSERT(inheritedAttribute.currentNodeOptions.size() < 4000);

  // Check if this is an IR node that is to be handled specially
  if (specialNodeList.find(node) != specialNodeList.end()) {
    inheritedAttribute.currentNodeOptions =
        "shape=circle,regular=0,URL=\"\\N\",tooltip=\"more info at "
        "\\N\",sides=6,peripheries=3,color=\"blue\",fillcolor=black,fontname="
        "\"12x24bold\",fontcolor=white,style=filled";
    ROSE_ASSERT(inheritedAttribute.currentNodeOptions.size() < 4000);
  }
  ROSE_ASSERT(inheritedAttribute.currentNodeOptions.size() < 4000);

  additionalNodeOptions = inheritedAttribute.currentNodeOptions;
  ROSE_ASSERT(additionalNodeOptions.size() < 4000);

  NodeType graphNode(node, labelWithSourceCode, additionalNodeOptions);
  ROSE_ASSERT(graphNode.optionString.size() < 4000);
  addNode(graphNode);

  return inheritedAttribute;
}

void SimpleColorFilesTraversal::generateGraph(SgProject *project,
                                              string filename,
                                              set<SgNode *> &specialNodeList) {
  // DQ (2/2/2007): Introduce tracking of performance of within AST merge
  std::string label =
      "SimpleColorFilesTraversal::generateGraph(" + filename + "):";
  TimingPerformance timer(label);

  // Custom control over the coloring of the "whole" AST for a memory pool
  // traversal
  SimpleColorFilesTraversal traversal(specialNodeList);
  SimpleColorFilesInheritedAttribute ih;
  traversal.traverse(project, ih);

  traversal.internalGenerateGraph(filename, project);
}

#include "wholeAST_API.h"

void generateWholeGraphOfAST(
    std::string filename, std::set<SgNode *> &skippedNodeSet,
    CustomMemoryPoolDOTGeneration::s_Filter_Flags *flags) {
  SimpleColorMemoryPoolTraversal::generateGraph(filename, skippedNodeSet,
                                                flags);
}

void generateWholeGraphOfAST(string filename, set<SgNode *> &skippedNodeSet) {
  SimpleColorMemoryPoolTraversal::generateGraph(filename, skippedNodeSet);
}

// Facilitate gdb to call this function
void generateWholeGraphOfAST(const char *filename) {
  generateWholeGraphOfAST(std::string(filename), NULL);
}

void generateWholeGraphOfAST(string filename) {
  generateWholeGraphOfAST(filename, NULL);
}

void generateWholeGraphOfAST(
    string filename,
    CustomMemoryPoolDOTGeneration::s_Filter_Flags *flags /*= NULL*/) {
  // Make this the default type of graph that we produce (filtering frontend
  // specific IR nodes)
  std::unique_ptr<CustomMemoryPoolDOTGeneration::s_Filter_Flags> owned_flags;
  if (flags == NULL) {
    owned_flags =
        std::make_unique<CustomMemoryPoolDOTGeneration::s_Filter_Flags>();
    flags = owned_flags.get();
  }

  std::set<SgNode *> skippedNodeSet = SageInterface::getFrontendSpecificNodes();
  SimpleColorMemoryPoolTraversal::generateGraph(filename, skippedNodeSet,
                                                flags);
}

void graphNodesAfterCopy(const set<SgNode *> &oldNodes, string filename) {
  auto skippedNodes = SageInterface::getFrontendSpecificNodes();
  SimpleColorMemoryPoolTraversal traversal(skippedNodes);
  traversal.traverseMemoryPool();
  traversal.buildExcludeList();
  traversal.DOTgraph.traverseMemoryPool();

  const auto outputFilename = Rose::TestOutput::resolvePath(filename + ".dot");
  ofstream output(outputFilename);
  if (!output)
    throw runtime_error("Cannot open AST copy graph: " + outputFilename);

  output << "digraph copyGraph {\n";
  traversal.DOTgraph.dotrep.writeDotoutStream(output);
  VariantVector variants(V_SgNode);
  const auto nodes = NodeQuery::queryMemoryPool(variants);
  for (bool original : {true, false}) {
    output << "subgraph " << (original ? "clusterOld" : "clusterNew")
           << " {\nlabel=\"" << (original ? "Old nodes" : "New nodes")
           << "\"; fillcolor=" << (original ? "lightgreen" : "lightyellow")
           << "; style=filled; fontsize=36;\n";
    for (SgNode *node : nodes) {
      if (traversal.DOTgraph.skipNodeList.count(node) != 0 ||
          (oldNodes.count(node) != 0) != original)
        continue;
      output << '"' << node << "\";\n";
      for (const auto &[target, label] : node->returnDataMemberPointers()) {
        if (target == nullptr &&
            traversal.DOTgraph.skipEdgeSet.count(
                CustomMemoryPoolDOTGenerationData::EdgeType(node, target,
                                                            label)) == 0)
          output << '"' << node << "__" << escape_double_quotes(label)
                 << "__null\";\n";
      }
    }
    output << "}\n";
  }
  output << "}\n";
  output.close();
  if (!output)
    throw runtime_error("Cannot write AST copy graph: " + outputFilename);
}

void generateGraphOfAST(SgProject *project, string filename,
                        set<SgNode *> skippedNodeSet) {
  SimpleColorFilesTraversal::generateGraph(project, filename, skippedNodeSet);
}

void generateGraphOfAST(SgProject *project, string filename) {
  set<SgNode *> emptyNodeSet;
  SimpleColorFilesTraversal::generateGraph(project, filename, emptyNodeSet);
}
