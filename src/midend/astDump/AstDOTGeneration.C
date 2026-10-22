

// Author: Markus Schordan
// $Id: AstDOTGeneration.C,v 1.7 2008/01/08 02:56:38 dquinlan Exp $

#ifndef ASTDOTGENERATION_C
#define ASTDOTGENERATION_C

#include "AstDOTGeneration.h"

#include "sage3basic.h"

#include "rose_test_output_path.h"

#include "transformationTracking.h"

// DQ (10/21/2010):  This should only be included by source files that require
// it. This fixed a reported bug which caused conflicts with configure-time
// macros (e.g. PACKAGE_BUGREPORT). Interestingly it must be at the top of the
// list of include files.
#include "rose_config.h"
#define TEMPLATE_IMPLEMENTATIONS
#include "AstConsistencyTests.h"

#include "AstDOTGenerationImpl.C"
#undef TEMPLATE_IMPLEMENTATIONS

using namespace std;  // DQ (12/31/2005): This is OK if not declared in a header
                      // file
using namespace Rose; // until this file is all moved into the rose namespace

void AstDOTGeneration::generate(SgProject *project,
                                std::string const &filename) {
  init();
  traversal = TOPDOWNBOTTOMUP;
  skip_write_during_traversal = true;

  DOTInheritedAttribute ia;
  traverse(project, ia);

  dotrep.writeToFileAsGraph(filename);
}

void AstDOTGeneration::generate(SgNode *node, string filename, traversalType tt,
                                string filenamePostfix) {
  init();
  traversal = tt;
  skip_write_during_traversal = true;
  this->filenamePostfix = filenamePostfix;
  DOTInheritedAttribute ia;
  traverse(node, ia);

  string filename2 = string("./") + filename;
  if (filenamePostfix != "") {
    filename2 += "." + filenamePostfix;
  }
  filename2 += ".dot";

  dotrep.writeToFileAsGraph(filename2);
}

void AstDOTGeneration::generate(SgProject *node, traversalType tt,
                                string filenamePostfix) {
  init();
  traversal = tt;
  skip_write_during_traversal = false;
  this->filenamePostfix = filenamePostfix;
  DOTInheritedAttribute ia;

  ROSE_ASSERT(node != NULL);
  traverse(node, ia);
}

void AstDOTGeneration::generateInputFiles(SgProject *node, traversalType tt,
                                          string filenamePostfix,
                                          bool excludeTemplateInstantiations) {
  init();
  traversal = tt;
  skip_write_during_traversal = false;
  this->filenamePostfix = filenamePostfix;
  DOTInheritedAttribute ia;

  ROSE_ASSERT(node != NULL);
  // DQ (12/20/2018): Skip the template instantiations that can make the files
  // to large for DOT visualizations.
  if (excludeTemplateInstantiations == true) {
    ia.skipTemplateInstantiations = true;
  }

  traverseInputFiles(node, ia);
}

void AstDOTGeneration::generateWithinFile(SgFile *node, traversalType tt,
                                          string filenamePostfix) {
  init();
  traversal = tt;
  skip_write_during_traversal = false;
  this->filenamePostfix = filenamePostfix;
  DOTInheritedAttribute ia;

  ROSE_ASSERT(node != NULL);
  traverseWithinFile(node, ia);
}

DOTInheritedAttribute
AstDOTGeneration::evaluateInheritedAttribute(SgNode *node,
                                             DOTInheritedAttribute ia) {
  visitedNodes.insert(node);

  Sg_File_Info *fileInfo = node->get_file_info();
  if (fileInfo != NULL && fileInfo->isCompilerGenerated() &&
      ia.skipTemplateInstantiations &&
      (isSgTemplateInstantiationDecl(node) != NULL ||
       isSgTemplateInstantiationFunctionDecl(node) != NULL ||
       isSgTemplateInstantiationMemberFunctionDecl(node) != NULL)) {
    ia.skipSubTree = true;
  }

  if (ia.skipSubTree == false) {
    ia.tdbuTracePos = tdbuTrace++;
    ia.tdTracePos = tdTrace++;
  }

  return ia;
}

