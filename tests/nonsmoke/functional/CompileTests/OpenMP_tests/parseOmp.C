/*
 * Parsing OpenMP pragma text
by Liao, 9/17/2008
Last Modified: 9/19/2008
*/
#include "rose.h"

#include "RoseAst.h"

#include <iostream>

#include <string>

#include "ompSupport.h"
using namespace std;
using namespace OmpSupport;

int main(int argc, char *argv[]) {
  SgProject *project = frontend(argc, argv);

  AstTests::runAllTests(project);

  //  visitorTraversal myvisitor;
  //  myvisitor.traverseInputFiles(project,preorder);
  return backend(project);
}
