#ifndef ROSE_COMMANDLINE_UTILITY_H
#define ROSE_COMMANDLINE_UTILITY_H
#include "rosedll.h"

#include <string>

#include <vector>
#define Rose_STL_Container std::vector

//! Command line processing utility functions. Functions in this namespace are
//! in the ROSE Utilities library and
//  therefore are unable to call other functions in the ROSE library. See also,
//  Rose::CommandLine for higher-level functions.
namespace CommandlineProcessing {
/** Convert a vector of string to a single string. */
ROSE_UTIL_API std::string
generateStringFromArgList(const Rose_STL_Container<std::string> &argList);

/** Convert an argc-argv pair into a string vector.
 *
 * @param argc Argument count.
 * @param argv Argument vector.
 * @return Parsed arguments. */
ROSE_UTIL_API Rose_STL_Container<std::string>
generateArgListFromArgcArgv(int argc, const char *const argv[]);

/** RAII storage for argv-style arguments. */
class ArgvStorage {
public:
  explicit ArgvStorage(const Rose_STL_Container<std::string> &args)
      : storage_(args) {
    argv_.reserve(storage_.size() + 1);
    for (auto &arg : storage_) {
      argv_.push_back(arg.data());
    }
    argv_.push_back(nullptr);
  }

  ArgvStorage(const ArgvStorage &) = delete;
  ArgvStorage &operator=(const ArgvStorage &) = delete;

  int argc() const { return static_cast<int>(storage_.size()); }
  char **argv() { return argv_.data(); }

private:
  Rose_STL_Container<std::string> storage_;
  std::vector<char *> argv_;
};

extern Rose_STL_Container<std::string> extraCppSourceFileSuffixes;

/** Search 'argv' for an option like optionPrefixOption.
 *
 * The argument 'option' adds () to the actual option, and allows the |(OR)
 * operations.
 *
 * @param argv Argument vector to search.
 * @param optionPrefix Prefix for the option.
 * @param Option Option name or regex.
 * @param removeOption Whether to remove the option from argv.
 * @return True if the option was found. */
ROSE_UTIL_API bool isOption(std::vector<std::string> &argv,
                            std::string optionPrefix, std::string Option,
                            bool removeOption);

/** Search 'argv' for 'optionPrefixOption value' (int).
 *
 * @param argv Argument vector to search.
 * @param optionPrefix Prefix for the option.
 * @param Option Option name or regex.
 * @param optionParameter Receives the parsed value.
 * @param removeOption Whether to remove the option from argv.
 * @return True if the option was found. */
ROSE_UTIL_API bool isOptionWithParameter(std::vector<std::string> &argv,
                                         std::string optionPrefix,
                                         std::string Option,
                                         int &optionParameter,
                                         bool removeOption);

/** Search 'argv' for 'optionPrefixOption value' (float).
 *
 * @param argv Argument vector to search.
 * @param optionPrefix Prefix for the option.
 * @param Option Option name or regex.
 * @param optionParameter Receives the parsed value.
 * @param removeOption Whether to remove the option from argv.
 * @return True if the option was found. */
ROSE_UTIL_API bool isOptionWithParameter(std::vector<std::string> &argv,
                                         std::string optionPrefix,
                                         std::string Option,
                                         float &optionParameter,
                                         bool removeOption);

/** Search 'argv' for 'optionPrefixOption value' (string).
 *
 * @param argv Argument vector to search.
 * @param optionPrefix Prefix for the option.
 * @param Option Option name or regex.
 * @param optionParameter Receives the parsed value.
 * @param removeOption Whether to remove the option from argv.
 * @return True if the option was found. */
ROSE_DLL_API bool isOptionWithParameter(std::vector<std::string> &argv,
                                        std::string optionPrefix,
                                        std::string Option,
                                        std::string &optionParameter,
                                        bool removeOption);

/** Add the strings in argList to the command line.
 *
 * @param argv Argument vector to append to.
 * @param prefix Prefix to prepend to each argument.
 * @param argList Arguments to add. */
ROSE_UTIL_API void
addListToCommandLine(std::vector<std::string> &argv, std::string prefix,
                     Rose_STL_Container<std::string> argList);
/** Remove all options matching a specified prefix.
 *
 * @param argv Argument vector to edit.
 * @param prefix Prefix to remove. */
ROSE_UTIL_API void removeArgs(std::vector<std::string> &argv,
                              std::string prefix);
/** Remove all options matching a specified prefix along with their values.
 *
 * @param argv Argument vector to edit.
 * @param prefix Prefix to remove. */
ROSE_UTIL_API void removeArgsWithParameters(std::vector<std::string> &argv,
                                            std::string prefix);
/** Remove file names specified in filenameList from argv.
 *
 * @param argv Argument vector to edit.
 * @param filenameList File names to remove.
 * @param exceptFilename File name to keep. */
ROSE_UTIL_API void
removeAllFileNamesExcept(std::vector<std::string> &argv,
                         Rose_STL_Container<std::string> filenameList,
                         std::string exceptFilename);

/** Exact language family selected by Clang's ordered `-x` driver state. */
enum class ClangLanguageFamily {
  Suffix,
  C,
  Cxx,
  Cuda,
  OpenCL,
};

/** Language state active at one exact driver input operand.
 *
 * `driverName` is empty for suffix-driven input and otherwise contains the
 * exact Clang language name to pass after `-x`.  The REX spelling `opencl` is
 * canonicalized to Clang's `cl` spelling.
 */
struct ClangLanguageSelection {
  ClangLanguageFamily family = ClangLanguageFamily::Suffix;
  std::string driverName;