void AstDOTGeneration::addAdditionalNodesAndEdges(SgNode *node) {
  //*****
  // Nodes and edges can be annotated with additional information. This
  // information is in the form of additional nodes and edges. These is added to
  // the output on a per-node basis.

  // DQ (11/17/2013): Added assertion.
  ROSE_ASSERT(node != NULL);

  // DQ (7/4/2008): Support for edges specified in AST attributes
  AstAttributeMechanism *astAttributeContainer = node->get_attributeMechanism();
  if (astAttributeContainer != NULL) {
    // Loop over all the attributes at this IR node
    for (const std::string &name :
         astAttributeContainer->getAttributeIdentifiers()) {
      AstAttribute *attribute = astAttributeContainer->operator[](name);
      ROSE_ASSERT(attribute != NULL);

      // This can return a non-empty list in user-defined attributes (derived
      // from AstAttribute).
      std::vector<AstAttribute::AttributeNodeInfo> nodeList =
          attribute->additionalNodeInfo();
      for (std::vector<AstAttribute::AttributeNodeInfo>::iterator i_node =
               nodeList.begin();
           i_node != nodeList.end(); i_node++) {
        SgNode *nodePtr = i_node->nodePtr;
        string nodelabel = i_node->label;
        string nodeoption = i_node->options;
        dotrep.addNode(nodePtr, nodelabel, nodeoption);
      }
      std::vector<AstAttribute::AttributeEdgeInfo> edgeList =
          attribute->additionalEdgeInfo();
      for (std::vector<AstAttribute::AttributeEdgeInfo>::iterator i_edge =
               edgeList.begin();
           i_edge != edgeList.end(); i_edge++) {
        string edgelabel = i_edge->label;
        string edgeoption = i_edge->options;
        dotrep.addEdge(i_edge->fromNode, edgelabel, i_edge->toNode,
                       edgeoption + "dir=forward");
      }
    }
  }
}

void AstDOTGeneration::writeIncidenceGraphToDOTFile(
    SgIncidenceDirectedGraph *graph, const std::string &filename) {
  // Output all nodes
  rose_graph_integer_node_hash_map &nodes = graph->get_node_index_to_node_map();

  for (rose_graph_integer_node_hash_map::iterator it = nodes.begin();
       it != nodes.end(); ++it) {
    SgGraphNode *node = it->second;

    if (commentOutNodeInGraph(node) == false) {
      string nodelabel = string("\\n") + node->get_name();

      nodelabel += additionalNodeInfo(node);
      string nodeoption = additionalNodeOptions(node);

      dotrep.addNode(node, nodelabel, nodeoption);

      addAdditionalNodesAndEdges(node);
    }
  }

  // Output edges
  rose_graph_integer_edge_hash_multimap &outEdges =
      graph->get_node_index_to_edge_multimap_edgesOut();

  for (rose_graph_integer_edge_hash_multimap::const_iterator outEdgeIt =
           outEdges.begin();
       outEdgeIt != outEdges.end(); ++outEdgeIt) {
    SgDirectedGraphEdge *graphEdge = isSgDirectedGraphEdge(outEdgeIt->second);
    ROSE_ASSERT(graphEdge != NULL);

    if (commentOutNodeInGraph(graphEdge) == false) {
      string edgelabel = string("\\n") + graphEdge->get_name();

      string edgeoption = additionalEdgeOptions(graphEdge->get_from(),
                                                graphEdge->get_to(), edgelabel);
      dotrep.addEdge(graphEdge->get_from(), edgelabel, graphEdge->get_to(),
                     edgeoption + "dir=forward");
      addAdditionalNodesAndEdges(graphEdge);
    }
  }

  dotrep.writeToFileAsGraph(filename);
}

