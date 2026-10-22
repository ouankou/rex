/**
 * \file    cmdline.C
 * \author  Justin Too <too1@llnl.gov>
 * \date    April 4, 2012
 */

/*-----------------------------------------------------------------------------
 *  Dependencies
 *---------------------------------------------------------------------------*/
#include <cstring>
#include <filesystem>
#include <iterator>

#include "Rose/StringUtility/FileUtility.h"

#include "Rose/StringUtility/Replace.h"

#include "cmdline.h"

#include "omp_simd.h"

#include "rex-backend-link-options.h"
#include "rose_config.h"
#include "sage3basic.h"

#include "rose_path_resolver.h"
#include "rose_test_output_path.h"

#include "clang-include-option.h"

#include "Outliner.hh"

using namespace Rose; // temporary, until this file lives in namespace Rose
using namespace std;

#include <inttypes.h> /* for %" PRIuPTR " vs. %Iu handling */

static bool isSplitClangTargetOption(const std::string &arg) {
  return arg == "-target" || arg == "-isysroot" || arg == "--sysroot" ||
         arg == "-resource-dir";
}

/*-----------------------------------------------------------------------------
 *  Variable Definitions
 *---------------------------------------------------------------------------*/
ROSE_DLL_API int Rose::Cmdline::verbose = 0;

/*-----------------------------------------------------------------------------
 *  namespace CommandlineProcessing {
 *---------------------------------------------------------------------------*/
bool CommandlineProcessing::isOptionWithParameter(vector<string> &argv,
                                                  string optionPrefix,
                                                  string option,
                                                  string &optionParameter,
                                                  bool removeOption) {
  string localString = "";
  int optionCount = sla(argv, optionPrefix, "($)^", option, &localString,
                        removeOption ? 1 : -1);

  if (optionCount > 0)
    optionParameter = localString;

  return (optionCount > 0);
}

bool CommandlineProcessing::isOptionTakingSecondParameter(string argument) {
  bool result = false;
  if (argument == "-o" ||   // Used to specify output file to compiler
      argument == "-opt" || // Used in loopProcessor
      // Compiler options whose separate value can have a source-code suffix.
      // Keep this classification centralized so frontend input discovery and
      // final backend command validation agree about operand ownership.
      argument == "-I" || argument == "-F" || argument == "-L" ||
      argument == "-B" || argument == "-D" || argument == "-U" ||
      argument == "-J" || argument == "-MJ" ||
      argument == "-fintrinsic-modules-path" || argument == "-imacros" ||
      argument == "-idirafter" || argument == "-iquote" ||
      argument == "-iframework" || argument == "-iframeworkwithsysroot" ||
      argument == "-include-pch" || argument == "-isystem-after" ||
      argument == "-stdlib++-isystem" || argument == "-serialize-diagnostics" ||
      argument == "-working-directory" || argument == "-Xclang" ||
      argument == "-Xpreprocessor" || argument == "-Xassembler" ||
      argument == "-Xlinker" || argument == "-Xcuda-ptxas" ||
      argument == "-mllvm" ||
      // DQ (1/13/2009): This option should only have a single leading "-",
      // not two. argument == "--include" ||                        // Used
      // for preinclude list (to include some header files before all others,
      // common requirement for compiler)
      argument == "-include" || // Used for preinclude file list (to include
                                // some header files before all others,
                                // common requirement for compiler)
      argument == "-isystem" || // Used for preinclude directory list (to
                                // specify include paths to be search before all
                                // others, common requirement for compiler)
      isSplitClangTargetOption(argument) ||

      // ROSE options
      argument == "-rose:output" || // Used to specify output file to ROSE
      argument == "-rose:o" ||      // Used to specify output file to ROSE
                                    // (alternative to -rose:output)
      argument ==
          "-rose:compilationPerformanceFile" || // Use to output performance
                                                // information about ROSE
                                                // compilation phases
      argument == "-rose:verbose" || // Used to specify output of internal
                                     // information about ROSE phases
      argument == "-rose:backendCompileFormat" ||
      argument == "-rose:outputFormat" ||
      argument == "-rose:includeCommentsAndDirectives" ||
      argument == "-rose:includeCommentsAndDirectivesFrom" ||
      argument == "-rose:excludeCommentsAndDirectives" ||
      argument == "-rose:excludeCommentsAndDirectivesFrom" ||
      argument == "-rose:includePath" || argument == "-rose:excludePath" ||
      argument == "-rose:includeFile" || argument == "-rose:excludeFile" ||

      // negara1 (08/16/2011)
      argument == "-rose:unparseHeaderFilesRootFolder" ||

      // DQ (11/6/2018): Adding support to specify the root directory of an
      // application for header file unparsing and token based unparsing).
      argument == "-rose:applicationRootDirectory" ||

      // DQ (8/20/2008): Add support for Qing's options!
      argument == "-annot" || argument == "-bs" ||
      isOptionTakingThirdParameter(argument) ||

      // AS (02/20/08):  When used with -M or -MM, -MF specifies a file to
      // write the dependencies to. Need to tell ROSE to ignore that output
      // paramater
      argument == "-MF" || argument == "-MT" || argument == "-MQ" ||
      argument == "-outputdir" || // FMZ (12/22/1009) added for caf compiler

      // DQ (9/26/2011): Added support for detection of dangling pointers
      // within translators built using ROSE.
      argument == "-rose:detect_dangling_pointers" || // Used to specify level
                                                      // of debugging support
                                                      // for optional detection
                                                      // of dangling pointers

      // DQ (1/16/2012): Added all of the currently defined dot file options.
      argument == "-rose:dotgraph:commentAndDirectiveFilter" ||
      argument == "-rose:dotgraph:ctorInitializerListFilter" ||
      argument == "-rose:dotgraph:defaultColorFilter" ||
      argument == "-rose:dotgraph:defaultFilter" ||
      argument == "-rose:dotgraph:edgeFilter" ||
      argument == "-rose:dotgraph:emptySymbolTableFilter" ||

      // DQ (7/22/2012): Added support to ignore some specific empty IR
      // nodes.
      argument == "-rose:dotgraph:emptyBasicBlockFilter" ||
      argument == "-rose:dotgraph:emptyFunctionParameterListFilter" ||

      argument == "-rose:dotgraph:expressionFilter" ||
      argument == "-rose:dotgraph:fileInfoFilter" ||
      argument == "-rose:dotgraph:frontendCompatibilityFilter" ||
      argument == "-rose:dotgraph:symbolFilter" ||
      argument == "-rose:dotgraph:typeFilter" ||
      argument == "-rose:dotgraph:variableDeclarationFilter" ||
      argument == "-rose:dotgraph:noFilter" ||

      // DQ (1/8/2014): We need the "-x" option which takes a single option
      // to specify the language "c" or "c++". This is required where within
      // the "git" build system the input file is "/dev/null" which does not
      // have a suffix from which to compute the associated language.
      argument == "-x" ||

      // DQ (1/20/2014): Adding support for gnu's -undefined option.
      argument == "-u" || argument == "-undefined" ||

      // DQ (1/26/2014): Support for usage such as -version-info 8:9:8
      argument == "-version-info" ||

      // DQ (12/10/2016): This does not take a parameter on any later version
      // compiler that I know of. DQ (1/26/2014): Support for make dependence
      // option -MM <file name for dependence info> argument == "-MM" ||

      // DQ (3/25/2014): We need the icpc/icc [-fp-model <arg>]  command-line
      // compiler option to be passed to the backend compiler properly.  The
      // [-fp-model] option always has a single argument.
      argument == "-fp-model" ||

      // DQ (1/21/2015): -diag-disable can take a collection of optional
      // parameters: e.g. cpu-dispatch
      argument == "-diag-enable" || argument == "-diag-disable" ||
      argument == "-diag-error" || argument == "-diag-warning" ||
      argument == "-diag-remark" ||

      // TOO1 (5/14/2015): Add support for GCC --param, e.g. "--param
      // inline-unit-growth=900" found in Valgrind
      argument == "--param" || // --param variable=value

      // Peihung (03/09/2020): Add support for ifort v.19
      argument == "-align" || argument == "-warn" || argument == "-check" ||
      argument == "-debug" || argument == "-debug-parameters") {
    result = true;
  }

  return result;
}

bool CommandlineProcessing::isOptionTakingThirdParameter(string argument) {
  return (argument == "-unroll");
}

namespace {

[[noreturn]] void rejectClangLanguageName(const std::string &name) {
  fprintf(stderr,
          "REX_FRONTEND_INVARIANT[clang-language]: explicit -x language '%s' "
          "is unsupported; expected a supported C, C++, CUDA, or OpenCL "
          "Clang 22 input type, or none\n",
          name.c_str());
  ROSE_ABORT();
}

CommandlineProcessing::ClangLanguageSelection
parseClangLanguageName(const std::string &name) {
  using CommandlineProcessing::ClangLanguageFamily;
  using CommandlineProcessing::ClangLanguageSelection;

  if (name == "none") {
    return {ClangLanguageFamily::Suffix, ""};
  }
  if (name == "c" || name == "c-header" || name == "cpp-output") {
    return {ClangLanguageFamily::C, name};
  }
  if (name == "c++" || name == "c++-header" || name == "c++-cpp-output" ||
      name == "c++-header-unit-cpp-output" || name == "c++-module") {
    return {ClangLanguageFamily::Cxx, name};
  }
  if (name == "cuda" || name == "cuda-cpp-output") {
    return {ClangLanguageFamily::Cuda, name};
  }
  if (name == "cl" || name == "opencl" || name == "cl-cpp-output") {
    return {ClangLanguageFamily::OpenCL, name == "opencl" ? "cl" : name};
  }
  rejectClangLanguageName(name);
}

[[noreturn]] void rejectMissingCommandLineOperand(const std::string &option) {
  fprintf(stderr,
          "REX_FRONTEND_INVARIANT[clang-option-operand]: option '%s' requires "
          "an argument\n",
          option.c_str());
  ROSE_ABORT();
}

[[noreturn]] void rejectClangStdinInput() {
  fprintf(stderr,
          "REX_FRONTEND_INVARIANT[clang-source-path]: stdin input '-' has no "
          "stable source identity and is unsupported\n");
  ROSE_ABORT();
}

size_t commandLineOperandCount(const std::string &option) {
  return CommandlineProcessing::isOptionTakingThirdParameter(option) ? 2 : 1;
}

bool sameCommandLineSource(const std::string &left, const std::string &right) {
  if (left == right) {
    return true;
  }
  // Unrelated linker inputs need not exist; only existing entries can share
  // a physical source identity. Other filesystem errors propagate.
  if (!std::filesystem::exists(left) || !std::filesystem::exists(right)) {
    return false;
  }
  return StringUtility::getAbsolutePathFromRelativePath(left) ==
         StringUtility::getAbsolutePathFromRelativePath(right);
}

bool isPositionalCommandLineArgument(const std::string &argument,
                                     bool afterDoubleDash) {
  return afterDoubleDash || argument.empty() || argument == "-" ||
         argument.front() != '-';
}

bool isUnsupportedObjectiveCSource(const std::string &argument) {
  const std::string suffix = StringUtility::fileNameSuffix(argument);
  return suffix == "m" || suffix == "M" || suffix == "mi" || suffix == "mm" ||
         suffix == "mii";
}

bool isUnsupportedOpenCLCxxSource(const std::string &argument) {
  const std::string suffix = StringUtility::fileNameSuffix(argument);
  return suffix == "clcpp" || suffix == "clii";
}

[[noreturn]] void
rejectUnsupportedObjectiveCSource(const std::string &argument) {
  fprintf(stderr,
          "REX_FRONTEND_INVARIANT[clang-language]: Objective-C source '%s' "
          "is not supported by REX\n",
          argument.c_str());
  ROSE_ABORT();
}

[[noreturn]] void
rejectUnsupportedOpenCLCxxSource(const std::string &argument) {
  fprintf(stderr,
          "REX_FRONTEND_INVARIANT[clang-language]: OpenCL C++ source '%s' "
          "is not supported by REX\n",
          argument.c_str());
  ROSE_ABORT();
}

} // namespace

void CommandlineProcessing::validateClangLanguageOptions(
    const std::vector<std::string> &argv) {
  bool afterDoubleDash = false;
  for (size_t i = argv.empty() ? 0 : 1; i < argv.size(); ++i) {
    const std::string &argument = argv[i];
    if (afterDoubleDash) {
      continue;
    }
    if (argument == "--") {
      afterDoubleDash = true;
      continue;
    }
    if (argument == "-x") {
      if (++i >= argv.size()) {
        rejectMissingCommandLineOperand(argument);
      }
      parseClangLanguageName(argv[i]);
      continue;
    }
    if (argument.rfind("-x", 0) == 0 && argument.size() > 2) {
      parseClangLanguageName(argument.substr(2));
      continue;
    }
    if (isOptionTakingSecondParameter(argument)) {
      const size_t operandCount = commandLineOperandCount(argument);
      if (operandCount >= argv.size() - i) {
        rejectMissingCommandLineOperand(argument);
      }
      i += operandCount;
    }
  }
}

CommandlineProcessing::ClangLanguageSelection
CommandlineProcessing::clangLanguageSelectionForSource(
    const std::vector<std::string> &argv, const std::string &source) {
  if (argv.empty()) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[clang-command]: command has no driver "
            "executable\n");
    ROSE_ABORT();
  }
  if (source.empty()) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[clang-source-identity]: requested source "
            "path is empty\n");
    ROSE_ABORT();
  }
  if (source == "-") {
    rejectClangStdinInput();
  }

  ClangLanguageSelection active;
  ClangLanguageSelection selected;
  size_t sourceCount = 0;
  bool afterDoubleDash = false;
  for (size_t i = 1; i < argv.size(); ++i) {
    const std::string &argument = argv[i];
    if (!afterDoubleDash && argument == "--") {
      afterDoubleDash = true;
      continue;
    }
    if (!afterDoubleDash && argument == "-x") {
      if (++i >= argv.size()) {
        rejectMissingCommandLineOperand(argument);
      }
      active = parseClangLanguageName(argv[i]);
      continue;
    }
    if (!afterDoubleDash && argument.rfind("-x", 0) == 0 &&
        argument.size() > 2) {
      active = parseClangLanguageName(argument.substr(2));
      continue;
    }
    if (!afterDoubleDash && isOptionTakingSecondParameter(argument)) {
      const size_t operandCount = commandLineOperandCount(argument);
      if (operandCount >= argv.size() - i) {
        rejectMissingCommandLineOperand(argument);
      }
      i += operandCount;
      continue;
    }
    if (isPositionalCommandLineArgument(argument, afterDoubleDash) &&
        sameCommandLineSource(argument, source)) {
      selected = active;
      ++sourceCount;
    }
  }

  if (sourceCount != 1) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[clang-source-identity]: source '%s' must "
            "occur exactly once as a positional driver input (count=%zu)\n",
            source.c_str(), sourceCount);
    ROSE_ABORT();
  }
  return selected;
}

CommandlineProcessing::ClangLanguageSelection
CommandlineProcessing::clangLanguageSelectionForSuffix(
    const std::string &source) {
  const std::string suffix = StringUtility::fileNameSuffix(source);

  if (suffix == "c") {
    return {ClangLanguageFamily::C, "c"};
  }
  if (suffix == "i") {
    return {ClangLanguageFamily::C, "cpp-output"};
  }
  if (suffix == "h") {
    return {ClangLanguageFamily::C, "c-header"};
  }

  static const char *cxxSourceSuffixes[] = {"C",   "cc",  "CC",  "cp",  "c++",
                                            "C++", "cpp", "CPP", "cxx", "CXX"};
  if (std::find(std::begin(cxxSourceSuffixes), std::end(cxxSourceSuffixes),
                suffix) != std::end(cxxSourceSuffixes)) {
    return {ClangLanguageFamily::Cxx, "c++"};
  }
  static const char *cxxHeaderSuffixes[] = {"H", "hh", "hpp", "hxx"};
  if (std::find(std::begin(cxxHeaderSuffixes), std::end(cxxHeaderSuffixes),
                suffix) != std::end(cxxHeaderSuffixes)) {
    return {ClangLanguageFamily::Cxx, "c++-header"};
  }
  if (suffix == "ii") {
    return {ClangLanguageFamily::Cxx, "c++-cpp-output"};
  }
  static const char *cxxModuleSuffixes[] = {"ccm", "c++m", "cppm", "cxxm"};
  if (std::find(std::begin(cxxModuleSuffixes), std::end(cxxModuleSuffixes),
                suffix) != std::end(cxxModuleSuffixes)) {
    return {ClangLanguageFamily::Cxx, "c++"};
  }
  if (suffix == "iim") {
    return {ClangLanguageFamily::Cxx, "c++-cpp-output"};
  }
  if (suffix == "iih") {
    return {ClangLanguageFamily::Cxx, "c++-header-unit-cpp-output"};
  }

  if (suffix == "cu") {
    return {ClangLanguageFamily::Cuda, "cuda"};
  }
  if (suffix == "cui") {
    return {ClangLanguageFamily::Cuda, "cuda-cpp-output"};
  }
  if (suffix == "ocl" || suffix == "cl") {
    return {ClangLanguageFamily::OpenCL, "cl"};
  }
  if (suffix == "cli") {
    return {ClangLanguageFamily::OpenCL, "cl-cpp-output"};
  }

  fprintf(stderr,
          "REX_FRONTEND_INVARIANT[clang-suffix-language]: source '%s' has no "
          "supported exact Clang suffix language\n",
          source.c_str());
  ROSE_ABORT();
}

std::vector<std::string> CommandlineProcessing::sliceCommandLineForSource(
    const std::vector<std::string> &argv,
    const Rose_STL_Container<std::string> &sourceFiles,
    const std::string &source) {
  (void)clangLanguageSelectionForSource(argv, source);

  std::vector<std::string> result;
  result.reserve(argv.size());
  if (argv.empty()) {
    return result;
  }
  result.push_back(argv.front());

  bool afterDoubleDash = false;
  for (size_t i = 1; i < argv.size(); ++i) {
    const std::string &argument = argv[i];
    if (!afterDoubleDash && argument == "--") {
      afterDoubleDash = true;
      result.push_back(argument);
      continue;
    }
    if (!afterDoubleDash && argument == "-x") {
      result.push_back(argument);
      if (++i >= argv.size()) {
        rejectMissingCommandLineOperand(argument);
      }
      parseClangLanguageName(argv[i]);
      result.push_back(argv[i]);
      continue;
    }
    if (!afterDoubleDash && argument.rfind("-x", 0) == 0 &&
        argument.size() > 2) {
      parseClangLanguageName(argument.substr(2));
      result.push_back(argument);
      continue;
    }
    if (!afterDoubleDash && isOptionTakingSecondParameter(argument)) {
      result.push_back(argument);
      const size_t operandCount = commandLineOperandCount(argument);
      if (operandCount >= argv.size() - i) {
        rejectMissingCommandLineOperand(argument);
      }
      for (size_t operand = 0; operand < operandCount; ++operand) {
        result.push_back(argv[++i]);
      }
      continue;
    }

    bool isSource = false;
    if (isPositionalCommandLineArgument(argument, afterDoubleDash)) {
      isSource =
          std::any_of(sourceFiles.begin(), sourceFiles.end(),
                      [&](const std::string &candidate) {
                        return sameCommandLineSource(argument, candidate);
                      });
    }
    if (!isSource || sameCommandLineSource(argument, source)) {
      result.push_back(argument);
    }
  }
  return result;
}

// DQ (1/16/2008): This function was moved from the commandling_processing.C
// file for centralized handling.
Rose_STL_Container<string> CommandlineProcessing::generateSourceFilenames(
    const Rose_STL_Container<string> &argList) {
  Rose_STL_Container<string> sourceFileList;
  ASSERT_require(argList.size() > 0);

  ClangLanguageSelection activeLanguage;
  bool afterDoubleDash = false;
  for (size_t i = 1; i < argList.size(); ++i) {
    const std::string &argument = argList[i];
    if (!afterDoubleDash && argument == "--") {
      afterDoubleDash = true;
      continue;
    }
    if (!afterDoubleDash && argument == "-x") {
      if (++i >= argList.size()) {
        rejectMissingCommandLineOperand(argument);
      }
      activeLanguage = parseClangLanguageName(argList[i]);
      continue;
    }
    if (!afterDoubleDash && argument.rfind("-x", 0) == 0 &&
        argument.size() > 2) {
      activeLanguage = parseClangLanguageName(argument.substr(2));
      continue;
    }
    if (!afterDoubleDash && isOptionTakingSecondParameter(argument)) {
      const size_t operandCount = commandLineOperandCount(argument);
      if (operandCount >= argList.size() - i) {
        rejectMissingCommandLineOperand(argument);
      }
      i += operandCount;
      continue;
    }
    if (!isPositionalCommandLineArgument(argument, afterDoubleDash)) {
      continue;
    }
    if (argument == "-") {
      rejectClangStdinInput();
    }

    // An explicit -x mode makes every following driver input a source until
    // `-x none` resets suffix-based classification, exactly as Clang does.
    if (activeLanguage.isExplicit()) {
      sourceFileList.push_back(argument);
      continue;
    }

    if (isUnsupportedObjectiveCSource(argument)) {
      rejectUnsupportedObjectiveCSource(argument);
    }
    if (isUnsupportedOpenCLCxxSource(argument)) {
      rejectUnsupportedOpenCLCxxSource(argument);
    }

    if (!isObjectFilename(argument) && isSourceFilename(argument)) {
      sourceFileList.push_back(argument);
    }
  }

  if (SgProject::get_verbose() > 1) {
    printf("sourceFileList = %s \n",
           StringUtility::listToString(sourceFileList).c_str());
    printf("######################### Leaving of "
           "CommandlineProcessing::generateSourceFilenames() "
           "############################ \n");
  }

  return sourceFileList;
}