  bool isExplicit() const { return family != ClangLanguageFamily::Suffix; }
};

/** Validate every split or joined Clang `-x` option in argument order. */
ROSE_DLL_API void
validateClangLanguageOptions(const std::vector<std::string> &argv);

/** Return the ordered `-x` state active at one exact source operand.
 *
 * The source must occur exactly once as a positional driver input.  Option
 * operands with the same spelling do not count as source occurrences.
 */
ROSE_DLL_API ClangLanguageSelection clangLanguageSelectionForSource(
    const std::vector<std::string> &argv, const std::string &source);

/** Return the exact Clang driver language selected by a supported suffix.
 *
 * The result is explicit for every suffix-driven C, C++, CUDA, or OpenCL
 * source accepted by REX. Unsupported or suffixless inputs are hard errors.
 */
ROSE_DLL_API ClangLanguageSelection
clangLanguageSelectionForSuffix(const std::string &source);

/** Build a per-translation-unit argv without changing ordered option state.
 *
 * Only positional source operands other than `source` are removed.  In
 * particular, option operands, linker inputs, `--`, and all ordered `-x`
 * switches are retained exactly.
 */
ROSE_DLL_API std::vector<std::string>
sliceCommandLineForSource(const std::vector<std::string> &argv,
                          const Rose_STL_Container<std::string> &sourceFiles,
                          const std::string &source);

/** Build the list of isolated file names from the command line.
 *
 * @param argList Argument list.
 * @return Source file names. */
ROSE_DLL_API Rose_STL_Container<std::string>
generateSourceFilenames(const Rose_STL_Container<std::string> &argList);

/** Enforce the final per-translation-unit backend compile contract.
 *
 * The command must be compile-only, contain exactly one source operand (the
 * expected source), and contain exactly one structurally valid `-o OUTPUT`
 * pair.  Values owned by options such as `-include` are not source operands,
 * even when their suffix looks like source code.
 *
 * Violations are internal compiler errors and abort immediately.
 *
 * @param argList Final backend command, including the compiler executable.
 * @param expectedSource Exact source operand owned by this translation unit.
 */
ROSE_DLL_API void validateBackendCompileOnlyCommandLine(
    const Rose_STL_Container<std::string> &argList,
    const std::string &expectedSource);

/** Add another valid C++ source file extension without a leading dot.
 *
 * @param suffix Nonempty extension component to add. */
ROSE_UTIL_API void addCppSourceFileSuffix(const std::string &suffix);

ROSE_UTIL_API bool isSourceFilename(std::string name);

ROSE_UTIL_API bool isObjectFilename(std::string name);
ROSE_UTIL_API bool isCFileNameSuffix(const std::string &suffix);
ROSE_UTIL_API bool isAssemblerFileNameSuffix(const std::string &suffix);

ROSE_UTIL_API bool isCppFileNameSuffix(const std::string &suffix);

ROSE_UTIL_API bool isFortranFileNameSuffix(const std::string &suffix);

ROSE_UTIL_API bool
isFortranFileNameSuffixRequiringCPP(const std::string &suffix);

ROSE_UTIL_API bool isFortran77FileNameSuffix(const std::string &suffix);
ROSE_UTIL_API bool isFortran90FileNameSuffix(const std::string &suffix);
ROSE_UTIL_API bool isFortran95FileNameSuffix(const std::string &suffix);
ROSE_UTIL_API bool isFortran2003FileNameSuffix(const std::string &suffix);
ROSE_UTIL_API bool isFortran2008FileNameSuffix(const std::string &suffix);

ROSE_UTIL_API bool isCoArrayFortranFileNameSuffix(const std::string &suffix);

ROSE_UTIL_API bool isCudaFileNameSuffix(const std::string &suffix);
ROSE_UTIL_API bool isOpenCLFileNameSuffix(const std::string &suffix);

ROSE_DLL_API bool isOptionTakingSecondParameter(std::string argument);
ROSE_DLL_API bool isOptionTakingThirdParameter(std::string argument);
}; // namespace CommandlineProcessing

/** Find the path of a ROSE support file from the source tree.
 *
 * For a build-tree library, resolve sourceTreeLocation under the source tree.
 * For an installed library, resolve installTreeLocation under the installation
 * prefix (see roseInstallPrefix()).
 *
 * @param sourceTreeLocation Relative path inside the source tree.
 * @param installTreeLocation Relative path inside the install tree.
 * @return Resolved support path. */
ROSE_DLL_API std::string
findRoseSupportPathFromSource(const std::string &sourceTreeLocation,
                              const std::string &installTreeLocation);

/** Find the path of a ROSE support file from the build tree.
 *
 * For a build-tree library, resolve buildTreeLocation under the build tree.
 * For an installed library, resolve installTreeLocation under the installation
 * prefix (see roseInstallPrefix()).
 *
 * @param buildTreeLocation Relative path inside the build tree.
 * @param installTreeLocation Relative path inside the install tree.
 * @return Resolved support path. */
ROSE_DLL_API std::string
findRoseSupportPathFromBuild(const std::string &buildTreeLocation,
                             const std::string &installTreeLocation);

/** Find the path of the ROSE install prefix.
 *
 * There is an assumption that <directory containing librose>/.. is the prefix,
 * and that other things can be found from that. This may not be true if the
 * various install directories are set by hand (rather than from $prefix). This
 * function either puts the prefix into RESULT and returns true (for an
 * installed copy of ROSE), or returns false (for a build tree).
 *
 * @param result Receives the install prefix.
 * @return True for installed ROSE, false for a build tree. */
ROSE_DLL_API bool roseInstallPrefix(std::string &result);

#endif
