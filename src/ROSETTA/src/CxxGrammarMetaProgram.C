static const char *purpose = "generates source code for defining grammars";

static const char *description =
    "This program demonstrates the Meta-Program Level where the details "
    "of a preprocessor are specified.  In this case this program "
    "represents the MetaProgram to build a preprocessor for the array "
    "class.\n\n"

    "Currently the grammars are defined, the execution of this program "
    "(a C++ program) generates the source code for defining the grammars "
    "to be used in building a preprocessor.  So this example does not "
    "yet build all the code required to build a preprocessor (the rest "
    "is specified in the ROSE/src directory structure).";

// include definitions of grammars, terminals, and non-terminals
// (objects within ROSETTA)
#include "grammar.h"

#include <iostream>

#include "rose_config.h"

#include <string>

#include <vector>

bool verbose = false;
std::string smallHeadersDir;

int main(int argc, char *argv[]) {
  using namespace std;

  // Main Function for ROSE Preprocessor
  ios::sync_with_stdio(); // Syncs C++ and C I/O subsystems!

  if (verbose) {
    printf("*******************************************************************"
           "********************** \n");
    printf("Build the C++ grammar (essentially an automated generation of a "
           "modified version of SAGE) \n");
    printf("*******************************************************************"
           "********************** \n");
  }

  // First build the C++ grammar
  std::string target_directory = ".";
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--small-headers") {
      if (i + 1 >= argc) {
        cerr << argv[0] << ": --small-headers requires a directory argument\n";
        exit(1);
      }
      smallHeadersDir = argv[++i];
    } else if (arg == "-v" || arg == "--verbose") {
      verbose = true;
    } else if (!arg.empty() && arg[0] == '-') {
      cerr << argv[0] << ": unknown option '" << arg << "'\n";
      exit(1);
    } else {
      if (target_directory != ".") {
        cerr << argv[0] << ": only one output directory is allowed\n";
        exit(1);
      }
      target_directory = arg;
    }
  }

  // Build the header files and source files representing the grammar's
  // implementation.
  std::string documentedConstructorPrototypes;
  try {
    // For base level grammar use prefix "Sg" to be compatable with SAGE
    Grammar sageGrammar(/* name of grammar */ "Cxx_Grammar",
                        /* Prefix to names */ "Sg",
                        /* Parent Grammar  */ "ROSE_BaseGrammar",
                        /* No parent Grammar */ NULL, target_directory,
                        smallHeadersDir);

    sageGrammar.buildCode();

    // Support for output of constructors as part of generated documentation
    documentedConstructorPrototypes =
        sageGrammar.staticContructorPrototypeString;
  } catch (const std::runtime_error &error) {
    std::cerr << "CxxGrammarMetaProgram: " << error.what() << std::endl;
    return 1;
  } catch (const std::string &error) {
    std::cerr << "CxxGrammarMetaProgram: " << error << std::endl;
    return 1;
  } catch (...) {
    std::cerr << "CxxGrammarMetaProgram: unknown error" << std::endl;
    return 1;
  }

  if (verbose) {
    printf("documentedConstructorPrototypes = %s \n",
           documentedConstructorPrototypes.c_str());
    printf("Rosetta finished.\n");
  }

  return 0;
}
