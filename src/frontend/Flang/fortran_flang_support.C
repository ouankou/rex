#include "sage3basic.h"

#include "rose_config.h"

#include "SageTreeBuilder.h"
#include "sageBuilder.h"

#include "flang-parser-driver.h"

#include "fortran_flang_support.h"

using namespace Rose;

#include <iostream>
using std::cout;

int experimental_fortran_main(int argc, char *argv[], SgSourceFile *srcFile,
                              const std::string &includeTempDir) {
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

  status = flang_parser_driver_main(argc, argv, srcFile, includeTempDir);

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
