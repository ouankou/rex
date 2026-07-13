#ifndef ROSE_FORTRAN_FLANG_SUPPORT_
#define ROSE_FORTRAN_FLANG_SUPPORT_

#include <string>

int experimental_fortran_main(int argc, char *argv[],
                              SgSourceFile *sg_source_file,
                              const std::string &include_temp_dir);

#endif /* ROSE_FORTRAN_FLANG_SUPPORT_ */
