#include "rose.h"

#include "modified_sage.h"
#include "unparser.h"

#include <sstream>
#include <utility>
#include <vector>

int main() {
  Unparser_Opt options;
  std::ostringstream firstOutput;
  std::ostringstream secondOutput;
  Unparser first(&firstOutput, "first.C", options);
  Unparser second(&secondOutput, "second.C", options);

  first.u_sage->pushActiveExternLinkageBraceLanguage("C");
  if (first.u_sage->getActiveExternLinkageBraceLanguage() != "C" ||
      !second.u_sage->getActiveExternLinkageBraceLanguage().empty()) {
    return 1;
  }

  second.u_sage->pushActiveExternLinkageBraceLanguage("C++");
  if (first.u_sage->getActiveExternLinkageBraceLanguage() != "C" ||
      second.u_sage->getActiveExternLinkageBraceLanguage() != "C++") {
    return 2;
  }

  std::vector<std::pair<bool, std::string>> firstColors;
  std::vector<std::pair<bool, std::string>> secondColors;
  first.u_sage->setupColorCodes(firstColors);
  second.u_sage->setupColorCodes(secondColors);

  auto *firstNode = new SgNullStatement;
  auto *secondNode = new SgNullStatement;
  first.u_sage->printColorCodes(firstNode, true, firstColors);
  second.u_sage->printColorCodes(secondNode, true, secondColors);
  if (firstOutput.str().find("colorCode:red:on") == std::string::npos ||
      secondOutput.str().find("colorCode:red:on") == std::string::npos) {
    return 3;
  }

  first.u_sage->printColorCodes(firstNode, false, firstColors);
  second.u_sage->printColorCodes(secondNode, false, secondColors);
  delete firstNode;
  delete secondNode;
  first.u_sage->popActiveExternLinkageBraceLanguage();
  second.u_sage->popActiveExternLinkageBraceLanguage();

  if (!first.u_sage->getActiveExternLinkageBraceLanguage().empty() ||
      !second.u_sage->getActiveExternLinkageBraceLanguage().empty()) {
    return 4;
  }

  return 0;
}
