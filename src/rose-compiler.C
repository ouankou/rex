#include <iostream>
#include "sage3basic.h"

int main( int argc, char * argv[] ) {
  MLOG_KEY_C(MLOG_DRIVER, "Entering the main program of the compiler\n");

  if (argc == 1){//Direct users to usage and exit with status == 1
	 fprintf (stderr,"Try option `--help' for more information.\n");
     exit (1);
  }

  std::vector<std::string> args(argv, argv+argc);

#if defined(ROSE_COMPILER_FOR_LANGUAGE)
  std::string language(ROSE_COMPILER_FOR_LANGUAGE);
  bool has_dialect = false;
  for (std::vector<std::string>::const_iterator arg = args.begin(); arg != args.end(); ++arg) {
    if (arg->find("-std=") == 0) {
      has_dialect = true;
      break;
    }
  }
  if (!has_dialect) {
    args.insert(args.begin()+1, "-std="+language);
  }
#endif

  SgProject * project = frontend(args);
  int frontend_status = project->get_frontendErrorCode();
  if (frontend_status != 0) {
    return frontend_status;
  }
  int status = backend(project);

// DQ (7/14/2025): Adding performance reporting.
  TimingPerformance::generateReport();

  return status;
}