void CommandlineProcessing::validateBackendCompileOnlyCommandLine(
    const Rose_STL_Container<string> &argList, const string &expectedSource) {
  auto fail = [&](const char *reason) {
    fprintf(stderr,
            "REX_BACKEND_INVARIANT[per-tu-command]: reason=%s "
            "expected-source=%s command=%s\n",
            reason, expectedSource.c_str(),
            CommandlineProcessing::generateStringFromArgList(argList).c_str());
    ROSE_ABORT();
  };

  if (argList.empty()) {
    fail("empty-command");
  }
  if (expectedSource.empty()) {
    fail("empty-expected-source");
  }

  size_t compileOnlyOptionCount = 0;
  size_t outputOptionCount = 0;
  string outputFilename;
  Rose_STL_Container<string> sourceOperands;
  ClangLanguageSelection activeLanguage;
  bool afterDoubleDash = false;

  for (size_t index = 1; index < argList.size(); ++index) {
    const string &argument = argList[index];
    if (!afterDoubleDash && argument == "--") {
      afterDoubleDash = true;
      continue;
    }
    if (!afterDoubleDash && argument == "-x") {
      if (++index >= argList.size()) {
        fail("option-missing-value");
      }
      activeLanguage = parseClangLanguageName(argList[index]);
      continue;
    }
    if (!afterDoubleDash && argument.rfind("-x", 0) == 0 &&
        argument.size() > 2) {
      activeLanguage = parseClangLanguageName(argument.substr(2));
      continue;
    }
    if (!afterDoubleDash && argument == "-c") {
      ++compileOnlyOptionCount;
      continue;
    }

    if (!afterDoubleDash &&
        CommandlineProcessing::isOptionTakingSecondParameter(argument)) {
      const size_t parameterCount =
          CommandlineProcessing::isOptionTakingThirdParameter(argument) ? 2 : 1;
      if (parameterCount >= argList.size() - index) {
        fail("option-missing-value");
      }
      if (argument == "-o") {
        ++outputOptionCount;
        outputFilename = argList[index + 1];
        if (outputFilename.empty() || outputFilename.front() == '-') {
          fail("invalid-output-value");
        }
      }
      index += parameterCount;
      continue;
    }

    if (!isPositionalCommandLineArgument(argument, afterDoubleDash)) {
      continue;
    }
    if (argument == "-") {
      rejectClangStdinInput();
    }
    const bool suffixSource =
        CommandlineProcessing::isSourceFilename(argument) ||
        CommandlineProcessing::isAssemblerFileNameSuffix(
            StringUtility::fileNameSuffix(argument));
    if (activeLanguage.isExplicit() || suffixSource) {
      sourceOperands.push_back(argument);
    }
  }

  if (compileOnlyOptionCount == 0) {
    fail("missing-compile-only-option");
  }
  if (sourceOperands.size() != 1) {
    fail("source-operand-count");
  }
  if (!sameCommandLineSource(sourceOperands.front(), expectedSource)) {
    fail("unexpected-source-operand");
  }
  if (outputOptionCount != 1) {
    fail("output-option-count");
  }
  if (outputFilename == expectedSource ||
      (std::filesystem::exists(outputFilename) &&
       std::filesystem::equivalent(outputFilename, expectedSource))) {
    fail("output-clobbers-source");
  }
}

static const char *const removedRexClangFrontendOptions[] = {
    "-rex:clang:disable-access-control", "-rex:clang:delayed-template-parsing",
    "-rex:clang:respect-rtti-flags"};

static bool isRemovedRexClangFrontendOption(const std::string &arg) {
  for (const char *option : removedRexClangFrontendOptions) {
    if (arg == option) {
      return true;
    }
  }

  return false;
}

[[noreturn]] static void
rejectRemovedRexClangFrontendOption(const std::string &option) {
  fprintf(stderr,
          "REX_FRONTEND_INVARIANT[removed-clang-option]: option '%s' was an "
          "inexact compatibility API and is not supported\n",
          option.c_str());
  ROSE_ABORT();
}

static void
rejectRemovedRexClangFrontendOptions(const std::vector<std::string> &argv) {
  for (const std::string &argument : argv) {
    if (isRemovedRexClangFrontendOption(argument)) {
      rejectRemovedRexClangFrontendOption(argument);
    }
  }
}

static const char *const removedRoseLanguageSelectionOptions[] = {
    "c",          "C",          "C_only",     "C89",        "C89_only",
    "C99",        "C99_only",   "C11",        "C11_only",   "C17",
    "C17_only",   "C23",        "C23_only",   "C2y",        "C2y_only",
    "cxx",        "Cxx",        "Cxx_only",   "Cxx11",      "Cxx0x",
    "Cxx0x_only", "Cxx11_only", "Cxx14",      "Cxx14_only", "Cxx17",
    "Cxx17_only", "Cxx20",      "Cxx20_only", "Cxx23",      "Cxx23_only",
    "Cxx26",      "Cxx26_only"};

static bool isRemovedRoseLanguageSelectionOption(const std::string &argument) {
  if (argument == "-std=c" || argument == "-std=gnu" ||
      argument == "-std=c++" || argument == "-std=gnu++") {
    return true;
  }

  const std::string rosePrefix = "-rose:";
  const std::string longRosePrefix = "--rose:";
  std::string option;
  if (argument.rfind(rosePrefix, 0) == 0) {
    option = argument.substr(rosePrefix.size());
  } else if (argument.rfind(longRosePrefix, 0) == 0) {
    option = argument.substr(longRosePrefix.size());
  } else {
    return false;
  }

  for (const char *removedOption : removedRoseLanguageSelectionOptions) {
    if (option == removedOption) {
      return true;
    }
  }
  return false;
}

[[noreturn]] static void
rejectRemovedRoseLanguageSelectionOption(const std::string &option) {
  fprintf(stderr,
          "REX_FRONTEND_INVARIANT[removed-language-option]: option '%s' was "
          "a deprecated language-selection API; use the source extension, "
          "-x, and -std= explicitly\n",
          option.c_str());
  ROSE_ABORT();
}

static void rejectRemovedRoseLanguageSelectionOptions(
    const std::vector<std::string> &argv) {
  for (const std::string &argument : argv) {
    if (isRemovedRoseLanguageSelectionOption(argument)) {
      rejectRemovedRoseLanguageSelectionOption(argument);
    }
  }
}

struct RemovedRexLegacyOption {
  const char *option;
  const char *reason;
};

static const RemovedRexLegacyOption removedRexLegacyOptions[] = {
    {"-rose:astMerge",
     "selected the removed AST-merge subsystem and was silently ignored"},
    {"-rose:ast:merge",
     "selected the removed AST-merge subsystem and was silently ignored"},
    {"-rose:astMergeCommandFile",
     "configured the removed AST-merge subsystem and was silently ignored"},
    {"-rose:skip_unparse_asm_commands",
     "silently dropped source-written inline assembly"},
    {"-rose:skip_unparse_cc_commands", "was a no-op compatibility option"},
    {"-rose:mangled:noshortname", "was an unsupported no-op mangling option"},
    {"-rose:translateCommentsAndDirectivesIntoAST",
     "selected an unimplemented AST conversion and was otherwise a no-op"},
    {"-rose:instantiation", "never controlled Clang template instantiation"},
    {"-rose:suppress_variable_declaration_normalization",
     "was an unparser-formatting compatibility switch; exact source "
     "declarator groups are represented by typed AST"}};

static void
rejectRemovedRexLegacyOptions(const std::vector<std::string> &argv) {
  for (const std::string &argument : argv) {
    for (const RemovedRexLegacyOption &removed : removedRexLegacyOptions) {
      if (argument == removed.option) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[removed-legacy-option]: option '%s' "
                "%s and is not supported\n",
                removed.option, removed.reason);
        ROSE_ABORT();
      }
    }
  }
}

/*-----------------------------------------------------------------------------
 *  namespace SgProject {
 *---------------------------------------------------------------------------*/
void SgProject::processCommandLine(const vector<string> &input_argv) {
  for (const auto &argument : input_argv)
    ROSE_ASSERT(argument.find('\0') == string::npos);
  rejectRemovedRexClangFrontendOptions(input_argv);
  rejectRemovedRoseLanguageSelectionOptions(input_argv);
  rejectRemovedRexLegacyOptions(input_argv);
  CommandlineProcessing::validateClangLanguageOptions(input_argv);

  // This functions only copies the command line and extracts information from
  // the command line which is useful at the SgProject level (other information
  // useful at the SgFile level is not extracted). Specifically:
  //      1) --help is processed (along with -help, to be friendly)
  //      2) -o <filename> is processed (since both the compilation and the
  //      linking
  //         phases must know the output file name and it makes sense to process
  //         that once).
  //      3) Lists of files and libraries are processed (sense they too are
  //      required in
  //         both compilation and linking).  (e.g. -l<libname>, -L <directory>,
  //         <libname>.a, <filename>.C, <filename>.c, -I<directory name>,
  //         <filename>.h
  // This function now makes an internal copy of the command line parameters to
  // allow the originals to remain unmodified (SLA modifies the command line).

  // local copies of argc and argv variables
  // The purpose of building local copies is to avoid
  // the modification of the command line by SLA (to save the original command
  // line)
  vector<string> local_commandLineArgumentList = input_argv;
  { // Perform normalization on CLI before processing and storing

    // Turn "-I <path>" into "-I<path>" for subsequent processing
    local_commandLineArgumentList = Rose::Cmdline::NormalizeIncludePathOptions(
        local_commandLineArgumentList);
  }

  // Save a deep copy fo the original command line input the the translator
  // pass in out copies of the argc and argv to make clear that we don't modify
  // argc and argv
  get_originalCommandLineArgumentList() = local_commandLineArgumentList;

  // Build the empty STL lists
  ROSE_ASSERT(p_fileList_ptr != NULL);
  p_fileList_ptr->get_listOfFiles().clear();

  // return value for calls to SLA
  int optionCount = 0;

  //
  // help option (allows alternative --h or --help)
  //
  optionCount =
      sla_none(local_commandLineArgumentList, "--", "($)", "(h|help)", 1);
  if (optionCount > 0) {
    cout << version_message() << endl;
    SgFile::usage();
    exit(0);
  }

  //
  // version option (allows alternative --version or --V)
  //
  optionCount =
      sla_none(local_commandLineArgumentList, "--", "($)", "(V|version)", 1);
  if (optionCount > 0) {
    cout << version_message() << endl;
    exit(0);
  }

  // DQ (10/15/2005): Added because verbose was not set when the number of files
  // (source files) was zero (case for linking only)
  //
  // specify verbose setting for projects (should be set even for linking where
  // there are no source files
  //
  ROSE_ASSERT(get_verbose() >= 0);
  ROSE_ASSERT(get_verbose() <= 10);

  // user specify verbose level from command line from 0 - 4, coresponding to 4
  // (NONE), 5, (KEY), 6(INFO), 7(MARCH), 8(TRACE)
  int integerOptionForVerbose = 0;
  if (CommandlineProcessing::isOptionWithParameter(
          local_commandLineArgumentList, "-rose:", "(v|verbose)",
          integerOptionForVerbose, true) == true) {
    // set_verbose(true);
    int needToFix = integerOptionForVerbose;
    integerOptionForVerbose += DEFAULT_MLOG_LEVEL;
    if (integerOptionForVerbose > MLOG_LEVEL_TRACE)
      integerOptionForVerbose = MLOG_LEVEL_TRACE;
    integerOptionForVerbose = needToFix;
    set_verbose(integerOptionForVerbose);
    Rose::Cmdline::verbose = integerOptionForVerbose;

    if (SgProject::get_verbose() > DEFAULT_MLOG_LEVEL)
      printf("Fake news: %s verbose mode ON (for SgProject)\n",
             mlogLevelToString_C[integerOptionForVerbose]);
  }

  if (CommandlineProcessing::isOption(local_commandLineArgumentList,
                                      "-rose:keep_going", "", true)) {
    fprintf(stderr,
            "REX_COMMANDLINE_INVARIANT[keep-going]: -rose:keep_going is not "
            "supported because compiler pipeline failures terminate at "
            "their source\n");
    ROSE_ABORT();
  }

  //
  // Standard compiler options (allows specification of language -x option to
  // just run compiler without /dev/null as input file)
  //
  // DQ (1/8/2014): This configuration is used by the git application to specify
  // the C language with the input file is /dev/null. This is a slightly bizare
  // corner case of our command line processing.
  // Clang's -x state is ordered and belongs to each input operand.  A project
  // can legitimately contain `-x c a -x c++ b`; publishing the last switch as
  // project-global language corrupts the first translation unit.  Validate the
  // complete option stream before publishing any project state, then let each
  // SgFile snapshot the state active at its own exact source operand.
  set_C_only(false);
  set_Cxx_only(false);

  //
  // Standard compiler options (allows alternative -E option to just run CPP)
  //
  if (CommandlineProcessing::isOption(local_commandLineArgumentList, "-", "(E)",
                                      false) == true) {
    p_C_PreprocessorOnly = true;
  }

  // DQ (1/19/2014): Adding support for gnu "-S" option, which means:
  // Stop after the stage of compilation proper; do not assemble. The output is
  // in the form of an assembler code file for each non-assembler input file
  // specified. By default, the assembler file name for a source file is made by
  // replacing the suffix .c, .i, etc., with .s. Input files that don't require
  // compilation are ignored.
  //
  // Standard compiler options (allows alternative -S option to just run gcc
  // directly)
  //
  if (CommandlineProcessing::isOption(local_commandLineArgumentList, "-", "(S)",
                                      false) == true) {
    // printf ("/* option -S found (just run backend compiler with -S to gcc) */
    // \n");
    p_stop_after_compilation_do_not_assemble_file = true;
  }

  // DQ (1/20/2014): Adding support for gnu -undefined option to ROSE command
  // line. -u SYMBOL, --undefined SYMBOL    Start with undefined reference to
  // SYMBOL
  string stringOptionForUndefinedSymbol;
  if (CommandlineProcessing::isOptionWithParameter(
          local_commandLineArgumentList, "-", "(u|undefined)",
          stringOptionForUndefinedSymbol, true) == true) {
    printf("Found -u -undefined option specified on command line: "
           "stringOptionForUndefinedSymbol = %s \n",
           stringOptionForUndefinedSymbol.c_str());

    p_gnuOptionForUndefinedSymbol = stringOptionForUndefinedSymbol;

    if (SgProject::get_verbose() >= 1)
      printf("-undefined option specified on command line (for SgFile)\n");
  }

  // DQ (12/10/2016): Areas build system appears to use the "-MM" option on the
  // command line and this is a problem for ROSE when not specified using a
  // parameter.
  if (CommandlineProcessing::isOption(local_commandLineArgumentList, "-",
                                      "(MM)", false) == true) {
    // The support for this should just be to call the backend compiler with the
    // "-MM" option.
    printf("NOTE: The use of the -MM option to ROSE is not recoreded "
           "internally (simple call to backend compiler using -MM option not "
           "implemented). \n");
    if (SgProject::get_verbose() >= 0)
      printf("-MM dependence file specification specified on command line (for "
             "SgFile)\n");
  }

  // DQ (1/26/2014): Adding support for gnu -version-info option to ROSE command
  // line.
  string stringOptionForVersionSpecification;
  if (CommandlineProcessing::isOptionWithParameter(
          local_commandLineArgumentList, "-", "(version-info)",
          stringOptionForVersionSpecification, true) == true) {
    printf("Found -version-info option specified on command line: "
           "stringOptionForVersionSpecification = %s \n",
           stringOptionForVersionSpecification.c_str());

    if (SgProject::get_verbose() >= 1)
      printf("-version-info option specified on command line (for SgFile)\n");
  }

  // DQ (1/20/2014): Adding support for "-m32" option for 32-bit mode on 64-bit
  // systems. The 32-bit environment sets int, long and pointer to 32 bits. The
  // 64-bit environment sets int to 32 bits and long and pointer to 64 bits
  // (ROSE does not support the -m64 command line option).
  //
  // Standard compiler options (allows alternative -m32 option)
  //
  if (CommandlineProcessing::isOption(local_commandLineArgumentList, "-",
                                      "(m32)", false) == true) {
    p_mode_32_bit = true;
  }

  // DQ (3/19/2014): This option causes the output of source code to an existing
  // file to be an error.
  //
  // noclobber_output_file
  //
  if (CommandlineProcessing::isOption(local_commandLineArgumentList, "-rose:",
                                      "noclobber_output_file", false) == true) {
    p_noclobber_output_file = true;
  }

  // DQ (3/19/2014): This option causes the output of source code to an existing
  // file to be an error if it results in a different file.
  //
  // noclobber_if_different_output_file
  //
  if (CommandlineProcessing::isOption(
          local_commandLineArgumentList,
          "-rose:", "noclobber_if_different_output_file", false) == true) {
    p_noclobber_if_different_output_file = true;

    // Make it an error to specify both of these noclobber options.
    if (p_noclobber_output_file == true) {
      printf(
          "Error: options -rose:noclobber_output_file and "
          "-rose:noclobber_if_different_output_file are mutually exclusive \n");
      ROSE_ABORT();
    }
  }

  // Pei-Hung (8/6/2014): This option appends PID into the output name to avoid
  // file collision in parallel compilation.
  //
  // appendPID
  //
  if (CommandlineProcessing::isOption(local_commandLineArgumentList,
                                      "-rose:", "appendPID", false) == true) {
    set_appendPID(true);
  }

  //
  // specify compilation only option (new style command line processing)
  //
  if (CommandlineProcessing::isOption(local_commandLineArgumentList, "-", "c",
                                      false) == true) {
    set_compileOnly(true);
  }

  // Liao 6/29/2012: support linking flags for OpenMP lowering when no SgFile is
  // available
  set_openmp_linking(false);
  if (CommandlineProcessing::isOption(local_commandLineArgumentList,
                                      "-rose:OpenMP:", "lowering",
                                      true) == true ||
      CommandlineProcessing::isOption(local_commandLineArgumentList,
                                      "--rose:OpenMP:", "lowering",
                                      true) == true ||
      CommandlineProcessing::isOption(local_commandLineArgumentList,
                                      "--rose:openmp:", "lowering",
                                      true) == true ||
      CommandlineProcessing::isOption(local_commandLineArgumentList,
                                      "-rose:openmp:", "lowering",
                                      true) == true) {
    if (SgProject::get_verbose() >= 1)
      printf("In SgProject: openmp_linking mode ON \n");
    set_openmp_linking(true);
  }

  Rose::Cmdline::Unparser::Process(this, local_commandLineArgumentList);
  Rose::Cmdline::Fortran::Process(this, local_commandLineArgumentList);
  Rose::Cmdline::Gnu::Process(this, local_commandLineArgumentList);

  // DQ (9/14/2013): Adding option to copy the location of the input file as the
  // position for the generated output file. This is now demonstrated to be
  // important in the case of ffmpeg-1.2 for the file "file.c" where it is
  // specified as "libavutil/file.c" on the command line and we by default put
  // it into the current directory (top level directory in the directory
  // structure).  But it is a subtle and difficult to reproduce error that the
  // generated file will not compile properly from the top level directory (even
  // when the "-I<absolute path>/libavutil" is specified). We need an option to
  // put the generated file back into the original directory where the input
  // source files is located, so that when the generated rose_*.c file is
  // compiled (with the backend compiler, e.g. gcc) it can use the identical
  // rules for resolving head files as it would have for the original input file
  // (had it been compiled using the backend compiler instead).
  set_unparse_in_same_directory_as_input_file(false);
  if (CommandlineProcessing::isOption(
          local_commandLineArgumentList,
          "-rose:", "unparse_in_same_directory_as_input_file", false) == true) {
    set_unparse_in_same_directory_as_input_file(true);
  }

  // DQ (10/3/2010): Adding support for CPP directives to be optionally a part
  // of the AST as declarations in global scope instead of handled similar to
  // comments.
  set_addCppDirectivesToAST(false);
  ROSE_ASSERT(get_addCppDirectivesToAST() == false);
  if (CommandlineProcessing::isOption(local_commandLineArgumentList, "-rose:",
                                      "addCppDirectivesToAST", true) == true) {
    if (SgProject::get_verbose() >= 1)
      printf("In SgProject: addCppDirectivesToAST mode ON \n");
    set_addCppDirectivesToAST(true);
  }

  if (CommandlineProcessing::isOption(local_commandLineArgumentList,
                                      "-rose:", "(prelink)", true) == true) {
    fprintf(stderr, "REX_COMMAND_LINE_ERROR: -rose:prelink is unsupported\n");
    ROSE_ABORT();
  }

  //
  // specify output file option
  //
  // Use 1 at end of argument list to SLA to force removal of option from argv
  // and decrement of argc
  string tempOutputFilename;
  optionCount = sla(local_commandLineArgumentList, "-", "($)^", "(o|output)",
                    &tempOutputFilename, 1);
  if (optionCount > 0) {
    // Make our own copy of the filename string
    p_outputFileName = Rose::TestOutput::resolvePath(tempOutputFilename);
  } else {
    // Usual default for output executables (comes from the default name for
    // assembler output a long long time ago, way way back in the 1970's).
    // For history of why "a.out" is the default see:
    // http://cm.bell-labs.com/cm/cs/who/dmr/chist.html
    p_outputFileName = "a.out";
  }

  // DQ (1/16/2008): Added include/exclude path lists for use internally by
  // translators. For example in Compass this is the basis of a mechanism to
  // exclude processing of header files from specific directorys (where messages
  // about the properties of the code there are not meaningful since they cannot
  // be changed by the user). These are ment to be used by ROSE translators and
  // as a result have unspecificed semantics in ROSE, it is just a common
  // requirement of many uses of ROSE.
  //
  // DQ (1/16/2008): Added include/exclude path lists for use internally by
  // translators. specify exclude path option
  string tempIncludePath;
  vector<string>::iterator iter = local_commandLineArgumentList.begin();
  while (iter != local_commandLineArgumentList.end()) {
    if (*iter == "-rose:includePath") {
      iter++;
      tempIncludePath = *iter;

      // printf ("Adding tempIncludePath = %s to p_includePathList
      // \n",tempIncludePath.c_str());
      p_includePathList.push_back(tempIncludePath);
    }

    iter++;
  }

  // DQ (1/16/2008): Added include/exclude path lists for use internally by
  // translators. specify exclude path option
  string tempExcludePath;
  iter = local_commandLineArgumentList.begin();
  while (iter != local_commandLineArgumentList.end()) {
    if (*iter == "-rose:excludePath") {
      iter++;
      tempExcludePath = *iter;

      // printf ("Adding tempExcludePath = %s to p_excludePathList
      // \n",tempExcludePath.c_str());
      p_excludePathList.push_back(tempExcludePath);
    }

    iter++;
  }

  // DQ (1/16/2008): Added include/exclude file lists for use internally by
  // translators. specify exclude path option
  string tempIncludeFile;
  iter = local_commandLineArgumentList.begin();
  while (iter != local_commandLineArgumentList.end()) {
    if (*iter == "-rose:includeFile") {
      iter++;
      tempIncludeFile = *iter;

      // DQ (1/17/2008): filename matching should be only done on the filename
      // and not the path
      string filenameWithoutPath =
          StringUtility::stripPathFromFileName(tempIncludeFile);
      // printf ("Adding tempIncludeFile = %s filenameWithoutPath = %s to
      // p_includeFileList
      // \n",tempIncludeFile.c_str(),filenameWithoutPath.c_str());
      p_includeFileList.push_back(filenameWithoutPath);
    }

    iter++;
  }

  // DQ (1/16/2008): Added include/exclude file lists for use internally by
  // translators. specify exclude path option
  string tempExcludeFile;
  iter = local_commandLineArgumentList.begin();
  while (iter != local_commandLineArgumentList.end()) {
    if (*iter == "-rose:excludeFile") {
      iter++;
      tempExcludeFile = *iter;

      // DQ (1/17/2008): filename matching should be only done on the filename
      // and not the path
      string filenameWithoutPath =
          StringUtility::stripPathFromFileName(tempExcludeFile);
      // printf ("Adding tempExcludeFile = %s filenameWithoutPath = %s to
      // p_excludeFileList
      // \n",tempExcludeFile.c_str(),filenameWithoutPath.c_str());
      p_excludeFileList.push_back(filenameWithoutPath);
    }

    iter++;
  }

  // DQ (1/13/2009): Added support for GNU -include <header_file> option
  // (include this file before all others).
  string tempHeaderFile;
  iter = local_commandLineArgumentList.begin();
  while (iter != local_commandLineArgumentList.end()) {
    if (*iter == "-include") {
      iter++;
      tempHeaderFile = *iter;
      p_preincludeFileList.push_back(tempHeaderFile);
    }

    iter++;
  }

  // DQ (1/13/2009): Added support for GNU -isystem <directory> option (include
  // this directory before all others).
  string tempDirectory;
  iter = local_commandLineArgumentList.begin();
  while (iter != local_commandLineArgumentList.end()) {
    if (SgProject::get_verbose() > 1)
      printf("Searching for -isystem options iter = %s \n", (*iter).c_str());
    if (*iter == "-isystem") {
      iter++;
      tempDirectory = *iter;

      if (SgProject::get_verbose() > 1)
        printf("Adding tempHeaderFile = %s to p_preincludeDirectoryList \n",
               tempDirectory.c_str());
      p_preincludeDirectoryList.push_back(tempDirectory);
    }

    iter++;
  }

  // DQ (10/16/2005):
  // Build versions of argc and argv that are separate from the input_argc and
  // input_argv (so that we can be clear that there are no side-effects to the
  // original argc and argv that come from the user's ROSE translator.  Mostly
  // we want to use the short names (e.g. "argc" and "argv").
  vector<string> argv = get_originalCommandLineArgumentList();
  ROSE_ASSERT(argv.size() > 0);

  p_sourceFileNameList = CommandlineProcessing::generateSourceFilenames(argv);

  // Build a list of source, object, and library files on the command line
  for (unsigned int i = 1; i < argv.size(); i++) {
    // find the source code filenames and modify them to be the output filenames
    unsigned int length = argv[i].size();

    // DQ (2/4/2009): Only put *.o files into the objectFileNameList for
    // linking. DQ (1/16/2008): This is a better (simpler) implementation
    if (CommandlineProcessing::isObjectFilename(argv[i]) == true) {
      p_objectFileNameList.push_back(argv[i]);
    }

    // look only for .a files (library files)
    if ((length > 2) && ((argv[i][0] != '-') && (argv[i][0] != '+')) &&
        ((argv[i][length - 2] == '.') && (argv[i][length - 1] == 'a'))) {
      std::string libraryFile = argv[i];
      p_libraryFileList.push_back(libraryFile);
    }

    // look only for -l library files (library files)
    if ((length > 2) && (argv[i][0] == '-') && (argv[i][1] == 'l')) {
      std::string librarySpecifier = argv[i].substr(2);
      p_librarySpecifierList.push_back(librarySpecifier);
    }

    // look only for -L directories (directories where -lxxx libraries will be
    // found)
    if ((length > 2) && (argv[i][0] == '-') && (argv[i][1] == 'L')) {
      const std::string libraryDirectorySpecifier =
          std::filesystem::absolute(argv[i].substr(2)).string();
      p_libraryDirectorySpecifierList.push_back(libraryDirectorySpecifier);
    }

    // look only for -I include directories (directories where
    // #include<filename> will be found)
    if ((length > 2) && (argv[i][0] == '-') && (argv[i][1] == 'I')) {
      const std::string include_path =
          std::filesystem::absolute(argv[i].substr(2)).string();

      p_includeDirectorySpecifierList.push_back("-I" + include_path);
    }

    // DQ (10/18/2010): Added support to collect "-D" options (assume no space
    // between the "-D" and the option (e.g. "-DmyMacro=8"). Note that we want
    // to collect these because we have to process "-D" options more explicitly
    // for Fortran (they are not required for C/C++ because they are processed
    // by the forontend directly.
    if ((length > 2) && (argv[i][0] == '-') && (argv[i][1] == 'D')) {
      std::string macroSpecifier = argv[i].substr(2);
      p_macroSpecifierList.push_back(macroSpecifier);
    }
  }

  // DQ (9/15/2018): Adding support for output of report on the header file
  // unparsing (for debugging).
  if (CommandlineProcessing::isOption(local_commandLineArgumentList,
                                      "-rose:", "(headerFileUnparsingReport)",
                                      true) == true) {
    set_reportOnHeaderFileUnparsing(true);
  }

  // DQ (10/28/2020): Adding option to output the compilation performance data.
  AstPerformance::outputCompilationPerformance = false;
  if (CommandlineProcessing::isOption(local_commandLineArgumentList,
                                      "-rose:", "(compilationPerformance)",
                                      true) == true) {
    AstPerformance::outputCompilationPerformance = true;
  }

  // DQ (8/29/2006): Added support for accumulation of performance data into CSV
  // data file (for later processing to build performance graphs)
  std::string compilationPerformanceFilenameParameter;
  if (CommandlineProcessing::isOptionWithParameter(
          local_commandLineArgumentList,
          "-rose:", "(compilationPerformanceFile)",
          compilationPerformanceFilenameParameter, true) == true) {
    p_compilationPerformanceFile = compilationPerformanceFilenameParameter;
  }

  // DQ (8/14/2025): Added support to supress name qualification on the whole
  // translation unit (focus only on the source file which will be unparsed).
  // The optional default behaviour is to support name qualification across the
  // whole translation using, this is a performance only option since the name
  // qualification represents about 40% of the total cost of processing large
  // files ($1M line translation units, source file plus expanded header files).
  set_suppressNameQualificationAcrossWholeTranslationUnit(false);
  if (CommandlineProcessing::isOption(
          local_commandLineArgumentList,
          "-rose:", "(suppressNameQualificationAcrossWholeTranslationUnit)",
          true) == true) {
    printf(
        "Using -rose:suppressNameQualificationAcrossWholeTranslationUnit \n");
    p_suppressNameQualificationAcrossWholeTranslationUnit = true;
    ROSE_ASSERT(get_suppressNameQualificationAcrossWholeTranslationUnit() ==
                true);
  }

  // Verbose ?

  if (get_verbose() > 1) {
    // Find out what file we are doing transformations upon
    printf("In SgProject::processCommandLine() (verbose mode ON): \n");
    display("In SgProject::processCommandLine()");
  }
}

