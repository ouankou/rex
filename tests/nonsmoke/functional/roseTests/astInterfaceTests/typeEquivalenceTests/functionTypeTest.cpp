/*
 * Program to test the type equivalence thing on variable types.
 */

#include "rose.h"
#include "RoseAst.h"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
//#include "typeEquivalenceChecker.hpp"


namespace {
bool readExpectedFromFile(const char *filename, int *expected) {
  if (filename == nullptr || expected == nullptr) {
    return false;
  }
  std::ifstream input(filename);
  if (!input) {
    return false;
  }
  std::string line;
  if (!std::getline(input, line)) {
    return false;
  }
  if (line.rfind("//", 0) == 0) {
    line.erase(0, 2);
  }
  const size_t start = line.find_first_not_of(" \t\r\n");
  if (start == std::string::npos) {
    return false;
  }
  const size_t end = line.find_last_not_of(" \t\r\n");
  line = line.substr(start, end - start + 1);
  char *endptr = nullptr;
  const long value = std::strtol(line.c_str(), &endptr, 10);
  if (endptr == line.c_str() || *endptr != '\0') {
    return false;
  }
  *expected = static_cast<int>(value);
  return true;
}
}  // namespace

class FunctionTypeAccu: public AstSimpleProcessing {

 public:
  FunctionTypeAccu();
  void visit(SgNode *node);

  std::vector < SgFunctionDeclaration * >funcs_;
  bool profile_;
};

FunctionTypeAccu::FunctionTypeAccu() {
}

void
FunctionTypeAccu::visit(SgNode *node) {
  if (isSgFunctionDeclaration(node)) {
    SgFunctionDeclaration *func = isSgFunctionDeclaration(node);

    funcs_.push_back(func);
  }
  return;
}


int
main(int argc, char **argv) {
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <input file>\n";
    return 1;
  }
  int expected = 0;
  const bool has_expected = readExpectedFromFile(argv[1], &expected);

  SgProject *proj = frontend(argc, argv);

  FunctionTypeAccu t;
  t.traverse(proj, preorder);

  bool checkEqual = false;

  std::vector < SgFunctionDeclaration * >::iterator i, j;
  for (i = t.funcs_.begin(); i != t.funcs_.end(); ++i) {
    for (j = t.funcs_.begin(); j != t.funcs_.end(); ++j) {
      if ((*i)->get_name().getString() != "a"
          || (*j)->get_name().getString() != "b") {
        continue;
      }
      // We use the type of the initialized names here to check
      checkEqual = SageInterface::checkTypesAreEqual((*i)->get_type(), (*j)->get_type());
      if (i == j) {
        continue;
      }
    }
  }
  const int actual = checkEqual ? 0 : 255;
  if (!has_expected) {
    return 0;
  }
  return (actual == expected) ? 0 : 1;
}
