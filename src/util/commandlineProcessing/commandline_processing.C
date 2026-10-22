#include "rosePublicConfig.h"

#include "rose_config.h"

#include "commandline_processing.h"

#include "mlog.h"

#include <algorithm>

#include <filesystem>

#include <vector>

#include "sla.h"

using namespace std;

Rose_STL_Container<std::string>
    CommandlineProcessing::extraCppSourceFileSuffixes;

//! Convert a vector of string to a single string
std::string CommandlineProcessing::generateStringFromArgList(
    const Rose_STL_Container<std::string> &argList) {
  string result;
  Rose_STL_Container<std::string>::const_iterator iter;
  for (iter = argList.begin(); iter != argList.end(); iter++) {
    if (iter != argList.begin())
      result += " ";
    result += *iter;
  }
  return result;
}

Rose_STL_Container<string>
CommandlineProcessing::generateArgListFromArgcArgv(int argc,
                                                   const char *const argv[]) {
  Rose_STL_Container<string> argList;
  for (int i = 0; i < argc; i++) {
    if (argv[i] != NULL)
      argList.push_back(argv[i]);
  }

  return argList;
}

void CommandlineProcessing::removeArgs(vector<string> &argv, string prefix) {
  for (auto it = argv.begin(); it != argv.end();) {
    if (it->compare(0, prefix.size(), prefix) == 0)
      it = argv.erase(it);
    else
      ++it;
  }
}

void CommandlineProcessing::removeArgsWithParameters(vector<string> &argv,
                                                     string prefix) {
  for (size_t i = 0; i < argv.size();) {
    if (argv[i].compare(0, prefix.size(), prefix) == 0) {
      ROSE_ASSERT(i + 1 < argv.size());
      argv.erase(argv.begin() + i, argv.begin() + i + 2);
    } else
      ++i;
  }
}
//! Remove file names specified in filenameList from argv, except for
//! 'exceptFilename'
void CommandlineProcessing::removeAllFileNamesExcept(
    vector<string> &argv, Rose_STL_Container<std::string> filenameList,
    std::string exceptFilename) {

  for (auto it = argv.begin(); it != argv.end();) {
    if (*it != exceptFilename &&
        std::find(filenameList.begin(), filenameList.end(), *it) !=
            filenameList.end())
      it = argv.erase(it);
    else
      ++it;
  }
}

bool CommandlineProcessing::isOption(vector<string> &argv, string optionPrefix,
                                     string option, bool removeOption) {
  // Use 1 at end of argument list to SLA to force removal of option from argv
  // and decrement of argc
  int optionCount = sla(argv, optionPrefix, "($)", option, (string *)NULL,
                        removeOption ? 1 : -1);
  return (optionCount > 0);
}

bool CommandlineProcessing::isOptionWithParameter(vector<string> &argv,
                                                  string optionPrefix,
                                                  string option,
                                                  int &optionParameter,
                                                  bool removeOption) {
  int localOptionParameter = 0;
  int optionCount = sla(argv, optionPrefix, "($)^", option,
                        &localOptionParameter, removeOption ? 1 : -1);
  if (optionCount > 0)
    optionParameter = localOptionParameter;
  return (optionCount > 0);
}
bool CommandlineProcessing::isOptionWithParameter(vector<string> &argv,
                                                  string optionPrefix,
                                                  string option,
                                                  float &optionParameter,
                                                  bool removeOption) {
  float localOptionParameter = 0;
  int optionCount = sla(argv, optionPrefix, "($)^", option,
                        &localOptionParameter, removeOption ? 1 : -1);
  if (optionCount > 0)
    optionParameter = localOptionParameter;
  return (optionCount > 0);
}

void CommandlineProcessing::addListToCommandLine(
    vector<string> &argv, string prefix, Rose_STL_Container<string> argList) {
  for (size_t i = 0; i < argList.size(); ++i) {
    argv.push_back(prefix + argList[i]);
  }
}

bool CommandlineProcessing::isSourceFilename(string name) {
  static constexpr const char *sourceSuffixes[] = {
      ".c",   ".i",   ".h",   ".C",    ".cc",   ".CC",   ".cp",  ".c++",
      ".C++", ".cpp", ".CPP", ".cxx",  ".CXX",  ".H",    ".hh",  ".hpp",
      ".hxx", ".ii",  ".ccm", ".c++m", ".cppm", ".cxxm", ".iim", ".iih",
      ".cu",  ".cui", ".ocl", ".cl",   ".cli"};

  const string extension = std::filesystem::path(name).extension().string();
  const string suffix = extension.empty() ? "" : extension.substr(1);

  if (CommandlineProcessing::isFortranFileNameSuffix(suffix))
    return true;

  const bool builtInSource =
      std::find(std::begin(sourceSuffixes), std::end(sourceSuffixes),
                extension) != std::end(sourceSuffixes);
  const bool customCppSource =
      std::find(extraCppSourceFileSuffixes.begin(),
                extraCppSourceFileSuffixes.end(),
                suffix) != extraCppSourceFileSuffixes.end();
  return builtInSource || customCppSource;
}