//------------------------------------------------------------------------------
//                                 Cmdline
//------------------------------------------------------------------------------
std::vector<std::string>
Rose::Cmdline::NormalizeIncludePathOptions(std::vector<std::string> &argv) {
  std::vector<std::string> r_argv;

  bool looking_for_include_path_arg = false;
  for (string &arg : argv) {
    // Must be first since there could be, for example, "-I -I",
    // in which case, the else if branch checking for -I would
    // be entered.
    if (looking_for_include_path_arg) {
      looking_for_include_path_arg = false; // reset for next iteration

      // Sanity check
      bool is_directory = std::filesystem::is_directory(arg);
      if (false == is_directory) {
        // DQ (3/15/2017): Fixed to use mlog message logging.
        {
          MLOG_WARN_CXX("sage_support")
              << "Invalid argument to -I; path does not exist: "
              << "'" << arg << "'" << std::endl;
        }
      }
      r_argv.push_back("-I" + arg + "");
    } else if ((arg.size() >= 2) && (arg[0] == '-') && (arg[1] == 'I')) {
      // -I <path>: There is a space between the option and the argument...
      //   ^
      //
      // ...meaning this current argument is exactly "-I".
      //
      if (arg.size() == 2) {
        looking_for_include_path_arg = true;
        continue; // next iteration should be the path argument
      } else {
        r_argv.push_back(arg);
      }
    } else // not an include path option
    {
      r_argv.push_back(arg);
    }
  } // argv.each

  // Found -I option but no accompanying <path> argument
  if (looking_for_include_path_arg == true) {
    std::cout << "[FATAL] "
              << "Missing required argument to -I; expecting '-I<path>'"
              << std::endl;
    exit(1);
  } else {
    return r_argv;
  }
} // NormalizeIncludePathOptions (std::vector<std::string>& argv)

//------------------------------------------------------------------------------
//                                  Unparser
//------------------------------------------------------------------------------

void Rose::Cmdline::Unparser::StripRoseOptions(std::vector<std::string> &argv) {
  sla(argv, "-rose:unparser:", "($)", "(clobber_input_file)", 1);
}

void Rose::Cmdline::Unparser::Process(SgProject *project,
                                      std::vector<std::string> &argv) {
  if (SgProject::get_verbose() > 1)
    std::cout << "[INFO] Processing Unparser commandline options" << std::endl;

  ProcessClobberInputFile(project, argv);
} // ::Rose::Cmdline::Unparser::Process

void Rose::Cmdline::Unparser::ProcessClobberInputFile(
    SgProject *project, std::vector<std::string> &argv) {
  bool has_clobber_input_file = CommandlineProcessing::isOption(
      argv, Cmdline::Unparser::option_prefix, "clobber_input_file",
      Cmdline::REMOVE_OPTION_FROM_ARGV);

  if (has_clobber_input_file) {
    if (SgProject::get_verbose() > 1)
      std::cout << "[INFO] **CAUTION** Turning on the Unparser's destructive "
                   "clobber mode =O"
                << std::endl;

    project->set_unparser__clobber_input_file(true);
  } else {
    project->set_unparser__clobber_input_file(false);
  }
} // ::Rose::Cmdline::Unparser::ProcessClobberInputFile

//------------------------------------------------------------------------------
//                                  Fortran
//------------------------------------------------------------------------------

void Rose::Cmdline::Fortran::Process(SgProject *project,
                                     std::vector<std::string> &argv) {
  if (SgProject::get_verbose() > 1)
    std::cout << "[INFO] Processing Fortran commandline options" << std::endl;

  ProcessFortranOnly(project, argv);
}

void Rose::Cmdline::Fortran::ProcessFortranOnly(
    SgProject *project, std::vector<std::string> &argv) {
  bool is_fortran_only =
      CommandlineProcessing::isOption(argv, "-rose:fortran", "", true);

  if (is_fortran_only) {
    if (SgProject::get_verbose() > 1)
      std::cout << "[INFO] Turning on Fortran only mode" << std::endl;

    project->set_Fortran_only(true);
  }
}

//------------------------------------------------------------------------------
//                                  Gnu
//------------------------------------------------------------------------------
//
void Rose::Cmdline::Gnu::Process(SgProject *project,
                                 std::vector<std::string> &argv) {
  if (SgProject::get_verbose() > 1)
    std::cout << "[INFO] Processing GNU commandline options" << std::endl;

  ProcessParam(project, argv);
}

void Rose::Cmdline::Gnu::ProcessParam(SgProject * /*project*/,
                                      std::vector<std::string> &argv) {
  std::string param;
  bool has_param = CommandlineProcessing::isOptionWithParameter(
      argv, "--", "(param)", param, true);

  if (has_param) {
    if (SgProject::get_verbose() > 1)
      std::cout << "[INFO] Detected GNU --param " << param << std::endl;
  }
}

/*-----------------------------------------------------------------------------
 *  namespace SgFile {
 *---------------------------------------------------------------------------*/
void SgFile::usage() {
  // Version details are emitted by command-line processing through
  // version_message().
  fputs("\n"
        "This ROSE translator provides a means for operating on C, C++, and "
        "Fortran source code.\n"
        "\n"
        "Usage: rose [OPTION]... FILENAME...\n"
        "\n"
        "If a long option shows a mandatory argument, it is mandatory for "
        "the equivalent\n"
        "short option as well, and similarly for optional arguments.\n"
        "\n"
        "Main operation mode:\n"
        "     -rose:(o|output) FILENAME\n"
        "                             file containing final unparsed C++ "
        "code\n"
        "                             (relative or absolute paths are "
        "supported)\n"
        "Operation modifiers:\n"
        "     -rose:OpenMP, -rose:openmp\n"
        "                             follow OpenMP 3.0 specification for "
        "C/C++ and Fortran, perform one of the following actions:\n"
        "     -rose:OpenMP:parse_only, -rose:openmp:parse_only\n"
        "                             parse OpenMP directives to OpenMPIR, "
        "no further actions (default behavior now)\n"
        "     -rose:OpenMP:ast_only, -rose:openmp:ast_only\n"
        "                             on top of -rose:openmp:parse_only, "
        "build OpenMP AST nodes from OpenMPIR, no further actions\n"
        "     -rose:OpenMP:lowering, -rose:openmp:lowering\n"
        "                             on top of -rose:openmp:ast_only, "
        "transform AST with OpenMP nodes into multithreaded code \n"
        "                             targeting LLVM OpenMP runtime "
        "(__kmpc/libomp)\n"
        "     -rose:OpenACC, -rose:openacc\n"
        "                             follow OpenACC 3.0 specification for "
        "Fortran, perform one of the following actions:\n"
        "     -rose:OpenACC:parse_only, -rose:openacc:parse_only\n"
        "                             parse OpenACC directives to "
        "OmpAccAttributes, "
        "no further actions (default behavior now)\n"
        "     -rose:OpenACC:ast_only, -rose:openacc:ast_only\n"
        "                             on top of -rose:openacc:parse_only, "
        "build OpenACC AST nodes from OmpAccAttributes, no further actions\n"
        "     --rex-omp-ast-only      REX convenience option: equivalent to "
        "-fopenmp --rose:openmp:ast_only\n"
        "     --rex-omp-lowering      REX convenience option: equivalent to "
        "-fopenmp -rose:openmp:lowering -rose:skipfinalCompileStep\n"
        "     -rose:fortran\n"
        "                             compile Fortran code, determining "
        "version of\n"
        "                             Fortran from file suffix)\n"
        "     -rose:fortran_std <f77|f90|f95|f2003|f2008|f2018>\n"
        "                             select a Fortran dialect for ROSE "
        "(not a backend -std flag)\n"
        "     -rose:CoArrayFortran\n"
        "                             compile Co-Array Fortran code "
        "(extension of Fortran 2003)\n"
        "     -rose:fortran_std is the only supported dialect selector\n"
        "     -rose:strict            strict enforcement of ANSI/ISO "
        "standards\n"
        "     -rose:compilationPerformance\n"
        "                             Output compilation performance after "
        "compilation of input file.\n"
        "                             Reports internal phases of ROSE "
        "compilation (time and memory requirements), output to stdout.\n"
        "                             See also "
        "\"-rose:compilationPerformanceFile FILE\" for CSV report in a "
        "file.\n"
        "     -rose:compilationPerformanceFile FILE\n"
        "                             filename where compiler performance "
        "for internal\n"
        "                             phases (in CSV form) is placed for "
        "later\n"
        "                             processing (using "
        "script/graphPerformance)\n"
        "     -rose:exit_after_parser just call the parser (C, C++, and "
        "fortran only)\n"
        "     -rose:skip_transformation\n"
        "                             read input file and skip all "
        "transformations\n"
        "     -rose:skip_unparse      read and process input file but skip "
        "generation of\n"
        "                             final C++ output file\n"
        "     -rose:skipfinalCompileStep\n"
        "                             read and process input file, \n"
        "                             but skip invoking the backend "
        "compiler\n"
        "     -rose:collectAllCommentsAndDirectives\n"
        "                             store all comments and CPP directives "
        "in header\n"
        "                             files into the AST\n"
        "     -rose:unparseHeaderFiles\n"
        "                             unparse all directly or indirectly "
        "modified\n"
        "                             header files\n"
        "     -rose:excludeCommentsAndDirectives PATH\n"
        "                             provide path to exclude when using "
        "the\n"
        "                             collectAllCommentsAndDirectives "
        "option\n"
        "     -rose:excludeCommentsAndDirectivesFrom FILENAME\n"
        "                             provide filename to file with paths "
        "to exclude\n"
        "                             when using the "
        "collectAllCommentsAndDirectives\n"
        "                             option\n"
        "     -rose:includeCommentsAndDirectives PATH\n"
        "                             provide path to include when using "
        "the\n"
        "                             collectAllCommentsAndDirectives "
        "option\n"
        "     -rose:includeCommentsAndDirectivesFrom FILENAME\n"
        "                             provide filename to file with paths "
        "to include\n"
        "                             when using the "
        "collectAllCommentsAndDirectives\n"
        "                             option\n"
        "     -rose:skip_commentsAndDirectives\n"
        "                             ignore all comments and CPP "
        "directives (can\n"
        "                             generate (unparse) invalid code if "
        "not used with\n"
        "                             -rose:unparse_includes)\n"
        "     -rose:skipAstConsistancyTests\n"
        "                             skip AST consitancy testing (for "
        "better performance)\n"
        "Plugin Mode:\n"
        "     -rose:plugin_lib <shared_lib_filename>\n"
        "                             Specify the file path to a shared "
        "library built from plugin source files \n"
        "                             This option can repeat multiple times "
        "to load multiple libraries \n"
        "     -rose:plugin_action <act_name>\n"
        "                             Specify the plugin action to be "
        "executed\n"
        "                             This option can repeat multiple times "
        "to execute multiple actions \n"
        "                             in the order shown up in command line "
        "\n"
        "     -rose:plugin_arg_<act_name>  <option>\n"
        "                             Specify one option to be passed to a "
        "plugin named act_name\n"
        "                             This option can repeat multiple times "
        "to provide multiple options to a plugin \n"
        "     -rose:showBackendCommandLine\n"
        "                             Show the command line that is passed to "
        "the backend compiler\n"
        "\n"
        "GNU g++ options recognized:\n"
        "     -ansi                   equivalent to -rose:strict\n"
        "     -fno-implicit-templates disable output of template "
        "instantiations in\n"
        "                             generated source\n"
        "     -fno-implicit-inline-templates\n"
        "                             disable output of inlined template "
        "instantiations\n"
        "                             in generated source\n"
        "     -S                      gnu option trivial\n"
        "     -u (-undefined)         gnu option trivial\n"
        "     -version-info <name>    gnu option trivial (option not passed "
        "on to linker yet, \n"
        "                             incomplete implementation)\n"
        "     -MM <filename>          gnu Makefile dependence generation "
        "(option not passed \n"
        "                             on to compiler yet, incomplete "
        "implementation)\n"
        "\n"
        "Informative output:\n"
        "     -rose:help, --help, -help, --h\n"
        "                             print this help, then exit\n"
        "     -rose:version, --version, --V\n"
        "                             print ROSE program version number, "
        "then exit\n"
        "     -rose:markGeneratedFiles\n"
        "                             add \"#define ROSE_GENERATED_CODE\" "
        "to top of all\n"
        "                               generated code\n"
        "     -rose:verbose [LEVEL]   verbosely list internal processing "
        "(0, 1, 2, 4) with default=0\n"
        "                                      0 (NONE), 1(KEY), 2(INFO), "
        "3(MARCH), 4(TRACE)\n"
        "                               Higher values generate more output "
        "(can be\n"
        "                               applied to individual files and to "
        "the project\n"
        "                               separately, TBI).\n"
        "\n"
        "     -rose:fortran_std <f77|f90|f95|f2003|f2008|f2018>\n"
        "                             select a Fortran dialect for ROSE\n"
        "     -rose:cray_pointer_support\n"
        "                             turn on internal support for cray "
        "pointers\n"
        "                             (Note: not implemented in front-end "
        "yet.)\n"
        "     -fortran:XXX            pass -XXX to independent semantic "
        "                             (useful for turning on specific "
        "warnings in front-end)\n"
        "\n"
        "Control code generation:\n"
        "     -rose:unparser:clobber_input_file\n"
        "                               **CAUTION**RED*ALERT**CAUTION**\n"
        "                               If you don't know what this option "
        "does, don't use it!\n"
        "                               We are not responsible for any "
        "mental or physical damage\n"
        "                               that you will incur with the use of "
        "this option :)\n"
        "\n"
        "                               Note: This option breaks parallel "
        "builds, so make sure\n"
        "                               that with this option you use ROSE, "
        "and run your build\n"
        "                               system, sequentially.\n"
        "                               **CAUTION**RED*ALERT**CAUTION**\n"
        "     -rose:unparse_line_directives\n"
        "                               unparse statements using #line "
        "directives with\n"
        "                               reference to the original file and "
        "line number\n"
        "                               to support view of original source "
        "in debuggers\n"
        "                               and external tools\n"
        "     -rose:unparse_includes\n"
        "                               unparse all include files into the "
        "source file.\n"
        "                               This is a backup option for "
        "fail-safe processing\n"
        "                               of CPP directives (which can be "
        "tricky)\n"
        "     -rose:C_output_language\n"
        "                             force use of C as output language "
        "(currently\n"
        "                               generates C/C++)\n"
        "     -rose:Cxx_output_language\n"
        "                             force use of C++ as output language\n"
        "     -rose:Fortran_output_language\n"
        "                             force use of Fortran as output "
        "language\n"
        "     -rose:outputFormat      generate code in either fixed/free "
        "format (fortran only)\n"
        "                               options are: "
        "fixedOutput|fixedFormatOutput or \n"
        "                                            "
        "freeOutput|freeFormatOutput\n"
        "     -rose:backendCompileFormat\n"
        "                             use backend compiler option to "
        "compile generated code\n"
        "                               in either fixed/free format "
        "(fortran only)\n"
        "                               options are: "
        "fixedOutput|fixedFormatOutput or \n"
        "                                            "
        "freeOutput|freeFormatOutput\n"
        "     -rose:unparseHeaderFilesRootFolder FOLDERNAME\n"
        "                             A relative or an absolute path to the "
        "root folder,\n"
        "                             in which unparsed header files are "
        "stored.\n"
        "                             Note that the folder must be empty "
        "(or does not exist).\n"
        "                             If not specified, the default "
        "relative location _rose_ \n"
        "                             is used.\n"
        "     -rose:applicationRootDirectory DIRECTORYNAME\n"
        "                             A relative or an absolute path to the "
        "root folder,\n"
        "                             in which all application files will "
        "use for the unparsing \n"
        "                             of source files and header files. If "
        "not specified, the default \n"
        "                             location is the current directory. \n"
        "     -rose:unparse_in_same_directory_as_input_file\n"
        "                             Build the generated source file "
        "(unparse) in the same directory as \n"
        "                             the input source file.  This allows "
        "the backend compiler \n"
        "                             to compile the generated file exactly "
        "the same as the \n"
        "                             input would have been compiled "
        "(following original header file \n"
        "                             source path lookup rules precisely "
        "(this is rarely required)). \n"
        "     -rose:suppressNameQualificationAcrossWholeTranslationUnit\n"
        "                             This option skips the name qualification "
        "on the parts of the \n"
        "                             translation unit that are not meant to "
        "be unparsed.\n"
        "     -rose:noclobber_output_file\n"
        "                             force error on rewrite of existing "
        "output file (default: false).\n"
        "     -rose:noclobber_if_different_output_file\n"
        "                             force error on rewrite of existing "
        "output file only if result\n"
        "                             if a different output file (default: "
        "false). \n"
        "     -rose:appendPID\n"
        "                             append PID into the temporary output "
        "name. \n"
        "                             This can avoid issues in parallel "
        "compilation (default: false). \n"
        "     -rose:unparse_tokens\n"
        "                             Unparses code using original token "
        "stream where possible.\n"
        "                             Only C/C++ are supported now. Fortran "
        "support is under development \n"
        "     -rose:unparse_template_ast\n"
        "                             unparse C++ templates from their AST. "
        "\n"
        "\n"
        "Debugging options:\n"
        "     -rose:detect_dangling_pointers LEVEL \n"
        "                             detects references to previously "
        "deleted IR nodes in the AST\n"
        "                             (part of AST consistancy tests, "
        "default is false since some codes fail this test)\n"
        "                             LEVEL is one of:\n"
        "                               0: off (does not issue warning)\n"
        "                               1: on (issues warning with "
        "information)\n"
        "                               2: on (issues error and exists)\n"
        "\n"
        "Testing Options:\n"
        "     -rose:negative_test     test ROSE using input that is "
        "expected to fail\n"
        "                               (returns 0 if input test failed, "
        "else error if\n"
        "                               passed)\n",
        stdout);
}

