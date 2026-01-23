#include "sage3basic.h"

#include "rose_config.h"

#include "SageTreeBuilder.h"
#include "rose_paths.h"
#include "sageBuilder.h"

#include "flang-external-builder-main.h"

#include "fortran_flang_support.h"

using namespace Rose;

#include <cstdlib>
#include <iostream>
using std::cout;

int experimental_fortran_main(int argc, char *argv[], SgSourceFile *srcFile) {
  int status{-1};

  if (SgProject::get_verbose() > 0) {
    cout << "\n";
    cout << "experimental_fortran_main: calling flang parser\n";
    std::cout << "--> argc=" << argc << " argv=";
    if (argc > 2)
      std::cout << argv[0] << ":" << argv[1] << ":" << argv[2] << std::endl;
  }

  SgGlobal *global_scope = Rose::builder::initialize_global_scope(srcFile);
  ROSE_ASSERT(global_scope &&
              "fortran_flang_support: failed initialize_global_scope");
  // Ensure Fortran case-insensitive semantics are enabled for scopes built
  // by the experimental Flang frontend.
  SageBuilder::symbol_table_case_insensitive_semantics = true;

  const char *fc_env = std::getenv("F18_FC");
  if (fc_env == nullptr || *fc_env == '\0') {
    if (!ROSE_GFORTRAN_PATH.empty()) {
      if (ROSE_GFORTRAN_PATH.find("flang") == std::string::npos) {
        std::cerr << "[FATAL] [ROSE] [frontend] [Fortran] "
                  << "ROSE_GFORTRAN_PATH must point to flang: "
                  << ROSE_GFORTRAN_PATH << "\n";
        ROSE_ABORT();
      }
      setenv("F18_FC", ROSE_GFORTRAN_PATH.c_str(), 1);
    }
  }

  std::vector<std::string> arg_storage;
  std::vector<char *> arg_ptrs;
  bool needs_compile_only = false;
  if (srcFile != nullptr) {
    if (SgProject *project = srcFile->get_project()) {
      needs_compile_only =
          project->get_compileOnly() || project->get_skipfinalCompileStep();
    }
  }

  if (needs_compile_only) {
    bool has_compile_only = false;
    for (int i = 1; i < argc; ++i) {
      if (argv[i] != nullptr && std::string(argv[i]) == "-c") {
        has_compile_only = true;
        break;
      }
    }
    if (!has_compile_only) {
      arg_storage.reserve(argc + 1);
      arg_ptrs.reserve(argc + 1);
      for (int i = 0; i < argc; ++i) {
        arg_storage.emplace_back(argv[i] != nullptr ? argv[i] : "");
      }
      arg_storage.emplace_back("-c");
      for (auto &arg : arg_storage) {
        arg_ptrs.push_back(arg.data());
      }
      status = flang_external_builder_main(static_cast<int>(arg_ptrs.size()),
                                           arg_ptrs.data(), srcFile);
    } else {
      status = flang_external_builder_main(argc, argv, srcFile);
    }
  } else {
    status = flang_external_builder_main(argc, argv, srcFile);
  }

  if (SgProject::get_verbose() > 0) {
    cout << "FINISHED parsing with status " << status << "\n";
  }

  return status;
}

#if !defined(USE_ROSE_OPEN_FORTRAN_PARSER_SUPPORT)
SgScopeStatement *getTopOfScopeStack() {
  ROSE_ASSERT(SageBuilder::emptyScopeStack() == false);
  SgScopeStatement *topOfStack = SageBuilder::topScopeStack();
  ROSE_ASSERT(topOfStack != nullptr);

  for (const auto *scope : SageBuilder::ScopeStack) {
    ROSE_ASSERT(scope != nullptr);
    ROSE_ASSERT(scope->isCaseInsensitive() == true);
  }

  return topOfStack;
}

bool emptyFortranStateStack() { return SageBuilder::emptyScopeStack(); }

bool emptyFortranScopeStack() { return SageBuilder::emptyScopeStack(); }
#endif
