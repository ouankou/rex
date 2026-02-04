#include "rose.h"

#include <algorithm>
#include <cctype>
#include <string>

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);

  Rose_STL_Container<SgNode *> types =
      NodeQuery::querySubTree(project, V_SgNonrealType);

  bool found_qualified = false;
  auto compact = [](std::string text) {
    text.erase(
        std::remove_if(text.begin(), text.end(),
                       [](unsigned char ch) { return std::isspace(ch); }),
        text.end());
    return text;
  };

  for (SgNode *node : types) {
    SgNonrealType *type = isSgNonrealType(node);
    if (type == nullptr) {
      continue;
    }
    std::string text = compact(type->unparseToString());
    if (text.find("ns::Later") != std::string::npos) {
      found_qualified = true;
      break;
    }
  }

  ROSE_ASSERT(found_qualified &&
              "Expected qualified nonreal type for dependent name");
  return 0;
}