DOTSynthesizedAttribute AstDOTGeneration::evaluateSynthesizedAttribute(
    SgNode *node, DOTInheritedAttribute ia, SubTreeSynthesizedAttributes l) {
  SubTreeSynthesizedAttributes::iterator iter;
  ROSE_ASSERT(node);

  // DQ (5/3/2006): Skip this IR node if it is specified as such in the
  // inherited attribute
  if (ia.skipSubTree == true) {
    // I am unclear if I should return NULL or node as a parameter to
    // DOTSynthesizedAttribute Figured this out: if we return a valid pointer
    // then we get a node in the DOT graph (with just the pointer value as a
    // label), where as if we return a DOTSynthesizedAttribute with a NUL
    // pointer then the node will NOT appear in the DOT graph. return
    // DOTSynthesizedAttribute(node);
    return DOTSynthesizedAttribute(NULL);
  }

  string nodeoption;
  AstTests::validateTraversalSuccessors(node);
  string nodelabel = string("\\n") + node->class_name();

  // DQ (7/20/2021): Added support for handle the SgSourceFile IR nodes (I want
  // to output the physical file id).
  if (isSgSourceFile(node) != NULL) {
    SgFile *file = dynamic_cast<SgFile *>(node);
    ROSE_ASSERT(file != NULL);

    string original_filename = file->getFileName();

    // DQ (7/4/2008): Fix filenamePostfix to go before the "."
    // string filename = string("./") +
    // Rose::utility_stripPathFromFileName(original_filename) +
    // "."+filenamePostfix+"dot";
    string filename = string("./") +
                      Rose::utility_stripPathFromFileName(original_filename) +
                      filenamePostfix + ".dot";

    int physical_file_id = Sg_File_Info::getIDFromFilename(original_filename);
    nodelabel +=
        string("\\n") + filename + string("\\n") +
        "physical_file_id = " + StringUtility::numberToString(physical_file_id);
  }

  // DQ (1/24/2009): Added support for output of isForward flag in the dot
  // graph.
  SgDeclarationStatement *genericDeclaration = isSgDeclarationStatement(node);
  if (genericDeclaration != NULL) {
    // At the moment the mnemonic name is stored, but it could be computed in
    // the future from the kind and the tostring() function.
    string name =
        (genericDeclaration->isForward() == true) ? "isForward" : "!isForward";
    ROSE_ASSERT(name.empty() == false);

    // DQ (3/20/2011): Added class names to the generated dot file graphs of the
    // AST.
    SgClassDeclaration *classDeclaration =
        isSgClassDeclaration(genericDeclaration);
    if (classDeclaration != NULL) {
      nodelabel += string("\\n") + classDeclaration->get_name();
    }

    // DQ (3/20/2011): Added function names to the generated dot file graphs of
    // the AST.
    SgFunctionDeclaration *functionDeclaration =
        isSgFunctionDeclaration(genericDeclaration);
    if (functionDeclaration != NULL) {
      nodelabel += string("\\n") + functionDeclaration->get_name();
    }

    // DQ (2/29/2012): Added typedef names to the generated dot file graphs of
    // the AST.
    SgTypedefDeclaration *typedefDeclaration =
        isSgTypedefDeclaration(genericDeclaration);
    if (typedefDeclaration != NULL) {
      nodelabel += string("\\n") + typedefDeclaration->get_name();

      // DQ (11/21/2015): Adding output of
      // typedefBaseTypeContainsDefiningDeclaration field.
      nodelabel +=
          string("\\n typedefBaseTypeContainsDefiningDeclaration = ") +
          (typedefDeclaration->get_typedefBaseTypeContainsDefiningDeclaration()
               ? "true "
               : "false ");
    }

    // DQ (3/20/2011): Added function names to the generated dot file graphs of
    // the AST.
    SgNamespaceDeclarationStatement *namespaceDeclarationStatement =
        isSgNamespaceDeclarationStatement(genericDeclaration);
    if (namespaceDeclarationStatement != NULL) {
      nodelabel += string("\\n") + namespaceDeclarationStatement->get_name();
    }

    // DQ (4/5/2015): Added variable names and associated info to the generated
    // dot file graphs of the AST.
    SgVariableDeclaration *variableDeclaration =
        isSgVariableDeclaration(genericDeclaration);
    if (variableDeclaration != NULL) {
      // This is added to support debugging of multiple variables in the same
      // declaration.
      nodelabel += string("\\n isAssociatedWithDeclarationList = ") +
                   (variableDeclaration->get_isAssociatedWithDeclarationList()
                        ? "true "
                        : "false ");

      // DQ (11/21/2015): Adding output of
      // typedefBaseTypeContainsDefiningDeclaration field.
      nodelabel +=
          string("\\n ownsBaseTypeDefiningDeclaration = ") +
          (variableDeclaration->get_baseTypeDefiningDeclaration() != NULL
               ? "true "
               : "false ");
    }

    nodelabel +=
        string("\\n isFriend = ") +
        (genericDeclaration->get_declarationModifier().isFriend() ? "true "
                                                                  : "false ");

    // DQ (4/2/2020): Need to add more detail to the graph output so
    // that we can debug Cxx_tests/test2020_02.C.
    nodelabel += string("\\n isExtern = ") +
                 (genericDeclaration->get_declarationModifier()
                          .get_storageModifier()
                          .isExtern()
                      ? "true "
                      : "false ");

    // DQ (10/18/2020): Add the reference to the defining and
    // non-defining declarations. Debugging outlining into seperate
    // file.
    nodelabel += string("\\n firstNondefiningDeclaration = ") +
                 StringUtility::numberToString(
                     genericDeclaration->get_firstNondefiningDeclaration());
    nodelabel += string("\\n definingDeclaration = ") +
                 StringUtility::numberToString(
                     genericDeclaration->get_definingDeclaration());

    nodelabel += string("\\n") + name;
  }

  // DQ (4/6/2011): Added support for output of the name for SgInitializedName
  // IR nodes.
  SgInitializedName *initializedName = isSgInitializedName(node);
  if (initializedName != NULL) {
    nodelabel += string("\\n") + initializedName->get_name();
  }

  // DQ (9/24/2018): Output this information for all located nodes (so that we
  // can include expressions).
  SgLocatedNode *genericLocatedNode = isSgLocatedNode(node);
  if (genericLocatedNode != NULL) {
    nodelabel +=
        string("\\n") + string("isModified = ") +
        string(genericLocatedNode->get_isModified() ? "true" : "false");
    nodelabel +=
        string("\\n") + string("containsTransformation = ") +
        string(genericLocatedNode->get_containsTransformation() ? "true"
                                                                : "false");
    if (genericLocatedNode->get_file_info()) {
      nodelabel +=
          string("\\n") + string("isTransformation = ") +
          string(genericLocatedNode->isTransformation() ? "true" : "false");
    } else {
      nodelabel += ("\\n *no file info*");
    }
  }

  // DQ (4/6/2011): Added support for output of the value within SgIntVal IR
  // nodes.
  SgIntVal *intValue = isSgIntVal(node);
  if (intValue != NULL) {
    nodelabel += string("\\n value = ") +
                 StringUtility::numberToString(intValue->get_value());
  }

  // DQ (4/6/2011): Added support for output of the name associated with
  // SgVarRefExp IR nodes.
  SgVarRefExp *varRefExp = isSgVarRefExp(node);
  if (varRefExp != NULL) {
    SgVariableSymbol *variableSymbol = varRefExp->get_symbol();
    ROSE_ASSERT(variableSymbol != NULL);
    nodelabel += string("\\n name = ") + variableSymbol->get_name().getString();
  }

  // DQ (8/28/2014): Added support for output of the name for function IR nodes.
  SgFunctionRefExp *functionRefExp = isSgFunctionRefExp(node);
  if (functionRefExp != NULL) {
    SgFunctionSymbol *functionSymbol = functionRefExp->get_symbol();
    ROSE_ASSERT(functionSymbol != NULL);

    // DQ (10/18/2020): Adding more inforamtion to the generated dot graphs.
    nodelabel += string("\\n functionSymbol = ") +
                 StringUtility::numberToString(functionSymbol);
    SgFunctionDeclaration *functionDeclaration =
        isSgFunctionDeclaration(functionSymbol->get_declaration());
    nodelabel += string("\\n functionDeclaration = ") +
                 StringUtility::numberToString(functionDeclaration);
    if (functionDeclaration != NULL) {
      nodelabel += string("\\n defining functionDeclaration = ") +
                   StringUtility::numberToString(
                       functionDeclaration->get_definingDeclaration());
      nodelabel += string("\\n nondefining functionDeclaration = ") +
                   StringUtility::numberToString(
                       functionDeclaration->get_firstNondefiningDeclaration());
    }

    nodelabel += string("\\n name = ") + functionSymbol->get_name().getString();
  }

  // DQ (8/28/2014): Added support for output of the name for member function IR
  // nodes.
  SgMemberFunctionRefExp *memberFunctionRefExp = isSgMemberFunctionRefExp(node);
  if (memberFunctionRefExp != NULL) {
    SgMemberFunctionSymbol *memberFunctionSymbol =
        memberFunctionRefExp->get_symbol();
    ROSE_ASSERT(memberFunctionSymbol != NULL);
    nodelabel +=
        string("\\n name = ") + memberFunctionSymbol->get_name().getString();
  }

  // DQ (10/19/2014): Added support for output of the name for
  // SgConstructorInitializer IR nodes.
  SgConstructorInitializer *constructorInitializer =
      isSgConstructorInitializer(node);
  if (constructorInitializer != NULL) {
    SgClassDeclaration *classDeclaration =
        constructorInitializer->get_class_decl();

    if (classDeclaration != NULL) {
      string name = classDeclaration->get_name();
      nodelabel += string("\\n name = ") + name;
    }
  }

  SgCastExp *castExpression = isSgCastExp(node);
  if (castExpression != NULL) {
    string name = SgCastExp::cast_type_to_string(castExpression->cast_type());
    nodelabel += string("\\n cast kind = ") + name;

    string cast_classification =
        castExpression->get_file_info()->isImplicitCast() ? "implicit cast"
                                                          : "explicit cast";
    nodelabel += string("\\n cast_classification = ") + cast_classification;
  }

  // DQ (4/6/2018): Adding support to output lvalue information.
  SgExpression *expression = isSgExpression(node);
  if (expression != NULL) {
    bool is_lvalue = expression->get_lvalue();
    nodelabel += string("\\n lvalue = ") + (is_lvalue ? "true" : "false");
  }

  // DQ (11/29/2008): Output the directives in the label of the IR node.
  SgC_PreprocessorDirectiveStatement *preprocessorDirective =
      isSgC_PreprocessorDirectiveStatement(node);
  if (preprocessorDirective != NULL) {
    const string &s = preprocessorDirective->get_directiveString();

    if (s.empty() == false)
      nodelabel += string("\\n") + s;
  }

  nodelabel += additionalNodeInfo(node);

  nodeoption += additionalNodeOptions(node);

  // DQ (7/27/2008): Added mechanism to support pruning of AST
  bool commentoutNode = commentOutNodeInGraph(node);
  if (commentoutNode == true) {
    if (SgProject::get_verbose() > 0) {
      printf("Skipping the use of this IR node in the DOT Graph \n");
    }
  } else {

    switch (traversal) {
    case TOPDOWNBOTTOMUP:
      dotrep.addNode(node,
                     dotrep.traceFormat(ia.tdbuTracePos, tdbuTrace) + nodelabel,
                     nodeoption);
      break;
    case PREORDER:
    case TOPDOWN:
      dotrep.addNode(node, dotrep.traceFormat(ia.tdTracePos) + nodelabel,
                     nodeoption);
      break;
    case POSTORDER:
    case BOTTOMUP:
      dotrep.addNode(node, dotrep.traceFormat(buTrace) + nodelabel, nodeoption);
      break;
    default:
      ROSE_ABORT();
    }

    ++tdbuTrace;
    ++buTrace;

    // add edges or null values
    int testnum = 0;
    for (iter = l.begin(); iter != l.end(); iter++) {
      string edgelabel =
          string(node->get_traversalSuccessorNamesContainer()[testnum]);
      string toErasePrefix = "p_";

      if (edgelabel.compare(0, toErasePrefix.size(), toErasePrefix) == 0) {
        edgelabel.erase(0, toErasePrefix.size());
      }

      if (iter->node == NULL) {
        AstSuccessorsSelectors::SuccessorsContainer c;
        AstSuccessorsSelectors::selectDefaultSuccessors(node, c);
        SgNode *snode = c[testnum];

        // isDefault shows that the default constructor for synth attribute was
        // used
        if (l[testnum].isDefault() && snode &&
            (visitedNodes.find(snode) != visitedNodes.end())) {
          dotrep.addEdge(node, edgelabel, snode,
                         "dir=forward arrowhead=\"odot\" color=red ");
        } else {
          if (snode == NULL) {
            dotrep.addNullValue(node, "", edgelabel, "");
          }
        }
      } else {
        // DQ (3/5/2007) added mechanism to add additional options (to add
        // color, etc.)
        string edgeoption = additionalEdgeOptions(node, iter->node, edgelabel);

        switch (traversal) {
        case TOPDOWNBOTTOMUP:
          dotrep.addEdge(node, edgelabel, (*iter).node,
                         edgeoption + "dir=both");
          break;
        case PREORDER:
        case TOPDOWN:
          dotrep.addEdge(node, edgelabel, (*iter).node,
                         edgeoption + "dir=forward");
          break;
        case POSTORDER:
        case BOTTOMUP:
          dotrep.addEdge(node, edgelabel, (*iter).node,
                         edgeoption + "dir=back");
          break;
        default:
          ROSE_ABORT();
        }
      }

      testnum++;
    }
  }

  // DQ (7/4/2008): Support for edges specified in AST attributes
  AstAttributeMechanism *astAttributeContainer = node->get_attributeMechanism();
  if (astAttributeContainer != NULL) {
    // Loop over all the attributes at this IR node
    for (const std::string &name :
         astAttributeContainer->getAttributeIdentifiers()) {
      AstAttribute *attribute = astAttributeContainer->operator[](name);
      ROSE_ASSERT(attribute != NULL);

      std::vector<AstAttribute::AttributeNodeInfo> nodeList =
          attribute->additionalNodeInfo();

      for (std::vector<AstAttribute::AttributeNodeInfo>::iterator i_node =
               nodeList.begin();
           i_node != nodeList.end(); i_node++) {
        SgNode *nodePtr = i_node->nodePtr;
        string nodelabel = i_node->label;
        string nodeoption = i_node->options;
        dotrep.addNode(nodePtr, dotrep.traceFormat(ia.tdTracePos) + nodelabel,
                       nodeoption);
      }

      std::vector<AstAttribute::AttributeEdgeInfo> edgeList =
          attribute->additionalEdgeInfo();
      for (std::vector<AstAttribute::AttributeEdgeInfo>::iterator i_edge =
               edgeList.begin();
           i_edge != edgeList.end(); i_edge++) {
        string edgelabel = i_edge->label;
        string edgeoption = i_edge->options;
        dotrep.addEdge(i_edge->fromNode, edgelabel, i_edge->toNode,
                       edgeoption + "dir=forward");
      }
    }
  }

  if (!skip_write_during_traversal) {
    switch (node->variantT()) {
    case V_SgProject: {
      SgProject *project = dynamic_cast<SgProject *>(node);
      ROSE_ASSERT(project != NULL);

      string generatedProjectName = SageInterface::generateProjectName(project);
      if (generatedProjectName.length() > 40) {
        generatedProjectName = "aggregatedFileNameTooLong";
        printf("Proposed (generated) filename is too long, shortened to: %s \n",
               generatedProjectName.c_str());
      }

      string filename = string("./") + generatedProjectName;
      if (!filenamePostfix.empty()) {
        filename += "." + filenamePostfix;
      }
      filename += ".dot";
      filename = Rose::TestOutput::resolvePath(filename);

      if (SgProject::get_verbose() >= 1)
        printf("Output the DOT graph from the SgProject IR node (filename = "
               "%s) \n",
               filename.c_str());

      dotrep.writeToFileAsGraph(filename);
      break;
    }

    case V_SgSourceFile: {
      SgFile *file = dynamic_cast<SgFile *>(node);
      ROSE_ASSERT(file != NULL);

      string original_filename = file->getFileName();

      string filename = string("./") +
                        Rose::utility_stripPathFromFileName(original_filename) +
                        filenamePostfix + ".dot";
      filename = Rose::TestOutput::resolvePath(filename);

      // DQ (9/1/2008): this effects the output of DOT files when
      // multiple files are specified on the command line.  A SgProject
      // is still built even when a single file is specificed on the
      // command line, however there are cases where a SgFile can be
      // built without a SgProject and this case allows those SgFile
      // rooted subtrees to be output as DOT files. If there is a
      // SgProject then output the dot file from there, else output as
      // a SgFile.
      if (file->get_parent() == NULL) {
        // If there is no SgProject then output the file now!
        if (SgProject::get_verbose() >= 1)
          printf("Output the DOT graph from the SgFile IR node (no SgProject "
                 "available) \n");

        dotrep.writeToFileAsGraph(filename);
      } else {
        // There is a SgProject IR node, but if we will be
        // traversing it we want to output the graph then (so that
        // the graph will include the SgProject IR nodes and connect
        // multiple files (SgSourceFile IR nodes).
        if (visitedNodes.find(file->get_parent()) == visitedNodes.end()) {
          // This SgProject node was not input as part of the traversal,
          // so we will not be traversing the SgProject IR nodes and we
          // have to output the graph now!

          if (SgProject::get_verbose() >= 1)
            printf("Output the DOT graph from the SgFile IR node (SgProject "
                   "was not traversed) \n");

          dotrep.writeToFileAsGraph(filename);
          dotrep.clear();
        } else {
          if (SgProject::get_verbose() >= 1)
            printf("Skip the output of the DOT graph from the SgFile IR node "
                   "(SgProject will be traversed) \n");
        }
      }

      break;
    }

    default: {
      break;
    }
    }
  }

  return DOTSynthesizedAttribute(node);
}

