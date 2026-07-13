// Example ROSE Translator: used within ROSE/tutorial

#include "rose.h"

#include "linearizeAST.h"

#include <functional>

#include <ostream>
using namespace std;

#include <set>
#include <vector>

void print_out_all_macros(std::ostream &outStream, SgNode *node) {

};

class VisitEveryNode : public SgSimpleProcessing {
private:
  std::ostream &outStream;
  std::set<int> inputFileIds;
  bool isFromInputFile(SgNode *node) const;

public:
  VisitEveryNode(std::ostream &outS, SgProject *project);
  // required visit function to define what is to be done
  void visit(SgNode *astNode);
};

VisitEveryNode::VisitEveryNode(std::ostream &outS, SgProject *project)
    : outStream(outS) {
  if (project == nullptr) {
    return;
  }

  for (SgFile *file : project->get_fileList()) {
    SgSourceFile *sourceFile = isSgSourceFile(file);
    if (sourceFile == nullptr || sourceFile->get_file_info() == nullptr) {
      continue;
    }
    inputFileIds.insert(sourceFile->get_file_info()->get_file_id());
  }
};

bool VisitEveryNode::isFromInputFile(SgNode *node) const {
  SgLocatedNode *locatedNode = isSgLocatedNode(node);
  if (locatedNode == nullptr || locatedNode->get_file_info() == nullptr) {
    return false;
  }

  return inputFileIds.count(locatedNode->get_file_info()->get_file_id()) != 0;
}

void VisitEveryNode::visit(SgNode *node) {
  // We don't want to unparse the whole file; this pulls in built-in functions
  // which are different between platforms
  if (isSgSourceFile(node) || isSgGlobal(node))
    return;
  if (!isFromInputFile(node))
    return;

  std::vector<SgNode *> linearizedSubtree = linearize_subtree(node);

  const bool ownerOnlySyntaxContainer =
      isSgCatchStatementSeq(node) != nullptr ||
      isSgFunctionParameterList(node) != nullptr ||
      isSgCtorInitializerList(node) != nullptr;
  outStream << "Unparsed: " << node->class_name();
  if (ownerOnlySyntaxContainer) {
    outStream << " <owned-syntax-container>";
  } else if (SgInitializedName *initializedName = isSgInitializedName(node)) {
    outStream << " " << initializedName->get_name().getString();
  } else {
    const std::string source = node->unparseToString();
    if (!source.empty()) {
      outStream << " " << source;
    }
  }
  outStream << std::endl;
  outStream << "          ";
  bool first = true;
  for (std::vector<SgNode *>::iterator it_sub = linearizedSubtree.begin();
       it_sub != linearizedSubtree.end(); ++it_sub) {
    if (!first) {
      outStream << " ";
    }
    first = false;
    outStream << (*it_sub)->class_name();
    if (isSgValueExp(*it_sub) != NULL) {
      const std::string value = (*it_sub)->unparseToString();
      if (!value.empty()) {
        outStream << " " << value;
      }
    }
  }

  outStream << std::endl;
};

int main(int argc, char *argv[]) {
  SgProject *project = frontend(argc, argv);
  VisitEveryNode aNode(std::cout, project);
  aNode.traverseInputFiles(project, preorder);
};