void SgFile::processRoseCommandLineOptions(vector<string> &argv) {
  // Reject removed APIs before any command-line mutation can hide them.
  rejectRemovedRoseLanguageSelectionOptions(argv);
  rejectRemovedRexLegacyOptions(argv);

  // This report is owned by the project, but a parsed translation unit may
  // retain the exact project option in its derived command.  Consume that
  // option only after verifying its arity, uniqueness, and project-owned
  // value.
  const std::string compilationPerformanceOption =
      "-rose:compilationPerformanceFile";
  const auto compilationPerformanceArgument =
      std::find(argv.begin(), argv.end(), compilationPerformanceOption);
  if (compilationPerformanceArgument != argv.end()) {
    if (std::find(std::next(compilationPerformanceArgument), argv.end(),
                  compilationPerformanceOption) != argv.end()) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[compilation-performance-option]: "
              "option '%s' appears more than once in one translation-unit "
              "command\n",
              compilationPerformanceOption.c_str());
      ROSE_ABORT();
    }
    const auto compilationPerformanceValue =
        std::next(compilationPerformanceArgument);
    if (compilationPerformanceValue == argv.end()) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[compilation-performance-option]: "
              "option '%s' requires an exact output filename\n",
              compilationPerformanceOption.c_str());
      ROSE_ABORT();
    }
    SgProject *project = get_project();
    if (project == nullptr || project->get_compilationPerformanceFile() !=
                                  *compilationPerformanceValue) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[compilation-performance-owner]: "
              "file=%p option=%s project=%p project-option=%s\n",
              static_cast<void *>(this), compilationPerformanceValue->c_str(),
              static_cast<void *>(project),
              project == nullptr
                  ? "<missing>"
                  : project->get_compilationPerformanceFile().c_str());
      ROSE_ABORT();
    }
    argv.erase(compilationPerformanceArgument,
               std::next(compilationPerformanceValue));
  }

  const std::string sameDirectoryOption =
      "-rose:unparse_in_same_directory_as_input_file";
  const auto sameDirectoryArgument =
      std::find(argv.begin(), argv.end(), sameDirectoryOption);
  if (sameDirectoryArgument != argv.end()) {
    if (std::find(std::next(sameDirectoryArgument), argv.end(),
                  sameDirectoryOption) != argv.end()) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[same-directory-option]: option '%s' "
              "appears more than once in one translation-unit command\n",
              sameDirectoryOption.c_str());
      ROSE_ABORT();
    }
    SgProject *project = get_project();
    if (project == nullptr ||
        !project->get_unparse_in_same_directory_as_input_file()) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[same-directory-option]: option '%s' "
              "does not match the typed project setting\n",
              sameDirectoryOption.c_str());
      ROSE_ABORT();
    }
    argv.erase(sameDirectoryArgument);
  }

  // Output overwrite policy is project state, not a Clang driver option.
  // Consume each exact zero-arity spelling at the SgFile boundary only after
  // proving that it agrees with the already parsed project state.  Keeping
  // these options in the derived translation-unit command would make Clang
  // diagnose a valid REX option, while removing an unvalidated spelling would
  // hide a malformed command.
  auto consumeProjectBooleanOption = [this, &argv](const std::string &option,
                                                   bool projectValue,
                                                   const char *invariant) {
    const auto argument = std::find(argv.begin(), argv.end(), option);
    if (argument == argv.end())
      return;
    if (std::find(std::next(argument), argv.end(), option) != argv.end()) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[%s]: option '%s' appears more than "
              "once in one translation-unit command\n",
              invariant, option.c_str());
      ROSE_ABORT();
    }
    if (get_project() == nullptr || !projectValue) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[%s]: option '%s' does not match the "
              "typed project setting\n",
              invariant, option.c_str());
      ROSE_ABORT();
    }
    argv.erase(argument);
  };
  SgProject *project = get_project();
  consumeProjectBooleanOption("-rose:noclobber_output_file",
                              project != nullptr &&
                                  project->get_noclobber_output_file(),
                              "noclobber-output-option");
  consumeProjectBooleanOption(
      "-rose:noclobber_if_different_output_file",
      project != nullptr && project->get_noclobber_if_different_output_file(),
      "noclobber-different-output-option");

  // Split out the ROSE options first

  // printf ("Before processing ROSE options argc = %d \n",argc);

  //
  // help option (allows alternative -h or -help instead of just -rose:help)
  // This is the only rose options that does not require the "-rose:" prefix
  //
  if (CommandlineProcessing::isOption(argv, "-rose:", "(h|help)", true) ==
          true ||
      CommandlineProcessing::isOption(argv, "--", "(h|help)", true) == true ||
      CommandlineProcessing::isOption(argv, "-", "(h|help)", true) == true) {
    cout << version_message() << endl;
    usage();
    exit(0);
  }
  //
  // version option
  //
  if (CommandlineProcessing::isOption(argv, "-rose:", "(V|version)", true) ==
      true) {
    // function in SAGE III to access version number of frontend
    extern std::string frontendVersionString();
    cout << version_message() << endl;
    printf("     Using C/C++ frontend: %s \n", frontendVersionString().c_str());
  }

  //
  // markGeneratedFiles option
  //
  set_markGeneratedFiles(false);
  if (CommandlineProcessing::isOption(argv, "-rose:", "(markGeneratedFiles)",
                                      true) == true) {
    // Optionally mark the generated files with "#define ROSE_GENERATED_CODE"
    set_markGeneratedFiles(true);
  }

  //
  // verbose option
  //
  int integerOptionForVerbose = 0;
  if (CommandlineProcessing::isOptionWithParameter(
          argv, "-rose:", "(v|verbose)", integerOptionForVerbose, true) ==
      true) {
    // set_verbose(true);
    set_verbose(integerOptionForVerbose);

    // DQ (8/12/2004): The semantics is to set the global concept of a value
    // for verbose to the maximum of that from the individual files.
    if (SgProject::get_verbose() < integerOptionForVerbose)
      SgProject::set_verbose(integerOptionForVerbose);

    if (SgProject::get_verbose() >= 1)
      printf("verbose mode ON (for SgFile)\n");
  }

  set_cray_pointer_support(false);
  ROSE_ASSERT(get_cray_pointer_support() == false);
  if (CommandlineProcessing::isOption(argv, "-rose:", "(cray_pointer_support)",
                                      true) == true) {
    if (SgProject::get_verbose() >= 1)
      printf("cray pointer mode ON \n");
    set_cray_pointer_support(true);
  }
  if (std::find(argv.begin(), argv.end(), "-fcray-pointer") != argv.end()) {
    fprintf(stderr, "REX_FRONTEND_INVARIANT[removed-cray-pointer-option]: "
                    "-fcray-pointer is not a supported Flang option; use "
                    "-rose:cray_pointer_support explicitly\n");
    ROSE_ABORT();
  }
  if (std::find(argv.begin(), argv.end(), "-fno-cray-pointer") != argv.end()) {
    fprintf(stderr, "REX_FRONTEND_INVARIANT[unsupported-cray-pointer-option]: "
                    "-fno-cray-pointer is not supported by the pinned Flang "
                    "frontend\n");
    ROSE_ABORT();
  }

  set_exit_after_parser(false);
  ROSE_ASSERT(get_exit_after_parser() == false);
  if (CommandlineProcessing::isOption(argv, "-rose:", "(exit_after_parser)",
                                      true) == true) {
    if (SgProject::get_verbose() >= 1)
      printf("exit after parser mode ON \n");
    set_exit_after_parser(true);
  }

  if (CommandlineProcessing::isOption(argv, "-rose:", "(skip_parser)", true) ==
      true) {
    if (SgProject::get_verbose() >= 1)
      printf("skip parser mode ON \n");
    set_skip_parser(true);
  }

  // Skip all ROSE frontend processing and preserve only command line
  // bookkeeping. This is used by frontendShell() to build project/file nodes
  // without invoking parsing, transformation, or backend compilation.
  if (CommandlineProcessing::isOption(argv, "-rose:", "(skip_rose)", true) ==
          true ||
      CommandlineProcessing::isOption(argv, "--rose:", "(skip_rose)", true) ==
          true) {
    if (SgProject::get_verbose() >= 1)
      printf("skip rose mode ON \n");
    set_useBackendOnly(true);
    set_skip_transformation(true);
    set_skipfinalCompileStep(true);
    set_skip_commentsAndDirectives(true);
  }

  //
  // DQ (11/20/2010): Added token handling support.
  // Turn on the output of the tokens from the parser (only applies to C and
  // Fortran support).
  //
  set_unparse_tokens(false);
  ROSE_ASSERT(get_unparse_tokens() == false);
  if (CommandlineProcessing::isOption(argv, "-rose:", "(unparse_tokens)",
                                      true) == true) {
    if (SgProject::get_verbose() >= 1)
      printf("unparse tokens mode ON \n");
    set_unparse_tokens(true);
  }

  // Liao 12/15/2016,  support unparsing template AST
  set_unparse_template_ast(false);
  ROSE_ASSERT(get_unparse_template_ast() == false);
  if (CommandlineProcessing::isOption(argv, "-rose:", "unparse_template_ast",
                                      true) == true) {
    if (SgProject::get_verbose() >= 1)
      printf("unparse template AST mode ON \n");
    set_unparse_template_ast(true);
  }

  if (CommandlineProcessing::isOption(argv, "-rose:", "(skip_syntax_check)",
                                      true) == true) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[removed-skip-syntax-check]: "
            "-rose:skip_syntax_check was removed because Flang semantic "
            "validation is mandatory\n");
    ROSE_ABORT();
  }

  if (CommandlineProcessing::isOption(argv, "-rose:", "(relax_syntax_check)",
                                      true) == true) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[removed-relax-syntax-check]: "
            "-rose:relax_syntax_check was removed because Flang semantic "
            "validation is mandatory\n");
    ROSE_ABORT();
  }

  // DQ (11/27/2020): Turn on generation of GraphViz representation of Clang's
  // internal representation
  set_clang_il_to_graphviz(false);
  ROSE_ASSERT(get_clang_il_to_graphviz() == false);
  if (CommandlineProcessing::isOption(argv, "-rose:", "clang_il_to_graphviz",
                                      true) == true) {
    if (SgProject::get_verbose() >= 1)
      printf("Clang IL to GraphViz ON \n");
    set_clang_il_to_graphviz(true);
  }

  // TV (10/01/2018): ROSE-1424

  // DQ (5/24/2015): Record type of optimization (-Os, -O, -O1, -O2, -O3,
  // -O4, -O5), note -O0 means no optimization. This is required so that
  // when optimization is specified we can turn on the __OPTIMIE__ macro.
  // See test2015_153.c.
  // if ( CommandlineProcessing::isOption(argv,"-O","('
  // '|0|1|2|3|4|5|s)",true) == true )
  if (CommandlineProcessing::isOption(argv, "-O", "(1|2|3|4|5|s)", false) ==
      true) {
    // printf ("optimizaztion specified on commend line (specific level
    // provided) \n");
    set_optimization(true);
  } else {
    if (CommandlineProcessing::isOption(argv, "-O", "", false) == true) {
      // printf ("optimizaztion specified on commend line (default level
      // specified: -O) \n");
      set_optimization(true);
    } else {
      // printf ("optimization not specified on commend line (-O0 or no
      // optimization specified) \n");
      set_optimization(false);
    }
  }

  ////////////////////////////////////////////////////////////////////////
  // START parsing standard specifications for C/C++/Fortran (ROSE-1529)

  // Parsing ROSE's Fortran dialect specification

  if (CommandlineProcessing::isOption(argv, "-rose:", "(fortran)", true) ==
      true) {
    set_Fortran_only(true);
  }

  if (CommandlineProcessing::isOption(
          argv, "-rose:",
          "(f|F|Fortran|f77|F77|Fortran77|f90|F90|Fortran90|f95|F95|Fortran95|"
          "f2003|F2003|Fortran2003|f2008|F2008|Fortran2008)",
          true) == true) {
    std::cerr << "[FATAL] [ROSE] [frontend] [Fortran] "
                 "error: Legacy -rose:Fortran* and -rose:fXX options are "
                 "removed. Use -rose:fortran for language selection and "
                 "-rose:fortran_std=<f77|f90|f95|f2003|f2008|f2018> for "
                 "dialect selection.\n";
    ROSE_ABORT();
  }

  std::string fortranStandardOption;
  if (CommandlineProcessing::isOptionWithParameter(
          argv, "-rose:", "(fortran_std)", fortranStandardOption, true) ==
      true) {
    std::string value =
        Rose::StringUtility::convertToLowerCase(fortranStandardOption);
    if (value == "f77" || value == "fortran77") {
      set_F77_only();
    } else if (value == "f90" || value == "fortran90") {
      set_F90_only();
    } else if (value == "f95" || value == "fortran95") {
      set_F95_only();
    } else if (value == "f2003" || value == "f03" || value == "fortran2003") {
      set_F2003_only();
    } else if (value == "f2008" || value == "f08" || value == "fortran2008") {
      set_F2008_only();
    } else if (value == "f2018" || value == "f18" || value == "fortran2018") {
      set_F2018_only();
    } else {
      std::cerr << "[FATAL] [ROSE] [frontend] [Fortran] "
                   "error: Unknown Fortran standard '"
                << fortranStandardOption
                << "'. Use f77, f90, f95, f2003, f2008, or f2018.\n";
      ROSE_ABORT();
    }
  }

  if (CommandlineProcessing::isOption(argv, "-rose:", "(CoArrayFortran)",
                                      true) == true) {
    set_CoArrayFortran_only(true);

    // Set this as also being F2003 code since Co-Array Fortran is an extension
    // of Fortran 2003
    set_F2003_only();
  }

  // Parsing compiler-standard dialect specifications

  for (unsigned int i = 1; i < argv.size(); i++) {
    if (argv[i] == "-std=fortran") {
      std::cerr << "[FATAL] [ROSE] [frontend] [Fortran] "
                   "error: -std=fortran is no longer supported. Use "
                   "-rose:fortran for language selection or "
                   "-rose:fortran_std=<f77|f90|f95|f2003|f2008|f2018> "
                   "for dialect selection.\n";
      ROSE_ABORT();

    } else if (argv[i] == "-std=c89") {
      set_C89_only();

    } else if (argv[i] == "-std=gnu89") {
      set_C89_gnu_only();

    } else if (argv[i] == "-std=c90" || argv[i] == "-std=iso9899:1990" ||
               argv[i] == "-std=iso9899:199409") {
      set_C90_only();
      if (argv[i] == "-std=iso9899:199409") {
        printf("WARNING: Requested standard ISO C90 as modified in amendment "
               "1. ROSE will pass -std=c90 to the backend.\n");
      }

    } else if (argv[i] == "-std=gnu90") {
      set_C90_gnu_only();

    } else if (argv[i] == "-std=c99" || argv[i] == "-std=c9x" ||
               argv[i] == "-std=iso9899:1999" ||
               argv[i] == "-std=iso9899:199x") {
      set_C99_only();

    } else if (argv[i] == "-std=gnu99" || argv[i] == "-std=gnu9x") {
      set_C99_gnu_only();

    } else if (argv[i] == "-std=c11" || argv[i] == "-std=c1x" ||
               argv[i] == "-std=iso9899:2011") {
      set_C11_only();

    } else if (argv[i] == "-std=gnu11" || argv[i] == "-std=gnu1x") {
      set_C11_gnu_only();

    } else if (argv[i] == "-std=c17" || argv[i] == "-std=c18" ||
               argv[i] == "-std=iso9899:2017" ||
               argv[i] == "-std=iso9899:2018") {
      set_C17_only();

    } else if (argv[i] == "-std=gnu17" || argv[i] == "-std=gnu18") {
      set_C17_gnu_only();

    } else if (argv[i] == "-std=c23" || argv[i] == "-std=iso9899:2024") {
      set_C23_only();

    } else if (argv[i] == "-std=c2y") {
      set_C2y_only();

    } else if (argv[i] == "-std=gnu23") {
      set_C23_gnu_only();

    } else if (argv[i] == "-std=c++98") {
      set_Cxx98_only();

    } else if (argv[i] == "-std=gnu++98") {
      set_Cxx98_gnu_only();

    } else if (argv[i] == "-std=c++03") {
      set_Cxx03_only();

    } else if (argv[i] == "-std=gnu++03") {
      set_Cxx03_gnu_only();

    } else if (argv[i] == "-std=c++11" || argv[i] == "-std=c++0x") {
      set_Cxx11_only();

    } else if (argv[i] == "-std=gnu++11" || argv[i] == "-std=gnu++0x") {
      set_Cxx11_gnu_only();

    } else if (argv[i] == "-std=c++14" || argv[i] == "-std=c++1y") {
      set_Cxx14_only();

    } else if (argv[i] == "-std=gnu++14" || argv[i] == "-std=gnu++1y") {
      set_Cxx14_gnu_only();

    } else if (argv[i] == "-std=c++17" || argv[i] == "-std=c++1z") {
      set_Cxx17_only();

    } else if (argv[i] == "-std=gnu++17" || argv[i] == "-std=gnu++1z") {
      set_Cxx17_gnu_only();

    } else if (argv[i] == "-std=c++20" || argv[i] == "-std=c++2a") {
      set_Cxx20_only();

    } else if (argv[i] == "-std=gnu++20" || argv[i] == "-std=gnu++2a") {
      set_Cxx20_gnu_only();

    } else if (argv[i] == "-std=c++23" || argv[i] == "-std=c++2b") {
      set_Cxx23_only();

    } else if (argv[i] == "-std=gnu++23" || argv[i] == "-std=gnu++2b") {
      set_Cxx23_gnu_only();

    } else if (argv[i] == "-std=c++26" || argv[i] == "-std=c++2c") {
      set_Cxx26_only();

    } else if (argv[i] == "-std=gnu++26" || argv[i] == "-std=gnu++2c") {
      set_Cxx26_gnu_only();

    } else if (argv[i] == "-std=f2018") {
      set_F2018_only();
    } else if (argv[i].rfind("-std=f", 0) == 0 ||
               argv[i].rfind("-std=F", 0) == 0) {
      std::cerr << "[FATAL] [ROSE] [frontend] [Fortran] "
                   "error: Unsupported Fortran standard flag '"
                << argv[i]
                << "'. Flang accepts only -std=f2018. Use "
                   "-rose:fortran_std=<f77|f90|f95|f2003|f2008|f2018> "
                   "to select a Fortran dialect for ROSE.\n";
      ROSE_ABORT();
    }
  }

  using ExplicitClangLanguage = CommandlineProcessing::ClangLanguageFamily;
  const ExplicitClangLanguage explicitClangLanguage =
      CommandlineProcessing::clangLanguageSelectionForSource(
          argv, get_sourceFileNameWithPath())
          .family;
  if (explicitClangLanguage == ExplicitClangLanguage::C) {
    set_C_only(true);
    set_Cxx_only(false);
    set_Cuda_only(false);
    set_OpenCL_only(false);
    Rose::is_C_language = true;
    Rose::is_Cxx_language = false;
  } else if (explicitClangLanguage == ExplicitClangLanguage::Cxx) {
    set_C_only(false);
    set_Cxx_only(true);
    set_Cuda_only(false);
    set_OpenCL_only(false);
    Rose::is_C_language = false;
    Rose::is_Cxx_language = true;
  } else if (explicitClangLanguage == ExplicitClangLanguage::Cuda) {
    set_C_only(false);
    set_Cxx_only(false);
    set_Cuda_only(true);
    set_OpenCL_only(false);
  } else if (explicitClangLanguage == ExplicitClangLanguage::OpenCL) {
    set_C_only(false);
    set_Cxx_only(false);
    set_Cuda_only(false);
    set_OpenCL_only(true);
  }

  auto rejectDialectLanguageMismatch = [&](const char *dialectFamily,
                                           const char *requiredLanguage) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[dialect-language]: %s dialect is "
            "incompatible with this input language; pass '-x %s' explicitly "
            "when overriding the source suffix\n",
            dialectFamily, requiredLanguage);
    ROSE_ABORT();
  };

  // Validate selected dialect against the exact suffix/-x language.

  switch (get_standard()) {
  case e_default_standard: {
    if (get_Fortran_only() &&
        get_sourceFileUsesFortranFileExtension() == false) {
      printf("WARNING: Non Fortran source file name specified with explicit "
             "-rose:fortran Fortran language option! \n");
    }
    if (get_C_only() && get_sourceFileUsesCppFileExtension() == true) {
      if (explicitClangLanguage != ExplicitClangLanguage::C) {
        rejectDialectLanguageMismatch("C", "c");
      }
    }
    break;
  }
  case e_c89_standard:
  case e_c90_standard:
  case e_c99_standard:
  case e_c11_standard:
  case e_c17_standard:
  case e_c23_standard:
  case e_c2y_standard: {
    if (explicitClangLanguage == ExplicitClangLanguage::Cxx ||
        explicitClangLanguage == ExplicitClangLanguage::Cuda ||
        (explicitClangLanguage != ExplicitClangLanguage::C &&
         explicitClangLanguage != ExplicitClangLanguage::OpenCL &&
         get_sourceFileUsesCppFileExtension())) {
      rejectDialectLanguageMismatch("C", "c");
    }
    break;
  }
  case e_cxx98_standard:
  case e_cxx03_standard:
  case e_cxx11_standard:
  case e_cxx14_standard:
  case e_cxx17_standard:
  case e_cxx20_standard:
  case e_cxx23_standard:
  case e_cxx26_standard: {
    if (explicitClangLanguage == ExplicitClangLanguage::C ||
        explicitClangLanguage == ExplicitClangLanguage::OpenCL ||
        (explicitClangLanguage != ExplicitClangLanguage::Cxx &&
         explicitClangLanguage != ExplicitClangLanguage::Cuda &&
         get_sourceFileUsesCppFileExtension() == false)) {
      rejectDialectLanguageMismatch("C++", "c++");
    }
    break; // NOP
  }
  case e_f77_standard: {
    if (get_sourceFileUsesFortran77FileExtension() == false) {
      printf("WARNING: Non Fortran77 source file name specificed with explicit "
             "-rose:fortran_std=f77 Fortran 77 dialect option! \n");
    }
    break;
  }
  case e_f90_standard: {
    if (get_sourceFileUsesFortran90FileExtension() == false) {
      printf("WARNING: Non Fortran90 source file name specificed with explicit "
             "-rose:fortran_std=f90 Fortran 90 dialect option! \n");
    }
    break;
  }
  case e_f95_standard: {
    if (get_sourceFileUsesFortran95FileExtension() == false) {
      printf("WARNING: Non Fortran95 source file name specificed with explicit "
             "-rose:fortran_std=f95 Fortran 95 dialect option! \n");
    }
    break;
  }
  case e_f03_standard: {
    if (get_sourceFileUsesFortran2003FileExtension() == false &&
        get_sourceFileUsesCoArrayFortranFileExtension() == false) {
      printf(
          "WARNING: Non Fortran2003 source file name specificed with "
          "explicit -rose:fortran_std=f2003 Fortran 2003 dialect option! \n");
    }
    if (get_CoArrayFortran_only() == true &&
        get_sourceFileUsesCoArrayFortranFileExtension() == false) {
      printf("WARNING: Non Co-Array Fortran source file name specificed with "
             "explicit -rose:CoArrayFortran language option! \n");
    }
    break;
  }
  case e_f08_standard: {
    if (get_sourceFileUsesFortran2008FileExtension() == false) {
      printf(
          "WARNING: Non Fortran2008 source file name specificed with "
          "explicit -rose:fortran_std=f2008 Fortran 2008 dialect option! \n");
    }
    break;
  }
  case e_f18_standard: {
    break;
  }
  }

  if (get_standard() == e_default_standard) {
    // Preserve the selected compiler driver's exact default for C and C++.
    // Synthesizing a dialect here can make frontend and backend semantics
    // diverge from the command line.  Fortran keeps the explicit REX default
    // because its source form/dialect state is part of the Sage file model.
    if (get_Fortran_only()) {
      set_F2003_only();
    }
  }

  if (SgProject::get_verbose() >= 1) {
    switch (get_standard()) {
    case e_default_standard:
      break; // NOP
    case e_c89_standard: {
      printf("C89 mode ON \n");
      break;
    }
    case e_c90_standard: {
      printf("C90 mode ON \n");
      break;
    }
    case e_c99_standard: {
      printf("C99 mode ON \n");
      break;
    }
    case e_c11_standard: {
      printf("C11 mode ON \n");
      break;
    }
    case e_c17_standard: {
      printf("C17 mode ON \n");
      break;
    }
    case e_c23_standard: {
      printf("C23 mode ON \n");
      break;
    }
    case e_c2y_standard: {
      printf("C2y mode ON \n");
      break;
    }
    case e_cxx98_standard: {
      printf("C++98 mode ON \n");
      break;
    }
    case e_cxx03_standard: {
      printf("C++03 mode ON \n");
      break;
    }
    case e_cxx11_standard: {
      printf("C++11 mode ON \n");
      break;
    }
    case e_cxx14_standard: {
      printf("C++14 mode ON \n");
      break;
    }
    case e_cxx17_standard: {
      printf("C++17 mode ON \n");
      break;
    }
    case e_cxx20_standard: {
      printf("C++20 mode ON \n");
      break;
    }
    case e_cxx23_standard: {
      printf("C++23 mode ON \n");
      break;
    }
    case e_cxx26_standard: {
      printf("C++26 mode ON \n");
      break;
    }
    case e_f77_standard: {
      printf("Fortran77 mode ON \n");
      break;
    }
    case e_f90_standard: {
      printf("Fortran90 mode ON \n");
      break;
    }
    case e_f95_standard: {
      printf("Fortran95 mode ON \n");
      break;
    }
    case e_f03_standard: {
      printf("Fortran2003 mode ON \n");
      break;
    }
    case e_f08_standard: {
      printf("Fortran2008 mode ON \n");
      break;
    }
    case e_f18_standard: {
      printf("Fortran2018 mode ON \n");
      break;
    }
    }

    if (is_gnu_standard()) {
      printf("GNU compatibility mode ON \n");
    }
  }

  // END parsing standard specifications for C/C++/Fortran (ROSE-1529)
  ////////////////////////////////////////////////////////////////////////

  // Fixed format v.s. free format option handling (ROSE defaults to fix or free
  // format, depending on the file extension). F77 default is fixed format, F90
  // and later default is free format. For Flang, use -ffixed-form/-ffree-form.
  // Fixed form line length can be set with -ffixed-line-length=<n> (use 0 for
  // no limit).

  if (get_F77_only() == true) {
    // Use the setting get_F77_only() == true as a default means to set this
    // value
    set_inputFormat(SgFile::e_fixed_form_output_format);
    set_outputFormat(SgFile::e_fixed_form_output_format);
    set_backendCompileFormat(SgFile::e_fixed_form_output_format);
  } else {
    // Use the setting get_F77_only() == true as a default means to set this
    // value
    set_inputFormat(SgFile::e_free_form_output_format);
    set_outputFormat(SgFile::e_free_form_output_format);
    set_backendCompileFormat(SgFile::e_free_form_output_format);
  }

  if (CommandlineProcessing::isOption(
          argv, "-", "(ffixed-form|fixed|Mfixed|qfixed)", true) == true) {
    if (SgProject::get_verbose() >= 1)
      printf("Fortran fixed format mode explicitly set: ON \n");
    set_inputFormat(SgFile::e_fixed_form_output_format);
    if (get_sourceFileUsesFortranFileExtension() == false) {
      printf("Error, Non Fortran source file name specificed with explicit "
             "fixed format code generation (unparser) option! \n");
      ROSE_ABORT();
    }
  }

  if (CommandlineProcessing::isOption(
          argv, "-", "(ffree-form|free|Mfree|qfree)", true) == true) {
    if (SgProject::get_verbose() >= 1) {
      printf("Fortran free format mode explicitly set: ON \n");
    }
    set_inputFormat(SgFile::e_free_form_output_format);
    if (get_sourceFileUsesFortranFileExtension() == false) {
      printf("Error, Non Fortran source file name specificed with explicit "
             "free format code generation (unparser) option! \n");
      ROSE_ABORT();
    }
  }

  if (CommandlineProcessing::isOption(
          argv, "-rose:", "(fixedOutput|fixedFormatOutput)", true) == true) {
    if (SgProject::get_verbose() >= 1)
      printf("Fortran fixed format output specified \n");
    set_outputFormat(SgFile::e_fixed_form_output_format);
    if (get_sourceFileUsesFortranFileExtension() == false) {
      printf("Warning, Non Fortran source file name specificed with code "
             "generation option: free format! \n");
      ROSE_ABORT();
    }
  }
  if (CommandlineProcessing::isOption(
          argv, "-rose:", "(freeOutput|freeFormatOutput)", true) == true) {
    if (SgProject::get_verbose() >= 1)
      printf("Fortran free format mode explicitly set: ON \n");
    set_outputFormat(SgFile::e_free_form_output_format);
    if (get_sourceFileUsesFortranFileExtension() == false) {
      printf("Warning, Non Fortran source file name specificed with code "
             "generation option: fixed format! \n");
      ROSE_ABORT();
    }
  }

  if (CommandlineProcessing::isOption(
          argv, "-rose:", "(compileFixed|backendCompileFixedFormat)", true) ==
      true) {
    if (SgProject::get_verbose() >= 1)
      printf("Fortran fixed format output specified \n");
    set_backendCompileFormat(SgFile::e_fixed_form_output_format);
    if (get_sourceFileUsesFortranFileExtension() == false) {
      printf("Warning, Non Fortran source file name specificed with code "
             "generation option: free format! \n");
      ROSE_ABORT();
    }
  }

  if (CommandlineProcessing::isOption(
          argv, "-rose:", "(compileFree|backendCompileFreeFormat)", true) ==
      true) {
    if (SgProject::get_verbose() >= 1)
      printf("Fortran free format mode explicitly set: ON \n");
    set_backendCompileFormat(SgFile::e_free_form_output_format);
    if (get_sourceFileUsesFortranFileExtension() == false) {
      printf("Warning, Non Fortran source file name specificed with code "
             "generation option: fixed format! \n");
      ROSE_ABORT();
    }
  }

  // Liao 3/12/2020: handle options for OpenACC support
  // Allows handling of OpenACC directives, enabling "!$" conditional
  // compilation sentinels.
  set_openacc(false);
  string ompacc_macro = "-D_OPENACC=" + StringUtility::numberToString(3);
  ROSE_ASSERT(get_openacc() == false);
  ROSE_ASSERT(get_openacc_parse_only() == false);
  ROSE_ASSERT(get_openacc_ast_only() == false);
  if (CommandlineProcessing::isOption(argv, "-rose:", "(OpenACC|openacc)",
                                      true) == true ||
      CommandlineProcessing::isOption(argv, "-", "(acc|openacc|fopenacc)",
                                      true) == true) {
    if (SgProject::get_verbose() >= 1)
      printf("OpenACC option specified \n");
    set_openacc(true);
    set_openacc_parse_only(true); // default is parse_only for now
    argv.push_back(ompacc_macro);
  }

  // Process sub-options
  if (CommandlineProcessing::isOption(argv, "-rose:OpenACC:", "parse_only",
                                      true) == true ||
      CommandlineProcessing::isOption(argv, "-rose:openacc:", "parse_only",
                                      true) == true ||
      CommandlineProcessing::isOption(argv, "--rose:OpenACC:", "parse_only",
                                      true) == true ||
      CommandlineProcessing::isOption(argv, "--rose:openacc:", "parse_only",
                                      true) == true) {
    if (SgProject::get_verbose() >= 1)
      printf("OpenACC sub option for parsing specified \n");
    set_openacc_parse_only(true);
    if (!get_openacc()) {
      set_openacc(true);
      argv.push_back(ompacc_macro);
    }
  }

  if (CommandlineProcessing::isOption(argv, "-rose:OpenACC:", "ast_only",
                                      true) == true ||
      CommandlineProcessing::isOption(argv, "-rose:openacc:", "ast_only",
                                      true) == true ||
      CommandlineProcessing::isOption(argv, "--rose:openacc:", "ast_only",
                                      true) == true ||
      CommandlineProcessing::isOption(argv, "--rose:OpenACC:", "ast_only",
                                      true) == true) {
    if (SgProject::get_verbose() >= 1)
      printf("OpenACC option for AST construction specified \n");
    set_openacc_ast_only(true);
    set_openacc_parse_only(false);
    if (!get_openacc()) {
      set_openacc(true);
      argv.push_back(ompacc_macro);
    }
  }

  // Liao 10/28/2008: I changed it to a more generic flag to indicate support
  // for either Fortran or C/C++ DQ (8/19/2007): I have added the option here so
  // that we can start to support OpenMP for Fortran. Allows handling of OpenMP
  // "!$omp" directives in free form and "c$omp", *$omp and "!$omp" directives
  // in fixed form, enables "!$" conditional compilation sentinels in free form
  // and "c$", "*$" and "!$" sentinels in fixed form and when linking arranges
  // for the OpenMP runtime library to be linked in. (Not implemented yet).
  set_openmp(false);
  string ompmacro = "-D_OPENMP=" + StringUtility::numberToString(OMPVERSION);
  ROSE_ASSERT(get_openmp() == false);
  // OpenMP is disabled until an exact REX/driver option enables the pragma
  // parser and Sage OpenMP AST constructor below.  These assertions protect
  // the command-line object's pre-option state; parse-only is inert while
  // OpenMP itself is disabled.
  ROSE_ASSERT(get_openmp_parse_only() == true);
  ROSE_ASSERT(get_openmp_ast_only() == false);
  ROSE_ASSERT(get_openmp_lowering() == false);

  // The command-line layer is the common C, C++, and Fortran ownership
  // boundary for OpenMP runtime selection.  Validate joined driver options
  // before any language adapter can consume or normalize them.
  for (const string &argument : argv) {
    if (argument.rfind("-fopenmp=", 0) != 0) {
      continue;
    }
    const string runtime = argument.substr(9);
    if (runtime != "libomp") {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[rex-openmp-option]: option '%s' "
              "selects a runtime other than REX's required LLVM libomp\n",
              argument.c_str());
      ROSE_ABORT();
    }
  }

  // Joined runtime options select REX's pragma parser and OpenMP AST
  // constructor here; language adapters may preserve only the frontend syntax
  // switch needed to retain directive provenance.
  const bool joined_openmp_driver_option_present =
      std::any_of(argv.begin(), argv.end(), [](const string &argument) {
        return argument.rfind("-fopenmp=", 0) == 0;
      });
  const bool rose_openmp_option_present =
      CommandlineProcessing::isOption(argv, "-rose:", "(OpenMP|openmp)", true);
  const bool standalone_openmp_driver_option_present =
      CommandlineProcessing::isOption(argv, "-",
                                      "(openmp|fopenmp|fopenmp-simd)", true);
  const bool openmp_driver_option_present =
      standalone_openmp_driver_option_present ||
      joined_openmp_driver_option_present;

  if (rose_openmp_option_present || openmp_driver_option_present) {
    if (SgProject::get_verbose() >= 1)
      printf("OpenMP option specified \n");

    set_openmp(true);
    argv.push_back(ompmacro);
    if (openmp_driver_option_present) {
      set_openmp_parse_only(false);
      set_openmp_ast_only(true);
    }
  }

  // Process sub-options for OpenMP handling, Liao 5/31/2009
  // We want to turn on OpenMP if any of its suboptions is used.  Liao ,
  // 8/11/2009
  if (CommandlineProcessing::isOption(argv, "-rose:OpenMP:", "parse_only",
                                      true) == true ||
      CommandlineProcessing::isOption(argv, "-rose:openmp:", "parse_only",
                                      true) == true ||
      CommandlineProcessing::isOption(argv, "--rose:openmp:", "parse_only",
                                      true) == true ||
      CommandlineProcessing::isOption(argv, "--rose:OpenMP:", "parse_only",
                                      true) == true) {
    if (SgProject::get_verbose() >= 1)
      printf("OpenMP sub option for parsing specified \n");
    set_openmp_parse_only(true);
    set_openmp_ast_only(false);
    // turn on OpenMP if not set explicitly by standalone -rose:OpenMP
    if (!get_openmp()) {
      set_openmp(true);
      if (!Outliner::select_omp_loop)
        argv.push_back(ompmacro);
    }
  }

  if (CommandlineProcessing::isOption(argv, "-rose:OpenMP:", "ast_only",
                                      true) == true ||
      CommandlineProcessing::isOption(argv, "-rose:openmp:", "ast_only",
                                      true) == true ||
      CommandlineProcessing::isOption(argv, "--rose:openmp:", "ast_only",
                                      true) == true ||
      CommandlineProcessing::isOption(argv, "--rose:OpenMP:", "ast_only",
                                      true) == true) {
    if (SgProject::get_verbose() >= 1)
      printf("OpenMP option for AST construction specified \n");
    set_openmp_ast_only(true);
    // we don't want to stop after parsing  if we want to proceed to ast
    // creation before stopping
    set_openmp_parse_only(false);
    // turn on OpenMP if not set explicitly by standalone -rose:OpenMP
    if (!get_openmp()) {
      set_openmp(true);
      if (!Outliner::select_omp_loop)
        argv.push_back(ompmacro);
    }
  }

  if (CommandlineProcessing::isOption(argv, "-rose:OpenMP:", "analyzing",
                                      true) == true ||
      CommandlineProcessing::isOption(argv, "-rose:openmp:", "analyzing",
                                      true) == true ||
      CommandlineProcessing::isOption(argv, "--rose:openmp:", "analyzing",
                                      true) == true ||
      CommandlineProcessing::isOption(argv, "--rose:OpenMP:", "analyzing",
                                      true) == true) {
    if (SgProject::get_verbose() >= 1)
      printf("OpenMP sub option for AST analyzing specified \n");
    set_openmp_analyzing(true);
    // we don't want to stop after parsing or ast construction
    set_openmp_parse_only(false);
    set_openmp_ast_only(false);
    // turn on OpenMP if not set explicitly by standalone -rose:OpenMP
    if (!get_openmp()) {
      set_openmp(true);
      if (!Outliner::select_omp_loop)
        argv.push_back(ompmacro);
    }
  }

  if (CommandlineProcessing::isOption(argv, "-rose:OpenMP:", "lowering",
                                      true) == true ||
      CommandlineProcessing::isOption(argv, "-rose:openmp:", "lowering",
                                      true) == true ||
      CommandlineProcessing::isOption(argv, "--rose:openmp:", "lowering",
                                      true) == true ||
      CommandlineProcessing::isOption(argv, "--rose:OpenMP:", "lowering",
                                      true) == true) {
    if (SgProject::get_verbose() >= 1)
      printf("OpenMP sub option for AST lowering specified \n");
    set_openmp_lowering(true);
    // we don't want to stop after parsing or ast construction
    set_openmp_parse_only(false);
    set_openmp_ast_only(false);
    set_openmp_analyzing(false);
    // turn on OpenMP if not set explicitly by standalone -rose:OpenMP
    if (!get_openmp()) {
      set_openmp(true);
      if (!Outliner::select_omp_loop)
        argv.push_back(ompmacro);
    }
  }

  // REX-specific convenience options for OpenMP
  // --rex-omp-ast-only: equivalent to -fopenmp --rose:openmp:ast_only
  if (CommandlineProcessing::isOption(argv, "--rex-omp-", "ast-only", true) ==
      true) {
    if (SgProject::get_verbose() >= 1)
      printf("REX OpenMP convenience option: --rex-omp-ast-only (enables "
             "-fopenmp --rose:openmp:ast_only)\n");
    set_openmp_ast_only(true);
    set_openmp_parse_only(false);
    // turn on OpenMP if not set explicitly
    if (!get_openmp()) {
      set_openmp(true);
      if (!Outliner::select_omp_loop)
        argv.push_back(ompmacro);
    }
  }

  // --rex-omp-lowering: equivalent to -fopenmp -rose:openmp:lowering
  // -rose:skipfinalCompileStep
  if (CommandlineProcessing::isOption(argv, "--rex-omp-", "lowering", true) ==
      true) {
    if (SgProject::get_verbose() >= 1)
      printf("REX OpenMP convenience option: --rex-omp-lowering (enables "
             "-fopenmp -rose:openmp:lowering -rose:skipfinalCompileStep)\n");
    set_openmp_lowering(true);
    set_openmp_parse_only(false);
    set_openmp_ast_only(false);
    set_openmp_analyzing(false);
    set_skipfinalCompileStep(true);
    // turn on OpenMP if not set explicitly
    if (!get_openmp()) {
      set_openmp(true);
      if (!Outliner::select_omp_loop)
        argv.push_back(ompmacro);
    }
  }

  // Patrick, 9/22/2022
  if (CommandlineProcessing::isOption(argv, "-rose:simd:", "(intel-avx)",
                                      true) == true) {
    simd_arch = Intel_AVX512;
    // Intel SIMD lowering emits the public intrinsic ABI, so its frontend
    // transaction must parse that ABI before building any transformed nodes.
    // This gives every generated vector, mask, and intrinsic reference an
    // exact declaration and also preserves the same forced include for final
    // compilation of the transformed output.
    argv.push_back("-include");
    argv.push_back("immintrin.h");
  } else if (CommandlineProcessing::isOption(argv, "-rose:simd:", "(arm-sve)",
                                             true) == true) {
    simd_arch = Arm_SVE2;
  } else if (CommandlineProcessing::isOption(argv, "-rose:simd:", "(addr3)",
                                             true) == true) {
    simd_arch = Addr3;
  }

  //
  // strict ANSI/ISO mode option
  //
  if (CommandlineProcessing::isOption(argv, "-rose:", "(strict)", true) ==
      true) {
    // Optionally specify strict language handling
    set_strict_language_handling(true);
  }

  //
  // specify output file option
  //
  // DQ (10/15/2005): There is a valid default value here, but we want to
  // overwrite it!
  std::string stringParameter;
  if (CommandlineProcessing::isOptionWithParameter(
          argv, "-rose:", "(o|output)", stringParameter, true) == true) {
    // printf ("-rose:output %s \n",stringParameter.c_str());
    if (get_unparse_output_filename().empty() == false) {
      printf("Overwriting value in get_unparse_output_filename() = %s \n",
             get_unparse_output_filename().c_str());
    }

    // DQ (10/15/2005): This is so much simpler!
    p_unparse_output_filename = stringParameter;
  }

  //

  //
  // skip_transformation option: if transformations of the AST check this
  // variable then the resulting translators can skip the transformatios via
  // this command-line specified option.
  //
  if (CommandlineProcessing::isOption(argv, "-rose:", "(skip_transformation)",
                                      true) == true) {
    if (SgProject::get_verbose() >= 1)
      printf("option -rose:skip_transformation found \n");
    set_skip_transformation(true);
  }

  if (CommandlineProcessing::isOption(argv, "-rose:", "(skip_unparse)", true) ==
      true) {
    if (SgProject::get_verbose() >= 1)
      printf("option -rose:skip_unparse found \n");
    set_skip_unparse(true);
  }

  // showBackendCommandLine: Outputs the command line passed to the backend
  // compiler
  //
  if (CommandlineProcessing::isOption(
          argv, "-rose:", "(showBackendCommandLine)", true) == true) {
    if (SgProject::get_verbose() >= 1)
      printf("option -rose:showBackendCommandLine found \n");
    SgProject::set_showBackendCommandLine(true);
  }

  // unparser language option
  // DQ (8/27/2007): This option controls the output language (which language
  // unparser is to be used). This allows the code generation of one language to
  // be tested using input of another langauge.  It is mostly a mechanism to
  // test the unparser in the early stages of their development. Unshared
  // language constructs are not supported and would have the be translated.
  // This step does none of this sort of translation, which might be difficult
  // debendingon the input and output languages selected. Supported options are:
  //      e_default_output_language
  //      e_C_output_language
  //      e_Cxx_output_language
  //      e_Fortran_output_language
  if (CommandlineProcessing::isOption(argv, "-rose:", "C_output_language",
                                      true) == true) {
    if (SgProject::get_verbose() >= 1)
      printf("Use the C language code generator (unparser) \n");
    set_outputLanguage(SgFile::e_C_language);
  }
  if (CommandlineProcessing::isOption(argv, "-rose:", "Cxx_output_language",
                                      true) == true) {
    if (SgProject::get_verbose() >= 1)
      printf("Use the C++ language code generator (unparser) \n");
    set_outputLanguage(SgFile::e_Cxx_language);
  }
  if (CommandlineProcessing::isOption(argv, "-rose:", "Fortran_output_language",
                                      true) == true) {
    if (SgProject::get_verbose() >= 1)
      printf("Use the Fortran language code generator (unparser) \n");
    set_outputLanguage(SgFile::e_Fortran_language);
  }

  //
  // unparse_includes option
  //
  if (CommandlineProcessing::isOption(argv, "-rose:", "(unparse_includes)",
                                      true) == true) {
    // printf ("option -rose:unparse_includes found \n");
    set_unparse_includes(true);
  }

  //
  // unparse_line_directives option (added 12/4/2007).
  //
  if (CommandlineProcessing::isOption(
          argv, "-rose:", "(unparse_line_directives)", true) == true) {
    // printf ("option -rose:unparse_line_directives found \n");
    set_unparse_line_directives(true);
  }

  //
  // collectAllCommentsAndDirectives option: operates across all files (include
  // files) and significantly slows the compilation.
  //
  if (CommandlineProcessing::isOption(
          argv, "-rose:", "(collectAllCommentsAndDirectives)", true) == true) {
    // printf ("option -rose:collectAllCommentsAndDirectives found \n");
    set_collectAllCommentsAndDirectives(true);
  }

  // negara1 (07/08/2011): Made unparsing of header files optional.
  if (CommandlineProcessing::isOption(argv, "-rose:", "(unparseHeaderFiles)",
                                      true) == true) {
    set_unparseHeaderFiles(true);
    // To support header files unparsing, it is always needed to collect all
    // directives.
    set_collectAllCommentsAndDirectives(true);
  }

  // negara1 (08/16/2011): A user may optionally specify the root folder for the
  // unparsed header files.
  if (CommandlineProcessing::isOptionWithParameter(
          argv, "-rose:", "(unparseHeaderFilesRootFolder)", stringParameter,
          true) == true) {
    // Although it is specified per file, it should be the same for the whole
    // project.
    get_project()->set_unparseHeaderFilesRootFolder(stringParameter);
  }

  // DQ (11/6/2018): Added to support a specification of the application
  // directory for source file and header file unparsing.
  if (CommandlineProcessing::isOptionWithParameter(
          argv, "-rose:", "(applicationRootDirectory)", stringParameter,
          true) == true) {
    // Although it is specified per file, it should be the same for the whole
    // project.
    get_project()->set_applicationRootDirectory(stringParameter);
    get_project()->set_usingApplicationRootDirectory(true);
  }

  //
  // skip_commentsAndDirectives option: if analysis that does not use comments
  // or CPP directives is required then this option can improve the performance
  // of the compilation.
  //
  if (CommandlineProcessing::isOption(
          argv, "-rose:", "(skip_commentsAndDirectives)", true) == true) {
    // printf ("option -rose:skip_commentsAndDirectives found \n");
    set_skip_commentsAndDirectives(true);

    // If we are skipping comments then we should not be collecting all comments
    // (does not make sense)
    ROSE_ASSERT(get_collectAllCommentsAndDirectives() == false);
  }

  // DQ (9/26/2011): Adding options to support internal debugging of ROSE based
  // tools and language support.
  // ****************
  // DEBUGING SUPPORT
  // ****************
  //
  // Support for detecting dangling pointers
  //     This is the first level of this sort of support. This level will be
  //     fooled by any reuse of the memory (from new allocations) previously
  //     deleted. A later leve of this option will disable reuse of previously
  //     deleted memory for IR nodes; but it will consume more memory for
  //     translators that delete a lot of IR nodes as part of there
  //     functionality.  I expect this to only seriously make a difference for
  //     the AST merge operations which allocate and delete large parts of ASTs
  //     frequently.
  //     Note: this detects only dangling pointers to ROSE IR nodes, nothing
  //     more.
  //
  int integerDebugOption = 0;
  if (CommandlineProcessing::isOptionWithParameter(
          argv, "-rose:", "detect_dangling_pointers", integerDebugOption,
          true) == true) {
    // If this was set and a parameter was not specified or the value set to
    // zero, then let the value be 1. I can't seem to detect if the option was
    // specified without a parameter.
    if (integerDebugOption == 0) {
      integerDebugOption = 1;
      if (SgProject::get_verbose() >= 1)
        printf(
            "option -rose:detect_dangling_pointers found but value not "
            "specified or set to 0 default (reset integerDebugOption = %d) \n",
            integerDebugOption);
    } else {
      if (SgProject::get_verbose() >= 1)
        printf("option -rose:detect_dangling_pointers found with level "
               "(integerDebugOption = %d) \n",
               integerDebugOption);
    }

    // Set the level of the debugging to support.
    set_detect_dangling_pointers(integerDebugOption);
  }

  //
  // skipAstConsistancyTests option (added 2/17/2013).
  //
  // DQ (2/17/2013): This option allows performance evaluations using HPCToolKit
  // (using binary instrumentation) to be focusd on the AST construction phase
  // and not the AST consistancy test phase (which can be about 30% of the
  // performance of ROSE for large files).
  if (CommandlineProcessing::isOption(
          argv, "-rose:", "(skipAstConsistancyTests)", true) == true) {
    // printf ("option -rose:skipAstConsistancyTests found \n");
    set_skipAstConsistancyTests(true);
  }

  //
  //
  // new_unparser option
  //
  if (CommandlineProcessing::isOption(argv, "-rose:", "(skipfinalCompileStep)",
                                      true) == true) {
    if (get_verbose() > 0) // Liao, 8/29/2008, Only show it in verbose mode.
      printf("option -rose:skipfinalCompileStep found \n");
    set_skipfinalCompileStep(true);
  }

  //
  // Standard compiler options (allows alternative -E option to just run CPP)
  //
  if (CommandlineProcessing::isOption(argv, "-", "(E)", true) == true) {
    p_useBackendOnly = true;
    // p_skip_buildHigherLevelGrammars  = true;

    p_skip_transformation = true;
    p_skip_unparse = true;
    p_skipfinalCompileStep = false;

    // DQ (8/22/2009): Verify that this was set when the command line was
    // processed at the SgProject level.
    SgProject *project = this->get_project();
    ROSE_ASSERT(project != NULL);
    ROSE_ASSERT(project->get_C_PreprocessorOnly() == true);
  }

  // DQ (1/19/2014): This option "-S" is required for some build systems (e.g.
  // valgrind).
  //
  // Standard compiler options (allows alternative -S option to just run with
  // gcc)
  //
  if (CommandlineProcessing::isOption(argv, "-", "(S)", true) == true) {
    // printf ("/* option -S found (just run backend compiler with -S to call
    // gcc) */ \n");
    p_useBackendOnly = true;
    // p_skip_buildHigherLevelGrammars  = true;

    p_skip_transformation = true;
    p_skip_unparse = true;
    p_skipfinalCompileStep = false;

    // DQ (8/22/2009): Verify that this was set when the command line was
    // processed at the SgProject level.
    SgProject *project = this->get_project();
    ROSE_ASSERT(project != NULL);
    ROSE_ASSERT(project->get_stop_after_compilation_do_not_assemble_file() ==
                true);
  }

  //
  // Standard compiler options (allows alternative -H option to just output
  // header file info)
  //
  if (CommandlineProcessing::isOption(argv, "-", "(H)", true) == true) {
    // DQ (1/23/2018): Note, we can call the backend compiler using -H or
    // call the frontend with -H. This option will call the backend compiler
    // with -H, if we want to call the frontend with -H then we use  as the
    // option to ROSE.
    p_useBackendOnly = true;
    // p_skip_buildHigherLevelGrammars  = true;

    p_skip_transformation = true;
    p_skip_unparse = true;
    p_skipfinalCompileStep = false;
  }

  //
  // negative_test option: allows passing all tests to be treated as an error!
  //
  if (CommandlineProcessing::isOption(argv, "-rose:", "(negative_test)",
                                      true) == true) {
    printf("option -rose:negative_test found \n");
    set_negative_test(true);
  }

  if (get_verbose() > 1) {
    cout << "The remaining non-rose options will be passed onto either the "
            "parser/front-end or the backend (vendor) the compiler: "
         << endl;
    for (unsigned int i = 1; i < argv.size(); i++) {
      cout << "  argv[" << i << "]= " << argv[i] << endl;
    }
  }
}