static std::string generateFileLineColumnString(Sg_File_Info *fileInfo) {
  // DQ (9/1/2013): Adding source position information for DOT output.
  string ss;

  ROSE_ASSERT(fileInfo != NULL);
  string file = fileInfo->get_filename();
  file = Rose::utility_stripPathFromFileName(file);

  int line = fileInfo->get_line();
  int column = fileInfo->get_col();

  ss += file;

  // DQ (9/1/2013): When ROSE is optimized, this will be output as "::" the
  // output of the line and column integers is optimized away (GNU 4.2.4).  To
  // fix this call to string functions to convert integers to strings
  // explicitly.
  ss += ":";
  ss += StringUtility::numberToString(line);
  ss += ":";
  ss += StringUtility::numberToString(column);

  // DQ (12/21/2014): Adding the physical line number (used by the token
  // mapping).
  int physical_line = fileInfo->get_physical_line();
  ss += " (physical line=";
  ss += StringUtility::numberToString(physical_line);
  ss += ")";

  // DQ (12/21/2014): Adding the physical line number (used by the token
  // mapping).
  int raw_line = fileInfo->get_raw_line();
  int raw_column = fileInfo->get_raw_col();
  ss += " (raw line:col=";
  ss += StringUtility::numberToString(raw_line);
  ss += ":";
  ss += StringUtility::numberToString(raw_column);
  ss += ")";

  ss += "\\n";

  return ss;
}

