// Warning: This is actually a header file! It's #included into multiple source
// files.

#ifdef TEMPLATE_IMPLEMENTATIONS

#ifndef ASTDOTGENERATION_TEMPLATES_C
#define ASTDOTGENERATION_TEMPLATES_C

// if we didn't include rose.h, then we need these files
#ifndef ROSE_H
#ifndef SAGE3_CLASSES_BASIC__H
#include "sage3basic.h"
#endif
#include "sageInterface.h"

#include "wholeAST_API.h"
#endif

#include "rose_test_output_path.h"

template <typename ExtraNodeInfo_t, typename ExtraNodeOptions_t,
          typename ExtraEdgeInfo_t, typename ExtraEdgeOptions_t>
DOTSynthesizedAttribute
AstDOTGenerationExtended<ExtraNodeInfo_t, ExtraNodeOptions_t, ExtraEdgeInfo_t,
                         ExtraEdgeOptions_t>::
    evaluateSynthesizedAttribute(SgNode *node, DOTInheritedAttribute ia,
                                 SubTreeSynthesizedAttributes l) {
  using namespace std;

  SubTreeSynthesizedAttributes::iterator iter;
  ROSE_ASSERT(node);

  if (ia.skipSubTree == true) {
    return DOTSynthesizedAttribute(NULL);
  }

  string nodelabel = string("\\n") + node->class_name();

  SgC_PreprocessorDirectiveStatement *preprocessorDirective =
      isSgC_PreprocessorDirectiveStatement(node);
  if (preprocessorDirective != NULL) {
    const string &s = preprocessorDirective->get_directiveString();

    if (s.empty() == false)
      nodelabel += string("\\n") + s;
  }

  nodelabel += eni(node);
  string nodeoption = eno(node);

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
        string edgeoption = additionalEdgeOptions(node, iter->node, edgelabel) +
                            eeo(node, iter->node, edgelabel);
        edgelabel += eei(node, iter->node, edgelabel);

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

  AstAttributeMechanism *astAttributeContainer = node->get_attributeMechanism();
  if (astAttributeContainer != NULL) {
    // Loop over all the attributes at this IR node
    AstAttributeMechanism::AttributeIdentifiers attrIdentifiers =
        astAttributeContainer->getAttributeIdentifiers();
    for (AstAttributeMechanism::AttributeIdentifiers::iterator i =
             attrIdentifiers.begin();
         i != attrIdentifiers.end(); ++i) {
      std::string name = *i;
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

  if (skip_write_during_traversal) {
    return DOTSynthesizedAttribute(node);
  }

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
      printf(
          "Output the DOT graph from the SgProject IR node (filename = %s) \n",
          filename.c_str());

    dotrep.writeToFileAsGraph(filename);
    break;
  }

  case V_SgSourceFile: {
    SgFile *file = dynamic_cast<SgFile *>(node);
    ROSE_ASSERT(file != NULL);

    string original_filename = file->getFileName();

    string filename =
        string("./") +
        Rose::StringUtility::stripPathFromFileName(original_filename) +
        filenamePostfix + ".dot";
    filename = Rose::TestOutput::resolvePath(filename);

    // Emit here only when the project node is outside this traversal.
    if (file->get_parent() == NULL) {
      if (SgProject::get_verbose() >= 1)
        printf("Output the DOT graph from the SgFile IR node (no SgProject "
               "available) \n");

      dotrep.writeToFileAsGraph(filename);
    } else {
      if (visitedNodes.find(file->get_parent()) == visitedNodes.end()) {
        if (SgProject::get_verbose() >= 1)
          printf("Output the DOT graph from the SgFile IR "
                 "node (SgProject was not traversed) \n");

        dotrep.writeToFileAsGraph(filename);
      } else {
        if (SgProject::get_verbose() >= 1)
          printf("Skip the output of the DOT graph from "
                 "the SgFile IR node (SgProject will be "
                 "traversed) \n");
      }
    }

    break;
  }

  default: {
    break;
  }
  }

  return DOTSynthesizedAttribute(node);
}

#endif // ASTDOTGENERATION_TEMPLATES_C

#else
#error Must include template implementation files after all header files.  If you have, make sure you define TEMPLATE_IMPLEMENTATIONS
#endif // TEMPLATE_IMPLEMENTATIONS