void SgFile::stripRoseCommandLineOptions(vector<string> &argv) {
  // Strip out the rose specific commandline options
  // the assume all other arguments are to be passed onto the C or C++ compiler

  // DQ (12/9/2016): Eliminating a warning that we want to be an error:
  // -Werror=unused-but-set-variable.
  int optionCount = 0;

  // int i = 0;

  if ((ROSE_DEBUG >= 1) || (SgProject::get_verbose() > 2)) {
    printf("In stripRoseCommandLineOptions (TOP): List ALL arguments: argc = "
           "%" PRIuPTR " \n",
           argv.size());
    for (size_t i = 0; i < argv.size(); i++)
      printf("     argv[%" PRIuPTR "] = %s \n", i, argv[i].c_str());
  }

  // Split out the ROSE options first

  //----------------------------------------------------------------------------
  //
  // TOO1 (2/13/2014): Refactor all of this into the Rose::Cmdline namespace
  //
  //----------------------------------------------------------------------------

  rejectRemovedRexClangFrontendOptions(argv);
  rejectRemovedRoseLanguageSelectionOptions(argv);
  rejectRemovedRexLegacyOptions(argv);
  Rose::Cmdline::Unparser::StripRoseOptions(argv);
  CommandlineProcessing::removeArgsWithParameters(argv, "-outputdir");
  for (size_t i = 0; i < argv.size();) {
    const std::string &arg = argv[i];
    if (arg.rfind("-rex:ast-json-checkpoint=", 0) == 0 ||
        arg.rfind("-rex:ast-json-dir=", 0) == 0) {
      argv.erase(argv.begin() + i);
      continue;
    }
    ++i;
  }

  //----------------------------------------------------------------------------

  // Use 1 at end of argument list to SLA to force removal of option from argv
  // and decrement of agrc
  optionCount = sla(argv, "-", "($)", "(h|help)", 1);
  optionCount = sla(argv, "-rose:", "($)", "(h|help)", 1);
  optionCount = sla(argv, "-rose:", "($)", "(V|version)", 1);
  char *loggingSpec = NULL;
  optionCount = sla(argv, "-rose:", "($)^", "(log)", loggingSpec, 1);
  int integerOption = 0;
  optionCount = sla(argv, "-rose:", "($)^", "(v|verbose)", &integerOption, 1);
  optionCount = sla(argv, "-rose:", "($)", "(OpenACC|openacc)", 1);
  optionCount =
      sla(argv, "-rose:", "($)", "(openacc:parse_only|OpenACC:parse_only)", 1);
  optionCount =
      sla(argv, "-rose:", "($)", "(openacc:ast_only|OpenACC:ast_only)", 1);
  // support --rose:openacc variants
  optionCount =
      sla(argv, "--rose:", "($)", "(openacc:parse_only|OpenACC:parse_only)", 1);
  optionCount =
      sla(argv, "--rose:", "($)", "(openacc:ast_only|OpenACC:ast_only)", 1);
  optionCount = sla(argv, "-rose:", "($)", "(OpenMP|openmp)", 1);
  optionCount =
      sla(argv, "-rose:", "($)", "(openmp:parse_only|OpenMP:parse_only)", 1);
  optionCount =
      sla(argv, "-rose:", "($)", "(openmp:ast_only|OpenMP:ast_only)", 1);
  optionCount =
      sla(argv, "-rose:", "($)", "(openmp:lowering|OpenMP:lowering)", 1);
  optionCount =
      sla(argv, "-rose:", "($)", "(openmp:analyzing|OpenMP:analyzing)", 1);
  // support --rose:openmp variants
  optionCount =
      sla(argv, "--rose:", "($)", "(openmp:parse_only|OpenMP:parse_only)", 1);
  optionCount =
      sla(argv, "--rose:", "($)", "(openmp:ast_only|OpenMP:ast_only)", 1);
  optionCount =
      sla(argv, "--rose:", "($)", "(openmp:lowering|OpenMP:lowering)", 1);
  optionCount =
      sla(argv, "--rose:", "($)", "(openmp:analyzing|OpenMP:analyzing)", 1);

  optionCount = sla(argv, "-rose:", "($)", "(cray_pointer_support)", 1);

  optionCount = sla(argv, "-rose:", "($)", "(unparse_tokens)", 1);
  optionCount = sla(argv, "-rose:simd:", "($)", "(intel-avx)", 1);
  optionCount = sla(argv, "-rose:simd:", "($)", "(arm-sve)", 1);
  optionCount = sla(argv, "-rose:simd:", "($)", "(addr3)", 1);

  // DQ (9/7/2016): remove this from the backend compiler command line (adding
  // more support for it's use). optionCount = sla(argv, "-rose:", "($)",
  // "(unparse_headers)",1);

  optionCount = sla(argv, "-rose:", "($)", "(unparse_template_ast)", 1);
  optionCount = sla(argv, "-rose:", "($)", "(exit_after_parser)", 1);
  // DQ (8/11/2007): Support for Fortran and its different flavors
  optionCount = sla(argv, "-rose:", "($)", "(fortran)", 1);
  optionCount = sla(argv, "-rose:", "($)", "(CoArrayFortran)", 1);

  // DQ (8/27/2007):Support for Fortran language output format

  optionCount =
      sla(argv, "-rose:", "($)", "(fixedOutput|fixedFormatOutput)", 1);
  optionCount = sla(argv, "-rose:", "($)", "(freeOutput|freeFormatOutput)", 1);

  optionCount =
      sla(argv, "-rose:", "($)", "(compileFixed|backendCompileFixedFormat)", 1);
  optionCount =
      sla(argv, "-rose:", "($)", "(compileFree|backendCompileFreeFormat)", 1);

  optionCount =
      sla(argv, "-rose:", "($)",
          "(C_output_language|Cxx_output_language|Fortran_output_language)", 1);

  // DQ (5/19/2005): The output file name is constructed from the input source
  // name (as I recall) optionCount = sla(argv, "-rose:", "($)^", "(o|output)",
  // &p_unparse_output_filename ,1);

  optionCount = sla(argv, "-rose:", "($)", "(skip_transformation)", 1);
  optionCount = sla(argv, "-rose:", "($)", "(skip_unparse)", 1);
  optionCount = sla(argv, "-rose:", "($)", "(skip_parser)", 1);
  optionCount = sla(argv, "-rose:", "($)", "(skip_rose)", 1);
  optionCount = sla(argv, "--rose:", "($)", "(skip_rose)", 1);
  optionCount = sla(argv, "-rose:", "($)", "(unparse_includes)", 1);
  optionCount = sla(argv, "-rose:", "($)", "(unparse_line_directives)", 1);

  optionCount =
      sla(argv, "-rose:", "($)", "(collectAllCommentsAndDirectives)", 1);
  optionCount = sla(argv, "-rose:", "($)", "(unparseHeaderFiles)", 1);
  optionCount = sla(argv, "-rose:", "($)", "(skip_commentsAndDirectives)", 1);
  optionCount = sla(argv, "-rose:", "($)", "(skipfinalCompileStep)", 1);
  optionCount = sla(argv, "-", "($)", "(ansi)", 1);
  optionCount = sla(argv, "-rose:", "($)", "(markGeneratedFiles)", 1);
  optionCount = sla(argv, "-rose:", "($)", "(negative_test)", 1);
  optionCount = sla(argv, "-rose:", "($)", "(showBackendCommandLine)", 1);
  integerOption = 0;

  optionCount = sla(argv, "-rose:", "($)", "(outputGrammarTreeFiles)", 1);
  optionCount =
      sla(argv, "-rose:", "($)", "(outputGrammarTreeFilesForHeaderFiles)", 1);

  optionCount = sla(argv, "-rose:", "($)", "(new_unparser)", 1);
  optionCount = sla(argv, "-rose:", "($)", "(negative_test)", 1);
  optionCount = sla(argv, "-rose:", "($)", "(strict)", 1);
  integerOption = 0;
  optionCount = sla(argv, "-rose:", "($)^", "test", &integerOption, 1);
  optionCount = sla(argv, "-rose:", "($)", "(skipfinalCompileStep)", 1);

  char *unparseHeaderFilesRootFolderOption = NULL;
  optionCount = sla(argv, "-rose:", "($)^", "(unparseHeaderFilesRootFolder)",
                    unparseHeaderFilesRootFolderOption, 1);

  // DQ (11/6/2018): Added to support specification of application root
  // directory to support source file and header file unparsing.
  char *applicationRootDirectoryOption = NULL;
  optionCount = sla(argv, "-rose:", "($)^", "(applicationRootDirectory)",
                    applicationRootDirectoryOption, 1);

  char *filename = NULL;

  // DQ (10/28/2020): Added to support output of compile-time performance data.
  optionCount = sla(argv, "-rose:", "($)^", "(compilationPerformance)", 1);

  optionCount =
      sla(argv, "-rose:", "($)^", "(compilationPerformanceFile)", filename, 1);

  // AS(093007) Remove paramaters relating to excluding and include comments and
  // directives
  optionCount = sla(argv, "-rose:", "($)^", "(excludeCommentsAndDirectives)",
                    &integerOption, 1);
  optionCount = sla(argv, "-rose:", "($)^",
                    "(excludeCommentsAndDirectivesFrom)", &integerOption, 1);
  optionCount = sla(argv, "-rose:", "($)^", "(includeCommentsAndDirectives)",
                    &integerOption, 1);
  optionCount = sla(argv, "-rose:", "($)^",
                    "(includeCommentsAndDirectivesFrom)", &integerOption, 1);

  // DQ (12/8/2007): Strip use of the "-rose:output <filename> option.
  optionCount = sla(argv, "-rose:", "($)^", "(o|output)", filename, 1);

  // Liao 2/26/2009: strip use of -rose:excludePath <pathname> ,
  // -rose:excludeFile <filename> ,etc which are introduced in
  // Compass::commandLineProcessing()
  char *pathname = NULL;
  optionCount = sla(argv, "-rose:", "($)^", "(excludePath)", pathname, 1);
  optionCount = sla(argv, "-rose:", "($)^", "(excludeFile)", filename, 1);
  optionCount = sla(argv, "-rose:", "($)^", "(includeFile)", filename, 1);

  // DQ (8/26/2007): Disassembly support from segments (true) instead of
  // sections (false, default).
  optionCount = sla(argv, "-rose:", "($)", "(aggressive)", 1);

  // DQ (10/3/2010): Adding support for having CPP directives explicitly in the
  // AST (as IR nodes instead of handled similar to comments).
  optionCount =
      sla(argv, "-rose:", "($)^", "(addCppDirectivesToAST)", filename, 1);

  // DQ (9/26/2011): Added support for different levesl of detection for
  // dangling pointers.
  optionCount = sla(argv, "-rose:", "($)^", "detect_dangling_pointers",
                    &integerOption, 1);

  // DQ (1/16/2012): Added all of the currently defined dot file options.
  optionCount = sla(argv, "-rose:dotgraph:", "($)^",
                    "(commentAndDirectiveFilter)", &integerOption, 1);
  optionCount = sla(argv, "-rose:dotgraph:", "($)^",
                    "(ctorInitializerListFilter)", &integerOption, 1);
  optionCount = sla(argv, "-rose:dotgraph:", "($)^", "(defaultColorFilter)",
                    &integerOption, 1);
  optionCount = sla(argv, "-rose:dotgraph:", "($)^", "(defaultFilter)",
                    &integerOption, 1);
  optionCount =
      sla(argv, "-rose:dotgraph:", "($)^", "(edgeFilter)", &integerOption, 1);
  optionCount = sla(argv, "-rose:dotgraph:", "($)^", "(emptySymbolTableFilter)",
                    &integerOption, 1);

  // DQ (7/22/2012): Added support to igmore some specific empty IR nodes.
  optionCount = sla(argv, "-rose:dotgraph:", "($)^", "(emptyBasicBlockFilter)",
                    &integerOption, 1);
  optionCount = sla(argv, "-rose:dotgraph:", "($)^",
                    "(emptyFunctionParameterListFilter)", &integerOption, 1);

  optionCount = sla(argv, "-rose:dotgraph:", "($)^", "(expressionFilter)",
                    &integerOption, 1);
  optionCount = sla(argv, "-rose:dotgraph:", "($)^", "(fileInfoFilter)",
                    &integerOption, 1);
  optionCount = sla(argv, "-rose:dotgraph:", "($)^",
                    "(frontendCompatibilityFilter)", &integerOption, 1);
  optionCount =
      sla(argv, "-rose:dotgraph:", "($)^", "(symbolFilter)", &integerOption, 1);
  optionCount =
      sla(argv, "-rose:dotgraph:", "($)^", "(typeFilter)", &integerOption, 1);
  optionCount = sla(argv, "-rose:dotgraph:", "($)^",
                    "(variableDeclarationFilter)", &integerOption, 1);
  optionCount =
      sla(argv, "-rose:dotgraph:", "($)^", "(noFilter)", &integerOption, 1);

  // DQ (2/17/2013): Added support for skipping AST consistancy testing (for
  // performance evaluation).
  optionCount = sla(argv, "-rose:", "($)", "(skipAstConsistancyTests)", 1);

  char *fortranStdOption = NULL;
  optionCount =
      sla(argv, "-rose:", "($)^", "(fortran_std)", fortranStdOption, 1);
  for (size_t i = 0; i < argv.size();) {
    const std::string &arg = argv[i];
    if (arg.rfind("-rose:fortran_std=", 0) == 0 ||
        arg.rfind("--rose:fortran_std=", 0) == 0) {
      argv.erase(argv.begin() + i);
      continue;
    }
    ++i;
  }

  // Generic backend command stripping can also receive the exact supported
  // project option outside an SgFile transaction. Its arity is zero and a
  // duplicate remains malformed.
  const std::string sameDirectoryOption =
      "-rose:unparse_in_same_directory_as_input_file";
  const auto sameDirectoryArgument =
      std::find(argv.begin(), argv.end(), sameDirectoryOption);
  if (sameDirectoryArgument != argv.end()) {
    if (std::find(std::next(sameDirectoryArgument), argv.end(),
                  sameDirectoryOption) != argv.end()) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[same-directory-option]: option '%s' "
              "appears more than once in one translation-unit command\n",
              sameDirectoryOption.c_str());
      ROSE_ABORT();
    }
    argv.erase(sameDirectoryArgument);
  }

  // DQ (1/26/2014): Remove this from being output to the backend compiler.
  // This also likely means that we are not passing it on to the backend
  // (linker). At the moment, this fixes a problem where the version number is
  // being treated as a file and causing ROSE to crash in the command line
  // handling.
  char *version_string = NULL;
  optionCount = sla(argv, "-", "($)^", "(version-info)", version_string, 1);

  // DQ (8/18/2025): Added option to support better performance of handling
  // large source code (name qualification on only the source file, not on the
  // whole translation unit).
  optionCount = sla(argv, "-rose:", "($)",
                    "(suppressNameQualificationAcrossWholeTranslationUnit)", 1);

  // DQ (3/19/2014): This option causes the output of source code to an existing
  // file to be an error.
  optionCount = sla(argv, "-rose:", "($)", "noclobber_output_file", 1);

  // DQ (3/19/2014): This option causes the output of source code to an existing
  // file to be an error if it results in a different file.
  optionCount =
      sla(argv, "-rose:", "($)", "noclobber_if_different_output_file", 1);

  // Pei-Hung (8/6/2014): This option appends PID into the output name to avoid
  // file collision in parallel compilation.
  optionCount = sla(argv, "-rose:", "($)", "appendPID", 1);

  // DQ (9/20/2018): Removing option to specify support for header file
  // unparsing report.
  optionCount = sla(argv, "-rose:", "($)", "(headerFileUnparsingReport)", 1);

  // DQ (11/27/2020): Generates GraphViz from Clang internal representation
  optionCount = sla(argv, "-rose:", "($)", "clang_il_to_graphviz", 1);

  // DQ (2/17/2024): Need to support stripping out c++03 type options, so that
  // we can support alternative languages versions. Adding the complete list of
  // C and C++ language versions. We need to remove them here and then add them
  // when we build the backend compiler command line, so that we can have the
  // added language version be generated. DQ (2/18/2024): Full list taken from
  // running "man g++" (for gnu g++ version 10.3), plus added entries for c++20,
  // c++23, and c++26. TV (11/20/2018): ROSE-1529: removed non-standard standard
  // selection Rasmussen (11/17/2018): ROSE-1584: separated "++" into single
  // characters [+][+] for regex handling.
  optionCount =
      sla(argv, "-std=", "($)",
          "(c89|c90|c99|c9x|c11|c1x|c17|c23|c2y|gnu89|gnu90|gnu9x|gnu11|"
          "gnu1x|gnu17|gnu23|gnu2y|c[+][+]98|c[+][+"
          "]03|c[+][+]11|c[+][+]0x|gnu[+][+]11|gnu[+][+]0x|c[+][+]14|c[+][+]1y|"
          "gnu[+][+]14|gnu[+][+]1y|c[+][+]17|c[+][+]1z|gnu[+][+]17|gnu[+][+]1z|"
          "c[+][+]20|c[+][+]2a|gnu[+][+]20|gnu[+][+]2a|c[+][+]23|c[+][+]2b|gnu["
          "+][+]23|gnu[+][+]2b|c[+][+]26|c[+][+]2c|gnu[+][+]26|gnu[+][+]2c)",
          1);

  // DQ (12/9/2016): Eliminating a warning that we want to be an error:
  // -Werror=unused-but-set-variable.
  ROSE_ASSERT(optionCount >= 0);

  // DQ (10/26/2019): Remove outliner options.
  optionCount = sla(argv, "-rose:outline:", "($)", "use_dlopen", 1);
  optionCount = sla(argv, "-rose:outline:", "($)", "copy_orig_file", 1);
  optionCount = sla(argv, "-rose:outline:", "($)", "temp_variable", 1);
  optionCount = sla(argv, "-rose:outline:", "($)", "exclude_headers", 1);
  optionCount =
      sla(argv, "-rose:outline:", "($)^", "(output_path)", filename, 1);

  if ((ROSE_DEBUG >= 1) || (SgProject::get_verbose() > 2)) {
    printf("In stripRoseCommandLineOptions (BOTTOM): List ALL arguments: argc "
           "= %" PRIuPTR " \n",
           argv.size());
    for (size_t i = 0; i < argv.size(); i++)
      printf("     argv[%" PRIuPTR "] = %s \n", i, argv[i].c_str());
  }
}