static std::string sourcePositionInformation(SgNode *node) {
  // DQ (8/31/2013): Adding source position information for DOT output.
  string ss;

  SgLocatedNode *locatedNode = isSgLocatedNode(node);
  if (locatedNode != NULL) {
    Sg_File_Info *fileInfo = locatedNode->get_file_info();
    if (fileInfo != NULL) {
      bool hasSpecialMode = false;
      if (fileInfo->isCompilerGenerated() == true) {
        ss += "compiler generated\\n";
        hasSpecialMode = true;

        ss += generateFileLineColumnString(locatedNode->get_startOfConstruct());
        ss += generateFileLineColumnString(locatedNode->get_endOfConstruct());
      } else {
        if (fileInfo->isFrontendSpecific() == true) {
          ss += "front-end specific\\n";
          hasSpecialMode = true;
        } else {
          if (fileInfo->isTransformation() == true) {
            ss += "is part of transformation\\n";
            hasSpecialMode = true;
          } else {
            // ss += "???\\n";
          }
        }
      }

      if (hasSpecialMode == true) {
        if (fileInfo->isOutputInCodeGeneration() == true) {
          ss += "IS output in generated code\\n";
        } else {
          ss += "is NOT output in generated code\\n";
        }
      } else {
        // DQ (9/1/2013): Name a few cases were we want to output the end of the
        // IR node construct's source position range. bool outputEndOfConstruct
        // = (isSgAggregateInitializer(node) != NULL || isSgScopeStatement(node)
        // != NULL);
        bool outputEndOfConstruct =
            true; // (isSgAggregateInitializer(node) != NULL ||
                  // isSgStatement(node) != NULL);

        if (outputEndOfConstruct == true) {
          // Output the end of the range represented by the IR node's source
          // position.
          ss +=
              generateFileLineColumnString(locatedNode->get_startOfConstruct());
          ss += generateFileLineColumnString(locatedNode->get_endOfConstruct());
        } else {
          // For an SgStatement this is the startOfConstruct, but for an
          // SgExpression this is the operator position (or sometimes equal to
          // the startOfConstruct).
          ss += generateFileLineColumnString(fileInfo);
        }
      }
    } else {
      ss += "no source position available\\n";
    }

    // DQ (1/28/2015): Added more info to support debugging the token-stream
    // unparsing.
    if (locatedNode->get_containsTransformationToSurroundingWhitespace() ==
        true) {
      ss += "containsTransformationToSurroundingWhitespace == true\\n";
    } else {
      ss += "containsTransformationToSurroundingWhitespace == false\\n";
    }
  } else {
    // DQ (9/1/2013): We could handle the source position of some other IR nodes
    // (e.g. output name of the file for SgFile).
    SgFile *file = isSgFile(node);
    if (file != NULL) {
      ROSE_ASSERT(file->get_file_info() != NULL);
      ss += generateFileLineColumnString(file->get_file_info());
    }
  }

  return ss;
}

