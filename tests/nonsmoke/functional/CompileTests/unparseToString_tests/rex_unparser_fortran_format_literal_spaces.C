#include "rose.h"

#include <string>

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ASSERT_not_null(project);
  project->skipfinalCompileStep(true);

  const Rose_STL_Container<SgNode *> statements =
      NodeQuery::querySubTree(project, V_SgFormatStatement);
  if (statements.size() != 1) {
    return 1;
  }
  SgFormatStatement *format = isSgFormatStatement(statements.front());
  ASSERT_not_null(format);
  ASSERT_not_null(format->get_format_item_list());
  if (format->get_format_item_list()->get_format_item_list().size() != 3) {
    return 2;
  }

  const std::string text = format->unparseToString();
  if (text.find("'single   spaces'") == std::string::npos) {
    return 3;
  }
  if (text.find("\"double  spaces\"") == std::string::npos) {
    return 4;
  }
  if (text.find("'single spaces'") != std::string::npos ||
      text.find("\"double spaces\"") != std::string::npos) {
    return 5;
  }
  return 0;
}