void SgFile::validateFortranCommandLineOptions(const vector<string> &argv) {
  // Validate Fortran-specific options that the backend compiler must accept.
  for (const auto &arg : argv) {
    if (arg == "-fcray-pointer") {
      std::cerr << "REX_BACKEND_INVARIANT[removed-cray-pointer-option]: "
                   "-fcray-pointer must not reach the Flang backend; use "
                   "-rose:cray_pointer_support explicitly\n";
      ROSE_ABORT();
    }
    if (arg == "-fno-cray-pointer") {
      std::cerr << "[FATAL] [ROSE] [backend] [Fortran] "
                   "error: -fno-cray-pointer is not supported with Flang. "
                   "Remove the flag or use standard Fortran pointers.\n";
      ROSE_ABORT();
    }
    if (arg.rfind("-std=", 0) == 0 && arg != "-std=f2018") {
      std::cerr << "[FATAL] [ROSE] [backend] [Fortran] "
                   "error: Unsupported Fortran standard flag '"
                << arg
                << "'. Flang accepts only -std=f2018. Use "
                   "-rose:fortran_std=<f77|f90|f95|f2003|f2008|f2018> "
                   "for ROSE dialect selection.\n";
      ROSE_ABORT();
    } else if (arg == "-std") {
      std::cerr << "[FATAL] [ROSE] [backend] [Fortran] "
                   "error: -std requires an '=f2018' value for Flang. "
                   "Use -std=f2018 or -rose:fortran_std.\n";
      ROSE_ABORT();
    } else if (arg == "-ffixed-line-length-none" ||
               arg == "-ffree-line-length-none") {
      std::cerr << "[FATAL] [ROSE] [backend] [Fortran] "
                   "error: Unsupported line-length flag '"
                << arg
                << "'. Use -ffixed-line-length=0 for fixed form, or omit "
                   "the flag for free form.\n";
      ROSE_ABORT();
    } else if (arg == "-fixed" || arg == "-free") {
      std::cerr << "[FATAL] [ROSE] [backend] [Fortran] "
                   "error: Unsupported format flag '"
                << arg << "'. Use -ffixed-form or -ffree-form.\n";
      ROSE_ABORT();
    }
  }
}