// DQ (9/19/2013): generate the number associated with each position relative to
// the attached IR node.
static size_t numberByRelativePosition(
    AttachedPreprocessingInfoType *commentsAndCppDirectives,
    PreprocessingInfo::RelativePositionType pos) {
  size_t returnValue = 0;

  for (vector<PreprocessingInfo *>::iterator i =
           commentsAndCppDirectives->begin();
       i != commentsAndCppDirectives->end(); i++) {
    if ((*i)->getRelativePosition() == pos) {
      returnValue++;
    }
  }

  return returnValue;
}

static std::string commentAndCppInformation(SgNode *node) {
  string ss;

  SgLocatedNode *locatedNode = isSgLocatedNode(node);
  if (locatedNode != NULL) {
    AttachedPreprocessingInfoType *commentsAndCppDirectives =
        locatedNode->getAttachedPreprocessingInfo();

    if (commentsAndCppDirectives != NULL) {
      ss += string("comments/directives (before) = ") +
            StringUtility::numberToString(numberByRelativePosition(
                commentsAndCppDirectives, PreprocessingInfo::before)) +
            "\\n";
      ss += string("comments/directives (inside) = ") +
            StringUtility::numberToString(numberByRelativePosition(
                commentsAndCppDirectives, PreprocessingInfo::inside)) +
            "\\n";
      ss += string("comments/directives (after)  = ") +
            StringUtility::numberToString(numberByRelativePosition(
                commentsAndCppDirectives, PreprocessingInfo::after)) +
            "\\n";
    }
  }

  return ss;
}

