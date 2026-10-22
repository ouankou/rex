
#include "sage3.h"

#include "AstInterface_ROSE.h"

#include "CFGImpl.h"

#include "CommandOptions.h"

#include "GraphDotOutput.h"

#include "GraphIO.h"

#include <iostream>

#include <string>

using namespace std;

void PrintUsage(char *name) {
  cerr << name << " <options> " << "<program name>" << "\n";
  cerr << "-dot :generate DOT output \n";
}

bool GenerateDOT(int argc, char *argv[]) {
  for (int i = 1; i < argc; ++i) {
    if (argv[i] != 0 && !strcmp(argv[i], "-dot"))
      return true;
  }
  return false;
}

class TestCFGWrap {
protected:
  DefaultCFGImpl graph;

public:
  void operator()(SgNode *head) {
    AstInterfaceImpl scope(head);
    AstInterface fa(&scope);
    ROSE_Analysis::BuildCFG(fa, AstNodePtrImpl(head), graph);
  }
};

class TestCFGWrap_Stdout : public TestCFGWrap {
public:
  void operator()(SgNode *head) {
    TestCFGWrap::operator()(head);
    write_graph(graph, std::cout, "edge");
    std::cout << "\n";
  }
};

class TestCFGWrap_DOT : public TestCFGWrap {
public:
  void operator()(SgNode *head, string fname) {
    TestCFGWrap::operator()(head);
    GraphDotOutput<DefaultCFGImpl> output(graph);
    output.writeToDOTFile(fname, "CFGGraph");
  }
};

int main(int argc, char *argv[]) {

  if (argc <= 1) {
    PrintUsage(argv[0]);
    return -1;
  }

  SgProject sageProject((int)argc, argv);

  SageInterface::changeAllBodiesToBlocks(&sageProject);

  CmdOptions::GetInstance()->SetOptions(argc, argv);

  int filenum = sageProject.numberOfFiles();
  for (int i = 0; i < filenum; ++i) {
    SgSourceFile *sageFile = isSgSourceFile(sageProject.get_fileList()[i]);
    ROSE_ASSERT(sageFile != NULL);

    string filename = sageFile->getFileName();

    TestCFGWrap_Stdout txtop;
    string dotname = filename.substr(filename.rfind('/') + 1) + ".dot";
    TestCFGWrap_DOT dotop;
    SgGlobal *root = sageFile->get_globalScope();
    SgDeclarationStatementPtrList &declList = root->get_declarations();
    for (SgDeclarationStatementPtrList::iterator p = declList.begin();
         p != declList.end(); ++p) {
      SgFunctionDeclaration *func = isSgFunctionDeclaration(*p);
      if (func == 0)
        continue;
      SgFunctionDefinition *defn = func->get_definition();
      if (defn == 0)
        continue;
      SgNode *stmts = defn;
      if (GenerateDOT(argc, argv)) {
        dotop(stmts, dotname);
      } else {
        txtop(stmts);
      }
    }
  }

  return 0;
}
