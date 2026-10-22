#include <iostream>

#include <memory>

#include <dlfcn.h>
#include <stdlib.h>

#include "mlog.h"
#include <string>

#include "plugin.h"
using namespace std;

namespace Rose {

std::vector<std::string> PluginLibs; // one or more plugin shared libraries
std::vector<std::string>
    PluginActions; // one or more actions defined in the shared libs
std::map<std::string, std::vector<std::string>> PluginArgs; // plugin arguments

template class Registry<PluginAction>;

static bool extractOptionWithParameter(vector<string> &argv,
                                       const string &option, string &value) {
  for (size_t i = 0; i < argv.size(); i++) {
    if (argv[i] == option) {
      ASSERT_require(i + 1 < argv.size());
      value = argv[i + 1];
      argv.erase(argv.begin() + i, argv.begin() + i + 2);
      return true;
    }
  }
  return false;
}

void processPluginCommandLine(vector<string> &argv) {
  string lib_name;
  while (extractOptionWithParameter(argv, "-rose:plugin_lib", lib_name)) {
    PluginLibs.push_back(lib_name);
  }

  string act_name;
  while (extractOptionWithParameter(argv, "-rose:plugin_action", act_name)) {
    PluginActions.push_back(act_name);
  }

  for (const string &action : PluginActions) {
    vector<string> args;
    string arg;
    while (
        extractOptionWithParameter(argv, "-rose:plugin_arg_" + action, arg)) {
      args.push_back(arg);
    }
    PluginArgs[action] = args;
  }
}

int obtainAndExecuteActions(SgProject *n) {
  int res = 0;
  // load the shared libraries
  for (size_t i = 0; i < PluginLibs.size(); i++) {
    const char *lib_file_name = PluginLibs[i].c_str();
    void *handle = dlopen(lib_file_name, RTLD_LAZY | RTLD_GLOBAL);

    if (!handle) {
      char *errKode = dlerror();
      ASSERT_require(errKode != NULL);
      cout << "Error in dlopen: error code: " << errKode << " when loading "
           << lib_file_name << endl;
      exit(1);
    }
  }

  // 4. Iterate through the registered plugins
  for (size_t i = 0; i < PluginActions.size(); i++) {
    string action_name = PluginActions[i];
    for (PluginRegistry::iterator it = PluginRegistry::begin();
         it != PluginRegistry::end(); ++it) {
      // find an action matching the action name
      if (it->getName() == action_name) {
        std::unique_ptr<PluginAction> p_action(it->instantiate());

        // call command line parsing for the plugin
        if (!p_action->ParseArgs(PluginArgs[action_name]))
          return 0;
        p_action->process(n);
        res++;
      }
    } // end for registry
  } // end for actions

  return res;
}
} // namespace Rose