// To improve the default output add additional information here
// Note you need to add "\\n" for newline
string AstDOTGeneration::additionalNodeInfo(SgNode *node) {
  ostringstream ss;
  ss << "\\n";

  // print number of max successors (= container size)
  AstSuccessorsSelectors::SuccessorsContainer c;
  AstSuccessorsSelectors::selectDefaultSuccessors(node, c);
  ss << "child_count:" << c.size() << "\\n";

  // add memory location of node to dot output
  ss << node << "\\n";

  // DQ (7/4/2014): Adding more debug support to the AST visualization.
  SgFunctionCallExp *functionCallExp = isSgFunctionCallExp(node);
  if (functionCallExp != NULL) {
    string value = "false";
    if (functionCallExp->get_uses_operator_syntax() == true) {
      value = "true";
    }
    ss << string("uses_operator_syntax() = ") << value << "\\n";
  }

  // DQ (8/31/2013): Added more information about the IR node to the dot graph.
  ss << sourcePositionInformation(node);

  // DQ (9/19/2013): Added more information about the IR node to the dot graph
  // (comments and C preprocessor directive information).
  ss << commentAndCppInformation(node);

  // DQ (7/4/2008): Added support for output of information about attributes
  AstAttributeMechanism *astAttributeContainer = node->get_attributeMechanism();
  if (astAttributeContainer != NULL) {
    ss << "Attribute list (size=" << astAttributeContainer->size()
       << "):" << "\\n";
    for (const std::string &name :
         astAttributeContainer->getAttributeIdentifiers()) {
      // pair<std::string,AstAttribute*>
      AstAttribute *attribute = astAttributeContainer->operator[](name);
      ROSE_ASSERT(attribute != NULL);

      // Note cast to void*
      std::string label = name + " : " + attribute->toString();
      ss << label << "\\n";
    }
    ss << "\\n";
    ss << "\\n";
  }
  // Liao, 4/3/2014  display Unique ID for some nodes
  if (TransformationTracking::getId(node) != 0)
    ss << "ID:" << TransformationTracking::getId(node) << "\\n";
  return ss.str();
}

