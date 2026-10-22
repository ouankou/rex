
#include "sage3basic.h"

#include "unparse_debug.h"

#include "unparser.h"

using namespace std;

void Unparse_Debug::printDebugInfo(const std::string &debug_info,
                                   bool newline) {
  if (unp->opt.get_debug_opt()) {
    cout << debug_info;
    if (newline)
      cout << endl;
  }
}

void Unparse_Debug::printDebugInfo(int debug_info, bool newline) {
  if (unp->opt.get_debug_opt()) {
    cout << debug_info;
    if (newline)
      cout << endl;
  }
}