static void stripRexFrontendCommandLineOptions(vector<string> &argv) {
  // The backend stripper also removes compiler-owned language controls such
  // as -std= and -ansi so they can be reconstructed from SgFile state.  A
  // Clang frontend command must instead retain the user's exact ordered driver
  // stream.  Remove only options owned by REX at this boundary.
  rejectRemovedRexClangFrontendOptions(argv);
  rejectRemovedRoseLanguageSelectionOptions(argv);
  Rose::Cmdline::Unparser::StripRoseOptions(argv);
  CommandlineProcessing::removeArgsWithParameters(argv, "-outputdir");
  for (size_t i = 0; i < argv.size();) {
    const std::string &arg = argv[i];
    if (arg.rfind("-rex:ast-json-checkpoint=", 0) == 0 ||
        arg.rfind("-rex:ast-json-dir=", 0) == 0) {
      argv.erase(argv.begin() + i);
      continue;
    }
    ++i;
  }
}

void SgFile::processBackendSpecificCommandLineOptions(
    const vector<string> &argvOrig) {
  if (std::find(argvOrig.begin(), argvOrig.end(), "-ansi") != argvOrig.end()) {
    set_strict_language_handling(true);
  }
}

void SgFile::build_CLANG_CommandLine(vector<string> &inputCommandLine,
                                     vector<string> &argv,
                                     int /*fileNameIndex*/) {
  // It filters ROSE-specific parameters and fixes the paths.

  if (argv.empty() || argv.front().empty()) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[clang-command]: command has no driver "
            "executable\n");
    ROSE_ABORT();
  }

  // This is the single ownership boundary between REX's driver options and
  // Clang's driver options.  In particular, REX's historical `-outputdir`
  // spelling is parsed by Clang as the joined `-o` option followed by a second
  // positional input.  Preserve the original command on the SgFile, while
  // removing only REX-owned options from the per-translation-unit frontend
  // command.  Compiler-owned options remain in their exact original order.
  vector<string> frontendArgv = argv;
  stripRexFrontendCommandLineOptions(frontendArgv);
  if (frontendArgv.empty() || frontendArgv.front() != argv.front()) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[clang-command]: REX option stripping "
            "changed or removed the driver executable\n");
    ROSE_ABORT();
  }

  auto fail_missing_operand = [](const std::string &option) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[clang-option-operand]: option '%s' "
            "requires an argument\n",
            option.c_str());
    ROSE_ABORT();
  };
  const std::string expectedSource = get_sourceFileNameWithPath();
  if (expectedSource.empty()) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[clang-source-identity]: Sage file has no "
            "owned source path\n");
    ROSE_ABORT();
  }
  const std::string resolvedExpectedSource =
      StringUtility::getAbsolutePathFromRelativePath(expectedSource);
  std::string sourceOperand;
  bool afterDoubleDash = false;
  inputCommandLine.clear();

  for (size_t i = 1; i < frontendArgv.size(); i++) {
    const std::string &currentArg = frontendArgv[i];
    if (!afterDoubleDash && currentArg == "--") {
      afterDoubleDash = true;
      inputCommandLine.push_back(currentArg);
      continue;
    }

    if (!afterDoubleDash && currentArg == "-std") {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[clang-standard-option]: Clang "
              "standards must use the exact joined -std=<dialect> form\n");
      ROSE_ABORT();
    }
    if (!afterDoubleDash && isRemovedRexClangFrontendOption(currentArg)) {
      rejectRemovedRexClangFrontendOption(currentArg);
    }
    if (!afterDoubleDash && (currentArg.rfind("-rose:", 0) == 0 ||
                             currentArg.rfind("--rose:", 0) == 0)) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[unknown-rose-option]: unconsumed option "
              "'%s' reached Clang command construction\n",
              currentArg.c_str());
      ROSE_ABORT();
    }

    if (!afterDoubleDash &&
        CommandlineProcessing::isOptionTakingSecondParameter(currentArg)) {
      inputCommandLine.push_back(currentArg);
      const size_t operandCount =
          CommandlineProcessing::isOptionTakingThirdParameter(currentArg) ? 2
                                                                          : 1;
      if (operandCount >= frontendArgv.size() - i) {
        fail_missing_operand(currentArg);
      }
      for (size_t operandIndex = 0; operandIndex < operandCount;
           ++operandIndex) {
        inputCommandLine.push_back(frontendArgv[++i]);
      }
      continue;
    }

    if (afterDoubleDash || currentArg.empty() || currentArg == "-" ||
        currentArg.front() != '-') {
      if (currentArg == "-") {
        rejectClangStdinInput();
      }
      if (sameCommandLineSource(currentArg, resolvedExpectedSource)) {
        if (!sourceOperand.empty()) {
          fprintf(stderr,
                  "REX_FRONTEND_INVARIANT[clang-source-count]: owned source "
                  "operand appears more than once ('%s', '%s')\n",
                  sourceOperand.c_str(), currentArg.c_str());
          ROSE_ABORT();
        }
        sourceOperand = currentArg;
      } else if (CommandlineProcessing::isSourceFilename(currentArg) ||
                 CommandlineProcessing::isAssemblerFileNameSuffix(
                     StringUtility::fileNameSuffix(currentArg))) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[clang-source-identity]: per-TU "
                "command contains foreign compilable source '%s', "
                "expected '%s'\n",
                currentArg.c_str(), resolvedExpectedSource.c_str());
        ROSE_ABORT();
      }
    }
    inputCommandLine.push_back(currentArg);
  }

  if (sourceOperand.empty()) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[clang-source-count]: command has no "
            "source operand\n");
    ROSE_ABORT();
  }
}