bool CommandlineProcessing::isObjectFilename(string name) {
  const string extension = std::filesystem::path(name).extension().string();
  return extension == ".o" || extension == ".obj";
}

void CommandlineProcessing::addCppSourceFileSuffix(const string &suffix) {
  if (suffix.empty() || suffix.find('.') != string::npos) {
    fprintf(stderr,
            "Invalid C++ source suffix '%s': expected one extension "
            "component without a dot\n",
            suffix.c_str());
    ROSE_ABORT();
  }
  extraCppSourceFileSuffixes.push_back(suffix);
}

bool CommandlineProcessing::isCFileNameSuffix(const std::string &suffix) {
  return suffix == "c" || suffix == "i" || suffix == "h";
}

bool CommandlineProcessing::isAssemblerFileNameSuffix(
    const std::string &suffix) {
  // GCC/Clang conventionally use ".s" for raw assembler and ".S" for
  // assembler that still needs preprocessing.
  return suffix == "s" || suffix == "S";
}

bool CommandlineProcessing::isCppFileNameSuffix(const std::string &suffix) {
  return suffix == "C" || suffix == "cc" || suffix == "CC" || suffix == "cp" ||
         suffix == "c++" || suffix == "C++" || suffix == "cpp" ||
         suffix == "CPP" || suffix == "cxx" || suffix == "CXX" ||
         suffix == "H" || suffix == "hh" || suffix == "hpp" ||
         suffix == "hxx" || suffix == "ii" || suffix == "ccm" ||
         suffix == "c++m" || suffix == "cppm" || suffix == "cxxm" ||
         suffix == "iim" || suffix == "iih" ||
         find(extraCppSourceFileSuffixes.begin(),
              extraCppSourceFileSuffixes.end(),
              suffix) != extraCppSourceFileSuffixes.end();
}

bool CommandlineProcessing::isFortranFileNameSuffix(const std::string &suffix) {
  // Returns true only if this is a valid Fortran source file name extension
  // (suffix)

  // Uppercase Fortran variants request preprocessing.
  return suffix == "f" || suffix == "f77" || suffix == "f90" ||
         suffix == "f95" || suffix == "f03" || suffix == "f08" ||
         suffix == "fxx" || suffix == "caf" || suffix == "F" ||
         suffix == "F77" || suffix == "F90" || suffix == "F95" ||
         suffix == "F03" || suffix == "F08" || suffix == "FXX" ||
         suffix == "CAF" || suffix == "rmod" || suffix == "mod" ||
         suffix == "RMOD" || suffix == "MOD";
}

bool CommandlineProcessing::isFortranFileNameSuffixRequiringCPP(
    const std::string &suffix) {
  // Lowercase Fortran suffixes do not request preprocessing.

  return suffix != "f" && suffix != "f77" && suffix != "f90" &&
         suffix != "f95" && suffix != "f03" && suffix != "f08" &&
         suffix != "fxx" && suffix != "caf" && suffix != "rmod" &&
         suffix != "mod";
}

bool CommandlineProcessing::isFortran77FileNameSuffix(
    const std::string &suffix) {
  // Returns true only if this is a valid Fortran 77 source file name extension
  // (suffix)

  // DQ (12/10/2007): Fortran 77 codes are defined by a *.f suffix, not a *.F77
  // suffix. But to make the system more robust, accept either as recognised F77
  // Fortran source.
  return suffix == "f" || suffix == "F" || suffix == "f77" || suffix == "F77";
}

bool CommandlineProcessing::isFortran90FileNameSuffix(
    const std::string &suffix) {
  // Returns true only if this is a valid Fortran 90 source file name extension
  // (suffix)

  return suffix == "f90" || suffix == "F90" || suffix == "rmod" ||
         suffix == "mod" || suffix == "RMOD" || suffix == "MOD";
}

bool CommandlineProcessing::isFortran95FileNameSuffix(
    const std::string &suffix) {
  // Returns true only if this is a valid Fortran 95 source file name extension
  // (suffix)

  return suffix == "f95" || suffix == "F95";
}

bool CommandlineProcessing::isFortran2003FileNameSuffix(
    const std::string &suffix) {
  // Returns true only if this is a valid Fortran 2003 source file name
  // extension (suffix)

  return suffix == "f03" || suffix == "F03";
}

bool CommandlineProcessing::isFortran2008FileNameSuffix(
    const std::string &suffix) {
  // Returns true only if this is a valid Fortran 2008 source file name
  // extension (suffix)

  return suffix == "f08" || suffix == "F08";
}

bool CommandlineProcessing::isCoArrayFortranFileNameSuffix(
    const std::string &suffix) {
  // Returns true only if this is a valid Co-Array Fortran (extension of Fortran
  // 2003) source file name extension (suffix)

  return suffix == "caf" || suffix == "CAF";
}

bool CommandlineProcessing::isCudaFileNameSuffix(const std::string &suffix) {
  return suffix == "cu" || suffix == "cui";
}

bool CommandlineProcessing::isOpenCLFileNameSuffix(const std::string &suffix) {
  return suffix == "ocl" || suffix == "cl" || suffix == "cli";
}
