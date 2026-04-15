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

  if (srcFile != nullptr) {
    srcFile->set_experimental_flang_frontend(true);
  }

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
      std::cerr << "[FATAL] [ROSE] [frontend] [Fortran] "
                   "error: Flang frontend requires -c when ROSE is in "
                   "compile-only mode.\n";
      ROSE_ABORT();
    }
  }

  status = flang_external_builder_main(argc, argv, srcFile);

  // The Fortran frontend builds source-backed AST in frontend-construction
  // mode, but all later midend-generated nodes must switch back to
  // transformation classification so they receive generated file-info instead
  // of NULL_FILE frontend placeholders.
  SageBuilder::setSourcePositionClassificationMode(
      SageBuilder::e_sourcePositionTransformation);

  if (SgProject::get_verbose() > 0) {
    cout << "FINISHED parsing with status " << status << "\n";
  }

  return status;
}

void set_flang_include_temp_dir(const std::string &path) {
  if (path.empty()) {
    flang_external_builder_set_include_tmpdir(nullptr);
    return;
  }
  flang_external_builder_set_include_tmpdir(path.c_str());
}

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