vector<string>
SgFile::buildCompilerCommandLineOptions(vector<string> &argv, int fileNameIndex,
                                        const string &compilerName) {
  // This function assembles the commandline that will be passed to the backend
  // (vendor) C/C++ or Fortran compiler (using the new generated source code
  // from the ROSE unparser).

  ROSE_ASSERT(fileNameIndex == 0);

  const SgSourceFile *sourceFile = isSgSourceFile(this);

  if (SgProject::get_verbose() > 0) {
    printf("In buildCompilerCommandLineOptions(): compilerName = %s \n",
           compilerName.c_str());
  }

  const std::vector<std::string> originalCommandLineArgs =
      get_originalCommandLineArgumentList();
  SgProject *owningProject = SageInterface::getProject(this);
  ROSE_ASSERT(owningProject != NULL);
  auto requireExactOwnedSource = [&](const std::vector<std::string> &command,
                                     const char *commandKind) -> std::string {
    if (command.empty()) {
      fprintf(stderr,
              "REX_BACKEND_INVARIANT[empty-owned-command]: file=%s "
              "command-kind=%s\n",
              getFileName().c_str(), commandKind);
      ROSE_ABORT();
    }

    const Rose_STL_Container<string> sourceInputs =
        CommandlineProcessing::generateSourceFilenames(command);
    if (sourceInputs.size() != 1) {
      fprintf(
          stderr,
          "REX_BACKEND_INVARIANT[owned-command-source-count]: file=%s "
          "command-kind=%s count=%zu inputs=%s command=%s\n",
          getFileName().c_str(), commandKind, sourceInputs.size(),
          StringUtility::listToString(sourceInputs).c_str(),
          CommandlineProcessing::generateStringFromArgList(command).c_str());
      ROSE_ABORT();
    }

    const std::string sourceInput = sourceInputs.front();
    const std::string absoluteSourceInput =
        StringUtility::getAbsolutePathFromRelativePath(sourceInput);
    if (absoluteSourceInput != get_sourceFileNameWithPath()) {
      fprintf(stderr,
              "REX_BACKEND_INVARIANT[owned-command-source-mismatch]: "
              "file=%s command-kind=%s input=%s resolved-input=%s\n",
              getFileName().c_str(), commandKind, sourceInput.c_str(),
              absoluteSourceInput.c_str());
      ROSE_ABORT();
    }
    return sourceInput;
  };
  const std::string ownedSourceInput =
      requireExactOwnedSource(originalCommandLineArgs, "original");
  std::vector<std::string> backendArgv = argv;
  SgFile::stripRoseCommandLineOptions(backendArgv);
  if (get_Fortran_only() == true) {
    SgFile::validateFortranCommandLineOptions(backendArgv);
  }
  const std::string backendSourceInput =
      requireExactOwnedSource(backendArgv, "backend");
  if (backendSourceInput != ownedSourceInput) {
    fprintf(stderr,
            "REX_BACKEND_INVARIANT[owned-command-source-spelling]: file=%s "
            "original-input=%s backend-input=%s\n",
            getFileName().c_str(), ownedSourceInput.c_str(),
            backendSourceInput.c_str());
    ROSE_ABORT();
  }

  vector<string> compilerNameString;
  if (get_inputLanguage() != get_outputLanguage()) {
    printf("Error: In buildCompilerCommandLineOptions(): get_inputLanguage() = "
           "%d get_outputLanguage() = %d \n",
           get_inputLanguage(), get_outputLanguage());
    printf("Error: In buildCompilerCommandLineOptions(): get_inputLanguage() = "
           "%s get_outputLanguage() = %s \n",
           get_outputLanguageOptionName(get_inputLanguage()).c_str(),
           get_outputLanguageOptionName(get_outputLanguage()).c_str());
  }
  ROSE_ASSERT(get_inputLanguage() == get_outputLanguage());

  switch (get_outputLanguage()) {
  case SgFile::e_error_language: {
    printf("Error: SgFile::e_error_language detected in "
           "SgFile::buildCompilerCommandLineOptions() \n");
    ROSE_ABORT();
  }

  case SgFile::e_default_language: {
    fprintf(stderr, "REX_BACKEND_INVARIANT[output-language]: backend command "
                    "construction requires an exact output language\n");
    ROSE_ABORT();
  }

  case SgFile::e_C_language: {
    switch (get_standard()) {
    case e_cxx98_standard:
    case e_cxx03_standard:
    case e_cxx11_standard:
    case e_cxx14_standard:
    case e_cxx17_standard:
    case e_cxx20_standard:
    case e_cxx23_standard:
    case e_cxx26_standard: {
      compilerNameString.assign(RexBackend::cxxCompilerCommand.begin(),
                                RexBackend::cxxCompilerCommand.end());
      break;
    }

    case e_c89_standard:
    case e_c90_standard:
    case e_c99_standard:
    case e_c11_standard:
    case e_c17_standard:
    case e_c23_standard:
    case e_c2y_standard:
    case e_default_standard: {
      compilerNameString.assign(RexBackend::cCompilerCommand.begin(),
                                RexBackend::cCompilerCommand.end());
      break;
    }

    default: {
      printf("Unhandled case in switch: get_standard() = %d = %s \n",
             get_standard(), display_standard(get_standard()).c_str());
      ROSE_ABORT();
    }
    }

    if (get_Cuda_only() || get_OpenCL_only()) {
      std::cerr << "[WARN] No backend compiler for CUDA and OpenCL."
                << std::endl;
    }

    break;
  }

  case SgFile::e_Cxx_language: {
    compilerNameString.assign(RexBackend::cxxCompilerCommand.begin(),
                              RexBackend::cxxCompilerCommand.end());

    break;
  }

  case SgFile::e_Fortran_language: {
    compilerNameString.assign(RexBackend::fortranCompilerCommand.begin(),
                              RexBackend::fortranCompilerCommand.end());

    if (get_backendCompileFormat() == e_fixed_form_output_format) {
      // The fixed-form unparser emits source against the standard 72-column
      // lexical boundary.  The backend must parse the same boundary: Flang's
      // unlimited mode changes the meaning of a continued character literal
      // when the first physical record ends at column 72.
      compilerNameString.push_back("-ffixed-form");
      compilerNameString.push_back("-ffixed-line-length=72");
    } else if (get_backendCompileFormat() == e_free_form_output_format) {
      // Explicit free-form compilation.
      compilerNameString.push_back("-ffree-form");
    } else {
      // Rely on the output filename suffix and compiler defaults.
    }
    break;
  }

  case SgFile::e_last_language: {
    printf("Error: SgFile::e_last_language detected in "
           "SgFile::buildCompilerCommandLineOptions() \n");
    ROSE_ABORT();
  }

  default: {
    printf(
        "Error: default reached in SgFile::buildCompilerCommandLineOptions() "
        "(unknown output language specified) \n");

    std::cerr << "[FATAL] Unknown backend compiler '" << compilerName
              << "', or not implemented." << std::endl;

    ROSE_ASSERT(!"Unknown backend compiler");
    break;
  }
  }

  ROSE_ASSERT(!compilerNameString.empty() && !compilerNameString.front().empty());
  if (get_C_only() && CommandlineProcessing::isCppFileNameSuffix(
                          StringUtility::fileNameSuffix(getFileName()))) {
    compilerNameString.push_back("-x");
    compilerNameString.push_back("c");
  }
  switch (get_standard()) {
  case e_default_standard: {
    break; // NOP
  }
  case e_c89_standard: {
    if (is_gnu_standard()) {
      compilerNameString.push_back("-std=gnu89");
    } else {
      compilerNameString.push_back("-std=c89");
    }
    break;
  }
  case e_c90_standard: {
    if (is_gnu_standard()) {
      compilerNameString.push_back("-std=gnu90");
    } else {
      compilerNameString.push_back("-std=c90");
    }
    break;
  }
  case e_c99_standard: {
    if (is_gnu_standard()) {
      compilerNameString.push_back("-std=gnu99");
    } else {
      compilerNameString.push_back("-std=c99");
    }
    break;
  }
  case e_c11_standard: {
    if (is_gnu_standard()) {
      compilerNameString.push_back("-std=gnu11");
    } else {
      compilerNameString.push_back("-std=c11");
    }
    break;
  }
  case e_c17_standard: {
    if (is_gnu_standard()) {
      compilerNameString.push_back("-std=gnu17");
    } else {
      compilerNameString.push_back("-std=c17");
    }
    break;
  }

    // PL (4/9/2025): Added support for C23 (newest C language standard).
  case e_c23_standard: {
    if (is_gnu_standard()) {
      compilerNameString.push_back("-std=gnu2x");
    } else {
      compilerNameString.push_back("-std=c2x");
    }
    break;
  }
  case e_c2y_standard: {
    if (is_gnu_standard()) {
      compilerNameString.push_back("-std=gnu2y");
    } else {
      compilerNameString.push_back("-std=c2y");
    }
    break;
  }
  case e_cxx98_standard: {
    if (is_gnu_standard()) {
      compilerNameString.push_back("-std=gnu++98");
    } else {
      compilerNameString.push_back("-std=c++98");
    }
    break;
  }
  case e_cxx03_standard: {
    if (is_gnu_standard()) {
      compilerNameString.push_back("-std=gnu++03");
    } else {
      compilerNameString.push_back("-std=c++03");
    }
    break;
  }
  case e_cxx11_standard: {
    if (is_gnu_standard()) {
      compilerNameString.push_back("-std=gnu++11");
    } else {
      compilerNameString.push_back("-std=c++11");
    }
    break;
  }
  case e_cxx14_standard: {
    if (is_gnu_standard()) {
      compilerNameString.push_back("-std=gnu++14");
    } else {
      compilerNameString.push_back("-std=c++14");
    }
    break;
  }
  case e_cxx17_standard: {
    if (is_gnu_standard()) {
      compilerNameString.push_back("-std=gnu++17");
    } else {
      compilerNameString.push_back("-std=c++17");
    }
    break;
  }
  case e_cxx20_standard: {
    if (is_gnu_standard()) {
      compilerNameString.push_back("-std=gnu++20");
    } else {
      compilerNameString.push_back("-std=c++20");
    }

#if defined(BACKEND_CXX_IS_GNU_COMPILER)
    // DQ (7/26/2020): the GNU 10.2 C++20 "coroutine" header file requires GNU
    // to be used with -fcoroutines using the macro "__cpp_impl_coroutine" to
    // control internal access.
    compilerNameString.push_back("-fcoroutines");

#endif
    break;
  }
  case e_cxx23_standard: {
    if (is_gnu_standard()) {
      compilerNameString.push_back("-std=gnu++23");
    } else {
      compilerNameString.push_back("-std=c++23");
    }

#if defined(BACKEND_CXX_IS_GNU_COMPILER)
    // DQ (7/26/2020): the GNU 10.2 C++20 "coroutine" header file requires GNU
    // to be used with -fcoroutines using the macro "__cpp_impl_coroutine" to
    // control internal access.
    compilerNameString.push_back("-fcoroutines");

#endif
    break;
  }
  case e_cxx26_standard: {
    if (is_gnu_standard()) {
      compilerNameString.push_back("-std=gnu++2c");
    } else {
      compilerNameString.push_back("-std=c++2c");
    }

#if defined(BACKEND_CXX_IS_GNU_COMPILER)
    // DQ (7/26/2020): the GNU 10.2 C++20 "coroutine" header file requires GNU
    // to be used with -fcoroutines using the macro "__cpp_impl_coroutine" to
    // control internal access.
    compilerNameString.push_back("-fcoroutines");

#endif
    break;
  }
  case e_f77_standard:
  case e_f90_standard:
  case e_f95_standard:
  case e_f03_standard:
  case e_f08_standard:
  case e_f18_standard: {
    break;
  }
  default: {
    printf("Unhandled case in switch: get_standard() = %d = %s \n",
           get_standard(), display_standard(get_standard()).c_str());
    ROSE_ABORT();
  }
  }

#if defined(BACKEND_CXX_IS_INTEL_COMPILER)
  // DQ (3/16/2017): Only turn this on for C and C++ modes (not for Fortran (or
  // anything else). DQ (3/15/2017): The intel compiler requires the use of the
  // "-restrict" option to support the "restrict" keyword.
  if (get_C_only() == true || get_Cxx_only() == true) {
    compilerNameString.push_back("-restrict");
  } else {
    // DQ (3/17/2017): It was a problem that C++11 was turned on for Fortran
    // when using the Intel and Clang compilers (this code checks this).
    ROSE_ASSERT(get_C11_only() == false);
    ROSE_ASSERT(get_C17_only() == false);
    ROSE_ASSERT(get_C23_only() == false);
    ROSE_ASSERT(get_C2y_only() == false);
    ROSE_ASSERT(get_Cxx11_only() == false);
    ROSE_ASSERT(get_Cxx14_only() == false);
    ROSE_ASSERT(get_Cxx17_only() == false);
    ROSE_ASSERT(get_Cxx20_only() == false);
    ROSE_ASSERT(get_Cxx23_only() == false);
    ROSE_ASSERT(get_Cxx26_only() == false);
  }
#endif

  // DQ (9/24/2006): Not clear if we want this, if we just skip stripping it out
  // then it will be passed to the backend directly! But we have to add it in
  // the case of "-rose:strict", so we have to add it uniformally and strip it
  // from the input.
  if (get_strict_language_handling() == true) {
    // Check if it is appears as "-ansi" on the original commandline
    if (CommandlineProcessing::isOption(backendArgv, "-", "ansi", false) ==
        true) {
      printf("Option -ansi detected on the original commandline \n");
    } else {
      // This is might be specific to GNU
      compilerNameString.push_back("-ansi");
    }
  }

  if (get_C_only() || get_Cxx_only() || get_Fortran_only()) {
    // specify compilation only option (new style command line processing)
    if (CommandlineProcessing::isOption(backendArgv, "-", "c", false) == true) {
      set_compileOnly(true);
    } else {
      // printf ("Option -c not found (compile AND link) ... \n");
      // compilerNameString += " -c ";
    }

    // Liao, 2/13/2009. I think we should pass this macro to the backend
    // compiler also User programs may have rose-specific tweaks to enable ROSE
    // translators to compile them Part of solution to bug 316 :
    // https://outreach.scidac.gov/tracker/
    compilerNameString.push_back("-DUSE_ROSE");

    // Add -D_OPENMP to backend compiler when OpenMP is enabled
    // This allows backend to process #ifdef _OPENMP guards in generated code
    // OMPVERSION follows ROSE_OPENMP_VERSION from CMake (OpenMP 6.0 => 202411).
    if (get_openmp_lowering() || (get_openmp() && !Outliner::select_omp_loop)) {
      string ompmacro =
          "-D_OPENMP=" + StringUtility::numberToString(OMPVERSION);
      compilerNameString.push_back(ompmacro);
    }

    if ((get_openmp() || get_openmp_lowering()) && !get_Fortran_only()) {
      const RosePathRoots roots = resolveRosePaths(nullptr);
      const string openmp_compat_include_dir =
          roots.compiler_header_root + "openmp-compat";
      const std::filesystem::path openmp_compat_header =
          std::filesystem::path(openmp_compat_include_dir) / "omp.h";
      if (!std::filesystem::exists(openmp_compat_header)) {
        fprintf(stderr,
                "REX_BACKEND_INVARIANT[openmp-compat-path]: missing REX "
                "OpenMP compatibility header: %s\n",
                openmp_compat_header.string().c_str());
        ROSE_ABORT();
      }
      bool has_openmp_compat_include = false;
      for (size_t idx = 0; idx < compilerNameString.size(); ++idx) {
        const string &arg = compilerNameString[idx];
        if ((arg == "-I" || arg == "-isystem") &&
            (idx + 1) < compilerNameString.size() &&
            compilerNameString[idx + 1] == openmp_compat_include_dir) {
          has_openmp_compat_include = true;
          break;
        }
        if (arg == "-I" + openmp_compat_include_dir ||
            arg == "-isystem" + openmp_compat_include_dir) {
          has_openmp_compat_include = true;
          break;
        }
      }
      if (!has_openmp_compat_include) {
        // Ensure backend compilations resolve <omp.h> to the same REX
        // compatibility wrapper used in the frontend.
        compilerNameString.push_back("-I");
        compilerNameString.push_back(openmp_compat_include_dir);
      }

      string llvm_openmp_include_dir;
#ifdef LLVM_OPENMP_INCLUDE_PATH
      llvm_openmp_include_dir = LLVM_OPENMP_INCLUDE_PATH;
#endif
      if (llvm_openmp_include_dir.empty()) {
        fprintf(stderr,
                "REX_BACKEND_INVARIANT[llvm-openmp-path]: LLVM OpenMP "
                "include directory is not configured\n");
        ROSE_ABORT();
      }
      const std::filesystem::path llvm_openmp_header =
          std::filesystem::path(llvm_openmp_include_dir) / "omp.h";
      if (!std::filesystem::exists(llvm_openmp_header)) {
        fprintf(stderr,
                "REX_BACKEND_INVARIANT[llvm-openmp-path]: missing LLVM "
                "OpenMP header: %s\n",
                llvm_openmp_header.string().c_str());
        ROSE_ABORT();
      }
      bool has_llvm_openmp_include = false;
      for (size_t idx = 0; idx < compilerNameString.size(); ++idx) {
        const string &arg = compilerNameString[idx];
        if (arg == "-isystem" && (idx + 1) < compilerNameString.size() &&
            compilerNameString[idx + 1] == llvm_openmp_include_dir) {
          has_llvm_openmp_include = true;
          break;
        }
        if (arg == "-I" + llvm_openmp_include_dir ||
            arg == "-isystem" + llvm_openmp_include_dir) {
          has_llvm_openmp_include = true;
          break;
        }
      }
      if (!has_llvm_openmp_include) {
        compilerNameString.push_back("-isystem");
        compilerNameString.push_back(llvm_openmp_include_dir);
      }
    }
  }

  Rose_STL_Container<string> argcArgvList = backendArgv;

  argcArgvList.erase(argcArgvList.begin());

  if (sourceFile != NULL) {
    // DQ (3/12/20202): the extraIncludeDirectorySpecifierList from the
    // SgProject is used to support extra directory paths required as part of
    // header file transformations that are projects wide instead of source file
    // specific.
    SgProject *project = SageInterface::getProject(sourceFile);
    ROSE_ASSERT(project != NULL);

    std::set<std::string> beforeListEncounters;
    for (size_t i = 0;
         i < project->get_extraIncludeDirectorySpecifierBeforeList().size();
         ++i) {
      if (!beforeListEncounters
               .insert(
                   project->get_extraIncludeDirectorySpecifierBeforeList()[i])
               .second) {
        // The string was already in beforeListEncounters
        project->get_extraIncludeDirectorySpecifierBeforeList().erase(
            project->get_extraIncludeDirectorySpecifierBeforeList().begin() +
            i);
        --i;
      }
    }

    std::set<std::string> afterListEncounters;
    for (size_t i = 0;
         i < project->get_extraIncludeDirectorySpecifierAfterList().size();
         ++i) {
      if (!afterListEncounters
               .insert(
                   project->get_extraIncludeDirectorySpecifierAfterList()[i])
               .second) {
        // The string was already in afterListEncounters
        project->get_extraIncludeDirectorySpecifierAfterList().erase(
            project->get_extraIncludeDirectorySpecifierAfterList().begin() + i);
        --i;
      }
    }

    // DQ (10/10/2020): Find the first include.
    string firstInclude;
    bool foundFirstInclude = false;
    std::vector<string>::iterator positionForIncludes = argcArgvList.begin();
    size_t i = 0;
    while (foundFirstInclude == false && i < argcArgvList.size()) {
      size_t length = argcArgvList[i].size();
      // look only for -I include directories (directories where
      // #include<filename> will be found)
      if ((foundFirstInclude == false) && (length > 2) &&
          (argcArgvList[i][0] == '-') && (argcArgvList[i][1] == 'I')) {
        firstInclude = argcArgvList[i];
        foundFirstInclude = true;
      }

      positionForIncludes++;
      i++;
    }

    // If the first include is the same as the first entry in the extra include
    // list then remove the first entry in the extra include list (it was added
    // previously).
    if (project->get_extraIncludeDirectorySpecifierBeforeList().size() > 0) {
      if (firstInclude ==
          project->get_extraIncludeDirectorySpecifierBeforeList()[0]) {
        project->get_extraIncludeDirectorySpecifierBeforeList().erase(
            project->get_extraIncludeDirectorySpecifierBeforeList().begin());
      }
    }

    // Header-unparse extra directories must participate in quote-include lookup
    // without changing normal angle-include precedence. Add -iquote entries for
    // the generated directories, then keep the existing -I entries available as
    // fallback include roots.
    Rose_STL_Container<string> expandedExtraIncludeDirectorySpecifierBeforeList;
    for (size_t includePathIndex = 0;
         includePathIndex <
         project->get_extraIncludeDirectorySpecifierBeforeList().size();
         ++includePathIndex) {
      const string &includePath =
          project->get_extraIncludeDirectorySpecifierBeforeList()
              [includePathIndex];
      if (includePath.rfind("-I", 0) == 0 && includePath.size() > 2) {
        expandedExtraIncludeDirectorySpecifierBeforeList.push_back(
            "-iquote" + includePath.substr(2));
      }
    }
    expandedExtraIncludeDirectorySpecifierBeforeList.insert(
        expandedExtraIncludeDirectorySpecifierBeforeList.end(),
        project->get_extraIncludeDirectorySpecifierBeforeList().begin(),
        project->get_extraIncludeDirectorySpecifierBeforeList().end());

    argcArgvList.insert(
        positionForIncludes,
        expandedExtraIncludeDirectorySpecifierBeforeList.begin(),
        expandedExtraIncludeDirectorySpecifierBeforeList.end());

    positionForIncludes = argcArgvList.end();
    argcArgvList.insert(
        positionForIncludes,
        project->get_extraIncludeDirectorySpecifierAfterList().begin(),
        project->get_extraIncludeDirectorySpecifierAfterList().end());
  }

  if (get_skip_unparse() == false) {
    // Rebuild each backend invocation from the command line owned by this
    // translation unit.  The project's parsed-input list is deliberately not
    // authoritative here: transformations can add output-only source files
    // after the frontend has run.  Leaving such a file in argcArgvList and
    // then appending its unparse output below presents the same translation
    // unit to Clang twice and makes a per-file -o invalid.
    const size_t sourceInputCount = std::count(
        argcArgvList.begin(), argcArgvList.end(), backendSourceInput);
    if (sourceInputCount != 1) {
      fprintf(stderr,
              "REX_BACKEND_INVARIANT[backend-source-occurrence-count]: "
              "file=%s source=%s count=%zu command=%s\n",
              getFileName().c_str(), backendSourceInput.c_str(),
              sourceInputCount,
              CommandlineProcessing::generateStringFromArgList(argcArgvList)
                  .c_str());
      ROSE_ABORT();
    }
    argcArgvList.erase(std::find(argcArgvList.begin(), argcArgvList.end(),
                                 backendSourceInput));
  }

  if (!Rose::global_options.get_backend_warnings()) {
    argcArgvList.push_back("-w");
  }

  bool objectNameSpecified = false;
  Rose_STL_Container<string>::iterator outputOption = argcArgvList.end();
  for (Rose_STL_Container<string>::iterator i = argcArgvList.begin();
       i != argcArgvList.end(); i++) {
    if (*i == "-o") {
      // DQ (6/12/2005): Added error checking!
      if (objectNameSpecified == true) {
        // Error: "-o" has been specified twice
        printf("Error: In SgFile::buildCompilerCommandLineOptions: \"-o \" has "
               "been specified twice \n");
        ROSE_ABORT();
      } else {
        Rose_STL_Container<string>::iterator j = i;
        j++;
        ROSE_ASSERT(j != argcArgvList.end());
        // Preserve an output explicitly selected on the file; otherwise record
        // the command-line destination for subsequent command construction.
        if (get_objectFileNameWithPath().empty()) {
          set_objectFileNameWithPath(*j);
        } else if (*j != get_objectFileNameWithPath()) {
          *j = get_objectFileNameWithPath();
        }
      }

      ROSE_ASSERT(objectNameSpecified == false);
      objectNameSpecified = true;
      outputOption = i;
    }
  }

  if (!get_objectFileNameWithPath().empty() && !get_compileOnly() &&
      outputOption != argcArgvList.end()) {
    argcArgvList.erase(outputOption, outputOption + 2);
  }

  Rose_STL_Container<string> tempArgcArgv;
  for (Rose_STL_Container<string>::iterator i = argcArgvList.begin();
       i != argcArgvList.end(); i++) {
    // Liao, 11/19/2009
    // We now only handles compilation for SgFile::compileOutput(),
    // so we need to remove linking related flags such as '-lxx' from the
    // original command line Otherwise gcc will complain:  -lm: linker input
    // file unused because linking not done
    if (i->substr(0, 2) != "-l") {
      tempArgcArgv.push_back(*i);
    }
  }

  argcArgvList.swap(tempArgcArgv);


  // Arguments are already tokenized and passed directly to execvp. Literal
  // quotes and backslashes must reach the compiler unchanged.


  // Add any options specified by the user (and add space at the end)
  compilerNameString.insert(compilerNameString.end(), argcArgvList.begin(),
                            argcArgvList.end());


  std::string sourceFileName = get_sourceFileNameWithPath();

  std::string oldFileNamePathOnly =
      Rose::getPathFromFileName(sourceFileName.c_str());
  std::string oldFileName =
      Rose::utility_stripPathFromFileName(sourceFileName.c_str());

  // DQ (4/13/2014): Added support to avoid output of a specified include path
  // twice.
  bool oldFileNamePathOnlyAlreadySpecifiedAsIncludePath = false;
  for (vector<string>::iterator i = argcArgvList.begin();
       i != argcArgvList.end(); i++) {
    string s = std::string("-I") + oldFileNamePathOnly;
    if (s == *i) {
      oldFileNamePathOnlyAlreadySpecifiedAsIncludePath = true;
    }
  }

  if (get_C_only() || get_Cxx_only()) {
    // DQ (12/8/2004): Add -Ipath option so that source file's directory will be
    // searched for any possible headers.  This is especially important when we
    // are compiling the generated file located in a different directory!  (When
    // the original source file included header files in the source directory!)
    // This is only important when get_useBackendOnly() == false since otherwise
    // the source file is the original source file and the compiler will search
    // its directory for header files.  Be sure the put the oldFile's source
    // directory last in the list of -I options so that it will be searched last
    // (preserving the semantics of #include "..."). Only add the path if it is
    // a valid name (not an empty name, in which case skip it since the oldFile
    // is in the current directory (likely a generated file itself; e.g. swig or
    // ROSE applied recursively, etc.)).
    if (oldFileNamePathOnly.empty() == false &&
        oldFileNamePathOnlyAlreadySpecifiedAsIncludePath == false) {
      vector<string>::iterator iter;
      // find the very first -Ixxx option's position
      for (iter = compilerNameString.begin(); iter != compilerNameString.end();
           iter++) {
        string cur_string = *iter;
        string::size_type pos = cur_string.find("-I", 0);
        if (pos == 0) {
          break;
        }
      }
      // Liao, 5/15/2009
      // the input source file's path has to be the first one to be searched for
      // header! This is required since one of the SPEC CPU 2006 benchmarks:
      // gobmk relies on this to be compiled. Header-file unparsing handles this
      // case separately.
      if (this->get_unparseHeaderFiles() == false) {
        // DQ (11/7/2018): This might be the better way to get at the SgProject
        // IR node.
        ROSE_ASSERT(this->get_project() != NULL);

        // DQ (9/15/2013): Added support for generated file to be placed into
        // the same directory as the source file. When
        // (get_unparse_in_same_directory_as_input_file() == true) we don't want
        // to add the include path to the source directory.
        SgProject *project = SageInterface::getProject(this);
        if (project != NULL) {
          if (project->get_unparse_in_same_directory_as_input_file() == false) {
            compilerNameString.insert(iter,
                                      std::string("-I") + oldFileNamePathOnly);
          }
        } else {
          printf("ERROR: In SgFile::buildCompilerCommandLineOptions(): file = "
                 "%p has no associated project \n",
                 this);
          ROSE_ABORT();
        }
      }
    }
  }

  // Liao 3/30/2011. the search path for the installation path should be the
  // last one, after paths inside source trees, such as
  // -I../../../../sourcetree/src/frontend/SageIII and
  // -I../../../../sourcetree/src/midend/programTransformation/ompLowering
  if (get_openmp_lowering()) {
    vector<string>::iterator iter, iter_last_inc = compilerNameString.begin();
    // find the very last -Ixxx option's position
    // This for loop cannot be merged with the previous one due to iterator
    // invalidation rules.
    for (iter = compilerNameString.begin(); iter != compilerNameString.end();
         iter++) {
      string cur_string = *iter;
      string::size_type pos = cur_string.find("-I", 0);
      if (pos == 0) {
        iter_last_inc = iter;
      }
    }
    if (iter_last_inc != compilerNameString.end())
      iter_last_inc++; // accommodate the insert-before-an-iterator semantics
                       // used in vector::insert()

    // Keep REX OpenMP lowering headers reachable from the installation include
    // tree (LLVM runtime only path).

    static const RosePathRoots roots = resolveRosePaths(nullptr);
    if (roots.in_install_tree && !roots.rose_include_root.empty()) {
      compilerNameString.insert(iter_last_inc, "-I" + roots.rose_include_root);
    }
  }

  if (get_skip_unparse() == false) {
    // Generate the name of the ROSE generated source file (instead of the
    // original source file) this file will be compiled by the backend (vendor)
    // compiler.
    ROSE_ASSERT(get_unparse_output_filename().empty() == false);
    compilerNameString.push_back(get_unparse_output_filename());
  }

  if (get_compileOnly() == true) {
    // DQ (1/26/2022): Use the name from using set_objectFileNameWithoutPath(),
    // if it is a valid (non-empty) string.
    std::string objectFileName = get_objectFileNameWithoutPath();
    if (objectFileName.length() == 0) {
      objectFileName = generateOutputFileName();
      MLOG_TRACE_CXX("sage_support")
          << "get_objectFileNameWithoutPath() returned empty string; "
             "using generateOutputFileName(): objectFileName = "
          << objectFileName << std::endl;
    } else {
      MLOG_TRACE_CXX("sage_support")
          << "using value from get_objectFileNameWithoutPath(): objectFileName "
             "= "
          << objectFileName << std::endl;
    }
    if (get_C_only() || get_Cxx_only() || get_Fortran_only()) {
      // DQ (7/14/2004): Suggested fix from Andreas, make the object file name
      // explicit
      if (objectNameSpecified == false) {
        compilerNameString.push_back("-o");
        compilerNameString.push_back(
            Rose::TestOutput::resolvePath(objectFileName));

      } else {
        if (get_multifile_support() == true) {
          // For multi-file handling we have to build a output (object file)
          // using the name of the source file.
          compilerNameString.push_back("-c");
          std::string objectFileName = generateOutputFileName();

          auto originalOutputOption = std::find(compilerNameString.begin(),
                                                compilerNameString.end(), "-o");
          ROSE_ASSERT(originalOutputOption != compilerNameString.end());
          ROSE_ASSERT(originalOutputOption + 1 != compilerNameString.end());
          compilerNameString.erase(originalOutputOption,
                                   originalOutputOption + 2);
          compilerNameString.push_back("-o");
          compilerNameString.push_back(
              Rose::TestOutput::resolvePath(objectFileName));
        }
      }
    }
  } else {
    // the case for both compiling and linking
    // Liao 11/19/2009, changed to support linking multiple source files within
    // one command line We change the compilation mode for each individual file
    // to compile-only even when the original command line is to generate the
    // final executable. We generate the final executable at the SgProject level
    // from object files of each source file

    if (get_C_only() || get_Cxx_only() || get_Fortran_only()) {

      // Liao 5/1/2015: support both single and multiple files like:
      // identityTranslator main.c introduce -c to compile this single file
      // first. the linking step will happen when handling SgProject
      compilerNameString.push_back("-c");
      // compilation step of the two (compile + link) steps
      std::string objectFileName = generateOutputFileName();

      compilerNameString.push_back("-o");
      compilerNameString.push_back(
          Rose::TestOutput::resolvePath(objectFileName));
    }
  }

  if (SgProject::get_unparseHeaderFilesDebug() >= 1) {
    printf("\n");
    printf("At base of buildCompilerCommandLineOptions: compilerNameString = "
           "\n%s\n",
           CommandlineProcessing::generateStringFromArgList(compilerNameString)
               .c_str());
    printf("\n");
  }
  // #endif

  return compilerNameString;
} // end of SgFile::buildCompilerCommandLineOptions()
