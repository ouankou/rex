#include "rose.h"
#include <iostream>
using namespace SageBuilder;
using namespace SageInterface;
using namespace std;

int
main (int argc, char *argv[])
{
  SgProject *project = frontend (argc, argv);
  ROSE_ASSERT (project != NULL);

  Rose_STL_Container<SgNode*> funcRefList = NodeQuery::querySubTree (project,V_SgFunctionRefExp);
  cout << "[DEBUG] Total FunctionRefExp nodes: " << funcRefList.size() << endl;

  Rose_STL_Container<SgNode*>::iterator i = funcRefList.begin();
  while (i != funcRefList.end())
  {
    SgFunctionRefExp * func_ref = isSgFunctionRefExp(*i);
    ROSE_ASSERT (func_ref != NULL);
    SgFunctionSymbol* func_sym = isSgFunctionSymbol(func_ref->get_symbol_i());
    SgName functionName = func_sym->get_name();

    cout << "[DEBUG] FunctionRef: " << functionName.getString();
    if (func_ref->get_parent()) {
        cout << " parent: " << func_ref->get_parent()->class_name();
    } else {
        cout << " parent: NULL";
    }
    cout << endl;

    if (functionName == "fooA")
    {
      SgFunctionCallExp * func_call = isSgFunctionCallExp(func_ref->get_parent());
      cout << "[DEBUG] fooA parent cast result: " << (void*)func_call << endl;
      if (func_call != NULL) {
          insertBeforeUsingCommaOp(buildVarRefExp("a", getScope(func_ref)), func_call);
      } else {
          cerr << "[ERROR] fooA function reference parent is not SgFunctionCallExp!" << endl;
      }
    }
    i++;
  }

  return backend (project);
}
