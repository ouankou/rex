// include definitions of grammars, terminals, and non-terminals
// (objects within ROSETTA)
#include "grammar.h"

#include <iostream>

#include "rose_config.h"

#include <stdexcept>
#include <string>

bool verbose = false;
std::string smallHeadersDir;

int main(int argc, char *argv[]) {
  using namespace std;

  // Main Function for ROSE Preprocessor
  ios::sync_with_stdio(); // Syncs C++ and C I/O subsystems!

  // First build the C++ grammar
  std::string target_directory = ".";
  bool output_directory_seen = false;
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
      if (output_directory_seen) {
        cerr << argv[0] << ": only one output directory is allowed\n";
        exit(1);
      }
      target_directory = arg;
      output_directory_seen = true;
    }
  }

  // Build the header files and source files representing the grammar's
  // implementation.
  try {
    // For base level grammar use prefix "Sg" to be compatable with SAGE
    Grammar sageGrammar(/* name of grammar */ "Cxx_Grammar",
                        /* Prefix to names */ "Sg", target_directory,
                        smallHeadersDir);

    sageGrammar.buildCode();
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
    printf("Rosetta finished.\n");
  }

  return 0;
}