// DQ (11/1/2003) added mechanism to add additional options (to add color, etc.)
string AstDOTGeneration::additionalNodeOptions(SgNode *node) {
  ostringstream ss;

  // DQ (7/4/2008): Added support for output of information about attributes
  AstAttributeMechanism *astAttributeContainer = node->get_attributeMechanism();
  if (astAttributeContainer != NULL) {
    for (const std::string &name :
         astAttributeContainer->getAttributeIdentifiers()) {
      AstAttribute *attribute = astAttributeContainer->operator[](name);
      ROSE_ASSERT(attribute != NULL);

      ss << attribute->additionalNodeOptions();
    }
  }

  return ss.str();
}

bool AstDOTGeneration::commentOutNodeInGraph(SgNode *node) {
  bool resultValue = false;

  // DQ (7/4/2008): Added support for output of information about attributes
  AstAttributeMechanism *astAttributeContainer = node->get_attributeMechanism();
  if (astAttributeContainer != NULL) {
    for (const std::string &name :
         astAttributeContainer->getAttributeIdentifiers()) {
      AstAttribute *attribute = astAttributeContainer->operator[](name);
      ROSE_ASSERT(attribute != NULL);

      // Turn it ON if there is an attribute to do so, but don't turn it off
      // (for attribute to be changed)
      if (resultValue == false)
        resultValue = attribute->commentOutNodeInGraph();
    }
  }

  return resultValue;
}

// DQ (6/25/2011): Put the function definition into the source file (avoid
// function definitions in header files). std::string operator()(SgNode* node)
std::string AstDOTGenerationExtended_Defaults::NamedExtraNodeInfo::operator()(
    SgNode *node) {
  std::ostringstream ss;

  // add namespace name
  if (SgNamespaceDeclarationStatement *n =
          isSgNamespaceDeclarationStatement(node)) {
    ss << n->get_qualified_name().str() << "\\n";
  }
  // add class name
  if (SgClassDeclaration *n = isSgClassDeclaration(node)) {
    ss << n->get_qualified_name().str() << "\\n";
  }
  // add function name
  if (SgFunctionDeclaration *n = isSgFunctionDeclaration(node)) {
    ss << n->get_qualified_name().str() << "\\n";
  }
  if (SgFunctionRefExp *n = isSgFunctionRefExp(node)) {
    SgFunctionDeclaration *decl = n->getAssociatedFunctionDeclaration();
    if (decl) // it's null if through a function pointer
    {
      ss << decl->get_qualified_name().str() << "\\n";
    }
  }
  // add variable name
  if (SgInitializedName *n = isSgInitializedName(node)) {
    ss << n->get_qualified_name().str() << "\\n";
  }
  if (SgVarRefExp *n = isSgVarRefExp(node)) {
    SgVariableSymbol *sym = n->get_symbol();
    ss << sym->get_name().getString() << "\\n";
  }
  // add variable name
  if (SgVariableSymbol *n = isSgVariableSymbol(node)) {
    ss << n->get_name().str() << "\\n";
  }

  return ss.str();
}

// DQ (6/25/2011): Put the function definition into the source file (avoid
// function definitions in header files). std::string operator()(SgNode* node)
std::string
AstDOTGenerationExtended_Defaults::TypeExtraNodeInfo::operator()(SgNode *node) {
  std::ostringstream ss;

  if (SgExpression *n = isSgExpression(node)) {
    SgType *expressionType = n->get_type();
    if (expressionType == nullptr) {
      fprintf(stderr,
              "REX_AST_INVARIANT[dot-expression-type]: "
              "expression=%p type=%s has no exact semantic value type\n",
              static_cast<void *>(n), n->class_name().c_str());
      ROSE_ABORT();
    }
    // Value types are semantic identities.  They do not imply a source type
    // occurrence at the expression node (implicit casts are the common
    // counterexample), so DOT diagnostics report the typed IR class directly.
    ss << expressionType->class_name() << "\\n";
  }

  return ss.str();
}

// DQ (6/25/2011): Put the function definition into the source file (avoid
// function definitions in header files). std::string operator()(SgNode* node)
std::string AstDOTGenerationExtended_Defaults::LValueExtraNodeInfo::operator()(
    SgNode *node) {
  std::ostringstream ss;

  // adds whether or not it is an l-value
  if (SgExpression *n = isSgExpression(node)) {
    ss << (n->isLValue() ? "L-Value" : "!L-Value") << "\\n";
  }

  return ss.str();
}

#endif
