/*
 * A test translator to process OpenMP Accelerator directives
 *
 * by Liao, 2/8/2013
 */
#include "rose.h"

#include <string>

using namespace std;
using namespace Rose;

int main(int argc, char *argv[]) {
  OmpSupport::enable_accelerator = true;

  vector<string> argvList(argv, argv + argc);

  SgProject *project = frontend(argvList);

  // TODO: turn this back on once blockDim.x * blockIdx.x + threadIdx.x is built
  // properly in omp_lowering.cpp
  //   AstTests::runAllTests(project);

  // Change the primary input file suffix to .cu. Lowering may synthesize
  // additional helper files in the project, so only require a non-empty list.
  ROSE_ASSERT(!project->get_fileList().empty());
  SgFile *cur_file = project->get_fileList().front();

  string orig_name = cur_file->get_file_info()->get_filenameString();
  orig_name = StringUtility::stripPathFromFileName(orig_name);
  string naked_name = StringUtility::stripFileSuffixFromFileName(orig_name);
  cur_file->set_unparse_output_filename("rose_" + naked_name + ".cu");

  int status = backend(project);
  SageInterface::tearDownAst(project);
  return status;
}
