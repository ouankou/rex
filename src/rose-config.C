#define CONFIG_NAME "rose-config.cfg"

static const char *purpose = "show ROSE configuration arguments";
static const char *description =
    "Usage: rose-config [--config FILE] KEY\n"
    "Read KEY from " CONFIG_NAME " beside the loaded REX library, or FILE.\n"
    "Configuration files contain KEY = VALUE lines and # comments.\n"
    "Values are printed verbatim.\n"
    "Common keys:\n"
    "  cc, cxx           C and C++ compiler names\n"
    "  cppflags          Preprocessor flags\n"
    "  cflags, cxxflags  C and C++ compiler flags\n"
    "  ldflags           Linker flags\n"
    "  libdirs           Colon-separated library directories\n"
    "  prefix            Installation prefix\n";

#include "rose.h" // POLICY_OK -- this is not a ROSE library source file

#include <cctype>
#include <dlfcn.h>
#include <filesystem>
#include <fstream>

#include <map>
#include <optional>

#include <regex>

#include <string>

#include <vector>

using namespace Rose;

typedef std::map<std::string, std::string> Configuration;

static const std::vector<std::string> &requiredKeys() {
  static const std::vector<std::string> required = {
      "ROSE_CC",       "ROSE_CXX",     "ROSE_CPPFLAGS", "ROSE_CFLAGS",
      "ROSE_CXXFLAGS", "ROSE_LDFLAGS", "ROSE_LIBDIRS",  "ROSE_PREFIX"};
  return required;
}

// Read a specific configuration file
static Configuration readConfigFile(const std::filesystem::path &configName) {
  Configuration retval;
  std::ifstream input(configName);
  if (!input) {
    MLOG_FATAL_CXX("rose-config")
        << "cannot open configuration file: " << configName << "\n";
    exit(1);
  }

  std::regex keyRe("[a-zA-Z][a-zA-Z_0-9]*");
  size_t lineNumber = 0;
  std::string s;
  while (std::getline(input, s)) {
    ++lineNumber;
    s = Rose::StringUtility::trim(s);
    if (s.empty() || '#' == s[0])
      continue;

    // Parse the "key=value" line
    size_t equal = s.find('=');
    std::string key = s.substr(0, equal);
    key = Rose::StringUtility::trim(key);
    std::string value =
        equal == std::string::npos ? std::string() : s.substr(equal + 1);
    value = Rose::StringUtility::trim(value);
    if (equal == std::string::npos || !std::regex_match(key, keyRe)) {
      MLOG_FATAL_CXX("rose-config") << configName << ":" << lineNumber
                                    << ": syntax error: expected KEY = VALUE\n";
      exit(1);
    }

    // Save the key and value for returning later
    if (!retval.insert(std::make_pair(key, value)).second) {
      MLOG_FATAL_CXX("rose-config")
          << configName << ":" << lineNumber << ": duplicate key \""
          << StringUtility::cEscape(key) << "\"\n";
      exit(1);
    }

    // Keep the lowercase command-line aliases for ROSE_* settings.
    if (key.compare(0, 5, "ROSE_") == 0 && key.size() > 5) {
      std::string alias = key.substr(5);
      std::transform(alias.begin(), alias.end(), alias.begin(),
                     [](unsigned char c) { return std::tolower(c); });
      if (!retval.emplace(alias, value).second) {
        MLOG_FATAL_CXX("rose-config")
            << configName << ":" << lineNumber << ": duplicate key \""
            << StringUtility::cEscape(alias) << "\"\n";
        exit(1);
      }
    }
  }

  if (input.bad() || !input.eof()) {
    MLOG_FATAL_CXX("rose-config")
        << configName << ": error reading configuration file\n";
    exit(1);
  }

  bool hadError = false;
  for (const std::string &key : requiredKeys()) {
    if (retval.find(key) == retval.end()) {
      MLOG_FATAL_CXX("rose-config")
          << configName << ":" << lineNumber << ": required key \"" << key
          << "\" is not defined\n";
      hadError = true;
    }
  }
  if (hadError)
    exit(1);

  return retval;
}

static std::filesystem::path defaultConfigFile() {
  Dl_info library{};
  if (dladdr(reinterpret_cast<void *>(&version_message), &library) == 0 ||
      library.dli_fname == nullptr || *library.dli_fname == '\0') {
    MLOG_FATAL_CXX("rose-config") << "cannot locate the loaded REX library\n";
    exit(1);
  }
  return std::filesystem::canonical(library.dli_fname).parent_path() /
         CONFIG_NAME;
}

int main(int argc, char *argv[]) {
  std::optional<std::filesystem::path> configFile;
  std::string key;
  if (argc == 2) { //--help or <key>
    std::string helpflag("--help");
    std::string versionflag("--version");
    if (helpflag.compare(argv[1]) == 0) {
      std::cout << purpose << "\n" << description << "\n";
      exit(0);
    } else if (versionflag.compare(argv[1]) == 0) {
      std::cout << version_message() << "\n";
      exit(0);
    } else {
      key = argv[1];
    }
  } else if (argc == 4) { //--config <file> <key>
    std::string configflag("--config");
    if (configflag.compare(argv[1]) == 0) {
      configFile = std::filesystem::path(argv[2]);
      key = std::string(argv[3]);
    } else {
      MLOG_FATAL_C("rose-config", "incorrect usage; see --help\n");
      exit(1);
    }
  } else { // argc == 1 || argc > 4
    MLOG_FATAL_C("rose-config", "incorrect usage; see --help\n");
    exit(1);
  }
  Configuration config =
      readConfigFile(configFile ? *configFile : defaultConfigFile());

  // Print the value
  Configuration::const_iterator found = config.find(key);
  if (found != config.end()) {
    std::cout << found->second << "\n";
    exit(0);
  }

  // Errors
  MLOG_FATAL_CXX("rose-config")
      << "unknown key \"" << StringUtility::cEscape(key) << "\"\n";
  exit(1);
}
